"""Modelo de dados do SysCare.

Relacionamento central:
    User (dono da conta) 1-N Device (pulseira) 1-N Caregiver (quem e avisado)
    Device 1-N Alert 1-N AlertDelivery (uma linha por tentativa de notificacao)
"""
from __future__ import annotations

import enum
import uuid
from datetime import datetime, timezone

from sqlalchemy import (
    Boolean,
    DateTime,
    Float,
    ForeignKey,
    Index,
    Integer,
    String,
    Text,
    UniqueConstraint,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.database import Base


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


def new_id() -> str:
    return uuid.uuid4().hex


class EventType(str, enum.Enum):
    FALL = "fall"                       # queda detectada pelo algoritmo
    PANIC = "panic"                     # botao de emergencia
    NO_MOVEMENT = "no_movement"         # imobilidade prolongada pos-impacto
    LOW_BATTERY = "low_battery"
    DEVICE_OFFLINE = "device_offline"   # gerado pela API, nao pela pulseira
    TEST = "test"


class AlertStatus(str, enum.Enum):
    OPEN = "open"                       # ninguem confirmou ainda -> escalona
    ACKED = "acked"                     # um cuidador assumiu o atendimento
    RESOLVED = "resolved"               # atendido
    FALSE_POSITIVE = "false_positive"   # usuario cancelou, nao era queda


class DeliveryStatus(str, enum.Enum):
    PENDING = "pending"
    SENT = "sent"
    FAILED = "failed"


class User(Base):
    __tablename__ = "users"

    id: Mapped[str] = mapped_column(String(32), primary_key=True, default=new_id)
    email: Mapped[str] = mapped_column(String(255), unique=True, index=True)
    name: Mapped[str] = mapped_column(String(120))
    password_hash: Mapped[str] = mapped_column(String(255))
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow)

    devices: Mapped[list[Device]] = relationship(back_populates="owner")
    push_tokens: Mapped[list[PushToken]] = relationship(back_populates="user")


class Device(Base):
    """Uma pulseira. `ble_id` e o identificador de 4 bytes que vai no advertising."""

    __tablename__ = "devices"

    id: Mapped[str] = mapped_column(String(32), primary_key=True, default=new_id)
    ble_id: Mapped[str] = mapped_column(String(16), unique=True, index=True)
    name: Mapped[str] = mapped_column(String(120))
    wearer_name: Mapped[str | None] = mapped_column(String(120), nullable=True)
    owner_id: Mapped[str] = mapped_column(ForeignKey("users.id"), index=True)

    # Chave simetrica compartilhada com o firmware, usada no HMAC do payload BLE.
    shared_secret: Mapped[str] = mapped_column(String(64))
    # Ultimo contador aceito: rejeita replay de um broadcast capturado no ar.
    last_seq: Mapped[int] = mapped_column(Integer, default=-1)

    battery_pct: Mapped[int | None] = mapped_column(Integer, nullable=True)
    last_seen_at: Mapped[datetime | None] = mapped_column(
        DateTime(timezone=True), nullable=True
    )
    active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow)

    owner: Mapped[User] = relationship(back_populates="devices")
    caregivers: Mapped[list[Caregiver]] = relationship(
        back_populates="device", cascade="all, delete-orphan"
    )
    alerts: Mapped[list[Alert]] = relationship(back_populates="device")


class Caregiver(Base):
    """Quem deve ser avisado. `priority` 1 e acionado primeiro."""

    __tablename__ = "caregivers"

    id: Mapped[str] = mapped_column(String(32), primary_key=True, default=new_id)
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id"), index=True)
    name: Mapped[str] = mapped_column(String(120))
    phone: Mapped[str | None] = mapped_column(String(24), nullable=True)
    email: Mapped[str | None] = mapped_column(String(255), nullable=True)
    # Conta no app: e por ela que o push FCM chega.
    user_id: Mapped[str | None] = mapped_column(
        ForeignKey("users.id"), nullable=True, index=True
    )
    priority: Mapped[int] = mapped_column(Integer, default=1)
    active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow)

    device: Mapped[Device] = relationship(back_populates="caregivers")


