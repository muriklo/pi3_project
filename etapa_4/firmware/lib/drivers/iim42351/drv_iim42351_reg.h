/*
 * SysCare - mapa de registradores do acelerometro IIM-42351
 *
 * Fonte: datasheet DS-000441 v1.4 (acc_validation/docs/IIM-42351-TYP-v1.4.pdf)
 *   - secao 13: mapa de registradores;
 *   - secoes 14 a 18: descricao de cada registrador (citadas abaixo);
 *   - secao 6: FIFO; secao 3: especificacoes eletricas.
 *
 * Estilo inspirado no icm42605_reg.h do Zephyr, mas reescrito para o
 * IIM-42351: nao ha giroscopio e varios bits diferem do ICM-42605.
 *
 * Convencoes:
 *   - IIM42351_REG_<NOME>: registrador. O banco fica nos bits 15:8 e o
 *     endereco nos bits 7:0 (IIM42351_REG(banco, endereco)); a camada de
 *     acesso troca o banco (REG_BANK_SEL) so quando ele muda.
 *   - IIM42351_<CAMPO>: mascara do campo, para FIELD_PREP()/FIELD_GET().
 *   - IIM42351_<CAMPO>_<VALOR>: valores do campo, ainda sem deslocamento
 *     (FIELD_PREP() posiciona).
 *
 * Campos reservados devem manter o valor de reset (secao 13). Os
 * registradores marcados [RMW] tem bits reservados diferentes de zero no
 * reset: escrever sempre com leitura-modificacao-escrita.
 *
 * Fora deste mapa, por nao serem usados no projeto: registradores so de
 * I3C/IBI (INTF_CONFIG6, INT_SOURCE8 a INT_SOURCE10) e pull-up/pull-down
 * dos pinos (banco 3).
 */

#ifndef DRV_IIM42351_REG_H_
#define DRV_IIM42351_REG_H_

#include <zephyr/sys/util.h>

/* =========================================================
 * Enderecamento por banco e protocolo SPI
 * ========================================================= */

#define IIM42351_REG(bank, addr)    (((bank) << 8) | (addr))
#define IIM42351_REG_BANK(reg)      ((uint8_t)((reg) >> 8))
#define IIM42351_REG_ADDR(reg)      ((uint8_t)((reg) & 0xFF))

/* Bit 7 do byte de endereco: 1 = leitura, 0 = escrita */
#define IIM42351_SPI_READ           BIT(7)

/* =========================================================
 * Banco 0
 * ========================================================= */

/* DEVICE_CONFIG (14.1) */
#define IIM42351_REG_DEVICE_CONFIG              IIM42351_REG(0, 0x11)
#define IIM42351_SPI_MODE                       BIT(4)  /* 0: modos 0 e 3 (padrao); 1: modos 1 e 2 */
#define IIM42351_SOFT_RESET_CONFIG              BIT(0)  /* 1: reset (esperar IIM42351_SOFT_RESET_MS) */

/* DRIVE_CONFIG (14.2) */
#define IIM42351_REG_DRIVE_CONFIG               IIM42351_REG(0, 0x13)
#define IIM42351_I2C_SLEW_RATE                  GENMASK(5, 3)
#define IIM42351_SPI_SLEW_RATE                  GENMASK(2, 0)

/* INT_CONFIG (14.3) */
#define IIM42351_REG_INT_CONFIG                 IIM42351_REG(0, 0x14)
#define IIM42351_INT2_MODE                      BIT(5)  /* 0: pulsado; 1: latched */
#define IIM42351_INT2_DRIVE_CIRCUIT             BIT(4)  /* 0: open drain; 1: push-pull */
#define IIM42351_INT2_POLARITY                  BIT(3)  /* 0: ativo baixo; 1: ativo alto */
#define IIM42351_INT1_MODE                      BIT(2)
#define IIM42351_INT1_DRIVE_CIRCUIT             BIT(1)
#define IIM42351_INT1_POLARITY                  BIT(0)

/* FIFO_CONFIG (14.4) */
#define IIM42351_REG_FIFO_CONFIG                IIM42351_REG(0, 0x16)
#define IIM42351_FIFO_MODE                      GENMASK(7, 6)
#define IIM42351_FIFO_MODE_BYPASS               0
#define IIM42351_FIFO_MODE_STREAM               1
#define IIM42351_FIFO_MODE_STOP_ON_FULL         2

/* Dados do sensor (14.5 a 14.12): big-endian por padrao (SENSOR_DATA_ENDIAN) */
#define IIM42351_REG_TEMP_DATA1                 IIM42351_REG(0, 0x1D)
#define IIM42351_REG_TEMP_DATA0                 IIM42351_REG(0, 0x1E)
#define IIM42351_REG_ACCEL_DATA_X1              IIM42351_REG(0, 0x1F)
#define IIM42351_REG_ACCEL_DATA_X0              IIM42351_REG(0, 0x20)
#define IIM42351_REG_ACCEL_DATA_Y1              IIM42351_REG(0, 0x21)
#define IIM42351_REG_ACCEL_DATA_Y0              IIM42351_REG(0, 0x22)
#define IIM42351_REG_ACCEL_DATA_Z1              IIM42351_REG(0, 0x23)
#define IIM42351_REG_ACCEL_DATA_Z0              IIM42351_REG(0, 0x24)

