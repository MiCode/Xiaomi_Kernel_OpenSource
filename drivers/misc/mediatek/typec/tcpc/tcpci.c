/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "inc/tcpci.h"
#include <linux/time.h>

#define TCPC_NOTIFY_OVERTIME	(20) /* ms */

static int tcpc_check_notify_time(struct tcpc_device *tcpc,
	struct tcp_notify *tcp_noti, uint8_t type, uint8_t state)
{
	int ret;
#ifdef CONFIG_PD_BEGUG_ON
	struct timeval begin, end;
	int timeval = 0;

	do_gettimeofday(&begin);
	ret = srcu_notifier_call_chain(&tcpc->evt_nh[type], state, tcp_noti);
	do_gettimeofday(&end);
	timeval = (timeval_to_ns(end) - timeval_to_ns(begin))/1000/1000;
	PD_BUG_ON(timeval > TCPC_NOTIFY_OVERTIME);
#else
	ret = srcu_notifier_call_chain(&tcpc->evt_nh[type], state, tcp_noti);
#endif
	return ret;
}

int tcpci_check_vbus_valid_from_ic(struct tcpc_device *tcpc)
{
	uint16_t power_status;
	int vbus_level = tcpc->vbus_level;

	if (tcpci_get_power_status(tcpc, &power_status) == 0) {
		if (vbus_level != tcpc->vbus_level) {
			TCPC_INFO("[Warning] ps_chagned %d ->%d\r\n",
				vbus_level, tcpc->vbus_level);
		}
	}

	return tcpci_check_vbus_valid(tcpc);
}

int tcpci_check_vsafe0v(
	struct tcpc_device *tcpc, bool detect_en)
{
	int ret = 0;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = (tcpc->vbus_level == TCPC_VBUS_SAFE0V);
#else
	ret = (tcpc->vbus_level == TCPC_VBUS_INVALID);
#endif

	return ret;
}

int tcpci_alert_status_clear(
	struct tcpc_device *tcpc, uint32_t mask)
{
	PD_BUG_ON(tcpc->ops->alert_status_clear == NULL);

	return tcpc->ops->alert_status_clear(tcpc, mask);
}

int tcpci_fault_status_clear(
	struct tcpc_device *tcpc, uint8_t status)
{
	PD_BUG_ON(tcpc->ops->fault_status_clear == NULL);

	return tcpc->ops->fault_status_clear(tcpc, status);
}

int tcpci_get_alert_mask(
	struct tcpc_device *tcpc, uint32_t *mask)
{
	PD_BUG_ON(tcpc->ops->get_alert_mask == NULL);

	return tcpc->ops->get_alert_mask(tcpc, mask);
}

int tcpci_get_alert_status(
	struct tcpc_device *tcpc, uint32_t *alert)
{
	PD_BUG_ON(tcpc->ops->get_alert_status == NULL);

	return tcpc->ops->get_alert_status(tcpc, alert);
}

int tcpci_get_fault_status(
	struct tcpc_device *tcpc, uint8_t *fault)
{
	if (tcpc->ops->get_fault_status)
		return tcpc->ops->get_fault_status(tcpc, fault);

	*fault = 0;
	return 0;
}

int tcpci_get_power_status(
	struct tcpc_device *tcpc, uint16_t *pw_status)
{
	int ret;

	PD_BUG_ON(tcpc->ops->get_power_status == NULL);

	ret = tcpc->ops->get_power_status(tcpc, pw_status);
	if (ret < 0)
		return ret;

	tcpci_vbus_level_init(tcpc, *pw_status);
	return 0;
}

int tcpci_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	uint16_t power_status;

	PD_BUG_ON(tcpc->ops->init == NULL);

	ret = tcpc->ops->init(tcpc, sw_reset);
	if (ret < 0)
		return ret;

	return tcpci_get_power_status(tcpc, &power_status);
}

