
#define pr_fmt(fmt)	"[USBPD-PM]: %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/usb/usbpd.h>
#include <linux/platform_device.h>

#include "inc/xm_pd_mngr.h"

#define PCA_PPS_CMD_RETRY_COUNT	2

#define PD_SRC_PDO_TYPE_FIXED       0
#define PD_SRC_PDO_TYPE_BATTERY     1
#define PD_SRC_PDO_TYPE_VARIABLE    2
#define PD_SRC_PDO_TYPE_AUGMENTED   3

#define BATT_MAX_CHG_VOLT           4472
#define BATT_FAST_CHG_CURR          12200
#define	BUS_OVP_THRESHOLD           12000
#define	BUS_OVP_ALARM_THRESHOLD     9500

#define BUS_VOLT_INIT_UP            200 //need to change 200

#define BAT_VOLT_LOOP_LMT           BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT           BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT           BUS_OVP_THRESHOLD

#define PM_WORK_RUN_INTERVAL        1000

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

    .fc2_taper_current      = 2000,
    .fc2_steps      = 1,

    .min_adapter_volt_required  = 10000,
    .min_adapter_curr_required  = 2000,

    .min_vbat_for_cp        = 3500,

    .cp_sec_enable          = true,
    .fc2_disable_sw			= true,
};

static struct usbpd_pm *__pdpm;
static int fc2_taper_timer;
static int ibus_lmt_change_timer;

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
    int maxwatt = 0;

    pr_err("usbpd_get_pps_status start\n");

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
			TCPM_POWER_CAP_APDO_TYPE_PPS, &cap_idx, &apdo_cap);
        if (ret != TCP_DPM_RET_SUCCESS) {
            pr_err("inquire pd apdo fail(%d)\n", ret);
            break;
        }

		maxwatt = apdo_cap.max_mv * apdo_cap.ma;
        pr_err("cap_idx[%d], %d W %d mv ~ %d mv, %d ma, pl: %d\n", cap_idx,
		maxwatt/1000000, apdo_cap.min_mv, apdo_cap.max_mv, apdo_cap.ma, apdo_cap.pwr_limit);

	if (apdo_cap.max_mv < pm_config.min_adapter_volt_required ||
		apdo_cap.ma < pm_config.min_adapter_curr_required)
		continue;

	if (apdo_idx == -1 || maxwatt > pdpm->apdo_maxwatt) {
		apdo_idx = cap_idx;
		pdpm->apdo_max_volt = apdo_cap.max_mv;
		pdpm->apdo_min_volt = apdo_cap.min_mv;
		pdpm->apdo_max_curr = apdo_cap.ma;
		pdpm->apdo_maxwatt = maxwatt;

		pr_err("select potential cap_idx[%d]\n", cap_idx);
		pr_err("potential apdo_max_volt %d, apdo_max_volt %d, apdo_maxwatt %d\n",
				pdpm->apdo_max_volt, pdpm->apdo_max_curr, pdpm->apdo_maxwatt);

		ret = xm_battmngr_write_iio_prop(g_battmngr_iio, PD_PHY,
					PD_APDO_VOLT_MAX, pdpm->apdo_max_volt);
		if (ret < 0) {
			pr_err("set APDO_MAX_VOLT fail(%d)\n", ret);
			return ret;
		}
		ret = xm_battmngr_write_iio_prop(g_battmngr_iio, PD_PHY,
					PD_APDO_CURR_MAX, pdpm->apdo_max_curr);
		if (ret < 0) {
			pr_err("set APDO_MAX_CURR fail(%d)\n", ret);
			return ret;
		}
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

	if (mV > pdpm->apdo_max_volt) {
		mV = pdpm->apdo_max_volt;
	} else if (mV < pdpm->apdo_min_volt) {
		mV = pdpm->apdo_min_volt;
		pr_err("use min voltage %dmV, %dmA\n", mV, mA);
	}

	if (mA >= 6000 && pdpm->cp.vout_volt > 7000) {
		if(pdpm->cp.ibat_curr <= 4000) {
			mA = 3000;
		} else if (pdpm->cp.ibat_curr > 4000 && pdpm->cp.ibat_curr < 6000) {
			mA = 4000;
		}
		if(!pdpm->cp_sec.charge_enabled  && pdpm->cp.ibat_curr < 3000)
			mA = 2000;
	}

    do {
        ret = tcpm_dpm_pd_request(pdpm->tcpc, mV, mA, NULL);
        cnt++;
    } while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

    if (ret != TCP_DPM_RET_SUCCESS) {
        ret = tcpm_reset_pd_charging_policy(pdpm->tcpc, NULL);
        pr_err("fail(%d) hardreset\n", ret);
	}

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
           __pdpm->psy_change_running = false;
           break;
       case PD_CONNECT_HARD_RESET:
           __pdpm->hrst_cnt++;
           pr_err("pd hardreset, cnt = %d\n",  __pdpm->hrst_cnt);
           __pdpm->is_pps_en_unlock = false;
           __pdpm->psy_change_running = false;
           break;
       case PD_CONNECT_PE_READY_SNK:
       case PD_CONNECT_PE_READY_SNK_PD30:
           cancel_delayed_work(&__pdpm->pd_work);
           schedule_delayed_work(&__pdpm->pd_work, msecs_to_jiffies(6000));
           break;
       case PD_CONNECT_PE_READY_SNK_APDO:
            __pdpm->apdo_max_volt = 0;
            __pdpm->apdo_max_curr = 0;
           if (__pdpm->hrst_cnt < 5) {
               pr_err("en unlock\n");
               __pdpm->is_pps_en_unlock = true;
               __pdpm->psy_change_running = false;
           }
           break;
       default:
           break;
        }
    default:
        break;
    }
    if (__pdpm->usb_psy)
    	power_supply_changed(__pdpm->usb_psy);

    return NOTIFY_OK;
}

