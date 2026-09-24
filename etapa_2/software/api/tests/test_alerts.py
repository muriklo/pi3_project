"""Testes do fluxo critico: um broadcast de queda vira alerta e notificacao.

Cobre os tres riscos do BLE broadcast: duplicacao, falsificacao e replay.
Execute com:  pytest -q
"""
from __future__ import annotations

import hashlib
import hmac
import os
import tempfile
from datetime import datetime, timedelta, timezone

import pytest
from fastapi.testclient import TestClient

# Precisa vir antes de importar o app: a config e lida na importacao.
_DB_FD, _DB_PATH = tempfile.mkstemp(suffix=".db")
os.environ["SYSCARE_DATABASE_URL"] = f"sqlite:///{_DB_PATH}"
os.environ["SYSCARE_JWT_SECRET"] = "secret-de-teste"
os.environ["SYSCARE_REQUIRE_HMAC"] = "true"
os.environ["SYSCARE_FCM_CREDENTIALS_FILE"] = ""

from app.ble import EVENT_CODES, decode_payload  # noqa: E402
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


# --------------------------------------------------------------------------- #
# Heartbeat (event_type 0x05) e telemetria
# --------------------------------------------------------------------------- #
def _minuto_cheio() -> datetime:
    agora = datetime.now(timezone.utc)
    return agora.replace(second=0, microsecond=0)


def test_heartbeat_e_decodificado_como_heartbeat(setup):
    raw = build_raw_payload(setup["secret"], seq=200, event="heartbeat", impact_dg=0)
    assert decode_payload(bytes.fromhex(raw))["event_type"] == "heartbeat"


def test_heartbeat_nao_vira_alerta(client, setup):
    """O heartbeat repete o seq do ultimo evento: se virasse alerta, seria falso."""
    raw = build_raw_payload(setup["secret"], seq=200, event="heartbeat", impact_dg=0)
    r = client.post(
        "/v1/alerts",
        json={"ble_id": BLE_ID, "raw_payload": raw},
        headers=setup["headers"],
    )
    assert r.status_code == 422
    assert "telemetry" in r.json()["detail"]


def test_telemetria_deduplica_por_janela_e_nao_por_seq(client, setup):
    """Dois celulares ouvem o mesmo heartbeat; minutos depois o seq e o mesmo."""
    base = _minuto_cheio()

    def envia(recorded_at: datetime, battery: int) -> dict:
        r = client.post(
            "/v1/telemetry",
            json={
                "ble_id": BLE_ID,
                "samples": [
                    {
                        "seq": 200,
                        "recorded_at": recorded_at.isoformat(),
                        "battery_pct": battery,
                        "rssi": -70,
                    }
                ],
            },
            headers=setup["headers"],
        )
        assert r.status_code == 202, r.text
        return r.json()

    # Mesma janela de 60 s, celulares diferentes: uma amostra so.
    assert envia(base + timedelta(seconds=5), 81) == {"accepted": 1, "duplicates": 0}
    assert envia(base + timedelta(seconds=7), 81) == {"accepted": 0, "duplicates": 1}
    # Janela seguinte, MESMO seq (nenhum evento novo): precisa ser aceito.
    assert envia(base + timedelta(seconds=65), 80) == {"accepted": 1, "duplicates": 0}

    device = client.get(
        f"/v1/devices/{setup['device']['id']}", headers=setup["headers"]
    ).json()
    assert device["battery_pct"] == 80


# --------------------------------------------------------------------------- #
# Responsavel que nao e o dono da pulseira
# --------------------------------------------------------------------------- #
def _conta(client, email: str, nome: str) -> dict:
    r = client.post(
        "/v1/auth/register",
        json={"email": email, "name": nome, "password": "senha-forte-1"},
    )
    assert r.status_code == 201, r.text
    return {
        "id": r.json()["user"]["id"],
        "headers": {"Authorization": f"Bearer {r.json()['access_token']}"},
    }


