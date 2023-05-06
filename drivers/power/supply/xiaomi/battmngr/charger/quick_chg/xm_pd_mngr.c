
#define pr_fmt(fmt)	"[USBPD-PM]: %s: " fmt, __func__

#include "inc/xm_pd_mngr.h"

static struct pdpm_config pm_config = {
	.bat_volt_lp_lmt		= BAT_VOLT_LOOP_LMT,
	.bat_curr_lp_lmt		= BAT_CURR_LOOP_LMT + 1000,
	.bus_volt_lp_lmt		= BUS_VOLT_LOOP_LMT,
	.bus_curr_lp_lmt		= BAT_CURR_LOOP_LMT >> 1,
	.bus_curr_compensate	= 0,

	.fc2_taper_current		= TAPER_DONE_NORMAL_MA,
	.fc2_steps			= 1,

	.min_adapter_volt_required	= 10000,
	.min_adapter_curr_required	= 2000,

	.min_vbat_for_cp		= 3500,

	.cp_sec_enable			= true,
	.fc2_disable_sw			= true,
};

static struct usbpd_pm *__pdpm;
static int fc2_taper_timer;
static int ibus_lmt_change_timer;
extern struct rt1711_chip *g_tcpc_rt1711h;
extern struct xm_pd_adapter_info *g_xm_pd_adapter;

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
	if (!pdpm->batt_psy) {
		pdpm->batt_psy = power_supply_get_by_name("battery");
		if (!pdpm->batt_psy)
			pr_err("batt psy not found!\n");
	}
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

static int usbpd_get_effective_fcc_val(struct usbpd_pm *pdpm)
{
	int effective_fcc_val = 0;

	if (!pdpm->fcc_votable)
		pdpm->fcc_votable = find_votable("FCC");

	if (!pdpm->fcc_votable)
		return -EINVAL;

	effective_fcc_val = get_effective_result(pdpm->fcc_votable);
	effective_fcc_val = effective_fcc_val / 1000;
	pr_info("effective_fcc_val: %d\n", effective_fcc_val);
	return effective_fcc_val;
}

/* get thermal level from battery power supply property */
static int pd_get_batt_current_thermal_level(struct usbpd_pm *pdpm, int *level)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	usbpd_check_batt_psy(pdpm);

	if (!pdpm->batt_psy)
		return -ENODEV;

	rc = power_supply_get_property(pdpm->batt_psy,
				POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &pval);
	if (rc < 0) {
		pr_info("Couldn't get batt_current_thermal_level:%d\n", rc);
		return rc;
	}

	if (g_battmngr_noti->misc_msg.disable_thermal)
		*level = 0;
	else
		*level = pval.intval;

	return rc;
}

static int pd_get_batt_capacity(struct usbpd_pm *pdpm, int *capacity)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	usbpd_check_batt_psy(pdpm);

	if (!pdpm->batt_psy)
		return -ENODEV;

	rc = power_supply_get_property(pdpm->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		pr_info("Couldn't get batt_capacity:%d\n", rc);
		return rc;
	}

	*capacity = pval.intval;
	return rc;
}

static void pd_bq_check_ibus_to_enable_dual_bq(struct usbpd_pm *pdpm, int ibus_ma)
{

	int capacity = 0;

	pd_get_batt_capacity(pdpm, &capacity);
	if (ibus_ma <= IBUS_THRESHOLD_MA_FOR_DUAL_BQ_LN8000 && !pdpm->no_need_en_slave_bq
			&& (pdpm->slave_bq_disabled_check_count < IBUS_THR_TO_CLOSE_SLAVE_COUNT_MAX)) {
		pdpm->slave_bq_disabled_check_count++;
		if (pdpm->slave_bq_disabled_check_count >= IBUS_THR_TO_CLOSE_SLAVE_COUNT_MAX) {
			pdpm->no_need_en_slave_bq = true;
			/* disable slave bq due to total low ibus to avoid bq ucp */
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
			&& (ibus_ma > (IBUS_THRESHOLD_MA_FOR_DUAL_BQ_LN8000 + IBUS_THR_MA_HYS_FOR_DUAL_BQ))) {
		if (!pdpm->cp_sec.charge_enabled) {
			pdpm->no_need_en_slave_bq = false;
			/* re-enable slave bq due to master ibus increase above threshold + hys */
			pr_err("ibus increase above threshold, re-enable slave bq now\n");
			usbpd_pm_enable_cp_sec(pdpm, true);
			msleep(50);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}
	} else {
		pdpm->slave_bq_disabled_check_count = 0;
	}
}

/* determine whether to disable cp according to jeita status */
static bool pd_disable_cp_by_jeita_status(struct usbpd_pm *pdpm)
{
	union power_supply_propval pval = {0,};
	int batt_temp = 0, input_suspend = 0;
	int rc;

	if (!pdpm->batt_psy)
		return false;

	if (!pdpm->input_suspend_votable)
		pdpm->input_suspend_votable = find_votable("INPUT_SUSPEND");

	if (!pdpm->input_suspend_votable)
		return -EINVAL;

	input_suspend = (get_client_vote(pdpm->input_suspend_votable, USER_VOTER) == 1);
	pr_info("input_suspend: %d\n", input_suspend);

	/* is input suspend is set true, do not allow bq quick charging */
	if (input_suspend)
		return true;

	rc = power_supply_get_property(pdpm->batt_psy,
				POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0) {
		pr_info("Couldn't get batt temp prop:%d\n", rc);
		return false;
	}

	batt_temp = pval.intval;

	if (batt_temp >= pdpm->battery_warm_th && !pdpm->jeita_triggered) {
		pdpm->jeita_triggered = true;
		return true;
	} else if (batt_temp <= JEITA_COOL_NOT_ALLOW_CP_THR
			&& !pdpm->jeita_triggered) {
		pdpm->jeita_triggered = true;
		return true;
	} else if (batt_temp <= (pdpm->battery_warm_th - JEITA_HYSTERESIS)
			&& (batt_temp >= (JEITA_COOL_NOT_ALLOW_CP_THR + JEITA_HYSTERESIS))
			&& pdpm->jeita_triggered) {
		pdpm->jeita_triggered = false;
		return false;
	} else {
		return pdpm->jeita_triggered;
	}
}

/* get bq27z561 digest verified to enable or disabled */
static bool pd_get_fg_digest_verified(struct usbpd_pm *pdpm)
{
	int rc, val1;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
					BATT_FG_BATTERY_AUTH, &val1);
	if (rc < 0) {
		pr_info("Couldn't get fg_digest_verified:%d\n", rc);
		return false;
	}

	pr_err("pd_get_fg_digest_verified: val1 %d\n", val1);

	if (val1 == 1)
		return true;
	else
		return false;
}

/* get bq27z561 chip ok*/
/* 
static bool pd_get_fg_chip_ok(struct usbpd_pm *pdpm)
{
	int rc, val1;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
					BATT_FG_CHIP_OK, &val1);
	if (rc < 0) {
		pr_info("Couldn't get chip ok:%d\n", rc);
		return false;
	}

	if (val1 == 1)
		return true;
	else
		return false;
}
*/