/* TMST_FSYNC (14.13 e 14.14) */
#define IIM42351_REG_TMST_FSYNCH                IIM42351_REG(0, 0x2B)
#define IIM42351_REG_TMST_FSYNCL                IIM42351_REG(0, 0x2C)

/* INT_STATUS (14.15): cada bit limpa na leitura */
#define IIM42351_REG_INT_STATUS                 IIM42351_REG(0, 0x2D)
#define IIM42351_UI_FSYNC_INT                   BIT(6)
#define IIM42351_RESET_DONE_INT                 BIT(4)
#define IIM42351_DATA_RDY_INT                   BIT(3)
#define IIM42351_FIFO_THS_INT                   BIT(2)
#define IIM42351_FIFO_FULL_INT                  BIT(1)
#define IIM42351_AGC_RDY_INT                    BIT(0)

/*
 * FIFO_COUNT (14.16 e 14.17): contagem em bytes ou registros
 * (FIFO_COUNT_REC). Ler o FIFO_COUNTL trava os 16 bits.
 */
#define IIM42351_REG_FIFO_COUNTH                IIM42351_REG(0, 0x2E)
#define IIM42351_REG_FIFO_COUNTL                IIM42351_REG(0, 0x2F)

/* FIFO_DATA (14.18) */
#define IIM42351_REG_FIFO_DATA                  IIM42351_REG(0, 0x30)

/* APEX_DATA (14.19 a 14.24) */
#define IIM42351_REG_APEX_DATA0                 IIM42351_REG(0, 0x31)  /* STEP_CNT[7:0]  / FF_DUR[7:0]  */
#define IIM42351_REG_APEX_DATA1                 IIM42351_REG(0, 0x32)  /* STEP_CNT[15:8] / FF_DUR[15:8] */
#define IIM42351_REG_APEX_DATA2                 IIM42351_REG(0, 0x33)  /* STEP_CADENCE (u6.2) */
#define IIM42351_REG_APEX_DATA3                 IIM42351_REG(0, 0x34)
#define IIM42351_DMP_IDLE                       BIT(2)
#define IIM42351_ACTIVITY_CLASS                 GENMASK(1, 0)
#define IIM42351_REG_APEX_DATA4                 IIM42351_REG(0, 0x35)
#define IIM42351_TAP_NUM                        GENMASK(4, 3)
#define IIM42351_TAP_AXIS                       GENMASK(2, 1)
#define IIM42351_TAP_DIR                        BIT(0)
#define IIM42351_REG_APEX_DATA5                 IIM42351_REG(0, 0x36)
#define IIM42351_DOUBLE_TAP_TIMING              GENMASK(5, 0)

/* INT_STATUS2 (14.25): cada bit limpa na leitura */
#define IIM42351_REG_INT_STATUS2                IIM42351_REG(0, 0x37)
#define IIM42351_SMD_INT                        BIT(3)
#define IIM42351_WOM_Z_INT                      BIT(2)
#define IIM42351_WOM_Y_INT                      BIT(1)
#define IIM42351_WOM_X_INT                      BIT(0)

/* INT_STATUS3 (14.26): cada bit limpa na leitura */
#define IIM42351_REG_INT_STATUS3                IIM42351_REG(0, 0x38)
#define IIM42351_STEP_DET_INT                   BIT(5)
#define IIM42351_STEP_CNT_OVF_INT               BIT(4)
#define IIM42351_TILT_DET_INT                   BIT(3)
#define IIM42351_FF_DET_INT                     BIT(1)
#define IIM42351_TAP_DET_INT                    BIT(0)

/* SIGNAL_PATH_RESET (14.27): bits de escrita que se limpam sozinhos */
#define IIM42351_REG_SIGNAL_PATH_RESET          IIM42351_REG(0, 0x4B)
#define IIM42351_DMP_INIT_EN                    BIT(6)
#define IIM42351_DMP_MEM_RESET_EN               BIT(5)
#define IIM42351_ABORT_AND_RESET                BIT(3)
#define IIM42351_TMST_STROBE                    BIT(2)
#define IIM42351_FIFO_FLUSH                     BIT(1)

/* INTF_CONFIG0 (14.28), reset 0x30 */
#define IIM42351_REG_INTF_CONFIG0               IIM42351_REG(0, 0x4C)
#define IIM42351_FIFO_HOLD_LAST_DATA_EN         BIT(7)
#define IIM42351_FIFO_COUNT_REC                 BIT(6)  /* 0: bytes; 1: registros */
#define IIM42351_FIFO_COUNT_ENDIAN              BIT(5)  /* 1: big-endian (padrao) */
#define IIM42351_SENSOR_DATA_ENDIAN             BIT(4)  /* 1: big-endian (padrao) */
#define IIM42351_UI_SIFS_CFG                    GENMASK(1, 0)
#define IIM42351_UI_SIFS_CFG_DISABLE_SPI        2
#define IIM42351_UI_SIFS_CFG_DISABLE_I2C        3

