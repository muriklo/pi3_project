"""Canal de SMS.

SMS e o canal mais caro do SysCare (~US$ 0,06 por mensagem para o Brasil via
Twilio, ou ~R$ 0,05-0,065 em provedores nacionais). Existe aqui para o cuidador
que NAO tem smartphone, e como ultimo recurso no escalonamento -- nao como canal
primario. Ver docs/canais_de_alerta.md.

Tres modos, escolhidos por SYSCARE_SMS_PROVIDER:
    none    (padrao) canal desligado
    dryrun  monta e valida a mensagem, registra no log, NAO envia e NAO cobra
    twilio  envio real pela API do Twilio

O modo `dryrun` existe para testar todo o caminho -- renderizacao, corte de
tamanho, contagem de segmentos, selecao de destinatarios -- sem conta em
provedor nenhum e sem gastar credito.
"""
from __future__ import annotations

import logging
import unicodedata

import httpx

from app.config import get_settings
from app.notifications.base import (
    DeliveryResult,
    NotificationChannel,
    NotificationMessage,
)

logger = logging.getLogger(__name__)

_TIMEOUT = httpx.Timeout(15.0)

# Um SMS cabe 160 caracteres se todos estiverem no alfabeto GSM-7. Um unico
# caractere fora dele (um "ç", um "ã", um emoji) joga a mensagem inteira para
# UCS-2, e o limite despenca para 70 -- ou seja, uma acentuacao inocente pode
# TRIPLICAR o custo do alerta. Por isso normalizamos para ASCII.
GSM7_SINGLE_LIMIT = 160
GSM7_MULTIPART_LIMIT = 153  # cada parte perde 7 caracteres para o cabecalho UDH


def to_gsm7_ascii(text: str) -> str:
    """Remove acentos e caracteres fora do GSM-7, preservando a legibilidade."""
    normalized = unicodedata.normalize("NFKD", text)
    ascii_text = normalized.encode("ascii", "ignore").decode("ascii")
    return " ".join(ascii_text.split())


def count_segments(text: str) -> int:
    """Quantos SMS serao cobrados por esta mensagem."""
    length = len(text)
    if length <= GSM7_SINGLE_LIMIT:
        return 1
    return -(-length // GSM7_MULTIPART_LIMIT)  # divisao inteira para cima


def render_sms_body(message: NotificationMessage, max_segments: int = 1) -> str:
    """Monta o texto do SMS: curto, sem acento e com link de mapa se houver GPS.

    O SMS nao tem espaco para contexto. Ele responde tres coisas: o que houve,
    com quem, e onde. O resto o cuidador ve no app.
    """
    parts = [f"SysCare: {message.title}"]

    wearer = message.data.get("wearer_name")
    if wearer:
        parts.append(wearer)

    lat = message.data.get("latitude")
    lon = message.data.get("longitude")
    if lat and lon:
        # Link curto que abre direto no app de mapas do celular.
        parts.append(f"https://maps.google.com/?q={lat},{lon}")

    body = to_gsm7_ascii(" - ".join(parts))

    limit = (
        GSM7_SINGLE_LIMIT
        if max_segments == 1
        else GSM7_MULTIPART_LIMIT * max_segments
    )
    if len(body) > limit:
        body = body[: limit - 3].rstrip() + "..."
    return body


class DryRunSmsChannel(NotificationChannel):
    """Registra o SMS que seria enviado. Nao chama provedor, nao gera custo."""

    name = "sms"

    def __init__(self, from_number: str = "+15005550006") -> None:
        self._from = from_number

    async def send(self, target: str, message: NotificationMessage) -> DeliveryResult:
        body = render_sms_body(message)
        logger.warning(
            "[SMS DRY-RUN] de=%s para=%s segmentos=%d chars=%d\n  corpo: %s",
            self._from,
            target,
            count_segments(body),
            len(body),
            body,
        )
        return DeliveryResult(ok=True, provider_message_id="dryrun")


class TwilioSmsChannel(NotificationChannel):
    """Envio real via Twilio Programmable Messaging.

    Nota para o Brasil: usar um numero longo (long code) do Twilio funciona sem
    cadastro previo, mas as operadoras brasileiras filtram trafego A2P nao
    registrado, entao a entrega nao e garantida. Sender ID alfanumerico
    ("SYSCARE" como remetente) exige registro documental junto as operadoras.
    Para o prototipo, long code resolve.
    """

    name = "sms"
    _API = "https://api.twilio.com/2010-04-01"

    def __init__(self) -> None:
        settings = get_settings()
        self._sid = settings.twilio_account_sid
        self._token = settings.twilio_auth_token
        self._from = settings.twilio_from_number

    @property
    def available(self) -> bool:
        return bool(self._sid and self._token and self._from)

    async def send(self, target: str, message: NotificationMessage) -> DeliveryResult:
        if not self.available:
            return DeliveryResult(ok=False, error="Twilio nao configurado.")

        body = render_sms_body(message)
        url = f"{self._API}/Accounts/{self._sid}/Messages.json"

        try:
            async with httpx.AsyncClient(timeout=_TIMEOUT) as client:
                response = await client.post(
                    url,
                    data={"From": self._from, "To": target, "Body": body},
                    auth=(self._sid, self._token),
                )
        except Exception as exc:  # noqa: BLE001
            return DeliveryResult(ok=False, error=f"{type(exc).__name__}: {exc}")

        if response.status_code in (200, 201):
            payload = response.json()
            logger.info(
                "SMS aceito pelo Twilio: sid=%s status=%s segmentos=%d",
                payload.get("sid"),
                payload.get("status"),
                count_segments(body),
            )
            # 'queued' != entregue. A confirmacao real vem por webhook de status.
            return DeliveryResult(ok=True, provider_message_id=payload.get("sid"))

        return DeliveryResult(
            ok=False, error=f"HTTP {response.status_code}: {response.text[:300]}"
        )


def get_sms_channel() -> NotificationChannel | None:
    """Devolve o canal de SMS configurado, ou None se estiver desligado."""
    provider = get_settings().sms_provider.lower()
    if provider == "dryrun":
        return DryRunSmsChannel(get_settings().twilio_from_number or "+15005550006")
    if provider == "twilio":
        channel = TwilioSmsChannel()
        if not channel.available:
            logger.error(
                "SYSCARE_SMS_PROVIDER=twilio mas faltam credenciais; SMS desligado."
            )
            return None
        return channel
    return None
