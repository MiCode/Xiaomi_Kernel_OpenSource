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
#include <linux/interrupt.h>
#include <mach/msm_iomap.h>
#include <mach/msm_bus.h>
#include <mach/socinfo.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_device.h"

#define KGSL_PWRFLAGS_POWER_ON 0
#define KGSL_PWRFLAGS_CLK_ON   1
#define KGSL_PWRFLAGS_AXI_ON   2
#define KGSL_PWRFLAGS_IRQ_ON   3

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

void kgsl_pwrctrl_pwrlevel_change(struct kgsl_device *device,
				unsigned int new_level)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	if (new_level < (pwr->num_pwrlevels - 1) &&
		new_level >= pwr->thermal_pwrlevel &&
		new_level != pwr->active_pwrlevel) {
		pwr->active_pwrlevel = new_level;
		if ((test_bit(KGSL_PWRFLAGS_CLK_ON, &pwr->power_flags)) ||
			(device->state == KGSL_STATE_NAP))
			clk_set_rate(pwr->grp_clks[0],
					pwr->pwrlevels[pwr->active_pwrlevel].
					gpu_freq);
		if (test_bit(KGSL_PWRFLAGS_AXI_ON, &pwr->power_flags)) {
			if (pwr->pcl)
				msm_bus_scale_client_update_request(pwr->pcl,
					pwr->pwrlevels[pwr->active_pwrlevel].
					bus_freq);
			else if (pwr->ebi1_clk)
				clk_set_rate(pwr->ebi1_clk,
					pwr->pwrlevels[pwr->active_pwrlevel].
					bus_freq);
		}
		KGSL_PWR_WARN(device, "kgsl pwr level changed to %d\n",
					  pwr->active_pwrlevel);
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
	for (i = 0; i < pwr->num_pwrlevels; i++) {
		if (abs(pwr->pwrlevels[i].gpu_freq - val) < delta) {
			if (max)
				pwr->thermal_pwrlevel = i;
			break;
		}
	}

	if (i == pwr->num_pwrlevels)
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
	struct kgsl_busy *b = &device->pwrctrl.busy;
	ret = snprintf(buf, 17, "%7d %7d\n",
				   b->on_time_old, b->time_old);
	if (!test_bit(KGSL_PWRFLAGS_AXI_ON, &device->pwrctrl.power_flags)) {
		b->on_time_old = 0;
		b->time_old = 0;
	}
	return ret;
}

DEVICE_ATTR(gpuclk, 0644, kgsl_pwrctrl_gpuclk_show, kgsl_pwrctrl_gpuclk_store);
DEVICE_ATTR(max_gpuclk, 0644, kgsl_pwrctrl_max_gpuclk_show,
	kgsl_pwrctrl_max_gpuclk_store);
DEVICE_ATTR(pwrnap, 0644, kgsl_pwrctrl_pwrnap_show, kgsl_pwrctrl_pwrnap_store);
DEVICE_ATTR(idle_timer, 0644, kgsl_pwrctrl_idle_timer_show,
	kgsl_pwrctrl_idle_timer_store);
DEVICE_ATTR(gpubusy, 0644, kgsl_pwrctrl_gpubusy_show,
	NULL);

