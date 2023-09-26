
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
};

enum {
	CP_VBUS_ERROR_NONE,
	CP_VBUS_ERROR_LOW,
	CP_VBUS_ERROR_HIGH,
};

enum {
	SC8551_CHARGE_MODE_DIV2,
	SC8551_CHARGE_MODE_BYPASS,
};

static struct pdpm_config pm_config = {
	.bat_volt_lp_lmt		= BAT_VOLT_LOOP_LMT,
	.bat_curr_lp_lmt		= BAT_CURR_LOOP_LMT + 1000,
	.bus_volt_lp_lmt		= BUS_VOLT_LOOP_LMT,
	.bus_curr_lp_lmt		= BAT_CURR_LOOP_LMT >> 1,

	.fc2_taper_current		= 2100,
	.fc2_taper_bus_current		= 1300,
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
	//find mt6360 charger
	if (!pdpm->ch1_dev) {
		pdpm->ch1_dev = get_charger_by_name("primary_chg");
		if (pdpm->ch1_dev) {
			pr_debug("%s: Found primary charger [%s]\n",
				__func__, pdpm->ch1_dev->props.alias_name);
		} else {
			pr_err("*** Error : can't find primary charger ***\n");
		}
	}

	//find bq2597x-master/standalone charger
	if (!pdpm->ch3_dev) {
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");
		if (pdpm->ch3_dev) {
			pr_debug("%s: Found tertiary charger [%s]\n",
				__func__, pdpm->ch3_dev->props.alias_name);
		} else {
			pr_err("*** Error : can't find tertiary charger ***\n");
		}
	}

	//find bq2597x-slave charger
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

/* get thermal scene from battery power supply property */
static int pd_get_batt_current_thermal_scene(struct usbpd_pm *pdpm, int *scene)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	usbpd_check_batt_psy(pdpm);

	if (!pdpm->sw_psy)
		return -ENODEV;

	rc = power_supply_get_property(pdpm->sw_psy,
				POWER_SUPPLY_PROP_CHARGE_CONTROL_SCENE, &pval);
	if (rc < 0) {
		pr_info("Couldn't get thermal scene:%d\n", rc);
		return rc;
	}

	pr_info("%s thermal_scene: %d\n", __func__, pval.intval);

	*scene = pval.intval;
	return rc;
}

/* get thermal level from battery power supply property */
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
	int thermal_level = 0, thermal_scene = 0;
	int ibus_thres = NORMAL_IBUS_THRES_MA_FOR_DUAL_BQ;

	pd_get_batt_capacity(pdpm, &capacity);
	pd_get_batt_current_thermal_level(pdpm, &thermal_level);
	pd_get_batt_current_thermal_scene(pdpm, &thermal_scene);
	pr_info("thermal(lvl:%d scene:%d) ibus_ma:%d no_need_en_slave_bq(%d cnt:%d)\n",
			thermal_level, thermal_scene, ibus_ma, pdpm->no_need_en_slave_bq, pdpm->slave_bq_disabled_check_count);

	if (thermal_scene == GAME_HIGHLOAD_CHG_SCENE)
		ibus_thres = GAME_CHG_IBUS_THRES_MA_FOR_DUAL_BQ;

	if (!pdpm->no_need_en_slave_bq
			&& pdpm->slave_bq_disabled_check_count < IBUS_THR_TO_CLOSE_SLAVE_COUNT_MAX
			&& ibus_ma <= ibus_thres
			&& pdpm->cp.sc8551_charge_mode != SC8551_CHARGE_MODE_BYPASS) {
		pdpm->slave_bq_disabled_check_count++;
		if (pdpm->slave_bq_disabled_check_count >= IBUS_THR_TO_CLOSE_SLAVE_COUNT_MAX) {
			pdpm->no_need_en_slave_bq = true;
			/* exit dual bq working due to total low ibus to avoid bq ucp */
			pr_err("ibus decrease to threshold, exit dual bq working now\n");
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
			usbpd_pm_check_cp_enabled(pdpm);
			if (!pdpm->cp.charge_enabled) {
				usbpd_pm_enable_cp(pdpm, true);
				msleep(50);
				usbpd_pm_check_cp_enabled(pdpm);
			}
		}
	} else if (pdpm->no_need_en_slave_bq
			&& capacity < CAPACITY_HIGH_THR
			&& thermal_level < MAX_THERMAL_LEVEL_FOR_DUAL_BQ
			&& ibus_ma > (ibus_thres + IBUS_THR_MA_HYS_FOR_DUAL_BQ)
			&& pdpm->cp.sc8551_charge_mode != SC8551_CHARGE_MODE_BYPASS) {
		if (!pdpm->cp_sec.charge_enabled) {
			pdpm->no_need_en_slave_bq = false;
			pr_err("ibus increase above threshold, dual bq re-working now\n");
			usbpd_pm_enable_cp_sec(pdpm, true);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}
	} else {
		pdpm->slave_bq_disabled_check_count = 0;
	}
}

/* get fastcharge mode */
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
/* get bq27z561 fastcharge mode to enable or disabled */

