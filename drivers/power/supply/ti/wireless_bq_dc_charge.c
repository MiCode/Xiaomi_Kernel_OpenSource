
#define pr_fmt(fmt)	"[WIRELESS-PM]: %s: " fmt, __func__

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
#include <linux/pmic-voter.h>

#include "wireless_bq_dc_charge.h"

#define BATT_MAX_CHG_VOLT		4450
#define BATT_FAST_CHG_CURR		6000
#define	BUS_OVP_THRESHOLD		12000
#define	BUS_OVP_ALARM_THRESHOLD		10000

#define BUS_VOLT_INIT_UP		500

#define BAT_VOLT_LOOP_LMT		BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT		BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT		BUS_OVP_THRESHOLD

#define WLDC_WORK_RUN_INTERVAL		600

enum {
	PM_ALGO_RET_OK,
	PM_ALGO_RET_THERM_FAULT,
	PM_ALGO_RET_OTHER_FAULT,
	PM_ALGO_RET_CHG_DISABLED,
	PM_ALGO_RET_TAPER_DONE,
};

static struct cppm_config pm_config = {
	.bat_volt_lp_lmt		= BAT_VOLT_LOOP_LMT,
	.bat_curr_lp_lmt		= BAT_CURR_LOOP_LMT + 1000,
	.bus_volt_lp_lmt		= BUS_VOLT_LOOP_LMT,
	.bus_curr_lp_lmt		= BAT_CURR_LOOP_LMT >> 1,

	.fc2_taper_current		= 2000,
	.fc2_steps			= 1,

	.min_adapter_volt_required	= 10000,
	.min_adapter_curr_required	= 2000,

	.min_vbat_for_cp		= 3500,

	.cp_sec_enable			= true,
	.fc2_disable_sw			= true,
};

static struct wireless_dc_device_info *__pm;

static int fc2_taper_timer;
static int iout_lmt_change_timer;
static int taper_triggered_timer;

static void wldc_check_wl_psy(struct wireless_dc_device_info *pm)
{
	if (!pm->wl_psy) {
		pm->wl_psy = power_supply_get_by_name("wireless");
		if (!pm->wl_psy)
			pr_err("wireless psy not found!\n");
	}
}

static void wldc_check_dc_psy(struct wireless_dc_device_info *pm)
{
	if (!pm->dc_psy) {
		pm->dc_psy = power_supply_get_by_name("dc");
		if (!pm->dc_psy)
			pr_err("dc psy not found!\n");
	}
}

static void wldc_check_usb_psy(struct wireless_dc_device_info *pm)
{
	if (!pm->usb_psy) {
		pm->usb_psy = power_supply_get_by_name("usb");
		if (!pm->usb_psy)
			pr_err("usb psy not found!\n");
	}
}


static void wldc_check_bms_psy(struct wireless_dc_device_info *pm)
{
	if (!pm->bms_psy) {
		pm->bms_psy = power_supply_get_by_name("bms");
		if (!pm->bms_psy)
			pr_err("bms psy not found!\n");
	}
}

static void wldc_check_cp_psy(struct wireless_dc_device_info *pm)
{
	if (!pm->cp_psy) {
		if (pm_config.cp_sec_enable)
			pm->cp_psy = power_supply_get_by_name("bq2597x-master");
		else
			pm->cp_psy = power_supply_get_by_name("bq2597x-standalone");
		if (!pm->cp_psy)
			pr_err("cp_psy not found\n");
	}
}

static void wldc_check_cp_sec_psy(struct wireless_dc_device_info *pm)
{
	if (!pm->cp_sec_enable)
		return;
	if (!pm->cp_sec_psy) {
		pm->cp_sec_psy = power_supply_get_by_name("bq2597x-slave");
		if (!pm->cp_sec_psy)
			pr_err("cp_sec_psy not found\n");
	}
}

static int wldc_pm_check_batt_psy(struct wireless_dc_device_info *pm)
{
	if (!pm->sw_psy) {
		pm->sw_psy = power_supply_get_by_name("battery");
		if (!pm->sw_psy) {
			pr_err("batt psy not found!\n");
			return -ENODEV;
		}
	}
	return 0;
}


static bool wldc_get_dc_online(struct wireless_dc_device_info *pm)
{
	union power_supply_propval pval = {0,};
	int rc;

	wldc_check_dc_psy(pm);

	if (!pm->dc_psy)
		return false;

	rc = power_supply_get_property(pm->dc_psy,
				POWER_SUPPLY_PROP_ONLINE, &pval);
	if (rc < 0) {
		pr_info("Couldn't get dc online prop:%d\n", rc);
		return false;
	}

	pr_debug("pval.intval: %d\n", pval.intval);

	if (pval.intval == 1)
		return true;
	else
		return false;
}

static int wldc_msleep(struct wireless_dc_device_info *pm, int sleep_ms)
{
	int i;
	int interval = 25;  //ms
	int cnt = sleep_ms/interval;

	wldc_check_dc_psy(pm);

	for(i = 0; i < cnt; i++) {
		msleep(interval);
		/* if dc online(dc_pon pin level is low) is absent, wireless disconnect now*/
		if (!wldc_get_dc_online(pm)) {
			pr_err("wireless tx disconnect, stop msleep\n");
			return -NORMAL_ERR;
		}
	}
	return 0;
}
static bool wldc_is_fcc_voter_esr(struct wireless_dc_device_info *pm){
	if (!pm->fcc_votable)
		pm->fcc_votable = find_votable("FCC");

	if (!pm->fcc_votable)
		return false;

	if (strcmp(get_effective_client_locked(pm->fcc_votable), "ESR_WORK_VOTER") == 0){
		return true;
	}

	return false;
}

static int wldc_get_effective_fcc_val(struct wireless_dc_device_info *pm)
{
	int effective_fcc_val = 0;

	if (!pm->fcc_votable)
		pm->fcc_votable = find_votable("FCC");

	if (!pm->fcc_votable)
		return -EINVAL;

	effective_fcc_val = get_effective_result(pm->fcc_votable);
	effective_fcc_val = effective_fcc_val / 1000;
	pr_info("effective_fcc_val: %d\n", effective_fcc_val);
	return effective_fcc_val;
}

/* get main switch mode charger charge type from battery power supply property */
static int wldc_get_batt_charge_type(struct wireless_dc_device_info *pm, int *charge_type)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	wldc_pm_check_batt_psy((pm));

	if (!(pm)->sw_psy)
		return -ENODEV;

	rc = power_supply_get_property((pm)->sw_psy,
				POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
	if (rc < 0) {
		pr_info("Couldn't get fastcharge mode:%d\n", rc);
		return rc;
	}

	pr_err("pval.intval: %d\n", pval.intval);

	*charge_type = pval.intval;
	return rc;
}

/* get step charge vfloat index from battery power supply property */
static int wldc_get_batt_step_vfloat_index(struct wireless_dc_device_info *pm, int *step_index)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	wldc_pm_check_batt_psy(pm);

	if (!(pm)->sw_psy)
		return -ENODEV;

	rc = power_supply_get_property((pm)->sw_psy,
				POWER_SUPPLY_PROP_STEP_VFLOAT_INDEX, &pval);
	if (rc < 0) {
		pr_info("Couldn't get fastcharge mode:%d\n", rc);
		return rc;
	}

	pr_err("pval.intval: %d\n", pval.intval);

	*step_index = pval.intval;
	return rc;
}

static int wldc_bq_soft_taper_by_main_charger_charge_type(struct wireless_dc_device_info *pm)
{
	int rc = 0;
	int step_index = 0, curr_charge_type = 0;
	int effective_fcc_bq_taper = 0;

	rc = wldc_get_batt_step_vfloat_index(pm, &step_index);
	if (rc >=0 && step_index == STEP_VFLOAT_INDEX_MAX) {
		rc = wldc_get_batt_charge_type(pm, &curr_charge_type);
		if (rc >=0 && curr_charge_type == POWER_SUPPLY_CHARGE_TYPE_TAPER) {
			effective_fcc_bq_taper = wldc_get_effective_fcc_val(pm);
			if (effective_fcc_bq_taper >= WLDC_SOFT_TAPER_DECREASE_MIN_MA) {
				effective_fcc_bq_taper -= WLDC_SOFT_TAPER_DECREASE_STEP_MA;
				pr_err("BS voltage is reached to maxium vfloat, decrease fcc: %d mA\n",
						effective_fcc_bq_taper);

				if (pm->fcc_votable)
					vote(pm->fcc_votable, BQ_TAPER_FCC_VOTER,
						true, effective_fcc_bq_taper * 1000);
			}
		}
	}

	return rc;
}

