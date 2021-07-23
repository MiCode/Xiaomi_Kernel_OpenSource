/*
 * Copyright (c) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Authors:
 *	YT Shen <yt.shen@mediatek.com>
 *	CK Hu <ck.hu@mediatek.com>
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

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <drm/drmP.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <soc/mediatek/smi.h>
#ifdef CONFIG_MTK_IOMMU_V2
#include "mt_iommu.h"
#include "mtk_iommu_ext.h"
#endif

#include "mtk_drm_drv.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_gem.h"
#include "mtk_dump.h"

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

#define DISP_OD_EN 0x0000
#define DISP_OD_INTEN 0x0008
#define DISP_OD_INTSTA 0x000c
#define DISP_OD_CFG 0x0020
#define DISP_OD_SIZE 0x0030
#define DISP_DITHER_5 0x0114
#define DISP_DITHER_7 0x011c
#define DISP_DITHER_15 0x013c
#define DISP_DITHER_16 0x0140

#define DISP_REG_SPLIT_START 0x0000

#define DISP_REG_UFO_START 0x0000
#define DISP_REG_UFO_WIDTH 0x0050
#define DISP_REG_UFO_HEIGHT 0x0054

#define OD_RELAYMODE BIT(0)

#define UFO_BYPASS BIT(2)
#define UFO_LR (BIT(3) | BIT(0))

#define DISP_DITHERING BIT(2)
#define DITHER_LSB_ERR_SHIFT_R(x) (((x)&0x7) << 28)
#define DITHER_OVFLW_BIT_R(x) (((x)&0x7) << 24)
#define DITHER_ADD_LSHIFT_R(x) (((x)&0x7) << 20)
#define DITHER_ADD_RSHIFT_R(x) (((x)&0x7) << 16)
#define DITHER_NEW_BIT_MODE BIT(0)
#define DITHER_LSB_ERR_SHIFT_B(x) (((x)&0x7) << 28)
#define DITHER_OVFLW_BIT_B(x) (((x)&0x7) << 24)
#define DITHER_ADD_LSHIFT_B(x) (((x)&0x7) << 20)
#define DITHER_ADD_RSHIFT_B(x) (((x)&0x7) << 16)
#define DITHER_LSB_ERR_SHIFT_G(x) (((x)&0x7) << 12)
#define DITHER_OVFLW_BIT_G(x) (((x)&0x7) << 8)
#define DITHER_ADD_LSHIFT_G(x) (((x)&0x7) << 4)
#define DITHER_ADD_RSHIFT_G(x) (((x)&0x7) << 0)

#define MMSYS_SODI_REQ_MASK	0xF4
#define SODI_REQ_SEL_ALL			REG_FLD_MSB_LSB(11, 8)
#define MT6873_SODI_REQ_SEL_ALL			REG_FLD_MSB_LSB(9, 8)
#define SODI_REQ_SEL_RDMA0_PD_MODE	REG_FLD_MSB_LSB(8, 8)
#define SODI_REQ_SEL_RDMA0_CG_MODE	REG_FLD_MSB_LSB(9, 9)
#define SODI_REQ_SEL_RDMA1_PD_MODE	REG_FLD_MSB_LSB(10, 10)
#define SODI_REQ_SEL_RDMA1_CG_MODE	REG_FLD_MSB_LSB(11, 11)

#define SODI_REQ_VAL_ALL			REG_FLD_MSB_LSB(15, 12)
#define MT6873_SODI_REQ_VAL_ALL			REG_FLD_MSB_LSB(13, 12)
#define SODI_REQ_VAL_RDMA0_PD_MODE	REG_FLD_MSB_LSB(12, 12)
#define SODI_REQ_VAL_RDMA0_CG_MODE	REG_FLD_MSB_LSB(13, 13)
#define SODI_REQ_VAL_RDMA1_PD_MODE	REG_FLD_MSB_LSB(14, 14)
#define SODI_REQ_VAL_RDMA1_CG_MODE	REG_FLD_MSB_LSB(15, 15)

#define MMSYS_EMI_REQ_CTL	0xF8
#define HRT_URGENT_CTL_SEL_ALL		REG_FLD_MSB_LSB(7, 0)
#define HRT_URGENT_CTL_SEL_RDMA0	REG_FLD_MSB_LSB(0, 0)
#define HRT_URGENT_CTL_SEL_WDMA0	REG_FLD_MSB_LSB(1, 1)
#define HRT_URGENT_CTL_SEL_RDMA1	REG_FLD_MSB_LSB(2, 2)
#define HRT_URGENT_CTL_SEL_WDMA1	REG_FLD_MSB_LSB(3, 3)
#define HRT_URGENT_CTL_SEL_RDMA4	REG_FLD_MSB_LSB(4, 4)
#define HRT_URGENT_CTL_SEL_RDMA5	REG_FLD_MSB_LSB(5, 5)
#define HRT_URGENT_CTL_SEL_MDP_RDMA4	REG_FLD_MSB_LSB(6, 6)

#define HRT_URGENT_CTL_VAL_ALL		REG_FLD_MSB_LSB(16, 9)
#define HRT_URGENT_CTL_VAL_RDMA0		REG_FLD_MSB_LSB(9, 9)
#define HRT_URGENT_CTL_VAL_WDMA0		REG_FLD_MSB_LSB(10, 10)
#define HRT_URGENT_CTL_VAL_RDMA4		REG_FLD_MSB_LSB(13, 13)
#define HRT_URGENT_CTL_VAL_MDP_RDMA4		REG_FLD_MSB_LSB(15, 15)

#define DVFS_HALT_MASK_SEL_ALL		REG_FLD_MSB_LSB(23, 18)
#define DVFS_HALT_MASK_SEL_RDMA0	REG_FLD_MSB_LSB(18, 18)
#define DVFS_HALT_MASK_SEL_RDMA1	REG_FLD_MSB_LSB(19, 19)
#define DVFS_HALT_MASK_SEL_RDMA4	REG_FLD_MSB_LSB(20, 20)
#define DVFS_HALT_MASK_SEL_RDMA5	REG_FLD_MSB_LSB(21, 21)
#define DVFS_HALT_MASK_SEL_WDMA0	REG_FLD_MSB_LSB(22, 22)
#define DVFS_HALT_MASK_SEL_WDMA1	REG_FLD_MSB_LSB(23, 23)

#define MT6833_INFRA_DISP_DDR_CTL  0x2C
#define MT6833_INFRA_FLD_DDR_MASK  REG_FLD_MSB_LSB(7, 4)

#define SMI_LARB_NON_SEC_CON 0x0380

#define MTK_DDP_COMP_USER "DISP"


void mtk_ddp_write(struct mtk_ddp_comp *comp, unsigned int value,
		   unsigned int offset, void *handle)
{
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
		       comp->regs_pa + offset, value, ~0);
#else
	writel(value, comp->regs + offset);
#endif
}

void mtk_ddp_write_relaxed(struct mtk_ddp_comp *comp, unsigned int value,
			   unsigned int offset, void *handle)
{
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
		       comp->regs_pa + offset, value, ~0);
#else
	writel_relaxed(value, comp->regs + offset);
#endif
}

void mtk_ddp_write_mask(struct mtk_ddp_comp *comp, unsigned int value,
			unsigned int offset, unsigned int mask, void *handle)
{
#ifdef CONFIG_MTK_DISPLAY_CMDQ
	cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
		       comp->regs_pa + offset, value, mask);
#else
	unsigned int tmp = readl(comp->regs + offset);

	tmp = (tmp & ~mask) | (value & mask);
	writel(tmp, comp->regs + offset);
#endif
}

void mtk_ddp_write_mask_cpu(struct mtk_ddp_comp *comp,
	unsigned int value, unsigned int offset, unsigned int mask)
{
	unsigned int tmp = readl(comp->regs + offset);

	tmp = (tmp & ~mask) | (value & mask);
	writel(tmp, comp->regs + offset);
}

void mtk_dither_set(struct mtk_ddp_comp *comp, unsigned int bpc,
		    unsigned int CFG, struct cmdq_pkt *handle)
{
	/* If bpc equal to 0, the dithering function didn't be enabled */
	if (bpc == 0)
		return;

	if (bpc >= MTK_MIN_BPC) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_DITHER_5, 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_DITHER_7, 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_DITHER_15,
			       DITHER_LSB_ERR_SHIFT_R(MTK_MAX_BPC - bpc) |
				       DITHER_ADD_LSHIFT_R(MTK_MAX_BPC - bpc) |
				       DITHER_NEW_BIT_MODE,
			       ~0);
		cmdq_pkt_write(
			handle, comp->cmdq_base, comp->regs_pa + DISP_DITHER_16,
			DITHER_LSB_ERR_SHIFT_B(MTK_MAX_BPC - bpc) |
				DITHER_ADD_LSHIFT_B(MTK_MAX_BPC - bpc) |
				DITHER_LSB_ERR_SHIFT_G(MTK_MAX_BPC - bpc) |
				DITHER_ADD_LSHIFT_G(MTK_MAX_BPC - bpc),
			~0);
		cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + CFG,
			       DISP_DITHERING, ~0);
	}
}

