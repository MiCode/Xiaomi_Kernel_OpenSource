// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_gem.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_graphics_base.h"

#define DISP_REG_MDP_RDMA_EN 0x000
#define ROT_ENABLE BIT(0)

#define DISP_REG_MDP_RDMA_INT_ENABLE 0x010
#define DISP_REG_MDP_RDMA_INT_STATUS 0x018
#define FRAME_COMPLETE_INT BIT(0)
#define REG_UPDATE_INT BIT(1)
#define UNDERRUN_INT BIT(2)

#define DISP_REG_MDP_RDMA_CON 0x020
#define OUTPUT_10B BIT(5)

#define DISP_REG_MDP_RDMA_SRC_CON 0x030
#define MEM_MODE_INPUT_FORMAT_RGB888 (0x1U)
#define MEM_MODE_INPUT_FORMAT_NV12 (0xcU)
#define LOOSE BIT(11)
#define SWAP BIT(14)
#define UNIFORM_CONFIG BIT(17)
#define BIT_NUMBER_8BIT (0x0 << 18)
#define BIT_NUMBER_10BIT (0x1 << 18)
#define BIT_NUMBER_16BIT (0x3 << 18)

#define DISP_REG_MDP_RDMA_MF_BKGD_SIZE_IN_BYTE 0x060
#define DISP_REG_MDP_RDMA_MF_SRC_SIZE 0x070
#define DISP_REG_MDP_RDMA_MF_CLIP_SIZE 0x078
#define DISP_REG_MDP_RDMA_SF_BKGD_SIZE_IN_BYTE 0x090
#define DISP_REG_MDP_RDMA_TRANSFORM_0 0x200
#define TRANS_EN BIT(16)
#define BT601F_TO_RGB (0x4 << 23)
#define BT709F_TO_RGB (0x5 << 23)
#define BT601_TO_RGB (0x6 << 23)
#define BT709_TO_RGB (0x7 << 23)

#define DISP_REG_MDP_RDMA_SRC_BASE_0 0xf00
#define DISP_REG_MDP_RDMA_SRC_BASE_1 0xf08
#define DISP_REG_MDP_RDMA_SRC_BASE_2 0xf10
#define DISP_REG_MDP_RDMA_SRC_BASE_0_MSB 0xf30
#define DISP_REG_MDP_RDMA_SRC_BASE_1_MSB 0xf34
#define DISP_REG_MDP_RDMA_SRC_BASE_2_MSB 0xf38

#define DISP_REG_MDP_RDMA_SRC_OFFSET_0 0x118
#define DISP_REG_MDP_RDMA_SRC_OFFSET_1 0x120
#define DISP_REG_MDP_RDMA_SRC_OFFSET_2 0x128

struct mtk_disp_mdp_rdma {
	struct mtk_ddp_comp ddp_comp;
	struct mtk_drm_gem_obj *fill_gem;
	unsigned int underrun_cnt;
	unsigned int cfg_h;
};

static inline struct mtk_disp_mdp_rdma *comp_to_mdp_rdma(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_mdp_rdma, ddp_comp);
}

