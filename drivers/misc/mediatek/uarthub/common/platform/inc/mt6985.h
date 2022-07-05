/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MT6985_H
#define MT6985_H

#define UARTHUB_MAX_NUM_DEV_HOST       3

#define UARTHUB_DEV_0_BAUD_RATE        12000000
#define UARTHUB_DEV_1_BAUD_RATE        4000000
#define UARTHUB_DEV_2_BAUD_RATE        12000000
#define UARTHUB_CMM_BAUD_RATE          12000000

#define UARTHUB_BASE_ADDR              0x11005000

#define GPIO_BASE_ADDR                 0x10005000
#define GPIO_HUB_MODE_TX_OFFSET        0x4C0
#define GPIO_HUB_MODE_TX_MASK          0x7
#define GPIO_HUB_MODE_TX_VALUE         0x2
#define GPIO_HUB_MODE_RX_OFFSET        0x4C0
#define GPIO_HUB_MODE_RX_MASK          0x70
#define GPIO_HUB_MODE_RX_VALUE         0x20
#define GPIO_HUB_DIR_TX_OFFSET         0x70
#define GPIO_HUB_DIR_TX_MASK           0x1
#define GPIO_HUB_DIR_TX_SHIFT          0
#define GPIO_HUB_DIR_RX_OFFSET         0x70
#define GPIO_HUB_DIR_RX_MASK           0x2
#define GPIO_HUB_DIR_RX_SHIFT          1
#define GPIO_HUB_DIN_RX_OFFSET         0x270
#define GPIO_HUB_DIN_RX_MASK           0x2
#define GPIO_HUB_DIN_RX_SHIFT          1

#define IOCFG_RM_BASE_ADDR             0x11C00000
#define GPIO_HUB_IES_TX_OFFSET         0x20
#define GPIO_HUB_IES_TX_MASK           0x2
#define GPIO_HUB_IES_TX_SHIFT          1
#define GPIO_HUB_IES_RX_OFFSET         0x20
#define GPIO_HUB_IES_RX_MASK           0x1
#define GPIO_HUB_IES_RX_SHIFT          0
#define GPIO_HUB_PU_TX_OFFSET          0x40
#define GPIO_HUB_PU_TX_MASK            0x2
#define GPIO_HUB_PU_TX_SHIFT           1
#define GPIO_HUB_PU_RX_OFFSET          0x40
#define GPIO_HUB_PU_RX_MASK            0x1
#define GPIO_HUB_PU_RX_SHIFT           0
#define GPIO_HUB_PD_TX_OFFSET          0x30
#define GPIO_HUB_PD_TX_MASK            0x2
#define GPIO_HUB_PD_TX_SHIFT           1
#define GPIO_HUB_PD_RX_OFFSET          0x30
#define GPIO_HUB_PD_RX_MASK            0x1
#define GPIO_HUB_PD_RX_SHIFT           0
#define GPIO_HUB_DRV_TX_OFFSET         0x0
#define GPIO_HUB_DRV_TX_MASK           0x38000
#define GPIO_HUB_DRV_TX_SHIFT          15
#define GPIO_HUB_DRV_RX_OFFSET         0x0
#define GPIO_HUB_DRV_RX_MASK           0x38000
#define GPIO_HUB_DRV_RX_SHIFT          15
#define GPIO_HUB_SMT_TX_OFFSET         0x60
#define GPIO_HUB_SMT_TX_MASK           0x20
#define GPIO_HUB_SMT_TX_SHIFT          5
#define GPIO_HUB_SMT_RX_OFFSET         0x60
#define GPIO_HUB_SMT_RX_MASK           0x20
#define GPIO_HUB_SMT_RX_SHIFT          5
#define GPIO_HUB_TDSEL_TX_OFFSET       0x70
#define GPIO_HUB_TDSEL_TX_MASK         0xF00000
#define GPIO_HUB_TDSEL_TX_SHIFT        20
#define GPIO_HUB_TDSEL_RX_OFFSET       0x70
#define GPIO_HUB_TDSEL_RX_MASK         0xF00000
#define GPIO_HUB_TDSEL_RX_SHIFT        20
#define GPIO_HUB_RDSEL_TX_OFFSET       0x50
#define GPIO_HUB_RDSEL_TX_MASK         0xC00
#define GPIO_HUB_RDSEL_TX_SHIFT        10
#define GPIO_HUB_RDSEL_RX_OFFSET       0x50
#define GPIO_HUB_RDSEL_RX_MASK         0xC00
#define GPIO_HUB_RDSEL_RX_SHIFT        10
#define GPIO_HUB_SEC_EN_TX_OFFSET      0xA00
#define GPIO_HUB_SEC_EN_TX_MASK        0x8
#define GPIO_HUB_SEC_EN_TX_SHIFT       3
#define GPIO_HUB_SEC_EN_RX_OFFSET      0xA00
#define GPIO_HUB_SEC_EN_RX_MASK        0x10
#define GPIO_HUB_SEC_EN_RX_SHIFT       4

#define PERICFG_AO_BASE_ADDR           0x11036000
#define PERICFG_AO_PERI_CG_1              0x14
#define PERICFG_AO_PERI_CG_1_MASK      0x3800000
#define PERICFG_AO_PERI_CG_1_SHIFT     23
#define PERICFG_AO_PERI_CLOCK_CON      0x20
#define PERICFG_AO_PERI_CLOCK_CON_MASK 0xF00
#define PERICFG_AO_PERI_UART_WAKEUP       0x50
#define PERICFG_AO_PERI_UART_WAKEUP_MASK  0x10
#define PERICFG_AO_PERI_UART_WAKEUP_SHIFT 4

#define HW_CCF_BASE_ADDR               0x10320000
#define HW_CCF_PLL_DONE_OFFSET         0x140C
#define HW_CCF_PLL_DONE_SHIFT          4

#define APMIXEDSYS_BASE_ADDR           0x1000C000
#define APMIXEDSYS_UNIVPLL_CON0        0x308
#define APMIXEDSYS_UNIVPLL_CON0_SHIFT  0

#define TOPCKGEN_BASE_ADDR             0x10000000
#define TOPCKGEN_CLK_CFG_6_OFFSET      0x70
#define TOPCKGEN_CLK_CFG_6_MASK        0x3
#define TOPCKGEN_CLK_CFG_6_SHIFT       0x0

#define SPM_BASE_ADDR                  0x1C001000
#define SPM_MD32PCM_SCU_CTRL1          0x104
#define SPM_REQ_STA_9                  0x86C
#define SPM_MD32PCM_SCU_CTRL1_MASK     (0x17 << 17)
#define SPM_MD32PCM_SCU_CTRL1_SHIFT    17
#define SPM_REQ_STA_9_MASK             (0x1F << 17)
#define SPM_REQ_STA_9_SHIFT            17

#define UART3_BASE_ADDR                0x11004000
#define AP_DMA_UART_3_TX_INT_FLAG_ADDR 0x11301300

#endif /* MT6985_H */
