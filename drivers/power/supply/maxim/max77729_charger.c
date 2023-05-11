/*
 * Copyrights (C) 2021 Maxim Integrated Products, Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt)	"[MAX77729-chg] %s: " fmt, __func__
#define DEBUG

#include <linux/mfd/max77729-private.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/mfd/max77729.h>
#include <linux/of_gpio.h>
#include <linux/usb/typec/maxim/max77729-muic.h>
#include <linux/usb/typec/maxim/max77729_usbc.h>
#include "max77729_charger.h"
#include "max77729_fuelgauge.h"
#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif
/* #include <linux/sec_debug.h> */

#define ENABLE 1
#define DISABLE 0

#if defined(CONFIG_SEC_FACTORY)
#define WC_CURRENT_WORK_STEP	250
#else
#define WC_CURRENT_WORK_STEP	1000
#endif
#define AICL_WORK_DELAY		100

#define CHG_ICL_CURR_MAX		3000
#define MAIN_ICL_MIN			650
#define MAX77729_CHG_VOTER		"MAX77729_CHG_VOTER"
#define MAIN_CHG_SUSPEND_VOTER	"MAIN_CHG_SUSPEND_VOTER"
#define MAIN_CHG_AICL_VOTER		"MAIN_CHG_AICL_VOTER"
#define MAIN_CHG_ENABLE_VOTER	"MAIN_CHG_ENABLE_VOTER"
#define THERMAL_DAEMON_VOTER    "THERMAL_DAEMON_VOTER"

#define PROBE_CNT_MAX	10


#define PROFILE_CHG_VOTER		"PROFILE_CHG_VOTER"
#define CHG_FCC_CURR_MAX		6000

#define NOTIFY_COUNT_MAX		40

/* extern unsigned int lpcharge; */
extern int factory_mode;
/*maxim glabal usbc struct for battery and usb psy status*/
extern struct max77729_usbc_platform_data *g_usbc_data;
/* extern bool for ffc status check in jeita*/
extern bool g_ffc_disable;

#if defined(CONFIG_NOPMI_CHARGER)
extern int get_prop_battery_charging_enabled(struct votable *usb_icl_votable,
					union power_supply_propval *val);
extern int set_prop_battery_charging_enabled(struct votable *usb_icl_votable,
				const union power_supply_propval *val);
#endif
extern void max77729_usbc_icurr(u8 curr);
extern void max77729_set_fw_noautoibus(int enable);
#if defined(CONFIG_SUPPORT_SHIP_MODE)
extern void max77729_set_fw_ship_mode(int enable);
extern int max77729_get_fw_ship_mode(void);
#endif

extern void stop_usb_host(void *data);
extern void stop_usb_peripheral(void *data);
extern void start_usb_host(void *data, bool ss);
extern void start_usb_peripheral(void *data);
extern int max77729_select_pdo(int num);
extern int max77729_select_pps(int num, int ppsVol, int ppsCur);
extern int com_to_usb_ap(struct max77729_muic_data *muic_data);
extern void max77729_rerun_chgdet(struct max77729_usbc_platform_data *usbpd_data);

static enum power_supply_property max77729_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property max77729_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

#if !defined(CONFIG_NOPMI_CHARGER)
static enum power_supply_property max77729_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_PD_ACTIVE,
	POWER_SUPPLY_PROP_MTBF_CUR,
#if 0
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
#endif
};

static enum power_supply_property max77729_batt_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TECHNOLOGY,
#if 0
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
#endif
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
	POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
};
#endif

static struct device_attribute max77729_charger_attrs[] = {
	MAX77729_CHARGER_ATTR(chip_id),
	MAX77729_CHARGER_ATTR(data),
};

static struct device_attribute sec_otg_attrs[] = {
	SEC_OTG_ATTR(sec_type),
};

static struct max77729_charger_data *g_max77729_charger;


static void max77729_charger_initialize(struct max77729_charger_data *charger);
static int max77729_get_vbus_state(struct max77729_charger_data *charger);
static int max77729_get_charger_state(struct max77729_charger_data *charger);
static void max77729_enable_aicl_irq(struct max77729_charger_data *charger);
static void max77729_chg_set_mode_state(struct max77729_charger_data *charger,
					unsigned int state);
static void max77729_set_switching_frequency(struct max77729_charger_data *charger,
					int frequency);

static int max77729_set_fast_charge_mode(struct max77729_charger_data *charger, int pd_active)
{
	int rc = 0;
	union power_supply_propval prop ={0, };
	int batt_verify = 0, batt_soc = 0, batt_temp = 0;

	if(!charger->psy_bms)
		charger->psy_bms = power_supply_get_by_name("bms");
	if(charger->psy_bms){
		rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_CHIP_OK, &prop);
		if (rc < 0) {
			pr_err("%s : get battery chip ok fail\n",__func__);
		}
		batt_verify = prop.intval;

		rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_CAPACITY, &prop);
		if (rc < 0) {
			pr_err("%s : get battery capatity fail\n", __func__);
		}
		batt_soc = prop.intval;

		rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_TEMP, &prop);
		if (rc < 0) {
			pr_err("%s : get battery temp fail\n", __func__);
		}
		batt_temp = prop.intval;
	}
	/*If TA plug in with PPS, battery auth success and soc less than 95%, FFC flag will enabled.
		The temp is normal set fastcharge mode as 1 and jeita loop also handle fastcharge prop*/
	//pr_err("%s: batt_verify: %d, batt_soc: %d, batt_temp: %d",__func__, batt_verify, batt_soc, batt_temp);
	if ((pd_active == 2) && batt_verify && batt_soc < 95){
		g_ffc_disable = false;
		if(batt_temp >= 150 && batt_temp <= 480){
			prop.intval = 1;
		}else{
			prop.intval = 0;
		}
	}else{
		/*If TA plug in without PPS, battery auth fail and soc exceed 95%, FFC will always be disabled*/
		prop.intval = 0;
		g_ffc_disable = true;
	}

	rc = psy_do_property("bms", set, POWER_SUPPLY_PROP_FASTCHARGE_MODE, prop);
	if (rc < 0) {
		pr_err("%s : set fastcharge mode fail\n", __func__);
	}
	return rc;
}

static bool max77729_charger_unlock(struct max77729_charger_data *charger)
{
	u8 reg_data, chgprot;
	int retry_cnt = 0;
	bool need_init = false;

	do {
		max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_06, &reg_data);
		chgprot = ((reg_data & 0x0C) >> 2);
		if (chgprot != 0x03) {
			max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_06,
					    (0x03 << 2), (0x03 << 2));
			need_init = true;
			msleep(20);
		} else {
			break;
		}
	} while ((chgprot != 0x03) && (++retry_cnt < 10));

	return need_init;
}

static void check_charger_unlock_state(struct max77729_charger_data *charger)
{
	/* pr_debug("%s\n", __func__); */

	if (max77729_charger_unlock(charger)) {
		pr_err("%s: charger locked state, reg init\n", __func__);
		max77729_charger_initialize(charger);
	}
}

static void max77729_test_read(struct max77729_charger_data *charger)
{
	u8 data = 0;
	u32 addr = 0;
	char str[1024] = { 0, };

	for (addr = 0xB1; addr <= 0xC3; addr++) {
		max77729_read_reg(charger->i2c, addr, &data);
		sprintf(str + strlen(str), "[0x%02x]0x%02x, ", addr, data);
	}
	/* pr_info("max77729 : %s\n", str); */
}

static int max77729_get_autoibus(struct max77729_charger_data *charger)
{
	u8 reg_data;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_00, &reg_data);
	if (reg_data & 0x80)
		return 1; /* set by charger */

	return 0; /* set by USBC */
}

static int max77729_get_vbus_state(struct max77729_charger_data *charger)
{
	u8 reg_data;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_00, &reg_data);

	if (is_wireless_type(charger->cable_type))
		reg_data = ((reg_data & MAX77729_WCIN_DTLS) >>
			    MAX77729_WCIN_DTLS_SHIFT);
	else
		reg_data = ((reg_data & MAX77729_CHGIN_DTLS) >>
			    MAX77729_CHGIN_DTLS_SHIFT);

	switch (reg_data) {
	case 0x00:
		/* pr_info("%s: VBUS is invalid. CHGIN < CHGIN_UVLO\n", __func__); */
		break;
	case 0x01:
		/* pr_info("%s: VBUS is invalid. CHGIN < MBAT+CHGIN2SYS and CHGIN > CHGIN_UVLO\n", __func__); */
		break;
	case 0x02:
		/* pr_info("%s: VBUS is invalid. CHGIN > CHGIN_OVLO", __func__); */
		break;
	case 0x03:
		/* pr_info("%s: VBUS is valid. CHGIN < CHGIN_OVLO", __func__); */
		break;
	default:
		break;
	}

	return reg_data;
}
static void max77729_chg_dump_reg(struct max77729_charger_data *charger)
{
	u8 reg_data, i;

	for(i = 0xB0; i <= 0xC3; i++){
		max77729_read_reg(charger->i2c, i, &reg_data);
		/* pr_err("77729_chg_dump_reg:0x%x:0x%x", i, reg_data); */
	}
}

static int max77729_get_charger_state(struct max77729_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 reg_data;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_01, &reg_data);
	/* pr_err("%s : charger status (0x%02x)\n", __func__, reg_data); */

	reg_data &= 0x0f;
	switch (reg_data) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
		status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x04:
		status = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x05:
	case 0x06:
	case 0x07:
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case 0x08:
	case 0xA:
	case 0xB:
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		break;
	}
	if (charger->tmr_chgoff >= 1)
		status = POWER_SUPPLY_STATUS_FULL;


	return (int)status;
}

static bool max77729_chg_get_wdtmr_status(struct max77729_charger_data *charger)
{
	u8 reg_data;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_01, &reg_data);
	reg_data = ((reg_data & MAX77729_CHG_DTLS) >> MAX77729_CHG_DTLS_SHIFT);

	if (reg_data == 0x0B) {
		dev_info(charger->dev, "WDT expired 0x%x !!\n", reg_data);
		return true;
	}

	return false;
}

static int max77729_chg_set_wdtmr_en(struct max77729_charger_data *charger,
					bool enable)
{
	/* pr_info("%s: WDT en = %d\n", __func__, enable); */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00,
			(enable ? CHG_CNFG_00_WDTEN_MASK : 0), CHG_CNFG_00_WDTEN_MASK);

	return 0;
}

static int max77729_chg_set_wdtmr_kick(struct max77729_charger_data *charger)
{
	/* pr_info("%s: WDT Kick\n", __func__); */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_06,
			    (MAX77729_WDTCLR << CHG_CNFG_06_WDTCLR_SHIFT),
			    CHG_CNFG_06_WDTCLR_MASK);

	return 0;
}

static bool max77729_is_constant_current(struct max77729_charger_data *charger)
{
	u8 reg_data;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_01, &reg_data);
	/* pr_info("%s : charger status (0x%02x)\n", __func__, reg_data); */
	reg_data &= 0x0f;

	if (reg_data == 0x01)
		return true;

	return false;
}

static int max77729_set_float_voltage(struct max77729_charger_data *charger,
					int float_voltage)
{
	u8 reg_data = 0;
	int ret = 0;
#if defined(CONFIG_SEC_FACTORY)
	if (factory_mode) {
		float_voltage = charger->pdata->fac_vsys;
		/* pr_info("%s: Factory Mode Skip set float voltage(%d)\n", __func__, float_voltage); */
		// do not return here
	}
#endif
	reg_data =
		(float_voltage == 0) ? 0x13 :
		(float_voltage == 3800) ? 0x38 :
		(float_voltage == 3900) ? 0x39 :
	    (float_voltage >= 4500) ? 0x23 :
	    (float_voltage <= 4200) ? (float_voltage - 4000) / 50 :
	    (((float_voltage - 4200) / 10) + 0x04);

	ret = max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_04,
			    (reg_data << CHG_CNFG_04_CHG_CV_PRM_SHIFT),
			    CHG_CNFG_04_CHG_CV_PRM_MASK);

	ret = max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_04, &reg_data);
	pr_info("%s: battery cv voltage 0x%x\n", __func__, reg_data);
	return ret;
}

static int max77729_get_float_voltage(struct max77729_charger_data *charger)
{
	u8 reg_data = 0;
	int float_voltage;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_04, &reg_data);
	reg_data &= 0x3F;
	float_voltage =
		reg_data <= 0x04 ? reg_data * 50 + 4000 : (reg_data - 4) * 10 + 4200;
	/* pr_debug("%s: battery cv reg : 0x%x, float voltage val : %d\n", */
		/* __func__, reg_data, float_voltage); */

	return float_voltage;
}

static int max77729_get_charging_health(struct max77729_charger_data *charger)
{
	union power_supply_propval value, val_iin, val_vbyp;
	int state = POWER_SUPPLY_HEALTH_GOOD;
	int vbus_state, retry_cnt;
	u8 chg_dtls, reg_data, chg_cnfg_00;
	bool wdt_status, abnormal_status = false;

	/* watchdog kick */
	/* max77729_chg_set_wdtmr_kick(charger); */

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_01, &reg_data);
	reg_data = ((reg_data & MAX77729_BAT_DTLS) >> MAX77729_BAT_DTLS_SHIFT);

	/* if (charger->pdata->enable_noise_wa) { */
		/* psy_do_property("battery", get, POWER_SUPPLY_PROP_CAPACITY, value); */
		/* if ((value.intval >= 80) && */
			/* (charger->fsw_now != MAX77729_CHG_FSW_3MHz)) */
			/* max77729_set_switching_frequency(charger, MAX77729_CHG_FSW_3MHz); */
		/* else if ((value.intval < 80) && */
			/* (charger->fsw_now != MAX77729_CHG_FSW_1_5MHz)) */
			/* max77729_set_switching_frequency(charger, MAX77729_CHG_FSW_1_5MHz); */
	/* } */

	/* pr_info("%s: reg_data(0x%x)\n", __func__, reg_data); */
	switch (reg_data) {
	case 0x00:
		pr_info("%s: No battery and the charger is suspended\n", __func__);
		break;
	case 0x01:
		pr_info("%s: battery is okay but its voltage is low(~VPQLB)\n", __func__);
		break;
	case 0x02:
		pr_info("%s: battery dead\n", __func__);
		break;
	case 0x03:
		break;
	case 0x04:
		pr_info("%s: battery is okay but its voltage is low\n", __func__);
		break;
	case 0x05:
		pr_info("%s: battery ovp\n", __func__);
		break;
	default:
		pr_info("%s: battery unknown\n", __func__);
		break;
	}
	if (charger->is_charging) {
		max77729_read_reg(charger->i2c,	MAX77729_CHG_REG_DETAILS_00, &reg_data);
		/* pr_info("%s: details00(0x%x)\n", __func__, reg_data); */
	}

	/* get wdt status */
	wdt_status = max77729_chg_get_wdtmr_status(charger);

	/* psy_do_property("battery", get, POWER_SUPPLY_EXT_PROP_HEALTH, value); */
	/* VBUS OVP state return battery OVP state */
	vbus_state = max77729_get_vbus_state(charger);
	/* read CHG_DTLS and detecting battery terminal error */
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_01, &chg_dtls);
	chg_dtls = ((chg_dtls & MAX77729_CHG_DTLS) >> MAX77729_CHG_DTLS_SHIFT);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00, &chg_cnfg_00);

	/* print the log at the abnormal case */
	if ((charger->is_charging == 1) && !charger->uno_on
		&& ((chg_dtls == 0x08) || (chg_dtls == 0x0B))) {
		max77729_test_read(charger);
		/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_CHARGING_OFF); */
		/* max77729_set_float_voltage(charger, charger->float_voltage); */
		/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_CHARGING); */
		abnormal_status = true;
	}

	val_iin.intval = SEC_BATTERY_IIN_MA;
	psy_do_property("bms", get,
		POWER_SUPPLY_EXT_PROP_MEASURE_INPUT, val_iin);

	val_vbyp.intval = SEC_BATTERY_VBYP;
	psy_do_property("bms", get,
		POWER_SUPPLY_EXT_PROP_MEASURE_INPUT, val_vbyp);

	/* pr_info("%s: vbus_state: 0x%x, chg_dtls: 0x%x, iin: %dmA, vbyp: %dmV, health: %d, abnormal: %s\n", */
		/* __func__, vbus_state, chg_dtls, val_iin.intval, */
		/* val_vbyp.intval, value.intval, (abnormal_status ? "true" : "false")); */

	/*  OVP is higher priority */
	if (vbus_state == 0x02) {	/*  CHGIN_OVLO */
		pr_info("%s: vbus ovp\n", __func__);
		if (is_wireless_type(charger->cable_type)) {
			retry_cnt = 0;
			do {
				msleep(50);
				vbus_state = max77729_get_vbus_state(charger);
			} while ((retry_cnt++ < 2) && (vbus_state == 0x02));
			if (vbus_state == 0x02) {
				state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
				pr_info("%s: wpc and over-voltage\n", __func__);
			} else {
				state = POWER_SUPPLY_HEALTH_GOOD;
			}
		} else {
			state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		}
	} else if (((vbus_state == 0x0) || (vbus_state == 0x01)) && (chg_dtls & 0x08)
		   && (chg_cnfg_00 & MAX77729_MODE_5_BUCK_CHG_ON)
		   && (is_not_wireless_type(charger->cable_type))) {
		pr_info("%s: vbus is under\n", __func__);
		state = POWER_SUPPLY_HEALTH_UNKNOWN;
	/* } else if ((value.intval == POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE) && */
		   /* ((vbus_state == 0x0) || (vbus_state == 0x01)) && */
		   /* (is_not_wireless_type(charger->cable_type))) { */
		/* pr_info("%s: keep under-voltage\n", __func__); */
		/* state = POWER_SUPPLY_EXT_HEALTH_UNDERVOLTAGE; */
	} else if (wdt_status) {
		pr_info("%s: wdt expired\n", __func__);
		state = POWER_SUPPLY_HEALTH_WATCHDOG_TIMER_EXPIRE;
	} else if (is_wireless_type(charger->cable_type)) {
		if (abnormal_status || (vbus_state == 0x00) || (vbus_state == 0x01))
			charger->misalign_cnt++;
		else
			charger->misalign_cnt = 0;

		if (charger->misalign_cnt >= 3) {
			psy_do_property("battery",
				get, POWER_SUPPLY_PROP_STATUS, value);
			if (value.intval != POWER_SUPPLY_STATUS_FULL) {
				pr_info("%s: invalid WCIN, Misalign occurs!\n", __func__);
				value.intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
				psy_do_property(charger->pdata->wireless_charger_name,
					set, POWER_SUPPLY_PROP_STATUS, value);
			} else {
				charger->misalign_cnt = 0;
			}
		}
	}

	if (state == POWER_SUPPLY_HEALTH_GOOD && charger->psy_bms) {
		power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_TEMP, &value);

		if(value.intval >= 600)
		{
			state = POWER_SUPPLY_HEALTH_OVERHEAT;
		}
		else if(value.intval >= 580 && value.intval < 600)
		{
			state = POWER_SUPPLY_HEALTH_HOT;
		}
		else if(value.intval >= 450 && value.intval < 580)
		{
			state = POWER_SUPPLY_HEALTH_WARM;
		}
		else if(value.intval >= 150 && value.intval < 450)
		{
			state = POWER_SUPPLY_HEALTH_GOOD;
		}
		else if(value.intval >= 0 && value.intval < 150)
		{
			state = POWER_SUPPLY_HEALTH_COOL;
		}
		else if(value.intval < 0)
		{
			state = POWER_SUPPLY_HEALTH_COLD;
		}
	}
	return (int)state;
}