/* get fastcharge mode */
static bool pd_get_fastcharge_mode_enabled(struct usbpd_pm *pdpm)
{
	int rc, val1;

	rc = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
					BATT_FG_FASTCHARGE_MODE, &val1);
	if (rc < 0) {
		pr_info("Couldn't get fastcharge mode:%d\n", rc);
		return false;
	}

	if (val1 == 1)
		return true;
	else
		return false;
}

static void usbpd_pm_update_cp_status(struct usbpd_pm *pdpm)
{
	int ret, val1;

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
        pdpm->cp.ibus_curr = val1;

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

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_CURRENT_NOW, &val1);
	if (!ret) {
		if (pdpm->cp.vbus_pres)
			pdpm->cp.ibat_curr = -(val1 / 1000);
	}

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG,
			BATT_FG_VOLTAGE_NOW, &val1);
	if (!ret)
		pdpm->cp.fg_vbat_mv = val1 / 1000;
	else
		pr_err("Failed to read fg voltage now\n");

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

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_MASTER,
			CHARGE_PUMP_LN_REG_STATUS, &val1);
	if (!ret) {
		pdpm->cp.vbat_reg = !!(val1 & VBAT_REG_STATUS_MASK);
		pdpm->cp.ibat_reg = !!(val1 & IBAT_REG_STATUS_MASK);
	}
}

static void usbpd_pm_update_cp_sec_status(struct usbpd_pm *pdpm)
{
    int ret, val1;

	if (!pm_config.cp_sec_enable)
		return;

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

static int usbpd_pm_check_sec_batt_present(struct usbpd_pm *pdpm)
{
    int ret, val1;

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, CP_SLAVE,
			CHARGE_PUMP_LN_BATTERY_PRESENT, &val1);
    if (!ret)
		pdpm->cp_sec.batt_connecter_present = !!val1;
	pr_info("pdpm->cp_sec.batt_connecter_present:%d\n", pdpm->cp_sec.batt_connecter_present);

	return ret;
}

static int usbpd_pm_enable_sw(struct usbpd_pm *pdpm, bool en)
{
	int smart_batt_fv = 0, fv = 0;
	if (!pdpm->fcc_votable)
		pdpm->fcc_votable = find_votable("FCC");

	if (!pdpm->fv_votable)
		pdpm->fv_votable = find_votable("FV");

	if (!pdpm->usb_icl_votable)
		pdpm->usb_icl_votable = find_votable("ICL");

	if (!pdpm->smart_batt_votable)
		pdpm->smart_batt_votable = find_votable("SMART_BATT");

	if (!pdpm->fcc_votable || !pdpm->fv_votable || !pdpm->usb_icl_votable || !pdpm->smart_batt_votable)
		return -EINVAL;

	if(!en) {
		xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_ENABLE_CHARGER_TERM, en);
		vote(pdpm->fv_votable, FFC_DISABLE_CP_FV_VOTER, false, 0);

		g_battmngr_noti->mainchg_msg.sw_disable = true;
	} else {
		g_battmngr_noti->mainchg_msg.sw_disable = false;

		vote(pdpm->usb_icl_votable, MAIN_CHG_ICL_VOTER, true, MAIN_CHG_ICL);
		vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER, false, 0);
		vote(pdpm->fcc_votable, BQ_TAPER_CELL_HGIH_FCC_VOTER, false, 0);
		smart_batt_fv = get_client_vote(pdpm->smart_batt_votable, SMART_BATTERY_FV);
		if (smart_batt_fv == 15 || smart_batt_fv == 10)
			fv = FFC_DISABLE_CP_FV_D;
		else
			fv = FFC_DISABLE_CP_FV;
		vote(pdpm->fv_votable, FFC_DISABLE_CP_FV_VOTER, true, fv - smart_batt_fv * 1000);

		msleep(1000);
		xm_battmngr_write_iio_prop(g_battmngr_iio, MAIN_CHG,
			MAIN_CHARGER_ENABLE_CHARGER_TERM, en);
	}
	rerun_election(pdpm->fcc_votable);
	pr_info("en = %d, smart_batt_fv = %d\n", en, smart_batt_fv);

    return 0;
}

static int usbpd_pm_check_night_charging_enabled(struct usbpd_pm *pdpm)
{
	int val = 0;

	if (!pdpm->fcc_votable)
		pdpm->fcc_votable = find_votable("FCC");

	if (!pdpm->fcc_votable)
		return -EINVAL;

	val = (get_client_vote(pdpm->fcc_votable, USER_VOTER) == 0);

	pdpm->sw.night_charging = !!val;

	return val;
}

static int usbpd_pm_check_sw_enabled(struct usbpd_pm *pdpm)
{
	pdpm->sw.charge_enabled = !g_battmngr_noti->mainchg_msg.sw_disable;

	return 0;
}

static void usbpd_pm_update_sw_status(struct usbpd_pm *pdpm)
{
	usbpd_pm_check_sw_enabled(pdpm);
}

/*******************************PD API******************************/
static inline int check_typec_attached_snk(struct tcpc_device *tcpc)
{
	if (tcpm_inquire_typec_attach_state(tcpc) != TYPEC_ATTACHED_SNK)
		return -EINVAL;

	return 0;
}

/*
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
*/

static bool usbpd_get_pps_status(struct usbpd_pm *pdpm)
{
	int ret;
	struct tcpm_power_cap_val apdo_cap = {0};
	u8 cap_idx = 0;
	int maxwatt = 0;

	pr_err("enter\n");

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

	/* apdo init */
	pdpm->apdo_max_volt = 0;
	pdpm->apdo_max_curr = 0;
	pdpm->apdo_maxwatt = 0;

	/* select TA boundary */
	while (1) {
		ret = tcpm_inquire_pd_source_apdo(pdpm->tcpc,
						  TCPM_POWER_CAP_APDO_TYPE_PPS,
						  &cap_idx, &apdo_cap);
		if (ret != TCP_DPM_RET_SUCCESS) {
			pr_err("inquire pd apdo fail(%d)\n", ret);
			break;
		}

		maxwatt = apdo_cap.max_mv * apdo_cap.ma;
		pr_err("cap_idx[%d], max: %d %d mv ~ %d mv, %d ma, pl: %d\n", cap_idx,
			 maxwatt, apdo_cap.min_mv, apdo_cap.max_mv, apdo_cap.ma,
			 apdo_cap.pwr_limit);

		if (apdo_cap.max_mv < pm_config.min_adapter_volt_required ||
		    apdo_cap.ma < pm_config.min_adapter_curr_required ||
		    apdo_cap.max_mv >= APDO_MAX_MV)
			continue;

		if (maxwatt > pdpm->apdo_maxwatt && maxwatt < APDO_MAX_WATT) {
			pdpm->apdo_max_volt = apdo_cap.max_mv;
			pdpm->apdo_max_curr = apdo_cap.ma;
			pdpm->apdo_maxwatt = maxwatt;

			pr_err("select potential cap_idx[%d]\n", cap_idx);
			pr_err("potential apdo_max_volt %d, apdo_max_curr %d, apdo_maxwatt %d\n",
				pdpm->apdo_max_volt, pdpm->apdo_max_curr, pdpm->apdo_maxwatt);

			ret = xm_battmngr_write_iio_prop(g_battmngr_iio, PD_PHY,
					PD_APDO_VOLT_MAX, pdpm->apdo_max_volt);
			if (ret < 0)
				pr_err("set APDO_MAX_VOLT fail(%d)\n", ret);

			ret = xm_battmngr_write_iio_prop(g_battmngr_iio, PD_PHY,
					PD_APDO_CURR_MAX, pdpm->apdo_max_curr);
			if (ret < 0)
				pr_err("set APDO_MAX_CURR fail(%d)\n", ret);
		}
	}

    if (!pdpm->apdo_maxwatt)
		return false;

    return true;
}