/*
 * INTF_CONFIG1 (14.29), reset 0x90 [RMW]
 * Diferente do ICM-42605: aqui nao ha campos de AFSR nem de PLL.
 */
#define IIM42351_REG_INTF_CONFIG1               IIM42351_REG(0, 0x4D)
#define IIM42351_ACCEL_LP_CLK_SEL               BIT(3)  /* 0: oscilador wake-up; 1: oscilador RC */
#define IIM42351_RTC_MODE                       BIT(2)
#define IIM42351_CLKSEL                         GENMASK(1, 0)
#define IIM42351_CLKSEL_RC                      0
#define IIM42351_CLKSEL_DISABLE_ALL             3

/* PWR_MGMT0 (14.30): depois de sair de OFF, nao escrever por IIM42351_ACCEL_START_US */
#define IIM42351_REG_PWR_MGMT0                  IIM42351_REG(0, 0x4E)
#define IIM42351_TEMP_DIS                       BIT(5)
#define IIM42351_IDLE                           BIT(4)
#define IIM42351_ACCEL_MODE                     GENMASK(1, 0)
#define IIM42351_ACCEL_MODE_OFF                 0
#define IIM42351_ACCEL_MODE_LP                  2
#define IIM42351_ACCEL_MODE_LN                  3

/*
 * ACCEL_CONFIG0 (14.31)
 * O datasheet da reset 0x03 (+-16 g, 8 kHz), mas a tabela do ACCEL_ODR
 * marca 1 kHz como padrao: o driver sempre escreve o ODR explicitamente.
 */
#define IIM42351_REG_ACCEL_CONFIG0              IIM42351_REG(0, 0x50)
#define IIM42351_ACCEL_FS_SEL                   GENMASK(7, 5)   /* 4 a 7: reservados */
#define IIM42351_ACCEL_FS_SEL_16G               0
#define IIM42351_ACCEL_FS_SEL_8G                1
#define IIM42351_ACCEL_FS_SEL_4G                2
#define IIM42351_ACCEL_FS_SEL_2G                3
#define IIM42351_ACCEL_ODR                      GENMASK(3, 0)   /* 0 a 2: reservados */
#define IIM42351_ACCEL_ODR_8000HZ               0x3     /* so LN */
#define IIM42351_ACCEL_ODR_4000HZ               0x4     /* so LN */
#define IIM42351_ACCEL_ODR_2000HZ               0x5     /* so LN */
#define IIM42351_ACCEL_ODR_1000HZ               0x6     /* so LN */
#define IIM42351_ACCEL_ODR_200HZ                0x7
#define IIM42351_ACCEL_ODR_100HZ                0x8
#define IIM42351_ACCEL_ODR_50HZ                 0x9
#define IIM42351_ACCEL_ODR_25HZ                 0xA
#define IIM42351_ACCEL_ODR_12_5HZ               0xB
#define IIM42351_ACCEL_ODR_6_25HZ               0xC     /* so LP */
#define IIM42351_ACCEL_ODR_3_125HZ              0xD     /* so LP */
#define IIM42351_ACCEL_ODR_1_5625HZ             0xE     /* so LP */
#define IIM42351_ACCEL_ODR_500HZ                0xF

/* TEMP_FILT_CONFIG (14.32), reset 0x16 [RMW] */
#define IIM42351_REG_TEMP_FILT_CONFIG           IIM42351_REG(0, 0x51)
#define IIM42351_TEMP_FILT_BW                   GENMASK(7, 5)

/*
 * ACCEL_FILT_CONFIG (14.33), reset 0x11 [RMW]
 * No ICM-42605 o endereco 0x52 e o GYRO_ACCEL_CONFIG0; aqui so tem o acelerometro.
 */
#define IIM42351_REG_ACCEL_FILT_CONFIG          IIM42351_REG(0, 0x52)
#define IIM42351_ACCEL_UI_FILT_BW               GENMASK(7, 4)
/* Modo LN: largura de banda do filtro passa-baixas */
#define IIM42351_ACCEL_UI_FILT_BW_LN_ODR_DIV2   0       /* BW = ODR/2 */
#define IIM42351_ACCEL_UI_FILT_BW_LN_DIV4       1       /* BW = max(400 Hz, ODR)/4 (padrao) */
#define IIM42351_ACCEL_UI_FILT_BW_LN_DIV5       2
#define IIM42351_ACCEL_UI_FILT_BW_LN_DIV8       3
#define IIM42351_ACCEL_UI_FILT_BW_LN_DIV10      4
#define IIM42351_ACCEL_UI_FILT_BW_LN_DIV16      5
#define IIM42351_ACCEL_UI_FILT_BW_LN_DIV20      6
#define IIM42351_ACCEL_UI_FILT_BW_LN_DIV40      7
#define IIM42351_ACCEL_UI_FILT_BW_LN_LOW_LAT1   14      /* dec2 a max(400 Hz, ODR) */
#define IIM42351_ACCEL_UI_FILT_BW_LN_LOW_LAT2   15      /* dec2 a max(200 Hz, 8*ODR) */
/* Modo LP: filtro de media */
#define IIM42351_ACCEL_UI_FILT_BW_LP_AVG_1X     1       /* padrao */
#define IIM42351_ACCEL_UI_FILT_BW_LP_AVG_16X    6

