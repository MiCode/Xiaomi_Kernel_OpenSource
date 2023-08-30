/*
 * Copyright (C) 2020 Richtek Inc.
 *
 * Power Delivery Policy Engine for Common
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

#include <linux/usb/tcpc/pd_core.h>
#include <linux/usb/tcpc/tcpci_event.h>
#include <linux/usb/tcpc/pd_process_evt.h>
#include <linux/usb/tcpc/pd_dpm_core.h>

/*
 * [BLOCK] DRP (dr_swap, pr_swap, vconn_swap)
 */

#ifdef CONFIG_USB_PD_DR_SWAP
static inline bool pd_evaluate_reject_dr_swap(struct pd_port *pd_port)
{
	if (pd_port->dpm_caps & DPM_CAP_LOCAL_DR_DATA) {
		if (pd_port->power_role == PD_ROLE_DFP)
			return pd_port->dpm_caps &
				DPM_CAP_DR_SWAP_REJECT_AS_UFP;
		return pd_port->dpm_caps & DPM_CAP_DR_SWAP_REJECT_AS_DFP;
	}

	return true;
}
#endif	/* CONFIG_USB_PD_DR_SWAP */

#ifdef CONFIG_USB_PD_PR_SWAP
static inline bool pd_evaluate_reject_pr_swap(struct pd_port *pd_port)
{
	if (pd_port->dpm_caps & DPM_CAP_LOCAL_DR_POWER) {
		if (pd_port->power_role == PD_ROLE_SOURCE)
			return pd_port->dpm_caps &
				DPM_CAP_PR_SWAP_REJECT_AS_SNK;
		return pd_port->dpm_caps & DPM_CAP_PR_SWAP_REJECT_AS_SRC;
	}

	return true;
}
#endif	/* CONFIG_USB_PD_PR_SWAP */

#ifdef CONFIG_USB_PD_VCONN_SWAP
static inline bool pd_evaluate_accept_vconn_swap(struct pd_port *pd_port)
{
	if (pd_port->dpm_caps & DPM_CAP_LOCAL_VCONN_SUPPLY)
		return true;

	return false;
}
#endif	/* CONFIG_USB_PD_VCONN_SWAP */

static inline bool pd_process_ctrl_msg_dr_swap(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
	if (pd_port->pe_data.modal_operation) {
		pe_transit_hard_reset_state(pd_port);
		return true;
	}

#ifdef CONFIG_USB_PD_DR_SWAP
	if (!pd_check_pe_state_ready(pd_port))
		return false;

	if (!pd_evaluate_reject_dr_swap(pd_port)) {
		pd_port->pe_data.during_swap = false;
		pd_port->state_machine = PE_STATE_MACHINE_DR_SWAP;

		PE_TRANSIT_DATA_STATE(pd_port,
			PE_DRS_UFP_DFP_EVALUATE_DR_SWAP,
			PE_DRS_DFP_UFP_EVALUATE_DR_SWAP);
		return true;
	}
#endif	/* CONFIG_USB_PD_DR_SWAP */

	PE_TRANSIT_STATE(pd_port, PE_REJECT);
	return true;
}

static inline bool pd_process_ctrl_msg_pr_swap(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
#ifdef CONFIG_USB_PD_PR_SWAP
	if (!pd_evaluate_reject_pr_swap(pd_port)) {
		pd_port->pe_data.during_swap = false;
		pd_port->state_machine = PE_STATE_MACHINE_PR_SWAP;
		pe_transit_evaluate_pr_swap_state(pd_port);
		return true;
	}
#endif	/* CONFIG_USB_PD_PR_SWAP */

	PE_TRANSIT_STATE(pd_port, PE_REJECT);
	return true;
}

static inline bool pd_process_ctrl_msg_vconn_swap(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
#ifdef CONFIG_USB_PD_VCONN_SWAP
	if (!pd_check_pe_state_ready(pd_port))
		return false;

	if (pd_evaluate_accept_vconn_swap(pd_port)) {
		pd_port->state_machine = PE_STATE_MACHINE_VCONN_SWAP;
		PE_TRANSIT_STATE(pd_port, PE_VCS_EVALUATE_SWAP);
		return true;
	}
#endif	/* CONFIG_USB_PD_VCONN_SWAP */

	if (!pd_check_rev30(pd_port)) {
		PE_TRANSIT_STATE(pd_port, PE_REJECT);
		return true;
	}

	return false;
}

/*
 * [BLOCK] BIST
 */

