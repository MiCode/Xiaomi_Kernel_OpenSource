/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TCPM_H_
#define TCPM_H_

#include <linux/kernel.h>
#include <linux/notifier.h>

#include "tcpci_config.h"
#include "tcpm_pd.h"

struct tcpc_device;

/*
 * Type-C Port Notify Chain
 */

enum typec_attach_type {
	TYPEC_UNATTACHED = 0,
	TYPEC_ATTACHED_SNK,
	TYPEC_ATTACHED_SRC,
	TYPEC_ATTACHED_AUDIO,
	TYPEC_ATTACHED_DEBUG,			/* Rd, Rd */

/* CONFIG_TYPEC_CAP_DBGACC_SNK */
	TYPEC_ATTACHED_DBGACC_SNK,		/* Rp, Rp */

/* CONFIG_TYPEC_CAP_CUSTOM_SRC */
	TYPEC_ATTACHED_CUSTOM_SRC,		/* Same Rp */

/* CONFIG_TYPEC_CAP_NORP_SRC */
	TYPEC_ATTACHED_NORP_SRC,		/* No Rp */
};

enum pd_connect_result {
	PD_CONNECT_NONE = 0,
	PD_CONNECT_TYPEC_ONLY_SNK_DFT,
	PD_CONNECT_TYPEC_ONLY_SNK,
	PD_CONNECT_TYPEC_ONLY_SRC,
	PD_CONNECT_PE_READY_SNK,
	PD_CONNECT_PE_READY_SRC,
	PD_CONNECT_PE_READY_SNK_PD30,
	PD_CONNECT_PE_READY_SRC_PD30,
	PD_CONNECT_PE_READY_SNK_APDO,
	PD_CONNECT_HARD_RESET,

/* CONFIG_USB_PD_CUSTOM_DBGACC */
	PD_CONNECT_PE_READY_DBGACC_UFP,
	PD_CONNECT_PE_READY_DBGACC_DFP,
};

enum tcpc_vconn_supply_mode {
	/* Never provide vconn even in TYPE-C state (reject swap to On) */
	TCPC_VCONN_SUPPLY_NEVER = 0,

	/* Always provide vconn */
	TCPC_VCONN_SUPPLY_ALWAYS,

	/* Always provide vconn only if we detect Ra, otherwise startup only */
	TCPC_VCONN_SUPPLY_EMARK_ONLY,

	/* Only provide vconn during DPM initial (aginst spec) */
	TCPC_VCONN_SUPPLY_STARTUP,

	TCPC_VCONN_SUPPLY_NR,
};

/* Power role */
#define PD_ROLE_SINK   0
#define PD_ROLE_SOURCE 1

/* Data role */
#define PD_ROLE_UFP    0
#define PD_ROLE_DFP    1

/* Vconn role */
#define PD_ROLE_VCONN_OFF 0
#define PD_ROLE_VCONN_ON  1
#define PD_ROLE_VCONN_DYNAMIC_OFF		2
#define PD_ROLE_VCONN_DYNAMIC_ON		3

enum {
	TCP_NOTIFY_IDX_MODE = 0,
	TCP_NOTIFY_IDX_VBUS,
	TCP_NOTIFY_IDX_USB,
	TCP_NOTIFY_IDX_MISC,
	TCP_NOTIFY_IDX_NR,
};

#define TCP_NOTIFY_TYPE_MODE	(1 << TCP_NOTIFY_IDX_MODE)
#define TCP_NOTIFY_TYPE_VBUS	(1 << TCP_NOTIFY_IDX_VBUS)
#define TCP_NOTIFY_TYPE_USB	(1 << TCP_NOTIFY_IDX_USB)
#define TCP_NOTIFY_TYPE_MISC	(1 << TCP_NOTIFY_IDX_MISC)

#define TCP_NOTIFY_TYPE_ALL	((1 << TCP_NOTIFY_IDX_NR) - 1)

enum {
	/* TCP_NOTIFY_TYPE_MODE */
	TCP_NOTIFY_ENTER_MODE,
	TCP_NOTIFY_MODE_START = TCP_NOTIFY_ENTER_MODE,
	TCP_NOTIFY_EXIT_MODE,
	TCP_NOTIFY_AMA_DP_STATE,
	TCP_NOTIFY_AMA_DP_ATTENTION,
	TCP_NOTIFY_AMA_DP_HPD_STATE,
	TCP_NOTIFY_DC_EN_UNLOCK,
	TCP_NOTIFY_UVDM,
	TCP_NOTIFY_MODE_END = TCP_NOTIFY_UVDM,

	/* TCP_NOTIFY_TYPE_VBUS */
	TCP_NOTIFY_DIS_VBUS_CTRL,
	TCP_NOTIFY_VBUS_START = TCP_NOTIFY_DIS_VBUS_CTRL,
	TCP_NOTIFY_SOURCE_VCONN,
	TCP_NOTIFY_SOURCE_VBUS,
	TCP_NOTIFY_SINK_VBUS,
	TCP_NOTIFY_EXT_DISCHARGE,
	TCP_NOTIFY_ATTACHWAIT_SNK,
	TCP_NOTIFY_ATTACHWAIT_SRC,
	TCP_NOTIFY_VBUS_END = TCP_NOTIFY_ATTACHWAIT_SRC,

	/* TCP_NOTIFY_TYPE_USB */
	TCP_NOTIFY_TYPEC_STATE,
	TCP_NOTIFY_USB_START = TCP_NOTIFY_TYPEC_STATE,
	TCP_NOTIFY_PD_STATE,
	TCP_NOTIFY_USB_END = TCP_NOTIFY_PD_STATE,

	/* TCP_NOTIFY_TYPE_MISC */
	TCP_NOTIFY_PR_SWAP,
	TCP_NOTIFY_MISC_START = TCP_NOTIFY_PR_SWAP,
	TCP_NOTIFY_DR_SWAP,
	TCP_NOTIFY_VCONN_SWAP,
	TCP_NOTIFY_HARD_RESET_STATE,
	TCP_NOTIFY_ALERT,
	TCP_NOTIFY_STATUS,
	TCP_NOTIFY_REQUEST_BAT_INFO,
	TCP_NOTIFY_WD_STATUS,
	TCP_NOTIFY_CABLE_TYPE,
	TCP_NOTIFY_SOFT_RESET,
	TCP_NOTIFY_MISC_END = TCP_NOTIFY_SOFT_RESET,
};

struct tcp_ny_pd_state {
	uint8_t connected;
};

struct tcp_ny_swap_state {
	uint8_t new_role;
};

struct tcp_ny_enable_state {
	bool en;
};

struct tcp_ny_typec_state {
	uint8_t rp_level;
	uint8_t local_rp_level;
	uint8_t polarity;
	uint8_t old_state;
	uint8_t new_state;
};

enum {
	TCP_VBUS_CTRL_REMOVE = 0,
	TCP_VBUS_CTRL_TYPEC = 1,
	TCP_VBUS_CTRL_PD = 2,

	TCP_VBUS_CTRL_HRESET = TCP_VBUS_CTRL_PD,
	TCP_VBUS_CTRL_PR_SWAP = 3,
	TCP_VBUS_CTRL_REQUEST = 4,
	TCP_VBUS_CTRL_STANDBY = 5,
	TCP_VBUS_CTRL_STANDBY_UP = 6,
	TCP_VBUS_CTRL_STANDBY_DOWN = 7,

	TCP_VBUS_CTRL_PD_DETECT = (1 << 7),

	TCP_VBUS_CTRL_PD_HRESET =
		TCP_VBUS_CTRL_HRESET | TCP_VBUS_CTRL_PD_DETECT,

