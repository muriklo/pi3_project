/*
 * SysCare - sequencias de configuracao do IIM-42351
 *
 * Init na ordem do icm42605_sensor_init() do exemplo, corrigida para o
 * IIM-42351. A identificacao vem antes do reset: se o SPI ou o sensor
 * nao responderem, o erro ja aparece com o valor lido.
 */

#include <errno.h>
#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "drv_iim42351_bus.h"
#include "drv_iim42351_reg.h"
#include "drv_iim42351_setup.h"

LOG_MODULE_DECLARE(iim42351, CONFIG_SYSCARE_IIM42351_LOG_LEVEL);

static int check_who_am_i(void)
{
    uint8_t id;
    int err;

    err = iim42351_reg_read(IIM42351_REG_WHO_AM_I, &id, 1);
    if (err) {
        LOG_ERR("Falha ao ler WHO_AM_I (%d)", err);
        return err;
    }

    if (id != IIM42351_WHO_AM_I_VALUE) {
        LOG_ERR("WHO_AM_I = 0x%02X, esperado 0x%02X", id, IIM42351_WHO_AM_I_VALUE);
        return -ENODEV;
    }

    LOG_INF("WHO_AM_I = 0x%02X", id);

    return 0;
}

/* Soft reset (14.1): espera 1 ms e confere o RESET_DONE_INT (14.15) */
static int soft_reset(void)
{
    uint8_t status;
    int err;

    err = iim42351_reg_write(IIM42351_REG_DEVICE_CONFIG, IIM42351_SOFT_RESET_CONFIG);
    if (err) {
        return err;
    }

    k_msleep(IIM42351_SOFT_RESET_MS);

    /* O reset devolve os registradores ao padrao, inclusive o banco */
    iim42351_bus_bank_reset();

    err = iim42351_reg_read(IIM42351_REG_INT_STATUS, &status, 1);
    if (err) {
        return err;
    }

    if (!(status & IIM42351_RESET_DONE_INT)) {
        LOG_ERR("Soft reset nao concluido (INT_STATUS = 0x%02X)", status);
        return -EIO;
    }

    LOG_DBG("Soft reset concluido");

    return 0;
}

int iim42351_setup_init(void)
{
    int err;

    err = check_who_am_i();
    if (err) {
        return err;
    }

    err = soft_reset();
    if (err) {
        return err;
    }

    /* So SPI: desliga a interface I2C (INTF_CONFIG0.UI_SIFS_CFG = 11, 14.28) */
    err = iim42351_reg_update(IIM42351_REG_INTF_CONFIG0, IIM42351_UI_SIFS_CFG,
                              FIELD_PREP(IIM42351_UI_SIFS_CFG,
                                         IIM42351_UI_SIFS_CFG_DISABLE_I2C));
    if (err) {
        return err;
    }

    /* INT_ASYNC_RESET = 0: exigido para os pinos INT1/INT2 (14.43) */
    err = iim42351_reg_update(IIM42351_REG_INT_CONFIG1, IIM42351_INT_ASYNC_RESET, 0);
    if (err) {
        return err;
    }

    /*
     * Diferente do exemplo icm42605: o INTF_CONFIG1 nao e escrito, porque
     * aqui ele nao tem os campos de AFSR/PLL (14.29).
     */

    LOG_INF("IIM-42351 inicializado");

    return 0;
}

int iim42351_setup_accel(uint8_t fs_sel, uint8_t odr_code, uint8_t accel_mode)
{
    uint8_t pwr;
    bool was_off;
    int err;

    err = iim42351_reg_read(IIM42351_REG_PWR_MGMT0, &pwr, 1);
    if (err) {
        return err;
    }

    /* ACCEL_MODE 0 e 1 desligam o acelerometro (14.30) */
    was_off = FIELD_GET(IIM42351_ACCEL_MODE, pwr) < IIM42351_ACCEL_MODE_LP;

    /* ACCEL_CONFIG0: o bit 4 e reservado, por isso a mascara */
    err = iim42351_reg_update(IIM42351_REG_ACCEL_CONFIG0,
                              IIM42351_ACCEL_FS_SEL | IIM42351_ACCEL_ODR,
                              FIELD_PREP(IIM42351_ACCEL_FS_SEL, fs_sel) |
                              FIELD_PREP(IIM42351_ACCEL_ODR, odr_code));
    if (err) {
        return err;
    }

    /*
     * Modo LP: o ACCEL_LP_CLK_SEL fica no padrao do reset (0, oscilador
     * wake-up), que e o usado nas sequencias do datasheet; o INTF_CONFIG1
     * nao pode ser escrito com o sensor ligado (13.6).
     */
    err = iim42351_reg_update(IIM42351_REG_PWR_MGMT0, IIM42351_ACCEL_MODE,
                              FIELD_PREP(IIM42351_ACCEL_MODE, accel_mode));
    if (err) {
        return err;
    }

    /* Saindo de OFF: nenhuma escrita de registrador por 200 us (14.30) */
    if (was_off && accel_mode != IIM42351_ACCEL_MODE_OFF) {
        k_usleep(IIM42351_ACCEL_START_US);
    }

    return 0;
}
