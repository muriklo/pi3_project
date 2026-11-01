/*
 * SysCare - driver do acelerometro IIM-42351
 *
 * O driver le a propria configuracao do devicetree (no com compatible
 * "invensense,iim42351") e confere em tempo de compilacao as premissas
 * de hardware.
 *
 * Divisao dos arquivos (inspirada no exemplo icm42605):
 *   - drv_iim42351.c       configuracao do DT, estado e API publica;
 *   - drv_iim42351_bus.c   acesso a registradores (unico que usa a HAL SPI);
 *   - drv_iim42351_setup.c sequencias de registradores do datasheet.
 */

#define DT_DRV_COMPAT invensense_iim42351

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include "drv_iim42351.h"
#include "drv_iim42351_bus.h"
#include "drv_iim42351_reg.h"
#include "drv_iim42351_setup.h"
#include "drv_iim42351_trigger.h"
#include "hal_spi.h"

LOG_MODULE_REGISTER(iim42351, CONFIG_SYSCARE_IIM42351_LOG_LEVEL);

/* =========================================================
 * Premissas de hardware conferidas no build
 * ========================================================= */

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
             "Esperado exatamente um IIM-42351 com status okay no devicetree");

/*
 * Se o sensor e alimentado por reguladores, eles precisam iniciar ANTES
 * do driver SPI: no init, o SPI coloca o CS em nivel alto, o que
 * alimentaria o sensor desligado pelo pino.
 */
#if DT_INST_NODE_HAS_PROP(0, vdd_supply) || DT_INST_NODE_HAS_PROP(0, vddio_supply)

BUILD_ASSERT(IS_ENABLED(CONFIG_REGULATOR_FIXED),
             "vdd-supply/vddio-supply exigem CONFIG_REGULATOR=y (regulator-fixed)");

#if defined(CONFIG_REGULATOR_FIXED)
BUILD_ASSERT(CONFIG_REGULATOR_FIXED_INIT_PRIORITY < CONFIG_SPI_INIT_PRIORITY,
             "Os reguladores do sensor precisam iniciar antes do SPI: "
             "CONFIG_REGULATOR_FIXED_INIT_PRIORITY < CONFIG_SPI_INIT_PRIORITY");
#endif

#endif

/* =========================================================
 * Configuracao (devicetree) e estado
 * ========================================================= */

/* Regulador de uma propriedade *-supply, ou NULL (alimentacao fixa) */
#define SUPPLY_OR_NULL(prop)                                            \
    COND_CODE_1(DT_INST_NODE_HAS_PROP(0, prop),                         \
                (DEVICE_DT_GET(DT_INST_PHANDLE(0, prop))), (NULL))

struct iim42351_config {
    hal_spi_dev_t spi;
    const struct device *vdd;
    const struct device *vddio;
};

static const struct iim42351_config cfg = {
    .spi = HAL_SPI_DEV_DT(DT_DRV_INST(0)),
    .vdd = SUPPLY_OR_NULL(vdd_supply),
    .vddio = SUPPLY_OR_NULL(vddio_supply),
};

/* Configuracao inicial (accel-mode: indice 0 = "low-noise", 1 = "low-power") */
#define DT_ACC_MODE                                                     \
    ((DT_INST_ENUM_IDX(0, accel_mode) == 0) ? ACC_MODE_LOW_NOISE : ACC_MODE_LOW_POWER)

static bool initialized;
static uint8_t cur_fs_sel;
static acc_mode_t cur_mode = ACC_MODE_OFF;

/*
 * FIFO: pacotes que cabem nela (um fica reservado para a escrita, 6.3),
 * maior watermark aceito e quantos pacotes o driver le por transacao SPI
 */
#define FIFO_PACKETS        (IIM42351_FIFO_SIZE / IIM42351_FIFO_PACKET1_SIZE)
#define FIFO_WATERMARK_MAX  (FIFO_PACKETS - 2)
#define FIFO_CHUNK_PACKETS  32

static bool fifo_on;
static uint16_t fifo_int_reg;       /* INT_SOURCE que rotea o watermark (0 = nenhum) */
static uint8_t fifo_int_bit;
static uint32_t fifo_discarded;     /* pacotes invalidos descartados desde o start */

/* Queda livre: ligada, e o INT_SOURCE que rotea o evento */
static bool ff_on;
static uint16_t ff_int_reg;
static uint8_t ff_int_bit;

