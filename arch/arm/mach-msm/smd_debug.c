/* arch/arm/mach-msm/smd_debug.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2014, The Linux Foundation. All rights reserved.
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

#include <soc/qcom/smem.h>

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
		"   Subsystem    | Interrupt ID |    In     |    Out    |\n");

	for (subsys = 0; subsys < NUM_SMD_SUBSYSTEMS; ++subsys) {
		subsys_name = smd_pid_to_subsystem(subsys);
		if (!IS_ERR_OR_NULL(subsys_name)) {
			seq_printf(s, "%-10s %4s |    %9d | %9u | %9u |\n",
				smd_pid_to_subsystem(subsys), "smd",
				stats->smd_interrupt_id,
				stats->smd_in_count,
				stats->smd_out_count);

			seq_printf(s, "%-10s %4s |    %9d | %9u | %9u |\n",
				smd_pid_to_subsystem(subsys), "smsm",
				stats->smsm_interrupt_id,
				stats->smsm_in_count,
				stats->smsm_out_count);
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
		stats->smd_out_count = 0;
		stats->smsm_in_count = 0;
		stats->smsm_out_count = 0;
		++stats;
	}
}

/* NNV: revist, it may not be smd version */
static void debug_read_smd_version(struct seq_file *s)
{
	uint32_t *smd_ver;
	uint32_t n, version;

	smd_ver = smem_find(SMEM_VERSION_SMD, 32 * sizeof(uint32_t),
							0, SMEM_ANY_HOST_FLAG);

	if (smd_ver)
		for (n = 0; n < 32; n++) {
			version = smd_ver[n];
			seq_printf(s, "entry %d: %d.%d\n", n,
				       version >> 16,
				       version & 0xffff);
		}
}

/**
 * pid_to_str - Convert a numeric processor id value into a human readable
 *		string value.
 *
 * @pid: the processor id to convert
 * @returns: a string representation of @pid
 */
static char *pid_to_str(int pid)
{
	switch (pid) {
	case SMD_APPS:
		return "APPS";
	case SMD_MODEM:
		return "MDMSW";
	case SMD_Q6:
		return "ADSP";
	case SMD_TZ:
		return "TZ";
	case SMD_WCNSS:
		return "WCNSS";
	case SMD_MODEM_Q6_FW:
		return "MDMFW";
	case SMD_RPM:
		return "RPM";
	default:
		return "???";
	}
}

/**
 * print_half_ch_state - Print the state of half of a SMD channel in a human
 *			readable format.
 *
 * @s: the sequential file to print to
 * @half_ch: half of a SMD channel that should have its state printed
 * @half_ch_funcs: the relevant channel access functions for @half_ch
 * @size: size of the fifo in bytes associated with @half_ch
 * @proc: the processor id that owns the part of the SMD channel associated with
 *		@half_ch
 */
static void print_half_ch_state(struct seq_file *s,
				void *half_ch,
				struct smd_half_channel_access *half_ch_funcs,
				unsigned size,
				int proc)
{
	seq_printf(s, "%-5s|%-7s|0x%05X|0x%05X|0x%05X|%c%c%c%c%c%c%c%c|0x%05X",
			pid_to_str(proc),
			chstate(half_ch_funcs->get_state(half_ch)),
			size,
			half_ch_funcs->get_tail(half_ch),
			half_ch_funcs->get_head(half_ch),
			half_ch_funcs->get_fDSR(half_ch) ? 'D' : 'd',
			half_ch_funcs->get_fCTS(half_ch) ? 'C' : 'c',
			half_ch_funcs->get_fCD(half_ch) ? 'C' : 'c',
			half_ch_funcs->get_fRI(half_ch) ? 'I' : 'i',
			half_ch_funcs->get_fHEAD(half_ch) ? 'W' : 'w',
			half_ch_funcs->get_fTAIL(half_ch) ? 'R' : 'r',
			half_ch_funcs->get_fSTATE(half_ch) ? 'S' : 's',
			half_ch_funcs->get_fBLOCKREADINTR(half_ch) ? 'B' : 'b',
			(half_ch_funcs->get_head(half_ch) -
				half_ch_funcs->get_tail(half_ch)) & (size - 1));
}

/**
 * smd_xfer_type_to_str - Convert a numeric transfer type value into a human
 *		readable string value.
 *
 * @xfer_type: the processor id to convert
 * @returns: a string representation of @xfer_type
 */
static char *smd_xfer_type_to_str(uint32_t xfer_type)
{
	if (xfer_type == 1)
		return "S"; /* streaming type */
	else if (xfer_type == 2)
		return "P"; /* packet type */
	else
		return "L"; /* legacy type */
}

/**
 * print_smd_ch_table - Print the current state of every valid SMD channel in a
 *			specific SMD channel allocation table to a human
 *			readable formatted output.
 *
 * @s: the sequential file to print to
 * @tbl: a valid pointer to the channel allocation table to print from
 * @num_tbl_entries: total number of entries in the table referenced by @tbl
 * @ch_base_id: the SMEM item id corresponding to the array of channel
 *		structures for the channels found in @tbl
 * @fifo_base_id: the SMEM item id corresponding to the array of channel fifos
 *		for the channels found in @tbl
 * @pid: processor id to use for any SMEM operations
 * @flags: flags to use for any SMEM operations
 */
