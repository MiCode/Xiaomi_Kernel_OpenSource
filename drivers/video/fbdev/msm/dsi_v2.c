/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/iopoll.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include "dsi_v2.h"

static struct dsi_interface dsi_intf;
static struct dsi_buf dsi_panel_tx_buf;

static int dsi_off(struct mdss_panel_data *pdata)
{
	int rc = 0;

	pr_debug("turn off dsi controller\n");
	if (dsi_intf.off)
		rc = dsi_intf.off(pdata);

	if (rc) {
		pr_err("mdss_dsi_off DSI failed %d\n", rc);
		return rc;
	}
	return rc;
}

static int dsi_on(struct mdss_panel_data *pdata)
{
	int rc = 0;

	pr_debug("dsi_on DSI controller on\n");
	if (dsi_intf.on)
		rc = dsi_intf.on(pdata);

	if (rc) {
		pr_err("mdss_dsi_on DSI failed %d\n", rc);
		return rc;
	}
	return rc;
}

static int dsi_panel_handler(struct mdss_panel_data *pdata, int enable)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_debug("dsi_panel_handler enable=%d\n", enable);
	if (!pdata)
		return -ENODEV;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (enable) {
		mdss_dsi_panel_reset(pdata, 1);

		rc = dsi_cmds_tx_v2(pdata, &dsi_panel_tx_buf,
					ctrl_pdata->on_cmds.cmds,
					ctrl_pdata->on_cmds.cmd_cnt);

		if (rc)
			pr_err("dsi_panel_handler panel on failed %d\n", rc);
	} else {
		if (dsi_intf.op_mode_config)
			dsi_intf.op_mode_config(DSI_CMD_MODE, pdata);

		dsi_cmds_tx_v2(pdata, &dsi_panel_tx_buf,
					ctrl_pdata->off_cmds.cmds,
					ctrl_pdata->off_cmds.cmd_cnt);

		mdss_dsi_panel_reset(pdata, 0);
	}
	return rc;
}

static int dsi_splash_on(struct mdss_panel_data *pdata)
{
	int rc = 0;

	pr_debug("%s:\n", __func__);

	if (dsi_intf.cont_on)
		rc = dsi_intf.cont_on(pdata);

	if (rc) {
		pr_err("mdss_dsi_on DSI failed %d\n", rc);
		return rc;
	}
	return rc;
}

static int dsi_event_handler(struct mdss_panel_data *pdata,
				int event, void *arg)
{
	int rc = 0;

	if (!pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -ENODEV;
	}

	switch (event) {
	case MDSS_EVENT_UNBLANK:
		rc = dsi_on(pdata);
		break;
	case MDSS_EVENT_BLANK:
		rc = dsi_off(pdata);
		break;
	case MDSS_EVENT_PANEL_ON:
		rc = dsi_panel_handler(pdata, 1);
		break;
	case MDSS_EVENT_PANEL_OFF:
		rc = dsi_panel_handler(pdata, 0);
		break;
	case MDSS_EVENT_CONT_SPLASH_BEGIN:
		rc = dsi_splash_on(pdata);
		break;
	default:
		pr_debug("%s: unhandled event=%d\n", __func__, event);
		break;
	}
	return rc;
}

