/*
 * Copyright (C) 2015 MediaTek Inc.
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
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <mach/pseudo_m4u.h>
#include "pseudo_m4u_debug.h"
#include "pseudo_m4u_log.h"
#include "mtk_iommu_ext.h"

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif

#if defined(CONFIG_MTK_LEGACY_SECMEM_SUPPORT)
#include "secmem.h"
#elif defined(CONFIG_MTK_SECURE_MEM_SUPPORT)
#include "trusted_mem_api.h"
#endif

#if IS_ENABLED(CONFIG_DEBUG_FS) || IS_ENABLED(CONFIG_PROC_FS)
#define M4U_MAGIC_KEY	(0x14045a)
#define M4U_CMD_SHIFT	(8)
#define M4U_CMD_MASK	(0xff)

int test_mva;

unsigned int gM4U_seed_mva;

int pseudo_test_alloc_dealloc(int port, unsigned long va,
		unsigned int size, struct sg_table *sg_table)
{
	struct m4u_client_t *client = NULL;
	unsigned long mva = 0;
	int ret = 0;

	M4U_INFO("test va=0x%lx,size=0x%x\n", va, size);

	client = pseudo_get_m4u_client();
	if (IS_ERR_OR_NULL(client)) {
		M4U_MSG("create client fail!\n");
		return -1;
	}

	ret = __pseudo_alloc_mva(client, port, va,
			    size, sg_table, 0, &mva);
	if (ret) {
		M4U_MSG("alloc mva fail:va=0x%lx,size=0x%x,ret=%d\n",
			va, size, ret);
		pseudo_put_m4u_client();
		return ret;
	}
	__m4u_dump_pgtable(NULL, 1, true, 0);
	ret = m4u_switch_acp(port, mva, size, 1);
	if (!ret) {
		__m4u_dump_pgtable(NULL, 1, true, 0);
		ret = m4u_switch_acp(port, mva, size, 0);
		if (!ret)
			__m4u_dump_pgtable(NULL, 1, true, 0);
	}
	ret = pseudo_dealloc_mva(client, port, mva);
	__m4u_dump_pgtable(NULL, 1, true, 0);
	pseudo_put_m4u_client();

	return 0;
}

int m4u_test_alloc_dealloc(int id, unsigned int size)
{
	unsigned long va = 0;
	int ret = 0;
	unsigned long populate = 0;

	if (id == 1) {
		va = (unsigned long)kmalloc(size, GFP_KERNEL);
		if (!va) {
			M4U_MSG("kmalloc failed!\n");
			return -1;
		}
	} else if (id == 2) {
		va = (unsigned long)vmalloc(size);
		if (!va) {
			M4U_MSG("vmalloc failed!\n");
			return -1;
		}
	} else if (id == 3) {
		down_write(&current->mm->mmap_sem);
		va = do_mmap_pgoff(NULL, 0, size,
			PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED,
			0, &populate, NULL);
		if (!va) {
			M4U_MSG("mmap pgoff failed!\n");
			return -1;
		}
		up_write(&current->mm->mmap_sem);
	}

	pseudo_test_alloc_dealloc(id, va, size, NULL);

	if (id == 1)
		kfree((void *)va);
	else if (id == 2)
		vfree((void *)va);
	else if (id == 3) {
		down_read(&current->mm->mmap_sem);
		ret = do_munmap(current->mm, va, size, NULL);
		up_read(&current->mm->mmap_sem);
		if (ret)
			M4U_MSG("do_munmap failed\n");
	}

	return 0;
}

static int m4u_test_map_kernel(void)
{
	struct m4u_client_t *client = NULL;
	unsigned long va = 0;
	unsigned int size = 1024 * 1024;
	unsigned long mva = 0;
	unsigned long kernel_va = 0;
	unsigned long kernel_size = 0;
	int i = 0;
	int ret = 0;
	unsigned long populate = 0;

	down_write(&current->mm->mmap_sem);
	va = do_mmap_pgoff(NULL, 0, size,
		PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED,
		0, &populate, NULL);
	up_write(&current->mm->mmap_sem);

	M4U_INFO("test va=0x%lx,size=0x%x\n", va, size);

	for (i = 0; i < size; i += 4)
		*(int *)(va + i) = i;

	client = pseudo_get_m4u_client();
	if (IS_ERR_OR_NULL(client)) {
		M4U_MSG("create client fail!\n");
		return -1;
	}

	ret = __pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG,
		va, size, NULL, 0, &mva);
	if (ret) {
		M4U_MSG("alloc using kmalloc fail:va=0x%lx,size=0x%x\n",
			va, size);
		pseudo_put_m4u_client();
		return -1;
	}

	ret = m4u_mva_map_kernel(mva, size, &kernel_va, &kernel_size, NULL);
	if (ret) {
		M4U_MSG("map kernel fail!\n");
		pseudo_put_m4u_client();
		return -1;
	}
	for (i = 0; i < size; i += 4) {
		if (*(int *)(kernel_va + i) != i) {
			M4U_MSG("wawawa, get map value fail! i=%d, map=%d\n", i,
			       *(int *)(kernel_va + i));
		}
	}

	ret = m4u_mva_unmap_kernel(mva, size, kernel_va);

	ret = pseudo_dealloc_mva(client, M4U_PORT_OVL_DEBUG, mva);
	down_read(&current->mm->mmap_sem);
	ret = do_munmap(current->mm, va, size, NULL);
	up_read(&current->mm->mmap_sem);
	if (ret)
		M4U_MSG("do_munmap failed\n");
	pseudo_put_m4u_client();

	return 0;
}

int m4u_test_fake_engine(void)
{
	unsigned char *pSrc = NULL;
	unsigned char *pDst = NULL;
	unsigned long mva_rd = 0;
	unsigned long mva_wr = 0;
	unsigned int allocated_size = 1024;
	unsigned int i = 0;
	struct m4u_client_t *client = pseudo_get_m4u_client();

	if (IS_ERR_OR_NULL(client)) {
		M4U_MSG("create client fail!\n");
		return -1;
	}
	iommu_perf_monitor_start(0);

	pSrc = vmalloc(allocated_size);
	if (!pSrc) {
		M4U_MSG("vmalloc failed!\n");
		pseudo_put_m4u_client();
		return -1;
	}
	memset(pSrc, 0xFF, allocated_size);
	M4U_MSG("(0) vmalloc pSrc:0x%p\n", pSrc);

	pDst = vmalloc(allocated_size);
	if (!pDst) {
		M4U_MSG("vmalloc failed!\n");
		vfree(pSrc);
		pseudo_put_m4u_client();
		return -1;
	}
	memset(pDst, 0xFF, allocated_size);
	M4U_MSG("(0) vmalloc pDst:0x%p\n", pDst);

	M4U_MSG("(1) pDst check 0x%x 0x%x 0x%x 0x%x  0x%x\n",
		*pDst, *(pDst+1),
		*(pDst+126), *(pDst+127), *(pDst+128));

	__pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG,
		(unsigned long)*pSrc,
		allocated_size, NULL, 0, &mva_rd);
	__pseudo_alloc_mva(client,  M4U_PORT_OVL_DEBUG,
		(unsigned long)*pDst,
		allocated_size, NULL, 0, &mva_wr);

	__m4u_dump_pgtable(NULL, 1, true, 0);

	m4u_display_fake_engine_test(mva_rd, mva_wr);

	M4U_MSG("(2) mva_wr:0x%lx\n", mva_wr);

	pseudo_dealloc_mva(client, M4U_PORT_OVL_DEBUG, mva_rd);
	pseudo_dealloc_mva(client, M4U_PORT_OVL_DEBUG, mva_wr);

	M4U_MSG("(3) pDst check 0x%x 0x%x 0x%x 0x%x  0x%x\n",
		*pDst, *(pDst+1), *(pDst+126), *(pDst+127),
		*(pDst+128));

	for (i = 0; i < 128; i++) {
		if (*(pDst+i) != 0) {
			M4U_MSG("(4) [Error] pDst check fail VA\n");
			M4U_MSG("0x%p: 0x%x\n",
				pDst+i*sizeof(unsigned char),
				*(pDst+i));
			vfree(pSrc);
			vfree(pDst);
			pseudo_put_m4u_client();
			return -2;
		}
	}
	if (i == 128)
		M4U_MSG("(4) m4u_disp_fake_test R/W 128 bytes PASS\n ");

	vfree(pSrc);
	vfree(pDst);

	iommu_perf_monitor_stop(0);
	pseudo_put_m4u_client();
	return 0;
}

int m4u_test_ddp(void)
{
	unsigned long *pSrc = NULL;
	unsigned long *pDst = NULL;
	unsigned long src_pa = 0;
	unsigned long dst_pa = 0;
	unsigned long size = 64 * 64 * 3;
	struct M4U_PORT_STRUCT port;
	struct m4u_client_t *client = pseudo_get_m4u_client();

	if (IS_ERR_OR_NULL(client)) {
		M4U_MSG("create client fail!\n");
		return -1;
	}
	pSrc = vmalloc(size);
	if (!pSrc) {
		M4U_MSG("vmalloc failed!\n");
		pseudo_put_m4u_client();
		return -1;
	}

	pDst = vmalloc(size);
	if (!pDst) {
		M4U_MSG("vmalloc failed!\n");
		vfree(pSrc);
		pseudo_put_m4u_client();
		return -1;
	}
	memset(pSrc, 0xFF, size);
	memset(pDst, 0xFF, size);

	__pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG, *pSrc,
		      size, NULL, 0, &src_pa);

	__pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG, *pDst,
		      size, NULL, 0, &dst_pa);

	M4U_INFO("pSrc=0x%p, pDst=0x%p, src_pa=0x%lx, dst_pa=0x%lx\n",
		pSrc, pDst, src_pa, dst_pa);

	port.ePortID = M4U_PORT_OVL_DEBUG;
	port.Direction = 0;
	port.Distance = 1;
	port.domain = 3;
	port.Security = 0;
	port.Virtuality = 1;
	m4u_config_port(&port);

	port.ePortID = M4U_PORT_MDP_DEBUG;
	m4u_config_port(&port);

	iommu_perf_monitor_start(0);
	__ddp_mem_test(pSrc, src_pa, pDst, dst_pa, 1);
	iommu_perf_monitor_stop(0);

	vfree(pSrc);
	vfree(pDst);
	pseudo_put_m4u_client();

	return 0;
}

enum mtk_iommu_callback_ret_t test_fault_callback(int port,
		unsigned long mva, void *data)
{
	if (data != NULL)
		M4U_MSG("fault call port=%d, mva=0x%lx, data=0x%x\n",
			port, mva, *(int *)data);
	else
		M4U_MSG("fault call port=%d, mva=0x%lx\n", port, mva);

	/* DO NOT print too much logs here !!!! */
	/* Do NOT use any lock hear !!!! */
	/* DO NOT do any other things except print !!! */
	/* DO NOT make any mistake here (or reboot will happen) !!! */
	return MTK_IOMMU_CALLBACK_HANDLED;
}

