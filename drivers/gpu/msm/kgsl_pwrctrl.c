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

#include <linux/export.h>
#include <linux/interrupt.h>
#include <asm/page.h>
#include <linux/pm_runtime.h>
#include <mach/msm_iomap.h>
#include <mach/msm_bus.h>
#include <linux/ktime.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"

#define KGSL_PWRFLAGS_POWER_ON 0
#define KGSL_PWRFLAGS_CLK_ON   1
#define KGSL_PWRFLAGS_AXI_ON   2
#define KGSL_PWRFLAGS_IRQ_ON   3

#define GPU_SWFI_LATENCY	3
#define UPDATE_BUSY_VAL		1000000
#define UPDATE_BUSY		50

struct clk_pair {
	const char *name;
	uint map;
};

struct clk_pair clks[KGSL_MAX_CLKS] = {
	{
		.name = "src_clk",
		.map = KGSL_CLK_SRC,
	},
	{
		.name = "core_clk",
		.map = KGSL_CLK_CORE,
	},
	{
		.name = "iface_clk",
		.map = KGSL_CLK_IFACE,
	},
	{
		.name = "mem_clk",
		.map = KGSL_CLK_MEM,
	},
	{
		.name = "mem_iface_clk",
		.map = KGSL_CLK_MEM_IFACE,
	},
};

/* Update the elapsed time at a particular clock level
 * if the device is active(on_time = true).Otherwise
 * store it as sleep time.
 */
static void update_clk_statistics(struct kgsl_device *device,
				bool on_time)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_clk_stats *clkstats = &pwr->clk_stats;
	ktime_t elapsed;
	int elapsed_us;
	if (clkstats->start.tv64 == 0)
		clkstats->start = ktime_get();
	clkstats->stop = ktime_get();
	elapsed = ktime_sub(clkstats->stop, clkstats->start);
	elapsed_us = ktime_to_us(elapsed);
	clkstats->elapsed += elapsed_us;
	if (on_time)
		clkstats->clock_time[pwr->active_pwrlevel] += elapsed_us;
	else
		clkstats->clock_time[pwr->num_pwrlevels - 1] += elapsed_us;
	clkstats->start = ktime_get();
}

void kgsl_pwrctrl_pwrlevel_change(struct kgsl_device *device,
				unsigned int new_level)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	if (new_level < (pwr->num_pwrlevels - 1) &&
		new_level >= pwr->thermal_pwrlevel &&
		new_level != pwr->active_pwrlevel) {
		struct kgsl_pwrlevel *pwrlevel = &pwr->pwrlevels[new_level];
		int diff = new_level - pwr->active_pwrlevel;
		int d = (diff > 0) ? 1 : -1;
		int level = pwr->active_pwrlevel;
		/* Update the clock stats */
		update_clk_statistics(device, true);
		/* Finally set active level */
		pwr->active_pwrlevel = new_level;
		if ((test_bit(KGSL_PWRFLAGS_CLK_ON, &pwr->power_flags)) ||
			(device->state == KGSL_STATE_NAP)) {
			/*
			 * On some platforms, instability is caused on
			 * changing clock freq when the core is busy.
			 * Idle the gpu core before changing the clock freq.
			 */
			if (pwr->idle_needed == true)
				device->ftbl->idle(device);

			/* Don't shift by more than one level at a time to
			 * avoid glitches.
			 */
			while (level != new_level) {
				level += d;
				clk_set_rate(pwr->grp_clks[0],
						pwr->pwrlevels[level].gpu_freq);
			}
		}
		if (test_bit(KGSL_PWRFLAGS_AXI_ON, &pwr->power_flags)) {
			if (pwr->pcl)
				msm_bus_scale_client_update_request(pwr->pcl,
					pwrlevel->bus_freq);
			else if (pwr->ebi1_clk)
				clk_set_rate(pwr->ebi1_clk, pwrlevel->bus_freq);
		}
		trace_kgsl_pwrlevel(device, pwr->active_pwrlevel,
				    pwrlevel->gpu_freq);
	}
}
EXPORT_SYMBOL(kgsl_pwrctrl_pwrlevel_change);

