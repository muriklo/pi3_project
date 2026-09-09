"""Escalonamento: reenvia o alerta enquanto ninguem confirmar o atendimento.

Uma notificacao entregue nao significa socorro a caminho: o cuidador pode estar
dormindo, com o celular no silencioso ou fora de casa. A cada
`SYSCARE_ESCALATION_SECONDS` sem ACK, o alerta e reenviado, ate
`SYSCARE_ESCALATION_MAX_ROUNDS` rodadas.

Roda como tarefa asyncio no ciclo de vida do app. Para varias instancias da API
em producao, troque por um worker externo (APScheduler/Celery) com trava, senao
duas instancias reenviam em duplicidade.
"""
from __future__ import annotations

import asyncio
import logging
from datetime import timedelta

from sqlalchemy import select

from app.config import get_settings
from app.database import SessionLocal
from app.models import Alert, AlertStatus, utcnow
from app.notifications.dispatcher import dispatch_alert

logger = logging.getLogger(__name__)

_TICK_SECONDS = 10


async def _run_round() -> None:
    settings = get_settings()
    limit = utcnow() - timedelta(seconds=settings.escalation_seconds)

    with SessionLocal() as db:
        pending = db.scalars(
            select(Alert).where(
                Alert.status == AlertStatus.OPEN,
                Alert.escalation_round < settings.escalation_max_rounds,
                Alert.last_notified_at.is_not(None),
                Alert.last_notified_at <= limit,
            )
        ).all()

        for alert in pending:
            alert.escalation_round += 1
            db.commit()
            logger.warning(
                "Escalonando alerta %s (rodada %d): sem confirmacao ha %ds.",
                alert.id,
                alert.escalation_round,
                settings.escalation_seconds,
            )
            await dispatch_alert(db, alert)


async def escalation_loop(stop: asyncio.Event) -> None:
    while not stop.is_set():
        try:
            await _run_round()
        except Exception:  # noqa: BLE001 - o loop nunca pode morrer
            logger.exception("Falha na rodada de escalonamento.")
        try:
            await asyncio.wait_for(stop.wait(), timeout=_TICK_SECONDS)
        except asyncio.TimeoutError:
            continue
