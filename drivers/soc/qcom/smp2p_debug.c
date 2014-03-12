/* drivers/soc/qcom/smp2p_debug.c
 *
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/ctype.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include "smp2p_private.h"

#if defined(CONFIG_DEBUG_FS)

/**
 * Dump interrupt statistics.
 *
 * @s:   pointer to output file
 */
static void smp2p_int_stats(struct seq_file *s)
{
	struct smp2p_interrupt_config *int_cfg;
	int pid;

	int_cfg = smp2p_get_interrupt_config();
	if (!int_cfg)
		return;

	seq_puts(s, "| Processor | Incoming Id | Incoming # |");
	seq_puts(s, " Outgoing # | Base Ptr |   Mask   |\n");

	for (pid = 0; pid < SMP2P_NUM_PROCS; ++pid) {
		if (!int_cfg[pid].is_configured &&
				pid != SMP2P_REMOTE_MOCK_PROC)
			continue;

		seq_printf(s, "| %5s (%d) | %11u | %10u | %10u | %p | %08x |\n",
			int_cfg[pid].name,
			pid, int_cfg[pid].in_int_id,
			int_cfg[pid].in_interrupt_count,
			int_cfg[pid].out_interrupt_count,
			int_cfg[pid].out_int_ptr,
			int_cfg[pid].out_int_mask);
	}
}

/**
 * Dump item header line 1.
 *
 * @buf:      output buffer
 * @max:      length of output buffer
 * @item_ptr: SMEM item pointer
 * @state:    item state
 * @returns: Number of bytes written to output buffer
 */
static int smp2p_item_header1(char *buf, int max, struct smp2p_smem *item_ptr,
	enum msm_smp2p_edge_state state)
{
	int i = 0;
	const char *state_text;

	if (!item_ptr) {
		i += scnprintf(buf + i, max - i, "None");
		return i;
	}

	switch (state) {
	case SMP2P_EDGE_STATE_CLOSED:
		state_text = "State: Closed";
		break;
	case SMP2P_EDGE_STATE_OPENING:
		state_text = "State: Opening";
		break;
	case SMP2P_EDGE_STATE_OPENED:
		state_text = "State: Opened";
		break;
	default:
		state_text = "";
		break;
	}

	i += scnprintf(buf + i, max - i,
		"%-14s LPID %d RPID %d",
		state_text,
		SMP2P_GET_LOCAL_PID(item_ptr->rem_loc_proc_id),
		SMP2P_GET_REMOTE_PID(item_ptr->rem_loc_proc_id)
		);

	return i;
}

/**
 * Dump item header line 2.
 *
 * @buf:      output buffer
 * @max:      length of output buffer
 * @item_ptr: SMEM item pointer
 * @returns: Number of bytes written to output buffer
 */
static int smp2p_item_header2(char *buf, int max, struct smp2p_smem *item_ptr)
{
	int i = 0;

	if (!item_ptr) {
		i += scnprintf(buf + i, max - i, "None");
		return i;
	}

	i += scnprintf(buf + i, max - i,
		"Version: %08x Features: %08x",
		SMP2P_GET_VERSION(item_ptr->feature_version),
		SMP2P_GET_FEATURES(item_ptr->feature_version)
		);

	return i;
}

/**
 * Dump item header line 3.
 *
 * @buf:      output buffer
 * @max:      length of output buffer
 * @item_ptr: SMEM item pointer
 * @state:    item state
 * @returns: Number of bytes written to output buffer
 */
static int smp2p_item_header3(char *buf, int max, struct smp2p_smem *item_ptr)
{
	int i = 0;

	if (!item_ptr) {
		i += scnprintf(buf + i, max - i, "None");
		return i;
	}

	i += scnprintf(buf + i, max - i,
		"Entries #/Max: %d/%d Flags: %c%c",
		SMP2P_GET_ENT_VALID(item_ptr->valid_total_ent),
		SMP2P_GET_ENT_TOTAL(item_ptr->valid_total_ent),
		item_ptr->flags & SMP2P_FLAGS_RESTART_ACK_MASK ? 'A' : 'a',
		item_ptr->flags & SMP2P_FLAGS_RESTART_DONE_MASK ? 'D' : 'd'
		);

	return i;
}

/**
 * Dump individual input/output item pair.
 *
 * @s:   pointer to output file
 */
