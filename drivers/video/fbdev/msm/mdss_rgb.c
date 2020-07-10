// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018, 2020, The Linux Foundation. All rights reserved. */


#include <linux/module.h>
#include <linux/err.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/msm-bus.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>

#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_rgb.h"
#include "mdss_debug.h"

static struct mdss_rgb_data *mdss_rgb_res;
static struct spi_device *spi_dev;

static int mdss_rgb_spi_write(struct mdss_rgb_data *rgb_data,
		bool cmd, u8 data)
{
	struct spi_transfer xfer = { 0 };
	struct spi_message msg;
	u16 txbuf;

	if (cmd)
		txbuf = (1 << 8) | data;
	else
		txbuf = data;

	spi_message_init(&msg);

	xfer.tx_buf = &txbuf;
	xfer.bits_per_word = 9;
	xfer.len = sizeof(txbuf);

	spi_message_add_tail(&xfer, &msg);
	return spi_sync(rgb_data->spi, &msg);
}

int mdss_rgb_write_command(struct mdss_rgb_data *rgb_data, u8 cmd)
{
	int rc;

	rc = mdss_rgb_spi_write(rgb_data, false, cmd);
	if (rc)
		pr_err("%s: spi write failed. cmd = 0x%x rc = %d\n",
				__func__, cmd, rc);

	return rc;
}

int mdss_rgb_write_data(struct mdss_rgb_data *rgb_data, u8 data)
{
	int rc;

	rc = mdss_rgb_spi_write(rgb_data, true, data);
	if (rc)
		pr_err("%s: spi write failed. data = 0x%x rc = %d\n",
				__func__, data, rc);

	return rc;

}

int mdss_rgb_read_command(struct mdss_rgb_data *rgb_data,
		u8 cmd, u8 *data, u8 data_len)
{
	int rc;

	rc = spi_write_then_read(spi_dev, &cmd, 1, data, data_len);
	if (rc)
		pr_err("%s: spi read failed. rc = %d\n", __func__, rc);

	return rc;
}

static int mdss_rgb_core_clk_init(struct platform_device *pdev,
		struct mdss_rgb_data *rgb_data)
{
	struct device *dev = NULL;
	int rc = 0;

	dev = &pdev->dev;

	rgb_data->mdp_core_clk = devm_clk_get(dev, "mdp_core_clk");
	if (IS_ERR(rgb_data->mdp_core_clk)) {
		rc = PTR_ERR(rgb_data->mdp_core_clk);
		pr_err("%s: Unable to get mdp core clk. rc=%d\n",
				__func__, rc);
		return rc;
	}

	rgb_data->ahb_clk = devm_clk_get(dev, "iface_clk");
	if (IS_ERR(rgb_data->ahb_clk)) {
		rc = PTR_ERR(rgb_data->ahb_clk);
		pr_err("%s: Unable to get mdss ahb clk. rc=%d\n",
				__func__, rc);
		return rc;
	}

	rgb_data->axi_clk = devm_clk_get(dev, "bus_clk");
	if (IS_ERR(rgb_data->axi_clk)) {
		rc = PTR_ERR(rgb_data->axi_clk);
		pr_err("%s: Unable to get axi bus clk. rc=%d\n",
				__func__, rc);
		return rc;
	}

	rgb_data->ext_byte0_clk = devm_clk_get(dev, "ext_byte0_clk");
	if (IS_ERR(rgb_data->ext_byte0_clk)) {
		pr_debug("%s: unable to get byte0 clk rcg. rc=%d\n",
				__func__, rc);
		rgb_data->ext_byte0_clk = NULL;
	}

	rgb_data->ext_pixel0_clk = devm_clk_get(dev, "ext_pixel0_clk");
	if (IS_ERR(rgb_data->ext_pixel0_clk)) {
		pr_debug("%s: unable to get pixel0 clk rcg. rc=%d\n",
				__func__, rc);
		rgb_data->ext_pixel0_clk = NULL;
	}

	rgb_data->mmss_misc_ahb_clk = devm_clk_get(dev, "core_mmss_clk");
	if (IS_ERR(rgb_data->mmss_misc_ahb_clk)) {
		rgb_data->mmss_misc_ahb_clk = NULL;
		pr_debug("%s: Unable to get mmss misc ahb clk\n",
				__func__);
	}

	rgb_data->mnoc_clk = devm_clk_get(dev, "mnoc_clk");
	if (IS_ERR(rgb_data->mnoc_clk)) {
		pr_debug("%s: Unable to get mnoc clk\n", __func__);
		rgb_data->mnoc_clk = NULL;
	}

	return 0;
}

static int mdss_rgb_bus_scale_init(struct platform_device *pdev,
		struct mdss_rgb_data *rgb_data)
{
	int rc = 0;

