/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LINUX_RT_TCPCI_CORE_H
#define __LINUX_RT_TCPCI_CORE_H

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/alarmtimer.h>
#include <linux/workqueue.h>
#include <linux/pm_wakeup.h>
#include <linux/notifier.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>

#include "tcpm.h"
#include "tcpci_timer.h"
#include "tcpci_config.h"

#ifdef CONFIG_USB_POWER_DELIVERY
#include "pd_core.h"
#endif

/* The switch of log message */
#define TYPEC_INFO_ENABLE	1
#define TYPEC_INFO2_ENABLE	1
#define PE_EVENT_DBG_ENABLE	1
#define PE_STATE_INFO_ENABLE	1
#define TCPC_INFO_ENABLE	1
#define TCPC_TIMER_DBG_EN	0
#define TCPC_TIMER_INFO_EN	0
#define PE_INFO_ENABLE		1
#define TCPC_DBG_ENABLE		0
#define TCPC_DBG2_ENABLE	0
#define DPM_INFO_ENABLE		1
#define DPM_INFO2_ENABLE	1
#define DPM_DBG_ENABLE		0
#define PD_ERR_ENABLE		1
#define PE_DBG_ENABLE		0
#define TYPEC_DBG_ENABLE	0


#define DP_INFO_ENABLE		1
#define DP_DBG_ENABLE		1

#define UVDM_INFO_ENABLE		1
#define TCPM_DBG_ENABLE		1

#ifdef CONFIG_USB_PD_ALT_MODE_RTDC
#define DC_INFO_ENABLE			1
#define DC_DBG_ENABLE			1
#endif	/* CONFIG_USB_PD_ALT_MODE_RTDC */

#define TCPC_ENABLE_ANYMSG	\
		((TCPC_DBG_ENABLE)|(TCPC_DBG2_ENABLE)|\
		(DPM_DBG_ENABLE)|\
		(PD_ERR_ENABLE)|(PE_INFO_ENABLE)|(TCPC_TIMER_INFO_EN)|\
		(PE_DBG_ENABLE)|(PE_EVENT_DBG_ENABLE)|\
		(PE_STATE_INFO_ENABLE)|(TCPC_INFO_ENABLE)|\
		(TCPC_TIMER_DBG_EN)|(TYPEC_DBG_ENABLE)|\
		(TYPEC_INFO_ENABLE)|\
		(DP_INFO_ENABLE)|(DP_DBG_ENABLE)|\
		(UVDM_INFO_ENABLE)|(TCPM_DBG_ENABLE))

/* Disable VDM DBG Msg */
#define PE_STATE_INFO_VDM_DIS	0
#define PE_EVT_INFO_VDM_DIS		0
#define PE_DBG_RESET_VDM_DIS	1

#define PD_BUG_ON(x)	WARN_ON(x)

struct tcpc_device;

struct tcpc_desc {
	uint8_t role_def;
	uint8_t rp_lvl;
	uint8_t vconn_supply;
	int notifier_supply_num;
	char *name;
};

/*---------------------------------------------------------------------------*/

#ifdef CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK
#define CONFIG_TYPEC_NOTIFY_ATTACHWAIT
#endif	/* CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK */

#ifdef CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SRC
#undef CONFIG_TYPEC_NOTIFY_ATTACHWAIT
#define CONFIG_TYPEC_NOTIFY_ATTACHWAIT
#endif	/* CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK */


#ifdef CONFIG_TCPC_FORCE_DISCHARGE_EXT
#define CONFIG_TCPC_EXT_DISCHARGE
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_EXT */

#ifdef CONFIG_TCPC_AUTO_DISCHARGE_EXT
#undef CONFIG_TCPC_EXT_DISCHARGE
#define CONFIG_TCPC_EXT_DISCHARGE
#endif	/* CONFIG_TCPC_AUTO_DISCHARGE_EXT */

/*---------------------------------------------------------------------------*/

/* TCPC Power Register Define */
#define TCPC_REG_POWER_STATUS_EXT_VSAFE0V	(1<<15)	/* extend */
#define TCPC_REG_POWER_STATUS_VBUS_PRES		(1<<2)