static int max77729_get_charge_current(struct max77729_charger_data *charger)
{
	u8 reg_data;
	int get_current = 0;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_02, &reg_data);
	reg_data &= MAX77729_CHG_CC;

	get_current = reg_data <= 0x2 ? 100 : reg_data * 50;

	/* pr_info("%s: reg:(0x%x), charging_current:(%d)\n", */
			/* __func__, reg_data, get_current); */

	return get_current;
}

static int max77729_get_input_current_type(struct max77729_charger_data
					*charger, int cable_type)
{
	u8 reg_data;
	int get_current = 0;

	if (cable_type == SEC_BATTERY_CABLE_WIRELESS) {
		max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_10, &reg_data);
		/* AND operation for removing the formal 2bit  */
		reg_data &= MAX77729_CHG_WCIN_LIM;

		if (reg_data <= 0x3)
			get_current = 100;
		else if (reg_data >= 0x3F)
			get_current = 1600;
		else
			get_current = (reg_data + 0x01) * 25;
	} else {
		max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_09, &reg_data);
		/* AND operation for removing the formal 1bit  */
		reg_data &= MAX77729_CHG_CHGIN_LIM;

		if (reg_data <= 0x3)
			get_current = 100;
		else if (reg_data >= 0x7F)
			get_current = 3200;
		else
			get_current = (reg_data + 0x01) * 25;
	}
	/* pr_info("%s: reg:(0x%x), charging_current:(%d)\n", */
			/* __func__, reg_data, get_current); */

	return get_current;
}

static int max77729_get_input_current(struct max77729_charger_data *charger)
{
	if (is_wireless_type(charger->cable_type))
		return max77729_get_input_current_type(charger, SEC_BATTERY_CABLE_WIRELESS);
	else
		return max77729_get_input_current_type(charger,	SEC_BATTERY_CABLE_TA);
}

static void reduce_input_current(struct max77729_charger_data *charger, int curr)
{
	u8 set_reg = 0, set_mask = 0, set_value = 0;
	unsigned int curr_step = 25;
	int input_current = 0, max_value = 0;

	input_current = max77729_get_input_current(charger);
	if (input_current <= MINIMUM_INPUT_CURRENT)
		return;

	if (is_wireless_type(charger->cable_type)) {
		set_reg = MAX77729_CHG_REG_CNFG_10;
		set_mask = MAX77729_CHG_WCIN_LIM;
		max_value = 1600;
	} else {
		set_reg = MAX77729_CHG_REG_CNFG_09;
		set_mask = MAX77729_CHG_CHGIN_LIM;
		max_value = 3200;
	}

	input_current -= curr;
	input_current = (input_current > max_value) ? max_value :
		((input_current < MINIMUM_INPUT_CURRENT) ? MINIMUM_INPUT_CURRENT : input_current);

	set_value |= (input_current / curr_step) - 0x01;
	max77729_update_reg(charger->i2c, set_reg, set_value, set_mask);

	/* pr_err("%s: reg:(0x%x), val(0x%x), input current(%d)\n", */
		/* __func__, set_reg, set_value, input_current); */
	charger->input_current = max77729_get_input_current(charger);
	charger->aicl_curr = input_current;
	vote(charger->usb_icl_votable, MAIN_CHG_AICL_VOTER, true, input_current);

}

static bool max77729_check_battery(struct max77729_charger_data *charger)
{
	u8 reg_data, reg_data2;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_OK, &reg_data);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_00, &reg_data2);
	/* pr_info("%s: CHG_INT_OK(0x%x), CHG_DETAILS00(0x%x)\n", */
		/* __func__, reg_data, reg_data2); */

	if ((reg_data & MAX77729_BATP_OK) || !(reg_data2 & MAX77729_BATP_DTLS))
		return true;
	else
		return false;
}

static void max77729_check_cnfg12_reg(struct max77729_charger_data *charger)
{
	static bool is_valid = true;
	u8 valid_cnfg12, reg_data;

	if (is_valid) {
		reg_data = MAX77729_CHG_WCINSEL;
		valid_cnfg12 = (is_wireless_type(charger->cable_type)) ?
			reg_data : (reg_data | (1 << CHG_CNFG_12_CHGINSEL_SHIFT));

		max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12, &reg_data);
		/* pr_info("%s: valid_data = 0x%2x, reg_data = 0x%2x\n", */
			/* __func__, valid_cnfg12, reg_data); */
		if (valid_cnfg12 != reg_data) {
			max77729_test_read(charger);
			is_valid = false;
		}
	}
}

static void max77729_change_charge_path(struct max77729_charger_data *charger,
					int path)
{
	u8 cnfg12;

	path =0;

	/* if (is_wireless_type(path)) */
		/* cnfg12 = (0 << CHG_CNFG_12_CHGINSEL_SHIFT); */
	/* else */
	cnfg12 = (1 << CHG_CNFG_12_CHGINSEL_SHIFT);

	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12,
			    cnfg12, CHG_CNFG_12_CHGINSEL_MASK);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12, &cnfg12);
	/* pr_info("%s : CHG_CNFG_12(0x%02x)\n", __func__, cnfg12); */

	max77729_check_cnfg12_reg(charger);
}

static void max77729_set_ship_mode(struct max77729_charger_data *charger,
					int enable)
{
	u8 cnfg07 = ((enable ? 1 : 0) << CHG_CNFG_07_REG_SHIPMODE_SHIFT);

	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_07,
			    cnfg07, CHG_CNFG_07_REG_SHIPMODE_MASK);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_07, &cnfg07);
	/* pr_info("%s : CHG_CNFG_07(0x%02x)\n", __func__, cnfg07); */
}

static void max77729_set_auto_ship_mode(struct max77729_charger_data *charger,
					int enable)
{
	u8 cnfg03 = ((enable ? 1 : 0) << CHG_CNFG_03_REG_AUTO_SHIPMODE_SHIFT);

	/* auto ship mode should work under 2.6V */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_03,
			    cnfg03, CHG_CNFG_03_REG_AUTO_SHIPMODE_MASK);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_03, &cnfg03);
	/* pr_info("%s : CHG_CNFG_03(0x%02x)\n", __func__, cnfg03); */
}

static int max77729_set_input_current(struct max77729_charger_data *charger,
					int input_current)
{
	int curr_step = 25;
	u8 set_reg, set_mask, reg_data = 0;
	int ret = 0;
#if defined(CONFIG_SEC_FACTORY)
	if (factory_mode) {
		pr_info("%s: Factory Mode Skip set input current\n", __func__);
		return -1;
	}
#endif

	mutex_lock(&charger->charger_mutex);

	if (is_wireless_type(charger->cable_type)) {
		set_reg = MAX77729_CHG_REG_CNFG_10;
		set_mask = MAX77729_CHG_WCIN_LIM;
		input_current = (input_current > 1600) ? 1600 : input_current;
	} else {
		set_reg = MAX77729_CHG_REG_CNFG_09;
		set_mask = MAX77729_CHG_CHGIN_LIM;
		input_current = (input_current > 3200) ? 3200 : input_current;
	}

	if (input_current >= 100)
		reg_data = (input_current / curr_step) - 0x01;

	ret = max77729_update_reg(charger->i2c, set_reg, reg_data, set_mask);

	if (!max77729_get_autoibus(charger))
		max77729_set_fw_noautoibus(MAX77729_AUTOIBUS_AT_OFF);

	mutex_unlock(&charger->charger_mutex);
	/* pr_err("[%s] REG(0x%02x) DATA(0x%02x), CURRENT(%d)\n", */
		/* __func__, set_reg, reg_data, input_current); */
	return ret;
}

static int  max77729_set_charge_current(struct max77729_charger_data *charger,
					int fast_charging_current)
{
	int curr_step = 50;
	u8 reg_data = 0;
	int ret = 0;
#if defined(CONFIG_SEC_FACTORY)
	if (factory_mode) {
		pr_info("%s: Factory Mode Skip set charge current\n", __func__);
		return -1;
	}
#endif

	fast_charging_current =
		(fast_charging_current > 3150) ? 3150 : fast_charging_current;
	if (fast_charging_current >= 100)
		reg_data |= (fast_charging_current / curr_step);

	ret = max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_02,
		reg_data, MAX77729_CHG_CC);

	/* pr_info("[%s] REG(0x%02x) DATA(0x%02x), CURRENT(%d)\n", __func__, */
		/* MAX77729_CHG_REG_CNFG_02, reg_data, fast_charging_current); */
	return ret;
}

static void max77729_set_wireless_input_current(
				struct max77729_charger_data *charger, int input_current)
{
	union power_supply_propval value;

	/* __pm_stay_awake(charger->wc_current_ws); */
	if (is_wireless_type(charger->cable_type)) {
		/* Wcurr-A) In cases of wireless input current change,
		 * configure the Vrect adj room to 270mV for safe wireless charging.
		 */
		/* __pm_stay_awake(charger->wc_current_ws); */
		value.intval = WIRELESS_VRECT_ADJ_ROOM_1;	/* 270mV */
		psy_do_property(charger->pdata->wireless_charger_name, set,
				POWER_SUPPLY_EXT_PROP_WIRELESS_RX_CONTROL, value);
		msleep(500); /* delay 0.5sec */
		charger->wc_pre_current = max77729_get_input_current(charger);
		charger->wc_current = input_current;
		pr_info("%s: wc_current(%d), wc_pre_current(%d)\n",
				__func__, charger->wc_current, charger->wc_pre_current);
		if (charger->wc_current > charger->wc_pre_current)
			max77729_set_charge_current(charger, charger->charging_current);
	}
	/* queue_delayed_work(charger->wqueue, &charger->wc_current_work, 0); */
}

static void max77729_set_topoff_current(struct max77729_charger_data *charger,
					int termination_current)
{
	int curr_base = 150, curr_step = 50;
	u8 reg_data;

	if (termination_current < curr_base)
		termination_current = curr_base;
	else if (termination_current > 500)
		termination_current = 500;

	reg_data = (termination_current - curr_base) / curr_step;
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_03,
			    reg_data, CHG_CNFG_03_TO_ITH_MASK);

	/* pr_info("%s: reg_data(0x%02x), topoff(%dmA)\n", */
		/* __func__, reg_data, termination_current); */
}

static void max77729_set_topoff_time(struct max77729_charger_data *charger,
					int topoff_time)
{
	u8 reg_data = (topoff_time / 10) << CHG_CNFG_03_TO_TIME_SHIFT;

	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_03,
		reg_data, CHG_CNFG_03_TO_TIME_MASK);

	/* pr_info("%s: reg_data(0x%02x), topoff_time(%dmin)\n", */
		/* __func__, reg_data, topoff_time); */
}

static void max77729_set_switching_frequency(struct max77729_charger_data *charger,
				int frequency)
{
	u8 cnfg_08;

	/* Set Switching Frequency */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_08,
			(frequency << CHG_CNFG_08_REG_FSW_SHIFT),
			CHG_CNFG_08_REG_FSW_MASK);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_08, &cnfg_08);

	charger->fsw_now = frequency;
	/* pr_info("%s : CHG_CNFG_08(0x%02x)\n", __func__, cnfg_08); */
}

static void max77729_set_skipmode(struct max77729_charger_data *charger,
				int enable)
{
	u8 reg_data = enable ? MAX77729_AUTO_SKIP : MAX77729_DISABLE_SKIP;

	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12,
			reg_data << CHG_CNFG_12_REG_DISKIP_SHIFT,
			CHG_CNFG_12_REG_DISKIP_MASK);
}

static void max77729_set_b2sovrc(struct max77729_charger_data *charger,
					u32 ocp_current, u32 ocp_dtc)
{
	u8 reg_data = MAX77729_B2SOVRC_4_6A;

	if (ocp_current == 0)
		reg_data = MAX77729_B2SOVRC_DISABLE;
	else
		reg_data += (ocp_current - 4600) / 200;

	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_05,
		(reg_data << CHG_CNFG_05_REG_B2SOVRC_SHIFT),
		CHG_CNFG_05_REG_B2SOVRC_MASK);

	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_06,
		((ocp_dtc == 100 ? MAX77729_B2SOVRC_DTC_100MS : 0)
		<< CHG_CNFG_06_B2SOVRC_DTC_SHIFT), CHG_CNFG_06_B2SOVRC_DTC_MASK);

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_05, &reg_data);
	/* pr_info("%s : CHG_CNFG_05(0x%02x)\n", __func__, reg_data); */
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_06, &reg_data);
	/* pr_info("%s : CHG_CNFG_06(0x%02x)\n", __func__, reg_data); */

	return;
}

static int max77729_check_wcin_before_otg_on(struct max77729_charger_data *charger)
{
	union power_supply_propval value = {0,};
	struct power_supply *psy;
	u8 reg_data;

	psy = get_power_supply_by_name("wireless");
	if (!psy)
		return -ENODEV;
	if ((psy->desc->get_property != NULL) &&
		(psy->desc->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &value) >= 0)) {
		if (value.intval)
			return 0;
	} else {
		return -ENODEV;
	}
	power_supply_put(psy);

#if defined(CONFIG_WIRELESS_TX_MODE)
	/* check TX status */
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00, &reg_data);
	reg_data &= CHG_CNFG_00_MODE_MASK;
	if (reg_data == MAX77729_MODE_8_BOOST_UNO_ON ||
		reg_data == MAX77729_MODE_C_BUCK_BOOST_UNO_ON ||
		reg_data == MAX77729_MODE_D_BUCK_CHG_BOOST_UNO_ON) {
		value.intval = BATT_TX_EVENT_WIRELESS_TX_OTG_ON;
		psy_do_property("wireless", set,
			POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR, value);
		return 0;
	}
#endif

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_00, &reg_data);
	reg_data = ((reg_data & MAX77729_WCIN_DTLS) >> MAX77729_WCIN_DTLS_SHIFT);
	if ((reg_data != 0x03) || (charger->pdata->wireless_charger_name == NULL))
		return 0;

	psy_do_property(charger->pdata->wireless_charger_name, get,
		POWER_SUPPLY_PROP_ENERGY_NOW, value);
	if (value.intval <= 0)
		return -ENODEV;

	value.intval = WIRELESS_VOUT_5V;
	psy_do_property(charger->pdata->wireless_charger_name, set,
		POWER_SUPPLY_EXT_PROP_INPUT_VOLTAGE_REGULATION, value);

    return 0;
}

static int max77729_set_otg(struct max77729_charger_data *charger, int enable)
{
	union power_supply_propval value;
	u8 chg_int_state;
	int ret = 0;

	/* pr_err("%s: CHGIN-OTG %s\n", __func__,	enable > 0 ? "on" : "off"); */
	/*if (charger->otg_on == enable) //|| lpcharge)
		return 0;*/

	if (charger->pdata->wireless_charger_name) {
		ret = max77729_check_wcin_before_otg_on(charger);
		if (ret < 0)
			return ret;
	}

	__pm_stay_awake(charger->otg_ws);
	/* CHGIN-OTG */
	value.intval = enable;

	/* otg current limit 900mA */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_02,
			MAX77729_OTG_ILIM_1500 << CHG_CNFG_02_OTG_ILIM_SHIFT,
			CHG_CNFG_02_OTG_ILIM_MASK);

	if (enable) {
		/* psy_do_property("wireless", set, */
			/* POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, value); */

		mutex_lock(&charger->charger_mutex);
		charger->otg_on = enable;
		/* OTG on, boost on */

		vote(charger->chgctrl_votable, MAX77729_CHG_VOTER, true, SEC_BAT_CHG_MODE_BUCK_OFF);
		/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_BUCK_OFF); */
		max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_OTG_ON);
		mutex_unlock(&charger->charger_mutex);
	} else {
		mutex_lock(&charger->charger_mutex);
		charger->otg_on = enable;
		/* OTG off(UNO on), boost off */
		max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_OTG_OFF);
		/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_CHARGING); */
		vote(charger->chgctrl_votable, MAX77729_CHG_VOTER, true, SEC_BAT_CHG_MODE_CHARGING);
		mutex_unlock(&charger->charger_mutex);
		msleep(50);

		/* psy_do_property("wireless", set, */
			/* POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, value); */
	}
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_MASK, &chg_int_state);

	__pm_relax(charger->otg_ws);
	/* pr_info("%s: INT_MASK(0x%x)\n", __func__, chg_int_state); */
	power_supply_changed(charger->psy_otg);

	return 0;
}

static void max77729_check_slow_charging(struct max77729_charger_data *charger,
					int input_current)
{
	union power_supply_propval value;

	/* under 400mA considered as slow charging concept for VZW */
	if (input_current <= SLOW_CHARGING_CURRENT_STANDARD &&
	    !is_nocharge_type(charger->cable_type)) {
		charger->slow_charging = true;
		psy_do_property("battery", set,	POWER_SUPPLY_PROP_CHARGE_TYPE, value);
	} else {
		charger->slow_charging = false;
	}
}

static void max77729_charger_initialize(struct max77729_charger_data *charger)
{
	u8 reg_data;
	int jig_gpio;

	/* pr_info("%s\n", __func__); */

	max77729_write_reg(charger->i2c,
			MAX77729_CHG_REG_INT_MASK, 0xff);                //disable all chg interrupt

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00, &reg_data);
	charger->cnfg00_mode = (reg_data & CHG_CNFG_00_MODE_MASK);

	/* unlock charger setting protect slowest LX slope
	 */
	reg_data = (0x03 << 2);
	reg_data |= 0x60;
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_06, reg_data, reg_data);

#if !defined(CONFIG_SEC_FACTORY)
	/* If DIS_AICL is enabled(CNFG06[4]: 1) from factory_mode,
	 * clear to 0 to disable DIS_AICL
	 */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_06,
			MAX77729_DIS_AICL << CHG_CNFG_06_DIS_AICL_SHIFT,
			CHG_CNFG_06_DIS_AICL_MASK);
