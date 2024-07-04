// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"BATTERY_CHG: %s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/extcon-provider.h>
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
#include <linux/soc/qcom/pmic_glink.h>
#include <linux/soc/qcom/battery_charger.h>
#include <linux/soc/qcom/panel_event_notifier.h>
#include "qti_typec_class.h"
/* N19 code for HQ-354491 by p-luozhibin1 at 2023.1.22 start */
#if IS_ENABLED(CONFIG_XIAOMI_USB_TOUCH_NOTIFIER)
#include <misc/xiaomi_usb_touch_notifier.h>
#endif
/* N19 code for HQ-354491 by p-luozhibin1 at 2023.1.22 end */

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
/* N19 code for HQ-351608 by p-gucheng at 2023/1/18 */
#define BC_SHUTDOWN_REQ_SET		0x37
#define BC_WLS_FW_CHECK_UPDATE		0x40
#define BC_WLS_FW_PUSH_BUF_REQ		0x41
#define BC_WLS_FW_UPDATE_STATUS_RESP	0x42
#define BC_WLS_FW_PUSH_BUF_RESP		0x43
#define BC_WLS_FW_GET_VERSION		0x44
#define BC_SHUTDOWN_NOTIFY		0x47
#define BC_HBOOST_VMAX_CLAMP_NOTIFY	0x79
#define BC_GENERIC_NOTIFY		0x80
/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - start*/
#define BC_XM_STATUS_GET		0x50
#define BC_XM_STATUS_SET		0x51
/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - end*/

/* Generic definitions */
#define MAX_STR_LEN			128
#define BC_WAIT_TIME_MS			1000
#define WLS_FW_PREPARE_TIME_MS		1000
#define WLS_FW_WAIT_TIME_MS		500
#define WLS_FW_UPDATE_TIME_MS		1000
#define WLS_FW_BUF_SIZE			128
#define DEFAULT_RESTRICT_FCC_UA		1000000

/* N19 code for HQ-354398 by p-gucheng at 2023/12/10 - start*/
#define USBPD_UVDM_SS_LEN	        4
#define BATTERY_SS_AUTH_DATA_LEN 	4

/* N19 code for HQ-355015&HQHW-6124 by p-yeyinzi at 2023/01/06 - start*/
#define PPS_THERMAL_LIMIT_MAX		6000000
#define QC_THERMAL_LIMIT_MAX		3600000
/* N19 code for HQ-355015&HQHW-6124 by p-yeyinzi at 2023/01/06 - end*/

#define BSWAP_32(x) \
	(u32)((((u32)(x) & 0xff000000) >> 24) | \
			(((u32)(x) & 0x00ff0000) >> 8) | \
			(((u32)(x) & 0x0000ff00) << 8) | \
			(((u32)(x) & 0x000000ff) << 24))
/* N19 code for HQ-354398 by p-gucheng at 2023/12/10 - end*/

/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - start*/
#define MONITOR_SOC_WAIT_MS	        1000
#define INIT_MONITOR_SOC_WAIT_MS	        10000
#define BATT_MA_AVG_SAMPLES		8
/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - end*/

enum usb_connector_type {
	USB_CONNECTOR_TYPE_TYPEC,
	USB_CONNECTOR_TYPE_MICRO_USB,
};

enum psy_type {
	PSY_TYPE_BATTERY,
	PSY_TYPE_USB,
	PSY_TYPE_WLS,
	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - start*/
	PSY_TYPE_XM,
	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - end*/
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
	WLS_PROP_MAX,
};

/* N19 code for HQ-354398 by p-gucheng at 2023/12/10 - start*/
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

/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/23 - start*/
enum xm_property_id {
	XM_PROP_REAL_TYPE,
	XM_PROP_CC_ORIENTATION,
	XM_PROP_INPUT_SUSPEND,
	XM_PROP_USB_OTG,
	XM_PROP_BAT_ID,
	XM_PROP_AUTHENTIC,
	XM_PROP_CHIP_OK,
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
	/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - end*/
	/* N19 code for HQ-363897 by p-yeyinzi at 2023/1/15 */
	XM_PROP_FAKE_TEMP,
	/* N19 code for HQ-351611 by p-tangsufeng at 2024/1/22 - start*/
	XM_PROP_SOC_DECIMAL,
	XM_PROP_SOC_DECIMAL_RATE,
	/* N19 code for HQ-351611 by p-tangsufeng at 2024/1/22 - end*/
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18 */
	XM_PROP_SHUTDOWN_DELAY,
	/* N19 code for HQ-357609&HQ-370651 by p-wumingzhu1 at 2024/02/04 start*/
	XM_PROP_SMART_CHG,
	XM_PROP_FAKE_CYCLE_COUNT,
	XM_PROP_SMART_BATT,
	/* N19 code for HQ-357609HQ-370651 by p-wumingzhu1 at 2024/02/04 end*/
	/* N19 code for HQHW-6475 by p-gucheng at 2024/03/09 */
	XM_PROP_CHARGE_DISABLED,
	/* N19 code for HQ-373306 by p-wuwencheng at 2024/3/15 - start*/
	XM_PROP_CHARGE_TYPE_MODE,
	/* N19 code for HQ-371056 by p-wuwencheng at 2024/3/19 - start*/
	XM_PROP_TYPEC_MODE,
	/* N19 code for HQ-378516 by p-gucheng at 2023/03/24 */
	XM_PROP_MTBF_MODE,
	XM_PROP_CP_ICL,
	XM_PROP_MAX,
};
/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - end*/

