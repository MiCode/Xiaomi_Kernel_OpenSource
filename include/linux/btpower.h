/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_BLUETOOTH_POWER_H
#define __LINUX_BLUETOOTH_POWER_H

#include <linux/types.h>

/*
 * voltage regulator information required for configuring the
 * bluetooth chipset
 */
enum bt_power_modes {
	BT_POWER_DISABLE = 0,
	BT_POWER_ENABLE,
	BT_POWER_RETENTION
};

struct bt_power_vreg_data {
	struct regulator *reg;  /* voltage regulator handle */
	const char *name;       /* regulator name */
	u32 min_vol;            /* min voltage level */
	u32 max_vol;            /* max voltage level */
	u32 load_curr;          /* current */
	bool is_enabled;        /* is this regulator enabled? */
	bool is_retention_supp; /* does this regulator support retention mode */
};

struct bt_power_clk_data {
	struct clk *clk;  /* clock regulator handle */
	const char *name; /* clock name */
	bool is_enabled;  /* is this clock enabled? */
};

/*
 * Platform data for the bluetooth power driver.
 */
struct bluetooth_power_platform_data {
	int bt_gpio_sys_rst;                   /* Bluetooth reset gpio */
	struct device *slim_dev;
	struct bt_power_vreg_data *vreg_info;  /* VDDIO voltage regulator */
	struct bt_power_clk_data *bt_chip_clk; /* bluetooth reference clock */
	int (*bt_power_setup)(int id); /* Bluetooth power setup function */
};

int btpower_register_slimdev(struct device *dev);
int btpower_get_chipset_version(void);

#define BT_CMD_SLIM_TEST		0xbfac
#define BT_CMD_PWR_CTRL			0xbfad
#define BT_CMD_CHIPSET_VERS		0xbfae
#endif /* __LINUX_BLUETOOTH_POWER_H */
