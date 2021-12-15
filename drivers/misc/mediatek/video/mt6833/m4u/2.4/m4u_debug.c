/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>

#include "m4u_debug.h"
#include "m4u_priv.h"

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"
#endif

#if defined(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)
#include "trusted_mem_api.h"
#endif

/* global variables */
int gM4U_log_to_uart = 2;
int gM4U_log_level = 2;

unsigned int gM4U_seed_mva;

int m4u_test_alloc_dealloc(int id, unsigned int size)
{
	struct m4u_client_t *client;
	unsigned long va = 0;
	unsigned int mva;
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

	M4UINFO("test va=0x%lx,size=0x%x\n", va, size);

	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client))
		M4UMSG("create client fail!\n");

	ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, va, NULL, size,
			    M4U_PROT_READ | M4U_PROT_CACHE, 0, &mva);
	if (ret) {
		M4UMSG(
			"alloc mva fail:va=0x%lx,size=0x%x,ret=%d\n",
			va, size, ret);
		return -1;
	}
	m4u_dump_pgtable(m4u_get_domain_by_port(M4U_PORT_DISP_OVL0), NULL);

	ret = m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, mva);
	m4u_dump_pgtable(m4u_get_domain_by_port(M4U_PORT_DISP_OVL0), NULL);

	if (id == 1)
		kfree((void *)va);
	else if (id == 2)
		vfree((void *)va);
	else if (id == 3) {
		down_read(&current->mm->mmap_sem);
		ret = do_munmap(current->mm, va, size, NULL);
		up_read(&current->mm->mmap_sem);
		if (ret)
			M4UMSG("do_munmap failed\n");
	}

/* clean */
	m4u_destroy_client(client);
	return 0;
}

enum m4u_callback_ret_t m4u_test_callback(int alloc_port, unsigned int mva,
				     unsigned int size, void *data)
{
	if (data != NULL)
		M4UMSG(
		"test callback port=%d, mva=0x%x, size=0x%x, data=0x%x\n",
		alloc_port, mva, size, *(int *)data);
	else
		M4UMSG(
		"test callback port=%d, mva=0x%x, size=0x%x\n",
		alloc_port, mva, size);

	return M4U_CALLBACK_HANDLED;
}

int m4u_test_reclaim(unsigned int size)
{
	struct m4u_client_t *client;
	unsigned int *va[10];
	unsigned int buf_size;
	unsigned int mva;
	int ret, i;

	/* register callback */
	m4u_register_reclaim_callback(M4U_PORT_DISP_OVL0,
		m4u_test_callback, NULL);

	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client))
		M4UMSG("createclientfail!\n");

	buf_size = size;
	for (i = 0; i < 10; i++) {
		va[i] = vmalloc(buf_size);

		ret = m4u_alloc_mva(client,
				M4U_PORT_DISP_OVL0, (unsigned long)va[i],
				NULL, buf_size,
				M4U_PROT_READ | M4U_PROT_CACHE, 0, &mva);
		if (ret) {
			M4UMSG(
				"alloc using kmalloc fail:va=0x%p,size=0x%x\n",
				va[i], buf_size);
			return -1;
		}
		M4UINFO(
			"alloc mva:va=0x%p,mva=0x%x,size=0x%x\n",
			va[i], mva, buf_size);

		buf_size += size;
	}

	for (i = 0; i < 10; i++)
		vfree((void *)va[i]);

	m4u_dump_buf_info(NULL);
	m4u_dump_pgtable(m4u_get_domain_by_port(M4U_PORT_DISP_OVL0), NULL);

	m4u_destroy_client(client);

	m4u_unregister_reclaim_callback(M4U_PORT_DISP_OVL0);

	return 0;
}

static int m4u_test_map_kernel(void)
{
	struct m4u_client_t *client;
	unsigned long va;
	unsigned int size = 1024 * 1024;
	unsigned int mva;
	unsigned long kernel_va;
	unsigned int kernel_size;
	int i;
	int ret;
	unsigned long populate;

	down_write(&current->mm->mmap_sem);
	va = do_mmap_pgoff(NULL, 0, size,
		PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED,
		0, &populate, NULL);
	up_write(&current->mm->mmap_sem);

	M4UINFO("test va=0x%lx,size=0x%x\n", va, size);

	for (i = 0; i < size; i += 4)
		*(int *)(va + i) = i;

	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client))
		M4UMSG("createclientfail!\n");

	ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, va, NULL, size,
		M4U_PROT_READ | M4U_PROT_CACHE, 0, &mva);
	if (ret) {
		M4UMSG(
			"alloc using kmalloc fail:va=0x%lx,size=0x%x\n",
			va, size);
		return -1;
	}

	ret = m4u_mva_map_kernel(mva, size, &kernel_va, &kernel_size);
	if (ret) {
		M4UMSG("map kernel fail!\n");
		return -1;
	}
	for (i = 0; i < size; i += 4) {
		if (*(int *)(kernel_va + i) != i) {
			M4UMSG("wawawa, get map value fail! i=%d, map=%d\n", i,
			       *(int *)(kernel_va + i));
		}
	}

	ret = m4u_mva_unmap_kernel(mva, size, kernel_va);

	ret = m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, mva);
	down_read(&current->mm->mmap_sem);
	ret = do_munmap(current->mm, va, size, NULL);
	up_read(&current->mm->mmap_sem);
	if (ret)
		M4UMSG("do_munmap failed\n");

	m4u_destroy_client(client);
	return 0;
}

