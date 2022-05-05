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
#include "mtk_disp_dither.h"
#include "platform/mtk_drm_6789.h"

#define DISP_DITHER_EN 0x0
#define DISP_DITHER_INTEN 0x08
#define DISP_DITHER_INTSTA 0x0c
#define DISP_REG_DITHER_CFG 0x20
#define DISP_REG_DITHER_SIZE 0x30
#define DISP_DITHER_5 0x0114
#define DISP_DITHER_7 0x011c
#define DISP_DITHER_15 0x013c
#define DISP_DITHER_16 0x0140
#define DISP_DITHER_PURECOLOR0 0x0160
#define DISP_DITHER_PURECOLOR1 0x0164


#define DITHER_REG(idx) (0x100 + (idx)*4)

#define DITHER_BYPASS_SHADOW	BIT(0)
#define DITHER_READ_WRK_REG		BIT(2)

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

#define DITHER_TOTAL_MODULE_NUM (2)
#define PURE_CLR_RGB (3)
#define PURE_CLR_NUM_MAX (7)
static unsigned int g_dither_relay_value[DITHER_TOTAL_MODULE_NUM];
#define index_of_dither(module) ((module == DDP_COMPONENT_DITHER0) ? 0 : 1)
static atomic_t g_dither_is_clock_on[2] = {
	ATOMIC_INIT(0), ATOMIC_INIT(0) };
static DEFINE_SPINLOCK(g_dither_clock_lock);
// It's a work around for no comp assigned in functions.
static struct mtk_ddp_comp *default_comp;
static struct workqueue_struct *dither_pure_detect_wq;
static struct work_struct dither_pure_detect_task;
static unsigned int g_dither_mode = 1;

enum COLOR_IOCTL_CMD {
	DITHER_SELECT = 0,
	SET_PARAM,
	BYPASS_DITHER,
	SET_INTERRUPT,
	SET_COLOR_DETECT,
};

enum PURE_CLR_RGB_ENUM {
	R_VALUE = 0,
	B_VALUE,
	G_VALUE,
};

struct mtk_disp_dither {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	int pwr_sta;
	unsigned int cfg_reg;
	const struct mtk_disp_dither_data *data;
};

struct mtk_disp_pure_clr_data {
	unsigned int pure_clr_det;
	unsigned int pure_clr_num;
	unsigned int pure_clr[PURE_CLR_NUM_MAX][PURE_CLR_RGB];
};
struct mtk_disp_pure_clr_data *g_pure_clr_param;

static int mtk_disp_dither_set_interrupt(struct mtk_ddp_comp *comp, int enabled)
{
	unsigned long flags;
	int ret = 0;
	int index = index_of_dither(comp->id);

	spin_lock_irqsave(&g_dither_clock_lock, flags);
	DDPDBG("%s @ %d......... spin_lock_irqsave -- ",
		__func__, __LINE__);
	if (atomic_read(&g_dither_is_clock_on[index]) != 1) {
		DDPINFO("%s: clock is off. enabled:%d\n",
			__func__, enabled);

		spin_unlock_irqrestore(&g_dither_clock_lock, flags);
		DDPDBG("%s @ %d......... spin_unlock_irqrestore -- ",
			__func__, __LINE__);
		return ret;
	}

	if (enabled) {
		if (readl(comp->regs + DISP_DITHER_EN) == 0) {
			/* Print error message */
			DDPINFO("[WARNING] DISP_DITHER_EN not enabled!\n");
		}
		/* Enable output frame end interrupt */
		writel(0x2, comp->regs + DISP_DITHER_INTEN);
		DDPINFO("%s: Interrupt enabled\n", __func__);
	} else {
		/* Disable output frame end interrupt */
		writel(0x0, comp->regs + DISP_DITHER_INTEN);
		DDPINFO("%s: Interrupt disabled\n", __func__);
	}
	spin_unlock_irqrestore(&g_dither_clock_lock, flags);
	DDPDBG("%s @ %d......... spin_unlock_irqrestore -- ",
		__func__, __LINE__);
	return ret;
}

