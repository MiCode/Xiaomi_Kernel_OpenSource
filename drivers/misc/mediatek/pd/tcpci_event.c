/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/tcpci_event.c
 * TCPC Interface for event handler
 *
 * Author: TH <tsunghan_tasi@richtek.com>
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

#if 1 /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)) */
#include <linux/sched/rt.h>
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)) */

#include "inc/tcpci_event.h"
#include "inc/tcpci_typec.h"
#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"

#ifdef CONFIG_USB_PD_POSTPONE_VDM
static void postpone_vdm_event(struct tcpc_device *tcpc_dev)
{
	/*
	 * Postpone VDM retry event due to the retry reason
	 * maybe interrupt by some PD event ....
	 */

	 pd_event_t *vdm_event = &tcpc_dev->pd_vdm_event;

	if (tcpc_dev->pd_pending_vdm_event && vdm_event->pd_msg) {
		tcpc_dev->pd_postpone_vdm_timeout = false;
		tcpc_restart_timer(tcpc_dev, PD_PE_VDM_POSTPONE);
	}
}
#endif	/* CONFIG_USB_PD_POSTPONE_VDM */

pd_msg_t *__pd_alloc_msg(struct tcpc_device *tcpc_dev)
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
	BUG_ON(true);

	return (pd_msg_t *)NULL;
}

pd_msg_t *pd_alloc_msg(struct tcpc_device *tcpc_dev)
{
	pd_msg_t *pd_msg = NULL;

	mutex_lock(&tcpc_dev->access_lock);
	pd_msg = __pd_alloc_msg(tcpc_dev);
	mutex_unlock(&tcpc_dev->access_lock);

	return pd_msg;
}

static void __pd_free_msg(struct tcpc_device *tcpc_dev, pd_msg_t *pd_msg)
{
	int index = pd_msg - tcpc_dev->pd_msg_buffer;
	uint8_t mask = 1 << index;

	BUG_ON((mask & tcpc_dev->pd_msg_buffer_allocated) == 0);
	tcpc_dev->pd_msg_buffer_allocated &= (~mask);
}

static void __pd_free_event(struct tcpc_device *tcpc_dev, pd_event_t *pd_event)
{
	if (pd_event->pd_msg) {
		__pd_free_msg(tcpc_dev, pd_event->pd_msg);
		pd_event->pd_msg = NULL;
	}
}