/* ACCEL_CONFIG1 (14.34), reset 0x0D [RMW] */
#define IIM42351_REG_ACCEL_CONFIG1              IIM42351_REG(0, 0x53)
#define IIM42351_ACCEL_UI_FILT_ORD              GENMASK(4, 3)   /* 0: 1a; 1: 2a; 2: 3a ordem */
#define IIM42351_ACCEL_DEC2_M2_ORD              GENMASK(2, 1)   /* 2: 3a ordem (unico valido) */

/* TMST_CONFIG (14.35), reset 0x23 [RMW] */
#define IIM42351_REG_TMST_CONFIG                IIM42351_REG(0, 0x54)
#define IIM42351_TMST_TO_REGS_EN                BIT(4)
#define IIM42351_TMST_RES                       BIT(3)  /* 0: 1 us; 1: 16 us */
#define IIM42351_TMST_DELTA_EN                  BIT(2)
#define IIM42351_TMST_FSYNC_EN                  BIT(1)
#define IIM42351_TMST_EN                        BIT(0)

/*
 * APEX_CONFIG0 (14.36), reset 0x82: DMP_POWER_SAVE vem ligado (exigiria
 * Wake on Motion para acordar o DMP; secao 8.2).
 */
#define IIM42351_REG_APEX_CONFIG0               IIM42351_REG(0, 0x56)
#define IIM42351_DMP_POWER_SAVE                 BIT(7)
#define IIM42351_TAP_ENABLE                     BIT(6)
#define IIM42351_PED_ENABLE                     BIT(5)
#define IIM42351_TILT_ENABLE                    BIT(4)
#define IIM42351_FF_ENABLE                      BIT(2)
#define IIM42351_DMP_ODR                        GENMASK(1, 0)
#define IIM42351_DMP_ODR_25HZ                   0
#define IIM42351_DMP_ODR_500HZ                  1
#define IIM42351_DMP_ODR_50HZ                   2
#define IIM42351_DMP_ODR_100HZ                  3

/* SMD_CONFIG (14.37) */
#define IIM42351_REG_SMD_CONFIG                 IIM42351_REG(0, 0x57)
#define IIM42351_WOM_INT_MODE                   BIT(3)  /* 0: OR dos eixos; 1: AND */
#define IIM42351_WOM_MODE                       BIT(2)  /* 0: compara com a 1a amostra; 1: com a anterior */
#define IIM42351_SMD_MODE                       GENMASK(1, 0)
#define IIM42351_SMD_MODE_DISABLED              0
#define IIM42351_SMD_MODE_WOM                   1
#define IIM42351_SMD_MODE_SHORT                 2
#define IIM42351_SMD_MODE_LONG                  3

/*
 * FIFO_CONFIG1 (14.38 e 6.4)
 * A secao 14.38 chama o bit 0 de FIFO_ACCEL_EN e o bit 1 de reservado;
 * a secao 13.1 e a tabela de pacotes da 6.4 definem os bits 1:0 como
 * FIFO_PACKET_EN. Seguimos a 6.4. Para o pacote 1 as duas dao 0b01.
 */
#define IIM42351_REG_FIFO_CONFIG1               IIM42351_REG(0, 0x5F)
#define IIM42351_FIFO_RESUME_PARTIAL_RD         BIT(6)
#define IIM42351_FIFO_WM_GT_TH                  BIT(5)
#define IIM42351_FIFO_HIRES_EN                  BIT(4)
#define IIM42351_FIFO_TMST_FSYNC_EN             BIT(3)
#define IIM42351_FIFO_TEMP_EN                   BIT(2)
#define IIM42351_FIFO_PACKET_EN                 GENMASK(1, 0)
#define IIM42351_FIFO_PACKET_EN_NONE            0
#define IIM42351_FIFO_PACKET_EN_P1              1       /* acel + temp, 8 bytes */
#define IIM42351_FIFO_PACKET_EN_P2_P3           3       /* 16 bytes, ou 20 com FIFO_HIRES_EN */

/* FIFO_CONFIG2/3 (14.39 e 14.40): watermark de 12 bits; nunca 0 */
#define IIM42351_REG_FIFO_CONFIG2               IIM42351_REG(0, 0x60)   /* FIFO_WM[7:0] */
#define IIM42351_REG_FIFO_CONFIG3               IIM42351_REG(0, 0x61)
#define IIM42351_FIFO_WM_HI                     GENMASK(3, 0)           /* FIFO_WM[11:8] */

