// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/usb/typec_dp.h>
#include <dt-bindings/soc/sprd,qogirn6pro-mask.h>
#include <dt-bindings/soc/sprd,qogirn6pro-regs.h>

#include "sprd_dp.h"
#include "../dp/dw_dptx.h"

#define BASE_REG_TCA 0x25310000
#define BASE_REG_DPU1_DPI 0x318801f0

#define REG_USB_DP_AUX_PHY_CFG 0x5c
#define REG_USB_DP_PHY_STAT0 0x74
#define REG_TCA_INTR_EN 0x4
#define REG_TCA_INTR_STS 0x8
#define REG_TCA_TCPC 0x14
#define REG_TCA_CTRLSYNCMODE_CFG0 0x20

#define REG_TCA_SIZE 0x20
#define REG_DPU1_DPI_SIZE 0x4

#define MASK_USB_DP_AUX_PHY_PWDNB 0x400
#define MASK_USB_DP_PHY0_SRAM_INIT_DONE 0x4
#define MASK_TCA_INTR_EN_ACK_EN 0x1
#define MASK_TCA_INTR_EN_TIMEOUT_EN 0x2

static struct clk *clk_ipa_apb_dpu1_eb;
static struct clk *clk_ipa_apb_dptx_eb;

static int dp_glb_parse_dt(struct dp_context *ctx,
				struct device_node *np)
{
	int ret;

	clk_ipa_apb_dpu1_eb =
		of_clk_get_by_name(np, "clk_ipa_apb_dpu1_eb");
	if (IS_ERR(clk_ipa_apb_dpu1_eb)) {
		pr_warn("read clk_ipa_apb_dpu1_eb failed\n");
		clk_ipa_apb_dpu1_eb = NULL;
	}

	clk_ipa_apb_dptx_eb =
		of_clk_get_by_name(np, "clk_ipa_apb_dptx_eb");
	if (IS_ERR(clk_ipa_apb_dptx_eb)) {
		pr_warn("read clk_ipa_apb_dptx_eb failed\n");
		clk_ipa_apb_dptx_eb = NULL;
	}

	ctx->ipa_usb31_dp = syscon_regmap_lookup_by_phandle(np,
					"sprd,syscon-usb31-dp-phy");
	if (IS_ERR(ctx->ipa_usb31_dp)) {
		pr_err("ipa usb31 dp phy syscon failed!\n");
		ctx->ipa_usb31_dp = NULL;
	}

	ctx->aon_apb = syscon_regmap_lookup_by_phandle(np,
					"sprd,syscon-aon-apb");
	if (IS_ERR(ctx->aon_apb)) {
		pr_err("aon_apb syscon failed!\n");
		ctx->aon_apb = NULL;
	}

	ctx->ipa_dispc1_glb_apb = syscon_regmap_lookup_by_phandle(np,
					"sprd,syscon-ipa-dispc1-glb-apb");
	if (IS_ERR(ctx->ipa_dispc1_glb_apb)) {
		pr_err("ipa_dispc1_glb_apb syscon failed!\n");
		ctx->ipa_dispc1_glb_apb = NULL;
	}

	ctx->ipa_apb = syscon_regmap_lookup_by_phandle(np,
					"sprd,syscon-ipa-apb");
	if (IS_ERR(ctx->ipa_apb)) {
		pr_err("ipa_apb syscon failed!\n");
		ctx->ipa_apb = NULL;
	}

	ctx->force_shutdown = syscon_regmap_lookup_by_phandle(np,
					"sprd,syscon-force-shutdown");
	if (IS_ERR(ctx->force_shutdown)) {
		pr_err("force_shutdown syscon failed!\n");
		ctx->force_shutdown = NULL;
	}

	ctx->gpio_en1 = of_get_named_gpio(np, "dp-gpios", 0);
	if (!gpio_is_valid(ctx->gpio_en1))
		ctx->gpio_en1 = -EINVAL;
	else {
		ret = gpio_request_one(ctx->gpio_en1, GPIOF_OUT_INIT_HIGH, "GPIO_EN1");
		if (ret < 0 && ret != -EBUSY)
			pr_err("gpio_request_one GPIO_EN1 failed!\n");
	}

