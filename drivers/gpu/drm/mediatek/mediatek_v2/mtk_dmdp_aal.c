// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_dump.h"

#define DMDP_AAL_EN		0x0000
#define DMDP_AAL_CFG		0x0020
#define DMDP_AAL_CFG_MAIN	0x0200
#define DMDP_AAL_SIZE		0x0030
#define DMDP_AAL_OUTPUT_SIZE	0x0034
#define DMDP_AAL_OUTPUT_OFFSET  0x0038
#define DMDP_AAL_SHADOW_CTRL    0x0F0
#define AAL_BYPASS_SHADOW	BIT(0)
#define AAL_READ_WRK_REG	BIT(2)
#define DMDP_AAL_DRE_BITPLUS_00 0x048C
#define DMDP_AAL_DRE_BILATERAL	0x053C
#define DMDP_AAL_Y2R_00		0x04BC
#define DMDP_AAL_R2Y_00		0x04D4

// DMDP AAL REGISTER
#define DMDP_AAL_SRAM_CFG                       (0x0c4)
#define DMDP_AAL_TILE_02			(0x0F4)
#define DMDP_AAL_DRE_BLOCK_INFO_07              (0x0f8)
#define DMDP_AAL_DRE_MAPPING_00                 (0x3b4)
#define DMDP_AAL_DRE_BLOCK_INFO_00              (0x468)
#define DMDP_AAL_DRE_BLOCK_INFO_01              (0x46c)
#define DMDP_AAL_DRE_BLOCK_INFO_02              (0x470)
#define DMDP_AAL_DRE_BLOCK_INFO_03              (0x474)
#define DMDP_AAL_DRE_BLOCK_INFO_04              (0x478)
#define DMDP_AAL_DRE_CHROMA_HIST_00             (0x480)
#define DMDP_AAL_DRE_CHROMA_HIST_01             (0x484)
#define DMDP_AAL_DRE_ALPHA_BLEND_00             (0x488)
#define DMDP_AAL_DRE_BLOCK_INFO_05              (0x4b4)
#define DMDP_AAL_DRE_BLOCK_INFO_06              (0x4b8)
#define DMDP_AAL_DUAL_PIPE_INFO_00              (0x4d0)
#define DMDP_AAL_DUAL_PIPE_INFO_01              (0x4d4)
#define DMDP_AAL_TILE_00			(0x4EC)
#define DMDP_AAL_TILE_01			(0x4F0)
#define DMDP_AAL_DRE_ROI_00			(0x520)
#define DMDP_AAL_DRE_ROI_01			(0x524)

#define AAL_EN BIT(0)

static atomic_t g_dmdp_aal_force_relay = ATOMIC_INIT(0);

static int g_dre30_support;
struct mtk_dmdp_aal_data {
	bool support_shadow;
	bool need_bypass_shadow;
	u32 block_info_00_mask;
};

struct mtk_dmdp_aal {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_dmdp_aal_data *data;
};

struct mtk_disp_mdp_aal_tile_overhead {
	unsigned int left_in_width;
	unsigned int left_overhead;
	unsigned int left_comp_overhead;
	unsigned int right_in_width;
	unsigned int right_overhead;
	unsigned int right_comp_overhead;
};

struct mtk_disp_mdp_aal_tile_overhead disp_mdp_aal_tile_overhead = { 0 };

static struct mtk_ddp_comp *default_comp;
static struct mtk_ddp_comp *default_comp1;

static inline struct mtk_dmdp_aal *comp_to_dmdp_aal(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_dmdp_aal, ddp_comp);
}

static void mtk_aal_write_mask(void __iomem *address, u32 data, u32 mask)
{
	u32 value = data;

	if (mask != ~0) {
		value = readl(address);
		value &= ~mask;
		data &= mask;
		value |= data;
	}
	writel(value, address);
}

static void mtk_dmdp_aal_start(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_EN,
		       AAL_EN, ~0);
}

static void mtk_dmdp_aal_stop(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_EN,
		       0x0, ~0);
}

static void mtk_dmdp_aal_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	DDPINFO("%s : bypass = %d g_dre30_support = %d\n",
			__func__, bypass, g_dre30_support);

	atomic_set(&g_dmdp_aal_force_relay, bypass);
	if (g_dre30_support) {
		if (bypass == 1) {
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_EN,
				   AAL_EN, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_CFG,
				   0x00400003, ~0);
		} else if (bypass == 0) {
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_EN,
				   AAL_EN, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_CFG,
				   0x00400022, ~0);
		}
	}
}

