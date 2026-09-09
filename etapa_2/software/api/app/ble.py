"""Definicao do payload BLE do SysCare, do lado do servidor.

Este modulo e a referencia canonica do formato: o firmware (nRF Connect SDK) e o
app Flutter devem produzir/consumir exatamente estes bytes. A especificacao
completa, com os intervalos de advertising e o orcamento de energia, esta em
docs/ble_payload.md.

O anuncio usa um AD type "Manufacturer Specific Data" (0xFF) com Company ID
0xFFFF, reservado para desenvolvimento e testes pela Bluetooth SIG.

    total: 2 (company id) + 14 (payload) = 16 bytes -> cabe folgado nos
    31 bytes do advertising legado, sem precisar de scan response nem de
    BLE 5 extended advertising.
"""
from __future__ import annotations

from app.models import EventType

COMPANY_ID = 0xFFFF
PROTOCOL_VERSION = 1

# Codigos que trafegam no ar. Nao renumere sem atualizar o firmware.
EVENT_CODES: dict[str, int] = {
    EventType.TEST.value: 0x00,
    EventType.FALL.value: 0x01,
    EventType.PANIC.value: 0x02,
    EventType.NO_MOVEMENT.value: 0x03,
    EventType.LOW_BATTERY.value: 0x04,
}
EVENT_NAMES: dict[int, str] = {v: k for k, v in EVENT_CODES.items()}


class PayloadError(ValueError):
    """Bytes de advertising malformados."""


def decode_payload(raw: bytes) -> dict:
    """Decodifica os 14 bytes de manufacturer data em um dicionario.

    O app Flutter normalmente ja faz esse parsing e posta campos nomeados, mas a
    API aceita tambem o hex cru (`raw_payload`) para que um gateway "burro"
    possa repassar o anuncio sem interpreta-lo.
    """
    if len(raw) != 14:
        raise PayloadError(f"esperado 14 bytes, recebido {len(raw)}")
    if raw[0] != PROTOCOL_VERSION:
        raise PayloadError(f"versao de protocolo nao suportada: {raw[0]}")

    code = raw[5]
    if code not in EVENT_NAMES:
        raise PayloadError(f"event_type desconhecido: 0x{code:02x}")

    return {
        "version": raw[0],
        "ble_id": raw[1:5].hex(),
        "event_type": EVENT_NAMES[code],
        "seq": raw[6] | (raw[7] << 8),
        "battery_pct": raw[8],
        "impact_g": raw[9] / 10.0,
        "signature": raw[10:14].hex(),
    }