int m4u_test_tf(void)
{
	unsigned long *pSrc = NULL;
	unsigned long *pDst = NULL;
	unsigned long src_pa = 0;
	unsigned long dst_pa = 0;
	unsigned long size = 64 * 64 * 3;
	struct M4U_PORT_STRUCT port;
	struct m4u_client_t *client = pseudo_get_m4u_client();
	int data = 88;

	if (IS_ERR_OR_NULL(client)) {
		M4U_MSG("create client fail!\n");
		return -1;
	}
	mtk_iommu_register_fault_callback(M4U_PORT_OVL_DEBUG,
		test_fault_callback, &data);
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_DEBUG,
		test_fault_callback, &data);

	pSrc = vmalloc(size);
	if (!pSrc) {
		M4U_MSG("vmalloc failed!\n");
		pseudo_put_m4u_client();
		return -1;
	}

	pDst = vmalloc(size);
	if (!pDst) {
		M4U_MSG("vmalloc failed!\n");
		vfree(pSrc);
		pseudo_put_m4u_client();
		return -1;
	}
	memset(pSrc, 0xFF, size);
	memset(pDst, 0xFF, size);

	__pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG, *pSrc,
		      size, NULL, 0, &src_pa);

	__pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG, *pDst,
		      size / 2, NULL, 0, &dst_pa);

	M4U_INFO("pSrc=0x%p, pDst=0x%p, src_pa=0x%lx, dst_pa=0x%lx\n",
		pSrc, pDst, src_pa, dst_pa);

	port.ePortID = M4U_PORT_OVL_DEBUG;
	port.Direction = 0;
	port.Distance = 1;
	port.domain = 3;
	port.Security = 0;
	port.Virtuality = 1;
	m4u_config_port(&port);

	port.ePortID = M4U_PORT_MDP_DEBUG;
	m4u_config_port(&port);

	iommu_perf_monitor_start(0);
	__ddp_mem_test(pSrc, src_pa, pDst, dst_pa, 0);
	iommu_perf_monitor_stop(0);

	pseudo_dealloc_mva(client, M4U_PORT_OVL_DEBUG, src_pa);
	pseudo_dealloc_mva(client, M4U_PORT_OVL_DEBUG, dst_pa);

	vfree(pSrc);
	vfree(pDst);
	pseudo_put_m4u_client();

	return 0;
}

