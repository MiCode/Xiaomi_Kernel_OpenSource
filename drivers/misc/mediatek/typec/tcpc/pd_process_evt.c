// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "inc/pd_core.h"
#include "inc/tcpci_event.h"
#include "inc/pd_process_evt.h"
#include "inc/pd_dpm_core.h"

/*
 * [BLOCK] print event
 */

#if PE_EVENT_DBG_ENABLE
static const char * const pd_ctrl_msg_name[] = {
	"ctrl0",
	"good_crc",
	"goto_min",
	"accept",
	"reject",
	"ping",
	"ps_rdy",
	"get_src_cap",
	"get_snk_cap",
	"dr_swap",
	"pr_swap",
	"vs_swap",
	"wait",
	"soft_reset",
	"ctrlE",
	"ctrlF",
#ifdef CONFIG_USB_PD_REV30
	"no_support",
	"get_src_cap_ex",
	"get_status",
	"fr_swap",
	"get_pps",
	"get_cc",
#endif	/* CONFIG_USB_PD_REV30 */
};

static inline void print_ctrl_msg_event(struct tcpc_device *tcpc, uint8_t msg)
{
	if (msg < PD_CTRL_MSG_NR)
		PE_EVT_INFO("%s\n", pd_ctrl_msg_name[msg]);
}

static const char * const pd_data_msg_name[] = {
	"data0",
	"src_cap",
	"request",
	"bist",
	"sink_cap",
#ifdef CONFIG_USB_PD_REV30
	"bat_status",
	"alert",
	"get_ci",
#else
	"data5",
	"data6",
	"data7",
#endif	/* CONFIG_USB_PD_REV30 */
	"data8",
	"data9",
	"dataA",
	"dataB",
	"dataC",
	"dataD",
	"dataE",
	"vdm",
};

static inline void print_data_msg_event(struct tcpc_device *tcpc, uint8_t msg)
{
	if (msg < PD_DATA_MSG_NR)
		PE_EVT_INFO("%s\n", pd_data_msg_name[msg]);
}

#ifdef CONFIG_USB_PD_REV30

static const char *const pd_ext_msg_name[] = {
	"ext0",
	"src_cap_ex",
	"status",
	"get_bat_cap",
	"get_bat_status",
	"bat_cap",
	"get_mfr_info",
	"mfr_info",
	"sec_req",
	"sec_resp",
	"fw_update_req",
	"fw_update_res",
	"pps_status",
	"ci",
	"cc",
};

static inline void print_ext_msg_event(struct tcpc_device *tcpc, uint8_t msg)
{
	if (msg < PD_EXT_MSG_NR)
		PE_EVT_INFO("%s\n", pd_ext_msg_name[msg]);
}

#endif	/* CONFIG_USB_PD_REV30 */

static const char *const pd_hw_msg_name[] = {
	"Detached",
	"Attached",
	"hard_reset",
	"vbus_high",
	"vbus_low",
	"vbus_0v",
	"vbus_stable",
	"tx_err",
	"discard",
	"retry_vdm",

#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	"sink_tx_change",
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
};

static inline void print_hw_msg_event(struct tcpc_device *tcpc, uint8_t msg)
{
	if (msg < PD_HW_MSG_NR)
		PE_EVT_INFO("%s\n", pd_hw_msg_name[msg]);
}

static const char *const pd_pe_msg_name[] = {
	"reset_prl_done",
	"pr_at_dft",
	"hard_reset_done",
	"pe_idle",
	"vdm_reset",
	"vdm_not_support",
};

static inline void print_pe_msg_event(struct tcpc_device *tcpc, uint8_t msg)
{
	if (msg < PD_PE_MSG_NR)
		PE_EVT_INFO("%s\n", pd_pe_msg_name[msg]);
}

static const char * const pd_dpm_msg_name[] = {
	"ack",
	"nak",
	"cap_change",
	"not_support",
};

static inline void print_dpm_msg_event(struct tcpc_device *tcpc, uint8_t msg)
{
	if (msg < PD_DPM_MSG_NR)
		PE_EVT_INFO("dpm_%s\n", pd_dpm_msg_name[msg]);
}