static void mtk_disp_mdp_aal_config_overhead(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg)
{
	DDPINFO("line: %d\n", __LINE__);

	if (cfg->tile_overhead.is_support) {
		/*set component overhead*/
		if (comp->id == DDP_COMPONENT_DMDP_AAL0) {
			disp_mdp_aal_tile_overhead.left_comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.left_overhead +=
				disp_mdp_aal_tile_overhead.left_comp_overhead;
			cfg->tile_overhead.left_in_width +=
				disp_mdp_aal_tile_overhead.left_comp_overhead;
			/*copy from total overhead info*/
			disp_mdp_aal_tile_overhead.left_in_width =
				cfg->tile_overhead.left_in_width;
			disp_mdp_aal_tile_overhead.left_overhead =
				cfg->tile_overhead.left_overhead;
		}
		if (comp->id == DDP_COMPONENT_DMDP_AAL1) {
			disp_mdp_aal_tile_overhead.right_comp_overhead = 0;
			/*add component overhead on total overhead*/
			cfg->tile_overhead.right_overhead +=
				disp_mdp_aal_tile_overhead.right_comp_overhead;
			cfg->tile_overhead.right_in_width +=
				disp_mdp_aal_tile_overhead.right_comp_overhead;
			/*copy from total overhead info*/
			disp_mdp_aal_tile_overhead.right_in_width =
				cfg->tile_overhead.right_in_width;
			disp_mdp_aal_tile_overhead.right_overhead =
				cfg->tile_overhead.right_overhead;
		}
	}
}

static void mtk_dmdp_aal_config(struct mtk_ddp_comp *comp,
			   struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int val = 0, out_val = 0;
	int width = cfg->w, height = cfg->h;
	int out_width = cfg->w;

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		if (comp->id == DDP_COMPONENT_DMDP_AAL0) {
			width = disp_mdp_aal_tile_overhead.left_in_width;
			out_width = width - disp_mdp_aal_tile_overhead.left_comp_overhead;
		} else {
			width = disp_mdp_aal_tile_overhead.right_in_width;
			out_width = width - disp_mdp_aal_tile_overhead.right_comp_overhead;
		}
	} else {
		if (comp->mtk_crtc->is_dual_pipe)
			width = cfg->w / 2;
		else
			width = cfg->w;

		out_width = width;
	}

	val = (width << 16) | height;
	out_val = (out_width << 16) | height;

	DDPINFO("%s: 0x%08x\n", __func__, val);

	if (g_dre30_support == 0 || atomic_read(&g_dmdp_aal_force_relay) == 1)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_CFG, 1, 0x1);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_CFG, 0, 0x1);

	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_SIZE,
			val, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_OUTPUT_SIZE, out_val, ~0);

	if (comp->mtk_crtc->is_dual_pipe && cfg->tile_overhead.is_support) {
		if (comp->id == DDP_COMPONENT_DMDP_AAL0) {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DMDP_AAL_OUTPUT_OFFSET, 0x0, ~0);
			//cmdq_pkt_write(handle, comp->cmdq_base,
			//	comp->regs_pa + DMDP_AAL_DRE_BLOCK_INFO_00,
			//	(cfg->w / 2 - 1) << 16 | 0, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DMDP_AAL_DRE_ROI_00,
				(out_width - 1) << 16 | 0, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DMDP_AAL_DRE_ROI_01,
				(cfg->h - 1) << 16 | 0, ~0);
		} else {
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DMDP_AAL_OUTPUT_OFFSET,
				(disp_mdp_aal_tile_overhead.right_comp_overhead << 16) | 0, ~0);
			//cmdq_pkt_write(handle, comp->cmdq_base,
			//	comp->regs_pa + DMDP_AAL_DRE_BLOCK_INFO_00,
			//	((cfg->w / 2 + disp_mdp_aal_tile_overhead.right_overhead
			//	- 1) << 16) | disp_mdp_aal_tile_overhead.right_overhead, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DMDP_AAL_DRE_ROI_00,
				(out_width - 1) << 16 | 0, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DMDP_AAL_DRE_ROI_01,
				(cfg->h - 1) << 16 | 0, ~0);
		}
	} else if (comp->mtk_crtc->is_dual_pipe) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_OUTPUT_OFFSET, 0x0, ~0);
		//cmdq_pkt_write(handle, comp->cmdq_base,
		//	comp->regs_pa + DMDP_AAL_DRE_BLOCK_INFO_00,
		//	(cfg->w / 2 - 1) << 16 | 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_DRE_ROI_00,
			(cfg->w / 2 - 1)  << 16 | 0, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_DRE_ROI_01,
			(cfg->h - 1) << 16 | 0, ~0);
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_OUTPUT_OFFSET, 0x0, ~0);
		//cmdq_pkt_write(handle, comp->cmdq_base,
		//	comp->regs_pa + DMDP_AAL_DRE_BLOCK_INFO_00,
		//	(cfg->w - 1) << 16 | 0, ~0);
	}

	//cmdq_pkt_write(handle, comp->cmdq_base,
	//		comp->regs_pa + DMDP_AAL_DRE_BILATERAL, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_DRE_BITPLUS_00, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_Y2R_00, 0, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DMDP_AAL_R2Y_00, 0, ~0);
	DDPINFO("%s [comp_id:%d]: g_dmdp_aal_force_relay[%d], DMDP_AAL_CFG = 0x%08x\n",
		__func__, comp->id, atomic_read(&g_dmdp_aal_force_relay),
		readl(comp->regs + DMDP_AAL_CFG));
}

