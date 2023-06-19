#define pr_fmt(fmt) "nopmi_chg_jeita %s: " fmt, __func__

#include "nopmi_chg_jeita.h"
#include "nopmi_chg_iio.h"
#include "nopmi_chg.h"

#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/ipc_logging.h>
#include <linux/printk.h>


struct nopmi_chg_jeita_st *g_nopmi_chg_jeita = NULL;
#define JEITA_CHG_VOTER		"JEITA_CHG_VOTER"
extern struct nopmi_chg *g_nopmi_chg;
bool g_ffc_disable = true;
EXPORT_SYMBOL_GPL(g_ffc_disable);
//extern int main_set_charge_enable(bool en);
extern int adapter_dev_get_pd_verified(void);

//add ipc log start
#if IS_ENABLED(CONFIG_FACTORY_BUILD)
	#if IS_ENABLED(CONFIG_DEBUG_OBJECTS)
		#define IPC_CHARGER_DEBUG_LOG
	#endif
#endif

#ifdef IPC_CHARGER_DEBUG_LOG
extern void *charger_ipc_log_context;

#undef pr_err
#define pr_err(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_ERR pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "nopmi_jeita: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}

#undef pr_info
#define pr_info(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_INFO pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "nopmi_jeita: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}

#endif
//add ipc log end

static int nopmi_chg_jeita_get_bat_temperature(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
	union power_supply_propval prop = {0, };
	int ret = 0;
	int temp = 0;

    if(!nopmi_chg_jeita->bms_psy)
    {
    	nopmi_chg_jeita->bms_psy = power_supply_get_by_name("bms");
        if (!nopmi_chg_jeita->bms_psy) {
        	pr_err("bms supply not found, defer probe\n");
        	return -EINVAL;
        }
	}

	ret = power_supply_get_property(nopmi_chg_jeita->bms_psy,
				POWER_SUPPLY_PROP_TEMP, &prop);
	if (ret < 0) {
		pr_err("couldn't read temperature property, ret=%d\n", ret);
		return -EINVAL;
	}
	temp = prop.intval/10;

	pr_info("get_bat_temperature is %d\n", temp);
	return temp;
}