static const struct device_attribute *pwrctrl_attr_list[] = {
	&dev_attr_gpuclk,
	&dev_attr_max_gpuclk,
	&dev_attr_pwrnap,
	&dev_attr_idle_timer,
	&dev_attr_gpubusy,
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

/* Track the amount of time the gpu is on vs the total system time. *
 * Regularly update the percentage of busy time displayed by sysfs. */
static void kgsl_pwrctrl_busy_time(struct kgsl_device *device, bool on_time)
{
	struct kgsl_busy *b = &device->pwrctrl.busy;
	int elapsed;
	if (b->start.tv_sec == 0)
		do_gettimeofday(&(b->start));
	do_gettimeofday(&(b->stop));
	elapsed = (b->stop.tv_sec - b->start.tv_sec) * 1000000;
	elapsed += b->stop.tv_usec - b->start.tv_usec;
	b->time += elapsed;
	if (on_time)
		b->on_time += elapsed;
	/* Update the output regularly and reset the counters. */
	if ((b->time > UPDATE_BUSY_VAL) ||
		!test_bit(KGSL_PWRFLAGS_AXI_ON, &device->pwrctrl.power_flags)) {
		b->on_time_old = b->on_time;
		b->time_old = b->time;
		b->on_time = 0;
		b->time = 0;
	}
	do_gettimeofday(&(b->start));
}

void kgsl_pwrctrl_clk(struct kgsl_device *device, int state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i = 0;
	if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_CLK_ON,
			&pwr->power_flags)) {
			KGSL_PWR_INFO(device,
				"clocks off, device %d\n", device->id);
			for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
				if (pwr->grp_clks[i])
					clk_disable(pwr->grp_clks[i]);
			if ((pwr->pwrlevels[0].gpu_freq > 0) &&
				(device->requested_state != KGSL_STATE_NAP))
				clk_set_rate(pwr->grp_clks[0],
					pwr->pwrlevels[pwr->num_pwrlevels - 1].
					gpu_freq);
			kgsl_pwrctrl_busy_time(device, true);
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_CLK_ON,
			&pwr->power_flags)) {
			KGSL_PWR_INFO(device,
				"clocks on, device %d\n", device->id);

			if ((pwr->pwrlevels[0].gpu_freq > 0) &&
				(device->state != KGSL_STATE_NAP))
				clk_set_rate(pwr->grp_clks[0],
					pwr->pwrlevels[pwr->active_pwrlevel].
						gpu_freq);

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
			KGSL_PWR_INFO(device,
				"axi off, device %d\n", device->id);
			if (pwr->ebi1_clk) {
				clk_set_rate(pwr->ebi1_clk, 0);
				clk_disable(pwr->ebi1_clk);
			}
			if (pwr->pcl)
				msm_bus_scale_client_update_request(pwr->pcl,
								    0);
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_AXI_ON,
			&pwr->power_flags)) {
			KGSL_PWR_INFO(device,
				"axi on, device %d\n", device->id);
			if (pwr->ebi1_clk) {
				clk_enable(pwr->ebi1_clk);
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
			KGSL_PWR_INFO(device,
				"power off, device %d\n", device->id);
			if (pwr->gpu_reg)
				regulator_disable(pwr->gpu_reg);
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_POWER_ON,
			&pwr->power_flags)) {
			KGSL_PWR_INFO(device,
				"power on, device %d\n", device->id);
			if (pwr->gpu_reg)
				regulator_enable(pwr->gpu_reg);
		}
	}
}

