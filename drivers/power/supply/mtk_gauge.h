/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#ifndef __MTK_GAUGE_INTF_H__
#define __MTK_GAUGE_INTF_H__

#include <linux/alarmtimer.h>
#include <linux/hrtimer.h>
#include <linux/nvmem-consumer.h>
#include <linux/power_supply.h>
#include <linux/sysfs.h>
#include <linux/wait.h>

#define CALI_CAR_TUNE_AVG_NUM   60

#define GAUGE_SYSFS_FIELD_RW(_name, _name_set, _name_get, _prop)	\
{									 \
	.attr   = __ATTR(_name, 0644, gauge_sysfs_show, gauge_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name_set,						\
	.get	= _name_get,						\
}

#define GAUGE_SYSFS_FIELD_RO(_name, _prop)	\
{		\
	.attr   = __ATTR(_name, 0444, gauge_sysfs_show, gauge_sysfs_store),\
	.prop   = _prop,				  \
	.get	= _name,						\
}

#define GAUGE_SYSFS_FIELD_WO(_name, _prop)	\
{								   \
	.attr	= __ATTR(_name, 0200, gauge_sysfs_show, gauge_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name,						\
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
	GAUGE_PROP_VBAT_HT_INTR_THRESHOLD,
	GAUGE_PROP_VBAT_LT_INTR_THRESHOLD,
	GAUGE_PROP_RTC_UI_SOC,
	GAUGE_PROP_PTIM_BATTERY_VOLTAGE,
	GAUGE_PROP_PTIM_RESIST,
	GAUGE_PROP_RESET,
	GAUGE_PROP_BOOT_ZCV,
	GAUGE_PROP_ZCV,
	GAUGE_PROP_ZCV_CURRENT,
	GAUGE_PROP_NAFG_CNT,
	GAUGE_PROP_NAFG_DLTV,
	GAUGE_PROP_NAFG_C_DLTV,
	GAUGE_PROP_NAFG_EN,
	GAUGE_PROP_NAFG_ZCV,
	GAUGE_PROP_NAFG_VBAT,
	GAUGE_PROP_RESET_FG_RTC,
	GAUGE_PROP_GAUGE_INITIALIZED,
	GAUGE_PROP_AVERAGE_CURRENT,
	GAUGE_PROP_BAT_PLUGOUT_EN,
	GAUGE_PROP_ZCV_INTR_THRESHOLD,
	GAUGE_PROP_ZCV_INTR_EN,
	GAUGE_PROP_SOFF_RESET,
	GAUGE_PROP_NCAR_RESET,
	GAUGE_PROP_BAT_CYCLE_INTR_THRESHOLD,
	GAUGE_PROP_HW_INFO,
	GAUGE_PROP_EVENT,
	GAUGE_PROP_EN_BAT_TMP_HT,
	GAUGE_PROP_EN_BAT_TMP_LT,
	GAUGE_PROP_BAT_TMP_HT_THRESHOLD,
	GAUGE_PROP_BAT_TMP_LT_THRESHOLD,

	GAUGE_PROP_2SEC_REBOOT,//bit info
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
	GAUGE_PROP_BAT_TEMP_FROZE_EN,
	GAUGE_PROP_BAT_EOC,
	GAUGE_PROP_MAX,
};

struct gauge_hw_status {

	/* hwocv related */
	int hw_ocv;
	int sw_ocv;
	bool flag_hw_ocv_unreliable;

	/* nafg info */
	int nafg_cnt;
	int nafg_dltv;
	int nafg_c_dltv;
	int nafg_c_dltv_th;
	int nafg_zcv;

	/* ivag intr en/disable for hal */
	int iavg_intr_flag;
	int iavg_lt;
	int iavg_ht;

	/* boot status */
	int pl_charger_status; /* for GM2.5 */

	u8 gspare0_reg, gspare3_reg;
	int rtc_invalid;
	int is_bat_plugout;
	int bat_plug_out_time;

	/* PCB related */
	int r_fg_value;
	int car_tune_value;
	int meta_current;
	int tmp_car_tune;

	/* hw setting */
	int vbat2_det_time;
	int vbat2_det_counter;

};

enum gauge_hw_version {
	GAUGE_NO_HW = 0,
	GAUGE_HW_V0500 = 500,
	GAUGE_HW_V1000 = 1000,
	GAUGE_HW_V1100 = 1100,
	GAUGE_HW_V2000 = 2000,
	GAUGE_HW_V2001 = 2001,

	GAUGE_HW_MAX
};

/* for gauge hal only */
struct gauge_hw_info_data {
	int current_1;
	int current_2;
	int current_avg;
	int current_avg_sign;
	int current_avg_valid;
	int car;
	int ncar;
	int time;
	int iavg_valid;

	int pmic_zcv;
	int pmic_zcv_rdy;
	int charger_zcv;
	int hw_zcv;
};

enum {
	FROM_SW_OCV = 1,
	FROM_PMIC_PLUG_IN,
	FROM_PMIC_PON_ON,
	FROM_CHR_IN
};

struct zcv_data {
	int charger_zcv;
	int pmic_in_zcv;
	int pmic_zcv;
	int pmic_rdy;
	int swocv;
	int zcv_from;
	int zcv_tmp;

	bool zcv_1st_read;
	int charger_zcv_1st;
	int pmic_in_zcv_1st;
	int pmic_zcv_1st;
	int pmic_rdy_1st;
	int swocv_1st;
	int zcv_from_1st;
	int zcv_tmp_1st;
	int moniter_plchg_bit;
	int pl_charging_status;
};

enum gauge_irq {
	COULOMB_H_IRQ,
	COULOMB_L_IRQ,
	VBAT_H_IRQ,
	VBAT_L_IRQ,
	NAFG_IRQ,
	BAT_PLUGOUT_IRQ,
	ZCV_IRQ,
	FG_N_CHARGE_L_IRQ,
	FG_IAVG_H_IRQ,
	FG_IAVG_L_IRQ,
	BAT_TMP_H_IRQ,
	BAT_TMP_L_IRQ,
	GAUGE_IRQ_MAX
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
	struct gauge_hw_info_data fg_hw_info;
	struct mutex fg_mutex;

	int irq_no[GAUGE_IRQ_MAX];

	bool vbat_l_en;
	bool vbat_h_en;

	struct iio_channel *chan_bat_temp;
	struct iio_channel *chan_bat_voltage;
	struct iio_channel *chan_bif;
	struct iio_channel *chan_ptim_bat_voltage;
	struct iio_channel *chan_ptim_r;

	struct mtk_gauge_sysfs_field_info *attr;
	struct zcv_data zcv_info;

	/* hw nafg */
	int nafg_corner;
	int nafg_zcv_mv;
	int nafg_c_dltv_mv;
	int zcv_reg;
	int thr_reg;

	/* sw nafg */
	int sw_nafg_vbat;
	int sw_nafg_en;
	int sw_nafg_cnt;
	int sw_nafg_zcv;
	int sw_nafg_c_dltv_threshold;
	int sw_nafg_dltv;
	int sw_nafg_c_dltv;
	int (*sw_nafg_irq)(struct mtk_battery *gm);

	/* sw vbat interrupt */
	int sw_vbat_h_en;
	int sw_vbat_l_en;
	int sw_vbat_h_cnt;
	int sw_vbat_l_cnt;
	int sw_vbat_h_threshold;
	int sw_vbat_l_threshold;
	int (*sw_vbat_h_irq)(struct mtk_battery *gm);
	int (*sw_vbat_l_irq)(struct mtk_battery *gm);

	/*thread*/
	wait_queue_head_t  wait_que;
	unsigned int gauge_update_flag;
	struct hrtimer gauge_hrtimer;

};

struct mtk_gauge_sysfs_field_info {
	struct device_attribute attr;
	enum gauge_property prop;
	int (*set)(struct mtk_gauge *gauge,
		struct mtk_gauge_sysfs_field_info *attr, int val);
	int (*get)(struct mtk_gauge *gauge,
		struct mtk_gauge_sysfs_field_info *attr, int *val);
};

#endif /* __MTK_GAUGE_INTF_H__ */