static bool disp_dither_purecolor_devide(struct mtk_ddp_comp *comp)
{
	unsigned int clr_red, clr_green, clr_blue, i;
	bool ret = false;
	int index = index_of_dither(comp->id);

	if (atomic_read(&g_dither_is_clock_on[index]) != 1) {
		DDPINFO("%s: clock is off.\n", __func__);
		return ret;
	}
	clr_red = (readl(comp->regs +
		DISP_DITHER_PURECOLOR0) >> 8) & 0xfff;
	clr_blue = readl(comp->regs +
		DISP_DITHER_PURECOLOR1) & 0xfff;
	clr_green = (readl(comp->regs +
		DISP_DITHER_PURECOLOR1) >> 12) & 0xfff;
	DDPINFO("%s: clr_red: 0x%x, clr_blue: 0x%x, clr_green: 0x%x"
		, __func__, clr_red, clr_blue, clr_green);
	for (i = 0; i < g_pure_clr_param->pure_clr_num; i++) {
		if (g_pure_clr_param->pure_clr[i][R_VALUE] == clr_red &&
			g_pure_clr_param->pure_clr[i][B_VALUE] == clr_blue &&
			g_pure_clr_param->pure_clr[i][G_VALUE] == clr_green) {
			ret = true;
			break;
		}
	}
	return ret;
}

static void disp_dither_purecolor_detection(struct mtk_ddp_comp *comp)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct drm_crtc *crtc = &mtk_crtc->base;
	unsigned int clr_det, clr_flag;

	int index = index_of_dither(comp->id);

	if (atomic_read(&g_dither_is_clock_on[index]) != 1) {
		DDPINFO("%s: clock is off.\n", __func__);
		return;
	}
	clr_det = readl(comp->regs + DISP_DITHER_PURECOLOR0) & 0x1;
	DDPINFO("%s: clr_det: 0x%x", __func__, clr_det);

	if (clr_det) {	/* pure color detection support */
		clr_flag = (readl(comp->regs + DISP_DITHER_PURECOLOR0) >> 4) & 0x1;
		DDPINFO("%s: clr_flag: 0x%x", __func__, clr_flag);
		if (clr_flag) {
			if (disp_dither_purecolor_devide(comp))
				disp_dither_set_bypass(crtc, 1);
			else
				disp_dither_set_bypass(crtc, 0);
		} else {
			disp_dither_set_bypass(crtc, 0);
		}
	} else {
		disp_dither_set_bypass(crtc, 0);
		mtk_disp_dither_set_interrupt(comp, 0);
	}
}

static void dither_pure_detect_work(struct work_struct *work_item)
{
	if (!default_comp)
		return;
	disp_dither_purecolor_detection(default_comp);
}

static void disp_dither_on_end_of_frame(struct mtk_ddp_comp *comp)
{
	unsigned int intsta;
	unsigned long flags;
	int index = index_of_dither(comp->id);

	DDPDBG("%s @ %d......... [IRQ] spin_trylock_irqsave ++ ",
		  __func__, __LINE__);
	if (spin_trylock_irqsave(&g_dither_clock_lock, flags)) {
		DDPDBG("%s @ %d......... spin_trylock_irqsave -- ",
			__func__, __LINE__);
		if (atomic_read(&g_dither_is_clock_on[index]) != 1) {
			DDPINFO("%s: clock is off. enabled:%d\n", __func__, 0);

			spin_unlock_irqrestore(&g_dither_clock_lock, flags);
			DDPDBG("%s @ %d......... spin_unlock_irqrestore -- ",
				__func__, __LINE__);
			return;
		}
		intsta = readl(comp->regs + DISP_DITHER_INTSTA);
		DDPINFO("%s: intsta: 0x%x", __func__, intsta);

		if (intsta & 0x2) {	/* End of frame */
			// Clear irq
			writel(intsta & ~0x3, comp->regs
				+ DISP_DITHER_INTSTA);

			queue_work(dither_pure_detect_wq,
				&dither_pure_detect_task);
		}
		DDPDBG("%s @ %d......... [IRQ] spin_unlock_irqrestore ++ ",
			__func__, __LINE__);
		spin_unlock_irqrestore(&g_dither_clock_lock, flags);
		DDPDBG("%s @ %d......... [IRQ] spin_unlock_irqrestore -- ",
			__func__, __LINE__);
	} else {
		DDPINFO("%s @ %d......... Failed to spin_trylock_irqsave -- ",
			__func__, __LINE__);
	}
}

