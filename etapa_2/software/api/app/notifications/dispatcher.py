"""Fan-out do alerta para os cuidadores, com registro de cada entrega.

Regra de acionamento (rodada 0, disparada no instante do POST /alerts):
    - notifica TODOS os cuidadores ativos de uma vez.

Rodadas seguintes sao disparadas pelo escalonador (app/escalation.py) enquanto
ninguem confirmar o atendimento. Numa emergencia, avisar demais custa menos que
avisar de menos, entao o SysCare nao serializa por prioridade na primeira
rodada; `priority` so define a ordem de exibicao e o ponto de partida de canais
caros (SMS/ligacao), se forem habilitados depois.
"""
from __future__ import annotations

import logging
from dataclasses import dataclass

from sqlalchemy import select
from sqlalchemy.orm import Session

from app.config import get_settings
from app.models import (
    Alert,
    AlertDelivery,
    Caregiver,
    Device,
    DeliveryStatus,
    EventType,
    PushToken,
    utcnow,
)
from app.notifications.base import NotificationChannel, NotificationMessage
from app.notifications.fcm import ConsoleChannel, FcmChannel
from app.notifications.sms import get_sms_channel

logger = logging.getLogger(__name__)

_EVENT_TITLES = {
    EventType.FALL: "QUEDA DETECTADA",
    EventType.PANIC: "BOTAO DE EMERGENCIA ACIONADO",
    EventType.NO_MOVEMENT: "IMOBILIDADE PROLONGADA",
    EventType.LOW_BATTERY: "Bateria da pulseira baixa",
    EventType.DEVICE_OFFLINE: "Pulseira fora de alcance",
    EventType.TEST: "Teste do SysCare",
}


def _push_channel() -> NotificationChannel:
    fcm = FcmChannel()
    return fcm if fcm.available else ConsoleChannel()


def build_message(alert: Alert, device: Device) -> NotificationMessage:
    who = device.wearer_name or device.name
    title = _EVENT_TITLES.get(alert.event_type, "Alerta SysCare")

    parts = [who]
    if alert.impact_g:
        parts.append(f"impacto de {alert.impact_g:.1f} g")
    if alert.latitude is not None and alert.longitude is not None:
        parts.append(f"local: {alert.latitude:.5f}, {alert.longitude:.5f}")
    if alert.battery_pct is not None:
        parts.append(f"bateria {alert.battery_pct}%")
    body = " - ".join(parts)

    # Todo valor de `data` no FCM precisa ser string.
    data = {
        "alert_id": alert.id,
        "device_id": alert.device_id,
        "event_type": alert.event_type.value
        if isinstance(alert.event_type, EventType)
        else str(alert.event_type),
        "occurred_at": alert.occurred_at.isoformat(),
        "wearer_name": who,
        "escalation_round": str(alert.escalation_round),
        "click_action": "FLUTTER_NOTIFICATION_CLICK",
    }
    if alert.latitude is not None and alert.longitude is not None:
        data["latitude"] = f"{alert.latitude}"
        data["longitude"] = f"{alert.longitude}"

    return NotificationMessage(
        title=title,
        body=body,
        alert_id=alert.id,
        device_id=alert.device_id,
        event_type=data["event_type"],
        data=data,
    )


@dataclass(frozen=True)
class _Plan:
    """Uma entrega planejada: para quem, por qual canal, em qual endereco."""

    caregiver: Caregiver
    channel: NotificationChannel
    target: str
    label: str


def _plan_deliveries(db: Session, device_id: str, round_: int) -> list[_Plan]:
    """Decide quem recebe o que nesta rodada.

    Push vai para todo cuidador com o app instalado, sempre. SMS custa dinheiro,
    entao entra em dois casos: cuidador sem app (unico jeito de alcanca-lo) ou
    escalonamento -- ninguem confirmou o alerta e o push claramente nao bastou.
    """
    push = _push_channel()
    sms = get_sms_channel()

    caregivers = db.scalars(
        select(Caregiver)
        .where(Caregiver.device_id == device_id, Caregiver.active.is_(True))
        .order_by(Caregiver.priority)
    ).all()

    settings = get_settings()
    plans: list[_Plan] = []

    for caregiver in caregivers:
        tokens = (
            db.scalars(
                select(PushToken).where(PushToken.user_id == caregiver.user_id)
            ).all()
            if caregiver.user_id
            else []
        )
        for token in tokens:
            plans.append(
                _Plan(caregiver, push, token.token, f"{caregiver.name} <{token.platform}>")
            )

        if sms is None or not caregiver.phone:
            continue
        if not tokens or round_ >= settings.sms_from_escalation_round:
            plans.append(
                _Plan(caregiver, sms, caregiver.phone, f"{caregiver.name} <{caregiver.phone}>")
            )

    return plans


async def dispatch_alert(db: Session, alert: Alert) -> int:
    """Envia o alerta por todos os canais disponiveis. Devolve quantas entregas OK."""
    device = db.get(Device, alert.device_id)
    if device is None:
        return 0

    message = build_message(alert, device)
    plans = _plan_deliveries(db, alert.device_id, alert.escalation_round)

    if not plans:
        logger.error(
            "Alerta %s sem nenhum destino alcancavel (device=%s).", alert.id, device.id
        )
        db.add(
            AlertDelivery(
                alert_id=alert.id,
                channel="none",
                target="(nenhum)",
                status=DeliveryStatus.FAILED,
                error="Nenhum cuidador ativo com token de push ou telefone alcancavel.",
                attempt=alert.escalation_round + 1,
            )
        )
        db.commit()
        return 0

    sent = 0
    for plan in plans:
        result = await plan.channel.send(plan.target, message)
        db.add(
            AlertDelivery(
                alert_id=alert.id,
                caregiver_id=plan.caregiver.id,
                channel=plan.channel.name,
                target=plan.label,
                status=DeliveryStatus.SENT if result.ok else DeliveryStatus.FAILED,
                provider_message_id=result.provider_message_id,
                error=result.error,
                attempt=alert.escalation_round + 1,
            )
        )
        if result.ok:
            sent += 1

    alert.last_notified_at = utcnow()
    db.commit()
    logger.info("Alerta %s: %d/%d entregas OK.", alert.id, sent, len(plans))
    return sent
