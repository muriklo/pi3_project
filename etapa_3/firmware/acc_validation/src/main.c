/*
 * SysCare - acc_validation: validacao do acelerometro IIM-42351
 * XIAO nRF52840 / Zephyr / NCS 3.4.1
 *
 * Projeto so com o acelerometro; os componentes reutilizaveis
 * (bindings, HAL, driver, servico) ficam no modulo ../lib.
 *
 * Fase 4: leitura em buffer pela FIFO
 *   1. Espera o terminal abrir na USB, para os logs do init aparecerem
 *   2. Configura +-4 g / 500 Hz e liga a FIFO com watermark
 *   3. Captura 3 s: acorda pelo evento de watermark (pino de interrupcao)
 *      ou, sem ele, por tempo
 *   4. Mostra a taxa medida (esperado ~500 Hz) e despeja as amostras em
 *      CSV (idx,x_mg,y_mg,z_mg) entre as linhas CSV_INICIO e CSV_FIM
 */

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "srv_accelerometer.h"

#define CAPTURE_ODR_HZ      500
#define CAPTURE_SECONDS     3
#define CAPTURE_SAMPLES     (CAPTURE_ODR_HZ * CAPTURE_SECONDS)

/* 100 amostras = 200 ms a 500 Hz (a FIFO enche em ~0,5 s) */
#define WATERMARK           100

/* Sem evento de watermark, le a FIFO a cada 250 ms */
#define WAIT_TIMEOUT        K_MSEC(250)

/* Pausa entre linhas do CSV, para nao estourar o buffer do log/USB */
#define CSV_LINE_PAUSE_MS   2

#define STATUS_PERIOD       K_SECONDS(5)

static acc_sample_t capture[CAPTURE_SAMPLES];

static K_SEM_DEFINE(fifo_sem, 0, 1);

/* Chamado na workqueue do sistema quando a FIFO chega ao watermark */
static void on_acc_event(const acc_event_t *evt)
{
    if (evt->type == ACC_EVENT_FIFO_WATERMARK) {
        k_sem_give(&fifo_sem);
    }
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

static void print_csv(size_t n)
{
    printk("CSV_INICIO\n");
    printk("idx,x_mg,y_mg,z_mg\n");

    for (size_t i = 0; i < n; i++) {
        printk("%u,%d,%d,%d\n", (unsigned int)i,
               capture[i].x_mg, capture[i].y_mg, capture[i].z_mg);
        k_msleep(CSV_LINE_PAUSE_MS);
    }

    printk("CSV_FIM\n");
}

int main(void)
{
    acc_config_t cfg = {
        .range_g = 4,
        .odr_hz = CAPTURE_ODR_HZ,
        .mode = ACC_MODE_LOW_NOISE,
    };
    size_t n = 0;
    size_t n_first = 0;
    int64_t t_first = 0;
    int64_t t_last = 0;
    uint32_t by_event = 0;
    uint32_t by_timeout = 0;
    uint32_t rate_hz = 0;
    int err;

    wait_for_terminal();

    printk("\n");
    printk("=================================\n");
    printk("  SysCare - acc_validation\n");
    printk("  Fase 4: leitura pela FIFO\n");
    printk("=================================\n");

    err = srv_accelerometer_init(on_acc_event);
    if (err) {
        while (1) {
            printk("srv_accelerometer_init: erro %d (ver o log acima)\n", err);
            k_sleep(STATUS_PERIOD);
        }
    }

    err = srv_accelerometer_config(&cfg);
    printk("Config: +-%u g, %u Hz (err %d)\n", cfg.range_g, cfg.odr_hz, err);

    err = srv_accelerometer_buffer_start(WATERMARK);
    if (err) {
        while (1) {
            printk("srv_accelerometer_buffer_start: erro %d\n", err);
            k_sleep(STATUS_PERIOD);
        }
    }

    printk("Capturando %u amostras (%u s)...\n", CAPTURE_SAMPLES, CAPTURE_SECONDS);

    while (n < CAPTURE_SAMPLES) {
        size_t got = 0;

        if (k_sem_take(&fifo_sem, WAIT_TIMEOUT) == 0) {
            by_event++;
        } else {
            by_timeout++;
        }

        err = srv_accelerometer_buffer_read(&capture[n], CAPTURE_SAMPLES - n, &got);
        if (err) {
            printk("srv_accelerometer_buffer_read: erro %d\n", err);
            break;
        }

        if (got == 0) {
            continue;
        }

        /* A taxa e medida a partir da 1a leitura, que esvazia a FIFO */
        if (t_first == 0) {
            t_first = k_uptime_get();
            n_first = n + got;
        }

        n += got;
        t_last = k_uptime_get();
    }

    srv_accelerometer_buffer_stop();

    if (t_last > t_first) {
        rate_hz = (uint32_t)(((n - n_first) * 1000) / (size_t)(t_last - t_first));
    }

    print_csv(n);

    while (1) {
        printk("Capturadas %u amostras; taxa medida %u Hz (esperado ~%u)\n",
               (unsigned int)n, (unsigned int)rate_hz, CAPTURE_ODR_HZ);
        printk("Leituras acordadas por evento: %u, por tempo: %u\n",
               (unsigned int)by_event, (unsigned int)by_timeout);
        k_sleep(STATUS_PERIOD);
    }

    return 0;
}
