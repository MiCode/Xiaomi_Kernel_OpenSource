// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */

#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/module.h>

#include "soft_bc12.h"

#define WORK_NO_NEED_RUN        (-1)
#define BC12_INFO(fmt, args...) pr_info("[BC1.2]" fmt, ##args)

SRCU_NOTIFIER_HEAD_STATIC(bc12_notifier);

static const char * const state_str[] = {
    "BC1.2 Detect Init",
    "Non-standard adapter detection",
    "Floating Detection",
    "BC1.2 Primary Detection",
    "HIZ set",
    "BC1.2 Secondary Detection",
    "HVDCP hanke",
};

static const char * const dpdm_str[] = {
    "0v To 0.325v",
    "0.325v To 1v",
    "1v To 1.35v",
    "1.35v To 2.2v",
    "2.2v To 3v",
    "higher than 3v",
};

enum bc12_sm {
    BC12_DETECT_INIT,
    NON_STANDARD_ADAPTER_DETECTION,
    FLOAT_DETECTION,
    BC12_PRIMARY_DETECTION,
    HIZ_SET,
    BC12_SECONDARY_DETECTION,
    HVDCP_HANKE,
};

static inline int bc12_get_state(struct soft_bc12 *bc)
{
    if (!bc || !bc->ops->update_bc12_state)
        return -EINVAL;
    bc->ops->update_bc12_state(bc);

    return 0;
}

static inline int bc12_set_state(struct soft_bc12 *bc, 
                                    enum DPDM_SET_STATE state)
{
    if (!bc || !bc->ops->set_bc12_state)
        return -EINVAL;
    bc->ops->set_bc12_state(bc, state);
    return 0;
}

static inline int bc12_get_vbus_online(struct soft_bc12 *bc)
{
    if (!bc || !bc->ops->init)
        return -EINVAL;
    return bc->ops->get_vbus_online(bc);
}

static inline int bc12_init(struct soft_bc12 *bc)
{
    if (!bc || !bc->ops->init)
        return -EINVAL;
    bc->ops->init(bc);
    return 0;
}

static inline int bc12_deinit(struct soft_bc12 *bc)
{
    if (!bc || !bc->ops->deinit)
        return -EINVAL;
    bc->ops->deinit(bc);
    return 0;
}

static inline void bc12_transfer_state(struct soft_bc12 *bc, 
                                    uint8_t state, int time)
{
    bc->bc12_sm = state;
    bc->next_run_time = time;
}

static inline void bc_set_result(struct soft_bc12 *bc, 
                                    enum BC12_RESULT result)
{
    bc->result = result | (bc->flag << 3);
    bc->detect_done = true;
}

static int bc12_detect_init(struct soft_bc12 *bc)
{
    bc->detect_done = false;

    bc12_init(bc);
    bc12_transfer_state(bc, NON_STANDARD_ADAPTER_DETECTION, 45);

    return 0;
}

static int bc12_detect_deinit(struct soft_bc12 *bc)
{
    bc12_deinit(bc);
    bc12_set_state(bc,DPDM_HIZ_STATE);

    return 0;
}

static int bc12_nostand_adapter_detect_entry(struct soft_bc12 *bc)
{
    if (bc->dp_state == DPDM_V2_2_TO_V3 && bc->dm_state == DPDM_V3)
        bc->flag = 1;
    else if (bc->dp_state == DPDM_V2_2_TO_V3 && bc->dm_state == DPDM_V1_35_TO_V22)
        bc->flag = 2;
    else if (bc->dp_state == DPDM_V1_TO_V1_35 && bc->dm_state == DPDM_V1_TO_V1_35)
        bc->flag = 3;
    else if (bc->dp_state == DPDM_V1_35_TO_V22 && bc->dm_state == DPDM_V2_2_TO_V3)
        bc->flag = 4;
    else if (bc->dp_state == DPDM_V2_2_TO_V3 && bc->dm_state == DPDM_V2_2_TO_V3)
        bc->flag = 5;
    else 
        bc->flag = 0;

    bc12_set_state(bc, DPDM_FLOATING_STATE);
    bc12_transfer_state(bc, FLOAT_DETECTION, 15);
    return 0;
}

static int bc12_float_detection_entry(struct soft_bc12 *bc)
{
    if (bc->dp_state >= DPDM_V1_TO_V1_35 && bc->flag == 0)
        bc_set_result(bc, UNKNOWN_DETECED);
    else {
        bc12_set_state(bc, DPDM_PRIMARY_STATE);
        bc12_transfer_state(bc, BC12_PRIMARY_DETECTION, 100);
    }

    return 0;
}

static int bc12_primary_detect_entry(struct soft_bc12 *bc)
{
    if (bc->dm_state == DPDM_V0_TO_V0_325 && bc->flag == 0) {
        bc_set_result(bc, SDP_DETECED);
    } else if (bc->dm_state == DPDM_V0_325_TO_V1) {
        bc12_set_state(bc, DPDM_HIZ_STATE);
        bc12_transfer_state(bc, HIZ_SET, 20);
    } else {
        if (bc->flag == 0)
            bc_set_result(bc, UNKNOWN_DETECED);
        else 
            bc_set_result(bc, NON_STANDARD_DETECTED);
    }
    return 0;
}

static int bc12_hiz_set_entry(struct soft_bc12 *bc)
{
    bc12_set_state(bc, DPDM_SECONDARY_STATE);
    bc12_transfer_state(bc, BC12_SECONDARY_DETECTION, 40);
    return 0;
}

