/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_DSC_H__
#define __MTK_DISP_DSC_H__

struct mtk_disp_dsc_data {
	bool support_shadow;
	bool need_bypass_shadow;
	bool need_obuf_sw;
	bool dsi_buffer;
	unsigned int shadow_ctrl_reg;
};

#endif
