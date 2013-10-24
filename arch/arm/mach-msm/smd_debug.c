/* arch/arm/mach-msm/smd_debug.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/jiffies.h>

#include <mach/msm_iomap.h>
#include <mach/msm_smem.h>

#include "smd_private.h"

#if defined(CONFIG_DEBUG_FS)

static char *chstate(unsigned n)
{
	switch (n) {
	case SMD_SS_CLOSED:
		return "CLOSED";
	case SMD_SS_OPENING:
		return "OPENING";
	case SMD_SS_OPENED:
		return "OPENED";
	case SMD_SS_FLUSHING:
		return "FLUSHING";
	case SMD_SS_CLOSING:
		return "CLOSING";
	case SMD_SS_RESET:
		return "RESET";
	case SMD_SS_RESET_OPENING:
		return "ROPENING";
	default:
		return "UNKNOWN";
	}
}

static void debug_int_stats(struct seq_file *s)
{
	int subsys;
	struct interrupt_stat *stats = interrupt_stats;
	const char *subsys_name;

	seq_puts(s,
		"   Subsystem    | Interrupt ID |    In     | Out (Hardcoded) |"
		" Out (Configured)|\n");

	for (subsys = 0; subsys < NUM_SMD_SUBSYSTEMS; ++subsys) {
		subsys_name = smd_pid_to_subsystem(subsys);
		if (subsys_name) {
			seq_printf(s,
				"%-10s %4s |    %9d | %9u |       %9u |       %9u |\n",
				smd_pid_to_subsystem(subsys), "smd",
				stats->smd_interrupt_id,
				stats->smd_in_count,
				stats->smd_out_hardcode_count,
				stats->smd_out_config_count);

			seq_printf(s,
				"%-10s %4s |    %9d | %9u |       %9u |       %9u |\n",
				smd_pid_to_subsystem(subsys), "smsm",
				stats->smsm_interrupt_id,
				stats->smsm_in_count,
				stats->smsm_out_hardcode_count,
				stats->smsm_out_config_count);
		}
		++stats;
	}
}

static void debug_int_stats_reset(struct seq_file *s)
{
	int subsys;
	struct interrupt_stat *stats = interrupt_stats;

	seq_puts(s, "Resetting interrupt stats.\n");

	for (subsys = 0; subsys < NUM_SMD_SUBSYSTEMS; ++subsys) {
		stats->smd_in_count = 0;
		stats->smd_out_hardcode_count = 0;
		stats->smd_out_config_count = 0;
		stats->smsm_in_count = 0;
		stats->smsm_out_hardcode_count = 0;
		stats->smsm_out_config_count = 0;
		++stats;
	}
}

static void debug_diag(struct seq_file *s)
{
	seq_puts(s, "Printing to log\n");
	smd_diag();
}

static void dump_ch(struct seq_file *s, int n,
		   void *half_ch_s,
		   void *half_ch_r,
		   struct smd_half_channel_access *half_ch_funcs,
		   unsigned size)
{
	seq_printf(s, "ch%02d:"
		" %8s(%04d/%04d) %c%c%c%c%c%c%c%c <->"
		" %8s(%04d/%04d) %c%c%c%c%c%c%c%c : %5x\n", n,
		chstate(half_ch_funcs->get_state(half_ch_s)),
		half_ch_funcs->get_tail(half_ch_s),
		half_ch_funcs->get_head(half_ch_s),
		half_ch_funcs->get_fDSR(half_ch_s) ? 'D' : 'd',
		half_ch_funcs->get_fCTS(half_ch_s) ? 'C' : 'c',
		half_ch_funcs->get_fCD(half_ch_s) ? 'C' : 'c',
		half_ch_funcs->get_fRI(half_ch_s) ? 'I' : 'i',
		half_ch_funcs->get_fHEAD(half_ch_s) ? 'W' : 'w',
		half_ch_funcs->get_fTAIL(half_ch_s) ? 'R' : 'r',
		half_ch_funcs->get_fSTATE(half_ch_s) ? 'S' : 's',
		half_ch_funcs->get_fBLOCKREADINTR(half_ch_s) ? 'B' : 'b',
		chstate(half_ch_funcs->get_state(half_ch_r)),
		half_ch_funcs->get_tail(half_ch_r),
		half_ch_funcs->get_head(half_ch_r),
		half_ch_funcs->get_fDSR(half_ch_r) ? 'D' : 'd',
		half_ch_funcs->get_fCTS(half_ch_r) ? 'C' : 'c',
		half_ch_funcs->get_fCD(half_ch_r) ? 'C' : 'c',
		half_ch_funcs->get_fRI(half_ch_r) ? 'I' : 'i',
		half_ch_funcs->get_fHEAD(half_ch_r) ? 'W' : 'w',
		half_ch_funcs->get_fTAIL(half_ch_r) ? 'R' : 'r',
		half_ch_funcs->get_fSTATE(half_ch_r) ? 'S' : 's',
		half_ch_funcs->get_fBLOCKREADINTR(half_ch_r) ? 'B' : 'b',
		size
		);
}

#if (!defined(CONFIG_MSM_SMD_PKG4) && !defined(CONFIG_MSM_SMD_PKG3))
static void debug_read_ch(struct seq_file *s)
{
	void *shared;
	int n;
	struct smd_alloc_elm *ch_tbl;
	unsigned ch_type;
	unsigned shared_size;

	ch_tbl = smem_find(ID_CH_ALLOC_TBL, sizeof(*ch_tbl) * 64);
	if (!ch_tbl)
		return;

	for (n = 0; n < SMD_CHANNELS; n++) {
		ch_type = SMD_CHANNEL_TYPE(ch_tbl[n].type);
		if (is_word_access_ch(ch_type))
			shared_size =
				sizeof(struct smd_half_channel_word_access);
		else
			shared_size = sizeof(struct smd_half_channel);
		shared = smem_find(ID_SMD_CHANNELS + n,
				2 * shared_size + SMD_BUF_SIZE);

		if (shared == 0)
			continue;
		dump_ch(s, n, shared,
			     (shared + shared_size +
			     SMD_BUF_SIZE), get_half_ch_funcs(ch_type),
			     SMD_BUF_SIZE);
	}
}
#else
static void debug_read_ch(struct seq_file *s)
{
	void *shared, *buffer;
	unsigned buffer_sz;
	int n;
	struct smd_alloc_elm *ch_tbl;
	unsigned ch_type;
	unsigned shared_size;

	ch_tbl = smem_find(ID_CH_ALLOC_TBL, sizeof(*ch_tbl) * 64);
	if (!ch_tbl)
		return;

	for (n = 0; n < SMD_CHANNELS; n++) {
		ch_type = SMD_CHANNEL_TYPE(ch_tbl[n].type);
		if (is_word_access_ch(ch_type))
			shared_size =
				sizeof(struct smd_half_channel_word_access);
		else
			shared_size = sizeof(struct smd_half_channel);

		shared = smem_find(ID_SMD_CHANNELS + n, 2 * shared_size);

		if (shared == 0)
			continue;

		buffer = smem_get_entry(SMEM_SMD_FIFO_BASE_ID + n, &buffer_sz);

		if (buffer == 0)
			continue;

		dump_ch(s, n, shared,
			     (shared + shared_size),
			     get_half_ch_funcs(ch_type),
			     buffer_sz / 2);
	}
}
#endif

/* NNV: revist, it may not be smd version */
static void debug_read_smd_version(struct seq_file *s)
{
	uint32_t *smd_ver;
	uint32_t n, version;

	smd_ver = smem_alloc(SMEM_VERSION_SMD, 32 * sizeof(uint32_t));

	if (smd_ver)
		for (n = 0; n < 32; n++) {
			version = smd_ver[n];
			seq_printf(s, "entry %d: %d.%d\n", n,
				       version >> 16,
				       version & 0xffff);
		}
}

