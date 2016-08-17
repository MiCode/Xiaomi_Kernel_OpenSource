/*
 * arch/arm/mach-tegra/board-info.c
 *
 * File to contain changes for sku id, serial id, chip id, etc.
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "board.h"

/* skuinfo is 18 character long xxx-xxxxx-xxxx-xxx
 *so buffer size is set to 18+1 */
#define SKUINFO_BUF_SIZE 19
/* skuver is 2 character long XX so buffer size is set to 2+1 */
#define SKUVER_BUF_SIZE 3

/* prodinfo is 18 character long xxx-xxxxx-xxxx-xxx
 * so buffer size is set to 18+1 */
#define PRODINFO_BUF_SIZE 19
/* prodver is 2 character long XX so buffer size is set to 2+1 */
#define PRODVER_BUF_SIZE 3

extern unsigned int system_serial_low;
extern unsigned int system_serial_high;

static unsigned int board_serial_low;
static unsigned int board_serial_high;
static char prodinfo_buffer[PRODINFO_BUF_SIZE];
static char prodver_buffer[PRODVER_BUF_SIZE];
static char skuinfo_buffer[SKUINFO_BUF_SIZE];
static char skuver_buffer[SKUVER_BUF_SIZE];

static int __init sku_info_setup(char *line)
{
	memcpy(skuinfo_buffer, line, SKUINFO_BUF_SIZE);
	return 1;
}

__setup("nvsku=", sku_info_setup);

static int __init sku_ver_setup(char *line)
{
	memcpy(skuver_buffer, line, SKUVER_BUF_SIZE);
	return 1;
}

__setup("SkuVer=", sku_ver_setup);

static int __init prod_info_setup(char *line)
{
	memcpy(prodinfo_buffer, line, PRODINFO_BUF_SIZE);
	return 1;
}

__setup("ProdInfo=", prod_info_setup);

static int __init prod_ver_setup(char *line)
{
	memcpy(prodver_buffer, line, PRODVER_BUF_SIZE);
	return 1;
}

__setup("ProdVer=", prod_ver_setup);

static int read_skuinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", skuinfo_buffer);
	return 0;
}

static int read_skuinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_skuinfo_proc_show, NULL);
}

static const struct file_operations read_skuinfo_proc_fops = {
	.open = read_skuinfo_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int read_skuver_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", skuver_buffer);
	return 0;
}

static int read_skuver_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_skuver_proc_show, NULL);
}

static const struct file_operations read_skuver_proc_fops = {
	.open = read_skuver_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int read_serialinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%013llu\n",
		   (((unsigned long long int)board_serial_high) << 32) |
		   board_serial_low);
	return 0;
}

static int read_serialinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_serialinfo_proc_show, NULL);
}

static const struct file_operations read_serialinfo_proc_fops = {
	.open = read_serialinfo_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int read_prodinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", prodinfo_buffer);
	return 0;
}

static int read_prodinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_prodinfo_proc_show, NULL);
}

static const struct file_operations read_prodinfo_proc_fops = {
	.open = read_prodinfo_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int read_prodver_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", prodver_buffer);
	return 0;
}

static int read_prodver_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_prodver_proc_show, NULL);
}

static const struct file_operations read_prodver_proc_fops = {
	.open = read_prodver_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int __init tegra_init_board_info(void)
{
	board_serial_low = system_serial_low;
	board_serial_high = system_serial_high;

	if (!proc_create("board_serial", 0, NULL, &read_serialinfo_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for board_serial!\n");
		return -1;
	}

	if (!proc_create("skuinfo", 0, NULL, &read_skuinfo_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for skuinfo!\n");
		return -1;
	}

	if (!proc_create("skuver", 0, NULL, &read_skuver_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for skuver!\n");
		return -1;
	}

	if (!proc_create("prodinfo", 0, NULL, &read_prodinfo_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for prodinfo!\n");
		return -1;
	}

	if (!proc_create("prodver", 0, NULL, &read_prodver_proc_fops)) {
		printk(KERN_ERR "Can't create proc entry for prodver!\n");
		return -1;
	}

	return 0;
}
