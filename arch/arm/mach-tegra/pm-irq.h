/*
 * Copyright (C) 2011 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@android.com>
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MACH_TERA_PM_IRQ_H_
#define _MACH_TERA_PM_IRQ_H_

#define PMC_MAX_WAKE_COUNT 64

#ifdef CONFIG_PM_SLEEP
u64 tegra_read_pmc_wake_status(void);
int tegra_pm_irq_set_wake(int wake, int enable);
int tegra_pm_irq_set_wake_type(int wake, int flow_type);
bool tegra_pm_irq_lp0_allowed(void);
int tegra_gpio_to_wake(int gpio);
void tegra_irq_to_wake(int irq, int *wak_list, int *wak_size);
int tegra_wake_to_irq(int wake);
int tegra_disable_wake_source(int wake);
#else
static inline int tegra_pm_irq_set_wake_type(int wake, int flow_type)
{
	return 0;
}
static inline int tegra_gpio_to_wake(int gpio)
{
	return 0;
}
static inline
void tegra_irq_to_wake(int irq, int *wak_list, int *wak_size)
{
	*wak_size = 0;
	return;
}
static inline int tegra_disable_wake_source(int wake)
{
	return 0;
}
#endif
void tegra_set_usb_wake_source(void);

/* tegra internal any polarity wake sources */
enum {
	ANY_WAKE_INDEX_VBUS = 0,
	ANY_WAKE_INDEX_ID
};

/* get chip specific list of internal any polarity wake sources */
void tegra_get_internal_any_wake_list(u8 *wake_count, u8 **any_wake,
	u8 *remote_usb_wak_index);

/*
 * is_vbus_connected - true when VBUS cable is connected
 * is_id_connected - true when ID cable is connected
 * returns error if failed to read the status for a chip
 * or if the API is not supported
 */
int get_vbus_id_cable_connect_state(bool *is_vbus_connected,
		bool *is_id_connected);

#endif
