/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <misc/wcn_bus.h>

#include "mdbg_type.h"
#include "../include/wcn_dbg.h"
#include "../sleep/slp_mgr.h"

#define WCN_REG_MAX_LEN 256
struct wcn_reg_ctl {
	unsigned int addr;
	unsigned int len;
	/* 1: rw_extended; 0:rw_direct */
	unsigned int rw_extended;
	/*
	 * make sure sdio critical buf >512,
	 * but the frame size is larger than 2048
	 * bytes is not permission in kernel to android
	 */
	unsigned int value[WCN_REG_MAX_LEN];
};

static ssize_t read_wcn_reg(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	struct wcn_reg_ctl *reg_rd;
	int i;

	WCN_INFO("__read_wcn_reg\n");

	reg_rd = kmalloc(sizeof(*reg_rd), GFP_KERNEL);
	if (!reg_rd) {
		WCN_INFO("%s alloc erro\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(reg_rd, user_buf, sizeof(*reg_rd))) {
		WCN_ERR("reg value copy's ret value is -eFAULT\n");
		kfree(reg_rd);
		return -EFAULT;
	}

	if (reg_rd->len > WCN_REG_MAX_LEN) {
		WCN_INFO("%s len is to long:%d\n", __func__, reg_rd->len);
		kfree(reg_rd);
		return -EPERM;
	}

	/* rw_direct SDIO */
	if (reg_rd->rw_extended == 0) {
		for (i = 0; i < reg_rd->len; i++)
			sprdwcn_bus_aon_readb(
					reg_rd->addr + i,
					(unsigned char *)&reg_rd->value[i]);
	} else {
		/* rw_extended reg */
		switch (reg_rd->len) {
		case 1:
			sprdwcn_bus_reg_read(reg_rd->addr,
					     &reg_rd->value[0], 4);
			break;
		default:
			sprdwcn_bus_direct_read(reg_rd->addr, reg_rd->value,
						reg_rd->len * sizeof(int));
			break;
		}
	}

	if (copy_to_user(user_buf, reg_rd, sizeof(*reg_rd))) {
		WCN_ERR("reg copy_to_user ret value is -eFAULT\n");
		kfree(reg_rd);
		return -EFAULT;
	}

	kfree(reg_rd);

	return count;
}

static ssize_t write_wcn_reg(struct file *file, const char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	struct wcn_reg_ctl *reg_wr;
	int i;

	WCN_INFO("_write_wcn_reg\n");

	reg_wr = kmalloc(sizeof(*reg_wr), GFP_KERNEL);
	if (!reg_wr) {
		WCN_INFO("%s alloc erro\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(reg_wr, user_buf, sizeof(*reg_wr))) {
		WCN_ERR("%s copy's ret value is -eFAULT\n", __func__);
		kfree(reg_wr);
		return -EFAULT;
	}

	if (reg_wr->len > WCN_REG_MAX_LEN) {
		WCN_INFO("%s len is to long:%d\n", __func__, reg_wr->len);
		kfree(reg_wr);
		return -EPERM;
	}

	/* rw_direct SDIO */
	if (reg_wr->rw_extended == 0) {
		for (i = 0; i < reg_wr->len; i++)
			sprdwcn_bus_aon_writeb(reg_wr->addr + i,
					       (unsigned char)reg_wr->value[i]);
	} else {
		/* rw_extended reg */
		switch (reg_wr->len) {
		case 1:
			sprdwcn_bus_reg_write(reg_wr->addr,
					      &reg_wr->value[0], 4);
			break;
		default:
			sprdwcn_bus_direct_write(reg_wr->addr, reg_wr->value,
						 reg_wr->len * sizeof(int));
			break;
		}
	}

	kfree(reg_wr);

	return count;

}

static const struct file_operations reg_debug_fops = {
	.read = read_wcn_reg,
	.write = write_wcn_reg,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

int wcn_init_debugfs(void)
{
	struct dentry *ent, *root = debugfs_create_dir("wcn", NULL);

	if (!root)
		return -ENXIO;

	ent = debugfs_create_file("regctl", 0644,
			    (struct dentry *)root, NULL,
			    &reg_debug_fops);

	return PTR_ERR_OR_ZERO(ent);
}
