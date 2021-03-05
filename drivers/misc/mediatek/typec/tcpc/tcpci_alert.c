// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/cpu.h>

#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"

#ifdef CONFIG_USB_POWER_DELIVERY
#include "inc/tcpci_event.h"
#endif /* CONFIG_USB_POWER_DELIVERY */

/*
 * [BLOCK] TCPCI IRQ Handler
 */

static int tcpci_alert_cc_changed(struct tcpc_device *tcpc)
{
	return tcpc_typec_handle_cc_change(tcpc);
}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC

static inline int tcpci_alert_vsafe0v(struct tcpc_device *tcpc)
{
	tcpc_typec_handle_vsafe0v(tcpc);

#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_USB_PD_SAFE0V_DELAY
	tcpc_enable_timer(tcpc, PD_TIMER_VSAFE0V_DELAY);
#else
	pd_put_vbus_safe0v_event(tcpc);
#endif	/* CONFIG_USB_PD_SAFE0V_DELAY */
#endif	/* CONFIG_USB_POWER_DELIVERY */

	return 0;
}

#endif	/* CONFIG_TCPC_VSAFE0V_DETECT_IC */

static inline void tcpci_vbus_level_init_v10(
	struct tcpc_device *tcpc, uint16_t power_status)
{
	mutex_lock(&tcpc->access_lock);

	tcpc->vbus_level =
			power_status & TCPC_REG_POWER_STATUS_VBUS_PRES ?
			TCPC_VBUS_VALID : TCPC_VBUS_INVALID;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	if (power_status & TCPC_REG_POWER_STATUS_EXT_VSAFE0V) {
		if (tcpc->vbus_level == TCPC_VBUS_INVALID)
			tcpc->vbus_level = TCPC_VBUS_SAFE0V;
		else
			TCPC_INFO("ps_confused: 0x%02x\r\n", power_status);
	}
#endif	/* CONFIG_TCPC_VSAFE0V_DETECT_IC */

	mutex_unlock(&tcpc->access_lock);
}

static inline void __tcpci_vbus_level_refresh(struct tcpc_device *tcpc)
{
	tcpc->vbus_level = tcpc->vbus_present ? TCPC_VBUS_VALID :
			       TCPC_VBUS_INVALID;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	if (tcpc->vbus_safe0v) {
		if (tcpc->vbus_level == TCPC_VBUS_INVALID)
			tcpc->vbus_level = TCPC_VBUS_SAFE0V;
		else
			TCPC_INFO("ps_confused: %d\r\n", tcpc->vbus_level);
	}
#endif	/* CONFIG_TCPC_VSAFE0V_DETECT_IC */
}

static inline void tcpci_vbus_level_refresh(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->access_lock);
	__tcpci_vbus_level_refresh(tcpc);
	mutex_unlock(&tcpc->access_lock);
}

void tcpci_vbus_level_init(struct tcpc_device *tcpc, uint16_t status)
{
	if (tcpc->tcpc_flags & TCPC_FLAGS_ALERT_V10) {
		tcpci_vbus_level_init_v10(tcpc, status);
		return;
	}

	mutex_lock(&tcpc->access_lock);

	tcpc->vbus_present = status & TCPC_REG_POWER_STATUS_VBUS_PRES ?
				 true : false;
	__tcpci_vbus_level_refresh(tcpc);
	mutex_unlock(&tcpc->access_lock);
}

static inline int tcpci_alert_power_status_changed_v10(struct tcpc_device *tcpc)
{
	int rv = 0;
	bool show_msg = true;
	uint16_t power_status = 0;

	rv = tcpci_get_power_status(tcpc, &power_status);
	if (rv < 0)
		return rv;

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
	if (tcpc->pd_during_direct_charge && tcpc->vbus_level != 0)
		show_msg = false;
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

	if (show_msg)
		TCPC_INFO("ps_change=%d\r\n", tcpc->vbus_level);

	rv = tcpc_typec_handle_ps_change(tcpc, tcpc->vbus_level);
	if (rv < 0)
		return rv;

#ifdef CONFIG_USB_POWER_DELIVERY
	pd_put_vbus_changed_event(tcpc, true);
#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	if (tcpc->vbus_level == TCPC_VBUS_SAFE0V)
		rv = tcpci_alert_vsafe0v(tcpc);
#endif	/* CONFIG_TCPC_VSAFE0V_DETECT_IC */

	return rv;
}

