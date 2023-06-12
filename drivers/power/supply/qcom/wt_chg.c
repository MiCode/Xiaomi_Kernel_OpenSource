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
#include "wt_chg_iio.h"

#define MSG_OWNER_BC			32778
#define MSG_TYPE_REQ_RESP		1
#define MSG_TYPE_NOTIFY			2

#define MAX_STR_LEN                    128

/* opcode for battery charger */
#define BC_XM_STATUS_GET		0x50
#define BC_XM_STATUS_SET		0x51
#define BC_SET_NOTIFY_REQ		0x04
#define BC_NOTIFY_IND			0x07
#define BC_BATTERY_STATUS_GET		0x30
#define BC_BATTERY_STATUS_SET		0x31
#define BC_USB_STATUS_GET		0x32
#define BC_USB_STATUS_SET		0x33
#define BC_WLS_STATUS_GET		0x34
#define BC_WLS_STATUS_SET		0x35
#define BC_SHIP_MODE_REQ_SET		0x36
#define BC_SHUTDOWN_REQ_SET		0x37
#define BC_WLS_FW_CHECK_UPDATE		0x40
#define BC_WLS_FW_PUSH_BUF_REQ		0x41
#define BC_WLS_FW_UPDATE_STATUS_RESP	0x42
#define BC_WLS_FW_PUSH_BUF_RESP		0x43
#define BC_WLS_FW_GET_VERSION		0x44
#define BC_SHUTDOWN_NOTIFY		0x47
#define BC_GENERIC_NOTIFY		0x80

#if defined(CONFIG_BQ_FG_1S)
#define BATTERY_DIGEST_LEN 32
#else
#define BATTERY_DIGEST_LEN 20
#endif
#define BATTERY_SS_AUTH_DATA_LEN 4

#define USBPD_UVDM_SS_LEN		4
#define USBPD_UVDM_VERIFIED_LEN		1

#define MAX_THERMAL_LEVEL		20

#define BATTERY_VOLTAGET_PRE     3500000
#define BATTERY_VOLTAGET_OFFSET  20000

enum uvdm_state {
	USBPD_UVDM_DISCONNECT,
	USBPD_UVDM_CHARGER_VERSION,
	USBPD_UVDM_CHARGER_VOLTAGE,
	USBPD_UVDM_CHARGER_TEMP,
	USBPD_UVDM_SESSION_SEED,
	USBPD_UVDM_AUTHENTICATION,
	USBPD_UVDM_VERIFIED,
	USBPD_UVDM_REMOVE_COMPENSATION,
	USBPD_UVDM_REVERSE_AUTHEN,
	USBPD_UVDM_CONNECT,
};

enum usb_connector_type {
	USB_CONNECTOR_TYPE_TYPEC,
	USB_CONNECTOR_TYPE_MICRO_USB,
};

enum psy_type {
	PSY_TYPE_BATTERY,
	PSY_TYPE_USB,
	PSY_TYPE_XM,
	PSY_TYPE_MAX,
};

/* property ids */
enum battery_property_id {
	BATT_STATUS,
	BATT_HEALTH,
	BATT_PRESENT,
	BATT_CHG_TYPE,
	BATT_CAPACITY,
	BATT_SOH,
	BATT_VOLT_OCV,
	BATT_VOLT_NOW,
	BATT_VOLT_MAX,
	BATT_CURR_NOW,
	BATT_CHG_CTRL_LIM,
	BATT_CHG_CTRL_LIM_MAX,
	BATT_CONSTANT_CURRENT,
	BATT_TEMP,
	BATT_TECHNOLOGY,
	BATT_CHG_COUNTER,
	BATT_CYCLE_COUNT,
	BATT_CHG_FULL_DESIGN,
	BATT_CHG_FULL,
	BATT_MODEL_NAME,
	BATT_TTF_AVG,
	BATT_TTE_AVG,
	BATT_RESISTANCE,
	BATT_POWER_NOW,
	BATT_POWER_AVG,
	BATT_PROP_MAX,
};

enum usb_property_id {
	USB_ONLINE,
	USB_VOLT_NOW,
	USB_VOLT_MAX,
	USB_CURR_NOW,
	USB_CURR_MAX,
	USB_INPUT_CURR_LIMIT,
	USB_TYPE,
	USB_ADAP_TYPE,
	USB_MOISTURE_DET_EN,
	USB_MOISTURE_DET_STS,
	USB_TEMP,
	USB_REAL_TYPE,
	USB_TYPEC_COMPLIANT,
	USB_SCOPE,
	USB_CONNECTOR_TYPE,
	USB_PROP_MAX,
};

enum xm_property_id {
	XM_PROP_RESISTANCE_ID,
	XM_PROP_VERIFY_DIGEST,
	XM_PROP_CONNECTOR_TEMP,
	XM_PROP_AUTHENTIC,
	XM_PROP_CHIP_OK,
	XM_PROP_SOC_DECIMAL,
	XM_PROP_SOC_DECIMAL_RATE,
	XM_PROP_SHUTDOWN_DELAY,
	XM_PROP_VBUS_DISABLE,
	XM_PROP_CC_ORIENTATION,
	XM_PROP_SLAVE_BATT_PRESENT,
#if defined(CONFIG_BQ2597X)
	XM_PROP_BQ2597X_CHIP_OK,
	XM_PROP_BQ2597X_SLAVE_CHIP_OK,
	XM_PROP_BQ2597X_BUS_CURRENT,
	XM_PROP_BQ2597X_SLAVE_BUS_CURRENT,
	XM_PROP_BQ2597X_BUS_DELTA,
	XM_PROP_BQ2597X_BUS_VOLTAGE,
	XM_PROP_BQ2597X_BATTERY_PRESENT,
	XM_PROP_BQ2597X_SLAVE_BATTERY_PRESENT,
	XM_PROP_BQ2597X_BATTERY_VOLTAGE,
	XM_PROP_COOL_MODE,
#endif
	XM_PROP_BT_TRANSFER_START,
	XM_PROP_MASTER_SMB1396_ONLINE,
	XM_PROP_MASTER_SMB1396_IIN,
	XM_PROP_SLAVE_SMB1396_ONLINE,
	XM_PROP_SLAVE_SMB1396_IIN,
	XM_PROP_SMB_IIN_DIFF,
	XM_PROP_INPUT_SUSPEND,
	XM_PROP_REAL_TYPE,
	/*used for pd authentic*/
	XM_PROP_VERIFY_PROCESS,
	XM_PROP_VDM_CMD_CHARGER_VERSION,
	XM_PROP_VDM_CMD_CHARGER_VOLTAGE,
	XM_PROP_VDM_CMD_CHARGER_TEMP,
	XM_PROP_VDM_CMD_SESSION_SEED,
	XM_PROP_VDM_CMD_AUTHENTICATION,
	XM_PROP_VDM_CMD_VERIFIED,
	XM_PROP_VDM_CMD_REMOVE_COMPENSATION,
	XM_PROP_VDM_CMD_REVERSE_AUTHEN,
	XM_PROP_CURRENT_STATE,
	XM_PROP_ADAPTER_ID,
	XM_PROP_ADAPTER_SVID,
	XM_PROP_PD_VERIFED,
	XM_PROP_PDO2,
	XM_PROP_UVDM_STATE,
	/*****************/
	XM_PROP_FASTCHGMODE,
	XM_PROP_APDO_MAX,
	XM_PROP_THERMAL_REMOVE,
	XM_PROP_VOTER_DEBUG,
	XM_PROP_FG_RM,
	XM_PROP_MTBF_CURRENT,
	XM_PROP_FAKE_TEMP,
	XM_PROP_QBG_VBAT,
	XM_PROP_QBG_VPH_PWR,
	XM_PROP_QBG_TEMP,
	XM_PROP_FB_BLANK_STATE,
	XM_PROP_THERMAL_TEMP,
	XM_PROP_TYPEC_MODE,
	XM_PROP_NIGHT_CHARGING,
	XM_PROP_SMART_BATT,
	XM_PROP_FG1_QMAX,
	XM_PROP_FG1_RM,
	XM_PROP_FG1_FCC,
	XM_PROP_FG1_SOH,
	XM_PROP_FG1_FCC_SOH,
	XM_PROP_FG1_CYCLE,
	XM_PROP_FG1_FAST_CHARGE,
	XM_PROP_FG1_CURRENT_MAX,
	XM_PROP_FG1_VOL_MAX,
	XM_PROP_FG1_TSIM,
	XM_PROP_FG1_TAMBIENT,
	XM_PROP_FG1_TREMQ,
	XM_PROP_FG1_TFULLQ,
	XM_PROP_SHIPMODE_COUNT_RESET,
	XM_PROP_SPORT_MODE,
	XM_PROP_MAX,
};
enum {
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP = 0x80,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5,
	QTI_POWER_SUPPLY_USB_TYPE_USB_FLOAT,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3_CLASSB,
};

struct psy_state {
	struct power_supply	*psy;
	char			*model;
	char			*version;
	const int		*map;
	u32			*prop;
	u32			prop_count;
	u32			opcode_get;
	u32			opcode_set;
};

