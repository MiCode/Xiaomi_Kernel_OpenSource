/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_CAM_RAW_DEBUG_H
#define _MTK_CAM_RAW_DEBUG_H

void debug_dma_fbc(struct device *dev,
			  void __iomem *base, void __iomem *yuvbase);

void mtk_cam_raw_dump_fbc(struct device *dev,
			  void __iomem *base, void __iomem *yuvbase);

void mtk_cam_raw_dump_dma_err_st(struct device *dev, void __iomem *base);
void mtk_cam_yuv_dump_dma_err_st(struct device *dev, void __iomem *yuvbase);

void mtk_cam_dump_req_rdy_status(struct device *dev,
				 void __iomem *base, void __iomem *yuvbase);

struct dma_debug_item {
	unsigned int	debug_sel;
	const char	*msg;
};

void mtk_cam_dump_dma_debug(struct device *dev,
			    void __iomem *dmatop_base,
			    const char *dma_name,
			    struct dma_debug_item *items, int n);

void mtk_cam_sw_reset_check(struct device *dev,
			    void __iomem *dmatop_base,
			    struct dma_debug_item *items, int n);

enum topdebug_event {
	ALL_THE_TIME	= 1 << 0,
	TG_OVERRUN	= 1 << 1,
	CQ_MAIN_VS_ERR	= 1 << 2,
	CQ_SUB_VS_ERR	= 1 << 3,
	RAW_DMA_ERR	= 1 << 4,
	YUV_DMA_ERR	= 1 << 5,
};

void mtk_cam_set_topdebug_rdyreq(struct device *dev,
				 void __iomem *base, void __iomem *yuvbase,
				 u32 event);

void mtk_cam_dump_topdebug_rdyreq(struct device *dev,
				  void __iomem *base, void __iomem *yuvbase);

#endif	/* _MTK_CAM_RAW_DEBUG_H */
