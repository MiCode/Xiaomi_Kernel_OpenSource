/* Copyright (c) 2015 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[xmusb350] %s: " fmt, __func__

#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
#include <linux/firmware.h>

#include <mt-plat/charger_class.h>
#include <mt-plat/charger_type.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_boot.h>

#include "mtk_charger_intf.h"
#include "mtk_charger_init.h"

#include <linux/stat.h>
#include <linux/ctype.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>	/*irq_to_desc*/
#include <linux/vmalloc.h>
#include <linux/preempt.h>
#include <linux/time.h>

#define xm35_info	pr_info
#define xm35_dbg	pr_err//pr_debug
#define xm35_err	pr_err
#define xm35_log	pr_err

#define INJOINIC_VID		0x49333530
#define WELTREND_VID		0x584d3335
#define FIRMWARE_FILE_LENGTH			15000
#define ERROR_REQUESET_FIRMWARE			-1
#define I350_FW_NAME		"I350_SNK_FIREWARE.bin"

/* only write registers */
#define XMUSB350_REG_RERUN_APSD		0x01// rerun apsd
/*
0x01: Quick Charge 2.0, 5V
0x02: Quick Charge 2.0, 9V
0x03: Quick Charge 2.0, 12V
0x04: Quick Charge 3.0, 5V
0x05: Quick Charge 3+, 5V
*/
#define XMUSB350_REG_QC_MS			0x02// QC mode selection
#define XMUSB350_REG_INTB_EN		0x03// Int enable[00: Disable, 01: Enable]
#define XMUSB350_REG_SOFT_RESET		0x04// Soft reset
#define XMUSB350_REG_HVDCP_EN		0x05// hvdcp enable[00: Disable, 01: Enable]
#define XMUSB350_REG_BC12_EN		0x06// bc12 enable[00: Disable, 01: Enable]
#define XMUSB350_REG_SLEEP_EN		0x07// Sleep enable[00: Disable, 01: Enable]
/*
Bit15-0 = {Data1, Data2}
Bit15: 0 DM, 1 DP
Bit14-Bit0: Number of Pules
*/
#define XMUSB350_REG_QC30_PM		0x73// Quick Charge 3.0 Pulse Mode, 200mV/Step
#define XMUSB350_REG_QC3_PLUS_PM	0x83// Quick Charge 3+ Pulse Mode, 20mV/Step

/* only Read registers */
#define XMUSB350_REG_CHGT_ERROR		0x11// Charge Type and Error Status
#define XMUSB350_REG_VIN		0x12// VIN Voltage= Data Value x 100mV
#define XMUSB350_REG_VID		0x13// Vendor ID
#define XMUSB350_REG_VERSION		0x14// HW VERSION + FW VERSION

/* registers parameter */
// mode
#define XMUSB350_MODE_QC20_V5			0x01
#define XMUSB350_MODE_QC20_V9			0x02
#define XMUSB350_MODE_QC20_V12			0x03
#define XMUSB350_MODE_QC30_V5			0x04
#define XMUSB350_MODE_QC3_PLUS_V5		0x05

#define ENABLE		0x01
#define DISABLE		0x00

#define QC35_DPDM	0x01
#define QC30_DPDM	0x00

#define RECHECK_DELAY		5000
#define MAX_RECHECK_COUNT	4

static const enum power_supply_type const xmusb350_apsd_results[] = {
	POWER_SUPPLY_TYPE_UNKNOWN,
	POWER_SUPPLY_TYPE_USB,
	POWER_SUPPLY_TYPE_USB_CDP,
	POWER_SUPPLY_TYPE_USB_FLOAT,
	POWER_SUPPLY_TYPE_USB_DCP,
};

enum thermal_status_levels {
	TEMP_SHUT_DOWN = 0,
	TEMP_SHUT_DOWN_SMB,
	TEMP_ALERT_LEVEL,
	TEMP_ABOVE_RANGE,
	TEMP_WITHIN_RANGE,
	TEMP_BELOW_RANGE,
};

enum usbsw_state {
	USBSW_CHG = 0,
	USBSW_USB,
};

/* QC35 Error State */
enum qc35_error_state {
	QC35_ERROR_NO,
	QC35_ERROR_QC30_DET,
	QC35_ERROR_TA_DET,
	QC35_ERROR_TA_CAP,
	QC35_QC3_PLUS_DET_OK,
};

struct xmusb350_charger {
	struct i2c_client		*client;
	struct device			*dev;
	struct xmusb350_desc		*desc;
	struct charger_device		*chg_dev;
	struct charger_device		*chg_dev_p;
	struct charger_properties	chg_props;
	struct charger_consumer		*chg_consumer;
	struct mt_charger		*mt_chg;


	bool				tcpc_attach;
	bool				otg_enable;
	bool				hvdcp_dpdm_status;
	bool				vbus_disable;
	bool				bc12_unsupported;
	bool				hvdcp_unsupported;
	bool				intb_unsupported;
	bool				sleep_unsupported;
	bool				distinguish_qc3;
	bool				irq_waiting;
	bool				resume_completed;
	bool				hvdcp_en;
	bool				irq_data_process_enable;
	bool				hvdcp_trig_reset;
	bool				hvdcp_det_lock;
	bool				check_overload;
	bool				fw_update_flag;

	enum charger_type		last_chg_type;
	enum charger_type		chg_type;
	enum thermal_status_levels	thermal_status;

	int				connector_temp;
	int				dpdm_mode;
	int				qc35_chg_type;
	int				qc35_err_sta;
	int				pulse_cnt;
	int				entry_time;
	int				rerun_apsd_count;
	int				ocp_timer;
	int				hvdcp_retry_timer;
	int				qc3_det_err_timer;
	int				usb_switch;
	int				recheck_count;

	struct delayed_work		conn_therm_work;
	struct delayed_work		charger_type_det_work;
	struct delayed_work		charger_type_recheck_work;
	struct delayed_work		fw_update_work;

	/* psy */
	struct power_supply		*usb_psy;
	struct power_supply		*bms_psy;
	struct power_supply		*main_psy;
	struct power_supply		*batt_psy;
	struct power_supply		*charger_psy;
	struct power_supply		*charger_identify_psy;
	struct power_supply_desc	charger_identify_psy_d;

	/* pinctrl */
	struct pinctrl			*xmusb350_pinctrl;
	struct pinctrl_state		*pinctrl_state_normal;
	struct pinctrl_state		*pinctrl_state_isp;
	int				xmusb350_sda_gpio;
	int				xmusb350_scl_gpio;
	int				xmusb350_rst_gpio;
	unsigned int			connect_therm_gpio;

	struct mutex			chgdet_lock;
	struct mutex			irq_complete;
	struct mutex			isp_sequence_lock;
};

struct xmusb350_desc {
	bool en_bc12;
	bool en_hvdcp;
	bool en_intb;
	bool en_sleep;
	const char *chg_dev_name;
	const char *alias_name;
};

struct charger_type_desc {
	int psy_type;
	int chg_type;
	int usb_switch;
};

uint32_t *file_data;
uint32_t *file_date_w;
uint32_t file_size;

static struct charger_type_desc charger_type_table[] = {
	[QC35_NA]		= {POWER_SUPPLY_TYPE_UNKNOWN,		CHARGER_UNKNOWN,		USBSW_CHG},
	[QC35_OCP]		= {POWER_SUPPLY_TYPE_USB_DCP,		STANDARD_CHARGER,		USBSW_CHG},
	[QC35_FLOAT]		= {POWER_SUPPLY_TYPE_USB_FLOAT,		NONSTANDARD_CHARGER,		USBSW_CHG},
	[QC35_SDP]		= {POWER_SUPPLY_TYPE_USB,		STANDARD_HOST,			USBSW_USB},
	[QC35_CDP]		= {POWER_SUPPLY_TYPE_USB_CDP,		CHARGING_HOST,			USBSW_USB},
	[QC35_DCP]		= {POWER_SUPPLY_TYPE_USB_DCP,		STANDARD_CHARGER,		USBSW_CHG},
	[QC35_HVDCP_20]		= {POWER_SUPPLY_TYPE_USB_HVDCP,		STANDARD_CHARGER,		USBSW_CHG},
	[QC35_HVDCP_30]		= {POWER_SUPPLY_TYPE_USB_HVDCP_3,	STANDARD_CHARGER,		USBSW_CHG},
	[QC35_HVDCP_3_PLUS_18]	= {POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS,	STANDARD_CHARGER,		USBSW_CHG},
	[QC35_HVDCP_3_PLUS_27]	= {POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS,	STANDARD_CHARGER,		USBSW_CHG},
	[QC35_HVDCP_30_18]	= {POWER_SUPPLY_TYPE_USB_HVDCP_3,	STANDARD_CHARGER,		USBSW_CHG},
	[QC35_HVDCP_30_27]	= {POWER_SUPPLY_TYPE_USB_HVDCP_3,	STANDARD_CHARGER,		USBSW_CHG},
	[QC35_PD]		= {POWER_SUPPLY_TYPE_USB_PD,		CHARGING_HOST,			USBSW_USB},
	[QC35_PD_DR]		= {POWER_SUPPLY_TYPE_USB_PD,		CHARGING_HOST,			USBSW_USB},
	[QC35_HVDCP]		= {POWER_SUPPLY_TYPE_USB_HVDCP,		STANDARD_CHARGER,		USBSW_CHG},
	[QC35_UNKNOW]		= {POWER_SUPPLY_TYPE_UNKNOWN,		CHARGER_UNKNOWN,		USBSW_CHG},
};

/* These default values will be used if there's no property in dts */
static struct xmusb350_desc xmusb350_default_desc = {
	.en_bc12 = true,
	.en_hvdcp = true,
	.en_intb = true,
	.en_sleep = true,
	.chg_dev_name = "secondary_chg",
	.alias_name = "xmusb350",
};
// basic i2c interface

static int xmusb350_psy_chg_type_changed(struct xmusb350_charger *chip);
static int xmusb350_set_usbsw_state(struct xmusb350_charger *chip, int state);
static int xmusb350_rerun_apsd(struct charger_device *chg_dev);

#if 0
static int __xmusb350_write_byte_only(struct xmusb350_charger *chip, u8 reg)
{
	s32 ret;

	ret = i2c_smbus_write_byte(chip->client, reg);
	if (ret < 0) {
		xm35_err("%s: reg[0x%02X] write failed.\n",
		       __func__, reg);
		return ret;
	}
	return 0;
}
#endif