static void print_smd_ch_table(struct seq_file *s,
				struct smd_alloc_elm *tbl,
				unsigned num_tbl_entries,
				unsigned ch_base_id,
				unsigned fifo_base_id,
				unsigned pid,
				unsigned flags)
{
	void *half_ch;
	unsigned half_ch_size;
	uint32_t ch_type;
	void *buffer;
	unsigned buffer_size;
	int n;

/*
 * formatted, human readable channel state output, ie:
ID|CHANNEL NAME       |T|PROC |STATE  |FIFO SZ|RDPTR  |WRPTR  |FLAGS   |DATAPEN
-------------------------------------------------------------------------------
00|DS                 |S|APPS |CLOSED |0x02000|0x00000|0x00000|dcCiwrsb|0x00000
  |                   | |MDMSW|OPENING|0x02000|0x00000|0x00000|dcCiwrsb|0x00000
-------------------------------------------------------------------------------
 */

	seq_printf(s, "%2s|%-19s|%1s|%-5s|%-7s|%-7s|%-7s|%-7s|%-8s|%-7s\n",
								"ID",
								"CHANNEL NAME",
								"T",
								"PROC",
								"STATE",
								"FIFO SZ",
								"RDPTR",
								"WRPTR",
								"FLAGS",
								"DATAPEN");
	seq_puts(s,
		"-------------------------------------------------------------------------------\n");
	for (n = 0; n < num_tbl_entries; ++n) {
		if (strlen(tbl[n].name) == 0)
			continue;

		seq_printf(s, "%2u|%-19s|%s|", tbl[n].cid, tbl[n].name,
			smd_xfer_type_to_str(SMD_XFER_TYPE(tbl[n].type)));
		ch_type = SMD_CHANNEL_TYPE(tbl[n].type);
		if (is_word_access_ch(ch_type))
			half_ch_size =
				sizeof(struct smd_half_channel_word_access);
		else
			half_ch_size = sizeof(struct smd_half_channel);

		half_ch = smem_find(ch_base_id + n, 2 * half_ch_size,
								pid, flags);
		buffer = smem_get_entry(fifo_base_id + n, &buffer_size,
								pid, flags);
		if (half_ch && buffer)
			print_half_ch_state(s,
					half_ch,
					get_half_ch_funcs(ch_type),
					buffer_size / 2,
					smd_edge_to_local_pid(ch_type));

		seq_puts(s, "\n");
		seq_printf(s, "%2s|%-19s|%1s|", "", "", "");

		if (half_ch && buffer)
			print_half_ch_state(s,
					half_ch + half_ch_size,
					get_half_ch_funcs(ch_type),
					buffer_size / 2,
					smd_edge_to_remote_pid(ch_type));

		seq_puts(s, "\n");
		seq_puts(s,
			"-------------------------------------------------------------------------------\n");
	}
}

/**
 * debug_ch - Print the current state of every valid SMD channel in a human
 *		readable formatted table.
 *
 * @s: the sequential file to print to
 */
static void debug_ch(struct seq_file *s)
{
	struct smd_alloc_elm *tbl;
	struct smd_alloc_elm *default_pri_tbl;
	struct smd_alloc_elm *default_sec_tbl;
	unsigned tbl_size;
	int i;

	tbl = smem_get_entry(ID_CH_ALLOC_TBL, &tbl_size, 0, SMEM_ANY_HOST_FLAG);
	default_pri_tbl = tbl;

	if (!tbl) {
		seq_puts(s, "Channel allocation table not found\n");
		return;
	}

	seq_puts(s, "Primary allocation table:\n");
	print_smd_ch_table(s, tbl, tbl_size / sizeof(*tbl), ID_SMD_CHANNELS,
							SMEM_SMD_FIFO_BASE_ID,
							0,
							SMEM_ANY_HOST_FLAG);

	tbl = smem_get_entry(SMEM_CHANNEL_ALLOC_TBL_2, &tbl_size, 0,
							SMEM_ANY_HOST_FLAG);
	default_sec_tbl = tbl;
	if (tbl) {
		seq_puts(s, "\n\nSecondary allocation table:\n");
		print_smd_ch_table(s, tbl, tbl_size / sizeof(*tbl),
						SMEM_SMD_BASE_ID_2,
						SMEM_SMD_FIFO_BASE_ID_2,
						0,
						SMEM_ANY_HOST_FLAG);
	}

	for (i = 1; i < NUM_SMD_SUBSYSTEMS; ++i) {
		tbl = smem_get_entry(ID_CH_ALLOC_TBL, &tbl_size, i, 0);
		if (tbl && tbl != default_pri_tbl) {
			seq_puts(s, "\n\n");
			seq_printf(s, "%s <-> %s Primary allocation table:\n",
							pid_to_str(SMD_APPS),
							pid_to_str(i));
			print_smd_ch_table(s, tbl, tbl_size / sizeof(*tbl),
							ID_SMD_CHANNELS,
							SMEM_SMD_FIFO_BASE_ID,
							i,
							0);
		}

		tbl = smem_get_entry(SMEM_CHANNEL_ALLOC_TBL_2, &tbl_size, i, 0);
		if (tbl && tbl != default_sec_tbl) {
			seq_puts(s, "\n\n");
			seq_printf(s, "%s <-> %s Secondary allocation table:\n",
							pid_to_str(SMD_APPS),
							pid_to_str(i));
			print_smd_ch_table(s, tbl, tbl_size / sizeof(*tbl),
						SMEM_SMD_BASE_ID_2,
						SMEM_SMD_FIFO_BASE_ID_2,
						i,
						0);
		}
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

	debug_create("ch", 0444, dent, debug_ch);
	debug_create("version", 0444, dent, debug_read_smd_version);
	debug_create("int_stats", 0444, dent, debug_int_stats);
	debug_create("int_stats_reset", 0444, dent, debug_int_stats_reset);

	return 0;
}

late_initcall(smd_debugfs_init);
#endif
