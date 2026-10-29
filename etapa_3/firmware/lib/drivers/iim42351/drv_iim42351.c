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

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "drv_iim42351.h"
#include "drv_iim42351_bus.h"
#include "drv_iim42351_reg.h"
#include "drv_iim42351_setup.h"
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

static acc_event_cb_t event_cb;
static bool initialized;
static uint8_t cur_fs_sel;
static acc_mode_t cur_mode = ACC_MODE_OFF;

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

    err = apply_config(&initial);
    if (err) {
        LOG_ERR("Falha ao aplicar a config inicial (%d)", err);
        return err;
    }

    LOG_INF("Config inicial (devicetree): +-%u g, %u Hz, %s",
            initial.range_g, initial.odr_hz, mode_name(initial.mode));

    event_cb = cb;
    initialized = true;

    return 0;
}

int drv_iim42351_config(acc_config_t *cfg)
{
    if (!initialized) {
        return -ENODEV;
    }

    return apply_config(cfg);
}
