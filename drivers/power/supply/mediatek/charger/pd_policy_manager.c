
#define pr_fmt(fmt)	"[USBPD-PM]: %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include "pd_policy_manager.h"
#include <mt-plat/prop_chgalgo_class.h>

#define PCA_PPS_CMD_RETRY_COUNT	2
/* +HONGMI-88979,wangbin wt.ADD,20210804,add cv set to  4.1v in dis-temp version*/
#ifdef CONFIG_MTK_DISABLE_TEMP_PROTECT
#define BATT_MAX_CHG_VOLT		4100
#else
#define BATT_MAX_CHG_VOLT		4460
#endif
/* -HONGMI-88979,wangbin wt.ADD,20210804,add cv set to  4.1v in dis-temp version*/

/* +Bug651592 caijiaqi.wt,20210609,ADD BATTERY CURRENT jeita */
#define BATT_FAST_CHG_CURR		5800
#define BUS_OVP_THRESHOLD		10500
/* -Bug651592 caijiaqi.wt,20210609,ADD BATTERY CURRENT jeita */

#define BUS_VOLT_INIT_UP		300

#define BAT_VOLT_LOOP_LMT		BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT		BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT		BUS_OVP_THRESHOLD

#define PM_WORK_RUN_INTERVAL		200

/* +Bug651592 caijiaqi.wt,20210609,ADD BATTERY CURRENT jeita */
#define CHG_BAT_TEMP_MIN      100
#define CHG_BAT_TEMP_15       150
#define FFC_BAT_TEMP_OFFSET   20
#define CHG_BAT_TEMP_MAX      450
#define BAT_TEMP_300          300
#define BAT_TEMP_340          340
#define BAT_TEMP_360          360
#define BAT_TEMP_370          370
#define BAT_TEMP_380          380
#define BAT_TEMP_390          390
#define BAT_TEMP_400          400
#define BAT_TEMP_410          410
#define BAT_TEMP_420          420
#define BAT_TEMP_430          430
#define BAT_TEMP_440          440
#define BAT_TEMP_460          460
#define BAT_CURR_6000MA       5800
#define BAT_CURR_5700MA       5700
#define BAT_CURR_5400MA       5050
#define BAT_CURR_5000MA       5300
#define BAT_CURR_4500MA       4400
#define BAT_CURR_4000MA       3900
#define BAT_CURR_3900MA       3600
#define BAT_CURR_3800MA       3700
#define BAT_CURR_3500MA       3500
#define BAT_CURR_3000MA       3000
#define BAT_CURR_2800MA       2800
#define BAT_CURR_2500MA       2500
#define BAT_CURR_2000MA       2000
#define BAT_CURR_1500MA       1500
#define BAT_CURR_1000MA       1000
#define BAT_CURR_100MA        100
/* +HONGMI-88979,wangbin wt.ADD,20210804,add cv set to  4.1v in dis-temp version*/
#ifdef CONFIG_MTK_DISABLE_TEMP_PROTECT
#define CHG_CUR_VOLT          4100
#define CHG_CUR_VOLT2         4100
#define CHG_CUR_VOLT3         4100
#else
#define CHG_CUR_VOLT          4240
#define CHG_CUR_VOLT2         4440
#define CHG_CUR_VOLT3         4470
#endif
/* -HONGMI-88979,wangbin wt.ADD,20210804,add cv set to  4.1v in dis-temp version*/
#define CHG_TEMP_STEP1        1
#define CHG_TEMP_STEP2        2
#define CHG_TEMP_STEP3        3
#define CHG_TEMP_STEP4        4
#define CHG_TEMP_STEP5        5
#define CHG_TEMP_STEP6        6
#define CHG_TEMP_OFFSET       10

extern int get_jeita_lcd_on_off(void);
/* -Bug651592 caijiaqi.wt,20210609,ADD BATTERY CURRENT jeita */
//Extb HONGMI-85045,ADD,wangbin.wt.20210709.add charging call state limit
extern bool get_charging_call_state(void);
extern bool is_kernel_power_off_charging(void);

enum {
	PM_ALGO_RET_OK,
	PM_ALGO_RET_THERM_FAULT,
	PM_ALGO_RET_OTHER_FAULT,
	PM_ALGO_RET_CHG_DISABLED,
	PM_ALGO_RET_TAPER_DONE,
};

enum {
	SC8551_2_1_CHARGER_MODE = 0,
	SC8551_1_1_CHARGER_MODE,
};


static const struct pdpm_config pm_config = {
	.bat_volt_lp_lmt		= BAT_VOLT_LOOP_LMT,
	.bat_curr_lp_lmt		= BAT_CURR_LOOP_LMT,
	.bus_volt_lp_lmt		= BUS_VOLT_LOOP_LMT,
	.bus_curr_lp_lmt		= (BAT_CURR_LOOP_LMT >> 1),

	.fc2_taper_current		= 2000,
	.fc2_steps				= 1,

	.min_adapter_volt_required	= 11000,
	.min_adapter_curr_required	= 2000,

	.min_vbat_for_cp		= 3500,

	.cp_sec_enable          = false,
	.fc2_disable_sw			= true,
};

static struct usbpd_pm *__pdpm;

static int fc2_taper_timer;
static int ibus_lmt_change_timer;

int pdpm_is_charge_pump_enable(void)
{
	if(__pdpm)
		return __pdpm->is_cp_ok;
	return 0;
}

/*******************************PD API******************************/
static inline int check_typec_attached_snk(struct tcpc_device *tcpc)
{
	if (tcpm_inquire_typec_attach_state(tcpc) != TYPEC_ATTACHED_SNK)
		return -EINVAL;
	return 0;
}

static int usbpd_pps_enable_charging(struct usbpd_pm *pdpm, bool en,
				   u32 mV, u32 mA)
{
	int ret, cnt = 0;

	if (check_typec_attached_snk(pdpm->tcpc) < 0)
		return -EINVAL;
	pr_err("en = %d, %dmV, %dmA\n", en, mV, mA);

	do {
		if (en)
			ret = tcpm_set_apdo_charging_policy(pdpm->tcpc,
				DPM_CHARGING_POLICY_PPS, mV, mA, NULL);
		else
			ret = tcpm_reset_pd_charging_policy(pdpm->tcpc, NULL);
		cnt++;
	} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

	if (ret != TCP_DPM_RET_SUCCESS)
		pr_err("fail(%d)\n", ret);
	return ret > 0 ? -ret : ret;
}

static bool usbpd_get_pps_status(struct usbpd_pm *pdpm)
{
	int ret, apdo_idx = -1;
	struct tcpm_power_cap_val apdo_cap = {0};
	u8 cap_idx;
	//u32 vta_meas, ita_meas, prog_mv;

	pr_err("++\n");

	if (check_typec_attached_snk(pdpm->tcpc) < 0)
		return false;
	

	if (!pdpm->is_pps_en_unlock) {
		pr_err("pps en is locked\n");
		return false;
	}

	if (!tcpm_inquire_pd_pe_ready(pdpm->tcpc)) {
		pr_err("PD PE not ready\n");
		return false;
	}

	/* select TA boundary */
	cap_idx = 0;
	while (1) {
		ret = tcpm_inquire_pd_source_apdo(pdpm->tcpc,
						  TCPM_POWER_CAP_APDO_TYPE_PPS,
						  &cap_idx, &apdo_cap);
		if (ret != TCP_DPM_RET_SUCCESS) {
			pr_err("inquire pd apdo fail(%d)\n", ret);
			break;
		}

		pr_err("cap_idx[%d], %d mv ~ %d mv, %d ma, pl: %d\n", cap_idx,
			 apdo_cap.min_mv, apdo_cap.max_mv, apdo_cap.ma,
			 apdo_cap.pwr_limit);

		/*
		 * !(apdo_cap.min_mv <= data->vcap_min &&
		 *   apdo_cap.max_mv >= data->vcap_max &&
		 *   apdo_cap.ma >= data->icap_min)
		 */
		if (apdo_cap.max_mv < pm_config.min_adapter_volt_required ||
		    apdo_cap.ma < pm_config.min_adapter_curr_required)
			continue;
		if (apdo_idx == -1) {
			apdo_idx = cap_idx;
			pr_err("select potential cap_idx[%d]\n", cap_idx);
			pdpm->apdo_max_volt = apdo_cap.max_mv;
			pdpm->apdo_max_curr = apdo_cap.ma;
		}
	}
    if (apdo_idx != -1){
		ret = usbpd_pps_enable_charging(pdpm, true, 5000, 3000);
		if (ret != TCP_DPM_RET_SUCCESS)
			return false;
		return true;
	}
    return false;
}