/************************wt API***************************/
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

	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_INPUT_CURRENT_SETTLED, aicr);
	if (ret < 0) {
		pr_err("set aicr fail(%d)\n", ret);
		return ret;
	}

	//set ichg
	/* 90% charging efficiency */
	ichg = (90 * pdpm->cp.vbus_volt * aicr / 100) / pdpm->cp.vbat_volt;
	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_CURRENT, ichg);
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
	if(!en) {
		xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_INPUT_CURRENT_SETTLED, 2000);
		xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_CURRENT, 100);
	} else {
		xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_ENABLE_CHARGER_TERM, true);
		xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_HZ, false);
	}
	pdpm->sw.charge_enabled = en;
	pr_info("en = %d\n", en);

    return 0;
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

static void usbpd_check_charger_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->usb_psy) {
		pdpm->usb_psy = power_supply_get_by_name("usb");
		if (!pdpm->usb_psy)
			pr_err("usb psy not found!\n");
	}
}

static void usbpd_pm_update_cp_status(struct usbpd_pm *pdpm)
{
    int ret;
	int val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_BATTERY_VOLTAGE, &val1);
	if (!ret)
        pdpm->cp.vbat_volt = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_BUS_VOLTAGE, &val1);
	if (!ret)
        pdpm->cp.vbus_volt = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_BUS_CURRENT, &val1);
	if (!ret)
        pdpm->cp.ibus_curr_cp = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_VBUS_ERROR_STATUS, &val1);
	if (!ret)
    {
        pdpm->cp.vbus_error_low = (val1 >> 5) & 0x01;
        pdpm->cp.vbus_error_high = (val1 >> 4) & 0x01;
    }

    pdpm->cp.ibus_curr = pdpm->cp.ibus_curr_cp ;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_BUS_TEMPERATURE, &val1);
	if (!ret)
        pdpm->cp.bus_temp = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_BATTERY_TEMPERATURE, &val1);
	if (!ret)
        pdpm->cp.bat_temp = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_DIE_TEMPERATURE, &val1);
	if (!ret)
        pdpm->cp.die_temp = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_BATTERY_PRESENT, &val1);
	if (!ret)
        pdpm->cp.batt_pres = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_VBUS_PRESENT, &val1);
	if (!ret)
        pdpm->cp.vbus_pres = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_CHARGING_ENABLED, &val1);
	if (!ret)
        pdpm->cp.charge_enabled = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_ALARM_STATUS, &val1);
	if (!ret) {
        pdpm->cp.bat_ovp_alarm = !!(val1 & BAT_OVP_ALARM_MASK);
        pdpm->cp.bat_ocp_alarm = !!(val1 & BAT_OCP_ALARM_MASK);
        pdpm->cp.bus_ovp_alarm = !!(val1 & BUS_OVP_ALARM_MASK);
        pdpm->cp.bus_ocp_alarm = !!(val1 & BUS_OCP_ALARM_MASK);
        pdpm->cp.bat_ucp_alarm = !!(val1 & BAT_UCP_ALARM_MASK);
        pdpm->cp.bat_therm_alarm = !!(val1 & BAT_THERM_ALARM_MASK);
        pdpm->cp.bus_therm_alarm = !!(val1 & BUS_THERM_ALARM_MASK);
        pdpm->cp.die_therm_alarm = !!(val1 & DIE_THERM_ALARM_MASK);
    }

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_FAULT_STATUS, &val1);
	if (!ret) {
        pdpm->cp.bat_ovp_fault = !!(val1 & BAT_OVP_FAULT_MASK);
        pdpm->cp.bat_ocp_fault = !!(val1 & BAT_OCP_FAULT_MASK);
        pdpm->cp.bus_ovp_fault = !!(val1 & BUS_OVP_FAULT_MASK);
        pdpm->cp.bus_ocp_fault = !!(val1 & BUS_OCP_FAULT_MASK);
        pdpm->cp.bat_therm_fault = !!(val1 & BAT_THERM_FAULT_MASK);
        pdpm->cp.bus_therm_fault = !!(val1 & BUS_THERM_FAULT_MASK);
        pdpm->cp.die_therm_fault = !!(val1 & DIE_THERM_FAULT_MASK);
    }
}