static void mtk_od_config(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg,
			  struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_OD_SIZE,
		       cfg->w << 16 | cfg->h, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_OD_CFG,
		       OD_RELAYMODE, ~0);
	mtk_dither_set(comp, cfg->bpc, DISP_OD_CFG, handle);
}

static void mtk_od_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DISP_OD_EN, 1,
		       ~0);
}

static void mtk_ufoe_config(struct mtk_ddp_comp *comp,
			    struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_UFO_WIDTH, cfg->w, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_UFO_HEIGHT, cfg->h, ~0);
}

static void mtk_ufoe_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_UFO_START, UFO_BYPASS, ~0);
}

static void mtk_split_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_SPLIT_START, 1, ~0);
}

static const struct mtk_ddp_comp_funcs ddp_od = {
	.config = mtk_od_config, .start = mtk_od_start,
};

static const struct mtk_ddp_comp_funcs ddp_ufoe = {
	.start = mtk_ufoe_start, .config = mtk_ufoe_config,
};

static const struct mtk_ddp_comp_funcs ddp_split = {
	.start = mtk_split_start,
};

static const char *const mtk_ddp_comp_stem[MTK_DDP_COMP_TYPE_MAX] = {
	[MTK_DISP_OVL] = "ovl",
	[MTK_DISP_RDMA] = "rdma",
	[MTK_DISP_WDMA] = "wdma",
	[MTK_DISP_COLOR] = "color",
	[MTK_DISP_CCORR] = "ccorr",
	[MTK_DISP_AAL] = "aal",
	[MTK_DISP_GAMMA] = "gamma",
	[MTK_DISP_DITHER] = "dither",
	[MTK_DISP_UFOE] = "ufoe",
	[MTK_DSI] = "dsi",
	[MTK_DP_INTF] = "dp_intf",
	[MTK_DPI] = "dpi",
	[MTK_DISP_PWM] = "pwm",
	[MTK_DISP_MUTEX] = "mutex",
	[MTK_DISP_OD] = "od",
	[MTK_DISP_BLS] = "bls",
	[MTK_DISP_RSZ] = "rsz",
	[MTK_DISP_POSTMASK] = "postmask",
	[MTK_DMDP_RDMA] = "mrdma",
	[MTK_DMDP_HDR] = "mhdr",
	[MTK_DMDP_AAL] = "maal",
	[MTK_DMDP_RSZ] = "mrsz",
	[MTK_DMDP_TDSHP] = "mtdshp",
	[MTK_DISP_DSC] = "dsc",
	[MTK_DISP_MERGE] = "merge",
	[MTK_DISP_DPTX] = "dptx",
	[MTK_DISP_VIRTUAL] = "virtual",
};