#if 1
#include <mtk/mtk_ion.h>
#include <ion_priv.h>

void m4u_test_ion(void)
{
	unsigned long *pSrc = NULL;
	unsigned long *pDst = NULL;
	unsigned int size = 64 * 64 * 3;
	struct ion_mm_data mm_data[2];
	struct ion_client *ion_client = NULL;
	struct ion_handle *src_handle = NULL, *dst_handle = NULL;

	/* FIX-ME: modified for linux-3.10 early porting */
	/* ion_client = ion_client_create(g_ion_device, 0xffffffff, "test"); */
	ion_client = ion_client_create(g_ion_device, "m4u_test_ion");

	src_handle = ion_alloc(ion_client,
		size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);
	dst_handle = ion_alloc(ion_client,
		size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);

	pSrc = ion_map_kernel(ion_client, src_handle);
	if (!pSrc) {
		M4U_MSG("ion map kernel failed!\n");
		goto out1;
	}
	pDst = ion_map_kernel(ion_client, dst_handle);
	if (!pDst) {
		M4U_MSG("vmalloc failed!\n");
		goto out;
	}

	mm_data[0].get_phys_param.kernel_handle = src_handle;
	mm_data[0].get_phys_param.module_id = M4U_PORT_OVL_DEBUG;
	mm_data[0].get_phys_param.security = 0;
	mm_data[0].get_phys_param.coherent = 0;
	mm_data[0].mm_cmd = ION_MM_GET_IOVA;
	if (ion_kernel_ioctl(ion_client,
		ION_CMD_MULTIMEDIA, (unsigned long)&mm_data[0]) < 0)
		M4U_MSG("ion_test_drv0: GET_IOVA failed.\n");

	mm_data[1].get_phys_param.kernel_handle = dst_handle;
	mm_data[1].get_phys_param.module_id = M4U_PORT_OVL_DEBUG;
	mm_data[1].get_phys_param.security = 0;
	mm_data[1].get_phys_param.coherent = 0;
	mm_data[1].mm_cmd = ION_MM_GET_IOVA;
	if (ion_kernel_ioctl(ion_client,
		ION_CMD_MULTIMEDIA, (unsigned long)&mm_data[1]) < 0)
		M4U_MSG("ion_test_drv1: GET_IOVA failed.\n");

	M4U_MSG(
		"ion alloced: pSrc=0x%p, pDst=0x%p, src_pa=0x%llx, dst_pa=0x%llx\n",
		pSrc, pDst,
		mm_data[0].get_phys_param.phy_addr,
		mm_data[1].get_phys_param.phy_addr);

	ion_unmap_kernel(ion_client, dst_handle);
out:
	ion_unmap_kernel(ion_client, src_handle);
out1:
	ion_free(ion_client, src_handle);
	ion_free(ion_client, dst_handle);

	ion_client_destroy(ion_client);
}
#else
#define m4u_test_ion(...)
#endif
#if 0
static int dma_buf_test_alloc_dealloc(int port, struct sg_table *table,
	enum dma_data_direction dir)
{
	struct device *dev = pseudo_get_larbdev(port);
	int ret = 0;

	if (!dev) {
		pr_notice("%s, %d, invalid port:%d\n",
			  __func__, __LINE__, port);
		return -1;
	}
	ret = dma_map_sg_attrs(dev, table->sgl,
			table->nents,
			DMA_BIDIRECTIONAL,
			DMA_ATTR_SKIP_CPU_SYNC);

	if (!ret) {
		pr_notice("%s, %d, failed at mapping sg table, ret:%d\n",
			  __func__, __LINE__, ret);
		return -1;
	}
	__m4u_dump_pgtable(NULL, 1, true, 0);

	dma_unmap_sg_attrs(dev, table->sgl,
			table->orig_nents, dir,
			DMA_ATTR_SKIP_CPU_SYNC);
	__m4u_dump_pgtable(NULL, 1, true, 0);
	return 0;
}
#endif