static const char *const tcp_dpm_evt_name[] = {
	/* TCP_DPM_EVT_UNKONW */
	"unknown",

	/* TCP_DPM_EVT_PD_COMMAND */
	"pr_swap_snk",
	"pr_swap_src",
	"dr_swap_ufp",
	"dr_swap_dfp",
	"vc_swap_off",
	"vc_swap_on",
	"goto_min",
	"soft_reset",
	"cable_soft_reset",
	"get_src_cap",
	"get_snk_cap",
	"request",
	"request_ex",
	"request_again",
	"bist_cm2",
	"dummy",

#ifdef CONFIG_USB_PD_REV30
	"get_src_cap_ext",
	"get_status",
	"fr_swap_snk",
	"fr_swap_src",
	"get_cc",
	"get_pps",

	"alert",
	"get_ci",
	"get_bat_cap",
	"get_bat_status",
	"get_mfrs_info",
#endif	/* CONFIG_USB_PD_REV30 */

	/* TCP_DPM_EVT_VDM_COMMAND */
	"disc_cable",
	"disc_id",
	"disc_svid",
	"disc_mode",
	"enter_mode",
	"exit_mode",
	"attention",

#ifdef CONFIG_USB_PD_ALT_MODE
	"dp_atten",
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	"dp_status",
	"dp_config",
#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */
#endif	/* CONFIG_USB_PD_ALT_MODE */

#ifdef CONFIG_USB_PD_CUSTOM_VDM
	"uvdm",
#endif	/* CONFIG_USB_PD_CUSTOM_VDM */

	/* TCP_DPM_EVT_IMMEDIATELY */
	"hard_reset",
	"error_recovery",
};

static inline void print_tcp_event(struct tcpc_device *tcpc, uint8_t msg)
{
	if (msg < TCP_DPM_EVT_NR)
		PE_EVT_INFO("tcp_event(%s), %d\n",
			tcp_dpm_evt_name[msg], msg);
}
#endif

static inline void print_event(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
#if PE_EVENT_DBG_ENABLE
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	switch (pd_event->event_type) {
	case PD_EVT_CTRL_MSG:
		print_ctrl_msg_event(tcpc, pd_event->msg);
		break;

	case PD_EVT_DATA_MSG:
		print_data_msg_event(tcpc, pd_event->msg);
		break;

#ifdef CONFIG_USB_PD_REV30
	case PD_EVT_EXT_MSG:
		print_ext_msg_event(tcpc, pd_event->msg);
		break;
#endif	/* CONFIG_USB_PD_REV30 */

	case PD_EVT_DPM_MSG:
		print_dpm_msg_event(tcpc, pd_event->msg);
		break;

	case PD_EVT_HW_MSG:
		print_hw_msg_event(tcpc, pd_event->msg);
		break;

	case PD_EVT_PE_MSG:
		print_pe_msg_event(tcpc, pd_event->msg);
		break;

	case PD_EVT_TIMER_MSG:
		PE_EVT_INFO("timer\n");
		break;

	case PD_EVT_TCP_MSG:
		print_tcp_event(tcpc, pd_event->msg);
		break;
	}
#endif
}

/*---------------------------------------------------------------------------*/

bool pd_make_pe_state_transit(struct pd_port *pd_port,
	uint8_t curr_state, const struct pe_state_reaction *state_reaction)
{
	int i;
	const struct pe_state_transition *state_transition =
		state_reaction->state_transition;

	for (i = 0; i < state_reaction->nr_transition; i++) {
		if (state_transition[i].curr_state == curr_state) {
			PE_TRANSIT_STATE(pd_port,
				state_transition[i].next_state);
			return true;
		}
	}

	return false;
}

/*---------------------------------------------------------------------------*/

#ifdef CONFIG_USB_PD_REV30
static inline bool pd30_process_ready_protocol_error(struct pd_port *pd_port)
{
	bool multi_chunk;

	if (!pd_check_rev30(pd_port))
		return false;

	if (!pd_port->curr_unsupported_msg) {
		pe_transit_soft_reset_state(pd_port);
		return true;
	}

	multi_chunk = pd_is_multi_chunk_msg(pd_port);

	if (pd_port->power_role == PD_ROLE_SINK) {
		PE_TRANSIT_STATE(pd_port, multi_chunk ?
			PE_SNK_CHUNK_RECEIVED : PE_SNK_SEND_NOT_SUPPORTED);
		return true;
	}

	PE_TRANSIT_STATE(pd_port, multi_chunk ?
		PE_SRC_CHUNK_RECEIVED : PE_SRC_SEND_NOT_SUPPORTED);
	return true;
}
#endif	/* CONFIG_USB_PD_REV30 */


