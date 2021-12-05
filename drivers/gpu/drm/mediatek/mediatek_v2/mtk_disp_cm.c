// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_fb.h"

#define DISP_REG_CM_EN			0x0000
	#define CM_EN BIT(0)
	#define CON_FLD_DISP_CM_EN		REG_FLD_MSB_LSB(0, 0)

#define DISP_REG_CM_RESET			0x0004
	#define CM_RESET BIT(0)

#define DISP_REG_CM_INTEN			0x0008
	#define IF_END_INT_EN BIT(0)
	#define OF_END_INT_EN BIT(1)

#define DISP_REG_CM_INTSTA			0x000C
	#define IF_END_INT BIT(0)
	#define OF_END_INT BIT(1)

#define DISP_REG_CM_STATUS			0x0010
	#define IF_UNFINISH BIT(0)
	#define OF_UNFINISH BIT(1)
	#define HANDSHAKE		REG_FLD_MSB_LSB(27, 4)

#define DISP_REG_CM_CFG			0x0020
	#define CON_FLD_DISP_CM_RELAY_MODE		REG_FLD_MSB_LSB(0, 0)
	#define RELAY_MODE		BIT(0)
	#define CM_ENGINE_EN	BIT(1)
	#define CM_GAMMA_OFF	BIT(2)
	#define CM_GAMMA_ROUND_EN	BIT(4)
	#define HG_FCM_CK_ON	BIT(8)
	#define HF_FCM_CK_ON	BIT(9)
	#define CM_8B_SWITCH	BIT(10)
	#define REPEAT_DCM_OFF	BIT(12)
	#define CHKSUM_EN		BIT(28)
	#define CHKSUM_SEL		REG_FLD_MSB_LSB(30, 29)

#define DISP_REG_CM_INPUT_COUNT			0x0024
	#define INP_PIX_CNT		REG_FLD_MSB_LSB(12, 0)
	#define INP_LINE_CNT	REG_FLD_MSB_LSB(28, 16)

#define DISP_REG_CM_OUTPUT_COUNT			0x0028
	#define OUTP_PIX_CNT	REG_FLD_MSB_LSB(12, 0)
	#define OUTP_LINE_CNT	REG_FLD_MSB_LSB(28, 16)

#define DISP_REG_CM_CHKSUM		0x002C
	#define CHKSUM		REG_FLD_MSB_LSB(31, 0)

#define DISP_REG_CM_SIZE			0x0030
	#define VSIZE			REG_FLD_MSB_LSB(12, 0)
	#define HSIZE			REG_FLD_MSB_LSB(28, 16)
#define DISP_REG_CM_COEF_0			0x0080
	#define CM_C01			REG_FLD_MSB_LSB(12, 0)
	#define CM_C00			REG_FLD_MSB_LSB(28, 16)
#define DISP_REG_CM_COEF_1			0x0084
	#define CM_C10			REG_FLD_MSB_LSB(12, 0)
	#define CM_C02			REG_FLD_MSB_LSB(28, 16)
#define DISP_REG_CM_COEF_2			0x0088
	#define CM_C12			REG_FLD_MSB_LSB(12, 0)
	#define CM_C11			REG_FLD_MSB_LSB(28, 16)
#define DISP_REG_CM_COEF_3			0x008C
	#define CM_C21			REG_FLD_MSB_LSB(12, 0)
	#define CM_C20			REG_FLD_MSB_LSB(28, 16)
#define DISP_REG_CM_COEF_4			0x0090
	#define CM_COEFF_PURE_GRAY_EN	BIT(0)
	#define CM_COEFF_ROUND_EN	BIT(1)
	#define CM_C22			REG_FLD_MSB_LSB(28, 16)

#define DISP_REG_CM_PRECISION			0x0094
	#define CM_PRECISION_MASK	REG_FLD_MSB_LSB(8, 0)

#define DISP_REG_CM_SHADOW		0x00A0
	#define READ_WRK_REG BIT(0)
	#define CM_FORCE_COMMIT BIT(1)
	#define CM_BYPASS_SHADOW BIT(2)
#define DISP_REG_CM_DUMMY_REG			0x00C0
	#define DUMMY_REG	REG_FLD_MSB_LSB(31, 0)

#define DISP_REG_CM_FUNC_DCM			0x00CC
	#define CM_FUNC_DCM_DIS	REG_FLD_MSB_LSB(31, 0)

#define DISP_REG_CM_COLOR_OFFSET_0			0x0100
	#define CM_COLOR_OFFSET_PURE_GRAY_EN	BIT(31)
	#define CM_COLOR_OFFSET_COEF_0			REG_FLD_MSB_LSB(29, 0)

