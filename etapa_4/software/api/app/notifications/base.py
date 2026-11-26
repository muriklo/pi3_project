"""Interface comum dos canais de notificacao.

Cada canal (push, WhatsApp, SMS, ligacao) implementa `NotificationChannel`. O
dispatcher nao sabe nada sobre provedores: e assim que da para acrescentar
WhatsApp ou SMS depois sem tocar na regra de alerta.
"""
from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass


@dataclass(frozen=True)
class NotificationMessage:
    """Mensagem ja renderizada, pronta para qualquer canal."""

    title: str
    body: str
    alert_id: str
    device_id: str
    event_type: str
    # Dados estruturados que o app Flutter usa para abrir a tela de alerta.
    data: dict[str, str]


@dataclass(frozen=True)
class DeliveryResult:
    ok: bool
    provider_message_id: str | None = None
    error: str | None = None


class NotificationChannel(ABC):
    name: str

    @abstractmethod
    async def send(self, target: str, message: NotificationMessage) -> DeliveryResult:
        """Entrega `message` para `target` (token FCM, telefone, chat id...)."""

    @property
    def available(self) -> bool:
        """False quando o canal esta sem credencial configurada."""
        return True
