// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 */

#define pr_fmt(fmt)	"BATTERY_CHG: %s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/thermal.h>
#include <linux/rpmsg.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>
#include <linux/reboot.h>
#include <linux/soc/qcom/pmic_glink.h>
#include <linux/soc/qcom/battery_charger.h>
#include <linux/soc/qcom/panel_event_notifier.h>
#include <linux/kernel.h>
#include <linux/notifier.h>

/* pen_connect_strategy start */
ATOMIC_NOTIFIER_HEAD(pen_charge_state_notifier);
/* pen_connect_strategy end */

#if defined (CONFIG_QTI_POGO_CHG)
#include <linux/battmngr/qti_use_pogo.h>
struct battery_chg_dev *g_bcdev = NULL;
EXPORT_SYMBOL(g_bcdev);

static int pogo_flag;
#define BC_NOTIFY_IRQ			0x52
#define POGO_TERM_FCC			200
#endif

#define MSG_OWNER_BC			32778
#define MSG_TYPE_REQ_RESP		1
#define MSG_TYPE_NOTIFY			2

/* opcode for battery charger */
#define BC_SET_NOTIFY_REQ		0x04
#define BC_DISABLE_NOTIFY_REQ		0x05
#define BC_NOTIFY_IND			0x07
#define BC_BATTERY_STATUS_GET		0x30
#define BC_BATTERY_STATUS_SET		0x31
#define BC_USB_STATUS_GET		0x32
#define BC_USB_STATUS_SET		0x33
#define BC_WLS_STATUS_GET		0x34
#define BC_WLS_STATUS_SET		0x35
#define BC_XM_STATUS_GET		0x50
#define BC_XM_STATUS_SET		0x51
#define BC_SHIP_MODE_REQ_SET		0x36
#define BC_SHUTDOWN_REQ_SET		0x37
#define BC_WLS_FW_CHECK_UPDATE		0x40
#define BC_WLS_FW_PUSH_BUF_REQ		0x41
#define BC_WLS_FW_UPDATE_STATUS_RESP	0x42
#define BC_WLS_FW_PUSH_BUF_RESP		0x43
#define BC_WLS_FW_GET_VERSION		0x44
#define BC_SHUTDOWN_NOTIFY		0x47
#define BC_HBOOST_VMAX_CLAMP_NOTIFY	0x79
#define BC_GENERIC_NOTIFY		0x80

/* Generic definitions */
#define MAX_STR_LEN			128
#define CHG_DEBUG_DATA_LEN	200
// wireless_chip_fw: xx.xx.xx.xx
#define WIRELESS_CHIP_FW_VERSION_LEN	16
#define BC_WAIT_TIME_MS			1000
#define WLS_FW_PREPARE_TIME_MS		1000
#define WLS_FW_WAIT_TIME_MS		500
#define WLS_FW_UPDATE_TIME_MS		1000
#define WLS_FW_BUF_SIZE			128
#define DEFAULT_RESTRICT_FCC_UA		1000000
#define BATTERY_DIGEST_LEN 32
#define BATTERY_SS_AUTH_DATA_LEN 4

#define WIRELESS_UUID_LEN	20

#define BATTERY_DIGEST_LEN 32
#define BATTERY_SS_AUTH_DATA_LEN 4
#define USBPD_UVDM_SS_LEN		4
#define USBPD_UVDM_VERIFIED_LEN		1

#define MAX_THERMAL_LEVEL		16

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

enum ship_mode_type {
	SHIP_MODE_PMIC,
	SHIP_MODE_PACK_SIDE,
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
	USB_PROP_MAX,
};

enum wireless_property_id {
	WLS_ONLINE,
	WLS_VOLT_NOW,
	WLS_VOLT_MAX,
	WLS_CURR_NOW,
	WLS_CURR_MAX,
	WLS_TYPE,
	WLS_BOOST_EN,
	WLS_HBOOST_VMAX,
	WLS_INPUT_CURR_LIMIT,
	WLS_ADAP_TYPE,
	WLS_CONN_TEMP,
	WLS_PROP_MAX,
};

enum {
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP = 0x80,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5,
	QTI_POWER_SUPPLY_USB_TYPE_USB_FLOAT,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3_CLASSB,
};

enum xm_property_id {
	XM_PROP_RESISTANCE_ID,
	XM_PROP_VERIFY_DIGEST,
	XM_PROP_CONNECTOR_TEMP,
	XM_PROP_AUTHENTIC,
	XM_PROP_BATTERY_ADAPT_POWER_MATCH,
	XM_PROP_CHIP_OK,
	XM_PROP_VBUS_DISABLE,
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
	/*charger_pump sys node*/
	XM_PROP_BQ2597X_CHIP_OK,
	XM_PROP_BQ2597X_SLAVE_CHIP_OK,
	XM_PROP_BQ2597X_BUS_CURRENT,
	XM_PROP_BQ2597X_SLAVE_BUS_CURRENT,
	XM_PROP_BQ2597X_BUS_DELTA,
	XM_PROP_BQ2597X_BUS_VOLTAGE,
	XM_PROP_BQ2597X_BATTERY_PRESENT,
	XM_PROP_BQ2597X_SLAVE_BATTERY_PRESENT,
	XM_PROP_BQ2597X_BATTERY_VOLTAGE,
	XM_PROP_MASTER_SMB1396_ONLINE,
	XM_PROP_MASTER_SMB1396_IIN,
	XM_PROP_SLAVE_SMB1396_ONLINE,
	XM_PROP_SLAVE_SMB1396_IIN,
	XM_PROP_SMB_IIN_DIFF,
	XM_PROP_CC_ORIENTATION,
	XM_PROP_INPUT_SUSPEND,
	XM_PROP_FASTCHGMODE,
	XM_PROP_NIGHT_CHARGING,
	XM_PROP_SOC_DECIMAL,
	XM_PROP_SOC_DECIMAL_RATE,
	XM_PROP_QUICK_CHARGE_TYPE,
	XM_PROP_APDO_MAX,
	XM_PROP_POWER_MAX,
	XM_PROP_DIE_TEMPERATURE,
	XM_PROP_SLAVE_DIE_TEMPERATURE,
	XM_PROP_FG_RAW_SOC,
	/* wireless charge infor */
	XM_PROP_WLS_START = 50,
	XM_PROP_TX_MACL,
	XM_PROP_TX_MACH,
	XM_PROP_RX_CRL,
	XM_PROP_RX_CRH,
	XM_PROP_RX_CEP,
	XM_PROP_BT_STATE,
	XM_PROP_REVERSE_CHG_MODE,
	XM_PROP_REVERSE_CHG_STATE,
	XM_PROP_RX_VOUT,
	XM_PROP_RX_VRECT,
	XM_PROP_RX_IOUT,
	XM_PROP_TX_ADAPTER,
	XM_PROP_OP_MODE,
	XM_PROP_WLS_DIE_TEMP,
	XM_PROP_WLS_BIN,
	XM_PROP_WLSCHARGE_CONTROL_LIMIT,
	XM_PROP_FW_VER,
	XM_PROP_TX_UUID,
	XM_PROP_WLS_THERMAL_REMOVE,
	XM_PROP_CHG_DEBUG,
	XM_PROP_WLS_FW_STATE,
	XM_PROP_WLS_CAR_ADAPTER,
	XM_PROP_WLS_TX_SPEED,
	XM_PROP_WLS_FC_FLAG,
	XM_PROP_RX_SS,
	XM_PROP_RX_OFFSET,
	XM_PROP_TX_Q,
	XM_PROP_PEN_MACL,
	XM_PROP_PEN_MACH,
	XM_PROP_TX_IOUT,
	XM_PROP_TX_VOUT,
	XM_PROP_PEN_SOC,
	XM_PROP_PEN_HALL3,
	XM_PROP_PEN_HALL4,
	XM_PROP_PEN_TX_SS,
	XM_PROP_PEN_PLACE_ERR,
	XM_PROP_FAKE_SS,
	XM_PROP_WLS_END = 90,
	/**********************/
	XM_PROP_SHUTDOWN_DELAY,
	XM_PROP_FAKE_TEMP,
	XM_PROP_THERMAL_REMOVE,
	XM_PROP_TYPEC_MODE,
	XM_PROP_MTBF_CURRENT,
	XM_PROP_THERMAL_TEMP,
	XM_PROP_FB_BLANK_STATE,
	XM_PROP_SMART_BATT,
	XM_PROP_SHIPMODE_COUNT_RESET,
	XM_PROP_SPORT_MODE,
	XM_PROP_BATT_CONNT_ONLINE,
	XM_PROP_FAKE_CYCLE,
	XM_PROP_AFP_TEMP,
	XM_PROP_PLATE_SHOCK,
	XM_PROP_CC_SHORT_VBUS,
	XM_PROP_OTG_UI_SUPPORT,
	XM_PROP_CID_STATUS,
	XM_PROP_CC_TOGGLE,
	XM_PROP_SMART_CHG,
	/*********nvt fuelgauge feature*********/
	XM_PROP_NVTFG_MONITOR_ISC,
	XM_PROP_NVTFG_MONITOR_SOA,
	XM_PROP_OVER_PEAK_FLAG,
	XM_PROP_CURRENT_DEVIATION,
	XM_PROP_POWER_DEVIATION,
	XM_PROP_AVERAGE_CURRENT,
	XM_PROP_AVERAGE_TEMPERATURE,
	XM_PROP_START_LEARNING,
	XM_PROP_STOP_LEARNING,
	XM_PROP_SET_LEARNING_POWER,
	XM_PROP_GET_LEARNING_POWER,
	XM_PROP_GET_LEARNING_POWER_DEV,
	XM_PROP_GET_LEARNING_TIME_DEV,
	XM_PROP_SET_CONSTANT_POWER,
	XM_PROP_GET_REMAINING_TIME,
	XM_PROP_SET_REFERANCE_POWER,
	XM_PROP_GET_REFERANCE_CURRENT,
	XM_PROP_GET_REFERANCE_POWER,
	XM_PROP_START_LEARNING_B,
	XM_PROP_STOP_LEARNING_B,
	XM_PROP_SET_LEARNING_POWER_B,
	XM_PROP_GET_LEARNING_POWER_B,
	XM_PROP_GET_LEARNING_POWER_DEV_B,
	/*********nvt fuelgauge feature*********/
	/*fuelgauge test node*/
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
	XM_PROP_FG1_RSOC,
	XM_PROP_FG1_AI,
	XM_PROP_FG1_CELL1_VOL,
	XM_PROP_FG1_CELL2_VOL,
	/*begin dual fuel high temperature intercept feature */
	XM_PROP_FG1_TEMP_MAX,
	XM_PROP_FG1_TIME_HT,
	XM_PROP_FG1_TIME_OT,
	XM_PROP_FG1_SEAL_SET,
	XM_PROP_FG1_SEAL_STATE,
	XM_PROP_FG1_DF_CHECK,
	/*end dual fuel high temperature intercept feature*/
	/*dual fuel gauge - second node - M18*/
	XM_PROP_SLAVE_CHIP_OK,
	XM_PROP_SLAVE_AUTHENTIC,
	XM_PROP_FG2_RM,
	XM_PROP_FG2_FCC,
	XM_PROP_FG2_SOH,
	XM_PROP_FG2_CURRENT_MAX,
	XM_PROP_FG2_VOL_MAX,
	XM_PROP_FG2_RSOC,
	XM_PROP_FG1_IBATT,
	XM_PROP_FG2_IBATT,
	XM_PROP_FG1_TEMP,
	XM_PROP_FG2_TEMP,
	XM_PROP_FG1_VOL,
	XM_PROP_FG2_VOL,
	XM_PROP_FG2_AI,
	XM_PROP_FG2_TREMQ,
	XM_PROP_FG2_TFULLCHGQ,
	XM_PROP_FG1_FullChargeFlag,
	XM_PROP_FG2_FullChargeFlag,
	XM_PROP_FG1_SOC,
	XM_PROP_FG2_SOC,
	XM_PROP_FG1_GET_RECORD_DELTA_R1,
	XM_PROP_FG1_GET_RECORD_DELTA_R2,
	XM_PROP_FG1_GET_R1_DISCHARGE_FLAG,
	XM_PROP_FG1_GET_R2_DISCHARGE_FLAG,
	XM_PROP_FG2_GET_RECORD_DELTA_R1,
	XM_PROP_FG2_GET_RECORD_DELTA_R2,
	XM_PROP_FG2_GET_R1_DISCHARGE_FLAG,
	XM_PROP_FG2_GET_R2_DISCHARGE_FLAG,
	/*dual fuel gauge - second node - M18*/
	/*dtpt fuelgauge feature-second fuelgauge node-M18*/
	XM_PROP_FG2_OVER_PEAK_FLAG,
	XM_PROP_FG2_CURRENT_DEVIATION,
	XM_PROP_FG2_POWER_DEVIATION,
	XM_PROP_FG2_AVERAGE_CURRENT,
	XM_PROP_FG2_AVERAGE_TEMPERATURE,
	XM_PROP_FG2_START_LEARNING,
	XM_PROP_FG2_STOP_LEARNING,
	XM_PROP_FG2_SET_LEARNING_POWER,
	XM_PROP_FG2_GET_LEARNING_POWER,
	XM_PROP_FG2_GET_LEARNING_POWER_DEV,
	XM_PROP_FG2_GET_LEARNING_TIME_DEV,
	XM_PROP_FG2_SET_CONSTANT_POWER,
	XM_PROP_FG2_GET_REMAINING_TIME,
	XM_PROP_FG2_SET_REFERANCE_POWER,
	XM_PROP_FG2_GET_REFERANCE_CURRENT,
	XM_PROP_FG2_GET_REFERANCE_POWER,
	XM_PROP_FG2_START_LEARNING_B,
	XM_PROP_FG2_STOP_LEARNING_B,
	XM_PROP_FG2_SET_LEARNING_POWER_B,
	XM_PROP_FG2_GET_LEARNING_POWER_B,
	XM_PROP_FG2_GET_LEARNING_POWER_DEV_B,
	XM_PROP_FG1_GET_DESIGN_CAPACITY,
	XM_PROP_FG2_GET_DESIGN_CAPACITY,
	/*dtpt fuelgauge feature-second fuelgauge node-M18*/
	XM_PROP_FG_VENDOR_ID,
	XM_PROP_HAS_DP,
	XM_PROP_DAM_OVPGATE,
	XM_PROP_CHARGING_SUSPEND_BATTERY,
#ifdef CONFIG_QTI_POGO_CHG
	/*set sc8561 mode and mos*/
	XM_PROP_SC8561_MODE,
	XM_PROP_SC8561_OVP_MOS,
	XM_PROP_SC8561_WPC_MOS,
	XM_PROP_DCIN_STATE,
	XM_PROP_ADC_CHGR_STATUS,
	XM_PROP_ENABLE_CHARGING,
	XM_PROP_BATTERY_TEMP,
	XM_PROP_TERMINATION_CUR,
	XM_PROP_KEYBOARD_PLUGIN,
	XM_PROP_ALL_VOTER,
#endif
	XM_PROP_LAST_NODE,
	XM_PROP_MAX,
};
enum fg_venodr{
	POWER_SUPPLY_VENDOR_BYD = 0,
	POWER_SUPPLY_VENDOR_COSLIGHT,
	POWER_SUPPLY_VENDOR_SUNWODA,
	POWER_SUPPLY_VENDOR_NVT,
	POWER_SUPPLY_VENDOR_SCUD,
	POWER_SUPPLY_VENDOR_TWS,
	POWER_SUPPLY_VENDOR_LISHEN,
	POWER_SUPPLY_VENDOR_DESAY,
};
struct battery_charger_set_notify_msg {
	struct pmic_glink_hdr	hdr;
	u32			battery_id;
	u32			power_state;
	u32			low_capacity;
	u32			high_capacity;
};

struct battery_charger_notify_msg {
	struct pmic_glink_hdr	hdr;
	u32			notification;
};

struct battery_charger_req_msg {
	struct pmic_glink_hdr	hdr;
	u32			battery_id;
	u32			property_id;
	u32			value;
};

struct battery_charger_resp_msg {
	struct pmic_glink_hdr	hdr;
	u32			property_id;
	u32			value;
	u32			ret_code;
};

struct battery_model_resp_msg {
	struct pmic_glink_hdr	hdr;
	u32			property_id;
	char			model[MAX_STR_LEN];
};

struct xm_set_wls_bin_req_msg {
  struct pmic_glink_hdr hdr;
  u32 property_id;
  u16 total_length;
  u8 serial_number;
  u8 fw_area;
  u8 wls_fw_bin[MAX_STR_LEN];
};  /* Message */

struct wireless_fw_check_req {
	struct pmic_glink_hdr	hdr;
	u32			fw_version;
	u32			fw_size;
	u32			fw_crc;
};

struct wireless_fw_check_resp {
	struct pmic_glink_hdr	hdr;
	u32			ret_code;
};

struct wireless_fw_push_buf_req {
	struct pmic_glink_hdr	hdr;
	u8			buf[WLS_FW_BUF_SIZE];
	u32			fw_chunk_id;
};

struct wireless_fw_push_buf_resp {
	struct pmic_glink_hdr	hdr;
	u32			fw_update_status;
};

struct wireless_fw_update_status {
	struct pmic_glink_hdr	hdr;
	u32			fw_update_done;
};

struct wireless_fw_get_version_req {
	struct pmic_glink_hdr	hdr;
};

struct wireless_fw_get_version_resp {
	struct pmic_glink_hdr	hdr;
	u32			fw_version;
};

struct battery_charger_ship_mode_req_msg {
	struct pmic_glink_hdr	hdr;
	u32			ship_mode_type;
};

struct battery_charger_shutdown_req_msg {
	struct pmic_glink_hdr	hdr;
};
struct xm_verify_digest_resp_msg {
	struct pmic_glink_hdr	hdr;
	u32			property_id;
	u8			digest[BATTERY_DIGEST_LEN];
	/*dual battery master and slave flag*/
	bool		slave_fg;
};

struct xm_ss_auth_resp_msg {
	struct pmic_glink_hdr	hdr;
	u32			property_id;
	u32			data[BATTERY_SS_AUTH_DATA_LEN];
};

#if defined (CONFIG_QTI_POGO_CHG)
struct xm_voter_resp_msg {
	struct pmic_glink_hdr	hdr;
	u32			property_id;
	u32			value;
	u8			vote_type;
	u32			return_code;
};
#endif

struct wireless_chip_fw_msg {
	struct pmic_glink_hdr	hdr;
	u32				property_id;
	u32				value;
	char			version[WIRELESS_CHIP_FW_VERSION_LEN];
};

struct wireless_tx_uuid_msg {
	struct pmic_glink_hdr	hdr;
	u32				property_id;
	u32				value;
	char			version[WIRELESS_UUID_LEN];
};

enum xm_chg_debug_type {
	CHG_WLS_DEBUG,
	CHG_ADSP_LOG,
	CHG_DEBUG_TYPE_MAX,
};

struct chg_debug_msg {
	struct pmic_glink_hdr   hdr;
	u32                     property_id;
	u8                      type;
	char                    data[CHG_DEBUG_DATA_LEN];
};

#ifndef CONFIG_QTI_POGO_CHG
enum psy_type {
	PSY_TYPE_BATTERY,
	PSY_TYPE_USB,
	PSY_TYPE_WLS,
	PSY_TYPE_XM,
	PSY_TYPE_MAX,
};

struct psy_state {
	struct power_supply	*psy;
	char			*model;
	const int		*map;
	u32			*prop;
	u32			prop_count;
	u32			opcode_get;
	u32			opcode_set;
};
#endif

struct battery_chg_dev {
	struct device			*dev;
	struct class			battery_class;
	struct pmic_glink_client	*client;
	struct mutex			rw_lock;
	struct rw_semaphore		state_sem;
	struct completion		ack;
	struct completion		fw_buf_ack;
	struct completion		fw_update_ack;
	struct psy_state		psy_list[PSY_TYPE_MAX];
	struct dentry			*debugfs_dir;
	void				*notifier_cookie;
	u32				*thermal_levels;
	const char			*wls_fw_name;
	int				curr_thermal_level;
	int				curr_wlsthermal_level;
	int				num_thermal_levels;
	int				shutdown_volt_mv;
	atomic_t			state;
	struct work_struct		subsys_up_work;
	struct work_struct		usb_type_work;
	struct work_struct		battery_check_work;
	struct delayed_work		charger_debug_info_print_work;
	struct delayed_work		batt_update_work;
	struct delayed_work		xm_prop_change_work;
	bool				debug_work_en;
	int				fake_soc;
	bool				block_tx;
	bool				ship_mode_en;
	bool				debug_battery_detected;
	bool				wls_fw_update_reqd;
	u32				wls_fw_version;
	u16				wls_fw_crc;
	u32				wls_fw_update_time_ms;
	char			wireless_chip_fw_version[WIRELESS_CHIP_FW_VERSION_LEN];
	char			wireless_tx_uuid_version[WIRELESS_UUID_LEN];
	struct notifier_block		reboot_notifier;
	struct notifier_block		shutdown_notifier;
	u32				thermal_fcc_ua;
	u32				restrict_fcc_ua;
	u32				last_fcc_ua;
	u32				usb_icl_ua;
	u32				reverse_chg_flag;
	u32				boost_mode;
	u32				thermal_fcc_step;
	bool				restrict_chg_en;
	/* To track the driver initialization status */
	bool				initialized;
	u8				*digest;
	u32				*ss_auth_data;
	char				wls_debug_data[CHG_DEBUG_DATA_LEN];
	bool				notify_en;
	/*battery auth check for ssr*/
	bool				battery_auth;
	/*dual battery authentic flag*/
	bool				slave_battery_auth;
	bool				slave_fg_verify_flag;
	int				mtbf_current;
	int				blank_state;
	/*shutdown delay is supported, dtsi config*/
	bool			shutdown_delay_en;
	bool			report_power_absent;
	/*extreme_mode is supported, dtsi config*/
	bool			extreme_mode_en;

#ifdef CONFIG_QTI_POGO_CHG
	struct battmngr_device* battmg_dev;
	struct votable *fcc_votable;
	struct votable *usb_icl_votable;
	struct votable *fv_votable;
	u8 car_app_value;
	bool full_flag;
	bool input_suspend;
#endif

	/* pen_connect_strategy start */
	struct work_struct pen_notifier_work;
	/* pen_connect_strategy end */
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
};