enum {
	/* N19 code for HQ-351613 by p-tangsufeng at 2024/1/8 - start*/
	QTI_POWER_SUPPLY_USB_TYPE_USB_FLOAT = 10,
	/* N19 code for HQ-351613 by p-tangsufeng at 2024/1/8 - end*/
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP = 0x80,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5,
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

/* N19 code for HQ-351608 by p-gucheng at 2023/1/18 - start */
struct battery_charger_shutdown_req_msg {
	struct pmic_glink_hdr	hdr;
};
/* N19 code for HQ-351608 by p-gucheng at 2023/1/18 - end */

struct psy_state {
	struct power_supply	*psy;
	char			*model;
	const int		*map;
	u32			*prop;
	u32			prop_count;
	u32			opcode_get;
	u32			opcode_set;
};

/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - start*/
struct xm_ss_auth_resp_msg {
        struct pmic_glink_hdr	hdr;
        u32			property_id;
        u32			data[BATTERY_SS_AUTH_DATA_LEN];
};
/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - end*/

struct battery_chg_dev {
	struct device			*dev;
	struct class			battery_class;
	struct pmic_glink_client	*client;
	struct typec_role_class		*typec_class;
	struct mutex			rw_lock;
	struct rw_semaphore		state_sem;
	struct completion		ack;
	struct completion		fw_buf_ack;
	struct completion		fw_update_ack;
	struct psy_state		psy_list[PSY_TYPE_MAX];
	struct dentry			*debugfs_dir;
	void				*notifier_cookie;
	/* extcon for VBUS/ID notification for USB for micro USB */
	struct extcon_dev		*extcon;
	u32				*thermal_levels;
	/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - start*/
	u32				*pps_thermal_levels;
	u32				*qc_thermal_levels;
	/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - end*/
	const char			*wls_fw_name;
	int				curr_thermal_level;
	int				num_thermal_levels;
	/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - start*/
	int				pps_num_thermal_levels;
	int				qc_num_thermal_levels;
	/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - end*/
	int				shutdown_volt_mv;
	/* N19 code for HQ-376015 by p-wumingzhu1 at 2024/03/13 */
	int				usb_vbus_type;
	atomic_t			state;
	struct work_struct		subsys_up_work;
	struct work_struct		usb_type_work;
	struct work_struct		battery_check_work;
	int				fake_soc;
	bool				block_tx;
	bool				ship_mode_en;
	bool				debug_battery_detected;
	bool				wls_not_supported;
	bool				wls_fw_update_reqd;
	u32				wls_fw_version;
	u16				wls_fw_crc;
	u32				wls_fw_update_time_ms;
	struct notifier_block		reboot_notifier;
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18 */
	struct notifier_block		shutdown_notifier;
	u32				thermal_fcc_ua;
	u32				restrict_fcc_ua;
	u32				last_fcc_ua;
	u32				usb_icl_ua;
	u32				thermal_fcc_step;
	u32				connector_type;
	u32				usb_prev_mode;
	bool				restrict_chg_en;
	/* To track the driver initialization status */
	bool				initialized;
	bool				notify_en;
	bool				error_prop;
	/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 */
	u32				*ss_auth_data;
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18 - start */
	bool				shutdown_delay_en;
	bool				report_power_absent;
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18 - end */
	/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 */
	struct delayed_work 		soc_monitor_work;
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

static const int wls_prop_map[WLS_PROP_MAX] = {
	[WLS_ONLINE]		= POWER_SUPPLY_PROP_ONLINE,
	[WLS_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[WLS_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[WLS_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[WLS_CURR_MAX]		= POWER_SUPPLY_PROP_CURRENT_MAX,
};

/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - start*/
static const int xm_prop_map[XM_PROP_MAX] = {
};
/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - end*/

static const unsigned int bcdev_usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

/* Standard usb_type definitions similar to power_supply_sysfs.c */
static const char * const power_supply_usb_type_text[] = {
	"Unknown", "SDP", "DCP", "CDP", "ACA", "C",
	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - start*/
	"PD", "PD_DRP", "PD_PPS", "DCP", "USB_FLOAT"
	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - end*/
};
/* N19 code for HQ-373306 by p-wuwencheng at 2024/3/15 - start*/
static const char * const power_supply_charge_type_text[] = {
	"UNKNOWN", "NORMAL", "FFC"
};
/* N19 code for HQ-373306 by p-wuwencheng at 2024/3/15 - end*/
/* Custom usb_type definitions */
static const char * const qc_power_supply_usb_type_text[] = {
	"HVDCP", "HVDCP_3", "HVDCP_3P5"
};
/* N19 code for HQ-371056 by p-wuwencheng at 2024/3/19 - start*/
static const char * const power_supply_usb_typec_mode_text[] = {
	"Nothing attached", "SOURCE_DEFAULT", "SOURCE_MEDIUM", "SOURCE_HIGH", "NON_COMPLIANT",
	"Sink attached", "SINK_POWERED_CABLE", "SINK_DEBUG_ACCESSORY", "Audio Adapter", "POWERED_CABLE_ONLY"
};
/* N19 code for HQ-371056 by p-wuwencheng at 2024/3/19 - end*/
/* N19 code for HQ-354491 by p-luozhibin1 at 2023.1.22 start */
#if IS_ENABLED(CONFIG_XIAOMI_USB_TOUCH_NOTIFIER)
int g_charger_flag = 0;
#endif
/* N19 code for HQ-354491 by p-luozhibin1 at 2023.1.22 end */

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

	reinit_completion(&bcdev->fw_buf_ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->fw_buf_ack,
					msecs_to_jiffies(WLS_FW_WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
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
		pr_debug("glink state is down\n");
		up_read(&bcdev->state_sem);
		return 0;
	}

	if (bcdev->debug_battery_detected && bcdev->block_tx) {
		up_read(&bcdev->state_sem);
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
			up_read(&bcdev->state_sem);
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

/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - start */
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

static int write_ss_auth_prop_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id, u32* buff)
{
	struct xm_ss_auth_resp_msg req_msg = { { 0 } };
	req_msg.property_id = prop_id;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;
	memcpy(req_msg.data, buff, BATTERY_SS_AUTH_DATA_LEN*sizeof(u32));
	pr_debug("psy: prop_id:%d size:%d data[0]:0x%x data[1]:0x%x data[2]:0x%x data[3]:0x%x\n",
		req_msg.property_id, sizeof(req_msg), req_msg.data[0], req_msg.data[1], req_msg.data[2], req_msg.data[3]);
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
	pr_debug("psy: %s prop_id: %u\n", pst->psy->desc->name,
		req_msg.property_id);
	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg));
}
/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - end */
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
EXPORT_SYMBOL(qti_battery_charger_set_prop);

static bool validate_message(struct battery_chg_dev *bcdev,
			struct battery_charger_resp_msg *resp_msg, size_t len)
{
	/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - start */	
        struct xm_ss_auth_resp_msg *ss_auth_req_msg = (struct xm_ss_auth_resp_msg *)resp_msg;
	if (len == sizeof(*ss_auth_req_msg)) {
                return true;
        }
	/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - end */

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
static void handle_message(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct battery_charger_resp_msg *resp_msg = data;
	struct battery_model_resp_msg *model_resp_msg = data;
	/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 */
	struct xm_ss_auth_resp_msg *ss_auth_req_msg = data;
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

	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - start*/
	case BC_XM_STATUS_GET:
		/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - start */
		if (len == sizeof(*ss_auth_req_msg) && bcdev->ss_auth_data) {
			memcpy(bcdev->ss_auth_data, ss_auth_req_msg->data, BATTERY_SS_AUTH_DATA_LEN*sizeof(u32));
			ack_set = true;
		break;
		}
		/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - end */

		pst = &bcdev->psy_list[PSY_TYPE_XM];
		if (validate_message(bcdev, resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - start*/
	case BC_BATTERY_STATUS_SET:
	case BC_USB_STATUS_SET:
	case BC_WLS_STATUS_SET:
	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - start*/
	case BC_XM_STATUS_SET:
	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - end*/
		if (validate_message(bcdev, data, len))
			ack_set = true;

		break;
	case BC_SET_NOTIFY_REQ:
	case BC_DISABLE_NOTIFY_REQ:
	case BC_SHUTDOWN_NOTIFY:
	case BC_SHIP_MODE_REQ_SET:
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18 */
	case BC_SHUTDOWN_REQ_SET:
		/* Always ACK response for notify or ship_mode request */
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

static void battery_chg_update_uusb_type(struct battery_chg_dev *bcdev,
					 u32 adap_type)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	/* Handle the extcon notification for uUSB case only */
	if (bcdev->connector_type != USB_CONNECTOR_TYPE_MICRO_USB)
		return;

	rc = read_property_id(bcdev, pst, USB_SCOPE);
	if (rc < 0) {
		pr_err("Failed to read USB_SCOPE rc=%d\n", rc);
		return;
	}

	switch (pst->prop[USB_SCOPE]) {
	case POWER_SUPPLY_SCOPE_DEVICE:
		if (adap_type == POWER_SUPPLY_USB_TYPE_SDP ||
		    adap_type == POWER_SUPPLY_USB_TYPE_CDP) {
			/* Device mode connect notification */
			extcon_set_state_sync(bcdev->extcon, EXTCON_USB, 1);
			bcdev->usb_prev_mode = EXTCON_USB;
			rc = qti_typec_partner_register(bcdev->typec_class,
							TYPEC_DEVICE);
			if (rc < 0)
				pr_err("Failed to register typec partner rc=%d\n",
					rc);
		}
		break;
	case POWER_SUPPLY_SCOPE_SYSTEM:
		/* Host mode connect notification */
		extcon_set_state_sync(bcdev->extcon, EXTCON_USB_HOST, 1);
		bcdev->usb_prev_mode = EXTCON_USB_HOST;
		rc = qti_typec_partner_register(bcdev->typec_class, TYPEC_HOST);
		if (rc < 0)
			pr_err("Failed to register typec partner rc=%d\n",
				rc);
		break;
	default:
		if (bcdev->usb_prev_mode == EXTCON_USB ||
		    bcdev->usb_prev_mode == EXTCON_USB_HOST) {
			/* Disconnect notification */
			extcon_set_state_sync(bcdev->extcon,
					      bcdev->usb_prev_mode, 0);
			bcdev->usb_prev_mode = EXTCON_NONE;
			qti_typec_partner_unregister(bcdev->typec_class);
		}
		break;
	}
}

static struct power_supply_desc usb_psy_desc;

static void battery_chg_update_usb_type_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, usb_type_work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
	if (rc < 0) {
		pr_err("Failed to read USB_ADAP_TYPE rc=%d\n", rc);
		return;
	}

	/* Reset usb_icl_ua whenever USB adapter type changes */
	if (pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_SDP &&
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

	battery_chg_update_uusb_type(bcdev, pst->prop[USB_ADAP_TYPE]);
}

static void battery_chg_check_status_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev,
					battery_check_work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
/* N19 code for HQ-354491 by p-luozhibin1 at 2023.1.22 start */
	struct psy_state *usb_pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc = 0;
/* N19 code for HQ-354491 by p-luozhibin1 at 2023.1.22 end */
	rc = read_property_id(bcdev, pst, BATT_STATUS);
	if (rc < 0) {
		pr_err("Failed to read BATT_STATUS, rc=%d\n", rc);
		return;
	}
/* N19 code for HQ-354491 by p-luozhibin1 at 2023.1.22 start */
 	rc = read_property_id(bcdev, usb_pst, USB_ONLINE);
	if (rc < 0) {
		pr_err("Failed to read USB_ONLINE, rc=%d\n", rc);
		return;
	}
/* N19 code for HQ-354491 by p-luozhibin1 at 2023.1.22 end */
	if (pst->prop[BATT_STATUS] == POWER_SUPPLY_STATUS_CHARGING) {
		pr_debug("Battery is charging\n");
		return;
	}

	rc = read_property_id(bcdev, pst, BATT_CAPACITY);
	if (rc < 0) {
		pr_err("Failed to read BATT_CAPACITY, rc=%d\n", rc);
		return;
	}

	if (DIV_ROUND_CLOSEST(pst->prop[BATT_CAPACITY], 100) > 0) {
		pr_debug("Battery SOC is > 0\n");
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

	if (pst->prop[BATT_VOLT_NOW] / 1000 > bcdev->shutdown_volt_mv) {
		/* N19 code for HQ-351608 by p-gucheng at 2023/1/18 */
		pr_info("Battery voltage is > %d mV\n",
			bcdev->shutdown_volt_mv);
		return;
	}

	pr_emerg("Initiating a shutdown in 100 ms\n");
	msleep(100);
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18 */
	bcdev->report_power_absent = true;
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
/* N19 code for HQ-354491 by p-luozhibin1 at 2023.1.22 start */
		schedule_work(&bcdev->battery_check_work);
/* N19 code for HQ-354491 by p-luozhibin1 at 2023.1.22 end */
		break;
	case BC_USB_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		schedule_work(&bcdev->usb_type_work);
		break;
	case BC_WLS_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_WLS];
		break;
	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - start*/
	case BC_XM_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_XM];
		break;
	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - end*/
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
			 bcdev->state);
		up_read(&bcdev->state_sem);
		return 0;
	}

	if (hdr->opcode == BC_NOTIFY_IND)
		handle_notification(bcdev, data, len);
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

static const char *get_usb_type_name(u32 usb_type)
{
	u32 i;

	if (usb_type >= QTI_POWER_SUPPLY_USB_TYPE_HVDCP &&
	    usb_type <= QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5) {
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

/* N19 code for HQ-351613 by p-tangsufeng at 2024/1/8 - start*/
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
	{ POWER_SUPPLY_USB_TYPE_SDP,        QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_DCP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_CDP,    QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_ACA,    QUICK_CHARGE_NORMAL },
	{ QTI_POWER_SUPPLY_USB_TYPE_USB_FLOAT,  QUICK_CHARGE_NORMAL },
	{ POWER_SUPPLY_USB_TYPE_PD,       QUICK_CHARGE_FAST },
	{ POWER_SUPPLY_USB_TYPE_PD_PPS,       QUICK_CHARGE_TURBE },
	{ QTI_POWER_SUPPLY_USB_TYPE_HVDCP,    QUICK_CHARGE_FAST },
	{ QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3,  QUICK_CHARGE_FAST },
	/* N19 code for HQ-371347 by p-yeyinzi at 2024/3/19 */
	{ QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5,  QUICK_CHARGE_FAST },
	{0, 0},
};

static int battery_psy_set_charge_current(struct battery_chg_dev *bcdev, int val);
static int quick_charge_type(struct battery_chg_dev *bcdev)
{
	struct psy_state *pst_xm = &bcdev->psy_list[PSY_TYPE_XM];
	struct psy_state *pst_bat = &bcdev->psy_list[PSY_TYPE_BATTERY];
	struct psy_state *pst_usb = &bcdev->psy_list[PSY_TYPE_USB];
	enum power_supply_usb_type real_charger_type = 0;
	int i = 0, usb_present = 0, batt_health = POWER_SUPPLY_HEALTH_GOOD;
	u8 result = QUICK_CHARGE_NORMAL;
	int rc = 0;

	batt_health = pst_bat->prop[BATT_HEALTH];
	if ((batt_health == POWER_SUPPLY_HEALTH_COLD) || (batt_health == POWER_SUPPLY_HEALTH_WARM)
		|| (batt_health == POWER_SUPPLY_HEALTH_OVERHEAT) || (batt_health == POWER_SUPPLY_HEALTH_OVERVOLTAGE))
		return result;
	usb_present = pst_usb->prop[USB_ONLINE];
	if (usb_present) {
		rc = read_property_id(bcdev, pst_xm, XM_PROP_REAL_TYPE);
		if (rc < 0)
			return rc;
		real_charger_type = pst_xm->prop[XM_PROP_REAL_TYPE];
		while (adapter_cap[i].adap_type != 0) {
			if (real_charger_type == adapter_cap[i].adap_type) {
				result = adapter_cap[i].adap_cap;
			}
			i++;
		}
		/* N19 code for HQ-376015 by p-wumingzhu1 at 2024/03/13 start*/
		if(bcdev->usb_vbus_type != pst_xm->prop[XM_PROP_REAL_TYPE]) {
			battery_psy_set_charge_current(bcdev, bcdev->curr_thermal_level);
			pr_info("real_type happen swap: thermal_level=%d", bcdev->curr_thermal_level);
		}
		/* N19 code for HQ-376015 by p-wumingzhu1 at 2024/03/13 end*/
	}
	/* N19 code for HQ-376015 by p-wumingzhu1 at 2024/03/13 start*/
	else
		bcdev->usb_vbus_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	pr_info("%s quick_type:%d, vbus_type=%d\n", __func__, result, bcdev->usb_vbus_type);
	/* N19 code for HQ-376015 by p-wumingzhu1 at 2024/03/13 end*/

	return result;
}

static ssize_t quick_charge_type_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	
	return scnprintf(buf, PAGE_SIZE, "%u", quick_charge_type(bcdev));
}
static CLASS_ATTR_RO(quick_charge_type);
/* N19 code for HQ-351613 by p-tangsufeng at 2024/1/8 - end*/

/* N19 code for HQ-351611 by p-tangsufeng at 2024/1/22 - start*/
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
/* N19 code for HQ-351611 by p-tangsufeng at 2024/1/22 - end*/

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

	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	/* N19 code for HQ-367753 by p-gucheng at 2024/01/22 - start */
	switch(prop) {
		case POWER_SUPPLY_PROP_ONLINE:
			rc = read_property_id(bcdev, pst, USB_REAL_TYPE);
			if (rc < 0)
				return rc;
			if ((pst->prop[USB_REAL_TYPE] == POWER_SUPPLY_USB_TYPE_UNKNOWN) &&
				(1 == pst->prop[prop_id])) {
					pval->intval = 0;
					pr_info("charge type not ready, report online = 0\n");
			}
			else
				pval->intval = pst->prop[prop_id];
		break;
		default:
			pval->intval = pst->prop[prop_id];
		break;
	}
	/* N19 code for HQ-367753 by p-gucheng at 2024/01/22 - end */

	if (prop == POWER_SUPPLY_PROP_TEMP)
		pval->intval = DIV_ROUND_CLOSEST((int)pval->intval, 10);

	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  - start */
	if (prop == POWER_SUPPLY_PROP_ONLINE) {
		if (pval->intval == true  && bcdev->report_power_absent)
			pval->intval = false;
	}
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18 - end */

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
	POWER_SUPPLY_PROP_SCOPE,
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
		pr_info("Set FCC to %u uA\n", fcc_ua);
		bcdev->last_fcc_ua = fcc_ua;
	}

	return rc;
}

static int battery_psy_set_charge_current(struct battery_chg_dev *bcdev,
					int val)
{
	int rc;
	u32 fcc_ua, prev_fcc_ua;

	/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - start*/
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	rc = read_property_id(bcdev, pst, XM_PROP_REAL_TYPE);
	/* N19 code for HQ-376015 by p-wumingzhu1 at 2024/03/13 */
	bcdev->usb_vbus_type = pst->prop[XM_PROP_REAL_TYPE];
	if (rc < 0){
		pr_err("Failed to get real_type rc=%d\n", rc);
	} else {
		if(pst->prop[XM_PROP_REAL_TYPE] == POWER_SUPPLY_USB_TYPE_PD_PPS){
			bcdev->num_thermal_levels = bcdev->pps_num_thermal_levels;
			bcdev->thermal_levels = bcdev->pps_thermal_levels;
		}
		else{
			bcdev->num_thermal_levels = bcdev->qc_num_thermal_levels;
			bcdev->thermal_levels = bcdev->qc_thermal_levels;
		}
	}
	/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - end*/

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

	prev_fcc_ua = bcdev->thermal_fcc_ua;
	bcdev->thermal_fcc_ua = fcc_ua;

	rc = __battery_psy_set_charge_current(bcdev, fcc_ua);
	if (!rc)
		bcdev->curr_thermal_level = val;
	else
		bcdev->thermal_fcc_ua = prev_fcc_ua;

	return rc;
}

/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - start*/
static struct timespec64 get_current_time(void)
{
	struct timespec64 now_time;
	ktime_t time;

