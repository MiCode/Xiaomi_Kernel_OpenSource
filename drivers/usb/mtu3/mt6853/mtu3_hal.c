/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <mtu3.h>
#include <mtu3_hal.h>
#include <mtk_idle.h>
#include "mtu3_priv.h"

struct ssusb_mtk *g_ssusb;

struct ssusb_mtk *get_ssusb(void)
{
	return g_ssusb;
}

int get_ssusb_ext_rscs(struct ssusb_mtk *ssusb)
{
	struct device *dev = ssusb->dev;
	struct ssusb_priv *priv;

	/* all elements are set to ZERO as default value */
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->vusb10 = devm_regulator_get(dev, "va09");
	if (IS_ERR(priv->vusb10)) {
		dev_info(dev, "failed to get vusb10\n");
		return PTR_ERR(priv->vusb10);
	}

	/* private mode setting */
	ssusb->force_vbus = true;
	ssusb->u1u2_disable = true;
	ssusb->u3_loopb_support = true;

	ssusb->priv_data = priv;
	g_ssusb = ssusb;
	return 0;
}

static int ssusb_host_clk_on(struct ssusb_mtk *ssusb)
{
	return 0;
}

static int ssusb_host_clk_off(struct ssusb_mtk *ssusb)
{
	return 0;
}

static int ssusb_sysclk_on(struct ssusb_mtk *ssusb)
{
	int ret = 0;

	ret = clk_prepare_enable(ssusb->sys_clk);
	if (ret)
		dev_info(ssusb->dev, "failed to enable sys_clk\n");

	ret = clk_prepare_enable(ssusb->host_clk);
	if (ret)
		dev_info(ssusb->dev, "failed to enable host_clk\n");

	ret = clk_prepare_enable(ssusb->ref_clk);
	if (ret)
		dev_info(ssusb->dev, "failed to enable ref_clk\n");

	ssusb_switch_usbpll_div13(ssusb, true);

	ret = clk_prepare_enable(ssusb->mcu_clk);
	if (ret)
		dev_info(ssusb->dev, "failed to enable mcu_clk\n");

	ret = clk_prepare_enable(ssusb->dma_clk);
	if (ret)
		dev_info(ssusb->dev, "failed to enable dma_clk\n");

	return ret;
}

static void ssusb_sysclk_off(struct ssusb_mtk *ssusb)
{
	clk_disable_unprepare(ssusb->dma_clk);
	clk_disable_unprepare(ssusb->mcu_clk);
	clk_disable_unprepare(ssusb->ref_clk);

	ssusb_switch_usbpll_div13(ssusb, false);

	clk_disable_unprepare(ssusb->host_clk);
	clk_disable_unprepare(ssusb->sys_clk);
}

int ssusb_clk_on(struct ssusb_mtk *ssusb, int host_mode)
{
	if (host_mode) {
		ssusb_sysclk_on(ssusb);
		ssusb_host_clk_on(ssusb);
	} else {
		ssusb_sysclk_on(ssusb);
	}
	return 0;
}

int ssusb_clk_off(struct ssusb_mtk *ssusb, int host_mode)
{
	if (host_mode) {
		ssusb_host_clk_off(ssusb);
		ssusb_sysclk_off(ssusb);
	} else {
		ssusb_sysclk_off(ssusb);
	}
	return 0;
}

int ssusb_ext_pwr_on(struct ssusb_mtk *ssusb, int mode)
{
	int ret = 0;
	struct ssusb_priv *priv;

	priv = ssusb->priv_data;
	ret = regulator_enable(priv->vusb10);
	if (ret)
		dev_info(ssusb->dev, "failed to enable vusb10\n");
	return ret;
}

int ssusb_ext_pwr_off(struct ssusb_mtk *ssusb, int mode)
{
	int ret = 0;
	struct ssusb_priv *priv;

	priv = ssusb->priv_data;
	ret = regulator_disable(priv->vusb10);
	if (ret)
		dev_info(ssusb->dev, "failed to disable vusb10\n");
	return ret;
}

void ssusb_dpidle_request(int mode)
{
	/* not support */
}
