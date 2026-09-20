// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/rpmsg.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/thermal.h>
#include <linux/soc/qcom/qti_pmic_glink.h>
#include <linux/soc/qcom/battery_charger.h>
#include <linux/soc/qcom/panel_event_notifier.h>
#include <linux/soc/qcom/fsa4480-i2c.h>
#include <linux/hardware_info.h>

#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/efi.h>
#include <linux/rcupdate.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_host.h>
#include <linux/completion.h>
#include "../../scsi/sd.h"
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/workqueue.h>
#include <linux/err.h>

/*add chg partion*/
#define CHARGER_WORK_INFO1_DELAY_MS            50000
#define CHARGER_WORK_DELAY_MS                  200
#define PARTITION_NAME                         "charger"
#define CHARGER_PARTITION_MAXSIZE              (0x10000)
#define CHARGER_PARTITION_RWSIZE               (0x1000)  /* read 1 block one time, a total of 256 blocks */
#define CHARGER_PARTITION_OFFSET               0
#define UFSHCD                                 "ufshcd"
#define PART_SECTOR_SIZE                       (0x200)
#define PART_BLOCK_SIZE                        (0x1000)
#define CHARGER_PARTITION_MAX_BLOCK_TRANSFERS  (128)
#define CHARGER_PARTITION_MAGIC                0x20240725

typedef enum
{
  CHARGER_PARTITION_HOST_LK,
  CHARGER_PARTITION_HOST_UEFI,
  CHARGER_PARTITION_HOST_ABL,
  CHARGER_PARTITION_HOST_ADSP,
  CHARGER_PARTITION_HOST_KERNEL,
  CHARGER_PARTITION_HOST_HAL,
  CHARGER_PARTITION_HOST_FRAMEWORK,
  CHARGER_PARTITION_HOST_APPLICATION,
  CHARGER_PARTITION_HOST_LAST,
  CHARGER_PARTITION_HOST_INVALID
} charger_partition_host_type;

/* a total of 256 blocks */
typedef enum
{
  CHARGER_PARTITION_HEADER,
  CHARGER_PARTITION_INFO_1,
  CHARGER_PARTITION_INFO_2,
  CHARGER_PARTITION_INFO_3,
  CHARGER_PARTITION_INFO_4,
  CHARGER_PARTITION_INFO_5,
  CHARGER_PARTITION_INFO_6,
  CHARGER_PARTITION_INFO_7,
  CHARGER_PARTITION_INFO_8,
  CHARGER_PARTITION_INFO_9,
  CHARGER_PARTITION_INFO_10,
  CHARGER_PARTITION_INFO_LAST,
  CHARGER_PARTITION_INFO_INVALID
} charger_partition_info_type;

enum charger_partition_prop_list {
  CHARGER_PARTITION_PROP_MISHOW,
  CHARGER_PARTITION_PROP_POWER_OFF_MODE,
  CHARGER_PARTITION_PROP_EU_MODE,
  CHARGER_PARTITION_PROP_IS_MISHOW_KPOC,
};

typedef struct
{
  u32   magic;     /* Check charger partition magic */
  u32   version;   /* Version 2 at 2024/07/25 */
  u32   initialized;
  u32   avaliable;
  u32   reserved;   /* reseved*/
} charger_partition_header;

typedef struct
{
  u32   power_off_mode;    /* If go into poweroff charging mode at this boot time */
  u32   zero_speed_mode;   /* If trigger zero speed mode at this boot time */
  u32   mishow;
  u32   is_mishow_kpoc; 
} charger_partition_info_1;

typedef struct
{
  u32   eu_mode;
  u32   test;
  u32   reserved;
} charger_partition_info_2;

#define DEFAULT_SDA_PART_NO (10)
struct ChargerPartition {
  struct scsi_device *sdev;
  struct delayed_work charger_partition_work;
  int part_info_part_number;
  char *part_info_part_name;
  //bool is_charger_partition_rdy;
};
static struct ChargerPartition *charger_partition;
static void* rw_buf;
static bool chg_parti_is_rdy = false;
typedef struct part {
  sector_t part_start;
  sector_t part_size;
} partinfo;
static partinfo part_info = { 0 };

/*add chg partion*/

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
#define BC_SHIP_MODE_REQ_SET		0x36
#define BC_WLS_FW_CHECK_UPDATE		0x40
#define BC_WLS_FW_PUSH_BUF_REQ		0x41
#define BC_WLS_FW_UPDATE_STATUS_RESP	0x42
#define BC_WLS_FW_PUSH_BUF_RESP		0x43
#define BC_WLS_FW_GET_VERSION		0x44
#define BC_SHUTDOWN_NOTIFY		0x47
#define BC_CHG_CTRL_LIMIT_EN		0x48
#define BC_XM_STATUS_GET		0x50
#define BC_XM_STATUS_SET		0x51
#define BC_HBOOST_VMAX_CLAMP_NOTIFY	0x79
#define BC_GENERIC_NOTIFY		0x80
#define BC_BATTERY_LPD_NOTIFY		0x9A
#define BC_BATTERY_LPD_RESET		0x9B

/* Generic definitions */
#define MAX_STR_LEN			128
#define BC_WAIT_TIME_MS			1000
#define WLS_FW_PREPARE_TIME_MS		1000
#define WLS_FW_WAIT_TIME_MS		500
#define WLS_FW_UPDATE_TIME_MS		1000
#define WLS_FW_BUF_SIZE			128
#define DEFAULT_RESTRICT_FCC_UA		1000000

#define BATTERY_DIGEST_LEN		32
#define BATTERY_DATE_LEN		8
#define BATTERY_SS_AUTH_DATA_LEN	4
#define USBPD_UVDM_SS_LEN		4
#define USBPD_UVDM_VERIFIED_LEN		1
#define USB_ICL_50MA	50000 /* 50mA */

#define DFX_CHARGE_WORK_TIME		5000 	//5s
#define DFX_FAST_CHARGE_WORK_TIME	1000 	//1s
#define DFX_DISCHARGE_WORK_TIME		10000 	//10s

#if IS_ENABLED(CONFIG_MIEV)
#include "miev/mievent.h"
#include <linux/string.h>
#endif

#define DFX_ID_CHG_PD_AUTHEN_FAIL              909001004
#define DFX_ID_CHG_CP_ENABLE_FAIL              909001005
#define DFX_ID_CHG_STANDARD_CHG_SLOW           909001006
#define DFX_ID_CHG_NONE_MI_PPS                 909001007

#define DFX_ID_CHG_NONE_STANDARD_CHG           909002001
#define DFX_ID_CHG_RP_SHORT_VBUS_DETECTED      909002002
#define DFX_ID_CHG_LPD_DETECTED                909002003
#define DFX_ID_CHG_CP_VBUS_OVP                 909002004
#define DFX_ID_CHG_CP_IBUS_OCP                 909002005
#define DFX_ID_CHG_CP_VBAT_OVP                 909002006
#define DFX_ID_CHG_CP_IBAT_OCP                 909002007

#define DFX_ID_CHG_BATTERY_CYCLECOUNT          909003001
#define DFX_ID_CHG_UISOC_NOT_FULL              909003002
#define DFX_ID_CHG_SMART_ENDURANCE_TRIGGERED   909003004
#define DFX_ID_CHG_SMART_NAVIGATION_TRIGGERED  909003006

#define DFX_ID_CHG_BATT_IIC_ERR                909005001
#define DFX_ID_CHG_CP_ABSENT                   909005002
#define DFX_ID_CHG_BATT_LINKER_ABSENT          909005003
#define DFX_ID_CHG_LOW_TEMP_DISCHARGING        909005007
#define DFX_ID_CHG_HIGH_TEMP_DISCHARGING       909005008

#define DFX_ID_CHG_SMART_ENDURANCE_SOC_ERR     909006010
#define DFX_ID_SMART_NAVIGATION_SOC_ERR        909006011

#define DFX_ID_CHG_BATTERY_AUTH_FAIL           909007001

#define DFX_ID_CHG_BATTERY_TEMP_HOT            909009001
#define DFX_ID_CHG_BATTERY_TEMP_COLD           909009002
#define CHG_DFX_DATA_LEN                       5

struct dfs_data_battery {
	int vbat;
	int uisoc;
	int rawsoc;
	int cycle;
	unsigned int adapter_id;
};

struct dfs_data_battery dfs_data_batt;

static const char *const xm_dfx_chg_report_text[][2] = { \
	{"DEFAULT_NAME", "DEFAULT_TEXT"}, \
	{"chgErrInfo", "FgI2cErr"}, \
	{"chgErrInfo", "BattLinkerAbsent"}, \
	{"chgErrInfo", "LpdDetected"}, \
	{"chgErrInfo", "NoneStandartChg"}, \
	{"chgErrInfo", "CorrosionDischarge"}, \
	{"chgErrInfo", "BattAuthFail"}, \
	{"chgStatInfo", "chgBattCycle"}, \
	{"chgErrInfo", "SocNotFull"}, \
	{"chgErrInfo", "CpVbusOvp"}, \
	{"chgErrInfo", "CpIbusOcp"}, \
	{"chgErrInfo", "CpVbatOvp"}, \
	{"chgErrInfo", "CpIbatOcp"}, \
	{"chgErrInfo", "CpEnFail"}, \
	{"chgErrInfo", "CpI2cErr"}, \
	{"chgStatInfo", "SmartNaviTrig"}, \
	{"chgErrInfo", "SmartNaviSocErr"}, \
	{"chgStatInfo", "SmartEnduraTrig"}, \
	{"chgErrInfo", "SmartEnduraSocErr"}, \
	{"chgErrInfo", "NotChgInLowTemp"}, \
	{"chgErrInfo", "NotChgInHighTemp"}, \
	{"chgStatInfo", "TbatHot"}, \
	{"chgStatInfo", "TbatCold"}, \
	{"chgErrInfo", "PdAuthFail"}, \
	{"chgErrInfo", "StandarChgSlow"}, \
	{"chgErrInfo", "NoneMiPPS"},
};

enum xm_chg_dfx_type {
	CHG_DFX_DEFAULT,
	CHG_DFX_FG_IIC_ERR,
	CHG_DFX_BATT_LINKER_ABSENT,
	CHG_DFX_LPD_DETECTED,
	CHG_DFX_NONE_STANDARD_CHG,
	CHG_DFX_RP_SHORT_VBUS_DETECTED,
	CHG_DFX_BATTERY_AUTH_FAIL,
	CHG_DFX_BATTERY_CYCLECOUNT,
	CHG_DFX_UISOC_NOT_FULL,
	CHG_DFX_CP_VBUS_OVP,
	CHG_DFX_CP_IBUS_OCP,
	CHG_DFX_CP_VBAT_OVP,
	CHG_DFX_CP_IBAT_OCP,
	CHG_DFX_CP_ENABLE_FAIL,
	CHG_DFX_CP_ABSENT,
	CHG_DFX_NAVIGATION,
	CHG_DFX_NAVIGATION_OVER_SOC,
	CHG_DFX_SMART_ENDURANCE_TRIGGERED,
	CHG_DFX_SMART_ENDURANCE_SOC_ERR,
	CHG_DFX_LOW_TEMP_DISCHARGING,
	CHG_DFX_HIGH_TEMP_DISCHARGING,
	CHG_DFX_BATTERY_TEMP_HOT,
	CHG_DFX_BATTERY_TEMP_COLD,
	CHG_DFX_PPS_AUTH_FAIL,
	CHG_DFX_STANDARD_CHG_SLOW,
	CHG_DFX_NONE_MI_PPS,
	CHG_DFX_MAX_INDEX,
};

