/*
 * SysCare - sequencias de configuracao do IIM-42351
 *
 * Cada funcao e uma sequencia de registradores do datasheet. Usa so o
 * acesso a registradores (drv_iim42351_bus.h).
 */

#ifndef DRV_IIM42351_SETUP_H_
#define DRV_IIM42351_SETUP_H_

#include <stdbool.h>
#include <stdint.h>

/*
 * Identifica o sensor (WHO_AM_I), faz o soft reset, desliga a interface
 * I2C e ajusta o INT_CONFIG1. Exige o sensor ja alimentado.
 * Retorna 0, -ENODEV (WHO_AM_I errado), -EIO (reset nao concluido) ou
 * erro do SPI.
 */
int iim42351_setup_init(void);

/*
 * Escreve fundo de escala, ODR e modo (codigos dos campos ACCEL_FS_SEL,
 * ACCEL_ODR e ACCEL_MODE). Pela secao 13.6 esses sao os unicos campos
 * que podem mudar com o sensor ligado. Saindo de OFF, espera o tempo
 * sem escritas exigido pelo PWR_MGMT0 (14.30).
 */
int iim42351_setup_accel(uint8_t fs_sel, uint8_t odr_code, uint8_t accel_mode);

/*
 * Pinos INT1/INT2 do sensor: latched, push-pull, ativo alto (14.3).
 * So mexe nos pinos indicados. Exige o acelerometro desligado (13.6).
 */
int iim42351_setup_int_pins(bool int1, bool int2);

/*
 * Liga a FIFO em modo stream com o pacote 1 (acel + temp, 8 bytes) e
 * contagem em pacotes. watermark: nivel em pacotes (0 = sem watermark);
 * int_src_reg/int_src_bit: registrador e bit INT_SOURCE que rotea o
 * watermark para o pino (int_src_reg 0 = sem interrupcao).
 * Desliga o acelerometro durante a configuracao (13.6) e restaura o modo.
 */
int iim42351_setup_fifo_start(uint16_t watermark, uint16_t int_src_reg, uint8_t int_src_bit);

/* Desfaz o iim42351_setup_fifo_start (mesmos int_src_reg/int_src_bit) */
int iim42351_setup_fifo_stop(uint16_t int_src_reg, uint8_t int_src_bit);

/*
 * Queda livre (APEX). A deteccao usa o DMP a 500 Hz: cada unidade de
 * FF_DUR (APEX_DATA0/1) vale 1/500 s (8.5).
 */
#define IIM42351_FF_DMP_ODR_HZ  500

/* Codigos dos campos de queda livre (banco 4, 18.6, 18.7 e 18.11) */
struct iim42351_ff_codes {
    uint8_t lowg_peak;      /* LOWG_PEAK_TH_SEL */
    uint8_t highg_peak;     /* HIGHG_PEAK_TH_SEL */
    uint8_t min_cm;         /* FF_MIN_DURATION_CM */
    uint8_t max_cm;         /* FF_MAX_DURATION_CM */
    uint8_t debounce;       /* FF_DEBOUNCE_DURATION */
};

/*
 * Liga a deteccao de queda livre na sequencia da secao 8.5: acelerometro
 * em LP a 500 Hz e +-16 g, DMP a 500 Hz sem power save, limiares,
 * inicializacao do DMP, rota da interrupcao (int_src_reg/int_src_bit:
 * INT_SOURCE6 ou 7) e por fim FF_ENABLE. Leva ~50 ms.
 */
int iim42351_setup_freefall_enable(const struct iim42351_ff_codes *codes,
                                   uint16_t int_src_reg, uint8_t int_src_bit);

/* Desliga a deteccao (mesmos int_src_reg/int_src_bit); o acelerometro segue ligado */
int iim42351_setup_freefall_disable(uint16_t int_src_reg, uint8_t int_src_bit);

#endif /* DRV_IIM42351_SETUP_H_ */
