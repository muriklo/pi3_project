/*
 * SysCare - sequencias de configuracao do IIM-42351
 *
 * Cada funcao e uma sequencia de registradores do datasheet. Usa so o
 * acesso a registradores (drv_iim42351_bus.h).
 */

#ifndef DRV_IIM42351_SETUP_H_
#define DRV_IIM42351_SETUP_H_

/*
 * Identifica o sensor (WHO_AM_I), faz o soft reset, desliga a interface
 * I2C e ajusta o INT_CONFIG1. Exige o sensor ja alimentado.
 * Retorna 0, -ENODEV (WHO_AM_I errado), -EIO (reset nao concluido) ou
 * erro do SPI.
 */
int iim42351_setup_init(void);

#endif /* DRV_IIM42351_SETUP_H_ */