	time = ktime_get_boottime();
	now_time = ktime_to_timespec64(time);

	return now_time;
}

static int calculate_delta_time(struct timespec64 *time_stamp, int *delta_time_s,struct timespec64 *now_time)
{
	/* default to delta time = 0 if anything fails */
	*delta_time_s = 0;
	*now_time = get_current_time();
	*delta_time_s = (now_time->tv_sec - time_stamp->tv_sec);

	return 0;
}

static void calculate_average_current(int batt_current, int *batt_ma_avg )
{
	static int samples_index = 0, samples_num = 0;
	static int batt_ma_avg_samples[BATT_MA_AVG_SAMPLES];
	static int batt_ma_prev = 0;
	static int last_batt_ma_avg = 0;
	int sum_ma = 0;
	int i;

	if(batt_current == batt_ma_prev)
		goto unchanged;
	else
		batt_ma_prev = batt_current;

	batt_ma_avg_samples[samples_index] = batt_current;
	samples_index = (samples_index + 1) % BATT_MA_AVG_SAMPLES;
	samples_num += 1;

	if(samples_num >= BATT_MA_AVG_SAMPLES)
		samples_num = BATT_MA_AVG_SAMPLES;

	for( i = 0; i <  samples_num; i++){
		sum_ma += batt_ma_avg_samples[i];
	}

	last_batt_ma_avg = sum_ma / samples_num;

unchanged:
	*batt_ma_avg = last_batt_ma_avg;
}

static void xm_uevent_report_shutdown_delay(struct battery_chg_dev *bcdev, bool shutdown_delay);
static int fg_battery_soc_smooth_tracking(struct battery_chg_dev *bcdev,  int  Monotonicsoc )
{
	static int OptimalSoc = -2;
	static int last_OptimalSoc;
	static struct timespec64 last_change_time = {0,0};
	struct timespec64 now_time;
	int batt_temp;
	int batt_current;
	int batt_ma_avg;
	int time_since_last_change_sec;
	int delta_time = 0;
	int soc_changed = 0;
	int status;
	unsigned char shutdown_delay = 0;
	static bool report_flag = 0;

	struct psy_state *pst_xm= &bcdev->psy_list[PSY_TYPE_XM];
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	/* get battery temp */
	batt_temp = DIV_ROUND_CLOSEST((int)pst->prop[BATT_TEMP], 10);

	/* get battery current */
	batt_current = pst->prop[BATT_CURR_NOW];

	/*get charging status */
	status = pst->prop[BATT_STATUS];

	/*get shutdown delay status */
	if (!bcdev->shutdown_delay_en)
		pst_xm->prop[XM_PROP_SHUTDOWN_DELAY] = 0;
	else {
		if (Monotonicsoc < 5) {
			read_property_id(bcdev, pst_xm, XM_PROP_SHUTDOWN_DELAY);
			shutdown_delay = pst_xm->prop[XM_PROP_SHUTDOWN_DELAY];
		}
	}

	/* initial variable */
	if (last_change_time.tv_sec == 0)
		last_change_time = get_current_time();

	if (OptimalSoc == -2) {
		OptimalSoc = Monotonicsoc;
		last_OptimalSoc = OptimalSoc;
	}

	/* smooth algorithm */
	calculate_delta_time(&last_change_time, &time_since_last_change_sec, &now_time);
	calculate_average_current(batt_current, &batt_ma_avg);

	if (batt_temp > 150) {
	/*  Battery in normal temperature */
		if (batt_ma_avg < -1000000)
		/* Heavy loading current, ignore battery soc limit*/
			delta_time = time_since_last_change_sec / 10;
		else if (batt_current > 0 || abs (OptimalSoc - Monotonicsoc) > 2)
			delta_time = time_since_last_change_sec / 20;
		else
			delta_time = time_since_last_change_sec / 60;
	} else if(batt_temp < -100) {
		if (batt_ma_avg < -1000000 ||  abs (OptimalSoc - Monotonicsoc) > 2)
			delta_time = time_since_last_change_sec / 5;
		else
			delta_time = time_since_last_change_sec / 10;
	} else {
		/* Calculated average current > 1000mA */
		if (batt_ma_avg < -1000000 ||  abs (OptimalSoc - Monotonicsoc) > 2)
			delta_time = time_since_last_change_sec / 10;
		else
			delta_time = time_since_last_change_sec / 20;
	}

	if (shutdown_delay == 2) {
		pr_info("vbat is too low, poweroff");
		kernel_power_off();
	} else if(shutdown_delay == 1){
		if ((Monotonicsoc == 1) && (OptimalSoc != 1)) {
			delta_time = time_since_last_change_sec / 5;
		} else if ((OptimalSoc == 1) && (!report_flag))  {
			xm_uevent_report_shutdown_delay(bcdev, true);
			report_flag = true;
		}
	} else if(shutdown_delay == 0) {
		if (report_flag) {
			xm_uevent_report_shutdown_delay(bcdev, false);
			report_flag = false;
		}
	}

	if (delta_time < 0)
		delta_time = 0;

	soc_changed = min(1, delta_time);

	pr_info("fg_battery_soc_smooth_tracking::batt_ma_avg=%d, batt_temp=%d, batt_current=%d, delta_time = %d, soc_changed = %d",
			batt_ma_avg, batt_temp, batt_current, delta_time, soc_changed);

	if (OptimalSoc >= 0) {
		if (OptimalSoc < 100 && Monotonicsoc == 100 && status == POWER_SUPPLY_STATUS_FULL)
			OptimalSoc = OptimalSoc + soc_changed;
		/* N19 code for HQHW-7063 by p-tangsufeng at 2024/04/10 - start */
		else if (OptimalSoc ==100 && Monotonicsoc >= 97 && status == POWER_SUPPLY_STATUS_FULL)
			/* keep 100 */
			OptimalSoc = 100;
		/* N19 code for HQHW-7063 by p-tangsufeng at 2024/04/10 - end */
		else if (OptimalSoc < Monotonicsoc && batt_current > 0)
			/* Battery in charging status
			* update the soc when resuming device
			*/
			OptimalSoc = OptimalSoc + soc_changed;
		else if (OptimalSoc > Monotonicsoc && batt_current < 0) {
			/* Battery in discharging status
			* update the soc when resuming device
			*/
			OptimalSoc = OptimalSoc - soc_changed;
			if(OptimalSoc <= 0)
				OptimalSoc = 0;
		}
	} else {
		OptimalSoc = 0;
	}

	if(last_OptimalSoc != OptimalSoc ){
		last_change_time = now_time;
		last_OptimalSoc = OptimalSoc;
		if(pst->psy)
			power_supply_changed(pst->psy);
	}

	return OptimalSoc;
}
/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - end*/

static int battery_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int prop_id, rc;
	/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 */
	int msoc;

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
		/* N19 code for HQHW-7063 by p-tangsufeng at 2024/04/10 - start */
		msoc = (pst->prop[prop_id] + 99) / 98;
		if (msoc > 100)
			msoc = 100;
		if (pst->prop[prop_id] == 100)
			msoc = 1;
		pval->intval = fg_battery_soc_smooth_tracking(bcdev, msoc);
		/* N19 code for HQHW-7063 by p-tangsufeng at 2024/04/10 - end */

