
#define pr_fmt(fmt)	"[USBPD-PM]: %s %d: " fmt, __func__, __LINE__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_charger.h>
#include <mt-plat/prop_chgalgo_class.h>
#include "pd_policy_manager.h"
#include "mtk_charger_intf.h"
#include <linux/timex.h>
#include <linux/rtc.h>

#define PCA_PPS_CMD_RETRY_COUNT	2
#ifdef CONFIG_MTK_DISABLE_TEMP_PROTECT
#define BATT_MAX_CHG_VOLT		4100
#else
#define BATT_MAX_CHG_VOLT		4470
#endif
#define BATT_CC_TO_CV_OFFST	50
#define BATT_CC_TO_CV_OFFST_1	5
#define BATT_SD_SECP_CURR		750
#define BATT_FAST_CHG_CURR		(12200)   //6100
#define BUS_OVP_THRESHOLD		(11800)          //11500
#define BUS_VOLT_INIT_UP		200

#define BAT_VOLT_LOOP_LMT		BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT		BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT		BUS_OVP_THRESHOLD

#define PM_WORK_RUN_INTERVAL		1000

#define CHG_BAT_TEMP_10		 		 100
#define CHG_BAT_TEMP_MIN      		 50
#define CHG_BAT_TEMP_15       		 150
#define FFC_BAT_TEMP_OFFSET   		 20
#define CHG_BAT_TEMP_MAX      		 480
/*
	battery JEITA
*/
#define EN_MI_THERMAL       (1)
#define BAT_TEMP_NEG10	    (-100)
#define BAT_TEMP_000	    0
#define BAT_TEMP_050	    50
#define BAT_TEMP_100	    100
#define BAT_TEMP_150	    150
#define BAT_TEMP_450	    450
#define BAT_TEMP_600	    600

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

#define BAT_CURR_6100MA       6000
#define BAT_CURR_5800MA       5800
#define BAT_CURR_5400MA       5400
#define BAT_CURR_5000MA       5000
#define BAT_CURR_4400MA       4400
#define BAT_CURR_4200MA       4200
#define BAT_CURR_4000MA       4000
#define BAT_CURR_3900MA       3900
#define BAT_CURR_3700MA       3700
#define BAT_CURR_3650MA       3650
#define BAT_CURR_2900MA       2900
#define BAT_CURR_2750MA       2750
#define BAT_CURR_2600MA       2600
#define BAT_CURR_2000MA       2000
#define BAT_CURR_1500MA       1500
#define BAT_CURR_1600MA       1600
#define BAT_CURR_1000MA       1000
#define BAT_CURR_100MA        100
#define BAT_CURR_50MA         50
#ifdef CONFIG_MTK_DISABLE_TEMP_PROTECT
#define CHG_CUR_VOLT          4100
#define CHG_CUR_VOLT_1        4100
#define CHG_CUR_VOLT1         4100
#define CHG_CUR_VOLT2         4100
#define CHG_CUR_VOLT3         4100
#define CHG_CUR_VOLT_NORMAL   4100
#else
#define CHG_CUR_VOLT       	  4200
#define CHG_CUR_VOLT_1        4150
#define CHG_CUR_VOLT1         4300
#define CHG_CUR_VOLT2         4400
#define CHG_CUR_VOLT3         4465
#define CHG_CUR_VOLT_NORMAL   4430
#endif
#define CHG_TEMP_STEP1        1
#define CHG_TEMP_STEP2        2
#define CHG_TEMP_STEP3        3
#define CHG_TEMP_STEP4        4
#define CHG_TEMP_STEP5        5
#define CHG_TEMP_STEP6        6
#define CHG_TEMP_OFFSET       10
#define CHG_CURR_2000MA       2000
#define CHG_CURR_3000MA       3000

#define SC89890H_ID 				4
extern int device_chipid;
extern int get_jeita_lcd_on_off(void);
extern bool get_charging_call_state(void);
enum {
	PM_ALGO_RET_OK,
	PM_ALGO_RET_THERM_FAULT,
	PM_ALGO_RET_OTHER_FAULT,
	PM_ALGO_RET_CHG_DISABLED,
	PM_ALGO_RET_TAPER_DONE,
};

static const struct pdpm_config pm_config = {
	.bat_volt_lp_lmt		= BAT_VOLT_LOOP_LMT,
	.bat_curr_lp_lmt		= BAT_CURR_LOOP_LMT,
	.bus_volt_lp_lmt		= BUS_VOLT_LOOP_LMT,
	.bus_curr_lp_lmt		= (BAT_CURR_LOOP_LMT >> 1),

	.fc2_taper_current		= 2000,
	.fc2_steps			= 1,

	.min_adapter_volt_required	= 10500,     //11v
	.min_adapter_curr_required	= 6200,

	.min_vbat_for_cp		= 3500,

	.cp_sec_enable          = true,
	.fc2_disable_sw			= true,
};

static struct usbpd_pm *__pdpm;

static int fc2_taper_timer;
static int ibus_lmt_change_timer;