static int m4u_debug_set(void *data, u64 val)
{
	u64 value;

	if ((val >> M4U_CMD_SHIFT) == M4U_MAGIC_KEY) {
		value = val & M4U_CMD_MASK;
		M4U_MSG("%s:val=%llu\n", __func__, value);
	} else {
		M4U_MSG("%s fail:val=0x%llx\n", __func__, val);
		return 0;
	}
	switch (value) {
	case 1:
	{ /* map4kpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i = 0;
		struct page *page = NULL;
		int page_num = 512;
		unsigned int id = M4U_PORT_BOUNDARY0_DEBUG;
		unsigned long size = page_num * PAGE_SIZE;

		page = alloc_pages(GFP_KERNEL, get_order(page_num));
		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i)
			sg_set_page(sg, page + i, PAGE_SIZE, 0);

		pseudo_test_alloc_dealloc(id, 0, size, sg_table);

		sg_free_table(sg_table);
		__free_pages(page, get_order(page_num));
	}
	break;
	case 2:
	{ /* map64kpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i = 0;
		int page_num = 51;
		unsigned long page_size = SZ_64K;
		unsigned int id = M4U_PORT_BOUNDARY1_DEBUG;
		unsigned long size = page_num * page_size;
		struct page *page = NULL;

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
			page = phys_to_page(page_size * (i + 1));
			sg_set_page(sg, page, page_size, 0);
		}

		pseudo_test_alloc_dealloc(id, 0, size, sg_table);
		sg_free_table(sg_table);
	}
	break;
	case 3:
	{ /* map1Mpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i = 0;
		int page_num = 37;
		unsigned long page_size = SZ_1M;
		unsigned int id = M4U_PORT_BOUNDARY2_DEBUG;
		unsigned long size = page_num * page_size;
		struct page *page = NULL;

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);

		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
			page = phys_to_page(page_size * (i + 1));
			sg_set_page(sg, page, page_size, 0);
		}
		pseudo_test_alloc_dealloc(id, 0, size, sg_table);

		sg_free_table(sg_table);
	}
	break;
	case 4:
	{ /* map16Mpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i = 0;
		int page_num = 2;
		unsigned long page_size = SZ_16M;
		unsigned int id = M4U_PORT_BOUNDARY3_DEBUG;
		unsigned long size = page_num * page_size;
		struct page *page = NULL;

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
			page = phys_to_page(page_size * (i + 1));
			sg_set_page(sg, page, page_size, 0);
		}
		pseudo_test_alloc_dealloc(id, 0, size, sg_table);
		sg_free_table(sg_table);
	}
	break;
	case 5:
	{ /* mapmiscpages */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		unsigned int size = SZ_16M * 2;
		unsigned int id = M4U_PORT_UNKNOWN;
		struct page *page = NULL;

		sg_alloc_table(sg_table, 1, GFP_KERNEL);
		sg = sg_table->sgl;
		sg_dma_address(sg) = 0x4000;
		sg_dma_len(sg) = size;
		page = phys_to_page(0x4000);
		sg_set_page(sg, page, size, 0);

		pseudo_test_alloc_dealloc(id, 0, size, sg_table);
		sg_free_table(sg_table);
	}
	break;
	case 6:
		m4u_test_alloc_dealloc(1, SZ_4M);
	break;
	case 7:
		m4u_test_alloc_dealloc(2, SZ_4M);
	break;
	case 8:
		m4u_test_alloc_dealloc(3, SZ_4M);
	break;
	case 9:                /* m4u_alloc_mvausingkmallocbuffer */
	{ /* map4kpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i = 0;
		struct page *page = NULL;
		int page_num = 512;
		unsigned int id = M4U_PORT_CCU;
		unsigned long size = page_num * PAGE_SIZE;

		page = alloc_pages(GFP_KERNEL, get_order(page_num));
		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i)
			sg_set_page(sg, page + i, PAGE_SIZE, 0);

		pseudo_test_alloc_dealloc(id, 0, size, sg_table);

		sg_free_table(sg_table);
		__free_pages(page, get_order(page_num));
	}
	break;
	case 10:
	{ /* map4kpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i = 0;
		struct page *page = NULL;
		int page_num = 512;
		unsigned int id = M4U_PORT_APU_CODE;
		unsigned long size = page_num * PAGE_SIZE;

		page = alloc_pages(GFP_KERNEL, get_order(page_num));
		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i)
			sg_set_page(sg, page + i, PAGE_SIZE, 0);

		pseudo_test_alloc_dealloc(id, 0, size, sg_table);

		sg_free_table(sg_table);
		__free_pages(page, get_order(page_num));
	}
	break;
	case 11:    /* map unmap kernel */
		m4u_test_map_kernel();
		break;
	case 12:
		/*ddp_mem_test();*/
		break;
	case 13:
	    m4u_test_ddp();
		break;
	case 14:
		m4u_test_tf();
		break;
	case 15:
	    m4u_test_ion();
		break;
	case 16:
	{
		int i, j;

		for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++)
			for (j = 0; j < MTK_IOMMU_MMU_COUNT; j++)
				mtk_dump_main_tlb(i, j, NULL);
	}
	break;
	case 17:
	{
		int i;

		for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++)
			mtk_dump_pfh_tlb(i, NULL);
	}
	break;
	case 18:
	{
		int i, j;
		struct mau_config_info mau_cfg = {
			.start = 0x1000,
			.end = 0xffffffff,
			.port_mask = 0xffffffff,
			.larb_mask = 0xffffffff,
			.wr = 0x1,
			.virt = 0x1,
			.io = 0x0,
			.start_bit32 = 0x0,
			.end_bit32 = 0x0
			};

		for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++) {
			for (j = 0; j < MTK_IOMMU_MMU_COUNT; j++)
				mau_start_monitor(i, j, 0, &mau_cfg);
		}
	}
	break;
	case 19:
	{
		int i, j;

		for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++)
			for (j = 0; j < MTK_IOMMU_MMU_COUNT; j++)
				mau_stop_monitor(i, j, 0, false);
	}
	break;
	case 20:
	{
		struct M4U_PORT_STRUCT rM4uPort;
		int i;

		rM4uPort.Virtuality = 1;
		rM4uPort.Security = 0;
		rM4uPort.Distance = 1;
		rM4uPort.Direction = 0;
		rM4uPort.domain = 3;
		for (i = 0; i < M4U_PORT_UNKNOWN; i++) {
			rM4uPort.ePortID = i;
			m4u_config_port(&rM4uPort);
		}
	}
	break;
	case 21:
	{
		struct M4U_PORT_STRUCT rM4uPort;
		int i;

		rM4uPort.Virtuality = 0;
		rM4uPort.Security = 0;
		rM4uPort.Distance = 1;
		rM4uPort.Direction = 0;
		rM4uPort.domain = 3;
		for (i = 0; i < M4U_PORT_UNKNOWN; i++) {
			rM4uPort.ePortID = i;
			m4u_config_port(&rM4uPort);
		}
	}
	break;
	case 22:
	{
		unsigned int err_port = 0, err_size = 0;

		__m4u_dump_pgtable(NULL, 1, true, 0);

#if (CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		pr_notice(" >> boundary 0 top 5 user\n");
		m4u_find_max_port_size(0, (1UL << 32) - 1,
					&err_port, &err_size);
		report_custom_iommu_leakage(
					    iommu_get_port_name(err_port),
					    err_size);

		pr_notice(" >> boundary 1 top 5 user\n");
		m4u_find_max_port_size(1UL << 32, (2UL << 32) - 1,
					&err_port, &err_size);
		report_custom_iommu_leakage(
					    iommu_get_port_name(err_port),
					    err_size);

		pr_notice(" >> boundary 2 top 5 user\n");
		m4u_find_max_port_size(2UL << 32, (3UL << 32) - 1,
					&err_port, &err_size);
		report_custom_iommu_leakage(
					    iommu_get_port_name(err_port),
					    err_size);

		pr_notice(" >> boundary 3 top 5 user\n");
		m4u_find_max_port_size(3UL << 32, (4UL << 32) - 1,
					&err_port, &err_size);
		report_custom_iommu_leakage(
					    iommu_get_port_name(err_port),
					    err_size);

		pr_notice(" >> CCU domain top 5 user\n");
		m4u_find_max_port_size(0x240000000UL,
					0x248000000UL - 1,
					&err_port, &err_size);
		report_custom_iommu_leakage(
					    iommu_get_port_name(err_port),
					    err_size);
#else
		pr_notice(" >> boundary 0 top 5 user\n");
		m4u_find_max_port_size(0, (1UL << 32) - 1,
					&err_port, &err_size);
		report_custom_iommu_leakage(
					    iommu_get_port_name(err_port),
					    err_size);

		pr_notice(" >> CCU domain top 5 user\n");
		m4u_find_max_port_size(0x40000000UL,
					0x48000000UL - 1,
					&err_port, &err_size);
		report_custom_iommu_leakage(
					    iommu_get_port_name(err_port),
					    err_size);
#endif
	}
	break;
	case 23:
	{
		unsigned long pgd_pa = 0;

		if (mtk_iommu_get_pgtable_base_addr(&pgd_pa)) {
			M4U_MSG("failed to get pgd info\n");
			break;
		}
		M4U_MSG("pgd_pa:0x%lx\n", pgd_pa);
	}
	break;
	case 24:
	{
		unsigned int *pSrc = NULL;
		unsigned long mva = 0;
		unsigned long pa = 0;
		struct m4u_client_t *client = pseudo_get_m4u_client();
		struct device *dev = pseudo_get_larbdev(M4U_PORT_UNKNOWN);

		if (IS_ERR_OR_NULL(client)) {
			M4U_MSG("create client fail!\n");
			return -1;
		}
		pSrc = vmalloc(128);
		if (!pSrc) {
			M4U_MSG("vmalloc failed!\n");
			pseudo_put_m4u_client();
			return -1;
		}
		memset(pSrc, 0xFF, 128);

		__pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG,
			(unsigned long)*pSrc, 128, NULL, 0, &mva);

		__m4u_dump_pgtable(NULL, 1, true, 0);

		mtk_iommu_iova_to_pa(dev, mva, &pa);
		M4U_MSG("(1) mva:0x%lx pa:0x%lx\n", mva, pa);
			pseudo_dealloc_mva(client, M4U_PORT_OVL_DEBUG, mva);
		mtk_iommu_iova_to_pa(dev, mva, &pa);
		M4U_MSG("(2) mva:0x%lx pa:0x%lx\n", mva, pa);
	}
	break;
	case 25:
	{
		int i;

		for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++)
			iommu_perf_monitor_start(i);
	}
	break;
	case 26:
	{
		int i;

		for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++)
			iommu_perf_monitor_stop(i);
	}
	break;
	case 27:
	{
		mtk_dump_reg_for_hang_issue(0);
		mtk_dump_reg_for_hang_issue(1);
	}
	break;
	case 28:
	{
		m4u_test_fake_engine();
	}
	break;
	case 29:
	{
		mtk_iommu_switch_tf_test(true, __func__);
	}
	break;
	case 30:
	{
		mtk_iommu_switch_tf_test(false, __func__);
	}
	break;
	case 31:
	{
		int i;

		for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++)
			mtk_iommu_power_switch_by_id(i, true, "pseudo_debug");
		for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++)
			mtk_iommu_power_switch_by_id(i, false, "pseudo_debug");
	}
	break;
	case 33:
	{
		int i, j;

		for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++)
			for (j = 0; j < MTK_IOMMU_MMU_COUNT; j++)
				mau_stop_monitor(i, j, 0, true);
	}
	break;
	case 34:
	{
		mtk_iommu_tlb_flush_all(NULL);
	}
	break;