#endif

	/* fast charge timer disable
	 * restart threshold disable
	 * pre-qual charge disable
	 */
	reg_data = (MAX77729_FCHGTIME_DISABLE << CHG_CNFG_01_FCHGTIME_SHIFT) |
			(MAX77729_CHG_RSTRT_DISABLE << CHG_CNFG_01_CHG_RSTRT_SHIFT) |
			(MAX77729_CHG_PQEN_DISABLE << CHG_CNFG_01_PQEN_SHIFT);
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_01, reg_data,
	(CHG_CNFG_01_FCHGTIME_MASK | CHG_CNFG_01_CHG_RSTRT_MASK | CHG_CNFG_01_PQEN_MASK));

	/* enalbe RECYCLE_EN for ocp */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_01,
			MAX77729_RECYCLE_EN_ENABLE << CHG_CNFG_01_RECYCLE_EN_SHIFT,
			CHG_CNFG_01_RECYCLE_EN_MASK);

	/* OTG off(UNO on), boost off */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00,
			0, CHG_CNFG_00_OTG_CTRL);

	/* otg current limit 900mA */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_02,
			MAX77729_OTG_ILIM_1500 << CHG_CNFG_02_OTG_ILIM_SHIFT,
			CHG_CNFG_02_OTG_ILIM_MASK);

	/* UNO ILIM 1.0A */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_05,
			MAX77729_UNOILIM_1000 << CHG_CNFG_05_REG_UNOILIM_SHIFT,
			CHG_CNFG_05_REG_UNOILIM_MASK);

	/* BAT to SYS OCP */
	max77729_set_b2sovrc(charger,
		charger->pdata->chg_ocp_current, charger->pdata->chg_ocp_dtc);

	/* top off current 200mA */
	/* reg_data = (MAX77729_TO_ITH_150MA << CHG_CNFG_03_TO_ITH_SHIFT) | */
			/* (MAX77729_SYS_TRACK_DISABLE << CHG_CNFG_03_SYS_TRACK_DIS_SHIFT); */
	/* max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_03, reg_data, */
	/* (CHG_CNFG_03_TO_ITH_MASK | CHG_CNFG_03_TO_TIME_MASK | CHG_CNFG_03_SYS_TRACK_DIS_MASK)); */

	max77729_write_reg(charger->i2c, 0xBA, 0x81);

	max77729_set_topoff_current(charger, 250);
	/* topoff_time */
	max77729_set_topoff_time(charger, 1);

	max77729_write_reg(charger->i2c, 0xB8, 0xD8);
	/* cv voltage 4.2V or 4.35V */
	max77729_set_float_voltage(charger, charger->pdata->chg_float_voltage);

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_OK, &reg_data);
	/* VCHGIN : REG=4.6V, UVLO=4.8V
	 * to fix CHGIN-UVLO issues including cheapy battery packs
	 */
	if (reg_data & MAX77729_CHGIN_OK){
//modify by HTH-234718 at 2022/05/26 begin
		max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12,
			0x00, CHG_CNFG_12_VCHGIN_REG_MASK);
//modify by HTH-234718 at 2022/05/26 end
	}
	/* VCHGIN : REG=4.5V, UVLO=4.7V */
	else{
//modify by HTH-234718 at 2022/05/26 begin
		max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12,
			0x00, CHG_CNFG_12_VCHGIN_REG_MASK);
//modify by HTH-234718 at 2022/05/26 end
	}

	/* Boost mode possible in FACTORY MODE */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_07,
			    MAX77729_CHG_FMBST, CHG_CNFG_07_REG_FMBST_MASK);

	if (charger->jig_low_active)
		jig_gpio = !gpio_get_value(charger->jig_gpio);
	else
		jig_gpio = gpio_get_value(charger->jig_gpio);

	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_07,
			    (jig_gpio << CHG_CNFG_07_REG_FGSRC_SHIFT),
			    CHG_CNFG_07_REG_FGSRC_MASK);

	/* Watchdog Enable */
	/* max77729_chg_set_wdtmr_en(charger, 1); */

	/* Active Discharge Enable */
	max77729_update_reg(charger->pmic_i2c, MAX77729_PMIC_REG_MAINCTRL1, 0x01, 0x01);

	max77729_write_reg(charger->i2c, MAX77729_CHG_REG_CNFG_09, 0x13);
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_09,
			    MAX77729_CHG_EN, MAX77729_CHG_EN);

	/* VBYPSET=5.0V */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_11,
			0x00, CHG_CNFG_11_VBYPSET_MASK);

	/* Switching Frequency */
	max77729_set_switching_frequency(charger, charger->pdata->fsw);

	/* Auto skip mode */
	max77729_set_skipmode(charger, 1);

	/* disable shipmode */
	max77729_set_ship_mode(charger, 0);

	/* enable auto shipmode, this should work under 2.6V */
	max77729_set_auto_ship_mode(charger, 0);

	max77729_test_read(charger);
}

static void max77729_set_sysovlo(struct max77729_charger_data *charger, int enable)
{
	u8 reg_data;

	max77729_read_reg(charger->pmic_i2c,
		MAX77729_PMIC_REG_SYSTEM_INT_MASK, &reg_data);

	reg_data = enable ? reg_data & 0xDF : reg_data | 0x20;
	max77729_write_reg(charger->pmic_i2c,
		MAX77729_PMIC_REG_SYSTEM_INT_MASK, reg_data);

	max77729_read_reg(charger->pmic_i2c,
		MAX77729_PMIC_REG_SYSTEM_INT_MASK, &reg_data);
	/* pr_info("%s: check topsys irq mask(0x%x), enable(%d)\n", */
		/* __func__, reg_data, enable); */
}

static void max77729_chg_monitor_work(struct max77729_charger_data *charger)
{
	u8 reg_b2sovrc = 0, reg_mode = 0;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00, &reg_mode);
	reg_mode = (reg_mode & CHG_CNFG_00_MODE_MASK) >> CHG_CNFG_00_MODE_SHIFT;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_05, &reg_b2sovrc);
	reg_b2sovrc =
		(reg_b2sovrc & CHG_CNFG_05_REG_B2SOVRC_MASK) >> CHG_CNFG_05_REG_B2SOVRC_SHIFT;

	/* pr_info("%s: [CHG] MODE(0x%x), B2SOVRC(0x%x), otg_on(%d)\n", */
		/* __func__, reg_mode, reg_b2sovrc, charger->otg_on); */
}

static int max77729_chg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(max77729_charger_attrs); i++) {
		rc = device_create_file(dev, &max77729_charger_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &max77729_charger_attrs[i]);
	return rc;
}

ssize_t max77729_chg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max77729_charger_data *charger = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max77729_charger_attrs;
	int i = 0;
	u8 addr, data;

	switch (offset) {
	case CHIP_ID:
		max77729_read_reg(charger->pmic_i2c, MAX77729_PMIC_REG_PMICID1, &data);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", data);
		break;
	case DATA:
		for (addr = 0xB1; addr <= 0xC3; addr++) {
			max77729_read_reg(charger->i2c, addr, &data);
			i += scnprintf(buf + i, PAGE_SIZE - i,
				       "0x%02x : 0x%02x\n", addr, data);
		}
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t max77729_chg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max77729_charger_data *charger = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - max77729_charger_attrs;
	int ret = 0;
	int x, y;

	switch (offset) {
	case CHIP_ID:
		ret = count;
		break;
	case DATA:
		if (sscanf(buf, "0x%8x 0x%8x", &x, &y) == 2) {
			if (x >= 0xB1 && x <= 0xC3) {
				u8 addr = x, data = y;

				if (max77729_write_reg(charger->i2c, addr, data) < 0)
					dev_info(charger->dev, "%s: addr: 0x%x write fail\n",
						__func__, addr);
			} else {
				dev_info(charger->dev, "%s: addr: 0x%x is wrong\n",
					__func__, x);
			}
		}
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

ssize_t sec_otg_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - sec_otg_attrs;
	int i = 0;

	switch (offset) {
	case OTG_SEC_TYPE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "OTG\n");
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

static int sec_otg_create_attrs(struct device *dev)
{
	unsigned long i = 0;
	int rc = 0;

	for (i = 0; i < ARRAY_SIZE(sec_otg_attrs); i++) {
		rc = device_create_file(dev, &sec_otg_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	while (i--)
		device_remove_file(dev, &sec_otg_attrs[i]);
create_attrs_succeed:
	return rc;
}

static void max77729_set_uno(struct max77729_charger_data *charger, int en)
{
	u8 chg_int_state;
	u8 reg;

	if (charger->otg_on) {
		pr_info("%s: OTG ON, then skip UNO Control\n", __func__);
		if (en) {
#if defined(CONFIG_WIRELESS_TX_MODE)
			union power_supply_propval value = {0, };
			psy_do_property("battery", get,
				POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ENABLE, value);
			if (value.intval) {
				value.intval = BATT_TX_EVENT_WIRELESS_TX_ETC;
				psy_do_property("wireless", set, POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR, value);
			}
#endif
		}
		return;
	}

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_OK, &reg);
	if (en && (reg & MAX77729_WCIN_OK)) {
		pr_info("%s: WCIN is already valid by wireless charging, then skip UNO Control\n",
			__func__);
		return;
	}

	if (en == SEC_BAT_CHG_MODE_UNO_ONLY) {
		charger->uno_on = true;
		charger->cnfg00_mode = MAX77729_MODE_8_BOOST_UNO_ON;
		max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00,
				charger->cnfg00_mode, CHG_CNFG_00_MODE_MASK);
	} else if (en == SEC_BAT_CHG_MODE_CHARGING_OFF) {
		charger->uno_on = true;
		/* UNO on, boost on */
		/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_UNO_ON); */
	} else {
		charger->uno_on = false;
		/* boost off */
		/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_UNO_OFF); */
		msleep(50);
	}
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_MASK, &chg_int_state);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00, &reg);
	pr_info("%s: UNO(%d), INT_MASK(0x%x), CHG_CNFG_00(0x%x)\n",
		__func__, charger->uno_on, chg_int_state, reg);
}

static void max77729_set_uno_iout(struct max77729_charger_data *charger, int iout)
{
	u8 reg = 0;

	if (iout < 300)
		reg = MAX77729_UNOILIM_200;
	else if (iout >= 300 && iout < 400)
		reg = MAX77729_UNOILIM_300;
	else if (iout >= 400 && iout < 600)
		reg = MAX77729_UNOILIM_400;
	else if (iout >= 600 && iout < 800)
		reg = MAX77729_UNOILIM_600;
	else if (iout >= 800 && iout < 1000)
		reg = MAX77729_UNOILIM_800;
	else if (iout >= 1000 && iout < 1500)
		reg = MAX77729_UNOILIM_1000;
	else if (iout >= 1500)
		reg = MAX77729_UNOILIM_1500;

	if (reg)
		max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_05,
				reg << CHG_CNFG_05_REG_UNOILIM_SHIFT,
				CHG_CNFG_05_REG_UNOILIM_MASK);

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_05, &reg);
	pr_info("@Tx_mode %s: CNFG_05 (0x%x)\n", __func__, reg);
}

static void max77729_set_uno_vout(struct max77729_charger_data *charger, int vout)
{
	u8 reg = 0;

	if (vout == WC_TX_VOUT_OFF) {
		pr_info("%s: set UNO default\n", __func__);
	} else {
		/* Set TX Vout(VBYPSET) */
		reg = (vout * 5);
		pr_info("%s: UNO VOUT (0x%x)\n", __func__, reg);
	}
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_11,
				reg, CHG_CNFG_11_VBYPSET_MASK);

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_11, &reg);
	pr_info("@Tx_mode %s: CNFG_11(0x%x)\n", __func__, reg);
}

#if !defined(CONFIG_NOPMI_CHARGER)
static int get_prop_battery_charging_enabled(struct max77729_charger_data *charger,
					union power_supply_propval *val)
{
	int icl = MAIN_ICL_MIN;

	val->intval = !(get_client_vote(charger->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER) == icl);
	pr_err("get_prop_battery_charging_enabled:%d", val->intval);

	return 0;
}
static int set_prop_battery_charging_enabled(struct max77729_charger_data *charger,
				const union power_supply_propval *val)
{
	int icl = MAIN_ICL_MIN;
	pr_err("set_prop_battery_charging_enabled:%d", val->intval);

	if (val->intval == 0) {
		vote(charger->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER, true, icl);
	} else {
		vote(charger->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER, false, 0);
	}

	return 0;
}
#endif

static int max77729_chg_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct max77729_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 reg_data;
	int rc;
	union power_supply_propval value;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = SEC_BATTERY_CABLE_NONE;
		if (!max77729_read_reg(charger->i2c,
				      MAX77729_CHG_REG_INT_OK, &reg_data)) {
			if (reg_data & MAX77729_WCIN_OK) {
				val->intval = SEC_BATTERY_CABLE_WIRELESS;
			} else if (reg_data & MAX77729_CHGIN_OK) {
				val->intval = SEC_BATTERY_CABLE_TA;
			}
		}

		if(!charger->psy_bms)
			charger->psy_bms = power_supply_get_by_name("bms");
                if (charger->psy_bms) {
                        rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_VOLTAGE_AVG, &value);
                        if (rc < 0) {
                                value.intval = 3800;
                                pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_AVG fail\n", __func__);
                        }
                }
                if (value.intval < 3300)
                       val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max77729_check_battery(charger);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77729_get_charger_state(charger);
		if (get_effective_result_locked(charger->fv_votable) < 4450) {
			if (val->intval == POWER_SUPPLY_STATUS_FULL) {
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			}
		} else if (get_client_vote_locked(charger->usb_icl_votable, MAIN_CHG_ENABLE_VOTER) == MAIN_ICL_MIN) {
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!charger->is_charging) {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		} else if (charger->slow_charging) {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			pr_info("%s: slow-charging mode\n", __func__);
		} else {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77729_get_charging_health(charger);
		max77729_check_cnfg12_reg(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = charger->input_current;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = max77729_get_input_current_type(charger,
			(is_wireless_type(val->intval)) ?
			SEC_BATTERY_CABLE_WIRELESS : SEC_BATTERY_CABLE_TA);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = charger->charging_current;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = max77729_get_charge_current(charger);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = max77729_get_float_voltage(charger);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		max77729_read_reg(charger->i2c,
				  MAX77729_CHG_REG_DETAILS_01, &reg_data);
		reg_data &= 0x0F;
		switch (reg_data) {
		case 0x01:
			val->strval = "CC Mode";
			break;
		case 0x02:
			val->strval = "CV Mode";
			break;
		case 0x03:
			val->strval = "EOC";
			break;
		case 0x04:
			val->strval = "DONE";
			break;
		default:
			val->strval = "NONE";
			break;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		val->intval = (charger->charge_mode == SEC_BAT_CHG_MODE_CHARGING?1 : 0);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = max77729_is_constant_current(charger) ? 0 : 1;
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_WDT_STATUS:
			if (max77729_chg_get_wdtmr_status(charger)) {
				dev_info(charger->dev, "charger WDT is expired!!\n");
				max77729_test_read(charger);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHIP_ID:
			if (!max77729_read_reg(charger->i2c,
					MAX77729_PMIC_REG_PMICREV, &reg_data)) {
				/* pmic_ver should below 0x7 */
				val->intval =
					(charger->pmic_ver >= 0x1 && charger->pmic_ver <= 0x7);
				pr_info("%s : IF PMIC ver.0x%x\n", __func__,
					charger->pmic_ver);
			} else {
				val->intval = 0;
				pr_info("%s : IF PMIC I2C fail.\n", __func__);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			max77729_test_read(charger);
			max77729_chg_monitor_work(charger);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_BOOST:
			max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00, &reg_data);
			reg_data &= CHG_CNFG_00_MODE_MASK;
			val->intval = (reg_data & MAX77729_MODE_BOOST) ? 1 : 0;
			break;
#if defined(CONFIG_AFC_CHARGER_MODE)
		case POWER_SUPPLY_EXT_PROP_AFC_CHARGER_MODE:
			return -ENODATA;
#endif
		case POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL:
			mutex_lock(&charger->charger_mutex);
			val->intval = charger->otg_on;
			mutex_unlock(&charger->charger_mutex);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_COUNTER_SHADOW:
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			val->intval = charger->charge_mode;
			break;
		case POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST:
#if defined(CONFIG_SUPPORT_SHIP_MODE)
			val->intval = max77729_get_fw_ship_mode();
			pr_info("%s: ship mode op is %d\n", __func__, val->intval);
#else
			val->intval = 0;
			pr_info("%s: ship mode is not supported\n", __func__);
#endif
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static void max77729_chg_set_mode_state(struct max77729_charger_data *charger,
					unsigned int state)
{
	u8 reg;

	if (state == SEC_BAT_CHG_MODE_CHARGING)
		charger->is_charging = true;
	else if (state == SEC_BAT_CHG_MODE_CHARGING_OFF ||
			state == SEC_BAT_CHG_MODE_BUCK_OFF)
		charger->is_charging = false;

#if defined(CONFIG_SEC_FACTORY)
	if (factory_mode) {
		if (state == SEC_BAT_CHG_MODE_CHARGING ||
			state == SEC_BAT_CHG_MODE_CHARGING_OFF ||
			state == SEC_BAT_CHG_MODE_BUCK_OFF) {
			pr_info("%s: Factory Mode Skip set charger state\n", __func__);
			return;
		}
	}
#endif

	mutex_lock(&charger->mode_mutex);
	/* pr_info("%s: current_mode(0x%x), state(%d)\n", __func__, charger->cnfg00_mode, state); */
	switch (charger->cnfg00_mode) {
		/* all off */
	case MAX77729_MODE_0_ALL_OFF:
		if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77729_MODE_4_BUCK_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77729_MODE_5_BUCK_CHG_ON;
		else if (state == SEC_BAT_CHG_MODE_OTG_ON)
			charger->cnfg00_mode = MAX77729_MODE_A_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_ON)
			charger->cnfg00_mode = MAX77729_MODE_8_BOOST_UNO_ON;
		break;
		/* buck only */
	case MAX77729_MODE_4_BUCK_ON:
		if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77729_MODE_5_BUCK_CHG_ON;
		else if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77729_MODE_0_ALL_OFF;
		else if (state == SEC_BAT_CHG_MODE_OTG_ON)
			charger->cnfg00_mode = MAX77729_MODE_E_BUCK_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_ON)
			charger->cnfg00_mode = MAX77729_MODE_C_BUCK_BOOST_UNO_ON;
		break;
		/* buck, charger on */
	case MAX77729_MODE_5_BUCK_CHG_ON:
		if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77729_MODE_0_ALL_OFF;
		else if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77729_MODE_4_BUCK_ON;
		else if (state == SEC_BAT_CHG_MODE_OTG_ON)
			charger->cnfg00_mode = MAX77729_MODE_F_BUCK_CHG_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_ON)
			charger->cnfg00_mode = MAX77729_MODE_D_BUCK_CHG_BOOST_UNO_ON;
		break;
	case MAX77729_MODE_8_BOOST_UNO_ON:
		if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77729_MODE_C_BUCK_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77729_MODE_D_BUCK_CHG_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_OFF)
			charger->cnfg00_mode = MAX77729_MODE_0_ALL_OFF;
		/* UNO -> OTG */
		else if (state == SEC_BAT_CHG_MODE_OTG_ON) {
			max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00,
			    MAX77729_MODE_4_BUCK_ON, CHG_CNFG_00_MODE_MASK);
			usleep_range(1000, 2000);
			/* mode 0x4, and 1msec delay, and then otg on */
			charger->cnfg00_mode = MAX77729_MODE_A_BOOST_OTG_ON;
		}
		break;
	case MAX77729_MODE_A_BOOST_OTG_ON:
		if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77729_MODE_E_BUCK_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77729_MODE_F_BUCK_CHG_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_OTG_OFF)
			charger->cnfg00_mode = MAX77729_MODE_0_ALL_OFF;
		/* OTG -> UNO */
		else if (state == SEC_BAT_CHG_MODE_UNO_ON) {
			max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00,
			    MAX77729_MODE_4_BUCK_ON, CHG_CNFG_00_MODE_MASK);
			usleep_range(1000, 2000);
			/* mode 0x4, and 1msec delay, and then uno on */
			charger->cnfg00_mode = MAX77729_MODE_8_BOOST_UNO_ON;
		}
		break;
	case MAX77729_MODE_C_BUCK_BOOST_UNO_ON:
		if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77729_MODE_8_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77729_MODE_D_BUCK_CHG_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_OFF)
			charger->cnfg00_mode = MAX77729_MODE_4_BUCK_ON;
		/* UNO -> OTG */
		else if (state == SEC_BAT_CHG_MODE_OTG_ON) {
			max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00,
			    MAX77729_MODE_4_BUCK_ON, CHG_CNFG_00_MODE_MASK);
			usleep_range(1000, 2000);
			/* mode 0x4, and 1msec delay, and then otg on */
			charger->cnfg00_mode = MAX77729_MODE_E_BUCK_BOOST_OTG_ON;
		}
		break;
	case MAX77729_MODE_D_BUCK_CHG_BOOST_UNO_ON:
		if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77729_MODE_8_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77729_MODE_C_BUCK_BOOST_UNO_ON;
		else if (state == SEC_BAT_CHG_MODE_UNO_OFF)
			charger->cnfg00_mode = MAX77729_MODE_5_BUCK_CHG_ON;
		/* UNO -> OTG */
		else if (state == SEC_BAT_CHG_MODE_OTG_ON) {
			max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00,
			    MAX77729_MODE_4_BUCK_ON, CHG_CNFG_00_MODE_MASK);
			usleep_range(1000, 2000);
			/* mode 0x4, and 1msec delay, and then otg on */
			charger->cnfg00_mode = MAX77729_MODE_E_BUCK_BOOST_OTG_ON;
		}
		break;
	case MAX77729_MODE_E_BUCK_BOOST_OTG_ON:
		if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77729_MODE_A_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_CHARGING)
			charger->cnfg00_mode = MAX77729_MODE_F_BUCK_CHG_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_OTG_OFF)
			charger->cnfg00_mode = MAX77729_MODE_4_BUCK_ON;
		/* OTG -> UNO */
		else if (state == SEC_BAT_CHG_MODE_UNO_ON) {
			max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00,
			    MAX77729_MODE_4_BUCK_ON, CHG_CNFG_00_MODE_MASK);
			usleep_range(1000, 2000);
			/* mode 0x4, and 1msec delay, and then uno on */
			charger->cnfg00_mode = MAX77729_MODE_C_BUCK_BOOST_UNO_ON;
		}
		break;
	case MAX77729_MODE_F_BUCK_CHG_BOOST_OTG_ON:
		if (state == SEC_BAT_CHG_MODE_CHARGING_OFF)
			charger->cnfg00_mode = MAX77729_MODE_E_BUCK_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_BUCK_OFF)
			charger->cnfg00_mode = MAX77729_MODE_A_BOOST_OTG_ON;
		else if (state == SEC_BAT_CHG_MODE_OTG_OFF)
			charger->cnfg00_mode = MAX77729_MODE_5_BUCK_CHG_ON;
		break;
	}

	if (state == SEC_BAT_CHG_MODE_OTG_ON &&
		charger->cnfg00_mode == MAX77729_MODE_F_BUCK_CHG_BOOST_OTG_ON) {
		/* W/A for shutdown problem when turn on OTG during wireless charging */
		pr_info("%s : disable WCIN_SEL before change mode to 0xF\n", __func__);
		max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12,
			(0 << CHG_CNFG_12_WCINSEL_SHIFT), CHG_CNFG_12_WCINSEL_MASK);
	}

	/* pr_info("%s: current_mode(0x%x)\n", __func__, charger->cnfg00_mode); */
	max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00,
			charger->cnfg00_mode, CHG_CNFG_00_MODE_MASK);

	if (state == SEC_BAT_CHG_MODE_OTG_ON &&
		charger->cnfg00_mode == MAX77729_MODE_F_BUCK_CHG_BOOST_OTG_ON) {
		/* W/A for shutdown problem when turn on OTG during wireless charging */
		pr_info("%s : enable WCIN_SEL after change mode to 0xF\n", __func__);
		/* max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12, */
			/* MAX77729_CHG_WCINSEL, CHG_CNFG_12_WCINSEL_MASK); */
	}

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00, &reg);
	/* pr_info("%s: CNFG_00 (0x%x)\n", __func__, reg); */
	mutex_unlock(&charger->mode_mutex);
}