/* FSYNC_CONFIG (14.41), reset 0x10 */
#define IIM42351_REG_FSYNC_CONFIG               IIM42351_REG(0, 0x62)
#define IIM42351_FSYNC_UI_SEL                   GENMASK(6, 4)
#define IIM42351_FSYNC_UI_FLAG_CLEAR_SEL        BIT(1)
#define IIM42351_FSYNC_POLARITY                 BIT(0)

/* INT_CONFIG0 (14.42): como limpar cada interrupcao no modo latched */
#define IIM42351_REG_INT_CONFIG0                IIM42351_REG(0, 0x63)
#define IIM42351_UI_DRDY_INT_CLEAR              GENMASK(5, 4)
#define IIM42351_FIFO_THS_INT_CLEAR             GENMASK(3, 2)
#define IIM42351_FIFO_FULL_INT_CLEAR            GENMASK(1, 0)
#define IIM42351_INT_CLEAR_ON_STATUS            0       /* ao ler o status (padrao) */
#define IIM42351_INT_CLEAR_ON_DATA              2       /* ao ler o dado */
#define IIM42351_INT_CLEAR_ON_STATUS_AND_DATA   3

/*
 * INT_CONFIG1 (14.43), reset 0x10: o datasheet manda zerar o
 * INT_ASYNC_RESET para os pinos INT1/INT2 funcionarem direito.
 */
#define IIM42351_REG_INT_CONFIG1                IIM42351_REG(0, 0x64)
#define IIM42351_INT_TPULSE_DURATION            BIT(6)  /* 0: 100 us; 1: 8 us (exigido se ODR >= 4 kHz) */
#define IIM42351_INT_TDEASSERT_DISABLE          BIT(5)
#define IIM42351_INT_ASYNC_RESET                BIT(4)

/* INT_SOURCE0 (14.44): fontes roteadas para o INT1, reset 0x10 */
#define IIM42351_REG_INT_SOURCE0                IIM42351_REG(0, 0x65)
#define IIM42351_UI_FSYNC_INT1_EN               BIT(6)
#define IIM42351_RESET_DONE_INT1_EN             BIT(4)
#define IIM42351_UI_DRDY_INT1_EN                BIT(3)
#define IIM42351_FIFO_THS_INT1_EN               BIT(2)
#define IIM42351_FIFO_FULL_INT1_EN              BIT(1)
#define IIM42351_UI_AGC_RDY_INT1_EN             BIT(0)

/* INT_SOURCE1 (14.45): fontes roteadas para o INT1 */
#define IIM42351_REG_INT_SOURCE1                IIM42351_REG(0, 0x66)
#define IIM42351_I3C_PROTOCOL_ERROR_INT1_EN     BIT(6)
#define IIM42351_SMD_INT1_EN                    BIT(3)
#define IIM42351_WOM_Z_INT1_EN                  BIT(2)
#define IIM42351_WOM_Y_INT1_EN                  BIT(1)
#define IIM42351_WOM_X_INT1_EN                  BIT(0)

/* INT_SOURCE3 (14.46): fontes roteadas para o INT2 */
#define IIM42351_REG_INT_SOURCE3                IIM42351_REG(0, 0x68)
#define IIM42351_UI_FSYNC_INT2_EN               BIT(6)
#define IIM42351_RESET_DONE_INT2_EN             BIT(4)
#define IIM42351_UI_DRDY_INT2_EN                BIT(3)
#define IIM42351_FIFO_THS_INT2_EN               BIT(2)
#define IIM42351_FIFO_FULL_INT2_EN              BIT(1)
#define IIM42351_UI_AGC_RDY_INT2_EN             BIT(0)

/* INT_SOURCE4 (14.47): fontes roteadas para o INT2 */
#define IIM42351_REG_INT_SOURCE4                IIM42351_REG(0, 0x69)
#define IIM42351_I3C_PROTOCOL_ERROR_INT2_EN     BIT(6)
#define IIM42351_SMD_INT2_EN                    BIT(3)
#define IIM42351_WOM_Z_INT2_EN                  BIT(2)
#define IIM42351_WOM_Y_INT2_EN                  BIT(1)
#define IIM42351_WOM_X_INT2_EN                  BIT(0)

/* FIFO_LOST_PKT (14.48 e 14.49): o byte baixo vem primeiro */
#define IIM42351_REG_FIFO_LOST_PKT0             IIM42351_REG(0, 0x6C)   /* [7:0]  */
#define IIM42351_REG_FIFO_LOST_PKT1             IIM42351_REG(0, 0x6D)   /* [15:8] */

/* SELF_TEST_CONFIG (14.50) */
#define IIM42351_REG_SELF_TEST_CONFIG           IIM42351_REG(0, 0x70)
#define IIM42351_ACCEL_ST_POWER                 BIT(6)
#define IIM42351_EN_AZ_ST                       BIT(5)
#define IIM42351_EN_AY_ST                       BIT(4)
#define IIM42351_EN_AX_ST                       BIT(3)

