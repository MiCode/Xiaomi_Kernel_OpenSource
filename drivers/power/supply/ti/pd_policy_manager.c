
#define pr_fmt(fmt)	"[USBPD-PM]: %s: " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>

#include "pd_policy_manager.h"

#include <linux/power/mtk_intf_mi.h>
#include <mt-plat/charger_class.h>
#include <mt-plat/mtk_charger.h>

#define pr_debug pr_err
#define PD_SRC_PDO_TYPE_FIXED		0
#define PD_SRC_PDO_TYPE_BATTERY		1
#define PD_SRC_PDO_TYPE_VARIABLE	2
#define PD_SRC_PDO_TYPE_AUGMENTED	3

#define BATT_MAX_CHG_VOLT		4480
#define BATT_FAST_CHG_CURR		6000
#define	BUS_OVP_THRESHOLD		12000
#define	BUS_OVP_ALARM_THRESHOLD		9500

#define BUS_VOLT_INIT_UP		300
#define BAT_VOLT_HYST			100
#define BUS_CURR_HYST			100

#define BAT_VOLT_LOOP_LMT		BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT		BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT		BUS_OVP_THRESHOLD

#define PM_WORK_RUN_NORMAL_INTERVAL		500
#define PM_WORK_RUN_QUICK_INTERVAL		200
#define PM_WORK_RUN_CRITICAL_INTERVAL		100

enum {
	PM_ALGO_RET_OK,
	PM_ALGO_RET_THERM_FAULT,
	PM_ALGO_RET_OTHER_FAULT,
	PM_ALGO_RET_CHG_DISABLED,
	PM_ALGO_RET_TAPER_DONE,
	PM_ALGO_RET_SC8551_CHARGE_MODE_DIV2,
};

enum {
	SC8551_STAGE_DIV2,
	SC8551_STAGE_BYPASS,
	SC8551_STAGE_SWITCHING,
};

enum {
	NOT_SUPPORT = -1,
	SC8551_CHARGE_MODE_DIV2,
	SC8551_CHARGE_MODE_BYPASS,
};

static struct pdpm_config pm_config = {
	.bat_volt_lp_lmt		= BAT_VOLT_LOOP_LMT,
	.bat_curr_lp_lmt		= BAT_CURR_LOOP_LMT + 1000,
	.bus_volt_lp_lmt		= BUS_VOLT_LOOP_LMT,
	.bus_curr_lp_lmt		= BAT_CURR_LOOP_LMT >> 1,

	.fc2_taper_current		= 2100,
	.fc2_taper_bus_current		= 1000,
	.fc2_steps			= 1,

	.min_adapter_volt_required	= 10000,
	.min_adapter_curr_required	= 2000,

	.min_vbat_for_cp		= 3500,

	.cp_sec_enable			= false,
	.fc2_disable_sw			= true,
};

static struct usbpd_pm *__pdpm;

static int fc2_taper_timer;
static int ibus_lmt_change_timer;

static int usbpd_pm_enable_cp_sec(struct usbpd_pm *pdpm, bool enable);
static int usbpd_pm_check_cp_sec_enabled(struct usbpd_pm *pdpm);
static int usbpd_pm_enable_cp(struct usbpd_pm *pdpm, bool enable);
static int usbpd_pm_check_cp_enabled(struct usbpd_pm *pdpm);
static void usbpd_pm_evaluate_src_caps(struct usbpd_pm *pdpm);
static void usbpd_check_usb_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->usb_psy) {
		pdpm->usb_psy = power_supply_get_by_name("usb");
		if (!pdpm->usb_psy)
			pr_err("usb psy not found!\n");
	}
}

static void usbpd_check_batt_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->sw_psy) {
		pdpm->sw_psy = power_supply_get_by_name("battery");
		if (!pdpm->sw_psy)
			pr_err("batt psy not found!\n");
	}
}

static void usbpd_check_bms_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->bms_psy) {
		pdpm->bms_psy = power_supply_get_by_name("bms");
		if (!pdpm->bms_psy)
			pr_err("bms psy not found!\n");
	}
}

static void cp_update_bms_ibat_vbat(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	usbpd_check_bms_psy(pdpm);

	if (pdpm->bms_psy) {
		ret = power_supply_get_property(pdpm->bms_psy,
				POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		if (!ret) {
			if (pdpm->cp.vbus_pres)
				pdpm->cp.ibat_curr = -(val.intval / 1000);
		}

		ret = power_supply_get_property(pdpm->bms_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
		if (!ret) {
			if (pdpm->cp.vbus_pres)
				pdpm->cp.vbat_volt = val.intval / 1000;
		}
	}
}

static void usbpd_check_charger_device(struct usbpd_pm *pdpm)
{
	if (!pdpm->ch1_dev) {
		pdpm->ch1_dev = get_charger_by_name("primary_chg");
		if (pdpm->ch1_dev) {
			pr_debug("%s: Found primary charger [%s]\n",
				__func__, pdpm->ch1_dev->props.alias_name);
		} else {
			pr_err("*** Error : can't find primary charger ***\n");
		}
	}

	if (!pdpm->ch3_dev) {
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");
		if (pdpm->ch3_dev) {
			pr_debug("%s: Found tertiary charger [%s]\n",
				__func__, pdpm->ch3_dev->props.alias_name);
		} else {
			pr_err("*** Error : can't find tertiary charger ***\n");
		}
	}

	if (!pdpm->ch4_dev) {
		pdpm->ch4_dev = get_charger_by_name("quaternary_chg");
		if (pdpm->ch4_dev) {
			pr_debug("%s: Found quaternary charger [%s]\n",
				__func__, pdpm->ch4_dev->props.alias_name);
		} else {
			pr_err("*** Error : can't find quaternary charger ***\n");
		}
	}
}

static int pd_set_bq_charge_done(struct usbpd_pm *pdpm ,bool enable)
{
	int ret;

	if (!pdpm->ch3_dev)
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");

	ret = charger_dev_set_bq_chg_done(pdpm->ch3_dev, enable);
	if (ret < 0)
		pr_err("set bq charge done failed, ret=%d\n", ret);

	return ret;
}

static int pd_get_hv_charge_enable(struct usbpd_pm *pdpm)
{
	int ret, enable;

	if (!pdpm->ch3_dev)
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");

	ret = charger_dev_get_hv_charge_enable(pdpm->ch3_dev, &enable);
	if (ret < 0)
		pr_err("get hv_charge_enable failed, ret=%d\n", ret);

	pr_err("get hv_charge_enable=%d\n", enable);
	return enable;
}

static int pd_get_batt_capacity(struct usbpd_pm *pdpm, int *capacity)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	usbpd_check_batt_psy(pdpm);

	if (!pdpm->sw_psy)
		return -ENODEV;

	rc = power_supply_get_property(pdpm->sw_psy,
				POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		pr_info("Couldn't get batt capacity:%d\n", rc);
		return rc;
	}

	pr_err("pval.intval: %d\n", pval.intval);
	*capacity = pval.intval;
	return rc;
}

static int pd_get_batt_current_thermal_level(struct usbpd_pm *pdpm, int *level)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	usbpd_check_batt_psy(pdpm);

	if (!pdpm->sw_psy)
		return -ENODEV;

	rc = power_supply_get_property(pdpm->sw_psy,
				POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &pval);
	if (rc < 0) {
		pr_info("Couldn't get thermal level:%d\n", rc);
		return rc;
	}

	pr_info("%s thermal_level: %d\n", __func__, pval.intval);

	*level = pval.intval;
	return rc;
}

