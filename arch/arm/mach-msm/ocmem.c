/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/genalloc.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <mach/ocmem_priv.h>

/* This code is to temporarily work around the default state of OCMEM
   regions in Virtio. These registers will be read from DT in a subsequent
   patch which initializes the regions to appropriate default state.
*/

#define OCMEM_REGION_CTL_BASE 0xFDD0003C
#define OCMEM_REGION_CTL_SIZE 0xFD0
#define REGION_ENABLE 0x00003333
#define GRAPHICS_REGION_CTL (0x17F000)

struct ocmem_partition {
	const char *name;
	int id;
	unsigned long p_start;
	unsigned long p_size;
	unsigned long p_min;
	unsigned int p_tail;
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
	"other_os",
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
	{ "other_os", OCMEM_OTHER_OS, 0x120000, 0x20000, 0x20000, 0},
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

int check_id(int id)
{
	return (id < OCMEM_CLIENT_MAX && id >= OCMEM_GRAPHICS);
}

const char *get_name(int id)
{
	if (!check_id(id))
		return NULL;
	return client_names[id];
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

int of_ocmem_parse_regions(struct device *dev,
			struct ocmem_partition **part)
{
	const char *name;
	struct device_node *child = NULL;
	int nr_parts = 0;
	int i = 0;
	int rc = 0;
	int id = -1;

	/*Compute total partitions */
	for_each_child_of_node(dev->of_node, child)
		nr_parts++;

	if (nr_parts == 0)
		return 0;

	*part = devm_kzalloc(dev, nr_parts * sizeof(**part),
			GFP_KERNEL);

	if (!*part)
		return -ENOMEM;

	for_each_child_of_node(dev->of_node, child)
	{
		const u32 *addr;
		u32 min;
		u64 size;
		u64 p_start;

		addr = of_get_address(child, 0, &size, NULL);

		if (!addr) {
			dev_err(dev, "Invalid addr for partition %d, ignored\n",
						i);
			continue;
		}

		rc = of_property_read_u32(child, "qcom,ocmem-part-min", &min);

		if (rc) {
			dev_err(dev, "No min for partition %d, ignored\n", i);
			continue;
		}

		rc = of_property_read_string(child, "qcom,ocmem-part-name",
							&name);

		if (rc) {
			dev_err(dev, "No name for partition %d, ignored\n", i);
			continue;
		}

		id = get_id(name);

		if (id < 0) {
			dev_err(dev, "Ignoring invalid partition %s\n", name);
			continue;
		}

		p_start = of_translate_address(child, addr);

		if (p_start == OF_BAD_ADDR) {
			dev_err(dev, "Invalid offset for partition %d\n", i);
			continue;
		}

		(*part)[i].p_start = p_start;
		(*part)[i].p_size = size;
		(*part)[i].id = id;
		(*part)[i].name = name;
		(*part)[i].p_min = min;
		(*part)[i].p_tail = of_property_read_bool(child, "tail");
		i++;
	}

	return i;
}

static struct ocmem_plat_data *parse_dt_config(struct platform_device *pdev)
{
	struct device   *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct ocmem_plat_data *pdata = NULL;
	struct ocmem_partition *parts = NULL;
	struct resource *ocmem_irq;
	struct resource *dm_irq;
	struct resource *ocmem_mem;
	struct resource *reg_base;
	struct resource *br_base;
	struct resource *dm_base;
	struct resource *ocmem_mem_io;
	unsigned nr_parts = 0;
	unsigned nr_regions = 0;

	pdata = devm_kzalloc(dev, sizeof(struct ocmem_plat_data),
			GFP_KERNEL);

	if (!pdata) {
		dev_err(dev, "Unable to allocate memory for platform data\n");
		return NULL;
	}

	ocmem_mem = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"ocmem_physical");
	if (!ocmem_mem) {
		dev_err(dev, "No OCMEM memory resource\n");
		return NULL;
	}

	ocmem_mem_io = request_mem_region(ocmem_mem->start,
				resource_size(ocmem_mem), pdev->name);

	if (!ocmem_mem_io) {
		dev_err(dev, "Could not claim OCMEM memory\n");
		return NULL;
	}

	pdata->base = ocmem_mem->start;
	pdata->size = resource_size(ocmem_mem);
	pdata->vbase = devm_ioremap_nocache(dev, ocmem_mem->start,
						resource_size(ocmem_mem));
	if (!pdata->vbase) {
		dev_err(dev, "Could not ioremap ocmem memory\n");
		return NULL;
	}

	reg_base = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"ocmem_ctrl_physical");
	if (!reg_base) {
		dev_err(dev, "No OCMEM register resource\n");
		return NULL;
	}

