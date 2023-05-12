// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
*/
#define pr_fmt(fmt)	"[SC-USBPD-PM]: %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/of.h>
#include <linux/iio/consumer.h>
//#include <linux/usb/usbpd.h>
#include "pd_policy_manager.h"

//config battery charge full voltage
#define BATT_MAX_CHG_VOLT           4460

//config fast charge current
#define BATT_FAST_CHG_CURR          6000

//config vbus max voltage
#define	BUS_OVP_THRESHOLD           11000

//config open CP vbus/vbat
#define BUS_VOLT_INIT_UP            210 / 100
#define BUS_VOLT_MIN                207 / 100
#define BUS_VOLT_MAX                215 / 100

//config monitor time (ms)
#define PM_WORK_RUN_INTERVAL        100

#define BAT_VOLT_LOOP_LMT           BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT           BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT           BUS_OVP_THRESHOLD

#define CHG_BAT_TEMP_MIN      50
#define CHG_BAT_TEMP_10      100
#define CHG_BAT_TEMP_15      150
#define CHG_BAT_TEMP_35     350
#define CHG_BAT_TEMP_48     480
#define CHG_BAT_TEMP_MAX    600

#define CHG_BAT_CURR_2450MA     2350
#define CHG_BAT_CURR_3920MA    3820
#define CHG_BAT_CURR_4000MA     3900
#define CHG_BAT_CURR_5400MA     5300
#define CHG_BAT_CURR_6000MA     5900

#define TAPER_TIMEOUT	(5000 / PM_WORK_RUN_INTERVAL)
#define IBUS_CHANGE_TIMEOUT  (500 / PM_WORK_RUN_INTERVAL)

enum {
    PM_ALGO_RET_OK,
    PM_ALGO_RET_CHG_DISABLED,
    PM_ALGO_RET_TAPER_DONE,
};

static const struct pdpm_config pm_config = {
    .bat_volt_lp_lmt            = BAT_VOLT_LOOP_LMT,
    .bat_curr_lp_lmt            = BAT_CURR_LOOP_LMT,
    .bus_volt_lp_lmt            = BUS_VOLT_LOOP_LMT,
    .bus_curr_lp_lmt            = (BAT_CURR_LOOP_LMT >> 1),

    //config CP to main charger current(ma)
    .fc2_taper_current          = 2200,
    //config adapter voltage step(PPS:1-->20mV)
    .fc2_steps                  = 1,
    //config adapter pps pdo min voltage
    .min_adapter_volt_required  = 11000,
    //config adapter pps pdo min curremt
    .min_adapter_curr_required  = 2000,
    //config CP charge min vbat voltage
    .min_vbat_for_cp            = 3500,
    //config standalone(false) or master+slave(true) CP
    .cp_sec_enable              = false,
    //config CP charging, main charger is disable
    .fc2_disable_sw			    = true,
};

static struct usbpd_pm *__pdpm;
int is_pps_en;
int switch_chg_ic;

static int thermal_mitigation[] = {
	6000000,5400000,5000000,4500000,4000000,3500000,3000000,2700000,
	2500000,2300000,2100000,1800000,1500000,900000,800000,500000,300000,
};

enum cp_iio_type {
	APDO,
};

enum adpo_iio_channels {
	APDO_MAX_VOLT,
	APDO_MAX_CURR,
};

static const char * const apdo_iio_chan_name[] = {
	[APDO_MAX_VOLT] = "apdo_max_volt",
	[APDO_MAX_CURR] = "apdo_max_curr",
};

static bool is_apdo_chan_valid(struct usbpd_pm *pdpm,
		enum adpo_iio_channels chan)
{
	int rc;

	if (IS_ERR(pdpm->apdo_iio[chan]))
		return false;

