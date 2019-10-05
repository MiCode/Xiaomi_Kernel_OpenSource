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
#include "secmem_api.h"
#endif

int test_mva;

/* global variables */
int gM4U_log_to_uart = 2;
int gM4U_log_level = 2;

unsigned int gM4U_seed_mva;

int m4u_test_alloc_dealloc(int id, unsigned int size)
{
	struct m4u_client_t *client;
	unsigned long va = 0;
	unsigned long mva;
	int ret;
	unsigned long populate;

	if (id == 1)
		va = (unsigned long)kmalloc(size, GFP_KERNEL);
	else if (id == 2)
		va = (unsigned long)vmalloc(size);
	else if (id == 3) {
		down_write(&current->mm->mmap_sem);
		va = do_mmap_pgoff(NULL, 0, size,
			PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED,
			0, &populate, NULL);
		up_write(&current->mm->mmap_sem);
	}

	M4U_INFO("test va=0x%lx,size=0x%x\n", va, size);

	client = pseudo_create_client();
	if (IS_ERR_OR_NULL(client))
		M4U_MSG("create client fail!\n");

	ret = __pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG, va, size, NULL,
			    0, &mva);
	if (ret) {
		M4U_MSG("alloc mva fail:va=0x%lx,size=0x%x,ret=%d\n",
			va, size, ret);
		return -1;
	}
	m4u_dump_pgtable();

	ret = pseudo_dealloc_mva(client, M4U_PORT_OVL_DEBUG, mva);
	m4u_dump_pgtable();

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

/* clean */
	pseudo_destroy_client(client);
	return 0;
}

static int m4u_test_map_kernel(void)
{
	struct m4u_client_t *client;
	unsigned long va;
	unsigned int size = 1024 * 1024;
	unsigned long mva;
	unsigned long kernel_va;
	unsigned long kernel_size;
	int i;
	int ret;
	unsigned long populate;

	down_write(&current->mm->mmap_sem);
	va = do_mmap_pgoff(NULL, 0, size,
		PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED,
		0, &populate, NULL);
	up_write(&current->mm->mmap_sem);

	M4U_INFO("test va=0x%lx,size=0x%x\n", va, size);

	for (i = 0; i < size; i += 4)
		*(int *)(va + i) = i;

	client = pseudo_create_client();
	if (IS_ERR_OR_NULL(client))
		M4U_MSG("createclientfail!\n");

	ret = __pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG,
		va, size, NULL, 0, &mva);
	if (ret) {
		M4U_MSG("alloc using kmalloc fail:va=0x%lx,size=0x%x\n",
			va, size);
		return -1;
	}

	ret = m4u_mva_map_kernel(mva, size, &kernel_va, &kernel_size);
	if (ret) {
		M4U_MSG("map kernel fail!\n");
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

	pseudo_destroy_client(client);
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
	struct m4u_client_t *client = pseudo_create_client();

	pSrc = vmalloc(size);
	pDst = vmalloc(size);

	__pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG, *pSrc,
		      size, NULL, 0, &src_pa);

	__pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG, *pDst,
		      size, NULL, 0, &dst_pa);

	M4U_INFO("pSrc=0x%p, pDst=0x%p, src_pa=0x%x, dst_pa=0x%x\n",
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

	pseudo_destroy_client(client);
	return 0;
}

enum mtk_iommu_callback_ret_t test_fault_callback(int port,
		unsigned int mva, void *data)
{
	if (data != NULL)
		M4U_MSG("fault call port=%d, mva=0x%x, data=0x%x\n",
			port, mva, *(int *)data);
	else
		M4U_MSG("fault call port=%d, mva=0x%x\n", port, mva);

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
	struct m4u_client_t *client = pseudo_create_client();
	int data = 88;

	mtk_iommu_register_fault_callback(M4U_PORT_OVL_DEBUG,
		test_fault_callback, &data);
	mtk_iommu_register_fault_callback(M4U_PORT_MDP_DEBUG,
		test_fault_callback, &data);

	pSrc = vmalloc(size);
	pDst = vmalloc(size);

	__pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG, *pSrc,
		      size, NULL, 0, &src_pa);

	__pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG, *pDst,
		      size / 2, NULL, 0, &dst_pa);

	M4U_INFO("pSrc=0x%p, pDst=0x%p, src_pa=0x%x, dst_pa=0x%x\n",
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

	pseudo_destroy_client(client);

	return 0;
}