static void wldc_pm_update_cp_status(struct wireless_dc_device_info *pm)
{
	int ret;
	union power_supply_propval val = {0,};

	wldc_check_cp_psy(pm);

	if (!pm->cp_psy)
		return;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_BATTERY_VOLTAGE, &val);
	if (!ret)
		pm->cp.vbat_volt = val.intval;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_BUS_VOLTAGE, &val);
	if (!ret)
		pm->cp.vbus_volt = val.intval;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_BUS_CURRENT, &val);
	if (!ret)
		pm->cp.ibus_curr = val.intval;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_BUS_TEMPERATURE, &val);
	if (!ret)
		pm->cp.bus_temp = val.intval;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_BATTERY_TEMPERATURE, &val);
	if (!ret)
		pm->cp.bat_temp = val.intval;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_DIE_TEMPERATURE, &val);
	if (!ret)
		pm->cp.die_temp = val.intval;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_BATTERY_PRESENT, &val);
	if (!ret)
		pm->cp.batt_pres = val.intval;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_VBUS_PRESENT, &val);
	if (!ret)
		pm->cp.vbus_pres = val.intval;

	wldc_check_bms_psy(pm);
	if (pm->bms_psy) {
		ret = power_supply_get_property(pm->bms_psy,
				POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		if (!ret) {
			if (pm->cp.vbus_pres)
				pm->cp.ibat_curr = -(val.intval / 1000);
		}
	}

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_CHARGE_ENABLED, &val);
	if (!ret)
		pm->cp.charge_enabled = val.intval;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_ALARM_STATUS, &val);
	if (!ret) {
		pm->cp.bat_ovp_alarm = !!(val.intval & BAT_OVP_ALARM_MASK);
		pm->cp.bat_ocp_alarm = !!(val.intval & BAT_OCP_ALARM_MASK);
		pm->cp.bus_ovp_alarm = !!(val.intval & BUS_OVP_ALARM_MASK);
		pm->cp.bus_ocp_alarm = !!(val.intval & BUS_OCP_ALARM_MASK);
		pm->cp.bat_ucp_alarm = !!(val.intval & BAT_UCP_ALARM_MASK);
		pm->cp.bat_therm_alarm = !!(val.intval & BAT_THERM_ALARM_MASK);
		pm->cp.bus_therm_alarm = !!(val.intval & BUS_THERM_ALARM_MASK);
		pm->cp.die_therm_alarm = !!(val.intval & DIE_THERM_ALARM_MASK);
	}

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_FAULT_STATUS, &val);
	if (!ret) {
		pm->cp.bat_ovp_fault = !!(val.intval & BAT_OVP_FAULT_MASK);
		pm->cp.bat_ocp_fault = !!(val.intval & BAT_OCP_FAULT_MASK);
		pm->cp.bus_ovp_fault = !!(val.intval & BUS_OVP_FAULT_MASK);
		pm->cp.bus_ocp_fault = !!(val.intval & BUS_OCP_FAULT_MASK);
		pm->cp.bat_therm_fault = !!(val.intval & BAT_THERM_FAULT_MASK);
		pm->cp.bus_therm_fault = !!(val.intval & BUS_THERM_FAULT_MASK);
		pm->cp.die_therm_fault = !!(val.intval & DIE_THERM_FAULT_MASK);
	}

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_REG_STATUS, &val);
	if (!ret) {
		pm->cp.vbat_reg = !!(val.intval & VBAT_REG_STATUS_MASK);
		pm->cp.ibat_reg = !!(val.intval & IBAT_REG_STATUS_MASK);
	}

	if (!pm->use_qcom_gauge) {
		ret = power_supply_get_property(pm->bms_psy,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
		if (!ret)
			pm->cp.bms_vbat_mv = val.intval / 1000;
		else
			pr_err("Failed to read bms voltage now\n");
	}
}

static void wldc_pm_update_cp_sec_status(struct wireless_dc_device_info *pm)
{
	int ret;
	union power_supply_propval val = {0,};

	if (!pm_config.cp_sec_enable)
		return;

	wldc_check_cp_sec_psy(pm);

	if (!pm->cp_sec_psy)
		return;

	ret = power_supply_get_property(pm->cp_sec_psy,
			POWER_SUPPLY_PROP_TI_BUS_CURRENT, &val);
	if (!ret)
		pm->cp_sec.ibus_curr = val.intval;

	ret = power_supply_get_property(pm->cp_sec_psy,
			POWER_SUPPLY_PROP_CHARGE_ENABLED, &val);
	if (!ret)
		pm->cp_sec.charge_enabled = val.intval;
}

static int wldc_pm_enable_cp(struct wireless_dc_device_info *pm, bool enable)
{
	int ret;
	union power_supply_propval val = {0,};

	wldc_check_cp_psy(pm);

	if (!pm->cp_psy)
		return -ENODEV;

	val.intval = enable;
	ret = power_supply_set_property(pm->cp_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);

	return ret;
}

static int wldc_pm_enable_cp_sec(struct wireless_dc_device_info *pm, bool enable)
{
	int ret;
	union power_supply_propval val = {0,};

	wldc_check_cp_sec_psy(pm);

	if (!pm->cp_sec_psy)
		return -ENODEV;

	val.intval = enable;
	ret = power_supply_set_property(pm->cp_sec_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);

	return ret;
}

static int wldc_pm_check_cp_enabled(struct wireless_dc_device_info *pm)
{
	int ret;
	union power_supply_propval val = {0,};

	wldc_check_cp_psy(pm);

	if (!pm->cp_psy)
		return -ENODEV;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (!ret)
		pm->cp.charge_enabled = !!val.intval;

	pr_info("pm->cp.charge_enabled:%d\n", pm->cp.charge_enabled);

	return ret;
}

static int wldc_pm_check_cp_sec_enabled(struct wireless_dc_device_info *pm)
{
	int ret;
	union power_supply_propval val = {0,};

	wldc_check_cp_sec_psy(pm);

	if (!pm->cp_sec_psy)
		return -ENODEV;

	ret = power_supply_get_property(pm->cp_sec_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (!ret)
		pm->cp_sec.charge_enabled = !!val.intval;
	pr_info("pm->cp_sec.charge_enabled:%d\n", pm->cp_sec.charge_enabled);
	return ret;
}


static int wldc_pm_enable_sw(struct wireless_dc_device_info *pm, bool enable)
{
	int ret;
	union power_supply_propval val = {0,};

	if (wldc_pm_check_batt_psy(pm))
		return -ENODEV;

	val.intval = enable;
	ret = power_supply_set_property(pm->sw_psy,
			POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &val);

	return ret;
}

static int wldc_pm_check_sw_enabled(struct wireless_dc_device_info *pm)
{
	int ret;
	union power_supply_propval val = {0,};

	if (wldc_pm_check_batt_psy(pm))
		return -ENODEV;

	ret = power_supply_get_property(pm->sw_psy,
			POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &val);
	if (!ret)
		pm->sw.charge_enabled = !!val.intval;

	return ret;
}

static int wldc_pm_get_batt_capacity(struct wireless_dc_device_info *pm, int *capacity)
{
	int ret;
	union power_supply_propval val = {0,};

	if (wldc_pm_check_batt_psy(pm))
		return -ENODEV;

	ret = power_supply_get_property(pm->sw_psy,
			POWER_SUPPLY_PROP_CAPACITY, &val);
	if (!ret)
		*capacity = val.intval;

	return ret;
}

static void wldc_pm_update_sw_status(struct wireless_dc_device_info *pm)
{
	wldc_pm_check_sw_enabled(pm);
}

static void wldc_dump_wl_volt(struct wireless_dc_device_info *pm)
{
	union power_supply_propval val = {0,};
	int ret = 0;

	if (!pm->wl_psy)
		return;

	ret = power_supply_get_property(pm->wl_psy,
			POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, &val);
	if (!ret)
		pr_info("wl_vout:%d mA\n", val.intval / 1000);
}

static int wldc_get_bq_cp_vbat(struct wireless_dc_device_info *pm, int *vbat_val)
{
	union power_supply_propval val = {0,};
	int ret = 0;

	if (!pm->cp_psy)
		return -ENODEV;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_BATTERY_VOLTAGE, &val);
	if (!ret) {
		pr_info("bq_vbat:%d mA\n", val.intval);
		*vbat_val = val.intval;
	}

	return ret;
}

