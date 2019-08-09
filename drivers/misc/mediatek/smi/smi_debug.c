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

#include <linux/string.h>
#ifdef CONFIG_MTK_M4U
#include <m4u.h>
#endif

#include "smi_reg.h"
#include "smi_common.h"
#include "smi_public.h"
#include "smi_debug.h"

static void smi_dumpper(int gce_buffer, unsigned long *offset,
	unsigned long base, int reg_number, bool zero_dump)
{
	int size = 0, length = 0, max_size = 128, i, j;
	char buffer[max_size + 1];

	for (i = 0; i < reg_number; i += j) {
		length = 0;
		max_size = 128;
		for (j = 0; i + j < reg_number; j++) {
			if (!zero_dump &&
				M4U_ReadReg32(base, offset[i + j]) == 0)
				continue;
			else if (zero_dump && base == 0) /* offset */
				size = snprintf(buffer + length, max_size + 1,
					" 0x%lx,", offset[i + j]);
			else if (base != 0) /* offset + value */
				size = snprintf(buffer + length, max_size + 1,
					" 0x%lx=0x%x,", offset[i + j],
					M4U_ReadReg32(base, offset[i + j]));

			if (size < 0 || max_size < size) {
				snprintf(buffer + length, max_size + 1, " ");
				break;
			}
			length = length + size;
			max_size = max_size - size;
		}
		SMIMSG3(gce_buffer, "%s\n", buffer);
	}
}

void smi_dumpCommonDebugMsg(int gce_buffer)
{
	unsigned long u4Base = 0;
	/* No verify API in CCF, assume clk is always on */
	unsigned int smiCommonClkEnabled = 1;

	u4Base = get_common_base_addr();
	smiCommonClkEnabled = smi_clk_get_ref_count(SMI_COMMON_REG_INDX);
	/* SMI COMMON dump */
	if (u4Base == SMI_ERROR_ADDR) {
		SMIMSG3(gce_buffer, "common reg dump not support\n");
		return;
	} else if (smiCommonClkEnabled == 0) {
		SMIMSG3(gce_buffer, "===== common clock is disable =====\n");
		return;
	}

	SMIMSG3(gce_buffer, "===== common reg dump, CLK: %d =====\n",
		smiCommonClkEnabled);

	smi_dumpper(gce_buffer, smi_common_debug_offset, u4Base,
		SMI_COMMON_DEBUG_OFFSET_NUM, false);
}
EXPORT_SYMBOL_GPL(smi_dumpCommonDebugMsg);

void smi_dumpLarbDebugMsg(unsigned int larb_indx, int gce_buffer)
{
	unsigned long u4Base = 0;
	/* No verify API in CCF, assume clk is always on */
	unsigned int larbClkEnabled = 1;

	u4Base = get_larb_base_addr(larb_indx);
	larbClkEnabled = smi_clk_get_ref_count(larb_indx);
	if (u4Base == SMI_ERROR_ADDR) {
		SMIMSG3(gce_buffer, "larb%d reg dump not support\n", larb_indx);
		return;
	} else if (larbClkEnabled == 0) {
		SMIMSG3(gce_buffer, "===== larb%d clock is disable =====\n",
			larb_indx);
		return;
	}

	SMIMSG3(gce_buffer, "===== larb%d reg dump, CLK: %d =====\n",
		larb_indx, larbClkEnabled);

	smi_dumpper(gce_buffer, smi_larb_debug_offset, u4Base,
		SMI_LARB_DEBUG_OFFSET_NUM, false);
}
EXPORT_SYMBOL_GPL(smi_dumpLarbDebugMsg);

void smi_dumpDebugMsg(void)
{
	unsigned int larb_indx;

	smi_dumpCommonDebugMsg(0);
	/* dump all SMI LARB */
	for (larb_indx = 0; larb_indx < SMI_LARB_NUM; larb_indx++)
		smi_dumpLarbDebugMsg(larb_indx, 0);

	/* dump m4u register status */
	for (larb_indx = 0; larb_indx < SMI_LARB_NUM; larb_indx++)
		smi_dump_larb_m4u_register(larb_indx);
}
EXPORT_SYMBOL_GPL(smi_dumpDebugMsg);

