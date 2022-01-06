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
#include "mtk_disp_gamma.h"
#include "mtk_dump.h"

#ifdef CONFIG_LEDS_MTK_MODULE
#define CONFIG_LEDS_BRIGHTNESS_CHANGED
#include <linux/leds-mtk.h>
#else
#define mtk_leds_brightness_set(x, y) do { } while (0)
#endif

#define DISP_GAMMA_EN 0x0000
#define DISP_GAMMA_SHADOW_SRAM 0x0014
#define DISP_GAMMA_CFG 0x0020
#define DISP_GAMMA_SIZE 0x0030
#define DISP_GAMMA_PURE_COLOR 0x0038
#define DISP_GAMMA_BANK 0x0100
#define DISP_GAMMA_LUT 0x0700
#define DISP_GAMMA_LUT_0 0x0700
#define DISP_GAMMA_LUT_1 0x0B00

#define LUT_10BIT_MASK 0x03ff

#define GAMMA_EN BIT(0)
#define GAMMA_LUT_EN BIT(1)
#define GAMMA_RELAYMODE BIT(0)
#define DISP_GAMMA_BLOCK_SIZE 256

static unsigned int g_gamma_relay_value[DISP_GAMMA_TOTAL];
#define index_of_gamma(module) ((module == DDP_COMPONENT_GAMMA0) ? 0 : 1)
// It's a work around for no comp assigned in functions.
static struct mtk_ddp_comp *default_comp;

static unsigned int g_gamma_data_mode;

struct gamma_color_protect {
	unsigned int gamma_color_protect_support;
	unsigned int gamma_color_protect_lsb;
};

static struct gamma_color_protect g_gamma_color_protect;

struct gamma_color_protect_mode {
	unsigned int red_support;
	unsigned int green_support;
	unsigned int blue_support;
	unsigned int black_support;
	unsigned int white_support;
};

static struct DISP_GAMMA_LUT_T *g_disp_gamma_lut[DISP_GAMMA_TOTAL] = { NULL };
static struct DISP_GAMMA_12BIT_LUT_T *g_disp_gamma_12bit_lut[DISP_GAMMA_TOTAL] = { NULL };
static struct DISP_GAMMA_LUT_T g_disp_gamma_lut_db;
static struct DISP_GAMMA_12BIT_LUT_T g_disp_gamma_12bit_lut_db;

static DEFINE_MUTEX(g_gamma_global_lock);

static atomic_t g_gamma_is_clock_on[DISP_GAMMA_TOTAL] = { ATOMIC_INIT(0),
	ATOMIC_INIT(0)};

static DEFINE_SPINLOCK(g_gamma_clock_lock);

/* TODO */
/* static ddp_module_notify g_gamma_ddp_notify; */

enum GAMMA_IOCTL_CMD {
	SET_GAMMALUT = 0,
	SET_12BIT_GAMMALUT,
	BYPASS_GAMMA,
};

enum GAMMA_MODE {
	HW_8BIT = 0,
	HW_12BIT_MODE_8BIT,
	HW_12BIT_MODE_12BIT,
};

struct mtk_disp_gamma {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
};
struct mtk_disp_gamma_sb_param {
	unsigned int gain[3];
	unsigned int bl;
};
struct mtk_disp_gamma_sb_param g_sb_param;

static void mtk_gamma_init(struct mtk_ddp_comp *comp,
	struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	unsigned int width;

	if (comp->mtk_crtc->is_dual_pipe)
		width = cfg->w / 2;
	else
		width = cfg->w;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_SIZE,
		(width << 16) | cfg->h, ~0);
	if (g_gamma_data_mode == HW_12BIT_MODE_8BIT ||
		g_gamma_data_mode == HW_12BIT_MODE_12BIT) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_BANK,
			(g_gamma_data_mode - 1) << 2, 0x4);
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_PURE_COLOR,
			g_gamma_color_protect.gamma_color_protect_support |
			g_gamma_color_protect.gamma_color_protect_lsb, ~0);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_EN, GAMMA_EN, ~0);

}

static void mtk_gamma_config(struct mtk_ddp_comp *comp,
			     struct mtk_ddp_config *cfg,
			     struct cmdq_pkt *handle)
{
	/* TODO: only call init function if frame dirty */
	mtk_gamma_init(comp, cfg, handle);
	//cmdq_pkt_write(handle, comp->cmdq_base,
	//	comp->regs_pa + DISP_GAMMA_SIZE,
	//	(cfg->w << 16) | cfg->h, ~0);
	//cmdq_pkt_write(handle, comp->cmdq_base,
	//	comp->regs_pa + DISP_GAMMA_CFG,
	//	GAMMA_RELAYMODE, BIT(0));
}