static int __gpuclk_store(int max, struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t count)
{	int ret, i, delta = 5000000;
	unsigned long val;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;

	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;

	ret = sscanf(buf, "%ld", &val);
	if (ret != 1)
		return count;

	mutex_lock(&device->mutex);
	for (i = 0; i < pwr->num_pwrlevels - 1; i++) {
		if (abs(pwr->pwrlevels[i].gpu_freq - val) < delta) {
			if (max)
				pwr->thermal_pwrlevel = i;
			break;
		}
	}

	if (i == (pwr->num_pwrlevels - 1))
		goto done;

	/*
	 * If the current or requested clock speed is greater than the
	 * thermal limit, bump down immediately.
	 */

	if (pwr->pwrlevels[pwr->active_pwrlevel].gpu_freq >
	    pwr->pwrlevels[pwr->thermal_pwrlevel].gpu_freq)
		kgsl_pwrctrl_pwrlevel_change(device, pwr->thermal_pwrlevel);
	else if (!max)
		kgsl_pwrctrl_pwrlevel_change(device, i);

done:
	mutex_unlock(&device->mutex);
	return count;
}

static int kgsl_pwrctrl_max_gpuclk_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	return __gpuclk_store(1, dev, attr, buf, count);
}

static int kgsl_pwrctrl_max_gpuclk_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;
	return snprintf(buf, PAGE_SIZE, "%d\n",
			pwr->pwrlevels[pwr->thermal_pwrlevel].gpu_freq);
}

static int kgsl_pwrctrl_gpuclk_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	return __gpuclk_store(0, dev, attr, buf, count);
}

static int kgsl_pwrctrl_gpuclk_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;
	return snprintf(buf, PAGE_SIZE, "%d\n",
			pwr->pwrlevels[pwr->active_pwrlevel].gpu_freq);
}

static int kgsl_pwrctrl_pwrnap_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	char temp[20];
	unsigned long val;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	int rc;

	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;

	snprintf(temp, sizeof(temp), "%.*s",
			 (int)min(count, sizeof(temp) - 1), buf);
	rc = strict_strtoul(temp, 0, &val);
	if (rc)
		return rc;

	mutex_lock(&device->mutex);

	if (val == 1)
		pwr->nap_allowed = true;
	else if (val == 0)
		pwr->nap_allowed = false;

	mutex_unlock(&device->mutex);

	return count;
}

static int kgsl_pwrctrl_pwrnap_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	if (device == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "%d\n", device->pwrctrl.nap_allowed);
}


static int kgsl_pwrctrl_idle_timer_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	char temp[20];
	unsigned long val;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	const long div = 1000/HZ;
	static unsigned int org_interval_timeout = 1;
	int rc;

	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;

	snprintf(temp, sizeof(temp), "%.*s",
			 (int)min(count, sizeof(temp) - 1), buf);
	rc = strict_strtoul(temp, 0, &val);
	if (rc)
		return rc;

	if (org_interval_timeout == 1)
		org_interval_timeout = pwr->interval_timeout;

	mutex_lock(&device->mutex);

	/* Let the timeout be requested in ms, but convert to jiffies. */
	val /= div;
	if (val >= org_interval_timeout)
		pwr->interval_timeout = val;

	mutex_unlock(&device->mutex);

	return count;
}

static int kgsl_pwrctrl_idle_timer_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	if (device == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "%d\n",
		device->pwrctrl.interval_timeout);
}

static int kgsl_pwrctrl_gpubusy_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_clk_stats *clkstats = &device->pwrctrl.clk_stats;
	ret = snprintf(buf, PAGE_SIZE, "%7d %7d\n",
			clkstats->on_time_old, clkstats->elapsed_old);
	if (!test_bit(KGSL_PWRFLAGS_AXI_ON, &device->pwrctrl.power_flags)) {
		clkstats->on_time_old = 0;
		clkstats->elapsed_old = 0;
	}
	return ret;
}

