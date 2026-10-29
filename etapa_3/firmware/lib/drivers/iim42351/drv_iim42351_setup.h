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
 * Liga a FIFO em modo stream com o pacote 1 (acel + temp, 8 bytes) e
 * contagem em pacotes. watermark: nivel em pacotes (0 = sem watermark);
 * int_src_reg/int_src_bit: registrador e bit INT_SOURCE que rotea o
 * watermark para o pino (int_src_reg 0 = sem interrupcao).
 * Desliga o acelerometro durante a configuracao (13.6) e restaura o modo.
 */
int iim42351_setup_fifo_start(uint16_t watermark, uint16_t int_src_reg, uint8_t int_src_bit);

/* Desfaz o iim42351_setup_fifo_start (mesmos int_src_reg/int_src_bit) */
int iim42351_setup_fifo_stop(uint16_t int_src_reg, uint8_t int_src_bit);

#endif /* DRV_IIM42351_SETUP_H_ */
