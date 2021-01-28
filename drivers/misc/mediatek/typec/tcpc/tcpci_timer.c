// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/version.h>

#include <linux/sched/rt.h>
#include <uapi/linux/sched/types.h>

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

static inline void tcpc_clear_timer_tick(struct tcpc_device *tcpc, int nr);

static inline uint64_t tcpc_get_timer_enable_mask(struct tcpc_device *tcpc)
{
	uint64_t data;
	unsigned long flags;

	down(&tcpc->timer_enable_mask_lock);
	local_irq_save(flags);
	data = tcpc->timer_enable_mask;
	local_irq_restore(flags);
	up(&tcpc->timer_enable_mask_lock);

	return data;
}

static inline void tcpc_reset_timer_enable_mask(struct tcpc_device *tcpc)
{
	unsigned long flags;

	down(&tcpc->timer_enable_mask_lock);
	local_irq_save(flags);
	tcpc->timer_enable_mask = 0;
	local_irq_restore(flags);
	up(&tcpc->timer_enable_mask_lock);
}

static inline void tcpc_clear_timer_enable_mask(
	struct tcpc_device *tcpc, int nr)
{
	unsigned long flags;

	down(&tcpc->timer_enable_mask_lock);
	local_irq_save(flags);
	tcpc->timer_enable_mask &= ~RT_MASK64(nr);

	spin_lock(&tcpc->timer_tick_lock);
	tcpc->timer_tick &= ~RT_MASK64(nr);
	spin_unlock(&tcpc->timer_tick_lock);

	local_irq_restore(flags);
	up(&tcpc->timer_enable_mask_lock);
}

static inline void tcpc_set_timer_enable_mask(
	struct tcpc_device *tcpc, int nr)
{
	unsigned long flags;

	down(&tcpc->timer_enable_mask_lock);
	local_irq_save(flags);
	tcpc->timer_enable_mask |= RT_MASK64(nr);
	local_irq_restore(flags);
	up(&tcpc->timer_enable_mask_lock);
}

static inline uint64_t tcpc_get_timer_tick(struct tcpc_device *tcpc)
{
	uint64_t data;
	unsigned long flags;

	spin_lock_irqsave(&tcpc->timer_tick_lock, flags);
	data = tcpc->timer_tick;
	spin_unlock_irqrestore(&tcpc->timer_tick_lock, flags);

	return data;
}

static inline void tcpc_clear_timer_tick(struct tcpc_device *tcpc, int nr)
{
	unsigned long flags;

	spin_lock_irqsave(&tcpc->timer_tick_lock, flags);
	tcpc->timer_tick &= ~RT_MASK64(nr);
	spin_unlock_irqrestore(&tcpc->timer_tick_lock, flags);
}

static inline void tcpc_set_timer_tick(struct tcpc_device *tcpc, int nr)
{
	unsigned long flags;

	spin_lock_irqsave(&tcpc->timer_tick_lock, flags);
	tcpc->timer_tick |= RT_MASK64(nr);
	spin_unlock_irqrestore(&tcpc->timer_tick_lock, flags);
}