#if 0
	case 35:
	{
		pseudo_m4u_bank_irq_debug(2);
	}
	break;
	case 36:
	{
		pseudo_m4u_bank_irq_debug(0);
	}
	break;
#endif
	case 37: /* dump mau register */
	{
		int i, j, ret;

		for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++) {
			ret = mtk_iommu_power_switch_by_id(i, true, "dump_mau");
			if (ret) {
				pr_notice("%s, m4u%d open clk failed\n",
					      __func__, i);
				continue;
			}
			ret = mtk_switch_secure_debug_func(i, 1);
			if (ret) {
				pr_notice("%s, m4u%d failed to enable secure debug signal\n",
					      __func__, i);
				mtk_iommu_power_switch_by_id(i,
						false, "dump_mau");
				continue;
			}
			for (j = 0; j < MTK_IOMMU_MMU_COUNT; j++) {
				struct mau_config_info cfg;

				cfg.m4u_id = i;
				cfg.slave = j;
				cfg.mau = 0;
				mau_get_config_info(&cfg);
				pr_notice("%s, m4u:%d,slave:%d,mau:%d,s:0x%x(0x%x),e:0x%x(0x%x),io:0x%x,wr:0x%x,virt:0x%x,larb:0x%x,port:0x%x\n",
					__func__,
					i, j, cfg.mau,
					cfg.start, cfg.start_bit32,
					cfg.end, cfg.end_bit32,
					cfg.io, cfg.wr, cfg.virt,
					cfg.larb_mask, cfg.port_mask);
			}
			ret = mtk_iommu_power_switch_by_id(i,
						false, "dump_mau");
			if (ret) {
				pr_notice("%s, m4u%d close clk failed\n",
					      __func__, i);
			}
			ret = mtk_switch_secure_debug_func(i, 0);
			if (ret)
				pr_notice("%s, m4u%d failed to disable secure debug signal\n",
					      __func__, i);
		}
	}
	break;
