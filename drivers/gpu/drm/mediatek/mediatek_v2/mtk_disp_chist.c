// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_disp_chist.h"
#include "mtk_dump.h"

#define DISP_CHIST_COLOR_FORMAT 0x3ff
/* channel 0~3 has 256 bins, 4~6 has 128 bins */
#define DISP_CHIST_MAX_BIN 256
#define DISP_CHIST_MAX_BIN_LOW 128

/* chist custom info */
#define DISP_CHIST_HWC_CHANNEL_INDEX 4
/* chist custom info end*/

#define DISP_CHIST_YUV_PARAM_COUNT  12
#define DISP_CHIST_POST_PARAM_INDEX 9

#define DISP_CHIST_CHANNEL_COUNT 7
#define CHIST_NUM 2
#define DISP_CHIST_MAX_RGB 0x0321

#define DISP_CHIST_DUAL_PIPE_OVERLAP 0

#define DISP_CHIST_EN                0x0
#define DISP_CHIST_INTEN             0x08
#define DISP_CHIST_INSTA             0x0C
#define DISP_CHIST_CFG               0x20
#define DISP_CHIST_SIZE              0x30
#define DISP_CHIST_CONFIG            0x40
#define DISP_CHIST_Y2R_PAPA_R0       0x50
#define DISP_CHIST_Y2R_PAPA_POST_A0  0x80
#define DISP_CHIST_SHADOW_CTRL       0xF0
// channel_n_win_x_main = DISP_CHIST_CH0_WIN_X_MAIN + n * 0x10
#define DISP_CHIST_CH0_WIN_X_MAIN    0x0460
#define DISP_CHIST_CH0_WIN_Y_MAIN    0x0464
#define DISP_CHIST_CH0_BLOCK_INFO    0x0468
#define DISP_CHIST_CH0_BLOCK_CROP    0x046C
#define DISP_CHIST_WEIGHT            0x0500
#define DISP_CHIST_BLD_CONFIG        0x0504
#define DISP_CHIST_HIST_CH_CFG1      0x0510
#define DISP_CHIST_HIST_CH_CFG2      0x0514
#define DISP_CHIST_HIST_CH_CFG3      0x0518
#define DISP_CHIST_HIST_CH_CFG4      0x051C
#define DISP_CHIST_HIST_CH_CFG5      0x0520
#define DISP_CHIST_HIST_CH_CFG6      0x0524
#define DISP_CHIST_HIST_CH_CNF0      0x0528
#define DISP_CHIST_HIST_CH_CNF1      0x052C
#define DISP_CHIST_HIST_CH_CH0_CNF0  0x0530
#define DISP_CHIST_HIST_CH_CH0_CNF1  0x0534
#define DISP_CHIST_HIST_CH_MON       0x0568

#define DISP_CHIST_APB_READ          0x0600
#define DISP_CHIST_SRAM_R_IF         0x0680

//#define DEBUG_UT_TEST

#ifdef DEBUG_UT_TEST
#define DISP_CHIST_SHIFT_NUM 0
#else
#define DISP_CHIST_SHIFT_NUM 5
#endif

static unsigned int g_chist_relay_value[2];
#define index_of_chist(module) ((module == DDP_COMPONENT_CHIST0) ? 0 : 1)

#define get_module_id(index) (index ? DDP_COMPONENT_CHIST1 : DDP_COMPONENT_CHIST0)

static bool debug_dump_hist;
static bool need_restore;

static DEFINE_SPINLOCK(g_chist_global_lock);
static DEFINE_SPINLOCK(g_chist_clock_lock);

static DECLARE_WAIT_QUEUE_HEAD(g_chist_get_irq_wq);

static atomic_t g_chist_get_irq[2] = {ATOMIC_INIT(0), ATOMIC_INIT(0)};

static unsigned int sel_index;

static int g_rgb_2_yuv[4][DISP_CHIST_YUV_PARAM_COUNT] = {
	// BT601 full
	{0x1322D,  0x25916,  0x74BC,   // RMR,RMG,RMB
	 0x7F5337, 0x7EACCA, 0x20000,  // GMR,GMG,GMB
	 0x20000,  0x7E5344, 0x7FACBD, // BMR,BMG,BMB
	 0X0,      0X7FF,    0X7FF},   // POST_RA,POST_GA,POST_BA}
	 // BT709 full
	{0xD9B3,   0x2D999,  0x49EE,   // RMR,RMG,RMB
	 0x7F8AAE, 0x7E76D0, 0x20000,  // GMR,GMG,GMB
	 0x20000,  0x7E30B4, 0x7FD10E, // BMR,BMG,BMB
	 0X0,      0X7FF,    0X7FF},   // POST_RA,POST_GA,POST_BA}
	 // BT601 limit
	{0x106F3,  0x2043A,  0x6441,   // RMR,RMG,RMB
	 0x7F6839, 0x7ED606, 0x1C1C1,  // GMR,GMG,GMB
	 0x1C1C1,  0x7E8763, 0x7FB6DC, // BMR,BMG,BMB
	 0X100,      0X7FF,    0X7FF},    // POST_RA,POST_GA,POST_BA}
	 // BT709 limit
	{0xBAF7,   0x27298,  0x3F7E,   // RMR,RMG,RMB
	 0x7F98F1, 0x7EA69D, 0x1C1C1,  // GMR,GMG,GMB
	 0x1C1C1,  0x7E6907, 0x7FD6C3, // BMR,BMG,BMB
	 0X100,      0X7FF,    0X7FF}  // POST_RA,POST_GA,POST_BA
};

