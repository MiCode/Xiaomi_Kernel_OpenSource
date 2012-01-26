/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
#ifndef __KGSL_PWRCTRL_H
#define __KGSL_PWRCTRL_H

/*****************************************************************************
** power flags
*****************************************************************************/
#define KGSL_PWRFLAGS_ON   1
#define KGSL_PWRFLAGS_OFF  0

#define KGSL_PWRLEVEL_TURBO 0
#define KGSL_PWRLEVEL_NOMINAL 1
#define KGSL_PWRLEVEL_LAST_OFFSET 2

#define KGSL_MAX_CLKS 5

struct platform_device;

struct kgsl_clk_stats {
	unsigned int old_clock_time[KGSL_MAX_PWRLEVELS];
	unsigned int clock_time[KGSL_MAX_PWRLEVELS];
	unsigned int on_time_old;
	ktime_t start;
	ktime_t stop;
	unsigned int no_nap_cnt;
	unsigned int elapsed;
	unsigned int elapsed_old;
};

struct kgsl_pwrctrl {
	int interrupt_num;
	struct clk *ebi1_clk;
	struct clk *grp_clks[KGSL_MAX_CLKS];
	unsigned long power_flags;
	struct kgsl_pwrlevel pwrlevels[KGSL_MAX_PWRLEVELS];
	unsigned int active_pwrlevel;
	int thermal_pwrlevel;
	unsigned int default_pwrlevel;
	unsigned int num_pwrlevels;
	unsigned int interval_timeout;
	bool strtstp_sleepwake;
	struct regulator *gpu_reg;
	struct regulator *gpu_cx;
	uint32_t pcl;
	unsigned int nap_allowed;
	unsigned int idle_needed;
	const char *irq_name;
	s64 time;
	unsigned int restore_slumber;
	struct kgsl_clk_stats clk_stats;
};

void kgsl_pwrctrl_irq(struct kgsl_device *device, int state);
int kgsl_pwrctrl_init(struct kgsl_device *device);
void kgsl_pwrctrl_close(struct kgsl_device *device);
void kgsl_timer(unsigned long data);
void kgsl_idle_check(struct work_struct *work);
void kgsl_pre_hwaccess(struct kgsl_device *device);
void kgsl_check_suspended(struct kgsl_device *device);
int kgsl_pwrctrl_sleep(struct kgsl_device *device);
void kgsl_pwrctrl_wake(struct kgsl_device *device);
void kgsl_pwrctrl_pwrlevel_change(struct kgsl_device *device,
	unsigned int level);
int kgsl_pwrctrl_init_sysfs(struct kgsl_device *device);
void kgsl_pwrctrl_uninit_sysfs(struct kgsl_device *device);
void kgsl_pwrctrl_enable(struct kgsl_device *device);
void kgsl_pwrctrl_disable(struct kgsl_device *device);
static inline unsigned long kgsl_get_clkrate(struct clk *clk)
{
	return (clk != NULL) ? clk_get_rate(clk) : 0;
}

void kgsl_pwrctrl_set_state(struct kgsl_device *device, unsigned int state);
void kgsl_pwrctrl_request_state(struct kgsl_device *device, unsigned int state);
#endif /* __KGSL_PWRCTRL_H */
