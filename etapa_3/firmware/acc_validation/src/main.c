/*
 * SysCare - acc_validation: validacao do acelerometro IIM-42351
 * XIAO nRF52840 / Zephyr / NCS 3.4.1
 *
 * Projeto so com o acelerometro; os componentes reutilizaveis
 * (bindings, HAL, driver, servico) ficam no modulo ../lib.
 *
 * Fase 5: deteccao de queda livre
 *   1. Espera o terminal abrir na USB, para os logs do init aparecerem
 *   2. Liga a deteccao de queda livre e mostra os parametros aplicados
 *   3. A cada queda, mostra a duracao e a altura estimada
 *
 * Teste: soltar a placa sobre uma almofada de alturas conhecidas
 * (20 cm, 50 cm, 1 m) e comparar; sacudir, balancar o braco e bater na
 * mesa nao devem gerar eventos.
 */

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "srv_accelerometer.h"

#define STATUS_PERIOD   K_SECONDS(10)

/* Eventos vindos do callback (workqueue do sistema) para o main */
K_MSGQ_DEFINE(event_q, sizeof(acc_event_t), 8, 4);

static void on_acc_event(const acc_event_t *evt)
{
    (void)k_msgq_put(&event_q, evt, K_NO_WAIT);
}

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
    acc_freefall_config_t ff = {
        .lowg_mg = 500,
        .highg_mg = 3000,
        .min_height_cm = 13,
        .max_height_cm = 201,
        .debounce_ms = 1000,
    };
    acc_event_t evt;
    uint32_t falls = 0;
    int err;

    wait_for_terminal();

    printk("\n");
    printk("=================================\n");
    printk("  SysCare - acc_validation\n");
    printk("  Fase 5: queda livre\n");
    printk("=================================\n");

    err = srv_accelerometer_init(on_acc_event);
    if (err) {
        while (1) {
            printk("srv_accelerometer_init: erro %d (ver o log acima)\n", err);
            k_sleep(STATUS_PERIOD);
        }
    }

    printk("Pedido:   low-g %u mg, high-g %u mg, %u a %u cm, debounce %u ms\n",
           ff.lowg_mg, ff.highg_mg, ff.min_height_cm, ff.max_height_cm, ff.debounce_ms);

    err = srv_accelerometer_freefall_enable(&ff);
    if (err) {
        while (1) {
            printk("srv_accelerometer_freefall_enable: erro %d\n", err);
            k_sleep(STATUS_PERIOD);
        }
    }

    printk("Aplicado: low-g %u mg, high-g %u mg, %u a %u cm, debounce %u ms\n",
           ff.lowg_mg, ff.highg_mg, ff.min_height_cm, ff.max_height_cm, ff.debounce_ms);
    printk("Aguardando quedas...\n");

    while (1) {
        if (k_msgq_get(&event_q, &evt, STATUS_PERIOD) != 0) {
            printk("(aguardando quedas; %u detectadas ate agora)\n", (unsigned int)falls);
            continue;
        }

        if (evt.type == ACC_EVENT_FREEFALL) {
            falls++;
            printk(">>> QUEDA LIVRE #%u: duracao %u ms, altura estimada ~%u cm\n",
                   (unsigned int)falls, evt.duration_ms, evt.height_cm);
        }
    }

    return 0;
}