static inline int tcpci_vbus_level_changed(struct tcpc_device *tcpc)
{
	int rv = 0;
	bool show_msg = true;

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
	if (tcpc->pd_during_direct_charge && tcpc->vbus_level != 0)
		show_msg = false;
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

	if (show_msg)
		TCPC_INFO("ps_change=%d\r\n", tcpc->vbus_level);

	rv = tcpc_typec_handle_ps_change(tcpc, tcpc->vbus_level);
	if (rv < 0)
		return rv;

#ifdef CONFIG_USB_POWER_DELIVERY
	pd_put_vbus_changed_event(tcpc, true);
#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	if (tcpc->vbus_level == TCPC_VBUS_SAFE0V)
		rv = tcpci_alert_vsafe0v(tcpc);
#endif	/* CONFIG_TCPC_VSAFE0V_DETECT_IC */
	return rv;
}

static int tcpci_alert_power_status_changed(struct tcpc_device *tcpc)
{
	int rv = 0;
	uint16_t status = 0;

	if (tcpc->tcpc_flags & TCPC_FLAGS_ALERT_V10)
		return tcpci_alert_power_status_changed_v10(tcpc);

	rv = tcpci_get_power_status(tcpc, &status);
	if (rv < 0)
		return rv;

	tcpc->vbus_present = (status & TCPC_REG_POWER_STATUS_VBUS_PRES) ?
				 true : false;
	return rv;
}

#ifdef CONFIG_USB_POWER_DELIVERY
static int tcpci_alert_tx_success(struct tcpc_device *tcpc)
{
	uint8_t tx_state;

	struct pd_event evt = {
		.event_type = PD_EVT_CTRL_MSG,
		.msg = PD_CTRL_GOOD_CRC,
		.pd_msg = NULL,
	};

	mutex_lock(&tcpc->access_lock);
	tx_state = tcpc->pd_transmit_state;
	tcpc->pd_transmit_state = PD_TX_STATE_GOOD_CRC;
	mutex_unlock(&tcpc->access_lock);

	if (tx_state == PD_TX_STATE_WAIT_CRC_VDM)
		pd_put_vdm_event(tcpc, &evt, false);
	else
		pd_put_event(tcpc, &evt, false);

	return 0;
}

static int tcpci_alert_tx_failed(struct tcpc_device *tcpc)
{
	uint8_t tx_state;

	mutex_lock(&tcpc->access_lock);
	tx_state = tcpc->pd_transmit_state;
	tcpc->pd_transmit_state = PD_TX_STATE_NO_GOOD_CRC;
	mutex_unlock(&tcpc->access_lock);

	if (tx_state == PD_TX_STATE_WAIT_CRC_VDM)
		vdm_put_hw_event(tcpc, PD_HW_TX_FAILED);
	else
		pd_put_hw_event(tcpc, PD_HW_TX_FAILED);

	return 0;
}

static int tcpci_alert_tx_discard(struct tcpc_device *tcpc)
{
	uint8_t tx_state;
	bool retry_crc_discard = false;

	mutex_lock(&tcpc->access_lock);
	tx_state = tcpc->pd_transmit_state;
	tcpc->pd_transmit_state = PD_TX_STATE_DISCARD;
	mutex_unlock(&tcpc->access_lock);

	TCPC_INFO("Discard\r\n");

	if (tx_state == PD_TX_STATE_WAIT_CRC_VDM)
		pd_put_last_vdm_event(tcpc);
	else {
		retry_crc_discard =
			(tcpc->tcpc_flags &
					TCPC_FLAGS_RETRY_CRC_DISCARD) != 0;

		if (retry_crc_discard) {
#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
			tcpc->pd_discard_pending = true;
			tcpc_enable_timer(tcpc, PD_TIMER_DISCARD);
#else
			TCPC_ERR("RETRY_CRC_DISCARD\r\n");
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
		} else {
			pd_put_hw_event(tcpc, PD_HW_TX_FAILED);
		}
	}
	return 0;
}

