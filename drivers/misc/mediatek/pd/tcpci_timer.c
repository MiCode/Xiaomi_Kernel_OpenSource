/*
 * drives/usb/pd/tcpci_timer.c
 * TCPC Interface for timer function
 *
 * Copyright (C) 2015 Richtek Technology Corp.
 * Author: TH <tsunghan_tasi@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/version.h>

#if 1 /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)) */
#include <linux/sched/rt.h>
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)) */

#include "inc/tcpci.h"
#include "inc/tcpci_timer.h"
#include "inc/tcpci_typec.h"

#define RT_MASK64(i)	(((uint64_t)1) << i)

#define TIMEOUT_VAL(val)	(val * 1000)
#define TIMEOUT_RANGE(min, max)		((min * 4000 + max * 1000)/5)
#define TIMEOUT_VAL_US(val)	(val)

/* Debug message Macro */
#if TCPC_TIMER_DBG_EN
#define TCPC_TIMER_DBG(tcpc, id)				\
{								\
	RT_DBG_INFO("Trigger %s\n", tcpc_timer_name[id]);	\
}
#else
#define TCPC_TIMER_DBG(format, args...)
#endif /* TCPC_TIMER_DBG_EN */

#if TCPC_TIMER_INFO_EN
#define TCPC_TIMER_EN_DBG(tcpc, id)				\
{								\
	RT_DBG_INFO("Enable %s\n", tcpc_timer_name[id]);	\
}
#else
#define TCPC_TIMER_EN_DBG(format, args...)
#endif /* TCPC_TIMER_INFO_EN */

static inline uint64_t rt_get_value(uint64_t *p)
{
	unsigned long flags;
	uint64_t data;

	raw_local_irq_save(flags);
	data = *p;
	raw_local_irq_restore(flags);
	return data;
}

static inline void rt_set_value(uint64_t *p, uint64_t data)
{
	unsigned long flags;

	raw_local_irq_save(flags);
	*p = data;
	raw_local_irq_restore(flags);
}

static inline void rt_clear_bit(int nr, uint64_t *addr)
{
	uint64_t mask = ((uint64_t)1) << nr;
	unsigned long flags;

	raw_local_irq_save(flags);
	*addr &= ~mask;
	raw_local_irq_restore(flags);
}

static inline void rt_set_bit(int nr, uint64_t *addr)
{
	uint64_t mask = ((uint64_t)1) << nr;
	unsigned long flags;

	raw_local_irq_save(flags);
	*addr |= mask;
	raw_local_irq_restore(flags);
}

const char *tcpc_timer_name[] = {
#ifdef CONFIG_USB_POWER_DELIVERY
	"PD_TIMER_BIST_CONT_MODE",
	"PD_TIMER_DISCOVER_ID",
	"PD_TIMER_HARD_RESET_COMPLETE",
	"PD_TIMER_NO_RESPONSE",
	"PD_TIMER_PS_HARD_RESET",
	"PD_TIMER_PS_SOURCE_OFF",
	"PD_TIMER_PS_SOURCE_ON",
	"PD_TIMER_PS_TRANSITION",
	"PD_TIMER_SENDER_RESPONSE",
	"PD_TIMER_SINK_ACTIVITY",
	"PD_TIMER_SINK_REQUEST",
	"PD_TIMER_SINK_WAIT_CAP",
	"PD_TIMER_SOURCE_ACTIVITY",
	"PD_TIMER_SOURCE_CAPABILITY",
	"PD_TIMER_SOURCE_START",
	"PD_TIMER_VCONN_ON",
	"PD_TIMER_VDM_MODE_ENTRY",
	"PD_TIMER_VDM_MODE_EXIT",
	"PD_TIMER_VDM_RESPONSE",
	"PD_TIMER_SOURCE_TRANSITION",
	"PD_TIMER_SRC_RECOVER",
	"PD_TIMER_VSAFE0V",
	"PD_TIMER_DISCARD",
	"PD_TIMER_VBUS_STABLE",
	"PD_PE_VDM_POSTPONE",

	"TYPEC_TRY_TIMER_DRP_TRY",
	"TYPEC_TRY_TIMER_DRP_TRYWAIT",

	"TYPEC_TIMER_CCDEBOUNCE",
	"TYPEC_TIMER_PDDEBOUNCE",
	"TYPEC_TIMER_ERROR_RECOVERY",
	"TYPEC_TIMER_SAFE0V",
	"TYPEC_TIMER_WAKEUP_TOUT",
	"TYPEC_TIMER_DRP_SRC_TOGGLE",
	"TYPEC_TIMER_PE_IDLE",
#else
	"TYPEC_TRY_TIMER_DRP_TRY",
	"TYPEC_TRY_TIMER_DRP_TRYWAIT",
	"TYPEC_TIMER_CCDEBOUNCE",
	"TYPEC_TIMER_PDDEBOUNCE",
	"TYPEC_TIMER_SAFE0V",
	"TYPEC_TIMER_WAKEUP_TOUT",
	"TYPEC_TIMER_DRP_SRC_TOGGLE",
#endif /* CONFIG_USB_POWER_DELIVERY */
};


