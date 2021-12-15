/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <drm/drmP.h>
#include <linux/clk.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_layering_rule.h"
#include "mtk_drm_trace.h"
#include "swpm_me.h"

#define DISP_REG_RDMA_INT_ENABLE 0x0000
#define DISP_REG_RDMA_INT_STATUS 0x0004
#define RDMA_TARGET_LINE_INT BIT(5)
#define RDMA_FIFO_UNDERFLOW_INT BIT(4)
#define RDMA_EOF_ABNORMAL_INT BIT(3)
#define RDMA_FRAME_END_INT BIT(2)
#define RDMA_FRAME_START_INT BIT(1)
#define RDMA_REG_UPDATE_INT BIT(0)
#define DISP_REG_RDMA_GLOBAL_CON 0x0010
#define RDMA_ENGINE_EN BIT(0)
#define RDMA_SOFT_RESET BIT(4)
#define RDMA_MODE_MEMORY BIT(1)
#define DISP_REG_RDMA_SIZE_CON_0 0x0014
#define RDMA_MATRIX_ENABLE BIT(17)
#define RDMA_MATRIX_INT_MTX_SEL (7UL << 20)
#define DISP_REG_RDMA_SIZE_CON_1 0x0018
#define DISP_REG_RDMA_TARGET_LINE 0x001c
#define DISP_REG_RDMA_MEM_CON 0x0024
#define DISP_REG_RDMA_MEM_START_ADDR 0x0f00
#define DISP_REG_RDMA_MEM_SRC_PITCH 0x002c
#define DISP_REG_RDMA_MEM_GMC_S0 0x0030
#define MEM_GMC_S0_FLD_PRE_ULTRA_THRESHOLD_LOW \
					REG_FLD_MSB_LSB(13, 0)
#define MEM_GMC_S0_FLD_PRE_ULTRA_THRESHOLD_HIGH \
					REG_FLD_MSB_LSB(29, 16)
#define MEM_GMC_S0_FLD_RG_VALID_THRESHOLD_FORCE_PREULTRA \
					REG_FLD_MSB_LSB(30, 30)
#define MEM_GMC_S0_FLD_RG_VDE_FORCE_PREULTRA \
					REG_FLD_MSB_LSB(31, 31)
#define DISP_REG_RDMA_MEM_GMC_S1 0x0034
#define MEM_GMC_S1_FLD_ULTRA_THRESHOLD_LOW \
					REG_FLD_MSB_LSB(13, 0)
#define MEM_GMC_S1_FLD_ULTRA_THRESHOLD_HIGH \
					REG_FLD_MSB_LSB(29, 16)
#define MEM_GMC_S1_FLD_RG_VALID_THRESHOLD_BLOCK_ULTRA \
					REG_FLD_MSB_LSB(30, 30)
#define MEM_GMC_S1_FLD_RG_VDE_BLOCK_ULTRA \
					REG_FLD_MSB_LSB(31, 31)

#define DISP_REG_RDMA_MEM_SLOW_CON 0x0038
#define DISP_REG_RDMA_MEM_GMC_S2 0x003c
#define MEM_GMC_S2_FLD_ISSUE_REQ_THRESHOLD REG_FLD_MSB_LSB(13, 0)
#define DISP_REG_RDMA_FIFO_LOG 0x0044
#define DISP_REG_RDMA_PRE_ADD_0 0x0078
#define DISP_REG_RDMA_PRE_ADD_1 0x007c
#define DISP_REG_RDMA_PRE_ADD_2 0x0080
#define DISP_REG_RDMA_POST_ADD_0 0x0084
#define DISP_REG_RDMA_POST_ADD_1 0x0088
#define DISP_REG_RDMA_POST_ADD_2 0x008c
#define DISP_REG_RDMA_DUMMY 0x0090
#define DISP_REG_RDMA_DEBUG_OUT_SEL 0x0094
#define DISP_REG_RDMA_BG_CON_0 0x00a0
#define DISP_REG_RDMA_BG_CON_1 0x00a4
#define DISP_REG_RDMA_THRESHOLD_FOR_SODI 0x00a8
#define RDMA_THRESHOLD_FOR_SODI_FLD_LOW	REG_FLD_MSB_LSB(13, 0)
#define RDMA_THRESHOLD_FOR_SODI_FLD_HIGH REG_FLD_MSB_LSB(29, 16)
#define DISP_REG_RDMA_THRESHOLD_FOR_DVFS 0x00ac
#define RDMA_THRESHOLD_FOR_DVFS_FLD_LOW REG_FLD_MSB_LSB(13, 0)
#define RDMA_THRESHOLD_FOR_DVFS_FLD_HIGH REG_FLD_MSB_LSB(29, 16)
#define DISP_REG_RDMA_SRAM_SEL 0x00b0
#define DISP_REG_RDMA_STALL_CG_CON 0x00b4
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
#define DISP_REG_RDMA_SHADOW_UPDATE 0x00b8
#endif
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6877) || defined(CONFIG_MACH_MT6781)
#define DISP_REG_RDMA_SHADOW_UPDATE 0x00bc
#define RDMA_BYPASS_SHADOW BIT(1)
#define RDMA_READ_WORK_REG BIT(2)
#endif
#define DISP_RDMA_SRAM_CASCADE 0x00c8
#define RG_DISP_RDMA_FIFO_SIZE REG_FLD_MSB_LSB(13, 0)
#define RG_DISP_RDMA_RSZ_FIFO_SIZE REG_FLD_MSB_LSB(29, 16)
#define DISP_REG_RDMA_DVFS_SETTING_PRE 0x00d0
#define RG_DVFS_PRE_ULTRA_THRESHOLD_LOW REG_FLD_MSB_LSB(13, 0)
#define RG_DVFS_PRE_ULTRA_THRESHOLD_HIGH REG_FLD_MSB_LSB(29, 16)
#define DISP_REG_RDMA_DVFS_SETTING_ULTRA 0x00d4
#define RG_DVFS_ULTRA_THRESHOLD_LOW REG_FLD_MSB_LSB(13, 0)
#define RG_DVFS_ULTRA_THRESHOLD_HIGH REG_FLD_MSB_LSB(29, 16)

#define DISP_REG_RDMA_LEAVE_DRS_SETTING 0x00d8
#define RG_IS_DRS_STATUS_THRESHOLD_LOW REG_FLD_MSB_LSB(13, 0)
#define RG_IS_DRS_STATUS_THRESHOLD_HIGH REG_FLD_MSB_LSB(29, 16)

#define DISP_REG_RDMA_ENTER_DRS_SETTING 0x00dc
#define RG_NOT_DRS_STATUS_THRESHOLD_LOW REG_FLD_MSB_LSB(13, 0)
#define RG_NOT_DRS_STATUS_THRESHOLD_HIGH REG_FLD_MSB_LSB(29, 16)

#define DISP_REG_RDMA_CROP_CON_0 0x00e0
#define CROP_CON_0_FLD_CROP_LEFT REG_FLD_MSB_LSB(12, 0)
#define CROP_CON_0_FLD_CROP_RIGHT REG_FLD_MSB_LSB(28, 16)
#define DISP_REG_RDMA_CROP_CON_1 0x00e4
#define CROP_CON_0_FLD_CROP_TOP REG_FLD_MSB_LSB(12, 0)
#define CROP_CON_0_FLD_CROP_BOTTOM REG_FLD_MSB_LSB(28, 16)
#define DISP_REG_RDMA_MEM_GMC_S3 0x00e8
#define FLD_LOW_FOR_URGENT REG_FLD_MSB_LSB(13, 0)
#define FLD_HIGH_FOR_URGENT REG_FLD_MSB_LSB(29, 16)
#define FLD_RG_VALID_THRESHOLD_BLOCK_URGENT REG_FLD_MSB_LSB(30, 30)
#define FLD_RG_VDE_BLOCK_URGENT REG_FLD_MSB_LSB(31, 31)

/* TODO: handle pixel/line cnt for other platform */
#define DISP_REG_RDMA_IN_P_CNT 0x0120
#define DISP_REG_RDMA_IN_LINE_CNT 0x0124
#define DISP_REG_RDMA_OUT_P_CNT 0x0128
#define DISP_REG_RDMA_OUT_LINE_CNT 0x012C
#define DISP_REG_RDMA_DBG_OUT 0x0100
#define DISP_REG_RDMA_DBG_OUT1 0x010c
#define DISP_REG_RDMA_DBG_OUT2 0x0110
#define DISP_REG_RDMA_DBG_OUT3 0x0114
#define DISP_REG_RDMA_DBG_OUT4 0x0118
#define DISP_REG_RDMA_DBG_OUT5 0x011c
#define DISP_REG_RDMA_DBG_120  0x0120
#define DISP_REG_RDMA_DBG_124  0x0124
#define DISP_REG_RDMA_DBG_128  0x0128
#define DISP_REG_RDMA_DBG_12c  0x012c
#define DISP_REG_RDMA_DBG_130  0x0130
#define DISP_REG_RDMA_DBG_134  0x0134
#define DISP_REG_RDMA_DBG_138  0x0138

#define DISP_REG_RDMA_GREQ_URG_NUM_SEL 0x01a8
#define FLD_RG_LAYER_SMI_ID_EN REG_FLD_MSB_LSB(29, 29)