static int kgsl_pwrctrl_gputop_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_clk_stats *clkstats = &device->pwrctrl.clk_stats;
	int i = 0;
	char *ptr = buf;

	ret = snprintf(buf, PAGE_SIZE, "%7d %7d ", clkstats->on_time_old,
					clkstats->elapsed_old);
	for (i = 0, ptr += ret; i < device->pwrctrl.num_pwrlevels;
							i++, ptr += ret)
		ret = snprintf(ptr, PAGE_SIZE, "%7d ",
						clkstats->old_clock_time[i]);

	if (!test_bit(KGSL_PWRFLAGS_AXI_ON, &device->pwrctrl.power_flags)) {
		clkstats->on_time_old = 0;
		clkstats->elapsed_old = 0;
		for (i = 0; i < KGSL_MAX_PWRLEVELS ; i++)
			clkstats->old_clock_time[i] = 0;
	}
	return (unsigned int) (ptr - buf);
}

static int kgsl_pwrctrl_gpu_available_frequencies_show(
					struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	int index, num_chars = 0;

	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;
	for (index = 0; index < pwr->num_pwrlevels - 1; index++)
		num_chars += snprintf(buf + num_chars, PAGE_SIZE, "%d ",
		pwr->pwrlevels[index].gpu_freq);
	buf[num_chars++] = '\n';
	return num_chars;
}

DEVICE_ATTR(gpuclk, 0644, kgsl_pwrctrl_gpuclk_show, kgsl_pwrctrl_gpuclk_store);
DEVICE_ATTR(max_gpuclk, 0644, kgsl_pwrctrl_max_gpuclk_show,
	kgsl_pwrctrl_max_gpuclk_store);
DEVICE_ATTR(pwrnap, 0664, kgsl_pwrctrl_pwrnap_show, kgsl_pwrctrl_pwrnap_store);
DEVICE_ATTR(idle_timer, 0644, kgsl_pwrctrl_idle_timer_show,
	kgsl_pwrctrl_idle_timer_store);
DEVICE_ATTR(gpubusy, 0444, kgsl_pwrctrl_gpubusy_show,
	NULL);
DEVICE_ATTR(gputop, 0444, kgsl_pwrctrl_gputop_show,
	NULL);
DEVICE_ATTR(gpu_available_frequencies, 0444,
	kgsl_pwrctrl_gpu_available_frequencies_show,
	NULL);

static const struct device_attribute *pwrctrl_attr_list[] = {
	&dev_attr_gpuclk,
	&dev_attr_max_gpuclk,
	&dev_attr_pwrnap,
	&dev_attr_idle_timer,
	&dev_attr_gpubusy,
	&dev_attr_gputop,
	&dev_attr_gpu_available_frequencies,
	NULL
};

int kgsl_pwrctrl_init_sysfs(struct kgsl_device *device)
{
	return kgsl_create_device_sysfs_files(device->dev, pwrctrl_attr_list);
}

void kgsl_pwrctrl_uninit_sysfs(struct kgsl_device *device)
{
	kgsl_remove_device_sysfs_files(device->dev, pwrctrl_attr_list);
}

static void update_statistics(struct kgsl_device *device)
{
	struct kgsl_clk_stats *clkstats = &device->pwrctrl.clk_stats;
	unsigned int on_time = 0;
	int i;
	int num_pwrlevels = device->pwrctrl.num_pwrlevels - 1;
	/*PER CLK TIME*/
	for (i = 0; i < num_pwrlevels; i++) {
		clkstats->old_clock_time[i] = clkstats->clock_time[i];
		on_time += clkstats->clock_time[i];
		clkstats->clock_time[i] = 0;
	}
	clkstats->old_clock_time[num_pwrlevels] =
				clkstats->clock_time[num_pwrlevels];
	clkstats->clock_time[num_pwrlevels] = 0;
	clkstats->on_time_old = on_time;
	clkstats->elapsed_old = clkstats->elapsed;
	clkstats->elapsed = 0;
}

/* Track the amount of time the gpu is on vs the total system time. *
 * Regularly update the percentage of busy time displayed by sysfs. */