static atomic_t g_chist_is_clock_on[2] = { ATOMIC_INIT(0),
	ATOMIC_INIT(0)};

enum CHIST_IOCTL_CMD {
	CHIST_CONFIG = 0,
	CHIST_UNKNOWN,
};

struct mtk_disp_block_config {
	unsigned int blk_xofs;
	unsigned int left_column;
	unsigned int sum_column;
	int merge_column;
};

static struct mtk_disp_block_config g_chist_block_config[CHIST_NUM][DISP_CHIST_CHANNEL_COUNT];
static struct drm_mtk_channel_config g_chist_config[CHIST_NUM][DISP_CHIST_CHANNEL_COUNT];
static struct drm_mtk_channel_hist g_disp_hist[CHIST_NUM][DISP_CHIST_CHANNEL_COUNT];

static unsigned int present_fence[2];
static unsigned int g_pipe_width;
static unsigned int g_frame_width;
static unsigned int g_frame_height;

int mtk_drm_ioctl_get_chist_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_CHIST0];
	struct drm_mtk_chist_caps *caps_info = data;
	unsigned int i = 0, index = 0;
	struct drm_crtc *crtc;
	u32 width = 0, height = 0;

	crtc = list_first_entry(&(dev)->mode_config.crtc_list,
		typeof(*crtc), head);

	mtk_drm_crtc_get_panel_original_size(crtc, &width, &height);
	if (width == 0 || height == 0) {
		DDPFUNC("panel original size error(%dx%d).\n", width, height);
		width = crtc->mode.hdisplay;
		height = crtc->mode.vdisplay;
	}

	caps_info->lcm_width = width;
	caps_info->lcm_height = height;

	DDPINFO("%s chist id:%d, w:%d,h:%d\n", __func__, caps_info->device_id,
		caps_info->lcm_width, caps_info->lcm_height);
	// just call from pqservice, device_id:low 16bit=module_id, high 16bit=panel_id
	if (comp_to_chist(comp)->data->module_count > 1 && (caps_info->device_id & 0xffff))
		index = 1;

	caps_info->support_color = DISP_CHIST_COLOR_FORMAT;
	for (; i < DISP_CHIST_CHANNEL_COUNT; i++) {
		memcpy(&(caps_info->chist_config[i]),
			&g_chist_config[index][i], sizeof(g_chist_config[index][i]));
		// pqservice use channel 0, 1, 2, 3, if has one chist
		if (index == 0 && i >= DISP_CHIST_HWC_CHANNEL_INDEX)
			caps_info->chist_config[i].channel_id = DISP_CHIST_CHANNEL_COUNT;
		else
			caps_info->chist_config[i].channel_id = i;
	}
	DDPINFO("%s --\n", __func__);
	return 0;
}

int mtk_drm_ioctl_set_chist_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_mtk_chist_config *config = data;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_CHIST0];
	struct drm_crtc *crtc = private->crtc[0];
	int i = 0;

	if (config->config_channel_count == 0 ||
			config->config_channel_count > DISP_CHIST_CHANNEL_COUNT) {
		DDPPR_ERR("%s, invalid config channel count:%u\n",
				__func__, config->config_channel_count);
		return -EINVAL;
	}

	if (comp_to_chist(comp)->data->module_count > 1
		&& config->caller == MTK_DRM_CHIST_CALLER_PQ
		&& (config->device_id & 0xffff))
		comp = private->ddp_comp[DDP_COMPONENT_CHIST1];

	DDPINFO("%s  chist id:%d, caller:%d, config count:%d\n", __func__,
		config->device_id, config->caller, config->config_channel_count);

	if (config->caller == MTK_DRM_CHIST_CALLER_HWC) {
		unsigned int channel_id = DISP_CHIST_HWC_CHANNEL_INDEX;

		for (; i < config->config_channel_count &&
			channel_id < DISP_CHIST_CHANNEL_COUNT; i++) {
			config->chist_config[i].channel_id = channel_id;
			channel_id++;
		}
	} else {
#ifndef DEBUG_UT_TEST
		if (index_of_chist(comp->id) == 0
			&& config->config_channel_count > DISP_CHIST_HWC_CHANNEL_INDEX)
			config->config_channel_count = DISP_CHIST_HWC_CHANNEL_INDEX;
#endif
	}

	need_restore = 1;
	DDPINFO("%s --\n", __func__);
	return mtk_crtc_user_cmd(crtc, comp, CHIST_CONFIG, data);
}