void mtk_dmdp_aal_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	cmdq_pkt_write(handle, comp->cmdq_base, comp->regs_pa + DMDP_AAL_CFG,
			0x00400022, ~0);
	mtk_dmdp_aal_config(comp, cfg, handle);
	mtk_dmdp_aal_start(comp, handle);
}

static atomic_t g_aal_initialed = ATOMIC_INIT(0);
struct aal_backup { /* structure for backup AAL register value */
	unsigned int DRE_MAPPING;
	unsigned int DRE_BLOCK_INFO_00;
	unsigned int DRE_BLOCK_INFO_01;
	unsigned int DRE_BLOCK_INFO_02;
	unsigned int DRE_BLOCK_INFO_04;
	unsigned int DRE_BLOCK_INFO_05;
	unsigned int DRE_BLOCK_INFO_06;
	unsigned int DRE_BLOCK_INFO_07;
	unsigned int DRE_CHROMA_HIST_00;
	unsigned int DRE_CHROMA_HIST_01;
	unsigned int DRE_ALPHA_BLEND_00;
	unsigned int SRAM_CFG;
	unsigned int DUAL_PIPE_INFO_00;
	unsigned int DUAL_PIPE_INFO_01;
	unsigned int TILE_00;
	unsigned int DRE0_TILE_00;
	unsigned int DRE1_TILE_00;
	unsigned int TILE_01;
	unsigned int DRE0_TILE_01;
	unsigned int DRE1_TILE_01;
	unsigned int TILE_02;
	unsigned int MDP_AAL_CFG;
	unsigned int DRE0_ROI_00;
	unsigned int DRE1_ROI_00;
	unsigned int DRE_ROI_00;
	unsigned int DRE_ROI_01;
	unsigned int DRE0_BLOCK_INFO_00;
	unsigned int DRE1_BLOCK_INFO_00;
};
static struct aal_backup g_aal_backup;

