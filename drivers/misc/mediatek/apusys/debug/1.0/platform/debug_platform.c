// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/sched/clock.h>

#include "debug_platform.h"
#include "debug_drv.h"

static void set_dbg_sel(int val, int offset, int shift, int mask, void *apu_top)
{
	void *target = apu_top + offset;
	u32 tmp = ioread32(target);

	tmp = (tmp & ~(mask << shift)) | (val << shift);
	iowrite32(tmp, target);
	tmp = ioread32(target);
}

static u32 dbg_read(struct dbg_mux_sel_value sel, void *apu_top,
					int mux_sel_count, struct dbg_mux_sel_info *info_tbl)
{
	int i;
	int offset;
	int shift;
	int length;
	int mask;
	void *addr = apu_top + sel.status_reg_offset;
	struct dbg_mux_sel_info info;

	LOG_DEBUG("+\n");

	for (i = 0; i < mux_sel_count; ++i) {
		if (sel.dbg_sel[i] >= 0) {
			info = info_tbl[i];
			offset = info.offset;
			shift = info.end_bit;
			length = info.start_bit - info.end_bit + 1;
			mask = (1 << length) - 1;

			set_dbg_sel(sel.dbg_sel[i], offset, shift, mask, apu_top);
		}
	}

	LOG_DEBUG("-\n");

	return ioread32(addr);
}

static void dump_gals_reg(bool dump_vpu, void *apu_top, u32 *gals_reg,
							int total_mux_count, int mux_sel_count,
							int skip_start, int skip_end,
							struct dbg_mux_sel_info *info_tbl,
							struct dbg_mux_sel_value *value_tbl)
{
	int i;

	LOG_DEBUG("+\n");

	for (i = 0; i < total_mux_count; ++i) {

		/* skip dump vpu gals reg */
		if (false == dump_vpu &&
			i >= skip_start &&
			i <= skip_end) {
			gals_reg[i] = NO_READ_VALUE;
			continue;
		}

		gals_reg[i] = dbg_read(value_tbl[i], apu_top, mux_sel_count, info_tbl);
	}

	LOG_DEBUG("-\n");
}

static void show_gals(struct seq_file *sfile, u32 *gals_reg,
					int total_mux_count, struct dbg_mux_sel_value *value_tbl)
{
	int i;

	LOG_DEBUG("+\n");

	for (i = 0; i < total_mux_count; ++i)
		seq_printf(sfile, "%s:0x%08x\n", value_tbl[i].name, gals_reg[i]);

	LOG_DEBUG("-\n");
}

void reg_dump_implement(void *apu_top, bool dump_vpu, char *mem,
						bool skip_gals, u32 *gals_reg, int platform_idx)
{
	int i, offset, size;
	struct dbg_hw_info hw_info = hw_info_set[platform_idx];

	LOG_DEBUG("+\n");

	if (!skip_gals)
		dump_gals_reg(dump_vpu, apu_top, gals_reg,
						hw_info.total_mux_ount,
						hw_info.mux_sel_count,
						hw_info.mux_vpu_start_idx,
						hw_info.mux_vpu_end_idx,
						hw_info.mux_sel_tbl,
						hw_info.value_tbl);

	for (i = 0; i < hw_info.seg_count; ++i) {
		offset = hw_info.range_tbl[i].base - APUSYS_BASE;
		size = hw_info.range_tbl[i].size;

		memcpy_fromio(mem + offset, apu_top + offset, size);
	}

	LOG_DEBUG("-\n");
}

int dump_show_implement(struct seq_file *sfile, void *v, char *mem,
						u32 *gals_reg, char *module_name, int platform_idx)
{
	u64 t;
	u64 nanosec_rem;
	int i;
	struct reg_dump_info info;
	struct dbg_hw_info hw_info = hw_info_set[platform_idx];

	LOG_DEBUG("+\n");

	t = sched_clock();
	nanosec_rem = do_div(t, 1000000000);

	seq_printf(sfile, "trigger dump module = %s\n", module_name);

	seq_printf(sfile, "[%5lu.%06lu] ------- dump GALS -------\n",
		(unsigned long) t, (unsigned long) (nanosec_rem / 1000));

	show_gals(sfile, gals_reg, hw_info.total_mux_ount, hw_info.value_tbl);

	seq_puts(sfile, "---- dump from 0x1900_0000 to 0x190F_FFFF ----\n");
	for (i = 0; i < hw_info.seg_count; ++i) {
		info = hw_info.range_tbl[i];
		seq_printf(sfile, "%s:0x%08x to 0x%08x\n", info.name, info.base,
					info.base + info.size);
	}

	seq_hex_dump(sfile, "", DUMP_PREFIX_OFFSET, 16, 4,
					mem, APUSYS_REG_SIZE, false);

	LOG_DEBUG("-\n");

	return 0;
}

