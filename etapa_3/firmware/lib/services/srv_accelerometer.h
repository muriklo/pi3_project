/*
 * SysCare - servico do acelerometro
 *
 * API generica usada pela aplicacao. Por baixo, chama o driver do modelo
 * habilitado no build (escolhido pelo devicetree e pelo Kconfig).
 * Retornos: 0 ou errno negativo; -ENOTSUP se o driver nao tiver o recurso.
 */

#ifndef SRV_ACCELEROMETER_H_
#define SRV_ACCELEROMETER_H_

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

#endif /* SRV_ACCELEROMETER_H_ */