int m4u_test_ddp(unsigned int prot)
{
	unsigned int *pSrc, *pDst;
	unsigned int src_pa, dst_pa;
	unsigned int size = 64 * 64 * 3;
	struct M4U_PORT_STRUCT port;
	struct m4u_client_t *client = m4u_create_client();

	pSrc = vmalloc(size);
	pDst = vmalloc(size);

	m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned long)pSrc, NULL,
		      size, prot, 0, &src_pa);

	m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned long)pDst, NULL,
		      size, prot, 0, &dst_pa);

	M4UINFO(
		"pSrc=0x%p, pDst=0x%p, src_pa=0x%x, dst_pa=0x%x\n",
		pSrc, pDst, src_pa, dst_pa);

	port.ePortID = M4U_PORT_DISP_OVL0;
	port.Direction = 0;
	port.Distance = 1;
	port.domain = 3;
	port.Security = 0;
	port.Virtuality = 1;
	m4u_config_port(&port);

	port.ePortID = M4U_PORT_DISP_WDMA0;
	m4u_config_port(&port);

	m4u_monitor_start(0);
	__ddp_mem_test(pSrc, src_pa, pDst, dst_pa, !(prot & M4U_PROT_CACHE));
	m4u_monitor_stop(0);

	vfree(pSrc);
	vfree(pDst);

	m4u_destroy_client(client);
	return 0;
}

enum m4u_callback_ret_t test_fault_callback(
	int port, unsigned int mva, void *data)
{
	if (data != NULL)
		M4UMSG(
			"fault call port=%d, mva=0x%x, data=0x%x\n",
			port, mva, *(int *)data);
	else
		M4UMSG("fault call port=%d, mva=0x%x\n", port, mva);

	/* DO NOT print too much logs here !!!! */
	/* Do NOT use any lock hear !!!! */
	/* DO NOT do any other things except print !!! */
	/* DO NOT make any mistake here (or reboot will happen) !!! */
	return M4U_CALLBACK_HANDLED;
}

int m4u_test_tf(unsigned int prot)
{
	unsigned int *pSrc, *pDst;
	unsigned int src_pa, dst_pa;
	unsigned int size = 64 * 64 * 3;
	struct M4U_PORT_STRUCT port;
	struct m4u_client_t *client = m4u_create_client();
	int data = 88;

	m4u_register_fault_callback(M4U_PORT_DISP_OVL0,
		test_fault_callback, &data);
	m4u_register_fault_callback(M4U_PORT_DISP_WDMA0,
		test_fault_callback, &data);

	pSrc = vmalloc(size);
	pDst = vmalloc(size);

	m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned long)pSrc, NULL,
		      size, prot, 0, &src_pa);

	m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned long)pDst, NULL,
		      size / 2, prot, 0, &dst_pa);

	M4UINFO(
		"pSrc=0x%p, pDst=0x%p, src_pa=0x%x, dst_pa=0x%x\n",
		pSrc, pDst, src_pa, dst_pa);

	port.ePortID = M4U_PORT_DISP_OVL0;
	port.Direction = 0;
	port.Distance = 1;
	port.domain = 3;
	port.Security = 0;
	port.Virtuality = 1;
	m4u_config_port(&port);

	port.ePortID = M4U_PORT_DISP_WDMA0;
	m4u_config_port(&port);

	m4u_monitor_start(0);
	__ddp_mem_test(pSrc, src_pa, pDst, dst_pa, !!(prot & M4U_PROT_CACHE));
	m4u_monitor_stop(0);

	m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, src_pa);
	m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, dst_pa);

	vfree(pSrc);
	vfree(pDst);

	m4u_destroy_client(client);

	return 0;
}

#if 0
#include <mtk/ion_drv.h>

void m4u_test_ion(void)
{
	unsigned int *pSrc, *pDst;
	unsigned long src_pa, dst_pa;
	unsigned int size = 64 * 64 * 3, tmp_size;
	M4U_PORT_STRUCT port;
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
	mm_data.config_buffer_param.eModuleID = M4U_PORT_DISP_OVL0;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 0;
	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	if (ion_kernel_ioctl(ion_client,
		ION_CMD_MULTIMEDIA, (unsigned long)&mm_data) < 0)
		M4UMSG("ion_test_drv: Config buffer failed.\n");

	mm_data.config_buffer_param.kernel_handle = dst_handle;
	if (ion_kernel_ioctl(ion_client,
		ION_CMD_MULTIMEDIA, (unsigned long)&mm_data) < 0)
		M4UMSG("ion_test_drv: Config buffer failed.\n");

	ion_phys(ion_client, src_handle, &src_pa, (size_t *)&tmp_size);
	ion_phys(ion_client, dst_handle, &dst_pa, (size_t *)&tmp_size);

	M4UMSG("ion alloced: pSrc=0x%p,	pDst=0x%p, src_pa=%lu, dst_pa=%lu\n",
		pSrc, pDst, src_pa, dst_pa);

	port.ePortID = M4U_PORT_DISP_OVL0;
	port.Direction = 0;
	port.Distance = 1;
	port.domain = 3;
	port.Security = 0;
	port.Virtuality = 1;
	m4u_config_port(&port);

	port.ePortID = M4U_PORT_DISP_WDMA0;
	m4u_config_port(&port);

	m4u_monitor_start(0);
	__ddp_mem_test(pSrc, src_pa, pDst, dst_pa, 0);
	m4u_monitor_stop(0);

	ion_free(ion_client, src_handle);
	ion_free(ion_client, dst_handle);

	ion_client_destroy(ion_client);
}
#else
#define m4u_test_ion(...)
#endif