int tcpci_init_alert_mask(struct tcpc_device *tcpc)
{
	PD_BUG_ON(tcpc->ops->init_alert_mask == NULL);

	return tcpc->ops->init_alert_mask(tcpc);
}

int tcpci_get_cc(struct tcpc_device *tcpc)
{
	int ret;
	int cc1, cc2;

	PD_BUG_ON(tcpc->ops->get_cc == NULL);

	ret = tcpc->ops->get_cc(tcpc, &cc1, &cc2);
	if (ret < 0)
		return ret;

	if ((cc1 == tcpc->typec_remote_cc[0]) &&
			(cc2 == tcpc->typec_remote_cc[1])) {
		return 0;
	}

	tcpc->typec_remote_cc[0] = cc1;
	tcpc->typec_remote_cc[1] = cc2;

	return 1;
}

int tcpci_set_cc(struct tcpc_device *tcpc, int pull)
{
	PD_BUG_ON(tcpc->ops->set_cc == NULL);

#ifdef CONFIG_USB_PD_DBG_ALWAYS_LOCAL_RP
	if (pull == TYPEC_CC_RP)
		pull = tcpc->typec_local_rp_level;
#endif /* CONFIG_USB_PD_DBG_ALWAYS_LOCAL_RP */

#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE
	if (pull == TYPEC_CC_DRP && tcpc->typec_legacy_cable) {
#ifdef CONFIG_TYPEC_CHECK_LEGACY_CABLE2
		if (tcpc->typec_legacy_cable == 2)
			pull = TYPEC_CC_RP;
		else if (tcpc->typec_legacy_retry_wk > 1)
			pull = TYPEC_CC_RP_3_0;
		else
#endif	/* CONFIG_TYPEC_CHECK_LEGACY_CABLE2 */
			pull = TYPEC_CC_RP_1_5;
		TCPC_DBG2("LC->Toggling (%d)\r\n", pull);
	}
#endif /* CONFIG_TYPEC_CHECK_LEGACY_CABLE */

	if (pull & TYPEC_CC_DRP) {
		tcpc->typec_remote_cc[0] =
		tcpc->typec_remote_cc[1] =
			TYPEC_CC_DRP_TOGGLING;
	}

	tcpc->typec_local_cc = pull;
	return tcpc->ops->set_cc(tcpc, pull);
}

int tcpci_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	PD_BUG_ON(tcpc->ops->set_polarity == NULL);

	return tcpc->ops->set_polarity(tcpc, polarity);
}

int tcpci_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
#ifdef CONFIG_TYPEC_CAP_LOW_RP_DUTY
	if (low_rp)
		TCPC_INFO("low_rp_duty\r\n");

	if (tcpc->ops->set_low_rp_duty)
		return tcpc->ops->set_low_rp_duty(tcpc, low_rp);
#endif	/* CONFIG_TYPEC_CAP_LOW_RP_DUTY */

	return 0;
}

int tcpci_set_vconn(struct tcpc_device *tcpc, int enable)
{
#ifdef CONFIG_TCPC_SOURCE_VCONN
	struct tcp_notify tcp_noti;

	if (tcpc->tcpc_source_vconn == enable)
		return 0;

	tcpc->tcpc_source_vconn = enable;

	tcp_noti.en_state.en = enable != 0;
	tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_VBUS, TCP_NOTIFY_SOURCE_VCONN);

	if (tcpc->ops->set_vconn)
		return tcpc->ops->set_vconn(tcpc, enable);
#endif	/* CONFIG_TCPC_SOURCE_VCONN */

	return 0;
}

int tcpci_is_low_power_mode(struct tcpc_device *tcpc)
{
	int rv = 1;

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	if (tcpc->ops->is_low_power_mode)
		rv = tcpc->ops->is_low_power_mode(tcpc);
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

	return rv;
}

int tcpci_set_low_power_mode(
	struct tcpc_device *tcpc, bool en, int pull)
{
	int rv = 0;

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	if (tcpc->ops->set_low_power_mode)
		rv = tcpc->ops->set_low_power_mode(tcpc, en, pull);
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

