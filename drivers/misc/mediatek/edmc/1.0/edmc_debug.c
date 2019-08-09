/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include "edmc_hw_reg.h"
#include "edmc_debug.h"

#define READ_BUFFER 1024
static struct dentry *edmc_droot;
static char read_buf[READ_BUFFER];

static ssize_t read_edmc_dump_regs(struct file *f, char __user *buf,
			       size_t count, loff_t *off);

static ssize_t read_edmc_dump_other_info(struct file *f, char __user *buf,
			       size_t count, loff_t *off);

static const struct file_operations edmc_dump_regs_ops = {
	.owner = THIS_MODULE,
	.open  = simple_open,
	.read  = read_edmc_dump_regs,
};

static const struct file_operations edmc_dump_other_info_ops = {
	.owner = THIS_MODULE,
	.open  = simple_open,
	.read  = read_edmc_dump_other_info,
};

static ssize_t read_edmc_dump_regs(struct file *f, char __user *buf,
			       size_t count, loff_t *off)
{
	ssize_t read;
	int pos, num, ret;

	//pr_info("[%s] entry\n", __func__);

	pos = *off;
	memset(read_buf, 0, READ_BUFFER);
	num = 0;
	/***make sure the data length is shorter than read_buf**/
	num = sprintf(read_buf, "===[ctl]===\n");
	num += sprintf(read_buf + num,  "[0x00]: %.8x ",
			edmc_reg_read(APU_EDM_CTL_0));
	num += sprintf(read_buf + num, "[0x04]: %.8x ",
		edmc_reg_read(APU_EDM_CFG_0));
	num += sprintf(read_buf + num, "[0x08]: %.8x ",
		edmc_reg_read(APU_EDM_INT_MASK));
	num += sprintf(read_buf + num, "[0x0C]: %.8x\n",
		edmc_reg_read(APU_EDM_DESP_STATUS));
	num += sprintf(read_buf + num, "[0x10]: %.8x ",
		edmc_reg_read(APU_EDM_INT_STATUS));
	num += sprintf(read_buf + num, "[0x14]: %.8x ",
		edmc_reg_read(APU_EDM_ERR_INT_MASK));
	num += sprintf(read_buf + num, "[0x18]: %.8x ",
		edmc_reg_read(APU_EDM_ERR_STATUS));
	num += sprintf(read_buf + num, "[0x1C]: %.8x\n",
		edmc_reg_read(APU_EDM_ERR_INT_STATUS));
	num += sprintf(read_buf + num, "===[0]===\n");
	num += sprintf(read_buf + num, "[0xC00]: %.8x ",
				edmc_reg_read(APU_DESP0_SRC_TILE_WIDTH));
	num += sprintf(read_buf + num, "[0xC04]: %.8x ",
		edmc_reg_read(APU_DESP0_DEST_TILE_WIDTH));
	num += sprintf(read_buf + num, "[0xC08]: %.8x ",
		edmc_reg_read(APU_DESP0_TILE_HEIGHT));
	num += sprintf(read_buf + num, "[0xC0C]: %.8x ",
		edmc_reg_read(APU_DESP0_SRC_STRIDE));
	num += sprintf(read_buf + num, "[0xC10]: %.8x\n",
		edmc_reg_read(APU_DESP0_DEST_STRIDE));
	num += sprintf(read_buf + num, "[0xC14]: %.8x ",
		edmc_reg_read(APU_DESP0_SRC_ADDR_0));
	num += sprintf(read_buf + num, "[0xC1C]: %.8x ",
		edmc_reg_read(APU_DESP0_DEST_ADDR_0));
	num += sprintf(read_buf + num, "[0xC24]: %.8x ",
		edmc_reg_read(APU_DESP0_CTL_0));
	num += sprintf(read_buf + num, "[0xC28]: %.8x\n",
		edmc_reg_read(APU_DESP0_CTL_1));
	num += sprintf(read_buf + num, "[0xC2C]: %.8x ",
		edmc_reg_read(APU_DESP0_FILL_VALUE));
	num += sprintf(read_buf + num, "[0xC30]: %.8x ",
		edmc_reg_read(APU_DESP0_ID));
	num += sprintf(read_buf + num, "[0xC34]: %.8x ",
		edmc_reg_read(APU_DESP0_RANGE_SCALE));
	num += sprintf(read_buf + num, "[0xC38]: %.8x\n",
		edmc_reg_read(APU_DESP0_MIN_FP32));
	num += sprintf(read_buf + num, "===[1]===\n");
	num += sprintf(read_buf + num, "[0xC40]: %.8x ",
				edmc_reg_read(APU_DESP1_SRC_TILE_WIDTH));
	num += sprintf(read_buf + num, "[0xC44]: %.8x ",
		edmc_reg_read(APU_DESP1_DEST_TILE_WIDTH));
	num += sprintf(read_buf + num, "[0xC48]: %.8x ",
		edmc_reg_read(APU_DESP1_TILE_HEIGHT));
	num += sprintf(read_buf + num, "[0xC4C]: %.8x ",
		edmc_reg_read(APU_DESP1_SRC_STRIDE));
	num += sprintf(read_buf + num, "[0xC50]: %.8x\n",
		edmc_reg_read(APU_DESP1_DEST_STRIDE));
	num += sprintf(read_buf + num, "[0xC54]: %.8x ",
		edmc_reg_read(APU_DESP1_SRC_ADDR_0));
	num += sprintf(read_buf + num, "[0xC5C]: %.8x ",
		edmc_reg_read(APU_DESP1_DEST_ADDR_0));
	num += sprintf(read_buf + num, "[0xC64]: %.8x ",
		edmc_reg_read(APU_DESP1_CTL_0));
	num += sprintf(read_buf + num, "[0xC68]: %.8x\n",
		edmc_reg_read(APU_DESP1_CTL_1));
	num += sprintf(read_buf + num, "[0xC6C]: %.8x ",
		edmc_reg_read(APU_DESP1_FILL_VALUE));
	num += sprintf(read_buf + num, "[0xC70]: %.8x ",
		edmc_reg_read(APU_DESP1_ID));
	num += sprintf(read_buf + num, "[0xC74]: %.8x ",
		edmc_reg_read(APU_DESP1_RANGE_SCALE));
	num += sprintf(read_buf + num, "[0xC78]: %.8x\n",
		edmc_reg_read(APU_DESP1_MIN_FP32));
	num += sprintf(read_buf + num, "===[2]===\n");
	num += sprintf(read_buf + num, "[0xC80]: %.8x ",
				edmc_reg_read(APU_DESP2_SRC_TILE_WIDTH));
	num += sprintf(read_buf + num, "[0xC84]: %.8x ",
		edmc_reg_read(APU_DESP2_DEST_TILE_WIDTH));
	num += sprintf(read_buf + num, "[0xC88]: %.8x ",
		edmc_reg_read(APU_DESP2_TILE_HEIGHT));
	num += sprintf(read_buf + num, "[0xC8C]: %.8x ",
		edmc_reg_read(APU_DESP2_SRC_STRIDE));
	num += sprintf(read_buf + num, "[0xC90]: %.8x\n",
		edmc_reg_read(APU_DESP2_DEST_STRIDE));
	num += sprintf(read_buf + num, "[0xC94]: %.8x ",
		edmc_reg_read(APU_DESP2_SRC_ADDR_0));
	num += sprintf(read_buf + num, "[0xC9C]: %.8x ",
		edmc_reg_read(APU_DESP2_DEST_ADDR_0));
	num += sprintf(read_buf + num, "[0xCA4]: %.8x ",
		edmc_reg_read(APU_DESP2_CTL_0));
	num += sprintf(read_buf + num, "[0xCA8]: %.8x\n",
		edmc_reg_read(APU_DESP2_CTL_1));
	num += sprintf(read_buf + num, "[0xCAC]: %.8x ",
		edmc_reg_read(APU_DESP2_FILL_VALUE));
	num += sprintf(read_buf + num, "[0xCB0]: %.8x ",
		edmc_reg_read(APU_DESP2_ID));
	num += sprintf(read_buf + num, "[0xCB4]: %.8x ",
		edmc_reg_read(APU_DESP2_RANGE_SCALE));
	num += sprintf(read_buf + num, "[0xCB8]: %.8x\n",
		edmc_reg_read(APU_DESP2_MIN_FP32));
	num += sprintf(read_buf + num, "===[3]===\n");
	num += sprintf(read_buf + num, "[0xCC0]: %.8x ",
				edmc_reg_read(APU_DESP3_SRC_TILE_WIDTH));
	num += sprintf(read_buf + num, "[0xCC4]: %.8x ",
		edmc_reg_read(APU_DESP3_DEST_TILE_WIDTH));
	num += sprintf(read_buf + num, "[0xCC8]: %.8x ",
		edmc_reg_read(APU_DESP3_TILE_HEIGHT));
	num += sprintf(read_buf + num, "[0xCCC]: %.8x ",
		edmc_reg_read(APU_DESP3_SRC_STRIDE));
	num += sprintf(read_buf + num, "[0xCD0]: %.8x\n",
		edmc_reg_read(APU_DESP3_DEST_STRIDE));
	num += sprintf(read_buf + num, "[0xCD4]: %.8x ",
		edmc_reg_read(APU_DESP3_SRC_ADDR_0));
	num += sprintf(read_buf + num, "[0xCDC]: %.8x ",
		edmc_reg_read(APU_DESP3_DEST_ADDR_0));
	num += sprintf(read_buf + num, "[0xCE4]: %.8x ",
		edmc_reg_read(APU_DESP3_CTL_0));
	num += sprintf(read_buf + num, "[0xCE8]: %.8x\n",
		edmc_reg_read(APU_DESP3_CTL_1));
	num += sprintf(read_buf + num, "[0xCEC]: %.8x ",
		edmc_reg_read(APU_DESP3_FILL_VALUE));
	num += sprintf(read_buf + num, "[0xCF0]: %.8x ",
		edmc_reg_read(APU_DESP3_ID));
	num += sprintf(read_buf + num, "[0xCF4]: %.8x ",
		edmc_reg_read(APU_DESP3_RANGE_SCALE));
	num += sprintf(read_buf + num, "[0xCF8]: %.8x\n",
		edmc_reg_read(APU_DESP3_MIN_FP32));
	if (pos >= num)
		return 0;

	read = num - pos;
	ret = copy_to_user(buf, &read_buf[pos], read);
	if (ret) {
		pr_info("[%s] copy_to_user error, ret =%d\n", __func__, ret);
		return ret;
	}
	*off = *off + read;

	//edmc_debug("%s: \"%s\"\n", __func__, regs_info);

	return read;

}

