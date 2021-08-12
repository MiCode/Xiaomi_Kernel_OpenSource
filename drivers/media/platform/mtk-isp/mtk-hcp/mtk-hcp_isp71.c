// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/slab.h>
#include "mtk-hcp_isp71.h"

struct mtk_hcp_reserve_mblock isp71_reserve_mblock[] = {
	{
		/*share buffer for frame setting, to be sw usage*/
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
		.num = WPE_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
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
		.num = WPE_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
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
		.num = TRAW_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*4MB*/
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
		.num = TRAW_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1400000,   /*20MB*/
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
		.num = DIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1700000,   /*23MB*/
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
		.num = DIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1D00000,   /*29MB*/
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
		.num = PQDIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x300000,   /*3MB*/
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
		.num = PQDIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x200000,   /*2MB*/
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
		.num = ADL_MEM_T_ID,
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
		.num = IMG_MEM_G_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1188000,   /*15MB GCE + 2MB TPIPE + 30KB BW*/
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
	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return isp71_reserve_mblock[id].start_phys;
	}
}
EXPORT_SYMBOL(isp71_get_reserve_mem_phys);

void *isp71_get_reserve_mem_virt(unsigned int id)
{
	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return isp71_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL(isp71_get_reserve_mem_virt);

phys_addr_t isp71_get_reserve_mem_dma(unsigned int id)
{
	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return isp71_reserve_mblock[id].start_dma;
	}
}
EXPORT_SYMBOL(isp71_get_reserve_mem_dma);

phys_addr_t isp71_get_reserve_mem_size(unsigned int id)
{
	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return isp71_reserve_mblock[id].size;
	}
}
EXPORT_SYMBOL(isp71_get_reserve_mem_size);

uint32_t isp71_get_reserve_mem_fd(unsigned int id)
{
	if ((id < 0) || (id >= NUMS_MEM_ID)) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else
		return isp71_reserve_mblock[id].fd;
}
EXPORT_SYMBOL(isp71_get_reserve_mem_fd);

void *isp71_get_gce_virt(void)
{
	return isp71_reserve_mblock[IMG_MEM_G_ID].start_virt;
}
EXPORT_SYMBOL(isp71_get_gce_virt);

void *isp71_get_hwid_virt(void)
{
	return isp71_reserve_mblock[DIP_MEM_FOR_HW_ID].start_virt;
}
EXPORT_SYMBOL(isp71_get_hwid_virt);