static int mtk_gamma_write_lut_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock)
{
	struct DISP_GAMMA_LUT_T *gamma_lut;
	int i;
	int ret = 0;
	int id = index_of_gamma(comp->id);

	if (lock)
		mutex_lock(&g_gamma_global_lock);
	gamma_lut = g_disp_gamma_lut[id];
	if (gamma_lut == NULL) {
		DDPINFO("%s: table [%d] not initialized\n", __func__, id);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	for (i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			(comp->regs_pa + DISP_GAMMA_LUT + i * 4),
			gamma_lut->lut[i], ~0);

		if ((i & 0x3f) == 0) {
			DDPINFO("[0x%08lx](%d) = 0x%x\n",
				(long)(comp->regs_pa + DISP_GAMMA_LUT + i * 4),
				i, gamma_lut->lut[i]);
		}
	}
	i--;
	DDPINFO("[0x%08lx](%d) = 0x%x\n",
		(long)(comp->regs_pa + DISP_GAMMA_LUT + i * 4),
		i, gamma_lut->lut[i]);

	if ((int)(gamma_lut->lut[0] & 0x3FF) -
		(int)(gamma_lut->lut[510] & 0x3FF) > 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x1 << 2, 0x4);
		DDPINFO("decreasing LUT\n");
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x0 << 2, 0x4);
		DDPINFO("Incremental LUT\n");
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG,
			0x2 | g_gamma_relay_value[id], 0x3);

gamma_write_lut_unlock:
	if (lock)
		mutex_unlock(&g_gamma_global_lock);

	return ret;
}

static int mtk_gamma_write_12bit_lut_reg(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int lock)
{
	struct DISP_GAMMA_12BIT_LUT_T *gamma_lut;
	int i, j, block_num;
	int ret = 0;
	int id = index_of_gamma(comp->id);
	unsigned int table_config_sel, table_out_sel;

	if (lock)
		mutex_lock(&g_gamma_global_lock);
	gamma_lut = g_disp_gamma_12bit_lut[id];
	if (gamma_lut == NULL) {
		DDPINFO("%s: table [%d] not initialized\n", __func__, id);
		ret = -EFAULT;
		goto gamma_write_lut_unlock;
	}

	if (g_gamma_data_mode == HW_12BIT_MODE_12BIT) {
		block_num = DISP_GAMMA_12BIT_LUT_SIZE / DISP_GAMMA_BLOCK_SIZE;
	} else if (g_gamma_data_mode == HW_12BIT_MODE_8BIT) {
		block_num = DISP_GAMMA_LUT_SIZE / DISP_GAMMA_BLOCK_SIZE;
	} else {
		DDPINFO("%s: g_gamma_data_mode is error\n", __func__);
		return -1;
	}

	if (readl(comp->regs + DISP_GAMMA_SHADOW_SRAM) & 0x2) {
		table_config_sel = 0;
		table_out_sel = 0;
	} else {
		table_config_sel = 1;
		table_out_sel = 1;
	}

	writel(table_config_sel << 1 |
		(readl(comp->regs + DISP_GAMMA_SHADOW_SRAM) & 0x1),
		comp->regs + DISP_GAMMA_SHADOW_SRAM);

	for (i = 0; i < block_num; i++) {
		writel(i | (g_gamma_data_mode - 1) << 2,
			comp->regs + DISP_GAMMA_BANK);
		for (j = 0; j < DISP_GAMMA_BLOCK_SIZE; j++) {
			writel(gamma_lut->lut_0[i * DISP_GAMMA_BLOCK_SIZE + j],
				comp->regs + DISP_GAMMA_LUT_0 + j * 4);
			writel(gamma_lut->lut_1[i * DISP_GAMMA_BLOCK_SIZE + j],
				comp->regs + DISP_GAMMA_LUT_1 + j * 4);
		}
	}

	if ((int)(gamma_lut->lut_0[0] & 0x3FF) -
		(int)(gamma_lut->lut_0[510] & 0x3FF) > 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x1 << 2, 0x4);
		DDPINFO("decreasing LUT\n");
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x0 << 2, 0x4);
		DDPINFO("Incremental LUT\n");
	}

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG,
			0x2 | g_gamma_relay_value[id], 0x3);

	cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_SHADOW_SRAM,
			table_config_sel << 1 | table_out_sel, ~0);
