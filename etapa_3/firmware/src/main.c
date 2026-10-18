
/*
 * SysCare - BLE Advertising por evento simulado
 * XIAO nRF52840 / Zephyr / NCS 3.4.0
 *
 * A cada 10 segundos:
 *   1. Simula um evento do acelerometro
 *   2. Monta o payload SysCare
 *   3. Atualiza os dados do advertising BLE
 *   4. Continua transmitindo sem parar o advertising
 *
 * O advertising e iniciado uma unica vez.
 *
 * HMAC temporariamente desativado.
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

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
 * Parametros do teste
 * ========================================================= */

#define SIMULATED_EVENT_PERIOD   K_SECONDS(10)

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
 * [10..11]  ID de queda (16 bits, little-endian)
 * [12..13]  Reservados para uso futuro
 *
 * O servidor deve utilizar o ID da pulseira junto com
 * o ID da queda para identificar retransmissoes do
 * mesmo evento e evitar disparos duplicados de SMS.
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
    uint8_t impact_dg,
    uint16_t fall_id)
{
    memset(adv_payload, 0, sizeof(adv_payload));

    /* Byte 0: versao do protocolo */
    adv_payload[0] = SYSCARE_PROTO_VER;

    /* Bytes 1 a 4: identificador da pulseira */
    memcpy(&adv_payload[1],
           device_ble_id,
           sizeof(device_ble_id));

    /* Byte 5: tipo do evento */
    adv_payload[5] = event;

    /* Bytes 6 e 7: sequencia little-endian */
    adv_payload[6] = (uint8_t)(seq & 0xFF);
    adv_payload[7] = (uint8_t)((seq >> 8) & 0xFF);

    /* Byte 8: bateria */
    adv_payload[8] = batt;

    /* Byte 9: impacto */
    adv_payload[9] = impact_dg;

    /*
     * Bytes 10 e 11: ID da queda (16 bits).
     *
     * O ID deve ser incrementado somente quando uma
     * nova queda for detectada.
     *
     * Retransmissoes referentes a mesma queda devem
     * utilizar o mesmo ID.
     */

    adv_payload[10] = (uint8_t)(fall_id & 0xFF);
    adv_payload[11] = (uint8_t)((fall_id >> 8) & 0xFF);

    /*
     * Bytes 12 e 13: reservados para uso futuro.
     */

    adv_payload[12] = 0x00;
    adv_payload[13] = 0x00;

    return 0;
}

/* =========================================================
 * Impressao do payload (so para imprimir na tela)
 * ========================================================= */

static void syscare_print_payload(void)
{
    printk("Payload SysCare:");

    for (size_t i = 0; i < sizeof(adv_payload); i++) {
        printk(" %02X", adv_payload[i]);
    }

    printk("\n");
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
 * Main
 * ========================================================= */

int main(void)
{
    int err;

    uint16_t seq = 0;
    uint8_t batt = 100;
    uint8_t impact_dg = 0;

    /*
     * ID da queda:
     * Mantem o mesmo valor enquanto nao houver uma nova
     * queda detectada.
     *
     * Neste teste, fall_id e incrementado a cada evento
     * FALL simulado.
     */

    uint16_t fall_id = 0;

    printk("\n");
    printk("=================================\n");
    printk("       SysCare BLE Teste\n");
    printk("=================================\n");

    /* Inicializa o Bluetooth uma unica vez */
    err = bt_enable(NULL);

    if (err) {
        printk("Falha ao inicializar Bluetooth: %d\n", err);
        return 0;
    }

    printk("Bluetooth inicializado!\n");
    printk("Advertising por evento simulado.\n");
    printk("Periodo entre eventos: 10 segundos.\n");
    printk("Teste: 5 heartbeats + 1 queda.\n");
    printk("Advertising permanecera ativo continuamente.\n");

    /*
     * Monta o payload inicial antes de iniciar o advertising,
     * garantindo que os dados estejam preparados.
     */

    err = syscare_build_payload(
        SYSCARE_EVT_HEARTBEAT,
        seq,
        batt,
        impact_dg,
        fall_id
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

    while (1) {

        /*
         * Aguarda 10 segundos.
         * Simula a chegada de um novo evento do sistema.
         */

        printk("\nAguardando proximo evento...\n");
        k_sleep(SIMULATED_EVENT_PERIOD);

        /* Incrementa a sequencia de mensagens */
        seq++;

        /*
         * A cada 6 mensagens:
         *   - 5 heartbeats normais
         *   - 1 evento de queda simulada
         */

        uint8_t event;

        if ((seq % 6) == 0) {

            /* Simula a deteccao de uma nova queda */
            event = SYSCARE_EVT_FALL;

            /* Novo ID para esta queda */
            fall_id++;

            /* Simula um valor de impacto */
            impact_dg = 150;

            printk("\n");
            printk("=================================\n");
            printk("QUEDA SIMULADA!\n");
            printk("Sequencia: %u\n", (unsigned int)seq);
            printk("ID da queda: %u\n", (unsigned int)fall_id);
            printk("=================================\n");

        } else {

            /* Evento normal: heartbeat */
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
            impact_dg,
            fall_id
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