static void usbpd_pm_update_cp_sec_status(struct usbpd_pm *pdpm)
{
    int ret;
	int val1 = 0;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_SLAVE,
			CHARGE_PUMP_LN_BUS_CURRENT, &val1);
	if (!ret)
        pdpm->cp_sec.ibus_curr = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_SLAVE,
			CHARGE_PUMP_LN_CHARGING_ENABLED, &val1);
	if (!ret)
        pdpm->cp_sec.charge_enabled = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_SLAVE,
			CHARGE_PUMP_LN_ALARM_STATUS, &val1);

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_SLAVE,
			CHARGE_PUMP_LN_FAULT_STATUS, &val1);
}

static int usbpd_pm_enable_cp(struct usbpd_pm *pdpm, bool enable)
{
    int ret, val1;

    val1 = enable;
	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_CHARGING_ENABLED, val1);

    return ret;
}

static int usbpd_pm_enable_cp_sec(struct usbpd_pm *pdpm, bool enable)
{
    int ret, val1;

    val1 = enable;
	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, CP_SLAVE,
			CHARGE_PUMP_LN_CHARGING_ENABLED, val1);

    return ret;
}

static int usbpd_pm_check_cp_enabled(struct usbpd_pm *pdpm)
{
    int ret, val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_CHARGING_ENABLED, &val1);
    if (!ret)
        pdpm->cp.charge_enabled = !!val1;

    return ret;
}

static int usbpd_pm_check_cp_sec_enabled(struct usbpd_pm *pdpm)
{
    int ret, val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_SLAVE,
			CHARGE_PUMP_LN_CHARGING_ENABLED, &val1);
    if (!ret)
        pdpm->cp_sec.charge_enabled = !!val1;

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
            POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &val);
    if (!ret)
    {
        if(val.intval == 100000)
			//pdpm->sw.charge_enabled = false;
            pr_err("usbpd_pm_check_sw_enabled : %d\n", val.intval);
        else
			//pdpm->sw.charge_enabled = true;
            pr_err("usbpd_pm_check_sw_enabled : %d\n", val.intval);
    }

    return ret;
}

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
		pdpm->apdo_selected_pdo, pdpm->apdo_max_volt, pdpm->apdo_max_curr);
    else
        pr_notice("Not qualified PPS adapter\n");
}

static int usbpd_update_ibat_curr(struct usbpd_pm *pdpm)
{
	int ret, val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_CURRENT_NOW, &val1);
	if (!ret)
        pdpm->cp.ibat_curr = -(int)(val1/1000);

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_VOLTAGE_NOW, &val1);
	if (!ret)
        pdpm->cp.vbat_volt = (int)(val1/1000);

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_CYCLE_COUNT, &val1);
	if (!ret)
		pdpm->cp.battery_cycle = val1;

	return ret;
}

#define CHG_CUR_VOLT_NORMAL     4480
#define CHG_CUR_VOLT1           4250
#define CHG_CUR_VOLT2           4300
#define CHG_CUR_VOLT3           4480
#define CHG_CUR_VOLT4           4530
#define CHG_VOLT1_MAX_CUR       12400
#define CHG_VOLT2_MAX_CUR       7920
#define CHG_VOLT3_MAX_CUR       6600
#define CHG_VOLT4_MAX_CUR       5280
#define CHG_VOLT5_MAX_CUR       3080

#define CHG_BAT_TEMP_MIN      50
#define CHG_BAT_TEMP_10      100
#define CHG_BAT_TEMP_15      150
#define CHG_BAT_TEMP_45      450
#define CHG_BAT_TEMP_MAX      480
static int bat_step(struct usbpd_pm *pdpm, int cur) {
	int step = 0;
	int step_cur = 100;
/*
	if (pdpm->request_current <= 3000)
		step_cur = 100;
	else if(pdpm->request_current <= 5000 && pdpm->request_current > 3000)
		step_cur = 300;
	else
		step_cur = 100;

	if (pdpm->cp.ibat_curr < 2500)
		step_cur = 100;
*/
	if (pdpm->cp.ibat_curr < cur - step_cur)
		step = pm_config.fc2_steps;
	else if (pdpm->cp.ibat_curr > cur + 50)
		step = -pm_config.fc2_steps;

	return step;
}