static int usbpd_select_pdo(struct usbpd_pm *pdpm, u32 mV, u32 mA)
{
	int ret, cnt = 0;

	if (check_typec_attached_snk(pdpm->tcpc) < 0)
		return -EINVAL;
	pr_err("%dmV, %dmA\n", mV, mA);

	if (!tcpm_inquire_pd_connected(pdpm->tcpc)) {
		pr_err("pd not connected\n");
		return -EINVAL;
	}

	do {
		ret = tcpm_dpm_pd_request(pdpm->tcpc, mV, mA, NULL);
		cnt++;
	} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

	if (ret != TCP_DPM_RET_SUCCESS)
		pr_err("fail(%d)\n", ret);
	return ret > 0 ? -ret : ret;
}

static int pca_pps_tcp_notifier_call(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	//struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
	struct tcp_notify *noti = data;

	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			pr_err("detached\n");
			__pdpm->is_pps_en_unlock = false;
			__pdpm->hrst_cnt = 0;
			break;
		case PD_CONNECT_HARD_RESET:
			__pdpm->hrst_cnt++;
			pr_err("pd hardreset, cnt = %d\n",
				 __pdpm->hrst_cnt);
			__pdpm->is_pps_en_unlock = false;
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			if (__pdpm->hrst_cnt < 5) {
				pr_err("en unlock\n");
				__pdpm->is_pps_en_unlock = true;
			}
			break;
		default:
			break;
		}
	default:
		break;
	}
	
	power_supply_changed(__pdpm->usb_psy);
	
	return NOTIFY_OK;
}

/************************6360 API***************************/
/*
 * Set AICR & ICHG of switching charger
 *
 * @aicr: setting of AICR
 * @ichg: setting of ICHG
 */
static int usbpd_pm_set_swchg_cap(struct usbpd_pm *pdpm, u32 aicr)
{
	int ret;
	u32 ichg;

	ret = charger_dev_set_input_current(pdpm->sw_chg, aicr * 1000);
	if (ret < 0) {
		pr_err("set aicr fail(%d)\n", ret);
		return ret;
	}

	//set ichg
	/* 90% charging efficiency */
	ichg = (90 * pdpm->cp.vbus_volt * aicr / 100) / pdpm->cp.vbat_volt;

	ret = charger_dev_set_charging_current(pdpm->sw_chg, ichg * 1000);
	if (ret < 0) {
		pr_err("set_ichg fail(%d)\n", ret);
		return ret;
	}

	pr_info("AICR = %dmA, ICHG = %dmA\n", aicr, ichg);
	return 0;
}

/*
 * Enable charging of switching charger
 * For divide by two algorithm, according to swchg_ichg to decide enable or not
 *
 * @en: enable/disable
 */
static int usbpd_pm_enable_sw(struct usbpd_pm *pdpm, bool en)
{
	int ret;
	pr_info("en = %d\n", en);
	if (en) {
		ret = charger_dev_enable(pdpm->sw_chg, true);
		if (ret < 0) {
			pr_err("en swchg fail(%d)\n", ret);
			return ret;
		}
		ret = charger_dev_enable_hz(pdpm->sw_chg, false);
		if (ret < 0) {
			pr_err("disable hz fail(%d)\n", ret);
			return ret;
		}
	} else {
		ret = charger_dev_enable_hz(pdpm->sw_chg, true);
		if (ret < 0) {
			pr_err("disable hz fail(%d)\n", ret);
			return ret;
		}
		ret = charger_dev_enable(pdpm->sw_chg, false);
		if (ret < 0) {
			pr_err("en swchg fail(%d)\n", ret);
			return ret;
		}
	}

	pdpm->sw.charge_enabled = en;

	return 0;
}

/*
 * Get ibus current of switching charger
 *
*/
static int usbpd_pm_update_sw_status(struct usbpd_pm *pdpm)
{
	int ret, ibus;

	ret = charger_dev_get_adc(pdpm->sw_chg, ADC_CHANNEL_IBUS, &ibus,
				   &ibus);
	if (ret < 0) {
		pr_err("get swchg ibus fail(%d)\n", ret);
		return ret;
	}
	pdpm->sw.ibus_curr = ibus / 1000;

	return ret;
}

static void usbpd_check_tcpc(struct usbpd_pm *pdpm)
{
	if (!pdpm->tcpc) {
		pdpm->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (!pdpm->tcpc) {
			pr_err("get tcpc dev fail\n");
		}
	}
}

static void usbpd_check_pca_chg_swchg(struct usbpd_pm *pdpm)
{
	if (!pdpm->sw_chg) {
		pdpm->sw_chg = get_charger_by_name("primary_chg");
		if (!pdpm->sw_chg) {
			pr_err("get primary_chg fail\n");
		}
	}
}

static void usbpd_check_charger_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->usb_psy) {
		pdpm->usb_psy = power_supply_get_by_name("charger");
		if (!pdpm->usb_psy)
			pr_err("usb psy not found!\n");
	}
}

static void usbpd_check_battery_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->battery_psy) {
		pdpm->battery_psy = power_supply_get_by_name("battery");
		if (!pdpm->battery_psy)
			pr_err("battery psy not found!\n");
	}
}
//+ Extb HOMGMI-84843,chenrui1.wt,ADD,20210514,add adpo_max node
static void usbpd_check_apdo_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->apdo_psy) {
		pdpm->apdo_psy = power_supply_get_by_name("usb");
		if (!pdpm->apdo_psy)
			pr_err("apdo psy not found!\n");
	}
}
//-Extb HOMGMI-84843,chenrui1.wt,ADD,20210514,add adpo_max node

static void usbpd_check_cp_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->cp_psy) {
		if (pm_config.cp_sec_enable)
			pdpm->cp_psy = power_supply_get_by_name("sc8551-master");
		else
			pdpm->cp_psy = power_supply_get_by_name("sc8551-standalone");
		//+Bug651594, chenrui1.wt, ADD, 20210517, add ln8000 charger bringup
		if (pdpm->cp_psy)
			return;
		if (pm_config.cp_sec_enable)
			pdpm->cp_psy = power_supply_get_by_name("ln8000-master");
		else
			pdpm->cp_psy = power_supply_get_by_name("ln8000-standalone");
		//-Bug651594, chenrui1.wt, ADD, 20210517, add ln8000 charger bringup
		if (!pdpm->cp_psy)
			pr_err("cp_psy not found\n");
	}
}

static void usbpd_check_cp_sec_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->cp_sec_psy) {
        pdpm->cp_sec_psy = power_supply_get_by_name("sc8551-slave");
        //+Bug651594, chenrui1.wt, ADD, 20210517, add ln8000 charger bringup
        if (pdpm->cp_psy)
            return;
        pdpm->cp_sec_psy = power_supply_get_by_name("ln8000-slave");
        //-Bug651594, chenrui1.wt, ADD, 20210517, add ln8000 charger bringup
        if (!pdpm->cp_sec_psy)
            pr_err("cp_sec_psy not found\n");
    }
}