static void disp_chist_set_interrupt(struct mtk_ddp_comp *comp,
					bool enabled, struct cmdq_pkt *handle)
{
	if (enabled && (readl(comp->regs + DISP_CHIST_INTEN) & 0x2))
		return;
	else if (!enabled && !(readl(comp->regs + DISP_CHIST_INTEN) & 0x2))
		return;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_INTEN, enabled ? 0x2 : 0, ~0);
}

static int disp_chist_copy_hist_to_user(struct drm_device *dev,
	struct drm_mtk_chist_info *hist)
{
	unsigned long flags;
	int ret = 0;
	unsigned int index = 0, i = 0;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_CHIST0];

	if (comp_to_chist(comp)->data->module_count > 1
		&& hist->caller == MTK_DRM_CHIST_CALLER_PQ
		&& (hist->device_id & 0xffff))
		comp = private->ddp_comp[DDP_COMPONENT_CHIST1];

	index = index_of_chist(comp->id);
	if (present_fence[index] == 0) {
		hist->present_fence = 0;
		DDPPR_ERR("%s, invalid present_fence:%d\n", __func__,
				present_fence[index]);
		return ret;
	}
	/* We assume only one thread will call this function */
	spin_lock_irqsave(&g_chist_global_lock, flags);

	for (; i < hist->get_channel_count; i++) {
		unsigned int channel_id = hist->channel_hist[i].channel_id;

		if (channel_id < DISP_CHIST_CHANNEL_COUNT &&
			g_chist_config[index][channel_id].enabled) {
			memcpy(&(hist->channel_hist[i]),
				&g_disp_hist[index][channel_id],
				sizeof(g_disp_hist[index][channel_id]));
		}
	}
	hist->present_fence = present_fence[index];

	spin_unlock_irqrestore(&g_chist_global_lock, flags);

	//dump all regs
	if (debug_dump_hist)
		mtk_chist_dump(comp);

	return ret;
}

static bool mtk_chist_get_dual_pipe_comp(
	struct mtk_ddp_comp *comp, struct mtk_ddp_comp **dual_comp)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	int index = index_of_chist(comp->id);

	if (comp_to_chist(comp)->data->module_count > 1) {
		if (index)
			*dual_comp = priv->ddp_comp[DDP_COMPONENT_CHIST3];
		else
			*dual_comp = priv->ddp_comp[DDP_COMPONENT_CHIST2];
	} else
		*dual_comp = priv->ddp_comp[DDP_COMPONENT_CHIST1];

	if (*dual_comp != NULL)
		return 1;

	DDPINFO("%s get dual comp fail for\n", __func__);
	return 0;
}

static bool is_dual_pipe_comp(struct mtk_ddp_comp *comp)
{
	if (comp_to_chist(comp)->data->module_count > 1)
		return (comp->id == DDP_COMPONENT_CHIST2 ||
				comp->id == DDP_COMPONENT_CHIST3);

	return (comp->id == DDP_COMPONENT_CHIST1);
}

void mtk_chist_dump_impl(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i = 0;

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);

	mtk_cust_dump_reg(baddr, DISP_CHIST_EN, DISP_CHIST_CFG,
		DISP_CHIST_SIZE, DISP_CHIST_INTEN);
	mtk_cust_dump_reg(baddr, DISP_CHIST_HIST_CH_CFG1, DISP_CHIST_HIST_CH_CFG2,
		DISP_CHIST_HIST_CH_CFG3, DISP_CHIST_HIST_CH_CFG4);
	mtk_cust_dump_reg(baddr, DISP_CHIST_WEIGHT, DISP_CHIST_BLD_CONFIG,
		DISP_CHIST_HIST_CH_CFG5, DISP_CHIST_HIST_CH_CFG6);
	for (; i < 7; i++) {
		mtk_cust_dump_reg(baddr, DISP_CHIST_CH0_WIN_X_MAIN + i * 0x10,
			DISP_CHIST_CH0_WIN_Y_MAIN + i * 0x10,
			DISP_CHIST_CH0_BLOCK_INFO + i * 0x10,
			DISP_CHIST_CH0_BLOCK_CROP + i * 0x10);
		mtk_cust_dump_reg(baddr, DISP_CHIST_HIST_CH_CNF0, DISP_CHIST_HIST_CH_CNF1,
			DISP_CHIST_HIST_CH_CH0_CNF0 + i * 8,
			DISP_CHIST_HIST_CH_CH0_CNF1 + i * 8);
	}
	for (i = 0; i < 3; i++)
		mtk_cust_dump_reg(baddr, DISP_CHIST_Y2R_PAPA_R0 + i * 4, 0x05c + i * 4,
			0x068 + i * 4, DISP_CHIST_Y2R_PAPA_POST_A0 + i * 4);
}

void mtk_chist_dump(struct mtk_ddp_comp *comp)
{
	struct mtk_ddp_comp *dual_comp;

	mtk_chist_dump_impl(comp);
	if (mtk_chist_get_dual_pipe_comp(comp, &dual_comp))
		mtk_chist_dump_impl(dual_comp);
}