#if 0// for test
static int __xmusb350_read_byte(struct xmusb350_charger *chip, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(chip->client, reg);
	if (ret < 0) {
		xm35_err("%s: reg[0x%02X] read failed.\n",
				__func__, reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}
#endif

static int __xmusb350_write_byte(struct xmusb350_charger *chip, u8 reg, u8 val)
{
	s32 ret;

	if (chip->fw_update_flag) {
		xm35_err("%s: fw update ing, i2c stop.\n", __func__);
		return 0;
	}

	ret = i2c_smbus_write_byte_data(chip->client, reg, val);
	if (ret < 0) {
		xm35_err("%s: reg[0x%02X] write failed.\n",
		       __func__, reg);
		return ret;
	}
	return 0;
}

static int __xmusb350_read_word(struct xmusb350_charger *chip, u8 reg, u16 *data)
{
	s32 ret;

	if (chip->fw_update_flag) {
		xm35_err("%s: fw update ing, i2c stop.\n", __func__);
		return 0;
	}

	ret = i2c_smbus_read_word_data(chip->client, reg);
	if (ret < 0) {
		xm35_err("%s: reg[0x%02X] read failed.\n",
				__func__, reg);
		return ret;
	}

	*data = (u16) ret;

	return 0;
}

static int __xmusb350_write_word(struct xmusb350_charger *chip, u8 reg, u16 val)
{
	s32 ret;

	if (chip->fw_update_flag) {
		xm35_err("%s: fw update ing, i2c stop.\n", __func__);
		return 0;
	}

	ret = i2c_smbus_write_word_data(chip->client, reg, val);
	if (ret < 0) {
		xm35_err("%s: reg[0x%02X] write failed.\n",
		       __func__, reg);
		return ret;
	}
	return 0;
}

static int __xmusb350_read_block(struct xmusb350_charger *chip, u8 reg, u8 *data)
{
	s32 ret;

	if (chip->fw_update_flag) {
		xm35_err("%s: fw update ing, i2c stop.\n", __func__);
		return 0;
	}

	ret = i2c_smbus_read_i2c_block_data(chip->client, reg, 4, data);
	if (ret < 0) {
		xm35_err("%s: reg[0x%02X] read failed.\n",
				__func__, reg);
		return ret;
	}

	return 0;
}

#if 0
static int xmusb350_write_byte_only(struct xmusb350_charger *chip, u8 reg)
{
	return __xmusb350_write_byte_only(chip, reg);
}
#endif

static int xmusb350_write_byte(struct xmusb350_charger *chip, u8 reg, u8 data)
{
	return __xmusb350_write_byte(chip, reg, data);
}

#if 0// for test
static int xmusb350_read_byte(struct xmusb350_charger *chip, u8 reg, u8 *data)
{
	return __xmusb350_read_byte(chip, reg, data);
}
#endif

static int xmusb350_read_word(struct xmusb350_charger *chip, u8 reg, u16 *data)
{
	return __xmusb350_read_word(chip, reg, data);
}

static int xmusb350_write_word(struct xmusb350_charger *chip, u8 reg, u16 data)
{
	return __xmusb350_write_word(chip, reg, data);
}

static int xmusb350_read_block(struct xmusb350_charger *chip, u8 reg, u8 *data)
{
	return __xmusb350_read_block(chip, reg, data);
}

static int xmusb350_vbus_disable(struct xmusb350_charger *chip,	bool disable)
{
	xm35_dbg("%s disable: %d\n", __func__, disable);
	if (disable) {
		gpio_direction_output(chip->connect_therm_gpio, 1);
		chip->vbus_disable = true;
	} else {
		gpio_direction_output(chip->connect_therm_gpio, 0);
		chip->vbus_disable = false;
	}
	return 0;
}

#define THERM_REG_RECHECK_DELAY_200MS		200	/* 1 sec */
#define THERM_REG_RECHECK_DELAY_1S		1000	/* 1 sec */
#define THERM_REG_RECHECK_DELAY_5S		5000	/* 5 sec */
#define THERM_REG_RECHECK_DELAY_8S		8000	/* 8 sec */
#define THERM_REG_RECHECK_DELAY_10S		10000	/* 10 sec */

#define CONNECTOR_THERM_ABOVE			200	/* 20 Dec */
#define CONNECTOR_THERM_HIG			500	/* 50 Dec */
#define CONNECTOR_THERM_TOO_HIG			750	/* 75 Dec */

static int smblib_set_sw_conn_therm_regulation(struct xmusb350_charger *chip, bool enable)
{
	int typec_mode = POWER_SUPPLY_TYPEC_NONE;
	union power_supply_propval val;

	if (chip->usb_psy) {
		power_supply_get_property(chip->usb_psy,
					POWER_SUPPLY_PROP_TYPEC_MODE, &val);
		typec_mode = val.intval;
	}

	xm35_dbg("%s enable: %d\n", __func__, enable);

	if (enable) {
		chip->entry_time = ktime_get();
		schedule_delayed_work(&chip->conn_therm_work,
				msecs_to_jiffies(THERM_REG_RECHECK_DELAY_1S));

	} else {
		if ((chip->thermal_status != TEMP_ABOVE_RANGE) && (typec_mode == POWER_SUPPLY_TYPEC_NONE))
			cancel_delayed_work(&chip->conn_therm_work);
	}

	return 0;
}

#define CHG_DETECT_CONN_THERM_US		10000000	/* 10sec */
static void smblib_conn_therm_work(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger,
						conn_therm_work.work);
	union power_supply_propval val;
	int wdog_timeout = THERM_REG_RECHECK_DELAY_10S;
	static int thermal_status = TEMP_BELOW_RANGE;
	int usb_present, typec_mode = POWER_SUPPLY_TYPEC_NONE;
	u64 elapsed_us;
	static bool enable_charging = true;

	/* Get usb power supply */
	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		xm35_err("%s: get usb power supply failed\n", __func__);
		return;
	}

	pm_stay_awake(chip->dev);
	power_supply_get_property(chip->usb_psy,
		POWER_SUPPLY_PROP_CONNECTOR_TEMP, &val);
	chip->connector_temp = val.intval;
	xm35_dbg("%s connector_temp = %d\n", __func__, chip->connector_temp);

	if (chip->connector_temp >= CONNECTOR_THERM_TOO_HIG) {
		xm35_err("chg->connector_temp:%d is too hig\n",
				chip->connector_temp);
		thermal_status = TEMP_ABOVE_RANGE;
		wdog_timeout = THERM_REG_RECHECK_DELAY_1S;
	} else if (chip->connector_temp >= CONNECTOR_THERM_HIG) {
		if ((thermal_status == TEMP_ABOVE_RANGE)  &&
			(chip->connector_temp > CONNECTOR_THERM_TOO_HIG - 100)) {
			xm35_err("chg->connector_temp:%d is warm\n", chip->connector_temp);
		} else {
			thermal_status = TEMP_ALERT_LEVEL;
		}
		wdog_timeout = THERM_REG_RECHECK_DELAY_1S;
	} else {
		if ((thermal_status == TEMP_ABOVE_RANGE)  &&
			(chip->connector_temp > CONNECTOR_THERM_TOO_HIG - 100)) {
			wdog_timeout = THERM_REG_RECHECK_DELAY_1S;
		} else
			thermal_status = TEMP_BELOW_RANGE;
		wdog_timeout = THERM_REG_RECHECK_DELAY_10S;
	}

	if (thermal_status != chip->thermal_status) {
		chip->thermal_status = thermal_status;
		if (thermal_status == TEMP_ABOVE_RANGE && enable_charging) {
			enable_charging = false;
			xm35_err("connect temp is too hot, input suspend\n");
			//charger_manager_enable_charging(chip->chg_consumer,
			//	MAIN_CHARGER, false);
			val.intval = 1;
			power_supply_set_property(chip->batt_psy,
					POWER_SUPPLY_PROP_INPUT_SUSPEND, &val);
			xmusb350_vbus_disable(chip, true);
		} else if (thermal_status != TEMP_ABOVE_RANGE && !enable_charging) {
			enable_charging = true;
			xm35_err("connect temp normal recovery input suspend\n");
			//charger_manager_enable_charging(chip->chg_consumer,
			//      MAIN_CHARGER, true);
			val.intval = 0;
			power_supply_set_property(chip->batt_psy,
					POWER_SUPPLY_PROP_INPUT_SUSPEND, &val);
			xmusb350_vbus_disable(chip, false);
		}
	}

	xm35_err("[connector_temp thermal_status enable_charging wdog_timeout] = [%d %d %d %d]\n",
			chip->connector_temp, thermal_status, enable_charging, wdog_timeout);

	elapsed_us = ktime_us_delta(ktime_get(), chip->entry_time);
	if (elapsed_us < CHG_DETECT_CONN_THERM_US)
		wdog_timeout = THERM_REG_RECHECK_DELAY_200MS;

	pm_relax(chip->dev);
	power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_PRESENT, &val);
	usb_present = val.intval;
	power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_TYPEC_MODE, &val);
	typec_mode = val.intval;

	if (!usb_present && thermal_status == TEMP_BELOW_RANGE
			&& !chip->otg_enable && typec_mode == POWER_SUPPLY_TYPEC_NONE)
		xm35_err("usb is disconnet cancel the connect them work, usb_present[%d],\
				thermal_status[%d], otg_enable[%d], typec_mode[%d]\n",
				usb_present, thermal_status, chip->otg_enable, typec_mode);
	else
		schedule_delayed_work(&chip->conn_therm_work,
				msecs_to_jiffies(wdog_timeout));
}

static int xmusb350_soft_reset(struct xmusb350_charger *chip)
{
	int ret = 0;

	//ret = xmusb350_write_byte_only(chip, XMUSB350_REG_SOFT_RESET);
	ret = xmusb350_write_byte(chip, XMUSB350_REG_SOFT_RESET, ENABLE);

	if (ret)
		xm35_err("%s: failed!\n", __func__);
	else
		xm35_info("%s: done!\n", __func__);

	return ret;
}

static int xmusb350_qc_mode_select(struct xmusb350_charger *chip, u8 mode)
{
	int ret = -1;

	if (mode > XMUSB350_MODE_QC3_PLUS_V5) {
		xm35_err("%s: qc mode[%d] is wrong!\n", __func__, mode);
		return ret;
	}

	ret = xmusb350_write_byte(chip, XMUSB350_REG_QC_MS, mode);

	if (ret)
		xm35_err("%s: select qc mode[%d] failed!\n", __func__, mode);
	else
		xm35_dbg("%s: select qc mode[%d] success!\n", __func__, mode);

	msleep(200);// need at least 120ms, then dp dm increase
	return ret;
}

static int xmusb350_hvdcp_enable(struct xmusb350_charger *chip, bool control)
{
	int ret = -1;

	if (control == ENABLE)
		msleep(500);// need 500ms, then QC20\QC30\QC35 detect

	ret = xmusb350_write_byte(chip, XMUSB350_REG_HVDCP_EN, (u8)control);

	if (ret)
		xm35_err("%s: hvdcp enable/disable[%d] failed!\n", __func__, control);
	else {
		xm35_err("%s: hvdcp enable/disable[%d] success!\n", __func__, control);
		if (chip->hvdcp_en != control)
			chip->hvdcp_en = control;
	}

	return ret;
}