static void ddp_aal_dre3_backup(struct mtk_ddp_comp *comp)
{
	g_aal_backup.DRE_BLOCK_INFO_01 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_01);
	g_aal_backup.DRE_BLOCK_INFO_02 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_02);
	g_aal_backup.DRE_BLOCK_INFO_04 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_04);
	g_aal_backup.DRE_CHROMA_HIST_00 =
		readl(comp->regs + DMDP_AAL_DRE_CHROMA_HIST_00);
	g_aal_backup.DRE_CHROMA_HIST_01 =
		readl(comp->regs + DMDP_AAL_DRE_CHROMA_HIST_01);
	g_aal_backup.DRE_ALPHA_BLEND_00 =
		readl(comp->regs + DMDP_AAL_DRE_ALPHA_BLEND_00);
	g_aal_backup.DRE_BLOCK_INFO_05 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_05);
	g_aal_backup.DRE_BLOCK_INFO_06 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_06);
	g_aal_backup.DRE_BLOCK_INFO_07 =
		readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_07);
	g_aal_backup.SRAM_CFG =
		readl(comp->regs + DMDP_AAL_SRAM_CFG);
	g_aal_backup.DUAL_PIPE_INFO_00 =
		readl(comp->regs + DMDP_AAL_DUAL_PIPE_INFO_00);
	g_aal_backup.DUAL_PIPE_INFO_01 =
		readl(comp->regs + DMDP_AAL_DUAL_PIPE_INFO_01);
	g_aal_backup.TILE_02 =
		readl(comp->regs + DMDP_AAL_TILE_02);
	if (comp->mtk_crtc->is_dual_pipe) {
		if (comp->id == DDP_COMPONENT_DMDP_AAL0) {
			g_aal_backup.DRE0_TILE_00 =
					readl(comp->regs + DMDP_AAL_TILE_00);
			g_aal_backup.DRE0_TILE_01 =
					readl(comp->regs + DMDP_AAL_TILE_01);
			g_aal_backup.DRE0_ROI_00 =
					readl(comp->regs + DMDP_AAL_DRE_ROI_00);
			g_aal_backup.DRE0_BLOCK_INFO_00 =
					readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_00);
		} else if (comp->id == DDP_COMPONENT_DMDP_AAL1) {
			g_aal_backup.DRE1_TILE_00 =
					readl(comp->regs + DMDP_AAL_TILE_00);
			g_aal_backup.DRE1_TILE_01 =
					readl(comp->regs + DMDP_AAL_TILE_01);
			g_aal_backup.DRE1_ROI_00 =
					readl(comp->regs + DMDP_AAL_DRE_ROI_00);
			g_aal_backup.DRE1_BLOCK_INFO_00 =
					readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_00);
		}
	} else {
		g_aal_backup.TILE_00 =
			readl(comp->regs + DMDP_AAL_TILE_00);
		g_aal_backup.TILE_01 =
			readl(comp->regs + DMDP_AAL_TILE_01);
		g_aal_backup.DRE_ROI_00 =
			readl(comp->regs + DMDP_AAL_DRE_ROI_00);
		g_aal_backup.DRE_BLOCK_INFO_00 =
			readl(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_00);
	}

	g_aal_backup.DRE_ROI_01 =
		readl(comp->regs + DMDP_AAL_DRE_ROI_01);
}

static void ddp_aal_dre_backup(struct mtk_ddp_comp *comp)
{
	g_aal_backup.DRE_MAPPING =
		readl(comp->regs + DMDP_AAL_DRE_MAPPING_00);
	g_aal_backup.MDP_AAL_CFG =
		readl(comp->regs + DMDP_AAL_CFG);
}

static void mtk_dmdp_aal_backup(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	ddp_aal_dre_backup(comp);
	ddp_aal_dre3_backup(comp);
	atomic_set(&g_aal_initialed, 1);
}

