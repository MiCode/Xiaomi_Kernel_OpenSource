/* Copyright (c) 2013 - 2014, The Linux Foundation. All rights reserved.
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

static int ili9341_init(struct qpic_panel_io_desc *panel_io)
{
	int rc;
	if (panel_io->vdd_vreg) {
		rc = regulator_set_voltage(panel_io->vdd_vreg,
			1800000, 1800000);
		if (rc) {
			pr_err("vdd_vreg->set_voltage failed, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	if (panel_io->avdd_vreg) {
		rc = regulator_set_voltage(panel_io->avdd_vreg,
			2700000, 2700000);
		if (rc) {
			pr_err("vdd_vreg->set_voltage failed, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	return 0;
}

void ili9341_off(struct qpic_panel_io_desc *qpic_panel_io)
{
	if (qpic_panel_io->ad8_gpio)
		gpio_free(qpic_panel_io->ad8_gpio);
	if (qpic_panel_io->cs_gpio)
		gpio_free(qpic_panel_io->cs_gpio);
	if (qpic_panel_io->rst_gpio)
		gpio_free(qpic_panel_io->rst_gpio);
	if (qpic_panel_io->te_gpio)
		gpio_free(qpic_panel_io->te_gpio);
	if (qpic_panel_io->bl_gpio)
		gpio_free(qpic_panel_io->bl_gpio);
	if (qpic_panel_io->vdd_vreg)
		regulator_disable(qpic_panel_io->vdd_vreg);
	if (qpic_panel_io->avdd_vreg)
		regulator_disable(qpic_panel_io->avdd_vreg);
}

static int ili9341_panel_power_on(struct qpic_panel_io_desc *qpic_panel_io)
{
	int rc;
	if (qpic_panel_io->vdd_vreg) {
		rc = regulator_enable(qpic_panel_io->vdd_vreg);
		if (rc) {
			pr_err("enable vdd failed, rc=%d\n", rc);
			return -ENODEV;
		}
	}

	if (qpic_panel_io->avdd_vreg) {
		rc = regulator_enable(qpic_panel_io->avdd_vreg);
		if (rc) {
			pr_err("enable avdd failed, rc=%d\n", rc);
			goto power_on_error;
		}
	}

	if ((qpic_panel_io->rst_gpio) &&
		(gpio_request(qpic_panel_io->rst_gpio, "disp_rst_n"))) {
		pr_err("%s request reset gpio failed\n", __func__);
		goto power_on_error;
	}

	if ((qpic_panel_io->cs_gpio) &&
		(gpio_request(qpic_panel_io->cs_gpio, "disp_cs_n"))) {
		pr_err("%s request cs gpio failed\n", __func__);
		goto power_on_error;
	}

	if ((qpic_panel_io->ad8_gpio) &&
		(gpio_request(qpic_panel_io->ad8_gpio, "disp_ad8_n"))) {
		pr_err("%s request ad8 gpio failed\n", __func__);
		goto power_on_error;
	}

	if ((qpic_panel_io->te_gpio) &&
		(gpio_request(qpic_panel_io->te_gpio, "disp_te_n"))) {
		pr_err("%s request te gpio failed\n", __func__);
		goto power_on_error;
	}

	if ((qpic_panel_io->bl_gpio) &&
		(gpio_request(qpic_panel_io->bl_gpio, "disp_bl_n"))) {
		pr_err("%s request bl gpio failed\n", __func__);
		goto power_on_error;
	}
	/* wait for 20 ms after enable gpio as suggested by hw */
	msleep(20);
	return 0;
power_on_error:
	ili9341_off(qpic_panel_io);
	return -EINVAL;
}

int ili9341_on(struct qpic_panel_io_desc *qpic_panel_io)
{
	u32 param[20];
	int ret;
	if (!qpic_panel_io->init) {
		ili9341_init(qpic_panel_io);
		qpic_panel_io->init = true;
	}
	ret = ili9341_panel_power_on(qpic_panel_io);
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
