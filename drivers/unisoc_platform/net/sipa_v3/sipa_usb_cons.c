// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sipa_rm: " fmt

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/sipa.h>

#include "sipa_priv.h"
#include "sipa_hal.h"

static void sipa_usb_rm_notify_cb(void *user_data,
				  enum sipa_rm_event event,
				  unsigned long data)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	pr_debug("%s: event %d\n", __func__, event);
	switch (event) {
	case SIPA_RM_EVT_GRANTED:
		complete(&ipa->usb_rm_comp);
		break;
	case SIPA_RM_EVT_RELEASED:
		break;
	default:
		pr_err("%s: unknown event %d\n", __func__, event);
		break;
	}
}

/**
 * sipa_rm_usb_cons_init() - register usb consumer.
 *
 * Register a usb consumer user to monitor the status of usb consumer.
 */
int sipa_rm_usb_cons_init(void)
{
	struct sipa_rm_register_params r_param;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	init_completion(&ipa->usb_rm_comp);

	r_param.user_data = ipa;
	r_param.notify_cb = sipa_usb_rm_notify_cb;
	return sipa_rm_register(SIPA_RM_RES_CONS_USB, &r_param);
}
EXPORT_SYMBOL(sipa_rm_usb_cons_init);

/**
 * sipa_rm_usb_cons_deinit() - register usb consumer.
 *
 * Cancel the monitoring of usb consumer.
 */
void sipa_rm_usb_cons_deinit(void)
{
	struct sipa_rm_register_params r_param;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	r_param.user_data = ipa;
	r_param.notify_cb = sipa_usb_rm_notify_cb;
	sipa_rm_deregister(SIPA_RM_RES_CONS_USB, &r_param);
}
EXPORT_SYMBOL(sipa_rm_usb_cons_deinit);

/**
 * sipa_rm_set_usb_eth_up() - request usb consumer.
 *
 * Request usb consumer, but there is no guarantee that the resource is
 * available when returned.
 */
int sipa_rm_set_usb_eth_up(void)
{
	int ret;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	reinit_completion(&ipa->usb_rm_comp);
	ret = sipa_rm_request_resource(SIPA_RM_RES_CONS_USB);
	if (ret) {
		if (ret != -EINPROGRESS)
			return ret;
		wait_for_completion(&ipa->usb_rm_comp);
	}

	return ret;
}
EXPORT_SYMBOL(sipa_rm_set_usb_eth_up);

/**
 * sipa_rm_set_usb_eth_down() - release usb consumer.
 *
 * Release the usb consumer and remove the dependency at the same time.
 */
void sipa_rm_set_usb_eth_down(void)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	reinit_completion(&ipa->usb_rm_comp);

	sipa_rm_release_resource(SIPA_RM_RES_CONS_USB);

	sipa_rm_delete_dependency(SIPA_RM_RES_CONS_USB,
				  SIPA_RM_RES_PROD_IPA);
}
EXPORT_SYMBOL(sipa_rm_set_usb_eth_down);

/**
 * sipa_rm_enable_usb_tether() - add producer dependent on usb consumer.
 */
void sipa_rm_enable_usb_tether(void)
{
	sipa_rm_add_dependency_sync(SIPA_RM_RES_CONS_USB,
				    SIPA_RM_RES_PROD_IPA);
}
EXPORT_SYMBOL(sipa_rm_enable_usb_tether);