/*******************************BAT API******************************/
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
				DPM_CHARGING_POLICY_PPS_IC, mV, mA, NULL);
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
	int apdo_max = 0, vol_max = 0, cur_max = 0, apdo_max_idx = 0;
	struct tcpm_power_cap_val apdo_cap = {0};
	u8 cap_idx;
	//u32 vta_meas, ita_meas, prog_mv;
	int apdo_val = 0;
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
		apdo_val =  ((apdo_cap.max_mv/100) * (apdo_cap.ma/100))/100;
		if (apdo_cap.max_mv < pm_config.min_adapter_volt_required &&
		    apdo_cap.ma < pm_config.min_adapter_curr_required) {
			if (apdo_max < apdo_val) {
				apdo_max = apdo_val;
				vol_max = apdo_cap.max_mv;
				cur_max = apdo_cap.ma;
				apdo_max_idx = cap_idx;
			}
			continue;
		}
		if (apdo_idx == -1) {
			if (apdo_max > apdo_val) {
				apdo_idx = apdo_max_idx;
				pr_err("select potential cap_idx[%d], vol_max = %d, cur_max = %d. \n",
										 cap_idx, vol_max, cur_max);
				pdpm->apdo_max_volt = vol_max;
				pdpm->apdo_max_curr = cur_max;
			} else {
				apdo_idx = cap_idx;
				pr_err("select potential cap_idx[%d], apdo_cap.max_mv = %d, apdo_cap.ma = %d. \n",
										 cap_idx, apdo_cap.max_mv, apdo_cap.ma);
				pdpm->apdo_max_volt = apdo_cap.max_mv;
				pdpm->apdo_max_curr = apdo_cap.ma;
			}

			if (apdo_cap.max_mv == 21000 && (apdo_cap.ma == 3000)) {
				pr_err("select deafult apdo_cap.max_mv = %d, apdo_cap.ma = %d. \n",
										pm_config.min_adapter_volt_required, apdo_cap.ma);
				pdpm->apdo_max_volt = pm_config.min_adapter_volt_required;
				pdpm->apdo_max_curr = apdo_cap.ma;
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

 bool usbpd_check_pps_status(void)
{
	int ret, apdo_idx = -1;
	struct tcpm_power_cap_val apdo_cap = {0};
	u8 cap_idx;

	pr_err("++\n");

	if (check_typec_attached_snk(__pdpm->tcpc) < 0)
		return false;

	if (!__pdpm->is_pps_en_unlock) {
		pr_err("pps en is locked\n");
		return false;
	}

	if (!tcpm_inquire_pd_pe_ready(__pdpm->tcpc)) {
		pr_err("PD PE not ready\n");
		return false;
	}

	/* select TA boundary */
	cap_idx = 0;
	while (1) {
		ret = tcpm_inquire_pd_source_apdo(__pdpm->tcpc,
						  TCPM_POWER_CAP_APDO_TYPE_PPS,
						  &cap_idx, &apdo_cap);
		if (ret != TCP_DPM_RET_SUCCESS) {
			pr_err("inquire pd apdo fail(%d)\n", ret);
			break;
		}

		pr_err("cap_idx[%d], %d mv ~ %d mv, %d ma, pl: %d\n", cap_idx,
			 apdo_cap.min_mv, apdo_cap.max_mv, apdo_cap.ma,
			 apdo_cap.pwr_limit);

		if (apdo_cap.max_mv < pm_config.min_adapter_volt_required)
			continue;
		if (apdo_idx == -1) {
			apdo_idx = cap_idx;
			pr_err("select potential cap_idx[%d]\n", cap_idx);
			__pdpm->apdo_max_volt = apdo_cap.max_mv;
			__pdpm->apdo_max_curr = apdo_cap.ma;
			if (apdo_cap.max_mv == 21000 && (apdo_cap.ma == 3000)) {
				pr_err("select deafult apdo_cap.max_mv = %d, apdo_cap.ma = %d. \n",
										pm_config.min_adapter_volt_required, apdo_cap.ma);
				__pdpm->apdo_max_volt = pm_config.min_adapter_volt_required;
				__pdpm->apdo_max_curr = apdo_cap.ma;
			}
			return true;
		}
	}
    return true;
}
static int usbpd_select_pdo(struct usbpd_pm *pdpm, u32 mV, u32 mA)
{
	int ret=0, cnt = 0;

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
		/*
				if failed at last time,get src cap
		*/
		if((ret != TCP_DPM_RET_SUCCESS) && (cnt == (PCA_PPS_CMD_RETRY_COUNT-1))){
			usbpd_check_pps_status();
			/*
			       update cap
			*/
			if(mV > pdpm->apdo_max_volt)
				mV = pdpm->apdo_max_volt;
			mA = pdpm->apdo_max_curr;
			pr_err("Update cap,mV = %d, mA = %d,apdo_max_volt = %d\n",mV,mA, pdpm->apdo_max_volt);
			tcpm_set_apdo_charging_policy(pdpm->tcpc,DPM_CHARGING_POLICY_PPS_IC, mV, mA, NULL);
		}
		
	} while ((ret != TCP_DPM_RET_SUCCESS) && (cnt < PCA_PPS_CMD_RETRY_COUNT));

	if (ret != TCP_DPM_RET_SUCCESS)
		pr_err("fail(%d)\n", ret);
	return ret > 0 ? -ret : ret;
}

static void usbpd_workfunc(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, pd_work.work);

	if (pdpm->cp.vbus_volt <= 7000) {
		if (pdpm->tcpc)
			tcpm_dpm_pd_request(pdpm->tcpc, 9000, 2000, NULL);
		 pr_err("pd request 9V/2A\n");
	}

}

static int pca_pps_tcp_notifier_call(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
    pr_err("%s event :%d, pd_state.connecte : %d \n",__func__,event,noti->pd_state.connected);
	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			pr_err("detached\n");
			__pdpm->is_pps_en_unlock = false;
			__pdpm->hrst_cnt = 0;
			__pdpm->state = PD_PM_STATE_FC2_EXIT;
			break;
		case PD_CONNECT_HARD_RESET:
			__pdpm->hrst_cnt++;
			pr_err("pd hardreset, cnt = %d\n",
				 __pdpm->hrst_cnt);
			__pdpm->is_pps_en_unlock = false;
			break;
		case PD_CONNECT_PE_READY_SNK:
		case PD_CONNECT_PE_READY_SNK_PD30:
			cancel_delayed_work(&__pdpm->pd_work);
        	schedule_delayed_work(&__pdpm->pd_work, msecs_to_jiffies(6000));
        	break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			if (__pdpm->hrst_cnt < 10) {
				pr_err("en unlock\n");
				__pdpm->is_pps_en_unlock = true;
				__pdpm->state = PD_PM_STATE_ENTRY;
			}
			break;
		default:
			break;
		}
	default:
		break;
	}
	if (noti->pd_state.connected != PD_CONNECT_PE_READY_SNK &&
		 (noti->pd_state.connected != PD_CONNECT_PE_READY_SNK_PD30)) {
		cancel_delayed_work(&__pdpm->pd_work);
	}
	if(__pdpm->usb_psy)
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
 #if 0
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
	//ichg = (90 * pdpm->cp.vbus_volt * aicr / 100) / pdpm->cp.vbat_volt;
	ichg = aicr*90/100;
	ret = charger_dev_set_charging_current(pdpm->sw_chg, ichg * 1000);
	if (ret < 0) {
		pr_err("set_ichg fail(%d)\n", ret);
		return ret;
	}

	pr_info("AICR = %dmA, ICHG = %dmA\n", aicr, ichg);
	return 0;

}
#endif
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

#if 0
	ret = charger_dev_get_adc(pdpm->sw_chg, ADC_CHANNEL_IBUS, &ibus,
				   &ibus);
	if (ret < 0) {
		pr_err("get swchg ibus fail(%d)\n", ret);
		return ret;
	}
	
#else
	charger_dev_get_ibus(pdpm->sw_chg,&ibus);
