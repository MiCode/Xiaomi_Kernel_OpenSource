/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * TCPC Interface for event handler
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
static void postpone_vdm_event(struct tcpc_device *tcpc_dev)
{
	/*
	 * Postpone VDM retry event due to the retry reason
	 * maybe interrupt by some PD event ....
	 */

	struct pd_event *vdm_event = &tcpc_dev->pd_vdm_event;

	if (tcpc_dev->pd_pending_vdm_event && vdm_event->pd_msg) {
		tcpc_dev->pd_postpone_vdm_timeout = false;
		tcpc_restart_timer(tcpc_dev, PD_PE_VDM_POSTPONE);
	}
}
#endif	/* CONFIG_USB_PD_POSTPONE_VDM */

struct pd_msg *__pd_alloc_msg(struct tcpc_device *tcpc_dev)
{
	int i;
	uint8_t mask;

	for (i = 0, mask = 1; i < PD_MSG_BUF_SIZE; i++, mask <<= 1) {
		if ((mask & tcpc_dev->pd_msg_buffer_allocated) == 0) {
			tcpc_dev->pd_msg_buffer_allocated |= mask;
			return tcpc_dev->pd_msg_buffer + i;
		}
	}

	PD_ERR("pd_alloc_msg failed\r\n");
	PD_BUG_ON(true);

	return (struct pd_msg *)NULL;
}

struct pd_msg *pd_alloc_msg(struct tcpc_device *tcpc_dev)
{
	struct pd_msg *pd_msg = NULL;

	mutex_lock(&tcpc_dev->access_lock);
	pd_msg = __pd_alloc_msg(tcpc_dev);
	mutex_unlock(&tcpc_dev->access_lock);

	return pd_msg;
}

static void __pd_free_msg(struct tcpc_device *tcpc_dev, struct pd_msg *pd_msg)
{
	int index = pd_msg - tcpc_dev->pd_msg_buffer;
	uint8_t mask = 1 << index;

	PD_BUG_ON((mask & tcpc_dev->pd_msg_buffer_allocated) == 0);
	tcpc_dev->pd_msg_buffer_allocated &= (~mask);
}

static void __pd_free_event(
		struct tcpc_device *tcpc_dev, struct pd_event *pd_event)
{
	if (pd_event->pd_msg) {
		__pd_free_msg(tcpc_dev, pd_event->pd_msg);
		pd_event->pd_msg = NULL;
	}
}

