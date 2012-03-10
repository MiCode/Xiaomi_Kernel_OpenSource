/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 * Pipe-Memory allocation/free management.
 */

#include <linux/types.h>	/* u32 */
#include <linux/kernel.h>	/* pr_info() */
#include <linux/io.h>		/* ioremap() */
#include <linux/mutex.h>	/* mutex */
#include <linux/list.h>		/* list_head */
#include <linux/genalloc.h>	/* gen_pool_alloc() */
#include <linux/errno.h>	/* ENOMEM */

#include "sps_bam.h"
#include "spsi.h"

static u32 iomem_phys;
static void *iomem_virt;
static u32 iomem_size;
static u32 iomem_offset;
static struct gen_pool *pool;
static u32 nid = 0xaa;

/* Debug */
static u32 total_alloc;
static u32 total_free;

/**
 * Translate physical to virtual address
 *
 */
void *spsi_get_mem_ptr(u32 phys_addr)
{
	void *virt = NULL;

	if ((phys_addr >= iomem_phys) &&
	    (phys_addr < (iomem_phys + iomem_size))) {
		virt = (u8 *) iomem_virt + (phys_addr - iomem_phys);
	} else {
		virt = phys_to_virt(phys_addr);
		SPS_ERR("sps:spsi_get_mem_ptr.invalid phys addr=0x%x.",
			phys_addr);
	}
	return virt;
}

/**
 * Allocate I/O (pipe) memory
 *
 */
u32 sps_mem_alloc_io(u32 bytes)
{
	u32 phys_addr = SPS_ADDR_INVALID;
	u32 virt_addr = 0;

	virt_addr = gen_pool_alloc(pool, bytes);
	if (virt_addr) {
		iomem_offset = virt_addr - (u32) iomem_virt;
		phys_addr = iomem_phys + iomem_offset;
		total_alloc += bytes;
	} else {
		SPS_ERR("sps:gen_pool_alloc %d bytes fail.", bytes);
		return SPS_ADDR_INVALID;
	}

	SPS_DBG2("sps:sps_mem_alloc_io.phys=0x%x.virt=0x%x.size=0x%x.",
		phys_addr, virt_addr, bytes);

	return phys_addr;
}

/**
 * Free I/O memory
 *
 */
void sps_mem_free_io(u32 phys_addr, u32 bytes)
{
	u32 virt_addr = 0;

	iomem_offset = phys_addr - iomem_phys;
	virt_addr = (u32) iomem_virt + iomem_offset;

	SPS_DBG2("sps:sps_mem_free_io.phys=0x%x.virt=0x%x.size=0x%x.",
		phys_addr, virt_addr, bytes);

	gen_pool_free(pool, virt_addr, bytes);
	total_free += bytes;
}

/**
 * Initialize driver memory module
 *
 */
int sps_mem_init(u32 pipemem_phys_base, u32 pipemem_size)
{
#ifndef CONFIG_SPS_SUPPORT_NDP_BAM
	int res;
#endif
	/* 2^8=128. The desc-fifo and data-fifo minimal allocation. */
	int min_alloc_order = 8;

#ifndef CONFIG_SPS_SUPPORT_NDP_BAM
	iomem_phys = pipemem_phys_base;
	iomem_size = pipemem_size;

	if (iomem_phys == 0) {
		SPS_ERR("sps:Invalid Pipe-Mem address");
		return SPS_ERROR;
	} else {
		iomem_virt = ioremap(iomem_phys, iomem_size);
		if (!iomem_virt) {
			SPS_ERR("sps:Failed to IO map pipe memory.\n");
			return -ENOMEM;
		}
	}

	iomem_offset = 0;
	SPS_DBG("sps:sps_mem_init.iomem_phys=0x%x,iomem_virt=0x%x.",
		iomem_phys, (u32) iomem_virt);
#endif

	pool = gen_pool_create(min_alloc_order, nid);

	if (!pool) {
		SPS_ERR("sps:Failed to create a new memory pool.\n");
		return -ENOMEM;
	}

#ifndef CONFIG_SPS_SUPPORT_NDP_BAM
	res = gen_pool_add(pool, (u32) iomem_virt, iomem_size, nid);
	if (res)
		return res;
#endif

	return 0;
}

/**
 * De-initialize driver memory module
 *
 */
int sps_mem_de_init(void)
{
	if (iomem_virt != NULL) {
		gen_pool_destroy(pool);
		pool = NULL;
		iounmap(iomem_virt);
		iomem_virt = NULL;
	}

	if (total_alloc == total_free)
		return 0;
	else {
		SPS_ERR("sps:sps_mem_de_init:some memory not free");
		return SPS_ERROR;
	}
}
