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
#include <linux/sipa.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <net/route.h>

#include "sfp.h"
#include "sfp_ipa.h"

static unsigned int proc_nfp_perms = 0666;

static struct proc_dir_entry *procdir;
static struct proc_dir_entry *sfp_proc_mgr_fwd;
static struct proc_dir_entry *sfp_proc_debug;
static struct proc_dir_entry *sfp_proc_fwd;
static struct proc_dir_entry *sfp_proc_ipa;
static struct proc_dir_entry *sfp_proc_enable;
static struct proc_dir_entry *sfp_proc_tether_scheme;
#if IS_ENABLED(CONFIG_SPRD_SFP_TEST)
static struct proc_dir_entry *sfp_test;
static struct proc_dir_entry *sfp_test_result;

/********Sfp Test and result show********************************/
static int sfp_test_proc_show(struct seq_file *seq, void *v)
{
	//seq_printf(seq, "do test=0x%02x\n", test_count);
	return 0;
}

static ssize_t sfp_test_proc_write(struct file *file,
				   const char __user *buffer,
				   size_t count,
				   loff_t *pos)
{
	unsigned int level;
	int ret;

	if (count > 0) {
		ret = kstrtouint_from_user(buffer, count, 10, &level);
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "test_proc = %d, ret %d\n", level, ret);
		if (ret < 0)
			return -EFAULT;
		if (level > 0)
			sfp_test_init(level);
	}
	return count;
}

static int sfp_test_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sfp_test_proc_show, NULL);
}

static const struct proc_ops proc_sfp_file_test_ops = {
	.proc_open  = sfp_test_proc_open,
	.proc_read  = seq_read,
	.proc_write  = sfp_test_proc_write,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static int sfp_test_result_proc_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "%d\n", test_result);
	return 0;
}

static ssize_t sfp_test_result_proc_write(struct file *file,
					  const char __user *buffer,
					  size_t count,
					  loff_t *pos)
{
	int result;
	int ret;

	if (count > 0) {
		ret = kstrtouint_from_user(buffer, count, 10, &result);
		FP_PRT_DBG(FP_PRT_DEBUG,
			   "test_result_proc = %d, ret %d\n", result, ret);
		if (ret < 0)
			return -EFAULT;

		test_result = result;
	}
	return count;
}

static int sfp_test_result_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sfp_test_result_proc_show, NULL);
}