void pd_free_msg(struct tcpc_device *tcpc_dev, struct pd_msg *pd_msg)
{
	mutex_lock(&tcpc_dev->access_lock);
	__pd_free_msg(tcpc_dev, pd_msg);
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_free_event(struct tcpc_device *tcpc_dev, struct pd_event *pd_event)
{
	mutex_lock(&tcpc_dev->access_lock);
	__pd_free_event(tcpc_dev, pd_event);
	mutex_unlock(&tcpc_dev->access_lock);
}

/*----------------------------------------------------------------------------*/

static bool __pd_get_event(
	struct tcpc_device *tcpc_dev, struct pd_event *pd_event)
{
	int index = 0;

	if (tcpc_dev->pd_event_count <= 0)
		return false;

	tcpc_dev->pd_event_count--;

	*pd_event =
		tcpc_dev->pd_event_ring_buffer[tcpc_dev->pd_event_head_index];

	if (tcpc_dev->pd_event_count) {
		index = tcpc_dev->pd_event_head_index + 1;
		index %= PD_EVENT_BUF_SIZE;
	}
	tcpc_dev->pd_event_head_index = index;
	return true;
}

bool pd_get_event(struct tcpc_device *tcpc_dev, struct pd_event *pd_event)
{
	bool ret;

	mutex_lock(&tcpc_dev->access_lock);
	ret = __pd_get_event(tcpc_dev, pd_event);
	mutex_unlock(&tcpc_dev->access_lock);
	return ret;
}

static bool __pd_put_event(struct tcpc_device *tcpc_dev,
	const struct pd_event *pd_event, bool from_port_partner)
{
	int index;

#ifdef CONFIG_USB_PD_POSTPONE_OTHER_VDM
	if (from_port_partner)
		postpone_vdm_event(tcpc_dev);
#endif	/* CONFIG_USB_PD_POSTPONE_OTHER_VDM */

	if (tcpc_dev->pd_event_count >= PD_EVENT_BUF_SIZE) {
		PD_ERR("pd_put_event failed\r\n");
		return false;
	}

	index = (tcpc_dev->pd_event_head_index + tcpc_dev->pd_event_count);
	index %= PD_EVENT_BUF_SIZE;

	tcpc_dev->pd_event_count++;
	tcpc_dev->pd_event_ring_buffer[index] = *pd_event;

	atomic_inc(&tcpc_dev->pending_event);
	wake_up_interruptible(&tcpc_dev->event_loop_wait_que);
	return true;
}

bool pd_put_event(struct tcpc_device *tcpc_dev, const struct pd_event *pd_event,
	bool from_port_partner)
{
	bool ret;

	mutex_lock(&tcpc_dev->access_lock);
	ret = __pd_put_event(tcpc_dev, pd_event, from_port_partner);
	mutex_unlock(&tcpc_dev->access_lock);

	return ret;
}

/*----------------------------------------------------------------------------*/

static inline void pd_get_attention_event(
	struct tcpc_device *tcpc_dev, struct pd_event *pd_event)
{
	struct pd_event attention_evt = {
		.event_type = PD_EVT_PD_MSG,
		.msg = PD_DATA_VENDOR_DEF,
		.pd_msg = NULL,
	};

	*pd_event = attention_evt;
	pd_event->pd_msg = __pd_alloc_msg(tcpc_dev);

	if (pd_event->pd_msg == NULL)
		return;

	tcpc_dev->pd_pending_vdm_attention = false;
	*pd_event->pd_msg = tcpc_dev->pd_attention_vdm_msg;
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

bool pd_get_vdm_event(struct tcpc_device *tcpc_dev, struct pd_event *pd_event)
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

	struct pd_event *vdm_event = &tcpc_dev->pd_vdm_event;

	if (tcpc_dev->pd_pending_vdm_discard) {
		mutex_lock(&tcpc_dev->access_lock);
		*pd_event = discard_evt;
		tcpc_dev->pd_pending_vdm_discard = false;
		mutex_unlock(&tcpc_dev->access_lock);
		return true;
	}

	if (tcpc_dev->pd_pending_vdm_event) {
		if (vdm_event->pd_msg && !tcpc_dev->pd_postpone_vdm_timeout)
			return false;

		mutex_lock(&tcpc_dev->access_lock);
		if (tcpc_dev->pd_pending_vdm_good_crc) {
			*pd_event = delay_evt;
			tcpc_dev->pd_pending_vdm_good_crc = false;
		} else if (tcpc_dev->pd_pending_vdm_reset) {
			*pd_event = reset_evt;
			tcpc_dev->pd_pending_vdm_reset = false;
		} else {
			*pd_event = *vdm_event;
			tcpc_dev->pd_pending_vdm_event = false;
		}

		mutex_unlock(&tcpc_dev->access_lock);
		return true;
	}

	if (tcpc_dev->pd_pending_vdm_attention
		&& pd_check_vdm_state_ready(&tcpc_dev->pd_port)) {
		mutex_lock(&tcpc_dev->access_lock);
		pd_get_attention_event(tcpc_dev, pd_event);
		mutex_unlock(&tcpc_dev->access_lock);
		return true;
	}

	return false;
}

static inline bool reset_pe_vdm_state(
		struct tcpc_device *tcpc_dev, uint32_t vdm_hdr)
{
	bool vdm_reset = false;
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	if (PD_VDO_SVDM(vdm_hdr)) {
		if (PD_VDO_CMDT(vdm_hdr) == CMDT_INIT)
			vdm_reset = true;
	} else {
		if (pd_port->data_role == PD_ROLE_UFP)
			vdm_reset = true;
	}

	if (vdm_reset)
		tcpc_dev->pd_pending_vdm_reset = true;

	return vdm_reset;
}

static inline bool pd_is_init_attention_event(
	struct tcpc_device *tcpc_dev, struct pd_event *pd_event)
{
	uint32_t vdm_hdr = pd_event->pd_msg->payload[0];

	if ((PD_VDO_CMDT(vdm_hdr) == CMDT_INIT) &&
			PD_VDO_CMD(vdm_hdr) == CMD_ATTENTION) {
		return true;
	}

	return false;
}

bool pd_put_vdm_event(struct tcpc_device *tcpc_dev,
		struct pd_event *pd_event, bool from_port_partner)
{
	bool ignore_evt = false;
	struct pd_msg *pd_msg = pd_event->pd_msg;

	mutex_lock(&tcpc_dev->access_lock);

	if (from_port_partner &&
		pd_is_init_attention_event(tcpc_dev, pd_event)) {
		TCPC_DBG("AttEvt\r\n");
		ignore_evt = true;
		tcpc_dev->pd_pending_vdm_attention = true;
		tcpc_dev->pd_attention_vdm_msg = *pd_msg;

		/* do not really wake up process*/
		atomic_inc(&tcpc_dev->pending_event);
		wake_up_interruptible(&tcpc_dev->event_loop_wait_que);
	}

	if (tcpc_dev->pd_pending_vdm_event && (!ignore_evt)) {
		/* If message from port partner, we have to overwrite it */
		/* If message from TCPM, we will reset_vdm later */
		ignore_evt = !from_port_partner;

		if (from_port_partner) {
			if (pd_event_ctrl_msg_match(
					&tcpc_dev->pd_vdm_event,
					PD_CTRL_GOOD_CRC)) {
				TCPC_DBG2("PostponeVDM GoodCRC\r\n");
				tcpc_dev->pd_pending_vdm_good_crc = true;
			}

			__pd_free_event(tcpc_dev, &tcpc_dev->pd_vdm_event);
		}
	}

	if (ignore_evt) {
		__pd_free_event(tcpc_dev, pd_event);
		mutex_unlock(&tcpc_dev->access_lock);
		return false;
	}

	tcpc_dev->pd_vdm_event = *pd_event;
	tcpc_dev->pd_pending_vdm_event = true;
	tcpc_dev->pd_postpone_vdm_timeout = true;

	if (from_port_partner) {

		PD_BUG_ON(pd_msg == NULL);
		/* pd_msg->time_stamp = 0; */
		tcpc_dev->pd_last_vdm_msg = *pd_msg;
		reset_pe_vdm_state(tcpc_dev, pd_msg->payload[0]);

#ifdef CONFIG_USB_PD_POSTPONE_FIRST_VDM
		postpone_vdm_event(tcpc_dev);
		mutex_unlock(&tcpc_dev->access_lock);
		return true;
#endif	/* CONFIG_USB_PD_POSTPONE_FIRST_VDM */
	}

	atomic_inc(&tcpc_dev->pending_event); /* do not really wake up process*/
	wake_up_interruptible(&tcpc_dev->event_loop_wait_que);
	mutex_unlock(&tcpc_dev->access_lock);

	return true;
}

bool pd_put_last_vdm_event(struct tcpc_device *tcpc_dev)
{
	struct pd_msg *pd_msg = &tcpc_dev->pd_last_vdm_msg;
	struct pd_event *vdm_event = &tcpc_dev->pd_vdm_event;

	mutex_lock(&tcpc_dev->access_lock);

	tcpc_dev->pd_pending_vdm_discard = true;
	atomic_inc(&tcpc_dev->pending_event); /* do not really wake up process*/
	wake_up_interruptible(&tcpc_dev->event_loop_wait_que);

	/* If the last VDM event isn't INIT event, don't put it again */
	if (!reset_pe_vdm_state(tcpc_dev, pd_msg->payload[0])) {
		mutex_unlock(&tcpc_dev->access_lock);
		return true;
	}

	vdm_event->event_type = PD_EVT_HW_MSG;
	vdm_event->msg = PD_HW_RETRY_VDM;

	if (tcpc_dev->pd_pending_vdm_event)
		__pd_free_event(tcpc_dev, &tcpc_dev->pd_vdm_event);

	vdm_event->pd_msg = __pd_alloc_msg(tcpc_dev);

	if (vdm_event->pd_msg == NULL) {
		mutex_unlock(&tcpc_dev->access_lock);
		return false;
	}

	*vdm_event->pd_msg = *pd_msg;
	tcpc_dev->pd_pending_vdm_event = true;
	tcpc_dev->pd_postpone_vdm_timeout = true;

#ifdef CONFIG_USB_PD_POSTPONE_RETRY_VDM
	postpone_vdm_event(tcpc_dev);
#endif	/* CONFIG_USB_PD_POSTPONE_RETRY_VDM */

	mutex_unlock(&tcpc_dev->access_lock);
	return true;
}

/*----------------------------------------------------------------------------*/

static bool __pd_get_deferred_tcp_event(
	struct tcpc_device *tcpc_dev, struct tcp_dpm_event *tcp_event)
{
	int index = 0;

	if (tcpc_dev->tcp_event_count <= 0)
		return false;

	tcpc_dev->tcp_event_count--;

	*tcp_event =
		tcpc_dev->tcp_event_ring_buffer[tcpc_dev->tcp_event_head_index];

	if (tcpc_dev->tcp_event_count) {
		index = tcpc_dev->tcp_event_head_index + 1;
		index %= TCP_EVENT_BUF_SIZE;
	}
	tcpc_dev->tcp_event_head_index = index;
	return true;
}

bool pd_get_deferred_tcp_event(
	struct tcpc_device *tcpc_dev, struct tcp_dpm_event *tcp_event)
{
	bool ret;

	mutex_lock(&tcpc_dev->access_lock);
	ret = __pd_get_deferred_tcp_event(tcpc_dev, tcp_event);
	mutex_unlock(&tcpc_dev->access_lock);

#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	if (tcpc_dev->tcp_event_count)
		tcpc_restart_timer(tcpc_dev, PD_TIMER_DEFERRED_EVT);
	else
		tcpc_disable_timer(tcpc_dev, PD_TIMER_DEFERRED_EVT);
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#endif	/* CONFIG_USB_PD_REV30 */

	return ret;
}

static bool __pd_put_deferred_tcp_event(
	struct tcpc_device *tcpc_dev, const struct tcp_dpm_event *tcp_event)
{
	int index;

	index = (tcpc_dev->tcp_event_head_index + tcpc_dev->tcp_event_count);
	index %= TCP_EVENT_BUF_SIZE;

	tcpc_dev->tcp_event_count++;
	tcpc_dev->tcp_event_ring_buffer[index] = *tcp_event;

	atomic_inc(&tcpc_dev->pending_event); /* do not really wake up process*/
	wake_up_interruptible(&tcpc_dev->event_loop_wait_que);
	return true;
}

bool pd_put_deferred_tcp_event(
	struct tcpc_device *tcpc_dev, const struct tcp_dpm_event *tcp_event)
{
	bool ret;
	struct pd_port *pd_port = &tcpc_dev->pd_port;

	if (!tcpc_dev->pd_pe_running || tcpc_dev->pd_wait_pe_idle) {
		PD_ERR("pd_put_tcp_event failed0\r\n");
		return false;
	}

	if (tcpc_dev->tcp_event_count >= TCP_EVENT_BUF_SIZE) {
		PD_ERR("pd_put_tcp_event failed1\r\n");
		return false;
	}

	if (tcpc_dev->pd_wait_hard_reset_complete) {
		PD_ERR("pd_put_tcp_event failed2\r\n");
		return false;
	}

#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	if (tcpc_dev->tcp_event_count == 0)
		tcpc_enable_timer(tcpc_dev, PD_TIMER_DEFERRED_EVT);
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#endif	/* CONFIG_USB_PD_REV30 */

	mutex_lock(&pd_port->pd_lock);

	switch (tcp_event->event_id) {
	case TCP_DPM_EVT_DISCOVER_CABLE:
	case TCP_DPM_EVT_CABLE_SOFTRESET:
		dpm_reaction_set(pd_port,
			DPM_REACTION_DYNAMIC_VCONN |
			DPM_REACTION_VCONN_STABLE_DELAY);
		break;
	}

	mutex_lock(&tcpc_dev->access_lock);
	ret = __pd_put_deferred_tcp_event(tcpc_dev, tcp_event);
	mutex_unlock(&tcpc_dev->access_lock);

#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	if (ret)
		pd_port->pe_data.pd_traffic_idle = false;
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */

	dpm_reaction_set_ready_once(pd_port);
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
		tcp_event->event_cb(pd_port->tcpc_dev, ret, tcp_event);

	pd_port->tcp_event_id_2nd = TCP_DPM_EVT_UNKONW;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */
}

void pd_notify_tcp_event_2nd_result(struct pd_port *pd_port, int ret)
{
#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	struct tcp_dpm_event *tcp_event = &pd_port->tcp_event;

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

	TCPC_DBG2("tcp_event_2nd:evt%d=%d\r\n",
		pd_port->tcp_event_id_2nd, ret);

	if (tcp_event->event_cb != NULL)
		tcp_event->event_cb(pd_port->tcpc_dev, ret, tcp_event);

	pd_port->tcp_event_id_2nd = TCP_DPM_EVT_UNKONW;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */
}

void pd_notify_tcp_event_1st_result(struct pd_port *pd_port, int ret)
{
	bool cb = true;
	struct tcp_dpm_event *tcp_event = &pd_port->tcp_event;

	if (pd_port->tcp_event_id_1st == TCP_DPM_EVT_UNKONW)
		return;

	TCPC_DBG2("tcp_event_1st:evt%d=%d\r\n",
		pd_port->tcp_event_id_1st, ret);

#ifdef CONFIG_USB_PD_TCPM_CB_2ND
	if (ret == TCP_DPM_RET_SENT) {
		cb = false;
		pd_port->tcp_event_id_2nd = tcp_event->event_id;
	}
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	if (cb && tcp_event->event_cb != NULL)
		tcp_event->event_cb(pd_port->tcpc_dev, ret, tcp_event);

	pd_port->tcp_event_id_1st = TCP_DPM_EVT_UNKONW;
}

static void __tcp_event_buf_reset(
	struct tcpc_device *tcpc_dev, uint8_t reason)
{
	struct tcp_dpm_event tcp_event;

	pd_notify_tcp_event_2nd_result(&tcpc_dev->pd_port, reason);

	while (__pd_get_deferred_tcp_event(tcpc_dev, &tcp_event)) {
		if (tcp_event.event_cb != NULL)
			tcp_event.event_cb(tcpc_dev, reason, &tcp_event);
	}
}

void pd_notify_tcp_event_buf_reset(struct pd_port *pd_port, uint8_t reason)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	pd_notify_tcp_event_1st_result(pd_port, reason);

	mutex_lock(&tcpc_dev->access_lock);
	__tcp_event_buf_reset(tcpc_dev, reason);
	mutex_unlock(&tcpc_dev->access_lock);
}