	if (!pdpm->apdo_iio[chan]) {
		pdpm->apdo_iio[chan] = iio_channel_get(pdpm->dev,
					apdo_iio_chan_name[chan]);
		if (IS_ERR(pdpm->apdo_iio[chan])) {
			rc = PTR_ERR(pdpm->apdo_iio[chan]);
			if (rc == -EPROBE_DEFER)
				pdpm->apdo_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				apdo_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

int usbpd_set_iio_channel(struct usbpd_pm *pdpm,
			enum cp_iio_type type, int channel, int val)
{
    	struct iio_channel *iio_chan_list;
	int rc;

	if(pdpm->shutdown_flag)
		return -ENODEV;

	switch (type) {
        case APDO:
            if (!is_apdo_chan_valid(pdpm, channel))
                return -ENODEV;
            iio_chan_list = pdpm->apdo_iio[channel];
            break;
        default:
            pr_err_ratelimited("iio_type %d is not supported\n", type);
            return -EINVAL;
	}

	rc = iio_write_channel_raw(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}

int usbpd_get_iio_channel(struct usbpd_pm *pdpm,
			enum cp_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;

	if(pdpm->shutdown_flag)
		return -ENODEV;

	switch (type) {
	case APDO:
		if (!is_apdo_chan_valid(pdpm, channel))
			return -ENODEV;
		iio_chan_list = pdpm->apdo_iio[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}

/****************Charge Pump API*****************/
int get_pps_enable_status(void)
{
    return is_pps_en;
}
EXPORT_SYMBOL(get_pps_enable_status);

int back_to_main_charge(void)
{
    return switch_chg_ic;
}
EXPORT_SYMBOL(back_to_main_charge);


static void usbpd_check_cp_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->cp_psy) {
            pdpm->cp_psy = power_supply_get_by_name("sc8551_standalone");
            if (!pdpm->cp_psy) {
                pdpm->cp_psy = power_supply_get_by_name("ln8000_standalone");
                if(!pdpm->cp_psy)
                    pr_err("cp_psy not found\n");
                else {
                    pr_err("find ln8000-standalone psy\n");
                    pdpm->cp_isln8000_flag = 1;
                }
            } else {
                pr_err("find sc8551-standalone psy\n");
                pdpm->cp_isln8000_flag = 0;
            }
    }
}

static void usbpd_enable_ln8000_ovp_check(struct usbpd_pm *pdpm, int enable)
{
    union power_supply_propval val = {0,};

    val.intval = enable;

    if (pdpm->cp_psy != NULL && pdpm->cp_isln8000_flag) {
        power_supply_set_property(pdpm->cp_psy, POWER_SUPPLY_PROP_STATUS, &val);
        pr_err("LN8000, enable check ovp %d\n", enable);
    }
    else
        pr_err("not LN8000,  need not check ovp\n");
}

static void usbpd_check_cp_sec_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->cp_sec_psy) {
        pdpm->cp_sec_psy = power_supply_get_by_name("sc8551-slave");
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
            POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
    if (!ret)
        pdpm->cp.vbus_volt = val.intval; 

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CURRENT_NOW, &val);
    if (!ret)
        pdpm->cp.ibus_curr = val.intval;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CHARGE_COUNTER, &val);
    if (!ret) {
        pdpm->cp.vbus_err_low = !!(val.intval & 0x20);
        pdpm->cp.vbus_err_high = !!(val.intval & 0x10);
    }

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_TEMP, &val);
    if (!ret)
        pdpm->cp.die_temp = val.intval; 

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_ONLINE, &val);
    if (!ret)
        pdpm->cp.charge_enabled = val.intval;

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
    if (!ret)
        pdpm->sw.vbat_volt = val.intval;
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
            POWER_SUPPLY_PROP_CURRENT_NOW, &val);
    if (!ret)
        pdpm->cp_sec.ibus_curr = val.intval; 

    ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_ONLINE, &val);
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
            POWER_SUPPLY_PROP_ONLINE, &val);

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
            POWER_SUPPLY_PROP_ONLINE, &val);
    
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
            POWER_SUPPLY_PROP_ONLINE, &val);
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
            POWER_SUPPLY_PROP_ONLINE, &val);
    if (!ret)
        pdpm->cp_sec.charge_enabled = !!val.intval;
    
    return ret;
}

static int select_sw_charger_current(struct usbpd_pm *pdpm, bool enable)
{
    int ret;
    union power_supply_propval val = {0,};
    int temp = 0;
    int vbat = 0;
    int curr = 3000000;

    if (!pdpm->bms_psy) {
        pdpm->bms_psy = power_supply_get_by_name("sm5602_bat");
        if(!pdpm->bms_psy)
            return -ENODEV;
    }

    ret = power_supply_get_property(pdpm->bms_psy, 
            POWER_SUPPLY_PROP_TEMP, &val);
    temp = val.intval;

    ret = power_supply_get_property(pdpm->bms_psy,
            POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
    vbat = (int)(val.intval/1000);

    if (temp <= -100)
        curr = 0;
    else if (temp > -100 && temp <= 0)
        curr = 490000;
    else if (temp > 0 && temp <= 50)
        curr = 980000;
    else if (temp > 480) {
        if (vbat > 4100)
            curr = 0;
    }

    if(!enable)
        curr = 100000;
    
    return curr;
}

static int usbpd_pm_enable_sw(struct usbpd_pm *pdpm, bool enable)
{
    /*TODO: config main charger enable or disable
    If the main charger needs to take 100mA current when the CC charging, you 
    can set the max current to 100mA when disable
    */

    int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->sw_psy) {
        pdpm->sw_psy = power_supply_get_by_name("bq25890_charger");
        if (!pdpm->sw_psy)
            return -ENODEV;
    }

    if(enable)
        val.intval = 3000000;
    else
        val.intval = 100000;

    ret = power_supply_set_property(pdpm->sw_psy, 
            POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);

    val.intval = select_sw_charger_current(pdpm, enable);
    ret = power_supply_set_property(pdpm->sw_psy, 
            POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);

	pr_info("usbpd_pm_enable_sw,set sw_charger %d, set ichg = %duA\n",val.intval, select_sw_charger_current(pdpm, enable));
    
    return ret;
}

