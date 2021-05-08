
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

#define PD_SRC_PDO_TYPE_FIXED		0
#define PD_SRC_PDO_TYPE_BATTERY		1
#define PD_SRC_PDO_TYPE_VARIABLE	2
#define PD_SRC_PDO_TYPE_AUGMENTED	3

#define BATT_MAX_CHG_VOLT		4480
#define BATT_FAST_CHG_CURR		6000
#define BUS_MIVR_THRESHOLD		4200
#define	BUS_OVP_THRESHOLD		12000
#define	BUS_OVP_ALARM_THRESHOLD		9500

#define BUS_VOLT_INIT_UP		300

#define BAT_VOLT_LOOP_LMT		BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT		BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT		BUS_OVP_THRESHOLD

#define PM_WORK_RUN_NORMAL_INTERVAL		500
#if defined(CONFIG_KERNEL_CUSTOM_FACTORY)
#define PM_WORK_RUN_QUICK_INTERVAL		100
#else
#define PM_WORK_RUN_QUICK_INTERVAL		200
#endif
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
	CP_VBUS_ERROR_HIGHT,
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
//struct pdc_data *pdc_data_config;

struct charger_device *ch1_dev = NULL;
struct charger_device *ch2_dev = NULL;
static struct charger_consumer *chg_consumer = NULL;

static int usbpd_pm_check_cp_enabled(struct usbpd_pm *pdpm);

bool bq2597x_load = false;
bool ln8000_load = false;
void set_bq2597x_load_flag(bool bqflag)
{
	bq2597x_load = bqflag;
}
bool get_bq2597x_load_flag(void)
{
	return bq2597x_load;
}
void set_ln8000_load_flag(bool lnflag)
{
	ln8000_load = lnflag;
}
bool get_ln8000_load_flag(void)
{
	return ln8000_load;
}
EXPORT_SYMBOL(set_bq2597x_load_flag);
EXPORT_SYMBOL(get_bq2597x_load_flag);
EXPORT_SYMBOL(set_ln8000_load_flag);
EXPORT_SYMBOL(get_ln8000_load_flag);
/* 2021.01.21 longcheer jiangshitian change for mic noise begin */
#if defined(CONFIG_HS_MIC_RECORD_NOISE_PD_CHG)
bool mic_exist = false; // true: mic plugin  false: mic plugout; default false
bool chg_exist = false; // true: charge plugin  false: charge plugout; default false
bool mic_chg_exist = false;//ture:mic and chg both plug in  false:not both plug in; default false
bool need_switch_IC = false;//true:need switch IC;false:do not need switch IC
SWITCH_CHG_IC enum_switch_CHG_IC = CHG_TYPE_NONE;

/* boot type definitions */
typedef enum {
	NORMAL_BOOT = 0,
	META_BOOT = 1,
	RECOVERY_BOOT = 2,
	SW_REBOOT = 3,
	FACTORY_BOOT = 4,
	ADVMETA_BOOT = 5,
	ATE_FACTORY_BOOT = 6,
	ALARM_BOOT = 7,
	KERNEL_POWER_OFF_CHARGING_BOOT = 8,
	LOW_POWER_OFF_CHARGING_BOOT = 9,
	FASTBOOT = 99,
	DOWNLOAD_BOOT = 100,
	UNKNOWN_BOOT
} BOOTMODE;

static void set_mic_exist_flag(bool micflag)
{
	mic_exist = micflag;
}

static void set_switch_IC_flag(bool needswitch)
{
	need_switch_IC = needswitch;
}

static bool get_switch_IC_flag(void)
{
	return need_switch_IC;
}

void set_chg_exist_flag(bool chgflag)
{
	chg_exist = chgflag;
}

bool get_chg_exist_flag(void)
{
	return chg_exist;
}

static bool get_mic_chg_exist_flag(void)
{
	if((true == mic_exist)&&(true == chg_exist))
	{
		mic_chg_exist = true;
	}
	else
	{
		mic_chg_exist = false;
	}

	return mic_chg_exist;
}


EXPORT_SYMBOL(set_chg_exist_flag);
EXPORT_SYMBOL(get_chg_exist_flag);

static bool kpoc_charge_check(void)
{
	unsigned int boot_mode = get_boot_mode();

	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT
	    || boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		return true;
	}

	return false;
}

static bool enable_switch_charge_PD(void)
{
	bool enable = true;
	#if defined(CONFIG_TARGET_PROJECT_K7B)
	if(0 == get_board_new_version())
	#endif
	{
		pr_info("%s  %d  get_mic_chg_exist_flag=%d enum_switch_CHG_IC=%d......\n", __func__, __LINE__,get_mic_chg_exist_flag(),enum_switch_CHG_IC);
		if(kpoc_charge_check())
		{
			enable = true;
		}
		else
		{
			if(true == get_mic_chg_exist_flag())
			{
				enable = false;
				enum_switch_CHG_IC = CHG_TYPE_NORMAL;
			}
			else
			{
				enable = true;
				enum_switch_CHG_IC = CHG_TYPE_PD;
			}
		}
	}
	return enable;
}

void switch_charge_IC_GPL(bool micflag) //micflag=1: mic plugin  micflag=0: mic plugout
{
	pr_info("%s  %d  get_board_new_version = %d......\n", __func__, __LINE__, get_board_new_version());
#if defined(CONFIG_TARGET_PROJECT_K7B)
	if(0 == get_board_new_version())
#endif
	{
		set_mic_exist_flag(micflag);
		set_switch_IC_flag(true);
	}
}
EXPORT_SYMBOL(switch_charge_IC_GPL);

#endif
/* 2021.01.21 longcheer jiangshitian change for mic noise end */

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

static int pd_set_bq_charge_done(struct usbpd_pm *pdpm ,bool enable)
{
	int ret;
	union power_supply_propval val = {0,};

	if (pdpm->cp_psy == NULL)
		return -ENODEV;

	val.intval = enable;
	ret = power_supply_set_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_BQ_CHARGE_DONE, &val);
	if (ret < 0)
		pr_err("set bq charge done failed, ret=%d", ret);

	return ret;
}