		if (IS_ENABLED(CONFIG_QTI_PMIC_GLINK_CLIENT_DEBUG) &&
		   (bcdev->fake_soc >= 0 && bcdev->fake_soc <= 100))
			pval->intval = bcdev->fake_soc;
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
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  - start */
	case POWER_SUPPLY_PROP_STATUS:
		pval->intval = pst->prop[prop_id];
		if (pval->intval == POWER_SUPPLY_STATUS_CHARGING && bcdev->report_power_absent)
			pval->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  - end */
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
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
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

	if (bcdev->wls_not_supported) {
		pr_debug("Wireless charging is not supported\n");
	} else {
		bcdev->psy_list[PSY_TYPE_WLS].psy =
			devm_power_supply_register(bcdev->dev, &wls_psy_desc, &psy_cfg);

		if (IS_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy)) {
			rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy);
			bcdev->psy_list[PSY_TYPE_WLS].psy = NULL;
			pr_err("Failed to register wireless power supply, rc=%d\n", rc);
			return rc;
		}
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
		pr_err("Failed to get FW version rc=%d\n", rc);
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
	pr_debug("Set fake soc to %d\n", val);

	if (IS_ENABLED(CONFIG_QTI_PMIC_GLINK_CLIENT_DEBUG) && pst->psy)
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

static ssize_t flash_active_show(struct class *c,
					struct class_attribute *attr, char *buf)
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

static ssize_t shipmode_count_reset_store(struct class *c, struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	if (kstrtobool(buf, &bcdev->ship_mode_en))
		return -EINVAL;

	return count;
}

static ssize_t shipmode_count_reset_show(struct class *c, struct class_attribute *attr,
				char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->ship_mode_en);
}
static CLASS_ATTR_RW(shipmode_count_reset);