static const int battery_prop_map[BATT_PROP_MAX] = {
	[BATT_STATUS]		= POWER_SUPPLY_PROP_STATUS,
	[BATT_HEALTH]		= POWER_SUPPLY_PROP_HEALTH,
	[BATT_PRESENT]		= POWER_SUPPLY_PROP_PRESENT,
	[BATT_CHG_TYPE]		= POWER_SUPPLY_PROP_CHARGE_TYPE,
	[BATT_CAPACITY]		= POWER_SUPPLY_PROP_CAPACITY,
	[BATT_VOLT_OCV]		= POWER_SUPPLY_PROP_VOLTAGE_OCV,
	[BATT_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[BATT_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[BATT_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[BATT_CHG_CTRL_LIM]	= POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	[BATT_CHG_CTRL_LIM_MAX]	= POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	[BATT_CONSTANT_CURRENT]	= POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	[BATT_TEMP]		= POWER_SUPPLY_PROP_TEMP,
	[BATT_TECHNOLOGY]	= POWER_SUPPLY_PROP_TECHNOLOGY,
	[BATT_CHG_COUNTER]	= POWER_SUPPLY_PROP_CHARGE_COUNTER,
	[BATT_CYCLE_COUNT]	= POWER_SUPPLY_PROP_CYCLE_COUNT,
	[BATT_CHG_FULL_DESIGN]	= POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	[BATT_CHG_FULL]		= POWER_SUPPLY_PROP_CHARGE_FULL,
	[BATT_MODEL_NAME]	= POWER_SUPPLY_PROP_MODEL_NAME,
	[BATT_TTF_AVG]		= POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	[BATT_TTE_AVG]		= POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	[BATT_POWER_NOW]	= POWER_SUPPLY_PROP_POWER_NOW,
	[BATT_POWER_AVG]	= POWER_SUPPLY_PROP_POWER_AVG,
};

static const int usb_prop_map[USB_PROP_MAX] = {
	[USB_ONLINE]		= POWER_SUPPLY_PROP_ONLINE,
	[USB_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[USB_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[USB_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[USB_CURR_MAX]		= POWER_SUPPLY_PROP_CURRENT_MAX,
	[USB_INPUT_CURR_LIMIT]	= POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	[USB_ADAP_TYPE]		= POWER_SUPPLY_PROP_USB_TYPE,
	[USB_TEMP]		= POWER_SUPPLY_PROP_TEMP,
	[USB_SCOPE]		= POWER_SUPPLY_PROP_SCOPE,
};

static const int xm_prop_map[XM_PROP_MAX] = {

};

/* Standard usb_type definitions similar to power_supply_sysfs.c */
static const char * const power_supply_usb_type_text[] = {
	"Unknown", "USB", "USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
	"USB_PD", "PD_DRP", "PD_PPS", "BrickID", "USB_HVDCP",
	"USB_HVDCP3","USB_HVDCP3P5", "USB_FLOAT"
};

static const char * const power_supply_usbc_text[] = {
	"Nothing attached",
	"Source attached (default current)",
	"Source attached (medium current)",
	"Source attached (high current)",
	"Non compliant",
	"Sink attached",
	"Powered cable w/ sink",
	"Debug Accessory",
	"Audio Adapter",
	"Powered cable w/o sink",
};

static const char * const qc_power_supply_usb_type_text[] = {
	"HVDCP", "HVDCP_3", "HVDCP_3P5","USB_FLOAT","HVDCP_3"
};

static int thermal_mitigation[] = {
	12400000, 12000000, 11500000, 11000000, 10500000,
	10000000, 9500000, 9000000, 8500000, 8000000,
	7500000, 7000000, 6500000, 6000000, 5500000,
	5000000, 4500000, 4000000, 3500000, 3000000,
	2500000, 2000000, 1500000, 1000000, 500000,
};

enum batt_iio_type {
	MAIN,
	BMS,
	CP_MASTER,
	CP_SLAVE,
};

struct batt_dt_props {
	int usb_icl_ua;
	bool no_battery;
	bool hvdcp_disable;
	int sec_charger_config;
	int auto_recharge_vbat_mv;
	int batt_profile_fcc_ua;
	int batt_profile_fv_uv;
	int batt_iterm;
	int jeita_temp_step0;
	int jeita_temp_step1;
	int jeita_temp_step2;
	int jeita_temp_step3;
	int jeita_temp_step4;
	int jeita_temp_step5;
	int jeita_temp_step6;
};

struct batt_chg {
	struct platform_device *pdev;
	struct device *dev;

	struct iio_channel *cp_master_therm;
	struct iio_channel *cp_slave_therm;
	struct iio_channel *connector_therm;

	struct power_supply *batt_psy;
	struct power_supply *usb_psy;

	struct iio_dev          *indio_dev;
	struct iio_chan_spec    *iio_chan;
	struct iio_channel	*int_iio_chans;

	struct iio_channel	**gq_ext_iio_chans;
	struct iio_channel	**cp_ext_iio_chans;
	struct iio_channel	**cp_psy_ext_iio_chans;
	struct iio_channel	**main_chg_ext_iio_chans;

	struct class battery_class;
	struct device batt_device;
	struct psy_state psy_list[PSY_TYPE_MAX];

	struct wakeup_source *wt_ws;
	struct batt_dt_props dt;
	struct delayed_work batt_chg_work;
	struct delayed_work usb_therm_work;
	struct delayed_work usb_type_work;

	struct delayed_work	xm_prop_change_work;
	struct delayed_work	charger_debug_info_print_work;

	int pd_active;
	int real_type;
	int apdo_max_volt;
	int apdo_max_curr;
	int pd_min_vol;
	int pd_max_vol;
	int pd_cur_max;
	int pd_usb_suspend;
	int pd_in_hard_reset;
	int pd_type_cnt;
	int wakeup_flag;
	int cp_master_temp;
	int cp_slave_temp;
	int ui_soc;
	int fastcharge_mode;

	bool node_flag;
	bool is_chg_control;
	bool is_battery_on;
	bool otg_enable;
	bool shutdown_delay_en;

	int therm_step;
	int pd_auth;
	int batt_auth;
	int is_pps_on;
	int batt_id;
	int lcd_on;
	int charging_call_state;
	int polarity_state;
	int mtbf_current;
	int connector_temp;
	int usb_temp;
	int usb_temp_flag;
	int vbus_cnt;
	int cp_master_adc;
	int cp_slave_adc;
	int start_cnt;
	int low_temp_flag;

	int batt_current_max;
	int batt_current_now;
	int batt_voltage_now;
	int batt_done_curr;
	int charge_voltage_max;
	int charge_design_voltage_max;
	int input_batt_current_max;
	int input_batt_current_now;
	int input_suspend;
	int battery_cv;
	int cv_flag;
	int battery_temp;
	int cycle_count;
	int cell_voltage;
	int high_temp_flag;
	int vbus_voltage;
	int input_limit_flag;

	int fake_batt_status;
	int thermal_levels;
	int system_temp_level;
	int batt_iterm;
	int shutdown_flag;
	int typec_mode;
	int mishow_flag;
	int charge_done;
	int update_cont;
	int old_capacity;
	int old_charge_type;
	int usb_float;
	int power_supply_count;
	bool isln8000flg;
};

struct batt_chg *g_batt_chg;

int get_iio_channel(struct batt_chg *chg, const char *propname,
                                        struct iio_channel **chan)
{
        int rc = 0;

        rc = of_property_match_string(chg->dev->of_node,
                                        "io-channel-names", propname);
        if (rc < 0)
                return 0;

        *chan = iio_channel_get(chg->dev, propname);
        if (IS_ERR(*chan)) {
                rc = PTR_ERR(*chan);
                if (rc != -EPROBE_DEFER)
                        pr_err("%s channel unavailable, %d\n", propname, rc);
                *chan = NULL;
        }

        return rc;
}

static bool is_bms_chan_valid(struct batt_chg *chip,
		enum batt_qg_exit_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->gq_ext_iio_chans[chan]))
		return false;

	if (!chip->gq_ext_iio_chans[chan]) {
		chip->gq_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					qg_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->gq_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->gq_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->gq_ext_iio_chans[chan] = NULL;

			pr_err("Failed to get IIO channel %s, rc=%d\n",
				qg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_cp_chan_valid(struct batt_chg *chip,
		enum cp_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->cp_ext_iio_chans[chan]))
		return false;

	if (!chip->cp_ext_iio_chans[chan]) {
		chip->cp_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					cp_iio_chan_name[chan]);
		if (IS_ERR(chip->cp_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->cp_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cp_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cp_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_cp_psy_chan_valid(struct batt_chg *chip,
		enum cp_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->cp_psy_ext_iio_chans[chan]))
		return false;

	if (!chip->cp_psy_ext_iio_chans[chan]) {
		chip->cp_psy_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					cp_sec_iio_chan_name[chan]);
		if (IS_ERR(chip->cp_psy_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->cp_psy_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cp_psy_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cp_sec_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_main_chg_chan_valid(struct batt_chg *chip,
		enum cp_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->main_chg_ext_iio_chans[chan]))
		return false;

	if (!chip->main_chg_ext_iio_chans[chan]) {
		chip->main_chg_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					main_iio_chan_name[chan]);
		if (IS_ERR(chip->main_chg_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->main_chg_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->main_chg_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				main_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

int batt_get_iio_channel(struct batt_chg *chg,
			enum batt_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;
	static bool firstflg = true;

	if(firstflg){
		if(is_cp_chan_valid(chg, 0)) {
			chg->isln8000flg = false;
			firstflg = false;
			pr_err("batt_get_iio_channel SC8551\n");
		} else if(is_cp_chan_valid(chg, IIO_WT_SECOND_OFFSET)) {
			chg->isln8000flg = true;
			firstflg = false;
			pr_err("batt_get_iio_channel LN8000\n");
		}
	}

	if(chg->shutdown_flag)
		return -ENODEV;

	switch (type) {
	case CP_MASTER:
		if (!chg->isln8000flg) {
			if (!is_cp_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_ext_iio_chans[channel];
		} else {
			if (!is_cp_chan_valid(chg, (channel + IIO_WT_SECOND_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_ext_iio_chans[channel + IIO_WT_SECOND_OFFSET];
		}
		break;
	case CP_SLAVE:
		if (!chg->isln8000flg) {
			if (!is_cp_psy_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_psy_ext_iio_chans[channel];
		} else {
			if (!is_cp_psy_chan_valid(chg, (channel + IIO_WT_SECOND_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_psy_ext_iio_chans[channel + IIO_WT_SECOND_OFFSET];
		}
		break;
	case BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->gq_ext_iio_chans[channel];
		break;
	case MAIN:
		if (!is_main_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}

int batt_set_iio_channel(struct batt_chg *chg,
			enum batt_iio_type type, int channel, int val)
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
			iio_chan_list = chg->cp_ext_iio_chans[channel];
		} else {
			if (!is_cp_chan_valid(chg, (channel + IIO_WT_SECOND_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_ext_iio_chans[channel + IIO_WT_SECOND_OFFSET];
		}
		break;
	case CP_SLAVE:
		if (!chg->isln8000flg) {
			if (!is_cp_psy_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_psy_ext_iio_chans[channel];
		} else {
			if (!is_cp_psy_chan_valid(chg, (channel + IIO_WT_SECOND_OFFSET)))
				return -ENODEV;
			iio_chan_list = chg->cp_psy_ext_iio_chans[channel + IIO_WT_SECOND_OFFSET];
		}
		break;
	case BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->gq_ext_iio_chans[channel];
		break;
	case MAIN:
		if (!is_main_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_write_channel_raw(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}

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

int get_charger_pump_master_temp(struct batt_chg *chg,
				 int *val)
{
	int rc, temp;

	if (chg->cp_master_therm) {
		rc = iio_read_channel_processed(chg->cp_master_therm,
				&temp);
		if (rc < 0) {
			pr_err("Error in reading temp channel, rc=%d\n", rc);
			return rc;
		}
		*val = temp / 100;
	} else {
		return -ENODATA;
	}

	return rc;
}

int get_charger_pump_slave_temp(struct batt_chg *chg,
				 int *val)
{
	int rc, temp;

	if (chg->cp_master_therm) {
		rc = iio_read_channel_processed(chg->cp_slave_therm,
				&temp);
		if (rc < 0) {
			pr_err("Error in reading temp channel, rc=%d\n", rc);
			return rc;
		}
		*val = temp / 100;
	} else {
		return -ENODATA;
	}

	return rc;
}

int get_charger_connector_temp(struct batt_chg *chg,
				 int *val)
{
	int rc, temp;

	if (chg->connector_therm) {
		rc = iio_read_channel_processed(chg->connector_therm,
				&temp);
		if (rc < 0) {
			pr_err("Error in reading temp channel, rc=%d\n", rc);
			return rc;
		}
		*val = temp / 100;
	} else {
		return -ENODATA;
	}

	return rc;
}

int get_prop_batt_health(struct batt_chg *chg, union power_supply_propval *val)
{
	int  rc = -EINVAL;
	if (!chg)
		return -EINVAL;

	rc = batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &val->intval);
	if (rc < 0)
		return -EINVAL;

	if (val->intval >= 600)
		val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (val->intval >= 450 && val->intval < 600)
		val->intval = POWER_SUPPLY_HEALTH_WARM;
	else if (val->intval >= 150 && val->intval < 450)
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
	else if (val->intval >= 0 && val->intval < 150)
		val->intval = POWER_SUPPLY_HEALTH_COOL;
	else if (val->intval < 0)
		val->intval = POWER_SUPPLY_HEALTH_COLD;

	return 0;
}

static int sw_battery_set_cv(struct batt_chg *chg)
{
	int cv = 0;
	int charger_term = 0;
	int temp, batt_id;
	int cur_now, cur = 0;
	int charge_full;
	static int cnt = 0;

	if (!chg)
		return -EINVAL;

	batt_get_iio_channel(chg, BMS, BATT_QG_BATTERY_ID, &batt_id);
	batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &temp);
	batt_get_iio_channel(chg, BMS, BATT_QG_CURRENT_NOW, &cur_now);
	batt_get_iio_channel(chg, MAIN, MAIN_CHARGER_DONE, &chg->charge_done);
	batt_get_iio_channel(chg, BMS, BATT_QG_STATUS, &charge_full);
	if(charge_full != POWER_SUPPLY_STATUS_FULL)
		charge_full = 0;
	if (cur_now < 0)
		cur = 0 - (cur_now / 1000);
	chg->batt_id = batt_id;
	if (chg->batt_auth && chg->pd_auth && (chg->system_temp_level != 24)) {
		if (temp >= 150 && temp < 350 && batt_id) {
			charger_term = 896;
			if(cur < charger_term - 40)
				cnt++;
			else
				cnt = 0;
			if (chg->is_pps_on) {
				cv = 4608;
				chg->cv_flag = 0;
			} else {
				if(chg->cell_voltage >= 4475000) {
					if (cur > charger_term){
						chg->batt_done_curr = cur - 64;
						batt_set_iio_channel(chg, MAIN, MAIN_CHAGER_CURRENT, chg->batt_done_curr);
						pr_err("cur %d\n", chg->batt_done_curr);
					} else {
						chg->cv_flag = 1;
					}
				}
				if(cnt > 3 && cur)
					cv = 4440;
				else if (chg->cv_flag)
					cv = 4480;
				else
					cv = 4496;
			}
		} else if (temp >= 350 && temp < 480 && batt_id) {
			if (batt_id == 1)
				charger_term = 960;
			else if(batt_id == 2 || batt_id == 4)
				charger_term = 980 + 30;
			if (cur < charger_term)
				cnt++;
			else
				cnt = 0;
			if (chg->is_pps_on) {
				cv = 4608;
				chg->cv_flag = 0;
			} else {
				if(chg->cell_voltage >= 4475000){
					if (cur > charger_term){
						chg->batt_done_curr = cur - 64;
						batt_set_iio_channel(chg, MAIN, MAIN_CHAGER_CURRENT, chg->batt_done_curr);
						pr_err("cur %d\n", chg->batt_done_curr);
					} else {
						chg->cv_flag = 1;
					}
				}
				if(cnt > 3 && cur)
					cv = 4440;
				else if(chg->cv_flag)
					cv = 4480;
				else
					cv = 4496;
			}
		} else if(temp >= 480) {
			charger_term = 256;
			if(chg->high_temp_flag)
				cv = 4450;
			else
				cv = 4100;
		} else if(temp >= 50 && temp < 150) {
			charger_term = 256;
			if (chg->is_pps_on)
				cv = 4608;
			else
				cv = 4450;
		} else {
			charger_term = 256;
			cv = 4450;
		}
	} else {
		charger_term = 256;
		if (chg->is_pps_on) {
			cv = 4608;
		} else if (temp >= 480 && !chg->high_temp_flag) {
			cv = 4100;
		} else {
			cv = 4450;
		}

	}

	pr_err("cur_now %d, chg->batt_auth %d, chg->pd_auth %d, batt_id %d temp %d, cnt %d,charger_term %d, cv %d,cell_voltage %d,charge_done %d\n",
		cur, chg->batt_auth, chg->pd_auth, batt_id, temp, cnt, charger_term, cv, chg->cell_voltage, chg->charge_done);

	chg->battery_cv = cv;
//	if(chg->ui_soc != 100) {
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_VOLTAGE_TERM, cv);
		batt_set_iio_channel(chg, MAIN, MAIN_CHAGER_TERM, charger_term);
//	}

	return 0;
}

static int sw_battery_recharge(struct batt_chg *chg)
{
	int r_soc;
	int en = 0;

	batt_get_iio_channel(chg, BMS, BATT_QG_CC_SOC, &r_soc);
	if(r_soc <= 9750 && chg->charge_done && r_soc > 9000){
		en = 0;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, en);
		pr_err("r_soc %d,en %d\n", r_soc, en);
		msleep(100);
		en = 1;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, en);
	}
	pr_err("r_soc %d,en %d\n", r_soc, en);
	return 0;
}

#define CHG_BAT_TEMP_0	340
#define CHG_BAT_TEMP_1	370
#define CHG_BAT_TEMP_2	380
#define CHG_BAT_TEMP_3	390
#define CHG_BAT_TEMP_4	400
#define CHG_BAT_TEMP_5	410
#define CHG_BAT_TEMP_6	420
#define CHG_BAT_TEMP_7	430
#define CHG_BAT_TEMP_8	440
#define CHG_BAT_TEMP_9	450
#define CHG_BAT_TEMP_10 460
#define CHG_BAT_TEMP_11 470

int power_off_charge_them(struct batt_chg *chg, int temp)
{
	int cur;

	if (temp < CHG_BAT_TEMP_1) {
		if (chg->therm_step == 0) {
			chg->therm_step = 0;
			cur = 12200;
		} else {
			if (temp < CHG_BAT_TEMP_0) {
				cur = 12200;
				chg->therm_step = 0;
			} else {
				cur = 11000;
				chg->therm_step = 1;
			}
		}
	} else if (temp >= CHG_BAT_TEMP_1 && temp < CHG_BAT_TEMP_2) {
		if (chg->therm_step <= 1) {
			cur = 11000;
			chg->therm_step = 1;
		} else if (chg->therm_step == 2) {
			if(temp < CHG_BAT_TEMP_1) {
				cur = 11000;
				chg->therm_step = 1;
			} else {
				cur = 10000;
				chg->therm_step = 2;
			}
		} else {
			cur = 10000;
			chg->therm_step = 2;
		}
	} else if (temp >= CHG_BAT_TEMP_2 && temp < CHG_BAT_TEMP_3) {
		if (chg->therm_step <= 2) {
			cur = 10000;
			chg->therm_step = 2;
		} else if(chg->therm_step == 3) {
			if(temp < CHG_BAT_TEMP_2) {
				cur = 10000;
				chg->therm_step = 2;
			} else {
				cur = 9000;
				chg->therm_step = 3;
			}
		} else {
			cur = 9000;
			chg->therm_step = 3;
		}
	} else if (temp >= CHG_BAT_TEMP_3 && temp < CHG_BAT_TEMP_4) {
		if (chg->therm_step <= 3) {
			cur = 9000;
			chg->therm_step = 3;
		} else if(chg->therm_step == 4) {
			if(temp < CHG_BAT_TEMP_3) {
				cur = 9000;
				chg->therm_step = 3;
			} else {
				cur = 8000;
				chg->therm_step = 4;
			}
		} else {
			cur = 8000;
			chg->therm_step = 4;
		}
	} else if (temp >= CHG_BAT_TEMP_4 && temp < CHG_BAT_TEMP_5) {
		if (chg->therm_step <= 4) {
			cur = 8000;
			chg->therm_step = 4;
		} else if(chg->therm_step == 5) {
			if(temp < CHG_BAT_TEMP_4) {
				cur = 8000;
				chg->therm_step = 4;
			} else {
				cur = 7000;
				chg->therm_step = 5;
			}
		} else {
			cur = 7000;
			chg->therm_step = 5;
		}
	} else if (temp >= CHG_BAT_TEMP_5 && temp < CHG_BAT_TEMP_6) {
		if (chg->therm_step <= 5) {
			cur = 7000;
			chg->therm_step = 5;
		} else if (chg->therm_step == 6) {
			if(temp < CHG_BAT_TEMP_5) {
				cur = 7000;
				chg->therm_step = 5;
			} else {
				cur = 7000;
				chg->therm_step = 6;
			}
		} else {
			cur = 6000;
			chg->therm_step = 6;
		}
	} else if (temp >= CHG_BAT_TEMP_6 && temp < CHG_BAT_TEMP_7) {
		if (chg->therm_step <= 6) {
			cur = 6000;
			chg->therm_step = 6;
		} else if (chg->therm_step == 7) {
			if(temp < CHG_BAT_TEMP_6) {
				cur = 6000;
				chg->therm_step = 6;
			} else {
				cur = 5000;
				chg->therm_step = 7;
			}
		} else {
			cur = 5000;
			chg->therm_step = 7;
		}
	} else if (temp >= CHG_BAT_TEMP_7 && temp < CHG_BAT_TEMP_8) {
		if (chg->therm_step <= 7) {
			cur = 5000;
			chg->therm_step = 7;
		} else if (chg->therm_step == 8) {
			if (temp < CHG_BAT_TEMP_7) {
				cur = 5000;
				chg->therm_step = 7;
			} else {
				cur = 4000;
				chg->therm_step = 8;
			}
		} else {
			cur = 4000;
			chg->therm_step = 8;
		}
	} else if (temp >= CHG_BAT_TEMP_8 && temp < CHG_BAT_TEMP_9) {
		if (chg->therm_step <= 8) {
			cur = 4000;
			chg->therm_step = 8;
		} else if (chg->therm_step == 9) {
			if (temp < CHG_BAT_TEMP_8) {
				cur = 4000;
				chg->therm_step = 8;
			} else {
				cur = 3000;
				chg->therm_step = 9;
			}
		} else {
			cur = 3000;
			chg->therm_step = 9;
		}
	} else if (temp >= CHG_BAT_TEMP_9 && temp < CHG_BAT_TEMP_11) {
		if (chg->therm_step <= 9) {
			cur = 3000;
			chg->therm_step = 9;
		} else if (chg->therm_step == 10) {
			if(temp < CHG_BAT_TEMP_10) {
				cur = 3000;
				chg->therm_step = 9;
			} else {
				cur = 2000;
				chg->therm_step = 10;
			}
		} else {
			cur = 2000;
			chg->therm_step = 10;
		}
	} else if (temp >= CHG_BAT_TEMP_11 && temp < 480) {
		cur = 2000;
		chg->therm_step = 10;
	} else {
		cur = 2000;
	}
	return cur;
}

int set_jeita_lcd_on_off(bool lcdon)
{
	g_batt_chg->lcd_on = lcdon;
	pr_err("%s, lcd_on %d\n", __func__, lcdon);
	return 0;
}
EXPORT_SYMBOL(set_jeita_lcd_on_off);

int get_jeita_lcd_on_off(void)
{
	return g_batt_chg->lcd_on;
}

static int sw_battery_jeita(struct batt_chg *chg)
{
	int en = 0;
	int cur = 0, them_curr;
	int temp;
	int type = 0;
	int type_cur = 0;
	int input_cur = 0;

	if(!chg)
		return -EINVAL;

	batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &temp);
	if(temp < chg->dt.jeita_temp_step0){
		en = 0;
	} else if(temp >= chg->dt.jeita_temp_step0 && temp < chg->dt.jeita_temp_step1) {
		en = 1;
		if(chg->batt_voltage_now <= 4200000 && chg->low_temp_flag != 1)
			cur = 1000;
		else {
			cur = 710;
			chg->low_temp_flag = 1;
		}
	} else if(temp >= chg->dt.jeita_temp_step1 && temp < chg->dt.jeita_temp_step2) {
		en = 1;
		cur = 2450;
	} else if(temp >= chg->dt.jeita_temp_step2 && temp < chg->dt.jeita_temp_step3) {
		en = 1;
		cur = 3500;
	} else if(temp >= chg->dt.jeita_temp_step3 && temp < chg->dt.jeita_temp_step4) {
		en = 1;
		cur = 5700;
	} else if(temp >= chg->dt.jeita_temp_step4 && temp < chg->dt.jeita_temp_step5) {
		en = 1;
		cur = 5700;
	} else if(temp >= chg->dt.jeita_temp_step5 && temp < chg->dt.jeita_temp_step6) {
		en = 1;
		cur = 2450;
	} else if(temp >= chg->dt.jeita_temp_step6){
		en = 0;
	}

	batt_get_iio_channel(chg, BMS, BATT_QG_VOLTAGE_NOW, &chg->cell_voltage);

	if (chg->batt_auth && chg->pd_auth) {
		if (temp >= chg->dt.jeita_temp_step4 && temp < chg->dt.jeita_temp_step5)
			cur = 12200;
		if (get_boot_mode()) {
			them_curr = power_off_charge_them(chg, temp);
			cur = 12050;
		} else
			them_curr = thermal_mitigation[chg->system_temp_level] / 1000;
		cur = min(cur, them_curr);
		batt_set_iio_channel(chg, BMS, BATT_QG_FCC_MAX, cur);
		if (chg->is_pps_on && !chg->fastcharge_mode && chg->pd_auth) {
			batt_set_iio_channel(chg, BMS, BATT_QG_FASTCHARGE_MODE, true);
			chg->fastcharge_mode = 1;
		}
	} else {
		if (get_boot_mode())
			them_curr = power_off_charge_them(chg, temp);
		else
			them_curr = thermal_mitigation[chg->system_temp_level] / 1000;
		cur = min(cur, them_curr);
		batt_set_iio_channel(chg, BMS, BATT_QG_FCC_MAX, cur);
	}

	batt_get_iio_channel(chg, MAIN, MAIN_CHARGER_TYPE, &type);
	if (chg->real_type == POWER_SUPPLY_TYPE_USB_PD) {
		if(type == POWER_SUPPLY_TYPE_USB) {
			input_cur = 1500;
			type_cur = 1500;
		} else if (type == POWER_SUPPLY_TYPE_USB_CDP){
			if(chg->ui_soc == 100)
				input_cur = 1200;
			else
				input_cur = 1500;
			type_cur = 1500;
		} else if (type == POWER_SUPPLY_TYPE_USB_FLOAT){
			input_cur = 1500;
			type_cur = 3010;
		} else if(type == POWER_SUPPLY_TYPE_USB_DCP){
			input_cur = 2000;
			if(get_boot_mode())
				type_cur = 3000;
			else
				type_cur = 3010;
		} else {
			input_cur = 2000;
			if(get_boot_mode())
				type_cur = 3000;
			else
				type_cur = 3010;
		}
		if (chg->pd_auth){
			input_cur = 3000;
		}
	} else {
		if(type == POWER_SUPPLY_TYPE_USB) {
			if(get_boot_mode())
				input_cur = 500;
			else
				input_cur = 500;
			type_cur = 512;
		} else if (type == POWER_SUPPLY_TYPE_USB_CDP) {
			if(chg->ui_soc == 100)
				input_cur = 1200;
			else
				input_cur = 1500;
			type_cur = 1500;
		} else if (type == POWER_SUPPLY_TYPE_USB_DCP) {
			type_cur = 2000;
			if(get_boot_mode()) {
				input_cur = 1950;
			} else
				input_cur = 2000;
		} else if (type == POWER_SUPPLY_TYPE_USB_HVDCP) {
			if(get_boot_mode())
				type_cur = 3000;
			else
				type_cur = 3010;
			if((chg->vbus_voltage > 5600 && chg->vbus_voltage < 8600) || chg->input_limit_flag){
				input_cur = 1200;
				chg->input_limit_flag = 1;
			} else {
				input_cur = 2000;
			}
			if(get_boot_mode())
				input_cur = 2000;	
		} else if (type == POWER_SUPPLY_TYPE_USB_FLOAT) {
			type_cur = 1000;
			input_cur = 1000;
		} else {
			type_cur = 500;
			input_cur = 500;
		}
	}

	cur = min(cur, type_cur);

	if(chg->mtbf_current > 500 && chg->mtbf_current <= 4000)
		batt_set_iio_channel(chg, MAIN, MAIN_INPUT_CURRENT_SETTLED, chg->mtbf_current);

	if (chg->charging_call_state)
		cur = min(cur, 1000);
	if (chg->batt_auth && chg->pd_auth && chg->ui_soc >= 98 && chg->batt_done_curr)
		cur = min(chg->batt_done_curr, cur);
	if(!chg->batt_id)
		cur = min(1999, cur);
	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_CHARGING_ENABLED, &chg->is_pps_on);
	if(chg->is_pps_on) {
		if(chg->pd_auth)
			chg->charge_voltage_max = 4480000;
		else
			chg->charge_voltage_max = 4450000;
		chg->batt_done_curr = 0;
		if(chg->ui_soc <= 50)
			input_cur = 100;
		else
			input_cur = 1500;
		batt_set_iio_channel(chg, MAIN, MAIN_CHAGER_CURRENT, 100);
	} else {
		chg->charge_voltage_max = 4450000;
		if(chg->battery_temp > 480 && chg->cell_voltage > 4110000) {
			en = 0;
			chg->high_temp_flag = 1;
		} else {
			chg->high_temp_flag = 0;
		}
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, en);
		batt_set_iio_channel(chg, MAIN, MAIN_CHAGER_CURRENT, cur);
	}
	batt_set_iio_channel(chg, MAIN, MAIN_INPUT_CURRENT_SETTLED, input_cur);
	pr_err("temp = %d,input_cur %d curr=%d, en %d, is_pps %d,mtbf_current %d, type %d cv_flag %d , input_limit_flag %d\n",
		temp, input_cur, cur, en, chg->is_pps_on, chg->mtbf_current, type, chg->cv_flag, chg->input_limit_flag);

	return 0;
}

static int get_real_type(struct batt_chg *chg)
{
	int type, real_type;
	if(!chg)
		return -EINVAL;

	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BUS_VOLTAGE, &chg->vbus_voltage);
	batt_get_iio_channel(chg, MAIN, MAIN_CHARGER_TYPE, &type);
	real_type = type;

	if (chg->pd_active == QTI_POWER_SUPPLY_PD_ACTIVE
		|| chg->pd_active == QTI_POWER_SUPPLY_PD_PPS_ACTIVE) {
		if(type || chg->vbus_voltage > 8500)
			real_type = POWER_SUPPLY_TYPE_USB_PD;
		else
			real_type = POWER_SUPPLY_TYPE_USB;
	}

	if(chg->old_charge_type == POWER_SUPPLY_TYPE_USB_FLOAT && real_type == POWER_SUPPLY_TYPE_UNKNOWN && !chg->usb_float) {
		chg->usb_float = 1;
		cancel_delayed_work(&chg->usb_type_work);
		schedule_delayed_work(&chg->usb_type_work, msecs_to_jiffies(1500));
	}

	if(type)
		chg->pd_type_cnt = 0;

	if(chg->otg_enable || chg->mishow_flag)
		real_type = 0;

	if (chg->vbus_cnt > 3) {
		chg->vbus_cnt = 0;
		chg->pd_active = 0;
	}

	if(chg->pd_type_cnt > 50 && type == POWER_SUPPLY_TYPE_UNKNOWN){
			real_type = POWER_SUPPLY_TYPE_USB;
	}

	if(!real_type) {
		chg->pd_auth = 0;
		chg->therm_step = 0;
		chg->batt_done_curr = 0;
		chg->apdo_max_curr = 0;
		if (chg->fastcharge_mode) {
			batt_set_iio_channel(chg, BMS, BATT_QG_FASTCHARGE_MODE, false);
			chg->fastcharge_mode = 0;
		}
	}

	if(chg->real_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		chg->charge_done = 0;
		chg->input_limit_flag = 0;
		if(chg->battery_cv < 4450) {
			chg->battery_cv = 4450;
			batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_VOLTAGE_TERM, chg->battery_cv);
		}
	}

	if(chg->usb_float){
		real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
	}

	chg->real_type = real_type;
	if(chg->old_charge_type != real_type) {
		chg->update_cont = 15;
		chg->pd_type_cnt = 0;
		power_supply_changed(chg->batt_psy);
		cancel_delayed_work(&chg->xm_prop_change_work);
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(10));
		cancel_delayed_work(&chg->batt_chg_work);
		schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(1000));
	}
	chg->old_charge_type = real_type;
	return real_type;
}

static int get_usb_real_type(struct batt_chg *chg)
{
	int type, real_type;
	if(!chg)
		return -EINVAL;

	batt_get_iio_channel(chg, MAIN, MAIN_CHARGER_TYPE, &type);
	real_type = type;

	if(type)
		chg->pd_type_cnt = 0;

	if(chg->usb_float){
		real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
	}

	if (chg->pd_active == QTI_POWER_SUPPLY_PD_ACTIVE
		|| chg->pd_active == QTI_POWER_SUPPLY_PD_PPS_ACTIVE) {
		if(type)
			real_type = POWER_SUPPLY_TYPE_USB_PD;
		else
			real_type = POWER_SUPPLY_TYPE_USB_PD;
	}
	if(chg->otg_enable || chg->mishow_flag || (chg->vbus_cnt > 3))
		real_type = 0;
	if(!real_type) {
		chg->pd_auth = 0;
		chg->therm_step = 0;
		chg->batt_done_curr = 0;
		chg->charge_done = 0;
	}

	if(chg->pd_type_cnt > 50 && type == POWER_SUPPLY_TYPE_UNKNOWN){
			real_type = POWER_SUPPLY_TYPE_USB_FLOAT;
	}

	if(chg->real_type == POWER_SUPPLY_TYPE_UNKNOWN)
		chg->charge_done = 0;

	chg->real_type = real_type;

	return real_type;
}

struct quick_charge adapter_cap[10] = {
	{ POWER_SUPPLY_TYPE_USB,        QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_DCP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_CDP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_ACA,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_FLOAT,  QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_TYPE_USB_PD,       QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP,    QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_USB_HVDCP_3,  QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_TYPE_WIRELESS,     QUICK_CHARGE_FAST },
	{0, 0},
};

int get_quick_charge_type(struct batt_chg *chg)
{
	int i = 0;

	if (!chg)
		return 0;

	chg->real_type = get_real_type(chg);
	if(chg->real_type == POWER_SUPPLY_TYPE_USB_PD){
		if(((chg->apdo_max_volt * chg->apdo_max_curr)/1000000 > 20) && ((chg->apdo_max_volt * chg->apdo_max_curr)/1000000 < 27))
			return QUICK_CHARGE_FLASH;
		else if(((chg->apdo_max_volt * chg->apdo_max_curr)/1000000 >= 27) && ((chg->apdo_max_volt * chg->apdo_max_curr)/1000000 <= 50))
			return QUICK_CHARGE_TURBE;
		else if((chg->apdo_max_volt * chg->apdo_max_curr)/1000000 > 50)
			return QUICK_CHARGE_TURBE;
		else
			return QUICK_CHARGE_FAST;
		//	return QUICK_CHARGE_SUPER;
	} else {
		while (adapter_cap[i].adap_type != 0) {
			if (chg->real_type == adapter_cap[i].adap_type) {
				pr_err("get_quick_charge_type is i=%d\n", i);
				return adapter_cap[i].adap_cap;
			}
			i++;
		}
	}
	pr_err("get_quick_charge_type is not supported in usb\n");

	return 0;
}

int get_apdo_max(struct batt_chg *chg)
{
	if (!chg)
		return 0;

	chg->real_type = get_real_type(chg);
	if(chg->real_type == POWER_SUPPLY_TYPE_USB_PD){
		return ((chg->apdo_max_volt * chg->apdo_max_curr)/1000000);
	}
	pr_err("get_apdo_max is not supported in usb\n");

	return 0;
}

static int batt_get_prop_batt_charge_type(struct batt_chg *chip,
		union power_supply_propval *val)
{
	int  rc = -EINVAL;
	if (!chip)
		return -EINVAL;

	if(get_real_type(chip)){
		if(chip->batt_voltage_now <= BATTERY_VOLTAGET_PRE)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		else if(chip->batt_voltage_now >= chip->dt.batt_profile_fv_uv - BATTERY_VOLTAGET_OFFSET) {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		}  else {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE;
		}
	} else {
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
	}

	return rc;
}

int batt_set_prop_system_temp_level(struct batt_chg *chg,
				const union power_supply_propval *val)
{
	int cur;
	if (val->intval < 0)
		return -EINVAL;

	if (chg->thermal_levels <= 0)
		return -EINVAL;

	if (val->intval > chg->thermal_levels)
		return -EINVAL;

	chg->system_temp_level = val->intval;
	cur = thermal_mitigation[val->intval] / 1000;
	sw_battery_jeita(chg);
	pr_err("batt_set_prop_system_temp_levels %d, cur = %d\n", chg->system_temp_level, cur);
	return 0;
}

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
	int current_cur = 0;
	int voltage_now = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_STATUS, &pval->intval);
		rc = get_real_type(chg);
		if (pval->intval == POWER_SUPPLY_STATUS_DISCHARGING && rc > 0)
			pval->intval = POWER_SUPPLY_STATUS_CHARGING;
		if(pval->intval == POWER_SUPPLY_STATUS_FULL) {
			if(chg->real_type)
				pval->intval = POWER_SUPPLY_STATUS_CHARGING;
			else
				pval->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		if (chg->charge_done && chg->ui_soc == 100)
			pval->intval = POWER_SUPPLY_STATUS_FULL;
		if(chg->usb_temp > 680 && pval->intval == POWER_SUPPLY_STATUS_CHARGING)
			pval->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		rc = get_prop_batt_health(chg, pval);
		if(rc != 0)
			pval->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_PRESENT, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		pval->intval = chg->input_batt_current_max;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		rc = batt_get_prop_batt_charge_type(chg, pval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CAPACITY, &pval->intval);
		if(pval->intval == -107)
			chg->is_battery_on = false;
		else
			chg->is_battery_on = true;
		if (!chg->is_battery_on) {
			pr_err("no battery,batt capacity is 15\n");
			pval->intval = 15;
			rc = batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BATTERY_VOLTAGE, &voltage_now);
			if(voltage_now < 3500)
				pval->intval = 1;
		}
//		pr_err("read batt capacity is %d\n", pval->intval);
		chg->ui_soc = pval->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		pval->intval = chg->system_temp_level;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		pval->intval = chg->thermal_levels;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if(chg->real_type && chg->battery_temp > 0 && chg->battery_temp < 150) {
			rc = batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BATTERY_VOLTAGE, &voltage_now);
			pval->intval = (voltage_now + 5) * 1000;
		} if (chg->real_type && chg->battery_temp >= 480) {
			rc = batt_get_iio_channel(chg, MAIN, MAIN_VBAT_VOLTAGE, &voltage_now);
			pval->intval = (voltage_now + 5) * 1000;
		} else {
			batt_get_iio_channel(chg, BMS, BATT_QG_VOLTAGE_NOW, &pval->intval);
		}
		if (!chg->is_battery_on)
			pval->intval = 3800000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pval->intval = chg->charge_voltage_max;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CURRENT_NOW, &current_cur);
		if (!chg->is_battery_on)
			pval->intval = 0;
		pval->intval -= current_cur;
		if(chg->ui_soc == 100 && current_cur < 0 && current_cur > -10000 && chg->pd_auth) {
			pval->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		pval->intval = chg->batt_current_max;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		pval->intval = chg->batt_current_now ;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		pval->intval = chg->batt_iterm ;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &pval->intval);
		if (!chg->is_battery_on)
			pval->intval = 250;
#ifdef CONFIG_DISABLE_TEMP_PROTECT
		pval->intval = 250;
#endif
		if(get_boot_mode() && pval->intval >= 590)
			pval->intval = 590;
		chg->battery_temp = pval->intval;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		pval->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CHARGE_FULL, &current_cur);
		pval->intval = (current_cur * chg->ui_soc) / 100;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CYCLE_COUNT, &pval->intval);
		chg->cycle_count = pval->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CHARGE_FULL, &pval->intval);
		if(chg->battery_temp >=10 && chg->battery_temp <= 480 && pval->intval < 4500000 && chg->cycle_count < 100)
			pval->intval += 100000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CHARGE_FULL_DESIGN, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_TIME_TO_FULL_NOW, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_TIME_TO_EMPTY_NOW, &pval->intval);
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

static int batt_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int rc = 0;
	struct batt_chg *chg = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == POWER_SUPPLY_STATUS_FULL)
			chg->fake_batt_status = val->intval;
		else
			chg->fake_batt_status = -EINVAL;
		power_supply_changed(chg->batt_psy);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		chg->input_batt_current_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		rc = batt_set_prop_system_temp_level(chg, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		batt_set_iio_channel(chg, BMS, BATT_QG_CAPACITY, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		chg->charge_voltage_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		chg->batt_current_max = val->intval;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		rc = batt_set_iio_channel(chg, BMS, BATT_QG_TEMP, val->intval);
	default:
		rc = -EINVAL;
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

/************************
 * USB PSY REGISTRATION *
 ************************/
static enum power_supply_property usb_psy_props[] = {
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
	POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE,
};

static int usb_psy_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int rc = 0, type = 0;
	struct batt_chg *chg = power_supply_get_drvdata(psy);
	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_PRESENT, &val->intval);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if(get_real_type(chg) > 0)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chg->charge_design_voltage_max;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = chg->charge_voltage_max;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_VOLTAGE_NOW, &chg->batt_voltage_now);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		rc = batt_get_iio_channel(chg, BMS, BATT_QG_CURRENT_NOW, &chg->batt_current_now);
		val->intval = chg->batt_current_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = chg->batt_current_max;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		type = get_real_type(chg);
		if (type == POWER_SUPPLY_TYPE_USB || type == POWER_SUPPLY_TYPE_USB_CDP)
			val->intval = POWER_SUPPLY_TYPE_USB;
		else
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = POWER_SUPPLY_SCOPE_SYSTEM;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = chg->input_batt_current_max;
		break;
	case POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE:
		val->intval = get_quick_charge_type(chg);
		//pr_err("test get_quick_charge_type %d\n", val->intval);
		if (chg->battery_temp >= 580)
			val->intval = QUICK_CHARGE_NORMAL;
		break;
	default:
		pr_err("get prop %d is not supported in usb\n", psp);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int usb_psy_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct batt_chg *chip = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		chip->batt_current_now = val->intval;
		break;
	default:
		pr_err("Set prop %d is not supported in usb psy\n",
				psp);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int usb_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_POWER_NOW:
		return 1;
	default:
		break;
	}

	return 0;
}

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB_PD,
	.properties = usb_psy_props,
	.num_properties = ARRAY_SIZE(usb_psy_props),
	.get_property = usb_psy_get_prop,
	.set_property = usb_psy_set_prop,
	.property_is_writeable = usb_psy_prop_is_writeable,
};

static int wt_init_usb_psy(struct batt_chg *chg)
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

static int batt_init_config(struct batt_chg *chg)
{
	chg->input_batt_current_max = 6100000;
	chg->batt_current_max = 12200000;
	chg->mishow_flag = 0;
	chg->charge_voltage_max = 4450000;
	chg->charge_design_voltage_max = 11000000;
	chg->system_temp_level = 0;
	chg->batt_iterm = 250;
	chg->thermal_levels = 24;
	chg->shutdown_flag = false;
	chg->dt.jeita_temp_step0 = -100;
	chg->dt.jeita_temp_step1 = 0;
	chg->dt.jeita_temp_step2 = 50;
	chg->dt.jeita_temp_step3 = 100;
	chg->dt.jeita_temp_step4 = 150;
	chg->dt.jeita_temp_step5 = 480;
	chg->dt.jeita_temp_step6 = 580;
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

	rc = get_iio_channel(chg, "main_therm", &chg->cp_master_therm);
	if (rc < 0)
		return rc;

	rc = get_iio_channel(chg, "slave_therm", &chg->cp_slave_therm);
	if (rc < 0)
		return rc;

	rc = get_iio_channel(chg, "usb_therm", &chg->connector_therm);
	if (rc < 0)
		return rc;

	return 0;
};

static void batt_chg_main(struct work_struct *work)
{
	int val = 0;
	int temp;
	int soc;
	int time = 0;

	struct batt_chg *chg = container_of(work,
				struct batt_chg, batt_chg_work.work);

	if (!chg)
		return;

	chg->start_cnt++;
	if(chg->start_cnt > 1000000)
		chg->start_cnt = 100;

	val = get_charger_pump_master_temp(chg, &chg->cp_master_temp);
	val = get_charger_pump_slave_temp(chg, &chg->cp_slave_temp);
	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BUS_VOLTAGE, &chg->vbus_voltage);
	batt_get_iio_channel(chg, BMS, BATT_QG_TEMP, &temp);
	batt_get_iio_channel(chg, BMS, BATT_QG_CAPACITY, &soc);
	pr_err("wt workup cp_master_temp %d, cp_slave_temp %d, get_boot_mode = %d, lcdon %d, vbus %d, count %d, count %d, soc %d fastcharge_mode %d\n",
		chg->cp_master_temp, chg->cp_slave_temp, get_boot_mode(), g_batt_chg->lcd_on, chg->vbus_voltage, chg->update_cont, chg->power_supply_count, soc, chg->fastcharge_mode);

	if(chg->mishow_flag) {
		if(chg->real_type)
			val = 1;
		else
			val = 0;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
		power_supply_changed(chg->usb_psy);
		pr_err("mishow capacity %d,mishow_flag %d\n", chg->ui_soc, chg->mishow_flag);
	}

	if(chg->mtbf_current == 1500 && chg->ui_soc <= 40){
		val = 0;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
		pr_err("mtbf %d\n", chg->ui_soc, val);
	}

	if(chg->ui_soc <= 1) {
		cancel_delayed_work(&chg->xm_prop_change_work);
		chg->update_cont = 15;
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(300));
	}

	if(chg->real_type) {
		if(!chg->wakeup_flag) {
			__pm_stay_awake(chg->wt_ws);
			chg->wakeup_flag = 1;
			pr_err("wt workup\n");
		}
		if (!chg->typec_mode && chg->pd_type_cnt > 50)
			chg->pd_type_cnt = 0;

		if(chg->usb_float){
			pr_err("chg->usb_float\n");
			schedule_delayed_work(&chg->usb_type_work, msecs_to_jiffies(1000));
		}

		if(chg->vbus_voltage < 4000)
			chg->vbus_cnt++;
		else
			chg->vbus_cnt = 0;
		if (!chg->isln8000flg) {
			if (!chg->cp_master_adc) {
				chg->cp_master_adc = 1;
				batt_set_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_ADC_ENABLE, chg->cp_master_adc);
			}
			if (!chg->cp_slave_adc) {
				chg->cp_slave_adc = 1;
				batt_set_iio_channel(chg, CP_SLAVE, CHARGE_PUMP_SC_ADC_ENABLE, chg->cp_slave_adc);
			}
		}
		sw_battery_jeita(chg);
		sw_battery_set_cv(chg);
		if (chg->real_type == POWER_SUPPLY_TYPE_USB || chg->real_type == POWER_SUPPLY_TYPE_USB_CDP)
			sw_battery_recharge(chg);
		chg->power_supply_count = 0;
		power_supply_changed(chg->usb_psy);
		if(chg->is_pps_on) {
			if (chg->start_cnt <= 60)
				time = 1000;
			else
				time = 3000;
			batt_set_iio_channel(chg, MAIN, MAIN_ENBALE_CHAGER_TERM, false);
			power_supply_changed(chg->batt_psy);
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(time));
		} else {
			if (chg->start_cnt <= 40)
				time = 1500;
			else if(chg->high_temp_flag && chg->pd_auth && chg->battery_temp < 500)
				time = 1000;
			else if(chg->ui_soc >= 99 && (chg->batt_current_now != 0)){
				time = 2000;
			} else  {
				time = 10000;
			}
			if (get_boot_mode())
				time = 30000;

			if(chg->battery_temp < 455 && chg->battery_temp >= 448)
				time = 2000;
			batt_set_iio_channel(chg, MAIN, MAIN_ENBALE_CHAGER_TERM, true);
			power_supply_changed(chg->batt_psy);
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(time));
		}
	} else {
		if(chg->wakeup_flag) {
			__pm_relax(chg->wt_ws);
			chg->wakeup_flag = 0;
			pr_err("wt workup relax\n");
		}
		if (soc != chg->old_capacity || temp > 550 || chg->power_supply_count >= 6 || soc <= 10) {
			chg->power_supply_count = 0;
			chg->old_capacity = soc;
			power_supply_changed(chg->batt_psy);
		} else {
			chg->power_supply_count += 1;
		}
		if (chg->fastcharge_mode) {
			batt_set_iio_channel(chg, BMS, BATT_QG_FASTCHARGE_MODE, false);
			chg->fastcharge_mode = 0;
		}

		chg->cv_flag = 0;

		if (!chg->isln8000flg && !chg->typec_mode && chg->is_battery_on) {
			if (chg->cp_master_adc) {
				chg->cp_master_adc = 0;
				batt_set_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_ADC_ENABLE, chg->cp_master_adc);
			}
			if (chg->cp_slave_adc) {
				chg->cp_slave_adc = 0;
				batt_set_iio_channel(chg, CP_SLAVE, CHARGE_PUMP_SC_ADC_ENABLE, chg->cp_slave_adc);
			}
		}

		chg->low_temp_flag = 0;
		chg->charge_voltage_max = 4450000;
		if(soc <= 10 || temp > 550 || temp < 50)
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(10000));
		else
			schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(30000));
	}