/*
static int xmusb350_bc12_enable(struct xmusb350_charger *chip, bool control)
{
	int ret = -1;

	ret = xmusb350_write_byte(chip, XMUSB350_REG_BC12_EN, (u8)control);

	if (ret)
		xm35_err("%s: bc12 enable/disable[%d] failed!\n", __func__, control);
	else
		xm35_info("%s: bc12 enable/disable[%d] success!\n", __func__, control);

	return ret;
}
*/

static int xmusb350_get_error_state(struct xmusb350_charger *chip, int *err_state)
{
	int ret;
	u16 val;

	ret = xmusb350_read_word(chip, XMUSB350_REG_CHGT_ERROR, &val);
	if (ret) {
		xm35_err("%s: get error state failed!\n", __func__);
	} else {
		xm35_dbg("%s: get error state success, %04x!\n", __func__, val);
		*err_state = (int)((val >> 8) & 0xFF);
		xm35_log("%s: error state is [%02x]!\n", __func__, *err_state);
	}

	return ret;
}

static int xmusb350_get_charger_type(struct xmusb350_charger *chip, int *chg_type)
{
	int ret;
	u16 val;

	ret = xmusb350_read_word(chip, XMUSB350_REG_CHGT_ERROR, &val);
	if (ret) {
		xm35_err("%s: get charger type failed!\n", __func__);
		*chg_type = QC35_NA;
	} else {
		xm35_dbg("%s: get charger type success, %02x!\n", __func__, val);
		*chg_type = (int)(val & 0x1F);
		xm35_log("%s: charger type is [0x%02x]!\n", __func__, *chg_type);
	}

	if (*chg_type > QC35_UNKNOW)
		*chg_type = QC35_UNKNOW;
	return ret;
}

static int xmusb350_hvdcp_dpdm(struct xmusb350_charger *chip, int data)
{
	int ret = -1;
	unsigned char word_h, word_l;

	word_h = (unsigned char)(((u16)data >> 8) & 0xFF);
	word_l = (unsigned char)(((u16)data) & 0xFF);
	data = (u16)(word_l << 8 | word_h);
	xm35_dbg("%s: word_h is %02x, word_l is %02x, data is %04x!\n", __func__, word_h, word_l, data);

	if (chip->dpdm_mode == QC30_DPDM) {
		ret = xmusb350_write_word(chip, XMUSB350_REG_QC30_PM, (u16)data);
		if (ret)
			xm35_err("Set XMUSB350_REG_QC30_PM[%d] failed!\n", data);
		else
			xm35_dbg("Set XMUSB350_REG_QC30_PM[%d] success!\n", data);
	} else if (chip->dpdm_mode == QC35_DPDM) {
		ret = xmusb350_write_word(chip, XMUSB350_REG_QC3_PLUS_PM, (u16)data);
		if (ret)
			xm35_err("Set XMUSB350_REG_QC3_PLUS_PM[%d] failed!\n", data);
		else
			xm35_dbg("Set XMUSB350_REG_QC3_PLUS_PM[%d] success!\n", data);
	}

	return ret;
}

static int xmusb350_get_vin_vol(struct xmusb350_charger *chip, u16 *val)
{
	int ret;
	u16 tmp = 0;

	ret = xmusb350_read_word(chip, XMUSB350_REG_VIN, &tmp);
	if (ret) {
		xm35_err("%s: get vin voltage failed!\n", __func__);
	} else {
		*val = (u16)(((tmp & 0xFF) << 8) | ((tmp >> 8) & 0xFF));
		xm35_dbg("%s: get vin voltage success, %04x!\n", __func__, *val);
	}

	return ret;
}

static int xmusb350_get_version(struct xmusb350_charger *chip, u16 *val)
{
	int ret;
	u16 tmp = 0;

	ret = xmusb350_read_word(chip, XMUSB350_REG_VERSION, &tmp);
	if (ret) {
		xm35_err("%s: get version failed!\n", __func__);
	} else {
		*val = (u16)(((tmp & 0xFF) << 8) | ((tmp >> 8) & 0xFF));
		xm35_dbg("%s: get version success, %04x!\n", __func__, *val);
	}

	return ret;
}

static int xmusb350_get_vendor_id(struct xmusb350_charger *chip, u8 *vid)
{
	int ret;
	struct charger_manager *cm = NULL;

	ret = xmusb350_read_block(chip, XMUSB350_REG_VID, vid);
	if (ret)
		xm35_err("%s: get vendor id failed!\n", __func__);
	else {
		xm35_dbg("%s: get vendor id success, %02x %02x %02x %02x!\n", __func__, vid[0], vid[1], vid[2], vid[3]);
		if ((chip->chg_consumer) && (chip->chg_consumer->cm))
			cm = chip->chg_consumer->cm;
		if (cm) {
			cm->xmusb_vid = (int)((vid[0] * 0x1000000) | (vid[1] * 0x10000) | (vid[2] * 0x100) | (vid[3]));
			xm35_log("%s: xmusb_vid = 0x%x, %d\n", __func__, cm->xmusb_vid, cm->xmusb_vid);
			if (cm->xmusb_vid == WELTREND_VID) {
				ret = 2;
			}
			if (cm->xmusb_vid == INJOINIC_VID) {
				ret = 3;
			}
		}
	}
	return ret;
}

void xmusb350_hard_reset(struct xmusb350_charger *chip)
{
	if (chip->fw_update_flag) {
		xm35_err("%s: fw update ing, i2c stop.\n", __func__);
		return;
	}

	gpio_set_value(chip->xmusb350_rst_gpio, 1);// OUT: 1
	mdelay(10);
	gpio_set_value(chip->xmusb350_rst_gpio, 0);// OUT: 0
	mdelay(60);

	xm35_info("%s: done!\n", __func__);
}

#define TCPC_ATTACH_FIRST_UPDATE_TIME_S	20
#define QC_WAIT_PD_CONN_TIME_S		20
#define QC2_RETRY_MAX_TIMES		6
#define QC3_DET_ERR_MAX_TIMES		3
#define OCP_TIMER			3
#define QC3_AB_THRE			12400
#define VBUS_PLUG_OUT_THR_MV		3000
#define QC3_DP_DM_PR_ICL		1500000
#define QC3_DP_DM_PR_FCC		2000000
static void xmusb350_charger_type_det_work(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger, charger_type_det_work.work);
	int ret = 0;
	int xmusb_type = QC35_UNKNOW, xmusb_err_stat = QC35_ERROR_NO, typec_mode = POWER_SUPPLY_TYPEC_NONE;
	union power_supply_propval pval = {0,};
	struct timespec time;
	int recheck_time = 0;
	bool ignore_tcpc_state = false;

	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		xm35_err("get usb power supply failed\n");
		goto fail;
	} else if (!chip->mt_chg) {
		chip->mt_chg = power_supply_get_drvdata(chip->usb_psy);
	}

	ret = power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_TYPEC_MODE, &pval);
	typec_mode = pval.intval;
	if (chip->tcpc_attach == false && typec_mode != POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER) {
		get_monotonic_boottime(&time);
		if (time.tv_sec > TCPC_ATTACH_FIRST_UPDATE_TIME_S) {
			xm35_err("usb already plugout\n");
			goto fail;
		} else {
			ignore_tcpc_state = true;
			xm35_log("Ignore tcpc_attach state after bootup within %ds\n", TCPC_ATTACH_FIRST_UPDATE_TIME_S);
		}
	}

	ret = xmusb350_get_charger_type(chip, &xmusb_type);
	if (ret) {
		xm35_err("get charger type failed!\n");
		goto fail;
	}

	ret = xmusb350_get_error_state(chip, &xmusb_err_stat);
	if (ret) {
		xm35_err("get error state failed!\n");
		xmusb_err_stat = QC35_ERROR_NO;
	}

	ret = power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_PD_ACTIVE, &pval);
	if (pval.intval != POWER_SUPPLY_PD_INACTIVE) {
		chip->hvdcp_det_lock = false;
		if (xmusb_type == QC35_SDP)
			xmusb_type = QC35_PD_DR;
		else
			xmusb_type = QC35_PD;
		goto update_type;
	}

	//force the charging type to DCP while ocp and float appear alternately
	if (xmusb_type == QC35_OCP || xmusb_type == QC35_FLOAT) {
		if ((chip->qc35_chg_type == QC35_OCP && xmusb_type == QC35_FLOAT) ||
			(chip->qc35_chg_type == QC35_FLOAT && xmusb_type == QC35_OCP)) {
			if (chip->ocp_timer++ >= OCP_TIMER) {
				pr_info("%s: Non standard OCP, force DCP 5v 1.5A\n", __func__);
				xmusb_type = QC35_DCP;
			}
		} else if (chip->qc35_chg_type == QC35_DCP && chip->ocp_timer >= OCP_TIMER) {
				pr_info("%s: Non standard OCP, keep DCP 5v 1.5A\n", __func__);
				xmusb_type = QC35_DCP;
		}
	} else {
		chip->ocp_timer = 0;
	}

	// lock hvdcp detection when bootup for waiting PD status
	if (xmusb_type == QC35_DCP || xmusb_type == QC35_HVDCP) {
		get_monotonic_boottime(&time);
		if (time.tv_sec < QC_WAIT_PD_CONN_TIME_S) {
			xm35_err("boot_time %ld, wait pd update status\n", time.tv_sec);
			if (chip->hvdcp_en)
				xmusb350_hvdcp_enable(chip, DISABLE);
			chip->hvdcp_det_lock = true;
			recheck_time = (QC_WAIT_PD_CONN_TIME_S - time.tv_sec) * 1000;
			goto update_type;
		}
	}
	chip->hvdcp_det_lock = false;

	// hvdcp need continue detect
	if (xmusb_type == QC35_HVDCP) {
		if (chip->hvdcp_retry_timer++ >= QC2_RETRY_MAX_TIMES) {
			xm35_err("%s: Non standard QC charger, force to QC2\n", __func__);
			xmusb_type = QC35_HVDCP_20;
			goto update_type;
		} else {
			ret = xmusb350_hvdcp_enable(chip, ENABLE);
			if (ret)
				xm35_err("%s: hvdcp enable failed!\n", __func__);

			if (xmusb_err_stat == QC35_ERROR_QC30_DET && chip->qc35_chg_type == QC35_HVDCP) {
				if (chip->qc3_det_err_timer++ >= QC3_DET_ERR_MAX_TIMES) {
					xm35_err("QC3 detect error, trigger soft reset!\n");
					chip->hvdcp_trig_reset = true;
					xmusb350_soft_reset(chip);
					goto update_type;
				}
			}
		}
	} else {
		chip->qc3_det_err_timer = 0;
		if (!chip->hvdcp_trig_reset)
			chip->hvdcp_retry_timer = 0;
	}

	if (xmusb_type == QC35_HVDCP_3_PLUS_18 && chip->qc35_chg_type == QC35_HVDCP
			&& chip->qc35_err_sta == QC35_ERROR_QC30_DET) {
		xm35_err("keep qc2 charging due to qc3 detect error!\n");
		xmusb_type = QC35_HVDCP_20;
		goto update_type;
	}
	if ((chip->qc35_chg_type == QC35_HVDCP_30_18 && xmusb_type == QC35_OCP)) {
		//ret = xmusb350_write_byte_only(chip, XMUSB350_REG_RERUN_APSD);
		ret = xmusb350_write_byte(chip, XMUSB350_REG_RERUN_APSD, ENABLE);
		if (ret) {
			xm35_err("%s: 18W overload rerun apsd failed!\n", __func__);
			goto fail;
		} else {
			xm35_err("%s: 18W overload rerun apsd sucessfull! xmusb_current type = %d\n", __func__, xmusb_type);
		}
		xmusb_type = QC35_HVDCP_30_18;
		chip->check_overload = 1;
		goto update_type;
	}

	// 18W and 27W
	if (xmusb_type == QC35_HVDCP_30 && chip->distinguish_qc3) {
		chip->dpdm_mode = QC30_DPDM;
		ret = xmusb350_qc_mode_select(chip, XMUSB350_MODE_QC20_V5);
		if (ret) {
			xm35_err("select qc2 dpdm mode failed!\n");
			goto fail;
		}

		ret = xmusb350_qc_mode_select(chip, XMUSB350_MODE_QC30_V5);
		if (ret) {
			xm35_err("select qc3 dpdm mode failed!\n");
			goto fail;
		}
		if (chip->check_overload == 1) {
			xmusb_type = QC35_HVDCP_30_18;
			chip->check_overload = 0;
			goto update_type;
		}

		xmusb350_hvdcp_dpdm(chip, 0x8027); // 39 pulses
		xm35_log("%s: dp dm process done!\n", __func__);
		msleep(300);

		ret = power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		xm35_log("%s: usb voltage is [%d]!\n", __func__, pval.intval);
		if (pval.intval <= VBUS_PLUG_OUT_THR_MV)
			goto fail;
		else if (pval.intval <= QC3_AB_THRE)
			xmusb_type = QC35_HVDCP_30_18;
		else
			xmusb_type = QC35_HVDCP_30_27;

		ret = xmusb350_qc_mode_select(chip, XMUSB350_MODE_QC20_V5);
		if (ret) {
			xm35_err("select qc2 dpdm mode failed!\n");
			goto fail;
		}

		ret = xmusb350_qc_mode_select(chip, XMUSB350_MODE_QC30_V5);
		if (ret) {
			xm35_err("select qc3 dpdm mode failed!\n");
			goto fail;
		}

		ret = power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		/* if usb plug out during qc3 class-a and class-b detection, set type to NA*/
		if (pval.intval <= VBUS_PLUG_OUT_THR_MV)
			goto fail;

		xm35_log("%s: charger type is [0x%02x]!\n", __func__, xmusb_type);
	} else if (xmusb_type == QC35_HVDCP_30) {
		xmusb_type = QC35_HVDCP_30_18;
	}

	if (chip->check_overload) {
		xmusb_type = QC35_HVDCP_30_18;
	}
	goto update_type;