static ssize_t read_edmc_dump_other_info(struct file *f, char __user *buf,
			       size_t count, loff_t *off)
{
	ssize_t read;
	int pos, num, ret;

	pr_info("[%s] entry\n", __func__);

	pos = *off;

	memset(read_buf, 0, READ_BUFFER);

	/***make sure the data length is shorter than read_buf**/
	num = sprintf(read_buf,  "g_edmc_seq_job = %llu\n",
			g_edmc_seq_job);
	num += sprintf(read_buf + num,  "g_edmc_seq_finish = %llu\n",
		g_edmc_seq_finish);
	num += sprintf(read_buf + num,	"g_edmc_seq_error = %llu\n",
		g_edmc_seq_error);
	num += sprintf(read_buf + num,	"cmd_list_len = %llu\n",
		cmd_list_len);
	num += sprintf(read_buf + num,	"g_edmc_seq_last = %llu\n",
		g_edmc_seq_last);

	if (pos >= num)
		return 0;

	read = num - pos;
	ret = copy_to_user(buf, &read_buf[pos], read);
	if (ret) {
		pr_info("[%s] copy_to_user error, ret =%d\n", __func__, ret);
		return ret;
	}

	*off = *off + read;

	//edmc_debug("%s: \"%s\"\n", __func__, regs_info);

	return read;

}