static int usbpd_select_pdo(struct usbpd_pm *pdpm, enum adapter_cap_type type, u32 mV, u32 mA)
{
	int ret, cnt = 0;

	if (check_typec_attached_snk(pdpm->tcpc) < 0)
		return -EINVAL;
	pr_err("%dmV, %dmA, adapter_cap_type:%d\n", mV, mA, type);

	if (!tcpm_inquire_pd_connected(pdpm->tcpc)) {
		pr_err("pd not connected\n");
		return -EINVAL;
	}

	do {
		if (type == XM_PD_APDO_START) {
			ret = tcpm_set_apdo_charging_policy(pdpm->tcpc,
				DPM_CHARGING_POLICY_PPS, mV, mA, NULL);
		} else if (type == XM_PD_APDO_END) {
			ret = tcpm_set_pd_charging_policy(pdpm->tcpc,
				DPM_CHARGING_POLICY_VSAFE5V, NULL);
		} else if (type == XM_PD_APDO) {
			ret = tcpm_dpm_pd_request(pdpm->tcpc, mV, mA, NULL);
		} else if (type == XM_PD_FIXED) {
			ret = tcpm_dpm_pd_request(pdpm->tcpc, mV, mA, NULL);
		}
		cnt++;
	} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

	if (ret != TCP_DPM_RET_SUCCESS)
		pr_err("fail(%d)\n", ret);

	if (ret == TCP_DPM_RET_DENIED_INVALID_REQUEST)
		usbpd_pm_evaluate_src_caps(pdpm);

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

static void usbpd_pm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
	bool retValue;

	retValue = usbpd_get_pps_status(pdpm);
	if (retValue)
		pdpm->pps_supported = true;
	else
		pdpm->pps_supported = false;


	if (pdpm->pps_supported)
		pr_err("PPS supported, max volt:%d, current:%d\n",
				pdpm->apdo_max_volt,
				pdpm->apdo_max_curr);
	else
		pr_err("Not qualified PPS adapter\n");
}

