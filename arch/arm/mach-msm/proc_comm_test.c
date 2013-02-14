/* Copyright (c) 2009, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * PROC COMM TEST Driver source file
 */

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <mach/proc_comm.h>

static struct dentry *dent;
static int proc_comm_test_res;

static int proc_comm_reverse_test(void)
{
	uint32_t data1, data2;
	int rc;

	data1 = 10;
	data2 = 20;

	rc = msm_proc_comm(PCOM_OEM_TEST_CMD, &data1, &data2);
	if (rc)
		return rc;

	if ((data1 != 20) || (data2 != 10))
		return -1;

	return 0;
}

static ssize_t debug_read(struct file *fp, char __user *buf,
			  size_t count, loff_t *pos)
{
	char _buf[16];

	snprintf(_buf, sizeof(_buf), "%i\n", proc_comm_test_res);

	return simple_read_from_buffer(buf, count, pos, _buf, strlen(_buf));
}

static ssize_t debug_write(struct file *fp, const char __user *buf,
			   size_t count, loff_t *pos)
{

	unsigned char cmd[64];
	int len;

	if (count < 1)
		return 0;

	len = count > 63 ? 63 : count;

	if (copy_from_user(cmd, buf, len))
		return -EFAULT;

	cmd[len] = 0;

	if (cmd[len-1] == '\n') {
		cmd[len-1] = 0;
		len--;
	}

	if (!strncmp(cmd, "reverse_test", 64))
		proc_comm_test_res = proc_comm_reverse_test();
	else
		proc_comm_test_res = -EINVAL;

	if (proc_comm_test_res)
		pr_err("proc comm test fail %d\n",
		       proc_comm_test_res);
	else
		pr_info("proc comm test passed\n");

	return count;
}

static int debug_release(struct inode *ip, struct file *fp)
{
	return 0;
}

static int debug_open(struct inode *ip, struct file *fp)
{
	return 0;
}

static const struct file_operations debug_ops = {
	.owner = THIS_MODULE,
	.open = debug_open,
	.release = debug_release,
	.read = debug_read,
	.write = debug_write,
};

static void __exit proc_comm_test_mod_exit(void)
{
	debugfs_remove(dent);
}

static int __init proc_comm_test_mod_init(void)
{
	dent = debugfs_create_file("proc_comm", 0444, 0, NULL, &debug_ops);
	proc_comm_test_res = -1;
	return 0;
}

module_init(proc_comm_test_mod_init);
module_exit(proc_comm_test_mod_exit);

MODULE_DESCRIPTION("PROC COMM TEST Driver");
MODULE_LICENSE("GPL v2");
