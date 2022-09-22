// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/slab.h>
#include <linux/kref.h>
#include "mtk_heap.h"
#include "mtk-hcp_isp71.h"

static struct mtk_hcp_reserve_mblock *mb;

static struct mtk_hcp_reserve_mblock isp71_smvr_mblock[] = {
	{
		/*share buffer for frame setting, to be sw usage*/
		.name = "IMG_MEM_FOR_HW_ID",
		.num = IMG_MEM_FOR_HW_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*need more than 4MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "WPE_MEM_C_ID",
		.num = WPE_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x300000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "WPE_MEM_T_ID",
		.num = WPE_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x500000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "TRAW_MEM_C_ID",
		.num = TRAW_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0xB00000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "TRAW_MEM_T_ID",
		.num = TRAW_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x3000000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "DIP_MEM_C_ID",
		.num = DIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0xF00000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "DIP_MEM_T_ID",
		.num = DIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x3000000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "PQDIP_MEM_C_ID",
		.num = PQDIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x200000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "PQDIP_MEM_T_ID",
		.num = PQDIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x200000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "ADL_MEM_C_ID",
		.num = ADL_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.name = "ADL_MEM_T_ID",
		.num = ADL_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x200000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.name = "IMG_MEM_G_ID",
		.num = IMG_MEM_G_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x22D0000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
};


struct mtk_hcp_reserve_mblock isp71_reserve_mblock[] = {
	{
		/*share buffer for frame setting, to be sw usage*/
		.name = "IMG_MEM_FOR_HW_ID",
		.num = IMG_MEM_FOR_HW_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*need more than 4MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "WPE_MEM_C_ID",
		.num = WPE_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0xE1000,   /*900KB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "WPE_MEM_T_ID",
		.num = WPE_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x196000,   /*1MB + 600KB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "TRAW_MEM_C_ID",
		.num = TRAW_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x4C8000,   /*4MB + 800KB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "TRAW_MEM_T_ID",
		.num = TRAW_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1CC8000,   /*28MB + 800KB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "DIP_MEM_C_ID",
		.num = DIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x5C8000,   /*5MB + 800KB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "DIP_MEM_T_ID",
		.num = DIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1FAF000,   /*31MB + 700KB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "PQDIP_MEM_C_ID",
		.num = PQDIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x130000,   /*1MB + 245KB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "PQDIP_MEM_T_ID",
		.num = PQDIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1F0000,   /*2MB + 31KB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "ADL_MEM_C_ID",
		.num = ADL_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.name = "ADL_MEM_T_ID",
		.num = ADL_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x200000,   /*2MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.name = "IMG_MEM_G_ID",
		.num = IMG_MEM_G_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1610000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
};

phys_addr_t isp71_get_reserve_mem_phys(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return mb[id].start_phys;
	}
}
EXPORT_SYMBOL(isp71_get_reserve_mem_phys);

void *isp71_get_reserve_mem_virt(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return mb[id].start_virt;
}
EXPORT_SYMBOL(isp71_get_reserve_mem_virt);

phys_addr_t isp71_get_reserve_mem_dma(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return mb[id].start_dma;
	}
}
EXPORT_SYMBOL(isp71_get_reserve_mem_dma);

phys_addr_t isp71_get_reserve_mem_size(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return mb[id].size;
	}
}
EXPORT_SYMBOL(isp71_get_reserve_mem_size);

uint32_t isp71_get_reserve_mem_fd(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return mb[id].fd;
}
EXPORT_SYMBOL(isp71_get_reserve_mem_fd);

void *isp71_get_gce_virt(void)
{
	return mb[IMG_MEM_G_ID].start_virt;
}
EXPORT_SYMBOL(isp71_get_gce_virt);

void *isp71_get_hwid_virt(void)
{
	return mb[DIP_MEM_FOR_HW_ID].start_virt;
}
EXPORT_SYMBOL(isp71_get_hwid_virt);

phys_addr_t isp71_get_gce_mem_size(void)
{
	return mb[IMG_MEM_G_ID].size;
}
EXPORT_SYMBOL(isp71_get_gce_mem_size);

int isp71_allocate_working_buffer(struct mtk_hcp *hcp_dev, unsigned int mode)
{
	enum isp71_rsv_mem_id_t id;
	struct mtk_hcp_reserve_mblock *mblock;
	unsigned int block_num;
	struct sg_table *sgt;
	struct dma_buf_attachment *attach;
	struct dma_heap *pdma_heap;
	void *buf_ptr;

	if (mode)
		mblock = hcp_dev->data->smblock;
	else
		mblock = hcp_dev->data->mblock;

	mb = mblock;
	block_num = hcp_dev->data->block_num;
	for (id = 0; id < block_num; id++) {
		if (mblock[id].is_dma_buf) {
			switch (id) {
			case IMG_MEM_FOR_HW_ID:
				/*allocated at probe via dts*/
				break;
			case IMG_MEM_G_ID:
				/* all supported heap name you can find with cmd */
				/* (ls /dev/dma_heap/) in shell */
				pdma_heap = dma_heap_find("mtk_mm");
				if (!pdma_heap) {
					pr_info("pdma_heap find fail\n");
					return -1;
				}
				mblock[id].d_buf = dma_heap_buffer_alloc(
					pdma_heap,
					mblock[id].size, O_RDWR | O_CLOEXEC,
					DMA_HEAP_VALID_HEAP_FLAGS);
				if (IS_ERR(mblock[id].d_buf)) {
					pr_info("dma_heap_buffer_alloc fail :%lld\n",
					PTR_ERR(mblock[id].d_buf));
					return -1;
				}