static int nopmi_chg_jeita_get_charger_voltage(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
	union power_supply_propval prop = {0, };
	int ret = 0;
	int voltage;

    if(!nopmi_chg_jeita->bbc_psy)
    {
     	nopmi_chg_jeita->bbc_psy = power_supply_get_by_name("bbc");
        if (!nopmi_chg_jeita->bbc_psy) {
        	pr_err("bbc supply not found, defer probe\n");
        	return -EINVAL;
        }
	}

	ret = power_supply_get_property(nopmi_chg_jeita->bbc_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	if (ret < 0) {
		pr_err("couldn't read voltage property, ret=%d\n", ret);
		return -EINVAL;
	}
	voltage = prop.intval;
	pr_info("get charger_voltage is %d\n", voltage);
	return voltage;
}

static int nopmi_chg_jeita_get_batt_id(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
    struct nopmi_chg *chg = container_of(nopmi_chg_jeita, struct nopmi_chg, jeita_ctl);
	int rc = 0;
	int batt_id = 0;

    	rc = nopmi_chg_get_iio_channel(chg, NOPMI_BMS, FG_RESISTANCE_ID, &batt_id);

	if (rc != 0) {
		pr_err("couldn't get batt_id property, ret=%d\n", rc);
		return -EINVAL;
	}
	pr_info("get batt_id: %d\n", batt_id);
	return batt_id;
}

static int nopmi_chg_jeita_get_pd_active(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
	int pd_active = 0;

    	pd_active = g_nopmi_chg->pd_active;
	pr_info("get pd_active: %d\n", pd_active);

	return pd_active;
}

static int nopmi_chg_jeita_get_charger_term_current(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
	union power_supply_propval prop = {0, };
	int ret = 0;
	int term_curr = 0;

	nopmi_chg_jeita->bbc_psy = power_supply_get_by_name("bbc");
	if (!nopmi_chg_jeita->bbc_psy) {
		pr_err("usb supply not found, defer probe\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(nopmi_chg_jeita->bbc_psy,
				POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &prop);
	if (ret < 0) {
		pr_err("couldn't get term_current property, ret=%d\n", ret);
		return -EINVAL;
	}
	term_curr = prop.intval;
	pr_info("get term_current: %d\n", prop.intval);
	return term_curr;
}

static int nopmi_chg_jeita_set_charger_current(struct nopmi_chg_jeita_st *nopmi_chg_jeita, int cc)
{
	union power_supply_propval prop = {0, };
	int ret = 0;

	nopmi_chg_jeita->bbc_psy = power_supply_get_by_name("bbc");
	if (!nopmi_chg_jeita->bbc_psy) {
		pr_err("bbc supply not found, defer probe\n");
		return -EINVAL;
	}
	prop.intval = cc;
	ret = power_supply_set_property(nopmi_chg_jeita->bbc_psy,
				POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
	if (ret < 0) {
		pr_err("couldn't set current property, ret=%d\n", ret);
		return -EINVAL;
	}
	pr_info("set current is %d\n", prop.intval);
	return 0;
}

static int nopmi_chg_jeita_set_charger_voltage(struct nopmi_chg_jeita_st *nopmi_chg_jeita, int cv)
{
	union power_supply_propval prop = {0, };
	int ret = 0;

	nopmi_chg_jeita->bbc_psy = power_supply_get_by_name("bbc");
	if (!nopmi_chg_jeita->bbc_psy) {
		pr_err("bbc supply not found, defer probe\n");
		return -EINVAL;
	}
	prop.intval = cv;
	ret = power_supply_set_property(nopmi_chg_jeita->bbc_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	if (ret < 0) {
		pr_err("couldn't set voltage property, ret=%d\n", ret);
		return -EINVAL;
	}
	pr_info("set voltage is %d\n", prop.intval);
	return 0;
}

static int nopmi_chg_jeita_set_charger_term_current(struct nopmi_chg_jeita_st *nopmi_chg_jeita, int term_curr)
{
	union power_supply_propval prop = {0, };
	int ret = 0;

	nopmi_chg_jeita->bbc_psy = power_supply_get_by_name("bbc");
	if (!nopmi_chg_jeita->bbc_psy) {
		pr_err("usb supply not found, defer probe\n");
		return -EINVAL;
	}
	prop.intval = term_curr;
	ret = power_supply_set_property(nopmi_chg_jeita->bbc_psy,
				POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &prop);
	if (ret < 0) {
		pr_err("couldn't set term_current property, ret=%d\n", ret);
		return -EINVAL;
	}
	pr_info("set term_current is %d\n", prop.intval);
	return 0;
}

/*static int nopmi_chg_jeita_set_charger_enabled(struct nopmi_chg_jeita_st *nopmi_chg_jeita, bool enabled)
{

	union power_supply_propval prop = {0, };
	int ret = 0;

	pr_err("2021.09.10 wsy start %s:enabled is %d\n", __func__, enabled);
	nopmi_chg_jeita->bbc_psy = power_supply_get_by_name("bbc");
	if (!nopmi_chg_jeita->bbc_psy) {
		pr_err("bbc supply not found, defer probe\n");
		return -EINVAL;
	}
	prop.intval = enabled;
	ret = power_supply_set_property(nopmi_chg_jeita->bbc_psy,
				POWER_SUPPLY_PROP_CHARGE_ENABLED, &prop);
	if (ret < 0) {
		pr_err("couldn't set voltage enabled, ret=%d\n", ret);
		return -EINVAL;
	}
	pr_err("2021.09.10 wsy end %s:enabled is %d\n", __func__, prop.intval);
	return 0;

}
*/
//add function for when charging and vbat > 4.1Vsuddenly temp > 480, batt ovp lead to reboot
static int batt_nopmi_chg_jeita_get_batt_voltage(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
	union power_supply_propval prop = {0, };
	int ret = 0;
	int volt = 0;

    if(!nopmi_chg_jeita->bms_psy)
    {
    	nopmi_chg_jeita->bms_psy = power_supply_get_by_name("bms");
        if (!nopmi_chg_jeita->bms_psy) {
        	pr_err("bms supply not found, defer probe\n");
        	return -EINVAL;
        }
	}

	ret = power_supply_get_property(nopmi_chg_jeita->bms_psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	if (ret < 0) {
		pr_err("couldn't read voltage property, ret=%d\n", ret);
		return -EINVAL;
	}
	volt = prop.intval/1000;

	pr_info("get_bat_voltage is %d\n", volt);
	return volt;
}
static void nopmi_chg_jeita_prevent_battery_ovp(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
	int batt_voltage = 0;
	struct sw_jeita_data *sw_jeita = nopmi_chg_jeita->sw_jeita;

	batt_voltage = batt_nopmi_chg_jeita_get_batt_voltage(nopmi_chg_jeita);

	if(batt_voltage >= (nopmi_chg_jeita->dt.jeita_temp_t3_to_t4_cv + 10)){ //batt_voltage >= (4.1 + 0.01)V
		sw_jeita->charging = false;
		sw_jeita->cv = nopmi_chg_jeita->dt.normal_charge_voltage;
	} else { //batt_voltage < (4.1 + 0.01)V
		sw_jeita->cv = nopmi_chg_jeita->dt.jeita_temp_t3_to_t4_cv;
	}
}

static void nopmi_chg_handle_jeita_current(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
	int ret = 0;
	static int jeita_current_limit = TEMP_T2_TO_T3_FCC;
	int chg1_cv = 0;
	int term_curr_pre = 0;
	int pd_active = 0;
	int pd_verified = 0;
	struct sw_jeita_data *sw_jeita = nopmi_chg_jeita->sw_jeita;
	union power_supply_propval prop = {0, };
	static int fast_charge_mode = 0;
	//struct power_supply *sc8551_psy;
   // union power_supply_propval pval = {0, };
    int cp_charging_enabled = 0; //this flag used by jeta:nopmi_chg_jeita.c to set sw_chip fv && used by fg_chip do another soc
    struct nopmi_chg *chg = container_of(nopmi_chg_jeita, struct nopmi_chg, jeita_ctl);

    //pr_info("enter.\n");

	sw_jeita->pre_sm = sw_jeita->sm;
	sw_jeita->charging = true;
	/* JEITA battery temp Standard */
	if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_t4_thres) {
		pr_info("[SW_JEITA] Battery Temperature(%d) Battery Over high Temperature(%d) !!\n",
			nopmi_chg_jeita->battery_temp,
			nopmi_chg_jeita->dt.temp_t4_thres);
		sw_jeita->sm = TEMP_ABOVE_T4;
		sw_jeita->charging = false;
	} else if (nopmi_chg_jeita->battery_temp > nopmi_chg_jeita->dt.temp_t3_thres) {
		/* control 45 degree to normal behavior */
		if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_t4_thres_minus_x_degree) {
			pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d,not allow charging yet!!\n",
				nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_t4_thres_minus_x_degree,
				nopmi_chg_jeita->dt.temp_t4_thres);
			sw_jeita->charging = false;
		} else {
			pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d !!\n",
				nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_t3_thres,
				nopmi_chg_jeita->dt.temp_t4_thres);
			sw_jeita->sm = TEMP_T3_TO_T4;
			jeita_current_limit = nopmi_chg_jeita->dt.temp_t3_to_t4_fcc;
		}
	} else if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_t2_thres) {
		if (((sw_jeita->sm == TEMP_T3_TO_T4)
		     && (nopmi_chg_jeita->battery_temp
			 >= nopmi_chg_jeita->dt.temp_t3_thres_minus_x_degree))
		    || ((sw_jeita->sm == TEMP_T1P5_TO_T2)
			&& (nopmi_chg_jeita->battery_temp
			    <= nopmi_chg_jeita->dt.temp_t2_thres_plus_x_degree))) {
			pr_info("[SW_JEITA] Battery Temperature not recovery to normal temperature charging mode yet!!\n");
		} else {
			pr_info("[SW_JEITA] Battery Normal Temperature(%d) between %d and %d !!\n",
		                nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_t2_thres,
				nopmi_chg_jeita->dt.temp_t3_thres);
			sw_jeita->sm = TEMP_T2_TO_T3;
			jeita_current_limit = nopmi_chg_jeita->dt.temp_t2_to_t3_fcc;
		}
	} else if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_t1p5_thres) {
		if ((sw_jeita->sm == TEMP_T1_TO_T1P5
		     || sw_jeita->sm == TEMP_T0_TO_T1)
		    && (nopmi_chg_jeita->battery_temp
			<= nopmi_chg_jeita->dt.temp_t1p5_thres_plus_x_degree)) {
			if (sw_jeita->sm == TEMP_T1_TO_T1P5) {
				pr_info("[SW_JEITA] Battery Normal Temperatur(%d) between %d and %d !!\n",
					nopmi_chg_jeita->battery_temp,
					nopmi_chg_jeita->dt.temp_t1p5_thres_plus_x_degree,
					nopmi_chg_jeita->dt.temp_t2_thres);
			}
			if (sw_jeita->sm == TEMP_T0_TO_T1) {
				pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d !!\n",
					nopmi_chg_jeita->battery_temp,
					nopmi_chg_jeita->dt.temp_t1_thres_plus_x_degree,
					nopmi_chg_jeita->dt.temp_t1p5_thres);
			}
			if (sw_jeita->sm == TEMP_TN1_TO_T0) {
				pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d !!\n",
				 nopmi_chg_jeita->battery_temp,
					nopmi_chg_jeita->dt.temp_t0_thres_plus_x_degree,
					nopmi_chg_jeita->dt.temp_tn1_thres);
			}
		} else {
			pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d !!\n",
				nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_t1p5_thres,
				nopmi_chg_jeita->dt.temp_t2_thres);
			sw_jeita->sm = TEMP_T1P5_TO_T2;
			jeita_current_limit = nopmi_chg_jeita->dt.temp_t1p5_to_t2_fcc;
		}
	} else if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_t1_thres) {
		if ((sw_jeita->sm == TEMP_T0_TO_T1
			|| sw_jeita->sm == TEMP_BELOW_T0
			|| sw_jeita->sm == TEMP_TN1_TO_T0)
			&& (nopmi_chg_jeita->battery_temp
			<= nopmi_chg_jeita->dt.temp_t1_thres_plus_x_degree)) {
		    if (sw_jeita->sm == TEMP_T0_TO_T1) {
			pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d !!\n",
				 nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_t1_thres_plus_x_degree,
				nopmi_chg_jeita->dt.temp_t1p5_thres);
			}
			if (sw_jeita->sm == TEMP_BELOW_T0) {
			pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d,not allow charging yet!!\n",
				nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_tn1_thres,
				nopmi_chg_jeita->dt.temp_tn1_thres_plus_x_degree);
				sw_jeita->charging = false;
			}
		} else {
			pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d !!\n",
				nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_t1_thres,
				nopmi_chg_jeita->dt.temp_t1p5_thres);
			sw_jeita->sm = TEMP_T1_TO_T1P5;
			jeita_current_limit = nopmi_chg_jeita->dt.temp_t1_to_t1p5_fcc;
		}
	} else if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_t0_thres) {
		if ((sw_jeita->sm == TEMP_BELOW_T0
			|| sw_jeita->sm == TEMP_TN1_TO_T0)
			&& (nopmi_chg_jeita->battery_temp
			<= nopmi_chg_jeita->dt.temp_t0_thres_plus_x_degree)) {
			if (sw_jeita->sm == TEMP_BELOW_T0) {
			pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d,not allow charging yet!!\n",
				nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_tn1_thres,
				nopmi_chg_jeita->dt.temp_tn1_thres_plus_x_degree);
				sw_jeita->charging = false;
			} else if (sw_jeita->sm == TEMP_TN1_TO_T0) {
			pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d !!\n",
				nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_t0_thres_plus_x_degree,
				nopmi_chg_jeita->dt.temp_tn1_thres);
			}
		} else {
			pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d !!\n",
				nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_t0_thres,
				nopmi_chg_jeita->dt.temp_t1_thres);
			sw_jeita->sm = TEMP_T0_TO_T1;
			jeita_current_limit = nopmi_chg_jeita->dt.temp_t0_to_t1_fcc;
		}
	} else if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_tn1_thres) {
		if ((sw_jeita->sm == TEMP_BELOW_T0)
			&& (nopmi_chg_jeita->battery_temp
			<= nopmi_chg_jeita->dt.temp_tn1_thres_plus_x_degree)) {
			pr_info("[SW_JEITA] 1Battery Temperature(%d) between %d and %d,not allow charging yet!!\n",
				nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_tn1_thres,
				nopmi_chg_jeita->dt.temp_tn1_thres_plus_x_degree);
			sw_jeita->charging = false;
		} else {
			pr_info("[SW_JEITA] Battery Temperature(%d) between %d and %d !!\n",
				nopmi_chg_jeita->battery_temp,
				nopmi_chg_jeita->dt.temp_t0_thres,
				nopmi_chg_jeita->dt.temp_tn1_thres);
			sw_jeita->sm = TEMP_TN1_TO_T0;
			jeita_current_limit = nopmi_chg_jeita->dt.temp_tn1_to_t0_fcc;
		}
	} else {
		pr_info("[SW_JEITA]Battery Temperature(%d) Battery below low Temperature(%d) !!\n",
		        nopmi_chg_jeita->battery_temp,
			    nopmi_chg_jeita->dt.temp_t0_thres);
		sw_jeita->sm = TEMP_BELOW_T0;
		sw_jeita->charging = false;
	}

	if(nopmi_chg_jeita->fcc_votable)
	{
		vote(nopmi_chg_jeita->fcc_votable, JEITA_VOTER, true, jeita_current_limit);
	} else {
		ret = nopmi_chg_jeita_set_charger_current(nopmi_chg_jeita, jeita_current_limit);
	}

    ret = nopmi_chg_get_iio_channel(chg, NOPMI_BMS, FG_FASTCHARGE_MODE, &fast_charge_mode);

	if (ret != 0) {
        pr_err("couldn't get fastcharge mode property, ret=%d\n", ret);
        return;
	}

	if(!g_ffc_disable && fast_charge_mode && (sw_jeita->sm != TEMP_T2_TO_T3)){
		prop.intval = 0;
		fast_charge_mode = 0;
		ret = nopmi_chg_set_iio_channel(chg, NOPMI_BMS, FG_FASTCHARGE_MODE, fast_charge_mode);
        if (ret != 0) {
            pr_err("couldn't set fastcharge mode property, ret=%d\n", ret);
            return;
	    }
	}else if(!g_ffc_disable && !fast_charge_mode && (sw_jeita->sm == TEMP_T2_TO_T3)){
		prop.intval = 1;
		fast_charge_mode = 1;
		ret = nopmi_chg_set_iio_channel(chg, NOPMI_BMS, FG_FASTCHARGE_MODE, fast_charge_mode);
         if (ret != 0) {
            pr_err("couldn't set fastcharge mode property, ret=%d\n", ret);
            return;
	    }
	}
    /* add for update fastcharge mode, end*/
	pd_verified = adapter_dev_get_pd_verified();
	/* set CV after temperature changed */
	/* In normal range, we adjust CV dynamically */
	if (sw_jeita->sm != TEMP_T2_TO_T3) {
		if (sw_jeita->sm == TEMP_ABOVE_T4){
			sw_jeita->cv = nopmi_chg_jeita->dt.jeita_temp_above_t4_cv;
			nopmi_chg_jeita_prevent_battery_ovp(nopmi_chg_jeita);
		}else if (sw_jeita->sm == TEMP_T3_TO_T4){
			sw_jeita->cv = nopmi_chg_jeita->dt.jeita_temp_t3_to_t4_cv;
			nopmi_chg_jeita_prevent_battery_ovp(nopmi_chg_jeita);
		}else if (sw_jeita->sm == TEMP_T2_TO_T3)
			sw_jeita->cv = nopmi_chg_jeita->dt.normal_charge_voltage;
		else if (sw_jeita->sm == TEMP_T1P5_TO_T2)
			sw_jeita->cv = nopmi_chg_jeita->dt.jeita_temp_t1p5_to_t2_cv;
		else if (sw_jeita->sm == TEMP_T1_TO_T1P5)
			sw_jeita->cv = nopmi_chg_jeita->dt.jeita_temp_t1_to_t1p5_cv;
		else if (sw_jeita->sm == TEMP_T0_TO_T1)
			sw_jeita->cv = nopmi_chg_jeita->dt.jeita_temp_t0_to_t1_cv;
		else if (sw_jeita->sm == TEMP_TN1_TO_T0)
			sw_jeita->cv = nopmi_chg_jeita->dt.jeita_temp_tn1_to_t0_cv;
		else if (sw_jeita->sm == TEMP_BELOW_T0)
			sw_jeita->cv = nopmi_chg_jeita->dt.jeita_temp_below_t0_cv;
		else
			sw_jeita->cv = nopmi_chg_jeita->dt.normal_charge_voltage;
	} else {
			sw_jeita->cv = nopmi_chg_jeita->dt.normal_charge_voltage;
			/*Both temp is normal and FFC is enabled, then improve FV*/
			if(fast_charge_mode && !g_ffc_disable){
				if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type()){
					sw_jeita->cv = 4470; //for maxim chip
				}else{
					sw_jeita->cv = 4480; //for other pmic chips
				}
				if(!pd_verified){
					sw_jeita->cv = 4450; //for unverified pd
				}

			}
	}

	if (is_cp_chan_valid(chg, 0)) {
		ret = nopmi_chg_get_iio_channel(chg, NOPMI_CP_MASTER, CHARGE_PUMP_CHARGING_ENABLED, &cp_charging_enabled);
		if (ret < 0) {
			pr_err("%s:get sc8551 charging_enabled error.\n",__func__);
		} else {
			pr_info("%s:get sc8551 cp_charging_enabled:%d\n", __func__, cp_charging_enabled);
		}
	} else if (is_cp_chan_valid(chg, LN8000_IIO_CHANNEL_OFFSET)) {
		ret = nopmi_chg_get_iio_channel(chg, NOPMI_CP_MASTER, CHARGE_PUMP_LN_CHARGING_ENABLED, &cp_charging_enabled);
		if (ret < 0) {
			pr_err("%s:get ln8000 charging_enabled error.\n", __func__);
		} else {
			pr_info("%s:get ln8000 cp_charging_enabled:%d\n", __func__, cp_charging_enabled);
		}
	} else {
		pr_err("%s:charge pump not found!\n", __func__);
	}

	if(cp_charging_enabled) {
		if(NOPMI_CHARGER_IC_MAXIM != nopmi_get_charger_ic_type()) {
			pr_info("charge pump :sw_jeita->cv = 4608.\n");
			sw_jeita->cv = 4608; //if charger pump working set sw_chip fv:4608
		}
	}

	if(nopmi_chg_jeita->fv_votable) {
		chg1_cv =  get_effective_result(nopmi_chg_jeita->fv_votable);
	} else {
		chg1_cv = nopmi_chg_jeita_get_charger_voltage(nopmi_chg_jeita);
	}

	if (sw_jeita->cv != chg1_cv){
		if(nopmi_chg_jeita->fv_votable)
		{
			vote(nopmi_chg_jeita->fv_votable, JEITA_VOTER, true, sw_jeita->cv);
		} else {
			ret = nopmi_chg_jeita_set_charger_voltage(nopmi_chg_jeita, sw_jeita->cv);
			if (ret < 0)
				pr_err("Couldn't set cv to %d, rc:%d\n", sw_jeita->cv, ret);
		}
	}


	if(NOPMI_CHARGER_IC_MAXIM != nopmi_get_charger_ic_type()) {
		/* set term current after temperature changed */
		pd_active = nopmi_chg_jeita_get_pd_active(nopmi_chg_jeita);
		if(sw_jeita->sm == TEMP_T2_TO_T3 && pd_active == 2 && fast_charge_mode) {
			if (nopmi_chg_jeita->battery_temp >= 35) {
				if(nopmi_chg_jeita->battery_id == BATTERY_VENDOR_XWD) {
					sw_jeita->term_curr = 784;
				} else if(nopmi_chg_jeita->battery_id == BATTERY_VENDOR_GY) {
					sw_jeita->term_curr = 833;
				} else if(nopmi_chg_jeita->battery_id == BATTERY_VENDOR_NVT) {
					sw_jeita->term_curr = 833;
				} else {
					sw_jeita->term_curr = 784;
				}
			} else {
				if(nopmi_chg_jeita->battery_id == BATTERY_VENDOR_NVT) {
					sw_jeita->term_curr = 768;  //0.15C 735mA
				} else {
					sw_jeita->term_curr = 768;
				}
			}
		} else {
			sw_jeita->term_curr = 256;
		}
		if(!pd_verified){
			sw_jeita->term_curr = 256;
		}

		term_curr_pre = nopmi_chg_jeita_get_charger_term_current(nopmi_chg_jeita);
		if(sw_jeita->term_curr != term_curr_pre){
			ret = nopmi_chg_jeita_set_charger_term_current(nopmi_chg_jeita, sw_jeita->term_curr);
			if (ret < 0)
				pr_err("Couldn't set term curr to %d, rc:%d\n", sw_jeita->term_curr, ret);
		}

		pr_info("[SW_JEITA]preState:%d newState:%d temp:%d cv:%d,%d jeita_current_limit:%d, term_curr:%d,%d, batt_id:%d, pd_active:%d\n",
				sw_jeita->pre_sm, sw_jeita->sm, nopmi_chg_jeita->battery_temp,
				sw_jeita->cv, chg1_cv, jeita_current_limit, sw_jeita->term_curr,
				term_curr_pre, nopmi_chg_jeita->battery_id, pd_active);
	}else{
		pr_info("[SW_JEITA]preState:%d newState:%d temp:%d cv:%d,%d jeita_current_limit:%d\n",
				sw_jeita->pre_sm, sw_jeita->sm, nopmi_chg_jeita->battery_temp,
				sw_jeita->cv, chg1_cv, jeita_current_limit);
	}
}