int mtk_chist_analysis(struct mtk_ddp_comp *comp)
{
	DDPDUMP("== %s ANALYSIS ==\n", mtk_dump_comp_str(comp));

	return 0;
}

int mtk_drm_ioctl_get_chist(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct drm_mtk_chist_info *hist = data;
	int i = 0;

	if (hist == NULL) {
		DDPPR_ERR("%s drm_mtk_hist_info is NULL\n", __func__);
		return -EFAULT;
	}

	DDPINFO("%s chist id:%d, get count:%d\n", __func__,
		hist->device_id, hist->get_channel_count);

	if (hist->get_channel_count == 0 ||
			hist->get_channel_count > DISP_CHIST_CHANNEL_COUNT) {
		DDPPR_ERR("%s invalid get channel count is %u\n",
				__func__, hist->get_channel_count);
		return -EFAULT;
	}

	if (hist->caller == MTK_DRM_CHIST_CALLER_HWC) {
		unsigned int channel_id = DISP_CHIST_HWC_CHANNEL_INDEX;

		for (; i < hist->get_channel_count &&
			channel_id < DISP_CHIST_CHANNEL_COUNT; i++) {
			hist->channel_hist[i].channel_id = channel_id;
			channel_id++;
		}
	}

	if (disp_chist_copy_hist_to_user(dev, hist) < 0)
		return -EFAULT;
	DDPINFO("%s --\n", __func__);
	return 0;
}

static void mtk_chist_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CFG, 0x106, ~0);
	g_chist_relay_value[index_of_chist(comp->id)] = 0;
}

static void mtk_chist_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CFG, 0x1, ~0);
	g_chist_relay_value[index_of_chist(comp->id)] = 1;
}

static void mtk_chist_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	if (bypass == 1)
		mtk_chist_stop(comp, handle);
	else
		mtk_chist_start(comp, handle);
}

static void mtk_chist_channel_enabled(unsigned int channel,
	bool enabled, struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{

#ifndef DEBUG_UT_TEST

	if (channel > DISP_CHIST_CHANNEL_COUNT)
		return;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG1,
		(enabled  ? 1 : 0) << channel, 1 << channel);

#else
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_MON, 0x7f7f, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_WEIGHT, 0x101020, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_BLD_CONFIG, 0x08, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG1, 0x3f107f55, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG2, 0xba93218, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG4, 0x77765442, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG5, 0x0b0b0b0b, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG6, 0x0d, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CNF0, 0x7f, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CNF1, 0x8765432, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CH0_CNF0 + channel * 8, 0xffc08040, ~0);
	if (channel == 6)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_CHIST_HIST_CH_CH0_CNF1 + channel * 8, 0x13, ~0);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_CHIST_HIST_CH_CH0_CNF1 + channel * 8, 0x0, ~0);
#endif

}

static unsigned int mtk_chist_bin_count_regs(unsigned int bin_count)
{
	switch (bin_count) {
	case 256:
		return 0;
	case 128:
		return 1;
	case 64:
		return 2;
	case 32:
		return 3;
	case 16:
		return 4;
	case 8:
		return 5;
	default:
		return 5;
	}
}

static void mtk_chist_channel_config(unsigned int channel,
	struct drm_mtk_channel_config *config,
	struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s channel:%d, config->blk_height:%d, config->blk_width:%d\n", __func__,
			channel, config->blk_height, config->blk_width);

	// roi
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CH0_WIN_X_MAIN + channel * 0x10,
		(config->roi_end_x << 16) | config->roi_start_x, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CH0_WIN_Y_MAIN + channel * 0x10,
		(config->roi_end_y << 16) | config->roi_start_y, ~0);

	// block
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CH0_BLOCK_INFO + channel * 0x10,
		(config->blk_height << 16) | config->blk_width, ~0);

	if (channel >= DISP_CHIST_CHANNEL_COUNT)
		return;

	if (is_dual_pipe_comp(comp))
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_CHIST_CH0_BLOCK_CROP + channel * 0x10,
			g_chist_block_config[index_of_chist(comp->id)][channel].blk_xofs, ~0);
	else
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_CHIST_CH0_BLOCK_CROP + channel * 0x10, 0x0, ~0);

	// bin count, 0:256,1:128,2:64,3:32,4:16,5:8
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG3,
		mtk_chist_bin_count_regs(config->bin_count) << channel * 4,
		0x07 << channel * 4);
	// channel sel color format, channel bin count
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG2,
		config->color_format << channel * 4, 0x0F << channel * 4);

	// (HS)V = max(R G B)
	if (config->color_format == MTK_DRM_COLOR_FORMAT_M)
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_CHIST_HIST_CH_CFG4, DISP_CHIST_MAX_RGB, ~0);

	mtk_chist_channel_enabled(channel, 1, comp, handle);
}

static void ceil(int num, int divisor, int *result)
{
	if (divisor > 0) {
		if (num % divisor == 0)
			*result = num / divisor;
		else
			*result = num / divisor + 1;
	} else
		*result = 0;
}