	pdata->reg_base = devm_ioremap_nocache(dev, reg_base->start,
				resource_size(reg_base));
	if (!pdata->reg_base) {
		dev_err(dev, "Could not ioremap register map\n");
		return NULL;
	}

	br_base = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"br_ctrl_physical");
	if (!br_base) {
		dev_err(dev, "No OCMEM BR resource\n");
		return NULL;
	}

	pdata->br_base = devm_ioremap_nocache(dev, br_base->start,
				resource_size(br_base));
	if (!pdata->br_base) {
		dev_err(dev, "Could not ioremap BR resource\n");
		return NULL;
	}

	dm_base = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"dm_ctrl_physical");
	if (!dm_base) {
		dev_err(dev, "No OCMEM DM resource\n");
		return NULL;
	}

	pdata->dm_base = devm_ioremap_nocache(dev, dm_base->start,
				resource_size(dm_base));
	if (!pdata->dm_base) {
		dev_err(dev, "Could not ioremap DM resource\n");
		return NULL;
	}

	ocmem_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							"ocmem_irq");

	if (!ocmem_irq) {
		dev_err(dev, "No OCMEM IRQ resource\n");
		return NULL;
	}

	dm_irq = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						"dm_irq");

	if (!dm_irq) {
		dev_err(dev, "No DM IRQ resource\n");
		return NULL;
	}

	if (of_property_read_u32(node, "qcom,ocmem-num-regions",
					&nr_regions)) {
		dev_err(dev, "No OCMEM memory regions specified\n");
	}

	if (nr_regions == 0) {
		dev_err(dev, "No hardware memory regions found\n");
		return NULL;
	}

	/* Figure out the number of partititons */
	nr_parts = of_ocmem_parse_regions(dev, &parts);
	if (nr_parts <= 0) {
		dev_err(dev, "No valid OCMEM partitions found\n");
		goto pdata_error;
	} else
		dev_dbg(dev, "Found %d ocmem partitions\n", nr_parts);

	pdata->nr_parts = nr_parts;
	pdata->parts = parts;
	pdata->nr_regions = nr_regions;
	pdata->ocmem_irq = ocmem_irq->start;
	pdata->dm_irq = dm_irq->start;
	return pdata;
pdata_error:
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

		start = part->p_start;
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
		INIT_LIST_HEAD(&zone->req_list);
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

	dev_dbg(dev, "Total active zones = %d\n", active_zones);
	return 0;
}

static int msm_ocmem_probe(struct platform_device *pdev)
{
	struct device   *dev = &pdev->dev;
	void *ocmem_region_vbase = NULL;

	if (!pdev->dev.of_node) {
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

	dev_info(dev, "OCMEM Virtual addr %p\n", ocmem_pdata->vbase);

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
	/* Enable the ocmem graphics mpU as a workaround in Virtio */
	/* This will be programmed by TZ after TZ support is integrated */
	writel_relaxed(GRAPHICS_REGION_CTL, ocmem_region_vbase + 0xFCC);

	if (ocmem_rdm_init(pdev))
		return -EBUSY;

	dev_dbg(dev, "initialized successfully\n");
	return 0;
}

static int msm_ocmem_remove(struct platform_device *pdev)
{
	return 0;
}

static struct of_device_id msm_ocmem_dt_match[] = {
	{       .compatible = "qcom,msm-ocmem",
	},
	{}
};

static struct platform_driver msm_ocmem_driver = {
	.probe = msm_ocmem_probe,
	.remove = msm_ocmem_remove,
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
