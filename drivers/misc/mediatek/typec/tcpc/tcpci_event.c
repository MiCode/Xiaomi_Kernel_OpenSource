// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kthread.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/version.h>

#include <linux/sched/rt.h>
#include <uapi/linux/sched/types.h>

#include "inc/tcpci_event.h"
#include "inc/tcpci_typec.h"
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"
#include "inc/pd_dpm_core.h"

#ifdef CONFIG_USB_PD_POSTPONE_VDM
static void postpone_vdm_event(struct tcpc_device *tcpc)
{
	/*
	 * Postpone VDM retry event due to the retry reason
	 * maybe interrupt by some PD event ....
	 */

	struct pd_event *vdm_event = &tcpc->pd_vdm_event;

	if (tcpc->pd_pending_vdm_event && vdm_event->pd_msg) {
		tcpc->pd_postpone_vdm_timeout = false;
		tcpc_restart_timer(tcpc, PD_PE_VDM_POSTPONE);
	}
}
#endif	/* CONFIG_USB_PD_POSTPONE_VDM */

struct pd_msg *__pd_alloc_msg(struct tcpc_device *tcpc)
{
	int i;
	uint8_t mask;

	for (i = 0, mask = 1; i < PD_MSG_BUF_SIZE; i++, mask <<= 1) {
		if ((mask & tcpc->pd_msg_buffer_allocated) == 0) {
			tcpc->pd_msg_buffer_allocated |= mask;
			return tcpc->pd_msg_buffer + i;
		}
	}

	PD_ERR("pd_alloc_msg failed\n");
	PD_BUG_ON(true);

	return (struct pd_msg *)NULL;
}

struct pd_msg *pd_alloc_msg(struct tcpc_device *tcpc)
{
	struct pd_msg *pd_msg = NULL;

	mutex_lock(&tcpc->access_lock);
	pd_msg = __pd_alloc_msg(tcpc);
	mutex_unlock(&tcpc->access_lock);

	return pd_msg;
}

static void __pd_free_msg(struct tcpc_device *tcpc, struct pd_msg *pd_msg)
{
	int index = pd_msg - tcpc->pd_msg_buffer;
	uint8_t mask = 1 << index;

	PD_BUG_ON((mask & tcpc->pd_msg_buffer_allocated) == 0);
	tcpc->pd_msg_buffer_allocated &= (~mask);
}

static void __pd_free_event(
		struct tcpc_device *tcpc, struct pd_event *pd_event)
{
	if (pd_event->pd_msg) {
		__pd_free_msg(tcpc, pd_event->pd_msg);
		pd_event->pd_msg = NULL;
	}
}

void pd_free_msg(struct tcpc_device *tcpc, struct pd_msg *pd_msg)
{
	mutex_lock(&tcpc->access_lock);
	__pd_free_msg(tcpc, pd_msg);
	mutex_unlock(&tcpc->access_lock);
}

void pd_free_event(struct tcpc_device *tcpc, struct pd_event *pd_event)
{
	mutex_lock(&tcpc->access_lock);
	__pd_free_event(tcpc, pd_event);
	mutex_unlock(&tcpc->access_lock);
}

/*----------------------------------------------------------------------------*/

static bool __pd_get_event(
	struct tcpc_device *tcpc, struct pd_event *pd_event)
{
	int index = 0;

	if (tcpc->pd_event_count <= 0)
		return false;

	tcpc->pd_event_count--;

	*pd_event =
		tcpc->pd_event_ring_buffer[tcpc->pd_event_head_index];

	if (tcpc->pd_event_count) {
		index = tcpc->pd_event_head_index + 1;
		index %= PD_EVENT_BUF_SIZE;
	}
	tcpc->pd_event_head_index = index;
	return true;
}

bool pd_get_event(struct tcpc_device *tcpc, struct pd_event *pd_event)
{
	bool ret;

	mutex_lock(&tcpc->access_lock);
	ret = __pd_get_event(tcpc, pd_event);
	mutex_unlock(&tcpc->access_lock);
	return ret;
}

static bool __pd_put_event(struct tcpc_device *tcpc,
	const struct pd_event *pd_event, bool from_port_partner)
{
	int index;

#ifdef CONFIG_USB_PD_POSTPONE_OTHER_VDM
	if (from_port_partner)
		postpone_vdm_event(tcpc);
#endif	/* CONFIG_USB_PD_POSTPONE_OTHER_VDM */

	if (tcpc->pd_event_count >= PD_EVENT_BUF_SIZE) {
		PD_ERR("pd_put_event failed\n");
		return false;
	}

	index = (tcpc->pd_event_head_index + tcpc->pd_event_count);
	index %= PD_EVENT_BUF_SIZE;

	tcpc->pd_event_count++;
	tcpc->pd_event_ring_buffer[index] = *pd_event;

	atomic_inc(&tcpc->pending_event);
	wake_up(&tcpc->event_wait_que);
	return true;
}

bool pd_put_event(struct tcpc_device *tcpc, const struct pd_event *pd_event,
	bool from_port_partner)
{
	bool ret;

	mutex_lock(&tcpc->access_lock);
	ret = __pd_put_event(tcpc, pd_event, from_port_partner);
	mutex_unlock(&tcpc->access_lock);

	return ret;
}

/*----------------------------------------------------------------------------*/

static inline void pd_get_attention_event(
	struct tcpc_device *tcpc, struct pd_event *pd_event)
{
	struct pd_event attention_evt = {
		.event_type = PD_EVT_PD_MSG,
		.msg = PD_DATA_VENDOR_DEF,
		.pd_msg = NULL,
	};

