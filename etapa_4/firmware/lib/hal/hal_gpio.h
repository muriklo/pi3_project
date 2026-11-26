/*
 * SysCare - HAL GPIO
 *
 * Entrada com interrupcao sobre gpio_dt_spec. O pino, o nivel ativo e o
 * pull vem da propriedade *-gpios do devicetree.
 *
 * Retornos: 0 ou errno negativo do Zephyr.
 */

#ifndef HAL_GPIO_H_
#define HAL_GPIO_H_

#include <stdbool.h>

#include <zephyr/drivers/gpio.h>

/* Chamado em contexto de interrupcao: so sinalizar (k_work, semaforo) */
typedef void (*hal_gpio_irq_cb_t)(void *user);

typedef struct {
    struct gpio_dt_spec spec;
    struct gpio_callback cb;
    hal_gpio_irq_cb_t user_cb;
    void *user;
} hal_gpio_dev_t;

/*
 * Inicializador a partir de uma propriedade *-gpios opcional. Sem a
 * propriedade no devicetree, o pino fica "ausente" (hal_gpio_is_present).
 */
#define HAL_GPIO_DEV_DT_OR_NONE(node_id, prop) \
    { .spec = GPIO_DT_SPEC_GET_OR(node_id, prop, {0}) }

/* A propriedade existe no devicetree */
bool hal_gpio_is_present(const hal_gpio_dev_t *dev);

/* Presente e com a porta GPIO pronta */
bool hal_gpio_is_ready(const hal_gpio_dev_t *dev);

/* Configura como entrada e chama cb na borda para o nivel ativo */
int hal_gpio_irq_enable(hal_gpio_dev_t *dev, hal_gpio_irq_cb_t cb, void *user);

/* Desliga a interrupcao do pino */
int hal_gpio_irq_disable(hal_gpio_dev_t *dev);

/* Nivel logico (1 = ativo), ou errno negativo */
int hal_gpio_get(const hal_gpio_dev_t *dev);

#endif /* HAL_GPIO_H_ */
