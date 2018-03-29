/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <aee.h>
#include <linux/timer.h>
/* #include <asm/system.h> */
#include <asm-generic/irq_regs.h>
/* #include <asm/mach/map.h> */
#include <sync_write.h>
/*#include <mach/irqs.h>*/
#include <asm/cacheflush.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/fb.h>
#include <linux/debugfs.h>
#include <m4u.h>
#include <mt_smi.h>

#include "smi_common.h"
#include "smi_reg.h"
#include "smi_debug.h"
#include "smi_configuration.h"

#define SMI_LOG_TAG "smi"

#if !defined(CONFIG_MTK_CLKMGR)
#define SMI_INTERNAL_CCF_SUPPORT
#endif

#if !defined(SMI_INTERNAL_CCF_SUPPORT)
#include <mach/mt_clkmgr.h>
#endif


/* Debug Function */
static void smi_dump_format(unsigned long base, unsigned int from, unsigned int to);
static void smi_dumpper(int output_gce_buffer, unsigned long *offset, unsigned long base, int reg_number)
{
	int num_of_set = 3;
	int remain_runtimes = 0;
	int runtimes = 0;
	int i = 0;

	remain_runtimes = reg_number % num_of_set;
	runtimes = reg_number / num_of_set;
	runtimes = runtimes * 3;

	do {
		SMIMSG3(output_gce_buffer, "[0x%lx,0x%lx,0x%lx]=[0x%x,0x%x,0x%x]\n",
			offset[i], offset[i + 1], offset[i + 2],
			M4U_ReadReg32(base, offset[i]), M4U_ReadReg32(base, offset[i + 1]),
			M4U_ReadReg32(base, offset[i + 2]));
		i += 3;
	} while (i < runtimes);

	switch (remain_runtimes) {
	case 2:
		SMIMSG3(output_gce_buffer, "[0x%lx,0x%lx]=[0x%x,0x%x]\n",
			offset[i], offset[i + 1],
			M4U_ReadReg32(base, offset[i]), M4U_ReadReg32(base, offset[i + 1]));
		break;
	case 1:
		SMIMSG3(output_gce_buffer, "[0x%lx]=[0x%x]\n",
			offset[i], M4U_ReadReg32(base, offset[i]));
		break;
	default:
		break;
	}
}

void smi_dumpCommonDebugMsg(int output_gce_buffer)
{
	unsigned long u4Base;

	/* No verify API in CCF, assume clk is always on */
	int smiCommonClkEnabled = 1;

#if !defined(SMI_INTERNAL_CCF_SUPPORT)
	smiCommonClkEnabled = clock_is_on(MT_CG_DISP0_SMI_COMMON);
#endif				/* !defined (SMI_INTERNAL_CCF_SUPPORT) */

	/* SMI COMMON dump */
	if ((!smiCommonClkEnabled)) {
		SMIMSG3(output_gce_buffer, "===SMI common clock is disabled===\n");
		return;
	}

	SMIMSG3(output_gce_buffer, "===SMI common reg dump, CLK: %d===\n", smiCommonClkEnabled);

	u4Base = SMI_COMMON_EXT_BASE;
	smi_dumpper(output_gce_buffer, smi_common_debug_offset, u4Base,
		    SMI_COMMON_DEBUG_OFFSET_NUM);
}

void smi_dumpLarbDebugMsg(unsigned int u4Index, int output_gce_buffer)
{
	unsigned long u4Base = 0;
	/* No verify API in CCF, assume clk is always on */
	int larbClkEnabled = 1;

	u4Base = get_larb_base_addr(u4Index);
#if !defined(SMI_INTERNAL_CCF_SUPPORT)
	larbClkEnabled = smi_larb_clock_is_on(u4Index);
#endif

	if (u4Base == SMI_ERROR_ADDR) {
		SMIMSG3(output_gce_buffer, "Doesn't support reg dump for Larb%d\n", u4Index);
		return;
	} else if ((larbClkEnabled != 0)) {
		SMIMSG3(output_gce_buffer, "===SMI LARB%d reg dump, CLK: %d===\n", u4Index,
			larbClkEnabled);

		smi_dumpper(output_gce_buffer, smi_larb_debug_offset[u4Index], u4Base,
			    smi_larb_debug_offset_num[u4Index]);

	} else {
		SMIMSG3(output_gce_buffer, "===SMI LARB%d clock is disabled===\n", u4Index);
	}

}

