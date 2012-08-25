/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_dsi.h"

static struct mdss_dsi_drv_pdata dsi_drv;

static unsigned char *mdss_dsi_base;

static int mdss_dsi_regulator_init(struct platform_device *pdev)
{
	int ret;
	dsi_drv.vdd_vreg = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(dsi_drv.vdd_vreg)) {
		pr_err("could not get 8941_l22, rc = %ld\n",
				PTR_ERR(dsi_drv.vdd_vreg));
		return -ENODEV;
	}

	ret = regulator_set_voltage(dsi_drv.vdd_vreg, 3000000, 3000000);
	if (ret) {
		pr_err("vdd_vreg->set_voltage failed, rc=%d\n", ret);
		return -EINVAL;
	}

	dsi_drv.vdd_io_vreg = devm_regulator_get(&pdev->dev, "vdd_io");
	if (IS_ERR(dsi_drv.vdd_io_vreg)) {
		pr_err("could not get 8941_l12, rc = %ld\n",
				PTR_ERR(dsi_drv.vdd_io_vreg));
		return -ENODEV;
	}

	ret = regulator_set_voltage(dsi_drv.vdd_io_vreg, 1800000, 1800000);
	if (ret) {
		pr_err("vdd_io_vreg->set_voltage failed, rc=%d\n", ret);
		return -EINVAL;
	}

	dsi_drv.dsi_vreg = devm_regulator_get(&pdev->dev, "vreg");
	if (IS_ERR(dsi_drv.dsi_vreg)) {
		pr_err("could not get 8941_l2, rc = %ld\n",
				PTR_ERR(dsi_drv.dsi_vreg));
		return -ENODEV;
	}

	ret = regulator_set_voltage(dsi_drv.dsi_vreg, 1200000, 1200000);
	if (ret) {
		pr_err("dsi_vreg->set_voltage failed, rc=%d\n", ret);
		return -EINVAL;
	}

	return 0;
}

static int mdss_dsi_panel_power_on(int enable)
{
	int ret;
	pr_debug("%s: enable=%d\n", __func__, enable);

	if (enable) {
		ret = regulator_set_optimum_mode(dsi_drv.vdd_vreg, 100000);
		if (ret < 0) {
			pr_err("%s: vdd_vreg set regulator mode failed.\n",
						       __func__);
			return ret;
		}

		ret = regulator_set_optimum_mode(dsi_drv.vdd_io_vreg, 100000);
		if (ret < 0) {
			pr_err("%s: vdd_io_vreg set regulator mode failed.\n",
						       __func__);
			return ret;
		}

		ret = regulator_set_optimum_mode(dsi_drv.dsi_vreg, 100000);
		if (ret < 0) {
			pr_err("%s: dsi_vreg set regulator mode failed.\n",
						       __func__);
			return ret;
		}

		ret = regulator_enable(dsi_drv.vdd_vreg);
		if (ret) {
			pr_err("%s: Failed to enable regulator.\n", __func__);
			return ret;
		}

		ret = regulator_enable(dsi_drv.vdd_io_vreg);
		if (ret) {
			pr_err("%s: Failed to enable regulator.\n", __func__);
			return ret;
		}

		ret = regulator_enable(dsi_drv.dsi_vreg);
		if (ret) {
			pr_err("%s: Failed to enable regulator.\n", __func__);
			return ret;
		}

		mdss_dsi_panel_reset(1);

	} else {

		mdss_dsi_panel_reset(0);

		ret = regulator_disable(dsi_drv.vdd_vreg);
		if (ret) {
			pr_err("%s: Failed to disable regulator.\n", __func__);
			return ret;
		}

		ret = regulator_disable(dsi_drv.vdd_io_vreg);
		if (ret) {
			pr_err("%s: Failed to disable regulator.\n", __func__);
			return ret;
		}

		ret = regulator_disable(dsi_drv.dsi_vreg);
		if (ret) {
			pr_err("%s: Failed to disable regulator.\n", __func__);
			return ret;
		}

		ret = regulator_set_optimum_mode(dsi_drv.vdd_vreg, 100);
		if (ret < 0) {
			pr_err("%s: vdd_vreg set regulator mode failed.\n",
						       __func__);
			return ret;
		}

		ret = regulator_set_optimum_mode(dsi_drv.vdd_io_vreg, 100);
		if (ret < 0) {
			pr_err("%s: vdd_io_vreg set regulator mode failed.\n",
						       __func__);
			return ret;
		}
		ret = regulator_set_optimum_mode(dsi_drv.dsi_vreg, 100);
		if (ret < 0) {
			pr_err("%s: dsi_vreg set regulator mode failed.\n",
						       __func__);
			return ret;
		}
	}
	return 0;
}

