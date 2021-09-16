// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include "conn_power_throttling.h"
#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
#include <mtk_low_battery_throttling.h>
#endif

static unsigned int g_enable = 1;
static unsigned int g_radio_pwr_level[CONN_PWR_DRV_MAX];
static unsigned int g_platform_pwr_level[CONN_PWR_PLAT_MAX];
static struct conn_pwr_event_max_temp g_thermal_info;
static unsigned int g_last_temp;
spinlock_t pwr_core_lock;

#define CONN_PWR_THERMAL_RECOVERY_INTERVAL 10
#define CONN_PWR_THERMAL_HIGHER_INTERVAL 10
#define CONN_PWR_THERMAL_HIGHEST_INTERVAL 20
#define CONN_PWR_CONNSYS_MAX_THERMAL 100

int conn_pwr_core_init(void)
{
	pr_info("%s\n", __func__);

	memset(g_radio_pwr_level, CONN_PWR_THR_LV_0, sizeof(g_radio_pwr_level));
	memset(g_platform_pwr_level, CONN_PWR_THR_LV_0, sizeof(g_platform_pwr_level));
	g_thermal_info.max_temp = CONN_PWR_CONNSYS_MAX_THERMAL;
	g_thermal_info.recovery_temp =
		CONN_PWR_CONNSYS_MAX_THERMAL - CONN_PWR_THERMAL_RECOVERY_INTERVAL;

	spin_lock_init(&pwr_core_lock);

	return 0;
}

int conn_pwr_core_enable(int enable)
{
	int i, default_lv = 0;

	if (enable) {
		g_enable = 1;
	} else {
		for (i = 0; i < CONN_PWR_DRV_MAX; i++) {
			if (conn_pwr_get_drv_status(i) == CONN_PWR_DRV_STATUS_ON &&
				g_radio_pwr_level[i] != default_lv) {
				conn_pwr_notify_event(i, CONN_PWR_EVENT_LEVEL, &default_lv);
			}
			g_radio_pwr_level[i] = default_lv;
		}
		g_enable = 0;
	}

	pr_info("%s\n enable = %d", __func__, enable);

	return 0;
}

int conn_pwr_core_resume(void)
{
	pr_info("%s low_battery=%d, thermal=%d, customer=0x%08x\n", __func__,
			g_platform_pwr_level[CONN_PWR_PLAT_LOW_BATTERY],
			g_platform_pwr_level[CONN_PWR_PLAT_THERMAL],
			g_platform_pwr_level[CONN_PWR_PLAT_CUSTOMER]);
	pr_info("%s bt=%d, FM=%d, GPS=%d, Wi-Fi=%d\n", __func__, g_radio_pwr_level[0],
			g_radio_pwr_level[1], g_radio_pwr_level[2], g_radio_pwr_level[3]);

	return 0;
}

int conn_pwr_core_suspend(void)
{
	return 0;
}

int conn_pwr_get_low_battery_level(struct conn_pwr_update_info *info)
{
	int ret = CONN_PWR_THR_LV_0;
	int low_battery_power_level = 0;

	if (conn_pwr_get_plat_level(CONN_PWR_PLAT_LOW_BATTERY, &low_battery_power_level) != 0) {
		pr_info("%s conn_pwr cant get battery level\n", __func__);
		return ret;
	}

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
	if (conn_pwr_get_drv_status(CONN_PWR_DRV_WIFI) == CONN_PWR_DRV_STATUS_ON ||
		(info->reason == CONN_PWR_ARB_SUBSYS_ON_OFF &&
		 info->drv == CONN_PWR_DRV_WIFI && info->status == CONN_PWR_DRV_STATUS_ON)) {
		switch (low_battery_power_level) {
		case LOW_BATTERY_LEVEL_0:
			break;
		case LOW_BATTERY_LEVEL_1:
		case LOW_BATTERY_LEVEL_2:
			ret = CONN_PWR_THR_LV_4;
			break;
		default:
			break;
		}
	}
#endif

	return ret;
}

