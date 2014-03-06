/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>

#include <linux/msm-sps.h>

#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_qpic.h"
#include "mdss_qpic_panel.h"

static u32 panel_is_on;
static u32 panel_refresh_rate;

static int (*qpic_panel_on)(struct qpic_panel_io_desc *qpic_panel_io);
static void (*qpic_panel_off)(struct qpic_panel_io_desc *qpic_panel_io);

u32 qpic_panel_get_framerate(void)
{
	return panel_refresh_rate;
}

u32 qpic_send_panel_cmd(u32 cmd, u32 *val, u32 length)
{
	u32 ret;
	u32 cmd_index;
	u32 size;
	u32 buffer[LCDC_INTERNAL_BUFFER_SIZE];
	u32 i;

	cmd_index = LCDC_EXTRACT_OP_CMD(cmd);
	size = LCDC_EXTRACT_OP_SIZE(cmd);
	if (size == INV_SIZE)
		size = length;
	/* short or pixel data commands need not conversion */
	if ((cmd == OP_WRITE_MEMORY_CONTINUE) ||
		(cmd == OP_WRITE_MEMORY_START)) {
		ret = qpic_flush_buffer(cmd_index, size, val, false);
	} else {
		if (size > LCDC_INTERNAL_BUFFER_SIZE)
			size = LCDC_INTERNAL_BUFFER_SIZE;
		/* correcting for encoding issues */
		for (i = 0; i < size; i += sizeof(u32)) {
			buffer[i] = (val[(i>>2)] >> 0) & 0xff;
			buffer[i+1] = (val[(i>>2)] >> 8) & 0xff;
			buffer[i+2] = (val[(i>>2)] >> 16) & 0xff;
			buffer[i+3] = (val[(i>>2)] >> 24) & 0xff;
		}
		ret = qpic_flush_buffer(cmd_index,
				size * sizeof(u32), buffer, true);
	}
	return ret;
}

u32 qpic_panel_set_cmd_only(u32 command)
{
	u32 param;
	return qpic_send_panel_cmd(command, &param, 0);
}

/* write a frame of pixels to a MIPI screen */
u32 qpic_send_frame(u32 x_start,
				u32 y_start,
				u32 x_end,
				u32 y_end,
				u32 *data,
				u32 total_bytes)
{
	u32 param;
	u32 status;
	u32 start_0_7;
	u32 end_0_7;
	u32 start_8_15;
	u32 end_8_15;

	/* convert to 16 bit representation */
	x_start = x_start & 0xffff;
	y_start = y_start & 0xffff;
	x_end = x_end & 0xffff;
	y_end = y_end & 0xffff;

	/* set column/page */
	start_0_7 = x_start & 0xff;
	end_0_7 = x_end & 0xff;
	start_8_15 = (x_start >> 8) & 0xff;
	end_8_15 = (x_end >> 8) & 0xff;
	param = (start_8_15 << 0) | (start_0_7 << 8) |
		(end_8_15 << 16) | (end_0_7 << 24U);
	status = qpic_send_panel_cmd(OP_SET_COLUMN_ADDRESS, &param, 0);
	if (status) {
		pr_err("Failed to set column address");
		return status;
	}

	start_0_7 = y_start & 0xff;
	end_0_7 = y_end & 0xff;
	start_8_15 = (y_start >> 8) & 0xff;
	end_8_15 = (y_end >> 8) & 0xff;
	param = (start_8_15 << 0) | (start_0_7 << 8) |
		(end_8_15 << 16) | (end_0_7 << 24U);
	status = qpic_send_panel_cmd(OP_SET_PAGE_ADDRESS, &param, 0);
	if (status) {
		pr_err("Failed to set page address");
		return status;
	}

	status = qpic_send_panel_cmd(OP_WRITE_MEMORY_START,
		&(data[0]), total_bytes);
	if (status) {
		pr_err("Failed to start memory write");
		return status;
	}
	return 0;
}

int mdss_qpic_panel_on(struct mdss_panel_data *pdata,
	struct qpic_panel_io_desc *panel_io)
{
	int rc = 0;

	if (panel_is_on)
		return 0;
	mdss_qpic_init();

	if (qpic_panel_on)
		rc = qpic_panel_on(panel_io);
	if (rc)
		return rc;
	panel_is_on = true;
	return 0;
}

int mdss_qpic_panel_off(struct mdss_panel_data *pdata,
	struct qpic_panel_io_desc *panel_io)
{
	if (qpic_panel_off)
		qpic_panel_off(panel_io);
	panel_is_on = false;
	return 0;
}


