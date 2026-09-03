/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : slp_mgr.h
 * Abstract : This file is a implementation for itm sipc command/event function
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SLP_MGR_H__
#define __SLP_MGR_H__

#include <linux/completion.h>
#include <misc/marlin_platform.h>
#include <misc/wcn_bus.h>

#define SLP_MGR_HEADER "[slp_mgr]"
#define SLP_MGR_ERR(fmt, args...)	\
	pr_err(SLP_MGR_HEADER fmt "\n", ## args)
#define SLP_MGR_INFO(fmt, args...)	\
	pr_info(SLP_MGR_HEADER fmt "\n", ## args)
#define SLP_MGR_DBG(fmt, args...)	\
	pr_debug(SLP_MGR_HEADER fmt "\n", ## args)

/* cp2 sleep status */
#define	STAY_SLPING		0
#define	STAY_AWAKING	1
#define	STAY_DEATH	2

struct slp_mgr_t {
	struct mutex    drv_slp_lock;
	struct mutex    wakeup_lock;
	struct completion wakeup_ack_completion;
	unsigned int active_module;
	atomic_t  cp2_state;
};

#ifdef SLP_MGR_TEST
int slp_test_init(void);
#endif
struct slp_mgr_t *slp_get_info(void);
int slp_mgr_init(void);
int slp_mgr_deinit(void);
void slp_mgr_drv_sleep(enum slp_subsys subsys, bool enable);
int slp_mgr_wakeup(enum slp_subsys subsys);
void slp_mgr_reset(void);
int slp_mgr_death(void);

#endif
