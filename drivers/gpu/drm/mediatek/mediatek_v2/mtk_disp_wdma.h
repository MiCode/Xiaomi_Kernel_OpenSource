/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_WDMA_H__
#define __MTK_DISP_WDMA_H__

struct mtk_disp_wdma_data {
	/* golden setting */
	unsigned int fifo_size_1plane;
	unsigned int fifo_size_uv_1plane;
	unsigned int fifo_size_2plane;
	unsigned int fifo_size_uv_2plane;
	unsigned int fifo_size_3plane;
	unsigned int fifo_size_uv_3plane;

	void (*sodi_config)(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
	unsigned int (*aid_sel)(struct mtk_ddp_comp *comp);
	resource_size_t (*check_wdma_sec_reg)(struct mtk_ddp_comp *comp);
	bool support_shadow;
	bool need_bypass_shadow;
	bool is_support_34bits;
	bool use_larb_control_sec;
};

#endif
