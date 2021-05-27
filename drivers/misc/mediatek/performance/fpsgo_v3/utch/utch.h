/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _TCHBST_H
#define _TCHBST_H

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>

#include "perf_ioctl.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/*tchbst 100 ms*/
#define TOUCH_TIMEOUT_NSEC 100000000
#define TOUCH_BOOST_OPP 2
#define TOUCH_FSTB_ACTIVE_US 100000
#define TOUCH_TIME_TO_LAST_TOUCH_MS 600000

/* mtk_perf_ioctl.ko */
extern int (*usrtch_ioctl_fp)(unsigned long arg);
extern struct proc_dir_entry *perfmgr_root;

/* mtk_fpsgo.ko */
//extern int is_fstb_active(long long time_diff);
extern void exit_utch_mod(void);
extern int init_utch_mod(void);
extern int notify_touch(int action);

#endif /* _TCHBST_H */

