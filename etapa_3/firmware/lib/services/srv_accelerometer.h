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
 * Liga e identifica o acelerometro.
 * cb: callback de eventos (queda livre); pode ser NULL.
 */
int srv_accelerometer_init(acc_event_cb_t cb);

#endif /* SRV_ACCELEROMETER_H_ */