	rgb_data->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (IS_ERR_OR_NULL(rgb_data->bus_scale_table)) {
		rc = PTR_ERR(rgb_data->bus_scale_table);
		pr_err("%s: msm_bus_cl_get_pdata() failed, rc=%d\n", __func__,
				rc);
		rgb_data->bus_scale_table = NULL;
		return rc;
	}

	rgb_data->bus_handle =
		msm_bus_scale_register_client(rgb_data->bus_scale_table);

	if (!rgb_data->bus_handle) {
		rc = -EINVAL;
		pr_err("%sbus_client register failed\n", __func__);
	}

	return rc;

}

static void mdss_rgb_bus_scale_deinit(struct mdss_rgb_data *sdata)
{
	if (sdata->bus_handle) {
		if (sdata->bus_refcount)
			msm_bus_scale_client_update_request(sdata->bus_handle,
					0);

		sdata->bus_refcount = 0;
		msm_bus_scale_unregister_client(sdata->bus_handle);
		sdata->bus_handle = 0;
	}
}

static int mdss_rgb_regulator_init(struct platform_device *pdev,
		struct mdss_rgb_data *rgb_data)
{
	int rc = 0, i = 0, j = 0;

	for (i = DSI_CORE_PM; !rc && (i < DSI_MAX_PM); i++) {
		rc = msm_dss_config_vreg(&pdev->dev,
			rgb_data->power_data[i].vreg_config,
			rgb_data->power_data[i].num_vreg, 1);
		if (rc) {
			pr_err("%s: failed to init vregs for %s\n",
				__func__, __mdss_dsi_pm_name(i));
			for (j = i-1; j >= DSI_CORE_PM; j--) {
				msm_dss_config_vreg(&pdev->dev,
					rgb_data->power_data[j].vreg_config,
					rgb_data->power_data[j].num_vreg, 0);
			}
		}
	}
	return rc;
}

static void mdss_rgb_res_deinit(struct platform_device *pdev)
{
	int i;
	struct mdss_rgb_data *sdata = platform_get_drvdata(pdev);

	for (i = (DSI_MAX_PM - 1); i >= DSI_CORE_PM; i--) {
		if (msm_dss_config_vreg(&pdev->dev,
					sdata->power_data[i].vreg_config,
					sdata->power_data[i].num_vreg, 1) < 0)
			pr_err("%s: failed to de-init vregs for %s\n",
					__func__, __mdss_dsi_pm_name(i));
		mdss_dsi_put_dt_vreg_data(&pdev->dev,
				&sdata->power_data[i]);
	}

	mdss_rgb_bus_scale_deinit(sdata);
}

static int mdss_rgb_res_init(struct platform_device *pdev)
{
	int rc = 0, i = 0;

	mdss_rgb_res = platform_get_drvdata(pdev);
	if (!mdss_rgb_res) {
		mdss_rgb_res = devm_kzalloc(&pdev->dev,
					sizeof(*mdss_rgb_res),
					GFP_KERNEL);
		if (!mdss_rgb_res) {
			rc = -ENOMEM;
			goto mem_fail;
		}

		mdss_rgb_res->spi = spi_dev;
		mdss_rgb_res->panel_data.panel_info.pdest = DISPLAY_1;
		mdss_rgb_res->ndx = DSI_CTRL_0;

		rc = mdss_rgb_core_clk_init(pdev, mdss_rgb_res);
		if (rc)
			goto mem_fail;

		/* Parse the ctrl regulator information */
		for (i = DSI_CORE_PM; i < DSI_MAX_PM; i++) {
			rc = mdss_dsi_get_dt_vreg_data(&pdev->dev,
					pdev->dev.of_node,
					&mdss_rgb_res->power_data[i], i);
			if (rc) {
				pr_err("%s: '%s' get_dt_vreg_data failed.rc=%d\n",
					__func__, __mdss_dsi_pm_name(i), rc);
				i--;
				for (; i >= DSI_CORE_PM; i--)
					mdss_dsi_put_dt_vreg_data(&pdev->dev,
						&mdss_rgb_res->power_data[i]);
				goto mem_fail;
			}
		}
		rc = mdss_rgb_regulator_init(pdev, mdss_rgb_res);
		if (rc)
			goto mem_fail;

		rc = mdss_rgb_bus_scale_init(pdev, mdss_rgb_res);
		if (rc)
			goto mem_fail;

		platform_set_drvdata(pdev, mdss_rgb_res);
	}

	mdss_rgb_res->pdev = pdev;
	pr_debug("%s: Setting up mdss_rgb_res=%pK\n", __func__, mdss_rgb_res);

	return 0;

mem_fail:
	mdss_rgb_res_deinit(pdev);
	return rc;
}

