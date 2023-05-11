#define pr_fmt(fmt) "nopmi_chg %s: " fmt, __func__

#include "nopmi_chg.h"

#define PROBE_CNT_MAX	10
#define MAIN_CHG_SUSPEND_VOTER "MAIN_CHG_SUSPEND_VOTER"
#define CHG_INPUT_SUSPEND_VOTER "CHG_INPUT_SUSPEND_VOTER"
#define THERMAL_DAEMON_VOTER            "THERMAL_DAEMON_VOTER"
#define MAIN_ICL_MIN 100
#define SLOW_CHARGING_CURRENT_STANDARD 400
static const int NOPMI_CHG_WORKFUNC_GAP = 10000;
static const int NOPMI_CHG_CV_STEP_MONITOR_WORKFUNC_GAP = 2000;
static const int NOPMI_CHG_WORKFUNC_FIRST_GAP = 5000;

struct nopmi_chg *g_nopmi_chg = NULL;

struct step_config cc_cv_step_config[STEP_TABLE_MAX] = {
	{4240,    5400},
	{4440,    3920},
};

static void start_nopmi_chg_workfunc(void);
static void stop_nopmi_chg_workfunc(void);

int get_prop_battery_charging_enabled(struct votable *usb_icl_votable,
					union power_supply_propval *val)
{
	int icl = MAIN_ICL_MIN;

	val->intval = !(get_client_vote(usb_icl_votable, MAIN_CHG_SUSPEND_VOTER) == icl);

	return 0;
}

int set_prop_battery_charging_enabled(struct votable *usb_icl_votable,
				const union power_supply_propval *val)
{
	int icl = MAIN_ICL_MIN;

	if (val->intval == 0) {
		vote(usb_icl_votable, MAIN_CHG_SUSPEND_VOTER, true, icl);
	} else {
		vote(usb_icl_votable, MAIN_CHG_SUSPEND_VOTER, false, 0);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(get_prop_battery_charging_enabled);
EXPORT_SYMBOL_GPL(set_prop_battery_charging_enabled);

static int nopmi_set_prop_input_suspend(struct nopmi_chg *nopmi_chg,
				const union power_supply_propval *val)
{
	int rc;
	rc = vote(nopmi_chg->fcc_votable, CHG_INPUT_SUSPEND_VOTER, (bool)val->intval, 0);
	if (rc < 0) {
		pr_err("%s : Couldn't vote to %s USB rc = %d\n",
			(bool)val->intval ? "suspend" : "resume", rc);
		return rc;
	}

	nopmi_chg->input_suspend = !!(val->intval);

	return rc;
}

static int nopmi_set_prop_system_temp_level(struct nopmi_chg *nopmi_chg,
				const union power_supply_propval *val)
{
	int  rc = 0;

	if (val->intval < 0 ||
		nopmi_chg->thermal_levels <=0 ||
		val->intval > nopmi_chg->thermal_levels)
		return -EINVAL;

	if (val->intval == nopmi_chg->system_temp_level)
		return rc;

	nopmi_chg->system_temp_level = val->intval;
	/*if temp level at max and should be disable buck charger(vote icl as 0) & CP(vote ffc as 0) */
	if (nopmi_chg->system_temp_level == nopmi_chg->thermal_levels) { // thermal temp level
		rc = vote(nopmi_chg->usb_icl_votable, THERMAL_DAEMON_VOTER, true, 0);
		if(rc < 0){
			pr_err("%s : icl vote failed\n", __func__);
		}
		rc = vote(nopmi_chg->fcc_votable, THERMAL_DAEMON_VOTER, true, 0);
		if(rc < 0){
			pr_err("%s : fcc vote failed \n", __func__);
		}
        	return rc;
	}

	/*if temp level exit max value and enable icl, and then continues fcc vote*/
	rc = vote(nopmi_chg->usb_icl_votable, THERMAL_DAEMON_VOTER, false, 0);

	if (nopmi_chg->system_temp_level == 0) {
		rc = vote(nopmi_chg->fcc_votable, THERMAL_DAEMON_VOTER, false, 0);
	}else {
		rc = vote(nopmi_chg->fcc_votable, THERMAL_DAEMON_VOTER, true,
			nopmi_chg->thermal_mitigation[nopmi_chg->system_temp_level] / 1000);//divide 1000 to match maxim driver fcc as mA
	}

	return rc;
}

static int nopmi_get_batt_health(struct nopmi_chg *nopmi_chg)
{
	union power_supply_propval pval = {0, };
	int ret;

	if (nopmi_chg == NULL) {
		pr_err("%s : nopmi_chg is null,can not use\n", __func__);
		return -EINVAL;
	}

	nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_GOOD;
	ret = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		pr_err("couldn't read batt temp property, ret=%d\n", ret);
		return -EINVAL;
	}

	pval.intval = pval.intval /10;

	if(pval.intval >= 60)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	}
	else if(pval.intval >= 58 && pval.intval < 60)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_HOT;
	}
	else if(pval.intval >= 45 && pval.intval < 58)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_WARM;
	}
	else if(pval.intval >= 15 && pval.intval < 45)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_GOOD;
	}
	else if(pval.intval >= 0 && pval.intval < 15)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_COOL;
	}
	else if(pval.intval < 0)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_COLD;
	}

	return nopmi_chg->batt_health;
}