#ifdef CONFIG_USB_PD_SAFE0V_DELAY
#define PD_TIMER_VSAFE0V_TOUT		TIMEOUT_VAL(50)
#else
#define PD_TIMER_VSAFE0V_TOUT		TIMEOUT_VAL(400)
#endif	/* CONFIG_USB_PD_SAFE0V_DELAY */

#ifdef CONFIG_TCPC_VSAFE0V_DETECT
#define TYPEC_TIMER_SAFE0V_TOUT		TIMEOUT_VAL(35)
#else
#define TYPEC_TIMER_SAFE0V_TOUT		TIMEOUT_VAL(100)
#endif

static const uint32_t tcpc_timer_timeout[PD_TIMER_NR] = {
#ifdef CONFIG_USB_POWER_DELIVERY
	TIMEOUT_RANGE(30, 60),		/* PD_TIMER_BIST_CONT_MODE */
	TIMEOUT_RANGE(40, 50),		/* PD_TIMER_DISCOVER_ID */
	TIMEOUT_RANGE(4, 5),	/* PD_TIMER_HARD_RESET_COMPLETE	(no used) */
	TIMEOUT_RANGE(4500, 5500),	/* PD_TIMER_NO_RESPONSE */
	TIMEOUT_RANGE(25, 35),		/* PD_TIMER_PS_HARD_RESET */
	TIMEOUT_RANGE(750, 920),	/* PD_TIMER_PS_SOURCE_OFF */
	TIMEOUT_RANGE(390, 480),	/* PD_TIMER_PS_SOURCE_ON, */
	TIMEOUT_RANGE(450, 550),	/* PD_TIMER_PS_TRANSITION */
	TIMEOUT_RANGE(24, 30),		/* PD_TIMER_SENDER_RESPONSE */
	TIMEOUT_RANGE(120, 150),	/* PD_TIMER_SINK_ACTIVITY (no used) */
	TIMEOUT_RANGE(100, 100),	/* PD_TIMER_SINK_REQUEST */
	TIMEOUT_RANGE(310, 620),	/* PD_TIMER_SINK_WAIT_CAP */
	TIMEOUT_RANGE(40, 50),		/* PD_TIMER_SOURCE_ACTIVITY (no used) */
	TIMEOUT_RANGE(100, 200),	/* PD_TIMER_SOURCE_CAPABILITY */
	TIMEOUT_VAL(20),		/* PD_TIMER_SOURCE_START */
	TIMEOUT_VAL(100),		/* PD_TIMER_VCONN_ON */
	TIMEOUT_RANGE(40, 50),		/* PD_TIMER_VDM_MODE_ENTRY */
	TIMEOUT_RANGE(40, 50),		/* PD_TIMER_VDM_MODE_EXIT */
	TIMEOUT_RANGE(24, 30),		/* PD_TIMER_VDM_RESPONSE */
	TIMEOUT_RANGE(25, 35),		/* PD_TIMER_SOURCE_TRANSITION */
	TIMEOUT_RANGE(660, 1000),	/* PD_TIMER_SRC_RECOVER */
	PD_TIMER_VSAFE0V_TOUT,		/* PD_TIMER_VSAFE0V (out of spec) */
	TIMEOUT_VAL(3),			/* PD_TIMER_DISCARD (out of spec) */
	/* PD_TIMER_VBUS_STABLE (out of spec) */
	TIMEOUT_VAL(CONFIG_USB_PD_VBUS_STABLE_TOUT),
	TIMEOUT_VAL_US(1500),       /* PD_PE_VDM_POSTPONE (out of spec) */

	/* TYPEC-TRY-TIMER */
	TIMEOUT_RANGE(75, 150),		/* TYPEC_TRY_TIMER_DRP_TRY */
	TIMEOUT_RANGE(400, 800),	/* TYPEC_TRY_TIMER_DRP_TRYWAIT */

	/* TYPEC-DEBOUNCE-TIMER */
	TIMEOUT_RANGE(100, 200),	/* TYPEC_TIMER_CCDEBOUNCE */
	TIMEOUT_RANGE(10, 10),		/* TYPEC_TIMER_PDDEBOUNCE */
	TIMEOUT_RANGE(25, 25),		/* TYPEC_TIMER_ERROR_RECOVERY */
	TYPEC_TIMER_SAFE0V_TOUT,	/* TYPEC_TIMER_SAFE0V (out of spec) */

	TIMEOUT_VAL(300*1000),	/* TYPEC_TIMER_WAKEUP_TOUT (out of spec) */
	TIMEOUT_VAL(60),		/* TYPEC_TIMER_DRP_SRC_TOGGLE */
	TIMEOUT_VAL(1),			/* TYPEC_TIMER_PE_IDLE (out of spec) */
#else
	TIMEOUT_RANGE(75, 150),		/* TYPEC_TRY_TIMER_DRP_TRY */
	TIMEOUT_RANGE(400, 800),	/* TYPEC_TRY_TIMER_DRP_TRYWAIT */
	TIMEOUT_RANGE(100, 200),	/* TYPEC_TIMER_CCDEBOUNCE */
	TIMEOUT_RANGE(10, 10),		/* TYPEC_TIMER_PDDEBOUNCE */
	TYPEC_TIMER_SAFE0V_TOUT,	/* TYPEC_TIMER_SAFE0V (out of spec) */

	TIMEOUT_VAL(300*1000),	/* TYPEC_TIMER_WAKEUP_TOUT (out of spec) */
	TIMEOUT_VAL(60),			/* TYPEC_TIMER_DRP_SRC_TOGGLE */
#endif /* CONFIG_USB_POWER_DELIVERY */
};