bool pd_process_protocol_error(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool power_change = false;
#if PE_INFO_ENABLE
	uint8_t event_type = pd_event->event_type;
	uint8_t msg_id = pd_get_msg_hdr_id(pd_port);
	uint8_t msg_type = pd_event->msg;
#endif
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	switch (pd_port->pe_state_curr) {
	case PE_SNK_TRANSITION_SINK:
		/* fall through */
	case PE_SRC_TRANSITION_SUPPLY:	/* never recv ping for Source =.=*/
		/* fall through */
	case PE_SRC_TRANSITION_SUPPLY2:
		power_change = true;
		if (pd_event_msg_match(pd_event,
				PD_EVT_CTRL_MSG, PD_CTRL_PING)) {
			PE_DBG("Ignore Ping\n");
			return false;
		}
		break;

#ifdef CONFIG_USB_PD_PR_SWAP
	case PE_PRS_SRC_SNK_WAIT_SOURCE_ON:
#endif	/* CONFIG_USB_PD_PR_SWAP */
		if (pd_event_msg_match(pd_event,
				PD_EVT_CTRL_MSG, PD_CTRL_PING)) {
			PE_DBG("Ignore Ping\n");
			return false;
		}
		break;

#ifdef CONFIG_USB_PD_REV30
	case PE_SNK_READY:
	case PE_SRC_READY:
		if (pd30_process_ready_protocol_error(pd_port))
			return true;
		break;
#endif	/* CONFIG_USB_PD_REV30 */
	};

	if (pd_port->pe_data.pe_state_flags &
			PE_STATE_FLAG_IGNORE_UNKNOWN_EVENT) {
		PE_DBG("Ignore Unknown Event\n");
		return false;
	}

	if (pd_check_pe_during_hard_reset(pd_port)) {
		PE_DBG("Ignore Event during HReset\n");
		return false;
	}

	/*
	 * msg_type: PD_EVT_CTRL_MSG (1), PD_EVT_DATA_MSG (2)
	 */

	PE_INFO("PRL_ERR: %d-%d-%d\n", event_type, msg_type, msg_id);

	if (pd_port->pe_data.during_swap) {
#ifdef CONFIG_USB_PD_PR_SWAP_ERROR_RECOVERY
		PE_TRANSIT_STATE(pd_port, PE_ERROR_RECOVERY);
#else
		pe_transit_hard_reset_state(pd_port);
#endif
	} else if (power_change)
		pe_transit_hard_reset_state(pd_port);
	else
		pe_transit_soft_reset_state(pd_port);

	return true;
}

bool pd_process_tx_failed(struct pd_port *pd_port)
{
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_check_pe_state_ready(pd_port) ||
		pd_check_pe_during_hard_reset(pd_port)) {
		PE_DBG("Ignore tx_failed\n");
		return false;
	}

	pe_transit_soft_reset_state(pd_port);
	return true;
}

/*---------------------------------------------------------------------------*/

#ifdef CONFIG_USB_PD_RESET_CABLE
static inline bool pd_process_cable_ctrl_msg_accept(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	switch (pd_port->pe_state_curr) {
#ifdef CONFIG_PD_SRC_RESET_CABLE
	case PE_SRC_CBL_SEND_SOFT_RESET:
		vdm_put_dpm_discover_cable_event(pd_port);
		return false;
#endif	/* CONFIG_PD_SRC_RESET_CABLE */

#ifdef CONFIG_PD_DFP_RESET_CABLE
	case PE_DFP_CBL_SEND_SOFT_RESET:
		pe_transit_ready_state(pd_port);
		return true;
#endif	/* CONFIG_PD_DFP_RESET_CABLE */
	}

	return false;
}
#endif	/* CONFIG_USB_PD_RESET_CABLE */

static inline bool pd_process_event_cable(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool ret = false;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

#ifdef CONFIG_USB_PD_RESET_CABLE
	if (pd_event->msg == PD_CTRL_ACCEPT)
		ret = pd_process_cable_ctrl_msg_accept(pd_port, pd_event);
#endif	/* CONFIG_USB_PD_RESET_CABLE */

	if (!ret)
		PE_DBG("Ignore not SOP Ctrl Msg\n");

	return ret;
}

