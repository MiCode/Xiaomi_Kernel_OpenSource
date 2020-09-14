/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __DEBUG_PLAT_INTERNAL_H__
#define __DEBUG_PLAT_INTERNAL_H__

#include <linux/platform_device.h>

struct debug_plat_drv {
	void (*reg_dump)(void *apu_top, bool dump_vpu, char *mem,
				bool skip_gals, u32 *gals_reg, int platform_idx);
	int (*dump_show)(struct seq_file *sfile, void *v, char *mem,
				u32 *gals_reg, char *module_name, int platform_idx);

	int platform_idx;

	u32 apusys_base;
	u32 apusys_reg_size;
	u32 total_dbg_mux_count;
};


void reg_dump_implement(void *apu_top, bool dump_vpu, char *mem,
				bool skip_gals, u32 *gals_reg, int platform_idx);
int dump_show_implement(struct seq_file *sfile, void *v, char *mem,
				u32 *gals_reg, char *module_name, int platform_idx);

#endif /* __DEBUG_PLAT_INTERNAL_H__ */

