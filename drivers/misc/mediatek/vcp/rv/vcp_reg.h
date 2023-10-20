/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_REG_H
#define __VCP_REG_H

#define VCP_SEMAPHORE			(vcpreg.cfg  + 0x0018)
#define VCP_VCP2SPM_VOL_LV		(vcpreg.cfg + 0x0020)

/* VCP to SPM IPC clear */
#define VCP_TO_SPM_REG			(vcpreg.cfg + 0x0094)

#define R_GIPC_IN_SET			(vcpreg.cfg + 0x0098)
#define R_GIPC_IN_CLR			(vcpreg.cfg + 0x009c)
	#define B_GIPC0_SETCLR_0	(1 << 0)
	#define B_GIPC0_SETCLR_1	(1 << 1)
	#define B_GIPC0_SETCLR_2	(1 << 2)
	#define B_GIPC0_SETCLR_3	(1 << 3)
	#define B_GIPC1_SETCLR_0	(1 << 4)
	#define B_GIPC1_SETCLR_1	(1 << 5)
	#define B_GIPC1_SETCLR_2	(1 << 6)
	#define B_GIPC1_SETCLR_3	(1 << 7)
	#define B_GIPC2_SETCLR_0	(1 << 8)
	#define B_GIPC2_SETCLR_1	(1 << 9)
	#define B_GIPC2_SETCLR_2	(1 << 10)
	#define B_GIPC2_SETCLR_3	(1 << 11)
	#define B_GIPC3_SETCLR_0	(1 << 12)
	#define B_GIPC3_SETCLR_1	(1 << 13)
	#define B_GIPC3_SETCLR_2	(1 << 14)
	#define B_GIPC3_SETCLR_3	(1 << 15)
	#define B_GIPC4_SETCLR_0	(1 << 16)
	#define B_GIPC4_SETCLR_1	(1 << 17)
	#define B_GIPC4_SETCLR_2	(1 << 18)
	#define B_GIPC4_SETCLR_3	(1 << 19)

#define VCP_BUS_DEBUG_OUT		(vcpreg.cfg + 0x0150)

#define R_CORE0_SW_RSTN_CLR	(vcpreg.cfg_core0 + 0x0000)
#define R_CORE1_SW_RSTN_CLR	(vcpreg.cfg_core1 + 0x0000)

#define R_CORE0_SW_RSTN_SET	(vcpreg.cfg_core0 + 0x0004)
#define R_CORE1_SW_RSTN_SET	(vcpreg.cfg_core1 + 0x0004)

#define R_CORE0_DBG_CTRL	(vcpreg.cfg_core0 + 0x0010)
#define R_CORE1_DBG_CTRL	(vcpreg.cfg_core1 + 0x0010)
	#define M_CORE_TBUF_DBG_SEL	(7 << 4)
	#define S_CORE_TBUF_DBG_SEL	(4)
	#define M_CORE_TBUF_DBG_SEL_RV55	(0xfff0ff0f)
	#define S_CORE_TBUF_S		(4)
	#define S_CORE_TBUF1_S		(16)
#define R_CORE0_WDT_IRQ		(vcpreg.cfg_core0 + 0x0030)
#define R_CORE1_WDT_IRQ		(vcpreg.cfg_core1 + 0x0030)
	#define B_WDT_IRQ	(1 << 0)

#define R_CORE0_WDT_CFG		(vcpreg.cfg_core0 + 0x0034)
#define R_CORE1_WDT_CFG		(vcpreg.cfg_core1 + 0x0034)
	#define V_INSTANT_WDT	0x80000000

#define R_CORE0_STATUS			(vcpreg.cfg_core0 + 0x0070)
	#define B_CORE_GATED		(1 << 0)
	#define B_HART0_HALT		(1 << 1)
	#define B_HART1_HALT		(1 << 2)
#define R_CORE0_MON_PC			(vcpreg.cfg_core0 + 0x0080)
#define R_CORE0_MON_LR			(vcpreg.cfg_core0 + 0x0084)
#define R_CORE0_MON_SP			(vcpreg.cfg_core0 + 0x0088)
#define R_CORE0_TBUF_WPTR		(vcpreg.cfg_core0 + 0x008c)

