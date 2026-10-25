/*
 * SysCare - acc_validation: validacao do acelerometro IIM-42351
 * XIAO nRF52840 / Zephyr / NCS 3.4.1
 *
 * Projeto so com o acelerometro; os componentes reutilizaveis
 * (bindings, HAL, driver, servico) ficam no modulo ../lib.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STATUS_PERIOD   K_SECONDS(2)

int main(void)
{
    /*
     * O console e USB (CDC ACM): mensagens enviadas antes de o
     * terminal abrir se perdem. Por isso a mensagem e repetida.
     */
    while (1) {
        printk("SysCare - acc_validation\n");
        k_sleep(STATUS_PERIOD);
    }

    return 0;
}