#endif
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
		}else{
			if(!pdpm->chg_consumer){
				pdpm->chg_consumer = charger_manager_get_by_name(&pdpm->sw_chg->dev,"charger_port1");
				if (!pdpm->chg_consumer) {
					pr_err("%s: get charger consumer device failed\n", __func__);
				}
			}
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

static void usbpd_check_apdo_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->apdo_psy) {
		pdpm->apdo_psy = power_supply_get_by_name("usb");
		if (!pdpm->apdo_psy)
			pr_err("apdo psy not found!\n");
	}
}

static void usbpd_check_cp_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->cp_psy) {
		pdpm->cp_psy = power_supply_get_by_name("ln8000-standalone");
		if (pdpm->cp_psy) {
			pdpm->cp_index = 1;
			return;
		}

		pdpm->cp_psy = power_supply_get_by_name("sc8551-standalone");
		if (pdpm->cp_psy) {
			pdpm->cp_index = 2;
		} else {
			pr_err("cp_psy not found\n");
		}
	}
}

static void usbpd_check_cp_sec_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->cp_sec_psy) {
        pdpm->cp_sec_psy = power_supply_get_by_name("ln8000-standalone-2");
        if (pdpm->cp_sec_psy) {
			pdpm->cp_sec_index = 1;
			return;
		}

        pdpm->cp_sec_psy = power_supply_get_by_name("sc8551_2-standalone");
        if (pdpm->cp_sec_psy) {
			pdpm->cp_sec_index = 2;
		} else {
			pr_err("cp_sec_psy not found\n");
		}
    }
}

static void usbpd_pm_update_cp_status(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};
	struct power_supply *bms = NULL;

	usbpd_check_cp_psy(pdpm);

	if (!pdpm->cp_psy)
		return;

	bms = power_supply_get_by_name("bms");
	if (!bms) {
  		pr_err("%s %d: get power supply failed!\n", __func__, __LINE__);
		return;
	}

	ret = power_supply_get_property(bms,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (!ret)
		pdpm->cp.vbat_volt = val.intval / 1000;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_BATTERY_CURRENT, &val);
	if (!ret)
		pdpm->cp.ibat_curr = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_BUS_VOLTAGE, &val);
	if (!ret)
		pdpm->cp.vbus_volt = val.intval;

	ret = power_supply_get_property(pdpm->cp_psy,
			POWER_SUPPLY_PROP_SC_BUS_CURRENT, &val);
	if (!ret)
		pdpm->cp.ibus_curr = val.intval;
	pr_err("cp.ibus_curr : %d\n",val.intval);
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
			POWER_SUPPLY_PROP_SC_BATTERY_VOLTAGE, &val);
	if (!ret)
		pdpm->cp_sec.vbat_volt = val.intval;

  	ret = power_supply_get_property(pdpm->cp_sec_psy,
  			POWER_SUPPLY_PROP_SC_BATTERY_CURRENT, &val);
  	if (!ret)
  		pdpm->cp_sec.ibat_curr = val.intval;

	ret = power_supply_get_property(pdpm->cp_sec_psy,
			POWER_SUPPLY_PROP_SC_BUS_CURRENT, &val);
	if (!ret)
		pdpm->cp_sec.ibus_curr = val.intval;

	ret = power_supply_get_property(pdpm->cp_sec_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (!ret)
		pdpm->cp_sec.charge_enabled = val.intval;
	pr_err("cp_sec.charge_enable : %d , cp_sec.ibus_curr : %d\n",pdpm->cp_sec.charge_enabled ,val.intval);
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
	if (!pdpm->cp_sec_psy){
		return -ENODEV;
	}

	val.intval = enable;
	ret = power_supply_set_property(pdpm->cp_sec_psy,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	return ret;
}

static void usbpd_pm_set_chgdev_info(struct usbpd_pm *pdpm)
{
	if (pdpm->sw_chg) {
		charger_dev_set_input_current(pdpm->sw_chg, 100000);
		charger_dev_set_constant_voltage(pdpm->sw_chg, 4608000);
		charger_dev_enable_termination(pdpm->sw_chg, false);
	}
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
	union power_supply_propval pval = {0, };
	struct power_supply *battery_psy;
	int real_apdo = 0;

	retValue = usbpd_get_pps_status(pdpm);
	if (retValue)
		pdpm->pps_supported = true;
	else
		pdpm->pps_supported = false;

	if (pdpm->pps_supported) {

		pdpm->lcdon_curr_step = 0;
		pdpm->lcdoff_curr_step = 0;
		pr_notice("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d\n",
				pdpm->apdo_selected_pdo,
				pdpm->apdo_max_volt,
				pdpm->apdo_max_curr);
		real_apdo = ((pdpm->apdo_max_volt/100) * (pdpm->apdo_max_curr/100))/100;
		pval.intval = real_apdo;
		power_supply_set_property(pdpm->apdo_psy,
				POWER_SUPPLY_PROP_APDO_MAX, &pval);

		power_supply_changed(pdpm->apdo_psy);
		battery_psy = power_supply_get_by_name("battery");
		if (NULL != battery_psy)
			power_supply_changed(battery_psy);
		else
			pr_err("battery_psy is null\n");
	}
	else
		pr_notice("Not qualified PPS adapter\n");

}


#define TAPER_TIMEOUT	(5000 / PM_WORK_RUN_INTERVAL)
#define IBUS_CHANGE_TIMEOUT  (500 / PM_WORK_RUN_INTERVAL)

static int bat_step(struct usbpd_pm *pdpm, int cur) {
	int step = 0;
	int ibat_total = battery_get_bat_current() / 1000;

	if (ibat_total < (cur - BAT_CURR_100MA))
		step = pm_config.fc2_steps;
	else if (ibat_total > (cur + BAT_CURR_50MA))
		step = -pm_config.fc2_steps;

	return step;
}
#if 0
static int bat_lcdon_temp(struct usbpd_pm *pdpm, int temp)
{
	int bat_temp = temp;
	int step_ibat = 1;

	if (bat_temp < BAT_TEMP_300) {
		if (!pdpm->lcdon_curr_step)
			step_ibat = bat_step(pdpm, BAT_CURR_6000MA);
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
		}
	} else if (bat_temp >= BAT_TEMP_340 && bat_temp < BAT_TEMP_370) {
		if (pdpm->lcdon_curr_step <= CHG_TEMP_STEP2) {
			pdpm->lcdon_curr_step = CHG_TEMP_STEP2;
			step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
		} else if (pdpm->lcdon_curr_step == CHG_TEMP_STEP3) {
			if (bat_temp <= BAT_TEMP_370 - CHG_TEMP_OFFSET) {
				pdpm->lcdon_curr_step = CHG_TEMP_STEP2;
				step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_2800MA);
			}
		}
	} else if (bat_temp >= BAT_TEMP_370 && bat_temp < BAT_TEMP_390) {
		if (pdpm->lcdon_curr_step <= CHG_TEMP_STEP3) {
			pdpm->lcdon_curr_step = CHG_TEMP_STEP3;
			step_ibat = bat_step(pdpm, BAT_CURR_2800MA);
		} else if (pdpm->lcdon_curr_step == CHG_TEMP_STEP4) {
			if (bat_temp <= BAT_TEMP_390 - CHG_TEMP_OFFSET) {
				pdpm->lcdon_curr_step = CHG_TEMP_STEP3;
				step_ibat = bat_step(pdpm, BAT_CURR_2800MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_2500MA);
			}
		}
	} else if (bat_temp >= BAT_TEMP_390 && bat_temp < BAT_TEMP_410) {
		if (pdpm->lcdon_curr_step <= CHG_TEMP_STEP4) {
			pdpm->lcdon_curr_step = CHG_TEMP_STEP4;
			step_ibat = bat_step(pdpm, BAT_CURR_2500MA);
		} else if (pdpm->lcdon_curr_step == CHG_TEMP_STEP5) {
			if (bat_temp <= BAT_TEMP_410 - CHG_TEMP_OFFSET) {
				pdpm->lcdon_curr_step = CHG_TEMP_STEP4;
				step_ibat = bat_step(pdpm, BAT_CURR_2500MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_2000MA);
			}
		}
	} else if (bat_temp >= BAT_TEMP_440) {
		pdpm->pps_temp_flag = false;
	}
	pr_err("[%s]lcdon_curr_step=%d\n", __func__, pdpm->lcdon_curr_step);
	return step_ibat;
}

