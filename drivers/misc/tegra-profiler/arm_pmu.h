/*
 * drivers/misc/tegra-profiler/arm_pmu.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __ARM_PMU_H
#define __ARM_PMU_H

#include <linux/list.h>

#define QUADD_MAX_PMU_COUNTERS	32

struct quadd_pmu_event_info {
	int quadd_event_id;
	int hw_value;

	struct list_head list;
};

struct quadd_pmu_ctx {
	int arch;
	char arch_name[64];

	u32 counters_mask;

	struct list_head used_events;

	int l1_cache_rw;
	unsigned int *current_map;
};

#endif	/* __ARM_PMU_H */