static void nopmi_chg_handle_jeita(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
	int ret = 0;
	struct nopmi_chg *chg = container_of(nopmi_chg_jeita, struct nopmi_chg, jeita_ctl);
	nopmi_chg_jeita->battery_temp = nopmi_chg_jeita_get_bat_temperature(nopmi_chg_jeita);
	nopmi_chg_handle_jeita_current(nopmi_chg_jeita);
	if(nopmi_chg_jeita->sw_jeita->charging == false)
	{
		nopmi_chg_jeita->sw_jeita->can_recharging = true;
		switch(nopmi_get_charger_ic_type())
		{
			case NOPMI_CHARGER_IC_MAXIM:
				vote(nopmi_chg_jeita->chgctrl_votable, JEITA_CHG_VOTER, true, CHG_MODE_CHARGING_OFF);
				break;
			case NOPMI_CHARGER_IC_SYV:
				//nopmi_chg_jeita_set_charger_enabled(nopmi_chg_jeita, false);
				//main_set_charge_enable(false);
				ret = nopmi_chg_set_iio_channel(chg, NOPMI_MAIN, MAIN_CHARGE_ENABLED, 0);
				pr_info("%s ret=%d\n", __func__, ret);
				break;
			case NOPMI_CHARGER_IC_SC:
				break;
			default:
				break;
		}
	}
	else
	{
		if(nopmi_chg_jeita->sw_jeita->can_recharging == true)
		{
			switch(nopmi_get_charger_ic_type())
			{
				case NOPMI_CHARGER_IC_MAXIM:
					vote(nopmi_chg_jeita->chgctrl_votable, JEITA_CHG_VOTER, false, CHG_MODE_CHARGING_OFF);
					break;
				case NOPMI_CHARGER_IC_SYV:
					//nopmi_chg_jeita_set_charger_enabled(nopmi_chg_jeita, true);
					//main_set_charge_enable(true);
					ret = nopmi_chg_set_iio_channel(chg, NOPMI_MAIN, MAIN_CHARGE_ENABLED, 1);
					pr_info("%s ret=%d\n", __func__, ret);
					break;
				case NOPMI_CHARGER_IC_SC:
					break;
				default:
					break;
			}
			nopmi_chg_jeita->sw_jeita->can_recharging = false;
		}
	}
}