/* WHO_AM_I (14.51) */
#define IIM42351_REG_WHO_AM_I                   IIM42351_REG(0, 0x75)
#define IIM42351_WHO_AM_I_VALUE                 0x6C

/*
 * REG_BANK_SEL (14.52): acessivel de qualquer banco, por isso e
 * definido so pelo endereco (sem banco).
 */
#define IIM42351_REG_BANK_SEL_ADDR              0x76
#define IIM42351_BANK_SEL                       GENMASK(2, 0)

/* =========================================================
 * Banco 1
 * ========================================================= */

/* SENSOR_CONFIG0 (15.1), reset 0xB8 [RMW] */
#define IIM42351_REG_SENSOR_CONFIG0             IIM42351_REG(1, 0x03)
#define IIM42351_ZA_DISABLE                     BIT(2)
#define IIM42351_YA_DISABLE                     BIT(1)
#define IIM42351_XA_DISABLE                     BIT(0)

/* TMSTVAL (15.2 a 15.4): contador de 20 bits travado pelo TMST_STROBE */
#define IIM42351_REG_TMSTVAL0                   IIM42351_REG(1, 0x62)   /* [7:0]   */
#define IIM42351_REG_TMSTVAL1                   IIM42351_REG(1, 0x63)   /* [15:8]  */
#define IIM42351_REG_TMSTVAL2                   IIM42351_REG(1, 0x64)
#define IIM42351_TMST_VALUE_HI                  GENMASK(3, 0)           /* [19:16] */

/* INTF_CONFIG4 (15.5), reset 0x03 [RMW] */
#define IIM42351_REG_INTF_CONFIG4               IIM42351_REG(1, 0x7A)
#define IIM42351_I3C_BUS_MODE                   BIT(6)
#define IIM42351_SPI_AP_4WIRE                   BIT(1)  /* 1: SPI de 4 fios (padrao) */

/* INTF_CONFIG5 (15.6), reset 0x20 [RMW]: o pino 9 precisa ficar como INT2 */
#define IIM42351_REG_INTF_CONFIG5               IIM42351_REG(1, 0x7B)
#define IIM42351_PIN9_FUNCTION                  GENMASK(2, 1)
#define IIM42351_PIN9_FUNCTION_INT2             0
#define IIM42351_PIN9_FUNCTION_FSYNC            1
#define IIM42351_PIN9_FUNCTION_CLKIN            2

/* =========================================================
 * Banco 2
 * ========================================================= */

/* ACCEL_CONFIG_STATIC2 a 4 (16.1 a 16.3): filtro anti-aliasing (secao 5.2) */
#define IIM42351_REG_ACCEL_CONFIG_STATIC2       IIM42351_REG(2, 0x03)   /* reset 0x7E */
#define IIM42351_ACCEL_AAF_DELT                 GENMASK(6, 1)
#define IIM42351_ACCEL_AAF_DIS                  BIT(0)
#define IIM42351_REG_ACCEL_CONFIG_STATIC3       IIM42351_REG(2, 0x04)   /* ACCEL_AAF_DELTSQR[7:0] */
#define IIM42351_REG_ACCEL_CONFIG_STATIC4       IIM42351_REG(2, 0x05)   /* reset 0x3F */
#define IIM42351_ACCEL_AAF_BITSHIFT             GENMASK(7, 4)
#define IIM42351_ACCEL_AAF_DELTSQR_HI           GENMASK(3, 0)           /* ACCEL_AAF_DELTSQR[11:8] */

/* XA/YA/ZA_ST_DATA (16.4 a 16.6): resultado do self-test de fabrica */
#define IIM42351_REG_XA_ST_DATA                 IIM42351_REG(2, 0x3B)
#define IIM42351_REG_YA_ST_DATA                 IIM42351_REG(2, 0x3C)
#define IIM42351_REG_ZA_ST_DATA                 IIM42351_REG(2, 0x3D)

/* =========================================================
 * Banco 4
 * ========================================================= */

/* FDR_CONFIG (18.1): decimacao dos pacotes da FIFO */
#define IIM42351_REG_FDR_CONFIG                 IIM42351_REG(4, 0x09)
#define IIM42351_FDR_SEL                        GENMASK(6, 0)   /* n: 1 pacote a cada n + 1 */

/* APEX_CONFIG1 (18.2), reset 0xA2 */
#define IIM42351_REG_APEX_CONFIG1               IIM42351_REG(4, 0x40)
#define IIM42351_LOW_ENERGY_AMP_TH_SEL          GENMASK(7, 4)
#define IIM42351_DMP_POWER_SAVE_TIME_SEL        GENMASK(3, 0)   /* n * 4 s */

/* APEX_CONFIG2 e 3 (18.3 e 18.4): pedometro, manter os valores padrao */
#define IIM42351_REG_APEX_CONFIG2               IIM42351_REG(4, 0x41)   /* reset 0x85 */
#define IIM42351_PED_AMP_TH_SEL                 GENMASK(7, 4)
#define IIM42351_PED_STEP_CNT_TH_SEL            GENMASK(3, 0)
#define IIM42351_REG_APEX_CONFIG3               IIM42351_REG(4, 0x42)   /* reset 0x51 */
#define IIM42351_PED_STEP_DET_TH_SEL            GENMASK(7, 5)
#define IIM42351_PED_SB_TIMER_TH_SEL            GENMASK(4, 2)
#define IIM42351_PED_HI_EN_TH_SEL               GENMASK(1, 0)