void smi_dump_larb_m4u_register(unsigned int larb_indx)
{
	unsigned long u4Base = 0;
	unsigned int larbClkEnabled = 0;

	u4Base = get_larb_base_addr(larb_indx);
	larbClkEnabled = smi_clk_get_ref_count(larb_indx);

	if (u4Base == SMI_ERROR_ADDR) {
		SMIMSG("larb%d reg dump not support\n", larb_indx);
		return;
	} else if (larbClkEnabled == 0) {
		SMIMSG("===== larb%d clock is disable =====\n", larb_indx);
		return;
	}

	SMIMSG("===== larb%d m4u secure register =====\n", larb_indx);
	smi_dumpper(0, smi_m4u_secure_offset, u4Base, SMI_MAX_PORT_NUM, false);
	SMIMSG("===== larb%d m4u non-secure register =====\n", larb_indx);
	smi_dumpper(0, smi_m4u_non_secure_offset, u4Base, SMI_MAX_PORT_NUM,
		false);
}
EXPORT_SYMBOL_GPL(smi_dump_larb_m4u_register);

static void smi_dump_debug_register(int gce_buffer)
{
	SMIMSG3(gce_buffer, "===== common register dump offset =====\n");
	smi_dumpper(gce_buffer, smi_common_debug_offset, 0,
		SMI_COMMON_DEBUG_OFFSET_NUM, true);

	SMIMSG3(gce_buffer, "===== larb register dump offset =====\n");
	smi_dumpper(gce_buffer, smi_larb_debug_offset, 0,
		SMI_LARB_DEBUG_OFFSET_NUM, true);
}

static int get_status_code(int smi_larb_clk_status, int smi_larb_busy_count,
			   int smi_common_busy_count, int max_count)
{
	int status_code;

	if (smi_larb_clk_status != 0)
		if (smi_larb_busy_count == max_count) /* larb always busy */
			if (smi_common_busy_count == max_count)
				status_code = 8; /* common always busy */
			else if (smi_common_busy_count == 0)
				status_code = 2; /* common always idle */
			else
				status_code = 5;
		else if (smi_larb_busy_count == 0) /* larb always idle */
			if (smi_common_busy_count == max_count)
				status_code = 6; /* common always busy */
			else if (smi_common_busy_count == 0)
				status_code = 0; /* common always idle */
			else
				status_code = 3;
		else
			if (smi_common_busy_count == max_count)
				status_code = 7;
			else if (smi_common_busy_count == 0)
				status_code = 1;
			else
				status_code = 4;
	else
		status_code = 9;
	return status_code;
}

int smi_debug_bus_hanging_detect(unsigned short larbs, int show_dump,
	int gce_buffer, int enable_m4u_reg_dump)
{
/* gce_buffer = 1, write log into kernel log and CMDQ buffer. */
/* dual_buffer = 0, write log into kernel log only */
/* call m4u dump API when enable_m4u_reg_dump = 1 */

	int i, dump_time, max_count = 5, status_code = 0;
	unsigned long u4Base = 0;
	unsigned char smi_common_busy_count = 0;
	unsigned char smi_larb_busy_count[SMI_LARB_NUM] = { 0 };

	/* dump resister and save resgister status */
	if (show_dump)
		smi_dump_debug_register(gce_buffer);

	for (dump_time = 0; dump_time < max_count; dump_time++) {
		u4Base = get_common_base_addr();
		/* check smi common busy register */
		if (u4Base != SMI_ERROR_ADDR &&
			smi_clk_get_ref_count(SMI_COMMON_REG_INDX) != 0) {
			if ((M4U_ReadReg32(u4Base, 0x440) & (1 << 0)) == 0)
				smi_common_busy_count++;
			if (show_dump != 0)
				smi_dumpCommonDebugMsg(gce_buffer);
		}

		for (i = 0; i < SMI_LARB_NUM; i++) { /* larb */
			u4Base = get_larb_base_addr(i);
			/* check smi larb busy register */
			if (u4Base != SMI_ERROR_ADDR &&
				smi_clk_get_ref_count(i) != 0) {
				if (M4U_ReadReg32(u4Base, 0x0) != 0)
					smi_larb_busy_count[i]++;
				if (show_dump != 0) {
					smi_dumpLarbDebugMsg(i, gce_buffer);
					smi_dump_larb_m4u_register(i);
				}
			}
		}
	}

	/* show result for each larb */
	for (i = 0; i < SMI_LARB_NUM; i++) {
		if (SMI_DBG_LARB_SELECT(larbs, i)) { /* larb i be selected */
			status_code = get_status_code(smi_clk_get_ref_count(i),
				smi_larb_busy_count[i], smi_common_busy_count,
				max_count);

			if (status_code == 9)
				SMIMSG3(gce_buffer,
					"larb%d clk is disbable\n", i);
			else
				SMIMSG3(gce_buffer,
					"busy: larb%d=%d/%d, common=%d/%d, check engine first\n",
					i, smi_larb_busy_count[i], max_count,
					smi_common_busy_count, max_count);
		}
	}
#ifdef CONFIG_MTK_M4U
	if (enable_m4u_reg_dump)
		m4u_dump_reg_for_smi_hang_issue();
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(smi_debug_bus_hanging_detect);
