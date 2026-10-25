"""Dependencias compartilhadas pelos routers: usuario autenticado e acesso ao device.

Dois niveis de acesso a uma pulseira:
    - dono: edita a pulseira e gerencia os responsaveis (com telefones de terceiros);
    - responsavel ativo com conta no app: ve a pulseira e o historico, e confirma e
      encerra alertas -- o "Estou indo" de qualquer responsavel para o reenvio.
"""
from __future__ import annotations

from typing import Annotated

from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer
from sqlalchemy import Select, or_, select
from sqlalchemy.orm import Session

from app.database import get_db
from app.models import Caregiver, Device, User
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


def visible_device_ids(user_id: str) -> Select:
    """Ids das pulseiras que o usuario enxerga: as dele e as de que e responsavel."""
    as_caregiver = select(Caregiver.device_id).where(
        Caregiver.user_id == user_id, Caregiver.active.is_(True)
    )
    return select(Device.id).where(
        or_(Device.owner_id == user_id, Device.id.in_(as_caregiver))
    )


def can_view_device(db: Session, device_id: str, user_id: str) -> bool:
    return (
        db.scalar(visible_device_ids(user_id).where(Device.id == device_id)) is not None
    )


def get_visible_device(device_id: str, db: DbSession, user: CurrentUser) -> Device:
    """Carrega a pulseira para leitura: dono ou responsavel ativo."""
    device = db.get(Device, device_id)
    if device is None or not can_view_device(db, device.id, user.id):
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="Pulseira nao encontrada."
        )
    return device


VisibleDevice = Annotated[Device, Depends(get_visible_device)]