void kgsl_pwrctrl_irq(struct kgsl_device *device, int state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_IRQ_ON,
			&pwr->power_flags)) {
			KGSL_PWR_INFO(device,
				"irq on, device %d\n", device->id);
			enable_irq(pwr->interrupt_num);
			device->ftbl->irqctrl(device, 1);
		}
	} else if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_IRQ_ON,
			&pwr->power_flags)) {
			KGSL_PWR_INFO(device,
				"irq off, device %d\n", device->id);
			device->ftbl->irqctrl(device, 0);
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

	pwr->gpu_reg = regulator_get(NULL, pwr->regulator_name);
	if (IS_ERR(pwr->gpu_reg))
		pwr->gpu_reg = NULL;

	pwr->power_flags = 0;

	pwr->nap_allowed = pdata->nap_allowed;
	pwr->interval_timeout = pdata->idle_timeout;
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

	/*acquire interrupt */
	pwr->interrupt_num =
		platform_get_irq_byname(pdev, pwr->irq_name);

	if (pwr->interrupt_num <= 0) {
		KGSL_PWR_ERR(device, "platform_get_irq_byname failed: %d\n",
					 pwr->interrupt_num);
		result = -EINVAL;
		goto done;
	}

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

	unregister_early_suspend(&device->display_off);

	if (pwr->interrupt_num > 0) {
		if (pwr->have_irq) {
			free_irq(pwr->interrupt_num, NULL);
			pwr->have_irq = 0;
		}
		pwr->interrupt_num = 0;
	}

	clk_put(pwr->ebi1_clk);

	if (pwr->pcl)
		msm_bus_scale_unregister_client(pwr->pcl);

	pwr->pcl = 0;

	if (pwr->gpu_reg) {
		regulator_put(pwr->gpu_reg);
		pwr->gpu_reg = NULL;
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

	mutex_lock(&device->mutex);
	if (device->state & (KGSL_STATE_ACTIVE | KGSL_STATE_NAP)) {
		if ((device->requested_state != KGSL_STATE_SLEEP) &&
			(device->requested_state != KGSL_STATE_SLUMBER))
			kgsl_pwrscale_idle(device);

		if (kgsl_pwrctrl_sleep(device) != 0) {
			mod_timer(&device->idle_timer,
					jiffies +
					device->pwrctrl.interval_timeout);
			/* If the GPU has been too busy to sleep, make sure *
			 * that is acurately reflected in the % busy numbers. */
			device->pwrctrl.busy.no_nap_cnt++;
			if (device->pwrctrl.busy.no_nap_cnt > UPDATE_BUSY) {
				kgsl_pwrctrl_busy_time(device, true);
				device->pwrctrl.busy.no_nap_cnt = 0;
			}
		}
	} else if (device->state & (KGSL_STATE_HUNG |
					KGSL_STATE_DUMP_AND_RECOVER)) {
		device->requested_state = KGSL_STATE_NONE;
	}

	mutex_unlock(&device->mutex);
}

void kgsl_timer(unsigned long data)
{
	struct kgsl_device *device = (struct kgsl_device *) data;

	KGSL_PWR_INFO(device, "idle timer expired device %d\n", device->id);
	if (device->requested_state != KGSL_STATE_SUSPEND) {
		device->requested_state = KGSL_STATE_SLEEP;
		/* Have work run in a non-interrupt context. */
		queue_work(device->work_queue, &device->idle_check_ws);
	}
}

void kgsl_pre_hwaccess(struct kgsl_device *device)
{
	BUG_ON(!mutex_is_locked(&device->mutex));
	if (device->state & (KGSL_STATE_SLEEP | KGSL_STATE_NAP |
				KGSL_STATE_SLUMBER))
		kgsl_pwrctrl_wake(device);
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
_slumber(struct kgsl_device *device)
{
	int status = -EINVAL;
	if (!device)
		return -EINVAL;
	KGSL_PWR_WARN(device, "Slumber start\n");

	device->requested_state = KGSL_STATE_SLUMBER;
	del_timer(&device->idle_timer);
	switch (device->state) {
	case KGSL_STATE_ACTIVE:
		/* Wait for the device to become idle */
		device->ftbl->idle(device, KGSL_TIMEOUT_DEFAULT);
	case KGSL_STATE_NAP:
	case KGSL_STATE_SLEEP:
		device->ftbl->suspend_context(device);
		device->ftbl->stop(device);
		device->state = KGSL_STATE_SLUMBER;
		device->pwrctrl.restore_slumber = 1;
		KGSL_PWR_WARN(device, "state -> SLUMBER, device %d\n",
				device->id);
		break;
	default:
		break;
	}
	status = 0;
	/* Don't set requested state to NONE
	It's done in kgsl_pwrctrl_sleep*/
	KGSL_PWR_WARN(device, "Done going to slumber\n");
	return status;
}

/******************************************************************/
/* Caller must hold the device mutex. */
int kgsl_pwrctrl_sleep(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	KGSL_PWR_INFO(device, "sleep device %d\n", device->id);

	/* Work through the legal state transitions */
	if ((device->requested_state == KGSL_STATE_NAP)) {
		if (device->pwrctrl.restore_slumber) {
			device->requested_state = KGSL_STATE_NONE;
			return 0;
		} else if (device->ftbl->isidle(device))
			goto nap;
	} else if (device->requested_state == KGSL_STATE_SLEEP) {
		if (device->state == KGSL_STATE_NAP ||
			device->ftbl->isidle(device)) {
			if (!device->pwrctrl.restore_slumber)
				goto sleep;
			else
				goto slumber;
			}
	} else if (device->requested_state == KGSL_STATE_SLUMBER) {
		if (device->state == KGSL_STATE_INIT)
			return 0;
		if (device->ftbl->isidle(device))
			goto slumber;
	}

	device->requested_state = KGSL_STATE_NONE;
	return -EBUSY;


slumber:
	_slumber(device);

sleep:
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_OFF);
	if (pwr->pwrlevels[0].gpu_freq > 0)
		clk_set_rate(pwr->grp_clks[0],
				pwr->pwrlevels[pwr->num_pwrlevels - 1].
				gpu_freq);
	kgsl_pwrctrl_busy_time(device, false);
	pwr->busy.start.tv_sec = 0;
	device->pwrctrl.time = 0;

	kgsl_pwrscale_sleep(device);
	goto clk_off;

nap:
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
clk_off:
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_OFF);

	device->state = device->requested_state;
	device->requested_state = KGSL_STATE_NONE;
	if (device->idle_wakelock.name)
		wake_unlock(&device->idle_wakelock);
	KGSL_PWR_WARN(device, "state -> NAP/SLEEP(%d), device %d\n",
				  device->state, device->id);

	return 0;
}
EXPORT_SYMBOL(kgsl_pwrctrl_sleep);