static int mdss_dsi_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_panel_info *pinfo;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;

	if (pdata->panel_info.type == MIPI_VIDEO_PANEL)
		mdss_dsi_controller_cfg(0, pdata);

	mdss_dsi_op_mode_config(DSI_CMD_MODE, pdata);

	ret = ctrl_pdata->off(pdata);
	if (ret) {
		pr_err("%s: Panel OFF failed\n", __func__);
		return ret;
	}

	spin_lock_bh(&dsi_clk_lock);
	mdss_dsi_clk_disable(pdata);

	/* disable dsi engine */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0004, 0);

	spin_unlock_bh(&dsi_clk_lock);

	mdss_dsi_unprepare_clocks();

	ret = mdss_dsi_panel_power_on(0);
	if (ret) {
		pr_err("%s: Panel power off failed\n", __func__);
		return ret;
	}

	pr_debug("%s-:\n", __func__);

	return ret;
}

static int mdss_dsi_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	u32 clk_rate;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	u32 hbp, hfp, vbp, vfp, hspw, vspw, width, height;
	u32 ystride, bpp, data;
	u32 dummy_xres, dummy_yres;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x118, 1);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x118, 0);

	mdss_dsi_phy_sw_reset((ctrl_pdata->ctrl_base));
	mdss_dsi_phy_enable((ctrl_pdata->ctrl_base), 1);
	mdss_dsi_phy_init(pdata);

	mdss_dsi_prepare_clocks();

	spin_lock_bh(&dsi_clk_lock);

	mdss_dsi_clk_enable(pdata);
	spin_unlock_bh(&dsi_clk_lock);

	clk_rate = pdata->panel_info.clk_rate;
	clk_rate = min(clk_rate, pdata->panel_info.clk_max);

	hbp = pdata->panel_info.lcdc.h_back_porch;
	hfp = pdata->panel_info.lcdc.h_front_porch;
	vbp = pdata->panel_info.lcdc.v_back_porch;
	vfp = pdata->panel_info.lcdc.v_front_porch;
	hspw = pdata->panel_info.lcdc.h_pulse_width;
	vspw = pdata->panel_info.lcdc.v_pulse_width;
	width = pdata->panel_info.xres;
	height = pdata->panel_info.yres;

	mipi  = &pdata->panel_info.mipi;
	if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
		dummy_xres = pdata->panel_info.lcdc.xres_pad;
		dummy_yres = pdata->panel_info.lcdc.yres_pad;

		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x24,
			((hspw + hbp + width + dummy_xres) << 16 |
			(hspw + hbp)));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x28,
			((vspw + vbp + height + dummy_yres) << 16 |
			(vspw + vbp)));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
			(vspw + vbp + height + dummy_yres +
				vfp - 1) << 16 | (hspw + hbp +
				width + dummy_xres + hfp - 1));

		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x30, (hspw << 16));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x34, 0);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x38, (vspw << 16));

	} else {		/* command mode */
		if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB888)
			bpp = 3;
		else if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB666)
			bpp = 3;
		else if (mipi->dst_format == DSI_CMD_DST_FORMAT_RGB565)
			bpp = 2;
		else
			bpp = 3;	/* Default format set to RGB888 */

		ystride = width * bpp + 1;

		/* DSI_COMMAND_MODE_MDP_STREAM_CTRL */
		data = (ystride << 16) | (mipi->vc << 8) | DTYPE_DCS_LWRITE;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x60, data);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x58, data);

		/* DSI_COMMAND_MODE_MDP_STREAM_TOTAL */
		data = height << 16 | width;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x64, data);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x5C, data);
	}

	mdss_dsi_host_init(mipi, pdata);

	if (mipi->force_clk_lane_hs) {
		u32 tmp;

		tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0xac);
		tmp |= (1<<28);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
		wmb();
	}

	ret = mdss_dsi_panel_power_on(1);
	if (ret) {
		pr_err("%s: Panel power on failed\n", __func__);
		return ret;
	}

	ret = ctrl_pdata->on(pdata);
	if (ret) {
		pr_err("%s: unable to initialize the panel\n", __func__);
		return ret;
	}

	mdss_dsi_op_mode_config(mipi->mode, pdata);

	pr_debug("%s-:\n", __func__);
	return ret;
}