static irqreturn_t mtk_disp_dither_irq_handler(int irq, void *dev_id)
{
	struct mtk_disp_dither *priv = dev_id;
	struct mtk_ddp_comp *dither = &priv->ddp_comp;

	disp_dither_on_end_of_frame(dither);

	return IRQ_HANDLED;
}


static void mtk_dither_config(struct mtk_ddp_comp *comp,
			      struct mtk_ddp_config *cfg,
			      struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(comp->dev);

	unsigned int enable = 1;
	unsigned int width;

	if (comp->mtk_crtc->is_dual_pipe)
		width = cfg->w / 2;
	else
		width = cfg->w;

	DDPINFO("%s: bbp = %u\n", __func__, cfg->bpc);
	DDPINFO("%s: width = %u height = %u\n", __func__, cfg->w, cfg->h);

	/* skip redundant config */
	if (priv->pwr_sta != 0)
		return;

	priv->pwr_sta = 1;

	if (g_gamma_data_mode == 0) {
		if (cfg->bpc == 8) { /* 888 */
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(15),
				       0x20200001, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(16),
				       0x20202020, ~0);
		} else if (cfg->bpc == 5) { /* 565 */
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(15),
				       0x50500001, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(16),
				       0x50504040, ~0);
		} else if (cfg->bpc == 6) { /* 666 */
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(15),
				       0x40400001, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(16),
				       0x40404040, ~0);
		} else if (cfg->bpc > 8) {
			/* High depth LCM, no need dither */
			DDPINFO("%s: High depth LCM (bpp = %u), no dither\n",
				__func__, cfg->bpc);
		} else {
			/* Invalid dither bpp, bypass dither */
			/* FIXME: this case would cause dither hang */
			DDPINFO("%s: Invalid dither bpp = %u\n",
				__func__, cfg->bpc);
			enable = 0;
		}
	} else {
		if (cfg->bpc == 10) { /* 101010 */
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(15),
				       0x20200001, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(16),
				       0x20202020, ~0);
		} else if (cfg->bpc == 8) { /* 888 */
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(15),
				       0x40400001, ~0);
			cmdq_pkt_write(handle, comp->cmdq_base,
				       comp->regs_pa + DITHER_REG(16),
				       0x40404040, ~0);
		} else if (cfg->bpc > 10) {
			/* High depth LCM, no need dither */
			DDPINFO("%s: High depth LCM (bpp = %u), no dither\n",
				__func__, cfg->bpc);
		} else {
			/* Invalid dither bpp, bypass dither */
			/* FIXME: this case would cause dither hang */
			DDPINFO("%s: Invalid dither bpp = %u\n",
				__func__, cfg->bpc);
			enable = 0;
		}
	}

	if (enable == 1) {
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(5),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(6),
			       0x00003000 | (0x1 << g_dither_mode), ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(7),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(8),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(9),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(10),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(11),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(12),
			       0x00000011, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(13),
			       0x00000000, ~0);
		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + DITHER_REG(14),
			       0x00000000, ~0);
	}

	priv->cfg_reg = enable << 1 | (priv->cfg_reg & ~(0x1 << 1));

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_DITHER_EN, enable, ~0);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_DITHER_CFG,
		enable << 1 |
		g_dither_relay_value[index_of_dither(comp->id)], 0x3);

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_DITHER_SIZE,
		width << 16 | cfg->h, ~0);
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_DITHER_PURECOLOR0,
		g_pure_clr_param->pure_clr_det, 0x1);

}

static void mtk_dither_start(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(comp->dev);

	DDPINFO("%s\n", __func__);

	priv->pwr_sta = 1;
	if (g_pure_clr_param->pure_clr_det)
		mtk_disp_dither_set_interrupt(comp, 1);
}