	*pd_event = attention_evt;
	pd_event->pd_msg = __pd_alloc_msg(tcpc);

	if (pd_event->pd_msg == NULL)
		return;

	tcpc->pd_pending_vdm_attention = false;
	*pd_event->pd_msg = tcpc->pd_attention_vdm_msg;
}

static inline bool pd_check_vdm_state_ready(struct pd_port *pd_port)
{
	switch (pd_port->pe_vdm_state) {
	case PE_SNK_READY:
	case PE_SRC_READY:
#ifdef CONFIG_USB_PD_CUSTOM_DBGACC
	case PE_DBG_READY:
#endif	/* CONFIG_USB_PD_CUSTOM_DBGACC */
		return true;

	default:
		return false;
	}
}

bool pd_get_vdm_event(struct tcpc_device *tcpc, struct pd_event *pd_event)
{
	struct pd_event delay_evt = {
		.event_type = PD_EVT_CTRL_MSG,
		.msg = PD_CTRL_GOOD_CRC,
		.pd_msg = NULL,
	};

	struct pd_event reset_evt = {
		.event_type = PD_EVT_PE_MSG,
		.msg = PD_PE_VDM_RESET,
		.pd_msg = NULL,
	};

	struct pd_event discard_evt = {
		.event_type = PD_EVT_HW_MSG,
		.msg = PD_HW_TX_DISCARD,
		.pd_msg = NULL,
	};

	struct pd_event *vdm_event = &tcpc->pd_vdm_event;

	if (tcpc->pd_pending_vdm_discard) {
		mutex_lock(&tcpc->access_lock);
		*pd_event = discard_evt;
		tcpc->pd_pending_vdm_discard = false;
		mutex_unlock(&tcpc->access_lock);
		return true;
	}

	if (tcpc->pd_pending_vdm_event) {
		if (vdm_event->pd_msg && !tcpc->pd_postpone_vdm_timeout)
			return false;

		mutex_lock(&tcpc->access_lock);
		if (tcpc->pd_pending_vdm_good_crc) {
			*pd_event = delay_evt;
			tcpc->pd_pending_vdm_good_crc = false;
		} else if (tcpc->pd_pending_vdm_reset) {
			*pd_event = reset_evt;
			tcpc->pd_pending_vdm_reset = false;
		} else {
			*pd_event = *vdm_event;
			tcpc->pd_pending_vdm_event = false;
		}

		mutex_unlock(&tcpc->access_lock);
		return true;
	}

	if (tcpc->pd_pending_vdm_attention
		&& pd_check_vdm_state_ready(&tcpc->pd_port)) {
		mutex_lock(&tcpc->access_lock);
		pd_get_attention_event(tcpc, pd_event);
		mutex_unlock(&tcpc->access_lock);
		return true;
	}

	return false;
}

static inline bool reset_pe_vdm_state(
		struct tcpc_device *tcpc, uint32_t vdm_hdr)
{
	bool vdm_reset = false;
	struct pd_port *pd_port = &tcpc->pd_port;

	if (PD_VDO_SVDM(vdm_hdr)) {
		if (PD_VDO_CMDT(vdm_hdr) == CMDT_INIT)
			vdm_reset = true;
	} else {
		if (pd_port->data_role == PD_ROLE_UFP)
			vdm_reset = true;
	}

	if (vdm_reset)
		tcpc->pd_pending_vdm_reset = true;

	return vdm_reset;
}

static inline bool pd_is_init_attention_event(
	struct tcpc_device *tcpc, struct pd_event *pd_event)
{
	uint32_t vdm_hdr = pd_event->pd_msg->payload[0];

	if (!PD_VDO_SVDM(vdm_hdr))
		return false;

	if ((PD_VDO_CMDT(vdm_hdr) == CMDT_INIT) &&
			PD_VDO_CMD(vdm_hdr) == CMD_ATTENTION) {
		return true;
	}

	return false;
}

bool pd_put_vdm_event(struct tcpc_device *tcpc,
		struct pd_event *pd_event, bool from_port_partner)
{
	bool ignore_evt = false;
	struct pd_msg *pd_msg = pd_event->pd_msg;

	mutex_lock(&tcpc->access_lock);

	if (from_port_partner &&
		pd_is_init_attention_event(tcpc, pd_event)) {
		TCPC_DBG("AttEvt\n");
		ignore_evt = true;
		tcpc->pd_pending_vdm_attention = true;
		tcpc->pd_attention_vdm_msg = *pd_msg;

		atomic_inc(&tcpc->pending_event);
		wake_up(&tcpc->event_wait_que);
	}

	if (tcpc->pd_pending_vdm_event && (!ignore_evt)) {
		/* If message from port partner, we have to overwrite it */
		/* If message from TCPM, we will reset_vdm later */
		ignore_evt = !from_port_partner;

		if (from_port_partner) {
			if (pd_event_ctrl_msg_match(
					&tcpc->pd_vdm_event,
					PD_CTRL_GOOD_CRC)) {
				TCPC_DBG2("PostponeVDM GoodCRC\n");
				tcpc->pd_pending_vdm_good_crc = true;
			}

			__pd_free_event(tcpc, &tcpc->pd_vdm_event);
		}
	}

	if (ignore_evt) {
		__pd_free_event(tcpc, pd_event);
		mutex_unlock(&tcpc->access_lock);
		return false;
	}

	tcpc->pd_vdm_event = *pd_event;
	tcpc->pd_pending_vdm_event = true;
	tcpc->pd_postpone_vdm_timeout = true;

