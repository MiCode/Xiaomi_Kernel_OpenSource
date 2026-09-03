/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/*
 * Copyright (C) 2020 UNISOC Technologies Co., Ltd.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 ********************************************************************
 * Auto generated c code from ASIC Documentation, PLEASE DONOT EDIT *
 ********************************************************************
 */

#ifndef __DT_BINDINGS_UNISOC_UMP9622_MASK_H
#define __DT_BINDINGS_UNISOC_UMP9622_MASK_H

#define MASK_ANA_UMP7522_CHIP_ID_LOW                        0xFFFF
#define MASK_ANA_UMP7522_CHIP_ID_HIGH                       0xFFFF
#define MASK_ANA_UMP7522_PINREG_EN                          0x0100
#define MASK_ANA_UMP7522_CAL_EN                             0x0001
#define MASK_ANA_UMP7522_CLK_CAL_EN                         0x0004
#define MASK_ANA_UMP7522_CLK_TSEN_ADC_EN                    0x0001
#define MASK_ANA_UMP7522_CAL_SOFT_RST                       0x0001
#define MASK_ANA_UMP7522_ARCH_EN                            0x0001
#define MASK_ANA_UMP7522_MCU_WR_PROT                        0x8000
#define MASK_ANA_UMP7522_MCU_WR_PROT_VALUE                  0x7FFF
#define MASK_ANA_UMP7522_PWR_OFF_SEQ_EN                     0x0001
#define MASK_ANA_UMP7522_DBG_CALCULATION_DONE               0x1000
#define MASK_ANA_UMP7522_DBG_CALIBRATION_CNT_FULL           0x0800
#define MASK_ANA_UMP7522_DBG_CURRENT_STATE                  0x0700
#define MASK_ANA_UMP7522_DBG_CBANK_RESET_EN                 0x0080
#define MASK_ANA_UMP7522_DBG_RC_32K_AUTO_SEL                0x0040
#define MASK_ANA_UMP7522_DBG_XTL_EN                         0x0020
#define MASK_ANA_UMP7522_DBG_OSC_EN                         0x0010
#define MASK_ANA_UMP7522_DBG_DCXO_32K_EN                    0x0008
#define MASK_ANA_UMP7522_DBG_DCXO_PD                        0x0004
#define MASK_ANA_UMP7522_DBG_DCXO_LP_EN                     0x0002
#define MASK_ANA_UMP7522_DBG_CLK_26M_RTC_EN                 0x0001
#define MASK_ANA_UMP7522_DBG_TSX_XO_WAKEUP_N                0x0400
#define MASK_ANA_UMP7522_DBG_C2R_SLP_DCXO_PD_EN             0x0200
#define MASK_ANA_UMP7522_DBG_C2R_SLP_DCXO_LP_EN             0x0100
#define MASK_ANA_UMP7522_DBG_SLP_DCXO_26M_REF_OUT3          0x0080
#define MASK_ANA_UMP7522_DBG_SLP_DCXO_26M_REF_OUT2          0x0040
#define MASK_ANA_UMP7522_DBG_SLP_DCXO_26M_REF_OUT1          0x0020
#define MASK_ANA_UMP7522_DBG_SLP_DCXO_26M_REF_OUT0          0x0010
#define MASK_ANA_UMP7522_DBG_DCXO_26M_REF_OUT3_WAKEUP       0x0008
#define MASK_ANA_UMP7522_DBG_DCXO_26M_REF_OUT2_WAKEUP       0x0004
#define MASK_ANA_UMP7522_DBG_DCXO_26M_REF_OUT1_WAKEUP       0x0002
#define MASK_ANA_UMP7522_DBG_DCXO_26M_REF_OUT0_WAKEUP       0x0001
#define MASK_ANA_UMP7522_ACK_BIT_OPT                        0x0001
#define MASK_ANA_UMP7522_RC1M_CAL                           0xFE00
#define MASK_ANA_UMP7522_RC1M_POWERSEL                      0x0100
#define MASK_ANA_UMP7522_RC_OSC_EN                          0x0001
#define MASK_ANA_UMP7522_SLP_IO_EN                          0x0002
#define MASK_ANA_UMP7522_SLP_LDO_PD_EN                      0x0001
#define MASK_ANA_UMP7522_CLK_RX_256K_SEL                    0x0008
#define MASK_ANA_UMP7522_CLK_DCXO_32K_SW_ON                 0x0004
#define MASK_ANA_UMP7522_CLK_DCXO_256K_SW_ON                0x0002
#define MASK_ANA_UMP7522_CLK_DCXO_SW_CTRL                   0x0001
#define MASK_ANA_UMP7522_SLP_DCXO_26M_REF_OUT3_EN           0x0100
#define MASK_ANA_UMP7522_SLP_DCXO_26M_REF_OUT2_EN           0x0080
#define MASK_ANA_UMP7522_SLP_DCXO_26M_REF_OUT1_EN           0x0040
#define MASK_ANA_UMP7522_SLP_DCXO_26M_REF_OUT0_EN           0x0020
#define MASK_ANA_UMP7522_DCXO_26M_REF_OUT_EN                0x000F
#define MASK_ANA_UMP7522_OPT_RC1M_SW_MODE_EN                0x0010
#define MASK_ANA_UMP7522_OPT_RC1M_SW_MODE                   0x0008
#define MASK_ANA_UMP7522_RC1M_CAL_SW_EN                     0x0004
#define MASK_ANA_UMP7522_RC1M_CAL_SW_CTRL                   0x0002
#define MASK_ANA_UMP7522_RC1M_CAL_SW_CLK_ON                 0x0001
#define MASK_ANA_UMP7522_SLP_DCXO_LP_EN                     0x8000
#define MASK_ANA_UMP7522_SLP_DCXO_PD_EN                     0x4000
#define MASK_ANA_UMP7522_DCXO_CORE_AML_CAL_EN               0x0008
#define MASK_ANA_UMP7522_DCXO_26M_REF_BUF2_DRV_LEVEL_CTRL   0xE000
#define MASK_ANA_UMP7522_DCXO_26M_REF_BUF3_DRV_LEVEL_CTRL   0x1C00
#define MASK_ANA_UMP7522_DCXO_32K_DIV_MODE_SEL              0x0200
#define MASK_ANA_UMP7522_DCXO_32K_CLKIN_26M_INV_CTRL        0x0100
#define MASK_ANA_UMP7522_DCXO_CORE_AML_CTRL                 0x00F0
#define MASK_ANA_UMP7522_DCXO_CORE_AML_CAL_OK_FLAG          0x0008
#define MASK_ANA_UMP7522_TCXO_MODE                          0x0002
#define MASK_ANA_UMP7522_DCXO_SCEN_DET_EN                   0x0001
#define MASK_ANA_UMP7522_DCXO_CORE_AML_CAL_CTRL_HP          0x00FF
#define MASK_ANA_UMP7522_DCXO_CORE_AML_CAL_CTRL_LP          0x00FF
#define MASK_ANA_UMP7522_DCXO_CORE_GM_HELPER_HP             0x0001
#define MASK_ANA_UMP7522_DCXO_CORE_GM_HELPER_LP             0x0001
#define MASK_ANA_UMP7522_DCXO_CORE_CBANK_HP                 0x00FF
#define MASK_ANA_UMP7522_DCXO_CORE_CBANK_LP                 0x00FF
#define MASK_ANA_UMP7522_DCXO_32K_FRAC_DIV_RATIO_CTRL_HP    0x0FFF
#define MASK_ANA_UMP7522_DCXO_32K_FRAC_DIV_RATIO_CTRL_LP    0x0FFF
#define MASK_ANA_UMP7522_DCXO_CORE_BUF_MODE_SEL             0x8000
#define MASK_ANA_UMP7522_DCXO_26M_REF_BUF2_DIV_MODE_SEL     0x4000
#define MASK_ANA_UMP7522_DCXO_26M_REF_BUF3_DIV_MODE_SEL     0x2000
#define MASK_ANA_UMP7522_DCXO_LP_CAL_EN                     0x0800
#define MASK_ANA_UMP7522_DCXO_26M_REF_BUF_OUTPUT_BYPASS     0x03C0
#define MASK_ANA_UMP7522_DCXO_26M_REF_BUF0_DRV_LEVEL_CTRL   0x0038
#define MASK_ANA_UMP7522_DCXO_26M_REF_BUF1_DRV_LEVEL_CTRL   0x0007
#define MASK_ANA_UMP7522_DCXO_CRYSTAL_FLAG                  0x6000
#define MASK_ANA_UMP7522_RTC_MODE                           0x1000
#define MASK_ANA_UMP7522_CLK_256K_EN                        0x0800
#define MASK_ANA_UMP7522_DCXO_THM_ARCH_SEL                  0x0400
#define MASK_ANA_UMP7522_DCXO_THM_RES_SHORT                 0x0200
#define MASK_ANA_UMP7522_DCXO_AC_COUPLE_BUF_SEL             0x0100
#define MASK_ANA_UMP7522_DCXO_PD                            0x0080
#define MASK_ANA_UMP7522_DCXO_CORE_BIAS_HP_WIDTH_SEL        0x0040
#define MASK_ANA_UMP7522_DCXO_CORE_BIAS_HP_NW_IBIAS_EN      0x0020
#define MASK_ANA_UMP7522_DCXO_CBANK_CFIX_SEL                0x0018
#define MASK_ANA_UMP7522_DCXO_26M_REF_BUF0_DIV_MODE_SEL     0x0002
#define MASK_ANA_UMP7522_DCXO_26M_REF_BUF1_DIV_MODE_SEL     0x0001
#define MASK_ANA_UMP7522_EXT_XTL_RX_IO_WAKEUP_EN            0x8000
#define MASK_ANA_UMP7522_EXT_XTL_RX_ODD_PARITY              0x4000
#define MASK_ANA_UMP7522_EXT_XTL_RX_CAP_SEL                 0x3000
#define MASK_ANA_UMP7522_EXT_XTL_RX_FORCE_MODE              0x0800
#define MASK_ANA_UMP7522_EXT_XTL_RX_DEBUG_MODE              0x0400
#define MASK_ANA_UMP7522_EXT_XTL_RX_DEBUG_SEND              0x0200
#define MASK_ANA_UMP7522_EXT_XTL_RX_DEBUG_DATA              0x01FF
#define MASK_ANA_UMP7522_EXT_XTL_RX_DATA                    0xF800
#define MASK_ANA_UMP7522_EXT_XTL_RX_ALL_RDY                 0x0400
#define MASK_ANA_UMP7522_EXT_XTL_RX_MSB_RDY                 0x0200
#define MASK_ANA_UMP7522_EXT_XTL_RX_ERROR_FLAG              0x0100
#define MASK_ANA_UMP7522_RESERVED_CORE                      0x00FF
#define MASK_ANA_UMP7522_RESERVED_RTC                       0x00FF
#define MASK_ANA_UMP7522_XO_LOW_CUR_FLAG                    0x2000
#define MASK_ANA_UMP7522_XO_LOW_CUR_FLAG_CLR                0x0200
#define MASK_ANA_UMP7522_XO_LOW_CUR_CNT_CLR                 0x0100
#define MASK_ANA_UMP7522_RC_32K_SEL                         0x0002
#define MASK_ANA_UMP7522_RTC_RC_OSC_EN                      0x0001
#define MASK_ANA_UMP7522_XO_LOW_CUR_CNT_LOW                 0xFFFF
#define MASK_ANA_UMP7522_XO_LOW_CUR_CNT_HIGH                0xFFFF
#define MASK_ANA_UMP7522_XTL_EN                             0x0100
#define MASK_ANA_UMP7522_EXT_XTL_RX_ENCODE_DATA             0x01FF
#define MASK_ANA_UMP7522_TSX_WR_PROT                        0x8000
#define MASK_ANA_UMP7522_TSX_WR_PROT_VALUE                  0x7FFF
#define MASK_ANA_UMP7522_AUTO_SWITCH_TO_RC_EN               0x8000
#define MASK_ANA_UMP7522_MONITOR_ACCURACY                   0x7800
#define MASK_ANA_UMP7522_TIME_BETWEEN_CALIBRATION           0x0780
#define MASK_ANA_UMP7522_TIME_FOR_CALIBRATION               0x0060
#define MASK_ANA_UMP7522_TIME_FOR_DCXO_STABLE               0x001E
#define MASK_ANA_UMP7522_LOW_PWR_CLK32K_EN                  0x0001
#define MASK_ANA_UMP7522_POR_VDDDCXO_N                      0x0040
#define MASK_ANA_UMP7522_POR_DVDD_V                         0x000C
#define MASK_ANA_UMP7522_POR_VDDDCXO_V                      0x0003
#define MASK_ANA_UMP7522_TSEN_CLKSEL                        0x1800
#define MASK_ANA_UMP7522_TSEN_CHOP_CLKSEL                   0x0600
#define MASK_ANA_UMP7522_TSEN_ADCLDO_V                      0x01E0
#define MASK_ANA_UMP7522_TSEN_CLK_SRC_SEL                   0x0010
#define MASK_ANA_UMP7522_TSEN_CLK_DUTY_CRC_EN               0x0008
#define MASK_ANA_UMP7522_TSEN_CLK_PHASE_SEL                 0x0007
#define MASK_ANA_UMP7522_TSEN_DATA_EDGE_SEL                 0x8000
#define MASK_ANA_UMP7522_TSEN_INPUT_EN                      0x2000
#define MASK_ANA_UMP7522_TSEN_UGBUF_CHOP_EN                 0x0400
#define MASK_ANA_UMP7522_TSEN_SDADC_CHOP_EN                 0x0200
#define MASK_ANA_UMP7522_UGBUF_CTRL                         0x0100
#define MASK_ANA_UMP7522_TSEN_SDADC_EN                      0x00F0
#define MASK_ANA_UMP7522_CLK_26M_TSEN_EN                    0x000F
#define MASK_ANA_UMP7522_BG_RBIAS_MODE                      0x0800
#define MASK_ANA_UMP7522_TSEN_VREF_SEL                      0x0400
#define MASK_ANA_UMP7522_TSEN_CH_DCXO_BJT_SEL               0x0200
#define MASK_ANA_UMP7522_TSEN_CLK_SOFT_RST                  0x0100
#define MASK_ANA_UMP7522_TSEN_CH_THM_SEL                    0x0080
#define MASK_ANA_UMP7522_TSEN_CLK_PHASE_MODE_SEL            0x0040
#define MASK_ANA_UMP7522_TSEN_SDADC_CTRL1                   0x003C
#define MASK_ANA_UMP7522_TSEN_INPUT_RC_EN                   0x0002
#define MASK_ANA_UMP7522_CLK_TSEN_SEL                       0x0001
#define MASK_ANA_UMP7522_TSEN_ADCLDO_EN                     0xF000
#define MASK_ANA_UMP7522_TSEN_UGBUF_EN                      0x0F00
#define MASK_ANA_UMP7522_TSEN_EN                            0x00F0
#define MASK_ANA_UMP7522_TSEN_SEL_CH                        0x0008
#define MASK_ANA_UMP7522_TSEN_TIME_SEL                      0x0007
#define MASK_ANA_UMP7522_TSEN_OUT_LOW                       0xFFFF
#define MASK_ANA_UMP7522_OSC_OUT_LOW                        0xFFFF
#define MASK_ANA_UMP7522_AUTO_SEL_HIGH                      0x3800
#define MASK_ANA_UMP7522_AUTO_SEL_LOW                       0x0700
#define MASK_ANA_UMP7522_TSEN_SEL_EN                        0x00C0
#define MASK_ANA_UMP7522_OSC_OUT_HIGH                       0x0038
#define MASK_ANA_UMP7522_TSEN_OUT_HIGH                      0x0007
#define MASK_ANA_UMP7522_TSEN_ADCLDO_V_RSLT                 0x03C0
#define MASK_ANA_UMP7522_TSEN_ADCLDO_V_WAIT                 0x0038
#define MASK_ANA_UMP7522_TSEN_ADCLDO_V_DONE                 0x0004
#define MASK_ANA_UMP7522_TSEN_ADCLDO_V_OVFL                 0x0002
#define MASK_ANA_UMP7522_TSEN_ADCLDO_V_CALB                 0x0001

#endif /* __DT_BINDINGS_UNISOC_UMP9620_MASK_H */