#define DISP_REG_DISP_CM_COLOR_OFFSET_1			0x0104
	#define CM_COLOR_OFFSET_COEF_1			REG_FLD_MSB_LSB(29, 0)
#define DISP_REG_DISP_CM_COLOR_OFFSET_2			0x0108
	#define CM_COLOR_OFFSET_COEF_2			REG_FLD_MSB_LSB(29, 0)

struct mtk_disp_cm_data {
	bool support_shadow;
	bool need_bypass_shadow;
};

/**
 * struct mtk_disp_cm - DISP_CM driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_cm {
	struct mtk_ddp_comp	 ddp_comp;
	const struct mtk_disp_cm_data *data;
	int enable;
};

static inline struct mtk_disp_cm *comp_to_cm(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_cm, ddp_comp);
}

static void mtk_cm_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	void __iomem *baddr = comp->regs;
	//struct mtk_disp_cm *cm = comp_to_cm(comp);

	mtk_ddp_write_mask(comp, CM_FORCE_COMMIT,
		DISP_REG_CM_SHADOW, CM_FORCE_COMMIT, handle);

	//if (cm->enable) {
		mtk_ddp_write_mask(comp, CM_EN, DISP_REG_CM_EN,
				CM_EN, handle);
	//}

	DDPINFO("%s, cm_start:0x%x\n",
		mtk_dump_comp_str(comp), readl(baddr + DISP_REG_CM_EN));
}

static void mtk_cm_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	void __iomem *baddr = comp->regs;

	mtk_ddp_write_mask(comp, 0x0, DISP_REG_CM_EN, CM_EN, handle);
	DDPINFO("%s, cm_stop:0x%x\n",
		mtk_dump_comp_str(comp), readl(baddr + DISP_REG_CM_EN));
}

static void mtk_cm_prepare(struct mtk_ddp_comp *comp)
{
	//struct mtk_disp_cm *cm = comp_to_cm(comp);
	DDPINFO("%s,\n", __func__);
	mtk_ddp_comp_clk_prepare(comp);

	/* Bypass shadow register and read shadow register */
	//if (cm->data->need_bypass_shadow)
	mtk_ddp_write_mask_cpu(comp, CM_BYPASS_SHADOW,
		DISP_REG_CM_SHADOW, CM_BYPASS_SHADOW);
}

static void mtk_cm_unprepare(struct mtk_ddp_comp *comp)
{
	mtk_ddp_comp_clk_unprepare(comp);
}

extern unsigned int disp_cm_bypass;

static void mtk_cm_config(struct mtk_ddp_comp *comp,
				 struct mtk_ddp_config *cfg,
				 struct cmdq_pkt *handle)
{
	struct mtk_panel_cm_params *cm_params;
	struct mtk_panel_cm_params *cm_tune_params;
	unsigned int width;

	if (comp->mtk_crtc->is_dual_pipe)
		width = cfg->w / 2;
	else
		width = cfg->w;

	DDPINFO("%s,\n", __func__);
	if (!comp->mtk_crtc || !comp->mtk_crtc->panel_ext)
		return;
	DDPMSG("mtk_cm_config111\n");
	cm_params = &comp->mtk_crtc->panel_ext->params->cm_params;
	cm_tune_params = comp->mtk_crtc->panel_cm_params;

	mtk_ddp_write(comp, 0x00000004, DISP_REG_CM_SHADOW, handle);
	mtk_ddp_write(comp, width << 16 | cfg->h, DISP_REG_CM_SIZE, handle);
	mtk_ddp_write_mask(comp, DISP_REG_CM_EN, CM_EN,
			DISP_REG_CM_EN, handle);
	mtk_ddp_write_mask(comp, CM_ENGINE_EN, DISP_REG_CM_CFG,
				CM_ENGINE_EN, handle);
	mtk_ddp_write_mask(comp, CM_GAMMA_ROUND_EN, DISP_REG_CM_CFG,
				CM_GAMMA_ROUND_EN, handle);

	if (disp_cm_bypass || cm_params->enable == 0 || cm_params->relay == 1) {
		mtk_ddp_write_mask(comp, RELAY_MODE, DISP_REG_CM_CFG,
				RELAY_MODE, handle);
		return;
	}