static int wldc_get_bq_cp_ibus(struct wireless_dc_device_info *pm, int *ibus_val)
{
	union power_supply_propval val = {0,};
	int ret = 0;

	if (!pm->cp_psy)
		return -ENODEV;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_BUS_CURRENT, &val);
	if (!ret) {
		pr_info("bq_ibus:%d mA\n", val.intval);
		*ibus_val = val.intval;
	}

	return ret;
}

static int wldc_get_bq_cp_vbus(struct wireless_dc_device_info *pm, int *vbus_val)
{
	union power_supply_propval val = {0,};
	int ret = 0;

	if (!pm->cp_psy)
		return -ENODEV;

	ret = power_supply_get_property(pm->cp_psy,
			POWER_SUPPLY_PROP_TI_BUS_VOLTAGE, &val);
	if (!ret) {
		pr_info("bq_vbus:%d mA\n", val.intval);
		*vbus_val = val.intval;
	}

	return ret;
}

static int wireless_charge_get_tx_adapter_type(struct wireless_dc_device_info *pm,
	int *adpater_type)
{
	union power_supply_propval val = {0,};
	int ret = 0;

	if (!pm->wl_psy)
		return -ENODEV;

	ret = power_supply_get_property(pm->wl_psy,
			POWER_SUPPLY_PROP_TX_ADAPTER, &val);
	if (!ret) {
		*adpater_type = val.intval;
		pr_info("TX adapter type is:%d\n", *adpater_type);
	}
	return ret;
}

static int wireless_charge_set_rx_vout(struct wireless_dc_device_info *pm, int rx_vout)
{
	int ret = 0;
	union power_supply_propval val = {0,};

	wldc_check_wl_psy(pm);

	if (!pm->wl_psy)
		return -ENODEV;

	pr_info("%s: rx_vout is set to %dmV\n", __func__, rx_vout);

	val.intval = rx_vout * 1000;
	ret = power_supply_set_property(pm->wl_psy,
		POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, &val);

	return ret;
}

static int wireless_charge_get_rx_vout(struct wireless_dc_device_info *pm, int *rx_vout)
{
	union power_supply_propval val = {0,};
	int ret = 0;

	if (!pm->wl_psy)
		return -ENODEV;

	ret = power_supply_get_property(pm->wl_psy,
			POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, &val);
	if (!ret) {
		*rx_vout = val.intval / 1000;
		pr_info("rx_vout:%d mV\n", *rx_vout);
	}
	return ret;
}

static int wireless_charge_get_rx_iout(struct wireless_dc_device_info *pm, int *rx_iout)
{
	union power_supply_propval val = {0,};
	int ret = 0;

	if (!pm->wl_psy)
		return -ENODEV;

	ret = power_supply_get_property(pm->wl_psy,
			POWER_SUPPLY_PROP_RX_IOUT, &val);
	if (!ret) {
		*rx_iout = val.intval / 1000;
		pr_info("rx_iout:%d mA\n", *rx_iout);
	}
	return ret;
}

static int wireless_charge_get_rx_vrect(struct wireless_dc_device_info *pm, int *vrect_val)
{
	union power_supply_propval val = {0,};
	int ret = 0;

	if (!pm->wl_psy)
		return -ENODEV;

	ret = power_supply_get_property(pm->wl_psy,
			POWER_SUPPLY_PROP_INPUT_VOLTAGE_VRECT, &val);
	if (!ret) {
		*vrect_val = val.intval / 1000;
		pr_info("vrect_val:%d mV\n", *vrect_val);
	}
	return ret;
}

static void wldc_regulate_power(struct wireless_dc_device_info *pm, int volt)
{
	union power_supply_propval val = {0,};
	int ret = 0;

	pr_info("volt: %d, rx_vout_set:%d\n", volt, pm->rx_vout_set);

	val.intval = volt * 1000;

	wldc_check_wl_psy(pm);

	if (!pm->wl_psy)
		return ;

	ret = power_supply_set_property(pm->wl_psy,
		POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, &val);

	wldc_msleep(pm, 50);

	wldc_dump_wl_volt(pm);
}
#define MAX_VOLTAGE_FOR_OPEN_MV 9750
static int wldc_soft_start_to_open_dc_path(struct wireless_dc_device_info *pm)
{
	int ret = 0, i = 0;
	int bq_vbatt, bq_ibus, bq_vbus, rx_vrect, rx_vout, rx_vout_set;
	int rx_iout = 0;
	pr_info("wldc_soft_start_to_open_dc_path start\n");

	if (!pm->wl_info.active)
		return -WL_DISCONNECTED_ERR;

	/* update latest bq cp status, such as bq vbat_vol, bq vbus, ibus and so on */
	ret = wldc_get_bq_cp_vbat(pm, &bq_vbatt);
	if (ret)
		return ret;

	if (bq_vbatt < 0) {
		pr_err("get bq_vbatt fail!\n");
		return -NORMAL_ERR;
	}

	rx_vout_set = bq_vbatt * 2 + BUS_VOLT_INIT_UP;
	ret = wireless_charge_set_rx_vout(pm, rx_vout_set);
	if (ret)
		pr_err("%s: set rx vout fail\n", __func__);

	ret = wldc_msleep(pm, 200);
	if (ret)
		return ret;
	pm->rx_vout_set = rx_vout_set;

	/* try to enable bq */
	//if (pm->cp_sec_enable && !pm->cp_sec.charge_enabled) {
	//	ret = wldc_pm_enable_cp_sec(pm, true);
	//}
	//if (pm->cp_sec_enable)
	//	ret = wldc_pm_check_cp_sec_enabled(pm);
	if (!pm->cp.charge_enabled)
		ret = wldc_pm_enable_cp(pm, true);
	ret = wldc_pm_check_cp_enabled(pm);
	if (ret) {
		pr_err("%s: bq cp open fail!\n", __func__);
	} else {
		pr_info("pm->cp.charge_enabled:%d\n", pm->cp.charge_enabled);
	}

	for (i = 0; i < WLDC_OPEN_DC_PATH_MAX_CNT; i++) {
		ret = wldc_msleep(pm, 300);  // used to wait for rx stable
		if (ret)
			return ret;

		if (!pm->wl_info.active)
			return -WL_DISCONNECTED_ERR;

		ret = wldc_get_bq_cp_ibus(pm, &bq_ibus);
		if (ret)
			return ret;

		ret = wldc_get_bq_cp_vbus(pm, &bq_vbus);
		if (ret)
			return ret;

		ret = wireless_charge_get_rx_iout(pm, &rx_iout);

		if (bq_ibus > WLDC_OPEN_PATH_RX_IOUT_MIN) {
			pr_info("[%s] get rx iout above IOUT_MIN succ\n", __func__);
			return 0;
		}
		ret = wldc_get_bq_cp_vbat(pm, &bq_vbatt);
		if (ret)
			return ret;
		if (bq_vbatt > pm_config.bat_volt_lp_lmt - VBAT_HIGH_FOR_FC_HYS_MV) {
			pr_err("%s: bq_vbatt(%dmV)is very high, bq_ibus = %d\n",
				__func__, bq_vbatt, bq_ibus);
			return -VBAT_TOO_HIGH_ERR;
		}

		if (rx_vout_set >= MAX_VOLTAGE_FOR_OPEN_MV) {
			pr_err("%s: rx_vout_set(%dmV)is very high, bq_ibus = %d\n",
				__func__, rx_vout_set, bq_ibus);
			return -RX_VOUT_SET_TOO_HIGH_ERR;
		}

		rx_vout_set += pm->wl_info.step_mv;
		ret = wireless_charge_set_rx_vout(pm, rx_vout_set);
		if (ret) {
			pr_err("%s: set rx vout fail\n", __func__);
			return -NORMAL_ERR;
		}
		pm->rx_vout_set = rx_vout_set;
		ret = wireless_charge_get_rx_vrect(pm, &rx_vrect);
		if (ret)
			return ret;
		ret = wireless_charge_get_rx_vout(pm, &rx_vout);
		if (ret)
			return ret;

		pr_info("[%s] bq_ibus = %dmA, vrect = %dmV, vout = %dmV\n",
			__func__, bq_ibus, rx_vrect, rx_vout);

		/* try to enable bq */
		//if (pm->cp_sec_enable && !pm->cp_sec.charge_enabled)
		//	ret = wldc_pm_enable_cp_sec(pm, true);

		//if (pm->cp_sec_enable)
		//	ret = wldc_pm_check_cp_sec_enabled(pm);

		ret = wldc_pm_check_cp_enabled(pm);

		if (!pm->cp.charge_enabled) {
			ret = wldc_pm_enable_cp(pm, true);
		}
		ret = wldc_pm_check_cp_enabled(pm);

		if (ret)
			pr_err("%s: bq cp open fail!\n", __func__);
		else
			pr_info("pm->cp.charge_enabled:%d\n", pm->cp.charge_enabled);
	}

	pr_info("wldc_soft_start_to_open_dc_path end\n");
	return -NORMAL_ERR;
}
#define DELAY_BEFORE_CP_OPEN_MS 0
static int wldc_security_confirm_before_start_fc2_charge(struct wireless_dc_device_info *pm)
{
	int ret;

	//TO DO
	ret = wldc_msleep(pm, DELAY_BEFORE_CP_OPEN_MS); //used here, delay for stable vout
	if (ret)
		return ret;
	return 0;
}