	return rv;
}

int tcpci_idle_poll_ctrl(
	struct tcpc_device *tcpc, bool en, bool lock)
{
	int rv = 0;

#ifdef CONFIG_TCPC_IDLE_MODE
	bool update_mode = false;

	if (lock)
		mutex_lock(&tcpc->access_lock);

	if (en) {
		if (tcpc->tcpc_busy_cnt == 0)
			update_mode = true;
		tcpc->tcpc_busy_cnt++;
	} else {	/* idle mode */
		if (tcpc->tcpc_busy_cnt <= 0)
			TCPC_DBG2("tcpc_busy_cnt<=0\r\n");
		else
			tcpc->tcpc_busy_cnt--;

		if (tcpc->tcpc_busy_cnt == 0)
			update_mode = true;
	}

	if (lock)
		mutex_unlock(&tcpc->access_lock);

	if (update_mode && tcpc->ops->set_idle_mode)
		rv = tcpc->ops->set_idle_mode(tcpc, !en);
#endif

	return rv;
}

int tcpci_set_watchdog(struct tcpc_device *tcpc, bool en)
{
	int rv = 0;

	if (tcpc->tcpc_flags & TCPC_FLAGS_WATCHDOG_EN)
		if (tcpc->ops->set_watchdog)
			rv = tcpc->ops->set_watchdog(tcpc, en);

	return rv;
}

int tcpci_alert_vendor_defined_handler(struct tcpc_device *tcpc)
{
	int rv = 0;

	if (tcpc->ops->alert_vendor_defined_handler)
		rv = tcpc->ops->alert_vendor_defined_handler(tcpc);

	return rv;
}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
int tcpci_is_vsafe0v(struct tcpc_device *tcpc)
{
	int rv = -ENOTSUPP;

	if (tcpc->ops->is_vsafe0v)
		rv = tcpc->ops->is_vsafe0v(tcpc);

	return rv;
}
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_WATER_DETECTION
int tcpci_is_water_detected(struct tcpc_device *tcpc)
{
	if (tcpc->ops->is_water_detected)
		return tcpc->ops->is_water_detected(tcpc);
	return 0;
}

int tcpci_set_water_protection(struct tcpc_device *tcpc, bool en)
{
	if (tcpc->ops->set_water_protection)
		return tcpc->ops->set_water_protection(tcpc, en);
	return 0;
}

int tcpci_set_usbid_polling(struct tcpc_device *tcpc, bool en)
{
	if (tcpc->ops->set_usbid_polling)
		return tcpc->ops->set_usbid_polling(tcpc, en);
	return 0;
}

int tcpci_notify_wd_status(struct tcpc_device *tcpc, bool water_detected)
{
	struct tcp_notify tcp_noti;

	tcp_noti.wd_status.water_detected = water_detected;
	return tcpc_check_notify_time(tcpc, &tcp_noti, TCP_NOTIFY_IDX_MISC,
				      TCP_NOTIFY_WD_STATUS);
}
#endif /* CONFIG_WATER_DETECTION */

#ifdef CONFIG_CABLE_TYPE_DETECTION
int tcpci_notify_cable_type(struct tcpc_device *tcpc)
{
	struct tcp_notify tcp_noti;

	tcp_noti.cable_type.type = tcpc->typec_cable_type;
	return tcpc_check_notify_time(tcpc, &tcp_noti, TCP_NOTIFY_IDX_MISC,
				      TCP_NOTIFY_CABLE_TYPE);
}
#endif /* CONFIG_CABLE_TYPE_DETECTION */

#ifdef CONFIG_USB_POWER_DELIVERY

int tcpci_set_msg_header(struct tcpc_device *tcpc,
	uint8_t power_role, uint8_t data_role)
{
	PD_BUG_ON(tcpc->ops->set_msg_header == NULL);

	return tcpc->ops->set_msg_header(tcpc, power_role, data_role);
}