static enum power_supply_property nopmi_batt_props[] = {
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	//POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_INPUT_SUSPEND,
	POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_SOC_DECIMAL,
	POWER_SUPPLY_PROP_SOC_DECIMAL_RATE,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_SHUTDOWN_DELAY,
};

static int nopmi_batt_get_prop_internal(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int rc = 0;
	static int vbat_mv = 3800;

	switch (psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		pval->intval = nopmi_get_batt_health(nopmi_chg);
		//pval->intval = 1;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
			pval->intval = 1;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:

		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if(nopmi_chg->main_psy)
			rc = power_supply_get_property(nopmi_chg->main_psy,
				POWER_SUPPLY_PROP_BATT_CHARGE_TYPE, pval);
		//pval->intval = 2;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:

		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		break;
	case POWER_SUPPLY_PROP_STATUS:
		rc = power_supply_get_property(nopmi_chg->main_psy, psp, pval);
		if(pval->intval == POWER_SUPPLY_STATUS_FULL)
			pval->intval = POWER_SUPPLY_STATUS_FULL;
		else if (nopmi_chg->input_suspend)
			pval->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (((pval->intval == POWER_SUPPLY_STATUS_DISCHARGING) ||
			(pval->intval == POWER_SUPPLY_STATUS_NOT_CHARGING)) &&
			(g_nopmi_chg->real_type > 0))
			pval->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if (g_nopmi_chg->pd_active)
			pval->intval = POWER_SUPPLY_STATUS_CHARGING;
		if (vbat_mv < 3300) {
			pval->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			if (!nopmi_chg->batt_psy)
				nopmi_chg->batt_psy = power_supply_get_by_name("battery");
			if (nopmi_chg->batt_psy)
				power_supply_changed(nopmi_chg->batt_psy);
			//pr_err("%s vbat_mv = %d\n", __func__, vbat_mv);
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, pval);
		vbat_mv = pval->intval / 1000;
		break;        
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_SOC_DECIMAL:
	case POWER_SUPPLY_PROP_SOC_DECIMAL_RATE:
	case POWER_SUPPLY_PROP_TECHNOLOGY:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
	case POWER_SUPPLY_PROP_SHUTDOWN_DELAY:
		rc = power_supply_get_property(nopmi_chg->bms_psy, psp, pval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		pval->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		rc = power_supply_get_property(nopmi_chg->bms_psy,
			POWER_SUPPLY_PROP_CAPACITY_LEVEL, pval);
		break;
	//case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
	//	break;
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		rc = get_prop_battery_charging_enabled(nopmi_chg->jeita_ctl.usb_icl_votable, pval);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		//rc = nopmi_get_prop_input_suspend(nopmi_chg->usb_icl_votable, pval);
		pval->intval = nopmi_chg->input_suspend;
		break;
	default:
		pr_err("batt power supply prop %d not supported\n", psp);
		return -EINVAL;
	}
	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int nopmi_batt_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int ret = 0;

	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		ret = max77729_batt_get_property(psy, psp, pval);
	}
	else
	{
		ret = nopmi_batt_get_prop_internal(psy, psp, pval);
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		pval->intval = nopmi_chg->system_temp_level;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		pval->intval = nopmi_chg->thermal_levels;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		pval->intval = nopmi_chg->system_temp_level;
		ret = 0;
		break;
	default:
		break;
	}
	return ret;
}

static int nopmi_batt_set_prop_internal(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int rc = 0;
	switch (prop) {
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
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
		rc = set_prop_battery_charging_enabled(nopmi_chg->jeita_ctl.usb_icl_votable, val);
		break;
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
		pr_err("%s: Set input suspend prop, value:%d\n",__func__,val->intval);
		rc = nopmi_set_prop_input_suspend(nopmi_chg, val);
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}
static int nopmi_batt_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int ret = 0;

	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		ret = max77729_batt_set_property(psy, prop, val);
	}
	else
	{
		ret = nopmi_batt_set_prop_internal(psy, prop, val);
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
		ret = nopmi_set_prop_system_temp_level(nopmi_chg, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		ret = nopmi_set_prop_system_temp_level(nopmi_chg, val);
		break;
	default:
		break;
	}
	return ret;
}

static int nopmi_batt_prop_is_writeable_internal(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED:
	case POWER_SUPPLY_PROP_INPUT_SUSPEND:
	case POWER_SUPPLY_PROP_SYSTEM_TEMP_LEVEL:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		return 1;
	default:
		break;
	}
	return 0;
}

static int nopmi_batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		return batt_prop_is_writeable(psy, psp);
	}
	else
	{
		return nopmi_batt_prop_is_writeable_internal(psy, psp);
	}
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = nopmi_batt_props,
	.num_properties = ARRAY_SIZE(nopmi_batt_props),
	.get_property = nopmi_batt_get_prop,
	.set_property = nopmi_batt_set_prop,
	.property_is_writeable = nopmi_batt_prop_is_writeable,
};