/*---------------------------------------------------------------------------*/

static void pd_copy_msg_data(struct pd_port *pd_port,
		uint8_t *payload, uint16_t count, uint8_t unit_sz)
{
	pd_port->pd_msg_data_size = count * unit_sz;
	pd_port->pd_msg_data_count = count;
	pd_port->pd_msg_data_payload = payload;
}

#ifdef CONFIG_USB_PD_REV30
static inline void pd_copy_msg_data_from_ext_evt(
	struct pd_port *pd_port, struct pd_msg *pd_msg)
{
	uint32_t *payload = pd_msg->payload;

	uint16_t *ext_hdr = (uint16_t *) payload;
	uint8_t *ext_data = (uint8_t *) (ext_hdr+1);

	uint16_t size = PD_EXT_HEADER_DATA_SIZE(*ext_hdr);

	pd_copy_msg_data(pd_port, ext_data, size, 1);
}
#endif	/* CONFIG_USB_PD_REV30 */

static inline void pd_copy_msg_data_from_evt(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	struct pd_msg *pd_msg = pd_event->pd_msg;

	switch (pd_event->event_type) {
	case PD_EVT_DATA_MSG:
		PD_BUG_ON(pd_msg == NULL);
		pd_copy_msg_data(pd_port, (uint8_t *)pd_msg->payload,
			pd_get_msg_hdr_cnt(pd_port), sizeof(uint32_t));
		break;

#ifdef CONFIG_USB_PD_REV30
	case PD_EVT_EXT_MSG:
		PD_BUG_ON(pd_msg == NULL);
		pd_copy_msg_data_from_ext_evt(pd_port, pd_msg);
		return;
#endif	/* CONFIG_USB_PD_REV30 */

	default:
		pd_copy_msg_data(pd_port, NULL, 0, 0);
	}

}

/*---------------------------------------------------------------------------*/

/*
 *
 * @ true : valid message
 * @ false : invalid message, pe should drop the message
 */

static inline bool pe_is_valid_pd_msg_id(struct pd_port *pd_port,
			struct pd_event *pd_event, struct pd_msg *pd_msg)
{
	uint8_t sop_type = pd_msg->frame_type;
	uint8_t msg_id = pd_get_msg_hdr_id(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_port->pe_state_curr == PE_BIST_TEST_DATA)
		return false;

	if (pd_event->event_type == PD_EVT_CTRL_MSG) {
		switch (pd_event->msg) {
		/* SofReset always has a MessageID value of zero */
		case PD_CTRL_SOFT_RESET:
			if (msg_id != 0) {
				PE_INFO("Repeat soft_reset\n");
				return false;
			}
			return true;

		case PD_CTRL_GOOD_CRC:
			PE_DBG("Discard_CRC\n");
			return true;

#ifdef CONFIG_USB_PD_IGNORE_PS_RDY_AFTER_PR_SWAP
		case PD_CTRL_PS_RDY:
			if (pd_port->msg_id_pr_swap_last == msg_id) {
				PE_INFO("Repeat ps_rdy\n");
				return false;
			}
			break;
#endif	/* CONFIG_USB_PD_IGNORE_PS_RDY_AFTER_PR_SWAP */
		}
	}

	if (pd_port->pe_data.msg_id_rx[sop_type] == msg_id) {
		PE_INFO("Repeat msg: %c:%d:%d\n",
			(pd_event->event_type == PD_EVT_CTRL_MSG) ? 'C' : 'D',
			pd_event->msg, msg_id);
		return false;
	}

	pd_port->pe_data.msg_id_rx[sop_type] = msg_id;
	return true;
}

static inline bool pe_is_valid_pd_msg_role(struct pd_port *pd_port,
			struct pd_event *pd_event, struct pd_msg *pd_msg)
{
	bool ret = true;
	uint8_t msg_pr, msg_dr;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_msg == NULL)	/* Good-CRC */
		return true;

	if (pd_msg->frame_type != TCPC_TX_SOP)
		return true;

	msg_pr = PD_HEADER_PR(pd_msg->msg_hdr);
	msg_dr = PD_HEADER_DR(pd_msg->msg_hdr);

	/*
	 * The Port Power Role field of a received Message shall not be verified
	 * by the receiver and no error recovery action shall be
	 * taken if it is incorrect.
	 */

	if (msg_pr == pd_port->power_role)
		PE_DBG("Wrong PR:%d\n", msg_pr);

	/*
	 * Should a Type-C Port receive a Message with the Port Data Role field
	 * set to the same Data Role as its current Data Role,
	 * except for the GoodCRC Message,
	 * Type-C error recovery actions as defined
	 * in [USBType-C 1.0] shall be performed.
	 */

	if (msg_dr == pd_port->data_role) {
#ifdef CONFIG_USB_PD_CHECK_DATA_ROLE
		ret = false;
#endif
		PE_INFO("Wrong DR:%d\n", msg_dr);
	}

	return ret;
}

