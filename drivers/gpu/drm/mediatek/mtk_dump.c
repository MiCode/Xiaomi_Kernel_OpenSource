/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"

static const char * const ddp_comp_str[] = {DECLARE_DDP_COMP(DECLARE_STR)};

const char *mtk_dump_comp_str(struct mtk_ddp_comp *comp)
{
	if (comp->id < 0) {
		DDPPR_ERR("%s: Invalid ddp comp id:%d\n", __func__, comp->id);
		comp->id = 0;
	}
	return ddp_comp_str[comp->id];
}

const char *mtk_dump_comp_str_id(unsigned int id)
{
	if (likely(id < DDP_COMPONENT_ID_MAX))
		return ddp_comp_str[id];

	return "invalid";
}

int mtk_dump_reg(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL3_2L:
		mtk_ovl_dump(comp);
		break;
	case DDP_COMPONENT_RDMA0:
	case DDP_COMPONENT_RDMA1:
	case DDP_COMPONENT_RDMA4:
	case DDP_COMPONENT_RDMA5:
		mtk_rdma_dump(comp);
		break;
	case DDP_COMPONENT_WDMA0:
	case DDP_COMPONENT_WDMA1:
		mtk_wdma_dump(comp);
		break;
	case DDP_COMPONENT_RSZ0:
	case DDP_COMPONENT_RSZ1:
		mtk_rsz_dump(comp);
		break;
	case DDP_COMPONENT_DSI0:
	case DDP_COMPONENT_DSI1:
		mtk_dsi_dump(comp);
		break;
#ifdef CONFIG_MTK_HDMI_SUPPORT
	case DDP_COMPONENT_DP_INTF0:
		mtk_dp_intf_dump(comp);
		break;
#endif
	case DDP_COMPONENT_COLOR0:
	case DDP_COMPONENT_COLOR1:
	case DDP_COMPONENT_COLOR2:
		mtk_color_dump(comp);
		break;
	case DDP_COMPONENT_CCORR0:
	case DDP_COMPONENT_CCORR1:
		mtk_ccorr_dump(comp);
		break;
	case DDP_COMPONENT_AAL0:
	case DDP_COMPONENT_AAL1:
		mtk_aal_dump(comp);
		break;
	case DDP_COMPONENT_DMDP_AAL0:
		mtk_dmdp_aal_dump(comp);
		break;
	case DDP_COMPONENT_DITHER0:
	case DDP_COMPONENT_DITHER1:
		mtk_dither_dump(comp);
		break;
	case DDP_COMPONENT_GAMMA0:
	case DDP_COMPONENT_GAMMA1:
		mtk_gamma_dump(comp);
		break;
	case DDP_COMPONENT_POSTMASK0:
	case DDP_COMPONENT_POSTMASK1:
		mtk_postmask_dump(comp);
		break;
	case DDP_COMPONENT_DSC0:
		mtk_dsc_dump(comp);
		break;
	case DDP_COMPONENT_MERGE0:
	case DDP_COMPONENT_MERGE1:
		mtk_merge_dump(comp);
		break;
	default:
		return 0;
	}

	return 0;
}

int mtk_dump_analysis(struct mtk_ddp_comp *comp)
{
	switch (comp->id) {
	case DDP_COMPONENT_OVL0:
	case DDP_COMPONENT_OVL1:
	case DDP_COMPONENT_OVL0_2L:
	case DDP_COMPONENT_OVL1_2L:
	case DDP_COMPONENT_OVL2_2L:
	case DDP_COMPONENT_OVL3_2L:
		mtk_ovl_analysis(comp);
		break;
	case DDP_COMPONENT_RDMA0:
	case DDP_COMPONENT_RDMA1:
	case DDP_COMPONENT_RDMA4:
	case DDP_COMPONENT_RDMA5:
		mtk_rdma_analysis(comp);
		break;
	case DDP_COMPONENT_WDMA0:
	case DDP_COMPONENT_WDMA1:
		mtk_wdma_analysis(comp);
		break;
	case DDP_COMPONENT_RSZ0:
	case DDP_COMPONENT_RSZ1:
		mtk_rsz_analysis(comp);
		break;
	case DDP_COMPONENT_DSI0:
	case DDP_COMPONENT_DSI1:
		mtk_dsi_analysis(comp);
		break;
#ifdef CONFIG_MTK_HDMI_SUPPORT
	case DDP_COMPONENT_DP_INTF0:
		mtk_dp_intf_analysis(comp);
		break;
#endif
	case DDP_COMPONENT_POSTMASK0:
	case DDP_COMPONENT_POSTMASK1:
		mtk_postmask_analysis(comp);
		break;
	case DDP_COMPONENT_DSC0:
		mtk_dsc_analysis(comp);
		break;
	case DDP_COMPONENT_MERGE0:
	case DDP_COMPONENT_MERGE1:
		mtk_merge_analysis(comp);
	default:
		return 0;
	}

	return 0;
}

void mtk_serial_dump_reg(void __iomem *base, unsigned int offset,
			 unsigned int num)
{
	unsigned int max_size = 54, i = 0, s = 0, l = 0;
	char buf[max_size];

	if (num > 4)
		num = 4;

	l = snprintf(buf, max_size, "0x%03x:", offset);

	for (i = 0; i < num; i++) {
		s = snprintf(buf + l, max_size, "0x%08x ",
			     readl(base + offset + i * 0x4));
		l += s;
	}

	DDPDUMP("%s\n", buf);
}

void mtk_cust_dump_reg(void __iomem *base, int off1, int off2, int off3,
		       int off4)
{
	unsigned int max_size = 84, i = 0, s = 0, l = 0;
	int off[] = {off1, off2, off3, off4};
	char buf[max_size];

	for (i = 0; i < 4; i++) {
		if (off[i] < 0)
			break;
		s = snprintf(buf + l, max_size, "0x%03x:0x%08x ", off[i],
			     readl(base + off[i]));
		if (s < 0) {
			/* Handle snprintf() error */
			return;
		}
		l += s;
	}

	DDPDUMP("%s\n", buf);
}