#define DISP_RDMA_MEM_CON 0x0024
#define MEM_MODE_INPUT_SWAP BIT(8)
#define DISP_RDMA_MEM_SRC_PITCH 0x002c
#define DISP_REG_RDMA_FIFO_CON 0x0040
#define FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD REG_FLD_MSB_LSB(13, 0)
#define FIFO_CON_FLD_FIFO_PSEUDO_SIZE	REG_FLD_MSB_LSB(29, 16)
#define FIFO_CON_FLD_FIFO_UNDERFLOW_EN	REG_FLD_MSB_LSB(31, 31)
#define RDMA_FIFO_UNDERFLOW_EN BIT(31)
#define RDMA_FIFO_PSEUDO_SIZE(bytes) (((bytes) / 16UL) << 16)
#define RDMA_OUTPUT_VALID_FIFO_THRESHOLD(bytes) ((bytes) / 16)
#define RDMA_FIFO_SIZE(module) ((module)->data->fifo_size)
#define DISP_RDMA_MEM_START_ADDR 0x0f00

#define MATRIX_INT_MTX_SEL_DEFAULT 0x000000

#define MEM_MODE_INPUT_FORMAT_RGB565 0x0U
#define MEM_MODE_INPUT_FORMAT_RGB888 (0x001U << 4)
#define MEM_MODE_INPUT_FORMAT_RGBA8888 (0x002U << 4)
#define MEM_MODE_INPUT_FORMAT_ARGB8888 (0x003U << 4)
#define MEM_MODE_INPUT_FORMAT_UYVY (0x004U << 4)
#define MEM_MODE_INPUT_FORMAT_YUYV (0x005U << 4)
#define RDMA_DUMMY_BUFFER_SIZE(h, v) ((h) * (v)*4)
#define RDMA_DUMMY_BUFFER_PITCH(h) ((h)*4)

#define GLOBAL_CON_FLD_ENGINE_EN REG_FLD_MSB_LSB(0, 0)
#define GLOBAL_CON_FLD_MODE_SEL REG_FLD_MSB_LSB(1, 1)
#define GLOBAL_CON_FLD_SMI_BUSY REG_FLD_MSB_LSB(12, 12)
#define RDMA_BG_CON_0_LEFT REG_FLD_MSB_LSB(12, 0)
#define RDMA_BG_CON_0_RIGHT REG_FLD_MSB_LSB(28, 16)
#define RDMA_BG_CON_1_TOP REG_FLD_MSB_LSB(12, 0)
#define RDMA_BG_CON_1_BOTTOM REG_FLD_MSB_LSB(28, 16)

/* golden setting */
enum GS_RDMA_FLD {
	GS_RDMA_PRE_ULTRA_TH_LOW = 0,
	GS_RDMA_PRE_ULTRA_TH_HIGH,
	GS_RDMA_VALID_TH_FORCE_PRE_ULTRA,
	GS_RDMA_VDE_FORCE_PRE_ULTRA,
	GS_RDMA_ULTRA_TH_LOW,
	GS_RDMA_ULTRA_TH_HIGH,
	GS_RDMA_VALID_TH_BLOCK_ULTRA,
	GS_RDMA_VDE_BLOCK_ULTRA,
	GS_RDMA_ISSUE_REQ_TH,
	GS_RDMA_OUTPUT_VALID_FIFO_TH,
	GS_RDMA_FIFO_SIZE,
	GS_RDMA_FIFO_UNDERFLOW_EN,
	GS_RDMA_TH_LOW_FOR_SODI,
	GS_RDMA_TH_HIGH_FOR_SODI,
	GS_RDMA_TH_LOW_FOR_DVFS,
	GS_RDMA_TH_HIGH_FOR_DVFS,
	GS_RDMA_SRAM_SEL,
	GS_RDMA_DVFS_PRE_ULTRA_TH_LOW,
	GS_RDMA_DVFS_PRE_ULTRA_TH_HIGH,
	GS_RDMA_DVFS_ULTRA_TH_LOW,
	GS_RDMA_DVFS_ULTRA_TH_HIGH,
	GS_RDMA_IS_DRS_STATUS_TH_LOW,
	GS_RDMA_IS_DRS_STATUS_TH_HIGH,
	GS_RDMA_NOT_DRS_STATUS_TH_LOW,
	GS_RDMA_NOT_DRS_STATUS_TH_HIGH,
	GS_RDMA_URGENT_TH_LOW,
	GS_RDMA_URGENT_TH_HIGH,
	GS_RDMA_SELF_FIFO_SIZE,
	GS_RDMA_RSZ_FIFO_SIZE,
	GS_RDMA_LAYER_SMI_ID_EN,
	GS_RDMA_FLD_NUM,
};

struct mtk_disp_rdma_data {
	unsigned int fifo_size;
	void (*sodi_config)(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
	bool support_shadow;
};

struct mtk_rdma_backup_info {
	unsigned long addr;
};

struct mtk_rdma_cfg_info {
	unsigned int addr;
	unsigned int width;
	unsigned int height;
	unsigned int fmt;
};

/**
 * struct mtk_disp_rdma - DISP_RDMA driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 * @crtc - associated crtc to report irq events to
 */
struct mtk_disp_rdma {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_rdma_data *data;
	struct drm_device *drm_dev;
	bool rdma_memory_mode;
	unsigned int underflow_cnt;
	unsigned int abnormal_cnt;
	unsigned int dummy_w;
	unsigned int dummy_h;
	struct mtk_rdma_backup_info backup_info;
	struct mtk_rdma_cfg_info cfg_info;
};

static inline struct mtk_disp_rdma *comp_to_rdma(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_rdma, ddp_comp);
}

static irqreturn_t mtk_disp_rdma_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_rdma *priv = dev_id;
	struct mtk_ddp_comp *rdma = &priv->ddp_comp;
	struct mtk_drm_crtc *mtk_crtc = rdma->mtk_crtc;
	struct mtk_crtc_state *state;
	unsigned int val = 0;
	unsigned int ret = 0;

	if (mtk_drm_top_clk_isr_get("rdma_irq") == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}

	val = readl(rdma->regs + DISP_REG_RDMA_INT_STATUS);
	if (!val) {
		ret = IRQ_NONE;
		goto out;
	}

	DRM_MMP_MARK(IRQ, irq, val);

	if (rdma->id == DDP_COMPONENT_RDMA0)
		DRM_MMP_MARK(rdma0, val, 0);
	if (rdma->id == DDP_COMPONENT_RDMA4)
		DRM_MMP_MARK(rdma4, val, 0);
	if (rdma->id == DDP_COMPONENT_RDMA5)
		DRM_MMP_MARK(rdma5, val, 0);

	if (val & 0x18)
		DRM_MMP_MARK(abnormal_irq,
			     (priv->underflow_cnt << 24) |
				     (priv->abnormal_cnt << 16) | val,
			     rdma->id);

	DDPIRQ("%s irq, val:0x%x\n", mtk_dump_comp_str(rdma), val);

	writel(~val, rdma->regs + DISP_REG_RDMA_INT_STATUS);

	if (val & (1 << 0))
		DDPIRQ("[IRQ] %s: reg update done!\n", mtk_dump_comp_str(rdma));

	if (val & (1 << 2)) {
		set_swpm_disp_work(); /* counting fps for swpm */
		if (rdma->id == DDP_COMPONENT_RDMA0)
			DRM_MMP_EVENT_END(rdma0, val, 0);
		DDPIRQ("[IRQ] %s: frame done!\n", mtk_dump_comp_str(rdma));
		if (rdma->mtk_crtc && rdma->mtk_crtc->esd_ctx)
			atomic_set(&rdma->mtk_crtc->esd_ctx->target_time, 0);
		if (rdma->id == DDP_COMPONENT_RDMA0) {
			unsigned long long rdma_end_time = sched_clock();

			lcm_fps_ctx_update(rdma_end_time,
					   priv->ddp_comp.mtk_crtc->base.index,
					   1);
		}
		if (!mtk_drm_is_idle(&(rdma->mtk_crtc->base)))
			mtk_drm_refresh_tag_end(&priv->ddp_comp);
	}

	if (val & (1 << 1)) {
		if (rdma->id == DDP_COMPONENT_RDMA0)
			DRM_MMP_EVENT_START(rdma0, val, 0);
		DDPINFO("[IRQ] %s: frame start!\n", mtk_dump_comp_str(rdma));

		if (mtk_crtc &&
			mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base)) {
			state = to_mtk_crtc_state(mtk_crtc->base.state);
			if (state &&
				!state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
				atomic_set(&mtk_crtc->pf_event, 1);
				wake_up_interruptible(&mtk_crtc->present_fence_wq);
			}
		}

		if (!mtk_drm_is_idle(&(rdma->mtk_crtc->base)))
			mtk_drm_refresh_tag_start(&priv->ddp_comp);
		MMPathTraceDRM(rdma);
	}

	if (val & (1 << 3)) {
		DDPPR_ERR("[IRQ] %s: abnormal! cnt=%d\n",
			  mtk_dump_comp_str(rdma), priv->abnormal_cnt);
		priv->abnormal_cnt++;
	}
	if (val & (1 << 4)) {
		DDPPR_ERR("[IRQ] %s: underflow! cnt=%d\n",
			  mtk_dump_comp_str(rdma), priv->underflow_cnt);

		DDPMSG("%s: pix(%d,%d,%d,%d)\n", mtk_dump_comp_str(rdma),
		       readl(DISP_REG_RDMA_IN_P_CNT + rdma->regs),
		       readl(DISP_REG_RDMA_IN_LINE_CNT + rdma->regs),
		       readl(DISP_REG_RDMA_OUT_P_CNT + rdma->regs),
		       readl(DISP_REG_RDMA_OUT_LINE_CNT + rdma->regs));
		if (rdma->mtk_crtc) {
			mtk_drm_crtc_analysis(&(rdma->mtk_crtc->base));
			mtk_drm_crtc_dump(&(rdma->mtk_crtc->base));
		}
		if (rdma->mtk_crtc) {
			struct mtk_drm_private *drm_priv = NULL;

			if (rdma->mtk_crtc->base.dev)
				drm_priv =
					rdma->mtk_crtc->base.dev->dev_private;
			if (drm_priv && mtk_drm_helper_get_opt(
				drm_priv->helper_opt,
				MTK_DRM_OPT_RDMA_UNDERFLOW_AEE)) {
				disp_met_set(NULL, 1);
				DDPAEE("%s: underflow! cnt=%d\n",
				       mtk_dump_comp_str(rdma),
				       priv->underflow_cnt);
			}
		}

		priv->underflow_cnt++;
	}
	if (val & (1 << 5)) {
		DDPIRQ("[IRQ] %s: target line!\n", mtk_dump_comp_str(rdma));
		if (mtk_crtc &&
			!mtk_crtc_is_frame_trigger_mode(&mtk_crtc->base)) {
			atomic_set(&mtk_crtc->sf_pf_event, 1);
			wake_up_interruptible(&mtk_crtc->sf_present_fence_wq);
		}
		if (rdma->mtk_crtc && rdma->mtk_crtc->esd_ctx &&
			(!(val & (1 << 2)))) {
			atomic_set(&rdma->mtk_crtc->esd_ctx->target_time, 1);
			wake_up_interruptible(
				&rdma->mtk_crtc->esd_ctx->check_task_wq);
		}
	}

	/* TODO: check if this is not necessary */
	/* mtk_crtc_ddp_irq(priv->crtc, rdma); */

	ret = IRQ_HANDLED;