static void pd_bq_check_ibus_to_enable_dual_bq(struct usbpd_pm *pdpm, int ibus_ma)
{

	int capacity = 0;
	int thermal_level = 0;

	pd_get_batt_capacity(pdpm, &capacity);
	pd_get_batt_current_thermal_level(pdpm, &thermal_level);
	pr_info("ibus_ma:%d no_need_en_slave_bq:%d slave_bq_disabled_check_count:%d\n",
		ibus_ma, pdpm->no_need_en_slave_bq, pdpm->slave_bq_disabled_check_count);
	if (ibus_ma <= IBUS_THRESHOLD_MA_FOR_DUAL_BQ && !pdpm->no_need_en_slave_bq
			&& (pdpm->slave_bq_disabled_check_count < IBUS_THR_TO_CLOSE_SLAVE_COUNT_MAX)) {
		pdpm->slave_bq_disabled_check_count++;
		if (pdpm->slave_bq_disabled_check_count >= IBUS_THR_TO_CLOSE_SLAVE_COUNT_MAX) {
			pdpm->no_need_en_slave_bq = true;
			pr_err("ibus decrease to threshold, disable slave bq now\n");
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
			usbpd_pm_check_cp_enabled(pdpm);
			if (!pdpm->cp.charge_enabled) {
				usbpd_pm_enable_cp(pdpm, true);
				msleep(50);
				usbpd_pm_check_cp_enabled(pdpm);
			}
		}
	} else if (pdpm->no_need_en_slave_bq && (capacity < CAPACITY_HIGH_THR)
			&& thermal_level < MAX_THERMAL_LEVEL_FOR_DUAL_BQ
			&& (ibus_ma > (IBUS_THRESHOLD_MA_FOR_DUAL_BQ + IBUS_THR_MA_HYS_FOR_DUAL_BQ))) {
		if (!pdpm->cp_sec.charge_enabled) {
			pdpm->no_need_en_slave_bq = false;
			pr_err("ibus increase above threshold, re-enable slave bq now\n");
			usbpd_pm_enable_cp_sec(pdpm, true);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}
	} else {
		pdpm->slave_bq_disabled_check_count = 0;
	}
}

static bool pd_get_fastcharge_mode_enabled(struct usbpd_pm *pdpm)
{
	union power_supply_propval pval = {0,};
	int rc;

	if (!pdpm->bms_psy)
		return false;

	rc = power_supply_get_property(pdpm->bms_psy,
				POWER_SUPPLY_PROP_FASTCHARGE_MODE, &pval);
	if (rc < 0) {
		pr_info("Couldn't get fastcharge mode:%d\n", rc);
		return false;
	}

	pr_err("fastcharge mode: %d\n", pval.intval);
	if (pval.intval == 1)
		return true;
	else
		return false;
}

static bool pd_disable_cp_by_jeita_status(struct usbpd_pm *pdpm)
{
	union power_supply_propval pval = {0,};
	int batt_temp = 0, bq_input_suspend = 0;
	int rc, bat_constant_voltage;
	bool is_fastcharge_mode = false;

	if (!pdpm->sw_psy)
		return false;

	rc = power_supply_get_property(pdpm->sw_psy,
			POWER_SUPPLY_PROP_INPUT_SUSPEND, &pval);
	if (!rc)
		bq_input_suspend = !!pval.intval;

	pr_err("bq_input_suspend: %d\n", bq_input_suspend);

	if (bq_input_suspend)
		return true;

	if (!pdpm->bms_psy)
		return false;

	rc = power_supply_get_property(pdpm->bms_psy,
				POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0) {
		pr_info("Couldn't get batt temp prop:%d\n", rc);
		return false;
	}

	batt_temp = pval.intval;
	pr_err("batt_temp: %d\n", batt_temp);

	bat_constant_voltage = pm_config.bat_volt_lp_lmt;
	is_fastcharge_mode = pd_get_fastcharge_mode_enabled(pdpm);
	if (is_fastcharge_mode)
		bat_constant_voltage += 10;

	if (!pdpm->ch1_dev)
		pdpm->ch1_dev = get_charger_by_name("primary_chg");

	if (batt_temp >= JEITA_WARM_THR && !pdpm->jeita_triggered) {
		pdpm->jeita_triggered = true;
		return true;
	} else if (batt_temp <= JEITA_COOL_NOT_ALLOW_CP_THR
			&& !pdpm->jeita_triggered) {
		pdpm->jeita_triggered = true;
		return true;
	} else if ((batt_temp <= (JEITA_WARM_THR - JEITA_HYSTERESIS))
				&& (batt_temp >= (JEITA_COOL_NOT_ALLOW_CP_THR + JEITA_HYSTERESIS))
			&& pdpm->jeita_triggered) {
		if (pdpm->ch1_dev)
			charger_dev_set_constant_voltage(pdpm->ch1_dev, (bat_constant_voltage * 1000));

		pdpm->jeita_triggered = false;
		return false;
	} else {
		return pdpm->jeita_triggered;
	}
}

static bool pd_get_bms_digest_verified(struct usbpd_pm *pdpm)
{
	union power_supply_propval pval = {0,};
	int rc;

	if (!pdpm->bms_psy)
		return false;

	rc = power_supply_get_property(pdpm->bms_psy,
				POWER_SUPPLY_PROP_AUTHENTIC, &pval);
	if (rc < 0) {
		pr_info("Couldn't get battery verified:%d\n", rc);
		return false;
	}

	pr_err("pval.intval: %d\n", pval.intval);

	if (pval.intval == 1)
		return true;
	else
		return false;
}

static bool pd_get_pps_charger_verified(struct usbpd_pm *pdpm)
{
	union power_supply_propval pval = {0,};
	int rc;

	if (!pdpm->usb_psy)
		return false;

	usbpd_pm_evaluate_src_caps(pdpm);

	rc = power_supply_get_property(pdpm->usb_psy,
				POWER_SUPPLY_PROP_PD_AUTHENTICATION, &pval);
	if (rc < 0) {
		pr_info("Couldn't get pd_authentication result:%d\n", rc);
		return false;
	}

	pr_err("pval.intval: %d\n", pval.intval);

	if (pval.intval == 1)
		return true;
	else
		return false;
}

static int usbpd_set_new_fcc_voter(struct usbpd_pm *pdpm)
{
	return 0;
}

static void usbpd_check_cp_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->cp_psy) {
		if (pm_config.cp_sec_enable)
			pdpm->cp_psy = power_supply_get_by_name("bq2597x-master");
		else
			pdpm->cp_psy = power_supply_get_by_name("bq2597x-standalone");
		if (!pdpm->cp_psy)
			pr_err("cp_psy not found\n");
	}

	if (!pdpm->ch3_dev)
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");
}

static void usbpd_check_cp_sec_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->cp_sec_psy) {
		pdpm->cp_sec_psy = power_supply_get_by_name("bq2597x-slave");
		if (!pdpm->cp_sec_psy)
			pr_err("cp_sec_psy not found\n");
	}

	if (!pdpm->ch4_dev)
		pdpm->ch4_dev = get_charger_by_name("quaternary_chg");

	pr_err("usbpd_check_cp_sec_psy\n");
}

static int usbpd_get_effective_fcc_val(struct usbpd_pm *pdpm)
{
	int effective_fcc_val = 0;
	int ret;
	union power_supply_propval val = {0,};

	usbpd_check_usb_psy(pdpm);
	if (pdpm->sw_psy) {
		ret = power_supply_get_property(pdpm->sw_psy,
				POWER_SUPPLY_PROP_FAST_CHARGE_CURRENT, &val);
		if (!ret)
			effective_fcc_val = val.intval / 1000;
	}
	pr_info("%s effective_fcc_val = %d.\n", __func__, effective_fcc_val);
	return effective_fcc_val;
}