gamma_write_lut_unlock:
	if (lock)
		mutex_unlock(&g_gamma_global_lock);

	return ret;
}

static int mtk_gamma_set_lut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_GAMMA_LUT_T *user_gamma_lut)
{
	/* TODO: use CPU to write register */
	int ret = 0;
	int id;
	struct DISP_GAMMA_LUT_T *gamma_lut, *old_lut;

	DDPINFO("%s\n", __func__);

	gamma_lut = kmalloc(sizeof(struct DISP_GAMMA_LUT_T),
		GFP_KERNEL);
	if (gamma_lut == NULL) {
		DDPPR_ERR("%s: no memory\n", __func__);
		return -EFAULT;
	}

	if (user_gamma_lut == NULL) {
		ret = -EFAULT;
		kfree(gamma_lut);
	} else {
		memcpy(gamma_lut, user_gamma_lut,
			sizeof(struct DISP_GAMMA_LUT_T));
		id = index_of_gamma(comp->id);

		if (id >= 0 && id < DISP_GAMMA_TOTAL) {
			mutex_lock(&g_gamma_global_lock);

			old_lut = g_disp_gamma_lut[id];
			g_disp_gamma_lut[id] = gamma_lut;

			DDPINFO("%s: Set module(%d) lut\n", __func__, comp->id);
			ret = mtk_gamma_write_lut_reg(comp, handle, 0);

			mutex_unlock(&g_gamma_global_lock);

			if (old_lut != NULL)
				kfree(old_lut);

			if (comp->mtk_crtc != NULL)
				mtk_crtc_check_trigger(comp->mtk_crtc, false,
					false);
		} else {
			DDPPR_ERR("%s: invalid ID = %d\n", __func__, comp->id);
			ret = -EFAULT;
		}
	}

	return ret;
}

static int mtk_gamma_12bit_set_lut(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct DISP_GAMMA_12BIT_LUT_T *user_gamma_lut)
{
	/* TODO: use CPU to write register */
	int ret = 0;
	int id;
	struct DISP_GAMMA_12BIT_LUT_T *gamma_lut, *old_lut;

	DDPINFO("%s\n", __func__);

	gamma_lut = kmalloc(sizeof(struct DISP_GAMMA_12BIT_LUT_T),
		GFP_KERNEL);
	if (gamma_lut == NULL) {
		DDPPR_ERR("%s: no memory\n", __func__);
		return -EFAULT;
	}

	if (user_gamma_lut == NULL) {
		ret = -EFAULT;
		kfree(gamma_lut);
	} else {
		memcpy(gamma_lut, user_gamma_lut,
			sizeof(struct DISP_GAMMA_12BIT_LUT_T));
		id = index_of_gamma(comp->id);

		if (id >= 0 && id < DISP_GAMMA_TOTAL) {
			mutex_lock(&g_gamma_global_lock);

			old_lut = g_disp_gamma_12bit_lut[id];
			g_disp_gamma_12bit_lut[id] = gamma_lut;

			DDPINFO("%s: Set module(%d) lut\n", __func__, comp->id);
			ret = mtk_gamma_write_12bit_lut_reg(comp, handle, 0);

			mutex_unlock(&g_gamma_global_lock);

			if (old_lut != NULL)
				kfree(old_lut);

			if (comp->mtk_crtc != NULL)
				mtk_crtc_check_trigger(comp->mtk_crtc, false,
					false);
		} else {
			DDPPR_ERR("%s: invalid ID = %d\n", __func__, comp->id);
			ret = -EFAULT;
		}
	}

	return ret;
}

int mtk_drm_ioctl_set_gammalut(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_GAMMA0];
	struct drm_crtc *crtc = private->crtc[0];

	g_disp_gamma_lut_db = *((struct DISP_GAMMA_LUT_T *)data);

	return mtk_crtc_user_cmd(crtc, comp, SET_GAMMALUT, data);
}

int mtk_drm_ioctl_set_12bit_gammalut(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_GAMMA0];
	struct drm_crtc *crtc = private->crtc[0];

	g_disp_gamma_12bit_lut_db = *((struct DISP_GAMMA_12BIT_LUT_T *)data);

	return mtk_crtc_user_cmd(crtc, comp, SET_12BIT_GAMMALUT, data);
}

int mtk_drm_ioctl_bypass_disp_gamma(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_GAMMA0];
	struct drm_crtc *crtc = private->crtc[0];

	return mtk_crtc_user_cmd(crtc, comp, BYPASS_GAMMA, data);
}

