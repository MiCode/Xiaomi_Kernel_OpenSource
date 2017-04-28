/* Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
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

#include "esoc-mdm.h"

/* This function can be called from atomic context. */
static int mdm4x_toggle_soft_reset(struct mdm_ctrl *mdm, bool atomic)
{
	int soft_reset_direction_assert = 0,
	    soft_reset_direction_de_assert = 1;

	if (mdm->soft_reset_inverted) {
		soft_reset_direction_assert = 1;
		soft_reset_direction_de_assert = 0;
	}
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_assert);
	/*
	 * Allow PS hold assert to be detected
	 */
	if (!atomic)
		usleep_range(8000, 9000);
	else
		mdelay(6);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_de_assert);
	return 0;
}

/* This function can be called from atomic context. */
static int mdm9x55_toggle_soft_reset(struct mdm_ctrl *mdm, bool atomic)
{
	int soft_reset_direction_assert = 0,
	    soft_reset_direction_de_assert = 1;

	if (mdm->soft_reset_inverted) {
		soft_reset_direction_assert = 1;
		soft_reset_direction_de_assert = 0;
	}
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_assert);
	/*
	 * Allow PS hold assert to be detected
	 */
	if (!atomic)
		usleep_range(203000, 300000);
	else
		mdelay(203);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_de_assert);
	return 0;
}

/* This function can be called from atomic context. */
static int mdm9x45_toggle_soft_reset(struct mdm_ctrl *mdm, bool atomic)
{
	int soft_reset_direction_assert = 0,
	    soft_reset_direction_de_assert = 1;

	if (mdm->soft_reset_inverted) {
		soft_reset_direction_assert = 1;
		soft_reset_direction_de_assert = 0;
	}
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_assert);
	/*
	 * Allow PS hold assert to be detected
	 */
	if (!atomic)
		usleep_range(1000000, 1005000);
	else
		mdelay(1000);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_de_assert);
	return 0;
}

static int mdm4x_do_first_power_on(struct mdm_ctrl *mdm)
{
	int i;
	int pblrdy;
	struct device *dev = mdm->dev;

	dev_dbg(dev, "Powering on modem for the first time\n");
	if (mdm->esoc->auto_boot)
		return 0;

	mdm_toggle_soft_reset(mdm, false);
	/* Add a delay to allow PON sequence to complete*/
	msleep(50);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_STATUS), 1);
	if (gpio_is_valid(MDM_GPIO(mdm, MDM2AP_PBLRDY))) {
		for (i = 0; i  < MDM_PBLRDY_CNT; i++) {
			pblrdy = gpio_get_value(MDM_GPIO(mdm, MDM2AP_PBLRDY));
			if (pblrdy)
				break;
			usleep_range(5000, 6000);
		}
		dev_dbg(dev, "pblrdy i:%d\n", i);
		msleep(200);
	}
	/*
	 * No PBLRDY gpio associated with this modem
	 * Send request for image. Let userspace confirm establishment of
	 * link to external modem.
	 */
	else
		esoc_clink_queue_request(ESOC_REQ_IMG, mdm->esoc);
	return 0;
}

static int mdm4x_power_down(struct mdm_ctrl *mdm)
{
	struct device *dev = mdm->dev;
	int soft_reset_direction = mdm->soft_reset_inverted ? 1 : 0;
	/* Assert the soft reset line whether mdm2ap_status went low or not */
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
					soft_reset_direction);
	dev_dbg(dev, "Doing a hard reset\n");
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
						soft_reset_direction);
	/*
	* Currently, there is a debounce timer on the charm PMIC. It is
	* necessary to hold the PMIC RESET low for 400ms
	* for the reset to fully take place. Sleep here to ensure the
	* reset has occurred before the function exits.
	*/
	msleep(400);
	return 0;
}

static int mdm9x55_power_down(struct mdm_ctrl *mdm)
{
	struct device *dev = mdm->dev;
	int soft_reset_direction = mdm->soft_reset_inverted ? 1 : 0;
	/* Assert the soft reset line whether mdm2ap_status went low or not */
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
					soft_reset_direction);
	dev_dbg(dev, "Doing a hard reset\n");
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
						soft_reset_direction);
	/*
	* Currently, there is a debounce timer on the charm PMIC. It is
	* necessary to hold the PMIC RESET low for 406ms
	* for the reset to fully take place. Sleep here to ensure the
	* reset has occurred before the function exits.
	*/
	msleep(406);
	return 0;
}