	if (from_port_partner) {

		PD_BUG_ON(pd_msg == NULL);
		tcpc->pd_last_vdm_msg = *pd_msg;
		reset_pe_vdm_state(tcpc, pd_msg->payload[0]);

#ifdef CONFIG_USB_PD_POSTPONE_FIRST_VDM
		postpone_vdm_event(tcpc);
		mutex_unlock(&tcpc->access_lock);
		return true;
#endif	/* CONFIG_USB_PD_POSTPONE_FIRST_VDM */
	}

	atomic_inc(&tcpc->pending_event);
	wake_up(&tcpc->event_wait_que);
	mutex_unlock(&tcpc->access_lock);

	return true;
}

bool pd_put_last_vdm_event(struct tcpc_device *tcpc)
{
	struct pd_msg *pd_msg = &tcpc->pd_last_vdm_msg;
	struct pd_event *vdm_event = &tcpc->pd_vdm_event;

	mutex_lock(&tcpc->access_lock);

	tcpc->pd_pending_vdm_discard = true;
	atomic_inc(&tcpc->pending_event);
	wake_up(&tcpc->event_wait_que);

	/* If the last VDM event isn't INIT event, don't put it again */
	if (!reset_pe_vdm_state(tcpc, pd_msg->payload[0])) {
		mutex_unlock(&tcpc->access_lock);
		return true;
	}

	vdm_event->event_type = PD_EVT_HW_MSG;
	vdm_event->msg = PD_HW_RETRY_VDM;

	if (tcpc->pd_pending_vdm_event)
		__pd_free_event(tcpc, &tcpc->pd_vdm_event);

	vdm_event->pd_msg = __pd_alloc_msg(tcpc);

	if (vdm_event->pd_msg == NULL) {
		mutex_unlock(&tcpc->access_lock);
		return false;
	}

	*vdm_event->pd_msg = *pd_msg;
	tcpc->pd_pending_vdm_event = true;
	tcpc->pd_postpone_vdm_timeout = true;

#ifdef CONFIG_USB_PD_POSTPONE_RETRY_VDM
	postpone_vdm_event(tcpc);
#endif	/* CONFIG_USB_PD_POSTPONE_RETRY_VDM */

	mutex_unlock(&tcpc->access_lock);
	return true;
}

/*----------------------------------------------------------------------------*/

static bool __pd_get_deferred_tcp_event(
	struct tcpc_device *tcpc, struct tcp_dpm_event *tcp_event)
{
	int index = 0;

	if (tcpc->tcp_event_count <= 0)
		return false;

	tcpc->tcp_event_count--;

	*tcp_event =
		tcpc->tcp_event_ring_buffer[tcpc->tcp_event_head_index];

	if (tcpc->tcp_event_count) {
		index = tcpc->tcp_event_head_index + 1;
		index %= TCP_EVENT_BUF_SIZE;
	}
	tcpc->tcp_event_head_index = index;
	return true;
}

bool pd_get_deferred_tcp_event(
	struct tcpc_device *tcpc, struct tcp_dpm_event *tcp_event)
{
	bool ret;

	mutex_lock(&tcpc->access_lock);
	ret = __pd_get_deferred_tcp_event(tcpc, tcp_event);
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	if (tcpc->tcp_event_count)
		tcpc_restart_timer(tcpc, PD_TIMER_DEFERRED_EVT);
	else
		tcpc_disable_timer(tcpc, PD_TIMER_DEFERRED_EVT);
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#endif	/* CONFIG_USB_PD_REV30 */
	mutex_unlock(&tcpc->access_lock);

	return ret;
}

static bool __pd_put_deferred_tcp_event(
	struct tcpc_device *tcpc, const struct tcp_dpm_event *tcp_event)
{
	int index;

	index = (tcpc->tcp_event_head_index + tcpc->tcp_event_count);
	index %= TCP_EVENT_BUF_SIZE;

	tcpc->tcp_event_count++;
	tcpc->tcp_event_ring_buffer[index] = *tcp_event;

	atomic_inc(&tcpc->pending_event);
	wake_up(&tcpc->event_wait_que);
	return true;
}

bool pd_put_deferred_tcp_event(
	struct tcpc_device *tcpc, const struct tcp_dpm_event *tcp_event)
{
	bool ret = true;
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);
	mutex_lock(&tcpc->access_lock);

	if (!tcpc->pd_pe_running || tcpc->pd_wait_pe_idle) {
		PD_ERR("pd_put_tcp_event failed0\n");
		ret = false;
		goto unlock_out;
	}

	if (tcpc->tcp_event_count >= TCP_EVENT_BUF_SIZE) {
		PD_ERR("pd_put_tcp_event failed1\n");
		ret = false;
		goto unlock_out;
	}

	if (tcpc->pd_wait_hard_reset_complete) {
		PD_ERR("pd_put_tcp_event failed2\n");
		ret = false;
		goto unlock_out;
	}

	switch (tcp_event->event_id) {
	case TCP_DPM_EVT_DISCOVER_CABLE:
	case TCP_DPM_EVT_CABLE_SOFTRESET:
		dpm_reaction_set(pd_port,
			DPM_REACTION_DYNAMIC_VCONN |
			DPM_REACTION_VCONN_STABLE_DELAY);
		break;
	}

	ret = __pd_put_deferred_tcp_event(tcpc, tcp_event);
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	if (ret)
		pd_port->pe_data.pd_traffic_idle = false;
	if (tcpc->tcp_event_count == 1)
		tcpc_enable_timer(tcpc, PD_TIMER_DEFERRED_EVT);
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#endif	/* CONFIG_USB_PD_REV30 */

	dpm_reaction_set_ready_once(pd_port);