static void usbpd_pm_update_cp_status(struct usbpd_pm *pdpm)
{
	int ret, alarm_status, fault_status, reg_status;
	int ibus;

	pr_debug("%s enter.\n", __func__);

	if (!pdpm->ch3_dev)
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");

	charger_dev_get_chg_mode(pdpm->ch3_dev, &pdpm->cp.sc8551_charge_mode);
	charger_dev_get_vbat(pdpm->ch3_dev, &pdpm->cp.vbat_volt);
	pr_debug("%s pdpm->cp.sc8551_charge_mode is %d.\n", __func__, pdpm->cp.sc8551_charge_mode);
	pr_debug("%s pdpm->cp.vbat_volt is %d.\n", __func__, pdpm->cp.vbat_volt);
	charger_dev_get_ibat(pdpm->ch3_dev, &pdpm->cp.ibat_curr);
	charger_dev_get_vbus(pdpm->ch3_dev, &pdpm->cp.vbus_volt);
	charger_dev_get_ibus(pdpm->ch3_dev, &pdpm->cp.ibus_curr);
	charger_dev_get_bus_temp(pdpm->ch3_dev, &pdpm->cp.bus_temp);
	charger_dev_get_battery_temp(pdpm->ch3_dev, &pdpm->cp.bat_temp);
	charger_dev_get_die_temp(pdpm->ch3_dev, &pdpm->cp.die_temp);
	charger_dev_get_battery_present(pdpm->ch3_dev, &pdpm->cp.batt_pres);
	charger_dev_get_vbus_present(pdpm->ch3_dev, &pdpm->cp.vbus_pres);

	cp_update_bms_ibat_vbat(pdpm);

	charger_dev_is_enabled(pdpm->ch3_dev, &pdpm->cp.charge_enabled);
	ret = charger_dev_get_alarm_status(pdpm->ch3_dev, &alarm_status);
	if (!ret) {
		pdpm->cp.bat_ovp_alarm = !!(alarm_status & BAT_OVP_ALARM_MASK);
		pdpm->cp.bat_ocp_alarm = !!(alarm_status & BAT_OCP_ALARM_MASK);
		pdpm->cp.bus_ovp_alarm = !!(alarm_status & BUS_OVP_ALARM_MASK);
		pdpm->cp.bus_ocp_alarm = !!(alarm_status & BUS_OCP_ALARM_MASK);
		pdpm->cp.bat_ucp_alarm = !!(alarm_status & BAT_UCP_ALARM_MASK);
		pdpm->cp.bat_therm_alarm = !!(alarm_status & BAT_THERM_ALARM_MASK);
		pdpm->cp.bus_therm_alarm = !!(alarm_status & BUS_THERM_ALARM_MASK);
		pdpm->cp.die_therm_alarm = !!(alarm_status & DIE_THERM_ALARM_MASK);
	}
	ret = charger_dev_get_fault_status(pdpm->ch3_dev, &fault_status);
	if (!ret) {
		pdpm->cp.bat_ovp_fault = !!(fault_status & BAT_OVP_FAULT_MASK);
		pdpm->cp.bat_ocp_fault = !!(fault_status & BAT_OCP_FAULT_MASK);
		pdpm->cp.bus_ovp_fault = !!(fault_status & BUS_OVP_FAULT_MASK);
		pdpm->cp.bus_ocp_fault = !!(fault_status & BUS_OCP_FAULT_MASK);
		pdpm->cp.bat_therm_fault = !!(fault_status & BAT_THERM_FAULT_MASK);
		pdpm->cp.bus_therm_fault = !!(fault_status & BUS_THERM_FAULT_MASK);
		pdpm->cp.die_therm_fault = !!(fault_status & DIE_THERM_FAULT_MASK);
	}

	ret = charger_dev_get_reg_status(pdpm->ch3_dev, &reg_status);
	if (!ret) {
		pdpm->cp.vbat_reg = !!(reg_status & VBAT_REG_STATUS_MASK);
		pdpm->cp.ibat_reg = !!(reg_status & IBAT_REG_STATUS_MASK);
	}
	pr_debug("%s pdpm->cp.bms_vbat_volt is %d.\n", __func__, pdpm->cp.vbat_volt);
	pr_debug("%s pdpm->cp.ibat_curr is %d.\n", __func__, pdpm->cp.ibat_curr);
	pr_debug("%s pdpm->cp.vbus_volt is %d.\n", __func__, pdpm->cp.vbus_volt);
	pr_debug("%s pdpm->cp.ibus_curr is %d.\n", __func__, pdpm->cp.ibus_curr);
	pr_debug("%s pdpm->cp.bus_temp is %d.\n", __func__, pdpm->cp.bus_temp);
	pr_debug("%s pdpm->cp.bat_temp is %d.\n", __func__, pdpm->cp.bat_temp);
	pr_debug("%s pdpm->cp.die_temp is %d.\n", __func__, pdpm->cp.die_temp);
	pr_debug("%s pdpm->cp.batt_pres is %d.\n", __func__, pdpm->cp.batt_pres);
	pr_debug("%s pdpm->cp.vbus_pres is %d.\n", __func__, pdpm->cp.vbus_pres);
	pr_debug("%s pdpm->cp.charge_enabled is %d.\n", __func__, pdpm->cp.charge_enabled);
	charger_manager_get_ibus(&ibus);
	pr_debug("%s 6360 ibus is %d.\n", __func__, ibus);
}

static void usbpd_pm_update_cp_sec_status(struct usbpd_pm *pdpm)
{

	if (!pm_config.cp_sec_enable)
		return;

	usbpd_check_cp_sec_psy(pdpm);

	if (!pdpm->cp_sec_psy)
		return;

	charger_dev_get_ibus(pdpm->ch4_dev, &pdpm->cp_sec.ibus_curr);
	charger_dev_is_enabled(pdpm->ch4_dev, &pdpm->cp_sec.charge_enabled);
}

static int usbpd_pm_enable_cp(struct usbpd_pm *pdpm, bool enable)
{
	int ret;

	if (!pdpm->chg_consumer)
		pdpm->chg_consumer = charger_manager_get_by_name(pdpm->dev, "charger_port1");

	if ((enable) && (pdpm->chg_consumer)) {
		pr_info("%s charger_dev_enable_powerpath enable:%d\n", __func__, enable);
		charger_manager_enable_power_path(pdpm->chg_consumer, MAIN_CHARGER, !enable);
	}

	if (!pdpm->ch1_dev)
		pdpm->ch1_dev = get_charger_by_name("primary_chg");
	if (!pdpm->ch3_dev)
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");

	pr_debug("%s enable = %d.\n", __func__, enable);

	ret = charger_dev_enable(pdpm->ch3_dev, enable);
	if ((!ret) && (pdpm->ch1_dev)) {
		charger_dev_enable(pdpm->ch1_dev, !enable);
		if (!enable) {
			charger_dev_set_input_current(pdpm->ch1_dev, 3000000);
		}
	}

	return ret;
}

static int usbpd_pm_enable_cp_sec(struct usbpd_pm *pdpm, bool enable)
{
	int ret;

	usbpd_check_cp_sec_psy(pdpm);

	if (!pdpm->cp_sec_psy)
		return -ENODEV;

	charger_dev_enable(pdpm->ch4_dev, enable);

	return ret;
}

static int usbpd_pm_check_cp_enabled(struct usbpd_pm *pdpm)
{
	int ret;
	bool enable;

	if (!pdpm->ch1_dev)
		pdpm->ch1_dev = get_charger_by_name("primary_chg");
	if (!pdpm->ch3_dev)
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");

	if (!pdpm->chg_consumer)
		pdpm->chg_consumer = charger_manager_get_by_name(pdpm->dev, "charger_port1");

	ret = charger_dev_is_enabled(pdpm->ch3_dev, &enable);
	if (!ret) {
		pdpm->cp.charge_enabled = !!enable;
		if ((pdpm->cp.charge_enabled == 0) && (pdpm->ch1_dev) && (pdpm->chg_consumer)) {
			charger_dev_enable(pdpm->ch1_dev, !pdpm->cp.charge_enabled);
			charger_manager_enable_power_path(pdpm->chg_consumer, MAIN_CHARGER, !pdpm->cp.charge_enabled);
			charger_dev_set_input_current(pdpm->ch1_dev, 3000000);
		}
	}

	pr_debug("pdpm->cp.charge_enabled:%d\n", pdpm->cp.charge_enabled);

	return ret;
}

static int usbpd_pm_check_cp_sec_enabled(struct usbpd_pm *pdpm)
{
	int ret;

	usbpd_check_cp_sec_psy(pdpm);

	if (!pdpm->cp_sec_psy)
		return -ENODEV;

	charger_dev_is_enabled(pdpm->ch4_dev, &pdpm->cp_sec.charge_enabled);

	pr_debug("pdpm->cp_sec.charge_enabled:%d\n", pdpm->cp_sec.charge_enabled);
	return ret;
}

static int usbpd_pm_sc8551_set_charge_mode(struct usbpd_pm *pdpm, int mode)
{
	int ret;

	if (!pdpm->ch3_dev)
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");

	ret = charger_dev_set_chg_mode(pdpm->ch3_dev, mode);
	if (ret < 0)
		pr_err("sc8551 set charge mode failed, ret=%d", ret);

	return ret;
}