fail:
	xmusb_type = QC35_UNKNOW;
	if (!chip->usb_psy || !chip->mt_chg)  {
		xm35_err("usb psy not found or mt_chg no found!\n");
		return;
	}

update_type:
	if (xmusb_type == QC35_NA
			|| (chip->tcpc_attach == false
			&& typec_mode != POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER
			&& !ignore_tcpc_state))
		xmusb_type = QC35_UNKNOW;

	xm35_log("old type = 0x%02x, new type = 0x%02x, typec_mode = %d\n", chip->qc35_chg_type, xmusb_type, typec_mode);

	if (chip->qc35_err_sta != xmusb_err_stat)
		chip->qc35_err_sta = xmusb_err_stat;

	if (chip->qc35_chg_type != xmusb_type
			|| chip->chg_type != charger_type_table[xmusb_type].chg_type
			|| chip->usb_switch != charger_type_table[xmusb_type].usb_switch) {
		chip->qc35_chg_type = xmusb_type;
		chip->mt_chg->usb_desc.type = charger_type_table[xmusb_type].psy_type;
		chip->chg_type = charger_type_table[xmusb_type].chg_type;
		chip->last_chg_type = charger_type_table[xmusb_type].chg_type;
		chip->usb_switch = charger_type_table[xmusb_type].usb_switch;

		if (chip->mt_chg->usb_desc.type == POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS) {
			chip->dpdm_mode = QC35_DPDM;
		} else {
			chip->dpdm_mode = QC30_DPDM;
		}
	}
	xmusb350_set_usbsw_state(chip, chip->usb_switch);
	xmusb350_psy_chg_type_changed(chip);

	xm35_log("%s: MTK type is [0x%02x], QC35 type is [0x%02x], usb_desc.type is [0x%02x].\n",
			__func__, chip->chg_type, chip->qc35_chg_type, chip->mt_chg->usb_desc.type);

	if (chip->hvdcp_det_lock)
		goto recheck;

	return;

recheck:
	schedule_delayed_work(&chip->charger_type_det_work, msecs_to_jiffies(recheck_time));

}

static void xmusb350_charger_type_recheck(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger, charger_type_recheck_work.work);
	int ret = 0;

	if (chip->chg_type == NONSTANDARD_CHARGER || chip->chg_type == CHARGER_UNKNOWN) {
		chip->recheck_count++;
		//ret = xmusb350_write_byte_only(chip, XMUSB350_REG_RERUN_APSD);
		ret = xmusb350_write_byte(chip, XMUSB350_REG_RERUN_APSD, ENABLE);
		if (ret)
			xm35_err("%s: recheck rerun apsd failed!\n", __func__);
		else
			xm35_info("%s: recheck rerun apsd success!\n", __func__);
	}

	if (chip->recheck_count < MAX_RECHECK_COUNT)
		schedule_delayed_work(&chip->charger_type_recheck_work, msecs_to_jiffies(RECHECK_DELAY));
}

static enum power_supply_property xmusb350_properties[] = {
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_QC35_CHG_TYPE,
	POWER_SUPPLY_PROP_QC35_ERROR_STATE,
	POWER_SUPPLY_PROP_QC35_VERSION,
	POWER_SUPPLY_PROP_QC35_VIN,
	POWER_SUPPLY_PROP_QC35_VID,
	POWER_SUPPLY_PROP_QC35_CHIP_OK,
/*
	POWER_SUPPLY_PROP_QC35_RERUN_APSD,
	POWER_SUPPLY_PROP_QC35_SOFT_RESET,
	POWER_SUPPLY_PROP_QC35_MODE_SELECT,
	POWER_SUPPLY_PROP_QC35_INTB_ENABLE,
	POWER_SUPPLY_PROP_QC35_HVDCP_ENABLE,
	POWER_SUPPLY_PROP_QC35_BC12_ENABLE,
	POWER_SUPPLY_PROP_QC35_SLEEP_ENABLE,*/
};

static int xmusb350_get_property(struct power_supply *psy,
			enum power_supply_property prop,
			union power_supply_propval *val)
{
	struct xmusb350_charger *chip = power_supply_get_drvdata(psy);
	int rc = 0;
	u16 vin = 0;
	u16 version = 0;
	u8 buf[50];

	switch (prop) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "xmusb350";
		break;
	case POWER_SUPPLY_PROP_QC35_CHG_TYPE:
		val->intval = chip->qc35_chg_type;
		break;
	case POWER_SUPPLY_PROP_QC35_ERROR_STATE:
		rc = xmusb350_get_error_state(chip, &chip->qc35_err_sta);
		val->intval = chip->qc35_err_sta;
		break;
	case POWER_SUPPLY_PROP_QC35_VERSION:
		rc = xmusb350_get_version(chip, &version);
		if (!rc) {
			val->arrayval[0] = (unsigned char)((version >> 8) & 0xFF);
			val->arrayval[1] = (unsigned char)(version & 0xFF);
		} else {
			return -EAGAIN;
		}
		break;
	case POWER_SUPPLY_PROP_QC35_VIN:
		rc = xmusb350_get_vin_vol(chip, &vin);
		if (!rc)
			val->intval = (vin * 2400 * 502) / (1024 * 62);// mv
		else
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_QC35_VID:
		rc = xmusb350_get_vendor_id(chip, buf);
		memcpy(val->arrayval, buf, 4);
		break;
	case POWER_SUPPLY_PROP_QC35_CHIP_OK:
		rc = xmusb350_get_vendor_id(chip, buf);
		if (rc >= 2) {
			rc = 0;
			val->intval = 1;
		} else {
			return -EAGAIN;
		}
		break;
	default:
		xm35_err("%s: unsupported property %d\n", __func__, prop);
		return -ENODATA;
	}

	if (rc < 0) {
		xm35_dbg("Couldn't get prop %d rc = %d\n", prop, rc);
		return -ENODATA;
	}
	return rc;
}

void dump_regs(struct xmusb350_charger *chip)
{
	//u8 data;
	u16 data_word;
	u8 vid[4];
	int rc;

	rc = xmusb350_read_word(chip, XMUSB350_REG_CHGT_ERROR, &data_word);
	if (!rc)
		xm35_dbg("%s: 0x%02x = 0x%04x\n", __func__,
				XMUSB350_REG_CHGT_ERROR, data_word);

	rc = xmusb350_read_word(chip, XMUSB350_REG_VIN, &data_word);
	if (!rc)
		xm35_dbg("%s: 0x%02x = 0x%04x\n", __func__,
				XMUSB350_REG_VIN, data_word);

	rc = xmusb350_read_block(chip, XMUSB350_REG_VID, vid);
	if (!rc)
		xm35_dbg("%s: 0x%02x = %02x %02x %02x %02x!\n", __func__,
				XMUSB350_REG_VID, vid[0], vid[1], vid[2], vid[3]);
}

static int xmusb350_dump_register(struct charger_device *chg_dev)
{
	int ret = 0;
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);

	/* return directly, as we only use it in debug mode */
	return ret;

	dump_regs(chip);
	return ret;
}