static void usbpd_fc2_exit_work(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					fc2_exit_work.work);

	while (pdpm->cp.vbus_volt > 6000) {
		if (!pdpm->fc2_exit_flag) {
			pr_info("fc2_exit_flag:false, break.\n");
			return;
		}

		pdpm->request_voltage -= 500;
		pr_info("request_voltage:%d.\n", pdpm->request_voltage);
		if (pdpm->request_voltage < 5500) {
			pr_info("request_voltage < 5.5V, break.\n");
			break;
		}
		usbpd_select_pdo(pdpm, XM_PD_APDO, pdpm->request_voltage, pdpm->request_current);
		msleep(500);
		usbpd_pm_update_cp_status(pdpm);
		pr_info("vbus_mv:%d.\n", pdpm->cp.vbus_volt);
	}

	pr_info("%s:select default 5V.\n", __func__);
	usbpd_select_pdo(pdpm, XM_PD_APDO_END, 5000, 3000);
	pdpm->fc2_exit_flag = false;
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
	int capacity = 0;
	int bq_taper_hys_mv = BQ_TAPER_HYS_MV;
	bool is_fastcharge_mode = false;
	bool unsupport_pps_status = false;
	static int curr_fcc_limit, curr_ibus_limit;
	static int pd_log_count;
	static int ibus_limit;
	static int last_thermal_level;

	is_fastcharge_mode = pd_get_fastcharge_mode_enabled(pdpm);
	if (is_fastcharge_mode) {
		pm_config.bat_volt_lp_lmt = pdpm->bat_volt_max;
		bq_taper_hys_mv = BQ_TAPER_HYS_MV;
		pm_config.fc2_taper_current = TAPER_DONE_FFC_MA_LN8000;
	} else {
		pm_config.bat_volt_lp_lmt = pdpm->non_ffc_bat_volt_max;
		bq_taper_hys_mv = NON_FFC_BQ_TAPER_HYS_MV;
		pm_config.fc2_taper_current = TAPER_DONE_NORMAL_MA;
	}

	pd_get_batt_capacity(pdpm, &capacity);
	/* if cell vol read from fuel gauge is higher than threshold, vote saft fcc to protect battery */
	if (is_fastcharge_mode) {
		if (pdpm->cp.fg_vbat_mv > pdpm->cell_vol_max_threshold_mv) {
			if (pdpm->over_cell_vol_max_count++ > CELL_VOLTAGE_MAX_COUNT_MAX) {
				pdpm->over_cell_vol_max_count = 0;
				effective_fcc_taper = usbpd_get_effective_fcc_val(pdpm);
				effective_fcc_taper -= BQ_TAPER_DECREASE_STEP_MA;
				pr_err("vcell is reached to max threshold, decrease fcc: %d mA\n",
							effective_fcc_taper);
				if (pdpm->fcc_votable) {
					if (effective_fcc_taper >= 2000)
						vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER,
							true, effective_fcc_taper * 1000);
				}
			}
		} else {
			pdpm->over_cell_vol_max_count = 0;
		}
	}

	effective_fcc_val = usbpd_get_effective_fcc_val(pdpm);

	if (effective_fcc_val > 0) {
		curr_fcc_limit = min(pm_config.bat_curr_lp_lmt, effective_fcc_val);
		if (pm_config.cp_sec_enable) {
			/* only master bq works, maxium target fcc should limit to 6A */
			if (pdpm->no_need_en_slave_bq)
				curr_fcc_limit = min(curr_fcc_limit, FCC_MAX_MA_FOR_MASTER_BQ);
		}
		curr_ibus_limit = curr_fcc_limit >> 1;
		/*
		 * bq25970 alone compensate 100mA,  bq25970 master ans slave  compensate 300mA,
		 * for target curr_ibus_limit for bq adc accurancy is below standard and power suuply system current
		 */
		curr_ibus_limit += pm_config.bus_curr_compensate;
		/* curr_ibus_limit should compare with apdo_max_curr here*/
		curr_ibus_limit = min(curr_ibus_limit, pdpm->apdo_max_curr);
	}

	/* if cell vol read from fuel gauge is higher than threshold, vote saft fcc to protect battery */
	if (is_fastcharge_mode) {
		if (pdpm->cp.fg_vbat_mv > pdpm->cell_vol_high_threshold_mv) {
			if (pdpm->over_cell_vol_high_count++ > CELL_VOLTAGE_HIGH_COUNT_MAX) {
				pdpm->over_cell_vol_high_count = 0;
				if (pdpm->fcc_votable)
					vote(pdpm->fcc_votable, BQ_TAPER_CELL_HGIH_FCC_VOTER,
							true, pdpm->step_charge_high_vol_curr_max * 1000);
			}
		} else {
			pdpm->over_cell_vol_high_count = 0;
		}
	}

	/* Avoid ibus current exceed 6200mA which is the max request */
	ibus_limit = curr_ibus_limit - 50;

	/* reduce bus current in cv loop */
	if (pdpm->cp.fg_vbat_mv > (pm_config.bat_volt_lp_lmt - bq_taper_hys_mv)) {
		if (ibus_lmt_change_timer++ > IBUS_CHANGE_TIMEOUT) {
			ibus_lmt_change_timer = 0;
			ibus_limit = curr_ibus_limit - 100;
			effective_fcc_taper = usbpd_get_effective_fcc_val(pdpm);
			effective_fcc_taper -= BQ_TAPER_DECREASE_STEP_MA;
			pr_err("bq set taper fcc to : %d mA\n", effective_fcc_taper);
			if (pdpm->fcc_votable) {
				if (effective_fcc_taper >= 2000)
					vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER,
							true, effective_fcc_taper * 1000);
			}
		}
	} else if (pdpm->cp.fg_vbat_mv < pm_config.bat_volt_lp_lmt - 250) {
		ibus_limit = curr_ibus_limit - 50;
		ibus_lmt_change_timer = 0;
	} else {
		ibus_lmt_change_timer = 0;
	}

	/* battery voltage loop*/

	if (pdpm->cp.fg_vbat_mv > pm_config.bat_volt_lp_lmt)
		step_vbat = -pm_config.fc2_steps;
	else if (pdpm->cp.fg_vbat_mv < pm_config.bat_volt_lp_lmt - 10)
		step_vbat = pm_config.fc2_steps;

	/* battery charge current loop*/
	if (pdpm->cp.ibat_curr < curr_fcc_limit)
		step_ibat = pm_config.fc2_steps;
	else if (pdpm->cp.ibat_curr > curr_fcc_limit + 50)
		step_ibat = -pm_config.fc2_steps;

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
	if (pdpm->cp.vbat_reg) /*|| pdpm->cp.ibat_reg*/
		step_bat_reg = 3 * (-pm_config.fc2_steps);
	else
		step_bat_reg = pm_config.fc2_steps;

	sw_ctrl_steps = min(min(step_vbat, step_ibus), step_ibat);
	sw_ctrl_steps = min(sw_ctrl_steps, step_bat_reg);

	/* hardware alarm loop */
	if (pdpm->cp.bus_ocp_alarm || pdpm->cp.bus_ovp_alarm)
		hw_ctrl_steps = -pm_config.fc2_steps;
	else
		hw_ctrl_steps = pm_config.fc2_steps;
	/* check if cp disabled due to other reason*/
	usbpd_pm_check_cp_enabled(pdpm);

	if (pm_config.cp_sec_enable)
		usbpd_pm_check_cp_sec_enabled(pdpm);

	pd_get_batt_current_thermal_level(pdpm, &thermal_level);
	pdpm->is_temp_out_fc2_range = pd_disable_cp_by_jeita_status(pdpm);

	if (pdpm->pd_active == POWER_SUPPLY_PPS_NON_VERIFIED &&
		pdpm->cp.ibat_curr > MAX_UNSUPPORT_PPS_CURRENT_MA) {
		pdpm->unsupport_pps_ta_check_count++;
		if (pdpm->unsupport_pps_ta_check_count > 3)
			unsupport_pps_status = true;
	} else {
		pdpm->unsupport_pps_ta_check_count = 0;
	}

	usbpd_pm_check_night_charging_enabled(pdpm);

	pd_log_count++;
	if (pd_log_count >= 20) {
		pd_log_count = 0;
		pr_info("ibus_limit:%d, ibus_total_ma:%d, ibus_master_ma:%d, ibus_slave_ma:%d, ibat_ma:%d\n",
				ibus_limit, ibus_total, pdpm->cp.ibus_curr, pdpm->cp_sec.ibus_curr, pdpm->cp.ibat_curr);
		pr_info("vbus_mv:%d, cp_vbat:%d, fg_vbat:%d\n",
				pdpm->cp.vbus_volt, pdpm->cp.vbat_volt, pdpm->cp.fg_vbat_mv);
		pr_info("master_cp_enable:%d, slave_cp_enable:%d\n",
				pdpm->cp.charge_enabled, pdpm->cp_sec.charge_enabled);
		pr_info("step_ibus:%d, step_ibat:%d, step_vbat, vbat_reg:%d, ibat_reg:%d, sw_ctrl_steps:%d, hw_ctrl_steps:%d\n",
				step_ibus, step_ibat, step_vbat, pdpm->cp.vbat_reg, pdpm->cp.ibat_reg, sw_ctrl_steps, hw_ctrl_steps);
	}

	if (pdpm->sw.night_charging) {
		pr_info("night charging enabled[%d]\n", pdpm->sw.night_charging);
		return PM_ALGO_RET_NIGHT_CHARGING;
	} else if (pdpm->cp.bat_therm_fault) { /* battery overheat, stop charge*/
		pr_info("bat_therm_fault:%d\n", pdpm->cp.bat_therm_fault);
		return PM_ALGO_RET_THERM_FAULT;
	} else if (!pdpm->cp.charge_enabled ||
		(pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled && !pdpm->no_need_en_slave_bq)) {
		pr_info("cp.charge_enabled:%d, cp_sec.charge_enabled:%d\n",
			pdpm->cp.charge_enabled, pdpm->cp_sec.charge_enabled);
		return PM_ALGO_RET_CHG_DISABLED;
	} else if (thermal_level >= pdpm->therm_level_threshold || pdpm->is_temp_out_fc2_range) {
		pr_info("thermal level too high:%d\n", thermal_level);
		return PM_ALGO_RET_CHG_DISABLED;
	} else if (pdpm->cp.bat_ocp_fault || pdpm->cp.bus_ocp_fault
			|| pdpm->cp.bat_ovp_fault || pdpm->cp.bus_ovp_fault) {
		pr_info("bat_ocp_fault:%d, bus_ocp_fault:%d, bat_ovp_fault:%d, bus_ovp_fault:%d\n",
				pdpm->cp.bat_ocp_fault, pdpm->cp.bus_ocp_fault,
				pdpm->cp.bat_ovp_fault, pdpm->cp.bus_ovp_fault);
		return PM_ALGO_RET_OTHER_FAULT; /* go to switch, and try to ramp up*/
	} else if (pm_config.cp_sec_enable && pdpm->no_need_en_slave_bq
			&& pdpm->cp.charge_enabled && pdpm->cp.ibus_curr < CRITICAL_LOW_IBUS_THR) {
		if (pdpm->master_ibus_below_critical_low_count++ >= 5) {
			pr_info("master ibus below critical low but still enabled\n");
			pdpm->master_ibus_below_critical_low_count = 0;
			return PM_ALGO_RET_CHG_DISABLED;
		}
	} else if (unsupport_pps_status) {
		pr_info("unsupport pps charger.\n");
		return PM_ALGO_RET_UNSUPPORT_PPSTA;
	} else if (pm_config.cp_sec_enable) {
		pdpm->master_ibus_below_critical_low_count = 0;
	}


	if (pdpm->cp.fg_vbat_mv > pm_config.bat_volt_lp_lmt - TAPER_VOL_HYS
			&& pdpm->cp.ibat_curr < pm_config.fc2_taper_current) {
			if (fc2_taper_timer++ > TAPER_TIMEOUT) {
				pr_info("charge pump taper charging done, vbat[%d], ibat_curr[%d], soc[%d]\n",
							pdpm->cp.fg_vbat_mv, pdpm->cp.ibat_curr, capacity);
				fc2_taper_timer = 0;
				return PM_ALGO_RET_TAPER_DONE;
			}
	} else {
		fc2_taper_timer = 0;
	}

	if ((last_thermal_level != thermal_level) && ((thermal_level == THERMAL_LEVEL_11)
			|| (thermal_level == THERMAL_LEVEL_12)))
		pdpm->request_voltage = pdpm->cp.vbat_volt * 2 + THERMAL_11_VBUS_UP;
	last_thermal_level = thermal_level;

	/*TODO: customer can add hook here to check system level
	 * thermal mitigation*/

	steps = min(sw_ctrl_steps, hw_ctrl_steps);
	pdpm->request_voltage += steps * STEP_MV;

	pdpm->request_current = min(pdpm->apdo_max_curr, curr_ibus_limit);

	/*if (pdpm->apdo_max_volt == PPS_VOL_MAX)
		pdpm->apdo_max_volt = pdpm->apdo_max_volt - PPS_VOL_HYS;*/

	if (pdpm->request_voltage >= pdpm->apdo_max_volt)
		pdpm->request_voltage = pdpm->apdo_max_volt - STEP_MV;

	pr_info("steps: %d, pdpm->request_voltage: %d\n", steps, pdpm->request_voltage);

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
	pr_info("state change:%s -> %s\n", pm_str[pdpm->state], pm_str[state]);
	pdpm->state = state;
}