/* determine whether to disable cp according to jeita status */
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

	/* is input suspend is set true, do not allow bq quick charging */
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
	} else if (batt_temp <= JEITA_COOL_THR && !pdpm->jeita_triggered) {
		pdpm->jeita_triggered = true;
		return true;
	} else if ((batt_temp <= (JEITA_WARM_THR - JEITA_HYSTERESIS))
				&& (batt_temp >= (JEITA_COOL_THR + JEITA_HYSTERESIS))
			&& pdpm->jeita_triggered) {
		if (pdpm->ch1_dev)
			charger_dev_set_constant_voltage(pdpm->ch1_dev, (bat_constant_voltage * 1000));

		pdpm->jeita_triggered = false;
		return false;
	} else {
		return pdpm->jeita_triggered;
	}
}

/* get bq27z561 battery verified to enable or disabled */
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

/* get pd pps charger verified result  */
static bool pd_get_pps_charger_verified(struct usbpd_pm *pdpm)
{
	union power_supply_propval pval = {0,};
	int rc;

	if (!pdpm->usb_psy)
		return false;

	// fix zimi 65W charger, which broadcast pps when plug in.
	// and when plug in, can not get pps info because of batterysecret.
	// so need, retry to get pps info after authentic process.
	// and update pdpm->apdo_max_curr\pdpm->apdo_max_volt ...
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

#if 0
/* get bq27z561 fastcharge mode to enable or disabled */
static int pd_get_bms_charge_current_max(struct usbpd_pm *pdpm, int *fcc_ua)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	if (!pdpm->bms_psy)
		return rc;

	rc = power_supply_get_property(pdpm->bms_psy,
				POWER_SUPPLY_PROP_CURRENT_MAX, &pval);
	if (rc < 0) {
		pr_info("Couldn't get current max:%d\n", rc);
		return rc;
	}

	*fcc_ua = pval.intval;
	return rc;

}
#endif

static int usbpd_set_new_fcc_voter(struct usbpd_pm *pdpm)
{
#if 0
	int rc = 0;
	int fcc_ua = 0;

	/* No need to use bms current max so far */
	return rc;


	rc = pd_get_bms_charge_current_max(pdpm, &fcc_ua);

	if (rc < 0)
		return rc;

	if (!pdpm->fcc_votable)
		pdpm->fcc_votable = find_votable("FCC");

	if (!pdpm->fcc_votable)
		return -EINVAL;

	if (pdpm->fcc_votable)
		vote(pdpm->fcc_votable, STEP_BMS_CHG_VOTER, true, fcc_ua);

	return rc;
#endif

	return 0;
}

static void usbpd_check_cp_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->cp_psy) {
		if (pm_config.cp_sec_enable) {
			if (!pdpm->cp_master_dis_first)
				pdpm->cp_psy = power_supply_get_by_name("bq2597x-master");
			else
				pdpm->cp_psy = power_supply_get_by_name("bq2597x-slave");
		} else
			pdpm->cp_psy = power_supply_get_by_name("bq2597x-standalone");
		if (!pdpm->cp_psy)
			pr_err("cp_psy not found\n");
	}

	if (!pdpm->cp_master_dis_first) {
		if (!pdpm->ch3_dev)
			pdpm->ch3_dev = get_charger_by_name("tertiary_chg");
	} else {
		if (!pdpm->ch4_dev)
			pdpm->ch4_dev = get_charger_by_name("quaternary_chg");
	}
}

