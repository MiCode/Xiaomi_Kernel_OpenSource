
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
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>
#include "pd_policy_manager.h"

#define PCA_PPS_CMD_RETRY_COUNT	2

#define PD_SRC_PDO_TYPE_FIXED       0
#define PD_SRC_PDO_TYPE_BATTERY     1
#define PD_SRC_PDO_TYPE_VARIABLE    2
#define PD_SRC_PDO_TYPE_AUGMENTED   3

#ifdef CONFIG_DISABLE_TEMP_PROTECT
#define BATT_MAX_CHG_VOLT           4100
#else
#define BATT_MAX_CHG_VOLT           4472
#endif
#define BATT_FAST_CHG_CURR          12200
#define	BUS_OVP_THRESHOLD           12000
#define	BUS_OVP_ALARM_THRESHOLD     9500

#define BUS_VOLT_INIT_UP            200 //need to change 200

#define BAT_VOLT_LOOP_LMT           BATT_MAX_CHG_VOLT
#define BAT_CURR_LOOP_LMT           BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT           BUS_OVP_THRESHOLD

#define PM_WORK_RUN_INTERVAL        1000

extern int pps_change_to_stop(void);
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

    .min_adapter_volt_required  = 11000,
    .min_adapter_curr_required  = 2000,

    .min_vbat_for_cp        = 3500,

    .cp_sec_enable          = true,
    .fc2_disable_sw			= true,
};

static struct usbpd_pm *__pdpm;

static int fc2_taper_timer;
static int ibus_lmt_change_timer;

enum cp_iio_type {
	CP_MASTER,
	CP_SLAVE,
	BMS,
	MAIN,
	APDO,
};

enum cp_iio_channels {
	CHARGE_PUMP_PRESENT,
	CHARGE_PUMP_CHARGING_ENABLED,
	CHARGE_PUMP_STATUS,
	CHARGE_PUMP_SC_BATTERY_PRESENT,
	CHARGE_PUMP_SC_VBUS_PRESENT,
	CHARGE_PUMP_SC_BATTERY_VOLTAGE,
	CHARGE_PUMP_SC_BATTERY_CURRENT,
	CHARGE_PUMP_SC_BATTERY_TEMPERATURE,
	CHARGE_PUMP_SC_BUS_VOLTAGE,
	CHARGE_PUMP_SC_BUS_CURRENT,
	CHARGE_PUMP_SC_BUS_TEMPERATURE,
	CHARGE_PUMP_SC_DIE_TEMPERATURE,
	CHARGE_PUMP_SC_ALARM_STATUS,
	CHARGE_PUMP_SC_FAULT_STATUS,
	CHARGE_PUMP_SC_VBUS_ERROR_STATUS,
	CHARGE_PUMP_LN_PRESENT,
	CHARGE_PUMP_LN_CHARGING_ENABLED,
	CHARGE_PUMP_LN_STATUS,
	CHARGE_PUMP_LN_BATTERY_PRESENT,
	CHARGE_PUMP_LN_VBUS_PRESENT,
	CHARGE_PUMP_LN_BATTERY_VOLTAGE,
	CHARGE_PUMP_LN_BATTERY_CURRENT,
	CHARGE_PUMP_LN_BATTERY_TEMPERATURE,
	CHARGE_PUMP_LN_BUS_VOLTAGE,
	CHARGE_PUMP_LN_BUS_CURRENT,
	CHARGE_PUMP_LN_BUS_TEMPERATURE,
	CHARGE_PUMP_LN_DIE_TEMPERATURE,
	CHARGE_PUMP_LN_ALARM_STATUS,
	CHARGE_PUMP_LN_FAULT_STATUS,
	CHARGE_PUMP_LN_VBUS_ERROR_STATUS,
};

static const char * const cp_iio_chan_name[] = {
	[CHARGE_PUMP_PRESENT] = "present",
	[CHARGE_PUMP_CHARGING_ENABLED] = "charging_enabled",
	[CHARGE_PUMP_STATUS] = "status",
	[CHARGE_PUMP_SC_BATTERY_PRESENT] = "sc_battery_present",
	[CHARGE_PUMP_SC_VBUS_PRESENT] = "sc_vbus_present",
	[CHARGE_PUMP_SC_BATTERY_VOLTAGE] = "sc_battery_voltage",
	[CHARGE_PUMP_SC_BATTERY_CURRENT] = "sc_battery_current",
	[CHARGE_PUMP_SC_BATTERY_TEMPERATURE] = "sc_battery_temperature",
	[CHARGE_PUMP_SC_BUS_VOLTAGE] = "sc_bus_voltage",
	[CHARGE_PUMP_SC_BUS_CURRENT] = "sc_bus_current",
	[CHARGE_PUMP_SC_BUS_TEMPERATURE] = "sc_bus_temperature",
	[CHARGE_PUMP_SC_DIE_TEMPERATURE] = "sc_die_temperature",
	[CHARGE_PUMP_SC_ALARM_STATUS] = "sc_alarm_status",
	[CHARGE_PUMP_SC_FAULT_STATUS] = "sc_fault_status",
	[CHARGE_PUMP_SC_VBUS_ERROR_STATUS] = "sc_vbus_error_status",
	[CHARGE_PUMP_LN_PRESENT] = "ln_present",
	[CHARGE_PUMP_LN_CHARGING_ENABLED] = "ln_charging_enabled",
	[CHARGE_PUMP_LN_STATUS] = "status",
	[CHARGE_PUMP_LN_BATTERY_PRESENT] = "ln_battery_present",
	[CHARGE_PUMP_LN_VBUS_PRESENT] = "ln_vbus_present",
	[CHARGE_PUMP_LN_BATTERY_VOLTAGE] = "ln_battery_voltage",
	[CHARGE_PUMP_LN_BATTERY_CURRENT] = "ln_battery_current",
	[CHARGE_PUMP_LN_BATTERY_TEMPERATURE] = "ln_battery_temperature",
	[CHARGE_PUMP_LN_BUS_VOLTAGE] = "ln_bus_voltage",
	[CHARGE_PUMP_LN_BUS_CURRENT] = "ln_bus_current",
	[CHARGE_PUMP_LN_BUS_TEMPERATURE] = "ln_bus_temperature",
	[CHARGE_PUMP_LN_DIE_TEMPERATURE] = "ln_die_temperature",
	[CHARGE_PUMP_LN_ALARM_STATUS] = "ln_alarm_status",
	[CHARGE_PUMP_LN_FAULT_STATUS] = "ln_fault_status",
	[CHARGE_PUMP_LN_VBUS_ERROR_STATUS] = "ln_vbus_error_status",
};