static void kgsl_pwrctrl_busy_time(struct kgsl_device *device, bool on_time)
{
	struct kgsl_clk_stats *clkstats = &device->pwrctrl.clk_stats;
	update_clk_statistics(device, on_time);
	/* Update the output regularly and reset the counters. */
	if ((clkstats->elapsed > UPDATE_BUSY_VAL) ||
		!test_bit(KGSL_PWRFLAGS_AXI_ON, &device->pwrctrl.power_flags)) {
		update_statistics(device);
	}
}

void kgsl_pwrctrl_clk(struct kgsl_device *device, int state,
					  int requested_state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i = 0;
	if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_CLK_ON,
			&pwr->power_flags)) {
			trace_kgsl_clk(device, state);
			for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
				if (pwr->grp_clks[i])
					clk_disable(pwr->grp_clks[i]);
			/* High latency clock maintenance. */
			if ((pwr->pwrlevels[0].gpu_freq > 0) &&
				(requested_state != KGSL_STATE_NAP)) {
				clk_set_rate(pwr->grp_clks[0],
					pwr->pwrlevels[pwr->num_pwrlevels - 1].
					gpu_freq);
				for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
					if (pwr->grp_clks[i])
						clk_unprepare(pwr->grp_clks[i]);
			}
			kgsl_pwrctrl_busy_time(device, true);
		} else if (requested_state == KGSL_STATE_SLEEP) {
			/* High latency clock maintenance. */
			if ((pwr->pwrlevels[0].gpu_freq > 0))
				clk_set_rate(pwr->grp_clks[0],
					pwr->pwrlevels[pwr->num_pwrlevels - 1].
					gpu_freq);
			for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
				if (pwr->grp_clks[i])
					clk_unprepare(pwr->grp_clks[i]);
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_CLK_ON,
			&pwr->power_flags)) {
			trace_kgsl_clk(device, state);
			/* High latency clock maintenance. */
			if (device->state != KGSL_STATE_NAP) {
				for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
					if (pwr->grp_clks[i])
						clk_prepare(pwr->grp_clks[i]);

				if (pwr->pwrlevels[0].gpu_freq > 0)
					clk_set_rate(pwr->grp_clks[0],
						pwr->pwrlevels
						[pwr->active_pwrlevel].
						gpu_freq);
			}
			/* as last step, enable grp_clk
			   this is to let GPU interrupt to come */
			for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
				if (pwr->grp_clks[i])
					clk_enable(pwr->grp_clks[i]);
			kgsl_pwrctrl_busy_time(device, false);
		}
	}
}

void kgsl_pwrctrl_axi(struct kgsl_device *device, int state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_AXI_ON,
			&pwr->power_flags)) {
			trace_kgsl_bus(device, state);
			if (pwr->ebi1_clk) {
				clk_set_rate(pwr->ebi1_clk, 0);
				clk_disable_unprepare(pwr->ebi1_clk);
			}
			if (pwr->pcl)
				msm_bus_scale_client_update_request(pwr->pcl,
								    0);
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_AXI_ON,
			&pwr->power_flags)) {
			trace_kgsl_bus(device, state);
			if (pwr->ebi1_clk) {
				clk_prepare_enable(pwr->ebi1_clk);
				clk_set_rate(pwr->ebi1_clk,
					pwr->pwrlevels[pwr->active_pwrlevel].
					bus_freq);
			}
			if (pwr->pcl)
				msm_bus_scale_client_update_request(pwr->pcl,
					pwr->pwrlevels[pwr->active_pwrlevel].
						bus_freq);
		}
	}
}

void kgsl_pwrctrl_pwrrail(struct kgsl_device *device, int state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_POWER_ON,
			&pwr->power_flags)) {
			trace_kgsl_rail(device, state);
			if (pwr->gpu_cx)
				regulator_disable(pwr->gpu_cx);
			if (pwr->gpu_reg)
				regulator_disable(pwr->gpu_reg);
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_POWER_ON,
			&pwr->power_flags)) {
			trace_kgsl_rail(device, state);
			if (pwr->gpu_reg) {
				int status = regulator_enable(pwr->gpu_reg);
				if (status)
					KGSL_DRV_ERR(device,
							"core regulator_enable "
							"failed: %d\n",
							status);
			}
			if (pwr->gpu_cx) {
				int status = regulator_enable(pwr->gpu_cx);
				if (status)
					KGSL_DRV_ERR(device,
							"cx regulator_enable "
							"failed: %d\n",
							status);
			}
		}
	}
}

