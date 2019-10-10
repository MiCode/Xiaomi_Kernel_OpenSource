// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2015, 2017-2019, The Linux Foundation. All rights reserved.
 */

#include "esoc-mdm.h"
#include <linux/input/qpnp-power-on.h>

/* This function can be called from atomic context. */
static int mdm9x55_toggle_soft_reset(struct mdm_ctrl *mdm, bool atomic)
{
	int soft_reset_direction_assert = 0,
	    soft_reset_direction_de_assert = 1;
	uint32_t reset_time_us = mdm->reset_time_ms * 1000;

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
		usleep_range(reset_time_us, reset_time_us + 100000);
	else
		/*
		 * The flow falls through this path as a part of the
		 * panic handler, which has to executed atomically.
		 */
		mdelay(mdm->reset_time_ms);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_de_assert);
	return 0;
}

/* This function can be called from atomic context. */
static int sdx50m_toggle_soft_reset(struct mdm_ctrl *mdm, bool atomic)
{
	int soft_reset_direction_assert = 0,
	    soft_reset_direction_de_assert = 1;

	if (mdm->soft_reset_inverted) {
		soft_reset_direction_assert = 1;
		soft_reset_direction_de_assert = 0;
	}

	esoc_mdm_log("RESET GPIO value (before doing a reset): %d\n",
			gpio_get_value(MDM_GPIO(mdm, AP2MDM_SOFT_RESET)));
	esoc_mdm_log("Setting AP2MDM_SOFT_RESET = %d\n",
				soft_reset_direction_assert);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_assert);
	/*
	 * Allow PS hold assert to be detected
	 */
	if (!atomic)
		usleep_range(80000, 180000);
	else
		/*
		 * The flow falls through this path as a part of the
		 * panic handler, which has to executed atomically.
		 */
		mdelay(100);

	esoc_mdm_log("Setting AP2MDM_SOFT_RESET = %d\n",
				soft_reset_direction_de_assert);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_de_assert);
	return 0;
}

/* This function can be called from atomic context. */
static int sdx55m_toggle_soft_reset(struct mdm_ctrl *mdm, bool atomic)
{
	int rc;

	esoc_mdm_log("Doing a Warm reset using SPMI\n");
	rc = qpnp_pon_modem_pwr_off(PON_POWER_OFF_WARM_RESET);
	if (rc) {
		dev_err(mdm->dev, "SPMI warm reset failed\n");
		esoc_mdm_log("SPMI warm reset failed\n");
		return rc;
	}
	esoc_mdm_log("Warm reset done using SPMI\n");
	return 0;
}

static int mdm4x_do_first_power_on(struct mdm_ctrl *mdm)
{
	int i;
	int pblrdy;
	struct device *dev = mdm->dev;

	esoc_mdm_log("Powering on modem for the first time\n");
	dev_dbg(dev, "Powering on modem for the first time\n");
	if (mdm->esoc->auto_boot)
		return 0;

	mdm_toggle_soft_reset(mdm, false);
	/* Add a delay to allow PON sequence to complete*/
	msleep(150);
	esoc_mdm_log("Setting AP2MDM_STATUS = 1\n");
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
	else {
		esoc_mdm_log("Queueing the request: ESOC_REQ_IMG\n");
		esoc_clink_queue_request(ESOC_REQ_IMG, mdm->esoc);
	}
	return 0;
}

static int mdm9x55_power_down(struct mdm_ctrl *mdm)
{
	struct device *dev = mdm->dev;
	int soft_reset_direction_assert = 0,
	    soft_reset_direction_de_assert = 1;

	if (mdm->soft_reset_inverted) {
		soft_reset_direction_assert = 1;
		soft_reset_direction_de_assert = 0;
	}
	/* Assert the soft reset line whether mdm2ap_status went low or not */
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_assert);
	dev_dbg(dev, "Doing a hard reset\n");
	/*
	 * Currently, there is a debounce timer on the charm PMIC. It is
	 * necessary to hold the PMIC RESET low for 406ms
	 * for the reset to fully take place. Sleep here to ensure the
	 * reset has occurred before the function exits.
	 */
	msleep(406);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			soft_reset_direction_de_assert);
	return 0;
}

static int sdx50m_power_down(struct mdm_ctrl *mdm)
{
	struct device *dev = mdm->dev;
	int soft_reset_direction = mdm->soft_reset_inverted ? 1 : 0;
	/* Assert the soft reset line whether mdm2ap_status went low or not */
	esoc_mdm_log("Setting AP2MDM_SOFT_RESET = %d\n", soft_reset_direction);
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
	msleep(300);
	return 0;
}

static int sdx55m_power_down(struct mdm_ctrl *mdm)
{
	esoc_mdm_log("Performing warm reset as cold reset is not supported\n");
	return sdx55m_toggle_soft_reset(mdm, false);
}

static void mdm9x55_cold_reset(struct mdm_ctrl *mdm)
{
	dev_dbg(mdm->dev, "Triggering mdm cold reset");

	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			!!mdm->soft_reset_inverted);

	/*
	 * The function is executed as a part of the atomic reboot handler.
	 * Hence, go with a busy loop instead of sleep.
	 */
	mdelay(334);

	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			!mdm->soft_reset_inverted);
}

static void sdx50m_cold_reset(struct mdm_ctrl *mdm)
{
	dev_dbg(mdm->dev, "Triggering mdm cold reset");
	esoc_mdm_log("Setting AP2MDM_SOFT_RESET = %d\n",
					!!mdm->soft_reset_inverted);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			!!mdm->soft_reset_inverted);

	/*
	 * The function is executed as a part of the atomic reboot handler.
	 * Hence, go with a busy loop instead of sleep.
	 */
	mdelay(600);

	esoc_mdm_log("Setting AP2MDM_SOFT_RESET = %d\n",
					!!mdm->soft_reset_inverted);
	gpio_direction_output(MDM_GPIO(mdm, AP2MDM_SOFT_RESET),
			!mdm->soft_reset_inverted);
}

static int mdm9x55_pon_dt_init(struct mdm_ctrl *mdm)
{
	int val;
	struct device_node *node = mdm->dev->of_node;
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;


	val = of_property_read_u32(node, "qcom,reset-time-ms",
				   &mdm->reset_time_ms);
	if (val)
		mdm->reset_time_ms = DEF_MDM9X55_RESET_TIME;

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

struct mdm_pon_ops mdm9x55_pon_ops = {
	.pon = mdm4x_do_first_power_on,
	.soft_reset = mdm9x55_toggle_soft_reset,
	.poff_force = mdm9x55_power_down,
	.cold_reset = mdm9x55_cold_reset,
	.dt_init = mdm9x55_pon_dt_init,
	.setup = mdm4x_pon_setup,
};

struct mdm_pon_ops sdx50m_pon_ops = {
	.pon = mdm4x_do_first_power_on,
	.soft_reset = sdx50m_toggle_soft_reset,
	.poff_force = sdx50m_power_down,
	.cold_reset = sdx50m_cold_reset,
	.dt_init = mdm4x_pon_dt_init,
	.setup = mdm4x_pon_setup,
};

struct mdm_pon_ops sdx55m_pon_ops = {
	.pon = mdm4x_do_first_power_on,
	.soft_reset = sdx55m_toggle_soft_reset,
	.poff_force = sdx55m_power_down,
};
