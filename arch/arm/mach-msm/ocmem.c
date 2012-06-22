/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <mach/ocmem_priv.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/genalloc.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

/* This code is to temporarily work around the default state of OCMEM
   regions in Virtio. These registers will be read from DT in a subsequent
   patch which initializes the regions to appropriate default state.
*/

#define OCMEM_REGION_CTL_BASE 0xFDD0003C
#define OCMEM_REGION_CTL_SIZE 0xC
#define REGION_ENABLE 0x00003333

struct ocmem_partition {
	const char *name;
	int id;
	unsigned long p_start;
	unsigned long p_size;
	unsigned long p_min;
	unsigned int p_tail;
};

struct ocmem_plat_data {
	void __iomem *vbase;
	unsigned long size;
	unsigned long base;
	struct ocmem_partition *parts;
	unsigned nr_parts;
};

struct ocmem_zone zones[OCMEM_CLIENT_MAX];

struct ocmem_zone *get_zone(unsigned id)
{
	if (id < OCMEM_GRAPHICS || id >= OCMEM_CLIENT_MAX)
		return NULL;
	else
		return &zones[id];
}

static struct ocmem_plat_data *ocmem_pdata;

#define CLIENT_NAME_MAX 10
/* Must be in sync with enum ocmem_client */
static const char *client_names[OCMEM_CLIENT_MAX] = {
	"graphics",
	"video",
	"camera",
	"hp_audio",
	"voice",
	"lp_audio",
	"sensors",
	"blast",
};

struct ocmem_quota_table {
	const char *name;
	int id;
	unsigned long start;
	unsigned long size;
	unsigned long min;
	unsigned int tail;
};

/* This static table will go away with device tree support */
static struct ocmem_quota_table qt[OCMEM_CLIENT_MAX] = {
	/* name,        id,     start,  size,   min, tail */
	{ "graphics", OCMEM_GRAPHICS, 0x0, 0x100000, 0x80000, 0},
	{ "video", OCMEM_VIDEO, 0x100000, 0x80000, 0x55000, 1},
	{ "camera", OCMEM_CAMERA, 0x0, 0x0, 0x0, 0},
	{ "voice", OCMEM_VOICE,  0x0, 0x0, 0x0, 0 },
	{ "hp_audio", OCMEM_HP_AUDIO, 0x0, 0x0, 0x0, 0},
	{ "lp_audio", OCMEM_LP_AUDIO, 0x80000, 0xA0000, 0xA0000, 0},
	{ "blast", OCMEM_BLAST, 0x120000, 0x20000, 0x20000, 0},
	{ "sensors", OCMEM_SENSORS, 0x140000, 0x40000, 0x40000, 0},
};

static inline int get_id(const char *name)
{
	int i = 0;
	for (i = 0 ; i < OCMEM_CLIENT_MAX; i++) {
		if (strncmp(name, client_names[i], CLIENT_NAME_MAX) == 0)
			return i;
	}
	return -EINVAL;
}

inline unsigned long phys_to_offset(unsigned long addr)
{
	if (!ocmem_pdata)
		return 0;
	if (addr < ocmem_pdata->base ||
		addr > (ocmem_pdata->base + ocmem_pdata->size))
		return 0;
	return addr - ocmem_pdata->base;
}

inline unsigned long offset_to_phys(unsigned long offset)
{
	if (!ocmem_pdata)
		return 0;
	if (offset > ocmem_pdata->size)
		return 0;
	return offset + ocmem_pdata->base;
}

static struct ocmem_plat_data *parse_static_config(struct platform_device *pdev)
{
	struct ocmem_plat_data *pdata = NULL;
	struct ocmem_partition *parts = NULL;
	struct device   *dev = &pdev->dev;
	unsigned nr_parts = 0;
	int i;
	int j;

	pdata = devm_kzalloc(dev, sizeof(struct ocmem_plat_data),
			GFP_KERNEL);

	if (!pdata) {
		dev_err(dev, "Unable to allocate memory for"
			" platform data\n");
		return NULL;
	}

	for (i = 0 ; i < ARRAY_SIZE(qt); i++)
		if (qt[i].size != 0x0)
			nr_parts++;

	if (nr_parts == 0x0) {
		dev_err(dev, "No valid ocmem partitions\n");
		return NULL;
	} else
		dev_info(dev, "Total partitions = %d\n", nr_parts);

	parts = devm_kzalloc(dev, sizeof(struct ocmem_partition) * nr_parts,
				 GFP_KERNEL);

	if (!parts) {
		dev_err(dev, "Unable to allocate memory for"
			" partition data\n");
		return NULL;
	}

	for (i = 0, j = 0; i < ARRAY_SIZE(qt); i++) {
		if (qt[i].size == 0x0) {
			dev_dbg(dev, "Skipping creation of pool for %s\n",
						qt[i].name);
			continue;
		}
		parts[j].id = qt[i].id;
		parts[j].p_size = qt[i].size;
		parts[j].p_start = qt[i].start;
		parts[j].p_min = qt[i].min;
		parts[j].p_tail = qt[i].tail;
		j++;
	}
	BUG_ON(j != nr_parts);
	pdata->nr_parts = nr_parts;
	pdata->parts = parts;
	pdata->base = OCMEM_PHYS_BASE;
	pdata->size = OCMEM_PHYS_SIZE;
	return pdata;
}