static void usbpd_check_cp_sec_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->cp_sec_psy) {
		if (!pdpm->cp_master_dis_first)
			pdpm->cp_sec_psy = power_supply_get_by_name("bq2597x-slave");
		else
			pdpm->cp_sec_psy = power_supply_get_by_name("bq2597x-master");
		if (!pdpm->cp_sec_psy)
			pr_err("cp_sec_psy not found\n");
	}

	if (!pdpm->cp_master_dis_first) {
		if (!pdpm->ch4_dev)
			pdpm->ch4_dev = get_charger_by_name("quaternary_chg");
	} else {
		if (!pdpm->ch3_dev)
			pdpm->ch3_dev = get_charger_by_name("tertiary_chg");
	}

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
	int ibus_pmic = 0;
	struct charger_device *chg_dev = NULL;
	struct charger_device *chg_sec_dev = NULL;

	pr_debug("%s enter.\n", __func__);

	if (!pdpm->cp_master_dis_first)
		chg_dev = pdpm->ch3_dev;
	else
		chg_dev = pdpm->ch4_dev;

	if (!chg_dev)
		return;

	charger_dev_get_vbat(chg_dev, &pdpm->cp.vbat_volt);
	pr_debug("%s pdpm->cp.vbat_volt is %d.\n", __func__, pdpm->cp.vbat_volt);
	charger_dev_get_ibat(chg_dev, &pdpm->cp.ibat_curr);
	charger_dev_get_vbus(chg_dev, &pdpm->cp.vbus_volt);
	charger_dev_get_ibus(chg_dev, &pdpm->cp.ibus_curr);
	charger_dev_get_bus_temp(chg_dev, &pdpm->cp.bus_temp);
	charger_dev_get_battery_temp(chg_dev, &pdpm->cp.bat_temp);
	charger_dev_get_die_temp(chg_dev, &pdpm->cp.die_temp);
	charger_dev_get_battery_present(chg_dev, &pdpm->cp.batt_pres);
	charger_dev_get_vbus_present(chg_dev, &pdpm->cp.vbus_pres);
	charger_dev_get_vbus_error_status(chg_dev, &pdpm->cp.vbus_error_status);
	charger_dev_get_chg_mode(chg_dev, &pdpm->cp.sc8551_charge_mode);
	charger_dev_get_bypass_mode_enable(chg_dev, &pdpm->cp.sc8551_bypass_charge_enable);

	cp_update_bms_ibat_vbat(pdpm);

	charger_dev_is_enabled(chg_dev, &pdpm->cp.charge_enabled);
	ret = charger_dev_get_alarm_status(chg_dev, &alarm_status);
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
	ret = charger_dev_get_fault_status(chg_dev, &fault_status);
	if (!ret) {
		pdpm->cp.bat_ovp_fault = !!(fault_status & BAT_OVP_FAULT_MASK);
		pdpm->cp.bat_ocp_fault = !!(fault_status & BAT_OCP_FAULT_MASK);
		pdpm->cp.bus_ovp_fault = !!(fault_status & BUS_OVP_FAULT_MASK);
		pdpm->cp.bus_ocp_fault = !!(fault_status & BUS_OCP_FAULT_MASK);
		pdpm->cp.bat_therm_fault = !!(fault_status & BAT_THERM_FAULT_MASK);
		pdpm->cp.bus_therm_fault = !!(fault_status & BUS_THERM_FAULT_MASK);
		pdpm->cp.die_therm_fault = !!(fault_status & DIE_THERM_FAULT_MASK);
	}

	ret = charger_dev_get_reg_status(chg_dev, &reg_status);
	if (!ret) {
		pdpm->cp.vbat_reg = !!(reg_status & VBAT_REG_STATUS_MASK);
		pdpm->cp.ibat_reg = !!(reg_status & IBAT_REG_STATUS_MASK);
	}

	charger_manager_get_ibus(&ibus_pmic);

	/* update sec_cp if exist */
	if (pm_config.cp_sec_enable && pdpm->cp_sec_psy) {
		if (!pdpm->cp_master_dis_first)
			chg_sec_dev = pdpm->ch4_dev;
		else
			chg_sec_dev = pdpm->ch3_dev;

		charger_dev_get_ibus(chg_sec_dev, &pdpm->cp_sec.ibus_curr);
		charger_dev_is_enabled(chg_sec_dev, &pdpm->cp_sec.charge_enabled);
		charger_dev_get_chg_mode(chg_sec_dev, &pdpm->cp_sec.sc8551_charge_mode);
	}

	pr_debug("cp: vbat:%d, ibat:%d, vbus:%d(hi_lo:%d), ibus(cp1:%d cp2:%d pmic:%d), present(bat:%d bus:%d), \
			chg_en(cp1:%d cp2:%d), bypass_en:%d, chg_mode:(cp1:%d cp2:%d), tbus:%d, tbat:%d, tdie:%d\n",
			pdpm->cp.vbat_volt, pdpm->cp.ibat_curr, pdpm->cp.vbus_volt, pdpm->cp.vbus_error_status,
			pdpm->cp.ibus_curr, pdpm->cp_sec.ibus_curr, ibus_pmic, pdpm->cp.batt_pres, pdpm->cp.vbus_pres,
			pdpm->cp.charge_enabled, pdpm->cp_sec.charge_enabled, pdpm->cp.sc8551_bypass_charge_enable,
			pdpm->cp.sc8551_charge_mode, pdpm->cp_sec.sc8551_charge_mode,
			pdpm->cp.bus_temp, pdpm->cp.bat_temp, pdpm->cp.die_temp);
}

static int usbpd_pm_enable_cp(struct usbpd_pm *pdpm, bool enable)
{
	int ret = 0;
	struct charger_device *chg_dev = NULL;

	if (!pdpm->chg_consumer)
		pdpm->chg_consumer = charger_manager_get_by_name(pdpm->dev, "charger_port1");

	if ((enable) && (pdpm->chg_consumer)) {
		pr_info("%s before enable cp, should disable pmic power path.\n", __func__);
		charger_manager_enable_power_path(pdpm->chg_consumer, MAIN_CHARGER, !enable);
	}

	if (!pdpm->ch1_dev)
			pdpm->ch1_dev = get_charger_by_name("primary_chg");
	if (!pdpm->ch3_dev)
			pdpm->ch3_dev = get_charger_by_name("tertiary_chg");
	if (!pdpm->ch4_dev)
			pdpm->ch4_dev = get_charger_by_name("quaternary_chg");
	if (!pdpm->cp_master_dis_first)
		chg_dev = pdpm->ch3_dev;
	else
		chg_dev = pdpm->ch4_dev;


	pr_debug("%s enable = %d.\n", __func__, enable);

	ret = charger_dev_enable(chg_dev, enable);
	if (!ret && pdpm->ch1_dev) {
		charger_dev_enable(pdpm->ch1_dev, !enable);
		if (!enable)
			charger_dev_set_input_current(pdpm->ch1_dev, 3000000);
	}

	return ret;

}

static int usbpd_pm_enable_cp_sec(struct usbpd_pm *pdpm, bool enable)
{
	int ret = 0;
	struct charger_device *chg_sec_dev = NULL;

	if (!pdpm->ch3_dev)
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");
	if (!pdpm->ch4_dev)
		pdpm->ch4_dev = get_charger_by_name("quaternary_chg");

	if (!pdpm->cp_master_dis_first)
		chg_sec_dev = pdpm->ch4_dev;
	else
		chg_sec_dev = pdpm->ch3_dev;

	pr_debug("%s enable = %d.\n", __func__, enable);
	charger_dev_enable(chg_sec_dev, enable);

	return ret;
}