static int m4u_debug_set(void *data, u64 val)
{
	struct m4u_domain *domain = data;

	M4UMSG("%s:val=%llu\n", __func__, val);

	switch (val) {
	case 1:
	{                   /* map4kpageonly */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		int i;
		struct page *page;
		int page_num = 512;
		unsigned int mva = 0x4000;

		page = alloc_pages(GFP_KERNEL, get_order(page_num));
		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i)
			sg_set_page(sg, page + i, PAGE_SIZE, 0);
		m4u_map_sgtable(domain, mva,
			sg_table, page_num * PAGE_SIZE,
			M4U_PROT_WRITE | M4U_PROT_READ);
		m4u_dump_pgtable(domain, NULL);
		m4u_unmap(domain, mva, page_num * PAGE_SIZE);
		m4u_dump_pgtable(domain, NULL);

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
		unsigned int page_size = SZ_64K;
		unsigned int mva = SZ_64K;

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
		}

		m4u_map_sgtable(domain, mva, sg_table,
			page_num * page_size, M4U_PROT_WRITE | M4U_PROT_READ);
		m4u_dump_pgtable(domain, NULL);
		m4u_unmap(domain, mva, page_num * page_size);
		m4u_dump_pgtable(domain, NULL);
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
		unsigned int page_size = SZ_1M;
		unsigned int mva = SZ_1M;

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);

		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
		}
		m4u_map_sgtable(domain, mva, sg_table,
			page_num * page_size, M4U_PROT_WRITE | M4U_PROT_READ);
		m4u_dump_pgtable(domain, NULL);
		m4u_unmap(domain, mva, page_num * page_size);
		m4u_dump_pgtable(domain, NULL);

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
		unsigned int page_size = SZ_16M;
		unsigned int mva = SZ_16M;

		sg_alloc_table(sg_table, page_num, GFP_KERNEL);
		for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
			sg_dma_address(sg) = page_size * (i + 1);
			sg_dma_len(sg) = page_size;
		}
		m4u_map_sgtable(domain, mva, sg_table,
			page_num * page_size, M4U_PROT_WRITE | M4U_PROT_READ);
		m4u_dump_pgtable(domain, NULL);
		m4u_unmap(domain, mva, page_num * page_size);
		m4u_dump_pgtable(domain, NULL);
		sg_free_table(sg_table);
		}
		break;
	case 5:
	{                   /* mapmiscpages */
		struct sg_table table;
		struct sg_table *sg_table = &table;
		struct scatterlist *sg;
		unsigned int mva = 0x4000;
		unsigned int size = SZ_16M * 2;

		sg_alloc_table(sg_table, 1, GFP_KERNEL);
		sg = sg_table->sgl;
		sg_dma_address(sg) = 0x4000;
		sg_dma_len(sg) = size;

		m4u_map_sgtable(domain, mva,
			sg_table, size, M4U_PROT_WRITE | M4U_PROT_READ);
		m4u_dump_pgtable(domain, NULL);
		m4u_unmap(domain, mva, size);
		m4u_dump_pgtable(domain, NULL);
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
	{
		m4u_test_reclaim(SZ_16K);
		m4u_mvaGraph_dump();
	}
	break;
	case 10:
	{
		unsigned int mva;

		mva = m4u_do_mva_alloc_fix(0, 0x90000000, 0x10000000, NULL);
		M4UINFO("mva alloc fix done:mva=0x%x\n", mva);
		mva = m4u_do_mva_alloc_fix(0, 0xb0000000, 0x10000000, NULL);
		M4UINFO("mva alloc fix done:mva=0x%x\n", mva);
		mva = m4u_do_mva_alloc_fix(0, 0xa0000000, 0x10000000, NULL);
		M4UINFO("mva alloc fix done:mva=0x%x\n", mva);
		mva = m4u_do_mva_alloc_fix(0, 0xa4000000, 0x10000000, NULL);
		M4UINFO("mva alloc fix done:mva=0x%x\n", mva);
		m4u_mvaGraph_dump();
		m4u_do_mva_free(0x90000000, 0x10000000);
		m4u_do_mva_free(0xa0000000, 0x10000000);
		m4u_do_mva_free(0xb0000000, 0x10000000);
		m4u_mvaGraph_dump();
	}
	break;
	case 11:    /* map unmap kernel */
		m4u_test_map_kernel();
		break;
	case 12:
		/* ddp_mem_test(); */
		break;
	case 13:
	    m4u_test_ddp(M4U_PROT_READ|M4U_PROT_WRITE);
		break;
	case 14:
		m4u_test_tf(M4U_PROT_READ|M4U_PROT_WRITE);
		break;
	case 15:
	    m4u_test_ion();
		break;
	case 16:
		m4u_dump_main_tlb(0, 0);
		break;
	case 17:
		m4u_dump_pfh_tlb(0);
		break;
	case 18:
		if (TOTAL_M4U_NUM == 2) {
			m4u_dump_main_tlb(1, 0);
			break;
		}
		break;
	case 19:
		if (TOTAL_M4U_NUM == 2) {
			m4u_dump_pfh_tlb(1);
			break;
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
		int i;
		unsigned int *pSrc;

		pSrc = vmalloc(128);
		if (!pSrc) {
			M4UMSG("vmalloc failed!\n");
			return 0;
		}
		memset(pSrc, 55, 128);
		m4u_cache_sync(NULL, 0, 0, 0, 0, M4U_CACHE_FLUSH_ALL);

		for (i = 0; i < 128 / 32; i += 32) {
			M4UMSG(
				"+0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			       8 * i, pSrc[i], pSrc[i + 1], pSrc[i + 2],
			       pSrc[i + 3], pSrc[i + 4],
			       pSrc[i + 5], pSrc[i + 6], pSrc[i + 7]);
		}
		vfree(pSrc);
	}
	break;
	case 23:
	{
		void *pgd_va;
		void *pgd_pa;
		unsigned int size;

		m4u_get_pgd(NULL, 0, &pgd_va, &pgd_pa, &size);
		M4UMSG("pgd_va:0x%p pgd_pa:0x%p, size: %d\n",
			pgd_va, pgd_pa, size);
	}
	break;
	case 24:
	{
		unsigned int *pSrc;
		unsigned int mva;
		unsigned long pa;
		struct m4u_client_t *client = m4u_create_client();

		pSrc = vmalloc(128);
		m4u_alloc_mva(client, M4U_PORT_DISP_OVL0,
			(unsigned long)pSrc, NULL, 128, 0, 0, &mva);

		m4u_dump_pgtable(domain, NULL);

		pa = m4u_mva_to_pa(NULL, 0, mva);
		M4UMSG("(1) mva:0x%x pa:0x%lx\n", mva, pa);
			m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, mva);
		pa = m4u_mva_to_pa(NULL, 0, mva);
		M4UMSG("(2) mva:0x%x pa:0x%lx\n", mva, pa);
		m4u_destroy_client(client);
	}
	break;
	case 25:
		m4u_monitor_start(0);
		break;
	case 26:
		m4u_monitor_stop(0);
		break;
	case 27:
		/*m4u_dump_reg_for_smi_hang_issue();*/
		break;
	case 28:
	{
#if 0
		unsigned char *pSrc;
		unsigned char *pDst;
		unsigned int mva_rd;
		unsigned int mva_wr;
		unsigned int allocated_size = 1024;
		unsigned int i;
		struct m4u_client_t *client = m4u_create_client();

		m4u_monitor_start(0);

		pSrc = vmalloc(allocated_size);
		memset(pSrc, 0xFF, allocated_size);
		M4UMSG("(0) vmalloc pSrc:0x%p\n", pSrc);
		pDst =  vmalloc(allocated_size);
		memset(pDst, 0xFF, allocated_size);
		M4UMSG("(0) vmalloc pDst:0x%p\n", pDst);
		M4UMSG("(1) pDst check 0x%x 0x%x 0x%x 0x%x  0x%x\n",
			*pDst, *(pDst+1),
			*(pDst+126), *(pDst+127), *(pDst+128));


		m4u_alloc_mva(client, M4U_PORT_DISP_FAKE_LARB0,
			(unsigned long)pSrc,
			NULL, allocated_size, 0, 0, &mva_rd);
		m4u_alloc_mva(client, M4U_PORT_DISP_FAKE_LARB0,
			(unsigned long)pDst,
			NULL, allocated_size, 0, 0, &mva_wr);

		m4u_dump_pgtable(domain, NULL);

		m4u_display_fake_engine_test(mva_rd, mva_wr);

		M4UMSG("(2) mva_wr:0x%x\n", mva_wr);

		m4u_dealloc_mva(client, M4U_PORT_DISP_FAKE_LARB0, mva_rd);
		m4u_dealloc_mva(client, M4U_PORT_DISP_FAKE_LARB0, mva_wr);

		m4u_cache_sync(NULL, 0, 0, 0, 0, M4U_CACHE_FLUSH_ALL);

		m4u_destroy_client(client);

		M4UMSG("(3) pDst check 0x%x 0x%x 0x%x 0x%x  0x%x\n",
			*pDst, *(pDst+1),
			*(pDst+126), *(pDst+127), *(pDst+128));

		vfree(pSrc);
		vfree(pDst);

		m4u_monitor_stop(0);
#endif
		break;
	}

	/*debug mva allocation for porting vpu in m4u 2.4 from case 30 to 33.*/
	case 30:
	{
		M4UMSG("start to test m4u 2.4\n");
		test_case_check_mva_region();
		break;
	}

	case 31:
	{
		M4UMSG("start to test m4u 2.4\n");
		test_case_m4u_do_mva_alloc();
		break;
	}

	case 32:
	{
		M4UMSG("start to test m4u 2.4\n");
		test_case_m4u_do_mva_alloc_fix();
		break;
	}

	case 33:
	{
		M4UMSG("start to test m4u 2.4\n");
		test_case_m4u_do_mva_alloc_start_from();
		break;
	}

	case 34:
	{
		M4UMSG("start to test m4u 2.4\n");
		test_case_m4u_do_mva_free();
		break;
	}

	case 35:
	{
		M4UMSG("start to test m4u 2.4\n");
		test_m4u_do_mva_alloc_stage3();
		break;
	}

	case 36:
	{
		M4UMSG("start to test m4u 2.4\n");
		test_m4u_do_mva_alloc_start_from_V2p4();
		break;
	}
	case 37:
	{
		M4UMSG("start to test m4u 2.4\n");
		test_m4u_do_mva_alloc_start_from_V2p4_case1();
		break;
	}

	/*debug pagetable corruption*/
	case 38:
	{
		int ret;
		unsigned int mva;
		int size;
		struct m4u_client_t *client;
		unsigned long va;

		size = 0x500000;
		va = (unsigned long)vmalloc(size);
		client = m4u_create_client();
		if (IS_ERR_OR_NULL(client))
			M4UMSG("create client fail!\n");
		ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, va, NULL, size,
				    M4U_PROT_READ | M4U_PROT_CACHE, 0, &mva);
		if (ret) {
			M4UMSG(
				"alloc mva fail: va=0x%lx size=0x%x,ret=%d\n",
				va, size, ret);
			return -1;
		}
		m4u_dump_pgtable_for_debug(mva, size);
		ret = m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, mva);
		/* clean */
		m4u_destroy_client(client);
		vfree((void *)va);
		break;
	}

