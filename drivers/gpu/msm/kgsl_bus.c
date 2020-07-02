// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/interconnect.h>
#include <linux/of.h>

#include "kgsl_bus.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"

static int interconnect_bus_set(struct kgsl_device *device, int level,
		u32 ab)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if ((level == pwr->cur_buslevel) && (ab == pwr->cur_ab))
		return 0;

	pwr->cur_buslevel = level;
	pwr->cur_ab = ab;

	icc_set_bw(pwr->icc_path, MBps_to_icc(ab),
		kBps_to_icc(pwr->ddr_table[level]));

	trace_kgsl_buslevel(device, pwr->active_pwrlevel, level);

	return 0;
}

static u32 _ab_buslevel_update(struct kgsl_pwrctrl *pwr,
		u32 ib)
{
	if (!ib)
		return 0;

	/* In the absence of any other settings, make ab 25% of ib */
	if ((!pwr->bus_percent_ab) && (!pwr->bus_ab_mbytes))
		return 25 * ib / 100;

	if (pwr->bus_width)
		return pwr->bus_ab_mbytes;

	return (pwr->bus_percent_ab * pwr->bus_max) / 100;
}


void kgsl_bus_update(struct kgsl_device *device, bool on)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	/* FIXME: this might be wrong? */
	int cur = pwr->pwrlevels[pwr->active_pwrlevel].bus_freq;
	int buslevel = 0;
	u32 ab;

	/* the bus should be ON to update the active frequency */
	if (on && !(test_bit(KGSL_PWRFLAGS_AXI_ON, &pwr->power_flags)))
		return;
	/*
	 * If the bus should remain on calculate our request and submit it,
	 * otherwise request bus level 0, off.
	 */
	if (on) {
		buslevel = min_t(int, pwr->pwrlevels[0].bus_max,
				cur + pwr->bus_mod);
		buslevel = max_t(int, buslevel, 1);
	} else {
		/* If the bus is being turned off, reset to default level */
		pwr->bus_mod = 0;
		pwr->bus_percent_ab = 0;
		pwr->bus_ab_mbytes = 0;
	}

	/* buslevel is the IB vote, update the AB */
	ab = _ab_buslevel_update(pwr, pwr->ddr_table[buslevel]);

	pwr->bus_set(device, buslevel, ab);
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

	levels = devm_kcalloc(&pdev->dev, num, sizeof(*levels), GFP_KERNEL);
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

	pwr->icc_path = of_icc_get(&pdev->dev, NULL);
	if (IS_ERR(pwr->icc_path) && !gmu_core_scales_bandwidth(device)) {
		WARN(1, "The CPU has no way to set the GPU bus levels\n");
		return PTR_ERR(pwr->icc_path);
	}

	pwr->bus_set = interconnect_bus_set;

	return 0;
}

void kgsl_bus_close(struct kgsl_device *device)
{
	icc_put(device->pwrctrl.icc_path);
}
