
/*
 * SysCare - firmware integrado: acelerometro + advertising BLE
 * XIAO nRF52840 / Zephyr / NCS 3.4.0
 *
 * A cada evento:
 *   1. Queda livre detectada pelo acelerometro (IIM-42351), ou
 *      heartbeat depois de 10 segundos sem queda
 *   2. Monta o payload SysCare
 *   3. Atualiza os dados do advertising BLE
 *   4. Continua transmitindo sem parar o advertising
 *
 * O advertising e iniciado uma unica vez. O acelerometro vem do modulo
 * lib/ (servico srv_accelerometer); se ele falhar, o firmware segue so
 * com os heartbeats.
 *
 * Os bytes 10..13 levam o HMAC-SHA256 truncado dos bytes 0..9, com a
 * chave gerada pela API no cadastro da pulseira (src/chave_pulseira.h,
 * fora do git). Sem a assinatura, a API recusa a queda (401) e nao avisa
 * os responsaveis. Contrato: etapa_3/software/api/docs/ble_payload.md.
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/settings/settings.h>

#include <psa/crypto.h>

#include "srv_accelerometer.h"

/* =========================================================
 * Protocolo SysCare
 * ========================================================= */

#define SYSCARE_COMPANY_ID       0xFFFF
#define SYSCARE_PROTO_VER        0x01

/* Tipos de eventos SysCare */

#define SYSCARE_EVT_TEST         0x00
#define SYSCARE_EVT_FALL         0x01
#define SYSCARE_EVT_PANIC        0x02
#define SYSCARE_EVT_IMMOBILITY   0x03
#define SYSCARE_EVT_LOW_BATTERY  0x04
#define SYSCARE_EVT_HEARTBEAT    0x05

/*
 * Outros eventos previstos no protocolo:
 * 0x00 - Teste
 * 0x01 - Queda
 * 0x02 - Botao de panico
 * 0x03 - Imobilidade
 * 0x04 - Bateria baixa
 * 0x05 - Heartbeat
 */

/* =========================================================
 * Parametros
 * ========================================================= */

/* Sem queda, um heartbeat a cada 10 segundos */
#define HEARTBEAT_PERIOD         K_SECONDS(10)

/*
 * Intervalo de advertising do teste:
 * 0x00A0 = 100 ms
 * 0x00B0 = 110 ms
 *
 * Usamos um intervalo curto para permitir varias
 * transmissoes enquanto o advertising estiver ativo.
 */

#define ADV_TEST \
    BT_LE_ADV_PARAM( \
        BT_LE_ADV_OPT_USE_IDENTITY, \
        0x00A0, \
        0x00B0, \
        NULL)

/* =========================================================
 * Identificacao da pulseira
 * ========================================================= */

static const uint8_t device_ble_id[4] = {
    0x01, 0x02, 0x03, 0x04
};

/*
 * Chave do HMAC: a que o app mostra (e a API guarda) no cadastro da
 * pulseira. Fica em src/chave_pulseira.h, que nao vai para o git, no
 * formato:
 *   #define SYSCARE_CHAVE_PULSEIRA { 0x.., ... 16 bytes ... }
 * Sem o arquivo, compila com a chave de teste 00..0F, que a API recusa.
 */
#if __has_include("chave_pulseira.h")
#include "chave_pulseira.h"
#else
#warning "src/chave_pulseira.h ausente: usando a chave de teste 00..0F"
#define SYSCARE_CHAVE_PULSEIRA { \
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, \
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F }
#endif

static const uint8_t shared_secret[16] = SYSCARE_CHAVE_PULSEIRA;

static psa_key_id_t hmac_key;

/* =========================================================
 * Numero de sequencia
 * ========================================================= */

/*
 * Pelo contrato, o seq so avanca em evento (queda); o heartbeat repete o
 * valor atual. Ele fica gravado na flash: se zerasse a cada reinicio, o
 * app e a API tratariam as quedas seguintes como repeticao de um anuncio
 * antigo e nao alarmariam.
 *
 * Sem valor gravado (primeira gravacao do firmware), comeca em 1000, a
 * frente dos numeros usados nos testes anteriores.
 */
#define SEQ_INICIAL 1000

static uint16_t seq = SEQ_INICIAL;