static void mtk_gamma_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_EN, GAMMA_EN, ~0);

	if (g_gamma_data_mode == HW_12BIT_MODE_8BIT ||
		g_gamma_data_mode == HW_12BIT_MODE_12BIT)
		mtk_gamma_write_12bit_lut_reg(comp, handle, 0);
	else
		mtk_gamma_write_lut_reg(comp, handle, 0);
}

static void mtk_gamma_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_EN, 0x0, ~0);
}

static void mtk_gamma_bypass(struct mtk_ddp_comp *comp, int bypass,
	struct cmdq_pkt *handle)
{
	DDPINFO("%s\n", __func__);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_GAMMA_CFG, bypass, 0x1);
	g_gamma_relay_value[index_of_gamma(comp->id)] = bypass;

}

static void mtk_gamma_set(struct mtk_ddp_comp *comp,
			  struct drm_crtc_state *state, struct cmdq_pkt *handle)
{
	unsigned int i;
	struct drm_color_lut *lut;
	u32 word = 0;
	u32 word_first = 0;
	u32 word_last = 0;

	DDPINFO("%s\n", __func__);

	if (state->gamma_lut) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DISP_GAMMA_CFG,
			       1<<GAMMA_LUT_EN, 1<<GAMMA_LUT_EN);
		lut = (struct drm_color_lut *)state->gamma_lut->data;
		for (i = 0; i < MTK_LUT_SIZE; i++) {
			word = GAMMA_ENTRY(lut[i].red >> 6,
				lut[i].green >> 6, lut[i].blue >> 6);
			cmdq_pkt_write(handle, comp->cmdq_base,
				comp->regs_pa
				+ (DISP_GAMMA_LUT + i * 4),
				word, ~0);

			// first & last word for
			//	decreasing/incremental LUT
			if (i == 0)
				word_first = word;
			else if (i == MTK_LUT_SIZE - 1)
				word_last = word;
		}
	}
	if ((word_first - word_last) > 0) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x1 << 2, 0x4);
		DDPINFO("decreasing LUT\n");
	} else {
		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_GAMMA_CFG, 0x0 << 2, 0x4);
		DDPINFO("Incremental LUT\n");
	}
}

static void calculateGammaLut(struct DISP_GAMMA_LUT_T *data)
{
	int i;

	for (i = 0; i < DISP_GAMMA_LUT_SIZE; i++)
		data->lut[i] = (((g_disp_gamma_lut_db.lut[i] & 0x3ff) *
			g_sb_param.gain[gain_b] + 4096) / 8192) |
			(((g_disp_gamma_lut_db.lut[i] >> 10 & 0x3ff) *
			g_sb_param.gain[gain_g] + 4096) / 8192) << 10 |
			(((g_disp_gamma_lut_db.lut[i] >> 20 & 0x3ff) *
			g_sb_param.gain[gain_r] + 4096) / 8192) << 20;

}

static void calculateGamma12bitLut(struct DISP_GAMMA_12BIT_LUT_T *data)
{
	int i, lut_size;

	if (g_gamma_data_mode == HW_12BIT_MODE_8BIT)
		lut_size = DISP_GAMMA_LUT_SIZE;

	if (g_gamma_data_mode == HW_12BIT_MODE_12BIT)
		lut_size = DISP_GAMMA_12BIT_LUT_SIZE;

	for (i = 0; i < lut_size; i++) {
		data->lut_0[i] =
			(((g_disp_gamma_12bit_lut_db.lut_0[i] & 0xfff) *
			g_sb_param.gain[gain_r] + 4096) / 8192) |
			(((g_disp_gamma_12bit_lut_db.lut_0[i] >> 12 & 0xfff) *
			g_sb_param.gain[gain_g] + 4096) / 8192) << 12;
		data->lut_1[i] =
			(((g_disp_gamma_12bit_lut_db.lut_1[i] & 0xfff) *
			g_sb_param.gain[gain_b] + 4096) / 8192);
	}
}