/* TCPC Alert Register Define */
#define TCPC_REG_ALERT_EXT_RA_DETACH		(1<<(16+5))
#define TCPC_REG_ALERT_EXT_WATCHDOG		(1<<(16+2))
#define TCPC_REG_ALERT_EXT_VBUS_80		(1<<(16+1))
#define TCPC_REG_ALERT_EXT_WAKEUP		(1<<(16+0))

#define TCPC_REG_ALERT_VBUS_DISCNCT (1<<11)
#define TCPC_REG_ALERT_RX_BUF_OVF   (1<<10)
#define TCPC_REG_ALERT_FAULT        (1<<9)
#define TCPC_REG_ALERT_V_ALARM_LO   (1<<8)
#define TCPC_REG_ALERT_V_ALARM_HI   (1<<7)
#define TCPC_REG_ALERT_TX_SUCCESS   (1<<6)
#define TCPC_REG_ALERT_TX_DISCARDED (1<<5)
#define TCPC_REG_ALERT_TX_FAILED    (1<<4)
#define TCPC_REG_ALERT_RX_HARD_RST  (1<<3)
#define TCPC_REG_ALERT_RX_STATUS    (1<<2)
#define TCPC_REG_ALERT_POWER_STATUS (1<<1)
#define TCPC_REG_ALERT_CC_STATUS    (1<<0)

#define TCPC_REG_ALERT_RX_MASK	\
	(TCPC_REG_ALERT_RX_STATUS | TCPC_REG_ALERT_RX_BUF_OVF)

#define TCPC_REG_ALERT_HRESET_SUCCESS	\
	(TCPC_REG_ALERT_TX_SUCCESS | TCPC_REG_ALERT_TX_FAILED)

#define TCPC_REG_ALERT_TX_MASK (TCPC_REG_ALERT_TX_SUCCESS | \
	TCPC_REG_ALERT_TX_FAILED | TCPC_REG_ALERT_TX_DISCARDED)

#define TCPC_REG_ALERT_TXRX_MASK	\
	(TCPC_REG_ALERT_TX_MASK | TCPC_REG_ALERT_RX_MASK)

/* TCPC Behavior Flags */
#define TCPC_FLAGS_RETRY_CRC_DISCARD		(1<<0)
#define TCPC_FLAGS_WAIT_HRESET_COMPLETE		(1<<1)	/* Always true */
#define TCPC_FLAGS_CHECK_CC_STABLE		(1<<2)
#define TCPC_FLAGS_LPM_WAKEUP_WATCHDOG		(1<<3)
#define TCPC_FLAGS_CHECK_RA_DETACHE		(1<<4)
#define TCPC_FLAGS_PREFER_LEGACY2		(1<<5)
#define TCPC_FLAGS_DISABLE_LEGACY		(1<<6)

#define TCPC_FLAGS_PD_REV30			(1<<7)

#define TCPC_FLAGS_WATCHDOG_EN			(1<<8)
#define TCPC_FLAGS_WATER_DETECTION		(1<<9)
#define TCPC_FLAGS_CABLE_TYPE_DETECTION		(1<<10)

enum tcpc_cc_pull {
	TYPEC_CC_RA = 0,
	TYPEC_CC_RP = 1,
	TYPEC_CC_RD = 2,
	TYPEC_CC_OPEN = 3,
	TYPEC_CC_DRP = 4,	/* from Rd */

	TYPEC_CC_RP_DFT = 1,		/* 0x00 + 1 */
	TYPEC_CC_RP_1_5 = 9,		/* 0x08 + 1*/
	TYPEC_CC_RP_3_0 = 17,		/* 0x10 + 1 */

	TYPEC_CC_DRP_DFT = 4,		/* 0x00 + 4 */
	TYPEC_CC_DRP_1_5 = 12,		/* 0x08 + 4 */
	TYPEC_CC_DRP_3_0 = 20,		/* 0x10 + 4 */
};

#define TYPEC_CC_PULL_GET_RES(pull)		(pull & 0x07)
#define TYPEC_CC_PULL_GET_RP_LVL(pull)	((pull & 0x18) >> 3)