static void mtk_chist_block_config(struct drm_mtk_channel_config *channel_config,
	struct drm_mtk_channel_config *channel_config1, unsigned int channel_id, unsigned int index)
{
	int roi_left_width = g_pipe_width - channel_config->roi_start_x;
	unsigned long flags;

	if (channel_config->roi_end_x > g_pipe_width
		&& channel_config->roi_start_x >= g_pipe_width) {
		// roi is in right pipe only
		channel_config1->roi_start_x = channel_config->roi_start_x - g_pipe_width;
		channel_config1->roi_end_x = channel_config->roi_end_x - g_pipe_width;
		channel_config->roi_start_x = 0;
		channel_config->roi_end_x = 0;
	} else if (channel_config->roi_end_x < g_pipe_width) {
		// roi is in left pipe only
		channel_config1->roi_start_x = 0;
		channel_config1->roi_end_x = 0;
	} else {
		channel_config1->roi_start_x = 0;
		channel_config1->roi_end_x = channel_config->roi_end_x - g_pipe_width;
		channel_config->roi_end_x = g_pipe_width - 1;
	}

	if (channel_config->blk_width < g_pipe_width) {
		int right_blk_xfos = roi_left_width % channel_config->blk_width;
		int left_blk_column = roi_left_width / channel_config->blk_width;

		if (index >= CHIST_NUM || channel_id >= DISP_CHIST_CHANNEL_COUNT)
			return;

		spin_lock_irqsave(&g_chist_global_lock, flags);
		if (right_blk_xfos) {
			g_chist_block_config[index][channel_id].merge_column = left_blk_column;
			left_blk_column++;
			g_chist_block_config[index][channel_id].blk_xofs = right_blk_xfos;
		} else
			g_chist_block_config[index][channel_id].merge_column = -1;

		g_chist_block_config[index][channel_id].left_column = left_blk_column;
		spin_unlock_irqrestore(&g_chist_global_lock, flags);
	}
}

static int mtk_chist_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	struct drm_mtk_chist_config *config = data;
	unsigned long flags;
	int i = 0;
	unsigned int index = index_of_chist(comp->id);
	int bypass = 1;

	if (config->config_channel_count == 0)
		return -EINVAL;

	for (; i < config->config_channel_count; i++) {
		struct drm_mtk_channel_config channel_config;
		unsigned int channel_id = 0;

		memcpy(&channel_config, &config->chist_config[i],
				sizeof(config->chist_config[i]));
		channel_id = channel_config.channel_id;

		if (index >= CHIST_NUM || channel_id >= DISP_CHIST_CHANNEL_COUNT)
			continue;

		spin_lock_irqsave(&g_chist_global_lock, flags);
		memset(&(g_chist_config[index][channel_id]), 0,
			sizeof(struct drm_mtk_channel_config));
		memset(&(g_chist_block_config[index][channel_id]), 0,
			sizeof(struct mtk_disp_block_config));
		spin_unlock_irqrestore(&g_chist_global_lock, flags);

		if (channel_config.enabled) {
			int blk_column = 0;
			// end of roi, width & height of block can't be 0
			channel_config.roi_end_x = channel_config.roi_end_x
			? channel_config.roi_end_x : g_frame_width - 1;
			channel_config.roi_end_y = channel_config.roi_end_y
				? channel_config.roi_end_y : g_frame_height - 1;
			channel_config.blk_width = channel_config.blk_width
				? channel_config.blk_width
				: channel_config.roi_end_x - channel_config.roi_start_x + 1;
			channel_config.blk_height = channel_config.blk_height
				? channel_config.blk_height
				: channel_config.roi_end_y - channel_config.roi_start_y + 1;

			ceil((channel_config.roi_end_x - channel_config.roi_start_x + 1),
				channel_config.blk_width, &blk_column);

			spin_lock_irqsave(&g_chist_global_lock, flags);
			g_chist_block_config[index][channel_id].sum_column = blk_column;

			memcpy(&(g_chist_config[index][channel_id]), &channel_config,
				sizeof(channel_config));
			spin_unlock_irqrestore(&g_chist_global_lock, flags);

			if (comp->mtk_crtc->is_dual_pipe) {
				struct mtk_ddp_comp *dual_comp = NULL;
				struct drm_mtk_channel_config channel_config1;

				memcpy(&channel_config1, &channel_config, sizeof(channel_config));

				if (!mtk_chist_get_dual_pipe_comp(comp, &dual_comp))
					return 1;

				if (channel_config.roi_start_x >= g_pipe_width) {
					// roi is in the right half, just config right
					channel_config1.roi_start_x = channel_config1.roi_start_x
						- g_pipe_width;
					channel_config1.roi_end_x = channel_config1.roi_end_x
						- g_pipe_width;
					mtk_chist_channel_config(channel_id,
						&channel_config1, dual_comp, handle);
				} else if (channel_config.roi_end_x > 0 &&
					channel_config.roi_end_x < g_pipe_width) {
					// roi is in the left half, just config left module
					mtk_chist_channel_config(channel_id,
							&channel_config, comp, handle);
				} else {
					mtk_chist_block_config(&channel_config,
						&channel_config1, channel_id, index);

					mtk_chist_channel_config(channel_id, &channel_config,
						comp, handle);
					mtk_chist_channel_config(channel_id, &channel_config1,
						dual_comp, handle);
				}
			} else {
				mtk_chist_channel_config(channel_id, &channel_config,
					comp, handle);
			}
		} else {
			mtk_chist_channel_enabled(channel_id, 0, comp, handle);
			if (comp->mtk_crtc->is_dual_pipe) {
				struct mtk_ddp_comp *dual_comp = NULL;

				if (mtk_chist_get_dual_pipe_comp(comp, &dual_comp))
					mtk_chist_channel_enabled(channel_id, 0, dual_comp, handle);
			}
		}
	}
	spin_lock_irqsave(&g_chist_global_lock, flags);
	for (i = 0; i < DISP_CHIST_CHANNEL_COUNT; i++) {
		if (g_chist_config[index][i].enabled) {
			bypass = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&g_chist_global_lock, flags);

	mtk_chist_bypass(comp, bypass, handle);
	if (comp->mtk_crtc->is_dual_pipe) {
		struct mtk_ddp_comp *dual_comp = NULL;

		if (mtk_chist_get_dual_pipe_comp(comp, &dual_comp))
			mtk_chist_bypass(dual_comp, bypass, handle);
	}
	return 0;
}


