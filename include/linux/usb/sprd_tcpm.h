/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * sprd_tcpm.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#ifndef __LINUX_USB_SPRD_TCPM_H
#define __LINUX_USB_SPRD_TCPM_H

#include <linux/kthread.h>
#include <linux/power_supply.h>
#include <linux/usb.h>
#include <linux/usb/sprd_pd.h>
#include <linux/usb/sprd_pd_ado.h>
#include <linux/usb/sprd_pd_bdo.h>
#include <linux/usb/sprd_pd_ext_sdb.h>
#include <linux/usb/sprd_pd_vdo.h>
#include <linux/usb/role.h>
#include <linux/usb/typec_altmode.h>

#include <linux/bitops.h>
#include <linux/usb/typec.h>
#include "sprd_pd.h"

enum sprd_typec_cc_status {
	SPRD_TYPEC_CC_OPEN,
	SPRD_TYPEC_CC_RA,
	SPRD_TYPEC_CC_RD,
	SPRD_TYPEC_CC_RP_DEF,
	SPRD_TYPEC_CC_RP_1_5,
	SPRD_TYPEC_CC_RP_3_0,
};

enum sprd_typec_cc_polarity {
	SPRD_TYPEC_POLARITY_CC1,
	SPRD_TYPEC_POLARITY_CC2,
};

/* Time to wait for TCPC to complete transmit */
#define SPRD_PD_T_TCPC_TX_TIMEOUT		200		/* in ms */
#define SPRD_PD_T_TCPC_TX_MIN_TIMEOUT		5		/* in ms */
#define SPRD_PD_ROLE_SWAP_TIMEOUT		(MSEC_PER_SEC * 10)
#define SPRD_PD_CTRL_TIMEOUT			(MSEC_PER_SEC * 3)
#define SPRD_PD_PPS_CTRL_TIMEOUT		(MSEC_PER_SEC * 10)


/*
 * The SPRD_PPS_5V_PROG_MAX reference value is derived from
 * section 10.2.3.2 of the PD3.0 spec.
 */
#define SPRD_PPS_5V_PROG_MAX			6200

enum sprd_tcpm_transmit_status {
	SPRD_TCPC_TX_SUCCESS = 0,
	SPRD_TCPC_TX_DISCARDED = 1,
	SPRD_TCPC_TX_FAILED = 2,
};

enum sprd_tcpm_transmit_type {
	SPRD_TCPC_TX_SOP = 0,
	SPRD_TCPC_TX_SOP_PRIME = 1,
	SPRD_TCPC_TX_SOP_PRIME_PRIME = 2,
	SPRD_TCPC_TX_SOP_DEBUG_PRIME = 3,
	SPRD_TCPC_TX_SOP_DEBUG_PRIME_PRIME = 4,
	SPRD_TCPC_TX_HARD_RESET = 5,
	SPRD_TCPC_TX_CABLE_RESET = 6,
	SPRD_TCPC_TX_BIST_MODE_2 = 7
};

/**
 * struct tcpc_config - Port configuration
 * @src_pdo:	PDO parameters sent to port partner as response to
 *		PD_CTRL_GET_SOURCE_CAP message
 * @nr_src_pdo:	Number of entries in @src_pdo
 * @snk_pdo:	PDO parameters sent to partner as response to
 *		PD_CTRL_GET_SINK_CAP message
 * @nr_snk_pdo:	Number of entries in @snk_pdo
 * @operating_snk_mw:
 *		Required operating sink power in mW
 * @type:	Port type (TYPEC_PORT_DFP, TYPEC_PORT_UFP, or
 *		TYPEC_PORT_DRP)
 * @default_role:
 *		Default port role (TYPEC_SINK or TYPEC_SOURCE).
 *		Set to TYPEC_NO_PREFERRED_ROLE if no default role.
 * @try_role_hw:True if try.{Src,Snk} is implemented in hardware
 * @alt_modes:	List of supported alternate modes
 */
struct tcpc_config {
	const u32 *src_pdo;
	unsigned int nr_src_pdo;

	const u32 *snk_pdo;
	unsigned int nr_snk_pdo;

