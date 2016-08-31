/*
 * drivers/misc/tegra-baseband/bb-power.h
 *
 * Copyright (C) 2012 NVIDIA Corporation
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

enum tegra_bb_state {
	/* Baseband state L0 - Running */
	BBSTATE_L0 = 0,
	/* Baseband state L02L2 - Running->Suspend */
	BBSTATE_L02L2 = 1,
	/* Baseband state L2 - Suspended */
	BBSTATE_L2 = 2,
	/* Baseband state L3 - Suspended and detached */
	BBSTATE_L3 = 3,
	/* Invalid baseband state */
	BBSTATE_UNKNOWN = 0xFF,
};

enum tegra_bb_pwrstate {
	/* System power state - Entering suspend */
	PWRSTATE_L2L3,
	/* System power state - Entering suspend, no irq */
	PWRSTATE_L2L3_NOIRQ,
	/* System power state - Resuming from suspend */
	PWRSTATE_L3L0,
	/* System power state - Resuming from suspend, no irq */
	PWRSTATE_L3L0_NOIRQ,
	/* Invalid system power state */
	PWRSTATE_INVALID,
};

enum tegra_bb_dlevel {
	/* Debug level - Initialization */
	DLEVEL_INIT = 0,
	/* Debug level - Sysfs callbacks */
	DLEVEL_SYS_CB = 1U << 0,
	/* Debug level - PM */
	DLEVEL_PM = 1U << 1,
	/* Debug level - Misc */
	DLEVEL_MISC = 1U << 2,
	/* Debug level - Max */
	DLEVEL_MAX = 1U << 3,
};

struct tegra_bb_gpio_data {
	/* Baseband gpio data */
	struct gpio data;
	/* Baseband gpio - Should it be exported to sysfs ? */
	bool doexport;
};

struct tegra_bb_gpio_irqdata {
	/* Baseband gpio IRQ - Id */
	int id;
	/* Baseband gpio IRQ - Friendly name */
	const char *name;
	/* Baseband gpio IRQ - IRQ handler */
	irq_handler_t handler;
	/* Baseband gpio IRQ - IRQ trigger flags */
	int flags;
	/* Baseband gpio IRQ - Can the gpio wake system from sleep ? */
	bool wake_capable;
	void *cookie;
};

typedef void* (*bb_get_cblist)(void);
typedef void* (*cb_init)(void *pdata);
typedef void* (*cb_deinit)(void);
typedef int (*cb_power)(int code);
typedef int (*cb_attrib_access)(struct device *dev, int value);
typedef int (*cb_usbnotify)(struct usb_device *udev, bool registered);
typedef int (*cb_pmnotify)(unsigned long event);

struct tegra_bb_power_gdata {
	struct tegra_bb_gpio_data *gpio;
	struct tegra_bb_gpio_irqdata *gpioirq;
};

struct tegra_bb_power_mdata {
	/* Baseband USB vendor ID */
	int vid;
	/* Baseband USB product ID */
	int pid;
	/* Baseband capability - Can it generate a wakeup ? */
	bool wake_capable;
	/* Baseband capability - Can it be auto/runtime suspended ? */
	bool autosuspend_ready;
};

struct tegra_bb_power_data {
	struct tegra_bb_power_gdata *gpio_data;
	struct tegra_bb_power_mdata *modem_data;
};

struct tegra_bb_callback {
	/* Init callback */
	cb_init init;
	/* Deinit callback */
	cb_deinit deinit;
	/* Powerstate transitions callback */
	cb_power power;
	/* Sysfs "load" callback */
	cb_attrib_access load;
	/* Sysfs "dlevel" callback */
	cb_attrib_access dlevel;
	/* USB notifier callback */
	cb_usbnotify usbnotify;
	/* PM notifier callback */
	cb_pmnotify pmnotify;
	bool valid;
};

#ifdef CONFIG_TEGRA_BB_OEM1
extern void *bb_oem1_get_cblist(void);
#define OEM1_CB bb_oem1_get_cblist
#else
#define OEM1_CB NULL
#endif