#if defined(CONFIG_UPDATE_BATTERY_DATA)
static int max77729_charger_parse_dt(struct max77729_charger_data *charger);
#endif
static int max77729_chg_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct max77729_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 reg_data = 0;

	/* check unlock status before does set the register */
	max77729_charger_unlock(charger);
	switch ((int)psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
        if (val->intval == SEC_BATTERY_CABLE_USB||
			val->intval == SEC_BATTERY_CABLE_USB_CDP||
			val->intval == SEC_BATTERY_CABLE_TA)
			schedule_delayed_work(&charger->adapter_change_work, msecs_to_jiffies(50));
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		charger->aicl_curr = 0;
		charger->slow_charging = false;
		charger->input_current = max77729_get_input_current(charger);
		max77729_change_charge_path(charger, charger->cable_type);
		if (!max77729_get_autoibus(charger))
			max77729_set_fw_noautoibus(MAX77729_AUTOIBUS_AT_OFF);

		if (false) { // (is_nocharge_type(charger->cable_type)) {
			charger->wc_pre_current = WC_CURRENT_START;
			/* VCHGIN : REG=4.5V, UVLO=4.7V */
//modify by HTH-234718 at 2022/05/26 begin
			max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12,
			    0x00, CHG_CNFG_12_VCHGIN_REG_MASK);
//modify by HTH-234718 at 2022/05/26 end
			if (charger->pdata->enable_sysovlo_irq)
				max77729_set_sysovlo(charger, 1);

			/* Enable AICL IRQ */
			if (charger->irq_aicl_enabled == 0) {
				charger->irq_aicl_enabled = 1;
				enable_irq(charger->irq_aicl);
				max77729_read_reg(charger->i2c,
					MAX77729_CHG_REG_INT_MASK, &reg_data);
				/* pr_info("%s: enable aicl : 0x%x\n", */
					/* __func__, reg_data); */
			}
		} else if (is_wired_type(charger->cable_type)) {
			/* VCHGIN : REG=4.6V, UVLO=4.8V
			 * to fix CHGIN-UVLO issues including cheapy battery packs
			 */
//modify by HTH-234718 at 2022/05/26 begin
			max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12,
				0x00, CHG_CNFG_12_VCHGIN_REG_MASK);
//modify by HTH-234718 at 2022/05/26 end

			if(0) //delete
			if (is_hv_wire_type(charger->cable_type) ||
				(charger->cable_type == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT)) {
				/* Disable AICL IRQ */
				if (charger->irq_aicl_enabled == 1) {
					charger->irq_aicl_enabled = 0;
					disable_irq_nosync(charger->irq_aicl);
					cancel_delayed_work(&charger->aicl_work);
					__pm_relax(charger->aicl_ws);
					max77729_read_reg(charger->i2c,
						MAX77729_CHG_REG_INT_MASK, &reg_data);
					pr_info("%s: disable aicl : 0x%x\n",
						__func__, reg_data);
					charger->aicl_curr = 0;
					charger->slow_charging = false;
				}
			}
		}
		break;
		/* val->intval : input charging current */
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		{
			int input_current = val->intval;
			/* pr_info("%s: current max (%d)\n", __func__, val->intval); */
			/* if (delayed_work_pending(&charger->aicl_work)) { */
				/* cancel_delayed_work(&charger->aicl_work); */
				/* charger->aicl_curr = 0; */
				/* queue_delayed_work(charger->wqueue, &charger->aicl_work, */
					/* msecs_to_jiffies(AICL_WORK_DELAY)); */
			/* } */

			if (is_wireless_type(charger->cable_type))
				max77729_set_wireless_input_current(charger, input_current);
			else
				max77729_set_input_current(charger, input_current);

			if (is_nocharge_type(charger->cable_type))
				max77729_set_wireless_input_current(charger, input_current);
			charger->input_current = input_current;
		}
		break;
		/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		charger->charging_current = val->intval;
		max77729_set_charge_current(charger, val->intval);
		break;
		/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		charger->charging_current = val->intval;
		if (is_not_wireless_type(charger->cable_type))
			max77729_set_charge_current(charger, charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (charger->otg_on) {
			pr_info("%s: SKIP MODE (%d)\n", __func__, val->intval);
			max77729_set_skipmode(charger, val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		charger->float_voltage = val->intval;
		/* pr_info("%s: float voltage(%d)\n", __func__, val->intval); */
		max77729_set_float_voltage(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		max77729_enable_aicl_irq(charger);
		max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_OK, &reg_data);
		if (!(reg_data & MAX77729_AICL_OK))
			queue_delayed_work(charger->wqueue, &charger->aicl_work,
					   msecs_to_jiffies(AICL_WORK_DELAY));
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		charger->charge_mode =(val->intval ? SEC_BAT_CHG_MODE_CHARGING: SEC_BAT_CHG_MODE_CHARGING_OFF);
		charger->misalign_cnt = 0;
		/* max77729_chg_set_mode_state(charger, charger->charge_mode); */
		vote(charger->chgctrl_votable, "charger-enable", true, charger->charge_mode);
		break;

	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		max77729_set_topoff_current(charger, val->intval);
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_SURGE:
			if (val->intval) {
				pr_info("%s : Charger IC reset by surge. charger re-initialize\n",
					__func__);
				check_charger_unlock_state(charger);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHGINSEL:
			max77729_change_charge_path(charger, charger->cable_type);
			break;
		case POWER_SUPPLY_EXT_PROP_PAD_VOLT_CTRL:
			/* __pm_relax(charger->wc_current_ws); */
			/* cancel_delayed_work(&charger->wc_current_work); */
			max77729_set_input_current(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST:
#if defined(CONFIG_SUPPORT_SHIP_MODE)
			if (val->intval == SHIP_MODE_EN) {
				pr_info("%s: set ship mode enable\n", __func__);
				max77729_set_ship_mode(charger, 1);
			} else if (val->intval == SHIP_MODE_EN_OP) {
				pr_info("%s: set ship mode op enable\n", __func__);
				max77729_set_fw_ship_mode(1);
			} else {
				pr_info("%s: ship mode disable is not supported\n", __func__);
			}
#else
			pr_info("%s: ship mode(%d) is not supported\n", __func__, val->intval);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_AUTO_SHIPMODE_CONTROL:
			if (val->intval) {
				pr_info("%s: auto ship mode is enabled\n", __func__);
				max77729_set_auto_ship_mode(charger, 1);
			} else {
				pr_info("%s: auto ship mode is disabled\n", __func__);
				max77729_set_auto_ship_mode(charger, 0);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING:
			{
				u8 reg_data = 0, reg_fgsrc = 0;

				/* if jig attached, change the power source */
				/* from the VBATFG to the internal VSYS */
				if ((val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_VSYS) ||
					(val->intval == SEC_BAT_FGSRC_SWITCHING_VSYS))
					reg_fgsrc = 1;
				else
					reg_fgsrc = 0;

				max77729_update_reg(charger->i2c,
					MAX77729_CHG_REG_CNFG_07,
					(reg_fgsrc << CHG_CNFG_07_REG_FGSRC_SHIFT),
					CHG_CNFG_07_REG_FGSRC_MASK);
				max77729_read_reg(charger->i2c,
					MAX77729_CHG_REG_CNFG_07, &reg_data);

				pr_info("%s: POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING(%d): reg(0x%x) val(0x%x)\n",
					__func__, reg_fgsrc, MAX77729_CHG_REG_CNFG_07, reg_data);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			charger->charge_mode = val->intval;
			charger->misalign_cnt = 0;
			/* max77729_chg_set_mode_state(charger, charger->charge_mode); */
			vote(charger->chgctrl_votable, "charger-enable", true, charger->charge_mode);
			break;
#if defined(CONFIG_AFC_CHARGER_MODE)
		case POWER_SUPPLY_EXT_PROP_AFC_CHARGER_MODE:
			muic_hv_charger_init();
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL:
			max77729_set_otg(charger, val->intval);
			break;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
		case POWER_SUPPLY_EXT_PROP_POWER_DESIGN:
			max77729_charger_parse_dt(charger);
			break;
#endif
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77729_otg_get_property(struct power_supply *psy,
				     enum power_supply_property psp,
				     union power_supply_propval *val)
{
	struct max77729_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 reg_data;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mutex_lock(&charger->charger_mutex);
		val->intval = charger->otg_on;
		mutex_unlock(&charger->charger_mutex);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGE_UNO_CONTROL:
			max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00,
					  &reg_data);
			reg_data &= CHG_CNFG_00_MODE_MASK;
			if (reg_data == MAX77729_MODE_8_BOOST_UNO_ON ||
				reg_data == MAX77729_MODE_C_BUCK_BOOST_UNO_ON ||
				reg_data == MAX77729_MODE_D_BUCK_CHG_BOOST_UNO_ON)
				val->intval = 1;
			else
				val->intval = 0;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77729_otg_set_property(struct power_supply *psy,
				     enum power_supply_property psp,
				     const union power_supply_propval *val)
{
	struct max77729_charger_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	u8 reg_data;
	/* bool mfc_fw_update = false; */
	/* union power_supply_propval value = {0, }; */

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		/* psy_do_property("battery", get, */
			/* POWER_SUPPLY_EXT_PROP_MFC_FW_UPDATE, value); */
		/* mfc_fw_update = val->intval */
		/* if (!mfc_fw_update) { */
			max77729_set_otg(charger, val->intval);
			if (val->intval && (charger->irq_aicl_enabled == 1)) {
				charger->irq_aicl_enabled = 0;
				disable_irq_nosync(charger->irq_aicl);
				cancel_delayed_work(&charger->aicl_work);
				__pm_relax(charger->aicl_ws);
				max77729_read_reg(charger->i2c,
					MAX77729_CHG_REG_INT_MASK, &reg_data);
				/* pr_info("%s : disable aicl : 0x%x\n", */
					/* __func__, reg_data); */
				charger->aicl_curr = 0;
				charger->slow_charging = false;
			} else if (!val->intval && (charger->irq_aicl_enabled == 0)) {
				/* charger->irq_aicl_enabled = 1; */
				/* enable_irq(charger->irq_aicl); */
				/* max77729_read_reg(charger->i2c, */
					/* MAX77729_CHG_REG_INT_MASK, &reg_data); */
				/* pr_info("%s : enable aicl : 0x%x\n", */
					/* __func__, reg_data); */
			}
		/* } else { */
			/* pr_info("%s : max77729_set_otg skip, mfc_fw_update(%d)\n", */
				/* __func__, mfc_fw_update); */
		/* } */
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		/* pr_info("POWER_SUPPLY_PROP_VOLTAGE_MAX, set otg current limit %dmA\n", (val->intval) ? 1500 : 900); */

		if (val->intval) {
			/* otg current limit 1500mA */
			max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_02,
					MAX77729_OTG_ILIM_1500 << CHG_CNFG_02_OTG_ILIM_SHIFT,
					CHG_CNFG_02_OTG_ILIM_MASK);
		} else {
			/* otg current limit 900mA */
			max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_02,
					MAX77729_OTG_ILIM_900 << CHG_CNFG_02_OTG_ILIM_SHIFT,
					CHG_CNFG_02_OTG_ILIM_MASK);
		}
		break;
	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VOUT:
			max77729_set_uno_vout(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_WIRELESS_TX_IOUT:
			max77729_set_uno_iout(charger, val->intval);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGE_UNO_CONTROL:
			pr_info("%s: WCIN-UNO %d\n", __func__, val->intval);
			max77729_set_uno(charger, val->intval);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int max77729_usb_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct max77729_charger_data *charger = g_max77729_charger;
	int rc = 0;
	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		//val->intval = charger->pd_active ? 1 : 0;
		val->intval = charger->usb_online;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = charger->pd_active ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
		if(charger->pd_active)
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		else
			val->intval = charger->real_type;
		/* pr_debug("get REAL_TYPE %d\n", val->intval); */
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		val->intval = charger->pd_active;
		/* pr_debug("get PD_ACTIVE %d\n", charger->pd_active); */
		break;
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		if (g_usbc_data)
			val->intval = g_usbc_data->cc_pin_status;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:

		val->intval = POWER_SUPPLY_TYPEC_NONE;
		if (g_usbc_data) {
			switch(g_usbc_data->cc_data->ccistat) {
				case NOT_IN_UFP_MODE:
					val->intval = POWER_SUPPLY_TYPEC_SINK;
					break;
				case CCI_1_5A:
					val->intval = POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
					break;
				case CCI_3_0A:
					val->intval = POWER_SUPPLY_TYPEC_SOURCE_HIGH;
					break;
				case CCI_500mA:
					val->intval = POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
				default:
					break;
			}
			if (g_usbc_data->plug_attach_done){
				if (g_usbc_data->acc_type == 1){
					val->intval = POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER;
				}
			} else {
				val->intval = POWER_SUPPLY_TYPEC_NONE;
			}
		}

		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MIN:
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_PD_CURRENT_MAX:
		break;
	case POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED:
		break;
	case POWER_SUPPLY_PROP_PD_IN_HARD_RESET:
		break;
	default:
#if 0//defined(CONFIG_NOPMI_CHARGER)
		pr_err("Get prop %d is not supported in usb psy,but other charger needs it\n", psp);
		rc = 0;
#else
		/* pr_err("Get prop %d is not supported in usb psy\n", psp); */
		rc = -EINVAL;
#endif
		break;
	}

	if (rc < 0) {
		/* pr_debug("Couldn't get prop %d rc = %d\n", psp, rc); */
		return -ENODATA;
	}

	return rc;
}

int max77729_usb_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct max77729_charger_data *charger = g_max77729_charger;
 	union power_supply_propval value;

	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_REAL_TYPE:
		if (charger->real_type != val->intval){
			charger->real_type = val->intval;
			schedule_delayed_work(&charger->notify_work, msecs_to_jiffies(300));
			/* pr_debug("max77729_usb_set_property REAL_TYPE:%d\n", charger->real_type); */
		}
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		/* if (charger->pd_active != val->intval) { */
			charger->pd_active = val->intval;
			schedule_delayed_work(&charger->notify_work, msecs_to_jiffies(300));
			pr_debug("max77729_usb_set_property PD_ACTIVE:%d\n", charger->pd_active);
			if (charger->pd_active == 2){
				value.intval = 1;
			} else {
				value.intval = 0;
			}
			max77729_set_fast_charge_mode(charger, charger->pd_active);
		/* } */
   	break;
	case POWER_SUPPLY_PROP_PRESENT:
		charger->usb_online =  val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->usb_online =  val->intval;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MIN:
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_PD_CURRENT_MAX:
		break;
	case POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED:
		break;
	case POWER_SUPPLY_PROP_PD_IN_HARD_RESET:
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		break;
	default:
#if 0//defined(CONFIG_NOPMI_CHARGER)
		pr_err("Set prop %d is not supported in usb psy,but other charger needs it\n", psp);
		rc = 0;
#else
		/* pr_err("Set prop %d is not supported in usb psy\n", psp); */
		rc = -EINVAL;
#endif
		break;
	}

	return rc;
}

int otg_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	default:
		break;
	}
	return 0;
}

int usb_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_PD_ACTIVE:
	case POWER_SUPPLY_PROP_REAL_TYPE:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_MTBF_CUR:
		return 1;
	default:
		break;
	}
	return 0;
}


