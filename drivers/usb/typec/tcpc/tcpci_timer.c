/*
 * Copyright (C) 2022 Richtek Inc.
 *
 * TCPC Interface for timer handler
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

#include "inc/tcpci.h"
#include "inc/tcpci_timer.h"
#include "inc/tcpci_typec.h"

#define RT_MASK64(i)			(((uint64_t)1) << i)

#define TIMEOUT_VAL(val)		(val * 1000)
#define TIMEOUT_RANGE(min, max)		((min * 4000 + max * 1000) / 5)

static inline uint64_t tcpc_get_timer_tick(struct tcpc_device *tcpc)
{
	uint64_t tick;
	unsigned long flags;

	spin_lock_irqsave(&tcpc->timer_tick_lock, flags);
	tick = tcpc->timer_tick;
	spin_unlock_irqrestore(&tcpc->timer_tick_lock, flags);

	return tick;
}

static inline uint64_t tcpc_get_and_clear_all_timer_tick(
	struct tcpc_device *tcpc)
{
	uint64_t tick;
	unsigned long flags;

	spin_lock_irqsave(&tcpc->timer_tick_lock, flags);
	tick = tcpc->timer_tick;
	tcpc->timer_tick = 0;
	spin_unlock_irqrestore(&tcpc->timer_tick_lock, flags);

	return tick;
}

static inline void tcpc_set_timer_tick(struct tcpc_device *tcpc, int nr)
{
	unsigned long flags;

	spin_lock_irqsave(&tcpc->timer_tick_lock, flags);
	tcpc->timer_tick |= RT_MASK64(nr);
	spin_unlock_irqrestore(&tcpc->timer_tick_lock, flags);
}

struct tcpc_timer_desc {
#if TCPC_TIMER_DBG_ENABLE
	const char *const name;
#endif /* TCPC_TIMER_DBG_ENABLE */
	const uint32_t tout;
};

#if TCPC_TIMER_DBG_ENABLE
#define DECL_TCPC_TIMEOUT_RANGE(name, min, max) { #name, \
						  TIMEOUT_RANGE(min, max) }
#define DECL_TCPC_TIMEOUT(name, ms)		{ #name, TIMEOUT_VAL(ms) }
#else
#define DECL_TCPC_TIMEOUT_RANGE(name, min, max)	{ TIMEOUT_RANGE(min, max) }
#define DECL_TCPC_TIMEOUT(name, ms)		{ TIMEOUT_VAL(ms) }
#endif /* TCPC_TIMER_DBG_ENABLE */

static const struct tcpc_timer_desc tcpc_timer_desc[PD_TIMER_NR] = {
#ifdef CONFIG_USB_POWER_DELIVERY
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_DISCOVER_ID, 40, 50),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_BIST_CONT_MODE, 30, 60),
#ifdef CONFIG_TYPEC_SC2150A
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_HARD_RESET_COMPLETE, 4, 5),
#endif	/* CONFIG_TYPEC_SC2150A */
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_NO_RESPONSE, 4500, 5500),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_HARD_RESET, 25, 35),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_SOURCE_OFF, 750, 920),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_SOURCE_ON, 390, 480),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_TRANSITION, 450, 550),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SENDER_RESPONSE, 24, 30),
DECL_TCPC_TIMEOUT(PD_TIMER_SINK_REQUEST, 100),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SINK_WAIT_CAP, 310, 620),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SOURCE_CAPABILITY, 100, 200),
DECL_TCPC_TIMEOUT(PD_TIMER_SOURCE_START, 20),
DECL_TCPC_TIMEOUT(PD_TIMER_VCONN_ON, 100),
#ifdef CONFIG_USB_PD_VCONN_STABLE_DELAY
DECL_TCPC_TIMEOUT(PD_TIMER_VCONN_STABLE, 50),
#endif	/* CONFIG_USB_PD_VCONN_STABLE_DELAY */
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_VDM_MODE_ENTRY, 40, 50),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_VDM_MODE_EXIT, 40, 50),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_VDM_RESPONSE, 24, 30),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SOURCE_TRANSITION, 25, 35),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SRC_RECOVER, 660, 1000),
#ifdef CONFIG_USB_PD_REV30
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_CK_NOT_SUPPORTED, 40, 50),
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SINK_TX, 16, 20),
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SOURCE_PPS_TIMEOUT, 12000, 15000),
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */
#endif	/* CONFIG_USB_PD_REV30 */
/* tSafe0V */
DECL_TCPC_TIMEOUT(PD_TIMER_HARD_RESET_SAFE0V, 650),
/* tSrcRecover + tSrcTurnOn = 1000 + 275 */
DECL_TCPC_TIMEOUT(PD_TIMER_HARD_RESET_SAFE5V, 1275),

