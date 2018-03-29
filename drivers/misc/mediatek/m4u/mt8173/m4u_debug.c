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

#include "m4u_priv.h"
#include "m4u_debug.h"

/* global variables */
int gM4U_log_to_uart = 2;
int gM4U_log_level = 2;


int m4u_test_alloc_dealloc(int id, unsigned int size)
{
	m4u_client_t *client;
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
		va = do_mmap_pgoff(NULL, 0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED,
				   0, &populate);
		up_write(&current->mm->mmap_sem);
	}

	M4UINFO("test va=0x%lx,size=0x%x\n", va, size);

	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client))
		M4UMSG("create client fail!\n");

	ret = m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, va, NULL, size,
			    M4U_PROT_READ | M4U_PROT_CACHE, 0, &mva);
	if (ret) {
		M4UMSG("alloc mva fail:va=0x%lx,size=0x%x,ret=%d\n", va, size, ret);
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
		ret = do_munmap(current->mm, va, size);
		up_read(&current->mm->mmap_sem);
		if (ret)
			M4UMSG("do_munmap failed\n");
	}
/* clean */
	m4u_destroy_client(client);
	return 0;
}


m4u_callback_ret_t m4u_test_callback(int alloc_port, unsigned int mva,
				     unsigned int size, void *data)
{
	if (NULL != data)
		M4UMSG("test callback port=%d, mva=0x%x, size=0x%x, data=0x%x\n", alloc_port, mva,
		       size, *(int *)data);
	else
		M4UMSG("test callback port=%d, mva=0x%x, size=0x%x\n", alloc_port, mva, size);

	return M4U_CALLBACK_HANDLED;
}


int m4u_test_reclaim(unsigned int size)
{
	m4u_client_t *client;
	unsigned int *va[10];
	unsigned int buf_size;
	unsigned int mva;
	int ret, i;

	/* register callback */
	m4u_register_reclaim_callback(M4U_PORT_DISP_OVL0, m4u_test_callback, NULL);


	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client))
		M4UMSG("createclientfail!\n");

	buf_size = size;
	for (i = 0; i < 10; i++) {
		va[i] = vmalloc(buf_size);

		ret =
		    m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned long)va[i], NULL, buf_size,
				  M4U_PROT_READ | M4U_PROT_CACHE, 0, &mva);
		if (ret) {
			M4UMSG("alloc using kmalloc fail:va=0x%p,size=0x%x\n", va[i], buf_size);
			return -1;
		}
		M4UINFO("alloc mva:va=0x%p,mva=0x%x,size=0x%x\n", va[i], mva, buf_size);

		buf_size += size;
	}


	for (i = 0; i < 10; i++)
		vfree((void *)va[i]);

	m4u_dump_buf_info(NULL);
	m4u_dump_pgtable(m4u_get_domain_by_port(M4U_PORT_DISP_OVL0), NULL);

	m4u_destroy_client(client);

	m4u_unregister_reclaim_callback(M4U_PORT_DISP_OVL0);
	m4u_unregister_reclaim_callback(M4U_PORT_AAO);
	/*m4u_unregister_reclaim_callback(M4U_PORT_GCE);*/

	return 0;
}

static int m4u_test_map_kernel(void)
{
	m4u_client_t *client;
	unsigned long va;
	unsigned int size = 1024 * 1024;
	unsigned int mva;
	unsigned long kernel_va;
	unsigned int kernel_size;
	int i;
	int ret;
	unsigned long populate;

	down_write(&current->mm->mmap_sem);
	va = do_mmap_pgoff(NULL, 0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, 0,
			   &populate);
	up_write(&current->mm->mmap_sem);

	M4UINFO("test va=0x%lx,size=0x%x\n", va, size);

	for (i = 0; i < size; i += 4)
		*(int *)(va + i) = i;

	client = m4u_create_client();
	if (IS_ERR_OR_NULL(client))
		M4UMSG("createclientfail!\n");

	ret =
	    m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, va, NULL, size,
			  M4U_PROT_READ | M4U_PROT_CACHE, 0, &mva);
	if (ret) {
		M4UMSG("alloc using kmalloc fail:va=0x%lx,size=0x%x\n", va, size);
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
	ret = do_munmap(current->mm, va, size);
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
	M4U_PORT_STRUCT port;
	m4u_client_t *client = m4u_create_client();

	pSrc = vmalloc(size);
	pDst = vmalloc(size);

	m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned long)pSrc, NULL,
		      size, prot, 0, &src_pa);

	m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned long)pDst, NULL,
		      size, prot, 0, &dst_pa);

	M4UINFO("pSrc=0x%p, pDst=0x%p, src_pa=0x%x, dst_pa=0x%x\n", pSrc, pDst, src_pa, dst_pa);

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