void kgsl_pwrctrl_irq(struct kgsl_device *device, int state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_IRQ_ON,
			&pwr->power_flags)) {
			trace_kgsl_irq(device, state);
			enable_irq(pwr->interrupt_num);
		}
	} else if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_IRQ_ON,
			&pwr->power_flags)) {
			trace_kgsl_irq(device, state);
			if (in_interrupt())
				disable_irq_nosync(pwr->interrupt_num);
			else
				disable_irq(pwr->interrupt_num);
		}
	}
}
EXPORT_SYMBOL(kgsl_pwrctrl_irq);

int kgsl_pwrctrl_init(struct kgsl_device *device)
{
	int i, result = 0;
	struct clk *clk;
	struct platform_device *pdev =
		container_of(device->parentdev, struct platform_device, dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_device_platform_data *pdata = pdev->dev.platform_data;

	/*acquire clocks */
	for (i = 0; i < KGSL_MAX_CLKS; i++) {
		if (pdata->clk_map & clks[i].map) {
			clk = clk_get(&pdev->dev, clks[i].name);
			if (IS_ERR(clk))
				goto clk_err;
			pwr->grp_clks[i] = clk;
		}
	}
	/* Make sure we have a source clk for freq setting */
	if (pwr->grp_clks[0] == NULL)
		pwr->grp_clks[0] = pwr->grp_clks[1];

	/* put the AXI bus into asynchronous mode with the graphics cores */
	if (pdata->set_grp_async != NULL)
		pdata->set_grp_async();

	if (pdata->num_levels > KGSL_MAX_PWRLEVELS) {
		KGSL_PWR_ERR(device, "invalid power level count: %d\n",
					 pdata->num_levels);
		result = -EINVAL;
		goto done;
	}
	pwr->num_pwrlevels = pdata->num_levels;
	pwr->active_pwrlevel = pdata->init_level;
	pwr->default_pwrlevel = pdata->init_level;
	for (i = 0; i < pdata->num_levels; i++) {
		pwr->pwrlevels[i].gpu_freq =
		(pdata->pwrlevel[i].gpu_freq > 0) ?
		clk_round_rate(pwr->grp_clks[0],
					   pdata->pwrlevel[i].
					   gpu_freq) : 0;
		pwr->pwrlevels[i].bus_freq =
			pdata->pwrlevel[i].bus_freq;
		pwr->pwrlevels[i].io_fraction =
			pdata->pwrlevel[i].io_fraction;
	}
	/* Do not set_rate for targets in sync with AXI */
	if (pwr->pwrlevels[0].gpu_freq > 0)
		clk_set_rate(pwr->grp_clks[0], pwr->
				pwrlevels[pwr->num_pwrlevels - 1].gpu_freq);

	pwr->gpu_reg = regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(pwr->gpu_reg))
		pwr->gpu_reg = NULL;

	if (pwr->gpu_reg) {
		pwr->gpu_cx = regulator_get(&pdev->dev, "vddcx");
		if (IS_ERR(pwr->gpu_cx))
			pwr->gpu_cx = NULL;
	} else
		pwr->gpu_cx = NULL;

	pwr->power_flags = 0;

	pwr->nap_allowed = pdata->nap_allowed;
	pwr->idle_needed = pdata->idle_needed;
	pwr->interval_timeout = pdata->idle_timeout;
	pwr->strtstp_sleepwake = pdata->strtstp_sleepwake;
	pwr->ebi1_clk = clk_get(&pdev->dev, "bus_clk");
	if (IS_ERR(pwr->ebi1_clk))
		pwr->ebi1_clk = NULL;
	else
		clk_set_rate(pwr->ebi1_clk,
					 pwr->pwrlevels[pwr->active_pwrlevel].
						bus_freq);
	if (pdata->bus_scale_table != NULL) {
		pwr->pcl = msm_bus_scale_register_client(pdata->
							bus_scale_table);
		if (!pwr->pcl) {
			KGSL_PWR_ERR(device,
					"msm_bus_scale_register_client failed: "
					"id %d table %p", device->id,
					pdata->bus_scale_table);
			result = -EINVAL;
			goto done;
		}
	}


	pm_runtime_enable(device->parentdev);
	register_early_suspend(&device->display_off);
	return result;

clk_err:
	result = PTR_ERR(clk);
	KGSL_PWR_ERR(device, "clk_get(%s) failed: %d\n",
				 clks[i].name, result);

done:
	return result;
}