unlock_out:
	mutex_unlock(&tcpc->access_lock);
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}

void pd_notify_tcp_vdm_event_2nd_result(struct pd_port *pd_port, uint8_t ret)
{
#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	struct tcp_dpm_event *tcp_event = &pd_port->tcp_event;

	if (pd_port->tcp_event_id_2nd  == TCP_DPM_EVT_UNKONW)
		return;

	if (pd_port->tcp_event_id_2nd < TCP_DPM_EVT_VDM_COMMAND ||
		pd_port->tcp_event_id_2nd >= TCP_DPM_EVT_IMMEDIATELY)
		return;

	if (tcp_event->event_cb != NULL)
		tcp_event->event_cb(pd_port->tcpc, ret, tcp_event);

	pd_port->tcp_event_id_2nd = TCP_DPM_EVT_UNKONW;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */
}

void pd_notify_tcp_event_2nd_result(struct pd_port *pd_port, int ret)
{
#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	struct tcp_dpm_event *tcp_event = &pd_port->tcp_event;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_port->tcp_event_id_2nd  == TCP_DPM_EVT_UNKONW)
		return;

	switch (ret) {
	case TCP_DPM_RET_DROP_SENT_SRESET:
		if (pd_port->tcp_event_id_2nd == TCP_DPM_EVT_SOFTRESET
			&& pd_port->tcp_event_drop_reset_once) {
			pd_port->tcp_event_drop_reset_once = false;
			return;
		}
		break;
	case TCP_DPM_RET_DROP_SENT_HRESET:
		if (pd_port->tcp_event_id_2nd == TCP_DPM_EVT_HARD_RESET
			&& pd_port->tcp_event_drop_reset_once) {
			pd_port->tcp_event_drop_reset_once = false;
			return;
		}
		break;
	case TCP_DPM_RET_SUCCESS:
		/* Ignore VDM */
		if (pd_port->tcp_event_id_2nd >= TCP_DPM_EVT_VDM_COMMAND
			&& pd_port->tcp_event_id_2nd < TCP_DPM_EVT_IMMEDIATELY)
			return;

		break;
	}

	TCPC_DBG2("tcp_event_2nd:evt%d=%d\n",
		pd_port->tcp_event_id_2nd, ret);

	if (tcp_event->event_cb != NULL)
		tcp_event->event_cb(tcpc, ret, tcp_event);

	pd_port->tcp_event_id_2nd = TCP_DPM_EVT_UNKONW;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */
}

void pd_notify_tcp_event_1st_result(struct pd_port *pd_port, int ret)
{
	bool cb = true;
	struct tcp_dpm_event *tcp_event = &pd_port->tcp_event;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_port->tcp_event_id_1st == TCP_DPM_EVT_UNKONW)
		return;

	TCPC_DBG2("tcp_event_1st:evt%d=%d\n",
		pd_port->tcp_event_id_1st, ret);

#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	if (ret == TCP_DPM_RET_SENT) {
		cb = false;
		pd_port->tcp_event_id_2nd = tcp_event->event_id;
	}
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	if (cb && tcp_event->event_cb != NULL)
		tcp_event->event_cb(tcpc, ret, tcp_event);

	pd_port->tcp_event_id_1st = TCP_DPM_EVT_UNKONW;
}

static void __tcp_event_buf_reset(
	struct tcpc_device *tcpc, uint8_t reason)
{
	struct tcp_dpm_event tcp_event;

	pd_notify_tcp_event_2nd_result(&tcpc->pd_port, reason);

	while (__pd_get_deferred_tcp_event(tcpc, &tcp_event)) {
		if (tcp_event.event_cb != NULL)
			tcp_event.event_cb(tcpc, reason, &tcp_event);
	}
}

void pd_notify_tcp_event_buf_reset(struct pd_port *pd_port, uint8_t reason)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	pd_notify_tcp_event_1st_result(pd_port, reason);

	mutex_lock(&tcpc->access_lock);
	__tcp_event_buf_reset(tcpc, reason);
	mutex_unlock(&tcpc->access_lock);
}

/*----------------------------------------------------------------------------*/

static void __pd_event_buf_reset(struct tcpc_device *tcpc, uint8_t reason)
{
	struct pd_event pd_event;

	tcpc->pd_hard_reset_event_pending = false;
	while (__pd_get_event(tcpc, &pd_event))
		__pd_free_event(tcpc, &pd_event);

	if (tcpc->pd_pending_vdm_event) {
		__pd_free_event(tcpc, &tcpc->pd_vdm_event);
		tcpc->pd_pending_vdm_event = false;
	}

	tcpc->pd_pending_vdm_reset = false;
	tcpc->pd_pending_vdm_good_crc = false;
	tcpc->pd_pending_vdm_attention = false;
	tcpc->pd_pending_vdm_discard = false;

	__tcp_event_buf_reset(tcpc, reason);
	/* PD_BUG_ON(tcpc->pd_msg_buffer_allocated != 0); */
}

void pd_event_buf_reset(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->access_lock);
	__pd_event_buf_reset(tcpc, TCP_DPM_RET_DROP_CC_DETACH);
	mutex_unlock(&tcpc->access_lock);
}

/*----------------------------------------------------------------------------*/

static inline bool __pd_put_hw_event(
	struct tcpc_device *tcpc, uint8_t hw_event)
{
	struct pd_event evt = {
		.event_type = PD_EVT_HW_MSG,
		.msg = hw_event,
		.pd_msg = NULL,
	};

	return __pd_put_event(tcpc, &evt, false);
}