enum tcpm_rx_cap_type {
	TCPC_RX_CAP_SOP = 1 << 0,
	TCPC_RX_CAP_SOP_PRIME = 1 << 1,
	TCPC_RX_CAP_SOP_PRIME_PRIME = 1 << 2,
	TCPC_RX_CAP_SOP_DEBUG_PRIME = 1 << 3,
	TCPC_RX_CAP_SOP_DEBUG_PRIME_PRIME = 1 << 4,
	TCPC_RX_CAP_HARD_RESET = 1 << 5,
	TCPC_RX_CAP_CABLE_RESET = 1 << 6,
};

struct tcpc_ops {
	int (*init)(struct tcpc_device *tcpc, bool sw_reset);
	int (*init_alert_mask)(struct tcpc_device *tcpc);
	int (*alert_status_clear)(struct tcpc_device *tcpc, uint32_t mask);
	int (*fault_status_clear)(struct tcpc_device *tcpc, uint8_t status);
	int (*get_alert_mask)(struct tcpc_device *tcpc, uint32_t *mask);
	int (*get_alert_status)(struct tcpc_device *tcpc, uint32_t *alert);
	int (*get_power_status)(struct tcpc_device *tcpc, uint16_t *pwr_status);
	int (*get_fault_status)(struct tcpc_device *tcpc, uint8_t *status);
	int (*get_cc)(struct tcpc_device *tcpc, int *cc1, int *cc2);
	int (*set_cc)(struct tcpc_device *tcpc, int pull);
	int (*set_polarity)(struct tcpc_device *tcpc, int polarity);
	int (*set_low_rp_duty)(struct tcpc_device *tcpc, bool low_rp);
	int (*set_vconn)(struct tcpc_device *tcpc, int enable);
	int (*deinit)(struct tcpc_device *tcpc);
	int (*alert_vendor_defined_handler)(struct tcpc_device *tcpc);

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	int (*is_vsafe0v)(struct tcpc_device *tcpc);
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_WATER_DETECTION
	int (*is_water_detected)(struct tcpc_device *tcpc);
	int (*set_water_protection)(struct tcpc_device *tcpc, bool en);
	int (*set_usbid_polling)(struct tcpc_device *tcpc, bool en);
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	int (*is_low_power_mode)(struct tcpc_device *tcpc);
	int (*set_low_power_mode)(struct tcpc_device *tcpc, bool en, int pull);
#endif /* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_IDLE_MODE
	int (*set_idle_mode)(struct tcpc_device *tcpc, bool en);
#endif /* CONFIG_TCPC_IDLE_MODE */

	int (*set_watchdog)(struct tcpc_device *tcpc, bool en);

#ifdef CONFIG_TCPC_INTRST_EN
	int (*set_intrst)(struct tcpc_device *tcpc, bool en);
#endif /* CONFIG_TCPC_INTRST_EN */

#ifdef CONFIG_TYPEC_CAP_AUTO_DISCHARGE
#ifdef CONFIG_TCPC_AUTO_DISCHARGE_IC
	int (*set_auto_discharge)(struct tcpc_device *tcpc, bool en);
#endif	/* CONFIG_TCPC_AUTO_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_AUTO_DISCHARGE */

#ifdef CONFIG_USB_POWER_DELIVERY
	int (*set_msg_header)(struct tcpc_device *tcpc,
			uint8_t power_role, uint8_t data_role);
	int (*set_rx_enable)(struct tcpc_device *tcpc, uint8_t enable);
	int (*get_message)(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *head, enum tcpm_transmit_type *type);
	int (*protocol_reset)(struct tcpc_device *tcpc);
	int (*transmit)(struct tcpc_device *tcpc,
			enum tcpm_transmit_type type,
			uint16_t header, const uint32_t *data);
	int (*set_bist_test_mode)(struct tcpc_device *tcpc, bool en);
	int (*set_bist_carrier_mode)(struct tcpc_device *tcpc, uint8_t pattern);

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	int (*retransmit)(struct tcpc_device *tcpc);
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#ifdef CONFIG_TCPC_FORCE_DISCHARGE_IC
	int (*set_force_discharge)(struct tcpc_device *tcpc, bool en, int mv);
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */

#endif	/* CONFIG_USB_POWER_DELIVERY */
};

#define TCPC_VBUS_SOURCE_0V		(0)
#define TCPC_VBUS_SOURCE_5V		(5000)

