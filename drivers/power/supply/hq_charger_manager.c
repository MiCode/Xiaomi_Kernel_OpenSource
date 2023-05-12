#define pr_fmt(fmt) "batt_chg %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/soc/qcom/pmic_glink.h>
#include <linux/soc/qcom/battery_charger.h>
#include <linux/pm_wakeup.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>
#include "hq_charger_manager.h"


#if 0
int set_jeita_lcd_on_off(bool lcdon)
{
	//hq need set lcd state
	pr_err("%s, lcd_on %d\n", __func__, lcdon);
	return 0;
}
EXPORT_SYMBOL(set_jeita_lcd_on_off);

int get_jeita_lcd_on_off(void)
{
	//hq need get lcd state
	return g_batt_chg->lcd_on;
}
#endif

extern bool is_mtbf_mode_func(void);
extern int main_chgic_reset(void);

static int is_stop_charge;

int get_is_stop_charge(void)
{
	return is_stop_charge;
}
EXPORT_SYMBOL(get_is_stop_charge);

extern int bq25890_charging_term_en(int val);
static int switch_count;

/*+++++++++++++++++++++ fg api add here +++++++++++++++++++++*/
static int batt_set_prop_system_temp_level(struct batt_chg *chg,
				const union power_supply_propval *pval)
{
	if (pval->intval < 0)
		return -EINVAL;
	chg->system_temp_level = pval->intval;
	chg->therm_cur = thermal_mitigation[chg->system_temp_level];

	return 0;
}

static int batt_get_charge_status(struct batt_chg *chg, int* status)
{
	int rc;
	union power_supply_propval pval = {0, };
	if (!chg->fg_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	*status = pval.intval;

	return rc;
}

static int batt_get_battery_health(struct batt_chg *chg, int* health)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	if (!chg->fg_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);

	if (pval.intval >= 600)
		*health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (pval.intval >= 580 && pval.intval < 600)
		*health = POWER_SUPPLY_HEALTH_HOT;
	else if (pval.intval >= 450 && pval.intval < 580)
		*health = POWER_SUPPLY_HEALTH_WARM;
	else if (pval.intval >= 150 && pval.intval < 450)
		*health = POWER_SUPPLY_HEALTH_GOOD;
	else if (pval.intval >= 0 && pval.intval < 150)
		*health = POWER_SUPPLY_HEALTH_COOL;
	else if (pval.intval < 0)
		*health = POWER_SUPPLY_HEALTH_COLD;

	return rc;
}

static int batt_get_battery_present(struct batt_chg *chg, int* present)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	if (!chg->fg_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_PRESENT, &pval);
	*present = pval.intval;

	return rc;
}

static int batt_get_battery_capacity(struct batt_chg *chg, int* capacity)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	if (!chg->fg_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	*capacity = pval.intval;
	chg->ui_soc =  *capacity;
	return rc;
}

static int batt_get_battery_volt_uV(struct batt_chg *chg, int* vbat)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	if (!chg->fg_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
	*vbat = pval.intval;
	return rc;
}

static int batt_get_battery_current_uA(struct batt_chg *chg, int* ibat)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	if (!chg->fg_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	*ibat = pval.intval;
	return rc;
}

static int batt_get_battery_constant_current(struct batt_chg *chg, int* contant_charge_current)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	if (!chg->fg_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &pval);
	*contant_charge_current = pval.intval;
	return rc;
}

static int batt_get_battery_temp(struct batt_chg *chg, int* temp)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	if (!chg->fg_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	*temp = pval.intval;
	chg->battery_temp = *temp;
	return rc;
}

static int batt_get_battery_technology(struct batt_chg *chg, int* technoloy)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_TECHNOLOGY, &pval);
	*technoloy = pval.intval;
	return rc;
}

static int batt_get_battery_cycle_count(struct batt_chg *chg, int* cycle_count)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	if (!chg->fg_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	*cycle_count = pval.intval;
	return rc;
}

static int batt_get_battery_full(struct batt_chg *chg, int* charge_full)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	if (!chg->fg_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &pval);
	*charge_full = 4900000;
	return rc;
}

static int batt_get_battery_full_design(struct batt_chg *chg, int* charge_full_design)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	if (!chg->fg_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &pval);
	*charge_full_design = 4900000;
	return rc;
}