static void usbpd_pm_sc8551_reset_statue(struct usbpd_pm *pdpm)
{
	usbpd_pm_sc8551_set_charge_mode(pdpm, SC8551_CHARGE_MODE_DIV2);
	pdpm->cp.sc8551_stage = SC8551_STAGE_DIV2;

	return;
}

static void usbpd_pm_sc8551_bypass_mode(struct usbpd_pm *pdpm)
{
	int fcc;

	if (pdpm->cp.sc8551_stage == SC8551_STAGE_DIV2) {
		usbpd_pm_enable_cp(pdpm, false);
		msleep(30);
		usbpd_pm_check_cp_enabled(pdpm);
		pdpm->cp.sc8551_stage = SC8551_STAGE_SWITCHING;
	}

	fcc = usbpd_get_effective_fcc_val(pdpm);
	if (fcc < 0)
		return;

	if (pdpm->cp.sc8551_stage == SC8551_STAGE_SWITCHING) {
		pdpm->request_voltage = pdpm->cp.vbat_volt + BAT_VOLT_HYST;
		pdpm->request_current = min(pdpm->request_current, MAX_BYPASS_CURRENT_MA);
		adapter_set_cap_bq(pdpm->request_voltage, pdpm->request_current);

		if (pdpm->cp.vbus_volt > 1.02 * pdpm->cp.vbat_volt
				&& pdpm->cp.vbus_volt < 1.17 * pdpm->cp.vbat_volt) {
			usbpd_pm_sc8551_set_charge_mode(pdpm, SC8551_CHARGE_MODE_BYPASS);
			usbpd_pm_enable_cp(pdpm, true);
			msleep(30);
			usbpd_pm_check_cp_enabled(pdpm);
			pdpm->cp.sc8551_stage = SC8551_STAGE_BYPASS;
		}
	}

	if (pdpm->cp.sc8551_stage == SC8551_STAGE_BYPASS) {
		adapter_set_cap_bq(pdpm->request_voltage, pdpm->request_current);
	}

	pr_info("Switch sc8551 to bypass mode, stage = %d, request_voltage = %d, request_current = %d\n",
			pdpm->cp.sc8551_stage, pdpm->request_voltage, pdpm->request_current);
}

static int usbpd_pm_enable_sw(struct usbpd_pm *pdpm, bool enable)
{
	int ret;

	if (!pdpm->sw_psy) {
		pdpm->sw_psy = power_supply_get_by_name("battery");
		if (!pdpm->sw_psy)
			return -ENODEV;
	}

	pdpm->sw.charge_enabled = enable;
	return ret;
}

static int usbpd_pm_check_sw_enabled(struct usbpd_pm *pdpm)
{
	int ret;

	if (!pdpm->sw_psy) {
		pdpm->sw_psy = power_supply_get_by_name("battery");
		if (!pdpm->sw_psy)
			return -ENODEV;
	}

	return ret;
}

static void usbpd_pm_update_sw_status(struct usbpd_pm *pdpm)
{
	usbpd_pm_check_sw_enabled(pdpm);
}

static void usbpd_pm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
	struct pps_cap_bq cap;
	int i = 0;
	unsigned long apdo_max_power = 0;
	union power_supply_propval pval = {0, };

	pr_debug("%s enter.\n", __func__);

	adapter_get_cap_bq(&cap);

	pdpm->apdo_max_volt = pm_config.min_adapter_volt_required;
	pdpm->apdo_max_curr = pm_config.min_adapter_curr_required;

	for (i = 0; i < cap.nr; i++) {
		if (cap.type[i] == 3) {
			if (cap.max_mv[i] <= PPS_VOL_MAX &&
					cap.ma[i] >= PPS_CURR_MIN &&
					(cap.max_mv[i] * cap.ma[i]) > apdo_max_power) {
				pdpm->apdo_max_volt = cap.max_mv[i];
				pdpm->apdo_max_curr = cap.ma[i];
				pdpm->apdo_selected_pdo = cap.selected_cap_idx;
				pdpm->pps_supported = true;
				apdo_max_power = cap.max_mv[i] * cap.ma[i];
			}
		}
	}
	if (pdpm->pps_supported) {
		pr_info("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d, apdo_max:%d\n",
				pdpm->apdo_selected_pdo,
				pdpm->apdo_max_volt,
				pdpm->apdo_max_curr,
				apdo_max_power);
		if (pdpm->apdo_max_curr <= LOW_POWER_PPS_CURR_THR)
			pdpm->apdo_max_curr = XIAOMI_LOW_POWER_PPS_CURR_MAX;
		pval.intval = apdo_max_power / 1000000;
		power_supply_set_property(pdpm->usb_psy,
				POWER_SUPPLY_PROP_APDO_MAX, &pval);
	} else {
		pr_info("Not qualified PPS adapter\n");
	}
}

static void usbpd_update_pps_status(struct usbpd_pm *pdpm)
{
	int ret;
	u32 status;

	return;

	ret = usbpd_get_pps_status(pdpm->pd, &status);

	if (!ret) {
		pr_info("get_pps_status: status_db :0x%x\n", status);
		pdpm->adapter_voltage = (status & 0xFFFF) * 20;
		pdpm->adapter_current = ((status >> 16) & 0xFF) * 50;
		pdpm->adapter_ptf = ((status >> 24) & 0x06) >> 1;
		pdpm->adapter_omf = !!((status >> 24) & 0x08);
		pr_info("adapter_volt:%d, adapter_current:%d\n",
				pdpm->adapter_voltage, pdpm->adapter_current);
		pr_info("pdpm->adapter_ptf:%d, pdpm->adapter_omf:%d\n",
				pdpm->adapter_ptf, pdpm->adapter_omf);
	}
}