static const char * const cp_sec_iio_chan_name[] = {
	[CHARGE_PUMP_PRESENT] = "present_slave",
	[CHARGE_PUMP_CHARGING_ENABLED] = "charging_enabled_slave",
	[CHARGE_PUMP_STATUS] = "status_slave",
	[CHARGE_PUMP_SC_BATTERY_PRESENT] = "sc_battery_present_slave",
	[CHARGE_PUMP_SC_VBUS_PRESENT] = "sc_vbus_present_slave",
	[CHARGE_PUMP_SC_BATTERY_VOLTAGE] = "sc_battery_voltage_slave",
	[CHARGE_PUMP_SC_BATTERY_CURRENT] = "sc_battery_current_slave",
	[CHARGE_PUMP_SC_BATTERY_TEMPERATURE] = "sc_battery_temperature_slave",
	[CHARGE_PUMP_SC_BUS_VOLTAGE] = "sc_bus_voltage_slave",
	[CHARGE_PUMP_SC_BUS_CURRENT] = "sc_bus_current_slave",
	[CHARGE_PUMP_SC_BUS_TEMPERATURE] = "sc_bus_temperature_slave",
	[CHARGE_PUMP_SC_DIE_TEMPERATURE] = "sc_die_temperature_slave",
	[CHARGE_PUMP_SC_ALARM_STATUS] = "sc_alarm_status_slave",
    [CHARGE_PUMP_SC_FAULT_STATUS] = "sc_fault_status_slave",
	[CHARGE_PUMP_SC_VBUS_ERROR_STATUS] = "sc_vbus_error_status_slave",
	[CHARGE_PUMP_LN_PRESENT] = "ln_present_slave",
	[CHARGE_PUMP_LN_CHARGING_ENABLED] = "ln_charging_enabled_slave",
	[CHARGE_PUMP_LN_STATUS] = "ln_status_slave",
	[CHARGE_PUMP_LN_BATTERY_PRESENT] = "ln_battery_present_slave",
	[CHARGE_PUMP_LN_VBUS_PRESENT] = "ln_vbus_present_slave",
	[CHARGE_PUMP_LN_BATTERY_VOLTAGE] = "ln_battery_voltage_slave",
	[CHARGE_PUMP_LN_BATTERY_CURRENT] = "ln_battery_current_slave",
	[CHARGE_PUMP_LN_BATTERY_TEMPERATURE] = "ln_battery_temperature_slave",
	[CHARGE_PUMP_LN_BUS_VOLTAGE] = "ln_bus_voltage_slave",
	[CHARGE_PUMP_LN_BUS_CURRENT] = "ln_bus_current_slave",
	[CHARGE_PUMP_LN_BUS_TEMPERATURE] = "ln_bus_temperature_slave",
	[CHARGE_PUMP_LN_DIE_TEMPERATURE] = "ln_die_temperature_slave",
	[CHARGE_PUMP_LN_ALARM_STATUS] = "ln_alarm_status_slave",
    [CHARGE_PUMP_LN_FAULT_STATUS] = "ln_fault_status_slave",
	[CHARGE_PUMP_LN_VBUS_ERROR_STATUS] = "ln_vbus_error_status_slave",
};

enum bms_iio_channels {
	BMS_VOLTAGE_NOW,
	BMS_CURRENT_NOW,
	BMS_TEMP_NOW,
	BMS_CYCLE_COUNT,
	BMS_THERM_CURR,
};

static const char * const bms_iio_chan_name[] = {
	[BMS_VOLTAGE_NOW] = "voltage_now",
	[BMS_CURRENT_NOW] = "current_now",
	[BMS_TEMP_NOW] = "temp",
	[BMS_CYCLE_COUNT] = "cycle_count",
	[BMS_THERM_CURR] = "therm_curr",
};

