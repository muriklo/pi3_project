"""Canal de push via Firebase Cloud Messaging (HTTP v1).

FCM nao cobra por mensagem, entao e o canal primario do SysCare. A API legada
(`/fcm/send` com "server key") foi desativada pelo Google; aqui usamos a HTTP v1,
que exige um token OAuth2 obtido a partir do JSON da service account.

Configuracao (uma vez):
    1. console.firebase.google.com -> criar projeto
    2. Configuracoes do projeto -> Contas de servico -> Gerar nova chave privada
    3. salvar o JSON e apontar SYSCARE_FCM_CREDENTIALS_FILE para ele

Sem credencial configurada, `available` e False e o dispatcher cai no
ConsoleChannel: a API sobe e funciona inteira mesmo antes de existir Firebase.
"""
from __future__ import annotations

import logging

import httpx

from app.config import get_settings
from app.notifications.base import DeliveryResult, NotificationChannel, NotificationMessage

logger = logging.getLogger(__name__)

_SCOPES = ["https://www.googleapis.com/auth/firebase.messaging"]
_TIMEOUT = httpx.Timeout(10.0)


class FcmChannel(NotificationChannel):
    name = "push"

    def __init__(self) -> None:
        settings = get_settings()
        self._credentials = None
        self._project_id = settings.fcm_project_id or ""

        if not settings.fcm_credentials_file:
            logger.warning("FCM sem credencial: canal de push desativado.")
            return

        try:
            from google.oauth2 import service_account

            self._credentials = service_account.Credentials.from_service_account_file(
                settings.fcm_credentials_file, scopes=_SCOPES
            )
            self._project_id = self._project_id or self._credentials.project_id
        except Exception:  # noqa: BLE001 - nao derrubar a API por causa do push
            logger.exception("Falha ao carregar credencial do FCM.")
            self._credentials = None

    @property
    def available(self) -> bool:
        return self._credentials is not None and bool(self._project_id)

    def _access_token(self) -> str:
        """Renova o token OAuth2 (validade de 1h) apenas quando necessario."""
        from google.auth.transport.requests import Request

        if not self._credentials.valid:
            self._credentials.refresh(Request())
        return self._credentials.token

    async def send(self, target: str, message: NotificationMessage) -> DeliveryResult:
        if not self.available:
            return DeliveryResult(ok=False, error="FCM nao configurado.")

        url = (
            f"https://fcm.googleapis.com/v1/projects/{self._project_id}/messages:send"
        )
        payload = {
            "message": {
                "token": target,
                "notification": {"title": message.title, "body": message.body},
                "data": message.data,
                "android": {
                    # 'high' furam o Doze; alerta de queda nao pode esperar
                    # a janela de manutencao do Android.
                    "priority": "high",
                    "notification": {
                        "channel_id": "syscare_emergencia",
                        "sound": "sirene",
                        "default_vibrate_timings": False,
                        "vibrate_timings": ["0.5s", "0.3s", "0.5s", "0.3s", "0.5s"],
                    },
                },
                "apns": {
                    "headers": {"apns-priority": "10"},
                    "payload": {
                        "aps": {
                            "sound": {"critical": 1, "name": "sirene.caf", "volume": 1.0}
                        }
                    },
                },
            }
        }

        try:
            token = self._access_token()
            async with httpx.AsyncClient(timeout=_TIMEOUT) as client:
                response = await client.post(
                    url,
                    json=payload,
                    headers={"Authorization": f"Bearer {token}"},
                )
        except Exception as exc:  # noqa: BLE001
            return DeliveryResult(ok=False, error=f"{type(exc).__name__}: {exc}")

        if response.status_code == 200:
            return DeliveryResult(ok=True, provider_message_id=response.json().get("name"))
        return DeliveryResult(
            ok=False, error=f"HTTP {response.status_code}: {response.text[:300]}"
        )


class ConsoleChannel(NotificationChannel):
    """Fallback de desenvolvimento: escreve o alerta no log em vez de enviar."""

    name = "console"

    async def send(self, target: str, message: NotificationMessage) -> DeliveryResult:
        logger.warning(
            "[ALERTA -> %s] %s | %s | alert_id=%s",
            target,
            message.title,
            message.body,
            message.alert_id,
        )
        return DeliveryResult(ok=True, provider_message_id="console")
