/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <mach/msm_iomap.h>

#include "spm.h"
#include "spm_driver.h"

struct msm_spm_power_modes {
	uint32_t mode;
	bool notify_rpm;
	uint32_t start_addr;

};

struct msm_spm_device {
	struct msm_spm_driver_data reg_data;
	struct msm_spm_power_modes *modes;
	uint32_t num_modes;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct msm_spm_device, msm_cpu_spm_device);
static atomic_t msm_spm_set_vdd_x_cpu_allowed = ATOMIC_INIT(1);

void msm_spm_allow_x_cpu_set_vdd(bool allowed)
{
	atomic_set(&msm_spm_set_vdd_x_cpu_allowed, allowed ? 1 : 0);
}

int msm_spm_set_vdd(unsigned int cpu, unsigned int vlevel)
{
	unsigned long flags;
	struct msm_spm_device *dev;
	int ret = -EIO;

	local_irq_save(flags);
	if (!atomic_read(&msm_spm_set_vdd_x_cpu_allowed) &&
				unlikely(smp_processor_id() != cpu)) {
		goto set_vdd_x_cpu_bail;
	}

	dev = &per_cpu(msm_cpu_spm_device, cpu);
	ret = msm_spm_drv_set_vdd(&dev->reg_data, vlevel);

set_vdd_x_cpu_bail:
	local_irq_restore(flags);
	return ret;
}

static int msm_spm_dev_set_low_power_mode(struct msm_spm_device *dev,
		unsigned int mode, bool notify_rpm)
{
	uint32_t i;
	uint32_t start_addr = 0;
	int ret = -EINVAL;

	if (mode == MSM_SPM_MODE_DISABLED) {
		ret = msm_spm_drv_set_spm_enable(&dev->reg_data, false);
	} else if (!msm_spm_drv_set_spm_enable(&dev->reg_data, true)) {
		for (i = 0; i < dev->num_modes; i++) {
			if ((dev->modes[i].mode == mode) &&
				(dev->modes[i].notify_rpm == notify_rpm)) {
				start_addr = dev->modes[i].start_addr;
				break;
			}
		}
		ret = msm_spm_drv_set_low_power_mode(&dev->reg_data,
					start_addr);
	}
	return ret;
}

static int __init msm_spm_dev_init(struct msm_spm_device *dev,
		struct msm_spm_platform_data *data)
{
	int i, ret = -ENOMEM;
	uint32_t offset = 0;

	dev->num_modes = data->num_modes;
	dev->modes = kmalloc(
			sizeof(struct msm_spm_power_modes) * dev->num_modes,
			GFP_KERNEL);

	if (!dev->modes)
		goto spm_failed_malloc;

	ret = msm_spm_drv_init(&dev->reg_data, data);

	if (ret)
		goto spm_failed_init;

	for (i = 0; i < dev->num_modes; i++) {

		ret = msm_spm_drv_write_seq_data(&dev->reg_data,
						data->modes[i].cmd, offset);
		if (ret < 0)
			goto spm_failed_init;

		dev->modes[i].mode = data->modes[i].mode;
		dev->modes[i].notify_rpm = data->modes[i].notify_rpm;
		dev->modes[i].start_addr = offset;
		offset += ret;
	}
	msm_spm_drv_flush_seq_entry(&dev->reg_data);
	return 0;

spm_failed_init:
	kfree(dev->modes);
spm_failed_malloc:
	return ret;
}

int msm_spm_set_low_power_mode(unsigned int mode, bool notify_rpm)
{
	struct msm_spm_device *dev = &__get_cpu_var(msm_cpu_spm_device);
	return msm_spm_dev_set_low_power_mode(dev, mode, notify_rpm);
}

int __init msm_spm_init(struct msm_spm_platform_data *data, int nr_devs)
{
	unsigned int cpu;
	int ret = 0;

	BUG_ON((nr_devs < num_possible_cpus()) || !data);

	for_each_possible_cpu(cpu) {
		struct msm_spm_device *dev = &per_cpu(msm_cpu_spm_device, cpu);
		ret = msm_spm_dev_init(dev, &data[cpu]);
		if (ret < 0) {
			pr_warn("%s():failed CPU:%u ret:%d\n", __func__,
					cpu, ret);
			break;
		}
	}

	return ret;
}

#if defined(CONFIG_MSM_L2_SPM)
static struct msm_spm_device msm_spm_l2_device;

int msm_spm_l2_set_low_power_mode(unsigned int mode, bool notify_rpm)
{
	return msm_spm_dev_set_low_power_mode(
			&msm_spm_l2_device, mode, notify_rpm);
}

int __init msm_spm_l2_init(struct msm_spm_platform_data *data)
{
	return msm_spm_dev_init(&msm_spm_l2_device, data);
}
#endif