static int xmusb350_psy_chg_type_changed(struct xmusb350_charger *chip)
{
	int ret = 0;
	union power_supply_propval propval;

	chip->charger_psy = power_supply_get_by_name("charger");
	if (!chip->charger_psy) {
		xm35_err("%s: get charger power supply failed\n", __func__);
		return -EINVAL;
	}

	propval.intval = chip->chg_type;
	ret = power_supply_set_property(chip->charger_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	if (ret < 0)
		xm35_err("psy type failed, ret = %d\n", ret);
	else
		xm35_log("chg_type = %d\n", chip->chg_type);

	return ret;
}

static int xmusb350_set_usbsw_state(struct xmusb350_charger *chip, int state)
{
	xm35_log("%s: state = %s\n", __func__, state ? "usb" : "chg");

	/* Switch D+D- to AP/xmusb350 */
	if (state == USBSW_CHG) {
		Charger_Detect_Init();
	} else {
		Charger_Detect_Release();
	}

	return 0;
}

static int xmusb350_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);
	int ret = 0, i = 0, typec_mode = POWER_SUPPLY_TYPEC_NONE;
	const int max_wait_cnt = 200;
	union power_supply_propval pval = {0,};

	xm35_info("%s: en = %d\n", __func__, en);

	/* Get usb power supply */
	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		xm35_err("get usb power supply failed\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_TYPEC_MODE, &pval);
	typec_mode = pval.intval;
	xm35_info("en = %d, typec_mode = %d\n", en, typec_mode);

	if (chip->tcpc_attach == en && typec_mode != POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER) {
		xm35_info("attach(%d) is the same\n", en);
		return ret;
	}

	chip->tcpc_attach = en;

	if (en) {
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy())
				break;
			pr_info("%s: is_usb_rdy is false\n", __func__);
			if (!chip->tcpc_attach) {
				pr_info("%s: plug out", __func__);
				return ret;
			}
			msleep(100);
		}
		if (i == max_wait_cnt)
			pr_err("%s: CDP timeout\n", __func__);
		else
			pr_info("%s: CDP free\n", __func__);
	}

	chip->usb_switch = en ? USBSW_CHG : USBSW_USB;
	ret = power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_PD_ACTIVE, &pval);
	if (chip->chg_type == STANDARD_HOST || chip->chg_type == CHARGING_HOST || pval.intval != POWER_SUPPLY_PD_INACTIVE)
		chip->usb_switch = USBSW_USB;
	xmusb350_set_usbsw_state(chip, chip->usb_switch);

	if (!en) {
		chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		chip->qc35_chg_type = QC35_UNKNOW;
		chip->chg_type = CHARGER_UNKNOWN;
		chip->hvdcp_trig_reset = false;
		xmusb350_hvdcp_enable(chip, DISABLE);
		xmusb350_psy_chg_type_changed(chip);
	} else {
		if (chip->last_chg_type == NONSTANDARD_CHARGER) {
			//xmusb350_write_byte_only(chip, XMUSB350_REG_RERUN_APSD);
			xmusb350_write_byte(chip, XMUSB350_REG_RERUN_APSD, ENABLE);
			xm35_info("rerun APSD\n");
		}
	}

	return ret;
}

static int xmusb350_enable_hvdcp_det(struct charger_device *chg_dev, bool enable)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;

	xm35_info("%s: enable = %d\n", __func__, enable);
	if (enable == false) {
		chip->hvdcp_en = false;
		ret = xmusb350_hvdcp_enable(chip, DISABLE);
		if (ret) {
			xm35_err("%s: disable hvdcp detect failed!\n", __func__);
		} else {
			xm35_info("%s: disable hvdcp detect successful!\n", __func__);
		}
	} else {
		chip->hvdcp_en = true;
	}

	return ret;
}

static int xmusb350_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);

	xm35_info("%s: en = %d\n", __func__, en);

	chip->otg_enable = en;
	if (en) {
		chip->usb_switch = USBSW_USB;
		xmusb350_set_usbsw_state(chip, chip->usb_switch);

		schedule_delayed_work(&chip->conn_therm_work,
				msecs_to_jiffies(THERM_REG_RECHECK_DELAY_200MS));

	} else {
		smblib_set_sw_conn_therm_regulation(chip, false);
	}

	return 0;
}

static int xmusb350_rerun_apsd(struct charger_device *chg_dev)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;

	xm35_info("%s: xmusb350 rerun apsd\n", __func__);

	//ret = xmusb350_write_byte_only(chip, XMUSB350_REG_RERUN_APSD);
	ret = xmusb350_write_byte(chip, XMUSB350_REG_RERUN_APSD, ENABLE);
	if (ret)
		xm35_err("%s: rerun apsd failed!\n", __func__);
	else
		xm35_info("%s: rerun apsd success!\n", __func__);

	return ret;
}

static int xmusb350_mode_select(struct charger_device *chg_dev, u8 mode)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;

	xm35_info("%s: xmusb350 mode select\n", __func__);

	ret = xmusb350_qc_mode_select(chip, mode);
	if (ret)
		xm35_err("%s: select qc mode[%d] failed!\n", __func__, mode);
	else
		xm35_dbg("%s: select qc mode[%d] success!\n", __func__, mode);

	return ret;
}

static int xmusb350_tune_hvdcp_dpdm(struct charger_device *chg_dev, int pulse)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;

	xm35_info("%s: xmusb350 tune hvdcp dpdm\n", __func__);

	ret = xmusb350_hvdcp_dpdm(chip, pulse);
	if (ret)
		xm35_err("%s: tune hvdcp dpdm[%d] failed!\n", __func__, pulse);
	else
		xm35_dbg("%s: tune hvdcp dpdm[%d] success!\n", __func__, pulse);

	return ret;
}

static int xmusb350_set_vbus_disable(struct charger_device *chg_dev, bool disable)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;

	xm35_info("%s: xmusb350 set vbus disable\n", __func__);

	ret = xmusb350_vbus_disable(chip, disable);
	if (ret)
		xm35_err("%s: vbus disable[%d] failed!\n", __func__, disable);
	else
		xm35_dbg("%s: vbus disable[%d] success!\n", __func__, disable);

	return ret;
}

static int xmusb350_get_vbus_disable(struct charger_device *chg_dev, bool *disable)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);

	*disable = chip->vbus_disable;
	xm35_info("%s: xmusb350 get vbus disable:%d\n", __func__, *disable);

	return 0;
}

static int xmusb350_get_chg_type(struct charger_device *chg_dev, int *type)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;

	ret = xmusb350_get_charger_type(chip, type);
	if (ret) {
		xm35_err("%s: get charger type failed[%d]!\n", __func__, *type);
		*type = QC35_UNKNOW;
	}

	xm35_info("%s: xmusb350 get chg type %d : %d\n", __func__,
			*type, chip->qc35_chg_type);

	if (*type == QC35_UNKNOW || *type == QC35_NA)
		chip->qc35_chg_type = QC35_UNKNOW;

	*type = chip->qc35_chg_type;
	return ret;
}

static int xmusb350_plug_out(struct charger_device *chg_dev)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);
	// work around for only 6360
	struct charger_manager *cm = NULL;
	// work around for only 6360
	int rc = -1;
	int type = QC35_NA;

	xm35_info("xmusb35 plug out enter\n");

	chip->hvdcp_retry_timer = 0;
	chip->ocp_timer = 0;
	chip->check_overload = 0;
	chip->hvdcp_det_lock = false;

	if ((chip->chg_consumer) && (chip->chg_consumer->cm))
		cm = chip->chg_consumer->cm;

	/* Get usb power supply */
	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy)
		xm35_err("%s: get usb power supply failed\n", __func__);

	if (!chip->otg_enable)
		rc = xmusb350_get_charger_type(chip, &type);
	if ((((rc == 0) && (type == QC35_UNKNOW)) || (rc != 0)) && !chip->otg_enable) {
		// irq process shut, d+d- to qc35 chip, when usb plug out
		//chip->irq_data_process_enable = false;
		chip->qc35_chg_type = QC35_NA;
		/*
		 * when usb plug out and type is qc35_unkown,
		 * should set unkown for power supply charger node
		 * to fix low rate plug out charger, usb type is dcp issue
		 */
		chip->chg_type = CHARGER_UNKNOWN;
		if (cm)
			cm->xmusb_chr_type = QC35_NA;
		chip->hvdcp_en = true;
		chip->hvdcp_dpdm_status = 0;
		if (chip->usb_psy != NULL) {
			chip->mt_chg = power_supply_get_drvdata(chip->usb_psy);
			chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		}
		xmusb350_set_usbsw_state(chip, USBSW_CHG);
	}

	/* Disable SW conn therm Regulation */
	rc = smblib_set_sw_conn_therm_regulation(chip, false);
	if (rc < 0)
		xm35_err("%s: Couldn't stop SW conn therm rc=%d\n", __func__, rc);

	// work around for only 6360
	if (cm) {
		if (cm->xmusb_vid != 0x584d3335)
			return 0;
	}
	// work around for only 6360
	if (!chip->otg_enable)
		xmusb350_psy_chg_type_changed(chip);

	if (chip->usb_psy != NULL && !chip->otg_enable)
		power_supply_changed(chip->usb_psy);

	cancel_delayed_work_sync(&chip->charger_type_recheck_work);

	return 0;
}

static int xmusb350_plug_in(struct charger_device *chg_dev)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);
	int rc;

	xm35_info("xmusb35 plug in enter\n");

	/* Get usb power supply */
	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		xm35_err("%s: get usb power supply failed\n", __func__);
		return -EINVAL;
	}
	if (chip->usb_psy != NULL) {
		chip->mt_chg = power_supply_get_drvdata(chip->usb_psy);
		xm35_dbg("%s: get usb_psy drvdata.\n", __func__);
	}

	if (chip->mt_chg != NULL) {
		if (chip->mt_chg->usb_desc.type != POWER_SUPPLY_TYPE_USB_PD)
			chip->hvdcp_en = true;
	}
	/* Enable SW conn therm Regulation */
	rc = smblib_set_sw_conn_therm_regulation(chip, true);
	if (rc < 0)
		xm35_err("%s: Couldn't start SW conn therm rc=%d\n", __func__, rc);

	chip->recheck_count = 0;
	schedule_delayed_work(&chip->charger_type_recheck_work, msecs_to_jiffies(RECHECK_DELAY));

	return 0;
}

static struct charger_ops xmusb350_chg_ops = {
	.dump_registers = xmusb350_dump_register,
	.enable_chg_type_det = xmusb350_enable_chg_type_det,
	.plug_out = xmusb350_plug_out,
	.enable_hvdcp_det = xmusb350_enable_hvdcp_det,
	.plug_in = xmusb350_plug_in,
	.enable_otg = xmusb350_enable_otg,
	.rerun_apsd = xmusb350_rerun_apsd,
	.mode_select = xmusb350_mode_select,
	.tune_hvdcp_dpdm = xmusb350_tune_hvdcp_dpdm,
	.set_vbus_disable = xmusb350_set_vbus_disable,
	.get_vbus_disable = xmusb350_get_vbus_disable,
	.get_chg_type = xmusb350_get_chg_type,
};