static irqreturn_t mtk_disp_mdp_rdma_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_mdp_rdma *priv = dev_id;
	struct mtk_ddp_comp *mdp_rdma = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned int val = 0;
	unsigned int ret = 0;

	if (IS_ERR_OR_NULL(priv))
		return IRQ_NONE;

	mdp_rdma = &priv->ddp_comp;
	if (IS_ERR_OR_NULL(mdp_rdma))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get("mdp_rdma_irq") == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	val = readl(mdp_rdma->regs + DISP_REG_MDP_RDMA_INT_STATUS);
	if (!val) {
		ret = IRQ_NONE;
		goto out;
	}
	DRM_MMP_MARK(IRQ, mdp_rdma->regs_pa, val);

	mtk_crtc = mdp_rdma->mtk_crtc;

	if (mdp_rdma->id == DDP_COMPONENT_MDP_RDMA0)
		DRM_MMP_MARK(mdp_rdma0, val, 0);
	else if (mdp_rdma->id == DDP_COMPONENT_MDP_RDMA1)
		DRM_MMP_MARK(mdp_rdma1, val, 0);

	DDPIRQ("%s irq, val:0x%x\n", mtk_dump_comp_str(mdp_rdma), val);

	writel(~val, mdp_rdma->regs + DISP_REG_MDP_RDMA_INT_STATUS);

	if (val & FRAME_COMPLETE_INT)
		DDPIRQ("[IRQ] %s: frame done!\n", mtk_dump_comp_str(mdp_rdma));

	if (val & REG_UPDATE_INT)
		DDPIRQ("[IRQ] %s: reg update done!\n", mtk_dump_comp_str(mdp_rdma));

	if (val & UNDERRUN_INT) {
		DDPPR_ERR("[IRQ] %s: underrun! cnt=%d\n",
			  mtk_dump_comp_str(mdp_rdma), priv->underrun_cnt);
		priv->underrun_cnt++;
	}

	ret = IRQ_HANDLED;

out:
	mtk_drm_top_clk_isr_put("mdp_rdma_irq");

	return ret;
}

void mtk_mdp_rdma_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}
	DDPDUMP("== DISP %s REGS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);

	for (i = 0; i < 0xb0; i += 0x10)
		mtk_serial_dump_reg(baddr, i, 4);

	for (i = 0x110; i < 0x160; i += 0x10)
		mtk_serial_dump_reg(baddr, i, 4);

	mtk_serial_dump_reg(baddr, 0x200, 1);

	mtk_serial_dump_reg(baddr, 0x2a0, 1);
	mtk_cust_dump_reg(baddr, 0xF00, 0xF08, 0xF10, -1);
	mtk_cust_dump_reg(baddr, 0xF30, 0xF34, 0xF38, -1);
	mtk_cust_dump_reg(baddr, 0xF44, 0xF48, 0xF4C, -1);

	for (i = 0x400; i < 0x4e0; i += 0x10)
		mtk_cust_dump_reg(baddr, i, i + 8, -1, -1);
	mtk_serial_dump_reg(baddr, 0x4e0, 1);
}

int mtk_mdp_rdma_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return 0;
	}
	DDPDUMP("== %s ANALYSIS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);

	return 0;
}

static void mtk_mdp_rdma_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	unsigned int val;

	mtk_ddp_write_mask(comp, OUTPUT_10B,
		DISP_REG_MDP_RDMA_CON, OUTPUT_10B, handle);
	mtk_ddp_write_mask(comp, ROT_ENABLE,
		DISP_REG_MDP_RDMA_EN, ROT_ENABLE, handle);
	val = FRAME_COMPLETE_INT | UNDERRUN_INT;
	mtk_ddp_write_mask(comp, val,
		DISP_REG_MDP_RDMA_INT_ENABLE, val, handle);
}

static void mtk_mdp_rdma_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	mtk_ddp_write(comp, 0, DISP_REG_MDP_RDMA_INT_ENABLE, handle);
	mtk_ddp_write_mask(comp, 0,
		DISP_REG_MDP_RDMA_EN, ROT_ENABLE, handle);
	mtk_ddp_write(comp, 0, DISP_REG_MDP_RDMA_INT_STATUS, handle);
}

static void mtk_mdp_rdma_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
}

static void mtk_mdp_rdma_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static unsigned int mdp_rdma_fmt_convert(unsigned int fmt)
{
	switch (fmt) {
	case DRM_FORMAT_BGR888:
		return MEM_MODE_INPUT_FORMAT_RGB888 | SWAP | BIT_NUMBER_8BIT;
	case DRM_FORMAT_P010:
		/* need use NV12 10bit mode*/
		return MEM_MODE_INPUT_FORMAT_NV12 | BIT_NUMBER_10BIT |
			LOOSE;
	case DRM_FORMAT_NV12:
		return MEM_MODE_INPUT_FORMAT_NV12;
	case DRM_FORMAT_NV21:
		return MEM_MODE_INPUT_FORMAT_NV12 | SWAP;
	default:
		DDPPR_ERR("[discrete] not support fmt:0x%x\n", fmt);
		return 0;
	}
}