enum psy_type {
	PSY_TYPE_BATTERY,
	PSY_TYPE_USB,
	PSY_TYPE_WLS,
	PSY_TYPE_XM,
	PSY_TYPE_MAX,
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
	BATT_CHG_CTRL_EN,
	BATT_CHG_CTRL_START_THR,
	BATT_CHG_CTRL_END_THR,
	BATT_CURR_AVG,
	BATT_CAPACITY_LEVEL,
	BATT_PARALLEL_CELL_COUNT,
	BATT_POWEROFF_VOLT,
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
	F_ACTIVE,
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

enum {
	USBPD_PE_STATE_PE_SRC_READY = 5,    /*PE_SRC_Ready*/
	USBPD_PE_STATE_PE_SNK_STARTUP = 25, /*PE_SNK_Startup*/
	USBPD_PE_STATE_PE_SNK_READY = 31,   /*PE_SNK_Ready*/
};

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

enum xm_property_id {
	/* Typec sys node */
	XM_PROP_TYPEC_MODE,
	XM_PROP_CC_ORIENTATION,
	XM_PROP_OTG_UI_SUPPORT,
	XM_PROP_CID_STATUS,
	XM_PROP_CC_TOGGLE,
	XM_PROP_SCREEN_CCTOG,
	XM_PROP_AUDIO_CCTOG,
	/* PD authentic*/
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
	XM_PROP_APDO_MAX,
	XM_PROP_POWER_MAX,
	/* Charge Pump */
	XM_PROP_CP_BUS_CURRENT,
	XM_PROP_PUMP_INFO,
	XM_PROP_CP_MANUFACTURE,
	/* Main charger */
	XM_PROP_INPUT_SUSPEND,
	XM_PROP_SHIPMODE_COUNT_RESET,
	XM_PROP_REAL_TYPE,
	XM_PROP_MTBF_CURRENT,
	/* Battery & Fuel Gauge */
	XM_PROP_BATT_ID,
	XM_PROP_CHIP_OK,
	XM_PROP_VERIFY_DIGEST,
	XM_PROP_MANUFACTURING_DATA,
	XM_PROP_SOH_SN,
	XM_PROP_UI_SOH,
	XM_PROP_FIRST_USAGE_DATE,
	XM_PROP_SOH_NEW,
	XM_PROP_AUTHENTIC,
	XM_PROP_FG1_SOH,
	XM_PROP_CIS_ALERT_LEVEL,
	XM_PROP_CLEAR_CYCLE,
	XM_PROP_CLEAR_CYCLE_FLAG,
	XM_PROP_SOA_ALERT_LEVEL,
	XM_PROP_EU_MODE,
	XM_PROP_FG1_RM,
	XM_PROP_RESISTANCE_ID,
	XM_PROP_FAKE_SOC,
	XM_PROP_FAKE_TEMP,
	XM_PROP_FAKE_CYCLE,
	XM_PROP_SHUTDOWN_DELAY,
	XM_PROP_RAW_SOC,
	XM_PROP_BATT_DOD_COUNT,
	XM_PROP_DF_CHECK,
	XM_PROP_CHEM_DF_SIGN,
	XM_PROP_FG1_CYCLE,
	/* Smart charge */
	XM_PROP_SMART_BATT,
	XM_PROP_SMART_FV,
	XM_PROP_THERMAL_BOARD_TEMP,
	XM_PROP_SMART_CHG,
	XM_PROP_NIGHT_CHARGING,
	/* Other */
	XM_PROP_ATO_SOC_USER_CONTROL,
	XM_PROP_REVERSE_QUICK_CHARGE,
	XM_PROP_FORCE_ROLE,
	XM_PROP_QUICK_CHARGE_TYPE,
	XM_PROP_SOC_DECIMAL,
	XM_PROP_SOC_DECIMAL_RATE,
	XM_PROP_FAST_CHG_MODE,
	XM_PROP_LPD_STATUS,
	XM_PROP_LPD_CHARGING,
	XM_POWER_TEMP_RAW,
	XM_POWER_SUPPLY_PROP_CHG_DEBUG,
	XM_PROP_REVERSE_QUICK_CHARGE_STATUS,
	XM_PROP_REVCHG_BCL,
	XM_PROP_BATT_IIC_STATE,
	XM_PROP_CC_SHORT_VBUS,
	XM_POWER_TEMP_DEBUG,
	XM_PROP_THERMAL_SCENE,
	XM_PROP_IS_MISHOW_KPOC,
	XM_PROP_BATT_IS_GLOBAL,
	XM_PROP_IS_CP_ENABLED,
	XM_PROP_CALC_RVALUE,
	XM_PROP_MAX,
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

struct xm_verify_digest_resp_msg {
	struct pmic_glink_hdr	hdr;
	u32			property_id;
	u8			data[BATTERY_DIGEST_LEN];
};

struct xm_ss_auth_resp_msg {
	struct pmic_glink_hdr	hdr;
	u32			property_id;
	u32			data[BATTERY_SS_AUTH_DATA_LEN];
};

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

struct battery_charger_chg_ctrl_msg {
	struct pmic_glink_hdr	hdr;
	u32			enable;
	u32			target_soc;
	u32			delta_soc;
};

struct chg_dfx_report_msg {
	struct pmic_glink_hdr   hdr;
	u32                     property_id;
	u8                      type;
	int            data[CHG_DFX_DATA_LEN];
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
	int				num_thermal_levels;
	int				shutdown_volt_mv;
	int				lpd_enable;
	int				lpd_status;
	int				lpd_charging;
	int				lpd_control;
	int				batt_authentic;
	int				chip_ok;
	int			 	batt_id;
	int				cp_index;
	atomic_t			state;
	struct work_struct		subsys_up_work;
	struct work_struct		usb_type_work;
	struct delayed_work		lpd_detection_work;
	struct delayed_work		mipps_check_work;
	struct delayed_work		standar_chg_slow_work;
	struct delayed_work		update_poweroff_chg_icl_work;
	struct work_struct		battery_check_work;
	int				fake_soc;
	bool				block_tx;
	bool				ship_mode_en;
	bool				ship_mode_immediate;
	bool				debug_battery_detected;
	bool				wls_fw_update_reqd;
	u32				wls_fw_version;
	u16				wls_fw_crc;
	u32				wls_fw_update_time_ms;
	struct notifier_block		reboot_notifier;
	u32				thermal_fcc_ua;
	u32				restrict_fcc_ua;
	u32				last_fcc_ua;
	u32				usb_icl_ua;
	u32				thermal_fcc_step;
	bool				restrict_chg_en;
	u8				chg_ctrl_start_thr;
	u8				chg_ctrl_end_thr;
	bool				chg_ctrl_en;
	/* To track the driver initialization status */
	bool				initialized;
	bool				notify_en;
	bool				error_prop;
	int				    mtbf_current;
	bool				shipmode_en;
	bool				shutdown_delay_en;
	struct delayed_work		xm_pump_info_work;
	struct delayed_work		xm_prop_change_work;
	u8				*digest;
	u8				*manufacturing_date;
	u8				*soh_sn_data;
	u8				*manuInfoC_data;
	u32				*ss_auth_data;
	int				screen_state;
	/*dfx*/
	struct delayed_work		dfx_monitor_work;
	int				dfx_cycle_count_flag;
	int				dfx_batt_auth_flag;
	int				dfx_chip_ok_check_flag;
	int				dfx_lpd_check_flag;
	int                             dfx_cp_ibus_ocp_flag;
	u16				dfx_work_delay;
	int				i2c_state;
	int				pd_verifed_dfx;
	int				mah_pre;
	int				threshold_mah;
};

static bool mi_pps_dfx_flag = true;
static bool standar_chg_slow_flag = true;
static bool dfx_first_check_flag = false;

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
	[BATT_CHG_CTRL_START_THR] = POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD,
	[BATT_CHG_CTRL_END_THR]   = POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD,
	[BATT_CURR_AVG]		= POWER_SUPPLY_PROP_CURRENT_AVG,
	[BATT_CAPACITY_LEVEL]	= POWER_SUPPLY_PROP_CAPACITY_LEVEL,
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
};

static const int xm_prop_map[XM_PROP_MAX] = {};

static const char * const batt_manufacturer_text[] = {
	"Unknown",
	"BM79_SWD",
	"BM79_COS",
	"BM7A_NVT",
	"BM7A_SWD",
};

static const char * const battery_type_text[] = {
	"Unknown",
	"BM79_SWD-8000mAh",
	"BM79_COS-8000mAh",
	"BM7A_NVT-7700mAh",
	"BM7A_SWD-7700mAh",
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

/* Standard usb_type definitions similar to power_supply_sysfs.c */
static const char * const power_supply_usb_type_text[] = {
	"Unknown", "USB", "USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
	"USB_PD", "PD_DRP", "PD_PPS", "BrickID"
};

/* Custom usb_type definitions */
static const char * const qc_power_supply_usb_type_text[] = {
	"USB_HVDCP", "USB_HVDCP_3", "USB_HVDCP3P5", "USB_FLOAT", "USB_HVDCP_3"
};

/* wireless_type definitions */
static const char * const qc_power_supply_wls_type_text[] = {
	"Unknown", "BPP", "EPP", "HPP"
};

static const char * const usbpd_state_strings[] = {
	"UNKNOWN",
	"SNK_Startup",
	"SNK_Ready",
	"SRC_Ready",
};

/* cp manufacturer definitions */
static const char * const cp_manufacturer_text[] = {
	"Unknown", "SC8531", "BQ25960H"
};

static struct battery_chg_dev *g_bcdev;
static int usb_psy_set_icl(struct battery_chg_dev *bcdev, u32 prop_id, int val);

static int charger_scsi_read_partition(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t blocks)
{
  uint8_t cdb[16];
  int ret = 0;
  struct scsi_sense_hdr sshdr = {};
  const struct scsi_exec_args exec_args = {
    .sshdr = &sshdr,
  };
  unsigned long flags = 0;

  spin_lock_irqsave(sdev->host->host_lock, flags);
  ret = scsi_device_get(sdev);
  if (!ret && !scsi_device_online(sdev)) {
    ret = -ENODEV;
    scsi_device_put(sdev);
    pr_err("chargerPartition %s get device fail\n", __func__);
  }
  spin_unlock_irqrestore(sdev->host->host_lock, flags);
  if (ret) {
    pr_err("chargerPartition %s 2\n", __func__);
    return ret;
  }
  sdev->host->eh_noresume = 1;

  // Fill in the CDB with SCSI command structure
  memset (cdb, 0, sizeof(cdb));
  cdb[0] = READ_10;               // Command
  cdb[1] = 0;
  cdb[2] = (lba >> 24) & 0xff;    // LBA
  cdb[3] = (lba >> 16) & 0xff;
  cdb[4] = (lba >> 8) & 0xff;
  cdb[5] = (lba) & 0xff;
  cdb[6] = 0;                     // Group Number
  cdb[7] = (blocks >> 8) & 0xff;  // Transfer Len
  cdb[8] = (blocks) & 0xff;
  cdb[9] = 0;                     // Control
#if 0
  int scsi_execute_cmd(struct scsi_device *sdev, const unsigned char *cmd, blk_opf_t opf, void *buffer, unsigned int bufflen,
          int timeout, int retries, const struct scsi_exec_args *args);

  extern int __scsi_execute(struct scsi_device *sdev, const unsigned char *cmd, int data_direction, void *buffer, unsigned bufflen,
      unsigned char *sense, struct scsi_sense_hdr *sshdr, int timeout, int retries, u64 flags,
      req_flags_t rq_flags, int *resid);
#endif
  ret = scsi_execute_cmd(sdev, cdb, REQ_OP_DRV_IN, buf, (blocks * PART_BLOCK_SIZE), HZ, 2, &exec_args);
  //ret = scsi_execute(sdev, cdb, DMA_FROM_DEVICE, buf, (blocks * PART_BLOCK_SIZE),NULL, &sshdr, HZ, 2, 0,0, 0);
  if (ret) {
    pr_err("chargerPartition %s read error %d\n", __func__, ret);
    return ret;
  }
  scsi_device_put(sdev);
  sdev->host->eh_noresume = 0;
  return ret;
}

static int charger_scsi_write_partition(struct scsi_device *sdev, void *buf, uint32_t lba, uint32_t blocks)
{
  uint8_t cdb[16];
  int ret = 0;
  struct scsi_sense_hdr sshdr = {};
  const struct scsi_exec_args exec_args = {
    .sshdr = &sshdr,
  };
  unsigned long flags = 0;

  spin_lock_irqsave(sdev->host->host_lock, flags);
  ret = scsi_device_get(sdev);
  if (!ret && !scsi_device_online(sdev)) {
    ret = -ENODEV;
    scsi_device_put(sdev);
    pr_err("chargerPartition %s get device fail\n", __func__);
  }
  spin_unlock_irqrestore(sdev->host->host_lock, flags);

  if (ret) {
    pr_err("chargerPartition %s get scsi fail\n", __func__);
    return ret;
  }
  sdev->host->eh_noresume = 1;

  // Fill in the CDB with SCSI command structure
  memset (cdb, 0, sizeof(cdb));
  cdb[0] = WRITE_10;              // Command
  cdb[1] = 0;
  cdb[2] = (lba >> 24) & 0xff;    // LBA
  cdb[3] = (lba >> 16) & 0xff;
  cdb[4] = (lba >> 8) & 0xff;
  cdb[5] = (lba) & 0xff;
  cdb[6] = 0;                     // Group Number
  cdb[7] = (blocks >> 8) & 0xff;  // Transfer Len
  cdb[8] = (blocks) & 0xff;
  cdb[9] = 0;                     // Control
  ret = scsi_execute_cmd(sdev, cdb, REQ_OP_DRV_OUT, buf, (blocks * PART_BLOCK_SIZE), HZ, 2, &exec_args);
  //ret = scsi_execute(sdev, cdb, DMA_TO_DEVICE, buf, (blocks * PART_BLOCK_SIZE),NULL, &sshdr, HZ, 2, 0,0, 0);
  if (ret) {
    pr_err("chargerPartition %s write error %d\n", __func__, ret);
    return ret;
  }
  scsi_device_put(sdev);
  sdev->host->eh_noresume = 0;
  pr_err("chargerPartition debug write done");
  return ret;
}

int charger_partition_alloc(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size)
{
  int ret = 0;

  if (!chg_parti_is_rdy) {
    pr_err("chargerPartition charger partition not rdy, can't do rw!");
    return -1;
  }
  if (charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
    pr_err("chargerPartition charger_partition_host_type not support!");
    return -1;
  }
  if (charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
    pr_err("chargerPartition charger_partition_info_type not support!");
    return -1;
  }
  if (size >= CHARGER_PARTITION_RWSIZE) {
    pr_err("chargerPartition read %u size not support!", size);
    return -1;
  }

  rw_buf = kzalloc(CHARGER_PARTITION_RWSIZE, GFP_KERNEL);
  if (!rw_buf) {
    pr_err("chargerPartition malloc buf error!");
    return -1;
  }

  /* check if avaliable */
  ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
  if (ret) {
    pr_err("chargerPartition %s charger read error %d\n", __func__, ret);
    kfree(rw_buf);
    return -1;
  }
  pr_err("chargerPartition %s avaliable:%u\n", __func__, ((charger_partition_header *)rw_buf)->avaliable);
  if (0 == ((charger_partition_header *)rw_buf)->avaliable) {
    pr_err("chargerPartition %s not avaliable, can't do rw now!\n", __func__);
    kfree(rw_buf);
    return -1;
  }
  ((charger_partition_header *)rw_buf)->avaliable = 0;
  pr_err("chargerPartition %s set avaliable:%u\n", __func__, ((charger_partition_header *)rw_buf)->avaliable);
  ret = charger_scsi_write_partition(charger_partition->sdev, (void *)rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
  if (ret) {
    pr_err("chargerPartition %s charger write error %d\n", __func__, ret);
    kfree(rw_buf);
    return -1;
  }

  pr_err("chargerPartition debug alloc done");
  return ret;
}
EXPORT_SYMBOL(charger_partition_alloc);

int charger_partition_dealloc(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size)
{
  int ret = 0;

  if (!chg_parti_is_rdy) {
    pr_err("chargerPartition charger partition not rdy, can't do rw!");
    return -1;
  }
  if (charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
    pr_err("chargerPartition charger_partition_host_type not support!");
    return -1;
  }
  if (charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
    pr_err("chargerPartition charger_partition_info_type not support!");
    return -1;
  }
  if (size >= CHARGER_PARTITION_RWSIZE) {
    pr_err("chargerPartition read %u size not support!", size);
    return -1;
  }

  memset(rw_buf, 0, CHARGER_PARTITION_RWSIZE);
  ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
  if (ret) {
    pr_err("chargerPartition %s charger read error %d\n", __func__, ret);
    kfree(rw_buf);
    return -1;
  }
  ((charger_partition_header *)rw_buf)->avaliable = 1;
  pr_err("chargerPartition %s set avaliable:%u\n", __func__, ((charger_partition_header *)rw_buf)->avaliable);
  ret = charger_scsi_write_partition(charger_partition->sdev, (void *)rw_buf, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
  if (ret) {
    pr_err("chargerPartition %s charger write error %d\n", __func__, ret);
    kfree(rw_buf);
    return -1;
  }

  kfree(rw_buf);
  return ret;
}
EXPORT_SYMBOL(charger_partition_dealloc);

void *charger_partition_read(u8 charger_partition_host_type, u8 charger_partition_info_type, uint32_t size)
{
  int ret = 0;

  if (!chg_parti_is_rdy) {
    pr_err("chargerPartition charger partition not rdy, can't do read!");
    return NULL;
  }
  if (!rw_buf) {
    pr_err("chargerPartition rw_buf null, please alloc first!");
    return NULL;
  }
  if (charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
    pr_err("chargerPartition charger_partition_host_type not support!");
    return NULL;
  }
  if (charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
    pr_err("chargerPartition charger_partition_info_type not support!");
    return NULL;
  }
  if (size >= CHARGER_PARTITION_RWSIZE) {
    pr_err("chargerPartition read %u size not support!", size);
    return NULL;
  }

  memset(rw_buf, 0, CHARGER_PARTITION_RWSIZE);
  ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + charger_partition_info_type), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
  if (ret) {
    pr_err("chargerPartition %s charger read error %d\n", __func__, ret);
    return NULL;
  }
  return rw_buf;
}
EXPORT_SYMBOL(charger_partition_read);

int charger_partition_write(u8 charger_partition_host_type, u8 charger_partition_info_type, void *buf, uint32_t size)
{
  int ret = 0;
  uint32_t offset;

  pr_err("chargerPartition debug write start");
  if (!chg_parti_is_rdy) {
    pr_err("chargerPartition charger partition not rdy, can't do read!");
    return -1;
  }
  if (!rw_buf) {
    pr_err("chargerPartition rw_buf null, please alloc first!");
    return -1;
  }
  if (charger_partition_host_type >= CHARGER_PARTITION_HOST_LAST) {
    pr_err("chargerPartition charger_partition_host_type not support!");
    return -1;
  }
  if (charger_partition_info_type >= CHARGER_PARTITION_INFO_LAST) {
    pr_err("chargerPartition charger_partition_info_type not support!");
    return -1;
  }
  if (size >= CHARGER_PARTITION_RWSIZE) {
    pr_err("chargerPartition read %u size not support!", size);
    return -1;
  }
/*
  memset(rw_buf, 0, CHARGER_PARTITION_RWSIZE);
  ret = charger_scsi_read_partition(charger_partition->sdev, rw_buf, (part_info.part_start + charger_partition_info_type), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
  if (ret) {
    pr_err("chargerPartition %s charger read error %d\n", __func__, ret);
    return -1;
  }
*/
  memcpy(rw_buf, buf, size);

  ret = charger_scsi_write_partition(charger_partition->sdev, rw_buf, (part_info.part_start + charger_partition_info_type), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
  if (ret) {
    pr_err("chargerPartition %s charger write error %d\n", __func__, ret);
    return -1;
  }

  offset = part_info.part_start + charger_partition_info_type;
  pr_err("chargerPartition %s: start: %llu, info_type:%d, offset:%u \n", __func__, part_info.part_start, charger_partition_info_type, offset);
  return ret;
}
EXPORT_SYMBOL(charger_partition_write);

static bool look_up_scsi_device(int lun)
{
  struct Scsi_Host *shost;

  shost = scsi_host_lookup(0);
  if (!shost)
    return false;

  charger_partition->sdev = scsi_device_lookup(shost, 0, 0, lun);
  if (!charger_partition->sdev)
    return false;

  scsi_host_put(shost);
  return true;
}

static bool check_device_is_correct(void)
{
  pr_err("chargerPartition %s proc name is %s", __func__, charger_partition->sdev->host->hostt->proc_name);
  if (strncmp(charger_partition->sdev->host->hostt->proc_name, UFSHCD, strlen(UFSHCD))) {
    /*check if the device is ufs. If not, just return directly.*/
    pr_err("chargerPartition %s proc name is not ufshcd, name: %s", __func__, charger_partition->sdev->host->hostt->proc_name);
    return false;
  }

  return true;
}

static bool get_charger_partition_info(void)
{
  struct scsi_disk *sdkp = NULL;
  struct scsi_device *sdev = charger_partition->sdev;
  int part_number = charger_partition->part_info_part_number;
  struct block_device *part;

  if (!sdev->sdev_gendev.driver_data) {
    pr_err("chargerPartition %s scsi disk is null\n", __func__);
    return false;
  }

  sdkp = (struct scsi_disk *)sdev->sdev_gendev.driver_data;
  if (!sdkp->disk) {
    pr_err("chargerPartition %s gendisk is null\n", __func__);
    return false;
  }

  charger_partition->part_info_part_name = sdkp->disk->disk_name;
  pr_err("chargerPartition %s partion: %s-%d \n",
      __func__, charger_partition->part_info_part_name, part_number);

  part = xa_load(&sdkp->disk->part_tbl, part_number);
  if (!part) {
    pr_err("chargerPartition %s device is null\n", __func__);
    return false;
  }

  pr_err("[ChgPartition] partition name %s\n",part->bd_meta_info->volname);
  if (strncmp(part->bd_meta_info->volname, "charger", sizeof("charger"))) {
    pr_err("[ChgPartition] this is not charger partition\n");
//    return false;
  }
  part_info.part_start = part->bd_start_sect * PART_SECTOR_SIZE / PART_BLOCK_SIZE;
  part_info.part_size = bdev_nr_sectors(part) * PART_SECTOR_SIZE / PART_BLOCK_SIZE;

  pr_err("chargerPartition %s partion: %s start %llu(block) size %llu(block)\n",
      __func__, charger_partition->part_info_part_name, part_info.part_start, part_info.part_size);
  return true;
}

int get_hwc_from_cmdline(char *hwc)
{
  struct device_node *of_chosen = NULL;
  char *bootargs = NULL;
  char androidboot[24];
  char *phwc = NULL;

  of_chosen = of_find_node_by_path("/chosen");
  if (of_chosen) {
    bootargs = (char *)of_get_property(of_chosen, "bootargs", NULL);
    if (!bootargs) {
      pr_err("%s search bootargs property failed\n", __func__);
    } else {
      phwc = strstr(bootargs, "androidboot.hwc");
      if (phwc) {
        memset(androidboot, 0, sizeof(androidboot));
        strncpy(androidboot, phwc, sizeof(androidboot));
        sscanf(androidboot, "androidboot.hwc=%s", hwc);
        //pr_err("hwc is %s \n", *hwc);
      } else {
        pr_err("search androidboot.hwc failed\n");
        return -EINVAL;
      }
    }
  } else {
    pr_err("search chosen node failed\n");
    return -EINVAL;
  }

  return 0;
}

int check_charger_partition_header(void)
{
  int ret = 0;
  charger_partition_header *header = NULL;

  header = kzalloc(CHARGER_PARTITION_RWSIZE, GFP_KERNEL);
  if (!header) {
    pr_err("chargerPartition malloc buf error!");
    return -1;
  }

  ret = charger_scsi_read_partition(charger_partition->sdev, (void *)header, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
  if (ret) {
    pr_err("chargerPartition %s charger read error %d\n", __func__, ret);
    kfree(header);
    return -1;
  }
  pr_err("chargerPartition %s magic:0x%0x, version:%d\n", __func__, header->magic, header->version);

  if (header->magic != CHARGER_PARTITION_MAGIC) {
    pr_err("chargerPartition %s magic error, set to default!\n", __func__);
    header->magic = CHARGER_PARTITION_MAGIC;
  }
  header->initialized = 1;
  header->avaliable = 1;
  pr_err("chargerPartition %s initiablized ok\n", __func__);

  ret = charger_scsi_write_partition(charger_partition->sdev, (void *)header, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
  if (ret) {
    pr_err("chargerPartition %s charger write error %d\n", __func__, ret);
    kfree(header);
    return -1;
  }

  ret = charger_scsi_read_partition(charger_partition->sdev, (void *)header, (part_info.part_start + CHARGER_PARTITION_HEADER), CHARGER_PARTITION_RWSIZE / PART_BLOCK_SIZE);
  if (ret) {
    pr_err("chargerPartition %s charger read error %d\n", __func__, ret);
    kfree(header);
    return -1;
  }
  pr_err("chargerPartition %s magic:0x%0x, version:%d\n", __func__, header->magic, header->version);

  kfree(header);
  return ret;
}

int get_charger_partition_info_1(void)
{
  int ret = 0;
  charger_partition_info_1 *info_1 = NULL;

  ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
  if (ret < 0) {
    pr_err("chargerPartition %s failed to alloc\n", __func__);
    return -1;
  }

  info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
  if (!info_1) {
    pr_err("chargerPartition %s failed to read\n", __func__);
    ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if (ret < 0) {
      pr_err("chargerPartition %s failed to dealloc\n", __func__);
      return -1;
    }
    return -1;
  }
  pr_err("ChgPartition %s ret: %d, info_1->mishow: 0x%0x, zero_speed_mode: %u, power_off_mode: %u\n", __func__, ret, info_1->mishow, info_1->zero_speed_mode, info_1->power_off_mode);

  ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
  if (ret < 0) {
    pr_err("chargerPartition %s failed to dealloc\n", __func__);
    return -1;
  }
  return 0;
}

int set_charger_partition_info_1(void)
{
  int ret = 0;
  charger_partition_info_1 *info_1 = NULL;

  ret = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
  if (ret < 0) {
    pr_err("chargerPartition %s failed to alloc\n", __func__);
    return -1;
  }

  info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
  if(!info_1) {
    pr_err("[ChgPartition] failed to read\n");
    ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if(ret < 0) {
        pr_err("[ChgPartition] failed to dealloc\n");
        return -1;
    }

    return -1;
  }

  pr_err("chargerPartition power_off_mode=%u,zero_speed_mode=%u\n",info_1->power_off_mode,info_1->zero_speed_mode);
  if (info_1->power_off_mode == 1 && g_bcdev) {
    schedule_delayed_work(&g_bcdev->update_poweroff_chg_icl_work, msecs_to_jiffies(5000));
    pr_err("%s is poweroff mode\n", __func__);
  }

  info_1->power_off_mode = 2;
  info_1->zero_speed_mode = 2;
  ret = charger_partition_write(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, (void *)info_1, sizeof(charger_partition_info_1));
  if (ret < 0) {
    pr_err("chargerPartition %s failed to write\n", __func__);
    ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if (ret < 0) {
      pr_err("chargerPartition %s failed to dealloc\n", __func__);
      return -1;
    }
    return -1;
  }

  ret = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
  if (ret < 0) {
    pr_err("chargerPartition %s failed to dealloc\n", __func__);
    return -1;
  }
  pr_err("chargerPartition %s return\n", __func__);
  return 0;
}

int charger_partition_get_prop(int type, int *val);
static int write_property_id(struct battery_chg_dev *bcdev, struct psy_state *pst, u32 prop_id, u32 val);
int charger_partition_get_is_mishow_kpoc(void);
static int eu_mode = 0;
static void charger_partition_work(struct work_struct *work)
{
  int lun = 0, ret = 0;
  char HWC[16] = { 0 };
  int is_mishow_kpoc;

  ret = get_hwc_from_cmdline(HWC);
  if (!ret) {
    pr_info("chargerPartition %s get HWC success: %s\n", __func__, HWC);
    if (!strncmp(HWC, "CN", 2)) {
      pr_info("chargerPartition %s CN machine \n", __func__);
      charger_partition->part_info_part_number = DEFAULT_SDA_PART_NO;
    } else if(!strncmp(HWC, "India", 5)) {
      pr_info("chargerPartition %s Indial machine \n", __func__);
      charger_partition->part_info_part_number = DEFAULT_SDA_PART_NO;
    } else if(!strncmp(HWC, "Global", 5)) {
      pr_info("chargerPartition %s Global machine %d \n", __func__,charger_partition->part_info_part_number);
      charger_partition->part_info_part_number = DEFAULT_SDA_PART_NO;
    } else {
      pr_info("chargerPartition %s other machine \n", __func__);
      charger_partition->part_info_part_number = DEFAULT_SDA_PART_NO;
    }
  } else {
    pr_err("chargerPartition %s get HWC failed, use defalut SDA part_no\n", __func__);
    charger_partition->part_info_part_number = DEFAULT_SDA_PART_NO;
  }

  //1. find charger partition
  for (lun = 0; lun < 6; lun++) {
    /*find charger partition scsi device*/
    if (!look_up_scsi_device(lun)) {
      pr_err("chargerPartition %s not find, continue...\n", __func__);
      continue;
    }

    /*check device is correct*/
    if (!check_device_is_correct()) {
      pr_err("chargerPartition %s not find finally, won't read charger partition!!!\n",
        __func__);
      return;
    }

    if (get_charger_partition_info()) {
      pr_info("chargerPartition %s get partition info ok\n", __func__);
      chg_parti_is_rdy = true;
      check_charger_partition_header();
      /*reset info_1: power_off_mode and zero_speed_mode*/
      get_charger_partition_info_1();
      set_charger_partition_info_1();
      get_charger_partition_info_1();
      ret = charger_partition_get_prop(CHARGER_PARTITION_PROP_EU_MODE, &eu_mode);
      is_mishow_kpoc = charger_partition_get_is_mishow_kpoc();
      if (g_bcdev){
        ret = write_property_id(g_bcdev, &g_bcdev->psy_list[PSY_TYPE_XM], XM_PROP_IS_MISHOW_KPOC, is_mishow_kpoc);
        if (ret < 0) {
          pr_err("XM_PROP_IS_MISHOW_KPOC 2 rc = %d fail\n", ret);
        } else {
          pr_info("[charger] %s set XM_PROP_IS_MISHOW_KPOC ok 2, is_mishow_kpoc:%d\n", __func__, is_mishow_kpoc);
        }
      }
      
      if (ret < 0) {
        pr_err("%s: get prop failed, ret = %d \n", __func__, ret);
        return;
      }
      break;
    }
  }
}

int charger_partition_set_prop(int type, int val)
{
  int rc = 0;
  charger_partition_info_1 *info_1 = NULL;
  charger_partition_info_2 *info_2 = NULL;
  if (!chg_parti_is_rdy) {
    pr_err("[ChgPartition] charger partition not rdy, can't set prop!");
    return -EINVAL;
  }
  pr_err("[ChgPartition] %s: set %d to %u \n", __func__, type, val);
  switch(type)
  {
    case CHARGER_PARTITION_PROP_MISHOW:
      rc = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to alloc\n", __func__);
        return -EINVAL;
      }
      info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		  if(!info_1) {
			  pr_err("[ChgPartition] failed to read\n");
			  return -1;
		  }
		  info_1->mishow = val;
      pr_err("ChgPartition debug info_1: %p, info_1->mishow:%d\n", info_1, info_1->mishow);
      rc = charger_partition_write(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, (void *)info_1, sizeof(charger_partition_info_1));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to write\n", __func__);
        return -EINVAL;
      }
      pr_err("ChgPartition debug info_1->mishow after write:%d\n", info_1->mishow);
      rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to dealloc\n", __func__);
        return -EINVAL;
      }
		  break;
    case CHARGER_PARTITION_PROP_POWER_OFF_MODE:
      rc = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to alloc\n", __func__);
        return -EINVAL;
      }
      info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
		  if(!info_1) {
			  pr_err("[ChgPartition] failed to read\n");
			  return -1;
		  }
		  info_1->power_off_mode = val;
      info_1->zero_speed_mode = 2;
      rc = charger_partition_write(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, (void *)info_1, sizeof(charger_partition_info_1));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to write\n", __func__);
        return -EINVAL;
      }
      pr_info("[ChgPartition] rc: %d, info_1->power_off_mode: %u\n", rc, info_1->power_off_mode);
      rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to dealloc\n", __func__);
        return -EINVAL;
      }
		  break;
    case CHARGER_PARTITION_PROP_EU_MODE:
      pr_err("ChgPartition debug set eumode enter");
      rc = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
      pr_err("ChgPartition debug alloc return");
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to alloc\n", __func__);
        return -EINVAL;
      }
      info_2 = (charger_partition_info_2 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
		  if(!info_2) {
			  pr_err("[ChgPartition] info_2 failed to read\n");
			  return -1;
		  }
      pr_err("ChgPartition debug set info_2");
      info_2->eu_mode = val;
      info_2->test = 0x34567890;
      info_2->reserved = 0;
      pr_err("ChgPartition debug set info_2 done");
      rc = charger_partition_write(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, (void *)info_2, sizeof(charger_partition_info_2));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to write\n", __func__);
        return -EINVAL;
      }
      pr_err("[ChgPartition] rc: %d, info_2->eu_mode: %u\n", rc, info_2->eu_mode);
      rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to dealloc\n", __func__);
        return -EINVAL;
      }
      break;
    default:
      pr_err("[ChgPartition] %s unsupport handle type!\n", __func__);
      break;
  }
  get_charger_partition_info_1();
  return rc;
}
EXPORT_SYMBOL_GPL(charger_partition_set_prop);