void pd_free_msg(struct tcpc_device *tcpc_dev, pd_msg_t *pd_msg)
{
	mutex_lock(&tcpc_dev->access_lock);
	__pd_free_msg(tcpc_dev, pd_msg);
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_free_event(struct tcpc_device *tcpc_dev, pd_event_t *pd_event)
{
	mutex_lock(&tcpc_dev->access_lock);
	__pd_free_event(tcpc_dev, pd_event);
	mutex_unlock(&tcpc_dev->access_lock);
}

/*----------------------------------------------------------------------------*/

static bool __pd_get_event(struct tcpc_device *tcpc_dev, pd_event_t *pd_event)
{
	int index = 0;

	if (tcpc_dev->pd_event_count <= 0)
		return false;

	tcpc_dev->pd_event_count--;

	*pd_event = tcpc_dev->
		pd_event_ring_buffer[tcpc_dev->pd_event_head_index];

	if (tcpc_dev->pd_event_count) {
		index = tcpc_dev->pd_event_head_index + 1;
		index %= PD_EVENT_BUF_SIZE;
	}
	tcpc_dev->pd_event_head_index = index;
	return true;
}

bool pd_get_event(struct tcpc_device *tcpc_dev, pd_event_t *pd_event)
{
	bool ret;

	mutex_lock(&tcpc_dev->access_lock);
	ret = __pd_get_event(tcpc_dev, pd_event);
	mutex_unlock(&tcpc_dev->access_lock);
	return ret;
}

static bool __pd_put_event(struct tcpc_device *tcpc_dev,
	const pd_event_t *pd_event, bool from_port_partner)
{
	int index;

#ifdef CONFIG_USB_PD_POSTPONE_OTHER_VDM
	if (from_port_partner)
		postpone_vdm_event(tcpc_dev);
#endif

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

bool pd_put_event(struct tcpc_device *tcpc_dev, const pd_event_t *pd_event,
	bool from_port_partner)
{
	bool ret;

	mutex_lock(&tcpc_dev->access_lock);
	ret = __pd_put_event(tcpc_dev, pd_event, from_port_partner);
	mutex_unlock(&tcpc_dev->access_lock);

	return ret;
}

/*----------------------------------------------------------------------------*/


bool pd_get_vdm_event(struct tcpc_device *tcpc_dev, pd_event_t *pd_event)
{
	pd_event_t *vdm_event = &tcpc_dev->pd_vdm_event;

	if (tcpc_dev->pd_pending_vdm_event) {
		if (vdm_event->pd_msg && !tcpc_dev->pd_postpone_vdm_timeout)
			return false;

		mutex_lock(&tcpc_dev->access_lock);
		*pd_event = *vdm_event;
		tcpc_dev->pd_pending_vdm_event = false;
		mutex_unlock(&tcpc_dev->access_lock);
		return true;
	}

	return false;
}

void reset_pe_vdm_state(pd_port_t *pd_port)
{
	pd_port->reset_vdm_state = true;
	pd_port->pe_vdm_state = pd_port->pe_pd_state;
}

bool pd_put_vdm_event(struct tcpc_device *tcpc_dev,
		const pd_event_t *pd_event, bool from_port_partner)
{
	pd_msg_t *pd_msg = pd_event->pd_msg;

	mutex_lock(&tcpc_dev->access_lock);

	if (tcpc_dev->pd_pending_vdm_event) {
		/* If message from port partner, we have to overwrite it */

		if (from_port_partner)
			__pd_free_event(tcpc_dev, &tcpc_dev->pd_vdm_event);
		else {
			mutex_unlock(&tcpc_dev->access_lock);
			return false;
		}
	}

	tcpc_dev->pd_vdm_event = *pd_event;
	tcpc_dev->pd_pending_vdm_event = true;
	tcpc_dev->pd_postpone_vdm_timeout = true;

	if (from_port_partner) {

		BUG_ON(pd_msg == NULL);
		/* pd_msg->time_stamp = 0; */
		tcpc_dev->pd_last_vdm_msg = *pd_msg;

		if (PD_VDO_SVDM(pd_msg->payload[0]) &&
				PD_VDO_CMDT(pd_msg->payload[0]) == CMDT_INIT)
			reset_pe_vdm_state(&tcpc_dev->pd_port);

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
	pd_msg_t *pd_msg = &tcpc_dev->pd_last_vdm_msg;
	pd_event_t *vdm_event = &tcpc_dev->pd_vdm_event;

	mutex_lock(&tcpc_dev->access_lock);

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
	if (PD_VDO_SVDM(pd_msg->payload[0]) &&
		PD_VDO_CMDT(pd_msg->payload[0]) == CMDT_INIT)
		reset_pe_vdm_state(&tcpc_dev->pd_port);
	postpone_vdm_event(tcpc_dev);
#else
	atomic_inc(&tcpc_dev->pending_event); /* do not really wake up process*/
	wake_up_interruptible(&tcpc_dev->event_loop_wait_que);
#endif	/* CONFIG_USB_PD_POSTPONE_RETRY_VDM */

	mutex_unlock(&tcpc_dev->access_lock);
	return true;
}

/*----------------------------------------------------------------------------*/

static void __pd_event_buf_reset(struct tcpc_device *tcpc_dev)
{
	pd_event_t pd_event;

	tcpc_dev->pd_hard_reset_event_pending = false;
	while (__pd_get_event(tcpc_dev, &pd_event))
		__pd_free_event(tcpc_dev, &pd_event);

	if (tcpc_dev->pd_pending_vdm_event) {
		__pd_free_event(tcpc_dev, &tcpc_dev->pd_vdm_event);
		tcpc_dev->pd_pending_vdm_event = false;
	}
}


void pd_event_buf_reset(struct tcpc_device *tcpc_dev)
{
	mutex_lock(&tcpc_dev->access_lock);
	__pd_event_buf_reset(tcpc_dev);
	mutex_unlock(&tcpc_dev->access_lock);
}

/*----------------------------------------------------------------------------*/

static inline bool __pd_put_hw_event(
	struct tcpc_device *tcpc_dev, uint8_t hw_event)
{
	pd_event_t evt = {
		.event_type = PD_EVT_HW_MSG,
		.msg = hw_event,
		.pd_msg = NULL,
	};

	return __pd_put_event(tcpc_dev, &evt, false);
}

static inline bool __pd_put_pe_event(
	struct tcpc_device *tcpc_dev, uint8_t pe_event)
{
	pd_event_t evt = {
		.event_type = PD_EVT_PE_MSG,
		.msg = pe_event,
		.pd_msg = NULL,
	};

	return __pd_put_event(tcpc_dev, &evt, false);
}


void pd_put_idle_event(struct tcpc_device *tcpc_dev)
{
	mutex_lock(&tcpc_dev->access_lock);

	__pd_event_buf_reset(tcpc_dev);
	__pd_put_hw_event(tcpc_dev, PD_HW_CC_DETACHED);

	tcpc_dev->pd_wait_pe_idle = true;
	tcpc_dev->pd_wait_pr_swap_complete = false;
	tcpc_dev->pd_wait_hard_reset_complete = false;
	tcpc_dev->pd_hard_reset_event_pending = false;
	tcpc_dev->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
	tcpc_dev->pd_bist_mode = PD_BIST_MODE_DISABLE;
	tcpc_dev->pd_ping_event_pending = false;
	tcpc_dev->pd_idle_mode = false;

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	tcpc_dev->pd_discard_pending = false;
#endif

	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_put_recv_hard_reset_event(struct tcpc_device *tcpc_dev)
{
	mutex_lock(&tcpc_dev->access_lock);

	tcpc_dev->pd_transmit_state = PD_TX_STATE_HARD_RESET;

	if ((!tcpc_dev->pd_hard_reset_event_pending) &&
		(!tcpc_dev->pd_wait_pe_idle)) {
		__pd_event_buf_reset(tcpc_dev);
		__pd_put_hw_event(tcpc_dev, PD_HW_RECV_HARD_RESET);
		tcpc_dev->pd_bist_mode = PD_BIST_MODE_DISABLE;
		tcpc_dev->pd_hard_reset_event_pending = true;
		tcpc_dev->pd_ping_event_pending = false;
	}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	tcpc_dev->pd_discard_pending = false;
#endif

	mutex_unlock(&tcpc_dev->access_lock);
}

bool pd_put_pd_msg_event(struct tcpc_device *tcpc_dev, pd_msg_t *pd_msg)
{
	uint32_t cnt, cmd;

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	bool discard_pending = false;
#endif

	pd_event_t evt = {
		.event_type = PD_EVT_PD_MSG,
		.pd_msg = pd_msg,
	};

	cnt = PD_HEADER_CNT(pd_msg->msg_hdr);
	cmd = PD_HEADER_TYPE(pd_msg->msg_hdr);

	/* bist mode */
	mutex_lock(&tcpc_dev->access_lock);
	if (tcpc_dev->pd_bist_mode != PD_BIST_MODE_DISABLE) {
		TCPC_DBG("BIST_MODE_RX\r\n");
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
			TCPC_DBG("RETRANSMIT\r\n");
			__pd_free_event(tcpc_dev, &evt);
			mutex_unlock(&tcpc_dev->access_lock);

			/* TODO: check it later */
			tcpc_disable_timer(tcpc_dev, PD_TIMER_DISCARD);
			tcpci_retransmit(tcpc_dev);
			return 0;
		}
	}
#endif

#ifdef CONFIG_USB_PD_DROP_REPEAT_PING
	if (cnt == 0 && cmd == PD_CTRL_PING) {
		if (tcpc_dev->pd_ping_event_pending) {
			TCPC_DBG("PING\r\n");
			__pd_free_event(tcpc_dev, &evt);
			mutex_unlock(&tcpc_dev->access_lock);
			return 0;
		}

		tcpc_dev->pd_ping_event_pending = true;
	}
#endif

	if (cnt != 0 && cmd == PD_DATA_BIST)
		tcpc_dev->pd_bist_mode = PD_BIST_MODE_EVENT_PENDING;

	mutex_unlock(&tcpc_dev->access_lock);

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	if (discard_pending) {
		tcpc_disable_timer(tcpc_dev, PD_TIMER_DISCARD);
		pd_put_hw_event(tcpc_dev, PD_HW_TX_FAILED);
	}
#endif

	if (cnt != 0 && cmd == PD_DATA_VENDOR_DEF)
		return pd_put_vdm_event(tcpc_dev, &evt, true);
	return pd_put_event(tcpc_dev, &evt, true);
}

void pd_put_vbus_changed_event(struct tcpc_device *tcpc_dev)
{
	int vbus_valid;

	mutex_lock(&tcpc_dev->access_lock);
	vbus_valid = tcpci_check_vbus_valid(tcpc_dev);

	switch (tcpc_dev->pd_wait_vbus_once) {
	case PD_WAIT_VBUS_VALID_ONCE:
		if (vbus_valid) {
			tcpc_dev->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
			__pd_put_hw_event(tcpc_dev, PD_HW_VBUS_PRESENT);
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
}

void pd_put_vbus_safe0v_event(struct tcpc_device *tcpc_dev)
{
	mutex_lock(&tcpc_dev->access_lock);
	if (tcpc_dev->pd_wait_vbus_once == PD_WAIT_VBUS_SAFE0V_ONCE) {
		tcpc_dev->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
		__pd_put_hw_event(tcpc_dev, PD_HW_VBUS_SAFE0V);
	}
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_put_vbus_stable_event(struct tcpc_device *tcpc_dev)
{
	mutex_lock(&tcpc_dev->access_lock);
	if (tcpc_dev->pd_wait_vbus_once == PD_WAIT_VBUS_STABLE_ONCE) {
		tcpc_dev->pd_wait_vbus_once = PD_WAIT_VBUS_DISABLE;
		__pd_put_hw_event(tcpc_dev, PD_HW_VBUS_STABLE);
	}
	mutex_unlock(&tcpc_dev->access_lock);
}

/* ---- PD Notify TCPC ---- */

void pd_notify_pe_idle(pd_port_t *pd_port)
{
	bool notify_pe_idle = false;
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	if (tcpc_dev->pd_wait_pe_idle) {
		notify_pe_idle = true;
		tcpc_dev->pd_wait_pe_idle = false;
	}

	tcpc_dev->pd_wait_error_recovery = false;
	mutex_unlock(&tcpc_dev->access_lock);

	pd_update_connect_state(pd_port, PD_CONNECT_NONE);

	if (notify_pe_idle)
		tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_PE_IDLE);
}

void pd_notify_pe_wait_vbus_once(pd_port_t *pd_port, int wait_evt)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_wait_vbus_once = wait_evt;
	mutex_unlock(&tcpc_dev->access_lock);

	switch (wait_evt) {
	case PD_WAIT_VBUS_VALID_ONCE:
	case PD_WAIT_VBUS_INVALID_ONCE:
		pd_put_vbus_changed_event(tcpc_dev);
		break;
	case PD_WAIT_VBUS_SAFE0V_ONCE:
#ifdef CONFIG_TCPC_VSAFE0V_DETECT
		if (tcpci_check_vsafe0v(tcpc_dev, true))
			pd_put_vbus_safe0v_event(tcpc_dev);
#else
		pd_enable_timer(pd_port, PD_TIMER_VSAFE0V);
#endif
		break;
	}
}

void pd_notify_pe_error_recovery(pd_port_t *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_wait_hard_reset_complete = false;
	tcpc_dev->pd_wait_pr_swap_complete = false;
	tcpc_dev->pd_wait_error_recovery = true;
	mutex_unlock(&tcpc_dev->access_lock);

	tcpci_set_cc(pd_port->tcpc_dev, TYPEC_CC_OPEN);
	tcpc_enable_timer(tcpc_dev, TYPEC_TIMER_ERROR_RECOVERY);
}

void pd_notify_pe_transit_to_default(pd_port_t *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_hard_reset_event_pending = false;
	tcpc_dev->pd_wait_hard_reset_complete = true;
	tcpc_dev->pd_wait_pr_swap_complete = false;
	tcpc_dev->pd_bist_mode = PD_BIST_MODE_DISABLE;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_hard_reset_completed(pd_port_t *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	if (!tcpc_dev->pd_wait_hard_reset_complete)
		return;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_wait_hard_reset_complete = false;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_send_hard_reset_start(pd_port_t *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_transmit_state = PD_TX_STATE_WAIT_HARD_RESET;
	tcpc_dev->pd_wait_hard_reset_complete = true;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_send_hard_reset_done(pd_port_t *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	if (tcpc_dev->pd_wait_hard_reset_complete) {
		__pd_event_buf_reset(tcpc_dev);
		__pd_put_pe_event(tcpc_dev, PD_PE_HARD_RESET_COMPLETED);
	} else
		TCPC_DBG("[HReset] Unattached\r\n");
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_execute_pr_swap(pd_port_t *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	pd_port->during_swap = true;
	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_wait_pr_swap_complete = true;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_reset_protocol(pd_port_t *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_wait_pr_swap_complete = false;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_noitfy_pe_bist_mode(pd_port_t *pd_port, uint8_t mode)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_bist_mode = mode;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_recv_ping_event(pd_port_t *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_ping_event_pending = false;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_transmit_msg(
	pd_port_t *pd_port, uint8_t type)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	mutex_lock(&tcpc_dev->access_lock);
	tcpc_dev->pd_transmit_state = type;
	mutex_unlock(&tcpc_dev->access_lock);
}

void pd_notify_pe_pr_changed(pd_port_t *pd_port)
{
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	/* Check mutex later, actually,
	   typec layer will igrone all cc-change during PR-SWAP */

	/* mutex_lock(&tcpc_dev->access_lock); */
	tcpc_typec_handle_pe_pr_swap(tcpc_dev);
	/* mutex_unlock(&tcpc_dev->access_lock); */
}

uint8_t pd_wait_tx_finished_event(pd_port_t *pd_port)
{
	uint8_t tx_status;
	struct tcpc_device *tcpc_dev = pd_port->tcpc_dev;

	int i = 0;

	/* Max : 8ms (HReset Max: 5ms)*/
	do {
		usleep_range(450, 500);

		mutex_lock(&tcpc_dev->access_lock);
		tx_status = tcpc_dev->pd_transmit_state;
		mutex_unlock(&tcpc_dev->access_lock);

#ifdef CONFIG_USB_PD_IGNORE_HRESET_COMPLETE_TIMER
		if (tx_status == PD_TX_STATE_WAIT_HARD_RESET &&
			!(tcpc_dev->tcpc_flags &
				TCPC_FLAGS_WAIT_HRESET_COMPLETE)) {
			TCPC_DBG("Ignore HTimer\r\n");
			tx_status = PD_TX_STATE_GOOD_CRC;
			break;
		}
#endif

		if (++i >= 16) {
			TCPC_INFO("--> tx_timeout\r\n");
			tx_status = PD_TX_STATE_NO_RESPONSE;
		}
	} while (tx_status >= PD_TX_STATE_WAIT);

	return tx_status;
}

/* ---- init  ---- */
static int tcpc_event_thread(void *param)
{
	struct tcpc_device *tcpc_dev = param;
	struct sched_param sch_param = {.sched_priority = MAX_RT_PRIO - 2};

#if 0
	set_user_nice(current, -20);
	current->flags |= PF_NOFREEZE;
#endif
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
	tcpc_dev->event_loop_thead_stop = true;
	wake_up_interruptible(&tcpc_dev->event_loop_wait_que);
	kthread_stop(tcpc_dev->event_task);
	return 0;
}
