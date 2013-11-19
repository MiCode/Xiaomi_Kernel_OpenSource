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

#define KGSL_MAX_CLKS 6

/* Only two supported levels, min & max */
#define KGSL_CONSTRAINT_PWR_MAXLEVELS 2

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

struct kgsl_pwr_constraint {
	unsigned int type;
	unsigned int sub_type;
	union {
		struct {
			unsigned int level;
		} pwrlevel;
	} hint;
};

/**
 * struct kgsl_pwrctrl - Power control settings for a KGSL device
 * @interrupt_num - The interrupt number for the device
 * @ebi1_clk - Pointer to the EBI clock structure
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
 * @idle_needed - true if the device needs a idle before clock change
 * @irq_name - resource name for the IRQ
 * @clk_stats - structure of clock statistics
 * @pm_qos_req_dma - the power management quality of service structure
 * @pm_qos_latency - allowed CPU latency in microseconds
 * @bus_control - true if the bus calculation is independent
 * @bus_index - default bus index into the bus_ib table
 * @bus_ib - the set of unique ib requests needed for the bus calculation
 * @constraint - currently active power constraint
 */

struct kgsl_pwrctrl {
	int interrupt_num;
	struct clk *ebi1_clk;
	struct clk *grp_clks[KGSL_MAX_CLKS];
	unsigned long power_flags;
	unsigned long ctrl_flags;
	struct kgsl_pwrlevel pwrlevels[KGSL_MAX_PWRLEVELS];
	unsigned int active_pwrlevel;
	int thermal_pwrlevel;
	unsigned int default_pwrlevel;
	unsigned int init_pwrlevel;
	unsigned int max_pwrlevel;
	unsigned int min_pwrlevel;
	unsigned int num_pwrlevels;
	unsigned int interval_timeout;
	bool strtstp_sleepwake;
	struct regulator *gpu_reg;
	struct regulator *gpu_cx;
	uint32_t pcl;
	unsigned int idle_needed;
	const char *irq_name;
	bool irq_last;
	struct kgsl_clk_stats clk_stats;
	struct pm_qos_request pm_qos_req_dma;
	unsigned int pm_qos_latency;
	bool bus_control;
	int bus_mod;
	unsigned int bus_index[KGSL_MAX_PWRLEVELS];
	uint64_t bus_ib[KGSL_MAX_PWRLEVELS];
	struct kgsl_pwr_constraint constraint;
};

void kgsl_pwrctrl_irq(struct kgsl_device *device, int state);
int kgsl_pwrctrl_init(struct kgsl_device *device);
void kgsl_pwrctrl_close(struct kgsl_device *device);
void kgsl_timer(unsigned long data);
void kgsl_idle_check(struct work_struct *work);
void kgsl_pre_hwaccess(struct kgsl_device *device);
int kgsl_pwrctrl_sleep(struct kgsl_device *device);
int kgsl_pwrctrl_wake(struct kgsl_device *device, int priority);
void kgsl_pwrctrl_pwrlevel_change(struct kgsl_device *device,
	unsigned int level);
void kgsl_pwrctrl_buslevel_update(struct kgsl_device *device,
	bool on);
int kgsl_pwrctrl_init_sysfs(struct kgsl_device *device);
void kgsl_pwrctrl_uninit_sysfs(struct kgsl_device *device);
void kgsl_pwrctrl_enable(struct kgsl_device *device);
void kgsl_pwrctrl_disable(struct kgsl_device *device);
bool kgsl_pwrctrl_isenabled(struct kgsl_device *device);

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

void kgsl_pwrctrl_set_state(struct kgsl_device *device, unsigned int state);
void kgsl_pwrctrl_request_state(struct kgsl_device *device, unsigned int state);

int __must_check kgsl_active_count_get(struct kgsl_device *device);
int __must_check kgsl_active_count_get_light(struct kgsl_device *device);
void kgsl_active_count_put(struct kgsl_device *device);
int kgsl_active_count_wait(struct kgsl_device *device, int count);

#endif /* __KGSL_PWRCTRL_H */
