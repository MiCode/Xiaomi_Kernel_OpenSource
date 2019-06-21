/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
