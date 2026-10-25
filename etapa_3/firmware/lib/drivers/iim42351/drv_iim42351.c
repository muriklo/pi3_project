/*
 * SysCare - driver do acelerometro IIM-42351
 *
 * O driver le a propria configuracao do devicetree (no com compatible
 * "invensense,iim42351") e confere em tempo de compilacao as premissas
 * de hardware.
 *
 * Fase 1: esqueleto do componente + mapa de registradores.
 */

#define DT_DRV_COMPAT invensense_iim42351

#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

#include "drv_iim42351_reg.h"

/* =========================================================
 * Premissas de hardware conferidas no build
 * ========================================================= */

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
             "Esperado exatamente um IIM-42351 com status okay no devicetree");

/*
 * Se o sensor e alimentado por reguladores, eles precisam iniciar ANTES
 * do driver SPI: no init, o SPI coloca o CS em nivel alto, o que
 * alimentaria o sensor desligado pelo pino.
 */
#if DT_INST_NODE_HAS_PROP(0, vdd_supply) || DT_INST_NODE_HAS_PROP(0, vddio_supply)

BUILD_ASSERT(IS_ENABLED(CONFIG_REGULATOR_FIXED),
             "vdd-supply/vddio-supply exigem CONFIG_REGULATOR=y (regulator-fixed)");

#if defined(CONFIG_REGULATOR_FIXED)
BUILD_ASSERT(CONFIG_REGULATOR_FIXED_INIT_PRIORITY < CONFIG_SPI_INIT_PRIORITY,
             "Os reguladores do sensor precisam iniciar antes do SPI: "
             "CONFIG_REGULATOR_FIXED_INIT_PRIORITY < CONFIG_SPI_INIT_PRIORITY");
#endif

#endif