static int usbpd_pm_sm(struct usbpd_pm *pdpm)
{
	int ret;
	int rc = 0;
	static int tune_vbus_retry;
	static bool stop_sw;
	static bool recover;
	static bool state_entry_exit;
	int effective_fcc_val = 0;
	int thermal_level = 0, capacity;
	static int curr_fcc_lmt, curr_ibus_lmt, chip_ok;
	chip_ok = g_battmngr_noti->fg_msg.chip_ok;

	switch (pdpm->state) {
	case PD_PM_STATE_ENTRY:
		stop_sw = false;
		recover = false;
		state_entry_exit = false;

		usbpd_pm_check_night_charging_enabled(pdpm);
		pd_get_batt_current_thermal_level(pdpm, &thermal_level);
		pdpm->is_temp_out_fc2_range = pd_disable_cp_by_jeita_status(pdpm);
		usbpd_pm_check_sec_batt_present(pdpm);
		pr_info("is_temp_out_fc2_range:%d\n", pdpm->is_temp_out_fc2_range);
		pd_get_batt_capacity(pdpm, &capacity);
		effective_fcc_val = usbpd_get_effective_fcc_val(pdpm);

		if (effective_fcc_val > 0) {
			curr_fcc_lmt = min(pm_config.bat_curr_lp_lmt, effective_fcc_val);
			curr_ibus_lmt = curr_fcc_lmt >> 1;
			pr_info("curr_ibus_lmt:%d\n", curr_ibus_lmt);
		}

		if (!chip_ok) {
			pr_info("chip_ok %d, waiting...\n", chip_ok);
		} else if (pdpm->cp.vbat_volt < pm_config.min_vbat_for_cp) {
			pr_info("batt_volt %d, waiting...\n", pdpm->cp.vbat_volt);
		} else if ((pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - VBAT_HIGH_FOR_FC_HYS_MV
			&& !pdpm->is_temp_out_fc2_range) || capacity >= CAPACITY_TOO_HIGH_THR) {
			pr_info("batt_volt %d is too high for cp, charging with switch charger\n",
					pdpm->cp.vbat_volt);
			state_entry_exit = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		} else if (!pd_get_fg_digest_verified(pdpm)) {
			pr_info("fg digest is not verified, waiting...\n");
		} else if (thermal_level >= pdpm->therm_level_threshold || pdpm->is_temp_out_fc2_range) {
			pr_info("thermal level is too high, waiting...\n");
		} else if (pdpm->sw.night_charging) {
			pr_info("night charging is open, waiting...\n");
		} else if (effective_fcc_val < START_DRIECT_CHARGE_FCC_MIN_THR) {
			pr_info("effective fcc is below start dc threshold, waiting...\n");
		} else if (pdpm->cp_sec_enable && !pdpm->cp_sec.batt_connecter_present) {
			pr_info("sec batt connecter miss! charging with switch charger\n");
			state_entry_exit = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		} else {
			pr_info("batt_volt-%d is ok, start flash charging\n",
					pdpm->cp.vbat_volt);
			pdpm->fc2_exit_flag = false;
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
		//pdpm->request_current = min(pdpm->apdo_max_curr, pm_config.bus_curr_lp_lmt);
		pdpm->request_current = min(pdpm->apdo_max_curr, curr_ibus_lmt);

		usbpd_select_pdo(pdpm, XM_PD_APDO_START, pdpm->request_voltage, pdpm->request_current);
		pr_info("request_voltage:%d, request_current:%d\n",
				pdpm->request_voltage, pdpm->request_current);

		usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);

		tune_vbus_retry = 0;
		break;

	case PD_PM_STATE_FC2_ENTRY_2:
		if (!chip_ok || !pdpm->pps_supported) {
			pr_info("Move to switch charging:%d\n", chip_ok);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		}
		pdpm->request_current = min(pdpm->apdo_max_curr, curr_ibus_lmt);
            	if (pdpm->cp.vbus_volt < (pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP - 100)) {
			tune_vbus_retry++;
			pdpm->request_voltage += STEP_MV;
			usbpd_select_pdo(pdpm, XM_PD_APDO, pdpm->request_voltage, pdpm->request_current);
		} else if (pdpm->cp.vbus_volt > (pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP + 200)) {
			tune_vbus_retry++;
			pdpm->request_voltage -= STEP_MV;
			usbpd_select_pdo(pdpm, XM_PD_APDO, pdpm->request_voltage, pdpm->request_current);
		} else {
			pr_info("adapter volt tune ok, retry %d times\n", tune_vbus_retry);
					usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_3);
			break;
		}

		if (tune_vbus_retry > 80) {
			pr_info("Failed to tune adapter volt into valid range, charge with switching charger\n");
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}
		break;

	case PD_PM_STATE_FC2_ENTRY_3:
/*The charging enable sequence for LN8000 must be execute on master first, and then execute on slave.*/
		if (!chip_ok || !pdpm->pps_supported) {
			pr_info("Move to switch charging:%d\n", chip_ok);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		}
            	if (!pdpm->cp.charge_enabled) {
			usbpd_pm_enable_cp(pdpm, true);
			msleep(30);
			usbpd_pm_check_cp_enabled(pdpm);
		}

		if (pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled) {
			pd_get_batt_current_thermal_level(pdpm, &thermal_level);
			pd_get_batt_capacity(pdpm, &capacity);
			if (thermal_level < MAX_THERMAL_LEVEL_FOR_DUAL_BQ
					&& capacity < CAPACITY_HIGH_THR) {
				usbpd_pm_enable_cp_sec(pdpm, true);
				msleep(30);
				usbpd_pm_check_cp_sec_enabled(pdpm);
			} else {
				pdpm->no_need_en_slave_bq = true;
			}
		}

		usbpd_pm_check_cp_sec_enabled(pdpm);
		usbpd_pm_check_cp_enabled(pdpm);
		usbpd_pm_update_cp_status(pdpm);
		if ((!pdpm->cp_sec.charge_enabled && pm_config.cp_sec_enable
				&& !pdpm->no_need_en_slave_bq) || !pdpm->cp.charge_enabled) {
			if (pdpm->cp.vbus_volt < 7200) {
				pr_info("vbus_volt:%d is low, retry.\n");
				usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
			}
		}

		if (pdpm->cp.charge_enabled) {
			if ((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled
					&& !pdpm->no_need_en_slave_bq)
					|| pdpm->no_need_en_slave_bq
					|| !pm_config.cp_sec_enable) {
				if (!pdpm->usb_icl_votable)
					pdpm->usb_icl_votable = find_votable("ICL");
				vote(pdpm->usb_icl_votable, MAIN_CHG_ICL_VOTER, true, 100 * 1000);
				msleep(1000);
				pr_info("vote 100mA to icl before enter fc2_tune\n");
				usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_TUNE);
				ibus_lmt_change_timer = 0;
				fc2_taper_timer = 0;
			}
		}
		break;

	case PD_PM_STATE_FC2_TUNE:
		ret = usbpd_pm_fc2_charge_algo(pdpm);
		if (!chip_ok || !pdpm->pps_supported) {
			pr_info("Move to switch charging:%d\n", chip_ok);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else	if (ret == PM_ALGO_RET_THERM_FAULT) {
			pr_info("Move to stop charging:%d\n", ret);
			stop_sw = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else if (ret == PM_ALGO_RET_OTHER_FAULT
				|| ret == PM_ALGO_RET_TAPER_DONE
				|| ret == PM_ALGO_RET_UNSUPPORT_PPSTA) {
			pr_info("Move to switch charging:%d\n", ret);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else if (ret == PM_ALGO_RET_CHG_DISABLED) {
			pr_info("Move to switch charging, will try to recover flash charging:%d\n",
					ret);
			recover = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else if (ret == PM_ALGO_RET_NIGHT_CHARGING) {
			recover = true;
			pr_info("Night Charging Feature is running %d\n", ret);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else {
			usbpd_select_pdo(pdpm, XM_PD_APDO, pdpm->request_voltage, pdpm->request_current);
			pr_info("request_voltage:%d, request_current:%d\n",
					pdpm->request_voltage, pdpm->request_current);
		}
		/*stop second charge pump if either of ibus is lower than 400ma during CV*/
		if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled && !pdpm->no_need_en_slave_bq
				&& pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - TAPER_WITH_IBUS_HYS
				&& (pdpm->cp.ibus_curr < TAPER_IBUS_THR || pdpm->cp_sec.ibus_curr < TAPER_IBUS_THR)) {
			pr_info("second cp is disabled due to ibus < 450mA\n");
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}
		break;

	case PD_PM_STATE_FC2_EXIT:
/*
		if (!pdpm->fc2_exit_flag) {
			if (pdpm->cp.vbus_volt > 6000) {
				pdpm->fc2_exit_flag = true;
				schedule_delayed_work(&pdpm->fc2_exit_work, 0);
			} else {
				usbpd_select_pdo(pdpm, XM_PD_APDO_END, 5000, 3000);
			}
		}
*/

		if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled) {
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}

		if (pdpm->cp.charge_enabled) {
			usbpd_pm_enable_cp(pdpm, false);
			usbpd_pm_check_cp_enabled(pdpm);
		}
		msleep(5);

		usbpd_select_pdo(pdpm, XM_PD_APDO_END, 5000, 3000);

		pdpm->no_need_en_slave_bq = false;
		pdpm->master_ibus_below_critical_low_count = 0;
		pdpm->chip_ok_count = 0;

		if (stop_sw && pdpm->sw.charge_enabled)
			usbpd_pm_enable_sw(pdpm, false);
		else if (!stop_sw && !pdpm->sw.charge_enabled)
			usbpd_pm_enable_sw(pdpm, true);
		if (state_entry_exit)
			usbpd_pm_enable_sw(pdpm, true);

		if (pdpm->fcc_votable) {
			vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER, false, 0);
			vote(pdpm->fcc_votable, BQ_TAPER_CELL_HGIH_FCC_VOTER, false, 0);
		}

		msleep(50);
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

	usbpd_pm_update_sw_status(pdpm);
	usbpd_pm_update_cp_status(pdpm);
	usbpd_pm_update_cp_sec_status(pdpm);

	if (!usbpd_pm_sm(pdpm) && pdpm->pd_active) {
		if (pdpm->cp.vbat_volt >= CRITICAL_HIGH_VOL_THR_MV
				&& pm_config.cp_sec_enable)
			internal = PM_WORK_RUN_CRITICAL_INTERVAL;
		else if (pdpm->cp.vbat_volt >= HIGH_VOL_THR_MV)
			internal = PM_WORK_RUN_QUICK_INTERVAL;
		else
			internal = PM_WORK_RUN_NORMAL_INTERVAL;
		schedule_delayed_work(&pdpm->pm_work,
				msecs_to_jiffies(internal));
	}
}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
	cancel_delayed_work_sync(&pdpm->pm_work);
	usbpd_pm_enable_cp_sec(pdpm, false);
	usbpd_pm_enable_cp(pdpm, false);

	pr_info("usbpd_pm_disconnect!\n");

	if (pdpm->fcc_votable) {
		vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER, false, 0);
		vote(pdpm->fcc_votable, BQ_TAPER_CELL_HGIH_FCC_VOTER, false, 0);
		vote(pdpm->fcc_votable, NON_PPS_PD_FCC_VOTER, false, 0);
	}
	if (pdpm->usb_icl_votable)
		vote(pdpm->usb_icl_votable, MAIN_CHG_ICL_VOTER, false, 0);
	if (pdpm->fv_votable)
		vote(pdpm->fv_votable, FFC_DISABLE_CP_FV_VOTER, false, 0);
	pdpm->pps_supported = false;
	pdpm->jeita_triggered = false;
	pdpm->is_temp_out_fc2_range = false;
	pdpm->no_need_en_slave_bq = false;
	pdpm->fc2_exit_flag = false;
	pdpm->over_cell_vol_high_count = 0;
	pdpm->slave_bq_disabled_check_count = 0;
	pdpm->master_ibus_below_critical_low_count = 0;
	pdpm->chip_ok_count = 0;
	pdpm->pd_auth_val = 0;
	pm_config.bat_curr_lp_lmt = pdpm->bat_curr_max;
	pm_config.bat_volt_lp_lmt = pdpm->bat_volt_max;
	pm_config.fc2_taper_current = TAPER_DONE_NORMAL_MA;
	usbpd_pm_enable_sw(pdpm, true);
	usbpd_pm_check_sw_enabled(pdpm);
	usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
}