int charger_partition_get_prop(int type, int *val)
{
  int rc = 0;
  charger_partition_info_1 *info_1 = NULL;
  charger_partition_info_2 *info_2 = NULL;
  if (!chg_parti_is_rdy) {
    pr_err("[ChgPartition] charger partition not rdy, can't get prop!");
    return -EINVAL;
  }
  switch(type) {
  case CHARGER_PARTITION_PROP_MISHOW:
    rc = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if (rc < 0) {
      pr_err("[ChgPartition] %s failed to alloc\n", __func__);
      return -EINVAL;
    }
    info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if (!info_1) {
      pr_err("[ChgPartition] %s failed to read\n", __func__);
      rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to dealloc\n", __func__);
        return -EINVAL;
      }
      return -EINVAL;
    }
    *val = (int)info_1->mishow;
    pr_info("[ChgPartition] rc: %d, info_1:%p, info_1->mishow: %u, val: %d\n", rc, info_1, info_1->mishow, *val);
    rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if (rc < 0) {
      pr_err("[ChgPartition] %s failed to dealloc\n", __func__);
      return -EINVAL;
    }
		break;
  case CHARGER_PARTITION_PROP_POWER_OFF_MODE:
    rc = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if (rc < 0) {
      pr_err("[ChgPartition] %s failed to alloc\n", __func__);
      return -EINVAL;
    }
    info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if (!info_1) {
      pr_err("[ChgPartition] %s failed to read\n", __func__);
      rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to dealloc\n", __func__);
        return -EINVAL;
      }
      return -EINVAL;
    }
    *val = (int)info_1->power_off_mode;
    pr_info("[ChgPartition] rc: %d, info_1->poweroffmode: %u, val: %d\n", rc, info_1->power_off_mode, *val);
    
    rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if (rc < 0) {
      pr_err("[ChgPartition] %s failed to dealloc\n", __func__);
      return -EINVAL;
    }
		break;
  case CHARGER_PARTITION_PROP_EU_MODE:
    rc = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
    if (rc < 0) {
      pr_err("[ChgPartition] %s failed to alloc\n", __func__);
      return -EINVAL;
    }
    info_2 = (charger_partition_info_2 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
    if (!info_2) {
      pr_err("[ChgPartition] %s failed to read\n", __func__);
      rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to dealloc\n", __func__);
        return -EINVAL;
      }
      return -EINVAL;
    }
    *val = (int)info_2->eu_mode;
    rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_2, sizeof(charger_partition_info_2));
    if (rc < 0) {
      pr_err("[ChgPartition] %s failed to dealloc\n", __func__);
      return -EINVAL;
    }
    break;
  case CHARGER_PARTITION_PROP_IS_MISHOW_KPOC:
    rc = charger_partition_alloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if (rc < 0) {
      pr_err("[ChgPartition] %s failed to alloc\n", __func__);
      return -EINVAL;
    }
    info_1 = (charger_partition_info_1 *)charger_partition_read(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if (!info_1) {
      pr_err("[ChgPartition] %s failed to read\n", __func__);
      rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
      if (rc < 0) {
        pr_err("[ChgPartition] %s failed to dealloc\n", __func__);
        return -EINVAL;
      }
      return -EINVAL;
    }
    *val = (int)info_1->is_mishow_kpoc;
    pr_info("[ChgPartition] rc: %d, info_1->is_mishow_kpoc: %u, val: %d\n", rc, info_1->is_mishow_kpoc, *val);
    
    rc = charger_partition_dealloc(CHARGER_PARTITION_HOST_KERNEL, CHARGER_PARTITION_INFO_1, sizeof(charger_partition_info_1));
    if (rc < 0) {
      pr_err("[ChgPartition] %s failed to dealloc\n", __func__);
      return -EINVAL;
    }
		break;
  default:
    pr_err("[ChgPartition] %s unsupport handle type!\n", __func__);
    break;
  }
  pr_err("[ChgPartition] %s: %d is %d \n", __func__, type, *val);
  return 0;
}
EXPORT_SYMBOL_GPL(charger_partition_get_prop);

int charger_partition_get_eu_mode(void)
{
  pr_info("chargerPartition %s: eu_mode:%d \n", __func__, eu_mode);
  return eu_mode;
}
EXPORT_SYMBOL_GPL(charger_partition_get_eu_mode);

int charger_partition_get_is_mishow_kpoc(void)
{
  int is_mishow_kpoc = 0;
  charger_partition_get_prop(CHARGER_PARTITION_PROP_IS_MISHOW_KPOC, &is_mishow_kpoc);
  pr_err("ChgPartition %s: is_mishow_kpoc:%d \n", __func__, is_mishow_kpoc);
  return is_mishow_kpoc;
}
EXPORT_SYMBOL_GPL(charger_partition_get_is_mishow_kpoc);

static int charger_partition_init(void)
{
  charger_partition = (struct ChargerPartition *)kzalloc(sizeof(struct ChargerPartition), GFP_KERNEL);
  pr_err("%s:init!", __func__);
  INIT_DELAYED_WORK(&charger_partition->charger_partition_work, charger_partition_work);
  schedule_delayed_work(&charger_partition->charger_partition_work, msecs_to_jiffies(CHARGER_WORK_DELAY_MS));
  return 0;
}

static void charger_partition_exit(void)
{
  chg_parti_is_rdy = false;
  cancel_delayed_work(&charger_partition->charger_partition_work);
  kfree(charger_partition);
  pr_err("%s:exit!", __func__);
}

int touch_factory_enable(void)
{
  int ret;
#ifdef CONFIG_FACTORY_BUILD
  ret = 1;
#else
  ret = 0;
#endif
  pr_info("%s\n", __func__);
  return ret;
}
EXPORT_SYMBOL(touch_factory_enable);

static const char *get_usbpd_state_name(u32 usbpd_state)
{
	if (usbpd_state == USBPD_PE_STATE_PE_SNK_STARTUP)
		return usbpd_state_strings[1];
	else if (usbpd_state == USBPD_PE_STATE_PE_SNK_READY)
		return usbpd_state_strings[2];
	else if (usbpd_state == USBPD_PE_STATE_PE_SRC_READY)
		return usbpd_state_strings[3];
	else
		return usbpd_state_strings[0];
}

static int StringToHex(char *str, unsigned char *out, unsigned int *outlen)
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

static RAW_NOTIFIER_HEAD(hboost_notifier);
struct drm_panel *panel, *active_panel = NULL;

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

static int battery_chg_fw_write(struct battery_chg_dev *bcdev, void *data,
				int len)
{
	int rc;

	down_read(&bcdev->state_sem);
	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		pr_debug("glink state is down\n");
		up_read(&bcdev->state_sem);
		return -ENOTCONN;
	}
	up_read(&bcdev->state_sem);

	reinit_completion(&bcdev->fw_buf_ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->fw_buf_ack,
					msecs_to_jiffies(WLS_FW_WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			return -ETIMEDOUT;
		}

		rc = 0;
	}

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
		pr_debug("glink state is down\n");
		up_read(&bcdev->state_sem);
		return 0;
	}
	up_read(&bcdev->state_sem);

	if (bcdev->debug_battery_detected && bcdev->block_tx) {
		return 0;
	}

	mutex_lock(&bcdev->rw_lock);
	reinit_completion(&bcdev->ack);
	bcdev->error_prop = false;
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->ack,
					msecs_to_jiffies(BC_WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			mutex_unlock(&bcdev->rw_lock);
			return -ETIMEDOUT;
		}
		rc = 0;

		/*
		 * In case the opcode used is not supported, the remote
		 * processor might ack it immediately with a return code indicating
		 * an error. This additional check is to check if such an error has
		 * happened and return immediately with error in that case. This
		 * avoids wasting time waiting in the above timeout condition for this
		 * type of error.
		 */
		if (bcdev->error_prop) {
			bcdev->error_prop = false;
			rc = -ENODATA;
		}
	}
	mutex_unlock(&bcdev->rw_lock);

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

	if (pst->psy)
		pr_debug("psy: %s prop_id: %u val: %u\n", pst->psy->desc->name,
			req_msg.property_id, val);

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

	if (pst->psy)
		pr_debug("psy: %s prop_id: %u\n", pst->psy->desc->name,
			req_msg.property_id);

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static int get_property_id(struct psy_state *pst,
			enum power_supply_property prop)
{
	u32 i;

	for (i = 0; i < pst->prop_count; i++)
		if (pst->map[i] == prop)
			return i;

	if (pst->psy)
		pr_err("No property id for property %d in psy %s\n", prop,
			pst->psy->desc->name);

	return -ENOENT;
}

static int write_batt_auth_prop_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id, u8* buff)
{
	struct xm_verify_digest_resp_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;

	if (!buff)
		return -EINVAL;

	memcpy(req_msg.data, buff, BATTERY_DIGEST_LEN*sizeof(u8));
	pr_err("psy: prop_id:%d size:%lu \n", req_msg.property_id, sizeof(req_msg));
	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static int read_batt_auth_property_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id)
{
	struct xm_verify_digest_resp_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_get;

	pr_debug("psy: %s prop_id: %u\n", pst->psy->desc->name, req_msg.property_id);
	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

static int write_ss_auth_property_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id, u32* buff)
{
	struct xm_ss_auth_resp_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;

	memcpy(req_msg.data, buff, BATTERY_SS_AUTH_DATA_LEN*sizeof(u32));
	pr_debug("psy: prop_id:%u size:%lu data[0]:0x%x data[1]:0x%x data[2]:0x%x data[3]:0x%x\n",
		req_msg.property_id, sizeof(req_msg), req_msg.data[0],
		req_msg.data[1], req_msg.data[2], req_msg.data[3]);
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
	pr_debug("psy: %s prop_id: %u\n", pst->psy->desc->name, req_msg.property_id);

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}

#if 0
static void battery_chg_notify_disable(struct battery_chg_dev *bcdev)
{
	struct battery_charger_set_notify_msg req_msg = { { 0 } };
	int rc;

	if (bcdev->notify_en) {
		/* Send request to disable notification */
		req_msg.hdr.owner = MSG_OWNER_BC;
		req_msg.hdr.type = MSG_TYPE_NOTIFY;
		req_msg.hdr.opcode = BC_DISABLE_NOTIFY_REQ;

		rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
		if (rc < 0)
			pr_err("Failed to disable notification rc=%d\n", rc);
		else
			bcdev->notify_en = false;
	}
}
#endif

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

	pr_debug("state: %d\n", state);

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
	power_supply_put(psy);
	if (!bcdev)
		return -ENODEV;

	switch (prop_id) {
	case BATTERY_RESISTANCE:
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
		rc = read_property_id(bcdev, pst, BATT_RESISTANCE);
		if (!rc)
			*val = pst->prop[BATT_RESISTANCE];
		break;
	default:
		break;
	}

	return rc;
}
EXPORT_SYMBOL(qti_battery_charger_get_prop);

int qti_battery_charger_set_prop(const char *name,
				enum battery_charger_prop prop_id, int val)
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
	power_supply_put(psy);
	if (!bcdev)
		return -ENODEV;

	switch (prop_id) {
	case FLASH_ACTIVE:
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		rc = write_property_id(bcdev, pst, F_ACTIVE, val);
		break;
	default:
		break;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(qti_battery_charger_set_prop);

static bool validate_message(struct battery_chg_dev *bcdev,
			struct battery_charger_resp_msg *resp_msg, size_t len)
{
	struct xm_verify_digest_resp_msg *verify_digest_resp_msg = (struct xm_verify_digest_resp_msg *)resp_msg;
	struct xm_ss_auth_resp_msg *ss_auth_resp_msg = (struct xm_ss_auth_resp_msg *)resp_msg;

	if (len == sizeof(*verify_digest_resp_msg) || len == sizeof(*ss_auth_resp_msg)) {
		return true;
	}

	if (len != sizeof(*resp_msg)) {
		pr_err("Incorrect response length %zu for opcode %#x\n", len,
			resp_msg->hdr.opcode);
		return false;
	}

	if (resp_msg->ret_code) {
		pr_err_ratelimited("Error in response for opcode %#x prop_id %u, rc=%d\n",
			resp_msg->hdr.opcode, resp_msg->property_id,
			(int)resp_msg->ret_code);
		bcdev->error_prop = true;
		return false;
	}

	return true;
}

#define MODEL_DEBUG_BOARD	"Debug_Board"
static void xm_handle_dfx_report(u8 type);
static struct chg_dfx_report_msg *chg_dfx_data;
static void handle_message(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct battery_charger_resp_msg *resp_msg = data;
	struct battery_model_resp_msg *model_resp_msg = data;
	struct wireless_fw_check_resp *fw_check_msg;
	struct wireless_fw_push_buf_resp *fw_resp_msg;
	struct wireless_fw_update_status *fw_update_msg;
	struct wireless_fw_get_version_resp *fw_ver_msg;
	struct xm_verify_digest_resp_msg *verify_digest_resp_msg = data;
	struct xm_ss_auth_resp_msg *ss_auth_resp_msg = data;
	struct psy_state *pst;
	bool ack_set = false;
	chg_dfx_data = data;

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
		if (validate_message(bcdev, resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_USB_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		if (validate_message(bcdev, resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_WLS_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_WLS];
		if (validate_message(bcdev, resp_msg, len) &&
			resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_XM_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_XM];

		if (bcdev->digest && bcdev->manufacturing_date && bcdev->soh_sn_data &&
				bcdev->manuInfoC_data && len == sizeof(*verify_digest_resp_msg))
		{
			if (verify_digest_resp_msg->property_id == XM_PROP_VERIFY_DIGEST)
			{
				memcpy(bcdev->digest, verify_digest_resp_msg->data, BATTERY_DIGEST_LEN);
			}
			else if (verify_digest_resp_msg->property_id == XM_PROP_MANUFACTURING_DATA)
			{
				memcpy(bcdev->manufacturing_date, verify_digest_resp_msg->data, BATTERY_DIGEST_LEN);
			}
			else if (verify_digest_resp_msg->property_id == XM_PROP_SOH_SN)
			{
				memcpy(bcdev->soh_sn_data, verify_digest_resp_msg->data, BATTERY_DIGEST_LEN);
			}
			else if (verify_digest_resp_msg->property_id == XM_PROP_UI_SOH || verify_digest_resp_msg->property_id == XM_PROP_FIRST_USAGE_DATE)
			{
				memcpy(bcdev->manuInfoC_data, verify_digest_resp_msg->data, BATTERY_DIGEST_LEN);
			}
			ack_set = true;
			break;
		}

		if (bcdev->ss_auth_data && len == sizeof(*ss_auth_resp_msg)) {
			memcpy(bcdev->ss_auth_data, ss_auth_resp_msg->data, BATTERY_SS_AUTH_DATA_LEN*sizeof(u32));
			ack_set = true;
			break;
		}

		if (len == sizeof(*chg_dfx_data)) {
			xm_handle_dfx_report(chg_dfx_data->type);
			ack_set = true;
			break;
		}

		if (validate_message(bcdev, resp_msg, len) &&
			resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_BATTERY_STATUS_SET:
	case BC_USB_STATUS_SET:
	case BC_WLS_STATUS_SET:
		if (validate_message(bcdev, data, len))
			ack_set = true;

		break;
	case BC_SET_NOTIFY_REQ:
	case BC_DISABLE_NOTIFY_REQ:
	case BC_SHUTDOWN_NOTIFY:
	case BC_SHIP_MODE_REQ_SET:
	case BC_CHG_CTRL_LIMIT_EN:
		/* Always ACK response for notify or ship_mode request */
		ack_set = true;
		break;
	case BC_XM_STATUS_SET:
		if (validate_message(bcdev, data, len))
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
		pr_err("Unknown opcode: %u\n", resp_msg->hdr.opcode);
		break;
	}

	if (ack_set || bcdev->error_prop)
		complete(&bcdev->ack);
}

static struct power_supply_desc usb_psy_desc;

static void battery_chg_lpd_detection_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev, lpd_detection_work.work);
	int rc;
	int ret = 0;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];

	ret = rust_detection_workfunc_open();
	if (ret == 1) {
		bcdev->lpd_status = 1;
	} else if (ret != 0) {
		pr_err("LPD detect fail ret=%d\n", ret);
	}

	if (bcdev->lpd_status == 1) {
		rc = write_property_id(bcdev, pst, XM_PROP_LPD_STATUS, bcdev->lpd_status);
		if (rc < 0) {
			pr_err("Failed to write lpd_status\n");
		}
		bcdev->lpd_charging = 1;

		rc = write_property_id(bcdev, pst, XM_PROP_LPD_CHARGING, bcdev->lpd_charging);
		if (rc < 0) {
			pr_err("Failed to write lpd_charging\n");
		}
    }
	
	pr_err("lpd enable:%d lpd status:%d lpd_charging:%d\n",bcdev->lpd_enable ,bcdev->lpd_status,bcdev->lpd_charging);
	rust_detection_workfunc_close();
}

static void battery_chg_standar_chg_slow_work(struct work_struct *work){
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev, standar_chg_slow_work.work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int mah_cur, mah_last, delta_mah;

	mah_cur = pst->prop[BATT_CHG_COUNTER];
	mah_last = bcdev->mah_pre;
	delta_mah = (mah_cur - mah_last);
	bcdev->mah_pre = mah_cur;
	pr_err("mah_last = %d, mah_cur = %d, delta_mah = %d, dfx_first_check_flag = %d\n", mah_last, mah_cur, delta_mah, dfx_first_check_flag);
	if (delta_mah < bcdev->threshold_mah && dfx_first_check_flag == true){
		xm_handle_dfx_report(CHG_DFX_STANDARD_CHG_SLOW);
		pr_err("CHG_DFX_STANDARD_CHG_SLOW\n");
	}
	dfx_first_check_flag = true;
	schedule_delayed_work(&bcdev->standar_chg_slow_work, msecs_to_jiffies(1000*60*10));
}

static void battery_chg_update_poweroff_icl_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev, update_poweroff_chg_icl_work.work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
	if (rc < 0) {
		pr_err("Failed to read USB_ADAP_TYPE rc=%d\n", rc);
		return;
	}

	if (pst->prop[USB_ADAP_TYPE] == POWER_SUPPLY_USB_TYPE_SDP) {
		rc = usb_psy_set_icl(bcdev, USB_INPUT_CURR_LIMIT, 500000);
		pr_err("%s poweroff charging sdp set icl=500mA\n", __func__);
	} else {
		pr_err("%s not sdp, return\n", __func__);
		return;
	}
}

