"""Contratos de entrada e saida da API (Pydantic v2).

Sao estes modelos que geram a documentacao OpenAPI em /docs.
"""
from __future__ import annotations

from datetime import datetime

from pydantic import BaseModel, ConfigDict, EmailStr, Field, field_validator

from app.models import AlertStatus, DeliveryStatus, EventType


# --------------------------------------------------------------------------- #
# Autenticacao
# --------------------------------------------------------------------------- #
class UserCreate(BaseModel):
    email: EmailStr
    name: str = Field(min_length=2, max_length=120)
    password: str = Field(min_length=8, max_length=128)


class UserLogin(BaseModel):
    email: EmailStr
    password: str


class UserOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: str
    email: str
    name: str
    created_at: datetime


class TokenOut(BaseModel):
    access_token: str
    token_type: str = "bearer"
    user: UserOut


# --------------------------------------------------------------------------- #
# Pulseiras
# --------------------------------------------------------------------------- #
class DeviceCreate(BaseModel):
    ble_id: str = Field(
        min_length=8,
        max_length=8,
        description="Identificador de 4 bytes em hex, igual ao gravado no firmware.",
    )
    name: str = Field(min_length=2, max_length=120)
    wearer_name: str | None = None

    @field_validator("ble_id")
    @classmethod
    def _hex_lower(cls, v: str) -> str:
        try:
            bytes.fromhex(v)
        except ValueError as exc:
            raise ValueError("ble_id deve ser hexadecimal de 8 caracteres") from exc
        return v.lower()


class DeviceUpdate(BaseModel):
    name: str | None = None
    wearer_name: str | None = None
    active: bool | None = None


class DeviceOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: str
    ble_id: str
    name: str
    wearer_name: str | None
    owner_id: str = Field(
        description="Dono da pulseira. Diferente do usuario logado = so responsavel."
    )
    battery_pct: int | None
    last_seen_at: datetime | None
    active: bool
    created_at: datetime


class DeviceCreated(DeviceOut):
    """Retornado apenas no cadastro: a chave nunca mais e exibida."""

    shared_secret: str = Field(
        description="Grave esta chave no firmware. Nao e possivel recupera-la depois."
    )


# --------------------------------------------------------------------------- #
# Cuidadores
# --------------------------------------------------------------------------- #
class CaregiverCreate(BaseModel):
    name: str = Field(min_length=2, max_length=120)
    phone: str | None = Field(default=None, max_length=24)
    email: EmailStr | None = None
    user_id: str | None = Field(
        default=None, description="Conta do app que recebera o push FCM."
    )
    priority: int = Field(default=1, ge=1, le=10)


class CaregiverUpdate(BaseModel):
    name: str | None = None
    phone: str | None = None
    email: EmailStr | None = None
    user_id: str | None = None
    priority: int | None = Field(default=None, ge=1, le=10)
    active: bool | None = None


class CaregiverOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: str
    device_id: str
    name: str
    phone: str | None
    email: str | None
    user_id: str | None
    priority: int
    active: bool


# --------------------------------------------------------------------------- #
# Push
# --------------------------------------------------------------------------- #
class PushTokenCreate(BaseModel):
    token: str = Field(min_length=10)
    platform: str = Field(default="android", pattern="^(android|ios|web)$")


class PushTokenOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: str
    platform: str
    created_at: datetime


# --------------------------------------------------------------------------- #
# Alertas
# --------------------------------------------------------------------------- #
class AlertIngest(BaseModel):
    """O que o app envia ao ouvir um broadcast de emergencia.

    Ou os campos ja decodificados, ou `raw_payload` com os 14 bytes em hex; se os
    dois vierem, `raw_payload` tem precedencia (e a fonte de verdade assinada).
    """

    ble_id: str = Field(min_length=8, max_length=8)
    event_type: EventType | None = None
    seq: int | None = Field(default=None, ge=0, le=65535)
    battery_pct: int | None = Field(default=None, ge=0, le=100)
    impact_g: float | None = Field(default=None, ge=0, le=25.5)
    signature: str | None = Field(
        default=None, description="HMAC de 4 bytes em hex vindo do advertising."
    )
    raw_payload: str | None = Field(
        default=None,
        description="Os 14 bytes de manufacturer data em hex, sem o company id.",
    )

    occurred_at: datetime | None = Field(
        default=None, description="Padrao: instante da recepcao no servidor."
    )
    rssi: int | None = Field(default=None, ge=-127, le=20)
    latitude: float | None = Field(default=None, ge=-90, le=90)
    longitude: float | None = Field(default=None, ge=-180, le=180)
    location_accuracy_m: float | None = Field(default=None, ge=0)
    gateway_label: str | None = Field(
        default=None,
        max_length=120,
        description="Qual celular ouviu o anuncio, ex.: 'Moto G - Maria'.",
    )


class DeliveryOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    channel: str
    target: str
    status: DeliveryStatus
    error: str | None
    attempt: int
    created_at: datetime


class AlertOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: str
    device_id: str
    event_type: EventType
    seq: int
    occurred_at: datetime
    received_at: datetime
    impact_g: float | None
    battery_pct: int | None
    rssi: int | None
    latitude: float | None
    longitude: float | None
    location_accuracy_m: float | None
    gateway_label: str | None
    status: AlertStatus
    acked_by_user_id: str | None
    acked_at: datetime | None
    resolved_at: datetime | None
    notes: str | None
    escalation_round: int
    deliveries: list[DeliveryOut] = []


class AlertIngestResult(BaseModel):
    alert: AlertOut
    duplicate: bool = Field(
        description="True quando outro celular ja havia reportado este mesmo evento."
    )
    notified: int = Field(description="Quantas notificacoes foram disparadas agora.")


class AlertResolve(BaseModel):
    status: AlertStatus = Field(
        description="Use 'resolved' ou 'false_positive'.",
    )
    notes: str | None = None

    @field_validator("status")
    @classmethod
    def _closing_status(cls, v: AlertStatus) -> AlertStatus:
        if v not in (AlertStatus.RESOLVED, AlertStatus.FALSE_POSITIVE):
            raise ValueError("status deve ser 'resolved' ou 'false_positive'")
        return v


# --------------------------------------------------------------------------- #
# Telemetria
# --------------------------------------------------------------------------- #
class TelemetrySample(BaseModel):
    seq: int = Field(
        ge=0,
        le=65535,
        description="Seq do ultimo evento, repetido pelo heartbeat (nao incrementa).",
    )
    recorded_at: datetime
    battery_pct: int | None = Field(default=None, ge=0, le=100)
    rssi: int | None = Field(default=None, ge=-127, le=20)


class TelemetryBatch(BaseModel):
    """O app acumula heartbeats offline e envia em lote para poupar radio/bateria.

    Basta uma amostra por janela de SYSCARE_TELEMETRY_BUCKET_SECONDS; as demais
    da mesma janela sao contadas como duplicadas.
    """

    ble_id: str = Field(min_length=8, max_length=8)
    samples: list[TelemetrySample] = Field(min_length=1, max_length=200)


class TelemetryResult(BaseModel):
    accepted: int
    duplicates: int