#define TAPER_TIMEOUT	(10000 / PM_WORK_RUN_QUICK_INTERVAL)
#define IBUS_CHANGE_TIMEOUT  (1000 / PM_WORK_RUN_QUICK_INTERVAL)
static int usbpd_pm_fc2_charge_algo(struct usbpd_pm *pdpm)
{
	int steps;
	int sw_ctrl_steps = 0;
	int hw_ctrl_steps = 0;
	int step_vbat = 0;
	int step_ibus = 0;
	int step_ibat = 0;
	int step_bat_reg = 0;
	int fcc_ibatt_diff = 0;
	int icl_ibus_diff = 0;
	int ibus_total = 0;
	int effective_fcc_val = 0;
	int effective_fcc_taper = 0;
	int thermal_level = 0;
	int ibus_max_lim = 0;
	bool is_fastcharge_mode = false;
	static int ibatt_count = 0;
	static int ibat_now_ma_before = 0;
	static int curr_fcc_limit, curr_ibus_limit;
	int ret;
	int taper_hys_mv = 0;

	static int ibus_limit;

	is_fastcharge_mode = pd_get_fastcharge_mode_enabled(pdpm);
	if (is_fastcharge_mode)
		pm_config.bat_volt_lp_lmt = pdpm->bat_volt_max;
	else
		pm_config.bat_volt_lp_lmt = pdpm->non_ffc_bat_volt_max;

	usbpd_set_new_fcc_voter(pdpm);

	effective_fcc_val = usbpd_get_effective_fcc_val(pdpm);

	if (!(pdpm->cp.sc8551_charge_mode))
		taper_hys_mv = BQ_TAPER_HYS_MV;
	else
		taper_hys_mv = SC8551_TAPER_HYS_MV;

	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - taper_hys_mv) {
		if (ibus_lmt_change_timer++ > IBUS_CHANGE_TIMEOUT) {
			ibus_lmt_change_timer = 0;
			effective_fcc_taper = usbpd_get_effective_fcc_val(pdpm);
			effective_fcc_taper -= BQ_TAPER_DECREASE_STEP_MA;
			pr_info("bq set taper fcc to: %d mA\n", effective_fcc_taper);

			ret = charger_manager_set_current_limit(effective_fcc_taper * 1000, BQ_FCC);
		}
	} else {
		ibus_lmt_change_timer = 0;
	}

	if (effective_fcc_val >= 0) {
		curr_fcc_limit = min(pm_config.bat_curr_lp_lmt, effective_fcc_val);
		if (pm_config.cp_sec_enable) {
			if (pdpm->no_need_en_slave_bq)
				curr_fcc_limit = min(curr_fcc_limit, FCC_MAX_MA_FOR_MASTER_BQ);
		}
		if (pdpm->cp.sc8551_charge_mode == SC8551_CHARGE_MODE_BYPASS) {
			curr_ibus_limit = min(curr_fcc_limit, pdpm->apdo_max_curr);
			ibus_limit = curr_ibus_limit;
		} else {
			if (pdpm->apdo_max_curr == PPS_CURR_MAX_67W)
				ibus_max_lim = PPS_CURR_MAX_67W + 100;
			else
				ibus_max_lim = pdpm->apdo_max_curr;

			ibus_max_lim = max(ibus_max_lim, pdpm->apdo_max_curr);
			curr_ibus_limit = min(curr_fcc_limit/2 + 180, ibus_max_lim);
			if (pdpm->cp.sc8551_charge_mode == NOT_SUPPORT) {
				ibus_limit = curr_ibus_limit + 100;
			} else {
					ibus_limit = curr_ibus_limit;
			}
		}
	}

	pr_info("curr_ibus_limit:%d, ibus_limit:%d, bat_curr_lp_lmt:%d, effective_fcc_val:%d, apdo_max_curr:%d\n",
		curr_ibus_limit, ibus_limit, pm_config.bat_curr_lp_lmt, effective_fcc_val, pdpm->apdo_max_curr);
	pr_info("sc8551_mode:%d\n", pdpm->cp.sc8551_charge_mode);

	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt)
		step_vbat = -pm_config.fc2_steps;
	else if (pdpm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - 10)
		step_vbat = pm_config.fc2_steps;;

	if (pdpm->cp.ibat_curr == ibat_now_ma_before && ibatt_count < pdpm->ibatt_count_index) {
		ibatt_count++;
	} else {
		ibatt_count = 0;
		ibat_now_ma_before = pdpm->cp.ibat_curr;
		if (pdpm->cp.ibat_curr < curr_fcc_limit - 50)
			step_ibat = pm_config.fc2_steps;
		else if (pdpm->cp.ibat_curr > curr_fcc_limit + 50)
			step_ibat = -pm_config.fc2_steps;
	}
	pr_info("step_ibat:%d, ibatt_count = %d, ibatt_count_index = %d\n",
			step_ibat, ibatt_count, pdpm->ibatt_count_index);

	ibus_total = pdpm->cp.ibus_curr;

	if (pm_config.cp_sec_enable) {
		ibus_total += pdpm->cp_sec.ibus_curr;
		pd_bq_check_ibus_to_enable_dual_bq(pdpm, ibus_total);
	}

	pr_info("ibus_total_ma: %d\n", ibus_total);
	pr_info("ibus_master_ma: %d\n", pdpm->cp.ibus_curr);
	pr_info("ibus_slave_ma: %d\n", pdpm->cp_sec.ibus_curr);
	pr_info("vbus_mv: %d\n", pdpm->cp.vbus_volt);
	pr_info("vbat_mv: %d\n", pdpm->cp.vbat_volt);
	pr_info("ibat_ma: %d\n", pdpm->cp.ibat_curr);

	if (ibus_total < ibus_limit - 50)
		step_ibus = pm_config.fc2_steps;
	else if (ibus_total > ibus_limit)
		step_ibus = -pm_config.fc2_steps;
	pr_info("step_ibus:%d\n", step_ibus);

	pr_info("pdpm->cp.vbat_reg:%d, pdpm->cp.ibat_reg:%d\n",
			pdpm->cp.vbat_reg, pdpm->cp.ibat_reg);
	if (pdpm->cp.vbat_reg)
		step_bat_reg = 3 * (-pm_config.fc2_steps);
	else
		step_bat_reg = pm_config.fc2_steps;

	pr_info("step_bat_reg:%d\n", step_bat_reg);
	sw_ctrl_steps = min(min(step_vbat, step_ibus), step_ibat);
	sw_ctrl_steps = min(sw_ctrl_steps, step_bat_reg);

	pr_info("sw_ctrl_steps:%d\n", sw_ctrl_steps);
	if (pdpm->cp.bus_ocp_alarm || pdpm->cp.bus_ovp_alarm)
		hw_ctrl_steps = -pm_config.fc2_steps;
	else
		hw_ctrl_steps = pm_config.fc2_steps;
	pr_info("hw_ctrl_steps:%d\n", hw_ctrl_steps);
	usbpd_pm_check_cp_enabled(pdpm);

	if (pm_config.cp_sec_enable)
		usbpd_pm_check_cp_sec_enabled(pdpm);

	pd_get_batt_current_thermal_level(pdpm, &thermal_level);

	pdpm->is_temp_out_fc2_range = pd_disable_cp_by_jeita_status(pdpm);
	pr_info("is_temp_out_fc2_range = %d, thermal_level = %d\n", pdpm->is_temp_out_fc2_range, thermal_level);

	if (pdpm->cp.bat_therm_fault) {
		pr_info("bat_therm_fault:%d\n", pdpm->cp.bat_therm_fault);
		return PM_ALGO_RET_THERM_FAULT;
	} else if (effective_fcc_val == 0 && !pdpm->cp_sec.charge_enabled) {
		pr_info("enable cool mode\n");
		return PM_ALGO_RET_CHG_DISABLED;
	} else if (thermal_level >= MAX_THERMAL_LEVEL
			|| pdpm->is_temp_out_fc2_range) {
		pr_info("thermal level too high or batt temp is out of fc2 range\n");
		return PM_ALGO_RET_CHG_DISABLED;
	} else if (pdpm->cp.bat_ocp_fault || pdpm->cp.bus_ocp_fault
			|| pdpm->cp.bat_ovp_fault || pdpm->cp.bus_ovp_fault) {
		pr_info("bat_ocp_fault:%d, bus_ocp_fault:%d, bat_ovp_fault:%d, bus_ovp_fault:%d\n",
				pdpm->cp.bat_ocp_fault, pdpm->cp.bus_ocp_fault,
				pdpm->cp.bat_ovp_fault, pdpm->cp.bus_ovp_fault);
		return PM_ALGO_RET_OTHER_FAULT;
	} else if ((!pdpm->cp.charge_enabled && pdpm->cp.sc8551_stage != SC8551_STAGE_SWITCHING)
			|| (pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled && !pdpm->no_need_en_slave_bq)) {
		pr_info("cp.charge_enabled:%d, cp_sec.charge_enabled:%d\n",
				pdpm->cp.charge_enabled, pdpm->cp_sec.charge_enabled);
		return PM_ALGO_RET_CHG_DISABLED;
	}

	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - TAPER_VOL_HYS
			&& (((pdpm->cp.sc8551_charge_mode != 1) && (pdpm->cp.ibat_curr < pm_config.fc2_taper_current))
				|| ((pdpm->cp.sc8551_charge_mode == 1) && (pdpm->cp.ibus_curr < pm_config.fc2_taper_bus_current)))) {
		if (fc2_taper_timer++ > TAPER_TIMEOUT) {
			pr_info("charge pump taper charging done\n");
			fc2_taper_timer = 0;
			return PM_ALGO_RET_TAPER_DONE;
		}
	} else {
		fc2_taper_timer = 0;
	}

	steps = min(sw_ctrl_steps, hw_ctrl_steps);

	if (pdpm->cp.ibat_curr > 0 && ibus_total > 0 && steps > 0 || effective_fcc_val == 0) {
		fcc_ibatt_diff = (pdpm->cp.ibat_curr > curr_fcc_limit) ? (pdpm->cp.ibat_curr - curr_fcc_limit) : (curr_fcc_limit - pdpm->cp.ibat_curr);
		icl_ibus_diff = (ibus_total > curr_ibus_limit) ? (ibus_total - curr_ibus_limit) : (curr_ibus_limit - ibus_total);
		if (fcc_ibatt_diff > 1000 && icl_ibus_diff > 500)
			steps = steps * 5;
		else if (fcc_ibatt_diff > 500 && fcc_ibatt_diff <= 1000 && icl_ibus_diff <= 500 && icl_ibus_diff > 250)
			steps = steps * 2;
	}

	pr_info("steps: %d, sw_ctrl_steps:%d, hw_ctrl_steps:%d\n", steps, sw_ctrl_steps, hw_ctrl_steps);

	pdpm->request_voltage += steps * STEP_MV;

	if (pdpm->request_current < curr_ibus_limit)
		pdpm->request_current = min(pdpm->apdo_max_curr, curr_ibus_limit);

	pr_info("steps: %d, pdpm->request_voltage: %d\n", steps, pdpm->request_voltage);

	if (pdpm->apdo_max_volt == PPS_VOL_MAX)
		pdpm->apdo_max_volt = pdpm->apdo_max_volt - PPS_VOL_HYS;

	if (pdpm->request_voltage > pdpm->apdo_max_volt)
		pdpm->request_voltage = pdpm->apdo_max_volt;

	if (pdpm->cp.sc8551_charge_mode != NOT_SUPPORT && effective_fcc_val <= BYPASS_IN && !pdpm->sc8551_bypass && pdpm->sc_bypass_enable == 1) {
		pr_info("Switch sc8551 to bypass mode\n");
		pdpm->sc8551_bypass = true;
	} else if (pdpm->cp.sc8551_stage == SC8551_STAGE_BYPASS && effective_fcc_val >= BYPASS_OUT && pdpm->sc8551_bypass && pdpm->sc_bypass_enable == 1) {
		pdpm->sc8551_bypass = false;
		return PM_ALGO_RET_SC8551_CHARGE_MODE_DIV2;
	}

	return PM_ALGO_RET_OK;
}