static void usbpd_pd_contact(struct usbpd_pm *pdpm, int status)
{
	pdpm->pd_active = status;

	if (status) {
		usbpd_pm_evaluate_src_caps(pdpm);
		if (pdpm->pps_supported)
			schedule_delayed_work(&pdpm->pm_work, 0);
	} else {
		usbpd_pm_disconnect(pdpm);
	}
}

static void usbpd_pps_non_verified_contact(struct usbpd_pm *pdpm, int status)
{
	pdpm->pd_active = status;

	if (status) {
		usbpd_pm_evaluate_src_caps(pdpm);
		if (pdpm->pps_supported)
			schedule_delayed_work(&pdpm->pm_work, 5*HZ);
	} else {
		usbpd_pm_disconnect(pdpm);
	}
}

static void usb_psy_change_work(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					usb_psy_change_work);
	union power_supply_propval val = {0,};
	struct tcpm_remote_power_cap pd_cap;
	int usb_present = 0;
	int ret = 0, val1 = 0;

	ret = power_supply_get_property(pdpm->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &val);
	if (ret) {
		pr_err("Failed to read usb preset!\n");
	}
	usb_present = val.intval;

/*
	ret = power_supply_get_property(pdpm->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_POWER_ROLE, &val);
	if (ret) {
		pr_err("Failed to read typec power role\n");
		goto out;
	}

	if (val.intval != POWER_SUPPLY_TYPEC_PR_SINK &&
			val.intval != POWER_SUPPLY_TYPEC_PR_DUAL)
		goto out;
*/

	ret = xm_battmngr_read_iio_prop(g_battmngr_iio, PD_PHY,
			PD_ACTIVE, &val1);
	if (ret) {
		pr_err("Failed to get usb pd active state\n");
	}

	if (!pdpm->fcc_votable)
		pdpm->fcc_votable = find_votable("FCC");

	if ((pdpm->pd_active < POWER_SUPPLY_PPS_VERIFIED) && (pdpm->pd_auth_val == 1)
			&& (val1 == POWER_SUPPLY_PD_PPS_ACTIVE)) {
		msleep(30);
		usbpd_pd_contact(pdpm, POWER_SUPPLY_PPS_VERIFIED);
		if (pdpm->fcc_votable)
			vote(pdpm->fcc_votable, NON_PPS_PD_FCC_VOTER, false, 0);
	} else if (!pdpm->pd_active
			&& (val1 == POWER_SUPPLY_PD_PPS_ACTIVE)) {
		usbpd_pps_non_verified_contact(pdpm, POWER_SUPPLY_PPS_NON_VERIFIED);
		if (pdpm->fcc_votable)
			vote(pdpm->fcc_votable, NON_PPS_PD_FCC_VOTER, false, 0);
		if (!pdpm->pps_supported) {
			pd_cap.nr = 0;
			pd_cap.selected_cap_idx = 0;
			tcpm_get_remote_power_cap(pdpm->tcpc, &pd_cap);
			if ((pd_cap.type[0] == 0) && (pd_cap.nr > 0)) {
					usbpd_select_pdo(pdpm, XM_PD_FIXED, pd_cap.max_mv[0], pd_cap.ma[0]);
					pr_err("%s: [1] cap.max_mv[%d], cap.ma[%d]\n", __func__, pd_cap.max_mv[0], pd_cap.ma[0]);
			}
			if ((pd_cap.type[1] == 0) && (pd_cap.nr > 1)) {
					usbpd_select_pdo(pdpm, XM_PD_FIXED, pd_cap.max_mv[1], pd_cap.ma[1]);
					pr_err("%s: [2] cap.max_mv[%d], cap.ma[%d]\n", __func__, pd_cap.max_mv[1], pd_cap.ma[1]);
			}
			pr_err("%s unsupported pps, select pd fixed 2.\n", __func__);
			if (pdpm->fcc_votable)
				vote(pdpm->fcc_votable, NON_PPS_PD_FCC_VOTER, true, NON_PPS_PD_FCC_LIMIT);
		}
	} else if (pdpm->pd_active && !val1) {
		usbpd_pd_contact(pdpm, POWER_SUPPLY_PPS_INACTIVE);
	} else if (!pdpm->pd_active && (val1 == POWER_SUPPLY_PD_ACTIVE) && usb_present) {
		pdpm->pd_active = 1;
		pd_cap.nr = 0;
		pd_cap.selected_cap_idx = 0;
		tcpm_get_remote_power_cap(pdpm->tcpc, &pd_cap);
		if ((pd_cap.type[0] == 0) && (pd_cap.nr > 0)) {
			usbpd_select_pdo(pdpm, XM_PD_FIXED, pd_cap.max_mv[0], pd_cap.ma[0]);
			pr_err("%s: [1] cap.max_mv[%d], cap.ma[%d]\n", __func__, pd_cap.max_mv[0], pd_cap.ma[0]);
		}
		if ((pd_cap.type[1] == 0) && (pd_cap.nr > 1)) {
			usbpd_select_pdo(pdpm, XM_PD_FIXED, pd_cap.max_mv[1], pd_cap.ma[1]);
			pr_err("%s: [2] cap.max_mv[%d], cap.ma[%d]\n", __func__, pd_cap.max_mv[1], pd_cap.ma[1]);
		}
		pr_err("%s this is pd2.0 adapter.\n", __func__);
		if (pdpm->fcc_votable)
			vote(pdpm->fcc_votable, NON_PPS_PD_FCC_VOTER, true, NON_PPS_PD_FCC_LIMIT);
	}
}

