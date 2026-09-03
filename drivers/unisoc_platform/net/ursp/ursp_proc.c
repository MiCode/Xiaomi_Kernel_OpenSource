// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <net/route.h>

#include "ursp.h"

int sysctl_net_ursp_enable  __read_mostly;

static unsigned int proc_nfp_perms = 0666;
static struct proc_dir_entry *procdir;
static struct proc_dir_entry *ursp_proc_debug;
static struct proc_dir_entry *ursp_proc_fwd;
static struct proc_dir_entry *ursp_proc_enable;

unsigned int fp_dbg_lvl = FP_PRT_ALL;

int get_ursp_enable(void)
{
	return sysctl_net_ursp_enable;
}
EXPORT_SYMBOL(get_ursp_enable);

static void procdebugprint_ipv4addr(struct seq_file *seq, u32 ipaddr)
{
	seq_printf(seq, "%d.%d.%d.%d", ((ipaddr >> 24) & 0xFF),
		   ((ipaddr >> 16) & 0xFF), ((ipaddr >> 8) & 0xFF),
		   ((ipaddr >> 0) & 0xFF));
}

static void procdebugprint_ipv6addr(struct seq_file *seq, u32 *ipv6addr)
{
	seq_printf(seq, "%pI6", ipv6addr);
}

static void procdebugprint_fwd_info(struct seq_file *seq,
				    struct nf_conntrack_tuple tuple)
{
	seq_puts(seq, "Entry Info:\n");
	seq_puts(seq, "\tIP Info:\t");
	if (tuple.src.l3num == NFPROTO_IPV4) {
		seq_puts(seq, "dst:");
		procdebugprint_ipv4addr(seq,
					ntohl(tuple.dst.u3.ip));
		seq_puts(seq, "\n");
	} else {
		seq_puts(seq, "dst:");
		procdebugprint_ipv6addr(seq, tuple.dst.u3.all);
		seq_puts(seq, "\n");
	}
	seq_puts(seq, "\tL4 Info:\t");
	seq_printf(seq, "proto:%u\t", tuple.dst.protonum);
	seq_printf(seq, "dst:%u\t", ntohs(tuple.dst.u.all));
	seq_puts(seq, "\n");
}

static ssize_t sysctl_net_ursp_enable_proc_write(struct file *file,
						const char __user *buffer,
						size_t count,
						loff_t *pos)
{
	char mode;
	int status = 0;

	if (count > 0) {
		if (get_user(mode, buffer))
			return -EFAULT;

		status = (mode != '0');
		if (status == 1 && sysctl_net_ursp_enable == 0)
			sysctl_net_ursp_enable = 1;
		else if (status == 0 && sysctl_net_ursp_enable == 1)
			sysctl_net_ursp_enable = 0;
	}
	return count;
}

static int sysctl_net_ursp_enable_proc_show(struct seq_file *seq, void *v)
{
	seq_puts(seq, sysctl_net_ursp_enable ? "1\n" : "0\n");
	return 0;
}

static int sysctl_net_ursp_enable_proc_open(struct inode *inode,
					    struct file *file)
{
	return single_open(file, sysctl_net_ursp_enable_proc_show, NULL);
}

static const struct proc_ops proc_ursp_file_switch_ops = {
	.proc_open  = sysctl_net_ursp_enable_proc_open,
	.proc_read  = seq_read,
	.proc_write  = sysctl_net_ursp_enable_proc_write,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static int ursp_debug_proc_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "now debug level=0x%02x\n", fp_dbg);
	return 0;
}

static ssize_t ursp_debug_proc_write(struct file *file,
				     const char __user *buffer,
				     size_t count,
				     loff_t *pos)
{
	unsigned int level2;
	int ret;

	ret = kstrtouint_from_user(buffer, count, 16, &level2);
	if (ret < 0)
		return -EFAULT;
	else if (level2 <= 0xFF)
		fp_dbg = level2;
	return count;
}

static int ursp_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ursp_debug_proc_show, NULL);
}

