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

#include "mt6885/debug_mt6885.h"
#include "debug_drv.h"

static void set_dbg_sel(int val, int offset, int shift, int mask, void *apu_top)
{
	LOG_DEBUG("+\n");

	void *target = apu_top + offset;
	u32 tmp = ioread32(target);

	tmp = (tmp & ~(mask << shift)) | (val << shift);
	iowrite32(tmp, target);
	tmp = ioread32(target);

	LOG_DEBUG("-\n");
}

static u32 dbg_read(struct dbg_mux_sel_value sel, void *apu_top)
{
	int i;
	int offset;
	int shift;
	int length;
	int mask;
	void *addr = apu_top + sel.status_reg_offset;
	struct dbg_mux_sel_info info;

	LOG_DEBUG("+\n");

	for (i = 0; i < DBG_MUX_SEL_COUNT; ++i) {
		if (sel.dbg_sel[i] >= 0) {
			info = info_table[i];
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

static void dump_gals_reg(bool dump_vpu, void *apu_top, u32 *gals_reg)
{
	int i;

	LOG_DEBUG("+\n");

	for (i = 0; i < TOTAL_DBG_MUX_COUNT; ++i) {

		/* skip dump vpu gals reg */
		if (false == dump_vpu &&
			i >= DBG_MUX_VPU_START_IDX &&
			i <= DBG_MUX_VPU_END_IDX) {
			gals_reg[i] = NO_READ_VALUE;
			continue;
		}

		gals_reg[i] = dbg_read(value_table[i], apu_top);
	}

	LOG_DEBUG("-\n");
}

static void show_gals(struct seq_file *sfile, u32 *gals_reg)
{
	int i;

	LOG_DEBUG("+\n");

	for (i = 0; i < TOTAL_DBG_MUX_COUNT; ++i)
		seq_printf(sfile, "%s:0x%08x\n",
			value_table[i].name, gals_reg[i]);

	LOG_DEBUG("-\n");
}

void apusys_reg_dump_mt6885(void *apu_top, bool dump_vpu, char *mem,
				bool skip_gals, u32 *gals_reg)
{
	int i, offset, size;

	LOG_DEBUG("+\n");

	if (!skip_gals)
		dump_gals_reg(dump_vpu, apu_top, gals_reg);

	for (i = 0; i < SEGMENT_COUNT; ++i) {
		offset = range_table[i].base - APUSYS_BASE;
		size = range_table[i].size;

		memcpy_fromio(mem + offset, apu_top + offset, size);
	}

	LOG_DEBUG("-\n");
}

int dump_show_mt6885(struct seq_file *sfile, void *v, char *mem, u32 *gals_reg,
							char *module_name)
{
	u64 t;
	u64 nanosec_rem;
	int i;

	LOG_DEBUG("+\n");

	t = sched_clock();
	nanosec_rem = do_div(t, 1000000000);

	seq_printf(sfile, "trigger dump module = %s\n", module_name);

	seq_printf(sfile, "[%5lu.%06lu] ------- dump GALS -------\n",
		(unsigned long) t, (unsigned long) (nanosec_rem / 1000));

	show_gals(sfile, gals_reg);

	seq_puts(sfile, "---- dump from 0x1900_0000 to 0x190F_FFFF ----\n");
	for (i = 0; i < SEGMENT_COUNT; ++i)
		seq_printf(sfile, "%s:0x%08x to 0x%08x\n", range_table[i].name,
			range_table[i].base,
			range_table[i].base + range_table[i].size);

	seq_hex_dump(sfile, "", DUMP_PREFIX_OFFSET, 16, 4,
		mem, APUSYS_REG_SIZE, false);

	LOG_DEBUG("-\n");

	return 0;
}