	const u32 *snk_vdo;
	unsigned int nr_snk_vdo;

	unsigned int operating_snk_mw;

	enum typec_port_type type;
	enum typec_port_data data;
	enum typec_role default_role;
	bool try_role_hw;	/* try.{src,snk} implemented in hardware */
	bool self_powered;	/* port belongs to a self powered device */

	const struct typec_altmode_desc *alt_modes;
	unsigned int nr_alt_modes;
};

/* Mux state attributes */
#define SPRD_TCPC_MUX_USB_ENABLED		BIT(0)	/* USB enabled */
#define SPRD_TCPC_MUX_DP_ENABLED		BIT(1)	/* DP enabled */
#define SPRD_TCPC_MUX_POLARITY_INVERTED		BIT(2)	/* Polarity inverted */

/**
 * struct tcpc_dev - Port configuration and callback functions
 * @config:	Pointer to port configuration
 * @fwnode:	Pointer to port fwnode
 * @get_vbus:	Called to read current VBUS state
 * @get_current_limit:
 *		Optional; called by the tcpm core when configured as a snk
 *		and cc=Rp-def. This allows the tcpm to provide a fallback
 *		current-limit detection method for the cc=Rp-def case.
 *		For example, some tcpcs may include BC1.2 charger detection
 *		and use that in this case.
 * @set_cc:	Called to set value of CC pins
 * @get_cc:	Called to read current CC pin values
 * @set_polarity:
 *		Called to set polarity
 * @set_vconn:	Called to enable or disable VCONN
 * @set_vbus:	Called to enable or disable VBUS
 * @set_current_limit:
 *		Optional; called to set current limit as negotiated
 *		with partner.
 * @set_pd_rx:	Called to enable or disable reception of PD messages
 * @set_roles:	Called to set power and data roles
 * @start_toggling:
 *		Optional; if supported by hardware, called to start dual-role
 *		toggling or single-role connection detection. Toggling stops
 *		automatically if a connection is established.
 * @try_role:	Optional; called to set a preferred role
 * @pd_transmit:Called to transmit PD message
 * @dp_altmode_notify:Called to notify dp altmode hotplug message
 * @mux:	Pointer to multiplexer data
 */
struct tcpc_dev {
	const struct tcpc_config *config;
	struct fwnode_handle *fwnode;

	int (*init)(struct tcpc_dev *dev);
	int (*get_vbus)(struct tcpc_dev *dev);
	int (*get_current_limit)(struct tcpc_dev *dev);
	int (*set_cc)(struct tcpc_dev *dev, enum sprd_typec_cc_status cc);
	int (*get_cc)(struct tcpc_dev *dev, enum sprd_typec_cc_status *cc1,
		      enum sprd_typec_cc_status *cc2);
	int (*force_swich_rp_rd)(struct tcpc_dev *dev, enum typec_role pwr_role);
	int (*set_typec_role)(struct tcpc_dev *tcpc,
			      enum typec_port_type role,
			      enum typec_data_role data);
	int (*set_polarity)(struct tcpc_dev *dev,
			    enum sprd_typec_cc_polarity polarity);
	int (*set_vconn)(struct tcpc_dev *dev, bool on);
	int (*set_vbus)(struct tcpc_dev *dev, bool on, bool charge);
	int (*set_current_limit)(struct tcpc_dev *dev, u32 max_ma, u32 mv);
	int (*set_pd_rx)(struct tcpc_dev *dev, bool on);
	int (*set_roles)(struct tcpc_dev *dev, bool attached,
			 enum typec_role role, enum typec_data_role data);
	int (*start_toggling)(struct tcpc_dev *dev,
			      enum typec_port_type port_type,
			      enum sprd_typec_cc_status cc);
	int (*try_role)(struct tcpc_dev *dev, int role);
	int (*pd_transmit)(struct tcpc_dev *dev, enum sprd_tcpm_transmit_type type,
			   const struct sprd_pd_message *msg);
	int (*dp_altmode_notify)(struct tcpc_dev *dev, u32 vdo);
	int (*reset_pd_rx_id)(struct tcpc_dev *dev);
	int (*set_pd_tx_id)(struct tcpc_dev *dev, unsigned int tx_id);
	int (*check_tx_goodcrc)(struct tcpc_dev *dev, bool check_tx_goodcrc);
	int (*enable_tx_auto_retry)(struct tcpc_dev *dev, bool enable_auto_retry);
};

