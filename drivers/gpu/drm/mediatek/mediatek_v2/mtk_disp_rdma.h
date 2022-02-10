/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_RDMA_H__
#define __MTK_DISP_RDMA_H__

struct mtk_disp_rdma_data {
	/* golden setting */
	unsigned int fifo_size;
	unsigned int pre_ultra_low_us;
	unsigned int pre_ultra_high_us;
	unsigned int ultra_low_us;
	unsigned int ultra_high_us;
	unsigned int urgent_low_us;
	unsigned int urgent_high_us;

	void (*sodi_config)(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
	unsigned int shadow_update_reg;
	bool support_shadow;
	bool need_bypass_shadow;
	bool has_greq_urg_num;
	bool is_support_34bits;
	bool dsi_buffer;
	bool rdma_irq_ts_debug;
	bool disable_underflow;
};

#endif
