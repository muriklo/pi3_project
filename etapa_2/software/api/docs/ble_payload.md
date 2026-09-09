# Protocolo BLE do SysCare — advertising e orçamento de energia

Este documento é o contrato entre **firmware (nRF Connect SDK)**, **app (Flutter)** e
**API**. A implementação de referência do lado servidor está em
[`app/ble.py`](../app/ble.py) e [`app/security.py`](../app/security.py).

## 1. Por que broadcast (e não conexão)

Uma conexão BLE exige *scan* → *connect* → *discovery de serviços* → *pareamento*.
Na prática são centenas de milissegundos a alguns segundos, com o rádio ligado o
tempo todo, e a conexão precisa ser mantida (eventos de *connection interval*
periódicos) mesmo quando nada acontece.

O advertising não-conectável, ao contrário, é o modo mais barato do BLE: o rádio
liga por ~1–3 ms, transmite em três canais e desliga. Para um evento único e
urgente como uma queda, é a escolha certa — e ainda alcança **qualquer** celular
próximo ao mesmo tempo, não só um pareado.

O preço é que broadcast **não tem confirmação nem autenticação nativas**. As
seções 3 e 4 resolvem isso.

## 2. Formato do anúncio

AD type `0xFF` (Manufacturer Specific Data), Company ID `0xFFFF` — reservado pela
Bluetooth SIG para desenvolvimento e testes, adequado para um projeto acadêmico.

```
AD structure:  [len=0x11][type=0xFF][CID lo=0xFF][CID hi=0xFF][ payload de 14 bytes ]
```

Total: 2 + 14 = 16 bytes de dados + 2 de cabeçalho = **18 bytes**, folgados dentro
dos 31 bytes do advertising legado. Não é preciso *scan response* nem *extended
advertising* do BLE 5 — o que também mantém a compatibilidade com celulares antigos.

### Payload (14 bytes, little-endian)

| Offset | Bytes | Campo | Descrição |
|---|---|---|---|
| 0 | 1 | `version` | Versão do protocolo. Hoje `0x01`. |
| 1–4 | 4 | `ble_id` | Identificador único da pulseira, gravado em fábrica. |
| 5 | 1 | `event_type` | `0x00` teste · `0x01` queda · `0x02` botão de pânico · `0x03` imobilidade · `0x04` bateria baixa |
| 6–7 | 2 | `seq` | Contador `uint16` monotônico, incrementado a cada evento. |
| 8 | 1 | `battery_pct` | Bateria em % (0–100). |
| 9 | 1 | `impact_dg` | Pico de aceleração em décimos de g (satura em 25,5 g). |
| 10–13 | 4 | `signature` | HMAC-SHA256 truncado sobre os bytes 0–9. |

Os bytes 0–9 são exatamente o que entra no HMAC — ver `build_signed_payload()`.

## 3. Autenticação: por que o HMAC é obrigatório

Sem assinatura, **qualquer** celular com um app de BLE genérico consegue anunciar
`event_type=0x01` com o `ble_id` de uma pulseira alheia e disparar um alerta falso
de queda. Num sistema de emergência, alerta falso repetido treina o cuidador a
ignorar o alerta — é uma falha de segurança que vira falha de segurança do paciente.

A pulseira guarda uma chave simétrica de 16 bytes, gerada pela API no cadastro
(`POST /v1/devices` devolve `shared_secret` **uma única vez**) e gravada no
firmware. Cada anúncio leva `HMAC-SHA256(chave, bytes[0..9])[:4]`.

Quatro bytes dão 1 chance em 4,3 bilhões de acerto às cegas. Como o `seq` também
precisa avançar, um atacante não consegue nem forçar bruta por repetição.

No nRF52840 há o CryptoCell (CC310); em nRF52832 use a implementação em software
do mbedTLS que já vem no nRF Connect SDK — um HMAC-SHA256 sobre 10 bytes é
irrelevante frente ao custo do rádio.

## 4. Anti-replay e deduplicação

**Replay.** O `seq` nunca retrocede. A API guarda o último aceito em
`Device.last_seq` e rejeita com `409` qualquer valor menor (com margem de 1000
para o *wrap-around* natural do contador de 16 bits). Assim, gravar um anúncio de
queda no ar e retransmiti-lo depois não funciona.

**Duplicação.** O anúncio de emergência é repetido por dezenas de segundos e pode
ser ouvido por vários celulares — todos vão chamar `POST /v1/alerts`. A API
deduplica por `(device_id, seq)` dentro de `SYSCARE_DEDUP_WINDOW_SECONDS` (180 s
por padrão) e responde `duplicate: true` sem notificar de novo. O segundo relato
ainda é útil: se ele traz GPS e o primeiro não trazia, a localização é
aproveitada.

