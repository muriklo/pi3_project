/*
 * SysCare - acc_validation: validacao do acelerometro IIM-42351
 * XIAO nRF52840 / Zephyr / NCS 3.4.1
 *
 * Projeto so com o acelerometro; os componentes reutilizaveis
 * (bindings, HAL, driver, servico) ficam no modulo ../lib.
 *
 * Fase 2: identificacao
 *   1. Espera o terminal abrir na USB, para os logs do init aparecerem
 *   2. Inicializa o acelerometro pelo servico (o driver loga o WHO_AM_I)
 *   3. Mostra o resultado periodicamente no console
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "srv_accelerometer.h"

#define STATUS_PERIOD   K_SECONDS(2)

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

int main(void)
{
    int err;

    wait_for_terminal();

    printk("\n");
    printk("=================================\n");
    printk("  SysCare - acc_validation\n");
    printk("  Fase 2: identificacao\n");
    printk("=================================\n");

    err = srv_accelerometer_init(NULL);

    while (1) {
        printk("srv_accelerometer_init: %d (%s)\n", err,
               err ? "falha, ver o log acima" : "OK, IIM-42351 identificado");
        k_sleep(STATUS_PERIOD);
    }

    return 0;
}