static inline void pe_translate_pd_msg_event(struct pd_port *pd_port,
			struct pd_event *pd_event, struct pd_msg *pd_msg)
{
	uint16_t msg_hdr;

	PD_BUG_ON(pd_msg == NULL);

	msg_hdr = pd_msg->msg_hdr;
	pd_port->curr_msg_hdr = msg_hdr;
	pd_event->msg = PD_HEADER_TYPE(msg_hdr);

	if (PD_HEADER_CNT(msg_hdr))
		pd_event->event_type = PD_EVT_DATA_MSG;
	else
		pd_event->event_type = PD_EVT_CTRL_MSG;

#ifdef CONFIG_USB_PD_REV30
	if (PD_HEADER_EXT(msg_hdr))
		pd_event->event_type = PD_EVT_EXT_MSG;

	if (pd_msg->frame_type == TCPC_TX_SOP_PRIME) {
		pd_sync_sop_prime_spec_revision(
			pd_port, PD_HEADER_REV(msg_hdr));
	}
#endif	/* CONFIG_USB_PD_REV30 */
}

static inline uint8_t pe_get_startup_state(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool act_as_sink = true;
	uint8_t startup_state = 0xff;

#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	pd_port->custom_dbgacc = false;
#endif	/* CONFIG_USB_PD_CUSTOM_DBGACC */

	switch (pd_event->msg_sec) {
	case TYPEC_ATTACHED_DBGACC_SNK:
#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
		pd_port->custom_dbgacc = true;
		startup_state = PE_DBG_READY;
		break;
#endif	/* CONFIG_USB_PD_CUSTOM_DBGACC */
	case TYPEC_ATTACHED_SNK:
		startup_state = PE_SNK_STARTUP;
		break;

	case TYPEC_ATTACHED_SRC:
		act_as_sink = false;
		startup_state = PE_SRC_STARTUP;
		break;
	}

	/* At least > 2 for Ellisys VNDI PR_SWAP */
#ifdef CONFIG_USB_PD_ERROR_RECOVERY_ONCE
	if (pd_port->error_recovery_once > 2)
		startup_state = PE_ERROR_RECOVERY_ONCE;
#endif	/* CONFIG_USB_PD_ERROR_RECOVERY_ONCE */

	pd_init_message_hdr(pd_port, act_as_sink);
	return startup_state;
}

static inline bool pe_transit_startup_state(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	uint8_t startup_state =
		pe_get_startup_state(pd_port, pd_event);

	if (startup_state == 0xff)
		return false;

	pd_dpm_notify_pe_startup(pd_port);
	PE_TRANSIT_STATE(pd_port, startup_state);

	return true;
}

enum {
	TII_TRAP_IN_IDLE = 0,
	TII_TRANSIT_STATE = 1,
	TII_PE_RUNNING = 2,
};

static inline uint8_t pe_check_trap_in_idle_state(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	switch (pd_port->pe_pd_state) {
	case PE_IDLE1:
	case PE_ERROR_RECOVERY:
		if (pd_event_pe_msg_match(pd_event, PD_PE_IDLE))  {
			PE_TRANSIT_STATE(pd_port, PE_IDLE2);
			return TII_TRANSIT_STATE;
		}

		pd_try_put_pe_idle_event(pd_port);
		break;

	case PE_IDLE2:
		if (pd_event_hw_msg_match(pd_event, PD_HW_CC_ATTACHED)) {
			if (pe_transit_startup_state(pd_port, pd_event))
				return TII_TRANSIT_STATE;
		}

		/* The original IDLE2 may trigger by PE_IDLE_TOUT */
		if (pd_event_hw_msg_match(pd_event, PD_HW_CC_DETACHED))
			pd_notify_pe_idle(pd_port);
		break;

	default:
		if (pd_event_hw_msg_match(pd_event, PD_HW_CC_DETACHED)) {
			PE_TRANSIT_STATE(pd_port, PE_IDLE1);
			return TII_TRANSIT_STATE;
		}
		return TII_PE_RUNNING;
	}

	PE_DBG("Trap in idle state, Ignore All MSG (%d:%d)\n",
		pd_event->event_type, pd_event->msg);
	return TII_TRAP_IN_IDLE;
}