static int nopmi_init_batt_psy(struct nopmi_chg *chg)
{
	struct power_supply_config batt_cfg = {};
	int rc = 0;

	if(!chg) {
		pr_err("chg is NULL\n");
		return rc;
	}

	batt_cfg.drv_data = chg;
	batt_cfg.of_node = chg->dev->of_node;
	chg->batt_psy = devm_power_supply_register(chg->dev,
					   &batt_psy_desc,
					   &batt_cfg);

	if (IS_ERR(chg->batt_psy)) {
		pr_err("Couldn't register battery power supply\n");
		return PTR_ERR(chg->batt_psy);
	}

	return rc;
}
#if 0 // zsa remove this func
static int get_real_type(void)
{
	int type,real_type;

	type = main_get_charge_type();
	if (type == BQ2589X_VBUS_USB_SDP)
		real_type = POWER_SUPPLY_TYPE_USB;
	else if (type == BQ2589X_VBUS_USB_CDP)
		real_type = POWER_SUPPLY_TYPE_USB_CDP;
	else if (type == BQ2589X_VBUS_USB_DCP)
		real_type = POWER_SUPPLY_TYPE_USB_DCP;
	else if (type == BQ2589X_VBUS_NONSTAND)
		real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
	else if (type == BQ2589X_VBUS_MAXC)
		real_type = POWER_SUPPLY_TYPE_USB_HVDCP;
	else
		real_type = POWER_SUPPLY_TYPE_UNKNOWN;
	if (g_nopmi_chg->pd_active == 1 && real_type != POWER_SUPPLY_TYPE_UNKNOWN)
		real_type = POWER_SUPPLY_TYPE_USB_PD;

	pr_err("get_real_type %d\n", real_type);
	return real_type;
}
#endif
/************************
 * USB PSY REGISTRATION *
 ************************/
static enum power_supply_property nopmi_usb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_REAL_TYPE,
	POWER_SUPPLY_PROP_PD_ACTIVE,
	POWER_SUPPLY_PROP_PD_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_PD_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_PD_CURRENT_MAX,
	POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED,
	POWER_SUPPLY_PROP_PD_IN_HARD_RESET,
	POWER_SUPPLY_PROP_TYPEC_MODE,
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
	POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE,
	POWER_SUPPLY_PROP_MTBF_CUR,
	POWER_SUPPLY_PROP_TERM_CURRENT,
};

static int nopmi_usb_get_prop_internal(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int rc = 0;

	val->intval = 0;

	switch (psp) {
	#if 0
	case POWER_SUPPLY_PROP_PRESENT:
		if(g_nopmi_chg->real_type > 0)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if(g_nopmi_chg->usb_online > 0)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	#endif
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_TERM_CURRENT:
		rc = power_supply_get_property(nopmi_chg->main_psy, psp, val);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		break;
	case POWER_SUPPLY_PROP_TYPE:
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		break;
	case POWER_SUPPLY_PROP_REAL_TYPE:
                if(!g_nopmi_chg){
		pr_err("g_nopmi_chg is null\n");
                break;
		}
		if (g_nopmi_chg->pd_active) {
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		} else {
			val->intval = g_nopmi_chg->real_type;
		}
		pr_err("real_type=%d\n",val->intval);
		break;
	case POWER_SUPPLY_PROP_PD_ACTIVE:
		val->intval = g_nopmi_chg->pd_active;
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MIN:
		val->intval = g_nopmi_chg->pd_min_vol;
		break;
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:
		val->intval = g_nopmi_chg->pd_max_vol;
		break;
	case POWER_SUPPLY_PROP_PD_CURRENT_MAX:
		val->intval = g_nopmi_chg->pd_cur_max;
		break;
	case POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED:
		val->intval = g_nopmi_chg->pd_usb_suspend;
		break;
	case POWER_SUPPLY_PROP_PD_IN_HARD_RESET:
		val->intval = g_nopmi_chg->pd_in_hard_reset;
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		val->intval = g_nopmi_chg->typec_mode;
		break;
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		val->intval = g_nopmi_chg->cc_orientation + 1;   /*for cit test*/
		break;
	default:
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;

}

static int nopmi_usb_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int ret = 0;
	int rc;
	union power_supply_propval value;
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);

	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		ret = max77729_usb_get_property(psy, psp, val);
	}
	else
	{
		ret = nopmi_usb_get_prop_internal(psy, psp, val);
	}
#if 1
	switch (psp) {
		case POWER_SUPPLY_PROP_PRESENT:
			if(g_nopmi_chg->real_type > 0)
				val->intval = 1;
			else
				val->intval = 0;
			ret = 0;
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			if(g_nopmi_chg->usb_online > 0)
				val->intval = 1;
			else
				val->intval = 0;
			ret = 0;

			if(!nopmi_chg->bms_psy)
				nopmi_chg->bms_psy = power_supply_get_by_name("bms");
			if (nopmi_chg->bms_psy) {
				rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
				if (rc < 0) {
					value.intval = 3800000;
					pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_AVG fail\n", __func__);
				}
			}
			if (value.intval < 3300000)
				val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE:
			val->intval = nopmi_get_quick_charge_type(psy);
			ret = 0;
			break;
		case POWER_SUPPLY_PROP_MTBF_CUR:
			val->intval = g_nopmi_chg->mtbf_cur;
			ret =0;
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_NOW:
			val->intval = get_effective_result(g_nopmi_chg->usb_icl_votable);
			ret =0;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			val->intval = get_effective_result(g_nopmi_chg->fv_votable);
			ret =0;
			break;
		default:
			break;
	}
