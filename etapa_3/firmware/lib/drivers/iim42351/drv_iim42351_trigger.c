/*
 * SysCare - interrupcoes do IIM-42351
 *
 * Como no exemplo icm42605: a interrupcao do pino so agenda trabalho; o
 * SPI (ler o status) fica fora da ISR, na workqueue do sistema. Os pinos
 * do sensor sao latched: o status lido limpa o pino (INT_CONFIG0 padrao).
 *
 * Cada evento prefere um pino e usa o outro se so ele existir:
 *   - queda livre: INT1 (pino exclusivo; o INT2 pode virar FSYNC/CLKIN);
 *   - watermark da FIFO: INT2.
 * Com um pino so, os dois eventos o dividem.
 */

#define DT_DRV_COMPAT invensense_iim42351

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include "drv_iim42351_bus.h"
#include "drv_iim42351_reg.h"
#include "drv_iim42351_setup.h"
#include "drv_iim42351_trigger.h"
#include "hal_gpio.h"

LOG_MODULE_DECLARE(iim42351, CONFIG_SYSCARE_IIM42351_LOG_LEVEL);

static hal_gpio_dev_t int1 = HAL_GPIO_DEV_DT_OR_NONE(DT_DRV_INST(0), int1_gpios);
static hal_gpio_dev_t int2 = HAL_GPIO_DEV_DT_OR_NONE(DT_DRV_INST(0), int2_gpios);

static acc_event_cb_t event_cb;
static struct k_work irq_work;

/* Reexecucoes seguidas da work com o pino ainda ativo (protecao contra laco) */
#define IRQ_RERUN_MAX   8
static uint8_t irq_reruns;

/* Contexto de interrupcao: so agenda o trabalho */
static void on_int_pin(void *user)
{
    ARG_UNUSED(user);
    k_work_submit(&irq_work);
}

static void notify(const acc_event_t *evt)
{
    if (event_cb != NULL) {
        event_cb(evt);
    }
}

/* FF_DUR (APEX_DATA0 = LSB, APEX_DATA1 = MSB) em amostras do DMP (8.5, 14.19) */
static void notify_freefall(void)
{
    uint8_t data[2];
    acc_event_t evt = { .type = ACC_EVENT_FREEFALL };
    uint64_t samples;

    if (iim42351_reg_read(IIM42351_REG_APEX_DATA0, data, sizeof(data)) != 0) {
        return;
    }

    samples = sys_get_le16(data);

    /* t = n / ODR;  d = g * t^2 / 2 = 981 cm/s^2 * n^2 / (2 * ODR^2) */
    evt.duration_ms = (uint16_t)((samples * 1000U) / IIM42351_FF_DMP_ODR_HZ);
    evt.height_cm = (uint16_t)((981U * samples * samples +
                                IIM42351_FF_DMP_ODR_HZ * IIM42351_FF_DMP_ODR_HZ) /
                               (2U * IIM42351_FF_DMP_ODR_HZ * IIM42351_FF_DMP_ODR_HZ));

    notify(&evt);
}

static bool pin_active(const hal_gpio_dev_t *pin)
{
    return hal_gpio_is_present(pin) && hal_gpio_get(pin) > 0;
}

static void irq_work_handler(struct k_work *work)
{
    uint8_t status;
    uint8_t status3;

    ARG_UNUSED(work);

    /* Ler os status limpa os bits e solta o pino (14.15, 14.26) */
    if (iim42351_reg_read(IIM42351_REG_INT_STATUS, &status, 1) != 0 ||
        iim42351_reg_read(IIM42351_REG_INT_STATUS3, &status3, 1) != 0) {
        return;
    }

    if (status3 & IIM42351_FF_DET_INT) {
        notify_freefall();
    }

    if (status & IIM42351_FIFO_THS_INT) {
        const acc_event_t evt = { .type = ACC_EVENT_FIFO_WATERMARK };

        notify(&evt);
    }

    /*
     * Pino dividido: se outro evento travou o pino enquanto os status eram
     * lidos, ele continua ativo e nao havera nova borda. Trata de novo.
     */
    if (pin_active(&int1) || pin_active(&int2)) {
        if (irq_reruns++ < IRQ_RERUN_MAX) {
            k_work_submit(&irq_work);
        } else {
            LOG_WRN("Pino de interrupcao preso em nivel ativo");
        }
    } else {
        irq_reruns = 0;
    }
}

static int pin_enable(hal_gpio_dev_t *pin, const char *name)
{
    int err;

    if (!hal_gpio_is_present(pin)) {
        return 0;
    }

    err = hal_gpio_irq_enable(pin, on_int_pin, NULL);
    if (err) {
        LOG_ERR("Falha ao habilitar a interrupcao do %s (%d)", name, err);
    }

    return err;
}

int iim42351_trigger_init(acc_event_cb_t cb)
{
    int err;

    event_cb = cb;
    k_work_init(&irq_work, irq_work_handler);

    /* Primeiro o sensor (push-pull, ativo alto), depois a interrupcao na placa */
    err = iim42351_setup_int_pins(hal_gpio_is_present(&int1), hal_gpio_is_present(&int2));
    if (err) {
        return err;
    }

    err = pin_enable(&int1, "INT1");
    if (err) {
        return err;
    }

    err = pin_enable(&int2, "INT2");
    if (err) {
        return err;
    }

    LOG_INF("Interrupcoes: INT1 %s, INT2 %s",
            hal_gpio_is_present(&int1) ? "ligado" : "ausente",
            hal_gpio_is_present(&int2) ? "ligado" : "ausente");

    return 0;
}

int iim42351_trigger_freefall_route(uint16_t *reg, uint8_t *bit)
{
    if (hal_gpio_is_present(&int1)) {
        *reg = IIM42351_REG_INT_SOURCE6;
        *bit = IIM42351_FREEFALL_DET_INT1_EN;
        return 0;
    }

    if (hal_gpio_is_present(&int2)) {
        *reg = IIM42351_REG_INT_SOURCE7;
        *bit = IIM42351_FREEFALL_DET_INT2_EN;
        return 0;
    }

    return -ENOTSUP;
}

int iim42351_trigger_fifo_route(uint16_t *reg, uint8_t *bit)
{
    if (hal_gpio_is_present(&int2)) {
        *reg = IIM42351_REG_INT_SOURCE3;
        *bit = IIM42351_FIFO_THS_INT2_EN;
        return 0;
    }

    if (hal_gpio_is_present(&int1)) {
        *reg = IIM42351_REG_INT_SOURCE0;
        *bit = IIM42351_FIFO_THS_INT1_EN;
        return 0;
    }

    return -ENOTSUP;
}
