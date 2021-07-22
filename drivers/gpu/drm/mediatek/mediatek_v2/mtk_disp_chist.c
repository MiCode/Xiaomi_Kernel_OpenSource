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

#define DISP_CHIST_COLOR_FORMAT 0x1ff
/* channel 0~3 has 256 bins, 4~6 has 128 bins */
#define DISP_CHIST_MAX_BIN 256
#define DISP_CHIST_MAX_BIN_LOW 128

/* chist custom info, if pq used chist1, need set DISP_CHIST_PQ_CHANNEL_INDEX to 7 */
#define DISP_CHIST_HWC_CHANNEL_INDEX 4
#define DISP_CHIST_PQ_CHANNEL_INDEX 0
/* chist custom info end*/

#define DISP_CHIST_CHANNEL_COUNT 7
#define DISP_CHIST_MAX_RGB 0x0321
// TODO: overlap need correct
#define DISP_CHIST_DUAL_PIPE_OVERLAP 100

#define DISP_CHIST_INTEN             0x08
#define DISP_CHIST_INSTA             0x0C
#define DISP_CHIST_CFG               0x20
#define DISP_CHIST_SIZE              0x30
#define DISP_CHIST_CONFIG            0x40
#define DISP_CHIST_Y2R_PAPA_R0       0x50
#define DISP_CHIST_SHADOW_CTRL       0xF0
// channel_n_win_x_main = DISP_CHIST_CH0_WIN_X_MAIN + n * 0x10
#define DISP_CHIST_CH0_WIN_X_MAIN    0x0460
#define DISP_CHIST_CH0_WIN_Y_MAIN    0x0464
#define DISP_CHIST_CH0_BLOCK_INFO    0x0468
#define DISP_CHIST_CH0_BLOCK_CROP    0x046C
#define DISP_CHIST_HIST_CH_CFG1      0x0510
#define DISP_CHIST_HIST_CH_CFG2      0x0514
#define DISP_CHIST_HIST_CH_CFG3      0x0518
#define DISP_CHIST_HIST_CH_CFG4      0x051C
#define DISP_CHIST_APB_READ          0x0600
#define DISP_CHIST_SRAM_R_IF         0x0680

static unsigned int g_chist_relay_value[2];
#define index_of_chist(module) ((module == DDP_COMPONENT_CHIST0) ? 0 : 1)

#define get_module_id(index) (index ? DDP_COMPONENT_CHIST1 : DDP_COMPONENT_CHIST0)

static bool debug_dump_hist;

static DEFINE_SPINLOCK(g_chist_global_lock);
static DEFINE_SPINLOCK(g_chist_clock_lock);

static DECLARE_WAIT_QUEUE_HEAD(g_chist_get_irq_wq);

static atomic_t g_chist_get_irq = ATOMIC_INIT(0);


static atomic_t g_chist_is_clock_on[2] = { ATOMIC_INIT(0),
	ATOMIC_INIT(0)};

//static bool debug_irq_log;
#define CHISTIRQ_LOG(fmt, arg...) do { \
	if (debug_irq_log) \
		pr_notice("[IRQ]%s:" fmt, __func__, arg); \
	} while (0)


enum CHIST_IOCTL_CMD {
	CHIST_CONFIG = 0,
	CHIST_UNKNOWN,
};

struct mtk_disp_block_config {
	unsigned int blk_count;
	unsigned int blk_xofs;
	unsigned int left_column;
	unsigned int sum_column;
	int merge_column;
};

static struct mtk_disp_block_config g_chist_block_config[DISP_CHIST_CHANNEL_COUNT * 2];

static struct drm_mtk_channel_config g_chist_config[DISP_CHIST_CHANNEL_COUNT * 2];

static struct drm_mtk_channel_hist g_disp_hist[DISP_CHIST_CHANNEL_COUNT * 2];

static unsigned int g_pipe_width;

struct mtk_disp_chist_data {
	bool support_shadow;
	unsigned int module_count;
};

struct mtk_disp_chist {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_chist_data *data;
};


