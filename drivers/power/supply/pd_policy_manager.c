
#define pr_fmt(fmt)	"[USBPD-PM]: %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/usb/usbpd.h>
#include "pd_policy_manager.h"

#define PCA_PPS_CMD_RETRY_COUNT	2
#define PD_SRC_PDO_TYPE_FIXED       0
#define PD_SRC_PDO_TYPE_BATTERY     1
#define PD_SRC_PDO_TYPE_VARIABLE    2
#define PD_SRC_PDO_TYPE_AUGMENTED   3

#define BATT_MAX_CHG_VOLT           4460 //new requirement from xiaomi hw, CP should config 4460
#define BATT_FAST_CHG_CURR          5900
#define	BUS_OVP_THRESHOLD           12000
#define	BUS_OVP_ALARM_THRESHOLD     9500

#define BUS_VOLT_INIT_UP            400

#define BAT_VOLT_LOOP_LMT           BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT           BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT           BUS_OVP_THRESHOLD

#define PM_WORK_RUN_INTERVAL        300

/* jeita related */
#define JEITA_WARM_DISABLE_CP_THR       480
#define JEITA_COOL_DISABLE_CP_THR       100
#define JEITA_BYPASS_WARM_DISABLE_CP_THR        480
#define JEITA_BYPASS_COOL_DISABLE_CP_THR        100

enum {
    PM_ALGO_RET_OK,
    PM_ALGO_RET_THERM_FAULT,
    PM_ALGO_RET_OTHER_FAULT,
    PM_ALGO_RET_CHG_DISABLED,
    PM_ALGO_RET_TAPER_DONE,
};

static const struct pdpm_config pm_config = {
    .bat_volt_lp_lmt        = BAT_VOLT_LOOP_LMT,
    .bat_curr_lp_lmt        = BAT_CURR_LOOP_LMT,
    .bus_volt_lp_lmt        = BUS_VOLT_LOOP_LMT,
    .bus_curr_lp_lmt        = (BAT_CURR_LOOP_LMT >> 1),

    .fc2_taper_current      = 2300,
    .fc2_steps      = 1,

    .min_adapter_volt_required  = 10000,
    .min_adapter_curr_required  = 2000,

    .min_vbat_for_cp        = 3500,

    .cp_sec_enable          = false,
    .fc2_disable_sw			= true,
};

static struct usbpd_pm *__pdpm;

static int fc2_taper_timer;
static int ibus_lmt_change_timer;

extern int usbpd_select_pdo_maxim(int pdo, int mv, int ma);

static void usbpd_check_batt_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->sw_psy) {
		pdpm->sw_psy = power_supply_get_by_name("battery");
		if (!pdpm->sw_psy)
			pr_err("batt psy not found!\n");
	}
}

/*Get thermal level from battery power supply property */
static int pd_get_batt_current_thermal_level(struct usbpd_pm *pdpm, int *level)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	usbpd_check_batt_psy(pdpm);
	if (!pdpm->sw_psy)
		return -ENODEV;

	rc = power_supply_get_property(pdpm->sw_psy,
				POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL, &pval);
	if (rc < 0) {
		pr_err("Couldn't get system temp level :%d\n", rc);
		return rc;
	}else{
		pr_info("system temp level : %d\n", pval.intval);
	}

	*level = pval.intval;

	return rc;
}

/* determine whether to disable cp according to jeita status */
static bool pd_disable_cp_by_jeita_status(struct usbpd_pm *pdpm)
{
	union power_supply_propval pval = {0,};
	int batt_temp = 0, input_suspend = 0;
	int rc, bat_constant_voltage;
	int warm_thres, cool_thres;

	if (!pdpm->sw_psy)
		return false;

	rc = power_supply_get_property(pdpm->sw_psy,
			POWER_SUPPLY_PROP_INPUT_SUSPEND, &pval);
	if (!rc)
		input_suspend = pval.intval;

	pr_err("input_suspend: %d\n", input_suspend);

	/* is input suspend is set true, do not allow bq quick charging */
	if (input_suspend)
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
	warm_thres = JEITA_WARM_DISABLE_CP_THR;
	cool_thres = JEITA_COOL_DISABLE_CP_THR;

	if (batt_temp >= warm_thres && !pdpm->jeita_triggered) {
		pdpm->jeita_triggered = true;
		return true;
	} else if (batt_temp <= cool_thres && !pdpm->jeita_triggered) {
		pdpm->jeita_triggered = true;
		return true;
	} else if ((batt_temp <= (warm_thres - JEITA_HYSTERESIS))
			&& (batt_temp >= (cool_thres + JEITA_HYSTERESIS))
			&& pdpm->jeita_triggered) {
		pdpm->jeita_triggered = false;
		return false;
	} else {
		return pdpm->jeita_triggered;
	}
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
		ret = usbpd_pps_enable_charging(pdpm, true, 9000, 2000);
		if (ret != TCP_DPM_RET_SUCCESS)
			return false;
		return true;
	}
    return false;
}