/*----------------------------------------------------------------------------*/

static void __pd_event_buf_reset(struct tcpc_device *tcpc_dev, uint8_t reason)
{
	struct pd_event pd_event;

	tcpc_dev->pd_hard_reset_event_pending = false;
	while (__pd_get_event(tcpc_dev, &pd_event))
		__pd_free_event(tcpc_dev, &pd_event);

	if (tcpc_dev->pd_pending_vdm_event) {
		__pd_free_event(tcpc_dev, &tcpc_dev->pd_vdm_event);
		tcpc_dev->pd_pending_vdm_event = false;
	}

	tcpc_dev->pd_pending_vdm_reset = false;
	tcpc_dev->pd_pending_vdm_good_crc = false;
	tcpc_dev->pd_pending_vdm_attention = false;
	tcpc_dev->pd_pending_vdm_discard = false;

	__tcp_event_buf_reset(tcpc_dev, reason);
	/* PD_BUG_ON(tcpc_dev->pd_msg_buffer_allocated != 0); */
}

void pd_event_buf_reset(struct tcpc_device *tcpc_dev)
{
	mutex_lock(&tcpc_dev->access_lock);
	__pd_event_buf_reset(tcpc_dev, TCP_DPM_RET_DROP_CC_DETACH);
	mutex_unlock(&tcpc_dev->access_lock);
}