static int usbpd_pm_check_sw_enabled(struct usbpd_pm *pdpm)
{
    /*TODO: check main charger enable or disable
    */
    int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->sw_psy) {
        pdpm->sw_psy = power_supply_get_by_name("bq25890_charger");
        if (!pdpm->sw_psy) {
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(pdpm->sw_psy, 
            POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
    if (!ret)
    {
        if(val.intval == 100000)
			pdpm->sw.charge_enabled = false;
        else
			pdpm->sw.charge_enabled = true;
    }
	pr_info("usbpd_pm_check_sw_enabled, enabled = %d\n", pdpm->sw.charge_enabled);
    return ret;
}

static void usbpd_pm_update_sw_status(struct usbpd_pm *pdpm)
{
    usbpd_pm_check_sw_enabled(pdpm);
}

/***************PD API****************/
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
        if (en) {
            ret = tcpm_set_apdo_charging_policy(pdpm->tcpc,
                DPM_CHARGING_POLICY_PPS | DPM_CHARGING_POLICY_IGNORE_MISMATCH_CURR, mV, mA, NULL);
            pr_err("tcpm_set_apdo_charging_policy null\n", ret);
        } else {
            ret = tcpm_reset_pd_charging_policy(pdpm->tcpc, NULL);
        }
        cnt++;
    } while (ret != TCP_DPM_RET_SUCCESS && cnt < 3);

    if (ret != TCP_DPM_RET_SUCCESS)
        pr_err("fail(%d)\n", ret);
    return ret > 0 ? -ret : ret;
}

static bool usbpd_get_pps_status(struct usbpd_pm *pdpm)
{
    int ret, apdo_idx = -1;
    struct tcpm_power_cap_val apdo_cap = {0};
    u8 cap_idx;

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
            pdpm->apdo_max_volt = apdo_cap.max_mv;
            pdpm->apdo_max_curr = apdo_cap.ma;
        } else {
            if (apdo_cap.ma > pdpm->apdo_max_curr) {
                apdo_idx = cap_idx;
                pdpm->apdo_max_volt = apdo_cap.max_mv;
                pdpm->apdo_max_curr = apdo_cap.ma;
            }
        }
        ret = usbpd_set_iio_channel(pdpm, APDO, APDO_MAX_VOLT, pdpm->apdo_max_volt);
        if (ret < 0) {
            pr_err("set APDO_MAX_VOLT fail(%d)\n", ret);
            return ret;
        }
        ret = usbpd_set_iio_channel(pdpm, APDO, APDO_MAX_CURR, pdpm->apdo_max_curr);
        if (ret < 0) {
            pr_err("set APDO_MAX_CURR fail(%d)\n", ret);
            return ret;
        }
    }
    if (apdo_idx != -1){
        ret = usbpd_pps_enable_charging(pdpm, true, 5000, 3000);
        if (ret != TCP_DPM_RET_SUCCESS)
            return false;
		pr_err("select potential cap_idx[%d]\n", cap_idx);
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
    } while (ret != TCP_DPM_RET_SUCCESS && cnt < 3);

    if (ret != TCP_DPM_RET_SUCCESS)
        pr_err("fail(%d)\n", ret);
    return ret > 0 ? -ret : ret;
}

static int pca_pps_tcp_notifier_call(struct notifier_block *nb,
                    unsigned long event, void *data)
{
    struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, tcp_nb);
    struct tcp_notify *noti = data;

    switch (event) {
    case TCP_NOTIFY_PD_STATE:
        switch (noti->pd_state.connected) {
        case PD_CONNECT_NONE:
            pr_err("detached\n");
            pdpm->is_pps_en_unlock = false;
            pdpm->hrst_cnt = 0;
            pdpm->psy_change_running = 0;
            break;
        case PD_CONNECT_HARD_RESET:
            pdpm->hrst_cnt++;
            pr_err("pd hardreset, cnt = %d\n",
                pdpm->hrst_cnt);
            pdpm->is_pps_en_unlock = false;
            pdpm->psy_change_running = 0;
            break;
        case PD_CONNECT_PE_READY_SNK_PD30:
            tcpm_dpm_pd_request(pdpm->tcpc, 5000, 3000, NULL);
            break;
        case PD_CONNECT_PE_READY_SNK_APDO:
            if (pdpm->hrst_cnt < 5) {
                pr_err("en unlock\n");
                pdpm->is_pps_en_unlock = true;
                pdpm->pd_active = 0;
                pdpm->psy_change_running = 0;
                switch_chg_ic = false;
            }
            break;
        default:
            break;
        }
    default:
        break;
    }
    if(pdpm->usb_psy)
        power_supply_changed(pdpm->usb_psy);
    return NOTIFY_OK;
}
static void usbpd_check_tcpc(struct usbpd_pm *pdpm)
{
    int ret;

    if (!pdpm->tcpc) {
        pdpm->tcpc = tcpc_dev_get_by_name("type_c_port0");
        if (!pdpm->tcpc) {
            pr_err("get tcpc dev fail\n");
            return;
        }

        pdpm->tcp_nb.notifier_call = pca_pps_tcp_notifier_call;
        ret = register_tcp_dev_notifier(pdpm->tcpc, &pdpm->tcp_nb,
                        TCP_NOTIFY_TYPE_USB);
        if (ret < 0) {
            pr_err("register tcpc notifier fail\n");
        }
    }
}
/*******************main charger API********************/
static void usbpd_check_usb_psy(struct usbpd_pm *pdpm)
{
    if (!pdpm->usb_psy) {
        pdpm->usb_psy = power_supply_get_by_name("usb");
        if (!pdpm->usb_psy)
            pr_err("usb psy not found!\n");
    }
}