static const struct proc_ops proc_sfp_file_test_result_ops = {
	.proc_open  = sfp_test_result_proc_open,
	.proc_read  = seq_read,
	.proc_write  = sfp_test_result_proc_write,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static int sfp_test_proc_create_data(void)
{
	int ret;

	sfp_test = proc_create_data("test", proc_nfp_perms,
				    procdir,
				    &proc_sfp_file_test_ops,
				    NULL);

	if (!sfp_test) {
		pr_err("nfp: failed to create sfp/test file\n");
		ret = -ENOMEM;
		goto no_test_entry;
	}

	sfp_test_result = proc_create_data("test_result", proc_nfp_perms,
					   procdir,
					   &proc_sfp_file_test_result_ops,
					   NULL);

	if (!sfp_test_result) {
		pr_err("nfp: failed to create sfp/test result file\n");
		ret = -ENOMEM;
		goto no_test_result_entry;
	}

	return 0;

no_test_result_entry:
	remove_proc_entry("test_result", procdir);
no_test_entry:
	remove_proc_entry("test", procdir);

	return ret;
}
#else
static int sfp_test_proc_create_data(void)
{
	return 0;
}
#endif /* CONFIG_SPRD_SFP_TEST */

unsigned int fp_dbg_lvl = FP_PRT_ALL;

int get_sfp_enable(void)
{
	return sysctl_net_sfp_enable;
}
EXPORT_SYMBOL(get_sfp_enable);

void sfp_mgr_disable(void)
{
	if (sysctl_net_sfp_enable == 1)
		sysctl_net_sfp_enable = 0;
	FP_PRT_DBG(FP_PRT_DEBUG, "Network Fast Processing Disabled\n");
}
EXPORT_SYMBOL(sfp_mgr_disable);

static void procdebugprint_ipv4addr(struct seq_file *seq, u32 ipaddr)
{
	seq_printf(seq, "%d.%d.%d.%d", ((ipaddr >> 24) & 0xFF),
		   ((ipaddr >> 16) & 0xFF), ((ipaddr >> 8) & 0xFF),
		   ((ipaddr >> 0) & 0xFF));
}

static void procdebugprint_ipv6addr(struct seq_file *seq, u32 *ip6addr)
{
	seq_printf(seq, "0x%08x:0x%08x:0x%08x:0x%08x",
		   ip6addr[0], ip6addr[1],
		   ip6addr[2], ip6addr[3]);
}

static void procdebugprint_mgr_fwd_info(struct seq_file *seq,
					struct sfp_mgr_fwd_tuple *tuple)
{
	seq_puts(seq, "Original INFO:\n");
	seq_puts(seq, "\tMAC INFO:\t");
	seq_printf(seq, "dst:%02x.%02x.%02x.%02x.%02x.%02x\t",
		   tuple->orig_mac_info.dst_mac[0],
		   tuple->orig_mac_info.dst_mac[1],
		   tuple->orig_mac_info.dst_mac[2],
		   tuple->orig_mac_info.dst_mac[3],
		   tuple->orig_mac_info.dst_mac[4],
		   tuple->orig_mac_info.dst_mac[5]);
	seq_printf(seq, "src:%02x.%02x.%02x.%02x.%02x.%02x\n",
		   tuple->orig_mac_info.src_mac[0],
		   tuple->orig_mac_info.src_mac[1],
		   tuple->orig_mac_info.src_mac[2],
		   tuple->orig_mac_info.src_mac[3],
		   tuple->orig_mac_info.src_mac[4],
		   tuple->orig_mac_info.src_mac[5]);
	seq_puts(seq, "\tIP INFO:\t");
	if (tuple->orig_info.l3_proto == NFPROTO_IPV4) {
		seq_puts(seq, "dst: ");
		procdebugprint_ipv4addr(seq, ntohl(tuple->orig_info.dst_ip.ip));
		seq_puts(seq, "\tsrc: ");
		procdebugprint_ipv4addr(seq, ntohl(tuple->orig_info.src_ip.ip));
		seq_puts(seq, "\n");
	} else {
		seq_puts(seq, "dst: ");
		procdebugprint_ipv6addr(seq, tuple->orig_info.dst_ip.all);
		seq_puts(seq, "\tsrc: ");
		procdebugprint_ipv6addr(seq, tuple->orig_info.src_ip.all);
		seq_puts(seq, "\n");
	}
	seq_puts(seq, "\tL4 INFO:\t");
	seq_printf(seq, "proto:%u\t", tuple->orig_info.l4_proto);
	seq_printf(seq, "dst:%u\t", tuple->orig_info.dst_l4_info.all);
	seq_printf(seq, "src:%u\n", tuple->orig_info.src_l4_info.all);
	seq_puts(seq, "Transfer Info:\n");
	seq_puts(seq, "\tMAC INFO:\t");
	seq_printf(seq, "dst:%02x.%02x.%02x.%02x.%02x.%02x\t",
		   tuple->trans_mac_info.dst_mac[0],
		   tuple->trans_mac_info.dst_mac[1],
		   tuple->trans_mac_info.dst_mac[2],
		   tuple->trans_mac_info.dst_mac[3],
		   tuple->trans_mac_info.dst_mac[4],
		   tuple->trans_mac_info.dst_mac[5]);
	seq_printf(seq, "src:%02x.%02x.%02x.%02x.%02x.%02x\n",
		   tuple->trans_mac_info.src_mac[0],
		   tuple->trans_mac_info.src_mac[1],
		   tuple->trans_mac_info.src_mac[2],
		   tuple->trans_mac_info.src_mac[3],
		   tuple->trans_mac_info.src_mac[4],
		   tuple->trans_mac_info.src_mac[5]);
	seq_puts(seq, "\tIP Info:\t");
	if (tuple->trans_info.l3_proto == NFPROTO_IPV4) {
		seq_puts(seq, "dst:");
		procdebugprint_ipv4addr(seq,
					ntohl(tuple->trans_info.dst_ip.ip));
		seq_puts(seq, "\tsrc:");
		procdebugprint_ipv4addr(seq,
					ntohl(tuple->trans_info.src_ip.ip));
		seq_puts(seq, "\n");
	} else {
		seq_puts(seq, "dst:");
		procdebugprint_ipv6addr(seq, tuple->trans_info.dst_ip.all);
		seq_puts(seq, "\tsrc:");
		procdebugprint_ipv6addr(seq, tuple->trans_info.src_ip.all);
		seq_puts(seq, "\n");
	}
	seq_puts(seq, "\tL4 Info:\t");
	seq_printf(seq, "proto:%u\t", tuple->trans_info.l4_proto);
	seq_printf(seq, "dst:%u\t", tuple->trans_info.dst_l4_info.all);
	seq_printf(seq, "src:%u\t", tuple->trans_info.src_l4_info.all);
	seq_printf(seq, "COUNT=%u\t", tuple->count);
	seq_printf(seq, "FLAGS %d\t", tuple->fwd_flags);
	seq_printf(seq, "(IN-OUT)-(%u-%u)\n", tuple->in_ifindex,
		   tuple->out_ifindex);
}

static void procdebugprint_fwd_info_2(struct seq_file *seq,
				      struct sfp_trans_tuple *tuple)
{
	seq_puts(seq, "Transfer Info:\n");
	seq_puts(seq, "\tMAC INFO:\t");
	seq_printf(seq, "dst:%02x.%02x.%02x.%02x.%02x.%02x\t",
		   tuple->trans_mac_info.dst_mac[0],
		   tuple->trans_mac_info.dst_mac[1],
		   tuple->trans_mac_info.dst_mac[2],
		   tuple->trans_mac_info.dst_mac[3],
		   tuple->trans_mac_info.dst_mac[4],
		   tuple->trans_mac_info.dst_mac[5]);
	seq_printf(seq, "src:%02x.%02x.%02x.%02x.%02x.%02x\n",
		   tuple->trans_mac_info.src_mac[0],
		   tuple->trans_mac_info.src_mac[1],
		   tuple->trans_mac_info.src_mac[2],
		   tuple->trans_mac_info.src_mac[3],
		   tuple->trans_mac_info.src_mac[4],
		   tuple->trans_mac_info.src_mac[5]);
	seq_puts(seq, "\tIP Info:\t");
	if (tuple->trans_info.l3_proto == NFPROTO_IPV4) {
		seq_puts(seq, "dst:");
		procdebugprint_ipv4addr(seq,
					ntohl(tuple->trans_info.dst_ip.ip));
		seq_puts(seq, "\tsrc:");
		procdebugprint_ipv4addr(seq,
					ntohl(tuple->trans_info.src_ip.ip));
		seq_puts(seq, "\n");
	} else {
		seq_puts(seq, "dst:");
		procdebugprint_ipv6addr(seq, tuple->trans_info.dst_ip.all);
		seq_puts(seq, "\tsrc:");
		procdebugprint_ipv6addr(seq, tuple->trans_info.src_ip.all);
		seq_puts(seq, "\n");
	}
	seq_puts(seq, "\tL4 Info:\t");
	seq_printf(seq, "proto:%u\t", tuple->trans_info.l4_proto);
	seq_printf(seq, "dst:%u\t", ntohs(tuple->trans_info.dst_l4_info.all));
	seq_printf(seq, "src:%u\t", ntohs(tuple->trans_info.src_l4_info.all));
	seq_printf(seq, "COUNT=%u\t", tuple->count);
	seq_printf(seq, "FLAGS: %d\t", tuple->fwd_flags);
	seq_printf(seq, "(IN-OUT)-(%u-%u)\n", tuple->in_ifindex,
		   tuple->out_ifindex);
}

static void procdebugprint_fwd_info(struct seq_file *seq, struct sfp_fwd_entry *tuple)
{
	struct sfp_trans_tuple *sfp_tuple;

	sfp_tuple = &tuple->ssfp_trans_tuple;
	procdebugprint_fwd_info_2(seq, sfp_tuple);
}

/********sfp mgr fwd show********************************/
static int sfp_fwd_proc_show(struct seq_file *seq, void *v)
{
	struct hlist_node *n;
	struct sfp_fwd_entry *curr_entry;
	struct sfpfwd_iter_state *st = seq->private;

	n = (struct hlist_node *)v;
	seq_printf(seq, "SFP Forward Entry index[%d]-hash[%u]:\n",
		   st->count++, st->bucket);
	curr_entry = hlist_entry(n, struct sfp_fwd_entry, entry_lst);
	procdebugprint_fwd_info(seq, curr_entry);

	return 0;
}

static void *sfp_fwd_get_next(struct seq_file *s, void *v)
{
	struct sfp_mgr_fwd_iter_state *st = s->private;
	struct hlist_node *n;

	n = (struct hlist_node *)v;
	n = rcu_dereference(hlist_next_rcu(n));
	if (!n) {
		while (!n && (++st->bucket) < SFP_ENTRIES_HASH_SIZE) {
			n = rcu_dereference(hlist_first_rcu(&sfp_fwd_entries[st->bucket]));
			if (n)
				return n;
		}
	}
	return n;
}

static void *sfp_fwd_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return sfp_fwd_get_next(s, v);
}