static int battery_sw_jeita(struct usbpd_pm *pdpm)
{
	int step_ibat = 1;
	int step_vbat = 0;
	int bat_temp = 0;
	int step_therm = 0;
	int therm_curr = 0;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_TEMP, &bat_temp);
	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_THERM_CURR, &therm_curr);

	if (bat_temp >= CHG_BAT_TEMP_MIN && bat_temp < CHG_BAT_TEMP_MAX) {
		pdpm->pps_temp_flag = true;
		if (pdpm->cp.vbat_volt < CHG_CUR_VOLT1) {
			step_vbat = bat_step(pdpm, CHG_VOLT1_MAX_CUR);
			pdpm->cp.set_ibat_cur = CHG_VOLT1_MAX_CUR;
		} else if (pdpm->cp.vbat_volt >= CHG_CUR_VOLT1 && pdpm->cp.vbat_volt < CHG_CUR_VOLT2) {
			pdpm->cp.set_ibat_cur = CHG_VOLT2_MAX_CUR;
			step_vbat = bat_step(pdpm, CHG_VOLT2_MAX_CUR);
		} else if (pdpm->cp.vbat_volt >= CHG_CUR_VOLT2 && pdpm->cp.vbat_volt < CHG_CUR_VOLT3) {
			step_vbat = bat_step(pdpm, CHG_VOLT3_MAX_CUR);
			pdpm->cp.set_ibat_cur = CHG_VOLT3_MAX_CUR;
		} else {
			step_vbat = bat_step(pdpm, CHG_VOLT4_MAX_CUR);
			pdpm->cp.set_ibat_cur = CHG_VOLT4_MAX_CUR;
		}

		if(bat_temp < CHG_BAT_TEMP_10) {
			step_vbat = bat_step(pdpm, CHG_VOLT5_MAX_CUR);
			pdpm->cp.set_ibat_cur = CHG_VOLT5_MAX_CUR;
			if(pdpm->cp.vbat_volt >= CHG_CUR_VOLT_NORMAL)
				step_vbat = -1;
		}

		if(bat_temp < CHG_BAT_TEMP_15 && bat_temp >= CHG_BAT_TEMP_10) {
			step_vbat = bat_step(pdpm, CHG_VOLT4_MAX_CUR);
			pdpm->cp.set_ibat_cur = CHG_VOLT4_MAX_CUR;
			if(pdpm->cp.vbat_volt >= CHG_CUR_VOLT_NORMAL)
				step_vbat = -1;
		}

		if(pdpm->cp.vbat_volt >= CHG_CUR_VOLT4)
			step_vbat = -1;

		if(pdpm->cp.vbat_volt >= (pdpm->voltage_max - 10) && (pdpm->voltage_max > 4400))
			step_vbat = -1;

		if (pdpm->cp.set_ibat_cur >= therm_curr)
			pdpm->cp.set_ibat_cur = therm_curr;

		step_therm = bat_step(pdpm, therm_curr);

		step_ibat = min(step_therm, step_ibat);
		if (therm_curr < 2000)
			pdpm->pps_temp_flag = false;
	} else {
		pdpm->pps_temp_flag = false;
	}
	pr_err("temp %d pdpm->cp.ibus_curr_sw %d step_ibat %d, step_vbat %d, therm_curr %d, cycle %d\n",
		bat_temp, pdpm->cp.ibus_curr_sw, step_ibat, step_vbat, therm_curr, pdpm->cp.battery_cycle);
	return min(step_vbat, step_ibat);
}

static int usbpd_update_apdo_data(struct usbpd_pm *pdpm)
{
	int ret, val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, PD_PHY,
					PD_APDO_VOLT_MAX, &val1);
	if (!ret)
        pdpm->apdo_max_volt = val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, PD_PHY,
					PD_APDO_CURR_MAX, &val1);
	if (!ret)
        pdpm->apdo_max_curr = val1;

	return ret;
}

#define TAPER_TIMEOUT	(5000 / PM_WORK_RUN_INTERVAL)
#define IBUS_CHANGE_TIMEOUT  (500 / PM_WORK_RUN_INTERVAL)
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
	int step_jeita = 0;

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


    /* battery charge current loop*/
    if (pdpm->cp.ibat_curr < pm_config.bat_curr_lp_lmt )
        step_ibat = pm_config.fc2_steps;
    else if (pdpm->cp.ibat_curr > pm_config.bat_curr_lp_lmt + 100)
        step_ibat = -pm_config.fc2_steps;


    /* bus current loop*/
    ibus_total = pdpm->cp.ibus_curr;

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
    if (pdpm->cp.bat_ocp_alarm /*|| pdpm->cp.bat_ovp_alarm */
        || pdpm->cp.bus_ocp_alarm || pdpm->cp.bus_ovp_alarm
        /*|| pdpm->cp.tbat_temp > 60
            || pdpm->cp.tbus_temp > 50*/)
        hw_ctrl_steps = -pm_config.fc2_steps;
    else
        hw_ctrl_steps = pm_config.fc2_steps;

    /* check if cp disabled due to other reason*/
    usbpd_pm_check_cp_enabled(pdpm);
    if (pm_config.cp_sec_enable) {
        usbpd_pm_check_cp_sec_enabled(pdpm);
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
    } else if ((!pdpm->cp.charge_enabled && (pdpm->cp.vbus_error_low
                || pdpm->cp.vbus_error_high)) || (pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled && !pdpm->cp_sec_stopped)) {
        pr_notice("cp.charge_enabled:%d  %d  %d\n",
                pdpm->cp.charge_enabled, pdpm->cp.vbus_error_low, pdpm->cp.vbus_error_high);
	pr_notice("cp_sec.charge_enabled:%d  %d  %d\n",
                pdpm->cp_sec.charge_enabled, pm_config.cp_sec_enable, pdpm->cp_sec_stopped);
        return PM_ALGO_RET_CHG_DISABLED;
    }

    if(pdpm->cp.batt_temp < 150)
        pdpm->cp.batt_volt_max = pm_config.bat_volt_lp_lmt - 22;
    else
	    pdpm->cp.batt_volt_max = pm_config.bat_volt_lp_lmt;
    /* charge pump taper charge */
    if (pdpm->cp.vbat_volt > pdpm->cp.batt_volt_max - 50
            && pdpm->cp.ibat_curr < pm_config.fc2_taper_current) {
        if (fc2_taper_timer++ > TAPER_TIMEOUT) {
            pr_notice("charge pump taper charging done\n");
            fc2_taper_timer = 0;
            return PM_ALGO_RET_TAPER_DONE;
        }
    } else {
        fc2_taper_timer = 0;
    }
    /* TODO: customer can add hook here to check system level thermal mitigation*/

	step_jeita = battery_sw_jeita(pdpm);
	sw_ctrl_steps = min(sw_ctrl_steps, step_jeita);

    steps = min(sw_ctrl_steps, hw_ctrl_steps);

    pr_err(">>>>>>%d %d %d sw %d hw %d all %d\n",
            step_vbat, step_ibat, step_ibus, sw_ctrl_steps, hw_ctrl_steps, steps);

    pdpm->request_voltage += steps * 20;

    if (pdpm->request_voltage > 11000)
        pdpm->request_voltage = 11000;

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

