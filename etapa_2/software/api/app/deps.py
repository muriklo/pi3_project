"""Dependencias compartilhadas pelos routers: usuario autenticado e posse do device."""
from __future__ import annotations

from typing import Annotated

from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from sqlalchemy.orm import Session

from app.database import get_db
from app.models import Device, User
from app.security import decode_access_token

bearer_scheme = HTTPBearer(auto_error=False)

DbSession = Annotated[Session, Depends(get_db)]


def get_current_user(
    db: DbSession,
    credentials: Annotated[
        HTTPAuthorizationCredentials | None, Depends(bearer_scheme)
    ] = None,
) -> User:
    if credentials is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Credenciais ausentes.",
            headers={"WWW-Authenticate": "Bearer"},
        )
    user_id = decode_access_token(credentials.credentials)
    if user_id is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Token invalido ou expirado.",
            headers={"WWW-Authenticate": "Bearer"},
        )
    user = db.get(User, user_id)
    if user is None:
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED, detail="Usuario inexistente."
        )
    return user


CurrentUser = Annotated[User, Depends(get_current_user)]


def get_owned_device(device_id: str, db: DbSession, user: CurrentUser) -> Device:
    """Carrega a pulseira garantindo que ela pertence a quem esta chamando."""
    device = db.get(Device, device_id)
    # 404 tambem quando existe mas nao e do usuario: nao vaza a existencia do id.
    if device is None or device.owner_id != user.id:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="Pulseira nao encontrada."
        )
    return device


OwnedDevice = Annotated[Device, Depends(get_owned_device)]
