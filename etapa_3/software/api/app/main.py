"""SysCare API - ponto de entrada.

    uvicorn app.main:app --reload --host 0.0.0.0 --port 8000

Documentacao interativa em http://localhost:8000/docs (Swagger) e /redoc.
"""
from __future__ import annotations

import asyncio
import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.config import get_settings
from app.database import Base, engine
from app.escalation import escalation_loop
from app.notifications.fcm import FcmChannel
from app.routers import alerts, auth, devices, telemetry

logging.basicConfig(
    level=logging.INFO, format="%(asctime)s %(levelname)-8s %(name)s: %(message)s"
)
logger = logging.getLogger("syscare")

settings = get_settings()


@asynccontextmanager
async def lifespan(app: FastAPI):
    # create_all resolve o prototipo da Etapa 2. Para a Etapa 3, migrar para
    # Alembic: alterar uma tabela em producao com create_all nao funciona.
    Base.metadata.create_all(bind=engine)

    if not FcmChannel().available:
        logger.warning(
            "FCM nao configurado: os alertas serao apenas registrados no log. "
            "Defina SYSCARE_FCM_CREDENTIALS_FILE para enviar push de verdade."
        )

    stop = asyncio.Event()
    task = asyncio.create_task(escalation_loop(stop))
    logger.info("SysCare API pronta (env=%s).", settings.env)
    try:
        yield
    finally:
        stop.set()
        await task


app = FastAPI(
    title="SysCare API",
    version="0.1.0",
    summary="Backend do sistema portatil de deteccao de quedas SysCare.",
    description=(
        "A pulseira anuncia o evento por BLE broadcast; o aplicativo escuta, "
        "alerta localmente e repassa para esta API, que notifica os cuidadores "
        "e mantem o historico. Ver docs/ble_payload.md para o formato do anuncio."
    ),
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    # O app Flutter em Android/iOS nao usa CORS; isto atende o painel web e o
    # Swagger. Restrinja aos dominios do painel antes de ir para producao.
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(auth.router)
app.include_router(devices.router)
app.include_router(alerts.router)
app.include_router(telemetry.router)


@app.get("/health", tags=["infra"], summary="Liveness probe")
def health() -> dict:
    return {
        "status": "ok",
        "env": settings.env,
        "push": "fcm" if FcmChannel().available else "console",
    }
