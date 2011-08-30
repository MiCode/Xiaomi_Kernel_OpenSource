/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/device.h>
#include <mach/vreg.h>
#include <linux/gpio.h>
#include <mach/rpc_pmapp.h>
#include <linux/err.h>
#include <linux/qcomwlan7x27a_pwrif.h>

#define WLAN_GPIO_EXT_POR_N     134

static const char *id = "WLAN";

enum {
	WLAN_VREG_L17 = 0,
	WLAN_VREG_S3,
	WLAN_VREG_TCXO_L11,
	WLAN_VREG_L19,
	WLAN_VREG_L5,
	WLAN_VREG_L6
};

struct wlan_vreg_info {
	const char *vreg_id;
	unsigned int vreg_level;
	unsigned int pmapp_id;
	unsigned int is_vreg_pin_controlled;
	struct vreg *vreg;
};


static struct wlan_vreg_info vreg_info[] = {
	{"bt", 3050, 21, 1, NULL},
	{"msme1", 1800, 2, 0, NULL},
	{"wlan_tcx0", 1800, 53, 0, NULL},
	{"wlan4", 1200, 23, 0, NULL},
	{"wlan2", 1350, 9, 1, NULL},
	{"wlan3", 1200, 10, 1, NULL} };


int chip_power_qrf6285(bool on)
{
	int rc = 0, index = 0;

	if (on) {
		rc = gpio_request(WLAN_GPIO_EXT_POR_N, "WLAN_DEEP_SLEEP_N");

		if (rc) {
			pr_err("WLAN reset GPIO %d request failed %d\n",
			WLAN_GPIO_EXT_POR_N, rc);
			goto fail;
		}
		rc = gpio_direction_output(WLAN_GPIO_EXT_POR_N, 1);
		if (rc < 0) {
			pr_err("WLAN reset GPIO %d set direction failed %d\n",
			WLAN_GPIO_EXT_POR_N, rc);
			goto fail_gpio_dir_out;
		}
		rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_A0,
					PMAPP_CLOCK_VOTE_ON);
		if (rc) {
			pr_err("%s: Configuring A0 to always"
			" on failed %d\n", __func__, rc);
			goto clock_vote_fail;
		}
	} else {
		gpio_set_value_cansleep(WLAN_GPIO_EXT_POR_N, 0);
		rc = gpio_direction_input(WLAN_GPIO_EXT_POR_N);
		if (rc) {
			pr_err("WLAN reset GPIO %d set direction failed %d\n",
			WLAN_GPIO_EXT_POR_N, rc);
		}
		gpio_free(WLAN_GPIO_EXT_POR_N);
		rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_A0,
					PMAPP_CLOCK_VOTE_OFF);
		if (rc) {
			pr_err("%s: Configuring A0 to turn OFF"
			" failed %d\n", __func__, rc);
		}
	}


	for (index = 0; index < ARRAY_SIZE(vreg_info); index++) {
		vreg_info[index].vreg = vreg_get(NULL,
						vreg_info[index].vreg_id);
		if (IS_ERR(vreg_info[index].vreg)) {
			pr_err("%s:%s vreg get failed %ld\n",
				__func__, vreg_info[index].vreg_id,
				PTR_ERR(vreg_info[index].vreg));
			rc = PTR_ERR(vreg_info[index].vreg);
			if (on)
				goto vreg_fail;
			else
				continue;
		}
		if (on) {
			rc = vreg_set_level(vreg_info[index].vreg,
					 vreg_info[index].vreg_level);
			if (rc) {
				pr_err("%s:%s vreg set level failed %d\n",
					__func__, vreg_info[index].vreg_id, rc);
				goto vreg_fail;
			}

			rc = vreg_enable(vreg_info[index].vreg);
			if (rc) {
				pr_err("%s:%s vreg enable failed %d\n",
					__func__,
					vreg_info[index].vreg_id, rc);
				goto vreg_fail;
			}

			if (vreg_info[index].is_vreg_pin_controlled) {
				rc = pmapp_vreg_lpm_pincntrl_vote(id,
					 vreg_info[index].pmapp_id,
					 PMAPP_CLOCK_ID_A0, 1);
				if (rc) {
					pr_err("%s:%s pmapp_vreg_lpm_pincntrl"
						" for enable failed %d\n",
						__func__,
						vreg_info[index].vreg_id, rc);
					goto vreg_pin_ctrl_fail;
				}
			}
			/*At this point CLK_PWR_REQ is high*/
			if (WLAN_VREG_L6 == index) {
				/*
				 * Configure A0 clock to be slave to
				 * WLAN_CLK_PWR_REQ
`				 */
				rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_A0,
						PMAPP_CLOCK_VOTE_PIN_CTRL);
				if (rc) {
					pr_err("%s: Configuring A0 to Pin"
					" controllable failed %d\n",
							 __func__, rc);
					goto vreg_clock_vote_fail;
				}
			}

		} else {

			if (vreg_info[index].is_vreg_pin_controlled) {
				rc = pmapp_vreg_lpm_pincntrl_vote(id,
						 vreg_info[index].pmapp_id,
						 PMAPP_CLOCK_ID_A0, 0);
				if (rc) {
					pr_err("%s:%s pmapp_vreg_lpm_pincntrl"
						" for disable failed %d\n",
						__func__,
						vreg_info[index].vreg_id, rc);
				}
			}
			rc = vreg_disable(vreg_info[index].vreg);
			if (rc) {
				pr_err("%s:%s vreg disable failed %d\n",
					__func__,
					vreg_info[index].vreg_id, rc);
			}
		}
	}
	return 0;
vreg_fail:
	index--;
vreg_pin_ctrl_fail:
vreg_clock_vote_fail:
	while (index > 0) {
		rc = vreg_disable(vreg_info[index].vreg);
		if (rc) {
			pr_err("%s:%s vreg disable failed %d\n",
				__func__, vreg_info[index].vreg_id, rc);
		}
		index--;
	}
	if (!on)
		goto fail;
clock_vote_fail:
	gpio_set_value_cansleep(WLAN_GPIO_EXT_POR_N, 0);
	rc = gpio_direction_input(WLAN_GPIO_EXT_POR_N);
	if (rc) {
		pr_err("WLAN reset GPIO %d set direction failed %d\n",
			WLAN_GPIO_EXT_POR_N, rc);
	}
fail_gpio_dir_out:
	gpio_free(WLAN_GPIO_EXT_POR_N);
fail:
	return rc;
}
EXPORT_SYMBOL(chip_power_qrf6285);