m4u_callback_ret_t test_fault_callback(int port, unsigned int mva, void *data)
{
	if (NULL != data)
		M4UMSG("fault call port=%d, mva=0x%x, data=0x%x\n", port, mva, *(int *)data);
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
	M4U_PORT_STRUCT port;
	m4u_client_t *client = m4u_create_client();
	int data = 88;


	m4u_register_fault_callback(M4U_PORT_DISP_OVL0, test_fault_callback, &data);
	m4u_register_fault_callback(M4U_PORT_DISP_WDMA0, test_fault_callback, &data);

	pSrc = vmalloc(size);
	pDst = vmalloc(size);

	m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned long)pSrc, NULL,
		      size, prot, 0, &src_pa);

	m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned long)pDst, NULL,
		      size / 2, prot, 0, &dst_pa);

	M4UINFO("pSrc=0x%p, pDst=0x%p, src_pa=0x%x, dst_pa=0x%x\n", pSrc, pDst, src_pa, dst_pa);

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

#define m4u_test_ion(...)

static int m4u_debug_set(void *data, u64 val)
{
	m4u_domain_t *domain = data;

	M4UMSG("m4u_debug_set:val=%llu\n", val);

	switch (val) {
	case 1:
		{		/* map4kpageonly */
			struct sg_table table;
			struct sg_table *sg_table = &table;
			struct scatterlist *sg;
			int i;
			struct page *page;
			int page_num = 512;
			unsigned int mva = 0x4000;

			page = alloc_pages(GFP_KERNEL, get_order(page_num));
			sg_alloc_table(sg_table, page_num, GFP_KERNEL);
			for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
				sg_set_page(sg, page + i, PAGE_SIZE, 0);
			}
			m4u_map_sgtable(domain, mva, sg_table, page_num * PAGE_SIZE,
					M4U_PROT_WRITE | M4U_PROT_READ);
			m4u_dump_pgtable(domain, NULL);
			m4u_unmap(domain, mva, page_num * PAGE_SIZE);
			m4u_dump_pgtable(domain, NULL);

			sg_free_table(sg_table);
			__free_pages(page, get_order(page_num));

		}
		break;
	case 2:
		{		/* map64kpageonly */
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

			m4u_map_sgtable(domain, mva, sg_table, page_num * page_size,
					M4U_PROT_WRITE | M4U_PROT_READ);
			m4u_dump_pgtable(domain, NULL);
			m4u_unmap(domain, mva, page_num * page_size);
			m4u_dump_pgtable(domain, NULL);
			sg_free_table(sg_table);
		}
		break;

	case 3:
		{		/* map1Mpageonly */
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
			m4u_map_sgtable(domain, mva, sg_table, page_num * page_size,
					M4U_PROT_WRITE | M4U_PROT_READ);
			m4u_dump_pgtable(domain, NULL);
			m4u_unmap(domain, mva, page_num * page_size);
			m4u_dump_pgtable(domain, NULL);

			sg_free_table(sg_table);

		}
		break;

	case 4:
		{		/* map16Mpageonly */
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
			m4u_map_sgtable(domain, mva, sg_table, page_num * page_size,
					M4U_PROT_WRITE | M4U_PROT_READ);
			m4u_dump_pgtable(domain, NULL);
			m4u_unmap(domain, mva, page_num * page_size);
			m4u_dump_pgtable(domain, NULL);
			sg_free_table(sg_table);
		}
		break;

	case 5:
		{		/* mapmiscpages */
			struct sg_table table;
			struct sg_table *sg_table = &table;
			struct scatterlist *sg;
			unsigned int mva = 0x4000;
			unsigned int size = SZ_16M * 2;

			sg_alloc_table(sg_table, 1, GFP_KERNEL);
			sg = sg_table->sgl;
			sg_dma_address(sg) = 0x4000;
			sg_dma_len(sg) = size;

			m4u_map_sgtable(domain, mva, sg_table, size,
					M4U_PROT_WRITE | M4U_PROT_READ);
			m4u_dump_pgtable(domain, NULL);
			m4u_unmap(domain, mva, size);
			m4u_dump_pgtable(domain, NULL);
			sg_free_table(sg_table);

		}
		break;

	case 6:
		{
			m4u_test_alloc_dealloc(1, SZ_4M);
		}
		break;

	case 7:
		{
			m4u_test_alloc_dealloc(2, SZ_4M);
		}
		break;

	case 8:
		{
			m4u_test_alloc_dealloc(3, SZ_4M);
		}
		break;

	case 9:		/* m4u_alloc_mvausingkmallocbuffer */
		{
			m4u_test_reclaim(SZ_16K);
			m4u_mvaGraph_dump();
		}
		break;

	case 10:
		{
			unsigned int mva;

			mva = m4u_do_mva_alloc_fix(0x90000000, 0x10000000, NULL);
			M4UINFO("mva alloc fix done:mva=0x%x\n", mva);
			mva = m4u_do_mva_alloc_fix(0xb0000000, 0x10000000, NULL);
			M4UINFO("mva alloc fix done:mva=0x%x\n", mva);
			mva = m4u_do_mva_alloc_fix(0xa0000000, 0x10000000, NULL);
			M4UINFO("mva alloc fix done:mva=0x%x\n", mva);
			mva = m4u_do_mva_alloc_fix(0xa4000000, 0x10000000, NULL);
			M4UINFO("mva alloc fix done:mva=0x%x\n", mva);
			m4u_mvaGraph_dump();
			m4u_do_mva_free(0x90000000, 0x10000000);
			m4u_do_mva_free(0xa0000000, 0x10000000);
			m4u_do_mva_free(0xb0000000, 0x10000000);
			m4u_mvaGraph_dump();
		}
		break;

	case 11:		/* map unmap kernel */
		m4u_test_map_kernel();
		break;

	case 12:
		ddp_mem_test();
		break;

	case 13:
		m4u_test_ddp(M4U_PROT_READ | M4U_PROT_WRITE);
		break;
	case 14:
		m4u_test_tf(M4U_PROT_READ | M4U_PROT_WRITE);
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
		m4u_dump_main_tlb(1, 0);
		break;
	case 19:
		m4u_dump_pfh_tlb(1);
		break;
	case 20:
		{
			M4U_PORT_STRUCT rM4uPort;
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
			M4U_PORT_STRUCT rM4uPort;
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
			memset(pSrc, 55, 128);
			m4u_cache_sync(NULL, 0, 0, 0, 0, M4U_CACHE_FLUSH_ALL);

			for (i = 0; i < 128 / 32; i += 32) {
				M4UMSG("+0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
				       8 * i, pSrc[i], pSrc[i + 1], pSrc[i + 2], pSrc[i + 3],
				       pSrc[i + 4], pSrc[i + 5], pSrc[i + 6], pSrc[i + 7]);
			}

		}
		break;
	case 23:
		{
			void *pgd_va;
			void *pgd_pa;
			unsigned size;

			m4u_get_pgd(NULL, 0, &pgd_va, &pgd_pa, &size);
			M4UMSG("pgd_va:0x%p pgd_pa:0x%p, size: %d\n", pgd_va, pgd_pa, size);
		}
		break;
	case 24:
		{
			unsigned int *pSrc;
			unsigned int mva;
			unsigned long pa;
			m4u_client_t *client = m4u_create_client();

			pSrc = vmalloc(128);
			m4u_alloc_mva(client, M4U_PORT_DISP_OVL0, (unsigned long)pSrc, NULL,
				      128, 0, 0, &mva);

			m4u_dump_pgtable(domain, NULL);

			pa = m4u_mva_to_pa(NULL, 0, mva);
			m4u_dealloc_mva(client, M4U_PORT_DISP_OVL0, mva);
			M4UMSG("mva:0x%x pa:0x%lx\n", mva, pa);
			m4u_destroy_client(client);
		}
		break;
	case 30:
		m4u_dump_buf_info(NULL);
		break;

	default:
		M4UMSG("m4u_debug_set error,val=%llu\n", val);
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
	M4UMSG("gM4U_log_level: %d, gM4U_log_to_uart:%d\n", gM4U_log_level, gM4U_log_to_uart);

	return 0;
}

