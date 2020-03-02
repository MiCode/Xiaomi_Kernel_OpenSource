/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#define pr_fmt(fmt) "<SEN_PER> " fmt

#include <hwmsensor.h>
#include <linux/types.h>
#include <sensor_performance.h>

#ifdef DEBUG_PERFORMANCE
struct time_records record[STATUS_MAX];
void mark_timestamp(u8 sensor_type, enum SENSOR_STATUS status, u64 current_time,
		    u64 event_time)
{
	record[status].check_time = current_time;
	record[status].count++;

	if (status == STATUS_MAX - 1) {
		int i;

		record[GOT_IPI].sum_kernel_time +=
			record[GOT_IPI].check_time - event_time;
		for (i = 1; i < STATUS_MAX; i++)
			record[i].sum_kernel_time +=
				record[i].check_time - record[i - 1].check_time;
		if (record[status].count == LIMIT) {
			for (i = 0; i < STATUS_MAX; i++) {
				pr_debug(
					"Sensor[%d] ====> last event stage[%d] check time:%lld\n",
					sensor_type, i, record[i].check_time);
				pr_debug(
					"sensor[%d] ====> stage[%d] average delta time:%lld on %d events\n",
					sensor_type, i,
					record[i].sum_kernel_time / LIMIT,
					record[i].count);
				record[i].sum_kernel_time = 0;
				record[i].count = 0;
			}
		}
	}
}

struct time_records ipi_time_records;
void mark_ipi_timestamp(uint64_t cyc)
{
#define ARCH_TIMER_MULT 161319385
#define ARCH_TIMER_SHIFT 21
	uint64_t time_ns = (cyc * ARCH_TIMER_MULT) >> ARCH_TIMER_SHIFT;

	ipi_time_records.sum_kernel_time += time_ns;
	ipi_time_records.count++;
	if (ipi_time_records.count == LIMIT) {
		pr_debug("Sensor ====> ipi average time on 1000 is :%lld\n",
			 ipi_time_records.sum_kernel_time / LIMIT);
		ipi_time_records.sum_kernel_time = 0;
		ipi_time_records.count = 0;
	}
}
#endif