static int mdss_rgb_get_panel_cfg(char *panel_cfg, size_t panel_cfg_len)
{
	int rc;
	struct mdss_panel_cfg *pan_cfg = NULL;
	struct mdss_util_intf *util;

	util = mdss_get_util_intf();

	pan_cfg = util->panel_intf_type(MDSS_PANEL_INTF_RGB);
	if (IS_ERR(pan_cfg)) {
		return PTR_ERR(pan_cfg);
	} else if (!pan_cfg) {
		panel_cfg[0] = 0;
		return 0;
	}

	pr_debug("%s:%d: cfg:[%s]\n", __func__, __LINE__,
			pan_cfg->arg_cfg);
	rc = strlcpy(panel_cfg, pan_cfg->arg_cfg,
			min(panel_cfg_len, sizeof(pan_cfg->arg_cfg)));

	return rc;
}

static struct device_node *mdss_rgb_pref_prim_panel(
		struct platform_device *pdev)
{
	struct device_node *dsi_pan_node = NULL;

	pr_debug("%s:%d: Select primary panel from dt\n",
			__func__, __LINE__);
	dsi_pan_node = of_parse_phandle(pdev->dev.of_node,
			"qcom,rgb-panel", 0);
	if (!dsi_pan_node)
		pr_err("%s:can't find panel phandle\n", __func__);

	return dsi_pan_node;
}

static struct device_node *mdss_rgb_find_panel_of_node(
		struct platform_device *pdev, char *panel_cfg)
{
	int len;
	char panel_name[MDSS_MAX_PANEL_LEN] = "";
	struct device_node *dsi_pan_node = NULL;
	struct mdss_rgb_data *rgb_data = platform_get_drvdata(pdev);

	len = strlen(panel_cfg);
	rgb_data->panel_data.dsc_cfg_np_name[0] = '\0';
	if (!len) {
		/* no panel cfg chg, parse dt */
		pr_err("%s:%d: no cmd line cfg present\n",
				__func__, __LINE__);
		goto end;
	}

end:
	if (strcmp(panel_name, NONE_PANEL))
		dsi_pan_node = mdss_rgb_pref_prim_panel(pdev);

	return dsi_pan_node;
}

static struct device_node *mdss_rgb_config_panel(struct platform_device *pdev)
{
	char panel_cfg[MDSS_MAX_PANEL_LEN] = { 0 };
	struct device_node *dsi_pan_node = NULL;
	struct mdss_rgb_data *rgb_data = platform_get_drvdata(pdev);
	int rc = 0;

	rc = mdss_rgb_get_panel_cfg(panel_cfg, MDSS_MAX_PANEL_LEN);
	if (!rc)
		pr_warn("%s:%d:dsi specific cfg not present\n",
				__func__, __LINE__);

	dsi_pan_node = mdss_rgb_find_panel_of_node(pdev, panel_cfg);
	if (!dsi_pan_node) {
		pr_err("%s: can't find panel node %s\n", __func__, panel_cfg);
		return NULL;
	}

	rc = mdss_rgb_panel_init(dsi_pan_node, rgb_data);
	if (rc) {
		pr_err("%s: dsi panel init failed\n", __func__);
		of_node_put(dsi_pan_node);
		return NULL;
	}

	return dsi_pan_node;
}

static int mdss_rgb_set_clk_rates(struct mdss_rgb_data *rgb_data)
{
	int rc;

	rc = mdss_dsi_clk_set_link_rate(rgb_data->clk_handle,
			MDSS_DSI_LINK_BYTE_CLK,
			rgb_data->byte_clk_rate,
			MDSS_DSI_CLK_UPDATE_CLK_RATE_AT_ON);
	if (rc) {
		pr_err("%s: dsi_byte_clk - clk_set_rate failed\n",
				__func__);
		return rc;
	}
	rc = mdss_dsi_clk_set_link_rate(rgb_data->clk_handle,
			MDSS_DSI_LINK_PIX_CLK,
			rgb_data->pclk_rate,
			MDSS_DSI_CLK_UPDATE_CLK_RATE_AT_ON);
	if (rc)
		pr_err("%s: dsi_pixel_clk - clk_set_rate failed\n",
				__func__);

	return rc;
}

int mdss_rgb_pix_clk_init(struct platform_device *pdev,
		struct mdss_rgb_data *rgb_data)
{
	struct device *dev = NULL;
	int rc = 0;

	dev = &pdev->dev;
	rgb_data->byte_clk_rgb = devm_clk_get(dev, "byte_clk");
	if (IS_ERR(rgb_data->byte_clk_rgb)) {
		rc = PTR_ERR(rgb_data->byte_clk_rgb);
		pr_err("%s: can't find dsi_byte_clk. rc=%d\n",
				__func__, rc);
		rgb_data->byte_clk_rgb = NULL;
		return rc;
	}

