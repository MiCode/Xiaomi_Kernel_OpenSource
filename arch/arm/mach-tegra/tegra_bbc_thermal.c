/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/slab.h>
#include <linux/err.h>
#include <linux/printk.h>
#include <linux/thermal.h>
#include <linux/nvshm_stats.h>

struct bbc_thermal_private {
	u32 disabled_safe;
	const u32 *enabled_ptr;
	struct thermal_zone_device **tzds;
	int tz_no;
	struct notifier_block nb;
};

static struct bbc_thermal_private private;

static int bbc_get_temp(struct thermal_zone_device *tzd, unsigned long *t)
{
	const u32 *temp = (const u32 *) tzd->devdata;

	/* Check that we thermal is enabled and temperature has been updated */
	if (!*private.enabled_ptr || (*temp > 300))
		return -ENODATA;

	/* °C to m°C */
	*t = *temp * 1000;
	return 0;
}

static const struct thermal_zone_device_ops bbc_thermal_ops = {
	.get_temp = bbc_get_temp,
};

static void bbc_thermal_remove(void)
{
	int i;

	private.enabled_ptr = &private.disabled_safe;
	if (!private.tzds)
		return;

	for (i = 0; i < private.tz_no; i++)
		thermal_zone_device_unregister(private.tzds[i]);

	kfree(private.tzds);
	private.tzds = NULL;
}

static int bbc_thermal_install(void)
{
	struct nvshm_stats_iter it;
	unsigned int index;
	const u32 *enabled_ptr;
	int rc = 0;

	if (private.tzds) {
		pr_warn("BBC thermal already registered, unregistering\n");
		bbc_thermal_remove();
	}

	/* Get iterator for top structure */
	enabled_ptr = nvshm_stats_top("DrvTemperatureSysStats", &it);
	if (IS_ERR(enabled_ptr)) {
		pr_err("BBC thermal zones missing");
		return PTR_ERR(enabled_ptr);
	}

	private.enabled_ptr = enabled_ptr;
	/* Look for array of sensor data structures */
	while (nvshm_stats_type(&it) != NVSHM_STATS_END) {
		if (!strcmp(nvshm_stats_name(&it), "sensorStats"))
			break;

		nvshm_stats_next(&it);
	}

	if (nvshm_stats_type(&it) != NVSHM_STATS_SUB) {
		pr_err("sensorStats not found or incorrect type: %d",
		       nvshm_stats_type(&it));
		return -EINVAL;
	}

	/* Parse sensors */
	private.tz_no = nvshm_stats_elems(&it);
	pr_info("BBC can report temperatures from %d thermal zones",
		private.tz_no);
	private.tzds = kmalloc(private.tz_no * sizeof(*private.tzds),
			       GFP_KERNEL);
	if (!private.tzds) {
		pr_err("failed to allocate array of sensors\n");
		return -ENOMEM;
	}

	for (index = 0; index < private.tz_no; index++) {
		struct nvshm_stats_iter sub_it;
		char name[16];

		/* Get iterator to sensor data structure */
		nvshm_stats_sub(&it, index, &sub_it);
		/* We only care about temperature */
		while (nvshm_stats_type(&sub_it) != NVSHM_STATS_END) {
			if (!strcmp(nvshm_stats_name(&sub_it), "tempCelcius"))
				break;

			nvshm_stats_next(&sub_it);
		}

		/* This will either fail at first time or not at all */
		if (nvshm_stats_type(&sub_it) != NVSHM_STATS_UINT32) {
			pr_err("tempCelcius not found or incorrect type: %d",
			       nvshm_stats_type(&sub_it));
			kfree(private.tzds);
			private.tzds = NULL;
			return -EINVAL;
		}

		/* Ok we got it, let's register a new thermal zone */
		sprintf(name, "BBC-therm%d", index);
		private.tzds[index] = thermal_zone_device_register(name, 0, 0,
			(void *) nvshm_stats_valueptr_uint32(&sub_it, 0),
			&bbc_thermal_ops, NULL, 0, 0);
		if (IS_ERR(private.tzds)) {
			pr_err("failed to register thermal zone #%d, abort\n",
			       index);
			rc = PTR_ERR(private.tzds);
			break;
		}
	}

	if (rc)
		bbc_thermal_remove();

	return rc;
}

static int bbc_thermal_notify(struct notifier_block *self,
			       unsigned long action,
			       void *user)
{
	switch (action) {
	case NVSHM_STATS_MODEM_UP:
		bbc_thermal_install();
		break;
	case NVSHM_STATS_MODEM_DOWN:
		bbc_thermal_remove();
		break;
	}
	return NOTIFY_OK;
}

void tegra_bbc_thermal_init(void)
{
	private.enabled_ptr = &private.disabled_safe;
	private.nb.notifier_call = bbc_thermal_notify;
	nvshm_stats_register(&private.nb);
}