static int tcpci_alert_recv_msg(struct tcpc_device *tcpc)
{
	int retval;
	struct pd_msg *pd_msg;
	enum tcpm_transmit_type type;

	pd_msg = pd_alloc_msg(tcpc);
	if (pd_msg == NULL) {
		tcpci_alert_status_clear(tcpc, TCPC_REG_ALERT_RX_MASK);
		return -EINVAL;
	}

	retval = tcpci_get_message(tcpc,
		pd_msg->payload, &pd_msg->msg_hdr, &type);
	if (retval < 0) {
		TCPC_INFO("recv_msg failed: %d\r\n", retval);
		pd_free_msg(tcpc, pd_msg);
		return retval;
	}

	pd_msg->frame_type = (uint8_t) type;
	pd_put_pd_msg_event(tcpc, pd_msg);
	return 0;
}

static int tcpci_alert_rx_overflow(struct tcpc_device *tcpc)
{
	int rv;
	uint32_t alert_status;

	TCPC_INFO("RX_OVERFLOW\r\n");

	rv = tcpci_get_alert_status(tcpc, &alert_status);
	if (rv)
		return rv;

	if (alert_status & TCPC_REG_ALERT_RX_STATUS)
		return tcpci_alert_recv_msg(tcpc);

	return 0;
}

static int tcpci_alert_recv_hard_reset(struct tcpc_device *tcpc)
{
	TCPC_INFO("HardResetAlert\r\n");
	pd_put_recv_hard_reset_event(tcpc);
	tcpci_init_alert_mask(tcpc);
	return 0;
}

#endif /* CONFIG_USB_POWER_DELIVERY */

static int tcpci_alert_vendor_defined(struct tcpc_device *tcpc)
{
	tcpci_alert_vendor_defined_handler(tcpc);
	return 0;
}

static int tcpci_alert_fault(struct tcpc_device *tcpc)
{
	uint8_t status = 0;

	tcpci_get_fault_status(tcpc, &status);
	TCPC_INFO("FaultAlert=0x%x\r\n", status);
	tcpci_fault_status_clear(tcpc, status);
	return 0;
}

#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG
static int tcpci_alert_wakeup(struct tcpc_device *tcpc)
{
	if (tcpc->tcpc_flags & TCPC_FLAGS_LPM_WAKEUP_WATCHDOG) {
		TCPC_INFO("Wakeup\r\n");

		if (tcpc->typec_remote_cc[0] == TYPEC_CC_DRP_TOGGLING)
			tcpc_enable_wakeup_timer(tcpc, true);
	}

	return 0;
}
#endif /* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */

#ifdef CONFIG_TYPEC_CAP_RA_DETACH
static int tcpci_alert_ra_detach(struct tcpc_device *tcpc)
{
	if (tcpc->tcpc_flags & TCPC_FLAGS_CHECK_RA_DETACHE) {
		TCPC_DBG("RA_DETACH\r\n");

		if (tcpc->typec_remote_cc[0] == TYPEC_CC_DRP_TOGGLING)
			tcpc_typec_enter_lpm_again(tcpc);
	}

	return 0;
}
#endif /* CONFIG_TYPEC_CAP_RA_DETACH */

struct tcpci_alert_handler {
	uint32_t bit_mask;
	int (*handler)(struct tcpc_device *tcpc);
};

#define DECL_TCPCI_ALERT_HANDLER(xbit, xhandler) {\
		.bit_mask = 1 << xbit,\
		.handler = xhandler, \
	}

static const struct tcpci_alert_handler tcpci_alert_handlers[] = {
	DECL_TCPCI_ALERT_HANDLER(15, tcpci_alert_vendor_defined),
#ifdef CONFIG_USB_POWER_DELIVERY
	DECL_TCPCI_ALERT_HANDLER(4, tcpci_alert_tx_failed),
	DECL_TCPCI_ALERT_HANDLER(5, tcpci_alert_tx_discard),
	DECL_TCPCI_ALERT_HANDLER(6, tcpci_alert_tx_success),
	DECL_TCPCI_ALERT_HANDLER(2, tcpci_alert_recv_msg),
	DECL_TCPCI_ALERT_HANDLER(7, NULL),
	DECL_TCPCI_ALERT_HANDLER(8, NULL),
	DECL_TCPCI_ALERT_HANDLER(3, tcpci_alert_recv_hard_reset),
	DECL_TCPCI_ALERT_HANDLER(10, tcpci_alert_rx_overflow),
#endif /* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG
	DECL_TCPCI_ALERT_HANDLER(16, tcpci_alert_wakeup),
#endif /* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */

#ifdef CONFIG_TYPEC_CAP_RA_DETACH
	DECL_TCPCI_ALERT_HANDLER(21, tcpci_alert_ra_detach),
#endif /* CONFIG_TYPEC_CAP_RA_DETACH */

	DECL_TCPCI_ALERT_HANDLER(9, tcpci_alert_fault),
	DECL_TCPCI_ALERT_HANDLER(0, tcpci_alert_cc_changed),
	DECL_TCPCI_ALERT_HANDLER(1, tcpci_alert_power_status_changed),
};