static int bat_lcdoff_temp(struct usbpd_pm *pdpm, int temp)
{
	int bat_temp = temp;
	int step_ibat = 1;

	if (bat_temp < BAT_TEMP_380) {
		if (!pdpm->lcdoff_curr_step)
			step_ibat = bat_step(pdpm, BAT_CURR_6000MA);
		else
			step_ibat = bat_step(pdpm, BAT_CURR_5000MA);
	} else if (bat_temp >= BAT_TEMP_380 && bat_temp < BAT_TEMP_390) {
		if (pdpm->lcdoff_curr_step <= CHG_TEMP_STEP1) {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP1;
			step_ibat = bat_step(pdpm, BAT_CURR_5000MA);
		} else if (pdpm->lcdoff_curr_step == CHG_TEMP_STEP2) {
			if (bat_temp <= BAT_TEMP_390 - CHG_TEMP_OFFSET) {
				pdpm->lcdoff_curr_step = CHG_TEMP_STEP1;
				step_ibat = bat_step(pdpm, BAT_CURR_5000MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_4500MA);
			}
		}
	} else if (bat_temp >= BAT_TEMP_390 && bat_temp < BAT_TEMP_400) {
		if (pdpm->lcdoff_curr_step <= CHG_TEMP_STEP2) {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP2;
			step_ibat = bat_step(pdpm, BAT_CURR_4500MA);
		} else if (pdpm->lcdoff_curr_step == CHG_TEMP_STEP3) {
			if (bat_temp <= BAT_TEMP_400 - CHG_TEMP_OFFSET) {
				pdpm->lcdoff_curr_step = CHG_TEMP_STEP2;
				step_ibat = bat_step(pdpm, BAT_CURR_4500MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_4000MA);
			}
		}
	} else if (bat_temp >= BAT_TEMP_400 && bat_temp < BAT_TEMP_410) {
		if (pdpm->lcdoff_curr_step <= CHG_TEMP_STEP3) {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP3;
			step_ibat = bat_step(pdpm, BAT_CURR_4000MA);
		} else if (pdpm->lcdoff_curr_step == CHG_TEMP_STEP4) {
			if (bat_temp <= BAT_TEMP_400 - CHG_TEMP_OFFSET) {
				pdpm->lcdoff_curr_step = CHG_TEMP_STEP3;
				step_ibat = bat_step(pdpm, BAT_CURR_4000MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_3500MA);
			}
		}
	} else if (bat_temp >= BAT_TEMP_410 && bat_temp < BAT_TEMP_420) {
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
		}
	} else if (bat_temp >= BAT_TEMP_420 && bat_temp < BAT_TEMP_430) {
		if (pdpm->lcdoff_curr_step <= CHG_TEMP_STEP5) {
			pdpm->lcdoff_curr_step = CHG_TEMP_STEP5;
			step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
		} else if (pdpm->lcdoff_curr_step == CHG_TEMP_STEP6) {
			if (bat_temp <= BAT_TEMP_420 - CHG_TEMP_OFFSET) {
				pdpm->lcdoff_curr_step = CHG_TEMP_STEP5;
				step_ibat = bat_step(pdpm, BAT_CURR_3500MA);
			} else {
				step_ibat = bat_step(pdpm, BAT_CURR_3000MA);
			}
		}
	} else if (bat_temp >= BAT_TEMP_430) {
		pdpm->lcdoff_curr_step = CHG_TEMP_STEP6;
		step_ibat = bat_step(pdpm, BAT_CURR_2500MA);
	}

	pr_err("[%s]lcdoff_curr_step=%d , step_ibat : %d\n", __func__, pdpm->lcdoff_curr_step , step_ibat);
	return step_ibat;
}
#endif

static int get_battery_authentic(void) {
         int ret = 0;
         union power_supply_propval propval;
         struct power_supply *bms = NULL;

         bms = power_supply_get_by_name("bms");
         if (!bms) {
                 pr_err("%s: get power supply failed\n", __func__);
                 return 0;
         }
	 ret = power_supply_get_property(bms, POWER_SUPPLY_PROP_AUTHENTIC, &propval);
         if (ret < 0) {
                 pr_err("%s: psy type failed, ret = %d\n", __func__, ret);
                 return 0;
         }
         return propval.intval;
}

static int get_battery_temp(void)
{
	struct power_supply *battery_psy;
	union power_supply_propval pval = {0, };

	battery_psy = power_supply_get_by_name("bms");
	if(battery_psy)
		power_supply_get_property(battery_psy,
			POWER_SUPPLY_PROP_TEMP, &pval);
	return pval.intval;
}