	rgb_data->pixel_clk_rgb = devm_clk_get(dev, "pixel_clk");
	if (IS_ERR(rgb_data->pixel_clk_rgb)) {
		rc = PTR_ERR(rgb_data->pixel_clk_rgb);
		pr_err("%s: can't find rgb_pixel_clk. rc=%d\n",
				__func__, rc);
		rgb_data->pixel_clk_rgb = NULL;
		return rc;
	}

	rgb_data->byte_clk_rcg = devm_clk_get(dev, "byte_clk_rcg");
	if (IS_ERR(rgb_data->byte_clk_rcg)) {
		pr_debug("%s: can't find byte clk rcg. rc=%d\n", __func__, rc);
		rgb_data->byte_clk_rcg = NULL;
	}

	rgb_data->pixel_clk_rcg = devm_clk_get(dev, "pixel_clk_rcg");
	if (IS_ERR(rgb_data->pixel_clk_rcg)) {
		pr_debug("%s: can't find pixel clk rcg. rc=%d\n", __func__, rc);
		rgb_data->pixel_clk_rcg = NULL;
	}

	return 0;
}

static int mdss_rgb_ctrl_clock_init(struct platform_device *ctrl_pdev,
		struct mdss_rgb_data *rgb_data)
{
	int rc = 0;
	struct mdss_dsi_clk_info info;
		struct mdss_dsi_clk_client client1 = {"dsi_clk_client"};
	struct mdss_dsi_clk_client client2 = {"mdp_event_client"};
	void *handle;


	if (mdss_rgb_pix_clk_init(ctrl_pdev, rgb_data))
		return -EPERM;

	memset(&info, 0x0, sizeof(info));

	info.core_clks.mdp_core_clk = rgb_data->mdp_core_clk;
	info.core_clks.mmss_misc_ahb_clk = rgb_data->mmss_misc_ahb_clk;
	info.link_clks.byte_clk = rgb_data->byte_clk_rgb;
	info.link_clks.pixel_clk = rgb_data->pixel_clk_rgb;

	info.priv_data = rgb_data;
	snprintf(info.name, sizeof(info.name), "DSI0");
	rgb_data->clk_mngr = mdss_dsi_clk_init(&info);

	if (IS_ERR_OR_NULL(rgb_data->clk_mngr)) {
		rc = PTR_ERR(rgb_data->clk_mngr);
		rgb_data->clk_mngr = NULL;
		pr_err("dsi clock registration failed, rc = %d\n", rc);
		return rc;
	}

	handle = mdss_dsi_clk_register(rgb_data->clk_mngr, &client1);
	if (IS_ERR_OR_NULL(handle)) {
		rc = PTR_ERR(handle);
		pr_err("failed to register %s client, rc = %d\n",
				client1.client_name, rc);
		return rc;
	}
	rgb_data->clk_handle = handle;

	handle = mdss_dsi_clk_register(rgb_data->clk_mngr, &client2);
	if (IS_ERR_OR_NULL(handle)) {
		rc = PTR_ERR(handle);
		pr_err("failed to register %s client, rc = %d\n",
				client2.client_name, rc);
		goto error_clk_client_deregister;
	} else {
		rgb_data->mdp_clk_handle = handle;
	}

	return 0;

error_clk_client_deregister:
	mdss_dsi_clk_deregister(rgb_data->clk_handle);
	return rc;
}

static int mdss_rgb_pinctrl_init(struct platform_device *pdev)
{
	struct mdss_rgb_data *rgb_data;

	rgb_data = platform_get_drvdata(pdev);
	rgb_data->pin_res.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(rgb_data->pin_res.pinctrl)) {
		pr_err("%s: failed to get pinctrl\n", __func__);
		return PTR_ERR(rgb_data->pin_res.pinctrl);
	}

	rgb_data->pin_res.gpio_state_active
		= pinctrl_lookup_state(rgb_data->pin_res.pinctrl,
				MDSS_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(rgb_data->pin_res.gpio_state_active))
		pr_warn("%s: can not get default pinstate\n", __func__);

	rgb_data->pin_res.gpio_state_suspend
		= pinctrl_lookup_state(rgb_data->pin_res.pinctrl,
				MDSS_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(rgb_data->pin_res.gpio_state_suspend))
		pr_warn("%s: can not get sleep pinstate\n", __func__);

	return 0;
}

static void mdss_rgb_config_clk_src(struct platform_device *pdev)
{
	struct mdss_rgb_data *rgb_res = platform_get_drvdata(pdev);

	if (!rgb_res->ext_pixel0_clk) {
		pr_err("%s: RGB ext. clocks not present\n", __func__);
		return;
	}

	if (rgb_res->pll_src_config == PLL_SRC_DEFAULT) {
		rgb_res->byte0_parent = rgb_res->ext_byte0_clk;
		rgb_res->pixel0_parent = rgb_res->ext_pixel0_clk;
	}
	pr_debug("%s: default: DSI0 <--> PLL0\n", __func__);
}

