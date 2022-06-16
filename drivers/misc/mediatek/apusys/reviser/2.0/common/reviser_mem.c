// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <asm/mman.h>
#include <linux/iommu.h>
#include <linux/module.h>

#include "reviser_cmn.h"
#include "reviser_mem.h"
#include "reviser_drv.h"

static struct reviser_mem g_mem_sys;

static int __reviser_free_iova(struct device *dev, size_t len,
		dma_addr_t given_iova)
{

	struct iommu_domain *domain;
	dma_addr_t iova;
	size_t size = len;
	size_t ret;

	domain = iommu_get_domain_for_dev(dev);
	if (domain == NULL) {
		LOG_ERR("iommu_unmap cannot get domain\n");
		return -ENOMEM;
	}
	iova = given_iova;

	ret = iommu_unmap(domain, iova, size);
	if (ret != size) {
		LOG_ERR("iommu_unmap iova: %llx, returned: %zx, expected: %zx\n",
					(u64)iova, ret, size);

		return -ENOMEM;
	}

	return 0;
}


static int __reviser_get_sgt(const char *buf,
		size_t len, struct sg_table *sgt)
{
	struct page **pages = NULL;
	unsigned int nr_pages;
	unsigned int index;
	const char *p;
	int ret;

	nr_pages = DIV_ROUND_UP((unsigned long)buf + len, PAGE_SIZE)
		- ((unsigned long)buf / PAGE_SIZE);
	pages = kmalloc_array(nr_pages, sizeof(struct page *), GFP_KERNEL);

	if (!pages)
		return -ENOMEM;

	p = buf - offset_in_page(buf);
	LOG_DBG_RVR_MEM("start p: %llx buf: %llx\n",
			(uint64_t)p, (uint64_t)buf);

	for (index = 0; index < nr_pages; index++) {
		if (is_vmalloc_addr(p))
			pages[index] = vmalloc_to_page(p);
		else
			pages[index] = kmap_to_page((void *)p);
		if (!pages[index]) {
			kfree(pages);
			LOG_ERR("map failed\n");
			return -EFAULT;
		}
		p += PAGE_SIZE;
	}


	ret = sg_alloc_table_from_pages(sgt, pages, index,
		offset_in_page(buf), len, GFP_KERNEL);
	kfree(pages);
	if (ret) {
		LOG_ERR("sg_alloc_table_from_pages: %d\n", ret);
		return ret;
	}



	LOG_DBG_RVR_MEM("buf: %p, len: %lx, sgt: %p nr_pages: %d\n",
		buf, len, sgt, nr_pages);

	return 0;
}

static dma_addr_t __reviser_get_iova(
	struct device *dev, struct scatterlist *sg,
	unsigned int nents, size_t len, dma_addr_t given_iova)
{
	struct iommu_domain *domain;
	dma_addr_t iova = 0;
	int prot = IOMMU_READ | IOMMU_WRITE;
	struct reviser_dev_info *rdv = dev_get_drvdata(dev);
	dma_addr_t boundary_mask;
	size_t iova_size;

	domain = iommu_get_domain_for_dev(dev);
	if (domain == NULL) {
		LOG_ERR("iommu_unmap cannot get domain\n");
		goto err;
	}

	iova = given_iova;
	//Need to check boundary region with iommu team every project
	boundary_mask = (dma_addr_t) rdv->plat.boundary << 32;
	iova |= boundary_mask;

	iova_size = iommu_map_sg(domain, iova, sg, nents, prot);

	if (iova_size == 0) {
		LOG_ERR("iommu_map_sg: len: %zx, iova: %llx, failed\n",
			len, (u64)iova);
		goto err;
	} else if (iova_size != len) {
		LOG_ERR("iommu_map_sg: len: %zx, iova: %llx, mismatch with mapped size: %zx\n",
			len, (u64)iova, iova_size);
		goto err;
	}
	LOG_INFO("sg_dma_address: size: %lx, mapped iova: 0x%llx iova_size: %lx\n",
		len, (uint64_t)iova, iova_size);

	return iova;

err:
	return 0;
}