static void battery_chg_mipps_check_work(struct work_struct *work){
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev, mipps_check_work.work);

	if(bcdev->pd_verifed_dfx != 1){
		pr_err("CHG_DFX_NONE_MI_PPS\n");
		xm_handle_dfx_report(CHG_DFX_NONE_MI_PPS);
        }
}

void mievent_upload(int miev_code, char *miev_param, ...)
{
	#if IS_ENABLED(CONFIG_MIEV)
	va_list arg;
	char buffer[128] = {0};//miev_param length should be less than 128 bytes.
	char *p1,*p2,*key,*value;
	int intval;
	struct misight_mievent *event  = cdev_tevent_alloc(miev_code);

	pr_info("%s\n",__func__);
	va_start(arg,miev_param);
	memcpy(buffer, miev_param, strlen(miev_param));
	p1 = buffer;
	while (p1 && *p1 != '\0') {
		p2 = strsep(&p1,",");
		while (p2 && *p2 != '\0') {
			key = strsep(&p2, ":");
			if (key)
				pr_err("[CHG_DFX] key:%s\n", key);
			else {
				pr_err("[CHG_DFX] none key\n");
				key = "None";
			}
			value = strsep(&p2, ":");
			if (value)
				pr_err("[CHG_DFX] value:%s\n", value);
			else {
				pr_err("[CHG_DFX] none value\n");
				value = "None";
			}
		}
		pr_info("code:%d key:%s value:%s \n", miev_code, key, value);
		if(kstrtoint(value, 10,  &intval)!=0){
			cdev_tevent_add_str(event, key, value);
		} else {
			cdev_tevent_add_int(event, key, intval);
		}
		cdev_tevent_add_str(event, key, value);
	}
	cdev_tevent_write(event);
	cdev_tevent_destroy(event);
	#endif
	return;
}

#define DATA_LEN_MAX 128
static void xm_handle_dfx_report(u8 type)
{
	char data[DATA_LEN_MAX] = {0};
	int len = 0;

	if (type < CHG_DFX_MAX_INDEX) {
		pr_err("CHG:DFX: report %s\n", xm_dfx_chg_report_text[type][1]);
		len += scnprintf((data+len), (DATA_LEN_MAX-len), "%s:%s", xm_dfx_chg_report_text[type][0], xm_dfx_chg_report_text[type][1]);
	} else {
		pr_info("CHG:DFX: unknown type to report\n");
		return;
	}

	switch (type) {
	case CHG_DFX_NONE_STANDARD_CHG:
		mievent_upload(DFX_ID_CHG_NONE_STANDARD_CHG, data);
		break;
	case CHG_DFX_STANDARD_CHG_SLOW:
		mievent_upload(DFX_ID_CHG_STANDARD_CHG_SLOW, data);
		break;
	case CHG_DFX_NONE_MI_PPS:
		mievent_upload(DFX_ID_CHG_NONE_MI_PPS, data);
		break;
	case CHG_DFX_RP_SHORT_VBUS_DETECTED:
		mievent_upload(DFX_ID_CHG_RP_SHORT_VBUS_DETECTED, data);
		break;
	case CHG_DFX_LPD_DETECTED:
		mievent_upload(DFX_ID_CHG_LPD_DETECTED, data);
		break;
	case CHG_DFX_BATT_LINKER_ABSENT:
		mievent_upload(DFX_ID_CHG_BATT_LINKER_ABSENT, data);
		break;
	case CHG_DFX_BATTERY_AUTH_FAIL:
		mievent_upload(DFX_ID_CHG_BATTERY_AUTH_FAIL, data);
		break;
	case CHG_DFX_BATTERY_CYCLECOUNT:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d","cycleCnt", dfs_data_batt.cycle);
		mievent_upload(DFX_ID_CHG_BATTERY_CYCLECOUNT, data);
		break;
	case CHG_DFX_UISOC_NOT_FULL:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d","vbat", dfs_data_batt.vbat);
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d","soc", dfs_data_batt.uisoc);
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d","rsoc", dfs_data_batt.rawsoc);
		mievent_upload(DFX_ID_CHG_UISOC_NOT_FULL, data);
		break;
	case CHG_DFX_CP_ABSENT:
		mievent_upload(DFX_ID_CHG_CP_ABSENT, data);
		break;
	case CHG_DFX_CP_VBUS_OVP:
		mievent_upload(DFX_ID_CHG_CP_VBUS_OVP, data);
		break;
	case CHG_DFX_CP_IBUS_OCP:
		mievent_upload(DFX_ID_CHG_CP_IBUS_OCP, data);
		break;
	case CHG_DFX_CP_VBAT_OVP:
		mievent_upload(DFX_ID_CHG_CP_VBAT_OVP, data);
		break;
	case CHG_DFX_CP_IBAT_OCP:
		mievent_upload(DFX_ID_CHG_CP_IBAT_OCP, data);
		break;
	case CHG_DFX_CP_ENABLE_FAIL:
		mievent_upload(DFX_ID_CHG_CP_ENABLE_FAIL, data);
		break;
	case CHG_DFX_NAVIGATION:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d","soc", dfs_data_batt.uisoc);
		mievent_upload(DFX_ID_CHG_SMART_NAVIGATION_TRIGGERED, data);
		break;
	case CHG_DFX_NAVIGATION_OVER_SOC:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d","soc", dfs_data_batt.uisoc);
		mievent_upload(DFX_ID_SMART_NAVIGATION_SOC_ERR, data);
		break;
	case CHG_DFX_SMART_ENDURANCE_TRIGGERED:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d","soc", chg_dfx_data->data[0]);
		mievent_upload(DFX_ID_CHG_SMART_ENDURANCE_TRIGGERED, data);
		break;
	case CHG_DFX_SMART_ENDURANCE_SOC_ERR:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d","soc", chg_dfx_data->data[0]);
		mievent_upload(DFX_ID_CHG_SMART_ENDURANCE_SOC_ERR, data);
		break;
	case CHG_DFX_LOW_TEMP_DISCHARGING:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d","tbat", chg_dfx_data->data[0] * 10);
		mievent_upload(DFX_ID_CHG_LOW_TEMP_DISCHARGING, data);
		break;
	case CHG_DFX_HIGH_TEMP_DISCHARGING:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d","tbat", chg_dfx_data->data[0] * 10);
		mievent_upload(DFX_ID_CHG_HIGH_TEMP_DISCHARGING, data);
		break;
	case CHG_DFX_BATTERY_TEMP_HOT:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d,%s:%d,%s:%d,%s:%d","tbat", chg_dfx_data->data[0],"maxtbat", chg_dfx_data->data[1],"bat_status", chg_dfx_data->data[2],"tboard", chg_dfx_data->data[3]);
		mievent_upload(DFX_ID_CHG_BATTERY_TEMP_HOT, data);
		break;
	case CHG_DFX_BATTERY_TEMP_COLD:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d,%s:%d,%s:%d,%s:%d","tbat", chg_dfx_data->data[0],"mintbat", chg_dfx_data->data[1],"bat_status", chg_dfx_data->data[2],"tboard", chg_dfx_data->data[3]);
		mievent_upload(DFX_ID_CHG_BATTERY_TEMP_COLD, data);
		break;
	case CHG_DFX_PPS_AUTH_FAIL:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%x","adapterId", dfs_data_batt.adapter_id);
		mievent_upload(DFX_ID_CHG_PD_AUTHEN_FAIL, data);
		break;
	case CHG_DFX_FG_IIC_ERR:
		len += scnprintf((data+len), (DATA_LEN_MAX-len), ",%s:%d","soc", dfs_data_batt.uisoc);
		mievent_upload(DFX_ID_CHG_BATT_IIC_ERR, data);
		break;
	default:
		pr_info("CHG:DFX: unknown type to report\n");
	}
	return;
}

static void battery_chg_update_usb_type_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, usb_type_work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;
	static bool dfx_notify = true;

	rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
	if (rc < 0) {
		pr_err("Failed to read USB_ADAP_TYPE rc=%d\n", rc);
		return;
	}

	if (pst->prop[USB_ADAP_TYPE] == POWER_SUPPLY_USB_TYPE_UNKNOWN) {
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
		pr_debug("Unknown USB adapter type\n");
		return;
	}

	if ((pst->prop[USB_REAL_TYPE] == QTI_POWER_SUPPLY_USB_TYPE_USB_FLOAT) && dfx_notify) {
		dfx_notify = false;
		xm_handle_dfx_report(CHG_DFX_NONE_STANDARD_CHG);
	} else if (pst->prop[USB_REAL_TYPE] == POWER_SUPPLY_USB_TYPE_UNKNOWN) {
		dfx_notify = true;
	}

	/* Reset usb_icl_ua whenever USB adapter type changes */
	if (pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_SDP &&
	    pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_CDP &&
	    pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_PD)
		bcdev->usb_icl_ua = 0;

	pr_debug("usb_adap_type: %u\n", pst->prop[USB_ADAP_TYPE]);

	switch (pst->prop[USB_ADAP_TYPE]) {
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
		usb_psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
	}
}

static void battery_chg_check_status_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev,
					battery_check_work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int rc;

	rc = read_property_id(bcdev, pst, BATT_STATUS);
	if (rc < 0) {
		pr_err("Failed to read BATT_STATUS, rc=%d\n", rc);
		return;
	}

	if (pst->prop[BATT_STATUS] == POWER_SUPPLY_STATUS_CHARGING) {
		pr_debug("Battery is charging\n");
		return;
	}

	rc = read_property_id(bcdev, pst, BATT_CAPACITY);
	if (rc < 0) {
		pr_err("Failed to read BATT_CAPACITY, rc=%d\n", rc);
		return;
	}

	if (DIV_ROUND_CLOSEST(pst->prop[BATT_CAPACITY], 100) > 1) {
		pr_debug("Battery SOC is > 1\n");
		return;
	}

	/*
	 * If we are here, then battery is not charging and SOC is 0.
	 * Check the battery voltage and if it's lower than shutdown voltage,
	 * then initiate an emergency shutdown.
	 */

	rc = read_property_id(bcdev, pst, BATT_VOLT_NOW);
	if (rc < 0) {
		pr_err("Failed to read BATT_VOLT_NOW, rc=%d\n", rc);
		return;
	}

	rc = read_property_id(bcdev, pst, BATT_POWEROFF_VOLT);
	if (rc < 0) {
		pr_err("Failed to read BATT_POWEROFF_VOLT, rc=%d\n", rc);
	} else {
		bcdev->shutdown_volt_mv = pst->prop[BATT_POWEROFF_VOLT];
		pr_info("power off voltage %d\n", bcdev->shutdown_volt_mv);
	}

	if (pst->prop[BATT_VOLT_NOW] / 1000 > bcdev->shutdown_volt_mv) {
		pr_debug("Battery voltage is > %d mV\n",
			bcdev->shutdown_volt_mv);
		return;
	}

	pr_emerg("Initiating a shutdown in 100 ms\n");
	msleep(100);
	pr_emerg("Attempting kernel_power_off: Battery voltage low\n");
	// kernel_power_off();
}
#define IBUS_OCP 5000000
void dfx_cp_ibus_ocp_check(struct battery_chg_dev *bcdev){
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	if (IS_ERR_OR_NULL(bcdev)) {
		pr_err("point bcdev is err or null\n");
		return;
	}
        pr_info("cp_ibus value = %u\n", pst->prop[XM_PROP_CP_BUS_CURRENT]);
	if(pst->prop[XM_PROP_CP_BUS_CURRENT] > IBUS_OCP && bcdev->dfx_cp_ibus_ocp_flag < 4){
		bcdev->dfx_cp_ibus_ocp_flag++;
		xm_handle_dfx_report(CHG_DFX_CP_IBUS_OCP);
  		pr_err("ibus ocp, send dfx report, val: %d\n",pst->prop[XM_PROP_CP_BUS_CURRENT]);
	}
}

void dfx_cyclecount_check(struct battery_chg_dev *bcdev)
{
	static int count = 0;

	if (IS_ERR_OR_NULL(bcdev)) {
		pr_err("point bcdev is err or null\n");
		return;
	}

	count = dfs_data_batt.cycle  / 100;
	if (dfs_data_batt.cycle  > 0 && dfs_data_batt.cycle  % 100 == 0 && bcdev->dfx_cycle_count_flag < count)
	{
		bcdev->dfx_cycle_count_flag++;
		xm_handle_dfx_report(CHG_DFX_BATTERY_CYCLECOUNT);
		pr_err("batt cyclecout, send dfx report, count :%d val: %d\n",dfs_data_batt.cycle ,bcdev->dfx_cycle_count_flag);
	}
}

void dfx_batt_auth_check(struct battery_chg_dev *bcdev)
{
	if (IS_ERR_OR_NULL(bcdev)) {
		pr_err("point bcdev is err or null\n");
		return;
	}

	if (bcdev->batt_authentic == 0 && bcdev->dfx_batt_auth_flag < 1)
	{
		bcdev->dfx_batt_auth_flag++;
		xm_handle_dfx_report(CHG_DFX_BATTERY_AUTH_FAIL);
		pr_err("batt authentic fail, send dfx report, val: %d\n",bcdev->batt_authentic);
	}
}

void dfx_batt_linker_absent_check(struct battery_chg_dev *bcdev)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	if (IS_ERR_OR_NULL(bcdev)) {
		pr_err("point bcdev is err or null\n");
		return;
	}

	if (bcdev->chip_ok == 0 && bcdev->dfx_chip_ok_check_flag < 1) {
		rc = read_property_id(bcdev, pst, XM_PROP_CHIP_OK);
		if (rc < 0)
			return;

		pr_info(" value = %u\n", pst->prop[XM_PROP_CHIP_OK]);
		bcdev->chip_ok = pst->prop[XM_PROP_CHIP_OK];

		bcdev->dfx_chip_ok_check_flag++;
		if (bcdev->chip_ok == 0)
			xm_handle_dfx_report(CHG_DFX_BATT_LINKER_ABSENT);

		return;
	}

	pr_info("batt linker present, needn't process\n");
	return;
}

static void dfx_lpd_check(struct battery_chg_dev *bcdev)
{
	if (IS_ERR_OR_NULL(bcdev)) {
		pr_err("CHG:DFX: point bcdev is err or null\n");
		return ;
	}
#ifndef CONFIG_FACTORY_BUILD
#if IS_ENABLED(CONFIG_RUST_DETECTION)
	if (bcdev->lpd_status && bcdev->dfx_lpd_check_flag < 1) {
		bcdev->dfx_lpd_check_flag++;
		xm_handle_dfx_report(CHG_DFX_LPD_DETECTED);
		pr_err("CHG:DFX: dfx_lpd_check, send dfx report\n");
	}
#endif
#endif
	pr_err("CHG:DFX:dfx_lpd_check_flag:%d\n", bcdev->dfx_lpd_check_flag);
	//pr_err("lpd enable:%d lpd status:%d\n",bcdev->lpd_enable ,bcdev->lpd_status);
}

static void dfx_uisoc_not_full_check(struct battery_chg_dev *bcdev)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	struct psy_state *batt_pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	static int pre_battery_status = -1;
	int battery_status = 0;
	int uisoc = 0;
	int rsoc = 0;

	if (IS_ERR_OR_NULL(bcdev)) {
		pr_err("CHG:DFX: point bcdev is err or null\n");
		return;
	}

	battery_status = batt_pst->prop[BATT_STATUS];
	if (pre_battery_status != POWER_SUPPLY_STATUS_FULL && battery_status == POWER_SUPPLY_STATUS_FULL)
	{
		uisoc = batt_pst->prop[BATT_CAPACITY] / 100;
		read_property_id(bcdev, pst, XM_PROP_RAW_SOC);
		rsoc = pst->prop[XM_PROP_RAW_SOC];
		if ((uisoc != 100 && !eu_mode) || ((rsoc != 100 && eu_mode))) {
			dfs_data_batt.vbat = batt_pst->prop[BATT_VOLT_NOW];
			dfs_data_batt.uisoc = uisoc;
			dfs_data_batt.rawsoc = rsoc;
			xm_handle_dfx_report(CHG_DFX_UISOC_NOT_FULL);
			pr_err("vbat:%d uisoc:%d rawsoc:%d\n",dfs_data_batt.vbat ,dfs_data_batt.uisoc ,dfs_data_batt.rawsoc);
		}
	}
	pre_battery_status = battery_status;
}

static void dfx_pd_auth_check(struct battery_chg_dev *bcdev, bool chip_ok)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	struct psy_state *usb_pst = &bcdev->psy_list[PSY_TYPE_USB];
	int usb_online = 0;
	int rc = 0;

	if (IS_ERR_OR_NULL(bcdev)) {
		pr_err("CHG:DFX: point bcdev is err or null\n");
		return;
	}

	usb_online = usb_pst->prop[USB_ONLINE];
	if (!usb_online)
		return;

	if (!chip_ok) {
		rc = read_property_id(bcdev, pst, XM_PROP_ADAPTER_SVID);
		if (rc < 0)
			return;

		if (pst->prop[XM_PROP_ADAPTER_SVID] != 0x2717) {
			pr_info("CHG:DFX:adapter(SVID:%04x) is not xm adapter, exit\n", pst->prop[XM_PROP_ADAPTER_SVID]);
			return;
		}
		rc = read_property_id(bcdev, pst, XM_PROP_ADAPTER_ID);
		if (rc < 0)
			return;
		dfs_data_batt.adapter_id = pst->prop[XM_PROP_ADAPTER_ID];
		xm_handle_dfx_report(CHG_DFX_PPS_AUTH_FAIL);
	}
}

static void dfx_batt_iic_check(struct battery_chg_dev *bcdev)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	struct psy_state *batt_pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int uisoc = 0;
	int rc = 0;

	if (IS_ERR_OR_NULL(bcdev)) {
		pr_err("point bcdev is err or null\n");
		return;
	}

	uisoc = batt_pst->prop[BATT_CAPACITY];

	rc = read_property_id(bcdev, pst, XM_PROP_BATT_IIC_STATE);
	if (rc < 0) {
		return;
	} else {
		pr_info("value = %u\n", pst->prop[XM_PROP_BATT_IIC_STATE]);
		bcdev->i2c_state = pst->prop[XM_PROP_BATT_IIC_STATE];
	}

	if (bcdev->i2c_state != 0) {
		pr_err("batt i2c error, send dfx report, val: %d\n",bcdev->i2c_state);
		dfs_data_batt.uisoc = uisoc;
		xm_handle_dfx_report(CHG_DFX_FG_IIC_ERR);
		return;
	}

	return;
}

static void battery_chg_dfx_monitor_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev, dfx_monitor_work.work);

	if (bcdev->dfx_work_delay == 0)
		bcdev->dfx_work_delay = DFX_DISCHARGE_WORK_TIME;

	dfx_batt_linker_absent_check(bcdev);
	dfx_lpd_check(bcdev);
	//dfx_batt_auth_check(bcdev);
	dfx_cyclecount_check(bcdev);
	dfx_uisoc_not_full_check(bcdev);
	dfx_batt_iic_check(bcdev);
        dfx_cp_ibus_ocp_check(bcdev);
#ifndef CONFIG_FACTORY_BUILD
	schedule_delayed_work(&bcdev->dfx_monitor_work, msecs_to_jiffies(bcdev->dfx_work_delay));
#endif
	return;
}

static void handle_notification(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct battery_charger_notify_msg *notify_msg = data;
	struct psy_state *pst = NULL;
	u32 hboost_vmax_mv, notification;

	if (len != sizeof(*notify_msg)) {
		pr_err("Incorrect response length %zu\n", len);
		return;
	}

	notification = notify_msg->notification;
	pr_debug("notification: %#x\n", notification);
	if ((notification & 0xffff) == BC_HBOOST_VMAX_CLAMP_NOTIFY) {
		hboost_vmax_mv = (notification >> 16) & 0xffff;
		raw_notifier_call_chain(&hboost_notifier, VMAX_CLAMP, &hboost_vmax_mv);
		pr_debug("hBoost is clamped at %u mV\n", hboost_vmax_mv);
		return;
	}

	switch (notification) {
	case BC_BATTERY_STATUS_GET:
	case BC_GENERIC_NOTIFY:
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
//		if (bcdev->shutdown_volt_mv > 0)
//			schedule_work(&bcdev->battery_check_work);
		break;
	case BC_BATTERY_LPD_NOTIFY:
		if(bcdev->lpd_enable && bcdev->lpd_control)
			schedule_delayed_work(&bcdev->lpd_detection_work, 150);
		bcdev->dfx_work_delay = DFX_CHARGE_WORK_TIME;
		break;
	case BC_BATTERY_LPD_RESET:
		cancel_delayed_work_sync(&bcdev->lpd_detection_work);
		bcdev->dfx_work_delay = DFX_DISCHARGE_WORK_TIME;
		bcdev->lpd_status = 0;
		break;
	case BC_USB_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		schedule_work(&bcdev->usb_type_work);
		schedule_delayed_work(&bcdev->xm_prop_change_work, 0);
		break;
	case BC_WLS_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_WLS];
		break;
	case BC_XM_STATUS_GET:
		schedule_delayed_work(&bcdev->xm_prop_change_work, 100);
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
		pm_wakeup_dev_event(bcdev->dev, 50, true);
	}
}