static const int wls_prop_map[WLS_PROP_MAX] = {
	[WLS_ONLINE]		= POWER_SUPPLY_PROP_ONLINE,
	[WLS_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[WLS_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[WLS_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[WLS_CURR_MAX]		= POWER_SUPPLY_PROP_CURRENT_MAX,
	[WLS_INPUT_CURR_LIMIT]	= POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	[WLS_CONN_TEMP]		= POWER_SUPPLY_PROP_TEMP,
	[WLS_BOOST_EN]		= POWER_SUPPLY_PROP_PRESENT,
};

static const char * const POWER_SUPPLY_VENDOR_TEXT[] = {
	[POWER_SUPPLY_VENDOR_BYD]		= "BYD",
	[POWER_SUPPLY_VENDOR_COSLIGHT]	= "COSLIGHT",
	[POWER_SUPPLY_VENDOR_SUNWODA]	= "SUNWODA",
	[POWER_SUPPLY_VENDOR_NVT]		= "NVT",
	[POWER_SUPPLY_VENDOR_SCUD]		= "SCUD",
	[POWER_SUPPLY_VENDOR_TWS]		= "TWS",
	[POWER_SUPPLY_VENDOR_DESAY]		= "DESAY",
};

static const int xm_prop_map[XM_PROP_MAX] = {};

/* Standard usb_type definitions similar to power_supply_sysfs.c */
static const char * const power_supply_usb_type_text[] = {
	"Unknown", "SDP", "DCP", "CDP", "ACA", "C",
	"PD", "PD_DRP", "PD_PPS", "BrickID"
};

/* Custom usb_type definitions */
static const char * const qc_power_supply_usb_type_text[] = {
	"HVDCP", "HVDCP_3", "HVDCP_3P5", "USB_FLOAT","HVDCP_3"
};

/* wireless_type definitions */
static const char * const qc_power_supply_wls_type_text[] = {
	"Unknown", "BPP", "EPP", "HPP"
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

static RAW_NOTIFIER_HEAD(hboost_notifier);

int register_hboost_event_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&hboost_notifier, nb);
}
EXPORT_SYMBOL(register_hboost_event_notifier);

int unregister_hboost_event_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&hboost_notifier, nb);
}
EXPORT_SYMBOL(unregister_hboost_event_notifier);

int StringToHex(char *str, unsigned char *out, unsigned int *outlen)
{
	char *p = str;
	char high = 0, low = 0;
	int tmplen = strlen(p), cnt = 0;
	tmplen = strlen(p);
	while(cnt < (tmplen / 2))
	{
		high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		low = (*(++ p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
		out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
		p ++;
		cnt ++;
	}
	if(tmplen % 2 != 0) out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;

	if(outlen != NULL) *outlen = tmplen / 2 + tmplen % 2;

	return tmplen / 2 + tmplen % 2;
}

static int battery_chg_fw_write(struct battery_chg_dev *bcdev, void *data,
				int len)
{
	int rc;

	down_read(&bcdev->state_sem);
	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		up_read(&bcdev->state_sem);
		return -ENOTCONN;
	}

	reinit_completion(&bcdev->fw_buf_ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->fw_buf_ack,
					msecs_to_jiffies(WLS_FW_WAIT_TIME_MS));
		if (!rc) {
			up_read(&bcdev->state_sem);
			return -ETIMEDOUT;
		}

		rc = 0;
	}

	up_read(&bcdev->state_sem);
	return rc;
}

static int battery_chg_write(struct battery_chg_dev *bcdev, void *data,
				int len)
{
	int rc;

	/*
	 * When the subsystem goes down, it's better to return the last
	 * known values until it comes back up. Hence, return 0 so that
	 * pmic_glink_write() is not attempted until pmic glink is up.
	 */
	down_read(&bcdev->state_sem);
	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		up_read(&bcdev->state_sem);
		return 0;
	}

	if (bcdev->debug_battery_detected && bcdev->block_tx) {
		up_read(&bcdev->state_sem);
		return 0;
	}

	mutex_lock(&bcdev->rw_lock);
	reinit_completion(&bcdev->ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->ack,
					msecs_to_jiffies(BC_WAIT_TIME_MS));
		if (!rc) {
			up_read(&bcdev->state_sem);
			mutex_unlock(&bcdev->rw_lock);
			return -ETIMEDOUT;
		}

		rc = 0;
	}
	mutex_unlock(&bcdev->rw_lock);
	up_read(&bcdev->state_sem);

	return rc;
}

static int write_property_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id, u32 val)
{
	struct battery_charger_req_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.battery_id = 0;
	req_msg.value = val;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static int read_property_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id)
{
	struct battery_charger_req_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.battery_id = 0;
	req_msg.value = 0;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_get;

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static int get_property_id(struct psy_state *pst,
			enum power_supply_property prop)
{
	u32 i;

	for (i = 0; i < pst->prop_count; i++)
		if (pst->map[i] == prop)
			return i;

	return -ENOENT;
}

#ifdef CONFIG_QTI_POGO_CHG
bool check_g_bcdev_ops(void)
{
	if (!g_bcdev) {
		return false;
	}
	return true;
}
EXPORT_SYMBOL(check_g_bcdev_ops);

static int get_fg_soc(int *soc)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_BATTERY];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, BATT_CAPACITY);
	if (rc < 0)
		return rc;
	*soc = pst->prop[BATT_CAPACITY] / 100;

	if (g_bcdev->fake_soc >= 0 && g_bcdev->fake_soc <= 100) {
		*soc = g_bcdev->fake_soc;
	}

	return rc;
}

static int get_fg_curr(int *curr)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_BATTERY];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, BATT_CURR_NOW);
	if (rc < 0)
		return rc;
	*curr = (int)(pst->prop[BATT_CURR_NOW]) / 1000;

	return rc;
}

static int get_fg_volt(int *volt)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_BATTERY];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, BATT_VOLT_NOW);
	if (rc < 0)
		return rc;
	*volt = pst->prop[BATT_VOLT_NOW] / 1000;

	return rc;
}

static int get_fg_temp(int *temp)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_BATTERY_TEMP);
	if (rc < 0)
		return rc;
	*temp = pst->prop[XM_PROP_BATTERY_TEMP];

	return rc;
}

static int get_fg1_curr(int *fg1_curr)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG1_IBATT);
	if (rc < 0)
		return rc;
	*fg1_curr = pst->prop[XM_PROP_FG1_IBATT];

	return rc;
}

static int get_fg2_curr(int *fg2_curr)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG2_IBATT);
	if (rc < 0)
		return rc;
	*fg2_curr = pst->prop[XM_PROP_FG2_IBATT];

	return rc;
}

static int get_fg1_volt(int *fg1_volt)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG1_VOL);
	if (rc < 0)
		return rc;
	*fg1_volt = pst->prop[XM_PROP_FG1_VOL];

	return rc;
}

static int get_fg2_volt(int *fg2_volt)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG2_VOL);
	if (rc < 0)
		return rc;
	*fg2_volt = pst->prop[XM_PROP_FG2_VOL];

	return rc;
}

static int get_fg1_fcc(int *fg1_fcc)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG1_FCC);
	if (rc < 0)
		return rc;
	*fg1_fcc = pst->prop[XM_PROP_FG1_FCC];

	return rc;
}

static int get_fg2_fcc(int *fg2_fcc)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG2_FCC);
	if (rc < 0)
		return rc;
	*fg2_fcc = pst->prop[XM_PROP_FG2_FCC];

	return rc;
}

static int get_fg1_rm(int *fg1_rm)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG1_RM);
	if (rc < 0)
		return rc;

	*fg1_rm = pst->prop[XM_PROP_FG1_RM];

	return rc;
}

static int get_fg2_rm(int *fg2_rm)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG2_RM);
	if (rc < 0)
		return rc;

	*fg2_rm = pst->prop[XM_PROP_FG2_RM];

	return rc;
}

static int get_fg1_raw_soc(int *fg1_raw_soc)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;
	int rm = 0, fcc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG1_RM);
	if (rc < 0)
		return rc;
	rc = read_property_id(g_bcdev, pst, XM_PROP_FG1_FCC);
	if (rc < 0)
		return rc;

	rm = pst->prop[XM_PROP_FG1_RM];
	fcc = pst->prop[XM_PROP_FG1_FCC];

	fcc /= 1000;
	*fg1_raw_soc = (rm *10) / fcc;

	return 0;
}

static int get_fg2_raw_soc(int* fg2_raw_soc)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;
	int rm = 0, fcc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG2_RM);
	if (rc < 0)
		return rc;
	rc = read_property_id(g_bcdev, pst, XM_PROP_FG2_FCC);
	if (rc < 0)
		return rc;

	rm = pst->prop[XM_PROP_FG2_RM];
	fcc = pst->prop[XM_PROP_FG2_FCC];

	fcc /= 1000;
	*fg2_raw_soc = (rm *10) / fcc;

	return 0;
}

static int get_fg1_soc(int *fg1_soc)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG1_SOC);
	if (rc < 0)
		return rc;

	*fg1_soc = pst->prop[XM_PROP_FG1_SOC];

	return rc;
}

static int get_fg2_soc(int* fg2_soc)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG2_SOC);
	if (rc < 0)
		return rc;

	*fg2_soc = pst->prop[XM_PROP_FG2_SOC];

	return rc;
}

static int set_fg1_fastCharge(int fg1_ffc)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = write_property_id(g_bcdev, pst, XM_PROP_FG1_FAST_CHARGE, fg1_ffc);
	if (rc < 0)
		return rc;

	return rc;
}

static int get_fg1_fastCharge(int *fastcharge_mode)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FASTCHGMODE);
	if (rc < 0)
		return rc;

	*fastcharge_mode = pst->prop[XM_PROP_FASTCHGMODE];

	return rc;}

static int set_fg2_fastCharge(int fg2_ffc)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = write_property_id(g_bcdev, pst, XM_PROP_FG1_FAST_CHARGE, fg2_ffc);
	if (rc < 0)
		return rc;

	return rc;
}

static int set_batt_suspend(bool suspend)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = write_property_id(g_bcdev, pst, XM_PROP_INPUT_SUSPEND, suspend);
	if (rc < 0)
		return rc;

	return rc;
}

static int get_batt_authentication(int *authentic)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_AUTHENTIC);
	if (rc < 0)
		return rc;

	*authentic = pst->prop[XM_PROP_AUTHENTIC];

	return rc;
}

static int get_chip_ok(int* chip_ok)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_CHIP_OK);
	if (rc < 0)
		return rc;

	*chip_ok = pst->prop[XM_PROP_CHIP_OK];

	return rc;
}

static int set_termination_current(int iterm)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = write_property_id(g_bcdev, pst, XM_PROP_TERMINATION_CUR, iterm);
	if (rc < 0)
		return rc;

	return rc;
}

static int get_fg1_temp(int *fg1_temp)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG1_TEMP);
	if (rc < 0)
		return rc;

	*fg1_temp = pst->prop[XM_PROP_FG1_TEMP];

	return rc;
}

static int get_fg2_temp(int *fg2_temp)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_FG2_TEMP);
	if (rc < 0)
		return rc;

	*fg2_temp = pst->prop[XM_PROP_FG2_TEMP];

	return rc;
}

static int get_charge_status(int *chg_status)
{
	int status = 0;
	int curr = 0, batt_temp = 0, volt_now = 0;
	static int last_status = 0;

	status = qti_get_ADC_CHGR_STATUS();
	switch (status)
	{
	case CHGR_BATT_CHGR_STATUS_TRICKLE:
	case CHGR_BATT_CHGR_STATUS_PRECHARGE:
	case CHGR_BATT_CHGR_STATUS_FULLON:
	case CHGR_BATT_CHGR_STATUS_TAPER:
	case CHGR_BATT_CHGR_STATUS_TERMINATION:
		*chg_status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case CHGR_BATT_CHGR_STATUS_INHIBIT:
		*chg_status = POWER_SUPPLY_STATUS_FULL;
		break;
	case CHGR_BATT_CHGR_STATUS_PAUSE:
	case CHGR_BATT_CHG_STATUS_CHARGING_DISABLED:
		*chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	default:
		*chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	if (((last_status == CHGR_BATT_CHGR_STATUS_TAPER) || (last_status == CHGR_BATT_CHGR_STATUS_TERMINATION))
		   && (status == CHGR_BATT_CHG_STATUS_CHARGING_DISABLED)) {
		get_fg_temp(&batt_temp);
		get_fg_volt(&volt_now);
		if (((batt_temp / 10) >= BATT_WARM_THRESHOLD) && ((volt_now * 1000) >= (BATT_WARM_VBAT_THRESHOLD - BATT_WARM_HYS_THRESHOLD)))
			pr_info("temp is :%d, batt_volt is :%d. stop charing", batt_temp / 10, volt_now);
		else {
			g_bcdev->full_flag = true;
		}
	}

	if (pogo_flag && (status == CHGR_BATT_CHG_STATUS_CHARGING_DISABLED)) {
		get_fg_curr(&curr);
		if ((curr*(-1) < POGO_TERM_FCC) && g_bcdev->full_flag)
			*chg_status = POWER_SUPPLY_STATUS_FULL;
		else
			*chg_status = POWER_SUPPLY_STATUS_CHARGING;
	}

  	if (last_status != status)
		last_status = status;

	return status;
}

static const struct battmngr_ops qti_fg_ops = {
	.fg_soc = get_fg_soc,
	.fg_curr = get_fg_curr,
	.fg_volt = get_fg_volt,
	.fg_temp = get_fg_temp,
	.charge_status = get_charge_status,
	.fg1_ibatt = get_fg1_curr,
	.fg2_ibatt = get_fg2_curr,
	.fg1_volt = get_fg1_volt,
	.fg2_volt = get_fg2_volt,
	.fg1_fcc = get_fg1_fcc,
	.fg2_fcc = get_fg2_fcc,
	.fg1_rm = get_fg1_rm,
	.fg2_rm = get_fg2_rm,
	.fg1_raw_soc = get_fg1_raw_soc,
	.fg2_raw_soc = get_fg2_raw_soc,
	.fg1_soc = get_fg1_soc,
	.fg2_soc = get_fg2_soc,
	.set_fg1_fastcharge = set_fg1_fastCharge,
	.set_fg2_fastcharge = set_fg2_fastCharge,
	.get_fg1_fastcharge = get_fg1_fastCharge,
	.get_fg2_fastcharge = NULL,
	.fg1_temp = get_fg1_temp,
	.fg2_temp = get_fg2_temp,
	.fg_suspend = set_batt_suspend,
	.set_term_cur = set_termination_current,
	.get_batt_auth = get_batt_authentication,
	.get_chip_ok = get_chip_ok,
};

int write_voter_prop_id(u8 buff, u32 val)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	struct xm_voter_resp_msg req_msg = { { 0 } };

	req_msg.property_id = XM_PROP_ALL_VOTER;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;
	req_msg.value = val;
	req_msg.vote_type = buff;
	req_msg.return_code = 1;

	return battery_chg_write(g_bcdev, &req_msg, sizeof(req_msg));
}
EXPORT_SYMBOL(write_voter_prop_id);

int read_voter_property_id(u8 buff, u32 prop_id)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	struct xm_voter_resp_msg req_msg = { { 0 } };

	req_msg.property_id = XM_PROP_ALL_VOTER;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_get;
	req_msg.value = 0;
	req_msg.vote_type = buff;

	return battery_chg_write(g_bcdev, &req_msg, sizeof(req_msg));
}
EXPORT_SYMBOL(read_voter_property_id);

int sc8651_wpc_gate_set(int value)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = write_property_id(g_bcdev, pst, XM_PROP_SC8561_WPC_MOS, value);
	if (rc < 0)
		return rc;

	return rc;
}
EXPORT_SYMBOL(sc8651_wpc_gate_set);

/*
*< 0-unknown (default), 1-charging (any charging, pre,trickle, taper fullon),
* 2-discharging (no input),
* 3-not charging (paused, disable charging),
* 4-full (terminated, inhibited)>
*/
int qti_get_ADC_CHGR_STATUS(void)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;
	int adc_chg_status = 0;

	rc = read_property_id(g_bcdev, pst, XM_PROP_ADC_CHGR_STATUS);
	if (rc < 0)
		return rc;
	adc_chg_status = pst->prop[XM_PROP_ADC_CHGR_STATUS];

	return adc_chg_status;
}
EXPORT_SYMBOL(qti_get_ADC_CHGR_STATUS);

int qti_enale_charge(int enable)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = write_property_id(g_bcdev, pst, XM_PROP_ENABLE_CHARGING, enable);
	if (rc < 0)
		return rc;

	return rc;
}
EXPORT_SYMBOL(qti_enale_charge);

int qti_set_keyboard_plugin(int plugin)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = write_property_id(g_bcdev, pst, XM_PROP_KEYBOARD_PLUGIN, plugin);
	if (rc < 0)
		return rc;

	return rc;
}
EXPORT_SYMBOL(qti_set_keyboard_plugin);


int qti_get_DCIN_STATE(void)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_XM];
	int rc, val;

	rc = read_property_id(g_bcdev, pst, XM_PROP_DCIN_STATE);
	if (rc < 0) {
		return rc;
	}

	val = pst->prop[XM_PROP_DCIN_STATE];
	return val;
}
EXPORT_SYMBOL(qti_get_DCIN_STATE);

int qti_deal_report(void)
{
	struct psy_state *pst = &g_bcdev->psy_list[PSY_TYPE_USB];

	if (g_bcdev)
		schedule_work(&g_bcdev->usb_type_work);

	if (pst && pst->psy) {
		power_supply_changed(pst->psy);
		schedule_delayed_work(&g_bcdev->xm_prop_change_work, 0);
	}

	return NOTIFY_OK;
}
EXPORT_SYMBOL(qti_deal_report);

#define BATT_OVERHEAT_THRESHOLD		580
#define BATT_WARM_THRESHOLD		480
#define BATT_COOL_THRESHOLD		150
#define BATT_COLD_THRESHOLD		0
static int qti_get_battery_health(void)
{
	int rc = 0, temp = 0, health = 0;

	rc = get_fg_temp(&temp);
	temp = DIV_ROUND_CLOSEST(temp, 10);

	if (rc < 0)
		return -EINVAL;

	if (temp >= BATT_OVERHEAT_THRESHOLD)
		health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (temp >= BATT_WARM_THRESHOLD && temp < BATT_OVERHEAT_THRESHOLD)
		health = POWER_SUPPLY_HEALTH_WARM;
	else if (temp >= BATT_COOL_THRESHOLD && temp < BATT_WARM_THRESHOLD)
		health = POWER_SUPPLY_HEALTH_GOOD;
	else if (temp >= BATT_COLD_THRESHOLD && temp < BATT_COOL_THRESHOLD)
		health = POWER_SUPPLY_HEALTH_COOL;
	else if (temp < BATT_COLD_THRESHOLD)
		health = POWER_SUPPLY_HEALTH_COLD;

	return health;
}

static int qti_get_charge_type(void)
{
	u8 val = 0;
	val = qti_get_ADC_CHGR_STATUS();

	switch (val)
	{
	case CHGR_BATT_CHGR_STATUS_TRICKLE:
	case CHGR_BATT_CHGR_STATUS_PRECHARGE:
		return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case CHGR_BATT_CHGR_STATUS_FULLON:
		return POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case CHGR_BATT_CHGR_STATUS_TAPER:
	case CHGR_BATT_CHGR_STATUS_TERMINATION:
		return POWER_SUPPLY_CHARGE_TYPE_TAPER_EXT;
		break;
	default:
		return POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	}

	return POWER_SUPPLY_CHARGE_TYPE_NONE;
}

static int qti_get_batt_constant_volt(struct battery_chg_dev *bcdev)
{
	int curr_volt = 0;

	if (!bcdev->fv_votable)
		bcdev->fv_votable = find_votable("FV");

	if (bcdev->fv_votable) {
		curr_volt = get_effective_result(bcdev->fv_votable);
	}

	return curr_volt;
}

static int qti_get_batt_constant_curr(struct battery_chg_dev *bcdev)
{
	int curr_limit = 0;

	if (!bcdev->fcc_votable)
		bcdev->fcc_votable = find_votable("FCC");

	if (bcdev->fcc_votable) {
		curr_limit = get_effective_result(bcdev->fcc_votable);
	}

	return curr_limit;
}
#endif