static void usbpd_check_batverify(struct usbpd_pm *pdpm)
{
    int rc, batt_id;
    union power_supply_propval val = {0,};

    pdpm->verify_psy = power_supply_get_by_name("batt_verify");
    if (!pdpm->verify_psy) {
        pr_err("verify psy not found!\n");
    } else {
        rc = power_supply_get_property(pdpm->verify_psy,
                POWER_SUPPLY_PROP_AUTHENTIC, &val);
        if (rc)
            pdpm->batt_auth = 0;
        else {
            if (val.intval) {
                rc = power_supply_get_property(pdpm->verify_psy, POWER_SUPPLY_PROP_MODEL_NAME, &val);
                if (rc) {
                    pr_err("get bat id err.");
                    batt_id = UNKNOW_SUPPLIER;
                } else {
                    if(strcmp(val.strval,"First supplier") == 0)
                    batt_id = FIRST_SUPPLIER;
                    else if(strcmp(val.strval,"Second supplier") == 0)
                        batt_id = SECOND_SUPPLIER;
                    else if(strcmp(val.strval,"Third supplier") == 0)
                        batt_id = THIRD_SUPPLIER;
                    else
                        batt_id = UNKNOW_SUPPLIER;
                }
                if (batt_id != UNKNOW_SUPPLIER)
                    pdpm->batt_auth = 1;
            } else
                pdpm->batt_auth = 0;
        }
    }
}