static inline bool __pd_put_pe_event(
	struct tcpc_device *tcpc, uint8_t pe_event)
{
	struct pd_event evt = {
		.event_type = PD_EVT_PE_MSG,
		.msg = pe_event,
		.pd_msg = NULL,
	};

	return __pd_put_event(tcpc, &evt, false);
}

bool __pd_put_cc_attached_event(
		struct tcpc_device *tcpc, uint8_t type)
{
	struct pd_event evt = {
		.event_type = PD_EVT_HW_MSG,
		.msg = PD_HW_CC_ATTACHED,
		.msg_sec = type,
		.pd_msg = NULL,
	};

	switch (type) {
	case TYPEC_ATTACHED_SNK:
	case TYPEC_ATTACHED_SRC:
	case TYPEC_ATTACHED_DBGACC_SNK:
		tcpc->pd_pe_running = true;
		tcpc->pd_wait_pe_idle = false;
		break;
	default:
		break;
	}

	return __pd_put_event(tcpc, &evt, false);
}

bool pd_put_cc_attached_event(
		struct tcpc_device *tcpc, uint8_t type)
{
	bool ret = false;
#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_USB_PD_WAIT_BC12
	int rv = 0;
	union power_supply_propval val = {.intval = 0};
#endif /* CONFIG_USB_PD_WAIT_BC12 */
#endif /* CONFIG_USB_POWER_DELIVERY */

	mutex_lock(&tcpc->access_lock);

#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_USB_PD_WAIT_BC12
	rv = power_supply_get_property(tcpc->chg_psy,
		POWER_SUPPLY_PROP_USB_TYPE, &val);
	if ((type == TYPEC_ATTACHED_SNK || type == TYPEC_ATTACHED_DBGACC_SNK) &&
		(rv < 0 || val.intval == POWER_SUPPLY_USB_TYPE_UNKNOWN)) {
		tcpc->pd_wait_bc12_count = 1;
		tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_PD_WAIT_BC12);
		mutex_unlock(&tcpc->access_lock);
		return ret;
	}
	tcpc->pd_wait_bc12_count = 0;
	tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_PD_WAIT_BC12);
#endif /* CONFIG_USB_PD_WAIT_BC12 */
#endif /* CONFIG_USB_POWER_DELIVERY */

	ret = __pd_put_cc_attached_event(tcpc, type);

	mutex_unlock(&tcpc->access_lock);

	return ret;
}

void pd_put_cc_detached_event(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->access_lock);

#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_USB_PD_WAIT_BC12
	tcpc->pd_wait_bc12_count = 0;
	tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_PD_WAIT_BC12);
#endif /* CONFIG_USB_PD_WAIT_BC12 */
#endif /* CONFIG_USB_POWER_DELIVERY */

	tcpci_notify_hard_reset_state(
		tcpc, TCP_HRESET_RESULT_FAIL);

	__pd_event_buf_reset(tcpc, TCP_DPM_RET_DROP_CC_DETACH);
	__pd_put_hw_event(tcpc, PD_HW_CC_DETACHED);

	tcpc->pd_wait_pe_idle = true;
	tcpc->pd_pe_running = false;
	tcpc->pd_wait_pr_swap_complete = false;
	tcpc->pd_hard_reset_event_pending = false;
	tcpc->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
	tcpc->pd_bist_mode = PD_BIST_MODE_DISABLE;
	tcpc->pd_ping_event_pending = false;

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
	tcpc->pd_during_direct_charge = false;
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	tcpc->pd_discard_pending = false;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

	mutex_unlock(&tcpc->access_lock);
}

void pd_put_recv_hard_reset_event(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->access_lock);

	tcpci_notify_hard_reset_state(
		tcpc, TCP_HRESET_SIGNAL_RECV);

	tcpc->pd_transmit_state = PD_TX_STATE_HARD_RESET;

	if ((!tcpc->pd_hard_reset_event_pending) &&
		(!tcpc->pd_wait_pe_idle) &&
		tcpc->pd_pe_running) {
		__pd_event_buf_reset(tcpc, TCP_DPM_RET_DROP_RECV_HRESET);
		__pd_put_hw_event(tcpc, PD_HW_RECV_HARD_RESET);
		tcpc->pd_bist_mode = PD_BIST_MODE_DISABLE;
		tcpc->pd_hard_reset_event_pending = true;
		tcpc->pd_ping_event_pending = false;

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
		tcpc->pd_during_direct_charge = false;
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */
	}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	tcpc->pd_discard_pending = false;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

	mutex_unlock(&tcpc->access_lock);
}

void pd_put_sent_hard_reset_event(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->access_lock);

	if (tcpc->pd_wait_hard_reset_complete)
		__pd_event_buf_reset(tcpc, TCP_DPM_RET_DROP_SENT_HRESET);
	else
		TCPC_DBG2("[HReset] Unattached\n");

	tcpc->pd_transmit_state = PD_TX_STATE_GOOD_CRC;
	__pd_put_pe_event(tcpc, PD_PE_HARD_RESET_COMPLETED);

	mutex_unlock(&tcpc->access_lock);
}