static int usbpd_select_pdo(struct usbpd_pm *pdpm, u32 mV, u32 mA)
{
	int ret, cnt = 0;

	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		if(!pdpm)
		{
			return usbpd_select_pdo_maxim(0, mV, mA);
		}
		else
		{
			return usbpd_select_pdo_maxim(pdpm->apdo_selected_pdo, mV, mA);
		}
	}
	else
	{
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

/*
 * Enable charging of switching charger
 * For divide by two algorithm, according to swchg_ichg to decide enable or not
 *
 * @en: enable/disable
 */
static int usbpd_pm_enable_sw(struct usbpd_pm *pdpm, bool en)
{
	union power_supply_propval val = {0,};
	int ret;

	pr_err("usbpd_pm_enable_sw:en:%d\n", en);
	if (!pdpm->sw_psy) {
		pdpm->sw_psy = power_supply_get_by_name("battery");
		if (!pdpm->sw_psy) {
			return -ENODEV;
		}
	}

	pdpm->sw.charge_enabled = en;
	val.intval = en;
	ret = power_supply_set_property(pdpm->sw_psy,
			POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &val);

	return 0;
}

/*
 * Get ibus current of switching charger
 *
*/
/*
static int usbpd_pm_update_sw_status(struct usbpd_pm *pdpm)
{
	int ret, ibus;

	ret = charger_dev_get_adc(pdpm->sw_chg, PCA_ADCCHAN_IBUS, &ibus,
				   &ibus);
	if (ret < 0) {
		pr_err("get swchg ibus fail(%d)\n", ret);
		return ret;
	}
	pdpm->sw.ibus_curr = ibus / 1000;

	return ret;
}
*/
static void usbpd_check_tcpc(struct usbpd_pm *pdpm)
{
	if (!pdpm->tcpc) {
		pdpm->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (!pdpm->tcpc) {
			pr_err("get tcpc dev fail\n");
		}
	}
}

/*
static void usbpd_check_pca_chg_swchg(struct usbpd_pm *pdpm)
{
	if (!pdpm->sw_chg) {
		pdpm->sw_chg = get_charger_by_name("primary_chg");
		if (!pdpm->sw_chg) {
			pr_err("get primary_chg fail\n");
		}
	}
}
*/
static void usbpd_check_charger_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->usb_psy) { 
		pdpm->usb_psy = power_supply_get_by_name("usb");
		if (!pdpm->usb_psy)
			pr_err("usb psy not found!\n");
	}
}
static void usbpd_check_cp_psy(struct usbpd_pm *pdpm)
{

    if (!pdpm->cp_psy) {
        if (pm_config.cp_sec_enable) {
            pdpm->cp_psy = power_supply_get_by_name("sc8551-master");
            pr_err("sc8551-master found\n");
		} else
            pdpm->cp_psy = power_supply_get_by_name("sc8551-standalone");
        if (!pdpm->cp_psy)
            pr_err("cp_psy not found\n");
    }
}

static void usbpd_check_cp_sec_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->cp_sec_psy) {
        pdpm->cp_sec_psy = power_supply_get_by_name("sc8551-slave");
        if (!pdpm->cp_sec_psy)
            pr_err("cp_sec_psy not found\n");
        else
            pr_err("sc8551-slave found\n");
    }
}

static void usbpd_pm_update_cp_status(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    usbpd_check_cp_psy(pdpm);

    if (!pdpm->cp_psy)
        return;

    /*ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_SC_BATTERY_VOLTAGE, &val);
    if (!ret)
        pdpm->cp.vbat_volt = val.intval; 

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_SC_BATTERY_CURRENT, &val);
    if (!ret)
        pdpm->cp.ibat_curr_cp = val.intval;*/

	if(pdpm->bms_psy){
		ret = power_supply_get_property(pdpm->bms_psy,
				POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		if(!ret)
			pdpm->cp.ibat_curr = (val.intval / 1000);
	}
	//pdpm->cp.ibat_curr = pdpm->cp.ibat_curr_sw + pdpm->cp.ibat_curr_cp;//+ pdpm->cp.ibat_curr_sw; 
    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_SC_BUS_VOLTAGE, &val);
    if (!ret)
        pdpm->cp.vbus_volt = val.intval; 

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_SC_BUS_CURRENT, &val);
    if (!ret)
        pdpm->cp.ibus_curr_cp = val.intval;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_SC_VBUS_ERROR_STATUS, &val);
    if (!ret)
    {
        pr_err(">>>>vbus error state : %02x\n", val.intval);
        pdpm->cp.vbus_error_low = (val.intval >> 5) & 0x01;
        pdpm->cp.vbus_error_high = (val.intval >> 4) & 0x01;
    }
        
    pdpm->cp.ibus_curr = pdpm->cp.ibus_curr_cp + pdpm->cp.ibus_curr_sw; 

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

    ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_SC_VBUS_ERROR_STATUS, &val);
    if (!ret){
        pr_err(">>>>slave cp vbus error state : %02x\n", val.intval);
        pdpm->cp_sec.vbus_error_low = (val.intval >> 5) & 0x01;
        pdpm->cp_sec.vbus_error_high = (val.intval >> 4) & 0x01;
    }

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
	
	pr_err("check_cp_enabled:%d", pdpm->cp.charge_enabled);
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

	pr_err("check_cp_sec_enabled:%d", pdpm->cp_sec.charge_enabled);
	return ret;
}

