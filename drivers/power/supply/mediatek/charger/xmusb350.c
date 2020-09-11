/* Copyright (C) 2018 XiaoMi, Inc. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
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

#include "mtk_charger_intf.h"
#include "mtk_charger_init.h"

#include <linux/stat.h>
#include <linux/ctype.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/vmalloc.h>
#include <linux/preempt.h>

#define xm35_info	pr_info
#define xm35_dbg	pr_debug
#define xm35_err	pr_err
#define xm35_log	pr_err

#define XMUSB350_REG_RERUN_APSD		0x01
#define XMUSB350_REG_QC_MS			0x02
#define XMUSB350_REG_INTB_EN		0x03
#define XMUSB350_REG_SOFT_RESET		0x04
#define XMUSB350_REG_HVDCP_EN		0x05
#define XMUSB350_REG_BC12_EN		0x06
#define XMUSB350_REG_SLEEP_EN		0x07

#define XMUSB350_REG_QC30_PM		0x73
#define XMUSB350_REG_QC3_PLUS_PM	0x83


#define XMUSB350_REG_CHGT_ERROR		0x11
#define XMUSB350_REG_VIN		0x12
#define XMUSB350_REG_VID		0x13
#define XMUSB350_REG_VERSION		0x14

#define ENABLE		0x01
#define DISABLE		0x00

#define QC35_DPDM	0x01
#define QC30_DPDM	0x00

static u16 chip_version;
static u16 file_version;

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

enum qc35_error_state {
	QC35_ERROR_NO,
	QC35_ERROR_QC30_DET,
	QC35_ERROR_TA_DET,
	QC35_ERROR_TA_CAP,
	QC35_QC3_PLUS_DET_OK,
};

struct xmusb350_charger {
	struct i2c_client			*client;
	struct device				*dev;

	struct xmusb350_desc		*desc;
	struct charger_device		*chg_dev;
	struct charger_device		*chg_dev_p;
	struct charger_properties	chg_props;

	struct charger_consumer		*chg_consumer;
	struct mt_charger			*mt_chg;

	bool						tcpc_attach;
	enum charger_type			chg_type;

	int xmusb350_rst_gpio;

	bool			otg_enable;

	struct mutex				chgdet_lock;
	bool						hvdcp_dpdm_status;
	unsigned int				connect_therm_gpio;
	int							connector_temp;
	struct mutex				irq_complete;

	int							qc35_chg_type;
	int							qc35_err_sta;
	int							pulse_cnt;
	bool						vbus_disable;
	enum thermal_status_levels	thermal_status;
	int							entry_time;
	int							rerun_apsd_count;

	bool						bc12_unsupported;
	bool						hvdcp_unsupported;
	bool						intb_unsupported;
	bool						sleep_unsupported;
	struct delayed_work			conn_therm_work;
	struct delayed_work			charger_type_det_work;
	struct delayed_work			chip_reset_xmusb350;
	struct delayed_work			chip_update_work;
	struct delayed_work			hvdcp_det_retry_work;

	/* psy */
	struct power_supply			*usb_psy;
	struct power_supply			*bms_psy;
	struct power_supply			*main_psy;
	struct power_supply			*charger_psy;
	struct power_supply			*charger_identify_psy;
	struct power_supply_desc	charger_identify_psy_d;

	bool						irq_waiting;
	bool						resume_completed;
	int							dpdm_mode;
	bool						hvdcp_en;
	bool						irq_data_process_enable;

	struct pinctrl				*xmusb350_pinctrl;
	struct pinctrl_state		*pinctrl_state_normal;
	struct pinctrl_state		*pinctrl_state_isp;
	int xmusb350_sda_gpio;
	int xmusb350_scl_gpio;
	struct mutex				isp_sequence_lock;

	int							hvdcp_timer;
	int							hvdcp_retry_timer;
	int							ocp_timer;
};

struct xmusb350_desc {
	bool en_bc12;
	bool en_hvdcp;
	bool en_intb;
	bool en_sleep;
	const char *chg_dev_name;
	const char *alias_name;
};

static struct xmusb350_desc xmusb350_default_desc = {
	.en_bc12 = true,
	.en_hvdcp = true,
	.en_intb = true,
	.en_sleep = true,
	.chg_dev_name = "secondary_chg",
	.alias_name = "xmusb350",
};

static int xmusb350_psy_chg_type_changed(struct xmusb350_charger *chip, bool force_update);
static int xmusb350_set_usbsw_state(struct xmusb350_charger *chip, int state);


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

static int __xmusb350_write_byte(struct xmusb350_charger *chip, u8 reg, u8 val)
{
	s32 ret;

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

	ret = i2c_smbus_read_i2c_block_data(chip->client, reg, 4, data);
	if (ret < 0) {
		xm35_err("%s: reg[0x%02X] read failed.\n",
				__func__, reg);
		return ret;
	}

	return 0;
}

static int xmusb350_write_byte_only(struct xmusb350_charger *chip, u8 reg)
{
	return __xmusb350_write_byte_only(chip, reg);
}

static int xmusb350_write_byte(struct xmusb350_charger *chip, u8 reg, u8 data)
{
	return __xmusb350_write_byte(chip, reg, data);
}

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

static int xmusb350_rerun_apsd(struct xmusb350_charger *chip)
{
	int ret;

	ret = xmusb350_write_byte_only(chip, XMUSB350_REG_RERUN_APSD);

	if (ret)
		xm35_err("%s: rerun apsd failed!\n", __func__);
	else
		xm35_info("%s: rerun apsd success!\n", __func__);

	return ret;
}

static int xmusb350_set_vbus_disable(struct xmusb350_charger *chip,
					bool disable)
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

#define THERM_REG_RECHECK_DELAY_200MS		200
#define THERM_REG_RECHECK_DELAY_1S		1000
#define THERM_REG_RECHECK_DELAY_5S		5000
#define THERM_REG_RECHECK_DELAY_8S		8000
#define THERM_REG_RECHECK_DELAY_10S		10000

#define CONNECTOR_THERM_ABOVE			200
#define CONNECTOR_THERM_HIG			500
#define CONNECTOR_THERM_TOO_HIG			800

static int smblib_set_sw_conn_therm_regulation(struct xmusb350_charger *chip,
						bool enable)
{
	xm35_dbg("%s enable: %d\n", __func__, enable);

	if (enable) {
		chip->entry_time = ktime_get();
		schedule_delayed_work(&chip->conn_therm_work,
				msecs_to_jiffies(THERM_REG_RECHECK_DELAY_1S));

	} else {
		if (chip->thermal_status != TEMP_ABOVE_RANGE)
			cancel_delayed_work(&chip->conn_therm_work);
	}

	return 0;
}

#define CHG_DETECT_CONN_THERM_US		10000000
static void smblib_conn_therm_work(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger,
						conn_therm_work.work);
	union power_supply_propval val;
	int wdog_timeout = THERM_REG_RECHECK_DELAY_10S;
	static int thermal_status = TEMP_BELOW_RANGE;
	int usb_present;
	u64 elapsed_us;
	static bool enable_charging = true;

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
		if (thermal_status == TEMP_ABOVE_RANGE) {
			enable_charging = false;
			xm35_err("connect temp is too hot, disable vbus\n");
			charger_manager_enable_charging(chip->chg_consumer,
				MAIN_CHARGER, false);
			xmusb350_set_vbus_disable(chip, true);
		} else {
			xm35_err("connect temp normal recovery vbus\n");
			if (enable_charging == false) {
				charger_manager_enable_charging(chip->chg_consumer,
					MAIN_CHARGER, true);
				enable_charging = true;
			}
			xmusb350_set_vbus_disable(chip, false);
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
	if (!usb_present && thermal_status == TEMP_BELOW_RANGE)
		xm35_dbg("usb is disconnet cancel the connect them work\n");
	else
		schedule_delayed_work(&chip->conn_therm_work,
				msecs_to_jiffies(wdog_timeout));
}

static int xmusb350_soft_reset(struct xmusb350_charger *chip, int number)
{
	return 0;
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

	mdelay(200);
	return ret;
}

static int xmusb350_intb_enable(struct xmusb350_charger *chip, bool control)
{
	return 0;
}

