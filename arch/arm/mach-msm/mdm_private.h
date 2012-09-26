/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#define GPIO_IS_VALID(gpio) \
	(gpio != -1)
struct mdm_modem_drv;

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
	unsigned mdm2ap_errfatal_gpio;
	unsigned ap2mdm_errfatal_gpio;
	unsigned mdm2ap_status_gpio;
	unsigned ap2mdm_status_gpio;
	unsigned mdm2ap_wakeup_gpio;
	unsigned ap2mdm_wakeup_gpio;
	unsigned ap2mdm_kpdpwr_n_gpio;
	unsigned ap2mdm_soft_reset_gpio;
	unsigned ap2mdm_pmic_pwr_en_gpio;
	unsigned mdm2ap_pblrdy;
	unsigned usb_switch_gpio;

	int mdm_errfatal_irq;
	int mdm_status_irq;
	int mdm_ready;
	int mdm_boot_status;
	int mdm_ram_dump_status;
	enum charm_boot_type boot_type;
	int mdm_debug_on;
	int mdm_unexpected_reset_occurred;
	int disable_status_check;

	struct mdm_ops *ops;
	struct mdm_platform_data *pdata;
};

int mdm_common_create(struct platform_device  *pdev,
					  struct mdm_ops *mdm_cb);
int mdm_common_modem_remove(struct platform_device *pdev);
void mdm_common_modem_shutdown(struct platform_device *pdev);
void mdm_common_set_debug_state(int value);

#endif