int tcpci_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
	PD_BUG_ON(tcpc->ops->set_rx_enable == NULL);

	return tcpc->ops->set_rx_enable(tcpc, enable);
}

int tcpci_protocol_reset(struct tcpc_device *tcpc)
{
	if (tcpc->ops->protocol_reset)
		return tcpc->ops->protocol_reset(tcpc);

	return 0;
}

int tcpci_get_message(struct tcpc_device *tcpc,
	uint32_t *payload, uint16_t *head, enum tcpm_transmit_type *type)
{
	PD_BUG_ON(tcpc->ops->get_message == NULL);

	return tcpc->ops->get_message(tcpc, payload, head, type);
}

int tcpci_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
	PD_BUG_ON(tcpc->ops->transmit == NULL);

	return tcpc->ops->transmit(tcpc, type, header, data);
}

int tcpci_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	if (tcpc->ops->set_bist_test_mode)
		return tcpc->ops->set_bist_test_mode(tcpc, en);

	return 0;
}

int tcpci_set_bist_carrier_mode(struct tcpc_device *tcpc, uint8_t pattern)
{
	PD_BUG_ON(tcpc->ops->set_bist_carrier_mode == NULL);

	if (pattern)	/* wait for GoodCRC */
		udelay(240);

	return tcpc->ops->set_bist_carrier_mode(tcpc, pattern);
}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
int tcpci_retransmit(struct tcpc_device *tcpc)
{
	PD_BUG_ON(tcpc->ops->retransmit == NULL);

	return tcpc->ops->retransmit(tcpc);
}
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
#endif	/* CONFIG_USB_POWER_DELIVERY */

int tcpci_notify_typec_state(struct tcpc_device *tcpc)
{
	struct tcp_notify tcp_noti;
	int ret;

	tcp_noti.typec_state.polarity = tcpc->typec_polarity;
	tcp_noti.typec_state.old_state = tcpc->typec_attach_old;
	tcp_noti.typec_state.new_state = tcpc->typec_attach_new;
	tcp_noti.typec_state.rp_level = tcpc->typec_remote_rp_level;

	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_USB, TCP_NOTIFY_TYPEC_STATE);
	return ret;
}

int tcpci_notify_role_swap(
	struct tcpc_device *tcpc, uint8_t event, uint8_t role)
{
	struct tcp_notify tcp_noti;
	int ret;

	tcp_noti.swap_state.new_role = role;
	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MISC, event);
	return ret;
}

int tcpci_notify_pd_state(struct tcpc_device *tcpc, uint8_t connect)
{
	struct tcp_notify tcp_noti;
	int ret;

	tcp_noti.pd_state.connected = connect;
	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_USB, TCP_NOTIFY_PD_STATE);
	return ret;
}

int tcpci_set_intrst(struct tcpc_device *tcpc, bool en)
{
#ifdef CONFIG_TCPC_INTRST_EN
	if (tcpc->ops->set_intrst)
		tcpc->ops->set_intrst(tcpc, en);
#endif	/* CONFIG_TCPC_INTRST_EN */

	return 0;
}

int tcpci_enable_watchdog(struct tcpc_device *tcpc, bool en)
{
	if (!(tcpc->tcpc_flags & TCPC_FLAGS_WATCHDOG_EN))
		return 0;

	TCPC_DBG2("enable_WG: %d\r\n", en);

	if (tcpc->typec_watchdog == en)
		return 0;

	mutex_lock(&tcpc->access_lock);
	tcpc->typec_watchdog = en;

	if (tcpc->ops->set_watchdog)
		tcpc->ops->set_watchdog(tcpc, en);

#ifdef CONFIG_TCPC_INTRST_EN
	if (!en || tcpc->attach_wake_lock.active)
		tcpci_set_intrst(tcpc, en);
#endif	/* CONFIG_TCPC_INTRST_EN */

	mutex_unlock(&tcpc->access_lock);

	return 0;
}