#ifdef WT_COMPILE_FACTORY_VERSION
	if(chg->ui_soc >= 79) {
		val = 1;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
	//	val = 0;
	//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, val);
		chg->is_chg_control = true;
		pr_err("ATO version capacity %d,node_flag %d\n", chg->ui_soc, chg->is_chg_control);
	}
	if(chg->ui_soc <= 60 && chg->is_chg_control) {
		val = 0;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
	//	val = 1;
	//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, val);
		chg->is_chg_control = false;
		pr_err("ATO version capacity %d,node_flag %d\n", chg->ui_soc, chg->is_chg_control);
	}
#endif

}

static void usb_therm(struct work_struct *work)
{
	int val;
	int temp;
	struct batt_chg *chg = container_of(work,
				struct batt_chg, usb_therm_work.work);

	get_charger_connector_temp(chg, &temp);
	if(chg->connector_temp != 0) {
		val = chg->connector_temp;
	} else {
		val = temp;
	}
#ifdef WT_COMPILE_JP_CHARGE
	chg->usb_temp = val;
	if(val >= 680){
		chg->usb_temp_flag = 1;
		batt_set_iio_channel(chg, BMS, BATT_QG_FCC_MAX, 1000);
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, true);
		pr_err("chg->connector_temp %d stop charging, temp %d", val, temp);
	} else {
		if(chg->usb_temp_flag) {
			batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, -1);
			chg->usb_temp_flag = 0;
		}
		pr_err("chg->connector_temp %d", val);
	}

	schedule_delayed_work(&chg->usb_therm_work, msecs_to_jiffies(3000));
