/*
 * Copyright (C) 2022 Nuvolta Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/notifier.h>

#include "inc/nu6601_qc.h"

#define NU6601_QC35_DRV_VERSION	"1.0.0_NVT"


#define QC30_T_TRY_WAIT (150) //ms


SRCU_NOTIFIER_HEAD_STATIC(qc35_notifier);

static const char * const soft_qc35_states[] = {
	FOREACH_STATE(GENERATE_STRING)
};

static inline int qc35_generate_pulses(struct soft_qc35 *qc, u8 pulses)
{
    if (!qc || !qc->ops->generate_pulses)
        return -EINVAL;
    qc->ops->generate_pulses(qc, pulses);

    return 0;
}

static inline int qc35_set_qc_mode(struct soft_qc35 *qc, u8 mode)
{
    if (!qc || !qc->ops->set_qc_mode)
        return -EINVAL;
    qc->ops->set_qc_mode(qc, mode);
    return 0;
}

static inline int qc35_get_vbus_volt(struct soft_qc35 *qc)
{
    if (!qc || !qc->ops->get_vbus)
        return -EINVAL;
   return qc->ops->get_vbus(qc);
}

static void qc35_notify_result(struct soft_qc35 *qc)
{
    mutex_lock(&qc->noti_mutex);
    srcu_notifier_call_chain(&qc35_notifier, qc->qc_type, qc);
    mutex_unlock(&qc->noti_mutex);
}

static void qc35_set_state(struct soft_qc35 *qc, enum soft_qc35_state state,
			   unsigned int delay_ms)
{
	if (delay_ms) {
		pr_err("pending state change %s -> %s @ %u ms]",
			 soft_qc35_states[qc->state], soft_qc35_states[state], delay_ms);
		qc->delayed_state = state;
		schedule_delayed_work(&qc->state_machine, msecs_to_jiffies(delay_ms));
		qc->delay_ms = delay_ms;
	} else {
		pr_err("state change %s -> %s",
			 soft_qc35_states[qc->state], soft_qc35_states[state]);
		qc->delayed_state = QC_NONE;
		qc->state = state;
		if (!qc->state_machine_running)
			schedule_delayed_work(&qc->state_machine, 0);
	}
}

static void run_state_machine(struct soft_qc35 *qc)
{
	int vbus_volt;
	int retry_cnt = 10;

	switch (qc->state) {
	case QC30_TRY_WAIT:
		qc35_generate_pulses(qc, DP_16PULSE);
		break;
	case QC30_DP_16PULSES:
		vbus_volt = qc35_get_vbus_volt(qc);
		if (vbus_volt > 7000) {
			qc->qc_type = QC35_HVDCP_30;
			qc35_generate_pulses(qc, DM_16PULSE);
		} else {
			pr_err("vbus %d mV, don't support qc30, find type: QC2!\n", vbus_volt);
			qc35_notify_result(qc);
		}
		break;
	case QC30_DM_16PULSES:
		qc35_set_state(qc, QC30_DONE, 10);
		break;
	case QC30_DONE:
		if (qc->qc_type == QC35_HVDCP_30) 
			qc35_set_state(qc, QC35_V6_WAIT, 100);
		else 
			qc35_notify_result(qc);
		break;
	case QC35_V6_WAIT:
		vbus_volt = qc35_get_vbus_volt(qc);
		while ((vbus_volt < 5500) && (--retry_cnt != 0)) {
			vbus_volt = qc35_get_vbus_volt(qc);
			msleep(20);
		}

		if(retry_cnt != 0) {
			qc35_set_state(qc, QC35_V6, 10);
		} else {
			pr_err("Detect qc35 failed, fina type: QC3!\n");
			qc35_notify_result(qc);
		}

		break;
	case QC35_V6:
		qc35_generate_pulses(qc, DPDM_3PULSE);
		break;
	case QC35_DPDM_3PULSES:
		vbus_volt = qc35_get_vbus_volt(qc);
		if (vbus_volt > 6500) {
			if ((vbus_volt > 6500) && (vbus_volt < 7500)) {
				qc->qc_type = QC35_HVDCP_3_PLUS_18;
				pr_err("get qc35 type: QC35_HVDCP_3_PLUS_18\n");
			} else if ((vbus_volt > 7500) && (vbus_volt < 8500)) {
				qc->qc_type = QC35_HVDCP_3_PLUS_27;
				pr_err("get qc35 type: QC35_HVDCP_3_PLUS_27\n");
			} else if ((vbus_volt > 8500) && (vbus_volt < 9000)) {
				qc->qc_type = QC35_HVDCP_3_PLUS_40;
				pr_err("get qc35 type: QC35_HVDCP_3_PLUS_40\n");
			}
			qc35_set_state(qc, QC35_V7, 10);
		}
		break;
	case QC35_V7:
		qc35_generate_pulses(qc, DPDM_2PULSE);
		break;
	case QC35_DPDM_2PULSES:
		qc35_set_state(qc, QC35_DONE, 10);
		break;
	case QC35_DONE:
		qc35_notify_result(qc);
		break;
	default:
		WARN(1, "Unexpected qc state %d\n", qc->state);
		break;
	}
}

static void qc35_state_machine_work(struct work_struct *work)
{
	struct soft_qc35 *qc = container_of(work, struct soft_qc35, state_machine.work);

	mutex_lock(&qc->lock);
	qc->state_machine_running = true;

	if (qc->delayed_state) {
		pr_err("state change %s -> %s [delayed %ld ms]",
			 soft_qc35_states[qc->state],
			 soft_qc35_states[qc->delayed_state], qc->delay_ms);
		qc->state = qc->delayed_state;
		qc->delayed_state = QC_NONE;
	}

	run_state_machine(qc);

	qc->state_machine_running = false;
	mutex_unlock(&qc->lock);
}

int qc35_detect_start(struct soft_qc35 *qc)
{
    if (!qc) {
        return -EINVAL;
	}

	qc->qc_type = QC35_HVDCP_NONE;
	msleep(300);
	qc35_set_qc_mode(qc, QC30_5V);
	qc35_set_state(qc, QC30_TRY_WAIT, QC30_T_TRY_WAIT);

	return 0;
}

int qc35_detect_stop(struct soft_qc35 *qc)
{
    if (!qc) {
        return -EINVAL;
	}

	qc->qc_type = QC35_HVDCP_NONE;
	cancel_delayed_work_sync(&qc->state_machine);

	return 0;
}

void soft_qc35_update_dpdm_state(struct soft_qc35 *qc, 
		enum soft_qc35_dpdm_state state )
{
    switch (state) {
    case DP_16PLUSE_DONE:
		qc35_set_state(qc, QC30_DP_16PULSES, 100);
        break;
    case DM_16PLUSE_DONE:
		qc35_set_state(qc, QC30_DM_16PULSES, 10);
        break;
    case DPDM_3PLUSE_DONE:
		qc35_set_state(qc, QC35_DPDM_3PULSES, 100);
        break;
    case DP_COT_PLUSE_DONE:
        break;
    case DM_COT_PLUSE_DONE:
        break;
    case DPDM_2PLUSE_DONE:
		qc35_set_state(qc, QC35_DONE, 10);
        break;
    default:
        break;
    }
}

struct soft_qc35 *soft_qc35_register(void *private, struct soft_qc35_ops *ops)
{
	struct soft_qc35 *qc;

	pr_info("%s: (%s)\n", __func__, NU6601_QC35_DRV_VERSION);

	qc = kzalloc(sizeof(*qc), GFP_KERNEL);

	qc->qc_type = QC35_HVDCP_NONE;
    qc->ops = ops;
    qc->private = private;
	mutex_init(&qc->lock);
    mutex_init(&qc->noti_mutex);
	INIT_DELAYED_WORK(&qc->state_machine, qc35_state_machine_work);

	pr_err("%s successfully\n", __func__);
	return qc;
}

void soft_qc35_unregister(struct soft_qc35 *qc)
{
	if (qc) {
		cancel_delayed_work_sync(&qc->state_machine);
		mutex_destroy(&qc->lock);
		mutex_destroy(&qc->noti_mutex);
		kfree(qc);
		pr_err("%s successfully\n", __func__);
	}
}

int qc35_register_notifier(struct soft_qc35 *qc, 
                                struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&qc35_notifier, nb);
}

void qc35_unregister_notifier(struct soft_qc35 *qc,
                                struct notifier_block *nb)
{
	srcu_notifier_chain_unregister(&qc35_notifier, nb);
}

MODULE_AUTHOR("mick.ye@nuvoltatech.com");
MODULE_DESCRIPTION("Nuvolta NU6601 QC3.5 Driver");
MODULE_LICENSE("GPL v2");