static int usbpd_pm_check_cp_enabled(struct usbpd_pm *pdpm)
{
	int ret = 0;
	bool enable = false;
	struct charger_device *chg_dev = NULL;

	if (!pdpm->ch1_dev)
		pdpm->ch1_dev = get_charger_by_name("primary_chg");
	if (!pdpm->ch3_dev)
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");
	if (!pdpm->ch4_dev)
		pdpm->ch4_dev = get_charger_by_name("quaternary_chg");
	if (!pdpm->chg_consumer)
		pdpm->chg_consumer = charger_manager_get_by_name(pdpm->dev, "charger_port1");

	if (!pdpm->cp_master_dis_first)
		chg_dev = pdpm->ch3_dev;
	else
		chg_dev = pdpm->ch4_dev;

	ret = charger_dev_is_enabled(chg_dev, &enable);
	if (!ret) {
		pdpm->cp.charge_enabled = !!enable;
		if (pdpm->cp.charge_enabled == 0 && pdpm->ch1_dev && pdpm->chg_consumer) {
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
	int ret = 0;
	bool enable = false;
	struct charger_device *chg_sec_dev = NULL;

	if (!pdpm->ch3_dev)
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");
	if (!pdpm->ch4_dev)
		pdpm->ch4_dev = get_charger_by_name("quaternary_chg");

	if (!pdpm->cp_master_dis_first)
		chg_sec_dev = pdpm->ch4_dev;
	else
		chg_sec_dev = pdpm->ch3_dev;

	ret = charger_dev_is_enabled(chg_sec_dev, &enable);
	if (!ret)
		pdpm->cp_sec.charge_enabled = !!enable;

	pr_debug("pdpm->cp_sec.charge_enabled:%d\n", pdpm->cp_sec.charge_enabled);
	return ret;
}

static int usbpd_pm_set_cp_charge_mode(struct usbpd_pm *pdpm, int mode)
{
	int ret1 = 0, ret2 = 0;

	usbpd_check_cp_psy(pdpm);
	if (!pdpm->cp_psy)
		return -ENODEV;
	if (pm_config.cp_sec_enable) {
		usbpd_check_cp_sec_psy(pdpm);
		if (!pdpm->cp_sec_psy)
			return -ENODEV;
	}

	ret1 = charger_dev_set_chg_mode(pdpm->ch3_dev, mode);
	if (pm_config.cp_sec_enable)
		ret2 = charger_dev_set_chg_mode(pdpm->ch4_dev, mode);
	if (ret1 < 0 || ret2 < 0)
		pr_err("cp set charge mode failed, ret=%d,%d", ret1, ret2);

	return min(ret1, ret2);
}

static int usbpd_pm_update_cp_charge_mode(struct usbpd_pm *pdpm)
{
	int fcc = 0, chg_mode = 0;
	int thermal_level = 0, thermal_scene = 0;

	usbpd_check_bms_psy(pdpm);
	if (!pdpm->bms_psy)
		return -ENODEV;

	if (pdpm->cp.sc8551_bypass_charge_enable == 0) {
		chg_mode = SC8551_CHARGE_MODE_DIV2;
	} else {
		pd_get_batt_current_thermal_scene(pdpm, &thermal_scene);
		fcc = usbpd_get_effective_fcc_val(pdpm);

		if (fcc <= BYPASS_FCC_ENTER_THRES) {
			if (thermal_scene == GAME_CHG_SCENE)
				chg_mode = SC8551_CHARGE_MODE_BYPASS;
			else
				chg_mode = SC8551_CHARGE_MODE_DIV2;
		} else if (fcc > BYPASS_FCC_EXIT_THRES)
			chg_mode = SC8551_CHARGE_MODE_DIV2;
		else
			chg_mode = pdpm->cp.sc8551_charge_mode;
	}

	pr_info("bypass_en:%d, cp_chg_mode:%d:%d, therm_scene:%d, fcc:%d, vbat:%d\n",
			pdpm->cp.sc8551_bypass_charge_enable,
			pdpm->cp.sc8551_charge_mode, chg_mode,
			thermal_scene, fcc, pdpm->cp.vbat_volt);

	return chg_mode;
}

static int usbpd_pm_check_cp_charge_mode(struct usbpd_pm *pdpm)
{
	int ret1 = 0, ret2 = 0;

	usbpd_check_cp_psy(pdpm);
	if (!pdpm->cp_psy)
		return -ENODEV;
	if (pm_config.cp_sec_enable) {
		usbpd_check_cp_sec_psy(pdpm);
		if (!pdpm->cp_sec_psy)
			return -ENODEV;
	}

	ret1 = charger_dev_get_chg_mode(pdpm->ch3_dev, &pdpm->cp.sc8551_charge_mode);
	if (pm_config.cp_sec_enable)
		ret2 = charger_dev_get_chg_mode(pdpm->ch4_dev, &pdpm->cp_sec.sc8551_charge_mode);

	return min(ret1, ret2);
}

static int usbpd_pm_switch_cp_charge_mode(struct usbpd_pm *pdpm)
{
	int ret = 0;
	int chg_mode = 0;

	if (pdpm->cp.sc8551_bypass_charge_enable != 1)
		return -EPERM;

	ret = usbpd_pm_check_cp_charge_mode(pdpm);
	if (ret < 0) {
		pr_err("Failed to check cp charge mode, ret:%d.\n", ret);
		return ret;
	}

	chg_mode = usbpd_pm_update_cp_charge_mode(pdpm);
	if (chg_mode != pdpm->cp.sc8551_charge_mode) {
		if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled) {
			usbpd_pm_enable_cp_sec(pdpm, false);
			msleep(30);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}

		if (pdpm->cp.charge_enabled) {
			usbpd_pm_enable_cp(pdpm, false);
			msleep(30);
			usbpd_pm_check_cp_enabled(pdpm);
		}

		usbpd_pm_set_cp_charge_mode(pdpm, chg_mode);
		pr_info("cp charge mode switch from %d to %d.\n",
				pdpm->cp.sc8551_charge_mode, chg_mode);

		ret = usbpd_pm_check_cp_charge_mode(pdpm);
		if (ret < 0)
			pr_err("Failed to check cp charge mode, ret:%d.\n", ret);

		ret = 1;
	}

	return ret;
}

static int usbpd_pm_enable_sw(struct usbpd_pm *pdpm, bool enable)
{
	int ret = 0;

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
	int ret = 0;
	//union power_supply_propval val = {0,};

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
	struct pps_cap_bq cap = {0, };
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

	//workaround for 33w GaN
	if (pdpm->apdo_max_curr == PPS_CURR_MAX_33W)
		pdpm->apdo_max_curr = PPS_CURR_MAX_33W - PPS_CURR_HYS;

	if (pdpm->pps_supported) {
		pr_info("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d, apdo_max:%d\n",
				pdpm->apdo_selected_pdo,
				pdpm->apdo_max_volt,
				pdpm->apdo_max_curr,
				apdo_max_power);
		if (pdpm->apdo_max_curr <= LOW_POWER_PPS_CURR_THR)
			pdpm->apdo_max_curr = XIAOMI_LOW_POWER_PPS_CURR_MAX;
		pval.intval = apdo_max_power / 1000000;
		pval.intval = min(pdpm->pd_power_max, pval.intval);
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

	/* we will use it later, to do */
	return;

	ret = usbpd_get_pps_status(pdpm->pd, &status);

	if (!ret) {
		pr_info("get_pps_status: status_db :0x%x\n", status);
		/*TODO: check byte order to insure data integrity*/
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
	int ret = 0;
	int steps;
	int sw_ctrl_steps = 0, hw_ctrl_steps = 0;
	int step_vbat = 0, step_ibus = 0, step_ibat = 0, step_bat_reg = 0;
	int fcc_ibatt_diff = 0, icl_ibus_diff = 0;
	int ibus_total = 0, ibus_max_lim = 0;
	int effective_fcc_val = 0, effective_fcc_taper = 0;
	int thermal_level = 0;
	int taper_hys_mv = 0;
	bool is_fastcharge_mode = false;
	static int ibatt_count = 0;
	static int ibat_now_ma_before = 0;
	static int curr_fcc_limit, curr_ibus_limit, ibus_limit;

	is_fastcharge_mode = pd_get_fastcharge_mode_enabled(pdpm);
	if (is_fastcharge_mode)
		pm_config.bat_volt_lp_lmt = pdpm->bat_volt_max;
	else
		pm_config.bat_volt_lp_lmt = pdpm->non_ffc_bat_volt_max;

	//usbpd_set_new_fcc_voter(pdpm);

	if (!(pdpm->cp.sc8551_charge_mode))
		taper_hys_mv = BQ_TAPER_HYS_MV;
	else
		taper_hys_mv = SC8551_TAPER_HYS_MV;

	/* reduce bus current in cv loop */
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

	effective_fcc_val = usbpd_get_effective_fcc_val(pdpm);
	if (effective_fcc_val >= 0) {
		curr_fcc_limit = min(pm_config.bat_curr_lp_lmt, effective_fcc_val);
		if (pm_config.cp_sec_enable) {
			/* only one cp works, maxium target fcc should limit to 6A */
			if (pdpm->no_need_en_slave_bq)
				curr_fcc_limit = min(curr_fcc_limit, FCC_MAX_MA_FOR_MASTER_BQ);
		}
		if (pdpm->cp.sc8551_charge_mode == SC8551_CHARGE_MODE_BYPASS) {
			curr_fcc_limit = min(curr_fcc_limit, MAX_BYPASS_CURRENT_MA);
		}

		/* curr_ibus_limit should compare with apdo_max_curr here */
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
			if (pdpm->cp.sc8551_charge_mode == SC8551_CHARGE_MODE_DIV2) {
				/* compensate 50mA for target ibus_limit for bq adc accurancy is below standard */
				ibus_limit = curr_ibus_limit + 100;
			} else {
					ibus_limit = curr_ibus_limit;
			}
		}
	}

	pr_info("chg_mode:%d, curr_ibus_limit:%d, ibus_limit:%d, bat_curr_lp_lmt:%d, effective_fcc_val:%d, apdo_max_curr:%d\n",
		pdpm->cp.sc8551_charge_mode, curr_ibus_limit, ibus_limit,
		pm_config.bat_curr_lp_lmt, effective_fcc_val, pdpm->apdo_max_curr);

	/* battery voltage loop*/
	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt)
		step_vbat = -pm_config.fc2_steps;
	else if (pdpm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - 10)
		step_vbat = pm_config.fc2_steps;;

	/* battery charge current loop*/
	/* BQG batt sampling frequency is 1S*/
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

	/* bus current loop*/
	ibus_total = pdpm->cp.ibus_curr;
	if (pm_config.cp_sec_enable) {
		ibus_total += pdpm->cp_sec.ibus_curr;
		pd_bq_check_ibus_to_enable_dual_bq(pdpm, ibus_total);
	}

	if (ibus_total < ibus_limit - 50)
		step_ibus = pm_config.fc2_steps;
	else if (ibus_total > ibus_limit)
		step_ibus = -pm_config.fc2_steps;

	/* hardware regulation loop*/
	if (pdpm->cp.vbat_reg)
		step_bat_reg = 3 * (-pm_config.fc2_steps);
	else
		step_bat_reg = pm_config.fc2_steps;

	sw_ctrl_steps = min(min(step_vbat, step_ibus), step_ibat);
	sw_ctrl_steps = min(sw_ctrl_steps, step_bat_reg);

	pr_info("vbus:%d, ibus:%d(m:%d,s:%d), vbat:%d(reg:%d), ibat:%d(cnt:%d idx:%d)\n",
			pdpm->cp.vbus_volt, ibus_total, pdpm->cp.ibus_curr, pdpm->cp_sec.ibus_curr,
			pdpm->cp.vbat_volt, pdpm->cp.vbat_reg, pdpm->cp.ibat_curr, ibatt_count, pdpm->ibatt_count_index);
	pr_info("sw_ctrl_steps:%d, step_vbat:%d, step_ibus:%d, step_ibat:%d, step_bat_reg:%d\n",
			sw_ctrl_steps, step_vbat, step_ibus, step_ibat, step_bat_reg);

	/* hardware alarm loop */
	if (pdpm->cp.bus_ocp_alarm || pdpm->cp.bus_ovp_alarm)
		hw_ctrl_steps = -pm_config.fc2_steps;
	else
		hw_ctrl_steps = pm_config.fc2_steps;
	pr_info("hw_ctrl_steps:%d\n", hw_ctrl_steps);
	/* check if cp disabled due to other reason*/
	usbpd_pm_check_cp_enabled(pdpm);

	if (pm_config.cp_sec_enable)
		usbpd_pm_check_cp_sec_enabled(pdpm);

	pd_get_batt_current_thermal_level(pdpm, &thermal_level);
	pdpm->is_temp_out_fc2_range = pd_disable_cp_by_jeita_status(pdpm);
	pr_info("is_temp_out_fc2_range = %d, thermal_level = %d\n", pdpm->is_temp_out_fc2_range, thermal_level);

	if (pdpm->cp.bat_therm_fault) { /* battery overheat, stop charge*/
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
		return PM_ALGO_RET_OTHER_FAULT; /* go to switch, and try to ramp up*/
	} else if (!pdpm->cp.charge_enabled
			|| (pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled && !pdpm->no_need_en_slave_bq)) {
		pr_info("cp.charge_enabled:%d, cp_sec.charge_enabled:%d\n",
				pdpm->cp.charge_enabled, pdpm->cp_sec.charge_enabled);
		return PM_ALGO_RET_CHG_DISABLED;
	}

	/* charge pump taper charge */
	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - TAPER_VOL_HYS
			&& (((pdpm->cp.sc8551_charge_mode != SC8551_CHARGE_MODE_BYPASS)
				&& (pdpm->cp.ibat_curr < pm_config.fc2_taper_current))
			|| ((pdpm->cp.sc8551_charge_mode == SC8551_CHARGE_MODE_BYPASS)
				&& (ibus_total < pm_config.fc2_taper_bus_current)))) {
		if (fc2_taper_timer++ > TAPER_TIMEOUT) {
			pr_info("charge pump taper charging done\n");
			fc2_taper_timer = 0;
			return PM_ALGO_RET_TAPER_DONE;
		}
	} else {
		fc2_taper_timer = 0;
	}

	/* sc8551 mode switch*/
	ret = usbpd_pm_switch_cp_charge_mode(pdpm);
	if (ret > 0) {
		pr_info("charge mode changed, re-run statemachine\n");
		return PM_ALGO_RET_CHG_DISABLED;
	}

	/*TODO: customer can add hook here to check system level
	 * thermal mitigation*/

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

	if (pdpm->apdo_max_volt == PPS_VOL_MAX)
		pdpm->apdo_max_volt = pdpm->apdo_max_volt - PPS_VOL_HYS;

	if (pdpm->request_voltage > pdpm->apdo_max_volt)
		pdpm->request_voltage = pdpm->apdo_max_volt;

	/*if (pdpm->adapter_voltage > 0
			&& pdpm->request_voltage > pdpm->adapter_voltage + 500)
		pdpm->request_voltage = pdpm->adapter_voltage + 500; */

	if (pdpm->request_current < curr_ibus_limit)
		pdpm->request_current = min(pdpm->apdo_max_curr, curr_ibus_limit);

	pr_info("steps:%d, pdpm->request_voltage:%d, pdpm->request_current:%d\n",
			steps, pdpm->request_voltage, pdpm->request_current);

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
#if 1
	pr_info("state change:%s -> %s\n",
		pm_str[pdpm->state], pm_str[state]);
#endif
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
	int thermal_level = 0, capacity = 0;
	//union power_supply_propval val = {0,};
	static int curr_fcc_lmt, curr_ibus_lmt;

	pr_debug("%s enter.\n", __func__);

	if (!pdpm->ch1_dev)
		pdpm->ch1_dev = get_charger_by_name("primary_chg");

	switch (pdpm->state) {
	case PD_PM_STATE_ENTRY:
		stop_sw = false;
		recover = false;

		pd_get_batt_current_thermal_level(pdpm, &thermal_level);
		pdpm->is_temp_out_fc2_range = pd_disable_cp_by_jeita_status(pdpm);
		pr_info("is_temp_out_fc2_range:%d\n", pdpm->is_temp_out_fc2_range);
		pd_get_batt_capacity(pdpm, &capacity);

		//usbpd_set_new_fcc_voter(pdpm); // update new fcc from bms charge current

		effective_fcc_val = usbpd_get_effective_fcc_val(pdpm);

		if (effective_fcc_val > 0) {
			curr_fcc_lmt = min(pm_config.bat_curr_lp_lmt, effective_fcc_val);
			if (pdpm->cp.sc8551_charge_mode == SC8551_CHARGE_MODE_BYPASS)
				curr_ibus_lmt = curr_fcc_lmt;
			else
				curr_ibus_lmt = curr_fcc_lmt >> 1;
			pr_info("curr_ibus_lmt:%d\n", curr_ibus_lmt);
		}

		if (pdpm->cp.vbat_volt < pm_config.min_vbat_for_cp) {
			pd_set_bq_charge_done(pdpm, true);
			pr_info("batt_volt %d, waiting...\n", pdpm->cp.vbat_volt);
		} else if ((pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - VBAT_HIGH_FOR_FC_HYS_MV
				&& !pdpm->is_temp_out_fc2_range
				&& pdpm->cp.sc8551_charge_mode != SC8551_CHARGE_MODE_BYPASS)
				|| capacity >= CAPACITY_TOO_HIGH_THR) {
			pr_info("vbat(%d) or soc(%d) too high, charging with switch charger\n",
					pdpm->cp.vbat_volt, capacity);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		} else if (!pd_get_bms_digest_verified(pdpm)) {
			pd_set_bq_charge_done(pdpm, true);
			pr_info("bms digest is not verified, waiting...\n");
		} else if (effective_fcc_val < START_DRIECT_CHARGE_FCC_MIN_THR
				&& pdpm->cp.sc8551_charge_mode != SC8551_CHARGE_MODE_BYPASS) {
			pd_set_bq_charge_done(pdpm, true);
			pr_info("effective fcc is too low, waiting...\n");
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
		usbpd_pm_switch_cp_charge_mode(pdpm);
		if (pdpm->cp.sc8551_bypass_charge_enable == 1
				&& pdpm->cp.sc8551_charge_mode == SC8551_CHARGE_MODE_BYPASS) {
			pdpm->request_voltage = pdpm->cp.vbat_volt + BUS_VOLT_INIT_UP;
			pdpm->request_current = min(pdpm->apdo_max_curr, curr_ibus_lmt);
			pdpm->request_current = min(pdpm->request_current, MAX_BYPASS_CURRENT_MA);
		} else {
			curr_ibus_lmt = curr_fcc_lmt >> 1;
			pdpm->request_voltage = pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP;
			pdpm->request_current = min(pdpm->apdo_max_curr, curr_ibus_lmt);
		}

		adapter_set_cap_start_bq(pdpm->request_voltage, pdpm->request_current);
		pr_debug("request_voltage:%d, request_current:%d\n",
				pdpm->request_voltage, pdpm->request_current);

		usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);

		tune_vbus_retry = 0;
		break;

	case PD_PM_STATE_FC2_ENTRY_2:
		pr_info("bus_err_st:%d, req_vol:%dmV, cur_vol:%d, req_curr:%d, vbat:%d, retry:%d\n",
				pdpm->cp.vbus_error_status, pdpm->request_voltage, pdpm->cp.vbus_volt,
				pdpm->request_current, pdpm->cp.vbat_volt, tune_vbus_retry);
		if (pdpm->cp.vbus_error_status == CP_VBUS_ERROR_LOW
				|| pdpm->cp.vbus_error_status == CP_VBUS_ERROR_HIGH
				|| ((pdpm->cp.vbus_volt < pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP - 50)
					&& (pdpm->cp.sc8551_charge_mode != SC8551_CHARGE_MODE_BYPASS))
				|| ((pdpm->cp.vbus_volt < pdpm->cp.vbat_volt + BUS_VOLT_INIT_UP)
					&& pdpm->cp.sc8551_charge_mode == SC8551_CHARGE_MODE_BYPASS)) {
			tune_vbus_retry++;
			if (pdpm->cp.vbus_error_status == CP_VBUS_ERROR_HIGH)
				pdpm->request_voltage -= STEP_MV;
			else
				pdpm->request_voltage += STEP_MV;

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
		if (!pdpm->cp.vbus_pres) {
			pr_info("Adapter plugout abnormally, exit pd_pm!\n");
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}

		if (pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled) {
			if (pdpm->cp.sc8551_charge_mode == SC8551_CHARGE_MODE_BYPASS) {
				usbpd_pm_enable_cp_sec(pdpm, true);
				msleep(30);
				usbpd_pm_check_cp_sec_enabled(pdpm);
			} else {
				pd_get_batt_current_thermal_level(pdpm, &thermal_level);
				pd_get_batt_capacity(pdpm, &capacity);
				if (thermal_level < MAX_THERMAL_LEVEL_FOR_DUAL_BQ && capacity < CAPACITY_HIGH_THR) {
					usbpd_pm_enable_cp_sec(pdpm, true);
					msleep(30);
					usbpd_pm_check_cp_sec_enabled(pdpm);
				} else
					pdpm->no_need_en_slave_bq = true;  
			}
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
#if 0
		if (pdpm->cp.vbat_volt < pm_config.min_vbat_for_cp - 400) {
			usbpd_pm_move_state(PD_PM_STATE_SW_ENTRY);
			break;
		}
#endif
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
		} else {
			ret = adapter_set_cap_bq(pdpm->request_voltage, pdpm->request_current);
			if (ret == ADAPTER_ADJUST) {
				pr_err("Adapter deny for invalid request, need to re-evaluate src caps!\n");
				usbpd_pm_evaluate_src_caps(pdpm);
				usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
			}
			pr_info("request_voltage:%d, request_current:%d\n", pdpm->request_voltage, pdpm->request_current);
		}
		/*stop second charge pump if either of ibus is lower than 400ma during CV*/
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
	}
}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval pval = {0, };

	cancel_delayed_work_sync(&pdpm->pm_work);
	usbpd_check_usb_psy(pdpm);

	if (pdpm->cp.charge_enabled) {
		pr_info("%s: cp need to disable when cable plug out\n", __func__);
		usbpd_pm_enable_cp(pdpm, false);
		usbpd_pm_check_cp_enabled(pdpm);
	}

	if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled) {
		pr_info("%s: cp_sec need to disable when cable plug out\n", __func__);
		usbpd_pm_enable_cp_sec(pdpm, false);
		usbpd_pm_check_cp_sec_enabled(pdpm);
	}

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
#if 0
	union power_supply_propval val = {0,};
	bool ac_pres = pdpm->cp.vbus_pres;
	int ret;

	if (!pdpm->cp_psy)
		return;

	ret = power_supply_get_property(pdpm->cp_psy, POWER_SUPPLY_PROP_TI_VBUS_PRESENT, &val);
	if (!ret)
		pdpm->cp.vbus_pres = val.intval;

	if (!ac_pres && pdpm->cp.vbus_pres)
		schedule_delayed_work(&pdpm->pm_work, 0);