static int mdss_dsi_resource_initialized;

static int __devinit mdss_dsi_probe(struct platform_device *pdev)
{
	int rc = 0;
	pr_debug("%s\n", __func__);

	if (pdev->dev.of_node && !mdss_dsi_resource_initialized) {
		struct resource *mdss_dsi_mres;
		pdev->id = 1;
		mdss_dsi_mres = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!mdss_dsi_mres) {
			pr_err("%s:%d unable to get the MDSS resources",
				       __func__, __LINE__);
			return -ENOMEM;
		}
		if (mdss_dsi_mres) {
			mdss_dsi_base = ioremap(mdss_dsi_mres->start,
				resource_size(mdss_dsi_mres));
			if (!mdss_dsi_base) {
				pr_err("%s:%d unable to remap dsi resources",
					       __func__, __LINE__);
				return -ENOMEM;
			}
		}

		rc = mdss_dsi_regulator_init(pdev);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: failed to init regulator, rc=%d\n",
							__func__, rc);
			iounmap(mdss_dsi_base);
			return rc;
		}

		if (mdss_dsi_clk_init(pdev)) {
			iounmap(mdss_dsi_base);
			return -EPERM;
		}

		rc = of_platform_populate(pdev->dev.of_node,
					NULL, NULL, &pdev->dev);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: failed to add child nodes, rc=%d\n",
							__func__, rc);
			iounmap(mdss_dsi_base);
			return rc;
		}

		mdss_dsi_resource_initialized = 1;
	}

	if (!mdss_dsi_resource_initialized)
		return -EPERM;

	return 0;
}

static int __devexit mdss_dsi_remove(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = platform_get_drvdata(pdev);
	iounmap(mdss_dsi_base);
	return 0;
}

struct device dsi_dev;

