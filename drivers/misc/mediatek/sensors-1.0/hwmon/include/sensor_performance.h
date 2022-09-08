/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SENSOR_PERFORMANCE_H__
#define __SENSOR_PERFORMANCE_H__

#include "hwmsensor.h"
#include <linux/types.h>

enum SENSOR_STATUS {
	GOT_IPI,
	WORK_START,
	DATA_REPORT,
	STATUS_MAX,
};

struct time_records {
	u64 check_time;
	u64 sum_kernel_time;
	u16 count;
};

#define LIMIT 1000

/* #define DEBUG_PERFORMANCE */

#ifdef DEBUG_PERFORMANCE
extern void mark_timestamp(u8 sensor_type, enum SENSOR_STATUS status,
	u64 current_time,
				  u64 event_time);
extern void mark_ipi_timestamp(uint64_t cyc);
#else
#define mark_timestamp(A, B, C, D)
#define mark_ipi_timestamp(A)
#endif

#endif
