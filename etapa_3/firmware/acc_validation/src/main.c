/*
 * SysCare - acc_validation: validacao do acelerometro IIM-42351
 * XIAO nRF52840 / Zephyr / NCS 3.4.1
 *
 * Projeto so com o acelerometro; os componentes reutilizaveis
 * (bindings, HAL, driver, servico) ficam no modulo ../lib.
 *
 * Fase 3: configuracao e leitura
 *   1. Espera o terminal abrir na USB, para os logs do init aparecerem
 *   2. Inicializa o acelerometro (o driver aplica e loga a config do DT)
 *   3. Pede +-3 g / 70 Hz: o esperado e voltar +-4 g / 100 Hz
 *   4. Le e imprime a aceleracao a 10 Hz. Parado, |a| fica perto de
 *      1000 mg em qualquer posicao.
 */

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "srv_accelerometer.h"

#define STATUS_PERIOD   K_SECONDS(2)
#define PRINT_PERIOD    K_MSEC(100)

/*
 * O console e USB (CDC ACM): o que e enviado antes de o terminal abrir
 * se perde. Espera o terminal sinalizar DTR (a maioria faz isso ao abrir
 * a porta).
 */
static void wait_for_terminal(void)
{
    const struct device *console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    uint32_t dtr = 0;

    if (!device_is_ready(console)) {
        return;
    }

    while (!dtr) {
        if (uart_line_ctrl_get(console, UART_LINE_CTRL_DTR, &dtr) != 0) {
            return;     /* console sem controle de linha: segue sem esperar */
        }
        k_msleep(100);
    }
}

/* Raiz quadrada inteira, para |a| sem ponto flutuante */
static uint32_t isqrt(uint32_t v)
{
    uint32_t root = 0;
    uint32_t bit = 1UL << 30;

    while (bit > v) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (v >= root + bit) {
            v -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }

    return root;
}

static void print_sample(const acc_sample_t *s)
{
    int32_t x = s->x_mg;
    int32_t y = s->y_mg;
    int32_t z = s->z_mg;
    uint32_t norm = isqrt((uint32_t)(x * x + y * y + z * z));

    printk("x=%6d  y=%6d  z=%6d  |a|=%5u mg\n",
           (int)x, (int)y, (int)z, (unsigned int)norm);
}

int main(void)
{
    acc_config_t cfg = {
        .range_g = 3,
        .odr_hz = 70,
        .mode = ACC_MODE_LOW_NOISE,
    };
    acc_sample_t sample;
    int err;

    wait_for_terminal();

    printk("\n");
    printk("=================================\n");
    printk("  SysCare - acc_validation\n");
    printk("  Fase 3: configuracao e leitura\n");
    printk("=================================\n");

    err = srv_accelerometer_init(NULL);
    if (err) {
        while (1) {
            printk("srv_accelerometer_init: erro %d (ver o log acima)\n", err);
            k_sleep(STATUS_PERIOD);
        }
    }

    printk("Pedido:   +-%u g, %u Hz, low-noise\n", cfg.range_g, cfg.odr_hz);
    err = srv_accelerometer_config(&cfg);
    if (err) {
        printk("srv_accelerometer_config: erro %d\n", err);
    } else {
        printk("Aplicado: +-%u g, %u Hz (esperado +-4 g, 100 Hz)\n",
               cfg.range_g, cfg.odr_hz);
    }

    while (1) {
        err = srv_accelerometer_read(&sample);
        if (err == 0) {
            print_sample(&sample);
        } else if (err != -EAGAIN) {
            printk("srv_accelerometer_read: erro %d\n", err);
        }

        k_sleep(PRINT_PERIOD);
    }

    return 0;
}