struct mtk_ddp_comp_match {
	enum mtk_ddp_comp_id index;
	enum mtk_ddp_comp_type type;
	int alias_id;
	const struct mtk_ddp_comp_funcs *funcs;
	bool is_output;
};

static const struct mtk_ddp_comp_match mtk_ddp_matches[DDP_COMPONENT_ID_MAX] = {
	{DDP_COMPONENT_AAL0, MTK_DISP_AAL, 0, NULL, 0},
	{DDP_COMPONENT_AAL1, MTK_DISP_AAL, 1, NULL, 0},
	{DDP_COMPONENT_BLS, MTK_DISP_BLS, 0, NULL, 0},
	{DDP_COMPONENT_CCORR0, MTK_DISP_CCORR, 0, NULL, 0},
	{DDP_COMPONENT_CCORR1, MTK_DISP_CCORR, 1, NULL, 0},
	{DDP_COMPONENT_COLOR0, MTK_DISP_COLOR, 0, NULL, 0},
	{DDP_COMPONENT_COLOR1, MTK_DISP_COLOR, 1, NULL, 0},
	{DDP_COMPONENT_COLOR2, MTK_DISP_COLOR, 2, NULL, 0},
	{DDP_COMPONENT_DITHER0, MTK_DISP_DITHER, 0, NULL, 0},
	{DDP_COMPONENT_DITHER1, MTK_DISP_DITHER, 1, NULL, 0},
	{DDP_COMPONENT_DPI0, MTK_DPI, 0, NULL, 1},
	{DDP_COMPONENT_DPI1, MTK_DPI, 1, NULL, 1},
	{DDP_COMPONENT_DSI0, MTK_DSI, 0, NULL, 1},
	{DDP_COMPONENT_DSI1, MTK_DSI, 1, NULL, 1},
	{DDP_COMPONENT_GAMMA0, MTK_DISP_GAMMA, 0, NULL, 0},
	{DDP_COMPONENT_GAMMA1, MTK_DISP_GAMMA, 0, NULL, 0},
	{DDP_COMPONENT_OD, MTK_DISP_OD, 0, &ddp_od, 0},
	{DDP_COMPONENT_OD1, MTK_DISP_OD, 1, &ddp_od, 0},
	{DDP_COMPONENT_OVL0, MTK_DISP_OVL, 0, NULL, 0},
	{DDP_COMPONENT_OVL1, MTK_DISP_OVL, 1, NULL, 0},
	{DDP_COMPONENT_OVL2, MTK_DISP_OVL, 2, NULL, 0},
	{DDP_COMPONENT_OVL0_2L, MTK_DISP_OVL, 3, NULL, 0},
	{DDP_COMPONENT_OVL1_2L, MTK_DISP_OVL, 4, NULL, 0},
	{DDP_COMPONENT_OVL2_2L, MTK_DISP_OVL, 5, NULL, 0},
	{DDP_COMPONENT_OVL3_2L, MTK_DISP_OVL, 6, NULL, 0},
	{DDP_COMPONENT_OVL0_2L_VIRTUAL0, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_OVL1_2L_VIRTUAL0, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_OVL0_VIRTUAL0, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_OVL1_VIRTUAL0, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_OVL0_OVL0_2L_VIRTUAL0, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_PWM0, MTK_DISP_PWM, 0, NULL, 0},
	{DDP_COMPONENT_PWM1, MTK_DISP_PWM, 1, NULL, 0},
	{DDP_COMPONENT_PWM2, MTK_DISP_PWM, 2, NULL, 0},
	{DDP_COMPONENT_RDMA0, MTK_DISP_RDMA, 0, NULL, 0},
	{DDP_COMPONENT_RDMA1, MTK_DISP_RDMA, 1, NULL, 0},
	{DDP_COMPONENT_RDMA2, MTK_DISP_RDMA, 2, NULL, 0},
	{DDP_COMPONENT_RDMA3, MTK_DISP_RDMA, 3, NULL, 0},
	{DDP_COMPONENT_RDMA4, MTK_DISP_RDMA, 4, NULL, 0},
	{DDP_COMPONENT_RDMA5, MTK_DISP_RDMA, 5, NULL, 0},
	{DDP_COMPONENT_RDMA0_VIRTUAL0, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_RDMA1_VIRTUAL0, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_RDMA2_VIRTUAL0, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_RSZ0, MTK_DISP_RSZ, 0, NULL, 0},
	{DDP_COMPONENT_RSZ1, MTK_DISP_RSZ, 1, NULL, 0},
	{DDP_COMPONENT_UFOE, MTK_DISP_UFOE, 0, &ddp_ufoe, 0},
	{DDP_COMPONENT_WDMA0, MTK_DISP_WDMA, 0, NULL, 1},
	{DDP_COMPONENT_WDMA1, MTK_DISP_WDMA, 1, NULL, 1},
	{DDP_COMPONENT_UFBC_WDMA0, MTK_DISP_WDMA, 2, NULL, 1},
	{DDP_COMPONENT_WDMA_VIRTUAL0, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_WDMA_VIRTUAL1, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_POSTMASK0, MTK_DISP_POSTMASK, 0, NULL, 0},
	{DDP_COMPONENT_POSTMASK1, MTK_DISP_POSTMASK, 1, NULL, 0},
	{DDP_COMPONENT_DMDP_RDMA0, MTK_DMDP_RDMA, 0, NULL, 0},
	{DDP_COMPONENT_DMDP_HDR0, MTK_DMDP_HDR, 0, NULL, 0},
	{DDP_COMPONENT_DMDP_AAL0, MTK_DMDP_AAL, 0, NULL, 0},
	{DDP_COMPONENT_DMDP_RSZ0, MTK_DMDP_RSZ, 0, NULL, 0},
	{DDP_COMPONENT_DMDP_TDSHP0, MTK_DMDP_TDSHP, 0, NULL, 0},
	{DDP_COMPONENT_DSC0, MTK_DISP_DSC, 0, NULL, 0},
	{DDP_COMPONENT_MERGE0, MTK_DISP_MERGE, 0, NULL, 0},
	{DDP_COMPONENT_DPTX, MTK_DISP_DPTX, 0, NULL, 1},
	{DDP_COMPONENT_DP_INTF0, MTK_DP_INTF, 0, NULL, 1},
	{DDP_COMPONENT_RDMA4_VIRTUAL0, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_RDMA5_VIRTUAL0, MTK_DISP_VIRTUAL, -1, NULL, 0},
	{DDP_COMPONENT_MERGE1, MTK_DISP_MERGE, 1, NULL, 0},
	{DDP_COMPONENT_SPR0_VIRTUAL, MTK_DISP_VIRTUAL, -1, NULL, 0},
};