int reviser_mem_free(struct device *dev, struct reviser_mem *mem, bool fix)
{
	int ret = 0;

	if (fix) {
		kvfree((void *) mem->kva);

		if (!__reviser_free_iova(dev, mem->size, mem->iova)) {
			sg_free_table(&mem->sgt);
			ret = 0;
			LOG_INFO("mem free (0x%x/%d/0x%llx)\n",
					mem->iova, mem->size, mem->kva);
		} else {
			ret = -1;
			LOG_INFO("mem free fail(0x%x/%d/0x%llx)\n",
					mem->iova, mem->size, mem->kva);
		}
	} else {
		dma_free_coherent(dev, mem->size,
				(void *)mem->kva, mem->iova);
	}


	return 0;
}

int reviser_mem_alloc(struct device *dev, struct reviser_mem *mem, bool fix)
{
	int ret = 0;
	void *kva;
	dma_addr_t iova = 0;
	struct reviser_dev_info *rdv = dev_get_drvdata(dev);

	if (fix) {
		kva = kvmalloc(mem->size, GFP_KERNEL);

		if (!kva) {
			LOG_ERR("kvmalloc: failed\n");
			ret = -ENOMEM;
			goto out;
		}
		memset((void *)kva, 0, mem->size);

		if (__reviser_get_sgt(kva, mem->size, &mem->sgt)) {
			LOG_ERR("get sgt: failed\n");
			ret = -ENOMEM;
			kvfree(kva);
			goto out;
		}

		iova = __reviser_get_iova(dev, mem->sgt.sgl, mem->sgt.nents,
				mem->size, rdv->plat.dram[0]);
		if ((!iova) || ((uint32_t)iova != rdv->plat.dram[0])) {
			LOG_ERR("iova wrong (0x%llx)\n", iova);
			kvfree(kva);
			goto out;
		}

	} else {
		kva = dma_alloc_coherent(dev, mem->size,
					&iova, GFP_KERNEL);
		if (!kva) {
			LOG_ERR("dma_alloc_coherent fail (0x%llx)\n", mem->size);
			goto out;
		}
	}

#ifndef MODULE
	/*
	 * Avoid a kmemleak false positive.
	 * The pointer is using for debugging,
	 * but it will be used by other apusys HW
	 */
	kmemleak_no_scan(kva);
#endif
	mem->kva = (uint64_t)kva;
	mem->iova = (uint64_t)iova;

	LOG_INFO("mem(0x%x/%d/0x%llx)\n",
			mem->iova, mem->size, mem->kva);

	goto out;
out:
	return ret;

}



int reviser_mem_invalidate(struct device *dev, struct reviser_mem *mem)
{
	dma_sync_sg_for_cpu(dev, mem->sgt.sgl, mem->sgt.nents,
		DMA_FROM_DEVICE);

	return 0;
}


int reviser_dram_remap_init(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	unsigned int ctx_max = 0;
	unsigned int i = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	ctx_max = rdv->plat.device[REVISER_DEVICE_MDLA]
				+ rdv->plat.device[REVISER_DEVICE_VPU]
				+ rdv->plat.device[REVISER_DEVICE_EDMA]
				+ rdv->plat.device[REVISER_DEVICE_SECURE_MD32];

	// dram_ctx = preemption_ctx + ip_ctx

	// fix_dram will be used on AP version
	if (rdv->plat.fix_dram)
		rdv->plat.dram_max = ctx_max * 2;
	else
		rdv->plat.dram_max = 15;

	g_mem_sys.size = rdv->plat.vlm_size * rdv->plat.dram_max;
	if (reviser_mem_alloc(rdv->dev, &g_mem_sys, rdv->plat.fix_dram)) {
		LOG_ERR("alloc fail\n");
		return -ENOMEM;
	}

	//_reviser_set_default_iova(drvinfo, g_mem_sys.iova);
	rdv->rsc.dram.base = (void *) g_mem_sys.kva;
	for (i = 0; i < rdv->plat.dram_max; i++)
		rdv->plat.dram[i] = g_mem_sys.iova + rdv->plat.vlm_size * (uint64_t) i;

	return 0;
}
int reviser_dram_remap_destroy(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	rdv = (struct reviser_dev_info *)drvinfo;

	reviser_mem_free(rdv->dev, &g_mem_sys, rdv->plat.fix_dram);
	rdv->rsc.dram.base = NULL;
	return 0;
}