static void usbpd_pm_update_cp_status(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	usbpd_check_cp_psy(pdpm);

	if (!pdpm->cp_psy)
		return;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_BATTERY_VOLTAGE, &val);
	if (!ret)
		pdpm->cp.vbat_volt = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_BUS_CURRENT, &val);
	if (!ret)
		pdpm->cp.ibus_curr = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_BUS_VOLTAGE, &val);
	if (!ret)
		pdpm->cp.vbus_volt = val.intval; 

	ret = power_supply_get_property(pdpm->battery_psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (!ret) {
		pdpm->cp.battery_curr = val.intval/1000;
		pdpm->cp.ibat_curr = pdpm->cp.battery_curr;
	}

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_VBUS_ERROR_STATUS, &val);
	if (!ret)
	{
		pr_err(">>>>vbus error state : %02x\n", val.intval);
		pdpm->cp.vbus_error_low = (val.intval >> 5) & 0x01;
		pdpm->cp.vbus_error_high = (val.intval >> 4) & 0x01;
	}

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_BUS_TEMPERATURE, &val);
	if (!ret)
		pdpm->cp.bus_temp = val.intval; 

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_BATTERY_TEMPERATURE, &val);
	if (!ret)
		pdpm->cp.bat_temp = val.intval; 

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_DIE_TEMPERATURE, &val);
	if (!ret)
		pdpm->cp.die_temp = val.intval; 

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_BATTERY_PRESENT, &val);
	if (!ret)
		pdpm->cp.batt_pres = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_VBUS_PRESENT, &val);
	if (!ret)
		pdpm->cp.vbus_pres = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (!ret)
		pdpm->cp.charge_enabled = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_ALARM_STATUS, &val);
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
			POWER_SUPPLY_PROP_SC_FAULT_STATUS, &val);
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
			POWER_SUPPLY_PROP_SC_DIRECT_CHARGE, &val);
	if (!ret) {
//		pdpm->cp.direct_charge = val.intval;
		pr_err("[%s] get direct_charge = %d", __func__, pdpm->cp.direct_charge);
	}

}

static int usbpd_pm_set_cp_charge_mode(struct usbpd_pm *pdpm, u8 charge_mode)
{
	int ret;
	union power_supply_propval val = {0,};

	usbpd_check_cp_psy(pdpm);

	if (!pdpm->cp_psy)
		return -ENODEV;

	if(!charge_mode)
		val.intval =  SC8551_2_1_CHARGER_MODE;
	else
		val.intval =  SC8551_1_1_CHARGER_MODE;
	ret = power_supply_set_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_CHARGE_MODE, &val);
	return ret;
}

static int usbpd_pm_check_cp_charge_mode(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	usbpd_check_cp_psy(pdpm);

	if (!pdpm->cp_psy)
		return -ENODEV;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_CHARGE_MODE, &val);
	if(!ret){
		if(!val.intval)
			pdpm->cp.charge_mode = SC8551_2_1_CHARGER_MODE;
		else
			pdpm->cp.charge_mode = SC8551_1_1_CHARGER_MODE;
	}
	return ret;
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
			POWER_SUPPLY_PROP_SC_BUS_CURRENT, &val);
	if (!ret)
		pdpm->cp_sec.ibus_curr = val.intval; 

	ret = power_supply_get_property(pdpm->cp_sec_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (!ret)
		pdpm->cp_sec.charge_enabled = val.intval;
}


static int usbpd_pm_enable_cp(struct usbpd_pm *pdpm, bool enable)
{
	int ret;
	union power_supply_propval val = {0,};

	usbpd_check_cp_psy(pdpm);

	if (!pdpm->cp_psy)
		return -ENODEV;

	val.intval = enable;
	ret = power_supply_set_property(pdpm->cp_psy, 
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	
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
	union power_supply_propval val = {0,};

	usbpd_check_cp_psy(pdpm);
	
	if (!pdpm->cp_psy)
		return -ENODEV;

	ret = power_supply_get_property(pdpm->cp_psy, 
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (!ret)
		pdpm->cp.charge_enabled = !!val.intval;

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

	return ret;
}

/******************************************************************************************/
static void usbpd_pm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
	bool retValue;
	// Extb HOMGMI-84843,chenrui1.wt,ADD,20210512,add adpo_max node
	union power_supply_propval pval = {0, };
	//Extb HOMGMI-84841,wangbin.wt,ADD,20210709,add for report soc decimal instantly
	struct power_supply *battery_psy;

	retValue = usbpd_get_pps_status(pdpm);
	if (retValue)
		pdpm->pps_supported = true;
	else
		pdpm->pps_supported = false;

	if (pdpm->pps_supported) {
		/* +Bug651592 caijiaqi.wt,20210609,ADD BATTERY CURRENT jeita */
		pdpm->lcdon_curr_step = 0;
		pdpm->lcdoff_curr_step = 0;
		/* -Bug651592 caijiaqi.wt,20210609,ADD BATTERY CURRENT jeita */
		pr_notice("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d\n",
				pdpm->apdo_selected_pdo,
				pdpm->apdo_max_volt,
				pdpm->apdo_max_curr);
		// +Extb HOMGMI-84843,chenrui1.wt,ADD,20210514,add adpo_max node
		pval.intval = (pdpm->apdo_max_volt / 1000) * (pdpm->apdo_max_curr / 1000);
		power_supply_set_property(pdpm->apdo_psy,
				POWER_SUPPLY_PROP_APDO_MAX, &pval);
		//Extb HONGMI-84841,chenrui1.wt,20210702,ADD,ADD changed apdo_max for soc decimal
		power_supply_changed(pdpm->apdo_psy);
		// -Extb HOMGMI-84843,chenrui1.wt,ADD,20210514,add adpo_max node
		/* +Extb HOMGMI-84841,wangbin.wt,ADD,20210709,add for report soc decimal instantly*/
		battery_psy = power_supply_get_by_name("battery");
		if (NULL != battery_psy)
			power_supply_changed(battery_psy);
		else
			pr_err("battery_psy is null\n");
		/* -Extb HOMGMI-84841,wangbin.wt,ADD,20210709,add for report soc decimal instantly*/
	}
	else
		pr_notice("Not qualified PPS adapter\n");

}


#define TAPER_TIMEOUT	(5000 / PM_WORK_RUN_INTERVAL)
#define IBUS_CHANGE_TIMEOUT  (500 / PM_WORK_RUN_INTERVAL)

/* +Bug651592 caijiaqi.wt,20210609,ADD BATTERY CURRENT jeita */
static int bat_step(struct usbpd_pm *pdpm, int cur) {
	int step = 0;

	if (cur <= BAT_CURR_3000MA)
		pdpm->cp.direct_charge = 0;
	else if (cur >= BAT_CURR_3500MA)
		pdpm->cp.direct_charge = 0;

	pr_err("[%s] curr = %d, direct_charge = %d, pdpm->pd_authen %d\n",
		__func__, cur, pdpm->cp.direct_charge, pdpm->pd_authen);

#if 0
	if (pdpm->cp.ibat_curr < cur)
		step = pm_config.fc2_steps;
	else if (pdpm->cp.ibat_curr > cur + BAT_CURR_100MA)
		step = -pm_config.fc2_steps;
#else
	if (pdpm->cp.battery_curr < cur)
		step = pm_config.fc2_steps;
	else if (pdpm->cp.battery_curr > cur + BAT_CURR_100MA)
		step = -pm_config.fc2_steps;
#endif
	return step;
}

static int bat_lcdon_temp(struct usbpd_pm *pdpm, int temp)
{
	int bat_temp = temp;
	int step_ibat = 0;

	if (bat_temp < BAT_TEMP_300) {
		if (!pdpm->lcdon_curr_step)
			//Bug674901,wangbin wt.MODIFY	.20210612,modify Max charge current is 4A when lcd is on.
			step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
		else
			step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
	} else if (bat_temp >= BAT_TEMP_300 && bat_temp < BAT_TEMP_340) {
		if (pdpm->lcdon_curr_step <= CHG_TEMP_STEP1) {
			pdpm->lcdon_curr_step = CHG_TEMP_STEP1;
			step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
		} else if (pdpm->lcdon_curr_step == CHG_TEMP_STEP2) {
			if (bat_temp <= BAT_TEMP_340 - CHG_TEMP_OFFSET) {
				pdpm->lcdon_curr_step = CHG_TEMP_STEP1;
				step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
			}
		} else {
			pdpm->lcdon_curr_step = CHG_TEMP_STEP2;
			step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
		}
	} else if (bat_temp >= BAT_TEMP_340 && bat_temp < BAT_TEMP_360) {
		if (pdpm->lcdon_curr_step <= CHG_TEMP_STEP2) {
			pdpm->lcdon_curr_step = CHG_TEMP_STEP2;
			step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
		} else if (pdpm->lcdon_curr_step == CHG_TEMP_STEP3) {
			if (bat_temp <= BAT_TEMP_360 - CHG_TEMP_OFFSET) {
				pdpm->lcdon_curr_step = CHG_TEMP_STEP2;
				step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_2500MA);
			}
		} else {
			pdpm->lcdon_curr_step = CHG_TEMP_STEP3;
			step_ibat = bat_step(pdpm, BAT_CURR_2500MA);
		}
	} else if (bat_temp >= BAT_TEMP_360 && bat_temp < BAT_TEMP_380) {
		if (pdpm->lcdon_curr_step <= CHG_TEMP_STEP3) {
			pdpm->lcdon_curr_step = CHG_TEMP_STEP3;
			step_ibat = bat_step(pdpm, BAT_CURR_2500MA);
		} else if (pdpm->lcdon_curr_step == CHG_TEMP_STEP4) {
			if (bat_temp <= BAT_TEMP_380 - CHG_TEMP_OFFSET) {
				pdpm->lcdon_curr_step = CHG_TEMP_STEP3;
				step_ibat = bat_step(pdpm, BAT_CURR_2500MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_2000MA);
			}
		} else {
			pdpm->lcdon_curr_step = CHG_TEMP_STEP4;
			step_ibat = bat_step(pdpm, BAT_CURR_2000MA);
		}
	} else if (bat_temp >= BAT_TEMP_380 && bat_temp < BAT_TEMP_440) {
		if (pdpm->lcdon_curr_step <= CHG_TEMP_STEP4) {
			pdpm->lcdon_curr_step = CHG_TEMP_STEP4;
			step_ibat = bat_step(pdpm, BAT_CURR_2000MA);
		} else if (pdpm->lcdon_curr_step == CHG_TEMP_STEP5) {
			if (bat_temp <= BAT_TEMP_410 - CHG_TEMP_OFFSET) {
				pdpm->lcdon_curr_step = CHG_TEMP_STEP4;
				step_ibat = bat_step(pdpm, BAT_CURR_2000MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_1500MA);
			}
		} else {
			pdpm->lcdon_curr_step = CHG_TEMP_STEP5;
			step_ibat = bat_step(pdpm, BAT_CURR_1500MA);
		}
	} else if (bat_temp >= BAT_TEMP_440) {
		pdpm->lcdon_curr_step = CHG_TEMP_STEP5;
		step_ibat = bat_step(pdpm, BAT_CURR_1500MA);
	}
	pr_err("[%s]lcdon_curr_step=%d, direct_charge = %d",
		__func__, pdpm->lcdon_curr_step, pdpm->cp.direct_charge);
	return step_ibat;
}