static int usbpd_pm_check_sw_enabled(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->sw_psy) {
        pdpm->sw_psy = power_supply_get_by_name("battery");
        if (!pdpm->sw_psy) {
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(pdpm->sw_psy, 
            POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &val);
    if (!ret)
        pdpm->sw.charge_enabled = !!val.intval;

    return ret;
}
/*
static void usbpd_pm_update_sw_status(struct usbpd_pm *pdpm)
{
    usbpd_pm_check_sw_enabled(pdpm);
}
*/
static void usbpd_pm_evaluate_src_caps_maxim(struct usbpd_pm *pdpm)
{
    //int ret;
    int i;

    pdpm->pdo = usbpd_fetch_pdo();
    pr_err("usbpd_pm_evaluate_src_caps::0x%x\n", (char *)pdpm->pdo);

    pdpm->apdo_max_volt = pm_config.min_adapter_volt_required;
    pdpm->apdo_max_curr = pm_config.min_adapter_curr_required;

    for (i = 0; i < 7; i++) {
        pr_err("[SC manager] %d type %d\n", i, pdpm->pdo[i].apdo);

        if (pdpm->pdo[i].apdo == true) {
            if (pdpm->pdo[i].max_voltage >= pdpm->apdo_max_volt
                    && pdpm->pdo[i].max_current > pdpm->apdo_max_curr) {
                pdpm->apdo_max_volt = pdpm->pdo[i].max_voltage;
                pdpm->apdo_max_curr = pdpm->pdo[i].max_current;
                pdpm->apdo_selected_pdo = i;
                pdpm->pps_supported = true;
                pr_err("[SC manager] vola %d  curr %d\n", 
                        pdpm->apdo_max_volt, pdpm->apdo_max_curr);
            }		
        }
    }

    if (pdpm->pps_supported)
        pr_notice("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d\n",
                pdpm->apdo_selected_pdo,
                pdpm->apdo_max_volt,
                pdpm->apdo_max_curr);
    else
        pr_notice("Not qualified PPS adapter\n");
}

#if 0
static void usbpd_pm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
    int ret;
    int i;

    if (!pdpm->pd) {
        pdpm->pd = smb_get_usbpd();
        if (!pdpm->pd) {
            pr_err("couldn't get usbpd device\n");
            return;
        }
    }

    ret = usbpd_fetch_pdo(pdpm->pd, pdpm->pdo);
    if (ret) {
        pr_err("Failed to fetch pdo info\n");
        return;
    }

    pdpm->apdo_max_volt = pm_config.min_adapter_volt_required;
    pdpm->apdo_max_curr = pm_config.min_adapter_curr_required;

    for (i = 0; i < 7; i++) {
        pr_err("[SC manager] %d type %d\n", i, pdpm->pdo[i].type);

        if (pdpm->pdo[i].type == PD_SRC_PDO_TYPE_AUGMENTED 
            && pdpm->pdo[i].pps && pdpm->pdo[i].pos) {
            if (pdpm->pdo[i].max_volt_mv >= pdpm->apdo_max_volt
                    && pdpm->pdo[i].curr_ma > pdpm->apdo_max_curr) {
                pdpm->apdo_max_volt = pdpm->pdo[i].max_volt_mv;
                pdpm->apdo_max_curr = pdpm->pdo[i].curr_ma;
                pdpm->apdo_selected_pdo = pdpm->pdo[i].pos;
                pdpm->pps_supported = true;
                pr_err("[SC manager] vola %d  curr %d\n", 
                        pdpm->apdo_max_volt, pdpm->apdo_max_curr);
            }		
        }
    }

    if (pdpm->pps_supported)
        pr_notice("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d\n",
                pdpm->apdo_selected_pdo,
                pdpm->apdo_max_volt,
                pdpm->apdo_max_curr);
    else
        pr_notice("Not qualified PPS adapter\n");
}

static void usbpd_update_pps_status(struct usbpd_pm *pdpm)
{
    int ret;
    u32 status;

    ret = usbpd_get_pps_status(pdpm->pd, &status);

    if (!ret) {
        /*TODO: check byte order to insure data integrity*/
        pdpm->adapter_voltage = ((status >> 16) & 0xFFFF )* 20;
        pdpm->adapter_current = ((status >> 8) & 0xFF ) * 50;
        pdpm->adapter_ptf = (status & 0x06) >> 1;
        pdpm->adapter_omf = !!(status & 0x08);
        pr_err("adapter_volt:%d, adapter_current:%d\n",
                pdpm->adapter_voltage, pdpm->adapter_current);
    }
}
#endif

static void usbpd_pm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
	bool retValue;

	retValue = usbpd_get_pps_status(pdpm);
	if (retValue)
		pdpm->pps_supported = true;
	else
		pdpm->pps_supported = false;

	
	if (pdpm->pps_supported)
		pr_notice("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d\n",
				pdpm->apdo_selected_pdo,
				pdpm->apdo_max_volt,
				pdpm->apdo_max_curr);
	else
		pr_notice("Not qualified PPS adapter\n");
}


