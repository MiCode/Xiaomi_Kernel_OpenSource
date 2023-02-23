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

/* this driver is compatible for XMUSB350 and I350 */

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
#include <linux/pinctrl/consumer.h>
#include <linux/firmware.h>
#include <linux/stat.h>
#include <linux/ctype.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/vmalloc.h>
#include <linux/preempt.h>
#include <linux/time.h>
#include <linux/regmap.h>

#include <mt-plat/v1/charger_type.h>
#include <mt-plat/v1/charger_class.h>
#include <mt-plat/v1/mtk_charger.h>
#include <linux/hwid.h>

/* firmware */
#define I350_VID		0x30353349
#define I350_TEST_MODE_ID	0x30079F00
#define I350_FW_VERSION		0x0F
#define XMUSB350_VID		0x35334D58
#define XMUSB350_FW_VERSION	0x0F
#define FIRMWARE_FILE_LENGTH	15000
#define ERROR_REQUESET_FIRMWARE	-1
#define UPDATE_FW_DELAY		4000
#define UPDATE_FW_LOW_CAPACITY	8
#define I350_FW_NAME		"I350_SNK_FIRMWARE.bin"

/* registers */
#define XMUSB350_REG_RERUN_APSD		0x01
#define XMUSB350_REG_QC_MS		0x02
#define XMUSB350_REG_INTB_EN		0x03
#define XMUSB350_REG_SOFT_RESET		0x04
#define XMUSB350_REG_HVDCP_EN		0x05
#define XMUSB350_REG_BC12_EN		0x06
#define XMUSB350_REG_SLEEP_EN		0x07
#define XMUSB350_REG_CHGT_ERROR		0x11
#define XMUSB350_REG_VIN		0x12
#define XMUSB350_REG_VID		0x13
#define XMUSB350_REG_VERSION		0x14
#define XMUSB350_REG_TEST_MODE_ID	0x41
#define XMUSB350_REG_QC3_PULSE		0x73
#define XMUSB350_REG_QC35_PULSE		0x83

#define QC35_DPDM			0x01
#define QC30_DPDM			0x00

#define QC3_CLASSAB_PULSE_COUNT		38
#define QC3_CLASS_HIGH_THRE		12250
#define QC3_CLASS_LOW_THRE		10000
#define VBUS_UVLO_THRE			4000

#define EARLY_UPDATE_FW_TIME		4
#define EARLY_BOOT_TIME			20
#define MAX_CHECK_USB_READY_COUNT	1000

#define CP_VAC_OVP_6500MV		6500
#define CP_VAC_OVP_22000MV		22000

#define CONN_THERM_WORK 0

enum xmusb350_error_state {
	NO_ERROR,
	QC3_ERROR,
	TA_ERROR,
	TA_CAP_ERROR,
	QC35_COMPLETE,
};

enum thermal_status_levels {
	TEMP_SHUT_DOWN = 0,
	TEMP_SHUT_DOWN_SMB,
	TEMP_ALERT_LEVEL,
	TEMP_ABOVE_RANGE,
	TEMP_WITHIN_RANGE,
	TEMP_BELOW_RANGE,
};

struct xmusb350_charger {
	struct i2c_client		*client;
	struct device			*dev;
	struct regmap			*regmap;
	struct wakeup_source 		*irq_handle_wakelock;
	struct wakeup_source 		*check_hvdcp3_wakelock;
	struct wakeup_source 		*check_usb_ready_wakelock;
	struct wakeup_source 		*update_fw_wakelock;
	struct wakeup_source 		*vbus_wave_wa_wakelock;
	struct wakeup_source 		*slow_pd_wa_wakelock;

	struct charger_properties	chg_props;
	struct charger_device		*chg_dev;
	struct charger_device		*master_cp;
	struct charger_device		*slave_cp;
	struct charger_consumer		*chg_consumer;

	enum thermal_status_levels	thermal_status;

	char				model_name[20];
	u8				hw_version;
	u8				fw_version;
	u32				vid;

	bool				attach;
	bool				fw_update;
	bool				chip_ok;
	bool				otg_enable;
	bool				vbus_disable;
	bool				weak_qc3;

	int				recheck_count;
	int				first_type;
	int				psy_type;
	int				mtk_type;
	int				hvdcp3_type;
	int				xmusb_type;
	int				error_status;
	int				pulse_cnt;
	int				entry_time;
	int				usb_switch;
	int				connector_temp;
	unsigned int			connect_therm_gpio;

	struct delayed_work		irq_handle_work;
	struct delayed_work		check_hvdcp3_work;
	struct delayed_work		update_fw_work;
	struct delayed_work		check_usb_ready_work;
	struct delayed_work		recheck_type_work;
#ifdef CONN_THERM_WORK
	struct delayed_work		conn_therm_work;
#endif
	struct delayed_work		slow_pd_wa_work;

	struct power_supply		*usb_psy;
	struct power_supply		*charger_psy;
	struct power_supply		*xmusb350_psy;
	struct power_supply		*bms_psy;

	struct pinctrl			*xmusb350_pinctrl;
	struct pinctrl_state		*pinctrl_state_normal;
	struct pinctrl_state		*pinctrl_state_isp;
	int				xmusb350_sda_gpio;
	int				xmusb350_scl_gpio;
	int				xmusb350_rst_gpio;
	int				xmusb350_irq_gpio;
	int				irq;

	struct mutex			isp_sequence_lock;

	struct timespec			update_fw_time;
};

struct charger_type_desc {
	int psy_type;
	int hvdcp3_type;
	int mtk_type;
	int usb_switch;
};

enum product_name {
	UNKNOW,
	RUBY,
	RUBYPRO,
	RUBYPLUS,
};

static int product_name = UNKNOW;

extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);

static int xmusb350_psy_chg_type_changed(struct xmusb350_charger *chip);
static void xmusb350_set_usbsw_state(struct xmusb350_charger *chip, int state);
static int log_level = 1;
static uint32_t *file_data;
static uint32_t *file_date_w;
static uint32_t file_size;

static const char * const xmusb350_error_status_name[] = {
	[NO_ERROR]		= "NO_ERROR",
	[QC3_ERROR]		= "QC3_ERROR",
	[TA_ERROR]		= "TA_ERROR",
	[TA_CAP_ERROR]		= "TA_CAP_ERROR",
	[QC35_COMPLETE]		= "QC35_COMPLETE",
};

static const char * const xmusb350_pulse_type[] = {
	[QC3_DM_PULSE]		= "QC3_DM_PULSE",
	[QC3_DP_PULSE]		= "QC3_DP_PULSE",
	[QC35_DM_PULSE]		= "QC35_DM_PULSE",
	[QC35_DP_PULSE]		= "QC35_DP_PULSE",
};

static const char * const xmusb350_qc_mode[] = {
	[QC_MODE_QC2_5]		= "QC_MODE_QC2_5",
	[QC_MODE_QC2_9]		= "QC_MODE_QC2_9",
	[QC_MODE_QC2_12]	= "QC_MODE_QC2_12",
	[QC_MODE_QC3_5]		= "QC_MODE_QC3_5",
	[QC_MODE_QC35_5]	= "QC_MODE_QC35_5",
};