/* get thermal level from battery power supply property */
static int wl_get_batt_current_thermal_level(struct wireless_dc_device_info *pm, int *level)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	rc = wldc_pm_check_batt_psy(pm);
	if (rc)
		return rc;

	rc = power_supply_get_property(pm->sw_psy,
				POWER_SUPPLY_PROP_DC_THERMAL_LEVELS, &pval);
	if (rc < 0) {
		pr_info("Couldn't get fastcharge mode:%d\n", rc);
		return rc;
	}

	pr_err("pval.intval: %d\n", pval.intval);

	*level = pval.intval;
	return rc;
}


static bool wl_disable_cp_by_jeita_status(struct wireless_dc_device_info *pm)
{
	union power_supply_propval pval = {0,};
	int batt_temp = 0;
	int rc;

	if (!pm->bms_psy)
		return false;

	rc = power_supply_get_property(pm->bms_psy,
				POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0) {
		pr_info("Couldn't get batt temp prop:%d\n", rc);
		return false;
	}

	batt_temp = pval.intval;
	pr_err("batt_temp: %d\n", batt_temp);

	if (batt_temp >= pm->warm_threshold_temp && !pm->jeita_triggered) {
		pm->jeita_triggered = true;
		return true;
	} else if (batt_temp <= JEITA_COOL_NOT_ALLOW_CP_THR && !pm->jeita_triggered ){
		pm->jeita_triggered = true;
		return true;
	} else if ((batt_temp <= (pm->warm_threshold_temp - JEITA_WARM_HYSTERESIS) )
			&& (batt_temp >= (JEITA_COOL_NOT_ALLOW_CP_THR + JEITA_COOL_HYSTERESIS))
			&& pm->jeita_triggered) {
		pm->jeita_triggered = false;
		return false;
	} else {
		return pm->jeita_triggered;
	}
}
/* get fastcharge mode */
static bool wl_get_fastcharge_mode_enabled(struct wireless_dc_device_info *pm)
{
	union power_supply_propval pval = {0,};
	int rc;

	if (!pm->bms_psy)
		return false;

	rc = power_supply_get_property(pm->bms_psy,
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

#define TAPER_TIMEOUT	(30000 / WLDC_WORK_RUN_INTERVAL)
#define IOUT_CHANGE_TIMEOUT  (3000 / WLDC_WORK_RUN_INTERVAL)
#define IOUT_SAFTY_THRESHOLD 1400
static int wldc_pm_fc2_charge_algo(struct wireless_dc_device_info *pm)
{
	int steps;
	int sw_ctrl_steps = 0;
	int hw_ctrl_steps = 0;
	int step_iout = 0;
	int step_vbat = 0;
	int step_bat_reg = 0;
	int ibus_total = 0;
	int effective_fcc_val = 0;
	int step_mv = 0;
	int ret = 0, rx_iout = 0;
	int tx_adapter_type = 0;
	int effective_iout_current_max = 0;
	int thermal_level = 0;
	static int iout_max_limit;
	int bq_taper_hys_mv = BQ_TAPER_HYS_MV;
	static int rx_iout_limit;
	bool is_fastcharge_mode = false;
	int effective_fcc_taper = 0;
	int vrect, vout;
	int soc = 0;

	if (!pm->wl_info.active) {
		pr_info("active is false, return\n");
		return PM_ALGO_RET_CHG_DISABLED;
	}

	ret = wireless_charge_get_rx_vrect(pm, &vrect);
	ret = wireless_charge_get_rx_vout(pm, &vout);
	pr_info("vrect = %dmV, vout = %dmV\n", vrect, vout);

	/* if tx adapter type is 27/40w chargers, set maxium rx iout curr to 1A */
	ret = wireless_charge_get_tx_adapter_type(pm, &tx_adapter_type);
	if (tx_adapter_type == ADAPTER_XIAOMI_QC3
			|| tx_adapter_type == ADAPTER_XIAOMI_PD
			|| tx_adapter_type == ADAPTER_ZIMI_CAR_POWER) {
		effective_iout_current_max = WLDC_XIAOMI_20W_IOUT_MAX;
	} else if ((tx_adapter_type == ADAPTER_XIAOMI_PD_40W)
			|| (tx_adapter_type == ADAPTER_VOICE_BOX)
			|| (tx_adapter_type == ADAPTER_XIAOMI_PD_50W)
			|| (tx_adapter_type == ADAPTER_XIAOMI_PD_60W)) {
		effective_iout_current_max = pm->rx_iout_curr_max;
	} else {
		pr_err("not our defined quick chargers, switch to main\n");
		return PM_ALGO_RET_CHG_DISABLED;
	}


	is_fastcharge_mode = wl_get_fastcharge_mode_enabled(pm);
	if (is_fastcharge_mode) {
		pm_config.bat_volt_lp_lmt = pm->bat_volt_max;
		bq_taper_hys_mv = BQ_TAPER_HYS_MV;
	} else {
		pm_config.bat_volt_lp_lmt = pm->non_ffc_bat_volt_max;
		bq_taper_hys_mv = NON_FFC_BQ_TAPER_HYS_MV;
	}

	/* if cell vol read from fuel gauge is higher than threshold, vote saft fcc to protect battery */
	if (!pm->use_qcom_gauge && is_fastcharge_mode) {
		pr_err("pm->cp.bms_vbat_mv: %d\n", pm->cp.bms_vbat_mv);
		if (pm->cp.bms_vbat_mv > pm->cell_vol_max_threshold_mv) {
			if (pm->over_cell_vol_max_count++ > CELL_VOLTAGE_MAX_COUNT_MAX) {
				pm->over_cell_vol_max_count = 0;
				effective_fcc_taper = wldc_get_effective_fcc_val(pm);
				effective_fcc_taper -= WLDC_BQ_TAPER_DECREASE_STEP_MA;
				pr_err("vcell is reached to max threshold, decrease fcc: %d mA\n",
							effective_fcc_taper);
				if (pm->fcc_votable)
					vote(pm->fcc_votable, BQ_TAPER_FCC_VOTER,
						true, effective_fcc_taper * 1000);
			}
		} else {
			pm->over_cell_vol_max_count = 0;
		}
	}

	/* if cell vol read from fuel gauge is higher than threshold, vote saft fcc to protect battery */
	if (!pm->use_qcom_gauge && is_fastcharge_mode) {
		if (pm->cp.bms_vbat_mv > pm->cell_vol_high_threshold_mv) {
			if (pm->over_cell_vol_high_count++ > CELL_VOLTAGE_HIGH_COUNT_MAX) {
				pm->over_cell_vol_high_count = 0;
				if (pm->fcc_votable)
					vote(pm->fcc_votable, BQ_TAPER_CELL_HGIH_FCC_VOTER,
							true, pm->step_charge_high_vol_curr_max * 1000);
			}
		} else {
			pm->over_cell_vol_high_count = 0;
		}
	}

	if (pm->use_qcom_gauge)
		wldc_bq_soft_taper_by_main_charger_charge_type(pm);

	step_mv = pm->wl_info.step_mv;

	/* reduce fcc vote value when bq enter cv loop, used for soft bq taper */
	if (pm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - bq_taper_hys_mv) {
		if (taper_triggered_timer++ > IOUT_CHANGE_TIMEOUT) {
			taper_triggered_timer = 0;
			effective_fcc_val = wldc_get_effective_fcc_val(pm);
			effective_fcc_val = min(effective_fcc_val, effective_iout_current_max * 4);
			effective_fcc_val -= WLDC_BQ_TAPER_DECREASE_STEP_MA;
			pr_err("bq taper, reduce fcc to: %d\n", effective_fcc_val);
			if (pm->fcc_votable)
				vote(pm->fcc_votable, BQ_TAPER_FCC_VOTER,
					true, effective_fcc_val * 1000);
		}
	} else {
		taper_triggered_timer = 0;
	}

	effective_fcc_val = wldc_get_effective_fcc_val(pm);

	if (effective_fcc_val > 0) {
		iout_max_limit = effective_fcc_val / 4;
		iout_max_limit = min(effective_iout_current_max, iout_max_limit);
		pr_info("iout_max_limit :%d\n", iout_max_limit);
	}

	if ( iout_max_limit <= IOUT_SAFTY_THRESHOLD)
		rx_iout_limit = iout_max_limit + 50;
	else
		rx_iout_limit = iout_max_limit - 10;

	/* reduce iout current in cv loop */
	if (pm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 50) {
		if (iout_lmt_change_timer++ > IOUT_CHANGE_TIMEOUT
				&& !pm->use_qcom_gauge) {
			iout_lmt_change_timer = 0;
			rx_iout_limit = iout_max_limit - 100;
			effective_fcc_taper = wldc_get_effective_fcc_val(pm);
			effective_fcc_taper -= WLDC_BQ_TAPER_DECREASE_STEP_MA;
			pr_err("bq set taper fcc to: %d mA\n", effective_fcc_taper);
			if (pm->fcc_votable)
				vote(pm->fcc_votable, BQ_TAPER_FCC_VOTER,
					true, effective_fcc_taper * 1000);

		}
	} else if (pm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - 250) {
		//rx_iout_limit = iout_max_limit;
		iout_lmt_change_timer = 0;
	} else {
		iout_lmt_change_timer = 0;
	}
	pr_info("rx_iout_limit:%d\n", rx_iout_limit);

	/* battery voltage loop*/
	if (pm->cp.vbat_volt > pm_config.bat_volt_lp_lmt)
		step_vbat = -pm_config.fc2_steps;
	else if (pm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - 10)
		step_vbat = pm_config.fc2_steps;;

	/* bus current loop*/
	ibus_total = pm->cp.ibus_curr;

	if (pm_config.cp_sec_enable)
		ibus_total += pm->cp_sec.ibus_curr;

	ret = wireless_charge_get_rx_iout(pm, &rx_iout);

	pr_err("ibus_total_ma: %d, vbus_mv = %d vbat_mv: %d "
			"ibat_ma: %d, rx_iout_ma: %d\n",
			ibus_total, pm->cp.vbus_volt, pm->cp.vbat_volt,
			pm->cp.ibat_curr, rx_iout);

	if (rx_iout < rx_iout_limit - 100)
		step_iout = pm_config.fc2_steps;
	else if (rx_iout > rx_iout_limit)
		step_iout = -pm_config.fc2_steps;
	pr_info("step_iout:%d\n", step_iout);

	pr_info("pm->cp.vbat_reg:%d, pm->cp.ibat_reg:%d\n",
			pm->cp.vbat_reg, pm->cp.ibat_reg);
	/* hardware regulation loop*/
	if (pm->cp.vbat_reg)
		step_bat_reg = -pm_config.fc2_steps;
	else
		step_bat_reg = pm_config.fc2_steps;

	/* hardware regulation loop used for quick bq taper */
	if (pm->cp.vbat_reg) {
		effective_fcc_val = wldc_get_effective_fcc_val(pm);
			effective_fcc_val = min(effective_fcc_val,
						effective_iout_current_max * 4);
			effective_fcc_val -= WLDC_BQ_VBAT_REGULATION_STEP_MA;
			pr_err("bq vbat_regulation hardware triggered, reduce fcc to: %d\n",
						effective_fcc_val);
			if (pm->fcc_votable)
				vote(pm->fcc_votable, BQ_TAPER_FCC_VOTER,
					true, effective_fcc_val * 1000);
	}

	pr_info("step_bat_reg:%d\n", step_bat_reg);
	sw_ctrl_steps = min(step_vbat, step_iout);
	sw_ctrl_steps = min(sw_ctrl_steps, step_bat_reg);

	pr_info("sw_ctrl_steps:%d\n", sw_ctrl_steps);
	/* hardware alarm loop */
	if (pm->cp.bus_ocp_alarm || pm->cp.bus_ovp_alarm)
		hw_ctrl_steps = -pm_config.fc2_steps;
	else
		hw_ctrl_steps = pm_config.fc2_steps;
	pr_info("hw_ctrl_steps:%d\n", hw_ctrl_steps);
	/* check if cp disabled due to other reason*/
	wldc_pm_check_cp_enabled(pm);

	if (pm_config.cp_sec_enable)
		wldc_pm_check_cp_sec_enabled(pm);

	if (pm->cp.bat_therm_fault) { /* battery overheat, stop charge*/
		pr_info("bat_therm_fault:%d\n", pm->cp.bat_therm_fault);
		return PM_ALGO_RET_THERM_FAULT;
	} else if (pm->cp.bat_ocp_fault || pm->cp.bus_ocp_fault
			|| pm->cp.bat_ovp_fault || pm->cp.bus_ovp_fault) {
		pr_info("bat_ocp_fault:%d, bus_ocp_fault:%d, bat_ovp_fault:%d, bus_ovp_fault:%d\n",
				pm->cp.bat_ocp_fault, pm->cp.bus_ocp_fault,
				pm->cp.bat_ovp_fault, pm->cp.bus_ovp_fault);
		return PM_ALGO_RET_OTHER_FAULT; /* go to switch, and try to ramp up*/
	} else if (!pm->cp.charge_enabled
			/* || (pm_config.cp_sec_enable && !pm->cp_sec.charge_enabled)*/) {
		pr_info("cp.charge_enabled:%d, cp_sec.charge_enabled:%d\n",
				pm->cp.charge_enabled, pm->cp_sec.charge_enabled);
		return PM_ALGO_RET_CHG_DISABLED;
	}

	ret = wldc_pm_get_batt_capacity(pm, &soc);
	if (ret) {
		pr_info("read soc error:%d\n", ret);
	} else if (soc > CAPACITY_TOO_HIGH_THR) {
		pr_info("high soc:%d, close bq pump\n", soc);
		return PM_ALGO_RET_CHG_DISABLED;
	}

	/* charge pump taper charge */
	if (pm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 80
			&& pm->cp.ibat_curr < pm_config.fc2_taper_current) {
		if (fc2_taper_timer++ > TAPER_TIMEOUT) {
			pr_info("charge pump taper charging done\n");
			fc2_taper_timer = 0;
			return PM_ALGO_RET_TAPER_DONE;
		}
	} else {
		fc2_taper_timer = 0;
	}

	/* do thermal and jeita check */
	wl_get_batt_current_thermal_level(pm, &thermal_level);
	pm->is_temp_out_fc2_range = wl_disable_cp_by_jeita_status(pm);
	pr_info("wl is_temp_out_fc2_range:%d\n", pm->is_temp_out_fc2_range);

	if ( thermal_level >= MAX_THERMAL_LEVEL || pm->is_temp_out_fc2_range ) {
		pr_info("cp.is_temp_out_fc2_range:%d thermal_level:%d\n",
			pm->is_temp_out_fc2_range, thermal_level);
		return PM_ALGO_RET_CHG_DISABLED;
	}

	steps = min(sw_ctrl_steps, hw_ctrl_steps);
	pr_info("steps: %d, sw_ctrl_steps:%d, hw_ctrl_steps:%d\n", steps, sw_ctrl_steps, hw_ctrl_steps);
	pm->rx_vout_set += steps * step_mv;

	if (pm->rx_vout_set > pm->wl_info.max_volt)
		pm->rx_vout_set = pm->wl_info.max_volt;

	pr_info("steps: %d, pm->rx_vout_set: %d\n", steps, pm->rx_vout_set);

	return PM_ALGO_RET_OK;
}

static const unsigned char *pm_str[] = {
	"CP_PM_STATE_ENTRY",
	"CP_PM_STATE_FC2_ENTRY",
	"CP_PM_STATE_FC2_ENTRY_1",
	"CP_PM_STATE_FC2_ENTRY_2",
	"CP_PM_STATE_FC2_ENTRY_3",
	"CP_PM_STATE_FC2_TUNE",
	"CP_PM_STATE_FC2_EXIT",
};

static void wldc_pm_move_state(struct wireless_dc_device_info *pm, enum pm_state state)
{
#if 1
	pr_info("state change:%s -> %s\n",
		pm_str[pm->state], pm_str[state]);
#endif
	pm->state = state;
}

static int wldc_get_usb_icl_val(struct wireless_dc_device_info *pm)
{
	int effective_usb_icl_val = 0;

	if (!pm->usb_icl_votable)
		pm->usb_icl_votable = find_votable("USB_ICL");

	if (!pm->usb_icl_votable)
		return -EINVAL;

	effective_usb_icl_val = get_effective_result(pm->usb_icl_votable);
	effective_usb_icl_val = effective_usb_icl_val/1000;
	pr_info("effective_usb_icl_val: %d\n", effective_usb_icl_val);
	return effective_usb_icl_val;
}


#define MAX_OPEN_RETRY_TIMES 10
#define MAX_MAIN_CHARGER_ICL_UA 1000000
#define MAX_MAIN_CHARGER_ICL_STEP_UA 300000
#define MIN_FCC_FOR_OPEN_BQ_MA 2000
#define MIN_FCC_FOR_SINGLE_BQ_MA 3000
#define FCC_HYSTERSIS_FOR_TWIN_BQ_MA 0
#define MIN_IBUS_FOR_SINGLE_BQ_MA 750
#define MIN_IBUS_FOR_TWIN_BQ_MA 500
static int wldc_pm_sm(struct wireless_dc_device_info *pm)
{
	int ret;
	int rc = 0;
	static bool stop_sw;
	static bool recover;
	static int open_retry_times;
	int tx_adapter_type = 0, capacity = 0;
	int thermal_level = 0;
	int effective_usb_icl = 0;
	int effective_fcc_val = 0;
	int no_need_slave_bq = 0;
	switch (pm->state) {
	case CP_PM_STATE_ENTRY:
		stop_sw = false;
		recover = false;
		open_retry_times = 0;

		/* first check whether are our defined quick chargers */
		ret = wireless_charge_get_tx_adapter_type(pm, &tx_adapter_type);
		ret = wldc_pm_get_batt_capacity(pm, &capacity);
		effective_fcc_val = wldc_get_effective_fcc_val(pm);

		wl_get_batt_current_thermal_level(pm, &thermal_level);
		pm->is_temp_out_fc2_range = wl_disable_cp_by_jeita_status(pm);
		pr_info("is_temp_out_fc2_range:%d\n", pm->is_temp_out_fc2_range);

		if (pm->cp.vbat_volt < pm_config.min_vbat_for_cp) {
			pr_info("batt_volt %d, waiting...\n", pm->cp.vbat_volt);
		} else if (tx_adapter_type > ADAPTER_XIAOMI_PD_60W ||
			tx_adapter_type < ADAPTER_XIAOMI_QC3) {
			pr_info("not our defined quick chargers, waiting...\n");
		} else if (pm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - VBAT_HIGH_FOR_FC_HYS_MV
				|| capacity >= CAPACITY_TOO_HIGH_THR) {
			pr_info("batt_volt %d or capacity is too high for cp,\
					charging with switch charger\n",
					pm->cp.vbat_volt);
			wldc_pm_move_state(pm, CP_PM_STATE_FC2_EXIT);
		} else if (thermal_level >= MAX_THERMAL_LEVEL || pm->is_temp_out_fc2_range) {
			pr_info("thermal level is too high, waiting...\n");
		} else if (effective_fcc_val <= MIN_FCC_FOR_OPEN_BQ_MA) {
			pr_info("fcc %d is too low, waiting...\n", effective_fcc_val);
		} else {
			pr_info("batt_volt-%d is ok, start flash charging\n",
					pm->cp.vbat_volt);
			wldc_pm_move_state(pm, CP_PM_STATE_FC2_ENTRY);
		}
		break;

	case CP_PM_STATE_FC2_ENTRY:
		wldc_pm_move_state(pm, CP_PM_STATE_FC2_ENTRY_1);
		break;

	case CP_PM_STATE_FC2_ENTRY_1:
		wldc_pm_move_state(pm, CP_PM_STATE_FC2_ENTRY_2);
		effective_usb_icl = wldc_get_usb_icl_val(pm);
		effective_usb_icl -= MAX_MAIN_CHARGER_ICL_STEP_UA;
		if (pm->usb_icl_votable) {
			while (effective_usb_icl > MAX_MAIN_CHARGER_ICL_UA ) {
				vote(pm->usb_icl_votable, STEP_BMS_CHG_VOTER, true, effective_usb_icl);
				effective_usb_icl -= MAX_MAIN_CHARGER_ICL_STEP_UA;
				pr_info("smooth down the icl \n");
				msleep(50);
			}
			vote(pm->usb_icl_votable, STEP_BMS_CHG_VOTER, true, MAX_MAIN_CHARGER_ICL_UA);
		}
		break;

	case CP_PM_STATE_FC2_ENTRY_2:
		ret = wldc_security_confirm_before_start_fc2_charge(pm);
		if(!ret) {
			wldc_pm_move_state(pm, CP_PM_STATE_FC2_ENTRY_3);
			break;
		} else {
			pr_info("wldc security confirm failed, enter state fc2_entry_1 now\n");
			wldc_pm_move_state(pm, CP_PM_STATE_FC2_ENTRY_1);
		}
		break;

	case CP_PM_STATE_FC2_ENTRY_3:
		/* soft start to open bq direct charging path */
		ret = wldc_soft_start_to_open_dc_path(pm);

		/* wireless disconnect while open bq */
		if (ret == -WL_DISCONNECTED_ERR) {
			wldc_pm_move_state(pm, CP_PM_STATE_ENTRY);
			break;
		}

		/* if can not oper BQ after retries, exit sm*/
		if (ret == -RX_VOUT_SET_TOO_HIGH_ERR) {
			pr_err("%s: open bq failed %d \n", __func__, open_retry_times);
			if ( ++open_retry_times >= MAX_OPEN_RETRY_TIMES) {
				open_retry_times = 0;
				pr_err("%s: open bq failed, exit\n", __func__);
				wldc_pm_move_state(pm, CP_PM_STATE_FC2_EXIT);
				break;
			}
		}

		if (ret) {
			wldc_pm_move_state(pm, CP_PM_STATE_FC2_ENTRY_1);
			break;
		}
		/* recheck master bq status */
		if (!pm->cp.charge_enabled) {
			ret = wldc_pm_enable_cp(pm, true);
			ret = wldc_pm_check_cp_enabled(pm);
		}

		/* close main charger */
		if (pm_config.fc2_disable_sw && pm->cp.charge_enabled) {
			if (pm->sw.charge_enabled) {
				wldc_pm_enable_sw(pm, false);
				wldc_pm_check_sw_enabled(pm);
			}
		}
		msleep(1000);
		effective_fcc_val = wldc_get_effective_fcc_val(pm);
		/* start to open slave bq */
		if (pm->cp_sec_enable && effective_fcc_val >= MIN_FCC_FOR_SINGLE_BQ_MA)
			ret = wldc_pm_check_cp_sec_enabled(pm);
		/* recheck slave bq status */
		if (pm->cp.charge_enabled
				&& (pm->cp_sec_enable && !pm->cp_sec.charge_enabled)
				&& effective_fcc_val >= MIN_FCC_FOR_SINGLE_BQ_MA){
			ret = wldc_pm_enable_cp_sec(pm, true);
			ret = wldc_pm_check_cp_sec_enabled(pm);
			no_need_slave_bq = 0;
		} else {
			no_need_slave_bq = 1;
		}

		if (pm->cp.charge_enabled) {
			if (no_need_slave_bq ||(pm_config.cp_sec_enable && pm->cp_sec.charge_enabled)
					|| !pm_config.cp_sec_enable) {

				wldc_pm_move_state(pm, CP_PM_STATE_FC2_TUNE);
				iout_lmt_change_timer = 0;
				fc2_taper_timer = 0;
			}
		}
		pr_err("%s: cp charge_enable is false\n", __func__);
		break;

	case CP_PM_STATE_FC2_TUNE:
		ret = wldc_pm_fc2_charge_algo(pm);
		if (ret == PM_ALGO_RET_THERM_FAULT) {
			pr_info("Move to stop charging:%d\n", ret);
			stop_sw = true;
			wldc_pm_move_state(pm, CP_PM_STATE_FC2_EXIT);
			break;
		} else if (ret == PM_ALGO_RET_OTHER_FAULT || ret == PM_ALGO_RET_TAPER_DONE) {
			pr_info("Move to switch charging:%d\n", ret);
			wldc_pm_move_state(pm, CP_PM_STATE_FC2_EXIT);
			break;
		} else if (ret == PM_ALGO_RET_CHG_DISABLED) {
			pr_info("Move to switch charging, will try to recover flash charging:%d\n",
					ret);
			recover = true;
			wldc_pm_move_state(pm, CP_PM_STATE_FC2_EXIT);
			break;
		} else {
			wldc_regulate_power(pm, pm->rx_vout_set);
			pr_info("rx_vout_set:%d\n", pm->rx_vout_set);
		}
		/*stop second charge pump if either of ibus is lower than 500ma during CV */
		if (pm_config.cp_sec_enable && pm->cp_sec.charge_enabled
				&& pm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 80
				&& (pm->cp.ibus_curr < MIN_IBUS_FOR_TWIN_BQ_MA || pm->cp_sec.ibus_curr < MIN_IBUS_FOR_TWIN_BQ_MA)) {
			pr_info("second cp is disabled due to ibus < 500mA\n");
			wldc_pm_enable_cp_sec(pm, false);
			wldc_pm_check_cp_sec_enabled(pm);
		}

		/* close master bq while ibus is less than 750ma */
		if ( pm->cp.charge_enabled
				&& !pm->cp_sec.charge_enabled
				&& pm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 80
				&& pm->cp.ibus_curr < MIN_IBUS_FOR_SINGLE_BQ_MA) {
			pr_info("master cp is disabled due sto ibus < 750 ma \n");
			wldc_pm_move_state(pm, CP_PM_STATE_FC2_EXIT);
			break;
		}

		effective_fcc_val = wldc_get_effective_fcc_val(pm);
		if (!wldc_is_fcc_voter_esr(pm)) {
			if (effective_fcc_val <= MIN_FCC_FOR_OPEN_BQ_MA) {
				/* close BQs */
				pr_info("disable bq because of fcc :%d ma \n", effective_fcc_val);
				wldc_pm_move_state(pm, CP_PM_STATE_FC2_EXIT);
				/* we need recovery after fcc is larger later */
				recover = true;
				break;
			} else if (effective_fcc_val < MIN_FCC_FOR_SINGLE_BQ_MA) {
				/* close slave bq*/
				if (pm_config.cp_sec_enable && pm->cp_sec.charge_enabled) {
					pr_info("second cp is disabled due to fcc :%d too low \n", effective_fcc_val);
					wldc_pm_enable_cp_sec(pm, false);
					wldc_pm_check_cp_sec_enabled(pm);
				}
				break;
			} else if (effective_fcc_val > (MIN_FCC_FOR_SINGLE_BQ_MA + FCC_HYSTERSIS_FOR_TWIN_BQ_MA)) {
				/* reopen slave bq */
				if (pm_config.cp_sec_enable
					&& !pm->cp_sec.charge_enabled
					&& pm->cp.ibus_curr >= MIN_IBUS_FOR_SINGLE_BQ_MA) {
					pr_info("second cp is enabled due to fcc :%d\n", effective_fcc_val);
					ret = wldc_pm_enable_cp_sec(pm, true);
					ret = wldc_pm_check_cp_sec_enabled(pm);
				}
				break;
			}
		}

		break;

	case CP_PM_STATE_FC2_EXIT:
		/* select default 11V (5.5V *2 will be set in rx driver side) */
		//wldc_regulate_power(pm, WLDC_DEFAULT_RX_VOUT_FOR_MAIN_CHARGER);

		if (pm->fcc_votable) {
			vote(pm->fcc_votable, BQ_TAPER_FCC_VOTER,
					false, 0);
			vote(pm->fcc_votable, BQ_TAPER_CELL_HGIH_FCC_VOTER,
					false, 0);
		}

		/* open main charger before close bq */
		if (stop_sw)
			wldc_pm_enable_sw(pm, false);
		else if (!stop_sw)
			wldc_pm_enable_sw(pm, true);

		wldc_pm_check_sw_enabled(pm);

		msleep(300);

		if (pm->cp.charge_enabled) {
			wldc_pm_enable_cp(pm, false);
			wldc_pm_check_cp_enabled(pm);
		}

		if (pm_config.cp_sec_enable && pm->cp_sec.charge_enabled) {
			wldc_pm_enable_cp_sec(pm, false);
			wldc_pm_check_cp_sec_enabled(pm);
		}

		if (pm->usb_icl_votable)
			vote(pm->usb_icl_votable, STEP_BMS_CHG_VOTER, false, 0);


		while (pm->rx_vout_set > 7100) {
			pm->rx_vout_set -= 500;
			if (pm->rx_vout_set >= 7100) {
				ret = wireless_charge_set_rx_vout(pm, pm->rx_vout_set);
				if (ret) {
					pr_err("%s: set rx vout fail\n", __func__);
					break;
				}
			} else {
				pm->rx_vout_set = 7100;
				ret = wireless_charge_set_rx_vout(pm, pm->rx_vout_set);
				if (ret) {
					pr_err("%s: set rx vout fail\n", __func__);
					break;
				}
				break;
			}
			msleep(300);
		}

		if (recover)
			wldc_pm_move_state(pm, CP_PM_STATE_ENTRY);
		else
			rc = 1;

		break;
	default:
		wldc_pm_move_state(pm, CP_PM_STATE_ENTRY);
		break;
	}

	return rc;
}

static void wldc_dc_ctrl_workfunc(struct work_struct *work)
{
	struct wireless_dc_device_info *pm = container_of(work,
				struct wireless_dc_device_info, wireles_dc_ctrl_work.work);

	wldc_pm_update_sw_status(pm);
	wldc_pm_update_cp_status(pm);
	wldc_pm_update_cp_sec_status(pm);

	if (!wldc_pm_sm(pm) && pm->wl_info.active)
		schedule_delayed_work(&pm->wireles_dc_ctrl_work,
				msecs_to_jiffies(WLDC_WORK_RUN_INTERVAL));
}

static void wldc_wl_disconnect(struct wireless_dc_device_info *pm)
{
	if (pm->fcc_votable) {
		vote(pm->fcc_votable, BQ_TAPER_FCC_VOTER,
				false, 0);
		vote(pm->fcc_votable, BQ_TAPER_CELL_HGIH_FCC_VOTER,
				false, 0);
	}

	if (pm->usb_icl_votable)
		vote(pm->usb_icl_votable, STEP_BMS_CHG_VOTER, false, 0);


	cancel_delayed_work_sync(&pm->wireles_dc_ctrl_work);
	pm->jeita_triggered = false;
	pm->is_temp_out_fc2_range = false;
	pm->over_cell_vol_high_count = 0;
	pm->over_cell_vol_max_count = 0;
	pm_config.bat_curr_lp_lmt = pm->bat_curr_max;
	wldc_pm_move_state(pm, CP_PM_STATE_ENTRY);
}

static void wldc_wl_contact(struct wireless_dc_device_info *pm, bool connected)
{
	pm->wl_info.active = connected;

	if (connected) {
		schedule_delayed_work(&pm->wireles_dc_ctrl_work, 0);
	} else {
		wldc_wl_disconnect(pm);
	}
}


static void cp_psy_change_work(struct work_struct *work)
{
	struct wireless_dc_device_info *pm = container_of(work,
				struct wireless_dc_device_info, cp_psy_change_work);
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
	pm->psy_change_running = false;
}

static void usb_psy_change_work(struct work_struct *work)
{
	struct wireless_dc_device_info *pm = container_of(work,
				struct wireless_dc_device_info, usb_psy_change_work);

	//TODO
	pm->psy_change_running = false;
	return;
}

static void wl_psy_change_work(struct work_struct *work)
{
	struct wireless_dc_device_info *pm = container_of(work,
				struct wireless_dc_device_info, wl_psy_change_work);
	union power_supply_propval val = {0,};
	int ret = 0;

	pr_info("enter");

	ret = power_supply_get_property(pm->wl_psy,
			POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);

	if (ret) {
		pr_err("Failed to read wl cp enable status\n");
		goto out;
	}
	pr_info("wl_info.active:%d, psy new value:%d",
			pm->wl_info.active, val.intval);

	if (!pm->wl_info.active && val.intval)
		wldc_wl_contact(pm, true);
	else if (pm->wl_info.active && !val.intval)
		wldc_wl_contact(pm, false);

out:
	pm->psy_change_running = false;
	pr_info("exit");
}


static int wldc_psy_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct wireless_dc_device_info *pm = container_of(nb,
				struct wireless_dc_device_info, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	wldc_check_cp_psy(pm);
	wldc_check_usb_psy(pm);
	wldc_check_wl_psy(pm);

	if (!pm->cp_psy || !pm->usb_psy || !pm->wl_psy)
		return NOTIFY_OK;

	if (psy == pm->cp_psy || psy == pm->usb_psy
			|| psy == pm->wl_psy) {
		spin_lock_irqsave(&pm->psy_change_lock, flags);
		if (!pm->psy_change_running) {
			pm->psy_change_running = true;
			if (psy == pm->cp_psy)
				schedule_work(&pm->cp_psy_change_work);
			else if (psy == pm->usb_psy)
				schedule_work(&pm->usb_psy_change_work);
			else
				schedule_work(&pm->wl_psy_change_work);
		}
		spin_unlock_irqrestore(&pm->psy_change_lock, flags);
	}

	return NOTIFY_OK;
}

static int wldc_charge_parse_dt(struct wireless_dc_device_info *pm)
{
	struct device_node *node = pm->dev->of_node;
	int rc = 0;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node,
			"mi,wc-dc-bat-volt-max", &pm->bat_volt_max);
	if (rc < 0)
		pr_err("wc-dc-bat-volt-max property missing, use default val\n");
	else
		pm_config.bat_volt_lp_lmt = pm->bat_volt_max;
	pr_info("pm_config.bat_volt_lp_lmt:%d\n", pm_config.bat_volt_lp_lmt);

	rc = of_property_read_u32(node,
			"mi,wc-dc-bat-curr-max", &pm->bat_curr_max);
	if (rc < 0)
		pr_err("wc-dc-bat-curr-max property missing, use default val\n");
	else
		pm_config.bat_curr_lp_lmt = pm->bat_curr_max;
	pr_info("pm_config.bat_curr_lp_lmt:%d\n", pm_config.bat_curr_lp_lmt);

	rc = of_property_read_u32(node,
			"mi,wc-dc-bus-volt-max", &pm->bus_volt_max);
	if (rc < 0)
		pr_err("wc-dc-bus-volt-max property missing, use default val\n");
	else
		pm_config.bus_volt_lp_lmt = pm->bus_volt_max;
	pr_info("pm_config.bus_volt_lp_lmt:%d\n", pm_config.bus_volt_lp_lmt);

	rc = of_property_read_u32(node,
			"mi,wc-dc-bus-curr-max", &pm->bus_curr_max);
	if (rc < 0)
		pr_err("wc-dc-bus-curr-max property missing, use default val\n");
	else
		pm_config.bus_curr_lp_lmt = pm->bus_curr_max;
	pr_info("pm_config.bus_curr_lp_lmt:%d\n", pm_config.bus_curr_lp_lmt);

	rc = of_property_read_u32(node,
			"mi,wc-dc-rx-iout-curr-max", &pm->rx_iout_curr_max);
	if (rc < 0) {
		pr_err("wc-dc-rx-iout-curr-max property missing, use default val\n");
		pm->rx_iout_curr_max = 1500;
	}

	pm->cp_sec_enable = of_property_read_bool(node,
				"mi,cp-sec-enable");
	pm_config.cp_sec_enable = pm->cp_sec_enable;
	pr_info("pm_config.cp_sec_enable:%d\n", pm_config.cp_sec_enable);

	pm->use_qcom_gauge = of_property_read_bool(node,
			"mi,use-qcom-gauge");

	rc = of_property_read_u32(node,
			"mi,wc-non-ffc-bat-volt-max", &pm->non_ffc_bat_volt_max);

	pr_info("pm->non_ffc_bat_volt_max:%d\n",
				pm->non_ffc_bat_volt_max);

	if (!pm->use_qcom_gauge ) {
		rc = of_property_read_u32(node,
				"mi,step-charge-high-vol-curr-max", &pm->step_charge_high_vol_curr_max);

		pr_info("pm->step_charge_high_vol_curr_max:%d\n",
					pm->step_charge_high_vol_curr_max);

		rc = of_property_read_u32(node,
				"mi,cell-vol-high-threshold-mv", &pm->cell_vol_high_threshold_mv);

		pr_info("pm->cell_vol_high_threshold_mv:%d\n",
					pm->cell_vol_high_threshold_mv);

		rc = of_property_read_u32(node,
				"mi,cell-vol-max-threshold-mv", &pm->cell_vol_max_threshold_mv);

		pr_info("pm->cell_vol_max_threshold_mv:%d\n",
					pm->cell_vol_max_threshold_mv);
	}

	return rc;
}

static int wldc_pm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct wireless_dc_device_info *pm;

	pr_info("%s enter\n", __func__);

	pm = kzalloc(sizeof(struct wireless_dc_device_info), GFP_KERNEL);
	if (!pm)
		return -ENOMEM;

	__pm = pm;

	pm->dev = dev;

	ret = wldc_charge_parse_dt(pm);
	if (ret < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, pm);

	/* Init wireless info max_volt and step_mv */
	pm->wl_info.max_volt = WLDC_MAX_VOL_THR_FOR_BQ;
	pm->wl_info.step_mv = WLDC_REGULATE_STEP_MV;
	pm->rx_init_vout_for_fc2 = WLDC_INIT_RX_VOUT_FOR_FC2;

	spin_lock_init(&pm->psy_change_lock);

	wldc_check_cp_psy(pm);
	wldc_check_cp_sec_psy(pm);
	wldc_check_usb_psy(pm);
	wldc_check_wl_psy(pm);
	wldc_check_dc_psy(pm);

	pm->over_cell_vol_high_count = 0;
	pm->over_cell_vol_max_count = 0;

	INIT_WORK(&pm->cp_psy_change_work, cp_psy_change_work);
	INIT_WORK(&pm->usb_psy_change_work, usb_psy_change_work);
	INIT_WORK(&pm->wl_psy_change_work, wl_psy_change_work);
	INIT_DELAYED_WORK(&pm->wireles_dc_ctrl_work, wldc_dc_ctrl_workfunc);

	if (pm->use_qcom_gauge)
		pm->warm_threshold_temp = JEITA_WARM_THR;
	else
		pm->warm_threshold_temp = JEITA_WARM_THR_NON_QCOM_GAUGE;

	pm->nb.notifier_call = wldc_psy_notifier_cb;
	power_supply_reg_notifier(&pm->nb);

	return ret;
}

static int wldc_pm_remove(struct platform_device *pdev)
{
	power_supply_unreg_notifier(&__pm->nb);
	cancel_delayed_work_sync(&__pm->wireles_dc_ctrl_work);
	cancel_work_sync(&__pm->cp_psy_change_work);
	cancel_work_sync(&__pm->usb_psy_change_work);

	return 0;
}

static const struct of_device_id wldc_pm_of_match[] = {
	{ .compatible = "xiaomi,wldc_bq", },
	{},
};

static struct platform_driver wldc_pm_driver = {
	.driver = {
		.name = "wldc-pm",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(wldc_pm_of_match),
	},
	.probe = wldc_pm_probe,
	.remove = wldc_pm_remove,
};

static int __init wldc_pm_init(void)
{
	return platform_driver_register(&wldc_pm_driver);
}

late_initcall(wldc_pm_init);

static void __exit wldc_pm_exit(void)
{
	return platform_driver_unregister(&wldc_pm_driver);
}
module_exit(wldc_pm_exit);

MODULE_AUTHOR("Fei Jiang<jiangfei1@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi wireless dc charge statemachine for bq");
MODULE_LICENSE("GPL");