static int xmusb350_hvdcp_enable(struct xmusb350_charger *chip, bool control)
{
	int ret = -1;

	if (chip->hvdcp_en == false)
		return ret;

	if (control == ENABLE)
		mdelay(500);

	ret = xmusb350_write_byte(chip, XMUSB350_REG_HVDCP_EN, (u8)control);

	if (ret)
		xm35_err("%s: hvdcp enable/disable[%d] failed!\n", __func__, control);
	else
		xm35_err("%s: hvdcp enable/disable[%d] success!\n", __func__, control);

	return ret;
}

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

static int xmusb350_sleep_enable(struct xmusb350_charger *chip, bool control)
{
	return 0;
}

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
			xm35_log("%s: xmusb_vid = %08x, %d\n", __func__, cm->xmusb_vid, cm->xmusb_vid);

			if (cm->xmusb_vid == 0x584d3335)
				ret = 2;
		}
	}

	return ret;
}

void xmusb350_hard_reset(struct xmusb350_charger *chip)
{
	gpio_set_value(chip->xmusb350_rst_gpio, 1);
	mdelay(10);
	gpio_set_value(chip->xmusb350_rst_gpio, 0);
	mdelay(60);
}

#define HVDCP_RETRY_TIMER			3
#define HVDCP_DET_RETRY_DELAY		10000
static void xmusb350_hvdcp_det_retry_work(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger,
						hvdcp_det_retry_work.work);
	int ret, type;
	struct charger_manager *cm = NULL;
	enum usbsw_state usbsw;

	ret = xmusb350_get_charger_type(chip, &type);
	if (ret) {
		xm35_err("%s: get charger type failed[%d]!\n", __func__, type);
	} else {
		xm35_log("%s: charger type is [%d]!\n", __func__, type);

		if (type == QC35_HVDCP) {
			if (chip->hvdcp_retry_timer++ >= HVDCP_RETRY_TIMER) {
				pr_info("qc detect failed, force DCP 5v 1.5A\n");
				type = QC35_DCP;

				if ((chip->chg_consumer) && (chip->chg_consumer->cm))
					cm = chip->chg_consumer->cm;

				chip->qc35_chg_type = type;
				if (cm)
					cm->xmusb_chr_type = type;

				chip->chg_type = STANDARD_CHARGER;
				chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB_DCP;

				xm35_log("%s: MTK type is [%02x], QC35 type is [%02x], usb_desc.type is [%02x].\n", __func__,
							chip->chg_type, chip->qc35_chg_type, chip->mt_chg->usb_desc.type);

				if ((chip->chg_type == STANDARD_HOST) || (chip->chg_type == CHARGING_HOST)) {
					usbsw = USBSW_USB;
					xmusb350_set_usbsw_state(chip, usbsw);
				}
				xmusb350_psy_chg_type_changed(chip, true);
			} else {
				ret = xmusb350_hvdcp_enable(chip, ENABLE);
				if (ret)
					xm35_err("%s: hvdcp enable failed!\n", __func__);
				schedule_delayed_work(&chip->hvdcp_det_retry_work,
					msecs_to_jiffies(HVDCP_DET_RETRY_DELAY));
			}
		} else {
			chip->hvdcp_retry_timer = 0;
		}
	}
}