#endif
}

static void batt_usb_type(struct work_struct *work)
{
	struct batt_chg *chg = container_of(work,
				struct batt_chg, usb_type_work.work);

	chg->usb_float = 0;
	chg->old_charge_type = POWER_SUPPLY_TYPE_UNKNOWN;
	power_supply_changed(chg->batt_psy);
	power_supply_changed(chg->usb_psy);
	pr_err("batt_usb_type chg->usb_float %d", chg->usb_float);
}

static const char *get_usb_type_name(u32 usb_type)
{
	u32 i;

	if (usb_type >= QTI_POWER_SUPPLY_USB_TYPE_HVDCP &&
	    usb_type <= QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3_CLASSB) {
		for (i = 0; i < ARRAY_SIZE(qc_power_supply_usb_type_text);
		     i++) {
			if (i == (usb_type - QTI_POWER_SUPPLY_USB_TYPE_HVDCP))
				return qc_power_supply_usb_type_text[i];
		}
		return "Unknown";
	}

	for (i = 0; i < ARRAY_SIZE(power_supply_usb_type_text); i++) {
		if (i == usb_type)
			return power_supply_usb_type_text[i];
	}

	return "Unknown";
}

/*static int read_property_id(struct batt_chg *chg,
			struct psy_state *pst, u32 prop_id)
{
	return 0;
}*/