static void mtk_dither_stop(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(comp->dev);

	DDPINFO("%s\n", __func__);

	priv->pwr_sta = 0;
	if (g_pure_clr_param->pure_clr_det)
		mtk_disp_dither_set_interrupt(comp, 0);
}

static void mtk_dither_bypass(struct mtk_ddp_comp *comp, int bypass,
			      struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(comp->dev);
	DDPINFO("%s\n", __func__);
	g_dither_relay_value[index_of_dither(comp->id)] = bypass;

	if (bypass)
		priv->cfg_reg = 0x1 | (priv->cfg_reg & ~0x1);
	else
		priv->cfg_reg = ~0x1 & priv->cfg_reg;

	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_DITHER_CFG,
		g_dither_relay_value[index_of_dither(comp->id)], 0x1);

}

struct dither_backup {
	unsigned int REG_DITHER_CFG;
};
static struct dither_backup g_dither_backup;

static void ddp_dither_backup(struct mtk_ddp_comp *comp)
{
	g_dither_backup.REG_DITHER_CFG =
		readl(comp->regs + DISP_REG_DITHER_CFG);
}

static void ddp_dither_restore(struct mtk_ddp_comp *comp)
{
	writel(g_dither_backup.REG_DITHER_CFG, comp->regs + DISP_REG_DITHER_CFG);
}

static void mtk_dither_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(comp->dev);
	int index = index_of_dither(comp->id);

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&g_dither_is_clock_on[index], 1);

	/* Bypass shadow register and read shadow register */
	if (priv->data->need_bypass_shadow)
		mtk_ddp_write_mask_cpu(comp, DITHER_BYPASS_SHADOW,
			DITHER_REG(0), DITHER_BYPASS_SHADOW);
	ddp_dither_restore(comp);
}

static void mtk_dither_unprepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;
	int index = index_of_dither(comp->id);

	DDPINFO("%s @ %d......... spin_lock_irqsave ++ ",
		__func__, __LINE__);
	spin_lock_irqsave(&g_dither_clock_lock, flags);
	DDPINFO("%s @ %d......... spin_lock_irqsave -- ",
		__func__, __LINE__);
	atomic_set(&g_dither_is_clock_on[index], 0);
	spin_unlock_irqrestore(&g_dither_clock_lock, flags);
	DDPINFO("%s @ %d......... spin_unlock_irqrestore ",
		__func__, __LINE__);
	ddp_dither_backup(comp);
	mtk_ddp_comp_clk_unprepare(comp);
}

/* TODO */
/* partial update
 * static int _dither_partial_update(struct mtk_ddp_comp *comp, void *arg,
 * struct cmdq_pkt *handle)
 * {
 * struct disp_rect *roi = (struct disp_rect *) arg;
 * int width = roi->width;
 * int height = roi->height;
 *
 * cmdq_pkt_write(handle, comp->cmdq_base,
 * comp->regs_pa + DISP_REG_DITHER_SIZE, width << 16 | height, ~0);
 * return 0;
 * }
 *
 * static int mtk_dither_io_cmd(struct mtk_ddp_comp *comp,
 * struct cmdq_pkt *handle,
 * enum mtk_ddp_io_cmd io_cmd,
 * void *params)
 * {
 * int ret = -1;
 * if (io_cmd == DDP_PARTIAL_UPDATE) {
 * _dither_partial_update(comp, params, handle);
 * ret = 0;
 * }
 * return ret;
 * }
 */