static int mdm9x45_power_down(struct mdm_ctrl *mdm)
{
	int soft_reset_direction_assert = 0,
	    soft_reset_direction_de_assert = 1;

	if (mdm->soft_reset_inverted) {
		soft_reset_direction_assert = 1;
		soft_reset_direction_de_assert = 0;
	}
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_assert);
	/*
	 * Allow PS hold assert to be detected
	 */
	msleep(3003);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_de_assert);
	return 0;
}

static void mdm4x_cold_reset(struct mdm_ctrl *mdm)
{
	if (!gpio_is_valid(MDM_GPIO(mdm, AP2MDM_SOFT_RESET)))
		return;

	dev_dbg(mdm->dev, "Triggering mdm cold reset");
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			!!mdm->soft_reset_inverted);
	msleep(300);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			!mdm->soft_reset_inverted);
}

static void mdm9x55_cold_reset(struct mdm_ctrl *mdm)
{
	dev_dbg(mdm->dev, "Triggering mdm cold reset");
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			!!mdm->soft_reset_inverted);
	mdelay(334);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			!mdm->soft_reset_inverted);
}

static int apq8096_pon_dt_init(struct mdm_ctrl *mdm)
{
	return 0;
}

static int mdm4x_pon_dt_init(struct mdm_ctrl *mdm)
{
	int val;
	struct device_node *node = mdm->dev->of_node;
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;

	val = of_get_named_gpio_flags(node, "qcom,ap2mdm-soft-reset-gpio",
						0, &flags);
	if (val >= 0) {
		MDM_GPIO(mdm, AP2MDM_SOFT_RESET) = val;
		if (flags & OF_GPIO_ACTIVE_LOW)
			mdm->soft_reset_inverted = 1;
		return 0;
	} else
		return -EIO;
}

static int mdm4x_pon_setup(struct mdm_ctrl *mdm)
{
	struct device *dev = mdm->dev;

	if (gpio_is_valid(MDM_GPIO(mdm, AP2MDM_SOFT_RESET))) {
		if (gpio_request(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
					 "AP2MDM_SOFT_RESET")) {
			dev_err(dev, "Cannot config AP2MDM_SOFT_RESET gpio\n");
			return -EIO;
		}
	}
	return 0;
}

/* This function can be called from atomic context. */
static int apq8096_toggle_soft_reset(struct mdm_ctrl *mdm, bool atomic)
{
	return 0;
}

static int apq8096_power_down(struct mdm_ctrl *mdm)
{
	return 0;
}

static void apq8096_cold_reset(struct mdm_ctrl *mdm)
{
}

struct mdm_pon_ops mdm9x25_pon_ops = {
	.pon = mdm4x_do_first_power_on,
	.soft_reset = mdm4x_toggle_soft_reset,
	.poff_force = mdm4x_power_down,
	.cold_reset = mdm4x_cold_reset,
	.dt_init = mdm4x_pon_dt_init,
	.setup = mdm4x_pon_setup,
};

struct mdm_pon_ops mdm9x35_pon_ops = {
	.pon = mdm4x_do_first_power_on,
	.soft_reset = mdm4x_toggle_soft_reset,
	.poff_force = mdm4x_power_down,
	.cold_reset = mdm4x_cold_reset,
	.dt_init = mdm4x_pon_dt_init,
	.setup = mdm4x_pon_setup,
};

struct mdm_pon_ops mdm9x45_pon_ops = {
	.pon = mdm4x_do_first_power_on,
	.soft_reset = mdm9x45_toggle_soft_reset,
	.poff_force = mdm9x45_power_down,
	.cold_reset = mdm4x_cold_reset,
	.dt_init = mdm4x_pon_dt_init,
	.setup = mdm4x_pon_setup,
};

struct mdm_pon_ops mdm9x55_pon_ops = {
	.pon = mdm4x_do_first_power_on,
	.soft_reset = mdm9x55_toggle_soft_reset,
	.poff_force = mdm9x55_power_down,
	.cold_reset = mdm9x55_cold_reset,
	.dt_init = mdm4x_pon_dt_init,
	.setup = mdm4x_pon_setup,
};

struct mdm_pon_ops apq8096_pon_ops = {
	.pon = mdm4x_do_first_power_on,
	.soft_reset = apq8096_toggle_soft_reset,
	.poff_force = apq8096_power_down,
	.cold_reset = apq8096_cold_reset,
	.dt_init = apq8096_pon_dt_init,
	.setup = mdm4x_pon_setup,
};