static int
_wake_from_slumber(struct kgsl_device *device)
{
	int status = -EINVAL;
	if (!device)
		return -EINVAL;

	KGSL_PWR_WARN(device, "wake from slumber start\n");

	device->requested_state = KGSL_STATE_ACTIVE;
	kgsl_pwrctrl_pwrlevel_change(device, KGSL_PWRLEVEL_NOMINAL);
	status = device->ftbl->start(device, 0);
	device->requested_state = KGSL_STATE_NONE;

	KGSL_PWR_WARN(device, "Done waking from slumber\n");
	return status;
}

/******************************************************************/
/* Caller must hold the device mutex. */
void kgsl_pwrctrl_wake(struct kgsl_device *device)
{
	if (device->state & (KGSL_STATE_SUSPEND | KGSL_STATE_INIT))
		return;

	if (device->state == KGSL_STATE_SLUMBER)
		_wake_from_slumber(device);

	if (device->state != KGSL_STATE_NAP) {
		kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);
		kgsl_pwrscale_wake(device);
	}

	/* Turn on the core clocks */
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_ON);

	/* Enable state before turning on irq */
	device->state = KGSL_STATE_ACTIVE;
	KGSL_PWR_WARN(device, "state -> ACTIVE, device %d\n", device->id);
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_ON);

	/* Re-enable HW access */
	mod_timer(&device->idle_timer,
				jiffies + device->pwrctrl.interval_timeout);

	if (device->idle_wakelock.name)
		wake_lock(&device->idle_wakelock);

	KGSL_PWR_INFO(device, "wake return for device %d\n", device->id);
}
EXPORT_SYMBOL(kgsl_pwrctrl_wake);

void kgsl_pwrctrl_enable(struct kgsl_device *device)
{
	/* Order pwrrail/clk sequence based upon platform */
	kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_ON);
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_ON);
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);
}
EXPORT_SYMBOL(kgsl_pwrctrl_enable);

void kgsl_pwrctrl_disable(struct kgsl_device *device)
{
	/* Order pwrrail/clk sequence based upon platform */
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_OFF);
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_OFF);
	kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_OFF);
}
EXPORT_SYMBOL(kgsl_pwrctrl_disable);
