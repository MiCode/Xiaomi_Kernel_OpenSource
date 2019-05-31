/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#ifndef __MTK_BATTERY_INTF_H__
#define __MTK_BATTERY_INTF_H__

#include <linux/alarmtimer.h>
#include <linux/hrtimer.h>
#include <linux/nvmem-consumer.h>
#include <linux/power_supply.h>
#include <linux/sysfs.h>
#include <linux/wait.h>

#define GAUGE_SYSFS_FIELD_RW(_name, _prop)	\
{									 \
	.attr   = __ATTR(_name, 0644, gauge_sysfs_show, gauge_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
	.get	= _name##_get,						\
}

#define GAUGE_SYSFS_FIELD_RO(_name, _prop)	\
{		\
	.attr   = __ATTR(_name, 0444, gauge_sysfs_show, gauge_sysfs_store),\
	.prop   = _prop,				  \
	.get	= _name##_get,						\
}

#define GAUGE_SYSFS_FIELD_WO(_name, _prop)	\
{								   \
	.attr	= __ATTR(_name, 0200, gauge_sysfs_show, gauge_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
}

#define GAUGE_SYSFS_INFO_FIELD_RW(_name, _prop)	\
{									 \
	.attr   = __ATTR(_name, 0644, gauge_sysfs_show, gauge_sysfs_store),\
	.prop	= _prop,	\
	.set	= info_set,	\
	.get	= info_get,	\
}

enum gauge_property {
	GAUGE_PROP_INITIAL,
	GAUGE_PROP_BATTERY_CURRENT,
	GAUGE_PROP_COULOMB,
	GAUGE_PROP_COULOMB_HT_INTERRUPT,
	GAUGE_PROP_COULOMB_LT_INTERRUPT,
	GAUGE_PROP_BATTERY_EXIST,
	GAUGE_PROP_HW_VERSION,
	GAUGE_PROP_BATTERY_VOLTAGE,
	GAUGE_PROP_BATTERY_TEMPERATURE_ADC,
	GAUGE_PROP_BIF_VOLTAGE,
	GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT,
	GAUGE_PROP_EN_LOW_VBAT_INTERRUPT,
	GAUGE_PROP_VBAT_HT_INTERRUPT,
	GAUGE_PROP_VBAT_LT_INTERRUPT,
	GAUGE_PROP_RTC_UI_SOC,
	GAUGE_PROP_PTIM_BATTERY_VOLTAGE,
	GAUGE_PROP_2SEC_REBOOT,
	GAUGE_PROP_PL_CHARGING_STATUS,
	GAUGE_PROP_MONITER_PLCHG_STATUS,
	GAUGE_PROP_BAT_PLUG_STATUS,
	GAUGE_PROP_IS_NVRAM_FAIL_MODE,
	GAUGE_PROP_MONITOR_SOFF_VALIDTIME,
	GAUGE_PROP_CON0_SOC,
	GAUGE_PROP_SHUTDOWN_CAR,
	GAUGE_PROP_CAR_TUNE_VALUE,
	GAUGE_PROP_R_FG_VALUE,
	GAUGE_PROP_VBAT2_DETECT_TIME,
	GAUGE_PROP_VBAT2_DETECT_COUNTER,
};

struct gauge_hw_status {

	/* hwocv related */
	int hw_ocv;
	int sw_ocv;
	bool flag_hw_ocv_unreliable;

	/* nafg info */
	signed int sw_car_nafg_cnt;
	signed int sw_car_nafg_dltv;
	signed int sw_car_nafg_c_dltv;

	/* ivag intr en/disable for hal */
	int iavg_intr_flag;
	int iavg_lt;
	int iavg_ht;

	/* boot status */
	int pl_charger_status; /* for GM2.5 */

	int gspare0_reg, gspare3_reg;
	int rtc_invalid;
	int is_bat_plugout;
	int bat_plug_out_time;

	/* PCB related */
	int r_fg_value;
	int car_tune_value;

	/* hw setting */
	int vbat2_det_time;
	int vbat2_det_counter;

};

enum gauge_hw_version {
	GAUGE_HW_V1000 = 1000,
	GAUGE_HW_V2000 = 2000,
	GAUGE_HW_V2001 = 2001,

	GAUGE_HW_MAX
};

struct mtk_gauge {
	struct mt6397_chip *chip;
	struct regmap *regmap;
	struct platform_device *pdev;
	struct mutex ops_lock;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;

	struct mtk_battery *gm;

	struct gauge_hw_status hw_status;

	int coulomb_h_irq;
	int coulomb_l_irq;
	int vbat_h_irq;
	int vbat_l_irq;

	bool vbat_l_en;
	bool vbat_h_en;

	struct iio_channel *chan_bat_temp;
	struct iio_channel *chan_bat_voltage;
	struct iio_channel *chan_bif;
	struct iio_channel *chan_ptim_bat_voltage;

	struct mtk_gauge_sysfs_field_info *attr;

};

struct mtk_gauge_sysfs_field_info {
	struct device_attribute attr;
	enum gauge_property prop;
	int (*set)(struct mtk_gauge *gauge,
		struct mtk_gauge_sysfs_field_info *attr, int val);
	int (*get)(struct mtk_gauge *gauge,
		struct mtk_gauge_sysfs_field_info *attr, int *val);
};

#endif /* __MTK_BATTERY_INTF_H__ */