bool mtk_ddp_comp_is_output(struct mtk_ddp_comp *comp)
{
	if (comp->id < 0 || comp->id >= DDP_COMPONENT_ID_MAX)
		return false;

	return mtk_ddp_matches[comp->id].is_output;
}

void mtk_ddp_comp_get_name(struct mtk_ddp_comp *comp, char *buf, int buf_len)
{
	int r;

	if (comp->id < 0 || comp->id >= DDP_COMPONENT_ID_MAX) {
		DDPPR_ERR("%s(), invalid id %d, set buf to 0\n",
			  __func__, comp->id);
		memset(buf, 0, buf_len);
		return;
	}

	if (buf_len > sizeof(buf))
		buf_len = sizeof(buf);

	r = snprintf(buf, buf_len, "%s%d",
		  mtk_ddp_comp_stem[mtk_ddp_matches[comp->id].type],
		  mtk_ddp_matches[comp->id].alias_id);
	if (r < 0) {
		/* Handle snprintf() error */
		DDPPR_ERR("snprintf error\n");
	}
}

int mtk_ddp_comp_get_type(enum mtk_ddp_comp_id comp_id)
{
	if (comp_id < 0 || comp_id >= DDP_COMPONENT_ID_MAX)
		return -EINVAL;

	return mtk_ddp_matches[comp_id].type;
}

static bool mtk_drm_find_comp_in_ddp(struct mtk_ddp_comp ddp_comp,
				     const struct mtk_crtc_path_data *path_data)
{
	unsigned int i, j, ddp_mode;
	const enum mtk_ddp_comp_id *path = NULL;

	if (path_data == NULL)
		return false;

	for (ddp_mode = 0U; ddp_mode < DDP_MODE_NR; ddp_mode++)
		for (i = 0U; i < DDP_PATH_NR; i++) {
			path = path_data->path[ddp_mode][i];
			for (j = 0U; j < path_data->path_len[ddp_mode][i]; j++)
				if (ddp_comp.id == path[j])
					return true;
		}

	return false;
}

enum mtk_ddp_comp_id mtk_ddp_comp_get_id(struct device_node *node,
					 enum mtk_ddp_comp_type comp_type)
{
	int id;
	int i;

	if (comp_type < 0)
		return -EINVAL;

	id = of_alias_get_id(node, mtk_ddp_comp_stem[comp_type]);

	DDPINFO("id:%d, comp_type:%d\n", id, comp_type);
	for (i = 0; i < ARRAY_SIZE(mtk_ddp_matches); i++) {
		if (comp_type == mtk_ddp_matches[i].type &&
		    (id < 0 || id == mtk_ddp_matches[i].alias_id))
			return mtk_ddp_matches[i].index;
	}

	return -EINVAL;
}

struct mtk_ddp_comp *mtk_ddp_comp_find_by_id(struct drm_crtc *crtc,
					     enum mtk_ddp_comp_id comp_id)
{
	unsigned int i = 0, j = 0, ddp_mode = 0;
	struct mtk_drm_crtc *mtk_crtc =
		container_of(crtc, struct mtk_drm_crtc, base);
	struct mtk_ddp_comp *comp;

	for_each_comp_in_all_crtc_mode(comp, mtk_crtc, i, j,
				       ddp_mode)
		if (comp_id == comp->id)
			return comp;

