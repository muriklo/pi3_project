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

/* =========================================================
 * FIFO (secao 6). Fora ODR, fundo de escala e modo, os registradores
 * so podem mudar com o acelerometro desligado (13.6).
 * ========================================================= */

static int accel_pause(uint8_t *saved_mode)
{
    uint8_t pwr;
    int err;

    err = iim42351_reg_read(IIM42351_REG_PWR_MGMT0, &pwr, 1);
    if (err) {
        return err;
    }

    *saved_mode = FIELD_GET(IIM42351_ACCEL_MODE, pwr);

    return iim42351_reg_update(IIM42351_REG_PWR_MGMT0, IIM42351_ACCEL_MODE,
                               FIELD_PREP(IIM42351_ACCEL_MODE, IIM42351_ACCEL_MODE_OFF));
}

static int accel_resume(uint8_t saved_mode)
{
    int err;

    err = iim42351_reg_update(IIM42351_REG_PWR_MGMT0, IIM42351_ACCEL_MODE,
                              FIELD_PREP(IIM42351_ACCEL_MODE, saved_mode));
    if (err) {
        return err;
    }

    if (saved_mode >= IIM42351_ACCEL_MODE_LP) {
        k_usleep(IIM42351_ACCEL_START_US);
    }

    return 0;
}

int iim42351_setup_fifo_start(uint16_t watermark, uint16_t int_src_reg, uint8_t int_src_bit)
{
    const uint8_t cfg1_mask = IIM42351_FIFO_RESUME_PARTIAL_RD | IIM42351_FIFO_WM_GT_TH |
                              IIM42351_FIFO_HIRES_EN | IIM42351_FIFO_TMST_FSYNC_EN |
                              IIM42351_FIFO_TEMP_EN | IIM42351_FIFO_PACKET_EN;
    uint8_t mode;
    int err;

    err = accel_pause(&mode);
    if (err) {
        return err;
    }

    /* FIFO_COUNT em pacotes, nao em bytes (14.28) */
    err = iim42351_reg_update(IIM42351_REG_INTF_CONFIG0, IIM42351_FIFO_COUNT_REC,
                              IIM42351_FIFO_COUNT_REC);
    if (err) {
        return err;
    }

    /*
     * Pacote 1 (6.4); leitura parcial retomavel, porque o driver le no
     * maximo o que cabe no buffer de quem chama; watermark reemitido a
     * cada amostra enquanto a FIFO estiver acima do nivel (sem isso ele
     * "dispara uma vez so", 14.38).
     */
    err = iim42351_reg_update(IIM42351_REG_FIFO_CONFIG1, cfg1_mask,
                              IIM42351_FIFO_RESUME_PARTIAL_RD | IIM42351_FIFO_WM_GT_TH |
                              FIELD_PREP(IIM42351_FIFO_PACKET_EN, IIM42351_FIFO_PACKET_EN_P1));
    if (err) {
        return err;
    }

    if (watermark > 0) {
        /* Nivel antes de rotear a interrupcao (14.39 e 14.40) */
        err = iim42351_reg_write(IIM42351_REG_FIFO_CONFIG2, watermark & 0xFF);
        if (!err) {
            err = iim42351_reg_update(IIM42351_REG_FIFO_CONFIG3, IIM42351_FIFO_WM_HI,
                                      FIELD_PREP(IIM42351_FIFO_WM_HI, watermark >> 8));
        }
        if (!err && int_src_reg != 0) {
            err = iim42351_reg_update(int_src_reg, int_src_bit, int_src_bit);
        }
        if (err) {
            return err;
        }
    }

    err = iim42351_reg_update(IIM42351_REG_FIFO_CONFIG, IIM42351_FIFO_MODE,
                              FIELD_PREP(IIM42351_FIFO_MODE, IIM42351_FIFO_MODE_STREAM));
    if (err) {
        return err;
    }

    err = iim42351_reg_write(IIM42351_REG_SIGNAL_PATH_RESET, IIM42351_FIFO_FLUSH);
    if (err) {
        return err;
    }

    return accel_resume(mode);
}

int iim42351_setup_fifo_stop(uint16_t int_src_reg, uint8_t int_src_bit)
{
    uint8_t mode;
    int err;

    err = accel_pause(&mode);
    if (err) {
        return err;
    }

    if (int_src_reg != 0) {
        err = iim42351_reg_update(int_src_reg, int_src_bit, 0);
        if (err) {
            return err;
        }
    }

    err = iim42351_reg_update(IIM42351_REG_FIFO_CONFIG, IIM42351_FIFO_MODE,
                              FIELD_PREP(IIM42351_FIFO_MODE, IIM42351_FIFO_MODE_BYPASS));
    if (err) {
        return err;
    }

    err = iim42351_reg_update(IIM42351_REG_FIFO_CONFIG1, IIM42351_FIFO_PACKET_EN,
                              FIELD_PREP(IIM42351_FIFO_PACKET_EN, IIM42351_FIFO_PACKET_EN_NONE));
    if (err) {
        return err;
    }

    err = iim42351_reg_write(IIM42351_REG_SIGNAL_PATH_RESET, IIM42351_FIFO_FLUSH);
    if (err) {
        return err;
    }

    return accel_resume(mode);
}
