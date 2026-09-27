"""Vetores de teste do anuncio BLE, gerados pela implementacao de referencia.

O app (pacote Dart `syscare_protocol`) e o firmware precisam ler e montar os
mesmos 14 bytes que a API. Este script usa `app.ble` e `app.security` para
gerar os casos, e o teste do app compara byte a byte com eles.

    # regrava os vetores usados pelos testes do app
    python scripts/gerar_vetores_ble.py

    # imprime um anuncio assinado para colar no nRF Connect (Advertiser),
    # em "Manufacturer Data", com Company ID 0xFFFF
    python scripts/gerar_vetores_ble.py anuncio --secret <shared_secret> \
        --ble-id a1b2c3d4 --seq 43 --evento fall
"""
from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.ble import EVENT_CODES, PayloadError, decode_payload  # noqa: E402
from app.security import build_signed_payload  # noqa: E402

DESTINO = (
    Path(__file__).resolve().parents[2]
    / "app"
    / "packages"
    / "syscare_protocol"
    / "test"
    / "vetores_api.json"
)

# Chave fixa so para os vetores: nunca a de uma pulseira real.
SECRET = "000102030405060708090a0b0c0d0e0f"
BLE_ID = "a1b2c3d4"


def anuncio(secret: str, ble_id: str, evento: str, seq: int, bateria: int, impacto_dg: int) -> bytes:
    assinado = build_signed_payload(
        ble_id=ble_id, event_type=evento, seq=seq, battery_pct=bateria, impact_dg=impacto_dg
    )
    assinatura = hmac.new(bytes.fromhex(secret), assinado, hashlib.sha256).digest()[:4]
    return assinado + assinatura


def _valido(nome: str, raw: bytes) -> dict:
    esperado = decode_payload(raw)
    esperado["impact_dg"] = raw[9]
    return {"nome": nome, "hex": raw.hex(), "esperado": esperado}


def _invalido(nome: str, raw: bytes) -> dict:
    try:
        decode_payload(raw)
    except PayloadError as exc:
        return {"nome": nome, "hex": raw.hex(), "erro": str(exc)}
    raise AssertionError(f"{nome}: a API aceitou um payload que deveria recusar")


def gerar() -> dict:
    validos = [
        _valido(f"evento {nome}", anuncio(SECRET, BLE_ID, nome, 43, 87, 32 if nome != "heartbeat" else 0))
        for nome in EVENT_CODES
    ]
    validos += [
        _valido("seq 0", anuncio(SECRET, BLE_ID, "fall", 0, 100, 25)),
        _valido("seq 255 (so byte baixo)", anuncio(SECRET, BLE_ID, "fall", 255, 50, 25)),
        _valido("seq 256 (little-endian)", anuncio(SECRET, BLE_ID, "fall", 256, 50, 25)),
        _valido("seq 65535 (maximo)", anuncio(SECRET, BLE_ID, "panic", 65535, 1, 0)),
        _valido("impacto saturado em 25,5 g", anuncio(SECRET, BLE_ID, "fall", 7, 60, 255)),
        _valido("bateria zerada", anuncio(SECRET, BLE_ID, "low_battery", 8, 0, 0)),
        _valido("outro ble_id", anuncio(SECRET, "ffee0001", "fall", 9, 70, 40)),
    ]

    base = anuncio(SECRET, BLE_ID, "fall", 43, 87, 32)
    invalidos = [
        _invalido("13 bytes", base[:13]),
        _invalido("15 bytes", base + b"\x00"),
        _invalido("versao 2", bytes([2]) + base[1:]),
        _invalido("evento 0x06 desconhecido", base[:5] + bytes([0x06]) + base[6:]),
        _invalido("evento 0xff desconhecido", base[:5] + bytes([0xFF]) + base[6:]),
    ]
    return {
        "gerado_por": "etapa_2/software/api/scripts/gerar_vetores_ble.py",
        "secret": SECRET,
        "validos": validos,
        "invalidos": invalidos,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="cmd")
    p = sub.add_parser("anuncio", help="imprime um anuncio assinado em hex")
    p.add_argument("--secret", required=True, help="shared_secret devolvido no cadastro")
    p.add_argument("--ble-id", required=True)
    p.add_argument("--seq", type=int, required=True)
    p.add_argument("--evento", default="fall", choices=sorted(EVENT_CODES))
    p.add_argument("--bateria", type=int, default=87)
    p.add_argument("--impacto-dg", type=int, default=32)
    args = parser.parse_args()

    if args.cmd == "anuncio":
        raw = anuncio(args.secret, args.ble_id.lower(), args.evento, args.seq,
                      args.bateria, args.impacto_dg)
        print("Company ID: 0xFFFF")
        print(f"Dados (14 bytes): {raw.hex().upper()}")
        return

    DESTINO.parent.mkdir(parents=True, exist_ok=True)
    DESTINO.write_text(json.dumps(gerar(), indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"vetores gravados em {DESTINO}")


if __name__ == "__main__":
    main()