/* PD_TIMER (out of spec) */
#ifdef CONFIG_USB_PD_SAFE0V_DELAY
DECL_TCPC_TIMEOUT(PD_TIMER_VSAFE0V_DELAY, 50),
#endif	/* CONFIG_USB_PD_SAFE0V_DELAY */
#ifdef CONFIG_USB_PD_SAFE0V_TIMEOUT
DECL_TCPC_TIMEOUT(PD_TIMER_VSAFE0V_TOUT, 700),
#endif	/* CONFIG_USB_PD_SAFE0V_TIMEOUT */
#ifdef CONFIG_USB_PD_SAFE5V_DELAY
DECL_TCPC_TIMEOUT(PD_TIMER_VSAFE5V_DELAY, 20),
#endif	/* CONFIG_USB_PD_SAFE5V_DELAY */
#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
DECL_TCPC_TIMEOUT(PD_TIMER_DISCARD, 3),
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
#if CONFIG_USB_PD_VBUS_STABLE_TOUT
DECL_TCPC_TIMEOUT(PD_TIMER_VBUS_STABLE, CONFIG_USB_PD_VBUS_STABLE_TOUT),
#endif	/* CONFIG_USB_PD_VBUS_STABLE_TOUT */
DECL_TCPC_TIMEOUT(PD_TIMER_UVDM_RESPONSE, CONFIG_USB_PD_CUSTOM_VDM_TOUT),
DECL_TCPC_TIMEOUT(PD_TIMER_DFP_FLOW_DELAY, CONFIG_USB_PD_DFP_FLOW_DLY),
DECL_TCPC_TIMEOUT(PD_TIMER_UFP_FLOW_DELAY, CONFIG_USB_PD_UFP_FLOW_DLY),
DECL_TCPC_TIMEOUT(PD_TIMER_VCONN_READY, CONFIG_USB_PD_VCONN_READY_TOUT),
DECL_TCPC_TIMEOUT(PD_PE_VDM_POSTPONE, 3),
#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
DECL_TCPC_TIMEOUT(PD_TIMER_DEFERRED_EVT, 5000),
#ifdef CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP
DECL_TCPC_TIMEOUT(PD_TIMER_SNK_FLOW_DELAY, CONFIG_USB_PD_UFP_FLOW_DLY),
#endif	/* CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP */
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#endif	/* CONFIG_USB_PD_REV30 */
DECL_TCPC_TIMEOUT(PD_TIMER_PE_IDLE_TOUT, 10),
#endif /* CONFIG_USB_POWER_DELIVERY */

/* TYPEC_RT_TIMER (out of spec) */
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_SAFE0V_DELAY, 35),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_SAFE0V_TOUT, 650),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_ROLE_SWAP_START, 20),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_ROLE_SWAP_STOP,
	CONFIG_TYPEC_CAP_ROLE_SWAP_TOUT),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_STATE_CHANGE, 50),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_NOT_LEGACY, 5000),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_LEGACY_STABLE, 30*1000),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_LEGACY_RECYCLE, 300*1000),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_DISCHARGE, CONFIG_TYPEC_CAP_DISCHARGE_TOUT),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_LOW_POWER_MODE, 500),
#ifdef CONFIG_USB_POWER_DELIVERY
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_PE_IDLE, 1),
#ifdef CONFIG_USB_PD_WAIT_BC12
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_PD_WAIT_BC12, 50),
#endif /* CONFIG_USB_PD_WAIT_BC12 */
#endif	/* CONFIG_USB_POWER_DELIVERY */
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TIMER_ERROR_RECOVERY, 25, 25),