int conn_pwr_get_thermal_level(struct conn_pwr_update_info *info, int current_temp)
{
	int ret = CONN_PWR_THR_LV_0;
	int thermal_max_level = 0;

	if (conn_pwr_get_plat_level(CONN_PWR_PLAT_THERMAL, &thermal_max_level) != 0) {
		pr_info("%s conn_pwr cant get thermal level\n", __func__);
		return ret;
	}

	g_thermal_info.max_temp = thermal_max_level;
	g_thermal_info.recovery_temp = thermal_max_level - CONN_PWR_THERMAL_RECOVERY_INTERVAL;

	if (current_temp > thermal_max_level) {
		pr_info("%s update param = %d, %d, %d\n", __func__,
				current_temp, thermal_max_level, g_last_temp);
		if (current_temp < (thermal_max_level + CONN_PWR_THERMAL_HIGHER_INTERVAL) ||
			g_radio_pwr_level[CONN_PWR_DRV_WIFI] < CONN_PWR_THR_LV_2 ||
			(g_radio_pwr_level[CONN_PWR_DRV_WIFI] < CONN_PWR_THR_LV_4 &&
			 current_temp <= g_last_temp)) {
			ret = CONN_PWR_THR_LV_2;
		} else if (current_temp < (thermal_max_level + CONN_PWR_THERMAL_HIGHEST_INTERVAL) ||
			g_radio_pwr_level[CONN_PWR_DRV_WIFI] < CONN_PWR_THR_LV_4 ||
			(g_radio_pwr_level[CONN_PWR_DRV_WIFI] < CONN_PWR_THR_LV_5 &&
			 current_temp <= g_last_temp)) {
			ret = CONN_PWR_THR_LV_4;
		} else {
			ret = CONN_PWR_THR_LV_5;
		}
		g_last_temp = current_temp;
	} else if (current_temp < (thermal_max_level - CONN_PWR_THERMAL_RECOVERY_INTERVAL)) {
		pr_info("%s recovery param = %d, %d, %d\n", __func__,
				current_temp, thermal_max_level, g_last_temp);
		ret = CONN_PWR_THR_LV_0;
		g_last_temp = current_temp;
	} else {
		ret = g_radio_pwr_level[CONN_PWR_DRV_WIFI];
	}

	return ret;
}

int conn_pwr_set_level(struct conn_pwr_update_info *info, int radio_power_level[], int current_temp)
{
	int customer_level = 0;
	int radio_value = 0;
	int i;

	switch (info->reason) {
	case CONN_PWR_ARB_SUBSYS_ON_OFF:
		if (info->drv == CONN_PWR_DRV_WIFI) {
			g_platform_pwr_level[CONN_PWR_PLAT_LOW_BATTERY] =
				conn_pwr_get_low_battery_level(info);
		}
		break;
	case CONN_PWR_ARB_LOW_BATTERY:
		g_platform_pwr_level[CONN_PWR_PLAT_LOW_BATTERY] =
			conn_pwr_get_low_battery_level(info);
		break;
	case CONN_PWR_ARB_THERMAL:
	case CONN_PWR_ARB_TEMP_CHECK:
		g_platform_pwr_level[CONN_PWR_PLAT_THERMAL] =
			conn_pwr_get_thermal_level(info, current_temp);
		break;
	default:
		break;
	}

	if (conn_pwr_get_plat_level(CONN_PWR_PLAT_CUSTOMER, &customer_level) != 0)
		customer_level = 0;

	g_platform_pwr_level[CONN_PWR_PLAT_CUSTOMER] = customer_level;

	for (i = 0; i < CONN_PWR_DRV_MAX; i++) {
		radio_power_level[i] = g_platform_pwr_level[CONN_PWR_PLAT_LOW_BATTERY];
		if (i == CONN_PWR_DRV_WIFI &&
			(radio_power_level[i] < g_platform_pwr_level[CONN_PWR_PLAT_THERMAL]))
			radio_power_level[i] = g_platform_pwr_level[CONN_PWR_PLAT_THERMAL];

		radio_value =
			CONN_PWR_GET_CUSTOMER_POWER_LEVEL(
				g_platform_pwr_level[CONN_PWR_PLAT_CUSTOMER], i);
		if (radio_power_level[i] < radio_value && radio_value < CONN_PWR_LOW_BATTERY_MAX)
			radio_power_level[i] = radio_value;
	}

	pr_info("%s low_battery=%d, thermal=%d, customer=0x%08x\n", __func__,
		g_platform_pwr_level[CONN_PWR_PLAT_LOW_BATTERY],
		g_platform_pwr_level[CONN_PWR_PLAT_THERMAL],
		g_platform_pwr_level[CONN_PWR_PLAT_CUSTOMER]);

	return 0;
}

int conn_pwr_get_drv_level(enum conn_pwr_drv_type type, enum conn_pwr_low_battery_level *level)
{
	if (level != NULL && type < CONN_PWR_DRV_MAX && type >= 0) {
		*level = g_radio_pwr_level[type];
		return 0;
	} else {
		return -1;
	}
}
EXPORT_SYMBOL(conn_pwr_get_drv_level);

