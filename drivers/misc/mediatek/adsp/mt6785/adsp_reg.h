/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __ADSP_REG_H
#define __ADSP_REG_H

/*#define ADSP_BASE                     (adspreg.cfg)*/
#define ADSP_A_REBOOT               (adspreg.cfg + 0x00)
#define ADSP_A_SW_RSTN              (1 << 0)
#define ADSP_A_SW_DBG_RSTN          (1 << 4)
#define ADSP_A_IO_CONFIG            (adspreg.cfg + 0x0008)
#define ADSP_HIFI3_IO_CONFIG        ADSP_A_IO_CONFIG
#define ADSP_RELEASE_RUNSTALL       (1 << 31)

#define ADSP_SWINT_REG              (adspreg.cfg + 0x001C) //sw int set
#define ADSP_SW_INT0                (0)
#define ADSP_SW_INT1                (1)
#define ADSP_SW_INT2                (2)
#define ADSP_SW_INT3                (3)
#define HOST_TO_ADSP_A              (1 << 0)

#define ADSP_A_TO_HOST_REG          (adspreg.cfg + 0x0038) //syscirq_clr
#define ADSP_IRQ_ADSP2HOST          (1 << 0)

#define ADSP_A_DVFSRC_STATE         (adspreg.cfg + 0x003C)
#define ADSP_A_DVFSRC_REQ           (adspreg.cfg + 0x0040)
#define ADSP_A_DDREN_REQ            (adspreg.cfg + 0x0044)
#define ADSP_A_DDR_REQ_SEL          (0x3 << 4)             // hw auto ddren
#define ADSP_A_DDR_ENABLE           (1 << 0)
#define ADSP_A_SPM_REQ              (adspreg.cfg + 0x0048)
#define ADSP_A_SPM_ACK              (adspreg.cfg + 0x004C)
#define ADSP_A_SPM_SRC_BITS         (0xF << 0)
#define ADSP_A_IRQ_EN               (adspreg.cfg + 0x0050)

#define ADSP_TO_SPM_REG             (adspreg.cfg + 0x005C) //spm wakeup

#define ADSP_SEMAPHORE              (adspreg.cfg + 0x0058)

#define ADSP_A_WDT_REG              (adspreg.cfg + 0x007C)
#define ADSP_A_WDT_INIT_VALUE       (adspreg.cfg + 0x0080)
#define ADSP_A_WDT_CNT              (adspreg.cfg + 0x0084)
#define ADSP_WDT_TRIGGER            (ADSP_A_WDT_INIT_VALUE)
#define WDT_EN_BIT  (1 << 31)
#define WDT_DIS_BIT (0 << 31)
#define WDT_KICK_BIT 0

#define ADSP_CFGREG_RSV_RW_REG0     (adspreg.cfg + 0x008C)
#define ADSP_CFGREG_RSV_RW_REG1     (adspreg.cfg + 0x0090)

/* Latch Debug info after WDT */
#define ADSP_A_WDT_DEBUG_PC_REG     (adspreg.cfg + 0x0170)
#define ADSP_A_WDT_DEBUG_SP_REG     (adspreg.cfg + 0x0174)
/* TODO : add in aee dump */
#define ADSP_A_WDT_EXCVADDR_REG     (adspreg.cfg + 0x0178)
/* TODO : add in aee dump */
#define ADSP_A_WDT_EXCCAUSE_REG     (adspreg.cfg + 0x017C)

/* Wakeup ADSP interrupt */
#define ADSP_WAKEUPSRC_MASK         (adspreg.cfg + 0x00A8)
#define ADSP_WAKEUPSRC_IRQ          (adspreg.cfg + 0x00AC)

/*latency monitor*/
#define ADSP_LATMON_DVFS_MODE       (adspreg.cfg + 0x0100)
#define ADSP_LATMON_CON1            (adspreg.cfg + 0x0104)
#define ADSP_LATMON_CON2            (adspreg.cfg + 0x0108)
#define ADSP_LATMON_MARGIN          (adspreg.cfg + 0x010C)
#define ADSP_LATMON_THRESHOLD       (adspreg.cfg + 0x0110)
#define ADSP_LATMON_STATE           (adspreg.cfg + 0x0114)
#define ADSP_LATMON_ACCCNT          (adspreg.cfg + 0x0118)
/* latency threshold & budget for 0.8V. 0.7V. 0.625V */
#define ADSP_LATMON_CONT0           (adspreg.cfg + 0x012C)
#define ADSP_LATMON_CONT1           (adspreg.cfg + 0x0130)
#define ADSP_LATMON_CONT2           (adspreg.cfg + 0x0134)


#define ADSP_A_DEBUG_PC_REG         (adspreg.cfg + 0x013C)
#define ADSP_DBG_PEND_CNT           (adspreg.cfg + 0x015C)
#define ADSP_SLEEP_STATUS_REG       (adspreg.cfg + 0x0158)
#define ADSP_BUS_MON_BASE           (adspreg.cfg + 0x5000)

/* adsp power state*/
#define ADSP_A_IS_RESET             (0x00)
#define ADSP_A_IS_ACTIVE            (0x10)
#define ADSP_A_IS_WFI               (0x20)
/* adsp current state */
#define ADSP_A_CUR_RESET             (0x0)
#define ADSP_A_CUR_STALL             (0x1)
#define ADSP_A_CUR_ACTIVE            (0x3)
#define ADSP_A_CUR_WFI               (0x4)
#define ADSP_A_CUR_WAKEUP            (0x5) //Wakeup state (Receive irq)

#define ADSP_A_AXI_BUS_IS_IDLE      (1 << 1)

/* clk reg */
#define ADSP_CLK_CTRL_OFFSET        (0x1000)
#define ADSP_CLK_CTRL_BASE          (adspreg.clkctrl)
#define ADSP_CLK_UART_EN            (1 << 5)
#define ADSP_CLK_DMA_EN             (1 << 4)
#define ADSP_CLK_TIMER_EN           (1 << 3)
#define ADSP_UART_CTRL              (adspreg.clkctrl + 0x0010)
#define ADSP_UART_RST_N             (1 << 3)
#define ADSP_UART_CLK_SEL           (1 << 1)
#define ADSP_UART_BCLK_CG           (1 << 0)


#define ADSP_A_SLEEP_DEBUG_REG      (adspreg.clkctrl + 0x0028)
#define ADSP_CLK_HIGH_CORE_CG       (adspreg.clkctrl + 0x005C)

/* INFRA_IRQ (always on register) */
#define AP_AWAKE_LOCK_BIT           (0)
#define AP_AWAKE_UNLOCK_BIT         (1)
#define CONNSYS_AWAKE_LOCK          (2)
#define CONNSYS_AWAKE_UNLOCK        (3)
#define AP_AWAKE_DUMP_BIT           (4)
#define AP_AWAKE_UPDATE_BIT         (5)
#define AP_AWAKE_STATE_BIT          (6)
#define ADSPPLL_UNLOCK_BIT          (8)

#define ADSP_ADSP2SPM_VOL_LV         (adspreg.cfg + 0x0094)

#endif