#endif
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

	if (!pdpm->ch3_dev)
		pdpm->ch3_dev = get_charger_by_name("tertiary_chg");

	charger_dev_get_vbus_present(pdpm->ch3_dev, &pdpm->cp.vbus_pres);

	if (pdpm->cp.vbus_pres) {
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

	rc = of_property_read_u32(node, "mi,pd-bat-volt-max", &pdpm->bat_volt_max);
	if (rc < 0)
		pr_err("pd-bat-volt-max property missing, use default val\n");
	else
		pm_config.bat_volt_lp_lmt = pdpm->bat_volt_max;
	pr_info("pm_config.bat_volt_lp_lmt:%d\n", pm_config.bat_volt_lp_lmt);

	rc = of_property_read_u32(node, "mi,pd-bat-curr-max", &pdpm->bat_curr_max);
	if (rc < 0)
		pr_err("pd-bat-curr-max property missing, use default val\n");
	else
		pm_config.bat_curr_lp_lmt = pdpm->bat_curr_max;
	pr_info("pm_config.bat_curr_lp_lmt:%d\n", pm_config.bat_curr_lp_lmt);

	rc = of_property_read_u32(node, "mi,pd-bus-volt-max", &pdpm->bus_volt_max);
	if (rc < 0)
		pr_err("pd-bus-volt-max property missing, use default val\n");
	else
		pm_config.bus_volt_lp_lmt = pdpm->bus_volt_max;
	pr_info("pm_config.bus_volt_lp_lmt:%d\n", pm_config.bus_volt_lp_lmt);

	rc = of_property_read_u32(node, "mi,pd-bus-curr-max", &pdpm->bus_curr_max);
	if (rc < 0)
		pr_err("pd-bus-curr-max property missing, use default val\n");
	else
		pm_config.bus_curr_lp_lmt = pdpm->bus_curr_max;
	pr_info("pm_config.bus_curr_lp_lmt:%d\n", pm_config.bus_curr_lp_lmt);

	rc = of_property_read_u32(node, "mi,pd-non-ffc-bat-volt-max", &pdpm->non_ffc_bat_volt_max);
	pr_info("pdpm->non_ffc_bat_volt_max:%d\n", pdpm->non_ffc_bat_volt_max);

	pdpm->cp_sec_enable = of_property_read_bool(node, "mi,cp-sec-enable");
	pm_config.cp_sec_enable = pdpm->cp_sec_enable;

	pdpm->cp_master_dis_first = of_property_read_bool(node, "mi,cp-master-disable-fisrt");

	rc = of_property_read_u32(node, "mi,pd-power-max", &pdpm->pd_power_max);
	pr_info("pd-power-max:%d\n", pdpm->pd_power_max);

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
MODULE_AUTHOR("Fei Jiang<jiangfei1@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi usb pd statemachine for bq");
MODULE_LICENSE("GPL");