int conn_pwr_get_platform_level(enum conn_pwr_plat_type type, int *level)
{
	if (level != NULL && type < CONN_PWR_PLAT_MAX && type >= 0) {
		*level = g_platform_pwr_level[type];
		return 0;
	} else {
		return -1;
	}
}
EXPORT_SYMBOL(conn_pwr_get_platform_level);

int conn_pwr_get_thermal(struct conn_pwr_event_max_temp *temp)
{
	if (temp == NULL)
		return -1;

	temp->max_temp = g_thermal_info.max_temp;
	temp->recovery_temp = g_thermal_info.recovery_temp;

	return 0;
}
EXPORT_SYMBOL(conn_pwr_get_thermal);

int conn_pwr_arbitrate(struct conn_pwr_update_info *info)
{
	int radio_power_level[CONN_PWR_DRV_MAX] = {CONN_PWR_THR_LV_0};
	int i;
	int current_temp = 0;
	int adie;
	unsigned long flag;

	if (!g_enable) {
		pr_info("%s disable\n", __func__);
		return 0;
	}

	if (info == NULL)
		return -1;

	adie = conn_pwr_get_adie_id();
	if (adie != 0x6637) {
		pr_info("%s no support 0x%x", __func__, adie);
		return 0;
	}

	if (info->reason == CONN_PWR_ARB_SUBSYS_ON_OFF) {
		if (info->drv == CONN_PWR_DRV_WIFI) {
			int battery_lv = 0;

			if (conn_pwr_get_plat_level(CONN_PWR_PLAT_LOW_BATTERY, &battery_lv) != 0) {
				pr_info("%s conn_pwr cant get battery level\n", __func__);
				battery_lv = 0;
			}
			if (battery_lv == 0) {
				pr_info("%s reason=%d, low battery level is 0\n", __func__,
						info->reason);
				return 0;
			}
		} else {
			pr_info("%s reason=%d, no need updated\n", __func__, info->reason);
			return 0;
		}
	}

	if (info->reason == CONN_PWR_ARB_THERMAL)
		conn_pwr_get_temp(&current_temp, 0);
	else if (info->reason == CONN_PWR_ARB_TEMP_CHECK)
		conn_pwr_get_temp(&current_temp, 1);

	spin_lock_irqsave(&pwr_core_lock, flag);
	conn_pwr_set_level(info, radio_power_level, current_temp);

	if (info->reason == CONN_PWR_ARB_SUBSYS_ON_OFF) {
		for (i = 0; i < CONN_PWR_DRV_MAX; i++) {
			if (info->drv != i &&
				conn_pwr_get_drv_status(i) == CONN_PWR_DRV_STATUS_ON &&
				g_radio_pwr_level[i] != radio_power_level[i])
				conn_pwr_notify_event(i, CONN_PWR_EVENT_LEVEL,
					&radio_power_level[i]);

			g_radio_pwr_level[i] = radio_power_level[i];
		}
	} else {
		for (i = 0; i < CONN_PWR_DRV_MAX; i++) {
			if (conn_pwr_get_drv_status(i) == CONN_PWR_DRV_STATUS_ON &&
				g_radio_pwr_level[i] != radio_power_level[i])
				conn_pwr_notify_event(i, CONN_PWR_EVENT_LEVEL,
					&radio_power_level[i]);

			g_radio_pwr_level[i] = radio_power_level[i];
		}

		if ((info->reason == CONN_PWR_ARB_THERMAL ||
			info->reason == CONN_PWR_ARB_TEMP_CHECK) &&
			conn_pwr_get_drv_status(CONN_PWR_DRV_WIFI) == CONN_PWR_DRV_STATUS_ON)
			conn_pwr_notify_event(CONN_PWR_DRV_WIFI,
				CONN_PWR_EVENT_MAX_TEMP, &g_thermal_info);
	}

	pr_info("%s reason=%d, bt=%d, FM=%d, GPS=%d, Wi-Fi=%d\n", __func__,
			info->reason, g_radio_pwr_level[0],
		g_radio_pwr_level[1], g_radio_pwr_level[2], g_radio_pwr_level[3]);

	spin_unlock_irqrestore(&pwr_core_lock, flag);

	return 0;
}

int conn_pwr_report_level_required(enum conn_pwr_drv_type type,
					enum conn_pwr_low_battery_level level)
{
	return 0;
}
EXPORT_SYMBOL(conn_pwr_report_level_required);
