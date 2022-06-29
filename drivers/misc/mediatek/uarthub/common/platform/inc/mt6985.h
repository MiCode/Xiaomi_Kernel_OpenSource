/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MT6985_H
#define MT6985_H

#define UARTHUB_MAX_NUM_DEV_HOST      3

#define UARTHUB_DEV_0_BAUD_RATE       12000000
#define UARTHUB_DEV_1_BAUD_RATE       4000000
#define UARTHUB_DEV_2_BAUD_RATE       12000000
#define UARTHUB_CMM_BAUD_RATE         12000000

#define UARTHUB_BASE_ADDR             0x11005000

#define GPIO_BASE_ADDR                0x10005000
#define GPIO_HUB_MODE_TX_OFFSET       0x4C0
#define GPIO_HUB_MODE_TX_MASK         0x7
#define GPIO_HUB_MODE_TX_VALUE        0x2
#define GPIO_HUB_MODE_RX_OFFSET       0x4C0
#define GPIO_HUB_MODE_RX_MASK         0x70
#define GPIO_HUB_MODE_RX_VALUE        0x20

#define PERICFG_AO_BASE_ADDR          0x11036000
#define PERICFG_AO_PERI_CG_1_OFFSET   0x14
#define PERICFG_AO_PERI_CG_1_MASK     0x3800000
#define PERICFG_AO_PERI_CG_1_SHIFT    23

#define HW_CCF_BASE_ADDR              0x10320000
#define HW_CCF_PLL_DONE_OFFSET        0x140C
#define HW_CCF_PLL_DONE_SHIFT         4

#define TOPCKGEN_BASE_ADDR            0x10000000
#define TOPCKGEN_CLK_CFG_6_OFFSET     0x70
#define TOPCKGEN_CLK_CFG_6_MASK       0x3
#define TOPCKGEN_CLK_CFG_6_SHIFT      0x0

#define UART3_BASE_ADDR                0x11004000
#define AP_DMA_UART_3_TX_INT_FLAG_ADDR 0x11301300

#endif /* MT6985_H */