static const unsigned char *pm_str[] = {
	"PD_PM_STATE_ENTRY",
	"PD_PM_STATE_FC2_ENTRY",
	"PD_PM_STATE_FC2_ENTRY_1",
	"PD_PM_STATE_FC2_ENTRY_2",
	"PD_PM_STATE_FC2_ENTRY_3",
	"PD_PM_STATE_FC2_TUNE",
	"PD_PM_STATE_FC2_EXIT",
};

static void usbpd_pm_move_state(struct usbpd_pm *pdpm, enum pm_state state)
{
	pr_info("state change:%s -> %s\n",
		pm_str[pdpm->state], pm_str[state]);
	pdpm->state = state;
}

static int usbpd_pm_sm(struct usbpd_pm *pdpm)
{
	int ret;
	int rc = 0;
	static int tune_vbus_retry = 0, adapter_adjust_retry = 0;
	static bool stop_sw;
	static bool recover;
	int effective_fcc_val = 0;
	int thermal_level = 0, capacity;
	static int curr_fcc_lmt, curr_ibus_lmt;

	pr_debug("%s enter.\n", __func__);

	if (!pdpm->ch1_dev)
		pdpm->ch1_dev = get_charger_by_name("primary_chg");

	switch (pdpm->state) {
	case PD_PM_STATE_ENTRY:
		stop_sw = false;
		recover = false;
		pdpm->sc8551_bypass = false;
		usbpd_pm_sc8551_reset_statue(pdpm);

		pd_get_batt_current_thermal_level(pdpm, &thermal_level);
		pdpm->is_temp_out_fc2_range = pd_disable_cp_by_jeita_status(pdpm);
		pr_info("is_temp_out_fc2_range:%d\n", pdpm->is_temp_out_fc2_range);
		pd_get_batt_capacity(pdpm, &capacity);

		usbpd_set_new_fcc_voter(pdpm);

		effective_fcc_val = usbpd_get_effective_fcc_val(pdpm);

		if (effective_fcc_val > 0) {
			curr_fcc_lmt = min(pm_config.bat_curr_lp_lmt, effective_fcc_val);
			curr_ibus_lmt = curr_fcc_lmt >> 1;
			pr_info("curr_ibus_lmt:%d\n", curr_ibus_lmt);
		}

		if (pdpm->cp.vbat_volt < pm_config.min_vbat_for_cp) {
			pd_set_bq_charge_done(pdpm, true);
			pr_info("batt_volt %d, waiting...\n", pdpm->cp.vbat_volt);
		} else if ((pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - VBAT_HIGH_FOR_FC_HYS_MV
			&& !pdpm->is_temp_out_fc2_range) || capacity >= CAPACITY_TOO_HIGH_THR) {
			pr_info("batt_volt %d is too high for cp,\
					charging with switch charger\n",
					pdpm->cp.vbat_volt);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		} else if (!pd_get_bms_digest_verified(pdpm)) {
			pd_set_bq_charge_done(pdpm, true);
			pr_info("bms digest is not verified, waiting...\n");
		} else if (effective_fcc_val < START_DRIECT_CHARGE_FCC_MIN_THR) {
			pd_set_bq_charge_done(pdpm, true);
			pr_info("effective fcc is below start dc threshold, waiting...\n");
		} else if (thermal_level >= MAX_THERMAL_LEVEL
				|| pdpm->is_temp_out_fc2_range) {
			pd_set_bq_charge_done(pdpm, true);
			pr_info("thermal too high or batt temp is out of fc2 range, waiting...\n");
		} else if (!pd_get_hv_charge_enable(pdpm)) {
			pd_set_bq_charge_done(pdpm, true);
			pr_info("hv charge disbale, waiting...\n");
		} else {
			pd_set_bq_charge_done(pdpm, false);
			if (pdpm->ch1_dev)
				charger_dev_set_input_current(pdpm->ch1_dev, 100000);
			pr_info("batt_volt-%d is ok, start flash charging\n",
					pdpm->cp.vbat_volt);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY);
		}
		break;

	case PD_PM_STATE_FC2_ENTRY:
		if (pm_config.fc2_disable_sw) {
			if (pdpm->sw.charge_enabled) {
				usbpd_pm_enable_sw(pdpm, false);
				usbpd_pm_check_sw_enabled(pdpm);
			}
			if (!pdpm->sw.charge_enabled)
				usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
		} else {
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
		}
		break;

	case PD_PM_STATE_FC2_ENTRY_1:
		pdpm->request_voltage = pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP;
		pdpm->request_current = min(pdpm->apdo_max_curr, curr_ibus_lmt);

		adapter_set_cap_start_bq(pdpm->request_voltage, pdpm->request_current);
		pr_debug("request_voltage:%d, request_current:%d\n",
				pdpm->request_voltage, pdpm->request_current);

		usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);

		tune_vbus_retry = 0;
		break;

	case PD_PM_STATE_FC2_ENTRY_2:
		if ((pdpm->cp.vbus_volt < (pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP - 50))
				|| (pdpm->cp.vbus_volt > (pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP + 100))) {
			tune_vbus_retry++;

			if (pdpm->cp.vbus_volt < (pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP - 50))
				pdpm->request_voltage += STEP_MV;
			else
				pdpm->request_voltage -= STEP_MV;

			ret = adapter_set_cap_bq(pdpm->request_voltage, pdpm->request_current);
			if (ret == ADAPTER_ADJUST) {
				pr_err("Adapter deny for invalid request, retry:%d!\n", adapter_adjust_retry);
				if (adapter_adjust_retry > 10) {
					usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
				} else {
					adapter_adjust_retry++;
					usbpd_pm_evaluate_src_caps(pdpm);
					usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
				}
			}
		} else {
			pr_info("adapter volt tune ok, retry %d times\n", tune_vbus_retry);
			tune_vbus_retry = 0;
			adapter_adjust_retry = 0;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_3);
			break;
		}

		if (tune_vbus_retry > 50) {
			pr_info("Failed to tune adapter volt into valid range, charge with switching charger\n");
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}
		break;

	case PD_PM_STATE_FC2_ENTRY_3:
		if (pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled) {
			pd_get_batt_current_thermal_level(pdpm, &thermal_level);
			pd_get_batt_capacity(pdpm, &capacity);
			if (thermal_level < MAX_THERMAL_LEVEL_FOR_DUAL_BQ && capacity < CAPACITY_HIGH_THR) {
				usbpd_pm_enable_cp_sec(pdpm, true);
				msleep(30);
				usbpd_pm_check_cp_sec_enabled(pdpm);
			} else
				pdpm->no_need_en_slave_bq = true;
		}

		if (!pdpm->cp.charge_enabled) {
			usbpd_pm_enable_cp(pdpm, true);
			msleep(30);
			usbpd_pm_check_cp_enabled(pdpm);
		}

		if (pdpm->cp.charge_enabled) {
			if ((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled && !pdpm->no_need_en_slave_bq)
					|| pdpm->no_need_en_slave_bq || !pm_config.cp_sec_enable) {
				pd_set_bq_charge_done(pdpm, false);
				usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_TUNE);
				ibus_lmt_change_timer = 0;
				fc2_taper_timer = 0;
			}
		}
		break;

	case PD_PM_STATE_FC2_TUNE:
		usbpd_update_pps_status(pdpm);

		ret = usbpd_pm_fc2_charge_algo(pdpm);

		if (ret == PM_ALGO_RET_THERM_FAULT) {
			pr_info("Move to stop charging:%d\n", ret);
			stop_sw = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else if (ret == PM_ALGO_RET_OTHER_FAULT || ret == PM_ALGO_RET_TAPER_DONE) {
			pr_info("Move to switch charging:%d\n", ret);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else if (ret == PM_ALGO_RET_CHG_DISABLED || !pd_get_hv_charge_enable(pdpm)) {
			pr_info("Move to switch charging, will try to recover flash charging:%d\n",
					ret);
			recover = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else if (pdpm->sc8551_bypass) {
			usbpd_pm_sc8551_bypass_mode(pdpm);
			break;
		} else if (ret == PM_ALGO_RET_SC8551_CHARGE_MODE_DIV2) {
			pr_info("Switch sc8551 to div2 mode\n");
			recover = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else {
			ret = adapter_set_cap_bq(pdpm->request_voltage, pdpm->request_current);
			if (ret == ADAPTER_ADJUST) {
				pr_err("Adapter deny for invalid request, need to re-evaluate src caps!\n");
				usbpd_pm_evaluate_src_caps(pdpm);
				usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
			}
			pr_info("request_voltage:%d, request_current:%d\n", pdpm->request_voltage, pdpm->request_current);
		}
		if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled && !pdpm->no_need_en_slave_bq
				&& pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - TAPER_WITH_IBUS_HYS
				&& (pdpm->cp.ibus_curr < TAPER_IBUS_THR
						|| pdpm->cp_sec.ibus_curr < TAPER_IBUS_THR)) {
			pr_info("second cp is disabled due to ibus < 450mA\n");
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}
		break;

	case PD_PM_STATE_FC2_EXIT:
		if (pdpm->ch1_dev)
			charger_dev_set_mivr(pdpm->ch1_dev, 4600000);
		adapter_set_cap_end_bq(5000, 3000);
		pd_set_bq_charge_done(pdpm, true);

		pdpm->no_need_en_slave_bq = false;
		usbpd_check_usb_psy(pdpm);
		usbpd_pm_check_cp_enabled(pdpm);

		adapter_adjust_retry = 0;

		ret = charger_manager_set_current_limit((pdpm->bat_curr_max * 1000), BQ_FCC);
		if (ret > 0)
			pr_info("%s: set current limit success\n", __func__);

		if (pdpm->cp.charge_enabled) {
			usbpd_pm_enable_cp(pdpm, false);
			usbpd_pm_check_cp_enabled(pdpm);
		}

		usbpd_pm_sc8551_reset_statue(pdpm);

		if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled) {
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}

		if (stop_sw && pdpm->sw.charge_enabled)
			usbpd_pm_enable_sw(pdpm, false);
		else if (!stop_sw && !pdpm->sw.charge_enabled)
			usbpd_pm_enable_sw(pdpm, true);

		usbpd_pm_check_sw_enabled(pdpm);

		if (recover)
			usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
		else
			rc = 1;

		break;
	default:
		usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
		break;
	}

	return rc;
}

