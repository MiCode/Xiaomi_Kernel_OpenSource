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

#define pr_fmt(fmt) "sipa: " fmt

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/ipv6.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/if_arp.h>
#include <asm/byteorder.h>
#include <linux/tty.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of_device.h>
#include <linux/sipa.h>
#include <uapi/linux/sched/types.h>

#include "sipa_hal.h"
#include "sipa_debug.h"
#include "sipa_dummy.h"
#include "sipa_eth.h"
#include "sipa_priv.h"
#include "sipa_test.h"

static int g_test_result;

enum {
	SIPA_TEST_PASS,
	SIPA_TEST_FAIL,
	SIPA_TEST_INPROGRESS,
};

static void sipa_test_glb_ops(unsigned long unused);
static void sipa_test_fifo_ops(unsigned long unused);

static struct {
	const char *name;
	void (*doit)(unsigned long arg);
	unsigned long arg;
} test_suites[] = {
	{"sipa_test_1", sipa_test_glb_ops, 0},
	{"sipa_test_2", sipa_test_fifo_ops, 0},
};

static int testsuite_print(struct seq_file *s, void *p)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(test_suites); i++)
		seq_printf(s, "%s\n", test_suites[i].name);

	return 0;
}

static int testsuite_open(struct inode *inode, struct file *file)
{
	return single_open(file, testsuite_print, inode->i_private);
}

static void sipa_test_glb_ops(unsigned long unused)
{
	struct sipa_plat_drv_cfg *s_sipa_core = sipa_get_ctrl_pointer();

	g_test_result = SIPA_TEST_INPROGRESS;
	sipa_glb_ops_init(&s_sipa_core->glb_ops);
	g_test_result = SIPA_TEST_PASS;
}

static void sipa_test_fifo_ops(unsigned long unused)
{
	struct sipa_plat_drv_cfg *s_sipa_core = sipa_get_ctrl_pointer();

	g_test_result = SIPA_TEST_INPROGRESS;
	sipa_fifo_ops_init(&s_sipa_core->fifo_ops);
	g_test_result = SIPA_TEST_PASS;
}

static ssize_t testsuite_write(struct file *file,
			       const char __user *user_buf, size_t count,
			       loff_t *ppos)
{
	char buff[64];
	int val, i;

	val = strncpy_from_user(buff, user_buf,
				min_t(long, sizeof(buff) - 1, count));
	if (val < 0)
		return -EFAULT;

	buff[val] = '\0';
	strim(buff);		/* Skip leading & tailing space */

	pr_debug("testsuite: %s\n", buff);

	for (i = 0; i < ARRAY_SIZE(test_suites); i++) {
		if (!strcmp(buff, test_suites[i].name)) {
			test_suites[i].doit(test_suites[i].arg);
			break;
		}
	}

	if (i == ARRAY_SIZE(test_suites)) {
		pr_err("[%s] is invalid\n", buff);
		return -EINVAL;
	}

	return count;
}

static const struct file_operations testsuite_fops = {
	.open = testsuite_open,
	.write = testsuite_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static ssize_t sipa_ltp_write(struct file *file,
			      const char __user *buffer,
			      size_t count, loff_t *pos)
{
	unsigned int sipatestid;
	int ret;

	if (count > 0) {
		ret = kstrtouint_from_user(buffer, count, 10, &sipatestid);
		pr_info("sipatestid= %d, ret %d\n", sipatestid, ret);
		if (ret < 0)
			return -EFAULT;

		switch (sipatestid) {
		case SIPA_LTP_CASE_1:
			sipa_test_glb_ops(0);
			break;
		case SIPA_LTP_CASE_2:
			sipa_test_fifo_ops(0);
			break;
		default:
			break;
		}
	}
	return count;
}

static int sipa_ltp_show(struct seq_file *seq, void *v)
{
	return 0;
}

static int sipa_ltp_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_ltp_show, NULL);
}

static const struct file_operations sipa_ltp_fops = {
	.open = sipa_ltp_open,
	.read = seq_read,
	.write = sipa_ltp_write,
	.llseek = seq_lseek,
	.release = single_release,
};

int sipa_test_init(struct sipa_plat_drv_cfg *ipa)
{
	struct dentry *file;

	file = debugfs_create_file("test-suite", 0644, ipa->debugfs_root,
				   ipa, &testsuite_fops);
	if (!file)
		return -ENOMEM;

	file = debugfs_create_file("sipa_ltp", 0644, ipa->debugfs_root,
				   ipa, &sipa_ltp_fops);
	if (!file)
		return -ENOMEM;

	debugfs_create_u32("sipa_ltp_result", 0644, ipa->debugfs_root,
			   &g_test_result);

	return 0;
}