static int mtk_mdp_rdma_yuv_convert(enum mtk_drm_dataspace plane_ds)
{
	int ret = 0;

	switch (plane_ds & MTK_DRM_DATASPACE_STANDARD_MASK) {
	case MTK_DRM_DATASPACE_STANDARD_BT601_625:
	case MTK_DRM_DATASPACE_STANDARD_BT601_625_UNADJUSTED:
	case MTK_DRM_DATASPACE_STANDARD_BT601_525:
	case MTK_DRM_DATASPACE_STANDARD_BT601_525_UNADJUSTED:
		switch (plane_ds & MTK_DRM_DATASPACE_RANGE_MASK) {
		case MTK_DRM_DATASPACE_RANGE_FULL:
			ret = BT601F_TO_RGB;
			break;
		default:
			ret = BT601_TO_RGB;
			break;
		}
		break;

	case MTK_DRM_DATASPACE_STANDARD_BT709:
	case MTK_DRM_DATASPACE_STANDARD_DCI_P3:
	case MTK_DRM_DATASPACE_STANDARD_BT2020:
		ret = BT709_TO_RGB;
		break;

	default:
		ret = BT709_TO_RGB;
		break;
	}

	ret |= TRANS_EN;

	return ret;
}

static void mtk_mdp_rdma_fill_config(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int fill_h, unsigned int cfg_h)
{
	struct mtk_disp_mdp_rdma *mdp_rdma = comp_to_mdp_rdma(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_mode_fb_cmd2 mode = {0};
	unsigned int width, pitch;
	unsigned int fmt = DRM_FORMAT_BGR888;
	int Bpp;
	struct cmdq_client *client;
	struct cmdq_pkt *cfg_handle;
	dma_addr_t addr;
	unsigned int con = 0;

	mtk_gem = mdp_rdma->fill_gem;
	if (IS_ERR_OR_NULL(mtk_gem)) {
		mode.width = crtc->state->adjusted_mode.hdisplay;
		mode.height = crtc->state->adjusted_mode.vdisplay;
		Bpp = mtk_get_format_bpp(fmt);
		mode.pitches[0] = mode.width * Bpp;
		mtk_gem = mtk_drm_gem_create(
			crtc->dev, mode.pitches[0] * mode.height, true);
		memset(mtk_gem->kvaddr, 0x77, mode.pitches[0] * mode.height);
		mdp_rdma->fill_gem = mtk_gem;
		DDPINFO("[discrete] create dummy buffer:0x%lx\n", mtk_gem->dma_addr);
	}

	if (cfg_h != 0) {
		cfg_handle = mtk_crtc->pending_handle;
		if (!cfg_handle) {
			//only config one layer, but not height enough
			client = mtk_crtc->gce_obj.client[CLIENT_CFG];
			mtk_crtc_pkt_create(&cfg_handle, crtc, client);
			mtk_crtc->pending_handle = cfg_handle;
			DDPINFO("[discrete] create pending hnd:0x%x", cfg_handle);
		}
		mtk_crtc_wait_comp_done(mtk_crtc, cfg_handle, comp, 0);
	} else {
		//no config any plane
		cfg_handle = handle;
	}
	DDPINFO("[discrete] fill frame use hnd:0x%x\n", cfg_handle);

	addr = mtk_gem->dma_addr;
	con = mdp_rdma_fmt_convert(fmt);
	con |= UNIFORM_CONFIG;
	width = crtc->state->adjusted_mode.hdisplay;
	pitch = width * 3;
	if (mtk_crtc->is_dual_pipe)
		width /= 2;

	DDPINFO("[discrete] comp:%s, already_cfg_h:%d, hnd:0x%x\n",
		mtk_dump_comp_str_id(comp->id),	cfg_h, cfg_handle);
	DDPINFO("[discrete] addr:0x%lx, pitch:%d, WxH:%dx%d, fmt:0x%x, con:0x%x\n",
		(unsigned long)addr, pitch, width, fill_h, DRM_FORMAT_RGB888, con);

	//1. addr
	cmdq_pkt_write(cfg_handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_SRC_BASE_0,
		addr, ~0);
	cmdq_pkt_write(cfg_handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_SRC_BASE_0_MSB,
		(addr >> 32), 0xf);
	//2. con
	cmdq_pkt_write(cfg_handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_SRC_CON,
		con, ~0);
	//3. pitch
	cmdq_pkt_write(cfg_handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_MF_BKGD_SIZE_IN_BYTE,
		pitch, ~0);
	//4. src roi
	cmdq_pkt_write(cfg_handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_MF_SRC_SIZE,
		(width | fill_h << 16), ~0);
	//5. clip
	cmdq_pkt_write(cfg_handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_MF_CLIP_SIZE,
		(width | fill_h << 16), ~0);
	//6. transform
	cmdq_pkt_write(cfg_handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_MDP_RDMA_TRANSFORM_0,
			0, ~0);

	if (cfg_h != 0) {
		mtk_disp_mutex_add_comp_with_cmdq(mtk_crtc, comp->id,
			0, cfg_handle, 1);
		mtk_disp_mutex_enable_cmdq(mtk_crtc->mutex[1],
			cfg_handle, comp->cmdq_base);
	}
}

static void mtk_mdp_rdma_fill_frame(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	struct mtk_disp_mdp_rdma *mdp_rdma = comp_to_mdp_rdma(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc;
	unsigned int cfg_h, fill_h, height;
	int crtc_index;

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("[discrete] comp has no crtc\n");
		return;
	}
	crtc = &mtk_crtc->base;
	crtc_index = drm_crtc_index(crtc);
	height = crtc->state->adjusted_mode.vdisplay;

	cfg_h = mdp_rdma->cfg_h;
	mdp_rdma->cfg_h = 0;

	if (height < cfg_h) {
		DDPPR_ERR("[discrete] out_h:%d < already_cfg_h:%d\n",
			height, cfg_h);
		return;
	} else if (height == cfg_h)
		return;

	fill_h = height - cfg_h;
	DDPINFO("[discrete] out_h:%d, already_cfg_h:%d, need_fill:%d\n",
		height, cfg_h, fill_h);
	CRTC_MMP_MARK(crtc_index, discrete_fill, cfg_h, fill_h);
	mtk_mdp_rdma_fill_config(comp, handle, fill_h, cfg_h);
}

static void mtk_mdp_rdma_config(struct mtk_ddp_comp *comp,
			    struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_disp_mdp_rdma *mdp_rdma = comp_to_mdp_rdma(comp);

	mdp_rdma->cfg_h = cfg->h;
	DDPINFO("[discrete] %s fill_out_h:%d\n", __func__, cfg->h);
	mtk_mdp_rdma_fill_config(comp, handle, cfg->h, 0);
}

static bool mtk_mdp_rdma_config_check(struct mtk_plane_pending_state *pending)
{
	unsigned int con = 0;

	if (!pending->enable || !pending->addr) {
		DDPINFO("[discrete] enable:%d, addr:0x%lx\n",
			pending->enable, (unsigned long)pending->addr);
		return false;
	}

	con = mdp_rdma_fmt_convert(pending->format);
	if (!con)
		return false;

	return true;
}

static void _mtk_mdp_rdma_layer_config(struct mtk_ddp_comp *comp,
		unsigned int idx, struct mtk_plane_state *state, struct cmdq_pkt *handle)
{
	struct mtk_disp_mdp_rdma *mdp_rdma = comp_to_mdp_rdma(comp);
	struct mtk_plane_pending_state *pending = &state->pending;
	struct drm_framebuffer *fb = state->base.fb;
	dma_addr_t addr = pending->addr;
	unsigned int fmt = pending->format;
	unsigned int pitch = pending->pitch;
	unsigned int width = pending->width;
	unsigned int height = pending->height;
	unsigned int src_x = pending->src_x;
	dma_addr_t uv_addr;
	unsigned int uv_offset, uv_pitch, offset;
	unsigned int con = 0;

	con = mdp_rdma_fmt_convert(fmt);
	con |= UNIFORM_CONFIG;

	if ((idx == 0 && mdp_rdma->cfg_h != 0) ||
			(idx != 0 && mdp_rdma->cfg_h == 0))
		DDPPR_ERR("[discrete] plane idx=%d, but cfg_h=%d\n",
					idx, mdp_rdma->cfg_h);

	offset = src_x * mtk_drm_format_plane_cpp(fmt, 0);

	DDPINFO("[discrete] comp:%s, idx:%d, en:%d, already_cfg_h:%d, hnd:0x%x\n",
		mtk_dump_comp_str_id(comp->id), idx, pending->enable,
		mdp_rdma->cfg_h, handle);
	DDPINFO("[discrete] addr:0x%lx, off:%d, pitch:%d, WxH:%dx%d, fmt:0x%x, con:0x%x\n",
		(unsigned long)addr, offset, pitch, width, height, fmt, con);

	mdp_rdma->cfg_h += height;

	//1. addr
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_SRC_BASE_0,
		addr, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_SRC_BASE_0_MSB,
		(addr >> 32), 0xf);
	//2. offset
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_SRC_OFFSET_0,
		offset, ~0);
	//3. con
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_SRC_CON,
		con, ~0);
	//4. pitch
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_MF_BKGD_SIZE_IN_BYTE,
		pitch, ~0);
	//5. src roi
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_MF_SRC_SIZE,
		(width | height << 16), ~0);
	//6. clip
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_MDP_RDMA_MF_CLIP_SIZE,
		(width | height << 16), ~0);

	if (fmt == DRM_FORMAT_P010 ||
		fmt == DRM_FORMAT_NV12 ||
		fmt == DRM_FORMAT_NV21) {
		unsigned int dataspace = pending->prop_val[PLANE_PROP_DATASPACE];
		int trans_value;

		//UV plane
		uv_offset = fb->offsets[1];
		uv_addr = addr + uv_offset;
		//x-axis in yuv420 has sub-sample, so src_x needs to be divided by 2
		offset = (src_x / 2) * mtk_drm_format_plane_cpp(fmt, 1);
		DDPINFO("[discrete][dual] src_x=%d, offset=0x%x, ds=%lld\n",
			src_x, offset, dataspace);
		uv_pitch = fb->pitches[1];
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_MDP_RDMA_SRC_BASE_1,
			uv_addr, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_MDP_RDMA_SRC_BASE_1_MSB,
			(uv_addr >> 32), 0xf);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_MDP_RDMA_SRC_OFFSET_1,
			offset, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_MDP_RDMA_SF_BKGD_SIZE_IN_BYTE,
			uv_pitch, ~0);

		//6. transform
		trans_value = mtk_mdp_rdma_yuv_convert((enum mtk_drm_dataspace)dataspace);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_MDP_RDMA_TRANSFORM_0,
			trans_value, ~0);
	} else {
		//6. transform
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_MDP_RDMA_TRANSFORM_0,
			0, ~0);
	}
}