static int write_ss_auth_prop_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id, u32* buff)
{
	struct xm_ss_auth_resp_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;
	memcpy(req_msg.data, buff, BATTERY_SS_AUTH_DATA_LEN*sizeof(u32));

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static int read_ss_auth_property_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id)
{
	struct xm_ss_auth_resp_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_get;

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static void battery_chg_notify_enable(struct battery_chg_dev *bcdev)
{
	struct battery_charger_set_notify_msg req_msg = { { 0 } };
	int rc;

	if (!bcdev->notify_en) {
		/* Send request to enable notification */
		req_msg.hdr.owner = MSG_OWNER_BC;
		req_msg.hdr.type = MSG_TYPE_NOTIFY;
		req_msg.hdr.opcode = BC_SET_NOTIFY_REQ;

		rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
		if (rc < 0)
			pr_err("Failed to enable notification rc=%d\n", rc);
		else
			bcdev->notify_en = true;
	}
}
static void battery_chg_state_cb(void *priv, enum pmic_glink_state state)
{
	struct battery_chg_dev *bcdev = priv;

	down_write(&bcdev->state_sem);
	if (!bcdev->initialized) {
		pr_warn("Driver not initialized, pmic_glink state %d\n", state);
		up_write(&bcdev->state_sem);
		return;
	}
	atomic_set(&bcdev->state, state);
	up_write(&bcdev->state_sem);

	if (state == PMIC_GLINK_STATE_UP)
		schedule_work(&bcdev->subsys_up_work);
	else if (state == PMIC_GLINK_STATE_DOWN)
		bcdev->notify_en = false;
}

/**
 * qti_battery_charger_get_prop() - Gets the property being requested
 *
 * @name: Power supply name
 * @prop_id: Property id to be read
 * @val: Pointer to value that needs to be updated
 *
 * Return: 0 if success, negative on error.
 */
int qti_battery_charger_get_prop(const char *name,
				enum battery_charger_prop prop_id, int *val)
{
	struct power_supply *psy;
	struct battery_chg_dev *bcdev;
	struct psy_state *pst;
	int rc = 0;

	if (prop_id >= BATTERY_CHARGER_PROP_MAX)
		return -EINVAL;

	if (strcmp(name, "battery") && strcmp(name, "usb") &&
	    strcmp(name, "wireless"))
		return -EINVAL;

	psy = power_supply_get_by_name(name);
	if (!psy)
		return -ENODEV;

	bcdev = power_supply_get_drvdata(psy);
	if (!bcdev)
		return -ENODEV;

	power_supply_put(psy);

	switch (prop_id) {
	case BATTERY_RESISTANCE:
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
		rc = read_property_id(bcdev, pst, BATT_RESISTANCE);
		if (!rc)
			*val = pst->prop[BATT_RESISTANCE];
		break;
#if defined(CONFIG_MI_ENABLE_DP)
	case USB_CC_ORIENTATION:
		pst = &bcdev->psy_list[PSY_TYPE_XM];
		rc = read_property_id(bcdev, pst, XM_PROP_CC_ORIENTATION);
		if (!rc) {
			*val = pst->prop[XM_PROP_CC_ORIENTATION];
		}
		break;
	case HAS_DP_PS5169:
		pst = &bcdev->psy_list[PSY_TYPE_XM];
		rc = read_property_id(bcdev, pst, XM_PROP_HAS_DP);
		if (!rc) {
			*val = pst->prop[XM_PROP_HAS_DP];
		}
		break;
#endif
	default:
		break;
	}

	return rc;
}
EXPORT_SYMBOL(qti_battery_charger_get_prop);

static bool validate_message(struct battery_charger_resp_msg *resp_msg,
				size_t len)
{
	struct xm_verify_digest_resp_msg *verify_digest_resp_msg = (struct xm_verify_digest_resp_msg *)resp_msg;
	struct xm_ss_auth_resp_msg *ss_auth_resp_msg = (struct xm_ss_auth_resp_msg *)resp_msg;
#if defined (CONFIG_QTI_POGO_CHG)
	struct xm_voter_resp_msg *voter_resp_msg = (struct xm_voter_resp_msg *)resp_msg;

	if (len == sizeof(*voter_resp_msg)) {
		return true;
	}
#endif

	if (len == sizeof(*verify_digest_resp_msg) || len == sizeof(*ss_auth_resp_msg)) {
		return true;
	}

	if (len != sizeof(*resp_msg)) {
		pr_err("Incorrect response length %zu for opcode %#x\n", len,
			resp_msg->hdr.opcode);
		return false;
	}

	if (resp_msg->ret_code) {
		pr_err("Error in response for opcode %#x prop_id %u, rc=%d\n",
			resp_msg->hdr.opcode, resp_msg->property_id,
			(int)resp_msg->ret_code);
		return false;
	}

	return true;
}

#define MODEL_DEBUG_BOARD	"Debug_Board"
static void handle_message(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct battery_charger_resp_msg *resp_msg = data;
	struct battery_model_resp_msg *model_resp_msg = data;
	struct xm_verify_digest_resp_msg *verify_digest_resp_msg = data;
	struct xm_ss_auth_resp_msg *ss_auth_resp_msg = data;
	struct wireless_chip_fw_msg *wireless_chip_fw_resp_msg = data;
	struct wireless_tx_uuid_msg *wireless_tx_uuid_msg = data;
	struct chg_debug_msg *chg_debug_data = data;
	struct wireless_fw_check_resp *fw_check_msg;
	struct wireless_fw_push_buf_resp *fw_resp_msg;
	struct wireless_fw_update_status *fw_update_msg;
	struct wireless_fw_get_version_resp *fw_ver_msg;
	struct psy_state *pst;
	bool ack_set = false;

	switch (resp_msg->hdr.opcode) {
	case BC_BATTERY_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

		/* Handle model response uniquely as it's a string */
		if (pst->model && len == sizeof(*model_resp_msg)) {
			memcpy(pst->model, model_resp_msg->model, MAX_STR_LEN);
			ack_set = true;
			bcdev->debug_battery_detected = !strcmp(pst->model,
					MODEL_DEBUG_BOARD);
			break;
		}

		/* Other response should be of same type as they've u32 value */
		if (validate_message(resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_USB_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		if (validate_message(resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_WLS_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_WLS];
		if (validate_message(resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}
		break;
	case BC_XM_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_XM];

		/* Handle digest response uniquely as it's a string */
		if (bcdev->digest && len == sizeof(*verify_digest_resp_msg)) {
			memcpy(bcdev->digest, verify_digest_resp_msg->digest, BATTERY_DIGEST_LEN);
			ack_set = true;
			break;
		}
		if (bcdev->ss_auth_data && len == sizeof(*ss_auth_resp_msg)) {
			memcpy(bcdev->ss_auth_data, ss_auth_resp_msg->data, BATTERY_SS_AUTH_DATA_LEN*sizeof(u32));
			ack_set = true;
			break;
		}
		/* Handle model response uniquely as it's a string */
		if (len == sizeof(*wireless_chip_fw_resp_msg)) {
			memcpy(bcdev->wireless_chip_fw_version, wireless_chip_fw_resp_msg->version, WIRELESS_CHIP_FW_VERSION_LEN);
			ack_set = true;
			break;
		}
		if (len == sizeof(*wireless_tx_uuid_msg)) {
			memcpy(bcdev->wireless_tx_uuid_version, wireless_tx_uuid_msg->version, WIRELESS_UUID_LEN);
			ack_set = true;
			break;
		}
		if (len == sizeof(*chg_debug_data)) {
			if (chg_debug_data->type == CHG_ADSP_LOG) {
				pr_err("[ADSP] %s\n", chg_debug_data->data); // print adsp log
			} else if (chg_debug_data->type == CHG_WLS_DEBUG) {
				memcpy(bcdev->wls_debug_data, chg_debug_data->data, CHG_DEBUG_DATA_LEN);
				ack_set = true;
			}
			break;
		}
		if (validate_message(resp_msg, len) && resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}
		break;
	case BC_BATTERY_STATUS_SET:
	case BC_USB_STATUS_SET:
	case BC_WLS_STATUS_SET:
		if (validate_message(data, len))
			ack_set = true;
		break;
	case BC_XM_STATUS_SET:
		if (validate_message(data, len))
			ack_set = true;
		break;
	case BC_SET_NOTIFY_REQ:
	case BC_DISABLE_NOTIFY_REQ:
	case BC_SHUTDOWN_NOTIFY:
	case BC_SHIP_MODE_REQ_SET:
	case BC_SHUTDOWN_REQ_SET:
		/* Always ACK response for notify or ship_mode or shutdown request */
		ack_set = true;
		break;
	case BC_WLS_FW_CHECK_UPDATE:
		if (len == sizeof(*fw_check_msg)) {
			fw_check_msg = data;
			if (fw_check_msg->ret_code == 1)
				bcdev->wls_fw_update_reqd = true;
			ack_set = true;
		} else {
			pr_err("Incorrect response length %zu for wls_fw_check_update\n",
				len);
		}
		break;
	case BC_WLS_FW_PUSH_BUF_RESP:
		if (len == sizeof(*fw_resp_msg)) {
			fw_resp_msg = data;
			if (fw_resp_msg->fw_update_status == 1)
				complete(&bcdev->fw_buf_ack);
		} else {
			pr_err("Incorrect response length %zu for wls_fw_push_buf_resp\n",
				len);
		}
		break;
	case BC_WLS_FW_UPDATE_STATUS_RESP:
		if (len == sizeof(*fw_update_msg)) {
			fw_update_msg = data;
			if (fw_update_msg->fw_update_done == 1)
				complete(&bcdev->fw_update_ack);
			else
				pr_err("Wireless FW update not done %d\n",
					(int)fw_update_msg->fw_update_done);
		} else {
			pr_err("Incorrect response length %zu for wls_fw_update_status_resp\n",
				len);
		}
		break;
	case BC_WLS_FW_GET_VERSION:
		if (len == sizeof(*fw_ver_msg)) {
			fw_ver_msg = data;
			bcdev->wls_fw_version = fw_ver_msg->fw_version;
			ack_set = true;
		} else {
			pr_err("Incorrect response length %zu for wls_fw_get_version\n",
				len);
		}
		break;
	default:
		break;
	}

	if (ack_set)
		complete(&bcdev->ack);
}

static struct power_supply_desc usb_psy_desc;

static void battery_chg_update_usb_type_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, usb_type_work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

#ifdef CONFIG_QTI_POGO_CHG
	if (!bcdev->battmg_dev) {
		bcdev->battmg_dev = check_nano_ops();
	}

	if (g_battmngr_noti && pogo_flag) {
		if (bcdev->battmg_dev) {
			pst->prop[USB_ADAP_TYPE] = battmngr_noops_get_usb_type(bcdev->battmg_dev);
		}
	} else {
		rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
		if (rc < 0) {
			return;
		}
	}
#else
	rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
	if (rc < 0) {
		return;
	}
#endif

	/* Reset usb_icl_ua whenever USB adapter type changes */
	if (pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_SDP &&
	    pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_PD)
		bcdev->usb_icl_ua = 0;

	switch (pst->prop[USB_ADAP_TYPE]) {
	if (bcdev->report_power_absent == 1
		&& pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_UNKNOWN) {
		usb_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	}
	case POWER_SUPPLY_USB_TYPE_SDP:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
	case POWER_SUPPLY_USB_TYPE_DCP:
	case POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID:
	case QTI_POWER_SUPPLY_USB_TYPE_HVDCP:
	case QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3:
	case QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case POWER_SUPPLY_USB_TYPE_CDP:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case POWER_SUPPLY_USB_TYPE_ACA:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_ACA;
		break;
	case POWER_SUPPLY_USB_TYPE_C:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_TYPE_C;
		break;
	case POWER_SUPPLY_USB_TYPE_PD:
	case POWER_SUPPLY_USB_TYPE_PD_DRP:
	case POWER_SUPPLY_USB_TYPE_PD_PPS:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB_PD;
		break;
	default:
		usb_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	}
}

static void battery_chg_check_status_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev,
					battery_check_work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	struct psy_state *usb_pst = &bcdev->psy_list[PSY_TYPE_USB];
	struct psy_state *wireless_pst = &bcdev->psy_list[PSY_TYPE_WLS];
	int rc;

	rc = read_property_id(bcdev, usb_pst, USB_ONLINE);
	if (rc < 0) {
		return;
	}

	rc = read_property_id(bcdev, wireless_pst, WLS_ONLINE);
	if (rc < 0) {
		return;
	}

	if (usb_pst->prop[USB_ONLINE] == 0 && wireless_pst->prop[WLS_ONLINE] == 0) {
		if (!bcdev->extreme_mode_en)
		  return;
	}

	rc = read_property_id(bcdev, pst, BATT_CAPACITY);
	if (rc < 0) {
		return;
	}

	if (DIV_ROUND_CLOSEST(pst->prop[BATT_CAPACITY], 100) > 0) {
		return;
	}

	/*
	 * If we are here, then battery is not charging and SOC is 0.
	 * Check the battery voltage and if it's lower than shutdown voltage,
	 * then initiate an emergency shutdown.
	 */

	rc = read_property_id(bcdev, pst, BATT_VOLT_NOW);
	if (rc < 0) {
		return;
	}

	if (pst->prop[BATT_VOLT_NOW] / 1000 > bcdev->shutdown_volt_mv) {
		return;
	}

	msleep(100);

	if (bcdev->extreme_mode_en) {
		kernel_power_off();
	} else {
		bcdev->report_power_absent = true;
	}
}

/* pen_connect_strategy start */
static void pen_charge_notifier_work(struct work_struct *work)
{
	int rc;
	int pen_charge_connect;
	static int pen_charge_connect_last_time  = -1;
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev, pen_notifier_work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];

	/* set pen_charge_connect as 1 when hall3 or hall4 was set as 0 */

	rc = read_property_id(bcdev, pst, XM_PROP_PEN_HALL3);
	if (rc < 0) {
		printk(KERN_ERR "%s:read_property_id XM_PROP_PEN_HALL3 err\n", __func__);
		return;
	}
	printk("%s:XM_PROP_PEN_HALL3 is %d\n", __func__, pst->prop[XM_PROP_PEN_HALL3]);

	rc = read_property_id(bcdev, pst, XM_PROP_PEN_HALL4);
	if (rc < 0) {
		printk(KERN_ERR "%s:read_property_id XM_PROP_PEN_HALL4 err\n", __func__);
		return;
	}
	printk("%s:XM_PROP_PEN_HALL4 is %d\n", __func__, pst->prop[XM_PROP_PEN_HALL4]);

	pen_charge_connect = !(!!pst->prop[XM_PROP_PEN_HALL3] & !!pst->prop[XM_PROP_PEN_HALL4]);

	if(pen_charge_connect_last_time != pen_charge_connect) {
		atomic_notifier_call_chain(&pen_charge_state_notifier, pen_charge_connect, NULL);
	} else {
		printk("%s:pen_charge_connect is %d, pen_charge_connect_last_time is %d, skip call chain\n", __func__, pen_charge_connect, pen_charge_connect_last_time);
	}
	pen_charge_connect_last_time = pen_charge_connect;
}

int pen_charge_state_notifier_register_client(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&pen_charge_state_notifier, nb);
}
EXPORT_SYMBOL(pen_charge_state_notifier_register_client);

int pen_charge_state_notifier_unregister_client(struct notifier_block *nb)
{
	return atomic_notifier_chain_unregister(&pen_charge_state_notifier, nb);
}
EXPORT_SYMBOL(pen_charge_state_notifier_unregister_client);
/* pen_connect_strategy end */

static void handle_notification(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct battery_charger_notify_msg *notify_msg = data;
	struct psy_state *pst = NULL;
	u32 hboost_vmax_mv, notification;

	if (len != sizeof(*notify_msg)) {
		return;
	}

	notification = notify_msg->notification;
	if ((notification & 0xffff) == BC_HBOOST_VMAX_CLAMP_NOTIFY) {
		hboost_vmax_mv = (notification >> 16) & 0xffff;
		raw_notifier_call_chain(&hboost_notifier, VMAX_CLAMP, &hboost_vmax_mv);
		return;
	}

	switch (notification) {
	case BC_BATTERY_STATUS_GET:
	case BC_GENERIC_NOTIFY:
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
		if (bcdev->shutdown_volt_mv > 0)
			schedule_work(&bcdev->battery_check_work);
		break;
	case BC_USB_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		schedule_work(&bcdev->usb_type_work);
		break;
	case BC_WLS_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_WLS];
		break;
	case BC_XM_STATUS_GET:
		schedule_delayed_work(&bcdev->xm_prop_change_work, 0);
		/* pen_connect_strategy start */
		schedule_work(&bcdev->pen_notifier_work);
		/* pen_connect_strategy end */
		break;
	default:
		break;
	}

	if (pst && pst->psy) {
		/*
		 * For charger mode, keep the device awake at least for 50 ms
		 * so that device won't enter suspend when a non-SDP charger
		 * is removed. This would allow the userspace process like
		 * "charger" to be able to read power supply uevents to take
		 * appropriate actions (e.g. shutting down when the charger is
		 * unplugged).
		 */
		power_supply_changed(pst->psy);
		if (!bcdev->reverse_chg_flag)
			pm_wakeup_dev_event(bcdev->dev, 50, true);
	}
}

#ifdef CONFIG_QTI_POGO_CHG
static void handle_irq_notification(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct battery_charger_irq_notify_msg *notify_msg = data;

	if (len != sizeof(*notify_msg)) {
		return;
	}

	if (g_battmngr_noti) {
		g_battmngr_noti->irq_msg.irq_type = (int)notify_msg->irq_type;
		g_battmngr_noti->irq_msg.value = (int)notify_msg->value;
		pogo_flag = g_battmngr_noti->irq_msg.value;
		if (g_battmngr_noti->irq_msg.irq_type == DCIN_IRQ)
			qti_deal_report();
		if (!pogo_flag)
			g_bcdev->full_flag = false;
		else
			g_battmngr_noti->misc_msg.thermal_level = g_bcdev->curr_thermal_level;
	} else {
		return;
	}

	switch (notify_msg->irq_type) {
	case DCIN_IRQ:
		battmngr_notifier_call_chain(BATTMNGR_EVENT_IRQ, g_battmngr_noti);
		break;
	case CHARGER_DONE_IRQ:
		battmngr_notifier_call_chain(BATTMNGR_EVENT_IRQ, g_battmngr_noti);
		break;
	case RECHARGE_IRQ:
		battmngr_notifier_call_chain(BATTMNGR_EVENT_IRQ, g_battmngr_noti);
		break;
	default:
		break;
	}
}
#endif

static int battery_chg_callback(void *priv, void *data, size_t len)
{
	struct pmic_glink_hdr *hdr = data;
	struct battery_chg_dev *bcdev = priv;

	down_read(&bcdev->state_sem);

	if (!bcdev->initialized) {
		up_read(&bcdev->state_sem);
		return 0;
	}

	if (hdr->opcode == BC_NOTIFY_IND)
		handle_notification(bcdev, data, len);
#ifdef CONFIG_QTI_POGO_CHG
	else if (hdr->opcode == BC_NOTIFY_IRQ)
		handle_irq_notification(bcdev, data, len);
#endif
	else
		handle_message(bcdev, data, len);

	up_read(&bcdev->state_sem);

	return 0;
}

static int wls_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_WLS];
	int prop_id, rc;

	pval->intval = -ENODATA;

	if (prop == POWER_SUPPLY_PROP_PRESENT) {
		pval->intval = bcdev->boost_mode;
		return 0;
	}

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	pval->intval = pst->prop[prop_id];

	if (prop == POWER_SUPPLY_PROP_ONLINE) {
		if (pval->intval == 1 && bcdev->report_power_absent)
			pval->intval = 0;
		if (bcdev->debug_work_en == 0 && pval->intval == 1)
			schedule_delayed_work(&bcdev->charger_debug_info_print_work, 5 * HZ);
	}

	return 0;
}

static int wls_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	return 0;
}

static int wls_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	return 0;
}

static enum power_supply_property wls_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_PRESENT,
};

static const struct power_supply_desc wls_psy_desc = {
	.name			= "wireless",
	.type			= POWER_SUPPLY_TYPE_WIRELESS,
	.properties		= wls_props,
	.num_properties		= ARRAY_SIZE(wls_props),
	.get_property		= wls_psy_get_prop,
	.set_property		= wls_psy_set_prop,
	.property_is_writeable	= wls_psy_prop_is_writeable,
};

static const char *get_wls_type_name(u32 wls_type)
{
	if (wls_type >= ARRAY_SIZE(qc_power_supply_wls_type_text))
		return "Unknown";

	return qc_power_supply_wls_type_text[wls_type];
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

typedef enum {
	POWER_SUPPLY_USB_REAL_TYPE_HVDCP2=0x80,
	POWER_SUPPLY_USB_REAL_TYPE_HVDCP3=0x81,
	POWER_SUPPLY_USB_REAL_TYPE_HVDCP3P5=0x82,
	POWER_SUPPLY_USB_REAL_TYPE_USB_FLOAT=0x83,
	POWER_SUPPLY_USB_REAL_TYPE_HVDCP3_CLASSB=0x84,
}power_supply_usb_type;

enum power_supply_quick_charge_type {
	QUICK_CHARGE_NORMAL = 0,		/* Charging Power <= 10W */
	QUICK_CHARGE_FAST,			/* 10W < Charging Power <= 20W */
	QUICK_CHARGE_FLASH,			/* 20W < Charging Power <= 30W */
	QUICK_CHARGE_TURBE,			/* 30W < Charging Power <= 50W */
	QUICK_CHARGE_SUPER,			/* Charging Power > 50W */
	QUICK_CHARGE_MAX,
};

struct quick_charge {
	int adap_type;
	enum power_supply_quick_charge_type adap_cap;
};

struct quick_charge adapter_cap[11] = {
	{ POWER_SUPPLY_USB_TYPE_SDP,        QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_DCP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_CDP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_ACA,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_REAL_TYPE_USB_FLOAT,  QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_PD,       QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_USB_REAL_TYPE_HVDCP2,    QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_USB_REAL_TYPE_HVDCP3,  QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_USB_REAL_TYPE_HVDCP3_CLASSB,  QUICK_CHARGE_FLASH },
	{ POWER_SUPPLY_USB_REAL_TYPE_HVDCP3P5,  QUICK_CHARGE_FLASH },
	{0, 0},
};
#define ADAPTER_NONE              0x0
#define ADAPTER_XIAOMI_QC3_20W    0x9
#define ADAPTER_XIAOMI_PD_20W     0xa
#define ADAPTER_XIAOMI_CAR_20W    0xb
#define ADAPTER_XIAOMI_PD_30W     0xc
#define ADAPTER_VOICE_BOX_30W     0xd
#define ADAPTER_XIAOMI_PD_50W     0xe
#define ADAPTER_XIAOMI_PD_60W     0xf
#define ADAPTER_XIAOMI_PD_100W    0x10
static ssize_t quick_charge_type_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int i = 0,verify_digiest = 0;
	int rc;
	u8 result = QUICK_CHARGE_NORMAL;
	enum power_supply_usb_type		real_charger_type = 0;
	int		batt_health;
	u32 power_max;
	u32 batt_auth;
	u32 bap_match;

#if defined(CONFIG_MI_WIRELESS)
	struct power_supply *wls_psy = NULL;
	union power_supply_propval val = {0, };
	int wls_present = 0;
#endif

#ifdef CONFIG_QTI_POGO_CHG
	if (g_battmngr_noti && pogo_flag) {
		if (!bcdev->battmg_dev) {
			bcdev->battmg_dev = check_nano_ops();
		}

		batt_health = qti_get_battery_health();
		pst->prop[BATT_HEALTH] = batt_health;

		if (bcdev->battmg_dev) {
			real_charger_type = battmngr_noops_get_real_type(bcdev->battmg_dev);
			power_max = battmngr_noops_get_power_max(bcdev->battmg_dev);
		}
	} else {
		rc = read_property_id(bcdev, pst, BATT_HEALTH);
		if (rc < 0)
			return rc;
		batt_health = pst->prop[BATT_HEALTH];
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		rc = read_property_id(bcdev, pst, USB_REAL_TYPE);
		if (rc < 0)
			return rc;
		real_charger_type = pst->prop[USB_REAL_TYPE];

		pst = &bcdev->psy_list[PSY_TYPE_XM];
		rc = read_property_id(bcdev, pst, XM_PROP_PD_VERIFED);
		verify_digiest = pst->prop[XM_PROP_PD_VERIFED];

		rc = read_property_id(bcdev, pst, XM_PROP_POWER_MAX);
		power_max = pst->prop[XM_PROP_POWER_MAX];
	}
	rc = read_property_id(bcdev, pst, XM_PROP_AUTHENTIC);
	batt_auth = pst->prop[XM_PROP_AUTHENTIC];

	rc = read_property_id(bcdev, pst, XM_PROP_BATTERY_ADAPT_POWER_MATCH);
	bap_match = pst->prop[XM_PROP_BATTERY_ADAPT_POWER_MATCH];
#else
	rc = read_property_id(bcdev, pst, BATT_HEALTH);
	if (rc < 0)
		return rc;
	batt_health = pst->prop[BATT_HEALTH];
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_REAL_TYPE);
	if (rc < 0)
		return rc;
	real_charger_type = pst->prop[USB_REAL_TYPE];

	pst = &bcdev->psy_list[PSY_TYPE_XM];
	rc = read_property_id(bcdev, pst, XM_PROP_PD_VERIFED);
	verify_digiest = pst->prop[XM_PROP_PD_VERIFED];

	rc = read_property_id(bcdev, pst, XM_PROP_POWER_MAX);
	power_max = pst->prop[XM_PROP_POWER_MAX];

	rc = read_property_id(bcdev, pst, XM_PROP_AUTHENTIC);
	batt_auth = pst->prop[XM_PROP_AUTHENTIC];

	rc = read_property_id(bcdev, pst, XM_PROP_BATTERY_ADAPT_POWER_MATCH);
	bap_match = pst->prop[XM_PROP_BATTERY_ADAPT_POWER_MATCH];
#endif


	if ((batt_health == POWER_SUPPLY_HEALTH_COLD) || (batt_health == POWER_SUPPLY_HEALTH_WARM) || (batt_health == POWER_SUPPLY_HEALTH_OVERHEAT)
		|| (batt_health == POWER_SUPPLY_HEALTH_HOT) || (batt_auth == 0) || (bap_match == 0))
		result = QUICK_CHARGE_NORMAL;
	else if (real_charger_type == POWER_SUPPLY_USB_TYPE_PD_PPS && verify_digiest ==1) {
		if(power_max >= 50) {
			result = QUICK_CHARGE_SUPER;
		}
		else
			result = QUICK_CHARGE_TURBE;
		}
	else if (real_charger_type == POWER_SUPPLY_USB_TYPE_PD_PPS)
		result = QUICK_CHARGE_FAST;
	else {
		while (adapter_cap[i].adap_type != 0) {
			if (real_charger_type == adapter_cap[i].adap_type) {
				result = adapter_cap[i].adap_cap;
			}
			i++;
		}
	}