	return NULL;
}

static void mtk_ddp_comp_set_larb(struct device *dev, struct device_node *node,
				  struct mtk_ddp_comp *comp)
{
	int ret;
	struct device_node *larb_node = NULL;
	struct platform_device *larb_pdev = NULL;
	enum mtk_ddp_comp_type type = mtk_ddp_comp_get_type(comp->id);
	unsigned int larb_id;

	comp->larb_dev = NULL;

	larb_node = of_parse_phandle(node, "mediatek,larb", 0);

	if (larb_node) {
		larb_pdev = of_find_device_by_node(larb_node);
		if (larb_pdev)
			comp->larb_dev = &larb_pdev->dev;
		of_node_put(larb_node);
	}

	if (!comp->larb_dev)
		return;

	ret = of_property_read_u32(node,
				"mediatek,smi-id", &larb_id);
	if (ret) {
		dev_err(comp->larb_dev,
			"read smi-id failed:%d\n", ret);
		return;
	}
	comp->larb_id = larb_id;

	/* check if this module need larb_dev */
	if (type == MTK_DISP_OVL || type == MTK_DISP_RDMA ||
	    type == MTK_DISP_WDMA || type == MTK_DISP_POSTMASK) {
		dev_warn(dev, "%s: %s need larb device\n", __func__,
				mtk_dump_comp_str(comp));
		DDPPR_ERR("%s: smi-id:%d\n", mtk_dump_comp_str(comp),
				comp->larb_id);
	}
}

unsigned int mtk_drm_find_possible_crtc_by_comp(struct drm_device *drm,
						struct mtk_ddp_comp ddp_comp)
{
	struct mtk_drm_private *private = drm->dev_private;
	unsigned int ret;

	if (mtk_drm_find_comp_in_ddp(ddp_comp, private->data->main_path_data) ==
	    true) {
		ret = BIT(0);
	} else if (mtk_drm_find_comp_in_ddp(
			   ddp_comp, private->data->ext_path_data) == true) {
		ret = BIT(1);
	} else if (mtk_drm_find_comp_in_ddp(
			   ddp_comp, private->data->third_path_data) == true) {
		ret = BIT(2);
	} else {
		DRM_INFO("Failed to find comp in ddp table\n");
		ret = 0;
	}

	return ret;
}

int mtk_ddp_comp_init(struct device *dev, struct device_node *node,
		      struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id comp_id,
		      const struct mtk_ddp_comp_funcs *funcs)
{
	enum mtk_ddp_comp_type type;
	struct platform_device *comp_pdev = NULL;
	struct resource res;

	DDPINFO("%s+\n", __func__);

	if (comp_id < 0 || comp_id >= DDP_COMPONENT_ID_MAX)
		return -EINVAL;

	type = mtk_ddp_matches[comp_id].type;

	comp->id = comp_id;
	comp->funcs = funcs ?: mtk_ddp_matches[comp_id].funcs;
	comp->dev = dev;

	/* get the first clk in the device node */
	comp->clk = of_clk_get(node, 0);
	if (IS_ERR(comp->clk)) {
		comp->clk = NULL;
		DDPPR_ERR("comp:%d get clock fail!\n", comp_id);
	}

	if (comp_id == DDP_COMPONENT_BLS || comp_id == DDP_COMPONENT_PWM0) {
		comp->regs_pa = 0;
		comp->regs = NULL;
		comp->irq = 0;
		return 0;
	}

	if (of_address_to_resource(node, 0, &res) != 0) {
		dev_err(dev, "Missing reg in %s node\n", node->full_name);
		return -EINVAL;
	}

	comp->regs_pa = res.start;

	if (comp_id == DDP_COMPONENT_DPI0 || comp_id == DDP_COMPONENT_DPI1 ||
	    comp_id == DDP_COMPONENT_DSI0 || comp_id == DDP_COMPONENT_DSI1)
		comp->irq = 0;
	else
		comp->irq = of_irq_get(node, 0);

	comp->regs = of_iomap(node, 0);
	DDPINFO("[DRM]regs_pa:0x%lx, regs:0x%p, node:%s\n",
		(unsigned long)comp->regs_pa, comp->regs, node->full_name);

	/* handle cmdq related resources */
	comp_pdev = of_find_device_by_node(node);
	if (!comp_pdev) {
		dev_warn(dev, "Waiting for comp device %s\n", node->full_name);
		return -EPROBE_DEFER;
	}

	comp->cmdq_base = cmdq_register_device(&comp_pdev->dev);

#if 0
	/* TODO: if no subsys id, use 99 instead. CMDQ owner would define 99 in
	 * DTS afterward.
	 */
	if (of_property_read_u8(node, "my_subsys_id", &comp->cmdq_subsys))
		comp->cmdq_subsys = 99;
#endif

	/* handle larb resources */
	mtk_ddp_comp_set_larb(dev, node, comp);

	DDPINFO("%s-\n", __func__);

	return 0;
}

int mtk_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *private = drm->dev_private;

	if (private->ddp_comp[comp->id])
		return -EBUSY;

	if (comp->id < 0)
		return -EINVAL;

	private->ddp_comp[comp->id] = comp;
	return 0;
}

void mtk_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *private = drm->dev_private;

	private->ddp_comp[comp->id] = NULL;
}