static int battery_chg_callback(void *priv, void *data, size_t len)
{
	struct pmic_glink_hdr *hdr = data;
	struct battery_chg_dev *bcdev = priv;

	pr_debug("owner: %u type: %u opcode: %#x len: %zu\n", hdr->owner,
		hdr->type, hdr->opcode, len);

	down_read(&bcdev->state_sem);
	if (!bcdev->initialized) {
		pr_debug("Driver initialization failed: Dropping glink callback message: state %d\n",
			 atomic_read(&bcdev->state));
		up_read(&bcdev->state_sem);
		return 0;
	}
	up_read(&bcdev->state_sem);

	if (hdr->opcode == BC_NOTIFY_IND)
		handle_notification(bcdev, data, len);
	else
		handle_message(bcdev, data, len);

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

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	pval->intval = pst->prop[prop_id];

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

static int usb_psy_set_icl(struct battery_chg_dev *bcdev, u32 prop_id, int val)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	u32 temp;
	int rc;

	rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
	if (rc < 0) {
		pr_err("Failed to read prop USB_ADAP_TYPE, rc=%d\n", rc);
		return rc;
	}

	/* Allow this only for SDP, CDP or USB_PD and not for other charger types */
	switch (pst->prop[USB_ADAP_TYPE]) {
	case POWER_SUPPLY_USB_TYPE_SDP:
	case POWER_SUPPLY_USB_TYPE_PD:
	case POWER_SUPPLY_USB_TYPE_CDP:
		break;
	default:
		bcdev->usb_icl_ua = 0;
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
		pr_debug("Set ICL to %u\n", temp);
		bcdev->usb_icl_ua = temp;
	}

	return rc;
}

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

struct quick_charge adapter_cap[12] = {
	{ POWER_SUPPLY_USB_TYPE_SDP,            QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_DCP,            QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_CDP,            QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_ACA,            QUICK_CHARGE_NORMAL },
	{ QTI_POWER_SUPPLY_USB_TYPE_USB_FLOAT,  QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_PD,             QUICK_CHARGE_FAST },
	{ QTI_POWER_SUPPLY_USB_TYPE_HVDCP,      QUICK_CHARGE_FAST },
	{ QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3,    QUICK_CHARGE_FAST },
	{ QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3_CLASSB,  QUICK_CHARGE_FLASH },
	{ QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5,  QUICK_CHARGE_FLASH },
	{ POWER_SUPPLY_USB_TYPE_PD_PPS,         QUICK_CHARGE_FAST },
	{ 0, 0 },
};

int get_quick_charge_type(struct battery_chg_dev *bcdev, union power_supply_propval *val)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	struct psy_state *batt_pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	enum power_supply_usb_type real_charger_type = 0;
	int i = 0, verify_digiest = 0;
	int rc, batt_temp;
	u8 result = QUICK_CHARGE_NORMAL;

	batt_temp = batt_pst->prop[BATT_TEMP];

	rc = read_property_id(bcdev, pst, USB_REAL_TYPE);
	if (rc < 0)
		return rc;
	real_charger_type = pst->prop[USB_REAL_TYPE];

	pst = &bcdev->psy_list[PSY_TYPE_XM];
	rc = read_property_id(bcdev, pst, XM_PROP_PD_VERIFED);
	verify_digiest = pst->prop[XM_PROP_PD_VERIFED];

	pr_err("usb_real_type[%d][%s] temp[%d] verify[%d]\n", pst->prop[USB_REAL_TYPE], get_usb_type_name(pst->prop[USB_REAL_TYPE]), batt_temp, verify_digiest);

	if (batt_temp < 500 || batt_temp >= 4500) {
		result = QUICK_CHARGE_NORMAL;
	} else if (real_charger_type == POWER_SUPPLY_USB_TYPE_PD_PPS && verify_digiest == 1) {
		result = QUICK_CHARGE_TURBE;
	} else {
		while (adapter_cap[i].adap_type != 0) {
			if (real_charger_type == adapter_cap[i].adap_type) {
				result = adapter_cap[i].adap_cap;
				break;
			}
			i++;
		}
	}
	val->intval = result;

	return 0;
}

static int usb_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	struct psy_state *bpst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int prop_id, rc, uisoc;

	pval->intval = -ENODATA;

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	uisoc = bpst->prop[BATT_CAPACITY] / 100;
	pval->intval = pst->prop[prop_id];

	if (prop == POWER_SUPPLY_PROP_TEMP)
		pval->intval = DIV_ROUND_CLOSEST((int)pval->intval, 10);
	else if (prop == POWER_SUPPLY_PROP_ONLINE) {
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		if (pval->intval && pst->prop[USB_ADAP_TYPE] == POWER_SUPPLY_USB_TYPE_UNKNOWN) {
			pval->intval = 0;
			pr_info("force usb offline\n");
		}
		else if (pval->intval == 1){
			if (mi_pps_dfx_flag == true){
				pr_err("pd_verifed_dfx = %d\n", bcdev->pd_verifed_dfx);
				schedule_delayed_work(&bcdev->mipps_check_work, msecs_to_jiffies(1000*60));
				mi_pps_dfx_flag = false;
                        }
                  	pr_err("uisoc = %d, mah_pre = %d\n", uisoc, bcdev->mah_pre);
                        if(uisoc < 80 && standar_chg_slow_flag == true && bcdev->pd_verifed_dfx == 1){
                          	standar_chg_slow_flag = false;
                          	pr_err("set standar_chg_slow_flag = false\n");
                        	schedule_delayed_work(&bcdev->standar_chg_slow_work, msecs_to_jiffies(1000));
                        }
                }
		else if(pval->intval == 0){
			cancel_delayed_work(&bcdev->mipps_check_work);
			cancel_delayed_work(&bcdev->standar_chg_slow_work);
			mi_pps_dfx_flag = true;
			pr_err("set standar_chg_slow_flag = true\n");
			standar_chg_slow_flag = true;
			dfx_first_check_flag = false;
			bcdev->pd_verifed_dfx = 0;
			bcdev->mah_pre = 0;
                }
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
		if (pval->intval >= USB_ICL_50MA) {
			rc = usb_psy_set_icl(bcdev, prop_id, pval->intval);
		}
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

#define CHARGE_CTRL_START_THR_MIN	50
#define CHARGE_CTRL_START_THR_MAX	95
#define CHARGE_CTRL_END_THR_MIN		55
#define CHARGE_CTRL_END_THR_MAX		100
#define CHARGE_CTRL_DELTA_SOC		5

static int battery_psy_set_charge_threshold(struct battery_chg_dev *bcdev,
					u32 target_soc, u32 delta_soc)
{
	struct battery_charger_chg_ctrl_msg msg = { { 0 } };
	int rc;

	if (!bcdev->chg_ctrl_en)
		return 0;

	if (target_soc > CHARGE_CTRL_END_THR_MAX)
		target_soc = CHARGE_CTRL_END_THR_MAX;

	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_CHG_CTRL_LIMIT_EN;
	msg.enable = 1;
	msg.target_soc = target_soc;
	msg.delta_soc = delta_soc;

	rc = battery_chg_write(bcdev, &msg, sizeof(msg));
	if (rc < 0)
		pr_err("Failed to set charge_control thresholds, rc=%d\n", rc);
	else
		pr_debug("target_soc: %u delta_soc: %u\n", target_soc, delta_soc);

	return rc;
}

static int battery_psy_set_charge_end_threshold(struct battery_chg_dev *bcdev,
					int val)
{
	u32 delta_soc = CHARGE_CTRL_DELTA_SOC;
	int rc;

	if (val < CHARGE_CTRL_END_THR_MIN ||
	    val > CHARGE_CTRL_END_THR_MAX) {
		pr_err("Charge control end_threshold should be within [%u %u]\n",
			CHARGE_CTRL_END_THR_MIN, CHARGE_CTRL_END_THR_MAX);
		return -EINVAL;
	}

	if (bcdev->chg_ctrl_start_thr && val > bcdev->chg_ctrl_start_thr)
		delta_soc = val - bcdev->chg_ctrl_start_thr;

	rc = battery_psy_set_charge_threshold(bcdev, val, delta_soc);
	if (rc < 0)
		pr_err("Failed to set charge control end threshold %u, rc=%d\n",
			val, rc);
	else
		bcdev->chg_ctrl_end_thr = val;

	return rc;
}

static int battery_psy_set_charge_start_threshold(struct battery_chg_dev *bcdev,
					int val)
{
	u32 target_soc, delta_soc;
	int rc;

	if (val < CHARGE_CTRL_START_THR_MIN ||
	    val > CHARGE_CTRL_START_THR_MAX) {
		pr_err("Charge control start_threshold should be within [%u %u]\n",
			CHARGE_CTRL_START_THR_MIN, CHARGE_CTRL_START_THR_MAX);
		return -EINVAL;
	}

	if (val > bcdev->chg_ctrl_end_thr) {
		target_soc = val +  CHARGE_CTRL_DELTA_SOC;
		delta_soc = CHARGE_CTRL_DELTA_SOC;
	} else {
		target_soc = bcdev->chg_ctrl_end_thr;
		delta_soc = bcdev->chg_ctrl_end_thr - val;
	}

	rc = battery_psy_set_charge_threshold(bcdev, target_soc, delta_soc);
	if (rc < 0)
		pr_err("Failed to set charge control start threshold %u, rc=%d\n",
			val, rc);
	else
		bcdev->chg_ctrl_start_thr = val;

	return rc;
}

static int get_charge_control_en(struct battery_chg_dev *bcdev)
{
	int rc;

	rc = read_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_BATTERY],
				BATT_CHG_CTRL_EN);
	if (rc < 0)
		pr_err("Failed to read the CHG_CTRL_EN, rc = %d\n", rc);
	else
		bcdev->chg_ctrl_en =
			bcdev->psy_list[PSY_TYPE_BATTERY].prop[BATT_CHG_CTRL_EN];

	return rc;
}

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
		pr_debug("Set FCC to %u uA\n", fcc_ua);
		bcdev->last_fcc_ua = fcc_ua;
	}

	return rc;
}

static int battery_psy_set_charge_current(struct battery_chg_dev *bcdev,
					int val)
{
	int rc;
	u32 fcc_ua, prev_fcc_ua;

	if (!bcdev->num_thermal_levels)
		return 0;

	if (bcdev->num_thermal_levels < 0) {
		pr_err("Incorrect num_thermal_levels\n");
		return -EINVAL;
	}

	if (val < 0 || val > bcdev->num_thermal_levels)
		return -EINVAL;

	if (bcdev->thermal_fcc_step == 0)
		fcc_ua = bcdev->thermal_levels[val];
	else
		fcc_ua = bcdev->psy_list[PSY_TYPE_BATTERY].prop[BATT_CHG_CTRL_LIM_MAX]
				- (bcdev->thermal_fcc_step * val);

	pr_info("thermal level set :%d\n", val);

	prev_fcc_ua = bcdev->thermal_fcc_ua;
	bcdev->thermal_fcc_ua = fcc_ua;

	rc = __battery_psy_set_charge_current(bcdev, fcc_ua);
	if (!rc)
		bcdev->curr_thermal_level = val;
	else
		bcdev->thermal_fcc_ua = prev_fcc_ua;

	return rc;
}

static int battery_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int prop_id, rc;
  	static int printcount = 0;
	static int printcurrent = 0;
  	static int last_current = 0;
  	static int last_temp = 0;

	pval->intval = -ENODATA;

	/*
	 * The prop id of TIME_TO_FULL_NOW and TIME_TO_FULL_AVG is same.
	 * So, map the prop id of TIME_TO_FULL_AVG for TIME_TO_FULL_NOW.
	 */
	if (prop == POWER_SUPPLY_PROP_TIME_TO_FULL_NOW)
		prop = POWER_SUPPLY_PROP_TIME_TO_FULL_AVG;

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		pval->strval = pst->model;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		pval->intval = DIV_ROUND_CLOSEST(pst->prop[prop_id], 100);
		if (bcdev->fake_soc >= 0 && bcdev->fake_soc <= 100)
			pval->intval = bcdev->fake_soc;
		if (pval->intval == 0)
			pval->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		pval->intval = DIV_ROUND_CLOSEST((int)pst->prop[prop_id], 10);
            	printcount++;
            	if((printcount >= 5) || (last_temp != pval->intval)){
                   pr_err("batt temp =%d \n",pval->intval);
                   printcount = 0;
                }
            	last_temp = pval->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
        	pval->intval = pst->prop[prop_id];
        	printcurrent++;
        	if((printcurrent >= 5) || (last_current == 0 && pval->intval != 0) || (last_current != 0 && pval->intval == 0 )){
            		pr_err("batt current = %d\n",pval->intval);
           		printcurrent = 0;
        	}
            	last_current = pval->intval;
        	break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		pval->intval = bcdev->curr_thermal_level;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		pval->intval = bcdev->num_thermal_levels;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		pval->intval = pst->prop[prop_id];
		dfs_data_batt.cycle = pval->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		pval->intval = pst->prop[prop_id];
		if(pval->intval == POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL) {
			pr_err("soc capacity level critical true\n");
		}
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

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD:
		return battery_psy_set_charge_start_threshold(bcdev,
								pval->intval);
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD:
		return battery_psy_set_charge_end_threshold(bcdev,
								pval->intval);
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		return battery_psy_set_charge_current(bcdev, pval->intval);
	default:
		return -EINVAL;
	}

	return 0;
}

static int battery_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
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
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD,
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
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
};

static const struct power_supply_desc batt_psy_desc = {
	.name			= "battery",
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

	psy_cfg.drv_data = bcdev;
	psy_cfg.of_node = bcdev->dev->of_node;
	bcdev->psy_list[PSY_TYPE_USB].psy =
		devm_power_supply_register(bcdev->dev, &usb_psy_desc, &psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_USB].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_USB].psy);
		bcdev->psy_list[PSY_TYPE_USB].psy = NULL;
		pr_err("Failed to register USB power supply, rc=%d\n", rc);
		return rc;
	}

	bcdev->psy_list[PSY_TYPE_WLS].psy =
		devm_power_supply_register(bcdev->dev, &wls_psy_desc, &psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy);
		bcdev->psy_list[PSY_TYPE_WLS].psy = NULL;
		pr_err("Failed to register wireless power supply, rc=%d\n", rc);
		return rc;
	}

	bcdev->psy_list[PSY_TYPE_BATTERY].psy =
		devm_power_supply_register(bcdev->dev, &batt_psy_desc,
						&psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_BATTERY].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_BATTERY].psy);
		bcdev->psy_list[PSY_TYPE_BATTERY].psy = NULL;
		pr_err("Failed to register battery power supply, rc=%d\n", rc);
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

	/*
	 * Give some time after enabling notification so that USB adapter type
	 * information can be obtained properly which is essential for setting
	 * USB ICL.
	 */
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

	pr_debug("Updating FW...\n");

	ptr = fw->data;
	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_WLS_FW_PUSH_BUF_REQ;

	for (i = 0; i < num_chunks; i++, ptr += WLS_FW_BUF_SIZE) {
		msg.fw_chunk_id = i + 1;
		memcpy(msg.buf, ptr, WLS_FW_BUF_SIZE);

		pr_debug("sending FW chunk %u\n", i + 1);
		rc = battery_chg_fw_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			return rc;
	}

	if (partial_chunk_size) {
		msg.fw_chunk_id = i + 1;
		memset(msg.buf, 0, WLS_FW_BUF_SIZE);
		memcpy(msg.buf, ptr, partial_chunk_size);

		pr_debug("sending partial FW chunk %u\n", i + 1);
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
		pr_err("wireless FW name is not specified\n");
		return -EINVAL;
	}

	pm_stay_awake(bcdev->dev);

	/*
	 * Check for USB presence. If nothing is connected, check whether
	 * battery SOC is at least 50% before allowing FW update.
	 */
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
			pr_err("Battery SOC should be at least 50%% or connect charger\n");
			rc = -EINVAL;
			goto out;
		}
	}

	rc = firmware_request_nowarn(&fw, bcdev->wls_fw_name, bcdev->dev);
	if (rc) {
		pr_err("Couldn't get firmware rc=%d\n", rc);
		goto out;
	}

	if (!fw || !fw->data || !fw->size) {
		pr_err("Invalid firmware\n");
		rc = -EINVAL;
		goto release_fw;
	}

	if (fw->size < SZ_16K) {
		pr_err("Invalid firmware size %zu\n", fw->size);
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

	pr_debug("FW size: %zu version: %#x\n", fw->size, version);

	rc = wireless_fw_check_for_update(bcdev, version, fw->size);
	if (rc < 0) {
		pr_err("Wireless FW update not needed, rc=%d\n", rc);
		goto release_fw;
	}

	if (!bcdev->wls_fw_update_reqd) {
		pr_warn("Wireless FW update not required\n");
		goto release_fw;
	}

	/* Wait for IDT to be setup by charger firmware */
	msleep(WLS_FW_PREPARE_TIME_MS);

	reinit_completion(&bcdev->fw_update_ack);
	rc = wireless_fw_send_firmware(bcdev, fw);
	if (rc < 0) {
		pr_err("Failed to send FW chunk, rc=%d\n", rc);
		goto release_fw;
	}

	pr_debug("Waiting for fw_update_ack\n");
	rc = wait_for_completion_timeout(&bcdev->fw_update_ack,
				msecs_to_jiffies(bcdev->wls_fw_update_time_ms));
	if (!rc) {
		pr_err("Error, timed out updating firmware\n");
		rc = -ETIMEDOUT;
		goto release_fw;
	} else {
		pr_debug("Waited for %d ms\n",
			bcdev->wls_fw_update_time_ms - jiffies_to_msecs(rc));
		rc = 0;
	}

	pr_info("Wireless FW update done\n");

release_fw:
	bcdev->wls_fw_crc = 0;
	release_firmware(fw);
out:
	pm_relax(bcdev->dev);

	return rc;
}

static ssize_t wireless_fw_update_time_ms_store(const struct class *c,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	if (kstrtou32(buf, 0, &bcdev->wls_fw_update_time_ms))
		return -EINVAL;

	return count;
}

static ssize_t wireless_fw_update_time_ms_show(const struct class *c,
				const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%u\n", bcdev->wls_fw_update_time_ms);
}
static CLASS_ATTR_RW(wireless_fw_update_time_ms);

static ssize_t wireless_fw_crc_store(const struct class *c,
					const struct class_attribute *attr,
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

static ssize_t wireless_fw_version_show(const struct class *c,
					const struct class_attribute *attr,
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
		pr_err("Failed to get FW version rc=%d\n", rc);
		return rc;
	}

	return scnprintf(buf, PAGE_SIZE, "%#x\n", bcdev->wls_fw_version);
}
static CLASS_ATTR_RO(wireless_fw_version);

static ssize_t wireless_fw_force_update_store(const struct class *c,
					const struct class_attribute *attr,
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

static ssize_t wireless_fw_update_store(const struct class *c,
					const struct class_attribute *attr,
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

static ssize_t wireless_type_show(const struct class *c,
				const struct class_attribute *attr, char *buf)
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

static ssize_t charge_control_en_store(const struct class *c,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	if (val == bcdev->chg_ctrl_en)
		return count;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_BATTERY],
				BATT_CHG_CTRL_EN, val);
	if (rc < 0) {
		pr_err("Failed to set charge_control_en, rc=%d\n", rc);
		return rc;
	}

	bcdev->chg_ctrl_en = val;

	return count;
}

static ssize_t charge_control_en_show(const struct class *c,
				const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;

	rc = get_charge_control_en(bcdev);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->chg_ctrl_en);
}
static CLASS_ATTR_RW(charge_control_en);

static ssize_t usb_typec_compliant_show(const struct class *c,
				const struct class_attribute *attr, char *buf)
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

static ssize_t usb_real_type_show(const struct class *c,
				const struct class_attribute *attr, char *buf)
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

static ssize_t restrict_cur_store(const struct class *c,
				const struct class_attribute *attr,
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

static ssize_t restrict_cur_show(const struct class *c,
				const struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%u\n", bcdev->restrict_fcc_ua);
}
static CLASS_ATTR_RW(restrict_cur);

static ssize_t restrict_chg_store(const struct class *c,
				const struct class_attribute *attr,
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

static ssize_t restrict_chg_show(const struct class *c,
				const struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->restrict_chg_en);
}
static CLASS_ATTR_RW(restrict_chg);

static ssize_t fake_soc_store(const struct class *c,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	bcdev->fake_soc = val;
	pr_debug("Set fake soc to %d\n", val);
	write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
		XM_PROP_FAKE_SOC, bcdev->fake_soc);
	if (IS_ENABLED(CONFIG_QTI_PMIC_GLINK_CLIENT_DEBUG) && pst->psy)
		power_supply_changed(pst->psy);

	return count;
}