static void mtk_chist_prepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;
	DDPINFO("%s\n", __func__);

	mtk_ddp_comp_clk_prepare(comp);
	spin_lock_irqsave(&g_chist_clock_lock, flags);
	atomic_set(&g_chist_is_clock_on[index_of_chist(comp->id)], 1);
	spin_unlock_irqrestore(&g_chist_clock_lock, flags);
}

static void mtk_chist_unprepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;
	DDPINFO("%s\n", __func__);

	spin_lock_irqsave(&g_chist_clock_lock, flags);
	atomic_set(&g_chist_is_clock_on[index_of_chist(comp->id)], 0);
	spin_unlock_irqrestore(&g_chist_clock_lock, flags);
	mtk_ddp_comp_clk_unprepare(comp);
}

static void disp_chist_restore_setting(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct drm_mtk_chist_config config;
	unsigned long flags;
	int i = 0;
	int index = index_of_chist(comp->id);

	config.config_channel_count = DISP_CHIST_CHANNEL_COUNT;
	for (; i < DISP_CHIST_CHANNEL_COUNT; i++) {
		spin_lock_irqsave(&g_chist_global_lock, flags);
		memcpy(&(config.chist_config[i]), &(g_chist_config[index][i]),
			sizeof(g_chist_config[index][i]));
		spin_unlock_irqrestore(&g_chist_global_lock, flags);
	}
	mtk_chist_user_cmd(comp, handle, CHIST_CONFIG, &config);
}

static void mtk_chist_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	unsigned int width;
	int i = 0;

	g_frame_width = cfg->w;
	g_frame_height = cfg->h;

	if (comp->mtk_crtc->is_dual_pipe) {
		width = cfg->w / 2;
		g_pipe_width = width;
	} else
		width = cfg->w;

	DDPINFO("%s, chist:%s\n", __func__, mtk_dump_comp_str(comp));
	// rgb 2 yuv regs
	for (; i < DISP_CHIST_YUV_PARAM_COUNT; i++) {
		if (i >= DISP_CHIST_POST_PARAM_INDEX)
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_CHIST_Y2R_PAPA_POST_A0 +
				(i % DISP_CHIST_POST_PARAM_INDEX) * 4,
				g_rgb_2_yuv[sel_index][i], ~0);
		else
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa + DISP_CHIST_Y2R_PAPA_R0 + i * 4,
				g_rgb_2_yuv[sel_index][i], ~0);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_CHIST_EN,
				   0x1, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + DISP_CHIST_SIZE,
			   (width << 16) | cfg->h, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_CHIST_SHADOW_CTRL,
				   0x1, ~0);

	if (comp->id != DDP_COMPONENT_CHIST0 &&
		comp->id != DDP_COMPONENT_CHIST1)
		return;
	// default by pass chist
	mtk_chist_bypass(comp, 1, handle);
	if (comp->id == DDP_COMPONENT_CHIST0
		|| (comp_to_chist(comp)->data->module_count > 1
		&& comp->id == DDP_COMPONENT_CHIST1)) {
		disp_chist_set_interrupt(comp, 1, handle);
		if (need_restore)
			disp_chist_restore_setting(comp, handle);
	}
}

void mtk_chist_first_cfg(struct mtk_ddp_comp *comp,
	       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	mtk_chist_config(comp, cfg, handle);
}

/* don't need start funcs, chist will start by ioctl_set_config*/
static const struct mtk_ddp_comp_funcs mtk_disp_chist_funcs = {
	.config = mtk_chist_config,
	.first_cfg = mtk_chist_first_cfg,
	.stop = mtk_chist_stop,
	.bypass = mtk_chist_bypass,
	.user_cmd = mtk_chist_user_cmd,
	.prepare = mtk_chist_prepare,
	.unprepare = mtk_chist_unprepare,
};