#endif
	return ret;
}

static int nopmi_usb_set_prop_internal(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
//	struct smb_charger *chg = &chip->chg;
	int rc = 0;

	switch (psp) {
		case POWER_SUPPLY_PROP_PRESENT:
			break;
		#if 0
		case POWER_SUPPLY_PROP_ONLINE:
			g_nopmi_chg->usb_online = val->intval;
			if(g_nopmi_chg->usb_online)
			{
				start_nopmi_chg_workfunc();
			}
			else
			{
				stop_nopmi_chg_workfunc();
			}
			break;
		#endif
		case POWER_SUPPLY_PROP_TERM_CURRENT:
			rc = power_supply_set_property(nopmi_chg->main_psy, psp, val);
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	//		rc = smblib_set_prop_sdp_current_max(chg, val->intval);
			break;
		case POWER_SUPPLY_PROP_POWER_NOW:
	//			chg->qc3p5_detected_mw = val->intval;
			break;
		case POWER_SUPPLY_PROP_PD_ACTIVE:
			g_nopmi_chg->pd_active = val->intval;
			if (g_nopmi_chg->pd_active)
				g_nopmi_chg->usb_online = 1;
			else
				g_nopmi_chg->usb_online = 0;
			power_supply_changed(nopmi_chg->usb_psy);
			break;
		case POWER_SUPPLY_PROP_PD_VOLTAGE_MIN:
			g_nopmi_chg->pd_min_vol = val->intval;
			break;
		case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:
			g_nopmi_chg->pd_max_vol = val->intval;
			break;
		case POWER_SUPPLY_PROP_PD_CURRENT_MAX:
			g_nopmi_chg->pd_cur_max = val->intval;
			break;
		case POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED:
			g_nopmi_chg->pd_usb_suspend =  val->intval;
			break;
		case POWER_SUPPLY_PROP_PD_IN_HARD_RESET:
			g_nopmi_chg->pd_in_hard_reset =  val->intval;
			break;
		case POWER_SUPPLY_PROP_REAL_TYPE:
			g_nopmi_chg->real_type = val->intval;
			break;
		case POWER_SUPPLY_PROP_TYPEC_MODE:
			g_nopmi_chg->typec_mode = val->intval;
			break;
		case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
			g_nopmi_chg->cc_orientation = val->intval;
			break;

		default:
			rc = -EINVAL;
			break;
	}

	return rc;

}

#if CONFIG_TOUCHSCREEN_COMMON
typedef struct touchscreen_usb_piugin_data{
		bool valid;
		bool usb_plugged_in;
		void (*event_callback)(void);
		} touchscreen_usb_piugin_data_t;
touchscreen_usb_piugin_data_t g_touchscreen_usb_pulgin = {0};
EXPORT_SYMBOL(g_touchscreen_usb_pulgin);
#endif

static int nopmi_usb_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	int ret = 0;
	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		ret = max77729_usb_set_property(psy, psp, val);
	}
	else
	{
		ret = nopmi_usb_set_prop_internal(psy, psp, val);
	}

	pr_err("2021.09.21 wsy %s psp=%d,val->intval=%d",__func__,psp,val->intval);
#if 1
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			g_nopmi_chg->usb_online = val->intval;
			if(g_nopmi_chg->usb_online)
			{
				if((NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type()) || (NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())||(NOPMI_CHARGER_IC_SC == nopmi_get_charger_ic_type()))
					pm_stay_awake(g_nopmi_chg->dev);
				start_nopmi_chg_workfunc();
			}
			else
			{
				if((NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type()) || (NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())||(NOPMI_CHARGER_IC_SC == nopmi_get_charger_ic_type()))
					pm_relax(g_nopmi_chg->dev);
				stop_nopmi_chg_workfunc();
			}
			ret = 0;
			#if CONFIG_TOUCHSCREEN_COMMON
			g_touchscreen_usb_pulgin.usb_plugged_in = g_nopmi_chg->usb_online;
			if(g_touchscreen_usb_pulgin.valid){
				g_touchscreen_usb_pulgin.event_callback();
			}
			#endif
			break;
		case POWER_SUPPLY_PROP_MTBF_CUR:
			pr_err("%s psp=%d Set MTBF current val->intval=%d",__func__, psp, val->intval);
			g_nopmi_chg->mtbf_cur = val->intval;
			ret =0;
			break;
		case POWER_SUPPLY_PROP_REAL_TYPE:           //for maxim solution,  use persent use g_nopmi_chg->real_type, we have to set it in global setting.
			g_nopmi_chg->real_type = val->intval;
			break;
		default:
			break;
	}
#endif
	return ret;
}

static int nopmi_usb_prop_is_writeable_internal(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_POWER_NOW:
	case POWER_SUPPLY_PROP_PD_ACTIVE:
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MIN:
	case POWER_SUPPLY_PROP_PD_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_PD_CURRENT_MAX:
	case POWER_SUPPLY_PROP_REAL_TYPE:
	case POWER_SUPPLY_PROP_TYPEC_MODE:
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
	case POWER_SUPPLY_PROP_MTBF_CUR:
		return 1;
	default:
		break;
	}

	return 0;

}