static void *sfp_fwd_get_first(struct seq_file *seq, loff_t *pos)
{
	struct sfp_mgr_fwd_iter_state *st = seq->private;
	struct hlist_node *n;

	if (st->bucket == SFP_ENTRIES_HASH_SIZE)
		return NULL;

	for (; st->bucket < SFP_ENTRIES_HASH_SIZE; st->bucket++) {
		n = rcu_dereference(hlist_first_rcu(&sfp_fwd_entries[st->bucket]));
		if (n)
			return n;
	}
	return NULL;
}

static void *sfp_fwd_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(RCU)
{
	rcu_read_lock();
	return sfp_fwd_get_first(seq, pos);
}

static void sfp_fwd_seq_stop(struct seq_file *s, void *v)
	__releases(RCU)
{
	rcu_read_unlock();
}

static const struct seq_operations sfp_fwd_ops = {
	.start = sfp_fwd_seq_start,
	.next  = sfp_fwd_seq_next,
	.stop  = sfp_fwd_seq_stop,
	.show  = sfp_fwd_proc_show
};

static int sfp_fwd_opt_proc_open(struct inode *inode, struct file *file)
{
	return seq_open_private(file,
				&sfp_fwd_ops,
				sizeof(struct sfpfwd_iter_state));
}

static const struct proc_ops proc_sfp_file_fwd_ops = {
	.proc_open = sfp_fwd_opt_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release
};