static int mtk_disp_chist_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_chist *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s\n", __func__);

	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_chist_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_chist *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	DDPINFO("%s\n", __func__);

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_chist_component_ops = {
	.bind = mtk_disp_chist_bind, .unbind = mtk_disp_chist_unbind,
};

static void mtk_get_hist_dual_pipe(struct mtk_ddp_comp *comp,
	unsigned int i, int sum_bins, unsigned int index)
{
	struct mtk_ddp_comp *dual_comp = NULL;
	unsigned int j = 0;

	if (!mtk_chist_get_dual_pipe_comp(comp, &dual_comp))
		return;
	// select channel id
	writel(0x30 | i, dual_comp->regs + DISP_CHIST_APB_READ);

	if (index >= CHIST_NUM || i >= DISP_CHIST_CHANNEL_COUNT)
		return;

	for (; j < sum_bins; j++) {
		if (g_chist_config[index][i].roi_start_x >= g_pipe_width)
			// read right
			g_disp_hist[index][i].hist[j] = readl(dual_comp->regs
				+ DISP_CHIST_SRAM_R_IF) >> DISP_CHIST_SHIFT_NUM;
		else if (g_chist_config[index][i].roi_end_x < g_pipe_width)
			// read left
			g_disp_hist[index][i].hist[j] = readl(comp->regs + DISP_CHIST_SRAM_R_IF)
				>> DISP_CHIST_SHIFT_NUM;
		else {
			if (g_chist_block_config[index][i].sum_column > 1) {
				int current_column = (j / g_chist_config[index][i].bin_count)
					% g_chist_block_config[index][i].sum_column;

				if (current_column < g_chist_block_config[index][i].left_column) {
					g_disp_hist[index][i].hist[j] = readl(comp->regs
						+ DISP_CHIST_SRAM_R_IF) >> DISP_CHIST_SHIFT_NUM;

					if (g_chist_block_config[index][i].merge_column >= 0 &&
						g_chist_block_config[index][i].merge_column
						== current_column) {
						g_disp_hist[index][i].hist[j] +=
							(readl(dual_comp->regs
							+ DISP_CHIST_SRAM_R_IF) - 0x10)
							>> DISP_CHIST_SHIFT_NUM;
					}
				} else {
					g_disp_hist[index][i].hist[j] = readl(dual_comp->regs
						+ DISP_CHIST_SRAM_R_IF) >> DISP_CHIST_SHIFT_NUM;
				}
			} else {
				g_disp_hist[index][i].hist[j] = readl(comp->regs
					+ DISP_CHIST_SRAM_R_IF) >> DISP_CHIST_SHIFT_NUM;
				g_disp_hist[index][i].hist[j] += (readl(dual_comp->regs
					+ DISP_CHIST_SRAM_R_IF) - 0x10) >> DISP_CHIST_SHIFT_NUM;
			}
		}
	}
}

static void mtk_get_chist(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_private *priv = NULL;
	unsigned long flags;
	int max_bins = 0;
	unsigned int i = 0, index = 0;
	unsigned int cur_present_fence;

	if (mtk_crtc == NULL)
		return;

	crtc = &mtk_crtc->base;
	priv = crtc->dev->dev_private;
	index = index_of_chist(comp->id);

	spin_lock_irqsave(&g_chist_clock_lock, flags);
	if (atomic_read(&(g_chist_is_clock_on[index_of_chist(comp->id)])) == 0) {
		spin_unlock_irqrestore(&g_chist_clock_lock, flags);
		return;
	}
	spin_lock_irqsave(&g_chist_global_lock, flags);
	for (; i < DISP_CHIST_CHANNEL_COUNT; i++) {
		if (g_chist_config[index][i].enabled) {
			g_disp_hist[index][i].bin_count = g_chist_config[index][i].bin_count;
			g_disp_hist[index][i].color_format = g_chist_config[index][i].color_format;
			g_disp_hist[index][i].channel_id = i;

			if (i >= DISP_CHIST_HWC_CHANNEL_INDEX)
				max_bins = DISP_CHIST_MAX_BIN_LOW;
			else
				max_bins = DISP_CHIST_MAX_BIN;

			// select channel id
			writel(0x30 | i, comp->regs + DISP_CHIST_APB_READ);

			if (mtk_crtc->is_dual_pipe)
				mtk_get_hist_dual_pipe(comp, i, max_bins, index);
			else {
				int j = 0;
				for (; j < max_bins; j++) {
					g_disp_hist[index][i].hist[j] = readl(comp->regs
						+ DISP_CHIST_SRAM_R_IF) >> DISP_CHIST_SHIFT_NUM;
				}
			}
		}
	}
	spin_unlock_irqrestore(&g_chist_global_lock, flags);
	spin_unlock_irqrestore(&g_chist_clock_lock, flags);

	cur_present_fence = *(unsigned int *)(mtk_get_gce_backup_slot_va(mtk_crtc,
				DISP_SLOT_PRESENT_FENCE(0)));
	if (cur_present_fence != 0) {
		if (present_fence[index] == cur_present_fence - 1)
			present_fence[index] = cur_present_fence;
		else
			present_fence[index] = cur_present_fence - 1;
	}
}