#ifdef M4U_TEE_SERVICE_ENABLE
	case 50:
	{
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT) &&	\
		defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)
		u32 sec_handle = 0;
		u32 refcount;

		trusted_mem_api_alloc(TRUSTED_MEM_REQ_SVP, 0, 0x1000, &refcount,
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

		memset(&port, 0,
			sizeof(struct M4U_PORT_STRUCT));

		port.ePortID = M4U_PORT_HW_VDEC_PP_EXT;
		port.Virtuality = 1;
		M4UMSG("(0) config port: mmu: %d, sec: %d\n",
			port.Virtuality, port.Security);
		m4u_config_port(&port);
		/* port.ePortID = M4U_PORT_MDP_WROT1;*/
		/*m4u_config_port(&port); */
		/* port.ePortID = M4U_PORT_IMGO;*/
		/*m4u_config_port(&port); */
		port.ePortID = M4U_PORT_VENC_RCPU;
		m4u_config_port(&port);
		/* port.ePortID = M4U_PORT_MJC_MV_RD;*/
		/*m4u_config_port(&port); */

		port.ePortID = M4U_PORT_HW_VDEC_PP_EXT;
		M4UMSG("(1) config port: mmu: %d, sec: %d\n",
			port.Virtuality, port.Security);
		m4u_config_port_tee(&port);
		port.Security = 1;
		M4UMSG("(2) config port: mmu: %d, sec: %d\n",
			port.Virtuality, port.Security);
		m4u_config_port_tee(&port);
	}
	break;
#endif
	default:
		M4UMSG("%s error,val=%llu\n", __func__, val);
	}

	return 0;
}