static irqreturn_t xmusb350_interrupt(int irq, void *dev_id)
{
	struct xmusb350_charger *chip = dev_id;

	xm35_info("%s: INT OCCURED\n", __func__);

	if (chip->otg_enable)
		return IRQ_HANDLED;

	mutex_lock(&chip->irq_complete);

	chip->irq_waiting = true;
	if (!chip->resume_completed) {
		xm35_dbg("IRQ triggered before device-resume\n");
		disable_irq_nosync(irq);
		mutex_unlock(&chip->irq_complete);
		return IRQ_HANDLED;
	}
	schedule_delayed_work(&chip->charger_type_det_work, 0);
	chip->irq_waiting = false;

	mutex_unlock(&chip->irq_complete);
	return IRQ_HANDLED;
}

static int load_fw(struct xmusb350_charger *chip, const char *fn, bool force)
{
	int rc;
	int i;
	int pos = 0;
	const struct firmware *fw = NULL;

	file_data = kmalloc(FIRMWARE_FILE_LENGTH, GFP_KERNEL);
	file_date_w = kmalloc(FIRMWARE_FILE_LENGTH, GFP_KERNEL);
	if (rc) {
		xm35_err("Unable to open firmware %s\n", fn);
		rc = ERROR_REQUESET_FIRMWARE;
		goto release_firmware;
	}
	memset(file_data, 0, FIRMWARE_FILE_LENGTH);
	memset(file_date_w, 0, FIRMWARE_FILE_LENGTH);
	rc = request_firmware(&fw, fn, chip->dev);
	if (rc) {
		xm35_err("Unable to open firmware %s\n", fn);
		rc = ERROR_REQUESET_FIRMWARE;
		goto release_firmware;
	}
	file_size = fw->size;
	for (i = 0; i < file_size/4; i++) {
		file_date_w[i] = (((uint32_t)(*(fw->data + pos + 0)) << 24) & 0xff000000) |
						(((uint32_t)(*(fw->data + pos + 1)) << 16) & 0x00ff0000) |
						(((uint32_t)(*(fw->data + pos + 2)) << 8) & 0x0000ff00) |
						(((uint32_t)(*(fw->data + pos + 3)) << 0) & 0x000000ff);
		pos += 4;
	}
	pos = 0;
	for (i = 0; i < file_size/4; i++) {
		file_data[i] = (((uint32_t)(*(fw->data + pos + 0)) << 0) & 0x000000ff) |
						(((uint32_t)(*(fw->data + pos + 1)) << 8) & 0x0000ff00) |
						(((uint32_t)(*(fw->data + pos + 2)) << 16) & 0x00ff0000) |
						(((uint32_t)(*(fw->data + pos + 3)) << 24) & 0xff000000);
		pos += 4;
	}

	return rc;

release_firmware:
	release_firmware(fw);
	kfree(file_data);
	kfree(file_date_w);
	return rc;
}


void Program_init(struct xmusb350_charger *chip)  		// Execute only once at the beginning of the program.
{
	i2c_smbus_write_byte_data(chip->client, 0x42, 0x1f);
	// close watchdog,open memory clock
	i2c_smbus_write_byte_data(chip->client, 0x43, 0x9f);
	//select the memory interface
	i2c_smbus_write_byte_data(chip->client, 0x44, 0x31);
	//write CS to 1, select the chip, select the memory interface
	i2c_smbus_write_byte_data(chip->client, 0x44, 0x33);
}

void Program_end(struct xmusb350_charger *chip) 		// Execute only once at the ending of the program.
{
	//clear the CS/READ signal
	i2c_smbus_write_byte_data(chip->client, 0x44, 0x31);
	i2c_smbus_write_byte_data(chip->client, 0x44, 0x30);
}

void verify_init(struct xmusb350_charger *chip)  		// Execute only once at the beginning of the program.
{
	// CPU reset
	i2c_smbus_write_byte_data(chip->client, 0x42, 0x1f);
	// close watchdog,open memory clock
	i2c_smbus_write_byte_data(chip->client, 0x43, 0x9f);
	//write READ to 1
	i2c_smbus_write_byte_data(chip->client, 0x44, 0x37);
}

void verify_end(struct xmusb350_charger *chip)  		// Execute only once at the ending of the program.
{
	//write READ to 0
	i2c_smbus_write_byte_data(chip->client, 0x44, 0x30);
}

uint8_t Check_CRC(uint32_t *file, uint32_t file_size)
{
	uint32_t crc = 0xFFFF1326;
	const uint32_t poly = 0x04C11DB6;
	uint32_t newbit, newword, rl_crc;
	uint16_t i = 0, j = 0;

    crc = 0xFFFF1326;
    for (i = 0; i < file_size/4; i++) {
		for (j = 0; j < 32; j++) {
			newbit = ((crc>>31) ^ ((*file>>j)&1)) & 1;
			if (newbit)
				newword = poly;
			else
				newword = 0;
			rl_crc = (crc<<1) | newbit;
			crc = rl_crc ^ newword;
		}
		file++;
    }
    if (crc == 0xC704DD7B)
		return 1;
	else
		return 0;
}

uint8_t Write_Memory_OneWord_IIC(struct xmusb350_charger *chip, uint16_t addr, uint32_t data)
{
	uint8_t BUSY_flag = 0 ;
	uint32_t i = 0;
	uint8_t DATA[6] = {addr & 0xff, (addr >> 8) & 0x03, data & 0xff, (data >> 8) & 0xFF, (data >> 16) & 0xFF, (data >> 24) & 0xFF};

	i2c_smbus_write_byte_data(chip->client, 0x45, DATA[0]);
	i2c_smbus_write_byte_data(chip->client, 0x46, DATA[1]);
	i2c_smbus_write_byte_data(chip->client, 0x47, DATA[5]);
	i2c_smbus_write_byte_data(chip->client, 0x48, DATA[4]);
	i2c_smbus_write_byte_data(chip->client, 0x49, DATA[3]);
	i2c_smbus_write_byte_data(chip->client, 0x4a, DATA[2]);
	// Select memory block, configure address and data
	i2c_smbus_write_byte_data(chip->client, 0x4B, 0x01);
	// Write enable
	i2c_smbus_write_byte_data(chip->client, 0x4B, 0x00);
	// Wait for a data program to complete, The timeout is 50*2000us
	for (i = 0; i < 50; i++) {
		// The value of the read 0x4b register is stored in variable BUSY_flag
		BUSY_flag = i2c_smbus_read_word_data(chip->client, 0x4B);
		// Bit7 is the busy flag bit
		BUSY_flag &= 0x80;
		if (BUSY_flag == 0x00)
			return 1;
		msleep(2);
	}
	return 0;
}

uint32_t Read_Memory_OneWord_IIC(struct xmusb350_charger *chip, uint16_t addr)
{
	uint32_t data;
	uint8_t temp3[4];
	uint8_t ADDR[2] = {addr&0xff, (addr>>8)&0x03};
	int ret;

	i2c_smbus_write_byte_data(chip->client, 0x45, ADDR[0]);
	i2c_smbus_write_byte_data(chip->client, 0x46, ADDR[1]);

	ret = i2c_smbus_read_i2c_block_data(chip->client, 0x4C, 4, &temp3[0]);
	data = (temp3[0] << 24) | (temp3[1] << 16) | (temp3[2] << 8) | temp3[3];

	return data;
}

//flag
#define NonTestMode 0x00
#define TestMode    0x01
#define ProgError   0x02

/*Programming example:*/
uint8_t Program(struct xmusb350_charger *chip, uint32_t *file, uint32_t file_size)
{
	uint32_t temp1;
	uint8_t temp3[4];
	uint8_t temp2;
	uint8_t flag = 0;
	uint32_t *file_w = file_date_w;
	uint32_t *file_r = file_date_w;
	uint32_t *file_tmp = file_date_w;
	uint16_t i = 0;

	chip->fw_update_flag = true;
	xm35_err("fw_update_flag:true.\n");

	if (Check_CRC(file, file_size) == 0) {
		xm35_err("i350 Check CRC failed! \n");
		return 0;
	}

	file_size /= 4;

	i2c_smbus_read_i2c_block_data(chip->client, 0x13, 4, &temp3[0]);
	temp1 = (temp3[0] << 24) | (temp3[1] << 16) | (temp3[2] << 8) | temp3[3];
	xm35_err("i350 get templ = 0x%x\n", temp1);
	if (temp1 == 0x49333530) {
		i2c_smbus_write_byte_data(chip->client, 0x40, 0x6B);
		msleep(1);
		temp2 = i2c_smbus_read_word_data(chip->client, 0x40);
		if (temp2 == 0x6B)
			flag = TestMode;
		else
			flag = NonTestMode;
	} else {
		i2c_smbus_read_i2c_block_data(chip->client, 0x41, 4, &temp3[0]);
		temp1 = (temp3[0] << 24) | (temp3[1] << 16) | (temp3[2] << 8) | temp3[3];
		xm35_err("i350 get test mode templ = 0x%x\n", temp1);
		if (temp1 == 0x009F0730)
			flag = TestMode;
		else
			flag = NonTestMode;
	}

	xm35_err("i350 get mode:%d.\n", flag);

	while (flag) {

		Program_init(chip);
		if (Write_Memory_OneWord_IIC(chip, 0x00, 0) == 0) {
			flag = ProgError;
			xm35_err("Failed to programming the first data to 0!\n");
			break;
		} else {
			for (i = 1; i < file_size; i++) {
				if (Write_Memory_OneWord_IIC(chip, i, *(file_w+i)) == 0) {
					flag = ProgError;
					xm35_err("i350 Programming failed! \n");
					break;
				} else
					flag = TestMode;
			}
			Program_end(chip);
		}

		if ((i == file_size) && (flag == TestMode)) {
			verify_init(chip);
			for (i = 1; i < file_size; i++) {
				uint32_t read_data = Read_Memory_OneWord_IIC(chip, i);
				pr_err("i350_data_check: get fw data[%d]=0x%x  read_verify = 0x%x\n", i, *(file_r+i), read_data);
				if (*(file_r+i) != read_data) {
					flag = ProgError;
					xm35_err("i350 Verify failed! \n");
					break;//check??
				} else
					flag = TestMode;
			}
			verify_end(chip);
		}

		if (flag == TestMode) {
			Program_init(chip);
			if (Write_Memory_OneWord_IIC(chip, 0x00, *file_tmp) == 0) {
				flag = ProgError;
				xm35_err("Failed to programming the first data! \n");
				break;
			}
			Program_end(chip);

			verify_init(chip);
			if (*file_tmp != Read_Memory_OneWord_IIC(chip, 0x00)) {
				flag = ProgError;
				xm35_err("Verify failed first data ! \n");
				break;//check错误
			}
			verify_end(chip);
		}

		chip->fw_update_flag = false;
		xm35_err("fw_update_flag:false.\n");

		if (flag == TestMode) {
			/* during HardReset, lock i2c0 bus */
			i2c_lock_bus(chip->client->adapter, I2C_LOCK_SEGMENT);
			xmusb350_hard_reset(chip);
			i2c_unlock_bus(chip->client->adapter, I2C_LOCK_SEGMENT);
			if (file_data != NULL)
				kfree(file_data);
			if (file_date_w != NULL)
				kfree(file_date_w);
			xm35_err("i350 upload fw successfull \n");
			return 1;
		}
	}

	chip->fw_update_flag = false;
	xm35_err("fw_update_flag:false.\n");
	/* during HardReset, lock i2c0 bus */
	i2c_lock_bus(chip->client->adapter, I2C_LOCK_SEGMENT);
	xmusb350_hard_reset(chip);
	i2c_unlock_bus(chip->client->adapter, I2C_LOCK_SEGMENT);
	if (file_data != NULL)
		kfree(file_data);
	if (file_date_w != NULL)
		kfree(file_date_w);
	return 0;
}