#define R_CORE0_MON_PC_LATCH		(vcpreg.cfg_core0 + 0x00d0)
#define R_CORE0_MON_LR_LATCH		(vcpreg.cfg_core0 + 0x00d4)
#define R_CORE0_MON_SP_LATCH		(vcpreg.cfg_core0 + 0x00d8)

#define R_CORE0_T1_MON_PC		(vcpreg.cfg_core0 + 0x0160)
#define R_CORE0_T1_MON_LR		(vcpreg.cfg_core0 + 0x0164)
#define R_CORE0_T1_MON_SP		(vcpreg.cfg_core0 + 0x0168)

#define R_CORE0_T1_MON_PC_LATCH		(vcpreg.cfg_core0 + 0x0170)
#define R_CORE0_T1_MON_LR_LATCH		(vcpreg.cfg_core0 + 0x0174)
#define R_CORE0_T1_MON_SP_LATCH		(vcpreg.cfg_core0 + 0x0178)

#define R_CORE1_STATUS			(vcpreg.cfg_core1 + 0x0070)
#define R_CORE1_MON_PC			(vcpreg.cfg_core1 + 0x0080)
#define R_CORE1_MON_LR			(vcpreg.cfg_core1 + 0x0084)
#define R_CORE1_MON_SP			(vcpreg.cfg_core1 + 0x0088)
#define R_CORE1_TBUF_WPTR		(vcpreg.cfg_core1 + 0x008c)

#define R_CORE1_MON_PC_LATCH		(vcpreg.cfg_core1 + 0x00d0)
#define R_CORE1_MON_LR_LATCH		(vcpreg.cfg_core1 + 0x00d4)
#define R_CORE1_MON_SP_LATCH		(vcpreg.cfg_core1 + 0x00d8)

#define R_CORE1_T1_MON_PC		(vcpreg.cfg_core1 + 0x0160)
#define R_CORE1_T1_MON_LR		(vcpreg.cfg_core1 + 0x0164)
#define R_CORE1_T1_MON_SP		(vcpreg.cfg_core1 + 0x0168)

#define R_CORE1_T1_MON_PC_LATCH		(vcpreg.cfg_core1 + 0x0170)
#define R_CORE1_T1_MON_LR_LATCH		(vcpreg.cfg_core1 + 0x0174)
#define R_CORE1_T1_MON_SP_LATCH		(vcpreg.cfg_core1 + 0x0178)

#define R_CORE0_TBUF_DATA31_0		(vcpreg.cfg_core0 + 0x00e0)
#define R_CORE0_TBUF_DATA63_32		(vcpreg.cfg_core0 + 0x00e4)
#define R_CORE0_TBUF_DATA95_64		(vcpreg.cfg_core0 + 0x00e8)
#define R_CORE0_TBUF_DATA127_96		(vcpreg.cfg_core0 + 0x00ec)

#define R_CORE0_TBUF1_DATA31_0		(vcpreg.cfg_core0 + 0x00f0)
#define R_CORE0_TBUF1_DATA63_32		(vcpreg.cfg_core0 + 0x00f4)
#define R_CORE0_TBUF1_DATA95_64		(vcpreg.cfg_core0 + 0x00f8)
#define R_CORE0_TBUF1_DATA127_96	(vcpreg.cfg_core0 + 0x00fc)

#define R_CORE1_TBUF_DATA31_0		(vcpreg.cfg_core1 + 0x00e0)
#define R_CORE1_TBUF_DATA63_32		(vcpreg.cfg_core1 + 0x00e4)
#define R_CORE1_TBUF_DATA95_64		(vcpreg.cfg_core1 + 0x00e8)
#define R_CORE1_TBUF_DATA127_96		(vcpreg.cfg_core1 + 0x00ec)

#define R_CORE1_TBUF1_DATA31_0		(vcpreg.cfg_core1 + 0x00f0)
#define R_CORE1_TBUF1_DATA63_32		(vcpreg.cfg_core1 + 0x00f4)
#define R_CORE1_TBUF1_DATA95_64		(vcpreg.cfg_core1 + 0x00f8)
#define R_CORE1_TBUF1_DATA127_96	(vcpreg.cfg_core1 + 0x00fc)