static int usbpd_vol_exit(struct usbpd_pm *pdpm)
{
	int ads_vol;
	int tmp;
	int i;

	if((pdpm->cp.vbus_volt > 7000) && (pdpm->cp.vbus_volt < 11000 )) {
		for(i = 0; i < 6; i++) {
			tmp = pdpm->cp.vbus_volt_old - pdpm->cp.vbus_volt;
			if(tmp < 0)
				ads_vol = 0 - tmp;
			else
				ads_vol = tmp;
			pr_err("pdpm->cp.vbus_volt %d, vbus_old %d, ads_vol %d\n",
				pdpm->cp.vbus_volt, pdpm->cp.vbus_volt_old, ads_vol);
			if(ads_vol > 500 && ads_vol < 1200) {
				msleep(5);
				xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
							CHARGE_PUMP_LN_BUS_VOLTAGE, &pdpm->cp.vbus_volt);
				pr_err("USBPD  exit pd pdpm->cp.vbus_volt %d, i = %d\n", pdpm->cp.vbus_volt, i);
				if(i == 5 && pdpm->cp.batt_temp > 70)
					usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
				if(pdpm->cp.batt_temp > 50 && pdpm->cp.batt_temp <= 60)
					pdpm->cp.vbus_volt_old = pdpm->cp.vbus_volt;
			} else {
				pdpm->cp.vbus_volt_old = pdpm->cp.vbus_volt;
				break;
			}
		}
	} else {
		pdpm->cp.vbus_volt_old = 0;
	}

	return 0;
}