static void ddp_aal_dre3_restore(struct mtk_ddp_comp *comp)
{
	struct mtk_dmdp_aal *dmdp_aal = comp_to_dmdp_aal(comp);

	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_01,
		g_aal_backup.DRE_BLOCK_INFO_01, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_02,
		g_aal_backup.DRE_BLOCK_INFO_02, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_04,
		g_aal_backup.DRE_BLOCK_INFO_04 & (0x3FF << 13), 0x3FF << 13);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_CHROMA_HIST_00,
		g_aal_backup.DRE_CHROMA_HIST_00, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_CHROMA_HIST_01,
		g_aal_backup.DRE_CHROMA_HIST_01 & 0x1FFFFFFF, 0x1FFFFFFF);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_ALPHA_BLEND_00,
		g_aal_backup.DRE_ALPHA_BLEND_00, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_05,
		g_aal_backup.DRE_BLOCK_INFO_05, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_06,
		g_aal_backup.DRE_BLOCK_INFO_06, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_07,
		g_aal_backup.DRE_BLOCK_INFO_07, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_SRAM_CFG,
		g_aal_backup.SRAM_CFG, 0x1);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DUAL_PIPE_INFO_00,
		g_aal_backup.DUAL_PIPE_INFO_00, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_DUAL_PIPE_INFO_01,
		g_aal_backup.DUAL_PIPE_INFO_01, ~0);
	mtk_aal_write_mask(comp->regs + DMDP_AAL_TILE_02,
		g_aal_backup.TILE_02, ~0);

	if (comp->mtk_crtc->is_dual_pipe) {
		if (comp->id == DDP_COMPONENT_DMDP_AAL0) {
			mtk_aal_write_mask(comp->regs + DMDP_AAL_TILE_00,
				g_aal_backup.DRE0_TILE_00, ~0);
			mtk_aal_write_mask(comp->regs + DMDP_AAL_TILE_01,
				g_aal_backup.DRE0_TILE_01, ~0);
			mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_ROI_00,
				g_aal_backup.DRE0_ROI_00, ~0);
			mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_00,
				g_aal_backup.DRE0_BLOCK_INFO_00 &
				(dmdp_aal->data->block_info_00_mask),
				dmdp_aal->data->block_info_00_mask);
		} else if (comp->id == DDP_COMPONENT_DMDP_AAL1) {
			mtk_aal_write_mask(comp->regs + DMDP_AAL_TILE_00,
				g_aal_backup.DRE1_TILE_00, ~0);
			mtk_aal_write_mask(comp->regs + DMDP_AAL_TILE_01,
				g_aal_backup.DRE1_TILE_01, ~0);
			mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_ROI_00,
				g_aal_backup.DRE1_ROI_00, ~0);
			mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_00,
				g_aal_backup.DRE1_BLOCK_INFO_00 &
				(dmdp_aal->data->block_info_00_mask),
				dmdp_aal->data->block_info_00_mask);
		}
	} else {
		mtk_aal_write_mask(comp->regs + DMDP_AAL_TILE_00,
			g_aal_backup.TILE_00, ~0);
		mtk_aal_write_mask(comp->regs + DMDP_AAL_TILE_01,
			g_aal_backup.TILE_01, ~0);
		mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_ROI_00,
				g_aal_backup.DRE_ROI_00, ~0);
		mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BLOCK_INFO_00,
			g_aal_backup.DRE_BLOCK_INFO_00 &
			(dmdp_aal->data->block_info_00_mask),
			dmdp_aal->data->block_info_00_mask);
	}

	mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_ROI_01,
		g_aal_backup.DRE_ROI_01, ~0);
}

static void ddp_aal_dre_restore(struct mtk_ddp_comp *comp)
{
	writel(g_aal_backup.DRE_MAPPING,
		comp->regs + DMDP_AAL_DRE_MAPPING_00);
	writel(g_aal_backup.MDP_AAL_CFG,
		comp->regs + DMDP_AAL_CFG);
}

static void mtk_dmdp_aal_restore(struct mtk_ddp_comp *comp)
{
	if (atomic_read(&g_aal_initialed) != 1)
		return;

	DDPINFO("%s\n", __func__);
	ddp_aal_dre_restore(comp);
	ddp_aal_dre3_restore(comp);
}

static void mtk_dmdp_aal_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_dmdp_aal *dmdp_aal = comp_to_dmdp_aal(comp);

	DDPINFO("%s: call aal prepare\n", __func__);
	mtk_ddp_comp_clk_prepare(comp);

	/* Bypass shadow register and read shadow register */
	if (dmdp_aal->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, AAL_BYPASS_SHADOW,
			DMDP_AAL_SHADOW_CTRL, AAL_BYPASS_SHADOW);

	mtk_dmdp_aal_restore(comp);

	if (g_dre30_support == 0) {
		mtk_aal_write_mask(comp->regs + DMDP_AAL_EN, AAL_EN, ~0);
		mtk_aal_write_mask(comp->regs + DMDP_AAL_CFG, 0x400003, ~0);
		mtk_aal_write_mask(comp->regs + DMDP_AAL_CFG_MAIN, 0, ~0);
		//mtk_aal_write_mask(comp->regs + DMDP_AAL_DRE_BILATERAL, 0, ~0);
	}
}

static void mtk_dmdp_aal_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s: call aal unprepare\n", __func__);
	mtk_dmdp_aal_backup(comp);
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_dmdp_aal_funcs = {
	.config = mtk_dmdp_aal_config,
	.first_cfg = mtk_dmdp_aal_first_cfg,
	.start = mtk_dmdp_aal_start,
	.stop = mtk_dmdp_aal_stop,
	.bypass = mtk_dmdp_aal_bypass,
	.prepare = mtk_dmdp_aal_prepare,
	.unprepare = mtk_dmdp_aal_unprepare,
	.config_overhead = mtk_disp_mdp_aal_config_overhead,
};

static int mtk_dmdp_aal_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_dmdp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		DDPMSG("Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_dmdp_aal_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_dmdp_aal *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_dmdp_aal_component_ops = {
	.bind = mtk_dmdp_aal_bind, .unbind = mtk_dmdp_aal_unbind,
};