void mtk_ddp_comp_clk_prepare(struct mtk_ddp_comp *comp)
{
	int ret;

	if (comp == NULL)
		return;

#ifdef CONFIG_MTK_SMI_EXT
	if (comp->larb_dev)
		smi_bus_prepare_enable(comp->larb_id, MTK_DDP_COMP_USER);
#endif

	if (comp->clk) {
		ret = clk_prepare_enable(comp->clk);
		if (ret)
			DDPPR_ERR("clk prepare enable failed:%s\n",
				mtk_dump_comp_str(comp));
	}
}

void mtk_ddp_comp_clk_unprepare(struct mtk_ddp_comp *comp)
{
	if (comp == NULL)
		return;

	if (comp->clk)
		clk_disable_unprepare(comp->clk);

#ifdef CONFIG_MTK_SMI_EXT
	if (comp->larb_dev)
		smi_bus_disable_unprepare(comp->larb_id, MTK_DDP_COMP_USER);
#endif
}

#ifdef CONFIG_MTK_IOMMU_V2
static enum mtk_iommu_callback_ret_t
	mtk_ddp_m4u_callback(int port, unsigned long mva,
				void *data)
{
	struct mtk_ddp_comp *comp = (struct mtk_ddp_comp *)data;

	DDPPR_ERR("fault call port=%d, mva=0x%lx, data=0x%p\n", port, mva,
		  data);

	if (comp) {
		mtk_dump_analysis(comp);
		mtk_dump_reg(comp);
	}

	return MTK_IOMMU_CALLBACK_HANDLED;
}
#endif

#define GET_M4U_PORT 0x1F
void mtk_ddp_comp_iommu_enable(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle)
{
	int port, index, ret;
	struct resource res;

	if (!comp->dev || !comp->larb_dev)
		return;

	index = 0;
	while (1) {
		ret = of_property_read_u32_index(comp->dev->of_node,
				"iommus", index * 2 + 1, &port);
		if (ret < 0)
			break;

#ifdef CONFIG_MTK_IOMMU_V2
		mtk_iommu_register_fault_callback(
			port, (mtk_iommu_fault_callback_t)mtk_ddp_m4u_callback,
			(void *)comp);
#endif

		port &= (unsigned int)GET_M4U_PORT;
		if (of_address_to_resource(comp->larb_dev->of_node, 0, &res) !=
		    0) {
			dev_err(comp->dev, "Missing reg in %s node\n",
				comp->larb_dev->of_node->full_name);
			return;
		}
		cmdq_pkt_write(handle, NULL,
			       res.start + SMI_LARB_NON_SEC_CON + port * 4, 0x1,
			       0x1);
		index++;
	}
}

void mt6779_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data)
{
	struct mtk_drm_private *priv = drm->dev_private;
	unsigned int val = 0, mask = 0;
	bool en = *((bool *)data);

	if (id == DDP_COMPONENT_ID_MAX) { /* config when top clk on */
		if (!en)
			return;
		val = 0x0F005506;
		mask = 0xFFFFFFFF;
	} else if (id == DDP_COMPONENT_RDMA0) {
		mask |= (BIT(9) + BIT(16));
		val |= (((!(unsigned int)en) << 9) + ((en) << 16));
	} else if (id == DDP_COMPONENT_RDMA1) {
		mask |= (BIT(11) + BIT(17));
		val |= (((!(unsigned int)en) << 11) + ((en) << 17));
	} else if (id == DDP_COMPONENT_WDMA0) {
		mask |= BIT(18);
		val |= ((en) << 18);
	} else
		return;

	if (handle == NULL) {
		unsigned int v = (readl(priv->config_regs + 0xF8) & (~mask));

		v += (val & mask);
		writel_relaxed(v, priv->config_regs + 0xF8);
	} else
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa + 0xF8, val,
			       mask);
}

void mt6853_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data)
{
	struct mtk_drm_private *priv = drm->dev_private;
	unsigned int sodi_req_val = 0, sodi_req_mask = 0;
	unsigned int emi_req_val = 0, emi_req_mask = 0;
	bool en = *((bool *)data);

	if (id == DDP_COMPONENT_ID_MAX) { /* config when top clk on */
		if (!en)
			return;

		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					0, MT6873_SODI_REQ_SEL_ALL);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					0, MT6873_SODI_REQ_VAL_ALL);

		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_SEL_RDMA0_PD_MODE);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_VAL_RDMA0_PD_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0x1, HRT_URGENT_CTL_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0x1, HRT_URGENT_CTL_SEL_WDMA0);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_WDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_RDMA4);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_MDP_RDMA4);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_RDMA4);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_WDMA0);
	} else if (id == DDP_COMPONENT_RDMA0) {
		SET_VAL_MASK(sodi_req_val, sodi_req_mask, (!en),
					SODI_REQ_SEL_RDMA0_CG_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask, (!en),
				HRT_URGENT_CTL_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
				DVFS_HALT_MASK_SEL_RDMA0);
	} else if (id == DDP_COMPONENT_WDMA0) {
		SET_VAL_MASK(emi_req_val, emi_req_mask, (!en),
					HRT_URGENT_CTL_SEL_WDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
					DVFS_HALT_MASK_SEL_WDMA0);
	} else
		return;

	if (handle == NULL) {
		unsigned int v;

		v = (readl(priv->config_regs + MMSYS_SODI_REQ_MASK)
			& (~sodi_req_mask));
		v += (sodi_req_val & sodi_req_mask);
		writel_relaxed(v, priv->config_regs + MMSYS_SODI_REQ_MASK);

		v = (readl(priv->config_regs +  MMSYS_EMI_REQ_CTL)
			& (~emi_req_mask));
		v += (emi_req_val & emi_req_mask);
		writel_relaxed(v, priv->config_regs +  MMSYS_EMI_REQ_CTL);
	} else {
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_SODI_REQ_MASK, sodi_req_val, sodi_req_mask);
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_EMI_REQ_CTL, emi_req_val, emi_req_mask);
	}
}

