"""Cria um alerta de verdade na API, como se a pulseira tivesse anunciado.

Serve para testar o lado do responsavel sem a pulseira: o alerta passa pela
validacao de assinatura, deduplicacao e notificacao, e aparece no historico de
quem tem acesso a pulseira.

    python scripts/simular_alerta.py --email admin@admin.com --ble-id a1b2c3d4
    python scripts/simular_alerta.py --email admin@admin.com --ble-id a1b2c3d4 \
        --evento panic --lat -27.5954 --lon -48.5480

A chave da pulseira so e mostrada uma vez no cadastro; este script le direto do
banco de desenvolvimento (syscare.db). Por isso so funciona na maquina que roda
a API -- e e de proposito: em producao ninguem deve conseguir forjar alerta.
"""
from __future__ import annotations

import argparse
import getpass
import hashlib
import hmac
import os
import sqlite3
import sys
from datetime import datetime, timezone
from pathlib import Path

import httpx

RAIZ_API = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RAIZ_API))

from app.ble import EVENT_CODES  # noqa: E402
from app.security import build_signed_payload  # noqa: E402

EVENTOS = ("fall", "panic", "no_movement", "low_battery", "test")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--email", required=True, help="conta que repassa o alerta (qualquer uma)")
    parser.add_argument("--ble-id", required=True, help="pulseira, ex.: a1b2c3d4")
    parser.add_argument("--evento", default="fall", choices=EVENTOS)
    parser.add_argument("--lat", type=float, help="latitude, ex.: -27.5954")
    parser.add_argument("--lon", type=float, help="longitude, ex.: -48.5480")
    parser.add_argument("--servidor", default="http://127.0.0.1:8000")
    parser.add_argument("--db", default=str(RAIZ_API / "syscare.db"))
    args = parser.parse_args()

    ble_id = args.ble_id.lower()
    with sqlite3.connect(args.db) as db:
        linha = db.execute(
            "SELECT shared_secret, last_seq FROM devices WHERE ble_id = ?", (ble_id,)
        ).fetchone()
    if linha is None:
        sys.exit(f"Pulseira {ble_id} nao esta cadastrada em {args.db}.")
    segredo, ultimo_seq = linha
    # Seq novo: a API recusa (409) qualquer seq que nao avance.
    seq = (ultimo_seq + 1) % 65536

    assinado = build_signed_payload(
        ble_id=ble_id, event_type=args.evento, seq=seq, battery_pct=87, impact_dg=32
    )
    assinatura = hmac.new(bytes.fromhex(segredo), assinado, hashlib.sha256).digest()[:4]
    assert EVENT_CODES[args.evento] == assinado[5]

    senha = os.environ.get("SYSCARE_SENHA") or getpass.getpass(f"Senha de {args.email}: ")
    corpo = {
        "ble_id": ble_id,
        "raw_payload": (assinado + assinatura).hex(),
        "occurred_at": datetime.now(timezone.utc).isoformat(),
        "gateway_label": "Simulador (PC)",
    }
    if args.lat is not None and args.lon is not None:
        corpo.update(latitude=args.lat, longitude=args.lon, location_accuracy_m=10)

    with httpx.Client(base_url=args.servidor, timeout=15) as api:
        r = api.post("/v1/auth/login", json={"email": args.email, "password": senha})
        if r.status_code != 200:
            sys.exit(f"Login falhou ({r.status_code}): {r.text}")
        r = api.post(
            "/v1/alerts",
            json=corpo,
            headers={"Authorization": f"Bearer {r.json()['access_token']}"},
        )
    if r.status_code != 201:
        sys.exit(f"Alerta recusado ({r.status_code}): {r.text}")
    resultado = r.json()
    alerta = resultado["alert"]
    print(f"Alerta criado: {alerta['event_type']} seq {seq} (id {alerta['id']})")
    print(f"Duplicado: {resultado['duplicate']} | notificacoes disparadas: {resultado['notified']}")
    for e in alerta["deliveries"]:
        print(f"  - {e['channel']} -> {e['target']}: {e['status']}")


if __name__ == "__main__":
    main()