static void mtk_mdp_rdma_layer_config(struct mtk_ddp_comp *comp,
		unsigned int idx, struct mtk_plane_state *state, struct cmdq_pkt *handle)
{
	struct mtk_plane_pending_state *pending = &state->pending;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc;
	struct cmdq_client *client;
	struct cmdq_pkt *pending_handle;

	if (!mtk_mdp_rdma_config_check(pending))
		return;

	if (idx == 0) {
		//plane0 config, need clear prev atomic commit frame done event
		mtk_crtc_clr_comp_done(mtk_crtc, handle, comp, 0);
		_mtk_mdp_rdma_layer_config(comp, idx, state, handle);
		return;
	}

	//after plane0 config, need to wait prev plane done first
	if (!mtk_crtc->pending_handle) {
		crtc = &mtk_crtc->base;
		client = mtk_crtc->gce_obj.client[CLIENT_CFG];
		mtk_crtc_pkt_create(&pending_handle, crtc, client);
		mtk_crtc->pending_handle = pending_handle;
		DDPINFO("[discrete] %s create hnd:0x%x\n",
			__func__, pending_handle);
	} else
		pending_handle = mtk_crtc->pending_handle;

	mtk_crtc_wait_comp_done(mtk_crtc, pending_handle, comp, 0);

	_mtk_mdp_rdma_layer_config(comp, idx, state, pending_handle);

	mtk_disp_mutex_add_comp_with_cmdq(mtk_crtc, comp->id,
		0, pending_handle, 1);
}

