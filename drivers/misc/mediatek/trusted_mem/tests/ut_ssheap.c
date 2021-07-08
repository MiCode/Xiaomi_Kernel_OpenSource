// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#define pr_fmt(fmt) "[TMEM] ssheap ut: " fmt

#include <linux/types.h>
#include <linux/of_reserved_mem.h>
#include <linux/printk.h>
#include <linux/cma.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/sizes.h>
#include <linux/dma-direct.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/arm-smccc.h>
#include <linux/parser.h>

#include <public/trusted_mem_api.h>
#include "private/ssheap_priv.h"

enum {
	UT_OPT_ERR = 0,
	UT_OPT_CMD = 1 << 0,
	UT_OPT_SIZE = 1 << 1,
	UT_OPT_ALIGN = 1 << 2,
	UT_OPT_DUMP = 1 << 3,
};

static const match_table_t opt_tokens = { { UT_OPT_CMD, "cmd=%d" },
					  { UT_OPT_SIZE, "size=%x" },
					  { UT_OPT_ALIGN, "align=%x" },
					  { UT_OPT_DUMP, "dump=%d" },
					  { UT_OPT_ERR, NULL } };

static int ssheap_open(__always_unused struct inode *inode,
		       __always_unused struct file *file)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	return 0;
}

static int ssheap_release(__always_unused struct inode *ino,
			  __always_unused struct file *file)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	return 0;
}

static void dump_debug_info(void)
{
	int total_allocated_size;

	pr_info("%s: ssheap base=%pa, size=%pa\n", __func__, &ssheap_base,
		&ssheap_size);

	total_allocated_size = atomic64_read(&ssheap_total_allocated_size);
	pr_info("%s: ssheap_total_allocated_size: 0x%x\n", __func__,
		total_allocated_size);
	pr_info("%s: free_size: 0x%llx\n", __func__,
		ssheap_size - total_allocated_size);
}

static void dump_buf_info(struct ssheap_buf_info *info)
{
	int i;
	struct scatterlist *sg;
	uint64_t dma_addr;

	pr_info("%s: info:%p", __func__, info);
	pr_info("%s: table=%p", __func__, info->table);
	pr_info("%s: alignment: 0x%lx", __func__, info->alignment);
	pr_info("%s: req_size: 0x%lx", __func__, info->req_size);
	pr_info("%s: aligned_req_size: 0x%lx", __func__,
		info->aligned_req_size);
	pr_info("%s: allocated_size: 0x%lx", __func__, info->allocated_size);
	pr_info("%s: elemts: %ld", __func__, info->elems);

	if (info->table) {
		for_each_sg(info->table->sgl, sg, info->table->nents, i) {
			dma_addr = sg_dma_address(sg);
			pr_info("%s: sg idx:%d dma_addr=%llx size=%x\n",
				__func__, i, dma_addr, sg->length);
		}
	}
}