bool pd_put_pd_msg_event(struct tcpc_device *tcpc, struct pd_msg *pd_msg)
{
	uint32_t cnt, cmd, extend;

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	bool discard_pending = false;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

	struct pd_event evt = {
		.event_type = PD_EVT_PD_MSG,
		.pd_msg = pd_msg,
	};

	cnt = PD_HEADER_CNT(pd_msg->msg_hdr);
	cmd = PD_HEADER_TYPE(pd_msg->msg_hdr);
	extend = PD_HEADER_EXT(pd_msg->msg_hdr);

	/* bist mode */
	mutex_lock(&tcpc->access_lock);
	if (tcpc->pd_bist_mode != PD_BIST_MODE_DISABLE) {
		TCPC_DBG2("BIST_MODE_RX\n");
		__pd_free_event(tcpc, &evt);
		mutex_unlock(&tcpc->access_lock);
		return 0;
	}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	if (tcpc->pd_discard_pending &&
		(pd_msg->frame_type == TCPC_TX_SOP) &&
		(tcpc->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD)) {

		discard_pending = true;
		tcpc->pd_discard_pending = false;

		if ((cmd == PD_CTRL_GOOD_CRC) && (cnt == 0)) {
			TCPC_DBG2("RETRANSMIT\n");
			__pd_free_event(tcpc, &evt);
			mutex_unlock(&tcpc->access_lock);

			/* TODO: check it later */
			tcpc_disable_timer(tcpc, PD_TIMER_DISCARD);
			tcpci_retransmit(tcpc);
			return 0;
		}
	}
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_USB_PD_DROP_REPEAT_PING
	if (cnt == 0 && cmd == PD_CTRL_PING) {
		/* reset ping_test_mode only if cc_detached */
		if (!tcpc->pd_ping_event_pending) {
			TCPC_INFO("ping_test_mode\n");
			tcpc->pd_ping_event_pending = true;
			tcpci_set_bist_test_mode(tcpc, true);
		} else {
			__pd_free_event(tcpc, &evt);
			mutex_unlock(&tcpc->access_lock);
			return 0;
		}
	}
#endif	/* CONFIG_USB_PD_DROP_REPEAT_PING */

	if (cnt != 0 && cmd == PD_DATA_BIST && extend == 0)
		tcpc->pd_bist_mode = PD_BIST_MODE_EVENT_PENDING;

	mutex_unlock(&tcpc->access_lock);

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	if (discard_pending) {
		tcpc_disable_timer(tcpc, PD_TIMER_DISCARD);
		pd_put_hw_event(tcpc, PD_HW_TX_FAILED);
	}
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

	if (cnt != 0 && cmd == PD_DATA_VENDOR_DEF)
		return pd_put_vdm_event(tcpc, &evt, true);

	if (!pd_put_event(tcpc, &evt, true)) {
		pd_free_event(tcpc, &evt);
		return false;
	}

	return true;
}

static void pd_report_vbus_present(struct tcpc_device *tcpc)
{
	tcpc->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
	__pd_put_hw_event(tcpc, PD_HW_VBUS_PRESENT);
}

void pd_put_vbus_changed_event(struct tcpc_device *tcpc, bool from_ic)
{
	int vbus_valid;
	bool postpone_vbus_present = false;

	mutex_lock(&tcpc->access_lock);
	vbus_valid = tcpci_check_vbus_valid(tcpc);

	switch (tcpc->pd_wait_vbus_once) {
	case PD_WAIT_VBUS_VALID_ONCE:
		if (vbus_valid) {
#if CONFIG_USB_PD_VBUS_PRESENT_TOUT
			postpone_vbus_present = from_ic;
#endif	/* CONFIG_USB_PD_VBUS_PRESENT_TOUT */
			if (!postpone_vbus_present)
				pd_report_vbus_present(tcpc);
		}
		break;

	case PD_WAIT_VBUS_INVALID_ONCE:
		if (!vbus_valid) {
			tcpc->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
			__pd_put_hw_event(tcpc, PD_HW_VBUS_ABSENT);
		}
		break;
	}
	mutex_unlock(&tcpc->access_lock);

#if CONFIG_USB_PD_VBUS_PRESENT_TOUT
	if (postpone_vbus_present)
		tcpc_enable_timer(tcpc, PD_TIMER_VBUS_PRESENT);
#endif	/* CONFIG_USB_PD_VBUS_PRESENT_TOUT */
}

void pd_put_vbus_safe0v_event(struct tcpc_device *tcpc)
{
#ifdef CONFIG_USB_PD_SAFE0V_TIMEOUT
	tcpc_disable_timer(tcpc, PD_TIMER_VSAFE0V_TOUT);
#endif	/* CONFIG_USB_PD_SAFE0V_TIMEOUT */

	mutex_lock(&tcpc->access_lock);
	if (tcpc->pd_wait_vbus_once == PD_WAIT_VBUS_SAFE0V_ONCE) {
		tcpc->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
		tcpci_enable_force_discharge(tcpc, false, 0);
		__pd_put_hw_event(tcpc, PD_HW_VBUS_SAFE0V);
	}
	mutex_unlock(&tcpc->access_lock);
}

void pd_put_vbus_stable_event(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->access_lock);
	if (tcpc->pd_wait_vbus_once == PD_WAIT_VBUS_STABLE_ONCE) {
		tcpc->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
#ifdef CONFIG_USB_PD_SRC_HIGHCAP_POWER
		tcpci_enable_force_discharge(tcpc, false, 0);
#endif	/* CONFIG_USB_PD_SRC_HIGHCAP_POWER */
		__pd_put_hw_event(tcpc, PD_HW_VBUS_STABLE);
	}
	mutex_unlock(&tcpc->access_lock);
}

void pd_put_vbus_present_event(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->access_lock);
	pd_report_vbus_present(tcpc);
	mutex_unlock(&tcpc->access_lock);
}

/* ---- PD Notify TCPC ---- */

void pd_try_put_pe_idle_event(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	mutex_lock(&tcpc->access_lock);
	if (tcpc->pd_transmit_state < PD_TX_STATE_WAIT)
		__pd_put_pe_event(tcpc, PD_PE_IDLE);
	mutex_unlock(&tcpc->access_lock);
}