static void debug_read_alloc_tbl(struct seq_file *s)
{
	struct smd_alloc_elm *shared;
	int n;

	shared = smem_find(ID_CH_ALLOC_TBL, sizeof(struct smd_alloc_elm[64]));

	if (!shared)
		return;

	for (n = 0; n < 64; n++) {
		seq_printf(s, "name=%s cid=%d ch type=%d "
				"xfer type=%d ref_count=%d\n",
				shared[n].name,
				shared[n].cid,
				SMD_CHANNEL_TYPE(shared[n].type),
				SMD_XFER_TYPE(shared[n].type),
				shared[n].ref_count);
	}
}

static int debugfs_show(struct seq_file *s, void *data)
{
	void (*show)(struct seq_file *) = s->private;

	show(s);

	return 0;
}

static int debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, debugfs_show, inode->i_private);
}

static const struct file_operations debug_ops = {
	.open = debug_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

static void debug_create(const char *name, umode_t mode,
			 struct dentry *dent,
			 void (*show)(struct seq_file *))
{
	struct dentry *file;

	file = debugfs_create_file(name, mode, dent, show, &debug_ops);
	if (!file)
		pr_err("%s: unable to create file '%s'\n", __func__, name);
}

static int __init smd_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("smd", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debug_create("ch", 0444, dent, debug_read_ch);
	debug_create("version", 0444, dent, debug_read_smd_version);
	debug_create("tbl", 0444, dent, debug_read_alloc_tbl);
	debug_create("print_diag", 0444, dent, debug_diag);
	debug_create("int_stats", 0444, dent, debug_int_stats);
	debug_create("int_stats_reset", 0444, dent, debug_int_stats_reset);

	return 0;
}

late_initcall(smd_debugfs_init);
#endif