static int usbpd_pm_sm(struct usbpd_pm *pdpm)
{
    int ret;
    int rc = 0;
    static int tune_vbus_retry;
    static bool stop_sw;
    static bool recover;
    int bat_temp;
    int therm_curr = 0;
    union power_supply_propval val = {0,};

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_TEMP, &bat_temp);
	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_THERM_CURR, &therm_curr);
	pdpm->cp.batt_temp = bat_temp;

    if(pdpm->sw_psy) {
        ret = power_supply_get_property(pdpm->sw_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
        if(val.intval >= 98 || (therm_curr < 2000))
            pdpm->pps_leave = true;
        else
            pdpm->pps_leave = false;
		ret = power_supply_get_property(pdpm->sw_psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
		if(val.intval / 1000 > 4400)
			pdpm->voltage_max = val.intval / 1000;
		else
			pdpm->voltage_max = 0;
	} else {
		pdpm->sw_psy = power_supply_get_by_name("battery");
	}

    if(!pdpm->pps_leave) {
        pr_err("state phase :%d, vbus_vol %d, vbat_vol %d  vout %d, volatge_max %d pdpm->apdo_max_curr %d\n",
		    pdpm->state, pdpm->cp.vbus_volt, pdpm->cp.vbat_volt, pdpm->cp.vout_volt, pdpm->voltage_max, pdpm->apdo_max_curr);
        pr_err("ibus_curr %d  ibus_curr_m %d, ibus_curr_s %d  ibat_curr %d\n",
		    pdpm->cp.ibus_curr + pdpm->cp_sec.ibus_curr, pdpm->cp.ibus_curr_cp, pdpm->cp_sec.ibus_curr, pdpm->cp.ibat_curr);
        usbpd_vol_exit(pdpm);
    }

    switch (pdpm->state) {
    case PD_PM_STATE_ENTRY:
        stop_sw = false;
        recover = false;

        if (pdpm->cp.vbat_volt < pm_config.min_vbat_for_cp) {
            pr_notice("batt_volt-%d, waiting...\n", pdpm->cp.vbat_volt);
        } else if (bat_temp >= CHG_BAT_TEMP_MAX || bat_temp < CHG_BAT_TEMP_MIN) {
			pr_notice("bat_temp-%d is too high for cp", bat_temp);
        } else if (pdpm->pps_leave) {
            pr_notice("bat capacity or therm_curr or pps_leave");
        } else if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 100) {
            pr_notice("batt_volt-%d is too high for cp,\
                    charging with switch charger\n",
                    pdpm->cp.vbat_volt);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
        } else {
            pr_notice("batt_volt-%d is ok, start flash charging\n",
                    pdpm->cp.vbat_volt);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY);
        }
        break;

    case PD_PM_STATE_FC2_ENTRY:
        pr_err("PD_PM_STATE_FC2_ENTRY pdpm->sw.charge_enabled %d\n", pdpm->sw.charge_enabled);
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
        usbpd_pm_enable_sw(pdpm, false);
        if (pm_config.cp_sec_enable)
            pdpm->request_voltage = pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP * 2;
        else
            pdpm->request_voltage = pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP;

        pdpm->request_current = min(pdpm->apdo_max_curr, pm_config.bus_curr_lp_lmt);

		usbpd_select_pdo(pdpm,pdpm->request_voltage,
						 pdpm->request_current);
        pr_err("request_voltage:%d, request_current:%d\n",
                pdpm->request_voltage, pdpm->request_current);

        usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);
        tune_vbus_retry = 0;
        break;

    case PD_PM_STATE_FC2_ENTRY_2:
        usbpd_pm_enable_sw(pdpm, false);
        pr_err("tune_vbus_retry %d\n", tune_vbus_retry);
        if (pdpm->cp.vbus_error_low || pdpm->cp.vbus_volt < pdpm->cp.vbat_volt * 207/100) {
            tune_vbus_retry++;
            pdpm->request_voltage += 20;
			usbpd_select_pdo(pdpm,pdpm->request_voltage,
						 pdpm->request_current);
            pr_err("request_voltage:%d, request_current:%d\n",
                    pdpm->request_voltage, pdpm->request_current);
        } else if (pdpm->cp.vbus_error_high || pdpm->cp.vbus_volt > pdpm->cp.vbat_volt * 230/100) {
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

        if (tune_vbus_retry > 30) {
            pr_notice("Failed to tune adapter volt into valid range, \
                    charge with switching charger\n");
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
        }
        break;
    case PD_PM_STATE_FC2_ENTRY_3:
		pr_err("PD_PM_STATE_FC2_ENTRY3 pdpm->cp.charge_enabled %d, pm_config.cp_sec_enable %d, pdpm->cp.charge_enabled %d, pdpm->cp_sec.charge_enabled %d\n",
               pdpm->cp.charge_enabled, pm_config.cp_sec_enable, pdpm->cp.charge_enabled, pdpm->cp_sec.charge_enabled);
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
                usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_TUNE);
                ibus_lmt_change_timer = 0;
                fc2_taper_timer = 0;
                }
        }
		pr_err("PD_PM_STATE_FC2_ENTRY3 pdpm->cp.charge_enabled %d, pm_config.cp_sec_enable %d, pdpm->cp_sec.charge_enabled %d\n",
				pdpm->cp.charge_enabled, pm_config.cp_sec_enable, pdpm->cp_sec.charge_enabled);
        break;

    case PD_PM_STATE_FC2_TUNE:
        if (!pdpm->cp.charge_enabled) {
            usbpd_pm_enable_cp(pdpm, true);
            usbpd_pm_check_cp_enabled(pdpm);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_3);
        }
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
            usbpd_select_pdo(pdpm,pdpm->request_voltage, pdpm->request_current);
            pdpm->request_voltage_old = pdpm->request_voltage;
            pr_err("request_voltage:%d, request_current:%d, pdpm->request_voltage_old %d\n",
                    pdpm->request_voltage, pdpm->request_current, pdpm->request_voltage_old);
        }

		if (!pdpm->pps_temp_flag || pdpm->pps_leave) {
			pr_notice("temp high or lower,stop charging\n");
			stop_sw = false;
			pdpm->sw.charge_enabled = false;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		}

        /*stop second charge pump if either of ibus is lower than 1000ma during CV*/
		if(pdpm->cp.batt_temp < 150)
			pdpm->cp.batt_volt_max = pm_config.bat_volt_lp_lmt - 22;
		else
			pdpm->cp.batt_volt_max = pm_config.bat_volt_lp_lmt;

        if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled
				&& pdpm->cp.vbat_volt > pdpm->cp.batt_volt_max - 50
				&& (pdpm->cp.ibus_curr < 750 || pdpm->cp_sec.ibus_curr < 750)) {
            pr_notice("second cp is disabled due to ibus < 750mA\n");
            usbpd_pm_enable_cp_sec(pdpm, false);
            usbpd_pm_check_cp_sec_enabled(pdpm);
            pdpm->cp_sec_stopped = true;
        }
        break;

    case PD_PM_STATE_FC2_EXIT:
        pdpm->psy_change_running = false;
        pdpm->cp.vbus_volt_old = 0;
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

        pr_err("sw state %d, %d\n", stop_sw, pdpm->sw.charge_enabled);
        if (stop_sw && pdpm->sw.charge_enabled) {
            usbpd_pm_enable_sw(pdpm, false);
            usbpd_pm_set_swchg_cap(pdpm, 3000);
        } else if (!stop_sw && !pdpm->sw.charge_enabled) {
            usbpd_pm_enable_sw(pdpm, true);
            usbpd_pm_set_swchg_cap(pdpm, 3000);
        }

        if (recover)
            usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
        else {
	    usbpd_pps_enable_charging(pdpm,false,5000,3000);
            rc = 1;
        }
		if (!pdpm->pps_temp_flag || pdpm->pps_leave) {
			pr_notice("temp is high or low waiting...\n");
			usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
			pdpm->pd_active = false;
			schedule_work(&pdpm->usb_psy_change_work);
		}
		break;
	}

	return rc;
}

