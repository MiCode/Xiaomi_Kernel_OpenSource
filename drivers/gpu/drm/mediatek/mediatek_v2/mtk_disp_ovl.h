/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_OVL_H__
#define __MTK_DISP_OVL_H__

#define DISP_REG_OVL_ADDR_BASE 0x0f40

struct mtk_disp_ovl_data {
	unsigned int addr;
	unsigned int el_addr_offset;
	unsigned int el_hdr_addr;
	unsigned int el_hdr_addr_offset;
	bool fmt_rgb565_is_0;
	unsigned int fmt_uyvy;
	unsigned int fmt_yuyv;
	const struct compress_info *compr_info;
	bool support_shadow;
	bool need_bypass_shadow;
	/* golden setting */
	unsigned int preultra_th_dc;
	unsigned int fifo_size;
	unsigned int issue_req_th_dl;
	unsigned int issue_req_th_dc;
	unsigned int issue_req_th_urg_dl;
	unsigned int issue_req_th_urg_dc;
	unsigned int greq_num_dl;
	bool is_support_34bits;
	unsigned int (*aid_sel_mapping)(struct mtk_ddp_comp *comp);
	resource_size_t (*mmsys_mapping)(struct mtk_ddp_comp *comp);
	unsigned int source_bpc;
};

struct compress_info {
	/* naming rule: tech_version_MTK_sub-version,
	 * i.e.: PVRIC_V3_1_MTK_1
	 * sub-version is used when compression version is the same
	 * but mtk decoder is different among platforms.
	 */
	const char name[25];

	bool (*l_config)(struct mtk_ddp_comp *comp,
			unsigned int idx, struct mtk_plane_state *state,
			struct cmdq_pkt *handle);
};

bool compr_l_config_AFBC_V1_2(struct mtk_ddp_comp *comp,
			unsigned int idx, struct mtk_plane_state *state,
			struct cmdq_pkt *handle);
#endif
