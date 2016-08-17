/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MACH_TEGRA_USB_MODEM_POWER_H
#define __MACH_TEGRA_USB_MODEM_POWER_H

#include <linux/interrupt.h>
#include <linux/edp.h>
#include <linux/usb.h>

/* modem capabilities */
#define TEGRA_MODEM_AUTOSUSPEND	0x01
#define TEGRA_MODEM_RECOVERY	0x02
#define TEGRA_USB_HOST_RELOAD	0x04

/* modem operations */
struct tegra_modem_operations {
	int (*init) (void);	/* modem init */
	void (*start) (void);	/* modem start */
	void (*stop) (void);	/* modem stop */
	void (*suspend) (void);	/* send L3 hint during system suspend */
	void (*resume) (void);	/* send L3->0 hint during system resume */
	void (*reset) (void);	/* modem reset */
};

/* tegra usb modem power platform data */
struct tegra_usb_modem_power_platform_data {
	const struct tegra_modem_operations *ops;
	const struct usb_device_id *modem_list; /* supported modem list */
	int wake_gpio;			/* remote wakeup gpio */
	unsigned long wake_irq_flags;	/* remote wakeup irq flags */
	int boot_gpio;			/* modem boot gpio */
	unsigned long boot_irq_flags;	/* modem boot irq flags */
	int autosuspend_delay;		/* autosuspend delay in milliseconds */
	int short_autosuspend_delay;	/* short autosuspend delay in ms */
	const struct platform_device *tegra_ehci_device; /* USB host device */
	const struct tegra_usb_platform_data *tegra_ehci_pdata;
	struct edp_client *modem_boot_edp_client;
	char *edp_manager_name;
	unsigned int i_breach_ppm;
	unsigned int i_thresh_3g_adjperiod;
	unsigned int i_thresh_lte_adjperiod;
};

#endif /* __MACH_TEGRA_USB_MODEM_POWER_H */
