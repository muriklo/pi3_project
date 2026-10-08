/*
 * SysCare - BLE Advertising (teste do heartbeat)
 * XIAO nRF52840 / Zephyr
 *
 * Contrato do pacote: etapa_2/software/api/docs/ble_payload.md
 */

// XIAO nRF52840
//       │
//       ├── BLE
//       │
//       └── Advertisement
//               │
//               ├── Company ID = 0xFFFF
//               │
//               └── 14 bytes
//                     │
//                     ├── versão = 0x01
//                     ├── device_ble_id
//                     ├── evento = 0x05
//                     ├── seq
//                     ├── bateria
//                     ├── impacto = 0
//                     └── HMAC-SHA256[0..3]


#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#include <mbedtls/md.h>

#include <errno.h>
#include <string.h>


/* =========================================================
 * SysCare
 * ========================================================= */

#define SYSCARE_COMPANY_ID    0xFFFF
#define SYSCARE_PROTO_VER     0x01
#define SYSCARE_EVT_HEARTBEAT 0x05

/* =========================================================
 * Dados do dispositivo
 * ========================================================= */

static const uint8_t device_ble_id[4] = {
    0x01,
    0x02,
    0x03,
    0x04
};


/*
 * Temporário para teste.
 *
 * Com esta chave o heartbeat funciona (a API não confere a assinatura da
 * telemetria), mas uma queda seria recusada. Para testar alertas, cadastre a
 * pulseira no app e grave aqui a chave que ele mostra (32 dígitos hex =
 * 16 bytes, na mesma ordem).
 */
static const uint8_t shared_secret[16] = {
    0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F
};


/* =========================================================
 * Payload
 * ========================================================= */

// byte       conteúdo
// --------------------------------
// 0          versão
// 1..4       device_ble_id
// 5          evento
// 6..7       seq
// 8          bateria
// 9          impacto
// 10..13     HMAC


// ┌────┬───────────┬──────┬───────┬─────────┬─────────┐
// │ 01 │ ID (4 B)  │ EVT  │ SEQ   │ BAT     │ HMAC    │
// │    │           │      │ (2 B) │ IMPACT  │ (4 B)   │
// └────┴───────────┴──────┴───────┴─────────┴─────────┘
//   1       4          1      2        2         4
//                  = 14 bytes

static uint8_t adv_payload[14];


/*
 * Manufacturer Data:
 *
 * 2 bytes = Company ID
 * 14 bytes = adv_payload
 */
static uint8_t adv_mfg_buf[16];

// ┌──────────────┬──────────────────────────────┐
// │ Company ID   │       adv_payload            │
// │    2 bytes   │          14 bytes            │
// └──────────────┴──────────────────────────────┘

/*
 * bytes 0..9 = dados, 10..13 = HMAC truncado.
 *
 * Retorna 0 em caso de sucesso. Se o HMAC falhar, o pacote não pode ir ao ar:
 * a assinatura seria lixo e a API recusaria o alerta.
 */
static int syscare_build_payload(uint8_t event, uint16_t seq, uint8_t batt, uint8_t impact_dg)
{
    uint8_t mac[32];
    int err;

    const mbedtls_md_info_t *md_info;

    adv_payload[0] = SYSCARE_PROTO_VER;

    memcpy(&adv_payload[1], device_ble_id, 4);

    adv_payload[5] = event;

    adv_payload[6] = seq & 0xFF;
    adv_payload[7] = seq >> 8;

    adv_payload[8] = batt;

    adv_payload[9] = impact_dg;


    /*
     * HMAC-SHA256 sobre os bytes 0..9
     */
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    if (md_info == NULL) {
        printk("Erro: SHA256 indisponivel\n");
        return -ENOTSUP;
    }

    err = mbedtls_md_hmac(
        md_info,
        shared_secret,
        sizeof(shared_secret),
        adv_payload,
        10,
        mac
    );

    if (err) {
        printk("Erro: HMAC falhou (%d)\n", err);
        return -EIO;
    }


    /*
     * Apenas os primeiros 4 bytes do HMAC
     */
    memcpy(&adv_payload[10], mac, 4);

    return 0;
}


/*
 * Mostra os 14 bytes no console, para comparar com o que o celular recebe.
 *
 * Com o ID e a chave de teste acima, o heartbeat inicial deve ser:
 * 01 01 02 03 04 05 00 00 64 00 2F CC B0 1D
 */