static inline bool pd_process_data_msg_bist(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_port->request_v > 5000) {
		PE_INFO("bist_not_vsafe5v\n");
		return false;
	}

	switch (BDO_MODE(pd_event->pd_msg->payload[0])) {
	case BDO_MODE_TEST_DATA:
		PE_DBG("bist_test\n");
		PE_TRANSIT_STATE(pd_port, PE_BIST_TEST_DATA);
		pd_noitfy_pe_bist_mode(pd_port, PD_BIST_MODE_TEST_DATA);
		return true;

	case BDO_MODE_CARRIER2:
		PE_DBG("bist_cm2\n");
		PE_TRANSIT_STATE(pd_port, PE_BIST_CARRIER_MODE_2);
		pd_noitfy_pe_bist_mode(pd_port, PD_BIST_MODE_DISABLE);
		return true;

	default:
#if 0
	case BDO_MODE_RECV:
	case BDO_MODE_TRANSMIT:
	case BDO_MODE_COUNTERS:
	case BDO_MODE_CARRIER0:
	case BDO_MODE_CARRIER1:
	case BDO_MODE_CARRIER3:
	case BDO_MODE_EYE:
#endif
		PE_DBG("Unsupport BIST\n");
		pd_noitfy_pe_bist_mode(pd_port, PD_BIST_MODE_DISABLE);
		return false;
	}

	return false;
}

/*
 * [BLOCK] Porcess Ctrl MSG
 */

static void pd_cancel_dpm_reaction(struct pd_port *pd_port)
{
	if (pd_port->pe_data.dpm_reaction_id < DPM_REACTION_REJECT_CANCEL)
		dpm_reaction_clear(pd_port, pd_port->pe_data.dpm_reaction_id);
}

static bool pd_process_ctrl_msg_wait_reject(struct pd_port *pd_port)
{
	pd_cancel_dpm_reaction(pd_port);

	if (pd_port->state_machine == PE_STATE_MACHINE_PR_SWAP)
		pd_notify_pe_cancel_pr_swap(pd_port);

	pe_transit_ready_state(pd_port);
	return true;
}

static inline bool pd_process_ctrl_msg_wait(struct pd_port *pd_port)
{

	return pd_process_ctrl_msg_wait_reject(pd_port);
}

static inline bool pd_process_ctrl_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool ret = false;

#ifdef CONFIG_USB_PD_REV30
	if (!pd_check_rev30(pd_port) &&
		pd_event->msg >= PD_CTRL_PD30_START) {
		pd_event->msg = PD_CTRL_MSG_NR;
		return false;
	}
#endif	/* CONFIG_USB_PD_REV30 */

	switch (pd_event->msg) {
	case PD_CTRL_GOOD_CRC:
		if (pd_port->pe_data.pe_state_flags &
			PE_STATE_FLAG_ENABLE_SENDER_RESPONSE_TIMER)
			pd_enable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);

		if (pd_port->pe_data.pe_state_flags2 &
			PE_STATE_FLAG_BACK_READY_IF_RECV_GOOD_CRC) {
			pe_transit_ready_state(pd_port);
			return true;
		}
		break;

	case PD_CTRL_REJECT:
		pd_notify_tcp_event_2nd_result(pd_port, TCP_DPM_RET_REJECT);

		if (pd_port->pe_data.pe_state_flags &
			PE_STATE_FLAG_BACK_READY_IF_RECV_REJECT) {
			return pd_process_ctrl_msg_wait_reject(pd_port);
		}
		break;
	case PD_CTRL_WAIT:
		pd_notify_tcp_event_2nd_result(pd_port, TCP_DPM_RET_WAIT);

		if (pd_port->pe_data.pe_state_flags &
			PE_STATE_FLAG_BACK_READY_IF_RECV_WAIT) {
			return pd_process_ctrl_msg_wait(pd_port);
		}
		break;

	case PD_CTRL_SOFT_RESET:
		if (!pd_port->pe_data.during_swap &&
			!pd_check_pe_during_hard_reset(pd_port)) {
			pe_transit_soft_reset_recv_state(pd_port);
			return true;
		}
		break;

	/* Swap */
	case PD_CTRL_DR_SWAP:
		ret = pd_process_ctrl_msg_dr_swap(pd_port, pd_event);
		break;

	case PD_CTRL_PR_SWAP:
		ret = pd_process_ctrl_msg_pr_swap(pd_port, pd_event);
		break;

	case PD_CTRL_VCONN_SWAP:
		ret = pd_process_ctrl_msg_vconn_swap(pd_port, pd_event);
		break;

