/*
 * SysCare - driver do acelerometro IIM-42351 (API publica)
 *
 * Usa so os tipos genericos de acc_types.h: o driver nao sabe que existe
 * um servico. Ha uma unica instancia, descrita no devicetree.
 */

#ifndef DRV_IIM42351_H_
#define DRV_IIM42351_H_

#include <stddef.h>
#include <stdint.h>

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

/*
 * Liga a FIFO do sensor (modo stream). watermark: nivel em amostras
 * (1 a 254) que gera o evento ACC_EVENT_FIFO_WATERMARK pelo pino de
 * interrupcao do devicetree (INT2, ou INT1 se so ele existir); 0 = sem
 * evento. Sem pino, a FIFO funciona igual e deve ser lida por polling.
 * A 500 Hz a FIFO enche em ~0,5 s.
 * Retorna 0, -EALREADY, -EINVAL (watermark grande demais), -ENODEV ou
 * erro do SPI.
 */
int drv_iim42351_buffer_start(uint16_t watermark);

/*
 * Le da FIFO no maximo max amostras validas para buf, em mg, e informa
 * em count quantas leu. Amostras invalidas sao descartadas.
 */
int drv_iim42351_buffer_read(acc_sample_t *buf, size_t max, size_t *count);

/* Desliga a FIFO */
int drv_iim42351_buffer_stop(void);

#endif /* DRV_IIM42351_H_ */