/* APEX_CONFIG4 (18.5), reset 0xA4: histerese dos limiares de queda livre */
#define IIM42351_REG_APEX_CONFIG4               IIM42351_REG(4, 0x43)
#define IIM42351_TILT_WAIT_TIME_SEL             GENMASK(7, 6)   /* n * 2 s */
#define IIM42351_LOWG_PEAK_TH_HYST_SEL          GENMASK(5, 3)   /* ver IIM42351_PEAK_TH_HYST_STEP_UG */
#define IIM42351_HIGHG_PEAK_TH_HYST_SEL         GENMASK(2, 0)

/* APEX_CONFIG5 (18.6), reset 0x8C: estado low-g (inicio da queda) */
#define IIM42351_REG_APEX_CONFIG5               IIM42351_REG(4, 0x44)
#define IIM42351_LOWG_PEAK_TH_SEL               GENMASK(7, 3)   /* ver IIM42351_LOWG_PEAK_TH_STEP_UG */
#define IIM42351_LOWG_TIME_TH_SEL               GENMASK(2, 0)   /* n + 1 amostras */

/* APEX_CONFIG6 (18.7), reset 0x5C: estado high-g (impacto) */
#define IIM42351_REG_APEX_CONFIG6               IIM42351_REG(4, 0x45)
#define IIM42351_HIGHG_PEAK_TH_SEL              GENMASK(7, 3)   /* ver IIM42351_HIGHG_PEAK_TH_STEP_MG */
#define IIM42351_HIGHG_TIME_TH_SEL              GENMASK(2, 0)   /* n + 1 amostras */

/* APEX_CONFIG7 e 8 (18.8 e 18.9): deteccao de toque, manter os valores padrao */
#define IIM42351_REG_APEX_CONFIG7               IIM42351_REG(4, 0x46)   /* reset 0x45 */
#define IIM42351_TAP_MIN_JERK_THR               GENMASK(7, 2)
#define IIM42351_TAP_MAX_PEAK_TOL               GENMASK(1, 0)
#define IIM42351_REG_APEX_CONFIG8               IIM42351_REG(4, 0x47)   /* reset 0x5B */
#define IIM42351_TAP_TMAX                       GENMASK(6, 5)
#define IIM42351_TAP_TAVG                       GENMASK(4, 3)
#define IIM42351_TAP_TMIN                       GENMASK(2, 0)

/* APEX_CONFIG9 (18.10) */
#define IIM42351_REG_APEX_CONFIG9               IIM42351_REG(4, 0x48)
#define IIM42351_SENSITIVITY_MODE               BIT(0)

/* APEX_CONFIG10 (18.11): faixa de altura e debounce da queda livre */
#define IIM42351_REG_APEX_CONFIG10              IIM42351_REG(4, 0x49)
#define IIM42351_FF_MIN_DURATION_CM             GENMASK(7, 5)   /* ver IIM42351_FF_MIN_DURATION_CM_LIST */
#define IIM42351_FF_MAX_DURATION_CM             GENMASK(4, 2)   /* ver IIM42351_FF_MAX_DURATION_CM_LIST */
#define IIM42351_FF_DEBOUNCE_DURATION           GENMASK(1, 0)   /* n segundos */

/* ACCEL_WOM_X/Y/Z_THR (18.12 a 18.14): limiar do Wake on Motion */
#define IIM42351_REG_ACCEL_WOM_X_THR            IIM42351_REG(4, 0x4A)
#define IIM42351_REG_ACCEL_WOM_Y_THR            IIM42351_REG(4, 0x4B)
#define IIM42351_REG_ACCEL_WOM_Z_THR            IIM42351_REG(4, 0x4C)

/*
 * INT_SOURCE6 (18.15): fontes APEX roteadas para o INT1.
 * O bit 1 e o FREEFALL; no ICM-42605 esse bit e o SLEEP.
 */
#define IIM42351_REG_INT_SOURCE6                IIM42351_REG(4, 0x4D)
#define IIM42351_STEP_DET_INT1_EN               BIT(5)
#define IIM42351_STEP_CNT_OFL_INT1_EN           BIT(4)
#define IIM42351_TILT_DET_INT1_EN               BIT(3)
#define IIM42351_FREEFALL_DET_INT1_EN           BIT(1)
#define IIM42351_TAP_DET_INT1_EN                BIT(0)

/* INT_SOURCE7 (18.16): fontes APEX roteadas para o INT2 */
#define IIM42351_REG_INT_SOURCE7                IIM42351_REG(4, 0x4E)
#define IIM42351_STEP_DET_INT2_EN               BIT(5)
#define IIM42351_STEP_CNT_OFL_INT2_EN           BIT(4)
#define IIM42351_TILT_DET_INT2_EN               BIT(3)
#define IIM42351_FREEFALL_DET_INT2_EN           BIT(1)
#define IIM42351_TAP_DET_INT2_EN                BIT(0)