static void syscare_print_payload(void)
{
    printk("Payload:");

    for (size_t i = 0; i < sizeof(adv_payload); i++) {
        printk(" %02X", adv_payload[i]);
    }

    printk("\n");
}


/* =========================================================
 * Manufacturer Data
 * ========================================================= */

static void syscare_build_mfg_data(void)
{
    /*
     * Company ID 0xFFFF
     *
     * Little-endian:
     * FF FF
     */

    adv_mfg_buf[0] = SYSCARE_COMPANY_ID & 0xFF;
    adv_mfg_buf[1] = SYSCARE_COMPANY_ID >> 8;


    /*
     * Payload de 14 bytes
     */
    memcpy(
        &adv_mfg_buf[2],
        adv_payload,
        sizeof(adv_payload)
    );
}


/* =========================================================
 * Advertising data
 * ========================================================= */

static const struct bt_data ad[] = {
    BT_DATA_BYTES(
        BT_DATA_FLAGS,
        BT_LE_AD_NO_BREDR
    ),

    BT_DATA(
        BT_DATA_MANUFACTURER_DATA,
        adv_mfg_buf,
        sizeof(adv_mfg_buf)
    ),
};


/* =========================================================
 * Advertising parameters
 * ========================================================= */

/*
 * repouso: não-conectável, 2000 ms — 0x0C80 = 3200 * 0,625 ms
 *
 * Não usar BT_GAP_ADV_SLOW_INT_MIN/MAX: eles valem 1,0–1,2 s.
 */
#define ADV_IDLE \
    BT_LE_ADV_PARAM( \
        BT_LE_ADV_OPT_USE_IDENTITY, \
        0x0C80, \
        0x0CD0, \
        NULL)


/* emergência: 100 ms — 0x00A0 = 160 * 0,625 ms */
#define ADV_ALERT \
    BT_LE_ADV_PARAM( \
        BT_LE_ADV_OPT_USE_IDENTITY, \
        0x00A0, \
        0x00B0, \
        NULL)


/* =========================================================
 * Main
 * ========================================================= */

int main(void)
{
    int err;

    /*
     * Seq atual.
     *
     * O heartbeat repete o seq atual.
     */
    uint16_t seq = 0;

    /*
     * Bateria em %. Fixa no teste; depois vem da leitura do ADC.
     */
    uint8_t batt = 100;


    printk("SysCare BLE\n");


    /* -----------------------------------------------------
     * Inicializa Bluetooth
     * ----------------------------------------------------- */

    err = bt_enable(NULL);

    if (err) {
        printk("Bluetooth init failed: %d\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");


    /* -----------------------------------------------------
     * Monta primeiro heartbeat
     * ----------------------------------------------------- */

    err = syscare_build_payload(
        SYSCARE_EVT_HEARTBEAT,
        seq,
        batt,
        0
    );

    if (err) {
        return 0;
    }

    syscare_build_mfg_data();

    syscare_print_payload();


    /* -----------------------------------------------------
     * Inicia advertising em repouso
     * ----------------------------------------------------- */

    err = bt_le_adv_start(
        ADV_IDLE,
        ad,
        ARRAY_SIZE(ad),
        NULL,
        0
    );

    if (err) {
        printk("Advertising failed: %d\n", err);
        return 0;
    }


    printk("SysCare advertising started\n");


    /* -----------------------------------------------------
     * Heartbeat
     * ----------------------------------------------------- */

    while (1) {

        k_sleep(K_SECONDS(2));


        /*
         * Heartbeat:
         *
         * evento = 0x05
         * seq    = seq atual
         * batt   = leitura atual
         * impact = 0
         */

        err = syscare_build_payload(
            SYSCARE_EVT_HEARTBEAT,
            seq,
            batt,
            0
        );

        if (err) {
            /* Mantém no ar o último pacote válido. */
            continue;
        }

        syscare_build_mfg_data();


        /*
         * O bt_le_adv_start() copiou os dados para o controlador. Mudar o
         * adv_mfg_buf não muda o que vai ao ar: é preciso reenviá-lo.
         */
        err = bt_le_adv_update_data(
            ad,
            ARRAY_SIZE(ad),
            NULL,
            0
        );

        if (err) {
            printk("Advertising update failed: %d\n", err);
        }
    }


    return 0;
}