int mtk_drm_ioctl_get_chist_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	// struct mtk_drm_private *private = dev->dev_private;
	struct drm_mtk_chist_caps *caps_info = data;
	int index_start = 0, i;
	int index_end = DISP_CHIST_HWC_CHANNEL_INDEX;
	//DDPINFO("%s chist id:%d\n", __func__, caps_info->chist_id);

	caps_info->support_color = DISP_CHIST_COLOR_FORMAT;
	if (DISP_CHIST_PQ_CHANNEL_INDEX) {
		index_start = DISP_CHIST_PQ_CHANNEL_INDEX;
		index_end = DISP_CHIST_PQ_CHANNEL_INDEX + DISP_CHIST_CHANNEL_COUNT;
	}
	for (i = index_start; i < index_end; i++) {
		memcpy(&(caps_info->drm_mtk_channel_config[i % DISP_CHIST_CHANNEL_COUNT]),
			&g_chist_config[i], sizeof(g_chist_config[i]));
	}
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

	if (config->caller == MTK_DRM_CHIST_CALLER_HWC) {
		unsigned int channel_id = DISP_CHIST_HWC_CHANNEL_INDEX;

		for (; i < config->config_channel_count &&
			channel_id < DISP_CHIST_CHANNEL_COUNT; i++) {
			config->chist_config[i].channel_id = channel_id;
			channel_id++;
		}
	} else if (DISP_CHIST_PQ_CHANNEL_INDEX == DISP_CHIST_CHANNEL_COUNT) {
		comp = private->ddp_comp[DDP_COMPONENT_CHIST1];
	}

	return mtk_crtc_user_cmd(crtc, comp, CHIST_CONFIG, data);
}

static void disp_chist_set_interrupt(struct mtk_ddp_comp *comp,
					int enabled, struct cmdq_pkt *handle)
{
	if (enabled && (readl(comp->regs + DISP_CHIST_INTEN) & 0x2))
		return;
	else if (!enabled && !(readl(comp->regs + DISP_CHIST_INTEN) & 0x2))
		return;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_INTEN, enabled ? 1 : 0, ~0);
}

static int disp_chist_copy_hist_to_user(struct drm_device *dev,
	struct drm_mtk_chist_info *hist)
{
	unsigned long flags;
	int ret = 0, i = 0;
	struct mtk_drm_private *private = dev->dev_private;

	/* We assume only one thread will call this function */
	spin_lock_irqsave(&g_chist_global_lock, flags);

	for (; i < hist->get_channel_count; i++) {
		unsigned int channel_id = hist->channel_hist[i].channel_id;

		if (g_chist_config[channel_id].enabled) {
			memcpy(&(hist->channel_hist[i]),
				&g_disp_hist[channel_id], sizeof(g_disp_hist[channel_id]));
		}
	}

	spin_unlock_irqrestore(&g_chist_global_lock, flags);
	hist->present_fence = atomic_read(&private->crtc_present[0]);

	return ret;
}

void mtk_chist_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	mtk_cust_dump_reg(baddr, DISP_CHIST_HIST_CH_CFG1, DISP_CHIST_HIST_CH_CFG2,
					DISP_CHIST_HIST_CH_CFG3, DISP_CHIST_HIST_CH_CFG4);
}

int mtk_drm_ioctl_get_chist(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	//struct mtk_drm_private *private = dev->dev_private;
	//struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_CHIST0];
	struct drm_mtk_chist_info *hist = data;
	int i = 0;

	if (hist == NULL) {
		DDPPR_ERR("%s drm_mtk_hist_info is NULL\n", __func__);
		return -1;
	}
	if (hist->get_channel_count == 0) {
		DDPPR_ERR("%s get channel count is 0\n", __func__);
		return -1;
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
	if (debug_dump_hist)
		mtk_chist_dump(data);

	return 0;
}

static void mtk_chist_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CFG, 0x06, ~0);
	g_chist_relay_value[index_of_chist(comp->id)] = 0;
}

static void mtk_chist_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CFG, 0x0, ~0);
}

static void mtk_chist_bypass(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CFG, 0x1, 0x1);
	g_chist_relay_value[index_of_chist(comp->id)] = 0x1;

}