static int mdss_rgb_set_clk_src(struct mdss_rgb_data *ctrl)
{
	int rc;
	struct clk  *byte_parent, *pixel_parent = NULL;

	if (!ctrl->byte_clk_rcg || !ctrl->pixel_clk_rcg) {
		pr_debug("%s: set_clk_src not needed\n", __func__);
		return 0;
	}

	byte_parent = ctrl->byte0_parent;
	pixel_parent = ctrl->pixel0_parent;

	if (pixel_parent == NULL || byte_parent == NULL)
		pr_debug("%s : clk_parent is null\n", __func__);

	rc = clk_set_parent(ctrl->byte_clk_rcg, byte_parent);
	if (rc) {
		pr_err("%s: failed to set parent for byte clk for rgb. rc=%d\n",
				__func__, rc);
		goto error;
	}

	rc = clk_set_parent(ctrl->pixel_clk_rcg, pixel_parent);
	if (rc) {
		pr_err("%s: failed to set parent for pixel clk for rgb. rc=%d\n",
				__func__, rc);
		goto error;
	}

	pr_debug("%s: rgb clock source set to pixel\n", __func__);
error:
	return rc;
}

static int mdss_rgb_probe(struct spi_device *spi, struct platform_device *pdev)
{
	struct mdss_panel_cfg *pan_cfg = NULL;
	struct mdss_util_intf *util;
	char *panel_cfg;
	struct device_node *dsi_pan_node = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int rc = 0;

	if (spi) {
		spi_dev = spi;
		spi_dev->bits_per_word = 9;
		return 0;
	}

	if (!spi_dev) {
		pr_err("%s: SPI controller not probed yet\n", __func__);
		return -EPROBE_DEFER;
	}

	if (!pdev || !pdev->dev.of_node) {
		pr_err("%s: RGB driver only supports device tree probe\n",
				__func__);
		return -ENOTSUPP;
	}

	util = mdss_get_util_intf();
	if (util == NULL) {
		pr_err("%s: Failed to get mdss utility functions\n", __func__);
		return -ENODEV;
	}

	if (!util->mdp_probe_done) {
		pr_err("%s: MDP not probed yet\n", __func__);
		return -EPROBE_DEFER;
	}

	pan_cfg = util->panel_intf_type(MDSS_PANEL_INTF_RGB);
	if (IS_ERR_OR_NULL(pan_cfg)) {
		rc = PTR_ERR(pan_cfg);
		return rc;
	}
	panel_cfg = pan_cfg->arg_cfg;

	rc = mdss_rgb_res_init(pdev);
	if (rc)
		return rc;

	rc = mdss_rgb_pinctrl_init(pdev);

	rc = mdss_rgb_ctrl_clock_init(pdev, mdss_rgb_res);
	if (rc)
		return rc;

	dsi_pan_node = mdss_rgb_config_panel(pdev);
	if (!dsi_pan_node)
		return -EINVAL;

	rc = rgb_panel_device_register(pdev, dsi_pan_node, mdss_rgb_res);
	if (rc)
		return rc;

	pinfo = &(mdss_rgb_res->panel_data.panel_info);
	rc = mdss_rgb_set_clk_rates(mdss_rgb_res);
	if (rc)
		return rc;

	mdss_rgb_config_clk_src(pdev);

	return 0;
}

int mdss_rgb_clk_div_config(struct mdss_rgb_data *rgb_data,
		struct mdss_panel_info *panel_info, int frame_rate)
{
	u64 h_period, v_period, clk_rate;
	u8 bpp;

	if (!panel_info)
		return -EINVAL;

	pr_debug("mipi.dst_format = %d\n", panel_info->mipi.dst_format);
	switch (panel_info->mipi.dst_format) {
	case DSI_CMD_DST_FORMAT_RGB888:
	case DSI_VIDEO_DST_FORMAT_RGB888:
	case DSI_VIDEO_DST_FORMAT_RGB666:
		bpp = 3;
		break;
	case DSI_CMD_DST_FORMAT_RGB565:
	case DSI_VIDEO_DST_FORMAT_RGB565:
		bpp = 2;
		break;
	default:
		bpp = 3;        /* Default format set to RGB888 */
		break;
	}

	h_period = mdss_panel_get_htotal(panel_info, true);
	v_period = mdss_panel_get_vtotal(panel_info);

	panel_info->clk_rate = h_period * v_period * frame_rate * bpp * 8;

	clk_rate = panel_info->clk_rate;
	do_div(clk_rate, 8 * bpp);

	panel_info->mipi.dsi_pclk_rate = (u32) clk_rate;