#if 0
#include <mtk/ion_drv.h>

void m4u_test_ion(void)
{
	unsigned int *pSrc, *pDst;
	unsigned long src_pa, dst_pa;
	unsigned int size = 64 * 64 * 3, tmp_size;
	struct M4U_PORT_STRUCT port;
	struct ion_mm_data mm_data;
	struct ion_client *ion_client;
	struct ion_handle *src_handle, *dst_handle;

	/* FIX-ME: modified for linux-3.10 early porting */
	/* ion_client = ion_client_create(g_ion_device, 0xffffffff, "test"); */
	ion_client = ion_client_create(g_ion_device, "test");

	src_handle = ion_alloc(ion_client,
		size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);
	dst_handle = ion_alloc(ion_client,
		size, 0, ION_HEAP_MULTIMEDIA_MASK, 0);

	pSrc = ion_map_kernel(ion_client, src_handle);
	pDst = ion_map_kernel(ion_client, dst_handle);

	mm_data.config_buffer_param.kernel_handle = src_handle;
	mm_data.config_buffer_param.eModuleID = M4U_PORT_OVL_DEBUG;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 0;
	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	if (ion_kernel_ioctl(ion_client,
		ION_CMD_MULTIMEDIA, (unsigned long)&mm_data) < 0)
		M4U_MSG("ion_test_drv: Config buffer failed.\n");

	mm_data.config_buffer_param.kernel_handle = dst_handle;
	if (ion_kernel_ioctl(ion_client,
		ION_CMD_MULTIMEDIA, (unsigned long)&mm_data) < 0)
		M4U_MSG("ion_test_drv: Config buffer failed.\n");

	ion_phys(ion_client, src_handle, &src_pa, (size_t *)&tmp_size);
	ion_phys(ion_client, dst_handle, &dst_pa, (size_t *)&tmp_size);

	M4U_MSG("ion alloced: pSrc=0x%p, pDst=0x%p, src_pa=%lu, dst_pa=%lu\n",
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

	ion_free(ion_client, src_handle);
	ion_free(ion_client, dst_handle);

	ion_client_destroy(ion_client);
}
#else
#define m4u_test_ion(...)
#endif

static int m4u_debug_set(void *data, u64 val)
{

	M4U_MSG("%s:val=%llu\n", __func__, val);

	switch (val) {
	case 1:
	{                   /* map4kpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i;
		struct page *page;
		int page_num = 512;
		struct device *dev = pseudo_get_larbdev(M4U_PORT_UNKNOWN);

		page = alloc_pages(GFP_KERNEL, get_order(page_num));
		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i)
			sg_set_page(sg, page + i, PAGE_SIZE, 0);
		dma_map_sg_attrs(dev, sg_table->sgl,
				sg_table->nents,
				DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);

		m4u_dump_pgtable();

		dma_unmap_sg_attrs(dev, sg_table->sgl,
				sg_table->orig_nents,
				DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);
		m4u_dump_pgtable();

		sg_free_table(sg_table);
		__free_pages(page, get_order(page_num));
	}
	break;
	case 2:
	{                   /* map64kpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i;
		int page_num = 51;
		unsigned long page_size = SZ_64K;
		struct device *dev = pseudo_get_larbdev(M4U_PORT_APU);

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
		}

		dma_map_sg_attrs(dev, sg_table->sgl,
				sg_table->nents,
				DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);

		m4u_dump_pgtable();

		dma_unmap_sg_attrs(dev, sg_table->sgl,
				sg_table->orig_nents,
				DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);
		m4u_dump_pgtable();
		sg_free_table(sg_table);
	}
	break;
	case 3:
	{                   /* map1Mpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i;
		int page_num = 37;
		unsigned long page_size = SZ_1M;
		struct device *dev = pseudo_get_larbdev(M4U_PORT_CCU);

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);

		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
		}
		dma_map_sg_attrs(dev, sg_table->sgl,
				sg_table->nents,
				DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);

		m4u_dump_pgtable();

		dma_unmap_sg_attrs(dev, sg_table->sgl,
				sg_table->orig_nents,
				DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);
		m4u_dump_pgtable();

		sg_free_table(sg_table);
		}
		break;
	case 4:
	{                   /* map16Mpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i;
		int page_num = 2;
		unsigned long page_size = SZ_16M;
		struct device *dev = pseudo_get_larbdev(M4U_PORT_UNKNOWN);

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
		}
		dma_map_sg_attrs(dev, sg_table->sgl,
				sg_table->nents,
				DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);

		m4u_dump_pgtable();

		dma_unmap_sg_attrs(dev, sg_table->sgl,
				sg_table->orig_nents,
				DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);
		m4u_dump_pgtable();
		sg_free_table(sg_table);
		}
		break;
	case 5:
	{                   /* mapmiscpages */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		unsigned int size = SZ_16M * 2;
		struct device *dev = pseudo_get_larbdev(M4U_PORT_APU);

		sg_alloc_table(sg_table, 1, GFP_KERNEL);
		sg = sg_table->sgl;
		sg_dma_address(sg) = 0x4000;
		sg_dma_len(sg) = size;

		dma_map_sg_attrs(dev, sg_table->sgl,
				sg_table->nents,
				DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);

		m4u_dump_pgtable();

		dma_unmap_sg_attrs(dev, sg_table->sgl,
				sg_table->orig_nents,
				DMA_BIDIRECTIONAL,
				DMA_ATTR_SKIP_CPU_SYNC);
		m4u_dump_pgtable();
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
	break;
	case 10:
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
		mtk_dump_main_tlb(0, 0);
		break;
	case 17:
		mtk_dump_pfh_tlb(0);
		break;
	case 18:
	{
		if (TOTAL_M4U_NUM > 2)
			mtk_dump_main_tlb(1, 0);
		break;
	}
	case 19:
	{
		if (TOTAL_M4U_NUM > 1)
			mtk_dump_pfh_tlb(1);
		break;
	}
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
	break;
	case 23:
	{
		void *pgd_pa;

		if (mtk_iommu_get_pgtable_base_addr(pgd_pa)) {
			M4U_MSG("failed to get pgd info\n");
			break;
		}
		M4U_MSG("pgd_pa:0x%p\n",
			pgd_pa);
	}
	break;
	case 24:
	{
		unsigned int *pSrc;
		unsigned long mva;
		unsigned long pa;
		struct m4u_client_t *client = pseudo_create_client();
		struct device *dev = pseudo_get_larbdev(M4U_PORT_UNKNOWN);

		pSrc = vmalloc(128);
		__pseudo_alloc_mva(client, M4U_PORT_OVL_DEBUG,
			(unsigned long)*pSrc, 128, NULL, 0, &mva);

		m4u_dump_pgtable();

		mtk_iommu_iova_to_pa(dev, mva, &pa);
		M4U_MSG("(1) mva:0x%x pa:0x%lx\n", mva, pa);
			pseudo_dealloc_mva(client, M4U_PORT_OVL_DEBUG, mva);
		mtk_iommu_iova_to_pa(dev, mva, &pa);
		M4U_MSG("(2) mva:0x%x pa:0x%lx\n", mva, pa);
		pseudo_destroy_client(client);
	}
	break;
	case 25:
		iommu_perf_monitor_start(0);
		break;
	case 26:
		iommu_perf_monitor_stop(0);
		break;
	case 27:
		mtk_dump_reg_for_hang_issue(0);
		mtk_dump_reg_for_hang_issue(1);
		break;
	case 28:
	{
		mtk_dump_reg_for_hang_issue(3);
		mtk_dump_reg_for_hang_issue(4);
#if 0
		unsigned char *pSrc;
		unsigned char *pDst;
		unsigned int mva_rd;
		unsigned int mva_wr;
		unsigned int allocated_size = 1024;
		unsigned int i;
		struct m4u_client_t *client = pseudo_create_client();

		iommu_perf_monitor_start(0);

		pSrc = vmalloc(allocated_size);
		memset(pSrc, 0xFF, allocated_size);
		M4U_MSG("(0) vmalloc pSrc:0x%p\n", pSrc);
		pDst =  vmalloc(allocated_size);
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

		m4u_dump_pgtable();

		m4u_display_fake_engine_test(mva_rd, mva_wr);

		M4U_MSG("(2) mva_wr:0x%x\n", mva_wr);

		pseudo_dealloc_mva(client, M4U_PORT_OVL_DEBUG, mva_rd);
		pseudo_dealloc_mva(client, M4U_PORT_OVL_DEBUG, mva_wr);

		pseudo_destroy_client(client);

		M4U_MSG("(3) pDst check 0x%x 0x%x 0x%x 0x%x  0x%x\n",
			*pDst, *(pDst+1), *(pDst+126), *(pDst+127),
			*(pDst+128));

		for (i = 0; i < 128; i++) {
			if (*(pDst+i) != 0) {
				M4U_MSG("(4) [Error] pDst check fail VA\n");
				M4U_MSG("0x%p: 0x%x\n",
					pDst+i*sizeof(unsigned char),
					*(pDst+i));
				break;
			}
		}
		if (i == 128)
			M4U_MSG("(4) m4u_disp_fake_test R/W 128 bytes PASS\n ");

		vfree(pSrc);
		vfree(pDst);

		iommu_perf_monitor_stop(0);
#endif
		break;
	}
	case 29:
	break;
	case 30:
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
	break;
	}
	case 51:
	{
		struct M4U_PORT_STRUCT port;

		memset(&port, 0, sizeof(struct M4U_PORT_STRUCT));

		port.ePortID = M4U_PORT_OVL_DEBUG;
		port.Virtuality = 1;
		M4U_MSG("(0) config port: mmu: %d, sec: %d\n",
			port.Virtuality, port.Security);
		m4u_config_port(&port);
		port.ePortID = M4U_PORT_MDP_DEBUG;
		m4u_config_port(&port);
		/* port.ePortID = M4U_PORT_MJC_MV_RD;*/
		/*m4u_config_port(&port); */

		port.ePortID = M4U_PORT_MDP_DEBUG;
		M4U_MSG("(1) config port: mmu: %d, sec: %d\n",
			port.Virtuality, port.Security);
		m4u_config_port_tee(&port);
		port.Security = 1;
		M4U_MSG("(2) config port: mmu: %d, sec: %d\n",
			port.Virtuality, port.Security);
		m4u_config_port_tee(&port);
	}
	break;
#endif
	case 52:
	{
		pseudo_dump_iova_reserved_region();
	}
	break;
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

DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_fops, m4u_debug_get, m4u_debug_set, "%llu\n");

static int m4u_log_level_set(void *data, u64 val)
{
	gM4U_log_to_uart = (val & 0xf0) >> 4;
	gM4U_log_level = val & 0xf;
	M4U_MSG("gM4U_log_level: %d, gM4U_log_to_uart:%d\n",
		gM4U_log_level, gM4U_log_to_uart);

	return 0;
}

static int m4u_log_level_get(void *data, u64 *val)
{
	*val = gM4U_log_level | (gM4U_log_to_uart << 4);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(m4u_log_level_fops,
	m4u_log_level_get, m4u_log_level_set, "%llu\n");

int m4u_debug_port_show(struct seq_file *s, void *unused)
{
	pseudo_dump_all_port_status(s);
	return 0;
}

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

int m4u_debug_buf_show(struct seq_file *s, void *unused)
{
	m4u_dump_pgtable();
	return 0;
}

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

int m4u_debug_monitor_show(struct seq_file *s, void *unused)
{
	iommu_perf_print_counter(0, 0, "monitor");
	return 0;
}

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

int m4u_debug_register_show(struct seq_file *s, void *unused)
{
	mtk_iommu_dump_reg(0, 0, 400);
	return 0;
}

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

int pseudo_debug_init(struct m4u_device *m4u_dev)
{
	struct dentry *debug_file;

	m4u_dev->debug_root = debugfs_create_dir("m4u", NULL);

	if (IS_ERR_OR_NULL(m4u_dev->debug_root))
		M4U_MSG("m4u: failed to create debug dir.\n");

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
		0644, m4u_dev->debug_root, NULL, &m4u_log_level_fops);
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

	return 0;
}