static void nopmi_chg_jeita_workfunc(struct work_struct *work)
{
	struct nopmi_chg_jeita_st *chg_jeita = container_of(work,
		struct nopmi_chg_jeita_st, jeita_work.work);

	pr_info("enter.\n");
	chg_jeita->usb_present = nopmi_chg_is_usb_present(chg_jeita->bbc_psy);
	if (!chg_jeita->usb_present)
		return;
	/* skip elapsed_us debounce for handling battery temperature */
	if(chg_jeita->dt.enable_sw_jeita == true)
	{
		nopmi_chg_handle_jeita(chg_jeita);
		chg_jeita->sw_jeita_start = true;
	}
	else
	{
		chg_jeita->sw_jeita_start = false;
	}

}

void start_nopmi_chg_jeita_workfunc(void)
{
	if(g_nopmi_chg_jeita)
	{
		schedule_delayed_work(&g_nopmi_chg_jeita->jeita_work,
			msecs_to_jiffies(JEITA_WORK_DELAY_MS));
	}
}

void stop_nopmi_chg_jeita_workfunc(void)
{
	if(g_nopmi_chg_jeita)
	{
		cancel_delayed_work_sync(&g_nopmi_chg_jeita->jeita_work);
		vote(g_nopmi_chg_jeita->fcc_votable, JEITA_VOTER, false, 0);
	}
}

