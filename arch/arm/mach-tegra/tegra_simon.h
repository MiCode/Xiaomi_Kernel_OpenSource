/*
 * arch/arm/mach-tegra/tegra_simon.h
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_TEGRA_SIMON_H_
#define _MACH_TEGRA_SIMON_H_

enum tegra_simon_domain {
	TEGRA_SIMON_DOMAIN_NONE = 0,
	TEGRA_SIMON_DOMAIN_CPU,
	TEGRA_SIMON_DOMAIN_GPU,
	TEGRA_SIMON_DOMAIN_CORE,

	TEGRA_SIMON_DOMAIN_NUM,
};

#define TEGRA_SIMON_GRADING_INTERVAL_SEC	86400
#define TEGRA_SIMON_GRADING_TIMEOUT_SEC		864000

struct tegra_simon_grader_desc {
	enum tegra_simon_domain		domain;
	int				settle_us;
	int				grading_mv_max;
	unsigned long			grading_rate_max;
	int				grading_temperature_min;
	int (*grade_simon_domain) (int domain, int mv, int temperature);
};

struct tegra_simon_grader {
	enum tegra_simon_domain		domain;
	const char			*domain_name;

	spinlock_t			grade_lock;
	struct timer_list		grade_wdt;
	ktime_t				last_grading;
	bool				stop_grading;
	int				grade;

	struct work_struct		grade_update_work;
	struct notifier_block		grading_condition_nb;
	struct thermal_zone_device	*tzd;
	struct clk			*clk;
	struct tegra_simon_grader_desc	*desc;
};

#ifdef CONFIG_TEGRA_USE_SIMON
int tegra_register_simon_notifier(struct notifier_block *nb);
void tegra_unregister_simon_notifier(struct notifier_block *nb);
int tegra_simon_add_grader(struct tegra_simon_grader_desc *desc);
#else
static inline int tegra_register_simon_notifier(struct notifier_block *nb)
{ return -ENOSYS; }
static inline void tegra_unregister_simon_notifier(struct notifier_block *nb)
{ }
static inline int tegra_simon_add_grader(struct tegra_simon_grader_desc *desc)
{ return -ENOSYS; }
#endif

#endif /* _MACH_TEGRA_SIMON_H_ */