	return 0;
}
static int mdss_rgb_pinctrl_set_state(struct mdss_rgb_data *rgb_data,
		bool active)
{
	struct pinctrl_state *pin_state;
	int ret = 0;

	pin_state = active ? rgb_data->pin_res.gpio_state_active
		: rgb_data->pin_res.gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pin_state)) {
		ret = pinctrl_select_state(rgb_data->pin_res.pinctrl,
				pin_state);
		if (ret)
			pr_err("%s: can not set %s pins\n", __func__,
				active ? MDSS_PINCTRL_STATE_DEFAULT
				: MDSS_PINCTRL_STATE_SLEEP);
	} else {
		pr_err("%s: invalid '%s' pinstate\n", __func__,
			active ? MDSS_PINCTRL_STATE_DEFAULT
			: MDSS_PINCTRL_STATE_SLEEP);
	}
	return ret;
}

static int mdss_rgb_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_rgb_data *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_rgb_data,
			panel_data);

	ret = mdss_rgb_pinctrl_set_state(ctrl_pdata, false);

	ret = msm_dss_enable_vreg(
			ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg, 0);
	if (ret)
		pr_err("%s: failed to disable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));

	return ret;
}

static int mdss_rgb_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_rgb_data *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_rgb_data,
			panel_data);

	ret = msm_dss_enable_vreg(
			ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg, 1);
	if (ret) {
		pr_err("%s: failed to enable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
		return ret;
	}

	ret  = mdss_rgb_pinctrl_set_state(ctrl_pdata, true);

	return ret;
}

int mdss_rgb_panel_power_ctrl(struct mdss_panel_data *pdata, int power_state)
{
	int ret = 0;
	struct mdss_panel_info *pinfo;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	pr_debug("%s: cur_power_state=%d req_power_state=%d\n", __func__,
			pinfo->panel_power_state, power_state);

	if (pinfo->panel_power_state == power_state) {
		pr_debug("%s: no change needed\n", __func__);
		return 0;
	}

	switch (power_state) {
	case MDSS_PANEL_POWER_ON:
		ret = mdss_rgb_panel_power_on(pdata);
		break;
	case MDSS_PANEL_POWER_OFF:
		ret = mdss_rgb_panel_power_off(pdata);
		break;
	default:
		pr_err("%s: unknown panel power state requested (%d)\n",
				__func__, power_state);
		ret = -EINVAL;
	}
	if (!ret)
		pinfo->panel_power_state = power_state;

	return ret;
}

static void mdss_rgb_phy_config(struct mdss_rgb_data *ctrl, bool phy_enable)
{
	if (phy_enable) {
		/* clk_en */
		MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_CMN_GLBL_TEST_CTRL, 0x1);
		MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_PLL_CLKBUFLR_EN, 0x01);
	} else {
		MIPI_OUTP(ctrl->phy_io.base + DSIPHY_PLL_CLKBUFLR_EN, 0);
		MIPI_OUTP(ctrl->phy_io.base + DSIPHY_CMN_GLBL_TEST_CTRL, 0);
	}
}

static int mdss_rgb_unblank(struct mdss_panel_data *pdata)
{
	struct mdss_rgb_data *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_rgb_data,
			panel_data);

	ctrl_pdata->on(pdata);

	return 0;
}

static int mdss_rgb_blank(struct mdss_panel_data *pdata, int power_state)
{
	struct mdss_rgb_data *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_rgb_data,
			panel_data);

	ctrl_pdata->off(pdata);

	return 0;
}

int mdss_rgb_on(struct mdss_panel_data *pdata)
{
	int rc = 0;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	struct mdss_rgb_data *ctrl_pdata = NULL;
	int cur_power_state;

	ctrl_pdata = container_of(pdata, struct mdss_rgb_data,
			panel_data);

	cur_power_state = pdata->panel_info.panel_power_state;
	pr_debug("%s+: ctrl=%pK cur_power_state=%d\n", __func__,
			ctrl_pdata, cur_power_state);

	pinfo = &pdata->panel_info;
	mipi = &pdata->panel_info.mipi;

	rc = mdss_rgb_panel_power_ctrl(pdata, MDSS_PANEL_POWER_ON);

	if (rc)
		goto end;

	if (mdss_panel_is_power_on(cur_power_state)) {
		pr_debug("%s: rgb_on from panel low power state\n", __func__);
		goto end;
	}

	rc = mdss_rgb_set_clk_src(ctrl_pdata);

	rc = mdss_dsi_clk_req_state(ctrl_pdata->clk_handle,
			MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_ON, 0);
	if (rc) {
		pr_err("%s: failed set clk state for Core Clks, rc = %d\n",
				__func__, rc);
		goto end;
	}
	mdss_rgb_phy_config(ctrl_pdata, 1);

	rc = mdss_dsi_clk_req_state(ctrl_pdata->clk_handle,
			MDSS_DSI_LINK_CLK, MDSS_DSI_CLK_ON, 0);
	if (rc)
		pr_err("%s: failed set clk state for Link Clks, rc = %d\n",
				__func__, rc);

end:
	pr_debug("%s-:\n", __func__);
	return rc;
}