void mtk_dither_select(struct mtk_ddp_comp *comp,
				       struct cmdq_pkt *handle,
				       unsigned int bpc)
{
	unsigned int enable = 0x1;

	if (bpc == 8) {  /* 888 */
		writel(0x20200001, comp->regs + DITHER_REG(15));
		writel(0x20202020, comp->regs + DITHER_REG(16));
	} else if (bpc == 5) {  /* 565 */
		writel(0x50500001, comp->regs + DITHER_REG(15));
		writel(0x50504040, comp->regs + DITHER_REG(16));
	} else if (bpc == 6) {  /* 666 */
		writel(0x40400001, comp->regs + DITHER_REG(15));
		writel(0x40404040, comp->regs + DITHER_REG(16));
	} else if (bpc > 8) {
		/* High depth LCM, no need dither */
		DDPINFO("%s: High depth LCM (bpp = %u), no dither\n",
			__func__, bpc);
	} else {
		/* Invalid dither bpp, bypass dither */
		/* FIXME: this case would cause dither hang */
		DDPINFO("%s: Invalid dither bpp = %u\n", __func__, bpc);
		enable = 0;
	}

	if (enable == 1) {
		writel(0x00000000, comp->regs + DITHER_REG(5));
		writel(0x00003002, comp->regs + DITHER_REG(6));
		writel(0x00000000, comp->regs + DITHER_REG(7));
		writel(0x00000000, comp->regs + DITHER_REG(8));
		writel(0x00000000, comp->regs + DITHER_REG(9));
		writel(0x00000000, comp->regs + DITHER_REG(10));
		writel(0x00000000, comp->regs + DITHER_REG(11));
		writel(0x00000011, comp->regs + DITHER_REG(12));
		writel(0x00000000, comp->regs + DITHER_REG(13));
		writel(0x00000000, comp->regs + DITHER_REG(14));
	}

	writel(enable, comp->regs + DISP_DITHER_EN);
	writel(enable << 1 | (~enable), comp->regs + DISP_REG_DITHER_CFG);
}

void mtk_dither_set_param(struct mtk_ddp_comp *comp,
			struct cmdq_pkt *handle,
			bool relay, uint32_t mode)
{
	bool bypass = relay;
	uint32_t dither_mode = 0x00003000 | (0x1 << mode);

	pr_notice("%s: bypass: %d, dither_mode: %x", __func__, bypass, dither_mode);
	if (bypass) {
		g_dither_relay_value[index_of_dither(comp->id)] = 0x1;

		cmdq_pkt_write(handle, comp->cmdq_base,
			comp->regs_pa + DISP_REG_DITHER_CFG, 0x1, 0x1);
	} else {
		g_dither_relay_value[index_of_dither(comp->id)] = 0x0;

		cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DISP_REG_DITHER_CFG, 0x0, 0x1);
	}
	cmdq_pkt_write(handle, comp->cmdq_base,
		comp->regs_pa + DITHER_REG(6), dither_mode, ~0);
}

static int mtk_dither_user_cmd(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, unsigned int cmd, void *data)
{
	DDPINFO("%s: cmd: %d\n", __func__, cmd);
	switch (cmd) {

	case DITHER_SELECT:
	{
		unsigned int bpc = *((unsigned int *)data);

		mtk_dither_select(comp, NULL, bpc);
	}
	break;
	case SET_PARAM:
	{
		struct DISP_DITHER_PARAM *ditherParam = (struct DISP_DITHER_PARAM *)data;
		bool relay = ditherParam->relay;
		uint32_t mode = ditherParam->mode;

		g_dither_mode = (unsigned int)(ditherParam->mode);
		pr_notice("%s: relay: %d, mode: %d", __func__, relay, mode);

		mtk_dither_set_param(comp, handle, relay, mode);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_dither1 = priv->ddp_comp[DDP_COMPONENT_DITHER1];

			mtk_dither_set_param(comp_dither1, handle, relay, mode);
		}
	}
	case BYPASS_DITHER:
	{
		int *value = data;

		mtk_dither_bypass(comp, *value, handle);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_dither1 = priv->ddp_comp[DDP_COMPONENT_DITHER1];

			mtk_dither_bypass(comp_dither1, *value, handle);
		}
	}
	break;
	case SET_INTERRUPT:
	{
		int *value = data;

		mtk_disp_dither_set_interrupt(comp, *value);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_dither1 = priv->ddp_comp[DDP_COMPONENT_DITHER1];

			mtk_disp_dither_set_interrupt(comp_dither1, *value);
		}
	}
	break;
	case SET_COLOR_DETECT:
	{
		int *value = data;
		int index = index_of_dither(comp->id);

		g_pure_clr_param->pure_clr_det = *value;
		if (atomic_read(&g_dither_is_clock_on[index]) != 1) {
			DDPINFO("%s: clock is off.\n",
				__func__);
		} else {
			writel(readl(comp->regs + DISP_DITHER_PURECOLOR0) |
				g_pure_clr_param->pure_clr_det,
				comp->regs + DISP_DITHER_PURECOLOR0);
		}
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_dither1 = priv->ddp_comp[DDP_COMPONENT_DITHER1];

			index = index_of_dither(comp_dither1->id);

			if (atomic_read(&g_dither_is_clock_on[index]) != 1) {
				DDPINFO("%s: clock is off.\n",
					__func__);
			} else {
				writel(readl(comp_dither1->regs + DISP_DITHER_PURECOLOR0) |
					g_pure_clr_param->pure_clr_det,
					comp_dither1->regs + DISP_DITHER_PURECOLOR0);
			}
		}
	}
	break;
	default:
		DDPPR_ERR("%s: error cmd: %d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

int mtk_drm_ioctl_set_dither_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv)
{
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_ddp_comp *comp = private->ddp_comp[DDP_COMPONENT_DITHER0];
	struct drm_crtc *crtc = private->crtc[0];

	return mtk_crtc_user_cmd(crtc, comp, SET_PARAM, data);
}