#define TCPC_VBUS_SINK_0V		(0)
#define TCPC_VBUS_SINK_5V		(5000)

#define TCPC_LOW_POWER_MODE_RETRY	5

/*
 * Confirm DUT is connected to legacy cable or not
 *	after suupect_counter > this threshold (0 = always check)
 */

#define TCPC_LEGACY_CABLE_SUSPECT_THD	1

/*
 * Try another s/w workaround after retry_counter more than this value
 * Try which soltuion first is determined by tcpc_flags
 */

#define TCPC_LEGACY_CABLE_RETRY_SOLUTION	2

struct tcpc_managed_res;

/*
 * tcpc device
 */

struct tcpc_device {
	struct i2c_client *client;
	struct tcpc_ops *ops;
	void *drv_data;
	struct tcpc_desc desc;
	struct device dev;
	bool wake_lock_user;
	uint8_t wake_lock_pd;
	struct wakeup_source *attach_wake_lock;
	struct wakeup_source *dettach_temp_wake_lock;

	/* For tcpc timer & event */
	uint32_t timer_handle_index;
	struct hrtimer tcpc_timer[PD_TIMER_NR];

	struct alarm wake_up_timer;
	struct delayed_work wake_up_work;
	struct wakeup_source *wakeup_wake_lock;

	ktime_t last_expire[PD_TIMER_NR];
	struct mutex access_lock;
	struct mutex typec_lock;
	struct mutex timer_lock;
	struct semaphore timer_enable_mask_lock;
	spinlock_t timer_tick_lock;
	atomic_t pending_event;
	uint64_t timer_tick;
	uint64_t timer_enable_mask;
	wait_queue_head_t event_loop_wait_que;
	wait_queue_head_t  timer_wait_que;
	struct task_struct *event_task;
	struct task_struct *timer_task;
	bool timer_thread_stop;
	bool event_loop_thread_stop;

	struct delayed_work	init_work;
	struct delayed_work	event_init_work;
	struct srcu_notifier_head evt_nh[TCP_NOTIFY_IDX_NR];
	struct tcpc_managed_res *mr_head;
	struct mutex mr_lock;

	/* For TCPC TypeC */
	uint8_t typec_state;
	uint8_t typec_role;
	uint8_t typec_attach_old;
	uint8_t typec_attach_new;
	uint8_t typec_local_cc;
	uint8_t typec_local_rp_level;
	uint8_t typec_remote_cc[2];
	uint8_t typec_remote_rp_level;
	uint8_t typec_wait_ps_change;
	bool typec_polarity;
	bool typec_drp_try_timeout;
	bool typec_lpm;
	bool typec_cable_only;
	bool typec_power_ctrl;
	bool typec_watchdog;
	bool typec_reach_vsafe0v;

	int typec_usb_sink_curr;

#ifdef CONFIG_TYPEC_CAP_CUSTOM_HV
	bool typec_during_custom_hv;
#endif	/* CONFIG_TYPEC_CAP_CUSTOM_HV */

	uint8_t typec_lpm_pull;
	uint8_t typec_lpm_retry;

#ifdef CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY
	bool typec_wakeup_once;
	bool typec_low_rp_duty_cntdown;
#endif	/* CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY */

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	uint8_t typec_legacy_cable;
#if TCPC_LEGACY_CABLE_SUSPECT_THD
	uint8_t typec_legacy_cable_suspect;
#endif	/* TCPC_LEGACY_CABLE_SUSPECT_THD */

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2
	uint8_t typec_legacy_retry_wk;
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

#ifdef CONFIG_TYPEC_CAP_ROLE_SWAP
	uint8_t typec_during_role_swap;
#endif	/* CONFIG_TYPEC_CAP_ROLE_SWAP */

#ifdef CONFIG_TYPEC_CAP_AUTO_DISCHARGE
#ifdef CONFIG_TCPC_AUTO_DISCHARGE_IC
	bool typec_auto_discharge;
#endif	/* CONFIG_TCPC_AUTO_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_AUTO_DISCHARGE */

#ifdef CONFIG_TCPC_EXT_DISCHARGE
	bool typec_ext_discharge;
#endif	/* CONFIG_TCPC_EXT_DISCHARGE */

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	uint8_t tcpc_vconn_supply;
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

#ifdef CONFIG_TCPC_SOURCE_VCONN
	bool tcpc_source_vconn;
#endif	/* CONFIG_TCPC_SOURCE_VCONN */