#ifdef M4U_TEE_SERVICE_ENABLE
	case 50:
	{
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) &&   \
		defined(CONFIG_MTK_TEE_GP_SUPPORT)
		u32 sec_handle = 0;
		u32 refcount;

		secmem_api_alloc(0, 0x1000, &refcount,
			&sec_handle, "m4u_ut", 0);
#elif defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT)
		u32 sec_handle = 0;
		u32 refcount = 0;
		int ret = 0;

		ret = KREE_AllocSecurechunkmemWithTag(0,
			&sec_handle, 0, 0x1000, "m4u_ut");
		if (ret != TZ_RESULT_SUCCESS) {
			IONMSG(
				"KREE_AllocSecurechunkmemWithTag failed, ret is 0x%x\n",
					ret);
			return ret;
		}
#endif
		m4u_sec_init();
	}
	break;
#endif
	case 51:/* must open iommu clk first */
	{
		int i, ret = 0;

		for (i = 0; i < SMI_LARB_NR; i++) {
			ret = larb_clock_on(i, 1);
			if (ret < 0) {
				M4U_MSG("enable larb%d fail, ret:%d\n", i, ret);
				return ret;
			}
		}

		//IOMMU_ATF_DUMP_SECURE_PORT_CONFIG
		for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++)
			mtk_iommu_atf_test(i, 9);

		for (i = 0; i < SMI_LARB_NR; i++)
			larb_clock_off(i, 1);
	}
	break;
#ifdef M4U_GZ_SERVICE_ENABLE
	case 52:
	{
		int ret = 0;

		ret = m4u_gz_sec_init(0);
		pr_notice("%s, %d, genie zone secure init, ret:%d\n",
			  __func__, __LINE__, ret);
	}
	break;
#endif
	default:
		M4U_MSG("%s error,val=%llu\n", __func__, val);
	}

	return 0;
}

static int m4u_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_fops,
	m4u_debug_get,
	m4u_debug_set,
	"%llu\n");
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
DEFINE_PROC_ATTRIBUTE(m4u_proc_fops,
	m4u_debug_get,
	m4u_debug_set,
	"%llu\n");
#endif

static int m4u_log_level_set(void *data, u64 val)
{
	m4u_log_to_uart = (val & 0xf0) >> 4;
	m4u_log_level = val & 0xf;
	M4U_MSG("m4u_log_level: %d, m4u_log_to_uart:%d\n",
		m4u_log_level, m4u_log_to_uart);

	return 0;
}

static int m4u_log_level_get(void *data, u64 *val)
{
	*val = m4u_log_level | (m4u_log_to_uart << 4);

	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_log_level_fops,
	m4u_log_level_get,
	m4u_log_level_set,
	"%llu\n");
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
DEFINE_PROC_ATTRIBUTE(m4u_proc_log_level_fops,
	m4u_log_level_get,
	m4u_log_level_set,
	"%llu\n");
#endif