void smi_dumpLarb(unsigned int index)
{
	unsigned long u4Base;

	u4Base = get_larb_base_addr(index);

	if (u4Base == SMI_ERROR_ADDR) {
		SMIMSG2("Doesn't support reg dump for Larb%d\n", index);
	} else {
		SMIMSG2("===SMI LARB%d reg dump base 0x%lx===\n", index, u4Base);
		smi_dump_format(u4Base, 0, 0x434);
		smi_dump_format(u4Base, 0xF00, 0xF0C);
	}
}

void smi_dumpCommon(void)
{
	SMIMSG2("===SMI COMMON reg dump base 0x%lx===\n", SMI_COMMON_EXT_BASE);

	smi_dump_format(SMI_COMMON_EXT_BASE, 0x1A0, 0x444);
}

static void smi_dump_format(unsigned long base, unsigned int from, unsigned int to)
{
	int i, j, left;
	unsigned int value[8];

	for (i = from; i <= to; i += 32) {
		for (j = 0; j < 8; j++)
			value[j] = M4U_ReadReg32(base, i + j * 4);

		SMIMSG2("%8x %x %x %x %x %x %x %x %x\n", i, value[0], value[1],
			value[2], value[3], value[4], value[5], value[6], value[7]);
	}

	left = ((from - to) / 4 + 1) % 8;

	if (left) {
		memset(value, 0, 8 * sizeof(unsigned int));

		for (j = 0; j < left; j++)
			value[j] = M4U_ReadReg32(base, i - 32 + j * 4);

		SMIMSG2("%8x %x %x %x %x %x %x %x %x\n", i - 32 + j * 4, value[0],
			value[1], value[2], value[3], value[4], value[5], value[6], value[7]);
	}
}

void smi_dumpDebugMsg(void)
{
	unsigned int u4Index;

	/* SMI COMMON dump, 0 stands for not pass log to CMDQ error dumping messages */
	smi_dumpCommonDebugMsg(0);

	/* dump all SMI LARB */
	/* SMI Larb dump, 0 stands for not pass log to CMDQ error dumping messages */
	for (u4Index = 0; u4Index < SMI_LARB_NR; u4Index++)
		smi_dumpLarbDebugMsg(u4Index, 0);
}

int smi_debug_bus_hanging_detect(unsigned int larbs, int show_dump)
{
	return smi_debug_bus_hanging_detect_ext(larbs, show_dump, 0);
}

static int get_status_code(int smi_larb_clk_status, int smi_larb_busy_count,
			   int smi_common_busy_count)
{
	int status_code = 0;

	if (smi_larb_clk_status != 0) {
		if (smi_larb_busy_count == 5) {	/* The larb is always busy */
			if (smi_common_busy_count == 5)	/* smi common is always busy */
				status_code = 1;
			else if (smi_common_busy_count == 0)	/* smi common is always idle */
				status_code = 2;
			else
				status_code = 5;	/* smi common is sometimes busy and idle */
		} else if (smi_larb_busy_count == 0) {	/* The larb is always idle */
			if (smi_common_busy_count == 5)	/* smi common is always busy */
				status_code = 3;
			else if (smi_common_busy_count == 0)	/* smi common is always idle */
				status_code = 4;
			else
				status_code = 6;	/* smi common is sometimes busy and idle */
		} else {	/* sometime the larb is busy */
			if (smi_common_busy_count == 5)	/* smi common is always busy */
				status_code = 7;
			else if (smi_common_busy_count == 0)	/* smi common is always idle */
				status_code = 8;
			else
				status_code = 9;	/* smi common is sometimes busy and idle */
		}
	} else {
		status_code = 10;
	}
	return status_code;
}

