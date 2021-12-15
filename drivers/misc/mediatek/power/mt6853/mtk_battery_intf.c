/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (C) 2021 MediaTek Inc.
*/
#include <linux/types.h>
#include <mt-plat/v1/mtk_battery.h>
#include <mt-plat/v1/mtk_charger.h>
#include <mt-plat/mtk_boot.h>
#include <mtk_gauge_class.h>
#include <mtk_battery_internal.h>

#ifdef CONFIG_CUSTOM_BATTERY_EXTERNAL_CHANNEL
#include <custome_external_battery.h>
#endif
#include <linux/of.h>

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

signed int battery_get_uisoc(void)
{
	struct mtk_battery *gm = get_mtk_battery();
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	if (gm != NULL){
		boot_node = gm->pdev_node;
		if (boot_node != NULL){
			tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
			if (tag != NULL){
				/*
				NORMAL_BOOT = 0,META_BOOT = 1,
				FACTORY_BOOT = 4,ADVMETA_BOOT = 5,ATE_FACTORY_BOOT = 6,
				*/
				if ((tag->bootmode == 1) ||
					(tag->bootmode == 5) ||
					(tag->bootmode == 4) ||
					(tag->bootmode == 6))
					return 75;
				else if (tag->bootmode == 0)
					return gm->ui_soc;
			}
		}
		return 50;
	}
	else
		return 50;
}

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

/*
signed int battery_get_uisoc(void)
{
	int boot_mode = get_boot_mode();

	if ((boot_mode == META_BOOT) ||
		(boot_mode == ADVMETA_BOOT) ||
		(boot_mode == FACTORY_BOOT) ||
		(boot_mode == ATE_FACTORY_BOOT))
		return 75;

	return 50;
}*/

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

	#ifdef CONFIG_CUSTOM_BATTERY_EXTERNAL_CHANNEL
_CODE_DEFINEDE
	#else

signed int battery_get_bat_voltage(void)
{
	return pmic_get_battery_voltage();
}

signed int battery_get_bat_current(void)
{
	int curr_val;
	bool is_charging;

	is_charging = gauge_get_current(&curr_val);
	if (is_charging == false)
		curr_val = 0 - curr_val;
	return curr_val;
}

signed int battery_get_bat_current_mA(void)
{
	return battery_get_bat_current() / 10;
}

signed int battery_get_soc(void)
{
	struct mtk_battery *gm = get_mtk_battery();

	if (gm != NULL)
		return gm->soc;
	else
		return 50;
}

/*
signed int battery_get_uisoc(void)
{
	int boot_mode = get_boot_mode();
	struct mtk_battery *gm = get_mtk_battery();

	if ((boot_mode == META_BOOT) ||
		(boot_mode == ADVMETA_BOOT) ||
		(boot_mode == FACTORY_BOOT) ||
		(boot_mode == ATE_FACTORY_BOOT))
		return 75;

	if (gm != NULL)
		return gm->ui_soc;
	else
		return 50;
}*/

signed int battery_get_bat_temperature(void)
{
	/* TODO */
	if (is_battery_init_done())
		return force_get_tbat(true);
	else
		return -127;
}

signed int battery_get_ibus(void)
{
	return pmic_get_ibus();
}

signed int battery_get_vbus(void)
{
	return charger_get_vbus();
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
#endif