	ctx->gpio_en2 = of_get_named_gpio(np, "dp-gpios", 1);
	if (!gpio_is_valid(ctx->gpio_en2))
		ctx->gpio_en2 = -EINVAL;
	else {
		ret = gpio_request_one(ctx->gpio_en2, GPIOF_OUT_INIT_HIGH, "GPIO_EN2");
		if (ret < 0 && ret != -EBUSY)
			pr_err("gpio_request_one GPIO_EN2 failed!\n");
	}
	ctx->dpu1_dpi_reg = ioremap_nocache(BASE_REG_DPU1_DPI, REG_DPU1_DPI_SIZE);
	ctx->tca_base = ioremap_nocache(BASE_REG_TCA, REG_TCA_SIZE);

	return 0;
}

static void dp_glb_enable(struct dp_context *ctx)
{
	int ret;

	ret = clk_prepare_enable(clk_ipa_apb_dpu1_eb);
	if (ret)
		pr_err("enable clk_ipa_apb_dptx_eb failed!\n");

	ret = clk_prepare_enable(clk_ipa_apb_dptx_eb);
	if (ret)
		pr_err("enable clk_ipa_apb_dptx_eb failed!\n");
}

static void dp_glb_disable(struct dp_context *ctx)
{
	clk_disable_unprepare(clk_ipa_apb_dptx_eb);
	clk_disable_unprepare(clk_ipa_apb_dpu1_eb);
}

static void dp_reset(struct dp_context *ctx)
{
}