int dsi_panel_device_register(struct platform_device *pdev,
			      struct mdss_panel_common_pdata *panel_data,
			      char backlight_ctrl)
{
	struct mipi_panel_info *mipi;
	int rc;
	u8 lanes = 0, bpp;
	u32 h_period, v_period, dsi_pclk_rate;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	h_period = ((panel_data->panel_info.lcdc.h_pulse_width)
			+ (panel_data->panel_info.lcdc.h_back_porch)
			+ (panel_data->panel_info.xres)
			+ (panel_data->panel_info.lcdc.h_front_porch));

	v_period = ((panel_data->panel_info.lcdc.v_pulse_width)
			+ (panel_data->panel_info.lcdc.v_back_porch)
			+ (panel_data->panel_info.yres)
			+ (panel_data->panel_info.lcdc.v_front_porch));

	mipi  = &panel_data->panel_info.mipi;

	panel_data->panel_info.type =
		((mipi->mode == DSI_VIDEO_MODE)
			? MIPI_VIDEO_PANEL : MIPI_CMD_PANEL);

	if (mipi->data_lane3)
		lanes += 1;
	if (mipi->data_lane2)
		lanes += 1;
	if (mipi->data_lane1)
		lanes += 1;
	if (mipi->data_lane0)
		lanes += 1;


	if ((mipi->dst_format == DSI_CMD_DST_FORMAT_RGB888)
	    || (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB888)
	    || (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB666_LOOSE))
		bpp = 3;
	else if ((mipi->dst_format == DSI_CMD_DST_FORMAT_RGB565)
		 || (mipi->dst_format == DSI_VIDEO_DST_FORMAT_RGB565))
		bpp = 2;
	else
		bpp = 3;		/* Default format set to RGB888 */

	if (panel_data->panel_info.type == MIPI_VIDEO_PANEL &&
		!panel_data->panel_info.clk_rate) {
		h_period += panel_data->panel_info.lcdc.xres_pad;
		v_period += panel_data->panel_info.lcdc.yres_pad;

		if (lanes > 0) {
			panel_data->panel_info.clk_rate =
			((h_period * v_period * (mipi->frame_rate) * bpp * 8)
			   / lanes);
		} else {
			pr_err("%s: forcing mdss_dsi lanes to 1\n", __func__);
			panel_data->panel_info.clk_rate =
				(h_period * v_period
					 * (mipi->frame_rate) * bpp * 8);
		}
	}
	pll_divider_config.clk_rate = panel_data->panel_info.clk_rate;

	rc = mdss_dsi_clk_div_config(bpp, lanes, &dsi_pclk_rate);
	if (rc) {
		pr_err("%s: unable to initialize the clk dividers\n", __func__);
		return rc;
	}

	if ((dsi_pclk_rate < 3300000) || (dsi_pclk_rate > 103300000))
		dsi_pclk_rate = 35000000;
	mipi->dsi_pclk_rate = dsi_pclk_rate;

	ctrl_pdata = devm_kzalloc(&pdev->dev,
		sizeof(struct mdss_dsi_ctrl_pdata), GFP_KERNEL);
	if (!ctrl_pdata)
		return -ENOMEM;

	(ctrl_pdata->panel_data).on = mdss_dsi_on;
	(ctrl_pdata->panel_data).off = mdss_dsi_off;
	memcpy(&((ctrl_pdata->panel_data).panel_info),
				&(panel_data->panel_info),
				       sizeof(struct mdss_panel_info));

	mdss_dsi_irq_handler_config(ctrl_pdata);
	(ctrl_pdata->panel_data).set_backlight = panel_data->bl_fnc;
	(ctrl_pdata->ctrl_base) = mdss_dsi_base;
	(ctrl_pdata->bl_ctrl) = backlight_ctrl;
	/*
	 * register in mdp driver
	 */
	rc = mdss_register_panel(&(ctrl_pdata->panel_data));
	if (rc) {
		dev_err(&pdev->dev, "unable to register MIPI DSI panel\n");
		devm_kfree(&pdev->dev, ctrl_pdata);
		return rc;
	}

	ctrl_pdata->on = panel_data->on;
	ctrl_pdata->off = panel_data->off;

	pr_debug("%s: Panal data initialized\n", __func__);
	return 0;
}

static const struct of_device_id msm_mdss_dsi_dt_match[] = {
	{.compatible = "qcom,msm-mdss-dsi"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_mdss_dsi_dt_match);

static struct platform_driver mdss_dsi_driver = {
	.probe = mdss_dsi_probe,
	.remove = __devexit_p(mdss_dsi_remove),
	.shutdown = NULL,
	.driver = {
		.name = "mdss_dsi",
		.of_match_table = msm_mdss_dsi_dt_match,
	},
};

static int mdss_dsi_register_driver(void)
{
	return platform_driver_register(&mdss_dsi_driver);
}

static int __init mdss_dsi_driver_init(void)
{
	int ret;

	mdss_dsi_init();

	ret = mdss_dsi_register_driver();
	if (ret) {
		pr_err("mdss_dsi_register_driver() failed!\n");
		return ret;
	}

	return ret;
}
module_init(mdss_dsi_driver_init);

static void __exit mdss_dsi_driver_cleanup(void)
{
	iounmap(mdss_dsi_base);
	platform_driver_unregister(&mdss_dsi_driver);
}
module_exit(mdss_dsi_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DSI controller driver");
MODULE_AUTHOR("Chandan Uddaraju <chandanu@codeaurora.org>");