#ifdef CONFIG_QTI_POGO_CHG
	if (g_battmngr_noti && pogo_flag) {
		if ((batt_health == POWER_SUPPLY_HEALTH_COLD) || (batt_health == POWER_SUPPLY_HEALTH_WARM) || (batt_health == POWER_SUPPLY_HEALTH_OVERHEAT)
			|| (batt_health == POWER_SUPPLY_HEALTH_HOT) || (batt_auth == 0) || (bap_match == 0) || bcdev->input_suspend)
			result = QUICK_CHARGE_NORMAL;
		else if (real_charger_type == POWER_SUPPLY_USB_TYPE_PD)
			result = QUICK_CHARGE_FLASH;
		pr_err("quick charge:%d, real_charger_type:%d\n", result, real_charger_type);
	}
	return scnprintf(buf, PAGE_SIZE, "%u", result);
#endif

#if defined(CONFIG_MI_WIRELESS)
	wls_psy = bcdev->psy_list[PSY_TYPE_WLS].psy;
	if (wls_psy != NULL) {
	rc = wls_psy_get_prop(wls_psy, POWER_SUPPLY_PROP_ONLINE, &val);
	if (!rc)
	      wls_present = val.intval;
	else
	      wls_present = 0;
	}
	if(wls_present) {
		if(power_max >= 30) {
			result = QUICK_CHARGE_SUPER;
		}
		else if(power_max == 20) {
			result = QUICK_CHARGE_FLASH;
		}
		else {
			result = QUICK_CHARGE_NORMAL;
		}
	}
#endif

	return scnprintf(buf, PAGE_SIZE, "%u", result);
}
static CLASS_ATTR_RO(quick_charge_type);

static ssize_t apdo_max_show(struct class *c,
                                        struct class_attribute *attr, char *buf)
{
        struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                                battery_class);
        struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
        int rc;

        rc = read_property_id(bcdev, pst, XM_PROP_APDO_MAX);
        if (rc < 0)
                return rc;

        return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_APDO_MAX]);
}
static CLASS_ATTR_RO(apdo_max);

static ssize_t soc_decimal_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SOC_DECIMAL);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_SOC_DECIMAL]);
}
static CLASS_ATTR_RO(soc_decimal);

static ssize_t soc_decimal_rate_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SOC_DECIMAL_RATE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_SOC_DECIMAL_RATE]);
}
static CLASS_ATTR_RO(soc_decimal_rate);

static ssize_t smart_batt_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_SMART_BATT, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t smart_batt_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SMART_BATT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SMART_BATT]);
}
static CLASS_ATTR_RW(smart_batt);

static ssize_t fg_raw_soc_show(struct class *c,
                                        struct class_attribute *attr, char *buf)
{
    struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                    battery_class);
    struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
    int rc;
    rc = read_property_id(bcdev, pst, XM_PROP_FG_RAW_SOC);
    if (rc < 0)
            return rc;
    return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG_RAW_SOC]);
}
static CLASS_ATTR_RO(fg_raw_soc);

static ssize_t night_charging_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_NIGHT_CHARGING, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t night_charging_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_NIGHT_CHARGING);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_NIGHT_CHARGING]);
}
static CLASS_ATTR_RW(night_charging);

static int usb_psy_set_icl(struct battery_chg_dev *bcdev, u32 prop_id, int val)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	u32 temp;
	int rc;

	rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
	if (rc < 0) {
		return rc;
	}

	/* Allow this only for SDP, CDP or USB_PD and not for other charger types */
	switch (pst->prop[USB_ADAP_TYPE]) {
	case POWER_SUPPLY_USB_TYPE_SDP:
	case POWER_SUPPLY_USB_TYPE_PD:
	case POWER_SUPPLY_USB_TYPE_CDP:
		break;
	default:
		return -EINVAL;
	}

	/*
	 * Input current limit (ICL) can be set by different clients. E.g. USB
	 * driver can request for a current of 500/900 mA depending on the
	 * port type. Also, clients like EUD driver can pass 0 or -22 to
	 * suspend or unsuspend the input for its use case.
	 */

	temp = val;
	if (val < 0)
		temp = UINT_MAX;

	rc = write_property_id(bcdev, pst, prop_id, temp);
	if (rc < 0) {
		pr_err("Failed to set ICL (%u uA) rc=%d\n", temp, rc);
	} else {
		bcdev->usb_icl_ua = temp;
	}

	return rc;
}

static int usb_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int prop_id, rc;

	pval->intval = -ENODATA;

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

#ifdef CONFIG_QTI_POGO_CHG
	if (g_battmngr_noti && pogo_flag) {
		if (!bcdev->battmg_dev) {
			bcdev->battmg_dev = check_nano_ops();
		}
		switch (prop_id)
		{
		case USB_ONLINE:
			if (bcdev->battmg_dev) {
				pst->prop[prop_id] = battmngr_noops_get_online(bcdev->battmg_dev);
			}
			break;
		case USB_INPUT_CURR_LIMIT:
			pst->prop[prop_id] = battmngr_noops_get_input_curr_limit(bcdev->battmg_dev);
			break;
		case USB_ADAP_TYPE:
			if (bcdev->battmg_dev) {
				pst->prop[prop_id] = battmngr_noops_get_usb_type(bcdev->battmg_dev);
			}
			break;
		default:
			break;
		}
	} else {
		rc = read_property_id(bcdev, pst, prop_id);
		if (rc < 0)
			return rc;
	}
#else
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;
#endif

	pval->intval = pst->prop[prop_id];
	if (prop == POWER_SUPPLY_PROP_TEMP)
		pval->intval = DIV_ROUND_CLOSEST((int)pval->intval, 10);

	if (prop == POWER_SUPPLY_PROP_ONLINE) {
		if (pval->intval == 1 && bcdev->report_power_absent)
			pval->intval = 0;
		if (bcdev->debug_work_en == 0 && pval->intval == 1)
			schedule_delayed_work(&bcdev->charger_debug_info_print_work, 5 * HZ);
	}

	return 0;
}

static int usb_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int prop_id, rc = 0;

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		rc = usb_psy_set_icl(bcdev, prop_id, pval->intval);
		break;
	default:
		break;
	}

	return rc;
}

static int usb_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	default:
		break;
	}

	return 0;
}

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_usb_type usb_psy_supported_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_ACA,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_PD_PPS,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID,
};

static struct power_supply_desc usb_psy_desc = {
	.name			= "usb",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= usb_props,
	.num_properties		= ARRAY_SIZE(usb_props),
	.get_property		= usb_psy_get_prop,
	.set_property		= usb_psy_set_prop,
	.usb_types		= usb_psy_supported_types,
	.num_usb_types		= ARRAY_SIZE(usb_psy_supported_types),
	.property_is_writeable	= usb_psy_prop_is_writeable,
};

static int __battery_psy_set_charge_current(struct battery_chg_dev *bcdev,
					u32 fcc_ua)
{
	int rc;

	if (bcdev->restrict_chg_en) {
		fcc_ua = min_t(u32, fcc_ua, bcdev->restrict_fcc_ua);
		fcc_ua = min_t(u32, fcc_ua, bcdev->thermal_fcc_ua);
	}

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_BATTERY],
				BATT_CHG_CTRL_LIM, fcc_ua);
	if (rc < 0) {
		pr_err("Failed to set FCC %u, rc=%d\n", fcc_ua, rc);
	} else {
		bcdev->last_fcc_ua = fcc_ua;
	}

	return rc;
}

static int battery_psy_set_charge_current(struct battery_chg_dev *bcdev,
					int val)
{
	int rc = 0;
	//u32 fcc_ua, prev_fcc_ua;
	struct psy_state *pst = NULL;

	pst = &bcdev->psy_list[PSY_TYPE_XM];
	if (!bcdev->num_thermal_levels)
		return 0;

	if (bcdev->num_thermal_levels < 0) {
		return -EINVAL;
	}

	if (val < 0 || val >= bcdev->num_thermal_levels)
		return -EINVAL;

#ifdef CONFIG_QTI_POGO_CHG
	if (g_battmngr_noti && pogo_flag) {
		mutex_lock(&g_battmngr_noti->notify_lock);

		g_battmngr_noti->misc_msg.thermal_level = val;
		battmngr_notifier_call_chain(BATTMNGR_EVENT_THERMAL, g_battmngr_noti);
		mutex_unlock(&g_battmngr_noti->notify_lock);
	} else {
		rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_BATTERY],
					BATT_CHG_CTRL_LIM, val);
		if (rc < 0)
			pr_err("Failed to set ccl:%d, rc=%d\n", val, rc);
	}
#else
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_BATTERY],
				BATT_CHG_CTRL_LIM, val);
	if (rc < 0)
		pr_err("Failed to set ccl:%d, rc=%d\n", val, rc);
#endif

	bcdev->curr_thermal_level = val;

	return rc;
}

static int battery_psy_set_fcc(struct battery_chg_dev *bcdev, u32 prop_id, int val)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	u32 temp;
	int rc;

	temp = val;
	if (val < 0)
		temp = UINT_MAX;

	rc = write_property_id(bcdev, pst, prop_id, temp);
	if (!rc)
		pr_debug("Set FCC to %u\n", temp);

	return rc;
}

static int battery_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int prop_id, rc = 0;

	pval->intval = -ENODATA;

	if (prop == POWER_SUPPLY_PROP_TIME_TO_FULL_NOW)
		prop = POWER_SUPPLY_PROP_TIME_TO_FULL_AVG;

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

#ifdef CONFIG_QTI_POGO_CHG
	if (g_battmngr_noti && pogo_flag) {
		switch (prop_id)
		{
		case BATT_STATUS:
			rc = get_charge_status(&pst->prop[prop_id]);
			break;
		case BATT_HEALTH:
			pst->prop[prop_id] = qti_get_battery_health();
			break;
		case BATT_PRESENT:
			pst->prop[prop_id] = 1;
			break;
		case BATT_CHG_TYPE:
			pst->prop[prop_id] = qti_get_charge_type();
			break;
		case BATT_VOLT_MAX:
			pst->prop[prop_id] = qti_get_batt_constant_volt(bcdev);
			break;
		case BATT_CONSTANT_CURRENT:
			pst->prop[prop_id] = qti_get_batt_constant_curr(bcdev);
			break;
		case BATT_TEMP:
			rc = get_fg_temp(&pst->prop[prop_id]);
			break;
		default:
			break;
		}
	} else {
		rc = read_property_id(bcdev, pst, prop_id);
		if (rc < 0)
			return rc;
	}
#else
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;
#endif

	switch (prop) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		pval->strval = pst->model;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (bcdev->fake_soc >= 0 && bcdev->fake_soc <= 100)
			pval->intval = bcdev->fake_soc;
		else
			pval->intval = DIV_ROUND_CLOSEST(pst->prop[prop_id], 100);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		pval->intval = DIV_ROUND_CLOSEST((int)pst->prop[prop_id], 10);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		pval->intval = bcdev->curr_thermal_level;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		pval->intval = bcdev->num_thermal_levels;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		pval->intval = pst->prop[prop_id];
		if (pval->intval == POWER_SUPPLY_STATUS_CHARGING &&
			bcdev->report_power_absent)
			pval->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		pval->intval = pst->prop[prop_id];
		break;
	}

	return rc;
}

static int battery_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst =&bcdev->psy_list[PSY_TYPE_BATTERY];
	int prop_id, rc = 0;

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		return battery_psy_set_charge_current(bcdev, pval->intval);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		rc = battery_psy_set_fcc(bcdev, prop_id, pval->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int battery_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		return 1;
	default:
		break;
	}

	return 0;
}

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
};

static int power_supply_read_temp(struct thermal_zone_device *tzd,
		int *temp)
{
	struct power_supply *psy;
	struct battery_chg_dev *bcdev = NULL;
	struct psy_state *pst = NULL;
	int rc = 0, batt_temp;
	static int last_temp = 0;
	ktime_t time_now;
	static ktime_t last_read_time;
	s64 delta;

	WARN_ON(tzd == NULL);
	psy = tzd->devdata;
	bcdev = power_supply_get_drvdata(psy);
	pst = &bcdev->psy_list[PSY_TYPE_XM];

	time_now = ktime_get();
	delta = ktime_ms_delta(time_now, last_read_time);


	batt_temp = pst->prop[XM_PROP_THERMAL_TEMP];
	last_read_time = time_now;

	*temp = batt_temp * 100;
	if (batt_temp!= last_temp) {
		last_temp = batt_temp;
		pr_err("batt_thermal temp:%d ,delta:%ld rc=%d\n",batt_temp,delta, rc);
	}
	return 0;
}

static struct thermal_zone_device_ops psy_tzd_ops = {
	.get_temp = power_supply_read_temp,
};
static const struct power_supply_desc batt_psy_desc = {
	.name			= "battery",
	.no_thermal		= true,
	.type			= POWER_SUPPLY_TYPE_BATTERY,
	.properties		= battery_props,
	.num_properties		= ARRAY_SIZE(battery_props),
	.get_property		= battery_psy_get_prop,
	.set_property		= battery_psy_set_prop,
	.property_is_writeable	= battery_psy_prop_is_writeable,
};

static int battery_chg_init_psy(struct battery_chg_dev *bcdev)
{
	struct power_supply_config psy_cfg = {};
	int rc;
	struct power_supply *psy;

	psy_cfg.drv_data = bcdev;
	psy_cfg.of_node = bcdev->dev->of_node;

	bcdev->psy_list[PSY_TYPE_BATTERY].psy =
		devm_power_supply_register(bcdev->dev, &batt_psy_desc,
						&psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_BATTERY].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_BATTERY].psy);
		bcdev->psy_list[PSY_TYPE_USB].psy = NULL;
		return rc;
	}

	psy = bcdev->psy_list[PSY_TYPE_BATTERY].psy;
	psy->tzd = thermal_zone_device_register(psy->desc->name,
					0, 0, psy, &psy_tzd_ops, NULL, 0, 0);

	bcdev->psy_list[PSY_TYPE_USB].psy =
		devm_power_supply_register(bcdev->dev, &usb_psy_desc, &psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_USB].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_USB].psy);
		bcdev->psy_list[PSY_TYPE_USB].psy = NULL;
		return rc;
	}

	bcdev->psy_list[PSY_TYPE_WLS].psy =
		devm_power_supply_register(bcdev->dev, &wls_psy_desc, &psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy);
		bcdev->psy_list[PSY_TYPE_WLS].psy = NULL;
		return rc;
	}

	return 0;
}

static void battery_chg_subsys_up_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, subsys_up_work);
	int rc;

	battery_chg_notify_enable(bcdev);

	msleep(200);

	if (bcdev->last_fcc_ua) {
		rc = __battery_psy_set_charge_current(bcdev,
				bcdev->last_fcc_ua);
		if (rc < 0)
			pr_err("Failed to set FCC (%u uA), rc=%d\n",
				bcdev->last_fcc_ua, rc);
	}

	if (bcdev->usb_icl_ua) {
		rc = usb_psy_set_icl(bcdev, USB_INPUT_CURR_LIMIT,
				bcdev->usb_icl_ua);
		if (rc < 0)
			pr_err("Failed to set ICL(%u uA), rc=%d\n",
				bcdev->usb_icl_ua, rc);
	}
}

static int wireless_fw_send_firmware(struct battery_chg_dev *bcdev,
					const struct firmware *fw)
{
	struct wireless_fw_push_buf_req msg = {};
	const u8 *ptr;
	u32 i, num_chunks, partial_chunk_size;
	int rc;

	num_chunks = fw->size / WLS_FW_BUF_SIZE;
	partial_chunk_size = fw->size % WLS_FW_BUF_SIZE;

	if (!num_chunks)
		return -EINVAL;

	ptr = fw->data;
	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_WLS_FW_PUSH_BUF_REQ;

	for (i = 0; i < num_chunks; i++, ptr += WLS_FW_BUF_SIZE) {
		msg.fw_chunk_id = i + 1;
		memcpy(msg.buf, ptr, WLS_FW_BUF_SIZE);

		rc = battery_chg_fw_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			return rc;
	}

	if (partial_chunk_size) {
		msg.fw_chunk_id = i + 1;
		memset(msg.buf, 0, WLS_FW_BUF_SIZE);
		memcpy(msg.buf, ptr, partial_chunk_size);

		rc = battery_chg_fw_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int wireless_fw_check_for_update(struct battery_chg_dev *bcdev,
					u32 version, size_t size)
{
	struct wireless_fw_check_req req_msg = {};

	bcdev->wls_fw_update_reqd = false;

	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = BC_WLS_FW_CHECK_UPDATE;
	req_msg.fw_version = version;
	req_msg.fw_size = size;
	req_msg.fw_crc = bcdev->wls_fw_crc;

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

#define IDT9415_FW_MAJOR_VER_OFFSET		0x84
#define IDT9415_FW_MINOR_VER_OFFSET		0x86
#define IDT_FW_MAJOR_VER_OFFSET		0x94
#define IDT_FW_MINOR_VER_OFFSET		0x96
static int wireless_fw_update(struct battery_chg_dev *bcdev, bool force)
{
	const struct firmware *fw;
	struct psy_state *pst;
	u32 version;
	u16 maj_ver, min_ver;
	int rc;

	if (!bcdev->wls_fw_name) {
		return -EINVAL;
	}

	pm_stay_awake(bcdev->dev);

	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_ONLINE);
	if (rc < 0)
		goto out;

	if (!pst->prop[USB_ONLINE]) {
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
		rc = read_property_id(bcdev, pst, BATT_CAPACITY);
		if (rc < 0)
			goto out;

		if ((pst->prop[BATT_CAPACITY] / 100) < 50) {
			rc = -EINVAL;
			goto out;
		}
	}

	rc = firmware_request_nowarn(&fw, bcdev->wls_fw_name, bcdev->dev);
	if (rc) {
		goto out;
	}

	if (!fw || !fw->data || !fw->size) {
		rc = -EINVAL;
		goto release_fw;
	}

	if (fw->size < SZ_16K) {
		rc = -EINVAL;
		goto release_fw;
	}

	if (strstr(bcdev->wls_fw_name, "9412")) {
		maj_ver = le16_to_cpu(*(__le16 *)(fw->data + IDT_FW_MAJOR_VER_OFFSET));
		min_ver = le16_to_cpu(*(__le16 *)(fw->data + IDT_FW_MINOR_VER_OFFSET));
	} else {
		maj_ver = le16_to_cpu(*(__le16 *)(fw->data + IDT9415_FW_MAJOR_VER_OFFSET));
		min_ver = le16_to_cpu(*(__le16 *)(fw->data + IDT9415_FW_MINOR_VER_OFFSET));
	}
	version = maj_ver << 16 | min_ver;

	if (force)
		version = UINT_MAX;

	rc = wireless_fw_check_for_update(bcdev, version, fw->size);
	if (rc < 0) {
		goto release_fw;
	}

	if (!bcdev->wls_fw_update_reqd) {
		goto release_fw;
	}

	msleep(WLS_FW_PREPARE_TIME_MS);

	reinit_completion(&bcdev->fw_update_ack);
	rc = wireless_fw_send_firmware(bcdev, fw);
	if (rc < 0) {
		goto release_fw;
	}

	rc = wait_for_completion_timeout(&bcdev->fw_update_ack,
				msecs_to_jiffies(bcdev->wls_fw_update_time_ms));
	if (!rc) {
		rc = -ETIMEDOUT;
		goto release_fw;
	} else {
		rc = 0;
	}

release_fw:
	bcdev->wls_fw_crc = 0;
	release_firmware(fw);
out:
	pm_relax(bcdev->dev);

	return rc;
}

static ssize_t wireless_fw_update_time_ms_store(struct class *c,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	if (kstrtou32(buf, 0, &bcdev->wls_fw_update_time_ms))
		return -EINVAL;

	return count;
}

static ssize_t wireless_fw_update_time_ms_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%u\n", bcdev->wls_fw_update_time_ms);
}
static CLASS_ATTR_RW(wireless_fw_update_time_ms);

static ssize_t wireless_fw_crc_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	u16 val;

	if (kstrtou16(buf, 0, &val) || !val)
		return -EINVAL;

	bcdev->wls_fw_crc = val;

	return count;
}
static CLASS_ATTR_WO(wireless_fw_crc);

static ssize_t wireless_fw_version_show(struct class *c,
					struct class_attribute *attr,
					char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct wireless_fw_get_version_req req_msg = {};
	int rc;

	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = BC_WLS_FW_GET_VERSION;

	rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
	if (rc < 0) {
		return rc;
	}

	return scnprintf(buf, PAGE_SIZE, "%#x\n", bcdev->wls_fw_version);
}
static CLASS_ATTR_RO(wireless_fw_version);

static ssize_t wireless_fw_force_update_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	bool val;
	int rc;

	if (kstrtobool(buf, &val) || !val)
		return -EINVAL;

	rc = wireless_fw_update(bcdev, true);
	if (rc < 0)
		return rc;

	return count;
}
static CLASS_ATTR_WO(wireless_fw_force_update);

static ssize_t wireless_fw_update_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	bool val;
	int rc;

	if (kstrtobool(buf, &val) || !val)
		return -EINVAL;

	rc = wireless_fw_update(bcdev, false);
	if (rc < 0)
		return rc;

	return count;
}
static CLASS_ATTR_WO(wireless_fw_update);

static ssize_t wireless_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_WLS];
	int rc;

	rc = read_property_id(bcdev, pst, WLS_ADAP_TYPE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			get_wls_type_name(pst->prop[WLS_ADAP_TYPE]));
}
static CLASS_ATTR_RO(wireless_type);

static ssize_t usb_typec_compliant_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_TYPEC_COMPLIANT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			(int)pst->prop[USB_TYPEC_COMPLIANT]);
}
static CLASS_ATTR_RO(usb_typec_compliant);

static ssize_t usb_real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_REAL_TYPE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			get_usb_type_name(pst->prop[USB_REAL_TYPE]));
}
static CLASS_ATTR_RO(usb_real_type);

static ssize_t restrict_cur_store(struct class *c, struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	u32 fcc_ua, prev_fcc_ua;

	if (kstrtou32(buf, 0, &fcc_ua) || fcc_ua > bcdev->thermal_fcc_ua)
		return -EINVAL;

	prev_fcc_ua = bcdev->restrict_fcc_ua;
	bcdev->restrict_fcc_ua = fcc_ua;
	if (bcdev->restrict_chg_en) {
		rc = __battery_psy_set_charge_current(bcdev, fcc_ua);
		if (rc < 0) {
			bcdev->restrict_fcc_ua = prev_fcc_ua;
			return rc;
		}
	}

	return count;
}

static ssize_t restrict_cur_show(struct class *c, struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%u\n", bcdev->restrict_fcc_ua);
}
static CLASS_ATTR_RW(restrict_cur);

static ssize_t restrict_chg_store(struct class *c, struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	bcdev->restrict_chg_en = val;
	rc = __battery_psy_set_charge_current(bcdev, bcdev->restrict_chg_en ?
			bcdev->restrict_fcc_ua : bcdev->thermal_fcc_ua);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t restrict_chg_show(struct class *c, struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->restrict_chg_en);
}
static CLASS_ATTR_RW(restrict_chg);