static void mtk_chist_channel_enabled(unsigned int channel,
	bool enabled, struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{

	unsigned int real_channel = channel % DISP_CHIST_CHANNEL_COUNT;
	unsigned long flags;
	int i = 0;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG1,
		(enabled  ? 1 : 0) << real_channel, 1 << real_channel);

	spin_lock_irqsave(&g_chist_global_lock, flags);
	g_chist_config[channel].enabled = enabled;

	if (channel > DISP_CHIST_CHANNEL_COUNT)
		i = DISP_CHIST_CHANNEL_COUNT;

	for (; (i - DISP_CHIST_CHANNEL_COUNT) < DISP_CHIST_CHANNEL_COUNT; i++) {
		if (g_chist_config[i].enabled)
			break;
	}
	if ((i % DISP_CHIST_CHANNEL_COUNT) == DISP_CHIST_CHANNEL_COUNT)
		mtk_chist_bypass(comp, handle);
	else
		mtk_chist_start(comp, handle);

	spin_unlock_irqrestore(&g_chist_global_lock, flags);

}

static void mtk_chist_channel_config(unsigned int channel,
	struct drm_mtk_channel_config *config,
	struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	unsigned int real_channel = channel % DISP_CHIST_CHANNEL_COUNT;
	// roi
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CH0_WIN_X_MAIN + real_channel * 0x10,
		(config->roi_end_x << 16) | config->roi_start_x, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CH0_WIN_Y_MAIN + real_channel * 0x10,
		(config->roi_end_y << 16) | config->roi_start_y, ~0);

	// block
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CH0_BLOCK_INFO + real_channel * 0x10,
		(config->blk_height << 16) | config->blk_width, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_CH0_BLOCK_CROP + real_channel * 0x10,
		g_chist_block_config[channel].blk_xofs, ~0);

	// channel sel color format, channel bin count
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG2,
		config->color_format << real_channel * 4, 0x0F << real_channel * 4);
	// bin count, 0:256,1:128,2:64,3:32,4:16,5:8
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_CHIST_HIST_CH_CFG3,
		(DISP_CHIST_MAX_BIN / config->bin_count - 1) << real_channel * 4,
		0x07 << real_channel * 4);

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
	}
	*result = 0;
}

static void mtk_chist_block_config(struct drm_mtk_channel_config channel_config,
	struct drm_mtk_channel_config channel_config1, int channel_id)
{
	int roi_width = channel_config.roi_end_x - channel_config.roi_start_x + 1;
	int roi_left_width = g_pipe_width - channel_config.roi_start_x;
	int roi_right_width = roi_width - roi_left_width;
	unsigned long flags;

	channel_config.roi_end_x = g_pipe_width - 1;
	channel_config1.roi_start_x = g_pipe_width + DISP_CHIST_DUAL_PIPE_OVERLAP;
	channel_config1.roi_end_x = channel_config1.roi_start_x
			+ roi_right_width - 1;

	if (channel_config.blk_width) {
		int right_blk_xfos = roi_left_width % channel_config.blk_width;
		int left_blk_column = roi_left_width / channel_config.blk_width;

		spin_lock_irqsave(&g_chist_global_lock, flags);
		if (right_blk_xfos) {
			g_chist_block_config[channel_id].merge_column = left_blk_column;
			left_blk_column++;
			g_chist_block_config[channel_id].blk_xofs = right_blk_xfos;
		} else
			g_chist_block_config[channel_id].merge_column = -1;

		g_chist_block_config[channel_id].left_column = left_blk_column;
		spin_unlock_irqrestore(&g_chist_global_lock, flags);
	}

}