int max77729_batt_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	struct max77729_charger_data *charger = g_max77729_charger;
	int rc = 0;
	union power_supply_propval val ={0, };

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		pval->intval = 1;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		pval->intval = max77729_get_charger_state(charger);
		if (get_effective_result_locked(charger->fv_votable) < 4450) {
			if (pval->intval == POWER_SUPPLY_STATUS_FULL) {
				pval->intval = POWER_SUPPLY_STATUS_CHARGING;
			}
		} else if (get_client_vote_locked(charger->usb_icl_votable, MAIN_CHG_ENABLE_VOTER) == MAIN_ICL_MIN) {
			pval->intval = POWER_SUPPLY_STATUS_CHARGING;
		}
		if (((pval->intval == POWER_SUPPLY_STATUS_DISCHARGING) ||
			(pval->intval == POWER_SUPPLY_STATUS_NOT_CHARGING)) &&
			(charger->real_type > 0))
			pval->intval = POWER_SUPPLY_STATUS_CHARGING;

		if(!charger->psy_bms)
                charger->psy_bms = power_supply_get_by_name("bms");
                if (charger->psy_bms) {
                        rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_VOLTAGE_AVG, &val);
                        if (rc < 0) {
                                val.intval = 3800;
                                pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_AVG fail\n", __func__);
                        }
                }
                //pr_err("%s vbat=%d\n", __func__, val.intval);
                if (val.intval < 3300) {
			pval->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			if (!charger->psy_batt)
				charger->psy_batt = power_supply_get_by_name("battery");
			if (charger->psy_batt)
				power_supply_changed(charger->psy_batt);
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_SOC_DECIMAL:
	case POWER_SUPPLY_PROP_SOC_DECIMAL_RATE:
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		if (!charger->psy_bms)
			charger->psy_bms = power_supply_get_by_name("bms");
		if(charger->psy_bms)
			rc = power_supply_get_property(charger->psy_bms, psp, pval);
		break;
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		pval->intval =!(get_client_vote_locked(charger->usb_icl_votable, MAIN_CHG_ENABLE_VOTER) == MAIN_ICL_MIN);
		break;
 	case POWER_SUPPLY_PROP_HEALTH:
 		pval->intval = max77729_get_charging_health(charger);
		max77729_check_cnfg12_reg(charger);
		break;
	case POWER_SUPPLY_PROP_SHUTDOWN_DELAY:
		pval->intval = charger->shutdown_delay;
		break;
 	case POWER_SUPPLY_PROP_CHARGE_TYPE:
 		if (!charger->is_charging) {
			pval->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		} else if (charger->slow_charging) {
			pval->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			pr_info("%s: slow-charging mode\n", __func__);
		} else {
			pval->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		pval->intval = (get_client_vote_locked(charger->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER) == 0);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		pval->intval = charger->charging_current;
		break;
	  /* case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL: */
		/* val->intval = charger->system_temp_level; */
		/* break; */
	 /* case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT: */
		/* val->intval = charger->system_temp_level; */
		/* break; */
#if 0
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		break;
#endif
////////////////
	default:
#if 0//defined(CONFIG_NOPMI_CHARGER)
		pr_err("Get prop %d is not supported in battery psy,but other charger needs it\n", psp);
		rc = 0;
#else
		/* pr_err("Get prop %d is not supported in battery psy\n", psp); */
		rc = -EINVAL;
#endif
	}

	if (rc < 0) {
		/* pr_debug("Couldn't get prop %d rc = %d\n", psp, rc); */
		return -ENODATA;
	}
	return rc;
}
#if 0
static int max77729_set_prop_system_temp_level(struct max77729_charger_data *chg,
				const union power_supply_propval *val)
{
	int  rc = 0;

	if (val->intval < 0 ||
		chg->thermal_levels <=0 ||
		val->intval > chg->thermal_levels)
		return -EINVAL;

	if (val->intval == chg->system_temp_level)
		return rc;

	chg->system_temp_level = val->intval;
	if (chg->system_temp_level == 0) {
		rc = vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, false, 0);
	} else if (chg->system_temp_level == chg->thermal_levels) { // thermal temp level
		rc = vote(chg->usb_icl_votable, THERMAL_DAEMON_VOTER, (bool)val->intval, 0);
	} else {
		rc = vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, true,
			chg->thermal_mitigation[chg->system_temp_level]);
	}

	return rc;
}

#endif

int max77729_batt_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct max77729_charger_data *charger = g_max77729_charger;
	static int last_shutdown_delay;
	int rc = 0;

	switch (prop) {
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		max77729_charger_unlock(charger);
		vote(charger->usb_icl_votable, MAIN_CHG_ENABLE_VOTER, !val->intval, MAIN_ICL_MIN);
		vote(charger->mainfcc_votable, MAIN_CHG_ENABLE_VOTER, !val->intval, 200);
		if (val->intval){
			max77729_set_topoff_current(charger, 500);
			max77729_set_topoff_time(charger, 1);
		} else {
			max77729_set_topoff_time(charger, 30);
			max77729_set_topoff_current(charger, 150);
		}
		/* charger->charge_mode = val->intval ? SEC_BAT_CHG_MODE_CHARGING : SEC_BAT_CHG_MODE_CHARGING_OFF; */
		/* charger->misalign_cnt = 0; */
		/* max77729_chg_set_mode_state(charger, charger->charge_mode); */
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		pr_err("%s: Set input suspend prop, value:%d\n",__func__,val->intval);
		vote(charger->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER, !!val->intval, 0);
		if (val->intval) {
			vote(charger->chgctrl_votable, MAIN_CHG_SUSPEND_VOTER, true, SEC_BAT_CHG_MODE_BUCK_OFF);
			/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_BUCK_OFF); */
		} else {
			vote(charger->chgctrl_votable, MAIN_CHG_SUSPEND_VOTER, false, SEC_BAT_CHG_MODE_BUCK_OFF);
			/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_CHARGING); */
//modify by HTH-234718 at 2022/05/26 begin
			vote(charger->usb_icl_votable, MAIN_CHG_AICL_VOTER, false, 0);//cancel  aicl when buck open agoin
//modify by HTH-234718 at 2022/05/26 end
		}
		/* power_supply_changed(charger->psy_batt); */
		break;
	case POWER_SUPPLY_PROP_SHUTDOWN_DELAY:
		if (last_shutdown_delay != val->intval) {
			charger->shutdown_delay = val->intval;
			if (!charger->psy_batt)
				charger->psy_batt = power_supply_get_by_name("battery");
			if (charger->psy_batt)
				power_supply_changed(charger->psy_batt);
		}
		last_shutdown_delay = charger->shutdown_delay;
		break;
	/* case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL: */
		/* rc = max77729_set_prop_system_temp_level(charger, val); */
		/* break; */
	/* case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT: */
		/* rc = max77729_set_prop_system_temp_level(charger, val); */
		/* break; */
#if 0
	case POWER_SUPPLY_PROP_STATUS:
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		break;
#endif
	default:
#if 0//defined(CONFIG_NOPMI_CHARGER)
		pr_err("Set prop %d is not supported in battery psy,but other charger needs it\n", prop);
		rc = 0;
#else
		/* pr_err("Set prop %d is not supported in battery psy\n", prop); */
		rc = -EINVAL;
#endif
	}

	return rc;
}

int batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		return 1;
	default:
		break;
	}

	return 0;
}
static int max77729_debugfs_show(struct seq_file *s, void *data)
{
	struct max77729_charger_data *charger = s->private;
	u8 reg, reg_data;

	seq_puts(s, "MAX77729 CHARGER IC :\n");
	seq_puts(s, "===================\n");
	for (reg = 0xB0; reg <= 0xC3; reg++) {
		max77729_read_reg(charger->i2c, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_puts(s, "\n");
	return 0;
}

static int max77729_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, max77729_debugfs_show, inode->i_private);
}

static const struct file_operations max77729_debugfs_fops = {
	.open = max77729_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void max77729_chg_isr_work(struct work_struct *work)
{
	struct max77729_charger_data *charger =
	    container_of(work, struct max77729_charger_data, isr_work.work);

	max77729_get_charger_state(charger);
	max77729_get_charging_health(charger);
}

static irqreturn_t max77729_chg_irq_thread(int irq, void *irq_data)
{
	struct max77729_charger_data *charger = irq_data;

	pr_info("%s: Charger interrupt occurred\n", __func__);

	if ((charger->pdata->full_check_type == SEC_BATTERY_FULLCHARGED_CHGINT)
		|| (charger->pdata->ovp_uvlo_check_type == SEC_BATTERY_OVP_UVLO_CHGINT))
		schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}

static irqreturn_t max77729_batp_irq(int irq, void *data)
{
	struct max77729_charger_data *charger = data;
	union power_supply_propval value;
	u8 reg_data;

	pr_info("%s : irq(%d)\n", __func__, irq);

	check_charger_unlock_state(charger);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_OK, &reg_data);

	if (!(reg_data & MAX77729_BATP_OK))
		psy_do_property("battery", set, POWER_SUPPLY_PROP_PRESENT, value);

	return IRQ_HANDLED;
}

#if defined(CONFIG_MAX77729_CHECK_B2SOVRC)
#if defined(CONFIG_REGULATOR_S2MPS18)
extern void s2mps18_print_adc_val_power(void);
#endif
static irqreturn_t max77729_bat_irq(int irq, void *data)
{
	struct max77729_charger_data *charger = data;
	union power_supply_propval value;
	u8 reg_int_ok, reg_data;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_OK, &reg_int_ok);
	if (!(reg_int_ok & MAX77729_BAT_OK)) {
		max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_01, &reg_data);
		reg_data = ((reg_data & MAX77729_BAT_DTLS) >> MAX77729_BAT_DTLS_SHIFT);
		if (reg_data == 0x06) {
			pr_info("OCP(B2SOVRC)\n");

			if (charger->uno_on) {
#if defined(CONFIG_WIRELESS_TX_MODE)
				union power_supply_propval val;
				val.intval = BATT_TX_EVENT_WIRELESS_TX_OCP;
				psy_do_property("wireless", set,
					POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR, val);
#endif
			}
#if defined(CONFIG_REGULATOR_S2MPS18)
			s2mps18_print_adc_val_power();
#endif
			/* print vnow, inow */
			psy_do_property("bms", get,
				POWER_SUPPLY_EXT_PROP_MONITOR_WORK, value);
		}
	} else {
		max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_01, &reg_data);
		reg_data = ((reg_data & MAX77729_BAT_DTLS) >> MAX77729_BAT_DTLS_SHIFT);
		pr_info("%s: reg_data(0x%x)\n", __func__, reg_data);
	}
	check_charger_unlock_state(charger);

	return IRQ_HANDLED;
}
#endif

static irqreturn_t max77729_bypass_irq(int irq, void *data)
{
	struct max77729_charger_data *charger = data;
#ifdef CONFIG_USB_HOST_NOTIFY
	struct otg_notify *o_notify;
#endif
	union power_supply_propval val;
	u8 dtls_02, byp_dtls;

	pr_info("%s: irq(%d)\n", __func__, irq);

	/* check and unlock */
	check_charger_unlock_state(charger);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_02, &dtls_02);

	byp_dtls = ((dtls_02 & MAX77729_BYP_DTLS) >> MAX77729_BYP_DTLS_SHIFT);
	pr_info("%s: BYP_DTLS(0x%02x)\n", __func__, byp_dtls);

	if (byp_dtls & 0x1) {
		pr_info("%s: bypass overcurrent limit\n", __func__);
		/* disable the register values just related to OTG and
		 * keep the values about the charging
		 */
		if (charger->otg_on) {
#ifdef CONFIG_USB_HOST_NOTIFY
			o_notify = get_otg_notify();
			if (o_notify)
				send_otg_notify(o_notify, NOTIFY_EVENT_OVERCURRENT, 0);
#endif
			val.intval = 0;
			psy_do_property("otg", set, POWER_SUPPLY_PROP_ONLINE, val);
		} else if (charger->uno_on) {
#if defined(CONFIG_WIRELESS_TX_MODE)
			val.intval = BATT_TX_EVENT_WIRELESS_TX_OCP;
			psy_do_property("wireless", set, POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR, val);
#endif
		}
	}
	return IRQ_HANDLED;
}

static void max77729_aicl_isr_work(struct work_struct *work)
{
	struct max77729_charger_data *charger =
		container_of(work, struct max77729_charger_data, aicl_work.work);
	/* union power_supply_propval value; */
	bool aicl_mode = false;
	u8 aicl_state = 0, reg_data;
	int aicl_current = 0;

	if (!charger->irq_aicl_enabled || charger->real_type == POWER_SUPPLY_TYPE_USB_HVDCP){ //||
			/* (charger->real_type != POWER_SUPPLY_TYPE_USB_DCP && */
			/* charger->real_type != POWER_SUPPLY_TYPE_USB && */
			/* charger->real_type != POWER_SUPPLY_TYPE_USB_FLOAT && */
			/* charger->real_type != POWER_SUPPLY_TYPE_USB_CDP)){ // || is_nocharge_type(charger->cable_type)) { */
		charger->aicl_curr = 0;
		__pm_relax(charger->aicl_ws);
		return;
	}

	__pm_stay_awake(charger->aicl_ws);
	/* mutex_lock(&charger->charger_mutex); */
	/* check and unlock */
	check_charger_unlock_state(charger);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_OK, &aicl_state);

	if (!(aicl_state & MAX77729_AICL_OK)) {
		reduce_input_current(charger, REDUCE_CURRENT_STEP);

		if (is_not_wireless_type(charger->cable_type))
			max77729_check_slow_charging(charger, charger->input_current);

		if ((charger->irq_aicl_enabled == 1) && (charger->slow_charging) &&
			(charger->input_current <= MINIMUM_INPUT_CURRENT)) {
			charger->irq_aicl_enabled = 0;
			disable_irq_nosync(charger->irq_aicl);
			max77729_read_reg(charger->i2c,
					MAX77729_CHG_REG_INT_MASK, &reg_data);
			aicl_current = MINIMUM_INPUT_CURRENT;
		} else {
			aicl_mode = true;
			queue_delayed_work(charger->wqueue, &charger->aicl_work,
				msecs_to_jiffies(AICL_WORK_DELAY));
		}
	} else {
		if (charger->aicl_curr) {
			aicl_current = charger->aicl_curr;
		}
	}

	/* mutex_unlock(&charger->charger_mutex); */
	if (!aicl_mode)
		__pm_relax(charger->aicl_ws);

	/* if (aicl_current) { */
		/* value.intval = aicl_current; */
		/* psy_do_property("battery", set, */
			/* POWER_SUPPLY_EXT_PROP_AICL_CURRENT, value); */
	/* } */
}

static irqreturn_t max77729_aicl_irq(int irq, void *data)
{
	struct max77729_charger_data *charger = data;

	__pm_stay_awake(charger->aicl_ws);
	queue_delayed_work(charger->wqueue, &charger->aicl_work,
		msecs_to_jiffies(AICL_WORK_DELAY));

	/* pr_info("%s: irq(%d)\n", __func__, irq); */
	/* __pm_relax(charger->wc_current_ws); */
	/* cancel_delayed_work(&charger->wc_current_work); */

	return IRQ_HANDLED;
}

static void max77729_enable_aicl_irq(struct max77729_charger_data *charger)
{
	int ret;

	ret = request_threaded_irq(charger->irq_aicl, NULL,
					max77729_aicl_irq, 0,
					"aicl-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request aicl IRQ: %d: %d\n",
		       __func__, charger->irq_aicl, ret);
		charger->irq_aicl_enabled = -1;
	} else {
		charger->irq_aicl_enabled = 0;
	}
	/* pr_info("%s: enabled(%d)\n", __func__, charger->irq_aicl_enabled); */
}

static void max77729_redet_work(struct work_struct *work)
{
	struct max77729_charger_data *charger =
		container_of(work, struct max77729_charger_data, redet_work.work);

	if (!charger->otg_on) {
	   if(charger->real_type == POWER_SUPPLY_TYPE_USB_FLOAT || charger->real_type == POWER_SUPPLY_TYPE_UNKNOWN){
		   max77729_rerun_chgdet(g_usbc_data);   //trigger the next detecion routine
		   pr_debug("%s: trigger the next detect\n", __func__);
		   schedule_delayed_work(&charger->adapter_change_work, msecs_to_jiffies(1000));
	   }
	}
}
static void max77729_chgin_isr_work(struct work_struct *work)
{
	struct max77729_charger_data *charger =
		container_of(work, struct max77729_charger_data, chgin_work.work);
	union power_supply_propval value= {0, };
	u8 chgin_dtls, chg_dtls, chg_cnfg_00, intok;

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_00, &chgin_dtls);
	chgin_dtls =
		((chgin_dtls & MAX77729_CHGIN_DTLS) >> MAX77729_CHGIN_DTLS_SHIFT);

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_01, &chg_dtls);
	chg_dtls = ((chg_dtls & MAX77729_CHG_DTLS) >> MAX77729_CHG_DTLS_SHIFT);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00, &chg_cnfg_00);
	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_OK, &intok);

 	if (!charger->psy_usb) {
		charger->psy_usb = power_supply_get_by_name("usb");
		if(!charger->psy_usb)
		{
			pr_err("%s get usb psy fail\n", __func__);
		}
	}
	if(!g_usbc_data || !g_usbc_data->muic_data || !g_usbc_data->cc_data)   {
		pr_err("%s usbc is not ready ignore this plugin\n", __func__);
	}

	if(charger->psy_usb) {
		max77729_read_reg(charger->muic, REG_BC_STATUS, &g_usbc_data->muic_data->bc_status);
		g_usbc_data->muic_data->vbusdet  =
			((g_usbc_data->muic_data->bc_status & BIT_VBUSDet) == BIT_VBUSDet) ? 1 : 0;

        if (g_usbc_data->muic_data->vbusdet){
			pr_debug("max77729 vbus detect!\n");
			schedule_delayed_work(&charger->adapter_change_work, msecs_to_jiffies(800));
			max77729_charger_unlock(charger);
  			max77729_set_topoff_current(charger, 250);
			max77729_set_topoff_time(charger, 1);
			max77729_chg_set_wdtmr_en(charger, 1);
			max77729_chg_set_wdtmr_kick(charger);
        } else {
			pr_debug("max77729 adapter plugout! \n");

			stop_usb_peripheral(g_usbc_data);
 			cancel_delayed_work(&charger->period_work);
			cancel_delayed_work(&charger->batt_notify_work);
			cancel_delayed_work_sync(&charger->adapter_change_work);
			cancel_delayed_work_sync(&charger->notify_work);
			vote(charger->usb_icl_votable, MAX77729_CHG_VOTER, true, 0);
			vote(charger->fcc_votable, MAX77729_CHG_VOTER, true, 0);
			vote(charger->chgctrl_votable, "charger-enable", false, charger->charge_mode);
			vote(charger->chgctrl_votable, MAIN_CHG_ENABLE_VOTER, false, charger->charge_mode);
			vote(charger->chgctrl_votable, "JEITA_CHG_VOTER", false, 0);
			charger->batt_notify_count = 0;

			charger->real_type = POWER_SUPPLY_TYPE_UNKNOWN;
			value.intval = POWER_SUPPLY_TYPE_UNKNOWN;
 			psy_do_property("usb", set, POWER_SUPPLY_PROP_REAL_TYPE, value);

			value.intval = SEC_BATTERY_CABLE_NONE;
			psy_do_property("bbc", set, POWER_SUPPLY_PROP_ONLINE, value);
			psy_do_property("bms", set, POWER_SUPPLY_PROP_ONLINE, value);
			value.intval = 0;
			psy_do_property("usb", set, POWER_SUPPLY_PROP_ONLINE, value);

			if (charger->irq_aicl_enabled) {
				charger->irq_aicl_enabled = 0;
				disable_irq_nosync(charger->irq_aicl);
				cancel_delayed_work(&charger->aicl_work);
				__pm_relax(charger->aicl_ws);
				charger->aicl_curr = 0;
				charger->slow_charging = false;
			}
			vote(charger->usb_icl_votable, MAIN_CHG_AICL_VOTER, false, 0);

             /* schedule_delayed_work(&charger->notify_work, msecs_to_jiffies(300)); */
			power_supply_changed(charger->psy_usb);
			if (!charger->psy_batt)
				charger->psy_batt = power_supply_get_by_name("battery");
			power_supply_changed(charger->psy_batt);

			max77729_chg_set_wdtmr_en(charger, 0);
		}
	} else {
		queue_delayed_work(charger->wqueue, &charger->chgin_work,
				msecs_to_jiffies(2000));
	}
	/* pr_err("%s: health(%d), chgin_dtls: 0x%x, chg_dtsl: 0x%x, chg_cnfg_00: 0x%x, intok: 0x%x\n", */
			/* __func__, value.intval, chgin_dtls, chg_dtls, chg_cnfg_00, intok); */

	/* enable_irq(charger->irq_aicl); */
	__pm_relax(charger->chgin_ws);
}

