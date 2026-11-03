/*
 * SysCare - HAL GPIO
 */

#include <errno.h>
#include <stddef.h>

#include <zephyr/sys/util.h>

#include "hal_gpio.h"

/* Callback do Zephyr: recupera o hal_gpio_dev_t pelo gpio_callback embutido */
static void hal_gpio_isr(const struct device *port, struct gpio_callback *cb,
                         gpio_port_pins_t pins)
{
    hal_gpio_dev_t *dev = CONTAINER_OF(cb, hal_gpio_dev_t, cb);

    ARG_UNUSED(port);
    ARG_UNUSED(pins);

    if (dev->user_cb != NULL) {
        dev->user_cb(dev->user);
    }
}

bool hal_gpio_is_present(const hal_gpio_dev_t *dev)
{
    return dev->spec.port != NULL;
}

bool hal_gpio_is_ready(const hal_gpio_dev_t *dev)
{
    return hal_gpio_is_present(dev) && gpio_is_ready_dt(&dev->spec);
}

int hal_gpio_irq_enable(hal_gpio_dev_t *dev, hal_gpio_irq_cb_t cb, void *user)
{
    int err;

    if (!hal_gpio_is_ready(dev)) {
        return -ENODEV;
    }

    dev->user_cb = cb;
    dev->user = user;

    err = gpio_pin_configure_dt(&dev->spec, GPIO_INPUT);
    if (err) {
        return err;
    }

    gpio_init_callback(&dev->cb, hal_gpio_isr, BIT(dev->spec.pin));

    err = gpio_add_callback_dt(&dev->spec, &dev->cb);
    if (err) {
        return err;
    }

    return gpio_pin_interrupt_configure_dt(&dev->spec, GPIO_INT_EDGE_TO_ACTIVE);
}

int hal_gpio_irq_disable(hal_gpio_dev_t *dev)
{
    int err;

    if (!hal_gpio_is_ready(dev)) {
        return -ENODEV;
    }

    err = gpio_pin_interrupt_configure_dt(&dev->spec, GPIO_INT_DISABLE);
    if (err) {
        return err;
    }

    return gpio_remove_callback_dt(&dev->spec, &dev->cb);
}

int hal_gpio_get(const hal_gpio_dev_t *dev)
{
    if (!hal_gpio_is_ready(dev)) {
        return -ENODEV;
    }

    return gpio_pin_get_dt(&dev->spec);
}