/* N19 code for HQ-354969 by p-yeyinzi at 2023/12/23 - start*/
/* N19 code for HQ-371347 by p-tangsufeng at 2024/2/19 - start*/
static void xm_uevent_report(struct battery_chg_dev *bcdev);
static ssize_t real_type_show(struct class *c, struct class_attribute *attr,
			char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_REAL_TYPE);
	if (rc < 0)
		return rc;

	/* N19 code for HQ-371347 by p-tangsufeng at 2024/2/19 - start*/
	xm_uevent_report(bcdev);

	if(pst->prop[XM_PROP_REAL_TYPE] == QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3 ||
		pst->prop[XM_PROP_REAL_TYPE] == QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5)
		pst->prop[XM_PROP_REAL_TYPE] = QTI_POWER_SUPPLY_USB_TYPE_HVDCP;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			get_usb_type_name(pst->prop[XM_PROP_REAL_TYPE]));
}
static CLASS_ATTR_RO(real_type);

static ssize_t typec_cc_orientation_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_CC_ORIENTATION);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CC_ORIENTATION]);
}
static CLASS_ATTR_RO(typec_cc_orientation);

static ssize_t input_suspend_store(struct class *c,
                                       struct class_attribute *attr,
                                       const char *buf, size_t count)
{
       struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                               battery_class);
       int rc;
       bool val;
       if (kstrtobool(buf, &val))
               return -EINVAL;
       pr_err("set charger input suspend %d\n", val);
       rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
                               XM_PROP_INPUT_SUSPEND, val);
       if (rc < 0)
               return rc;
       return count;
}

static ssize_t input_suspend_show(struct class *c,
                                       struct class_attribute *attr, char *buf)
{
       struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                               battery_class);
       struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
       int rc;
       rc = read_property_id(bcdev, pst, XM_PROP_INPUT_SUSPEND);
       if (rc < 0)
               return rc;
       return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_INPUT_SUSPEND]);
}
static CLASS_ATTR_RW(input_suspend);

static ssize_t usb_otg_show(struct class *c,
                                       struct class_attribute *attr, char *buf)
{
       struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                               battery_class);
       struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
       int rc;
       rc = read_property_id(bcdev, pst, XM_PROP_USB_OTG);
       if (rc < 0)
               return rc;
       return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_USB_OTG]);
}
static CLASS_ATTR_RO(usb_otg);

static ssize_t bat_id_show(struct class *c,
                                       struct class_attribute *attr, char *buf)
{
       struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                               battery_class);
       struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
       int rc;
       rc = read_property_id(bcdev, pst, XM_PROP_BAT_ID);
       if (rc < 0)
               return rc;
       return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_BAT_ID]);
}
static CLASS_ATTR_RO(bat_id);

static ssize_t authentic_show(struct class *c,
                                       struct class_attribute *attr, char *buf)
{
       struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                               battery_class);
       struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
       int rc;
       rc = read_property_id(bcdev, pst, XM_PROP_AUTHENTIC);
       if (rc < 0)
               return rc;
       return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_AUTHENTIC]);
}
static CLASS_ATTR_RO(authentic);

static ssize_t chip_ok_show(struct class *c,
                                       struct class_attribute *attr, char *buf)
{
       struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
                                               battery_class);
       struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
       int rc;
       rc = read_property_id(bcdev, pst, XM_PROP_CHIP_OK);
       if (rc < 0)
               return rc;
       return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CHIP_OK]);
}
static CLASS_ATTR_RO(chip_ok);
/* N19 code for HQ-354969 by p-yeyinzi at 2023/12/23 - end*/

/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - start */
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
	pr_err("usbpd_request_vdm_cmd:cmd = %d, data = %d\n", cmd, *data);
	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		/* N19 code for HQHW-6429 by p-xuyechen at 2024.3.11 */
		msleep(200);
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
	pr_info("%s:buf:%s cmd:%d, buffer:%s\n", __func__, buf, cmd, buffer);
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
	pr_info("request_vdm_cmd_show  uvdm_state: %d\n", cmd);
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
		pr_info("request_vdm_cmd_show  uvdm_state: %d, read_ss_auth\n", cmd);
		rc = read_ss_auth_property_id(bcdev, pst, prop_id);
		if (rc < 0) {
			pr_info("request_vdm_cmd_show  uvdm_state: %d, Error ret = %d\n", cmd, rc);
			return rc;
		}
		pr_info("request_vdm_cmd_show auth:0x%x 0x%x 0x%x 0x%x\n",
			bcdev->ss_auth_data[0],bcdev->ss_auth_data[1],bcdev->ss_auth_data[2],bcdev->ss_auth_data[3]);
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			memset(data, 0, sizeof(data));
			snprintf(data, sizeof(data), "%08lx", bcdev->ss_auth_data[i]);
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
		return snprintf(buf, PAGE_SIZE, "%s %d", usbpd_state_strings[0], pst->prop[XM_PROP_CURRENT_STATE]);
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

/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - end */

/* N19 code for HQ-363897 by p-yeyinzi at 2023/1/15 - start*/
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
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FAKE_TEMP]);
}
static CLASS_ATTR_RW(fake_temp);
/* N19 code for HQ-363897 by p-yeyinzi at 2023/1/15 - end*/

/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  - start */
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

	if (!bcdev->shutdown_delay_en)
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
	pr_err("use contral shutdown delay featue enable= %d\n", bcdev->shutdown_delay_en);

	return count;
}
static CLASS_ATTR_RW(shutdown_delay);

static ssize_t smart_chg_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	int val;
	if (kstrtoint(buf, 0, &val))
		return -EINVAL;
	pr_err("set smart charging engine, %d\n", val);
	val = 0;
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

