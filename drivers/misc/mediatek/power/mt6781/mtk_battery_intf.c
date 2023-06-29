/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/types.h>
#include <mt-plat/mtk_battery.h>
#include <mt-plat/mtk_charger.h>
#include <mt-plat/mtk_boot.h>
#include <mtk_gauge_class.h>
#include <mtk_battery_internal.h>

int __attribute__((weak)) charger_get_vbus(void)
{
	return 4500;
}

#if (CONFIG_MTK_GAUGE_VERSION != 30)
signed int battery_get_bat_voltage(void)
{
	return 4000;
}

signed int battery_get_bat_current(void)
{
	return 0;
}

signed int battery_get_bat_current_mA(void)
{
	return 0;
}

signed int battery_get_soc(void)
{
	return 50;
}

signed int battery_get_uisoc(void)
{
	int boot_mode = get_boot_mode();

	if ((boot_mode == META_BOOT) ||
		(boot_mode == ADVMETA_BOOT) ||
		(boot_mode == FACTORY_BOOT) ||
		(boot_mode == ATE_FACTORY_BOOT))
		return 75;

	return 50;
}

signed int battery_get_bat_temperature(void)
{
	return 25;
}

signed int battery_get_ibus(void)
{
	return 0;
}

signed int battery_get_vbus(void)
{
	return 0;
}

signed int battery_get_bat_avg_current(void)
{
	return 0;
}
#else

signed int battery_get_bat_voltage(void)
{
	return pmic_get_battery_voltage();
}

signed int battery_get_FG_bat_voltage(void)
{
	int ret = 0;
	union power_supply_propval propval;
	struct power_supply *bms = NULL;
	/* Get BMS power supply */
	bms = power_supply_get_by_name("bms");
	if (!bms) {
		pr_err("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	ret = power_supply_get_property(bms,POWER_SUPPLY_PROP_VOLTAGE_NOW,&propval);
	if (ret < 0)
		pr_err("%s: psy type failed, ret = %d\n", __func__, ret);
	else
		pr_err("%s: current_now = %d\n", __func__, propval.intval);
	return propval.intval;
}
signed int battery_get_bat_current(void)
{
#if 0
	int curr_val;
	bool is_charging;

	is_charging = gauge_get_current(&curr_val);
	if (is_charging == false)
		curr_val = 0 - curr_val;
	return curr_val;
#else
	int ret = 0;
	union power_supply_propval propval;
	struct power_supply *bms = NULL;
	/* Get BMS power supply */
	bms = power_supply_get_by_name("bms");
	if (!bms) {
		pr_err("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	ret = power_supply_get_property(bms,POWER_SUPPLY_PROP_CURRENT_NOW,&propval);
	if (ret < 0)
		pr_err("%s: psy type failed, ret = %d\n", __func__, ret);
	else
		pr_err("%s: current_now = %d\n", __func__, propval.intval);
	return propval.intval;
#endif
}

signed int battery_get_bat_current_mA(void)
{
	return battery_get_bat_current() / 10;
}

signed int battery_get_soc(void)
{
#if 0
	struct mtk_battery *gm = get_mtk_battery();

	if (gm != NULL)
		return gm->soc;
	else
		return 50;
#else
	int ret = 0;
	union power_supply_propval propval;
	struct power_supply *bms = NULL;
	/* Get BMS power supply */
	bms = power_supply_get_by_name("bms");
	if (!bms) {
		pr_err("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}
	ret = power_supply_get_property(bms,POWER_SUPPLY_PROP_CAPACITY,&propval);
	if (ret < 0)
		pr_err("%s: psy type failed, ret = %d\n", __func__, ret);
	else
		pr_err("%s: capacity = %d\n", __func__, propval.intval);
	return propval.intval;
#endif
}

signed int battery_get_uisoc(void)
{
	int ret = 0;
	int boot_mode = get_boot_mode();
	union power_supply_propval propval;
	struct power_supply *bms = NULL;

	if ((boot_mode == META_BOOT) ||
		(boot_mode == ADVMETA_BOOT) ||
		(boot_mode == FACTORY_BOOT) ||
		(boot_mode == ATE_FACTORY_BOOT))
		return 75;

	bms = power_supply_get_by_name("bms");
	if (!bms) {
		pr_err("%s %d: get power supply failed!\n", __func__, __LINE__);
		return 50;
	}

	ret = power_supply_get_property(bms,POWER_SUPPLY_PROP_CAPACITY,&propval);
	if (ret < 0){
		pr_err("%s %d: psy type failed, ret = %d, default uisoc:50!\n", __func__, __LINE__, ret);
		return 50;
	}else{
		pr_err("%s: capacity = %d\n", __func__, propval.intval);
		return propval.intval;
	}
}

signed int battery_get_bat_temperature(void)
{
#if 0
	/* TODO */
	if (is_battery_init_done())
		return force_get_tbat(true);
	else
		return -127;
#else
#ifdef CONFIG_MTK_DISABLE_TEMP_PROTECT
	return 25;
#else
	int ret = 0;
	union power_supply_propval propval;
	struct power_supply *bms = NULL;
	/* Get BMS power supply */
	bms = power_supply_get_by_name("bms");
	if (!bms) {
		pr_err("%s: get power supply failed\n", __func__);
		return 25;
	}
	ret = power_supply_get_property(bms,POWER_SUPPLY_PROP_TEMP,&propval);
	if (ret < 0)
		pr_err("%s: psy type failed, ret = %d\n", __func__, ret);
	else
		pr_err("%s: temp = %d\n", __func__, propval.intval);
	return propval.intval / 10;
#endif
#endif
}

signed int battery_get_ibus(void)
{
	return pmic_get_ibus();
}

signed int battery_get_vbus(void)
{
	return pmic_get_vbus();
}

signed int battery_get_bat_avg_current(void)
{
	bool valid;

	return gauge_get_average_current(&valid);
}

/* 4.9 already remove, only leave in 4.4 */
signed int battery_get_bat_uisoc(void)
{
	return battery_get_uisoc();
}

signed int battery_get_bat_soc(void)
{
	return battery_get_soc();
}

bool battery_get_bat_current_sign(void)
{
	int curr_val;

	return gauge_get_current(&curr_val);
}

int get_ui_soc(void)
{
	return battery_get_uisoc();
}

signed int battery_get_bat_avg_voltage(void)
{
	return gauge_get_nag_vbat();
}

signed int battery_meter_get_battery_current(void)
{
	return battery_get_bat_current();
}

bool battery_meter_get_battery_current_sign(void)
{
	return battery_get_bat_current_sign();
}

signed int battery_meter_get_battery_temperature(void)
{
	return battery_get_bat_temperature();
}

signed int battery_meter_get_charger_voltage(void)
{
	return battery_get_vbus();
}

unsigned long BAT_Get_Battery_Current(int polling_mode)
{
	return (long)battery_get_bat_avg_current();
}

unsigned long BAT_Get_Battery_Voltage(int polling_mode)
{
	long int ret;

	ret = (long)battery_get_bat_voltage();
	return ret;
}

unsigned int bat_get_ui_percentage(void)
{
	return battery_get_uisoc();
}

unsigned int battery_get_is_kpoc(void)
{
	return is_kernel_power_off_charging();
}

bool battery_is_battery_exist(void)
{
	return pmic_is_battery_exist();
}


void wake_up_bat(void)
{
}
EXPORT_SYMBOL(wake_up_bat);
/* end of legacy API */


/* GM25 only */
int en_intr_VBATON_UNDET(int en)
{
	return 0;
}

int reg_VBATON_UNDET(void (*callback)(void))
{
	return 0;
}

#endif