#define HVDCP_TIMER			3
#define OCP_TIMER			3
#define VBUS_PLUG_OUT_THR_MV			3000
#define QC3_DP_DM_PR_ICL 1500000
#define QC3_DP_DM_PR_FCC 2000000
static void xmusb350_charger_type_det_work(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger,
						charger_type_det_work.work);
	int ret, type, i;
	static int last_type;
	union power_supply_propval pval = {0,};
	struct charger_manager *cm = NULL;
	enum usbsw_state usbsw;

	if (chip->otg_enable)
		return;

	ret = xmusb350_get_charger_type(chip, &type);
	if (ret) {
		xm35_err("%s: get charger type failed[%d]!\n", __func__, type);
	} else {
		xm35_log("%s: charger type is [%d]!\n", __func__, type);

		pr_info("%s: last_type=%d type=%d\n", __func__, last_type, type);
		if (type == QC35_OCP || type == QC35_FLOAT) {
			if ((last_type == QC35_OCP && type == QC35_FLOAT) ||
				(last_type == QC35_FLOAT && type == QC35_OCP)) {
				if (chip->ocp_timer++ >= OCP_TIMER) {
					pr_info("%s: Non standard OCP, force DCP 5v 1.5A\n", __func__);
					type = QC35_DCP;
				}
			}
		} else {
			chip->ocp_timer = 0;
		}
		last_type = type;

		if (type == QC35_HVDCP) {
			if (chip->hvdcp_timer++ >= HVDCP_TIMER) {
				pr_info("%s: Non standard QC2.0, force DCP 5v 1.5A\n", __func__);
				type = QC35_DCP;

				cancel_delayed_work_sync(&chip->hvdcp_det_retry_work);
			} else {
				ret = xmusb350_hvdcp_enable(chip, ENABLE);
				if (ret)
					xm35_err("%s: hvdcp enable failed!\n", __func__);

				schedule_delayed_work(&chip->hvdcp_det_retry_work,
					msecs_to_jiffies(HVDCP_DET_RETRY_DELAY));
			}
		} else {
			chip->hvdcp_timer = 0;
			cancel_delayed_work_sync(&chip->hvdcp_det_retry_work);
		}

		if (type == QC35_HVDCP_30) {
			chip->dpdm_mode = QC30_DPDM;
			ret = xmusb350_qc_mode_select(chip, XMUSB350_MODE_QC20_V5);
			if (ret) {
				xm35_err("%s: qc mode select failed!\n", __func__);
			} else {
				xm35_log("%s: enter qc20 5V mode!\n", __func__);
				ret = xmusb350_qc_mode_select(chip, XMUSB350_MODE_QC30_V5);
				if (ret) {
					xm35_err("%s: qc mode select failed!\n", __func__);
				} else {
					if (chip->chg_dev_p != NULL) {
						charger_dev_set_input_current(chip->chg_dev_p, QC3_DP_DM_PR_ICL);
						charger_dev_set_charging_current(chip->chg_dev_p, QC3_DP_DM_PR_FCC);
					}
					xm35_log("%s: enter qc30 5V mode!\n", __func__);
					for (i = 0; i < 38; i++) {
						xmusb350_hvdcp_dpdm(chip, 0x8001);
						mdelay(50);
					}
					xm35_log("%s: dp dm process done!\n", __func__);
					mdelay(300);

					if (chip->otg_enable)
						return;

					chip->usb_psy = power_supply_get_by_name("usb");
					if (!chip->usb_psy) {
						xm35_err("%s: get usb power supply failed\n", __func__);
					} else {
						ret = power_supply_get_property(chip->usb_psy,
								POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
						xm35_log("%s: usb voltage is [%d]!\n", __func__, pval.intval);

						if (pval.intval <= 12350)
							type = QC35_HVDCP_30_18;
						else
							type = QC35_HVDCP_30_27;

						if (pval.intval <= VBUS_PLUG_OUT_THR_MV) {
							chip->qc35_chg_type = QC35_NA;
							return;
						}
						ret = xmusb350_qc_mode_select(chip, XMUSB350_MODE_QC20_V5);
						if (ret) {
							xm35_err("%s: qc mode select failed!\n", __func__);
						}  else {
							xm35_log("%s: enter qc20 5V mode!\n", __func__);
							ret = xmusb350_qc_mode_select(chip, XMUSB350_MODE_QC30_V5);
							if (ret)
								xm35_err("%s: qc mode select failed!\n", __func__);
							else
								xm35_log("%s: enter qc30 5V mode!\n", __func__);
						}

						ret = power_supply_get_property(chip->usb_psy,
								POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
						xm35_log("%s: usb voltage is [%d]!\n", __func__, pval.intval);

						if (pval.intval <= VBUS_PLUG_OUT_THR_MV) {
							chip->qc35_chg_type = QC35_NA;
							return;
						}
						if (chip->otg_enable)
							return;
					}
				}
			}
		}

		if (type != chip->qc35_chg_type) {
			if ((chip->chg_consumer) && (chip->chg_consumer->cm))
				cm = chip->chg_consumer->cm;

			chip->qc35_chg_type = type;
			if (cm)
				cm->xmusb_chr_type = type;

			switch (chip->qc35_chg_type) {
			case QC35_SDP:
				chip->chg_type = STANDARD_HOST;
				break;
			case QC35_CDP:
				chip->chg_type = CHARGING_HOST;
				break;
			case QC35_OCP:
			case QC35_DCP:
				chip->chg_type = STANDARD_CHARGER;
				chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
				break;
			case QC35_HVDCP:
			case QC35_HVDCP_20:
				chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB_HVDCP;
				chip->chg_type = STANDARD_CHARGER;
				break;
			case QC35_HVDCP_30:
			case QC35_HVDCP_30_18:
			case QC35_HVDCP_30_27:
				chip->dpdm_mode = QC30_DPDM;
				chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB_HVDCP_3;
				chip->chg_type = STANDARD_CHARGER;
				break;
			case QC35_HVDCP_3_PLUS_18:
			case QC35_HVDCP_3_PLUS_27:
				chip->dpdm_mode = QC35_DPDM;
				chip->chg_type = STANDARD_CHARGER;
				chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS;
				pr_info("ffc qc35 enable ffc\n");
				chg_set_fastcharge_mode(true);
				break;
			case QC35_FLOAT:
				chip->chg_type = NONSTANDARD_CHARGER;
				break;
			case QC35_NA:
			case QC35_UNKNOW:
				chip->chg_type = CHARGER_UNKNOWN;
				break;
			default:
				chip->chg_type = CHARGER_UNKNOWN;
				break;
			}
			xm35_log("%s: MTK type is [%02x], QC35 type is [%02x], usb_desc.type is [%02x].\n", __func__,
						chip->chg_type, chip->qc35_chg_type, chip->mt_chg->usb_desc.type);

			if ((chip->chg_type == STANDARD_HOST) || (chip->chg_type == CHARGING_HOST)) {
				usbsw = USBSW_USB;
				xmusb350_set_usbsw_state(chip, usbsw);
			}
			xmusb350_psy_chg_type_changed(chip, true);
		}
	}
}


static enum power_supply_property xmusb350_properties[] = {
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_QC35_CHG_TYPE,
	POWER_SUPPLY_PROP_QC35_ERROR_STATE,
	POWER_SUPPLY_PROP_QC35_VIN,
	POWER_SUPPLY_PROP_QC35_VERSION,
	POWER_SUPPLY_PROP_QC35_VID,
	POWER_SUPPLY_PROP_QC35_CHIP_OK,
	POWER_SUPPLY_PROP_QC35_RERUN_APSD,
	POWER_SUPPLY_PROP_QC35_SOFT_RESET,
	POWER_SUPPLY_PROP_QC35_MODE_SELECT,
	POWER_SUPPLY_PROP_QC35_INTB_ENABLE,
	POWER_SUPPLY_PROP_QC35_HVDCP_ENABLE,
	POWER_SUPPLY_PROP_QC35_BC12_ENABLE,
	POWER_SUPPLY_PROP_QC35_SLEEP_ENABLE,
	POWER_SUPPLY_PROP_QC35_HVDCP_DPDM,
	POWER_SUPPLY_PROP_VBUS_DISABLE,
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
	int type = QC35_NA;

	switch (prop) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "xmusb350";
		break;
	case POWER_SUPPLY_PROP_QC35_CHG_TYPE:
		rc = xmusb350_get_charger_type(chip, &type);
		if (rc) {
			xm35_err("%s: get charger type failed[%d]!\n", __func__, type);
			type = QC35_UNKNOW;
		}

		if (type == QC35_UNKNOW || type == QC35_NA)
			chip->qc35_chg_type = QC35_UNKNOW;

		val->intval = chip->qc35_chg_type;
		break;
	case POWER_SUPPLY_PROP_QC35_ERROR_STATE:
		rc = xmusb350_get_error_state(chip, &chip->qc35_err_sta);
		if (!rc)
			val->intval = chip->qc35_err_sta;
		else
			return -EAGAIN;
		break;
	case POWER_SUPPLY_PROP_QC35_VIN:
		rc = xmusb350_get_vin_vol(chip, &vin);
		if (!rc)
			val->intval = (vin * 2400 * 502) / (1024 * 62);
		else
			return -EAGAIN;
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
	case POWER_SUPPLY_PROP_QC35_VID:
		rc = xmusb350_get_vendor_id(chip, buf);
		if ((rc == 2) || (rc == 0)) {
			rc = 0;
			memcpy(val->arrayval, buf, 4);
		} else {
			return -EAGAIN;
		}
		break;
	case POWER_SUPPLY_PROP_QC35_CHIP_OK:
		rc = xmusb350_get_vendor_id(chip, buf);
		if (rc == 2) {
			rc = 0;
			val->intval = 1;
		} else {
			return -EAGAIN;
		}
		break;
	case POWER_SUPPLY_PROP_VBUS_DISABLE:
		val->intval = chip->vbus_disable;
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

static int xmusb350_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct xmusb350_charger *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_QC35_RERUN_APSD:
		rc = xmusb350_rerun_apsd(chip);
		break;
	case POWER_SUPPLY_PROP_QC35_SOFT_RESET:
		rc = xmusb350_soft_reset(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_QC35_MODE_SELECT:
		rc = xmusb350_qc_mode_select(chip, (u8)val->intval);
		break;
	case POWER_SUPPLY_PROP_QC35_INTB_ENABLE:
		rc = xmusb350_intb_enable(chip, (bool)val->intval);
		break;
	case POWER_SUPPLY_PROP_QC35_HVDCP_ENABLE:
		rc = xmusb350_hvdcp_enable(chip, (bool)val->intval);
		break;
	case POWER_SUPPLY_PROP_QC35_BC12_ENABLE:
		rc = xmusb350_bc12_enable(chip, (bool)val->intval);
		break;
	case POWER_SUPPLY_PROP_QC35_SLEEP_ENABLE:
		rc = xmusb350_sleep_enable(chip, (bool)val->intval);
		break;
	case POWER_SUPPLY_PROP_QC35_HVDCP_DPDM:
		rc = xmusb350_hvdcp_dpdm(chip, (int)val->intval);
		break;
	case POWER_SUPPLY_PROP_VBUS_DISABLE:
		xmusb350_set_vbus_disable(chip, !!val->intval);
		break;
	default:
		xm35_err("%s: unsupported property %d\n", __func__, psp);
		return -ENODATA;
	}

	return rc;
}

static int xmusb350_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_QC35_RERUN_APSD:
	case POWER_SUPPLY_PROP_QC35_SOFT_RESET:
	case POWER_SUPPLY_PROP_QC35_MODE_SELECT:
	case POWER_SUPPLY_PROP_QC35_INTB_ENABLE:
	case POWER_SUPPLY_PROP_QC35_HVDCP_ENABLE:
	case POWER_SUPPLY_PROP_QC35_BC12_ENABLE:
	case POWER_SUPPLY_PROP_QC35_SLEEP_ENABLE:
	case POWER_SUPPLY_PROP_QC35_HVDCP_DPDM:
	case POWER_SUPPLY_PROP_VBUS_DISABLE:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}
	return rc;
}

void dump_regs(struct xmusb350_charger *chip)
{
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

	return ret;

	dump_regs(chip);
	return ret;
}

static int xmusb350_psy_chg_type_changed(struct xmusb350_charger *chip, bool force_update)
{
	int ret = 0;
	union power_supply_propval propval;
	struct charger_manager *cm = NULL;
	int pd_active = 0;

	chip->charger_psy = power_supply_get_by_name("charger");
	if (!chip->charger_psy) {
		xm35_err("%s: get charger power supply failed\n", __func__);
		return -EINVAL;
	}

	chip->main_psy = power_supply_get_by_name("main");
	if (!chip->main_psy) {
		xm35_err("%s: get main power supply failed\n", __func__);
	}

	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		xm35_err("%s: get usb power supply failed\n", __func__);
		return -EINVAL;
	}
	if (chip->usb_psy != NULL) {
		chip->mt_chg = power_supply_get_drvdata(chip->usb_psy);
		xm35_dbg("%s: get usb_psy drvdata.\n", __func__);
	}

	if (!chip->otg_enable) {
		propval.intval = chip->chg_type;
		ret = power_supply_set_property(chip->charger_psy,
						POWER_SUPPLY_PROP_CHARGE_TYPE,
						&propval);
		if (ret < 0)
			xm35_err("%s: psy type failed, ret = %d\n", __func__, ret);
		else
			xm35_info("%s: chg_type = %d\n", __func__, chip->chg_type);
	}

	if ((chip->chg_consumer) && (chip->chg_consumer->cm))
		cm = chip->chg_consumer->cm;
	if (cm) {
		if ((cm->pd_type == MTK_PD_CONNECT_PE_READY_SNK ||
			cm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 ||
			cm->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) &&
			!force_update)
			return 0;
	}

	ret = power_supply_get_property(chip->usb_psy,
					POWER_SUPPLY_PROP_PD_ACTIVE,
					&propval);
	pd_active = propval.intval;
	if (ret < 0)
		xm35_err("%s: psy get pd_active failed, ret = %d\n", __func__, ret);
	else
		xm35_info("%s: pd_active = %d\n", __func__, pd_active);

	if (pd_active != POWER_SUPPLY_PD_INACTIVE) {
		chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB_PD;
		return 0;
	}

	if (((chip->mt_chg->usb_desc.type == POWER_SUPPLY_TYPE_USB_HVDCP)
		|| (chip->mt_chg->usb_desc.type == POWER_SUPPLY_TYPE_USB_HVDCP_3)
		|| (chip->mt_chg->usb_desc.type == POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS))
		&& chip->chg_type == STANDARD_CHARGER)
		return 0;

	if (chip->chg_type > sizeof(xmusb350_apsd_results))
		chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
	else
		chip->mt_chg->usb_desc.type = xmusb350_apsd_results[chip->chg_type];

	xm35_log("%s: usb type is [%02x].\n", __func__, chip->mt_chg->usb_desc.type);
	return ret;
}