static ssize_t usb_real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	chg->real_type = get_usb_real_type(chg);
	val = chg->real_type - 3;

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static CLASS_ATTR_RO(usb_real_type);

static ssize_t real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	int val;

	chg->real_type = get_usb_real_type(chg);
	val = chg->real_type - 3;

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static CLASS_ATTR_RO(real_type);

static ssize_t pd_auth_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = chg->pd_auth;
	power_supply_changed(chg->batt_psy);
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t pd_auth_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->pd_auth = val;
	pr_err("pd_auth = %d\n", chg->pd_auth);

	return count;
}
static CLASS_ATTR_RW(pd_auth);

static ssize_t cp_ibus_slave_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, CP_SLAVE, CHARGE_PUMP_SC_BUS_CURRENT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_ibus_slave);

static ssize_t cp_ibus_master_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BUS_CURRENT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_ibus_master);

static ssize_t input_current_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;
	int temp;

	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BUS_CURRENT, &temp);
	val = temp;
	batt_get_iio_channel(chg, CP_SLAVE, CHARGE_PUMP_SC_BUS_CURRENT, &temp);
	val += temp;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(input_current);

static ssize_t cp_temp_slave_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = get_charger_pump_slave_temp(chg, &chg->cp_slave_temp);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->cp_slave_temp);
}
static CLASS_ATTR_RO(cp_temp_slave);

