/*
 * SysCare - interrupcoes do IIM-42351
 *
 * Como no exemplo icm42605: a interrupcao do pino so agenda trabalho; o
 * SPI (ler o status) fica fora da ISR, na workqueue do sistema. Os pinos
 * do sensor sao latched: o status lido limpa o pino (INT_CONFIG0 padrao).
 */

#define DT_DRV_COMPAT invensense_iim42351

#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
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

/* Contexto de interrupcao: so agenda o trabalho */
static void on_int_pin(void *user)
{
    ARG_UNUSED(user);
    k_work_submit(&irq_work);
}

static void notify(acc_event_type_t type)
{
    const acc_event_t evt = { .type = type };

    if (event_cb != NULL) {
        event_cb(&evt);
    }
}

static void irq_work_handler(struct k_work *work)
{
    uint8_t status;

    ARG_UNUSED(work);

    /* Ler o INT_STATUS limpa os bits e solta o pino (14.15) */
    if (iim42351_reg_read(IIM42351_REG_INT_STATUS, &status, 1) != 0) {
        return;
    }

    if (status & IIM42351_FIFO_THS_INT) {
        notify(ACC_EVENT_FIFO_WATERMARK);
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