static const char * const xmusb350_chg_type_name[] = {
	[XMUSB350_TYPE_OCP]		= "OCP",
	[XMUSB350_TYPE_FLOAT]		= "FLOAT",
	[XMUSB350_TYPE_SDP]		= "SDP",
	[XMUSB350_TYPE_CDP]		= "CDP",
	[XMUSB350_TYPE_DCP]		= "DCP",
	[XMUSB350_TYPE_HVDCP_2]		= "HVDCP_2",
	[XMUSB350_TYPE_HVDCP_3]		= "HVDCP_3",
	[XMUSB350_TYPE_HVDCP_35_18]	= "HVDCP_35_18",
	[XMUSB350_TYPE_HVDCP_35_27]	= "HVDCP_35_27",
	[XMUSB350_TYPE_HVDCP_3_18]	= "HVDCP_3_18",
	[XMUSB350_TYPE_HVDCP_3_27]	= "HVDCP_3_27",
	[XMUSB350_TYPE_PD]		= "PD",
	[XMUSB350_TYPE_PD_DR]		= "PD_DR",
	[XMUSB350_TYPE_HVDCP]		= "HVDCP",
	[XMUSB350_TYPE_UNKNOW]		= "UNKNOW",
};

static struct charger_type_desc charger_type_table[] = {
	[XMUSB350_TYPE_OCP]		= {POWER_SUPPLY_TYPE_USB_DCP,		HVDCP3_NONE,	STANDARD_CHARGER,	USBSW_XMUSB350},
	[XMUSB350_TYPE_FLOAT]		= {POWER_SUPPLY_TYPE_USB_FLOAT,		HVDCP3_NONE,	NONSTANDARD_CHARGER,	USBSW_XMUSB350},
	[XMUSB350_TYPE_SDP]		= {POWER_SUPPLY_TYPE_USB,		HVDCP3_NONE,	STANDARD_HOST,		USBSW_SOC},
	[XMUSB350_TYPE_CDP]		= {POWER_SUPPLY_TYPE_USB_CDP,		HVDCP3_NONE,	CHARGING_HOST,		USBSW_SOC},
	[XMUSB350_TYPE_DCP]		= {POWER_SUPPLY_TYPE_USB_DCP,		HVDCP3_NONE,	STANDARD_CHARGER,	USBSW_XMUSB350},
	[XMUSB350_TYPE_HVDCP_2]		= {POWER_SUPPLY_TYPE_USB_HVDCP,		HVDCP3_NONE,	STANDARD_CHARGER,	USBSW_XMUSB350},
	[XMUSB350_TYPE_HVDCP_3]		= {POWER_SUPPLY_TYPE_USB_HVDCP_3,	HVDCP3_NONE,	STANDARD_CHARGER,	USBSW_XMUSB350},
	[XMUSB350_TYPE_HVDCP_35_18]	= {POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS,	HVDCP35_18,	STANDARD_CHARGER,	USBSW_XMUSB350},
	[XMUSB350_TYPE_HVDCP_35_27]	= {POWER_SUPPLY_TYPE_USB_HVDCP_3_PLUS,	HVDCP35_27,	STANDARD_CHARGER,	USBSW_XMUSB350},
	[XMUSB350_TYPE_HVDCP_3_18]	= {POWER_SUPPLY_TYPE_USB_HVDCP_3,	HVDCP3_18,	STANDARD_CHARGER,	USBSW_XMUSB350},
	[XMUSB350_TYPE_HVDCP_3_27]	= {POWER_SUPPLY_TYPE_USB_HVDCP_3,	HVDCP3_27,	STANDARD_CHARGER,	USBSW_XMUSB350},
	[XMUSB350_TYPE_PD]		= {POWER_SUPPLY_TYPE_USB_PD,		HVDCP3_NONE,	STANDARD_CHARGER,	USBSW_SOC},
	[XMUSB350_TYPE_PD_DR]		= {POWER_SUPPLY_TYPE_USB_PD,		HVDCP3_NONE,	CHARGING_HOST,		USBSW_SOC},
	[XMUSB350_TYPE_HVDCP]		= {POWER_SUPPLY_TYPE_USB_HVDCP,		HVDCP3_NONE,	STANDARD_CHARGER,	USBSW_XMUSB350},
	[XMUSB350_TYPE_UNKNOW]		= {POWER_SUPPLY_TYPE_UNKNOWN,		HVDCP3_NONE,	CHARGER_UNKNOWN,	USBSW_XMUSB350},
};

static struct regmap_config xmusb350_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = 0x83,
};

