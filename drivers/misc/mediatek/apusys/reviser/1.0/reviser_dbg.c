/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include "reviser_hw.h"
#include "reviser_mem_mgt.h"

struct dentry *reviser_dbg_root;
//hw
struct dentry *reviser_dbg_hw;
struct dentry *reviser_dbg_remap_table;
struct dentry *reviser_dbg_context_ID;
struct dentry *reviser_dbg_boundary;
struct dentry *reviser_dbg_iova;

//table
struct dentry *reviser_dbg_table;
struct dentry *reviser_dbg_table_vlm;
struct dentry *reviser_dbg_table_vlm_ctxid;
struct dentry *reviser_dbg_table_ctxid;
struct dentry *reviser_dbg_table_tcm;
//mem
struct dentry *reviser_dbg_mem;
struct dentry *reviser_dbg_mem_tcm;
struct dentry *reviser_dbg_mem_tcm_bank;

struct dentry *reviser_dbg_mem_vlm;

struct dentry *reviser_dbg_mem_dram;
struct dentry *reviser_dbg_mem_dram_bank;
struct dentry *reviser_dbg_mem_dram_ctxid;
struct dentry *reviser_dbg_mem_swap;
struct dentry *reviser_dbg_log;

struct dentry *reviser_dbg_err;
struct dentry *reviser_dbg_err_info;
struct dentry *reviser_dbg_err_reg;

struct dentry *reviser_dbg_rw;

u8 g_reviser_log_level = REVISER_LOG_INFO;
uint32_t g_reviser_vlm_ctxid;
uint32_t g_reviser_mem_tcm_bank;
uint32_t g_reviser_mem_dram_bank;
uint32_t g_reviser_mem_dram_ctxid;

//----------------------------------------------
// test node
static int reviser_dbg_show_rw(struct seq_file *s, void *unused)
{
/* Debug only*/
#if 0
	struct reviser_dev_info *reviser_device = s->private;

	if (!reviser_is_power(reviser_device)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -EINVAL;
	}

	reviser_print_rw(reviser_device, s);

#endif
	return 0;
}

static int reviser_dbg_rw_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			reviser_dbg_show_rw, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_rw = {
	.open = reviser_dbg_rw_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	//.write = seq_write,
};