				mtk_dma_buf_set_name(mblock[id].d_buf, mblock[id].name);

				mblock[id].attach = dma_buf_attach(
				mblock[id].d_buf, hcp_dev->dev);
				attach = mblock[id].attach;
				if (IS_ERR(attach)) {
					pr_info("dma_buf_attach fail :%lld\n",
					PTR_ERR(attach));
					return -1;
				}

				mblock[id].sgt = dma_buf_map_attachment(attach,
				DMA_BIDIRECTIONAL);
				sgt = mblock[id].sgt;
				if (IS_ERR(sgt)) {
					dma_buf_detach(mblock[id].d_buf, attach);
					pr_info("dma_buf_map_attachment fail sgt:%lld\n",
					PTR_ERR(sgt));
					return -1;
				}
				mblock[id].start_phys = sg_dma_address(sgt->sgl);
				mblock[id].start_dma =
				mblock[id].start_phys;
				buf_ptr = dma_buf_vmap(mblock[id].d_buf);
				if (!buf_ptr) {
					pr_info("sg_dma_address fail\n");
					return -1;
				}
				mblock[id].start_virt = buf_ptr;
				get_dma_buf(mblock[id].d_buf);
				mblock[id].fd =
				dma_buf_fd(mblock[id].d_buf,
				O_RDWR | O_CLOEXEC);
				dma_buf_begin_cpu_access(mblock[id].d_buf, DMA_BIDIRECTIONAL);
				kref_init(&mblock[id].kref);
				pr_debug("%s:[HCP][%s] phys:0x%llx, virt:0x%p, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d, d_buf:0x%p\n",
					__func__, mblock[id].name, isp71_get_reserve_mem_phys(id),
					isp71_get_reserve_mem_virt(id),
					isp71_get_reserve_mem_dma(id),
					isp71_get_reserve_mem_size(id),
					mblock[id].is_dma_buf,
					isp71_get_reserve_mem_fd(id),
					mblock[id].d_buf);
				break;
			default:

				/* all supported heap name you can find with cmd */
				/* (ls /dev/dma_heap/) in shell */
				pdma_heap = dma_heap_find("mtk_mm-uncached");
				if (!pdma_heap) {
					pr_info("pdma_heap find fail\n");
					return -1;
				}
				mblock[id].d_buf = dma_heap_buffer_alloc(
					pdma_heap,
					mblock[id].size, O_RDWR | O_CLOEXEC,
					DMA_HEAP_VALID_HEAP_FLAGS);
				if (IS_ERR(mblock[id].d_buf)) {
					pr_info("dma_heap_buffer_alloc fail :%lld\n",
					PTR_ERR(mblock[id].d_buf));
					return -1;
				}

				mtk_dma_buf_set_name(mblock[id].d_buf, mblock[id].name);

				mblock[id].attach = dma_buf_attach(
				mblock[id].d_buf, hcp_dev->dev);
				attach = mblock[id].attach;
				if (IS_ERR(attach)) {
					pr_info("dma_buf_attach fail :%lld\n",
					PTR_ERR(attach));
					return -1;
				}

