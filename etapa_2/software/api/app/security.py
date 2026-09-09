"""Autenticacao de usuarios (JWT) e verificacao do payload assinado da pulseira.

Duas camadas independentes de confianca:

1. `create_access_token` / `decode_access_token` autenticam o APP contra a API.
2. `verify_device_signature` autentica a PULSEIRA contra a API. Como o BLE aqui
   e broadcast (nao conectavel, sem pareamento), qualquer radio proximo pode
   forjar um anuncio de queda. A pulseira assina o payload com HMAC-SHA256
   truncado, usando uma chave gravada no firmware; o celular so repassa os bytes.
"""
from __future__ import annotations

import hashlib
import hmac
import secrets
from datetime import datetime, timedelta, timezone

import jwt

from app.config import get_settings

settings = get_settings()

_PBKDF2_ITERATIONS = 480_000


def hash_password(password: str) -> str:
    """PBKDF2-HMAC-SHA256 da stdlib: sem dependencia externa e sem limite de 72 bytes."""
    salt = secrets.token_bytes(16)
    digest = hashlib.pbkdf2_hmac(
        "sha256", password.encode(), salt, _PBKDF2_ITERATIONS
    )
    return f"pbkdf2_sha256${_PBKDF2_ITERATIONS}${salt.hex()}${digest.hex()}"


def verify_password(password: str, stored: str) -> bool:
    try:
        algo, iterations, salt_hex, digest_hex = stored.split("$")
    except ValueError:
        return False
    if algo != "pbkdf2_sha256":
        return False
    digest = hashlib.pbkdf2_hmac(
        "sha256", password.encode(), bytes.fromhex(salt_hex), int(iterations)
    )
    return hmac.compare_digest(digest.hex(), digest_hex)


def create_access_token(user_id: str) -> str:
    now = datetime.now(timezone.utc)
    payload = {
        "sub": user_id,
        "iat": now,
        "exp": now + timedelta(minutes=settings.jwt_expire_minutes),
    }
    return jwt.encode(payload, settings.jwt_secret, algorithm=settings.jwt_algorithm)


def decode_access_token(token: str) -> str | None:
    """Devolve o user_id, ou None se o token for invalido/expirado."""
    try:
        payload = jwt.decode(
            token, settings.jwt_secret, algorithms=[settings.jwt_algorithm]
        )
    except jwt.PyJWTError:
        return None
    return payload.get("sub")


def new_device_secret() -> str:
    """Chave de 16 bytes (hex) para gravar no firmware da pulseira."""
    return secrets.token_hex(16)


def build_signed_payload(
    ble_id: str, event_type: str, seq: int, battery_pct: int, impact_dg: int
) -> bytes:
    """Reconstroi os bytes que a pulseira assinou.

    Deve casar byte a byte com o firmware. Layout (little-endian), ver
    docs/ble_payload.md:
        [0]    versao do protocolo (1)
        [1..4] ble_id  (4 bytes)
        [5]    event_type
        [6..7] seq     (uint16)
        [8]    bateria em %
        [9]    impacto em decimos de g (saturado em 25.5 g)
    """
    from app.ble import EVENT_CODES

    return bytes(
        [
            1,
            *bytes.fromhex(ble_id),
            EVENT_CODES[event_type],
            seq & 0xFF,
            (seq >> 8) & 0xFF,
            max(0, min(100, battery_pct)),
            max(0, min(255, impact_dg)),
        ]
    )


def verify_device_signature(
    shared_secret_hex: str, payload: bytes, signature_hex: str
) -> bool:
    """Confere o HMAC-SHA256 truncado em 4 bytes que veio no advertising."""
    expected = hmac.new(
        bytes.fromhex(shared_secret_hex), payload, hashlib.sha256
    ).digest()[:4]
    try:
        received = bytes.fromhex(signature_hex)
    except ValueError:
        return False
    return hmac.compare_digest(expected, received)