static irqreturn_t max77729_chgin_irq(int irq, void *data)
{
	struct max77729_charger_data *charger = data;

	__pm_stay_awake(charger->chgin_ws);
	/* disable_irq_nosync(charger->irq_aicl); */

	cancel_delayed_work(&charger->chgin_work);
	queue_delayed_work(charger->wqueue, &charger->chgin_work,
			msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

static void max77729_batt_notify_work(struct work_struct *work)
{
	struct max77729_charger_data *charger =
		container_of(work, struct max77729_charger_data, batt_notify_work.work);
	union power_supply_propval pval ={0,};
	int usb_online = 0;

	if (!charger->psy_usb) {
		charger->psy_usb = power_supply_get_by_name("usb");
		if(!charger->psy_usb)
		{
			pr_err("%s get usb psy fail\n", __func__);
		}
	}
	if (charger->psy_usb) {
		power_supply_get_property(charger->psy_usb, POWER_SUPPLY_PROP_ONLINE, &pval);
		usb_online = pval.intval;
	}
	if (usb_online) {
		pr_err("%s usb_online is ok now\n", __func__);
		power_supply_changed(charger->psy_usb);
		msleep(500);
		if (!charger->psy_batt)
			charger->psy_batt = power_supply_get_by_name("battery");
		if (charger->psy_batt)
			power_supply_changed(charger->psy_batt);
	}
	if (charger->batt_notify_count++ < 5)
		schedule_delayed_work(&charger->batt_notify_work, msecs_to_jiffies(1000));
	else
		charger->batt_notify_count = 5;
}

static void max77729_notify_work(struct work_struct *work)
{
	struct max77729_charger_data *charger =
		container_of(work, struct max77729_charger_data, notify_work.work);
	union power_supply_propval pval ={0,};
	int input_curr_limit = 0, fastchg_curr = 6000, mtbf_cur = 0, i;

 	if (!charger->psy_usb) {
		charger->psy_usb = power_supply_get_by_name("usb");
		if(!charger->psy_usb)
		{
			pr_err("%s get usb psy fail\n", __func__);
		}
	}
	if (charger->psy_usb) {
		power_supply_get_property(charger->psy_usb, POWER_SUPPLY_PROP_ONLINE, &pval);
		if (charger->usb_online != pval.intval){
			pval.intval = charger->usb_online;
			power_supply_set_property(charger->psy_usb, POWER_SUPPLY_PROP_ONLINE, &pval);
		}

 		switch(charger->real_type) {
			case POWER_SUPPLY_TYPE_USB_PD:
				input_curr_limit = 3000;
				break;
			case POWER_SUPPLY_TYPE_USB:
				input_curr_limit = 500;
				if (g_usbc_data && g_usbc_data->muic_data) {
					if (g_usbc_data->muic_data->pr_chg_type == 4){
						charger->real_type = POWER_SUPPLY_TYPE_USB_DCP;
						fastchg_curr = 2000;
						input_curr_limit = 2000;
					} else if (g_usbc_data->muic_data->pr_chg_type == 2){
						charger->real_type = POWER_SUPPLY_TYPE_USB_DCP;
					}
				}

				break;
			case POWER_SUPPLY_TYPE_USB_CDP:
				input_curr_limit = 1500;
				if (g_usbc_data && g_usbc_data->muic_data) {
					if (g_usbc_data->muic_data->pr_chg_type == 3){
						input_curr_limit = 1000;
						charger->real_type = POWER_SUPPLY_TYPE_USB_DCP;
					}
				}
				break;
			case POWER_SUPPLY_TYPE_USB_DCP:
				fastchg_curr = 2000;
			case POWER_SUPPLY_TYPE_USB_HVDCP:
				input_curr_limit = 2000;
				break;
			case POWER_SUPPLY_TYPE_USB_FLOAT:
				input_curr_limit = 1000;//only for factory test,it shouble be 1000
				break;
			case POWER_SUPPLY_TYPE_UNKNOWN:
				if (!charger->otg_on) {
					input_curr_limit = 300;
					break;
				}
			default:
				power_supply_changed(charger->psy_usb);
				return;
		}
 		if (charger->pd_active == 1) {
			charger->real_type = POWER_SUPPLY_TYPE_USB_PD;
			if (g_usbc_data->pd_data->pdo_list) {
				if (g_usbc_data->pd_data->pd_noti.sink_status.available_pdo_num > 0) {
 					if (g_usbc_data->pd_data->pd_noti.sink_status.available_pdo_num > 1){
						input_curr_limit = g_usbc_data->pd_data->pd_noti.sink_status.power_list[2].max_current;
						max77729_select_pdo(2);
						if (charger->irq_aicl_enabled) {
							charger->irq_aicl_enabled = 0;
							disable_irq_nosync(charger->irq_aicl);
							cancel_delayed_work(&charger->aicl_work);
							__pm_relax(charger->aicl_ws);
							charger->aicl_curr = 0;
							charger->slow_charging = false;
						}

						/* vote(charger->usb_icl_votable, MAIN_CHG_AICL_VOTER, false, 0); */
					}else {
						input_curr_limit = g_usbc_data->pd_data->pd_noti.sink_status.power_list[1].max_current;
						max77729_select_pdo(1);
						charger->real_type = POWER_SUPPLY_TYPE_USB_DCP;
					}
				}
			}
			fastchg_curr  = 3000;
		} else if (charger->pd_active == 2) {
			int selected_fixed_pdo = 0;
			POWER_LIST* pPower_list = NULL;
			input_curr_limit = 2000;
			for (i = 0; i < g_usbc_data->pd_data->pd_noti.sink_status.available_pdo_num; ++i) {
				pPower_list = &g_usbc_data->pd_data->pd_noti.sink_status.power_list[i + 1];
				if (pPower_list->apdo && pPower_list->max_voltage > 10000 && g_usbc_data->pd_data->pd_noti.sink_status.selected_pdo_num == 0){
					if (pPower_list->max_current > 2000) {
						input_curr_limit = pPower_list->max_current;
						max77729_select_pps(i+1, 6000, pPower_list->max_current);
						charger->real_type = POWER_SUPPLY_TYPE_USB_PD;
						if (charger->irq_aicl_enabled) {
							charger->irq_aicl_enabled = 0;
							disable_irq_nosync(charger->irq_aicl);
							cancel_delayed_work(&charger->aicl_work);
							__pm_relax(charger->aicl_ws);
							charger->aicl_curr = 0;
							charger->slow_charging = false;
						}
						selected_fixed_pdo = 0;
						break;
					} else if (selected_fixed_pdo){
						max77729_select_pdo(selected_fixed_pdo);
						if (g_usbc_data->pd_data->pd_noti.sink_status.power_list[selected_fixed_pdo].max_voltage > 6000){
							charger->real_type = POWER_SUPPLY_TYPE_USB_PD;
							if (charger->irq_aicl_enabled) {
								charger->irq_aicl_enabled = 0;
								disable_irq_nosync(charger->irq_aicl);
								cancel_delayed_work(&charger->aicl_work);
								__pm_relax(charger->aicl_ws);
								charger->aicl_curr = 0;
								charger->slow_charging = false;
							}
						} else {
							charger->real_type = POWER_SUPPLY_TYPE_USB_DCP;
						}
						selected_fixed_pdo = 0;
						break;
					}
				} else if (!pPower_list->apdo) {
					if (pPower_list->max_voltage <= 9000) {
						input_curr_limit = pPower_list->max_current;
						selected_fixed_pdo = (i+1);
					}
				}
			}

			if (selected_fixed_pdo){
				if (g_usbc_data->pd_data->pd_noti.sink_status.power_list[selected_fixed_pdo].max_voltage > 6000){
					charger->real_type = POWER_SUPPLY_TYPE_USB_PD;
					if (charger->irq_aicl_enabled) {
						charger->irq_aicl_enabled = 0;
						disable_irq_nosync(charger->irq_aicl);
						cancel_delayed_work(&charger->aicl_work);
						__pm_relax(charger->aicl_ws);
						charger->aicl_curr = 0;
						charger->slow_charging = false;
					}
				} else {
					charger->real_type = POWER_SUPPLY_TYPE_USB_DCP;
				}
			}
			fastchg_curr  = 6000;
			/* vote(charger->usb_icl_votable, MAIN_CHG_AICL_VOTER, false, 0); */
		}

		power_supply_get_property(charger->psy_usb, POWER_SUPPLY_PROP_MTBF_CUR, &pval);
		mtbf_cur = pval.intval;
		if (input_curr_limit <= 1000 && mtbf_cur >= 1500){
			fastchg_curr = 1500;
			input_curr_limit = mtbf_cur;
		}

		/* pr_err("max77729 override current cfg. input[%d] charging[%d] mtbf[%d]\n", */
				/* input_curr_limit, fastchg_curr, mtbf_cur); */
		vote(charger->usb_icl_votable, MAX77729_CHG_VOTER, true, input_curr_limit);
		vote(charger->usb_icl_votable, MAIN_CHG_AICL_VOTER, true,input_curr_limit);
		vote(charger->fcc_votable, MAX77729_CHG_VOTER, true, fastchg_curr);

		power_supply_changed(charger->psy_usb);

		//delay batt_psy uevent update
		//if (!charger->psy_batt)
		//	charger->psy_batt = power_supply_get_by_name("battery");
		//power_supply_changed(charger->psy_batt);
	}
}


static void max77729_adapter_changed_work(struct work_struct *work)
{
	struct max77729_charger_data *charger =
		container_of(work, struct max77729_charger_data, adapter_change_work.work);
	union power_supply_propval value ={0,};
	union power_supply_propval pvalue ={0,};
	enum power_supply_type real_charger_type;
	static int pre_pd_active = 0;
	bool post_usb_state = false;
	u8 chgin_dtls, dcdtmo, ccstat;

 	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_DETAILS_00, &chgin_dtls);
	chgin_dtls =
		((chgin_dtls & MAX77729_CHGIN_DTLS) >> MAX77729_CHGIN_DTLS_SHIFT);

	if (!charger->psy_usb) {
		charger->psy_usb = power_supply_get_by_name("usb");
		if(!charger->psy_usb)
		{
			pr_err("%s get usb psy fail\n", __func__);
		}
	}
	if(!g_usbc_data || !g_usbc_data->muic_data || !g_usbc_data->cc_data)   {
		pr_err("%s usbc is not ready ignore this plugin\n", __func__);
	}

	/* if(charger->psy_usb) { */

		max77729_read_reg(charger->muic, REG_CC_STATUS0, &g_usbc_data->cc_data->cc_status0);
		max77729_read_reg(charger->muic, REG_BC_STATUS, &g_usbc_data->muic_data->bc_status);
		g_usbc_data->muic_data->chg_type = (g_usbc_data->muic_data->bc_status & BIT_ChgTyp)
		>> FFS(BIT_ChgTyp);
		g_usbc_data->muic_data->pr_chg_type = (g_usbc_data->muic_data->bc_status & BIT_PrChgTyp)
			>> FFS(BIT_PrChgTyp);


		dcdtmo = (g_usbc_data->muic_data->bc_status & BIT_DCDTmo)
			>> FFS(BIT_DCDTmo);
		g_usbc_data->muic_data->vbusdet  =
			((g_usbc_data->muic_data->bc_status & BIT_VBUSDet) == BIT_VBUSDet) ? 1 : 0;
		ccstat =  (g_usbc_data->cc_data->cc_status0 & BIT_CCStat) >> FFS(BIT_CCStat);

		switch(g_usbc_data->muic_data->chg_type) {
			case CHGTYP_NOTHING:
				value.intval = SEC_BATTERY_CABLE_NONE;
				real_charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
				if (g_usbc_data->muic_data->vbusdet && !g_usbc_data->plug_attach_done) {
					value.intval = SEC_BATTERY_CABLE_USB;
					real_charger_type = POWER_SUPPLY_TYPE_USB;
					com_to_usb_ap(g_usbc_data->muic_data);
					start_usb_peripheral(g_usbc_data);
				} else
				if (g_usbc_data->muic_data->vbusdet == 1 && dcdtmo){
					pr_err("%s float detect0... \n", __func__);
					real_charger_type = POWER_SUPPLY_TYPE_USB_FLOAT;
				}
				break;
			case CHGTYP_CDP_T:
				value.intval = SEC_BATTERY_CABLE_USB_CDP;
				real_charger_type = POWER_SUPPLY_TYPE_USB_CDP;
				com_to_usb_ap(g_usbc_data->muic_data);
				start_usb_peripheral(g_usbc_data);
				break;
			case CHGTYP_USB_SDP:
				value.intval = SEC_BATTERY_CABLE_USB;
				real_charger_type = POWER_SUPPLY_TYPE_USB;
                 /* com_to_usb_ap(g_usbc_data->muic_data); */
				/* start_usb_peripheral(g_usbc_data); */
				if (g_usbc_data->muic_data->vbusdet == 1 && dcdtmo){
					pr_err("%s float detect1... \n", __func__);
					value.intval = SEC_BATTERY_CABLE_TA;
					real_charger_type = POWER_SUPPLY_TYPE_USB_FLOAT;

					schedule_delayed_work(&charger->redet_work, msecs_to_jiffies(5000));
				}else {
					/* if (dcdtmo){ */
						/* g_usbc_data->muic_data->dcdtmo++; */
						/* schedule_delayed_work(&charger->adapter_change_work, msecs_to_jiffies(500)); */
					/* } else { */
						/* g_usbc_data->muic_data->dcdtmo = 0; */
						/* value.intval = SEC_BATTERY_CABLE_USB; */
						/* real_charger_type = POWER_SUPPLY_TYPE_USB; */
						com_to_usb_ap(g_usbc_data->muic_data);
						start_usb_peripheral(g_usbc_data);
					/* } */
				}
				break;
			case CHGTYP_DCP:
				value.intval = SEC_BATTERY_CABLE_TA;
				real_charger_type = POWER_SUPPLY_TYPE_USB_DCP;
				if (g_usbc_data->is_hvdcp)
					real_charger_type = POWER_SUPPLY_TYPE_USB_HVDCP;
				break;
		}
		if (ccstat == cc_SOURCE) {
			if (charger->otg_on) {
				value.intval = SEC_BATTERY_CABLE_OTG;
				real_charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
			}else {
				value.intval = 1;
				psy_do_property("bbc", set, POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL, value);
				pr_err("adapter error power role is mismatch. \n");
				value.intval = SEC_BATTERY_CABLE_OTG;
			}
			if (charger->irq_aicl_enabled == 1){
				charger->irq_aicl_enabled = 0;
				disable_irq_nosync(charger->irq_aicl);
				cancel_delayed_work(&charger->aicl_work);
				__pm_relax(charger->aicl_ws);
				charger->aicl_curr = 0;
				charger->slow_charging = false;
			}
		} else{
			if (g_usbc_data->is_hvdcp){
				if (charger->irq_aicl_enabled == 1){
					charger->irq_aicl_enabled = 0;
					disable_irq_nosync(charger->irq_aicl);
					cancel_delayed_work(&charger->aicl_work);
					__pm_relax(charger->aicl_ws);
					charger->aicl_curr = 0;
				}
				vote(charger->usb_icl_votable, MAIN_CHG_AICL_VOTER, false, 0);
			}else
			if (charger->irq_aicl_enabled == 0) {
				charger->irq_aicl_enabled = 1;
				enable_irq(charger->irq_aicl);
			}
		}

		psy_do_property("bbc", get, POWER_SUPPLY_PROP_ONLINE, pvalue);
		if (pvalue.intval != value.intval){
			psy_do_property("bbc", set, POWER_SUPPLY_PROP_ONLINE, value);
		}
		/* psy_do_property("bms", get, POWER_SUPPLY_PROP_ONLINE, pvalue); */
		/* if (pvalue.intval != value.intval){ */
		psy_do_property("bms", set, POWER_SUPPLY_PROP_ONLINE, value);
		/* } */

		value.intval = real_charger_type > 0 ? 1 : 0;
		/* power_supply_set_property(charger->psy_usb, */
				/* POWER_SUPPLY_PROP_CHARGING_ENABLED, &value); */

		charger->usb_online = value.intval;
		/* if (value.intval != charger->usb_online) */
			psy_do_property("usb", set, POWER_SUPPLY_PROP_ONLINE, value);

		/* value.intval =g_usbc_data->cc_pin_status; */
		/* psy_do_property("usb", set, POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION, value); */

		/* if (charger->real_type != real_charger_type) { */
			/* charger->real_type = real_charger_type; */
			/* post_usb_state  = true; */
			if (charger->real_type == POWER_SUPPLY_TYPE_USB_PD){
				real_charger_type = POWER_SUPPLY_TYPE_USB_PD;
			}
			value.intval = real_charger_type;
			charger->real_type = real_charger_type;
			psy_do_property("usb", set, POWER_SUPPLY_PROP_REAL_TYPE, value);
		/* } */
		if (charger->pd_active != g_usbc_data->pd_active) {
			charger->pd_active = g_usbc_data->pd_active;
			post_usb_state  = true;
		}
		if (charger->pd_active != pre_pd_active) {
			post_usb_state  = true;
		}

		charger->tmr_chgoff = 0;
		if(g_usbc_data->muic_data->vbusdet) {
			/* vote(charger->usb_icl_votable, MAX77729_CHG_VOTER, true, input_curr_limit); */
			/* vote(charger->fcc_votable, MAX77729_CHG_VOTER, true, fastchg_curr); */

			schedule_delayed_work(&charger->notify_work, 0);
			schedule_delayed_work(&charger->period_work, msecs_to_jiffies(1000));
			schedule_delayed_work(&charger->batt_notify_work, msecs_to_jiffies(600));

			pr_err("max77729 adapter plugin. real_type :%d \n",
				real_charger_type);
		}
	/* } */
	pre_pd_active = charger->pd_active;
}

static int max77729_chg_read_rawsoc(struct max77729_charger_data *charger)
{
	u8 data[2];
	int soc;

	if (max77729_bulk_read(charger->fg_i2c, SOCREP_REG, 2, data) < 0) {
		pr_err("%s: Failed to read SOCREP_REG\n", __func__);
		return -1;
	}
	soc = (data[1] * 100) + (data[0] * 100 / 256);

	/* pr_debug("%s: raw capacity (0.01%%) (%d)\n", __func__, soc); */

	return min(soc, 10000);
}

static void max77729_period_work(struct work_struct *work)
{
	struct max77729_charger_data *charger =
		container_of(work, struct max77729_charger_data, period_work.work);
	static int wdt_count = 0;
	static int notify_count = 0;
	int batt_curr = 0, batt_volt = 0, batt_ocv = 0, batt_temp=0, fast_mode = 0, batt_soc, raw_soc, batt_id = 1;
	union power_supply_propval pval ={0, };
	u8 reg_data;
	int rc = 0;

	if (charger->in_suspend){
		pr_err("%s : charger is in suspend, i2c is not ready\n", __func__);
		schedule_delayed_work(&charger->period_work, msecs_to_jiffies(1500));
		return;
	}

	max77729_chg_set_wdtmr_kick(charger);

 	if(!charger->psy_bms)
		charger->psy_bms = power_supply_get_by_name("bms");

	if (charger->psy_bms) {
		pval.intval = SEC_BATTERY_CURRENT_UA;
		rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_CURRENT_AVG, &pval);
		if (rc < 0) {
			pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_AVG fail\n", __func__);
		}
		batt_curr = pval.intval / 1000;

		pval.intval = SEC_BATTERY_VOLTAGE_AVERAGE;
		rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_VOLTAGE_AVG, &pval);
		if (rc < 0) {
			pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_AVG fail\n", __func__);
		}
		batt_volt = pval.intval;

		/* pval.intval = SEC_BATTERY_VOLTAGE_OCV; */
		/* rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_VOLTAGE_AVG, &pval); */
		/* if (rc < 0) { */
			/* pr_err("%s : get POWER_SUPPLY_PROP_ocv fail\n", __func__); */
		/* } */
		/* batt_ocv = pval.intval; */
		batt_ocv = 0;                  //move ocv printf into soc get function

		rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
		if (rc < 0) {
			pr_err("%s : get POWER_SUPPLY_PROP_FASTCHARGE_MODE fail\n", __func__);
		}
		fast_mode = pval.intval;

		rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_TEMP, &pval);
		if (rc < 0) {
			pr_err("%s : get POWER_SUPPLY_PROP_FASTCHARGE_MODE fail\n", __func__);
		}
		batt_temp = pval.intval;

		pval.intval = 0;
		rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_CAPACITY, &pval);
		batt_soc = pval.intval;

		/* pval.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RAW; */
		/* rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_CAPACITY, &pval); */
		/* raw_soc = pval.intval; */
		raw_soc = max77729_chg_read_rawsoc(charger)/10;

		rc = power_supply_get_property(charger->psy_bms, POWER_SUPPLY_PROP_RESISTANCE_ID, &pval);
		batt_id = pval.intval;

 		max77729_read_reg(charger->i2c,
				  MAX77729_CHG_REG_DETAILS_01, &reg_data);

		psy_do_property("battery", get, POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, pval);

		if (batt_id == BATTERY_VENDOR_UNKNOWN){
			schedule_delayed_work(&charger->period_work, msecs_to_jiffies(10000));
			schedule_delayed_work(&charger->batt_notify_work, msecs_to_jiffies(11000));
			return;
		}
		if (fast_mode){
			if (batt_temp >=150 && batt_temp <350) {
				/* max77729_write_word(fuelgauge->i2c, MISCCFG_REG, misccfg); */
				max77729_write_word(charger->fg_i2c, ICHGTERM_REG, 0x0400);
			} else if (batt_temp >= 350 && batt_temp < 480) {
				switch(batt_id) {
					case 1:
						max77729_write_word(charger->fg_i2c, ICHGTERM_REG, 0x0426);
						break;
					case 2:
						max77729_write_word(charger->fg_i2c, ICHGTERM_REG, 0x0466);
						break;
					case 0:
						pr_err("max77729_period_work: NVT battery id, this project should not be used it");
						max77729_write_word(charger->fg_i2c, ICHGTERM_REG, 0x0426);
						break;
					default:
						pr_err("max77729_period_work: unknown battery id, this project should not be used it");
						break;
				}
			}
		} else {
			max77729_write_word(charger->fg_i2c, ICHGTERM_REG, 0x0160);
		}
		max77729_charger_unlock(charger);
		if (pval.intval) {
			if (fast_mode) {
				if ((reg_data & 0xF) == 0x2) {
					/* vote(charger->fv_votable, MAX77729_CHG_VOTER, true, 4460); */
					if (batt_temp >=150 && batt_temp <350) {
						/* max77729_write_word(fuelgauge->i2c, MISCCFG_REG, misccfg); */
						if (batt_curr <= 750 && raw_soc > 920) {
							charger->tmr_chgoff++;
							/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_CHARGING_OFF); */
						} else {
							charger->tmr_chgoff = 0;
						}
					} else if (batt_temp >= 350 && batt_temp < 480 && raw_soc > 920) {
						if (batt_curr <= 833 && batt_id == 2) {
							charger->tmr_chgoff++;
						} else if (batt_curr <= 784 && batt_id == 1) {
							charger->tmr_chgoff++;
							/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_CHARGING_OFF); */
						} else {
							charger->tmr_chgoff = 0;
						}
					} else {
						charger->tmr_chgoff = 0;
					}
					if (charger->tmr_chgoff >= 1) {
						/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_CHARGING_OFF); */
						vote(charger->chgctrl_votable, MAIN_CHG_ENABLE_VOTER, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
					} else if (batt_curr < 1000){
						schedule_delayed_work(&charger->period_work, msecs_to_jiffies(1500));
						return;
					}
					/* max77729_write_word(charger->fg_i2c, FULLSOCTHR_REG, 0x5A00); */
					if (raw_soc <= 990 && charger->tmr_chgoff > 12){
						/* max77729_chg_set_mode_state(charger, SEC_BAT_CHG_MODE_CHARGING); */
						vote(charger->chgctrl_votable, MAIN_CHG_ENABLE_VOTER, true, SEC_BAT_CHG_MODE_CHARGING);
						charger->tmr_chgoff = 0;
					}
				} else if ((reg_data & 0xF) == 0x8){
					if (get_client_vote(charger->chgctrl_votable, MAIN_CHG_ENABLE_VOTER) == SEC_BAT_CHG_MODE_CHARGING_OFF){
						if (raw_soc < 990) {
							vote(charger->chgctrl_votable, MAIN_CHG_ENABLE_VOTER, true, SEC_BAT_CHG_MODE_CHARGING);
							charger->tmr_chgoff = 0;
						}
					}
				}
			} else {
				charger->tmr_chgoff = 0;
				max77729_set_topoff_current(charger, 250);
				max77729_set_topoff_time(charger, 1);

				if (get_effective_result_locked(charger->fv_votable) >= 4450){
					if (raw_soc <= 990 && (reg_data & 0xF) == 0x4) {
						vote(charger->chgctrl_votable, MAIN_CHG_ENABLE_VOTER, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
						vote(charger->chgctrl_votable, MAIN_CHG_ENABLE_VOTER, true, SEC_BAT_CHG_MODE_CHARGING);
					}
					max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_01, (0x3<< CHG_CNFG_01_CHG_RSTRT_SHIFT),
							CHG_CNFG_01_CHG_RSTRT_MASK);
				} else {
					max77729_update_reg(charger->i2c, MAX77729_CHG_REG_CNFG_01, (0x0<< CHG_CNFG_01_CHG_RSTRT_SHIFT),
							CHG_CNFG_01_CHG_RSTRT_MASK);
				}
				/* if (batt_temp < 480){ */
					/* max77729_write_word(charger->fg_i2c, FULLSOCTHR_REG, 0x5A00); */
				/* } */
				/* else { */
				/* max77729_write_word(charger->fg_i2c, FULLSOCTHR_REG, 0x4100); */
				/* } */

				/* max77729_write_reg(charger->i2c, 0xBA, 0x81); */
				/* max77729_write_reg(charger->i2c, 0xB8, 0xD8); */
			}
		}
	}


	if(wdt_count++ > 10){
		wdt_count = 0;
		/* max77729_chg_set_wdtmr_kick(charger); */
		if (0)
			max77729_chg_dump_reg(charger);
	}

	//if (!charger->psy_batt)
	//	charger->psy_batt = power_supply_get_by_name("battery");
	//if (charger->psy_batt && charger->batt_notify_count++ < 5)
	//	power_supply_changed(charger->psy_batt);
	//else
	//	charger->batt_notify_count = 5;

	//notify quick_charge_type
	if ((charger->real_type == POWER_SUPPLY_TYPE_USB_HVDCP) ||
		(charger->real_type == POWER_SUPPLY_TYPE_USB_PD)) {
		if (!charger->psy_usb)
			charger->psy_usb = power_supply_get_by_name("usb");
		/* pr_err("notify_count:%d\n", notify_count); */

		if (notify_count++ < NOTIFY_COUNT_MAX) {
			if (charger->psy_usb)
				power_supply_changed(charger->psy_usb);
		} else {
			notify_count = NOTIFY_COUNT_MAX;
		}

	}

	schedule_delayed_work(&charger->period_work, msecs_to_jiffies(5000));
}