static void dp_detect(struct dp_context *ctx, int hpd_status)
{
	u32 reg = 0, mask;
	struct sprd_dp *dp = container_of(ctx, struct sprd_dp, ctx);

	if (hpd_status == DP_HOT_PLUG) {
		mask = MASK_PMU_APB_PD_DPU_DP_FORCE_SHUTDOWN;
		regmap_update_bits(ctx->force_shutdown, REG_PMU_APB_PD_DPU_DP_CFG_0, mask, 0);

		/* bypass hdcp */
		regmap_write(ctx->ipa_dispc1_glb_apb, REG_DISPC1_GLB_APB_DPTX_CTRL, 0x1);

		/* aux power */
		regmap_read(ctx->ipa_usb31_dp, REG_USB_DP_AUX_PHY_CFG, &reg);
		reg |= MASK_USB_DP_AUX_PHY_PWDNB;
		regmap_write(ctx->ipa_usb31_dp, REG_USB_DP_AUX_PHY_CFG, reg);

		/* usb eb and usb ref eb :0x25000004 */
		mask = MASK_IPA_APB_USB_EB | MASK_IPA_APB_USB_REF_EB;
		regmap_update_bits(ctx->ipa_apb, REG_IPA_APB_IPA_EB, mask, mask);

		/* PHY & dpu1 Reset */
		mask = MASK_DISPC1_GLB_APB_DPU1_SOFT_RST | MASK_DISPC1_GLB_APB_PHY_SOFT_RST;
		regmap_update_bits(ctx->ipa_dispc1_glb_apb, REG_DISPC1_GLB_APB_APB_RST, mask, mask);

		/* reset usb */
		mask = MASK_IPA_APB_USB_SOFT_RST;
		regmap_update_bits(ctx->ipa_apb, REG_IPA_APB_IPA_RST, mask, mask);

		udelay(1);
		/* ssphy power on @0x64900d14 */
		mask = MASK_AON_APB_PHY_TEST_POWERDOWN;
		regmap_update_bits(ctx->aon_apb, REG_AON_APB_USB31DPCOMBPHY_CTRL, mask, 0);

		udelay(50);

		mask = MASK_DISPC1_GLB_APB_DPU1_SOFT_RST | MASK_DISPC1_GLB_APB_PHY_SOFT_RST;
		regmap_update_bits(ctx->ipa_dispc1_glb_apb, REG_DISPC1_GLB_APB_APB_RST, mask, 0);

		/* reset usb */
		mask = MASK_IPA_APB_USB_SOFT_RST;
		regmap_update_bits(ctx->ipa_apb, REG_IPA_APB_IPA_RST, mask, 0);

		while (1) {
			regmap_read(ctx->ipa_usb31_dp, REG_USB_DP_PHY_STAT0, &reg);
			if (reg & MASK_USB_DP_PHY0_SRAM_INIT_DONE)
				break;
			udelay(1);
		}

		/* workaround dpi porlarity issue */
		writel(0x3, ctx->dpu1_dpi_reg);

		dptx_core_init(dp->snps_dptx);

		/* clear TCA INT status */
		writel(0xffff, ctx->tca_base + REG_TCA_INTR_STS);

		/* enable TCA INT */
		writel(MASK_TCA_INTR_EN_ACK_EN | MASK_TCA_INTR_EN_TIMEOUT_EN,
				ctx->tca_base + REG_TCA_INTR_EN);

		reg = readl(ctx->tca_base + REG_TCA_CTRLSYNCMODE_CFG0);
		reg |= 0x6;
		writel(reg, ctx->tca_base + REG_TCA_CTRLSYNCMODE_CFG0);

		/* type-c switch port */
		regmap_read(ctx->aon_apb, REG_AON_APB_BOOT_MODE, &reg);
		if (reg & BIT(10))
			gpio_set_value(ctx->gpio_en2, 0);
		else
			gpio_set_value(ctx->gpio_en2, 1);

		gpio_set_value(ctx->gpio_en1, 0);

		/* tca config dp mode */
		reg = 0x12 | (~(reg >> 8) & 0x4);
		writel(reg, ctx->tca_base + REG_TCA_TCPC);

		/* generate HOT_PLUG interrupt */
		mask = MASK_DISPC1_GLB_APB_HPD_STATE | MASK_DISPC1_GLB_APB_DPTX_CONFIG_EN;
		regmap_write(ctx->ipa_dispc1_glb_apb, REG_DISPC1_GLB_APB_HPD_UPDATE, mask);
		mask |= MASK_DISPC1_GLB_APB_HPD_UPDATE;
		regmap_write(ctx->ipa_dispc1_glb_apb, REG_DISPC1_GLB_APB_HPD_UPDATE, mask);
	} else if (hpd_status == DP_HOT_UNPLUG) {
		/* generate HOT_UNPLUG interrupt */
		mask = MASK_DISPC1_GLB_APB_DPTX_CONFIG_EN;
		regmap_write(ctx->ipa_dispc1_glb_apb, REG_DISPC1_GLB_APB_HPD_UPDATE, mask);

		mask |= MASK_DISPC1_GLB_APB_HPD_UPDATE;
		regmap_write(ctx->ipa_dispc1_glb_apb, REG_DISPC1_GLB_APB_HPD_UPDATE, mask);
	} else if (hpd_status == DP_HPD_IRQ) {
		/* generate HOT_UNPLUG interrupt */
		mask = MASK_DISPC1_GLB_APB_DPTX_CONFIG_EN | MASK_DISPC1_GLB_APB_HPD_STATE |
				MASK_DISPC1_GLB_APB_HPD_IRQ;
		regmap_write(ctx->ipa_dispc1_glb_apb, REG_DISPC1_GLB_APB_HPD_UPDATE, mask);

		mask |= MASK_DISPC1_GLB_APB_HPD_UPDATE;
		regmap_write(ctx->ipa_dispc1_glb_apb, REG_DISPC1_GLB_APB_HPD_UPDATE, mask);
	}
}

const struct dp_glb_ops qogirn6pro_dp_glb_ops = {
	.parse_dt = dp_glb_parse_dt,
	.reset = dp_reset,
	.enable = dp_glb_enable,
	.disable = dp_glb_disable,
	.detect = dp_detect,
};

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("chen.he@unisoc.com");
MODULE_DESCRIPTION("sprd sharkl6Pro dp global APB regs low-level config");