static int usbpd_psy_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	usbpd_check_usb_psy(pdpm);

	if (!pdpm->usb_psy)
		return NOTIFY_OK;

	if (psy == pdpm->usb_psy) {
		spin_lock_irqsave(&pdpm->psy_change_lock, flags);
		schedule_work(&pdpm->usb_psy_change_work);
		spin_unlock_irqrestore(&pdpm->psy_change_lock, flags);
	}

	return NOTIFY_OK;
}

static int usbpd_process_event_pd(struct battmngr_notify *noti_data)
{
	int rc = 0;

	pr_err("%s: msg_type %d\n", __func__, noti_data->pd_msg.msg_type);

	switch (noti_data->pd_msg.msg_type) {
	case BATTMNGR_MSG_PD_VERIFED:
		__pdpm->pd_auth_val = noti_data->pd_msg.pd_verified;
		power_supply_changed(__pdpm->usb_psy);
		break;
	default:
		break;
	}

	return rc;
}

static int usbpd_battmngr_notifier_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct battmngr_notify *noti_data = data;

	pr_err("%s: event %d\n", __func__, event);

	switch (event) {
	case BATTMNGR_EVENT_PD:
		usbpd_process_event_pd(noti_data);
		break;
	default:
		break;
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
			"mi,step-charge-high-vol-curr-max", &pdpm->step_charge_high_vol_curr_max);

	pr_info("pdpm->step_charge_high_vol_curr_max:%d\n",
				pdpm->step_charge_high_vol_curr_max);

	rc = of_property_read_u32(node,
			"mi,cell-vol-high-threshold-mv", &pdpm->cell_vol_high_threshold_mv);

	pr_info("pdpm->cell_vol_high_threshold_mv:%d\n",
				pdpm->cell_vol_high_threshold_mv);

	rc = of_property_read_u32(node,
			"mi,cell-vol-max-threshold-mv", &pdpm->cell_vol_max_threshold_mv);

	pr_info("pdpm->cell_vol_max_threshold_mv:%d\n",
				pdpm->cell_vol_max_threshold_mv);

	rc = of_property_read_u32(node,
			"mi,pd-non-ffc-bat-volt-max", &pdpm->non_ffc_bat_volt_max);

	pr_info("pdpm->non_ffc_bat_volt_max:%d\n",
				pdpm->non_ffc_bat_volt_max);

	rc = of_property_read_u32(node,
			"mi,pd-bus-curr-compensate", &pdpm->bus_curr_compensate);
	if (rc < 0)
		pr_err("pd-bus-curr-compensate property missing, use default val\n");
	else
		pm_config.bus_curr_compensate = pdpm->bus_curr_compensate;

	pdpm->therm_level_threshold = MAX_THERMAL_LEVEL;
	rc = of_property_read_u32(node,
			"mi,therm-level-threshold", &pdpm->therm_level_threshold);
	if (rc < 0)
		pr_err("therm-level-threshold missing, use default val\n");
	pr_info("therm-level-threshold:%d\n", pdpm->therm_level_threshold);

	pdpm->battery_warm_th = JEITA_WARM_THR;
	rc = of_property_read_u32(node,
			"mi,pd-battery-warm-th", &pdpm->battery_warm_th);
	if (rc < 0)
		pr_err("pd-battery-warm-th missing, use default val\n");
	pr_info("pd-battery-warm-th:%d\n", pdpm->battery_warm_th);

	pdpm->cp_sec_enable = of_property_read_bool(node,
				"mi,cp-sec-enable");
	pm_config.cp_sec_enable = pdpm->cp_sec_enable;

	rc = of_property_read_u32(node, "mi,pd-power-max", &pdpm->pd_power_max);
	pr_info("pd-power-max:%d\n", pdpm->pd_power_max);

	return rc;
}