static int mtk_chist_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{

	struct drm_mtk_chist_config *config = data;
	unsigned long flags;
	int i = 0;

	if (config->config_channel_count == 0)
		return -EINVAL;

	DDPINFO("%s: cmd: %d, config channel count:%d\n", __func__,
		config->config_channel_count);

	disp_chist_set_interrupt(comp, 1, handle);

	for (; i < config->config_channel_count; i++) {
		struct drm_mtk_channel_config channel_config = config->chist_config[i];
		unsigned int channel_id = channel_config.channel_id;

		if (channel_config.enabled) {
			spin_lock_irqsave(&g_chist_global_lock, flags);
			if (channel_config.blk_width > 0) {
				int blk_column = 0;
				int blk_row = 0;

				ceil((channel_config.roi_end_x - channel_config.roi_start_x + 1),
					channel_config.blk_width, &blk_column);
				ceil((channel_config.roi_end_y - channel_config.roi_start_y + 1),
					channel_config.blk_height, &blk_row);
				g_chist_block_config[channel_id].blk_count = blk_column * blk_row;
				g_chist_block_config[channel_id].sum_column = blk_column;
			}
			g_chist_config[channel_id] = channel_config;
			spin_unlock_irqrestore(&g_chist_global_lock, flags);

			if (comp->mtk_crtc->is_dual_pipe) {
				struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
				struct drm_crtc *crtc = &mtk_crtc->base;
				struct mtk_drm_private *priv = crtc->dev->dev_private;
				struct mtk_ddp_comp *chist1 = priv->ddp_comp[DDP_COMPONENT_CHIST1];
				struct drm_mtk_channel_config channel_config1 = channel_config;

				if (channel_config.roi_start_x >= g_pipe_width) {
					// roi is in the right half, just config right
					mtk_chist_channel_config(channel_id,
						&channel_config1, chist1, handle);
				} else if (channel_config.roi_end_x > 0 &&
					channel_config.roi_end_x < g_pipe_width) {
					// roi is in the left half, just config left module
					mtk_chist_channel_config(channel_id,
							&channel_config, comp, handle);
				} else {
					if (channel_config.roi_end_x)
						mtk_chist_block_config(channel_config,
							channel_config1, channel_id);

					mtk_chist_channel_config(channel_id, &channel_config,
						comp, handle);
					mtk_chist_channel_config(channel_id, &channel_config1,
						chist1, handle);
				}
			} else {
				mtk_chist_channel_config(channel_id, &channel_config,
					comp, handle);
			}
		} else {
			mtk_chist_channel_enabled(channel_id, 0, comp, handle);
			if (comp->mtk_crtc->is_dual_pipe) {
				struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
				struct drm_crtc *crtc = &mtk_crtc->base;
				struct mtk_drm_private *priv = crtc->dev->dev_private;
				struct mtk_ddp_comp *chist1 = priv->ddp_comp[DDP_COMPONENT_CHIST1];

				mtk_chist_channel_enabled(channel_id, 0, chist1, handle);
			}
		}
	}
	return 0;
}


static void mtk_chist_prepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;

	mtk_ddp_comp_clk_prepare(comp);
	spin_lock_irqsave(&g_chist_clock_lock, flags);
	atomic_set(&g_chist_is_clock_on[index_of_chist(comp->id)], 1);
	spin_unlock_irqrestore(&g_chist_clock_lock, flags);
}

static void mtk_chist_unprepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;

	spin_lock_irqsave(&g_chist_clock_lock, flags);
	atomic_set(&g_chist_is_clock_on[index_of_chist(comp->id)], 0);
	spin_unlock_irqrestore(&g_chist_clock_lock, flags);
	mtk_ddp_comp_clk_unprepare(comp);
}

static void mtk_chist_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	unsigned int width;

	if (comp->mtk_crtc->is_dual_pipe) {
		width = cfg->w / 2;
		g_pipe_width = width;
	} else
		width = cfg->w;

	DDPINFO("%s\n", __func__);
	// TODO: config rgb->yuv reg
	cmdq_pkt_write(handle, comp->cmdq_base,
			   comp->regs_pa + DISP_CHIST_SIZE,
			   (width << 16) | cfg->h, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
				   comp->regs_pa + DISP_CHIST_SHADOW_CTRL,
				   0, ~0);
}

static const struct mtk_ddp_comp_funcs mtk_disp_chist_funcs = {
	.config = mtk_chist_config,
	.start = mtk_chist_start,
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

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_chist_component_ops = {
	.bind = mtk_disp_chist_bind, .unbind = mtk_disp_chist_unbind,
};

static void mtk_get_hist_dual_pipe(struct mtk_ddp_comp *comp, int i, int sum_bins)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *chist1 = priv->ddp_comp[DDP_COMPONENT_CHIST1];
	int j = 0;

	writel(0x10 & (i % DISP_CHIST_CHANNEL_COUNT),
		chist1->regs + DISP_CHIST_APB_READ);
	for (; j < sum_bins; j++) {
		if (g_chist_config[i].roi_start_x >= g_pipe_width) {
			g_disp_hist[i].hist[j] = readl(chist1->regs + DISP_CHIST_SRAM_R_IF);
		} else if (g_chist_config[i].roi_end_x > 0
			&& g_chist_config[i].roi_end_x < g_pipe_width) {
			g_disp_hist[i].hist[j] = readl(comp->regs + DISP_CHIST_SRAM_R_IF);
		} else {
			if (g_chist_block_config[i].sum_column > 0) {
				int current_column = (j / g_chist_config[i].bin_count)
					% g_chist_block_config[i].sum_column;

				if (current_column < g_chist_block_config[i].left_column) {
					g_disp_hist[i].hist[j] = readl(comp->regs
						+ DISP_CHIST_SRAM_R_IF);
					if (g_chist_block_config[i].merge_column >= 0 &&
						g_chist_block_config[i].merge_column
						== current_column) {
						g_disp_hist[i].hist[j] += readl(chist1->regs
							+ DISP_CHIST_SRAM_R_IF);
					}
				} else {
					g_disp_hist[i].hist[j] = readl(chist1->regs
						+ DISP_CHIST_SRAM_R_IF);
				}
			} else {
				g_disp_hist[i].hist[j] = readl(comp->regs
					+ DISP_CHIST_SRAM_R_IF);
				g_disp_hist[i].hist[j] += readl(chist1->regs
					+ DISP_CHIST_SRAM_R_IF);
			}
		}
	}
}