static int xmusb350_set_usbsw_state(struct xmusb350_charger *chip, int state)
{
	xm35_log("%s: state = %s\n", __func__, state ? "usb" : "chg");

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
	struct charger_manager *cm = NULL;
	int ret = 0;
	int i;
	const int max_wait_cnt = 200;
	enum usbsw_state usbsw = en ? USBSW_CHG : USBSW_USB;
	union power_supply_propval pval = {0,};
	int pd_active = 0;

	xm35_info("%s: en = %d\n", __func__, en);

	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		xm35_err("%s: get usb power supply failed\n", __func__);
		return -EINVAL;
	}
	if (chip->usb_psy != NULL) {
		chip->mt_chg = power_supply_get_drvdata(chip->usb_psy);
		xm35_err("%s: get usb_psy drvdata.\n", __func__);
	}

	if (chip->tcpc_attach == en) {
		xm35_info("%s: attach(%d) is the same\n",
				    __func__, en);
		goto out;
	}
	chip->tcpc_attach = en;

	if (!en) {
		chip->chg_type = CHARGER_UNKNOWN;
		goto out;
	}

	for (i = 0; i < max_wait_cnt; i++) {
		if (is_usb_rdy())
			break;
		pr_info("%s: CDP block\n", __func__);
		if (!chip->tcpc_attach) {
			pr_info("%s: plug out", __func__);
			goto out;
		}
		msleep(100);
	}
	if (i == max_wait_cnt)
		pr_err("%s: CDP timeout\n", __func__);
	else
		pr_info("%s: CDP free\n", __func__);

	xmusb350_set_usbsw_state(chip, USBSW_CHG);

	ret = power_supply_get_property(chip->usb_psy,
					POWER_SUPPLY_PROP_PD_ACTIVE,
					&pval);
	pd_active = pval.intval;
	if (ret < 0)
		xm35_err("%s: psy get pd_active failed, ret = %d\n", __func__, ret);
	else
		xm35_info("%s: pd_active = %d\n", __func__, pd_active);

	if (pd_active != POWER_SUPPLY_PD_INACTIVE)
		goto out;

	ret = xmusb350_get_error_state(chip, &chip->qc35_err_sta);
	if (ret) {
		xm35_err("%s: get error state failed[%d]!\n", __func__, chip->qc35_err_sta);
		goto out;
	}

	if (chip->qc35_chg_type != QC35_HVDCP_30_27 &&
		chip->qc35_chg_type != QC35_HVDCP_30_18 &&
		chip->qc35_chg_type != QC35_DCP) {
		ret = xmusb350_get_charger_type(chip, &chip->qc35_chg_type);
		if (ret) {
			xm35_err("%s: get charger type failed[%d]!\n", __func__, chip->qc35_chg_type);
			goto out;
		}
	}

	xm35_log("%s: charger type is [%d]!\n", __func__, chip->qc35_chg_type);

	if ((chip->chg_consumer) && (chip->chg_consumer->cm))
		cm = chip->chg_consumer->cm;
	if (cm) {
		if (chip->qc35_chg_type > QC35_UNKNOW)
			cm->xmusb_chr_type = QC35_UNKNOW;
		else
			cm->xmusb_chr_type = chip->qc35_chg_type;
	}

	switch (chip->qc35_chg_type) {
	case QC35_SDP:
		chip->chg_type = STANDARD_HOST;
		break;
	case QC35_CDP:
		chip->chg_type = CHARGING_HOST;
		break;
	case QC35_OCP:
	case QC35_DCP:
		chip->chg_type = STANDARD_CHARGER;
		break;
	case QC35_HVDCP:
	case QC35_HVDCP_20:
		chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB_HVDCP;
		chip->chg_type = STANDARD_CHARGER;
		break;
	case QC35_HVDCP_30:
	case QC35_HVDCP_30_18:
	case QC35_HVDCP_30_27:
		chip->dpdm_mode = QC30_DPDM;
		chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB_HVDCP_3;
		chip->chg_type = STANDARD_CHARGER;
		break;
	case QC35_HVDCP_3_PLUS_18:
	case QC35_HVDCP_3_PLUS_27:
		chip->dpdm_mode = QC35_DPDM;
		chip->chg_type = STANDARD_CHARGER;
		chip->mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS;
		break;
	case QC35_FLOAT:
		chip->chg_type = NONSTANDARD_CHARGER;
		break;
	case QC35_NA:
	case QC35_UNKNOW:
		chip->chg_type = CHARGER_UNKNOWN;
		break;
	default:
		chip->chg_type = CHARGER_UNKNOWN;
	}
	xm35_log("%s: MTK type is [%02x], QC35 type is [%02x], usb type is [%02x].\n", __func__,
				chip->chg_type, chip->qc35_chg_type, chip->mt_chg->usb_desc.type);
	if ((chip->chg_type == STANDARD_HOST) || (chip->chg_type == CHARGING_HOST))
		usbsw = USBSW_USB;
out:
	xmusb350_psy_chg_type_changed(chip, true);
	xmusb350_set_usbsw_state(chip, usbsw);
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
	int ret = 0;

	xm35_info("%s: en = %d\n", __func__, en);

	chip->otg_enable = en;
	xmusb350_hard_reset(chip);
	ret = xmusb350_bc12_enable(chip, !en);
	if (ret)
		xm35_err("%s: [OTG] can not shut bc12, ret = %d.\n", __func__, ret);
	return 0;
}

static int xmusb350_plug_out(struct charger_device *chg_dev)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);
	struct charger_manager *cm = NULL;
	int rc = -1;
	int type = QC35_NA;

	xm35_info("xmusb35 plug out enter\n");

	chip->hvdcp_timer = 0;
	chip->ocp_timer = 0;

	if ((chip->chg_consumer) && (chip->chg_consumer->cm))
		cm = chip->chg_consumer->cm;

	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy)
		xm35_err("%s: get usb power supply failed\n", __func__);

	if (!chip->otg_enable)
		rc = xmusb350_get_charger_type(chip, &type);
	if ((((rc == 0) && (type == QC35_UNKNOW)) || (rc != 0)) && !chip->otg_enable) {
		chip->qc35_chg_type = QC35_NA;
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

	rc = smblib_set_sw_conn_therm_regulation(chip, false);
	if (rc < 0)
		xm35_err("%s: Couldn't stop SW conn therm rc=%d\n", __func__, rc);

	if (cm) {
		if (cm->xmusb_vid != 0x584d3335)
			return 0;
	}
	if (!chip->otg_enable)
		xmusb350_psy_chg_type_changed(chip, true);

	if (chip->usb_psy != NULL && !chip->otg_enable)
		power_supply_changed(chip->usb_psy);

	return 0;
}

static int xmusb350_plug_in(struct charger_device *chg_dev)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);
	int rc;

	xm35_info("xmusb35 plug in enter\n");

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
	rc = smblib_set_sw_conn_therm_regulation(chip, true);
	if (rc < 0)
		xm35_err("%s: Couldn't start SW conn therm rc=%d\n", __func__, rc);
	return 0;
}

