/*
 * SysCare - HAL SPI
 *
 * Transacoes SPI genericas. Nao conhece nenhum dispositivo: o protocolo
 * de cada um (bit de leitura, endereco, banco) fica no driver dele.
 *
 * Os parametros do dispositivo vem todos do no no devicetree:
 *   - frequencia: spi-max-frequency;
 *   - modo: spi-cpol / spi-cpha;
 *   - CS: cs-gpios do barramento, indexado pelo reg do no.
 *
 * Retornos: 0 ou errno negativo do Zephyr. O lock do barramento em cada
 * transacao e feito pelo driver SPI do Zephyr.
 */

#ifndef HAL_SPI_H_
#define HAL_SPI_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/drivers/spi.h>

typedef struct {
    struct spi_dt_spec spec;
} hal_spi_dev_t;

/* Inicializador de um hal_spi_dev_t a partir de um no SPI do devicetree */
#define HAL_SPI_DEV_DT(node_id) \
    { .spec = SPI_DT_SPEC_GET(node_id, SPI_OP_MODE_MASTER | SPI_WORD_SET(8)) }

/* Barramento e porta do CS prontos */
bool hal_spi_is_ready(const hal_spi_dev_t *dev);

/* Envia tx[0..len-1] */
int hal_spi_write(const hal_spi_dev_t *dev, const uint8_t *tx, size_t len);

/*
 * Envia tx[0..tx_len-1] (ex.: comando/endereco) e, na mesma transacao
 * (CS ativo o tempo todo), le rx_len bytes em rx. O que chega durante o
 * envio do tx e descartado.
 */
int hal_spi_write_read(const hal_spi_dev_t *dev,
                       const uint8_t *tx, size_t tx_len,
                       uint8_t *rx, size_t rx_len);

/* Full-duplex: envia tx e recebe rx ao mesmo tempo (tx ou rx podem ser NULL) */
int hal_spi_transceive(const hal_spi_dev_t *dev,
                       const uint8_t *tx, uint8_t *rx, size_t len);

#endif /* HAL_SPI_H_ */
