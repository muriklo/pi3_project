/*
 * SysCare - servico do acelerometro
 *
 * API generica usada pela aplicacao. Por baixo, chama o driver do modelo
 * habilitado no build (escolhido pelo devicetree e pelo Kconfig).
 * Retornos: 0 ou errno negativo; -ENOTSUP se o driver nao tiver o recurso.
 */

#ifndef SRV_ACCELEROMETER_H_
#define SRV_ACCELEROMETER_H_

#include <stddef.h>
#include <stdint.h>

#include "acc_types.h"

/*
 * Liga e identifica o acelerometro e aplica a configuracao inicial do
 * devicetree.
 * cb: callback de eventos (queda livre); pode ser NULL.
 */
int srv_accelerometer_init(acc_event_cb_t cb);

/*
 * Aplica fundo de escala, ODR e modo. Devolve em cfg o que foi aplicado
 * (o driver arredonda para o menor valor suportado que atende o pedido).
 */
int srv_accelerometer_config(acc_config_t *cfg);

/* Le uma amostra em mg; -EAGAIN enquanto ainda nao ha amostra valida */
int srv_accelerometer_read(acc_sample_t *sample);

/*
 * Liga a leitura em buffer (FIFO do sensor). watermark: quantas amostras
 * acumular antes de gerar o evento ACC_EVENT_FIFO_WATERMARK (0 = sem
 * evento). Se o hardware nao tiver pino de interrupcao, nao ha evento:
 * chamar srv_accelerometer_buffer_read periodicamente.
 */
int srv_accelerometer_buffer_start(uint16_t watermark);

/* Le ate max amostras do buffer; count recebe quantas foram lidas */
int srv_accelerometer_buffer_read(acc_sample_t *buf, size_t max, size_t *count);

/* Desliga a leitura em buffer */
int srv_accelerometer_buffer_stop(void);

/*
 * Liga a deteccao de queda livre: cada queda gera o evento
 * ACC_EVENT_FREEFALL (duracao e altura) no callback do init. cfg volta
 * com os valores aplicados. Enquanto estiver ligada, a configuracao do
 * acelerometro fica fixa (config e buffer_start retornam -EBUSY).
 */
int srv_accelerometer_freefall_enable(acc_freefall_config_t *cfg);

/* Desliga a deteccao de queda livre */
int srv_accelerometer_freefall_disable(void);

#endif /* SRV_ACCELEROMETER_H_ */