void mtk_trans_gain_to_gamma(struct drm_crtc *crtc,
	unsigned int gain[3], unsigned int bl)
{
	if (g_sb_param.gain[gain_r] != gain[gain_r] &&
		g_sb_param.gain[gain_g] != gain[gain_g] &&
		g_sb_param.gain[gain_b] != gain[gain_b]) {

		g_sb_param.gain[gain_r] = gain[gain_r];
		g_sb_param.gain[gain_g] = gain[gain_g];
		g_sb_param.gain[gain_b] = gain[gain_b];

		if (g_gamma_data_mode == HW_8BIT) {
			struct DISP_GAMMA_LUT_T data;

			calculateGammaLut(&data);
			mtk_crtc_user_cmd(crtc, default_comp,
				SET_GAMMALUT, (void *)&data);
		}

		if (g_gamma_data_mode == HW_12BIT_MODE_8BIT ||
			g_gamma_data_mode == HW_12BIT_MODE_12BIT) {
			struct DISP_GAMMA_12BIT_LUT_T data;

			calculateGamma12bitLut(&data);
			mtk_crtc_user_cmd(crtc, default_comp,
				SET_12BIT_GAMMALUT, (void *)&data);
		}

		mtk_leds_brightness_set("lcd-backlight", bl);
		mtk_crtc_check_trigger(default_comp->mtk_crtc, false, true);
		DDPINFO("%s : gain = %d, backlight = %d",
			__func__, g_sb_param.gain[gain_r], bl);
	} else {
		if (g_sb_param.bl != bl) {
			g_sb_param.bl = bl;
			mtk_leds_brightness_set("lcd-backlight", bl);
			DDPINFO("%s : backlight = %d", __func__, bl);
		}
	}
}

static int mtk_gamma_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	DDPINFO("%s: cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case SET_GAMMALUT:
	{
		struct DISP_GAMMA_LUT_T *config = data;

		if (mtk_gamma_set_lut(comp, handle, config) < 0) {
			DDPPR_ERR("%s: failed\n", __func__);
			return -EFAULT;
		}
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_gamma1 = priv->ddp_comp[DDP_COMPONENT_GAMMA1];

			if (mtk_gamma_set_lut(comp_gamma1, handle, config) < 0) {
				DDPPR_ERR("%s: comp_gamma1 failed\n", __func__);
				return -EFAULT;
			}
		}
	}
	break;
	case SET_12BIT_GAMMALUT:
	{
		struct DISP_GAMMA_12BIT_LUT_T *config = data;

		if (mtk_gamma_12bit_set_lut(comp, handle, config) < 0) {
			DDPPR_ERR("%s: failed\n", __func__);
			return -EFAULT;
		}
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_gamma1 = priv->ddp_comp[DDP_COMPONENT_GAMMA1];

			if (mtk_gamma_12bit_set_lut(comp_gamma1, handle, config) < 0) {
				DDPPR_ERR("%s: comp_gamma1 failed\n", __func__);
				return -EFAULT;
			}
		}
	}
	break;
	case BYPASS_GAMMA:
	{
		int *value = data;

		mtk_gamma_bypass(comp, *value, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_gamma1 = priv->ddp_comp[DDP_COMPONENT_GAMMA1];

			mtk_gamma_bypass(comp_gamma1, *value, handle);
		}
	}
	break;
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

struct gamma_backup {
	unsigned int GAMMA_CFG;
};
static struct gamma_backup g_gamma_backup;

static void ddp_dither_backup(struct mtk_ddp_comp *comp)
{
	g_gamma_backup.GAMMA_CFG =
		readl(comp->regs + DISP_GAMMA_CFG);
}

static void ddp_dither_restore(struct mtk_ddp_comp *comp)
{
	writel(g_gamma_backup.GAMMA_CFG, comp->regs + DISP_GAMMA_CFG);
}

static void mtk_gamma_prepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&g_gamma_is_clock_on[index_of_gamma(comp->id)], 1);
	ddp_dither_restore(comp);
}

static void mtk_gamma_unprepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;

	DDPINFO("%s @ %d......... spin_trylock_irqsave ++ ",
		__func__, __LINE__);
	spin_lock_irqsave(&g_gamma_clock_lock, flags);
	DDPINFO("%s @ %d......... spin_trylock_irqsave -- ",
		__func__, __LINE__);
	atomic_set(&g_gamma_is_clock_on[index_of_gamma(comp->id)], 0);
	spin_unlock_irqrestore(&g_gamma_clock_lock, flags);
	DDPINFO("%s @ %d......... spin_unlock_irqrestore ",
		__func__, __LINE__);
	ddp_dither_backup(comp);
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_gamma_funcs = {
	.gamma_set = mtk_gamma_set,
	.config = mtk_gamma_config,
	.start = mtk_gamma_start,
	.stop = mtk_gamma_stop,
	.bypass = mtk_gamma_bypass,
	.user_cmd = mtk_gamma_user_cmd,
	.prepare = mtk_gamma_prepare,
	.unprepare = mtk_gamma_unprepare,
};