void mt6833_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data)
{
	struct mtk_drm_private *priv = drm->dev_private;
	unsigned int sodi_req_val = 0, sodi_req_mask = 0;
	unsigned int emi_req_val = 0, emi_req_mask = 0;
	unsigned int infra_req_val = 0, infra_req_mask = 0;
	bool en = *((bool *)data);

	if (id == DDP_COMPONENT_ID_MAX) { /* config when top clk on */
		if (!en)
			return;

		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					0, MT6873_SODI_REQ_SEL_ALL);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					0, MT6873_SODI_REQ_VAL_ALL);

		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_SEL_RDMA0_PD_MODE);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_VAL_RDMA0_PD_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0x1, HRT_URGENT_CTL_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0x1, HRT_URGENT_CTL_SEL_WDMA0);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_WDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_RDMA4);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_MDP_RDMA4);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_RDMA4);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_WDMA0);
	} else if (id == DDP_COMPONENT_RDMA0) {
		SET_VAL_MASK(sodi_req_val, sodi_req_mask, (!en),
					SODI_REQ_SEL_RDMA0_CG_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask, (!en),
				HRT_URGENT_CTL_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
				DVFS_HALT_MASK_SEL_RDMA0);
	} else if (id == DDP_COMPONENT_WDMA0) {
		SET_VAL_MASK(emi_req_val, emi_req_mask, (!en),
					HRT_URGENT_CTL_SEL_WDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
					DVFS_HALT_MASK_SEL_WDMA0);
	} else
		return;

	if (priv->data->bypass_infra_ddr_control)
		SET_VAL_MASK(infra_req_val, infra_req_mask,
				0xf, MT6833_INFRA_FLD_DDR_MASK);

	if (handle == NULL) {
		unsigned int v;

		v = (readl(priv->config_regs + MMSYS_SODI_REQ_MASK)
			& (~sodi_req_mask));
		v += (sodi_req_val & sodi_req_mask);
		writel_relaxed(v, priv->config_regs + MMSYS_SODI_REQ_MASK);

		v = (readl(priv->config_regs +  MMSYS_EMI_REQ_CTL)
			& (~emi_req_mask));
		v += (emi_req_val & emi_req_mask);
		writel_relaxed(v, priv->config_regs +  MMSYS_EMI_REQ_CTL);
		if (priv->data->bypass_infra_ddr_control) {
			if (!IS_ERR(priv->infra_regs)) {
				v = (readl(priv->infra_regs + MT6833_INFRA_DISP_DDR_CTL)
					| MT6833_INFRA_FLD_DDR_MASK);
				writel_relaxed(v, priv->infra_regs + MT6833_INFRA_DISP_DDR_CTL);
			} else
				DDPINFO("%s: failed to disable infra ddr control\n", __func__);
		}
	} else {
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_SODI_REQ_MASK, sodi_req_val, sodi_req_mask);
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_EMI_REQ_CTL, emi_req_val, emi_req_mask);
		if (priv->data->bypass_infra_ddr_control) {
			if (priv->infra_regs_pa) {
				cmdq_pkt_write(handle, NULL,  priv->infra_regs_pa +
						MT6833_INFRA_DISP_DDR_CTL,
						infra_req_val, infra_req_mask);
			} else
				DDPINFO("%s: failed to disable infra ddr control\n", __func__);
		}
	}
}

void mt6873_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data)
{
	struct mtk_drm_private *priv = drm->dev_private;
	unsigned int sodi_req_val = 0, sodi_req_mask = 0;
	unsigned int emi_req_val = 0, emi_req_mask = 0;
	bool en = *((bool *)data);

	if (id == DDP_COMPONENT_ID_MAX) { /* config when top clk on */
		if (!en)
			return;

		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					0, MT6873_SODI_REQ_SEL_ALL);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					0, MT6873_SODI_REQ_VAL_ALL);

		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_SEL_RDMA0_PD_MODE);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_VAL_RDMA0_PD_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0x1, HRT_URGENT_CTL_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0x1, HRT_URGENT_CTL_SEL_WDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0x1, HRT_URGENT_CTL_SEL_RDMA4);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0x1, HRT_URGENT_CTL_SEL_MDP_RDMA4);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_WDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_RDMA4);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_MDP_RDMA4);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_RDMA4);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_WDMA0);
	} else if (id == DDP_COMPONENT_RDMA0) {
		SET_VAL_MASK(sodi_req_val, sodi_req_mask, (!(unsigned int)en),
					SODI_REQ_SEL_RDMA0_CG_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask, (!(unsigned int)en),
				HRT_URGENT_CTL_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
				DVFS_HALT_MASK_SEL_RDMA0);
	} else if (id == DDP_COMPONENT_RDMA4) {
		SET_VAL_MASK(emi_req_val, emi_req_mask, (!(unsigned int)en),
					HRT_URGENT_CTL_SEL_RDMA4);
		SET_VAL_MASK(emi_req_val, emi_req_mask, (unsigned int)en,
					DVFS_HALT_MASK_SEL_RDMA4);
	} else if (id == DDP_COMPONENT_WDMA0) {
		SET_VAL_MASK(emi_req_val, emi_req_mask, (!(unsigned int)en),
					HRT_URGENT_CTL_SEL_WDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
					DVFS_HALT_MASK_SEL_WDMA0);
	} else
		return;

	if (handle == NULL) {
		unsigned int v;

		v = (readl(priv->config_regs + MMSYS_SODI_REQ_MASK)
			& (~sodi_req_mask));
		v += (sodi_req_val & sodi_req_mask);
		writel_relaxed(v, priv->config_regs + MMSYS_SODI_REQ_MASK);

		v = (readl(priv->config_regs +  MMSYS_EMI_REQ_CTL)
			& (~emi_req_mask));
		v += (emi_req_val & emi_req_mask);
		writel_relaxed(v, priv->config_regs +  MMSYS_EMI_REQ_CTL);
	} else {
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_SODI_REQ_MASK, sodi_req_val, sodi_req_mask);
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_EMI_REQ_CTL, emi_req_val, emi_req_mask);
	}
}

