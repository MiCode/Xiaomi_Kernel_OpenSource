/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef __QCOM_AOSS_H__
#define __QCOM_AOSS_H__

#include <linux/err.h>
#include <linux/device.h>

struct qmp;
//MIUI ADD: DCVS ARBI
#if IS_ENABLED(CONFIG_MI_DCVS_ARBI)
extern wait_queue_head_t mi_wait_queue;
extern bool is_ready_send;

void stat_data(u32 data);
#endif
//END DCVS ARBI
#if IS_ENABLED(CONFIG_QCOM_AOSS_QMP)

int qmp_send(struct qmp *qmp, const char *fmt, ...);
struct qmp *qmp_get(struct device *dev);
void qmp_put(struct qmp *qmp);

#else

static inline int qmp_send(struct qmp *qmp, const char *fmt, ...)
{
	return -ENODEV;
}

static inline struct qmp *qmp_get(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}

static inline void qmp_put(struct qmp *qmp)
{
}

#endif

#endif
