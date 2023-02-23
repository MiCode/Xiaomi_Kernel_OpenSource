/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <drm/drmP.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_log.h"
#include "mtk_dump.h"

#define DISP_DITHER_EN 0x0
#define DISP_REG_DITHER_CFG 0x20
#define DISP_REG_DITHER_SIZE 0x30
#define DISP_DITHER_5 0x0114
#define DISP_DITHER_7 0x011c
#define DISP_DITHER_15 0x013c
#define DISP_DITHER_16 0x0140

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
static unsigned int g_dither_relay_value[DITHER_TOTAL_MODULE_NUM];
#define index_of_dither(module) ((module == DDP_COMPONENT_DITHER0) ? 0 : 1)
static atomic_t g_dither_is_clock_on = ATOMIC_INIT(0);
static DEFINE_SPINLOCK(g_dither_clock_lock);
// It's a work around for no comp assigned in functions.
static struct mtk_ddp_comp *default_comp;
static unsigned int g_dither_mode = 1;

enum COLOR_IOCTL_CMD {
	DITHER_SELECT = 0,
	SET_PARAM,
	BYPASS_DITHER,
	SET_INTERRUPT,
	SET_COLOR_DETECT,
};

struct mtk_disp_dither_data {
	bool support_shadow;
};

struct mtk_disp_dither {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	int pwr_sta;
	unsigned int cfg_reg;
	const struct mtk_disp_dither_data *data;
};

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

}

static void mtk_dither_start(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(comp->dev);

	DDPINFO("%s\n", __func__);

	priv->pwr_sta = 1;
}

static void mtk_dither_stop(struct mtk_ddp_comp *comp,
				struct cmdq_pkt *handle)
{
	struct mtk_disp_dither *priv = dev_get_drvdata(comp->dev);

	DDPINFO("%s\n", __func__);

	priv->pwr_sta = 0;
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
#if defined(CONFIG_DRM_MTK_SHADOW_REGISTER_SUPPORT)
	struct mtk_disp_dither *priv = dev_get_drvdata(comp->dev);
#endif

	mtk_ddp_comp_clk_prepare(comp);
	atomic_set(&g_dither_is_clock_on, 1);

#if defined(CONFIG_DRM_MTK_SHADOW_REGISTER_SUPPORT)
	if (priv->data->support_shadow) {
		/* Enable shadow register and read shadow register */
		mtk_ddp_write_mask_cpu(comp, 0x0,
			DITHER_REG(0), DITHER_BYPASS_SHADOW);
	} else {
		/* Bypass shadow register and read shadow register */
		mtk_ddp_write_mask_cpu(comp, DITHER_BYPASS_SHADOW,
			DITHER_REG(0), DITHER_BYPASS_SHADOW);
	}
#else
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6877) \
	|| defined(CONFIG_MACH_MT6781)
	/* Bypass shadow register and read shadow register */
	mtk_ddp_write_mask_cpu(comp, DITHER_BYPASS_SHADOW,
		DITHER_REG(0), DITHER_BYPASS_SHADOW);
#endif
#endif
	ddp_dither_restore(comp);
}

static void mtk_dither_unprepare(struct mtk_ddp_comp *comp)
{
	unsigned long flags;

	DDPINFO("%s @ %d......... spin_lock_irqsave ++ ",
		__func__, __LINE__);
	spin_lock_irqsave(&g_dither_clock_lock, flags);
	DDPINFO("%s @ %d......... spin_lock_irqsave -- ",
		__func__, __LINE__);
	atomic_set(&g_dither_is_clock_on, 0);
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
		writel(0x00003000 | (0x1 << g_dither_mode), comp->regs + DITHER_REG(6));
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

	pr_info("%s: bypass: %d, dither_mode: %x", __func__, bypass, dither_mode);

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

		DDPINFO("%s: relay: %d, mode: %d", __func__, relay, mode);

		if (relay == false) {
			//mtk_dither_select(comp, NULL, 8);
		} else {
			//mtk_dither_bypass(comp, true, handle);
		}
		mtk_dither_set_param(comp, handle, relay, mode);
		if (comp->mtk_crtc->is_dual_pipe) {
			struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
			struct drm_crtc *crtc = &mtk_crtc->base;
			struct mtk_drm_private *priv = crtc->dev->dev_private;
			struct mtk_ddp_comp *comp_dither1 = priv->ddp_comp[DDP_COMPONENT_DITHER1];

			mtk_dither_set_param(comp_dither1, handle, relay, mode);
		}
	}
	break;
	case BYPASS_DITHER:
	{
		int *value = data;

		mtk_dither_bypass(comp, *value, handle);
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

	DDPDUMP("== %s REGS ==\n", mtk_dump_comp_str(comp));
	mtk_cust_dump_reg(baddr, 0x0, 0x20, 0x30, -1);
	mtk_cust_dump_reg(baddr, 0x24, 0x28, -1, -1);
}

static int mtk_disp_dither_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_dither *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

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

	priv->pwr_sta = 0;
	priv->cfg_reg = 0x80000100;

	platform_set_drvdata(pdev, priv);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_disp_dither_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		pm_runtime_disable(dev);
	}
	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_dither_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &mtk_disp_dither_component_ops);

	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct mtk_disp_dither_data mt6779_dither_driver_data = {
	.support_shadow = false,
};

static const struct mtk_disp_dither_data mt6885_dither_driver_data = {
	.support_shadow = false,
};

static const struct mtk_disp_dither_data mt6873_dither_driver_data = {
	.support_shadow = false,
};

static const struct mtk_disp_dither_data mt6853_dither_driver_data = {
	.support_shadow = false,
};

static const struct mtk_disp_dither_data mt6877_dither_driver_data = {
	.support_shadow = false,
};

static const struct mtk_disp_dither_data mt6833_dither_driver_data = {
	.support_shadow = false,
};

static const struct mtk_disp_dither_data mt6781_dither_driver_data = {
	.support_shadow = false,
};

static const struct of_device_id mtk_disp_dither_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6779-disp-dither",
	  .data = &mt6779_dither_driver_data},
	{ .compatible = "mediatek,mt6885-disp-dither",
	  .data = &mt6885_dither_driver_data},
	{ .compatible = "mediatek,mt6873-disp-dither",
	  .data = &mt6873_dither_driver_data},
	{ .compatible = "mediatek,mt6853-disp-dither",
	  .data = &mt6853_dither_driver_data},
	{ .compatible = "mediatek,mt6877-disp-dither",
	  .data = &mt6877_dither_driver_data},
	{ .compatible = "mediatek,mt6833-disp-dither",
	  .data = &mt6833_dither_driver_data},
	{ .compatible = "mediatek,mt6781-disp-dither",
	  .data = &mt6781_dither_driver_data},
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
	DDPFUNC("ret = %d", ret);
}