static ssize_t sysctl_net_sfp_enable_proc_write(struct file *file,
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
		if (status == 1 && sysctl_net_sfp_enable == 0) {
			sfp_mgr_proc_enable();
			sysctl_net_sfp_enable = 1;
		} else if (status == 0 && sysctl_net_sfp_enable == 1) {
			sysctl_net_sfp_enable = 0;
			sfp_mgr_proc_disable();
		}
	}
	return count;
}

static int sysctl_net_sfp_enable_proc_show(struct seq_file *seq, void *v)
{
	seq_puts(seq, sysctl_net_sfp_enable ? "1\n" : "0\n");
	return 0;
}

static int sysctl_net_sfp_enable_proc_open(struct inode *inode,
					   struct file *file)
{
	return single_open(file, sysctl_net_sfp_enable_proc_show, NULL);
}

static const struct proc_ops proc_sfp_file_switch_ops = {
	.proc_open  = sysctl_net_sfp_enable_proc_open,
	.proc_read  = seq_read,
	.proc_write  = sysctl_net_sfp_enable_proc_write,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static int sfp_debug_proc_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "now debug level=0x%02x\n", fp_dbg_lvl);
	return 0;
}

static ssize_t sfp_debug_proc_write(struct file *file,
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
		fp_dbg_lvl = level2;
	return count;
}

static int sfp_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sfp_debug_proc_show, NULL);
}

