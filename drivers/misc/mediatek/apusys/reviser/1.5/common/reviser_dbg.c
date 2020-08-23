// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/io.h>

#include "reviser_drv.h"
#include "reviser_cmn.h"
#include "reviser_dbg.h"
#include "reviser_table_mgt.h"
#include "reviser_mem.h"
#include "reviser_hw_mgt.h"
#include "reviser_power.h"

static struct dentry *reviser_dbg_root;
//hw
static struct dentry *reviser_dbg_hw;
static struct dentry *reviser_dbg_remap_table;
static struct dentry *reviser_dbg_context_ID;
static struct dentry *reviser_dbg_boundary;
static struct dentry *reviser_dbg_iova;

//table
static struct dentry *reviser_dbg_table;
static struct dentry *reviser_dbg_table_vlm;
static struct dentry *reviser_dbg_table_vlm_ctx;
static struct dentry *reviser_dbg_table_ctx;
static struct dentry *reviser_dbg_table_tcm;
//mem
static struct dentry *reviser_dbg_mem;
static struct dentry *reviser_dbg_mem_tcm;
static struct dentry *reviser_dbg_mem_tcm_bank;

static struct dentry *reviser_dbg_mem_vlm;

static struct dentry *reviser_dbg_mem_dram;
static struct dentry *reviser_dbg_mem_dram_bank;
static struct dentry *reviser_dbg_mem_dram_ctx;
static struct dentry *reviser_dbg_log;

static struct dentry *reviser_dbg_err;
static struct dentry *reviser_dbg_err_info;
static struct dentry *reviser_dbg_err_reg;
static struct dentry *reviser_dbg_err_debug;


u8 g_reviser_log_level = REVISER_LOG_INFO;
static uint32_t g_reviser_vlm_ctx;
static uint32_t g_reviser_mem_tcm_bank;
static uint32_t g_reviser_mem_dram_bank;
static uint32_t g_reviser_mem_dram_ctx;

//----------------------------------------------
// user table dump
static int reviser_dbg_show_remap_table(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *rdv = s->private;

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -EINVAL;
	}
	reviser_mgt_dmp_rmp(rdv, s);
	return 0;
}

static int reviser_dbg_remap_table_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			reviser_dbg_show_remap_table, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_remap_table = {
	.open = reviser_dbg_remap_table_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};

//----------------------------------------------
//----------------------------------------------
// context ID table
static int reviser_dbg_show_context_ID(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *rdv = s->private;

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -EINVAL;
	}
	reviser_mgt_dmp_ctx(rdv, s);
	return 0;
}