struct sprd_typec_device_ops {
	const char *name;

	int (*set_typec_int_clear)(void);
	int (*set_typec_int_disable)(void);
	int (*set_typec_int_enable)(void);
	int (*typec_set_pd_dr_swap_flag)(u8 flag);
	int (*typec_set_pr_swap_flag)(u8 flag);
	int (*typec_set_pd_swap_event)(u8 pd_swap_flag);
	int (*set_typec_rp_rd)(enum sprd_typec_cc_status cc);
	int (*set_typec_rp_level)(enum sprd_typec_cc_status cc);
	void (*set_support_accessory_mode)(struct typec_port *port);
	int (*typec_pr_swap_no_chk_detach)(bool on);
	void (*set_typec_err_recovery_enter)(void);
	void (*typec_notify_sink_ready_state)(void);
};

struct sprd_charger_ops {
	const char *name;

	void (*update_ac_usb_online)(bool is_pd_hub);
	void (*negotiated_limit_current)(enum sprd_pd_pdo_type pdo_type,
					 int req_vol_uv,
					 int req_cur_ua,
					 bool enable_limit);
	void (*set_rp_limit_current)(int rp_limit);
};

enum sprd_tcpm_typec_pd_swap {
	TCPM_TYPEC_NO_SWAP,
	TCPM_TYPEC_SOURCE_TO_SINK,
	TCPM_TYPEC_SINK_TO_SOURCE,
	TCPM_TYPEC_HOST_TO_DEVICE,
	TCPM_TYPEC_DEVICE_TO_HOST,
};

struct adapter_power_cap {
	uint8_t type[SPRD_PDO_MAX_OBJECTS];
	int max_mv[SPRD_PDO_MAX_OBJECTS];
	int min_mv[SPRD_PDO_MAX_OBJECTS];
	int ma[SPRD_PDO_MAX_OBJECTS];
	int pwr_mw_limit[SPRD_PDO_MAX_OBJECTS];
	uint8_t nr_source_caps;
};

enum sprd_tcpm_state {
	INVALID_STATE = 0,
	TOGGLING,
	SRC_UNATTACHED,
	SRC_ATTACH_WAIT,
	SRC_ATTACHED,
	SRC_STARTUP = 5,
	SRC_SEND_CAPABILITIES,
	SRC_SEND_CAPABILITIES_TIMEOUT,
	SRC_NEGOTIATE_CAPABILITIES,
	SRC_TRANSITION_SUPPLY,
	SRC_READY = 10,
	SRC_WAIT_NEW_CAPABILITIES,

	SNK_UNATTACHED,
	SNK_ATTACH_WAIT,
	SNK_DEBOUNCED,
	SNK_ATTACHED = 15,
	SNK_STARTUP,
	SNK_DISCOVERY,
	SNK_DISCOVERY_DEBOUNCE,
	SNK_DISCOVERY_DEBOUNCE_DONE,
	SNK_WAIT_CAPABILITIES = 20,
	SNK_NEGOTIATE_CAPABILITIES,
	SNK_NEGOTIATE_PPS_CAPABILITIES,
	SNK_TRANSITION_SINK,
	SNK_TRANSITION_SINK_VBUS,
	SNK_READY = 25,

	ACC_UNATTACHED,
	DEBUG_ACC_ATTACHED,
	AUDIO_ACC_ATTACHED,
	AUDIO_ACC_DEBOUNCE,

	HARD_RESET_SEND = 30,
	HARD_RESET_START,
	SRC_HARD_RESET_VBUS_OFF,
	SRC_HARD_RESET_VBUS_ON,
	SNK_HARD_RESET_SINK_OFF,
	SNK_HARD_RESET_WAIT_VBUS = 35,
	SNK_HARD_RESET_SINK_ON,

	SOFT_RESET,
	SOFT_RESET_SEND,

