// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */

#ifndef __LINUX_SOUTHCHIP_SOFT_BC12_H__
#define __LINUX_SOUTHCHIP_SOFT_BC12_H__

#include <linux/mutex.h>
#include <linux/workqueue.h>


enum DPDM_SET_STATE{
    DPDM_HIZ_STATE = 0,
    DPDM_FLOATING_STATE,
    DPDM_PRIMARY_STATE,
    DPDM_SECONDARY_STATE,
    DPDM_HVDCP_STATE,
};

enum DPDM_STATE {
    DPDM_V0_TO_V0_325 = 0,
    DPDM_V0_325_TO_V1,
    DPDM_V1_TO_V1_35,
    DPDM_V1_35_TO_V22,
    DPDM_V2_2_TO_V3,
    DPDM_V3,
    DPDM_MAX,
};

enum BC12_RESULT {
    NONE,
    UNKNOWN_DETECED,
    SDP_DETECED,
    CDP_DETECED,
    DCP_DETECED,
    HVDCP_DETECED,
    NON_STANDARD_DETECTED,
    APPLE_3A_DETECTED   = (1 << 3) | NON_STANDARD_DETECTED,
    APPLE_2_1A_DETECTED = (2 << 3) | NON_STANDARD_DETECTED,
    SS_2A_DETECTED      = (3 << 3) | NON_STANDARD_DETECTED,
    APPLE_1A_DETECTED   = (4 << 3) | NON_STANDARD_DETECTED,
    APPLE_2_4A_DETECTED = (5 << 3) | NON_STANDARD_DETECTED,
};


struct soft_bc12;

struct soft_bc12_ops {
    int (*init)(struct soft_bc12 *);
    int (*deinit)(struct soft_bc12 *);
    int (*update_bc12_state)(struct soft_bc12 *);
    int (*set_bc12_state)(struct soft_bc12 *, enum DPDM_SET_STATE);
    int (*get_vbus_online)(struct soft_bc12 *);
};

struct soft_bc12 {
    void *private;
    uint8_t bc12_sm;
    enum DPDM_STATE dp_state;
    enum DPDM_STATE dm_state;
    enum BC12_RESULT result;
    struct soft_bc12_ops *ops;
    bool support_hvdcp;

    uint8_t flag;
    bool detect_done;
    struct mutex running_lock;
    struct mutex release_lock;
    struct mutex noti_mutex;
    struct delayed_work detect_work;
    int next_run_time;
};

struct soft_bc12 * bc12_register(void *private, struct soft_bc12_ops *ops,
                                        bool support_hvdcp);
void bc12_unregister(struct soft_bc12 *bc);
int bc12_detect_start(struct soft_bc12 *bc);
int bc12_detect_stop(struct soft_bc12 *bc);
int bc12_register_notifier(struct soft_bc12 *bc, struct notifier_block *nb);
void bc12_unregister_notifier(struct soft_bc12 *bc, struct notifier_block *nb);

#endif