static int reviser_dbg_context_ID_open(struct inode *inode, struct file *file)
{
	return single_open(file, reviser_dbg_show_context_ID, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_context_ID = {
	.open = reviser_dbg_context_ID_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};

//----------------------------------------------
//----------------------------------------------
//----------------------------------------------
// context boundary table
static int reviser_dbg_show_boundary(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *rdv = s->private;

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -EINVAL;
	}
	reviser_mgt_dmp_boundary(rdv, s);
	return 0;
}

static int reviser_dbg_boundary_open(struct inode *inode, struct file *file)
{
	return single_open(file, reviser_dbg_show_boundary, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_boundary = {
	.open = reviser_dbg_boundary_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};
//----------------------------------------------
// context iova
static int reviser_dbg_show_iova(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *rdv = s->private;

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -EINVAL;
	}
	reviser_mgt_dmp_default(rdv, s);
	return 0;
}

static int reviser_dbg_iova_open(struct inode *inode, struct file *file)
{
	return single_open(file, reviser_dbg_show_iova, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_iova = {
	.open = reviser_dbg_iova_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};

//----------------------------------------------
//----------------------------------------------
// vlm table
static int reviser_dbg_show_table_vlm(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *rdv = s->private;

	reviser_table_print_vlm(rdv, g_reviser_vlm_ctx, s);
	return 0;
}

static int reviser_dbg_table_vlm_open(struct inode *inode, struct file *file)
{
	return single_open(file, reviser_dbg_show_table_vlm, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_table_vlm = {
	.open = reviser_dbg_table_vlm_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};

//----------------------------------------------
//----------------------------------------------
// contex id table
static int reviser_dbg_show_table_ctx(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *rdv = s->private;

	reviser_table_print_ctx(rdv, s);

	return 0;
}

static int reviser_dbg_table_ctx_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			reviser_dbg_show_table_ctx, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_table_ctx = {
	.open = reviser_dbg_table_ctx_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};
//----------------------------------------------
//----------------------------------------------
// tcm table
static int reviser_dbg_show_table_tcm(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *rdv = s->private;

	reviser_table_print_tcm(rdv, s);

	return 0;
}

static int reviser_dbg_table_tcm_open(struct inode *inode, struct file *file)
{
	return single_open(file, reviser_dbg_show_table_tcm, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_table_tcm = {
	.open = reviser_dbg_table_tcm_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};
//----------------------------------------------
//----------------------------------------------
// tcm mem
static ssize_t reviser_dbg_read_mem_tcm(struct file *filp, char *buffer,
	size_t length, loff_t *offset)
{
	struct reviser_dev_info *rdv = filp->private_data;
	int res = 0;
	unsigned char *vbuffer;

	if (g_reviser_mem_tcm_bank >= rdv->plat.tcm_bank_max) {
		LOG_ERR("copy TCM out of range. %d\n", g_reviser_mem_tcm_bank);
		return -EINVAL;
	}

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -EINVAL;
	}

	vbuffer = (unsigned char *)
			__get_free_pages(GFP_KERNEL, get_order(rdv->plat.bank_size));

	if (vbuffer == NULL) {
		LOG_ERR("allocate fail 0x%x\n", rdv->plat.bank_size);
		return res;
	}
	LOG_DEBUG("copy buffer size 0x%x ...\n", rdv->plat.bank_size);

	memcpy_fromio(vbuffer,
			rdv->tcm_base +
			g_reviser_mem_tcm_bank * rdv->plat.bank_size,
			rdv->plat.bank_size);

	res = simple_read_from_buffer(buffer, length, offset,
			vbuffer, rdv->plat.bank_size);

	//vfree(vbuffer);

	free_pages((unsigned long)vbuffer, get_order(rdv->plat.bank_size));
	return res;
}

static const struct file_operations reviser_dbg_fops_mem_tcm = {
	.read = reviser_dbg_read_mem_tcm,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

//----------------------------------------------
//----------------------------------------------
// dram
static ssize_t reviser_dbg_read_mem_dram(struct file *filp, char *buffer,
	size_t length, loff_t *offset)
{
	struct reviser_dev_info *rdv = filp->private_data;
	int res = 0;
	uint64_t dram_offset;
	unsigned int ctx_max = 0;
	unsigned int dram_max = 0;

	if (g_reviser_mem_dram_bank >= rdv->plat.vlm_bank_max) {
		LOG_ERR("copy dram bank out of range. %d\n",
				g_reviser_mem_dram_bank);
		return res;
	}

	ctx_max = rdv->plat.mdla_max + rdv->plat.vpu_max
			+ rdv->plat.edma_max + rdv->plat.up_max;


	if (g_reviser_mem_dram_ctx >= ctx_max) {
		LOG_ERR("copy dram ctx out of range. %d\n",
				g_reviser_mem_dram_ctx);
		return res;
	}

	dram_max = rdv->plat.vlm_size * ctx_max;

	dram_offset = g_reviser_mem_dram_ctx * rdv->plat.vlm_size +
			g_reviser_mem_dram_bank * rdv->plat.bank_size;
	if (dram_offset >= dram_max) {
		LOG_ERR("copy dram out of range. 0x%llx\n", dram_offset);
		return res;
	}


	res = simple_read_from_buffer(buffer, length, offset,
			rdv->dram_base + dram_offset, rdv->plat.bank_size);

	return res;
}

static const struct file_operations reviser_dbg_fops_mem_dram = {
	.read = reviser_dbg_read_mem_dram,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

//----------------------------------------------
//----------------------------------------------
// show plat info

static void reviser_print_plat(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser platform info\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "boundary: 0x%x\n", rdv->plat.boundary);
	LOG_CON(s, "bank_size: 0x%x\n", rdv->plat.bank_size);
	LOG_CON(s, "vlm_size: 0x%x\n", rdv->plat.vlm_size);
	LOG_CON(s, "vlm_bank_max: 0x%x\n", rdv->plat.vlm_bank_max);
	LOG_CON(s, "rmp_max: 0x%x\n", rdv->plat.rmp_max);
	LOG_CON(s, "ctx_max: 0x%x\n", rdv->plat.ctx_max);
	LOG_CON(s, "tcm_size: 0x%x\n", rdv->plat.tcm_size);
	LOG_CON(s, "tcm_bank_max: 0x%x\n", rdv->plat.tcm_bank_max);
	LOG_CON(s, "mdla_max: 0x%x\n", rdv->plat.mdla_max);
	LOG_CON(s, "vpu_max: 0x%x\n", rdv->plat.vpu_max);
	LOG_CON(s, "edma_max: 0x%x\n", rdv->plat.edma_max);
	LOG_CON(s, "up_max: 0x%x\n", rdv->plat.up_max);
	LOG_CON(s, "dram_offset: 0x%x\n", rdv->plat.dram_offset);
	LOG_CON(s, "tcm_addr: 0x%x\n", rdv->plat.tcm_addr);
	LOG_CON(s, "vlm_addr: 0x%x\n", rdv->plat.vlm_addr);
	LOG_CON(s, "hw_ver: 0x%x\n", rdv->plat.hw_ver);
	LOG_CON(s, "=============================\n");
}
//----------------------------------------------
//----------------------------------------------
// show exception reg
static int reviser_dbg_show_dbg_reg(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *rdv = s->private;

	reviser_print_plat(rdv, s);
	return 0;
}

static int reviser_dbg_err_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			reviser_dbg_show_dbg_reg, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_err_dbg = {
	.open = reviser_dbg_err_dbg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};
//----------------------------------------------
//----------------------------------------------
// show err info
static void reviser_print_error(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	struct seq_file *s = (struct seq_file *)s_file;
	unsigned long flags;
	int count = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	spin_lock_irqsave(&rdv->lock_dump, flags);
	count = rdv->dump.err_count;
	spin_unlock_irqrestore(&rdv->lock_dump, flags);
	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser error info\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "count: %d\n", count);
	LOG_CON(s, "=============================\n");


}

static int reviser_dbg_show_err_info(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *rdv = s->private;

	reviser_print_error(rdv, s);
	return 0;
}

static int reviser_dbg_err_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, reviser_dbg_show_err_info, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_err_info = {
	.open = reviser_dbg_err_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};

//----------------------------------------------
//----------------------------------------------
// show vlm
static int reviser_dbg_show_mem_vlm(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *rdv = s->private;

	reviser_print_dram(rdv, s);

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return 0;
	}
	reviser_print_tcm(rdv, s);
	return 0;
}

static int reviser_dbg_mem_vlm_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			reviser_dbg_show_mem_vlm, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_mem_vlm = {
	.open = reviser_dbg_mem_vlm_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};//----------------------------------------------
// show exception reg
static int reviser_dbg_show_err_reg(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *rdv = s->private;

	if (!reviser_is_power(rdv)) {
		LOG_ERR("Can Not Read when power disable\n");
		return 0;
	}
	reviser_mgt_dmp_exception(rdv, s);
	return 0;
}

static int reviser_dbg_err_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			reviser_dbg_show_err_reg, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_err_reg = {
	.open = reviser_dbg_err_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};
//----------------------------------------------
int reviser_dbg_init(struct reviser_dev_info *rdv)
{
	int ret = 0;

	g_reviser_vlm_ctx = 0;
	g_reviser_mem_tcm_bank = 0;
	g_reviser_mem_dram_bank = 0;
	g_reviser_mem_dram_ctx = 0;

	reviser_dbg_root = debugfs_create_dir(REVISER_DBG_DIR, NULL);
	reviser_dbg_table = debugfs_create_dir(REVISER_DBG_SUBDIR_TABLE,
			reviser_dbg_root);
	reviser_dbg_mem = debugfs_create_dir(REVISER_DBG_SUBDIR_MEM,
			reviser_dbg_root);
	reviser_dbg_hw = debugfs_create_dir(REVISER_DBG_SUBDIR_HW,
			reviser_dbg_root);
	reviser_dbg_err = debugfs_create_dir(REVISER_DBG_SUBDIR_ERR,
			reviser_dbg_root);

	ret = IS_ERR_OR_NULL(reviser_dbg_root);
	if (ret) {
		LOG_ERR("failed to create debug dir.\n");
		goto out;
	}

	reviser_dbg_remap_table = debugfs_create_file("remap_table", 0644,
			reviser_dbg_hw, rdv,
			&reviser_dbg_fops_remap_table);

	ret = IS_ERR_OR_NULL(reviser_dbg_remap_table);
	if (ret) {
		LOG_ERR("failed to create debug node(remap_table).\n");
		goto out;
	}

	reviser_dbg_context_ID = debugfs_create_file("ctx", 0644,
			reviser_dbg_hw, rdv,
			&reviser_dbg_fops_context_ID);

	ret = IS_ERR_OR_NULL(reviser_dbg_context_ID);
	if (ret) {
		LOG_ERR("failed to create debug node(context_ID).\n");
		goto out;
	}

	reviser_dbg_boundary = debugfs_create_file("boundary", 0644,
			reviser_dbg_hw, rdv,
			&reviser_dbg_fops_boundary);

	ret = IS_ERR_OR_NULL(reviser_dbg_boundary);
	if (ret) {
		LOG_ERR("failed to create debug node(boundary).\n");
		goto out;
	}

	reviser_dbg_iova = debugfs_create_file("iova", 0644,
			reviser_dbg_hw, rdv,
			&reviser_dbg_fops_iova);

	ret = IS_ERR_OR_NULL(reviser_dbg_iova);
	if (ret) {
		LOG_ERR("failed to create debug node(iova).\n");
		goto out;
	}

	reviser_dbg_table_vlm = debugfs_create_file("vlm", 0644,
			reviser_dbg_table, rdv,
			&reviser_dbg_fops_table_vlm);

	ret = IS_ERR_OR_NULL(reviser_dbg_table_vlm);
	if (ret) {
		LOG_ERR("failed to create debug node(table_vlm).\n");
		goto out;
	}

	reviser_dbg_table_vlm_ctx = debugfs_create_u32("vlm_select", 0644,
			reviser_dbg_table,
			&g_reviser_vlm_ctx);

	ret = IS_ERR_OR_NULL(reviser_dbg_table_vlm_ctx);
	if (ret) {
		LOG_ERR("failed to create debug node(vlm_select).\n");
		goto out;
	}

	reviser_dbg_table_ctx = debugfs_create_file("ctx", 0644,
			reviser_dbg_table, rdv,
			&reviser_dbg_fops_table_ctx);

	ret = IS_ERR_OR_NULL(reviser_dbg_table_ctx);
	if (ret) {
		LOG_ERR("failed to create debug node(ctx).\n");
		goto out;
	}

	reviser_dbg_table_tcm = debugfs_create_file("tcm", 0644,
			reviser_dbg_table, rdv,
			&reviser_dbg_fops_table_tcm);

	ret = IS_ERR_OR_NULL(reviser_dbg_table_tcm);
	if (ret) {
		LOG_ERR("failed to create debug node(tcm).\n");
		goto out;
	}
	/*  dump tcm */
	reviser_dbg_mem_tcm_bank = debugfs_create_u32("tcm_bank", 0644,
			reviser_dbg_mem, &g_reviser_mem_tcm_bank);

	ret = IS_ERR_OR_NULL(reviser_dbg_mem_tcm_bank);
	if (ret) {
		LOG_ERR("failed to create debug node(tcm_bank).\n");
		goto out;
	}
	reviser_dbg_mem_tcm = debugfs_create_file("tcm", 0644,
			reviser_dbg_mem, rdv,
			&reviser_dbg_fops_mem_tcm);

	ret = IS_ERR_OR_NULL(reviser_dbg_mem_tcm);
	if (ret) {
		LOG_ERR("failed to create debug node(tcm).\n");
		goto out;
	}

	/*  dump dram */
	reviser_dbg_mem_dram_bank = debugfs_create_u32("dram_bank", 0644,
			reviser_dbg_mem,
			&g_reviser_mem_dram_bank);

	ret = IS_ERR_OR_NULL(reviser_dbg_mem_dram_bank);
	if (ret) {
		LOG_ERR("failed to create debug node(dram_bank).\n");
		goto out;
	}

	reviser_dbg_mem_dram_ctx = debugfs_create_u32("dram_ctx", 0644,
			reviser_dbg_mem,
			&g_reviser_mem_dram_ctx);

	ret = IS_ERR_OR_NULL(reviser_dbg_mem_dram_ctx);
	if (ret) {
		LOG_ERR("failed to create debug node(dram_ctx).\n");
		goto out;
	}

	reviser_dbg_mem_dram = debugfs_create_file("dram", 0644,
			reviser_dbg_mem, rdv,
			&reviser_dbg_fops_mem_dram);

	ret = IS_ERR_OR_NULL(reviser_dbg_mem_dram);
	if (ret) {
		LOG_ERR("failed to create debug node(dram).\n");
		goto out;
	}

	/* create log level */
	reviser_dbg_log = debugfs_create_u8("log_level", 0644,
			reviser_dbg_root, &g_reviser_log_level);


	reviser_dbg_err_info = debugfs_create_file("info", 0644,
			reviser_dbg_err, rdv,
			&reviser_dbg_fops_err_info);

	ret = IS_ERR_OR_NULL(reviser_dbg_err_info);
	if (ret) {
		LOG_ERR("failed to create debug node(count).\n");
		goto out;
	}

	reviser_dbg_mem_vlm = debugfs_create_file("vlm", 0644,
			reviser_dbg_mem, rdv,
			&reviser_dbg_fops_mem_vlm);

	ret = IS_ERR_OR_NULL(reviser_dbg_mem_vlm);
	if (ret) {
		LOG_ERR("failed to create debug node(vlm).\n");
		goto out;
	}


	reviser_dbg_err_reg = debugfs_create_file("reg", 0644,
			reviser_dbg_err, rdv,
			&reviser_dbg_fops_err_reg);

	ret = IS_ERR_OR_NULL(reviser_dbg_err_reg);
	if (ret) {
		LOG_ERR("failed to create debug node(reg).\n");
		goto out;
	}

	reviser_dbg_err_debug = debugfs_create_file("debug", 0644,
			reviser_dbg_err, rdv,
			&reviser_dbg_fops_err_dbg);

	ret = IS_ERR_OR_NULL(reviser_dbg_err_reg);
	if (ret) {
		LOG_ERR("failed to create debug node(reg).\n");
		goto out;
	}

out:
	return ret;
}

int reviser_dbg_destroy(struct reviser_dev_info *rdv)
{
	debugfs_remove_recursive(reviser_dbg_root);

	return 0;
}