static void nopmi_chg_jeita_state_init(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
	struct sw_jeita_data *sw_jeita = nopmi_chg_jeita->sw_jeita;

	if (nopmi_chg_jeita->dt.enable_sw_jeita == true) {
		nopmi_chg_jeita->battery_temp = nopmi_chg_jeita_get_bat_temperature(nopmi_chg_jeita);
		if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_t4_thres)
			sw_jeita->sm = TEMP_ABOVE_T4;
		else if (nopmi_chg_jeita->battery_temp > nopmi_chg_jeita->dt.temp_t3_thres)
			sw_jeita->sm = TEMP_T3_TO_T4;
		else if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_t2_thres)
			sw_jeita->sm = TEMP_T2_TO_T3;
		else if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_t1p5_thres)
			sw_jeita->sm = TEMP_T1P5_TO_T2;
		else if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_t1_thres)
			sw_jeita->sm = TEMP_T1_TO_T1P5;
		else if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_t0_thres)
			sw_jeita->sm = TEMP_T0_TO_T1;
		else if (nopmi_chg_jeita->battery_temp >= nopmi_chg_jeita->dt.temp_tn1_thres)
			sw_jeita->sm = TEMP_TN1_TO_T0;
		else
			sw_jeita->sm = TEMP_BELOW_T0;

		pr_info("[SW_JEITA] tmp:%d sm:%d\n",
			nopmi_chg_jeita->battery_temp, sw_jeita->sm);
	}
}