int m4u_debug_help_show(struct seq_file *s, void *unused)
{
	M4U_PRINT_SEQ(s,
		      "echo 8'd1 > /d/m4u/debug:	boundary0 domain: map/unmap the sg table(512 count of 4KB pages) to IOVA(aligned of 0x4000)\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd2 > /d/m4u/debug:	boundary1 domain: map/unmap the sg table(51 count of 64KB pages) to IOVA(aligned of 0x10000)\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd3 > /d/m4u/debug:	boundary2 domain: map/unmap the sg table(37 count of 1MB pages) to IOVA(aligned of 0x100000)\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd4 > /d/m4u/debug:	boundary3 domain: map/unmap the sg table(2 count of 16MB pages) to IOVA(aligned of 0x1000000)\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd5 > /d/m4u/debug:	unknown domain: map/unmap the sg table(1 count of 32MB pages) to IOVA(aligned of 0x4000)\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd6 > /d/m4u/debug:	map/unmap 4MB kernel space virtual buffer(kmalloc) to IOVA\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd7 > /d/m4u/debug:	map/unmap 4MB kernel space virtual buffer(vmalloc) to IOVA\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd8 > /d/m4u/debug:	map/unmap 4MB user space virtual buffer(do_mmap_pgoff) to IOVA\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd9 > /d/m4u/debug:	CCU domain: map/unmap the sg table(512 count of 4KB pages) to IOVA(aligned of 0x4000)\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd10 > /d/m4u/debug:	APU CODE domain: map/unmap the sg table(512 count of 4KB pages) to IOVA(aligned of 0x4000)\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd11 > /d/m4u/debug:	map userspace VA to IOVA / map IOVA to kernel VA / unmap kernel VA / unmap IOVA / unmap userspace VA\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd12 > /d/m4u/debug:	do display memory test\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd13 > /d/m4u/debug:	do IOVA mapping of OVL, start performance monitor of OVL, trigger OVL read/write\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd14 > /d/m4u/debug:	register the fault callback of OVL and MDP, check if it works well\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd15 > /d/m4u/debug:	ION test: alloc / config port / IOVA mapping / performance monitor\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd16 > /d/m4u/debug:	dump main TLB\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd17 > /d/m4u/debug:	dump prefetch TLB\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd18 > /d/m4u/debug:	start MAU monitor of 4KB~4GB\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd19 > /d/m4u/debug:	stop MAU monitor of 4KB~4GB\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd20 > /d/m4u/debug:	config all ports of IOVA path\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd21 > /d/m4u/debug:	config all ports of PA path\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd22 > /d/m4u/debug:	aee dump of the top 5 users of IOVA space\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd23 > /d/m4u/debug:	dump IOVA page table base address\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd24 > /d/m4u/debug:	dump the PA addr mapped by the target IOVA\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd25 > /d/m4u/debug:	start performance monitor\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd26 > /d/m4u/debug:	stop performance monitor\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd27 > /d/m4u/debug:	dump the debug registers of SMI bus hang\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd28 > /d/m4u/debug:	test display fake engine read/write\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd29 > /d/m4u/debug:	enable translation fault debug\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd30 > /d/m4u/debug:	disable translation fault debug\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd31 > /d/m4u/debug:	iommu power on/off test\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd33 > /d/m4u/debug:	disable mau monitor interrupt\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd34 > /d/m4u/debug:	TLB flush All\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd35 > /d/m4u/debug:	enable iommu bank irq test\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd36 > /d/m4u/debug:	disable iommu bank irq test\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd37 > /d/m4u/debug:	dump iommu mau info\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd50 > /d/m4u/debug:	init the Trustlet and T-drv of secure IOMMU\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd51 > /d/m4u/debug:	IOMMU ATF (all) command test\n");
	M4U_PRINT_SEQ(s,
		      "echo 8'd52 > /d/m4u/debug:	init the genie zone secure iommu\n");

	M4U_PRINT_SEQ(s, "\nM4U_MAGIC_KEY:0x%x(bit31~bit8)\n", M4U_MAGIC_KEY);
	M4U_PRINT_SEQ(s, "ex: adb shell \"echo 0x%xxx > /d/m4u/debug\"\n",
			M4U_MAGIC_KEY);
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_help_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_help_show, inode->i_private);
}

const struct file_operations m4u_debug_help_fops = {
	.open = m4u_debug_help_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_help_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_help_show, PDE_DATA(inode));
}

const struct file_operations m4u_proc_help_fops = {
	.open = m4u_proc_help_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int m4u_debug_domain_show(struct seq_file *s, void *unused)
{
	pr_notice("[iommu][debug]: %s\n", __func__);
	pseudo_dump_iova_reserved_region(s);
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_domain_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_domain_show, inode->i_private);
}

const struct file_operations m4u_debug_domain_fops = {
	.open = m4u_debug_domain_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_domain_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_domain_show, PDE_DATA(inode));
}

const struct file_operations m4u_proc_domain_fops = {
	.open = m4u_proc_domain_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int m4u_debug_port_show(struct seq_file *s, void *unused)
{
	pr_notice("[iommu][debug]: %s\n", __func__);
	pseudo_dump_all_port_status(s);
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_port_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_port_show, inode->i_private);
}

const struct file_operations m4u_debug_port_fops = {
	.open = m4u_debug_port_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_port_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_port_show, PDE_DATA(inode));
}

const struct file_operations m4u_proc_port_fops = {
	.open = m4u_proc_port_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int m4u_debug_buf_show(struct seq_file *s, void *unused)
{
	pr_notice("[iommu][debug]: %s\n", __func__);
	__m4u_dump_pgtable(s, 1, true, 0);
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_buf_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_buf_show, inode->i_private);
}

const struct file_operations m4u_debug_buf_fops = {
	.open = m4u_debug_buf_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_buf_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_buf_show, PDE_DATA(inode));
}

const struct file_operations m4u_proc_buf_fops = {
	.open = m4u_proc_buf_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int m4u_debug_monitor_show(struct seq_file *s, void *unused)
{
	int i, j;

	pr_notice("[iommu][debug]: %s\n", __func__);
	for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++)
		for (j = 0; j < MTK_IOMMU_MMU_COUNT; j++)
			iommu_perf_print_counter(i, j, "monitor");
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_monitor_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_monitor_show, inode->i_private);
}

const struct file_operations m4u_debug_monitor_fops = {
	.open = m4u_debug_monitor_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif


#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_monitor_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_monitor_show, PDE_DATA(inode));
}

const struct file_operations m4u_proc_monitor_fops = {
	.open = m4u_proc_monitor_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int m4u_debug_register_show(struct seq_file *s, void *unused)
{
	int i;

	pr_notice("[iommu][debug]: %s\n", __func__);
	for (i = 0; i < MTK_IOMMU_M4U_COUNT; i++)
		mtk_iommu_dump_reg(i, 0, 400, "/d/m4u/register");

	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_register_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_register_show, inode->i_private);
}