int isp71_allocate_working_buffer(struct mtk_hcp *hcp_dev)
{
	enum isp71_rsv_mem_id_t id;
	struct mtk_hcp_reserve_mblock *mblock;
	unsigned int block_num;
	struct sg_table *sgt;
	struct dma_buf_attachment *attach;
	struct dma_heap *pdma_heap;
	void *buf_ptr;

	mblock = hcp_dev->data->mblock;
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
				mblock[id].fd =
				dma_buf_fd(mblock[id].d_buf,
				O_RDWR | O_CLOEXEC);
				dma_buf_get(mblock[id].fd);
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
				mblock[id].fd =
				dma_buf_fd(mblock[id].d_buf,
				O_RDWR | O_CLOEXEC);
				dma_buf_get(mblock[id].fd);
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
		pr_info(
			"%s: [HCP][mem_reserve-%d] phys:0x%llx, virt:0x%llx, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d, d_buf:0x%llx\n",
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

int isp71_release_working_buffer(struct mtk_hcp *hcp_dev)
{
	enum isp71_rsv_mem_id_t id;
	struct mtk_hcp_reserve_mblock *mblock;
	unsigned int block_num;

	mblock = hcp_dev->data->mblock;
	block_num = hcp_dev->data->block_num;

	/* release reserved memory */
	for (id = 0; id < NUMS_MEM_ID; id++) {
		if (mblock[id].is_dma_buf) {
			switch (id) {
			case IMG_MEM_FOR_HW_ID:
				/*allocated at probe via dts*/
				break;
			/* case IMG_MEM_G_ID: */
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
		pr_info(
			"%s: [HCP][mem_reserve-%d] phys:0x%llx, virt:0x%llx, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d\n",
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
	/*WPE:0, TRAW:1, DIP:2, PQDIP:3, ADL:4 */
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

	// TRAW
	info->module_info[1].c_wbuf =
				isp71_get_reserve_mem_phys(TRAW_MEM_C_ID);
	info->module_info[1].c_wbuf_dma =
				isp71_get_reserve_mem_dma(TRAW_MEM_C_ID);
	info->module_info[1].c_wbuf_sz =
				isp71_get_reserve_mem_size(TRAW_MEM_C_ID);
	info->module_info[1].c_wbuf_fd =
				isp71_get_reserve_mem_fd(TRAW_MEM_C_ID);
	info->module_info[1].t_wbuf =
				isp71_get_reserve_mem_phys(TRAW_MEM_T_ID);
	info->module_info[1].t_wbuf_dma =
				isp71_get_reserve_mem_dma(TRAW_MEM_T_ID);
	info->module_info[1].t_wbuf_sz =
				isp71_get_reserve_mem_size(TRAW_MEM_T_ID);
	info->module_info[1].t_wbuf_fd =
				isp71_get_reserve_mem_fd(TRAW_MEM_T_ID);

		// DIP
	info->module_info[2].c_wbuf =
				isp71_get_reserve_mem_phys(DIP_MEM_C_ID);
	info->module_info[2].c_wbuf_dma =
				isp71_get_reserve_mem_dma(DIP_MEM_C_ID);
	info->module_info[2].c_wbuf_sz =
				isp71_get_reserve_mem_size(DIP_MEM_C_ID);
	info->module_info[2].c_wbuf_fd =
				isp71_get_reserve_mem_fd(DIP_MEM_C_ID);
	info->module_info[2].t_wbuf =
				isp71_get_reserve_mem_phys(DIP_MEM_T_ID);
	info->module_info[2].t_wbuf_dma =
				isp71_get_reserve_mem_dma(DIP_MEM_T_ID);
	info->module_info[2].t_wbuf_sz =
				isp71_get_reserve_mem_size(DIP_MEM_T_ID);
	info->module_info[2].t_wbuf_fd =
				isp71_get_reserve_mem_fd(DIP_MEM_T_ID);

	// PQDIP
	info->module_info[3].c_wbuf =
				isp71_get_reserve_mem_phys(PQDIP_MEM_C_ID);
	info->module_info[3].c_wbuf_dma =
				isp71_get_reserve_mem_dma(PQDIP_MEM_C_ID);
	info->module_info[3].c_wbuf_sz =
				isp71_get_reserve_mem_size(PQDIP_MEM_C_ID);
	info->module_info[3].c_wbuf_fd =
			isp71_get_reserve_mem_fd(PQDIP_MEM_C_ID);
	info->module_info[3].t_wbuf =
				isp71_get_reserve_mem_phys(PQDIP_MEM_T_ID);
	info->module_info[3].t_wbuf_dma =
				isp71_get_reserve_mem_dma(PQDIP_MEM_T_ID);
	info->module_info[3].t_wbuf_sz =
				isp71_get_reserve_mem_size(PQDIP_MEM_T_ID);
	info->module_info[3].t_wbuf_fd =
				isp71_get_reserve_mem_fd(PQDIP_MEM_T_ID);

	info->module_info[4].c_wbuf =
				isp71_get_reserve_mem_phys(ADL_MEM_C_ID);
	info->module_info[4].c_wbuf_dma =
				isp71_get_reserve_mem_dma(ADL_MEM_C_ID);
	info->module_info[4].c_wbuf_sz =
				isp71_get_reserve_mem_size(ADL_MEM_C_ID);
	info->module_info[4].c_wbuf_fd =
				isp71_get_reserve_mem_fd(ADL_MEM_C_ID);
	info->module_info[4].t_wbuf =
				isp71_get_reserve_mem_phys(ADL_MEM_T_ID);
	info->module_info[4].t_wbuf_dma =
				isp71_get_reserve_mem_dma(ADL_MEM_T_ID);
	info->module_info[4].t_wbuf_sz =
				isp71_get_reserve_mem_size(ADL_MEM_T_ID);
	info->module_info[4].t_wbuf_fd =
				isp71_get_reserve_mem_fd(ADL_MEM_T_ID);

	/*common*/
	/* info->g_wbuf_fd = isp71_get_reserve_mem_fd(IMG_MEM_G_ID); */
	info->g_wbuf_fd = isp71_get_reserve_mem_fd(IMG_MEM_G_ID);
	info->g_wbuf = isp71_get_reserve_mem_phys(IMG_MEM_G_ID);
	/*info->g_wbuf_sw = isp71_get_reserve_mem_virt(IMG_MEM_G_ID);*/
	info->g_wbuf_sz = isp71_get_reserve_mem_size(IMG_MEM_G_ID);

	return 0;
}

struct mtk_hcp_data isp71_hcp_data = {
	.mblock = isp71_reserve_mblock,
	.block_num = ARRAY_SIZE(isp71_reserve_mblock),
	.allocate = isp71_allocate_working_buffer,
	.release = isp71_release_working_buffer,
	.get_init_info = isp71_get_init_info,
	.get_gce_virt = isp71_get_gce_virt,
	.get_hwid_virt = isp71_get_hwid_virt,
};