static ssize_t fake_soc_store(struct class *c, struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	bcdev->fake_soc = val;

	if (pst->psy)
		power_supply_changed(pst->psy);

	return count;
}

static ssize_t fake_soc_show(struct class *c, struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->fake_soc);
}
static CLASS_ATTR_RW(fake_soc);

static ssize_t wireless_boost_en_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_WLS],
				WLS_BOOST_EN, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t wireless_boost_en_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_WLS];
	int rc;

	rc = read_property_id(bcdev, pst, WLS_BOOST_EN);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[WLS_BOOST_EN]);
}
static CLASS_ATTR_RW(wireless_boost_en);

static ssize_t moisture_detection_en_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_USB],
				USB_MOISTURE_DET_EN, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t moisture_detection_en_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_MOISTURE_DET_EN);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			pst->prop[USB_MOISTURE_DET_EN]);
}
static CLASS_ATTR_RW(moisture_detection_en);

static ssize_t moisture_detection_status_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_MOISTURE_DET_STS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			pst->prop[USB_MOISTURE_DET_STS]);
}
static CLASS_ATTR_RO(moisture_detection_status);

static ssize_t resistance_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int rc;

	rc = read_property_id(bcdev, pst, BATT_RESISTANCE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[BATT_RESISTANCE]);
}
static CLASS_ATTR_RO(resistance);

static ssize_t soh_show(struct class *c, struct class_attribute *attr,
			char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int rc;

	rc = read_property_id(bcdev, pst, BATT_SOH);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[BATT_SOH]);
}
static CLASS_ATTR_RO(soh);

static ssize_t ship_mode_en_store(struct class *c, struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	struct battery_charger_ship_mode_req_msg msg = { { 0 } };
	int rc = 0;

	if (kstrtobool(buf, &bcdev->ship_mode_en))
		return -EINVAL;

	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_SHIP_MODE_REQ_SET;
	msg.ship_mode_type = SHIP_MODE_PMIC;
	rc = battery_chg_write(bcdev, &msg, sizeof(msg));
	if (rc < 0)
		pr_err("Failed to write ship mode: %d\n", rc);

	return count;
}

static ssize_t ship_mode_en_show(struct class *c, struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->ship_mode_en);
}
static CLASS_ATTR_RW(ship_mode_en);

static ssize_t bq2597x_chip_ok_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BQ2597X_CHIP_OK);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_BQ2597X_CHIP_OK]);
}
static CLASS_ATTR_RO(bq2597x_chip_ok);

static ssize_t bq2597x_slave_chip_ok_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BQ2597X_SLAVE_CHIP_OK);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_BQ2597X_SLAVE_CHIP_OK]);
}
static CLASS_ATTR_RO(bq2597x_slave_chip_ok);

static ssize_t bq2597x_bus_current_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BQ2597X_BUS_CURRENT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_BQ2597X_BUS_CURRENT]);
}
static CLASS_ATTR_RO(bq2597x_bus_current);

static ssize_t bq2597x_slave_bus_current_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BQ2597X_SLAVE_BUS_CURRENT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_BQ2597X_SLAVE_BUS_CURRENT]);
}
static CLASS_ATTR_RO(bq2597x_slave_bus_current);

static ssize_t bq2597x_bus_delta_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BQ2597X_BUS_DELTA);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_BQ2597X_BUS_DELTA]);
}
static CLASS_ATTR_RO(bq2597x_bus_delta);

static ssize_t bq2597x_bus_voltage_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BQ2597X_BUS_VOLTAGE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_BQ2597X_BUS_VOLTAGE]);
}
static CLASS_ATTR_RO(bq2597x_bus_voltage);

static ssize_t bq2597x_battery_present_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BQ2597X_BATTERY_PRESENT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_BQ2597X_BATTERY_PRESENT]);
}
static CLASS_ATTR_RO(bq2597x_battery_present);

static ssize_t bq2597x_slave_battery_present_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BQ2597X_SLAVE_BATTERY_PRESENT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_BQ2597X_SLAVE_BATTERY_PRESENT]);
}
static CLASS_ATTR_RO(bq2597x_slave_battery_present);

static ssize_t bq2597x_battery_voltage_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BQ2597X_BATTERY_VOLTAGE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_BQ2597X_BATTERY_VOLTAGE]);
}
static CLASS_ATTR_RO(bq2597x_battery_voltage);

static ssize_t real_type_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

#ifdef CONFIG_QTI_POGO_CHG
	if (g_battmngr_noti && pogo_flag) {
		if (!bcdev->battmg_dev) {
			bcdev->battmg_dev = check_nano_ops();
		}
		if (bcdev->battmg_dev) {
			pst->prop[XM_PROP_REAL_TYPE] = battmngr_noops_get_real_type(bcdev->battmg_dev);
		}
	} else {
		rc = read_property_id(bcdev, pst, XM_PROP_REAL_TYPE);
		if (rc < 0)
			return rc;
	}
#else
	rc = read_property_id(bcdev, pst, XM_PROP_REAL_TYPE);
	if (rc < 0)
		return rc;
#endif

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(pst->prop[XM_PROP_REAL_TYPE]));
}
static CLASS_ATTR_RO(real_type);

static ssize_t battcont_online_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_BATT_CONNT_ONLINE);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_BATT_CONNT_ONLINE]);
}
static CLASS_ATTR_RO(battcont_online);

static ssize_t connector_temp_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
						XM_PROP_CONNECTOR_TEMP, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t connector_temp_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CONNECTOR_TEMP);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d", pst->prop[XM_PROP_CONNECTOR_TEMP]);
}
static CLASS_ATTR_RW(connector_temp);

static ssize_t authentic_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	bcdev->battery_auth = val;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
						XM_PROP_AUTHENTIC, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t authentic_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_AUTHENTIC);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_AUTHENTIC]);
}
static CLASS_ATTR_RW(authentic);

static ssize_t bap_match_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
						XM_PROP_BATTERY_ADAPT_POWER_MATCH, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t bap_match_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BATTERY_ADAPT_POWER_MATCH);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_BATTERY_ADAPT_POWER_MATCH]);
}
static CLASS_ATTR_RW(bap_match);

static int write_verify_digest_prop_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id, u8* buff)
{
	struct xm_verify_digest_resp_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;
	req_msg.slave_fg = bcdev->slave_fg_verify_flag;
	memcpy(req_msg.digest, buff, BATTERY_DIGEST_LEN);

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static int read_verify_digest_property_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id)
{
	struct xm_verify_digest_resp_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_get;
	req_msg.slave_fg = bcdev->slave_fg_verify_flag;

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static ssize_t verify_slave_flag_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	bcdev->slave_fg_verify_flag = val;

	return count;
}

static ssize_t verify_slave_flag_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%u\n", bcdev->slave_fg_verify_flag);
}
static CLASS_ATTR_RW(verify_slave_flag);


static ssize_t verify_digest_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	u8 random_1s[BATTERY_DIGEST_LEN + 1] = {0};
	char kbuf_1s[70] = {0};
	int rc;
	int i;


	memset(kbuf_1s, 0, sizeof(kbuf_1s));
	strncpy(kbuf_1s, buf, count - 1);
	StringToHex(kbuf_1s, random_1s, &i);
	rc = write_verify_digest_prop_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
			XM_PROP_VERIFY_DIGEST, random_1s);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t verify_digest_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	u8 digest_buf[4];
	int i;
	int len;

	rc = read_verify_digest_property_id(bcdev, pst, XM_PROP_VERIFY_DIGEST);
	if (rc < 0)
		return rc;

	for (i = 0; i < BATTERY_DIGEST_LEN; i++) {
		memset(digest_buf, 0, sizeof(digest_buf));
		snprintf(digest_buf, sizeof(digest_buf) - 1, "%02x", bcdev->digest[i]);
		strlcat(buf, digest_buf, BATTERY_DIGEST_LEN * 2 + 1);
	}
	len = strlen(buf);
	buf[len] = '\0';
	return strlen(buf) + 1;
}
static CLASS_ATTR_RW(verify_digest);

static ssize_t chip_ok_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CHIP_OK);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CHIP_OK]);
}
static CLASS_ATTR_RO(chip_ok);

static ssize_t resistance_id_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_RESISTANCE_ID);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_RESISTANCE_ID]);
}
static CLASS_ATTR_RO(resistance_id);

static ssize_t input_suspend_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	int rc = 0;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

#ifdef CONFIG_QTI_POGO_CHG
	if (g_battmngr_noti) {
		if (!bcdev->usb_icl_votable)
			bcdev->usb_icl_votable = find_votable("ICL");

		if (bcdev->usb_icl_votable) {
			rc = vote(bcdev->usb_icl_votable, USER_VOTER, val, 0);
			if (rc < 0)
				pr_err("Couldn't vote to %s USB rc=%d", val ? "suspend":"resume", rc);
		}
		qti_deal_report();
		bcdev->input_suspend = val;
	}

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
		XM_PROP_INPUT_SUSPEND, val);
	if (rc < 0)
		return rc;
#else
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
						XM_PROP_INPUT_SUSPEND, val);
	if (rc < 0)
		return rc;
#endif

	return count;
}

static ssize_t input_suspend_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

#ifdef CONFIG_QTI_POGO_CHG
	if (g_battmngr_noti && pogo_flag) {
		if (bcdev->usb_icl_votable)
			pst->prop[XM_PROP_INPUT_SUSPEND]
						= (get_client_vote(bcdev->usb_icl_votable, USER_VOTER) == 0);
	} else {
		rc = read_property_id(bcdev, pst, XM_PROP_INPUT_SUSPEND);
		if (rc < 0)
			return rc;
	}
#else
	rc = read_property_id(bcdev, pst, XM_PROP_INPUT_SUSPEND);
	if (rc < 0)
		return rc;
#endif

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_INPUT_SUSPEND]);
}
static CLASS_ATTR_RW(input_suspend);

static ssize_t fastchg_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FASTCHGMODE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FASTCHGMODE]);
}
static CLASS_ATTR_RO(fastchg_mode);

static ssize_t cc_orientation_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CC_ORIENTATION);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CC_ORIENTATION]);
}
static CLASS_ATTR_RO(cc_orientation);


static ssize_t thermal_remove_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_THERMAL_REMOVE, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t thermal_remove_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_THERMAL_REMOVE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_THERMAL_REMOVE]);
}
static CLASS_ATTR_RW(thermal_remove);


static ssize_t typec_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_TYPEC_MODE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%s\n", power_supply_usbc_text[pst->prop[XM_PROP_TYPEC_MODE]]);
}
static CLASS_ATTR_RO(typec_mode);

static ssize_t mtbf_current_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	bcdev->mtbf_current = val;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_MTBF_CURRENT, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t mtbf_current_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_MTBF_CURRENT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_MTBF_CURRENT]);
}
static CLASS_ATTR_RW(mtbf_current);

static ssize_t fake_temp_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FAKE_TEMP, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t fake_temp_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FAKE_TEMP);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FAKE_TEMP]);
}
static CLASS_ATTR_RW(fake_temp);

/*test*/
static ssize_t fake_cycle_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
		struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
												battery_class);
		int rc;
		int val;
		if(kstrtoint(buf, 10, &val))
					return -EINVAL;
			rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
									XM_PROP_FAKE_CYCLE, val);
			if(rc < 0)
					return rc;
			return count;
}
static ssize_t fake_cycle_show(struct class *c,
                                        struct class_attribute *attr, char *buf)
{
        struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                                battery_class);
        struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
        int rc;

        rc = read_property_id(bcdev, pst, XM_PROP_FAKE_CYCLE);
        if (rc < 0)
                return rc;

        return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FAKE_CYCLE]);
}
static CLASS_ATTR_RW(fake_cycle);

static ssize_t afp_temp_show(struct class *c,
                                        struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
											battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_AFP_TEMP);
	if (rc < 0)
			return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_AFP_TEMP]);
}
static CLASS_ATTR_RO(afp_temp);

static ssize_t plate_shock_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
		struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
												battery_class);
		int rc;
		int val;
		if(kstrtoint(buf, 10, &val))
					return -EINVAL;
			rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
									XM_PROP_PLATE_SHOCK, val);
			if(rc < 0)
					return rc;
			return count;
}
static ssize_t plate_shock_show(struct class *c,
                                        struct class_attribute *attr, char *buf)
{
        struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                                battery_class);
        struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
        int rc;

        rc = read_property_id(bcdev, pst, XM_PROP_PLATE_SHOCK);
        if (rc < 0)
                return rc;

        return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_PLATE_SHOCK]);
}
static CLASS_ATTR_RW(plate_shock);

static ssize_t shutdown_delay_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SHUTDOWN_DELAY);
	if (rc < 0)
		return rc;

	if (!bcdev->shutdown_delay_en ||
	    (bcdev->fake_soc >= 0 && bcdev->fake_soc <= 100))
		pst->prop[XM_PROP_SHUTDOWN_DELAY] = 0;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_SHUTDOWN_DELAY]);
}

static ssize_t shutdown_delay_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	bcdev->shutdown_delay_en = val;

	return count;
}

static CLASS_ATTR_RW(shutdown_delay);

static ssize_t cc_short_vbus_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CC_SHORT_VBUS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_CC_SHORT_VBUS]);
}

static ssize_t cc_short_vbus_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int val;
	int rc;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_CC_SHORT_VBUS, val);
	if (rc < 0)
		return rc;

	return count;
}

static CLASS_ATTR_RW(cc_short_vbus);

static ssize_t shipmode_count_reset_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_SHIPMODE_COUNT_RESET, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t shipmode_count_reset_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SHIPMODE_COUNT_RESET);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SHIPMODE_COUNT_RESET]);
}
static CLASS_ATTR_RW(shipmode_count_reset);

static ssize_t otg_ui_support_show(struct class *c,
                                        struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
											battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_OTG_UI_SUPPORT);
	if (rc < 0)
			return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_OTG_UI_SUPPORT]);
}
static CLASS_ATTR_RO(otg_ui_support);

static ssize_t cid_status_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CID_STATUS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_CID_STATUS]);
}
static CLASS_ATTR_RO(cid_status);

static ssize_t cc_toggle_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_CC_TOGGLE, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t cc_toggle_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CC_TOGGLE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_CC_TOGGLE]);
}
static CLASS_ATTR_RW(cc_toggle);

static ssize_t smart_chg_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	u32 val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_SMART_CHG, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t smart_chg_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SMART_CHG);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SMART_CHG]);
}

static CLASS_ATTR_RW(smart_chg);


#if defined(CONFIG_MI_WIRELESS)
static int write_wls_bin_prop_id(struct battery_chg_dev *bcdev, struct psy_state *pst,
			u32 prop_id, u16 total_length, u8 serial_number, u8 fw_area, u8* buff)
{
	struct xm_set_wls_bin_req_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;
	req_msg.total_length = total_length;
	req_msg.serial_number = serial_number;
	req_msg.fw_area = fw_area;
	if(serial_number < total_length/MAX_STR_LEN)
		memcpy(req_msg.wls_fw_bin, buff, MAX_STR_LEN);
	else if(serial_number == total_length/MAX_STR_LEN)
		memcpy(req_msg.wls_fw_bin, buff, total_length - serial_number*MAX_STR_LEN);

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static ssize_t wls_bin_store(struct class *c,
			struct class_attribute *attr,
			const char *buf, size_t count)
{

	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
				battery_class);
	int rc, retry, tmp_serial;
	static u16 total_length = 0;
	static u8 serial_number = 0;
	static u8 fw_area = 0;

	if( strncmp("length:", buf, 7 ) == 0 ) {
		if (kstrtou16( buf+7, 10, &total_length))
		      return -EINVAL;
		serial_number = 0;
	} else if( strncmp("area:", buf, 5 ) == 0 ) {
		if (kstrtou8( buf+5, 10, &fw_area))
		      return -EINVAL;
	}else {
		for( tmp_serial=0;
			(tmp_serial<(count+MAX_STR_LEN-1)/MAX_STR_LEN) && (serial_number<(total_length+MAX_STR_LEN-1)/MAX_STR_LEN);
			++tmp_serial,++serial_number)
		{
			for(retry = 0; retry < 3; ++retry )
			{
				rc = write_wls_bin_prop_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
							XM_PROP_WLS_BIN,
							total_length,
							serial_number,
							fw_area,
							(u8 *)buf+tmp_serial*MAX_STR_LEN);
				if (rc == 0)
				      break;
			}
		}
	}
	return count;
}
static CLASS_ATTR_WO(wls_bin);

static ssize_t wireless_chip_fw_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	struct wireless_chip_fw_msg req_msg = { { 0 } };
	int rc;

	req_msg.property_id = XM_PROP_FW_VER;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_get;

	rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
	if (rc < 0)
		return -EINVAL;

	return scnprintf(buf, PAGE_SIZE, "%s\n", bcdev->wireless_chip_fw_version);
}

static ssize_t wireless_chip_fw_store(struct class *c,
		 struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	struct wireless_chip_fw_msg req_msg = { { 0 } };
	int rc;
	u32 val;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	req_msg.property_id = XM_PROP_FW_VER;
	req_msg.value = val;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;

	rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
	if (rc < 0)
		return rc;

	return count;
}
static CLASS_ATTR_RW(wireless_chip_fw);

static ssize_t wireless_tx_uuid_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev =
		container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	struct wireless_tx_uuid_msg req_msg = { { 0 } };
	int rc;

	req_msg.property_id = XM_PROP_TX_UUID;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_get;

	rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
	if (rc < 0)
		return -EINVAL;

	return scnprintf(buf, PAGE_SIZE, "%s\n", bcdev->wireless_tx_uuid_version);
}
static CLASS_ATTR_RO(wireless_tx_uuid);

static ssize_t wireless_version_forcit_show(struct class *c,
                struct class_attribute *attr, char *buf)
{
    struct battery_chg_dev *bcdev =
            container_of(c, struct battery_chg_dev, battery_class);
    char *buffer = NULL;
    buffer = (char*)get_zeroed_page(GFP_KERNEL);
    if(!buffer)
        return 0;

    wireless_chip_fw_show(&(bcdev->battery_class), NULL, buffer);
    while (strncmp(buffer, "updating", strlen("updating")) == 0) {
        msleep(200);
        wireless_chip_fw_show(&(bcdev->battery_class), NULL, buffer);
    }
    if(strncmp(buffer, "00.00.00.00", strlen("00.00.00.00")) == 0
        || strstr(buffer, "fe") != 0) {
        return scnprintf(buf, PAGE_SIZE, "%s\n", "fail");
    } else {
        return scnprintf(buf, PAGE_SIZE, "%s\n", "pass");
    }
}
static CLASS_ATTR_RO(wireless_version_forcit);

static ssize_t wls_debug_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
							battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	struct chg_debug_msg req_msg = { { 0 } };
	int rc;

	req_msg.property_id = XM_PROP_CHG_DEBUG;
	req_msg.type = CHG_WLS_DEBUG;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;

	memset(req_msg.data, '\0', sizeof(req_msg.data));
	strncpy(req_msg.data, buf, count);

	rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
	if (rc < 0)
		return rc;
	return count;
}

static ssize_t wls_debug_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	struct chg_debug_msg req_msg = { { 0 } };
	int rc;

	req_msg.property_id = XM_PROP_CHG_DEBUG;
	req_msg.type = CHG_WLS_DEBUG;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_get;

	rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%s", bcdev->wls_debug_data);
}
static CLASS_ATTR_RW(wls_debug);

static ssize_t wls_fw_state_show(struct class *c,
			struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
				battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_WLS_FW_STATE);
	if (rc < 0)
	      return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_WLS_FW_STATE]);
}
static CLASS_ATTR_RO(wls_fw_state);

static ssize_t wls_car_adapter_show(struct class *c,
			struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
				battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_WLS_CAR_ADAPTER);
	if (rc < 0)
	      return rc;
	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_WLS_CAR_ADAPTER]);
}
static CLASS_ATTR_RO(wls_car_adapter);

static ssize_t wls_tx_speed_store(struct class *c,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
				battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
	      return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_WLS_TX_SPEED, val);
	if (rc < 0)
	      return rc;
	return count;
}
static ssize_t wls_tx_speed_show(struct class *c,
			struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
				battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_WLS_TX_SPEED);
	if (rc < 0)
	      return rc;
	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_WLS_TX_SPEED]);
}
static CLASS_ATTR_RW(wls_tx_speed);

static ssize_t wls_fc_flag_show(struct class *c,
                       struct class_attribute *attr, char *buf)
{
       struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                               battery_class);
       struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
       int rc;
       rc = read_property_id(bcdev, pst, XM_PROP_WLS_FC_FLAG);
       if (rc < 0)
             return rc;
       return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_WLS_FC_FLAG]);
}
static CLASS_ATTR_RO(wls_fc_flag);

static ssize_t tx_mac_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	u64 value = 0;

	rc = read_property_id(bcdev, pst, XM_PROP_TX_MACL);
	if (rc < 0)
		return rc;

	rc = read_property_id(bcdev, pst, XM_PROP_TX_MACH);
	if (rc < 0)
		return rc;
	value = pst->prop[XM_PROP_TX_MACH];
	value = (value << 32) + pst->prop[XM_PROP_TX_MACL];

	return scnprintf(buf, PAGE_SIZE, "%llx", value);
}
static CLASS_ATTR_RO(tx_mac);

static ssize_t rx_cr_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	u64 value = 0;

	rc = read_property_id(bcdev, pst, XM_PROP_RX_CRL);
	if (rc < 0)
		return rc;

	rc = read_property_id(bcdev, pst, XM_PROP_RX_CRH);
	if (rc < 0)
		return rc;
	value = pst->prop[XM_PROP_RX_CRH];
	value = (value << 32) + pst->prop[XM_PROP_RX_CRL];

	return scnprintf(buf, PAGE_SIZE, "%llx", value);
}
static CLASS_ATTR_RO(rx_cr);

static ssize_t rx_cep_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_RX_CEP);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%x", pst->prop[XM_PROP_RX_CEP]);
}
static CLASS_ATTR_RO(rx_cep);

static ssize_t bt_state_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_BT_STATE, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t bt_state_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BT_STATE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_BT_STATE]);
}
static CLASS_ATTR_RW(bt_state);

static ssize_t wlscharge_control_limit_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if(val == bcdev->curr_wlsthermal_level)
	      return count;

	if (bcdev->num_thermal_levels <= 0) {
		return -EINVAL;
	}

	if (val < 0 || val >= bcdev->num_thermal_levels)
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_WLSCHARGE_CONTROL_LIMIT, val);
	if (rc < 0)
		return rc;

	bcdev->curr_wlsthermal_level = val;

	return count;
}

static ssize_t wlscharge_control_limit_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_WLSCHARGE_CONTROL_LIMIT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_WLSCHARGE_CONTROL_LIMIT]);
}
static CLASS_ATTR_RW(wlscharge_control_limit);

#ifndef CONFIG_WIRELESS_REVERSE_CLOSE
static ssize_t reverse_chg_mode_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	bcdev->boost_mode = val;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_REVERSE_CHG_MODE, val);
	if (rc < 0)
		return rc;
	return count;
}