	TCP_VBUS_CTRL_PD_PR_SWAP =
		TCP_VBUS_CTRL_PR_SWAP | TCP_VBUS_CTRL_PD_DETECT,

	TCP_VBUS_CTRL_PD_REQUEST =
		TCP_VBUS_CTRL_REQUEST | TCP_VBUS_CTRL_PD_DETECT,

	TCP_VBUS_CTRL_PD_STANDBY =
		TCP_VBUS_CTRL_STANDBY | TCP_VBUS_CTRL_PD_DETECT,

	TCP_VBUS_CTRL_PD_STANDBY_UP =
		TCP_VBUS_CTRL_STANDBY_UP | TCP_VBUS_CTRL_PD_DETECT,

	TCP_VBUS_CTRL_PD_STANDBY_DOWN =
		TCP_VBUS_CTRL_STANDBY_DOWN | TCP_VBUS_CTRL_PD_DETECT,
};

struct tcp_ny_vbus_state {
	int mv;
	int ma;
	uint8_t type;
};

struct tcp_ny_mode_ctrl {
	uint16_t svid;
	uint8_t ops;
	uint32_t mode;
};

enum {
	SW_USB = 0,
	SW_DFP_D,
	SW_UFP_D,
};

struct tcp_ny_ama_dp_state {
	uint8_t sel_config;
	uint8_t signal;
	uint8_t pin_assignment;
	uint8_t polarity;
	uint8_t active;
};

enum {
	TCP_DP_UFP_U_MASK = 0x7C,
	TCP_DP_UFP_U_POWER_LOW = 1 << 2,
	TCP_DP_UFP_U_ENABLED = 1 << 3,
	TCP_DP_UFP_U_MF_PREFER = 1 << 4,
	TCP_DP_UFP_U_USB_CONFIG = 1 << 5,
	TCP_DP_UFP_U_EXIT_MODE = 1 << 6,
};

struct tcp_ny_ama_dp_attention {
	uint8_t state;
};

struct tcp_ny_ama_dp_hpd_state {
	uint8_t irq;
	uint8_t state;
};

struct tcp_ny_uvdm {
	bool ack;
	uint8_t uvdm_cnt;
	uint16_t uvdm_svid;
	uint32_t *uvdm_data;
};

/*
 * TCP_NOTIFY_HARD_RESET_STATE
 *
 * Please don't expect that every signal will have a corresponding result.
 * The signal can be generated multiple times before receiving a result.
 */

enum {
	/* HardReset finished because recv GoodCRC or TYPE-C only */
	TCP_HRESET_RESULT_DONE = 0,

	/* HardReset failed because detach or error recovery */
	TCP_HRESET_RESULT_FAIL,

	/* HardReset signal from Local Policy Engine */
	TCP_HRESET_SIGNAL_SEND,

	/* HardReset signal from Port Partner */
	TCP_HRESET_SIGNAL_RECV,
};

struct tcp_ny_hard_reset_state {
	uint8_t state;
};

struct tcp_ny_alert {
	uint32_t ado;
};

struct tcp_ny_status {
	const struct pd_status *sdb;
};

struct tcp_ny_request_bat {
	enum pd_battery_reference ref;
};

struct tcp_ny_wd_status {
	bool water_detected;
};

enum tcpc_cable_type {
	TCPC_CABLE_TYPE_NONE = 0,
	TCPC_CABLE_TYPE_A2C,
	TCPC_CABLE_TYPE_C2C,
	TCPC_CABLE_TYPE_MAX,
};

struct tcp_ny_cable_type {
	enum tcpc_cable_type type;
};

struct tcp_notify {
	union {
		struct tcp_ny_enable_state en_state;
		struct tcp_ny_vbus_state vbus_state;
		struct tcp_ny_typec_state typec_state;
		struct tcp_ny_swap_state swap_state;
		struct tcp_ny_pd_state pd_state;
		struct tcp_ny_mode_ctrl mode_ctrl;
		struct tcp_ny_ama_dp_state ama_dp_state;
		struct tcp_ny_ama_dp_attention ama_dp_attention;
		struct tcp_ny_ama_dp_hpd_state ama_dp_hpd_state;
		struct tcp_ny_uvdm uvdm_msg;
		struct tcp_ny_hard_reset_state hreset_state;
		struct tcp_ny_alert alert_msg;
		struct tcp_ny_status status_msg;
		struct tcp_ny_request_bat request_bat;
		struct tcp_ny_wd_status wd_status;
		struct tcp_ny_cable_type cable_type;
	};
};

/*
 * Type-C Port Control I/F
 */

enum tcpm_error_list {
	TCPM_SUCCESS = 0,
	TCPM_ERROR_UNKNOWN = -1,
	TCPM_ERROR_UNATTACHED = -2,
	TCPM_ERROR_PARAMETER = -3,
	TCPM_ERROR_PUT_EVENT = -4,
	TCPM_ERROR_NO_SUPPORT = -5,
	TCPM_ERROR_NO_PD_CONNECTED = -6,
	TCPM_ERROR_NO_POWER_CABLE = -7,
	TCPM_ERROR_NO_PARTNER_INFORM = -8,
	TCPM_ERROR_NO_SOURCE_CAP = -9,
	TCPM_ERROR_NO_SINK_CAP = -10,
	TCPM_ERROR_NOT_DRP_ROLE = -11,
	TCPM_ERROR_DURING_ROLE_SWAP = -12,
	TCPM_ERROR_NO_EXPLICIT_CONTRACT = -13,
	TCPM_ERROR_ERROR_RECOVERY = -14,
	TCPM_ERROR_NOT_FOUND = -15,
	TCPM_ERROR_INVALID_POLICY = -16,
	TCPM_ERROR_EXPECT_CB2 = -17,
	TCPM_ERROR_POWER_ROLE = -18,
	TCPM_ERROR_PE_NOT_READY = -19,
	TCPM_ERROR_REPEAT_POLICY = -20,
	TCPM_ERROR_CUSTOM_SRC = -21,
	TCPM_ERROR_NO_IMPLEMENT = -22,
	TCPM_ALERT = -23,
};

/* Inquire TCPM status */

enum tcpc_cc_voltage_status {
	TYPEC_CC_VOLT_OPEN = 0,
	TYPEC_CC_VOLT_RA = 1,
	TYPEC_CC_VOLT_RD = 2,

	TYPEC_CC_VOLT_SNK_DFT = 5,
	TYPEC_CC_VOLT_SNK_1_5 = 6,
	TYPEC_CC_VOLT_SNK_3_0 = 7,

	TYPEC_CC_DRP_TOGGLING = 15,
};

enum tcpm_vbus_level {
#ifdef CONFIG_TCPC_VSAFE0V_DETECT
	TCPC_VBUS_SAFE0V = 0,	/* < 0.8V */
	TCPC_VBUS_INVALID,		/* > 0.8V */
	TCPC_VBUS_VALID,		/* > 4.5V */
#else
	TCPC_VBUS_INVALID = 0,
	TCPC_VBUS_VALID,
#endif /* CONFIG_TCPC_VSAFE0V_DETECT */
};

enum typec_role_defination {
	TYPEC_ROLE_UNKNOWN = 0,
	TYPEC_ROLE_SNK,
	TYPEC_ROLE_SRC,
	TYPEC_ROLE_DRP,
	TYPEC_ROLE_TRY_SRC,
	TYPEC_ROLE_TRY_SNK,
	TYPEC_ROLE_NR,
};

enum pd_cable_current_limit {
	PD_CABLE_CURR_UNKNOWN = 0,
	PD_CABLE_CURR_1A5 = 1,
	PD_CABLE_CURR_3A = 2,
	PD_CABLE_CURR_5A = 3,
};

/* DPM Flags */