#define xm35_err(fmt, ...)							\
do {										\
	if (log_level >= 0)							\
		printk(KERN_ERR "[XMCHG_XMUSB350] " fmt, ##__VA_ARGS__);	\
} while (0)

#define xm35_info(fmt, ...)							\
do {										\
	if (log_level >= 1)							\
		printk(KERN_ERR "[XMCHG_XMUSB350] " fmt, ##__VA_ARGS__);	\
} while (0)

#define xm35_dbg(fmt, ...)							\
do {										\
	if (log_level >= 2)							\
		printk(KERN_ERR "[XMCHG_XMUSB350] " fmt, ##__VA_ARGS__);	\
} while (0)

static int xmusb350_select_qc_mode(struct xmusb350_charger *chip, int mode)
{
	int ret = 0;
	unsigned int data = mode;

	if (chip->fw_update)
		return ret;

	if (data < QC_MODE_QC2_5 || data > QC_MODE_QC35_5) {
		xm35_err("not support QC_MODE\n");
		return -1;
	}

	xm35_info("select qc mode = %s\n", xmusb350_qc_mode[mode]);

	ret = regmap_write(chip->regmap, XMUSB350_REG_QC_MS, data);
	if (ret) {
		xm35_err("failed to set QC_MODE\n");
		return ret;
	}

	return ret;
}

static int xmusb350_rerun_apsd(struct xmusb350_charger *chip)
{
	int ret = 0;

	xm35_info("rerun APSD\n");

	if (chip->fw_update)
		return ret;

	ret = regmap_write(chip->regmap, XMUSB350_REG_RERUN_APSD, 0x01);
	if (ret) {
		xm35_err("failed to set rerun APSD\n");
		return ret;
	}

	return ret;
}

static int xmusb350_soft_reset(struct xmusb350_charger *chip)
{
	int ret = 0;

	if (chip->fw_update)
		return ret;

	ret = regmap_write(chip->regmap, XMUSB350_REG_SOFT_RESET, 0x00);
	if (ret) {
		xm35_err("failed to soft reset\n");
		return ret;
	}

	return ret;
}

static int xmusb350_enable_hvdcp(struct xmusb350_charger *chip, bool enable)
{
	int ret = 0;
	unsigned int data = 0;

	if (chip->fw_update)
		return ret;

	if (enable) {
		data = 0x01;
		msleep(500);
	}

	if (!chip->attach || chip->psy_type == POWER_SUPPLY_TYPE_USB_PD)
		return ret;

	ret = regmap_write(chip->regmap, XMUSB350_REG_HVDCP_EN, data);
	if (ret) {
		xm35_err("failed to set HVDCP_EN\n");
		return ret;
	}

	return ret;
}

static int xmusb350_get_charger_type(struct xmusb350_charger *chip, u8 *data)
{
	int ret = 0;

	if (chip->fw_update)
		return -1;

	ret = regmap_raw_read(chip->regmap, XMUSB350_REG_CHGT_ERROR, data, 2);
	if (ret) {
		xm35_err("failed to get charger type and error status\n");
		data[0] = XMUSB350_TYPE_UNKNOW;
		data[1] = NO_ERROR;
	}

	if (data[0] < XMUSB350_TYPE_OCP || (data[0] > XMUSB350_TYPE_PD_DR && data[0] != XMUSB350_TYPE_HVDCP && data[0] != XMUSB350_TYPE_UNKNOW)) {
		xm35_err("not support type = 0x%02x\n", data[0]);
		return -1;
	}

	xm35_info("get charger type = %s\n", xmusb350_chg_type_name[data[0]]);

	return ret;
}

static int xmusb350_qc3_dpdm_pulse(struct xmusb350_charger *chip, int pulse_type, int count)
{
	int ret = 0;
	u8 reg = 0, data[2] = {0, 0};

	if (chip->fw_update)
		return ret;

	switch (pulse_type) {
	case QC3_DM_PULSE:
		reg = XMUSB350_REG_QC3_PULSE;
		data[0] &= 0x7F;
		break;
	case QC3_DP_PULSE:
		reg = XMUSB350_REG_QC3_PULSE;
		data[0] |= 0x80;
		break;
	case QC35_DM_PULSE:
		reg = XMUSB350_REG_QC35_PULSE;
		data[0] &= 0x7F;
		break;
	case QC35_DP_PULSE:
		reg = XMUSB350_REG_QC35_PULSE;
		data[0] |= 0x80;
		break;
	default:
		xm35_err("pulse_type not support\n");
		return -1;
	}

	xm35_info("dpdm pulse, type = %s, count = %d\n", xmusb350_pulse_type[pulse_type], count);
	data[0] |= (((count & 0x7F00) >> 8) & 0x7F);
	data[1] = count & 0xFF;

	ret = regmap_raw_write(chip->regmap, reg, data, 2);
	if (ret)
		xm35_err("failed to send dpdm pulse command\n");

	return ret;
}

static int xmusb350_get_id(struct xmusb350_charger *chip)
{
	int ret = 0;
	u8 data[4] = {0, 0, 0, 0};

	if (chip->fw_update)
		return ret;

	ret = regmap_raw_read(chip->regmap, XMUSB350_REG_VID, data, 4);
	if (ret) {
		xm35_err("failed to get VID, try to read test mode ID\n");
		ret = regmap_raw_read(chip->regmap, XMUSB350_REG_TEST_MODE_ID, data, 4);
		if (ret) {
			xm35_err("failed to get VID or test mode ID\n");
			return ret;
		}
	}

	chip->vid = (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
	if (chip->vid == I350_VID || chip->vid == I350_TEST_MODE_ID)
		strcpy(chip->model_name, "I350");
	else if (chip->vid == XMUSB350_VID)
		strcpy(chip->model_name, "XMUSB350");

	ret = regmap_raw_read(chip->regmap, XMUSB350_REG_VERSION, data, 2);
	if (ret) {
		xm35_err("failed to get HW_VERSION and FW_VERSION\n");
	} else {
		chip->hw_version = data[0];
		chip->fw_version = data[1];
	}

	xm35_info("VID = 0x%08x, HW_VERSION = 0x%02x, FW_VERSION = 0x%02x\n", chip->vid, chip->hw_version, chip->fw_version);

	if (!(chip->update_fw_wakelock->active) && ((chip->vid == I350_VID && chip->fw_version != I350_FW_VERSION) ||
		chip->vid == I350_TEST_MODE_ID || (chip->vid == 0 && chip->fw_version == 0))) {
		__pm_stay_awake(chip->update_fw_wakelock);
		schedule_delayed_work(&chip->update_fw_work, msecs_to_jiffies(UPDATE_FW_DELAY));
	}

	return ret;
}

void xmusb350_hard_reset(struct xmusb350_charger *chip)
{
	i2c_lock_bus(chip->client->adapter, I2C_LOCK_SEGMENT);
	gpio_set_value(chip->xmusb350_rst_gpio, 1);
	mdelay(10);
	gpio_set_value(chip->xmusb350_rst_gpio, 0);
	mdelay(60);
	i2c_unlock_bus(chip->client->adapter, I2C_LOCK_SEGMENT);

	xm35_info("hard reset done\n");
}

static int xmusb350_psy_chg_type_changed(struct xmusb350_charger *chip)
{
	int ret = 0;
	union power_supply_propval prop;

	prop.intval = chip->psy_type;
	ret = power_supply_set_property(chip->usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &prop);
	if (ret < 0)
		xm35_err("failed to set psy_type");

	prop.intval = chip->mtk_type;
	ret = power_supply_set_property(chip->charger_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &prop);
	if (ret < 0)
		xm35_err("failed to set mtk_type");

	return ret;
}

static void xmusb350_recheck_type(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger, recheck_type_work.work);
	u8 data[2];

	if (chip->otg_enable || !chip->attach || chip->fw_update)
		goto done;

	chip->recheck_count++;

	if (chip->recheck_count < 10 && (chip->xmusb_type == XMUSB350_TYPE_UNKNOW || chip->xmusb_type == XMUSB350_TYPE_FLOAT)) {
		xm35_info("recheck charger type, count = %d\n", chip->recheck_count);
		if (chip->xmusb_type == XMUSB350_TYPE_UNKNOW)
			xmusb350_get_charger_type(chip, data);
		xmusb350_rerun_apsd(chip);
		schedule_delayed_work(&chip->recheck_type_work, msecs_to_jiffies(3500));
		return;
	} else if (chip->recheck_count <= 4) {
		schedule_delayed_work(&chip->recheck_type_work, msecs_to_jiffies(3500));
		return;
	}

done:
	__pm_relax(chip->vbus_wave_wa_wakelock);
	return;
}

static void xmusb350_slow_pd_wa(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger, slow_pd_wa_work.work);

	if (chip->xmusb_type == XMUSB350_TYPE_DCP && chip->attach) {
		xm35_info("check slow PD workaround\n");
		xmusb350_rerun_apsd(chip);
	}

	__pm_relax(chip->slow_pd_wa_wakelock);
	return;
}

static void xmusb350_check_usb_ready(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger, check_usb_ready_work.work);
	u8 data[2];
	int i = 0;

	xmusb350_get_charger_type(chip, data);

	for (i = 0; i < MAX_CHECK_USB_READY_COUNT; i++) {
		if (is_usb_rdy(chip->dev))
			break;
		msleep(150);
	}

	if (i == MAX_CHECK_USB_READY_COUNT)
		xm35_err("check usb ready timeout\n");
	else
		xm35_info("check usb ready done\n");

	if (chip->mtk_type == STANDARD_HOST || chip->mtk_type == CHARGING_HOST) {
		//mt_usb_connect();
		power_supply_changed(chip->usb_psy);
	}

	__pm_relax(chip->check_usb_ready_wakelock);

	return;
}

static int xmusb350_vbus_disable(struct xmusb350_charger *chip,	bool disable)
{
	xm35_dbg("%s disable: %d\n", __func__, disable);
	if (disable) {
		//gpio_direction_output(chip->connect_therm_gpio, 1);
		chip->vbus_disable = true;
	} else {
		//gpio_direction_output(chip->connect_therm_gpio, 0);
		chip->vbus_disable = false;
	}
	return 0;
}

#ifdef CONN_THERM_WORK
#define THERM_REG_RECHECK_DELAY_200MS		200	/* 1 sec */
#define THERM_REG_RECHECK_DELAY_1S		1000	/* 1 sec */
#define THERM_REG_RECHECK_DELAY_5S		5000	/* 5 sec */
#define THERM_REG_RECHECK_DELAY_8S		8000	/* 8 sec */
#define THERM_REG_RECHECK_DELAY_10S		10000	/* 10 sec */

#define CONNECTOR_THERM_ABOVE			200	/* 20 Dec */
#define CONNECTOR_THERM_HIG			500	/* 50 Dec */
#define CONNECTOR_THERM_TOO_HIG			750	/* 75 Dec */

#define CHG_DETECT_CONN_THERM_US		10000000	/* 10sec */
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
		if (thermal_status == TEMP_ABOVE_RANGE) {
			enable_charging = false;
			xm35_err("connect temp is too hot, disable vbus\n");
			charger_manager_enable_charging(chip->chg_consumer,
				MAIN_CHARGER, false);
			xmusb350_vbus_disable(chip, true);
		} else {
			xm35_err("connect temp normal recovery vbus\n");
			if (enable_charging == false) {
				charger_manager_enable_charging(chip->chg_consumer,
					MAIN_CHARGER, true);
				enable_charging = true;
			}
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
	if (!usb_present && thermal_status == TEMP_BELOW_RANGE && !chip->otg_enable)
		xm35_dbg("usb is disconnet cancel the connect them work\n");
	else
		schedule_delayed_work(&chip->conn_therm_work,
				msecs_to_jiffies(wdog_timeout));
}
#endif

#define RECHECK_DELAY		5000
#define MAX_RECHECK_COUNT	4
static int xmusb350_update_chgtype(struct xmusb350_charger *chip, int xmusb_type)
{
	struct timespec time;
	int ret = 0;

	xm35_info("old_type = %s, new_type = %s, error_status = %s, OTG = %d\n",
		xmusb350_chg_type_name[chip->xmusb_type], xmusb350_chg_type_name[xmusb_type], xmusb350_error_status_name[chip->error_status], chip->otg_enable);

	if (chip->otg_enable)
		return ret;

	if (xmusb_type == XMUSB350_TYPE_PD)
		xmusb350_enable_hvdcp(chip, false);

	if (chip->xmusb_type == XMUSB350_TYPE_PD_DR && (xmusb_type == XMUSB350_TYPE_SDP || xmusb_type == XMUSB350_TYPE_PD))
		xmusb_type = XMUSB350_TYPE_PD_DR;

	if ((chip->xmusb_type == XMUSB350_TYPE_PD && xmusb_type == XMUSB350_TYPE_SDP) || (chip->xmusb_type == XMUSB350_TYPE_SDP && xmusb_type == XMUSB350_TYPE_PD))
		xmusb_type = XMUSB350_TYPE_PD_DR;

	chip->xmusb_type = xmusb_type;
	chip->psy_type = charger_type_table[xmusb_type].psy_type;
	chip->mtk_type = charger_type_table[xmusb_type].mtk_type;
	chip->hvdcp3_type = charger_type_table[xmusb_type].hvdcp3_type;
	chip->usb_switch = charger_type_table[xmusb_type].usb_switch;

	get_monotonic_boottime(&time);
	if (time.tv_sec <= 15 && chip->psy_type)
		chip->first_type = chip->psy_type;

	if (chip->hvdcp3_type || chip->psy_type == POWER_SUPPLY_TYPE_USB_PD || chip->psy_type == POWER_SUPPLY_TYPE_USB_HVDCP|| chip->psy_type == POWER_SUPPLY_TYPE_USB_HVDCP_3) {
		charger_dev_set_ac_ovp(chip->master_cp, CP_VAC_OVP_22000MV);
		charger_dev_set_ac_ovp(chip->slave_cp, CP_VAC_OVP_22000MV);
	} else {
		charger_dev_set_ac_ovp(chip->master_cp, CP_VAC_OVP_6500MV);
		charger_dev_set_ac_ovp(chip->slave_cp, CP_VAC_OVP_6500MV);
	}

	xmusb350_set_usbsw_state(chip, chip->usb_switch);
	ret = xmusb350_psy_chg_type_changed(chip);

done:

	return ret;
}

static void xmusb350_check_hvdcp3(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger, check_hvdcp3_work.work);
	int xmusb_type = XMUSB350_TYPE_UNKNOW, vbus = 0, ret = 0;
	union power_supply_propval pval = {0,};

	if (chip->psy_type == POWER_SUPPLY_TYPE_USB_PD) {
		xm35_info("PD detected, no need check HVDCP\n");
		goto done;
	}

	ret = xmusb350_select_qc_mode(chip, QC_MODE_QC3_5);
	msleep(200);
	if (ret) {
		xm35_err("select qc3 dpdm mode failed!\n");
		goto done;
	}

	ret = xmusb350_qc3_dpdm_pulse(chip, QC3_DP_PULSE, QC3_CLASSAB_PULSE_COUNT);
	if (ret) {
		xm35_err("failed to send qc3 classab check pulse\n");
		goto done;
	}

	msleep(300);

	ret = power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	vbus = pval.intval;
	xm35_info("QC3 class A/B check, vbus = %d\n", vbus);

	if (vbus <= VBUS_UVLO_THRE) {
		xmusb_type = XMUSB350_TYPE_UNKNOW;
	} else if (vbus <= QC3_CLASS_LOW_THRE) {
		chip->weak_qc3 = true;
		xmusb_type = XMUSB350_TYPE_HVDCP_3_18;
	} else if (vbus <= QC3_CLASS_HIGH_THRE) {
		xmusb_type = XMUSB350_TYPE_HVDCP_3_18;
	} else {
		xmusb_type = XMUSB350_TYPE_HVDCP_3_27;
	}

	ret = xmusb350_select_qc_mode(chip, QC_MODE_QC2_5);
	msleep(50);
	if (ret) {
		xm35_err("select qc2 5v mode failed!\n");
		goto done;
	}

	ret = xmusb350_select_qc_mode(chip, QC_MODE_QC3_5);
	msleep(50);
	if (ret) {
		xm35_err("select qc3 dpdm mode failed!\n");
		goto done;
	}

	ret = power_supply_get_property(chip->usb_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	vbus = pval.intval;
	if (vbus <= VBUS_UVLO_THRE || chip->psy_type == POWER_SUPPLY_TYPE_USB_PD){
		xm35_info("ignore QC3 class A/B type\n");
		goto done;
	}

	xmusb350_update_chgtype(chip, xmusb_type);

done:
	__pm_relax(chip->check_hvdcp3_wakelock);

	return;
}

static void xmusb350_irq_handler(struct work_struct *work)
{
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger, irq_handle_work.work);
	int ret = 0;
	int xmusb_type = XMUSB350_TYPE_UNKNOW;
	u8 data[2];
	struct timespec time;

	ret = xmusb350_get_charger_type(chip, data);
	if (ret) {
		xm35_err("get charger type failed!\n");
		goto done;
	}

	if (chip->otg_enable || !chip->attach)
		goto done;

	get_monotonic_boottime(&time);
	if (chip->update_fw_time.tv_sec) {
		if (data[0] == XMUSB350_TYPE_OCP || data[0] == XMUSB350_TYPE_FLOAT) {
			if (time.tv_sec >= chip->update_fw_time.tv_sec && time.tv_sec <= chip->update_fw_time.tv_sec + EARLY_UPDATE_FW_TIME) {
				chip->update_fw_time.tv_sec = 0;
				goto done;
			}
		}
		chip->update_fw_time.tv_sec = 0;
	}

	if (chip->psy_type == POWER_SUPPLY_TYPE_USB_PD)
		goto done;

	if ((chip->weak_qc3 || chip->check_hvdcp3_wakelock->active) && (data[0] == XMUSB350_TYPE_DCP || data[0] == XMUSB350_TYPE_HVDCP || data[0] == XMUSB350_TYPE_UNKNOW)) {
		xm35_info("weak QC3 or 18W/27W is checking\n");
		goto done;
	}

	xmusb_type = data[0];
	chip->error_status = data[1];

	if (xmusb_type == XMUSB350_TYPE_HVDCP) {
		if (time.tv_sec <= EARLY_BOOT_TIME) {
			xmusb_type = XMUSB350_TYPE_DCP;
			if (!chip->slow_pd_wa_wakelock->active) {
				__pm_stay_awake(chip->slow_pd_wa_wakelock);
				schedule_delayed_work(&chip->slow_pd_wa_work, msecs_to_jiffies((EARLY_BOOT_TIME + 5 - time.tv_sec) * 1000));
			}
			goto update_type;
		}
		charger_dev_set_ac_ovp(chip->master_cp, CP_VAC_OVP_22000MV);
		charger_dev_set_ac_ovp(chip->slave_cp, CP_VAC_OVP_22000MV);
		xmusb350_enable_hvdcp(chip, true);
		if (!chip->attach || chip->psy_type == POWER_SUPPLY_TYPE_USB_PD)
			goto done;
	}

	if (xmusb_type == XMUSB350_TYPE_HVDCP_3) {
		if (!chip->check_hvdcp3_wakelock->active)
			__pm_stay_awake(chip->check_hvdcp3_wakelock);
		schedule_delayed_work(&chip->check_hvdcp3_work, 0);
	}

	goto update_type;

fail:
	xmusb_type = XMUSB350_TYPE_UNKNOW;

update_type:
	ret = xmusb350_update_chgtype(chip, xmusb_type);

done:
	__pm_relax(chip->irq_handle_wakelock);

	return;
}