int tcpci_source_vbus(
	struct tcpc_device *tcpc, uint8_t type, int mv, int ma)
{
	struct tcp_notify tcp_noti;
	int ret;

#ifdef CONFIG_USB_POWER_DELIVERY
	if (type >= TCP_VBUS_CTRL_PD &&
			tcpc->pd_port.pe_data.pd_prev_connected)
		type |= TCP_VBUS_CTRL_PD_DETECT;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	if (ma < 0) {
		if (mv != 0) {
			switch (tcpc->typec_local_rp_level) {
			case TYPEC_CC_RP_1_5:
				ma = 1500;
				break;
			case TYPEC_CC_RP_3_0:
				ma = 3000;
				break;
			default:
			case TYPEC_CC_RP_DFT:
				ma = CONFIG_TYPEC_SRC_CURR_DFT;
				break;
			}
		} else
			ma = 0;
	}

	tcp_noti.vbus_state.ma = ma;
	tcp_noti.vbus_state.mv = mv;
	tcp_noti.vbus_state.type = type;

	tcpci_enable_watchdog(tcpc, mv != 0);
	TCPC_DBG("source_vbus: %d mV, %d mA\r\n", mv, ma);
	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_VBUS, TCP_NOTIFY_SOURCE_VBUS);
	return ret;
}

int tcpci_sink_vbus(
	struct tcpc_device *tcpc, uint8_t type, int mv, int ma)
{
	struct tcp_notify tcp_noti;
	int ret;

#ifdef CONFIG_USB_POWER_DELIVERY
	if (type >= TCP_VBUS_CTRL_PD &&
			tcpc->pd_port.pe_data.pd_prev_connected)
		type |= TCP_VBUS_CTRL_PD_DETECT;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	if (ma < 0) {
		if (mv != 0) {
			switch (tcpc->typec_remote_rp_level) {
			case TYPEC_CC_VOLT_SNK_1_5:
				ma = 1500;
				break;
			case TYPEC_CC_VOLT_SNK_3_0:
				ma = 3000;
				break;
			default:
			case TYPEC_CC_VOLT_SNK_DFT:
				ma = tcpc->typec_usb_sink_curr;
				break;
			}
#if CONFIG_TYPEC_SNK_CURR_LIMIT > 0
		if (ma > CONFIG_TYPEC_SNK_CURR_LIMIT)
			ma = CONFIG_TYPEC_SNK_CURR_LIMIT;
#endif	/* CONFIG_TYPEC_SNK_CURR_LIMIT */
		} else
			ma = 0;
	}

	tcp_noti.vbus_state.ma = ma;
	tcp_noti.vbus_state.mv = mv;
	tcp_noti.vbus_state.type = type;

	TCPC_DBG("sink_vbus: %d mV, %d mA\r\n", mv, ma);
	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_VBUS, TCP_NOTIFY_SINK_VBUS);
	return ret;
}

int tcpci_disable_vbus_control(struct tcpc_device *tcpc)
{
#ifdef CONFIG_TYPEC_USE_DIS_VBUS_CTRL
	struct tcp_notify tcp_noti;
	int ret;

	TCPC_DBG("disable_vbus\r\n");
	tcpci_enable_watchdog(tcpc, false);

	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_VBUS, TCP_NOTIFY_DIS_VBUS_CTRL);
	return ret;
#else
	tcpci_sink_vbus(tcpc, TCP_VBUS_CTRL_REMOVE, TCPC_VBUS_SINK_0V, 0);
	tcpci_source_vbus(tcpc, TCP_VBUS_CTRL_REMOVE, TCPC_VBUS_SOURCE_0V, 0);
	return 0;
#endif	/* CONFIG_TYPEC_USE_DIS_VBUS_CTRL */
}

