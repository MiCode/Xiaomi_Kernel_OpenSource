/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_BLUETOOTH_POWER_H
#define __LINUX_BLUETOOTH_POWER_H

/*
 * voltage regulator information required for configuring the
 * bluetooth chipset
 */
struct bt_power_vreg_data {
	/* voltage regulator handle */
	struct regulator *reg;
	/* regulator name */
	const char *name;
	/* voltage levels to be set */
	unsigned int low_vol_level;
	unsigned int high_vol_level;
	/* current level to be set */
	unsigned int load_uA;
	/*
	 * is set voltage supported for this regulator?
	 * false => set voltage is not supported
	 * true  => set voltage is supported
	 *
	 * Some regulators (like gpio-regulators, LVS (low voltage swtiches)
	 * PMIC regulators) dont have the capability to call
	 * regulator_set_voltage or regulator_set_optimum_mode
	 * Use this variable to indicate if its a such regulator or not
	 */
	bool set_voltage_sup;
	/* is this regulator enabled? */
	bool is_enabled;
};

struct bt_power_clk_data {
	/* clock regulator handle */
	struct clk *clk;
	/* clock name */
	const char *name;
	/* is this clock enabled? */
	bool is_enabled;
};

/*
 * Platform data for the bluetooth power driver.
 */
struct bluetooth_power_platform_data {
	/* Bluetooth reset gpio */
	int bt_gpio_sys_rst;
	struct device *slim_dev;
	/* VDDIO voltage regulator */
	struct bt_power_vreg_data *bt_vdd_io;
	/* VDD_PA voltage regulator */
	struct bt_power_vreg_data *bt_vdd_pa;
	/* VDD_LDOIN voltage regulator */
	struct bt_power_vreg_data *bt_vdd_ldo;
	/* VDD_XTAL voltage regulator */
	struct bt_power_vreg_data *bt_vdd_xtal;
	/* VDD_CORE voltage regulator */
	struct bt_power_vreg_data *bt_vdd_core;
	/* VDD_AON digital voltage regulator */
	struct bt_power_vreg_data *bt_vdd_aon;
	/* VDD_DIG digital voltage regulator */
	struct bt_power_vreg_data *bt_vdd_dig;
	/* VDD RFA1 digital voltage regulator */
	struct bt_power_vreg_data *bt_vdd_rfa1;
	/* VDD RFA2 digital voltage regulator */
	struct bt_power_vreg_data *bt_vdd_rfa2;
	/* VDD ASD digital voltage regulator */
	struct bt_power_vreg_data *bt_vdd_asd;
	/* Optional: chip power down gpio-regulator
	 * chip power down data is required when bluetooth module
	 * and other modules like wifi co-exist in a single chip and
	 * shares a common gpio to bring chip out of reset.
	 */
	struct bt_power_vreg_data *bt_chip_pwd;
	/* bluetooth reference clock */
	struct bt_power_clk_data *bt_chip_clk;
	/* Optional: Bluetooth power setup function */
	int (*bt_power_setup)(int id);
};

int bt_register_slimdev(struct device *dev);
int get_chipset_version(void);

#define BT_CMD_SLIM_TEST		0xbfac
#define BT_CMD_PWR_CTRL			0xbfad
#define BT_CMD_CHIPSET_VERS		0xbfae
#endif /* __LINUX_BLUETOOTH_POWER_H */