void reviser_print_dram(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	int index;
	unsigned char *data;
	struct seq_file *s = (struct seq_file *)s_file;
	unsigned int ctx_max = 0;
	unsigned int vlm_size = 0;
	unsigned int vlm_bank_size = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;
	data = (unsigned char *)rdv->rsc.dram.base;

	ctx_max = rdv->plat.device[REVISER_DEVICE_MDLA]
			+ rdv->plat.device[REVISER_DEVICE_VPU]
			+ rdv->plat.device[REVISER_DEVICE_EDMA]
			+ rdv->plat.device[REVISER_DEVICE_SECURE_MD32];
	vlm_size = rdv->plat.vlm_size;
	vlm_bank_size = rdv->plat.bank_size;

	reviser_mem_invalidate(rdv->dev, &g_mem_sys);

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser dram table info\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "== PAGE[NUM] == [BANK0][BANK1][BANK2][BANK3]\n");
	LOG_CON(s, "-----------------------------\n");

	for (index = 0; index < ctx_max; index++) {
		LOG_CON(s, "== PAGE[%02d] == [%02x][%02x][%02x][%02x][%02x][%02x][%02x][%02x]\n",
				index,
				*(data + vlm_size*index + vlm_bank_size*0),
				*(data + vlm_size*index + vlm_bank_size*1),
				*(data + vlm_size*index + vlm_bank_size*2),
				*(data + vlm_size*index + vlm_bank_size*3),
				*(data + vlm_size*index + vlm_bank_size*4),
				*(data + vlm_size*index + vlm_bank_size*5),
				*(data + vlm_size*index + vlm_bank_size*6),
				*(data + vlm_size*index + vlm_bank_size*7));

	}


	LOG_CON(s, "=============================\n");
	return;

}

void reviser_print_tcm(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	uint8_t bank0[32], bank1[32], bank2[32], bank3[32];
	struct seq_file *s = (struct seq_file *)s_file;
	unsigned int vlm_bank_size = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;
	vlm_bank_size = rdv->plat.bank_size;

	if (rdv->plat.pool_size[0] == 0) {
		LOG_ERR("invalid TCM\n");
		return;
	}

	memcpy_fromio(bank0, rdv->rsc.pool[0].base + vlm_bank_size*0, 32);
	memcpy_fromio(bank1, rdv->rsc.pool[0].base + vlm_bank_size*1, 32);
	memcpy_fromio(bank2, rdv->rsc.pool[0].base + vlm_bank_size*2, 32);
	memcpy_fromio(bank3, rdv->rsc.pool[0].base + vlm_bank_size*3, 32);
	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser pool[0] table info\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "== BANK[NUM] == [DATA][DATA][DATA][DATA]\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "== BANK[0] == [%02x][%02x][%02x]\n",
					*(bank0), *(bank0 + 1), *(bank0 + 2));
	LOG_CON(s, "== BANK[1] == [%02x][%02x][%02x]\n",
					*(bank1), *(bank1 + 1), *(bank1 + 2));
	LOG_CON(s, "== BANK[2] == [%02x][%02x][%02x]\n",
					*(bank2), *(bank2 + 1), *(bank2 + 2));
	LOG_CON(s, "== BANK[3] == [%02x][%02x][%02x]\n",
					*(bank3), *(bank3 + 1), *(bank3 + 2));
	LOG_CON(s, "=============================\n");
	return;

}