static int pd_get_hv_charge_enable(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval pval = {0,};

	if (pdpm->cp_psy == NULL)
		return -ENODEV;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_HV_CHARGE_ENABLED, &pval);
	if (ret < 0)
		pr_err("get hv_charge_enable failed, ret=%d", ret);

	return pval.intval;
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
		pr_info("Couldn't get fastcharge mode:%d\n", rc);
		return rc;
	}

	pr_err("pval.intval: %d\n", pval.intval);
	*level = pval.intval;

	return rc;
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
	int warm_thres, cool_thres;

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

	if (!ch1_dev)
		ch1_dev = get_charger_by_name("primary_chg");

	if (pdpm->cp.sc8551_charge_mode != NOT_SUPPORT) {
		warm_thres = JEITA_BYPASS_WARM_DISABLE_CP_THR;
		cool_thres = JEITA_BYPASS_COOL_DISABLE_CP_THR;
	} else {
		warm_thres = JEITA_WARM_DISABLE_CP_THR;
		cool_thres = JEITA_COOL_DISABLE_CP_THR;
	}

	if (batt_temp >= warm_thres && !pdpm->jeita_triggered) {
		pdpm->jeita_triggered = true;
		return true;
	} else if (batt_temp <= cool_thres && !pdpm->jeita_triggered) {
		pdpm->jeita_triggered = true;
		return true;
	} else if ((batt_temp <= (warm_thres - JEITA_HYSTERESIS))
			&& (batt_temp >= (cool_thres + JEITA_HYSTERESIS))
			&& pdpm->jeita_triggered) {
		if (ch1_dev)
			charger_dev_set_constant_voltage(ch1_dev, (bat_constant_voltage * 1000));

		pdpm->jeita_triggered = false;
		return false;
	} else {
		return pdpm->jeita_triggered;
	}
}

/* get bq27z561 fastcharge mode to enable or disabled */
static bool pd_get_bms_digest_verified(struct usbpd_pm *pdpm)
{
	//union power_supply_propval pval = {0,};
	//int rc;

	if (!pdpm->bms_psy)
		return false;

#if 0
	rc = power_supply_get_property(pdpm->bms_psy,
				POWER_SUPPLY_PROP_AUTHENTIC, &pval);
	if (rc < 0) {
		pr_info("Couldn't get fastcharge mode:%d\n", rc);
		return false;
	}

	pr_err("pval.intval: %d\n", pval.intval);

	if (pval.intval == 1)
		return true;
	else
		return false;
#endif
	return true;
}

/* get pd pps charger verified result  */
static bool pd_get_pps_charger_verified(struct usbpd_pm *pdpm)
{
	union power_supply_propval pval = {0,};
	int rc;

	if (!pdpm->usb_psy)
		return false;

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
		if(get_bq2597x_load_flag())
		{
			pr_info("bq2597x usbpd_check_cp_psy\n");
			if (pm_config.cp_sec_enable)
				pdpm->cp_psy = power_supply_get_by_name("bq2597x-master");
			else
				pdpm->cp_psy = power_supply_get_by_name("bq2597x-standalone");
		}
		else if(get_ln8000_load_flag())
		{
			pr_info("ln8000 usbpd_check_cp_psy\n");
			pdpm->cp_psy = power_supply_get_by_name("ln8000");
		}
		if (!pdpm->cp_psy)
			pr_err("cp_psy not found\n");
	}
}

static void usbpd_check_cp_sec_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->cp_sec_psy) {
		pdpm->cp_sec_psy = power_supply_get_by_name("bq2597x-slave");
		if (!pdpm->cp_sec_psy)
			pr_err("cp_sec_psy not found\n");
	}
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
	int ret;
	union power_supply_propval val = {0,};
	int ibus;

	pr_debug("%s enter.\n", __func__);
	usbpd_check_cp_psy(pdpm);

	if (!pdpm->cp_psy)
		return;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_CHARGE_MODE, &val);
	if (!ret)
		pdpm->cp.sc8551_charge_mode = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_BYPASS_MODE_ENABLED, &val);
	if (!ret)
		pdpm->cp.sc8551_bypass_charge_enable = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_BATTERY_VOLTAGE, &val);
	if (!ret)
		pdpm->cp.vbat_volt = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_BATTERY_CURRENT, &val);
	if (!ret)
		pdpm->cp.ibat_curr = val.intval;
	pr_debug("%s pdpm->cp.ibat_curr form bq is %d.\n", __func__, pdpm->cp.ibat_curr);

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_BUS_VOLTAGE, &val);
	if (!ret)
		pdpm->cp.vbus_volt = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_BUS_CURRENT, &val);
	if (!ret)
		pdpm->cp.ibus_curr = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_BUS_TEMPERATURE, &val);
	if (!ret)
		pdpm->cp.bus_temp = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_BATTERY_TEMPERATURE, &val);
	if (!ret)
		pdpm->cp.bat_temp = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_DIE_TEMPERATURE, &val);
	if (!ret)
		pdpm->cp.die_temp = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_BATTERY_PRESENT, &val);
	if (!ret)
		pdpm->cp.batt_pres = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_VBUS_PRESENT, &val);
	if (!ret)
		pdpm->cp.vbus_pres = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_BUS_ERROR_STATUS, &val);
	if (!ret)
		pdpm->cp.bus_error_status = val.intval;

	usbpd_check_bms_psy(pdpm);
	if (pdpm->bms_psy) {
		ret = power_supply_get_property(pdpm->bms_psy,
				POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		if (!ret) {
			if (pdpm->cp.vbus_pres)
				pdpm->cp.ibat_curr = -(val.intval / 1000);
		}
	}

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_CHARGE_ENABLED, &val);
	if (!ret)
		pdpm->cp.charge_enabled = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_ALARM_STATUS, &val);
	if (!ret) {
		pdpm->cp.bat_ovp_alarm = !!(val.intval & BAT_OVP_ALARM_MASK);
		pdpm->cp.bat_ocp_alarm = !!(val.intval & BAT_OCP_ALARM_MASK);
		pdpm->cp.bus_ovp_alarm = !!(val.intval & BUS_OVP_ALARM_MASK);
		pdpm->cp.bus_ocp_alarm = !!(val.intval & BUS_OCP_ALARM_MASK);
		pdpm->cp.bat_ucp_alarm = !!(val.intval & BAT_UCP_ALARM_MASK);
		pdpm->cp.bat_therm_alarm = !!(val.intval & BAT_THERM_ALARM_MASK);
		pdpm->cp.bus_therm_alarm = !!(val.intval & BUS_THERM_ALARM_MASK);
		pdpm->cp.die_therm_alarm = !!(val.intval & DIE_THERM_ALARM_MASK);
	}

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_FAULT_STATUS, &val);
	if (!ret) {
		pdpm->cp.bat_ovp_fault = !!(val.intval & BAT_OVP_FAULT_MASK);
		pdpm->cp.bat_ocp_fault = !!(val.intval & BAT_OCP_FAULT_MASK);
		pdpm->cp.bus_ovp_fault = !!(val.intval & BUS_OVP_FAULT_MASK);
		pdpm->cp.bus_ocp_fault = !!(val.intval & BUS_OCP_FAULT_MASK);
		pdpm->cp.bat_therm_fault = !!(val.intval & BAT_THERM_FAULT_MASK);
		pdpm->cp.bus_therm_fault = !!(val.intval & BUS_THERM_FAULT_MASK);
		pdpm->cp.die_therm_fault = !!(val.intval & DIE_THERM_FAULT_MASK);
	}

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_REG_STATUS, &val);
	if (!ret) {
		pdpm->cp.vbat_reg = !!(val.intval & VBAT_REG_STATUS_MASK);
		pdpm->cp.ibat_reg = !!(val.intval & IBAT_REG_STATUS_MASK);
	}

	pr_debug("%s pdpm->cp.vbat_volt is %d.\n", __func__, pdpm->cp.vbat_volt);
	pr_debug("%s pdpm->cp.ibat_curr is %d.\n", __func__, pdpm->cp.ibat_curr);
	pr_debug("%s pdpm->cp.vbus_volt is %d, err_hi_lo is %d.\n", __func__, pdpm->cp.vbus_volt, pdpm->cp.bus_error_status);
	pr_debug("%s pdpm->cp.ibus_curr is %d.\n", __func__, pdpm->cp.ibus_curr);
	pr_debug("%s pdpm->cp.bus_temp is %d.\n", __func__, pdpm->cp.bus_temp);
	pr_debug("%s pdpm->cp.bat_temp is %d.\n", __func__, pdpm->cp.bat_temp);
	pr_debug("%s pdpm->cp.die_temp is %d.\n", __func__, pdpm->cp.die_temp);
	pr_debug("%s pdpm->cp.batt_pres is %d.\n", __func__, pdpm->cp.batt_pres);
	pr_debug("%s pdpm->cp.vbus_pres is %d.\n", __func__, pdpm->cp.vbus_pres);
	pr_debug("%s pdpm->cp.charge_enabled is %d.\n", __func__, pdpm->cp.charge_enabled);
	charger_manager_get_ibus(&ibus);
	pr_debug("%s 6360 ibus is %d.\n", __func__, ibus);
	pr_debug("%s cp bypass_mode: en=%d, mode=%d.\n",
			__func__, pdpm->cp.sc8551_bypass_charge_enable, pdpm->cp.sc8551_charge_mode);
}