static int m4u_debug_get(void *data, u64 *val)
{
	*val = 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_fops, m4u_debug_get, m4u_debug_set, "%llu\n");

#if (M4U_DVT != 0)
static void m4u_test_init(void)
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

	m4u_invalid_tlb_all(0);
	m4u_confirm_all_invalidated(0);

	m4u_monitor_stop(0);
}

static void m4u_test_start(void)
{
	struct M4U_PORT_STRUCT rM4uPort;
	int i;

	m4u_monitor_start(0);

	rM4uPort.Virtuality = 1;
	rM4uPort.Security = 0;
	rM4uPort.Distance = 1;
	rM4uPort.Direction = 0;
	rM4uPort.domain = 3;
	for (i = 0; i < M4U_PORT_UNKNOWN; i++) {
		rM4uPort.ePortID = i;
		m4u_config_port(&rM4uPort);
	}

	for (i = 0; i < 100; i++)
		M4UMSG("test %d !!!\n", i);
}

static void m4u_test_end(int invalid_tlb)
{
	struct M4U_PORT_STRUCT rM4uPort;
	int i;

	m4u_monitor_stop(0);

	rM4uPort.Virtuality = 0;
	rM4uPort.Security = 0;
	rM4uPort.Distance = 1;
	rM4uPort.Direction = 0;
	rM4uPort.domain = 3;
	for (i = 0; i < M4U_PORT_UNKNOWN; i++) {
		rM4uPort.ePortID = i;
		m4u_config_port(&rM4uPort);
	}

	if (invalid_tlb == 1) {
		m4u_invalid_tlb_all(0);
		m4u_confirm_all_invalidated(0);
	}
}
#endif

#if (M4U_DVT != 0)
static int __vCatchTranslationFault(struct m4u_domain *domain,
		unsigned int layer,
				    unsigned int seed_mva)
{
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	unsigned int backup;
	unsigned int *backup_ptr;
	int count;

	int pt_type = m4u_get_pt_type(domain, seed_mva);

	M4UMSG(
		"%s, layer = %d, seed_mva = 0x%x.\n",
			__func__, layer, seed_mva);

	if (seed_mva == 0) {
		M4UMSG("seed_mva = 0 !!!!!!!!!!!\n");
		return 0;
	}

	pgd = imu_pgd_offset(domain, seed_mva);
	if (layer == 0) {
		int i = 0;

		backup = imu_pgd_val(*pgd);
		backup_ptr = (unsigned int *)pgd;
		if (pt_type == MMU_PT_TYPE_SUPERSECTION) {
			for (i = 0; i < 16; i++)
				imu_pgd_val(*(pgd + i)) = 0x0;
		} else {
			imu_pgd_val(*pgd) = 0x0;
		}
	} else {
		int i = 0;

		pte = imu_pte_offset_map(pgd, seed_mva);
		backup = imu_pte_val(*pte);
		backup_ptr = (unsigned int *)pte;
		if (pt_type == MMU_PT_TYPE_LARGE_PAGE) {
			for (i = 0; i < 16; i++)
				imu_pte_val(*(pte + i)) = 0x0;
		} else {
			imu_pte_val(*pte) = 0x0;
		}
	}

	for (count = 0; count < 100; count++)
		M4UMSG("test %d ......\n", count);

	/* restore */
	*backup_ptr = backup;

	return 0;
}

static int __vCatchInvalidPhyFault(struct m4u_domain *domain,
	int g4_mode, unsigned int seed_mva)
{
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	unsigned int backup;
	unsigned int fault_pa;
	int count;

	if (seed_mva == 0) {
		M4UMSG("seed_mva = 0 !!!!!!!!!!!\n");
		return 0;
	}

	pgd = imu_pgd_offset(domain, seed_mva);
#if (M4U_DVT == MMU_PT_TYPE_SMALL_PAGE || M4U_DVT == MMU_PT_TYPE_LARGE_PAGE)
	pte = imu_pte_offset_map(pgd, seed_mva);
	backup = imu_pte_val(*pte);
	if (!g4_mode) {
		imu_pte_val(*pte) = 0x2;
		fault_pa = 0;
	} else {
		imu_pte_val(*pte) = 0x10000002;
		fault_pa = 0x10000000;
	}
#else
	backup = imu_pgd_val(*pgd);
	if (!g4_mode) {
		imu_pgd_val(*pgd) = 0x2;
		fault_pa = 0;
	} else {
		imu_pgd_val(*pgd) = 0x10000002;
		fault_pa = 0x10000000;
	}
#endif
	M4UMSG("fault_pa (%d): 0x%x\n", g4_mode, fault_pa);

	for (count = 0; count < 100; count++)
		M4UMSG("test %d ......\n", count);


	/* restore */
#if (M4U_DVT == MMU_PT_TYPE_SMALL_PAGE || M4U_DVT == MMU_PT_TYPE_LARGE_PAGE)
	imu_pte_val(*pte) = backup;
#else
	imu_pgd_val(*pgd) = backup;
#endif
	return 0;
}

#endif

