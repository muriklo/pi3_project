/*
 * SysCare - acesso a registradores do IIM-42351
 *
 * Protocolo SPI do sensor (secao 9.6 do datasheet):
 *   - leitura: 1 byte de endereco com o bit 7 em 1, depois os dados
 *     (aceita burst);
 *   - escrita: 1 byte de endereco com o bit 7 em 0, depois o valor
 *     (so escrita simples, um registrador por vez).
 * O REG_BANK_SEL (0x76) e acessivel de qualquer banco; ele so e escrito
 * quando o banco muda (cache em cur_bank).
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "drv_iim42351_bus.h"
#include "drv_iim42351_reg.h"

#define BANK_UNKNOWN    0xFF

static const hal_spi_dev_t *bus_spi;
static uint8_t cur_bank = BANK_UNKNOWN;
static K_MUTEX_DEFINE(bus_lock);

void iim42351_bus_init(const hal_spi_dev_t *spi)
{
    k_mutex_lock(&bus_lock, K_FOREVER);
    bus_spi = spi;
    cur_bank = BANK_UNKNOWN;
    k_mutex_unlock(&bus_lock);
}

void iim42351_bus_bank_reset(void)
{
    k_mutex_lock(&bus_lock, K_FOREVER);
    cur_bank = BANK_UNKNOWN;
    k_mutex_unlock(&bus_lock);
}

/* =========================================================
 * Acesso sem lock (quem chama ja segura o bus_lock)
 * ========================================================= */

static int select_bank(uint8_t bank)
{
    uint8_t tx[2] = {
        IIM42351_REG_BANK_SEL_ADDR,
        FIELD_PREP(IIM42351_BANK_SEL, bank),
    };
    int err;

    if (bank == cur_bank) {
        return 0;
    }

    err = hal_spi_write(bus_spi, tx, sizeof(tx));
    cur_bank = err ? BANK_UNKNOWN : bank;

    return err;
}

static int reg_read_locked(uint16_t reg, uint8_t *buf, size_t len)
{
    uint8_t cmd = IIM42351_SPI_READ | IIM42351_REG_ADDR(reg);
    int err;

    err = select_bank(IIM42351_REG_BANK(reg));
    if (err) {
        return err;
    }

    return hal_spi_write_read(bus_spi, &cmd, 1, buf, len);
}

static int reg_write_locked(uint16_t reg, uint8_t val)
{
    uint8_t tx[2] = { IIM42351_REG_ADDR(reg), val };
    int err;

    err = select_bank(IIM42351_REG_BANK(reg));
    if (err) {
        return err;
    }

    return hal_spi_write(bus_spi, tx, sizeof(tx));
}

/* =========================================================
 * API
 * ========================================================= */

int iim42351_reg_read(uint16_t reg, uint8_t *buf, size_t len)
{
    int err;

    if (bus_spi == NULL) {
        return -ENODEV;
    }

    k_mutex_lock(&bus_lock, K_FOREVER);
    err = reg_read_locked(reg, buf, len);
    k_mutex_unlock(&bus_lock);

    return err;
}

int iim42351_reg_write(uint16_t reg, uint8_t val)
{
    int err;

    if (bus_spi == NULL) {
        return -ENODEV;
    }

    k_mutex_lock(&bus_lock, K_FOREVER);
    err = reg_write_locked(reg, val);
    k_mutex_unlock(&bus_lock);

    return err;
}

int iim42351_reg_update(uint16_t reg, uint8_t mask, uint8_t val)
{
    uint8_t cur;
    uint8_t new;
    int err;

    if (bus_spi == NULL) {
        return -ENODEV;
    }

    k_mutex_lock(&bus_lock, K_FOREVER);

    err = reg_read_locked(reg, &cur, 1);
    if (!err) {
        new = (cur & ~mask) | (val & mask);
        if (new != cur) {
            err = reg_write_locked(reg, new);
        }
    }

    k_mutex_unlock(&bus_lock);

    return err;
}