static irqreturn_t max77729_sysovlo_irq(int irq, void *data)
{
	struct max77729_charger_data *charger = data;
	/* union power_supply_propval value; */

	/* pr_info("%s\n", __func__); */
	__pm_wakeup_event(charger->sysovlo_ws, jiffies_to_msecs(HZ * 5));

	/* psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_SYSOVLO, value); */

	max77729_set_sysovlo(charger, 0);
	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static int max77729_charger_parse_dt(struct max77729_charger_data *charger)
{
	struct device_node *np;
	max77729_charger_platform_data_t *pdata = charger->pdata;
	int ret = 0;

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s: np(battery) NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
					   &pdata->chg_float_voltage);
		if (ret) {
			pr_info("%s: battery,chg_float_voltage is Empty\n", __func__);
			pdata->chg_float_voltage = 4450;
		}
		charger->float_voltage = pdata->chg_float_voltage;

		ret = of_property_read_u32(np, "battery,chg_ocp_current",
					   &pdata->chg_ocp_current);
		if (ret) {
			pr_info("%s: battery,chg_ocp_current is Empty\n", __func__);
			pdata->chg_ocp_current = 5600; /* mA */
		}

		ret = of_property_read_u32(np, "battery,chg_ocp_dtc",
					   &pdata->chg_ocp_dtc);
		if (ret) {
			pr_info("%s: battery,chg_ocp_dtc is Empty\n", __func__);
			pdata->chg_ocp_dtc = 6; /* ms */
		}

		ret = of_property_read_u32(np, "battery,topoff_time",
					   &pdata->topoff_time);
		if (ret) {
			pr_info("%s: battery,topoff_time is Empty\n", __func__);
			pdata->topoff_time = 1; /* min */
		}

		ret = of_property_read_string(np, "battery,wireless_charger_name",
					(char const **)&pdata->wireless_charger_name);
		if (ret)
			pr_info("%s: Wireless charger name is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,full_check_type_2nd",
					   &pdata->full_check_type_2nd);
		if (ret)
			pr_info("%s : Full check type 2nd is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,wireless_cc_cv",
						&pdata->wireless_cc_cv);
		if (ret)
			pr_info("%s : wireless_cc_cv is Empty\n", __func__);

		/* pr_info("%s: fv : %d, ocp_curr : %d, ocp_dtc : %d, topoff_time : %d\n", */
				/* __func__, charger->float_voltage, pdata->chg_ocp_current, */
				/* pdata->chg_ocp_dtc, pdata->topoff_time); */
	}

	np = of_find_node_by_name(NULL, "max77729-charger");
	if (!np) {
		pr_err("%s: np(max77729-charger) NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "charger,fac_vsys", &pdata->fac_vsys);
		if (ret) {
			pr_info("%s : fac_vsys is Empty\n", __func__);
			pdata->fac_vsys = 3800; /* mV */
		}

		pdata->enable_sysovlo_irq =
		    of_property_read_bool(np, "charger,enable_sysovlo_irq");
		pdata->enable_sysovlo_irq = false;

		pdata->enable_noise_wa =
		    of_property_read_bool(np, "charger,enable_noise_wa");


		pdata->enable_noise_wa = false;
		if (of_property_read_u32(np, "charger,fsw", &pdata->fsw)) {
			pr_info("%s : fsw is Empty\n", __func__);
			pdata->fsw = MAX77729_CHG_FSW_1_5MHz;
		}
		charger->fsw_now = pdata->fsw;

		/* pr_info("%s: fac_vsys:%d, fsw:%d\n", __func__, pdata->fac_vsys, pdata->fsw); */
	}

	np = of_find_node_by_name(NULL, "max77729-fuelgauge");
	if (!np) {
		pr_err("%s: np(max77729-fuelgauge) NULL\n", __func__);
	} else {
		charger->jig_low_active = of_property_read_bool(np,
						"fuelgauge,jig_low_active");
		charger->jig_gpio = of_get_named_gpio(np, "fuelgauge,jig_gpio", 0);
		if (charger->jig_gpio < 0) {
			pr_err("%s: error reading jig_gpio = %d\n",
				__func__, charger->jig_gpio);
			charger->jig_gpio = 0;
		}
	}

	return ret;
}
#endif

static const struct power_supply_desc max77729_charger_power_supply_desc = {
	.name = "bbc",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = max77729_charger_props,
	.num_properties = ARRAY_SIZE(max77729_charger_props),
	.get_property = max77729_chg_get_property,
	.set_property = max77729_chg_set_property,
	.no_thermal = true,
};

static const struct power_supply_desc otg_power_supply_desc = {
	.name = "otg",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = max77729_otg_props,
	.num_properties = ARRAY_SIZE(max77729_otg_props),
	.get_property = max77729_otg_get_property,
	.set_property = max77729_otg_set_property,
	.property_is_writeable = otg_prop_is_writeable,
};

#if !defined(CONFIG_NOPMI_CHARGER)
static const struct power_supply_desc usb_power_supply_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = max77729_usb_props,
	.num_properties = ARRAY_SIZE(max77729_usb_props),
	.get_property = max77729_usb_get_property,
	.set_property = max77729_usb_set_property,
	.property_is_writeable = usb_prop_is_writeable,
};

static const struct power_supply_desc batt_power_supply_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = max77729_batt_props,
	.num_properties = ARRAY_SIZE(max77729_batt_props),
	.get_property = max77729_batt_get_property,
	.set_property = max77729_batt_set_property,
	.property_is_writeable = batt_prop_is_writeable,
};
#endif

static int main_vote_callback(struct votable *votable, void *data,
			int fcc_ma, const char *client)
{
	/* struct max77729_charger_data *charger = data; */
	union power_supply_propval value;
	/* int rc; */

	if (fcc_ma < 0)
		return 0;
	if (fcc_ma > 3000)
		fcc_ma = 3000;

	value.intval = fcc_ma;
	psy_do_property("bbc", set,
			POWER_SUPPLY_PROP_CURRENT_NOW, value);

	/* rc = max77729_set_charge_current(charger, fcc_ma); */
	/* if (rc < 0) { */
		/* pr_err("failed to set charge current\n"); */
		/* return rc; */
	/* } */
	return 0;
}

static int fcc_vote_callback(struct votable *votable, void *data,
			int fcc_ma, const char *client)
{
	struct max77729_charger_data *charger = data;
	/* union power_supply_propval value; */
	/* int rc; */

	if (fcc_ma < 0)
		return 0;
	if (fcc_ma > 3000)
		fcc_ma = 3000;

	vote(charger->mainfcc_votable, "fcc_vote_callback", true, fcc_ma);
	/* value.intval = fcc_ma; */
	/* psy_do_property("bbc", set, */
			/* POWER_SUPPLY_PROP_CURRENT_NOW, value); */

	/* rc = max77729_set_charge_current(charger, fcc_ma); */
	/* if (rc < 0) { */
		/* pr_err("failed to set charge current\n"); */
		/* return rc; */
	/* } */
	return 0;
}

static int chg_vote_callback(struct votable *votable, void *data,
			int mode, const char *client)
{
	struct max77729_charger_data *charger = data;
	union power_supply_propval value ={0, };

	switch(mode){
		case SEC_BAT_CHG_MODE_BUCK_OFF:
		case SEC_BAT_CHG_MODE_CHARGING_OFF:
		case SEC_BAT_CHG_MODE_CHARGING:
			max77729_charger_unlock(charger);
			max77729_chg_set_mode_state(charger, mode);
            value.intval = mode;
			psy_do_property("bms", set, POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED, value);
			break;
		default:
			break;
	}

	return 0;
}

static int fv_vote_callback(struct votable *votable, void *data,
			int fv_mv, const char *client)
{
	/* struct max77729_charger_data *charger = data; */
	union power_supply_propval value;
	/* int rc; */

	if (fv_mv < 0)
		return 0;

	value.intval = fv_mv;
	psy_do_property("bbc", set,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, value);

	/* rc = max77729_set_float_voltage(charger, fv_mv); */
	/* if (rc < 0) { */
		/* pr_err("failed to set chargevoltage\n"); */
		/* return rc; */
	/* } */
	return 0;
}

static int usb_icl_vote_callback(struct votable *votable, void *data,
			int icl_ma, const char *client)
{
	struct max77729_charger_data *charger = data;
	union power_supply_propval value;
	/* int rc; */

	if (icl_ma < 0)
		return 0;
	if (icl_ma > MAX77729_MAX_ICL)
		icl_ma = MAX77729_MAX_ICL;
	if (icl_ma == 0) {
		vote(charger->fcc_votable, MAIN_CHG_SUSPEND_VOTER, true, 0);
	} else {
		vote(charger->fcc_votable, MAIN_CHG_SUSPEND_VOTER, false, 0);
	}
	/* rc = max77729_set_input_current(charger, icl_ma); */

	value.intval = icl_ma;
	psy_do_property("bbc", set,
			POWER_SUPPLY_PROP_CURRENT_MAX, value);

	/* if (rc < 0) { */
		/* pr_err("failed to set input current limit\n"); */
		/* return rc; */
	/* } */
	return 0;
}

