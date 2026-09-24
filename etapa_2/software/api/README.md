# SysCare API

Backend do sistema portátil de detecção de quedas. Recebe os eventos que o
aplicativo ouviu por BLE, guarda o histórico e notifica os cuidadores.

## Onde cada peça roda

```
  ┌──────────────────┐   BLE advertising    ┌──────────────────┐   HTTPS    ┌─────────────┐
  │  Pulseira nRF52  │ ───────────────────► │  App Flutter     │ ─────────► │  SysCare    │
  │  IIM-42351 +     │  não-conectável      │  (celular)       │  POST      │  API        │
  │  algoritmo queda │  14 bytes assinados  │                  │  /v1/alerts│  (servidor) │
  └──────────────────┘                      └──────────────────┘            └──────┬──────┘
        bateria crítica                       pop-up + sirene                      │ FCM
        (µA importam aqui)                    imediatos, offline                   ▼
                                                                          celulares dos
                                                                            cuidadores
```

A API é um processo num servidor Linux — **nada dela roda no nRF**. O consumo da
pulseira depende do protocolo BLE (intervalo de anúncio, potência de TX,
tamanho do payload), documentado em [`docs/ble_payload.md`](docs/ble_payload.md).

## Subir em 4 comandos

```bash
python -m venv .venv && source .venv/Scripts/activate   # Linux/macOS: .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

Swagger em <http://localhost:8000/docs>. Sem Firebase configurado a API sobe
igual: os alertas saem no log em vez de virarem push (`"push": "console"` em
`/health`), o que permite desenvolver o app antes de existir conta no Firebase.

```bash
pytest -q
```

## Fluxo de uso

```bash
# 1. criar conta do responsável
curl -X POST localhost:8000/v1/auth/register -H 'Content-Type: application/json' \
  -d '{"email":"maria@exemplo.com","name":"Maria","password":"senha-forte-1"}'

# 2. cadastrar a pulseira -> devolve shared_secret UMA ÚNICA VEZ (grave no firmware)
curl -X POST localhost:8000/v1/devices -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"ble_id":"a1b2c3d4","name":"Pulseira 01","wearer_name":"Sr. João"}'

# 3. registrar o token FCM do celular e vincular o cuidador
curl -X POST localhost:8000/v1/auth/push-tokens ...
curl -X POST localhost:8000/v1/devices/$DEVICE_ID/caregivers ...

