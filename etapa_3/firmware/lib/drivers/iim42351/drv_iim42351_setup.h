/*
 * SysCare - sequencias de configuracao do IIM-42351
 *
 * Cada funcao e uma sequencia de registradores do datasheet. Usa so o
 * acesso a registradores (drv_iim42351_bus.h).
 */

#ifndef DRV_IIM42351_SETUP_H_
#define DRV_IIM42351_SETUP_H_

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

#endif /* DRV_IIM42351_SETUP_H_ */
