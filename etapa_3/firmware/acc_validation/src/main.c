/*
 * SysCare - acc_validation: validacao do acelerometro IIM-42351
 * XIAO nRF52840 / Zephyr / NCS 3.4.1
 *
 * Projeto so com o acelerometro; os componentes reutilizaveis
 * (bindings, HAL, driver, servico) ficam no modulo ../lib.
 *
 * Fase 0: devicetree
 *   1. Le do devicetree o no do acelerometro (SPI, CS, INT1, INT2,
 *      config inicial) e os reguladores que o alimentam
 *   2. Confere se o barramento SPI, os GPIOs e os reguladores estao prontos
 *   3. Mostra tudo periodicamente no console
 *
 * Nenhum pino e escrito aqui: tudo vem do app.overlay.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/printk.h>

#define STATUS_PERIOD   K_SECONDS(2)

/* =========================================================
 * Acelerometro no devicetree
 * ========================================================= */

#define ACC_NODE        DT_NODELABEL(accel)

#if !DT_NODE_HAS_STATUS_OKAY(ACC_NODE)
#error "No 'accel' nao encontrado no devicetree (app.overlay)"
#endif

#define ACC_VDD_NODE    DT_PHANDLE(ACC_NODE, vdd_supply)
#define ACC_VDDIO_NODE  DT_PHANDLE(ACC_NODE, vddio_supply)

/* Porta do nRF52840 (0 -> P0.xx, 1 -> P1.xx) do controlador GPIO */
#define PORT_OF(ctlr)   DT_PROP(ctlr, port)

static const struct spi_dt_spec acc_spi =
    SPI_DT_SPEC_GET(ACC_NODE, SPI_OP_MODE_MASTER | SPI_WORD_SET(8));

static const struct gpio_dt_spec acc_int1 = GPIO_DT_SPEC_GET(ACC_NODE, int1_gpios);
static const struct gpio_dt_spec acc_int2 = GPIO_DT_SPEC_GET(ACC_NODE, int2_gpios);

static const struct device *const acc_vdd = DEVICE_DT_GET(ACC_VDD_NODE);
static const struct device *const acc_vddio = DEVICE_DT_GET(ACC_VDDIO_NODE);

/* =========================================================
 * Impressao
 * ========================================================= */

static void print_spi(void)
{
    const struct spi_config *cfg = &acc_spi.config;
    uint32_t mode = SPI_MODE_GET(cfg->operation);
    unsigned int spi_mode = ((mode & SPI_MODE_CPOL) ? 2U : 0U) |
                            ((mode & SPI_MODE_CPHA) ? 1U : 0U);

    printk("Barramento SPI (%s): %s\n", acc_spi.bus->name,
           spi_is_ready_dt(&acc_spi) ? "pronto" : "NAO pronto");

    printk("  CS    P%u.%02u, %u Hz, modo %u\n",
           (unsigned int)PORT_OF(DT_SPI_DEV_CS_GPIOS_CTLR(ACC_NODE)),
           (unsigned int)cfg->cs.gpio.pin,
           (unsigned int)cfg->frequency, spi_mode);
}

static void print_int(const char *name, const struct gpio_dt_spec *spec,
                      unsigned int port)
{
    printk("  %-5s P%u.%02u: %s\n", name, port, (unsigned int)spec->pin,
           gpio_is_ready_dt(spec) ? "pronto" : "NAO pronto");
}

/*
 * regulator_is_enabled() responde pela contagem de uso do regulador:
 * com regulator-boot-on ele ja comeca em 1 (ligado no boot).
 */
static void print_regulator(const char *name, const struct device *reg,
                            unsigned int port, unsigned int pin)
{
    const char *state;

    if (!device_is_ready(reg)) {
        state = "NAO pronto";
    } else {
        state = regulator_is_enabled(reg) ? "ligado" : "desligado";
    }

    printk("  %-5s P%u.%02u: %s\n", name, port, pin, state);
}

static void print_status(void)
{
    printk("\n");
    printk("=================================\n");
    printk("  SysCare - Teste IIM-42351\n");
    printk("  Fase 0: devicetree\n");
    printk("=================================\n");

    print_spi();

    printk("Interrupcoes:\n");
    print_int("INT1", &acc_int1,
              PORT_OF(DT_GPIO_CTLR(ACC_NODE, int1_gpios)));
    print_int("INT2", &acc_int2,
              PORT_OF(DT_GPIO_CTLR(ACC_NODE, int2_gpios)));

    printk("Alimentacao:\n");
    print_regulator("VDD", acc_vdd,
                    PORT_OF(DT_GPIO_CTLR(ACC_VDD_NODE, enable_gpios)),
                    DT_GPIO_PIN(ACC_VDD_NODE, enable_gpios));
    print_regulator("VDDIO", acc_vddio,
                    PORT_OF(DT_GPIO_CTLR(ACC_VDDIO_NODE, enable_gpios)),
                    DT_GPIO_PIN(ACC_VDDIO_NODE, enable_gpios));

    printk("Config inicial: %u Hz, +-%u g, %s\n",
           (unsigned int)DT_PROP(ACC_NODE, accel_hz),
           (unsigned int)DT_PROP(ACC_NODE, accel_fs),
           DT_PROP(ACC_NODE, accel_mode));
}

/* =========================================================
 * Main
 * ========================================================= */

int main(void)
{
    /*
     * O console e USB (CDC ACM): mensagens enviadas antes de o
     * terminal abrir se perdem. Por isso o estado e repetido.
     */
    while (1) {
        print_status();
        k_sleep(STATUS_PERIOD);
    }

    return 0;
}