static struct charger_ops xmusb350_chg_ops = {
	.dump_registers = xmusb350_dump_register,
	.enable_chg_type_det = xmusb350_enable_chg_type_det,
	.plug_out = xmusb350_plug_out,
	.enable_hvdcp_det = xmusb350_enable_hvdcp_det,
	.plug_in = xmusb350_plug_in,
	.enable_otg = xmusb350_enable_otg,
};

static irqreturn_t xmusb350_interrupt(int irq, void *dev_id)
{
	struct xmusb350_charger *chip = dev_id;
	int ret = 0;
	int type;
	union power_supply_propval propval = {0,};
	int pd_verify_in_process = 0;

	xm35_info("%s: INT OCCURED\n", __func__);

	if (chip->usb_psy) {
		ret = power_supply_get_property(chip->usb_psy,
					POWER_SUPPLY_PROP_PD_VERIFY_IN_PROCESS,
					&propval);
		pd_verify_in_process = propval.intval;
		if (ret < 0)
			xm35_err("%s: psy get pd verify process prop failed, ret = %d\n",
					__func__, ret);

		if (pd_verify_in_process == 1) {
			pr_err("device is in pd verify process, ignore\n");
			return IRQ_HANDLED;
		}
	}

	ret = xmusb350_get_charger_type(chip, &type);
	if (ret) {
		xm35_err("%s: get charger type failed[%d]!\n", __func__, type);
	} else {
		xm35_log("%s: xmusb350 charger type is [%d]!\n", __func__, type);
	}

	if ((chip->mt_chg->usb_desc.type == POWER_SUPPLY_TYPE_USB_PD)
		&& (type != QC35_SDP)) {
		pr_err("pd adapter present, no need do bc12 later process\n");
		return IRQ_HANDLED;
	}

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

char xmusb350_addr = 0x2B;
char xmusb350_data = 0x48;
char xmusb350_addr_real = 0x34;

int xmusb350_i2c_master_send(const struct i2c_client *client, const char *buf, int count)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;

	msg.addr = xmusb350_addr_real;
	msg.flags = 0x0000;
	msg.flags = msg.flags | I2C_M_STOP;
	msg.len = count;
	msg.buf = (char *)buf;


	ret = i2c_transfer(adap, &msg, 1);

	return (ret == 1) ? count : ret;
}

int xmusb350_i2c_get_chip_id(const struct i2c_client *client, const char *buf)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg[2];
	char send_buf[2] = {0x80, 0x00};

	msg[0].addr = xmusb350_addr_real;
	msg[0].len = 2;
	msg[0].buf = send_buf;

	msg[1].addr = xmusb350_addr_real;
	msg[1].flags = I2C_M_STOP & I2C_M_NOSTART;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = (char *)buf;

	ret = i2c_transfer(adap, &msg[0], 2);

	return ret;
}

static ssize_t isp_reset_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	int cmd = 0;
	struct xmusb350_charger *chip = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	xmusb350_hard_reset(chip);

	dev_err(&chip->client->dev, "%s: reset finish.\n",
			__func__);
	return count;
}

static int xmusb350_i2c_sequence_send(const struct i2c_client *client, const char *buf, int count)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;

	msg.addr = xmusb350_addr;
	msg.flags = 0x0000;
	msg.flags = msg.flags | I2C_M_NO_RD_ACK | I2C_M_IGNORE_NAK | I2C_M_NOSTART;
	msg.len = count;
	msg.buf = (char *)buf;


	ret = i2c_transfer(adap, &msg, 1);

	return (ret == 1) ? count : ret;

}

static ssize_t isp_i2c_addr_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%02x\n", xmusb350_addr);
}

static ssize_t isp_i2c_addr_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	struct xmusb350_charger *chip = dev_get_drvdata(dev);
	if (sscanf(buf, "%2hhx", &xmusb350_addr) != 1)
		return -EINVAL;

	dev_err(&chip->client->dev, "%s: addr is %02x\n",
			__func__, xmusb350_addr);
	return count;
}

static ssize_t isp_i2c_data_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%02x\n", xmusb350_data);
}

static ssize_t isp_i2c_data_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	struct xmusb350_charger *chip = dev_get_drvdata(dev);
	if (sscanf(buf, "%2hhx", &xmusb350_data) != 1)
		return -EINVAL;

	dev_err(&chip->client->dev, "%s: data is %02x\n",
			__func__, xmusb350_data);

	return count;
}

static ssize_t enter_isp_and_get_chipid_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	char chip_id = 0x00;
	int rc = 0;
	char i2c_buf[1] = {0};
	char i2c_buf_enable_cmd[7] = {0x57, 0x54, 0x36, 0x36, 0x37, 0x30, 0x46};

	struct xmusb350_charger *chip = dev_get_drvdata(dev);

	i2c_buf[0] = xmusb350_data;
	dev_err(chip->dev, "%s: step 1\n", __func__);
	gpio_set_value(chip->xmusb350_rst_gpio, 1);
	mdelay(15);
	gpio_set_value(chip->xmusb350_rst_gpio, 0);
	mdelay(1);

	xmusb350_i2c_sequence_send(chip->client, i2c_buf, 1);
	mdelay(10);
	rc = xmusb350_i2c_master_send(chip->client, i2c_buf_enable_cmd, 7);

	rc = xmusb350_i2c_get_chip_id(chip->client, &chip_id);
	dev_err(chip->dev, "%s: rc=%d, chip_id=%02x\n", __func__, rc, chip_id);

	return scnprintf(buf, PAGE_SIZE, "%02x\n", chip_id);
}

static ssize_t isp_i2c_addr_real_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%02x\n", xmusb350_addr_real);
}

static ssize_t isp_i2c_addr_real_store(struct device *dev,
struct device_attribute *attr,
const char *buf, size_t count)
{
	struct xmusb350_charger *chip = dev_get_drvdata(dev);
	if (sscanf(buf, "%2hhx", &xmusb350_addr_real) != 1)
		return -EINVAL;

	dev_err(&chip->client->dev, "%s: addr is %02x\n",
			__func__, xmusb350_addr_real);
	return count;
}

static ssize_t isp_pinctrl_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	char chip_id = 0x00;
	int rc = 0;
	u16 version = 0;
	char i2c_buf_enable_cmd[7] = {0x57, 0x54, 0x36, 0x36, 0x37, 0x30, 0x46};
	struct xmusb350_charger *chip = dev_get_drvdata(dev);

	rc = xmusb350_get_version(chip, &version);
	if ((rc) || (version != 0x0D0A)) {
		xm35_err("get version failed and will update firmware, rc=%d.\n", rc);
		mutex_lock(&chip->isp_sequence_lock);
		dev_err(chip->dev, "%s: step 1\n", __func__);
		gpio_set_value(chip->xmusb350_rst_gpio, 1);
		mdelay(10);
		gpio_set_value(chip->xmusb350_rst_gpio, 0);
		mdelay(30);

		dev_err(chip->dev, "%s: step 2\n", __func__);
		dev_err(chip->dev, "%s: step 2.1: select pinstate\n", __func__);
		rc = pinctrl_select_state(chip->xmusb350_pinctrl,
					chip->pinctrl_state_isp);
		if (rc < 0) {
			dev_err(chip->dev,
				"%s: Failed to select isp pinstate %d\n",
				__func__, rc);
			goto error;
		}

		dev_err(chip->dev, "%s: step 2.2: request 2 gpio\n", __func__);
		rc = devm_gpio_request(chip->dev,
					chip->xmusb350_sda_gpio, "xmusb350_sda_gpio");
		if (rc) {
			xm35_err("request xmusb350_sda_gpio failed, rc=%d\n",
					rc);
			goto error;
		}
		rc = devm_gpio_request(chip->dev,
					chip->xmusb350_scl_gpio, "xmusb350_scl_gpio");
		if (rc) {
			xm35_err("request xmusb350_scl_gpio failed, rc=%d\n",
					rc);
			goto error;
		}

		gpio_direction_output(chip->xmusb350_sda_gpio, 1);
		gpio_direction_output(chip->xmusb350_scl_gpio, 1);

		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 0);// sda:0
		udelay(5);

		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		gpio_set_value(chip->xmusb350_sda_gpio, 0);// sda:[0]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 1);// sda:[1]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 0);// sda:[0]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 1);// sda:[1]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 0);// sda:[0]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 1);// sda:[1]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 1);// sda:[1]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 0);// sda:[0]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 1);// sda:[1]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 0);// sda:[0]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 1);// sda:[1]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 0);// sda:[0]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 0);// sda:[0]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 1);// sda:[1]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		gpio_set_value(chip->xmusb350_sda_gpio, 0);// sda:[0]
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 1);// scl:1
		udelay(5);
		gpio_set_value(chip->xmusb350_scl_gpio, 0);// scl:0

		mdelay(10);

		devm_gpio_free(chip->dev,
					chip->xmusb350_sda_gpio);
		devm_gpio_free(chip->dev,
					chip->xmusb350_scl_gpio);
		dev_err(chip->dev, "%s: step 2.2: select pinstate normal.\n", __func__);
		rc = pinctrl_select_state(chip->xmusb350_pinctrl,
					chip->pinctrl_state_normal);
		if (rc < 0) {
			dev_err(chip->dev,
				"%s: Failed to select normal pinstate %d\n",
				__func__, rc);
			goto error;
		}

		mutex_unlock(&chip->isp_sequence_lock);
		dev_err(chip->dev, "%s: step 3\n", __func__);
		rc = xmusb350_i2c_master_send(chip->client, i2c_buf_enable_cmd, 7);

		dev_err(chip->dev, "%s: step 4\n", __func__);
		rc = xmusb350_i2c_get_chip_id(chip->client, &chip_id);
		dev_err(chip->dev, "%s: rc=%d, chip_id=%02x\n", __func__, rc, chip_id);
	}