out:
	mtk_drm_top_clk_isr_put("rdma_irq");

	return ret;
}

#if 0
static void mtk_rdma_enable_vblank(struct mtk_ddp_comp *comp,
				   struct drm_crtc *crtc,
				   struct cmdq_pkt *handle)
{
	struct mtk_disp_rdma *rdma = comp_to_rdma(comp);

	rdma->crtc = crtc;
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_INT_ENABLE,
		       RDMA_FRAME_END_INT, RDMA_FRAME_END_INT);
}

static void mtk_rdma_disable_vblank(struct mtk_ddp_comp *comp,
				    struct cmdq_pkt *handle)
{
	struct mtk_disp_rdma *rdma = comp_to_rdma(comp);

	rdma->crtc = NULL;
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_INT_ENABLE,
		       RDMA_FRAME_END_INT, 0);
}
#endif

static int mtk_rdma_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			   enum mtk_ddp_io_cmd io_cmd, void *params);

static void mtk_rdma_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	int ret;
	struct mtk_disp_rdma *rdma = comp_to_rdma(comp);
	const struct mtk_disp_rdma_data *data = rdma->data;
	bool en = 1;

	ret = pm_runtime_get_sync(comp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to enable power domain: %d\n", ret);

	mtk_ddp_write_mask(comp, MATRIX_INT_MTX_SEL_DEFAULT,
			   DISP_REG_RDMA_SIZE_CON_0, 0xff0000, handle);

	mtk_rdma_io_cmd(comp, handle, IRQ_LEVEL_ALL, NULL);

	mtk_ddp_write_mask(comp, RDMA_ENGINE_EN, DISP_REG_RDMA_GLOBAL_CON,
			   RDMA_ENGINE_EN, handle);

	if (data && data->sodi_config)
		data->sodi_config(comp->mtk_crtc->base.dev, comp->id, handle,
				  &en);
}

static void mtk_rdma_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	int ret;
	bool en = 0;
	struct mtk_disp_rdma *rdma = comp_to_rdma(comp);
	const struct mtk_disp_rdma_data *data = rdma->data;

	mtk_ddp_write(comp, 0x0, DISP_REG_RDMA_INT_ENABLE, handle);
	mtk_ddp_write(comp, RDMA_SOFT_RESET, DISP_REG_RDMA_GLOBAL_CON, handle);
	mtk_ddp_write(comp, 0x0, DISP_REG_RDMA_GLOBAL_CON, handle);
	mtk_ddp_write(comp, 0x0, DISP_REG_RDMA_INT_STATUS, handle);

	if (data && data->sodi_config)
		data->sodi_config(comp->mtk_crtc->base.dev, comp->id, handle,
				  &en);

	ret = pm_runtime_put(comp->dev);
	if (ret < 0)
		DRM_ERROR("Failed to disable power domain: %d\n", ret);
}

/* TODO RDMA1, wrot sram */
void mtk_rdma_cal_golden_setting(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, unsigned int *gs)
{
	/* fixed variable */
	unsigned int mmsys_clk = 208;
	unsigned int FP = 1000;
#if defined(CONFIG_MACH_MT6781)
	unsigned int fifo_size = 3062;
#else
	unsigned int fifo_size = 2240;
#endif
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
	unsigned int pre_ultra_low_us = 245, pre_ultra_high_us = 255;
	unsigned int ultra_low_us = 230, ultra_high_us = 245;
	unsigned int urgent_low_us = 113, urgent_high_us = 117;
#endif
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6781)
	unsigned int pre_ultra_low_us = 250, pre_ultra_high_us = 260;
	unsigned int ultra_low_us = 230, ultra_high_us = 250;
	unsigned int urgent_low_us = 110, urgent_high_us = 120;
#endif

	/* input variable */
	struct golden_setting_context *gsc = cfg->p_golden_setting_context;
	unsigned long long width = gsc->dst_width, height = gsc->dst_height;
	unsigned int Bpp;
	bool is_dc = gsc->is_dc;
	unsigned int if_fps = gsc->vrefresh;

	unsigned int fill_rate = 0;	  /* 100 times */
	unsigned long long consume_rate = 0; /* 100 times */

	if (if_fps == 0) {
		DDPPR_ERR("%s invalid vrefresh %u\n",
			__func__, if_fps);
		if_fps = 60;
	}

	if (comp->mtk_crtc->is_dual_pipe)
		width /= 2;

	switch (cfg->bpc) {
	case 8:
		/* 888 */
		Bpp = 3;
		break;
	case 5:
		/* 565 */
		Bpp = 2;
		break;
	case 6:
		/* 666 */
		Bpp = 3;
		break;
	default:
		/* 888 */
		Bpp = 3;
		break;
	}

	/* critical variable calc */
	if (is_dc)
		fill_rate = 96 * mmsys_clk; /* FIFO depth / us */
	else
		fill_rate = 96 * mmsys_clk * 3 / 16; /* FIFO depth / us */

	DDPINFO("%s,w:%llu,h:%llu,vrefresh:%d,bpc:%d,is_vdo:%d,is_dc:%d\n",
		__func__, width, height, if_fps, cfg->bpc,
		gsc->is_vdo_mode, gsc->is_dc);

	consume_rate = width * height * if_fps * Bpp;
	do_div(consume_rate, 1000);
	consume_rate *= 125;
	do_div(consume_rate, 16 * 1000);

	/* RDMA golden setting calculation */
	/* DISP_RDMA_MEM_GMC_SETTING_0 */
#if BITS_PER_LONG == 32
	gs[GS_RDMA_PRE_ULTRA_TH_LOW] =
		DIV64_U64_ROUND_UP(consume_rate * (pre_ultra_low_us), FP);
	gs[GS_RDMA_PRE_ULTRA_TH_HIGH] =
		DIV64_U64_ROUND_UP(consume_rate * (pre_ultra_high_us), FP);
#else
	gs[GS_RDMA_PRE_ULTRA_TH_LOW] =
		DIV_ROUND_UP(consume_rate * (pre_ultra_low_us), FP);
	gs[GS_RDMA_PRE_ULTRA_TH_HIGH] =
		DIV_ROUND_UP(consume_rate * (pre_ultra_high_us), FP);
#endif
	if (gsc->is_vdo_mode) {
		gs[GS_RDMA_VALID_TH_FORCE_PRE_ULTRA] = 0;
		gs[GS_RDMA_VDE_FORCE_PRE_ULTRA] = 1;
	} else {
		gs[GS_RDMA_VALID_TH_FORCE_PRE_ULTRA] = 1;
		gs[GS_RDMA_VDE_FORCE_PRE_ULTRA] = 0;
	}

	/* DISP_RDMA_MEM_GMC_SETTING_1 */
#if BITS_PER_LONG == 32
	gs[GS_RDMA_ULTRA_TH_LOW] =
		DIV64_U64_ROUND_UP(consume_rate * (ultra_low_us), FP);
#else
	gs[GS_RDMA_ULTRA_TH_LOW] =
		DIV_ROUND_UP(consume_rate * (ultra_low_us), FP);
#endif
	gs[GS_RDMA_ULTRA_TH_HIGH] = gs[GS_RDMA_PRE_ULTRA_TH_LOW];
	if (gsc->is_vdo_mode) {
		gs[GS_RDMA_VALID_TH_BLOCK_ULTRA] = 0;
		gs[GS_RDMA_VDE_BLOCK_ULTRA] = 1;
	} else {
		gs[GS_RDMA_VALID_TH_BLOCK_ULTRA] = 1;
		gs[GS_RDMA_VDE_BLOCK_ULTRA] = 0;
	}

	/* DISP_RDMA_FIFO_CON */
	if (gsc->is_vdo_mode)
		gs[GS_RDMA_OUTPUT_VALID_FIFO_TH] = 0;
	else
		gs[GS_RDMA_OUTPUT_VALID_FIFO_TH] = gs[GS_RDMA_PRE_ULTRA_TH_LOW];
	gs[GS_RDMA_FIFO_SIZE] = fifo_size;
	gs[GS_RDMA_FIFO_UNDERFLOW_EN] = 0;

	/* DISP_RDMA_MEM_GMC_SETTING_2 */
	/* do not min this value with 256 to avoid hrt fail in
	 * dc mode under SODI CG mode
	 */
	gs[GS_RDMA_ISSUE_REQ_TH] =
		((gs[GS_RDMA_FIFO_SIZE] -
		gs[GS_RDMA_PRE_ULTRA_TH_LOW]) >= 256) ? 256 :
		(gs[GS_RDMA_FIFO_SIZE] - gs[GS_RDMA_PRE_ULTRA_TH_LOW]);

	/* DISP_RDMA_THRESHOLD_FOR_SODI */