#ifdef CONFIG_USB_POWER_DELIVERY
static inline bool tcpci_check_hard_reset_complete(
	struct tcpc_device *tcpc, uint32_t alert_status)
{
	if ((alert_status & TCPC_REG_ALERT_HRESET_SUCCESS)
			== TCPC_REG_ALERT_HRESET_SUCCESS) {
		pd_put_sent_hard_reset_event(tcpc);
		return true;
	}

	if (alert_status & TCPC_REG_ALERT_TX_DISCARDED) {
		TCPC_INFO("HResetFailed\r\n");
		tcpci_transmit(tcpc, TCPC_TX_HARD_RESET, 0, NULL);
		return false;
	}

	return false;
}
#endif	/* CONFIG_USB_POWER_DELIVERY */
int tcpci_alert(struct tcpc_device *tcpc)
{
	int rv, i;
	uint32_t alert_status;
	uint32_t alert_mask;

	rv = tcpci_get_alert_status(tcpc, &alert_status);
	if (rv)
		return rv;

	rv = tcpci_get_alert_mask(tcpc, &alert_mask);
	if (rv)
		return rv;

	/* mask all alert */
	rv = tcpci_set_alert_mask(tcpc, 0);
	if (rv)
		return rv;

#ifdef CONFIG_USB_PD_DBG_ALERT_STATUS
	if (alert_status != 0)
		TCPC_INFO("Alert:0x%04x, Mask:0x%04x\r\n",
			  alert_status, alert_mask);
#endif /* CONFIG_USB_PD_DBG_ALERT_STATUS */

	alert_status &= alert_mask;

	tcpci_alert_status_clear(tcpc,
		alert_status & (~TCPC_REG_ALERT_RX_MASK));

	if (tcpc->typec_role == TYPEC_ROLE_UNKNOWN)
		return 0;

	if ((tcpc->tcpc_flags & TCPC_FLAGS_ALERT_V10) &&
	    (alert_status & TCPC_REG_ALERT_EXT_VBUS_80))
		alert_status |= TCPC_REG_ALERT_POWER_STATUS;

#ifdef CONFIG_USB_POWER_DELIVERY
	if (tcpc->pd_transmit_state == PD_TX_STATE_WAIT_HARD_RESET) {
		tcpci_check_hard_reset_complete(tcpc, alert_status);
		alert_status &= ~TCPC_REG_ALERT_TX_MASK;
	}
#endif	/* CONFIG_USB_POWER_DELIVERY */

#ifndef CONFIG_USB_PD_DBG_SKIP_ALERT_HANDLER
	for (i = 0; i < ARRAY_SIZE(tcpci_alert_handlers); i++) {
		if (tcpci_alert_handlers[i].bit_mask & alert_status) {
			if (tcpci_alert_handlers[i].handler != 0)
				tcpci_alert_handlers[i].handler(tcpc);
		}
	}
#endif /* CONFIG_USB_PD_DBG_SKIP_ALERT_HANDLER */

	/* unmask alert */
	rv = tcpci_set_alert_mask(tcpc, alert_mask);
	if (rv)
		return rv;

	if (tcpc->tcpc_flags & TCPC_FLAGS_ALERT_V10)
		return 0;

	tcpci_vbus_level_refresh(tcpc);
	tcpci_vbus_level_changed(tcpc);
	return 0;
}

/*
 * [BLOCK] TYPEC device changed
 */

static inline void tcpci_attach_wake_lock(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TCPC_ATTACH_WAKE_LOCK_TOUT
	__pm_wakeup_event(tcpc->attach_wake_lock,
		CONFIG_TCPC_ATTACH_WAKE_LOCK_TOUT);
#else
	__pm_stay_awake(tcpc->attach_wake_lock);
#endif	/* CONFIG_TCPC_ATTACH_WAKE_LOCK_TOUT */
}

int tcpci_set_wake_lock(
	struct tcpc_device *tcpc, bool pd_lock, bool user_lock)
{
	bool ori_lock, new_lock;