#define DPM_FLAGS_PARTNER_DR_POWER		(1<<0)
#define DPM_FLAGS_PARTNER_DR_DATA		(1<<1)
#define DPM_FLAGS_PARTNER_EXTPOWER		(1<<2)
#define DPM_FLAGS_PARTNER_USB_COMM		(1<<3)
#define DPM_FLAGS_PARTNER_USB_SUSPEND		(1<<4)
#define DPM_FLAGS_PARTNER_HIGH_CAP		(1<<5)

#define DPM_FLAGS_PARTNER_MISMATCH		(1<<7)
#define DPM_FLAGS_PARTNER_GIVE_BACK		(1<<8)
#define DPM_FLAGS_PARTNER_NO_SUSPEND		(1<<9)

#define DPM_FLAGS_RESET_PARTNER_MASK	\
	(DPM_FLAGS_PARTNER_DR_POWER | DPM_FLAGS_PARTNER_DR_DATA |\
	 DPM_FLAGS_PARTNER_EXTPOWER | DPM_FLAGS_PARTNER_USB_COMM |\
	 DPM_FLAGS_PARTNER_USB_SUSPEND)

/* DPM_CAPS */

#define DPM_CAP_LOCAL_DR_POWER			(1<<0)
#define DPM_CAP_LOCAL_DR_DATA			(1<<1)
#define DPM_CAP_LOCAL_EXT_POWER			(1<<2)
#define DPM_CAP_LOCAL_USB_COMM			(1<<3)
#define DPM_CAP_LOCAL_USB_SUSPEND		(1<<4)
#define DPM_CAP_LOCAL_HIGH_CAP			(1<<5)
#define DPM_CAP_LOCAL_GIVE_BACK			(1<<6)
#define DPM_CAP_LOCAL_NO_SUSPEND		(1<<7)
#define DPM_CAP_LOCAL_VCONN_SUPPLY		(1<<8)

#define DPM_CAP_ATTEMP_ENTER_DC_MODE		(1<<11)
#define DPM_CAP_ATTEMP_DISCOVER_CABLE_DFP	(1<<12)
#define DPM_CAP_ATTEMP_ENTER_DP_MODE		(1<<13)
#define DPM_CAP_ATTEMP_DISCOVER_CABLE		(1<<14)
#define DPM_CAP_ATTEMP_DISCOVER_ID		(1<<15)
#define DPM_CAP_ATTEMP_DISCOVER_SVID		(1<<16)

enum dpm_cap_pr_check_prefer {
	DPM_CAP_PR_CHECK_DISABLE = 0,
	DPM_CAP_PR_CHECK_PREFER_SNK = 1,
	DPM_CAP_PR_CHECK_PREFER_SRC = 2,
};

#define DPM_CAP_PR_CHECK_PROP(cap)		((cap & 0x03) << 18)
#define DPM_CAP_EXTRACT_PR_CHECK(raw)		((raw >> 18) & 0x03)
#define DPM_CAP_PR_SWAP_REJECT_AS_SRC		(1<<20)
#define DPM_CAP_PR_SWAP_REJECT_AS_SNK		(1<<21)
#define DPM_CAP_PR_SWAP_CHECK_GP_SRC		(1<<22)
#define DPM_CAP_PR_SWAP_CHECK_GP_SNK		(1<<23)
#define DPM_CAP_PR_SWAP_CHECK_GOOD_POWER	\
	(DPM_CAP_PR_SWAP_CHECK_GP_SRC | DPM_CAP_PR_SWAP_CHECK_GP_SNK)

#define DPM_CAP_CHECK_EXT_POWER	\
	(DPM_CAP_LOCAL_EXT_POWER | DPM_CAP_PR_SWAP_CHECK_GOOD_POWER)

enum dpm_cap_dr_check_prefer {
	DPM_CAP_DR_CHECK_DISABLE = 0,
	DPM_CAP_DR_CHECK_PREFER_UFP = 1,
	DPM_CAP_DR_CHECK_PREFER_DFP = 2,
};

#define DPM_CAP_DR_CHECK_PROP(cap)		((cap & 0x03) << 22)
#define DPM_CAP_EXTRACT_DR_CHECK(raw)		((raw >> 22) & 0x03)
#define DPM_CAP_DR_SWAP_REJECT_AS_DFP		(1<<24)
#define DPM_CAP_DR_SWAP_REJECT_AS_UFP		(1<<25)

#define DPM_CAP_DP_PREFER_MF				(1<<29)


/* Power Data Object related structure */

struct tcpm_power_cap {
	uint8_t cnt;
	uint32_t pdos[PDO_MAX_NR];
};

struct tcpm_remote_power_cap {
	uint8_t selected_cap_idx;
	uint8_t nr;
	int max_mv[PDO_MAX_NR];
	int min_mv[PDO_MAX_NR];
	int ma[PDO_MAX_NR];
	uint8_t type[PDO_MAX_NR];
};

enum tcpm_power_cap_val_type {
	TCPM_POWER_CAP_VAL_TYPE_FIXED = 0,
	TCPM_POWER_CAP_VAL_TYPE_BATTERY = 1,
	TCPM_POWER_CAP_VAL_TYPE_VARIABLE = 2,
	TCPM_POWER_CAP_VAL_TYPE_AUGMENT = 3,

	TCPM_POWER_CAP_VAL_TYPE_UNKNOWN = 0xff,
};

#define TCPM_APDO_TYPE_MASK		(0x0f)

enum tcpm_power_cap_apdo_type {
	TCPM_POWER_CAP_APDO_TYPE_PPS = 1 << 0,

	TCPM_POWER_CAP_APDO_TYPE_PPS_CF = (1 << 7),
};

struct tcpm_power_cap_val {
	uint8_t type;
	uint8_t apdo_type;
	uint8_t pwr_limit;

	int max_mv;
	int min_mv;

	union {
		int uw;
		int ma;
	};
};

struct tcpm_power_cap_list {
	uint8_t nr;
	struct tcpm_power_cap_val cap_val[PDO_MAX_NR];
};

/* Request TCPM to execure PD/VDM function */

struct tcp_dpm_event;

enum tcp_dpm_return_code {
	TCP_DPM_RET_SUCCESS = 0,
	TCP_DPM_RET_SENT = 0,
	TCP_DPM_RET_VDM_ACK = 0,

	TCP_DPM_RET_DENIED_UNKNOWN,
	TCP_DPM_RET_DENIED_NOT_READY,
	TCP_DPM_RET_DENIED_LOCAL_CAP,
	TCP_DPM_RET_DENIED_PARTNER_CAP,
	TCP_DPM_RET_DENIED_SAME_ROLE,
	TCP_DPM_RET_DENIED_INVALID_REQUEST,
	TCP_DPM_RET_DENIED_REPEAT_REQUEST,
	TCP_DPM_RET_DENIED_WRONG_DATA_ROLE,
	TCP_DPM_RET_DENIED_PD_REV,

	TCP_DPM_RET_DROP_CC_DETACH,
	TCP_DPM_RET_DROP_SENT_SRESET,
	TCP_DPM_RET_DROP_RECV_SRESET,
	TCP_DPM_RET_DROP_SENT_HRESET,
	TCP_DPM_RET_DROP_RECV_HRESET,
	TCP_DPM_RET_DROP_ERROR_REOCVERY,
	TCP_DPM_RET_DROP_SEND_BIST,
	TCP_DPM_RET_DROP_PE_BUSY,	/* SinkTXNg*/

	TCP_DPM_RET_WAIT,
	TCP_DPM_RET_REJECT,
	TCP_DPM_RET_TIMEOUT,
	TCP_DPM_RET_VDM_NAK,
	TCP_DPM_RET_NOT_SUPPORT,