static int dsi_parse_gpio(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct device_node *np = pdev->dev.of_node;
	int rc = 0;

	ctrl_pdata->disp_en_gpio = of_get_named_gpio(np,
		"qcom,platform-enable-gpio", 0);

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_err("%s:%d, Disp_en gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->disp_en_gpio, "disp_enable");
		if (rc) {
			pr_err("request reset gpio failed, rc=%d\n",
			       rc);
			gpio_free(ctrl_pdata->disp_en_gpio);
			return -ENODEV;
		}
	}

	if (ctrl_pdata->panel_data.panel_info.mipi.mode == DSI_CMD_MODE) {
		ctrl_pdata->disp_te_gpio = of_get_named_gpio(np,
						"qcom,platform-te-gpio", 0);
		if (!gpio_is_valid(ctrl_pdata->disp_te_gpio)) {
			pr_err("%s:%d, Disp_te gpio not specified\n",
							__func__, __LINE__);
		} else {
			rc = gpio_request(ctrl_pdata->disp_te_gpio, "disp_te");
			if (rc) {
				pr_err("request TE gpio failed, rc=%d\n",
								       rc);
				gpio_free(ctrl_pdata->disp_te_gpio);
				return -ENODEV;
			}
			rc = gpio_tlmm_config(GPIO_CFG(
					ctrl_pdata->disp_te_gpio, 1,
					GPIO_CFG_INPUT,
					GPIO_CFG_PULL_DOWN,
					GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);

			if (rc) {
				pr_err("%s: unable to config tlmm = %d\n",
					__func__, ctrl_pdata->disp_te_gpio);
				gpio_free(ctrl_pdata->disp_te_gpio);
				return -ENODEV;
			}

			rc = gpio_direction_input(ctrl_pdata->disp_te_gpio);
			if (rc) {
				pr_err("set_direction for disp_en gpio failed, rc=%d\n",
								       rc);
				gpio_free(ctrl_pdata->disp_te_gpio);
				if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
					gpio_free(ctrl_pdata->disp_en_gpio);
				return -ENODEV;
			}
			pr_debug("%s: te_gpio=%d\n", __func__,
					ctrl_pdata->disp_te_gpio);
		}
	}

	ctrl_pdata->rst_gpio = of_get_named_gpio(np,
					"qcom,platform-reset-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_err("%s:%d, reset gpio not specified\n",
						__func__, __LINE__);
	} else {
		rc = gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n");
		if (rc) {
			pr_err("request reset gpio failed, rc=%d\n",
				rc);
			gpio_free(ctrl_pdata->rst_gpio);
			if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
				gpio_free(ctrl_pdata->disp_en_gpio);
			if (gpio_is_valid(ctrl_pdata->disp_te_gpio))
				gpio_free(ctrl_pdata->disp_te_gpio);
			return -ENODEV;
		}
	}

	if (ctrl_pdata->panel_data.panel_info.mode_gpio_state !=
						MODE_GPIO_NOT_VALID) {
		ctrl_pdata->mode_gpio = of_get_named_gpio(np,
						"qcom,platform-mode-gpio", 0);
		if (!gpio_is_valid(ctrl_pdata->mode_gpio)) {
			pr_info("%s:%d, reset gpio not specified\n",
							__func__, __LINE__);
		} else {
			rc = gpio_request(ctrl_pdata->mode_gpio, "panel_mode");
			if (rc) {
				pr_err("request panel mode gpio failed,rc=%d\n",
									rc);
				gpio_free(ctrl_pdata->mode_gpio);
				if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
					gpio_free(ctrl_pdata->disp_en_gpio);
				if (gpio_is_valid(ctrl_pdata->rst_gpio))
					gpio_free(ctrl_pdata->rst_gpio);
				if (gpio_is_valid(ctrl_pdata->disp_te_gpio))
					gpio_free(ctrl_pdata->disp_te_gpio);
				return -ENODEV;
			}
		}
	}
	return 0;
}

void dsi_ctrl_config_deinit(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct dss_module_power *module_power = &(ctrl_pdata->power_data);
	if (!module_power) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	if (module_power->vreg_config) {
		devm_kfree(&(pdev->dev), module_power->vreg_config);
		module_power->vreg_config = NULL;
	}
	module_power->num_vreg = 0;

	if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
		gpio_free(ctrl_pdata->disp_en_gpio);
	if (gpio_is_valid(ctrl_pdata->rst_gpio))
		gpio_free(ctrl_pdata->rst_gpio);
	if (gpio_is_valid(ctrl_pdata->disp_te_gpio))
		gpio_free(ctrl_pdata->disp_te_gpio);
	if (gpio_is_valid(ctrl_pdata->mode_gpio))
		gpio_free(ctrl_pdata->mode_gpio);
}

