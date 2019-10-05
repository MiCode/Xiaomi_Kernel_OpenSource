/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __SCP_REG_H
#define __SCP_REG_H

#define R_CORE0_SW_RSTN_CLR	(scpreg.cfg_core0 + 0x0000)
#define R_CORE1_SW_RSTN_CLR	(scpreg.cfg_core1 + 0x0000)

#define R_CORE0_SW_RSTN_SET	(scpreg.cfg_core0 + 0x0004)
#define R_CORE1_SW_RSTN_SET	(scpreg.cfg_core1 + 0x0004)

#define R_CORE0_WDT_IRQ		(scpreg.cfg_core0 + 0x0030)
#define R_CORE1_WDT_IRQ		(scpreg.cfg_core1 + 0x0030)
	#define B_WDT_IRQ	(1 << 0)

#define SCP_TO_SPM_REG           (scpreg.cfg + 0x0020)

#define SCP_GIPC_IN_REG		(scpreg.cfg + 0x0028)
	#define HOST_TO_SCP_A       (1 << 0)
	#define HOST_TO_SCP_B       (1 << 1)
	/* scp awake lock definition*/
	#define SCP_A_IPI_AWAKE_NUM		(2)
	#define SCP_B_IPI_AWAKE_NUM		(3)

#define R_CORE0_STATUS			(scpreg.cfg_core0 + 0x0070)
	#define B_CORE_GATED		(1 << 0)
	#define B_CORE_HALT		(1 << 1)
#define R_CORE0_MON_PC			(scpreg.cfg_core0 + 0x0080)
#define R_CORE0_MON_LR			(scpreg.cfg_core0 + 0x0084)
#define R_CORE0_MON_SP			(scpreg.cfg_core0 + 0x0088)
#define R_CORE0_MON_PC_LATCH		(scpreg.cfg_core0 + 0x00d0)
#define R_CORE0_MON_LR_LATCH		(scpreg.cfg_core0 + 0x00d4)
#define R_CORE0_MON_SP_LATCH		(scpreg.cfg_core0 + 0x00d8)

#define R_CORE1_STATUS			(scpreg.cfg_core1 + 0x0070)
#define R_CORE1_MON_PC			(scpreg.cfg_core1 + 0x0080)
#define R_CORE1_MON_LR			(scpreg.cfg_core1 + 0x0084)
#define R_CORE1_MON_SP			(scpreg.cfg_core1 + 0x0088)
#define R_CORE1_MON_PC_LATCH		(scpreg.cfg_core1 + 0x00d0)
#define R_CORE1_MON_LR_LATCH		(scpreg.cfg_core1 + 0x00d4)
#define R_CORE1_MON_SP_LATCH		(scpreg.cfg_core1 + 0x00d8)


#define SCP_A_DEBUG_PC_REG       (scpreg.cfg + 0x00B4)
#define SCP_A_DEBUG_PSP_REG      (scpreg.cfg + 0x00B0)
#define SCP_A_DEBUG_LR_REG       (scpreg.cfg + 0x00AC)
#define SCP_A_DEBUG_SP_REG       (scpreg.cfg + 0x00A8)
#define SCP_A_WDT_REG            (scpreg.cfg + 0x0084)

#define SCP_A_GENERAL_REG0       (scpreg.cfg_core0 + 0x0040)
/* DRAM reserved address and size */
#define SCP_A_GENERAL_REG1       (scpreg.cfg_core0 + 0x0044)
#define DRAM_RESV_ADDR_REG	 SCP_A_GENERAL_REG1
#define SCP_A_GENERAL_REG2       (scpreg.cfg_core0 + 0x0048)
#define DRAM_RESV_SIZE_REG	 SCP_A_GENERAL_REG2
/*EXPECTED_FREQ_REG*/
#define SCP_A_GENERAL_REG3       (scpreg.cfg_core0 + 0x004C)
#define EXPECTED_FREQ_REG        (scpreg.cfg_core0  + 0x4C)
/*CURRENT_FREQ_REG*/
#define SCP_A_GENERAL_REG4       (scpreg.cfg_core0 + 0x0050)
#define CURRENT_FREQ_REG         (scpreg.cfg_core0  + 0x50)
/*SCP_GPR_CM4_A_REBOOT*/
#define SCP_A_GENERAL_REG5		(scpreg.cfg_core0 + 0x0054)
#define SCP_GPR_CORE0_REBOOT		(scpreg.cfg_core0 + 0x54)
	#define CORE_RDY_TO_REBOOT	0x34
	#define CORE_REBOOT_OK		0x1
#define SCP_A_GENERAL_REG6		(scpreg.cfg_core0 + 0x0058)
#define SCP_A_GENERAL_REG7		(scpreg.cfg_core0 + 0x005C)