static ssize_t cp_temp_master_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = get_charger_pump_master_temp(chg, &chg->cp_master_temp);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->cp_master_temp);
}
static CLASS_ATTR_RO(cp_temp_master);

static ssize_t cp_present_slave_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, CP_SLAVE, CHARGE_PUMP_SC_PRESENT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_present_slave);

static ssize_t cp_present_master_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_PRESENT, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_present_master);

static ssize_t cp_vbus_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, CP_MASTER, CHARGE_PUMP_SC_BUS_VOLTAGE, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cp_vbus_voltage);

static ssize_t cell_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;
	batt_get_iio_channel(chg, BMS, BATT_QG_VOLTAGE_NOW, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(cell_voltage);

static ssize_t vbus_voltage_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, MAIN, MAIN_BUS_VOLTAGE, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(vbus_voltage);

static ssize_t typec_cc_orientation_show(struct class *c,
				struct class_attribute *attr, char *buf)
{

	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n",chg->polarity_state);

}
static CLASS_ATTR_RO(typec_cc_orientation);

static ssize_t stopcharging_test_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = 1;
	batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
//	val = 0;
//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, val);
	chg->node_flag = true;
	power_supply_changed(chg->usb_psy);
	power_supply_changed(chg->batt_psy);
	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->node_flag);
}
static CLASS_ATTR_RO(stopcharging_test);

static ssize_t startcharging_test_show(struct class *c,
				struct class_attribute *attr, char *buf)
{

	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = 0;
	chg->node_flag = false;
	batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
//	val = 1;
//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, val);
	power_supply_changed(chg->usb_psy);
	power_supply_changed(chg->batt_psy);
	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->node_flag);

}
static CLASS_ATTR_RO(startcharging_test);

static ssize_t charging_call_state_show(struct class *c,
				struct class_attribute *attr, char *buf)
{

	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->charging_call_state);

}
static ssize_t charging_call_state_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->charging_call_state = val;
	pr_err("chg->charging_call_state enable= %d\n", chg->charging_call_state);

	return count;
}
static CLASS_ATTR_RW(charging_call_state);

static ssize_t soc_decimal_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, BMS, BATT_QG_SOC_DECIMAL, &val);
	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
static CLASS_ATTR_RO(soc_decimal);

static ssize_t soc_decimal_rate_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, BMS, BATT_QG_SOC_DECIMAL_RATE, &val);
	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
static CLASS_ATTR_RO(soc_decimal_rate);

static ssize_t shutdown_delay_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int rc = 0;
	int vaule = 0;

	pr_err(" %s start vaule = %d\n", __func__,vaule);

	rc = batt_get_iio_channel(chg, BMS, BATT_QG_SHUTDOWN_DELAY, &vaule);
	if (rc < 0) {
		pr_err("Couldn't get prop rc = %d\n", rc);
		return -ENODATA;
	}
	if(chg->ui_soc > 1)
		vaule = 0;
	if (!chg->is_battery_on && chg->ui_soc == 1) {
		pr_err("no battery,batt capacity is 1\n");
		vaule = 1;
	}
	pr_err(" %s vaule = %d\n", __func__,vaule);

	return scnprintf(buf, PAGE_SIZE, "%d", vaule);
}
static CLASS_ATTR_RO(shutdown_delay);

#ifdef CONFIG_WT_QGKI
extern int get_verify_digest(char *buf);
extern int set_verify_digest(u8 *rand_num);
static ssize_t verify_digest_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	get_verify_digest(buf);

	return scnprintf(buf, PAGE_SIZE, "%s", buf);
}

static ssize_t verify_digest_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	char kbuf[70] = {0};
	pr_err("verify_digest_store = %s\n", buf);
	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);

	set_verify_digest(kbuf);
	return count;
}

static CLASS_ATTR_RW(verify_digest);
#endif

int pps_change_to_stop(void)
{
	int val;
	if(g_batt_chg->input_suspend)
		val = 1;
	else
		val = 0;
	return val;
}
EXPORT_SYMBOL(pps_change_to_stop);

static ssize_t input_suspend_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d", chg->input_suspend);
}