				mblock[id].sgt = dma_buf_map_attachment(attach,
				DMA_TO_DEVICE);
				sgt = mblock[id].sgt;
				if (IS_ERR(sgt)) {
					dma_buf_detach(mblock[id].d_buf, attach);
					pr_info("dma_buf_map_attachment fail sgt:%lld\n",
					PTR_ERR(sgt));
					return -1;
				}
				mblock[id].start_phys = sg_dma_address(sgt->sgl);
				mblock[id].start_dma =
				mblock[id].start_phys;
				buf_ptr = dma_buf_vmap(mblock[id].d_buf);
				if (!buf_ptr) {
					pr_info("sg_dma_address fail\n");
					return -1;
				}
				mblock[id].start_virt = buf_ptr;
				get_dma_buf(mblock[id].d_buf);
				mblock[id].fd =
				dma_buf_fd(mblock[id].d_buf,
				O_RDWR | O_CLOEXEC);
				break;
			}
		} else {
			mblock[id].start_virt =
				kzalloc(mblock[id].size,
					GFP_KERNEL);
			mblock[id].start_phys =
				virt_to_phys(
					mblock[id].start_virt);
			mblock[id].start_dma = 0;
		}
		pr_debug(
			"%s: [HCP][mem_reserve-%d] phys:0x%llx, virt:0x%p, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d, d_buf:0x%p\n",
			__func__, id, isp71_get_reserve_mem_phys(id),
			isp71_get_reserve_mem_virt(id),
			isp71_get_reserve_mem_dma(id),
			isp71_get_reserve_mem_size(id),
			mblock[id].is_dma_buf,
			isp71_get_reserve_mem_fd(id),
			mblock[id].d_buf);
	}

	return 0;
}
EXPORT_SYMBOL(isp71_allocate_working_buffer);