/*----------------------------------------------------------------------------*/

static inline bool __pd_put_hw_event(
	struct tcpc_device *tcpc_dev, uint8_t hw_event)
{
	struct pd_event evt = {
		.event_type = PD_EVT_HW_MSG,
		.msg = hw_event,
		.pd_msg = NULL,
	};

	return __pd_put_event(tcpc_dev, &evt, false);
}

static inline bool __pd_put_pe_event(
	struct tcpc_device *tcpc_dev, uint8_t pe_event)
{
	struct pd_event evt = {
		.event_type = PD_EVT_PE_MSG,
		.msg = pe_event,
		.pd_msg = NULL,
	};

	return __pd_put_event(tcpc_dev, &evt, false);
}

bool pd_put_cc_attached_event(
		struct tcpc_device *tcpc_dev, uint8_t type)
{
	struct pd_event evt = {
		.event_type = PD_EVT_HW_MSG,
		.msg = PD_HW_CC_ATTACHED,
		.msg_sec = type,
		.pd_msg = NULL,
	};

	switch (tcpc_dev->typec_attach_new) {
	case TYPEC_ATTACHED_SNK:
	case TYPEC_ATTACHED_SRC:
		tcpc_dev->pd_pe_running = true;
		tcpc_dev->pd_wait_pe_idle = false;
		break;
#ifdef CONFIG_TYPEC_CAP_DBGACC_SNK
	case TYPEC_ATTACHED_DBGACC_SNK:
		tcpc_dev->pd_pe_running = true;
		tcpc_dev->pd_wait_pe_idle = false;
		break;
#endif	/* CONFIG_TYPEC_CAP_DBGACC_SNK */
	default:
		break;
	}