const struct file_operations m4u_debug_register_fops = {
	.open = m4u_debug_register_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_register_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_register_show, PDE_DATA(inode));
}

const struct file_operations m4u_proc_register_fops = {
	.open = m4u_proc_register_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif


int m4u_debug_db_show(struct seq_file *s, void *unused)
{
	pr_debug("[iommu][debug]: %s\n", __func__);
	pseudo_m4u_db_debug(MTK_IOMMU_M4U_COUNT, s);
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
int m4u_debug_db_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_db_show, inode->i_private);
}

const struct file_operations m4u_debug_db_fops = {
	.open = m4u_debug_db_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
int m4u_proc_db_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_db_show, PDE_DATA(inode));
}

const struct file_operations m4u_proc_db_fops = {
	.open = m4u_proc_db_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

int pseudo_debug_init(struct m4u_device *m4u_dev)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *debug_file;
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
	struct proc_dir_entry *proc_file;
#endif

#if IS_ENABLED(CONFIG_DEBUG_FS)
	m4u_dev->debug_root = debugfs_create_dir("m4u", NULL);

	if (IS_ERR_OR_NULL(m4u_dev->debug_root))
		M4U_MSG("m4u: failed to create debug dir.\n");

	debug_file = debugfs_create_file("help",
		0644, m4u_dev->debug_root, NULL, &m4u_debug_help_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4U_MSG("m4u: failed to create debug files 0.\n");

	debug_file = debugfs_create_file("buffer",
		0644, m4u_dev->debug_root, NULL, &m4u_debug_buf_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4U_MSG("m4u: failed to create debug files 1.\n");

	debug_file = debugfs_create_file("debug",
		0644, m4u_dev->debug_root, NULL, &m4u_debug_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4U_MSG("m4u: failed to create debug files 2.\n");

	debug_file = debugfs_create_file("port",
		0644, m4u_dev->debug_root, NULL, &m4u_debug_port_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4U_MSG("m4u: failed to create debug files 4.\n");

	debug_file = debugfs_create_file("log_level",
		0644, m4u_dev->debug_root, NULL, &m4u_debug_log_level_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4U_MSG("m4u: failed to create debug files 5.\n");

	debug_file = debugfs_create_file("monitor",
		0644, m4u_dev->debug_root, NULL, &m4u_debug_monitor_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4U_MSG("m4u: failed to create debug files 6.\n");

	debug_file = debugfs_create_file("register",
		0644, m4u_dev->debug_root, NULL, &m4u_debug_register_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4U_MSG("m4u: failed to create debug files 7.\n");

	debug_file = debugfs_create_file("domain",
		0644, m4u_dev->debug_root, NULL, &m4u_debug_domain_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4U_MSG("m4u: failed to create debug files 8.\n");

	debug_file = debugfs_create_file("iommu_db_dump",
		0644, m4u_dev->debug_root, NULL, &m4u_debug_db_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4U_MSG("m4u: failed to create debug files 9.\n");
#endif

#if IS_ENABLED(CONFIG_PROC_FS)
	m4u_dev->proc_root = proc_mkdir("m4u_dbg", NULL);

	if (IS_ERR_OR_NULL(m4u_dev->proc_root))
		M4U_MSG("m4u: failed to create proc dir.\n");

	proc_file = proc_create("help",
				S_IFREG | 0644,
				m4u_dev->proc_root,
				&m4u_proc_help_fops);
	if (IS_ERR_OR_NULL(proc_file))
		M4U_MSG("m4u: failed to create proc files 0.\n");

	proc_file = proc_create("buffer",
				S_IFREG | 0644,
				m4u_dev->proc_root,
				&m4u_proc_buf_fops);
	if (IS_ERR_OR_NULL(proc_file))
		M4U_MSG("m4u: failed to create proc files 1.\n");

	proc_file = proc_create("debug",
				S_IFREG | 0644,
				m4u_dev->proc_root,
				&m4u_proc_fops);
	if (IS_ERR_OR_NULL(proc_file))
		M4U_MSG("m4u: failed to create proc files 2.\n");

	proc_file = proc_create("port",
				S_IFREG | 0644,
				m4u_dev->proc_root,
				&m4u_proc_port_fops);
	if (IS_ERR_OR_NULL(proc_file))
		M4U_MSG("m4u: failed to create proc files 4.\n");

	proc_file = proc_create("log_level",
				S_IFREG | 0644,
				m4u_dev->proc_root,
				&m4u_proc_log_level_fops);
	if (IS_ERR_OR_NULL(proc_file))
		M4U_MSG("m4u: failed to create proc files 5.\n");

	proc_file = proc_create("monitor",
				S_IFREG | 0644,
				m4u_dev->proc_root,
				&m4u_proc_monitor_fops);
	if (IS_ERR_OR_NULL(proc_file))
		M4U_MSG("m4u: failed to create proc files 6.\n");

	proc_file = proc_create("register",
				S_IFREG | 0644,
				m4u_dev->proc_root,
				&m4u_proc_register_fops);
	if (IS_ERR_OR_NULL(proc_file))
		M4U_MSG("m4u: failed to create proc files 7.\n");

	proc_file = proc_create("domain",
				S_IFREG | 0644,
				m4u_dev->proc_root,
				&m4u_proc_domain_fops);
	if (IS_ERR_OR_NULL(proc_file))
		M4U_MSG("m4u: failed to create proc files 8.\n");

	proc_file = proc_create("iommu_db_dump",
				S_IFREG | 0644,
				m4u_dev->proc_root,
				&m4u_proc_db_fops);
	if (IS_ERR_OR_NULL(proc_file))
		M4U_MSG("m4u: failed to create proc files 9.\n");
#endif

	return 0;
}
#else
int pseudo_debug_init(struct m4u_device *m4u_dev)
{
	/* do nothing */
}
#endif
