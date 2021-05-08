/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __ADSP_REG_H
#define __ADSP_REG_H

#define ADSP_CFGREG_SW_RSTN         (ADSP_BASE + 0x00)
#define ADSP_A_SW_RSTN              (0x11)

#define ADSP_HIFI3_IO_CONFIG        (ADSP_BASE + 0x0008)
#define ADSP_A_RUNSTALL             (1 << 31)
#define ADSP_RUNSTALL               (ADSP_A_RUNSTALL)

/* swint register no update naming */
#define ADSP_SW_INT_SET             (ADSP_BASE + 0x001C)
#define ADSP_SW_INT_CLR             (ADSP_BASE + 0x0020)
#define ADSP_SW_INT0                (0)
#define ADSP_SW_INT1                (1)
#define ADSP_SW_INT2                (2)
#define ADSP_SW_INT3                (3)
#define HOST_TO_ADSP_A              (1 << 0)
#define ADSP_A_SW_INT               (1 << ADSP_SW_INT1)

#define ADSP_GENERAL_IRQ_SET        (ADSP_BASE + 0x0034)
#define ADSP_GENERAL_IRQ_CLR        (ADSP_BASE + 0x0038)
#define ADSP_A_2HOST_IRQ_BIT        (1 << 0)
#define ADSP_A_2CONN_IRQ_BIT        (1 << 4)
#define ADSP_GENERAL_IRQ_INUSED     \
	(ADSP_A_2HOST_IRQ_BIT)

#define ADSP_DVFSRC_STATE           (ADSP_BASE + 0x003C)
#define ADSP_DVFSRC_REQ             (ADSP_BASE + 0x0040)
#define ADSP_DDREN_REQ              (ADSP_BASE + 0x0044)
#define ADSP_DDR_REQ_SEL            (0x3 << 4)             // hw auto ddren
#define ADSP_DDR_ENABLE             (1 << 0)
#define ADSP_SPM_REQ                (ADSP_BASE + 0x0048)
#define ADSP_SPM_ACK                (ADSP_BASE + 0x004C)
#define ADSP_SPM_SRC_BITS           (0xF << 0)
#define ADSP_IRQ_EN                 (ADSP_BASE + 0x0050)

#define ADSP_A_SPM_WAKEUPSRC        (ADSP_BASE + 0x005C)
#define ADSP_WAKEUP_SPM             (0x1 << 0)

#define ADSP_SEMAPHORE              (ADSP_BASE + 0x0058)

#define ADSP_A_WDT_REG              (ADSP_BASE + 0x007C)
#define ADSP_A_WDT_INIT_VALUE       (ADSP_BASE + 0x0080)
#define ADSP_A_WDT_CNT              (ADSP_BASE + 0x0084)
#define ADSP_WDT_TRIGGER            (ADSP_A_WDT_INIT_VALUE)
#define WDT_EN_BIT  (1 << 31)
#define WDT_DIS_BIT (0 << 31)
#define WDT_KICK_BIT 0

#define ADSP_CFGREG_RSV_RW_REG0     (ADSP_BASE + 0x008C)
#define ADSP_CFGREG_RSV_RW_REG1     (ADSP_BASE + 0x0090)
#define ADSP_CREG_BOOTUP_MARK       ADSP_CFGREG_RSV_RW_REG0

/* Latch Debug info after WDT */
#define ADSP_A_WDT_DEBUG_PC_REG     (ADSP_BASE + 0x0170)
#define ADSP_A_WDT_DEBUG_SP_REG     (ADSP_BASE + 0x0174)
/* TODO : add in aee dump */
#define ADSP_A_WDT_EXCVADDR_REG     (ADSP_BASE + 0x0178)
/* TODO : add in aee dump */
#define ADSP_A_WDT_EXCCAUSE_REG     (ADSP_BASE + 0x017C)

/* Wakeup ADSP interrupt */
#define ADSP_WAKEUPSRC_MASK         (ADSP_BASE + 0x00A8)
#define ADSP_WAKEUPSRC_IRQ          (ADSP_BASE + 0x00AC)

/*latency monitor*/
#define ADSP_LATMON_DVFS_MODE       (ADSP_BASE + 0x0100)
#define ADSP_LATMON_CON1            (ADSP_BASE + 0x0104)
#define ADSP_LATMON_CON2            (ADSP_BASE + 0x0108)
#define ADSP_LATMON_MARGIN          (ADSP_BASE + 0x010C)
#define ADSP_LATMON_THRESHOLD       (ADSP_BASE + 0x0110)
#define ADSP_LATMON_STATE           (ADSP_BASE + 0x0114)
#define ADSP_LATMON_ACCCNT          (ADSP_BASE + 0x0118)
/* latency threshold & budget for 0.8V. 0.7V. 0.625V */
#define ADSP_LATMON_CONT0           (ADSP_BASE + 0x012C)
#define ADSP_LATMON_CONT1           (ADSP_BASE + 0x0130)
#define ADSP_LATMON_CONT2           (ADSP_BASE + 0x0134)

#define ADSP_A_DEBUG_PC_REG         (ADSP_BASE + 0x013C)
#define ADSP_DBG_PEND_CNT           (ADSP_BASE + 0x015C)
#define ADSP_SLEEP_STATUS_REG       (ADSP_BASE + 0x0158)
#define ADSP_BUS_MON_BASE           (ADSP_BASE + 0x5000)

#define ADSP_A_IS_WFI               (1 << 0)
#define ADSP_AXI_BUS_IS_IDLE        (1 << 1)

 /* clk reg */
 #define ADSP_CLK_CTRL_BASE         (ADSP_BASE + 0x1000)
 #define ADSP_CLK_UART_EN           (1 << 5)
 #define ADSP_CLK_DMA_EN            (1 << 4)
 #define ADSP_CLK_TIMER_EN          (1 << 3)
 #define ADSP_CLK_CORE_0_EN         (1 << 0)
 #define ADSP_UART_CTRL             (ADSP_BASE + 0x1010)
 #define ADSP_UART_RST_N            (1 << 3)
 #define ADSP_UART_CLK_SEL          (1 << 1)
 #define ADSP_UART_BCLK_CG          (1 << 0)

/* INFRA_IRQ (always on register) */
#define INFRA_AXI_PROT              (INFRACFG_AO_BASE + 0x0220)
#define INFRA_AXI_PROT_STA1         (INFRACFG_AO_BASE + 0x0228)
#define INFRA_AXI_PROT_SET          (INFRACFG_AO_BASE + 0x02A0)
#define INFRA_AXI_PROT_CLR          (INFRACFG_AO_BASE + 0x02A4)
#define ADSP_AXI_PROT_MASK          (0x1 << 15)
#define ADSP_AXI_PROT_READY_MASK    (0x1 << 15)
#define ADSP_WAY_EN_CTRL            (INFRACFG_AO_BASE + 0x0240)
#define ADSP_WAY_EN_MASK            (0x1 << 13)

#define AP_AWAKE_LOCK_BIT           (0)
#define AP_AWAKE_UNLOCK_BIT         (1)
#define CONNSYS_AWAKE_LOCK          (2)
#define CONNSYS_AWAKE_UNLOCK        (3)
#define AP_AWAKE_DUMP_BIT           (4)
#define AP_AWAKE_UPDATE_BIT         (5)
#define AP_AWAKE_STATE_BIT          (6)
#define ADSPPLL_UNLOCK_BIT          (8)

#define ADSP_ADSP2SPM_VOL_LV        (ADSP_BASE + 0x0094)

#endif