static int m4u_log_level_get(void *data, u64 *val)
{
	*val = gM4U_log_level | (gM4U_log_to_uart << 4);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(m4u_log_level_fops, m4u_log_level_get, m4u_log_level_set, "%llu\n");


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
	.release = seq_release,
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
	.release = seq_release,
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
	.release = seq_release,
};

int m4u_debug_register_show(struct seq_file *s, void *unused)
{
	m4u_dump_reg(0);
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
	.release = seq_release,
};


int m4u_debug_init(struct m4u_device *m4u_dev)
{
	struct dentry *debug_file;
	m4u_domain_t *domain = m4u_get_domain_by_id(0);


	m4u_dev->debug_root = debugfs_create_dir("m4u", NULL);

	if (IS_ERR_OR_NULL(m4u_dev->debug_root))
		M4UMSG("m4u: failed to create debug dir.\n");

	debug_file =
	    debugfs_create_file("buffer", 0644, m4u_dev->debug_root, domain, &m4u_debug_buf_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 1.\n");

	debug_file =
	    debugfs_create_file("debug", 0644, m4u_dev->debug_root, domain, &m4u_debug_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 2.\n");

	debug_file =
	    debugfs_create_file("port", 0644, m4u_dev->debug_root, domain, &m4u_debug_port_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 3.\n");

	debug_file =
	    debugfs_create_file("log_level", 0644, m4u_dev->debug_root, domain,
				&m4u_log_level_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 4.\n");

	debug_file =
	    debugfs_create_file("monitor", 0644, m4u_dev->debug_root, domain,
				&m4u_debug_monitor_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 5.\n");

	debug_file =
	    debugfs_create_file("register", 0644, m4u_dev->debug_root, domain,
				&m4u_debug_register_fops);
	if (IS_ERR_OR_NULL(debug_file))
		M4UMSG("m4u: failed to create debug files 6.\n");


	return 0;
}