static int verify_check_time;
static void usbpd_pm_workfunc(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					pm_work.work);
	int internal = PM_WORK_RUN_NORMAL_INTERVAL;

	pr_debug("%s enter.\n", __func__);
	usbpd_pm_update_sw_status(pdpm);
	usbpd_pm_update_cp_status(pdpm);
	usbpd_pm_update_cp_sec_status(pdpm);

	if (verify_check_time < 3) {
		verify_check_time++;
		if (!pd_get_pps_charger_verified(pdpm)) {
			if (verify_check_time == 3) {
				if (pm_config.bat_curr_lp_lmt > NON_VERIFIED_PPS_FCC_MAX)
					pm_config.bat_curr_lp_lmt = NON_VERIFIED_PPS_FCC_MAX;
			}
		}
	}

	if (!usbpd_pm_sm(pdpm) && pdpm->pd_active) {
		if (pdpm->cp.vbat_volt >= CRITICAL_HIGH_VOL_THR_MV && pm_config.cp_sec_enable)
			internal = PM_WORK_RUN_CRITICAL_INTERVAL;
		else if (pdpm->cp.vbat_volt >= HIGH_VOL_THR_MV)
			internal = PM_WORK_RUN_QUICK_INTERVAL;
		else if (pdpm->state > PD_PM_STATE_ENTRY && pdpm->state < PD_PM_STATE_FC2_TUNE)
			internal = PM_WORK_RUN_CRITICAL_INTERVAL;
		else
			internal = PM_WORK_RUN_NORMAL_INTERVAL;

		pdpm->ibatt_count_index = 1000 / internal;
		schedule_delayed_work(&pdpm->pm_work,
				msecs_to_jiffies(internal));
	} else {
		if (pdpm->cp.sc8551_charge_mode != NOT_SUPPORT)
			usbpd_pm_sc8551_reset_statue(pdpm);
	}

}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval pval = {0, };

	cancel_delayed_work_sync(&pdpm->pm_work);
	usbpd_check_usb_psy(pdpm);

	ret = charger_manager_set_current_limit((pdpm->bat_curr_max * 1000), BQ_FCC);
	if (ret > 0)
		pr_info("%s: set current limit success\n", __func__);

	verify_check_time = 0;
	pdpm->pps_supported = false;
	pdpm->jeita_triggered = false;
	pdpm->is_temp_out_fc2_range = false;
	pdpm->no_need_en_slave_bq = false;
	pdpm->pd_verified_checked = false;
	pdpm->apdo_selected_pdo = 0;
	pdpm->slave_bq_disabled_check_count = 0;
	pm_config.bat_volt_lp_lmt = pdpm->bat_volt_max;
	memset(&pdpm->pdo, 0, sizeof(pdpm->pdo));
	pm_config.bat_curr_lp_lmt = pdpm->bat_curr_max;

	pval.intval = 0;
	power_supply_set_property(pdpm->usb_psy,
			POWER_SUPPLY_PROP_APDO_MAX, &pval);
	usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
}

static void usbpd_pd_contact(struct usbpd_pm *pdpm, bool connected)
{
	int ret;
	union power_supply_propval val = {0,};

	pdpm->pd_active = connected;
	if (pdpm->usb_psy == NULL)
		usbpd_check_usb_psy(pdpm);

	if (connected) {
		usbpd_pm_evaluate_src_caps(pdpm);
		ret = power_supply_get_property(pdpm->usb_psy,
						POWER_SUPPLY_PROP_PD_TYPE, &val);
		if (ret)
			pr_err("Failed to read pd type!\n");

		if (val.intval == POWER_SUPPLY_PD_APDO)
			schedule_delayed_work(&pdpm->pm_work, 0);

		pr_debug("%s pd is connected.\n", __func__);
	} else {
		usbpd_pm_disconnect(pdpm);
		pr_debug("%s pd is not connected.\n", __func__);
	}
}

static void usbpd_pps_non_verified_contact(struct usbpd_pm *pdpm, bool connected)
{
	int ret;
	union power_supply_propval val = {0,};

	pdpm->pd_active = connected;
	if (pdpm->usb_psy == NULL)
		usbpd_check_usb_psy(pdpm);

	if (connected) {
		usbpd_pm_evaluate_src_caps(pdpm);
		ret = power_supply_get_property(pdpm->usb_psy,
						POWER_SUPPLY_PROP_PD_TYPE, &val);
		if (ret)
			pr_err("Failed to read pd type!\n");

		if (val.intval == POWER_SUPPLY_PD_APDO)
			schedule_delayed_work(&pdpm->pm_work, 5*HZ);

		pr_debug("%s pd is connected.\n", __func__);
	} else {
		usbpd_pm_disconnect(pdpm);
		pr_debug("%s pd is not connected.\n", __func__);
	}
}

