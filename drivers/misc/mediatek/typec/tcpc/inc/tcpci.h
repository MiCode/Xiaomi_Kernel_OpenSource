/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LINUX_RT_TCPC_H
#define __LINUX_RT_TCPC_H

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/pm_wakeup.h>
#include <linux/err.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/sched.h>

#include "tcpci_core.h"

#ifdef CONFIG_PD_DBG_INFO
#include "pd_dbg_info.h"
#endif /* CONFIG_PD_DBG_INFO */

#ifdef CONFIG_USB_POWER_DELIVERY
#include "pd_core.h"
#endif /* CONFIG_USB_POWER_DELIVERY */

#define PE_STATE_FULL_NAME	0

#define TCPC_LOW_RP_DUTY		(100)		/* 10 % */
#define TCPC_NORMAL_RP_DUTY	(330)		/* 33 % */

/* provide to TCPC interface */
extern int tcpci_report_usb_port_changed(struct tcpc_device *tcpc);
extern int tcpci_set_wake_lock(
	struct tcpc_device *tcpc, bool pd_lock, bool user_lock);
extern int tcpci_report_power_control(struct tcpc_device *tcpc, bool en);
extern int tcpc_typec_init(struct tcpc_device *tcpc, uint8_t typec_role);
extern void tcpc_typec_deinit(struct tcpc_device *tcpc);
extern int tcpc_dual_role_phy_init(struct tcpc_device *tcpc);

extern struct tcpc_device *tcpc_device_register(
		struct device *parent, struct tcpc_desc *tcpc_desc,
		struct tcpc_ops *ops, void *drv_data);
extern void tcpc_device_unregister(
			struct device *dev, struct tcpc_device *tcpc);

extern int tcpc_schedule_init_work(struct tcpc_device *tcpc);

extern void *tcpc_get_dev_data(struct tcpc_device *tcpc);
extern void tcpci_lock_typec(struct tcpc_device *tcpc);
extern void tcpci_unlock_typec(struct tcpc_device *tcpc);
extern int tcpci_alert(struct tcpc_device *tcpc);

extern void tcpci_vbus_level_init(
		struct tcpc_device *tcpc, uint16_t power_status);

static inline int tcpci_check_vbus_valid(struct tcpc_device *tcpc)
{
	return tcpc->vbus_level >= TCPC_VBUS_VALID;
}

int tcpci_check_vbus_valid_from_ic(struct tcpc_device *tcpc);
int tcpci_check_vsafe0v(struct tcpc_device *tcpc, bool detect_en);
int tcpci_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask);
int tcpci_fault_status_clear(struct tcpc_device *tcpc, uint8_t status);
int tcpci_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask);
int tcpci_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert);
int tcpci_get_fault_status(struct tcpc_device *tcpc, uint8_t *fault);
int tcpci_get_power_status(struct tcpc_device *tcpc, uint16_t *pw_status);
int tcpci_init(struct tcpc_device *tcpc, bool sw_reset);
int tcpci_init_alert_mask(struct tcpc_device *tcpc);

int tcpci_get_cc(struct tcpc_device *tcpc);
int tcpci_set_cc(struct tcpc_device *tcpc, int pull);
int tcpci_set_polarity(struct tcpc_device *tcpc, int polarity);
int tcpci_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp);
int tcpci_set_vconn(struct tcpc_device *tcpc, int enable);

int tcpci_is_low_power_mode(struct tcpc_device *tcpc);
int tcpci_set_low_power_mode(struct tcpc_device *tcpc, bool en, int pull);
int tcpci_idle_poll_ctrl(struct tcpc_device *tcpc, bool en, bool lock);
int tcpci_set_watchdog(struct tcpc_device *tcpc, bool en);
int tcpci_alert_vendor_defined_handler(struct tcpc_device *tcpc);
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
int tcpci_is_vsafe0v(struct tcpc_device *tcpc);
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_WATER_DETECTION
int tcpci_is_water_detected(struct tcpc_device *tcpc);
int tcpci_set_water_protection(struct tcpc_device *tcpc, bool en);
int tcpci_set_usbid_polling(struct tcpc_device *tcpc, bool en);
int tcpci_notify_wd_status(struct tcpc_device *tcpc, bool water_detected);
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_CABLE_TYPE_DETECTION
int tcpci_notify_cable_type(struct tcpc_device *tcpc);
#endif /* CONFIG_CABLE_TYPE_DETECTION */