static void usbpd_pm_update_cp_sec_status(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	if (!pm_config.cp_sec_enable)
		return;

	usbpd_check_cp_sec_psy(pdpm);

	if (!pdpm->cp_sec_psy)
		return;

	ret = power_supply_get_property(pdpm->cp_sec_psy,
			POWER_SUPPLY_PROP_TI_BUS_CURRENT, &val);
	if (!ret)
		pdpm->cp_sec.ibus_curr = val.intval;

	ret = power_supply_get_property(pdpm->cp_sec_psy,
			POWER_SUPPLY_PROP_CHARGE_ENABLED, &val);
	if (!ret)
		pdpm->cp_sec.charge_enabled = val.intval;
}


static int usbpd_pm_check_fault(struct usbpd_pm *pdpm)
{
	int ret = 0;
	union power_supply_propval val = {0,};

	usbpd_check_cp_psy(pdpm);
	if (!pdpm->cp_psy)
		return -ENODEV;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_ALARM_STATUS, &val);
	if (!ret) {
		pdpm->cp.bat_ovp_alarm = !!(val.intval & BAT_OVP_ALARM_MASK);
		pdpm->cp.bat_ocp_alarm = !!(val.intval & BAT_OCP_ALARM_MASK);
		pdpm->cp.bus_ovp_alarm = !!(val.intval & BUS_OVP_ALARM_MASK);
		pdpm->cp.bus_ocp_alarm = !!(val.intval & BUS_OCP_ALARM_MASK);
		pdpm->cp.bat_ucp_alarm = !!(val.intval & BAT_UCP_ALARM_MASK);
		pdpm->cp.bat_therm_alarm = !!(val.intval & BAT_THERM_ALARM_MASK);
		pdpm->cp.bus_therm_alarm = !!(val.intval & BUS_THERM_ALARM_MASK);
		pdpm->cp.die_therm_alarm = !!(val.intval & DIE_THERM_ALARM_MASK);
	}

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_FAULT_STATUS, &val);
	if (!ret) {
		pdpm->cp.bat_ovp_fault = !!(val.intval & BAT_OVP_FAULT_MASK);
		pdpm->cp.bat_ocp_fault = !!(val.intval & BAT_OCP_FAULT_MASK);
		pdpm->cp.bus_ovp_fault = !!(val.intval & BUS_OVP_FAULT_MASK);
		pdpm->cp.bus_ocp_fault = !!(val.intval & BUS_OCP_FAULT_MASK);
		pdpm->cp.bat_therm_fault = !!(val.intval & BAT_THERM_FAULT_MASK);
		pdpm->cp.bus_therm_fault = !!(val.intval & BUS_THERM_FAULT_MASK);
		pdpm->cp.die_therm_fault = !!(val.intval & DIE_THERM_FAULT_MASK);
	}

	return ret;
}