static int nopmi_usb_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		return usb_prop_is_writeable(psy, psp);
	}
	else
	{
		return nopmi_usb_prop_is_writeable_internal(psy, psp);
	}
}

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB_PD,
	.properties = nopmi_usb_props,
	.num_properties = ARRAY_SIZE(nopmi_usb_props),
	.get_property = nopmi_usb_get_prop,
	.set_property = nopmi_usb_set_prop,
	.property_is_writeable = nopmi_usb_prop_is_writeable,
};

static int nopmi_init_usb_psy(struct nopmi_chg *chg)
{
	struct power_supply_config usb_cfg = {};

	usb_cfg.drv_data = chg;
	usb_cfg.of_node = chg->dev->of_node;
	chg->usb_psy = devm_power_supply_register(chg->dev,
						  &usb_psy_desc,
						  &usb_cfg);
	if (IS_ERR(chg->usb_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->usb_psy);
	}

	return 0;
}

static int nopmi_parse_dt_jeita(struct nopmi_chg *chg, struct device_node *np)
{
	u32 val;

	if (of_property_read_bool(np, "enable_sw_jeita"))
		chg->jeita_ctl.dt.enable_sw_jeita = true;
	else
		chg->jeita_ctl.dt.enable_sw_jeita = false;

	if (of_property_read_u32(np, "jeita_temp_above_t4_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_above_t4_cv = val;
	else {
		pr_err("use default JEITA_TEMP_ABOVE_T4_CV:%d\n",
		   JEITA_TEMP_ABOVE_T4_CV);
		chg->jeita_ctl.dt.jeita_temp_above_t4_cv = JEITA_TEMP_ABOVE_T4_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t3_to_t4_cv	 = val;
	else {
		pr_err("use default JEITA_TEMP_T3_TO_T4_CV:%d\n",
		   JEITA_TEMP_T3_TO_T4_CV);
		chg->jeita_ctl.dt.jeita_temp_t3_to_t4_cv = JEITA_TEMP_T3_TO_T4_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t2_to_t3_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T2_TO_T3_CV:%d\n",
		   JEITA_TEMP_T2_TO_T3_CV);
		chg->jeita_ctl.dt.jeita_temp_t2_to_t3_cv = JEITA_TEMP_T2_TO_T3_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_t1p5_to_t2_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t1p5_to_t2_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T1P5_TO_T2_CV:%d\n",
		   JEITA_TEMP_T1P5_TO_T2_CV);
		chg->jeita_ctl.dt.jeita_temp_t1p5_to_t2_cv = JEITA_TEMP_T1P5_TO_T2_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_t1_to_t1p5_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t1_to_t1p5_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T1_TO_T1P5_CV:%d\n",
		   JEITA_TEMP_T1_TO_T1P5_CV);
		chg->jeita_ctl.dt.jeita_temp_t1_to_t1p5_cv = JEITA_TEMP_T1_TO_T1P5_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t0_to_t1_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T0_TO_T1_CV:%d\n",
		   JEITA_TEMP_T0_TO_T1_CV);
		chg->jeita_ctl.dt.jeita_temp_t0_to_t1_cv = JEITA_TEMP_T0_TO_T1_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_tn1_to_t0_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_tn1_to_t0_cv = val;
	else {
		pr_err("use default JEITA_TEMP_TN1_TO_T0_CV:%d\n",
		   JEITA_TEMP_TN1_TO_T0_CV);
		chg->jeita_ctl.dt.jeita_temp_tn1_to_t0_cv = JEITA_TEMP_TN1_TO_T0_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_below_t0_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_below_t0_cv = val;
	else {
		pr_err("use default JEITA_TEMP_BELOW_T0_CV:%d\n",
		   JEITA_TEMP_BELOW_T0_CV);
		chg->jeita_ctl.dt.jeita_temp_below_t0_cv = JEITA_TEMP_BELOW_T0_CV;
	}

	if (of_property_read_u32(np, "normal-charge-voltage", &val) >= 0)
		chg->jeita_ctl.dt.normal_charge_voltage = val;
	else {
		pr_err("use default JEITA_TEMP_NORMAL_VOLTAGE:%d\n",
		   JEITA_TEMP_NORMAL_VOLTAGE);
		chg->jeita_ctl.dt.normal_charge_voltage = JEITA_TEMP_NORMAL_VOLTAGE;
	}
#if 0
	if(!of_property_read_u32(np, "normal-charge-voltage",&chg->jeita_ctl.dt.normal_charge_voltage))
	{
		chg->jeita_ctl.dt.normal_charge_voltage = JEITA_TEMP_NORMAL_VOLTAGE;
	}
#endif

	if (of_property_read_u32(np, "temp_t4_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t4_thres = val;
	else {
		pr_err("use default TEMP_T4_THRES:%d\n",
		   TEMP_T4_THRES);
		chg->jeita_ctl.dt.temp_t4_thres = TEMP_T4_THRES;
	}
	if (of_property_read_u32(np, "temp_t4_thres_minus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t4_thres_minus_x_degree = val;
	else {
		pr_err("use default TEMP_T4_THRES_MINUS_X_DEGREE:%d\n",
		   TEMP_T4_THRES_MINUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t4_thres_minus_x_degree =
				   TEMP_T4_THRES_MINUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_t3_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t3_thres = val;
	else {
		pr_err("use default TEMP_T3_THRES:%d\n",
		   TEMP_T3_THRES);
		chg->jeita_ctl.dt.temp_t3_thres = TEMP_T3_THRES;
	}
	if (of_property_read_u32(np, "temp_t3_thres_minus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t3_thres_minus_x_degree = val;
	else {
		pr_err("use default TEMP_T3_THRES_MINUS_X_DEGREE:%d\n",
		   TEMP_T3_THRES_MINUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t3_thres_minus_x_degree =
				   TEMP_T3_THRES_MINUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_t2_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t2_thres = val;
	else {
		pr_err("use default TEMP_T2_THRES:%d\n",
		   TEMP_T2_THRES);
		chg->jeita_ctl.dt.temp_t2_thres = TEMP_T2_THRES;
	}
	if (of_property_read_u32(np, "temp_t2_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t2_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T2_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T2_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t2_thres_plus_x_degree =
				   TEMP_T2_THRES_PLUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_t1p5_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1p5_thres = val;
	else {
		pr_err("use default TEMP_T1P5_THRES:%d\n",
		   TEMP_T1P5_THRES);
		chg->jeita_ctl.dt.temp_t1p5_thres = TEMP_T1P5_THRES;
	}
	if (of_property_read_u32(np, "temp_t1p5_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1p5_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T1P5_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T1P5_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t1p5_thres_plus_x_degree =
				   TEMP_T1P5_THRES_PLUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_t1_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1_thres = val;
	else {
		pr_err("use default TEMP_T1_THRES:%d\n",
		   TEMP_T1_THRES);
		chg->jeita_ctl.dt.temp_t1_thres = TEMP_T1_THRES;
	}
	if (of_property_read_u32(np, "temp_t1_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T1_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T1_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t1_thres_plus_x_degree =
				   TEMP_T1_THRES_PLUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_t0_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t0_thres = val;
	else {
		pr_err("use default TEMP_T0_THRES:%d\n",
		   TEMP_T0_THRES);
		chg->jeita_ctl.dt.temp_t0_thres = TEMP_T0_THRES;
	}
	if (of_property_read_u32(np, "temp_t0_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t0_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T0_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T0_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t0_thres_plus_x_degree =
				   TEMP_T0_THRES_PLUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_tn1_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_tn1_thres = val;
	else {
		pr_err("use default TEMP_TN1_THRES:%d\n",
		   TEMP_TN1_THRES);
		chg->jeita_ctl.dt.temp_tn1_thres = TEMP_TN1_THRES;
	}
	if (of_property_read_u32(np, "temp_tn1_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_tn1_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_TN1_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_TN1_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_tn1_thres_plus_x_degree =
				   TEMP_TN1_THRES_PLUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_neg_10_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_neg_10_thres = val;
	else {
		pr_err("use default TEMP_NEG_10_THRES:%d\n",
		   TEMP_NEG_10_THRES);
		chg->jeita_ctl.dt.temp_neg_10_thres = TEMP_NEG_10_THRES;
	}
	if (of_property_read_u32(np, "temp_t3_to_t4_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t3_to_t4_fcc = val;
	else {
		pr_err("use default TEMP_T3_TO_T4_FCC:%d\n",
		   TEMP_T3_TO_T4_FCC);
		chg->jeita_ctl.dt.temp_t3_to_t4_fcc = TEMP_T3_TO_T4_FCC;
	}
	if (of_property_read_u32(np, "temp_t2_to_t3_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t2_to_t3_fcc = val;
	else {
		pr_err("use default TEMP_T2_TO_T3_FCC:%d\n",
		   TEMP_T2_TO_T3_FCC);
		chg->jeita_ctl.dt.temp_t2_to_t3_fcc = TEMP_T2_TO_T3_FCC;
	}
	if (of_property_read_u32(np, "temp_t1p5_to_t2_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1p5_to_t2_fcc = val;
	else {
		pr_err("use default TEMP_T1P5_TO_T2_FCC:%d\n",
		   TEMP_T1P5_TO_T2_FCC);
		chg->jeita_ctl.dt.temp_t1p5_to_t2_fcc = TEMP_T1P5_TO_T2_FCC;
	}
	if (of_property_read_u32(np, "temp_t1_to_t1p5_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1_to_t1p5_fcc = val;
	else {
		pr_err("use default TEMP_T1_TO_T1P5_FCC:%d\n",
		   TEMP_T1_TO_T1P5_FCC);
		chg->jeita_ctl.dt.temp_t1_to_t1p5_fcc = TEMP_T1_TO_T1P5_FCC;
	}
	if (of_property_read_u32(np, "temp_t0_to_t1_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t0_to_t1_fcc = val;
	else {
		pr_err("use default TEMP_T0_TO_T1_FCC:%d\n",
		   TEMP_T0_TO_T1_FCC);
		chg->jeita_ctl.dt.temp_t0_to_t1_fcc = TEMP_T0_TO_T1_FCC;
	}
	if (of_property_read_u32(np, "temp_tn1_to_t0_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_tn1_to_t0_fcc = val;
	else {
		pr_err("use default TEMP_TN1_TO_T0_FCC:%d\n",
		   TEMP_TN1_TO_T0_FCC);
		chg->jeita_ctl.dt.temp_tn1_to_t0_fcc = TEMP_TN1_TO_T0_FCC;
	}

	return 0;
}

static int nopmi_parse_dt_thermal(struct nopmi_chg *chg, struct device_node *np)
{
	int byte_len;
	int rc = 0;

	if (of_find_property(np, "nopmi,thermal-mitigation", &byte_len)) {
		chg->thermal_mitigation = devm_kzalloc(chg->dev, byte_len,
			GFP_KERNEL);

		if (chg->thermal_mitigation == NULL)
			return -ENOMEM;

		chg->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(np,
				"nopmi,thermal-mitigation",
				chg->thermal_mitigation,
				chg->thermal_levels);
		if (rc < 0) {
			pr_err("Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}
	}

	return rc;
}

static int nopmi_parse_dt(struct nopmi_chg *chg)
{
	struct device_node *np = chg->dev->of_node;
	int rc = 0;

	if (!np) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(np,
				"qcom,fv-max-uv", &chg->dt.batt_profile_fv_uv);
	if (rc < 0)
		chg->dt.batt_profile_fv_uv = -EINVAL;
	else
		pr_err("nopmi_parse_dt %d\n",chg->dt.batt_profile_fv_uv);

	rc = nopmi_parse_dt_jeita(chg, np);
	if (rc < 0)
		return rc;

	rc = nopmi_parse_dt_thermal(chg, np);
	if (rc < 0)
		return rc;

	return 0;
};

static void nopmi_chg_workfunc(struct work_struct *work)
{
	struct nopmi_chg *chg = container_of(work,
		struct nopmi_chg, nopmi_chg_work.work);

	pr_err("2021.09.22 wsy %s g_nopmi_chg:0x%x\n",__func__, g_nopmi_chg);

	if(nopmi_chg_is_usb_present(chg->usb_psy))
	{
		start_nopmi_chg_jeita_workfunc();
		schedule_delayed_work(&chg->nopmi_chg_work, msecs_to_jiffies(NOPMI_CHG_WORKFUNC_GAP));
	}
}

static void start_nopmi_chg_workfunc(void)
{
	pr_err("2021.09.21 wsy %s g_nopmi_chg:0x%x\n",__func__, g_nopmi_chg);
	if(g_nopmi_chg)
	{
		schedule_delayed_work(&g_nopmi_chg->nopmi_chg_work, 0);
		schedule_delayed_work(&g_nopmi_chg->cvstep_monitor_work,
					msecs_to_jiffies(NOPMI_CHG_CV_STEP_MONITOR_WORKFUNC_GAP));
	}
}

static void stop_nopmi_chg_workfunc(void)
{
	pr_err("2021.09.21 wsy %s g_nopmi_chg:0x%x\n",__func__, g_nopmi_chg);

	if(g_nopmi_chg)
	{
		cancel_delayed_work_sync(&g_nopmi_chg->cvstep_monitor_work);
		cancel_delayed_work_sync(&g_nopmi_chg->nopmi_chg_work);
		stop_nopmi_chg_jeita_workfunc();
	}
}

static void  nopmi_cv_step_monitor_work(struct work_struct *work)
{
#if 1
	struct nopmi_chg *nopmi_chg = container_of(work,
		struct nopmi_chg, cvstep_monitor_work.work);
#endif
	//struct nopmi_chg *nopmi_chg = g_nopmi_chg;
	union power_supply_propval pval={0, };
	int rc = 0;
	int batt_curr = 0, batt_volt = 0;
	u32 i = 0, stepdown = 0, finalFCC = 0, votFCC = 0;
	static u32 count = 0;
	struct step_config *pcc_cv_step_config;
	u32 step_table_max;

	if (nopmi_chg->bms_psy) {
		pval.intval = 0;
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (rc < 0) {
			pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_NOW fail\n", __func__);
			goto out;
		}
		batt_curr =  pval.intval / 1000;

		pval.intval = 0;
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0) {
			pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_NOW fail\n", __func__);
			goto out;
		}
		batt_volt = pval.intval / 1000;
	}
	else
	{
		goto out;
	}
	pr_err("fg_cc_cv_step_check: batt_volt:%d batt_curr:%d", batt_volt, batt_curr);
	/*discharging*/
  	if(!nopmi_chg->fcc_votable)
        {
          	nopmi_chg->fcc_votable  = find_votable("FCC");
          	if(!nopmi_chg->fcc_votable)
        		goto out;
        }
	if(batt_curr < 0){
		vote(nopmi_chg->fcc_votable, CC_CV_STEP_VOTER, false, votFCC);
		goto out;
	}
	pcc_cv_step_config = cc_cv_step_config;
	step_table_max = STEP_TABLE_MAX;
	for(i = 0; i < step_table_max; i++){
		if((batt_volt >= (pcc_cv_step_config[i].volt_lim - CV_BATT_VOLT_HYSTERESIS))
						&& (batt_curr > pcc_cv_step_config[i].curr_lim)){
			count++;
			if(count >= 2){
				stepdown = true;
				count = 0;
				pr_err("fg_cc_cv_step_check:stepdown");
			}
			break;
		}
	}
	finalFCC = get_effective_result(nopmi_chg->fcc_votable);
	if(!stepdown || finalFCC <= pcc_cv_step_config[step_table_max-1].curr_lim)
		goto out;

	if(finalFCC - pcc_cv_step_config[i].curr_lim < STEP_DOWN_CURR_MA)
		votFCC = pcc_cv_step_config[i].curr_lim;
	else
		votFCC = finalFCC - STEP_DOWN_CURR_MA;
	vote(nopmi_chg->fcc_votable, CC_CV_STEP_VOTER, true, votFCC);
	pr_err("fg_cc_cv_step_check: i:%d cccv_step vote:%d stepdown:%d finalFCC:%d",
					i, votFCC, stepdown, finalFCC);
out:
	schedule_delayed_work(&nopmi_chg->cvstep_monitor_work,
				msecs_to_jiffies(NOPMI_CHG_CV_STEP_MONITOR_WORKFUNC_GAP));
	pr_err("nopmi_cv_step_monitor_work: end");
}

static int nopmi_chg_probe(struct platform_device *pdev)
{
	struct nopmi_chg *nopmi_chg = NULL;
	int rc;
	static int probe_cnt = 0;
	struct power_supply *psy;
	if (probe_cnt > PROBE_CNT_MAX)
	{
		pr_err("wsy nopmi_chg probe stop probe_cnt:%d,PROBE_CNT_MAX=%d\n", probe_cnt,PROBE_CNT_MAX);
		return 0;
	}

 	psy = power_supply_get_by_name("bms");
	if (!psy) {
		pr_err("%s get bms psy fail\n", __func__);
		if (probe_cnt <= PROBE_CNT_MAX)
		{
			return -EPROBE_DEFER;
		}
		else
		{
			rc = 0;
		}
	}else
		power_supply_put(psy);

	psy = power_supply_get_by_name("bbc");
	if (!psy) {
		pr_err("%s get main psy fail\n", __func__);
		if (probe_cnt <= PROBE_CNT_MAX)
		{
			return -EPROBE_DEFER;
		}
		else
		{
			rc = 0;
		}
	}else
		power_supply_put(psy);


	if (!pdev->dev.of_node)
		return -ENODEV;

	if (pdev->dev.of_node) {
		nopmi_chg = devm_kzalloc(&pdev->dev,
			sizeof(struct nopmi_chg),
			GFP_KERNEL);
		if (!nopmi_chg) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	}
	if (!nopmi_chg) {
		pr_err("No platform data found\n");
		return -EINVAL;
	}

	nopmi_chg->dev = &pdev->dev;
	nopmi_chg->pdev = pdev;
	platform_set_drvdata(pdev, nopmi_chg);

	g_nopmi_chg = nopmi_chg;

	rc = nopmi_parse_dt(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		goto err_free;
	}

	rc = nopmi_init_batt_psy(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = nopmi_init_usb_psy(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize usb psy rc=%d\n", rc);
		goto cleanup;
	}

	nopmi_chg->bms_psy = power_supply_get_by_name("bms");

	nopmi_chg->main_psy = power_supply_get_by_name("bbc");


	INIT_DELAYED_WORK(&nopmi_chg->nopmi_chg_work, nopmi_chg_workfunc);

	INIT_DELAYED_WORK(&nopmi_chg->cvstep_monitor_work, nopmi_cv_step_monitor_work);
//2021.09.21 wsy edit reomve vote to jeita
#if 1
	nopmi_chg->fcc_votable = find_votable("FCC");
	nopmi_chg->fv_votable = find_votable("FV");
	nopmi_chg->usb_icl_votable = find_votable("USB_ICL");
#endif
	nopmi_chg_jeita_init(&nopmi_chg->jeita_ctl);
	schedule_delayed_work(&nopmi_chg->nopmi_chg_work, msecs_to_jiffies(NOPMI_CHG_WORKFUNC_FIRST_GAP));
	if((NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type()) || (NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())||(NOPMI_CHARGER_IC_SC == nopmi_get_charger_ic_type()))
		device_init_wakeup(g_nopmi_chg->dev, true);

	pr_err("wsy nopmi_chg probe end\n");
	return 0;

cleanup:
	pr_err("nopmi_chg probe fail\n");
err_free:
	devm_kfree(&pdev->dev,nopmi_chg);
	return rc;
}

static int nopmi_chg_remove(struct platform_device *pdev)
{
	struct nopmi_chg *nopmi_chg = platform_get_drvdata(pdev);

	nopmi_chg_jeita_deinit(&nopmi_chg->jeita_ctl);

	devm_kfree(&pdev->dev,nopmi_chg);

	return 0;
}

static const struct of_device_id nopmi_chg_dt_match[] = {
	{.compatible = "qcom,nopmi-chg"},
	{},
};

static struct platform_driver nopmi_chg_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "qcom,nopmi-chg",
		.of_match_table = nopmi_chg_dt_match,
	},
	.probe = nopmi_chg_probe,
	.remove = nopmi_chg_remove,
};

static int __init nopmi_chg_init(void)
{
    platform_driver_register(&nopmi_chg_driver);
	pr_err("nopmi_chg init end\n");
    return 0;
}

static void __exit nopmi_chg_exit(void)
{
	pr_err("nopmi_chg exit\n");
	platform_driver_unregister(&nopmi_chg_driver);
}

module_init(nopmi_chg_init);
module_exit(nopmi_chg_exit);

MODULE_AUTHOR("WingTech Inc.");
MODULE_DESCRIPTION("battery driver");
MODULE_LICENSE("GPL");