static void smp2p_item(struct seq_file *s, int remote_pid)
{
	struct smp2p_smem *out_ptr;
	struct smp2p_smem *in_ptr;
	struct smp2p_interrupt_config *int_cfg;
	char tmp_buff[64];
	int state;
	int entry;
	struct smp2p_entry_v1 *out_entries = NULL;
	struct smp2p_entry_v1 *in_entries = NULL;
	int out_valid = 0;
	int in_valid = 0;
	char entry_name[SMP2P_MAX_ENTRY_NAME];

	int_cfg = smp2p_get_interrupt_config();
	if (!int_cfg)
		return;
	if (!int_cfg[remote_pid].is_configured &&
			remote_pid != SMP2P_REMOTE_MOCK_PROC)
		return;

	out_ptr = smp2p_get_out_item(remote_pid, &state);
	in_ptr = smp2p_get_in_item(remote_pid);

	if (!out_ptr && !in_ptr)
		return;

	/* print item headers */
	seq_printf(s, "%s%s\n",
		" ====================================== ",
		"======================================");
	scnprintf(tmp_buff, sizeof(tmp_buff),
		"Apps(%d)->%s(%d)",
		SMP2P_APPS_PROC, int_cfg[remote_pid].name, remote_pid);
	seq_printf(s, "| %-37s", tmp_buff);

	scnprintf(tmp_buff, sizeof(tmp_buff),
		"%s(%d)->Apps(%d)",
		int_cfg[remote_pid].name, remote_pid, SMP2P_APPS_PROC);
	seq_printf(s, "| %-37s|\n", tmp_buff);
	seq_printf(s, "%s%s\n",
		" ====================================== ",
		"======================================");

	smp2p_item_header1(tmp_buff, sizeof(tmp_buff), out_ptr, state);
	seq_printf(s, "| %-37s", tmp_buff);
	smp2p_item_header1(tmp_buff, sizeof(tmp_buff), in_ptr, -1);
	seq_printf(s, "| %-37s|\n", tmp_buff);

	smp2p_item_header2(tmp_buff, sizeof(tmp_buff), out_ptr);
	seq_printf(s, "| %-37s", tmp_buff);
	smp2p_item_header2(tmp_buff, sizeof(tmp_buff), in_ptr);
	seq_printf(s, "| %-37s|\n", tmp_buff);

	smp2p_item_header3(tmp_buff, sizeof(tmp_buff), out_ptr);
	seq_printf(s, "| %-37s", tmp_buff);
	smp2p_item_header3(tmp_buff, sizeof(tmp_buff), in_ptr);
	seq_printf(s, "| %-37s|\n", tmp_buff);

	seq_printf(s, " %s%s\n",
		"-------------------------------------- ",
		"--------------------------------------");
	seq_printf(s, "| %-37s",
		"Entry Name       Value");
	seq_printf(s, "| %-37s|\n",
		"Entry Name       Value");
	seq_printf(s, " %s%s\n",
		"-------------------------------------- ",
		"--------------------------------------");

	/* print entries */
	if (out_ptr) {
		out_entries = (struct smp2p_entry_v1 *)((void *)out_ptr +
				sizeof(struct smp2p_smem));
		out_valid = SMP2P_GET_ENT_VALID(out_ptr->valid_total_ent);
	}

	if (in_ptr) {
		in_entries = (struct smp2p_entry_v1 *)((void *)in_ptr +
				sizeof(struct smp2p_smem));
		in_valid = SMP2P_GET_ENT_VALID(in_ptr->valid_total_ent);
	}

	for (entry = 0; out_entries || in_entries; ++entry) {
		if (out_entries && entry < out_valid) {
			memcpy_fromio(entry_name, out_entries->name,
							SMP2P_MAX_ENTRY_NAME);
			scnprintf(tmp_buff, sizeof(tmp_buff),
					"%-16s 0x%08x",
					entry_name,
					out_entries->entry);
			++out_entries;
		} else {
			out_entries = NULL;
			scnprintf(tmp_buff, sizeof(tmp_buff), "None");
		}
		seq_printf(s, "| %-37s", tmp_buff);

		if (in_entries && entry < in_valid) {
			memcpy_fromio(entry_name, in_entries->name,
							SMP2P_MAX_ENTRY_NAME);
			scnprintf(tmp_buff, sizeof(tmp_buff),
					"%-16s 0x%08x",
					entry_name,
					in_entries->entry);
			++in_entries;
		} else {
			in_entries = NULL;
			scnprintf(tmp_buff, sizeof(tmp_buff), "None");
		}
		seq_printf(s, "| %-37s|\n", tmp_buff);
	}
	seq_printf(s, " %s%s\n\n",
		"-------------------------------------- ",
		"--------------------------------------");
}

/**
 * Dump item state.
 *
 * @s:   pointer to output file
 */
static void smp2p_items(struct seq_file *s)
{
	int pid;

	for (pid = 0; pid < SMP2P_NUM_PROCS; ++pid)
		smp2p_item(s, pid);
}

static struct dentry *dent;

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

void debug_create(const char *name,
			 void (*show)(struct seq_file *))
{
	struct dentry *file;

	file = debugfs_create_file(name, 0444, dent, show, &debug_ops);
	if (!file)
		pr_err("%s: unable to create file '%s'\n", __func__, name);
}

static int __init smp2p_debugfs_init(void)
{
	dent = debugfs_create_dir("smp2p", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debug_create("int_stats", smp2p_int_stats);
	debug_create("items", smp2p_items);

	return 0;
}

late_initcall(smp2p_debugfs_init);
#endif /* CONFIG_DEBUG_FS */