error:
	pinctrl_select_state(chip->xmusb350_pinctrl,
					chip->pinctrl_state_normal);
	devm_gpio_free(chip->dev,
				chip->xmusb350_sda_gpio);
	devm_gpio_free(chip->dev,
				chip->xmusb350_scl_gpio);
	return scnprintf(buf, PAGE_SIZE, "%02x\n", chip_id);
}

#define XMUSB350_FW_NAME		"WT6670F_QC3p_backup.bin"
#define XMUSB350_FW_NAME_INTER		"WT6670F_QC3p.bin"

unsigned char AnalyseHEX(const u8 *inputHexData, int len)
{
    unsigned char m_checksum = 0x00;
	int i = 0;

    for (i = 0; i < len; i++) {
       m_checksum += *inputHexData;
       inputHexData++;
    }
    m_checksum %= 256;
    m_checksum = 0x100 - m_checksum;

    return m_checksum;
}

int xmusb350_i2c_read_cmd(const struct i2c_client *client, char read_addr, const char *buf)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg[2];
	char send_buf[2] = {0x61, 0x00};

	send_buf[1] = read_addr;

	msg[0].addr = xmusb350_addr_real;
	msg[0].len = 2;
	msg[0].buf = send_buf;

	msg[1].addr = xmusb350_addr_real;
	msg[1].flags = I2C_M_STOP & I2C_M_NOSTART;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = 64;
	msg[1].buf = (char *)buf;

	ret = i2c_transfer(adap, &msg[0], 2);

	return ret;
}


#define FIRMWARE_FILE_LENGTH			15000
#define UPDATE_SUCCESS					0
#define ERROR_REQUESET_FIRMWARE			-1
#define ERROR_CHECK_FIREWARE_FORMAT		-2
#define ERROR_GET_CHIPID_FAILED			-3
#define ERROR_ERASE_FAILED				-4
#define ERROR_FINISH_CMD_FAILED			-5
#define ERROR_FILE_CRC_FAILED			-6
#define ERROR_HIGHADD_CMD_FAILED		-7
#define ERROR_PROGRAM_CMD_FAILED		-8
#define ERROR_CALLBACK_FAILED			-9
static int load_fw(struct device *dev, const char *fn, bool force)
{
	struct xmusb350_charger *chip = dev_get_drvdata(dev);
	const struct firmware *fw = NULL;
	unsigned int pos = 0;
	char chip_id = 0x00;
	int rc = 0;
	char i2c_buf[1] = {0};
	char enable_isp_cmd[7] = {0x57, 0x54, 0x36, 0x36, 0x37, 0x30, 0x46};
	char enable_isp_flash_mode_cmd[3] = {0x10, 0x02, 0x08};
	char chip_erase_cmd[3] = {0x20, 0x00, 0x00};
	char finish_cmd[3] = {0x00, 0x00, 0x00};
	char set_addr_high_byte_cmd[3] = {0x10, 0x01, 0x00};
	char program_cmd[20] = {0x00};
	char high_addr = 0;
	char low_addr = 0;
	char length = 0;
	int i = 0;
	int j = 0;
	char mem_data[64] = {0x00};
	unsigned int pos_file = 0;
	u8 *file_data;
	char flash_addr = 0;

	if (in_interrupt()) {
		file_data = kmalloc(FIRMWARE_FILE_LENGTH, GFP_ATOMIC);
	} else {
		file_data = kmalloc(FIRMWARE_FILE_LENGTH, GFP_KERNEL);
	}
	memset(file_data, 0, FIRMWARE_FILE_LENGTH);
	rc = request_firmware(&fw, fn, dev);
	if (rc) {
		dev_err(dev, "Unable to open firmware %s\n", fn);
		rc = ERROR_REQUESET_FIRMWARE;
		goto release_firmware;
	}

	for (i = 0; i < fw->size; i++) {
		dev_dbg(dev, "fw_data[%d] = %02x\n", i, *(fw->data + i));
	}

	file_version = (u16)(((fw->data[81] & 0x0F) << 8) | fw->data[82]);
	dev_err(chip->dev, "%s: firmware file version is %02x%02x, %04x\n", __func__,
		fw->data[81], fw->data[82], file_version);

	if ((force == false) && (file_version == chip_version)) {
		dev_err(chip->dev, "%s: chip and file version is same, no need update.\n", __func__);
		goto release_firmware;
	}
	xmusb350_addr = 0x2B;
	xmusb350_data = 0x48;
	i2c_buf[0] = xmusb350_data;
	program_cmd[0] = 0x41;

	dev_dbg(chip->dev, "%s: step 1\n", __func__);
	gpio_set_value(chip->xmusb350_rst_gpio, 1);
	mdelay(15);
	gpio_set_value(chip->xmusb350_rst_gpio, 0);
	mdelay(1);

	xmusb350_i2c_sequence_send(chip->client, i2c_buf, 1);
	mdelay(10);

	rc = xmusb350_i2c_master_send(chip->client, enable_isp_cmd, 7);
	if (rc != 7)
		dev_err(chip->dev, "%s: enable_isp_cmd is failed, rc=%d\n", __func__, rc);

	xmusb350_i2c_get_chip_id(chip->client, &chip_id);
	dev_dbg(chip->dev, "%s: rc=%d, chip_id=%02x\n", __func__, rc, chip_id);

	if (chip_id == 0x70) {
		rc = xmusb350_i2c_master_send(chip->client, enable_isp_flash_mode_cmd, 3);
		dev_err(chip->dev, "%s: enable_isp_flash_mode_cmd, rc=%d\n", __func__, rc);

		rc = xmusb350_i2c_master_send(chip->client, chip_erase_cmd, 3);
		if (rc != 3) {
			dev_err(chip->dev, "%s: chip_erase_cmd is failed, rc=%d\n", __func__, rc);
			rc = ERROR_ERASE_FAILED;
			goto update_failed;
		}
		mdelay(20);

		rc = xmusb350_i2c_master_send(chip->client, finish_cmd, 3);
		if (rc != 3) {
			dev_err(chip->dev, "%s: finish_cmd is failed, rc=%d\n", __func__, rc);
			rc = ERROR_FINISH_CMD_FAILED;
			goto update_failed;
		}

		while (pos < (fw->size - 1)) {
			high_addr = *(fw->data + pos + 1);
			low_addr = *(fw->data + pos + 2);
			length = *(fw->data + pos) + 5;
			dev_dbg(chip->dev, "%s: high_addr = %02x, low_addr = %02x, length=%d\n",
					__func__, high_addr, low_addr, length);

			if (AnalyseHEX((fw->data + pos), length) == 0) {
				dev_dbg(chip->dev, "%s: crc is pass.\n", __func__);

				set_addr_high_byte_cmd[2] = high_addr;
				rc = xmusb350_i2c_master_send(chip->client, set_addr_high_byte_cmd, 3);
				if (rc != 3) {
					dev_err(chip->dev, "%s: set_addr_high_byte_cmd is failed, rc=%d\n", __func__, rc);
					rc = ERROR_HIGHADD_CMD_FAILED;
					goto update_failed;
				}

				program_cmd[1] = low_addr;
				memcpy(program_cmd + 2, fw->data + pos + 4, *(fw->data + pos));
				memcpy(file_data + pos_file, fw->data + pos + 4, *(fw->data + pos));
				pos_file += 16;
				rc = xmusb350_i2c_master_send(chip->client, program_cmd, *(fw->data + pos) + 2);
				if (rc != *(fw->data + pos) + 2) {
					dev_err(chip->dev, "%s: program_cmd is failed, rc=%d\n", __func__, rc);
					rc = ERROR_PROGRAM_CMD_FAILED;
					goto update_failed;
				}

				rc = xmusb350_i2c_master_send(chip->client, finish_cmd, 3);
				if (rc != 3) {
					dev_err(chip->dev, "%s: finish_cmd is failed, rc=%d\n", __func__, rc);
					rc = ERROR_FINISH_CMD_FAILED;
					goto update_failed;
				}
			} else {
				dev_err(chip->dev, "%s: crc is error.\n", __func__);
				rc = ERROR_FILE_CRC_FAILED;
				goto update_failed;
			}
			pos = pos + length;
			dev_err(chip->dev, "%s: pos=%d\n", __func__, pos);
		}

		for (i = 0; i < 16; i++) {
			set_addr_high_byte_cmd[2] = i;
			rc = xmusb350_i2c_master_send(chip->client, set_addr_high_byte_cmd, 3);
			if (rc != 3) {
				dev_err(chip->dev, "%s: set_addr_high_byte_cmd is failed, rc=%d\n", __func__, rc);
				rc = ERROR_HIGHADD_CMD_FAILED;
				goto update_failed;
			}

			flash_addr = 0x00;
			xmusb350_i2c_read_cmd(chip->client, flash_addr, mem_data);
			for (j = 0; j < 64; j++) {
				dev_dbg(chip->dev, "%s: mem_data[%d]=%02x\n", __func__, j + flash_addr + i*256, mem_data[j]);
				dev_dbg(chip->dev, "%s: file_data[%d]=%02x\n", __func__, j + flash_addr + i*256, file_data[j + flash_addr + i*256]);
				if (file_data[j + flash_addr + i*256] != mem_data[j]) {
					dev_err(chip->dev, "%s: flash data is wrong.\n", __func__);
					rc = ERROR_CALLBACK_FAILED;
					goto update_failed;
				}
			}

			flash_addr = 0x40;
			xmusb350_i2c_read_cmd(chip->client, flash_addr, mem_data);
			for (j = 0; j < 64; j++) {
				dev_dbg(chip->dev, "%s: mem_data[%d]=%02x\n", __func__, j + flash_addr + i*256, mem_data[j]);
				dev_dbg(chip->dev, "%s: file_data[%d]=%02x\n", __func__, j + flash_addr + i*256, file_data[j + flash_addr + i*256]);
				if (file_data[j + flash_addr + i*256] != mem_data[j]) {
					dev_err(chip->dev, "%s: flash data is wrong.\n", __func__);
					rc = ERROR_CALLBACK_FAILED;
					goto update_failed;
				}
			}

			flash_addr = 0x80;
			xmusb350_i2c_read_cmd(chip->client, flash_addr, mem_data);
			for (j = 0; j < 64; j++) {
				dev_dbg(chip->dev, "%s: mem_data[%d]=%02x\n", __func__, j + flash_addr + i*256, mem_data[j]);
				dev_dbg(chip->dev, "%s: file_data[%d]=%02x\n", __func__, j + flash_addr + i*256, file_data[j + flash_addr + i*256]);
				if (file_data[j + flash_addr + i*256] != mem_data[j]) {
					dev_err(chip->dev, "%s: flash data is wrong.\n", __func__);
					rc = ERROR_CALLBACK_FAILED;
					goto update_failed;
				}
			}

			flash_addr = 0xC0;
			xmusb350_i2c_read_cmd(chip->client, flash_addr, mem_data);
			for (j = 0; j < 64; j++) {
				dev_dbg(chip->dev, "%s: mem_data[%d]=%02x\n", __func__, j + flash_addr + i*256, mem_data[j]);
				dev_dbg(chip->dev, "%s: file_data[%d]=%02x\n", __func__, j + flash_addr + i*256, file_data[j + flash_addr + i*256]);
				if (file_data[j + flash_addr + i*256] != mem_data[j]) {
					dev_err(chip->dev, "%s: flash data is wrong.\n", __func__);
					rc = ERROR_CALLBACK_FAILED;
					goto update_failed;
				}
			}
		}

		rc = UPDATE_SUCCESS;
	} else {
		dev_err(chip->dev, "%s: chip id is not right, and end update.\n", __func__);
		rc = ERROR_GET_CHIPID_FAILED;
		goto update_failed;
	}

update_failed:
	xmusb350_hard_reset(chip);
release_firmware:
	release_firmware(fw);
	kfree(file_data);
	return rc;
}