static const struct mtk_ddp_comp_funcs mtk_disp_dither_funcs = {
	.config = mtk_dither_config,
	.start = mtk_dither_start,
	.stop = mtk_dither_stop,
	.bypass = mtk_dither_bypass,
	.user_cmd = mtk_dither_user_cmd,
	.prepare = mtk_dither_prepare,
	.unprepare = mtk_dither_unprepare,
	/* partial update
	 * .io_cmd = mtk_dither_io_cmd,
	 */
};

static int mtk_disp_dither_bind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(dev);
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

static void mtk_disp_dither_unbind(struct device *dev, struct device *master,
				   void *data)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_dither_component_ops = {
	.bind	= mtk_disp_dither_bind,
	.unbind = mtk_disp_dither_unbind,
};

void mtk_dither_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s REGS:0x%llx ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, -1);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, -1, -1);
}

static void mtk_disp_dither_dts_parse(const struct device_node *np,
	enum mtk_ddp_comp_id comp_id)
{
	if (of_property_read_u32(np, "pure_clr_det",
		&g_pure_clr_param->pure_clr_det)) {
		DDPPR_ERR("comp_id: %d, pure_clr_det = %d\n",
			comp_id, g_pure_clr_param->pure_clr_det);
		g_pure_clr_param->pure_clr_det = 0;
	}

	if (of_property_read_u32(np, "pure_clr_num",
		&g_pure_clr_param->pure_clr_num)) {
		DDPPR_ERR("comp_id: %d, pure_clr_num = %d\n",
			comp_id, g_pure_clr_param->pure_clr_num);
		g_pure_clr_param->pure_clr_num = 0;
	}

	if (of_property_read_u32_array(np, "pure_clr_rgb",
		&g_pure_clr_param->pure_clr[0][0],
		PURE_CLR_RGB * g_pure_clr_param->pure_clr_num)) {
		DDPPR_ERR("comp_id: %d, get pure_clr error\n", comp_id);
		memset(&g_pure_clr_param->pure_clr[0][0],
			0, sizeof(g_pure_clr_param->pure_clr));
	}
}

static int mtk_disp_dither_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_dither *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret, irq;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DITHER);
	if ((int)comp_id < 0) {
		DDPPR_ERR("Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	if (!default_comp)
		default_comp = &priv->ddp_comp;

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_dither_funcs);
	if (ret != 0) {
		DDPPR_ERR("Failed to initialize component: %d\n", ret);
		return ret;
	}

	priv->data = of_device_get_match_data(dev);

	priv->pwr_sta = 0;
	priv->cfg_reg = 0x80000100;

	platform_set_drvdata(pdev, priv);

	ret = devm_request_irq(dev, irq, mtk_disp_dither_irq_handler,
		IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(dev), priv);
	if (ret)
		dev_err(dev, "devm_request_irq fail: %d\n", ret);

	if (comp_id == DDP_COMPONENT_DITHER0) {
		g_pure_clr_param = devm_kzalloc(dev,
			sizeof(*g_pure_clr_param), GFP_KERNEL);
		if (g_pure_clr_param == NULL)
			return -ENOMEM;

		mtk_disp_dither_dts_parse(dev->of_node, comp_id);
	}

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_dither_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}
	dither_pure_detect_wq =
		create_singlethread_workqueue("dither_pure_detect_wq");
	INIT_WORK(&dither_pure_detect_task, dither_pure_detect_work);

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_dither_remove(struct platform_device *pdev)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_dither_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_dither_data mt6779_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_dither_data mt6885_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = false,
};

