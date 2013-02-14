/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/thermal.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <mach/msm_memtypes.h>

#define POP_MEM_LPDDR1_REFRESH_MASK	0x00000700
#define POP_MEM_LPDDR1_REFRESH_SHIFT	0x8

#define POP_MEM_LPDDR2_REFRESH_MASK	0x00000007
#define POP_MEM_LPDDR2_REFRESH_SHIFT	0x0

#define POP_MEM_REFRESH_REG		0x3C

#define POP_MEM_LOW_TEMPERATURE		25000
#define POP_MEM_NORMAL_TEMPERATURE	50000
#define POP_MEM_HIGH_TEMPERATURE	85000

#define POP_MEM_TRIP_OUT_OF_SPEC	0
#define POP_MEM_TRIP_NUM		1

struct pop_mem_tm_device {
	unsigned long			baseaddr;
	struct thermal_zone_device	*tz_dev;
	unsigned long			refresh_mask;
	unsigned int			refresh_shift;
};


static int pop_mem_tm_read_refresh(struct pop_mem_tm_device *tm,
				   unsigned int *ref_rate){
	unsigned int ref;

	ref = __raw_readl(tm->baseaddr + POP_MEM_REFRESH_REG);
	*ref_rate = (ref & tm->refresh_mask) >> tm->refresh_shift;

	return 0;
}


