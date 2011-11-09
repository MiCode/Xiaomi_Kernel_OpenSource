/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_RPM_H
#define __ARCH_ARM_MACH_MSM_CLOCK_RPM_H

#include <mach/rpm.h>

struct clk_ops;
extern struct clk_ops clk_ops_rpm;

struct rpm_clk {
	const int rpm_clk_id;
	const int rpm_status_id;
	const bool active_only;
	unsigned last_set_khz;
	/* 0 if active_only. Otherwise, same as last_set_khz. */
	unsigned last_set_sleep_khz;
	bool enabled;

	struct rpm_clk *peer;
	struct clk c;
};

static inline struct rpm_clk *to_rpm_clk(struct clk *clk)
{
	return container_of(clk, struct rpm_clk, c);
}

#define DEFINE_CLK_RPM(name, active, r_id, dep) \
	static struct rpm_clk active; \
	static struct rpm_clk name = { \
		.rpm_clk_id = MSM_RPM_ID_##r_id##_CLK, \
		.rpm_status_id = MSM_RPM_STATUS_ID_##r_id##_CLK, \
		.peer = &active, \
		.c = { \
			.ops = &clk_ops_rpm, \
			.flags = CLKFLAG_SKIP_AUTO_OFF, \
			.dbg_name = #name, \
			CLK_INIT(name.c), \
			.depends = dep, \
		}, \
	}; \
	static struct rpm_clk active = { \
		.rpm_clk_id = MSM_RPM_ID_##r_id##_CLK, \
		.rpm_status_id = MSM_RPM_STATUS_ID_##r_id##_CLK, \
		.peer = &name, \
		.active_only = true, \
		.c = { \
			.ops = &clk_ops_rpm, \
			.flags = CLKFLAG_SKIP_AUTO_OFF, \
			.dbg_name = #active, \
			CLK_INIT(active.c), \
			.depends = dep, \
		}, \
	};

#endif
