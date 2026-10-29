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
 * o WHO_AM_I, faz o soft reset e aplica a configuracao inicial do
 * devicetree (accel-fs, accel-hz, accel-mode).
 * cb: callback de eventos (queda livre); pode ser NULL.
 * Retorna 0, -EALREADY (ja inicializado), -ENODEV (SPI nao pronto ou
 * sensor nao identificado), -EIO ou erro do SPI.
 */
int drv_iim42351_init(acc_event_cb_t cb);

/*
 * Aplica fundo de escala, ODR e modo. Usa o menor valor suportado que
 * atende o pedido (ex.: 3 g -> 4 g; 70 Hz -> 100 Hz) e devolve em cfg o
 * que foi aplicado. Pedidos acima do maximo usam o maximo do modo.
 * ODRs so do modo LP: 1,5625 a 6,25 Hz; so do LN: 1 a 8 kHz.
 * Retorna 0, -ENODEV (nao inicializado), -EINVAL ou erro do SPI.
 */
int drv_iim42351_config(acc_config_t *cfg);

/*
 * Le uma amostra dos tres eixos, em mg.
 * Retorna 0, -EAGAIN (ainda sem amostra valida, logo depois de ligar),
 * -ENODATA (acelerometro desligado), -ENODEV, -EINVAL ou erro do SPI.
 */
int drv_iim42351_read(acc_sample_t *sample);

#endif /* DRV_IIM42351_H_ */
