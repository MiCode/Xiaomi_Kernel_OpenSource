/*
 * Copyright (C) 2010-2013 NVIDIA Corporation.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_IRQCHIP_TEGRA_H
#define __LINUX_IRQCHIP_TEGRA_H

#define PMC_MAX_WAKE_COUNT 64
#define	GIC_V1	1
#define	GIC_V2	2

/* tegra internal any polarity wake sources */
enum {
	ANY_WAKE_INDEX_VBUS = 0,
	ANY_WAKE_INDEX_ID
};

#if defined(CONFIG_HOTPLUG_CPU) || defined(CONFIG_PM_SLEEP)
void tegra_gic_cpu_disable(bool disable_pass_through);
void tegra_gic_cpu_enable(void);
#endif

#if defined(CONFIG_PM_SLEEP)
int tegra_gic_pending_interrupt(void);
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
void tegra_gic_dist_disable(void);
void tegra_gic_dist_enable(void);

void tegra_gic_disable_affinity(void);
void tegra_gic_restore_affinity(void);
void tegra_gic_affinity_to_cpu0(void);
#endif
#endif

u32 tegra_gic_version(void);

extern int *tegra_gpio_wake_table;
extern int *tegra_irq_wake_table;
extern int tegra_wake_table_len;

#if defined(CONFIG_PM_SLEEP)
u64 tegra_read_pmc_wake_status(void);
int tegra_pm_irq_set_wake(int wake, int enable);
int tegra_pm_irq_set_wake_type(int wake, int flow_type);
bool tegra_pm_irq_lp0_allowed(void);
int tegra_set_wake_gpio(unsigned int wake, int gpio);
int tegra_set_wake_irq(unsigned int wake, int irq);
int tegra_gpio_to_wake(int gpio);
void tegra_irq_to_wake(int irq, int *wak_list, int *wak_size);
int tegra_wake_to_irq(int wake);
int tegra_set_wake_source(int wake, int wake_int);
int tegra_disable_wake_source(int wake);
#else
static inline int tegra_set_wake_gpio(unsigned int wake, int gpio)
{
	return 0;
}
static inline int tegra_set_wake_irq(unsigned int wake, int irq)
{
	return 0;
}
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
static inline int tegra_set_wake_source(int wake, int wake_int)
{
	return 0;
}
static inline int tegra_disable_wake_source(int wake)
{
	return 0;
}
#endif

void tegra_set_usb_wake_source(void);

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

/* enable/disable an interrupt that is an FIQ (safe from FIQ context?) */
void tegra_fiq_enable(int n);
void tegra_fiq_disable(int n);

void tegra_init_legacy_irq_cop(void);

/* lp1 wake interrupts enabled or disabled using this API */
int tegra_update_lp1_irq_wake(unsigned int irq, bool enable);

#endif /* __LINUX_IRQCHIP_TEGRA_H */