static void ssheap_ut(const char *buf)
{
	unsigned long attrs = 0;
	void *ptr = NULL;
	dma_addr_t handle;
	struct ssheap_buf_info *info;
	static struct ssheap_buf_info *last_info;
	int token;
	substring_t args[MAX_OPT_ARGS];
	uint32_t ut_cmd = 0;
	uint32_t ut_size = 0;
	uint32_t ut_align = 0;
	uint32_t ut_dump = 0;
	char *options, *p, *o;
	struct page *page = NULL;
	unsigned long smc_ret;

	options = o = kstrdup(buf, GFP_KERNEL);

	while ((p = strsep(&o, " \t\n")) != NULL) {
		if (!*p)
			continue;
		pr_info("args: %s\n", p);
		token = match_token(p, opt_tokens, args);
		switch (token) {
		case UT_OPT_CMD:
			if (match_int(args, &token))
				break;
			ut_cmd = token;
			break;
		case UT_OPT_SIZE:
			if (match_hex(args, &token))
				break;
			ut_size = token;
			break;
		case UT_OPT_ALIGN:
			if (match_hex(args, &token))
				break;
			ut_align = token;
			break;
		case UT_OPT_DUMP:
			if (match_hex(args, &token))
				break;
			ut_dump = token;
			break;
		default:
			break;
		}
	}

	pr_info("cmd=%d size=%#x align=%#x\n", ut_cmd, ut_size, ut_align);
	if (ut_cmd == 1) {
		info = ssheap_alloc_non_contig(ut_size, ut_align, 0xff);
		if (info == NULL) {
			pr_info("cmd=%d FAILED\n", ut_cmd);
		} else {
			smc_ret = mtee_assign_buffer(info, 0xff);
			pr_debug("secure buffer ret:%d (0x%x)\n", smc_ret,
				 smc_ret);
			smc_ret = mtee_unassign_buffer(info, 0xff);
			pr_debug("unsecure buffer ret:%d (0x%x)\n", smc_ret,
				 smc_ret);
			if (ut_dump)
				dump_buf_info(info);
			ssheap_free_non_contig(info);
			pr_info("cmd=%d PASSED\n", ut_cmd);
		}
	}
	if (ut_cmd == 11) {
		if (last_info) {
			pr_info("cmd=%d FAILED (last_info should be NULL)\n",
				ut_cmd);
			goto out;
		}
		last_info = ssheap_alloc_non_contig(ut_size, ut_align, 0xff);
		if (last_info == NULL) {
			pr_info("cmd=%d FAILED\n", ut_cmd);
		} else {
			smc_ret = mtee_assign_buffer(last_info, 0xff);
			pr_info("secure buffer ret:%d (0x%x)\n", smc_ret,
				smc_ret);
			if (ut_dump)
				dump_buf_info(last_info);
			pr_info("cmd=%d PASSED\n", ut_cmd);
		}
	}
	if (ut_cmd == 12) {
		if (last_info == NULL) {
			pr_info("last_info is NULL\n");
			pr_info("cmd=%d FAILED\n", ut_cmd);
			goto out;
		}
		smc_ret = mtee_unassign_buffer(last_info, 0xff);
		pr_info("unsecure buffer ret:%d (0x%x)\n", smc_ret, smc_ret);

		ssheap_free_non_contig(last_info);
		last_info = NULL;
		pr_info("cmd=%d PASSED\n", ut_cmd);
	}
	if (ut_cmd == 3) {
		attrs = DMA_ATTR_NO_KERNEL_MAPPING;
		ptr = dma_alloc_attrs(ssheap_dev, ut_size, &handle, GFP_KERNEL,
				      attrs);
		pr_info("ptr=%p dma_handle=%lx dma_phys=%llx\n", ptr,
			(unsigned long)handle, dma_to_phys(ssheap_dev, handle));
		if (ptr) {
			pr_info("free ptr=%p dma_handle=%lx\n", handle);
			dma_free_attrs(ssheap_dev, ut_size, ptr, handle, attrs);
			pr_info("cmd=%d PASSED\n", ut_cmd);
		} else {
			pr_info("cmd=%d FAILED\n", ut_cmd);
		}
	}
	if (ut_cmd == 4) {
		page = cma_alloc(ssheap_dev->cma_area, ut_size >> PAGE_SHIFT,
				 get_order(ut_align), GFP_KERNEL);
		if (page) {
			pr_info("allocated phys_addr=%lx\n",
				page_to_phys(page));
			cma_release(ssheap_dev->cma_area, page,
				    ut_size >> PAGE_SHIFT);
			pr_info("cmd=%d PASSED\n", ut_cmd);
		} else {
			pr_info("cmd=%d FAILED\n", ut_cmd);
		}
	}

out:
	if (ut_dump)
		dump_debug_info();

	kfree(options);
}

static ssize_t ssheap_write(struct file *file, const char __user *buffer,
			    size_t count, loff_t *data)
{
	char desc[128];

	if (copy_from_user(desc, buffer, count))
		return 0;
	pr_info("write count:%d\n", count);
	ssheap_ut(desc);
	return count;
}

static const struct proc_ops ssheap_fops = {
	.proc_open = ssheap_open,
	.proc_release = ssheap_release,
	.proc_ioctl = NULL,
	.proc_write = ssheap_write,
};

void create_ssheap_ut_device(void)
{
	proc_create("ssheap0", 0664, NULL, &ssheap_fops);
}