static const char *const tcpc_timer_name[] = {

#ifdef CONFIG_USB_POWER_DELIVERY
	"PD_TIMER_DISCOVER_ID",
	"PD_TIMER_BIST_CONT_MODE",
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
#ifdef CONFIG_USB_PD_VCONN_STABLE_DELAY
	"PD_TIMER_VCONN_STABLE",
#endif	/* CONFIG_USB_PD_VCONN_STABLE_DELAY */
	"PD_TIMER_VDM_MODE_ENTRY",
	"PD_TIMER_VDM_MODE_EXIT",
	"PD_TIMER_VDM_RESPONSE",
	"PD_TIMER_SOURCE_TRANSITION",
	"PD_TIMER_SRC_RECOVER",
#ifdef CONFIG_USB_PD_REV30
	"PD_TIMER_CK_NO_SUPPORT",
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	"PD_TIMER_SINK_TX",
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
	"PD_TIMER_SOURCE_PPS_TIMEOUT",
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */
#endif	/* CONFIG_USB_PD_REV30 */

/* PD_TIMER (out of spec ) */
	"PD_TIMER_VSAFE0V_DELAY",
	"PD_TIMER_VSAFE0V_TOUT",
	"PD_TIMER_DISCARD",
	"PD_TIMER_VBUS_STABLE",
	"PD_TIMER_VBUS_PRESENT",
	"PD_TIMER_UVDM_RESPONSE",
	"PD_TIMER_DFP_FLOW_DELAY",
	"PD_TIMER_UFP_FLOW_DELAY",
	"PD_TIMER_VCONN_READY",
	"PD_PE_VDM_POSTPONE",

#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	"PD_TIMER_DEFERRED_EVT",
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#ifdef CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP
	"PD_TIMER_SNK_FLOW_DELAY",
#endif	/* CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP */
#endif	/* CONFIG_USB_PD_REV30 */

	"PD_TIMER_PE_IDLE_TOUT",
#endif /* CONFIG_USB_POWER_DELIVERY */

/* TYPEC_RT_TIMER (out of spec) */
	"TYPEC_RT_TIMER_SAFE0V_DELAY",
	"TYPEC_RT_TIMER_SAFE0V_TOUT",
	"TYPEC_RT_TIMER_ROLE_SWAP_START",
	"TYPEC_RT_TIMER_ROLE_SWAP_STOP",
	"TYPEC_RT_TIMER_STATE_CHANGE",
	"TYPEC_RT_TIMER_NOT_LEGACY",
	"TYPEC_RT_TIMER_LEGACY_STABLE",
	"TYPEC_RT_TIMER_LEGACY_RECYCLE",
	"TYPEC_RT_TIMER_AUTO_DISCHARGE",
	"TYPEC_RT_TIMER_LOW_POWER_MODE",
#ifdef CONFIG_USB_POWER_DELIVERY
	"TYPEC_RT_TIMER_PE_IDLE",
#ifdef CONFIG_MTK_WAIT_BC12
	"TYPEC_RT_TIMER_SINK_WAIT_BC12",
#endif /* CONFIG_MTK_WAIT_BC12 */
#endif	/* CONFIG_USB_POWER_DELIVERY */
	"TYPEC_TIMER_ERROR_RECOVERY",
/* TYPEC-TRY-TIMER */
	"TYPEC_TRY_TIMER_DRP_TRY",
	"TYPEC_TRY_TIMER_DRP_TRYWAIT",
/* TYPEC-DEBOUNCE-TIMER */
	"TYPEC_TIMER_CCDEBOUNCE",
	"TYPEC_TIMER_PDDEBOUNCE",
#ifdef CONFIG_COMPATIBLE_APPLE_TA
	"TYPEC_TIMER_APPLE_CC_OPEN",
#endif /* CONFIG_COMPATIBLE_APPLE_TA */
	"TYPEC_TIMER_TRYCCDEBOUNCE",
	"TYPEC_TIMER_SRCDISCONNECT",
	"TYPEC_TIMER_DRP_SRC_TOGGLE",
#ifdef CONFIG_TYPEC_CAP_NORP_SRC
	"TYPEC_TIMER_NORP_SRC",
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
};
/* CONFIG_USB_PD_SAFE0V_DELAY */
#ifdef CONFIG_TCPC_VSAFE0V_DETECT
#define PD_TIMER_VSAFE0V_DLY_TOUT		50
#else
/* #ifndef CONFIG_TCPC_VSAFE0V_DETECT (equal timeout)*/
#define PD_TIMER_VSAFE0V_DLY_TOUT		400
#endif	/* CONFIG_TCPC_VSAFE0V_DETECT */

/* CONFIG_TYPEC_ATTACHED_SRC_SAFE0V_DELAY */
#ifdef CONFIG_TCPC_VSAFE0V_DETECT
#define TYPEC_RT_TIMER_SAFE0V_DLY_TOUT		35
#else
#define TYPEC_RT_TIMER_SAFE0V_DLY_TOUT		100
#endif

#define DECL_TCPC_TIMEOUT(enum, ms)	\
	TIMEOUT_VAL(ms)

#define DECL_TCPC_TIMEOUT_US(enum, us)	\
	TIMEOUT_VAL_US(us)

#define DECL_TCPC_TIMEOUT_RANGE(enum, min, max)	\
	TIMEOUT_RANGE(min, max)