static int usbpd_update_ibat_curr(struct usbpd_pm *pdpm)
{
    /*TODO: update ibat and vbat by gauge
    */
    int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->bms_psy) {
        pdpm->bms_psy = power_supply_get_by_name("sm5602_bat");
        if (!pdpm->bms_psy) {
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(pdpm->bms_psy, 
            POWER_SUPPLY_PROP_CURRENT_NOW, &val);
    if (!ret)
        pdpm->sw.ibat_curr= -(int)(val.intval/1000);

    /*
    ret = power_supply_get_property(pdpm->bms_psy, 
            POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
    if (!ret)
        pdpm->sw.vbat_volt = (int)(val.intval/1000);
    */

    return ret;
    return 0;
}

static int usbpd_update_ibus_curr(struct usbpd_pm *pdpm)
{
    /*TODO: update ibus of main charger
    */
    int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->sw_psy) {
        pdpm->sw_psy = power_supply_get_by_name("bq25890_charger");
        if (!pdpm->sw_psy) {
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(pdpm->sw_psy, 
            POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
    if (!ret)
        pdpm->sw.ibus_curr = (int)(val.intval/1000);

    return ret;
}

static void usbpd_pm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
    bool retValue;

    retValue = usbpd_get_pps_status(pdpm);
    if (retValue)
        pdpm->pps_supported = true;

    
    if (pdpm->pps_supported)
        pr_err("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d\n",
                pdpm->apdo_selected_pdo,
                pdpm->apdo_max_volt,
                pdpm->apdo_max_curr);
    else {
        is_pps_en = false;
        pr_err("Not qualified PPS adapter\n");
    }
}

static int  get_batt_temp_thermal_curr(struct usbpd_pm *pdpm)
{
    union power_supply_propval val = {0,};
    int ret;

    if (!pdpm->bms_psy) {
        pdpm->bms_psy = power_supply_get_by_name("sm5602_bat");
        if (!pdpm->bms_psy) {
            return -ENODEV;
        }
    }

    if (!pdpm->batt_psy) {
        pdpm->batt_psy = power_supply_get_by_name("battery");
        if (!pdpm->batt_psy) {
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(pdpm->bms_psy,
            POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
    if (!ret)
        pdpm->bat_cycle = val.intval;

    ret = power_supply_get_property(pdpm->bms_psy,
            POWER_SUPPLY_PROP_TEMP, &val);
    if (!ret)
        pdpm->bat_temp = val.intval;

    ret = power_supply_get_property(pdpm->batt_psy,
            POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &val);
    if (!ret)
        pdpm->therm_curr = thermal_mitigation[val.intval]/1000;

        return 0;
}

extern int get_usbpd_verifed_state(void);
static int battery_sw_jeita(struct usbpd_pm *pdpm)
{
    int jeita_curr = 0;
    int pd_auth = 0;
    int cycle_volt = 0;

    pd_auth = get_usbpd_verifed_state();
    usbpd_check_batverify(pdpm);

    if (pdpm->bat_cycle <= 100)
        cycle_volt = 4240;
    else
        cycle_volt = 4200;

    if (pdpm->bat_temp >= CHG_BAT_TEMP_MIN && pdpm->bat_temp < CHG_BAT_TEMP_48) {
        pdpm->pps_temp_flag = 1;
        if (pdpm->bat_temp <= CHG_BAT_TEMP_10)
            jeita_curr  = CHG_BAT_CURR_2450MA;
        else if (pdpm->bat_temp > CHG_BAT_TEMP_10 && pdpm->bat_temp <= CHG_BAT_TEMP_15)
            jeita_curr  =  CHG_BAT_CURR_3920MA;
        else if (pdpm->bat_temp > CHG_BAT_TEMP_15 && pdpm->bat_temp < CHG_BAT_TEMP_48) {
            if(pd_auth) {
                if( pdpm->sw.vbat_volt <= cycle_volt )
                    jeita_curr  =  CHG_BAT_CURR_6000MA;
                else if ( pdpm->sw.vbat_volt > cycle_volt && pdpm->sw.vbat_volt <= 4440 )
                    jeita_curr  =  CHG_BAT_CURR_5400MA;
                else
                    jeita_curr  =  CHG_BAT_CURR_4000MA;
            } else
                jeita_curr  =  CHG_BAT_CURR_5400MA;
        }
        else
            jeita_curr  = CHG_BAT_CURR_2450MA;

        if (pdpm->batt_auth != 1)
            jeita_curr = 2000;

        if(pdpm->therm_curr < 2000)
            pdpm->pps_temp_flag = 0;

    } else
        pdpm->pps_temp_flag = 0;

    pr_err("battery_sw_jeita, bat_temp = %d, jeita_curr = %d, therm_curr = %d, pps_temp_flag = %d, pd_auth = %d, cycle_count = %d, batt_auth = %d\n",
                        pdpm->bat_temp,  jeita_curr, pdpm->therm_curr, pdpm->pps_temp_flag, pd_auth, pdpm->bat_cycle, pdpm->batt_auth);

    return min(pdpm->therm_curr, jeita_curr);
}

static int usbpd_pm_fc2_charge_algo(struct usbpd_pm *pdpm)
{
    int steps;
    int step_vbat = 0;
    int step_ibus = 0;
    int step_ibat = 0;
    int ibus_total = 0;
    int ibat_limit = 0;
    int vbat_now = 0;
    int ibat_now = 0;
    int fcc_curr = 0;

    static int ibus_limit;

    /*if vbat_volt update by main charger
    * vbat_limit = pdpm->sw.vbat_volt;
    */
    vbat_now = pdpm->sw.vbat_volt;

    if (ibus_limit == 0)
        ibus_limit = pm_config.bus_curr_lp_lmt;

    /* reduce bus current in cv loop */
    if (vbat_now > pm_config.bat_volt_lp_lmt - 50) {
        if (pdpm->ibus_lmt_change_timer++ > IBUS_CHANGE_TIMEOUT) {
            pdpm->ibus_lmt_change_timer = 0;
            ibus_limit = pm_config.bus_curr_lp_lmt;
        }
    } else if (vbat_now < pm_config.bat_volt_lp_lmt - 250) {
        ibus_limit = pm_config.bus_curr_lp_lmt;
        pdpm->ibus_lmt_change_timer = 0;
    } else {
        pdpm->ibus_lmt_change_timer = 0;
    }

    /* battery voltage loop*/
    if (vbat_now > pm_config.bat_volt_lp_lmt)
        step_vbat = -pm_config.fc2_steps;
    else if (vbat_now < pm_config.bat_volt_lp_lmt - 7)
        step_vbat = pm_config.fc2_steps;


    /* battery charge current loop*/
    /*TODO: thermal contrl
    * bat_limit = min(pm_config.bat_curr_lp_lmt, fcc_curr);
    */
    fcc_curr = battery_sw_jeita(pdpm);
    ibat_limit = min(pm_config.bat_curr_lp_lmt, fcc_curr) - 100;
    if (pdpm->input_suspend == 1 || pdpm->is_stop_charge == 1)
        ibat_limit = 0;
 //   ibat_limit = pm_config.bat_curr_lp_lmt;
    ibat_now = pdpm->sw.ibat_curr;
    if (ibat_now < ibat_limit)
        step_ibat = pm_config.fc2_steps;
    else if (pdpm->sw.ibat_curr > ibat_limit + 100)
        step_ibat = -pm_config.fc2_steps;

    /* bus current loop*/
    ibus_total = pdpm->cp.ibus_curr + pdpm->sw.ibus_curr;

    if (pm_config.cp_sec_enable)
        ibus_total += pdpm->cp_sec.ibus_curr;

    if (ibus_total < ibus_limit - 50)
        step_ibus = pm_config.fc2_steps;
    else if (ibus_total > ibus_limit)
        step_ibus = -pm_config.fc2_steps;

    steps = min(min(step_vbat, step_ibus), step_ibat);

    /* check if cp disabled due to other reason*/
    usbpd_pm_check_cp_enabled(pdpm);
    if (pm_config.cp_sec_enable) {
        usbpd_pm_check_cp_sec_enabled(pdpm);
    }

    if (!pdpm->cp.charge_enabled || (pm_config.cp_sec_enable && 
        !pdpm->cp_sec_stopped && !pdpm->cp_sec.charge_enabled)) {
        pr_err("cp.charge_enabled:%d  %d  %d\n",
                pdpm->cp.charge_enabled, pdpm->cp.vbus_err_low, pdpm->cp.vbus_err_high);
        return PM_ALGO_RET_CHG_DISABLED;
    }

    /* charge pump taper charge */
    if (vbat_now > pm_config.bat_volt_lp_lmt - 50 
            && ibat_now < pm_config.fc2_taper_current) {
        if (pdpm->fc2_taper_timer++ > TAPER_TIMEOUT) {
            pr_err("charge pump taper charging done\n");
            pdpm->fc2_taper_timer = 0;
            return PM_ALGO_RET_TAPER_DONE;
        }
    } else {
        pdpm->fc2_taper_timer = 0;
    }
        
    /*TODO: customer can add hook here to check system level 
        * thermal mitigation*/

    pr_err("%s %d %d %d all %d\n", __func__,  
            step_vbat, step_ibat, step_ibus, steps);

    pdpm->request_voltage += steps * 20;

    if (pdpm->request_voltage > pdpm->apdo_max_volt - 1000)
        pdpm->request_voltage = pdpm->apdo_max_volt - 1000;


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

#ifdef CONFIG_BUILD_QGKI
extern int get_input_suspend_flag(void);
extern int get_is_stop_charge(void);
#endif
static int usbpd_pm_sm(struct usbpd_pm *pdpm)
{
    int ret;
    int rc = 0;
    static int tune_vbus_retry;
    static bool stop_sw;
    static bool recover;

#ifdef CONFIG_BUILD_QGKI
    pdpm->input_suspend = get_input_suspend_flag();
    pdpm->is_stop_charge = get_is_stop_charge();
#else
    pdpm->input_suspend = 0;
    pdpm->is_stop_charge = 0;
#endif
    pr_err("state phase :%d\n", pdpm->state);
    pr_err("vbus_vol %d  sw vbat_vol %d\n", pdpm->cp.vbus_volt, pdpm->sw.vbat_volt);
    pr_err("cp m ibus_curr %d  cp s ibus_curr %d sw ibus_curr %d ibat_curr %d\n", 
            pdpm->cp.ibus_curr, pdpm->cp_sec.ibus_curr, pdpm->sw.ibus_curr, 
            pdpm->sw.ibat_curr);
    get_batt_temp_thermal_curr(pdpm);
    switch (pdpm->state) {
    case PD_PM_STATE_ENTRY:
        stop_sw = false;
        recover = false;
        if (pdpm->sw.vbat_volt < pm_config.min_vbat_for_cp) {
            is_pps_en = false;
            pr_err("batt_volt-%d, waiting...\n", pdpm->sw.vbat_volt);
        } else if (pdpm->bat_temp >= CHG_BAT_TEMP_48 || pdpm->bat_temp < CHG_BAT_TEMP_MIN
                ||  pdpm->therm_curr < 2000 || pdpm->input_suspend == 1 || pdpm->is_stop_charge == 1) {
            is_pps_en = false;
            pr_err("bat_temp:%d or pdpm->therm_curr :%d or input_suspend :%d or is_stop_charge:%d",
                    pdpm->bat_temp, pdpm->therm_curr, pdpm->input_suspend, pdpm->is_stop_charge);
        } else if (pdpm->sw.vbat_volt > pm_config.bat_volt_lp_lmt - 100) {
            pr_err("batt_volt-%d is too high for cp,\
                    charging with switch charger\n", 
                    pdpm->sw.vbat_volt);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
        } else {
            pr_err("batt_volt-%d is ok, start flash charging\n", 
                    pdpm->sw.vbat_volt);
            is_pps_en = true;
            usbpd_enable_ln8000_ovp_check(pdpm, true);
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
        if (pm_config.cp_sec_enable)
            pdpm->request_voltage = pdpm->sw.vbat_volt * BUS_VOLT_INIT_UP + 200;
        else
            pdpm->request_voltage = pdpm->sw.vbat_volt * BUS_VOLT_INIT_UP;

        pdpm->request_current = min(pdpm->apdo_max_curr, pm_config.bus_curr_lp_lmt);
        usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
        pr_err("request_voltage:%d, request_current:%d\n",
                pdpm->request_voltage, pdpm->request_current);

        usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);
        tune_vbus_retry = 0;
        break;

    case PD_PM_STATE_FC2_ENTRY_2:
        pr_err("tune_vbus_retry %d\n", tune_vbus_retry);
        if (pdpm->cp.vbus_err_low || (pdpm->cp.vbus_volt < 
                pdpm->sw.vbat_volt * BUS_VOLT_MIN)) {
            tune_vbus_retry++;
            pdpm->request_voltage += 20;
            usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
            pr_err("request_voltage:%d, request_current:%d,cp.vbus_err_high:%d,cp.vbus_volt:%d\n",
                    pdpm->request_voltage, pdpm->request_current,pdpm->cp.vbus_err_high,pdpm->cp.vbus_volt);
        } else if (pdpm->cp.vbus_err_high || (pdpm->cp.vbus_volt > 
                pdpm->sw.vbat_volt * BUS_VOLT_MAX)) {
            tune_vbus_retry++;
            pdpm->request_voltage -= 20;
            usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
            pr_err("request_voltage:%d, request_current:%d\n",
                    pdpm->request_voltage, pdpm->request_current);
        } else {
            pr_err("adapter volt tune ok, retry %d times\n", tune_vbus_retry);
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_3);
            break;
        }
        
        if (tune_vbus_retry > 30) {
            pr_err("Failed to tune adapter volt into valid range, \
                    charge with switching charger\n");
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
        }	
        break;
    case PD_PM_STATE_FC2_ENTRY_3:
        usbpd_pm_check_cp_enabled(pdpm);
        if (!pdpm->cp.charge_enabled) {
            usbpd_pm_enable_cp(pdpm, true);
            msleep(100);
            usbpd_pm_check_cp_enabled(pdpm);
        }

        if (pm_config.cp_sec_enable) {
            usbpd_pm_check_cp_sec_enabled(pdpm);
            if(!pdpm->cp_sec.charge_enabled) {
                usbpd_pm_enable_cp_sec(pdpm, true);
                msleep(100);
                usbpd_pm_check_cp_sec_enabled(pdpm);
            }
        }

        if (pdpm->cp.charge_enabled) {
            if ((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled)
                    || !pm_config.cp_sec_enable) {
                usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_TUNE);
                pdpm->ibus_lmt_change_timer = 0;
                pdpm->fc2_taper_timer = 0;
            }
        }
        break;

    case PD_PM_STATE_FC2_TUNE:
        ret = usbpd_pm_fc2_charge_algo(pdpm);
        if (ret == PM_ALGO_RET_TAPER_DONE) {
            pr_err("Move to switch charging:%d\n", ret);
            switch_chg_ic = true;
            stop_sw = false;
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
            break;
        } else if (ret == PM_ALGO_RET_CHG_DISABLED) {
            pr_err("Move to switch charging, will try to recover \
                    flash charging:%d\n", ret);
            recover = true;
            stop_sw = false;
            usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
            break;
        } else {
            usbpd_select_pdo(pdpm, pdpm->request_voltage, pdpm->request_current);
            pr_err("request_voltage:%d, request_current:%d\n",
                    pdpm->request_voltage, pdpm->request_current);
            if (!pdpm->pps_temp_flag) {
                pr_notice("temp high or lower,stop charging\n");
                stop_sw = false;
                recover = true;
                pdpm->sw.charge_enabled = false;
                is_pps_en = false;
                usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
                break;
            }
        }
        
        /*stop second charge pump if either of ibus is lower than 750ma during CV*/
        if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled 
                && pdpm->sw.vbat_volt > pm_config.bat_volt_lp_lmt - 50
                && (pdpm->cp.ibus_curr < 750 || pdpm->cp_sec.ibus_curr < 750)) {
            pr_err("second cp is disabled due to ibus < 750mA\n");
            usbpd_pm_enable_cp_sec(pdpm, false);
            usbpd_pm_check_cp_sec_enabled(pdpm);
            pdpm->cp_sec_stopped = true;
        }
        break;

    case PD_PM_STATE_FC2_EXIT:
        /* select default 5V*/
        is_pps_en = false;
        usbpd_enable_ln8000_ovp_check(pdpm, false);
        usbpd_select_pdo(pdpm, 5000, 3000);

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
            
        usbpd_pm_check_sw_enabled(pdpm);

        if (recover)
            usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
        else
            rc = 1;
        
        break;
    }

    return rc;
}

static void usbpd_pm_workfunc(struct work_struct *work)
{
    struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, pm_work.work);

    usbpd_pm_update_sw_status(pdpm);
    usbpd_update_ibus_curr(pdpm);
    usbpd_pm_update_cp_status(pdpm);
    usbpd_pm_update_cp_sec_status(pdpm);
    usbpd_update_ibat_curr(pdpm);

    if (!usbpd_pm_sm(pdpm) && pdpm->pd_active)
        schedule_delayed_work(&pdpm->pm_work,
            msecs_to_jiffies(PM_WORK_RUN_INTERVAL));

}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
    usbpd_pm_enable_cp(pdpm, false);
    usbpd_pm_check_cp_enabled(pdpm);
    if (pm_config.cp_sec_enable) {
        usbpd_pm_enable_cp_sec(pdpm, false);
        usbpd_pm_check_cp_sec_enabled(pdpm);
    }
    cancel_delayed_work_sync(&pdpm->pm_work);

    if (!pdpm->sw.charge_enabled) {
        usbpd_pm_enable_sw(pdpm, true);
        usbpd_pm_check_sw_enabled(pdpm);
    }

    pdpm->pps_supported = false;
    is_pps_en = false;
    usbpd_enable_ln8000_ovp_check(pdpm, false);
    pdpm->apdo_selected_pdo = 0;

    usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
}

static void usbpd_pd_contact(struct usbpd_pm *pdpm, bool connected)
{
    pdpm->pd_active = connected;
    pr_err("[SC manager] >> pd_active %d\n", pdpm->pd_active);
    if (connected) {
        msleep(10);
        usbpd_pm_evaluate_src_caps(pdpm);
        usbpd_pm_enable_sw(pdpm, true);
        usbpd_pm_check_sw_enabled(pdpm);
        pr_err("[SC manager] >>start cp charging pps support %d\n", 
            pdpm->pps_supported);
        if (pdpm->pps_supported)
            schedule_delayed_work(&pdpm->pm_work, 0);
        else
            pdpm->pd_active = false;
            power_supply_changed(pdpm->usb_psy);
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
    int ret;
    struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
                    usb_psy_change_work);
    union power_supply_propval val = {0,};

    pr_err("[SC manager] >> usb change work\n");

	#if 0
    if (check_typec_attached_snk(pdpm->tcpc) < 0) {
        if (pdpm->pd_active) {
            usbpd_pd_contact(pdpm, false);
        }
        goto out;
    }
	#endif

    ret = power_supply_get_property(pdpm->usb_psy,
            POWER_SUPPLY_PROP_USB_TYPE, &val);
    if (ret) {
        pr_err("Failed to get usb pd active state\n");
        goto out;
    }

    pr_err("[SC manager] >> pd_active %d,  val.intval %d\n",
        pdpm->pd_active, val.intval);

    if ((!pdpm->pd_active) && (pdpm->is_pps_en_unlock))
        usbpd_pd_contact(pdpm, true);
    else if (pdpm->pd_active && (!pdpm->is_pps_en_unlock))
        usbpd_pd_contact(pdpm, false);
out:
    pdpm->psy_change_running = false;
}

static int usbpd_psy_notifier_cb(struct notifier_block *nb, 
            unsigned long event, void *data)
{
    struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
    struct power_supply *psy = data;
    unsigned long flags;

	pr_err("[SC manager] >>>enter usbpd_psy_notifier_cb psy = %s,event = %d\n",psy->desc->name,event);
    if (event != PSY_EVENT_PROP_CHANGED)
        return NOTIFY_OK;

    usbpd_check_cp_psy(pdpm);
    usbpd_check_usb_psy(pdpm);
    usbpd_check_tcpc(pdpm);

    if (!pdpm->usb_psy)
        return NOTIFY_OK;

    if (psy == pdpm->usb_psy) {
        spin_lock_irqsave(&pdpm->psy_change_lock, flags);
        if (!pdpm->psy_change_running) {
            pdpm->psy_change_running = true;
             pr_err("[SC manager] >>>pdpm->psy_change_running : %d\n", pdpm->psy_change_running);
             schedule_work(&pdpm->usb_psy_change_work);
        }
        spin_unlock_irqrestore(&pdpm->psy_change_lock, flags);
    }
    return NOTIFY_OK;
}

static int  usbpd_iio_init(struct usbpd_pm *pdpm)
{
	pr_err("usbpd_iio_init start\n");
    pdpm->shutdown_flag = false;
	pdpm->apdo_iio = devm_kcalloc(pdpm->dev,
		ARRAY_SIZE(apdo_iio_chan_name), sizeof(*pdpm->apdo_iio), GFP_KERNEL);
	if (!pdpm->apdo_iio)
		return -ENOMEM;
	pr_err("usbpd_iio_init end\n");
	return 0;
}

static int usbpd_pm_probe(struct platform_device *pdev)
{
    struct usbpd_pm *pdpm;

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

    usbpd_iio_init(pdpm);

    INIT_WORK(&pdpm->cp_psy_change_work, cp_psy_change_work);
    INIT_WORK(&pdpm->usb_psy_change_work, usb_psy_change_work);

    spin_lock_init(&pdpm->psy_change_lock);

    usbpd_check_cp_psy(pdpm);
    usbpd_check_cp_sec_psy(pdpm);
    usbpd_check_usb_psy(pdpm);
    usbpd_check_tcpc(pdpm);

    INIT_DELAYED_WORK(&pdpm->pm_work, usbpd_pm_workfunc);

    pdpm->nb.notifier_call = usbpd_psy_notifier_cb;
    power_supply_reg_notifier(&pdpm->nb);

    return 0;
}

static int usbpd_pm_remove(struct platform_device *pdev)
{
    power_supply_unreg_notifier(&__pdpm->nb);
    cancel_delayed_work(&__pdpm->pm_work);
    cancel_work_sync(&__pdpm->cp_psy_change_work);
    cancel_work_sync(&__pdpm->usb_psy_change_work);
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
	{.compatible = "qcom,cp_manager",},
	{},
};
MODULE_DEVICE_TABLE(of, usbpd_pm_dt_match);

static struct platform_driver usbpd_pm_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "cp_manager",
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
}

static void __exit usbpd_pm_exit(void)
{
    platform_driver_unregister(&usbpd_pm_driver);
}

module_init(usbpd_pm_init);
module_exit(usbpd_pm_exit);

MODULE_DESCRIPTION("SC Charge Pump Policy Manager");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip <Aiden-yu@southchip.com>");