static int usbpd_pm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct usbpd_pm *pdpm;
	static int probe_cnt = 0;

	pr_err("probe_cnt = %d\n", ++probe_cnt);

	pdpm = kzalloc(sizeof(struct usbpd_pm), GFP_KERNEL);
	if (!pdpm)
		return -ENOMEM;

	if (!g_tcpc_rt1711h || !g_battmngr || !g_xm_pd_adapter) {
		pr_err("g_tcpc_rt1711h or g_battmngr or g_xm_pd_adapter not ready, defer\n");
		ret = -EPROBE_DEFER;
		msleep(100);
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			goto err_g_tcpc_rt1711h_battmngr;
	}

	__pdpm = pdpm;
	pdpm->dev = dev;

	ret = pd_policy_parse_dt(pdpm);
	if (ret < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, pdpm);

	spin_lock_init(&pdpm->psy_change_lock);
	usbpd_check_usb_psy(pdpm);
	usbpd_check_batt_psy(pdpm);
	usbpd_check_tcpc(pdpm);

	pdpm->over_cell_vol_high_count = 0;
	pdpm->master_ibus_below_critical_low_count = 0;
	pdpm->chip_ok_count = 0;
	INIT_WORK(&pdpm->usb_psy_change_work, usb_psy_change_work);
	INIT_DELAYED_WORK(&pdpm->pm_work, usbpd_pm_workfunc);
	INIT_DELAYED_WORK(&pdpm->fc2_exit_work, usbpd_fc2_exit_work);

	pdpm->nb.notifier_call = usbpd_psy_notifier_cb;
	power_supply_reg_notifier(&pdpm->nb);

	pdpm->battmngr_nb.notifier_call = usbpd_battmngr_notifier_cb;
	battmngr_notifier_register(&pdpm->battmngr_nb);

	pdpm->tcp_nb.notifier_call = pca_pps_tcp_notifier_call;
	register_tcp_dev_notifier(pdpm->tcpc, &pdpm->tcp_nb, TCP_NOTIFY_TYPE_USB);

out:
	platform_set_drvdata(pdev, pdpm);
	pr_err("%s!!\n", ret == -EPROBE_DEFER ? "Over probe cnt max" : "OK");
	return 0;

err_g_tcpc_rt1711h_battmngr:
	return ret;
}

static int usbpd_pm_remove(struct platform_device *pdev)
{
	power_supply_unreg_notifier(&__pdpm->nb);
	battmngr_notifier_unregister(&__pdpm->battmngr_nb);
	unregister_tcp_dev_notifier(__pdpm->tcpc,
		&__pdpm->tcp_nb, TCP_NOTIFY_TYPE_USB);
	cancel_delayed_work_sync(&__pdpm->pm_work);
	cancel_delayed_work_sync(&__pdpm->fc2_exit_work);
	cancel_work_sync(&__pdpm->usb_psy_change_work);

	return 0;
}

static const struct of_device_id usbpd_pm_of_match[] = {
	{ .compatible = "xiaomi,pd_cp_manager", },
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

static int __init usbpd_pm_init(void)
{
	return platform_driver_register(&usbpd_pm_driver);
}

late_initcall(usbpd_pm_init);

static void __exit usbpd_pm_exit(void)
{
	return platform_driver_unregister(&usbpd_pm_driver);
}
module_exit(usbpd_pm_exit);

MODULE_DESCRIPTION("Xiaomi pd Manager");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("getian@xiaomi.com");

