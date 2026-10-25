"""Testes do canal de SMS: renderizacao, custo e regra de acionamento.

O que importa aqui e nao gastar dinheiro a toa: cada segmento extra de SMS e uma
cobranca a mais, e mandar SMS quando o push ja resolveu e desperdicio.
"""
from __future__ import annotations

import pytest

from app.notifications.base import NotificationMessage
from app.notifications.sms import (
    GSM7_SINGLE_LIMIT,
    count_segments,
    render_sms_body,
    to_gsm7_ascii,
)


def make_message(**data) -> NotificationMessage:
    base = {
        "alert_id": "a" * 32,
        "device_id": "d1",
        "event_type": "fall",
        "wearer_name": "Sr. Joao",
    }
    base.update(data)
    return NotificationMessage(
        title="QUEDA DETECTADA",
        body="ignorado no SMS",
        alert_id=base["alert_id"],
        device_id=base["device_id"],
        event_type=base["event_type"],
        data=base,
    )


def test_acentos_sao_removidos_para_nao_triplicar_o_custo():
    """Um unico caractere fora do GSM-7 derruba o limite de 160 para 70."""
    assert to_gsm7_ascii("Avó João Conceição") == "Avo Joao Conceicao"
    assert to_gsm7_ascii("emoji 🚨 aqui") == "emoji aqui"


def test_corpo_do_sms_cabe_em_um_segmento():
    body = render_sms_body(make_message(latitude="-27.5954", longitude="-48.5480"))
    assert len(body) <= GSM7_SINGLE_LIMIT
    assert count_segments(body) == 1
    assert body.isascii()


def test_link_de_mapa_entra_quando_ha_gps():
    com_gps = render_sms_body(
        make_message(latitude="-27.5954", longitude="-48.5480")
    )
    sem_gps = render_sms_body(make_message())
    assert "maps.google.com/?q=-27.5954,-48.5480" in com_gps
    assert "maps.google.com" not in sem_gps


def test_mensagem_longa_e_truncada_e_nao_vira_varios_segmentos():
    body = render_sms_body(make_message(wearer_name="Jose " * 60))
    assert len(body) <= GSM7_SINGLE_LIMIT
    assert body.endswith("...")


@pytest.mark.parametrize(
    ("length", "expected"),
    [(1, 1), (160, 1), (161, 2), (306, 2), (307, 3)],
)
def test_contagem_de_segmentos(length, expected):
    assert count_segments("x" * length) == expected
