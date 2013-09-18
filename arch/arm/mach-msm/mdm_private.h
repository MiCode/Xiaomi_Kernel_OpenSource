/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_MDM_PRIVATE_H
#define _ARCH_ARM_MACH_MSM_MDM_PRIVATE_H

#define MDM_DEBUG_MASK_VDDMIN_SETUP (0x00000002)
#define MDM_DEBUG_MASK_SHDN_LOG     (0x00000004)
#define INVALID_GPIO (-1)
#define GPIO_IS_VALID(gpio) \
	(gpio != INVALID_GPIO)
#define MDM_GPIO(i) \
	(mdm_drv->gpios[i])

struct mdm_modem_drv;

enum {
	MDM2AP_ERRFATAL = 0,
	AP2MDM_ERRFATAL,
	MDM2AP_STATUS,
	AP2MDM_STATUS,
	AP2MDM_SOFT_RESET,
	MDM2AP_WAKEUP,
	AP2MDM_WAKEUP,
	AP2MDM_CHNLRDY,
	AP2MDM_KPDPWR,
	AP2MDM_PMIC_PWR_EN,
	MDM2AP_PBLRDY,
	USB_SW,
	AP2MDM_VDDMIN,
	MDM2AP_VDDMIN,
	GPIO_TOTAL
};

struct mdm_ops {
	void (*power_on_mdm_cb)(struct mdm_modem_drv *mdm_drv);
	void (*reset_mdm_cb)(struct mdm_modem_drv *mdm_drv);
	void (*atomic_reset_mdm_cb)(struct mdm_modem_drv *mdm_drv);
	void (*normal_boot_done_cb)(struct mdm_modem_drv *mdm_drv);
	void (*power_down_mdm_cb)(struct mdm_modem_drv *mdm_drv);
	void (*debug_state_changed_cb)(int value);
	void (*status_cb)(struct mdm_modem_drv *mdm_drv, int value);
	void (*image_upgrade_cb)(struct mdm_modem_drv *mdm_drv, int type);
};

/* Private mdm2 data structure */
struct mdm_modem_drv {
	unsigned gpios[GPIO_TOTAL];
	atomic_t mdm_ready;
	int mdm_boot_status;
	int mdm_ram_dump_status;
	enum charm_boot_type boot_type;
	int mdm_debug_on;
	int mdm_unexpected_reset_occurred;
	int disable_status_check;
	unsigned int dump_timeout_ms;
	int power_on_count;
	int peripheral_status;
	struct mutex peripheral_status_lock;
	int device_id;

	struct mdm_platform_data *pdata;
};
int mdm_get_ops(struct mdm_ops **mdm_ops);

#endif