static ssize_t reverse_chg_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_REVERSE_CHG_MODE);
	if (rc < 0)
		goto out;

	if (bcdev->reverse_chg_flag != pst->prop[XM_PROP_REVERSE_CHG_MODE]) {
		if (pst->prop[XM_PROP_REVERSE_CHG_MODE]) {
			pm_stay_awake(bcdev->dev);
			dev_info(bcdev->dev, "reverse chg add lock\n");
		}
		else {
			pm_relax(bcdev->dev);
			dev_info(bcdev->dev, "reverse chg release lock\n");
		}
		bcdev->reverse_chg_flag = pst->prop[XM_PROP_REVERSE_CHG_MODE];
	}

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_REVERSE_CHG_MODE]);

out:
	dev_err(bcdev->dev, "read reverse chg mode error\n");
	bcdev->reverse_chg_flag = 0;
	pm_relax(bcdev->dev);
	return rc;
}
static CLASS_ATTR_RW(reverse_chg_mode);

static ssize_t reverse_chg_state_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_REVERSE_CHG_STATE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_REVERSE_CHG_STATE]);
}
static CLASS_ATTR_RO(reverse_chg_state);
#endif

static ssize_t rx_vout_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_RX_VOUT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_RX_VOUT]);
}
static CLASS_ATTR_RO(rx_vout);

static ssize_t rx_ss_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_RX_SS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_RX_SS]);
}
static CLASS_ATTR_RO(rx_ss);

static ssize_t tx_q_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_TX_Q);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_TX_Q]);
}
static CLASS_ATTR_RO(tx_q);

static ssize_t rx_offset_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, pst, XM_PROP_RX_OFFSET, val);

	if (rc < 0)
		return rc;

	return count;
}

static ssize_t rx_offset_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_RX_OFFSET);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_RX_OFFSET]);
}
static CLASS_ATTR_RW(rx_offset);

static ssize_t rx_vrect_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_RX_VRECT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_RX_VRECT]);
}
static CLASS_ATTR_RO(rx_vrect);

static ssize_t rx_iout_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_RX_IOUT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_RX_IOUT]);
}
static CLASS_ATTR_RO(rx_iout);

static ssize_t tx_adapter_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_TX_ADAPTER);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_TX_ADAPTER]);
}
static CLASS_ATTR_RO(tx_adapter);

static ssize_t op_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_OP_MODE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_OP_MODE]);
}
static CLASS_ATTR_RO(op_mode);


static ssize_t wls_die_temp_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_WLS_DIE_TEMP);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_WLS_DIE_TEMP]);
}
static CLASS_ATTR_RO(wls_die_temp);

static ssize_t wls_thermal_remove_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_WLS_THERMAL_REMOVE, val);
	if (rc < 0)
		return rc;
	return count;
}

static ssize_t wls_thermal_remove_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_WLS_THERMAL_REMOVE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_WLS_THERMAL_REMOVE]);
}
static CLASS_ATTR_RW(wls_thermal_remove);

// wls pen
#if defined(CONFIG_MI_PEN_WIRELESS)
static ssize_t pen_mac_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	u64 value = 0;
	rc = read_property_id(bcdev, pst, XM_PROP_PEN_MACL);
	if (rc < 0)
		return rc;
	rc = read_property_id(bcdev, pst, XM_PROP_PEN_MACH);
	if (rc < 0)
		return rc;
	value = pst->prop[XM_PROP_PEN_MACH];
	value = (value << 32) + pst->prop[XM_PROP_PEN_MACL];
	return scnprintf(buf, PAGE_SIZE, "%llx", value);
}
static CLASS_ATTR_RO(pen_mac);

static ssize_t tx_iout_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_TX_IOUT);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_TX_IOUT]);
}
static CLASS_ATTR_RO(tx_iout);

static ssize_t tx_vout_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_TX_VOUT);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_TX_VOUT]);
}
static CLASS_ATTR_RO(tx_vout);

static ssize_t pen_soc_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_PEN_SOC);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_PEN_SOC]);
}
static CLASS_ATTR_RO(pen_soc);

static ssize_t pen_hall3_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_PEN_HALL3);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d", pst->prop[XM_PROP_PEN_HALL3]);
}
static CLASS_ATTR_RO(pen_hall3);

static ssize_t pen_hall4_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_PEN_HALL4);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d", pst->prop[XM_PROP_PEN_HALL4]);
}
static CLASS_ATTR_RO(pen_hall4);

static ssize_t pen_tx_ss_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_PEN_TX_SS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d", pst->prop[XM_PROP_PEN_TX_SS]);
}
static CLASS_ATTR_RO(pen_tx_ss);

static ssize_t pen_place_err_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_PEN_PLACE_ERR);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d", pst->prop[XM_PROP_PEN_PLACE_ERR]);
}
static CLASS_ATTR_RO(pen_place_err);

static ssize_t fake_ss_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FAKE_SS, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t fake_ss_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FAKE_SS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FAKE_SS]);
}
static CLASS_ATTR_RW(fake_ss);
#endif	//CONFIG_MI_PEN_WIRELESS
#endif

/*test fuelgauge node*/
static ssize_t fg_vendor_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG_VENDOR_ID);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG_VENDOR_ID]);
}
static CLASS_ATTR_RO(fg_vendor);

static ssize_t fg1_qmax_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_QMAX);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_QMAX]);
}
static CLASS_ATTR_RO(fg1_qmax);

static ssize_t fg1_rm_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_RM);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_RM]);
}
static CLASS_ATTR_RO(fg1_rm);

static ssize_t fg1_fcc_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_FCC);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_FCC]);
}
static CLASS_ATTR_RO(fg1_fcc);

static ssize_t fg1_soh_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_SOH);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_SOH]);
}
static CLASS_ATTR_RO(fg1_soh);

static ssize_t fg1_rsoc_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_RSOC);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_RSOC]);
}
static CLASS_ATTR_RO(fg1_rsoc);

static ssize_t fg1_ai_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_AI);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_AI]);
}
static CLASS_ATTR_RO(fg1_ai);

static ssize_t fg1_fcc_soh_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_FCC_SOH);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_FCC_SOH]);
}
static CLASS_ATTR_RO(fg1_fcc_soh);

static ssize_t fg1_cycle_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_CYCLE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_CYCLE]);
}
static CLASS_ATTR_RO(fg1_cycle);

static ssize_t fg1_fastcharge_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_FAST_CHARGE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_FAST_CHARGE]);
}
static CLASS_ATTR_RO(fg1_fastcharge);

static ssize_t fg1_current_max_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_CURRENT_MAX);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_CURRENT_MAX]);
}
static CLASS_ATTR_RO(fg1_current_max);

static ssize_t fg1_vol_max_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_VOL_MAX);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_VOL_MAX]);
}
static CLASS_ATTR_RO(fg1_vol_max);

static ssize_t fg1_tsim_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_TSIM);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_TSIM]);
}
static CLASS_ATTR_RO(fg1_tsim);

static ssize_t fg1_tambient_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_TAMBIENT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_TAMBIENT]);
}
static CLASS_ATTR_RO(fg1_tambient);

static ssize_t fg1_tremq_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_TREMQ);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_TREMQ]);
}
static CLASS_ATTR_RO(fg1_tremq);

static ssize_t fg1_tfullq_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_TFULLQ);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_TFULLQ]);
}
static CLASS_ATTR_RO(fg1_tfullq);

static ssize_t fg1_temp_max_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_TEMP_MAX);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_TEMP_MAX]);
}
static CLASS_ATTR_RO(fg1_temp_max);

static ssize_t fg1_time_ot_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c,
			struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_TIME_OT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_TIME_OT]);
}
static CLASS_ATTR_RO(fg1_time_ot);

static ssize_t fg1_time_ht_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c,
			struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_TIME_HT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_TIME_HT]);
}
static CLASS_ATTR_RO(fg1_time_ht);

static ssize_t fg1_seal_set_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c,
			struct battery_chg_dev, battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	pr_err("seal set %d\n", val);

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FG1_SEAL_SET, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t fg1_seal_set_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c,
			struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_SEAL_SET);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG1_SEAL_SET]);
}
static CLASS_ATTR_RW(fg1_seal_set);

static ssize_t fg1_seal_state_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c,
			struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_SEAL_STATE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_SEAL_STATE]);
}
static CLASS_ATTR_RO(fg1_seal_state);

static ssize_t fg1_df_check_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c,
			struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_DF_CHECK);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_DF_CHECK]);
}
static CLASS_ATTR_RO(fg1_df_check);

/*************************end fuelgauge node *****************************/
#define BSWAP_32(x) \
	(u32)((((u32)(x) & 0xff000000) >> 24) | \
			(((u32)(x) & 0x00ff0000) >> 8) | \
			(((u32)(x) & 0x0000ff00) << 8) | \
			(((u32)(x) & 0x000000ff) << 24))

static void usbpd_sha256_bitswap32(unsigned int *array, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		array[i] = BSWAP_32(array[i]);
	}
}

static void usbpd_request_vdm_cmd(struct battery_chg_dev *bcdev, enum uvdm_state cmd, unsigned int *data)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	u32 prop_id, val = 0;
	int rc;

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		prop_id = XM_PROP_VDM_CMD_CHARGER_VERSION;
		break;
	case USBPD_UVDM_CHARGER_VOLTAGE:
		prop_id = XM_PROP_VDM_CMD_CHARGER_VOLTAGE;
		break;
	case USBPD_UVDM_CHARGER_TEMP:
		prop_id = XM_PROP_VDM_CMD_CHARGER_TEMP;
		break;
	case USBPD_UVDM_SESSION_SEED:
		prop_id = XM_PROP_VDM_CMD_SESSION_SEED;
		usbpd_sha256_bitswap32(data, USBPD_UVDM_SS_LEN);
		val = *data;
		break;
	case USBPD_UVDM_AUTHENTICATION:
		prop_id = XM_PROP_VDM_CMD_AUTHENTICATION;
		usbpd_sha256_bitswap32(data, USBPD_UVDM_SS_LEN);
		val = *data;
		break;
	case USBPD_UVDM_REVERSE_AUTHEN:
                prop_id = XM_PROP_VDM_CMD_REVERSE_AUTHEN;
                usbpd_sha256_bitswap32(data, USBPD_UVDM_SS_LEN);
                val = *data;
                break;
	case USBPD_UVDM_REMOVE_COMPENSATION:
		prop_id = XM_PROP_VDM_CMD_REMOVE_COMPENSATION;
		val = *data;
		break;
	case USBPD_UVDM_VERIFIED:
		prop_id = XM_PROP_VDM_CMD_VERIFIED;
		val = *data;
		break;
	default:
		prop_id = XM_PROP_VDM_CMD_CHARGER_VERSION;
		break;
	}

	if(cmd == USBPD_UVDM_SESSION_SEED || cmd == USBPD_UVDM_AUTHENTICATION || cmd == USBPD_UVDM_REVERSE_AUTHEN) {
		rc = write_ss_auth_prop_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				prop_id, data);
	}
	else
		rc = write_property_id(bcdev, pst, prop_id, val);
}


static ssize_t request_vdm_cmd_store(struct class *c,
					struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int cmd, ret;
	unsigned char buffer[64];
	unsigned char data[32];
	int ccount;

	ret = sscanf(buf, "%d,%s\n", &cmd, buffer);

	StringToHex(buffer, data, &ccount);
	usbpd_request_vdm_cmd(bcdev, cmd, (unsigned int *)data);
	return count;
}

static ssize_t request_vdm_cmd_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	u32 prop_id = 0;
	int i;
	char data[16], str_buf[128] = {0};
	enum uvdm_state cmd;

	rc = read_property_id(bcdev, pst, XM_PROP_UVDM_STATE);
	if (rc < 0)
		return rc;

	cmd = pst->prop[XM_PROP_UVDM_STATE];

	switch (cmd){
	  case USBPD_UVDM_CHARGER_VERSION:
	  	prop_id = XM_PROP_VDM_CMD_CHARGER_VERSION;
		rc = read_property_id(bcdev, pst, prop_id);
		return snprintf(buf, PAGE_SIZE, "%d,%d", cmd, pst->prop[prop_id]);
	  	break;
	  case USBPD_UVDM_CHARGER_TEMP:
	  	prop_id = XM_PROP_VDM_CMD_CHARGER_TEMP;
		rc = read_property_id(bcdev, pst, prop_id);
		return snprintf(buf, PAGE_SIZE, "%d,%d", cmd, pst->prop[prop_id]);
	  	break;
	  case USBPD_UVDM_CHARGER_VOLTAGE:
	  	prop_id = XM_PROP_VDM_CMD_CHARGER_VOLTAGE;
		rc = read_property_id(bcdev, pst, prop_id);
		return snprintf(buf, PAGE_SIZE, "%d,%d", cmd, pst->prop[prop_id]);
	  	break;
	  case USBPD_UVDM_CONNECT:
	  case USBPD_UVDM_DISCONNECT:
	  case USBPD_UVDM_SESSION_SEED:
	  case USBPD_UVDM_VERIFIED:
	  case USBPD_UVDM_REMOVE_COMPENSATION:
	  case USBPD_UVDM_REVERSE_AUTHEN:
	  	return snprintf(buf, PAGE_SIZE, "%d,Null", cmd);
	  	break;
	  case USBPD_UVDM_AUTHENTICATION:
	  	prop_id = XM_PROP_VDM_CMD_AUTHENTICATION;
		rc = read_ss_auth_property_id(bcdev, pst, prop_id);
		if (rc < 0)
			return rc;
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			memset(data, 0, sizeof(data));
			snprintf(data, sizeof(data), "%08lx", bcdev->ss_auth_data[i]);
			strlcat(str_buf, data, sizeof(str_buf));
		}
		return snprintf(buf, PAGE_SIZE, "%d,%s", cmd, str_buf);
	  	break;
	  default:
		break;
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[prop_id]);
}
static CLASS_ATTR_RW(request_vdm_cmd);

static const char * const usbpd_state_strings[] = {
	"UNKNOWN",
	"SNK_Startup",
	"SNK_Ready",
	"SRC_Ready",
};

static ssize_t current_state_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CURRENT_STATE);
	if (rc < 0)
		return rc;
	if (pst->prop[XM_PROP_CURRENT_STATE] == 25)
		return snprintf(buf, PAGE_SIZE, "%s", usbpd_state_strings[1]);
	else if (pst->prop[XM_PROP_CURRENT_STATE] == 31)
		return snprintf(buf, PAGE_SIZE, "%s", usbpd_state_strings[2]);
	else if (pst->prop[XM_PROP_CURRENT_STATE] == 5)
		return snprintf(buf, PAGE_SIZE, "%s", usbpd_state_strings[3]);
	else
		return snprintf(buf, PAGE_SIZE, "%s", usbpd_state_strings[0]);

}
static CLASS_ATTR_RO(current_state);

static ssize_t adapter_id_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_ADAPTER_ID);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%08x", pst->prop[XM_PROP_ADAPTER_ID]);
}
static CLASS_ATTR_RO(adapter_id);

static ssize_t adapter_svid_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_ADAPTER_SVID);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%04x", pst->prop[XM_PROP_ADAPTER_SVID]);
}
static CLASS_ATTR_RO(adapter_svid);

static ssize_t pd_verifed_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_PD_VERIFED, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t pd_verifed_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_PD_VERIFED);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_PD_VERIFED]);
}
static CLASS_ATTR_RW(pd_verifed);

static ssize_t pdo2_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_PDO2);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%08x\n", pst->prop[XM_PROP_PDO2]);
}
static CLASS_ATTR_RO(pdo2);

static ssize_t verify_process_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_VERIFY_PROCESS, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t verify_process_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_VERIFY_PROCESS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_VERIFY_PROCESS]);
}
static CLASS_ATTR_RW(verify_process);

static ssize_t power_max_show(struct class *c,
			struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
				battery_class);
	struct psy_state *xm_pst = &bcdev->psy_list[PSY_TYPE_XM];
	union power_supply_propval val = {0, };
	struct power_supply *usb_psy = NULL;
	struct power_supply *wls_psy = NULL;
	int rc, usb_present = 0, wls_present = 0;
	usb_psy = bcdev->psy_list[PSY_TYPE_USB].psy;
	wls_psy = bcdev->psy_list[PSY_TYPE_WLS].psy;

	if (usb_psy != NULL) {
		rc = usb_psy_get_prop(usb_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (!rc)
		      usb_present = val.intval;
		else
		      usb_present = 0;
	}
	if (wls_psy != NULL) {
		rc = wls_psy_get_prop(wls_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (!rc)
		      wls_present = val.intval;
		else
		      wls_present = 0;
	}
	if (usb_present || wls_present) {
#ifdef CONFIG_QTI_POGO_CHG
		if (g_battmngr_noti && pogo_flag) {
			if (!bcdev->battmg_dev) {
        		bcdev->battmg_dev = check_nano_ops();
    		}
			if (bcdev->battmg_dev) {
				xm_pst->prop[XM_PROP_POWER_MAX] = battmngr_noops_get_power_max(bcdev->battmg_dev);
			}
		} else {
			rc = read_property_id(bcdev, xm_pst, XM_PROP_POWER_MAX);
			if (rc < 0)
				return rc;
		}
#else
		rc = read_property_id(bcdev, xm_pst, XM_PROP_POWER_MAX);
		if (rc < 0)
		      return rc;
#endif

		return scnprintf(buf, PAGE_SIZE, "%u", xm_pst->prop[XM_PROP_POWER_MAX]);
	}

	return scnprintf(buf, PAGE_SIZE, "%u", 0);
}
static CLASS_ATTR_RO(power_max);

static ssize_t battmoni_isc_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_NVTFG_MONITOR_ISC);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_NVTFG_MONITOR_ISC]);
}

static ssize_t battmoni_isc_store(struct class *c, struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, pst, XM_PROP_NVTFG_MONITOR_ISC, val);
	if (rc < 0)
		return rc;
	return count;
}
static CLASS_ATTR_RW(battmoni_isc);
static ssize_t battmoni_soa_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_NVTFG_MONITOR_SOA);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_NVTFG_MONITOR_SOA]);
}
static CLASS_ATTR_RO(battmoni_soa);

#if defined(CONFIG_MI_DTPT)
static ssize_t over_peak_flag_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_OVER_PEAK_FLAG);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_OVER_PEAK_FLAG]);
}
static CLASS_ATTR_RO(over_peak_flag);
static ssize_t current_deviation_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_CURRENT_DEVIATION);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_CURRENT_DEVIATION]);
}
static CLASS_ATTR_RO(current_deviation);
static ssize_t power_deviation_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_POWER_DEVIATION);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_POWER_DEVIATION]);
}
static CLASS_ATTR_RO(power_deviation);
static ssize_t average_current_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_AVERAGE_CURRENT);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_AVERAGE_CURRENT]);
}
static CLASS_ATTR_RO(average_current);
static ssize_t average_temp_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_AVERAGE_TEMPERATURE);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_AVERAGE_TEMPERATURE]);
}
static CLASS_ATTR_RO(average_temp);
static ssize_t start_learn_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_START_LEARNING, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t start_learn_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_START_LEARNING);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_START_LEARNING]);
}
static CLASS_ATTR_RW(start_learn);
static ssize_t stop_learn_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_STOP_LEARNING, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t stop_learn_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_STOP_LEARNING);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_STOP_LEARNING]);
}
static CLASS_ATTR_RW(stop_learn);
static ssize_t set_learn_power_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_SET_LEARNING_POWER, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t set_learn_power_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_SET_LEARNING_POWER);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_SET_LEARNING_POWER]);
}
static CLASS_ATTR_RW(set_learn_power);
static ssize_t get_learn_power_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_GET_LEARNING_POWER);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_GET_LEARNING_POWER]);
}
static CLASS_ATTR_RO(get_learn_power);
static ssize_t get_learn_power_dev_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_GET_LEARNING_POWER_DEV);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_GET_LEARNING_POWER_DEV]);
}
static CLASS_ATTR_RO(get_learn_power_dev);
static ssize_t start_learn_b_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_START_LEARNING_B, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t start_learn_b_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_START_LEARNING_B);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_START_LEARNING_B]);
}
static CLASS_ATTR_RW(start_learn_b);
static ssize_t stop_learn_b_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_STOP_LEARNING_B, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t stop_learn_b_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_STOP_LEARNING_B);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_STOP_LEARNING_B]);
}
static CLASS_ATTR_RW(stop_learn_b);
static ssize_t set_learn_power_b_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_SET_LEARNING_POWER_B, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t set_learn_power_b_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_SET_LEARNING_POWER_B);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_SET_LEARNING_POWER_B]);
}
static CLASS_ATTR_RW(set_learn_power_b);
static ssize_t get_learn_power_b_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_GET_LEARNING_POWER_B);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_GET_LEARNING_POWER_B]);
}
static CLASS_ATTR_RO(get_learn_power_b);
static ssize_t get_learn_power_dev_b_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_GET_LEARNING_POWER_DEV_B);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_GET_LEARNING_POWER_DEV_B]);
}
static CLASS_ATTR_RO(get_learn_power_dev_b);
static ssize_t get_learn_time_dev_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_GET_LEARNING_TIME_DEV);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_GET_LEARNING_TIME_DEV]);
}
static CLASS_ATTR_RO(get_learn_time_dev);
static ssize_t constant_power_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_SET_CONSTANT_POWER, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t constant_power_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_SET_CONSTANT_POWER);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_SET_CONSTANT_POWER]);
}
static CLASS_ATTR_RW(constant_power);
static ssize_t remaining_time_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_GET_REMAINING_TIME);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_GET_REMAINING_TIME]);
}
static CLASS_ATTR_RO(remaining_time);
static ssize_t referance_power_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_SET_REFERANCE_POWER, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t referance_power_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_SET_REFERANCE_POWER);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_SET_REFERANCE_POWER]);
}
static CLASS_ATTR_RW(referance_power);
static ssize_t nvt_referance_current_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_GET_REFERANCE_CURRENT);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_GET_REFERANCE_CURRENT]);
}
static CLASS_ATTR_RO(nvt_referance_current);
static ssize_t nvt_referance_power_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_GET_REFERANCE_POWER);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_GET_REFERANCE_POWER]);
}
static CLASS_ATTR_RO(nvt_referance_power);
#endif