static const uint32_t tcpc_timer_timeout[PD_TIMER_NR] = {
#ifdef CONFIG_USB_POWER_DELIVERY
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_DISCOVER_ID, 30, 60),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_BIST_CONT_MODE, 40, 50),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_HARD_RESET_COMPLETE, 4, 5),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_NO_RESPONSE, 4500, 5500),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_HARD_RESET, 25, 35),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_SOURCE_OFF, 750, 920),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_SOURCE_ON, 390, 480),

DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_PS_TRANSITION, 450, 550),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SENDER_RESPONSE, 24, 30),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SINK_ACTIVITY, 120, 150),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SINK_REQUEST, 100, 100),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SINK_WAIT_CAP, 310, 620),
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_SOURCE_ACTIVITY, 40, 50),
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
DECL_TCPC_TIMEOUT_RANGE(PD_TIMER_CK_NO_SUPPORT, 40, 50),
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
DECL_TCPC_TIMEOUT(PD_TIMER_SINK_TX, 25),	/* 16 ~ 20 */
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
DECL_TCPC_TIMEOUT(PD_TIMER_SOURCE_PPS_TIMEOUT, 14000),
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */
#endif	/* CONFIG_USB_PD_REV30 */

/* PD_TIMER (out of spec) */
DECL_TCPC_TIMEOUT(PD_TIMER_VSAFE0V_DELAY, PD_TIMER_VSAFE0V_DLY_TOUT),
DECL_TCPC_TIMEOUT(PD_TIMER_VSAFE0V_TOUT, 650),
DECL_TCPC_TIMEOUT(PD_TIMER_DISCARD, 3),

DECL_TCPC_TIMEOUT(PD_TIMER_VBUS_STABLE,
	CONFIG_USB_PD_VBUS_STABLE_TOUT),
DECL_TCPC_TIMEOUT(PD_TIMER_VBUS_PRESENT,
	CONFIG_USB_PD_VBUS_PRESENT_TOUT),
DECL_TCPC_TIMEOUT(PD_TIMER_UVDM_RESPONSE,
	CONFIG_USB_PD_CUSTOM_VDM_TOUT),
DECL_TCPC_TIMEOUT(PD_TIMER_DFP_FLOW_DELAY,
	CONFIG_USB_PD_DFP_FLOW_DLY),
DECL_TCPC_TIMEOUT(PD_TIMER_UFP_FLOW_DELAY,
	CONFIG_USB_PD_UFP_FLOW_DLY),
DECL_TCPC_TIMEOUT(PD_TIMER_VCONN_READY,
	CONFIG_USB_PD_VCONN_READY_TOUT),

DECL_TCPC_TIMEOUT_US(PD_PE_VDM_POSTPONE, 3000),

#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
DECL_TCPC_TIMEOUT(PD_TIMER_DEFERRED_EVT, 5000),
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#ifdef CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP
DECL_TCPC_TIMEOUT(PD_TIMER_SNK_FLOW_DELAY,
	CONFIG_USB_PD_UFP_FLOW_DLY),
#endif	/* CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP */
#endif	/* CONFIG_USB_PD_REV30 */

DECL_TCPC_TIMEOUT(PD_TIMER_PE_IDLE_TOUT, 10),
#endif /* CONFIG_USB_POWER_DELIVERY */

/* TYPEC_RT_TIMER (out of spec) */
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_SAFE0V_DELAY,
	TYPEC_RT_TIMER_SAFE0V_DLY_TOUT),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_SAFE0V_TOUT, 650),

DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_ROLE_SWAP_START, 20),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_ROLE_SWAP_STOP,
	CONFIG_TYPEC_CAP_ROLE_SWAP_TOUT),

DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_STATE_CHANGE, 50),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_NOT_LEGACY, 5000),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_LEGACY_STABLE, 30*1000),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_LEGACY_RECYCLE, 300*1000),

DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_AUTO_DISCHARGE,
	CONFIG_TYPEC_CAP_AUTO_DISCHARGE_TOUT),
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_LOW_POWER_MODE, 500),

#ifdef CONFIG_USB_POWER_DELIVERY
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_PE_IDLE, 1),
#ifdef CONFIG_MTK_WAIT_BC12
DECL_TCPC_TIMEOUT(TYPEC_RT_TIMER_SINK_WAIT_BC12, 50),
#endif /* CONFIG_MTK_WAIT_BC12 */
#endif	/* CONFIG_USB_POWER_DELIVERY */
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TIMER_ERROR_RECOVERY, 25, 25),