class PushToken(Base):
    """Token FCM de um aparelho. Um usuario pode ter varios (celular + tablet)."""

    __tablename__ = "push_tokens"

    id: Mapped[str] = mapped_column(String(32), primary_key=True, default=new_id)
    user_id: Mapped[str] = mapped_column(ForeignKey("users.id"), index=True)
    token: Mapped[str] = mapped_column(Text, unique=True)
    platform: Mapped[str] = mapped_column(String(16), default="android")
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow)
    last_used_at: Mapped[datetime | None] = mapped_column(
        DateTime(timezone=True), nullable=True
    )

    user: Mapped[User] = relationship(back_populates="push_tokens")


class Alert(Base):
    __tablename__ = "alerts"
    __table_args__ = (
        Index("ix_alerts_dedup", "device_id", "seq", "received_at"),
        Index("ix_alerts_open", "status", "received_at"),
    )

    id: Mapped[str] = mapped_column(String(32), primary_key=True, default=new_id)
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id"), index=True)
    event_type: Mapped[EventType] = mapped_column(String(24))
    seq: Mapped[int] = mapped_column(Integer)

    occurred_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    received_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utcnow
    )

    # Contexto medido pela pulseira e pelo celular que ouviu o broadcast.
    impact_g: Mapped[float | None] = mapped_column(Float, nullable=True)
    battery_pct: Mapped[int | None] = mapped_column(Integer, nullable=True)
    rssi: Mapped[int | None] = mapped_column(Integer, nullable=True)
    latitude: Mapped[float | None] = mapped_column(Float, nullable=True)
    longitude: Mapped[float | None] = mapped_column(Float, nullable=True)
    location_accuracy_m: Mapped[float | None] = mapped_column(Float, nullable=True)
    gateway_label: Mapped[str | None] = mapped_column(String(120), nullable=True)

    status: Mapped[AlertStatus] = mapped_column(String(20), default=AlertStatus.OPEN)
    acked_by_user_id: Mapped[str | None] = mapped_column(
        ForeignKey("users.id"), nullable=True
    )
    acked_at: Mapped[datetime | None] = mapped_column(
        DateTime(timezone=True), nullable=True
    )
    resolved_at: Mapped[datetime | None] = mapped_column(
        DateTime(timezone=True), nullable=True
    )
    notes: Mapped[str | None] = mapped_column(Text, nullable=True)

    escalation_round: Mapped[int] = mapped_column(Integer, default=0)
    last_notified_at: Mapped[datetime | None] = mapped_column(
        DateTime(timezone=True), nullable=True
    )

    device: Mapped[Device] = relationship(back_populates="alerts")
    deliveries: Mapped[list[AlertDelivery]] = relationship(
        back_populates="alert", cascade="all, delete-orphan"
    )


class AlertDelivery(Base):
    """Log de cada tentativa de notificacao: quem, por qual canal, deu certo?"""

    __tablename__ = "alert_deliveries"

    id: Mapped[str] = mapped_column(String(32), primary_key=True, default=new_id)
    alert_id: Mapped[str] = mapped_column(ForeignKey("alerts.id"), index=True)
    caregiver_id: Mapped[str | None] = mapped_column(
        ForeignKey("caregivers.id"), nullable=True
    )
    channel: Mapped[str] = mapped_column(String(24))
    target: Mapped[str] = mapped_column(String(255))
    status: Mapped[DeliveryStatus] = mapped_column(
        String(16), default=DeliveryStatus.PENDING
    )
    provider_message_id: Mapped[str | None] = mapped_column(String(255), nullable=True)
    error: Mapped[str | None] = mapped_column(Text, nullable=True)
    attempt: Mapped[int] = mapped_column(Integer, default=1)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow)

    alert: Mapped[Alert] = relationship(back_populates="deliveries")


class Telemetry(Base):
    """Heartbeat periodico da pulseira: prova que ela esta viva e com bateria."""

    __tablename__ = "telemetry"
    __table_args__ = (UniqueConstraint("device_id", "seq", name="uq_telemetry_seq"),)

    id: Mapped[str] = mapped_column(String(32), primary_key=True, default=new_id)
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id"), index=True)
    seq: Mapped[int] = mapped_column(Integer)
    recorded_at: Mapped[datetime] = mapped_column(DateTime(timezone=True))
    battery_pct: Mapped[int | None] = mapped_column(Integer, nullable=True)
    rssi: Mapped[int | None] = mapped_column(Integer, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow)
