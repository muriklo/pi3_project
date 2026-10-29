/*
 * SysCare - interrupcoes do IIM-42351 (uso interno do driver)
 *
 * Os pinos vem das propriedades opcionais int1-gpios e int2-gpios do
 * devicetree. Sem CONFIG_SYSCARE_IIM42351_TRIGGER (nenhum pino no
 * devicetree), as funcoes viram stubs e o driver funciona por polling.
 */

#ifndef DRV_IIM42351_TRIGGER_H_
#define DRV_IIM42351_TRIGGER_H_

#include <errno.h>
#include <stdint.h>

#include <zephyr/sys/util.h>

#include "acc_types.h"

#if defined(CONFIG_SYSCARE_IIM42351_TRIGGER)

/*
 * Configura os pinos do sensor e da placa. cb recebe os eventos, no
 * contexto da workqueue do sistema. Exige o acelerometro desligado.
 */
int iim42351_trigger_init(acc_event_cb_t cb);

/*
 * Registrador e bit INT_SOURCE para rotear o watermark da FIFO: INT2 se
 * existir no devicetree, senao INT1. -ENOTSUP sem nenhum pino.
 */
int iim42351_trigger_fifo_route(uint16_t *reg, uint8_t *bit);

#else

static inline int iim42351_trigger_init(acc_event_cb_t cb)
{
    ARG_UNUSED(cb);
    return 0;
}

static inline int iim42351_trigger_fifo_route(uint16_t *reg, uint8_t *bit)
{
    ARG_UNUSED(reg);
    ARG_UNUSED(bit);
    return -ENOTSUP;
}

#endif /* CONFIG_SYSCARE_IIM42351_TRIGGER */

#endif /* DRV_IIM42351_TRIGGER_H_ */