/* Alturas (cm) de cada codigo de FF_MIN/MAX_DURATION_CM (18.11) */
static const uint16_t ff_min_cm[] = { IIM42351_FF_MIN_DURATION_CM_LIST };
static const uint16_t ff_max_cm[] = { IIM42351_FF_MAX_DURATION_CM_LIST };

/* =========================================================
 * Traducao dos valores genericos para os codigos do IIM-42351
 * ========================================================= */

/* Fundos de escala suportados, do menor para o maior (14.31) */
static const struct {
    uint8_t range_g;
    uint8_t fs_sel;
} fs_table[] = {
    { 2,  IIM42351_ACCEL_FS_SEL_2G },
    { 4,  IIM42351_ACCEL_FS_SEL_4G },
    { 8,  IIM42351_ACCEL_FS_SEL_8G },
    { 16, IIM42351_ACCEL_FS_SEL_16G },
};

/* ODRs suportados, do menor para o maior, e os modos em que valem (14.31) */
#define ODR_LP  BIT(0)
#define ODR_LN  BIT(1)

static const struct {
    uint16_t hz;            /* truncado: 1 = 1,5625 Hz, 12 = 12,5 Hz */
    uint8_t code;
    uint8_t modes;
} odr_table[] = {
    { 1,    IIM42351_ACCEL_ODR_1_5625HZ, ODR_LP },
    { 3,    IIM42351_ACCEL_ODR_3_125HZ,  ODR_LP },
    { 6,    IIM42351_ACCEL_ODR_6_25HZ,   ODR_LP },
    { 12,   IIM42351_ACCEL_ODR_12_5HZ,   ODR_LP | ODR_LN },
    { 25,   IIM42351_ACCEL_ODR_25HZ,     ODR_LP | ODR_LN },
    { 50,   IIM42351_ACCEL_ODR_50HZ,     ODR_LP | ODR_LN },
    { 100,  IIM42351_ACCEL_ODR_100HZ,    ODR_LP | ODR_LN },
    { 200,  IIM42351_ACCEL_ODR_200HZ,    ODR_LP | ODR_LN },
    { 500,  IIM42351_ACCEL_ODR_500HZ,    ODR_LP | ODR_LN },
    { 1000, IIM42351_ACCEL_ODR_1000HZ,   ODR_LN },
    { 2000, IIM42351_ACCEL_ODR_2000HZ,   ODR_LN },
    { 4000, IIM42351_ACCEL_ODR_4000HZ,   ODR_LN },
    { 8000, IIM42351_ACCEL_ODR_8000HZ,   ODR_LN },
};

/* Menor fundo de escala >= o pedido (ou o maior) */
static size_t pick_fs(uint8_t range_g)
{
    for (size_t i = 0; i < ARRAY_SIZE(fs_table); i++) {
        if (fs_table[i].range_g >= range_g) {
            return i;
        }
    }

    return ARRAY_SIZE(fs_table) - 1;
}

/* Menor ODR >= o pedido entre os validos no modo (ou o maior do modo) */
static size_t pick_odr(uint16_t hz, acc_mode_t mode)
{
    /* Desligado: usa a tabela do LN, so para deixar um valor valido */
    uint8_t need = (mode == ACC_MODE_LOW_POWER) ? ODR_LP : ODR_LN;
    size_t last = 0;

    for (size_t i = 0; i < ARRAY_SIZE(odr_table); i++) {
        if (!(odr_table[i].modes & need)) {
            continue;
        }
        last = i;
        if (odr_table[i].hz >= hz) {
            return i;
        }
    }

    return last;
}

static uint8_t accel_mode_code(acc_mode_t mode)
{
    switch (mode) {
    case ACC_MODE_LOW_POWER:
        return IIM42351_ACCEL_MODE_LP;
    case ACC_MODE_LOW_NOISE:
        return IIM42351_ACCEL_MODE_LN;
    default:
        return IIM42351_ACCEL_MODE_OFF;
    }
}

static const char *mode_name(acc_mode_t mode)
{
    switch (mode) {
    case ACC_MODE_LOW_POWER:
        return "low-power";
    case ACC_MODE_LOW_NOISE:
        return "low-noise";
    default:
        return "off";
    }
}

/* Converte a leitura crua para mg conforme o fundo de escala atual */
static int16_t raw_to_mg(int16_t raw)
{
    int32_t sens = IIM42351_SENSITIVITY_LSB_PER_G(cur_fs_sel);

    return (int16_t)(((int32_t)raw * 1000) / sens);
}

/* =========================================================
 * Alimentacao
 * ========================================================= */

