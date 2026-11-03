/*
 * SysCare - HAL SPI
 *
 * Generalizacao do inv_spi_read do exemplo icm42605: o buffer de
 * recepcao comeca com um trecho descartado (buf NULL) do tamanho do
 * comando enviado.
 */

#include <zephyr/sys/util.h>

#include "hal_spi.h"

bool hal_spi_is_ready(const hal_spi_dev_t *dev)
{
    return spi_is_ready_dt(&dev->spec);
}

int hal_spi_write(const hal_spi_dev_t *dev, const uint8_t *tx, size_t len)
{
    const struct spi_buf tx_buf = { .buf = (void *)tx, .len = len };
    const struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };

    return spi_write_dt(&dev->spec, &tx_set);
}

int hal_spi_write_read(const hal_spi_dev_t *dev,
                       const uint8_t *tx, size_t tx_len,
                       uint8_t *rx, size_t rx_len)
{
    const struct spi_buf tx_buf = { .buf = (void *)tx, .len = tx_len };
    const struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    const struct spi_buf rx_bufs[] = {
        { .buf = NULL, .len = tx_len },     /* descarta o que chega durante o tx */
        { .buf = rx, .len = rx_len },
    };
    const struct spi_buf_set rx_set = { .buffers = rx_bufs, .count = ARRAY_SIZE(rx_bufs) };

    return spi_transceive_dt(&dev->spec, &tx_set, &rx_set);
}

int hal_spi_transceive(const hal_spi_dev_t *dev,
                       const uint8_t *tx, uint8_t *rx, size_t len)
{
    const struct spi_buf tx_buf = { .buf = (void *)tx, .len = len };
    const struct spi_buf rx_buf = { .buf = rx, .len = len };
    const struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    const struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };

    return spi_transceive_dt(&dev->spec,
                             (tx != NULL) ? &tx_set : NULL,
                             (rx != NULL) ? &rx_set : NULL);
}