int tcpci_notify_attachwait_state(struct tcpc_device *tcpc, bool as_sink)
{
#ifdef CONFIG_TYPEC_NOTIFY_ATTACHWAIT
	uint8_t notify = 0;
	struct tcp_notify tcp_noti;
	int ret;

#ifdef CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK
	if (as_sink)
		notify = TCP_NOTIFY_ATTACHWAIT_SNK;
#endif	/* CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SNK */

#ifdef CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SRC
	if (!as_sink)
		notify = TCP_NOTIFY_ATTACHWAIT_SRC;
#endif	/* CONFIG_TYPEC_NOTIFY_ATTACHWAIT_SRC */

	if (notify == 0)
		return 0;

	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_VBUS, notify);
	return ret;
#else
	return 0;
#endif	/* CONFIG_TYPEC_NOTIFY_ATTACHWAIT */

}

int tcpci_enable_ext_discharge(struct tcpc_device *tcpc, bool en)
{
	int ret = 0;

#ifdef CONFIG_TCPC_EXT_DISCHARGE
	struct tcp_notify tcp_noti;

	mutex_lock(&tcpc->access_lock);

	if (tcpc->typec_ext_discharge != en) {
		tcpc->typec_ext_discharge = en;
		tcp_noti.en_state.en = en;
		TCPC_DBG("EXT-Discharge: %d\r\n", en);
		ret = tcpc_check_notify_time(tcpc, &tcp_noti,
			TCP_NOTIFY_IDX_VBUS, TCP_NOTIFY_EXT_DISCHARGE);
	}

	mutex_unlock(&tcpc->access_lock);
#endif	/* CONFIG_TCPC_EXT_DISCHARGE */

	return ret;
}

int tcpci_enable_auto_discharge(struct tcpc_device *tcpc, bool en)
{
	int ret = 0;

#ifdef CONFIG_TYPEC_CAP_AUTO_DISCHARGE
#ifdef CONFIG_TCPC_AUTO_DISCHARGE_IC
	if (tcpc->typec_auto_discharge != en) {
		tcpc->typec_auto_discharge = en;
		if (tcpc->ops->set_auto_discharge)
			ret = tcpc->ops->set_auto_discharge(tcpc, en);
	}
#endif	/* CONFIG_TCPC_AUTO_DISCHARGE_IC */
#endif	/* CONFIG_TYPEC_CAP_AUTO_DISCHARGE */

	return ret;
}

int tcpci_enable_force_discharge(struct tcpc_device *tcpc, int mv)
{
	int ret = 0;

#ifdef CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#ifdef CONFIG_TCPC_FORCE_DISCHARGE_IC
	if (!tcpc->pd_force_discharge) {
		tcpc->pd_force_discharge = true;
		if (tcpc->ops->set_force_discharge)
			ret = tcpc->ops->set_force_discharge(tcpc, true, mv);
	}
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */

#ifdef CONFIG_TCPC_FORCE_DISCHARGE_EXT
	ret = tcpci_enable_ext_discharge(tcpc, true);
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_EXT */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */

	return ret;
}

int tcpci_disable_force_discharge(struct tcpc_device *tcpc)
{
	int ret = 0;

#ifdef CONFIG_TYPEC_CAP_FORCE_DISCHARGE
#ifdef CONFIG_TCPC_FORCE_DISCHARGE_IC
	if (tcpc->pd_force_discharge) {
		tcpc->pd_force_discharge = false;
		if (tcpc->ops->set_force_discharge)
			ret = tcpc->ops->set_force_discharge(tcpc, false, 0);
	}
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_IC */

#ifdef CONFIG_TCPC_FORCE_DISCHARGE_EXT
	ret = tcpci_enable_ext_discharge(tcpc, false);
#endif	/* CONFIG_TCPC_FORCE_DISCHARGE_EXT */
#endif	/* CONFIG_TYPEC_CAP_FORCE_DISCHARGE */

	return ret;
}

#ifdef CONFIG_USB_POWER_DELIVERY