**O `seq` deve sobreviver ao reset.** Grave-o em flash/NVS a cada evento (são
poucos por dia, não desgasta a memória). Se o contador zerar num reset, os
eventos seguintes cairão na regra anti-replay.

## 5. Intervalos de advertising e orçamento de energia

É **aqui** que se ganha ou se perde autonomia — não na escolha da linguagem da API.

| Estado | Intervalo | Duração | Observação |
|---|---|---|---|
| Repouso (heartbeat) | 2000 ms | contínuo | Prova de presença e bateria. |
| **Emergência** | **100 ms** | 30 s | Máxima chance de o celular ouvir na primeira varredura. |
| Emergência (sustentada) | 500 ms | +5 min | Cobre o caso de o cuidador chegar depois. |
| Pós-confirmação | volta a 2000 ms | — | Ver seção 6. |

Ordem de grandeza (nRF52832, TX 0 dBm, 3 canais, ~1,5 ms por evento de anúncio,
pico ~5 mA): a 2000 ms o advertising fica em torno de alguns µA de média — bem
abaixo do consumo do acelerômetro em modo *wake-on-motion*. A 100 ms sobe para
dezenas de µA, mas só por 30 segundos. **O advertising de repouso não é o que
domina o consumo; o acelerômetro e o algoritmo de detecção é que dominam.**

Meça de verdade com o Power Profiler Kit II antes de fechar o número no relatório
da Etapa 4 — os valores acima são para dimensionar, não para citar.

Duas economias que valem mais que ajustar o intervalo:
- **`BT_LE_ADV_OPT_USE_IDENTITY` desligado / endereço aleatório resolvível**: sem
  impacto de energia, mas evita rastrear o usuário pelo endereço MAC fixo.
- **TX power**: −4 dBm em vez de +4 dBm corta consumo do rádio e ainda cobre uma
  casa. Só suba a potência se os testes de alcance exigirem.

## 6. O que o broadcast não resolve: confirmação e dados brutos

Broadcast é unidirecional — a pulseira **não sabe** se alguém ouviu. Duas
consequências de projeto:

1. **A pulseira precisa de feedback local.** Buzzer/vibração dizendo "alerta
   enviado", e um botão de cancelar dentro de ~10 s antes de anunciar (mata o
   falso positivo na origem, antes de incomodar ninguém).
2. **Para confirmar de fato**, adote o padrão *broadcast + connect*: durante a
   emergência, a pulseira anuncia **também** como conectável. O app, depois de
   tratar o alerta, conecta e (a) confirma o recebimento, para a pulseira parar o
   anúncio agressivo e economizar bateria; (b) baixa a janela de aceleração de
   ±5 s em torno do impacto, para análise posterior do algoritmo.

O item (b) é o que dá material para calibrar o detector na Etapa 4: cruzando a
janela de sinal com o desfecho registrado em `POST /v1/alerts/{id}/resolve`
(`false_positive` vs `resolved`), vocês montam um dataset rotulado do próprio
protótipo.

## 7. Esqueleto do firmware (nRF Connect SDK / Zephyr)

```c
#define SYSCARE_COMPANY_ID   0xFFFF
#define SYSCARE_PROTO_VER    0x01

static uint8_t adv_payload[14];

/* bytes 0..9 = dados, 10..13 = HMAC truncado */
static void syscare_build_payload(uint8_t event, uint16_t seq,
                                  uint8_t batt, uint8_t impact_dg)
{
    adv_payload[0] = SYSCARE_PROTO_VER;
    memcpy(&adv_payload[1], device_ble_id, 4);   /* gravado em fábrica */
    adv_payload[5] = event;
    adv_payload[6] = seq & 0xFF;
    adv_payload[7] = seq >> 8;
    adv_payload[8] = batt;
    adv_payload[9] = impact_dg;

    uint8_t mac[32];
    /* mbedtls_md_hmac(SHA256, shared_secret, 16, adv_payload, 10, mac) */
    memcpy(&adv_payload[10], mac, 4);
}

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_mfg_buf, sizeof(adv_mfg_buf)),
};

/* repouso: não-conectável, 2000 ms */
#define ADV_IDLE  BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_IDENTITY, \
                                  BT_GAP_ADV_SLOW_INT_MIN, BT_GAP_ADV_SLOW_INT_MAX, NULL)

/* emergência: 100 ms — 0x00A0 = 160 * 0,625 ms */
#define ADV_ALERT BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_IDENTITY, 0x00A0, 0x00B0, NULL)
```

Trocar de intervalo é `bt_le_adv_stop()` seguido de `bt_le_adv_start()` com o
outro parâmetro.

## 8. Teste sem hardware

O arquivo [`tests/test_alerts.py`](../tests/test_alerts.py) tem
`build_raw_payload()`, que monta e assina o payload em Python exatamente como o
firmware deve fazer. Use-o para validar a API antes de a pulseira existir — e,
depois, para conferir byte a byte se o firmware está assinando certo.