int edmc_debugfs_init(void)
{
	int ret;

	pr_info("[%s] entry\n", __func__);

	edmc_droot = debugfs_create_dir("edmc", NULL);

	ret = IS_ERR_OR_NULL(edmc_droot);
	if (ret) {
		pr_info("[%s]failed to create debug dir.\n", __func__);
		return -1;
	}

	if (!debugfs_create_u32("log_level", 0664,
			edmc_droot, &g_edmc_log_level))
		goto error;

	if (!debugfs_create_u32("power_off_time", 0664,
			edmc_droot, &g_edmc_poweroff_time))
		goto error;
#if 0 //Not support
	if (!debugfs_create_file("dump_regs", 0444,
			edmc_droot, NULL, &edmc_dump_regs_ops))
		goto error;
#endif
	if (!debugfs_create_file("dump_other_info", 0444,
			edmc_droot, NULL, &edmc_dump_other_info_ops))
		goto error;

	pr_info("[%s] suceess!!!!\n", __func__);

	return 0;
error:
	pr_info("[%s] error happened!!!!\n", __func__);
	debugfs_remove_recursive(edmc_droot);
	return -ENOMEM;
}

void edmc_debugfs_exit(void)
{
	pr_info("[%s] entry\n", __func__);

	debugfs_remove_recursive(edmc_droot);
}