static void xmusb350_set_usbsw_state(struct xmusb350_charger *chip, int usbsw)
{
	xm35_info("switch DPDM to %s\n", usbsw == USBSW_XMUSB350 ? "XMUSB350" : "SOC");

	if (usbsw == USBSW_XMUSB350)
		Charger_Detect_Init();
	else
		Charger_Detect_Release();

	return;
}

static irqreturn_t xmusb350_interrupt(int irq, void *dev_id)
{
	struct xmusb350_charger *chip = dev_id;
	u8 data[2];

	xm35_info("xmusb350_interrupt\n");

	if (chip->fw_update)
		return IRQ_HANDLED;

	if (chip->irq_handle_wakelock->active) {
		xmusb350_get_charger_type(chip, data);
		return IRQ_HANDLED;
	}

	__pm_stay_awake(chip->irq_handle_wakelock);
	schedule_delayed_work(&chip->irq_handle_work, 0);

	return IRQ_HANDLED;
}

static int xmusb350_enable_chg_type_det(struct xmusb350_charger *chip, bool en)
{
	struct timespec time;
	int ret = 0;
	bool early_boot = false;

	chip->attach = en;
	chip->recheck_count = 0;
	chip->weak_qc3 = false;

	if (en) {
		if (!chip->vid || !chip->hw_version || !chip->fw_version)
			xmusb350_get_id(chip);

		get_monotonic_boottime(&time);
		if (time.tv_sec <= EARLY_BOOT_TIME)
			early_boot = true;

		chip->usb_switch = USBSW_XMUSB350;
		xmusb350_set_usbsw_state(chip, chip->usb_switch);

		if (early_boot) {
			xmusb350_rerun_apsd(chip);
		}

		if (!chip->vbus_wave_wa_wakelock->active) {
			__pm_stay_awake(chip->vbus_wave_wa_wakelock);
			schedule_delayed_work(&chip->recheck_type_work, msecs_to_jiffies(2000));
		}
	} else {
		xmusb350_enable_hvdcp(chip, false);
		cancel_delayed_work_sync(&chip->recheck_type_work);
		__pm_relax(chip->vbus_wave_wa_wakelock);
		ret = xmusb350_update_chgtype(chip, XMUSB350_TYPE_UNKNOW);
	}

	return ret;
}