static int mtk_chist_read_kthread(void *data)
{
	struct mtk_ddp_comp *comp = (struct mtk_ddp_comp *)data;

	while (!kthread_should_stop()) {
		int ret = 0;

		if (atomic_read(&(g_chist_get_irq[index_of_chist(comp->id)])) == 0) {
			DDPDBG("%s: wait_event_interruptible ++ ", __func__);
			ret = wait_event_interruptible(g_chist_get_irq_wq,
				atomic_read(&(g_chist_get_irq[index_of_chist(comp->id)])) == 1);
			if (ret < 0)
				DDPPR_ERR("wait %s fail, ret=%d\n", __func__, ret);
			else
				DDPDBG("%s: wait_event_interruptible -- ", __func__);
		} else {
			DDPDBG("%s: get_irq = 0", __func__);
		}
		atomic_set(&(g_chist_get_irq[index_of_chist(comp->id)]), 0);

		mtk_get_chist((struct mtk_ddp_comp *)data);
	}
	return 0;
}

static irqreturn_t mtk_disp_chist_irq_handler(int irq, void *dev_id)
{
	irqreturn_t ret = IRQ_NONE;
	unsigned int intsta = 0;
	unsigned long flags;
	struct mtk_disp_chist *priv = dev_id;
	struct mtk_ddp_comp *comp = NULL;

	if (IS_ERR_OR_NULL(priv))
		return ret;

	comp = &priv->ddp_comp;
	if (IS_ERR_OR_NULL(comp))
		return ret;

	spin_lock_irqsave(&g_chist_clock_lock, flags);
	if (atomic_read(&g_chist_is_clock_on[index_of_chist(comp->id)]) != 1) {
		DDPINFO("%s, chist clk is off\n", __func__);
		spin_unlock_irqrestore(&g_chist_clock_lock, flags);
		return ret;
	}

	intsta = readl(comp->regs + DISP_CHIST_INSTA);
	if (intsta & 0x2) {
		// Clear irq
		writel(0, comp->regs + DISP_CHIST_INSTA);
		atomic_set(&(g_chist_get_irq[index_of_chist(comp->id)]), 1);
		wake_up_interruptible(&g_chist_get_irq_wq);
	}
	spin_unlock_irqrestore(&g_chist_clock_lock, flags);
	ret = IRQ_HANDLED;
	return ret;
}

static int mtk_disp_chist_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_chist *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_CHIST);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_chist_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);
	ret = devm_request_irq(dev, priv->ddp_comp.irq, mtk_disp_chist_irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev), priv);
	if (ret)
		dev_err(dev, "devm_request_irq fail: %d\n", ret);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_chist_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	if (priv->ddp_comp.id == DDP_COMPONENT_CHIST0)
		kthread_run(mtk_chist_read_kthread,
			&(priv->ddp_comp), "mtk_chist_read");
	else if (priv->data->module_count > 1
		&& priv->ddp_comp.id == DDP_COMPONENT_CHIST1)
		kthread_run(mtk_chist_read_kthread,
			&(priv->ddp_comp), "mtk_chist1_read");

	DDPINFO("%s-\n", __func__);
	return ret;
}

static int mtk_disp_chist_remove(struct platform_device *pdev)
{
	struct mtk_disp_chist *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_chist_component_ops);

	mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	return 0;
}

static const struct mtk_disp_chist_data mt6983_chist_driver_data = {
	.support_shadow = true,
	.module_count = 2,
	.color_format = DISP_CHIST_COLOR_FORMAT,
	.max_channel = 3,
	.max_bin = DISP_CHIST_MAX_BIN_LOW,
};

static const struct mtk_disp_chist_data mt6895_chist_driver_data = {
	.support_shadow = true,
	.module_count = 2,
	.color_format = DISP_CHIST_COLOR_FORMAT,
	.max_channel = 3,
	.max_bin = DISP_CHIST_MAX_BIN_LOW,
};

static const struct mtk_disp_chist_data mt6879_chist_driver_data = {
	.support_shadow = true,
	.module_count = 1,
	.color_format = DISP_CHIST_COLOR_FORMAT,
	.max_channel = 3,
	.max_bin = DISP_CHIST_MAX_BIN_LOW,
};

static const struct of_device_id mtk_disp_chist_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6983-disp-chist",
	  .data = &mt6983_chist_driver_data},
	{ .compatible = "mediatek,mt6895-disp-chist",
	  .data = &mt6895_chist_driver_data},
	{ .compatible = "mediatek,mt6879-disp-chist",
	  .data = &mt6879_chist_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_chist_driver_dt_match);

struct platform_driver mtk_disp_chist_driver = {
	.probe = mtk_disp_chist_probe,
	.remove = mtk_disp_chist_remove,
	.driver = {
			.name = "mediatek-disp-chist",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_chist_driver_dt_match,
		},
};