#ifdef CONFIG_USB_POWER_DELIVERY

int tcpci_set_msg_header(struct tcpc_device *tcpc,
	uint8_t power_role, uint8_t data_role);

int tcpci_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable);

int tcpci_protocol_reset(struct tcpc_device *tcpc);

int tcpci_get_message(struct tcpc_device *tcpc,
	uint32_t *payload, uint16_t *head, enum tcpm_transmit_type *type);

int tcpci_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data);

int tcpci_set_bist_test_mode(struct tcpc_device *tcpc, bool en);

int tcpci_set_bist_carrier_mode(struct tcpc_device *tcpc, uint8_t pattern);

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
int tcpci_retransmit(struct tcpc_device *tcpc);
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
#endif	/* CONFIG_USB_POWER_DELIVERY */

int tcpci_notify_typec_state(struct tcpc_device *tcpc);

int tcpci_notify_role_swap(
	struct tcpc_device *tcpc, uint8_t event, uint8_t role);
int tcpci_notify_pd_state(struct tcpc_device *tcpc, uint8_t connect);

int tcpci_set_intrst(struct tcpc_device *tcpc, bool en);
int tcpci_enable_watchdog(struct tcpc_device *tcpc, bool en);

int tcpci_source_vbus(struct tcpc_device *tcpc, uint8_t type, int mv, int ma);
int tcpci_sink_vbus(struct tcpc_device *tcpc, uint8_t type, int mv, int ma);
int tcpci_disable_vbus_control(struct tcpc_device *tcpc);
int tcpci_notify_attachwait_state(struct tcpc_device *tcpc, bool as_sink);
int tcpci_enable_ext_discharge(struct tcpc_device *tcpc, bool en);
int tcpci_enable_auto_discharge(struct tcpc_device *tcpc, bool en);
int __tcpci_enable_force_discharge(struct tcpc_device *tcpc, bool en, int mv);
int tcpci_enable_force_discharge(struct tcpc_device *tcpc, bool en, int mv);
int tcpci_disable_force_discharge(struct tcpc_device *tcpc);

#ifdef CONFIG_USB_POWER_DELIVERY

int tcpci_notify_hard_reset_state(struct tcpc_device *tcpc, uint8_t state);

int tcpci_enter_mode(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops, uint32_t mode);
int tcpci_exit_mode(struct tcpc_device *tcpc, uint16_t svid);

#ifdef CONFIG_USB_PD_ALT_MODE
int tcpci_report_hpd_state(struct tcpc_device *tcpc, uint32_t dp_status);
int tcpci_dp_status_update(struct tcpc_device *tcpc, uint32_t dp_status);
int tcpci_dp_configure(struct tcpc_device *tcpc, uint32_t dp_config);
int tcpci_dp_attention(struct tcpc_device *tcpc, uint32_t dp_status);

int tcpci_dp_notify_status_update_done(
	struct tcpc_device *tcpc, uint32_t dp_status, bool ack);

int tcpci_dp_notify_config_start(struct tcpc_device *tcpc);
int tcpci_dp_notify_config_done(struct tcpc_device *tcpc,
	uint32_t local_cfg, uint32_t remote_cfg, bool ack);
#endif	/* CONFIG_USB_PD_ALT_MODE */

#ifdef CONFIG_USB_PD_CUSTOM_VDM
int tcpci_notify_uvdm(struct tcpc_device *tcpc, bool ack);
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

#ifdef CONFIG_USB_PD_ALT_MODE_RTDC
int tcpci_dc_notify_en_unlock(struct tcpc_device *tcpc);
#endif	/* CONFIG_USB_PD_ALT_MODE_RTDC */

#ifdef CONFIG_USB_PD_REV30

#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
int tcpci_notify_alert(struct tcpc_device *tcpc, uint32_t ado);
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */

#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
int tcpci_notify_status(struct tcpc_device *tcpc, struct pd_status *sdb);
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */

#ifdef CONFIG_USB_PD_REV30_BAT_INFO
int tcpci_notify_request_bat_info(
	struct tcpc_device *tcpc, enum pd_battery_reference ref);
#endif	/* CONFIG_USB_PD_REV30_BAT_INFO */
#endif	/* CONFIG_USB_PD_REV30 */

#endif	/* CONFIG_USB_POWER_DELIVERY */

#endif /* #ifndef __LINUX_RT_TCPC_H */