static int ops_xmusb350_enable_chg_type_det(struct charger_device *chg_dev, bool enable)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);

	return xmusb350_enable_chg_type_det(chip, enable);
}

static int ops_xmusb350_update_chgtype(struct charger_device *chg_dev, int type)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);

	return xmusb350_update_chgtype(chip, type);
}

static int ops_xmusb350_qc3_dpdm_pulse(struct charger_device *chg_dev, int type, int count)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);

	if (chip->check_hvdcp3_wakelock->active) {
		xm35_info("can't qc3_dpdm_pulse, dpdm busy for check QC3 class\n");
		return 0;
	} else {
		return xmusb350_qc3_dpdm_pulse(chip, type, count);
	}
}

static int ops_xmusb350_select_qc_mode(struct charger_device *chg_dev, int type)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);

	if (chip->check_hvdcp3_wakelock->active) {
		xm35_info("can't select_qc_mode, dpdm busy for check QC3 class\n");
		return 0;
	} else {
		return xmusb350_select_qc_mode(chip, type);
	}
}

static int ops_xmusb350_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);

	xm35_info("%s: en = %d\n", __func__, en);

	if (en) {
		chip->usb_switch = USBSW_SOC;
#ifdef CONN_THERM_WORK
		if(product_name == RUBYPLUS)
			schedule_delayed_work(&chip->conn_therm_work,
				msecs_to_jiffies(THERM_REG_RECHECK_DELAY_200MS));
#endif
	} else {
		chip->usb_switch = USBSW_XMUSB350;
#ifdef CONN_THERM_WORK
		if(product_name == RUBYPLUS && chip->thermal_status != TEMP_ABOVE_RANGE)
			cancel_delayed_work_sync(&chip->conn_therm_work);
#endif
	}
	chip->otg_enable = en;
	xmusb350_set_usbsw_state(chip, chip->usb_switch);

	return 0;
}

static int ops_get_first_charger_type(struct charger_device *chg_dev)
{
	struct xmusb350_charger *chip = dev_get_drvdata(&chg_dev->dev);

	return chip->first_type;
}

static struct charger_ops xmusb350_chg_ops = {
	.enable_chg_type_det = ops_xmusb350_enable_chg_type_det,
	.update_chgtype = ops_xmusb350_update_chgtype,
	.qc3_dpdm_pulse = ops_xmusb350_qc3_dpdm_pulse,
	.select_qc_mode = ops_xmusb350_select_qc_mode,
	.enable_otg = ops_xmusb350_enable_otg,
	.get_first_charger_type = ops_get_first_charger_type,
};

static const struct charger_properties xmusb350_chg_props = {
	.alias_name = "xmusb350",
};

static enum power_supply_property xmusb350_properties[] = {
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_CHG_TYPE,
	POWER_SUPPLY_PROP_HVDCP3_TYPE,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_FW_VERSION,
	POWER_SUPPLY_PROP_RECHECK_COUNT,
};

static int xmusb350_get_property(struct power_supply *psy, enum power_supply_property prop, union power_supply_propval *val)
{
	struct xmusb350_charger *chip = power_supply_get_drvdata(psy);
	int ret = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = chip->model_name;
		break;
	case POWER_SUPPLY_PROP_CHG_TYPE:
		val->intval = chip->xmusb_type;
		break;
	case POWER_SUPPLY_PROP_HVDCP3_TYPE:
		val->intval = chip->hvdcp3_type;
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->chip_ok;
		break;
	case POWER_SUPPLY_PROP_FW_VERSION:
		val->intval = chip->fw_version;
		break;
	case POWER_SUPPLY_PROP_RECHECK_COUNT:
		val->intval = chip->recheck_count;
		break;
	default:
		xm35_err("%s: unsupported property %d\n", __func__, prop);
		return -ENODATA;
	}

	if (ret < 0) {
		xm35_dbg("Couldn't get prop %d ret = %d\n", prop, ret);
		return -ENODATA;
	}
	return ret;
}

static const struct power_supply_desc xmusb350_psy_desc = {
	.name = "xmusb350",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = xmusb350_properties,
	.num_properties = ARRAY_SIZE(xmusb350_properties),
	.get_property = xmusb350_get_property,
};

static int xmusb350_init_psy(struct xmusb350_charger *chip)
{
	struct power_supply_config xmusb350_psy_cfg = {};

	xmusb350_psy_cfg.drv_data = chip;
	xmusb350_psy_cfg.of_node = chip->dev->of_node;
	chip->xmusb350_psy = devm_power_supply_register(chip->dev, &xmusb350_psy_desc, &xmusb350_psy_cfg);
	if (IS_ERR(chip->xmusb350_psy)) {
		xm35_err("failed to register xmusb350 psy\n");
		return -1;
	}

	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		xm35_err("failed to check usb_psy\n");
		return -1;
	}

	chip->charger_psy = power_supply_get_by_name("charger");
	if (!chip->charger_psy) {
		xm35_err("failed to check charger_psy\n");
		return -1;
	}

	chip->bms_psy = power_supply_get_by_name("bms");
	if (!chip->bms_psy) {
		xm35_err("failed to check bms_psy\n");
		return -1;
	}

	return 0;
}

