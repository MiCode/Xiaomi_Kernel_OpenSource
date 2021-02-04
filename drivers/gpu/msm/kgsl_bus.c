// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/interconnect.h>
#include <linux/of.h>
#include <linux/devfreq.h>

#include "../../devfreq/governor.h"
#include "kgsl_bus.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"


static u32 _ab_buslevel_update(struct kgsl_pwrctrl *pwr,
		u32 ib)
{
	if (!ib)
		return 0;

	/*
	 * In the absence of any other settings, make ab 25% of ib
	 * where the ib vote is in kbps
	 */
	if ((!pwr->bus_percent_ab) && (!pwr->bus_ab_mbytes))
		return 25 * ib / 100000;

	if (pwr->bus_width)
		return pwr->bus_ab_mbytes;

	return (pwr->bus_percent_ab * pwr->bus_max) / 100;
}

static void set_ddr_qos(struct kgsl_device *device, int buslevel)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct devfreq *dev = pwr->ddr_qos_devfreq;
	static unsigned long cur_min_freq;
	unsigned long new_min_freq = DEVFREQ_MIN_FREQ;
	int ret;

	if (!dev)
		return;

	/*
	 * Instead of creating a new dummy governor, we are using powersave
	 * governor for this devfreq device. Therefore, modify the minimum
	 * frequency to point to the desired QOS level.
	 */
	if (buslevel == pwr->pwrlevels[0].bus_max)
		new_min_freq = DEVFREQ_MAX_FREQ;

	if (new_min_freq == cur_min_freq)
		return;

	/*
	 * We need the event lock to protect against concurrent governor
	 * re-assignments.
	 */
	event_mutex_lock(dev);
	mutex_lock(&dev->lock);
	/*
	 * Update both min/max to make sure correct vote is set regardless
	 * of the governor, which can be changed from sysfs
	 */
	dev->min_freq = new_min_freq;
	dev->max_freq = new_min_freq;
	ret = update_devfreq(dev);
	mutex_unlock(&dev->lock);
	event_mutex_unlock(dev);

	if (!ret)
		cur_min_freq = new_min_freq;
}

int kgsl_bus_update(struct kgsl_device *device,
			 enum kgsl_bus_vote vote_state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	/* FIXME: this might be wrong? */
	int cur = pwr->pwrlevels[pwr->active_pwrlevel].bus_freq;
	int ret, buslevel = 0;
	u32 ab;

	/* the bus should be ON to update the active frequency */
	if ((vote_state != KGSL_BUS_VOTE_OFF) &&
		!(test_bit(KGSL_PWRFLAGS_AXI_ON, &pwr->power_flags)))
		return 0;
	/*
	 * If the bus should remain on calculate our request and submit it,
	 * otherwise request bus level 0, off.
	 */
	if (vote_state == KGSL_BUS_VOTE_ON) {
		buslevel = min_t(int, pwr->pwrlevels[0].bus_max,
				cur + pwr->bus_mod);
		buslevel = max_t(int, buslevel, 1);
	} else if (vote_state == KGSL_BUS_VOTE_MINIMUM) {
		/* Request bus level 1, minimum non-zero value */
		buslevel = 1;
		pwr->bus_mod = 0;
		pwr->bus_percent_ab = 0;
		pwr->bus_ab_mbytes = 0;
	} else if (vote_state == KGSL_BUS_VOTE_OFF) {
		/* If the bus is being turned off, reset to default level */
		pwr->bus_mod = 0;
		pwr->bus_percent_ab = 0;
		pwr->bus_ab_mbytes = 0;
	}

	/* buslevel is the IB vote, update the AB */
	ab = _ab_buslevel_update(pwr, pwr->ddr_table[buslevel]);

	ret = device->ftbl->gpu_bus_set(device, buslevel, ab);
	if (ret)
		return ret;

	set_ddr_qos(device, buslevel);

	return 0;
}

static void validate_pwrlevels(struct kgsl_device *device, u32 *ibs,
		int count)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i;

	for (i = 0; i < pwr->num_pwrlevels - 1; i++) {
		struct kgsl_pwrlevel *pwrlevel = &pwr->pwrlevels[i];

		if (pwrlevel->bus_freq >= count) {
			dev_err(device->dev, "Bus setting for GPU freq %d is out of bounds\n",
				pwrlevel->gpu_freq);
			pwrlevel->bus_freq = count - 1;
		}

		if (pwrlevel->bus_max >= count) {
			dev_err(device->dev, "Bus max for GPU freq %d is out of bounds\n",
				pwrlevel->gpu_freq);
			pwrlevel->bus_max = count - 1;
		}

		if (pwrlevel->bus_min >= count) {
			dev_err(device->dev, "Bus min for GPU freq %d is out of bounds\n",
				pwrlevel->gpu_freq);
			pwrlevel->bus_min = count - 1;
		}

		if (pwrlevel->bus_min > pwrlevel->bus_max) {
			dev_err(device->dev, "Bus min is bigger than bus max for GPU freq %d\n",
				pwrlevel->gpu_freq);
			pwrlevel->bus_min = pwrlevel->bus_max;
		}
	}
}

u32 *kgsl_bus_get_table(struct platform_device *pdev,
		const char *name, int *count)
{
	u32 *levels;
	int i, num = of_property_count_elems_of_size(pdev->dev.of_node,
		name, sizeof(u32));

	/* If the bus wasn't specified, then build a static table */
	if (num <= 0)
		return ERR_PTR(-EINVAL);

	levels = kcalloc(num, sizeof(*levels), GFP_KERNEL);
	if (!levels)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < num; i++)
		of_property_read_u32_index(pdev->dev.of_node,
			name, i, &levels[i]);

	*count = num;
	return levels;
}

int kgsl_bus_init(struct kgsl_device *device, struct platform_device *pdev)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int count;
	int ddr = of_fdt_get_ddrtype();

	if (ddr >= 0) {
		char str[32];

		snprintf(str, sizeof(str), "qcom,bus-table-ddr%d", ddr);

		pwr->ddr_table = kgsl_bus_get_table(pdev, str, &count);
		if (!IS_ERR(pwr->ddr_table))
			goto done;
	}

	/* Look if a generic table is present */
	pwr->ddr_table = kgsl_bus_get_table(pdev, "qcom,bus-table-ddr", &count);
	if (IS_ERR(pwr->ddr_table)) {
		int ret = PTR_ERR(pwr->ddr_table);

		pwr->ddr_table = NULL;
		return ret;
	}
done:
	pwr->ddr_table_count = count;

	validate_pwrlevels(device, pwr->ddr_table, pwr->ddr_table_count);

	pwr->icc_path = of_icc_get(&pdev->dev, "gpu_icc_path");
	if (IS_ERR(pwr->icc_path) && !gmu_core_scales_bandwidth(device)) {
		WARN(1, "The CPU has no way to set the GPU bus levels\n");

		kfree(pwr->ddr_table);
		pwr->ddr_table = NULL;
		return PTR_ERR(pwr->icc_path);
	}

	return 0;
}

void kgsl_bus_close(struct kgsl_device *device)
{
	kfree(device->pwrctrl.ddr_table);
	device->pwrctrl.ddr_table = NULL;
	icc_put(device->pwrctrl.icc_path);
	if (device->pwrctrl.ddr_qos_devfreq)
		put_device(&device->pwrctrl.ddr_qos_devfreq->dev);
}