int tcpci_notify_hard_reset_state(struct tcpc_device *tcpc, uint8_t state)
{
	struct tcp_notify tcp_noti;
	int ret;

	tcp_noti.hreset_state.state = state;

	if (state >= TCP_HRESET_SIGNAL_SEND)
		tcpc->pd_wait_hard_reset_complete = true;
	else if (tcpc->pd_wait_hard_reset_complete)
		tcpc->pd_wait_hard_reset_complete = false;
	else
		return 0;

	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MISC, TCP_NOTIFY_HARD_RESET_STATE);
	return ret;
}

int tcpci_enter_mode(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops, uint32_t mode)
{
	struct tcp_notify tcp_noti;
	int ret;

	tcp_noti.mode_ctrl.svid = svid;
	tcp_noti.mode_ctrl.ops = ops;
	tcp_noti.mode_ctrl.mode = mode;

	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_ENTER_MODE);
	return ret;
}

int tcpci_exit_mode(struct tcpc_device *tcpc, uint16_t svid)
{
	struct tcp_notify tcp_noti;
	int ret;

	tcp_noti.mode_ctrl.svid = svid;
	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_EXIT_MODE);
	return ret;

}

#ifdef CONFIG_USB_PD_ALT_MODE

int tcpci_report_hpd_state(struct tcpc_device *tcpc, uint32_t dp_status)
{
	struct tcp_notify tcp_noti;
	struct dp_data *dp_data = pd_get_dp_data(&tcpc->pd_port);

	/* UFP_D to DFP_D only */

	if (PD_DP_CFG_DFP_D(dp_data->local_config)) {
		tcp_noti.ama_dp_hpd_state.irq = PD_VDO_DPSTS_HPD_IRQ(dp_status);
		tcp_noti.ama_dp_hpd_state.state =
					PD_VDO_DPSTS_HPD_LVL(dp_status);
		tcpc_check_notify_time(tcpc, &tcp_noti,
			TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_AMA_DP_HPD_STATE);
	}

	return 0;
}

int tcpci_dp_status_update(struct tcpc_device *tcpc, uint32_t dp_status)
{
	DP_INFO("Status0: 0x%x\r\n", dp_status);
	tcpci_report_hpd_state(tcpc, dp_status);
	return 0;
}

int tcpci_dp_configure(struct tcpc_device *tcpc, uint32_t dp_config)
{
	struct tcp_notify tcp_noti;
	int ret;

	DP_INFO("LocalCFG: 0x%x\r\n", dp_config);

	switch (dp_config & 0x03) {
	case 0:
		tcp_noti.ama_dp_state.sel_config = SW_USB;
		break;
	case MODE_DP_SNK:
		tcp_noti.ama_dp_state.sel_config = SW_UFP_D;
		tcp_noti.ama_dp_state.pin_assignment = (dp_config >> 16) & 0xff;
		break;
	case MODE_DP_SRC:
		tcp_noti.ama_dp_state.sel_config = SW_DFP_D;
		tcp_noti.ama_dp_state.pin_assignment = (dp_config >> 8) & 0xff;
		break;
	}

	tcp_noti.ama_dp_state.signal = (dp_config >> 2) & 0x0f;
	tcp_noti.ama_dp_state.polarity = tcpc->typec_polarity;
	tcp_noti.ama_dp_state.active = 1;
	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_AMA_DP_STATE);
	return ret;
}

int tcpci_dp_attention(struct tcpc_device *tcpc, uint32_t dp_status)
{
	/* DFP_U : Not call this function during internal flow */
	struct tcp_notify tcp_noti;

	DP_INFO("Attention: 0x%x\r\n", dp_status);
	tcp_noti.ama_dp_attention.state = (uint8_t) dp_status;
	tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_AMA_DP_ATTENTION);
	return tcpci_report_hpd_state(tcpc, dp_status);
}

int tcpci_dp_notify_status_update_done(
	struct tcpc_device *tcpc, uint32_t dp_status, bool ack)
{
	/* DFP_U : Not call this function during internal flow */
	DP_INFO("Status1: 0x%x, ack=%d\r\n", dp_status, ack);
	return 0;
}