static int usbpd_update_ibat_curr(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->bms_psy) {
        pdpm->bms_psy = power_supply_get_by_name("bms");
        if (!pdpm->bms_psy) {
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(pdpm->bms_psy, 
            POWER_SUPPLY_PROP_CURRENT_NOW, &val);
    if (!ret)
        pdpm->cp.ibat_curr_sw= (int)(val.intval/1000);

    ret = power_supply_get_property(pdpm->bms_psy, 
            POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
    if (!ret)
        pdpm->cp.vbat_volt = (int)(val.intval/1000);

	pr_err("usbpd_update_ibat_curr: ibat_curr_fg:%d vbat_volt_fg:%d\n",
		pdpm->cp.ibat_curr_sw, pdpm->cp.vbat_volt);

    return ret;
}

#if 0
static int usbpd_update_ibus_curr(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->sw_psy) {
        pdpm->sw_psy = power_supply_get_by_name("usb");
        if (!pdpm->sw_psy) {
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(pdpm->sw_psy, 
            POWER_SUPPLY_PROP_INPUT_CURRENT_NOW, &val);
    if (!ret)
        pdpm->cp.ibus_curr_sw = (int)(val.intval/1000);

    return ret;
}
#endif
/*static void usbpd_pm_disconnect(struct usbpd_pm *pdpm);
static void usb_psy_pd_active_update(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    ret = power_supply_get_property(pdpm->usb_psy,
            POWER_SUPPLY_PROP_PD_ACTIVE, &val);
    if (ret) {
        pr_err("Failed to get usb pd active state\n");
        return;
    }

    if(!val.intval)
    {
        pdpm->pd_active = 0;
    }
    else{
        pdpm->pd_active = 1;
    }
}*/


#define TAPER_TIMEOUT	10
#define IBUS_CHANGE_TIMEOUT  5
static int usbpd_pm_fc2_charge_algo(struct usbpd_pm *pdpm)
{
    int steps = 0, sw_ctrl_steps = 0, hw_ctrl_steps = 0;
    int step_vbat = 0, step_ibus = 0, step_ibat = 0;
    int step_bat_reg = 0;
    int ibus_total = 0;
	int fcc_vote_val = 0, effective_fcc_taper = 0;
	int fcc_ibatt_diff = 0 ,sicl_ibus_diff = 0;
	int bq_taper_hys_mv = BQ_TAPER_HYS_MV;
    static int ibus_limit, fcc_limit;
	int time_delta = 0;
	int thermal_level = 0;

	pd_get_batt_current_thermal_level(pdpm, &thermal_level);
	time_delta = ktime_ms_delta(ktime_get(), pdpm->entry_bq_cv_time);
	if(pdpm->fcc_votable)
		fcc_vote_val = get_effective_result(pdpm->fcc_votable);
	else{
		pdpm->fcc_votable = find_votable("FCC");
		fcc_vote_val = get_effective_result(pdpm->fcc_votable);
	}
	fcc_limit = min(fcc_vote_val, pm_config.bat_curr_lp_lmt);
    ibus_limit = fcc_limit >> 1;

    /* reduce bus current in cv loop */
    if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - bq_taper_hys_mv) {
        if (ibus_lmt_change_timer++ > IBUS_CHANGE_TIMEOUT) {
            ibus_lmt_change_timer = 0;
			ibus_limit = ibus_limit - 100;
            effective_fcc_taper = fcc_vote_val - BQ_TAPER_DECREASE_STEP_MA;
			if(pdpm->fcc_votable){
				if(effective_fcc_taper >= 2000)
					vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER, true, effective_fcc_taper);
				pr_err("bq set taper fcc to : %d mA\n", effective_fcc_taper);
			}
        }
    } else if (pdpm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - 250) {
        ibus_lmt_change_timer = 0;
    } else {
        ibus_lmt_change_timer = 0;
    }

    /* battery voltage loop*/
    if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt)
        step_vbat = -pm_config.fc2_steps;
    else if (pdpm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - 10)
        step_vbat = pm_config.fc2_steps;;
	pr_err("vbat:%d lmt:%d step:%d", pdpm->cp.vbat_volt, pm_config.bat_volt_lp_lmt, step_vbat);

    /* battery charge current loop*/
    if (pdpm->cp.ibat_curr < fcc_limit)
        step_ibat = pm_config.fc2_steps;
    else if (pdpm->cp.ibat_curr > fcc_limit + 100)
        step_ibat = -pm_config.fc2_steps;
	pr_err("ibat:%d lmt:%d step:%d", pdpm->cp.ibat_curr, fcc_limit, step_ibat);

    /* bus current loop*/
    ibus_total = pdpm->cp.ibus_curr;
    if (pm_config.cp_sec_enable)
		ibus_total += pdpm->cp_sec.ibus_curr;

    if (ibus_total < ibus_limit - 130)
        step_ibus = pm_config.fc2_steps;
    else if (ibus_total > ibus_limit - 80)
        step_ibus = -pm_config.fc2_steps;
	pr_err("ibus:%d lmt:%d step:%d", ibus_total, ibus_limit, step_ibus);

    /* hardware regulation loop*/
    /*if (pdpm->cp.vbat_reg || pdpm->cp.ibat_reg)
        step_bat_reg = 5 * (-pm_config.fc2_steps);
    else
        step_bat_reg = pm_config.fc2_steps;*/
    step_bat_reg = pm_config.fc2_steps;

    sw_ctrl_steps = min(min(step_vbat, step_ibus), step_ibat);
    sw_ctrl_steps = min(sw_ctrl_steps, step_bat_reg);

    /* hardware alarm loop */
    if (pdpm->cp.bat_ocp_alarm /*|| pdpm->cp.bat_ovp_alarm */
        || pdpm->cp.bus_ocp_alarm || pdpm->cp.bus_ovp_alarm
        //|| pdpm->cp.vbus_error_high || pdpm->cp_sec.vbus_error_high
        /*|| pdpm->cp.tbat_temp > 60 
            || pdpm->cp.tbus_temp > 50*/)
        hw_ctrl_steps = -pm_config.fc2_steps;
    else
        hw_ctrl_steps = pm_config.fc2_steps;

	pr_err("sw_steps:%d hw_steps:%d m_vbush:%d s_vbush:%d",
		sw_ctrl_steps, hw_ctrl_steps, pdpm->cp.vbus_error_high, pdpm->cp_sec.vbus_error_high);

    /* check if cp disabled due to other reason*/
    usbpd_pm_check_cp_enabled(pdpm);
    pr_err("cp enable: %d\n", pdpm->cp.charge_enabled);
    if (pm_config.cp_sec_enable) {
        usbpd_pm_check_cp_sec_enabled(pdpm);
        pr_err("cp sec enable: %d\n", pdpm->cp_sec.charge_enabled);
		if(!pdpm->cp_sec.charge_enabled && pdpm->cp.ibat_curr > 3000)
			usbpd_pm_enable_cp_sec(pdpm, true);
    }
    pdpm->is_temp_out_fc2_range = pd_disable_cp_by_jeita_status(pdpm);
    pr_info("is_temp_out_fc2_range = %d\n", pdpm->is_temp_out_fc2_range);

    if (pdpm->cp.bat_therm_fault ) { /* battery overheat, stop charge*/
        pr_notice("bat_therm_fault:%d\n", pdpm->cp.bat_therm_fault);
        return PM_ALGO_RET_THERM_FAULT;
    }else if (thermal_level >= MAX_THERMAL_LEVEL_FOR_CP || pdpm->is_temp_out_fc2_range) {
        pr_info("system thermal level too high or batt temp is out of fc2 range\n");
        return PM_ALGO_RET_CHG_DISABLED;
    } else if (pdpm->cp.bat_ocp_fault || pdpm->cp.bus_ocp_fault 
            || pdpm->cp.bat_ovp_fault || pdpm->cp.bus_ovp_fault) {
        pr_notice("bat_ocp_fault:%d, bus_ocp_fault:%d, bat_ovp_fault:%d,bus_ovp_fault:%d\n", pdpm->cp.bat_ocp_fault,
                pdpm->cp.bus_ocp_fault, pdpm->cp.bat_ovp_fault,
                pdpm->cp.bus_ovp_fault);
            return PM_ALGO_RET_OTHER_FAULT; /* go to switch, and try to ramp up*/
    } else if ((!pdpm->cp.charge_enabled && (pdpm->cp.vbus_error_low || pdpm->cp.vbus_error_high)) 
    			//|| (pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled && !pdpm->cp_sec_stopped)
    			) {
        pr_notice("cp.charge_enabled:%d  %d  %d\n", pdpm->cp.charge_enabled, pdpm->cp.vbus_error_low, pdpm->cp.vbus_error_high);
        return PM_ALGO_RET_CHG_DISABLED;
    }

    /* charge pump taper charge */
    if ((pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - TAPER_VOL_HYS)
            && pdpm->cp.ibat_curr < pm_config.fc2_taper_current) {
        if (fc2_taper_timer++ > TAPER_TIMEOUT) {
            pr_err("charge pump taper charging done\n");
            fc2_taper_timer = 0;
            return PM_ALGO_RET_TAPER_DONE;
        }
    } else {
        fc2_taper_timer = 0;
    }
        
    /*TODO: customer can add hook here to check system level 
        * thermal mitigation*/

    steps = min(sw_ctrl_steps, hw_ctrl_steps);

    if(pdpm->cp.ibat_curr > 0 && ibus_total > 0 && time_delta < QUICK_RAISE_VOLT_INTERVAL_S){
		fcc_ibatt_diff = (pdpm->cp.ibat_curr > fcc_limit) ? (pdpm->cp.ibat_curr - fcc_limit) : (fcc_limit - pdpm->cp.ibat_curr);
		sicl_ibus_diff = (ibus_total > ibus_limit) ? (ibus_total - ibus_limit) : (ibus_limit - ibus_total);
		pr_notice("fcc_ibatt_diff:%d sicl_ibus_diff:%d\n", fcc_ibatt_diff, sicl_ibus_diff);
		if (fcc_ibatt_diff > 1200 && sicl_ibus_diff > 500)
			steps = steps * 5;
		else if (fcc_ibatt_diff > 500 && fcc_ibatt_diff <= 1200 && sicl_ibus_diff <= 500 && sicl_ibus_diff > 250)
			steps = steps * 3;
	}
    pr_err(">>>>>>%d %d %d sw %d hw %d all %d\n", 
            step_vbat, step_ibat, step_ibus, sw_ctrl_steps, hw_ctrl_steps, steps);
    pdpm->request_voltage += steps * 20;
    if (pdpm->request_voltage > pdpm->apdo_max_volt - 1000)
        pdpm->request_voltage = pdpm->apdo_max_volt - 1000;

    /*if (pdpm->adapter_voltage > 0
            && pdpm->request_voltage > pdpm->adapter_voltage + 500)
        pdpm->request_voltage = pdpm->adapter_voltage + 500;*/

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
    pr_err("state change:%s -> %s\n", 
        pm_str[pdpm->state], pm_str[state]);
    pdpm->state = state;
}

