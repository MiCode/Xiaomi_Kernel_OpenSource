/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/memory.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>

#include <mach/hardware.h>
#include <asm/system.h>
#include <asm/mach-types.h>

#include "mdss_qpic.h"
#include "mdss_qpic_panel.h"

enum {
	OP_ILI9341_TEARING_EFFECT_LINE_ON = OP_SIZE_PAIR(0x35, 1),
	OP_ILI9341_INTERFACE_CONTROL = OP_SIZE_PAIR(0xf6, 3),
	OP_ILI9341_WRITE_CTRL_DISPLAY  = OP_SIZE_PAIR(0x53, 1),
	OP_ILI9341_POWER_CONTROL_A  = OP_SIZE_PAIR(0xcb, 5),
	OP_ILI9341_POWER_CONTROL_B  = OP_SIZE_PAIR(0xcf, 3),
	OP_ILI9341_DRIVER_TIMING_CONTROL_A  = OP_SIZE_PAIR(0xe8, 3),
	OP_ILI9341_DRIVER_TIMING_CONTROL_B  = OP_SIZE_PAIR(0xea, 3),
	OP_ILI9341_POWER_ON_SEQUENCE_CONTROL  = OP_SIZE_PAIR(0xed, 4),
	OP_ILI9341_PUMP_RATIO_CONTROL  = OP_SIZE_PAIR(0xf7, 1),
	OP_ILI9341_POWER_CONTROL_1  = OP_SIZE_PAIR(0xc0, 1),
	OP_ILI9341_POWER_CONTROL_2  = OP_SIZE_PAIR(0xc1, 1),
	OP_ILI9341_VCOM_CONTROL_1  = OP_SIZE_PAIR(0xc5, 2),
	OP_ILI9341_VCOM_CONTROL_2  = OP_SIZE_PAIR(0xc7, 1),
	OP_ILI9341_MEMORY_ACCESS_CONTROL  = OP_SIZE_PAIR(0x36, 1),
	OP_ILI9341_FRAME_RATE_CONTROL  = OP_SIZE_PAIR(0xb1, 2),
	OP_ILI9341_DISPLAY_FUNCTION_CONTROL = OP_SIZE_PAIR(0xb6, 4),
	OP_ILI9341_ENABLE_3G = OP_SIZE_PAIR(0xf2, 1),
	OP_ILI9341_COLMOD_PIXEL_FORMAT_SET = OP_SIZE_PAIR(0x3a, 1),
	OP_ILI9341_GAMMA_SET = OP_SIZE_PAIR(0x26, 1),
	OP_ILI9341_POSITIVE_GAMMA_CORRECTION = OP_SIZE_PAIR(0xe0, 15),
	OP_ILI9341_NEGATIVE_GAMMA_CORRECTION = OP_SIZE_PAIR(0xe1, 15),
	OP_ILI9341_READ_DISPLAY_ID = OP_SIZE_PAIR(0x04, 4),
	OP_ILI9341_READ_DISPLAY_POWER_MODE = OP_SIZE_PAIR(0x0a, 2),
	OP_ILI9341_READ_DISPLAY_MADCTL = OP_SIZE_PAIR(0x0b, 2),
};

static int rst_gpio;
static int cs_gpio;
static int ad8_gpio;
static int te_gpio;
struct regulator *vdd_vreg;
struct regulator *avdd_vreg;

int ili9341_init(struct platform_device *pdev,
			struct device_node *np)
{
	int rc = 0;
	rst_gpio = of_get_named_gpio(np, "qcom,rst-gpio", 0);
	cs_gpio = of_get_named_gpio(np, "qcom,cs-gpio", 0);
	ad8_gpio = of_get_named_gpio(np, "qcom,ad8-gpio", 0);
	te_gpio = of_get_named_gpio(np, "qcom,te-gpio", 0);
	if (!gpio_is_valid(rst_gpio)) {
		pr_err("%s: reset gpio not specified\n" , __func__);
		return -EINVAL;
	}
	if (!gpio_is_valid(cs_gpio)) {
		pr_err("%s: cs gpio not specified\n", __func__);
		return -EINVAL;
	}
	if (!gpio_is_valid(ad8_gpio)) {
		pr_err("%s: ad8 gpio not specified\n", __func__);
		return -EINVAL;
	}
	if (!gpio_is_valid(te_gpio)) {
		pr_err("%s: te gpio not specified\n", __func__);
		return -EINVAL;
	}
	vdd_vreg = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(vdd_vreg)) {
		pr_err("%s could not get vdd,", __func__);
		return -ENODEV;
	}
	avdd_vreg = devm_regulator_get(&pdev->dev, "avdd");
	if (IS_ERR(avdd_vreg)) {
		pr_err("%s could not get avdd,", __func__);
		return -ENODEV;
	}
	rc = regulator_set_voltage(vdd_vreg, 1800000, 1800000);
	if (rc) {
		pr_err("vdd_vreg->set_voltage failed, rc=%d\n", rc);
		return -EINVAL;
	}
	rc = regulator_set_voltage(avdd_vreg, 2700000, 2700000);
	if (rc) {
		pr_err("vdd_vreg->set_voltage failed, rc=%d\n", rc);
		return -EINVAL;
	}
	return 0;
}