void mtk_dmdp_aal_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, 0x4D8);
	mtk_cust_dump_reg(baddr, 0x200, 0xf4, 0xf8, 0x468);
	mtk_cust_dump_reg(baddr, 0x46c, 0x470, 0x474, 0x478);
	mtk_cust_dump_reg(baddr, 0x4ec, 0x4f0, 0x528, 0x52c);
}

void mtk_dmdp_aal_regdump(void)
{
	void __iomem *baddr = default_comp->regs;
	int k;

	DDPDUMP("== %s REGS:0x%x ==\n", mtk_dump_comp_str(default_comp),
			default_comp->regs_pa);
	DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(default_comp));
	for (k = 0; k <= 0x600; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(baddr + k),
			readl(baddr + k + 0x4),
			readl(baddr + k + 0x8),
			readl(baddr + k + 0xc));
	}
	DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(default_comp));
	if (default_comp->mtk_crtc->is_dual_pipe && default_comp1) {
		baddr = default_comp1->regs;
		DDPDUMP("== %s REGS:0x%x ==\n", mtk_dump_comp_str(default_comp1),
				default_comp1->regs_pa);
		DDPDUMP("[%s REGS Start Dump]\n", mtk_dump_comp_str(default_comp1));
		for (k = 0; k <= 0x600; k += 16) {
			DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
				readl(baddr + k),
				readl(baddr + k + 0x4),
				readl(baddr + k + 0x8),
				readl(baddr + k + 0xc));
		}
		DDPDUMP("[%s REGS End Dump]\n", mtk_dump_comp_str(default_comp1));
	}
}

static int mtk_dmdp_aal_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_dmdp_aal *priv;
	struct device_node *aal_node;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPMSG("%s\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DMDP_AAL);
	if ((int)comp_id < 0) {
		DDPMSG("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	aal_node = of_find_compatible_node(NULL, NULL, "mediatek,disp_aal0");
	if (of_property_read_u32(aal_node, "mtk_dre30_support",
		&g_dre30_support)) {
		DDPMSG("comp_id: %d, mtk_dre30_support = %d\n",
			comp_id, g_dre30_support);
		return -EINVAL;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_dmdp_aal_funcs);
	if (ret != 0) {
		DDPMSG("Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_dmdp_aal_component_ops);
	if (ret != 0) {
		DDPMSG("Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	if (!default_comp && comp_id == DDP_COMPONENT_DMDP_AAL0)
		default_comp = &priv->ddp_comp;
	if (!default_comp1 && comp_id == DDP_COMPONENT_DMDP_AAL1)
		default_comp1 = &priv->ddp_comp;

	return ret;
}

static int mtk_dmdp_aal_remove(struct platform_device *pdev)
{
	struct mtk_dmdp_aal *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_dmdp_aal_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_dmdp_aal_data mt6885_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = false,
	.block_info_00_mask = 0x3FFFFFF,
};

static const struct mtk_dmdp_aal_data mt6873_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.block_info_00_mask = 0x3FFF3FFF,
};

static const struct mtk_dmdp_aal_data mt6895_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.block_info_00_mask = 0xFFFFFFFF,
};

static const struct mtk_dmdp_aal_data mt6983_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.block_info_00_mask = 0xFFFFFFFF,
};

static const struct mtk_dmdp_aal_data mt6985_dmdp_aal_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
	.block_info_00_mask = 0xFFFFFFFF,
};

static const struct of_device_id mtk_dmdp_aal_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6885-dmdp-aal",
	  .data = &mt6885_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6873-dmdp-aal",
	  .data = &mt6873_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6983-dmdp-aal",
	  .data = &mt6983_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6895-dmdp-aal",
	  .data = &mt6895_dmdp_aal_driver_data},
	{ .compatible = "mediatek,mt6985-dmdp-aal",
	  .data = &mt6985_dmdp_aal_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_dmdp_aal_driver_dt_match);

struct platform_driver mtk_dmdp_aal_driver = {
	.probe = mtk_dmdp_aal_probe,
	.remove = mtk_dmdp_aal_remove,
	.driver = {

			.name = "mediatek-dmdp-aal",
			.owner = THIS_MODULE,
			.of_match_table = mtk_dmdp_aal_driver_dt_match,
		},
};