static int mtk_mdp_rdma_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum mtk_ddp_io_cmd cmd, void *params)
{
	int ret = 0;
	struct mtk_drm_private *priv =
		comp->mtk_crtc->base.dev->dev_private;

	switch (cmd) {
	case MDP_RDMA_FILL_FRAME:
	{
		mtk_mdp_rdma_fill_frame(comp, handle);
	}
		break;
	case PMQOS_SET_HRT_BW:
	{
		u32 bw_val = *(unsigned int *)params;

		if (priv && !mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;

		__mtk_disp_set_module_hrt(comp->hrt_qos_req, bw_val);
		ret = MDP_RDMA_REQ_HRT;
	}
		break;
	default:
		break;
	}

	return ret;
}

static const struct mtk_ddp_comp_funcs mtk_disp_mdp_rdma_funcs = {
	.start = mtk_mdp_rdma_start,
	.stop = mtk_mdp_rdma_stop,
	.prepare = mtk_mdp_rdma_prepare,
	.unprepare = mtk_mdp_rdma_unprepare,
	.config = mtk_mdp_rdma_config,
	.layer_config = mtk_mdp_rdma_layer_config,
	.io_cmd = mtk_mdp_rdma_io_cmd,
};

static int mtk_disp_mdp_rdma_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_mdp_rdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *private = drm_dev->dev_private;
	int ret;
	char buf[50];

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)) {
		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "hrt_qos");
		priv->ddp_comp.hrt_qos_req = of_mtk_icc_get(dev, buf);
		if (!IS_ERR(priv->ddp_comp.hrt_qos_req))
			DDPMSG("%s, %s create success, dev:%s\n", __func__, buf, dev_name(dev));
	}

	return 0;
}