#if BITS_PER_LONG == 32
	gs[GS_RDMA_TH_LOW_FOR_SODI] =
		DIV64_U64_ROUND_UP(consume_rate * (ultra_low_us + 50), FP);
	gs[GS_RDMA_TH_HIGH_FOR_SODI] = DIV64_U64_ROUND_UP(
		gs[GS_RDMA_FIFO_SIZE] * FP - (fill_rate - consume_rate) * 12,
		FP);
#else
	gs[GS_RDMA_TH_LOW_FOR_SODI] =
		DIV_ROUND_UP(consume_rate * (ultra_low_us + 50), FP);
	gs[GS_RDMA_TH_HIGH_FOR_SODI] = DIV_ROUND_UP(
		gs[GS_RDMA_FIFO_SIZE] * FP - (fill_rate - consume_rate) * 12,
		FP);
#endif
	if (gs[GS_RDMA_TH_HIGH_FOR_SODI] < gs[GS_RDMA_PRE_ULTRA_TH_HIGH])
		gs[GS_RDMA_TH_HIGH_FOR_SODI] = gs[GS_RDMA_PRE_ULTRA_TH_HIGH];

	if (gs[GS_RDMA_TH_HIGH_FOR_SODI] >= gs[GS_RDMA_FIFO_SIZE])
		gs[GS_RDMA_TH_HIGH_FOR_SODI] = gs[GS_RDMA_FIFO_SIZE] - 1;

	/* DISP_RDMA_THRESHOLD_FOR_DVFS */
	gs[GS_RDMA_TH_LOW_FOR_DVFS] = gs[GS_RDMA_PRE_ULTRA_TH_LOW];
	gs[GS_RDMA_TH_HIGH_FOR_DVFS] = gs[GS_RDMA_PRE_ULTRA_TH_LOW] + 1;

	/* DISP_RDMA_SRAM_SEL */
	gs[GS_RDMA_SRAM_SEL] = 0;
#if BITS_PER_LONG == 32
	/* DISP_RDMA_DVFS_SETTING_PREULTRA */
	gs[GS_RDMA_DVFS_PRE_ULTRA_TH_LOW] =
		DIV64_U64_ROUND_UP(consume_rate * (pre_ultra_low_us + 40), FP);
	gs[GS_RDMA_DVFS_PRE_ULTRA_TH_HIGH] =
		DIV64_U64_ROUND_UP(consume_rate * (pre_ultra_high_us + 40), FP);

	/* DISP_RDMA_DVFS_SETTING_ULTRA */
	gs[GS_RDMA_DVFS_ULTRA_TH_LOW] =
		DIV64_U64_ROUND_UP(consume_rate * (ultra_low_us + 40), FP);
	gs[GS_RDMA_DVFS_ULTRA_TH_HIGH] = gs[GS_RDMA_DVFS_PRE_ULTRA_TH_LOW];

	/* DISP_RDMA_LEAVE_DRS_SETTING */
	gs[GS_RDMA_IS_DRS_STATUS_TH_LOW] =
		DIV64_U64_ROUND_UP(consume_rate * (pre_ultra_low_us + 20), FP);
	gs[GS_RDMA_IS_DRS_STATUS_TH_HIGH] =
		DIV64_U64_ROUND_UP(consume_rate * (pre_ultra_low_us + 20), FP);

	/* DISP_RDMA_ENTER_DRS_SETTING */
	gs[GS_RDMA_NOT_DRS_STATUS_TH_LOW] =
		DIV64_U64_ROUND_UP(consume_rate * (ultra_high_us + 40), FP);
	gs[GS_RDMA_NOT_DRS_STATUS_TH_HIGH] =
		DIV64_U64_ROUND_UP(consume_rate * (ultra_high_us + 40), FP);

	/* DISP_RDMA_MEM_GMC_SETTING_3 */
	gs[GS_RDMA_URGENT_TH_LOW] = DIV64_U64_ROUND_UP(consume_rate *
		urgent_low_us, FP);
	gs[GS_RDMA_URGENT_TH_HIGH] = DIV64_U64_ROUND_UP(consume_rate *
		urgent_high_us, FP);
#else
	/* DISP_RDMA_DVFS_SETTING_PREULTRA */
	gs[GS_RDMA_DVFS_PRE_ULTRA_TH_LOW] =
		DIV_ROUND_UP(consume_rate * (pre_ultra_low_us + 40), FP);
	gs[GS_RDMA_DVFS_PRE_ULTRA_TH_HIGH] =
		DIV_ROUND_UP(consume_rate * (pre_ultra_high_us + 40), FP);

	/* DISP_RDMA_DVFS_SETTING_ULTRA */
	gs[GS_RDMA_DVFS_ULTRA_TH_LOW] =
		DIV_ROUND_UP(consume_rate * (ultra_low_us + 40), FP);
	gs[GS_RDMA_DVFS_ULTRA_TH_HIGH] = gs[GS_RDMA_DVFS_PRE_ULTRA_TH_LOW];

	/* DISP_RDMA_LEAVE_DRS_SETTING */
	gs[GS_RDMA_IS_DRS_STATUS_TH_LOW] =
		DIV_ROUND_UP(consume_rate * (pre_ultra_low_us + 20), FP);
	gs[GS_RDMA_IS_DRS_STATUS_TH_HIGH] =
		DIV_ROUND_UP(consume_rate * (pre_ultra_low_us + 20), FP);

	/* DISP_RDMA_ENTER_DRS_SETTING */
	gs[GS_RDMA_NOT_DRS_STATUS_TH_LOW] =
		DIV_ROUND_UP(consume_rate * (ultra_high_us + 40), FP);
	gs[GS_RDMA_NOT_DRS_STATUS_TH_HIGH] =
		DIV_ROUND_UP(consume_rate * (ultra_high_us + 40), FP);

	/* DISP_RDMA_MEM_GMC_SETTING_3 */
	gs[GS_RDMA_URGENT_TH_LOW] = DIV_ROUND_UP(consume_rate *
		urgent_low_us, FP);
	gs[GS_RDMA_URGENT_TH_HIGH] = DIV_ROUND_UP(consume_rate *
		urgent_high_us, FP);
#endif

	/* DISP_RDMA_GREQ_URG_NUM_SEL */
	gs[GS_RDMA_LAYER_SMI_ID_EN] = 1;

#if 0
	/* DISP_RDMA_SRAM_CASCADE */
	gs[GS_RDMA_SELF_FIFO_SIZE] = 1536;
	gs[GS_RDMA_RSZ_FIFO_SIZE] = 1536;
#endif
}

/* Set register with value from mtk_rdma_cal_golden_setting.
 * Do not do any math here!
 */
static void mtk_rdma_set_ultra_l(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	unsigned int gs[GS_RDMA_FLD_NUM] = {0};
	unsigned int val = 0;

	if ((comp->id != DDP_COMPONENT_RDMA0)
		&& (comp->id != DDP_COMPONENT_RDMA1)
		&& (comp->id != DDP_COMPONENT_RDMA4)
		&& (comp->id != DDP_COMPONENT_RDMA5)) {
		DDPPR_ERR("unsupport golden setting, id:%d\n", comp->id);
		return;
	}

	if (!cfg->p_golden_setting_context) {
		DDPPR_ERR("golden setting is null, %s,%d\n", __FILE__,
			  __LINE__);
		return;
	}

	/* calculate golden setting */
	mtk_rdma_cal_golden_setting(comp, cfg, gs);

	/* set golden setting */
	val = gs[GS_RDMA_PRE_ULTRA_TH_LOW] +
	      (gs[GS_RDMA_PRE_ULTRA_TH_HIGH] << 16) +
	      (gs[GS_RDMA_VALID_TH_FORCE_PRE_ULTRA] << 30) +
	      (gs[GS_RDMA_VDE_FORCE_PRE_ULTRA] << 31);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_MEM_GMC_S0, val, ~0);

	val = gs[GS_RDMA_ULTRA_TH_LOW] + (gs[GS_RDMA_ULTRA_TH_HIGH] << 16) +
	      (gs[GS_RDMA_VALID_TH_BLOCK_ULTRA] << 30) +
	      (gs[GS_RDMA_VDE_BLOCK_ULTRA] << 31);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_MEM_GMC_S1, val, ~0);

	val = gs[GS_RDMA_ISSUE_REQ_TH];
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_MEM_GMC_S2, val,
		       ~0);

	val = gs[GS_RDMA_OUTPUT_VALID_FIFO_TH] + (gs[GS_RDMA_FIFO_SIZE] << 16) +
	      (gs[GS_RDMA_FIFO_UNDERFLOW_EN] << 31);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_FIFO_CON, val, ~0);

	val = gs[GS_RDMA_TH_LOW_FOR_SODI] +
	      (gs[GS_RDMA_TH_HIGH_FOR_SODI] << 16);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_THRESHOLD_FOR_SODI, val,
		       ~0);

	val = gs[GS_RDMA_TH_LOW_FOR_DVFS] +
	      (gs[GS_RDMA_TH_HIGH_FOR_DVFS] << 16);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_THRESHOLD_FOR_DVFS, val,
		       ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_SRAM_SEL,
		       gs[GS_RDMA_SRAM_SEL], ~0);

	val = gs[GS_RDMA_DVFS_PRE_ULTRA_TH_LOW] +
	      (gs[GS_RDMA_DVFS_PRE_ULTRA_TH_HIGH] << 16);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_DVFS_SETTING_PRE, val, ~0);

	val = gs[GS_RDMA_DVFS_ULTRA_TH_LOW] +
	      (gs[GS_RDMA_DVFS_ULTRA_TH_HIGH] << 16);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_DVFS_SETTING_ULTRA, val,
		       ~0);

	val = gs[GS_RDMA_IS_DRS_STATUS_TH_LOW] +
	      (gs[GS_RDMA_IS_DRS_STATUS_TH_HIGH] << 16);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_LEAVE_DRS_SETTING, val,
		       ~0);

	val = gs[GS_RDMA_NOT_DRS_STATUS_TH_LOW] +
	      (gs[GS_RDMA_NOT_DRS_STATUS_TH_HIGH] << 16);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_ENTER_DRS_SETTING, val,
		       ~0);

	val = gs[GS_RDMA_URGENT_TH_LOW] + (gs[GS_RDMA_URGENT_TH_HIGH] << 16);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_MEM_GMC_S3, val,
		       ~0);