static ssize_t fake_soc_show(const struct class *c,
				const struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->fake_soc);
}
static CLASS_ATTR_RW(fake_soc);

static ssize_t wireless_boost_en_store(const struct class *c,
					const struct class_attribute *attr,
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

static ssize_t wireless_boost_en_show(const struct class *c,
					const struct class_attribute *attr,
					char *buf)
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

static ssize_t moisture_detection_en_store(const struct class *c,
					const struct class_attribute *attr,
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

static ssize_t moisture_detection_en_show(const struct class *c,
					const struct class_attribute *attr,
					char *buf)
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

static ssize_t moisture_detection_status_show(const struct class *c,
					const struct class_attribute *attr,
					char *buf)
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

static ssize_t resistance_show(const struct class *c,
					const struct class_attribute *attr,
					char *buf)
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

static ssize_t flash_active_show(const struct class *c,
					const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, F_ACTIVE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[F_ACTIVE]);
}
static CLASS_ATTR_RO(flash_active);

static ssize_t soh_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
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

static int battery_chg_ship_mode(struct battery_chg_dev *bcdev)
{
	struct battery_charger_ship_mode_req_msg msg = { { 0 } };
	int rc;

	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_SHIP_MODE_REQ_SET;
	msg.ship_mode_type = SHIP_MODE_PMIC;

	rc = battery_chg_write(bcdev, &msg, sizeof(msg));
	if (rc < 0)
		pr_emerg("Failed to write ship mode: %d\n", rc);

	return rc;
}

static ssize_t ship_mode_en_store(const struct class *c,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;

	if (kstrtobool(buf, &bcdev->ship_mode_en))
		return -EINVAL;

	if (bcdev->ship_mode_en && bcdev->ship_mode_immediate) {
		rc = battery_chg_ship_mode(bcdev);
		if (rc < 0)
			return rc;
	}

	return count;
}

static ssize_t ship_mode_en_show(const struct class *c,
				const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->ship_mode_en);
}
static CLASS_ATTR_RW(ship_mode_en);

#define BATT_PARALLEL_CELL_AVAIL_COUNT(val)		FIELD_GET(GENMASK(15, 8), val)
#define BATT_PARALLEL_CELL_TOTAL_COUNT(val)		FIELD_GET(GENMASK(7, 0), val)

static ssize_t battery_parallel_cell_count_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int rc;

	rc = read_property_id(bcdev, pst, BATT_PARALLEL_CELL_COUNT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "Parallel battery cell count (available/total): %lu/%lu\n",
		BATT_PARALLEL_CELL_AVAIL_COUNT(pst->prop[BATT_PARALLEL_CELL_COUNT]),
		BATT_PARALLEL_CELL_TOTAL_COUNT(pst->prop[BATT_PARALLEL_CELL_COUNT]));
}
static CLASS_ATTR_RO(battery_parallel_cell_count);

static ssize_t typec_mode_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_TYPEC_MODE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%s\n", power_supply_usbc_text[pst->prop[XM_PROP_TYPEC_MODE]]);
}
static CLASS_ATTR_RO(typec_mode);

static ssize_t typec_cc_orientation_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CC_ORIENTATION);
	if (rc < 0)
		return rc;

	pr_err("cc_orientation vlaue = %u\n", pst->prop[XM_PROP_CC_ORIENTATION]);
	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CC_ORIENTATION]);
}
static CLASS_ATTR_RO(typec_cc_orientation);

static ssize_t otg_ui_support_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_OTG_UI_SUPPORT);
	if (rc < 0)
			return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_OTG_UI_SUPPORT]);
}
static CLASS_ATTR_RO(otg_ui_support);

static ssize_t cid_status_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CID_STATUS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CID_STATUS]);
}
static CLASS_ATTR_RO(cid_status);

static ssize_t cc_toggle_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
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

static ssize_t cc_toggle_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CC_TOGGLE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CC_TOGGLE]);
}
static CLASS_ATTR_RW(cc_toggle);

static ssize_t screen_cctog_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_SCREEN_CCTOG, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t screen_cctog_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SCREEN_CCTOG);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SCREEN_CCTOG]);
}
static CLASS_ATTR_RW(screen_cctog);

static ssize_t audio_cctog_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_AUDIO_CCTOG, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t audio_cctog_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_AUDIO_CCTOG);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_AUDIO_CCTOG]);
}
static CLASS_ATTR_RW(audio_cctog);

static ssize_t verify_process_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
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

static ssize_t verify_process_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_VERIFY_PROCESS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_VERIFY_PROCESS]);
}
static CLASS_ATTR_RW(verify_process);

static ssize_t current_state_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CURRENT_STATE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usbpd_state_name(pst->prop[XM_PROP_CURRENT_STATE]));

}
static CLASS_ATTR_RO(current_state);

static ssize_t adapter_id_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_ADAPTER_ID);
	if (rc < 0)
		return rc;

	pr_err("adapter_svid_show:cmd svid:%04x\n", pst->prop[XM_PROP_ADAPTER_ID]);
	return scnprintf(buf, PAGE_SIZE, "%08x", pst->prop[XM_PROP_ADAPTER_ID]);
}
static CLASS_ATTR_RO(adapter_id);

static ssize_t adapter_svid_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_ADAPTER_SVID);
	if (rc < 0)
		return rc;

	pr_err("adapter_svid_show:cmd svid:%04x\n", pst->prop[XM_PROP_ADAPTER_SVID]);
	return scnprintf(buf, PAGE_SIZE, "%04x", pst->prop[XM_PROP_ADAPTER_SVID]);
}
static CLASS_ATTR_RO(adapter_svid);

static ssize_t pd_verifed_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_PD_VERIFED, val);
	bcdev->pd_verifed_dfx = val;
	pr_err("bcdev->pd_verifed_dfx = %d\n", bcdev->pd_verifed_dfx);
	if (rc < 0)
		return rc;
	if (val == 0) {
		dfx_pd_auth_check(bcdev, val);
	}
	return count;
}

static ssize_t pd_verifed_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_PD_VERIFED);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_PD_VERIFED]);
}
static CLASS_ATTR_RW(pd_verifed);

static ssize_t pdo2_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_PDO2);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%08x\n", pst->prop[XM_PROP_PDO2]);
}
static CLASS_ATTR_RO(pdo2);

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

static void usbpd_request_vdm_cmd(struct battery_chg_dev *bcdev,
			enum uvdm_state cmd, unsigned int *data)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	u32 prop_id, val = 0;
	int rc;

	pr_err("usbpd_request_vdm_cmd:cmd = %d, data = %d\n", cmd, *data);
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
		pr_err("SESSION_SEED:data = %d\n", val);
		break;
	case USBPD_UVDM_AUTHENTICATION:
		prop_id = XM_PROP_VDM_CMD_AUTHENTICATION;
		usbpd_sha256_bitswap32(data, USBPD_UVDM_SS_LEN);
		val = *data;
		pr_err("AUTHENTICATION:data = %d\n", val);
		break;
	case USBPD_UVDM_REVERSE_AUTHEN:
		prop_id = XM_PROP_VDM_CMD_REVERSE_AUTHEN;
		usbpd_sha256_bitswap32(data, USBPD_UVDM_SS_LEN);
		val = *data;
		pr_err("AUTHENTICATION:data = %d\n", val);
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
		pr_info("cmd:%d is not support\n", cmd);
		break;
	}

	if (cmd == USBPD_UVDM_SESSION_SEED || cmd == USBPD_UVDM_AUTHENTICATION ||
		cmd == USBPD_UVDM_REVERSE_AUTHEN) {
		rc = write_ss_auth_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
			prop_id, data);
	} else {
		rc = write_property_id(bcdev, pst, prop_id, val);
	}
}

static ssize_t request_vdm_cmd_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int cmd, ret;
	unsigned char buffer[64];
	unsigned char data[32];
	int ccount;

	ret = sscanf(buf, "%d,%s\n", &cmd, buffer);

	pr_info("%s:buf:%s cmd:%d, buffer:%s\n", __func__, buf, cmd, buffer);

	StringToHex(buffer, data, &ccount);
	usbpd_request_vdm_cmd(bcdev, cmd, (unsigned int *)data);
	return count;
}

static ssize_t request_vdm_cmd_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
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
	pr_info("request_vdm_cmd_show  uvdm_state: %d\n", cmd);

	switch (cmd) {
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
		pr_info("auth:0x%x 0x%x 0x%x 0x%x\n",
			bcdev->ss_auth_data[0], bcdev->ss_auth_data[1],
			bcdev->ss_auth_data[2], bcdev->ss_auth_data[3]);
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			memset(data, 0, sizeof(data));
			snprintf(data, sizeof(data), "%08x", bcdev->ss_auth_data[i]);
			strlcat(str_buf, data, sizeof(str_buf));
		}
		return snprintf(buf, PAGE_SIZE, "%d,%s", cmd, str_buf);
		break;
	default:
		pr_info("feedbak cmd:%d is not support\n", cmd);
		break;
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[prop_id]);
}
static CLASS_ATTR_RW(request_vdm_cmd);

static ssize_t cp_bus_current_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CP_BUS_CURRENT);
	if (rc < 0)
		return rc;

	pr_info("cp_bus_current value = %u\n", pst->prop[XM_PROP_CP_BUS_CURRENT]);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CP_BUS_CURRENT]);
}
static CLASS_ATTR_RO(cp_bus_current);

static ssize_t input_suspend_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	pr_err("input_suspend_store set input_suspend to %u\n", val);
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_INPUT_SUSPEND, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t input_suspend_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_INPUT_SUSPEND);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_INPUT_SUSPEND]);
}
static CLASS_ATTR_RW(input_suspend);

static ssize_t shipmode_count_reset_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;

	if (kstrtobool(buf, &bcdev->shipmode_en))
		return -EINVAL;

	pr_err("triggered shipmode_en: %d\n", bcdev->shipmode_en);

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_SHIPMODE_COUNT_RESET, bcdev->shipmode_en);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t shipmode_count_reset_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SHIPMODE_COUNT_RESET);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SHIPMODE_COUNT_RESET]);
}
static CLASS_ATTR_RW(shipmode_count_reset);

static ssize_t real_type_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_REAL_TYPE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(pst->prop[XM_PROP_REAL_TYPE]));
}
static CLASS_ATTR_RO(real_type);

static ssize_t mtbf_current_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	bcdev->mtbf_current = val;
	pr_err("mtbf_current_store set mtbf_current to %u\n", val);
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_MTBF_CURRENT, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t mtbf_current_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_MTBF_CURRENT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_MTBF_CURRENT]);
}
static CLASS_ATTR_RW(mtbf_current);

static ssize_t manufacturer_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", "qcom-PM7250b");
}
static CLASS_ATTR_RO(manufacturer);

static ssize_t StopCharging_Test_show(const struct class *c,
					const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;
	pr_err("set StopCharging_Test\n");
	val = true;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_INPUT_SUSPEND, val);
	if (rc < 0)
		return rc;
	return sprintf(buf, "chr=0\n");
}
static CLASS_ATTR_RO(StopCharging_Test);

static ssize_t StartCharging_Test_show(const struct class *c,
					const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;
	pr_err("set StartCharging_Test\n");
	val = false;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_INPUT_SUSPEND, val);
	if (rc < 0)
		return rc;
	return sprintf(buf, "chr=1\n");    
}
static CLASS_ATTR_RO(StartCharging_Test);

/*wait FACTORY_BUILD*/
#if 1
static ssize_t ato_soc_user_control_store(const struct class *c,
                                        const struct class_attribute *attr,
                                        const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                                battery_class);
	int rc;
	bool val;
	if (kstrtobool(buf, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
                                XM_PROP_ATO_SOC_USER_CONTROL, val);
	if (rc < 0)
		return rc;
	return count;
}

static ssize_t ato_soc_user_control_show(const struct class *c,
                                        const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                                battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_ATO_SOC_USER_CONTROL);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_ATO_SOC_USER_CONTROL]);
}
static CLASS_ATTR_RW(ato_soc_user_control);
#endif

static ssize_t apdo_max_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_APDO_MAX);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_APDO_MAX]);
}
static CLASS_ATTR_RO(apdo_max);

static ssize_t power_max_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_POWER_MAX);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_POWER_MAX]);
}
static CLASS_ATTR_RO(power_max);

static ssize_t quick_charge_type_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	int rc;
	union power_supply_propval val = { 0, };
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);

	rc = get_quick_charge_type(bcdev, &val);
	if (rc) {
		pr_err("Failed to get quick_charge_type, rc = %d \n", rc);
		return rc;
	}
	return scnprintf(buf, PAGE_SIZE, "%u", val.intval);
}
static CLASS_ATTR_RO(quick_charge_type);

static ssize_t batt_id_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	if (bcdev->batt_id == -1) {
		rc = read_property_id(bcdev, pst, XM_PROP_BATT_ID);
		if (rc < 0)
			return rc;
		pr_info("value = %u\n", pst->prop[XM_PROP_BATT_ID]);
		bcdev->batt_id = pst->prop[XM_PROP_BATT_ID];
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", bcdev->batt_id);
}
static CLASS_ATTR_RO(batt_id);

static ssize_t chip_ok_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	if (bcdev->chip_ok == 0) {
		rc = read_property_id(bcdev, pst, XM_PROP_CHIP_OK);
		if (rc < 0)
			return rc;
		bcdev->chip_ok = pst->prop[XM_PROP_CHIP_OK];
	}

	pr_info("value = %u\n", bcdev->chip_ok);

	return scnprintf(buf, PAGE_SIZE, "%u\n", bcdev->chip_ok);
}
static CLASS_ATTR_RO(chip_ok);

static ssize_t authentic_store(const struct class *c,
			const struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc, val;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	pr_err("authentic_store set value to %u\n", val);
	bcdev->batt_authentic = val;
	if (!bcdev->batt_authentic)
		xm_handle_dfx_report(CHG_DFX_BATTERY_AUTH_FAIL);

	rc = write_property_id(bcdev, pst, XM_PROP_AUTHENTIC, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t authentic_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_AUTHENTIC);
	if (rc < 0)
		return rc;
	pr_info(" value = %u\n", pst->prop[XM_PROP_AUTHENTIC]);

	if ((pst->prop[XM_PROP_AUTHENTIC] == 0) && (bcdev->batt_authentic == 1)) {
		rc = write_property_id(bcdev, pst, XM_PROP_AUTHENTIC, 1);
		if (rc < 0)
			return rc;
		return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_AUTHENTIC]);
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_AUTHENTIC]);
}
static CLASS_ATTR_RW(authentic);

static ssize_t verify_digest_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int i;
	u8 random[BATTERY_DIGEST_LEN+1] = { 0 };
	char kbuf[70] = { 0 };

	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);
	StringToHex(kbuf, random, &i);

	pr_err("VERIFY DIGEST \n");
	rc = write_batt_auth_prop_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_VERIFY_DIGEST, random);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t verify_digest_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	u8 digest_buf[4];
	int i;
	int len;
	int rc;

	rc = read_batt_auth_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM], XM_PROP_VERIFY_DIGEST);
	if (rc < 0)
		return rc;

	for (i = 0; i < BATTERY_DIGEST_LEN; i++) {
		memset(digest_buf, 0, sizeof(digest_buf));
		snprintf(digest_buf, sizeof(digest_buf) - 1, "%02x", bcdev->digest[i]);
		strlcat(buf, digest_buf, BATTERY_DIGEST_LEN * 2 + 1);
	}

	len = strlen(buf);
	buf[len] = '\0';
	pr_err("verify_digest_show :%s \n", buf);
	return strlen(buf) + 1;
}
static CLASS_ATTR_RW(verify_digest);


static ssize_t fg1_soh_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_SOH);
	if (rc < 0)
		return rc;
	pr_info(" value = %u\n", pst->prop[XM_PROP_FG1_SOH]);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG1_SOH]);
}
static CLASS_ATTR_RO(fg1_soh);


static ssize_t cis_alert_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CIS_ALERT_LEVEL);
	if (rc < 0)
		return rc;
	pr_info("XM_PROP_CIS_ALERT_LEVEL value = %u\n", pst->prop[XM_PROP_CIS_ALERT_LEVEL]);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CIS_ALERT_LEVEL]);
}

static ssize_t cis_alert_store(const struct class *c,
			const struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int val;
	int rc;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM], XM_PROP_CIS_ALERT_LEVEL, val);
	if (rc < 0) {
		pr_err("set cis_alert val = %d fail\n", val);
		return rc;
	}
	pr_info("XM_PROP_CIS_ALERT_LEVEL val = %u\n", val);
	return count;
}
static CLASS_ATTR_RW(cis_alert);

static ssize_t clear_cycle_flag_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
  	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CLEAR_CYCLE_FLAG);
	if (rc < 0)
  		return rc;
  	pr_info("clear_cycle_flag value = %u\n", pst->prop[XM_PROP_CLEAR_CYCLE_FLAG]);
	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CLEAR_CYCLE_FLAG]);
}
static CLASS_ATTR_RO(clear_cycle_flag);

static ssize_t clear_cycle_store(const struct class *c,
			const struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int val = 0;
	int rc;
	char reset_key[16] = {
		'c',  'l',  'r',  'c',  'l',  's',  '\0', '\0',
		'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
	};

	if (count < 6) {
		return -EINVAL;
	}
	pr_err("XM_PROP_RESET_CYCLE enter\n");
	if (memcmp(buf, reset_key, strlen(reset_key))) {
		pr_err("%s: key error\n", __func__);
		return -EINVAL;
	}
	val = 1;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM], XM_PROP_CLEAR_CYCLE, val);
	if (rc < 0) {
		pr_err("XM_PROP_RESET_CYCLE rc = %d fail\n", rc);
		return rc;
	}
	return count;
}

static CLASS_ATTR_WO(clear_cycle);

static ssize_t soa_alert_show(const struct class *c,
                       const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SOA_ALERT_LEVEL);
	if (rc < 0)
		return rc;
	pr_info("XM_PROP_SOA_ALERT_LEVEL value = %u\n", pst->prop[XM_PROP_SOA_ALERT_LEVEL]);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SOA_ALERT_LEVEL]);
}
static CLASS_ATTR_RO(soa_alert);

static ssize_t fg1_rm_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_RM);
	if (rc < 0)
		return rc;
	pr_info(" value = %u\n", pst->prop[XM_PROP_FG1_RM]);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FG1_RM]);
}
static CLASS_ATTR_RO(fg1_rm);

static ssize_t resistance_id_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_RESISTANCE_ID);
	if (rc < 0)
		return rc;
	pr_info(" value = %u\n", pst->prop[XM_PROP_RESISTANCE_ID]);

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_RESISTANCE_ID]);
}
static CLASS_ATTR_RO(resistance_id);

static ssize_t soc_decimal_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SOC_DECIMAL);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_SOC_DECIMAL]);
}
static CLASS_ATTR_RO(soc_decimal);

static ssize_t soc_decimal_rate_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SOC_DECIMAL_RATE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_SOC_DECIMAL_RATE]);
}
static CLASS_ATTR_RO(soc_decimal_rate);

static ssize_t reverse_quick_charge_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int val;
	int rc;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
		XM_PROP_REVERSE_QUICK_CHARGE, val);
	if (rc < 0){
		pr_err("set reverse quick charge val = %d\n", val);
		return rc;
	}

	return count;
}

static ssize_t reverse_quick_charge_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_REVERSE_QUICK_CHARGE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_REVERSE_QUICK_CHARGE]);
}
static CLASS_ATTR_RW(reverse_quick_charge);

static ssize_t source_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int val;
	int rc;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	if (val == 8)
		val = 2;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
		XM_PROP_FORCE_ROLE, val);
	if (rc < 0){
		pr_err("set reverse quick charge val = %d\n", val);
		return rc;
	}

	return count;
}

static ssize_t source_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FORCE_ROLE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_FORCE_ROLE]);
}
static CLASS_ATTR_RW(source);

static ssize_t fake_temp_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	pr_err("fake_temp_store set temp to %d\n", val);
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FAKE_TEMP, val);
	if (rc < 0)
		return rc;
	return count;
}

static ssize_t fake_temp_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FAKE_TEMP);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FAKE_TEMP]);
}
static CLASS_ATTR_RW(fake_temp);

static ssize_t fake_cycle_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	pr_err("fake_cycle_store set cycle to %u\n", val);

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FAKE_CYCLE, val);
	if (rc < 0)
		return rc;
	return count;
}

static ssize_t fake_cycle_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FAKE_CYCLE);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FAKE_CYCLE]);
}
static CLASS_ATTR_RW(fake_cycle);

static ssize_t batt_manufacturer_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	if (bcdev->batt_id == -1) {
		rc = read_property_id(bcdev, pst, XM_PROP_BATT_ID);
		if (rc < 0)
			return rc;
		bcdev->batt_id = pst->prop[XM_PROP_BATT_ID];
	}

	return scnprintf(buf, PAGE_SIZE, "%s", batt_manufacturer_text[bcdev->batt_id]);
}
static CLASS_ATTR_RO(batt_manufacturer);