static ssize_t fake_cycle_count_store(struct class *c,
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
				XM_PROP_FAKE_CYCLE_COUNT, val);
	if (rc < 0)
		return rc;
	return count;
}
static ssize_t fake_cycle_count_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;
	rc = read_property_id(bcdev, pst, XM_PROP_FAKE_CYCLE_COUNT);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_FAKE_CYCLE_COUNT]);
}
static CLASS_ATTR_RW(fake_cycle_count);

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

	pr_err("set smart batt charging %d\n", val);

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
/* N19 code for HQ-357609&HQ-370651 by p-wumingzhu1 at 2024/02/04 end*/

/* N19 code for HQHW-6475 by p-gucheng at 2024/03/09 - start */
static ssize_t charge_disabled_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CHARGE_DISABLED);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pst->prop[XM_PROP_CHARGE_DISABLED]);
}
static CLASS_ATTR_RO(charge_disabled);
/* N19 code for HQHW-6475 by p-gucheng at 2024/03/09 - end */
/* N19 code for HQ-373306 by p-wuwencheng at 2024/3/15 - start*/
static ssize_t charge_type_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CHARGE_TYPE_MODE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%s\n", power_supply_charge_type_text[pst->prop[XM_PROP_CHARGE_TYPE_MODE]]);
}
static CLASS_ATTR_RO(charge_type_mode);
/* N19 code for HQ-373306 by p-wuwencheng at 2024/3/15 - end*/
/* N19 code for HQ-371056 by p-wuwencheng at 2024/3/19 - start*/
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
	return scnprintf(buf, PAGE_SIZE, "%s\n", power_supply_usb_typec_mode_text[pst->prop[XM_PROP_TYPEC_MODE]]);
}
static CLASS_ATTR_RO(typec_mode);
/* N19 code for HQ-371056 by p-wuwencheng at 2024/3/19 - end*/

/* N19 code for HQ-378516 by p-gucheng at 2024/04/12 start */
static ssize_t mtbf_mode_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc = 0;
	int val = 0;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	pr_info("set mtbf mode %d\n", val);

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_XM],
				XM_PROP_MTBF_MODE, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t mtbf_mode_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_MTBF_MODE);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_MTBF_MODE]);
}
static CLASS_ATTR_RW(mtbf_mode);
/* N19 code for HQ-378516 by p-gucheng at 2024/04/12 end */

static ssize_t cp_icl_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int rc;

	rc = read_property_id(bcdev, pst, XM_PROP_CP_ICL);
	if (rc < 0)
		return rc;
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[XM_PROP_CP_ICL]);
}
static CLASS_ATTR_RO(cp_icl);

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
	&class_attr_shipmode_count_reset.attr,
	&class_attr_restrict_chg.attr,
	&class_attr_restrict_cur.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_usb_typec_compliant.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class);

static struct attribute *battery_class_no_wls_attrs[] = {
	/* N19 code for HQ-351611 by p-tangsufeng at 2024/1/22 - start*/
	&class_attr_soc_decimal.attr,
	&class_attr_soc_decimal_rate.attr,
	/* N19 code for HQ-351611 by p-tangsufeng at 2024/1/22 - end*/
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_flash_active.attr,
	&class_attr_moisture_detection_status.attr,
	&class_attr_moisture_detection_en.attr,
	&class_attr_fake_soc.attr,
	&class_attr_shipmode_count_reset.attr,
	&class_attr_restrict_chg.attr,
	&class_attr_restrict_cur.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_usb_typec_compliant.attr,
/* N19 code for HQ-354969 by p-yeyinzi at 2023/12/23 - start*/
	&class_attr_real_type.attr,
	&class_attr_typec_cc_orientation.attr,
	&class_attr_input_suspend.attr,
	&class_attr_usb_otg.attr,
	&class_attr_bat_id.attr,
	&class_attr_authentic.attr,
	&class_attr_chip_ok.attr,
/* N19 code for HQ-354969 by p-yeyinzi at 2023/12/23 - end*/
/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - start */
	&class_attr_verify_process.attr,
	&class_attr_request_vdm_cmd.attr,
	&class_attr_current_state.attr,
	&class_attr_adapter_id.attr,
	&class_attr_adapter_svid.attr,
	&class_attr_pd_verifed.attr,
	&class_attr_pdo2.attr,
/* N19 code for HQ-354398 by p-gucheng at 2023/12/14 - end */
/* N19 code for HQ-351613 by p-tangsufeng at 2024/1/8 */
	&class_attr_quick_charge_type.attr,
/* N19 code for HQ-363897 by p-yeyinzi at 2023/1/15 */
	&class_attr_fake_temp.attr,
/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  */
	&class_attr_shutdown_delay.attr,
/* N19 code for HQ-357609&HQ-370651 by p-wumingzhu1 at 2024/02/04 start*/
	&class_attr_smart_chg.attr,
	&class_attr_fake_cycle_count.attr,
	&class_attr_smart_batt.attr,
/* N19 code for HQ-357609&HQ-370651 by p-wumingzhu1 at 2024/02/04 end*/
/* N19 code for HQHW-6475 by p-gucheng at 2024/03/09 */
	&class_attr_charge_disabled.attr,
/* N19 code for HQ-373306 by p-wuwencheng at 2024/3/15 */
	&class_attr_charge_type_mode.attr,
/* N19 code for HQ-371056 by p-wuwencheng at 2024/3/19 */
	&class_attr_typec_mode.attr,
/* N19 code for HQ-378516 by p-gucheng at 2024/04/12 */
	&class_attr_mtbf_mode.attr,
	&class_attr_cp_icl.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class_no_wls);

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
	/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - start*/
	int i, pps_len, qc_len, len, rc;
	u32 pps_prev, qc_prev, val;
	/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - end*/

	bcdev->wls_not_supported = of_property_read_bool(node,
			"qcom,wireless-charging-not-supported");

	of_property_read_string(node, "qcom,wireless-fw-name",
				&bcdev->wls_fw_name);

	of_property_read_u32(node, "qcom,shutdown-voltage",
				&bcdev->shutdown_volt_mv);

	/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - start*/
	pps_len = of_property_count_elems_of_size(node, "qcom,pps-thermal-mitigation",
						sizeof(u32));
	qc_len = of_property_count_elems_of_size(node, "qcom,qc-thermal-mitigation",
						sizeof(u32));

	if ((pps_len <= 0) && (qc_len <= 0)) {
	/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - end*/

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
		/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - start*/
		bcdev->thermal_fcc_step = 0;
		pps_prev = PPS_THERMAL_LIMIT_MAX;
		qc_prev = QC_THERMAL_LIMIT_MAX;

		for (i = 0; i < pps_len; i++) {
			rc = of_property_read_u32_index(node, "qcom,pps-thermal-mitigation",
				i, &val);
			if (rc < 0)
				return rc;

			if (val > pps_prev) {
				pr_err("Thermal levels should be in descending order\n");
			}

			pps_prev = val;
		}

		for (i = 0; i < qc_len; i++) {
			rc = of_property_read_u32_index(node, "qcom,qc-thermal-mitigation",
				i, &val);
			if (rc < 0)
				return rc;

			if (val > qc_prev) {
				pr_err("Thermal levels should be in descending order\n");
			}

			qc_prev = val;
		}

		bcdev->pps_thermal_levels = devm_kcalloc(bcdev->dev, pps_len,
						sizeof(*bcdev->pps_thermal_levels),
						GFP_KERNEL);
		if (!bcdev->pps_thermal_levels)
			return -ENOMEM;

		bcdev->qc_thermal_levels = devm_kcalloc(bcdev->dev, qc_len,
						sizeof(*bcdev->qc_thermal_levels),
						GFP_KERNEL);
		if (!bcdev->qc_thermal_levels)
			return -ENOMEM;

		/*
		 * Element 0 is for normal charging current. Elements from index 1
		 * onwards is for thermal mitigation charging currents.
		 */

		rc = of_property_read_u32_array(node, "qcom,pps-thermal-mitigation",
					&bcdev->pps_thermal_levels[0], pps_len);
		if (rc < 0) {
			pr_err("Error in reading qcom,pps-thermal-mitigation, rc=%d\n", rc);
			return rc;
		}
		for (i = 0; i < pps_len; i++) {
			pr_info("bcdev->pps_thermal_levels[%d]=%d\n", i, bcdev->pps_thermal_levels[i]);
		}

		rc = of_property_read_u32_array(node, "qcom,qc-thermal-mitigation",
					&bcdev->qc_thermal_levels[0], qc_len);
		if (rc < 0) {
			pr_err("Error in reading qcom,qc-thermal-mitigation, rc=%d\n", rc);
			return rc;
		}
		for (i = 0; i < qc_len; i++) {
			pr_info("bcdev->qc_thermal_levels[%d]=%d\n", i, bcdev->qc_thermal_levels[i]);
		}
	}

	bcdev->pps_num_thermal_levels = pps_len - 1;
	bcdev->qc_num_thermal_levels = qc_len - 1;
	bcdev->thermal_fcc_ua = QC_THERMAL_LIMIT_MAX;
	/* N19 code for HQ-355015 by p-yeyinzi at 2023/01/06 - end*/

	return 0;
}