/* TYPEC-TRY-TIMER */
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TRY_TIMER_DRP_TRY, 75, 150),
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TRY_TIMER_DRP_TRYWAIT, 400, 800),

/* TYPEC-DEBOUNCE-TIMER */
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TIMER_CCDEBOUNCE, 100, 200),
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TIMER_PDDEBOUNCE, 10, 10),
#ifdef CONFIG_COMPATIBLE_APPLE_TA
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TIMER_APPLE_CC_OPEN, 200, 200),
#endif /* CONFIG_COMPATIBLE_APPLE_TA */
DECL_TCPC_TIMEOUT_RANGE(TYPEC_TIMER_TRYCCDEBOUNCE, 10, 10),
DECL_TCPC_TIMEOUT(TYPEC_TIMER_SRCDISCONNECT, 5),
DECL_TCPC_TIMEOUT(TYPEC_TIMER_DRP_SRC_TOGGLE, 60),
#ifdef CONFIG_TYPEC_CAP_NORP_SRC
DECL_TCPC_TIMEOUT(TYPEC_TIMER_NORP_SRC, 300),
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
};

typedef enum hrtimer_restart (*tcpc_hrtimer_call)(struct hrtimer *timer);

#ifdef CONFIG_USB_POWER_DELIVERY
static inline void on_pe_timer_timeout(
		struct tcpc_device *tcpc_dev, uint32_t timer_id)
{
	struct pd_event pd_event = {0};

	pd_event.event_type = PD_EVT_TIMER_MSG;
	pd_event.msg = timer_id;
	pd_event.pd_msg = NULL;

	tcpc_disable_timer(tcpc_dev, timer_id);

	switch (timer_id) {
	case PD_TIMER_VDM_MODE_ENTRY:
	case PD_TIMER_VDM_MODE_EXIT:
	case PD_TIMER_VDM_RESPONSE:
	case PD_TIMER_UVDM_RESPONSE:
		pd_put_vdm_event(tcpc_dev, &pd_event, false);
		break;

	case PD_TIMER_VSAFE0V_DELAY:
		pd_put_vbus_safe0v_event(tcpc_dev);
		break;

#ifdef CONFIG_USB_PD_SAFE0V_TIMEOUT
	case PD_TIMER_VSAFE0V_TOUT:
		TCPC_INFO("VSafe0V TOUT (%d)\r\n", tcpc_dev->vbus_level);
		if (!tcpci_check_vbus_valid_from_ic(tcpc_dev))
			pd_put_vbus_safe0v_event(tcpc_dev);
		break;
#endif	/* CONFIG_USB_PD_SAFE0V_TIMEOUT */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	case PD_TIMER_DISCARD:
		tcpc_dev->pd_discard_pending = false;
		pd_put_hw_event(tcpc_dev, PD_HW_TX_FAILED);
		break;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#if CONFIG_USB_PD_VBUS_STABLE_TOUT
	case PD_TIMER_VBUS_STABLE:
		pd_put_vbus_stable_event(tcpc_dev);
		break;
#endif	/* CONFIG_USB_PD_VBUS_STABLE_TOUT */

#if CONFIG_USB_PD_VBUS_PRESENT_TOUT
	case PD_TIMER_VBUS_PRESENT:
		pd_put_vbus_present_event(tcpc_dev);
		break;
#endif	/* CONFIG_USB_PD_VBUS_PRESENT_TOUT */

	case PD_PE_VDM_POSTPONE:
		tcpc_dev->pd_postpone_vdm_timeout = true;
		atomic_inc(&tcpc_dev->pending_event);
		wake_up_interruptible(&tcpc_dev->event_loop_wait_que);
		break;

	case PD_TIMER_PE_IDLE_TOUT:
		TCPC_INFO("pe_idle tout\n");
		pd_put_pe_event(&tcpc_dev->pd_port, PD_PE_IDLE);
		break;

	default:
		pd_put_event(tcpc_dev, &pd_event, false);
		break;
	}
}
#endif	/* CONFIG_USB_POWER_DELIVERY */