	if (tcpc->wake_lock_pd && tcpc->wake_lock_user)
		ori_lock = true;
	else
		ori_lock = false;

	if (pd_lock && user_lock)
		new_lock = true;
	else
		new_lock = false;

	if (new_lock != ori_lock) {
		if (new_lock) {
			TCPC_DBG("wake_lock=1\r\n");
			tcpci_attach_wake_lock(tcpc);
			if (tcpc->typec_watchdog)
				tcpci_set_intrst(tcpc, true);
		} else {
			TCPC_DBG("wake_lock=0\r\n");
			if (tcpc->typec_watchdog)
				tcpci_set_intrst(tcpc, false);
			__pm_relax(tcpc->attach_wake_lock);
		}
		return 1;
	}

	return 0;
}

static inline int tcpci_set_wake_lock_pd(
	struct tcpc_device *tcpc, bool pd_lock)
{
	uint8_t wake_lock_pd;

	mutex_lock(&tcpc->access_lock);

	wake_lock_pd = tcpc->wake_lock_pd;

	if (pd_lock)
		wake_lock_pd++;
	else if (wake_lock_pd > 0)
		wake_lock_pd--;

	if (wake_lock_pd == 0)
		__pm_wakeup_event(tcpc->dettach_temp_wake_lock, 5000);

	tcpci_set_wake_lock(tcpc, wake_lock_pd, tcpc->wake_lock_user);

	if (wake_lock_pd == 1)
		__pm_relax(tcpc->dettach_temp_wake_lock);

	tcpc->wake_lock_pd = wake_lock_pd;
	mutex_unlock(&tcpc->access_lock);
	return 0;
}

static inline int tcpci_report_usb_port_attached(struct tcpc_device *tcpc)
{
	TCPC_INFO("usb_port_attached\r\n");

	switch (tcpc->typec_attach_new) {
	case TYPEC_ATTACHED_SNK:
	case TYPEC_ATTACHED_CUSTOM_SRC:
	case TYPEC_ATTACHED_NORP_SRC:
		tcpc->dual_role_pr = DUAL_ROLE_PROP_PR_SNK;
		tcpc->dual_role_dr = DUAL_ROLE_PROP_DR_DEVICE;
		tcpc->dual_role_mode = DUAL_ROLE_PROP_MODE_UFP;
		tcpc->dual_role_vconn = DUAL_ROLE_PROP_VCONN_SUPPLY_NO;
		tcpc->typec_caps.type = TYPEC_PORT_SNK;
		tcpc->typec_caps.data = TYPEC_PORT_UFP;
		typec_set_data_role(tcpc->typec_port, TYPEC_DEVICE);
		typec_set_pwr_role(tcpc->typec_port, TYPEC_SINK);
		typec_set_vconn_role(tcpc->typec_port, TYPEC_SINK);
		break;
	case TYPEC_ATTACHED_SRC:
		tcpc->dual_role_pr = DUAL_ROLE_PROP_PR_SRC;
		tcpc->dual_role_dr = DUAL_ROLE_PROP_DR_HOST;
		tcpc->dual_role_mode = DUAL_ROLE_PROP_MODE_DFP;
		tcpc->dual_role_vconn = DUAL_ROLE_PROP_VCONN_SUPPLY_YES;
		tcpc->typec_caps.type = TYPEC_PORT_SRC;
		tcpc->typec_caps.data = TYPEC_PORT_DFP;
		typec_set_data_role(tcpc->typec_port, TYPEC_HOST);
		typec_set_pwr_role(tcpc->typec_port, TYPEC_SOURCE);
		typec_set_vconn_role(tcpc->typec_port, TYPEC_SOURCE);
		break;
	default:
		break;
	}
	/* set typec switch orientation */
	typec_set_orientation(tcpc->typec_port, tcpc->typec_polarity ?
			TYPEC_ORIENTATION_NORMAL : TYPEC_ORIENTATION_REVERSE);

	tcpci_set_wake_lock_pd(tcpc, true);

#ifdef CONFIG_USB_POWER_DELIVERY

#ifdef CONFIG_USB_PD_DISABLE_PE
	if (tcpc->disable_pe)
		return 0;
#endif	/* CONFIG_USB_PD_DISABLE_PE */

	/* MTK Only */
	if (tcpc->pd_inited_flag)
		pd_put_cc_attached_event(tcpc, tcpc->typec_attach_new);
#endif /* CONFIG_USB_POWER_DLEIVERY */

	return 0;
}