static const struct proc_ops proc_sfp_file_debug_ops = {
	.proc_open  = sfp_debug_proc_open,
	.proc_read  = seq_read,
	.proc_write  = sfp_debug_proc_write,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

/********Mgr_fp fwd show********************************/
static int sfp_mgr_fwd_proc_show(struct seq_file *seq, void *v)
{
	struct hlist_node *n;
	struct sfp_mgr_fwd_tuple_hash *entry_info1, *entry_info2;
	struct sfpfwd_iter_state *st = seq->private;
	struct sfp_conn *sfp_ct;

	n = (struct hlist_node *)v;
	entry_info1 = hlist_entry(n, struct sfp_mgr_fwd_tuple_hash, entry_lst);

	if (entry_info1->tuple.dst.dir != IP_CT_DIR_ORIGINAL)
		return 0;

	sfp_ct = sfp_ct_tuplehash_to_ctrack(entry_info1);
	seq_printf(seq, "SFP MGR Forward Entry index[%d]-hash[%u]:\n",
		   st->count++, st->bucket);
	procdebugprint_mgr_fwd_info(seq, &entry_info1->ssfp_fwd_tuple);
	entry_info2 = &sfp_ct->tuplehash[IP_CT_DIR_REPLY];
	procdebugprint_mgr_fwd_info(seq, &entry_info2->ssfp_fwd_tuple);
	seq_printf(seq, "\t time=%ld",
		   (sfp_ct->timeout.expires - jiffies) / HZ);
	seq_puts(seq, "\n");
	return 0;
}

static void *sfp_mgr_fwd_get_next(struct seq_file *s, void *v)
{
	struct sfpfwd_iter_state *st = s->private;
	struct hlist_node *n;

	n = (struct hlist_node *)v;
	n = rcu_dereference(hlist_next_rcu(n));
	if (!n) {
		while (!n && (++st->bucket) < SFP_ENTRIES_HASH_SIZE) {
			n = rcu_dereference(hlist_first_rcu(&mgr_fwd_entries[st->bucket]));
			if (n)
				return n;
		}
	}
	return n;
}

static void *sfp_mgr_fwd_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	return sfp_mgr_fwd_get_next(s, v);
}

static void *sfp_mgr_fwd_get_first(struct seq_file *seq, loff_t *pos)
{
	struct sfpfwd_iter_state *st = seq->private;
	struct hlist_node *n;

	if (st->bucket == SFP_ENTRIES_HASH_SIZE)
		return NULL;

	for (; st->bucket < SFP_ENTRIES_HASH_SIZE; st->bucket++) {
		n = rcu_dereference(hlist_first_rcu(&mgr_fwd_entries[st->bucket]));
		if (n)
			return n;
	}
	return NULL;
}

static void *sfp_mgr_fwd_seq_start(struct seq_file *seq, loff_t *pos)
	__acquires(RCU)
{
	rcu_read_lock();
	return sfp_mgr_fwd_get_first(seq, pos);
}

static void sfp_mgr_fwd_seq_stop(struct seq_file *s, void *v)
	__releases(RCU)
{
	rcu_read_unlock();
}

static const struct seq_operations sfp_mgr_fwd_ops = {
	.start = sfp_mgr_fwd_seq_start,
	.next  = sfp_mgr_fwd_seq_next,
	.stop  = sfp_mgr_fwd_seq_stop,
	.show  = sfp_mgr_fwd_proc_show
};

static int sfp_mgr_fwd_opt_proc_open(struct inode *inode, struct file *file)
{
	return seq_open_private(file,
				&sfp_mgr_fwd_ops,
				sizeof(struct sfpfwd_iter_state));
}

static const struct proc_ops proc_mgr_sfp_file_fwd_ops = {
	.proc_open = sfp_mgr_fwd_opt_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release
};

static int sfp_net_tether_scheme_proc_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "%d\n", get_sfp_tether_scheme());
	return 0;
}

static int sfp_net_tether_scheme_proc_open(struct inode *inode,
					   struct file *file)
{
	return single_open(file, sfp_net_tether_scheme_proc_show, NULL);
}

static ssize_t sfp_net_tether_scheme_proc_write(struct file *file,
						const char __user *buffer,
						size_t count,
						loff_t *pos)
{
	char mode;

	if (count > 0) {
		if (get_user(mode, buffer))
			return -EFAULT;

		if (mode == '0')
			set_sfp_tether_scheme(SFP_HARD_PATH);
		else if (mode == '1')
			set_sfp_tether_scheme(SFP_SOFT_PATH);
		else if (mode == '2')
			set_sfp_tether_scheme(SFP_HALF_PATH);
		else
			FP_PRT_DBG(FP_PRT_ERR, "unsupport tether scheme\n");
	}
	return count;
}