static int ili9341_panel_power_on(void)
{
	int rc;
	rc = regulator_enable(vdd_vreg);
	if (rc) {
		pr_err("enable vdd failed, rc=%d\n", rc);
		return -ENODEV;
	}
	rc = regulator_enable(avdd_vreg);
	if (rc) {
		pr_err("enable avdd failed, rc=%d\n", rc);
		return -ENODEV;
	}

	if (gpio_request(rst_gpio, "disp_rst_n")) {
		pr_err("%s request reset gpio failed\n", __func__);
		return -EINVAL;
	}
	if (gpio_request(cs_gpio, "disp_cs_n")) {
		gpio_free(rst_gpio);
		pr_err("%s request cs gpio failed\n", __func__);
		return -EINVAL;
	}

	if (gpio_request(ad8_gpio, "disp_ad8_n")) {
		gpio_free(cs_gpio);
		gpio_free(rst_gpio);
		pr_err("%s request ad8 gpio failed\n", __func__);
		return -EINVAL;
	}
	if (gpio_request(te_gpio, "disp_te_n")) {
		gpio_free(ad8_gpio);
		gpio_free(cs_gpio);
		gpio_free(rst_gpio);
		pr_err("%s request te gpio failed\n", __func__);
		return -EINVAL;
	}
	/* wait for 20 ms after enable gpio as suggested by hw */
	msleep(20);
	return 0;
}

static void ili9341_panel_power_off(void)
{
	gpio_free(ad8_gpio);
	gpio_free(cs_gpio);
	gpio_free(rst_gpio);
	gpio_free(te_gpio);
	regulator_disable(vdd_vreg);
	regulator_disable(avdd_vreg);
}

int ili9341_on(void)
{
	u32 param[20];
	int ret;
	ret = ili9341_panel_power_on();
	if (ret)
		return ret;
	qpic_panel_set_cmd_only(OP_SOFT_RESET);
	/* wait for 120 ms after reset as panel spec suggests */
	msleep(120);
	qpic_panel_set_cmd_only(OP_SET_DISPLAY_OFF);
	/* wait for 20 ms after disply off */
	msleep(20);

	/* set memory access control */
	param[0] = ((0x48)<<0) | ((0x00)<<8) | ((0x00)<<16) | ((0x00U)<<24U);
	qpic_send_panel_cmd(OP_ILI9341_MEMORY_ACCESS_CONTROL, param, 0);
	/* wait for 20 ms after command sent as panel spec suggests */
	msleep(20);

	/* set COLMOD: Pixel Format Set */
	param[0] = ((0x66)<<0) | ((0x00)<<8) | ((0x00)<<16) | ((0x00U)<<24U);
	qpic_send_panel_cmd(OP_ILI9341_COLMOD_PIXEL_FORMAT_SET, param, 0);
	/* wait for 20 ms after command sent as panel spec suggests */
	msleep(20);

	/* set interface */
	param[0] = ((0x01)<<0) | ((0x00)<<8) | ((0x00)<<16) | ((0x00U)<<24U);
	qpic_send_panel_cmd(OP_ILI9341_INTERFACE_CONTROL, &param[0], 0);
	/* wait for 20 ms after command sent */
	msleep(20);

	/* exit sleep mode */
	qpic_panel_set_cmd_only(OP_EXIT_SLEEP_MODE);
	/* wait for 20 ms after command sent as panel spec suggests */
	msleep(20);

	/* normal mode */
	qpic_panel_set_cmd_only(OP_ENTER_NORMAL_MODE);
	/* wait for 20 ms after command sent as panel spec suggests */
	msleep(20);

	/* display on */
	qpic_panel_set_cmd_only(OP_SET_DISPLAY_ON);
	/* wait for 20 ms after command sent as panel spec suggests */
	msleep(20);

	/* tearing effect  */
	param[0] = ((0x00)<<0) | ((0x00)<<8) | ((0x00)<<16) | ((0x00U)<<24U);
	qpic_send_panel_cmd(OP_ILI9341_TEARING_EFFECT_LINE_ON, param, 0);
	/* wait for 20 ms after command sent as panel spec suggests */
	msleep(20);

	return 0;
}

void ili9341_off(void)
{
	ili9341_panel_power_off();
}