#if defined(CONFIG_MI_DTPT) && defined(CONFIG_DUAL_FUEL_GAUGE)
static ssize_t fg2_over_peak_flag_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_OVER_PEAK_FLAG);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_OVER_PEAK_FLAG]);
}
static CLASS_ATTR_RO(fg2_over_peak_flag);
static ssize_t fg2_current_deviation_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_CURRENT_DEVIATION);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_CURRENT_DEVIATION]);
}
static CLASS_ATTR_RO(fg2_current_deviation);
static ssize_t fg2_power_deviation_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_POWER_DEVIATION);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_POWER_DEVIATION]);
}
static CLASS_ATTR_RO(fg2_power_deviation);
static ssize_t fg2_average_current_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_AVERAGE_CURRENT);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_AVERAGE_CURRENT]);
}
static CLASS_ATTR_RO(fg2_average_current);
static ssize_t fg2_average_temp_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_AVERAGE_TEMPERATURE);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_AVERAGE_TEMPERATURE]);
}
static CLASS_ATTR_RO(fg2_average_temp);
static ssize_t fg2_start_learn_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FG2_START_LEARNING, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t fg2_start_learn_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_START_LEARNING);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_START_LEARNING]);
}
static CLASS_ATTR_RW(fg2_start_learn);
static ssize_t fg2_stop_learn_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FG2_STOP_LEARNING, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t fg2_stop_learn_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_STOP_LEARNING);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_STOP_LEARNING]);
}
static CLASS_ATTR_RW(fg2_stop_learn);
static ssize_t fg2_set_learn_power_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FG2_SET_LEARNING_POWER, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t fg2_set_learn_power_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_SET_LEARNING_POWER);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_SET_LEARNING_POWER]);
}
static CLASS_ATTR_RW(fg2_set_learn_power);
static ssize_t fg2_get_learn_power_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_LEARNING_POWER);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_GET_LEARNING_POWER]);
}
static CLASS_ATTR_RO(fg2_get_learn_power);
static ssize_t fg2_get_learn_power_dev_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_LEARNING_POWER_DEV);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_GET_LEARNING_POWER_DEV]);
}
static CLASS_ATTR_RO(fg2_get_learn_power_dev);
static ssize_t fg2_start_learn_b_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FG2_START_LEARNING_B, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t fg2_start_learn_b_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_START_LEARNING_B);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_START_LEARNING_B]);
}
static CLASS_ATTR_RW(fg2_start_learn_b);
static ssize_t fg2_stop_learn_b_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FG2_STOP_LEARNING_B, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t fg2_stop_learn_b_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_STOP_LEARNING_B);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_STOP_LEARNING_B]);
}
static CLASS_ATTR_RW(fg2_stop_learn_b);
static ssize_t fg2_set_learn_power_b_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FG2_SET_LEARNING_POWER_B, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t fg2_set_learn_power_b_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_SET_LEARNING_POWER_B);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_SET_LEARNING_POWER_B]);
}
static CLASS_ATTR_RW(fg2_set_learn_power_b);
static ssize_t fg2_get_learn_power_b_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_LEARNING_POWER_B);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_GET_LEARNING_POWER_B]);
}
static CLASS_ATTR_RO(fg2_get_learn_power_b);
static ssize_t fg2_get_learn_power_dev_b_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_LEARNING_POWER_DEV_B);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_GET_LEARNING_POWER_DEV_B]);
}
static CLASS_ATTR_RO(fg2_get_learn_power_dev_b);
static ssize_t fg2_get_learn_time_dev_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_LEARNING_TIME_DEV);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_GET_LEARNING_TIME_DEV]);
}
static CLASS_ATTR_RO(fg2_get_learn_time_dev);
static ssize_t fg2_constant_power_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FG2_SET_CONSTANT_POWER, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t fg2_constant_power_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_SET_CONSTANT_POWER);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_SET_CONSTANT_POWER]);
}
static CLASS_ATTR_RW(fg2_constant_power);
static ssize_t fg2_remaining_time_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_REMAINING_TIME);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_GET_REMAINING_TIME]);
}
static CLASS_ATTR_RO(fg2_remaining_time);
static ssize_t fg2_referance_power_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FG2_SET_REFERANCE_POWER, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t fg2_referance_power_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_SET_REFERANCE_POWER);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_SET_REFERANCE_POWER]);
}
static CLASS_ATTR_RW(fg2_referance_power);
static ssize_t fg2_nvt_referance_current_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_REFERANCE_CURRENT);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_GET_REFERANCE_CURRENT]);
}
static CLASS_ATTR_RO(fg2_nvt_referance_current);
static ssize_t fg2_nvt_referance_power_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_REFERANCE_POWER);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_GET_REFERANCE_POWER]);
}
static CLASS_ATTR_RO(fg2_nvt_referance_power);
static ssize_t fg1_design_capacity_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG1_GET_DESIGN_CAPACITY);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_GET_DESIGN_CAPACITY]);
}
static CLASS_ATTR_RO(fg1_design_capacity);
static ssize_t fg2_design_capacity_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_DESIGN_CAPACITY);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_GET_DESIGN_CAPACITY]);
}
static CLASS_ATTR_RO(fg2_design_capacity);
#endif
#if defined(CONFIG_DUAL_FUEL_GAUGE)
static ssize_t slave_chip_ok_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SLAVE_CHIP_OK);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SLAVE_CHIP_OK]);
}
static CLASS_ATTR_RO(slave_chip_ok);

static ssize_t slave_authentic_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	bcdev->slave_battery_auth = val;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_SLAVE_AUTHENTIC, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t slave_authentic_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SLAVE_AUTHENTIC);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SLAVE_AUTHENTIC]);
}
static CLASS_ATTR_RW(slave_authentic);

static ssize_t fg2_rm_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_RM);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_RM]);
}
static CLASS_ATTR_RO(fg2_rm);

static ssize_t fg2_fcc_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_FCC);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_FCC]);
}
static CLASS_ATTR_RO(fg2_fcc);

static ssize_t fg2_soh_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_SOH);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_SOH]);
}
static CLASS_ATTR_RO(fg2_soh);

static ssize_t fg2_current_max_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_CURRENT_MAX);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_CURRENT_MAX]);
}
static CLASS_ATTR_RO(fg2_current_max);

static ssize_t fg2_vol_max_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_VOL_MAX);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_VOL_MAX]);
}
static CLASS_ATTR_RO(fg2_vol_max);

static ssize_t fg2_rsoc_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_RSOC);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_RSOC]);
}
static CLASS_ATTR_RO(fg2_rsoc);

static ssize_t fg1_ibatt_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_IBATT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_IBATT]);
}
static CLASS_ATTR_RO(fg1_ibatt);

static ssize_t fg2_ibatt_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_IBATT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_IBATT]);
}
static CLASS_ATTR_RO(fg2_ibatt);

static ssize_t fg1_temp_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_TEMP);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_TEMP]);
}
static CLASS_ATTR_RO(fg1_temp);

static ssize_t fg2_temp_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_TEMP);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_TEMP]);
}
static CLASS_ATTR_RO(fg2_temp);

static ssize_t fg1_vol_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_VOL);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG1_VOL]);
}
static CLASS_ATTR_RO(fg1_vol);

static ssize_t fg2_vol_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_VOL);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG2_VOL]);
}
static CLASS_ATTR_RO(fg2_vol);

static ssize_t fg2_ai_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_AI);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_AI]);
}
static CLASS_ATTR_RO(fg2_ai);

static ssize_t fg2_tremq_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_TREMQ);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_TREMQ]);
}
static CLASS_ATTR_RO(fg2_tremq);

static ssize_t fg2_tfullq_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_TFULLCHGQ);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_TFULLCHGQ]);
}
static CLASS_ATTR_RO(fg2_tfullq);

static ssize_t fg1_FullChargeFlag_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_FullChargeFlag);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_FullChargeFlag]);
}
static CLASS_ATTR_RO(fg1_FullChargeFlag);

static ssize_t fg2_FullChargeFlag_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_FullChargeFlag);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG2_FullChargeFlag]);
}
static CLASS_ATTR_RO(fg2_FullChargeFlag);

static ssize_t fg1_soc_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_SOC);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG1_SOC]);
}
static CLASS_ATTR_RO(fg1_soc);

static ssize_t fg2_soc_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_SOC);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG2_SOC]);
}
static CLASS_ATTR_RO(fg2_soc);

static ssize_t fg1_record_delta_r1_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_GET_RECORD_DELTA_R1);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG1_GET_RECORD_DELTA_R1]);
}
static CLASS_ATTR_RO(fg1_record_delta_r1);

static ssize_t fg1_record_delta_r2_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_GET_RECORD_DELTA_R2);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG1_GET_RECORD_DELTA_R2]);
}
static CLASS_ATTR_RO(fg1_record_delta_r2);

static ssize_t fg1_r1_discharge_flag_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_GET_R1_DISCHARGE_FLAG);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG1_GET_R1_DISCHARGE_FLAG]);
}
static CLASS_ATTR_RO(fg1_r1_discharge_flag);

static ssize_t fg1_r2_discharge_flag_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_GET_R2_DISCHARGE_FLAG);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG1_GET_R2_DISCHARGE_FLAG]);
}
static CLASS_ATTR_RO(fg1_r2_discharge_flag);

static ssize_t fg2_record_delta_r1_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_RECORD_DELTA_R1);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG2_GET_RECORD_DELTA_R1]);
}
static CLASS_ATTR_RO(fg2_record_delta_r1);

static ssize_t fg2_record_delta_r2_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_RECORD_DELTA_R2);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG2_GET_RECORD_DELTA_R2]);
}
static CLASS_ATTR_RO(fg2_record_delta_r2);

static ssize_t fg2_r1_discharge_flag_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_R1_DISCHARGE_FLAG);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG2_GET_R1_DISCHARGE_FLAG]);
}
static CLASS_ATTR_RO(fg2_r1_discharge_flag);

static ssize_t fg2_r2_discharge_flag_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG2_GET_R2_DISCHARGE_FLAG);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG2_GET_R2_DISCHARGE_FLAG]);
}
static CLASS_ATTR_RO(fg2_r2_discharge_flag);
#endif

#if defined(CONFIG_MI_ENABLE_DP)
static ssize_t has_dp_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_HAS_DP);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_HAS_DP]);
}
static CLASS_ATTR_RO(has_dp);
#endif

static ssize_t dam_ovpgate_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;
	if (kstrtobool(buf, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_DAM_OVPGATE, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t dam_ovpgate_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_DAM_OVPGATE);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_DAM_OVPGATE]);
}
static CLASS_ATTR_RW(dam_ovpgate);

static ssize_t charging_suspend_battery_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;
	if (kstrtobool(buf, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_CHARGING_SUSPEND_BATTERY, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t charging_suspend_battery_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_CHARGING_SUSPEND_BATTERY);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CHARGING_SUSPEND_BATTERY]);
}
static CLASS_ATTR_RW(charging_suspend_battery);

static ssize_t last_node_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_LAST_NODE);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_LAST_NODE]);
}
static CLASS_ATTR_RO(last_node);

#ifdef CONFIG_QTI_POGO_CHG
static ssize_t car_app_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	u8 val = 0;
	if (!bcdev)
        return -EINVAL;

	val = pogo_flag;

	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
static CLASS_ATTR_RO(car_app);

static ssize_t pogo_connect_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	if (!bcdev)
        return -EINVAL;

	rc = read_property_id(bcdev, pst, XM_PROP_DCIN_STATE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_DCIN_STATE]);
}
static CLASS_ATTR_RO(pogo_connect);

static ssize_t cp_ovp_mode_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_SC8561_OVP_MOS, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t cp_ovp_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SC8561_OVP_MOS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SC8561_OVP_MOS]);
}
static CLASS_ATTR_RW(cp_ovp_mode);

#endif

static struct attribute *battery_class_attrs[] = {
	/*common charging node*/
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_moisture_detection_status.attr,
	&class_attr_moisture_detection_en.attr,
	&class_attr_wireless_boost_en.attr,
	&class_attr_fake_soc.attr,
	&class_attr_wireless_fw_update.attr,
	&class_attr_wireless_fw_force_update.attr,
	&class_attr_wireless_fw_version.attr,
	&class_attr_wireless_fw_crc.attr,
	&class_attr_wireless_fw_update_time_ms.attr,
	&class_attr_wireless_type.attr,
	&class_attr_ship_mode_en.attr,
	&class_attr_restrict_chg.attr,
	&class_attr_restrict_cur.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_usb_typec_compliant.attr,
	&class_attr_bq2597x_chip_ok.attr,
	&class_attr_bq2597x_slave_chip_ok.attr,
	&class_attr_bq2597x_bus_current.attr,
	&class_attr_bq2597x_slave_bus_current.attr,
	&class_attr_bq2597x_bus_delta.attr,
	&class_attr_bq2597x_bus_voltage.attr,
	&class_attr_bq2597x_battery_present.attr,
	&class_attr_bq2597x_slave_battery_present.attr,
	&class_attr_bq2597x_battery_voltage.attr,
	&class_attr_resistance_id.attr,
	&class_attr_input_suspend.attr,
	&class_attr_cc_orientation.attr,
	&class_attr_authentic.attr,
	&class_attr_bap_match.attr,
	&class_attr_chip_ok.attr,
	&class_attr_soc_decimal.attr,
	&class_attr_soc_decimal_rate.attr,
	&class_attr_quick_charge_type.attr,
	&class_attr_apdo_max.attr,
	&class_attr_power_max.attr,
	&class_attr_smart_batt.attr,
	&class_attr_fg_raw_soc.attr,
	&class_attr_night_charging.attr,
	&class_attr_connector_temp.attr,
	&class_attr_real_type.attr,
	&class_attr_battcont_online.attr,
	&class_attr_request_vdm_cmd.attr,
	&class_attr_current_state.attr,
	&class_attr_adapter_id.attr,
	&class_attr_adapter_svid.attr,
	&class_attr_pd_verifed.attr,
	&class_attr_pdo2.attr,
	&class_attr_verify_process.attr,
	&class_attr_verify_digest.attr,
	&class_attr_verify_slave_flag.attr,
	&class_attr_thermal_remove.attr,
	&class_attr_fastchg_mode.attr,
	&class_attr_typec_mode.attr,
	&class_attr_mtbf_current.attr,
	&class_attr_fake_temp.attr,
	&class_attr_fake_cycle.attr,
	&class_attr_afp_temp.attr,
	&class_attr_plate_shock.attr,
	&class_attr_shutdown_delay.attr,
	&class_attr_shipmode_count_reset.attr,
	&class_attr_cc_short_vbus.attr,
	&class_attr_otg_ui_support.attr,
	&class_attr_cid_status.attr,
	&class_attr_cc_toggle.attr,
	&class_attr_smart_chg.attr,
	/*wireless charging node*/
#if defined(CONFIG_MI_WIRELESS)
	&class_attr_tx_mac.attr,
	&class_attr_rx_cr.attr,
	&class_attr_rx_cep.attr,
	&class_attr_bt_state.attr,
#ifndef CONFIG_WIRELESS_REVERSE_CLOSE
	&class_attr_reverse_chg_mode.attr,
	&class_attr_reverse_chg_state.attr,
#endif
	&class_attr_wireless_chip_fw.attr,
	&class_attr_wireless_tx_uuid.attr,
    &class_attr_wireless_version_forcit.attr,
	&class_attr_wls_bin.attr,
	&class_attr_rx_vout.attr,
	&class_attr_rx_vrect.attr,
	&class_attr_rx_iout.attr,
	&class_attr_tx_adapter.attr,
	&class_attr_op_mode.attr,
	&class_attr_wls_die_temp.attr,
	&class_attr_wlscharge_control_limit.attr,
	&class_attr_wls_thermal_remove.attr,
	&class_attr_wls_debug.attr,
	&class_attr_wls_fw_state.attr,
	&class_attr_wls_car_adapter.attr,
	&class_attr_wls_tx_speed.attr,
	&class_attr_wls_fc_flag.attr,
	&class_attr_rx_ss.attr,
	&class_attr_rx_offset.attr,
	&class_attr_tx_q.attr,
#if defined(CONFIG_MI_PEN_WIRELESS)
	&class_attr_pen_mac.attr,
	&class_attr_tx_iout.attr,
	&class_attr_tx_vout.attr,
	&class_attr_pen_soc.attr,
	&class_attr_pen_hall3.attr,
	&class_attr_pen_hall4.attr,
	&class_attr_pen_tx_ss.attr,
	&class_attr_pen_place_err.attr,
	&class_attr_fake_ss.attr,
#endif	//CONFIG_MI_PEN_WIRELESS
#endif
	/*fuelgauge debug node*/
	&class_attr_fg_vendor.attr,
	&class_attr_fg1_qmax.attr,
	&class_attr_fg1_rm.attr,
	&class_attr_fg1_fcc.attr,
	&class_attr_fg1_soh.attr,
	&class_attr_fg1_fcc_soh.attr,
	&class_attr_fg1_cycle.attr,
	&class_attr_fg1_fastcharge.attr,
	&class_attr_fg1_current_max.attr,
	&class_attr_fg1_vol_max.attr,
	&class_attr_fg1_tsim.attr,
	&class_attr_fg1_tambient.attr,
	&class_attr_fg1_tremq.attr,
	&class_attr_fg1_tfullq.attr,
	&class_attr_fg1_rsoc.attr,
	&class_attr_fg1_ai.attr,
	&class_attr_fg1_temp_max.attr,
	&class_attr_fg1_time_ot.attr,
	&class_attr_fg1_time_ht.attr,
	&class_attr_fg1_seal_set.attr,
	&class_attr_fg1_seal_state.attr,
	&class_attr_fg1_df_check.attr,
	&class_attr_battmoni_isc.attr,
	&class_attr_battmoni_soa.attr,
#if defined(CONFIG_MI_DTPT)
	/*dtpt fuelgauge feature*/
	&class_attr_over_peak_flag.attr,
	&class_attr_current_deviation.attr,
	&class_attr_power_deviation.attr,
	&class_attr_average_current.attr,
	&class_attr_average_temp.attr,
	&class_attr_start_learn.attr,
	&class_attr_stop_learn.attr,
	&class_attr_set_learn_power.attr,
	&class_attr_get_learn_power.attr,
	&class_attr_get_learn_power_dev.attr,
	&class_attr_get_learn_time_dev.attr,
	&class_attr_constant_power.attr,
	&class_attr_remaining_time.attr,
	&class_attr_referance_power.attr,
	&class_attr_nvt_referance_current.attr,
	&class_attr_nvt_referance_power.attr,
	&class_attr_start_learn_b.attr,
	&class_attr_stop_learn_b.attr,
	&class_attr_set_learn_power_b.attr,
	&class_attr_get_learn_power_b.attr,
	&class_attr_get_learn_power_dev_b.attr,
#endif
#if defined(CONFIG_MI_DTPT) && defined(CONFIG_DUAL_FUEL_GAUGE)
	&class_attr_fg2_over_peak_flag.attr,
	&class_attr_fg2_current_deviation.attr,
	&class_attr_fg2_power_deviation.attr,
	&class_attr_fg2_average_current.attr,
	&class_attr_fg2_average_temp.attr,
	&class_attr_fg2_start_learn.attr,
	&class_attr_fg2_stop_learn.attr,
	&class_attr_fg2_set_learn_power.attr,
	&class_attr_fg2_get_learn_power.attr,
	&class_attr_fg2_get_learn_power_dev.attr,
	&class_attr_fg2_get_learn_time_dev.attr,
	&class_attr_fg2_constant_power.attr,
	&class_attr_fg2_remaining_time.attr,
	&class_attr_fg2_referance_power.attr,
	&class_attr_fg2_nvt_referance_current.attr,
	&class_attr_fg2_nvt_referance_power.attr,
	&class_attr_fg2_start_learn_b.attr,
	&class_attr_fg2_stop_learn_b.attr,
	&class_attr_fg2_set_learn_power_b.attr,
	&class_attr_fg2_get_learn_power_b.attr,
	&class_attr_fg2_get_learn_power_dev_b.attr,
	&class_attr_fg1_design_capacity.attr,
	&class_attr_fg2_design_capacity.attr,
#endif
#if defined(CONFIG_DUAL_FUEL_GAUGE)
	/*dual fuelgauge node*/
	&class_attr_slave_chip_ok.attr,
	&class_attr_slave_authentic.attr,
	&class_attr_fg2_rm.attr,
	&class_attr_fg2_fcc.attr,
	&class_attr_fg2_soh.attr,
	&class_attr_fg2_current_max.attr,
	&class_attr_fg2_vol_max.attr,
	&class_attr_fg2_rsoc.attr,
	&class_attr_fg1_ibatt.attr,
	&class_attr_fg2_ibatt.attr,
	&class_attr_fg1_temp.attr,
	&class_attr_fg2_temp.attr,
	&class_attr_fg1_vol.attr,
	&class_attr_fg2_vol.attr,
	&class_attr_fg2_ai.attr,
	&class_attr_fg2_tremq.attr,
	&class_attr_fg2_tfullq.attr,
	&class_attr_fg1_FullChargeFlag.attr,
	&class_attr_fg2_FullChargeFlag.attr,
	&class_attr_fg1_soc.attr,
	&class_attr_fg2_soc.attr,
	&class_attr_fg1_record_delta_r1.attr,
	&class_attr_fg1_record_delta_r2.attr,
	&class_attr_fg1_r1_discharge_flag.attr,
	&class_attr_fg1_r2_discharge_flag.attr,
	&class_attr_fg2_record_delta_r1.attr,
	&class_attr_fg2_record_delta_r2.attr,
	&class_attr_fg2_r1_discharge_flag.attr,
	&class_attr_fg2_r2_discharge_flag.attr,
#endif
#if defined(CONFIG_MI_ENABLE_DP)
	&class_attr_has_dp.attr,
#endif
	&class_attr_dam_ovpgate.attr,
	&class_attr_charging_suspend_battery.attr,
	&class_attr_last_node.attr,
#ifdef CONFIG_QTI_POGO_CHG
	&class_attr_car_app.attr,
	&class_attr_pogo_connect.attr,
	&class_attr_cp_ovp_mode.attr,
#endif
	NULL,
};
ATTRIBUTE_GROUPS(battery_class);

#ifdef CONFIG_DEBUG_FS
static void battery_chg_add_debugfs(struct battery_chg_dev *bcdev)
{
	int rc;
	struct dentry *dir;

	dir = debugfs_create_dir("battery_charger", NULL);
	if (IS_ERR(dir)) {
		rc = PTR_ERR(dir);
		return;
	}

	bcdev->debugfs_dir = dir;
	debugfs_create_bool("block_tx", 0600, dir, &bcdev->block_tx);
}
#else
static void battery_chg_add_debugfs(struct battery_chg_dev *bcdev) { }
#endif

static void generate_xm_charge_uvent(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev, xm_prop_change_work.work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_WLS];
	int prop_id, rc;

	dev_err(bcdev->dev,"%s+++", __func__);

	kobject_uevent_env(&bcdev->dev->kobj, KOBJ_CHANGE, NULL);

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_PRESENT);
	if (prop_id < 0)
		return;
	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return;
	bcdev->boost_mode = pst->prop[WLS_BOOST_EN];

	return;
}