#ifdef CONFIG_USB_PD_REV30

#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL
	case PD_CTRL_GET_COUNTRY_CODE:
		if (pd_port->country_nr) {
			ret = PE_MAKE_STATE_TRANSIT_SINGLE(
				pe_get_curr_ready_state(pd_port),
				PE_GIVE_COUNTRY_CODES);
		}
		break;
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL */

	case PD_CTRL_NOT_SUPPORTED:
		pd_cancel_dpm_reaction(pd_port);
		pd_notify_tcp_event_2nd_result(
				pd_port, TCP_DPM_RET_NOT_SUPPORT);

		if (pd_port->pe_data.pe_state_flags &
			PE_STATE_FLAG_BACK_READY_IF_SR_TIMER_TOUT) {
			pe_transit_ready_state(pd_port);
			return true;
		} else if (pd_port->pe_data.vdm_state_timer) {
			vdm_put_pe_event(
				pd_port->tcpc, PD_PE_VDM_NOT_SUPPORT);
		}
		break;
#endif	/* CONFIG_USB_PD_REV30 */
	}

	return ret;
}

/*
 * [BLOCK] Porcess Data MSG
 */

static inline bool pd_process_data_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool ret = false;
	uint8_t ready_state = pe_get_curr_ready_state(pd_port);

#ifdef CONFIG_USB_PD_REV30
	if (!pd_check_rev30(pd_port) &&
		pd_event->msg >= PD_DATA_PD30_START) {
		pd_event->msg = PD_DATA_MSG_NR;
		return false;
	}
#endif	/* CONFIG_USB_PD_REV30 */

	switch (pd_event->msg) {
	case PD_DATA_BIST:
		if (pd_port->pe_state_curr == ready_state)
			ret = pd_process_data_msg_bist(pd_port, pd_event);
		break;

#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE
	case PD_DATA_BAT_STATUS:
		ret = PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_GET_BATTERY_STATUS, ready_state);
		break;
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE */

#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL
	case PD_DATA_GET_COUNTRY_INFO:
		if (pd_port->country_nr) {
			ret = PE_MAKE_STATE_TRANSIT_SINGLE(
				ready_state, PE_GIVE_COUNTRY_INFO);
		}
		break;
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL */
#endif	/* CONFIG_USB_PD_REV30 */
	}

	return ret;
}

/*
 * [BLOCK] Porcess Extend MSG
 */

#ifdef CONFIG_USB_PD_REV30
static inline bool pd_process_ext_msg(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool ret = false;
	uint8_t ready_state = pe_get_curr_ready_state(pd_port);

	if (!pd_check_rev30(pd_port)) {
		pd_event->msg = PD_DATA_MSG_NR;
		return false;
	}

#ifndef CONFIG_USB_PD_REV30_CHUNKING_BY_PE
	if (pd_port->pe_state_curr == ready_state &&
		pd_is_multi_chunk_msg(pd_port)) {
		pd_port->curr_unsupported_msg = true;
		return pd_process_protocol_error(pd_port, pd_event);
	}
#endif	/* CONFIG_USB_PD_REV30_CHUNKING_BY_PE */

	switch (pd_event->msg) {

#ifdef CONFIG_USB_PD_REV30_BAT_CAP_LOCAL
	case PD_EXT_GET_BAT_CAP:
		if (pd_port->bat_nr) {
			ret = PE_MAKE_STATE_TRANSIT_SINGLE(
				ready_state, PE_GIVE_BATTERY_CAP);
		}
		break;
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_LOCAL */

#ifdef CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL
	case PD_EXT_GET_BAT_STATUS:
		if (pd_port->bat_nr) {
			ret = PE_MAKE_STATE_TRANSIT_SINGLE(
				ready_state, PE_GIVE_BATTERY_STATUS);
		}
		break;
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL */

#ifdef CONFIG_USB_PD_REV30_BAT_CAP_REMOTE
	case PD_EXT_BAT_CAP:
		ret = PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_GET_BATTERY_CAP, ready_state);
		break;
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_REMOTE */

#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL
	case PD_EXT_GET_MFR_INFO:
		ret = PE_MAKE_STATE_TRANSIT_SINGLE(
			ready_state, PE_GIVE_MANUFACTURER_INFO);
		break;
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL */

#ifdef CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE
	case PD_EXT_MFR_INFO:
		ret = PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_GET_MANUFACTURER_INFO, ready_state);
		break;
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE */

