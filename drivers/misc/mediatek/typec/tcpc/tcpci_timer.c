// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kthread.h>

#include "inc/tcpci.h"
#include "inc/tcpci_timer.h"
#include "inc/tcpci_typec.h"

#define RT_MASK64(i)			(((uint64_t)1) << i)

#define TIMEOUT_VAL(val)		(val * USEC_PER_MSEC)
#define TIMEOUT_RANGE(min, max)		((min * 4 + max) * USEC_PER_MSEC / 5)

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
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_DISCOVER_ID, 40, 50),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_BIST_CONT_MODE, 30, 60),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_HARD_RESET_COMPLETE, 4, 5),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_NO_RESPONSE, 4500, 5500),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_HARD_RESET, 25, 35),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_SOURCE_OFF, 750, 920),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_SOURCE_ON, 390, 480),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_TRANSITION, 450, 550),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SENDER_RESPONSE, 27, 30),
DECL_TCPC_TIMEOUT(PD_TIMER_SINK_REQUEST, 100),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SINK_WAIT_CAP, 310, 900),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SOURCE_CAPABILITY, 100, 200),
DECL_TCPC_TIMEOUT(PD_TIMER_SOURCE_START, 20),
DECL_TCPC_TIMEOUT(PD_TIMER_VCONN_ON, 100),
#if CONFIG_USB_PD_VCONN_STABLE_DELAY
DECL_TCPC_TIMEOUT(PD_TIMER_VCONN_STABLE, 50),
#endif	/* CONFIG_USB_PD_VCONN_STABLE_DELAY */
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_VDM_MODE_ENTRY, 40, 50),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_VDM_MODE_EXIT, 40, 50),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_VDM_RESPONSE, 24, 30),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SOURCE_TRANSITION, 25, 35),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SRC_RECOVER, 660, 1000),
#if CONFIG_USB_PD_REV30
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_CK_NOT_SUPPORTED, 40, 50),
#if CONFIG_USB_PD_REV30_COLLISION_AVOID
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SINK_TX, 16, 20),
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#if CONFIG_USB_PD_REV30_PPS_SOURCE
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SOURCE_PPS_TIMEOUT, 12000, 15000),
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */
#endif	/* CONFIG_USB_PD_REV30 */
/* tSafe0V */
DECL_TCPC_TIMEOUT(PD_TIMER_HARD_RESET_SAFE0V, 650),
/* tSrcRecover + tSrcTurnOn = 1000 + 275 */
DECL_TCPC_TIMEOUT(PD_TIMER_HARD_RESET_SAFE5V, 1275),

/* PD_TIMER (out of spec) */
#if CONFIG_USB_PD_SAFE0V_DELAY
DECL_TCPC_TIMEOUT(PD_TIMER_VSAFE0V_DELAY, 50),
#endif	/* CONFIG_USB_PD_SAFE0V_DELAY */
#if CONFIG_USB_PD_SAFE0V_TIMEOUT
DECL_TCPC_TIMEOUT(PD_TIMER_VSAFE0V_TOUT, 600),
#endif	/* CONFIG_USB_PD_SAFE0V_TIMEOUT */
#if CONFIG_USB_PD_SAFE5V_DELAY
DECL_TCPC_TIMEOUT(PD_TIMER_VSAFE5V_DELAY, 20),
#endif	/* CONFIG_USB_PD_SAFE5V_DELAY */
#if CONFIG_USB_PD_RETRY_CRC_DISCARD
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
#if CONFIG_USB_PD_REV30
#if CONFIG_USB_PD_REV30_COLLISION_AVOID
DECL_TCPC_TIMEOUT(PD_TIMER_DEFERRED_EVT, 5000),
#if CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP
DECL_TCPC_TIMEOUT(PD_TIMER_SNK_FLOW_DELAY, CONFIG_USB_PD_UFP_FLOW_DLY),
#endif	/* CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP */
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#if CONFIG_USB_PD_REV30_PPS_SINK
DECL_TCPC_TIMEOUT(PD_TIMER_PPS_REQUEST, CONFIG_USB_PD_PPS_REQUEST_INTERVAL),
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
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
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_DISCHARGE, CONFIG_TYPEC_CAP_DISCHARGE_TOUT),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_LOW_POWER_MODE, 500),
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_PE_IDLE, 1),
#if CONFIG_USB_PD_WAIT_BC12
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
#if CONFIG_TYPEC_CAP_NORP_SRC
DECL_TCPC_TIMEOUT(TYPEC_TIMER_NORP_SRC, 300),
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
#if CONFIG_COMPATIBLE_APPLE_TA
DECL_TCPC_TIMEOUT(TYPEC_TIMER_APPLE_CC_OPEN, 200),
#endif /* CONFIG_COMPATIBLE_APPLE_TA */
};

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
static inline void on_pe_timer_timeout(
		struct tcpc_device *tcpc, uint32_t timer_id)
{
	struct pd_event pd_event = {0};
	int ret = 0;
	int rv = 0;
        uint32_t chip_id = 0;
	pd_event.event_type = PD_EVT_TIMER_MSG;
	pd_event.msg = timer_id;
	pd_event.pd_msg = NULL;