int mdss_qpic_panel_io_init(struct platform_device *pdev,
	struct qpic_panel_io_desc *qpic_panel_io)
{
	struct device_node *np = pdev->dev.of_node;
	int rst_gpio, cs_gpio, te_gpio, ad8_gpio, bl_gpio;
	struct regulator *vdd_vreg;
	struct regulator *avdd_vreg;

	rst_gpio = of_get_named_gpio(np, "qcom,rst-gpio", 0);
	cs_gpio = of_get_named_gpio(np, "qcom,cs-gpio", 0);
	ad8_gpio = of_get_named_gpio(np, "qcom,ad8-gpio", 0);
	te_gpio = of_get_named_gpio(np, "qcom,te-gpio", 0);
	bl_gpio = of_get_named_gpio(np, "qcom,bl-gpio", 0);

	if (!gpio_is_valid(rst_gpio))
		pr_err("%s: reset gpio not specified\n" , __func__);
	else
		qpic_panel_io->rst_gpio = rst_gpio;

	if (!gpio_is_valid(cs_gpio))
		pr_err("%s: cs gpio not specified\n", __func__);
	else
		qpic_panel_io->cs_gpio = cs_gpio;

	if (!gpio_is_valid(ad8_gpio))
		pr_err("%s: ad8 gpio not specified\n", __func__);
	else
		qpic_panel_io->ad8_gpio = ad8_gpio;

	if (!gpio_is_valid(te_gpio))
		pr_err("%s: te gpio not specified\n", __func__);
	else
		qpic_panel_io->te_gpio = te_gpio;

	if (!gpio_is_valid(bl_gpio))
		pr_err("%s: te gpio not specified\n", __func__);
	else
		qpic_panel_io->bl_gpio = bl_gpio;

	vdd_vreg = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(vdd_vreg))
		pr_err("%s could not get vdd,", __func__);
	else
		qpic_panel_io->vdd_vreg = vdd_vreg;

	avdd_vreg = devm_regulator_get(&pdev->dev, "avdd");
	if (IS_ERR(avdd_vreg))
		pr_err("%s could not get avdd,", __func__);
	else
		qpic_panel_io->avdd_vreg = avdd_vreg;

	return 0;
}

static int mdss_panel_parse_dt(struct platform_device *pdev,
			       struct mdss_panel_data *panel_data)
{
	struct device_node *np = pdev->dev.of_node;
	u32 res[6], tmp;
	int rc;

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-res", res, 2);
	if (rc) {
		pr_err("%s:%d, panel resolution not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.xres = (!rc ? res[0] : 240);
	panel_data->panel_info.yres = (!rc ? res[1] : 320);
	rc = of_property_read_u32(np, "qcom,mdss-pan-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, panel bpp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.bpp = (!rc ? tmp : 24);
	of_property_read_u32(np, "qcom,refresh_rate", &panel_refresh_rate);

	panel_data->panel_info.type = EBI2_PANEL;
	panel_data->panel_info.pdest = DISPLAY_1;

	return rc;
}

static int mdss_qpic_panel_probe(struct platform_device *pdev)
{
	int rc = 0;
	static struct mdss_panel_data vendor_pdata;
	static const char *panel_name;

	pr_debug("%s:%d, debug info id=%d", __func__, __LINE__, pdev->id);
	if (!pdev->dev.of_node)
		return -ENODEV;

	panel_name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!panel_name)
		pr_info("%s:%d, panel name not specified\n",
						__func__, __LINE__);
	else
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);

	/* select panel according to label */
	qpic_panel_on = ili9341_on;
	qpic_panel_off = ili9341_off;

	rc = mdss_panel_parse_dt(pdev, &vendor_pdata);
	if (rc)
		return rc;
	rc = qpic_register_panel(&vendor_pdata);
	if (rc)
		return rc;

	return 0;
}

static const struct of_device_id mdss_qpic_panel_match[] = {
	{.compatible = "qcom,mdss-qpic-panel"},
	{}
};

static struct platform_driver this_driver = {
	.probe  = mdss_qpic_panel_probe,
	.driver = {
		.name = "qpic_panel",
		.of_match_table = mdss_qpic_panel_match,
	},
};

static int __init mdss_qpic_panel_init(void)
{
	return platform_driver_register(&this_driver);
}
MODULE_DEVICE_TABLE(of, mdss_qpic_panel_match);
module_init(mdss_qpic_panel_init);
