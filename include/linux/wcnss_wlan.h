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
 *
 */

#ifndef _WCNSS_WLAN_H_
#define _WCNSS_WLAN_H_

#include <linux/device.h>

enum wcnss_opcode {
	WCNSS_WLAN_SWITCH_OFF = 0,
	WCNSS_WLAN_SWITCH_ON,
};

enum wcnss_hw_type {
	WCNSS_RIVA_HW = 0,
	WCNSS_PRONTO_HW,
};

struct wcnss_wlan_config {
	int		use_48mhz_xo;
};

#define WCNSS_WLAN_IRQ_INVALID -1
#define HAVE_WCNSS_SUSPEND_RESUME_NOTIFY 1
#define HAVE_WCNSS_RESET_INTR 1

struct device *wcnss_wlan_get_device(void);
struct resource *wcnss_wlan_get_memory_map(struct device *dev);
int wcnss_wlan_get_dxe_tx_irq(struct device *dev);
int wcnss_wlan_get_dxe_rx_irq(struct device *dev);
void wcnss_wlan_register_pm_ops(struct device *dev,
				const struct dev_pm_ops *pm_ops);
void wcnss_wlan_unregister_pm_ops(struct device *dev,
				const struct dev_pm_ops *pm_ops);
void wcnss_register_thermal_mitigation(struct device *dev,
				void (*tm_notify)(struct device *dev, int));
void wcnss_unregister_thermal_mitigation(
				void (*tm_notify)(struct device *dev, int));
struct platform_device *wcnss_get_platform_device(void);
struct wcnss_wlan_config *wcnss_get_wlan_config(void);
int wcnss_wlan_power(struct device *dev,
				struct wcnss_wlan_config *cfg,
				enum wcnss_opcode opcode);
int wcnss_req_power_on_lock(char *driver_name);
int wcnss_free_power_on_lock(char *driver_name);
unsigned int wcnss_get_serial_number(void);
void wcnss_flush_delayed_boot_votes(void);
void wcnss_allow_suspend(void);
void wcnss_prevent_suspend(void);
int wcnss_hardware_type(void);
void *wcnss_prealloc_get(unsigned int size);
int wcnss_prealloc_put(void *ptr);
void wcnss_reset_intr(void);
void wcnss_suspend_notify(void);
void wcnss_resume_notify(void);

#define wcnss_wlan_get_drvdata(dev) dev_get_drvdata(dev)
#define wcnss_wlan_set_drvdata(dev, data) dev_set_drvdata((dev), (data))
/* WLAN driver uses these names */
#define req_riva_power_on_lock(name) wcnss_req_power_on_lock(name)
#define free_riva_power_on_lock(name) wcnss_free_power_on_lock(name)

#endif /* _WCNSS_WLAN_H_ */
