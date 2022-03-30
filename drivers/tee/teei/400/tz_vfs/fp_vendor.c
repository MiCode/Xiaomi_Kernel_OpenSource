// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#include "fp_vendor.h"
#include <linux/types.h>
#include "../tz_driver/include/nt_smc_call.h"
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/rtc.h>

#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

#define FPC_VENDOR_ID       0x12
#define GOODIX_VENDOR_ID    0x13

int fp_vendor_active;
int fp_vendor = FP_VENDOR_INVALID;
static DEFINE_MUTEX(fp_vendor_lock);

int get_fp_vendor(void)
{
	uint64_t fp_vendor_id_64 = 0;
	uint32_t *p_temp = NULL;
	uint32_t fp_vendor_id_32 = 0;

	mutex_lock(&fp_vendor_lock);

	if (fp_vendor_active) {
		mutex_unlock(&fp_vendor_lock);
		return fp_vendor;
	}

	get_t_device_id(&fp_vendor_id_64);

	p_temp = (uint32_t *)&fp_vendor_id_64;
	fp_vendor_id_32 = *p_temp;
	fp_vendor_id_32 = (fp_vendor_id_32 >> 8) & 0xff;

	IMSG_INFO("%s:%d->0x%x\n", __func__, __LINE__, fp_vendor_id_32);

	switch (fp_vendor_id_32) {
	case FPC_VENDOR_ID:
		fp_vendor = FPC_VENDOR;
		break;

	case GOODIX_VENDOR_ID:
		fp_vendor = GOODIX_VENDOR;
		break;

	default:
		fp_vendor = FP_VENDOR_INVALID;
		break;
	}

	fp_vendor_active = 1;
	mutex_unlock(&fp_vendor_lock);

	return fp_vendor;
}