int nopmi_chg_jeita_init(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
	int rc = 0;
	g_nopmi_chg_jeita = nopmi_chg_jeita;

	pr_info("enter.\n");

	if (!nopmi_chg_jeita->sw_jeita ) {
		nopmi_chg_jeita->sw_jeita = kmalloc(sizeof(struct sw_jeita_data), GFP_KERNEL);
		if(!nopmi_chg_jeita->sw_jeita)
		{
			pr_err(" nopmi_chg_jeita_init Failed to allocate memory\n");
			return -ENOMEM;
		}
	}

	nopmi_chg_jeita->fcc_votable = find_votable("FCC");
	nopmi_chg_jeita->fv_votable = find_votable("FV");
	nopmi_chg_jeita->usb_icl_votable = find_votable("USB_ICL");
	nopmi_chg_jeita->chgctrl_votable = find_votable("CHG_CTRL");
	nopmi_chg_jeita_state_init(nopmi_chg_jeita);
	if(NOPMI_CHARGER_IC_MAXIM != nopmi_get_charger_ic_type()){
		nopmi_chg_jeita->battery_id = nopmi_chg_jeita_get_batt_id(nopmi_chg_jeita);
	}

	INIT_DELAYED_WORK(&nopmi_chg_jeita->jeita_work, nopmi_chg_jeita_workfunc);
	return rc;
}

int nopmi_chg_jeita_deinit(struct nopmi_chg_jeita_st *nopmi_chg_jeita)
{
	int rc = 0;
	g_nopmi_chg_jeita = NULL;

	pr_info("enter.\n");

	if (!nopmi_chg_jeita->sw_jeita ) {
		cancel_delayed_work_sync(&nopmi_chg_jeita->jeita_work);
		kfree(nopmi_chg_jeita->sw_jeita);
	}
	return rc;

}