static int mdss_rgb_off(struct mdss_panel_data *pdata, int power_state)
{
	int ret = 0;
	struct mdss_rgb_data *ctrl_pdata = NULL;
	struct mdss_panel_info *panel_info = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_rgb_data,
				panel_data);

	panel_info = &ctrl_pdata->panel_data.panel_info;

	pr_debug("%s+: ctrl=%pK power_state=%d\n",
		__func__, ctrl_pdata, power_state);

	if (power_state == panel_info->panel_power_state) {
		pr_debug("%s: No change in power state %d -> %d\n", __func__,
			panel_info->panel_power_state, power_state);
		goto end;
	}

	if (mdss_panel_is_power_on(power_state)) {
		pr_debug("%s: dsi_off with panel always on\n", __func__);
		goto panel_power_ctrl;
	}

	/*
	 * Link clocks should be turned off before PHY can be disabled.
	 * For command mode panels, all clocks are turned off prior to reaching
	 * here, so core clocks should be turned on before accessing hardware
	 * registers. For video mode panel, turn off link clocks and then
	 * disable PHY
	 */
	if (pdata->panel_info.type == MIPI_CMD_PANEL)
		ret = mdss_dsi_clk_req_state(ctrl_pdata->clk_handle,
				MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_ON, 0);
	else
		ret = mdss_dsi_clk_req_state(ctrl_pdata->clk_handle,
				MDSS_DSI_LINK_CLK, MDSS_DSI_CLK_OFF, 0);

	/* disable phy */
	mdss_rgb_phy_config(ctrl_pdata, 0);

	ret = mdss_dsi_clk_req_state(ctrl_pdata->clk_handle,
			MDSS_DSI_CORE_CLK, MDSS_DSI_CLK_OFF, 0);

panel_power_ctrl:
	mdss_rgb_panel_power_ctrl(pdata, power_state);
end:
	pr_debug("%s-:\n", __func__);
	return ret;
}

static int mdss_rgb_event_handler(struct mdss_panel_data *pdata,
		int event, void *arg)
{
	int rc = 0;
	struct mdss_rgb_data *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;
	int power_state;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	ctrl_pdata = container_of(pdata, struct mdss_rgb_data,
			panel_data);

	switch (event) {
	case MDSS_EVENT_LINK_READY:
		if (ctrl_pdata->refresh_clk_rate)
			rc = mdss_rgb_clk_refresh(ctrl_pdata);

		rc = mdss_rgb_on(pdata);
		break;

	case MDSS_EVENT_UNBLANK:
		rc = mdss_rgb_unblank(pdata);
		break;
	case MDSS_EVENT_BLANK:
		power_state = (int) (unsigned long) arg;
		rc = mdss_rgb_blank(pdata, power_state);
		break;
	case MDSS_EVENT_PANEL_OFF:
		power_state = (int) (unsigned long) arg;
		rc = mdss_rgb_blank(pdata, power_state);
		rc = mdss_rgb_off(pdata, power_state);
		break;
	}
	pr_debug("%s+: event=%d\n", __func__, event);
	return rc;
}

static int mdss_rgb_parse_gpio_params(struct platform_device *ctrl_pdev,
		struct mdss_rgb_data *rgb_data)
{
	int rc = 0;

