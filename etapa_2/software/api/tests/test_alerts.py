"""Testes do fluxo critico: um broadcast de queda vira alerta e notificacao.

Cobre os tres riscos do BLE broadcast: duplicacao, falsificacao e replay.
Execute com:  pytest -q
"""
from __future__ import annotations

import hashlib
import hmac
import os
import tempfile

import pytest
from fastapi.testclient import TestClient

# Precisa vir antes de importar o app: a config e lida na importacao.
_DB_FD, _DB_PATH = tempfile.mkstemp(suffix=".db")
os.environ["SYSCARE_DATABASE_URL"] = f"sqlite:///{_DB_PATH}"
os.environ["SYSCARE_JWT_SECRET"] = "secret-de-teste"
os.environ["SYSCARE_REQUIRE_HMAC"] = "true"
os.environ["SYSCARE_FCM_CREDENTIALS_FILE"] = ""

from app.ble import EVENT_CODES  # noqa: E402
from app.database import Base, engine  # noqa: E402
from app.main import app  # noqa: E402

BLE_ID = "a1b2c3d4"


@pytest.fixture(scope="module")
def client():
    Base.metadata.create_all(bind=engine)
    with TestClient(app) as c:
        yield c
    Base.metadata.drop_all(bind=engine)
    os.close(_DB_FD)


@pytest.fixture(scope="module")
def setup(client):
    """Conta + pulseira + cuidador com token de push, prontos para receber alerta."""
    r = client.post(
        "/v1/auth/register",
        json={"email": "maria@exemplo.com", "name": "Maria", "password": "senha-forte-1"},
    )
    assert r.status_code == 201, r.text
    token = r.json()["access_token"]
    user_id = r.json()["user"]["id"]
    headers = {"Authorization": f"Bearer {token}"}

    r = client.post(
        "/v1/devices",
        json={"ble_id": BLE_ID, "name": "Pulseira 01", "wearer_name": "Sr. Joao"},
        headers=headers,
    )
    assert r.status_code == 201, r.text
    device = r.json()
    secret = device["shared_secret"]

    client.post(
        "/v1/auth/push-tokens",
        json={"token": "token-fcm-fake-do-celular-da-maria", "platform": "android"},
        headers=headers,
    )
    r = client.post(
        f"/v1/devices/{device['id']}/caregivers",
        json={"name": "Maria", "user_id": user_id, "priority": 1},
        headers=headers,
    )
    assert r.status_code == 201, r.text

    return {"headers": headers, "device": device, "secret": secret}


def build_raw_payload(
    secret_hex: str,
    seq: int,
    event: str = "fall",
    battery: int = 87,
    impact_dg: int = 32,
    ble_id: str = BLE_ID,
) -> str:
    """Reproduz em Python o que o firmware do nRF monta e assina."""
    signed = bytes(
        [
            1,
            *bytes.fromhex(ble_id),
            EVENT_CODES[event],
            seq & 0xFF,
            (seq >> 8) & 0xFF,
            battery,
            impact_dg,
        ]
    )
    signature = hmac.new(bytes.fromhex(secret_hex), signed, hashlib.sha256).digest()[:4]
    return (signed + signature).hex()


def test_queda_assinada_gera_alerta_e_notifica(client, setup):
    payload = build_raw_payload(setup["secret"], seq=10)
    r = client.post(
        "/v1/alerts",
        json={"ble_id": BLE_ID, "raw_payload": payload, "gateway_label": "Moto G"},
        headers=setup["headers"],
    )
    assert r.status_code == 201, r.text
    body = r.json()
    assert body["duplicate"] is False
    assert body["notified"] == 1  # canal console, mas o fan-out rodou
    assert body["alert"]["event_type"] == "fall"
    assert body["alert"]["impact_g"] == pytest.approx(3.2)
    assert body["alert"]["battery_pct"] == 87
    assert body["alert"]["status"] == "open"


def test_segundo_celular_nao_duplica_o_alerta(client, setup):
    """Dois aparelhos ouvem o mesmo anuncio: um unico alerta, uma unica notificacao."""
    payload = build_raw_payload(setup["secret"], seq=20)
    first = client.post(
        "/v1/alerts",
        json={"ble_id": BLE_ID, "raw_payload": payload},
        headers=setup["headers"],
    ).json()

    second = client.post(
        "/v1/alerts",
        json={
            "ble_id": BLE_ID,
            "raw_payload": payload,
            "latitude": -23.5505,
            "longitude": -46.6333,
        },
        headers=setup["headers"],
    ).json()

    assert second["duplicate"] is True
    assert second["notified"] == 0
    assert second["alert"]["id"] == first["alert"]["id"]
    # O duplicado ainda contribui com a localizacao que o primeiro nao tinha.
    assert second["alert"]["latitude"] == pytest.approx(-23.5505)


def test_assinatura_invalida_e_rejeitada(client, setup):
    """Um radio qualquer anunciando 'queda' sem a chave nao entra no sistema."""
    forjado = build_raw_payload("00" * 16, seq=30)
    r = client.post(
        "/v1/alerts",
        json={"ble_id": BLE_ID, "raw_payload": forjado},
        headers=setup["headers"],
    )
    assert r.status_code == 401


def test_replay_de_anuncio_antigo_e_rejeitado(client, setup):
    """Payload legitimo, mas com contador ja usado: gravacao retransmitida."""
    client.post(
        "/v1/alerts",
        json={"ble_id": BLE_ID, "raw_payload": build_raw_payload(setup["secret"], 100)},
        headers=setup["headers"],
    )
    # seq menor, fora da janela de deduplicacao por ser um evento diferente
    r = client.post(
        "/v1/alerts",
        json={"ble_id": BLE_ID, "raw_payload": build_raw_payload(setup["secret"], 50)},
        headers=setup["headers"],
    )
    assert r.status_code == 409


def test_ack_e_resolucao(client, setup):
    payload = build_raw_payload(setup["secret"], seq=200, event="panic")
    alert_id = client.post(
        "/v1/alerts",
        json={"ble_id": BLE_ID, "raw_payload": payload},
        headers=setup["headers"],
    ).json()["alert"]["id"]

    r = client.post(f"/v1/alerts/{alert_id}/ack", headers=setup["headers"])
    assert r.status_code == 200
    assert r.json()["status"] == "acked"
    assert r.json()["acked_at"] is not None

    r = client.post(
        f"/v1/alerts/{alert_id}/resolve",
        json={"status": "false_positive", "notes": "Bateu o braco na mesa."},
        headers=setup["headers"],
    )
    assert r.status_code == 200
    assert r.json()["status"] == "false_positive"


def test_alerta_sem_autenticacao_e_recusado(client):
    r = client.post("/v1/alerts", json={"ble_id": BLE_ID, "event_type": "fall", "seq": 1})
    assert r.status_code == 401