static int mtk_disp_gamma_bind(struct device *dev, struct device *master,
			       void *data)
{
	struct mtk_disp_gamma *priv = dev_get_drvdata(dev);
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

static void mtk_disp_gamma_unbind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_gamma *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_gamma_component_ops = {
	.bind = mtk_disp_gamma_bind, .unbind = mtk_disp_gamma_unbind,
};

void mtk_gamma_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s REGS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x24, 0x28);
}

static void mtk_disp_gamma_dts_parse(const struct device_node *np,
	enum mtk_ddp_comp_id comp_id)
{
	struct gamma_color_protect_mode color_protect_mode;

	if (of_property_read_u32(np, "gamma_data_mode",
		&g_gamma_data_mode)) {
		DDPPR_ERR("comp_id: %d, gamma_data_mode = %d\n",
			comp_id, g_gamma_data_mode);
		g_gamma_data_mode = HW_8BIT;
	}

	if (of_property_read_u32(np, "color_protect_lsb",
		&g_gamma_color_protect.gamma_color_protect_lsb)) {
		DDPPR_ERR("comp_id: %d, color_protect_lsb = %d\n",
			comp_id, g_gamma_color_protect.gamma_color_protect_lsb);
		g_gamma_color_protect.gamma_color_protect_lsb = 0;
	}

	if (of_property_read_u32(np, "color_protect_red",
		&color_protect_mode.red_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_red = %d\n",
			comp_id, color_protect_mode.red_support);
		color_protect_mode.red_support = 0;
	}

	if (of_property_read_u32(np, "color_protect_green",
		&color_protect_mode.green_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_green = %d\n",
			comp_id, color_protect_mode.green_support);
		color_protect_mode.green_support = 0;
	}

	if (of_property_read_u32(np, "color_protect_blue",
		&color_protect_mode.blue_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_blue = %d\n",
			comp_id, color_protect_mode.blue_support);
		color_protect_mode.blue_support = 0;
	}

	if (of_property_read_u32(np, "color_protect_black",
		&color_protect_mode.black_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_black = %d\n",
			comp_id, color_protect_mode.black_support);
		color_protect_mode.black_support = 0;
	}

	if (of_property_read_u32(np, "color_protect_white",
		&color_protect_mode.white_support)) {
		DDPPR_ERR("comp_id: %d, color_protect_white = %d\n",
			comp_id, color_protect_mode.white_support);
		color_protect_mode.white_support = 0;
	}

	g_gamma_color_protect.gamma_color_protect_support =
		color_protect_mode.red_support << 4 |
		color_protect_mode.green_support << 5 |
		color_protect_mode.blue_support << 6 |
		color_protect_mode.black_support << 7 |
		color_protect_mode.white_support << 8;
}

static int mtk_disp_gamma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_gamma *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_GAMMA);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	if (comp_id == DDP_COMPONENT_GAMMA0)
		mtk_disp_gamma_dts_parse(dev->of_node, comp_id);

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_gamma_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	if (!default_comp)
		default_comp = &priv->ddp_comp;

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_gamma_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_gamma_remove(struct platform_device *pdev)
{
	struct mtk_disp_gamma *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_gamma_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_disp_gamma_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6779-disp-gamma",},
	{ .compatible = "mediatek,mt6789-disp-gamma",},
	{ .compatible = "mediatek,mt6885-disp-gamma",},
	{ .compatible = "mediatek,mt6873-disp-gamma",},
	{ .compatible = "mediatek,mt6853-disp-gamma",},
	{ .compatible = "mediatek,mt6833-disp-gamma",},
	{ .compatible = "mediatek,mt6983-disp-gamma",},
	{ .compatible = "mediatek,mt6895-disp-gamma",},
	{ .compatible = "mediatek,mt6879-disp-gamma",},
	{ .compatible = "mediatek,mt6855-disp-gamma",},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_gamma_driver_dt_match);

struct platform_driver mtk_disp_gamma_driver = {
	.probe = mtk_disp_gamma_probe,
	.remove = mtk_disp_gamma_remove,
	.driver = {

			.name = "mediatek-disp-gamma",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_gamma_driver_dt_match,
		},
};

void disp_gamma_set_bypass(struct drm_crtc *crtc, int bypass)
{
	int ret;

	ret = mtk_crtc_user_cmd(crtc, default_comp, BYPASS_GAMMA, &bypass);

	DDPINFO("%s : ret = %d", __func__, ret);
}
