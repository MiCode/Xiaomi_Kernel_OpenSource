/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __ADSP_SERVICE_H__
#define __ADSP_SERVICE_H__

#include <linux/notifier.h>
#include <linux/interrupt.h>
#include "adsp_reg.h"
#include "adsp_feature_define.h"
#include "adsp_ipi.h"

void adsp_read_status_release(const unsigned long dsp_event);
void reset_hal_feature_table(void);
long adsp_driver_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg);
long adsp_driver_compat_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg);

#endif