#ifdef CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE
	case PD_EXT_COUNTRY_INFO:
		ret = PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_GET_COUNTRY_INFO, ready_state);
		break;
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE */

#ifdef CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE
	case PD_EXT_COUNTRY_CODES:
		ret = PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_GET_COUNTRY_CODES, ready_state);
		break;
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE */
	}

	return ret;
}
#endif	/* CONFIG_USB_PD_REV30 */

/*
 * [BLOCK] Porcess DPM MSG
 */

static inline bool pd_process_dpm_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool ret = false;

	switch (pd_event->msg) {
	case PD_DPM_ACK:
		if (pd_port->pe_data.pe_state_flags2 &
			PE_STATE_FLAG_BACK_READY_IF_DPM_ACK) {
			pe_transit_ready_state(pd_port);
			return true;
		}
		break;

#ifdef CONFIG_USB_PD_REV30
	case PD_DPM_NOT_SUPPORT:
		if (pd_check_rev30(pd_port)) {
			PE_TRANSIT_STATE(pd_port, PE_VDM_NOT_SUPPORTED);
			return true;
		}
		break;
#endif	/* CONFIG_USB_PD_REV30 */
	}

	return ret;
}

/*
 * [BLOCK] Porcess HW MSG
 */

static inline bool pd_process_recv_hard_reset(
		struct pd_port *pd_port, struct pd_event *pd_event)
{
#ifdef CONFIG_USB_PD_RECV_HRESET_COUNTER
	if (pd_port->pe_data.recv_hard_reset_count > PD_HARD_RESET_COUNT) {
		PE_TRANSIT_STATE(pd_port, PE_OVER_RECV_HRESET_LIMIT);
		return true;
	}

	pd_port->pe_data.recv_hard_reset_count++;
#endif	/* CONFIG_USB_PD_RECV_HRESET_COUNTER */

#ifdef CONFIG_USB_PD_RENEGOTIATION_COUNTER
	if (pd_check_pe_during_hard_reset(pd_port))
		pd_port->pe_data.renegotiation_count++;
#endif	/* CONFIG_USB_PD_RENEGOTIATION_COUNTER */

	pe_transit_hard_reset_recv_state(pd_port);
	return true;
}

static inline bool pd_process_hw_msg_tx_failed(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
#ifdef CONFIG_USB_PD_RENEGOTIATION_COUNTER
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_port->pe_data.renegotiation_count > PD_HARD_RESET_COUNT) {
		PE_INFO("renegotiation failed\n");
		PE_TRANSIT_STATE(pd_port, PE_ERROR_RECOVERY);
		return true;
	}
#endif	/* CONFIG_USB_PD_RENEGOTIATION_COUNTER */

	if (pd_port->pe_data.pe_state_flags &
		PE_STATE_FLAG_BACK_READY_IF_TX_FAILED) {
		pd_notify_tcp_event_2nd_result(
			pd_port, TCP_DPM_RET_NO_RESPONSE);
		pe_transit_ready_state(pd_port);
		return true;
	} else if (pd_port->pe_data.pe_state_flags &
		PE_STATE_FLAG_HRESET_IF_TX_FAILED) {
		pe_transit_hard_reset_state(pd_port);
		return true;
	}

	return false;
}

static inline bool pd_process_hw_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->msg) {
	case PD_HW_RECV_HARD_RESET:
		return pd_process_recv_hard_reset(pd_port, pd_event);

	case PD_HW_TX_FAILED:
		return pd_process_hw_msg_tx_failed(pd_port, pd_event);

	default:
		return false;
	};
}

/*
 * [BLOCK] Porcess Timer MSG
 */

#ifdef CONFIG_USB_PD_CHECK_RX_PENDING_IF_SRTOUT
static inline bool pd_check_rx_pending(struct pd_port *pd_port)
{
	uint32_t alert;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (tcpci_get_alert_status(tcpc, &alert))
		return false;

	if (alert & TCPC_REG_ALERT_RX_STATUS) {
		PE_INFO("rx_pending\n");
#ifndef CONFIG_USB_PD_ONLY_PRINT_SYSTEM_BUSY
		pd_enable_timer(pd_port, PD_TIMER_SENDER_RESPONSE);
#endif
		return true;
	}

	return false;
}
#endif	/* CONFIG_USB_PD_CHECK_RX_PENDING_IF_SRTOUT */