static int dsi_parse_vreg(struct device *dev, struct dss_module_power *mp)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *supply_node = NULL;
	struct device_node *np = NULL;

	if (!dev || !mp) {
		pr_err("%s: invalid input\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	np = dev->of_node;

	mp->num_vreg = 0;
	for_each_child_of_node(np, supply_node) {
		if (!strncmp(supply_node->name, "qcom,platform-supply-entry",
					strlen("qcom,platform-supply-entry")))
			++mp->num_vreg;
	}
	if (mp->num_vreg == 0) {
		pr_err("%s: no vreg\n", __func__);
		goto novreg;
	} else {
		pr_debug("%s: vreg found. count=%d\n", __func__, mp->num_vreg);
	}

	mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
		mp->num_vreg, GFP_KERNEL);
	if (!mp->vreg_config) {
		pr_err("%s: can't alloc vreg mem\n", __func__);
		rc = -ENOMEM;
		goto error;
	}

	for_each_child_of_node(np, supply_node) {
		if (!strncmp(supply_node->name, "qcom,platform-supply-entry",
					strlen("qcom,platform-supply-entry"))) {
			const char *st = NULL;
			/* vreg-name */
			rc = of_property_read_string(supply_node,
				"qcom,supply-name", &st);
			if (rc) {
				pr_err("%s: error reading name. rc=%d\n",
					__func__, rc);
				goto error;
			}
			strlcpy(mp->vreg_config[i].vreg_name, st,
				sizeof(mp->vreg_config[i].vreg_name));
			/* vreg-min-voltage */
			rc = of_property_read_u32(supply_node,
				"qcom,supply-min-voltage", &tmp);
			if (rc) {
				pr_err("%s: error reading min volt. rc=%d\n",
					__func__, rc);
				goto error;
			}
			mp->vreg_config[i].min_voltage = tmp;

			/* vreg-max-voltage */
			rc = of_property_read_u32(supply_node,
				"qcom,supply-max-voltage", &tmp);
			if (rc) {
				pr_err("%s: error reading max volt. rc=%d\n",
					__func__, rc);
				goto error;
			}
			mp->vreg_config[i].max_voltage = tmp;

			/* enable-load */
			rc = of_property_read_u32(supply_node,
				"qcom,supply-enable-load", &tmp);
			if (rc) {
				pr_err("%s: error reading enable load. rc=%d\n",
					__func__, rc);
				goto error;
			}
			mp->vreg_config[i].enable_load = tmp;

			/* disable-load */
			rc = of_property_read_u32(supply_node,
				"qcom,supply-disable-load", &tmp);
			if (rc) {
				pr_err("%s: error reading disable load. rc=%d\n",
					__func__, rc);
				goto error;
			}
			mp->vreg_config[i].disable_load = tmp;

			/* pre-sleep */
			rc = of_property_read_u32(supply_node,
				"qcom,supply-pre-on-sleep", &tmp);
			if (rc) {
				pr_debug("%s: error reading supply pre sleep value. rc=%d\n",
					__func__, rc);
			}
			mp->vreg_config[i].pre_on_sleep = (!rc ? tmp : 0);

			rc = of_property_read_u32(supply_node,
				"qcom,supply-pre-off-sleep", &tmp);
			if (rc) {
				pr_debug("%s: error reading supply pre sleep value. rc=%d\n",
					__func__, rc);
			}
			mp->vreg_config[i].pre_off_sleep = (!rc ? tmp : 0);

			/* post-sleep */
			rc = of_property_read_u32(supply_node,
				"qcom,supply-post-on-sleep", &tmp);
			if (rc) {
				pr_debug("%s: error reading supply post sleep value. rc=%d\n",
					__func__, rc);
			}
			mp->vreg_config[i].post_on_sleep = (!rc ? tmp : 0);

			rc = of_property_read_u32(supply_node,
				"qcom,supply-post-off-sleep", &tmp);
			if (rc) {
				pr_debug("%s: error reading supply post sleep value. rc=%d\n",
					__func__, rc);
			}
			mp->vreg_config[i].post_off_sleep = (!rc ? tmp : 0);

			pr_debug("%s: %s min=%d, max=%d, enable=%d, disable=%d, preonsleep=%d, postonsleep=%d, preoffsleep=%d, postoffsleep=%d\n",
				__func__,
				mp->vreg_config[i].vreg_name,
				mp->vreg_config[i].min_voltage,
				mp->vreg_config[i].max_voltage,
				mp->vreg_config[i].enable_load,
				mp->vreg_config[i].disable_load,
				mp->vreg_config[i].pre_on_sleep,
				mp->vreg_config[i].post_on_sleep,
				mp->vreg_config[i].pre_off_sleep,
				mp->vreg_config[i].post_off_sleep
				);
			++i;
		}
	}

	return 0;

error:
	if (mp->vreg_config) {
		devm_kfree(dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
novreg:
	mp->num_vreg = 0;

	return rc;
}

static int dsi_parse_phy(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct device_node *np = pdev->dev.of_node;
	int i, len;
	const char *data;
	struct mdss_dsi_phy_ctrl *phy_db
		= &(ctrl_pdata->panel_data.panel_info.mipi.dsi_phy_db);

	data = of_get_property(np, "qcom,platform-regulator-settings", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy regulator settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_db->regulator[i] = data[i];

	data = of_get_property(np, "qcom,platform-strength-ctrl", &len);
	if ((!data) || (len != 2)) {
		pr_err("%s:%d, Unable to read Phy Strength ctrl settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	phy_db->strength[0] = data[0];
	phy_db->strength[1] = data[1];

	data = of_get_property(np, "qcom,platform-bist-ctrl", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy Bist Ctrl settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_db->bistctrl[i] = data[i];

	data = of_get_property(np, "qcom,platform-lane-config", &len);
	if ((!data) || (len != 30)) {
		pr_err("%s:%d, Unable to read Phy lane configure settings",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		phy_db->lanecfg[i] = data[i];

	return 0;
}

int dsi_ctrl_config_init(struct platform_device *pdev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc;

	rc = dsi_parse_vreg(&pdev->dev, &ctrl_pdata->power_data);
	if (rc) {
		pr_err("%s:%d unable to get the regulator resources",
			__func__, __LINE__);
		return rc;
	}

	rc = dsi_parse_gpio(pdev, ctrl_pdata);
	if (rc) {
		pr_err("fail to parse panel GPIOs\n");
		return rc;
	}

	rc = dsi_parse_phy(pdev, ctrl_pdata);
	if (rc) {
		pr_err("fail to parse DSI PHY settings\n");
		return rc;
	}

	return 0;
}
int dsi_panel_device_register_v2(struct platform_device *dev,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mipi_panel_info *mipi;
	int rc;
	u8 lanes = 0, bpp;
	u32 h_period, v_period;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	h_period = ((pinfo->lcdc.h_pulse_width)
			+ (pinfo->lcdc.h_back_porch)
			+ (pinfo->xres)
			+ (pinfo->lcdc.h_front_porch));

	v_period = ((pinfo->lcdc.v_pulse_width)
			+ (pinfo->lcdc.v_back_porch)
			+ (pinfo->yres)
			+ (pinfo->lcdc.v_front_porch));

	mipi  = &pinfo->mipi;

	pinfo->type =
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
		bpp = 3; /* Default format set to RGB888 */

	if (pinfo->type == MIPI_VIDEO_PANEL &&
		!pinfo->clk_rate) {
		h_period += pinfo->lcdc.xres_pad;
		v_period += pinfo->lcdc.yres_pad;

		if (lanes > 0) {
			pinfo->clk_rate =
			((h_period * v_period * (mipi->frame_rate) * bpp * 8)
			   / lanes);
		} else {
			pr_err("%s: forcing mdss_dsi lanes to 1\n", __func__);
			pinfo->clk_rate =
				(h_period * v_period
					 * (mipi->frame_rate) * bpp * 8);
		}
	}

	ctrl_pdata->panel_data.event_handler = dsi_event_handler;

	rc = dsi_buf_alloc(&dsi_panel_tx_buf,
				ALIGN(DSI_BUF_SIZE,
				SZ_4K));
	if (rc)
		return rc;

	/*
	 * register in mdp driver
	 */
	rc = mdss_register_panel(dev, &(ctrl_pdata->panel_data));
	if (rc) {
		dev_err(&dev->dev, "unable to register MIPI DSI panel\n");
		kfree(dsi_panel_tx_buf.start);
		return rc;
	}

	pr_debug("%s: Panal data initialized\n", __func__);
	return 0;
}

void dsi_register_interface(struct dsi_interface *intf)
{
	dsi_intf = *intf;
}

int dsi_cmds_tx_v2(struct mdss_panel_data *pdata,
			struct dsi_buf *tp, struct dsi_cmd_desc *cmds,
			int cnt)
{
	int rc = 0;

	if (!dsi_intf.tx)
		return -EINVAL;

	rc = dsi_intf.tx(pdata, tp, cmds, cnt);
	return rc;
}

int dsi_cmds_rx_v2(struct mdss_panel_data *pdata,
			struct dsi_buf *tp, struct dsi_buf *rp,
			struct dsi_cmd_desc *cmds, int rlen)
{
	int rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (!dsi_intf.rx)
		return -EINVAL;

	rc = dsi_intf.rx(pdata, tp, rp, cmds, rlen);
	return rc;
}

static char *dsi_buf_reserve(struct dsi_buf *dp, int len)
{
	dp->data += len;
	return dp->data;
}


static char *dsi_buf_push(struct dsi_buf *dp, int len)
{
	dp->data -= len;
	dp->len += len;
	return dp->data;
}

static char *dsi_buf_reserve_hdr(struct dsi_buf *dp, int hlen)
{
	dp->hdr = (u32 *)dp->data;
	return dsi_buf_reserve(dp, hlen);
}

char *dsi_buf_init(struct dsi_buf *dp)
{
	int off;

	dp->data = dp->start;
	off = (int)dp->data;
	/* 8 byte align */
	off &= 0x07;
	if (off)
		off = 8 - off;
	dp->data += off;
	dp->len = 0;
	return dp->data;
}

int dsi_buf_alloc(struct dsi_buf *dp, int size)
{
	dp->start = kmalloc(size, GFP_KERNEL);
	if (dp->start == NULL) {
		pr_err("%s:%u\n", __func__, __LINE__);
		return -ENOMEM;
	}

	dp->end = dp->start + size;
	dp->size = size;

	if ((int)dp->start & 0x07) {
		pr_err("%s: buf NOT 8 bytes aligned\n", __func__);
		return -EINVAL;
	}

	dp->data = dp->start;
	dp->len = 0;
	return 0;
}

/*
 * mipi dsi generic long write
 */
static int dsi_generic_lwrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	char *bp;
	u32 *hp;
	int i, len;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	bp = dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);

	/* fill up payload */
	if (cm->payload) {
		len = dchdr->dlen;
		len += 3;
		len &= ~0x03; /* multipled by 4 */
		for (i = 0; i < dchdr->dlen; i++)
			*bp++ = cm->payload[i];

		/* append 0xff to the end */
		for (; i < len; i++)
			*bp++ = 0xff;

		dp->len += len;
	}

	/* fill up header */
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(dchdr->dlen);
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_DTYPE(DTYPE_GEN_LWRITE);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len;
}

/*
 * mipi dsi generic short write with 0, 1 2 parameters
 */
static int dsi_generic_swrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	int len;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	if (dchdr->dlen && cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return 0;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	len = (dchdr->dlen > 2) ? 2 : dchdr->dlen;

	if (len == 1) {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_WRITE1);
		*hp |= DSI_HDR_DATA1(cm->payload[0]);
		*hp |= DSI_HDR_DATA2(0);
	} else if (len == 2) {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_WRITE2);
		*hp |= DSI_HDR_DATA1(cm->payload[0]);
		*hp |= DSI_HDR_DATA2(cm->payload[1]);
	} else {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_WRITE);
		*hp |= DSI_HDR_DATA1(0);
		*hp |= DSI_HDR_DATA2(0);
	}

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len;
}

/*
 * mipi dsi gerneric read with 0, 1 2 parameters
 */
static int dsi_generic_read(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	int len;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	if (dchdr->dlen && cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return 0;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_BTA;
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	len = (dchdr->dlen > 2) ? 2 : dchdr->dlen;

	if (len == 1) {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_READ1);
		*hp |= DSI_HDR_DATA1(cm->payload[0]);
		*hp |= DSI_HDR_DATA2(0);
	} else if (len == 2) {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_READ2);
		*hp |= DSI_HDR_DATA1(cm->payload[0]);
		*hp |= DSI_HDR_DATA2(cm->payload[1]);
	} else {
		*hp |= DSI_HDR_DTYPE(DTYPE_GEN_READ);
		*hp |= DSI_HDR_DATA1(0);
		*hp |= DSI_HDR_DATA2(0);
	}

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return dp->len;
}

/*
 * mipi dsi dcs long write
 */
static int dsi_dcs_lwrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	char *bp;
	u32 *hp;
	int i, len;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	bp = dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);

	/*
	 * fill up payload
	 * dcs command byte (first byte) followed by payload
	 */
	if (cm->payload) {
		len = dchdr->dlen;
		len += 3;
		len &= ~0x03; /* multipled by 4 */
		for (i = 0; i < dchdr->dlen; i++)
			*bp++ = cm->payload[i];

		/* append 0xff to the end */
		for (; i < len; i++)
			*bp++ = 0xff;

		dp->len += len;
	}

	/* fill up header */
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(dchdr->dlen);
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_LWRITE);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len;
}