static void cp_psy_change_work(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					cp_psy_change_work);
	pr_debug("%s enter.\n", __func__);
	pdpm->psy_change_running = false;
}

static void usb_psy_change_work(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					usb_psy_change_work);
	union power_supply_propval val = {0,};
	union power_supply_propval pd_auth_val = {0,};
	int ret = 0;
	bool pps_support = false;

	pr_debug("%s enter.\n", __func__);

	ret = power_supply_get_property(pdpm->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_POWER_ROLE, &val);
	if (ret) {
		pr_err("Failed to read typec power role\n");
		goto out;
	}

	pr_debug("%s power role is %d.\n", __func__, val.intval);
	if (val.intval != 1 &&
			val.intval != 3 &&
			val.intval != 5)
		goto out;

	ret = power_supply_get_property(pdpm->usb_psy,
					POWER_SUPPLY_PROP_PD_TYPE, &val);
	if (ret) {
		pr_err("Failed to read pd type!\n");
		goto out;
	}
	if (val.intval == POWER_SUPPLY_PD_APDO) {
		pps_support = true;
		pr_debug("pps support.\n");
	} else {
		pps_support = false;
		pr_debug("pps unsupport.\n");
	}

	ret = power_supply_get_property(pdpm->usb_psy,
				POWER_SUPPLY_PROP_PD_AUTHENTICATION, &pd_auth_val);
	if (ret) {
		pr_err("Failed to read typec power role\n");
		goto out;
	}

	pr_debug("%s pd_auth_val.intval is %d.\n", __func__, pd_auth_val.intval);
	pr_debug("%s pdpm->pd_active is %d.\n", __func__, pdpm->pd_active);
	if (!pdpm->pd_active && (pd_auth_val.intval == 1)
			&& (pps_support == true)) {
		msleep(1500);
		usbpd_pd_contact(pdpm, true);
		pr_debug("this is xiaomi TA and authentic is ok.\n");
	} else if (!pdpm->pd_active
			&& (pps_support == true)) {
		usbpd_pps_non_verified_contact(pdpm, true);
		pr_debug("this is commom pps TA.\n");
	} else if (pdpm->pd_active && (pps_support == false)) {
		usbpd_pd_contact(pdpm, false);
		pr_debug("this is commom pd TA.\n");
	}
out:
	pdpm->psy_change_running = false;
}

static int usbpd_psy_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	pr_debug("%s enter\n", __func__);
	usbpd_check_cp_psy(pdpm);
	usbpd_check_usb_psy(pdpm);

	if (!pdpm->cp_psy || !pdpm->usb_psy)
		return NOTIFY_OK;

	if (psy == pdpm->cp_psy || psy == pdpm->usb_psy) {
		spin_lock_irqsave(&pdpm->psy_change_lock, flags);
		if (!pdpm->psy_change_running) {
			pdpm->psy_change_running = true;
			pr_debug("%s psy_change is running\n", __func__);
			if (psy == pdpm->cp_psy)
				schedule_work(&pdpm->cp_psy_change_work);
			else
				schedule_work(&pdpm->usb_psy_change_work);
		}
		spin_unlock_irqrestore(&pdpm->psy_change_lock, flags);
	}

	return NOTIFY_OK;
}

static int pd_policy_parse_dt(struct usbpd_pm *pdpm)
{
	struct device_node *node = pdpm->dev->of_node;
	int rc = 0;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node,
			"mi,pd-bat-volt-max", &pdpm->bat_volt_max);
	if (rc < 0)
		pr_err("pd-bat-volt-max property missing, use default val\n");
	else
		pm_config.bat_volt_lp_lmt = pdpm->bat_volt_max;
	pr_info("pm_config.bat_volt_lp_lmt:%d\n", pm_config.bat_volt_lp_lmt);

	rc = of_property_read_u32(node,
			"mi,pd-bat-curr-max", &pdpm->bat_curr_max);
	if (rc < 0)
		pr_err("pd-bat-curr-max property missing, use default val\n");
	else
		pm_config.bat_curr_lp_lmt = pdpm->bat_curr_max;
	pr_info("pm_config.bat_curr_lp_lmt:%d\n", pm_config.bat_curr_lp_lmt);

	rc = of_property_read_u32(node,
			"mi,pd-bus-volt-max", &pdpm->bus_volt_max);
	if (rc < 0)
		pr_err("pd-bus-volt-max property missing, use default val\n");
	else
		pm_config.bus_volt_lp_lmt = pdpm->bus_volt_max;
	pr_info("pm_config.bus_volt_lp_lmt:%d\n", pm_config.bus_volt_lp_lmt);

	rc = of_property_read_u32(node,
			"mi,pd-bus-curr-max", &pdpm->bus_curr_max);
	if (rc < 0)
		pr_err("pd-bus-curr-max property missing, use default val\n");
	else
		pm_config.bus_curr_lp_lmt = pdpm->bus_curr_max;
	pr_info("pm_config.bus_curr_lp_lmt:%d\n", pm_config.bus_curr_lp_lmt);

	rc = of_property_read_u32(node,
			"mi,pd-non-ffc-bat-volt-max", &pdpm->non_ffc_bat_volt_max);

	pr_info("pdpm->non_ffc_bat_volt_max:%d\n",
				pdpm->non_ffc_bat_volt_max);

	pdpm->cp_sec_enable = of_property_read_bool(node,
				"mi,cp-sec-enable");
	pm_config.cp_sec_enable = pdpm->cp_sec_enable;

	pdpm->sc_bypass_enable = of_property_read_bool(node,
				"mi,sc_bypass_enable");

	return 0;
}

static int usbpd_pm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct usbpd_pm *pdpm;

	pr_info("%s enter\n", __func__);

	pdpm = kzalloc(sizeof(struct usbpd_pm), GFP_KERNEL);
	if (!pdpm)
		return -ENOMEM;

	__pdpm = pdpm;

	pdpm->dev = dev;

	pdpm->sc_bypass_enable = 0;

	ret = pd_policy_parse_dt(pdpm);
	if (ret < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, pdpm);

	spin_lock_init(&pdpm->psy_change_lock);

	usbpd_check_cp_psy(pdpm);
	usbpd_check_cp_sec_psy(pdpm);
	usbpd_check_usb_psy(pdpm);
	usbpd_check_charger_device(pdpm);

	pdpm->chg_consumer = charger_manager_get_by_name(&pdev->dev, "charger_port1");
	if (!pdpm->chg_consumer) {
		pr_err("%s: get charger consumer device failed\n", __func__);
		return -ENODEV;
	}
	INIT_WORK(&pdpm->cp_psy_change_work, cp_psy_change_work);
	INIT_WORK(&pdpm->usb_psy_change_work, usb_psy_change_work);
	INIT_DELAYED_WORK(&pdpm->pm_work, usbpd_pm_workfunc);

	pdpm->nb.notifier_call = usbpd_psy_notifier_cb;
	power_supply_reg_notifier(&pdpm->nb);

	pr_info("%s probe success.\n", __func__);
	return ret;
}

static int usbpd_pm_remove(struct platform_device *pdev)
{
	power_supply_unreg_notifier(&__pdpm->nb);
	cancel_delayed_work(&__pdpm->pm_work);
	cancel_work(&__pdpm->cp_psy_change_work);
	cancel_work(&__pdpm->usb_psy_change_work);

	return 0;
}

static const struct of_device_id usbpd_pm_of_match[] = {
	{ .compatible = "xiaomi,usbpd-pm", },
	{},
};

static struct platform_driver usbpd_pm_driver = {
	.driver = {
		.name = "usbpd-pm",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(usbpd_pm_of_match),
	},
	.probe = usbpd_pm_probe,
	.remove = usbpd_pm_remove,
};

module_platform_driver(usbpd_pm_driver);
MODULE_AUTHOR("bsp@xiaomi.com");
MODULE_DESCRIPTION("Xiaomi usb pd statemachine for bq");
MODULE_LICENSE("GPL");