/* TYPEC_TRY_TIMER */
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TRY_TIMER_DRP_TRY, 75, 150),
/* TYPEC_DEBOUNCE_TIMER */
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TIMER_CCDEBOUNCE, 100, 200),
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TIMER_PDDEBOUNCE, 10, 10),
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TIMER_TRYCCDEBOUNCE, 10, 20),
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TIMER_SRCDISCONNECT, 0, 20),
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TIMER_DRP_SRC_TOGGLE, 50, 100),

/* TYPEC_TIMER (out of spec) */
#ifdef CONFIG_TYPEC_CAP_NORP_SRC
DECL_TCPC_TIMEOUT(TYPEC_TIMER_NORP_SRC, 300),
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
#ifdef CONFIG_COMPATIBLE_APPLE_TA
DECL_TCPC_TIMEOUT(TYPEC_TIMER_APPLE_CC_OPEN, 200),
#endif /* CONFIG_COMPATIBLE_APPLE_TA */
};

#ifdef CONFIG_USB_POWER_DELIVERY
static inline void on_pe_timer_timeout(
		struct tcpc_device *tcpc, uint32_t timer_id)
{
	struct pd_event pd_event = {0};
#if 1//def CONFIG_TYPEC_SC2150A
	int rv = 0;
	uint32_t chip_id = 0;
#endif /* CONFIG_TYPEC_SC2150A */

	pd_event.event_type = PD_EVT_TIMER_MSG;
	pd_event.msg = timer_id;
	pd_event.pd_msg = NULL;

	tcpc_disable_timer(tcpc, timer_id);

	switch (timer_id) {
	case PD_TIMER_VDM_MODE_ENTRY:
	case PD_TIMER_VDM_MODE_EXIT:
	case PD_TIMER_VDM_RESPONSE:
	case PD_TIMER_UVDM_RESPONSE:
		pd_put_vdm_event(tcpc, &pd_event, false);
		break;

#ifdef CONFIG_USB_PD_SAFE0V_DELAY
	case PD_TIMER_VSAFE0V_DELAY:
		pd_put_vbus_safe0v_event(tcpc);
		break;
#endif	/* CONFIG_USB_PD_SAFE0V_DELAY */

#ifdef CONFIG_USB_PD_SAFE0V_TIMEOUT
	case PD_TIMER_VSAFE0V_TOUT:
		TCPC_INFO("VSafe0V TOUT (%d)\n", tcpc->vbus_level);
		if (!tcpci_check_vbus_valid_from_ic(tcpc))
			pd_put_vbus_safe0v_event(tcpc);
		break;
#endif	/* CONFIG_USB_PD_SAFE0V_TIMEOUT */

#ifdef CONFIG_USB_PD_SAFE5V_DELAY
	case PD_TIMER_VSAFE5V_DELAY:
		pd_put_vbus_changed_event(tcpc);
		break;
#endif	/* CONFIG_USB_PD_SAFE5V_DELAY */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	case PD_TIMER_DISCARD:
		tcpc->pd_discard_pending = false;
		pd_put_hw_event(tcpc, PD_HW_TX_FAILED);
		break;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#if CONFIG_USB_PD_VBUS_STABLE_TOUT
	case PD_TIMER_VBUS_STABLE:
		pd_put_vbus_stable_event(tcpc);
		break;
#endif	/* CONFIG_USB_PD_VBUS_STABLE_TOUT */

	case PD_PE_VDM_POSTPONE:
		pd_postpone_vdm_event_timeout(tcpc);
		break;

	case PD_TIMER_PE_IDLE_TOUT:
		TCPC_INFO("pe_idle tout\n");
		pd_put_pe_event(&tcpc->pd_port, PD_PE_IDLE);
		break;

#ifdef CONFIG_TYPEC_SC2150A
	case PD_TIMER_HARD_RESET_COMPLETE:
		rv = tcpci_get_chip_id(tcpc, &chip_id);
		if (!rv &&  SC2150A_DID == chip_id) {
			pd_put_sent_hard_reset_event(tcpc);
		} else {
			pd_put_event(tcpc, &pd_event, false);
		}
		break;
#endif /* CONFIG_TYPEC_SC2150A */
	default:
		pd_put_event(tcpc, &pd_event, false);
		break;
	}
}
#endif	/* CONFIG_USB_POWER_DELIVERY */

