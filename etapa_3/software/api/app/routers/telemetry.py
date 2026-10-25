"""Heartbeat da pulseira: bateria, presenca e qualidade do enlace BLE.

A pulseira anuncia o heartbeat (event_type 0x05) a cada 2 s. O app nao posta
cada anuncio ouvido: seriam 43 mil amostras por dia por pulseira. Ele agrega e
manda um resumo a cada poucos minutos, economizando radio do celular e banco no
servidor.
"""
from __future__ import annotations

from datetime import datetime, timezone

from fastapi import APIRouter, HTTPException, status
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError

from app.config import get_settings
from app.deps import CurrentUser, DbSession
from app.models import Device, Telemetry
from app.schemas import TelemetryBatch, TelemetryResult

router = APIRouter(prefix="/v1/telemetry", tags=["telemetria"])


def _as_utc(ts: datetime) -> datetime:
    # O SQLite devolve datas sem fuso mesmo com DateTime(timezone=True), e o
    # corpo da requisicao pode vir com ou sem. Tudo que e comparado vira UTC.
    return ts.replace(tzinfo=timezone.utc) if ts.tzinfo is None else ts.astimezone(timezone.utc)


def _bucket_start(ts: datetime, seconds: int) -> datetime:
    epoch = int(_as_utc(ts).timestamp())
    return datetime.fromtimestamp(epoch - epoch % seconds, tz=timezone.utc)


@router.post("", response_model=TelemetryResult, status_code=status.HTTP_202_ACCEPTED)
def ingest_telemetry(
    body: TelemetryBatch, db: DbSession, user: CurrentUser
) -> TelemetryResult:
    settings = get_settings()
    device = db.scalar(select(Device).where(Device.ble_id == body.ble_id.lower()))
    if device is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Pulseira nao cadastrada.")

    accepted = 0
    duplicates = 0
    for sample in body.samples:
        # A restricao unica (device_id, bucket_start) faz a deduplicacao quando
        # varios celulares ouvem o mesmo heartbeat. Nao da para usar o seq: ele
        # so muda quando acontece um evento, e fica parado entre um e outro.
        db.add(
            Telemetry(
                device_id=device.id,
                seq=sample.seq,
                recorded_at=sample.recorded_at,
                bucket_start=_bucket_start(
                    sample.recorded_at, settings.telemetry_bucket_seconds
                ),
                battery_pct=sample.battery_pct,
                rssi=sample.rssi,
            )
        )
        try:
            db.commit()
            accepted += 1
        except IntegrityError:
            db.rollback()
            duplicates += 1

    latest = max(body.samples, key=lambda s: _as_utc(s.recorded_at))
    latest_at = _as_utc(latest.recorded_at)
    if device.last_seen_at is None or latest_at > _as_utc(device.last_seen_at):
        device.last_seen_at = latest_at
        if latest.battery_pct is not None:
            device.battery_pct = latest.battery_pct
        db.commit()

    return TelemetryResult(accepted=accepted, duplicates=duplicates)