	uint32_t tcpc_flags;

#ifdef CONFIG_DUAL_ROLE_USB_INTF
	struct dual_role_phy_instance *dr_usb;
	uint8_t dual_role_supported_modes;
	uint8_t dual_role_mode;
	uint8_t dual_role_pr;
	uint8_t dual_role_dr;
	uint8_t dual_role_vconn;
#endif /* CONFIG_DUAL_ROLE_USB_INTF */

#ifdef CONFIG_USB_POWER_DELIVERY
	/* Event */
	uint8_t pd_event_count;
	uint8_t pd_event_head_index;
	uint8_t pd_msg_buffer_allocated;

	uint8_t pd_last_vdm_msg_id;
	bool pd_pending_vdm_event;
	bool pd_pending_vdm_reset;
	bool pd_pending_vdm_good_crc;
	bool pd_pending_vdm_discard;
	bool pd_pending_vdm_attention;
	bool pd_postpone_vdm_timeout;

	struct pd_msg pd_last_vdm_msg;
	struct pd_msg pd_attention_vdm_msg;
	struct pd_event pd_vdm_event;

	struct pd_msg pd_msg_buffer[PD_MSG_BUF_SIZE];
	struct pd_event pd_event_ring_buffer[PD_EVENT_BUF_SIZE];

	uint8_t tcp_event_count;
	uint8_t tcp_event_head_index;
	struct tcp_dpm_event tcp_event_ring_buffer[TCP_EVENT_BUF_SIZE];

	bool pd_pe_running;
	bool pd_wait_pe_idle;
	bool pd_hard_reset_event_pending;
	bool pd_wait_hard_reset_complete;
	bool pd_wait_pr_swap_complete;
	bool pd_ping_event_pending;
	uint8_t pd_bist_mode;
	uint8_t pd_transmit_state;
	uint8_t pd_wait_vbus_once;

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
	bool pd_during_direct_charge;
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	bool pd_discard_pending;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#ifdef CONFIG_TCPC_FORCE_DISCHARGE_IC
	bool pd_force_discharge;
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */

#ifdef CONFIG_USB_PD_REV30
	uint8_t pd_retry_count;
#endif	/* CONFIG_USB_PD_REV30 */

#ifdef CONFIG_USB_PD_DISABLE_PE
	bool disable_pe; /* typec only */
#endif	/* CONFIG_USB_PD_DISABLE_PE */

	struct pd_port pd_port;
#ifdef CONFIG_USB_PD_REV30
	struct notifier_block bat_nb;
	struct delayed_work bat_update_work;
	struct power_supply *bat_psy;
	uint8_t charging_status;
	int bat_soc;
#endif /* CONFIG_USB_PD_REV30 */
#ifdef CONFIG_MTK_WAIT_BC12
	uint8_t wait_bc12_cnt;
	struct power_supply *chg_psy;
#endif /* CONFIG_MTK_WAIT_BC12 */
#endif /* CONFIG_USB_POWER_DELIVERY */
	u8 vbus_level:2;
	bool vbus_safe0v;
	bool vbus_present;
	u8 irq_enabled:1;
	u8 pd_inited_flag:1; /* MTK Only */

	/* TypeC Shield Protection */
#ifdef CONFIG_WATER_DETECTION
	int usbid_calib;
#endif /* CONFIG_WATER_DETECTION */
#ifdef CONFIG_CABLE_TYPE_DETECTION
	enum tcpc_cable_type typec_cable_type;
#endif /* CONFIG_CABLE_TYPE_DETECTION */
};


#define to_tcpc_device(obj) container_of(obj, struct tcpc_device, dev)

#ifdef CONFIG_PD_DBG_INFO
#define RT_DBG_INFO	pd_dbg_info
#else
#define RT_DBG_INFO	pr_info
#endif /* CONFIG_PD_DBG_INFO */

#if TYPEC_DBG_ENABLE
#define TYPEC_DBG(format, args...)		\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TYPEC:" format, ##args)
#else
#define TYPEC_DBG(format, args...)
#endif /* TYPEC_DBG_ENABLE */