	if (cm_params->bits_switch == 1)
		mtk_ddp_write_mask(comp, CM_8B_SWITCH, DISP_REG_CM_CFG,
				CM_8B_SWITCH, handle);
	if (cm_tune_params && cm_tune_params->enable) {
		mtk_ddp_write(comp, cm_tune_params->cm_c00 << 16 | cm_tune_params->cm_c01,
				DISP_REG_CM_COEF_0, handle);
		mtk_ddp_write(comp, cm_tune_params->cm_c02 << 16 | cm_tune_params->cm_c10,
				DISP_REG_CM_COEF_1, handle);
		mtk_ddp_write(comp, cm_tune_params->cm_c11 << 16 | cm_tune_params->cm_c12,
				DISP_REG_CM_COEF_2, handle);
		mtk_ddp_write(comp, cm_tune_params->cm_c20 << 16 | cm_tune_params->cm_c21,
				DISP_REG_CM_COEF_3, handle);
		mtk_ddp_write(comp, cm_tune_params->cm_c22 << 16 |
					cm_params->cm_coeff_round_en << 1
					| cm_params->cm_gray_en,
					DISP_REG_CM_COEF_4, handle);
	} else {
		mtk_ddp_write(comp, cm_params->cm_c00 << 16 | cm_params->cm_c01,
				DISP_REG_CM_COEF_0, handle);
		mtk_ddp_write(comp, cm_params->cm_c02 << 16 | cm_params->cm_c10,
				DISP_REG_CM_COEF_1, handle);
		mtk_ddp_write(comp, cm_params->cm_c11 << 16 | cm_params->cm_c12,
				DISP_REG_CM_COEF_2, handle);
		mtk_ddp_write(comp, cm_params->cm_c20 << 16 | cm_params->cm_c21,
				DISP_REG_CM_COEF_3, handle);
		mtk_ddp_write(comp, cm_params->cm_c22 << 16
					| cm_params->cm_coeff_round_en << 1
					| cm_params->cm_gray_en,
					DISP_REG_CM_COEF_4, handle);
	}
	mtk_ddp_write(comp, cm_params->cm_precision_mask,
			DISP_REG_CM_PRECISION, handle);
}

void mtk_cm_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	int i;

	DDPDUMP("== %s REGS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);

	DDPDUMP("en=%d, cm_bypass=%d\n",
		 DISP_REG_GET_FIELD(CON_FLD_DISP_CM_EN,
				baddr + DISP_REG_CM_EN),
		 DISP_REG_GET_FIELD(CON_FLD_DISP_CM_RELAY_MODE,
				baddr + DISP_REG_CM_CFG));
	DDPDUMP("-- Start dump cm registers --\n");
	for (i = 0; i < 300; i += 16) {
		DDPDUMP("CM+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(baddr + i),
			 readl(baddr + i + 0x4), readl(baddr + i + 0x8),
			 readl(baddr + i + 0xc));
	}
}

int mtk_cm_analysis(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	DDPDUMP("== %s ANALYSIS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	DDPDUMP("en=%d, cm_bypass=%d\n",
		 DISP_REG_GET_FIELD(CON_FLD_DISP_CM_EN,
				baddr + DISP_REG_CM_EN),
		 DISP_REG_GET_FIELD(CON_FLD_DISP_CM_RELAY_MODE,
				baddr + DISP_REG_CM_CFG));

	return 0;
}

static const struct mtk_ddp_comp_funcs mtk_disp_cm_funcs = {
	.config = mtk_cm_config,
	.start = mtk_cm_start,
	.stop = mtk_cm_stop,
	.prepare = mtk_cm_prepare,
	.unprepare = mtk_cm_unprepare,
};

static int mtk_disp_cm_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_disp_cm *priv = dev_get_drvdata(dev);
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

static void mtk_disp_cm_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct mtk_disp_cm *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_cm_component_ops = {
	.bind = mtk_disp_cm_bind,
	.unbind = mtk_disp_cm_unbind,
};

static int mtk_disp_cm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_cm *priv;
	enum mtk_ddp_comp_id comp_id;
	int irq;
	int ret;

	DDPMSG("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_CM);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_cm_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_cm_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPMSG("%s-\n", __func__);
	return ret;
}

static int mtk_disp_cm_remove(struct platform_device *pdev)
{
	struct mtk_disp_cm *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_cm_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct mtk_disp_cm_data mt6853_cm_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_cm_data mt6983_cm_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_cm_data mt6895_cm_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct mtk_disp_cm_data mt6879_cm_driver_data = {
	.support_shadow = false,
	.need_bypass_shadow = true,
};

static const struct of_device_id mtk_disp_cm_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6853-disp-cm",
	  .data = &mt6853_cm_driver_data},
	{ .compatible = "mediatek,mt6983-disp-cm",
	  .data = &mt6983_cm_driver_data},
	{ .compatible = "mediatek,mt6895-disp-cm",
	  .data = &mt6895_cm_driver_data},
	{ .compatible = "mediatek,mt6879-disp-cm",
	  .data = &mt6879_cm_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_cm_driver_dt_match);

struct platform_driver mtk_disp_cm_driver = {
	.probe = mtk_disp_cm_probe,
	.remove = mtk_disp_cm_remove,
	.driver = {
		.name = "mediatek-disp-cm",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_cm_driver_dt_match,
	},
};
