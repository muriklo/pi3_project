/*
 * SysCare - tipos genericos do acelerometro
 *
 * Interface compartilhada entre o servico e os drivers: nenhum dos dois
 * depende do outro, os dois dependem deste arquivo. Cresce fase a fase.
 */

#ifndef ACC_TYPES_H_
#define ACC_TYPES_H_

#include <stdint.h>

/* Modo de operacao */
typedef enum {
    ACC_MODE_OFF,
    ACC_MODE_LOW_POWER,
    ACC_MODE_LOW_NOISE,
} acc_mode_t;

/*
 * Configuracao. Na chamada vai o pedido; na volta, o que foi aplicado:
 * o driver usa o menor valor suportado que atende o pedido.
 * odr_hz e inteiro: frequencias fracionarias aparecem truncadas
 * (ex.: 12 = 12,5 Hz, 6 = 6,25 Hz), como no devicetree.
 */
typedef struct {
    uint8_t range_g;        /* fundo de escala: +-range_g */
    uint16_t odr_hz;        /* taxa de amostragem */
    acc_mode_t mode;
} acc_config_t;

/* Amostra em mg */
typedef struct {
    int16_t x_mg;
    int16_t y_mg;
    int16_t z_mg;
} acc_sample_t;

/* Eventos detectados pelo sensor */
typedef enum {
    ACC_EVENT_FREEFALL,         /* queda livre */
    ACC_EVENT_FIFO_WATERMARK,   /* FIFO chegou ao nivel pedido: hora de ler */
} acc_event_type_t;

typedef struct {
    acc_event_type_t type;
    uint16_t duration_ms;   /* ACC_EVENT_FREEFALL: duracao da queda */
    uint16_t height_cm;     /* ACC_EVENT_FREEFALL: altura estimada */
} acc_event_t;

/* Chamado fora de interrupcao (workqueue) quando ocorre um evento */
typedef void (*acc_event_cb_t)(const acc_event_t *evt);

#endif /* ACC_TYPES_H_ */