void kgsl_pwrctrl_close(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i;

	KGSL_PWR_INFO(device, "close device %d\n", device->id);

	pm_runtime_disable(device->parentdev);
	unregister_early_suspend(&device->display_off);

	clk_put(pwr->ebi1_clk);

	if (pwr->pcl)
		msm_bus_scale_unregister_client(pwr->pcl);

	pwr->pcl = 0;

	if (pwr->gpu_reg) {
		regulator_put(pwr->gpu_reg);
		pwr->gpu_reg = NULL;
	}

	if (pwr->gpu_cx) {
		regulator_put(pwr->gpu_cx);
		pwr->gpu_cx = NULL;
	}

	for (i = 1; i < KGSL_MAX_CLKS; i++)
		if (pwr->grp_clks[i]) {
			clk_put(pwr->grp_clks[i]);
			pwr->grp_clks[i] = NULL;
		}

	pwr->grp_clks[0] = NULL;
	pwr->power_flags = 0;
}

void kgsl_idle_check(struct work_struct *work)
{
	struct kgsl_device *device = container_of(work, struct kgsl_device,
							idle_check_ws);
	WARN_ON(device == NULL);
	if (device == NULL)
		return;

	mutex_lock(&device->mutex);
	if (device->state & (KGSL_STATE_ACTIVE | KGSL_STATE_NAP)) {
		kgsl_pwrscale_idle(device, 0);

		if (kgsl_pwrctrl_sleep(device) != 0) {
			mod_timer(&device->idle_timer,
					jiffies +
					device->pwrctrl.interval_timeout);
			/* If the GPU has been too busy to sleep, make sure *
			 * that is acurately reflected in the % busy numbers. */
			device->pwrctrl.clk_stats.no_nap_cnt++;
			if (device->pwrctrl.clk_stats.no_nap_cnt >
							 UPDATE_BUSY) {
				kgsl_pwrctrl_busy_time(device, true);
				device->pwrctrl.clk_stats.no_nap_cnt = 0;
			}
		}
	} else if (device->state & (KGSL_STATE_HUNG |
					KGSL_STATE_DUMP_AND_RECOVER)) {
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
	}

	mutex_unlock(&device->mutex);
}

void kgsl_timer(unsigned long data)
{
	struct kgsl_device *device = (struct kgsl_device *) data;

	KGSL_PWR_INFO(device, "idle timer expired device %d\n", device->id);
	if (device->requested_state != KGSL_STATE_SUSPEND) {
		if (device->pwrctrl.restore_slumber ||
					device->pwrctrl.strtstp_sleepwake)
			kgsl_pwrctrl_request_state(device, KGSL_STATE_SLUMBER);
		else
			kgsl_pwrctrl_request_state(device, KGSL_STATE_SLEEP);
		/* Have work run in a non-interrupt context. */
		queue_work(device->work_queue, &device->idle_check_ws);
	}
}

