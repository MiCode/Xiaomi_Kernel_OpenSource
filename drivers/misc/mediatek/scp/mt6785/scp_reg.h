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

/*#define SCP_BASE						(scpreg.cfg)*/
#define SCP_AP_RESOURCE		(scpreg.cfg + 0x0004)
#define SCP_BUS_RESOURCE	(scpreg.cfg + 0x0008)

#define SCP_A_TO_HOST_REG			(scpreg.cfg + 0x001C)
	#define SCP_IRQ_SCP2HOST     (1 << 0)
	#define SCP_IRQ_WDT          (1 << 8)

#define SCP_TO_SPM_REG           (scpreg.cfg + 0x0020)

#define SCP_GIPC_IN_REG					(scpreg.cfg + 0x0028)
	#define HOST_TO_SCP_A       (1 << 0)
	#define HOST_TO_SCP_B       (1 << 1)
	/* scp awake lock definition*/
	#define SCP_A_IPI_AWAKE_NUM		(2)
	#define SCP_B_IPI_AWAKE_NUM		(3)


#define SCP_A_DEBUG_PC_REG       (scpreg.cfg + 0x00B4)
#define SCP_A_DEBUG_PSP_REG      (scpreg.cfg + 0x00B0)
#define SCP_A_DEBUG_LR_REG       (scpreg.cfg + 0x00AC)
#define SCP_A_DEBUG_SP_REG       (scpreg.cfg + 0x00A8)
#define SCP_A_WDT_REG            (scpreg.cfg + 0x0084)

#define SCP_A_GENERAL_REG0       (scpreg.cfg + 0x0050)
#define SCP_A_GENERAL_REG1       (scpreg.cfg + 0x0054)
#define SCP_A_GENERAL_REG2       (scpreg.cfg + 0x0058)
/*EXPECTED_FREQ_REG*/
#define SCP_A_GENERAL_REG3       (scpreg.cfg + 0x005C)
#define EXPECTED_FREQ_REG        (scpreg.cfg  + 0x5C)
/*CURRENT_FREQ_REG*/
#define SCP_A_GENERAL_REG4       (scpreg.cfg + 0x0060)
#define CURRENT_FREQ_REG         (scpreg.cfg  + 0x60)
/*SCP_GPR_CM4_A_REBOOT*/
#define SCP_A_GENERAL_REG5       (scpreg.cfg + 0x0064)
#define SCP_GPR_CM4_A_REBOOT     (scpreg.cfg + 0x64)
	#define CM4_A_READY_TO_REBOOT  0x34
	#define CM4_A_REBOOT_OK        0x1
#define SCP_A_GENERAL_REG6       (scpreg.cfg + 0x0068)
#define SCP_A_GENERAL_REG7       (scpreg.cfg + 0x006C)

#define SCP_SEMAPHORE	         (scpreg.cfg  + 0x90)
#define SCP_SCP2SPM_VOL_LV		 (scpreg.cfg + 0x0094)

#define SCP_SLP_PROTECT_CFG			(scpreg.cfg + 0x00C8)

#define SCP_WDT_SP					(scpreg.cfg + 0x00B8)
#define SCP_WDT_LR					(scpreg.cfg + 0x00BC)
#define SCP_WDT_PSP					(scpreg.cfg + 0x00C0)
#define SCP_WDT_PC					(scpreg.cfg + 0x00C4)
#define SCP_BUS_CTRL				(scpreg.cfg + 0x00F0)
	#define dbg_irq_info_sel_shift 26
	#define dbg_irq_info_sel_mask (0x3 << 26)
#define SCP_DEBUG_ADDR_S2R			(scpreg.cfg + 0x00F4)
#define SCP_DEBUG_ADDR_DMA			(scpreg.cfg + 0x00F8)
#define SCP_DEBUG_ADDR_SPI0			(scpreg.cfg + 0x00FC)
#define SCP_DEBUG_ADDR_SPI1			(scpreg.cfg + 0x0100)
#define SCP_DEBUG_ADDR_SPI2			(scpreg.cfg + 0x0104)
#define SCP_DEBUG_BUS_STATUS		(scpreg.cfg + 0x0110)

#define SCP_CPU_SLEEP_STATUS			(scpreg.cfg + 0x0114)
	#define SCP_A_DEEP_SLEEP_BIT	(1)
	#define SCP_B_DEEP_SLEEP_BIT	(3)

#define SCP_SLEEP_STATUS_REG     (scpreg.cfg + 0x0114)
	#define SCP_A_IS_SLEEP          (1<<0)
	#define SCP_A_IS_DEEPSLEEP      (1<<1)
	#define SCP_B_IS_SLEEP          (1<<2)
	#define SCP_B_IS_DEEPSLEEP      (1<<3)

#define INFRA_CTRL_STATUS		(scpreg.cfg + 0x011C)
#define SCP_DEBUG_IRQ_INFO		(scpreg.cfg + 0x0160)

/* clk reg*/
#define SCP_CLK_CTRL_BASE			(scpreg.clkctrl)
#define SCP_CLK_SW_SEL				(scpreg.clkctrl)
#define SCP_CLK_ENABLE				(scpreg.clkctrl + 0x0004)
#define SCP_A_SLEEP_DEBUG_REG		(scpreg.clkctrl + 0x0028)
#define SCP_SRAM_PDN				(scpreg.clkctrl + 0x002C)
#define SCP_CLK_HIGH_CORE_CG		(scpreg.clkctrl + 0x005C)
#define SCP_CLK_CTRL_L1_SRAM_PD		(scpreg.clkctrl + 0x0080)
#define SCP_CLK_CTRL_TCM_TAIL_SRAM_PD	(scpreg.clkctrl + 0x0094)

/* SCP INTC register*/
#define SCP_INTC_IRQ_STATUS		(scpreg.cfg + 0x2000)
#define SCP_INTC_IRQ_ENABLE		(scpreg.cfg + 0x2004)
#define SCP_INTC_IRQ_SLEEP		(scpreg.cfg + 0x200C)
#define SCP_INTC_IRQ_STATUS_MSB		(scpreg.cfg + 0x2080)
#define SCP_INTC_IRQ_ENABLE_MSB		(scpreg.cfg + 0x2084)
#define SCP_INTC_IRQ_SLEEP_MSB		(scpreg.cfg + 0x208C)

/* SCP System Reset */
#define MODULE_RESET_SET		(scpreg.scpsys + 0x0140)
#define MODULE_RESET_CLR		(scpreg.scpsys + 0x0144)
#define MODULE_RESET_STATUS		(scpreg.scpsys + 0x0148)
	#define SCP_RESET_BIT			(1 << 3)
	#define SCP_SEC_RESET_BIT		(1 << 10)

/* INFRA Sleep Protect */
#define INFRA_SLP_PROT_SET		(scpreg.scpsys + 0x220)
#define INFRA_SLP_PROT_STAT		(scpreg.scpsys + 0x224)
	#define SCP_TO_INFRA_BIT		(1 << 30)
	#define SCP_TO_AUDIO_BIT		(1 << 31)

/* INFRA_IRQ */
#define INFRA_IRQ_SET			(scpreg.scpsys + 0x0B14)
	#define AP_AWAKE_LOCK		(0)
	#define AP_AWAKE_UNLOCK		(1)
	#define CONNSYS_AWAKE_LOCK	(2)
	#define CONNSYS_AWAKE_UNLOCK	(3)
#define INFRA_IRQ_CLEAR			(scpreg.scpsys + 0x0B18)

#define SCP_SYS_INFRA_MON       (scpreg.scpsys + 0x0D50)

#endif
