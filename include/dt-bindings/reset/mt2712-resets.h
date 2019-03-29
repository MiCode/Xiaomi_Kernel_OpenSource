/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Yong Liang <yong.liang@mediatek.com>
 */

#ifndef _DT_BINDINGS_RESET_CONTROLLER_MT2712
#define _DT_BINDINGS_RESET_CONTROLLER_MT2712

/* INFRACFG resets */
#define MT2712_INFRA_EMI_REG_RST        0
#define MT2712_INFRA_DRAMC0_A0_RST      1
#define MT2712_INFRA_APCIRQ_EINT_RST    3
#define MT2712_INFRA_APXGPT_RST         4
#define MT2712_INFRA_SCPSYS_RST         5
#define MT2712_INFRA_KP_RST             6
#define MT2712_INFRA_PMIC_WRAP_RST      7
#define MT2712_INFRA_MPIP_RST           8
#define MT2712_INFRA_CEC_RST            9
#define MT2712_INFRA_EMI_RST            32
#define MT2712_INFRA_DRAMC0_RST         34
#define MT2712_INFRA_APMIXEDSYS_RST     35
#define MT2712_INFRA_MIPI_DSI_RST       36
#define MT2712_INFRA_TRNG_RST           37
#define MT2712_INFRA_SYSIRQ_RST         38
#define MT2712_INFRA_MIPI_CSI_RST       39
#define MT2712_INFRA_GCE_FAXI_RST       40
#define MT2712_INFRA_MMIOMMURST         47


/*  PERICFG resets */
#define MT2712_PERI_UART0_SW_RST        0
#define MT2712_PERI_UART1_SW_RST        1
#define MT2712_PERI_UART2_SW_RST        2
#define MT2712_PERI_UART3_SW_RST        3
#define MT2712_PERI_IRRX_SW_RST         4
#define MT2712_PERI_PWM_SW_RST          8
#define MT2712_PERI_AUXADC_SW_RST       10
#define MT2712_PERI_DMA_SW_RST          11
#define MT2712_PERI_I2C6_SW_RST         13
#define MT2712_PERI_NFI_SW_RST          14
#define MT2712_PERI_THERM_SW_RST        16
#define MT2712_PERI_MSDC2_SW_RST        17
#define MT2712_PERI_MSDC3_SW_RST        18
#define MT2712_PERI_MSDC0_SW_RST        19
#define MT2712_PERI_MSDC1_SW_RST        20
#define MT2712_PERI_I2C0_SW_RST         22
#define MT2712_PERI_I2C1_SW_RST         23
#define MT2712_PERI_I2C2_SW_RST         24
#define MT2712_PERI_I2C3_SW_RST         25
#define MT2712_PERI_I2C4_SW_RST         26
#define MT2712_PERI_HDMI_SW_RST         29
#define MT2712_PERI_SPI0_SW_RST         33

#endif  /* _DT_BINDINGS_RESET_CONTROLLER_MT2712 */