#define VCP_A_GENERAL_REG0       (vcpreg.cfg_core0 + 0x0040)
/* DRAM reserved address and size */
#define VCP_A_GENERAL_REG1       (vcpreg.cfg_core0 + 0x0044)
#define DRAM_RESV_ADDR_REG	 VCP_A_GENERAL_REG1
#define VCP_A_GENERAL_REG2       (vcpreg.cfg_core0 + 0x0048)
#define DRAM_RESV_SIZE_REG	 VCP_A_GENERAL_REG2
/*EXPECTED_FREQ_REG*/
#define VCP_A_GENERAL_REG3       (vcpreg.cfg_core0 + 0x004C)
#define EXPECTED_FREQ_REG        VCP_A_GENERAL_REG3
/*CURRENT_FREQ_REG*/
#define VCP_A_GENERAL_REG4       (vcpreg.cfg_core0 + 0x0050)
#define CURRENT_FREQ_REG         VCP_A_GENERAL_REG4
/*VCP_GPR_CM4_A_REBOOT*/
#define VCP_A_GENERAL_REG5		(vcpreg.cfg_core0 + 0x0054)
#define VCP_GPR_CORE0_REBOOT		VCP_A_GENERAL_REG5
	#define CORE_RDY_TO_REBOOT	0x34
	#define CORE_REBOOT_OK		0x1
#define VCP_GPR_CORE1_REBOOT		(vcpreg.cfg_core1 + 0x0054)

#define VCP_A_GENERAL_REG6		(vcpreg.cfg_core0 + 0x0058)
#define VCP_A_GENERAL_REG7		(vcpreg.cfg_core0 + 0x005C)

/* bus tracker reg */
#define VCP_BUS_DBG_CON			(vcpreg.bus_tracker)
#define VCP_BUS_DBG_AR_TRACK0_L		(vcpreg.bus_tracker + 0x100)
#define VCP_BUS_DBG_AR_TRACK1_L		(vcpreg.bus_tracker + 0x108)
#define VCP_BUS_DBG_AR_TRACK2_L		(vcpreg.bus_tracker + 0x110)
#define VCP_BUS_DBG_AR_TRACK3_L		(vcpreg.bus_tracker + 0x118)
#define VCP_BUS_DBG_AR_TRACK4_L		(vcpreg.bus_tracker + 0x120)
#define VCP_BUS_DBG_AR_TRACK5_L		(vcpreg.bus_tracker + 0x128)
#define VCP_BUS_DBG_AR_TRACK6_L		(vcpreg.bus_tracker + 0x130)
#define VCP_BUS_DBG_AR_TRACK7_L		(vcpreg.bus_tracker + 0x138)

#define VCP_BUS_DBG_AW_TRACK0_L		(vcpreg.bus_tracker + 0x300)
#define VCP_BUS_DBG_AW_TRACK1_L		(vcpreg.bus_tracker + 0x308)
#define VCP_BUS_DBG_AW_TRACK2_L		(vcpreg.bus_tracker + 0x310)
#define VCP_BUS_DBG_AW_TRACK3_L		(vcpreg.bus_tracker + 0x318)
#define VCP_BUS_DBG_AW_TRACK4_L		(vcpreg.bus_tracker + 0x320)
#define VCP_BUS_DBG_AW_TRACK5_L		(vcpreg.bus_tracker + 0x328)
#define VCP_BUS_DBG_AW_TRACK6_L		(vcpreg.bus_tracker + 0x330)
#define VCP_BUS_DBG_AW_TRACK7_L		(vcpreg.bus_tracker + 0x338)

#define R_SEC_CTRL			(vcpreg.cfg_sec + 0x0000)
	#define B_CORE0_CACHE_DBG_EN	(1 << 28)
	#define B_CORE1_CACHE_DBG_EN	(1 << 29)

#define R_CORE0_CACHE_RAM		(vcpreg.l1cctrl + 0x00000)
#define R_CORE1_CACHE_RAM		(vcpreg.l1cctrl + 0x20000)

#define VCP_GCE_MMU			(vcpreg.cfg_mmu + 0x0000)
	#define B_MMU_EN		(0x3 << 14)

#endif