static int usbpd_pm_sm(struct usbpd_pm *pdpm)
{
    int ret = 0, rc = 0, thermal_level = 0;
    static int tune_vbus_retry;
    static bool stop_sw;
    static bool recover;

    pr_err("pm_sm state phase :%d\n", pdpm->state);
    pr_err("pm_sm vbus_vol:%d vbat_vol:%d ibat_curr:%d\n",
		pdpm->cp.vbus_volt, pdpm->cp.vbat_volt, pdpm->cp.ibat_curr);
    switch (pdpm->state) {
    case PD_PM_STATE_ENTRY:
        stop_sw = false;
        recover = false;
        pd_get_batt_current_thermal_level(pdpm, &thermal_level);
        pdpm->is_temp_out_fc2_range =pd_disable_cp_by_jeita_status(pdpm);
        pr_info("is_temp_out_fc2_range:%d\n", pdpm->is_temp_out_fc2_range);

        if (pdpm->cp.vbat_volt < pm_config.min_vbat_for_cp) {
            pr_err("pm_sm batt_volt:%d, waiting...\n", pdpm->cp.vbat_volt);
        } else if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 100) {
            pr_err("pm_sm batt_volt:%d too high for cp,switch main charger\n", pdpm->cp.vbat_volt);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
        } else if(thermal_level >= MAX_THERMAL_LEVEL_FOR_CP || pdpm->is_temp_out_fc2_range){
            pr_info("system thermal level too high or batt temp is out of fc2 range, waiting...\n");
	}else{
            pr_err("pm_sm batt_volt:%d is ok, start flash charging\n", pdpm->cp.vbat_volt);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY);
        }
        break;

    case PD_PM_STATE_FC2_ENTRY:
        if (pm_config.fc2_disable_sw) {
			usbpd_pm_enable_sw(pdpm, false);
            /* if (pdpm->sw.charge_enabled) { */
                /* usbpd_pm_enable_sw(pdpm, false); */
				/* usbpd_pm_check_sw_enabled(pdpm); */
            /* } */
            if (!pdpm->sw.charge_enabled)
                usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
        } else {
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
        }
        break;

    case PD_PM_STATE_FC2_ENTRY_1:
        pdpm->request_voltage = pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP;
        pdpm->request_current = min(pdpm->apdo_max_curr, pm_config.bus_curr_lp_lmt);
		usbpd_select_pdo(pdpm,pdpm->request_voltage, pdpm->request_current);
        pr_err("request_voltage:%d, request_current:%d\n", pdpm->request_voltage, pdpm->request_current);
        usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);
        tune_vbus_retry = 0;
        break;

    case PD_PM_STATE_FC2_ENTRY_2:
        pr_err("tune_vbus_retry %d vbus_low:%d vbus_high:%d\n", tune_vbus_retry, pdpm->cp.vbus_error_low, pdpm->cp.vbus_error_high);
        if (pdpm->cp.vbus_error_low || pdpm->cp.vbus_volt < pdpm->cp.vbat_volt*2 + BUS_VOLT_INIT_UP - 50) {
            tune_vbus_retry++;
            pdpm->request_voltage += 20;
			usbpd_select_pdo(pdpm,pdpm->request_voltage, pdpm->request_current);
            pr_err("vbus low,request_volt:%d,request_curr:%d\n", pdpm->request_voltage, pdpm->request_current);
        } else if (pdpm->cp.vbus_error_high || pdpm->cp.vbus_volt > pdpm->cp.vbat_volt*2 + BUS_VOLT_INIT_UP + 200) {
            tune_vbus_retry++;
            pdpm->request_voltage -= 20;
			usbpd_select_pdo(pdpm,pdpm->request_voltage,pdpm->request_current);
            pr_err("vbus high,request_volt:%d, request_cur:%d\n", pdpm->request_voltage, pdpm->request_current);
        } else {
            pr_notice("adapter volt tune ok, retry %d times\n", tune_vbus_retry);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_3);
            break;
        }
        
        if (tune_vbus_retry > 30) {
            pr_notice("Failed to tune adapter volt into valid range,charge with switching charger,will try to recover\n");
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
            if (NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type())
            	recover = true;
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
            if(!pdpm->cp_sec.charge_enabled) {
                usbpd_pm_enable_cp_sec(pdpm, true);
                usbpd_pm_check_cp_sec_enabled(pdpm);
            }
        }

        if (pdpm->cp.charge_enabled) {
            if ((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled)
                    || !pm_config.cp_sec_enable) {
                pdpm->entry_bq_cv_time  = ktime_get();
                usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_TUNE);
                ibus_lmt_change_timer = 0;
                fc2_taper_timer = 0;
            }
        }
        break;

    case PD_PM_STATE_FC2_TUNE:
        ret = usbpd_pm_fc2_charge_algo(pdpm);
        if (ret == PM_ALGO_RET_THERM_FAULT) {
            pr_err("Move to stop charging:%d\n", ret);
            stop_sw = true;
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
            break;
        } else if (ret == PM_ALGO_RET_OTHER_FAULT || ret == PM_ALGO_RET_TAPER_DONE) {
            pr_err("Move to switch charging:%d\n", ret);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
            break;
        } else if (ret == PM_ALGO_RET_CHG_DISABLED) {
            pr_err("Move to switch charging, will try to recover flash charging:%d\n", ret);
            recover = true;
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
            break;
        } else {
			usbpd_select_pdo(pdpm,pdpm->request_voltage,
					 pdpm->request_current);
            pr_err("request_voltage:%d, request_current:%d\n",
                    pdpm->request_voltage, pdpm->request_current);
        }

        /*stop second charge pump if either of ibus is lower than 750ma during CV*/
        if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled 
				&& pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 50
				&& (pdpm->cp.ibus_curr < 750 || pdpm->cp_sec.ibus_curr < 750)) {
			pr_notice("second cp is disabled due to ibus < 750mA\n");
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
			pdpm->cp_sec_stopped = true;
		}
        break;

    case PD_PM_STATE_FC2_EXIT:
        /* select default 5V*/
		/* if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type()) */
		/* { */
            /* usbpd_select_pdo(0,5000,3000); */
		/* } */
		/* else */
		{
			usbpd_select_pdo(pdpm,9000,2000); // 9V-2A 
		}

        if(pdpm->fcc_votable)
        	vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER, false, 0);

        if (pdpm->cp.charge_enabled) {
            usbpd_pm_enable_cp(pdpm, false);
            usbpd_pm_check_cp_enabled(pdpm);
        }

        if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled) {
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}

		pr_err(">>>sw state %d   %d\n", stop_sw, pdpm->sw.charge_enabled);
		if (stop_sw && pdpm->sw.charge_enabled)
			usbpd_pm_enable_sw(pdpm, false);
		else if (!stop_sw && !pdpm->sw.charge_enabled)
			usbpd_pm_enable_sw(pdpm, true);

		if (recover)
			usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
		else
		{
			if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type()){
				//do nothing 
			}
			else if(NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type()){
				//do nothing
			}
			else{
				usbpd_pps_enable_charging(pdpm,false,5000,3000);
			}
			rc = 1;
		}
		
	        break;
	}

    return rc;
}