static void wake_up_work_func(struct work_struct *work)
{
	struct tcpc_device *tcpc = container_of(
			work, struct tcpc_device, wake_up_work.work);

	mutex_lock(&tcpc->typec_lock);

	TCPC_INFO("%s\n", __func__);
#ifdef CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY
	tcpc->typec_wakeup_once = true;
#endif	/* CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY */

	tcpc_typec_enter_lpm_again(tcpc);

	mutex_unlock(&tcpc->typec_lock);
	__pm_relax(tcpc->wakeup_wake_lock);
}

static enum alarmtimer_restart
	tcpc_timer_wakeup(struct alarm *alarm, ktime_t now)
{
	struct tcpc_device *tcpc =
		container_of(alarm, struct tcpc_device, wake_up_timer);

	pm_wakeup_ws_event(tcpc->wakeup_wake_lock, 1000, true);
	schedule_delayed_work(&tcpc->wake_up_work, 0);
	return ALARMTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_call(struct hrtimer *timer)
{
	struct tcpc_timer *tcpc_timer =
		container_of(timer, struct tcpc_timer, timer);
	struct tcpc_device *tcpc = tcpc_timer->tcpc;

	tcpc_set_timer_tick(tcpc, tcpc_timer - tcpc->tcpc_timer);
	wake_up(&tcpc->timer_wait_que);

	return HRTIMER_NORESTART;
}

/*
 * [BLOCK] Control Timer
 */

static void __tcpc_enable_wakeup_timer(struct tcpc_device *tcpc, bool en)
{
	int tout = 300; /* s */

	if (en) {
		TCPC_INFO("wakeup_timer\n");

#ifdef CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY
		if (!tcpc->typec_wakeup_once) {
			if (tcpc->typec_low_rp_duty_cntdown)
				tout = 5;
			else
				tout = 20;
		}
#endif  /* CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY */

		alarm_start_relative(&tcpc->wake_up_timer, ktime_set(tout, 0));
	} else
		alarm_cancel(&tcpc->wake_up_timer);
}

static inline void tcpc_reset_timer_range(
		struct tcpc_device *tcpc, int start, int end)
{
	int i;

	for (i = start; i < end; i++)
		hrtimer_try_to_cancel(&tcpc->tcpc_timer[i].timer);

	if (end == PD_TIMER_NR)
		__tcpc_enable_wakeup_timer(tcpc, false);
}

void tcpc_restart_timer(struct tcpc_device *tcpc, uint32_t timer_id)
{
	tcpc_disable_timer(tcpc, timer_id);
	tcpc_enable_timer(tcpc, timer_id);
}

void tcpc_enable_wakeup_timer(struct tcpc_device *tcpc, bool en)
{
	mutex_lock(&tcpc->timer_lock);
	__tcpc_enable_wakeup_timer(tcpc, en);
	mutex_unlock(&tcpc->timer_lock);
}

void tcpc_enable_timer(struct tcpc_device *tcpc, uint32_t timer_id)
{
	uint32_t r, mod, tout;

	TCPC_TIMER_DBG("Enable %s\n", tcpc_timer_desc[timer_id].name);
	if (timer_id >= PD_TIMER_NR) {
		PD_BUG_ON(1);
		return;
	}
	mutex_lock(&tcpc->timer_lock);
	if (timer_id >= TYPEC_TIMER_START_ID)
		tcpc_reset_timer_range(tcpc, TYPEC_TIMER_START_ID, PD_TIMER_NR);

	tout = tcpc_timer_desc[timer_id].tout;

#ifdef CONFIG_USB_PD_RANDOM_FLOW_DELAY
	if (timer_id == PD_TIMER_DFP_FLOW_DELAY ||
		timer_id == PD_TIMER_UFP_FLOW_DELAY)
		tout += TIMEOUT_VAL(jiffies & 0x07);
#endif	/* CONFIG_USB_PD_RANDOM_FLOW_DELAY */

	r =  tout / 1000000;
	mod = tout % 1000000;

	mutex_unlock(&tcpc->timer_lock);
	hrtimer_start(&tcpc->tcpc_timer[timer_id].timer,
				ktime_set(r, mod*1000), HRTIMER_MODE_REL);
}

void tcpc_disable_timer(struct tcpc_device *tcpc, uint32_t timer_id)
{
	if (timer_id >= PD_TIMER_NR) {
		PD_BUG_ON(1);
		return;
	}
	hrtimer_try_to_cancel(&tcpc->tcpc_timer[timer_id].timer);
}

#ifdef CONFIG_USB_POWER_DELIVERY
void tcpc_reset_pe_timer(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->timer_lock);
	tcpc_reset_timer_range(tcpc, 0, PD_PE_TIMER_END_ID);
	mutex_unlock(&tcpc->timer_lock);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

void tcpc_reset_typec_debounce_timer(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->timer_lock);
	tcpc_reset_timer_range(tcpc, TYPEC_TIMER_START_ID, PD_TIMER_NR);
	mutex_unlock(&tcpc->timer_lock);
}