static int seq_set(const char *name, size_t len,
                   settings_read_cb read_cb, void *cb_arg)
{
    if (settings_name_steq(name, "seq", NULL) && len == sizeof(seq)) {
        return read_cb(cb_arg, &seq, sizeof(seq)) == sizeof(seq) ? 0 : -EIO;
    }

    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(syscare, "syscare", NULL, seq_set, NULL, NULL);

static void seq_load(void)
{
    int err = settings_subsys_init();

    if (!err) {
        err = settings_load_subtree("syscare");
    }

    if (err) {
        printk("Falha ao ler o seq da flash (%d): usando %u\n",
               err, (unsigned int)seq);
        return;
    }

    printk("Seq atual: %u\n", (unsigned int)seq);
}

static void seq_save(void)
{
    int err = settings_save_one("syscare/seq", &seq, sizeof(seq));

    if (err) {
        printk("Falha ao gravar o seq na flash: %d\n", err);
    }
}

/* =========================================================
 * Criptografia
 * ========================================================= */

static int syscare_crypto_init(void)
{
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_status_t status;

    status = psa_crypto_init();

    if (status != PSA_SUCCESS) {
        printk("Falha no psa_crypto_init: %d\n", status);
        return -EIO;
    }

    /* Chave HMAC de 128 bits, so para assinar */
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attr, 8 * sizeof(shared_secret));

    status = psa_import_key(&attr, shared_secret, sizeof(shared_secret), &hmac_key);

    psa_reset_key_attributes(&attr);

    if (status != PSA_SUCCESS) {
        printk("Falha no psa_import_key: %d\n", status);
        return -EIO;
    }

    return 0;
}

/* =========================================================
 * Buffers
 * ========================================================= */

/*
 * Payload SysCare: 14 bytes
 *
 * [0]       Versao
 * [1..4]    ID da pulseira
 * [5]       Evento:
 *             0x00 teste
 *             0x01 queda
 *             0x02 botao de panico
 *             0x03 imobilidade
 *             0x04 bateria baixa
 *             0x05 heartbeat
 * [6..7]    Sequencia (little-endian)
 * [8]       Bateria
 * [9]       Impacto
 * [10..13]  HMAC-SHA256 dos bytes 0..9, truncado em 4 bytes
 *
 * O app e o servidor usam o ID da pulseira junto com a
 * sequencia para identificar retransmissoes do mesmo
 * evento e evitar alarmes e SMS duplicados.
 */

static uint8_t adv_payload[14];

/*
 * Manufacturer Data:
 * 2 bytes de Company ID + 14 bytes de payload = 16 bytes.
 *
 * [0..1]  Company ID (0xFFFF, little-endian)
 * [2..15] Payload SysCare
 */

static uint8_t adv_mfg_buf[16];

/* =========================================================
 * Montagem do payload
 * ========================================================= */

static int syscare_build_payload(
                                uint8_t event,
                                uint16_t seq,
                                uint8_t batt,
                                uint8_t impact_dg)
{
    uint8_t mac[32];
    size_t mac_len;
    psa_status_t status;

    memset(adv_payload, 0, sizeof(adv_payload));

    // Byte 0: versao do protocolo=
    adv_payload[0] = SYSCARE_PROTO_VER;

    // Bytes 1 a 4: identificador da pulseira
    memcpy(&adv_payload[1],
           device_ble_id,
           sizeof(device_ble_id));

    // Byte 5: tipo do evento 
    adv_payload[5] = event;

    // Bytes 6 e 7: sequencia little-endian 
    adv_payload[6] = (uint8_t)(seq & 0xFF);
    adv_payload[7] = (uint8_t)((seq >> 8) & 0xFF);

    // Byte 8: bateria 
    adv_payload[8] = batt;

    // Byte 9: impacto 
    adv_payload[9] = impact_dg;

    /*
     * Bytes 10 a 13: os 4 primeiros bytes do HMAC-SHA256 dos
     * bytes 0..9. Sem a assinatura, o pacote nao vai ao ar.
     */

    status = psa_mac_compute(
        hmac_key,
        PSA_ALG_HMAC(PSA_ALG_SHA_256),
        adv_payload,
        10,
        mac,
        sizeof(mac),
        &mac_len
    );

    if (status != PSA_SUCCESS) {
        printk("Falha no HMAC: %d\n", status);
        return -EIO;
    }

    memcpy(&adv_payload[10], mac, 4);

    return 0;
}

/* =========================================================
 * Impressao do payload (so para imprimir na tela)
 * ========================================================= */

// static void syscare_print_payload(void)
// {
//     printk("Payload SysCare:");

//     for (size_t i = 0; i < sizeof(adv_payload); i++) {
//         printk(" %02X", adv_payload[i]);
//     }

//     printk("\n");
// }

LOG_MODULE_REGISTER(syscare, LOG_LEVEL_INF);