	TCP_DPM_RET_BK_TIMEOUT,
	TCP_DPM_RET_NO_RESPONSE,

	TCP_DPM_RET_NR,
};

enum TCP_DPM_EVT_ID {
	TCP_DPM_EVT_UNKONW = 0,

	TCP_DPM_EVT_PD_COMMAND,

	TCP_DPM_EVT_PR_SWAP_AS_SNK = TCP_DPM_EVT_PD_COMMAND,
	TCP_DPM_EVT_PR_SWAP_AS_SRC,
	TCP_DPM_EVT_DR_SWAP_AS_UFP,
	TCP_DPM_EVT_DR_SWAP_AS_DFP,
	TCP_DPM_EVT_VCONN_SWAP_OFF,
	TCP_DPM_EVT_VCONN_SWAP_ON,
	TCP_DPM_EVT_GOTOMIN,

	TCP_DPM_EVT_SOFTRESET,
	TCP_DPM_EVT_CABLE_SOFTRESET,

	TCP_DPM_EVT_GET_SOURCE_CAP,
	TCP_DPM_EVT_GET_SINK_CAP,

	TCP_DPM_EVT_REQUEST,
	TCP_DPM_EVT_REQUEST_EX,
	TCP_DPM_EVT_REQUEST_AGAIN,
	TCP_DPM_EVT_BIST_CM2,

	TCP_DPM_EVT_DUMMY,	/* wakeup event thread */

#ifdef CONFIG_USB_PD_REV30
	TCP_DPM_EVT_PD30_COMMAND,
	TCP_DPM_EVT_GET_SOURCE_CAP_EXT = TCP_DPM_EVT_PD30_COMMAND,
	TCP_DPM_EVT_GET_STATUS,
	TCP_DPM_EVT_FR_SWAP_AS_SINK,
	TCP_DPM_EVT_FR_SWAP_AS_SOURCE,
	TCP_DPM_EVT_GET_COUNTRY_CODE,
	TCP_DPM_EVT_GET_PPS_STATUS,

	TCP_DPM_EVT_ALERT,
	TCP_DPM_EVT_GET_COUNTRY_INFO,

	TCP_DPM_EVT_GET_BAT_CAP,
	TCP_DPM_EVT_GET_BAT_STATUS,
	TCP_DPM_EVT_GET_MFRS_INFO,
#endif	/* CONFIG_USB_PD_REV30 */

	TCP_DPM_EVT_VDM_COMMAND,
	TCP_DPM_EVT_DISCOVER_CABLE = TCP_DPM_EVT_VDM_COMMAND,
	TCP_DPM_EVT_DISCOVER_ID,
	TCP_DPM_EVT_DISCOVER_SVIDS,
	TCP_DPM_EVT_DISCOVER_MODES,
	TCP_DPM_EVT_ENTER_MODE,
	TCP_DPM_EVT_EXIT_MODE,
	TCP_DPM_EVT_ATTENTION,

#ifdef CONFIG_USB_PD_ALT_MODE
	TCP_DPM_EVT_DP_ATTENTION,
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	TCP_DPM_EVT_DP_STATUS_UPDATE,
	TCP_DPM_EVT_DP_CONFIG,
#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */
#endif	/* CONFIG_USB_PD_ALT_MODE */

#ifdef CONFIG_USB_PD_CUSTOM_VDM
	TCP_DPM_EVT_UVDM,
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

	TCP_DPM_EVT_IMMEDIATELY,
	TCP_DPM_EVT_HARD_RESET = TCP_DPM_EVT_IMMEDIATELY,
	TCP_DPM_EVT_ERROR_RECOVERY,

	TCP_DPM_EVT_NR,
};

typedef int (*tcp_dpm_event_cb)(
	struct tcpc_device *tcpc, int ret, struct tcp_dpm_event *event);

struct tcp_dpm_event_cb_data {
	void *user_data;
	tcp_dpm_event_cb event_cb;
};

static const struct tcp_dpm_event_cb_data tcp_dpm_evt_cb_null = {
	.user_data = NULL,
	.event_cb = NULL,
};

struct tcp_dpm_new_role {
	uint8_t new_role;
};

struct tcp_dpm_pd_request {
	int mv;
	int ma;
};

struct tcp_dpm_pd_request_ex {
	uint8_t pos;

	union {
		uint32_t max;
		uint32_t max_uw;
		uint32_t max_ma;
	};

	union {
		uint32_t oper;
		uint32_t oper_uw;
		uint32_t oper_ma;
	};
};

struct tcp_dpm_svdm_data {
	uint16_t svid;
	uint8_t ops;
};

struct tcp_dpm_dp_data {
	uint32_t val;
	uint32_t mask;
};

struct tcp_dpm_custom_vdm_data {
	bool wait_resp;
	uint8_t cnt;
	uint32_t vdos[PD_DATA_OBJ_SIZE];
};

struct tcp_dpm_event {
	uint8_t event_id;
	void *user_data;
	tcp_dpm_event_cb event_cb;

	union {
		struct tcp_dpm_pd_request pd_req;
		struct tcp_dpm_pd_request_ex pd_req_ex;


		struct tcp_dpm_dp_data dp_data;
		struct tcp_dpm_custom_vdm_data vdm_data;

		struct tcp_dpm_svdm_data svdm_data;


		struct pd_get_battery_capabilities gbcdb;
		struct pd_get_battery_status gbsdb;
		struct pd_get_manufacturer_info gmidb;

		uint32_t	index;
		uint32_t	data_object[PD_DATA_OBJ_SIZE];
	} tcp_dpm_data;
};

/* KEEP_SVID */

struct tcpm_svid_list {
	uint8_t cnt;
	uint16_t svids[VDO_MAX_SVID_NR];
};

struct tcpm_mode_list {
	uint8_t cnt;
	uint32_t modes[VDO_MAX_NR];
};

/* ALT_DP */

enum pd_dp_ufp_u_state {
	DP_UFP_U_NONE = 0,
	DP_UFP_U_STARTUP,
	DP_UFP_U_WAIT,
	DP_UFP_U_OPERATION,
	DP_UFP_U_STATE_NR,

	DP_UFP_U_ERR = 0X10,
};

enum pd_dp_dfp_u_state {
	DP_DFP_U_NONE = 0,
	DP_DFP_U_DISCOVER_ID,
	DP_DFP_U_DISCOVER_SVIDS,
	DP_DFP_U_DISCOVER_MODES,
	DP_DFP_U_ENTER_MODE,
	DP_DFP_U_STATUS_UPDATE,
	DP_DFP_U_WAIT_ATTENTION,
	DP_DFP_U_CONFIGURE,
	DP_DFP_U_OPERATION,
	DP_DFP_U_STATE_NR,

	DP_DFP_U_ERR = 0X10,

	DP_DFP_U_ERR_DISCOVER_ID_TYPE,
	DP_DFP_U_ERR_DISCOVER_ID_NAK_TIMEOUT,

	DP_DFP_U_ERR_DISCOVER_SVID_DP_SID,
	DP_DFP_U_ERR_DISCOVER_SVID_NAK_TIMEOUT,

	DP_DFP_U_ERR_DISCOVER_MODE_DP_SID,
	DP_DFP_U_ERR_DISCOVER_MODE_CAP,	/* NO SUPPORT UFP-D */
	DP_DFP_U_ERR_DISCOVER_MODE_NAK_TIMEROUT,

	DP_DFP_U_ERR_ENTER_MODE_DP_SID,
	DP_DFP_U_ERR_ENTER_MODE_NAK_TIMEOUT,

	DP_DFP_U_ERR_EXIT_MODE_DP_SID,
	DP_DFP_U_ERR_EXIT_MODE_NAK_TIMEOUT,