static void i350_fw_update_work(struct work_struct *work)
{
	u16 version = 0;
	u8 vid[4];
	uint32_t temp1;
	uint8_t temp3[4];
	int v_id = 0;
	static int retry_count;
	union power_supply_propval val = {0, };
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger, fw_update_work.work);
	int error = 0;

	power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_PRESENT, &val);
	if (val.intval) {
		xm35_err("i350 update firmware aboad usb in\n");
		return;
	}

	/* during HardReset, lock i2c0 bus */
	i2c_lock_bus(chip->client->adapter, I2C_LOCK_SEGMENT);
	xmusb350_hard_reset(chip);
	i2c_unlock_bus(chip->client->adapter, I2C_LOCK_SEGMENT);

	power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (val.intval < 15) {
		xm35_err("i350 update firmware aboad low capacity\n");
		return;
	}

	xmusb350_get_version(chip, &version);
	v_id = xmusb350_get_vendor_id(chip, vid);
	xm35_err("i350 try update vid = %d firmware current version is 0x%x  count = %d\n", v_id, version, retry_count);
	if (v_id == 2) {
		xm35_err("v_id is xmusb350.\n");
		return;
	}
	if ((version == 0x0a0f) && (v_id == 3)) {
		xm35_err("i350 fw is latest.\n");
		return;
	}

	if ((version != 0x0a0f) && (v_id == 3)) {
		xm35_err("i350 try update firmware after 5S\n");
		retry_count = 0;
	} else {
		if (!chip->fw_update_flag) {
			i2c_smbus_read_i2c_block_data(chip->client, 0x41, 4, &temp3[0]);
			temp1 = (temp3[0] << 24) | (temp3[1] << 16) | (temp3[2] << 8) | temp3[3];
			xm35_err("i350 get test mode templ = 0x%x\n", temp1);
			if (temp1 == 0x009F0730) {
				xm35_err("i350 vid is err, but its test mode, start update fw.\n");
				error = load_fw(chip, I350_FW_NAME, true);
				if (error) {
					xm35_err("i350 The firmware update failed(%d)\n", error);
					return;
				} else {
					xm35_err("i350 The firmware load succeeded\n");
				}
				error = Program(chip, file_data, file_size);
				if (!error) {
					xm35_err("i350 The firmware update failed(%d)\n", error);
				} else {
					xm35_err("i350 The firmware update succeeded\n");
					xmusb350_get_version(chip, &version);
				}
			} else {
				xm35_err("i350 isnt test mode.\n");
			}
		}

		if (retry_count > 5) {
			xm35_err("i350 update firmware timeout\n");
			return;
		}

		schedule_delayed_work(&chip->fw_update_work, msecs_to_jiffies(5000));
		retry_count++;
		return;
	}

	error = load_fw(chip, I350_FW_NAME, true);
	if (error) {
		xm35_err("i350 The firmware update failed(%d)\n", error);
		return;
	} else {
		xm35_err("i350 The firmware load succeeded\n");
	}

	error = Program(chip, file_data, file_size);
	if (!error) {
		xm35_err("i350 The firmware update failed(%d)\n", error);
		return;
	} else {
		xm35_err("i350 The firmware update succeeded\n");
		xmusb350_get_version(chip, &version);
	}
}

static ssize_t update_fw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int cmd = 0;
	int error;
	struct xmusb350_charger *chip = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	error = Program(chip, file_data, file_size);
	if (!error) {
		xm35_err("i350 firmware update failed(%d)\n", error);
		count = error;
	} else {
		xm35_err("i350 firmware update succeeded\n");
	}
	return 1;
}

static ssize_t load_fw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int cmd = 0;
	int error;
	struct xmusb350_charger *chip = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	error = load_fw(chip, I350_FW_NAME, true);
	if (error) {
		xm35_err("i350 firmware update failed(%d)\n", error);
		count = error;
	} else {
		xm35_err("i350 firmware load succeeded\n");
	}
	return count;
}

static DEVICE_ATTR(update_firmware, S_IWUSR,
		NULL,
		update_fw_store);

static DEVICE_ATTR(load_firmware, S_IWUSR,
		NULL,
		load_fw_store);

static struct attribute *xmusb350_attributes[] = {
	&dev_attr_load_firmware.attr,
	&dev_attr_update_firmware.attr,
	NULL,

};

static const struct attribute_group xmusb350_attr_group = {
	.attrs = xmusb350_attributes,
};


// parse dts
static int xmusb350_parse_dt(struct xmusb350_charger *chip)
{
	struct xmusb350_desc *desc = NULL;
	struct device_node *np = chip->dev->of_node;
	//int ret;

	if (!np) {
		xm35_err("device tree info missing\n");
		return -EINVAL;
	}

	// parse gpio
	chip->xmusb350_rst_gpio = of_get_named_gpio(np,
					"xm,xmusb350_rst_gpio", 0);
	if ((!gpio_is_valid(chip->xmusb350_rst_gpio))) {
		dev_err(chip->dev, "%s: no xmusb350_rst_gpio\n", __func__);
		//return -EINVAL;
	}
	xm35_dbg("xmusb350_rst_gpio: %d\n", chip->xmusb350_rst_gpio);

	chip->xmusb350_sda_gpio = of_get_named_gpio(np,
					"xm,xmusb350_sda_gpio", 0);
	if ((!gpio_is_valid(chip->xmusb350_sda_gpio))) {
		dev_err(chip->dev, "%s: no xmusb350_sda_gpio\n", __func__);
		//return -EINVAL;
	}
	xm35_dbg("xmusb350_sda_gpio: %d\n", chip->xmusb350_sda_gpio);

	chip->xmusb350_scl_gpio = of_get_named_gpio(np,
					"xm,xmusb350_scl_gpio", 0);
	if ((!gpio_is_valid(chip->xmusb350_scl_gpio))) {
		dev_err(chip->dev, "%s: no xmusb350_scl_gpio\n", __func__);
		//return -EINVAL;
	}
	xm35_dbg("xmusb350_scl_gpio: %d\n", chip->xmusb350_scl_gpio);

	chip->connect_therm_gpio = of_get_named_gpio(np, "mi,connect_therm", 0);
	if ((!gpio_is_valid(chip->connect_therm_gpio))) {
		dev_err(chip->dev, "%s: no connect_therm_gpio\n", __func__);
		return -EINVAL;
	}
	xm35_dbg("connect_therm_gpio: %d\n", chip->connect_therm_gpio);

	chip->bc12_unsupported = of_property_read_bool(np,
					"xm,bc12_unsupported");
	chip->hvdcp_unsupported = of_property_read_bool(np,
					"xm,hvdcp_unsupported");
	chip->intb_unsupported = of_property_read_bool(np,
					"xm,intb_unsupported");
	chip->sleep_unsupported = of_property_read_bool(np,
					"xm,sleep_unsupported");
	chip->distinguish_qc3 = of_property_read_bool(np,
					"xm,distinguish-QC3");

	chip->desc = &xmusb350_default_desc;

	desc = devm_kzalloc(chip->dev, sizeof(struct xmusb350_desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	memcpy(desc, &xmusb350_default_desc, sizeof(struct xmusb350_desc));

	/*
	 * For dual charger, one is primary_chg;
	 * another one will be secondary_chg
	 */
	if (of_property_read_string(np, "charger_name",
				&desc->chg_dev_name) < 0)
		dev_err(chip->dev, "%s: dts no charger name\n", __func__);

	if (of_property_read_string(np, "alias_name", &desc->alias_name) < 0)
		dev_err(chip->dev, "%s: no alias name\n", __func__);

	desc->en_bc12 = of_property_read_bool(np, "en_bc12");
	desc->en_hvdcp = of_property_read_bool(np, "en_hvdcp");
	desc->en_intb = of_property_read_bool(np, "en_intb");
	desc->en_sleep = of_property_read_bool(np, "en_sleep");

	chip->desc = desc;
	chip->chg_props.alias_name = chip->desc->alias_name;
	dev_err(chip->dev, "%s: chg_name:%s alias:%s\n", __func__,
			chip->desc->chg_dev_name, chip->chg_props.alias_name);

	return 0;
}

static int xmusb350_pinctrl_init(struct xmusb350_charger *chip)
{
	int ret;

	chip->xmusb350_pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->xmusb350_pinctrl)) {
		ret = PTR_ERR(chip->xmusb350_pinctrl);
		dev_err(chip->dev,
			"Target does not use pinctrl %d\n", ret);
		goto err_pinctrl_get;
	}

	chip->pinctrl_state_normal
		= pinctrl_lookup_state(chip->xmusb350_pinctrl,
					"xmusb350_normal");
	if (IS_ERR_OR_NULL(chip->pinctrl_state_normal)) {
		ret = PTR_ERR(chip->pinctrl_state_normal);
		dev_err(chip->dev,
			"Can not lookup xmusb350_normal pinstate %d\n",
			ret);
		goto err_pinctrl_lookup;
	}

	chip->pinctrl_state_isp
		= pinctrl_lookup_state(chip->xmusb350_pinctrl,
					"xmusb350_isp");
	if (IS_ERR_OR_NULL(chip->pinctrl_state_isp)) {
		ret = PTR_ERR(chip->pinctrl_state_isp);
		dev_err(chip->dev,
			"Can not lookup xmusb350_isp  pinstate %d\n",
			ret);
		goto err_pinctrl_lookup;
	}

	return 0;

err_pinctrl_get:
	devm_pinctrl_put(chip->xmusb350_pinctrl);
err_pinctrl_lookup:
	chip->xmusb350_pinctrl = NULL;

	return ret;
}