static void syscare_print_payload(void)
{
    LOG_HEXDUMP_INF(
        adv_payload,
        sizeof(adv_payload),
        "Payload SysCare"
    );
}

/* =========================================================
 * Montagem do Manufacturer Data
 * ========================================================= */

static void syscare_build_mfg_data(void)
{
    /*
     * Identificador Company ID 0xFFFF em little-endian.
     */

    adv_mfg_buf[0] =
        (uint8_t)(SYSCARE_COMPANY_ID & 0xFF);

    adv_mfg_buf[1] =
        (uint8_t)((SYSCARE_COMPANY_ID >> 8) & 0xFF);

    /*
     * Copia os 14 bytes do payload e coloca o Company ID
     * na frente, totalizando 16 bytes de Manufacturer Data.
     */

    memcpy(&adv_mfg_buf[2],
           adv_payload,
           sizeof(adv_payload));
}

/* =========================================================
 * Dados anunciados via BLE
 * ========================================================= */

static const struct bt_data ad[] = {
    BT_DATA_BYTES(
        BT_DATA_FLAGS,
        BT_LE_AD_NO_BREDR
    ),

    BT_DATA(
        BT_DATA_NAME_COMPLETE,
        "SYSCARE",
        sizeof("SYSCARE") - 1
    ),

    BT_DATA(
        BT_DATA_MANUFACTURER_DATA,
        adv_mfg_buf,
        sizeof(adv_mfg_buf)
    ),
};

/* =========================================================
 * Acelerometro
 * ========================================================= */

/*
 * Deteccao de queda livre, com os parametros validados no acc_validation:
 * inicio abaixo de 500 mg, impacto acima de 3 g, quedas de 13 cm a ~2 m
 * e 1 s de debounce depois do impacto.
 */

static acc_freefall_config_t freefall_cfg = {
    .lowg_mg = 500,
    .highg_mg = 3000,
    .min_height_cm = 13,
    .max_height_cm = 201,
    .debounce_ms = 1000,
};

/* Eventos do acelerometro: o callback roda na workqueue do sistema */
K_MSGQ_DEFINE(acc_event_q, sizeof(acc_event_t), 8, 4);

static void on_acc_event(const acc_event_t *evt)
{
    (void)k_msgq_put(&acc_event_q, evt, K_NO_WAIT);
}

static int accelerometer_start(void)
{
    int err;

    err = srv_accelerometer_init(on_acc_event);

    if (err) {
        printk("Falha ao inicializar o acelerometro: %d\n", err);
        return err;
    }

    err = srv_accelerometer_freefall_enable(&freefall_cfg);

    if (err) {
        printk("Falha ao ligar a deteccao de queda: %d\n", err);
        return err;
    }

    printk("Deteccao de queda ligada (%u a %u cm).\n",
           freefall_cfg.min_height_cm,
           freefall_cfg.max_height_cm);

    return 0;
}

/* =========================================================
 * Main
 * ========================================================= */

