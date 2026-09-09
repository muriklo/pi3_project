"""Heartbeat da pulseira: bateria, presenca e qualidade do enlace BLE.

O app envia em lote, nao a cada anuncio ouvido. Anunciando a cada 2 s, seriam
43 mil amostras por dia por pulseira; o app agrega e manda um resumo a cada
poucos minutos, economizando radio do celular e banco no servidor.
"""
from __future__ import annotations

from fastapi import APIRouter, HTTPException, status
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError

from app.deps import CurrentUser, DbSession
from app.models import Device, Telemetry
from app.schemas import TelemetryBatch, TelemetryResult

router = APIRouter(prefix="/v1/telemetry", tags=["telemetria"])


@router.post("", response_model=TelemetryResult, status_code=status.HTTP_202_ACCEPTED)
def ingest_telemetry(
    body: TelemetryBatch, db: DbSession, user: CurrentUser
) -> TelemetryResult:
    device = db.scalar(select(Device).where(Device.ble_id == body.ble_id.lower()))
    if device is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Pulseira nao cadastrada.")

    accepted = 0
    duplicates = 0
    for sample in body.samples:
        # A restricao unica (device_id, seq) faz o trabalho de deduplicacao
        # quando varios celulares ouvem o mesmo heartbeat.
        db.add(
            Telemetry(
                device_id=device.id,
                seq=sample.seq,
                recorded_at=sample.recorded_at,
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

    latest = max(body.samples, key=lambda s: s.recorded_at)
    if device.last_seen_at is None or latest.recorded_at > device.last_seen_at:
        device.last_seen_at = latest.recorded_at
        if latest.battery_pct is not None:
            device.battery_pct = latest.battery_pct
        db.commit()

    return TelemetryResult(accepted=accepted, duplicates=duplicates)