static int battery_chg_ship_mode(struct notifier_block *nb, unsigned long code,
		void *unused)
{
	struct battery_charger_notify_msg msg_notify = { { 0 } };
	struct battery_charger_ship_mode_req_msg msg = { { 0 } };
	struct battery_chg_dev *bcdev = container_of(nb, struct battery_chg_dev,
						     reboot_notifier);
	int rc;

	pr_err("enter battery_chg_ship_mode");
	msg_notify.hdr.owner = MSG_OWNER_BC;
	msg_notify.hdr.type = MSG_TYPE_NOTIFY;
	msg_notify.hdr.opcode = BC_SHUTDOWN_NOTIFY;

	rc = battery_chg_write(bcdev, &msg_notify, sizeof(msg_notify));
	if (rc < 0)
		pr_err("Failed to send shutdown notification rc=%d\n", rc);

	if (!bcdev->ship_mode_en)
		return NOTIFY_DONE;

	pr_err("battery_chg_ship_mode::ship_mode_en is true");
	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_SHIP_MODE_REQ_SET;
	msg.ship_mode_type = SHIP_MODE_PMIC;

	/* N19 code for HQ-351328 by p-gucheng at 2024/01/23 */
	if (code == SYS_RESTART) {
		rc = battery_chg_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			pr_emerg("Failed to write ship mode: %d\n", rc);
	}

	return NOTIFY_DONE;
}

/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  - start */
static int battery_chg_shutdown(struct notifier_block *nb, unsigned long code,
		void *unused)
{
	struct battery_charger_shutdown_req_msg msg = { { 0 } };
	struct battery_chg_dev *bcdev = container_of(nb, struct battery_chg_dev,
						     shutdown_notifier);
	int rc;

	pr_err("enter battery_chg_shutdown");
	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_SHUTDOWN_REQ_SET;

	if (code == SYS_POWER_OFF || code == SYS_RESTART) {
		pr_err("start adsp shutdown\n");
		rc = battery_chg_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			pr_emerg("Failed to write shutdown cmd to adsp: %d\n", rc);
	}

	return NOTIFY_DONE;
}
/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  - end */

static void panel_event_notifier_callback(enum panel_event_notifier_tag tag,
			struct panel_event_notification *notification, void *data)
{
	struct battery_chg_dev *bcdev = data;

	if (!notification) {
		pr_debug("Invalid panel notification\n");
		return;
	}

	pr_debug("panel event received, type: %d\n", notification->notif_type);
	switch (notification->notif_type) {
	case DRM_PANEL_EVENT_BLANK:
		battery_chg_notify_disable(bcdev);
		break;
	case DRM_PANEL_EVENT_UNBLANK:
		battery_chg_notify_enable(bcdev);
		break;
	default:
		pr_debug("Ignore panel event: %d\n", notification->notif_type);
		break;
	}
}

static int battery_chg_register_panel_notifier(struct battery_chg_dev *bcdev)
{
	struct device_node *np = bcdev->dev->of_node;
	struct device_node *pnode;
	struct drm_panel *panel, *active_panel = NULL;
	void *cookie = NULL;
	int i, count, rc;

	count = of_count_phandle_with_args(np, "qcom,display-panels", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		pnode = of_parse_phandle(np, "qcom,display-panels", i);
		if (!pnode)
			return -ENODEV;

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
			dev_err(bcdev->dev, "Failed to find active panel, rc=%d\n");
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

	pr_debug("register panel notifier successful\n");
	bcdev->notifier_cookie = cookie;
	return 0;
}

static int register_extcon_conn_type(struct battery_chg_dev *bcdev)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	rc = read_property_id(bcdev, pst, USB_CONNECTOR_TYPE);
	if (rc < 0) {
		pr_err("Failed to read prop USB_CONNECTOR_TYPE, rc=%d\n",
			rc);
		return rc;
	}

	bcdev->connector_type = pst->prop[USB_CONNECTOR_TYPE];
	bcdev->usb_prev_mode = EXTCON_NONE;

	bcdev->extcon = devm_extcon_dev_allocate(bcdev->dev,
						bcdev_usb_extcon_cable);
	if (IS_ERR(bcdev->extcon)) {
		rc = PTR_ERR(bcdev->extcon);
		pr_err("Failed to allocate extcon device rc=%d\n", rc);
		return rc;
	}

	rc = devm_extcon_dev_register(bcdev->dev, bcdev->extcon);
	if (rc < 0) {
		pr_err("Failed to register extcon device rc=%d\n", rc);
		return rc;
	}
	rc = extcon_set_property_capability(bcdev->extcon, EXTCON_USB,
					    EXTCON_PROP_USB_SS);
	rc |= extcon_set_property_capability(bcdev->extcon,
					     EXTCON_USB_HOST, EXTCON_PROP_USB_SS);
	if (rc < 0)
		pr_err("failed to configure extcon capabilities rc=%d\n", rc);
	else
		pr_debug("Registered extcon, connector_type %s\n",
			 bcdev->connector_type ? "uusb" : "Typec");

	return rc;
}

/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - start */
static void batt_soc_monitor_work(struct work_struct *work)
{
	int prop_id, msoc;
	struct battery_chg_dev *bcdev = container_of(work, struct battery_chg_dev,soc_monitor_work.work);
	/* get Monotionic SOC*/
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	prop_id = get_property_id(pst, POWER_SUPPLY_PROP_CAPACITY);
	read_property_id(bcdev, pst, prop_id);
	/* N19 code for HQHW-7063 by p-tangsufeng at 2024/04/10 - start */
	msoc = (pst->prop[prop_id] + 99) / 98;
	if (msoc > 100)
		msoc = 100;
	if (pst->prop[prop_id] == 100)
		msoc = 1;
	pr_info(" soc_monitor_work  msoc = %d UISOC = %d", pst->prop[prop_id], msoc);
	/* N19 code for HQHW-7063 by p-tangsufeng at 2024/04/10 - end */
	fg_battery_soc_smooth_tracking(bcdev, msoc);

	schedule_delayed_work(&bcdev->soc_monitor_work,
				msecs_to_jiffies(MONITOR_SOC_WAIT_MS));
}
/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - end */