static inline bool pd_process_timer_msg(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	uint8_t ready_state = pe_get_curr_ready_state(pd_port);

	switch (pd_event->msg) {
#ifndef CONFIG_USB_PD_DBG_IGRONE_TIMEOUT
	case PD_TIMER_SENDER_RESPONSE:

#ifdef CONFIG_USB_PD_CHECK_RX_PENDING_IF_SRTOUT
#ifndef CONFIG_USB_PD_ONLY_PRINT_SYSTEM_BUSY
		if (pd_check_rx_pending(pd_port))
			return false;
#else
		pd_check_rx_pending(pd_port);
#endif	/* CONFIG_USB_PD_PRINT_SYSTEM_BUSY */
#endif	/* CONFIG_USB_PD_CHECK_RX_PENDING_IF_SRTOUT */


		pd_cancel_dpm_reaction(pd_port);
		pd_notify_pe_cancel_pr_swap(pd_port);
		pd_notify_tcp_event_2nd_result(pd_port, TCP_DPM_RET_TIMEOUT);
		if (pd_port->pe_data.pe_state_flags &
			PE_STATE_FLAG_BACK_READY_IF_SR_TIMER_TOUT) {
			PE_TRANSIT_STATE(pd_port, ready_state);
			return true;
		}

		if (pd_port->pe_data.pe_state_flags &
			PE_STATE_FLAG_HRESET_IF_SR_TIMEOUT) {
			pe_transit_hard_reset_state(pd_port);
			return true;
		}
		break;
#endif	/* CONFIG_USB_PD_DBG_IGRONE_TIMEOUT */
	case PD_TIMER_BIST_CONT_MODE:
		if (PE_MAKE_STATE_TRANSIT_SINGLE(
			PE_BIST_CARRIER_MODE_2, ready_state))
			return true;
		break;

#ifdef CONFIG_USB_PD_DFP_FLOW_DELAY
	case PD_TIMER_DFP_FLOW_DELAY:
		if (pd_port->pe_state_curr == ready_state &&
			pd_port->data_role == PD_ROLE_DFP) {
			dpm_reaction_set_clear(pd_port,
				DPM_REACTION_CAP_READY_ONCE,
				DPM_REACTION_DFP_FLOW_DELAY);
		}
		break;
#endif	/* CONFIG_USB_PD_DFP_FLOW_DELAY */

#ifdef CONFIG_USB_PD_UFP_FLOW_DELAY
	case PD_TIMER_UFP_FLOW_DELAY:
		if (pd_port->pe_state_curr == ready_state &&
			pd_port->data_role == PD_ROLE_UFP) {
			dpm_reaction_set_clear(pd_port,
				DPM_REACTION_CAP_READY_ONCE,
				DPM_REACTION_UFP_FLOW_DELAY);
		}
		break;
#endif	/* CONFIG_USB_PD_UFP_FLOW_DELAY */

#ifdef CONFIG_USB_PD_VCONN_STABLE_DELAY
	case PD_TIMER_VCONN_STABLE:
		if (pd_port->vconn_role == PD_ROLE_VCONN_DYNAMIC_ON) {
			pd_set_vconn(pd_port, PD_ROLE_VCONN_ON);
			dpm_reaction_set_clear(pd_port,
				DPM_REACTION_CAP_READY_ONCE,
				DPM_REACTION_VCONN_STABLE_DELAY);
		}
		break;
#endif	/* CONFIG_USB_PD_VCONN_STABLE_DELAY */

#if defined(CONFIG_USB_PD_REV30) && defined(CONFIG_USB_PD_REV30_COLLISION_AVOID)
	case PD_TIMER_DEFERRED_EVT:
		pd_notify_tcp_event_buf_reset(
			pd_port, TCP_DPM_RET_DROP_PE_BUSY);
		break;
#endif

	default:
		break;
	}

	return false;
}

bool pd_process_event_com(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_event->event_type) {
	case PD_EVT_CTRL_MSG:
		return pd_process_ctrl_msg(pd_port, pd_event);

	case PD_EVT_DATA_MSG:
		return pd_process_data_msg(pd_port, pd_event);

#ifdef CONFIG_USB_PD_REV30
	case PD_EVT_EXT_MSG:
		return pd_process_ext_msg(pd_port, pd_event);
#endif	/* CONFIG_USB_PD_REV30 */

	case PD_EVT_DPM_MSG:
		return pd_process_dpm_msg(pd_port, pd_event);

	case PD_EVT_HW_MSG:
		return pd_process_hw_msg(pd_port, pd_event);

	case PD_EVT_TIMER_MSG:
		return pd_process_timer_msg(pd_port, pd_event);

	default:
		return false;
	}
}