typedef enum hrtimer_restart (*tcpc_hrtimer_call)(struct hrtimer *timer);

static inline void on_pe_timer_timeout(
		struct tcpc_device *tcpc_dev, uint32_t timer_id)
{
#ifdef CONFIG_USB_POWER_DELIVERY
	pd_event_t pd_event;

	pd_event.event_type = PD_EVT_TIMER_MSG;
	pd_event.msg = timer_id;
	pd_event.pd_msg = NULL;

	switch (timer_id) {
	case PD_TIMER_VDM_MODE_ENTRY:
	case PD_TIMER_VDM_MODE_EXIT:
	case PD_TIMER_VDM_RESPONSE:
		pd_put_vdm_event(tcpc_dev, &pd_event, false);
		break;

	case PD_TIMER_VSAFE0V:
		pd_put_vbus_safe0v_event(tcpc_dev);
		break;

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	case PD_TIMER_DISCARD:
		tcpc_dev->pd_discard_pending = false;
		pd_put_hw_event(tcpc_dev, PD_HW_TX_FAILED);
		break;
#endif /* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#if CONFIG_USB_PD_VBUS_STABLE_TOUT
	case PD_TIMER_VBUS_STABLE:
		pd_put_vbus_stable_event(tcpc_dev);
		break;
#endif	/* CONFIG_USB_PD_VBUS_STABLE_TOUT */
	case PD_PE_VDM_POSTPONE:
		tcpc_dev->pd_postpone_vdm_timeout = true;
		atomic_inc(&tcpc_dev->pending_event);
		wake_up_interruptible(&tcpc_dev->event_loop_wait_que);
		break;

	default:
		pd_put_event(tcpc_dev, &pd_event, false);
		break;
	}
#endif

	tcpc_disable_timer(tcpc_dev, timer_id);
}