static const struct mtk_disp_dither_data mt6873_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6853_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6833_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6983_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6895_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6879_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_dither_data mt6855_dither_driver_data = {
	.support_shadow     = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_dither_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6779-disp-dither",
	  .data = &mt6779_dither_driver_data},
	{ .compatible = "mediatek,mt6789-disp-dither",
	  .data = &mt6789_dither_driver_data},
	{ .compatible = "mediatek,mt6885-disp-dither",
	  .data = &mt6885_dither_driver_data},
	{ .compatible = "mediatek,mt6873-disp-dither",
	  .data = &mt6873_dither_driver_data},
	{ .compatible = "mediatek,mt6853-disp-dither",
	  .data = &mt6853_dither_driver_data},
	{ .compatible = "mediatek,mt6833-disp-dither",
	  .data = &mt6833_dither_driver_data},
	{ .compatible = "mediatek,mt6983-disp-dither",
	  .data = &mt6983_dither_driver_data},
	{ .compatible = "mediatek,mt6895-disp-dither",
	  .data = &mt6895_dither_driver_data},
	{ .compatible = "mediatek,mt6879-disp-dither",
	  .data = &mt6879_dither_driver_data},
	{ .compatible = "mediatek,mt6855-disp-dither",
	  .data = &mt6855_dither_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_dither_driver_dt_match);

struct platform_driver mtk_disp_dither_driver = {
	.probe = mtk_disp_dither_probe,
	.remove = mtk_disp_dither_remove,
	.driver = {

			.name = "mediatek-disp-dither",
			.owner = THIS_MODULE,
			.of_match_table = mtk_disp_dither_driver_dt_match,
		},
};


void dither_test(const char *cmd, char *debug_output, struct mtk_ddp_comp *comp)
{
	unsigned int bpc;

	debug_output[0] = '\0';
	DDPINFO("%s: %s\n", __func__, cmd);

	if (strncmp(cmd, "sel:", 4) == 0) {
		if (cmd[4] == '0') {
			bpc = 0;
			mtk_dither_user_cmd(comp, NULL, DITHER_SELECT, &bpc);
			DDPINFO("bpc = 0\n");
		} else if (cmd[4] == '1') {
			bpc = 5;
			mtk_dither_user_cmd(comp, NULL, DITHER_SELECT, &bpc);
			DDPINFO("bpc = 5\n");
		} else if (cmd[4] == '2') {
			bpc = 6;
			mtk_dither_user_cmd(comp, NULL, DITHER_SELECT, &bpc);
			DDPINFO("bpc = 6\n");
		} else if (cmd[4] == '3') {
			bpc = 7;
			mtk_dither_user_cmd(comp, NULL, DITHER_SELECT, &bpc);
			DDPINFO("bpc = 7\n");
		} else {
			DDPINFO("unknown bpc\n");
		}
	}
}

void disp_dither_set_bypass(struct drm_crtc *crtc, int bypass)
{
	int ret;

	ret = mtk_crtc_user_cmd(crtc, default_comp, BYPASS_DITHER, &bypass);
	mtk_crtc_check_trigger(default_comp->mtk_crtc, false, true);

	DDPINFO("%s : ret = %d", __func__, ret);
}

void disp_dither_set_color_detect(struct drm_crtc *crtc, int enable)
{
	mtk_crtc_user_cmd(crtc, default_comp, SET_COLOR_DETECT, &enable);
	mtk_crtc_user_cmd(crtc, default_comp, SET_INTERRUPT, &enable);
	mtk_crtc_check_trigger(default_comp->mtk_crtc, false, true);
}
