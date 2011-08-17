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


#ifndef __PMIC8058_MISC_H__
#define __PMIC8058_MISC_H__

enum pm8058_vib_en_mode {
	PM8058_VIB_MANUAL,
	PM8058_VIB_DTEST1,
	PM8058_VIB_DTEST2,
	PM8058_VIB_DTEST3
};

enum pm8058_coincell_chg_voltage {
	PM8058_COINCELL_VOLTAGE_3p2V = 1,
	PM8058_COINCELL_VOLTAGE_3p1V,
	PM8058_COINCELL_VOLTAGE_3p0V,
	PM8058_COINCELL_VOLTAGE_2p5V = 16
};

enum pm8058_coincell_chg_resistor {
	PM8058_COINCELL_RESISTOR_2100_OHMS,
	PM8058_COINCELL_RESISTOR_1700_OHMS,
	PM8058_COINCELL_RESISTOR_1200_OHMS,
	PM8058_COINCELL_RESISTOR_800_OHMS
};

enum pm8058_coincell_chg_state {
	PM8058_COINCELL_CHG_DISABLE,
	PM8058_COINCELL_CHG_ENABLE
};

struct pm8058_vib_config {
	u16			drive_mV;
	u8			active_low;
	enum pm8058_vib_en_mode	enable_mode;
};

struct pm8058_coincell_chg_config {
	enum pm8058_coincell_chg_state		state;
	enum pm8058_coincell_chg_voltage	voltage;
	enum pm8058_coincell_chg_resistor	resistor;
};

int pm8058_vibrator_config(struct pm8058_vib_config *vib_config);
int pm8058_coincell_chg_config(struct pm8058_coincell_chg_config *chg_config);

#endif /* __PMIC8058_MISC_H__ */