static int bat_lcdoff_temp(struct usbpd_pm *pdpm, int temp)
{
	int bat_temp = temp;
	int step_ibat = 1;

	if (bat_temp < BAT_TEMP_390) {
		if (!pdpm->lcdoff_curr_step)
			step_ibat = bat_step(pdpm, BAT_CURR_6000MA);
		else
			step_ibat = bat_step(pdpm, BAT_CURR_5000MA);
	} else if (bat_temp >= BAT_TEMP_390 && bat_temp < BAT_TEMP_400) {
		if (pdpm->lcdoff_curr_step <= CHG_TEMP_STEP1) {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP1;
			step_ibat = bat_step(pdpm, BAT_CURR_5000MA);
		} else if (pdpm->lcdoff_curr_step == CHG_TEMP_STEP2) {
			if (bat_temp <= BAT_TEMP_400 - CHG_TEMP_OFFSET) {
				pdpm->lcdoff_curr_step = CHG_TEMP_STEP1;
				step_ibat = bat_step(pdpm, BAT_CURR_5000MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_4500MA);
			}
		} else {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP2;
			step_ibat = bat_step(pdpm, BAT_CURR_4500MA);
		}
	} else if (bat_temp >= BAT_TEMP_400 && bat_temp < BAT_TEMP_410) {
		if (pdpm->lcdoff_curr_step <= CHG_TEMP_STEP2) {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP2;
			step_ibat = bat_step(pdpm, BAT_CURR_4500MA);
		} else if (pdpm->lcdoff_curr_step == CHG_TEMP_STEP3) {
			if (bat_temp <= BAT_TEMP_420 - CHG_TEMP_OFFSET) {
				pdpm->lcdoff_curr_step = CHG_TEMP_STEP2;
				step_ibat = bat_step(pdpm, BAT_CURR_4500MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_4000MA);
			}
		} else {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP3;
			step_ibat = bat_step(pdpm, BAT_CURR_4000MA);
		}
	} else if (bat_temp >= BAT_TEMP_410 && bat_temp < BAT_TEMP_420) {
		if (pdpm->lcdoff_curr_step <= CHG_TEMP_STEP3) {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP3;
			step_ibat = bat_step(pdpm, BAT_CURR_4000MA);
		} else if (pdpm->lcdoff_curr_step == CHG_TEMP_STEP4) {
			if (bat_temp <= BAT_TEMP_440 - CHG_TEMP_OFFSET) {
				pdpm->lcdoff_curr_step = CHG_TEMP_STEP3;
				step_ibat = bat_step(pdpm, BAT_CURR_4000MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_3500MA);
			}
		} else {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP4;
			step_ibat = bat_step(pdpm, BAT_CURR_3500MA);
		}
	} else if (bat_temp >= BAT_TEMP_420 && bat_temp < BAT_TEMP_430) {
		if (pdpm->lcdoff_curr_step <= CHG_TEMP_STEP4) {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP4;
			step_ibat = bat_step(pdpm, BAT_CURR_3500MA);
		} else if (pdpm->lcdoff_curr_step == CHG_TEMP_STEP5) {
			if (bat_temp <= BAT_TEMP_410 - CHG_TEMP_OFFSET) {
				pdpm->lcdoff_curr_step = CHG_TEMP_STEP4;
				step_ibat = bat_step(pdpm, BAT_CURR_3500MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
			}
		} else {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP5;
			step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
		}
	} else if (bat_temp >= BAT_TEMP_430 && bat_temp < BAT_TEMP_440) {
		if (pdpm->lcdoff_curr_step <= CHG_TEMP_STEP5) {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP5;
			step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
		} else if (pdpm->lcdoff_curr_step == CHG_TEMP_STEP6) {
			if (bat_temp <= BAT_TEMP_410 - CHG_TEMP_OFFSET) {
				pdpm->lcdoff_curr_step = CHG_TEMP_STEP5;
				step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_2500MA);
			}
		} else {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP6;
			step_ibat = bat_step(pdpm, BAT_CURR_2500MA);
		}
	} else if (bat_temp >= BAT_TEMP_440) {
		pdpm->lcdoff_curr_step = CHG_TEMP_STEP6;
		step_ibat = bat_step(pdpm, BAT_CURR_2500MA);
	}
	pr_err("[%s]lcdoff_curr_step=%d, direct_charge = %d",
		__func__, pdpm->lcdoff_curr_step, pdpm->cp.direct_charge);
	return step_ibat;
}