static bool xmusb350_check_charger_dev(struct xmusb350_charger *chip)
{
	chip->master_cp = get_charger_by_name("cp_master");
	if (!chip->master_cp) {
		xm35_err("failed to get master_cp\n");
		return false;
	}

	chip->slave_cp = get_charger_by_name("cp_slave");
	if (!chip->slave_cp) {
		xm35_err("failed to get slave_cp\n");
		return false;
	}

	return true;
}

static int load_fw(struct xmusb350_charger *chip, const char *fn, bool force)
{
	int ret;
	int i;
	int pos = 0;
	const struct firmware *fw = NULL;

	file_data = kmalloc(FIRMWARE_FILE_LENGTH, GFP_KERNEL);
	file_date_w = kmalloc(FIRMWARE_FILE_LENGTH, GFP_KERNEL);
	if (ret) {
		xm35_err("Unable to open firmware %s\n", fn);
		ret = ERROR_REQUESET_FIRMWARE;
		goto release_firmware;
	}

	memset(file_data, 0, FIRMWARE_FILE_LENGTH);
	memset(file_date_w, 0, FIRMWARE_FILE_LENGTH);

	ret = request_firmware(&fw, fn, chip->dev);
	if (ret) {
		xm35_err("Unable to open firmware %s\n", fn);
		ret = ERROR_REQUESET_FIRMWARE;
		goto release_firmware;
	}

	file_size = fw->size;
	for (i = 0; i < file_size / 4; i++) {
		file_date_w[i] =(((uint32_t)(*(fw->data + pos + 0)) << 24) & 0xff000000) |
						(((uint32_t)(*(fw->data + pos + 1)) << 16) & 0x00ff0000) |
						(((uint32_t)(*(fw->data + pos + 2)) << 8) & 0x0000ff00) |
						(((uint32_t)(*(fw->data + pos + 3)) << 0) & 0x000000ff);
		pos += 4;
	}
	pos = 0;

	for (i = 0; i < file_size / 4; i++) {
		file_data[i] =(((uint32_t)(*(fw->data + pos + 0)) << 0) & 0x000000ff) |
						(((uint32_t)(*(fw->data + pos + 1)) << 8) & 0x0000ff00) |
						(((uint32_t)(*(fw->data + pos + 2)) << 16) & 0x00ff0000) |
						(((uint32_t)(*(fw->data + pos + 3)) << 24) & 0xff000000);
		pos += 4;
	}

	return ret;

release_firmware:
	release_firmware(fw);
	kfree(file_data);
	kfree(file_date_w);
	return ret;
}


void Program_init(struct xmusb350_charger *chip)
{
	regmap_write(chip->regmap, 0x42, 0x1F);	/* select test mode */
	regmap_write(chip->regmap, 0x43, 0x9F);	/* close watchdog and open memory clock */
	regmap_write(chip->regmap, 0x44, 0x31); /* select the memory interface */
	regmap_write(chip->regmap, 0x44, 0x33); /* write CS to 1, select the chip, select the memory interface */
}

void Program_end(struct xmusb350_charger *chip)
{
	/* clear the CS/READ signal */
	regmap_write(chip->regmap, 0x44, 0x31);
	regmap_write(chip->regmap, 0x44, 0x30);
}

void verify_init(struct xmusb350_charger *chip)
{
	regmap_write(chip->regmap, 0x42, 0x1F);	/* select test mode */
	regmap_write(chip->regmap, 0x43, 0x9F);	/* close watchdog and open memory clock */
	regmap_write(chip->regmap, 0x44, 0x37);	/* write CS to 1, select the chip, select the memory interface */
}

void verify_end(struct xmusb350_charger *chip)
{
	regmap_write(chip->regmap, 0x44, 0x30);	/* write READ to 0 */
}

uint8_t Check_CRC(uint32_t* file, uint32_t file_size)
{
	uint32_t crc=0xFFFF1326;
	const uint32_t poly = 0x04C11DB6;
	uint32_t newbit, newword, rl_crc;
	uint16_t i=0,j=0;

    crc = 0xFFFF1326;
    for(i = 0; i < file_size/4; i++)
    {
		for(j=0; j<32; j++)
		{
			newbit = ((crc>>31) ^ ((*file>>j)&1)) & 1;
			if(newbit) newword=poly;
			else newword=0;
			rl_crc = (crc<<1) | newbit;
			crc = rl_crc ^ newword;
		}
		file++;
    }
    if(crc == 0xC704DD7B)
        return 1;
	else
		return 0;

}

uint8_t Write_Memory_OneWord_IIC(struct xmusb350_charger *chip, uint16_t addr, uint32_t data)
{
	int i = 0;
	unsigned int write_data[6] = {addr & 0xFF, (addr >> 8) & 0x03, data & 0xFF, (data >> 8) & 0xFF , (data >> 16) & 0xFF , (data >> 24) & 0xFF};
	unsigned int read_date = 0;

	regmap_write(chip->regmap, 0x45, write_data[0]);
	regmap_write(chip->regmap, 0x46, write_data[1]);
	regmap_write(chip->regmap, 0x47, write_data[5]);
	regmap_write(chip->regmap, 0x48, write_data[4]);
	regmap_write(chip->regmap, 0x49, write_data[3]);
	regmap_write(chip->regmap, 0x4a, write_data[2]);
	regmap_write(chip->regmap, 0x4B, 0x01);
	regmap_write(chip->regmap, 0x4B, 0x00);

	for (i = 0; i < 50; i++) {
		regmap_read(chip->regmap, 0x4B, &read_date);
		if(!(read_date & 0x80))
			return 1;
		msleep(2);
	}

	return 0;
}

uint32_t Read_Memory_OneWord_IIC(struct xmusb350_charger *chip, uint16_t addr)
{
	uint32_t data;
	unsigned int write_data[2] = {addr & 0xff, (addr >> 8) & 0x03};
	u8 read_data[4] = {0, 0, 0, 0};

	regmap_write(chip->regmap, 0x45, write_data[0]);
	regmap_write(chip->regmap, 0x46, write_data[1]);

	regmap_raw_read(chip->regmap, 0x4C, read_data, 4);
	data = (read_data[0] << 24) | (read_data[1] << 16) | (read_data[2] << 8) | read_data[3];

	return data;
}

/*Programming example:*/