static void usbpd_pm_workfunc(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, pm_work.work);
	int time;

    if(!pdpm->pps_leave) {
	    usbpd_pm_update_cp_status(pdpm);
	    usbpd_pm_update_cp_sec_status(pdpm);
	    usbpd_update_ibat_curr(pdpm);
	    usbpd_update_apdo_data(pdpm);
    }

	if ((pdpm->cp.set_ibat_cur - pdpm->cp.ibat_curr > 2000) && (pdpm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - 50))
		time = 300;
	else if ((pdpm->cp.set_ibat_cur - pdpm->cp.ibat_curr <= 2000)
		&& (pdpm->cp.set_ibat_cur - pdpm->cp.ibat_curr > 1500)
		&& (pdpm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - 50))
		time = 500;
	else if(pdpm->cp.ibat_curr > 10800)
		time = 1000;
	else
		time = PM_WORK_RUN_INTERVAL;

	if (!usbpd_pm_sm(pdpm) && pdpm->pd_active)
		schedule_delayed_work(&pdpm->pm_work, msecs_to_jiffies(time));

	return;
}

static void usbpd_workfunc(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, pd_work.work);

	if (pdpm->cp.vbus_volt <= 7000 && !pdpm->is_pps_en_unlock) {
		if (pdpm->tcpc)
			tcpm_dpm_pd_request(pdpm->tcpc, 9000, 2000, NULL);
		 pr_err("pd request 9V/2A\n");
	}

	return;
}

static int usbpd_psy_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data);
static void tcp_notify_workfunc(struct work_struct *work)
{
    int ret = 0;
    static int reg_flag = 0;
    struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, tcp_work.work);

    usbpd_check_tcpc(pdpm);
    if (pdpm->tcpc && !reg_flag) {
        /* register tcp notifier callback */
        pdpm->tcp_nb.notifier_call = pca_pps_tcp_notifier_call;
        ret = register_tcp_dev_notifier(pdpm->tcpc, &pdpm->tcp_nb,
                                        TCP_NOTIFY_TYPE_USB);
        if (ret < 0) {
            pr_err("register tcpc notifier fail\n");
            return;
        }
        pdpm->nb.notifier_call = usbpd_psy_notifier_cb;
		power_supply_reg_notifier(&pdpm->nb);
        reg_flag = 1;
        return;
    } else {
        pr_err("register tcpc notifier fail\n");
        schedule_delayed_work(&pdpm->tcp_work, msecs_to_jiffies(2000));
   }

	return;
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
    pdpm->pps_leave = false;
    pdpm->apdo_selected_pdo = 0;
    pdpm->psy_change_running = false;
    usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);

	return;
}

static void usbpd_pd_contact(struct usbpd_pm *pdpm, bool connected)
{
    pdpm->pd_active = connected;
    pr_err("[SC manager] >> pd_active %d\n", pdpm->pd_active);
    if (connected) {
        msleep(10);
        usbpd_pm_evaluate_src_caps(pdpm);
        pr_err("[SC manager] >>start cp charging pps support %d\n", pdpm->pps_supported);
        if (pdpm->pps_supported)
            schedule_delayed_work(&pdpm->pm_work, 0);
    } else {
        usbpd_pm_disconnect(pdpm);
    }

	return;
}