/* OFFSET_USER4 a 8 (18.20 a 18.24): offset de 12 bits por eixo, 0,5 mg/LSB, ate +-1 g */
#define IIM42351_REG_OFFSET_USER4               IIM42351_REG(4, 0x7B)
#define IIM42351_ACCEL_X_OFFUSER_HI             GENMASK(7, 4)   /* [11:8] */
#define IIM42351_REG_OFFSET_USER5               IIM42351_REG(4, 0x7C)   /* X [7:0] */
#define IIM42351_REG_OFFSET_USER6               IIM42351_REG(4, 0x7D)   /* Y [7:0] */
#define IIM42351_REG_OFFSET_USER7               IIM42351_REG(4, 0x7E)
#define IIM42351_ACCEL_Z_OFFUSER_HI             GENMASK(7, 4)   /* [11:8] */
#define IIM42351_ACCEL_Y_OFFUSER_HI             GENMASK(3, 0)   /* [11:8] */
#define IIM42351_REG_OFFSET_USER8               IIM42351_REG(4, 0x7F)   /* Z [7:0] */

/* =========================================================
 * FIFO (secao 6)
 * ========================================================= */

#define IIM42351_FIFO_SIZE                      2048    /* bytes fisicos (6.3) */
#define IIM42351_FIFO_SIZE_MAX                  2080    /* buffer do driver deve ter este tamanho (6.3) */

#define IIM42351_FIFO_PACKET1_SIZE              8       /* header + acel + temp[7:0] */
#define IIM42351_FIFO_PACKET2_SIZE              16      /* + timestamp */
#define IIM42351_FIFO_PACKET3_SIZE              20      /* alta resolucao */

/* Header de 1 byte de cada pacote (6.2) */
#define IIM42351_FIFO_HEADER_MSG                BIT(7)          /* 1: FIFO vazia */
#define IIM42351_FIFO_HEADER_PACKET_FORMAT      GENMASK(6, 4)
#define IIM42351_FIFO_HEADER_FORMAT_P1          0x4     /* acel + temp */
#define IIM42351_FIFO_HEADER_FORMAT_P2          0x6     /* acel + temp + timestamp */
#define IIM42351_FIFO_HEADER_FORMAT_P3          0x7     /* alta resolucao */
#define IIM42351_FIFO_HEADER_TIMESTAMP_FSYNC    GENMASK(3, 2)
#define IIM42351_FIFO_HEADER_ODR_ACCEL          BIT(1)          /* ODR mudou desde o pacote anterior */

/* =========================================================
 * Conversoes e constantes
 * ========================================================= */

/* Sensibilidade (secao 3.1): 2048 LSB/g em +-16 g ... 16384 LSB/g em +-2 g */
#define IIM42351_SENSITIVITY_LSB_PER_G(fs_sel)  (2048U << (fs_sel))

/* Amostra invalida nos registradores e na FIFO (14.27/14.28, FIFO_HOLD_LAST_DATA_EN = 0) */
#define IIM42351_INVALID_SAMPLE                 (-32768)

/* APEX_CONFIG5: LOWG_PEAK_TH_SEL n -> (n + 1) * 31,25 mg (31 a 1000 mg) */
#define IIM42351_LOWG_PEAK_TH_STEP_UG           31250
/* APEX_CONFIG6: HIGHG_PEAK_TH_SEL n -> (n + 1) * 250 mg (250 a 8000 mg) */
#define IIM42351_HIGHG_PEAK_TH_STEP_MG          250
/* APEX_CONFIG4: *_PEAK_TH_HYST_SEL n -> (n + 1) * 31,25 mg (31 a 250 mg) */
#define IIM42351_PEAK_TH_HYST_STEP_UG           31250

/* APEX_CONFIG10: altura (cm) correspondente a cada codigo, do 0 ao 7 */
#define IIM42351_FF_MIN_DURATION_CM_LIST        13, 19, 28, 38, 50, 64, 78, 95
#define IIM42351_FF_MAX_DURATION_CM_LIST        113, 154, 201, 255, 314, 380, 452, 531

/* ACCEL_WOM_*_THR: 1 LSB = 1 g / 256 */
#define IIM42351_WOM_TH_UG_PER_LSB              3906

/* =========================================================
 * Tempos
 * ========================================================= */

/* Do power-up ate aceitar leitura/escrita de registradores: < 10 ms (secao 3.2) */
#define IIM42351_POWER_UP_MS                    10
/* Depois do SOFT_RESET_CONFIG (14.1) */
#define IIM42351_SOFT_RESET_MS                  1
/* Depois de sair de OFF, nenhuma escrita de registrador (14.30) */
#define IIM42351_ACCEL_START_US                 200

#endif /* DRV_IIM42351_REG_H_ */