static int batt_get_charge_counter(struct batt_chg *chg, int* charge_counter)
{
	int rc = 0;
	union power_supply_propval pval = {0, };
	rc = power_supply_get_property(chg->fg_psy, POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
	*charge_counter = pval.intval;
	return rc;
}

static int batt_get_batt_verify_state(struct batt_chg *chg)
{
	int rc;
	union power_supply_propval pval = {0, };

	if (!chg->verify_psy) {
		pr_err("charge manager %s:%d, cannot finds verify psy", __func__, __LINE__);
		return -1;
	}

	rc = power_supply_get_property(chg->verify_psy,
            POWER_SUPPLY_PROP_AUTHENTIC, &pval);
	if (rc)
		chg->batt_auth = 0;
	else {
		if(pval.intval == 1) {
			rc = power_supply_get_property(chg->verify_psy, POWER_SUPPLY_PROP_MODEL_NAME, &pval);
			if (rc) {
				pr_err("get bat id err.");
				chg->batt_id = UNKNOW_SUPPLIER;
				return rc;
			} else {
				if(strcmp(pval.strval,"First supplier") == 0)
					chg->batt_id = FIRST_SUPPLIER;
				else if(strcmp(pval.strval,"Second supplier") == 0)
					chg->batt_id = SECOND_SUPPLIER;
				else if(strcmp(pval.strval,"Third supplier") == 0)
					chg->batt_id = THIRD_SUPPLIER;
				else
					chg->batt_id = UNKNOW_SUPPLIER;
			}
			if (chg->batt_id != UNKNOW_SUPPLIER)
				chg->batt_auth = 1;
		} else
			chg->batt_auth = 0;
	}
	return rc;
}

/*+++++++++++++++++++++ end +++++++++++++++++++++*/

/*+++++++++++++++++++++ sw api add here +++++++++++++++++++++*/
static int get_usb_charger_type(struct batt_chg *chg, int *type)
{
	union power_supply_propval val;
	int ret = 0;
	int i = 0;
	int usb_type;

	if (!chg->usb_psy) {
		pr_err("charge manager %s:%d, cannot finds usb psy", __func__, __LINE__);
		return -1;
	}

	mutex_lock(&chg->charger_type_mtx);
	ret = power_supply_get_property(chg->usb_psy, POWER_SUPPLY_PROP_USB_TYPE, &val);
	if (ret == 0) {
		usb_type = val.intval;
	}

	while (charger_type[i].type != 0) {
		if (usb_type == charger_type[i].type) {
			*type = charger_type[i].adap_type;
			chg->real_type = *type;
			mutex_unlock(&chg->charger_type_mtx);
			pr_info("usb_type %d, charger_type %d", usb_type, chg->real_type);
			return 0;
		}
		i++;
	}

	if ( usb_type == 0 ) {
		chg->real_type = 0;
		*type = 0;
	}

	mutex_unlock(&chg->charger_type_mtx);
	pr_info("usb_type %d, charger_type %d", usb_type, chg->real_type);
	return ret;
}
/*+++++++++++++++++++++ end +++++++++++++++++++++*/

static enum power_supply_property batt_psy_props[] = {
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
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
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
};

static int batt_psy_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	struct batt_chg *chg = power_supply_get_drvdata(psy);
	int rc = 0;
	int chg_type;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		batt_get_charge_status(chg, &pval->intval);
		if (chg->old_real_type != chg->real_type) {
			chg->old_real_type = chg->real_type;
			pr_err("usb type changed , schedule batt_chg_work\n");
			cancel_delayed_work(&chg->batt_chg_work);
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(100));
		}
		rc = get_usb_charger_type(chg, &chg_type);
		if (chg_type != 0 && (chg->battery_temp >= 480 || chg->is_stop_charge)) {
			if ( pval->intval == POWER_SUPPLY_STATUS_FULL )
				pval->intval = POWER_SUPPLY_STATUS_CHARGING;
		}

		break;
	case POWER_SUPPLY_PROP_HEALTH:
		rc = batt_get_battery_health(chg, &pval->intval);
		if(rc != 0)
			pval->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = batt_get_battery_present(chg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = batt_get_battery_capacity(chg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		pval->intval = chg->system_temp_level;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		pval->intval = sizeof(thermal_mitigation)/sizeof(int) - 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = batt_get_battery_volt_uV(chg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = batt_get_battery_current_uA(chg, &pval->intval);
		pval->intval = pval->intval/1000;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		rc = batt_get_battery_constant_current(chg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc =  batt_get_battery_temp(chg, &pval->intval);
		if(pval->intval >= 600)
			pval->intval = 601;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		rc = batt_get_battery_technology(chg, &pval->intval);
		if (rc != 0)
			pval->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		rc = batt_get_charge_counter(chg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		rc = batt_get_battery_cycle_count(chg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		rc = batt_get_battery_full(chg, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		rc = batt_get_battery_full_design(chg, &pval->intval);
		break;
	default:
		pr_err("batt power supply prop %d not supported\n", psp);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int batt_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int rc = 0;
	struct batt_chg *chg = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = batt_set_prop_system_temp_level(chg, val);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int batt_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = batt_psy_props,
	.num_properties = ARRAY_SIZE(batt_psy_props),
	.get_property = batt_psy_get_prop,
	.set_property = batt_psy_set_prop,
	.property_is_writeable = batt_psy_prop_is_writeable,
};

#if 0
static int sw_battery_recharge(struct batt_chg *chg)
{
	int r_soc = 0;
	int en = 0;

	//batt_get_iio_channel(chg, BMS, BATT_QG_CC_SOC, &r_soc);
	//hq need add fg soc
	if(r_soc <= 9750 && chg->charge_done && r_soc > 9000){
		en = 0;
		//batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, en);
		//hq need disable sw
		pr_err("r_soc %d,en %d\n", r_soc, en);
		msleep(100);
		en = 1;
		//batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, en);
		//hq need enable sw
	}
	pr_err("r_soc %d,en %d\n", r_soc, en);
	return 0;
}
#endif

static int judge_need_to_stop_charge(struct batt_chg *chg, int temp, int vbat)
{
	if (temp >= 480 && vbat >= 4100000)
			chg->is_stop_charge = 1;

	if (chg->is_stop_charge) {
		if (temp <= 460 || vbat <= 4000000)
			chg->is_stop_charge = 0;
	}

	pr_err("temp : %d, vbat: %d, is_stop: %d\n", temp, vbat, chg->is_stop_charge);

	return chg->is_stop_charge;
}

static int swchg_termination_current_adjust(struct batt_chg *chg, int temp, int type)
{
	union power_supply_propval pval ={0, };
	int rc;
	int sw_iterm;

	sw_iterm=chg->batt_iterm;
	if(type == POWER_SUPPLY_TYPE_USB_PD && chg->switch_chg_ic) {
		if(temp >= chg->dt.jeita_temp_step4 && temp < chg->dt.jeita_temp_step5)
			sw_iterm = 750000;//iterm 0.15c
		if (temp >= chg->dt.jeita_temp_step5 && temp < chg->dt.jeita_temp_step6) {
			if (chg->batt_id == FIRST_SUPPLIER)
				sw_iterm = 800000;//COSMX iterm 0.16c
			if (chg->batt_id == SECOND_SUPPLIER)
				sw_iterm = 850000;//NVT iterm 0.17c
		} 
	}

	pval.intval = sw_iterm;
	pr_err("sw_iterm=%d\n", sw_iterm);
	rc = power_supply_set_property(chg->sw_psy, POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &pval);
	return rc;
}

static int sw_battery_set_cv(struct batt_chg *chg, int temp, int type)
{
	union power_supply_propval pval = {0, };
	int rc;
	int sw_vreg = 4448000;

	if (type == POWER_SUPPLY_TYPE_USB_PD) {
		if (chg->is_pps_on) {
			sw_vreg = CM_FFC_SW_VREG_HIGH;
			bq25890_charging_term_en(false);
		} else {
			if (switch_count++ > 3) {
				switch_count = 0;
				bq25890_charging_term_en(true);
			}
		}
	} else {
		switch_count = 0;
		bq25890_charging_term_en(true);
	}

	if (type == POWER_SUPPLY_TYPE_USB_PD && chg->switch_chg_ic) {
		if (temp >= chg->dt.jeita_temp_step4 && temp < chg->dt.jeita_temp_step6)
			sw_vreg = CM_FFC_SW_VREG_LOW;
		else
			sw_vreg = chg->charge_voltage_max;
	}

	pval.intval = sw_vreg;
	pr_err("sw_vreg=%d, chg->is_pps_on=%d\n", sw_vreg, chg->is_pps_on);
	rc = power_supply_set_property(chg->sw_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &pval);

	return rc;
}

static void  sw_battery_jeita(struct batt_chg *chg, int temp)
{
	int vbat = 0;
	int rc;

	rc = batt_get_battery_volt_uV(chg, &vbat);

	if(temp <= chg->dt.jeita_temp_step0 ||  temp >= chg->dt.jeita_temp_step7){
			chg->jeita_cur = 0;
	} else {
		if(temp <= chg->dt.jeita_temp_step1) {
			chg->jeita_cur = 470000;
		} else if(temp > chg->dt.jeita_temp_step1 && temp <= chg->dt.jeita_temp_step2) {
			chg->jeita_cur = 896000;
		} else if(temp > chg->dt.jeita_temp_step2 && temp <= chg->dt.jeita_temp_step3) {
			chg->jeita_cur = 2350000;
		} else if(temp > chg->dt.jeita_temp_step3 && temp <= chg->dt.jeita_temp_step4) {
			chg->jeita_cur = 3820000;
		} else if(temp > chg->dt.jeita_temp_step4 && temp < chg->dt.jeita_temp_step6) {
			chg->jeita_cur = 5300000;
		} else if (temp >= chg->dt.jeita_temp_step6)
			chg->jeita_cur = 2450000;
	}

	if (temp >= chg->dt.jeita_temp_step7) {
		rc = main_chgic_reset();
        	if(rc)
        		pr_err("error\n");
    }

	is_stop_charge = judge_need_to_stop_charge(chg, temp, vbat);

	if (is_stop_charge)
		chg->jeita_cur = 0;
}

static void sw_get_charger_type_current_limit(struct batt_chg *chg,int type)
{
	switch (type)
	{
		case POWER_SUPPLY_TYPE_USB:
			chg->charge_limit_cur = 500000;
			chg->input_limit_cur = 500000;
			break;
		case POWER_SUPPLY_TYPE_USB_FLOAT:
			chg->charge_limit_cur = 1000000;
			chg->input_limit_cur = 1000000;
			break;
		case POWER_SUPPLY_TYPE_USB_CDP:
			chg->charge_limit_cur = 1500000;
			chg->input_limit_cur = 1500000;
			break;
		case POWER_SUPPLY_TYPE_USB_DCP:
			chg->charge_limit_cur = 1950000;
			chg->input_limit_cur = 1950000;
			break;
		case POWER_SUPPLY_TYPE_USB_PD:
			if (chg->batt_auth)
				chg->charge_limit_cur = 3000000;
			else
				chg->charge_limit_cur = 2000000;
			chg->input_limit_cur = 3000000;
			break;
		case POWER_SUPPLY_TYPE_UNKNOWN:
			chg->charge_limit_cur = 0;
			chg->input_limit_cur = 0;
			break;
		default:
			chg->charge_limit_cur = 500000;
			chg->input_limit_cur = 500000;
			break;
	}

	if (chg->pd_cur_max != 0) {
		chg->charge_limit_cur = min(chg->charge_limit_cur, chg->pd_cur_max);
		chg->input_limit_cur = min(chg->input_limit_cur, chg->pd_cur_max);
	}
}

static int swchg_select_charging_current_limit(struct batt_chg *chg, int temp, int type)
{
	int icl,ibat;
	union power_supply_propval val;
	int rc = 0;
	
	sw_get_charger_type_current_limit(chg, type);
	sw_battery_jeita(chg, temp);

	if(type != POWER_SUPPLY_TYPE_USB_PD) {
		icl = chg->input_limit_cur;
		ibat = min(chg->charge_limit_cur, chg->jeita_cur);
		if(chg->therm_cur != 0)
			ibat = min(ibat, chg->therm_cur);
		/*add for mtbf current limit if charge type is sdp or cdp*/
		if (is_mtbf_mode_func() && (type == POWER_SUPPLY_TYPE_USB || type == POWER_SUPPLY_TYPE_USB_CDP)) {
			icl = 1500000;
			ibat = 1500000;
		}
		//for phone current limit
		if (ibat == 900000) {
			if (chg->sw_chg_chip_id == 3)
				ibat = 2500000;
			else
				ibat = 2000000;
			icl = 900000;
		}
		/*end*/
		val.intval = icl;
		rc = power_supply_set_property(chg->sw_psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
		val.intval = ibat;
		rc = power_supply_set_property(chg->sw_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);

		pr_err("%s: charger state is %d, temp %d, jeita_cur %d, thermal_cur %d, type_icl %d, type_ibat %d, icl %d, ibat %d, is_mtbf %d\n", 
			__func__, type, chg->battery_temp, chg->jeita_cur, chg->therm_cur, chg->input_limit_cur,  chg->charge_limit_cur, icl, ibat, is_mtbf_mode_func());
	} else {
		if(!chg->is_pps_on) {
			icl = chg->input_limit_cur;
			ibat = min(chg->charge_limit_cur, chg->jeita_cur);
			if(chg->therm_cur != 0)
				ibat = min(ibat, chg->therm_cur);
			//for phone current limit
		if (ibat == 900000) {
			if (chg->sw_chg_chip_id == 3)
				ibat = 2500000;
			else
				ibat = 2000000;
			icl = 900000;
		}
			val.intval = icl;
			rc = power_supply_set_property(chg->sw_psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
			val.intval = ibat;
			rc = power_supply_set_property(chg->sw_psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
			pr_err("%s: charger state is pd, temp %d, jeita_cur %d, thermal_cur %d, icl %d, ibat %d, pd_cur_max %d\n", 
				__func__, chg->battery_temp, chg->jeita_cur, chg->therm_cur, icl, ibat, chg->pd_cur_max);
		} else
			pr_err("%s: charger state is pps, hq cm can not set sw current limit\n", __func__);
	}
	return rc;
}

extern int get_pps_enable_status(void);
extern int back_to_main_charge(void);
extern int get_pd_max_current_mA(void);
extern int get_sw_charger_chip_id(void);
static void swchg_curr_vreg_iterm_config(struct batt_chg *chg)
{
	int rc;
	int temp;
	int type;

	rc = batt_get_battery_temp(chg, &temp);
	rc = get_usb_charger_type(chg, &type);
	chg->is_pps_on = get_pps_enable_status();
	chg->switch_chg_ic = back_to_main_charge();
	chg->pd_cur_max = get_pd_max_current_mA();
	chg->sw_chg_chip_id = get_sw_charger_chip_id();

	rc = swchg_termination_current_adjust(chg, temp, type);
	if (rc)
		pr_err("swchg set iterm error.");

	rc = sw_battery_set_cv(chg, temp, type);
	if (rc)
		pr_err("swchg set vreg error.");

	rc = swchg_select_charging_current_limit(chg, temp, type);
	if (rc)
		pr_err("swchg set ibat and icl error.");
}

static int wt_init_batt_psy(struct batt_chg *chg)
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

static int wt_init_ext_psy(struct batt_chg *chg)
{
	if(!chg) {
		pr_err("chg is NULL\n");
		return -1;
	}

	chg->sw_psy = power_supply_get_by_name("bq25890_charger");
	if (chg->sw_psy == NULL) {
		pr_err("can not find sw psy\n");
		return -1;
	}

	chg->cp_psy = power_supply_get_by_name("ln8000_standalone");
	if (chg->cp_psy == NULL) {
		chg->cp_psy = power_supply_get_by_name("sc8551_standalone");
		if (chg->cp_psy == NULL) {
			pr_err("can not find cp psy\n");
			return -1;
		}
	}

	chg->fg_psy = power_supply_get_by_name("sm5602_bat");
	if (chg->fg_psy == NULL) {
		pr_err("can not find fg psy\n");
		return -1;
	}

	chg->usb_psy = power_supply_get_by_name("usb");
	if (chg->usb_psy == NULL) {
		pr_err("can not find usb psy\n");
		return -1;
	}

	chg->verify_psy = power_supply_get_by_name("batt_verify");
	if (chg->verify_psy == NULL) {
		pr_err("can not find batt_verify psy\n");
		//return -1;
	}

	return 1;
}

static int batt_init_config(struct batt_chg *chg)
{
	chg->input_batt_current_max = 6100000;
	chg->batt_current_max = 12200000;
	chg->mishow_flag = 0;
	chg->charge_voltage_max = 4448000;
	chg->charge_design_voltage_max = 11000000;
	chg->system_temp_level = 0;
	chg->batt_iterm = 256000;
	chg->shutdown_flag = false;
	chg->dt.jeita_temp_step0 = -100;
	chg->dt.jeita_temp_step1 = 0;
	chg->dt.jeita_temp_step2 = 50;
	chg->dt.jeita_temp_step3 = 100;
	chg->dt.jeita_temp_step4 = 150;
	chg->dt.jeita_temp_step5 = 350;
	chg->dt.jeita_temp_step6 = 480;
	chg->dt.jeita_temp_step7 = 600;
	chg->mtbf_current = 0;
	chg->node_flag = false;
	chg->is_chg_control = false;
	chg->therm_step = 0;
	chg->update_cont = 0;
	chg->fastcharge_mode= 0;
	chg->cp_master_adc = 1;
	chg->cp_slave_adc = 1;
	chg->start_cnt = 0;
	chg->high_temp_flag = 0;
	chg->connector_temp = 0;
	chg->usb_temp = 0;
	chg->input_limit_flag = 0;
	chg->pd_type_cnt = 0;
	chg->low_temp_flag = 0;
	chg->old_real_type = 0;
	chg->real_type = 0;

	return 0;
}

static int batt_parse_dt(struct batt_chg *chg)
{
	struct device_node *node = chg->dev->of_node;
	int rc = 0;

	if (!node) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(node,
				"qcom,fv-max-uv", &chg->dt.batt_profile_fv_uv);
	if (rc < 0)
		chg->dt.batt_profile_fv_uv = 4450000;
	else
		pr_err("test %d\n",chg->dt.batt_profile_fv_uv);

	rc = of_property_read_u32(node,
				"qcom,fcc-max-ua", &chg->dt.batt_profile_fcc_ua);
	if (rc < 0)
		chg->dt.batt_profile_fcc_ua = 10000000;
	else
		pr_err("test %d\n",chg->dt.batt_profile_fcc_ua);

	rc = of_property_read_u32(node,
				"qcom,batt_iterm", &chg->dt.batt_iterm);
	if (rc < 0)
		chg->dt.batt_iterm = 300000;
	else
		pr_err("test %d\n",chg->dt.batt_iterm);

	return 0;
};

#if 0
static int get_boot_mode(void)
{
#ifdef CONFIG_WT_QGKI
	char *bootmode_string= NULL;
	char bootmode_start[32] = " ";
	int rc;

	bootmode_string = strstr(saved_command_line,"androidboot.mode=");
	if(bootmode_string != NULL){
		strncpy(bootmode_start, bootmode_string+17, 7);
		rc = strncmp(bootmode_start, "charger", 7);
		if(rc == 0){
	//		pr_err("Offcharger mode!\n");
			return 1;
		}
	}
#endif
	return 0;
}
#endif

extern void do_msm_poweroff(void);
#define MAX_UEVENT_LENGTH 50
static void judge_shutdown_delay(struct batt_chg *chg)
{
	int rc = 0;
	int vbat = 0;
	int status = 0;
	//static char *envp[1];

	static char uevent_string[][MAX_UEVENT_LENGTH+1] = {
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n",//28
	};
	static char *envp[] = {
		uevent_string[0],
		NULL,
	};

	if(chg->ui_soc == 1) {
		rc = batt_get_battery_volt_uV(chg, &vbat);
		if (rc)
			pr_err("get vbat error.\n");
		rc = batt_get_charge_status(chg, &status);
		if (rc)
			pr_err("get charge status error.\n");
		
		if (vbat/1000 < (SHUTDOWN_DELAY_VOL_LOW - 100) && !chg->shutdown_delay) {
			pr_err("vbat under 3.2V, poweroff.\n");
			rc = main_chgic_reset();
			if(rc)
				pr_err("main chgic reset failed.\n");
			msleep(1000);
			do_msm_poweroff();
		}	

		if ((vbat/1000 >= SHUTDOWN_DELAY_VOL_LOW && vbat/1000 < SHUTDOWN_DELAY_VOL_HIGH)
			&& status != POWER_SUPPLY_STATUS_CHARGING){
				chg->shutdown_delay = true;
		} else if (status == POWER_SUPPLY_STATUS_CHARGING
						&& chg->shutdown_delay) {
				chg->shutdown_delay = false;
		} else {
			chg->shutdown_delay = false;
		}
	} else
		chg->shutdown_delay = false;

	if (chg->last_shutdown_delay != chg->shutdown_delay) {
		chg->last_shutdown_delay = chg->shutdown_delay;
		if (chg->shutdown_delay == true)
			strncpy(uevent_string[0]+28, "1",MAX_UEVENT_LENGTH-28);
		else
			strncpy(uevent_string[0]+28, "0",MAX_UEVENT_LENGTH-28);
		pr_err("envp[0] = %s\n", envp[0]);
		kobject_uevent_env(&chg->dev->kobj, KOBJ_CHANGE, envp);
	}
}

static void batt_chg_main(struct work_struct *work)
{
	int time = 0;
	int rc;
	int type;
	struct batt_chg *chg = container_of(work,
				struct batt_chg, batt_chg_work.work);
	if (!chg)
		return;

	rc = get_usb_charger_type(chg, &type);
	chg->real_type = type;
	rc = batt_get_battery_temp(chg,&chg->battery_temp);
	rc = batt_get_battery_capacity(chg,&chg->ui_soc);
	rc = batt_get_batt_verify_state(chg);
	pr_err("real_type %d, temp %d, soc %d, batt_id %d, batt_auth %d\n",
		chg->real_type, chg->battery_temp, chg->ui_soc, chg->batt_id, chg->batt_auth);

	judge_shutdown_delay(chg);

	if(chg->real_type) {
		if(!chg->wakeup_flag) {
			__pm_stay_awake(chg->wt_ws);
			chg->wakeup_flag = 1;
			pr_err("wt workup\n");
		}
		swchg_curr_vreg_iterm_config(chg);

		chg->power_supply_count = 0;
		power_supply_changed(chg->usb_psy);
		if(chg->is_pps_on) {
			time = 3000;
			power_supply_changed(chg->batt_psy);
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(time));
		} else {
			time = 3000;
			power_supply_changed(chg->batt_psy);
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(time));
		}
	} else {
		if(chg->wakeup_flag) {
			__pm_relax(chg->wt_ws);
			chg->wakeup_flag = 0;
			pr_err("wt workup relax\n");
		}
		if (chg->ui_soc != chg->old_capacity || chg->battery_temp > 550 || chg->power_supply_count >= 6 || chg->ui_soc <= 10) {
			chg->power_supply_count = 0;
			chg->old_capacity = chg->ui_soc;
			power_supply_changed(chg->batt_psy);
		} else {
			chg->power_supply_count += 1;
		}
		if(chg->ui_soc <= 10 || chg->battery_temp > 550 || chg->battery_temp < 50)
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(10000));
		else
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(30000));
	}
}

#if 0
static void batt_usb_type(struct work_struct *work)
{
	struct batt_chg *chg = container_of(work,
				struct batt_chg, usb_type_work.work);

	chg->usb_float = 0;
	chg->old_real_type  = POWER_SUPPLY_TYPE_UNKNOWN;
	power_supply_changed(chg->batt_psy);
	power_supply_changed(chg->usb_psy);
	pr_err("batt_usb_type chg->usb_float %d", chg->usb_float);
}

static void lower_poweroff_work(struct work_struct *work)
{
	struct batt_chg *chg = container_of(work,
				struct batt_chg, lower_poweroff_work.work);
	
	int batt_uV = 0;
	int rc;
	static int uvlo_trigger_cnt = 0;

	pr_err("enter lower_poweroff_work\n");

	rc = batt_get_battery_volt_uV(chg, &batt_uV);
	if (rc)
		pr_err("batt_get_battery_volt_uV error\n");
	
	if (batt_uV <= CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD)
		uvlo_trigger_cnt++;
	else
		uvlo_trigger_cnt = 0;

	if (uvlo_trigger_cnt > CM_UVLO_CALIBRATION_CNT_THRESHOLD) {
		pr_err("vbat less than uvlo, will shutdown\n");
		#ifdef CONFIG_HQ_QGKI
		do_msm_poweroff();
		#endif
	}	

	if (batt_uV < CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD)
		schedule_delayed_work(&chg->lower_poweroff_work, msecs_to_jiffies(1000));
	else
		schedule_delayed_work(&chg->lower_poweroff_work, msecs_to_jiffies(10000));
}

static void xm_charger_debug_info_print_work(struct work_struct *work)
{
	struct batt_chg *chg = container_of(work, struct batt_chg, charger_debug_info_print_work.work);
	int type;

	get_usb_charger_type(chg,&type);

	schedule_delayed_work(&chg->charger_debug_info_print_work, msecs_to_jiffies(1000));
}
#endif

#ifdef CONFIG_HQ_QGKI
extern void devm_iio_device_free(struct device *dev, struct iio_dev *indio_dev);
#endif

static int batt_chg_probe(struct platform_device *pdev)
{
	struct batt_chg *batt_chg = NULL;
	struct iio_dev *indio_dev;
	int rc;
	static int probe_cnt = 0;

	if (pdev->dev.of_node) {
		indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct batt_chg));
		if (!indio_dev) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	} else {
		return -ENODEV;
	}
	#ifdef CONFIG_HQ_QGKI
	probe_cnt++;
	#else
	pr_err("Do not need update probe_cnt");
	#endif

	batt_chg = iio_priv(indio_dev);
	batt_chg->indio_dev = indio_dev;
	batt_chg->dev = &pdev->dev;
	batt_chg->pdev = pdev;
	batt_chg->is25thermmitigflg = false;

	rc = wt_init_ext_psy(batt_chg);
	if (rc == -1) {
		pr_err("can not find chg fg psy");

		if (probe_cnt >= PROBE_CNT_MAX) {
			rc = -ENODEV;
			goto out;
		} else {
			rc = -EPROBE_DEFER;
			goto cleanup;
		}
	}

	rc = batt_parse_dt(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
#ifdef CONFIG_WT_QGKI
		goto cleanup;
#endif
	}

	rc = batt_init_config(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		goto cleanup;
	}

	rc = wt_init_batt_psy(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	mutex_init(&batt_chg->charger_type_mtx);

	//INIT_DELAYED_WORK(&batt_chg->lower_poweroff_work, lower_poweroff_work);
	INIT_DELAYED_WORK(&batt_chg->batt_chg_work, batt_chg_main);
//	INIT_DELAYED_WORK(&batt_chg->usb_type_work, batt_usb_type);

	platform_set_drvdata(pdev, batt_chg);

	batt_chg->wt_ws = wakeup_source_register(batt_chg->dev, "charge_wakeup");
	if (!batt_chg->wt_ws) {
		pr_err("wt chg workup fail!\n");
		wakeup_source_unregister(batt_chg->wt_ws);
	}

	//schedule_delayed_work(&batt_chg->lower_poweroff_work, msecs_to_jiffies(3000));
	schedule_delayed_work(&batt_chg->batt_chg_work, msecs_to_jiffies(3000));
	//INIT_DELAYED_WORK( &batt_chg->charger_debug_info_print_work, xm_charger_debug_info_print_work);
	//schedule_delayed_work(&batt_chg->charger_debug_info_print_work, 30 * HZ);
	g_batt_chg = batt_chg;
	pr_err("batt_chg probe success\n");
	return 0;

out:
	pr_err("hq_charger_manager Over probe cnt max");

cleanup:
	pr_err("batt_chg probe fail\n");
	g_batt_chg = NULL;
	#ifdef CONFIG_HQ_QGKI
	if (batt_chg && batt_chg->indio_dev)
		devm_iio_device_free(&pdev->dev, batt_chg->indio_dev);
	#endif
	return rc;
}

static int batt_chg_remove(struct platform_device *pdev)
{
	return 0;
}

static void batt_chg_shutdown(struct platform_device *pdev)
{
	struct batt_chg *chg = platform_get_drvdata(pdev);

	pr_err("%s batt_chg_shutdown\n", __func__);
	if (!chg)
		return;

	chg->shutdown_flag = true;

	return;
}

static const struct of_device_id batt_chg_dt_match[] = {
	{.compatible = "qcom,wt_chg"},
	{},
};

static struct platform_driver batt_chg_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "wt_chg",
		.of_match_table = batt_chg_dt_match,
	},
	.probe = batt_chg_probe,
	.remove = batt_chg_remove,
	.shutdown = batt_chg_shutdown,
};

static int __init batt_chg_init(void)
{
    platform_driver_register(&batt_chg_driver);
	pr_err("batt_chg init end\n");
    return 0;
}

static void __exit batt_chg_exit(void)
{
	pr_err("batt_chg exit\n");
	platform_driver_unregister(&batt_chg_driver);
}

module_init(batt_chg_init);
module_exit(batt_chg_exit);

MODULE_AUTHOR("huaqin Inc.");
MODULE_DESCRIPTION("battery driver");
MODULE_LICENSE("GPL");