static int get_battery_cycle_count(void)
{
	struct power_supply *battery_psy;
	union power_supply_propval pval = {0, };

	battery_psy = power_supply_get_by_name("bms");
	if(battery_psy)
		power_supply_get_property(battery_psy,
			POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	return pval.intval;
}

static int battery_sw_jeita(struct usbpd_pm *pdpm)
{
	int step_temp = 0;
	int step_ibat = 0;
	int step_vbat = 0;
	int bat_temp = 0;
	struct charger_manager *info=NULL;
	struct charger_data *pdata = NULL;
	int  temp_thermal_input_current = 0;
	int  temp_thermal_charging_current = 0;
	int  temp_thermal_mitigation_current = 0;
	int cycle_volt = 0, cycle_count = 0;

	cycle_count = get_battery_cycle_count();
	if(pdpm->chg_consumer){
		info = pdpm->chg_consumer->cm;
		if(info){
			pdata = &info->chg1_data;
			temp_thermal_charging_current = pdata->thermal_charging_current_limit/2000;
			temp_thermal_input_current = pdata->thermal_input_current_limit/2000;
			temp_thermal_mitigation_current = info->thermal_mitigation_current/2000;
			pr_err("[%d][%d][%d]\n",temp_thermal_input_current,temp_thermal_charging_current,temp_thermal_mitigation_current);
		}
	}	
	bat_temp = get_battery_temp();
	if (bat_temp >= CHG_BAT_TEMP_MIN && bat_temp <= CHG_BAT_TEMP_MAX) {
		pdpm->pps_temp_flag = true;
		if (cycle_count <= 100) {
			cycle_volt = CHG_CUR_VOLT;
		} else {
			cycle_volt = CHG_CUR_VOLT_1;
		}
		if (pdpm->cp.vbat_volt < cycle_volt) {
			step_vbat = bat_step(pdpm, BAT_CURR_6100MA * 2);
			pdpm->cp.set_ibat_cur = BAT_CURR_6100MA * 2;
		} else if (pdpm->cp.vbat_volt >= cycle_volt && pdpm->cp.vbat_volt < CHG_CUR_VOLT1) {
			step_vbat = bat_step(pdpm, BAT_CURR_4400MA * 2);
			pdpm->cp.set_ibat_cur = BAT_CURR_4400MA * 2;
		} else if (pdpm->cp.vbat_volt >= CHG_CUR_VOLT1 && pdpm->cp.vbat_volt < CHG_CUR_VOLT2) {
			step_vbat = bat_step(pdpm, BAT_CURR_3650MA * 2);
			pdpm->cp.set_ibat_cur = BAT_CURR_3650MA * 2;
		} else if (pdpm->cp.vbat_volt >= CHG_CUR_VOLT2 && pdpm->cp.vbat_volt < CHG_CUR_VOLT3) {
			step_vbat = bat_step(pdpm, BAT_CURR_2900MA * 2);
			pdpm->cp.set_ibat_cur = BAT_CURR_2900MA * 2;
		} else {
			step_vbat = bat_step(pdpm, BAT_CURR_100MA * 2);
			pdpm->cp.set_ibat_cur = BAT_CURR_100MA * 2;
		}

		if(bat_temp < CHG_BAT_TEMP_10) {
			if (device_chipid == SC89890H_ID) {
				step_vbat = bat_step(pdpm, BAT_CURR_1600MA * 2);
				pdpm->cp.set_ibat_cur = BAT_CURR_1600MA * 2;
			} else {
				step_vbat = bat_step(pdpm, BAT_CURR_1500MA * 2);
				pdpm->cp.set_ibat_cur = BAT_CURR_1500MA * 2;
			}
			if(pdpm->cp.vbat_volt >= CHG_CUR_VOLT_NORMAL)
				step_vbat = -1;
		}

		if(bat_temp < CHG_BAT_TEMP_15 && bat_temp >= CHG_BAT_TEMP_10) {
			step_vbat = bat_step(pdpm, BAT_CURR_2750MA * 2);
			pdpm->cp.set_ibat_cur = BAT_CURR_2750MA * 2;
			if(pdpm->cp.vbat_volt >= CHG_CUR_VOLT_NORMAL)
				step_vbat = -1;
		}

		if(pdpm->cp.vbat_volt > CHG_CUR_VOLT3)
			step_vbat = -pm_config.fc2_steps;

		if (!pdpm->pd_authen) {
			if (pdpm->cp.vbat_volt <= 4415) {
				if ((pdpm->apdo_max_volt / 1000) * (pdpm->apdo_max_curr / 1000) >= 33) {
					step_temp = bat_step(pdpm, BAT_CURR_2750MA * 2);
					pdpm->cp.set_ibat_cur = BAT_CURR_2750MA * 2;
				} else {
					step_temp = bat_step(pdpm, BAT_CURR_4200MA);
					pdpm->cp.set_ibat_cur = BAT_CURR_4200MA;
				}
			} else {
				step_temp = -pm_config.fc2_steps;
			}
			step_vbat = min(step_vbat, step_temp);
		}
#if EN_MI_THERMAL		
		if(temp_thermal_mitigation_current >= BAT_CURR_1000MA){
			step_ibat = bat_step(pdpm, temp_thermal_mitigation_current * 2);
			pdpm->cp.set_ibat_cur = temp_thermal_mitigation_current * 2;
		}else{
			pdpm->pps_temp_flag = false;
		}
#endif
	} else {
		pdpm->pps_temp_flag = false;
	}
	pr_err(">>>>temp %d,vbat_volt %d, pdpm->cp.ibus_curr %d, step_ibat %d, step_vbat %d, lcd_on %d, cycle_count %d, pd_authen %d, temp_thermal_mitigation_current :%d\n",
					bat_temp, pdpm->cp.vbat_volt, pdpm->cp.ibus_curr, step_ibat, step_vbat, get_jeita_lcd_on_off(),
					cycle_count, pdpm->pd_authen,temp_thermal_mitigation_current);
#if EN_MI_THERMAL
	if(temp_thermal_mitigation_current >= BAT_CURR_1000MA)
		return min(step_vbat, step_ibat);
	else
		return step_vbat;
#else
	return step_vbat;
#endif
}

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
	int ibat_total = battery_get_bat_current() / 1000;
	static int ibus_limit;
	int batt_temp;

	pr_err("%s START!",__func__);
	if (ibus_limit == 0)
		ibus_limit = pm_config.bus_curr_lp_lmt;// + 400;

	/* reduce bus current in cv loop */
	if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - BATT_CC_TO_CV_OFFST) {
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
		step_vbat = pm_config.fc2_steps;

	/* battery charge current loop*/
	if (ibat_total < pm_config.bat_curr_lp_lmt)
		step_ibat = pm_config.fc2_steps;
	else if (ibat_total > pm_config.bat_curr_lp_lmt + 100)
		step_ibat = -pm_config.fc2_steps;

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

	/* check if cp disabled due to other reason*/
	usbpd_pm_check_cp_enabled(pdpm);
	pr_err(">>>>cp enable bit %d ,ibus_total : %d , ibus_limit : %d\n", pdpm->cp.charge_enabled, ibus_total,ibus_limit);

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
		pr_notice("cp.charge_enabled:%d  vbus_error_low:%d  vbus_error_high:%d,cp_sec.charge_enabled:%d\n",
				pdpm->cp.charge_enabled, pdpm->cp.vbus_error_low, pdpm->cp.vbus_error_high,pdpm->cp_sec.charge_enabled);
		return PM_ALGO_RET_CHG_DISABLED;
	}

	batt_temp = get_battery_temp();
    if((batt_temp < 150) || !pdpm->pd_authen)
        pdpm->cp.batt_volt_max = pm_config.bat_volt_lp_lmt - 22;
    else
	    pdpm->cp.batt_volt_max = pm_config.bat_volt_lp_lmt;

	/* charge pump taper charge */
	if (((pdpm->cp.vbat_volt >= (pdpm->cp.batt_volt_max - BATT_CC_TO_CV_OFFST))
			&& (ibat_total <= pm_config.fc2_taper_current)) || (mtk_get_battery_capacity() == 100)) {
		if (fc2_taper_timer++ > TAPER_TIMEOUT) {
			pr_notice("charge pump taper charging done\n");
			fc2_taper_timer = 0;
			charger_dev_enable(pdpm->sw_chg, true);
			charger_dev_set_input_current(pdpm->sw_chg, CHG_CURR_2000MA*1000);
			charger_dev_set_charging_current(pdpm->sw_chg, CHG_CURR_2000MA*1000);
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
	int retSelect = 0;
	int rc = 0;
	static int tune_vbus_retry;
	static bool stop_sw;
	static bool recover;
	int bat_temp;
	int ibat_total = battery_get_bat_current();
	struct timex  txc;
	struct rtc_time tm;
	struct charger_manager *info=NULL;
	bool pre_cp_status = false;
	static int entry3_retry_count = 0;
#if EN_MI_THERMAL
	struct charger_data *pdata = NULL;
	int  temp_thermal_input_current = 0;
	int  temp_thermal_charging_current = 0;
	int  temp_thermal_mitigation_current = 0;
#endif
	int batt_temp;

	do_gettimeofday(&(txc.time));
	rtc_time_to_tm(txc.time.tv_sec,&tm);
	pr_err("%s START!",__func__);
	bat_temp = get_battery_temp();
	pr_err(">>>>>>>>>>>state phase :%d\n", pdpm->state);
	pr_err("charge_Info: time :%d-%d-%d %d:%d:%d vbat_vol_1 %d, vbat_vol_2 %d, ibat_curr_1 %d, ibat_curr_2 %d, ibat_total %d, Vbus %d, Ibus %d\n",
				tm.tm_year+1900,tm.tm_mon+1, tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec,
 				pdpm->cp.vbat_volt, pdpm->cp_sec.vbat_volt, pdpm->cp.ibat_curr,
				pdpm->cp_sec.ibat_curr,ibat_total,pdpm->cp.vbus_volt, pdpm->cp.ibus_curr + pdpm->cp_sec.ibus_curr);

	/*
		check cp charger apdo
	*/
	usbpd_check_pps_status();
	switch (pdpm->state) {
	case PD_PM_STATE_ENTRY:
		stop_sw = false;
		recover = false;
		entry3_retry_count = 0;
#if EN_MI_THERMAL
		if(pdpm->chg_consumer){
			info = pdpm->chg_consumer->cm;
			if(info){
				pdata = &info->chg1_data;
				temp_thermal_charging_current = pdata->thermal_charging_current_limit/2000;
				temp_thermal_input_current = pdata->thermal_input_current_limit/2000;
				temp_thermal_mitigation_current = info->thermal_mitigation_current/2000;
				pr_err("[%d][%d][%d]\n",temp_thermal_input_current,temp_thermal_charging_current,temp_thermal_mitigation_current);
			}
			if(temp_thermal_mitigation_current >= BAT_CURR_1000MA){
				pdpm->pps_temp_flag = true;
			}else{
				pdpm->pps_temp_flag = false;
			}
		}
#endif
		if (!pdpm->safe_batt_flag) {
			if (pdpm->safe_batt_retry_count < 20) {
				pdpm->safe_batt_flag = get_battery_authentic();
				pdpm->safe_batt_retry_count++;
			}
		}
		if (pdpm->cp.vbat_volt < pm_config.min_vbat_for_cp) {
			pr_notice("batt_volt-%d, waiting...\n", pdpm->cp.vbat_volt);
		} else if (pdpm->cp.vbat_volt > pm_config.bat_volt_lp_lmt - 100) {
			pr_notice("batt_volt-%d is too high for cp,\
					charging with switch charger\n",
					pdpm->cp.vbat_volt);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
#if EN_MI_THERMAL	
		} else if ((bat_temp >= CHG_BAT_TEMP_MAX)
			|| bat_temp <= CHG_BAT_TEMP_MIN
			|| get_charging_call_state() ||(!pdpm->pps_temp_flag) || (!pdpm->safe_batt_flag)) {
#else
		} else if ((bat_temp >= CHG_BAT_TEMP_MAX)
			|| bat_temp <= CHG_BAT_TEMP_MIN
			|| get_charging_call_state() || (!pdpm->safe_batt_flag)) {
#endif
			pr_notice("temp is %d, high or low, waiting...\n", bat_temp);
		} else {
			pr_notice("batt_volt-%d is ok, start flash charging\n",
					pdpm->cp.vbat_volt);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY);
		}

		if (!pdpm->pps_supported) {
			pr_notice("pps supported is failed\n");
			recover = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		} else if ((!pdpm->safe_batt_flag) && (pdpm->safe_batt_retry_count >= 20)) {
			pr_notice("battery authentic is failed\n");
			recover = false;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}
		break;

	case PD_PM_STATE_FC2_ENTRY:
		pr_err("fc2_disable_sw:%d, charge_enabled:%d\n",
				pm_config.fc2_disable_sw,pdpm->sw.charge_enabled);
		if (pm_config.fc2_disable_sw) {
			usbpd_pm_enable_sw(pdpm, true);
#if 0
			if (!pdpm->sw.charge_enabled)
				usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
#else
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
#endif
		} else {
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
		}
		break;

	case PD_PM_STATE_FC2_ENTRY_1:
		if (pm_config.cp_sec_enable)
           		 pdpm->request_voltage = pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP * 2;
             else
           		 pdpm->request_voltage = pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP;

		pdpm->request_current = min(pdpm->apdo_max_curr, pm_config.bus_curr_lp_lmt);
		retSelect = usbpd_select_pdo(pdpm,pdpm->request_voltage, pdpm->request_current);
		pr_err("FC2_ENTRY_1:request_voltage:%d, request_current:%d - [%d - %d]\n",
				pdpm->request_voltage, pdpm->request_current,pdpm->apdo_max_curr,pm_config.bus_curr_lp_lmt);
		if(retSelect == TCP_DPM_RET_SUCCESS ){
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);
		}else{
			recover = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}
		tune_vbus_retry = 0;
		break;

	case PD_PM_STATE_FC2_ENTRY_2:
		pr_err("tune_vbus_retry %d,vbus_error_low : %d , vbus_error_high : %d\n", tune_vbus_retry,pdpm->cp.vbus_error_low ,pdpm->cp.vbus_error_high);
		pr_err("cp_vbat:%d, cp_vbus:%d\n",pdpm->cp.vbat_volt, pdpm->cp.vbus_volt);
		if (pdpm->cp.vbus_error_low || (pdpm->cp.vbus_volt < (pdpm->cp.vbat_volt * 195/100))) {
			tune_vbus_retry++;
			pdpm->request_voltage += 20;
			retSelect =  usbpd_select_pdo(pdpm,pdpm->request_voltage,
						 pdpm->request_current);
			pr_err("FC2_ENTRY_2-1:request_voltage:%d, request_current:%d\n",
					pdpm->request_voltage, pdpm->request_current);
		} else if (pdpm->cp.vbus_error_high || pdpm->cp.vbus_volt > pdpm->cp.vbat_volt * 217/100) {
			tune_vbus_retry++;
			pdpm->request_voltage -= 20;
			retSelect = usbpd_select_pdo(pdpm,pdpm->request_voltage,
						 pdpm->request_current);
			pr_err("FC2_ENTRY_2-2:request_voltage:%d, request_current:%d\n",
					pdpm->request_voltage, pdpm->request_current);
		} else {
			pr_notice("adapter volt tune ok, retry %d times\n", tune_vbus_retry);
			entry3_retry_count = 0;
		    usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_3);
			break;
		}
		
		if(retSelect != TCP_DPM_RET_SUCCESS ){
			recover = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}
		if (tune_vbus_retry > 35) {
			pr_notice("Failed to tune adapter volt into valid range, \
					charge with switching charger\n");
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}
		break;
	case PD_PM_STATE_FC2_ENTRY_3:
		entry3_retry_count++;
		if (entry3_retry_count <= 5) {
			usbpd_pm_set_chgdev_info(pdpm);
		}
		usbpd_pm_check_cp_enabled(pdpm);
		if (!pdpm->cp.charge_enabled) {
			usbpd_pm_enable_cp(pdpm, true);
			usbpd_pm_check_cp_enabled(pdpm);
			pr_err("[%d] enable master cp\n",__LINE__);
		}

		if (pm_config.cp_sec_enable && pdpm->cp.charge_enabled) {
			usbpd_pm_check_cp_sec_enabled(pdpm);
			pr_err("[%d] enable slave cp:%d, cp_sec.charge_enable : %d\n",__LINE__,pm_config.cp_sec_enable,pdpm->cp_sec.charge_enabled);
			if (!pdpm->cp_sec.charge_enabled) {
				usbpd_pm_enable_cp_sec(pdpm, true);
				usbpd_pm_check_cp_sec_enabled(pdpm);
			}
		}
		if (pdpm->cp.charge_enabled) {
			if ((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled) || !pm_config.cp_sec_enable) {
				entry3_retry_count = 0;
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
		} else if ((ret == PM_ALGO_RET_TAPER_DONE) || (ret == PM_ALGO_RET_OTHER_FAULT)) {
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
			retSelect = usbpd_select_pdo(pdpm,pdpm->request_voltage,
						 pdpm->request_current);
			pr_err("request_voltage:%d, request_current:%d\n",
					pdpm->request_voltage, pdpm->request_current);
		}

		if (!pdpm->pps_temp_flag
			|| get_charging_call_state()) {
			pr_notice("temp high or lower,stop charging\n");
			stop_sw = false;
			pdpm->sw.charge_enabled = false;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		}

#ifdef WT_COMPILE_FACTORY_VERSION
		if (mtk_get_battery_capacity() >= 79) {
			stop_sw = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			pr_err("wt_debug : %s slave charger exit", __func__);
		}
#endif

		batt_temp = get_battery_temp();
		if((batt_temp < 150) || !pdpm->pd_authen)
			pdpm->cp.batt_volt_max = pm_config.bat_volt_lp_lmt - 22;
		else
			pdpm->cp.batt_volt_max = pm_config.bat_volt_lp_lmt;

		/*stop second charge pump if either of ibus is lower than 100ma during CV*/
		if((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled
				&& pdpm->cp.vbat_volt >= pdpm->cp.batt_volt_max - BATT_CC_TO_CV_OFFST
				&& (pdpm->cp.ibus_curr < BATT_SD_SECP_CURR || pdpm->cp_sec.ibus_curr < BATT_SD_SECP_CURR)) || !pdpm->pps_temp_flag) {
			pr_notice("second cp is disabled due to ibus < 750 mA or temp high\n");
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
			pdpm->cp_sec_stopped = true;
		}

		if(retSelect != TCP_DPM_RET_SUCCESS ){
			recover = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}
		break;

	case PD_PM_STATE_FC2_EXIT:
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
		}else if (!stop_sw && !pdpm->sw.charge_enabled){
			usbpd_pm_enable_sw(pdpm, true);
		}

		if (recover){
			usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
		}else{
			usbpd_pps_enable_charging(pdpm,false,5000,2000);
			charger_dev_set_mivr(pdpm->sw_chg,4600000);
			rc = 1;
		}
		if (!pdpm->pps_temp_flag
			|| get_charging_call_state()) {
			pr_notice("temp is high or low waiting...\n");
			usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
			pdpm->pd_active = false;
			schedule_work(&pdpm->usb_psy_change_work);
		}
        break;
	}
	if(pdpm->chg_consumer){
		info = pdpm->chg_consumer->cm;
		if(info){
			/*
					stop_charging = true  ;disable all CHG_IC
			*/
			info->stop_charging = stop_sw;
			/*
				update cp_status 1 :enable cp charging;
							    0 : disabel cp charging;
			*/
			pre_cp_status = info->cp_status;
			info->cp_status = pdpm->cp.charge_enabled;
			if (!info->cp_status && (pre_cp_status != info->cp_status)) {
				charger_dev_enable(pdpm->sw_chg, true);
				charger_dev_set_mivr(pdpm->sw_chg,4600000);
				charger_dev_set_input_current(pdpm->sw_chg, CHG_CURR_2000MA*1000);
				charger_dev_set_charging_current(pdpm->sw_chg, CHG_CURR_2000MA*1000);
				charger_dev_enable_termination(pdpm->sw_chg, true);
				_wake_up_charger(info);
			} else if (info->cp_status && (pre_cp_status != info->cp_status)) {
				charger_dev_set_constant_voltage(pdpm->sw_chg, 4608000);
				charger_dev_enable_termination(pdpm->sw_chg, false);
				_wake_up_charger(info);
			}
			pr_notice("cp_status : %d,stop_charging %d\n",pdpm->cp.charge_enabled,stop_sw);
		}
	}
	return rc;
}

