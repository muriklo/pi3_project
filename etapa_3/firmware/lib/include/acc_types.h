/*
 * SysCare - tipos genericos do acelerometro
 *
 * Interface compartilhada entre o servico e os drivers: nenhum dos dois
 * depende do outro, os dois dependem deste arquivo. Cresce fase a fase.
 */

#ifndef ACC_TYPES_H_
#define ACC_TYPES_H_

#include <stdint.h>

/* Eventos detectados pelo sensor */
typedef enum {
    ACC_EVENT_FREEFALL,
} acc_event_type_t;

typedef struct {
    acc_event_type_t type;
    uint16_t duration_ms;   /* duracao da queda */
    uint16_t height_cm;     /* altura estimada */
} acc_event_t;

/* Chamado fora de interrupcao (workqueue) quando ocorre um evento */
typedef void (*acc_event_cb_t)(const acc_event_t *evt);

#endif /* ACC_TYPES_H_ */