@pytest.fixture(scope="module")
def cuidador(client, setup):
    """Joao recebe o push da pulseira da Maria, mas nao e o dono dela."""
    joao = _conta(client, "joao@exemplo.com", "Joao")
    r = client.post(
        f"/v1/devices/{setup['device']['id']}/caregivers",
        json={"name": "Joao", "user_id": joao["id"], "priority": 2},
        headers=setup["headers"],
    )
    assert r.status_code == 201, r.text
    joao["caregiver_id"] = r.json()["id"]
    return joao


def _novo_alerta(client, setup, seq: int) -> str:
    r = client.post(
        "/v1/alerts",
        json={"ble_id": BLE_ID, "raw_payload": build_raw_payload(setup["secret"], seq)},
        headers=setup["headers"],
    )
    assert r.status_code == 201, r.text
    return r.json()["alert"]["id"]


def test_responsavel_ve_a_pulseira_e_o_historico(client, setup, cuidador):
    device_id = setup["device"]["id"]
    alert_id = _novo_alerta(client, setup, 300)

    pulseiras = client.get("/v1/devices", headers=cuidador["headers"]).json()
    assert [d["id"] for d in pulseiras] == [device_id]
    # O app usa owner_id para saber se mostra as acoes de edicao.
    assert pulseiras[0]["owner_id"] != cuidador["id"]
    r = client.get(f"/v1/devices/{device_id}", headers=cuidador["headers"])
    assert r.status_code == 200
    historico = client.get("/v1/alerts", headers=cuidador["headers"]).json()
    assert alert_id in [a["id"] for a in historico]
    r = client.get(f"/v1/alerts/{alert_id}", headers=cuidador["headers"])
    assert r.status_code == 200


def test_responsavel_confirma_e_encerra_o_alerta(client, setup, cuidador):
    """O "Estou indo" de qualquer responsavel interrompe o reenvio (Etapa 1)."""
    alert_id = _novo_alerta(client, setup, 301)

    r = client.post(f"/v1/alerts/{alert_id}/ack", headers=cuidador["headers"])
    assert r.status_code == 200, r.text
    assert r.json()["status"] == "acked"
    assert r.json()["acked_by_user_id"] == cuidador["id"]

    r = client.post(
        f"/v1/alerts/{alert_id}/resolve",
        json={"status": "resolved"},
        headers=cuidador["headers"],
    )
    assert r.status_code == 200
    assert r.json()["status"] == "resolved"


def test_responsavel_nao_edita_a_pulseira(client, setup, cuidador):
    """Editar e ver a lista de responsaveis (com telefones) continua so do dono."""
    device_id = setup["device"]["id"]
    r = client.patch(
        f"/v1/devices/{device_id}", json={"name": "Outra"}, headers=cuidador["headers"]
    )
    assert r.status_code == 404
    r = client.get(f"/v1/devices/{device_id}/caregivers", headers=cuidador["headers"])
    assert r.status_code == 404


def test_estranho_nao_ve_nem_confirma(client, setup):
    estranho = _conta(client, "estranho@exemplo.com", "Estranho")
    alert_id = _novo_alerta(client, setup, 302)

    assert client.get("/v1/devices", headers=estranho["headers"]).json() == []
    assert client.get("/v1/alerts", headers=estranho["headers"]).json() == []
    r = client.post(f"/v1/alerts/{alert_id}/ack", headers=estranho["headers"])
    assert r.status_code == 404


def test_responsavel_desativado_perde_o_acesso(client, setup, cuidador):
    alert_id = _novo_alerta(client, setup, 303)
    r = client.patch(
        f"/v1/devices/{setup['device']['id']}/caregivers/{cuidador['caregiver_id']}",
        json={"active": False},
        headers=setup["headers"],
    )
    assert r.status_code == 200, r.text

    r = client.get(f"/v1/alerts/{alert_id}", headers=cuidador["headers"])
    assert r.status_code == 404
    assert client.get("/v1/devices", headers=cuidador["headers"]).json() == []