#if !defined(CONFIG_MACH_MT6833)
	val = gs[GS_RDMA_LAYER_SMI_ID_EN] << 29;
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_GREQ_URG_NUM_SEL, val,
		       REG_FLD_MASK(FLD_RG_LAYER_SMI_ID_EN));
#endif

	/*esd will wait this target line irq*/
	mtk_ddp_write(comp, (cfg->h * 9) / 10,
		DISP_REG_RDMA_TARGET_LINE, handle);
#if 0
	val = gs[GS_RDMA_SELF_FIFO_SIZE] + (gs[GS_RDMA_RSZ_FIFO_SIZE] << 16);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_RDMA_SRAM_CASCADE, val, ~0);
#endif
}

static void mtk_rdma_config(struct mtk_ddp_comp *comp,
			    struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
#if 0
	unsigned long long threshold;
	unsigned int reg;
#endif
	unsigned int w;
	struct mtk_disp_rdma *rdma = comp_to_rdma(comp);
	bool *rdma_memory_mode = comp->comp_mode;

	//for dual pipe one layer
	if (comp->mtk_crtc->is_dual_pipe) {
		w = cfg->w / 2;
		DDPFUNC();
	} else
		w = cfg->w;
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_SIZE_CON_0, w,
		       0x1fff);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_SIZE_CON_1, cfg->h,
		       0xfffff);

	if (*rdma_memory_mode == true) {
		rdma->dummy_w = w;
		rdma->dummy_h = cfg->h;
		mtk_ddp_write_mask(comp, MATRIX_INT_MTX_SEL_DEFAULT,
				   DISP_REG_RDMA_SIZE_CON_0, 0xff0000, handle);
		mtk_ddp_write_relaxed(comp, RDMA_DUMMY_BUFFER_PITCH(w),
				      DISP_RDMA_MEM_SRC_PITCH, handle);
		mtk_ddp_write_mask(comp, RDMA_MODE_MEMORY,
				   DISP_REG_RDMA_GLOBAL_CON, RDMA_MODE_MEMORY,
				   handle);
	} else {
		mtk_ddp_write_mask(comp, 0, DISP_REG_RDMA_SIZE_CON_0, 0xff0000,
				   handle);
		mtk_ddp_write_relaxed(comp, 0, DISP_RDMA_MEM_SRC_PITCH, handle);
		mtk_ddp_write_relaxed(comp, 0, DISP_REG_RDMA_MEM_CON, handle);
		mtk_ddp_write_mask(comp, 0, DISP_REG_RDMA_GLOBAL_CON,
				   RDMA_MODE_MEMORY, handle);
		mtk_ddp_write_relaxed(comp, 0, DISP_RDMA_MEM_START_ADDR,
				      handle);
	}

#if 0
	/*
	 * Enable FIFO underflow since DSI and DPI can't be blocked.
	 * Keep the FIFO pseudo size reset default of 8 KiB. Set the
	 * output threshold to 6 microseconds with 7/6 overhead to
	 * account for blanking, and with a pixel depth of 4 bytes:
	 */
	threshold = div_u64((unsigned long long)cfg->w * cfg->h *
				    cfg->vrefresh * 4 * 7,
			    1000000);
	reg = RDMA_FIFO_UNDERFLOW_EN |
	      RDMA_FIFO_PSEUDO_SIZE(RDMA_FIFO_SIZE(rdma)) |
	      RDMA_OUTPUT_VALID_FIFO_THRESHOLD(threshold);
	cmdq_pkt_write(handle, comp->cmdq_base,
		       comp->regs_pa + DISP_REG_RDMA_FIFO_CON, reg, ~0);
#endif

	mtk_rdma_set_ultra_l(comp, cfg, handle);
}

static void mtk_rdma_backup_info_cmp(struct mtk_ddp_comp *comp, bool *compare)
{
	struct mtk_disp_rdma *rdma = comp_to_rdma(comp);
	void __iomem *baddr = comp->regs;
	unsigned long addr;

	addr = readl(DISP_REG_RDMA_MEM_START_ADDR + baddr);

	if (addr == 0 || (addr != 0 && rdma->backup_info.addr != addr))
		*compare = 1;
	else
		*compare = 0;
	rdma->backup_info.addr = addr;
}

static int mtk_rdma_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			   enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = 0;

	switch (io_cmd) {
	case MTK_IO_CMD_RDMA_GOLDEN_SETTING: {
		struct mtk_ddp_config *cfg;

		cfg = (struct mtk_ddp_config *)params;
		mtk_rdma_set_ultra_l(comp, cfg, handle);
		break;
	}
	case IRQ_LEVEL_ALL: {
		unsigned int inten;

		inten = RDMA_FRAME_START_INT | RDMA_FRAME_END_INT |
			RDMA_EOF_ABNORMAL_INT | RDMA_FIFO_UNDERFLOW_INT |
			RDMA_TARGET_LINE_INT;
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_RDMA_INT_ENABLE, inten,
			       inten);
		break;
	}
	case IRQ_LEVEL_IDLE: {
		unsigned int inten;

		inten = RDMA_REG_UPDATE_INT | RDMA_FRAME_START_INT |
			RDMA_FRAME_END_INT | RDMA_TARGET_LINE_INT;
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_REG_RDMA_INT_ENABLE, 0,
			       inten);
		break;
	}
#ifdef MTK_FB_MMDVFS_SUPPORT
	case PMQOS_SET_HRT_BW: {
		bool *rdma_memory_mode = comp->comp_mode;
		u32 bw_val = *(unsigned int *)params;
		struct mtk_ddp_comp *output_comp;

		output_comp = mtk_ddp_comp_request_output(comp->mtk_crtc);
		if (*rdma_memory_mode == true) {
			if (output_comp)
				mtk_ddp_comp_io_cmd(output_comp, NULL,
					GET_FRAME_HRT_BW_BY_DATARATE, &bw_val);
			ret = RDMA_REQ_HRT;
		}
		__mtk_disp_set_module_hrt(&comp->hrt_qos_req, bw_val);

		break;
	}
#endif
	case BACKUP_INFO_CMP: {
		mtk_rdma_backup_info_cmp(comp, params);
		break;
	}
	default:
		break;
	}

	return ret;
}

void mtk_rdma_dump_golden_setting(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	unsigned int value;

	DDPDUMP("-- %s Golden Setting --\n", mtk_dump_comp_str(comp));
	DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x 0x%03x:0x%08x 0x%03x:0x%08x\n",
		0x30, readl(DISP_REG_RDMA_MEM_GMC_S0 + baddr),
		0x34, readl(DISP_REG_RDMA_MEM_GMC_S1 + baddr),
		0x3c, readl(DISP_REG_RDMA_MEM_GMC_S2 + baddr),
		0x40, readl(DISP_REG_RDMA_FIFO_CON + baddr));
	DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x 0x%03x:0x%08x 0x%03x:0x%08x\n",
		0xa8, readl(DISP_REG_RDMA_THRESHOLD_FOR_SODI + baddr),
		0xac, readl(DISP_REG_RDMA_THRESHOLD_FOR_DVFS + baddr),
		0xb0, readl(DISP_REG_RDMA_SRAM_SEL + baddr),
		0xc8, readl(DISP_RDMA_SRAM_CASCADE + baddr));
	DDPDUMP("0x%03x:0x%08x 0x%08x 0x%08x 0x%08x\n",
		0xd0, readl(DISP_REG_RDMA_DVFS_SETTING_PRE + baddr),
		readl(DISP_REG_RDMA_DVFS_SETTING_ULTRA + baddr),
		readl(DISP_REG_RDMA_LEAVE_DRS_SETTING + baddr),
		readl(DISP_REG_RDMA_ENTER_DRS_SETTING + baddr));
#if !defined(CONFIG_MACH_MT6833)
	DDPDUMP("0x%03x:0x%08x 0x%03x:0x%08x\n",
		0xe8, readl(DISP_REG_RDMA_MEM_GMC_S3 + baddr),
		0x1a8, readl(DISP_REG_RDMA_GREQ_URG_NUM_SEL + baddr));
#else
	DDPDUMP("0x%03x:0x%08x\n",
		0xe8, readl(DISP_REG_RDMA_MEM_GMC_S3 + baddr));