/* N19 code for HQ-371347 by p-tangsufeng at 2024/2/19 - start*/
static void xm_uevent_report(struct battery_chg_dev *bcdev)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_XM];
	int soc_decimal_rate = 0;
	int soc_decimal = 0;
	/* N19 code for HQ-376529 by p-tangsufeng at 2024/3/18 - start*/
	int quick_chg_type = 0;
	static int quick_chg_type_cnt = 0;
	/* N19 code for HQ-376529 by p-tangsufeng at 2024/3/18 - end*/

	char quick_charge_string[64];
	char soc_decimal_string[64];
	char soc_decimal_string_rate[64];

	char *envp[] = {
		quick_charge_string,
		soc_decimal_string,
		soc_decimal_string_rate,
		NULL,
	};
	
	/* N19 code for HQ-376529 by p-tangsufeng at 2024/3/18 - start*/
	quick_chg_type = quick_charge_type(bcdev);
	sprintf(quick_charge_string, "POWER_SUPPLY_QUICK_CHARGE_TYPE=%d", quick_chg_type);

	if (quick_chg_type == 3 && quick_chg_type_cnt <1) {
		read_property_id(bcdev, pst, XM_PROP_SOC_DECIMAL);
		read_property_id(bcdev, pst, XM_PROP_SOC_DECIMAL_RATE);
		quick_chg_type_cnt ++;
		msleep(1000);
		soc_decimal = pst->prop[XM_PROP_SOC_DECIMAL];
		if (soc_decimal < 0)
			soc_decimal = 0;
		sprintf(soc_decimal_string, "POWER_SUPPLY_SOC_DECIMAL=%d", soc_decimal);

		soc_decimal_rate = pst->prop[XM_PROP_SOC_DECIMAL_RATE];
		if (soc_decimal_rate < 0 || soc_decimal_rate > 100)
			soc_decimal_rate = 0;
		sprintf(soc_decimal_string_rate, "POWER_SUPPLY_SOC_DECIMAL_RATE=%d", soc_decimal_rate);
	} else if (quick_chg_type != 3)
		quick_chg_type_cnt = 0;

	kobject_uevent_env(&bcdev->dev->kobj, KOBJ_CHANGE, envp);

	pr_err("envp[0]:%s envp[1]:%s envp[2]:%s",envp[0],envp[1],envp[2]);
}
/* N19 code for HQ-371347 by p-tangsufeng at 2024/2/19 - end*/

/* N19 code for HQHW-6390 by p-xuyechen at 2024/2/29 - start */
static void xm_uevent_report_shutdown_delay(struct battery_chg_dev *bcdev, bool shutdown_delay)
{
	char shutdown_string[64] = {0};
	char *envp[] = {
		shutdown_string,
		NULL,
	};

	sprintf(shutdown_string, "POWER_SUPPLY_SHUTDOWN_DELAY=%d", shutdown_delay);

	kobject_uevent_env(&bcdev->dev->kobj, KOBJ_CHANGE, envp);

	pr_info("shutdown uvent:%s, shutdown_delay_en = %d", envp[0], bcdev->shutdown_delay_en);
}
/* N19 code for HQHW-6390 by p-xuyechen at 2024/2/29 - end */

static int battery_chg_probe(struct platform_device *pdev)
{
	struct battery_chg_dev *bcdev;
	struct device *dev = &pdev->dev;
	struct pmic_glink_client_data client_data = { };
	int rc, i;

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
	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - start*/
	bcdev->psy_list[PSY_TYPE_XM].map = xm_prop_map;
	bcdev->psy_list[PSY_TYPE_XM].prop_count = XM_PROP_MAX;
	bcdev->psy_list[PSY_TYPE_XM].opcode_get = BC_XM_STATUS_GET;
	bcdev->psy_list[PSY_TYPE_XM].opcode_set = BC_XM_STATUS_SET;
	/* N19 code for HQ-353528 by p-yeyinzi at 2023/12/10 - start*/

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

	bcdev->ss_auth_data =
		devm_kzalloc(&pdev->dev, BATTERY_SS_AUTH_DATA_LEN * sizeof(u32), GFP_KERNEL);
        if (!bcdev->ss_auth_data) {
		return -ENOMEM;
	}

	mutex_init(&bcdev->rw_lock);
	init_rwsem(&bcdev->state_sem);
	init_completion(&bcdev->ack);
	init_completion(&bcdev->fw_buf_ack);
	init_completion(&bcdev->fw_update_ack);
	INIT_WORK(&bcdev->subsys_up_work, battery_chg_subsys_up_work);
	INIT_WORK(&bcdev->usb_type_work, battery_chg_update_usb_type_work);
	INIT_WORK(&bcdev->battery_check_work, battery_chg_check_status_work);

	/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - start*/
	INIT_DELAYED_WORK( &bcdev->soc_monitor_work, batt_soc_monitor_work);
	schedule_delayed_work(&bcdev->soc_monitor_work, msecs_to_jiffies(INIT_MONITOR_SOC_WAIT_MS));
	/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - end*/

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
	/*
	 * This should be initialized here so that battery_chg_callback
	 * can run successfully when battery_chg_parse_dt() starts
	 * reading BATT_CHG_CTRL_LIM_MAX parameter and waits for a response.
	 */
	bcdev->initialized = true;
	up_write(&bcdev->state_sem);

	bcdev->reboot_notifier.notifier_call = battery_chg_ship_mode;
	bcdev->reboot_notifier.priority = 255;
	register_reboot_notifier(&bcdev->reboot_notifier);

	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  - start */
	bcdev->shutdown_notifier.notifier_call = battery_chg_shutdown;
	bcdev->shutdown_notifier.priority = 255;
	register_reboot_notifier(&bcdev->shutdown_notifier);
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  - end */

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

	if (bcdev->wls_not_supported)
		bcdev->battery_class.class_groups = battery_class_no_wls_groups;
	else
		bcdev->battery_class.class_groups = battery_class_groups;

	rc = class_register(&bcdev->battery_class);
	if (rc < 0) {
		dev_err(dev, "Failed to create battery_class rc=%d\n", rc);
		goto error;
	}

	bcdev->wls_fw_update_time_ms = WLS_FW_UPDATE_TIME_MS;
	battery_chg_add_debugfs(bcdev);
	bcdev->notify_en = false;
	battery_chg_notify_enable(bcdev);
	device_init_wakeup(bcdev->dev, true);
	rc = register_extcon_conn_type(bcdev);
	if (rc < 0)
		dev_warn(dev, "Failed to register extcon rc=%d\n", rc);

	if (bcdev->connector_type == USB_CONNECTOR_TYPE_MICRO_USB) {
		bcdev->typec_class = qti_typec_class_init(bcdev->dev);
		if (IS_ERR_OR_NULL(bcdev->typec_class)) {
			dev_err(dev, "Failed to init typec class err=%d\n",
				PTR_ERR(bcdev->typec_class));
			return PTR_ERR(bcdev->typec_class);
		}
	}
	schedule_work(&bcdev->usb_type_work);
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  */
	bcdev->shutdown_delay_en = true;

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
	/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 */
	cancel_delayed_work_sync(&bcdev->soc_monitor_work);
	complete(&bcdev->ack);
	unregister_reboot_notifier(&bcdev->reboot_notifier);
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  */
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

	qti_typec_class_deinit(bcdev->typec_class);
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
	/* N19 code for HQ-351608 by p-gucheng at 2023/1/18  */
	unregister_reboot_notifier(&bcdev->shutdown_notifier);
	return 0;
}

/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - start */
static int chg_suspend(struct device *dev)
{
	struct battery_chg_dev *bcdev = dev_get_drvdata(dev);
	cancel_delayed_work(&bcdev->soc_monitor_work);
	pr_err("chg suspend\n");
	return 0;
}

static int chg_resume(struct device *dev)
{
	struct battery_chg_dev *bcdev = dev_get_drvdata(dev);

	schedule_delayed_work(&bcdev->soc_monitor_work,
				msecs_to_jiffies(MONITOR_SOC_WAIT_MS));

	pr_err("chg resume\n");
	return 0;
}
/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - end */

static const struct of_device_id battery_chg_match_table[] = {
	{ .compatible = "qcom,battery-charger" },
	{},
};

/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - start*/
static const struct dev_pm_ops chg_pm_ops = {
	.resume      = chg_resume,
	.suspend	= chg_suspend,
};
/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 - end*/

static struct platform_driver battery_chg_driver = {
	.driver = {
		.name = "qti_battery_charger",
		.of_match_table = battery_chg_match_table,
		/* N19 code for HQHW-6569 by p-gucheng at 2024/03/18 */
		.pm     = &chg_pm_ops,
	},
	.probe = battery_chg_probe,
	.remove = battery_chg_remove,
};
module_platform_driver(battery_chg_driver);

MODULE_DESCRIPTION("QTI Glink battery charger driver");
MODULE_LICENSE("GPL v2");