	DR_SWAP_ACCEPT,
	DR_SWAP_SEND = 40,
	DR_SWAP_SEND_TIMEOUT,
	DR_SWAP_CANCEL,
	DR_SWAP_CHANGE_DR,

	PR_SWAP_ACCEPT,
	PR_SWAP_SEND = 45,
	PR_SWAP_SEND_TIMEOUT,
	PR_SWAP_CANCEL,
	PR_SWAP_START,
	PR_SWAP_SRC_SNK_TRANSITION_OFF,
	PR_SWAP_SRC_SNK_SOURCE_OFF = 50,
	PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED,
	PR_SWAP_SRC_SNK_SINK_ON,
	PR_SWAP_SNK_SRC_SINK_OFF,
	PR_SWAP_SNK_SRC_SOURCE_ON,
	PR_SWAP_SNK_SRC_SOURCE_ON_VBUS_RAMPED_UP = 55,

	VCONN_SWAP_ACCEPT,
	VCONN_SWAP_SEND,
	VCONN_SWAP_SEND_TIMEOUT,
	VCONN_SWAP_CANCEL,
	VCONN_SWAP_START = 60,
	VCONN_SWAP_WAIT_FOR_VCONN,
	VCONN_SWAP_TURN_ON_VCONN,
	VCONN_SWAP_TURN_OFF_VCONN,

	SNK_TRY,
	SNK_TRY_WAIT = 65,
	SNK_TRY_WAIT_DEBOUNCE,
	SNK_TRY_WAIT_DEBOUNCE_CHECK_VBUS,
	SRC_TRYWAIT,
	SRC_TRYWAIT_DEBOUNCE,
	SRC_TRYWAIT_UNATTACHED = 70,

	SRC_TRY,
	SRC_TRY_WAIT,
	SRC_TRY_DEBOUNCE,
	SNK_TRYWAIT,
	SNK_TRYWAIT_DEBOUNCE = 75,
	SNK_TRYWAIT_VBUS,
	BIST_RX,

	GET_STATUS_SEND,
	GET_STATUS_SEND_TIMEOUT,
	GET_PPS_STATUS_SEND = 80,
	GET_PPS_STATUS_SEND_TIMEOUT,

	ERROR_RECOVERY,
	PORT_RESET,
	PORT_RESET_WAIT_OFF,
	CHUNK_NOT_SUPP,
};