#if TYPEC_INFO_ENABLE
#define TYPEC_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TYPEC:" format, ##args)
#else
#define TYPEC_INFO(format, args...)
#endif /* TYPEC_INFO_ENABLE */

#if TYPEC_INFO2_ENABLE
#define TYPEC_INFO2(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TYPEC:" format, ##args)
#else
#define TYPEC_INFO2(format, args...)
#endif /* TYPEC_INFO_ENABLE */

#if TCPC_INFO_ENABLE
#define TCPC_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TCPC:" format, ##args)
#else
#define TCPC_INFO(foramt, args...)
#endif /* TCPC_INFO_ENABLE */

#if TCPC_DBG_ENABLE
#define TCPC_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TCPC:" format, ##args)
#else
#define TCPC_DBG(format, args...)
#endif /* TCPC_DBG_ENABLE */

#if TCPC_DBG2_ENABLE
#define TCPC_DBG2(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TCPC:" format, ##args)
#else
#define TCPC_DBG2(format, args...)
#endif /* TCPC_DBG2_ENABLE */

#define TCPC_ERR(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TCPC-ERR:" format, ##args)

#define DP_ERR(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DP-ERR:" format, ##args)

#if DPM_INFO_ENABLE
#define DPM_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DPM:" format, ##args)
#else
#define DPM_INFO(format, args...)
#endif /* DPM_DBG_INFO */

#if DPM_INFO2_ENABLE
#define DPM_INFO2(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DPM:" format, ##args)
#else
#define DPM_INFO2(format, args...)
#endif /* DPM_DBG_INFO */

#if DPM_DBG_ENABLE
#define DPM_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DPM:" format, ##args)
#else
#define DPM_DBG(format, args...)
#endif /* DPM_DBG_ENABLE */

#if PD_ERR_ENABLE
#define PD_ERR(format, args...) \
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "PD-ERR:" format, ##args)
#else
#define PD_ERR(format, args...)
#endif /* PD_ERR_ENABLE */

#if PE_INFO_ENABLE
#define PE_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "PE:" format, ##args)
#else
#define PE_INFO(format, args...)
#endif /* PE_INFO_ENABLE */

#if PE_EVENT_DBG_ENABLE
#define PE_EVT_INFO(format, args...) \
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "PE-EVT:" format, ##args)
#else
#define PE_EVT_INFO(format, args...)
#endif /* PE_EVENT_DBG_ENABLE */

#if PE_DBG_ENABLE
#define PE_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "PE:" format, ##args)
#else
#define PE_DBG(format, args...)
#endif /* PE_DBG_ENABLE */

#if PE_STATE_INFO_ENABLE
#define PE_STATE_INFO(format, args...) \
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "PE:" format, ##args)
#else
#define PE_STATE_INFO(format, args...)
#endif /* PE_STATE_IFNO_ENABLE */

#if DP_INFO_ENABLE
#define DP_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DP:" format, ##args)
#else
#define DP_INFO(format, args...)
#endif /* DP_INFO_ENABLE */

#if DP_DBG_ENABLE
#define DP_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DP:" format, ##args)
#else
#define DP_DBG(format, args...)
#endif /* DP_DBG_ENABLE */

#if UVDM_INFO_ENABLE
#define UVDM_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "UVDM:" format, ## args)
#else
#define UVDM_INFO(format, args...)
#endif

#if TCPM_DBG_ENABLE
#define TCPM_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "TCPM:" format, ## args)
#else
#define TCPM_DBG(format, args...)
#endif

#ifdef CONFIG_USB_PD_ALT_MODE_RTDC

#if DC_INFO_ENABLE
#define DC_INFO(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DC> " format, ## args)
#else
#define DC_INFO(format, args...)
#endif

#if DC_DBG_ENABLE
#define DC_DBG(format, args...)	\
	RT_DBG_INFO(CONFIG_TCPC_DBG_PRESTR "DC> " format, ## args)
#else
#define DC_DBG(format, args...)
#endif

#endif	/* CONFIG_USB_PD_ALT_MODE_RTDC */

#endif /* #ifndef __LINUX_RT_TCPCI_CORE_H */