static ssize_t update_fw_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int cmd = 0;
	int error;

	if (sscanf(buf, "%d", &cmd) != 1)
		return -EINVAL;

	error = load_fw(dev, XMUSB350_FW_NAME, true);
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		count = error;
	} else {
		dev_info(dev, "The firmware update succeeded\n");
	}

	return count;
}

static DEVICE_ATTR(isp_reset, S_IWUSR | S_IWGRP,
		NULL,
		isp_reset_store);
static DEVICE_ATTR(isp_i2c_addr, S_IRUGO | S_IWUSR | S_IWGRP,
		isp_i2c_addr_show,
		isp_i2c_addr_store);
static DEVICE_ATTR(isp_i2c_data, S_IRUGO | S_IWUSR | S_IWGRP,
		isp_i2c_data_show,
		isp_i2c_data_store);
static DEVICE_ATTR(isp_i2c_addr_real, S_IRUGO | S_IWUSR | S_IWGRP,
		isp_i2c_addr_real_show,
		isp_i2c_addr_real_store);
static DEVICE_ATTR(enter_isp_and_get_chipid, S_IRUGO,
		enter_isp_and_get_chipid_show,
		NULL);
static DEVICE_ATTR(isp_pinctrl, S_IRUGO,
		isp_pinctrl_show,
		NULL);
static DEVICE_ATTR(update_fw, S_IWUSR,
		NULL,
		update_fw_store);

static struct attribute *xmusb350_attributes[] = {
	&dev_attr_isp_reset.attr,
	&dev_attr_isp_i2c_addr.attr,
	&dev_attr_isp_i2c_data.attr,
	&dev_attr_isp_i2c_addr_real.attr,
	&dev_attr_enter_isp_and_get_chipid.attr,
	&dev_attr_isp_pinctrl.attr,
	&dev_attr_update_fw.attr,
	NULL,
};

static const struct attribute_group xmusb350_attr_group = {
	.attrs = xmusb350_attributes,
};

#define CHIP_RESET_DELAY		10000	/* 10 sec */
static void xmusb350_hard_reset_work(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger,
						chip_reset_xmusb350.work);
	int rc;

	xmusb350_hard_reset(chip);

	rc = xmusb350_get_version(chip, &chip_version);
	if (rc)
		chip_version = 0x0000;
	xm35_err("%s: chip_version = %04x\n", __func__, chip_version);

}

#define CHIP_UPDATE_DELAY		15000	/* 15 sec */
static void xmusb350_chip_update_work(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger,
						chip_update_work.work);
	int rc;
	u8 buf[4];
	int error = 0;
	union power_supply_propval val = {0,};

	if (!chip->bms_psy)
		chip->bms_psy = power_supply_get_by_name("bms");

	if (chip->bms_psy) {
		rc = power_supply_get_property(chip->bms_psy,
			POWER_SUPPLY_PROP_CAPACITY, &val);
		xm35_err("battery capacity is %d, rc=%d\n", val.intval, rc);
		if (!rc) {
			if (val.intval > 10) {
				error = load_fw(chip->dev, XMUSB350_FW_NAME_INTER, false);
				if (error != UPDATE_SUCCESS) {
					dev_info(chip->dev, "The firmware update failed, error=%d.\n", error);
					if ((error == ERROR_REQUESET_FIRMWARE) || (error == ERROR_CHECK_FIREWARE_FORMAT))
						error = load_fw(chip->dev, XMUSB350_FW_NAME, false);

					if (error == UPDATE_SUCCESS)
						dev_info(chip->dev, "The firmware update succeeded.\n");
					else
						dev_info(chip->dev, "The firmware update failed, error=%d.\n", error);
				} else {
					dev_info(chip->dev, "The firmware update succeeded.\n");
				}

				rc = xmusb350_get_vendor_id(chip, buf);
				if ((buf[0] == 0x58) && (buf[1] == 0x4d) && (buf[2] == 0x33) && (buf[3] == 0x35)) {
					dev_err(chip->dev, "The firmware is good.\n");
				} else {
					dev_err(chip->dev, "The firmware is bad.\n");
				}
			}
		}
	}
}