	switch (timer_id) {
	case PD_TIMER_VDM_MODE_ENTRY:
	case PD_TIMER_VDM_MODE_EXIT:
	case PD_TIMER_VDM_RESPONSE:
	case PD_TIMER_UVDM_RESPONSE:
		pd_put_vdm_event(tcpc, &pd_event, false);
		break;

#if CONFIG_USB_PD_SAFE0V_DELAY
	case PD_TIMER_VSAFE0V_DELAY:
		pd_put_vbus_safe0v_event(tcpc, true);
		break;
#endif	/* CONFIG_USB_PD_SAFE0V_DELAY */

#if CONFIG_USB_PD_SAFE0V_TIMEOUT
	case PD_TIMER_VSAFE0V_TOUT:
		mutex_lock(&tcpc->typec_lock);
		TCPC_INFO("VSafe0V TOUT (%d)\n", tcpc->vbus_level);
		ret = tcpci_check_vbus_valid_from_ic(tcpc);
		mutex_unlock(&tcpc->typec_lock);
		pd_put_vbus_safe0v_event(tcpc, !ret);
		break;
#endif	/* CONFIG_USB_PD_SAFE0V_TIMEOUT */

#if CONFIG_USB_PD_SAFE5V_DELAY
	case PD_TIMER_VSAFE5V_DELAY:
		pd_put_vbus_changed_event(tcpc);
		break;
#endif	/* CONFIG_USB_PD_SAFE5V_DELAY */

#if CONFIG_USB_PD_RETRY_CRC_DISCARD
	case PD_TIMER_DISCARD:
		mutex_lock(&tcpc->typec_lock);
		tcpc->pd_discard_pending = false;
		pd_put_hw_event(tcpc, PD_HW_TX_DISCARD);
		mutex_unlock(&tcpc->typec_lock);
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
	
	case PD_TIMER_HARD_RESET_COMPLETE:
		rv = tcpci_get_chip_id(tcpc, &chip_id);
		if (!rv &&  SC2150A_DID == chip_id) {
			pd_put_sent_hard_reset_event(tcpc);
		}
		break;

	default:
		pd_put_event(tcpc, &pd_event, false);
		break;
	}
}
#endif	/* CONFIG_USB_POWER_DELIVERY */

static enum alarmtimer_restart
	tcpc_timer_call(struct alarm *alarm, ktime_t now)
{
	struct tcpc_timer *tcpc_timer =
		container_of(alarm, struct tcpc_timer, alarm);
	struct tcpc_device *tcpc = tcpc_timer->tcpc;

	tcpc_set_timer_tick(tcpc, tcpc_timer - tcpc->tcpc_timer);
	wake_up(&tcpc->timer_wait_que);

	atomic_dec_if_positive(&tcpc->suspend_pending);

	return ALARMTIMER_NORESTART;
}

/*
 * [BLOCK] Control Timer
 */

void tcpc_enable_lpm_timer(struct tcpc_device *tcpc, bool en)
{
	struct alarm *alarm =
		&tcpc->tcpc_timer[TYPEC_RT_TIMER_LOW_POWER_MODE].alarm;
	uint32_t r = 0, mod = 0, tout =
		tcpc_timer_desc[TYPEC_RT_TIMER_LOW_POWER_MODE].tout;

	TCPC_TIMER_DBG("%s en = %d\n", __func__, en);

	mutex_lock(&tcpc->timer_lock);

	if (en) {
		tout += tcpc->typec_lpm_tout;
		if (tout > 300 * USEC_PER_SEC)
			tout = 300 * USEC_PER_SEC;
		tcpc->typec_lpm_tout = tout;
		TCPC_TIMER_DBG("%s tout = %dms\n", __func__,
						   tout / USEC_PER_MSEC);
		r = tout / USEC_PER_SEC;
		mod = tout % USEC_PER_SEC;
		alarm_start_relative(alarm, ktime_set(r, mod * NSEC_PER_USEC));
	} else {
		alarm_try_to_cancel(alarm);
		tcpc->typec_lpm_tout = 0;
	}

	mutex_unlock(&tcpc->timer_lock);
}