static int get_battery_temp(void)
{
	struct power_supply *battery_psy;
	union power_supply_propval pval = {0, };

	battery_psy = power_supply_get_by_name("battery");
	if(battery_psy)
		power_supply_get_property(battery_psy,
			POWER_SUPPLY_PROP_TEMP, &pval);
	return pval.intval;
}

extern int charger_manager_get_thermal_mitigation_current(void);

static int battery_sw_jeita(struct usbpd_pm *pdpm)
{
	int step_temp = 0;
	int step_ibat = 0;
	int step_vbat = 0;
	int bat_temp = 0;
	int therm_curr = 0;
	int step_therm = 0;

	bat_temp = get_battery_temp();
	therm_curr = charger_manager_get_thermal_mitigation_current() / 1000;
	if (bat_temp >= CHG_BAT_TEMP_MIN && bat_temp <= CHG_BAT_TEMP_MAX) {
		pdpm->pps_temp_flag = true;
		if (pdpm->cp.vbat_volt < CHG_CUR_VOLT) {
			if (pdpm->therm_flag)
				step_vbat = bat_step(pdpm, BAT_CURR_6000MA);
			else
				step_vbat = bat_step(pdpm, BAT_CURR_6000MA);
		} else if (pdpm->cp.vbat_volt >= CHG_CUR_VOLT && pdpm->cp.vbat_volt < CHG_CUR_VOLT2)
			step_vbat = bat_step(pdpm, BAT_CURR_5400MA);
		else
			step_vbat = bat_step(pdpm, BAT_CURR_3900MA);

		if (bat_temp <= CHG_BAT_TEMP_15)
			step_vbat = bat_step(pdpm, BAT_CURR_3800MA);

		if (!pdpm->pd_authen) {
			if (pdpm->cp.vbat_volt <= CHG_CUR_VOLT2)
				step_temp = bat_step(pdpm, BAT_CURR_5400MA);
			else
				step_temp = -pm_config.fc2_steps;
			step_vbat = min(step_vbat, step_temp);
		}
		if(pdpm->therm_flag) {
			if (get_jeita_lcd_on_off()) {
				step_therm = bat_step(pdpm, therm_curr);
				step_ibat = bat_lcdon_temp(pdpm, bat_temp);
			} else {
				step_ibat = bat_lcdoff_temp(pdpm, bat_temp);
				step_therm = bat_step(pdpm, therm_curr);
			}
			step_ibat = min(step_therm, step_ibat);
		} else {
			step_therm = bat_step(pdpm, therm_curr);
			if(get_jeita_lcd_on_off()) {
				step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
				step_ibat = min(step_therm, step_ibat);
			} else {
				step_ibat = step_therm;
			}
		}
	} else {
		pdpm->pps_temp_flag = false;
	}
	pr_err(">>>>temp %d pdpm->cp.ibus_curr %d step_ibat %d, step_vbat %d, lcd_on %d,  pd_authen %d, therm_curr %d, therm_flag %d\n",
		bat_temp, pdpm->cp.ibus_curr, step_ibat, step_vbat, get_jeita_lcd_on_off(), pdpm->pd_authen, therm_curr, pdpm->therm_flag);
	return min(step_vbat, step_ibat);
}
/* -Bug651592 caijiaqi.wt,20210609,ADD BATTERY CURRENT jeita */

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

	static int ibus_limit;

	if (ibus_limit == 0)
		ibus_limit = pm_config.bus_curr_lp_lmt;// + 400;

	/* reduce bus current in cv loop */
	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 50) {
		if (ibus_lmt_change_timer++ > IBUS_CHANGE_TIMEOUT) {
			ibus_lmt_change_timer = 0;
			ibus_limit = pm_config.bus_curr_lp_lmt;// - 400;
		}
	} else if (pdpm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - 250) {
		ibus_limit = pm_config.bus_curr_lp_lmt;// + 400;
		ibus_lmt_change_timer = 0;
	} else {
		ibus_lmt_change_timer = 0;
	}

	/* battery voltage loop*/
	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt)
		step_vbat = -pm_config.fc2_steps;
	else if (pdpm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - 7)
		step_vbat = pm_config.fc2_steps;;

//	pr_err("[%s] direct_charge = %d", __func__, pdpm->cp.direct_charge);
	/* battery charge current loop*/
#if 0
	if(!pdpm->cp.direct_charge){
		if (pdpm->cp.ibat_curr < pm_config.bat_curr_lp_lmt )
			step_ibat = pm_config.fc2_steps;
		else if (pdpm->cp.ibat_curr > pm_config.bat_curr_lp_lmt + 100)
			step_ibat = -pm_config.fc2_steps;
	}
	else{
		if (pdpm->cp.ibat_curr < (pm_config.bat_curr_lp_lmt >> 1))
			step_ibat = pm_config.fc2_steps;
		else if (pdpm->cp.ibat_curr > (pm_config.bat_curr_lp_lmt + 100) >> 1)
			step_ibat = -pm_config.fc2_steps;
	}
#else
	if(!pdpm->cp.direct_charge){
		if (pdpm->cp.battery_curr < pm_config.bat_curr_lp_lmt )
			step_ibat = pm_config.fc2_steps;
		else if (pdpm->cp.battery_curr > pm_config.bat_curr_lp_lmt + 100)
			step_ibat = -pm_config.fc2_steps;
	}
	else{
		if (pdpm->cp.battery_curr < (pm_config.bat_curr_lp_lmt >> 1))
			step_ibat = pm_config.fc2_steps;
		else if (pdpm->cp.battery_curr > (pm_config.bat_curr_lp_lmt + 100) >> 1)
			step_ibat = -pm_config.fc2_steps;
	}