static int bc12_secondary_detect_entry(struct soft_bc12 *bc)
{
    if (bc->dp_state < DPDM_V1_35_TO_V22)
        bc_set_result(bc, CDP_DETECED);
    else if (bc->dp_state == DPDM_V1_35_TO_V22) {
        if (bc->support_hvdcp) {
            bc12_set_state(bc,DPDM_HVDCP_STATE);
            bc12_transfer_state(bc, HVDCP_HANKE, 2000);
        } else {
            bc_set_result(bc, DCP_DETECED);
        }
    } else {
        if (bc->flag == 0)
            bc_set_result(bc, UNKNOWN_DETECED);
        else 
            bc_set_result(bc, NON_STANDARD_DETECTED);
    }
    return 0;
}

static int bc12_hvdcp_hanke_entry(struct soft_bc12 *bc)
{
    if (bc->dm_state == DPDM_V0_TO_V0_325)
        bc_set_result(bc, HVDCP_DETECED);
    else
        bc_set_result(bc, DCP_DETECED);
    bc->next_run_time = WORK_NO_NEED_RUN;
    return 0;
}

static void bc12_notify_result(struct soft_bc12 *bc)
{
    mutex_lock(&bc->noti_mutex);
    srcu_notifier_call_chain(&bc12_notifier, bc->result, bc);
    mutex_unlock(&bc->noti_mutex);
}

static void bc12_relese_sm(struct soft_bc12 *bc)
{
    mutex_lock(&bc->release_lock);
    if (mutex_is_locked(&bc->running_lock)) {
        mutex_unlock(&bc->running_lock);
    }
    mutex_unlock(&bc->release_lock);
}

static void bc12_work_func(struct work_struct *work)
{
    struct soft_bc12 *bc = container_of(work, struct soft_bc12, 
            detect_work.work);

    if (bc12_get_vbus_online(bc) <= 0)
        goto out;

    bc12_get_state(bc);
    BC12_INFO("dp volt range %s; dm volt range %s\n", dpdm_str[bc->dp_state],
                                                dpdm_str[bc->dm_state]);
    BC12_INFO("state : %s\n", state_str[bc->bc12_sm]);
    
    switch (bc->bc12_sm) {
    case BC12_DETECT_INIT:
        bc12_detect_init(bc);
        break;
    case NON_STANDARD_ADAPTER_DETECTION:
        bc12_nostand_adapter_detect_entry(bc);
        break;
    case FLOAT_DETECTION:
        bc12_float_detection_entry(bc);
        break;
    case BC12_PRIMARY_DETECTION:
        bc12_primary_detect_entry(bc);
        break;
    case HIZ_SET:
        bc12_hiz_set_entry(bc);
        break;
    case BC12_SECONDARY_DETECTION:
        bc12_secondary_detect_entry(bc);
        break;
    case HVDCP_HANKE:
        bc12_hvdcp_hanke_entry(bc);
        break;
    default:
        break;
    }

    if (bc->detect_done) {
        goto out;
    }

    if (bc->next_run_time != WORK_NO_NEED_RUN) {
        schedule_delayed_work(&bc->detect_work, 
                        msecs_to_jiffies(bc->next_run_time));
    } else {
        goto out;
    }

    return;
out:
    bc12_detect_deinit(bc);
    // notify
    bc12_notify_result(bc);
    bc12_relese_sm(bc);
}

int bc12_detect_start(struct soft_bc12 *bc)
{
    if (!bc)
        return -EINVAL;
    if (bc12_get_vbus_online(bc) <= 0)
        return -EBUSY;
    if (!mutex_trylock(&bc->running_lock))
        return -EBUSY;
    bc->bc12_sm = BC12_DETECT_INIT;

    schedule_delayed_work(&bc->detect_work, 0);
    return 0;
}
EXPORT_SYMBOL(bc12_detect_start);

int bc12_detect_stop(struct soft_bc12 *bc)
{
    if (!bc)
        return -EINVAL;
    bc12_detect_deinit(bc);
    cancel_delayed_work_sync(&bc->detect_work);
    bc12_relese_sm(bc);
    return 0;
}
EXPORT_SYMBOL(bc12_detect_stop);

struct soft_bc12 * bc12_register(void *private, struct soft_bc12_ops *ops,
                                        bool support_hvdcp)
{
    struct soft_bc12 *bc;

    bc = kzalloc(sizeof(*bc), GFP_KERNEL);
    if (!bc)
        return NULL;

    bc->ops = ops;
    bc->private = private;
    bc->support_hvdcp = support_hvdcp;

    INIT_DELAYED_WORK(&bc->detect_work, bc12_work_func);
    mutex_init(&bc->noti_mutex);
    mutex_init(&bc->running_lock);
    mutex_init(&bc->release_lock);
    return bc;
}

void bc12_unregister(struct soft_bc12 *bc)
{
    if (!bc)
        return;
    mutex_destroy(&bc->release_lock);
    mutex_destroy(&bc->running_lock);
    mutex_destroy(&bc->noti_mutex);
    cancel_delayed_work_sync(&bc->detect_work);
    kfree(bc);
}
EXPORT_SYMBOL(bc12_unregister);

int bc12_register_notifier(struct soft_bc12 *bc, 
                                struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&bc12_notifier, nb);
}

void bc12_unregister_notifier(struct soft_bc12 *bc,
                                struct notifier_block *nb)
{
	srcu_notifier_chain_unregister(&bc12_notifier, nb);
}
EXPORT_SYMBOL(bc12_unregister_notifier);

MODULE_AUTHOR("Lipei Liu <lipei-liu@southchip.com>");
MODULE_DESCRIPTION("Southchip Soft BC1.2");
MODULE_LICENSE("GPL v2");
