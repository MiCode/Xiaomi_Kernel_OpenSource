#ifndef HQ_CHARGER_MANAGER_H
#define HQ_CHARGER_MANAGER_H

#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/iio/iio.h>

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

#define PROBE_CNT_MAX			10

#define CM_HIGH_TEMP_CHARGE_SW_VREG 4100000
#define CM_FFC_SW_VREG_HIGH 4608000
#define CM_FFC_SW_VREG_LOW 4480000

#define CHARGING_PERIOD_S		30
#define DISCHARGE_PERIOD_S		300

#define SHUTDOWN_DELAY_VOL_LOW	3300
#define SHUTDOWN_DELAY_VOL_HIGH	3400

#define CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD	2900000
#define CM_UVLO_CALIBRATION_CNT_THRESHOLD	5

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
	int jeita_temp_step7;
};

struct batt_chg {
	struct platform_device *pdev;
	struct device *dev;

	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct power_supply *sw_psy;
	struct power_supply *cp_psy;
	struct power_supply *fg_psy;
	struct power_supply *verify_psy;

	struct iio_dev          *indio_dev;
	struct iio_chan_spec    *iio_chan;
	struct iio_channel	*int_iio_chans;

	struct class battery_class;
	struct device batt_device;

	struct wakeup_source *wt_ws;
	struct batt_dt_props dt;
	struct delayed_work batt_chg_work;
//	struct delayed_work lower_poweroff_work;
//	struct delayed_work usb_type_work;

//	struct delayed_work	charger_debug_info_print_work;

	struct mutex charger_type_mtx;

	int pd_active;
	int old_pd_active;
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
	int is_stop_charge;
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
	int switch_chg_ic;

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
	int old_real_type;
	int usb_float;
	int power_supply_count;
	bool isln8000flg;
	bool is25thermmitigflg;

	int jeita_cur;
	int charge_limit_cur;
	int input_limit_cur;
	int therm_cur;
	bool shutdown_delay;
	bool last_shutdown_delay;
	int sw_chg_chip_id;
};

struct batt_chg *g_batt_chg;

static int thermal_mitigation[] = {
	6000000,5400000,5000000,4500000,4000000,3500000,3000000,2700000,
	2500000,2300000,2100000,1800000,1500000,900000,800000,500000,300000,
};


struct charger_type {
	int type;
	enum power_supply_type adap_type;
};

static struct charger_type charger_type[16] = {
	{POWER_SUPPLY_USB_TYPE_SDP, POWER_SUPPLY_TYPE_USB},
	{POWER_SUPPLY_USB_TYPE_DCP, POWER_SUPPLY_TYPE_USB_DCP},
	{POWER_SUPPLY_USB_TYPE_CDP, POWER_SUPPLY_TYPE_USB_CDP},
	{POWER_SUPPLY_USB_TYPE_ACA, POWER_SUPPLY_TYPE_USB_ACA},
	{POWER_SUPPLY_USB_TYPE_C, POWER_SUPPLY_TYPE_USB_FLOAT},
	{POWER_SUPPLY_USB_TYPE_PD, POWER_SUPPLY_TYPE_USB_PD},
	{POWER_SUPPLY_USB_TYPE_PD_DRP, POWER_SUPPLY_TYPE_USB_PD_DRP},
	{POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID, POWER_SUPPLY_TYPE_APPLE_BRICK_ID},
	{0, 0},
};

enum battery_id{
	FIRST_SUPPLIER,
	SECOND_SUPPLIER,
	THIRD_SUPPLIER,
	UNKNOW_SUPPLIER,
};

#endif
