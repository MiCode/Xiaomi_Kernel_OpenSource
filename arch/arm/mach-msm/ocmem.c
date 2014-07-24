/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#define OCMEM_REGION_CTL_BASE 0xFDD0003C
#define OCMEM_REGION_CTL_SIZE 0xFD0
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

/* Must be in sync with enum ocmem_zstat_item */
static const char *zstat_names[NR_OCMEM_ZSTAT_ITEMS] = {
	"Allocation requests",
	"Synchronous allocations",
	"Ranged allocations",
	"Asynchronous allocations",
	"Allocation failures",
	"Allocations grown",
	"Allocations freed",
	"Allocations shrunk",
	"OCMEM maps",
	"Map failures",
	"OCMEM unmaps",
	"Unmap failures",
	"Transfers to OCMEM",
	"Transfers to DDR",
	"Transfer failures",
	"Evictions",
	"Restorations",
	"Dump requests",
	"Dump completed",
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
		return "Unknown";
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

inline int zone_active(int id)
{
	struct ocmem_zone *z = get_zone(id);
	if (z)
		return z->active == true ? 1 : 0;
	else
		return 0;
}

inline void inc_ocmem_stat(struct ocmem_zone *z,
				enum ocmem_zstat_item item)
{
	if (!z)
		return;
	atomic_long_inc(&z->z_stat[item]);
}

inline unsigned long get_ocmem_stat(struct ocmem_zone *z,
				enum ocmem_zstat_item item)
{
	if (!z)
		return 0;
	else
		return atomic_long_read(&z->z_stat[item]);
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

int __devinit of_ocmem_parse_regions(struct device *dev,
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

#if defined(CONFIG_MSM_OCMEM_LOCAL_POWER_CTRL)
static int parse_power_ctrl_config(struct ocmem_plat_data *pdata,
					struct device_node *node)
{
	pdata->rpm_pwr_ctrl = false;
	pdata->rpm_rsc_type = ~0x0;
	return 0;
}
#else
static int parse_power_ctrl_config(struct ocmem_plat_data *pdata,
					struct device_node *node)
{
	unsigned rsc_type = ~0x0;
	pdata->rpm_pwr_ctrl = false;
	if (of_property_read_u32(node, "qcom,resource-type",
					&rsc_type))
		return -EINVAL;
	pdata->rpm_pwr_ctrl = true;
	pdata->rpm_rsc_type = rsc_type;
	return 0;

}
#endif /* CONFIG_MSM_OCMEM_LOCAL_POWER_CTRL */

/* Core Clock Operations */
int ocmem_enable_core_clock(void)
{
	int ret;
	ret = clk_prepare_enable(ocmem_pdata->core_clk);
	if (ret) {
		pr_err("ocmem: Failed to enable core clock\n");
		return ret;
	}
	pr_debug("ocmem: Enabled core clock\n");
	return 0;
}

void ocmem_disable_core_clock(void)
{
	clk_disable_unprepare(ocmem_pdata->core_clk);
	pr_debug("ocmem: Disabled core clock\n");
}

/* Branch Clock Operations */
int ocmem_enable_iface_clock(void)
{
	int ret;

	if (!ocmem_pdata->iface_clk)
		return 0;

	ret = clk_prepare_enable(ocmem_pdata->iface_clk);
	if (ret) {
		pr_err("ocmem: Failed to disable iface clock\n");
		return ret;
	}
	pr_debug("ocmem: Enabled iface clock\n");
	return 0;
}

void ocmem_disable_iface_clock(void)
{
	if (!ocmem_pdata->iface_clk)
		return;

	clk_disable_unprepare(ocmem_pdata->iface_clk);
	pr_debug("ocmem: Disabled iface clock\n");
}

static struct ocmem_plat_data * __devinit parse_dt_config
						(struct platform_device *pdev)
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
	unsigned nr_macros = 0;

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

	if (of_property_read_u32(node, "qcom,ocmem-num-macros",
							 &nr_macros)) {
		dev_err(dev, "No OCMEM macros specified\n");
	}

	if (nr_macros == 0) {
		dev_err(dev, "No hardware macros found\n");
		return NULL;
	}

	/* Figure out the number of partititons */
	nr_parts = of_ocmem_parse_regions(dev, &parts);
	if (nr_parts <= 0) {
		dev_err(dev, "No valid OCMEM partitions found\n");
		goto pdata_error;
	} else
		dev_dbg(dev, "Found %d ocmem partitions\n", nr_parts);

	if (parse_power_ctrl_config(pdata, node)) {
		dev_err(dev, "No OCMEM RPM Resource specified\n");
		return NULL;
	}

	pdata->nr_parts = nr_parts;
	pdata->parts = parts;
	pdata->nr_regions = nr_regions;
	pdata->nr_macros = nr_macros;
	pdata->ocmem_irq = ocmem_irq->start;
	pdata->dm_irq = dm_irq->start;
	return pdata;
pdata_error:
	return NULL;
}

static int ocmem_zones_show(struct seq_file *f, void *dummy)
{
	unsigned i = 0;
	for (i = OCMEM_GRAPHICS; i < OCMEM_CLIENT_MAX; i++) {
		struct ocmem_zone *z = get_zone(i);
		if (z && z->active == true)
			seq_printf(f, "zone %s\t:0x%08lx - 0x%08lx (%4ld KB)\n",
				get_name(z->owner), z->z_start, z->z_end - 1,
				(z->z_end - z->z_start)/SZ_1K);
	}
	return 0;
}

static int ocmem_zones_open(struct inode *inode, struct file *file)
{
	return single_open(file, ocmem_zones_show, inode->i_private);
}

static const struct file_operations zones_show_fops = {
	.open = ocmem_zones_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int ocmem_stats_show(struct seq_file *f, void *dummy)
{
	unsigned i = 0;
	unsigned j = 0;
	for (i = OCMEM_GRAPHICS; i < OCMEM_CLIENT_MAX; i++) {
		struct ocmem_zone *z = get_zone(i);
		if (z && z->active == true) {
			seq_printf(f, "zone %s:\n", get_name(z->owner));
			for (j = 0 ; j < ARRAY_SIZE(zstat_names); j++) {
				seq_printf(f, "\t %s: %lu\n", zstat_names[j],
					get_ocmem_stat(z, j));
			}
		}
	}
	return 0;
}

static int ocmem_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ocmem_stats_show, inode->i_private);
}

static const struct file_operations stats_show_fops = {
	.open = ocmem_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int ocmem_timing_show(struct seq_file *f, void *dummy)
{
	unsigned i = 0;
	for (i = OCMEM_GRAPHICS; i < OCMEM_CLIENT_MAX; i++) {
		struct ocmem_zone *z = get_zone(i);
		if (z && z->active == true)
			seq_printf(f, "zone %s\t: alloc_delay:[max:%d, min:%d, total:%llu,cnt:%lu] free_delay:[max:%d, min:%d, total:%llu, cnt:%lu]\n",
				get_name(z->owner), z->max_alloc_time,
				z->min_alloc_time, z->total_alloc_time,
				get_ocmem_stat(z, 1), z->max_free_time,
				z->min_free_time, z->total_free_time,
				get_ocmem_stat(z, 6));
	}
	return 0;
}

static int ocmem_timing_open(struct inode *inode, struct file *file)
{
	return single_open(file, ocmem_timing_show, inode->i_private);
}

static const struct file_operations timing_show_fops = {
	.open = ocmem_timing_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

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
		zone->active = false;

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
		zone->max_alloc_time = 0;
		zone->min_alloc_time = 0xFFFFFFFF;
		zone->total_alloc_time = 0;
		zone->max_free_time = 0;
		zone->min_free_time = 0xFFFFFFFF;
		zone->total_free_time = 0;

		if (part->p_tail) {
			z_ops->allocate = allocate_tail;
			z_ops->free = free_tail;
		} else {
			z_ops->allocate = allocate_head;
			z_ops->free = free_head;
		}
		/* zap the counters */
		memset(zone->z_stat, 0 , sizeof(zone->z_stat));
		zone->active = true;
		active_zones++;

		if (active_zones == 1)
			pr_info("Physical OCMEM zone layout:\n");

		pr_info(" zone %s\t: 0x%08lx - 0x%08lx (%4ld KB)\n",
				client_names[part->id], zone->z_start,
				zone->z_end - 1, part->p_size/SZ_1K);
	}

	if (!debugfs_create_file("zones", S_IRUGO, pdata->debug_node,
					NULL, &zones_show_fops)) {
		dev_err(dev, "Unable to create debugfs node for zones\n");
		return -EBUSY;
	}

	if (!debugfs_create_file("stats", S_IRUGO, pdata->debug_node,
					NULL, &stats_show_fops)) {
		dev_err(dev, "Unable to create debugfs node for stats\n");
		return -EBUSY;
	}

	if (!debugfs_create_file("timing", S_IRUGO, pdata->debug_node,
					NULL, &timing_show_fops)) {
		dev_err(dev, "Unable to create debugfs node for timing\n");
		return -EBUSY;
	}

	dev_dbg(dev, "Total active zones = %d\n", active_zones);
	return 0;
}

/* Enable the ocmem graphics mpU as a workaround */
#ifdef CONFIG_MSM_OCMEM_NONSECURE
static int ocmem_init_gfx_mpu(struct platform_device *pdev)
{
	int rc;
	struct device *dev = &pdev->dev;
	void __iomem *ocmem_region_vbase = NULL;

	ocmem_region_vbase = devm_ioremap_nocache(dev, OCMEM_REGION_CTL_BASE,
							OCMEM_REGION_CTL_SIZE);
	if (!ocmem_region_vbase)
		return -EBUSY;

	rc = ocmem_enable_core_clock();

	if (rc < 0)
		return rc;

	writel_relaxed(GRAPHICS_REGION_CTL, ocmem_region_vbase + 0xFCC);
	ocmem_disable_core_clock();
	return 0;
}
#else
static int ocmem_init_gfx_mpu(struct platform_device *pdev)
{
	return 0;
}
#endif /* CONFIG_MSM_OCMEM_NONSECURE */

static int __devinit ocmem_debugfs_init(struct platform_device *pdev)
{
	struct dentry *debug_dir = NULL;
	struct ocmem_plat_data *pdata = platform_get_drvdata(pdev);

	debug_dir = debugfs_create_dir("ocmem", NULL);
	if (!debug_dir || IS_ERR(debug_dir)) {
		pr_err("ocmem: Unable to create debugfs root\n");
		return PTR_ERR(debug_dir);
	}

	pdata->debug_node =  debug_dir;
	return 0;
}

static void __devexit ocmem_debugfs_exit(struct platform_device *pdev)
{
	struct ocmem_plat_data *pdata = platform_get_drvdata(pdev);
	debugfs_remove_recursive(pdata->debug_node);
}

static int __devinit msm_ocmem_probe(struct platform_device *pdev)
{
	struct device   *dev = &pdev->dev;
	struct clk *ocmem_core_clk = NULL;
	struct clk *ocmem_iface_clk = NULL;
	int rc;

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

	ocmem_core_clk = devm_clk_get(dev, "core_clk");

	if (IS_ERR(ocmem_core_clk)) {
		dev_err(dev, "Unable to get the core clock\n");
		return PTR_ERR(ocmem_core_clk);
	}

	/* The core clock is synchronous with graphics */
	if (clk_set_rate(ocmem_core_clk, 1000) < 0) {
		dev_err(dev, "Set rate failed on the core clock\n");
		return -EBUSY;
	}

	ocmem_iface_clk = devm_clk_get(dev, "iface_clk");

	if (IS_ERR_OR_NULL(ocmem_iface_clk))
		ocmem_iface_clk = NULL;


	ocmem_pdata->core_clk = ocmem_core_clk;
	ocmem_pdata->iface_clk = ocmem_iface_clk;

	platform_set_drvdata(pdev, ocmem_pdata);

	rc = ocmem_enable_core_clock();
	if (rc < 0)
		goto core_clk_fail;

	rc = ocmem_enable_iface_clock();
	if (rc < 0)
		goto iface_clk_fail;

	/* Parameter to be updated based on TZ */
	/* Allow the OCMEM CSR to be programmed */
	if (ocmem_restore_sec_program(OCMEM_SECURE_DEV_ID))
		return -EBUSY;
	
	ocmem_disable_iface_clock();
	ocmem_disable_core_clock();

	if (ocmem_debugfs_init(pdev))
		dev_err(dev, "ocmem: No debugfs node available\n");

	if (ocmem_core_init(pdev))
		return -EBUSY;

	if (ocmem_zone_init(pdev))
		return -EBUSY;

	if (ocmem_notifier_init())
		return -EBUSY;

	if (ocmem_sched_init(pdev))
		return -EBUSY;

	if (ocmem_rdm_init(pdev))
		return -EBUSY;

	if (ocmem_init_gfx_mpu(pdev)) {
		dev_err(dev, "Unable to initialize Graphics mPU\n");
		return -EBUSY;
	}

	dev_dbg(dev, "initialized successfully\n");
	return 0;

iface_clk_fail:
	ocmem_disable_core_clock();
core_clk_fail:
	pr_err("ocmem: Failed to turn on core clk\n");
	return rc;
}

static int __devexit msm_ocmem_remove(struct platform_device *pdev)
{
	ocmem_debugfs_exit(pdev);
	return 0;
}

static struct of_device_id msm_ocmem_dt_match[] = {
	{       .compatible = "qcom,msm-ocmem",
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