#endif


	value = readl(DISP_REG_RDMA_MEM_GMC_S0 + baddr);
	DDPDUMP("GMC_SETTING_0 [11:0]:%u [27:16]:%u [30]:%u [31]:%u\n",
		REG_FLD_VAL_GET(
			MEM_GMC_S0_FLD_PRE_ULTRA_THRESHOLD_LOW, value),
		REG_FLD_VAL_GET(
			MEM_GMC_S0_FLD_PRE_ULTRA_THRESHOLD_HIGH, value),
		REG_FLD_VAL_GET(
			MEM_GMC_S0_FLD_RG_VALID_THRESHOLD_FORCE_PREULTRA,
			value),
		REG_FLD_VAL_GET(
			MEM_GMC_S0_FLD_RG_VDE_FORCE_PREULTRA, value));

	value = readl(DISP_REG_RDMA_MEM_GMC_S1 + baddr);
	DDPDUMP("GMC_SETTING_1 [11:0]:%u [27:16]:%u [30]:%u [31]:%u\n",
		REG_FLD_VAL_GET(MEM_GMC_S1_FLD_ULTRA_THRESHOLD_LOW, value),
		REG_FLD_VAL_GET(MEM_GMC_S1_FLD_ULTRA_THRESHOLD_HIGH, value),
		REG_FLD_VAL_GET(
			MEM_GMC_S1_FLD_RG_VALID_THRESHOLD_BLOCK_ULTRA, value),
		REG_FLD_VAL_GET(
			MEM_GMC_S1_FLD_RG_VDE_BLOCK_ULTRA, value));

	value = readl(DISP_REG_RDMA_MEM_GMC_S2 + baddr);
	DDPDUMP("GMC_SETTING_2 [11:0]:%u\n",
		REG_FLD_VAL_GET(MEM_GMC_S2_FLD_ISSUE_REQ_THRESHOLD, value));

	value = readl(DISP_REG_RDMA_FIFO_CON + baddr);
	DDPDUMP("FIFO_CON [11:0]:%u [27:16]:%d [31]:%u\n",
		REG_FLD_VAL_GET(
			FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD, value),
		REG_FLD_VAL_GET(FIFO_CON_FLD_FIFO_PSEUDO_SIZE, value),
		REG_FLD_VAL_GET(FIFO_CON_FLD_FIFO_UNDERFLOW_EN, value));

	value = readl(DISP_REG_RDMA_THRESHOLD_FOR_SODI + baddr);
	DDPDUMP("THRSHOLD_SODI [11:0]:%u [27:16]:%u\n",
		REG_FLD_VAL_GET(RDMA_THRESHOLD_FOR_SODI_FLD_LOW, value),
		REG_FLD_VAL_GET(RDMA_THRESHOLD_FOR_SODI_FLD_HIGH, value));

	value = readl(DISP_REG_RDMA_THRESHOLD_FOR_DVFS + baddr);
	DDPDUMP("THRSHOLD_DVFS [11:0]:%u [27:16]:%u\n",
		REG_FLD_VAL_GET(RDMA_THRESHOLD_FOR_DVFS_FLD_LOW, value),
		REG_FLD_VAL_GET(RDMA_THRESHOLD_FOR_DVFS_FLD_HIGH, value));

	DDPDUMP("SRAM_SEL [0]:%u\n", readl(DISP_REG_RDMA_SRAM_SEL + baddr));

#if 0
	value = readl(DISP_RDMA_SRAM_CASCADE + baddr);
	DDPDUMP("SRAM_CASCADE [13:0]:%u [27:16]:%u\n",
		REG_FLD_VAL_GET(RG_DISP_RDMA_FIFO_SIZE, value),
		REG_FLD_VAL_GET(RG_DISP_RDMA_RSZ_FIFO_SIZE, value));
#endif

	value = readl(DISP_REG_RDMA_DVFS_SETTING_PRE + baddr);
	DDPDUMP("DVFS_SETTING_PREULTRA [11:0]:%u [27:16]:%u\n",
		REG_FLD_VAL_GET(RG_DVFS_PRE_ULTRA_THRESHOLD_LOW, value),
		REG_FLD_VAL_GET(RG_DVFS_PRE_ULTRA_THRESHOLD_HIGH, value));

	value = readl(DISP_REG_RDMA_DVFS_SETTING_ULTRA + baddr);
	DDPDUMP("DVFS_SETTING_ULTRA [11:0]:%u [27:16]:%u\n",
		REG_FLD_VAL_GET(RG_DVFS_ULTRA_THRESHOLD_LOW, value),
		REG_FLD_VAL_GET(RG_DVFS_ULTRA_THRESHOLD_HIGH, value));

	value = readl(DISP_REG_RDMA_LEAVE_DRS_SETTING + baddr);
	DDPDUMP("LEAVE_DRS_SETTING [11:0]:%u [27:16]:%u\n",
		REG_FLD_VAL_GET(RG_IS_DRS_STATUS_THRESHOLD_LOW, value),
		REG_FLD_VAL_GET(RG_IS_DRS_STATUS_THRESHOLD_HIGH, value));

	value = readl(DISP_REG_RDMA_ENTER_DRS_SETTING + baddr);
	DDPDUMP("ENTER_DRS_SETTING [11:0]:%u [27:16]:%u\n",
		REG_FLD_VAL_GET(RG_NOT_DRS_STATUS_THRESHOLD_LOW, value),
		REG_FLD_VAL_GET(RG_NOT_DRS_STATUS_THRESHOLD_HIGH, value));

	value = readl(DISP_REG_RDMA_MEM_GMC_S3 + baddr);
	DDPDUMP("GMC_SETTING_3 [11:0]:%u [27:16]:%u\n",
		REG_FLD_VAL_GET(FLD_LOW_FOR_URGENT, value),
		REG_FLD_VAL_GET(FLD_HIGH_FOR_URGENT, value));

#if !defined(CONFIG_MACH_MT6833)
	value = readl(DISP_REG_RDMA_GREQ_URG_NUM_SEL + baddr);
	DDPDUMP("GREQ URG NUM SEL [29:29]: %u\n",
		REG_FLD_VAL_GET(FLD_RG_LAYER_SMI_ID_EN, value));
#endif
}

