// SPDX-License-Identifier: GPL-2.0+
/*
 * mi_locking will show kernel lock wait/hold time.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#define MI_LOCK_LOG_TAG       "locking_main"

#include <linux/module.h>
#include <linux/sched/clock.h>

#include "locking_main.h"
#include "holdlock.h"
#include "waitlock.h"
#include "internal.h"

/*  wait&hold lock cnt */
unsigned int g_opt_enable;
/*  debug log printf */
unsigned int g_opt_debug;
/*  wait&hold stack trace */
unsigned int g_opt_stack;
/* wait lock sort by max,avg,total,cnt */
unsigned int g_opt_sort;
/* hold lock check nvcsw first */
unsigned int g_opt_nvcsw;

#define WAITLOCK_DIRNAME        "wait"
#define HOLDLOCK_DIRNAME        "hold"

struct proc_dir_entry *d_xm_locking;
struct proc_dir_entry *d_xm_locking_wait;
struct proc_dir_entry *d_xm_locking_hold;
u64 lockstat_init_time = 0;

void lock_sysfs_init(void)
{
    /* create /proc/trace_locking */
    d_xm_locking = xiaomi_proc_mkdir(XM_LOCKING_DIRNAME, NULL);
    if (!d_xm_locking) {
        ml_err("Failed to create /proc/trace_locking\n");
        goto err_exit;
    }

    /* create /proc/trace_locking/wait */
    d_xm_locking_wait = proc_mkdir(WAITLOCK_DIRNAME, d_xm_locking);
    if (!d_xm_locking_wait) {
        ml_err("Failed to create /proc/trace_locking/wait\n");
        goto err_cleanup_d_xm_locking;
    }

    // create /proc/trace_locking/hold */
    d_xm_locking_hold = proc_mkdir(HOLDLOCK_DIRNAME, d_xm_locking);
    if (!d_xm_locking_hold) {
        ml_err("Failed to create /proc/trace_locking/hold\n");
        goto err_cleanup_d_xm_locking_wait;
    }

    ml_info("Successfully created /proc/trace_locking and subdirectories\n");
    return;

err_cleanup_d_xm_locking_wait:
    remove_proc_entry(WAITLOCK_DIRNAME, d_xm_locking);

err_cleanup_d_xm_locking:
    remove_proc_entry(XM_LOCKING_DIRNAME, NULL);

err_exit:
    return;
}

void lock_sysfs_exit(void)
{
    if (d_xm_locking_hold) {
        remove_proc_subtree(HOLDLOCK_DIRNAME, d_xm_locking);
        d_xm_locking_hold = NULL;
        ml_info("Removed /proc/trace_locking/hold\n");
    } else {
        ml_err("/proc/trace_locking/hold does not exist\n");
    }

    if (d_xm_locking_wait) {
        remove_proc_subtree(WAITLOCK_DIRNAME, d_xm_locking);
        d_xm_locking_wait = NULL;
        ml_info("Removed /proc/trace_locking/wait\n");
    } else {
        ml_err("/proc/trace_locking/wait does not exist\n");
    }

    if (d_xm_locking) {
        remove_proc_entry(XM_LOCKING_DIRNAME, NULL);
        d_xm_locking = NULL;
        ml_info("Removed /proc/trace_locking\n");
    } else {
        ml_err("/proc/trace_locking does not exist\n");
    }
}


int  xm_locking_init(void)
{
	int ret = 0;

    lockstat_init_time = local_clock();
    ////g_opt_enable = 1;
	g_opt_enable |= WAIT_LK_ENABLE;
	////g_opt_enable |= HOLD_LK_ENABLE;
	g_opt_nvcsw = 0;
	g_opt_stack = 1;
	g_opt_sort = 0;

	/* creat wait & hold folder */
	lock_sysfs_init();
	/* create wait sub file & folder ,init vendor hook */
	kern_lstat_init();
	/* creat hold sub file & folder, init vendor hook */
	holdlock_init();

	return ret;
}

void xm_locking_exit(void)
{
	////g_opt_enable = 0;
	g_opt_debug = 0;
	g_opt_stack = 1;
	g_opt_sort = 0;

	holdlock_exit();

	kern_lstat_exit();

	lock_sysfs_exit();
}

////module_init(xm_locking_init);
////module_exit(xm_locking_exit);

module_param_named(locking_stack, g_opt_stack, uint, 0660);
module_param_named(locking_enable, g_opt_enable, uint, 0660);
module_param_named(locking_sort, g_opt_sort, uint, 0660);
module_param_named(locking_debug, g_opt_debug, uint, 0660);
module_param_named(locking_nvcsw, g_opt_nvcsw, uint, 0660);


////MODULE_DESCRIPTION("Xiaomi Locking Vender Hooks Driver");
////MODULE_LICENSE("GPL v2");