uint8_t Program(struct xmusb350_charger *chip,uint32_t* file, uint32_t file_size)
{
	uint32_t temp1;
	uint8_t temp3[4];
	uint8_t  temp2;
	uint8_t  flag=0;
	uint32_t* file_w = file_date_w;
	uint16_t i = 0;
	unsigned int data = 0;

	if (Check_CRC(file, file_size) == 0) {
		xm35_err("i350 Check CRC failed! \n");
		return 0;
	}

	file_size/=4;

	if (chip->vid == I350_VID || (chip->vid == 0 && chip->fw_version == 0)) {
		regmap_write(chip->regmap, 0x40, 0x6B);
		msleep(5);
		regmap_read(chip->regmap, 0x40, &data);
		if (data == 0x6B)
			flag = 1;
		else
			flag = 0;
	} else if (chip->vid == I350_TEST_MODE_ID) {
		flag = 1;
	} else {
		flag = 0;
	}

	while (flag) {
		Program_init(chip);
		if (Write_Memory_OneWord_IIC(chip, 0x00, 0) == 0) {
			flag = 0;
			xm35_err("failed to programming the first data to 0\n");
			break;
		}
		for (i = 1; i < file_size; i++) {
			if (Write_Memory_OneWord_IIC(chip, i, *(file_w + i)) == 0) {
				flag = 0;
				xm35_err("i350 Programming failed\n");
				break;
			} else {
				flag = 1;
			}
		}
		Program_end(chip);
		if (i == file_size && flag == 1) {
			verify_init(chip);
			for (i = 1; i < file_size; i++) {
				uint32_t read_data = Read_Memory_OneWord_IIC(chip, i);
				if(*(file_w + i) != read_data){
					flag = 0;
					xm35_err("i350 Verify failed, i = %d\n", i);
					break;
				} else {
					flag = 1;
				}
			}
			verify_end(chip);
		}
		if (flag == 1) {
			Program_init(chip);
			if (Write_Memory_OneWord_IIC(chip, 0x00, *file_w) == 0) {
				flag = 0;
				xm35_err("failed to programming the first data\n");
				break;
			} else {
				flag = 1;
			}
			Program_end(chip);

			verify_init(chip);
			if (*file_w != Read_Memory_OneWord_IIC(chip, 0x00)) {
				flag = 0;
				xm35_err("failed to verify first data\n");
				break;
			} else {
				flag = 1;
			}
			verify_end(chip);
		}

		if (flag == 1) {
			xmusb350_hard_reset(chip);
			chip->fw_update = false;
			get_monotonic_boottime(&chip->update_fw_time);
			xmusb350_get_id(chip);
			if(file_data != NULL)
				kfree(file_data);
			if(file_date_w != NULL)
				kfree(file_date_w);
			xm35_err("i350 upload fw successfull \n");
			return 1;
		}
	}

	if (!flag)
		xmusb350_hard_reset(chip);

	return 0;
}

static void xmusb350_update_fw(struct work_struct *work)
{
	u16 version = 0;
	int v_id = 0;
	static int retry_count = 0;
	union power_supply_propval val = {0,};
	struct xmusb350_charger *chip = container_of(work, struct xmusb350_charger, update_fw_work.work);
	int error = 0;

	chip->fw_update = true;

	power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (val.intval < UPDATE_FW_LOW_CAPACITY) {
		xm35_info("low battery capacity, don't update firmware\n");
		goto done;
	}

	error = load_fw(chip, I350_FW_NAME, true);
	if (error) {
		xm35_err("i350 The firmware update failed(%d)\n", error);
		goto done;
	} else {
		xm35_err("i350 The firmware load succeeded\n");
	}

	error = Program(chip, file_data, file_size);
	if (!error) {
		xm35_err("i350 The firmware update failed(%d)\n", error);
		goto done;
	} else {
		xm35_err("i350 The firmware update succeeded\n");
	}

done:
	chip->fw_update = false;
	__pm_relax(chip->update_fw_wakelock);
}

static ssize_t xmusb350_update_fw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
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

static ssize_t xmusb350_load_fw_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
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

static ssize_t xmusb350_show_log_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", log_level);
	xm35_info("show log_level = %d\n", log_level);

	return ret;
}

static ssize_t xmusb350_store_log_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	ret = sscanf(buf, "%d", &log_level);
	xm35_info("store log_level = %d\n", log_level);

	return count;
}

static ssize_t xmusb350_show_vid_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct xmusb350_charger *chip = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "VID = 0x%08x, HW_VERSION = 0x%02x, FW_VERSION = 0x%02x\n",
			chip->vid, chip->hw_version, chip->fw_version);

	return ret;
}

static DEVICE_ATTR(update_firmware, S_IWUSR, NULL, xmusb350_update_fw_store);
static DEVICE_ATTR(load_firmware, S_IWUSR, NULL, xmusb350_load_fw_store);
static DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR, xmusb350_show_log_level, xmusb350_store_log_level);
static DEVICE_ATTR(vid_version, S_IRUGO, xmusb350_show_vid_version, NULL);

static struct attribute *xmusb350_attributes[] = {
	&dev_attr_load_firmware.attr,
	&dev_attr_update_firmware.attr,
	&dev_attr_log_level.attr,
	&dev_attr_vid_version.attr,
	NULL,
};

static const struct attribute_group xmusb350_attr_group = {
	.attrs = xmusb350_attributes,
};

static int xmusb350_parse_dt(struct xmusb350_charger *chip)
{
	struct device_node *np = chip->dev->of_node;
	int ret = 0;

	if (!np) {
		xm35_err("device tree info missing\n");
		return -1;
	}

	chip->xmusb350_rst_gpio = of_get_named_gpio(np, "xmusb350_rst_gpio", 0);
	if (!gpio_is_valid(chip->xmusb350_rst_gpio)) {
		xm35_err("failed to parse xmusb350_rst_gpio\n");
		return -1;
	}

	chip->xmusb350_sda_gpio = of_get_named_gpio(np, "xmusb350_sda_gpio", 0);
	if (!gpio_is_valid(chip->xmusb350_sda_gpio)) {
		xm35_err("failed to parse xmusb350_sda_gpio\n");
		return -1;
	}

	chip->xmusb350_scl_gpio = of_get_named_gpio(np, "xmusb350_scl_gpio", 0);
	if (!gpio_is_valid(chip->xmusb350_scl_gpio)) {
		xm35_err("failed to parse xmusb350_scl_gpio\n");
		return -1;
	}

	chip->xmusb350_irq_gpio = of_get_named_gpio(np, "xmusb350_irq_gpio", 0);
	if (!gpio_is_valid(chip->xmusb350_irq_gpio)) {
		xm35_err("failed to parse xmusb350_irq_gpio\n");
		return -1;
	}

	//chip->connect_therm_gpio = of_get_named_gpio(np, "vbus_control_gpio", 0);
	//if ((!gpio_is_valid(chip->connect_therm_gpio))) {
	//	dev_err(chip->dev, "%s: no connect_therm_gpio\n", __func__);
	//	return -EINVAL;
	//}

	chip->xmusb350_pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->xmusb350_pinctrl)) {
		xm35_err("failed to get xmusb350_pinctrl\n");
		return -1;
	}

	chip->pinctrl_state_normal = pinctrl_lookup_state(chip->xmusb350_pinctrl, "xmusb350_normal");
	if (IS_ERR_OR_NULL(chip->pinctrl_state_normal)) {
		xm35_err("failed to parse pinctrl_state_normal\n");
		return -1;
	}

	chip->pinctrl_state_isp = pinctrl_lookup_state(chip->xmusb350_pinctrl, "xmusb350_isp");
	if (IS_ERR_OR_NULL(chip->pinctrl_state_isp)) {
		xm35_err("failed to parse pinctrl_state_isp\n");
		return -1;
	}

	ret = pinctrl_select_state(chip->xmusb350_pinctrl, chip->pinctrl_state_normal);
	if (ret) {
		xm35_err("failed to select pinctrl_state_normal\n");
		return -1;
	}

	return 0;
}

static int xmusb350_init_irq(struct xmusb350_charger *chip)
{
	int ret = 0;

	ret = devm_gpio_request(chip->dev, chip->xmusb350_irq_gpio, dev_name(chip->dev));
	if (ret < 0) {
		xm35_err("failed to request gpio\n");
		return -1;
	}

	chip->irq = gpio_to_irq(chip->xmusb350_irq_gpio);
	if (chip->irq < 0) {
		xm35_err("failed to get gpio_irq\n");
		return -1;
	}

	ret = request_irq(chip->irq, xmusb350_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, dev_name(chip->dev), chip);
	if (ret < 0) {
		xm35_err("failed to request irq\n");
		return -1;
	}

	return 0;
}