#endif
	/* Bug651592 caijiaqi.wt,20210609,ADD BATTERY CURRENT jeita */
	step_ibat = battery_sw_jeita(pdpm);

	/* bus current loop*/
	ibus_total = pdpm->cp.ibus_curr ;//+ pdpm->sw.ibus_curr;
	if (pm_config.cp_sec_enable)
		ibus_total += pdpm->cp_sec.ibus_curr;

	if (ibus_total < ibus_limit - 50)
		step_ibus = pm_config.fc2_steps;
	else if (ibus_total > ibus_limit)
		step_ibus = -pm_config.fc2_steps;

	/* hardware regulation loop*/
	/*if (pdpm->cp.vbat_reg || pdpm->cp.ibat_reg)
		step_bat_reg = 5 * (-pm_config.fc2_steps);
	else
		step_bat_reg = pm_config.fc2_steps;*/
	step_bat_reg = pm_config.fc2_steps;

	sw_ctrl_steps = min(min(step_vbat, step_ibus), step_ibat);
	sw_ctrl_steps = min(sw_ctrl_steps, step_bat_reg);

	/* hardware alarm loop */
	if (pdpm->cp.bat_ocp_alarm
		|| pdpm->cp.bus_ocp_alarm || pdpm->cp.bus_ovp_alarm) 
		hw_ctrl_steps = -pm_config.fc2_steps;
	else
		hw_ctrl_steps = pm_config.fc2_steps;

	usbpd_pm_check_cp_charge_mode(pdpm);
	/* check if cp disabled due to other reason*/
	usbpd_pm_check_cp_enabled(pdpm);
	pr_err(">>>>cp enable bit %d\n", pdpm->cp.charge_enabled);

	if (pm_config.cp_sec_enable) {
        usbpd_pm_check_cp_sec_enabled(pdpm);
        pr_err(">>>>cp sec enable bit %d\n", pdpm->cp_sec.charge_enabled);
    }

	if (pdpm->cp.bat_therm_fault ) { /* battery overheat, stop charge*/
		pr_notice("bat_therm_fault:%d\n", pdpm->cp.bat_therm_fault);
		return PM_ALGO_RET_THERM_FAULT;
	} else if (pdpm->cp.bat_ocp_fault || pdpm->cp.bus_ocp_fault 
			|| pdpm->cp.bat_ovp_fault || pdpm->cp.bus_ovp_fault) {
		pr_notice("bat_ocp_fault:%d, bus_ocp_fault:%d, bat_ovp_fault:%d, \
				bus_ovp_fault:%d\n", pdpm->cp.bat_ocp_fault,
				pdpm->cp.bus_ocp_fault, pdpm->cp.bat_ovp_fault,
				pdpm->cp.bus_ovp_fault);
	        return PM_ALGO_RET_OTHER_FAULT; /* go to switch, and try to ramp up*/
	} else if ( !pdpm->cp.charge_enabled ||(pdpm->cp.charge_enabled && (pdpm->cp.vbus_error_low
                || pdpm->cp.vbus_error_high)) || (pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled && !pdpm->cp_sec_stopped)){
		pr_notice("cp.charge_enabled:%d  %d  %d,cp_sec.charge_enabled:%d\n",
				pdpm->cp.charge_enabled, pdpm->cp.vbus_error_low, pdpm->cp.vbus_error_high,pdpm->cp_sec.charge_enabled);
		return PM_ALGO_RET_CHG_DISABLED;
	}
	if(pdpm->cp.direct_charge != pdpm->cp.charge_mode)
	{
		pr_err(">>>>SC8551 direct charge %d   charge mode %d\n",
			pdpm->cp.direct_charge, pdpm->cp.charge_mode);
		usbpd_pm_enable_cp(pdpm, false);
		usbpd_pm_check_cp_enabled(pdpm);
		if(pdpm->cp.direct_charge)
			usbpd_pm_set_cp_charge_mode(pdpm, SC8551_1_1_CHARGER_MODE);
		else
			usbpd_pm_set_cp_charge_mode(pdpm, SC8551_2_1_CHARGER_MODE);
		pr_err(">>>>SC8551 direct charge %d   charge mode %d end\n",
			pdpm->cp.direct_charge, pdpm->cp.charge_mode);
		return PM_ALGO_RET_CHG_DISABLED;
	}

	/* charge pump taper charge */
	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 50 
			&& pdpm->cp.ibat_curr < pm_config.fc2_taper_current) {
		if (fc2_taper_timer++ > TAPER_TIMEOUT) {
			pr_notice("charge pump taper charging done\n");
			fc2_taper_timer = 0;
			return PM_ALGO_RET_TAPER_DONE;
		}
	} else {
		fc2_taper_timer = 0;
	}
	   
	/*TODO: customer can add hook here to check system level 
	 * thermal mitigation*/


	steps = min(sw_ctrl_steps, hw_ctrl_steps);

	pr_err(">>>>>>%d %d %d sw %d hw %d all %d\n", 
			step_vbat, step_ibat, step_ibus, sw_ctrl_steps, hw_ctrl_steps, steps);

	pdpm->request_voltage += steps * 20;

	if (pdpm->request_voltage > pdpm->apdo_max_volt - 300)
		pdpm->request_voltage = pdpm->apdo_max_volt - 300;

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