static const char * const sprd_tcpm_states[] = {
	"INVALID_STATE",				/* = 0 */
	"TOGGLING",
	"SRC_UNATTACHED",
	"SRC_ATTACH_WAIT",
	"SRC_ATTACHED",
	"SRC_STARTUP",					/* = 5 */
	"SRC_SEND_CAPABILITIES",
	"SRC_SEND_CAPABILITIES_TIMEOUT",
	"SRC_NEGOTIATE_CAPABILITIES",
	"SRC_TRANSITION_SUPPLY",
	"SRC_READY",					/* = 10 */
	"SRC_WAIT_NEW_CAPABILITIES",

	"SNK_UNATTACHED",
	"SNK_ATTACH_WAIT",
	"SNK_DEBOUNCED",
	"SNK_ATTACHED",					/* = 15 */
	"SNK_STARTUP",
	"SNK_DISCOVERY",
	"SNK_DISCOVERY_DEBOUNCE",
	"SNK_DISCOVERY_DEBOUNCE_DONE",
	"SNK_WAIT_CAPABILITIES",			/* = 20 */
	"SNK_NEGOTIATE_CAPABILITIES",
	"SNK_NEGOTIATE_PPS_CAPABILITIES",
	"SNK_TRANSITION_SINK",
	"SNK_TRANSITION_SINK_VBUS",
	"SNK_READY",					/* = 25 */

	"ACC_UNATTACHED",
	"DEBUG_ACC_ATTACHED",
	"AUDIO_ACC_ATTACHED",
	"AUDIO_ACC_DEBOUNCE",

	"HARD_RESET_SEND",				/* = 30 */
	"HARD_RESET_START",
	"SRC_HARD_RESET_VBUS_OFF",
	"SRC_HARD_RESET_VBUS_ON",
	"SNK_HARD_RESET_SINK_OFF",
	"SNK_HARD_RESET_WAIT_VBUS",			/* = 35 */
	"SNK_HARD_RESET_SINK_ON",

	"SOFT_RESET",
	"SOFT_RESET_SEND",

	"DR_SWAP_ACCEPT",
	"DR_SWAP_SEND",					/* = 40 */
	"DR_SWAP_SEND_TIMEOUT",
	"DR_SWAP_CANCEL",
	"DR_SWAP_CHANGE_DR",

	"PR_SWAP_ACCEPT",
	"PR_SWAP_SEND",					/* = 45 */
	"PR_SWAP_SEND_TIMEOUT",
	"PR_SWAP_CANCEL",
	"PR_SWAP_START",
	"PR_SWAP_SRC_SNK_TRANSITION_OFF",
	"PR_SWAP_SRC_SNK_SOURCE_OFF",			/* = 50 */
	"PR_SWAP_SRC_SNK_SOURCE_OFF_CC_DEBOUNCED",
	"PR_SWAP_SRC_SNK_SINK_ON",
	"PR_SWAP_SNK_SRC_SINK_OFF",
	"PR_SWAP_SNK_SRC_SOURCE_ON",
	"PR_SWAP_SNK_SRC_SOURCE_ON_VBUS_RAMPED_UP",	/* = 55 */

	"VCONN_SWAP_ACCEPT",
	"VCONN_SWAP_SEND",
	"VCONN_SWAP_SEND_TIMEOUT",
	"VCONN_SWAP_CANCEL",
	"VCONN_SWAP_START",				/* = 60 */
	"VCONN_SWAP_WAIT_FOR_VCONN",
	"VCONN_SWAP_TURN_ON_VCONN",
	"VCONN_SWAP_TURN_OFF_VCONN",

	"SNK_TRY",
	"SNK_TRY_WAIT",					/* = 65 */
	"SNK_TRY_WAIT_DEBOUNCE",
	"SNK_TRY_WAIT_DEBOUNCE_CHECK_VBUS",
	"SRC_TRYWAIT",
	"SRC_TRYWAIT_DEBOUNCE",
	"SRC_TRYWAIT_UNATTACHED",			/* = 70 */

	"SRC_TRY",
	"SRC_TRY_WAIT",
	"SRC_TRY_DEBOUNCE",
	"SNK_TRYWAIT",
	"SNK_TRYWAIT_DEBOUNCE",				/* = 75 */
	"SNK_TRYWAIT_VBUS",
	"BIST_RX",

	"GET_STATUS_SEND",
	"GET_STATUS_SEND_TIMEOUT",
	"GET_PPS_STATUS_SEND",				/* = 80 */
	"GET_PPS_STATUS_SEND_TIMEOUT",

	"ERROR_RECOVERY",
	"PORT_RESET",
	"PORT_RESET_WAIT_OFF",
	"CHUNK_NOT_SUPP",
};

enum sprd_vdm_states {
	VDM_STATE_ERR_BUSY = -3,
	VDM_STATE_ERR_SEND = -2,
	VDM_STATE_ERR_TMOUT = -1,
	VDM_STATE_DONE = 0,
	/* Anything >0 represents an active state */
	VDM_STATE_READY = 1,
	VDM_STATE_BUSY = 2,
	VDM_STATE_WAIT_RSP_BUSY = 3,
};

enum sprd_pd_msg_request {
	PD_MSG_NONE = 0,
	PD_MSG_CTRL_REJECT,
	PD_MSG_CTRL_WAIT,
	PD_MSG_CTRL_NOT_SUPP,
	PD_MSG_DATA_SINK_CAP,
	PD_MSG_DATA_SOURCE_CAP,
	PD_MSG_CTRL_GET_REVISION,
};

enum sprd_pd_chunk_msg_request {
	PD_CHUNK_MSG_NONE = 0,
	PD_CHUNK_MSG_CTRL_GET_SINK_CAP_EXT,
	PD_CHUNK_MSG_EXT_GET_BATTERY_CAP_EXT,
};

/* Events from low level driver */