static int supply_enable(const struct device *reg, const char *name)
{
    if (reg == NULL) {
        return 0;
    }

#if defined(CONFIG_REGULATOR)
    int err;

    if (!device_is_ready(reg)) {
        LOG_ERR("Regulador %s nao pronto", name);
        return -ENODEV;
    }

    /* Ja ligado no boot (regulator-boot-on): so registra o uso */
    err = regulator_enable(reg);
    if (err) {
        LOG_ERR("Falha ao ligar %s (%d)", name, err);
    }

    return err;
#else
    ARG_UNUSED(name);
    return -ENOTSUP;
#endif
}

static int power_on(void)
{
    int err;

    err = supply_enable(cfg.vdd, "VDD");
    if (err) {
        return err;
    }

    err = supply_enable(cfg.vddio, "VDDIO");
    if (err) {
        return err;
    }

    /*
     * Com regulator-boot-on o regulador nao aplica o startup-delay-us:
     * garante aqui o tempo ate o sensor aceitar registradores (3.2).
     */
    k_msleep(IIM42351_POWER_UP_MS);

    return 0;
}

/* =========================================================
 * Configuracao
 * ========================================================= */

static int apply_config(acc_config_t *cfg)
{
    size_t fs;
    size_t odr;
    int err;

    if (cfg == NULL || cfg->mode > ACC_MODE_LOW_NOISE) {
        return -EINVAL;
    }

    fs = pick_fs(cfg->range_g);
    odr = pick_odr(cfg->odr_hz, cfg->mode);

    err = iim42351_setup_accel(fs_table[fs].fs_sel, odr_table[odr].code,
                               accel_mode_code(cfg->mode));
    if (err) {
        return err;
    }

    cur_fs_sel = fs_table[fs].fs_sel;
    cur_mode = cfg->mode;

    cfg->range_g = fs_table[fs].range_g;
    cfg->odr_hz = odr_table[odr].hz;

    return 0;
}

/* =========================================================
 * API
 * ========================================================= */

int drv_iim42351_init(acc_event_cb_t cb)
{
    acc_config_t initial = {
        .range_g = DT_INST_PROP(0, accel_fs),
        .odr_hz = DT_INST_PROP(0, accel_hz),
        .mode = DT_ACC_MODE,
    };
    int err;

    if (initialized) {
        return -EALREADY;
    }

    err = power_on();
    if (err) {
        return err;
    }

    if (!hal_spi_is_ready(&cfg.spi)) {
        LOG_ERR("Barramento SPI nao pronto");
        return -ENODEV;
    }

    iim42351_bus_init(&cfg.spi);

    err = iim42351_setup_init();
    if (err) {
        return err;
    }

    /* Antes de ligar o acelerometro: INT_CONFIG so muda com ele desligado */
    err = iim42351_trigger_init(cb);
    if (err) {
        return err;
    }

    err = apply_config(&initial);
    if (err) {
        LOG_ERR("Falha ao aplicar a config inicial (%d)", err);
        return err;
    }

    LOG_INF("Config inicial (devicetree): +-%u g, %u Hz, %s",
            initial.range_g, initial.odr_hz, mode_name(initial.mode));

    initialized = true;

    return 0;
}

int drv_iim42351_config(acc_config_t *cfg)
{
    if (!initialized) {
        return -ENODEV;
    }

    /* A queda livre exige a configuracao fixa da secao 8.5 */
    if (ff_on) {
        return -EBUSY;
    }

    return apply_config(cfg);
}

int drv_iim42351_read(acc_sample_t *sample)
{
    uint8_t buf[6];
    int16_t raw[3];
    int err;

    if (!initialized) {
        return -ENODEV;
    }

    if (sample == NULL) {
        return -EINVAL;
    }

    if (cur_mode == ACC_MODE_OFF) {
        return -ENODATA;
    }

    /* Burst de ACCEL_DATA_X1 a ACCEL_DATA_Z0, big-endian (14.7 a 14.12) */
    err = iim42351_reg_read(IIM42351_REG_ACCEL_DATA_X1, buf, sizeof(buf));
    if (err) {
        return err;
    }

    for (size_t i = 0; i < ARRAY_SIZE(raw); i++) {
        raw[i] = (int16_t)sys_get_be16(&buf[2 * i]);
        if (raw[i] == IIM42351_INVALID_SAMPLE) {
            return -EAGAIN;
        }
    }

    sample->x_mg = raw_to_mg(raw[0]);
    sample->y_mg = raw_to_mg(raw[1]);
    sample->z_mg = raw_to_mg(raw[2]);

    return 0;
}