void pd_notify_pe_idle(struct pd_port *pd_port)
{
	bool notify_pe_idle = false;
	struct tcpc_device *tcpc = pd_port->tcpc;

	mutex_lock(&tcpc->access_lock);
	if (tcpc->pd_wait_pe_idle) {
		notify_pe_idle = true;
		tcpc->pd_wait_pe_idle = false;
	}

	tcpc->pd_pe_running = false;
	mutex_unlock(&tcpc->access_lock);

	pd_update_connect_state(pd_port, PD_CONNECT_NONE);

	if (notify_pe_idle)
		tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_PE_IDLE);
}

void pd_notify_pe_wait_vbus_once(struct pd_port *pd_port, int wait_evt)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	mutex_lock(&tcpc->access_lock);
	tcpc->pd_wait_vbus_once = wait_evt;
	mutex_unlock(&tcpc->access_lock);

	switch (wait_evt) {
	case PD_WAIT_VBUS_VALID_ONCE:
	case PD_WAIT_VBUS_INVALID_ONCE:
		pd_put_vbus_changed_event(tcpc, false);
		break;

	case PD_WAIT_VBUS_SAFE0V_ONCE:
#ifdef CONFIG_TCPC_VSAFE0V_DETECT
		if (tcpci_check_vsafe0v(tcpc, true)) {
			pd_put_vbus_safe0v_event(tcpc);
			break;
		}
#else
		pd_enable_timer(pd_port, PD_TIMER_VSAFE0V_DELAY);
#endif	/* CONFIG_TCPC_VSAFE0V_DETECT */

#ifdef CONFIG_USB_PD_SAFE0V_TIMEOUT
		pd_enable_timer(pd_port, PD_TIMER_VSAFE0V_TOUT);
#endif	/* CONFIG_USB_PD_SAFE0V_TIMEOUT */

		mutex_lock(&tcpc->access_lock);
		tcpci_enable_force_discharge(tcpc, true, 0);
		mutex_unlock(&tcpc->access_lock);
		break;
	}
}

void pd_notify_pe_error_recovery(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	mutex_lock(&tcpc->access_lock);

	tcpci_notify_hard_reset_state(
		tcpc, TCP_HRESET_RESULT_FAIL);

	tcpc->pd_wait_pr_swap_complete = false;
	__tcp_event_buf_reset(tcpc, TCP_DPM_RET_DROP_ERROR_REOCVERY);
	mutex_unlock(&tcpc->access_lock);

	tcpci_lock_typec(tcpc);
	tcpc_typec_error_recovery(tcpc);
	tcpci_unlock_typec(tcpc);
}

#ifdef CONFIG_USB_PD_RECV_HRESET_COUNTER
void pd_notify_pe_over_recv_hreset(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	mutex_lock(&tcpc->access_lock);
	tcpc->pd_wait_hard_reset_complete = false;
	tcpc->pd_wait_pr_swap_complete = false;
	mutex_unlock(&tcpc->access_lock);

	disable_irq(chip->irq);
	tcpci_lock_typec(tcpc);
	tcpci_init(tcpc, true);
	tcpci_set_cc(tcpc, TYPEC_CC_OPEN);
	tcpci_unlock_typec(tcpc);
	tcpci_set_rx_enable(tcpc, PD_RX_CAP_PE_IDLE);
	tcpc_enable_timer(tcpc, TYPEC_TIMER_ERROR_RECOVERY);
	enable_irq_wake(chip->irq);
}
#endif	/* CONFIG_USB_PD_RECV_HRESET_COUNTER */

void pd_notify_pe_transit_to_default(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	pd_update_connect_state(pd_port, PD_CONNECT_HARD_RESET);

	mutex_lock(&tcpc->access_lock);
	tcpc->pd_hard_reset_event_pending = false;
	tcpc->pd_wait_pr_swap_complete = false;
	tcpc->pd_bist_mode = PD_BIST_MODE_DISABLE;

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
	tcpc->pd_during_direct_charge = false;
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */
	mutex_unlock(&tcpc->access_lock);
}

void pd_notify_pe_hard_reset_completed(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	mutex_lock(&tcpc->access_lock);
	tcpci_notify_hard_reset_state(
		tcpc, TCP_HRESET_RESULT_DONE);
	mutex_unlock(&tcpc->access_lock);
}

void pd_notify_pe_send_hard_reset(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	mutex_lock(&tcpc->access_lock);
	tcpc->pd_transmit_state = PD_TX_STATE_WAIT_HARD_RESET;
	tcpci_notify_hard_reset_state(tcpc, TCP_HRESET_SIGNAL_SEND);
	mutex_unlock(&tcpc->access_lock);
}

void pd_notify_pe_execute_pr_swap(struct pd_port *pd_port, bool start_swap)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	pd_port->pe_data.during_swap = start_swap;
	mutex_lock(&tcpc->access_lock);
	tcpc->pd_wait_pr_swap_complete = true;
	mutex_unlock(&tcpc->access_lock);
}

void pd_notify_pe_cancel_pr_swap(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	if (!tcpc->pd_wait_pr_swap_complete)
		return;

	pd_port->pe_data.during_swap = false;
	mutex_lock(&tcpc->access_lock);
	tcpc->pd_wait_pr_swap_complete = false;
	mutex_unlock(&tcpc->access_lock);

	/*
	 *	CC_Alert was ignored if pd_wait_pr_swap_complete = true
	 *	So enable PDDebounce to detect CC_Again after cancel_pr_swap.
	 */

	tcpc_enable_timer(tcpc, TYPEC_TIMER_PDDEBOUNCE);

	if (!tcpci_check_vbus_valid(tcpc)
		&& (pd_port->request_v >= 4000)) {
		TCPC_DBG("cancel_pr_swap_vbus=0\n");
		pd_put_tcp_pd_event(pd_port, TCP_DPM_EVT_ERROR_RECOVERY);
	}
}