enum main_iio_channels {
	MAIN_CHAGER_HZ,
	MAIN_INPUT_CURRENT_SETTLED,
	MAIN_CHAGER_CURRENT,
	MAIN_CHARGER_ENABLED,
	MAIN_ENBALE_CHAGER_TERM,
};

static const char * const main_iio_chan_name[] = {
	[MAIN_CHAGER_HZ] = "main_chager_hz",
	[MAIN_INPUT_CURRENT_SETTLED] = "main_input_current_settled",
	[MAIN_CHAGER_CURRENT] = "main_charge_current",
	[MAIN_CHARGER_ENABLED] = "charger_enable",
	[MAIN_ENBALE_CHAGER_TERM] = "enable_charger_term",
};

enum adpo_iio_channels {
	APDO_MAX_VOLT,
	APDO_MAX_CURR,
};

static const char * const apdo_iio_chan_name[] = {
	[APDO_MAX_VOLT] = "apdo_max_volt",
	[APDO_MAX_CURR] = "apdo_max_curr",
};

static bool is_cp_chan_valid(struct usbpd_pm *chip,
		enum cp_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->cp_iio[chan]))
		return false;

	if (!chip->cp_iio[chan]) {
		chip->cp_iio[chan] = iio_channel_get(chip->dev,
					cp_iio_chan_name[chan]);
		if (IS_ERR(chip->cp_iio[chan])) {
			rc = PTR_ERR(chip->cp_iio[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cp_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cp_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_cp_psy_chan_valid(struct usbpd_pm *chip,
		enum cp_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->cp_sec_iio[chan]))
		return false;

	if (!chip->cp_sec_iio[chan]) {
		chip->cp_sec_iio[chan] = iio_channel_get(chip->dev,
					cp_sec_iio_chan_name[chan]);
		if (IS_ERR(chip->cp_sec_iio[chan])) {
			rc = PTR_ERR(chip->cp_sec_iio[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cp_sec_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cp_sec_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_bms_chan_valid(struct usbpd_pm *chip,
		enum bms_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->bms_iio[chan]))
		return false;

	if (!chip->bms_iio[chan]) {
		chip->bms_iio[chan] = iio_channel_get(chip->dev,
					bms_iio_chan_name[chan]);
		if (IS_ERR(chip->bms_iio[chan])) {
			rc = PTR_ERR(chip->bms_iio[chan]);
			if (rc == -EPROBE_DEFER)
				chip->bms_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				bms_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_main_chan_valid(struct usbpd_pm *chip,
		enum main_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->main_iio[chan]))
		return false;

	if (!chip->main_iio[chan]) {
		chip->main_iio[chan] = iio_channel_get(chip->dev,
					main_iio_chan_name[chan]);
		if (IS_ERR(chip->main_iio[chan])) {
			rc = PTR_ERR(chip->bms_iio[chan]);
			if (rc == -EPROBE_DEFER)
				chip->main_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				main_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_apdo_chan_valid(struct usbpd_pm *chip,
		enum adpo_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->apdo_iio[chan]))
		return false;

	if (!chip->apdo_iio[chan]) {
		chip->apdo_iio[chan] = iio_channel_get(chip->dev,
					apdo_iio_chan_name[chan]);
		if (IS_ERR(chip->apdo_iio[chan])) {
			rc = PTR_ERR(chip->apdo_iio[chan]);
			if (rc == -EPROBE_DEFER)
				chip->apdo_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				apdo_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

int usbpd_get_iio_channel(struct usbpd_pm *chg,
			enum cp_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;

	if(chg->shutdown_flag)
		return -ENODEV;

	switch (type) {
	case CP_MASTER:
		if (!chg->isln8000flg) {
			if (!is_cp_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_iio[channel];
		} else {
			if (!is_cp_chan_valid(chg, (channel + IIO_SECOND_CHANNEL_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_iio[channel + IIO_SECOND_CHANNEL_OFFSET];
		}
		break;
	case CP_SLAVE:
		if (!chg->isln8000flg) {
			if (!is_cp_psy_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_sec_iio[channel];
		} else {
			if (!is_cp_psy_chan_valid(chg, (channel + IIO_SECOND_CHANNEL_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_sec_iio[channel + IIO_SECOND_CHANNEL_OFFSET];
		}
		break;
	case BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->bms_iio[channel];
		break;
	case MAIN:
		if (!is_main_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_iio[channel];
		break;
	case APDO:
		if (!is_apdo_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->apdo_iio[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}

int usbpd_set_iio_channel(struct usbpd_pm *chg,
			enum cp_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int rc;

	if(chg->shutdown_flag)
		return -ENODEV;

	switch (type) {
	case CP_MASTER:
		if (!chg->isln8000flg) {
			if (!is_cp_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_iio[channel];
		} else {
			if (!is_cp_chan_valid(chg, (channel + IIO_SECOND_CHANNEL_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_iio[channel + IIO_SECOND_CHANNEL_OFFSET];
		}
		break;
	case CP_SLAVE:
		if (!chg->isln8000flg) {
			if (!is_cp_psy_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_sec_iio[channel];
		} else {
			if (!is_cp_psy_chan_valid(chg, (channel + IIO_SECOND_CHANNEL_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_sec_iio[channel + IIO_SECOND_CHANNEL_OFFSET];
		}
		break;
	case BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->bms_iio[channel];
		break;
	case MAIN:
		if (!is_main_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_iio[channel];
		break;
	case APDO:
		if (!is_apdo_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->apdo_iio[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_write_channel_raw(iio_chan_list, val);

	return rc < 0 ? rc : 0;
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
			TCPM_POWER_CAP_APDO_TYPE_PPS, &cap_idx, &apdo_cap);
        if (ret != TCP_DPM_RET_SUCCESS) {
            pr_err("inquire pd apdo fail(%d)\n", ret);
            break;
        }

        pr_err("cap_idx[%d], %d mv ~ %d mv, %d ma, pl: %d\n", cap_idx,
            apdo_cap.min_mv, apdo_cap.max_mv, apdo_cap.ma, apdo_cap.pwr_limit);
		if(pdpm->apdo_max_volt == 10000 && pdpm->apdo_max_curr > apdo_cap.ma) {
			pr_err("select potential apdo_max_volt %d, apdo_max_volt %d\n", pdpm->apdo_max_volt, pdpm->apdo_max_curr);
		} else {
			pdpm->apdo_max_volt = apdo_cap.max_mv;
			pdpm->apdo_min_volt = apdo_cap.min_mv;
			pdpm->apdo_max_curr = apdo_cap.ma;
		}
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
				if(pdpm->apdo_max_volt == 10000 && pdpm->apdo_max_curr > apdo_cap.ma) {
					pr_err("potential apdo_max_volt %d, apdo_max_volt %d\n", pdpm->apdo_max_volt, pdpm->apdo_max_curr);
				} else {
					pdpm->apdo_max_volt = apdo_cap.max_mv;
					pdpm->apdo_min_volt = apdo_cap.min_mv;
					pdpm->apdo_max_curr = apdo_cap.ma;
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

	ret = usbpd_set_iio_channel(pdpm, MAIN, MAIN_INPUT_CURRENT_SETTLED, aicr);
	if (ret < 0) {
		pr_err("set aicr fail(%d)\n", ret);
		return ret;
	}

	//set ichg
	/* 90% charging efficiency */
	ichg = (90 * pdpm->cp.vbus_volt * aicr / 100) / pdpm->cp.vbat_volt;
	ret = usbpd_set_iio_channel(pdpm, MAIN, MAIN_CHAGER_CURRENT, ichg);
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
	//int ret;
/*
	int val;
	val = !en;
	ret = usbpd_set_iio_channel(pdpm, MAIN, MAIN_CHAGER_HZ, val);
	if (ret < 0) {
		pr_err("disable hz fail(%d)\n", ret);
		return ret;
	}
	val = en;
	ret = usbpd_set_iio_channel(pdpm, MAIN, MAIN_CHARGER_ENABLED, val);
	if (ret < 0) {
		pr_err("en swchg fail(%d)\n", ret);
		return ret;
	}
*/
	if(!en) {
		usbpd_set_iio_channel(pdpm, MAIN, MAIN_INPUT_CURRENT_SETTLED, 2000);
		usbpd_set_iio_channel(pdpm, MAIN, MAIN_CHAGER_CURRENT, 100);
	} else {
		usbpd_set_iio_channel(pdpm, MAIN, MAIN_ENBALE_CHAGER_TERM, true);
//		ret = usbpd_set_iio_channel(pdpm, MAIN, MAIN_CHAGER_HZ, true);
//		msleep(300);
//		ret = usbpd_set_iio_channel(pdpm, MAIN, MAIN_CHAGER_HZ, false);
	}
	pdpm->sw.charge_enabled = en;
	pr_info("en = %d\n", en);

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
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static void usbpd_pm_update_cp_status(struct usbpd_pm *pdpm)
{
    int ret;
	int val1;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SC_BATTERY_VOLTAGE, &val1);
	if (!ret)
        pdpm->cp.vbat_volt = val1;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SC_BUS_VOLTAGE, &val1);
	if (!ret)
        pdpm->cp.vbus_volt = val1;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SC_BUS_CURRENT, &val1);
	if (!ret)
        pdpm->cp.ibus_curr_cp = val1;


    ret = usbpd_get_iio_channel(pdpm, CP_SLAVE,
		 CHARGE_PUMP_SC_BUS_CURRENT, &val1);
	if (!ret)
        pdpm->cp_sec.ibus_curr = val1;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SC_VBUS_ERROR_STATUS, &val1);
    if (!ret)
    {
 //       pr_err(">>>>vbus error state : %02x\n", val1);
        pdpm->cp.vbus_error_low = (val1 >> 5) & 0x01;
        pdpm->cp.vbus_error_high = (val1 >> 4) & 0x01;
    }

    pdpm->cp.ibus_curr = pdpm->cp.ibus_curr_cp ;//+ pdpm->cp.ibus_curr_sw;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SC_BUS_TEMPERATURE, &val1);
    if (!ret)
        pdpm->cp.bus_temp = val1;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SC_BATTERY_TEMPERATURE, &val1);
    if (!ret)
        pdpm->cp.bat_temp = val1;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SC_DIE_TEMPERATURE, &val1);
    if (!ret)
        pdpm->cp.die_temp = val1;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SC_BATTERY_PRESENT, &val1);
    if (!ret)
        pdpm->cp.batt_pres = val1;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SC_VBUS_PRESENT, &val1);
    if (!ret)
        pdpm->cp.vbus_pres = val1;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_CHARGING_ENABLED, &val1);
    if (!ret)
        pdpm->cp.charge_enabled = val1;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SC_ALARM_STATUS, &val1);
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

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SC_FAULT_STATUS, &val1);
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

	ret = usbpd_get_iio_channel(pdpm, CP_SLAVE,
		CHARGE_PUMP_SC_BUS_CURRENT, &val1);
	if (!ret)
        pdpm->cp_sec.ibus_curr = val1;

	ret = usbpd_get_iio_channel(pdpm, CP_SLAVE,
		CHARGE_PUMP_CHARGING_ENABLED, &val1);
    if (!ret)
        pdpm->cp_sec.charge_enabled = val1;
	if(pdpm->isln8000flg) {
		ret = usbpd_get_iio_channel(pdpm, CP_SLAVE,
			 CHARGE_PUMP_SC_ALARM_STATUS, &val1);
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
	}
}

static int usbpd_pm_enable_cp(struct usbpd_pm *pdpm, bool enable)
{
    int ret, val1;

    val1 = enable;
	ret = usbpd_set_iio_channel(pdpm, CP_MASTER,
			CHARGE_PUMP_CHARGING_ENABLED, val1);

    return ret;
}

static int usbpd_pm_enable_cp_sec(struct usbpd_pm *pdpm, bool enable)
{
    int ret, val1;

    val1 = enable;
	ret = usbpd_set_iio_channel(pdpm, CP_SLAVE,
			CHARGE_PUMP_CHARGING_ENABLED, val1);

    return ret;
}

static int usbpd_pm_check_cp_enabled(struct usbpd_pm *pdpm)
{
    int ret, val1;

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
			CHARGE_PUMP_CHARGING_ENABLED, &val1);
    if (!ret)
        pdpm->cp.charge_enabled = !!val1;

    return ret;
}

static int usbpd_pm_check_cp_sec_enabled(struct usbpd_pm *pdpm)
{
    int ret, val1;

	ret = usbpd_get_iio_channel(pdpm, CP_SLAVE,
			CHARGE_PUMP_CHARGING_ENABLED, &val1);
    if (!ret)
        pdpm->cp_sec.charge_enabled = !!val1;

    return ret;
}
#else
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

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_SC_BATTERY_VOLTAGE, &val);
    if (!ret)
        pdpm->cp.vbat_volt = val.intval;

  /*  ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_SC_BATTERY_CURRENT, &val);
    if (!ret)
        pdpm->cp.ibat_curr_cp = val.intval;

    pdpm->cp.ibat_curr = pdpm->cp.ibat_curr_cp ;//+ pdpm->cp.ibat_curr_sw; 
*/
    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_SC_BUS_VOLTAGE, &val);
    if (!ret)
        pdpm->cp.vbus_volt = val.intval; 

    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_SC_BUS_CURRENT, &val);
    if (!ret)
        pdpm->cp.ibus_curr_cp = val.intval;
   /* 
    if (pdpm->cp_sec_psy) {
        ret = power_supply_get_property(pdpm->cp_sec_psy,
            POWER_SUPPLY_PROP_SC_BUS_CURRENT, &val);
        if (!ret)
            pdpm->cp.ibus_curr_sw = val.intval;
    }
    */
    ret = power_supply_get_property(pdpm->cp_psy,
            POWER_SUPPLY_PROP_SC_VBUS_ERROR_STATUS, &val);
    if (!ret)
    {
        pr_err(">>>>vbus error state : %02x\n", val.intval);
        pdpm->cp.vbus_error_low = (val.intval >> 5) & 0x01;
        pdpm->cp.vbus_error_high = (val.intval >> 4) & 0x01;
    }
 
    pdpm->cp.ibus_curr = pdpm->cp.ibus_curr_cp; // + pdpm->cp.ibus_curr_sw;

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
#endif
/*
static int usbpd_pm_enable_sw(struct usbpd_pm *pdpm, bool enable)
{
    int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->sw_psy) {
        pdpm->sw_psy = power_supply_get_by_name("battery");
        if (!pdpm->sw_psy) {
            return -ENODEV;
        }
    }

    if(enable)   val.intval = 3000000;
    else         val.intval = 100000;

    ret = power_supply_set_property(pdpm->sw_psy,
            POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX, &val);

    return ret;
}
*/

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
	//		pdpm->sw.charge_enabled = false;
            pr_err("usbpd_pm_check_sw_enabled : %d\n", val.intval);
        else
	//		pdpm->sw.charge_enabled = true;
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
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	int ret, val1;

	ret = usbpd_get_iio_channel(pdpm, BMS,
		 BMS_CURRENT_NOW, &val1);
	if (!ret)
        pdpm->cp.ibat_curr = -(int)(val1/1000);

	ret = usbpd_get_iio_channel(pdpm, BMS,
		 BMS_VOLTAGE_NOW, &val1);
    if (!ret)
        pdpm->cp.vbat_volt = (int)(val1/1000);

    ret = usbpd_get_iio_channel(pdpm, BMS,
		 BMS_CYCLE_COUNT, &val1);
    if (!ret)
	 pdpm->cp.battery_cycle = val1;

	return ret;
#else
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
        pdpm->cp.ibat_curr = -(int)(val.intval/1000);

    ret = power_supply_get_property(pdpm->bms_psy, 
            POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
    if (!ret)
        pdpm->cp.vbat_volt = (int)(val.intval/1000);

    return ret;
#endif
}

#define CHG_CUR_VOLT_NORMAL     4430
#define CHG_CUR_VOLT1       	4175
#define CHG_CUR_VOLT1_1       	4125
#define CHG_CUR_VOLT2           4285
#define CHG_CUR_VOLT3           4385
#define CHG_CUR_VOLT4           4472
#define CHG_VOLT1_MAX_CUR       12200
#define CHG_VOLT2_MAX_CUR       8620
#define CHG_VOLT3_MAX_CUR       7150
#define CHG_VOLT4_MAX_CUR       5680
#define CHG_VOLT5_MAX_CUR       3230

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
//	int step_temp = 0;
	int step_ibat = 1;
	int step_vbat = 0;
	int bat_temp = 0;
	int step_therm = 0;
	int therm_curr = 0;
	int cycle_volt = 0;

	usbpd_get_iio_channel(pdpm, BMS, BMS_TEMP_NOW, &bat_temp);
	usbpd_get_iio_channel(pdpm, BMS, BMS_THERM_CURR,&therm_curr);

	if (bat_temp >= CHG_BAT_TEMP_MIN && bat_temp < CHG_BAT_TEMP_MAX) {
		pdpm->pps_temp_flag = true;
		if (pdpm->cp.battery_cycle <= 100) {
			cycle_volt = CHG_CUR_VOLT1;
		} else {
			cycle_volt = CHG_CUR_VOLT1_1;
		}
		if (pdpm->cp.vbat_volt < cycle_volt) {
			step_vbat = bat_step(pdpm, CHG_VOLT1_MAX_CUR);
			pdpm->cp.set_ibat_cur = CHG_VOLT1_MAX_CUR;
		} else if (pdpm->cp.vbat_volt >= cycle_volt && pdpm->cp.vbat_volt < CHG_CUR_VOLT2) {
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
	pr_err(">>>>temp %d pdpm->cp.ibus_curr_sw %d step_ibat %d, step_vbat %d, therm_curr %d, cycle %d\n",
		bat_temp, pdpm->cp.ibus_curr_sw, step_ibat, step_vbat, therm_curr, pdpm->cp.battery_cycle);
	return min(step_vbat, step_ibat);
}

static int usbpd_update_apdo_data(struct usbpd_pm *pdpm)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	int ret, val1;

	ret = usbpd_get_iio_channel(pdpm, APDO,
		 APDO_MAX_VOLT, &val1);
	if (!ret)
        pdpm->apdo_max_volt = val1;

	ret = usbpd_get_iio_channel(pdpm, APDO,
		 APDO_MAX_CURR, &val1);
    if (!ret)
        pdpm->apdo_max_curr = val1;

	return ret;
#else
#endif
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
 //   pr_err(">>>>cp enable bit %d\n", pdpm->cp.charge_enabled);
    if (pm_config.cp_sec_enable) {
        usbpd_pm_check_cp_sec_enabled(pdpm);
  //      pr_err(">>>>cp sec enable bit %d\n", pdpm->cp_sec.charge_enabled);
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
    /*TODO: customer can add hook here to check system level 
        * thermal mitigation*/

	step_jeita = battery_sw_jeita(pdpm);
	sw_ctrl_steps = min(sw_ctrl_steps, step_jeita);

    steps = min(sw_ctrl_steps, hw_ctrl_steps);

    pr_err(">>>>>>%d %d %d sw %d hw %d all %d\n", 
            step_vbat, step_ibat, step_ibus, sw_ctrl_steps, hw_ctrl_steps, steps);

    pdpm->request_voltage += steps * 20;

    if (pdpm->request_voltage > 10000)
        pdpm->request_voltage = 10000;

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
			pr_err(">>>>>>>pdpm->cp.vbus_volt %d, vbus_old %d, ads_vol %d\n",
				pdpm->cp.vbus_volt, pdpm->cp.vbus_volt_old, ads_vol);
			if(ads_vol > 500 && ads_vol < 1200) {
				msleep(5);
				usbpd_get_iio_channel(pdpm, CP_MASTER, CHARGE_PUMP_SC_BUS_VOLTAGE, &pdpm->cp.vbus_volt);
				pr_err(">>>>>>> USBPD  exit pd pdpm->cp.vbus_volt %d, i = %d\n", pdpm->cp.vbus_volt, i);
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

    usbpd_get_iio_channel(pdpm, BMS, BMS_THERM_CURR,&therm_curr);
    usbpd_get_iio_channel(pdpm, BMS, BMS_TEMP_NOW, &bat_temp);
	pdpm->cp.batt_temp = bat_temp;
    if(pdpm->sw_psy) {
        ret = power_supply_get_property(pdpm->sw_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
#ifdef WT_COMPILE_FACTORY_VERSION
        if(val.intval >= 79 || pps_change_to_stop() || (therm_curr < 2000))
            pdpm->pps_leave = true;
        else
            pdpm->pps_leave = false;
#else
        if(val.intval >= 98 || pps_change_to_stop() || (therm_curr < 2000))
            pdpm->pps_leave = true;
        else
            pdpm->pps_leave = false;
#endif
		ret = power_supply_get_property(pdpm->sw_psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
		if(val.intval / 1000 > 4400)
			pdpm->voltage_max = val.intval / 1000;
		else
			pdpm->voltage_max = 0;
	} else {
		pdpm->sw_psy = power_supply_get_by_name("battery");
	}

    if(!pdpm->pps_leave) {
        pr_err(">>>>>>>>>>>state phase :%d, vbus_vol %d, vbat_vol %d  vout %d, volatge_max %d pdpm->apdo_max_curr %d\n",
		    pdpm->state, pdpm->cp.vbus_volt, pdpm->cp.vbat_volt, pdpm->cp.vout_volt, pdpm->voltage_max, pdpm->apdo_max_curr);
        pr_err(">>>>>ibus_curr %d  ibus_curr_m %d, ibus_curr_s %d  ibat_curr %d\n",
		    pdpm->cp.ibus_curr + pdpm->cp_sec.ibus_curr, pdpm->cp.ibus_curr_cp, pdpm->cp_sec.ibus_curr, pdpm->cp.ibat_curr);
   //     if(pdpm->state == PD_PM_STATE_FC2_TUNE)
            usbpd_vol_exit(pdpm);
    }

    switch (pdpm->state) {
    case PD_PM_STATE_ENTRY:
        stop_sw = false;
        recover = false;

        if (pdpm->cp.vbat_volt < pm_config.min_vbat_for_cp) {
            pr_notice("batt_volt-%d, waiting...\n", pdpm->cp.vbat_volt);
        } else if (bat_temp >= CHG_BAT_TEMP_45 || bat_temp < CHG_BAT_TEMP_MIN) {
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
        //usbpd_pm_enable_sw(pdpm, false);
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
//        usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_TUNE);
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
         //   if (pdpm->request_voltage_old != pdpm->request_voltage)
            usbpd_select_pdo(pdpm,pdpm->request_voltage, pdpm->request_current);
            pdpm->request_voltage_old = pdpm->request_voltage;
            pr_err("request_voltage:%d, request_current:%d, pdpm->request_voltage_old %d\n",
                    pdpm->request_voltage, pdpm->request_current, pdpm->request_voltage_old);
        }

	//	pr_notice("temp high or lower,pdpm->pps_leave %d, pps_temp_flag %d\n", pdpm->pps_leave, pdpm->pps_temp_flag);
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

        pr_err(">>>sw state %d   %d\n", stop_sw, pdpm->sw.charge_enabled);
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

	//pr_err("usbpd_pm_workfunc\n");
	//usbpd_pm_update_sw_status(pdpm);
	//usbpd_update_ibus_curr(pdpm);
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
		time = 300;
	else
		time = PM_WORK_RUN_INTERVAL;

	if (!usbpd_pm_sm(pdpm) && pdpm->pd_active)
		schedule_delayed_work(&pdpm->pm_work, msecs_to_jiffies(time));
}

static void usbpd_workfunc(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, pd_work.work);

	if (pdpm->cp.vbus_volt <= 7000 && !pdpm->is_pps_en_unlock) {
		if (pdpm->tcpc)
			tcpm_dpm_pd_request(pdpm->tcpc, 9000, 2000, NULL);
		 pr_err("pd request 9V/2A\n");
	}

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
}

#if 0
static void cp_psy_change_work(struct work_struct *work)
{

    struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
                    cp_psy_change_work);

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
    pdpm->psy_change_running = false;
}
#endif

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
		ret = usbpd_set_iio_channel(pdpm, CP_MASTER, CHARGE_PUMP_PRESENT, true);
		if(pm_config.cp_sec_enable)
			ret = usbpd_set_iio_channel(pdpm, CP_SLAVE, CHARGE_PUMP_PRESENT, true);
		usbpd_pd_contact(pdpm, true);
   } else if (pdpm->pd_active && !pdpm->is_pps_en_unlock)
		usbpd_pd_contact(pdpm, false);

 //    pdpm->psy_change_running = false;
}

static int usbpd_check_plugout(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

 //   pr_err("[SC manager] >>>usbpd_check_plugout\n");
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
	int ret;
	static bool firstflg = true;

 //  pr_err("[SC manager] >>>usbpd_psy_notifier_cb event %d, PSY_EVENT_PROP_CHANGED %d\n",
 //         event, PSY_EVENT_PROP_CHANGED);
   if (event != PSY_EVENT_PROP_CHANGED)
        return NOTIFY_OK;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	if(firstflg){
		if(is_cp_chan_valid(pdpm, 0)) {
			pdpm->isln8000flg = false;
			firstflg = false;
			pr_err("[usbpd_psy_notifier_cb] SC8551\n");
		} else if(is_cp_chan_valid(pdpm, IIO_SECOND_CHANNEL_OFFSET)) {
			pdpm->isln8000flg = true;
			firstflg = false;
			pr_err("[usbpd_psy_notifier_cb] LN8000\n");
		}
	}
	if (!pdpm->isln8000flg) {
		ret = is_cp_chan_valid(pdpm, 0);
		if(ret < 0)
			return NOTIFY_OK;
		if (pm_config.cp_sec_enable) {
	        ret = is_cp_psy_chan_valid(pdpm, 0);
			if(ret < 0)
				return NOTIFY_OK;
	    }
	} else {
		ret = is_cp_chan_valid(pdpm, IIO_SECOND_CHANNEL_OFFSET);
		if(ret < 0)
			return NOTIFY_OK;
		if (pm_config.cp_sec_enable) {
	        ret = is_cp_psy_chan_valid(pdpm, IIO_SECOND_CHANNEL_OFFSET);
			if(ret < 0)
				return NOTIFY_OK;
		}
	}
	usbpd_check_charger_psy(pdpm);
	usbpd_check_tcpc(pdpm);
    if (!pdpm->tcpc)
        return NOTIFY_OK;
#else
    usbpd_check_cp_psy(pdpm);
    if (pm_config.cp_sec_enable) {
        usbpd_check_cp_sec_psy(pdpm);
    }

    usbpd_check_charger_psy(pdpm);
    usbpd_check_tcpc(pdpm);
    //usbpd_check_pca_chg_swchg(pdpm);

    if (!pdpm->cp_psy || !pdpm->usb_psy 
        || !pdpm->tcpc)
        //	|| !pdpm->tcpc || !pdpm->sw_chg)
        return NOTIFY_OK;
#endif

    usbpd_check_plugout(pdpm);
 //   pr_err("[SC manager] >>>pdpm->psy_change_running : %d\n", pdpm->psy_change_running);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	if (psy == pdpm->usb_psy) {
		spin_lock_irqsave(&pdpm->psy_change_lock, flags);
        pr_err("[SC manager] >>>pdpm->psy_change_running : %d\n", pdpm->psy_change_running);
        if (!pdpm->psy_change_running) {
            pdpm->psy_change_running = true;
            schedule_work(&pdpm->usb_psy_change_work);
        }
        spin_unlock_irqrestore(&pdpm->psy_change_lock, flags);
    }	
#else
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
#endif
    return NOTIFY_OK;
}

static int  usbpd_iio_init(struct usbpd_pm *pdpm)
{
	pr_err("usbpd_iio_init start\n");
	pdpm->shutdown_flag = false;
	pdpm->isln8000flg = false;
	pdpm->cp_iio = devm_kcalloc(pdpm->dev,
		ARRAY_SIZE(cp_iio_chan_name), sizeof(*pdpm->cp_iio), GFP_KERNEL);
	if (!pdpm->cp_iio)
		return -ENOMEM;
	pdpm->cp_sec_iio = devm_kcalloc(pdpm->dev,
		ARRAY_SIZE(cp_sec_iio_chan_name), sizeof(*pdpm->cp_sec_iio), GFP_KERNEL);
	if (!pdpm->cp_iio)
		return -ENOMEM;
	pdpm->bms_iio = devm_kcalloc(pdpm->dev,
		ARRAY_SIZE(bms_iio_chan_name), sizeof(*pdpm->bms_iio), GFP_KERNEL);
	if (!pdpm->bms_iio)
		return -ENOMEM;
	pdpm->main_iio = devm_kcalloc(pdpm->dev,
		ARRAY_SIZE(main_iio_chan_name), sizeof(*pdpm->main_iio), GFP_KERNEL);
	if (!pdpm->main_iio)
		return -ENOMEM;
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

  //  INIT_WORK(&pdpm->cp_psy_change_work, cp_psy_change_work);
    INIT_WORK(&pdpm->usb_psy_change_work, usb_psy_change_work);

    spin_lock_init(&pdpm->psy_change_lock);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#else
    usbpd_check_cp_psy(pdpm);
    if (pm_config.cp_sec_enable) {
        usbpd_check_cp_sec_psy(pdpm);
    }
#endif

    usbpd_check_charger_psy(pdpm);
    usbpd_check_tcpc(pdpm);
    //usbpd_check_pca_chg_swchg(pdpm);

    INIT_DELAYED_WORK(&pdpm->pm_work, usbpd_pm_workfunc);
    INIT_DELAYED_WORK(&pdpm->pd_work, usbpd_workfunc);
    INIT_DELAYED_WORK(&pdpm->tcp_work, tcp_notify_workfunc);
    schedule_delayed_work(&pdpm->tcp_work, msecs_to_jiffies(1000));

    return 0;
}

static int usbpd_pm_remove(struct platform_device *pdev)
{
	struct usbpd_pm *pdpm = platform_get_drvdata(pdev);

	power_supply_unreg_notifier(&__pdpm->nb);
	cancel_delayed_work(&__pdpm->pm_work);
//	cancel_work_sync(&__pdpm->cp_psy_change_work);
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
	{.compatible = "qcom,cp_manager"},
	{},
};

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
};

static void __exit usbpd_pm_exit(void)
{
	platform_driver_unregister(&usbpd_pm_driver);
}

module_init(usbpd_pm_init);
module_exit(usbpd_pm_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("PPM");
MODULE_DESCRIPTION("pd_policy_manager");
MODULE_VERSION("1.0.0");