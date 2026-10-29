/*
 * SysCare - driver do acelerometro IIM-42351 (API publica)
 *
 * Usa so os tipos genericos de acc_types.h: o driver nao sabe que existe
 * um servico. Ha uma unica instancia, descrita no devicetree.
 */

#ifndef DRV_IIM42351_H_
#define DRV_IIM42351_H_

#include "acc_types.h"

/*
 * Liga o sensor (reguladores do devicetree), espera o power-up, confere
 * o WHO_AM_I e faz o soft reset.
 * cb: callback de eventos (queda livre); pode ser NULL.
 * Retorna 0, -EALREADY (ja inicializado), -ENODEV (SPI nao pronto ou
 * sensor nao identificado), -EIO ou erro do SPI.
 */
int drv_iim42351_init(acc_event_cb_t cb);

#endif /* DRV_IIM42351_H_ */