int tcpci_dp_notify_config_start(struct tcpc_device *tcpc)
{
	/* DFP_U : Put signal & mux into the Safe State */
	struct tcp_notify tcp_noti;

	DP_INFO("ConfigStart\r\n");
	tcp_noti.ama_dp_state.sel_config = SW_USB;
	tcp_noti.ama_dp_state.active = 0;
	tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_AMA_DP_STATE);
	return 0;
}

int tcpci_dp_notify_config_done(struct tcpc_device *tcpc,
	uint32_t local_cfg, uint32_t remote_cfg, bool ack)
{
	/* DFP_U : If DP success,
	 * internal flow will enter this function finally
	 */
	DP_INFO("ConfigDone, L:0x%x, R:0x%x, ack=%d\r\n",
		local_cfg, remote_cfg, ack);

	if (ack)
		tcpci_dp_configure(tcpc, local_cfg);

	return 0;
}

#endif	/* CONFIG_USB_PD_ALT_MODE */

#ifdef CONFIG_USB_PD_CUSTOM_VDM
int tcpci_notify_uvdm(struct tcpc_device *tcpc, bool ack)
{
	struct tcp_notify tcp_noti;
	struct pd_port *pd_port = &tcpc->pd_port;

	tcp_noti.uvdm_msg.ack = ack;

	if (ack) {
		tcp_noti.uvdm_msg.uvdm_cnt = pd_port->uvdm_cnt;
		tcp_noti.uvdm_msg.uvdm_svid = pd_port->uvdm_svid;
		tcp_noti.uvdm_msg.uvdm_data = pd_port->uvdm_data;
	}

	tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_UVDM);
	return 0;
}
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

#ifdef CONFIG_USB_PD_ALT_MODE_RTDC
int tcpci_dc_notify_en_unlock(struct tcpc_device *tcpc)
{
	struct tcp_notify tcp_noti;
	int ret;

	DC_INFO("DirectCharge en_unlock\r\n");
	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MODE, TCP_NOTIFY_DC_EN_UNLOCK);
	return ret;
}
#endif	/* CONFIG_USB_PD_ALT_MODE_RTDC */

/* ---- Policy Engine (PD30) ---- */

#ifdef CONFIG_USB_PD_REV30

#ifdef CONFIG_USB_PD_REV30_ALERT_REMOTE
int tcpci_notify_alert(struct tcpc_device *tcpc, uint32_t ado)
{
	struct tcp_notify tcp_noti;
	int ret;

	tcp_noti.alert_msg.ado = ado;
	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MISC, TCP_NOTIFY_ALERT);
	return ret;
}
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */

#ifdef CONFIG_USB_PD_REV30_STATUS_REMOTE
int tcpci_notify_status(struct tcpc_device *tcpc, struct pd_status *sdb)
{
	struct tcp_notify tcp_noti;
	int ret;

	tcp_noti.status_msg.sdb = sdb;
	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MISC, TCP_NOTIFY_STATUS);
	return ret;
}
#endif	/* CONFIG_USB_PD_REV30_STATUS_REMOTE */

#ifdef CONFIG_USB_PD_REV30_BAT_INFO
int tcpci_notify_request_bat_info(
	struct tcpc_device *tcpc, enum pd_battery_reference ref)
{
	struct tcp_notify tcp_noti;
	int ret;

	tcp_noti.request_bat.ref = ref;
	ret = tcpc_check_notify_time(tcpc, &tcp_noti,
		TCP_NOTIFY_IDX_MISC, TCP_NOTIFY_REQUEST_BAT_INFO);
	return ret;
}
#endif	/* CONFIG_USB_PD_REV30_BAT_INFO */

#endif	/* CONFIG_USB_PD_REV30 */

#endif	/* CONFIG_USB_POWER_DELIVERY */