int mtk_rdma_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	if (mtk_ddp_comp_helper_get_opt(comp,
					MTK_DRM_OPT_REG_PARSER_RAW_DUMP)) {
		unsigned int i = 0;

		for (i = 0; i < 0x200; i += 0x10)
			mtk_serial_dump_reg(baddr, i, 4);
	} else {
		DDPDUMP("(0x000)R_INTEN=0x%x\n",
			readl(DISP_REG_RDMA_INT_ENABLE + baddr));
		DDPDUMP("(0x004)R_INTS=0x%x\n",
			readl(DISP_REG_RDMA_INT_STATUS + baddr));
		DDPDUMP("(0x010)R_CON=0x%x\n",
			readl(DISP_REG_RDMA_GLOBAL_CON + baddr));
		DDPDUMP("(0x014)R_SIZE0=0x%x\n",
			readl(DISP_REG_RDMA_SIZE_CON_0 + baddr));
		DDPDUMP("(0x018)R_SIZE1=0x%x\n",
			readl(DISP_REG_RDMA_SIZE_CON_1 + baddr));
		DDPDUMP("(0x01c)R_TAR_LINE=0x%x\n",
			readl(DISP_REG_RDMA_TARGET_LINE + baddr));
		DDPDUMP("(0x024)R_M_CON=0x%x\n",
			readl(DISP_REG_RDMA_MEM_CON + baddr));
		DDPDUMP("(0xf00)R_M_S_ADDR=0x%x\n",
			readl(DISP_REG_RDMA_MEM_START_ADDR + baddr));
		DDPDUMP("(0x02c)R_M_SRC_PITCH=0x%x\n",
			readl(DISP_REG_RDMA_MEM_SRC_PITCH + baddr));
		DDPDUMP("(0x030)R_M_GMC_SET0=0x%x\n",
			readl(DISP_REG_RDMA_MEM_GMC_S0 + baddr));
		DDPDUMP("(0x034)R_M_GMC_SET1=0x%x\n",
			readl(DISP_REG_RDMA_MEM_GMC_S1 + baddr));
		DDPDUMP("(0x038)R_M_SLOW_CON=0x%x\n",
			readl(DISP_REG_RDMA_MEM_SLOW_CON + baddr));
		DDPDUMP("(0x03c)R_M_GMC_SET2=0x%x\n",
			readl(DISP_REG_RDMA_MEM_GMC_S2 + baddr));
		DDPDUMP("(0x040)R_FIFO_CON=0x%x\n",
			readl(DISP_REG_RDMA_FIFO_CON + baddr));
		DDPDUMP("(0x044)R_FIFO_LOG=0x%x\n",
			readl(DISP_REG_RDMA_FIFO_LOG + baddr));
		DDPDUMP("(0x078)R_PRE_ADD0=0x%x\n",
			readl(DISP_REG_RDMA_PRE_ADD_0 + baddr));
		DDPDUMP("(0x07c)R_PRE_ADD1=0x%x\n",
			readl(DISP_REG_RDMA_PRE_ADD_1 + baddr));
		DDPDUMP("(0x080)R_PRE_ADD2=0x%x\n",
			readl(DISP_REG_RDMA_PRE_ADD_2 + baddr));
		DDPDUMP("(0x084)R_POST_ADD0=0x%x\n",
			readl(DISP_REG_RDMA_POST_ADD_0 + baddr));
		DDPDUMP("(0x088)R_POST_ADD1=0x%x\n",
			readl(DISP_REG_RDMA_POST_ADD_1 + baddr));
		DDPDUMP("(0x08c)R_POST_ADD2=0x%x\n",
			readl(DISP_REG_RDMA_POST_ADD_2 + baddr));
		DDPDUMP("(0x090)R_DUMMY=0x%x\n",
			readl(DISP_REG_RDMA_DUMMY + baddr));
		DDPDUMP("(0x094)R_OUT_SEL=0x%x\n",
			readl(DISP_REG_RDMA_DEBUG_OUT_SEL + baddr));
		DDPDUMP("(0xf00)R_M_START=0x%x\n",
			readl(DISP_REG_RDMA_MEM_START_ADDR + baddr));
		DDPDUMP("(0x0a0)R_BG_CON_0=0x%x\n",
			readl(DISP_REG_RDMA_BG_CON_0 + baddr));
		DDPDUMP("(0x0a4)R_BG_CON_1=0x%x\n",
			readl(DISP_REG_RDMA_BG_CON_1 + baddr));
		DDPDUMP("(0x0a8)R_FOR_SODI=0x%x\n",
			readl(DISP_REG_RDMA_THRESHOLD_FOR_SODI + baddr));
		DDPDUMP("(0x0ac)R_FOR_DVFS=0x%x\n",
			readl(DISP_REG_RDMA_THRESHOLD_FOR_DVFS + baddr));
		DDPDUMP("(0x0b0)R_FOR_SRAM=0x%x\n",
			readl(DISP_REG_RDMA_SRAM_SEL + baddr));
		DDPDUMP("(0x0b4)DISP_REG_RDMA_STALL_CG_CON=0x%x\n",
			readl(DISP_REG_RDMA_STALL_CG_CON + baddr));
#if defined(CONFIG_MACH_MT6885) || defined(CONFIG_MACH_MT6893)
		DDPDUMP("(0x0b8)DISP_REG_RDMA_SHADOW_UPDATE=0x%x\n",
			readl(DISP_REG_RDMA_SHADOW_UPDATE + baddr));
#elif defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6877) || defined(CONFIG_MACH_MT6781)
		DDPDUMP("(0x0bc)DISP_REG_RDMA_SHADOW_UPDATE=0x%x\n",
			readl(DISP_REG_RDMA_SHADOW_UPDATE + baddr));
#endif
		DDPDUMP("(0x0c8)DISP_RDMA_SRAM_CASCADE=0x%x\n",
			readl(DISP_RDMA_SRAM_CASCADE + baddr));
		DDPDUMP("(0x0d0)DISP_REG_RDMA_DVFS_SETTING_PRE=0x%x\n",
			readl(DISP_REG_RDMA_DVFS_SETTING_PRE + baddr));
		DDPDUMP("(0x0d4)DISP_REG_RDMA_DVFS_SETTING_ULTRA=0x%x\n",
			readl(DISP_REG_RDMA_DVFS_SETTING_ULTRA + baddr));
		DDPDUMP("(0x0d8)DISP_REG_RDMA_LEAVE_DRS_SETTING=0x%x\n",
			readl(DISP_REG_RDMA_LEAVE_DRS_SETTING + baddr));
		DDPDUMP("(0x0dc)DISP_REG_RDMA_ENTER_DRS_SETTING=0x%x\n",
			readl(DISP_REG_RDMA_ENTER_DRS_SETTING + baddr));
		DDPDUMP("(0x0e0)DISP_REG_RDMA_CROP_CON_0=0x%x\n",
			readl(DISP_REG_RDMA_CROP_CON_1 + baddr));
		DDPDUMP("(0x0e4)DISP_REG_RDMA_CROP_CON_1=0x%x\n",
			readl(DISP_REG_RDMA_CROP_CON_0 + baddr));
		DDPDUMP("(0x0e8)DISP_REG_RDMA_MEM_GMC_SETTING_3=0x%x\n",
			readl(DISP_REG_RDMA_MEM_GMC_S3 + baddr));
		DDPDUMP("(0x0f0)R_IN_PXL_CNT=0x%x\n",
			readl(DISP_REG_RDMA_IN_P_CNT + baddr));
		DDPDUMP("(0x0f4)R_IN_LINE_CNT=0x%x\n",
			readl(DISP_REG_RDMA_IN_LINE_CNT + baddr));
		DDPDUMP("(0x0f8)R_OUT_PXL_CNT=0x%x\n",
			readl(DISP_REG_RDMA_OUT_P_CNT + baddr));
		DDPDUMP("(0x0fc)R_OUT_LINE_CNT=0x%x\n",
			readl(DISP_REG_RDMA_OUT_LINE_CNT + baddr));
		DDPDUMP("(0x100)0x%x\n", readl(DISP_REG_RDMA_DBG_OUT + baddr));
		DDPDUMP("(0x10c)0x%x\n", readl(DISP_REG_RDMA_DBG_OUT1 + baddr));
		DDPDUMP("(0x110)0x%x\n", readl(DISP_REG_RDMA_DBG_OUT2 + baddr));
		DDPDUMP("(0x114)0x%x\n", readl(DISP_REG_RDMA_DBG_OUT3 + baddr));
		DDPDUMP("(0x118)0x%x\n", readl(DISP_REG_RDMA_DBG_OUT4 + baddr));
		DDPDUMP("(0x11c)0x%x\n", readl(DISP_REG_RDMA_DBG_OUT5 + baddr));
		DDPDUMP("(0x120)0x%x\n", readl(DISP_REG_RDMA_DBG_120 + baddr));
		DDPDUMP("(0x124)0x%x\n", readl(DISP_REG_RDMA_DBG_124 + baddr));
		DDPDUMP("(0x12c)0x%x\n", readl(DISP_REG_RDMA_DBG_12c + baddr));
		DDPDUMP("(0x130)0x%x\n", readl(DISP_REG_RDMA_DBG_130 + baddr));
		DDPDUMP("(0x134)0x%x\n", readl(DISP_REG_RDMA_DBG_134 + baddr));
		DDPDUMP("(0x138)0x%x\n", readl(DISP_REG_RDMA_DBG_138 + baddr));
	}

	mtk_rdma_dump_golden_setting(comp);

	return 0;
}

int mtk_rdma_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	unsigned int global_ctrl;
	unsigned int bg0 = readl(baddr + DISP_REG_RDMA_BG_CON_0);
	unsigned int bg1 = readl(baddr + DISP_REG_RDMA_BG_CON_1);
	unsigned int fifo = readl(baddr + DISP_REG_RDMA_FIFO_CON);

	global_ctrl = readl(DISP_REG_RDMA_GLOBAL_CON + baddr);
	DDPDUMP("== %s ANALYSIS ==\n", mtk_dump_comp_str(comp));
	DDPDUMP("en=%d,mode:%s,smi_busy:%d\n",
		REG_FLD_VAL_GET(GLOBAL_CON_FLD_ENGINE_EN, global_ctrl),
		REG_FLD_VAL_GET(GLOBAL_CON_FLD_MODE_SEL, global_ctrl)
				? "mem" : "DL",
		REG_FLD_VAL_GET(GLOBAL_CON_FLD_SMI_BUSY, global_ctrl));
	DDPDUMP("wh(%dx%d),pitch=%d,addr=0x%08x\n",
		readl(DISP_REG_RDMA_SIZE_CON_0 + baddr) & 0xfff,
		readl(DISP_REG_RDMA_SIZE_CON_1 + baddr) & 0xfffff,
		readl(DISP_REG_RDMA_MEM_SRC_PITCH + baddr),
		readl(DISP_REG_RDMA_MEM_START_ADDR + baddr));
	DDPDUMP("fifo_sz=%u,output_valid_threshold=%u,fifo_min=%d\n",
#if 0 /* TODO */
		unified_color_fmt_name(display_fmt_reg_to_unified_fmt(
				(readl(DISP_REG_RDMA_MEM_CON +
				       baddr) >> 4) & 0xf,
				(readl(DISP_REG_RDMA_MEM_CON +
				       baddr) >> 8) & 0x1, 0)),
#endif
		REG_FLD_VAL_GET(FIFO_CON_FLD_FIFO_PSEUDO_SIZE, fifo),
		REG_FLD_VAL_GET(FIFO_CON_FLD_OUTPUT_VALID_FIFO_THRESHOLD, fifo),
		readl(DISP_REG_RDMA_FIFO_LOG + baddr));
	DDPDUMP("pos:in(%d,%d)out(%d,%d),bg(t%d,b%d,l%d,r%d)\n",
		readl(DISP_REG_RDMA_IN_P_CNT + baddr),
		readl(DISP_REG_RDMA_IN_LINE_CNT + baddr),
		readl(DISP_REG_RDMA_OUT_P_CNT + baddr),
		readl(DISP_REG_RDMA_OUT_LINE_CNT + baddr),
		REG_FLD_VAL_GET(RDMA_BG_CON_1_TOP, bg1),
		REG_FLD_VAL_GET(RDMA_BG_CON_1_BOTTOM, bg1),
		REG_FLD_VAL_GET(RDMA_BG_CON_0_LEFT, bg0),
		REG_FLD_VAL_GET(RDMA_BG_CON_0_RIGHT, bg0));
#if 0 /* TODO */
	DDPDUMP("irq cnt:start=%d,end=%d,underflow=%d,targetline=%d\n",
		rdma_start_irq_cnt[idx], rdma_done_irq_cnt[idx],
		rdma_underflow_irq_cnt[idx], rdma_targetline_irq_cnt[idx]);
#endif

	return 0;
}

static void mtk_rdma_prepare(struct mtk_ddp_comp *comp)
{
#if defined(CONFIG_DRM_MTK_SHADOW_REGISTER_SUPPORT)
	struct mtk_disp_rdma *rdma = comp_to_rdma(comp);
#endif

	mtk_ddp_comp_clk_prepare(comp);

#if defined(CONFIG_DRM_MTK_SHADOW_REGISTER_SUPPORT)
	if (rdma->data->support_shadow) {
		/* Enable shadow register and read shadow register */
		mtk_ddp_write_mask_cpu(comp, 0x0,
			DISP_REG_RDMA_SHADOW_UPDATE, RDMA_BYPASS_SHADOW);
	} else {
		/* Bypass shadow register and read shadow register */
		mtk_ddp_write_mask_cpu(comp, RDMA_BYPASS_SHADOW,
			DISP_REG_RDMA_SHADOW_UPDATE, RDMA_BYPASS_SHADOW);
	}
#else
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6877) || defined(CONFIG_MACH_MT6781)
	/* Bypass shadow register and read shadow register */
	mtk_ddp_write_mask_cpu(comp, RDMA_BYPASS_SHADOW,
		DISP_REG_RDMA_SHADOW_UPDATE, RDMA_BYPASS_SHADOW);