#define SPRD_TCPM_CC_EVENT		BIT(0)
#define SPRD_TCPM_VBUS_EVENT		BIT(1)
#define SPRD_TCPM_RESET_EVENT		BIT(2)
#define SPRD_TCPM_SOFT_RESET_EVENT	BIT(3)

#define SPRD_LOG_BUFFER_ENTRIES		2048
#define SPRD_LOG_BUFFER_ENTRY_SIZE	128

/* Google limit power transfer support */

#define SPRD_LIMIT_POWER_TRANSFER_MA	110

/* Alternate mode support */

#define SPRD_SVID_DISCOVERY_MAX		16
#define SPRD_ALTMODE_DISCOVERY_MAX	(SPRD_SVID_DISCOVERY_MAX * MODE_DISCOVERY_MAX)

enum usb_pd_svdm_ver_minor {
	SVDM_VER_MIMOR_0 = 0,
	SVDM_VER_MIMOR_1 = 1,
	SVDM_VER_MIMOR_MAX = SVDM_VER_MIMOR_1,
};

struct sprd_pd_mode_data {
	int svid_index;		/* current SVID index		*/
	int nsvids;
	u16 svids[SPRD_SVID_DISCOVERY_MAX];
	int altmodes;		/* number of alternate modes	*/
	struct typec_altmode_desc altmode_desc[SPRD_ALTMODE_DISCOVERY_MAX];
};

/*
 * @min_volt: Actual min voltage at the local port
 * @req_min_volt: Requested min voltage to the port partner
 * @max_volt: Actual max voltage at the local port
 * @req_max_volt: Requested max voltage to the port partner
 * @max_curr: Actual max current at the local port
 * @req_max_curr: Requested max current of the port partner
 * @req_out_volt: Requested output voltage to the port partner
 * @req_op_curr: Requested operating current to the port partner
 * @supported: Parter has at least one APDO hence supports PPS
 * @active: PPS mode is active
 */
struct sprd_pd_pps_data {
	u32 min_volt;
	u32 req_min_volt;
	u32 max_volt;
	u32 req_max_volt;
	u32 max_curr;
	u32 req_max_curr;
	u32 req_out_volt;
	u32 req_op_curr;
	bool supported;
	bool active;
};

struct sprd_tcpm_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_log_ctl;
	struct device_attribute attr_vbus_wait_ctl;
	struct device_attribute attr_tcc_debounce_ctl;
	struct device_attribute attr_first_pd_cap_delay_ctl;
	struct attribute *attrs[5];

	struct sprd_tcpm_port *port;
};

struct sprd_tcpm_port {
	struct device *dev;

	struct mutex lock;		/* tcpm state machine lock */
	struct kthread_worker tcpm_kworker;
	struct task_struct *tcpm_event_task;

	struct typec_capability typec_caps;
	struct typec_port *typec_port;

	struct tcpc_dev	*tcpc;
	struct usb_role_switch *role_sw;

	enum typec_role vconn_role;
	enum typec_role pwr_role;
	enum typec_data_role data_role;
	enum typec_pwr_opmode pwr_opmode;
	enum typec_orientation orientation;

	struct usb_pd_identity partner_ident;
	struct typec_partner_desc partner_desc;
	struct typec_partner *partner;

	enum sprd_typec_cc_status cc_req;

	enum sprd_typec_cc_status cc1;
	enum sprd_typec_cc_status cc2;
	enum sprd_typec_cc_polarity polarity;

	bool attached;
	bool connected;
	bool registered;
	enum typec_port_type port_type;
	bool vbus_present;
	bool vbus_never_low;
	bool vbus_source;
	bool vbus_charge;

	bool send_discover;
	bool op_vsafe5v;

	int try_role;
	int try_snk_count;
	int try_src_count;

	enum sprd_pd_msg_request queued_message;
	enum sprd_pd_chunk_msg_request queued_chunk_message;

	enum sprd_tcpm_state enter_state;
	enum sprd_tcpm_state prev_state;
	enum sprd_tcpm_state state;
	enum sprd_tcpm_state delayed_state;
	ktime_t delayed_runtime;
	unsigned long delay_ms;

