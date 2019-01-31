/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Honghui Zhang <honghui.zhang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_IOMMU_EXT_H_
#define _MTK_IOMMU_EXT_H_
#include <linux/io.h>

extern phys_addr_t mtkfb_get_fb_base(void);
size_t mtkfb_get_fb_size(void);
int smi_reg_backup_sec(void);
int smi_reg_restore_sec(void);

enum mtk_iommu_callback_ret_t {
	MTK_IOMMU_CALLBACK_HANDLED,
	MTK_IOMMU_CALLBACK_NOT_HANDLED,
};


typedef enum mtk_iommu_callback_ret_t (*mtk_iommu_fault_callback_t)(int port,
				unsigned int mva, void *cb_data);

int mtk_iommu_register_fault_callback(int port,
				      mtk_iommu_fault_callback_t fn,
				      void *cb_data);
int mtk_iommu_unregister_fault_callback(int port);
int mtk_iommu_enable_tf(int port, bool fgenable);
void *mtk_iommu_iova_to_va(struct device *dev, dma_addr_t iova, size_t size);
int mtk_smi_larb_get_ext(struct device *larbdev);


bool enable_custom_tf_report(void);
bool report_custom_iommu_fault(
	void __iomem	*base,
	unsigned int	int_state,
	unsigned int	fault_iova,
	unsigned int	fault_pa,
	unsigned int	fault_id);

void mtk_iommu_debug_init(void);
void mtk_iommu_debug_reset(void);


enum IOMMU_PROFILE_TYPE {
	IOMMU_ALLOC = 0,
	IOMMU_DEALLOC,
	IOMMU_MAP,
	IOMMU_UNMAP,
	IOMMU_EVENT_MAX,
};

void mtk_iommu_trace_map(unsigned long orig_iova,
			 phys_addr_t orig_pa,
			 size_t size);
void mtk_iommu_trace_unmap(unsigned long orig_iova,
			   size_t size,
			   size_t unmapped);
void mtk_iommu_trace_log(int event,
			 unsigned int data1,
			 unsigned int data2,
			 unsigned int data3);

void mtk_iommu_log_dump(void *seq_file);

#if 0
void mtk_smi_larb_put(struct device *larbdev);
int mtk_smi_larb_ready(int larbid);
#endif



#endif
