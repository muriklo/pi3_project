/*
 * SysCare - servico do acelerometro
 *
 * O driver e escolhido UMA vez, por #if, na tabela de operacoes; o resto
 * do servico so chama a tabela. Campo NULL = recurso nao suportado pelo
 * driver (-ENOTSUP).
 */

#include <errno.h>
#include <stddef.h>

#include "srv_accelerometer.h"

/* Operacoes de um driver de acelerometro (cresce fase a fase) */
typedef struct {
    int (*init)(acc_event_cb_t cb);
    int (*config)(acc_config_t *cfg);
    int (*read)(acc_sample_t *sample);
} acc_ops_t;

#if defined(CONFIG_SYSCARE_IIM42351)

#include "drv_iim42351.h"

static const acc_ops_t ops = {
    .init = drv_iim42351_init,
    .config = drv_iim42351_config,
    .read = drv_iim42351_read,
};

#else
#error "Nenhum driver de acelerometro habilitado (falta o no no devicetree?)"
#endif

int srv_accelerometer_init(acc_event_cb_t cb)
{
    if (ops.init == NULL) {
        return -ENOTSUP;
    }

    return ops.init(cb);
}

int srv_accelerometer_config(acc_config_t *cfg)
{
    if (ops.config == NULL) {
        return -ENOTSUP;
    }

    return ops.config(cfg);
}

int srv_accelerometer_read(acc_sample_t *sample)
{
    if (ops.read == NULL) {
        return -ENOTSUP;
    }

    return ops.read(sample);
}