bool tcpc_is_timer_active(struct tcpc_device *tcpc, int start, int end)
{
	int i;
	bool ret = false;

	mutex_lock(&tcpc->timer_lock);
	for (i = start; i < end; i++) {
		if (hrtimer_active(&tcpc->tcpc_timer[i].alarm.timer)) {
			ret = true;
			break;
		}
	}
	mutex_unlock(&tcpc->timer_lock);

	return ret;
}

static inline void tcpc_reset_timer_range(
		struct tcpc_device *tcpc, int start, int end)
{
	int i;

	for (i = start; i < end; i++)
		if (alarm_try_to_cancel(&tcpc->tcpc_timer[i].alarm) == 1)
			atomic_dec_if_positive(&tcpc->suspend_pending);
}

void tcpc_enable_timer(struct tcpc_device *tcpc, uint32_t timer_id)
{
	uint32_t r, mod, tout;

	if (timer_id >= PD_TIMER_NR) {
		PD_BUG_ON(1);
		return;
	}

	TCPC_TIMER_DBG("Enable %s\n", tcpc_timer_desc[timer_id].name);

	mutex_lock(&tcpc->timer_lock);
	if (timer_id >= TYPEC_TIMER_START_ID)
		tcpc_reset_timer_range(tcpc, TYPEC_TIMER_START_ID, PD_TIMER_NR);

	tout = tcpc_timer_desc[timer_id].tout;
#if PD_DYNAMIC_SENDER_RESPONSE
	if ((timer_id == PD_TIMER_SENDER_RESPONSE) &&
		(tout > tcpc->tx_time_diff) && (tcpc->tx_time_diff > 2000)) {
		tout -= (tcpc->tx_time_diff - 2000);
		tcpc->tx_time_diff = 0;
	}
#endif

#if CONFIG_USB_PD_RANDOM_FLOW_DELAY
	if (timer_id == PD_TIMER_DFP_FLOW_DELAY ||
		timer_id == PD_TIMER_UFP_FLOW_DELAY)
		tout += TIMEOUT_VAL(jiffies & 0x07);
#endif	/* CONFIG_USB_PD_RANDOM_FLOW_DELAY */

	r =  tout / USEC_PER_SEC;
	mod = tout % USEC_PER_SEC;

	if (alarm_try_to_cancel(&tcpc->tcpc_timer[timer_id].alarm) != 1)
		atomic_inc(&tcpc->suspend_pending);
	alarm_start_relative(&tcpc->tcpc_timer[timer_id].alarm,
			     ktime_set(r, mod * NSEC_PER_USEC));
	mutex_unlock(&tcpc->timer_lock);
}

void tcpc_disable_timer(struct tcpc_device *tcpc, uint32_t timer_id)
{
	if (timer_id >= PD_TIMER_NR) {
		PD_BUG_ON(1);
		return;
	}
	mutex_lock(&tcpc->timer_lock);
	if (alarm_try_to_cancel(&tcpc->tcpc_timer[timer_id].alarm) == 1)
		atomic_dec_if_positive(&tcpc->suspend_pending);
	mutex_unlock(&tcpc->timer_lock);
}

void tcpc_restart_timer(struct tcpc_device *tcpc, uint32_t timer_id)
{
	tcpc_disable_timer(tcpc, timer_id);
	tcpc_enable_timer(tcpc, timer_id);
}

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
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

	atomic_inc(&tcpc->suspend_pending);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
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

	atomic_dec_if_positive(&tcpc->suspend_pending);
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
		alarm_init(&tcpc->tcpc_timer[i].alarm,
			   ALARM_REALTIME, tcpc_timer_call);
		tcpc->tcpc_timer[i].tcpc = tcpc;
	}

	pr_info("%s : init OK\n", __func__);
	return 0;
}

int tcpci_timer_deinit(struct tcpc_device *tcpc)
{
	kthread_stop(tcpc->timer_task);
	mutex_lock(&tcpc->timer_lock);
	tcpc_reset_timer_range(tcpc, 0, PD_TIMER_NR);
	mutex_unlock(&tcpc->timer_lock);

	pr_info("%s : de init OK\n", __func__);
	return 0;
}