static inline int tcpci_report_usb_port_detached(struct tcpc_device *tcpc)
{
	TCPC_INFO("usb_port_detached\r\n");

	tcpc->typec_caps.type = TYPEC_PORT_DRP;
	tcpc->typec_caps.data = TYPEC_PORT_DRD;
	tcpc->dual_role_pr = DUAL_ROLE_PROP_PR_NONE;
	tcpc->dual_role_dr = DUAL_ROLE_PROP_DR_NONE;
	tcpc->dual_role_mode = DUAL_ROLE_PROP_MODE_NONE;
	tcpc->dual_role_vconn = DUAL_ROLE_PROP_VCONN_SUPPLY_NO;
	typec_set_data_role(tcpc->typec_port, TYPEC_DEVICE);
	typec_set_pwr_role(tcpc->typec_port, TYPEC_SINK);
	typec_set_vconn_role(tcpc->typec_port, TYPEC_SINK);
	typec_set_pwr_opmode(tcpc->typec_port, TYPEC_PWR_MODE_USB);
	/* set typec switch orientation */
	typec_set_orientation(tcpc->typec_port, TYPEC_ORIENTATION_NONE);

#ifdef CONFIG_USB_POWER_DELIVERY
	/* MTK Only */
	if (tcpc->pd_inited_flag)
		pd_put_cc_detached_event(tcpc);
	else {
		pd_event_buf_reset(tcpc);
		tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_PE_IDLE);
	}
#endif /* CONFIG_USB_POWER_DELIVERY */

	tcpci_set_wake_lock_pd(tcpc, false);
	return 0;
}

int tcpci_report_usb_port_changed(struct tcpc_device *tcpc)
{
	tcpci_notify_typec_state(tcpc);

	if (tcpc->typec_attach_old == TYPEC_UNATTACHED)
		tcpci_report_usb_port_attached(tcpc);
	else if (tcpc->typec_attach_new == TYPEC_UNATTACHED)
		tcpci_report_usb_port_detached(tcpc);
	else
		TCPC_DBG2("TCPC Attach Again\r\n");

	return 0;
}

/*
 * [BLOCK] TYPEC power control changed
 */

int tcpci_report_power_control_on(struct tcpc_device *tcpc)
{
	tcpci_set_wake_lock_pd(tcpc, true);

	mutex_lock(&tcpc->access_lock);

#ifdef CONFIG_TYPEC_CAP_AUTO_DISCHARGE

#ifdef CONFIG_TCPC_AUTO_DISCHARGE_EXT
	tcpci_enable_ext_discharge(tcpc, false);
#endif	/* CONFIG_TCPC_AUTO_DISCHARGE_EXT */

#ifdef CONFIG_TCPC_AUTO_DISCHARGE_IC
	tcpci_enable_auto_discharge(tcpc, true);
#endif	/* CONFIG_TCPC_AUTO_DISCHARGE_IC */

	tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_AUTO_DISCHARGE);
#endif	/* CONFIG_TYPEC_CAP_AUTO_DISCHARGE */

	mutex_unlock(&tcpc->access_lock);

	return 0;
}

int tcpci_report_power_control_off(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->access_lock);

#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#ifdef CONFIG_TCPC_FORCE_DISCHARGE_IC
	__tcpci_enable_force_discharge(tcpc, false, 0);
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */
#endif	/* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_TYPEC_CAP_AUTO_DISCHARGE

#ifdef CONFIG_TCPC_AUTO_DISCHARGE_EXT
	tcpci_enable_ext_discharge(tcpc, true);
#endif	/* CONFIG_TCPC_AUTO_DISCHARGE_EXT */

	tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_AUTO_DISCHARGE);
#endif	/* CONFIG_TYPEC_CAP_AUTO_DISCHARGE */

	mutex_unlock(&tcpc->access_lock);

	tcpci_set_wake_lock_pd(tcpc, false);
	return 0;
}

int tcpci_report_power_control(struct tcpc_device *tcpc, bool en)
{
	if (tcpc->typec_power_ctrl == en)
		return 0;

	tcpc->typec_power_ctrl = en;

	if (en)
		tcpci_report_power_control_on(tcpc);
	else
		tcpci_report_power_control_off(tcpc);

	return 0;
}
