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
    int (*buffer_start)(uint16_t watermark);
    int (*buffer_read)(acc_sample_t *buf, size_t max, size_t *count);
    int (*buffer_stop)(void);
    int (*freefall_enable)(acc_freefall_config_t *cfg);
    int (*freefall_disable)(void);
} acc_ops_t;

#if defined(CONFIG_SYSCARE_IIM42351)

#include "drv_iim42351.h"

static const acc_ops_t ops = {
    .init = drv_iim42351_init,
    .config = drv_iim42351_config,
    .read = drv_iim42351_read,
    .buffer_start = drv_iim42351_buffer_start,
    .buffer_read = drv_iim42351_buffer_read,
    .buffer_stop = drv_iim42351_buffer_stop,
    .freefall_enable = drv_iim42351_freefall_enable,
    .freefall_disable = drv_iim42351_freefall_disable,
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

int srv_accelerometer_buffer_start(uint16_t watermark)
{
    if (ops.buffer_start == NULL) {
        return -ENOTSUP;
    }

    return ops.buffer_start(watermark);
}

int srv_accelerometer_buffer_read(acc_sample_t *buf, size_t max, size_t *count)
{
    if (ops.buffer_read == NULL) {
        return -ENOTSUP;
    }

    return ops.buffer_read(buf, max, count);
}

int srv_accelerometer_buffer_stop(void)
{
    if (ops.buffer_stop == NULL) {
        return -ENOTSUP;
    }

    return ops.buffer_stop();
}

int srv_accelerometer_freefall_enable(acc_freefall_config_t *cfg)
{
    if (ops.freefall_enable == NULL) {
        return -ENOTSUP;
    }

    return ops.freefall_enable(cfg);
}

int srv_accelerometer_freefall_disable(void)
{
    if (ops.freefall_disable == NULL) {
        return -ENOTSUP;
    }

    return ops.freefall_disable();
}
