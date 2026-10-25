"""Testes do corpo do push: e o contrato com o app (lib/dados/push.dart).

O app so alarma direito se a mensagem chegar sem o bloco `notification` no
Android e com `ble_id`/`seq` para reconhecer o evento que o celular ja ouviu.
"""
from __future__ import annotations

from datetime import datetime, timezone

from app.models import Alert, Device, EventType
from app.notifications.dispatcher import build_message
from app.notifications.fcm import build_fcm_payload


def _mensagem():
    device = Device(id="d1", ble_id="a1b2c3d4", name="Pulseira 01", wearer_name="Sr. Jose")
    alert = Alert(
        id="f" * 32,
        device_id="d1",
        event_type=EventType.FALL,
        seq=43,
        occurred_at=datetime(2026, 10, 15, 13, 0, tzinfo=timezone.utc),
        impact_g=3.2,
        battery_pct=87,
        latitude=-27.5954,
        longitude=-48.548,
        escalation_round=0,
    )
    return build_message(alert, device)


def test_mensagem_leva_ble_id_e_seq_para_o_app_reconhecer_o_evento():
    data = _mensagem().data
    assert data["ble_id"] == "a1b2c3d4"
    assert data["seq"] == "43"
    assert data["alert_id"] == "f" * 32


def test_android_recebe_so_dados_para_o_app_tocar_o_alarme():
    corpo = build_fcm_payload("token-fcm", _mensagem())["message"]
    assert "notification" not in corpo
    assert "notification" not in corpo["android"]
    assert corpo["android"]["priority"] == "high"
    assert corpo["data"]["title"] == "QUEDA DETECTADA"
    assert corpo["data"]["body"].startswith("Sr. Jose")


def test_todo_valor_de_data_e_texto():
    # O FCM recusa a mensagem inteira (HTTP 400) se um valor nao for string.
    corpo = build_fcm_payload("token-fcm", _mensagem())["message"]
    assert all(isinstance(v, str) for v in corpo["data"].values())


def test_ios_recebe_o_texto_no_aps():
    aps = build_fcm_payload("token-fcm", _mensagem())["message"]["apns"]["payload"]["aps"]
    assert aps["alert"] == {"title": "QUEDA DETECTADA", "body": _mensagem().body}