int smi_debug_bus_hanging_detect_ext(unsigned int larbs, int show_dump, int output_gce_buffer)
{
/* output_gce_buffer = 1, write log into kernel log and CMDQ buffer. */
/* dual_buffer = 0, write log into kernel log only */
	int i = 0;
	int dump_time = 0;
	int is_smi_issue = 0;
	int status_code = 0;
	/* Keep the dump result */
	unsigned char smi_common_busy_count = 0;
	unsigned int u4Index = 0;
	unsigned long u4Base = 0;

	volatile unsigned int reg_temp = 0;
	unsigned char smi_larb_busy_count[SMI_LARB_NR] = { 0 };
	unsigned char smi_larb_mmu_status[SMI_LARB_NR] = { 0 };
	int smi_larb_clk_status[SMI_LARB_NR] = { 0 };
	/* dump resister and save resgister status */
	for (dump_time = 0; dump_time < 5; dump_time++) {
		reg_temp = M4U_ReadReg32(SMI_COMMON_EXT_BASE, 0x440);
		if ((reg_temp & (1 << 0)) == 0) {
			/* smi common is busy */
			smi_common_busy_count++;
		}
		/* Dump smi common regs */
		if (show_dump != 0)
			smi_dumpCommonDebugMsg(output_gce_buffer);

		for (u4Index = 0; u4Index < SMI_LARB_NR; u4Index++) {
			u4Base = get_larb_base_addr(u4Index);

			smi_larb_clk_status[u4Index] = smi_larb_clock_is_on(u4Index);
			/* check larb clk is enable */
			if (smi_larb_clk_status[u4Index] != 0) {
				if (u4Base != SMI_ERROR_ADDR) {
					reg_temp = M4U_ReadReg32(u4Base, 0x0);
					if (reg_temp != 0) {
						/* Larb is busy */
						smi_larb_busy_count[u4Index]++;
					}
					smi_larb_mmu_status[u4Index] = M4U_ReadReg32(u4Base, 0xa0);
					if (show_dump != 0)
						smi_dumpLarbDebugMsg(u4Index, output_gce_buffer);
				}
			}

		}

		/* Show the checked result */
		for (i = 0; i < SMI_LARB_NR; i++) {	/* Check each larb */
			if (SMI_DGB_LARB_SELECT(larbs, i)) {
				/* larb i has been selected */
				/* Get status code */
				status_code = get_status_code(smi_larb_clk_status[i], smi_larb_busy_count[i],
						smi_common_busy_count);

				/* Send the debug message according to the final result */
				switch (status_code) {
				case 1:
				case 3:
				case 5:
				case 7:
				case 8:
					SMIMSG3(output_gce_buffer,
						"Larb%d Busy=%d/5, SMI Common Busy=%d/5, status=%d ==> Check engine's state first\n",
						i, smi_larb_busy_count[i], smi_common_busy_count,
						status_code);
					SMIMSG3(output_gce_buffer,
						"If the engine is waiting for Larb%ds' response, it needs SMI HW's check\n",
						i);
					break;
				case 2:
					if (smi_larb_mmu_status[i] == 0) {
						SMIMSG3(output_gce_buffer,
							"Larb%d Busy=%d/5, SMI Common Busy=%d/5, status=%d ==> Check engine state first\n",
							i, smi_larb_busy_count[i],
							smi_common_busy_count, status_code);
						SMIMSG3(output_gce_buffer,
							"If the engine is waiting for Larb%ds' response, it needs SMI HW's check\n",
							i);
					} else {
						SMIMSG3(output_gce_buffer,
							"Larb%d Busy=%d/5, SMI Common Busy=%d/5, status=%d ==> MMU port config error\n",
							i, smi_larb_busy_count[i],
							smi_common_busy_count, status_code);
						is_smi_issue = 1;
					}
					break;
				case 4:
				case 6:
				case 9:
					SMIMSG3(output_gce_buffer,
						"Larb%d Busy=%d/5, SMI Common Busy=%d/5, status=%d ==> not SMI issue\n",
						i, smi_larb_busy_count[i], smi_common_busy_count,
						status_code);
					break;
				case 10:
					SMIMSG3(output_gce_buffer,
						"Larb%d clk is disbable, status=%d ==> no need to check\n",
						i, status_code);
					break;
				default:
					SMIMSG3(output_gce_buffer,
						"Larb%d Busy=%d/5, SMI Common Busy=%d/5, status=%d ==> status unknown\n",
						i, smi_larb_busy_count[i], smi_common_busy_count,
						status_code);
					break;
				}
			}

		}

	}
	return is_smi_issue;
}
void smi_dump_clk_status(void)
{
	int i = 0;

	for (i = 0 ; i < SMI_CLK_CNT ; ++i)
		SMIMSG("CLK status of %s is 0x%x", smi_clk_info[i].name,
		M4U_ReadReg32(smi_clk_info[i].base_addr, smi_clk_info[i].offset));
}
