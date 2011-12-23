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

#ifndef _ARCH_ARM_MACH_MSM_MDM_PRIVATE_H
#define _ARCH_ARM_MACH_MSM_MDM_PRIVATE_H

struct mdm_modem_drv;

/* Private mdm2 data structure */
struct mdm_modem_drv {
	unsigned mdm2ap_errfatal_gpio;
	unsigned ap2mdm_errfatal_gpio;
	unsigned mdm2ap_status_gpio;
	unsigned ap2mdm_status_gpio;
	unsigned mdm2ap_wakeup_gpio;
	unsigned ap2mdm_wakeup_gpio;
	unsigned ap2mdm_pmic_reset_n_gpio;
	unsigned ap2mdm_kpdpwr_n_gpio;

	int mdm_errfatal_irq;
	int mdm_status_irq;
	int mdm_ready;
	int mdm_boot_status;
	int mdm_ram_dump_status;
	enum charm_boot_type boot_type;
	int mdm_debug_on;

	void (*power_on_mdm_cb)(struct mdm_modem_drv *mdm_drv);
	void (*normal_boot_done_cb)(struct mdm_modem_drv *mdm_drv);
	void (*power_down_mdm_cb)(struct mdm_modem_drv *mdm_drv);
	void (*debug_state_changed_cb)(int value);
	void (*status_cb)(int value);
};

struct mdm_callbacks {
	void (*power_on_mdm_cb)(struct mdm_modem_drv *mdm_drv);
	void (*normal_boot_done_cb)(struct mdm_modem_drv *mdm_drv);
	void (*power_down_mdm_cb)(struct mdm_modem_drv *mdm_drv);
	void (*debug_state_changed_cb)(int value);
	void (*status_cb)(int value);
};

int mdm_common_create(struct platform_device  *pdev,
					  struct mdm_callbacks *mdm_cb);
int mdm_common_modem_remove(struct platform_device *pdev);
void mdm_common_modem_shutdown(struct platform_device *pdev);
void mdm_common_set_debug_state(int value);

#endif