#if (M4U_DVT != 0)
static int m4u_test_set(void *data, u64 val)
{
	struct m4u_domain *domain = data;

	M4UMSG("%s:val=%llu\n", __func__, val);

	switch (val) {
	case 1:
		M4UMSG(
			"---------- 1. MMU translation with main TLB only. ---------- Start!\n");
		m4u_test_init();
		m4u_enable_prefetch(0, 0);
		m4u_test_start();
		m4u_test_end(1);
		m4u_enable_prefetch(0, 1);
		M4UMSG(
			"---------- 1. MMU translation with main TLB only. ---------- End!\n");
		break;

	case 2:
		M4UMSG(
			"---------- 2. MMU translation with both main TLB and pre-fetch TLB. ---------- Start!\n");
		m4u_test_init();
		m4u_enable_prefetch(0, 1);
		m4u_test_start();
		m4u_test_end(1);
		M4UMSG(
			"---------- 2. MMU translation with both main TLB and pre-fetch TLB. ---------- End!\n");
		break;

	case 3:
		M4UMSG(
			"---------- 3. Range invalidate TLBs static test. ---------- Start!\n");
		m4u_test_init();
		m4u_test_start();
		m4u_test_end(0);
		m4u_dump_valid_main_tlb(0, 0);
		m4u_invalid_tlb_by_range(domain,
			gM4U_seed_mva, gM4U_seed_mva + 0x1000000);
		m4u_confirm_range_invalidated(0,
			gM4U_seed_mva, gM4U_seed_mva + 0x1000000);
		m4u_dump_valid_main_tlb(0, 0);
		M4UMSG(
			"---------- 3. Range invalidate TLBs static test. ---------- End!\n");
		break;

	case 4:
		{
			int i;

			M4UMSG(
				"---------- 4. Range invalidate TLBs dynamic test. ---------- Start!\n");
			m4u_test_init();
			m4u_test_start();
			for (i = 0; i < 100; i++)
				m4u_invalid_tlb_by_range(domain, gM4U_seed_mva,
					gM4U_seed_mva + 0x1000000);
			m4u_test_end(1);
			M4UMSG(
				"---------- 4. Range invalidate TLBs dynamic test. ---------- End!\n");
		}
		break;

	case 5:
		M4UMSG(
			"---------- 5. Invalidate all TLBs static test. ---------- Start!\n");
		m4u_test_init();
		m4u_test_start();
		m4u_test_end(0);
		m4u_dump_valid_main_tlb(0, 0);
		m4u_invalid_tlb_all(0);
		m4u_confirm_all_invalidated(0);
		m4u_dump_valid_main_tlb(0, 0);
		M4UMSG(
			"---------- 5. Invalidate all TLBs static test. ---------- End!\n");
		break;

	case 6:
		{
			int i;

			M4UMSG(
				"---------- 6. Invalidate all TLBs dynamic test. ---------- Start!\n");
			m4u_test_init();
			m4u_test_start();
			for (i = 0; i < 100; i++)
				m4u_invalid_tlb_all(0);
			m4u_test_end(1);
			m4u_dump_valid_main_tlb(0, 0);
			M4UMSG(
				"---------- 6. Invalidate all TLBs dynamic test. ---------- End!\n");
		}
		break;

	case 8:
		M4UMSG(
			"---------- 8. SW manual mode to program main TLB. ---------- Start!\n");
		m4u_test_init();
		m4u_dump_main_tlb(0, 0);
#if (M4U_DVT == MMU_PT_TYPE_LARGE_PAGE || M4U_DVT == MMU_PT_TYPE_SMALL_PAGE)
		m4u_manual_insert_entry(0, gM4U_seed_mva,
			1, M4U_DVT, 0, 0, gM4U_seed_mva);
		m4u_dump_valid_main_tlb(0, 0);
#else
		m4u_manual_insert_entry(0, gM4U_seed_mva,
			0, M4U_DVT, 0, 0, gM4U_seed_mva);
		m4u_dump_valid_main_tlb(0, 0);
#endif
		m4u_test_start();
		m4u_test_end(1);
		M4UMSG(
			"---------- 8. SW manual mode to program main TLB. ---------- End!\n");
		break;

	case 9:
		M4UMSG(
			"---------- 9. Main TLB lock mode. ---------- Start!\n");
		m4u_test_init();
#if (M4U_DVT == MMU_PT_TYPE_LARGE_PAGE || M4U_DVT == MMU_PT_TYPE_SMALL_PAGE)
		m4u_manual_insert_entry(0,
			gM4U_seed_mva, 1, M4U_DVT, 0, 1, gM4U_seed_mva);
#else
		m4u_manual_insert_entry(0,
			gM4U_seed_mva, 0, M4U_DVT, 0, 1, gM4U_seed_mva);
#endif
		m4u_dump_valid_main_tlb(0, 0);

		m4u_test_start();
		m4u_dump_valid_main_tlb(0, 0);
		m4u_test_end(1);
		m4u_dump_valid_main_tlb(0, 0);

		M4UMSG(
			"---------- 9. Main TLB lock mode. ---------- End!\n");
		break;

	case 10:
		{
			int i, j;
			int seq_id;

			M4UMSG(
				"---------- 10. Sequential range feature. ---------- Start!\n");

			seq_id = m4u_insert_seq_range(0,
				gM4U_seed_mva, gM4U_seed_mva + 0x1000000);
			m4u_test_init();
			m4u_dump_valid_main_tlb(0, 0);
			m4u_test_start();
			m4u_test_end(0);
			m4u_dump_valid_main_tlb(0, 0);
			if (seq_id >= 0)
				m4u_invalid_seq_range_by_id(0, seq_id);
			M4UMSG(
				"---------- 10. Sequential range feature. ---------- End!\n");
		}
		break;

	case 11:
		{
			int i;

			M4UMSG(
				"---------- 11. Single entry test. ---------- Start!\n");
			m4u_test_init();
			m4u_enable_MTLB_allshare(0, 1);
			for (i = 0; i < 31; i++)
#if (M4U_DVT == MMU_PT_TYPE_LARGE_PAGE || M4U_DVT == MMU_PT_TYPE_SMALL_PAGE)
				m4u_manual_insert_entry(0,
					gM4U_seed_mva + i * 4096, 1, M4U_DVT, 0,
					1, gM4U_seed_mva + i * 4096);
#endif
#if (M4U_DVT == MMU_PT_TYPE_SECTION)
			m4u_manual_insert_entry(0,
				gM4U_seed_mva + i * 4096, 0, M4U_DVT, 0, 1,
				gM4U_seed_mva + i * 4096);
#endif
#if (M4U_DVT == MMU_PT_TYPE_SUPERSECTION)
			m4u_manual_insert_entry(0,
				i * 4096, 1, MMU_PT_TYPE_SMALL_PAGE, 0, 1,
				i * 4096);
#endif

			m4u_dump_valid_main_tlb(0, 0);
			m4u_test_start();
			m4u_dump_valid_main_tlb(0, 0);
			m4u_test_end(1);
			m4u_enable_MTLB_allshare(0, 0);

			M4UMSG(
				"---------- 11. Single entry test. ---------- End!\n");
		}
		break;


	case 13:
		{
			int count;

			M4UMSG(
				"---------- 13. MMU performance counter. ---------- Start!\n");
			m4u_test_init();
			m4u_test_start();
			for (count = 0; count < 100; count++)
				M4UMSG("test %d ......\n", count);
			m4u_test_end(1);

			M4UMSG(
				"---------- 13. MMU performance counter. ---------- End!\n");
		}
		break;


	case 14:
		{
			int i;
			int count;

			M4UMSG(
				"---------- 14. Entry number versus performance evaluation. ---------- Start!\n");

			m4u_test_init();
			m4u_enable_MTLB_allshare(0, 1);
			for (i = 0; i < 30; i++)
				m4u_manual_insert_entry(0, i * 4096,
					1, MMU_PT_TYPE_SMALL_PAGE, 0,
							1, i * 4096);

			m4u_dump_valid_main_tlb(0, 0);
			m4u_test_start();
			for (count = 0; count < 100; count++)
				M4UMSG("test %d ......\n", count);
			m4u_test_end(1);

			m4u_enable_MTLB_allshare(0, 0);

			M4UMSG(
				"---------- 14. Entry number versus performance evaluation. ---------- End!\n");
		}
		break;



	case 15:
		{
			M4UMSG(
				"---------- 15. Translation fault. ---------- Start!\n");
			m4u_test_init();
			m4u_test_start();
			__vCatchTranslationFault(domain, 0, gM4U_seed_mva);
			m4u_test_end(1);
#if (M4U_DVT == MMU_PT_TYPE_LARGE_PAGE || M4U_DVT == MMU_PT_TYPE_SMALL_PAGE)
			m4u_test_init();
			m4u_test_start();
			__vCatchTranslationFault(domain, 1, gM4U_seed_mva);
			m4u_test_end(1);
#endif
			M4UMSG(
				"---------- 15. Translation fault. ---------- End!\n");
		}
		break;

	case 16:
		M4UMSG(
			"---------- 16. TLB multi-hit fault. ---------- Start!\n");
		m4u_test_init();
#if (M4U_DVT == MMU_PT_TYPE_LARGE_PAGE || M4U_DVT == MMU_PT_TYPE_SMALL_PAGE)
		m4u_manual_insert_entry(0,
			gM4U_seed_mva, 1, M4U_DVT, 0, 0, gM4U_seed_mva);
#else
		m4u_manual_insert_entry(0,
			gM4U_seed_mva, 0, M4U_DVT, 0, 0, gM4U_seed_mva);
#endif
		M4UMSG("valid main tlb 1\n");
		m4u_dump_valid_main_tlb(0, 0);
#if (M4U_DVT == MMU_PT_TYPE_LARGE_PAGE || M4U_DVT == MMU_PT_TYPE_SMALL_PAGE)
		m4u_manual_insert_entry(0,
			gM4U_seed_mva, 1, M4U_DVT, 0, 0, gM4U_seed_mva);
#else
		m4u_manual_insert_entry(0,
			gM4U_seed_mva, 0, M4U_DVT, 0, 0, gM4U_seed_mva);
#endif

		M4UMSG("valid main tlb 2\n");
		m4u_dump_valid_main_tlb(0, 0);
		m4u_test_start();
		M4UMSG("valid main tlb 3\n");
		m4u_dump_main_tlb(0, 0);
		m4u_dump_valid_main_tlb(0, 0);
		m4u_test_end(1);
		M4UMSG(
			"---------- 16. TLB multi-hit fault. ---------- End!\n");
		break;

	case 17:
		{
			int i;

			M4UMSG(
				"---------- 17. Entry replacement fault. ---------- Start!\n");
			m4u_enable_MTLB_allshare(0, 1);
			m4u_test_init();
			for (i = 0; i < 32; i++)
#if (M4U_DVT == MMU_PT_TYPE_LARGE_PAGE || M4U_DVT == MMU_PT_TYPE_SMALL_PAGE)
				m4u_manual_insert_entry(0,
					gM4U_seed_mva + i * 4096, 1, M4U_DVT, 0,
					1, gM4U_seed_mva + i * 4096);
#endif
#if (M4U_DVT == MMU_PT_TYPE_SECTION)
			m4u_manual_insert_entry(0,
				gM4U_seed_mva + i * 4096, 0, M4U_DVT, 0, 1,
						gM4U_seed_mva + i * 4096);
#endif
#if (M4U_DVT == MMU_PT_TYPE_SUPERSECTION)
			m4u_manual_insert_entry(0, i * 4096,
				1, MMU_PT_TYPE_SMALL_PAGE, 0, 1,
						i * 4096);
#endif

			m4u_dump_valid_main_tlb(0, 0);
			m4u_test_start();
			m4u_test_end(1);
			m4u_enable_MTLB_allshare(0, 0);
			M4UMSG(
				"---------- 17. Entry replacement fault. ---------- End!\n");
		}
		break;

	case 18:
		M4UMSG(
			"---------- 18. Invalid physical address fault. ---------- Start!\n");
		m4u_test_init();
		m4u_test_start();
		__vCatchInvalidPhyFault(domain, 0, gM4U_seed_mva);
		m4u_test_end(1);
		m4u_test_init();
		m4u_test_start();
		__vCatchInvalidPhyFault(domain, 1, gM4U_seed_mva);
		m4u_test_end(1);

		M4UMSG(
			"---------- 18. Invalid physical address fault. ---------- End!\n");
		break;

	case 20:
		{
			int i;
			void *protectva = (void *)gM4U_ProtectVA;

			M4UMSG(
				"---------- 20. Translation fault Protection. ---------- Start!\n");
			memset(protectva, 0x55, 128);
			m4u_test_init();
			m4u_test_start();
			__vCatchTranslationFault(domain, 0, gM4U_seed_mva);
			m4u_test_end(1);

			M4UMSG(
				"---------- 20. Translation fault Protection. ---------- End!\n");
		}
		break;

	case 21:
		M4UMSG(
			"---------- 21. MMU interrupt hang function. ---------- Start!\n");
		m4u_enable_error_hang(0, 1);
		m4u_test_init();
		m4u_test_start();
		__vCatchTranslationFault(domain, 0, gM4U_seed_mva);
		m4u_test_end(1);
		m4u_enable_error_hang(0, 0);

		M4UMSG(
			"---------- 21. MMU interrupt hang function. ---------- End!\n");
		break;

	case 22:
		{
			int i;

			M4UMSG(
				"---------- 22. Physical MAU assert test(traffic after MMU). ---------- Start!\n");
			m4u_test_init();
			m4u_test_start();
			for (i = 0; i < 4; i++)
				mau_start_monitor(0, 0, i, 0,
				0, 0, 0, gM4U_seed_mva + i * 0x100000,
							gM4U_seed_mva +
							(i + 1) * 0x100000 - 1,
						  0xffffffff, 0xffffffff);
			m4u_test_end(1);

			M4UMSG(
				"---------- 22. MMU interrupt hang function. ---------- End!\n");
		}
		break;

	case 23:
		{
			int i;

			M4UMSG(
				"---------- 23. Virtual MPU assert test(traffic before MMU). ---------- Start!\n");
			m4u_test_init();
			m4u_test_start();
			for (i = 0; i < 4; i++)
				mau_start_monitor(0, 0, i, 0,
					1, 0, 0,
					gM4U_seed_mva + i * 0x100000,
						  gM4U_seed_mva +
						  (i + 1) * 0x100000 - 1,
						  0xffffffff, 0xffffffff);
			m4u_test_end(1);

			M4UMSG(
				"---------- 23. Virtual MPU assert test. ---------- End!\n");
		}
		break;

	case 29:
		M4UMSG(
			"---------- 29. Legacy 4KB-only mode test. ---------- Start!\n");

		M4UMSG(
			"---------- 29. Legacy 4KB-only mode test. ---------- End!\n");
		break;

	default:
		M4UMSG("%s error,val=%llu\n", __func__, val);
	}

	return 0;
}

