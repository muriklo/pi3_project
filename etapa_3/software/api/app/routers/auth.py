"""Cadastro e login do responsavel/cuidador, e registro do token de push."""
from __future__ import annotations

from fastapi import APIRouter, HTTPException, Response, status
from sqlalchemy import select

from app.deps import CurrentUser, DbSession
from app.models import PushToken, User, utcnow
from app.schemas import (
    PushTokenCreate,
    PushTokenOut,
    TokenOut,
    UserCreate,
    UserLogin,
    UserOut,
)
from app.security import create_access_token, hash_password, verify_password

router = APIRouter(prefix="/v1/auth", tags=["autenticacao"])


@router.post("/register", response_model=TokenOut, status_code=status.HTTP_201_CREATED)
def register(body: UserCreate, db: DbSession) -> TokenOut:
    email = body.email.lower()
    if db.scalar(select(User).where(User.email == email)) is not None:
        raise HTTPException(status.HTTP_409_CONFLICT, "E-mail ja cadastrado.")

    user = User(email=email, name=body.name, password_hash=hash_password(body.password))
    db.add(user)
    db.commit()
    db.refresh(user)
    return TokenOut(
        access_token=create_access_token(user.id), user=UserOut.model_validate(user)
    )


@router.post("/login", response_model=TokenOut)
def login(body: UserLogin, db: DbSession) -> TokenOut:
    user = db.scalar(select(User).where(User.email == body.email.lower()))
    # Mesma mensagem nos dois casos: nao revela se o e-mail existe.
    if user is None or not verify_password(body.password, user.password_hash):
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "E-mail ou senha invalidos.")
    return TokenOut(
        access_token=create_access_token(user.id), user=UserOut.model_validate(user)
    )


@router.get("/me", response_model=UserOut)
def me(user: CurrentUser) -> User:
    return user


@router.post(
    "/push-tokens",
    response_model=PushTokenOut,
    status_code=status.HTTP_201_CREATED,
    summary="Registrar o token FCM deste aparelho",
)
def register_push_token(
    body: PushTokenCreate, db: DbSession, user: CurrentUser
) -> PushToken:
    existing = db.scalar(select(PushToken).where(PushToken.token == body.token))
    if existing is not None:
        # O FCM reaproveita tokens entre reinstalacoes e trocas de conta.
        existing.user_id = user.id
        existing.platform = body.platform
        existing.last_used_at = utcnow()
        db.commit()
        db.refresh(existing)
        return existing

    token = PushToken(user_id=user.id, token=body.token, platform=body.platform)
    db.add(token)
    db.commit()
    db.refresh(token)
    return token


@router.delete(
    "/push-tokens/{token_id}",
    status_code=status.HTTP_204_NO_CONTENT,
    response_class=Response,
    response_model=None,
    summary="Remover o token no logout",
)
def delete_push_token(token_id: str, db: DbSession, user: CurrentUser) -> None:
    token = db.get(PushToken, token_id)
    if token is None or token.user_id != user.id:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Token nao encontrado.")
    db.delete(token)
    db.commit()
