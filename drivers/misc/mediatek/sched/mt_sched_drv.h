#ifndef _MT_SCHED_DRV_H
#define _MT_SCHED_DRV_H

#include <linux/ioctl.h>
#include <linux/cpumask.h>
#include <linux/pid.h>
#include <linux/mt_sched_ioctl.h>

#ifdef CONFIG_COMPAT
long sched_ioctl_compat(struct file *filp, unsigned int cmd, unsigned long arg);
#endif

#endif