#define TCPC_TIMER_TRIGGER()	do \
{				\
	tcpc_set_timer_tick(tcpc_dev, index);	\
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

static enum hrtimer_restart
	tcpc_timer_hard_reset_complete(struct hrtimer *timer)
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

#ifdef CONFIG_USB_PD_VCONN_STABLE_DELAY
static enum hrtimer_restart tcpc_timer_vconn_stable(struct hrtimer *timer)
{
	int index = PD_TIMER_VCONN_STABLE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}
#endif	/* CONFIG_USB_PD_VCONN_STABLE_DELAY */

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

#ifdef CONFIG_USB_PD_REV30
static enum hrtimer_restart tcpc_timer_ck_no_support(struct hrtimer *timer)
{
	int index = PD_TIMER_CK_NO_SUPPORT;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
static enum hrtimer_restart tcpc_timer_sink_tx(struct hrtimer *timer)
{
	int index = PD_TIMER_SINK_TX;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */

#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
static enum hrtimer_restart tcpc_timer_source_pps(struct hrtimer *timer)
{
	int index = PD_TIMER_SOURCE_PPS_TIMEOUT;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */

#endif	/* CONFIG_USB_PD_REV30 */

/* PD_TIMER (out of spec )*/
static enum hrtimer_restart tcpc_timer_vsafe0v_delay(struct hrtimer *timer)
{
	int index = PD_TIMER_VSAFE0V_DELAY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_vsafe0v_tout(struct hrtimer *timer)
{
	int index = PD_TIMER_VSAFE0V_TOUT;
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

static enum hrtimer_restart tcpc_timer_vbus_present(struct hrtimer *timer)
{
	int index = PD_TIMER_VBUS_PRESENT;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_uvdm_response(struct hrtimer *timer)
{
	int index = PD_TIMER_UVDM_RESPONSE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_dfp_flow_delay(struct hrtimer *timer)
{
	int index = PD_TIMER_DFP_FLOW_DELAY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_ufp_flow_delay(struct hrtimer *timer)
{
	int index = PD_TIMER_UFP_FLOW_DELAY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_vconn_ready(struct hrtimer *timer)
{
	int index = PD_TIMER_VCONN_READY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_vdm_postpone(struct hrtimer *timer)
{
	int index = PD_PE_VDM_POSTPONE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
static enum hrtimer_restart tcpc_timer_deferred_evt(struct hrtimer *timer)
{
	int index = PD_TIMER_DEFERRED_EVT;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */

#ifdef CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP
static enum hrtimer_restart tcpc_timer_snk_flow_delay(struct hrtimer *timer)
{
	int index = PD_TIMER_SNK_FLOW_DELAY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}
#endif	/* CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP */
#endif	/* CONFIG_USB_PD_REV30 */

static enum hrtimer_restart tcpc_timer_pe_idle_tout(struct hrtimer *timer)
{
	int index = PD_TIMER_PE_IDLE_TOUT;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

#endif /* CONFIG_USB_POWER_DELIVERY */

/* TYPEC_RT_TIMER (out of spec ) */

static enum hrtimer_restart tcpc_timer_rt_vsafe0v_delay(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_SAFE0V_DELAY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_rt_vsafe0v_tout(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_SAFE0V_TOUT;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_rt_role_swap_start(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_ROLE_SWAP_START;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_rt_role_swap_stop(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_ROLE_SWAP_STOP;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_rt_legacy(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_STATE_CHANGE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_rt_not_legacy(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_NOT_LEGACY;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_rt_legacy_stable(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_LEGACY_STABLE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_rt_legacy_recycle(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_LEGACY_RECYCLE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_rt_auto_discharge(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_AUTO_DISCHARGE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_rt_low_power_mode(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_LOW_POWER_MODE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

#ifdef CONFIG_USB_POWER_DELIVERY
static enum hrtimer_restart tcpc_timer_rt_pe_idle(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_PE_IDLE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

#ifdef CONFIG_MTK_WAIT_BC12
static enum hrtimer_restart tcpc_timer_rt_sink_wait_bc12(struct hrtimer *timer)
{
	int index = TYPEC_RT_TIMER_SINK_WAIT_BC12;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}
#endif /* CONFIG_MTK_WAIT_BC12 */
#endif	/* CONFIG_USB_POWER_DELIVERY */

/* TYPEC-TRY-TIMER */
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

/* TYPEC-DEBOUNCE-TIMER */
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

static enum hrtimer_restart tcpc_timer_apple_cc_open(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_APPLE_CC_OPEN;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_tryccdebounce(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_TRYCCDEBOUNCE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_srcdisconnect(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_SRCDISCONNECT;
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

static void wake_up_work_func(struct work_struct *work)
{
	struct tcpc_device *tcpc_dev = container_of(
			work, struct tcpc_device, wake_up_work.work);

	mutex_lock(&tcpc_dev->typec_lock);

	TCPC_INFO("%s\n", __func__);
#ifdef CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY
	tcpc_dev->typec_wakeup_once = true;
#endif	/* CONFIG_TYPEC_WAKEUP_ONCE_LOW_DUTY */

	tcpc_typec_enter_lpm_again(tcpc_dev);

	mutex_unlock(&tcpc_dev->typec_lock);
	__pm_relax(tcpc_dev->wakeup_wake_lock);
}

static enum alarmtimer_restart
	tcpc_timer_wakeup(struct alarm *alarm, ktime_t now)
{
	struct tcpc_device *tcpc_dev =
		container_of(alarm, struct tcpc_device, wake_up_timer);

	__pm_wakeup_event(tcpc_dev->wakeup_wake_lock, 1000);
	schedule_delayed_work(&tcpc_dev->wake_up_work, 0);
	return ALARMTIMER_NORESTART;
}

static enum hrtimer_restart tcpc_timer_drp_src_toggle(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_DRP_SRC_TOGGLE;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
static enum hrtimer_restart tcpc_timer_norp_src(struct hrtimer *timer)
{
	int index = TYPEC_TIMER_NORP_SRC;
	struct tcpc_device *tcpc_dev =
		container_of(timer, struct tcpc_device, tcpc_timer[index]);

	TCPC_TIMER_TRIGGER();
	return HRTIMER_NORESTART;
}
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */

static tcpc_hrtimer_call tcpc_timer_call[PD_TIMER_NR] = {
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpc_timer_discover_id,
	tcpc_timer_bist_cont_mode,
	tcpc_timer_hard_reset_complete,
	tcpc_timer_no_response,
	tcpc_timer_ps_hard_reset,
	tcpc_timer_ps_source_off,
	tcpc_timer_ps_source_on,
	tcpc_timer_ps_transition,
	tcpc_timer_sender_response,
	tcpc_timer_sink_activity,
	tcpc_timer_sink_request,
	tcpc_timer_sink_wait_cap,
	tcpc_timer_source_activity,
	tcpc_timer_source_capability,
	tcpc_timer_source_start,
	tcpc_timer_vconn_on,
#ifdef CONFIG_USB_PD_VCONN_STABLE_DELAY
	tcpc_timer_vconn_stable,
#endif	/* CONFIG_USB_PD_VCONN_STABLE_DELAY */
	tcpc_timer_vdm_mode_entry,
	tcpc_timer_vdm_mode_exit,
	tcpc_timer_vdm_response,
	tcpc_timer_source_transition,
	tcpc_timer_src_recover,
#ifdef CONFIG_USB_PD_REV30
	tcpc_timer_ck_no_support,
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	tcpc_timer_sink_tx,
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#ifdef CONFIG_USB_PD_REV30_PPS_SOURCE
	tcpc_timer_source_pps,
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */
#endif	/* CONFIG_USB_PD_REV30 */

/* PD_TIMER (out of spec )*/
	tcpc_timer_vsafe0v_delay,
	tcpc_timer_vsafe0v_tout,
	tcpc_timer_pd_discard,
	tcpc_timer_vbus_stable,
	tcpc_timer_vbus_present,
	tcpc_timer_uvdm_response,
	tcpc_timer_dfp_flow_delay,
	tcpc_timer_ufp_flow_delay,
	tcpc_timer_vconn_ready,
	tcpc_timer_vdm_postpone,

#ifdef CONFIG_USB_PD_REV30
#ifdef CONFIG_USB_PD_REV30_COLLISION_AVOID
	tcpc_timer_deferred_evt,
#endif	/* CONFIG_USB_PD_REV30_COLLISION_AVOID */
#ifdef CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP
	tcpc_timer_snk_flow_delay,
#endif	/* CONFIG_USB_PD_REV30_SNK_FLOW_DELAY_STARTUP */
#endif	/* CONFIG_USB_PD_REV30 */

	tcpc_timer_pe_idle_tout,
#endif /* CONFIG_USB_POWER_DELIVERY */

/* TYPEC_RT_TIMER (out of spec )*/
	tcpc_timer_rt_vsafe0v_delay,
	tcpc_timer_rt_vsafe0v_tout,
	tcpc_timer_rt_role_swap_start,
	tcpc_timer_rt_role_swap_stop,
	tcpc_timer_rt_legacy,
	tcpc_timer_rt_not_legacy,
	tcpc_timer_rt_legacy_stable,
	tcpc_timer_rt_legacy_recycle,
	tcpc_timer_rt_auto_discharge,
	tcpc_timer_rt_low_power_mode,
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpc_timer_rt_pe_idle,
#ifdef CONFIG_MTK_WAIT_BC12
	tcpc_timer_rt_sink_wait_bc12,
#endif /* CONFIG_MTK_WAIT_BC12 */
#endif	/* CONFIG_USB_POWER_DELIVERY */
	tcpc_timer_error_recovery,
/* TYPEC-TRY-TIMER */
	tcpc_timer_try_drp_try,
	tcpc_timer_try_drp_trywait,
/* TYPEC-DEBOUNCE-TIMER */
	tcpc_timer_ccdebounce,
	tcpc_timer_pddebounce,
#ifdef CONFIG_COMPATIBLE_APPLE_TA
	tcpc_timer_apple_cc_open,
#endif /* CONFIG_COMPATIBLE_APPLE_TA */
	tcpc_timer_tryccdebounce,
	tcpc_timer_srcdisconnect,
	tcpc_timer_drp_src_toggle,
#ifdef CONFIG_TYPEC_CAP_NORP_SRC
	tcpc_timer_norp_src,
#endif
};

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
	uint64_t mask;

	mask = tcpc_get_timer_enable_mask(tcpc);

	for (i = start; i < end; i++) {
		if (mask & RT_MASK64(i)) {
			hrtimer_try_to_cancel(&tcpc->tcpc_timer[i]);
			tcpc_clear_timer_enable_mask(tcpc, i);
		}
	}

	if (end == PD_TIMER_NR)
		__tcpc_enable_wakeup_timer(tcpc, false);
}

void tcpc_restart_timer(struct tcpc_device *tcpc, uint32_t timer_id)
{
	uint64_t mask;

	mask = tcpc_get_timer_enable_mask(tcpc);

	if (mask & RT_MASK64(timer_id))
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

	TCPC_TIMER_EN_DBG(tcpc, timer_id);
	if (timer_id >= PD_TIMER_NR) {
		PD_BUG_ON(1);
		return;
	}
	mutex_lock(&tcpc->timer_lock);
	if (timer_id >= TYPEC_TIMER_START_ID)
		tcpc_reset_timer_range(tcpc, TYPEC_TIMER_START_ID, PD_TIMER_NR);

	tcpc_set_timer_enable_mask(tcpc, timer_id);

	tout = tcpc_timer_timeout[timer_id];

#ifdef CONFIG_USB_PD_RANDOM_FLOW_DELAY
	if (timer_id == PD_TIMER_DFP_FLOW_DELAY ||
		timer_id == PD_TIMER_UFP_FLOW_DELAY)
		tout += TIMEOUT_VAL(jiffies & 0x07);
#endif	/* CONFIG_USB_PD_RANDOM_FLOW_DELAY */

	r =  tout / 1000000;
	mod = tout % 1000000;

	mutex_unlock(&tcpc->timer_lock);
	hrtimer_start(&tcpc->tcpc_timer[timer_id],
				ktime_set(r, mod*1000), HRTIMER_MODE_REL);
}

void tcpc_disable_timer(struct tcpc_device *tcpc_dev, uint32_t timer_id)
{
	uint64_t mask;

	mask = tcpc_get_timer_enable_mask(tcpc_dev);

	if (timer_id >= PD_TIMER_NR) {
		PD_BUG_ON(1);
		return;
	}
	if (mask & RT_MASK64(timer_id)) {
		hrtimer_try_to_cancel(&tcpc_dev->tcpc_timer[timer_id]);
		tcpc_clear_timer_enable_mask(tcpc_dev, timer_id);
	}
}

void tcpc_timer_reset(struct tcpc_device *tcpc_dev)
{
	uint64_t mask;
	int i;

	mask = tcpc_get_timer_enable_mask(tcpc_dev);

	for (i = 0; i < PD_TIMER_NR; i++)
		if (mask & RT_MASK64(i))
			hrtimer_try_to_cancel(&tcpc_dev->tcpc_timer[i]);

	tcpc_reset_timer_enable_mask(tcpc_dev);
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
	uint64_t enable_mask;
	uint64_t triggered_timer;
	int i = 0;

	triggered_timer = tcpc_get_timer_tick(tcpc_dev);
	enable_mask = tcpc_get_timer_enable_mask(tcpc_dev);

#ifdef CONFIG_USB_POWER_DELIVERY
	for (i = 0; i < PD_PE_TIMER_END_ID; i++) {
		if (triggered_timer & RT_MASK64(i)) {
			TCPC_TIMER_DBG(tcpc_dev, i);
			if (enable_mask & RT_MASK64(i))
				on_pe_timer_timeout(tcpc_dev, i);
			tcpc_clear_timer_tick(tcpc_dev, i);
		}
	}
#endif /* CONFIG_USB_POWER_DELIVERY */

	mutex_lock(&tcpc_dev->typec_lock);
	for (; i < PD_TIMER_NR; i++) {
		if (triggered_timer & RT_MASK64(i)) {
			TCPC_TIMER_DBG(tcpc_dev, i);
			if (enable_mask & RT_MASK64(i))
				tcpc_typec_handle_timeout(tcpc_dev, i);
			tcpc_clear_timer_tick(tcpc_dev, i);
		}
	}
	mutex_unlock(&tcpc_dev->typec_lock);

}

static int tcpc_timer_thread(void *param)
{
	struct tcpc_device *tcpc_dev = param;

	uint64_t *timer_tick;
	struct sched_param sch_param = {.sched_priority = MAX_RT_PRIO - 1};

	timer_tick = &tcpc_dev->timer_tick;

	sched_setscheduler(current, SCHED_FIFO, &sch_param);
	while (true) {
		wait_event_interruptible(tcpc_dev->timer_wait_que,
				((*timer_tick) ? true : false) |
				tcpc_dev->timer_thread_stop);
		if (kthread_should_stop() || tcpc_dev->timer_thread_stop)
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

	tcpc_dev->timer_tick = 0;
	tcpc_dev->timer_enable_mask = 0;

	wake_up_process(tcpc_dev->timer_task);
	for (i = 0; i < PD_TIMER_NR; i++) {
		hrtimer_init(&tcpc_dev->tcpc_timer[i],
					CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		tcpc_dev->tcpc_timer[i].function = tcpc_timer_call[i];
	}
	tcpc_dev->wakeup_wake_lock =
		wakeup_source_register(NULL, "wakeup_wake_lock");
	INIT_DELAYED_WORK(&tcpc_dev->wake_up_work, wake_up_work_func);
	alarm_init(&tcpc_dev->wake_up_timer, ALARM_REALTIME, tcpc_timer_wakeup);

	pr_info("%s : init OK\n", __func__);
	return 0;
}

int tcpci_timer_deinit(struct tcpc_device *tcpc_dev)
{
	uint64_t mask;
	int i;

	mask = tcpc_get_timer_enable_mask(tcpc_dev);

	mutex_lock(&tcpc_dev->timer_lock);
	tcpc_dev->timer_thread_stop = true;
	wake_up_interruptible(&tcpc_dev->timer_wait_que);
	kthread_stop(tcpc_dev->timer_task);
	for (i = 0; i < PD_TIMER_NR; i++) {
		if (mask & RT_MASK64(i))
			hrtimer_try_to_cancel(&tcpc_dev->tcpc_timer[i]);
	}
	wakeup_source_unregister(tcpc_dev->wakeup_wake_lock);

	pr_info("%s : de init OK\n", __func__);
	mutex_unlock(&tcpc_dev->timer_lock);
	return 0;
}