static void xmusb_parse_cmdline(void)
{
	char *ruby = NULL, *rubypro = NULL, *rubyplus = NULL;
	const char *sku = get_hw_sku();

	ruby = strnstr(sku, "ruby", strlen(sku));
	rubypro = strnstr(sku, "rubypro", strlen(sku));
	rubyplus = strnstr(sku, "rubyplus", strlen(sku));

	if (rubyplus)
		product_name = RUBYPLUS;
	else if (rubypro)
		product_name = RUBYPRO;
	else if (ruby)
		product_name = RUBY;

	xm35_info("product_name = %d, ruby = %d, rubypro = %d, rubyplus = %d\n", product_name, ruby ? 1 : 0, rubypro ? 1 : 0, rubyplus ? 1 : 0);
}

static int xmusb350_charger_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct xmusb350_charger *chip;

	xmusb_parse_cmdline();

	if(product_name != RUBYPLUS)
	      return -ENODEV;

	xm35_info("xmusb350 probe start\n");

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		xm35_err("Failed to allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;

	mutex_init(&chip->isp_sequence_lock);
	chip->irq_handle_wakelock = wakeup_source_register(NULL, "xmusb350_irq_handle_wakelock");
	chip->check_hvdcp3_wakelock = wakeup_source_register(NULL, "xmusb350_check_hvdcp3_wakelock");
	chip->check_usb_ready_wakelock = wakeup_source_register(NULL, "xmusb350_check_usb_ready_wakelock");
	chip->update_fw_wakelock = wakeup_source_register(NULL, "xmusb350_update_fw_wakelock");
	chip->vbus_wave_wa_wakelock = wakeup_source_register(NULL, "xmusb350_vbus_wave_wa_wakelock");
	chip->slow_pd_wa_wakelock = wakeup_source_register(NULL, "xmusb350_slow_pd_wa_wakelock");

	chip->error_status = NO_ERROR;
	chip->xmusb_type = XMUSB350_TYPE_UNKNOW;
	chip->first_type = POWER_SUPPLY_TYPE_UNKNOWN;
	strcpy(chip->model_name, "UNKNOWN");
	chip->thermal_status = TEMP_BELOW_RANGE;

	chip->regmap = devm_regmap_init_i2c(client, &xmusb350_regmap_config);
	if (IS_ERR(chip->regmap)) {
		xm35_err("failed to allocate register map\n");
		return PTR_ERR(chip->regmap);
	}

	i2c_set_clientdata(client, chip);

	ret = xmusb350_parse_dt(chip);
	if (ret) {
		xm35_err("failed to parse DTS\n");
		goto err_mutex;
	}

	ret = xmusb350_init_psy(chip);
	if (ret) {
		xm35_err("failed to init psy\n");
		goto err_psy;
	}

	if (!xmusb350_check_charger_dev(chip)) {
		xm35_err("failed to check charger_dev\n");
		//goto err_psy;
	}

	ret = xmusb350_init_irq(chip);
	if (ret) {
		xm35_err("failed to init irq\n");
		goto err_psy;
	}

	chip->chg_consumer = charger_manager_get_by_name(chip->dev, "charger_port1");
	if (!chip->chg_consumer)
		xm35_info("%s: get charger consumer device failed\n", __func__);

	chip->vbus_disable = false;

	INIT_DELAYED_WORK(&chip->irq_handle_work, xmusb350_irq_handler);
	INIT_DELAYED_WORK(&chip->check_hvdcp3_work, xmusb350_check_hvdcp3);
	INIT_DELAYED_WORK(&chip->update_fw_work, xmusb350_update_fw);
	INIT_DELAYED_WORK(&chip->check_usb_ready_work, xmusb350_check_usb_ready);
	INIT_DELAYED_WORK(&chip->recheck_type_work, xmusb350_recheck_type);
	INIT_DELAYED_WORK(&chip->slow_pd_wa_work, xmusb350_slow_pd_wa);
#ifdef CONN_THERM_WORK
	if(product_name == RUBYPLUS)
		INIT_DELAYED_WORK(&chip->conn_therm_work, smblib_conn_therm_work);
#endif

	chip->chg_dev = charger_device_register("xmusb350", &client->dev, chip, &xmusb350_chg_ops, &xmusb350_chg_props);
	if (!chip->chg_dev) {
		xm35_err("failed to register chg_dev\n");
		goto err_psy;
	}

	ret = sysfs_create_group(&chip->dev->kobj, &xmusb350_attr_group);
	if (ret) {
		xm35_err("failed to register sysfs\n");
		goto err_sysfs;
	}

	__pm_stay_awake(chip->check_usb_ready_wakelock);
	schedule_delayed_work(&chip->check_usb_ready_work, 0);

	chip->chip_ok = true;
	xm35_info("xmusb350 probe success\n");
	return 0;

err_sysfs:
	sysfs_remove_group(&chip->dev->kobj, &xmusb350_attr_group);
err_psy:
	power_supply_unregister(chip->xmusb350_psy);
err_mutex:
	mutex_destroy(&chip->isp_sequence_lock);
	devm_kfree(&client->dev, chip);
	return ret;
}

static int xmusb350_charger_remove(struct i2c_client *client)
{
	struct xmusb350_charger *chip = i2c_get_clientdata(client);

	xm35_info("%s: remove\n", __func__);
	mutex_destroy(&chip->isp_sequence_lock);
	cancel_delayed_work_sync(&chip->irq_handle_work);
	cancel_delayed_work_sync(&chip->check_hvdcp3_work);
	cancel_delayed_work(&chip->check_usb_ready_work);
	cancel_delayed_work_sync(&chip->recheck_type_work);
	cancel_delayed_work_sync(&chip->slow_pd_wa_work);
#ifdef CONN_THERM_WORK
	if(product_name == RUBYPLUS)
		cancel_delayed_work_sync(&chip->conn_therm_work);
#endif

	return 0;
}

static int xmusb350_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct xmusb350_charger *chip = i2c_get_clientdata(client);
#ifdef CONN_THERM_WORK
	if(product_name == RUBYPLUS)
		cancel_delayed_work_sync(&chip->conn_therm_work);
#endif

	return enable_irq_wake(chip->irq);
}

static int xmusb350_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct xmusb350_charger *chip = i2c_get_clientdata(client);
#ifdef CONN_THERM_WORK
	if(product_name == RUBYPLUS && chip->vbus_disable)
		schedule_delayed_work(&chip->conn_therm_work,
					msecs_to_jiffies(THERM_REG_RECHECK_DELAY_200MS));
#endif

	return disable_irq_wake(chip->irq);
}

static const struct dev_pm_ops xmusb350_pm_ops = {
	.suspend	= xmusb350_suspend,
	.resume		= xmusb350_resume,
};

static void xmusb350_charger_shutdown(struct i2c_client *client)
{
	struct xmusb350_charger *chip = i2c_get_clientdata(client);

	xm35_info("%s: shutdown\n", __func__);

	if (chip->hvdcp3_type || chip->xmusb_type == XMUSB350_TYPE_HVDCP_2)
		xmusb350_select_qc_mode(chip, QC_MODE_QC2_5);

	cancel_delayed_work_sync(&chip->irq_handle_work);
	cancel_delayed_work_sync(&chip->check_hvdcp3_work);
	cancel_delayed_work(&chip->check_usb_ready_work);
	cancel_delayed_work_sync(&chip->recheck_type_work);
	cancel_delayed_work_sync(&chip->slow_pd_wa_work);
	mutex_destroy(&chip->isp_sequence_lock);
}

static const struct of_device_id xmusb350_match_table[] = {
	{ .compatible = "xmusb350",},
	{ },
};

static const struct i2c_device_id xmusb350_charger_id[] = {
	{"xmusb350", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, xmusb350_charger_id);

static struct i2c_driver xmusb350_charger_driver = {
	.driver		= {
		.name		= "xmusb350",
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

MODULE_DESCRIPTION("xmusb350");
MODULE_LICENSE("GPL v2");