//----------------------------------------------
// user table dump
static int reviser_dbg_show_remap_table(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *reviser_device = s->private;

	if (!reviser_is_power(reviser_device)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -EINVAL;
	}

	reviser_print_remap_table(reviser_device, s);
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
	struct reviser_dev_info *reviser_device = s->private;

	if (!reviser_is_power(reviser_device)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -EINVAL;
	}

	reviser_print_context_ID(reviser_device, s);
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
	struct reviser_dev_info *reviser_device = s->private;

	if (!reviser_is_power(reviser_device)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -EINVAL;
	}

	reviser_print_boundary(reviser_device, s);
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
	struct reviser_dev_info *reviser_device = s->private;

	if (!reviser_is_power(reviser_device)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -EINVAL;
	}

	reviser_print_default_iova(reviser_device, s);
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
	struct reviser_dev_info *reviser_device = s->private;

	reviser_table_print_vlm(reviser_device, g_reviser_vlm_ctxid, s);
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
static int reviser_dbg_show_table_ctxid(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *reviser_device = s->private;

	reviser_table_print_ctxID(reviser_device, s);

	return 0;
}

static int reviser_dbg_table_ctxid_open(struct inode *inode, struct file *file)
{
	return single_open(file,
			reviser_dbg_show_table_ctxid, inode->i_private);
}

static const struct file_operations reviser_dbg_fops_table_ctxid = {
	.open = reviser_dbg_table_ctxid_open,
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
	struct reviser_dev_info *reviser_device = s->private;

	reviser_table_print_tcm(reviser_device, s);

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
	struct reviser_dev_info *reviser_device = filp->private_data;
	int res = 0;
	unsigned char *vbuffer;

	if (!reviser_device->tcm_base) {
		LOG_ERR("No TCM\n");
		return -EINVAL;
	}

	if (g_reviser_mem_tcm_bank >= VLM_TCM_BANK_MAX) {
		LOG_ERR("copy TCM out of range. %d\n", g_reviser_mem_tcm_bank);
		return -EINVAL;
	}

	if (!reviser_is_power(reviser_device)) {
		LOG_ERR("Can Not Read when power disable\n");
		return -EINVAL;
	}

	vbuffer = (unsigned char *)
			__get_free_pages(GFP_KERNEL, get_order(VLM_BANK_SIZE));

	if (vbuffer == NULL) {
		LOG_ERR("allocate fail 0x%x\n", VLM_BANK_SIZE);
		return res;
	}
	LOG_DEBUG("copy buffer size 0x%x ...\n", VLM_BANK_SIZE);

	memcpy_fromio(vbuffer,
			reviser_device->tcm_base +
			g_reviser_mem_tcm_bank * VLM_BANK_SIZE,
			VLM_BANK_SIZE);

	res = simple_read_from_buffer(buffer, length, offset,
			vbuffer, VLM_BANK_SIZE);

	//vfree(vbuffer);

	free_pages((unsigned long)vbuffer, get_order(VLM_BANK_SIZE));
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
	struct reviser_dev_info *reviser_device = filp->private_data;
	int res = 0;
	uint64_t dram_offset;

	if (g_reviser_mem_dram_bank >= VLM_DRAM_BANK_MAX) {
		LOG_ERR("copy dram bank out of range. %d\n",
				g_reviser_mem_dram_bank);
		return res;
	}
	if (g_reviser_mem_dram_ctxid >= VLM_CTXT_CTX_ID_COUNT) {
		LOG_ERR("copy dram ctxid out of range. %d\n",
				g_reviser_mem_dram_ctxid);
		return res;
	}
	dram_offset = g_reviser_mem_dram_ctxid * VLM_CTXT_DRAM_OFFSET +
			g_reviser_mem_dram_bank * VLM_BANK_SIZE;
	if (dram_offset >= REMAP_DRAM_SIZE) {
		LOG_ERR("copy dram out of range. 0x%llx\n", dram_offset);
		return res;
	}


	res = simple_read_from_buffer(buffer, length, offset,
			reviser_device->dram_base + dram_offset, VLM_BANK_SIZE);

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
// swap
static ssize_t reviser_dbg_read_mem_swap(struct file *filp, char *buffer,
	size_t length, loff_t *offset)
{
	struct reviser_dev_info *reviser_device = filp->private_data;
	int res = 0;
	uint64_t dram_offset;
	uint64_t addr = 0;

	if (g_reviser_mem_dram_bank >= VLM_DRAM_BANK_MAX) {
		LOG_ERR("copy dram bank out of range. %d\n",
				g_reviser_mem_dram_bank);
		return res;
	}
	if (g_reviser_mem_dram_ctxid >= VLM_CTXT_CTX_ID_COUNT) {
		LOG_ERR("copy dram ctxid out of range. %d\n",
				g_reviser_mem_dram_ctxid);
		return res;
	}
	dram_offset = g_reviser_mem_dram_bank * VLM_BANK_SIZE;
	if (dram_offset >= REMAP_DRAM_SIZE) {
		LOG_ERR("copy dram out of range. 0x%llx\n",
				dram_offset);
		return res;
	}

	mutex_lock(&reviser_device->mutex_vlm_pgtable);
	addr = reviser_device->pvlm[g_reviser_mem_dram_ctxid].swap_addr;
	if (addr == 0) {
		LOG_ERR("copy dram no swap_addr 0x%llx\n", addr);
		goto fail;
	}

	res = simple_read_from_buffer(buffer, length, offset,
			(void *) addr + dram_offset,
			VLM_BANK_SIZE);
fail:
	mutex_unlock(&reviser_device->mutex_vlm_pgtable);
	return res;
}

static const struct file_operations reviser_dbg_fops_mem_swap = {
	.read = reviser_dbg_read_mem_swap,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

//----------------------------------------------
//----------------------------------------------
// context error info
static int reviser_dbg_show_err_info(struct seq_file *s, void *unused)
{
	struct reviser_dev_info *reviser_device = s->private;

	reviser_print_error(reviser_device, s);
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
	struct reviser_dev_info *reviser_device = s->private;

	reviser_print_dram(reviser_device, s);

	if (!reviser_is_power(reviser_device)) {
		LOG_ERR("Can Not Read when power disable\n");
		return 0;
	}
	reviser_print_tcm(reviser_device, s);
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
	struct reviser_dev_info *reviser_device = s->private;

	if (!reviser_is_power(reviser_device)) {
		LOG_ERR("Can Not Read when power disable\n");
		return 0;
	}
	reviser_print_exception(reviser_device, s);
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
int reviser_dbg_init(struct reviser_dev_info *reviser_device)
{
	int ret = 0;

	g_reviser_vlm_ctxid = 0;
	g_reviser_mem_tcm_bank = 0;
	g_reviser_mem_dram_bank = 0;
	g_reviser_mem_dram_ctxid = 0;

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
			reviser_dbg_hw, reviser_device,
			&reviser_dbg_fops_remap_table);

	ret = IS_ERR_OR_NULL(reviser_dbg_remap_table);
	if (ret) {
		LOG_ERR("failed to create debug node(remap_table).\n");
		goto out;
	}

	reviser_dbg_context_ID = debugfs_create_file("ctxID", 0644,
			reviser_dbg_hw, reviser_device,
			&reviser_dbg_fops_context_ID);

	ret = IS_ERR_OR_NULL(reviser_dbg_context_ID);
	if (ret) {
		LOG_ERR("failed to create debug node(context_ID).\n");
		goto out;
	}

	reviser_dbg_boundary = debugfs_create_file("boundary", 0644,
			reviser_dbg_hw, reviser_device,
			&reviser_dbg_fops_boundary);

	ret = IS_ERR_OR_NULL(reviser_dbg_boundary);
	if (ret) {
		LOG_ERR("failed to create debug node(boundary).\n");
		goto out;
	}

	reviser_dbg_iova = debugfs_create_file("iova", 0644,
			reviser_dbg_hw, reviser_device,
			&reviser_dbg_fops_iova);

	ret = IS_ERR_OR_NULL(reviser_dbg_iova);
	if (ret) {
		LOG_ERR("failed to create debug node(iova).\n");
		goto out;
	}

	reviser_dbg_table_vlm = debugfs_create_file("vlm", 0644,
			reviser_dbg_table, reviser_device,
			&reviser_dbg_fops_table_vlm);

	ret = IS_ERR_OR_NULL(reviser_dbg_table_vlm);
	if (ret) {
		LOG_ERR("failed to create debug node(table_vlm).\n");
		goto out;
	}

	reviser_dbg_table_vlm_ctxid = debugfs_create_u32("vlm_select", 0644,
			reviser_dbg_table,
			&g_reviser_vlm_ctxid);

	ret = IS_ERR_OR_NULL(reviser_dbg_table_vlm_ctxid);
	if (ret) {
		LOG_ERR("failed to create debug node(vlm_select).\n");
		goto out;
	}

	reviser_dbg_table_ctxid = debugfs_create_file("ctxid", 0644,
			reviser_dbg_table, reviser_device,
			&reviser_dbg_fops_table_ctxid);

	ret = IS_ERR_OR_NULL(reviser_dbg_table_ctxid);
	if (ret) {
		LOG_ERR("failed to create debug node(ctxid).\n");
		goto out;
	}

	reviser_dbg_table_tcm = debugfs_create_file("tcm", 0644,
			reviser_dbg_table, reviser_device,
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
			reviser_dbg_mem, reviser_device,
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

	reviser_dbg_mem_dram_ctxid = debugfs_create_u32("dram_ctxid", 0644,
			reviser_dbg_mem,
			&g_reviser_mem_dram_ctxid);

	ret = IS_ERR_OR_NULL(reviser_dbg_mem_dram_ctxid);
	if (ret) {
		LOG_ERR("failed to create debug node(dram_ctxid).\n");
		goto out;
	}

	reviser_dbg_mem_dram = debugfs_create_file("dram", 0644,
			reviser_dbg_mem, reviser_device,
			&reviser_dbg_fops_mem_dram);

	ret = IS_ERR_OR_NULL(reviser_dbg_mem_dram);
	if (ret) {
		LOG_ERR("failed to create debug node(dram).\n");
		goto out;
	}

	reviser_dbg_mem_swap = debugfs_create_file("swap", 0644,
			reviser_dbg_mem, reviser_device,
			&reviser_dbg_fops_mem_swap);

	ret = IS_ERR_OR_NULL(reviser_dbg_mem_swap);
	if (ret) {
		LOG_ERR("failed to create debug node(swap).\n");
		goto out;
	}

	/* create log level */
	reviser_dbg_log = debugfs_create_u8("log_level", 0644,
			reviser_dbg_root, &g_reviser_log_level);


	reviser_dbg_err_info = debugfs_create_file("info", 0644,
			reviser_dbg_err, reviser_device,
			&reviser_dbg_fops_err_info);

	ret = IS_ERR_OR_NULL(reviser_dbg_err_info);
	if (ret) {
		LOG_ERR("failed to create debug node(count).\n");
		goto out;
	}

	reviser_dbg_mem_vlm = debugfs_create_file("vlm", 0644,
			reviser_dbg_mem, reviser_device,
			&reviser_dbg_fops_mem_vlm);

	ret = IS_ERR_OR_NULL(reviser_dbg_mem_vlm);
	if (ret) {
		LOG_ERR("failed to create debug node(vlm).\n");
		goto out;
	}


	reviser_dbg_rw = debugfs_create_file("rw", 0644,
			reviser_dbg_root, reviser_device,
			&reviser_dbg_fops_rw);

	ret = IS_ERR_OR_NULL(reviser_dbg_rw);
	if (ret) {
		LOG_ERR("failed to create debug node(rw).\n");
		goto out;
	}

	reviser_dbg_err_reg = debugfs_create_file("reg", 0644,
			reviser_dbg_err, reviser_device,
			&reviser_dbg_fops_err_reg);

	ret = IS_ERR_OR_NULL(reviser_dbg_err_reg);
	if (ret) {
		LOG_ERR("failed to create debug node(reg).\n");
		goto out;
	}

out:
	return ret;
}

int reviser_dbg_destroy(struct reviser_dev_info *reviser_device)
{
	debugfs_remove_recursive(reviser_dbg_root);

	return 0;
}