static ssize_t battery_type_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	if (bcdev->batt_id == -1) {
		rc = read_property_id(bcdev, pst, XM_PROP_BATT_ID);
		if (rc < 0)
			return rc;
		bcdev->batt_id = pst->prop[XM_PROP_BATT_ID];
	}

	return scnprintf(buf, PAGE_SIZE, "%s", battery_type_text[bcdev->batt_id]);
}
static CLASS_ATTR_RO(battery_type);

static ssize_t cp_manufacturer_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	if (bcdev->cp_index == -1) {
		rc = read_property_id(bcdev, pst, XM_PROP_CP_MANUFACTURE);
		if (rc < 0)
			return rc;
	}

	bcdev->cp_index = pst->prop[XM_PROP_CP_MANUFACTURE];

	return scnprintf(buf, PAGE_SIZE, "%s\n", cp_manufacturer_text[bcdev->cp_index]);
}
static CLASS_ATTR_RO(cp_manufacturer);

static ssize_t thermal_level_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->curr_thermal_level);
}
static CLASS_ATTR_RO(thermal_level);

static ssize_t shutdown_delay_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,	battery_class);
	int val;

	if (kstrtou32(buf, 10, &val))
		return -EINVAL;
	bcdev->shutdown_delay_en = val;

	pr_err("use contral shutdown delay featue enable= %d\n", bcdev->shutdown_delay_en);
	return count;
}

static ssize_t shutdown_delay_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,	battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SHUTDOWN_DELAY);
	if (rc < 0)
		return rc;
	if (!bcdev->shutdown_delay_en)
		pst->prop[XM_PROP_SHUTDOWN_DELAY] = 0;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_SHUTDOWN_DELAY]);
}
static CLASS_ATTR_RW(shutdown_delay);

static ssize_t raw_soc_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,	battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_RAW_SOC);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_RAW_SOC]);
}
static CLASS_ATTR_RO(raw_soc);

/* xm smart charge start */
static ssize_t smart_batt_store(const struct class *c,
		const struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int val;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, pst, XM_PROP_SMART_BATT, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t smart_batt_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];

	rc = read_property_id(bcdev, pst, XM_PROP_SMART_BATT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SMART_BATT]);
}
static CLASS_ATTR_RW(smart_batt);

/* xm smart fv start */
static ssize_t smart_fv_store(const struct class *c,
		const struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int val;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, pst, XM_PROP_SMART_FV, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t smart_fv_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];

	rc = read_property_id(bcdev, pst, XM_PROP_SMART_FV);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SMART_FV]);
}
static CLASS_ATTR_RW(smart_fv);

static ssize_t smart_chg_store(const struct class *c, const struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	u32 val;

	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;
    pr_err("set smart charging engine, u:%u, d:%d, 0x:0x%0x\n", val, val, val);
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM], XM_PROP_SMART_CHG, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t smart_chg_show(const struct class *c, const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SMART_CHG);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_SMART_CHG]);
}
static CLASS_ATTR_RW(smart_chg);

static ssize_t night_charging_store(const struct class *c,
			const struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int val;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	pr_err("set night charging %d\n", val);

	rc = write_property_id(bcdev, pst, XM_PROP_NIGHT_CHARGING, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t night_charging_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_NIGHT_CHARGING);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_NIGHT_CHARGING]);
}
static CLASS_ATTR_RW(night_charging);

static ssize_t manufacturing_date_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;

	rc = read_batt_auth_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM], XM_PROP_MANUFACTURING_DATA);
	if (rc < 0)
		return rc;

	memcpy(buf, bcdev->manufacturing_date, BATTERY_DATE_LEN);
	//buf[BATTERY_DATE_LEN] = '\0';
	pr_err("manufacturing_date_show :%s \n", buf);

	return strlen(buf);
}
static CLASS_ATTR_RO(manufacturing_date);

static ssize_t first_usage_date_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	char date_str[32];
	int i, rc = 0, date_len;
	int j = 0;

	date_len = strlen(buf);
	for (i = 0; i < date_len; i++) {
		if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\n') {
			if (j >= BATTERY_DATE_LEN ) {
				pr_err("date length too large\n");
				return -E2BIG;
			}
			if (buf[i] < '0' || buf[i] > '9') {
				pr_err("date has invalid char:%c(0x%02x)\n", buf[i], buf[i]);
				return -EINVAL;
			}
			date_str[j++] = buf[i];
		}
	}
	pr_debug("activiate date hex: %02X %02X %02X %02X %02X %02X %02X %02X(%c%c%c%c%c%c%c%c)\n",
		date_str[0], date_str[1], date_str[2], date_str[3], date_str[4], date_str[5],  date_str[6],  date_str[7],
		date_str[0], date_str[1], date_str[2], date_str[3], date_str[4], date_str[5],  date_str[6],  date_str[7]);

	rc = write_batt_auth_prop_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_FIRST_USAGE_DATE, date_str);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t first_usage_date_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;

	rc = read_batt_auth_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM], XM_PROP_FIRST_USAGE_DATE);
	if (rc < 0)
		return rc;

	if (rc) {
		memset(buf, '9', BATTERY_DATE_LEN);
		buf[BATTERY_DATE_LEN] = '\0';
		pr_err("failed to get FG_MAC_CMD_MANU_INFOC:%d\n", rc);
	} else {
		pr_err("battery activiate date hex: %02X%02X%02X\n",
			bcdev->manuInfoC_data[11], bcdev->manuInfoC_data[12], bcdev->manuInfoC_data[13]);
		if (bcdev->manuInfoC_data[11] == 0x00 && bcdev->manuInfoC_data[12] == 0x00 && bcdev->manuInfoC_data[13] == 0x00) {
			memset(buf, '0', BATTERY_DATE_LEN);
			buf[BATTERY_DATE_LEN] = '\0';
			pr_err("reset data to 0\n");
		} else {
			buf[0] = '2';
			buf[1] = '0';
			buf[2] = '0' + bcdev->manuInfoC_data[11] / 10;
			buf[3] = '0' + bcdev->manuInfoC_data[11] % 10;
			buf[4] = '0' + bcdev->manuInfoC_data[12] / 10;
			buf[5] = '0' + bcdev->manuInfoC_data[12] % 10;
			buf[6] = '0' + bcdev->manuInfoC_data[13] / 10;
			buf[7] = '0' + bcdev->manuInfoC_data[13] % 10;
			buf[8] = '\0';
			pr_err("battery activiate date string:%s\n", buf);
		}
	}

	return strlen(buf);
}
static CLASS_ATTR_RW(first_usage_date);

static ssize_t ui_soh_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	char t_data[70] = {0};
	char *pchar = NULL, *qchar = NULL;
	u8 ui_soh_data[40] = {0,};
	int ret = 0, i = 0;
	u8 val = 0;

	pr_err("%s raw data buf: %s \n", __func__, buf);
	memset(t_data, 0, sizeof(t_data));
	strncpy(t_data, buf, count);
	pr_err("%s t_data : %s\n", __func__, t_data);

	qchar = t_data;

	while ((pchar = strsep(&qchar, " ")))
	{
		ret = kstrtou8(pchar, 10, &val);
		if (ret < 0) {
			pr_err("kstrtou8 error return %d \n", ret);
			return count;
		}
		ui_soh_data[i] = val;
		val = 0;
		pr_err("%s ui_soh_data[%d]: %d \n", __func__ ,i, ui_soh_data[i]);
		i++;
	}

	ret = write_batt_auth_prop_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_UI_SOH, ui_soh_data);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t ui_soh_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;

	rc = read_batt_auth_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM], XM_PROP_UI_SOH);
	if (rc < 0)
		return rc;

	return snprintf(buf, PAGE_SIZE, "%u %u %u %u %u %u %u %u %u %u %u\n",
		bcdev->manuInfoC_data[0], bcdev->manuInfoC_data[1], bcdev->manuInfoC_data[2], bcdev->manuInfoC_data[3], bcdev->manuInfoC_data[4],
		bcdev->manuInfoC_data[5], bcdev->manuInfoC_data[6], bcdev->manuInfoC_data[7], bcdev->manuInfoC_data[8], bcdev->manuInfoC_data[9], bcdev->manuInfoC_data[10]);

}
static CLASS_ATTR_RW(ui_soh);

static ssize_t soh_sn_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	u8 soh_sn_data[4];
	int i;
	int len;
	int rc;

	rc = read_batt_auth_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM], XM_PROP_SOH_SN);
	if (rc < 0)
		return rc;

	for (i = 0; i < BATTERY_DIGEST_LEN; i++) {
		memset(soh_sn_data, 0, sizeof(soh_sn_data));
		snprintf(soh_sn_data, sizeof(soh_sn_data) - 1, "%c", bcdev->soh_sn_data[i]);
		strlcat(buf, soh_sn_data, BATTERY_DIGEST_LEN * 2 + 1);
	}

	len = strlen(buf);
	buf[len] = '\0';
	pr_err("soh_sn_show :%s \n", buf);
	return strlen(buf) + 1;
}
static CLASS_ATTR_RO(soh_sn);

static ssize_t soh_new_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,	battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	static unsigned int last_soh = 100;
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_SOH_NEW);
	if (rc < 0 || 0 == pst->prop[XM_PROP_SOH_NEW])
		return scnprintf(buf, PAGE_SIZE, "%u", last_soh);

	last_soh = pst->prop[XM_PROP_SOH_NEW];
	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_SOH_NEW]);
}
static CLASS_ATTR_RO(soh_new);

static ssize_t fast_chg_mode_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FAST_CHG_MODE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_FAST_CHG_MODE]);
}
static CLASS_ATTR_RO(fast_chg_mode);

static ssize_t lpd_status_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);

	pr_info("lpd status:%d\n",bcdev->lpd_status);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->lpd_status);
}
static CLASS_ATTR_RO(lpd_status);

static ssize_t lpd_charging_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int rc;
	int val;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];

	if (kstrtos32(buf, 0, &val))
		return -EINVAL;

	bcdev->lpd_charging = val;

	rc = write_property_id(bcdev, pst, XM_PROP_LPD_CHARGING, bcdev->lpd_charging);
	if (rc < 0) {
		pr_err("Failed to set LPD charging status: %d\n", rc);
		return rc;
        }

	return count;
}

static ssize_t lpd_charging_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->lpd_charging);
}
static CLASS_ATTR_RW(lpd_charging);

static ssize_t lpd_control_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	int val;

	if (kstrtos32(buf, 0, &val))
		return -EINVAL;
	bcdev->lpd_control = val;

	return count;
}

static ssize_t lpd_control_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->lpd_control);
}
static CLASS_ATTR_RW(lpd_control);

static ssize_t ntc_temp_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_POWER_TEMP_RAW);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_POWER_TEMP_RAW]);
}
static CLASS_ATTR_RO(ntc_temp);

static ssize_t debug_temp_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_POWER_TEMP_DEBUG);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_POWER_TEMP_DEBUG]);
}
static CLASS_ATTR_RO(debug_temp);

static ssize_t pack_vendor_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BATT_ID);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_BATT_ID]);
}
static CLASS_ATTR_RO(pack_vendor);

static ssize_t fg1_df_check_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_DF_CHECK);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_DF_CHECK]);
}
static CLASS_ATTR_RO(fg1_df_check);

static ssize_t fg1_chemid_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CHEM_DF_SIGN);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_CHEM_DF_SIGN]);
}
static CLASS_ATTR_RO(fg1_chemid);

static ssize_t fg1_cycle_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_FG1_CYCLE);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FG1_CYCLE]);
}
static CLASS_ATTR_RO(fg1_cycle);

static ssize_t dfx_event_test_store(const struct class *c,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int val;

	if (kstrtos32(buf, 0, &val))
		return -EINVAL;

	if (val >= 0 && val < CHG_DFX_MAX_INDEX)
	{
		if(val == CHG_DFX_UISOC_NOT_FULL)
		{
			dfs_data_batt.uisoc = 90;
			dfs_data_batt.vbat = 4300;
			dfs_data_batt.rawsoc = 80;
		}
		pr_err("CHG:DFX: test report %d\n",val);
		xm_handle_dfx_report(val);
	}

	return count;
}
static CLASS_ATTR_WO(dfx_event_test);

static ssize_t reverse_quick_charge_status_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_REVERSE_QUICK_CHARGE_STATUS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_REVERSE_QUICK_CHARGE_STATUS]);
}
static CLASS_ATTR_RO(reverse_quick_charge_status);

static ssize_t revchg_bcl_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc = 0;

	rc = read_property_id(bcdev, pst, XM_PROP_REVCHG_BCL);
	if (rc < 0)
		return rc;

	pr_err("chargerPartition %s revchg_val: %d \n", __func__, pst->prop[XM_PROP_REVCHG_BCL]);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_REVCHG_BCL]);
}

static ssize_t revchg_bcl_store(const struct class *c,
			const struct class_attribute *attr,
			const char *ubuf, size_t len)
{
	int rc = 0;
	int val = 0;
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);

	rc = kstrtoint(ubuf, 10, &val);
	if (rc) {
		pr_err("%s kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM], XM_PROP_REVCHG_BCL, val);
	if (rc < 0) {
		pr_err("XM_PROP_REVCHG_BCL rc = %d fail\n", rc);
		return rc;
	}

	return len;
}
static CLASS_ATTR_RW(revchg_bcl);

static ssize_t dod_count_show(const struct class *c,
	const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_BATT_DOD_COUNT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_BATT_DOD_COUNT]);;
}

static ssize_t dod_count_store(const struct class *c,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	u32 val;

	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;
	pr_info("set dod count %d\n", val);
	rc = write_property_id(bcdev, pst, XM_PROP_BATT_DOD_COUNT, val);
	if (rc < 0)
		return rc;
	return count;
}
static CLASS_ATTR_RW(dod_count);

static ssize_t cc_short_vbus_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CC_SHORT_VBUS);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_CC_SHORT_VBUS]);
}
static CLASS_ATTR_RO(cc_short_vbus);

static ssize_t charger_partition_poweroffmode_store(const struct class *c,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	bool power_off_mode = 0;
	if (kstrtobool(buf, &power_off_mode))
		return -EINVAL;
	if (power_off_mode)
		charger_partition_set_prop(CHARGER_PARTITION_PROP_POWER_OFF_MODE, 1);
	else
		charger_partition_set_prop(CHARGER_PARTITION_PROP_POWER_OFF_MODE, 2);
	return count;
}
static ssize_t charger_partition_poweroffmode_show(const struct class *c,
					const struct class_attribute *attr, char *buf)
{
	uint32_t power_off_mode = 0;
	charger_partition_get_prop(CHARGER_PARTITION_PROP_POWER_OFF_MODE, &power_off_mode);
	return scnprintf(buf, PAGE_SIZE, "%u\n", power_off_mode);
}
static CLASS_ATTR_RW(charger_partition_poweroffmode);

static ssize_t charger_partition_mishow_store(const struct class *c,
					const struct class_attribute *attr,
					const char *ubuf, size_t count)
{
	int mishow = 0;
	if (kstrtoint(ubuf, 10, &mishow))
		return -EINVAL;
	if (mishow) {
		pr_err("charger_partition_mishow_store 1");
		charger_partition_set_prop(CHARGER_PARTITION_PROP_MISHOW, 1);
	}
	else{
          	pr_err("charger_partition_mishow_store 0");
		charger_partition_set_prop(CHARGER_PARTITION_PROP_MISHOW, 0);
        }
	return count;
}

static ssize_t charger_partition_mishow_show(const struct class *c,
					const struct class_attribute *attr, char *buf)
{
	uint32_t mishow = 0;
	charger_partition_get_prop(CHARGER_PARTITION_PROP_MISHOW, &mishow);
	return scnprintf(buf, PAGE_SIZE, "%u\n", mishow);
}
static CLASS_ATTR_RW(charger_partition_mishow);

static ssize_t is_eu_model_show(const struct class *c,
			const struct class_attribute *attr, char *ubuf)
{
	int rc = 0;
	int val = 0;
	rc = charger_partition_get_prop(CHARGER_PARTITION_PROP_EU_MODE, &val);
	if (rc < 0) {
		pr_err("chargerPartition %s get eu_mode from charger parition failed, ret = %d\n", __func__, rc);
		return -EINVAL;
	}
	pr_err("chargerPartition %s eu_mode_val: %d \n", __func__, val);
	return scnprintf(ubuf, PAGE_SIZE, "%d\n", val);
}

static ssize_t is_eu_model_store(const struct class *c,const struct class_attribute *attr,const char *ubuf, size_t len)
{
	int rc = 0;
	int val = 0;
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);

	rc = kstrtoint(ubuf, 10, &val);
	if (rc) {
		pr_err("%s kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM], XM_PROP_EU_MODE, val);
	if (rc < 0) {
		pr_err("XM_PROP_EU_MODE rc = %d fail\n", rc);
		return rc;
	}
	rc = charger_partition_set_prop(CHARGER_PARTITION_PROP_EU_MODE, val);
	if (rc < 0) {
		pr_err("chargerPartition %s set eu_mode to charger parition failed, ret = %d\n", __func__, rc);
		return -EINVAL;
	}
	return len;
}
static CLASS_ATTR_RW(is_eu_model);

static ssize_t thermal_board_temp_store(const struct class *c,const struct class_attribute *attr,const char *buf, size_t count)
{
        struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
        struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
        int rc, val;

        if (kstrtoint(buf, 10, &val))
                return -EINVAL;

        rc = write_property_id(bcdev, pst, XM_PROP_THERMAL_BOARD_TEMP, val);

        if (rc < 0)
                return rc;

        return count;
}

static ssize_t thermal_board_temp_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
        struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
        struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
        int rc;

        rc = read_property_id(bcdev, pst, XM_PROP_THERMAL_BOARD_TEMP);
        if (rc < 0)
                return rc;

        return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_THERMAL_BOARD_TEMP]);
}
static CLASS_ATTR_RW(thermal_board_temp);

static ssize_t thermal_scene_store(const struct class *c,const struct class_attribute *attr,const char *buf, size_t count)
{
        struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
        struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
        int rc, val;

        if (kstrtoint(buf, 10, &val))
                return -EINVAL;

        rc = write_property_id(bcdev, pst, XM_PROP_THERMAL_SCENE, val);
        if (rc < 0)
                return rc;

        return count;
}

static ssize_t thermal_scene_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
        struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
        struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
        int rc;

        rc = read_property_id(bcdev, pst, XM_PROP_THERMAL_SCENE);
        if (rc < 0)
                return rc;

        return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_THERMAL_SCENE]);
}
static CLASS_ATTR_RW(thermal_scene);

int get_board_id_from_dtb(char *board_mem) {
      struct device_node *proj_info_node;
      const char *read_board_id;
      int ret;

      proj_info_node = of_find_node_by_name(NULL,"project-info");
      if (NULL == proj_info_node){
         pr_err("%s:device project-info node not exist.",__func__);
         return -1;
      }
      ret = of_property_read_string(proj_info_node,"board_id",&read_board_id);
      if (ret != 0) {
          pr_err("%s:device board id read fail:ret=%d.",__func__,ret);
          return ret;
      }
      strlcpy(board_mem,read_board_id,16);
      pr_err("%s:device board id read success:%s,%s",__func__,read_board_id,board_mem);
      return ret;
}

static ssize_t batt_global_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_BATT_IS_GLOBAL);
	if (rc < 0)
		return rc;
	pr_err("XM_PROP_BATT_IS_GLOBAL =%d",pst->prop[XM_PROP_BATT_IS_GLOBAL]);
	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_BATT_IS_GLOBAL]);
}

static ssize_t batt_global_store(const struct class *c,const struct class_attribute *attr,const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;
	if (kstrtobool(buf, &val))
		return -EINVAL;
	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_BATT_IS_GLOBAL, val);
	if (rc < 0)
		return rc;
	return count;
}
static CLASS_ATTR_RW(batt_global);

static ssize_t is_cp_enabled_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_IS_CP_ENABLED);
	if (rc < 0)
		return rc;
	pr_err("XM_PROP_IS_CP_ENABLED =%d",pst->prop[XM_PROP_IS_CP_ENABLED]);
	return scnprintf(buf, PAGE_SIZE, "%u", pst->prop[XM_PROP_IS_CP_ENABLED]);
}
static CLASS_ATTR_RO(is_cp_enabled);

static ssize_t calc_rvalue_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CALC_RVALUE);
	if (rc < 0)
		return rc;

	pr_err("%s CALC_RVALUE:%d\n", __func__, pst->prop[XM_PROP_CALC_RVALUE]);
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_CALC_RVALUE]);
}
static CLASS_ATTR_RO(calc_rvalue);