static int pop_mem_tm_get_temperature(struct thermal_zone_device *thermal,
			       unsigned long *temperature)
{
	struct pop_mem_tm_device *tm = thermal->devdata;
	unsigned int ref_rate;
	int rc;

	if (!tm || !temperature)
		return -EINVAL;

	rc = pop_mem_tm_read_refresh(tm, &ref_rate);
	if (rc < 0)
		return rc;

	switch (ref_rate) {
	case 0:
	case 1:
	case 2:
		*temperature = POP_MEM_LOW_TEMPERATURE;
		break;
	case 3:
	case 4:
		*temperature = POP_MEM_NORMAL_TEMPERATURE;
		break;
	case 5:
	case 6:
	case 7:
		*temperature = POP_MEM_HIGH_TEMPERATURE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int pop_mem_tm_get_trip_type(struct thermal_zone_device *thermal,
				    int trip, enum thermal_trip_type *type)
{
	struct pop_mem_tm_device *tm = thermal->devdata;

	if (!tm || trip < 0 || !type)
		return -EINVAL;

	if (trip == POP_MEM_TRIP_OUT_OF_SPEC)
		*type = THERMAL_TRIP_CRITICAL;
	else
		return -EINVAL;

	return 0;
}

static int pop_mem_tm_get_trip_temperature(struct thermal_zone_device *thermal,
				    int trip, unsigned long *temperature)
{
	struct pop_mem_tm_device *tm = thermal->devdata;

	if (!tm || trip < 0 || !temperature)
		return -EINVAL;

	if (trip == POP_MEM_TRIP_OUT_OF_SPEC)
		*temperature = POP_MEM_HIGH_TEMPERATURE;
	else
		return -EINVAL;

	return 0;
}


static int pop_mem_tm_get_crit_temperature(struct thermal_zone_device *thermal,
				    unsigned long *temperature)
{
	struct pop_mem_tm_device *tm = thermal->devdata;

	if (!tm || !temperature)
		return -EINVAL;

	*temperature = POP_MEM_HIGH_TEMPERATURE;

	return 0;
}


static struct thermal_zone_device_ops pop_mem_thermal_zone_ops = {
	.get_temp = pop_mem_tm_get_temperature,
	.get_trip_type = pop_mem_tm_get_trip_type,
	.get_trip_temp = pop_mem_tm_get_trip_temperature,
	.get_crit_temp = pop_mem_tm_get_crit_temperature,
};


static int __devinit pop_mem_tm_probe(struct platform_device *pdev)
{
	int rc, len, numcontrollers;
	struct resource *controller_mem = NULL;
	struct resource *res_mem = NULL;
	struct pop_mem_tm_device *tmdev = NULL;
	void __iomem *base = NULL;

	rc = len = 0;
	numcontrollers = get_num_populated_chipselects();

	if (pdev->id >= numcontrollers) {
		pr_err("%s: memory controller %d does not exist", __func__,
			pdev->id);
		rc = -ENODEV;
		goto fail;
	}

	controller_mem = platform_get_resource_byname(pdev,
						  IORESOURCE_MEM, "physbase");
	if (!controller_mem) {
		pr_err("%s: could not get resources for controller %d",
			__func__, pdev->id);
		rc = -EFAULT;
		goto fail;
	}

	len = controller_mem->end - controller_mem->start + 1;

	res_mem = request_mem_region(controller_mem->start, len,
				     controller_mem->name);
	if (!res_mem) {
		pr_err("%s: Could not request memory region: "
			"start=%p, len=%d\n", __func__,
			(void *) controller_mem->start, len);
		rc = -EBUSY;
		goto fail;

	}

	base = ioremap(res_mem->start, len);
	if (!base) {
		pr_err("%s: Could not ioremap: start=%p, len=%d\n",
			 __func__, (void *) controller_mem->start, len);
		rc = -EBUSY;
		goto fail;

	}

	tmdev = kzalloc(sizeof(*tmdev), GFP_KERNEL);
	if (tmdev == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		rc = -ENOMEM;
		goto fail;
	}

	if (numcontrollers == 1) {
		tmdev->refresh_mask = POP_MEM_LPDDR1_REFRESH_MASK;
		tmdev->refresh_shift = POP_MEM_LPDDR1_REFRESH_SHIFT;
	} else {
		tmdev->refresh_mask = POP_MEM_LPDDR2_REFRESH_MASK;
		tmdev->refresh_shift = POP_MEM_LPDDR2_REFRESH_SHIFT;
	}
	tmdev->baseaddr = (unsigned long) base;
	tmdev->tz_dev = thermal_zone_device_register("msm_popmem_tz",
						     POP_MEM_TRIP_NUM, tmdev,
						     &pop_mem_thermal_zone_ops,
						     0, 0, 0, 0);

	if (tmdev->tz_dev == NULL) {
		pr_err("%s: thermal_zone_device_register() failed.\n",
			__func__);
		goto fail;
	}

	platform_set_drvdata(pdev, tmdev);

	pr_notice("%s: device %d probed successfully\n", __func__, pdev->id);

	return rc;

fail:
	if (base)
		iounmap(base);
	if (res_mem)
		release_mem_region(controller_mem->start, len);
	kfree(tmdev);

	return rc;
}

static int __devexit pop_mem_tm_remove(struct platform_device *pdev)
{

	int len;
	struct pop_mem_tm_device *tmdev = platform_get_drvdata(pdev);
	struct resource *controller_mem;

	iounmap((void __iomem *)tmdev->baseaddr);

	controller_mem = platform_get_resource_byname(pdev,
						  IORESOURCE_MEM, "physbase");
	len = controller_mem->end - controller_mem->start + 1;
	release_mem_region(controller_mem->start, len);

	thermal_zone_device_unregister(tmdev->tz_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(tmdev);

	return 0;
}

static struct platform_driver pop_mem_tm_driver = {
	.probe          = pop_mem_tm_probe,
	.remove         = pop_mem_tm_remove,
	.driver         = {
		.name = "msm_popmem-tm",
		.owner = THIS_MODULE
	},
};

static int __init pop_mem_tm_init(void)
{
	return platform_driver_register(&pop_mem_tm_driver);
}

static void __exit pop_mem_tm_exit(void)
{
	platform_driver_unregister(&pop_mem_tm_driver);
}

module_init(pop_mem_tm_init);
module_exit(pop_mem_tm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Pop memory thermal manager driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:popmem-tm");
