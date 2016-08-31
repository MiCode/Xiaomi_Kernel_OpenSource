/*
 * Bit COREPDRQ toggling interface
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

static inline uint32_t read_dbgprcr(void)
{
	uint32_t ret_val = 0;

	asm volatile("mrc p14, 0, %[result], c1, c4, 4"
		: [result] "=r" (ret_val)
		: /* no input */
		: /* no clobber */
		);
	return ret_val;
}

static inline void write_dbgprcr(uint32_t val)
{
	asm volatile("mov r8, %[val_in]\n\t"
		"mcr p14, 0, r8, c1, c4, 4"
		: /* no output */
		: [val_in] "r" (val)
		: "r8"
		);
	return;
}

static struct dentry *corenpdrq_debugfs_root;

static ssize_t corenpdrq_toggle_show(struct seq_file *s, void *data)
{
	uint32_t ret_val = 0;

	ret_val = read_dbgprcr();
	seq_printf(s, "%X\n", (ret_val & 0x1));

	return 0;
}

static int corenpdrq_open(struct inode *inode, struct file *file)
{
	return single_open(file, corenpdrq_toggle_show, inode->i_private);
}

static int corenpdrq_write(struct file *file, const char __user *userbuf,
	size_t count, loff_t *f_pos)
{
	char buf[32];
	unsigned long in_val = 0;
	uint32_t val = 0;

	if (sizeof buf <= count)
		goto write_err;

	if (copy_from_user(buf, userbuf, count))
		goto write_err;

	buf[count] = '\0';
	strim(buf);

	if (kstrtoul(buf, 10, &in_val) < 0)
		goto write_err;

	val = read_dbgprcr();
	if (in_val)
		val |= 1;
	else
		val &= ~1;

	write_dbgprcr(val);
	return count;

write_err:
	pr_err("can't program bit CORENPDRQ\n");

	return -EINVAL;
}

static const struct file_operations corenpdrq_fops = {
	.open		= corenpdrq_open,
	.read		= seq_read,
	.write		= corenpdrq_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init corenpdrq_debug_init(void)
{
	corenpdrq_debugfs_root = debugfs_create_dir("corenpdrq", NULL);
	if (!corenpdrq_debugfs_root)
		return -ENOMEM;

	if (!debugfs_create_file(
		"core_npdrq", S_IRUGO | S_IWUSR, corenpdrq_debugfs_root, NULL,
		&corenpdrq_fops))
		goto err_out;

	return 0;

err_out:
	debugfs_remove_recursive(corenpdrq_debugfs_root);
	return -ENOMEM;
}

late_initcall(corenpdrq_debug_init);