void tcpc_reset_typec_try_timer(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->timer_lock);
	tcpc_reset_timer_range(tcpc,
			TYPEC_TRY_TIMER_START_ID, TYPEC_TIMER_START_ID);
	mutex_unlock(&tcpc->timer_lock);
}

static void tcpc_handle_timer_triggered(struct tcpc_device *tcpc)
{
	int i = 0;
	uint64_t tick = tcpc_get_and_clear_all_timer_tick(tcpc);

#ifdef CONFIG_USB_POWER_DELIVERY
	for (i = 0; i < PD_PE_TIMER_END_ID; i++) {
		if (tick & RT_MASK64(i)) {
			TCPC_TIMER_DBG("Trigger %s\n", tcpc_timer_desc[i].name);
			on_pe_timer_timeout(tcpc, i);
		}
	}
#endif /* CONFIG_USB_POWER_DELIVERY */

	mutex_lock(&tcpc->typec_lock);
	for (; i < PD_TIMER_NR; i++) {
		if (tick & RT_MASK64(i)) {
			TCPC_TIMER_DBG("Trigger %s\n", tcpc_timer_desc[i].name);
			tcpc_typec_handle_timeout(tcpc, i);
		}
	}
	mutex_unlock(&tcpc->typec_lock);
}

static int tcpc_timer_thread_fn(void *data)
{
	struct tcpc_device *tcpc = data;
	int ret = 0;

	sched_set_fifo(current);

	while (true) {
		ret = wait_event_interruptible(tcpc->timer_wait_que,
					       tcpc_get_timer_tick(tcpc) ||
					       kthread_should_stop());
		if (kthread_should_stop() || ret) {
			dev_notice(&tcpc->dev, "%s exits(%d)\n", __func__, ret);
			break;
		}
		tcpc_handle_timer_triggered(tcpc);
	}

	return 0;
}

int tcpci_timer_init(struct tcpc_device *tcpc)
{
	int i;

	pr_info("PD Timer number = %d\n", PD_TIMER_NR);

	init_waitqueue_head(&tcpc->timer_wait_que);
	tcpc->timer_tick = 0;
	tcpc->timer_task = kthread_run(tcpc_timer_thread_fn, tcpc,
				       "tcpc_timer_%s", tcpc->desc.name);
	for (i = 0; i < PD_TIMER_NR; i++) {
		hrtimer_init(&tcpc->tcpc_timer[i].timer,
					CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		tcpc->tcpc_timer[i].timer.function = tcpc_timer_call;
		tcpc->tcpc_timer[i].tcpc = tcpc;
	}
	tcpc->wakeup_wake_lock =
		wakeup_source_register(&tcpc->dev, "tcpc_wakeup_wake_lock");
	INIT_DELAYED_WORK(&tcpc->wake_up_work, wake_up_work_func);
	alarm_init(&tcpc->wake_up_timer, ALARM_REALTIME, tcpc_timer_wakeup);

	pr_info("%s : init OK\n", __func__);
	return 0;
}

int tcpci_timer_deinit(struct tcpc_device *tcpc)
{
	kthread_stop(tcpc->timer_task);
	mutex_lock(&tcpc->timer_lock);
	tcpc_reset_timer_range(tcpc, 0, PD_TIMER_NR);
	mutex_unlock(&tcpc->timer_lock);
	cancel_delayed_work_sync(&tcpc->wake_up_work);
	wakeup_source_unregister(tcpc->wakeup_wake_lock);

	pr_info("%s : de init OK\n", __func__);
	return 0;
}
