"""Envia (ou simula) um SMS de teste do SysCare para um numero.

    python scripts/send_test_sms.py +5548991791826
    python scripts/send_test_sms.py +5548991791826 --event fall --gps -27.5954,-48.5480

Sem SYSCARE_SMS_PROVIDER=twilio configurado, roda em DRY-RUN: mostra o texto
exato, quantos segmentos seriam cobrados e a requisicao HTTP que seria feita --
sem enviar nada e sem gastar credito.

Com credenciais do Twilio no .env, envia de verdade. O script pede confirmacao
antes, porque um SMS real custa dinheiro e chega no celular de alguem.
"""
from __future__ import annotations

import argparse
import asyncio
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.config import get_settings  # noqa: E402
from app.notifications.base import NotificationMessage  # noqa: E402
from app.notifications.sms import (  # noqa: E402
    DryRunSmsChannel,
    TwilioSmsChannel,
    count_segments,
    render_sms_body,
)

_E164 = re.compile(r"^\+[1-9]\d{7,14}$")

_TITLES = {
    "fall": "QUEDA DETECTADA",
    "panic": "BOTAO DE EMERGENCIA ACIONADO",
    "no_movement": "IMOBILIDADE PROLONGADA",
    "test": "Teste do SysCare",
}

# Preco por segmento para o Brasil (Twilio, set/2026). So para estimativa.
_USD_PER_SEGMENT = 0.0599


def build_demo_message(event: str, wearer: str, gps: str | None) -> NotificationMessage:
    data = {
        "alert_id": "demo0000000000000000000000000000",
        "device_id": "demo",
        "event_type": event,
        "wearer_name": wearer,
        "escalation_round": "1",
    }
    if gps:
        lat, lon = gps.split(",")
        data["latitude"] = lat.strip()
        data["longitude"] = lon.strip()

    return NotificationMessage(
        title=_TITLES.get(event, "Alerta SysCare"),
        body=wearer,
        alert_id=data["alert_id"],
        device_id="demo",
        event_type=event,
        data=data,
    )


async def main() -> int:
    parser = argparse.ArgumentParser(description="Teste do canal de SMS do SysCare.")
    parser.add_argument("to", help="Numero destino em E.164, ex.: +5548991791826")
    parser.add_argument("--event", default="fall", choices=sorted(_TITLES))
    parser.add_argument("--wearer", default="Sr. Joao")
    parser.add_argument(
        "--gps",
        default="-27.5954,-48.5480",
        help="'lat,lon' para incluir link de mapa. Use '' para omitir.",
    )
    parser.add_argument(
        "--yes", action="store_true", help="Nao pedir confirmacao no envio real."
    )
    args = parser.parse_args()

    if not _E164.match(args.to):
        print(f"ERRO: '{args.to}' nao esta no formato E.164 (ex.: +5548991791826).")
        return 2

    settings = get_settings()
    message = build_demo_message(args.event, args.wearer, args.gps or None)
    body = render_sms_body(message)
    segments = count_segments(body)

    print("=" * 72)
    print("SysCare - teste do canal de SMS")
    print("=" * 72)
    print(f"  provider configurado : {settings.sms_provider}")
    print(f"  destino              : {args.to}")
    print(f"  remetente            : {settings.twilio_from_number or '(nao definido)'}")
    print(f"  caracteres           : {len(body)}")
    print(f"  segmentos cobrados   : {segments}")
    print(f"  custo estimado       : US$ {segments * _USD_PER_SEGMENT:.4f}")
    print("-" * 72)
    print("  TEXTO QUE SERIA ENVIADO:")
    print(f"  {body}")
    print("=" * 72)

    if settings.sms_provider.lower() != "twilio":
        print()
        print("MODO DRY-RUN: nada foi enviado, nenhum credito foi gasto.")
        print("Para enviar de verdade, preencha no .env:")
        print("    SYSCARE_SMS_PROVIDER=twilio")
        print("    SYSCARE_TWILIO_ACCOUNT_SID=AC...")
        print("    SYSCARE_TWILIO_AUTH_TOKEN=...")
        print("    SYSCARE_TWILIO_FROM_NUMBER=+1...")
        channel = DryRunSmsChannel(settings.twilio_from_number or "+15005550006")
        await channel.send(args.to, message)
        return 0

    channel = TwilioSmsChannel()
    if not channel.available:
        print("ERRO: provider=twilio mas faltam SID, token ou numero de origem.")
        return 1

    if not args.yes:
        print()
        print(f"Isto vai enviar um SMS REAL para {args.to} e gastar credito.")
        if input("Confirma? [s/N] ").strip().lower() not in ("s", "sim", "y", "yes"):
            print("Cancelado.")
            return 0

    result = await channel.send(args.to, message)
    if result.ok:
        print(f"\nOK - aceito pelo Twilio. SID: {result.provider_message_id}")
        print("Atencao: 'aceito' nao e 'entregue'. Confira o status no console do")
        print("Twilio -- operadoras brasileiras filtram trafego A2P nao registrado.")
        return 0

    print(f"\nFALHOU: {result.error}")
    return 1


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