static void usbpd_pm_workfunc(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					pm_work.work);
	int time;
	int ibat_total = battery_get_bat_current() / 1000;

	usbpd_pm_update_sw_status(pdpm);
	usbpd_pm_update_cp_status(pdpm);
	usbpd_pm_update_cp_sec_status(pdpm);
	pr_err("pd_active %d\n",pdpm->pd_active);

	if ((pdpm->cp.set_ibat_cur - ibat_total > 2000) && (pdpm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - BATT_CC_TO_CV_OFFST))
		time = 300;
	else if ((pdpm->cp.set_ibat_cur - ibat_total <= 2000)
		&& (pdpm->cp.set_ibat_cur - ibat_total > 1500)
		&& (pdpm->cp.vbat_volt < pm_config.bat_volt_lp_lmt - BATT_CC_TO_CV_OFFST))
		time = 500;
	else if(ibat_total > 10800)
		time = 300;
	else
		time = PM_WORK_RUN_INTERVAL;

	if (!usbpd_pm_sm(pdpm) && pdpm->pd_active)
		schedule_delayed_work(&pdpm->pm_work,
				msecs_to_jiffies(time));
}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
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
	pdpm->lcdon_curr_step = 0;
	pdpm->lcdoff_curr_step = 0;
	pdpm->apdo_selected_pdo = 0;
	pval.intval = 0;
	ret = power_supply_set_property(pdpm->apdo_psy,
			POWER_SUPPLY_PROP_APDO_MAX, &pval);
	if (ret)
		pr_err("%s:ret=%d\n",__func__,ret);
	usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
	schedule_delayed_work(&pdpm->pm_work, 0);
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
	int pdAuthen = 0;

	pdpm->pd_active = connected;
	pr_err("[SC manager] >> pd_active %d\n",pdpm->pd_active);

	if (connected) {
		usbpd_pm_evaluate_src_caps(pdpm);
		mi_pd_auth(pdpm);
		pr_err("[SC manager] >>start cp charging pps support %d , PD Authen %d\n", pdpm->pps_supported,pdAuthen);
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
	struct power_supply *batt_verify;
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
					usb_psy_change_work);

	batt_verify = power_supply_get_by_name("bms");
	if (batt_verify) {
		power_supply_get_property(batt_verify,
			POWER_SUPPLY_PROP_AUTHENTIC, &propval);
		pr_err("[SC manager] >> battery authentic = %d\n",propval.intval);
		pdpm->safe_batt_flag = propval.intval;
	}
	pr_err("[SC manager] >> usb change work\n");
	if(pdpm->safe_batt_flag || (pdpm->safe_batt_retry_count < 20)){
		if (!pdpm->pd_active && pdpm->is_pps_en_unlock)
			usbpd_pd_contact(pdpm, true);
		else if (pdpm->pd_active  && !pdpm->is_pps_en_unlock)
			usbpd_pd_contact(pdpm, false);
	}
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
	
	pr_err("%s start",__func__);
	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	usbpd_check_cp_psy(pdpm);
	if (pm_config.cp_sec_enable) {
		usbpd_check_cp_sec_psy(pdpm);
	}
	usbpd_check_charger_psy(pdpm);
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
	pr_err("%s START!",__func__);
	pdpm = kzalloc(sizeof(*pdpm), GFP_KERNEL);
	if (!pdpm)
		return -ENOMEM;

	__pdpm = pdpm;

	INIT_WORK(&pdpm->cp_psy_change_work, cp_psy_change_work);
	INIT_WORK(&pdpm->usb_psy_change_work, usb_psy_change_work);

	spin_lock_init(&pdpm->psy_change_lock);
	
	pdpm->cp_index = 0;
	pdpm->cp_sec_index = 0;
	pdpm->safe_batt_flag = 0;
	pdpm->safe_batt_retry_count = 0;
	usbpd_check_cp_psy(pdpm);
	if (pm_config.cp_sec_enable) {
		usbpd_check_cp_sec_psy(pdpm);
	}
	usbpd_check_apdo_psy(pdpm);
	usbpd_check_charger_psy(pdpm);
	usbpd_check_tcpc(pdpm);
	usbpd_check_pca_chg_swchg(pdpm);

	INIT_DELAYED_WORK(&pdpm->pm_work, usbpd_pm_workfunc);
	INIT_DELAYED_WORK(&pdpm->pd_work, usbpd_workfunc);

	if (pdpm->tcpc && pdpm->cp_index && (pdpm->cp_index == pdpm->cp_sec_index)) {
		/* register tcp notifier callback */
		pdpm->tcp_nb.notifier_call = pca_pps_tcp_notifier_call;
		ret = register_tcp_dev_notifier(pdpm->tcpc, &pdpm->tcp_nb,
					TCP_NOTIFY_TYPE_USB);
		if (ret < 0) {
			pr_err("register tcpc notifier fail\n");
			return ret;
		}

		pdpm->nb.notifier_call = usbpd_psy_notifier_cb;
		power_supply_reg_notifier(&pdpm->nb);
	} else {
		pr_err("%s cp and cp_sec do not match, Contact customer service.\n",__func__);
	}
	pr_err("%s END!",__func__);
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