	rgb_data->rst_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			 "qcom,platform-reset-gpio", 0);
	if (!gpio_is_valid(rgb_data->rst_gpio)) {
		pr_err("%s:%d, reset gpio not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}

	rc = gpio_request(rgb_data->rst_gpio, "disp_rst_n");
	if (rc) {
		pr_err("request reset gpio failed, rc=%d\n", rc);
		return rc;
	}

	rc = gpio_direction_output(rgb_data->rst_gpio, 1);
	if (rc) {
		pr_err("%s: unable to set dir for rst gpio\n",
				__func__);
		return rc;
	}

	pr_debug("%s: reset gpio set\n", __func__);
	return 0;
}

int rgb_panel_device_register(struct platform_device *ctrl_pdev,
	struct device_node *pan_node, struct mdss_rgb_data *rgb_data)
{
	struct mipi_panel_info *mipi;
	int rc = 0;
	struct mdss_panel_info *pinfo = &(rgb_data->panel_data.panel_info);
	struct mdss_util_intf *util;
	u64 clk_rate;

	mipi  = &(pinfo->mipi);
	util = mdss_get_util_intf();
	pinfo->type = RGB_PANEL;

	mdss_rgb_clk_div_config(rgb_data, pinfo, mipi->frame_rate);

	rgb_data->pclk_rate = mipi->dsi_pclk_rate;
	clk_rate = pinfo->clk_rate;
	do_div(clk_rate, 8U);
	rgb_data->byte_clk_rate = (u32)clk_rate;
	pr_debug("%s: pclk=%d, bclk=%d\n", __func__,
			rgb_data->pclk_rate, rgb_data->byte_clk_rate);

	rc = mdss_dsi_get_dt_vreg_data(&ctrl_pdev->dev, pan_node,
			&rgb_data->panel_power_data, DSI_PANEL_PM);
	if (rc) {
		pr_err("%s: '%s' get_dt_vreg_data failed.rc=%d\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM), rc);
		return rc;
	}

	rc = msm_dss_config_vreg(&ctrl_pdev->dev,
			rgb_data->panel_power_data.vreg_config,
			rgb_data->panel_power_data.num_vreg, 1);
	if (rc) {
		pr_err("%s: failed to init regulator, rc=%d\n",
				__func__, rc);
		return rc;
	}

	pinfo->panel_max_fps = mdss_panel_get_framerate(pinfo);
	pinfo->panel_max_vtotal = mdss_panel_get_vtotal(pinfo);

	rc = mdss_rgb_parse_gpio_params(ctrl_pdev, rgb_data);
	if (rc)
		return rc;

	rc = msm_dss_ioremap_byname(ctrl_pdev, &rgb_data->phy_io, "dsi_phy");
	if (rc) {
		pr_err("%s:%d unable to remap dsi phy resources\n",
				__func__, __LINE__);
		return rc;
	}

	rgb_data->panel_data.event_handler = mdss_rgb_event_handler;

	pinfo->is_prim_panel = true;

	pinfo->cont_splash_enabled =
		util->panel_intf_status(pinfo->pdest,
				MDSS_PANEL_INTF_RGB) ? true : false;

	pr_info("%s: Continuous splash %s\n", __func__,
			pinfo->cont_splash_enabled ? "enabled" : "disabled");

	rc = mdss_register_panel(ctrl_pdev, &(rgb_data->panel_data));


	if (rc) {
		pr_err("%s: unable to register RGB panel\n", __func__);
		return rc;
	}
	panel_debug_register_base("panel",
		rgb_data->ctrl_base, rgb_data->reg_size);

	pr_debug("%s: Panel data initialized\n", __func__);

	return 0;
}

static int mdss_rgb_remove(struct spi_device *spi, struct platform_device *pdev)
{
	if (pdev)
		mdss_rgb_res_deinit(pdev);

	return 0;
}

static int mdss_rgb_probe_pdev(struct platform_device *pdev)
{
	return mdss_rgb_probe(NULL, pdev);
}

static int mdss_rgb_remove_pdev(struct platform_device *pdev)
{
	return mdss_rgb_remove(NULL, pdev);
}

static int mdss_rgb_probe_spi(struct spi_device *spi)
{
	return mdss_rgb_probe(spi, NULL);
}

static int mdss_rgb_remove_spi(struct spi_device *spi)
{
	return mdss_rgb_remove(spi, NULL);
}

static const struct of_device_id mdss_rgb_spi_dt_match[] = {
	{.compatible = "qcom,mdss-rgb-spi"},
	{}
};
MODULE_DEVICE_TABLE(of, mdss_rgb_spi_dt_match);

static const struct of_device_id mdss_rgb_dt_match[] = {
	{.compatible = "qcom,mdss-rgb"},
	{}
};
MODULE_DEVICE_TABLE(of, mdss_rgb_dt_match);

static struct spi_driver mdss_rgb_spi_driver = {
	.probe  = mdss_rgb_probe_spi,
	.remove = mdss_rgb_remove_spi,
	.driver = {
		.name   = "mdss_rgb_spi",
		.of_match_table = mdss_rgb_spi_dt_match,
	},
};

static struct platform_driver mdss_rgb_driver = {
	.probe = mdss_rgb_probe_pdev,
	.remove = mdss_rgb_remove_pdev,
	.shutdown = NULL,
	.driver = {
		.name = "mdss_rgb",
		.of_match_table = mdss_rgb_dt_match,
	},
};

static int __init mdss_rgb_driver_init(void)
{
	int ret;

	ret = spi_register_driver(&mdss_rgb_spi_driver);
	if (ret < 0) {
		pr_err("failed to register mdss_rgb spi driver\n");
		return ret;
	}

	ret = platform_driver_register(&mdss_rgb_driver);
	if (ret)
		pr_err("failed to register mdss_rgb platform driver\n");

	return ret;
}

module_init(mdss_rgb_driver_init);

static void __exit mdss_rgb_driver_cleanup(void)
{
	platform_driver_unregister(&mdss_rgb_driver);
	spi_unregister_driver(&mdss_rgb_spi_driver);
}
module_exit(mdss_rgb_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("RGB driver");