/* =========================================================
 * FIFO
 * ========================================================= */

int drv_iim42351_buffer_start(uint16_t watermark)
{
    int err;

    if (!initialized) {
        return -ENODEV;
    }

    if (fifo_on) {
        return -EALREADY;
    }

    /* Ligar a FIFO desliga o acelerometro por um instante e interromperia o DMP */
    if (ff_on) {
        return -EBUSY;
    }

    if (watermark > FIFO_WATERMARK_MAX) {
        return -EINVAL;
    }

    fifo_int_reg = 0;
    fifo_int_bit = 0;

    if (watermark > 0 && iim42351_trigger_fifo_route(&fifo_int_reg, &fifo_int_bit) != 0) {
        LOG_WRN("Sem pino de interrupcao no devicetree: ler a FIFO por polling");
    }

    err = iim42351_setup_fifo_start(watermark, fifo_int_reg, fifo_int_bit);
    if (err) {
        return err;
    }

    fifo_discarded = 0;
    fifo_on = true;

    return 0;
}

/* Le o FIFO_COUNTL antes do H: e a leitura do L que trava os 16 bits (14.16, 14.17) */
static int fifo_count(uint16_t *packets)
{
    uint8_t high;
    uint8_t low;
    int err;

    err = iim42351_reg_read(IIM42351_REG_FIFO_COUNTL, &low, 1);
    if (err) {
        return err;
    }

    err = iim42351_reg_read(IIM42351_REG_FIFO_COUNTH, &high, 1);
    if (err) {
        return err;
    }

    *packets = ((uint16_t)high << 8) | low;

    return 0;
}

/*
 * Pacote 1 (6.1): header, X, Y, Z (big-endian) e temperatura.
 * Retorna 1 com amostra valida, 0 para descartar, -ENODATA se a FIFO
 * acabou (header com HEADER_MSG).
 */
static int fifo_parse(const uint8_t *pkt, acc_sample_t *s)
{
    int16_t raw[3];

    if (pkt[0] & IIM42351_FIFO_HEADER_MSG) {
        return -ENODATA;
    }

    if (FIELD_GET(IIM42351_FIFO_HEADER_PACKET_FORMAT, pkt[0]) != IIM42351_FIFO_HEADER_FORMAT_P1) {
        return 0;
    }

    for (size_t i = 0; i < ARRAY_SIZE(raw); i++) {
        raw[i] = (int16_t)sys_get_be16(&pkt[1 + 2 * i]);
        if (raw[i] == IIM42351_INVALID_SAMPLE) {
            return 0;
        }
    }

    s->x_mg = raw_to_mg(raw[0]);
    s->y_mg = raw_to_mg(raw[1]);
    s->z_mg = raw_to_mg(raw[2]);

    return 1;
}

int drv_iim42351_buffer_read(acc_sample_t *buf, size_t max, size_t *count)
{
    uint8_t raw[FIFO_CHUNK_PACKETS * IIM42351_FIFO_PACKET1_SIZE];
    uint16_t available;
    size_t to_read;
    size_t n = 0;
    int err;

    if (!initialized || !fifo_on) {
        return -ENODEV;
    }

    if (buf == NULL || count == NULL) {
        return -EINVAL;
    }

    *count = 0;

    err = fifo_count(&available);
    if (err) {
        return err;
    }

    /* Nunca le mais pacotes do que cabem no buffer de quem chama */
    to_read = MIN((size_t)available, max);

    while (to_read > 0) {
        size_t chunk = MIN(to_read, (size_t)FIFO_CHUNK_PACKETS);

        err = iim42351_reg_read(IIM42351_REG_FIFO_DATA, raw,
                                chunk * IIM42351_FIFO_PACKET1_SIZE);
        if (err) {
            break;
        }

        for (size_t i = 0; i < chunk; i++) {
            int ret = fifo_parse(&raw[i * IIM42351_FIFO_PACKET1_SIZE], &buf[n]);

            if (ret == -ENODATA) {
                to_read = chunk;    /* FIFO vazia: encerra depois deste bloco */
                break;
            }

            if (ret == 1) {
                n++;
            } else {
                fifo_discarded++;
            }
        }

        to_read -= chunk;
    }

    *count = n;

    return err;
}

int drv_iim42351_buffer_stop(void)
{
    int err;

    if (!initialized || !fifo_on) {
        return -ENODEV;
    }

    err = iim42351_setup_fifo_stop(fifo_int_reg, fifo_int_bit);
    if (err) {
        return err;
    }

    fifo_on = false;
    LOG_INF("FIFO parada: %u pacotes invalidos descartados", (unsigned int)fifo_discarded);

    return 0;
}