static void usb_psy_change_work(struct work_struct *work)
{
    int ret = 0;
    union power_supply_propval propval;
    struct usbpd_pm *pdpm = container_of(work,
                      struct usbpd_pm, usb_psy_change_work);

    pr_err("[SC manager] >> usb change work\n");
    ret = power_supply_get_property(pdpm->usb_psy,
					POWER_SUPPLY_PROP_ONLINE,
					&propval);

   pr_err("[SC manager] >> pd_active %d,  propval.intval %d\n",
			pdpm->pd_active, propval.intval);

   if (!pdpm->pd_active && pdpm->is_pps_en_unlock) {
		//ret = xm_battmngr_write_iio_prop(g_battmngr_iio, CP_MASTER,
		//			CHARGE_PUMP_LN_PRESENT, true);
		//if(pm_config.cp_sec_enable)
		//	ret = xm_battmngr_write_iio_prop(g_battmngr_iio, CP_SLAVE,
		//			CHARGE_PUMP_LN_PRESENT, true);
		usbpd_pd_contact(pdpm, true);
   } else if (pdpm->pd_active && !pdpm->is_pps_en_unlock)
		usbpd_pd_contact(pdpm, false);

	return;
}

static int usbpd_check_plugout(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

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

static int usbpd_psy_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
    struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
    struct power_supply *psy = data;
    unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
        return NOTIFY_OK;

	usbpd_check_charger_psy(pdpm);
	usbpd_check_tcpc(pdpm);
    if (!pdpm->tcpc)
        return NOTIFY_OK;

    usbpd_check_plugout(pdpm);

	if (psy == pdpm->usb_psy) {
		spin_lock_irqsave(&pdpm->psy_change_lock, flags);
        pr_err("[SC manager] >>>pdpm->psy_change_running : %d\n", pdpm->psy_change_running);
        if (!pdpm->psy_change_running) {
            pdpm->psy_change_running = true;
            schedule_work(&pdpm->usb_psy_change_work);
        }
        spin_unlock_irqrestore(&pdpm->psy_change_lock, flags);
    }

    return NOTIFY_OK;
}

static int usbpd_pm_probe(struct platform_device *pdev)
{
	struct usbpd_pm *pdpm;
	static int probe_cnt = 0;
	int ret = 0;

	pr_err("%s probe_cnt = %d\n", __func__, ++probe_cnt);

	if (!pdev->dev.of_node)
		return -ENODEV;

	if (pdev->dev.of_node) {
		pdpm = devm_kzalloc(&pdev->dev,
			sizeof(struct usbpd_pm), GFP_KERNEL);
		if (!pdpm) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	} else {
		return -ENODEV;
	}

	pdpm->dev = &pdev->dev;
	pdpm->pdev = pdev;
	platform_set_drvdata(pdev, pdpm);
	__pdpm = pdpm;

	if (!g_battmngr) {
		pr_err("%s: g_battmngr not ready, defer\n", __func__);
		ret = -EPROBE_DEFER;
		msleep(100);
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			goto err_g_battmngr;
	}

    INIT_WORK(&pdpm->usb_psy_change_work, usb_psy_change_work);
    spin_lock_init(&pdpm->psy_change_lock);

    usbpd_check_charger_psy(pdpm);
    usbpd_check_tcpc(pdpm);

    INIT_DELAYED_WORK(&pdpm->pm_work, usbpd_pm_workfunc);
    INIT_DELAYED_WORK(&pdpm->pd_work, usbpd_workfunc);
    INIT_DELAYED_WORK(&pdpm->tcp_work, tcp_notify_workfunc);
    schedule_delayed_work(&pdpm->tcp_work, msecs_to_jiffies(1000));

    pr_err("%s: End!\n", __func__);

out:
	platform_set_drvdata(pdev, pdpm);
	pr_err("%s %s!!\n", __func__, ret == -EPROBE_DEFER ?
				"Over probe cnt max" : "OK");
	return 0;

err_g_battmngr:
	return ret;
}

static int usbpd_pm_remove(struct platform_device *pdev)
{
	struct usbpd_pm *pdpm = platform_get_drvdata(pdev);

	power_supply_unreg_notifier(&__pdpm->nb);
	cancel_delayed_work(&__pdpm->pm_work);
	cancel_work_sync(&__pdpm->usb_psy_change_work);
	kfree(pdpm);
	return 0;
}

static void usbpd_pm_shutdown(struct platform_device *pdev)
{

	struct usbpd_pm *pdpm = platform_get_drvdata(pdev);

	pr_err("%s usbpd_pm_shutdown\n", __func__);
	if(!pdpm)
		return;
	pdpm->shutdown_flag = true;

	return;
}

static const struct of_device_id usbpd_pm_dt_match[] = {
	{.compatible = "xiaomi,pd_cp_manager"},
	{},
};

static struct platform_driver usbpd_pm_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "pd_cp_manager",
		.of_match_table = usbpd_pm_dt_match,
	},
	.probe = usbpd_pm_probe,
	.remove = usbpd_pm_remove,
	.shutdown = usbpd_pm_shutdown,
};

static int __init usbpd_pm_init(void)
{
	platform_driver_register(&usbpd_pm_driver);
	pr_err("usbpd_pm_init end\n");
    return 0;
};
late_initcall(usbpd_pm_init);

static void __exit usbpd_pm_exit(void)
{
	platform_driver_unregister(&usbpd_pm_driver);
}
module_exit(usbpd_pm_exit);

MODULE_DESCRIPTION("Xiaomi pd Manager");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("getian@xiaomi.com");