#define TCPC_TIMER_TRIGGER()	do \
{				\
	down(&tcpc_dev->timer_tick_lock);			\
	rt_set_bit(index, (uint64_t *)&tcpc_dev->timer_tick);	\
	up(&tcpc_dev->timer_tick_lock);				\
	wake_up_interruptible(&tcpc_dev->timer_wait_que);	\
} while (0)

#ifdef CONFIG_USB_POWER_DELIVERY
static enum hrtimer_restart tcpc_timer_bist_cont_mode(struct hrtimer *timer)
{
	int index = PD_TIMER_BIST_CONT_MODE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_discover_id(struct hrtimer *timer)
{
	int index = PD_TIMER_DISCOVER_ID;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_hard_reset_complete(
						struct hrtimer *timer)
{
	int index = PD_TIMER_HARD_RESET_COMPLETE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_no_response(struct hrtimer *timer)
{
	int index = PD_TIMER_NO_RESPONSE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_ps_hard_reset(struct hrtimer *timer)
{
	int index = PD_TIMER_PS_HARD_RESET;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_ps_source_off(struct hrtimer *timer)
{
	int index = PD_TIMER_PS_SOURCE_OFF;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_ps_source_on(struct hrtimer *timer)
{
	int index = PD_TIMER_PS_SOURCE_ON;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_ps_transition(struct hrtimer *timer)
{
	int index = PD_TIMER_PS_TRANSITION;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_sender_response(struct hrtimer *timer)
{
	int index = PD_TIMER_SENDER_RESPONSE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_sink_activity(struct hrtimer *timer)
{
	int index = PD_TIMER_SINK_ACTIVITY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_sink_request(struct hrtimer *timer)
{
	int index = PD_TIMER_SINK_REQUEST;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_sink_wait_cap(struct hrtimer *timer)
{
	int index = PD_TIMER_SINK_WAIT_CAP;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_source_activity(struct hrtimer *timer)
{
	int index = PD_TIMER_SOURCE_ACTIVITY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_source_capability(struct hrtimer *timer)
{
	int index = PD_TIMER_SOURCE_CAPABILITY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_source_start(struct hrtimer *timer)
{
	int index = PD_TIMER_SOURCE_START;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_vconn_on(struct hrtimer *timer)
{
	int index = PD_TIMER_VCONN_ON;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_vdm_mode_entry(struct hrtimer *timer)
{
	int index = PD_TIMER_VDM_MODE_ENTRY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_vdm_mode_exit(struct hrtimer *timer)
{
	int index = PD_TIMER_VDM_MODE_EXIT;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_vdm_response(struct hrtimer *timer)
{
	int index = PD_TIMER_VDM_RESPONSE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_source_transition(struct hrtimer *timer)
{
	int index = PD_TIMER_SOURCE_TRANSITION;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_src_recover(struct hrtimer *timer)
{
	int index = PD_TIMER_SRC_RECOVER;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_vsafe0v(struct hrtimer *timer)
{
	int index = PD_TIMER_VSAFE0V;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_error_recovery(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_ERROR_RECOVERY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_pe_idle(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_PE_IDLE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_pd_discard(struct hrtimer *timer)
{
	int index = PD_TIMER_DISCARD;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);
	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_vbus_stable(struct hrtimer *timer)
{
	int index = PD_TIMER_VBUS_STABLE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);
	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart pd_pe_vdm_postpone_timeout(struct hrtimer *timer)
{
	int index = PD_PE_VDM_POSTPONE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);
	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

#endif /* CONFIG_USB_POWER_DELIVERY */

static enum hrtimer_restart tcpc_timer_try_drp_try(struct hrtimer *timer)
{
	int index = TYPEC_TRY_TIMER_DRP_TRY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_try_drp_trywait(struct hrtimer *timer)
{
	int index = TYPEC_TRY_TIMER_DRP_TRYWAIT;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_ccdebounce(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_CCDEBOUNCE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_pddebounce(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_PDDEBOUNCE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_safe0v(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_SAFE0V;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_wakeup(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_WAKEUP;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_drp_src_toggle(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_DRP_SRC_TOGGLE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static tcpc_hrtimer_call tcpc_timer_call[PD_TIMER_NR] = {
#ifdef CONFIG_USB_POWER_DELIVERY
	[PD_TIMER_BIST_CONT_MODE] = tcpc_timer_bist_cont_mode,
	[PD_TIMER_DISCOVER_ID] = tcpc_timer_discover_id,
	[PD_TIMER_HARD_RESET_COMPLETE] = tcpc_timer_hard_reset_complete,
	[PD_TIMER_NO_RESPONSE] = tcpc_timer_no_response,
	[PD_TIMER_PS_HARD_RESET] = tcpc_timer_ps_hard_reset,
	[PD_TIMER_PS_SOURCE_OFF] = tcpc_timer_ps_source_off,
	[PD_TIMER_PS_SOURCE_ON] = tcpc_timer_ps_source_on,
	[PD_TIMER_PS_TRANSITION] = tcpc_timer_ps_transition,
	[PD_TIMER_SENDER_RESPONSE] = tcpc_timer_sender_response,
	[PD_TIMER_SINK_ACTIVITY] = tcpc_timer_sink_activity,
	[PD_TIMER_SINK_REQUEST] = tcpc_timer_sink_request,
	[PD_TIMER_SINK_WAIT_CAP] = tcpc_timer_sink_wait_cap,
	[PD_TIMER_SOURCE_ACTIVITY] = tcpc_timer_source_activity,
	[PD_TIMER_SOURCE_CAPABILITY] = tcpc_timer_source_capability,
	[PD_TIMER_SOURCE_START] = tcpc_timer_source_start,
	[PD_TIMER_VCONN_ON] = tcpc_timer_vconn_on,
	[PD_TIMER_VDM_MODE_ENTRY] = tcpc_timer_vdm_mode_entry,
	[PD_TIMER_VDM_MODE_EXIT] = tcpc_timer_vdm_mode_exit,
	[PD_TIMER_VDM_RESPONSE] = tcpc_timer_vdm_response,
	[PD_TIMER_SOURCE_TRANSITION] = tcpc_timer_source_transition,
	[PD_TIMER_SRC_RECOVER] = tcpc_timer_src_recover,
	[PD_TIMER_VSAFE0V] = tcpc_timer_vsafe0v,
	[PD_TIMER_DISCARD] = tcpc_timer_pd_discard,
	[PD_TIMER_VBUS_STABLE] = tcpc_timer_vbus_stable,
	[PD_PE_VDM_POSTPONE] = pd_pe_vdm_postpone_timeout,
	[TYPEC_TRY_TIMER_DRP_TRY] = tcpc_timer_try_drp_try,
	[TYPEC_TRY_TIMER_DRP_TRYWAIT] = tcpc_timer_try_drp_trywait,
	[TYPEC_TIMER_CCDEBOUNCE] = tcpc_timer_ccdebounce,
	[TYPEC_TIMER_PDDEBOUNCE] = tcpc_timer_pddebounce,
	[TYPEC_TIMER_ERROR_RECOVERY] = tcpc_timer_error_recovery,
	[TYPEC_TIMER_SAFE0V] = tcpc_timer_safe0v,
	[TYPEC_TIMER_WAKEUP] = tcpc_timer_wakeup,
	[TYPEC_TIMER_DRP_SRC_TOGGLE] = tcpc_timer_drp_src_toggle,
	[TYPEC_TIMER_PE_IDLE] = tcpc_timer_pe_idle,
#else
	[TYPEC_TRY_TIMER_DRP_TRY] = tcpc_timer_try_drp_try,
	[TYPEC_TRY_TIMER_DRP_TRYWAIT] = tcpc_timer_try_drp_trywait,
	[TYPEC_TIMER_CCDEBOUNCE] = tcpc_timer_ccdebounce,
	[TYPEC_TIMER_PDDEBOUNCE] = tcpc_timer_pddebounce,
	[TYPEC_TIMER_SAFE0V] = tcpc_timer_safe0v,
	[TYPEC_TIMER_WAKEUP] = tcpc_timer_wakeup,
	[TYPEC_TIMER_DRP_SRC_TOGGLE] = tcpc_timer_drp_src_toggle,
#endif /* CONFIG_USB_POWER_DELIVERY */
};

/*
 * [BLOCK] Control Timer
 */

static inline void tcpc_reset_timer_range(
		struct tcpc_device *tcpc, int start, int end)
{
	int i;
	uint64_t mask;

	down(&tcpc->timer_enable_mask_lock);
	mask = rt_get_value((uint64_t *)&tcpc->timer_enable_mask);
	up(&tcpc->timer_enable_mask_lock);

	for (i = start; i <= end; i++) {
		if (mask & (((uint64_t)1) << i)) {
			hrtimer_try_to_cancel(&tcpc->tcpc_timer[i]);
			down(&tcpc->timer_enable_mask_lock);
			rt_clear_bit(i, (uint64_t *)&tcpc->timer_enable_mask);
			up(&tcpc->timer_enable_mask_lock);
		}
	}
}

void tcpc_restart_timer(struct tcpc_device *tcpc, uint32_t timer_id)
{
	uint64_t mask;

	down(&tcpc->timer_enable_mask_lock);
	mask = rt_get_value((uint64_t *)&tcpc->timer_enable_mask);
	up(&tcpc->timer_enable_mask_lock);
	if (mask & (((uint64_t)1) << timer_id))
		tcpc_disable_timer(tcpc, timer_id);
	tcpc_enable_timer(tcpc, timer_id);
}

void tcpc_enable_timer(struct tcpc_device *tcpc, uint32_t timer_id)
{

	uint32_t r, mod;

	TCPC_TIMER_EN_DBG(tcpc, timer_id);
	BUG_ON(timer_id >= PD_TIMER_NR);

	mutex_lock(&tcpc->timer_lock);
	if (timer_id >= TYPEC_TIMER_START_ID)
		/* tcpc_reset_typec_debounce_timer */
		tcpc_reset_timer_range(tcpc, TYPEC_TIMER_START_ID, PD_TIMER_NR);

	down(&tcpc->timer_enable_mask_lock);
	rt_set_bit(timer_id, (uint64_t *)&tcpc->timer_enable_mask);
	up(&tcpc->timer_enable_mask_lock);
	r = tcpc_timer_timeout[timer_id] / 1000000;
	mod = tcpc_timer_timeout[timer_id] % 1000000;

	mutex_unlock(&tcpc->timer_lock);
	hrtimer_start(&tcpc->tcpc_timer[timer_id],
				ktime_set(r, mod*1000), HRTIMER_MODE_REL);
}

void tcpc_disable_timer(struct tcpc_device *tcpc_dev, uint32_t timer_id)
{
	uint64_t mask;

	down(&tcpc_dev->timer_enable_mask_lock);
	mask = rt_get_value((uint64_t *)&tcpc_dev->timer_enable_mask);
	up(&tcpc_dev->timer_enable_mask_lock);

	BUG_ON(timer_id >= PD_TIMER_NR);
	if (mask&(((uint64_t)1)<<timer_id)) {
		hrtimer_try_to_cancel(&tcpc_dev->tcpc_timer[timer_id]);
		rt_clear_bit(timer_id,
			(uint64_t *)&tcpc_dev->timer_enable_mask);
	}
}

void tcpc_timer_reset(struct tcpc_device *tcpc_dev)
{
	uint64_t mask;
	int i;

	down(&tcpc_dev->timer_enable_mask_lock);
	mask = rt_get_value((uint64_t *)&tcpc_dev->timer_enable_mask);
	up(&tcpc_dev->timer_enable_mask_lock);
	for (i = 0; i < PD_TIMER_NR; i++)
		if (mask & (((uint64_t)1) << i))
			hrtimer_try_to_cancel(&tcpc_dev->tcpc_timer[i]);
	rt_set_value((uint64_t *)&tcpc_dev->timer_enable_mask, 0);
}

#ifdef CONFIG_USB_POWER_DELIVERY
void tcpc_reset_pe_timer(struct tcpc_device *tcpc_dev)
{
	mutex_lock(&tcpc_dev->timer_lock);
	tcpc_reset_timer_range(tcpc_dev, 0, PD_PE_TIMER_END_ID);
	mutex_unlock(&tcpc_dev->timer_lock);
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

static void tcpc_handle_timer_triggered(struct tcpc_device *tcpc_dev)
{
	uint64_t triggered_timer;
	int i = 0;

	down(&tcpc_dev->timer_tick_lock);
	triggered_timer = rt_get_value((uint64_t *)&tcpc_dev->timer_tick);
	up(&tcpc_dev->timer_tick_lock);

#ifdef CONFIG_USB_POWER_DELIVERY
	for (i = 0; i < PD_PE_TIMER_END_ID; i++) {
		if (triggered_timer & RT_MASK64(i)) {
			TCPC_TIMER_DBG(tcpc_dev, i);
			on_pe_timer_timeout(tcpc_dev, i);
			down(&tcpc_dev->timer_tick_lock);
			rt_clear_bit(i, (uint64_t *)&tcpc_dev->timer_tick);
			up(&tcpc_dev->timer_tick_lock);
		}
	}
#endif /* CONFIG_USB_POWER_DELIVERY */

	mutex_lock(&tcpc_dev->typec_lock);
	for (; i < PD_TIMER_NR; i++) {
		if (triggered_timer & RT_MASK64(i)) {
			TCPC_TIMER_DBG(tcpc_dev, i);
			tcpc_typec_handle_timeout(tcpc_dev, i);
			down(&tcpc_dev->timer_tick_lock);
			rt_clear_bit(i, (uint64_t *)&tcpc_dev->timer_tick);
			up(&tcpc_dev->timer_tick_lock);
		}
	}
	mutex_unlock(&tcpc_dev->typec_lock);

}

static int tcpc_timer_thread(void *param)
{
	struct tcpc_device *tcpc_dev = param;

	uint64_t *timer_tick;
	struct sched_param sch_param = {.sched_priority = MAX_RT_PRIO - 1};


	down(&tcpc_dev->timer_tick_lock);
	timer_tick = &tcpc_dev->timer_tick;
	up(&tcpc_dev->timer_tick_lock);

	sched_setscheduler(current, SCHED_FIFO, &sch_param);
	while (true) {
		wait_event_interruptible(tcpc_dev->timer_wait_que,
				((*timer_tick) ? true : false) |
				tcpc_dev->timer_thead_stop);
		if (kthread_should_stop() || tcpc_dev->timer_thead_stop)
			break;
		do {
			tcpc_handle_timer_triggered(tcpc_dev);
		} while (*timer_tick);
	}
	return 0;
}

int tcpci_timer_init(struct tcpc_device *tcpc_dev)
{
	int i;

	pr_info("PD Timer number = %d\n", PD_TIMER_NR);
	tcpc_dev->timer_task = kthread_create(tcpc_timer_thread, tcpc_dev,
			"tcpc_timer_%s.%p", dev_name(&tcpc_dev->dev), tcpc_dev);
	init_waitqueue_head(&tcpc_dev->timer_wait_que);
	down(&tcpc_dev->timer_tick_lock);
	tcpc_dev->timer_tick = 0;
	up(&tcpc_dev->timer_tick_lock);
	rt_set_value((uint64_t *)&tcpc_dev->timer_enable_mask, 0);
	wake_up_process(tcpc_dev->timer_task);
	for (i = 0; i < PD_TIMER_NR; i++) {
		hrtimer_init(&tcpc_dev->tcpc_timer[i],
					CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		tcpc_dev->tcpc_timer[i].function = tcpc_timer_call[i];
	}

	pr_info("%s : init OK\n", __func__);
	return 0;
}

int tcpci_timer_deinit(struct tcpc_device *tcpc_dev)
{
	uint64_t mask;
	int i;

	down(&tcpc_dev->timer_enable_mask_lock);
	mask = rt_get_value((uint64_t *)&tcpc_dev->timer_enable_mask);
	up(&tcpc_dev->timer_enable_mask_lock);

	mutex_lock(&tcpc_dev->timer_lock);
	wake_up_interruptible(&tcpc_dev->timer_wait_que);
	kthread_stop(tcpc_dev->timer_task);
	for (i = 0; i < PD_TIMER_NR; i++) {
		if (mask & (1 << i))
			hrtimer_try_to_cancel(&tcpc_dev->tcpc_timer[i]);
	}

	pr_info("%s : de init OK\n", __func__);
	mutex_unlock(&tcpc_dev->timer_lock);
	return 0;
}