void kgsl_pre_hwaccess(struct kgsl_device *device)
{
	BUG_ON(!mutex_is_locked(&device->mutex));
	switch (device->state) {
	case KGSL_STATE_ACTIVE:
		return;
	case KGSL_STATE_NAP:
	case KGSL_STATE_SLEEP:
	case KGSL_STATE_SLUMBER:
		kgsl_pwrctrl_wake(device);
		break;
	case KGSL_STATE_SUSPEND:
		kgsl_check_suspended(device);
		break;
	case KGSL_STATE_INIT:
	case KGSL_STATE_HUNG:
	case KGSL_STATE_DUMP_AND_RECOVER:
		if (test_bit(KGSL_PWRFLAGS_CLK_ON,
					 &device->pwrctrl.power_flags))
			break;
		else
			KGSL_PWR_ERR(device,
					"hw access while clocks off from state %d\n",
					device->state);
		break;
	default:
		KGSL_PWR_ERR(device, "hw access while in unknown state %d\n",
					 device->state);
		break;
	}
}
EXPORT_SYMBOL(kgsl_pre_hwaccess);

void kgsl_check_suspended(struct kgsl_device *device)
{
	if (device->requested_state == KGSL_STATE_SUSPEND ||
				device->state == KGSL_STATE_SUSPEND) {
		mutex_unlock(&device->mutex);
		wait_for_completion(&device->hwaccess_gate);
		mutex_lock(&device->mutex);
	} else if (device->state == KGSL_STATE_DUMP_AND_RECOVER) {
		mutex_unlock(&device->mutex);
		wait_for_completion(&device->recovery_gate);
		mutex_lock(&device->mutex);
	} else if (device->state == KGSL_STATE_SLUMBER)
		kgsl_pwrctrl_wake(device);
}

static int
_nap(struct kgsl_device *device)
{
	switch (device->state) {
	case KGSL_STATE_ACTIVE:
		if (!device->ftbl->isidle(device)) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
			return -EBUSY;
		}
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_OFF, KGSL_STATE_NAP);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_NAP);
	case KGSL_STATE_NAP:
	case KGSL_STATE_SLEEP:
	case KGSL_STATE_SLUMBER:
		break;
	default:
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		break;
	}
	return 0;
}

static void
_sleep_accounting(struct kgsl_device *device)
{
	kgsl_pwrctrl_busy_time(device, false);
	device->pwrctrl.clk_stats.start = ktime_set(0, 0);
	device->pwrctrl.time = 0;
	kgsl_pwrscale_sleep(device);
}

static int
_sleep(struct kgsl_device *device)
{
	switch (device->state) {
	case KGSL_STATE_ACTIVE:
		if (!device->ftbl->isidle(device)) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
			return -EBUSY;
		}
		/* fall through */
	case KGSL_STATE_NAP:
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_OFF);
		_sleep_accounting(device);
		kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_OFF, KGSL_STATE_SLEEP);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_SLEEP);
		pm_qos_update_request(&device->pm_qos_req_dma,
					PM_QOS_DEFAULT_VALUE);
		break;
	case KGSL_STATE_SLEEP:
	case KGSL_STATE_SLUMBER:
		break;
	default:
		KGSL_PWR_WARN(device, "unhandled state %s\n",
				kgsl_pwrstate_to_str(device->state));
		break;
	}
	return 0;
}

static int
_slumber(struct kgsl_device *device)
{
	switch (device->state) {
	case KGSL_STATE_ACTIVE:
		if (!device->ftbl->isidle(device)) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
			return -EBUSY;
		}
		/* fall through */
	case KGSL_STATE_NAP:
	case KGSL_STATE_SLEEP:
		del_timer_sync(&device->idle_timer);
		device->ftbl->suspend_context(device);
		device->ftbl->stop(device);
		_sleep_accounting(device);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_SLUMBER);
		pm_qos_update_request(&device->pm_qos_req_dma,
						PM_QOS_DEFAULT_VALUE);
		break;
	case KGSL_STATE_SLUMBER:
		break;
	default:
		KGSL_PWR_WARN(device, "unhandled state %s\n",
				kgsl_pwrstate_to_str(device->state));
		break;
	}
	return 0;
}