	DP_DFP_U_ERR_STATUS_UPDATE_DP_SID,
	DP_DFP_U_ERR_STATUS_UPDATE_NAK_TIMEOUT,
	DP_DFP_U_ERR_STATUS_UPDATE_ROLE,

	DP_DFP_U_ERR_CONFIGURE_SELECT_MODE,
};

/* Custom VDM */

#define PD_UVDM_HDR(vid, custom)	\
	(((vid) << 16) | ((custom) & 0x7FFF))

#define PD_SVDM_HDR(vid, custom)	\
	(((vid) << 16) | (1<<15) | ((custom) & 0x1FDF))

#define PD_UVDM_HDR_CMD(hdr)	\
	(hdr & 0x7FFF)

#define PD_SVDM_HDR_CMD(hdr)	\
	(hdr & 0x1FDF)

#define DPM_CHARGING_POLICY_MASK	(0x0f)

/* Charging Policy */

enum dpm_charging_policy {
	/* VSafe5V only */
	DPM_CHARGING_POLICY_VSAFE5V = 0,

	/* Max Power */
	DPM_CHARGING_POLICY_MAX_POWER = 1,

	/* Custom defined Policy */
	DPM_CHARGING_POLICY_CUSTOM = 2,

	/*  Runtime Policy, restore to default after plug-out or hard-reset */
	DPM_CHARGING_POLICY_RUNTIME = 3,

	/* Direct charge <Variable PDO only> */
	DPM_CHARGING_POLICY_DIRECT_CHARGE = 3,

	/* PPS <Augmented PDO only> */
	DPM_CHARGING_POLICY_PPS = 4,

	/* Default Charging Policy <from DTS>*/
	DPM_CHARGING_POLICY_DEFAULT = 0xff,

	DPM_CHARGING_POLICY_IGNORE_MISMATCH_CURR = 1 << 4,
	DPM_CHARGING_POLICY_PREFER_LOW_VOLTAGE = 1 << 5,
	DPM_CHARGING_POLICY_PREFER_HIGH_VOLTAGE = 1 << 6,

	DPM_CHARGING_POLICY_MAX_POWER_LV =
		DPM_CHARGING_POLICY_MAX_POWER |
		DPM_CHARGING_POLICY_PREFER_LOW_VOLTAGE,
	DPM_CHARGING_POLICY_MAX_POWER_LVIC =
		DPM_CHARGING_POLICY_MAX_POWER_LV |
		DPM_CHARGING_POLICY_IGNORE_MISMATCH_CURR,

	DPM_CHARGING_POLICY_MAX_POWER_HV =
		DPM_CHARGING_POLICY_MAX_POWER |
		DPM_CHARGING_POLICY_PREFER_HIGH_VOLTAGE,
	DPM_CHARGING_POLICY_MAX_POWER_HVIC =
		DPM_CHARGING_POLICY_MAX_POWER_HV |
		DPM_CHARGING_POLICY_IGNORE_MISMATCH_CURR,

	/* DPM_CHARGING_POLICY_PPS */

	DPM_CHARGING_POLICY_PPS_IC =
		DPM_CHARGING_POLICY_PPS |
		DPM_CHARGING_POLICY_IGNORE_MISMATCH_CURR,
};

#ifdef CONFIG_TCPC_CLASS

extern struct tcpc_device
		*tcpc_dev_get_by_name(const char *name);

extern int register_tcp_dev_notifier(struct tcpc_device *tcp_dev,
				struct notifier_block *nb, uint8_t flags);
extern int unregister_tcp_dev_notifier(struct tcpc_device *tcp_dev,
				struct notifier_block *nb, uint8_t flags);

extern int tcpm_shutdown(struct tcpc_device *tcpc);

extern int tcpm_inquire_remote_cc(struct tcpc_device *tcpc,
	uint8_t *cc1, uint8_t *cc2, bool from_ic);
extern int tcpm_inquire_vbus_level(struct tcpc_device *tcpc, bool from_ic);
extern int tcpm_inquire_typec_remote_rp_curr(struct tcpc_device *tcpc);
extern bool tcpm_inquire_cc_polarity(struct tcpc_device *tcpc);
extern uint8_t tcpm_inquire_typec_attach_state(struct tcpc_device *tcpc);
extern uint8_t tcpm_inquire_typec_role(struct tcpc_device *tcpc);
extern uint8_t tcpm_inquire_typec_local_rp(struct tcpc_device *tcpc);

extern int tcpm_typec_set_wake_lock(
	struct tcpc_device *tcpc, bool user_lock);

extern int tcpm_typec_set_usb_sink_curr(
	struct tcpc_device *tcpc, int curr);

extern int tcpm_typec_set_rp_level(
	struct tcpc_device *tcpc, uint8_t level);

extern int tcpm_typec_set_custom_hv(
	struct tcpc_device *tcpc, bool en);

extern int tcpm_typec_role_swap(
	struct tcpc_device *tcpc);

extern int tcpm_typec_change_role(
	struct tcpc_device *tcpc, uint8_t typec_role);

extern int tcpm_typec_change_role_postpone(
	struct tcpc_device *tcpc, uint8_t typec_role, bool postpone);

extern int tcpm_typec_error_recovery(struct tcpc_device *tcpc);

extern int tcpm_typec_disable_function(
	struct tcpc_device *tcpc, bool disable);

#ifdef CONFIG_USB_POWER_DELIVERY

extern bool tcpm_inquire_pd_connected(
	struct tcpc_device *tcpc);

extern bool tcpm_inquire_pd_prev_connected(
	struct tcpc_device *tcpc);

extern uint8_t tcpm_inquire_pd_data_role(
	struct tcpc_device *tcpc);

extern uint8_t tcpm_inquire_pd_power_role(
	struct tcpc_device *tcpc);

extern uint8_t tcpm_inquire_pd_state_curr(
	struct tcpc_device *tcpc);

extern uint8_t tcpm_inquire_pd_vconn_role(
	struct tcpc_device *tcpc);

extern uint8_t tcpm_inquire_pd_pe_ready(
	struct tcpc_device *tcpc);

extern uint8_t tcpm_inquire_cable_current(
	struct tcpc_device *tcpc);

extern uint32_t tcpm_inquire_dpm_flags(
	struct tcpc_device *tcpc);

extern uint32_t tcpm_inquire_dpm_caps(
	struct tcpc_device *tcpc);

extern void tcpm_set_dpm_caps(
	struct tcpc_device *tcpc, uint32_t caps);

/* Request TCPM to send PD Request */

extern int tcpm_put_tcp_dpm_event(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event);

/* TCPM DPM PD I/F */

extern int tcpm_inquire_pd_contract(
	struct tcpc_device *tcpc, int *mv, int *ma);
extern int tcpm_inquire_cable_inform(
	struct tcpc_device *tcpc, uint32_t *vdos);
extern int tcpm_inquire_pd_partner_inform(
	struct tcpc_device *tcpc, uint32_t *vdos);
extern int tcpm_inquire_pd_partner_svids(
	struct tcpc_device *tcpc, struct tcpm_svid_list *list);
extern int tcpm_inquire_pd_partner_modes(
	struct tcpc_device *tcpc, uint16_t svid, struct tcpm_mode_list *list);
extern int tcpm_inquire_pd_source_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap *cap);
extern int tcpm_inquire_pd_sink_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap *cap);

extern bool tcpm_extract_power_cap_val(
	uint32_t pdo, struct tcpm_power_cap_val *cap);

extern bool tcpm_extract_power_cap_list(
	struct tcpm_power_cap *cap, struct tcpm_power_cap_list *cap_list);

extern int tcpm_get_remote_power_cap(struct tcpc_device *tcpc,
	struct tcpm_remote_power_cap *cap);