#define CHARGING_PERIOD_S		30
static void xm_charger_debug_info_print_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev, charger_debug_info_print_work.work);
	struct power_supply *usb_psy = NULL;
	struct power_supply *wls_psy = NULL;
	int rc, usb_present = 0, wls_present = 0;
	unsigned int raw_soc = 0;
	int vbus_vol_uv = 0, ibus_ua = 0;
	int interval = CHARGING_PERIOD_S;
	union power_supply_propval val = {0, };
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];

	usb_psy = bcdev->psy_list[PSY_TYPE_USB].psy;
	if (usb_psy != NULL) {
		rc = usb_psy_get_prop(usb_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (!rc)
			usb_present = val.intval;
		else
			usb_present = 0;
	} else {
		return;
	}

#if defined(CONFIG_MI_WIRELESS)
	wls_psy = bcdev->psy_list[PSY_TYPE_WLS].psy;
	if (wls_psy != NULL) {
		rc = wls_psy_get_prop(wls_psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (!rc)
			wls_present = val.intval;
		else
			wls_present = 0;
	} else {
		wls_present = 0;
	}
#endif

#ifdef CONFIG_QTI_POGO_CHG
	rc = wls_psy_get_prop(wls_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (!rc)
		  vbus_vol_uv = val.intval;
	else
		  vbus_vol_uv = 0;

	rc = wls_psy_get_prop(wls_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (!rc)
		  ibus_ua = val.intval;
	else
		  ibus_ua = 0;
#endif

	if ((usb_present == 1) || (wls_present == 1)) {

		rc = read_property_id(bcdev, pst, XM_PROP_FG_VENDOR_ID);

		if (usb_present == 1) {
			rc = usb_psy_get_prop(usb_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
			if (!rc)
			      vbus_vol_uv = val.intval;
			else
			      vbus_vol_uv = 0;

			rc = usb_psy_get_prop(usb_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
			if (!rc)
			      ibus_ua = val.intval;
			else
			      ibus_ua = 0;

		} else if(wls_present == 1) {
			rc = wls_psy_get_prop(wls_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
			if (!rc)
			      vbus_vol_uv = val.intval;
			else
			      vbus_vol_uv = 0;

			rc = wls_psy_get_prop(wls_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
			if (!rc)
			      ibus_ua = val.intval;
			else
			      ibus_ua = 0;
		}

		rc = read_property_id(bcdev, pst, XM_PROP_MTBF_CURRENT);
		if (!rc && pst->prop[XM_PROP_MTBF_CURRENT] != bcdev->mtbf_current) {
			rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
					XM_PROP_MTBF_CURRENT, bcdev->mtbf_current);
		}

		rc = read_property_id(bcdev, pst, XM_PROP_AUTHENTIC);
		if (!rc && !pst->prop[XM_PROP_AUTHENTIC] && bcdev->battery_auth) {
			rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
					XM_PROP_AUTHENTIC, bcdev->battery_auth);
		}

		rc = read_property_id(bcdev, pst, XM_PROP_SLAVE_AUTHENTIC);
		if (!rc && !pst->prop[XM_PROP_SLAVE_AUTHENTIC] && bcdev->slave_battery_auth) {
			rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
					XM_PROP_SLAVE_AUTHENTIC, bcdev->slave_battery_auth);
		}

		rc = read_property_id(bcdev, pst, XM_PROP_FG1_RM);
		rc = read_property_id(bcdev, pst, XM_PROP_FG1_FCC);
		raw_soc = (pst->prop[XM_PROP_FG1_RM] * 10) / (pst->prop[XM_PROP_FG1_FCC] / 1000);

		interval = CHARGING_PERIOD_S;
		schedule_delayed_work(&bcdev->charger_debug_info_print_work, interval * HZ);
		bcdev->debug_work_en = 1;
	} else {
		bcdev->debug_work_en = 0;
	}

}

#define BATT_UPDATE_PERIOD_10S		10
#define BATT_UPDATE_PERIOD_20S		20
#define BATT_UPDATE_PERIOD_8S		  8
static void xm_batt_update_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev, batt_update_work.work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	struct psy_state *batt_pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int interval = BATT_UPDATE_PERIOD_10S;
	int rc = 0;

	rc = read_property_id(bcdev, batt_pst, BATT_CAPACITY);
	if ((batt_pst->prop[BATT_CAPACITY] / 100) < 15)
		interval = BATT_UPDATE_PERIOD_8S;
	rc = read_property_id(bcdev, pst, XM_PROP_THERMAL_TEMP);
	if (bcdev->blank_state)
		interval = BATT_UPDATE_PERIOD_20S;

	schedule_delayed_work(&bcdev->batt_update_work, interval * HZ);
}

static int battery_chg_parse_dt(struct battery_chg_dev *bcdev)
{
	struct device_node *node = bcdev->dev->of_node;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int rc, len;

	of_property_read_string(node, "qcom,wireless-fw-name",
				&bcdev->wls_fw_name);

	of_property_read_u32(node, "qcom,shutdown-voltage",
				&bcdev->shutdown_volt_mv);
	rc = of_property_count_elems_of_size(node, "qcom,thermal-mitigation",
						sizeof(u32));
	if (rc <= 0)
		return 0;
	len = rc;

	bcdev->thermal_levels = devm_kcalloc(bcdev->dev, len + 1,
					sizeof(*bcdev->thermal_levels),
					GFP_KERNEL);
	if (!bcdev->thermal_levels)
		return -ENOMEM;

	/*
	 * Element 0 is for normal charging current. Elements from index 1
	 * onwards is for thermal mitigation charging currents.
	 */

	bcdev->thermal_levels[0] = pst->prop[BATT_CHG_CTRL_LIM_MAX];

	rc = of_property_read_u32_array(node, "qcom,thermal-mitigation",
					&bcdev->thermal_levels[1], len);
	if (rc < 0) {
		pr_err("Error in reading qcom,thermal-mitigation, rc=%d\n", rc);
		return rc;
	}

	bcdev->num_thermal_levels = MAX_THERMAL_LEVEL;
	bcdev->thermal_fcc_ua = pst->prop[BATT_CHG_CTRL_LIM_MAX];
	bcdev->shutdown_delay_en = of_property_read_bool(node, "mi,support-shutdown-delay");
	bcdev->extreme_mode_en = of_property_read_bool(node, "mi,support-extreme-mode");

	return 0;
}


static int battery_chg_shutdown(struct notifier_block *nb, unsigned long code,
		void *unused)
{
	struct battery_charger_shutdown_req_msg msg = { { 0 } };
	struct battery_chg_dev *bcdev = container_of(nb, struct battery_chg_dev,
						     shutdown_notifier);
	int rc,shipmode,afp_temp;
	char prop_buf1[10];
	char prop_buf2[10];

	shipmode_count_reset_show(&(bcdev->battery_class), NULL, prop_buf1);
	if (kstrtoint(prop_buf1, 10, &shipmode))
		return -EINVAL;
	
	afp_temp_show(&(bcdev->battery_class), NULL, prop_buf2);
	if (kstrtoint(prop_buf2, 10, &afp_temp))
		return -EINVAL;

	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_SHUTDOWN_REQ_SET;

	if (code == SYS_POWER_OFF || code == SYS_RESTART) {
		rc = battery_chg_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			pr_emerg("Failed to write shutdown cmd to adsp: %d\n", rc);
		else {
			msleep(1000);
			pr_emerg("adsp shutdown success\n");
		}
	}

	return NOTIFY_DONE;
}

static int battery_chg_ship_mode(struct notifier_block *nb, unsigned long code,
		void *unused)
{
	struct battery_charger_ship_mode_req_msg msg = { { 0 } };
	struct battery_chg_dev *bcdev = container_of(nb, struct battery_chg_dev,
						     reboot_notifier);
	int rc;

	if (!bcdev->ship_mode_en)
		return NOTIFY_DONE;

	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_SHIP_MODE_REQ_SET;
	msg.ship_mode_type = SHIP_MODE_PMIC;

	if (code == SYS_POWER_OFF) {
		rc = battery_chg_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			pr_emerg("Failed to write ship mode: %d\n", rc);
	}

	return NOTIFY_DONE;
}

static void panel_event_notifier_callback(enum panel_event_notifier_tag tag,
			struct panel_event_notification *notification, void *data)
{
	struct battery_chg_dev *bcdev = data;

	if (!notification) {
		return;
	}

	if(notification->notif_data.early_trigger) {
		return;
	}

	switch (notification->notif_type) {
	case DRM_PANEL_EVENT_BLANK:
	case DRM_PANEL_EVENT_BLANK_LP:
		bcdev->blank_state = 1; 
		break;
	case DRM_PANEL_EVENT_UNBLANK:
		bcdev->blank_state = 0;
		break;
	case DRM_PANEL_EVENT_FPS_CHANGE:
		return;
	default:
		break;
	}
}

static int battery_chg_register_panel_notifier(struct battery_chg_dev *bcdev)
{
	struct device_node *pnode;
	struct drm_panel *panel, *active_panel = NULL;
	void *cookie = NULL;
	int i, count, rc = 0;

	pnode = of_find_node_by_name(NULL, "charge-screen");
	if (!pnode) {
		return 0;
	}
	count = of_count_phandle_with_args(pnode, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		pnode = of_parse_phandle(pnode, "panel", i);
		if (!pnode) {
			return 0;
		}

		panel = of_drm_find_panel(pnode);
		of_node_put(pnode);
		if (!IS_ERR(panel)) {
			active_panel = panel;
			break;
		}
	}

	if (!active_panel) {
		rc = PTR_ERR(panel);
		if (rc != -EPROBE_DEFER)
		return 0;
	}

	cookie = panel_event_notifier_register(
			PANEL_EVENT_NOTIFICATION_PRIMARY,
			PANEL_EVENT_NOTIFIER_CLIENT_BATTERY_CHARGER,
			active_panel,
			panel_event_notifier_callback,
			(void *)bcdev);
	if (IS_ERR(cookie)) {
		rc = PTR_ERR(cookie);
	}

	bcdev->notifier_cookie = cookie;
	return 0;
}

#define MAX_UEVENT_LENGTH 50
static int add_xiaomi_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct battery_chg_dev *bcdev = platform_get_drvdata(pdev);

	char *prop_buf = NULL;
	char uevent_string[MAX_UEVENT_LENGTH+1];
	u32 i = 0;
#if defined(CONFIG_MI_PEN_WIRELESS)
	int val;
#endif

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return 0;

	/*add our prop start*/
#if defined(CONFIG_MI_WIRELESS)
#ifndef CONFIG_WIRELESS_REVERSE_CLOSE
	reverse_chg_state_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_REVERSE_CHG_STATE=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	reverse_chg_mode_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_REVERSE_CHG_MODE=%s", prop_buf);
	add_uevent_var(env, uevent_string);
#endif

#if defined(CONFIG_MI_PEN_WIRELESS)
	reverse_chg_state_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_REVERSE_PEN_CHG_STATE=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	pen_hall3_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_PEN_HALL3=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	pen_hall4_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_PEN_HALL4=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	pen_soc_show( &(bcdev->battery_class), NULL, prop_buf);
	if (!kstrtoint(prop_buf, 10, &val)) {
		if (val != 0xff) {
			snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_REVERSE_PEN_SOC=%d", val);
			add_uevent_var(env, uevent_string);
		}
	}

	pen_mac_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_PEN_MAC=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	pen_place_err_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_PEN_PLACE_ERR=%s", prop_buf);
	add_uevent_var(env, uevent_string);
#endif

	wls_fw_state_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_WLS_FW_STATE=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	wls_car_adapter_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_WLS_CAR_ADAPTER=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	tx_adapter_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_TX_ADAPTER=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	rx_offset_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_RX_OFFSET=%s", prop_buf);
	add_uevent_var(env, uevent_string);
#endif

	soc_decimal_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_SOC_DECIMAL=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	soc_decimal_rate_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_SOC_DECIMAL_RATE=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	quick_charge_type_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_QUICK_CHARGE_TYPE=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	shutdown_delay_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_SHUTDOWN_DELAY=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	connector_temp_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_CONNECTOR_TEMP=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	cc_short_vbus_show(&(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_CC_SHORT_VBUS=%s", prop_buf);
	add_uevent_var(env, uevent_string);

#ifdef CONFIG_QTI_POGO_CHG
	car_app_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_CAR_APP_STATE=%s", prop_buf);
	add_uevent_var(env, uevent_string);
#endif
	dev_err(bcdev->dev," %s ", env->envp[env->envp_idx -1]);

	dev_err(bcdev->dev,"currnet uevent info :");
	for(i = 0; i < env->envp_idx; ++i){
#ifndef CONFIG_MI_PEN_WIRELESS		//2617887
		if(i <= 9 || (i >= 12 && i <= 16 && i != 14))
			continue;
#endif
	    dev_err(bcdev->dev," %s ", env->envp[i]);
	}

	free_page((unsigned long)prop_buf);
	return 0;
}

static struct device_type dev_type_xiaomi_uevent = {
	.name = "dev_type_xiaomi_uevent",
	.uevent = add_xiaomi_uevent,
};

static int
battery_chg_get_max_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long *state)
{
	struct battery_chg_dev *bcdev = tcd->devdata;

	*state = bcdev->num_thermal_levels;

	return 0;
}

static int
battery_chg_get_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long *state)
{
	struct battery_chg_dev *bcdev = tcd->devdata;

	*state = bcdev->curr_thermal_level;

	return 0;
}

static int
battery_chg_set_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long state)
{
	struct battery_chg_dev *bcdev = tcd->devdata;

	return battery_psy_set_charge_current(bcdev, (int)state);
}

static const struct thermal_cooling_device_ops battery_tcd_ops = {
	.get_max_state = battery_chg_get_max_charge_cntl_limit,
	.get_cur_state = battery_chg_get_cur_charge_cntl_limit,
	.set_cur_state = battery_chg_set_cur_charge_cntl_limit,
};


static int battery_chg_probe(struct platform_device *pdev)
{
	struct battery_chg_dev *bcdev;
	struct device *dev = &pdev->dev;
	struct pmic_glink_client_data client_data = { };
	struct thermal_cooling_device *tcd;
	struct psy_state *pst;
	int rc, i;
	dev_err(dev, "battery_chg probe start\n");
	bcdev = devm_kzalloc(&pdev->dev, sizeof(*bcdev), GFP_KERNEL);
	if (!bcdev)
		return -ENOMEM;

	bcdev->psy_list[PSY_TYPE_BATTERY].map = battery_prop_map;
	bcdev->psy_list[PSY_TYPE_BATTERY].prop_count = BATT_PROP_MAX;
	bcdev->psy_list[PSY_TYPE_BATTERY].opcode_get = BC_BATTERY_STATUS_GET;
	bcdev->psy_list[PSY_TYPE_BATTERY].opcode_set = BC_BATTERY_STATUS_SET;
	bcdev->psy_list[PSY_TYPE_USB].map = usb_prop_map;
	bcdev->psy_list[PSY_TYPE_USB].prop_count = USB_PROP_MAX;
	bcdev->psy_list[PSY_TYPE_USB].opcode_get = BC_USB_STATUS_GET;
	bcdev->psy_list[PSY_TYPE_USB].opcode_set = BC_USB_STATUS_SET;
	bcdev->psy_list[PSY_TYPE_WLS].map = wls_prop_map;
	bcdev->psy_list[PSY_TYPE_WLS].prop_count = WLS_PROP_MAX;
	bcdev->psy_list[PSY_TYPE_WLS].opcode_get = BC_WLS_STATUS_GET;
	bcdev->psy_list[PSY_TYPE_WLS].opcode_set = BC_WLS_STATUS_SET;
	bcdev->psy_list[PSY_TYPE_XM].map = xm_prop_map;
	bcdev->psy_list[PSY_TYPE_XM].prop_count = XM_PROP_MAX;
	bcdev->psy_list[PSY_TYPE_XM].opcode_get = BC_XM_STATUS_GET;
	bcdev->psy_list[PSY_TYPE_XM].opcode_set = BC_XM_STATUS_SET;

	for (i = 0; i < PSY_TYPE_MAX; i++) {
		bcdev->psy_list[i].prop =
			devm_kcalloc(&pdev->dev, bcdev->psy_list[i].prop_count,
					sizeof(u32), GFP_KERNEL);
		if (!bcdev->psy_list[i].prop)
			return -ENOMEM;
	}

	bcdev->psy_list[PSY_TYPE_BATTERY].model =
		devm_kzalloc(&pdev->dev, MAX_STR_LEN, GFP_KERNEL);
	if (!bcdev->psy_list[PSY_TYPE_BATTERY].model)
		return -ENOMEM;

	bcdev->digest=
		devm_kzalloc(&pdev->dev, BATTERY_DIGEST_LEN, GFP_KERNEL);
	if (!bcdev->digest)
		return -ENOMEM;
	bcdev->ss_auth_data=
		devm_kzalloc(&pdev->dev, BATTERY_SS_AUTH_DATA_LEN * sizeof(u32), GFP_KERNEL);
	if (!bcdev->ss_auth_data)
		return -ENOMEM;

	mutex_init(&bcdev->rw_lock);
	init_rwsem(&bcdev->state_sem);
	init_completion(&bcdev->ack);
	init_completion(&bcdev->fw_buf_ack);
	init_completion(&bcdev->fw_update_ack);
	INIT_WORK(&bcdev->subsys_up_work, battery_chg_subsys_up_work);
	INIT_WORK(&bcdev->usb_type_work, battery_chg_update_usb_type_work);
	INIT_WORK(&bcdev->battery_check_work, battery_chg_check_status_work);
	INIT_WORK( &bcdev->pen_notifier_work, pen_charge_notifier_work);
	INIT_DELAYED_WORK( &bcdev->xm_prop_change_work, generate_xm_charge_uvent);
	INIT_DELAYED_WORK( &bcdev->charger_debug_info_print_work, xm_charger_debug_info_print_work);
	INIT_DELAYED_WORK( &bcdev->batt_update_work, xm_batt_update_work);
	bcdev->dev = dev;

	rc = battery_chg_register_panel_notifier(bcdev);
	if (rc < 0)
		return rc;

	client_data.id = MSG_OWNER_BC;
	client_data.name = "battery_charger";
	client_data.msg_cb = battery_chg_callback;
	client_data.priv = bcdev;
	client_data.state_cb = battery_chg_state_cb;

	bcdev->client = pmic_glink_register_client(dev, &client_data);
	if (IS_ERR(bcdev->client)) {
		rc = PTR_ERR(bcdev->client);
		if (rc != -EPROBE_DEFER)
			dev_err(dev, "Error in registering with pmic_glink %d\n",
				rc);
		goto reg_error;
	}

	down_write(&bcdev->state_sem);
	atomic_set(&bcdev->state, PMIC_GLINK_STATE_UP);
	bcdev->initialized = true;
	up_write(&bcdev->state_sem);

	bcdev->reboot_notifier.notifier_call = battery_chg_ship_mode;
	bcdev->reboot_notifier.priority = 255;
	register_reboot_notifier(&bcdev->reboot_notifier);

	bcdev->shutdown_notifier.notifier_call = battery_chg_shutdown;
	bcdev->shutdown_notifier.priority = 255;
	register_reboot_notifier(&bcdev->shutdown_notifier);

	rc = battery_chg_parse_dt(bcdev);
	if (rc < 0) {
		dev_err(dev, "Failed to parse dt rc=%d\n", rc);
		goto error;
	}

	bcdev->restrict_fcc_ua = DEFAULT_RESTRICT_FCC_UA;
	platform_set_drvdata(pdev, bcdev);
	bcdev->fake_soc = -EINVAL;
	rc = battery_chg_init_psy(bcdev);
	if (rc < 0)
		goto error;

	bcdev->battery_class.name = "qcom-battery";
	bcdev->battery_class.class_groups = battery_class_groups;
	rc = class_register(&bcdev->battery_class);
	if (rc < 0) {
		dev_err(dev, "Failed to create battery_class rc=%d\n", rc);
		goto error;
	}

	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	tcd = devm_thermal_of_cooling_device_register(dev, dev->of_node,
			(char *)pst->psy->desc->name, bcdev, &battery_tcd_ops);
	if (IS_ERR_OR_NULL(tcd)) {
		rc = PTR_ERR_OR_ZERO(tcd);
		dev_err(dev, "Failed to register thermal cooling device rc=%d\n",
			rc);
		class_unregister(&bcdev->battery_class);
		goto error;
	}

	bcdev->wls_fw_update_time_ms = WLS_FW_UPDATE_TIME_MS;
	battery_chg_add_debugfs(bcdev);
	bcdev->notify_en = false;
	battery_chg_notify_enable(bcdev);
	device_init_wakeup(bcdev->dev, true);
	schedule_work(&bcdev->usb_type_work);
	schedule_delayed_work(&bcdev->charger_debug_info_print_work, 5 * HZ);
	schedule_delayed_work(&bcdev->batt_update_work, 0);
	bcdev->debug_work_en = 1;
	dev->type = &dev_type_xiaomi_uevent;

	strcpy(bcdev->wireless_chip_fw_version, "00.00.00.00");
	strcpy(bcdev->wireless_tx_uuid_version, "00.00.00.00");
	bcdev->battery_auth = false;
	bcdev->slave_battery_auth = false;
	bcdev->slave_fg_verify_flag = false;
	bcdev->mtbf_current = 0;
	bcdev->reverse_chg_flag = 0;

#ifdef CONFIG_QTI_POGO_CHG
	dev_err(dev, "CONFIG_QTI_POGO_CHG\n");
	battmngr_device_register("qti_ops", bcdev->dev, bcdev, &qti_fg_ops, NULL);
	g_bcdev = bcdev;

	set_fg1_fastCharge(false);
	set_fg2_fastCharge(false);
	rc = qti_get_DCIN_STATE();
	pogo_flag = rc;
	dev_err(dev, "pogo_flag:%d\n", pogo_flag);
#endif

	dev_err(dev, "battery_chg probe done %d\n");
	return 0;
error:
	down_write(&bcdev->state_sem);
	atomic_set(&bcdev->state, PMIC_GLINK_STATE_DOWN);
	bcdev->initialized = false;
	up_write(&bcdev->state_sem);

	pmic_glink_unregister_client(bcdev->client);
	cancel_work_sync(&bcdev->usb_type_work);
	cancel_work_sync(&bcdev->subsys_up_work);
	cancel_work_sync(&bcdev->battery_check_work);
	complete(&bcdev->ack);
	unregister_reboot_notifier(&bcdev->reboot_notifier);
	unregister_reboot_notifier(&bcdev->shutdown_notifier);
reg_error:
	if (bcdev->notifier_cookie)
		panel_event_notifier_unregister(bcdev->notifier_cookie);
	return rc;
}

static int battery_chg_remove(struct platform_device *pdev)
{
	struct battery_chg_dev *bcdev = platform_get_drvdata(pdev);

	down_write(&bcdev->state_sem);
	atomic_set(&bcdev->state, PMIC_GLINK_STATE_DOWN);
	bcdev->initialized = false;
	up_write(&bcdev->state_sem);

	if (bcdev->notifier_cookie)
		panel_event_notifier_unregister(bcdev->notifier_cookie);

	device_init_wakeup(bcdev->dev, false);
	debugfs_remove_recursive(bcdev->debugfs_dir);
	class_unregister(&bcdev->battery_class);
	pmic_glink_unregister_client(bcdev->client);
	cancel_work_sync(&bcdev->subsys_up_work);
	cancel_work_sync(&bcdev->usb_type_work);
	cancel_work_sync(&bcdev->battery_check_work);
	unregister_reboot_notifier(&bcdev->reboot_notifier);
	unregister_reboot_notifier(&bcdev->shutdown_notifier);
	
	return 0;
}

static const struct of_device_id battery_chg_match_table[] = {
	{ .compatible = "qcom,battery-charger" },
	{},
};

static struct platform_driver battery_chg_driver = {
	.driver = {
		.name = "qti_battery_charger",
		.of_match_table = battery_chg_match_table,
	},
	.probe = battery_chg_probe,
	.remove = battery_chg_remove,
};
module_platform_driver(battery_chg_driver);

MODULE_DESCRIPTION("QTI Glink battery charger driver");
MODULE_LICENSE("GPL v2");