/******************************************************************/
/* Caller must hold the device mutex. */
int kgsl_pwrctrl_sleep(struct kgsl_device *device)
{
	int status = 0;
	KGSL_PWR_INFO(device, "sleep device %d\n", device->id);

	/* Work through the legal state transitions */
	switch (device->requested_state) {
	case KGSL_STATE_NAP:
		status = _nap(device);
		break;
	case KGSL_STATE_SLEEP:
		status = _sleep(device);
		break;
	case KGSL_STATE_SLUMBER:
		status = _slumber(device);
		break;
	default:
		KGSL_PWR_INFO(device, "bad state request 0x%x\n",
				device->requested_state);
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		status = -EINVAL;
		break;
	}
	return status;
}
EXPORT_SYMBOL(kgsl_pwrctrl_sleep);

/******************************************************************/
/* Caller must hold the device mutex. */
void kgsl_pwrctrl_wake(struct kgsl_device *device)
{
	int status;
	kgsl_pwrctrl_request_state(device, KGSL_STATE_ACTIVE);
	switch (device->state) {
	case KGSL_STATE_SLUMBER:
		status = device->ftbl->start(device, 0);
		if (status) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
			KGSL_DRV_ERR(device, "start failed %d\n", status);
			break;
		}
		/* fall through */
	case KGSL_STATE_SLEEP:
		kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);
		kgsl_pwrscale_wake(device);
		/* fall through */
	case KGSL_STATE_NAP:
		/* Turn on the core clocks */
		kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_ON, KGSL_STATE_ACTIVE);
		/* Enable state before turning on irq */
		kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_ON);
		/* Re-enable HW access */
		mod_timer(&device->idle_timer,
				jiffies + device->pwrctrl.interval_timeout);
		pm_qos_update_request(&device->pm_qos_req_dma,
					GPU_SWFI_LATENCY);
	case KGSL_STATE_ACTIVE:
		break;
	default:
		KGSL_PWR_WARN(device, "unhandled state %s\n",
				kgsl_pwrstate_to_str(device->state));
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		break;
	}
}
EXPORT_SYMBOL(kgsl_pwrctrl_wake);

void kgsl_pwrctrl_enable(struct kgsl_device *device)
{
	/* Order pwrrail/clk sequence based upon platform */
	kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_ON);
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_ON, KGSL_STATE_ACTIVE);
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);
}
EXPORT_SYMBOL(kgsl_pwrctrl_enable);

void kgsl_pwrctrl_disable(struct kgsl_device *device)
{
	/* Order pwrrail/clk sequence based upon platform */
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_OFF);
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_OFF, KGSL_STATE_SLEEP);
	kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_OFF);
}
EXPORT_SYMBOL(kgsl_pwrctrl_disable);

void kgsl_pwrctrl_set_state(struct kgsl_device *device, unsigned int state)
{
	trace_kgsl_pwr_set_state(device, state);
	device->state = state;
	device->requested_state = KGSL_STATE_NONE;
}
EXPORT_SYMBOL(kgsl_pwrctrl_set_state);

void kgsl_pwrctrl_request_state(struct kgsl_device *device, unsigned int state)
{
	if (state != KGSL_STATE_NONE && state != device->requested_state)
		trace_kgsl_pwr_request_state(device, state);
	device->requested_state = state;
}
EXPORT_SYMBOL(kgsl_pwrctrl_request_state);

const char *kgsl_pwrstate_to_str(unsigned int state)
{
	switch (state) {
	case KGSL_STATE_NONE:
		return "NONE";
	case KGSL_STATE_INIT:
		return "INIT";
	case KGSL_STATE_ACTIVE:
		return "ACTIVE";
	case KGSL_STATE_NAP:
		return "NAP";
	case KGSL_STATE_SLEEP:
		return "SLEEP";
	case KGSL_STATE_SUSPEND:
		return "SUSPEND";
	case KGSL_STATE_HUNG:
		return "HUNG";
	case KGSL_STATE_DUMP_AND_RECOVER:
		return "DNR";
	case KGSL_STATE_SLUMBER:
		return "SLUMBER";
	default:
		break;
	}
	return "UNKNOWN";
}
EXPORT_SYMBOL(kgsl_pwrstate_to_str);

