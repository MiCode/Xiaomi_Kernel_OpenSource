/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __ADSP_REG_H
#define __ADSP_REG_H

/*#define ADSP_BASE in use file */
#define ADSP_CFGREG_SW_RSTN         (ADSP_BASE + 0x0000)
#define ADSP_A_SW_RSTN              (0x11)
#define ADSP_B_SW_RSTN              (0x22)
#define ADSP_SW_RSTN                (0x33)

#define ADSP_HIFI3_IO_CONFIG        (ADSP_BASE + 0x000C)
#define ADSP_A_RUNSTALL             (1 << 31)
#define ADSP_B_RUNSTALL             (1 << 30)
#define ADSP_RUNSTALL               (ADSP_A_RUNSTALL | ADSP_B_RUNSTALL)

#define ADSP_A_INTR_STATUS          (ADSP_BASE + 0x0010)
#define ADSP_B_INTR_STATUS          (ADSP_BASE + 0x0014)
#define INFRABUS_TIMEOUT_IRQ        (1 << 24)

#define ADSP_SW_INT_SET             (ADSP_BASE + 0x0018)
#define ADSP_SW_INT_CLR             (ADSP_BASE + 0x001C)
#define ADSP_A_SW_INT               (1 << 0)
#define ADSP_B_SW_INT               (1 << 1)
#define ADSP_AB_SW_INT              (1 << 2)

#define ADSP_GENERAL_IRQ_SET        (ADSP_BASE + 0x0034)
#define ADSP_GENERAL_IRQ_CLR        (ADSP_BASE + 0x0038)
#define ADSP_A_2HOST_IRQ_BIT        (1 << 0)
#define ADSP_B_2HOST_IRQ_BIT        (1 << 1)
#define ADSP_A_AFE2HOST_IRQ_BIT     (1 << 2)
#define ADSP_B_AFE2HOST_IRQ_BIT     (1 << 3)
#define ADSP_GENERAL_IRQ_INUSED     \
	(ADSP_A_2HOST_IRQ_BIT | ADSP_B_2HOST_IRQ_BIT \
	| ADSP_A_AFE2HOST_IRQ_BIT | ADSP_B_AFE2HOST_IRQ_BIT)

#define ADSP_A_DDREN_REQ            (ADSP_BASE + 0x0044)
#define ADSP_B_DDREN_REQ            (ADSP_BASE + 0x0048)
#define ADSP_SPM_ACK                (ADSP_BASE + 0x004C)
#define ADSP_DDR_REQ_SEL            (0x3 << 6)
#define ADSP_DDR_EN                 (0x1 << 4)
#define ADSP_SRCLKENA               (0x1 << 3)
#define ADSP_APSRC_EN               (0x1 << 2)
#define ADSP_VREF18_REQ             (0x1 << 1)
#define ADSP_INFRA_REQ              (0x1 << 0)
#define ADSP_SPM_SRC_BITS           (ADSP_DDR_EN | ADSP_SRCLKENA \
	| ADSP_APSRC_EN | ADSP_VREF18_REQ \
	| ADSP_INFRA_REQ)
#define ADSP_A_SPM_SRC_BITS         (0xF << 0)
#define ADSP_A_IRQ_EN               (ADSP_BASE + 0x0050)
#define ADSP_B_IRQ_EN               (ADSP_BASE + 0x0058)

#define ADSP_A_SPM_WAKEUPSRC        (ADSP_BASE + 0x005C)
#define ADSP_B_SPM_WAKEUPSRC        (ADSP_BASE + 0x0060)
#define ADSP_WAKEUP_SPM             (0x1 << 0)

#define ADSP_SEMAPHORE              (ADSP_BASE + 0x0064)

#define ADSP_B_WDT_REG              (ADSP_BASE + 0x0068)
#define ADSP_B_WDT_INIT_VALUE       (ADSP_BASE + 0x006C)
#define ADSP_B_WDT_CNT              (ADSP_BASE + 0x0070)

#define ADSP_A_WDT_REG              (ADSP_BASE + 0x007C)
#define ADSP_A_WDT_INIT_VALUE       (ADSP_BASE + 0x0080)
#define ADSP_A_WDT_CNT              (ADSP_BASE + 0x0084)
#define ADSP_WDT_TRIGGER            (ADSP_A_WDT_INIT_VALUE)
#define WDT_EN_BIT  (1 << 31)
#define WDT_DIS_BIT (0 << 31)
#define WDT_KICK_BIT 0

#define ADSP_CFGREG_RSV_RW_REG0     (ADSP_BASE + 0x008C)
#define ADSP_CFGREG_RSV_RW_REG1     (ADSP_BASE + 0x0090)

/* Latch Debug info after WDT */
#define ADSP_A_WDT_DEBUG_PC_REG     (ADSP_BASE + 0x0170)
#define ADSP_A_WDT_DEBUG_SP_REG     (ADSP_BASE + 0x0174)
/* TODO : add in aee dump */
#define ADSP_A_WDT_EXCVADDR_REG     (ADSP_BASE + 0x0178)
/* TODO : add in aee dump */
#define ADSP_A_WDT_EXCCAUSE_REG     (ADSP_BASE + 0x017C)

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

/* adsp power state*/
#define ADSP_A_IS_RESET             (0x00)
#define ADSP_A_IS_ACTIVE            (0x10)
/* adsp current state */
#define ADSP_A_CUR_RESET             (0x0)
#define ADSP_A_CUR_STALL             (0x1)
#define ADSP_A_CUR_ACTIVE            (0x3)
#define ADSP_A_CUR_WFI               (0x4)
#define ADSP_A_CUR_WAKEUP            (0x5) //Wakeup state (Receive irq)

#define ADSP_A_IS_WFI               (1 << 0)
#define ADSP_B_IS_WFI               (1 << 1)
#define ADSP_AXI_BUS_IS_IDLE        (1 << 2)

/* clk reg */
#define ADSP_CLK_CTRL_BASE          (ADSP_BASE + 0x1000)
#define ADSP_CLK_UART_EN            (1 << 5)
#define ADSP_CLK_DMA_EN             (1 << 4)
#define ADSP_CLK_TIMER_EN           (1 << 3)
#define ADSP_CLK_CORE_1_EN          (1 << 1)
#define ADSP_CLK_CORE_0_EN          (1 << 0)
#define ADSP_UART_CTRL              (ADSP_BASE + 0x1010)
#define ADSP_UART_RST_N             (1 << 3)
#define ADSP_UART_CLK_SEL           (1 << 1)
#define ADSP_UART_BCLK_CG           (1 << 0)

#endif