void mt6885_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data)
{
	struct mtk_drm_private *priv = drm->dev_private;
	unsigned int sodi_req_val = 0, sodi_req_mask = 0;
	unsigned int emi_req_val = 0, emi_req_mask = 0;
	bool en = *((bool *)data);

	if (id == DDP_COMPONENT_ID_MAX) { /* config when top clk on */
		if (!en)
			return;

		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					0, SODI_REQ_SEL_ALL);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					0, SODI_REQ_VAL_ALL);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_SEL_RDMA0_PD_MODE);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_VAL_RDMA0_PD_MODE);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_SEL_RDMA1_PD_MODE);
		SET_VAL_MASK(sodi_req_val, sodi_req_mask,
					1, SODI_REQ_VAL_RDMA1_PD_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0xFF, HRT_URGENT_CTL_SEL_ALL);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, HRT_URGENT_CTL_VAL_ALL);
		SET_VAL_MASK(emi_req_val, emi_req_mask,
					0, DVFS_HALT_MASK_SEL_ALL);
	} else if (id == DDP_COMPONENT_RDMA0) {
		SET_VAL_MASK(sodi_req_val, sodi_req_mask, (!(unsigned int)en),
					SODI_REQ_SEL_RDMA0_CG_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask, (!(unsigned int)en),
				HRT_URGENT_CTL_SEL_RDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
				DVFS_HALT_MASK_SEL_RDMA0);
	} else if (id == DDP_COMPONENT_RDMA1) {
		SET_VAL_MASK(sodi_req_val, sodi_req_mask, (!(unsigned int)en),
					SODI_REQ_SEL_RDMA1_CG_MODE);

		SET_VAL_MASK(emi_req_val, emi_req_mask, (!(unsigned int)en),
					HRT_URGENT_CTL_SEL_RDMA1);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
					DVFS_HALT_MASK_SEL_RDMA1);
	} else if (id == DDP_COMPONENT_RDMA4) {
		SET_VAL_MASK(emi_req_val, emi_req_mask, (!(unsigned int)en),
					HRT_URGENT_CTL_SEL_RDMA4);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
					DVFS_HALT_MASK_SEL_RDMA4);
	} else if (id == DDP_COMPONENT_RDMA5) {
		SET_VAL_MASK(emi_req_val, emi_req_mask, (!(unsigned int)en),
					HRT_URGENT_CTL_SEL_RDMA5);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
					DVFS_HALT_MASK_SEL_RDMA5);
	} else if (id == DDP_COMPONENT_WDMA0) {
		SET_VAL_MASK(emi_req_val, emi_req_mask, (!(unsigned int)en),
					HRT_URGENT_CTL_SEL_WDMA0);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
					DVFS_HALT_MASK_SEL_WDMA0);
	} else if (id == DDP_COMPONENT_WDMA1) {
		SET_VAL_MASK(emi_req_val, emi_req_mask, (!(unsigned int)en),
					HRT_URGENT_CTL_SEL_WDMA1);
		SET_VAL_MASK(emi_req_val, emi_req_mask, en,
					DVFS_HALT_MASK_SEL_WDMA1);
	} else
		return;

	if (handle == NULL) {
		unsigned int v;

		v = (readl(priv->config_regs + MMSYS_SODI_REQ_MASK)
			& (~sodi_req_mask));
		v += (sodi_req_val & sodi_req_mask);
		writel_relaxed(v, priv->config_regs + MMSYS_SODI_REQ_MASK);

		v = (readl(priv->config_regs +  MMSYS_EMI_REQ_CTL)
			& (~emi_req_mask));
		v += (emi_req_val & emi_req_mask);
		writel_relaxed(v, priv->config_regs +  MMSYS_EMI_REQ_CTL);
	} else {
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_SODI_REQ_MASK, sodi_req_val, sodi_req_mask);
		cmdq_pkt_write(handle, NULL, priv->config_regs_pa +
			MMSYS_EMI_REQ_CTL, emi_req_val, emi_req_mask);
	}
}

int mtk_ddp_comp_helper_get_opt(struct mtk_ddp_comp *comp,
				enum MTK_DRM_HELPER_OPT option)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = NULL;
	struct mtk_drm_helper *helper_opt = NULL;

	if (!mtk_crtc) {
		DDPINFO("%s: crtc is empty\n", __func__);
		return -EINVAL;
	}

	priv = mtk_crtc->base.dev->dev_private;
	helper_opt = priv->helper_opt;

	return mtk_drm_helper_get_opt(helper_opt, option);
}