static void gce_release(struct kref *ref)
{
	struct mtk_hcp_reserve_mblock *mblock =
		container_of(ref, struct mtk_hcp_reserve_mblock, kref);

	dma_buf_vunmap(mblock->d_buf, mblock->start_virt);
	/* free iova */
	dma_buf_unmap_attachment(mblock->attach, mblock->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(mblock->d_buf, mblock->attach);
	dma_buf_end_cpu_access(mblock->d_buf, DMA_BIDIRECTIONAL);
	dma_buf_put(mblock->d_buf);
	pr_debug("%s:[HCP][%s] phys:0x%llx, virt:0x%p, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d, d_buf:0x%p\n",
		__func__, mblock->name, isp71_get_reserve_mem_phys(IMG_MEM_G_ID),
		isp71_get_reserve_mem_virt(IMG_MEM_G_ID),
		isp71_get_reserve_mem_dma(IMG_MEM_G_ID),
		isp71_get_reserve_mem_size(IMG_MEM_G_ID),
		mblock->is_dma_buf,
		isp71_get_reserve_mem_fd(IMG_MEM_G_ID),
		mblock->d_buf);
	// close fd in user space driver, you can't close fd in kernel site
	// dma_heap_buffer_free(mblock[id].d_buf);
	//dma_buf_put(my_dma_buf);
	//also can use this api, but not recommended
	mblock->mem_priv = NULL;
	mblock->mmap_cnt = 0;
	mblock->start_dma = 0x0;
	mblock->start_virt = 0x0;
	mblock->start_phys = 0x0;
	mblock->d_buf = NULL;
	mblock->fd = -1;
	mblock->pIonHandle = NULL;
	mblock->attach = NULL;
	mblock->sgt = NULL;
	pr_info("%s", __func__);
}


int isp71_release_working_buffer(struct mtk_hcp *hcp_dev)
{
	enum isp71_rsv_mem_id_t id;
	struct mtk_hcp_reserve_mblock *mblock;
	unsigned int block_num;

	mblock = mb;
	block_num = hcp_dev->data->block_num;

	/* release reserved memory */
	for (id = 0; id < NUMS_MEM_ID; id++) {
		if (mblock[id].is_dma_buf) {
			switch (id) {
			case IMG_MEM_FOR_HW_ID:
				/*allocated at probe via dts*/
				break;
			case IMG_MEM_G_ID:
				kref_put(&mblock[id].kref, gce_release);
				break;
			default:
				/* free va */
				dma_buf_vunmap(mblock[id].d_buf,
				mblock[id].start_virt);
				/* free iova */
				dma_buf_unmap_attachment(mblock[id].attach,
				mblock[id].sgt, DMA_TO_DEVICE);
				dma_buf_detach(mblock[id].d_buf,
				mblock[id].attach);
				dma_buf_put(mblock[id].d_buf);
				// close fd in user space driver, you can't close fd in kernel site
				// dma_heap_buffer_free(mblock[id].d_buf);
				//dma_buf_put(my_dma_buf);
				//also can use this api, but not recommended
				mblock[id].mem_priv = NULL;
				mblock[id].mmap_cnt = 0;
				mblock[id].start_dma = 0x0;
				mblock[id].start_virt = 0x0;
				mblock[id].start_phys = 0x0;
				mblock[id].d_buf = NULL;
				mblock[id].fd = -1;
				mblock[id].pIonHandle = NULL;
				mblock[id].attach = NULL;
				mblock[id].sgt = NULL;
				break;
			}
		} else {
			kfree(mblock[id].start_virt);
			mblock[id].start_virt = 0x0;
			mblock[id].start_phys = 0x0;
			mblock[id].start_dma = 0x0;
			mblock[id].mmap_cnt = 0;
		}
		pr_debug(
			"%s: [HCP][mem_reserve-%d] phys:0x%llx, virt:0x%p, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d\n",
			__func__, id, isp71_get_reserve_mem_phys(id),
			isp71_get_reserve_mem_virt(id),
			isp71_get_reserve_mem_dma(id),
			isp71_get_reserve_mem_size(id),
			mblock[id].is_dma_buf,
			isp71_get_reserve_mem_fd(id));
	}

	return 0;
}
EXPORT_SYMBOL(isp71_release_working_buffer);

int isp71_get_init_info(struct img_init_info *info)
{

	if (!info) {
		pr_info("%s:NULL info\n", __func__);
		return -1;
	}

	info->hw_buf = isp71_get_reserve_mem_phys(DIP_MEM_FOR_HW_ID);
	/*WPE:0, ADL:1, TRAW:2, DIP:3, PQDIP:4 */
	info->module_info[0].c_wbuf =
				isp71_get_reserve_mem_phys(WPE_MEM_C_ID);
	info->module_info[0].c_wbuf_dma =
				isp71_get_reserve_mem_dma(WPE_MEM_C_ID);
	info->module_info[0].c_wbuf_sz =
				isp71_get_reserve_mem_size(WPE_MEM_C_ID);
	info->module_info[0].c_wbuf_fd =
				isp71_get_reserve_mem_fd(WPE_MEM_C_ID);
	info->module_info[0].t_wbuf =
				isp71_get_reserve_mem_phys(WPE_MEM_T_ID);
	info->module_info[0].t_wbuf_dma =
				isp71_get_reserve_mem_dma(WPE_MEM_T_ID);
	info->module_info[0].t_wbuf_sz =
				isp71_get_reserve_mem_size(WPE_MEM_T_ID);
	info->module_info[0].t_wbuf_fd =
				isp71_get_reserve_mem_fd(WPE_MEM_T_ID);

  // ADL
	info->module_info[1].c_wbuf =
				isp71_get_reserve_mem_phys(ADL_MEM_C_ID);
	info->module_info[1].c_wbuf_dma =
				isp71_get_reserve_mem_dma(ADL_MEM_C_ID);
	info->module_info[1].c_wbuf_sz =
				isp71_get_reserve_mem_size(ADL_MEM_C_ID);
	info->module_info[1].c_wbuf_fd =
				isp71_get_reserve_mem_fd(ADL_MEM_C_ID);
	info->module_info[1].t_wbuf =
				isp71_get_reserve_mem_phys(ADL_MEM_T_ID);
	info->module_info[1].t_wbuf_dma =
				isp71_get_reserve_mem_dma(ADL_MEM_T_ID);
	info->module_info[1].t_wbuf_sz =
				isp71_get_reserve_mem_size(ADL_MEM_T_ID);
	info->module_info[1].t_wbuf_fd =
				isp71_get_reserve_mem_fd(ADL_MEM_T_ID);

	// TRAW
	info->module_info[2].c_wbuf =
				isp71_get_reserve_mem_phys(TRAW_MEM_C_ID);
	info->module_info[2].c_wbuf_dma =
				isp71_get_reserve_mem_dma(TRAW_MEM_C_ID);
	info->module_info[2].c_wbuf_sz =
				isp71_get_reserve_mem_size(TRAW_MEM_C_ID);
	info->module_info[2].c_wbuf_fd =
				isp71_get_reserve_mem_fd(TRAW_MEM_C_ID);
	info->module_info[2].t_wbuf =
				isp71_get_reserve_mem_phys(TRAW_MEM_T_ID);
	info->module_info[2].t_wbuf_dma =
				isp71_get_reserve_mem_dma(TRAW_MEM_T_ID);
	info->module_info[2].t_wbuf_sz =
				isp71_get_reserve_mem_size(TRAW_MEM_T_ID);
	info->module_info[2].t_wbuf_fd =
				isp71_get_reserve_mem_fd(TRAW_MEM_T_ID);

		// DIP
	info->module_info[3].c_wbuf =
				isp71_get_reserve_mem_phys(DIP_MEM_C_ID);
	info->module_info[3].c_wbuf_dma =
				isp71_get_reserve_mem_dma(DIP_MEM_C_ID);
	info->module_info[3].c_wbuf_sz =
				isp71_get_reserve_mem_size(DIP_MEM_C_ID);
	info->module_info[3].c_wbuf_fd =
				isp71_get_reserve_mem_fd(DIP_MEM_C_ID);
	info->module_info[3].t_wbuf =
				isp71_get_reserve_mem_phys(DIP_MEM_T_ID);
	info->module_info[3].t_wbuf_dma =
				isp71_get_reserve_mem_dma(DIP_MEM_T_ID);
	info->module_info[3].t_wbuf_sz =
				isp71_get_reserve_mem_size(DIP_MEM_T_ID);
	info->module_info[3].t_wbuf_fd =
				isp71_get_reserve_mem_fd(DIP_MEM_T_ID);

	// PQDIP
	info->module_info[4].c_wbuf =
				isp71_get_reserve_mem_phys(PQDIP_MEM_C_ID);
	info->module_info[4].c_wbuf_dma =
				isp71_get_reserve_mem_dma(PQDIP_MEM_C_ID);
	info->module_info[4].c_wbuf_sz =
				isp71_get_reserve_mem_size(PQDIP_MEM_C_ID);
	info->module_info[4].c_wbuf_fd =
			isp71_get_reserve_mem_fd(PQDIP_MEM_C_ID);
	info->module_info[4].t_wbuf =
				isp71_get_reserve_mem_phys(PQDIP_MEM_T_ID);
	info->module_info[4].t_wbuf_dma =
				isp71_get_reserve_mem_dma(PQDIP_MEM_T_ID);
	info->module_info[4].t_wbuf_sz =
				isp71_get_reserve_mem_size(PQDIP_MEM_T_ID);
	info->module_info[4].t_wbuf_fd =
				isp71_get_reserve_mem_fd(PQDIP_MEM_T_ID);

	/*common*/
	/* info->g_wbuf_fd = isp71_get_reserve_mem_fd(IMG_MEM_G_ID); */
	info->g_wbuf_fd = isp71_get_reserve_mem_fd(IMG_MEM_G_ID);
	info->g_wbuf = isp71_get_reserve_mem_phys(IMG_MEM_G_ID);
	/*info->g_wbuf_sw = isp71_get_reserve_mem_virt(IMG_MEM_G_ID);*/
	info->g_wbuf_sz = isp71_get_reserve_mem_size(IMG_MEM_G_ID);

	return 0;
}

static int isp71_put_gce(void)
{
	kref_put(&mb[IMG_MEM_G_ID].kref, gce_release);
	return 0;
}

static int isp71_get_gce(void)
{
	kref_get(&mb[IMG_MEM_G_ID].kref);
	return 0;
}

struct mtk_hcp_data isp71_hcp_data = {
	.mblock = isp71_reserve_mblock,
	.block_num = ARRAY_SIZE(isp71_reserve_mblock),
	.smblock = isp71_smvr_mblock,
	.allocate = isp71_allocate_working_buffer,
	.release = isp71_release_working_buffer,
	.get_init_info = isp71_get_init_info,
	.get_gce_virt = isp71_get_gce_virt,
	.get_gce = isp71_get_gce,
	.put_gce = isp71_put_gce,
	.get_hwid_virt = isp71_get_hwid_virt,
	.get_gce_mem_size = isp71_get_gce_mem_size,
};