//+Bug669735,chenrui.wt,MODIFY,20210625,FAMMI charger test failed
#ifdef WT_COMPILE_FACTORY_VERSION
static int mtk_get_battery_capacity(void)
{
	int ret = 0;
	union power_supply_propval pval = {0, };
	struct power_supply *battery_psy;

	battery_psy = power_supply_get_by_name("battery");
	if (!battery_psy)
		return 0;
	ret = power_supply_get_property(battery_psy,
		POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (ret)
		return -1;
	pr_err("wt_debug %s: bat_cap is %d\n", __func__, pval.intval);
	return pval.intval;
}
//-Bug669735,chenrui.wt,MODIFY,20210625,FAMMI charger test failed
#endif

static void usbpd_pm_move_state(struct usbpd_pm *pdpm, enum pm_state state)
{
#if 1
	pr_err("state change:%s -> %s\n", 
		pm_str[pdpm->state], pm_str[state]);
#endif
	pdpm->state = state;
}

static int usbpd_pm_sm(struct usbpd_pm *pdpm)
{
	int ret;
	int rc = 0;
	static int tune_vbus_retry;
	static bool stop_sw;
	static bool recover;
	/* +Bug651592 caijiaqi.wt,20210709,ADD BATTERY CURRENT jeita */
	int bat_temp;

	bat_temp = get_battery_temp();
	/* -Bug651592 caijiaqi.wt,20210709,ADD BATTERY CURRENT jeita */
	pr_err(">>>>>>>>>>>state phase :%d\n", pdpm->state);
	pr_err(">>>>>vbus_vol %d    vbat_vol %d   vout %d\n", pdpm->cp.vbus_volt, pdpm->cp.vbat_volt, pdpm->cp.vout_volt);
	pr_err(">>>>>ibus_curr %d    ibat_curr %d mt6360_ibat_curr %d\n", pdpm->cp.ibus_curr + pdpm->cp_sec.ibus_curr, pdpm->cp.ibat_curr,pdpm->cp.battery_curr);
	switch (pdpm->state) {
	case PD_PM_STATE_ENTRY:
		stop_sw = false;
		recover = false;

		if (pdpm->cp.vbat_volt < pm_config.min_vbat_for_cp) {
			pr_notice("batt_volt-%d, waiting...\n", pdpm->cp.vbat_volt);
		} else if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 100) {
			pr_notice("batt_volt-%d is too high for cp,\
					charging with switch charger\n", 
					pdpm->cp.vbat_volt);
		//	usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		/* +Bug651592 caijiaqi.wt,20210709,ADD BATTERY CURRENT jeita */
		} else if ((bat_temp >= BAT_TEMP_460)
			|| bat_temp <= CHG_BAT_TEMP_MIN
			|| get_charging_call_state()) {
			pr_notice("temp is %d, high or low, waiting...\n", bat_temp);
		/* -Bug651592 caijiaqi.wt,20210709,ADD BATTERY CURRENT jeita */
		} else {
			pr_notice("batt_volt-%d is ok, start flash charging\n", 
					pdpm->cp.vbat_volt);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY);
		}
		break;

	case PD_PM_STATE_FC2_ENTRY:
		if (pm_config.fc2_disable_sw) {
			usbpd_pm_set_swchg_cap(pdpm, 3000);
			usbpd_pm_enable_sw(pdpm, false);
			pdpm->is_cp_ok = true;
			if (!pdpm->sw.charge_enabled)
				usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
		} else {
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
		}
		break;

	case PD_PM_STATE_FC2_ENTRY_1:
		usbpd_pm_check_cp_charge_mode(pdpm);
		if (pdpm->cp.charge_mode) {
			usbpd_pm_set_cp_charge_mode(pdpm, SC8551_1_1_CHARGER_MODE);
			pdpm->request_voltage = pdpm->cp.vbat_volt + BUS_VOLT_INIT_UP/2;
		} else {
			usbpd_pm_set_cp_charge_mode(pdpm, SC8551_2_1_CHARGER_MODE);
			pdpm->request_voltage = pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP;
		}

		pdpm->request_current = min(pdpm->apdo_max_curr, pm_config.bus_curr_lp_lmt);

		usbpd_select_pdo(pdpm,pdpm->request_voltage,
						 pdpm->request_current);
		pr_err("request_voltage:%d, request_current:%d\n",
				pdpm->request_voltage, pdpm->request_current);
	
		pr_err(">>>>SC8551 direct charge %d   charge mode %d\n",
					pdpm->cp.direct_charge, pdpm->cp.charge_mode);
		if(pdpm->cp.direct_charge == pdpm->cp.charge_mode)
		{
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);
		}


		tune_vbus_retry = 0;
		break;

	case PD_PM_STATE_FC2_ENTRY_2:
		pr_err("tune_vbus_retry %d\n", tune_vbus_retry);
		if (pdpm->cp.vbus_error_low) {// || pdpm->cp.vbus_volt < pdpm->cp.vbat_volt * 213/100) {
			tune_vbus_retry++;
			pdpm->request_voltage += 20;
			usbpd_select_pdo(pdpm,pdpm->request_voltage,
						 pdpm->request_current);
			pr_err("request_voltage:%d, request_current:%d\n",
					pdpm->request_voltage, pdpm->request_current);
		} else if (pdpm->cp.vbus_error_high) {// || pdpm->cp.vbus_volt > pdpm->cp.vbat_volt * 217/100) {
			tune_vbus_retry++;
			pdpm->request_voltage -= 20;
			usbpd_select_pdo(pdpm,pdpm->request_voltage,
						 pdpm->request_current);
			pr_err("request_voltage:%d, request_current:%d\n",
					pdpm->request_voltage, pdpm->request_current);
		} else {
			pr_notice("adapter volt tune ok, retry %d times\n", tune_vbus_retry);
		    usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_3);
			break;
		}
		
		if (tune_vbus_retry > 25) {
			pr_notice("Failed to tune adapter volt into valid range, \
					charge with switching charger\n");
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}	
		break;
	case PD_PM_STATE_FC2_ENTRY_3:
		usbpd_pm_check_cp_enabled(pdpm);
		if (!pdpm->cp.charge_enabled) {
			usbpd_pm_enable_cp(pdpm, true);
			usbpd_pm_check_cp_enabled(pdpm);
		}
		
		if (pm_config.cp_sec_enable) {
            usbpd_pm_check_cp_sec_enabled(pdpm);
            if (!pdpm->cp_sec.charge_enabled) {
                usbpd_pm_enable_cp_sec(pdpm, true);
                usbpd_pm_check_cp_sec_enabled(pdpm);
            }
        }
		if (pdpm->cp.charge_enabled) {
			if ((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled)
                    || !pm_config.cp_sec_enable) {
                usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_TUNE);
                ibus_lmt_change_timer = 0;
                fc2_taper_timer = 0;
            }
		}
		
		break;

	case PD_PM_STATE_FC2_TUNE:
		ret = usbpd_pm_fc2_charge_algo(pdpm);
		if (ret == PM_ALGO_RET_THERM_FAULT) {
			pr_notice("Move to stop charging:%d\n", ret);
			stop_sw = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else if (ret == PM_ALGO_RET_OTHER_FAULT || ret == PM_ALGO_RET_TAPER_DONE) {
			pr_notice("Move to switch charging:%d\n", ret);
			stop_sw = false;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else if (ret == PM_ALGO_RET_CHG_DISABLED) {
			pr_notice("Move to switch charging, will try to recover \
					flash charging:%d\n", ret);
			recover = true;
			stop_sw = false;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else {
			usbpd_select_pdo(pdpm,pdpm->request_voltage,
						 pdpm->request_current);
			pr_err("request_voltage:%d, request_current:%d\n",
					pdpm->request_voltage, pdpm->request_current);
		}

		/* +Bug651592 caijiaqi.wt,20210709,ADD BATTERY CURRENT jeita */
		if (!pdpm->pps_temp_flag
			|| get_charging_call_state()) {
			pr_notice("temp high or lower,stop charging\n");
			stop_sw = false;
			pdpm->sw.charge_enabled = false;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		}
		/* -Bug651592 caijiaqi.wt,20210709,ADD BATTERY CURRENT jeita */

//+Bug669735,chenrui.wt,MODIFY,20210625,FAMMI charger test failed
#ifdef WT_COMPILE_FACTORY_VERSION
		if (mtk_get_battery_capacity() >= 79) {
			stop_sw = false;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			pr_err("wt_debug : %s slave charger exit", __func__);
		}
#endif
//-Bug669735,chenrui.wt,MODIFY,20210625,FAMMI charger test failed
		break;

	case PD_PM_STATE_FC2_EXIT:
		pdpm->is_cp_ok = false;
		/* select default 5V*/
		usbpd_select_pdo(pdpm,5000,3000);
		

		if (pdpm->cp.charge_enabled) {
			usbpd_pm_enable_cp(pdpm, false);
			usbpd_pm_check_cp_enabled(pdpm);
		}
		if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled) {
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}

		pr_err(">>>sw state %d   %d\n", stop_sw, pdpm->sw.charge_enabled);
		if (stop_sw && pdpm->sw.charge_enabled) {
			usbpd_pm_enable_sw(pdpm, false);
			usbpd_pm_set_swchg_cap(pdpm, 3000);
		}
		else if (!stop_sw && !pdpm->sw.charge_enabled) {
			usbpd_pm_enable_sw(pdpm, true);
			usbpd_pm_set_swchg_cap(pdpm, 3000);
		}

		if (recover)
			usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
		else
		{
			usbpd_pps_enable_charging(pdpm,false,5000,3000);
			rc = 1;
		}
		/* +Bug651592 caijiaqi.wt,20210709,ADD BATTERY CURRENT jeita */
		if (!pdpm->pps_temp_flag
			|| get_charging_call_state()) {
			pr_notice("temp is high or low waiting...\n");
			usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
			pdpm->pd_active = false;
			schedule_work(&pdpm->usb_psy_change_work);
		}
		/* -Bug651592 caijiaqi.wt,20210709,ADD BATTERY CURRENT jeita */
	        break;
	}

	return rc;
}

static void usbpd_pm_workfunc(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					pm_work.work);

	usbpd_pm_update_sw_status(pdpm);
	usbpd_pm_update_cp_status(pdpm);
	usbpd_pm_update_cp_sec_status(pdpm);

	if (!usbpd_pm_sm(pdpm) && pdpm->pd_active)
		schedule_delayed_work(&pdpm->pm_work,
				msecs_to_jiffies(PM_WORK_RUN_INTERVAL));
}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
	//Extb HOMGMI-84843,chenrui1.wt,ADD,20210514,add adpo_max node
	union power_supply_propval pval = {0, };
	int ret = 0;
	usbpd_pm_enable_cp(pdpm, false);
    usbpd_pm_check_cp_enabled(pdpm);
    if (pm_config.cp_sec_enable) {
        usbpd_pm_enable_cp_sec(pdpm, false);
        usbpd_pm_check_cp_sec_enabled(pdpm);
    }
    cancel_delayed_work(&pdpm->pm_work);

    if (!pdpm->sw.charge_enabled) {
        usbpd_pm_enable_sw(pdpm, true);
    }

    pdpm->pps_supported = false;
    /* +Bug651592 caijiaqi.wt,20210609,ADD BATTERY CURRENT jeita */
    pdpm->lcdon_curr_step = 0;
    pdpm->lcdoff_curr_step = 0;
    pdpm->is_cp_ok = false;
    /* -Bug651592 caijiaqi.wt,20210609,ADD BATTERY CURRENT jeita */
    pdpm->apdo_selected_pdo = 0;
	//+Extb HOMGMI-84843,chenrui1.wt,ADD,20210514add adpo_max node
	pval.intval = 0;
	ret = power_supply_set_property(pdpm->apdo_psy,
			POWER_SUPPLY_PROP_APDO_MAX, &pval);
	if (ret)
		pr_err("%s:ret=%d\n",__func__,ret);
	//-Extb HOMGMI-84843,chenrui1.wt,ADD,20210514add adpo_max node
    usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
}