/* =========================================================
 * Queda livre (APEX)
 * ========================================================= */

/* Indice do valor da tabela mais proximo (empate: o menor) */
static uint8_t pick_nearest(const uint16_t *table, size_t n, uint16_t value)
{
    uint8_t best = 0;

    for (size_t i = 1; i < n; i++) {
        if (abs((int)table[i] - (int)value) < abs((int)table[best] - (int)value)) {
            best = (uint8_t)i;
        }
    }

    return best;
}

/*
 * Pedido generico -> codigos do IIM-42351, e devolve em cfg o que foi
 * aplicado. Limiares: low-g (n + 1) * 31,25 mg e high-g (n + 1) * 250 mg,
 * n de 0 a 31 (18.6, 18.7); debounce de 0 a 3 s (18.11).
 */
static void ff_translate(acc_freefall_config_t *cfg, struct iim42351_ff_codes *codes)
{
    uint32_t lowg = DIV_ROUND_CLOSEST((uint32_t)cfg->lowg_mg * 1000U,
                                      IIM42351_LOWG_PEAK_TH_STEP_UG);
    uint32_t highg = DIV_ROUND_CLOSEST((uint32_t)cfg->highg_mg, IIM42351_HIGHG_PEAK_TH_STEP_MG);
    uint32_t debounce = DIV_ROUND_CLOSEST((uint32_t)cfg->debounce_ms, 1000U);

    codes->lowg_peak = (uint8_t)(CLAMP(lowg, 1U, 32U) - 1U);
    codes->highg_peak = (uint8_t)(CLAMP(highg, 1U, 32U) - 1U);
    codes->min_cm = pick_nearest(ff_min_cm, ARRAY_SIZE(ff_min_cm), cfg->min_height_cm);
    codes->max_cm = pick_nearest(ff_max_cm, ARRAY_SIZE(ff_max_cm), cfg->max_height_cm);
    codes->debounce = (uint8_t)MIN(debounce, 3U);

    cfg->lowg_mg = (uint16_t)DIV_ROUND_CLOSEST((codes->lowg_peak + 1U) *
                                               IIM42351_LOWG_PEAK_TH_STEP_UG, 1000U);
    cfg->highg_mg = (uint16_t)((codes->highg_peak + 1U) * IIM42351_HIGHG_PEAK_TH_STEP_MG);
    cfg->min_height_cm = ff_min_cm[codes->min_cm];
    cfg->max_height_cm = ff_max_cm[codes->max_cm];
    cfg->debounce_ms = (uint16_t)(codes->debounce * 1000U);
}

int drv_iim42351_freefall_enable(acc_freefall_config_t *cfg)
{
    struct iim42351_ff_codes codes;
    int err;

    if (!initialized) {
        return -ENODEV;
    }

    if (cfg == NULL) {
        return -EINVAL;
    }

    if (ff_on) {
        return -EALREADY;
    }

    /* A FIFO desligaria o acelerometro por um instante e interromperia o DMP */
    if (fifo_on) {
        return -EBUSY;
    }

    /* O evento so chega por um pino de interrupcao */
    if (iim42351_trigger_freefall_route(&ff_int_reg, &ff_int_bit) != 0) {
        LOG_ERR("Queda livre exige int1-gpios ou int2-gpios no devicetree");
        return -ENOTSUP;
    }

    ff_translate(cfg, &codes);

    err = iim42351_setup_freefall_enable(&codes, ff_int_reg, ff_int_bit);
    if (err) {
        return err;
    }

    /* A sequencia da 8.5 deixa o acelerometro em LP, 500 Hz, +-16 g */
    cur_fs_sel = IIM42351_ACCEL_FS_SEL_16G;
    cur_mode = ACC_MODE_LOW_POWER;
    ff_on = true;

    LOG_INF("Queda livre: low-g %u mg, high-g %u mg, %u a %u cm, debounce %u ms",
            cfg->lowg_mg, cfg->highg_mg, cfg->min_height_cm, cfg->max_height_cm,
            cfg->debounce_ms);

    return 0;
}

int drv_iim42351_freefall_disable(void)
{
    int err;

    if (!initialized || !ff_on) {
        return -ENODEV;
    }

    err = iim42351_setup_freefall_disable(ff_int_reg, ff_int_bit);
    if (err) {
        return err;
    }

    ff_on = false;

    return 0;
}
