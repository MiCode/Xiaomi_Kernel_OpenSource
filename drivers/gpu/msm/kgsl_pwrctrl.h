/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
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

#define KGSL_PWR_ON	0xFFFF

#define KGSL_MAX_CLKS 9

/* Only two supported levels, min & max */
#define KGSL_CONSTRAINT_PWR_MAXLEVELS 2

#define KGSL_RBBMTIMER_CLK_FREQ	19200000

/* Symbolic table for the constraint type */
#define KGSL_CONSTRAINT_TYPES \
	{ KGSL_CONSTRAINT_NONE, "None" }, \
	{ KGSL_CONSTRAINT_PWRLEVEL, "Pwrlevel" }
/* Symbolic table for the constraint sub type */
#define KGSL_CONSTRAINT_PWRLEVEL_SUBTYPES \
	{ KGSL_CONSTRAINT_PWR_MIN, "Min" }, \
	{ KGSL_CONSTRAINT_PWR_MAX, "Max" }

/*
 * States for thermal cycling.  _DISABLE means that no cycling has been
 * requested.  _ENABLE means that cycling has been requested, but GPU
 * DCVS is currently recommending running at a lower frequency than the
 * cycle frequency.  _ACTIVE means that the frequency is actively being
 * cycled.
 */
#define CYCLE_DISABLE	0
#define CYCLE_ENABLE	1
#define CYCLE_ACTIVE	2

struct platform_device;

struct kgsl_clk_stats {
	unsigned int busy;
	unsigned int total;
	unsigned int busy_old;
	unsigned int total_old;
};

struct kgsl_pwr_constraint {
	unsigned int type;
	unsigned int sub_type;
	union {
		struct {
			unsigned int level;
		} pwrlevel;
	} hint;
	unsigned long expires;
	uint32_t owner_id;
};

/**
 * struct kgsl_pwrctrl - Power control settings for a KGSL device
 * @interrupt_num - The interrupt number for the device
 * @grp_clks - Array of clocks structures that we control
 * @power_flags - Control flags for power
 * @pwrlevels - List of supported power levels
 * @active_pwrlevel - The currently active power level
 * @thermal_pwrlevel - maximum powerlevel constraint from thermal
 * @default_pwrlevel - device wake up power level
 * @init_pwrlevel - device inital power level
 * @max_pwrlevel - maximum allowable powerlevel per the user
 * @min_pwrlevel - minimum allowable powerlevel per the user
 * @num_pwrlevels - number of available power levels
 * @interval_timeout - timeout in jiffies to be idle before a power event
 * @strtstp_sleepwake - true if the device supports low latency GPU start/stop
 * @gpu_reg - pointer to the regulator structure for gpu_reg
 * @gpu_cx - pointer to the regulator structure for gpu_cx
 * @pcl - bus scale identifier
 * @irq_name - resource name for the IRQ
 * @clk_stats - structure of clock statistics
 * @pm_qos_req_dma - the power management quality of service structure
 * @pm_qos_active_latency - allowed CPU latency in microseconds when active
 * @pm_qos_wakeup_latency - allowed CPU latency in microseconds during wakeup
 * @bus_control - true if the bus calculation is independent
 * @bus_mod - modifier from the current power level for the bus vote
 * @bus_percent_ab - current percent of total possible bus usage
 * @bus_index - default bus index into the bus_ib table
 * @bus_ib - the set of unique ib requests needed for the bus calculation
 * @constraint - currently active power constraint
 * @superfast - Boolean flag to indicate that the GPU start should be run in the
 * higher priority thread
 */

struct kgsl_pwrctrl {
	int interrupt_num;
	struct clk *grp_clks[KGSL_MAX_CLKS];
	unsigned long power_flags;
	unsigned long ctrl_flags;
	struct kgsl_pwrlevel pwrlevels[KGSL_MAX_PWRLEVELS];
	unsigned int active_pwrlevel;
	unsigned int thermal_pwrlevel;
	unsigned int default_pwrlevel;
	unsigned int init_pwrlevel;
	unsigned int wakeup_maxpwrlevel;
	unsigned int max_pwrlevel;
	unsigned int min_pwrlevel;
	unsigned int num_pwrlevels;
	unsigned long interval_timeout;
	bool strtstp_sleepwake;
	struct regulator *gpu_reg;
	struct regulator *gpu_cx;
	uint32_t pcl;
	const char *irq_name;
	struct kgsl_clk_stats clk_stats;
	struct pm_qos_request pm_qos_req_dma;
	unsigned int pm_qos_active_latency;
	unsigned int pm_qos_wakeup_latency;
	bool bus_control;
	int bus_mod;
	unsigned int bus_percent_ab;
	struct device *devbw;
	unsigned int bus_index[KGSL_MAX_PWRLEVELS];
	uint64_t bus_ib[KGSL_MAX_PWRLEVELS];
	struct kgsl_pwr_constraint constraint;
	bool superfast;
	struct work_struct thermal_cycle_ws;
	struct timer_list thermal_timer;
	uint32_t thermal_timeout;
	uint32_t thermal_cycle;
	uint32_t thermal_highlow;
};

void kgsl_pwrctrl_irq(struct kgsl_device *device, int state);
int kgsl_pwrctrl_init(struct kgsl_device *device);
void kgsl_pwrctrl_close(struct kgsl_device *device);
void kgsl_timer(unsigned long data);
void kgsl_idle_check(struct work_struct *work);
void kgsl_pre_hwaccess(struct kgsl_device *device);
void kgsl_pwrctrl_pwrlevel_change(struct kgsl_device *device,
	unsigned int level);
void kgsl_pwrctrl_buslevel_update(struct kgsl_device *device,
	bool on);
int kgsl_pwrctrl_init_sysfs(struct kgsl_device *device);
void kgsl_pwrctrl_uninit_sysfs(struct kgsl_device *device);
int kgsl_pwrctrl_enable(struct kgsl_device *device);
void kgsl_pwrctrl_disable(struct kgsl_device *device);
bool kgsl_pwrctrl_isenabled(struct kgsl_device *device);
int kgsl_pwrctrl_change_state(struct kgsl_device *device, int state);

static inline unsigned long kgsl_get_clkrate(struct clk *clk)
{
	return (clk != NULL) ? clk_get_rate(clk) : 0;
}

/*
 * kgsl_pwrctrl_active_freq - get currently configured frequency
 * @pwr: kgsl_pwrctrl structure for the device
 *
 * Returns the currently configured frequency for the device.
 */
static inline unsigned long
kgsl_pwrctrl_active_freq(struct kgsl_pwrctrl *pwr)
{
	return pwr->pwrlevels[pwr->active_pwrlevel].gpu_freq;
}

int __must_check kgsl_active_count_get(struct kgsl_device *device);
void kgsl_active_count_put(struct kgsl_device *device);
int kgsl_active_count_wait(struct kgsl_device *device, int count);
void kgsl_pwrctrl_busy_time(struct kgsl_device *device, u64 time, u64 busy);
void kgsl_pwrctrl_set_constraint(struct kgsl_device *device,
			struct kgsl_pwr_constraint *pwrc, uint32_t id);
#endif /* __KGSL_PWRCTRL_H */