static ssize_t input_suspend_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->input_suspend = val;
	pr_err("chg->input_suspend = %d\n", chg->input_suspend);
	if(val == 1) {
		chg->input_suspend  = 1;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, chg->input_suspend);
	//	chg->input_suspend = 0;
	//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, chg->input_suspend);
	//	chg->input_suspend  = 1;
	}
	if(val == 0) {
		chg->input_suspend = 0;
		batt_set_iio_channel(chg, MAIN, MAIN_CHARGER_HZ, val);
	//	chg->input_suspend = 1;
	//	batt_set_iio_channel(chg, MAIN, MAIN_CHARGING_ENABLED, val);
	//	chg->input_suspend = 0;
	}
	power_supply_changed(chg->usb_psy);
	power_supply_changed(chg->batt_psy);
	return count;
}
static CLASS_ATTR_RW(input_suspend);

static ssize_t battery_name_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	if(chg->batt_id == 1)
		return scnprintf(buf, PAGE_SIZE, "S88006_SWD_4V45_5000mAh\n");
	else if(chg->batt_id == 2)
		return scnprintf(buf, PAGE_SIZE, "S88006_NVT_4V45_5000mAh\n");
	else if(chg->batt_id == 4)
		return scnprintf(buf, PAGE_SIZE, "S88006_NVT_4V45_5000mAh\n");
	else
		return scnprintf(buf, PAGE_SIZE, "NOT_DEFAULT_BATTERY\n");
}
static CLASS_ATTR_RO(battery_name);

static ssize_t chip_ok_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	int val;

	batt_get_iio_channel(chg, BMS, BATT_QG_CHIP_OK, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(chip_ok);

static ssize_t authentic_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = chg->batt_auth;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t authentic_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->batt_auth = val;
	batt_set_iio_channel(chg, BMS, BATT_QG_BATTERY_AUTH, val);
	pr_err("batt_auth = %d\n", chg->batt_auth);

	return count;
}
static CLASS_ATTR_RW(authentic);

static ssize_t quick_charge_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", get_quick_charge_type(chg));
}
static CLASS_ATTR_RO(quick_charge_type);

static ssize_t apdo_max_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", get_apdo_max(chg));
}
static CLASS_ATTR_RO(apdo_max);

static const char * const usb_typec_mode_text[] = {
	"Nothing attached", "Source attached", "Sink attached",
	"Audio Adapter", "Non compliant",
};

static ssize_t typec_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int type;

	type = get_usb_real_type(chg);
	if (chg->typec_mode == 0 && type != 0){
		type = 1;
	} else {
		type = chg->typec_mode;
	}
	pr_err(" %s vaule = %d\n", __func__, type);
	return scnprintf(buf, PAGE_SIZE, "%s", usb_typec_mode_text[type]);
}
static CLASS_ATTR_RO(typec_mode);

static ssize_t mtbf_current_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	val = chg->mtbf_current;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t mtbf_current_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->mtbf_current = val;

	batt_set_iio_channel(chg, MAIN, MAIN_INPUT_CURRENT_SETTLED, val);
	pr_err("mtbf_current = %d\n", chg->mtbf_current);

	return count;
}
static CLASS_ATTR_RW(mtbf_current);

static ssize_t fastcharge_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, BMS, BATT_QG_FASTCHARGE_MODE, &chg->fastcharge_mode);
	val = chg->fastcharge_mode;
	if (chg->battery_temp < chg->dt.jeita_temp_step4 || chg->battery_temp > chg->dt.jeita_temp_step5)
		val = 0;
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(fastcharge_mode);

static ssize_t soh_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	batt_get_iio_channel(chg, BMS, BATT_QG_RESISTANCE_ID, &val);

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(soh);

static ssize_t resistance_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	int val = 0;

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}
static CLASS_ATTR_RO(resistance);

static ssize_t connector_temp_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;
	int temp;

	get_charger_connector_temp(chg, &temp);
	if(chg->connector_temp != 0) {
		val = chg->connector_temp;
	} else {
		val = temp;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t connector_temp_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct batt_chg *chg = container_of(c, struct batt_chg,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	chg->connector_temp = val;

	pr_err("connector_temp = %d\n", chg->connector_temp);

	return count;
}
static CLASS_ATTR_RW(connector_temp);

static struct attribute *battery_class_attrs[] = {
	&class_attr_soc_decimal.attr,
	&class_attr_shutdown_delay.attr,
	&class_attr_soc_decimal_rate.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_real_type.attr,
	&class_attr_pd_auth.attr,
	&class_attr_cell_voltage.attr,
	&class_attr_vbus_voltage.attr,
	&class_attr_cp_vbus_voltage.attr,
	&class_attr_input_current.attr,
	&class_attr_cp_ibus_slave.attr,
	&class_attr_cp_ibus_master.attr,
	&class_attr_cp_present_slave.attr,
	&class_attr_cp_present_master.attr,
	&class_attr_cp_temp_slave.attr,
	&class_attr_cp_temp_master.attr,
	&class_attr_charging_call_state.attr,
#ifdef CONFIG_WT_QGKI
	&class_attr_verify_digest.attr,
#endif
	&class_attr_authentic.attr,
	&class_attr_battery_name.attr,
	&class_attr_chip_ok.attr,
	&class_attr_typec_cc_orientation.attr,
	&class_attr_stopcharging_test.attr,
	&class_attr_startcharging_test.attr,
	&class_attr_input_suspend.attr,
	&class_attr_quick_charge_type.attr,
	&class_attr_apdo_max.attr,
	&class_attr_typec_mode.attr,
	&class_attr_mtbf_current.attr,
	&class_attr_fastcharge_mode.attr,
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_connector_temp.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class);

#ifdef CONFIG_WT_QGKI
static ssize_t dev_verify_digest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	get_verify_digest(buf);

	return scnprintf(buf, PAGE_SIZE, "%s", buf);
}

static ssize_t dev_verify_digest_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char kbuf[70] = {0};
	pr_err("verify_digest_store = %s\n", buf);
	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);

	set_verify_digest(kbuf);
	return count;
}

static DEVICE_ATTR(verify_digest, 0660, dev_verify_digest_show, dev_verify_digest_store);
#endif
static ssize_t dev_real_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int val;
	int type, real_type;

	batt_get_iio_channel(g_batt_chg, MAIN, MAIN_CHARGER_TYPE, &type);
	real_type = get_usb_real_type(g_batt_chg);
/*
	if(real_type == POWER_SUPPLY_TYPE_USB_PD && type){
		if(type == POWER_SUPPLY_TYPE_USB)
			real_type = POWER_SUPPLY_TYPE_USB;
		else
			real_type = POWER_SUPPLY_TYPE_USB_PD;
	} else {
		real_type = POWER_SUPPLY_TYPE_UNKNOWN;
	}
*/
	val = real_type - 3;

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static DEVICE_ATTR(real_type, 0664, dev_real_type_show, NULL);

static ssize_t dev_pd_auth_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	return scnprintf(buf, PAGE_SIZE, "%d\n", g_batt_chg->pd_auth);
}

static ssize_t dev_pd_auth_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	g_batt_chg->pd_auth = val;
	pr_err("g_batt_chg->pd_auth enable= %d\n", g_batt_chg->pd_auth);

	return count;
}
static DEVICE_ATTR(pd_auth, 0664, dev_pd_auth_show, dev_pd_auth_store);

static ssize_t dev_authentic_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", g_batt_chg->batt_auth);
}

static ssize_t dev_authentic_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	g_batt_chg->batt_auth = val;
	pr_err("g_batt_chg->batt_auth enable= %d\n", g_batt_chg->batt_auth);

	return count;
}
static DEVICE_ATTR(authentic, 0664, dev_authentic_show, dev_authentic_store);

static ssize_t dev_input_suspend_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d", g_batt_chg->input_suspend);
}

static ssize_t dev_input_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	g_batt_chg->input_suspend = val;
	pr_err("dev chg->input_suspend = %d\n", g_batt_chg->input_suspend);
	if(val == 1) {
		g_batt_chg->input_suspend  = 1;
		batt_set_iio_channel(g_batt_chg, MAIN, MAIN_CHARGER_HZ, g_batt_chg->input_suspend);
	//	g_batt_chg->input_suspend = 0;
	//	batt_set_iio_channel(g_batt_chg, MAIN, MAIN_CHARGING_ENABLED, g_batt_chg->input_suspend);
	//	g_batt_chg->input_suspend  = 1;
		g_batt_chg->mishow_flag = true;
	}
	if(val == 0) {
		g_batt_chg->input_suspend = 0;
		batt_set_iio_channel(g_batt_chg, MAIN, MAIN_CHARGER_HZ, val);
	//	g_batt_chg->input_suspend = 1;
	//	batt_set_iio_channel(g_batt_chg, MAIN, MAIN_CHARGING_ENABLED, val);
	//	g_batt_chg->input_suspend = 0;
		g_batt_chg->mishow_flag = false;
	}
	power_supply_changed(g_batt_chg->usb_psy);
	power_supply_changed(g_batt_chg->batt_psy);
	return count;
}
static DEVICE_ATTR(input_suspend, 0664, dev_input_suspend_show, dev_input_suspend_store);

static struct attribute *battery_attributes[] = {
#ifdef CONFIG_WT_QGKI
	&dev_attr_verify_digest.attr,
#endif
	&dev_attr_real_type.attr,
	&dev_attr_pd_auth.attr,
	&dev_attr_authentic.attr,
	&dev_attr_input_suspend.attr,
	NULL,
};

static const struct attribute_group battery_attr_group = {
	.attrs = battery_attributes,
};

static const struct attribute_group *battery_attr_groups[] = {
	&battery_attr_group,
	NULL,
};

static int wt_init_dev_class(struct batt_chg *chg)
{
	int rc = -EINVAL;
	if(!chg)
		return rc;

	chg->battery_class.name = "qcom-battery";
	chg->battery_class.class_groups = battery_class_groups;
	rc = class_register(&chg->battery_class);
	if (rc < 0) {
		pr_err("Failed to create battery_class rc=%d\n", rc);
	}

	chg->batt_device.class = &chg->battery_class;
	dev_set_name(&chg->batt_device, "odm_battery");
	chg->batt_device.parent = chg->dev;
	chg->batt_device.groups = battery_attr_groups;
	rc = device_register(&chg->batt_device);
	if (rc < 0) {
		pr_err("Failed to create battery_class rc=%d\n", rc);
	}

	return rc;
}

static int batt_node_map(struct batt_chg *chg)
{
	int i;
	if(!chg)
		return -ENOMEM;

	chg->psy_list[PSY_TYPE_BATTERY].map = battery_prop_map;
	chg->psy_list[PSY_TYPE_BATTERY].prop_count = BATT_PROP_MAX;
	chg->psy_list[PSY_TYPE_BATTERY].opcode_get = BC_BATTERY_STATUS_GET;
	chg->psy_list[PSY_TYPE_BATTERY].opcode_set = BC_BATTERY_STATUS_SET;
	chg->psy_list[PSY_TYPE_USB].map = usb_prop_map;
	chg->psy_list[PSY_TYPE_USB].prop_count = USB_PROP_MAX;
	chg->psy_list[PSY_TYPE_USB].opcode_get = BC_USB_STATUS_GET;
	chg->psy_list[PSY_TYPE_USB].opcode_set = BC_USB_STATUS_SET;
	chg->psy_list[PSY_TYPE_XM].prop_count = XM_PROP_MAX;
	chg->psy_list[PSY_TYPE_XM].opcode_get = BC_XM_STATUS_GET;
	chg->psy_list[PSY_TYPE_XM].opcode_set = BC_XM_STATUS_SET;

	for (i = 0; i < PSY_TYPE_MAX; i++) {
		chg->psy_list[i].prop =
			devm_kcalloc(chg->dev, chg->psy_list[i].prop_count,
					sizeof(u32), GFP_KERNEL);
		if (!chg->psy_list[i].prop)
			return -ENOMEM;
	}

	chg->psy_list[PSY_TYPE_BATTERY].model =
		devm_kzalloc(chg->dev, MAX_STR_LEN, GFP_KERNEL);
	if (!chg->psy_list[PSY_TYPE_BATTERY].model)
		return -ENOMEM;

	pr_err("wt_node_map start\n");
	return 0;
}