	return pd_put_event(tcpc_dev, &evt, false);
}

void pd_put_cc_detached_event(struct tcpc_device *tcpc_dev)
{
	mutex_lock(&tcpc_dev->access_lock);

	tcpci_notify_hard_reset_state(
		tcpc_dev, TCP_HRESET_RESULT_FAIL);

	__pd_event_buf_reset(tcpc_dev, TCP_DPM_RET_DROP_CC_DETACH);
	__pd_put_hw_event(tcpc_dev, PD_HW_CC_DETACHED);

	tcpc_dev->pd_wait_pe_idle = true;
	tcpc_dev->pd_pe_running = false;
	tcpc_dev->pd_wait_pr_swap_complete = false;
	tcpc_dev->pd_hard_reset_event_pending = false;
	tcpc_dev->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
	tcpc_dev->pd_bist_mode = PD_BIST_MODE_DISABLE;
	tcpc_dev->pd_ping_event_pending = false;

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
	tcpc_dev->pd_during_direct_charge = false;
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	tcpc_dev->pd_discard_pending = false;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_put_recv_hard_reset_event(struct tcpc_device *tcpc_dev)
{
	mutex_lock(&tcpc_dev->access_lock);

	tcpci_notify_hard_reset_state(
		tcpc_dev, TCP_HRESET_SIGNAL_RECV);

	tcpc_dev->pd_transmit_state = PD_TX_STATE_HARD_RESET;

	if ((!tcpc_dev->pd_hard_reset_event_pending) &&
		(!tcpc_dev->pd_wait_pe_idle) &&
		tcpc_dev->pd_pe_running) {
		__pd_event_buf_reset(tcpc_dev, TCP_DPM_RET_DROP_RECV_HRESET);
		__pd_put_hw_event(tcpc_dev, PD_HW_RECV_HARD_RESET);
		tcpc_dev->pd_bist_mode = PD_BIST_MODE_DISABLE;
		tcpc_dev->pd_hard_reset_event_pending = true;
		tcpc_dev->pd_ping_event_pending = false;

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
		tcpc_dev->pd_during_direct_charge = false;
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */
	}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	tcpc_dev->pd_discard_pending = false;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_put_sent_hard_reset_event(struct tcpc_device *tcpc_dev)
{
	mutex_lock(&tcpc_dev->access_lock);

	if (tcpc_dev->pd_wait_hard_reset_complete)
		__pd_event_buf_reset(tcpc_dev, TCP_DPM_RET_DROP_SENT_HRESET);
	else
		TCPC_DBG2("[HReset] Unattached\r\n");

	tcpc_dev->pd_transmit_state = PD_TX_STATE_GOOD_CRC;
	__pd_put_pe_event(tcpc_dev, PD_PE_HARD_RESET_COMPLETED);

	mutex_unlock(&tcpc_dev->access_lock);
}

bool pd_put_pd_msg_event(struct tcpc_device *tcpc_dev, struct pd_msg *pd_msg)
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
	mutex_lock(&tcpc_dev->access_lock);
	if (tcpc_dev->pd_bist_mode != PD_BIST_MODE_DISABLE) {
		TCPC_DBG2("BIST_MODE_RX\r\n");
		__pd_free_event(tcpc_dev, &evt);
		mutex_unlock(&tcpc_dev->access_lock);
		return 0;
	}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	if (tcpc_dev->pd_discard_pending &&
		(pd_msg->frame_type == TCPC_TX_SOP) &&
		(tcpc_dev->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD)) {

		discard_pending = true;
		tcpc_dev->pd_discard_pending = false;

		if ((cmd == PD_CTRL_GOOD_CRC) && (cnt == 0)) {
			TCPC_DBG2("RETRANSMIT\r\n");
			__pd_free_event(tcpc_dev, &evt);
			mutex_unlock(&tcpc_dev->access_lock);

			/* TODO: check it later */
			tcpc_disable_timer(tcpc_dev, PD_TIMER_DISCARD);
			tcpci_retransmit(tcpc_dev);
			return 0;
		}
	}
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_USB_PD_DROP_REPEAT_PING
	if (cnt == 0 && cmd == PD_CTRL_PING) {
		/* reset ping_test_mode only if cc_detached */
		if (!tcpc_dev->pd_ping_event_pending) {
			TCPC_INFO("ping_test_mode\r\n");
			tcpc_dev->pd_ping_event_pending = true;
			tcpci_set_bist_test_mode(tcpc_dev, true);
		} else {
			__pd_free_event(tcpc_dev, &evt);
			mutex_unlock(&tcpc_dev->access_lock);
			return 0;
		}
	}
#endif	/* CONFIG_USB_PD_DROP_REPEAT_PING */

	if (cnt != 0 && cmd == PD_DATA_BIST && extend == 0)
		tcpc_dev->pd_bist_mode = PD_BIST_MODE_EVENT_PENDING;

	mutex_unlock(&tcpc_dev->access_lock);

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	if (discard_pending) {
		tcpc_disable_timer(tcpc_dev, PD_TIMER_DISCARD);
		pd_put_hw_event(tcpc_dev, PD_HW_TX_FAILED);
	}
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

	if (cnt != 0 && cmd == PD_DATA_VENDOR_DEF)
		return pd_put_vdm_event(tcpc_dev, &evt, true);

	if (!pd_put_event(tcpc_dev, &evt, true)) {
		pd_free_event(tcpc_dev, &evt);
		return false;
	}

	return true;
}

static void pd_report_vbus_present(struct tcpc_device *tcpc_dev)
{
	tcpc_dev->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
	__pd_put_hw_event(tcpc_dev, PD_HW_VBUS_PRESENT);
}

void pd_put_vbus_changed_event(struct tcpc_device *tcpc_dev, bool from_ic)
{
	int vbus_valid;
	bool postpone_vbus_present = false;

	mutex_lock(&tcpc_dev->access_lock);
	vbus_valid = tcpci_check_vbus_valid(tcpc_dev);

	switch (tcpc_dev->pd_wait_vbus_once) {
	case PD_WAIT_VBUS_VALID_ONCE:
		if (vbus_valid) {
#if CONFIG_USB_PD_VBUS_PRESENT_TOUT
			postpone_vbus_present = from_ic;
#endif	/* CONFIG_USB_PD_VBUS_PRESENT_TOUT */
			if (!postpone_vbus_present)
				pd_report_vbus_present(tcpc_dev);
		}
		break;

	case PD_WAIT_VBUS_INVALID_ONCE:
		if (!vbus_valid) {
			tcpc_dev->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
			__pd_put_hw_event(tcpc_dev, PD_HW_VBUS_ABSENT);
		}
		break;
	}
	mutex_unlock(&tcpc_dev->access_lock);

#if CONFIG_USB_PD_VBUS_PRESENT_TOUT
	if (postpone_vbus_present)
		tcpc_enable_timer(tcpc_dev, PD_TIMER_VBUS_PRESENT);
#endif	/* CONFIG_USB_PD_VBUS_PRESENT_TOUT */
}

void pd_put_vbus_safe0v_event(struct tcpc_device *tcpc_dev)
{
#ifdef CONFIG_USB_PD_SAFE0V_TIMEOUT
	tcpc_disable_timer(tcpc_dev, PD_TIMER_VSAFE0V_TOUT);
#endif	/* CONFIG_USB_PD_SAFE0V_TIMEOUT */

	if (tcpc_dev->pd_wait_vbus_once == PD_WAIT_VBUS_SAFE0V_ONCE)
		tcpci_disable_force_discharge(tcpc_dev);

	mutex_lock(&tcpc_dev->access_lock);
	if (tcpc_dev->pd_wait_vbus_once == PD_WAIT_VBUS_SAFE0V_ONCE) {
		tcpc_dev->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
		__pd_put_hw_event(tcpc_dev, PD_HW_VBUS_SAFE0V);
	}
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_put_vbus_stable_event(struct tcpc_device *tcpc_dev)
{
#ifdef CONFIG_USB_PD_SRC_HIGHCAP_POWER
	if (tcpc_dev->pd_wait_vbus_once == PD_WAIT_VBUS_STABLE_ONCE)
		tcpci_disable_force_discharge(tcpc_dev);
#endif	/* CONFIG_USB_PD_SRC_HIGHCAP_POWER */

	mutex_lock(&tcpc_dev->access_lock);
	if (tcpc_dev->pd_wait_vbus_once == PD_WAIT_VBUS_STABLE_ONCE) {
		tcpc_dev->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
		__pd_put_hw_event(tcpc_dev, PD_HW_VBUS_STABLE);
	}
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_put_vbus_present_event(struct tcpc_device *tcpc_dev)
{
	mutex_lock(&tcpc_dev->access_lock);
	pd_report_vbus_present(tcpc_dev);
	mutex_unlock(&tcpc_dev->access_lock);
}

/* ---- PD Notify TCPC ---- */

void pd_try_put_pe_idle_event(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	if (tcpc_dev->pd_transmit_state < PD_TX_STATE_WAIT)
		__pd_put_pe_event(tcpc_dev, PD_PE_IDLE);
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_idle(struct pd_port *pd_port)
{
	bool notify_pe_idle = false;
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	if (tcpc_dev->pd_wait_pe_idle) {
		notify_pe_idle = true;
		tcpc_dev->pd_wait_pe_idle = false;
	}

	tcpc_dev->pd_pe_running = false;
	mutex_unlock(&tcpc_dev->access_lock);

	pd_update_connect_state(pd_port, PD_CONNECT_NONE);

	if (notify_pe_idle)
		tcpc_enable_timer(tcpc_dev, TYPEC_RT_TIMER_PE_IDLE);
}

void pd_notify_pe_wait_vbus_once(struct pd_port *pd_port, int wait_evt)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_wait_vbus_once = wait_evt;
	mutex_unlock(&tcpc_dev->access_lock);

	switch (wait_evt) {
	case PD_WAIT_VBUS_VALID_ONCE:
	case PD_WAIT_VBUS_INVALID_ONCE:
		pd_put_vbus_changed_event(tcpc_dev, false);
		break;

	case PD_WAIT_VBUS_SAFE0V_ONCE:
#ifdef CONFIG_TCPC_VSAFE0V_DETECT
		if (tcpci_check_vsafe0v(tcpc_dev, true)) {
			pd_put_vbus_safe0v_event(tcpc_dev);
			break;
		}
#else
		pd_enable_timer(pd_port, PD_TIMER_VSAFE0V_DELAY);
#endif	/* CONFIG_TCPC_VSAFE0V_DETECT */

#ifdef CONFIG_USB_PD_SAFE0V_TIMEOUT
		pd_enable_timer(pd_port, PD_TIMER_VSAFE0V_TOUT);
#endif	/* CONFIG_USB_PD_SAFE0V_TIMEOUT */

		tcpci_enable_force_discharge(tcpc_dev, 0);
		break;
	}
}

void pd_notify_pe_error_recovery(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);

	tcpci_notify_hard_reset_state(
		tcpc_dev, TCP_HRESET_RESULT_FAIL);

	tcpc_dev->pd_wait_pr_swap_complete = false;
	__tcp_event_buf_reset(tcpc_dev, TCP_DPM_RET_DROP_ERROR_REOCVERY);
	mutex_unlock(&tcpc_dev->access_lock);

	tcpc_typec_error_recovery(tcpc_dev);
}

#ifdef CONFIG_USB_PD_RECV_HRESET_COUNTER
void pd_notify_pe_over_recv_hreset(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_wait_hard_reset_complete = false;
	tcpc_dev->pd_wait_pr_swap_complete = false;
	mutex_unlock(&tcpc_dev->access_lock);

	disable_irq(chip->irq);
	tcpci_init(tcpc_dev, true);
	tcpci_set_cc(tcpc_dev, TYPEC_CC_OPEN);
	tcpci_set_rx_enable(tcpc_dev, PD_RX_CAP_PE_IDLE);
	tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_ERROR_RECOVERY);
	enable_irq_wake(chip->irq);
}
#endif	/* CONFIG_USB_PD_RECV_HRESET_COUNTER */

void pd_notify_pe_transit_to_default(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	pd_update_connect_state(pd_port, PD_CONNECT_HARD_RESET);

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_hard_reset_event_pending = false;
	tcpc_dev->pd_wait_pr_swap_complete = false;
	tcpc_dev->pd_bist_mode = PD_BIST_MODE_DISABLE;

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
	tcpc_dev->pd_during_direct_charge = false;
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_hard_reset_completed(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpci_notify_hard_reset_state(
		tcpc_dev, TCP_HRESET_RESULT_DONE);
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_send_hard_reset(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_transmit_state = PD_TX_STATE_WAIT_HARD_RESET;
	tcpci_notify_hard_reset_state(tcpc_dev, TCP_HRESET_SIGNAL_SEND);
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_execute_pr_swap(struct pd_port *pd_port, bool start_swap)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	pd_port->pe_data.during_swap = start_swap;
	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_wait_pr_swap_complete = true;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_cancel_pr_swap(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	if (!tcpc_dev->pd_wait_pr_swap_complete)
		return;

	pd_port->pe_data.during_swap = false;
	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_wait_pr_swap_complete = false;
	mutex_unlock(&tcpc_dev->access_lock);

	/*
	 *	CC_Alert was ignored if pd_wait_pr_swap_complete = true
	 *	So enable PDDebounce to detect CC_Again after cancel_pr_swap.
	 */

	tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_PDDEBOUNCE);

	if (!tcpci_check_vbus_valid(tcpc_dev)
		&& (pd_port->request_v >= 4000)) {
		TCPC_DBG("cancel_pr_swap_vbus=0\r\n");
		pd_put_tcp_pd_event(pd_port, TCP_DPM_EVT_ERROR_RECOVERY);
	}
}

void pd_notify_pe_reset_protocol(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_wait_pr_swap_complete = false;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_noitfy_pe_bist_mode(struct pd_port *pd_port, uint8_t mode)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_bist_mode = mode;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_transmit_msg(
	struct pd_port *pd_port, uint8_t type)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_transmit_state = type;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_pr_changed(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	/* Check mutex later, actually,
	 * typec layer will ignore all cc-change during PR-SWAP
	 */

	/* mutex_lock(&tcpc_dev->access_lock); */
	tcpc_typec_handle_pe_pr_swap(tcpc_dev);
	/* mutex_unlock(&tcpc_dev->access_lock); */
}

void pd_notify_pe_snk_explicit_contract(struct pd_port *pd_port)
{
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	struct pe_data *pe_data = &pd_port->pe_data;
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	if (pe_data->explicit_contract)
		return;

	if (tcpc_dev->typec_remote_rp_level == TYPEC_CC_VOLT_SNK_3_0)
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
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

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

	if (tcpc_dev->typec_local_rp_level == TYPEC_CC_RP_DFT)
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

	if (pull)
		tcpci_set_cc(tcpc_dev, pull);
}

#ifdef CONFIG_USB_PD_DIRECT_CHARGE
void pd_notify_pe_direct_charge(struct pd_port *pd_port, bool en)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

#ifdef CONFIG_USB_PD_REV30_PPS_SINK
	/* TODO: check it later */
	if (pd_port->request_apdo)
		en = true;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_during_direct_charge = en;
	mutex_unlock(&tcpc_dev->access_lock);
}
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

/* ---- init  ---- */
static int tcpc_event_thread(void *param)
{
	struct tcpc_device *tcpc_dev = param;
	struct sched_param sch_param = {.sched_priority = MAX_RT_PRIO - 2};

	/* set_user_nice(current, -20); */
	/* current->flags |= PF_NOFREEZE;*/

	sched_setscheduler(current, SCHED_FIFO, &sch_param);

	while (true) {
		wait_event_interruptible(tcpc_dev->event_loop_wait_que,
				atomic_read(&tcpc_dev->pending_event) |
				tcpc_dev->event_loop_thead_stop);
		if (kthread_should_stop() || tcpc_dev->event_loop_thead_stop)
			break;
		do {
			atomic_dec_if_positive(&tcpc_dev->pending_event);
		} while (pd_policy_engine_run(tcpc_dev));
	}

	return 0;
}

int tcpci_event_init(struct tcpc_device *tcpc_dev)
{
	tcpc_dev->event_task = kthread_create(tcpc_event_thread, tcpc_dev,
			"tcpc_event_%s.%p", dev_name(&tcpc_dev->dev), tcpc_dev);
	tcpc_dev->event_loop_thead_stop = false;

	init_waitqueue_head(&tcpc_dev->event_loop_wait_que);
	atomic_set(&tcpc_dev->pending_event, 0);
	wake_up_process(tcpc_dev->event_task);

	return 0;
}

int tcpci_event_deinit(struct tcpc_device *tcpc_dev)
{
	if (tcpc_dev->event_task != NULL) {
		tcpc_dev->event_loop_thead_stop = true;
		wake_up_interruptible(&tcpc_dev->event_loop_wait_que);
		kthread_stop(tcpc_dev->event_task);
	}
	return 0;
}