static int m4u_test_get(void *data, u64 *val)
{
	gM4U_seed_mva = get_first_valid_mva() + 0x200000;

	*val = gM4U_seed_mva;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(m4u_test_fops, m4u_test_get, m4u_test_set, "%llu\n");
#endif

static int m4u_log_level_set(void *data, u64 val)
{
	gM4U_log_to_uart = (val & 0xf0) >> 4;
	gM4U_log_level = val & 0xf;
	M4UMSG("gM4U_log_level: %d, gM4U_log_to_uart:%d\n",
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

static int m4u_debug_freemva_set(void *data, u64 val)
{
	struct m4u_domain *domain = data;
	struct m4u_buf_info_t *pMvaInfo;
	unsigned int mva = (unsigned int)val;

	M4UMSG("free mva: 0x%x\n", mva);
	pMvaInfo = mva_get_priv(mva);
	if (pMvaInfo) {
		m4u_unmap(domain, mva, pMvaInfo->size);
		m4u_do_mva_free(mva, pMvaInfo->size);
	}
	return 0;
}

static int m4u_debug_freemva_get(void *data, u64 *val)
{
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(m4u_debug_freemva_fops,
	m4u_debug_freemva_get, m4u_debug_freemva_set, "%llu\n");

int m4u_debug_port_show(struct seq_file *s, void *unused)
{
	m4u_print_port_status(s, 0);
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

int m4u_debug_mva_show(struct seq_file *s, void *unused)
{
	m4u_mvaGraph_dump();
	return 0;
}

int m4u_debug_mva_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_mva_show, inode->i_private);
}

const struct file_operations m4u_debug_mva_fops = {
	.open = m4u_debug_mva_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int m4u_debug_buf_show(struct seq_file *s, void *unused)
{
	m4u_dump_buf_info(s);
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
	m4u_print_perf_counter(0, 0, "monitor");
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
	m4u_dump_reg(0, 0, 400);
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

int m4u_debug_init(struct m4u_device *m4u_dev)
{
	struct dentry *debug_file;
	struct m4u_domain *domain = m4u_get_domain_by_id(0);

	m4u_dev->debug_root = debugfs_create_dir("m4u", NULL);

	if (IS_ERR_OR_NULL(m4u_dev->debug_root))
		M4UMSG("m4u: failed to create debug dir.\n");

	debug_file = debugfs_create_file("buffer", 0644,
		m4u_dev->debug_root, domain, &m4u_debug_buf_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 1.\n");

	debug_file = debugfs_create_file("debug", 0644,
		m4u_dev->debug_root, domain, &m4u_debug_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 2.\n");

#if (M4U_DVT != 0)
	debug_file = debugfs_create_file("test", 0644,
		m4u_dev->debug_root, domain, &m4u_test_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 3.\n");
#endif

	debug_file = debugfs_create_file("port", 0644,
		m4u_dev->debug_root, domain, &m4u_debug_port_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 4.\n");

	debug_file = debugfs_create_file("log_level", 0644,
		m4u_dev->debug_root, domain, &m4u_log_level_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 5.\n");

	debug_file = debugfs_create_file("monitor", 0644,
		m4u_dev->debug_root, domain, &m4u_debug_monitor_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 6.\n");

	debug_file = debugfs_create_file("register", 0644,
		m4u_dev->debug_root, domain, &m4u_debug_register_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 7.\n");

	debug_file = debugfs_create_file("freemva", 0644,
		m4u_dev->debug_root, domain, &m4u_debug_freemva_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 8.\n");

	debug_file = debugfs_create_file("mva", 0644,
		m4u_dev->debug_root, domain, &m4u_debug_mva_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 9.\n");


	return 0;
}

void m4u_dump_pgtable_for_debug(unsigned int mva, unsigned int mva_size)
{
	m4u_dump_pgtable_in_range(m4u_get_domain_by_id(0), mva, mva_size);
}