static int usbpd_pm_enable_cp(struct usbpd_pm *pdpm, bool enable)
{
	int ret;
	union power_supply_propval val = {0,} , fastcharge_limit = {0,};
	u32 mincurrent;

	usbpd_check_cp_psy(pdpm);
	usbpd_check_usb_psy(pdpm);
	if (pdpm->sw_psy) {
	ret = power_supply_get_property(pdpm->sw_psy,
		POWER_SUPPLY_PROP_FAST_CHARGE_CURRENT, &fastcharge_limit);
	}
	mincurrent = min(fastcharge_limit.intval, NORMAL_WORK_MAX_INPUT_CURRENT_THRESHOLD_UA);

	if (!pdpm->cp_psy)
		return -ENODEV;

	if (!chg_consumer)
		chg_consumer = charger_manager_get_by_name(pdpm->dev, "charger_port1");

	if ((enable) && (chg_consumer)) {
		pr_info("%s charger_dev_enable_powerpath enable:%d\n", __func__, !enable);
		charger_manager_enable_power_path(chg_consumer, MAIN_CHARGER, !enable);
	}

	if (!ch1_dev)
		ch1_dev = get_charger_by_name("primary_chg");

	pr_debug("%s enable = %d.\n", __func__, enable);
	val.intval = enable;
	ret = power_supply_set_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if ((!ret) && (ch1_dev)) {
		charger_dev_enable(ch1_dev, !enable);
		if (!enable) {
			charger_dev_set_input_current(ch1_dev, mincurrent); //2020.12.18 longcheer jiangshitian edit for sc/bq pps current limited
		}
	}

	return ret;
}

static int usbpd_pm_enable_cp_sec(struct usbpd_pm *pdpm, bool enable)
{
	int ret;
	union power_supply_propval val = {0,};

	usbpd_check_cp_sec_psy(pdpm);

	if (!pdpm->cp_sec_psy)
		return -ENODEV;

	val.intval = enable;
	ret = power_supply_set_property(pdpm->cp_sec_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);

	return ret;
}

static int usbpd_pm_check_cp_enabled(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,}, fastcharge_limit = {0,};
	u32 mincurrent;
	
	usbpd_check_cp_psy(pdpm);
	usbpd_check_usb_psy(pdpm);
	if (pdpm->sw_psy) {
	ret = power_supply_get_property(pdpm->sw_psy,
			POWER_SUPPLY_PROP_FAST_CHARGE_CURRENT, &fastcharge_limit);
	}
	mincurrent = min(fastcharge_limit.intval, NORMAL_WORK_MAX_INPUT_CURRENT_THRESHOLD_UA);

	if (!pdpm->cp_psy)
		return -ENODEV;

	if (!ch1_dev)
		ch1_dev = get_charger_by_name("primary_chg");

	if (!chg_consumer)
		chg_consumer = charger_manager_get_by_name(pdpm->dev, "charger_port1");

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (!ret) {
		pdpm->cp.charge_enabled = !!val.intval;
		// bq is disabled, then open 6360
		if ((pdpm->cp.charge_enabled == 0) && (ch1_dev) && (chg_consumer)) {
			charger_dev_enable(ch1_dev, !pdpm->cp.charge_enabled);
			charger_manager_enable_power_path(chg_consumer, MAIN_CHARGER, !pdpm->cp.charge_enabled);
			charger_dev_set_input_current(ch1_dev, mincurrent); //2020.12.18 longcheer jiangshitian edit for sc/bq pps current limited
		}
	}

	pr_debug("pdpm->cp.charge_enabled:%d\n", pdpm->cp.charge_enabled);

	return ret;
}

static int usbpd_pm_check_cp_sec_enabled(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	usbpd_check_cp_sec_psy(pdpm);

	if (!pdpm->cp_sec_psy)
		return -ENODEV;

	ret = power_supply_get_property(pdpm->cp_sec_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (!ret)
		pdpm->cp_sec.charge_enabled = !!val.intval;
	pr_debug("pdpm->cp_sec.charge_enabled:%d\n", pdpm->cp_sec.charge_enabled);
	return ret;
}

static int usbpd_pm_set_cp_charge_mode(struct usbpd_pm *pdpm, int mode)
{
	int ret;
	union power_supply_propval val = {0,};

	usbpd_check_cp_psy(pdpm);

	if (!pdpm->cp_psy)
		return -ENODEV;

	pr_info("%s, chg_mode is set to %d\n", __func__, mode);

	val.intval = mode;
	ret = power_supply_set_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_CHARGE_MODE, &val);

	return ret;
}

static int usbpd_pm_update_cp_charge_mode(struct usbpd_pm *pdpm)
{
	int fcc = 0, chg_mode = NOT_SUPPORT;
	int thermal_level = 0;

	usbpd_check_bms_psy(pdpm);

	if (!pdpm->bms_psy)
		return -ENODEV;

	if (pdpm->cp.sc8551_charge_mode == NOT_SUPPORT)
		return chg_mode;

	if (pdpm->cp.sc8551_bypass_charge_enable == 0) {
		chg_mode = SC8551_CHARGE_MODE_DIV2;
	} else {
		fcc = usbpd_get_effective_fcc_val(pdpm);
		pd_get_batt_current_thermal_level(pdpm, &thermal_level);
		if (thermal_level > 9) {
			chg_mode = SC8551_CHARGE_MODE_BYPASS;
		} else if (thermal_level < 9){
			chg_mode = SC8551_CHARGE_MODE_DIV2;
		} else {
			chg_mode = pdpm->cp.sc8551_charge_mode;
		}
	}

	pr_info("%s, en_chg_mode:%d, cp_chg_mode:%d:%d, therm_level:%d, fcc:%d, vbat:%d\n",
			__func__, pdpm->cp.sc8551_bypass_charge_enable,
			pdpm->cp.sc8551_charge_mode, chg_mode,
			thermal_level, fcc, pdpm->cp.vbat_volt);

	return chg_mode;
}

static int usbpd_pm_check_cp_charge_mode(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	usbpd_check_cp_psy(pdpm);

	if (!pdpm->cp_psy)
		return -ENODEV;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_TI_CHARGE_MODE, &val);
	if (!ret)
		pdpm->cp.sc8551_charge_mode = val.intval;

	return ret;
}