#define SCP_GPR_CORE1_REBOOT		(scpreg.cfg_core1 + 0x54)

#define SCP_SEMAPHORE			(scpreg.cfg  + 0x90)
#define SCP_SCP2SPM_VOL_LV		 (scpreg.cfg + 0x0094)

#define SCP_SLP_PROTECT_CFG		(scpreg.cfg + 0x00C8)

#define SCP_WDT_SP			(scpreg.cfg + 0x00B8)
#define SCP_WDT_LR			(scpreg.cfg + 0x00BC)
#define SCP_WDT_PSP			(scpreg.cfg + 0x00C0)
#define SCP_WDT_PC			(scpreg.cfg + 0x00C4)
#define SCP_BUS_CTRL			(scpreg.cfg + 0x00F0)
	#define dbg_irq_info_sel_shift 26
	#define dbg_irq_info_sel_mask (0x3 << 26)
#define SCP_DEBUG_ADDR_S2R		(scpreg.cfg + 0x00F4)
#define SCP_DEBUG_ADDR_DMA		(scpreg.cfg + 0x00F8)
#define SCP_DEBUG_ADDR_SPI0		(scpreg.cfg + 0x00FC)
#define SCP_DEBUG_ADDR_SPI1		(scpreg.cfg + 0x0100)
#define SCP_DEBUG_ADDR_SPI2		(scpreg.cfg + 0x0104)
#define SCP_DEBUG_BUS_STATUS		(scpreg.cfg + 0x0110)

#define SCP_CPU_SLEEP_STATUS		(scpreg.cfg + 0x0114)
	#define SCP_A_DEEP_SLEEP_BIT	(1)
	#define SCP_B_DEEP_SLEEP_BIT	(3)


#define INFRA_CTRL_STATUS		(scpreg.cfg + 0x011C)
#define SCP_DEBUG_IRQ_INFO		(scpreg.cfg + 0x0160)

/* clk reg*/
#define SCP_CLK_CTRL_BASE		(scpreg.clkctrl)
#define SCP_CLK_SW_SEL			(scpreg.clkctrl)
#define SCP_CLK_ENABLE			(scpreg.clkctrl + 0x0004)
#define SCP_A_SLEEP_DEBUG_REG		(scpreg.clkctrl + 0x0028)
#define SCP_SRAM_PDN				(scpreg.clkctrl + 0x002C)
#define SCP_CLK_HIGH_CORE_CG		(scpreg.clkctrl + 0x005C)
#define SCP_CLK_CTRL_L1_SRAM_PD		(scpreg.clkctrl + 0x0080)
#define SCP_CLK_CTRL_TCM_TAIL_SRAM_PD	(scpreg.clkctrl + 0x0094)

/* SCP System Reset */
#define MODULE_RESET_SET		(scpreg.scpsys + 0x0140)
#define MODULE_RESET_CLR		(scpreg.scpsys + 0x0144)
#define MODULE_RESET_STATUS		(scpreg.scpsys + 0x0148)
    #define SCP_RESET_BIT		(1 << 3)
    #define SCP_SEC_RESET_BIT		(1 << 10)
/* SCP INTC register*/
#define SCP_INTC_IRQ_STATUS		(scpreg.cfg + 0x2000)
#define SCP_INTC_IRQ_ENABLE		(scpreg.cfg + 0x2004)
#define SCP_INTC_IRQ_SLEEP		(scpreg.cfg + 0x200C)
#define SCP_INTC_IRQ_STATUS_MSB		(scpreg.cfg + 0x2080)
#define SCP_INTC_IRQ_ENABLE_MSB		(scpreg.cfg + 0x2084)
#define SCP_INTC_IRQ_SLEEP_MSB		(scpreg.cfg + 0x208C)

#define R_SEC_CTRL			(scpreg.cfg_sec + 0x0000)
	#define B_CORE0_CACHE_DBG_EN	(1 << 28)
	#define B_CORE1_CACHE_DBG_EN	(1 << 29)

#define R_CORE0_CACHE_RAM		(scpreg.l1cctrl + 0x00000)
#define R_CORE1_CACHE_RAM		(scpreg.l1cctrl + 0x20000)

/* INFRA_IRQ (always on register) */
#define INFRA_IRQ_SET			(scpreg.scpsys + 0x0B14)
	#define AP_AWAKE_LOCK		(0)
	#define AP_AWAKE_UNLOCK		(1)
	#define CONNSYS_AWAKE_LOCK	(2)
	#define CONNSYS_AWAKE_UNLOCK	(3)
#define INFRA_IRQ_CLEAR			(scpreg.scpsys + 0x0B18)
#define SCP_SYS_INFRA_MON		(scpreg.scpsys + 0x0D50)

#endif