extern int tcpm_inquire_select_source_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap_val *cap_val);

/* Request TCPM to send PD Request */

extern int tcpm_dpm_pd_power_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_pd_data_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_pd_vconn_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_pd_goto_min(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_pd_soft_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_pd_get_source_cap(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_pd_get_sink_cap(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_pd_request(struct tcpc_device *tcpc,
	int mv, int ma, const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_pd_request_ex(struct tcpc_device *tcpc,
	uint8_t pos, uint32_t max, uint32_t oper,
	const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_pd_bist_cm2(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data);

#ifdef CONFIG_USB_PD_REV30
extern int tcpm_dpm_pd_get_source_cap_ext(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data,
	struct pd_source_cap_ext *src_cap_ext);
extern int tcpm_dpm_pd_fast_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_pd_get_status(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data, struct pd_status *status);
extern int tcpm_dpm_pd_get_pps_status_raw(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_pps_status_raw *pps_status);
extern int tcpm_dpm_pd_get_pps_status(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data,
	struct pd_pps_status *pps_status);
extern int tcpm_dpm_pd_get_country_code(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data,
	struct pd_country_codes *ccdb);
extern int tcpm_dpm_pd_get_country_info(struct tcpc_device *tcpc,
	uint32_t ccdo, const struct tcp_dpm_event_cb_data *data,
	struct pd_country_info *cidb);
extern int tcpm_dpm_pd_get_bat_cap(struct tcpc_device *tcpc,
	struct pd_get_battery_capabilities *gbcdb,
	const struct tcp_dpm_event_cb_data *data,
	struct pd_battery_capabilities *bcdb);
extern int tcpm_dpm_pd_get_bat_status(struct tcpc_device *tcpc,
	struct pd_get_battery_status *gbsdb,
	const struct tcp_dpm_event_cb_data *data, uint32_t *bsdo);
extern int tcpm_dpm_pd_get_mfrs_info(struct tcpc_device *tcpc,
	struct pd_get_manufacturer_info *gmidb,
	const struct tcp_dpm_event_cb_data *data,
	struct pd_manufacturer_info *midb);
extern int tcpm_dpm_pd_alert(struct tcpc_device *tcpc,
	uint32_t ado, const struct tcp_dpm_event_cb_data *data);
#endif	/* CONFIG_USB_PD_REV30 */

extern int tcpm_dpm_pd_hard_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_pd_error_recovery(struct tcpc_device *tcpc);

/* Request TCPM to send SOP' request */

extern int tcpm_dpm_pd_cable_soft_reset(
	struct tcpc_device *tcpc, const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_vdm_discover_cable(
	struct tcpc_device *tcpc, const struct tcp_dpm_event_cb_data *data);

/* Request TCPM to send VDM request */

extern int tcpm_dpm_vdm_discover_id(
	struct tcpc_device *tcpc, const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_vdm_discover_svid(
	struct tcpc_device *tcpc, const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_vdm_discover_mode(struct tcpc_device *tcpc,
	uint16_t svid, const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_vdm_enter_mode(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops, const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_vdm_exit_mode(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops, const struct tcp_dpm_event_cb_data *data);
extern int tcpm_dpm_vdm_attention(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops, const struct tcp_dpm_event_cb_data *data);

/* Request TCPM to send DP Request */

#ifdef CONFIG_USB_PD_ALT_MODE

extern int tcpm_inquire_dp_ufp_u_state(
	struct tcpc_device *tcpc, uint8_t *state);

extern int tcpm_dpm_dp_attention(struct tcpc_device *tcpc,
	uint32_t dp_status, uint32_t mask,
	const struct tcp_dpm_event_cb_data *data);

#ifdef CONFIG_USB_PD_ALT_MODE_DFP

extern int tcpm_inquire_dp_dfp_u_state(
	struct tcpc_device *tcpc, uint8_t *state);

extern int tcpm_dpm_dp_status_update(struct tcpc_device *tcpc,
	uint32_t dp_status, uint32_t mask,
	const struct tcp_dpm_event_cb_data *data);

extern int tcpm_dpm_dp_config(struct tcpc_device *tcpc,
	uint32_t dp_config, uint32_t mask,
	const struct tcp_dpm_event_cb_data *data);
#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */
#endif	/* CONFIG_USB_PD_ALT_MODE */

/* Request TCPM to send PD-UVDM Request */

#ifdef CONFIG_USB_PD_CUSTOM_VDM

extern int tcpm_dpm_send_custom_vdm(
	struct tcpc_device *tcpc,
	struct tcp_dpm_custom_vdm_data *vdm_data,
	const struct tcp_dpm_event_cb_data *cb_data);

#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

/* Notify TCPM */

extern int tcpm_notify_vbus_stable(struct tcpc_device *tcpc);

/* Charging Policy: Select PDO */

extern int tcpm_reset_pd_charging_policy(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data);

extern int tcpm_set_pd_charging_policy(struct tcpc_device *tcpc,
	uint8_t policy, const struct tcp_dpm_event_cb_data *data);

extern int tcpm_set_pd_charging_policy_default(
	struct tcpc_device *tcpc, uint8_t policy);

extern uint8_t tcpm_inquire_pd_charging_policy(struct tcpc_device *tcpc);
extern uint8_t tcpm_inquire_pd_charging_policy_default(
	struct tcpc_device *tcpc);

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
extern int tcpm_set_direct_charge_en(struct tcpc_device *tcpc, bool en);
extern bool tcpm_inquire_during_direct_charge(struct tcpc_device *tcpc);
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */


#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
extern int tcpm_dpm_set_vconn_supply_mode(
	struct tcpc_device *tcpc, uint8_t mode);
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */


#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_PPS_SINK
extern int tcpm_set_apdo_charging_policy(
	struct tcpc_device *tcpc, uint8_t policy, int mv, int ma,
	const struct tcp_dpm_event_cb_data *data);
extern int tcpm_inquire_pd_source_apdo(struct tcpc_device *tcpc,
	uint8_t apdo_type, uint8_t *cap_i, struct tcpm_power_cap_val *cap);
extern bool tcpm_inquire_during_pps_charge(struct tcpc_device *tcpc);
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

#ifdef CONFIG_USB_PD_REV30_BAT_INFO

/**
 * tcpm_update_bat_status
 *
 * Update current capacity and charging status of the specified battery
 *
 * If the battery's real capacity in not known,
 *	all batteries's capacity can be updated with SoC.
 *
 * If the battery status is only update when a noticiation is received,
 *	using no_mutex version, otherwise it will cause deadlock.
 *
 * This function may trigger DPM to send alert message
 *
 * @ ref : Specifies which battery to update
 * @ status : refer to BSDO_BAT_INFO, idle, charging, discharging
 * @ wh : current capacity, unit 1/10 wh
 * @ soc : current soc, unit: 0.1 %
 *
 */

extern int tcpm_update_bat_status_wh(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t wh);

extern int tcpm_update_bat_status_wh_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t wh);

extern int tcpm_update_bat_status_soc(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t soc);

extern int tcpm_update_bat_status_soc_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t soc);

/**
 * tcpm_update_bat_last_full
 *
 * Update last full capacity of the specified battery
 *
 * If the battery status is only update when a noticiation is received,
 *	using no_mutex version, otherwise it will cause deadlock.
 *
 * @ ref : Specifies which battery to update
 * @ wh : current capacity, unit 1/10 wh
 *
 */

extern int tcpm_update_bat_last_full(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint16_t wh);

extern int tcpm_update_bat_last_full_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint16_t wh);

#endif	/* CONFIG_USB_PD_REV30_BAT_INFO */


#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL

/**
 * tcpm_update_pd_status
 *
 * Update local status: temperature, power input,  and OT/OC/OV event.
 *
 * This function may trigger DPM to send alert message
 *
 * @ ptf :
 *	Present Temperature Flag
 * @ temperature :
 *	0 = feature not supported
 *	1 = temperature is less than 2 degree c.
 *	2-255 = temperature in degree c.
 *
 * @ input/input_mask :
 *	refer to pd_status.present_input
 *
 * @ bat_in / bat_in_mask :
 *	refer to pd_status.present_battey_input
 *	This function will auto_update INT_POWER_BAT
 *
 * @ evt :
 *	refer to pd_status.event_flags
 */

extern int tcpm_update_pd_status_temp(struct tcpc_device *tcpc,
	enum pd_present_temperature_flag ptf, uint8_t temperature);

extern int tcpm_update_pd_status_input(
	struct tcpc_device *tcpc, uint8_t input, uint8_t mask);

extern int tcpm_update_pd_status_bat_input(
	struct tcpc_device *tcpc, uint8_t bat_input, uint8_t bat_mask);

extern int tcpm_update_pd_status_event(
	struct tcpc_device *tcpc, uint8_t evt);

#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */


#endif	/* CONFIG_USB_PD_REV30 */

#endif	/* CONFIG_USB_POWER_DELIVERY */
#endif	/* CONFIG_TCPC_CLASS */

/* Empty function if configuration not defined */

#define TCPC_CLASS_NA
#define USB_POWER_DELIVERY_NA
#define USB_PD_REV30_NA
#define USB_PD_ALT_MODE_NA
#define USB_PD_ALT_MODE_DFP_NA
#define USB_PD_CUSTOM_VDM_NA
#define USB_PD_DIRECT_CHARGE_NA
#define TCPC_VCONN_SUPPLY_MODE_NA
#define USB_PD_REV30_PPS_SINK_NA
#define USB_PD_REV30_BAT_INFO_NA
#define USB_PD_REV30_STATUS_NA

#ifdef CONFIG_TCPC_CLASS
#undef TCPC_CLASS_NA

#ifdef CONFIG_USB_POWER_DELIVERY
#undef USB_POWER_DELIVERY_NA

#ifdef CONFIG_USB_PD_REV30
#undef USB_PD_REV30_NA
#ifdef CONFIG_USB_PD_REV30_PPS_SINK
#undef USB_PD_REV30_PPS_SINK_NA
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
#ifdef CONFIG_USB_PD_REV30_BAT_INFO
#undef USB_PD_REV30_BAT_INFO_NA
#endif	/* CONFIG_USB_PD_REV30_BAT_INFO */
#ifdef CONFIG_USB_PD_REV30_STATUS_LOCAL
#undef USB_PD_REV30_STATUS_NA
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */
#endif	/* CONFIG_USB_PD_REV30 */

#ifdef CONFIG_USB_PD_ALT_MODE
#undef USB_PD_ALT_MODE_NA
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
#undef USB_PD_ALT_MODE_DFP_NA
#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */
#endif	/* CONFIG_USB_PD_ALT_MODE */

#ifdef CONFIG_USB_PD_CUSTOM_VDM
#undef USB_PD_CUSTOM_VDM_NA
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
#undef USB_PD_DIRECT_CHARGE_NA
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
#undef TCPC_VCONN_SUPPLY_MODE_NA
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

#endif	/* CONFIG_USB_POWER_DELIVERY */
#endif	/* CONFIG_TCPC_CLASS */

#ifdef TCPC_CLASS_NA

static inline struct tcpc_device
		*tcpc_dev_get_by_name(const char *name)
{
	return NULL;
}

static inline int register_tcp_dev_notifier(struct tcpc_device *tcp_dev,
				struct notifier_block *nb, uint8_t flags)
{
	return -ENODEV;
}

static inline int unregister_tcp_dev_notifier(struct tcpc_device *tcp_dev,
				struct notifier_block *nb, uint8_t flags)
{
	return -ENODEV;
}

static inline int tcpm_shutdown(struct tcpc_device *tcpc)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_remote_cc(struct tcpc_device *tcpc,
	uint8_t *cc1, uint8_t *cc2, bool from_ic)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_vbus_level(
	struct tcpc_device *tcpc, bool from_ic)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_typec_remote_rp_curr(
	struct tcpc_device *tcpc)
{
	return 0;
}

static inline bool tcpm_inquire_cc_polarity(struct tcpc_device *tcpc)
{
	return false;
}

static inline uint8_t tcpm_inquire_typec_attach_state(
				struct tcpc_device *tcpc)
{
	return TYPEC_UNATTACHED;
}

static inline uint8_t tcpm_inquire_typec_role(struct tcpc_device *tcpc)
{
	return TYPEC_ROLE_UNKNOWN;
}

static inline uint8_t tcpm_inquire_typec_local_rp(struct tcpc_device *tcpc)
{
	return 0;
}

static inline int tcpm_typec_set_wake_lock(
	struct tcpc_device *tcpc, bool user_lock)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_typec_set_usb_sink_curr(
	struct tcpc_device *tcpc, int curr)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_typec_set_rp_level(
	struct tcpc_device *tcpc, uint8_t level)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_typec_set_custom_hv(
	struct tcpc_device *tcpc, bool en)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_typec_role_swap(
	struct tcpc_device *tcpc)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_typec_change_role(
	struct tcpc_device *tcpc, uint8_t typec_role)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_typec_change_role_postpone(
	struct tcpc_device *tcpc, uint8_t typec_role, bool postpone)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_typec_error_recovery(struct tcpc_device *tcpc)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}
#endif	/* TCPC_CLASS_NA */

#ifdef USB_POWER_DELIVERY_NA

static inline bool tcpm_inquire_pd_connected(
	struct tcpc_device *tcpc)
{
	return false;
}

static inline bool tcpm_inquire_pd_prev_connected(
	struct tcpc_device *tcpc)
{
	return false;
}

static inline uint8_t tcpm_inquire_pd_data_role(
	struct tcpc_device *tcpc)
{
	return 0;
}

static inline uint8_t tcpm_inquire_pd_power_role(
	struct tcpc_device *tcpc)
{
	return 0;
}

static inline uint8_t tcpm_inquire_pd_vconn_role(
	struct tcpc_device *tcpc)
{
	return 0;
}

static inline uint8_t tcpm_inquire_pd_pe_ready(
	struct tcpc_device *tcpc)
{
	return 0;
}

static inline uint8_t tcpm_inquire_cable_current(
	struct tcpc_device *tcpc)
{
	return PD_CABLE_CURR_UNKNOWN;
}

static inline uint32_t tcpm_inquire_dpm_flags(
	struct tcpc_device *tcpc)
{
	return 0;
}

static inline uint32_t tcpm_inquire_dpm_caps(
	struct tcpc_device *tcpc)
{
	return 0;
}

static inline void tcpm_set_dpm_caps(
	struct tcpc_device *tcpc, uint32_t caps)
{
}

static inline int tcpm_put_tcp_dpm_event(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_pd_contract(
	struct tcpc_device *tcpc, int *mv, int *ma)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_cable_inform(
	struct tcpc_device *tcpc, uint32_t *vdos)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_pd_partner_inform(
	struct tcpc_device *tcpc, uint32_t *vdos)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_pd_partner_svids(
	struct tcpc_device *tcpc, struct tcpm_svid_list *list)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_pd_partner_modes(
	struct tcpc_device *tcpc, uint16_t svid, struct tcpm_mode_list *list)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_pd_source_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap *cap)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_pd_sink_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap *cap)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline bool tcpm_extract_power_cap_val(
	uint32_t pdo, struct tcpm_power_cap_val *cap)
{
	return false;
}

static inline bool tcpm_extract_power_cap_list(
	struct tcpm_power_cap *cap, struct tcpm_power_cap_list *cap_list)
{
	return false;
}

static inline int tcpm_get_remote_power_cap(struct tcpc_device *tcpc,
	struct tcpm_remote_power_cap *cap)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_select_source_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap_val *cap_val)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_power_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_data_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_vconn_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_goto_min(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_soft_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_get_source_cap(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_get_sink_cap(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_request(struct tcpc_device *tcpc,
	int mv, int ma, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_request_ex(struct tcpc_device *tcpc,
	uint8_t pos, uint32_t max, uint32_t oper,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_bist_cm2(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}
#endif	/* USB_POWER_DELIVERY_NA */

#ifdef USB_PD_REV30_NA
static inline int tcpm_dpm_pd_get_source_cap_ext(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data,
	struct pd_source_cap_ext *src_cap_ext)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_fast_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_get_status(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data, struct pd_status *status)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_get_pps_status(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data,
	struct pd_pps_status *pps_status)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_get_country_code(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data,
	struct pd_country_codes *ccdb)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_get_country_info(struct tcpc_device *tcpc,
	uint32_t ccdo, const struct tcp_dpm_event_cb_data *data,
	struct pd_country_info *cidb)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_get_bat_cap(struct tcpc_device *tcpc,
	struct pd_get_battery_capabilities *gbcdb,
	const struct tcp_dpm_event_cb_data *data,
	struct pd_battery_capabilities *bcdb)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_get_bat_status(struct tcpc_device *tcpc,
	struct pd_get_battery_status *gbsdb,
	const struct tcp_dpm_event_cb_data *data, uint32_t *bsdo)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_get_mfrs_info(struct tcpc_device *tcpc,
	struct pd_get_manufacturer_info *gmidb,
	const struct tcp_dpm_event_cb_data *data,
	struct pd_manufacturer_info *midb)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_alert(struct tcpc_device *tcpc,
	uint32_t ado, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}
#endif	/* USB_PD_REV30_NA */

#ifdef USB_POWER_DELIVERY_NA

static inline int tcpm_dpm_pd_hard_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_pd_error_recovery(struct tcpc_device *tcpc)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

/* Request TCPM to send SOP' request */

static inline int tcpm_dpm_pd_cable_soft_reset(
	struct tcpc_device *tcpc, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_vdm_discover_cable(
	struct tcpc_device *tcpc, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

/* Request TCPM to send VDM request */

static inline int tcpm_dpm_vdm_discover_id(
	struct tcpc_device *tcpc, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_vdm_discover_svid(
	struct tcpc_device *tcpc, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_vdm_discover_mode(struct tcpc_device *tcpc,
	uint16_t svid, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_vdm_enter_mode(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_vdm_attention(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_vdm_exit_mode(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}
#endif	/* USB_POWER_DELIVERY_NA */

#ifdef USB_PD_ALT_MODE_NA
static inline int tcpm_inquire_dp_ufp_u_state(
	struct tcpc_device *tcpc, uint8_t *state)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_dp_attention(struct tcpc_device *tcpc,
	uint32_t dp_status, uint32_t mask,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}
#endif	/* USB_PD_ALT_MODE_NA */

#ifdef USB_PD_ALT_MODE_DFP_NA
static inline int tcpm_inquire_dp_dfp_u_state(
	struct tcpc_device *tcpc, uint8_t *state)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_dp_status_update(struct tcpc_device *tcpc,
	uint32_t dp_status, uint32_t mask,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_dpm_dp_config(struct tcpc_device *tcpc,
	uint32_t dp_config, uint32_t mask,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}
#endif	/* USB_PD_ALT_MODE_DFP_NA */

#ifdef USB_PD_CUSTOM_VDM_NA
static inline int tcpm_dpm_send_custom_vdm(
	struct tcpc_device *tcpc,
	struct tcp_dpm_custom_vdm_data *vdm_data,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}
#endif	/* USB_PD_CUSTOM_VDM_NA */

#ifdef USB_POWER_DELIVERY_NA
static inline int tcpm_notify_vbus_stable(struct tcpc_device *tcpc)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_reset_pd_charging_policy(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_set_pd_charging_policy(struct tcpc_device *tcpc,
	uint8_t policy, const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_set_pd_charging_policy_default(
	struct tcpc_device *tcpc, uint8_t policy)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline uint8_t tcpm_inquire_pd_charging_policy(
	struct tcpc_device *tcpc)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline uint8_t tcpm_inquire_pd_charging_policy_default(
	struct tcpc_device *tcpc)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}
#endif	/* USB_POWER_DELIVERY_NA */

#ifdef USB_PD_DIRECT_CHARGE_NA
static inline int tcpm_set_direct_charge_en(
	struct tcpc_device *tcpc, bool en)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline bool tcpm_inquire_during_direct_charge(
	struct tcpc_device *tcpc)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}
#endif	/* USB_PD_DIRECT_CHARGE_NA */

#ifdef TCPC_VCONN_SUPPLY_MODE_NA
static inline int tcpm_dpm_set_vconn_supply_mode(
	struct tcpc_device *tcpc, uint8_t mode)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}
#endif	/* TCPC_VCONN_SUPPLY_MODE_NA */

#ifdef USB_PD_REV30_PPS_SINK_NA
static inline int tcpm_set_apdo_charging_policy(
	struct tcpc_device *tcpc, uint8_t policy, int mv, int ma,
	const struct tcp_dpm_event_cb_data *data)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_inquire_pd_source_apdo(struct tcpc_device *tcpc,
	uint8_t apdo_type, uint8_t *cap_i, struct tcpm_power_cap_val *cap)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline bool tcpm_inquire_during_pps_charge(struct tcpc_device *tcpc)
{
	return false;
}

#endif	/* USB_PD_REV30_PPS_SINK_NA */

#ifdef USB_PD_REV30_BAT_INFO_NA

static inline int tcpm_update_bat_status_wh(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t wh)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_update_bat_status_wh_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t wh)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_update_bat_status_soc(struct tcpc_device *tcpc,
	uint8_t status, uint16_t soc)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_update_bat_status_soc_no_mutex(struct tcpc_device *tcpc,
	uint8_t status, uint16_t soc)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_update_bat_last_full(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint16_t wh)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_update_bat_last_full_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint16_t wh)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

#endif	/* USB_PD_REV30_BAT_INFO_NA */


#ifdef USB_PD_REV30_STATUS_NA

static inline int tcpm_update_pd_status_temp(struct tcpc_device *tcpc,
	enum pd_present_temperature_flag ptf, uint8_t temperature)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_update_pd_status_input(
	struct tcpc_device *tcpc, uint8_t input, uint8_t mask)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_update_pd_status_bat_input(
	struct tcpc_device *tcpc, uint8_t bat_input, uint8_t bat_mask)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

static inline int tcpm_update_pd_status_event(
	struct tcpc_device *tcpc, uint8_t evt)
{
	return TCPM_ERROR_NO_IMPLEMENT;
}

#endif	/* USB_PD_REV30_STATUS_NA */

#undef TCPC_CLASS_NA
#undef USB_POWER_DELIVERY_NA
#undef USB_PD_REV30_NA
#undef USB_PD_ALT_MODE_NA
#undef USB_PD_ALT_MODE_DFP_NA
#undef USB_PD_CUSTOM_VDM_NA
#undef USB_PD_DIRECT_CHARGE_NA
#undef TCPC_VCONN_SUPPLY_MODE_NA
#undef USB_PD_REV30_PPS_SINK_NA
#undef USB_PD_REV30_BAT_INFO_NA
#undef USB_PD_REV30_STATUS_NA

#endif /* TCPM_H_ */