static int usbpd_pm_switch_cp_charge_mode(struct usbpd_pm *pdpm)
{
	int ret = 0;
	int chg_mode = NOT_SUPPORT;

	ret = usbpd_pm_check_cp_charge_mode(pdpm);
	if (ret < 0) {
		pr_err("Failed to check cp charge mode!\n");
		return ret;
	}

	if (pdpm->cp.sc8551_charge_mode == NOT_SUPPORT
			&& pdpm->cp.sc8551_bypass_charge_enable != 1) {
		ret = NOT_SUPPORT;
		return ret;
	}

	chg_mode = usbpd_pm_update_cp_charge_mode(pdpm);
	if (chg_mode != pdpm->cp.sc8551_charge_mode && chg_mode > NOT_SUPPORT) {
		if (pdpm->cp.charge_enabled) {
			usbpd_pm_enable_cp(pdpm, false);
			msleep(30);
			usbpd_pm_check_cp_enabled(pdpm);
		}

		if (chg_mode == SC8551_CHARGE_MODE_BYPASS) {
			usbpd_pm_set_cp_charge_mode(pdpm, SC8551_CHARGE_MODE_BYPASS);
		} else if (pdpm->cp.sc8551_charge_mode == SC8551_CHARGE_MODE_BYPASS
				&& chg_mode == SC8551_CHARGE_MODE_DIV2) {
			usbpd_pm_set_cp_charge_mode(pdpm, SC8551_CHARGE_MODE_DIV2);
		}

		pr_info("cp charge mode need to switch from %d to %d!\n",
				pdpm->cp.sc8551_charge_mode, chg_mode);

		ret = usbpd_pm_check_cp_charge_mode(pdpm);
		if (ret < 0) {
			pr_err("Failed to check cp charge mode, force update!\n");
			pdpm->cp.sc8551_charge_mode = chg_mode;
		}
		ret = 1;
	}

	return ret;
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
	struct pps_cap_bq cap;
	int ret = 0, i = 0, retry_cnt = 0;

	pr_debug("%s enter.\n", __func__);

	for (retry_cnt = 0; retry_cnt < 5; retry_cnt++) {
		ret = adapter_get_cap_bq(&cap);
		if (ret == ADAPTER_RET_VERIFYING) {
			pr_info("adapter is verifying, retry:%d\n", retry_cnt);
			msleep(200);
			continue;
		} else if (ret == 0) {
			break;
		}
	}
	if (retry_cnt >= 5)
		pr_info("adapter_get_cap_bq failed, ret:%d\n", ret);

	pdpm->apdo_max_volt = pm_config.min_adapter_volt_required;
	pdpm->apdo_max_curr = pm_config.min_adapter_curr_required;

	for (i = 0; i < cap.nr; i++) {
		if (cap.type[i] == 3) {
			if (cap.max_mv[i] >= pdpm->apdo_max_volt
					&& cap.ma[i] >= pdpm->apdo_max_curr) {
				pdpm->apdo_max_volt = cap.max_mv[i];
				pdpm->apdo_max_curr = cap.ma[i];
				pdpm->apdo_selected_pdo = cap.selected_cap_idx;//
				pdpm->pps_supported = true;
			}
		}
	}
	if (pdpm->pps_supported) {
		pr_info("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d\n",
				pdpm->apdo_selected_pdo,
				pdpm->apdo_max_volt,
				pdpm->apdo_max_curr);
		if (pdpm->apdo_max_curr <= LOW_POWER_PPS_CURR_THR)
			pdpm->apdo_max_curr = XIAOMI_LOW_POWER_PPS_CURR_MAX;
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
	int steps;
	int sw_ctrl_steps = 0;
	int hw_ctrl_steps = 0;
	int step_vbat = 0;
	int step_ibus = 0;
	int step_ibat = 0;
	int step_bat_reg = 0;
	int ibus_total = 0;
	int effective_fcc_val = 0;
	int effective_fcc_taper = 0;
	int thermal_level = 0;
	bool is_fastcharge_mode = false;
	static int curr_fcc_limit, curr_ibus_limit, ibus_limit;
	int ret;

	is_fastcharge_mode = pd_get_fastcharge_mode_enabled(pdpm);
	if (is_fastcharge_mode)
		pm_config.bat_volt_lp_lmt = pdpm->bat_volt_max;
	else
		pm_config.bat_volt_lp_lmt = pdpm->non_ffc_bat_volt_max;

	usbpd_set_new_fcc_voter(pdpm);

	effective_fcc_val = usbpd_get_effective_fcc_val(pdpm);

	/* reduce bus current in cv loop */
	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - BQ_TAPER_HYS_MV) {
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

	/* curr_ibus_limit should compare with apdo_max_curr */
	if (effective_fcc_val > 0) {
		curr_fcc_limit = min(pm_config.bat_curr_lp_lmt, effective_fcc_val);

		if (pdpm->cp.sc8551_charge_mode == SC8551_CHARGE_MODE_BYPASS) {
			curr_ibus_limit = min(curr_fcc_limit, pdpm->apdo_max_curr);
			ibus_limit = curr_ibus_limit;
		} else {
			curr_ibus_limit = min(curr_fcc_limit/2, pdpm->apdo_max_curr);
			if (pdpm->cp.sc8551_charge_mode == NOT_SUPPORT) {
				/* compensate 50mA for target ibus_limit for bq adc accurancy is below standard */
				ibus_limit = curr_ibus_limit + 50;
			} else {
				ibus_limit = curr_ibus_limit;
			}
		}
	}

	pr_info("curr_ibus_limit:%d, ibus_limit:%d, bat_curr_lp_lmt:%d, effective_fcc_val:%d, apdo_max_curr:%d\n",
			curr_ibus_limit, ibus_limit, pm_config.bat_curr_lp_lmt, effective_fcc_val, pdpm->apdo_max_curr);
	pr_info("sc8551_mode:%d\n", pdpm->cp.sc8551_charge_mode);

	/* battery voltage loop*/
	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt)
		step_vbat = -pm_config.fc2_steps;
	else if (pdpm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - 10)
		step_vbat = pm_config.fc2_steps;;

	/* battery charge current loop*/
	if (pdpm->cp.ibat_curr < curr_fcc_limit - 50)
		step_ibat = pm_config.fc2_steps;
	else if (pdpm->cp.ibat_curr > curr_fcc_limit)
		step_ibat = -pm_config.fc2_steps;

	/* bus current loop*/
	ibus_total = pdpm->cp.ibus_curr;

	if (pm_config.cp_sec_enable)
		ibus_total += pdpm->cp_sec.ibus_curr;

	if (ibus_total < ibus_limit)
		step_ibus = pm_config.fc2_steps;
	else if (ibus_total > ibus_limit + 50)
		step_ibus = -pm_config.fc2_steps;

	pr_info("ibus_total_ma: %d\n", ibus_total);
	pr_info("ibus_master_ma: %d\n", pdpm->cp.ibus_curr);
	pr_info("ibus_slave_ma: %d\n", pdpm->cp_sec.ibus_curr);
	pr_info("vbus_mv: %d\n", pdpm->cp.vbus_volt);
	pr_info("vbat_mv: %d\n", pdpm->cp.vbat_volt);
	pr_info("ibat_ma: %d\n", pdpm->cp.ibat_curr);

	pr_info("pdpm->cp.vbat_reg:%d, pdpm->cp.ibat_reg:%d\n",
			pdpm->cp.vbat_reg, pdpm->cp.ibat_reg);
	/* hardware regulation loop*/
	if (pdpm->cp.vbat_reg)
		step_bat_reg = 3 * (-pm_config.fc2_steps);
	else
		step_bat_reg = pm_config.fc2_steps;

	sw_ctrl_steps = min(min(step_vbat, step_ibus), step_ibat);
	sw_ctrl_steps = min(sw_ctrl_steps, step_bat_reg);
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
	usbpd_pm_check_fault(pdpm);

	if (pm_config.cp_sec_enable)
		usbpd_pm_check_cp_sec_enabled(pdpm);

	pd_get_batt_current_thermal_level(pdpm, &thermal_level);

	pdpm->is_temp_out_fc2_range = pd_disable_cp_by_jeita_status(pdpm);
	pr_info("is_temp_out_fc2_range = %d, thermal_level = %d\n", pdpm->is_temp_out_fc2_range, thermal_level);

	if (pdpm->cp.bat_therm_fault) { /* battery overheat, stop charge*/
		pr_info("bat_therm_fault:%d\n", pdpm->cp.bat_therm_fault);
		return PM_ALGO_RET_THERM_FAULT;
	} else if (thermal_level >= MAX_THERMAL_LEVEL
			|| pdpm->is_temp_out_fc2_range) {
		pr_info("thermal level too high or batt temp is out of fc2 range\n");
		if (ch1_dev && thermal_level >= MAX_THERMAL_LEVEL)
			charger_dev_set_charging_current(ch1_dev, 800000);
		return PM_ALGO_RET_CHG_DISABLED;
	} else if (pdpm->cp.bat_ocp_fault || pdpm->cp.bus_ocp_fault
			|| pdpm->cp.bat_ovp_fault || pdpm->cp.bus_ovp_fault) {
		pr_info("bat_ocp_fault:%d, bus_ocp_fault:%d, bat_ovp_fault:%d, bus_ovp_fault:%d\n",
				pdpm->cp.bat_ocp_fault, pdpm->cp.bus_ocp_fault,
				pdpm->cp.bat_ovp_fault, pdpm->cp.bus_ovp_fault);
		return PM_ALGO_RET_OTHER_FAULT; /* go to switch, and try to ramp up*/
	} else if ((!pdpm->cp.charge_enabled && pdpm->cp.bus_error_status != CP_VBUS_ERROR_NONE)
			|| (pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled)) {
		pr_info("cp.charge_enabled:%d, cp_sec.charge_enabled:%d\n",
				pdpm->cp.charge_enabled, pdpm->cp_sec.charge_enabled);
		return PM_ALGO_RET_CHG_DISABLED;
	}

	/* charge pump taper charge */
	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - TAPER_VOL_HYS
			&& pdpm->cp.ibat_curr < pm_config.fc2_taper_current) {
		if (fc2_taper_timer++ > TAPER_TIMEOUT) {
			pr_info("charge pump taper charging done\n");
			fc2_taper_timer = 0;
			return PM_ALGO_RET_TAPER_DONE;
		}
	} else {
		fc2_taper_timer = 0;
	}

	/* sc8551 mode switch*/
	if (pdpm->cp.sc8551_charge_mode != NOT_SUPPORT) {
		ret = usbpd_pm_switch_cp_charge_mode(pdpm);
		if (ret > 0)
			return PM_ALGO_RET_CHG_DISABLED;
	}

	/*TODO: customer can add hook here to check system level
	 * thermal mitigation*/

	steps = min(sw_ctrl_steps, hw_ctrl_steps);
	pr_info("steps: %d, sw_ctrl_steps:%d, hw_ctrl_steps:%d\n", steps, sw_ctrl_steps, hw_ctrl_steps);
	pdpm->request_voltage += steps * STEP_MV;
	pdpm->request_voltage = min(pdpm->request_voltage, PPS_VOL_MAX + PPS_VOL_HYS);

	if (pdpm->apdo_max_volt == PPS_VOL_MAX)
		pdpm->apdo_max_volt = pdpm->apdo_max_volt - PPS_VOL_HYS;

	if (pdpm->request_voltage > pdpm->apdo_max_volt)
		pdpm->request_voltage = pdpm->apdo_max_volt;

	/*if (pdpm->adapter_voltage > 0
			&& pdpm->request_voltage > pdpm->adapter_voltage + 500)
		pdpm->request_voltage = pdpm->adapter_voltage + 500; */

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
	int ret, rc = 0;
	int effective_fcc_val = 0, thermal_level = 0;
	bool is_ch1_pwr_path_en = false;
	static int retry_cnt = 0, curr_fcc_lmt, curr_ibus_lmt;
	static bool stop_sw, recover;

	pr_debug("%s enter. pdpm->state =%d\n", __func__,pdpm->state);

	if (!ch1_dev)
		ch1_dev = get_charger_by_name("primary_chg");

	switch (pdpm->state) {
	case PD_PM_STATE_ENTRY:
		stop_sw = false;
		recover = false;

		pd_get_batt_current_thermal_level(pdpm, &thermal_level);
		pdpm->is_temp_out_fc2_range = pd_disable_cp_by_jeita_status(pdpm);
		pr_info("is_temp_out_fc2_range:%d\n", pdpm->is_temp_out_fc2_range);

		/* update new fcc from bms charge current */
		usbpd_set_new_fcc_voter(pdpm);

		effective_fcc_val = usbpd_get_effective_fcc_val(pdpm);

		if (effective_fcc_val > 0)
			curr_fcc_lmt = min(pm_config.bat_curr_lp_lmt, effective_fcc_val);

		if (pdpm->cp.vbat_volt < pm_config.min_vbat_for_cp) {
			pd_set_bq_charge_done(pdpm, true);
			pr_info("batt_volt %d, waiting...\n", pdpm->cp.vbat_volt);
		} else if ((pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 150
				&& pdpm->cp.sc8551_charge_mode != SC8551_CHARGE_MODE_BYPASS)
				|| (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 50
				&& pdpm->cp.sc8551_charge_mode == SC8551_CHARGE_MODE_BYPASS)) {
			pr_info("batt_volt %d is too high for cp, charging with switch charger\n",
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
			if (ch1_dev)
				charger_dev_set_input_current(ch1_dev, 100000);
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
			curr_ibus_lmt = curr_fcc_lmt;
			pdpm->request_voltage = pdpm->cp.vbat_volt + BUS_VOLT_INIT_UP / 2;
			pdpm->request_current = min(pdpm->apdo_max_curr, curr_ibus_lmt);
			pdpm->request_current = min(pdpm->request_current, MAX_BYPASS_CURRENT_MA);
		} else {
			curr_ibus_lmt = curr_fcc_lmt >> 1;
			pdpm->request_voltage = pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP;
			pdpm->request_current = min(pdpm->apdo_max_curr, curr_ibus_lmt);
		}

		ret = adapter_set_cap_start_bq(pdpm->request_voltage, pdpm->request_current);
		pr_info("curr_ibus_lmt:%d, request_voltage:%d, request_current:%d, ret:%d\n",
				curr_ibus_lmt, pdpm->request_voltage, pdpm->request_current, ret);

		if (ret == ADAPTER_RET_VERIFYING && retry_cnt++ < 5) {
			pr_info("adapter is verifying, retry:%d\n", retry_cnt);
			msleep(200);
			break;
		} else if (ret != 0) {
			pr_info("adapter_set_cap_start_bq failed, switch to main charger!\n");
			retry_cnt = 0;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}

		usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);
		retry_cnt = 0;
		break;

	case PD_PM_STATE_FC2_ENTRY_2:
		pr_info("PD_PM_STATE_FC2_ENTRY_2: bus_err_st:%d, req_vol:%dmV, cur_vol:%d, retry:%d\n",
				pdpm->cp.bus_error_status, pdpm->request_voltage, pdpm->cp.vbus_volt, retry_cnt);

		if (pdpm->cp.bus_error_status == CP_VBUS_ERROR_LOW) {
			if (pdpm->cp.vbus_volt < BUS_MIVR_THRESHOLD + 200) {
				if (ch1_dev) {
					ret = charger_dev_is_powerpath_enabled(ch1_dev, &is_ch1_pwr_path_en);
				}
				if (ret >= 0 && is_ch1_pwr_path_en) {
					pr_info("PD_PM_STATE_FC2_ENTRY_2: vbus too low while main charger working, disable it\n");
				/* 2021.01.21 longcheer jiangshitian change for mic noise begin */
				#if defined(CONFIG_HS_MIC_RECORD_NOISE_PD_CHG)
					if(!enable_switch_charge_PD())
					{
						usbpd_pm_enable_cp(pdpm, false);
					}
					else
					{
						usbpd_pm_enable_cp(pdpm, true);
					}
				#else
					usbpd_pm_enable_cp(pdpm, true);
				#endif
				/* 2021.01.21 longcheer jiangshitian change for mic noise end */
					usbpd_pm_check_cp_enabled(pdpm);
				}
			}

			retry_cnt++;
			pdpm->request_voltage += STEP_MV;
			adapter_set_cap_bq(pdpm->request_voltage, pdpm->request_current);
		} else if (pdpm->cp.bus_error_status == CP_VBUS_ERROR_HIGHT) {
			retry_cnt++;
			pdpm->request_voltage -= STEP_MV;
			adapter_set_cap_bq(pdpm->request_voltage, pdpm->request_current);
		} else {
			pr_info("adapter volt tune ok, retry %d times\n", retry_cnt);
			retry_cnt = 0;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_3);
			break;
		}

		if (retry_cnt > 50) {
			pr_info("Failed to tune adapter volt into valid range, charge with switching charger\n");
			retry_cnt = 0;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}
		break;

	case PD_PM_STATE_FC2_ENTRY_3:
		if (pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled) {
			usbpd_pm_enable_cp_sec(pdpm, true);
			msleep(30);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}

		if (!pdpm->cp.charge_enabled) {
		/* 2021.01.21 longcheer jiangshitian change for mic noise begin */
		#if defined(CONFIG_HS_MIC_RECORD_NOISE_PD_CHG)
			if(!enable_switch_charge_PD())
			{
				usbpd_pm_enable_cp(pdpm, false);
			}
			else
			{
				usbpd_pm_enable_cp(pdpm, true);
			}
		#else
			usbpd_pm_enable_cp(pdpm, true);
		#endif
		/* 2021.01.21 longcheer jiangshitian change for mic noise end */
			msleep(30);
			usbpd_pm_check_cp_enabled(pdpm);
		}

		if (pdpm->cp.charge_enabled) {
			if ((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled)
					|| !pm_config.cp_sec_enable) {
				pd_set_bq_charge_done(pdpm, false);
			/* 2021.01.21 longcheer jiangshitian change for mic noise begin */
			#if defined(CONFIG_HS_MIC_RECORD_NOISE_PD_CHG)
				if(!enable_switch_charge_PD())
				{
					usbpd_pm_enable_cp(pdpm, false);
				}
				else
				{
					usbpd_pm_enable_cp(pdpm, true);
				}
			#else
				usbpd_pm_enable_cp(pdpm, true);
			#endif
			/* 2021.01.21 longcheer jiangshitian change for mic noise end */
				usbpd_pm_check_cp_enabled(pdpm);
				usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_TUNE);
				ibus_lmt_change_timer = 0;
				fc2_taper_timer = 0;
			}
		}
		break;

	case PD_PM_STATE_FC2_TUNE:
		/* 2021.01.21 longcheer jiangshitian change for mic noise begin */
		#if defined(CONFIG_HS_MIC_RECORD_NOISE_PD_CHG)
			if(get_switch_IC_flag())
			{
				usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
				set_switch_IC_flag(false);
				break;
			}
		#endif
		/* 2021.01.21 longcheer jiangshitian change for mic noise end */
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
			adapter_set_cap_bq(pdpm->request_voltage, pdpm->request_current);
			pr_info("request_voltage:%d, request_current:%d\n",
					pdpm->request_voltage, pdpm->request_current);
		}
		/*stop second charge pump if either of ibus is lower than 400ma during CV*/
		if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled
				&& pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - TAPER_WITH_IBUS_HYS
				&& (pdpm->cp.ibus_curr < TAPER_IBUS_THR
						|| pdpm->cp_sec.ibus_curr < TAPER_IBUS_THR)) {
			pr_info("second cp is disabled due to ibus < 450mA\n");
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}
		break;

	case PD_PM_STATE_FC2_EXIT:
		if (ch1_dev) {
			charger_dev_set_mivr(ch1_dev, 4600000);
			charger_dev_is_powerpath_enabled(ch1_dev, &is_ch1_pwr_path_en);
		}
		adapter_set_cap_end_bq(5000, 3000);
		pd_set_bq_charge_done(pdpm, true);

		usbpd_check_usb_psy(pdpm);

		ret = charger_manager_set_current_limit((pdpm->bat_curr_max * 1000), BQ_FCC);
		if (ret > 0)
			pr_info("%s: set current limit success\n", __func__);

		if (pdpm->cp.charge_enabled
				|| (ch1_dev && !is_ch1_pwr_path_en)) {
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

static void usbpd_pm_workfunc(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					pm_work.work);
	int internal = PM_WORK_RUN_NORMAL_INTERVAL;

	pr_debug("%s enter.\n", __func__);

	usbpd_pm_update_sw_status(pdpm);
	usbpd_pm_update_cp_status(pdpm);
	usbpd_pm_update_cp_sec_status(pdpm);

	if (!pdpm->pd_verified_checked) {
		if (!pd_get_pps_charger_verified(pdpm)) {
			if (pm_config.bat_curr_lp_lmt > NON_VERIFIED_PPS_FCC_MAX)
				pm_config.bat_curr_lp_lmt = NON_VERIFIED_PPS_FCC_MAX;

			pr_info("%s: pd not verified, fcc limit to %d\n", __func__, pm_config.bat_curr_lp_lmt);
		}
		pdpm->pd_verified_checked = true;
	}
#if defined(CONFIG_KERNEL_CUSTOM_FACTORY)
	if (!usbpd_pm_sm(pdpm) && pdpm->pd_active) {
		if (pdpm->cp.vbat_volt >= HIGH_VOL_THR_MV)
			internal = PM_WORK_RUN_QUICK_INTERVAL;
		else
			internal = PM_WORK_RUN_QUICK_INTERVAL;
		schedule_delayed_work(&pdpm->pm_work,
				msecs_to_jiffies(internal));
	}
#else
	if (!usbpd_pm_sm(pdpm) && pdpm->pd_active) {
		if (pdpm->cp.vbat_volt >= HIGH_VOL_THR_MV)
			internal = PM_WORK_RUN_QUICK_INTERVAL;
		else
		{
			/* 2021.02.20 longcheer jiangshitian change for mic noise begin */
			#if defined(CONFIG_HS_MIC_RECORD_NOISE_PD_CHG)
				if(!enable_switch_charge_PD())
				{
					internal = (PM_WORK_RUN_NORMAL_INTERVAL - 200);
				}
				else
				{
					internal = PM_WORK_RUN_NORMAL_INTERVAL;
				}
			#else
				internal = PM_WORK_RUN_NORMAL_INTERVAL;
			#endif
			/* 2021.02.20 longcheer jiangshitian change for mic noise end */
		}
		schedule_delayed_work(&pdpm->pm_work,
				msecs_to_jiffies(internal));
	}
#endif

}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	/* 2021.01.21 longcheer jiangshitian change for mic noise begin */
	#if defined(CONFIG_HS_MIC_RECORD_NOISE_PD_CHG)
	enum_switch_CHG_IC = CHG_TYPE_NONE;
	#endif
	/* 2021.01.21 longcheer jiangshitian change for mic noise end */

	cancel_delayed_work_sync(&pdpm->pm_work);
	usbpd_check_usb_psy(pdpm);

	if (!chg_consumer)
		chg_consumer = charger_manager_get_by_name(pdpm->dev, "charger_port1");

	if (chg_consumer) {
		val.intval = 0;
		power_supply_set_property(pdpm->cp_psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
		charger_manager_enable_power_path(chg_consumer, MAIN_CHARGER, true);
	}

	ret = charger_manager_set_current_limit((pdpm->bat_curr_max * 1000), BQ_FCC);
	if (ret > 0)
		pr_info("%s: set current limit success\n", __func__);

	pdpm->pps_supported = false;
	pdpm->jeita_triggered = false;
	pdpm->is_temp_out_fc2_range = false;
	pdpm->pd_verified_checked = false;
	pdpm->apdo_selected_pdo = 0;
	pm_config.bat_volt_lp_lmt = pdpm->bat_volt_max;
	memset(&pdpm->pdo, 0, sizeof(pdpm->pdo));
	pm_config.bat_curr_lp_lmt = pdpm->bat_curr_max;
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

	ch1_dev = get_charger_by_name("primary_chg");
	if (ch1_dev)
		pr_debug("%s: Found primary charger [%s]\n",
			__func__, ch1_dev->props.alias_name);
	else
		pr_err("%s: *** Error : can't find primary charger ***\n", __func__);

	ch2_dev = get_charger_by_name("secondary_chg");
	if (ch2_dev)
		pr_debug("%s: Found secondary charger [%s]\n",
			__func__, ch2_dev->props.alias_name);
	else
		pr_err("%s: *** Error: can't find secondary charger\n", __func__);

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
		usbpd_pd_contact(pdpm, true);
		pr_debug("this is xiaomi TA and authentic is ok.\n");
			}
	else if (!pdpm->pd_active
			&& (pps_support == true)) {
		usbpd_pps_non_verified_contact(pdpm, true);
		pr_debug("this is commom pps TA.\n");
			}
	else if (pdpm->pd_active && (pps_support == false)) {
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

	ch1_dev = get_charger_by_name("primary_chg");

	spin_lock_init(&pdpm->psy_change_lock);

	usbpd_check_cp_psy(pdpm);
	usbpd_check_cp_sec_psy(pdpm);
	usbpd_check_usb_psy(pdpm);

	chg_consumer = charger_manager_get_by_name(&pdev->dev, "charger_port1");
	if (!chg_consumer) {
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
