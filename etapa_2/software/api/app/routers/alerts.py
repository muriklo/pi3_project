"""Ingestao e ciclo de vida dos alertas.

O caminho critico do sistema e o POST /v1/alerts. Ele resolve tres problemas que
sao consequencia direta de o BLE ser broadcast:

1. DUPLICACAO - o anuncio e repetido por dezenas de segundos e pode ser ouvido
   por mais de um celular. Todos vao postar. Deduplicamos por (device, seq).
2. FALSIFICACAO - qualquer radio pode anunciar "queda". Conferimos o HMAC que a
   pulseira colocou no payload.
3. REPLAY - um anuncio antigo, gravado, pode ser retransmitido. O contador `seq`
   e monotonico e nunca retrocede.
"""
from __future__ import annotations

import logging
from datetime import timedelta

from fastapi import APIRouter, HTTPException, Query, status
from sqlalchemy import select

from app.ble import HEARTBEAT, PayloadError, decode_payload
from app.config import get_settings
from app.deps import CurrentUser, DbSession, can_view_device, visible_device_ids
from app.models import (
    Alert,
    AlertStatus,
    Device,
    EventType,
    utcnow,
)
from app.notifications.dispatcher import dispatch_alert
from app.schemas import AlertIngest, AlertIngestResult, AlertOut, AlertResolve
from app.security import build_signed_payload, verify_device_signature

logger = logging.getLogger(__name__)
router = APIRouter(prefix="/v1/alerts", tags=["alertas"])

# Contador de 16 bits: depois de 65535 ele volta a zero. Aceitamos o salto para
# tras quando a distancia e grande, senao a pulseira ficaria muda apos o wrap.
_SEQ_WRAP_MARGIN = 1000


def _merge_payload(body: AlertIngest) -> dict:
    """Normaliza a entrada: `raw_payload` (assinado) vence os campos soltos."""
    if body.raw_payload:
        try:
            decoded = decode_payload(bytes.fromhex(body.raw_payload))
        except (PayloadError, ValueError) as exc:
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail=f"raw_payload invalido: {exc}",
            ) from exc
        if decoded["ble_id"] != body.ble_id.lower():
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="ble_id do corpo diverge do ble_id assinado no payload.",
            )
        if decoded["event_type"] == HEARTBEAT:
            raise HTTPException(
                status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
                detail="Heartbeat nao e alerta: envie para /v1/telemetry.",
            )
        return decoded

    if body.event_type is None or body.seq is None:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail="Informe raw_payload, ou event_type e seq.",
        )
    return {
        "ble_id": body.ble_id.lower(),
        "event_type": body.event_type.value,
        "seq": body.seq,
        "battery_pct": body.battery_pct,
        "impact_g": body.impact_g,
        "signature": body.signature,
    }


def _check_signature(device: Device, data: dict) -> None:
    settings = get_settings()
    signature = data.get("signature")

    if not signature:
        if settings.require_hmac:
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="Payload sem assinatura da pulseira.",
            )
        logger.warning("Alerta sem HMAC aceito (device=%s, modo dev).", device.ble_id)
        return

    payload = build_signed_payload(
        ble_id=data["ble_id"],
        event_type=data["event_type"],
        seq=data["seq"],
        battery_pct=data.get("battery_pct") or 0,
        impact_dg=round((data.get("impact_g") or 0) * 10),
    )
    if not verify_device_signature(device.shared_secret, payload, signature):
        logger.error("HMAC invalido para a pulseira %s.", device.ble_id)
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="Assinatura da pulseira invalida.",
        )


def _check_replay(device: Device, seq: int) -> None:
    if device.last_seq < 0:
        return
    if seq > device.last_seq:
        return
    # Volta grande = wrap-around legitimo do contador de 16 bits.
    if device.last_seq - seq > _SEQ_WRAP_MARGIN:
        return
    raise HTTPException(
        status_code=status.HTTP_409_CONFLICT,
        detail=f"Contador retrocedeu (seq={seq}, ultimo={device.last_seq}): "
        "possivel repeticao de um anuncio antigo.",
    )