	spinlock_t pd_event_lock;
	u32 pd_events;

	struct kthread_work event_work;
	struct hrtimer state_machine_timer;
	struct kthread_work state_machine;
	struct hrtimer vdm_state_machine_timer;
	struct kthread_work vdm_state_machine;
	struct kthread_work rp_state_work;
	struct hrtimer rp_state_change_timer;
	struct delayed_work chunk_msg_work;
	struct delayed_work adjust_ctc_current;
	bool state_machine_running;

	struct completion tx_complete;
	enum sprd_tcpm_transmit_status tx_status;

	struct mutex swap_lock;		/* swap command lock */
	bool swap_pending;
	bool non_pd_role_swap;
	struct completion swap_complete;
	int swap_status;

	bool try_pr_swap;

	unsigned int negotiated_rev;
	unsigned int message_id;
	unsigned int caps_count;
	unsigned int hard_reset_count;
	unsigned int power_role_send_psrdy_count;
	bool pd_capable;
	bool explicit_contract;
	unsigned int rx_msgid;
	bool sink_in_hard_reset;

	/* Partner capabilities/requests */
	u32 sink_request;
	u32 source_caps[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_source_caps;
	u32 sink_caps[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_sink_caps;

	/* Local capabilities */
	u32 src_pdo[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_src_pdo;
	u32 xvddsrc_pdo[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_xvddsrc_pdo;
	u32 src_pdo_ext[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_src_pdo_ext;
	u32 snk_pdo[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_snk_pdo;
	u32 snk_default_pdo[SPRD_PDO_MAX_OBJECTS];
	unsigned int nr_snk_default_pdo;
	u32 snk_vdo[SPRD_VDO_MAX_OBJECTS];
	unsigned int nr_snk_vdo;

	unsigned int operating_snk_mw;
	unsigned int operating_snk_default_mw;
	bool update_sink_caps;
	bool update_ext_src_caps;

	/* Requested current / voltage to the port partner */
	u32 req_current_limit;
	u32 req_supply_voltage;
	/* Actual current / voltage limit of the local port */
	u32 current_limit;
	u32 supply_voltage;

	/* Requested fixed PD voltage */
	bool fixed_pd_pending;
	u32 fixed_pd_voltage;
	struct completion fixed_pd_complete;

	/* Used to export TA voltage and current */
	struct power_supply *psy;
	struct power_supply_desc psy_desc;
	enum power_supply_usb_type usb_type;
	enum power_supply_usb_type last_usb_type;

	u32 bist_request;

	/* PD state for Vendor Defined Messages */
	enum sprd_vdm_states vdm_state;
	u32 vdm_retries;
	/* next Vendor Defined Message to send */
	u32 vdo_data[SPRD_VDO_MAX_SIZE];
	u8 vdo_count;
	/* VDO to retry if UFP responder replied busy */
	u32 vdo_retry;
	struct delayed_work dp_work;
	u32 dp_status;
	u32 vdm_discovery_id_retry;
	bool vdm_sent;
	bool vdm_queue;

	/* PPS */
	struct sprd_pd_pps_data pps_data;
	struct completion pps_complete;
	bool pps_pending;
	int pps_status;

	/* Alternate mode data */
	struct sprd_pd_mode_data mode_data;
	struct typec_altmode *partner_altmode[SPRD_ALTMODE_DISCOVERY_MAX];
	struct typec_altmode *port_altmode[SPRD_ALTMODE_DISCOVERY_MAX];

	/* Deadline in jiffies to exit src_try_wait state */
	unsigned long max_wait;

	/* port belongs to a self powered device */
	bool self_powered;

	/* power or data role swap */
	bool role_swap_flag;
	bool power_role_swap;
	bool data_role_swap;
	bool drs_not_vdm;
	bool power_role_swap_hard_reset;
	bool can_power_data_role_swap;
	bool set_rp_limint_en;
	unsigned int data_role_send_count;

	/* tcpm debug log*/
	int vbus_wait;
	int tcc_debounce;
	int first_pd_cap_delay;
	void *driver_data;
	int (*get_vbus_ok)(void *driver_data);
	struct dentry *dentry;
	struct mutex logbuffer_lock;	/* log buffer access lock */
	struct kthread_worker log_kworker;
	struct kthread_delayed_work log_kwork;
	struct task_struct *log_task;
	struct mutex logprintk_lock;	/* log buffer printk lock */
	struct sprd_tcpm_sysfs *sysfs;
	int logbuffer_head;
	int logbuffer_tail;
	int logbuffer_last;
	bool logbuffer_full;
	int logbuffer_idle_count;
	int logbuffer_show_last;
	bool logbuffer_show_full;
	u8 *logbuffer[SPRD_LOG_BUFFER_ENTRIES];
	bool enable_tcpm_log;
	bool log_output_running;
	bool xts_limit_cur;
	int negotiated_limit_cur;
	int rp_limit;

	enum usb_pd_svdm_ver_minor default_svdm_ver_minor;
	enum usb_pd_svdm_ver_minor negotiated_svdm_ver_minor;

	bool negotiated_limit_ic_current;

	int received_get_snk_cap_cnt;
	bool received_bad_good_crc;
	bool support_pd_cts;
	bool support_usb_suspend;
	bool partner_support_usb_suspend;

	struct wakeup_source *pd_source_ws;
	struct mutex keep_source_awake_mtx;
	bool keep_source_awake;

	bool bist_test_data;

	/* chunk */
	struct completion tx_chunk_request;
	bool tx_chunk_msg;
	u8 data[SPRD_PD_EXT_MAX_CHUNK_DATA];

	u64 tx_complete_curr_time;
};


struct sprd_tcpm_port;

#if IS_ENABLED(CONFIG_SPRD_TYPEC_TCPM)
int sprd_tcpm_typec_device_ops_register(struct sprd_typec_device_ops *ops);
int sprd_tcpm_charger_ops_register(struct sprd_charger_ops *ops);
#else
static inline
int sprd_tcpm_typec_device_ops_register(struct sprd_typec_device_ops *ops)
{
	return 0;
}
static inline
int sprd_tcpm_charger_ops_register(struct sprd_charger_ops *ops)
{
	return 0;
}
#endif

struct sprd_tcpm_port *sprd_tcpm_register_port(struct device *dev, struct tcpc_dev *tcpc);
void sprd_tcpm_unregister_port(struct sprd_tcpm_port *port);

int sprd_tcpm_update_sink_capabilities(struct sprd_tcpm_port *port, const u32 *pdo,
				       unsigned int nr_pdo,
				       unsigned int operating_snk_mw);
int sprd_tcpm_update_ext_source_capabilities(struct sprd_tcpm_port *port,
					     const u32 *pdo,
					     unsigned int nr_pdo);
void sprd_tcpm_get_source_capabilities(struct sprd_tcpm_port *port,
				       struct adapter_power_cap *pd_source_cap);

void sprd_tcpm_vbus_change(struct sprd_tcpm_port *port);
void sprd_tcpm_cc_change(struct sprd_tcpm_port *port);
void sprd_tcpm_pd_receive(struct sprd_tcpm_port *port,
		     const struct sprd_pd_message *msg);
void sprd_tcpm_pd_transmit_complete(struct sprd_tcpm_port *port,
				    enum sprd_tcpm_transmit_status status);
void sprd_tcpm_pd_hard_reset(struct sprd_tcpm_port *port);
void sprd_tcpm_pd_soft_reset(struct sprd_tcpm_port *port);
void sprd_tcpm_tcpc_reset(struct sprd_tcpm_port *port);

void sprd_tcpm_shutdown(struct sprd_tcpm_port *port);
void sprd_tcpm_set_support_accessory_mode(struct sprd_tcpm_port *port);
#if IS_ENABLED(CONFIG_SPRD_TYPEC_TCPM)
void sprd_tcpm_log_do_outside(struct sprd_tcpm_port *port, const char *dev_tag,
			      const char *fmt, va_list args);
#else
static inline
void sprd_tcpm_log_do_outside(struct sprd_tcpm_port *port, const char *dev_tag,
			      const char *fmt, va_list args)
{

}
#endif

#endif /* __LINUX_USB_SPRD_TCPM_H */