static const struct proc_ops proc_sfp_file_tether_scheme_ops = {
	.proc_open  = sfp_net_tether_scheme_proc_open,
	.proc_read  = seq_read,
	.proc_write  = sfp_net_tether_scheme_proc_write,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

static void sfp_print_ipa_fwd_entry(int index, struct seq_file *seq,
				    struct fwd_entry *cur_entry)
{
	seq_printf(seq, "------------INDEX#%d----------\n", index);
	seq_puts(seq, "Original:");
	if (cur_entry->orig_info.l3_proto == NFPROTO_IPV4) {
		seq_printf(seq, "%pI4->%pI4\t", &cur_entry->orig_info.src_ip.ip,
			   &cur_entry->orig_info.dst_ip.ip);
	} else {
		seq_printf(seq, "%pI6->%pI6\t", &cur_entry->orig_info.src_ip.ip,
			   &cur_entry->orig_info.dst_ip.ip);
	}

	seq_printf(seq, "%d->%d\t", ntohs(cur_entry->orig_info.src_l4_info.all),
		   ntohs(cur_entry->orig_info.dst_l4_info.all));

	seq_printf(seq, "l3proto: %d l4proto: %d\n",
		   cur_entry->orig_info.l3_proto,
		   cur_entry->orig_info.l4_proto);

	seq_puts(seq, "Transfer:");
	if (cur_entry->trans_info.l3_proto == NFPROTO_IPV4) {
		seq_printf(seq, "%pI4->%pI4\t",
			   &cur_entry->trans_info.src_ip.ip,
			   &cur_entry->trans_info.dst_ip.ip);
	} else {
		seq_printf(seq, "%pI6->%pI6\t",
			   &cur_entry->trans_info.src_ip.ip6,
			   &cur_entry->trans_info.dst_ip.ip6);
	}

	seq_printf(seq, "%d->%d\t",
		   ntohs(cur_entry->trans_info.src_l4_info.all),
		   ntohs(cur_entry->trans_info.dst_l4_info.all));

	seq_printf(seq, "l3proto: %d l4proto: %d\n",
		   cur_entry->trans_info.l3_proto,
		   cur_entry->trans_info.l4_proto);

	seq_printf(seq, "MAC:%pM->%pM\n", &cur_entry->trans_mac_info.src_mac,
		   &cur_entry->trans_mac_info.dst_mac);

	seq_printf(seq, "dst id: %d fwd_flags %d\n", cur_entry->out_ifindex,
		   cur_entry->fwd_flags);

#if IS_ENABLED(CONFIG_UNISOC_SIPA_V3)
	seq_printf(seq, "mac_info_opts %d\n", cur_entry->mac_info_opts);

	seq_printf(seq, "pkt_drop_th %d pkt_current_idx %d\n",
		   cur_entry->pkt_drop_th, cur_entry->pkt_current_idx);
	seq_printf(seq, "pkt_total_cnt %d pkt_current_cnt %d pkt_drop_cnt %d\n",
		   cur_entry->pkt_total_cnt, cur_entry->pkt_current_cnt,
		   cur_entry->pkt_drop_cnt);
#endif

	seq_printf(seq, "time_stamp %d\n", cur_entry->time_stamp);
}

static int sfp_ipa_proc_show(struct seq_file *seq, void *v)
{
	int i;
	u8 *v_hash;
	struct fwd_entry *cur_entry;
	int cur_tether_scheme;

	cur_tether_scheme = get_sfp_tether_scheme();

	if (cur_tether_scheme == SFP_HARD_PATH || cur_tether_scheme == SFP_HALF_PATH) {
#if IS_ENABLED(CONFIG_UNISOC_SIPA_V3)
		/* Before cat ipa_fwd_entries,
		 * update ipa timestamp from ddr.
		 */
		sipa_hal_set_hash_sync_req();
#endif

		seq_printf(seq, "T0: entry_cnt %d\n",
			   atomic_read(&fwd_tbl.entry_cnt));

		seq_puts(seq, "################Table0 START################\n");
		v_hash = sfp_get_hash_vtbl(T0);

		for (i = 0; i < atomic_read(&fwd_tbl.entry_cnt); i++) {
			cur_entry = (struct fwd_entry *)(v_hash + SFP_ENTRIES_HASH_SIZE * 8) + i;
			sfp_print_ipa_fwd_entry(i, seq, cur_entry);
		}

		seq_puts(seq, "################Table0 END##################\n");
		seq_printf(seq, "T1: entry_cnt %d\n",
			   atomic_read(&fwd_tbl.entry_cnt));
		seq_puts(seq, "**************Table1 START******************\n");
		v_hash = sfp_get_hash_vtbl(T1);

		for (i = 0; i < atomic_read(&fwd_tbl.entry_cnt); i++) {
			cur_entry = (struct fwd_entry *)(v_hash + SFP_ENTRIES_HASH_SIZE * 8) + i;
			sfp_print_ipa_fwd_entry(i, seq, cur_entry);
		}

		seq_puts(seq, "**************Table1 END********************\n");
	}
	return 0;
}

static int sfp_ipa_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sfp_ipa_proc_show, NULL);
}