@router.post(
    "",
    response_model=AlertIngestResult,
    status_code=status.HTTP_201_CREATED,
    summary="Reportar um evento ouvido por BLE",
)
async def ingest_alert(
    body: AlertIngest, db: DbSession, user: CurrentUser
) -> AlertIngestResult:
    settings = get_settings()
    data = _merge_payload(body)

    device = db.scalar(select(Device).where(Device.ble_id == data["ble_id"]))
    if device is None or not device.active:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Pulseira nao cadastrada ou desativada.",
        )

    _check_signature(device, data)

    now = utcnow()
    occurred_at = body.occurred_at or now
    skew = abs((now - occurred_at).total_seconds())
    if skew > settings.max_event_skew_seconds:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail=f"occurred_at fora da janela aceita ({skew:.0f}s de diferenca).",
        )

    # Deduplicacao: dois celulares ouvindo o mesmo anuncio nao geram dois alertas.
    window_start = now - timedelta(seconds=settings.dedup_window_seconds)
    existing = db.scalar(
        select(Alert)
        .where(
            Alert.device_id == device.id,
            Alert.seq == data["seq"],
            Alert.received_at >= window_start,
        )
        .order_by(Alert.received_at.desc())
    )
    if existing is not None:
        # Aproveita o que o segundo celular sabe e o primeiro nao: localizacao.
        if existing.latitude is None and body.latitude is not None:
            existing.latitude = body.latitude
            existing.longitude = body.longitude
            existing.location_accuracy_m = body.location_accuracy_m
            db.commit()
        return AlertIngestResult(
            alert=AlertOut.model_validate(existing), duplicate=True, notified=0
        )

    _check_replay(device, data["seq"])

    alert = Alert(
        device_id=device.id,
        event_type=EventType(data["event_type"]),
        seq=data["seq"],
        occurred_at=occurred_at,
        received_at=now,
        impact_g=data.get("impact_g"),
        battery_pct=data.get("battery_pct"),
        rssi=body.rssi,
        latitude=body.latitude,
        longitude=body.longitude,
        location_accuracy_m=body.location_accuracy_m,
        gateway_label=body.gateway_label,
        status=AlertStatus.OPEN,
    )
    db.add(alert)

    device.last_seq = data["seq"]
    device.last_seen_at = now
    if data.get("battery_pct") is not None:
        device.battery_pct = data["battery_pct"]
    db.commit()
    db.refresh(alert)

    notified = await dispatch_alert(db, alert)
    db.refresh(alert)
    return AlertIngestResult(
        alert=AlertOut.model_validate(alert), duplicate=False, notified=notified
    )


@router.get("", response_model=list[AlertOut], summary="Historico de alertas")
def list_alerts(
    db: DbSession,
    user: CurrentUser,
    device_id: str | None = Query(default=None),
    status_filter: AlertStatus | None = Query(default=None, alias="status"),
    limit: int = Query(default=50, ge=1, le=200),
    offset: int = Query(default=0, ge=0),
) -> list[Alert]:
    # Alertas das pulseiras do usuario e daquelas de que ele e responsavel.
    query = select(Alert).where(Alert.device_id.in_(visible_device_ids(user.id)))
    if device_id:
        query = query.where(Alert.device_id == device_id)
    if status_filter:
        query = query.where(Alert.status == status_filter)
    query = query.order_by(Alert.received_at.desc()).limit(limit).offset(offset)
    return list(db.scalars(query).all())


def _get_visible_alert(db: DbSession, user: CurrentUser, alert_id: str) -> Alert:
    """Dono ou responsavel ativo: quem recebe o push precisa poder atender."""
    alert = db.get(Alert, alert_id)
    if alert is None or not can_view_device(db, alert.device_id, user.id):
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Alerta nao encontrado.")
    return alert


@router.get("/{alert_id}", response_model=AlertOut)
def get_alert(alert_id: str, db: DbSession, user: CurrentUser) -> Alert:
    return _get_visible_alert(db, user, alert_id)


@router.post(
    "/{alert_id}/ack",
    response_model=AlertOut,
    summary="Assumir o atendimento (para o escalonamento)",
)
def ack_alert(alert_id: str, db: DbSession, user: CurrentUser) -> Alert:
    alert = _get_visible_alert(db, user, alert_id)
    if alert.status != AlertStatus.OPEN:
        # Idempotente de proposito: dois cuidadores podem tocar ao mesmo tempo.
        return alert
    alert.status = AlertStatus.ACKED
    alert.acked_by_user_id = user.id
    alert.acked_at = utcnow()
    db.commit()
    db.refresh(alert)
    logger.info("Alerta %s confirmado por %s.", alert.id, user.email)
    return alert


@router.post(
    "/{alert_id}/resolve",
    response_model=AlertOut,
    summary="Encerrar o alerta (atendido ou falso positivo)",
)
def resolve_alert(
    alert_id: str, body: AlertResolve, db: DbSession, user: CurrentUser
) -> Alert:
    alert = _get_visible_alert(db, user, alert_id)
    alert.status = body.status
    alert.resolved_at = utcnow()
    if alert.acked_at is None:
        alert.acked_by_user_id = user.id
        alert.acked_at = alert.resolved_at
    if body.notes:
        alert.notes = body.notes
    db.commit()
    db.refresh(alert)
    # Falso positivo e o dado mais valioso para calibrar o algoritmo da pulseira.
    logger.info("Alerta %s encerrado como %s.", alert.id, body.status)
    return alert