static int xmusb350_charger_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int rc;
	struct xmusb350_charger *chip;
	struct power_supply_config charger_identify_psy_cfg = {};
	unsigned int boot_mode = get_boot_mode();

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		xm35_err("Failed to allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;
	mutex_init(&chip->irq_complete);
	mutex_init(&chip->isp_sequence_lock);

	chip->thermal_status = TEMP_BELOW_RANGE;
	chip->rerun_apsd_count = 0;
	chip->tcpc_attach = false;
	chip->irq_data_process_enable = false;
	chip->irq_waiting = false;
	chip->dpdm_mode = QC30_DPDM;
	chip->resume_completed = true;
	chip->hvdcp_en = true;
	chip->qc35_err_sta = QC35_ERROR_NO;
	chip->hvdcp_retry_timer = 0;
	chip->check_overload = 0;
	chip->ocp_timer = 0;
	chip->qc3_det_err_timer = 0;
	chip->hvdcp_trig_reset = false;
	chip->fw_update_flag = false;

	i2c_set_clientdata(client, chip);

	/* Get chg type det power supply */
	chip->charger_psy = power_supply_get_by_name("charger");
	if (!chip->charger_psy) {
		xm35_err("%s: get charger power supply failed, defer!\n", __func__);
		rc = -EPROBE_DEFER;
		goto err_mutex_init;
	}

	/* Get main power supply */
	chip->main_psy = power_supply_get_by_name("main");
	if (!chip->main_psy) {
		xm35_err("%s: get main power supply failed, defer!\n", __func__);
		rc = -EPROBE_DEFER;
		goto err_mutex_init;
	}
	/* Get usb power supply */
	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		xm35_err("%s: get usb power supply failed, defer!\n", __func__);
		rc = -EPROBE_DEFER;
		goto err_mutex_init;
	}

	/* Get battery power supply */
	chip->bms_psy = power_supply_get_by_name("bms");
	if (!chip->bms_psy) {
		xm35_err("%s: get bms power supply failed, defer!\n", __func__);
		rc = -EPROBE_DEFER;
		goto err_mutex_init;
	}

	/* Get battery power supply */
	chip->batt_psy = power_supply_get_by_name("battery");
	if (!chip->batt_psy) {
		xm35_err("%s: get battery power supply failed, defer!\n", __func__);
		rc = -EPROBE_DEFER;
		goto err_mutex_init;
	}

	if (chip->usb_psy)
		chip->mt_chg = power_supply_get_drvdata(chip->usb_psy);

	xmusb350_set_usbsw_state(chip, USBSW_CHG);
	rc = xmusb350_parse_dt(chip);
	if (rc) {
		xm35_err("Failed to parse DT nodes, rc=%d\n", rc);
		goto err_mutex_init;
	}

	rc = xmusb350_pinctrl_init(chip);
	if (rc) {
		xm35_err("Failed to init pinctrl, rc=%d\n", rc);
		goto err_mutex_init;
	}
	rc = pinctrl_select_state(chip->xmusb350_pinctrl, chip->pinctrl_state_normal);
	if (rc < 0)
		xm35_err("Failed to select active pinstate, rc=%d\n", rc);

	// request xmusb350 reset gpio
	rc = devm_gpio_request(&client->dev,
				chip->xmusb350_rst_gpio, "xmusb350 reset gpio");
	if (rc) {
		xm35_err("Failed to request xmusb350 reset gpio, rc=%d\n", rc);
		goto err_mutex_init;
	}
	rc = gpio_direction_output(chip->xmusb350_rst_gpio, 1);
	if (rc)
		xm35_err("Failed to set direction for xmusb350 reset gpio, rc=%d\n", rc);

	/* during HardReset, lock i2c0 bus */
	i2c_lock_bus(chip->client->adapter, I2C_LOCK_SEGMENT);
	if ((boot_mode != KERNEL_POWER_OFF_CHARGING_BOOT)
			&& (boot_mode != LOW_POWER_OFF_CHARGING_BOOT))
		xmusb350_hard_reset(chip);
	i2c_unlock_bus(chip->client->adapter, I2C_LOCK_SEGMENT);

	// request connect_therm_gpio
	rc = devm_gpio_request(&client->dev,
				chip->connect_therm_gpio, "connect_therm_gpio");
	if (rc) {
		xm35_err("Failed to request connector thermal gpio, rc=%d\n", rc);
		goto err_mutex_init;
	}
	rc = gpio_direction_output(chip->connect_therm_gpio, 0);
	if (rc)
		xm35_err("Failed to set direction for connector thermal gpio, rc=%d\n", rc);

	/* register charger_identify power supply */
	chip->charger_identify_psy_d.name = "Charger_Identify";
	chip->charger_identify_psy_d.type = POWER_SUPPLY_TYPE_CHARGER_IDENTIFY;
	chip->charger_identify_psy_d.get_property = xmusb350_get_property;
	chip->charger_identify_psy_d.properties = xmusb350_properties;
	chip->charger_identify_psy_d.num_properties = ARRAY_SIZE(xmusb350_properties);

	charger_identify_psy_cfg.drv_data = chip;
	charger_identify_psy_cfg.num_supplicants = 0;
	chip->charger_identify_psy = devm_power_supply_register(chip->dev,
				&chip->charger_identify_psy_d,
				&charger_identify_psy_cfg);
	if (IS_ERR(chip->charger_identify_psy)) {
		xm35_err("Couldn't register charger_identify psy rc=%ld\n",
				PTR_ERR(chip->charger_identify_psy));
		goto err_mutex_init;
	}

	/* charger class register */
	chip->chg_dev = charger_device_register(chip->desc->chg_dev_name,
		&client->dev, chip, &xmusb350_chg_ops, &chip->chg_props);
	if (IS_ERR_OR_NULL(chip->chg_dev))
		goto err_charger_identify_psy;

	chip->chg_consumer = charger_manager_get_by_name(chip->dev, "charger_port1");
	if (!chip->chg_consumer)
			xm35_info("%s: get charger consumer device failed\n", __func__);

	INIT_DELAYED_WORK(&chip->charger_type_det_work, xmusb350_charger_type_det_work);
	INIT_DELAYED_WORK(&chip->charger_type_recheck_work, xmusb350_charger_type_recheck);
	INIT_DELAYED_WORK(&chip->fw_update_work, i350_fw_update_work);

	/* STAT irq configuration */
	if (client->irq) {
		rc = devm_request_threaded_irq(&client->dev, client->irq,
				NULL, xmusb350_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"xmusb350 irq", chip);
		if (rc < 0) {
			xm35_err("request irq for irq=%d failed, rc =%d\n",
							client->irq, rc);
			goto err_charger_device;
		}
		/* no need to enable this irq as a wakeup source */
		/* enable_irq_wake(client->irq); */
	}
	device_init_wakeup(chip->dev, 1);

	rc = sysfs_create_group(&chip->dev->kobj, &xmusb350_attr_group);
	if (rc) {
		xm35_err("Failed to register sysfs, err:%d\n", rc);
		goto xmusb350_create_group_err;
	}

	chip->vbus_disable = false;
	INIT_DELAYED_WORK(&chip->conn_therm_work, smblib_conn_therm_work);

	// disable qc detection when bootup
	xmusb350_hvdcp_enable(chip, DISABLE);

	schedule_delayed_work(&chip->fw_update_work, msecs_to_jiffies(10000));
	xm35_info("xmusb350 successfully probed.\n");
	return 0;
xmusb350_create_group_err:
err_charger_device:
	charger_device_unregister(chip->chg_dev);
err_charger_identify_psy:
	power_supply_unregister(chip->charger_identify_psy);
err_mutex_init:
	mutex_destroy(&chip->irq_complete);
	mutex_destroy(&chip->isp_sequence_lock);
	devm_kfree(&client->dev, chip);
	return rc;
}

static int xmusb350_charger_remove(struct i2c_client *client)
{
	struct xmusb350_charger *chip = i2c_get_clientdata(client);

	xm35_info("%s: remove\n", __func__);
	mutex_destroy(&chip->irq_complete);
	mutex_destroy(&chip->isp_sequence_lock);
	chip->hvdcp_en = true;
	cancel_delayed_work_sync(&chip->conn_therm_work);
	cancel_delayed_work_sync(&chip->charger_type_det_work);
	cancel_delayed_work_sync(&chip->charger_type_recheck_work);

	return 0;
}

static int xmusb350_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct xmusb350_charger *chip = i2c_get_clientdata(client);
	union power_supply_propval val = {0,};

	if (chip->usb_psy) {
		power_supply_get_property(chip->usb_psy,
				POWER_SUPPLY_PROP_CONNECTOR_TEMP, &val);
		chip->connector_temp = val.intval;
	}

	if (chip->connector_temp < CONNECTOR_THERM_TOO_HIG)
		smblib_set_sw_conn_therm_regulation(chip, false);

	mutex_lock(&chip->irq_complete);
	chip->resume_completed = false;
	mutex_unlock(&chip->irq_complete);

	return 0;
}

static int xmusb350_suspend_noirq(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct xmusb350_charger *chip = i2c_get_clientdata(client);

	if (chip->irq_waiting) {
		pr_err_ratelimited("Aborting suspend, an interrupt was detected while suspending\n");
		return -EBUSY;
	}
	return 0;
}

static int xmusb350_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct xmusb350_charger *chip = i2c_get_clientdata(client);

	if (chip->vbus_disable)
		schedule_delayed_work(&chip->conn_therm_work,
				msecs_to_jiffies(THERM_REG_RECHECK_DELAY_200MS));

	mutex_lock(&chip->irq_complete);
	chip->resume_completed = true;
	if (chip->irq_waiting) {
		mutex_unlock(&chip->irq_complete);
		enable_irq(client->irq);
	} else {
		mutex_unlock(&chip->irq_complete);
	}
	return 0;
}

static const struct dev_pm_ops xmusb350_pm_ops = {
	.suspend	= xmusb350_suspend,
	.suspend_noirq	= xmusb350_suspend_noirq,
	.resume		= xmusb350_resume,
};

static void xmusb350_charger_shutdown(struct i2c_client *client)
{
	struct xmusb350_charger *chip = i2c_get_clientdata(client);

	xm35_info("%s: shutdown\n", __func__);

	chip->hvdcp_en = true;
	cancel_delayed_work_sync(&chip->conn_therm_work);
	cancel_delayed_work_sync(&chip->charger_type_det_work);
	cancel_delayed_work_sync(&chip->charger_type_recheck_work);
	mutex_destroy(&chip->irq_complete);
	mutex_destroy(&chip->isp_sequence_lock);
}

static const struct of_device_id xmusb350_match_table[] = {
	{ .compatible = "xm,xmusb350-charger",},
	{ },
};

static const struct i2c_device_id xmusb350_charger_id[] = {
	{"xmusb350-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, xmusb350_charger_id);

static struct i2c_driver xmusb350_charger_driver = {
	.driver		= {
		.name		= "xmusb350-charger",
		.owner		= THIS_MODULE,
		.of_match_table	= xmusb350_match_table,
		.pm		= &xmusb350_pm_ops,
	},
	.probe		= xmusb350_charger_probe,
	.remove		= xmusb350_charger_remove,
	.id_table	= xmusb350_charger_id,
	.shutdown	= xmusb350_charger_shutdown,
};

module_i2c_driver(xmusb350_charger_driver);

MODULE_DESCRIPTION("xmusb350 Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:xmusb350-charger");