static void mtk_get_chist(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	unsigned long flags;
	int i = 0;

	spin_lock_irqsave(&g_chist_global_lock, flags);
	for (; i < DISP_CHIST_CHANNEL_COUNT * 2; i++) {
		if (g_chist_config[i].enabled) {
			int sum_bins = g_chist_block_config[i].blk_count ?
				g_chist_block_config[i].blk_count * g_chist_config[i].bin_count
				: g_chist_config[i].bin_count;
			g_disp_hist[i].bin_count = g_chist_config[i].bin_count;
			g_disp_hist[i].color_format = g_chist_config[i].color_format;

			// select channel id
			writel(0x10 & (i % DISP_CHIST_CHANNEL_COUNT),
				comp->regs + DISP_CHIST_APB_READ);

			if (mtk_crtc->is_dual_pipe)
				mtk_get_hist_dual_pipe(comp, i, sum_bins);
			else {
				int j = 0;

				for (; j < sum_bins; j++)
					g_disp_hist[i].hist[j] = readl(comp->regs
						+ DISP_CHIST_SRAM_R_IF);
			}
		}
	}
	spin_unlock_irqrestore(&g_chist_global_lock, flags);
}

static int mtk_chist_read_kthread(void *data)
{
	while (!kthread_should_stop()) {
		int ret = 0;

		if (atomic_read(&g_chist_get_irq) == 0) {
			DDPDBG("%s: wait_event_interruptible ++ ", __func__);
			ret = wait_event_interruptible(g_chist_get_irq_wq,
				atomic_read(&g_chist_get_irq) == 1);
			DDPDBG("%s: wait_event_interruptible -- ", __func__);
		} else {
			DDPINFO("%s: get_irq = 0", __func__);
		}
		atomic_set(&g_chist_get_irq, 0);

		mtk_get_chist((struct mtk_ddp_comp *)data);
	}
	return 0;
}

static irqreturn_t mtk_disp_chist_irq_handler(int irq, void *dev_id)
{
	unsigned long flags;
	irqreturn_t ret = IRQ_NONE;
	unsigned int intsta;
	struct mtk_disp_chist *priv = dev_id;
	struct mtk_ddp_comp *comp = &priv->ddp_comp;

	if (spin_trylock_irqsave(&g_chist_clock_lock, flags)) {
		intsta = readl(comp->regs + DISP_CHIST_INSTA);
		DDPINFO("%s: intsta: 0x%x", __func__, intsta);

		if (intsta & 0x2) {
			// Clear irq
			writel(0, comp->regs + DISP_CHIST_INSTA);
			atomic_set(&g_chist_get_irq, 1);
			wake_up_interruptible(&g_chist_get_irq_wq);
		}
		ret = IRQ_HANDLED;
		spin_unlock_irqrestore(&g_chist_clock_lock, flags);
	}
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

	platform_set_drvdata(pdev, priv);
	ret = devm_request_irq(dev, priv->ddp_comp.irq, mtk_disp_chist_irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev), priv);
	if (ret)
		dev_err(dev, "devm_request_irq fail: %d\n", ret);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_disp_chist_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}

	kthread_run(mtk_chist_read_kthread,
		&(priv->ddp_comp), "mtk_chist_read");
	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_chist_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_chist_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct mtk_disp_chist_data mt6983_chist_driver_data = {
	.support_shadow = true,
	.module_count = 2,
};

static const struct of_device_id mtk_disp_chist_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6983-disp-chist",
	  .data = &mt6983_chist_driver_data},
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