static inline void pe_init_curr_state(struct pd_port *pd_port)
{
	if (pd_port->power_role == PD_ROLE_SINK) {
		pd_port->curr_ready_state = PE_SNK_READY;
		pd_port->curr_hreset_state = PE_SNK_HARD_RESET;
		pd_port->curr_sreset_state = PE_SNK_SEND_SOFT_RESET;
	} else {
		pd_port->curr_ready_state = PE_SRC_READY;
		pd_port->curr_hreset_state = PE_SRC_HARD_RESET;
		pd_port->curr_sreset_state = PE_SRC_SEND_SOFT_RESET;
	}

	pd_port->curr_unsupported_msg = false;

#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	if (pd_port->custom_dbgacc)
		pd_port->curr_ready_state = PE_DBG_READY;
#endif	/* CONFIG_USB_PD_CUSTOM_DBGACC */
}

bool pd_process_event(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	bool ret = false;
	struct pd_msg *pd_msg = pd_event->pd_msg;
	uint8_t tii = pe_check_trap_in_idle_state(pd_port, pd_event);

	if (tii < TII_PE_RUNNING)
		return tii;

	pe_init_curr_state(pd_port);

	if (pd_event->event_type == PD_EVT_PD_MSG)
		pe_translate_pd_msg_event(pd_port, pd_event, pd_msg);

#if PE_EVT_INFO_VDM_DIS
	if (!pd_curr_is_vdm_evt(pd_port))
#endif
		print_event(pd_port, pd_event);

	if ((pd_event->event_type < PD_EVT_PD_MSG_END) && (pd_msg != NULL)) {

		if (!pe_is_valid_pd_msg_id(pd_port, pd_event, pd_msg))
			return false;

		if (!pe_is_valid_pd_msg_role(pd_port, pd_event, pd_msg)) {
			PE_TRANSIT_STATE(pd_port, PE_ERROR_RECOVERY);
			return true;
		}
	}

	pd_copy_msg_data_from_evt(pd_port, pd_event);

	if (pd_curr_is_vdm_evt(pd_port))
		return pd_process_event_vdm(pd_port, pd_event);

	if (pd_event->event_type == PD_EVT_TCP_MSG)
		return pd_process_event_tcp(pd_port, pd_event);

#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	if (pd_port->custom_dbgacc)
		return pd_process_event_dbg(pd_port, pd_event);
#endif	/* CONFIG_USB_PD_CUSTOM_DBGACC */

	if ((pd_event->event_type == PD_EVT_CTRL_MSG) &&
		(pd_event->msg != PD_CTRL_GOOD_CRC) &&
		(pd_msg != NULL) && (pd_msg->frame_type != TCPC_TX_SOP))
		return pd_process_event_cable(pd_port, pd_event);

	if (pd_process_event_com(pd_port, pd_event))
		return true;

	switch (pd_port->state_machine) {
#ifdef CONFIG_USB_PD_DR_SWAP
	case PE_STATE_MACHINE_DR_SWAP:
		ret = pd_process_event_drs(pd_port, pd_event);
		break;
#endif	/* CONFIG_USB_PD_DR_SWAP */

#ifdef CONFIG_USB_PD_PR_SWAP
	case PE_STATE_MACHINE_PR_SWAP:
		ret = pd_process_event_prs(pd_port, pd_event);
		break;
#endif	/* CONFIG_USB_PD_PR_SWAP */

#ifdef CONFIG_USB_PD_VCONN_SWAP
	case PE_STATE_MACHINE_VCONN_SWAP:
		ret = pd_process_event_vcs(pd_port, pd_event);
		break;
#endif	/* CONFIG_USB_PD_VCONN_SWAP */
	}

	if (ret)
		return true;

	if (pd_port->power_role == PD_ROLE_SINK)
		ret = pd_process_event_snk(pd_port, pd_event);
	else
		ret = pd_process_event_src(pd_port, pd_event);

	return ret;
}