static const struct proc_ops proc_ursp_file_debug_ops = {
	.proc_open  = ursp_debug_proc_open,
	.proc_read  = seq_read,
	.proc_write  = ursp_debug_proc_write,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static int ursp_fwd_proc_show(struct seq_file *seq, void *v)
{
	struct hlist_node *n;
	struct ursp_fwd_entry *curr_entry;
	struct urspfwd_iter_state *st = seq->private;

	n = (struct hlist_node *)v;
	seq_printf(seq, "URSP Entry index[%d]-hash[%u]:\n",
		   st->count++, st->bucket);
	curr_entry = hlist_entry(n, struct ursp_fwd_entry, entry_lst);
	procdebugprint_fwd_info(seq, curr_entry->ursp_ct->tuple);

	return 0;
}

static void *ursp_fwd_get_next(struct seq_file *s, void *v)
{
	struct urspfwd_iter_state *st = s->private;
	struct hlist_node *n;

	n = (struct hlist_node *)v;
	n = rcu_dereference(hlist_next_rcu(n));
	if (!n) {
		while (!n && (++st->bucket) < URSP_ENTRIES_HASH_SIZE) {
			n = rcu_dereference(hlist_first_rcu(&ursp_fwd_entries[st->bucket]));
			if (n)
				return n;
		}
	}
	return n;
}

static void *ursp_fwd_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return ursp_fwd_get_next(s, v);
}

static void *ursp_fwd_get_first(struct seq_file *seq, loff_t *pos)
{
	struct urspfwd_iter_state *st = seq->private;
	struct hlist_node *n;

	if (st->bucket == URSP_ENTRIES_HASH_SIZE)
		return NULL;

	for (; st->bucket < URSP_ENTRIES_HASH_SIZE; st->bucket++) {
		n = rcu_dereference(hlist_first_rcu(&ursp_fwd_entries[st->bucket]));
		if (n)
			return n;
	}
	return NULL;
}

static void *ursp_fwd_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(RCU)
{
	rcu_read_lock();
	return ursp_fwd_get_first(seq, pos);
}

static void ursp_fwd_seq_stop(struct seq_file *s, void *v)
	__releases(RCU)
{
	rcu_read_unlock();
}

static const struct seq_operations ursp_fwd_ops = {
	.start = ursp_fwd_seq_start,
	.next  = ursp_fwd_seq_next,
	.stop  = ursp_fwd_seq_stop,
	.show  = ursp_fwd_proc_show
};

static int ursp_fwd_opt_proc_open(struct inode *inode, struct file *file)
{
	return seq_open_private(file,
				&ursp_fwd_ops,
				sizeof(struct urspfwd_iter_state));
}

static const struct proc_ops proc_ursp_file_fwd_ops = {
	.proc_open = ursp_fwd_opt_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release
};

int ursp_proc_create(void)
{
#if IS_ENABLED(CONFIG_PROC_FS)
	int ret;

	procdir = proc_mkdir("ursp", init_net.proc_net);
	if (!procdir) {
		pr_err("failed to create proc/.../ursp\n");
		ret = -ENOMEM;
		goto no_dir;
	}
	ursp_proc_debug = proc_create_data("debug", proc_nfp_perms,
					   procdir,
					   &proc_ursp_file_debug_ops,
					   NULL);
	if (!ursp_proc_debug) {
		pr_err("nfp: failed to create ursp/debug file\n");
		ret = -ENOMEM;
		goto no_debug_entry;
	}
	ursp_proc_enable = proc_create_data("enable", proc_nfp_perms,
					    procdir,
					    &proc_ursp_file_switch_ops,
					    NULL);
	if (!ursp_proc_enable) {
		pr_err("nfp: failed to create ursp/enable file\n");
		ret = -ENOMEM;
		goto no_enable_entry;
	}
	ursp_proc_fwd = proc_create_data("ursp_fwd_entries", proc_nfp_perms,
					 procdir,
					 &proc_ursp_file_fwd_ops,
					 NULL);
	if (!ursp_proc_fwd) {
		pr_err("nfp: failed to create ursp/fwd_entries file\n");
		ret = -ENOMEM;
		goto no_fwd_entry;
	}

	return 0;
no_debug_entry:
	remove_proc_entry("debug", procdir);
no_enable_entry:
	remove_proc_entry("enable", procdir);
no_fwd_entry:
	remove_proc_entry("ursp_fwd_entries", procdir);
no_dir:
	return ret;
#endif
}
EXPORT_SYMBOL(ursp_proc_create);

void nfp_proc_exit(void)
{
	remove_proc_entry("debug", procdir);
	remove_proc_entry("enable", procdir);
	remove_proc_entry("ursp_fwd_entries", procdir);
	remove_proc_entry("ursp", NULL);
}