static void usbpd_pm_workfunc(struct work_struct *work)
{
    struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, pm_work.work);

 //   usbpd_pm_update_sw_status(pdpm);
 //   usbpd_update_ibus_curr(pdpm);
    usbpd_pm_update_cp_status(pdpm);
    usbpd_pm_update_cp_sec_status(pdpm);
    usbpd_update_ibat_curr(pdpm);

    if (!usbpd_pm_sm(pdpm) && pdpm->pd_active)
        schedule_delayed_work(&pdpm->pm_work, msecs_to_jiffies(PM_WORK_RUN_INTERVAL));

}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
    usbpd_pm_enable_cp(pdpm, false);
    usbpd_pm_check_cp_enabled(pdpm);
    if (pm_config.cp_sec_enable) {
        usbpd_pm_enable_cp_sec(pdpm, false);
        usbpd_pm_check_cp_sec_enabled(pdpm);
    }
    cancel_delayed_work(&pdpm->pm_work);

    if (!pdpm->sw.charge_enabled) {
        usbpd_pm_enable_sw(pdpm, true);
        usbpd_pm_check_sw_enabled(pdpm);
    }

    pdpm->pps_supported = false;
    pdpm->apdo_selected_pdo = 0;
    pdpm->jeita_triggered = false;
    pdpm->is_temp_out_fc2_range = false;

    usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
}