static struct attribute *battery_class_attrs[] = {
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_flash_active.attr,
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
	&class_attr_charge_control_en.attr,
	&class_attr_battery_parallel_cell_count.attr,
	&class_attr_typec_mode.attr,
	&class_attr_typec_cc_orientation.attr,
	&class_attr_otg_ui_support.attr,
	&class_attr_cid_status.attr,
	&class_attr_cc_toggle.attr,
	&class_attr_screen_cctog.attr,
	&class_attr_audio_cctog.attr,
	&class_attr_verify_process.attr,
	&class_attr_current_state.attr,
	&class_attr_adapter_id.attr,
	&class_attr_adapter_svid.attr,
	&class_attr_pd_verifed.attr,
	&class_attr_pdo2.attr,
	&class_attr_request_vdm_cmd.attr,
	&class_attr_cp_bus_current.attr,
	&class_attr_input_suspend.attr,
	&class_attr_shipmode_count_reset.attr,
	&class_attr_real_type.attr,
	&class_attr_mtbf_current.attr,
	&class_attr_manufacturer.attr,
	&class_attr_StopCharging_Test.attr,
	&class_attr_StartCharging_Test.attr,
/*wait FACTORY_BUILD*/
#if 1
	&class_attr_ato_soc_user_control.attr,
#endif
	&class_attr_apdo_max.attr,
	&class_attr_power_max.attr,
	&class_attr_quick_charge_type.attr,
	&class_attr_batt_id.attr,
	&class_attr_chip_ok.attr,
	&class_attr_authentic.attr,
	&class_attr_verify_digest.attr,
	&class_attr_fg1_soh.attr,
	&class_attr_cis_alert.attr,
	&class_attr_clear_cycle.attr,
	&class_attr_clear_cycle_flag.attr,
	&class_attr_soa_alert.attr,
	&class_attr_fg1_rm.attr,
	&class_attr_resistance_id.attr,
	&class_attr_soc_decimal.attr,
	&class_attr_soc_decimal_rate.attr,
	&class_attr_fake_temp.attr,
	&class_attr_fake_cycle.attr,
	&class_attr_batt_manufacturer.attr,
	&class_attr_battery_type.attr,
	&class_attr_cp_manufacturer.attr,
	&class_attr_thermal_level.attr,
	&class_attr_shutdown_delay.attr,
	&class_attr_raw_soc.attr,
	&class_attr_smart_batt.attr,
	&class_attr_smart_fv.attr,
	&class_attr_smart_chg.attr,
	&class_attr_night_charging.attr,
	&class_attr_manufacturing_date.attr,
	&class_attr_first_usage_date.attr,
	&class_attr_ui_soh.attr,
	&class_attr_soh_sn.attr,
	&class_attr_soh_new.attr,
	&class_attr_reverse_quick_charge.attr,
	&class_attr_source.attr,
	&class_attr_fast_chg_mode.attr,
	&class_attr_lpd_status.attr,
	&class_attr_lpd_charging.attr,
	&class_attr_lpd_control.attr,
	&class_attr_ntc_temp.attr,
	&class_attr_debug_temp.attr,
	&class_attr_dfx_event_test.attr,
	&class_attr_reverse_quick_charge_status.attr,
	&class_attr_revchg_bcl.attr,
	&class_attr_dod_count.attr,
	&class_attr_pack_vendor.attr,
	&class_attr_fg1_df_check.attr,
	&class_attr_fg1_chemid.attr,
	&class_attr_fg1_cycle.attr,
	&class_attr_cc_short_vbus.attr,
	&class_attr_is_eu_model.attr,
	&class_attr_thermal_board_temp.attr,
	&class_attr_charger_partition_poweroffmode.attr,
	&class_attr_charger_partition_mishow.attr,
	&class_attr_thermal_scene.attr,
	&class_attr_batt_global.attr,
	&class_attr_is_cp_enabled.attr,
	&class_attr_calc_rvalue.attr,
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
		pr_err("Failed to create charger debugfs directory, rc=%d\n",
			rc);
		return;
	}

	bcdev->debugfs_dir = dir;
	debugfs_create_bool("block_tx", 0600, dir, &bcdev->block_tx);
}
#else
static void battery_chg_add_debugfs(struct battery_chg_dev *bcdev) { }
#endif

static int battery_chg_parse_dt(struct battery_chg_dev *bcdev)
{
	struct device_node *node = bcdev->dev->of_node;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int i, rc, len;
	u32 prev, val;

	of_property_read_string(node, "qcom,wireless-fw-name",
				&bcdev->wls_fw_name);

	of_property_read_u32(node, "qcom,shutdown-voltage",
				&bcdev->shutdown_volt_mv);

	bcdev->lpd_enable = of_property_read_bool(node, "qcom,lpd_enable");
    pr_err("battery_chg_parse_dt, bcdev->lpd_enable=%d\n",bcdev->lpd_enable);
	//bcdev->lpd_enable=1;// for test
	bcdev->ship_mode_immediate = of_property_read_bool(node, "qcom,ship-mode-immediate");

	rc = read_property_id(bcdev, pst, BATT_CHG_CTRL_LIM_MAX);
	if (rc < 0) {
		pr_err("Failed to read prop BATT_CHG_CTRL_LIM_MAX, rc=%d\n",
			rc);
		return rc;
	}

	rc = of_property_count_elems_of_size(node, "qcom,thermal-mitigation",
						sizeof(u32));
	if (rc <= 0) {

		rc = of_property_read_u32(node, "qcom,thermal-mitigation-step",
						&val);

		if (rc < 0)
			return 0;

		if (val < 500000 || val >= pst->prop[BATT_CHG_CTRL_LIM_MAX]) {
			pr_err("thermal_fcc_step %d is invalid\n", val);
			return -EINVAL;
		}

		bcdev->thermal_fcc_step = val;
		len = pst->prop[BATT_CHG_CTRL_LIM_MAX] / bcdev->thermal_fcc_step;

		/*
		 * FCC values must be above 500mA.
		 * Since len is truncated when calculated, check and adjust len so
		 * that the above requirement is met.
		 */
		if (pst->prop[BATT_CHG_CTRL_LIM_MAX] - (bcdev->thermal_fcc_step * len) < 500000)
			len = len - 1;
	} else {
		bcdev->thermal_fcc_step = 0;
		len = rc;
		prev = pst->prop[BATT_CHG_CTRL_LIM_MAX];

		for (i = 0; i < len; i++) {
			rc = of_property_read_u32_index(node, "qcom,thermal-mitigation",
				i, &val);
			if (rc < 0)
				return rc;

			if (val > prev) {
				pr_err("Thermal levels should be in descending order\n");
				bcdev->num_thermal_levels = -EINVAL;
				return 0;
			}

			prev = val;
		}

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
	}

	bcdev->num_thermal_levels = len;
	bcdev->thermal_fcc_ua = pst->prop[BATT_CHG_CTRL_LIM_MAX];

	return 0;
}

static int battery_chg_reboot_notify(struct notifier_block *nb, unsigned long code,
		void *unused)
{
	struct battery_charger_notify_msg msg_notify = { { 0 } };
	struct battery_chg_dev *bcdev = container_of(nb, struct battery_chg_dev,
						     reboot_notifier);
	int rc;

	msg_notify.hdr.owner = MSG_OWNER_BC;
	msg_notify.hdr.type = MSG_TYPE_NOTIFY;
	msg_notify.hdr.opcode = BC_SHUTDOWN_NOTIFY;

	rc = battery_chg_write(bcdev, &msg_notify, sizeof(msg_notify));
	if (rc < 0)
		pr_err("Failed to send shutdown notification rc=%d\n", rc);

	if (!bcdev->ship_mode_en && !bcdev->shipmode_en)
		return NOTIFY_DONE;

	if ((code == SYS_POWER_OFF || code == SYS_RESTART) && !bcdev->ship_mode_immediate)
		battery_chg_ship_mode(bcdev);

	return NOTIFY_DONE;
}

static void panel_event_notifier_callback(enum panel_event_notifier_tag tag,
			struct panel_event_notification *notification, void *data)
{
	struct battery_chg_dev *bcdev = data;
	int rc;

	if (!notification) {
		pr_debug("Invalid panel notification\n");
		return;
	}

	pr_debug("panel event received, type: %d\n", notification->notif_type);
	switch (notification->notif_type) {
	case DRM_PANEL_EVENT_BLANK:
	case DRM_PANEL_EVENT_BLANK_LP:
		//battery_chg_notify_disable(bcdev);
		bcdev->screen_state = 0;
		break;
	case DRM_PANEL_EVENT_UNBLANK:
		//battery_chg_notify_enable(bcdev);
		bcdev->screen_state = 1;
		break;
	case DRM_PANEL_EVENT_FPS_CHANGE:
		return;
	default:
		pr_debug("Ignore panel event: %d\n", notification->notif_type);
		return;
	}

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM], XM_PROP_SCREEN_CCTOG, bcdev->screen_state);
	if (rc < 0) {
		pr_info("%s, screen_state notify fail\n", __func__);
	}
	pr_info("%s, screen_state = %d\n", __func__, bcdev->screen_state);
}

static int charge_check_panel(struct device_node *np)
{
	int i;
	int count;
	struct device_node *pnode;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0) {
		pr_err("%s ERROR: count is negative !", __func__);
		return 0;
	}

	for (i = 0; i < count; i++) {
		pnode = of_parse_phandle(np, "panel", i);
		if (!pnode) {
			pr_err("%s ERROR: Cannot fine panel of node!", __func__);
			return 0;
		}
		panel = of_drm_find_panel(pnode);
		of_node_put(pnode);
		if (!IS_ERR(panel)) {
			active_panel = panel;
			return 0;
		}else{
			active_panel = NULL;
		}
	}
	return PTR_ERR(panel);
}

static int battery_chg_register_panel_notifier(struct battery_chg_dev *bcdev)
{
	struct device_node *pnode;
	void *cookie = NULL;
	int rc;
	int error = 0;

	pnode = of_find_node_by_name(NULL, "charge-screen");
	if (!pnode) {
		pr_err("%s ERROR: Cannot find node with panel!", __func__);
		return 0;
	}

	error = charge_check_panel(pnode);
	if (error == -EPROBE_DEFER)
		pr_err("%s ERROR: Cannot fine panel of node!", __func__);


	if (!active_panel) {
		rc = PTR_ERR(panel);
		if (rc != -EPROBE_DEFER)
			dev_err(bcdev->dev, "Failed to find active panel, rc=%d\n", rc);
		return rc;
	}

	cookie = panel_event_notifier_register(
			PANEL_EVENT_NOTIFICATION_PRIMARY,
			PANEL_EVENT_NOTIFIER_CLIENT_BATTERY_CHARGER,
			active_panel,
			panel_event_notifier_callback,
			(void *)bcdev);
	if (IS_ERR(cookie)) {
		rc = PTR_ERR(cookie);
		dev_err(bcdev->dev, "Failed to register panel event notifier, rc=%d\n", rc);
		return rc;
	}

	pr_info("register panel notifier successful\n");
	bcdev->notifier_cookie = cookie;
	return 0;
}

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

static void get_pump_info(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev, xm_pump_info_work.work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_PUMP_INFO);
	if (rc < 0){
		schedule_delayed_work(&bcdev->xm_pump_info_work, 2000);
		pr_err("get_pump_info fail retry");
		return ;
	}
	pr_err("get_pump_info suc info =%d",pst->prop[XM_PROP_PUMP_INFO]);
	if(pst->prop[XM_PROP_PUMP_INFO])
		hardwareinfo_set_prop(HARDWARE_SUB_CHARGER_MASTER, "SC8531_PUMP");
	else
		hardwareinfo_set_prop(HARDWARE_SUB_CHARGER_MASTER, "BQ25960_PUMP");
}

static void generate_xm_charge_uvent(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev, xm_prop_change_work.work);

	pr_err("%s+++", __func__);

	kobject_uevent_env(&bcdev->dev->kobj, KOBJ_CHANGE, NULL);

	return;
}


#define MAX_UEVENT_LENGTH 50
static int add_xiaomi_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct battery_chg_dev *bcdev = platform_get_drvdata(pdev);

	char *prop_buf = NULL;
	char uevent_string[MAX_UEVENT_LENGTH+1];
	u32 i = 0;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return 0;

	/*add our prop start*/

	soc_decimal_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_SOC_DECIMAL=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	soc_decimal_rate_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_SOC_DECIMAL_RATE=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	shutdown_delay_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_SHUTDOWN_DELAY=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	quick_charge_type_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_QUICK_CHARGE_TYPE=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_MOISTURE_DET_STS=%d", bcdev->lpd_status);
	add_uevent_var(env, uevent_string);

	reverse_quick_charge_status_show( &(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_REVERSE_QUICK_CHARGE=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	cc_short_vbus_show(&(bcdev->battery_class), NULL, prop_buf);
	snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_CC_SHORT_VBUS=%s", prop_buf);
	add_uevent_var(env, uevent_string);

	// connector_temp_show( &(bcdev->battery_class), NULL, prop_buf);
	// snprintf(uevent_string, MAX_UEVENT_LENGTH, "POWER_SUPPLY_CONNECTOR_TEMP=%s", prop_buf);
	// add_uevent_var(env, uevent_string);

	/*add our prop end*/

	dev_info(bcdev->dev,"currnet uevent info :");
	for (i = 0; i < env->envp_idx; ++i) {
		dev_info(bcdev->dev," %s ", env->envp[i]);
	}

	free_page((unsigned long)prop_buf);
	return 0;
}

static struct device_type dev_type_xiaomi_uevent = {
	.name = "dev_type_xiaomi_uevent",
	.uevent = add_xiaomi_uevent,
};

void set_board_temp(int board_temp)
{
	int rc;

	if (!g_bcdev)
		return;

	pr_info("%s:%d\n", __func__, board_temp);
	rc = write_property_id(g_bcdev, &g_bcdev->psy_list[PSY_TYPE_XM], XM_PROP_THERMAL_BOARD_TEMP, board_temp);
	if (rc < 0)
		return;
}
EXPORT_SYMBOL_GPL(set_board_temp);

void set_thermal_scene(int thermal_scene)
{
	int rc;

	if (!g_bcdev)
		return;

	pr_info("%s:%d\n", __func__, thermal_scene);
	rc = write_property_id(g_bcdev, &g_bcdev->psy_list[PSY_TYPE_XM], XM_PROP_THERMAL_SCENE, thermal_scene);
	if (rc < 0)
		return;
}
EXPORT_SYMBOL_GPL(set_thermal_scene);

void battery_chg_get_batt_global(struct battery_chg_dev *bcdev)
{
	int ret,rc;
	char boardid[16]="";
	bool is_global = 0;

	memset(boardid, 0 , sizeof(boardid));
	ret = get_board_id_from_dtb(boardid);
	if (ret != 0) {
        	pr_err("device board id get failed.");
	}else if ((strlen(boardid) >= 6) && (strncmp(boardid, "S88939", 6) == 0))
	{
		is_global = 1;
        	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
			XM_PROP_BATT_IS_GLOBAL, is_global);
	}
	pr_err("device board id get success %s.",boardid);
}

static int battery_chg_probe(struct platform_device *pdev)
{
	struct battery_chg_dev *bcdev;
	struct device *dev = &pdev->dev;
	struct pmic_glink_client_data client_data = { };
	struct thermal_cooling_device *tcd;
	struct psy_state *pst;
	int rc, i;
	int is_mishow_kpoc;

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

	bcdev->digest = devm_kzalloc(&pdev->dev, BATTERY_DIGEST_LEN, GFP_KERNEL);
	if (!bcdev->digest)
		return -ENOMEM;

	bcdev->manufacturing_date = devm_kzalloc(&pdev->dev, BATTERY_DIGEST_LEN, GFP_KERNEL);
	if (!bcdev->manufacturing_date)
		return -ENOMEM;

	bcdev->soh_sn_data = devm_kzalloc(&pdev->dev, BATTERY_DIGEST_LEN, GFP_KERNEL);
	if (!bcdev->soh_sn_data)
		return -ENOMEM;

	bcdev->manuInfoC_data = devm_kzalloc(&pdev->dev, BATTERY_DIGEST_LEN, GFP_KERNEL);
	if (!bcdev->manuInfoC_data)
		return -ENOMEM;

	bcdev->ss_auth_data=
		devm_kzalloc(&pdev->dev, BATTERY_SS_AUTH_DATA_LEN * sizeof(u32), GFP_KERNEL);
	if (!bcdev->ss_auth_data)
		return -ENOMEM;
	
	charger_partition_init();
	mutex_init(&bcdev->rw_lock);
	init_rwsem(&bcdev->state_sem);
	init_completion(&bcdev->ack);
	init_completion(&bcdev->fw_buf_ack);
	init_completion(&bcdev->fw_update_ack);
	INIT_WORK(&bcdev->subsys_up_work, battery_chg_subsys_up_work);
	INIT_WORK(&bcdev->usb_type_work, battery_chg_update_usb_type_work);
	INIT_DELAYED_WORK(&bcdev->lpd_detection_work, battery_chg_lpd_detection_work);
	INIT_WORK(&bcdev->battery_check_work, battery_chg_check_status_work);
	INIT_DELAYED_WORK( &bcdev->xm_pump_info_work,get_pump_info);
	INIT_DELAYED_WORK( &bcdev->xm_prop_change_work, generate_xm_charge_uvent);
	INIT_DELAYED_WORK( &bcdev->dfx_monitor_work, battery_chg_dfx_monitor_work);
	INIT_DELAYED_WORK(&bcdev->mipps_check_work, battery_chg_mipps_check_work);
	INIT_DELAYED_WORK(&bcdev->standar_chg_slow_work, battery_chg_standar_chg_slow_work);
	INIT_DELAYED_WORK(&bcdev->update_poweroff_chg_icl_work, battery_chg_update_poweroff_icl_work);
	schedule_delayed_work(&bcdev->xm_pump_info_work, 5000);

	pr_err("get_pump_info start");
	bcdev->dev = dev;
	bcdev->lpd_status = 0;
	bcdev->lpd_charging = 0;
#ifndef CONFIG_FACTORY_BUILD
	bcdev->lpd_control = 1;
#else
	bcdev->lpd_control = 0;
#endif
	bcdev->batt_id = -1;
	bcdev->cp_index = -1;
	bcdev->shutdown_delay_en = true;
	bcdev->dfx_cycle_count_flag = 0;
	bcdev->dfx_batt_auth_flag = 0;
	bcdev->dfx_chip_ok_check_flag = 0;
	bcdev->dfx_lpd_check_flag = 0;
	bcdev->dfx_cp_ibus_ocp_flag = 0;
	bcdev->threshold_mah = 200000;
	bcdev->pd_verifed_dfx = 0;
	bcdev->mah_pre = 0;
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
	/*
	 * This should be initialized here so that battery_chg_callback
	 * can run successfully when battery_chg_parse_dt() starts
	 * reading BATT_CHG_CTRL_LIM_MAX parameter and waits for a response.
	 */
	bcdev->initialized = true;
	up_write(&bcdev->state_sem);

	g_bcdev = bcdev;
	eu_mode = charger_partition_get_eu_mode();
	is_mishow_kpoc = charger_partition_get_is_mishow_kpoc();
	if (g_bcdev) {
		rc = write_property_id(g_bcdev, &g_bcdev->psy_list[PSY_TYPE_XM], XM_PROP_EU_MODE, eu_mode);
		if (rc < 0) {
			pr_err("XM_PROP_EU_MODE rc = %d fail\n", rc);
		} else {
			pr_info("chargerPartition %s set XM_PROP_EU_MODE ok, eu_mode:%d\n", __func__, eu_mode);
		}

		rc = write_property_id(g_bcdev, &g_bcdev->psy_list[PSY_TYPE_XM], XM_PROP_IS_MISHOW_KPOC, is_mishow_kpoc);
		if (rc < 0) {
			pr_err("XM_PROP_IS_MISHOW_KPOC rc = %d fail\n", rc);
		} else {
			pr_info("[charger] %s set XM_PROP_IS_MISHOW_KPOC ok, is_mishow_kpoc:%d\n", __func__, is_mishow_kpoc);
		}
	}

	bcdev->reboot_notifier.notifier_call = battery_chg_reboot_notify;
	bcdev->reboot_notifier.priority = 255;
	register_reboot_notifier(&bcdev->reboot_notifier);

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
#ifndef CONFIG_FACTORY_BUILD
	bcdev->dfx_work_delay = DFX_DISCHARGE_WORK_TIME;
	schedule_delayed_work(&bcdev->dfx_monitor_work, msecs_to_jiffies(bcdev->dfx_work_delay));
#endif
	dev->type = &dev_type_xiaomi_uevent;

	rc = get_charge_control_en(bcdev);
	battery_chg_get_batt_global(bcdev);
	hardwareinfo_set_prop(HARDWARE_CHARGER_IC,"PM7250B_CHARGER");
	hardwareinfo_set_prop(HARDWARE_BMS_GAUGE,"MPC8011B_GAUGE");
	hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"BATTERY_DEFAULT");
	if (rc < 0)
		pr_debug("Failed to read charge_control_en, rc = %d\n", rc);

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
	cancel_delayed_work_sync(&bcdev->lpd_detection_work);
	complete(&bcdev->ack);
	unregister_reboot_notifier(&bcdev->reboot_notifier);
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

	charger_partition_exit();
	device_init_wakeup(bcdev->dev, false);
	debugfs_remove_recursive(bcdev->debugfs_dir);
	class_unregister(&bcdev->battery_class);
	pmic_glink_unregister_client(bcdev->client);
	cancel_work_sync(&bcdev->subsys_up_work);
	cancel_work_sync(&bcdev->usb_type_work);
	cancel_work_sync(&bcdev->battery_check_work);
	cancel_delayed_work_sync(&bcdev->lpd_detection_work);
	unregister_reboot_notifier(&bcdev->reboot_notifier);

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
MODULE_LICENSE("GPL");