static int max77729_charger_probe(struct platform_device *pdev)
{
	struct max77729_dev *max77729 = dev_get_drvdata(pdev->dev.parent);
	struct max77729_platform_data *pdata = dev_get_platdata(max77729->dev);
	max77729_charger_platform_data_t *charger_data;
	struct max77729_charger_data *charger;
	struct power_supply_config charger_cfg = { };
	int ret = 0;
	u8 reg_data;

	pr_info("%s: max77729 Charger Driver Loading\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger_data = kzalloc(sizeof(max77729_charger_platform_data_t), GFP_KERNEL);
	if (!charger_data) {
		ret = -ENOMEM;
		goto err_free;
	}

	mutex_init(&charger->charger_mutex);
	mutex_init(&charger->mode_mutex);

	charger->dev = &pdev->dev;
	charger->i2c = max77729->charger;
	charger->pmic_i2c = max77729->i2c;
	charger->fg_i2c = max77729->fuelgauge;
	charger->muic = max77729->muic;
	charger->pdata = charger_data;
	charger->aicl_curr = 0;
	charger->slow_charging = false;
	charger->otg_on = false;
	charger->uno_on = false;
	charger->max77729_pdata = pdata;
	charger->wc_pre_current = WC_CURRENT_START;
	charger->cable_type = SEC_BATTERY_CABLE_NONE;
	charger->in_suspend = false;

	if (max77729_read_reg(max77729->i2c, MAX77729_PMIC_REG_PMICREV, &reg_data) < 0) {
		pr_err("device not found on this channel (this is not an error)\n");
		ret = -ENOMEM;
		goto err_pdata_free;
	} else {
		charger->pmic_ver = (reg_data & 0x7);
		pr_info("%s : device found : ver.0x%x\n", __func__, charger->pmic_ver);
	}

#if defined(CONFIG_OF)
	ret = max77729_charger_parse_dt(charger);
	if (ret < 0)
		pr_err("%s not found charger dt! ret[%d]\n", __func__, ret);
#endif
	platform_set_drvdata(pdev, charger);

	max77729_charger_initialize(charger);

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_OK, &reg_data);
	/* if (reg_data & MAX77729_WCIN_OK) */
		/* charger->cable_type = SEC_BATTERY_CABLE_WIRELESS; */
	charger->input_current = max77729_get_input_current(charger);
	charger->charging_current = max77729_get_charge_current(charger);

	(void)debugfs_create_file("max77729-regs",
				S_IRUGO, NULL, (void *)charger,
				  &max77729_debugfs_fops);

	charger->wqueue = create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_pdata_free;
	}
	charger->chgin_ws = wakeup_source_register(&pdev->dev, "charger->chgin");
	charger->aicl_ws = wakeup_source_register(&pdev->dev, "charger-aicl");
	charger->wc_current_ws = wakeup_source_register(&pdev->dev, "charger->wc-current");
	charger->otg_ws = wakeup_source_register(&pdev->dev, "charger->otg");

	charger_cfg.drv_data = charger;
	charger->psy_chg = devm_power_supply_register(&pdev->dev,
				&max77729_charger_power_supply_desc, &charger_cfg);
	if (IS_ERR(charger->psy_chg)) {
		ret = PTR_ERR(charger->psy_chg);
		pr_err("%s: Failed to Register psy_chg(%d)\n", __func__, ret);
	}
	//pr_err("max77729_charger_probe register psy_chg:0x%x\n", (char *)(charger->psy_chg));

	charger->psy_otg = devm_power_supply_register(&pdev->dev,
				  &otg_power_supply_desc, &charger_cfg);
	if (IS_ERR(charger->psy_otg)) {
		ret = PTR_ERR(charger->psy_otg);
		pr_err("%s: Failed to Register otg_chg(%d)\n", __func__, ret);
	}

#if !defined(CONFIG_NOPMI_CHARGER)
	charger->psy_usb = devm_power_supply_register(&pdev->dev,
				  &usb_power_supply_desc, &charger_cfg);
	if (IS_ERR(charger->psy_usb)) {
		ret = PTR_ERR(charger->psy_usb);
		pr_err("%s: Failed to Register usb_chg(%d)\n", __func__, ret);
	}

	charger->psy_batt = devm_power_supply_register(&pdev->dev,
				  &batt_power_supply_desc, &charger_cfg);
	if (IS_ERR(charger->psy_batt)) {
		ret = PTR_ERR(charger->psy_batt);
		pr_err("%s: Failed to Register psy_batt(%d)\n", __func__, ret);
	}
	//pr_err("max77729_charger_probe register psy_batt:0x%x\n", (char *)(charger->psy_batt));
#endif

	charger->psy_bms = power_supply_get_by_name("bms");
	if (!charger->psy_bms) {
		pr_err("%s get bms psy fail\n", __func__);
	}

	INIT_DELAYED_WORK(&charger->redet_work, max77729_redet_work);
	INIT_DELAYED_WORK(&charger->chgin_work, max77729_chgin_isr_work);
	INIT_DELAYED_WORK(&charger->aicl_work, max77729_aicl_isr_work);
	INIT_DELAYED_WORK(&charger->notify_work, max77729_notify_work);
	INIT_DELAYED_WORK(&charger->batt_notify_work, max77729_batt_notify_work);
	INIT_DELAYED_WORK(&charger->period_work, max77729_period_work);
	INIT_DELAYED_WORK(&charger->adapter_change_work, max77729_adapter_changed_work);

	charger->usb_icl_votable = create_votable("USB_ICL", VOTE_MIN,
					usb_icl_vote_callback,
					charger);
	if (IS_ERR(charger->usb_icl_votable)) {
		ret = PTR_ERR(charger->usb_icl_votable);
		destroy_votable(charger->usb_icl_votable);
		charger->usb_icl_votable = NULL;
		goto err_power_supply_registers;
	}

	charger->fcc_votable = create_votable("FCC", VOTE_MIN,
						fcc_vote_callback,
						charger);
	if (IS_ERR(charger->fcc_votable)) {
		ret = PTR_ERR(charger->fcc_votable);
		destroy_votable(charger->fcc_votable);
		charger->fcc_votable = NULL;
		goto err_power_supply_registers;
	}

 	charger->mainfcc_votable = create_votable("MAIN_FCC", VOTE_MIN,
						main_vote_callback,
						charger);
	if (IS_ERR(charger->mainfcc_votable)) {
		ret = PTR_ERR(charger->mainfcc_votable);
		destroy_votable(charger->mainfcc_votable);
		charger->mainfcc_votable = NULL;
		goto err_power_supply_registers;
	}

	charger->fv_votable = create_votable("FV", VOTE_MIN,
					fv_vote_callback,
					charger);
	if (IS_ERR(charger->fv_votable)) {
		ret = PTR_ERR(charger->fv_votable);
		destroy_votable(charger->fv_votable);
		charger->fv_votable = NULL;
		goto err_power_supply_registers;
	}

 	charger->chgctrl_votable = create_votable("CHG_CTRL", VOTE_MIN,
					chg_vote_callback,
					charger);
	if (IS_ERR(charger->chgctrl_votable)) {
		ret = PTR_ERR(charger->chgctrl_votable);
		destroy_votable(charger->chgctrl_votable);
		charger->chgctrl_votable = NULL;
		goto err_power_supply_registers;
	}

	if (0)           //disable charger irq
	if (charger->pdata->chg_irq) {
		INIT_DELAYED_WORK(&charger->isr_work, max77729_chg_isr_work);

		ret = request_threaded_irq(charger->pdata->chg_irq, NULL,
				max77729_chg_irq_thread, 0,
				"charger-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Request IRQ\n", __func__);
			goto err_irq;
		}

		ret = enable_irq_wake(charger->pdata->chg_irq);
		if (ret < 0)
			pr_err("%s: Failed to Enable Wakeup Source(%d)\n", __func__, ret);
	}

	max77729_read_reg(charger->i2c, MAX77729_CHG_REG_INT_OK, &reg_data);
	charger->irq_chgin = pdata->irq_base + MAX77729_USBC_IRQ_VBUS_INT;

	if (charger->irq_chgin) {
		ret = request_threaded_irq(charger->irq_chgin,
				NULL, max77729_chgin_irq,
				0,
				"chgin-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n",
				__func__, ret);
			goto err_irq;
		}
	}

#if 0
	charger->irq_chgin = pdata->irq_base + MAX77729_CHG_IRQ_CHGIN_I;
	ret = request_threaded_irq(charger->irq_chgin, NULL,
			max77729_chgin_irq, 0,
			"chgin-irq", charger);
	if (ret < 0)
		pr_err("%s: fail to request chgin IRQ: %d: %d\n",
				__func__, charger->irq_chgin, ret);
#endif

	charger->irq_bypass = pdata->irq_base + MAX77729_CHG_IRQ_BYP_I;
	ret = request_threaded_irq(charger->irq_bypass, NULL,
					max77729_bypass_irq, 0,
					"bypass-irq", charger);
	if (ret < 0)
		pr_err("%s: fail to request bypass IRQ: %d: %d\n",
		       __func__, charger->irq_bypass, ret);

	charger->irq_aicl_enabled = -1;
	charger->irq_aicl = pdata->irq_base + MAX77729_CHG_IRQ_AICL_I;
	charger->irq_batp = pdata->irq_base + MAX77729_CHG_IRQ_BATP_I;
	max77729_enable_aicl_irq(charger);

	charger->irq_aicl_enabled = 0;
	disable_irq_nosync(charger->irq_aicl);

	ret = request_threaded_irq(charger->irq_batp, NULL,
					max77729_batp_irq, 0,
					"batp-irq", charger);
	if (ret < 0)
		pr_err("%s: fail to request Battery Presense IRQ: %d: %d\n",
		       __func__, charger->irq_batp, ret);

#if defined(CONFIG_MAX77729_CHECK_B2SOVRC)
	if ((sec_debug_get_debug_level() & 0x1) == 0x1) {
		/* only work for debug level is mid */
		charger->irq_bat = pdata->irq_base + MAX77729_CHG_IRQ_BAT_I;
		ret = request_threaded_irq(charger->irq_bat, NULL,
					   max77729_bat_irq, 0,
					   "bat-irq", charger);
		if (ret < 0)
			pr_err("%s: fail to request Battery IRQ: %d: %d\n",
				   __func__, charger->irq_bat, ret);
	}
#endif

	if (charger->pdata->enable_sysovlo_irq) {
		charger->sysovlo_ws = wakeup_source_register(&pdev->dev, "max77729-sysovlo");
		if(!charger->sysovlo_ws)
		{
			pr_err("%s: fail to register charger->sysovlo_ws\n",__func__);
			goto err_power_supply_register_sysovlo;
		}
		/* Enable BIAS */
		max77729_update_reg(max77729->i2c, MAX77729_PMIC_REG_MAINCTRL1,
				    0x80, 0x80);
		/* set IRQ thread */
		charger->irq_sysovlo =
		    pdata->irq_base + MAX77729_SYSTEM_IRQ_SYSOVLO_INT;
		ret = request_threaded_irq(charger->irq_sysovlo, NULL,
						max77729_sysovlo_irq, 0,
						"sysovlo-irq", charger);
		if (ret < 0)
			pr_err("%s: fail to request sysovlo IRQ: %d: %d\n",
			       __func__, charger->irq_sysovlo, ret);
		enable_irq_wake(charger->irq_sysovlo);
	}

	ret = max77729_chg_create_attrs(&charger->psy_chg->dev);
	if (ret) {
		dev_err(charger->dev, "%s : Failed to max77729_chg_create_attrs\n", __func__);
		goto err_atts;
	}

	ret = sec_otg_create_attrs(&charger->psy_otg->dev);
	if (ret) {
		dev_err(charger->dev, "%s : Failed to sec_otg_create_attrs\n", __func__);
		goto err_atts;
	}

	/* watchdog kick */
	/* max77729_chg_set_wdtmr_kick(charger); */
    /* if (g_usbc_data) */
		/* max77729_rerun_chgdet(g_usbc_data);   //trigger the next detecion routine */
 	schedule_delayed_work(&charger->adapter_change_work, msecs_to_jiffies(1500));
     /* schedule_delayed_work(&charger->notify_work, msecs_to_jiffies(10000)); */

	/* schedule_delayed_work(&charger->period_work, msecs_to_jiffies(15000)); */
	g_max77729_charger = charger;

	vote(charger->usb_icl_votable, MAX77729_CHG_VOTER, true, 500);
	vote(charger->fcc_votable, PROFILE_CHG_VOTER, true, CHG_FCC_CURR_MAX);
	pr_info("%s: MAX77729 Charger Driver Loaded\n", __func__);
	return 0;

err_atts:
	/* free_irq(charger->pdata->chg_irq, charger); */
err_power_supply_register_sysovlo:
	wakeup_source_unregister(charger->sysovlo_ws);
err_irq:
err_power_supply_registers:
	wakeup_source_unregister(charger->otg_ws);
	wakeup_source_unregister(charger->wc_current_ws);
	wakeup_source_unregister(charger->aicl_ws);
	wakeup_source_unregister(charger->chgin_ws);
	destroy_workqueue(charger->wqueue);
err_pdata_free:
	mutex_destroy(&charger->mode_mutex);
	mutex_destroy(&charger->charger_mutex);
	kfree(charger_data);
err_free:
	kfree(charger);
	return ret;
}

static int max77729_charger_remove(struct platform_device *pdev)
{
	struct max77729_charger_data *charger = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	destroy_workqueue(charger->wqueue);

	if (charger->i2c) {
		u8 reg_data;

		reg_data = MAX77729_MODE_4_BUCK_ON;
		max77729_write_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00, reg_data);
		reg_data = 0x0F;
		max77729_write_reg(charger->i2c, MAX77729_CHG_REG_CNFG_09, reg_data);
		reg_data = 0x10;
		max77729_write_reg(charger->i2c, MAX77729_CHG_REG_CNFG_10, reg_data);
		reg_data = 0x60;
		max77729_write_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12, reg_data);
	} else {
		pr_err("%s: no max77729 i2c client\n", __func__);
	}

	/* if (charger->irq_sysovlo) */
		/* free_irq(charger->irq_sysovlo, charger); */
	/* if (charger->pdata->chg_irq) */
		/* free_irq(charger->pdata->chg_irq, charger); */
	if (charger->psy_chg)
		power_supply_unregister(charger->psy_chg);
	if (charger->psy_otg)
		power_supply_unregister(charger->psy_otg);

	wakeup_source_unregister(charger->sysovlo_ws);
	wakeup_source_unregister(charger->otg_ws);
	wakeup_source_unregister(charger->wc_current_ws);
	wakeup_source_unregister(charger->aicl_ws);
	wakeup_source_unregister(charger->chgin_ws);

	kfree(charger);

	pr_info("%s: --\n", __func__);

	return 0;
}

#if defined CONFIG_PM
static int max77729_charger_prepare(struct device *dev)
{
	struct max77729_charger_data *charger = dev_get_drvdata(dev);

	pr_info("%s\n", __func__);

	if ((charger->cable_type == SEC_BATTERY_CABLE_USB ||
		charger->cable_type == SEC_BATTERY_CABLE_TA)
		&& charger->input_current >= 500) {
		u8 reg_data;

		max77729_read_reg(charger->i2c, MAX77729_CHG_REG_CNFG_09, &reg_data);
		reg_data &= MAX77729_CHG_CHGIN_LIM;
		max77729_usbc_icurr(reg_data);
		max77729_set_fw_noautoibus(MAX77729_AUTOIBUS_ON);
	}

	return 0;
}

static int max77729_charger_suspend(struct device *dev)
{
	struct max77729_charger_data *charger = dev_get_drvdata(dev);
	charger->in_suspend = true;
	return 0;
}

static int max77729_charger_resume(struct device *dev)
{
	struct max77729_charger_data *charger = dev_get_drvdata(dev);
	charger->in_suspend = false;
	return 0;
}

static void max77729_charger_complete(struct device *dev)
{
	struct max77729_charger_data *charger = dev_get_drvdata(dev);

	pr_info("%s\n", __func__);

	if (!max77729_get_autoibus(charger))
		max77729_set_fw_noautoibus(MAX77729_AUTOIBUS_AT_OFF);
}
#else
#define max77729_charger_prepare NULL
#define max77729_charger_suspend NULL
#define max77729_charger_resume NULL
#define max77729_charger_complete NULL
#endif

static void max77729_charger_shutdown(struct platform_device *pdev)
{
	struct max77729_charger_data *charger = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

#if defined(CONFIG_SEC_FACTORY)
	if (factory_mode)
		goto free_chg;	/* prevent SMPL during SMD ARRAY shutdown */
#endif
	if (charger->i2c) {
		u8 reg_data;

		reg_data = MAX77729_MODE_4_BUCK_ON;	/* Buck on, Charge off */
		max77729_write_reg(charger->i2c, MAX77729_CHG_REG_CNFG_00, reg_data);
#if !defined(CONFIG_SEC_FACTORY)
		if ((is_wired_type(charger->cable_type))
			&& (charger->cable_type != SEC_BATTERY_CABLE_USB))
			reg_data = 0x3B;	/* CHGIN_ILIM 1500mA */
		else
#endif
			reg_data = 0x13;	/* CHGIN_ILIM 500mA */
		max77729_write_reg(charger->i2c, MAX77729_CHG_REG_CNFG_09, reg_data);
		reg_data = 0x13;	/* WCIN_ILIM 500mA */
		max77729_write_reg(charger->i2c, MAX77729_CHG_REG_CNFG_10, reg_data);
		reg_data = 0x60;	/* CHGINSEL/WCINSEL enable */
		max77729_write_reg(charger->i2c, MAX77729_CHG_REG_CNFG_12, reg_data);

		/* enable auto shipmode, this should work under 2.6V */
		max77729_set_auto_ship_mode(charger, 1);
	} else {
		pr_err("%s: no max77729 i2c client\n", __func__);
	}

#if defined(CONFIG_SEC_FACTORY)
free_chg:
#endif
	if (charger->irq_aicl)
		free_irq(charger->irq_aicl, charger);
	if (charger->irq_chgin)
		free_irq(charger->irq_chgin, charger);
	if (charger->irq_bypass)
		free_irq(charger->irq_bypass, charger);
	if (charger->irq_batp)
		free_irq(charger->irq_batp, charger);
#if defined(CONFIG_MAX77729_CHECK_B2SOVRC)
	if (charger->irq_bat)
		free_irq(charger->irq_bat, charger);
#endif
	/* if (charger->irq_sysovlo) */
		/* free_irq(charger->irq_sysovlo, charger); */
	/* if (charger->pdata->chg_irq) */
		/* free_irq(charger->pdata->chg_irq, charger); */

	/* if (charger->pdata->chg_irq) */
		/* cancel_delayed_work(&charger->isr_work); */

	cancel_delayed_work(&charger->adapter_change_work);
	cancel_delayed_work(&charger->period_work);
	cancel_delayed_work(&charger->batt_notify_work);
	cancel_delayed_work(&charger->notify_work);
	cancel_delayed_work(&charger->aicl_work);
	cancel_delayed_work(&charger->chgin_work);
	cancel_delayed_work(&charger->redet_work);
	charger->batt_notify_count = 0;

	pr_info("%s: --\n", __func__);
}

static const struct dev_pm_ops max77729_charger_pm_ops = {
	.prepare = max77729_charger_prepare,
	.suspend = max77729_charger_suspend,
	.resume = max77729_charger_resume,
	.complete = max77729_charger_complete,
};

static struct platform_driver max77729_charger_driver = {
	.driver = {
		   .name = "max77729-charger",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &max77729_charger_pm_ops,
#endif
	},
	.probe = max77729_charger_probe,
	.remove = max77729_charger_remove,
	.shutdown = max77729_charger_shutdown,
};

static int __init max77729_charger_init(void)
{
	pr_info("%s:\n", __func__);
	return platform_driver_register(&max77729_charger_driver);
}
fs_initcall(max77729_charger_init);

static void __exit max77729_charger_exit(void)
{
	platform_driver_unregister(&max77729_charger_driver);
}

/* module_init(max77729_charger_init); */
module_exit(max77729_charger_exit);

MODULE_DESCRIPTION("MAX77729 Charger Driver");
MODULE_LICENSE("GPL");