int main(void)
{
    int err;

    uint8_t batt = 100;
    uint8_t impact_dg = 0;

    printk("\n");
    printk("=================================\n");
    printk("        SysCare Firmware\n");
    printk("=================================\n");

    /* Inicializa o Bluetooth uma unica vez */
    err = bt_enable(NULL);

    if (err) {
        printk("Falha ao inicializar Bluetooth: %d\n", err);
        return 0;
    }

    printk("Bluetooth inicializado!\n");
    printk("Advertising por evento do acelerometro.\n");
    printk("Heartbeat a cada 10 segundos sem queda.\n");
    printk("Advertising permanecera ativo continuamente.\n");

    /* Sem a chave importada nao ha como assinar: nao anuncia */
    err = syscare_crypto_init();

    if (err) {
        return 0;
    }

    /* Retoma a sequencia de onde parou antes de desligar */
    seq_load();

    /*
     * Monta o payload inicial antes de iniciar o advertising,
     * garantindo que os dados estejam preparados.
     */

    err = syscare_build_payload(
        SYSCARE_EVT_HEARTBEAT,
        seq,
        batt,
        impact_dg
    );

    if (err) {
        printk("Falha ao montar payload inicial: %d\n", err);
        return 0;
    }

    /* Prepara os dados que serao anunciados */
    syscare_build_mfg_data();

    /* Mostra os 14 bytes no console */
    syscare_print_payload();

    /*
     * Inicia o advertising somente uma vez.
     *
     * Depois disso, os dados serao atualizados com
     * bt_le_adv_update_data(), sem parar a transmissao.
     */

    printk("Iniciando advertising BLE...\n");

    err = bt_le_adv_start(
        ADV_TEST,
        ad,
        ARRAY_SIZE(ad),
        NULL,
        0
    );

    if (err) {
        printk("Falha ao iniciar advertising: %d\n", err);
        return 0;
    }

    printk("Advertising iniciado com sucesso!\n");

    /*
     * Liga o acelerometro depois do advertising: se ele
     * falhar, a pulseira continua mandando heartbeats.
     */

    (void)accelerometer_start();

    while (1) {

        acc_event_t acc_evt;
        bool fall_detected = false;

        /*
         * Aguarda uma queda por ate 10 segundos.
         * Sem queda nesse tempo, o evento e um heartbeat.
         */

        printk("\nAguardando proximo evento...\n");

        if (k_msgq_get(&acc_event_q, &acc_evt, HEARTBEAT_PERIOD) == 0) {

            /* So a queda vira mensagem; outros eventos sao ignorados */
            if (acc_evt.type != ACC_EVENT_FREEFALL) {
                continue;
            }

            fall_detected = true;
        }

        uint8_t event;

        if (fall_detected) {

            /* Queda detectada pelo acelerometro */
            event = SYSCARE_EVT_FALL;

            /* Novo evento: avanca a sequencia e guarda na flash */
            seq++;
            seq_save();

            /*
             * O sensor nao mede a intensidade do impacto, so
             * a duracao da queda (e dela a altura). Mantem o
             * valor fixo da simulacao (15,0 g) para o app se
             * comportar como no teste anterior.
             */
            impact_dg = 150;

            printk("\n");
            printk("=================================\n");
            printk("QUEDA DETECTADA!\n");
            printk("Duracao: %u ms, altura estimada: ~%u cm\n",
                   (unsigned int)acc_evt.duration_ms,
                   (unsigned int)acc_evt.height_cm);
            printk("Sequencia: %u\n", (unsigned int)seq);
            printk("=================================\n");

        } else {

            /* Evento normal: heartbeat, repete o seq do ultimo evento */
            event = SYSCARE_EVT_HEARTBEAT;
            impact_dg = 0;

            printk("\nHeartbeat normal.\n");
            printk("Sequencia: %u\n", (unsigned int)seq);
        }

        /* Simula uma pequena reducao da bateria */
        if (batt > 0) {
            batt--;
        }

        /* Monta o payload SysCare */
        err = syscare_build_payload(
            event,
            seq,
            batt,
            impact_dg
        );

        if (err) {
            printk("Falha ao montar payload: %d\n", err);
            continue;
        }

        /* Prepara o Manufacturer Data */
        syscare_build_mfg_data();

        /* Mostra os 14 bytes do payload */
        syscare_print_payload();

        /*
         * Atualiza os dados do advertising BLE.
         * A transmissao permanece ativa.
         */

        printk("Atualizando advertising BLE...\n");

        err = bt_le_adv_update_data(
            ad,
            ARRAY_SIZE(ad),
            NULL,
            0
        );

        if (err) {
            printk("Falha ao atualizar advertising: %d\n", err);
            continue;
        }

        printk("Advertising atualizado com sucesso!\n");
        printk("Transmissao continua ativa.\n");
    }

    return 0;
}



// testes feitos:

//  * Payload SysCare: 14 bytes
//  *
//  * [0]       Versao
//  * [1..4]    ID da pulseira
//  * [5]       Evento:
//  *             0x00 teste
//  *             0x01 queda
//  *             0x02 botao de panico
//  *             0x03 imobilidade
//  *             0x04 bateria baixa
//  *             0x05 heartbeat
//  * [6..7]    Sequencia (little-endian)
//  * [8]       Bateria
//  * [9]       Impacto
//  * [10..11]  ID de queda (16 bits, little-endian)
//  * [12..13]  Reservados para uso futuro


// --> 05 no quinto espaço indica a queda
// Heartbeat normal.
// Sequencia: 9
// Payload SysCare: 01 01 02 03 04 05 09 00 5B 00 01 00 00 00
// Atualizando advertising BLE...
// Advertising atualizado com sucesso!
// Transmissao continua ativa.

// Aguardando proximo evento...

// --> 01 no quinto espaço indica a queda
// =================================
// QUEDA SIMULADA!
// Sequencia: 12
// ID da queda: 2
// =================================
// Payload SysCare: 01 01 02 03 04 01 0C 00 58 96 02 00 00 00
// Atualizando advertising BLE...
// Advertising atualizado com sucesso!
// Transmissao continua ativa.

// Aguardando proximo evento...