#endif
#endif
}

static void mtk_rdma_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

static unsigned int rdma_fmt_convert(struct mtk_disp_rdma *rdma,
				     unsigned int fmt)
{
	switch (fmt) {
	default:
	case DRM_FORMAT_RGB565:
		return MEM_MODE_INPUT_FORMAT_RGB565;
	case DRM_FORMAT_BGR565:
		return MEM_MODE_INPUT_FORMAT_RGB565 | MEM_MODE_INPUT_SWAP;
	case DRM_FORMAT_RGB888:
		return MEM_MODE_INPUT_FORMAT_RGB888;
	case DRM_FORMAT_BGR888:
		return MEM_MODE_INPUT_FORMAT_RGB888 | MEM_MODE_INPUT_SWAP;
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA8888:
		return MEM_MODE_INPUT_FORMAT_ARGB8888;
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA8888:
		return MEM_MODE_INPUT_FORMAT_ARGB8888 | MEM_MODE_INPUT_SWAP;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		return MEM_MODE_INPUT_FORMAT_RGBA8888;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		return MEM_MODE_INPUT_FORMAT_RGBA8888 | MEM_MODE_INPUT_SWAP;
	case DRM_FORMAT_UYVY:
		return MEM_MODE_INPUT_FORMAT_UYVY;
	case DRM_FORMAT_YUYV:
		return MEM_MODE_INPUT_FORMAT_YUYV;
	}
}

static void mtk_rdma_layer_config(struct mtk_ddp_comp *comp, unsigned int idx,
				  struct mtk_plane_state *state,
				  struct cmdq_pkt *handle)
{
	struct mtk_disp_rdma *rdma = comp_to_rdma(comp);
	struct mtk_rdma_cfg_info *cfg_info = &rdma->cfg_info;
	struct mtk_plane_pending_state *pending = &state->pending;
	unsigned int addr = pending->addr;
	unsigned int pitch = pending->pitch & 0xffff;
	unsigned int fmt = pending->format;
	unsigned int con;

	if (pending->height == 0u || pending->width == 0u)
		return;

	DDPINFO("%s addr: 0x%x\n", __func__, addr);

	con = rdma_fmt_convert(rdma, fmt);
	mtk_ddp_write_relaxed(comp, con, DISP_RDMA_MEM_CON, handle);
	if (fmt == DRM_FORMAT_UYVY || fmt == DRM_FORMAT_YUYV)
		mtk_ddp_write_mask(comp,
				   RDMA_MATRIX_ENABLE | RDMA_MATRIX_INT_MTX_SEL,
				   DISP_REG_RDMA_SIZE_CON_0, 0xff0000, handle);
	else
		mtk_ddp_write_mask(comp, MATRIX_INT_MTX_SEL_DEFAULT,
				   DISP_REG_RDMA_SIZE_CON_0, 0xff0000, handle);

	mtk_ddp_write_relaxed(comp, addr, DISP_RDMA_MEM_START_ADDR, handle);
	mtk_ddp_write_relaxed(comp, pitch, DISP_RDMA_MEM_SRC_PITCH, handle);

	cfg_info->addr = addr;
	cfg_info->width = pending->width;
	cfg_info->height = pending->height;
	cfg_info->fmt = fmt;
}

int MMPathTraceRDMA(struct mtk_ddp_comp *ddp_comp, char *str,
	unsigned int strlen, unsigned int n)
{
	struct mtk_disp_rdma *rdma = comp_to_rdma(ddp_comp);
	struct mtk_rdma_cfg_info *cfg_info = &rdma->cfg_info;

	n += scnprintf(str + n, strlen - n,
		"in=0x%x, in_width=%d, in_height=%d, in_fmt=%s, in_bpp=%d, ",
		cfg_info->addr,
		cfg_info->width,
		cfg_info->height,
		mtk_get_format_name(cfg_info->fmt),
		mtk_get_format_bpp(cfg_info->fmt));

	return n;
}

static const struct mtk_ddp_comp_funcs mtk_disp_rdma_funcs = {
	.config = mtk_rdma_config,
	.start = mtk_rdma_start,
	.stop = mtk_rdma_stop,
#if 0
	.enable_vblank = mtk_rdma_enable_vblank,
	.disable_vblank = mtk_rdma_disable_vblank,
#endif
	.io_cmd = mtk_rdma_io_cmd,
	.prepare = mtk_rdma_prepare,
	.unprepare = mtk_rdma_unprepare,
	.layer_config = mtk_rdma_layer_config,
};

static int mtk_disp_rdma_bind(struct device *dev, struct device *master,
			      void *data)
{
	int ret;
	struct mtk_disp_rdma *priv = dev_get_drvdata(dev);
	struct mtk_ddp_comp *comp = &priv->ddp_comp;
	struct mtk_disp_rdma *rdma = comp_to_rdma(comp);
	struct drm_device *drm_dev = data;
#ifdef MTK_FB_MMDVFS_SUPPORT
	struct mtk_drm_private *drm_priv = drm_dev->dev_private;
	int qos_req_port;
#endif

	DDPINFO("%s\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	rdma->drm_dev = drm_dev;

	comp->comp_mode = &priv->rdma_memory_mode;

#ifdef MTK_FB_MMDVFS_SUPPORT
	qos_req_port = __mtk_disp_pmqos_port_look_up(priv->ddp_comp.id);
	if (qos_req_port < 0) {
		DDPPR_ERR("Failed to request QOS port\n");
	} else {
		mm_qos_add_request(&drm_priv->bw_request_list,
				   &priv->ddp_comp.qos_req, qos_req_port);
		mm_qos_add_request(&drm_priv->hrt_request_list,
				   &priv->ddp_comp.hrt_qos_req, qos_req_port);
	}
#endif

	return 0;
}

static void mtk_disp_rdma_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_disp_rdma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_rdma_component_ops = {
	.bind = mtk_disp_rdma_bind, .unbind = mtk_disp_rdma_unbind,
};

static int mtk_disp_rdma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_rdma *priv;
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

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_RDMA);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_rdma_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	/* Disable and clear pending interrupts */
	writel(0x0, priv->ddp_comp.regs + DISP_REG_RDMA_INT_ENABLE);
	writel(0x0, priv->ddp_comp.regs + DISP_REG_RDMA_INT_STATUS);

	ret = devm_request_irq(dev, irq, mtk_disp_rdma_irq_handler,
			       IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev),
			       priv);
	if (ret < 0) {
		DDPAEE("%s:%d, failed to request irq:%d ret:%d comp_id:%d\n",
				__func__, __LINE__,
				irq, ret, comp_id);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_disp_rdma_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_rdma_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_rdma_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct mtk_disp_rdma_data mt2701_rdma_driver_data = {
	.fifo_size = SZ_4K,
	.support_shadow = false,
};

static const struct mtk_disp_rdma_data mt6779_rdma_driver_data = {
	.fifo_size = SZ_8K + SZ_16K,
	.sodi_config = mt6779_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_rdma_data mt8173_rdma_driver_data = {
	.fifo_size = SZ_8K,
	.support_shadow = false,
};

static const struct mtk_disp_rdma_data mt6885_rdma_driver_data = {
	.fifo_size = SZ_1K * 3 + SZ_32K,
	.sodi_config = mt6885_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_rdma_data mt6873_rdma_driver_data = {
	.fifo_size = SZ_1K * 3 + SZ_32K,
	.sodi_config = mt6873_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_rdma_data mt6853_rdma_driver_data = {
	.fifo_size = SZ_1K * 3 + SZ_32K,
	.sodi_config = mt6853_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_rdma_data mt6877_rdma_driver_data = {
	.fifo_size = SZ_1K * 3 + SZ_32K,
	.sodi_config = mt6877_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_rdma_data mt6833_rdma_driver_data = {
	.fifo_size = SZ_1K * 3 + SZ_32K,
	.sodi_config = mt6833_mtk_sodi_config,
	.support_shadow = false,
};

static const struct mtk_disp_rdma_data mt6781_rdma_driver_data = {
	.fifo_size = SZ_1K * 3 + SZ_32K,
	.sodi_config = mt6781_mtk_sodi_config,
	.support_shadow = false,
};

static const struct of_device_id mtk_disp_rdma_driver_dt_match[] = {
	{.compatible = "mediatek,mt2701-disp-rdma",
	 .data = &mt2701_rdma_driver_data},
	{.compatible = "mediatek,mt6779-disp-rdma",
	 .data = &mt6779_rdma_driver_data},
	{.compatible = "mediatek,mt8173-disp-rdma",
	 .data = &mt8173_rdma_driver_data},
	{.compatible = "mediatek,mt6885-disp-rdma",
	 .data = &mt6885_rdma_driver_data},
	{.compatible = "mediatek,mt6873-disp-rdma",
	 .data = &mt6873_rdma_driver_data},
	{.compatible = "mediatek,mt6853-disp-rdma",
	 .data = &mt6853_rdma_driver_data},
	{.compatible = "mediatek,mt6877-disp-rdma",
	 .data = &mt6877_rdma_driver_data},
	{.compatible = "mediatek,mt6833-disp-rdma",
	 .data = &mt6833_rdma_driver_data},
	{.compatible = "mediatek,mt6781-disp-rdma",
	 .data = &mt6781_rdma_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_rdma_driver_dt_match);

struct platform_driver mtk_disp_rdma_driver = {
	.probe = mtk_disp_rdma_probe,
	.remove = mtk_disp_rdma_remove,
	.driver = {

			.name = "mediatek-disp-rdma",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_rdma_driver_dt_match,
		},
};