static void usbpd_pd_contact(struct usbpd_pm *pdpm, bool connected)
{
    pdpm->pd_active = connected;
    pr_err("usbpd_pd_contact  pd_active %d\n", pdpm->pd_active);
    if (connected) {
        msleep(10);
		if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
		{
			usbpd_pm_evaluate_src_caps_maxim(pdpm);
		}
		else
		{
        	usbpd_pm_evaluate_src_caps(pdpm);
		}
        pr_err("start cp charging pps support %d\n", pdpm->pps_supported);
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

	pr_err("cp_psy_change_work\n");
    pdpm->psy_change_running = false;
}


static int usbpd_check_plugout(struct usbpd_pm *pdpm)
{
    int ret;
	union power_supply_propval val = {0,};

	/*plugout disable master/slave cp*/
	ret = power_supply_get_property(pdpm->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &val);
    if (!ret) {
        if (!val.intval) {
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

static void usb_psy_change_work(struct work_struct *work)
{
	int ret = 0;
	union power_supply_propval propval;
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					usb_psy_change_work);

    usbpd_check_plugout(pdpm);
	ret = power_supply_get_property(pdpm->usb_psy, POWER_SUPPLY_PROP_PD_ACTIVE, &propval);

	pr_err("pre_pd_active %d,now:%d\n",pdpm->pd_active, propval.intval);

	if (!pdpm->pd_active && propval.intval == 2)
		usbpd_pd_contact(pdpm, true);
	else if (!pdpm->pd_active && propval.intval == 1) {
		if(NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type())
			usbpd_select_pdo(pdpm,9000,2000);
	} else if (pdpm->pd_active && !propval.intval)
		usbpd_pd_contact(pdpm, false);

	pdpm->psy_change_running = false;

}


static int usbpd_psy_notifier_cb(struct notifier_block *nb, 
			unsigned long event, void *data)
{
    struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
    struct power_supply *psy = data;
    unsigned long flags;

	pr_err("usbpd_psy_notifier_cb  start\n");
	pr_err("nopmi_init_usb_psy:0x%x\n", (char *)psy);

    if (event != PSY_EVENT_PROP_CHANGED)
        return NOTIFY_OK;

    usbpd_check_cp_psy(pdpm);
	if (pm_config.cp_sec_enable) {
		usbpd_check_cp_sec_psy(pdpm);
	}
	usbpd_check_charger_psy(pdpm);
	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		//do nothing
	}
	else
	{
		usbpd_check_tcpc(pdpm);
		if(!pdpm->tcpc)
		{
			return NOTIFY_OK;
		}
	}
	//usbpd_check_pca_chg_swchg(pdpm);

	if (!pdpm->cp_psy || !pdpm->usb_psy)
		return NOTIFY_OK;
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
	static int probe_cnt = 0;
	pr_err("usbpd_pm_init start: probe_cnt:%d\n", ++probe_cnt);

	pr_err("2012.09.04 wsy %s: start\n", __func__);

    pdpm = kzalloc(sizeof(*pdpm), GFP_KERNEL);
    if (!pdpm)
        return -ENOMEM;

    __pdpm = pdpm;

    spin_lock_init(&pdpm->psy_change_lock);

	//get master/slave cp phy
	usbpd_check_cp_psy(pdpm);
	if (pm_config.cp_sec_enable) {
		usbpd_check_cp_sec_psy(pdpm);
	}
	//get usb phy, tcpc port
	usbpd_check_charger_psy(pdpm);
	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		//do nothing
	}
	else
	{
		usbpd_check_tcpc(pdpm);
		if(!pdpm->tcpc)
		{
			ret = -EPROBE_DEFER;
			if (probe_cnt <= 10)
				return ret;
		}
	}
	if(!pdpm->cp_psy || (pm_config.cp_sec_enable && !pdpm->cp_sec_psy) || !pdpm->usb_psy){
		ret = -EPROBE_DEFER;
		if (probe_cnt <= 10)
			return ret;
	}
	//usbpd_check_pca_chg_swchg(pdpm);
	INIT_WORK(&pdpm->cp_psy_change_work, cp_psy_change_work);
    INIT_WORK(&pdpm->usb_psy_change_work, usb_psy_change_work);
	INIT_DELAYED_WORK(&pdpm->pm_work, usbpd_pm_workfunc);

	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		//do nothing
	}
	else
	{
		/* register tcp notifier callback */
		pdpm->tcp_nb.notifier_call = pca_pps_tcp_notifier_call;
		ret = register_tcp_dev_notifier(pdpm->tcpc, &pdpm->tcp_nb,
						TCP_NOTIFY_TYPE_USB);
		if (ret < 0) {
			pr_err("register tcpc notifier fail\n");
			return ret;
		}
	}
    pdpm->nb.notifier_call = usbpd_psy_notifier_cb;
    power_supply_reg_notifier(&pdpm->nb);

	pdpm->fcc_votable = find_votable("FCC");
	pr_err("usbpd_pm_init end\n");
	pr_err("2012.09.04 wsy %s: end\n", __func__);
    return 0;
}

static void __exit usbpd_pm_exit(void)
{
    power_supply_unreg_notifier(&__pdpm->nb);
    cancel_delayed_work(&__pdpm->pm_work);
    cancel_work_sync(&__pdpm->cp_psy_change_work);
    cancel_work_sync(&__pdpm->usb_psy_change_work);
}

late_initcall(usbpd_pm_init);
module_exit(usbpd_pm_exit);
