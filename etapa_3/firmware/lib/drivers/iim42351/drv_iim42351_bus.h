/*
 * SysCare - acesso a registradores do IIM-42351
 *
 * Unico arquivo do driver que conhece a HAL SPI: suportar outra
 * interface (ex.: I2C) seria trocar so este par .h/.c.
 *
 * Cuida do protocolo do sensor (bit de leitura e troca de banco) e
 * serializa os acessos, porque trocar o banco e acessar o registrador
 * precisam acontecer juntos.
 *
 * Os registradores sao os IIM42351_REG_* de drv_iim42351_reg.h (banco +
 * endereco). Retornos: 0 ou errno negativo.
 */

#ifndef DRV_IIM42351_BUS_H_
#define DRV_IIM42351_BUS_H_

#include <stddef.h>
#include <stdint.h>

#include "hal_spi.h"

/* Associa o barramento; o banco atual passa a ser desconhecido */
void iim42351_bus_init(const hal_spi_dev_t *spi);

/* Chamar depois do soft reset: o banco volta a ser desconhecido */
void iim42351_bus_bank_reset(void);

/* Le len bytes a partir de reg (burst) */
int iim42351_reg_read(uint16_t reg, uint8_t *buf, size_t len);

/* Escreve um byte */
int iim42351_reg_write(uint16_t reg, uint8_t val);

/*
 * Leitura-modificacao-escrita: so os bits de mask mudam (val ja
 * posicionado, ex.: FIELD_PREP). Nao escreve se o valor nao mudar.
 */
int iim42351_reg_update(uint16_t reg, uint8_t mask, uint8_t val);

#endif /* DRV_IIM42351_BUS_H_ */