void pd_notify_pe_reset_protocol(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	mutex_lock(&tcpc->access_lock);
	tcpc->pd_wait_pr_swap_complete = false;
	mutex_unlock(&tcpc->access_lock);
}

void pd_noitfy_pe_bist_mode(struct pd_port *pd_port, uint8_t mode)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	mutex_lock(&tcpc->access_lock);
	tcpc->pd_bist_mode = mode;
	mutex_unlock(&tcpc->access_lock);
}

void pd_notify_pe_transmit_msg(
	struct pd_port *pd_port, uint8_t type)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	mutex_lock(&tcpc->access_lock);
	tcpc->pd_transmit_state = type;
	mutex_unlock(&tcpc->access_lock);
}

void pd_notify_pe_pr_changed(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	/* Check mutex later, actually,
	 * typec layer will ignore all cc-change during PR-SWAP
	 */

	/* mutex_lock(&tcpc->access_lock); */
	tcpc_typec_handle_pe_pr_swap(tcpc);
	/* mutex_unlock(&tcpc->access_lock); */
}

void pd_notify_pe_snk_explicit_contract(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	struct pe_data *pe_data = &pd_port->pe_data;
	struct tcpc_device *tcpc = pd_port->tcpc;

	if (pe_data->explicit_contract)
		return;

	if (tcpc->typec_remote_rp_level == TYPEC_CC_VOLT_SNK_3_0)
		pe_data->pd_traffic_control = PD_SINK_TX_OK;
	else
		pe_data->pd_traffic_control = PD_SINK_TX_NG;

#ifdef CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP
	if (pe_data->pd_traffic_control == PD_SINK_TX_OK) {
		pe_data->pd_traffic_control = PD_SINK_TX_START;
		pd_restart_timer(pd_port, PD_TIMER_SNK_FLOW_DELAY);
	}
#endif	/* CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP */
#endif /* CONFIG_USB_PD_REV30_COLLISION_AVOID */
}

void pd_notify_pe_src_explicit_contract(struct pd_port *pd_port)
{
	uint8_t pull = 0;

	struct pe_data *pe_data = &pd_port->pe_data;
	struct tcpc_device *tcpc = pd_port->tcpc;

	if (pe_data->explicit_contract) {
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
#ifdef CONFIG_USB_PD_REV30_SRC_FLOW_DELAY_STARTUP
		if (pd_check_rev30(pd_port) &&
			(pe_data->pd_traffic_control == PD_SOURCE_TX_START))
			pd_restart_timer(pd_port, PD_TIMER_SINK_TX);
#endif	/* CONFIG_USB_PD_REV30_SRC_FLOW_DELAY_STARTUP */
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
		return;
	}

	if (tcpc->typec_local_rp_level == TYPEC_RP_DFT)
		pull = TYPEC_CC_RP_1_5;

#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	if (pd_check_rev30(pd_port)) {
		pull = TYPEC_CC_RP_3_0;

#ifdef CONFIG_USB_PD_REV30_SRC_FLOW_DELAY_STARTUP
		pe_data->pd_traffic_control = PD_SOURCE_TX_START;
		pd_enable_timer(pd_port, PD_TIMER_SINK_TX);
#else
		pe_data->pd_traffic_control = PD_SINK_TX_OK;
#endif	/* CONFIG_USB_PD_REV30_SRC_FLOW_DELAY_STARTUP */
	}
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */

	if (pull) {
		tcpci_lock_typec(tcpc);
		tcpci_set_cc(tcpc, pull);
		tcpci_unlock_typec(tcpc);
	}
}

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
void pd_notify_pe_direct_charge(struct pd_port *pd_port, bool en)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	/* TODO: check it later */
	if (pd_port->request_apdo)
		en = true;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	mutex_lock(&tcpc->access_lock);
	tcpc->pd_during_direct_charge = en;
	mutex_unlock(&tcpc->access_lock);
}
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

/* ---- init  ---- */
static int tcpc_event_thread_fn(void *data)
{
	struct tcpc_device *tcpc = data;
	struct sched_param sch_param = {.sched_priority = MAX_RT_PRIO - 2};
	int ret = 0;

	/* set_user_nice(current, -20); */
	/* current->flags |= PF_NOFREEZE;*/

	sched_setscheduler(current, SCHED_FIFO, &sch_param);

	while (true) {
		ret = wait_event_interruptible(tcpc->event_wait_que,
				atomic_read(&tcpc->pending_event) ||
				kthread_should_stop());
		if (kthread_should_stop() || ret) {
			dev_notice(&tcpc->dev, "%s exits(%d)\n", __func__, ret);
			break;
		}
		do {
			atomic_dec_if_positive(&tcpc->pending_event);
		} while (pd_policy_engine_run(tcpc) && !kthread_should_stop());
	}

	return 0;
}

int tcpci_event_init(struct tcpc_device *tcpc)
{
	init_waitqueue_head(&tcpc->event_wait_que);
	atomic_set(&tcpc->pending_event, 0);
	tcpc->event_task = kthread_run(tcpc_event_thread_fn, tcpc,
				       "tcpc_event_%s", tcpc->desc.name);

	return 0;
}

int tcpci_event_deinit(struct tcpc_device *tcpc)
{
	if (tcpc->event_task != NULL)
		kthread_stop(tcpc->event_task);

	return 0;
}