/*
 * mipi dsi dcs short write with 0 parameters
 */
static int dsi_dcs_swrite(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	int len;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	if (cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return -EINVAL;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	if (dchdr->ack)
		*hp |= DSI_HDR_BTA;
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	len = (dchdr->dlen > 1) ? 1 : dchdr->dlen;

	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_WRITE);
	*hp |= DSI_HDR_DATA1(cm->payload[0]); /* dcs command byte */
	*hp |= DSI_HDR_DATA2(0);

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);
	return dp->len;
}

/*
 * mipi dsi dcs short write with 1 parameters
 */
static int dsi_dcs_swrite1(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	if (dchdr->dlen < 2 || cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return -EINVAL;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	if (dchdr->ack)
		*hp |= DSI_HDR_BTA;
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_WRITE1);
	*hp |= DSI_HDR_DATA1(cm->payload[0]); /* dcs comamnd byte */
	*hp |= DSI_HDR_DATA2(cm->payload[1]); /* parameter */

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len;
}

/*
 * mipi dsi dcs read with 0 parameters
 */
static int dsi_dcs_read(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	if (cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return -EINVAL;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_BTA;
	*hp |= DSI_HDR_DTYPE(DTYPE_DCS_READ);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	*hp |= DSI_HDR_DATA1(cm->payload[0]); /* dcs command byte */
	*hp |= DSI_HDR_DATA2(0);

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_cm_on(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_CM_ON);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_cm_off(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_CM_OFF);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_peripheral_on(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_PERIPHERAL_ON);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_peripheral_off(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_PERIPHERAL_OFF);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_set_max_pktsize(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	if (cm->payload == 0) {
		pr_err("%s: NO payload error\n", __func__);
		return 0;
	}

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_MAX_PKTSIZE);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	*hp |= DSI_HDR_DATA1(cm->payload[0]);
	*hp |= DSI_HDR_DATA2(cm->payload[1]);

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_null_pkt(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(dchdr->dlen);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_NULL_PKT);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

static int dsi_blank_pkt(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	u32 *hp;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	dsi_buf_reserve_hdr(dp, DSI_HOST_HDR_SIZE);
	hp = dp->hdr;
	*hp = 0;
	*hp = DSI_HDR_WC(dchdr->dlen);
	*hp |= DSI_HDR_LONG_PKT;
	*hp |= DSI_HDR_VC(dchdr->vc);
	*hp |= DSI_HDR_DTYPE(DTYPE_BLANK_PKT);
	if (dchdr->last)
		*hp |= DSI_HDR_LAST;

	dsi_buf_push(dp, DSI_HOST_HDR_SIZE);

	return dp->len; /* 4 bytes */
}

/*
 * prepare cmd buffer to be txed
 */
int dsi_cmd_dma_add(struct dsi_buf *dp, struct dsi_cmd_desc *cm)
{
	int len = 0;
	struct dsi_ctrl_hdr *dchdr = &cm->dchdr;

	switch (dchdr->dtype) {
	case DTYPE_GEN_WRITE:
	case DTYPE_GEN_WRITE1:
	case DTYPE_GEN_WRITE2:
		len = dsi_generic_swrite(dp, cm);
		break;
	case DTYPE_GEN_LWRITE:
		len = dsi_generic_lwrite(dp, cm);
		break;
	case DTYPE_GEN_READ:
	case DTYPE_GEN_READ1:
	case DTYPE_GEN_READ2:
		len = dsi_generic_read(dp, cm);
		break;
	case DTYPE_DCS_LWRITE:
		len = dsi_dcs_lwrite(dp, cm);
		break;
	case DTYPE_DCS_WRITE:
		len = dsi_dcs_swrite(dp, cm);
		break;
	case DTYPE_DCS_WRITE1:
		len = dsi_dcs_swrite1(dp, cm);
		break;
	case DTYPE_DCS_READ:
		len = dsi_dcs_read(dp, cm);
		break;
	case DTYPE_MAX_PKTSIZE:
		len = dsi_set_max_pktsize(dp, cm);
		break;
	case DTYPE_NULL_PKT:
		len = dsi_null_pkt(dp, cm);
		break;
	case DTYPE_BLANK_PKT:
		len = dsi_blank_pkt(dp, cm);
		break;
	case DTYPE_CM_ON:
		len = dsi_cm_on(dp, cm);
		break;
	case DTYPE_CM_OFF:
		len = dsi_cm_off(dp, cm);
		break;
	case DTYPE_PERIPHERAL_ON:
		len = dsi_peripheral_on(dp, cm);
		break;
	case DTYPE_PERIPHERAL_OFF:
		len = dsi_peripheral_off(dp, cm);
		break;
	default:
		pr_debug("%s: dtype=%x NOT supported\n",
					__func__, dchdr->dtype);
		break;

	}

	return len;
}

/*
 * mdss_dsi_short_read1_resp: 1 parameter
 */
int dsi_short_read1_resp(struct dsi_buf *rp)
{
	/* strip out dcs type */
	rp->data++;
	rp->len = 1;
	return rp->len;
}

/*
 * mdss_dsi_short_read2_resp: 2 parameter
 */
int dsi_short_read2_resp(struct dsi_buf *rp)
{
	/* strip out dcs type */
	rp->data++;
	rp->len = 2;
	return rp->len;
}

int dsi_long_read_resp(struct dsi_buf *rp)
{
	short len;

	len = rp->data[2];
	len <<= 8;
	len |= rp->data[1];
	/* strip out dcs header */
	rp->data += 4;
	rp->len -= 4;
	/* strip out 2 bytes of checksum */
	rp->len -= 2;
	return len;
}