static void mtk_disp_mdp_rdma_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_mdp_rdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_mdp_rdma_component_ops = {
	.bind = mtk_disp_mdp_rdma_bind, .unbind = mtk_disp_mdp_rdma_unbind,
};

static int mtk_disp_mdp_rdma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_mdp_rdma *priv;
	enum mtk_ddp_comp_id comp_id;
	int irq;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_MDP_RDMA);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_mdp_rdma_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	/* Disable and clear pending interrupts */
	writel(0x0, priv->ddp_comp.regs + DISP_REG_MDP_RDMA_INT_STATUS);
	writel(0x0, priv->ddp_comp.regs + DISP_REG_MDP_RDMA_INT_ENABLE);

	ret = devm_request_irq(dev, irq, mtk_disp_mdp_rdma_irq_handler,
			       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
			       priv);
	if (ret < 0) {
		DDPAEE("%s:%d, failed to request irq:%d ret:%d comp_id:%d\n",
				__func__, __LINE__,
				irq, ret, comp_id);
		return ret;
	}

//	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_mdp_rdma_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_mdp_rdma_remove(struct platform_device *pdev)
{
	struct mtk_disp_mdp_rdma *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_mdp_rdma_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_disp_mdp_rdma_driver_dt_match[] = {
	{.compatible = "mediatek,mt6985-disp-mdp-rdma",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_mdp_rdma_driver_dt_match);

struct platform_driver mtk_disp_mdp_rdma_driver = {
	.probe = mtk_disp_mdp_rdma_probe,
	.remove = mtk_disp_mdp_rdma_remove,
	.driver = {
		.name = "mediatek-disp-mdp-rdma",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_mdp_rdma_driver_dt_match,
	},
};