static int mi_pd_auth(struct usbpd_pm *pdpm)
{
	union power_supply_propval val = {0,};
	int ret = 0;

	ret = power_supply_get_property(pdpm->apdo_psy,
		POWER_SUPPLY_PROP_PD_AUTHENTICATION, &val);
	if (ret)
		pr_err("Failed to read typec power role\n");
	else
		pdpm->pd_authen = val.intval;
	return pdpm->pd_authen;
}

static void usbpd_pd_contact(struct usbpd_pm *pdpm, bool connected)
{
	pdpm->pd_active = connected;
	pr_err("[SC manager] >> pd_active %d\n",
			pdpm->pd_active);

	if (connected) {
		usbpd_pm_evaluate_src_caps(pdpm);
		pr_err("[SC manager] >>start cp charging pps support %d\n", pdpm->pps_supported);
		mi_pd_auth(pdpm);
		if (pdpm->pps_supported)
			schedule_delayed_work(&pdpm->pm_work, 0);
		else
			pdpm->pd_active = false;
	} else {
		usbpd_pm_disconnect(pdpm);
	}
}

static void cp_psy_change_work(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					cp_psy_change_work);

	pdpm->psy_change_running = false;
}

static void usb_psy_change_work(struct work_struct *work)
{
	union power_supply_propval propval;
	/* +Bug651592 caijiaqi.wt,20210607,ADD Secret battery */
	struct power_supply *batt_verify;
	bool safe_batt_falg = false;
	/* -Bug651592 caijiaqi.wt,20210607,ADD Secret battery */
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					usb_psy_change_work);

	/* +Bug651592 caijiaqi.wt,20210607,ADD Secret battery */
	batt_verify = power_supply_get_by_name("batt_verify");
	if (batt_verify) {
		power_supply_get_property(batt_verify,
			POWER_SUPPLY_PROP_MI_BATTERY_ID, &propval);
		if (propval.intval == 0x57 || propval.intval == 0x47)
			safe_batt_falg = true;
		pr_err("[SC manager] >> battery id %d safe flag  = %d\n",
			propval.intval, safe_batt_falg);
	}
	/* -Bug651592 caijiaqi.wt,20210607,ADD Secret battery */

	pr_err("[SC manager] >> usb change work, pdpm->pd_active=%d,"
			"__pdpm->is_pps_en_unlock=%d\n", pdpm->pd_active,
			__pdpm->is_pps_en_unlock);

	/* +Bug651592 caijiaqi.wt,20210607,MODIFY Secret battery */
	if (!pdpm->pd_active && __pdpm->is_pps_en_unlock && safe_batt_falg)
		usbpd_pd_contact(pdpm, true);
	else if (pdpm->pd_active && !__pdpm->is_pps_en_unlock)
		usbpd_pd_contact(pdpm, false);

	pdpm->psy_change_running = false;
}

static int usbpd_check_plugout(struct usbpd_pm *pdpm)
{
    int ret;
	union power_supply_propval val = {0,};

	ret = power_supply_get_property(pdpm->usb_psy,
			POWER_SUPPLY_PROP_ONLINE, &val);
	if (!ret) {
        if (!val.intval) {
            pdpm->is_cp_ok = false;
            usbpd_pm_enable_cp(pdpm, false);
            usbpd_pm_check_cp_enabled(pdpm);
            if (pm_config.cp_sec_enable) {
                usbpd_pm_enable_cp_sec(pdpm, false);
                usbpd_pm_check_cp_sec_enabled(pdpm);
            }
        }
    }
    
    return ret;
}

static int usbpd_psy_notifier_cb(struct notifier_block *nb, 
			unsigned long event, void *data)
{
	struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
	struct power_supply *psy = data;
	unsigned long flags;
	

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	usbpd_check_cp_psy(pdpm);
	if (pm_config.cp_sec_enable) {
		usbpd_check_cp_sec_psy(pdpm);
	}
	usbpd_check_charger_psy(pdpm);
	usbpd_check_battery_psy(pdpm);
	usbpd_check_tcpc(pdpm);
	usbpd_check_pca_chg_swchg(pdpm);

	if (!pdpm->cp_psy || !pdpm->usb_psy 
		|| !pdpm->tcpc || !pdpm->sw_chg)
		return NOTIFY_OK;

	usbpd_check_plugout(pdpm);

	if (psy == pdpm->cp_psy || psy == pdpm->usb_psy) {
		spin_lock_irqsave(&pdpm->psy_change_lock, flags);
		pr_err("[SC manager] >>>pdpm->psy_change_running : %d\n", pdpm->psy_change_running);
		if (!pdpm->psy_change_running) {
			pdpm->psy_change_running = true;
			if (psy == pdpm->cp_psy)
				schedule_work(&pdpm->cp_psy_change_work);
			else
				schedule_work(&pdpm->usb_psy_change_work);
		}
		spin_unlock_irqrestore(&pdpm->psy_change_lock, flags);
	}

	return NOTIFY_OK;
}
static int __init usbpd_pm_init(void)
{
	struct usbpd_pm *pdpm;
    int ret = 0;

	pdpm = kzalloc(sizeof(*pdpm), GFP_KERNEL);
	if (!pdpm)
		return -ENOMEM;

	__pdpm = pdpm;

	INIT_WORK(&pdpm->cp_psy_change_work, cp_psy_change_work);
	INIT_WORK(&pdpm->usb_psy_change_work, usb_psy_change_work);

	spin_lock_init(&pdpm->psy_change_lock);

	usbpd_check_cp_psy(pdpm);
	if (pm_config.cp_sec_enable) {
		usbpd_check_cp_sec_psy(pdpm);
	}
	//Extb HOMGMI-84843,chenrui1.wt,ADD,20210514,add adpo_max node
	usbpd_check_apdo_psy(pdpm);
	usbpd_check_charger_psy(pdpm);
	usbpd_check_battery_psy(pdpm);
	usbpd_check_tcpc(pdpm);
	usbpd_check_pca_chg_swchg(pdpm);

	INIT_DELAYED_WORK(&pdpm->pm_work, usbpd_pm_workfunc);

	/* register tcp notifier callback */
	pdpm->tcp_nb.notifier_call = pca_pps_tcp_notifier_call;
	ret = register_tcp_dev_notifier(pdpm->tcpc, &pdpm->tcp_nb,
					TCP_NOTIFY_TYPE_USB);
	if (ret < 0) {
		pr_err("register tcpc notifier fail\n");
		return ret;
	}

	if (is_kernel_power_off_charging())
		pdpm->therm_flag = true;
	else
		pdpm->therm_flag = false;

	pdpm->nb.notifier_call = usbpd_psy_notifier_cb;
	power_supply_reg_notifier(&pdpm->nb);

	return 0;
}

static void __exit usbpd_pm_exit(void)
{
	power_supply_unreg_notifier(&__pdpm->nb);
	cancel_delayed_work(&__pdpm->pm_work);
	cancel_work_sync(&__pdpm->cp_psy_change_work);
	cancel_work_sync(&__pdpm->usb_psy_change_work);

}

module_init(usbpd_pm_init);
module_exit(usbpd_pm_exit);


