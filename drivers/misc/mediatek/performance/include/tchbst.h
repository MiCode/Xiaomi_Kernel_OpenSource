/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef _TCHBST_H
#define _TCHBST_H

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>


#include "cpu_ctrl.h"
#include "eas_ctrl.h"
//#include "fpsgo_common.h"
#include "perf_ioctl.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/*tchbst 100 ms*/
#define TOUCH_TIMEOUT_NSEC 100000000
#define TOUCH_BOOST_EAS 80
#define TOUCH_BOOST_OPP 2
#define TOUCH_FSTB_ACTIVE_US 100000
#define TOUCH_TIME_TO_LAST_TOUCH_MS 600000

/*touch boost parent*/
int init_tchbst(struct proc_dir_entry *parent);

/*user*/
void switch_usrtch(int enable);
long usrtch_ioctl(unsigned int cmd, unsigned long arg);
int init_utch(struct proc_dir_entry *parent);
int notify_touch(int action);

/*kernel*/
int init_ktch(struct proc_dir_entry *parent);
int ktch_suspend(void);


#endif /* _TCHBST_H */