static int xmusb350_parse_dt(struct xmusb350_charger *chip)
{
	struct xmusb350_desc *desc = NULL;
	struct device_node *np = chip->dev->of_node;

	if (!np) {
		xm35_err("device tree info missing\n");
		return -EINVAL;
	}

	chip->xmusb350_rst_gpio = of_get_named_gpio(np,
					"xm,xmusb350_rst_gpio", 0);
	if ((!gpio_is_valid(chip->xmusb350_rst_gpio))) {
		dev_err(chip->dev, "%s: no xmusb350_rst_gpio\n", __func__);
		return -EINVAL;
	}
	xm35_dbg("xmusb350_rst_gpio: %d\n", chip->xmusb350_rst_gpio);

	chip->xmusb350_sda_gpio = of_get_named_gpio(np,
					"xm,xmusb350_sda_gpio", 0);
	if ((!gpio_is_valid(chip->xmusb350_sda_gpio))) {
		dev_err(chip->dev, "%s: no xmusb350_sda_gpio\n", __func__);
	}
	xm35_dbg("xmusb350_sda_gpio: %d\n", chip->xmusb350_sda_gpio);

	chip->xmusb350_scl_gpio = of_get_named_gpio(np,
					"xm,xmusb350_scl_gpio", 0);
	if ((!gpio_is_valid(chip->xmusb350_scl_gpio))) {
		dev_err(chip->dev, "%s: no xmusb350_scl_gpio\n", __func__);
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

	chip->desc = &xmusb350_default_desc;

	desc = devm_kzalloc(chip->dev, sizeof(struct xmusb350_desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	memcpy(desc, &xmusb350_default_desc, sizeof(struct xmusb350_desc));

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
	u8 buf[4];
	struct xmusb350_charger *chip;
	struct power_supply_config charger_identify_psy_cfg = {};

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		xm35_err("Couldn't allocate memory\n");
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
	chip->hvdcp_timer = 0;
	chip->ocp_timer = 0;

	i2c_set_clientdata(client, chip);

	chip->charger_psy = power_supply_get_by_name("charger");
	if (!chip->charger_psy) {
		xm35_err("%s: get charger power supply failed\n", __func__);
		rc = -EPROBE_DEFER;
		return rc;
	}

	chip->main_psy = power_supply_get_by_name("main");
	if (!chip->main_psy) {
		xm35_err("%s: get main power supply failed\n", __func__);
		rc = -EPROBE_DEFER;
		return rc;
	}

	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		xm35_err("%s: get usb power supply failed\n", __func__);
		rc = -EPROBE_DEFER;
		return rc;
	}

	chip->bms_psy = power_supply_get_by_name("bms");
	if (!chip->bms_psy) {
		xm35_err("%s: get bms power supply failed\n", __func__);
		rc = -EPROBE_DEFER;
		return rc;
	}

	xmusb350_set_usbsw_state(chip, USBSW_CHG);
	rc = xmusb350_parse_dt(chip);
	if (rc) {
		xm35_err("Couldn't parse DT nodes rc=%d\n", rc);
		goto err_mutex_init;
	}

	rc = xmusb350_pinctrl_init(chip);
	if (!rc && chip->xmusb350_pinctrl) {
		rc = pinctrl_select_state(chip->xmusb350_pinctrl,
					chip->pinctrl_state_normal);
		if (rc < 0)
			dev_err(&client->dev,
				"%s: Failed to select active pinstate %d\n",
				__func__, rc);
	}

	rc = devm_gpio_request(&client->dev,
				chip->xmusb350_rst_gpio, "xmusb350 reset gpio");
	if (rc) {
		xm35_err("request xmusb350 reset gpio failed, rc=%d\n",
				rc);
		goto err_mutex_init;
	}
	gpio_direction_output(chip->xmusb350_rst_gpio, 1);
	gpio_set_value(chip->xmusb350_rst_gpio, 0);

	INIT_DELAYED_WORK(&chip->chip_update_work, xmusb350_chip_update_work);
	INIT_DELAYED_WORK(&chip->chip_reset_xmusb350, xmusb350_hard_reset_work);
	INIT_DELAYED_WORK(&chip->hvdcp_det_retry_work, xmusb350_hvdcp_det_retry_work);
	schedule_delayed_work(&chip->chip_update_work,
		msecs_to_jiffies(CHIP_UPDATE_DELAY));
	schedule_delayed_work(&chip->chip_reset_xmusb350,
		msecs_to_jiffies(CHIP_RESET_DELAY));

	rc = devm_gpio_request(&client->dev,
				chip->connect_therm_gpio, "connect_therm_gpio");
	if (rc) {
		dev_err(&client->dev,
		"%s: unable to request smb suspend gpio [%d]\n",
			__func__,
			chip->connect_therm_gpio);
		goto err_mutex_init;
	}
	rc = gpio_direction_output(chip->connect_therm_gpio, 0);
	if (rc) {
		dev_err(&client->dev,
			"%s: unable to set direction for smb suspend gpio [%d]\n",
				__func__,
				chip->connect_therm_gpio);
	}

	if (chip->usb_psy != NULL)
		chip->mt_chg = power_supply_get_drvdata(chip->usb_psy);

	rc = xmusb350_get_vendor_id(chip, buf);
	if (rc != 2) {
		xm35_err("xmusb350 connect failed, rc=%d\n", rc);
	}

	chip->charger_identify_psy_d.name = "Charger_Identify";
	chip->charger_identify_psy_d.type = POWER_SUPPLY_TYPE_CHARGER_IDENTIFY;
	chip->charger_identify_psy_d.get_property = xmusb350_get_property;
	chip->charger_identify_psy_d.set_property = xmusb350_set_property;
	chip->charger_identify_psy_d.properties = xmusb350_properties;
	chip->charger_identify_psy_d.property_is_writeable
				= xmusb350_is_writeable;
	chip->charger_identify_psy_d.num_properties
				= ARRAY_SIZE(xmusb350_properties);

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

	chip->chg_dev = charger_device_register(chip->desc->chg_dev_name,
		&client->dev, chip, &xmusb350_chg_ops, &chip->chg_props);
	if (IS_ERR_OR_NULL(chip->chg_dev))
		goto err_charger_identify_psy;

	chip->chg_dev_p = get_charger_by_name("primary_chg");

	INIT_DELAYED_WORK(&chip->charger_type_det_work, xmusb350_charger_type_det_work);

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
	}
	device_init_wakeup(chip->dev, 1);

	chip->chg_consumer = charger_manager_get_by_name(chip->dev,
			"charger_port1");
	if (!chip->chg_consumer)
			xm35_err("%s: get charger consumer device failed\n",
				__func__);

	rc = sysfs_create_group(&chip->dev->kobj, &xmusb350_attr_group);
	if (rc) {
		xm35_err("Failed to register sysfs, err:%d\n", rc);
		goto xmusb350_create_group_err;
	}

	chip->vbus_disable = false;
	INIT_DELAYED_WORK(&chip->conn_therm_work, smblib_conn_therm_work);

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
	cancel_delayed_work_sync(&chip->chip_reset_xmusb350);
	cancel_delayed_work_sync(&chip->chip_update_work);
	cancel_delayed_work_sync(&chip->hvdcp_det_retry_work);

	return 0;
}

static int xmusb350_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct xmusb350_charger *chip = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&chip->conn_therm_work);

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
	union power_supply_propval val;
	int usb_present = 0;

	power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &val);
	usb_present = val.intval;
	if (usb_present)
		schedule_delayed_work(&chip->conn_therm_work,
				msecs_to_jiffies(THERM_REG_RECHECK_DELAY_5S));

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
	cancel_delayed_work_sync(&chip->chip_reset_xmusb350);
	cancel_delayed_work_sync(&chip->chip_update_work);
	cancel_delayed_work_sync(&chip->hvdcp_det_retry_work);
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

MODULE_AUTHOR("bsp@xiaomi.com");
MODULE_DESCRIPTION("xmusb350 Charger");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("i2c:xmusb350-charger");
