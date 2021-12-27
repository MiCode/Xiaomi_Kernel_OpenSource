/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_REQ_MGR_TIMER_H_
#define _CAM_REQ_MGR_TIMER_H_

#include <linux/slab.h>
#include <linux/timer.h>

#include "cam_req_mgr_core_defs.h"

/** struct cam_req_mgr_timer
 * @expires      : timeout value for timer
 * @sys_timer    : system timer variable
 * @parent       : priv data - link pointer
 * @timer_cb     : callback func which will be called when timeout expires
 * @pause_timer  : flag to pause SOF timer
 */
struct cam_req_mgr_timer {
	int32_t            expires;
	struct timer_list  sys_timer;
	void               *parent;
	void               (*timer_cb)(struct timer_list *timer_data);
	bool                pause_timer;
};

/**
 * crm_timer_modify()
 * @brief : allows ser to modify expiry time.
 * @timer : timer which will be reset to expires values
 */
void crm_timer_modify(struct cam_req_mgr_timer *crm_timer,
	int32_t expires);

/**
 * crm_timer_reset()
 * @brief : destroys the timer allocated.
 * @timer : timer which will be reset to expires values
 */
void crm_timer_reset(struct cam_req_mgr_timer *timer);

/**
 * crm_timer_init()
 * @brief    : create a new general purpose timer.
 *             timer utility takes care of allocating memory and deleting
 * @timer    : double pointer to new timer allocated
 * @expires  : Timeout value to fire callback
 * @parent   : void pointer which caller can use for book keeping
 * @timer_cb : caller can chose to use its own callback function when
 *             timer fires the timeout. If no value is set timer util
 *             will use default.
 */
int crm_timer_init(struct cam_req_mgr_timer **timer,
	int32_t expires, void *parent, void (*timer_cb)(struct timer_list *));

/**
 * crm_timer_exit()
 * @brief : destroys the timer allocated.
 * @timer : timer pointer which will be freed
 */
void crm_timer_exit(struct cam_req_mgr_timer **timer);

extern struct kmem_cache *g_cam_req_mgr_timer_cachep;
#endif
