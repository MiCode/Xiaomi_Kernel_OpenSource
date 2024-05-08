#define pr_fmt(fmt) "ispv4 thermal" fmt
#ifndef __XM_ISPV_THERMAL_H
#define __XM_ISPV_THERMAL_H
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include "ispv4_rproc.h"
#include <linux/timer.h>
#include <linux/atomic.h>
#include <linux/component.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/kernfs.h>
// #include <linux/mfd/ispv4_cam_dev.h>
#include <linux/module.h>
#include <linux/timer.h>
#include "linux/kernel.h"
#include "../base/base.h"

extern void xm_ipc_knotify_register(struct xm_ispv4_rproc *rp,
			     void (*fn)(void *priv, void *data, int len),
			     void *p);

extern long ispv4_eptdev_isp_send(struct xm_ispv4_rproc *rp, enum xm_ispv4_etps ept,
			   u32 cmd, int len, void *data, bool user_buf,
			   int *msgid);
#endif