static const struct proc_ops proc_sfp_file_ipa_ops = {
	.proc_open  = sfp_ipa_proc_open,
	.proc_read  = seq_read,
	.proc_lseek  = seq_lseek,
	.proc_release = single_release,
};

int sfp_proc_create(void)
{
#if IS_ENABLED(CONFIG_PROC_FS)
	int ret;

	procdir = proc_mkdir("sfp", init_net.proc_net);
	if (!procdir) {
		pr_err("failed to create proc/.../sfp\n");
		ret = -ENOMEM;
		goto no_dir;
	}
	sfp_proc_mgr_fwd = proc_create_data("mgr_fwd_entries", proc_nfp_perms,
					    procdir,
					    &proc_mgr_sfp_file_fwd_ops,
					    NULL);
	if (!sfp_proc_mgr_fwd) {
		pr_err("nfp: failed to create sfp/nat file\n");
		ret = -ENOMEM;
		goto no_mgr_fwd_entry;
	}
	sfp_proc_fwd = proc_create_data("sfp_fwd_entries", proc_nfp_perms,
					procdir,
					&proc_sfp_file_fwd_ops,
					NULL);
	if (!sfp_proc_fwd) {
		pr_err("nfp: failed to create sfp/fwd_entries file\n");
		ret = -ENOMEM;
		goto no_fwd_entry;
	}
	sfp_proc_enable = proc_create_data("enable", proc_nfp_perms,
					   procdir,
					   &proc_sfp_file_switch_ops,
					   NULL);
	if (!sfp_proc_enable) {
		pr_err("nfp: failed to create sfp/enable file\n");
		ret = -ENOMEM;
		goto no_enable_entry;
	}
	sfp_proc_debug = proc_create_data("debug", proc_nfp_perms,
					  procdir,
					  &proc_sfp_file_debug_ops,
					  NULL);
	if (!sfp_proc_debug) {
		pr_err("nfp: failed to create sfp/debug file\n");
		ret = -ENOMEM;
		goto no_debug_entry;
	}

	sfp_proc_tether_scheme = proc_create_data("tether_scheme",
						  proc_nfp_perms,
						  procdir,
						  &proc_sfp_file_tether_scheme_ops,
						  NULL);
	if (!sfp_proc_tether_scheme) {
		pr_err("nfp: failed to create sfp/tether_scheme file\n");
		ret = -ENOMEM;
		goto no_tether_scheme_entry;
	}
	/* ipa_fwd_entries */
	sfp_proc_ipa = proc_create_data("ipa_fwd_entries", proc_nfp_perms,
					procdir,
					&proc_sfp_file_ipa_ops,
					NULL);
	if (!sfp_proc_ipa) {
		pr_err("nfp: failed to create ipa/fwd_entries file\n");
		ret = -ENOMEM;
		goto no_ipa_entry;
	}

	sfp_test_proc_create_data();

	return 0;

no_tether_scheme_entry:
	remove_proc_entry("tether_scheme", procdir);
no_enable_entry:
	remove_proc_entry("enable", procdir);
no_debug_entry:
	remove_proc_entry("debug", procdir);
no_fwd_entry:
	remove_proc_entry("sfp_fwd_entries", procdir);
no_mgr_fwd_entry:
	remove_proc_entry("mgr_fwd_entries", procdir);
no_ipa_entry:
	remove_proc_entry("ipa_fwd_entries", procdir);
no_dir:
	remove_proc_entry("sfp", NULL);

	return ret;
#endif
}
EXPORT_SYMBOL(sfp_proc_create);

void nfp_proc_exit(void)
{
	remove_proc_entry("test_result", procdir);
	remove_proc_entry("test", procdir);
	remove_proc_entry("debug", procdir);
	remove_proc_entry("tether_scheme", procdir);
	remove_proc_entry("enable", procdir);
	remove_proc_entry("sfp_fwd_entries", procdir);
	remove_proc_entry("mgr_fwd_entries", procdir);
	remove_proc_entry("sfp", NULL);
}
