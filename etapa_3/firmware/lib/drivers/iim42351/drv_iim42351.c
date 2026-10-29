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

static acc_event_cb_t event_cb;
static bool initialized;

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
 * API
 * ========================================================= */

int drv_iim42351_init(acc_event_cb_t cb)
{
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

    event_cb = cb;
    initialized = true;

    return 0;
}