#define MAX_UEVENT_LENGTH 50
static void generate_xm_charge_uvent(struct work_struct *work)
{
	int count;
	struct batt_chg *chg = container_of(work, struct batt_chg, xm_prop_change_work.work);

	static char uevent_string[][MAX_UEVENT_LENGTH+1] = {
		"POWER_SUPPLY_SOC_DECIMAL=\n",	//length=31+8
		"POWER_SUPPLY_SOC_DECIMAL_RATE=\n",	//length=31+8
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n",//28+8
	};
	static char *envp[] = {
		uevent_string[0],
		uevent_string[1],
		uevent_string[2],

		NULL,

	};
	char *prop_buf = NULL;

	count = chg->update_cont;
	if(chg->update_cont < 0)
		return;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return;

	soc_decimal_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[0]+25, prop_buf,MAX_UEVENT_LENGTH-25);

	soc_decimal_rate_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[1]+30, prop_buf,MAX_UEVENT_LENGTH-30);

	shutdown_delay_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[2]+28, prop_buf,MAX_UEVENT_LENGTH-28);

	pr_err("uevent test : %s\n %s\n %s count %d\n",
			envp[0], envp[1], envp[2], count);

	/*add our prop end*/

	kobject_uevent_env(&chg->dev->kobj, KOBJ_CHANGE, envp);

	free_page((unsigned long)prop_buf);
	chg->update_cont = count - 1;
	if(chg->ui_soc > 1)
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(500));
	else
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(2000));
	return;
}

#define CHARGING_PERIOD_S		30
#define DISCHARGE_PERIOD_S		300
static void xm_charger_debug_info_print_work(struct work_struct *work)
{
	struct batt_chg *chg = container_of(work, struct batt_chg, charger_debug_info_print_work.work);
	int interval = DISCHARGE_PERIOD_S;
	static int update_cnt_old;

	if(chg->start_cnt != update_cnt_old) {
		update_cnt_old = chg->start_cnt;
	} else {
		cancel_delayed_work(&chg->batt_chg_work);
		schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(100));
	}
	pr_err("update_cnt_old %d\n", update_cnt_old);
	schedule_delayed_work(&chg->charger_debug_info_print_work, interval * HZ);
}

static int batt_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct batt_chg *chg = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_PD_ACTIVE:
		chg->pd_active = val1;
		chg->update_cont = 15;
		batt_set_iio_channel(chg, MAIN, MAIN_INPUT_CURRENT_SETTLED, 512);
		cancel_delayed_work(&chg->xm_prop_change_work);
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(100));
		cancel_delayed_work(&chg->batt_chg_work);
		schedule_delayed_work(&chg->batt_chg_work, msecs_to_jiffies(1000));
		power_supply_changed(chg->usb_psy);
		power_supply_changed(chg->batt_psy);
		break;
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
		chg->pd_usb_suspend = val1;
		break;
	case PSY_IIO_PD_IN_HARD_RESET:
		chg->pd_in_hard_reset = val1;
		pr_err("chg->pd_in_hard_reset  %d\n", chg->pd_in_hard_reset);
		if(chg->pd_in_hard_reset)
			chg->pd_type_cnt = 0;
		break;
	case PSY_IIO_PD_CURRENT_MAX:
		chg->pd_cur_max = val1;
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		chg->pd_min_vol = val1;
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		chg->pd_max_vol = val1;
		break;
	case PSY_IIO_OTG_ENABLE:
		chg->otg_enable = val1;
		batt_set_iio_channel(chg, MAIN, MAIN_OTG_ENABLE, val1);
		pr_err("enable otg %d\n", chg->otg_enable);
		break;
	case PSY_IIO_TYPEC_CC_ORIENTATION:
		chg->polarity_state = val1;
		break;
	case PSY_IIO_APDO_MAX_VOLT:
		chg->apdo_max_volt = val1;
		pr_err("set Achg->apdo_max_volt(%d)\n", chg->apdo_max_volt);
		break;
	case PSY_IIO_APDO_MAX_CURR:
		chg->apdo_max_curr = val1;
		pr_err("set Achg->apdo_max_curr(%d)\n", chg->apdo_max_curr);
		power_supply_changed(chg->batt_psy);
		break;
	case PSY_IIO_TYPEC_MODE:
		chg->typec_mode = val1;
		pr_err("chg->typec_mode(%d)\n", chg->typec_mode);
		break;
	default:
		pr_debug("Unsupported QG IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}
	if (rc < 0)
		pr_err_ratelimited("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int batt_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct batt_chg *chip = iio_priv(indio_dev);
	int rc = 0;

	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_PD_ACTIVE:
		*val1 = chip->pd_active;;
		break;
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
		*val1 = chip->pd_usb_suspend;
		break;
	case PSY_IIO_PD_IN_HARD_RESET:
		*val1 = chip->pd_in_hard_reset;
		break;
	case PSY_IIO_PD_CURRENT_MAX:
		*val1 = chip->pd_cur_max;
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		*val1 = chip->pd_min_vol;
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		*val1 = chip->pd_max_vol;;
		break;
	case PSY_IIO_USB_REAL_TYPE:
		*val1 = get_real_type(chip);
		if(*val1 == 0) {
			chip->pd_type_cnt++;
		}
		break;
	case PSY_IIO_OTG_ENABLE:
		*val1 = chip->otg_enable;
		break;
	case PSY_IIO_TYPEC_CC_ORIENTATION:
		*val1 = chip->polarity_state;
		break;
	case PSY_IIO_APDO_MAX_VOLT:
		*val1 = chip->apdo_max_volt;
		break;
	case PSY_IIO_APDO_MAX_CURR:
		*val1 = chip->apdo_max_curr;
		break;

	default:
		pr_debug("Unsupported battery IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0) {
		pr_err_ratelimited("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int batt_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct batt_chg *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(battery_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info battery_iio_info = {
	.read_raw	= batt_iio_read_raw,
	.write_raw	= batt_iio_write_raw,
	.of_xlate	= batt_iio_of_xlate,
};

static int batt_init_iio_psy(struct batt_chg *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(battery_iio_psy_channels);
	int rc, i;

	pr_err("battery_init_iio_psy start\n");
	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);
	if (!chip->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &battery_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "batt_chg";
	for (i = 0; i < num_iio_channels; i++) {
		chip->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = battery_iio_psy_channels[i].channel_num;
		chan->type = battery_iio_psy_channels[i].type;
		chan->datasheet_name =
			battery_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			battery_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			battery_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register battery IIO device, rc=%d\n", rc);

	pr_err("battery IIO device, rc=%d\n", rc);
	return rc;
}

static int batt_ext_init_iio_psy(struct batt_chg *chip)
{
	if (!chip)
		return -ENOMEM;

	chip->gq_ext_iio_chans = devm_kcalloc(chip->dev,
				ARRAY_SIZE(qg_ext_iio_chan_name),
				sizeof(*chip->gq_ext_iio_chans),
				GFP_KERNEL);
	if (!chip->gq_ext_iio_chans)
		return -ENOMEM;

	chip->cp_ext_iio_chans = devm_kcalloc(chip->dev,
		ARRAY_SIZE(cp_iio_chan_name), sizeof(*chip->cp_ext_iio_chans), GFP_KERNEL);
	if (!chip->cp_ext_iio_chans)
		return -ENOMEM;

	chip->cp_psy_ext_iio_chans = devm_kcalloc(chip->dev,
		ARRAY_SIZE(cp_sec_iio_chan_name), sizeof(*chip->cp_psy_ext_iio_chans), GFP_KERNEL);
	if (!chip->cp_psy_ext_iio_chans)
		return -ENOMEM;

	chip->main_chg_ext_iio_chans = devm_kcalloc(chip->dev,
		ARRAY_SIZE(main_iio_chan_name), sizeof(*chip->main_chg_ext_iio_chans), GFP_KERNEL);
	if (!chip->main_chg_ext_iio_chans)
		return -ENOMEM;

	return 0;
}

static int batt_chg_probe(struct platform_device *pdev)
{
	struct batt_chg *batt_chg = NULL;
	struct iio_dev *indio_dev;
	int rc;

	if (pdev->dev.of_node) {
		indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct batt_chg));
		if (!indio_dev) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	} else {
		return -ENODEV;
	}

	batt_chg = iio_priv(indio_dev);
	batt_chg->indio_dev = indio_dev;
	batt_chg->dev = &pdev->dev;
	batt_chg->pdev = pdev;
	platform_set_drvdata(pdev, batt_chg);

	g_batt_chg = batt_chg;

	rc = batt_init_iio_psy(batt_chg);
	if (rc < 0) {
		pr_err("Failed to initialize QG IIO PSY, rc=%d\n", rc);
	}

	rc = batt_ext_init_iio_psy(batt_chg);
	if (rc < 0) {
		pr_err("Failed to initialize QG IIO PSY, rc=%d\n", rc);
	}

	rc = batt_parse_dt(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
#ifdef CONFIG_WT_QGKI
		return rc;
#endif
	}

	rc = batt_init_config(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		return rc;
	}

	rc = batt_node_map(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		return rc;
	}

	platform_set_drvdata(pdev, batt_chg);

	rc = wt_init_batt_psy(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = wt_init_usb_psy(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = wt_init_dev_class(batt_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	INIT_DELAYED_WORK(&batt_chg->batt_chg_work, batt_chg_main);
	INIT_DELAYED_WORK(&batt_chg->usb_therm_work, usb_therm);
	INIT_DELAYED_WORK(&batt_chg->usb_type_work, batt_usb_type);

	batt_chg->wt_ws = wakeup_source_register(batt_chg->dev, "charge_wakeup");
	if (!batt_chg->wt_ws) {
		pr_err("wt chg workup fail!\n");
		wakeup_source_unregister(batt_chg->wt_ws);
	}

	schedule_delayed_work(&batt_chg->batt_chg_work, 0);
	INIT_DELAYED_WORK( &batt_chg->xm_prop_change_work, generate_xm_charge_uvent);
	INIT_DELAYED_WORK( &batt_chg->charger_debug_info_print_work, xm_charger_debug_info_print_work);
	schedule_delayed_work(&batt_chg->charger_debug_info_print_work, 30 * HZ);
	schedule_delayed_work(&batt_chg->xm_prop_change_work, msecs_to_jiffies(30000));
	schedule_delayed_work(&batt_chg->usb_therm_work, msecs_to_jiffies(10000));

	pr_err("batt_chg probe success\n");
	return 0;

cleanup:
	pr_err("batt_chg probe fail\n");
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

MODULE_AUTHOR("WingTech Inc.");
MODULE_DESCRIPTION("battery driver");
MODULE_LICENSE("GPL");