static struct ocmem_plat_data *parse_dt_config(struct platform_device *pdev)
{
	return NULL;
}

static int ocmem_zone_init(struct platform_device *pdev)
{

	int ret = -1;
	int i = 0;
	unsigned active_zones = 0;

	struct ocmem_zone *zone = NULL;
	struct ocmem_zone_ops *z_ops = NULL;
	struct device   *dev = &pdev->dev;
	unsigned long start;
	struct ocmem_plat_data *pdata = NULL;

	pdata = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->nr_parts; i++) {
		struct ocmem_partition *part = &pdata->parts[i];
		zone = get_zone(part->id);

		dev_dbg(dev, "Partition %d, start %lx, size %lx for %s\n",
				i, part->p_start, part->p_size,
				client_names[part->id]);

		if (part->p_size > pdata->size) {
			dev_alert(dev, "Quota > ocmem_size for id:%d\n",
					part->id);
			continue;
		}

		zone->z_pool = gen_pool_create(PAGE_SHIFT, -1);

		if (!zone->z_pool) {
			dev_alert(dev, "Creating pool failed for id:%d\n",
					part->id);
			return -EBUSY;
		}

		start = pdata->base + part->p_start;
		ret = gen_pool_add(zone->z_pool, start,
					part->p_size, -1);

		if (ret < 0) {
			gen_pool_destroy(zone->z_pool);
			dev_alert(dev, "Unable to back pool %d with "
				"buffer:%lx\n", part->id, part->p_size);
			return -EBUSY;
		}

		/* Initialize zone allocators */
		z_ops = devm_kzalloc(dev, sizeof(struct ocmem_zone_ops),
				GFP_KERNEL);
		if (!z_ops) {
			pr_alert("ocmem: Unable to allocate memory for"
					"zone ops:%d\n", i);
			return -EBUSY;
		}

		/* Initialize zone parameters */
		zone->z_start = start;
		zone->z_head = zone->z_start;
		zone->z_end = start + part->p_size;
		zone->z_tail = zone->z_end;
		zone->z_free = part->p_size;
		zone->owner = part->id;
		zone->active_regions = 0;
		zone->max_regions = 0;
		INIT_LIST_HEAD(&zone->region_list);
		zone->z_ops = z_ops;
		if (part->p_tail) {
			z_ops->allocate = allocate_tail;
			z_ops->free = free_tail;
		} else {
			z_ops->allocate = allocate_head;
			z_ops->free = free_head;
		}
		active_zones++;

		if (active_zones == 1)
			pr_info("Physical OCMEM zone layout:\n");

		pr_info(" zone %s\t: 0x%08lx - 0x%08lx (%4ld KB)\n",
				client_names[part->id], zone->z_start,
				zone->z_end, part->p_size/SZ_1K);
	}

	dev_info(dev, "Total active zones = %d\n", active_zones);
	return 0;
}

static int __devinit msm_ocmem_probe(struct platform_device *pdev)
{
	struct device   *dev = &pdev->dev;
	void *ocmem_region_vbase = NULL;

	if (!pdev->dev.of_node->child) {
		dev_info(dev, "Missing Configuration in Device Tree\n");
		ocmem_pdata = parse_static_config(pdev);
	} else {
		ocmem_pdata = parse_dt_config(pdev);
	}

	/* Check if we have some configuration data to start */
	if (!ocmem_pdata)
		return -ENODEV;

	/* Sanity Checks */
	BUG_ON(!IS_ALIGNED(ocmem_pdata->size, PAGE_SIZE));
	BUG_ON(!IS_ALIGNED(ocmem_pdata->base, PAGE_SIZE));

	platform_set_drvdata(pdev, ocmem_pdata);

	if (ocmem_zone_init(pdev))
		return -EBUSY;

	if (ocmem_notifier_init())
		return -EBUSY;

	if (ocmem_sched_init())
		return -EBUSY;

	ocmem_region_vbase = devm_ioremap_nocache(dev, OCMEM_REGION_CTL_BASE,
							OCMEM_REGION_CTL_SIZE);
	if (!ocmem_region_vbase)
		return -EBUSY;
	/* Enable all the 3 regions until we have support for power features */
	writel_relaxed(REGION_ENABLE, ocmem_region_vbase);
	writel_relaxed(REGION_ENABLE, ocmem_region_vbase + 4);
	writel_relaxed(REGION_ENABLE, ocmem_region_vbase + 8);
	dev_info(dev, "initialized successfully\n");
	return 0;
}

static int __devexit msm_ocmem_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id msm_ocmem_dt_match[] = {
	{       .compatible = "qcom,msm_ocmem",
	},
	{}
};

static struct platform_driver msm_ocmem_driver = {
	.probe = msm_ocmem_probe,
	.remove = __devexit_p(msm_ocmem_remove),
	.driver = {
		.name = "msm_ocmem",
		.owner = THIS_MODULE,
		.of_match_table = msm_ocmem_dt_match,
	},
};

static int __init ocmem_init(void)
{
	return platform_driver_register(&msm_ocmem_driver);
}
subsys_initcall(ocmem_init);

static void __exit ocmem_exit(void)
{
	platform_driver_unregister(&msm_ocmem_driver);
}
module_exit(ocmem_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Support for On-Chip Memory on MSM");