# 4. o app reporta a queda ouvida no ar
curl -X POST localhost:8000/v1/alerts -H "Authorization: Bearer $TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"ble_id":"a1b2c3d4","raw_payload":"01a1b2c3d401 0a00 57 20 1a2b3c4d","latitude":-23.55}'
```

## Endpoints

| Método | Rota | Para quê |
|---|---|---|
| POST | `/v1/auth/register` · `/login` | Conta do responsável/cuidador |
| GET | `/v1/auth/me` | Dados do usuário logado |
| POST/DELETE | `/v1/auth/push-tokens` | Token FCM do aparelho (registro/logout) |
| POST/GET | `/v1/devices` | Cadastrar e listar pulseiras |
| GET/PATCH | `/v1/devices/{id}` | Detalhe (dono e responsáveis) e edição (só o dono) |
| POST/GET/PATCH/DELETE | `/v1/devices/{id}/caregivers` | Quem é avisado (só o dono: a lista tem telefones de terceiros) |
| **POST** | **`/v1/alerts`** | **Reportar evento ouvido no BLE** |
| GET | `/v1/alerts` · `/v1/alerts/{id}` | Histórico |
| POST | `/v1/alerts/{id}/ack` | "Estou indo" — interrompe o escalonamento; dono ou qualquer responsável ativo |
| POST | `/v1/alerts/{id}/resolve` | Encerrar: `resolved` ou `false_positive` |
| POST | `/v1/telemetry` | Lote de heartbeats `0x05` (bateria, presença), deduplicado por janela de 60 s |
| GET | `/health` | Liveness |

## As três decisões de projeto que importam

**1. Deduplicação.** BLE broadcast é ouvido por todos os celulares no alcance, e
todos vão reportar. A API deduplica por `(device_id, seq)` numa janela de 180 s:
um alerta, uma notificação — mas aproveitando o GPS de quem quer que o tenha.

**2. Assinatura.** Sem HMAC, qualquer rádio próximo forja um alerta de queda. A
pulseira assina o payload com uma chave gravada no firmware; o celular só
repassa os bytes. Ligue com `SYSCARE_REQUIRE_HMAC=true` assim que o firmware
implementar. Durante o desenvolvimento, deixe `false` e a API aceita payloads sem
assinatura, registrando um aviso no log.

**3. Escalonamento.** Notificação entregue ≠ socorro a caminho. Enquanto ninguém
chamar `/ack`, a API reenvia o alerta a cada `SYSCARE_ESCALATION_SECONDS`, até
`SYSCARE_ESCALATION_MAX_ROUNDS` rodadas.

## Testar o canal de SMS

```bash
python scripts/send_test_sms.py +5548991791826
```

Sem credenciais, roda em **dry-run**: mostra o texto exato, quantos segmentos
seriam cobrados e o custo estimado, sem enviar nada. Para o dispatcher também
simular SMS num alerta de verdade, use `SYSCARE_SMS_PROVIDER=dryrun`.

Para enviar **de verdade** é preciso conta num provedor — não existe SMS grátis
por API. Com uma conta trial do Twilio:

```env
SYSCARE_SMS_PROVIDER=twilio
SYSCARE_TWILIO_ACCOUNT_SID=AC...
SYSCARE_TWILIO_AUTH_TOKEN=...
SYSCARE_TWILIO_FROM_NUMBER=+1...
```

Duas armadilhas do trial: só envia para números **verificados** no console, e
prefixa o texto com "Sent from your Twilio trial account". E, para o Brasil,
tráfego A2P de long code não registrado pode ser filtrado pelas operadoras —
"aceito pelo Twilio" não é o mesmo que "entregue".

**Quando o SMS dispara.** Push vai para todo cuidador com o app, sempre. SMS
custa dinheiro, então entra só em dois casos: cuidador **sem** app (é o único
jeito de alcançá-lo) ou **escalonamento** — ninguém confirmou e o push não
bastou. Controlado por `SYSCARE_SMS_FROM_ESCALATION_ROUND`.

**Por que o texto do SMS não tem acento.** Um único caractere fora do GSM-7
("ã", "ç", emoji) joga a mensagem inteira para UCS-2 e derruba o limite de 160
para 70 caracteres — um acento inocente triplica o custo do alerta. O
`render_sms_body()` normaliza para ASCII e corta em um segmento.

## Alerta local é responsabilidade do app, não da API

O pop-up e a sirene do celular que ouviu o BLE devem disparar **antes** de
qualquer chamada de rede, e funcionar sem internet. Se o `POST /v1/alerts`
falhar, o app enfileira o envio e continua alertando localmente. A comparação de
canais (incluindo o fallback grátis de SMS/ligação pelo próprio celular) está em
[`docs/canais_de_alerta.md`](docs/canais_de_alerta.md).

## Para a Etapa 3

- [ ] Migrar de `create_all` para **Alembic** — `create_all` não altera tabelas existentes.
- [ ] Trocar SQLite por **PostgreSQL**.
- [ ] Rate limit no `POST /v1/alerts`.
- [ ] Escalonador em worker externo, se rodar mais de uma instância da API.
- [ ] Endpoint de exportação da janela de aceleração, para calibrar o detector
      cruzando sinal bruto com o desfecho (`false_positive` vs `resolved`).

## Estrutura

```
app/
  main.py            aplicação, CORS, ciclo de vida, /health
  config.py          configuração por variável de ambiente
  database.py        engine e sessão SQLAlchemy
  models.py          User, Device, Caregiver, PushToken, Alert, AlertDelivery, Telemetry
  schemas.py         contratos de entrada/saída (gera o OpenAPI)
  security.py        JWT, hash de senha, HMAC do payload da pulseira
  ble.py             formato canônico do advertising
  deps.py            usuário autenticado, posse do device
  escalation.py      reenvio enquanto não houver confirmação
  routers/           auth, devices, alerts, telemetry
  notifications/     base (interface), fcm, sms, dispatcher
scripts/
  send_test_sms.py   teste do canal de SMS (dry-run ou envio real)
  gerar_vetores_ble.py vetores de teste do anúncio para o app; payload para o nRF Connect
docs/
  ble_payload.md     protocolo BLE, energia, esqueleto do firmware
  canais_de_alerta.md comparativo de custo dos canais
tests/
  test_alerts.py     duplicação, falsificação, replay, ack/resolve, heartbeat
  test_sms.py        renderização GSM-7, contagem de segmentos, truncamento
```
