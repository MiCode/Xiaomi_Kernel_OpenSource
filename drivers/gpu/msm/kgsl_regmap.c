// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/io.h>
#include <linux/platform_device.h>

#include "kgsl_regmap.h"
#include "kgsl_trace.h"

#define region_addr(region, _offset) \
	((region)->virt + (((_offset) - (region)->offset) << 2))

static int kgsl_regmap_init_region(struct kgsl_regmap *regmap,
		struct platform_device *pdev,
		struct kgsl_regmap_region *region,
		struct resource *res, const struct kgsl_regmap_ops *ops,
		void *priv)
{
	void __iomem *ptr;

	ptr = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!ptr)
		return -ENOMEM;

	region->virt = ptr;
	region->offset = (res->start - regmap->base->start) >> 2;
	region->size = resource_size(res) >> 2;
	region->ops = ops;
	region->priv = priv;

	return 0;
}

/* Initialize the regmap with the base region.  All added regions will be offset
 * from this base
 */
int kgsl_regmap_init(struct platform_device *pdev, struct kgsl_regmap *regmap,
		const char *name, const struct kgsl_regmap_ops *ops,
		void *priv)
{
	struct kgsl_regmap_region *region;
	struct resource *res;
	int ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (!res)
		return -ENODEV;

	regmap->base = res;

	region = &regmap->region[0];
	ret = kgsl_regmap_init_region(regmap, pdev, region, res, ops, priv);

	if (!ret)
		regmap->count = 1;

	return ret;
}

/* Add a new region to the regmap */
int kgsl_regmap_add_region(struct kgsl_regmap *regmap, struct platform_device *pdev,
		const char *name, const struct kgsl_regmap_ops *ops, void *priv)
{
	struct kgsl_regmap_region *region;
	struct resource *res;
	int ret;

	if (WARN_ON(regmap->count >= ARRAY_SIZE(regmap->region)))
		return -ENODEV;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (!res)
		return -ENODEV;

	region = &regmap->region[regmap->count];

	ret = kgsl_regmap_init_region(regmap, pdev, region, res, ops, priv);
	if (!ret)
		regmap->count++;

	return ret;
}

#define in_range(a, base, len) \
	(((a) >= (base)) && ((a) < ((base) + (len))))

struct kgsl_regmap_region *kgsl_regmap_get_region(struct kgsl_regmap *regmap,
		u32 offset)
{
	int i;

	for (i = 0; i < regmap->count; i++) {
		struct kgsl_regmap_region *region = &regmap->region[i];

		if (in_range(offset, region->offset, region->size))
			return region;
	}

	return NULL;
}

u32 kgsl_regmap_read(struct kgsl_regmap *regmap, u32 offset)
{
	struct kgsl_regmap_region *region = kgsl_regmap_get_region(regmap, offset);
	u32 val;

	if (WARN(!region, "Out of bounds register read offset: 0x%x\n", offset))
		return 0;

	if (region->ops && region->ops->preaccess)
		region->ops->preaccess(region);

	val = readl_relaxed(region_addr(region, offset));
	/* Allow previous read to post before returning the value */
	rmb();

	return val;
}

void kgsl_regmap_write(struct kgsl_regmap *regmap, u32 value, u32 offset)
{
	struct kgsl_regmap_region *region = kgsl_regmap_get_region(regmap, offset);

	if (WARN(!region, "Out of bounds register write offset: 0x%x\n", offset))
		return;

	if (region->ops && region->ops->preaccess)
		region->ops->preaccess(region);

	/* Make sure all pending writes have posted first */
	wmb();
	writel_relaxed(value, region_addr(region, offset));

	trace_kgsl_regwrite(offset, value);
}

void kgsl_regmap_multi_write(struct kgsl_regmap *regmap,
		const struct kgsl_regmap_list *list, int count)
{
	struct kgsl_regmap_region *region, *prev = NULL;
	int i;

	/*
	 * do one write barrier to ensure all previous writes are done before
	 * starting the list
	 */
	wmb();

	for (i = 0; i < count; i++) {
		region = kgsl_regmap_get_region(regmap, list[i].offset);

		if (WARN(!region, "Out of bounds register write offset: 0x%x\n",
			list[i].offset))
			continue;

		/*
		 * The registers might be in different regions. If a region has
		 * a preaccess function we need to call it at least once before
		 * writing registers but we don't want to call it every time if
		 * we can avoid it. "cache" the current region and don't call
		 * pre-access if it is the same region from the previous access.
		 * This isn't perfect but it should cut down on some unneeded
		 * cpu cycles
		 */

		if (region != prev && region->ops && region->ops->preaccess)
			region->ops->preaccess(region);

		prev = region;

		writel_relaxed(list[i].val, region_addr(region, list[i].offset));
		trace_kgsl_regwrite(list[i].val, list[i].offset);
	}
}

void kgsl_regmap_rmw(struct kgsl_regmap *regmap, u32 offset, u32 mask,
		u32 or)
{
	struct kgsl_regmap_region *region = kgsl_regmap_get_region(regmap, offset);
	u32 val;

	if (WARN(!region, "Out of bounds register read-modify-write offset: 0x%x\n",
		offset))
		return;

	if (region->ops && region->ops->preaccess)
		region->ops->preaccess(region);

	val = readl_relaxed(region_addr(region, offset));
	/* Make sure the read posted and all pending writes are done */
	mb();
	writel_relaxed((val & ~mask) | or, region_addr(region, offset));

	trace_kgsl_regwrite(offset, (val & ~mask) | or);
}

void kgsl_regmap_bulk_write(struct kgsl_regmap *regmap, u32 offset,
		const void *data, int dwords)
{
	struct kgsl_regmap_region *region = kgsl_regmap_get_region(regmap, offset);

	if (WARN(!region, "Out of bounds register bulk write offset: 0x%x\n", offset))
		return;

	if (region->ops && region->ops->preaccess)
		region->ops->preaccess(region);

	/*
	 * A bulk write operation can only be in one region - it cannot
	 * cross boundaries
	 */
	if (WARN((offset - region->offset) + dwords > region->size,
		"OUt of bounds bulk write size: 0x%x\n", offset + dwords))
		return;

	/* Make sure all pending write are done first */
	wmb();
	memcpy_toio(region_addr(region, offset), data, dwords << 2);
}

void kgsl_regmap_bulk_read(struct kgsl_regmap *regmap, u32 offset,
		const void *data, int dwords)
{
	struct kgsl_regmap_region *region = kgsl_regmap_get_region(regmap, offset);

	if (WARN(!region, "Out of bounds register bulk read offset: 0x%x\n", offset))
		return;

	if (region->ops && region->ops->preaccess)
		region->ops->preaccess(region);

	/*
	 * A bulk read operation can only be in one region - it cannot
	 * cross boundaries
	 */
	if (WARN((offset - region->offset) + dwords > region->size,
		"Out of bounds bulk read size: 0x%x\n", offset + dwords))
		return;

	memcpy_fromio(region_addr(region, offset), data, dwords << 2);

	/* Make sure the copy is finished before moving on */
	rmb();
}

void __iomem *kgsl_regmap_virt(struct kgsl_regmap *regmap, u32 offset)
{
	struct kgsl_regmap_region *region = kgsl_regmap_get_region(regmap, offset);

	if (region)
		return region_addr(region, offset);

	return NULL;
}

void kgsl_regmap_read_indexed(struct kgsl_regmap *regmap, u32 addr,
		u32 data, u32 *dest, int count)
{
	struct kgsl_regmap_region *region = kgsl_regmap_get_region(regmap, addr);
	int i;

	if (!region)
		return;

	/* Make sure the offset is in the same region */
	if (kgsl_regmap_get_region(regmap, data) != region)
		return;

	if (region->ops && region->ops->preaccess)
		region->ops->preaccess(region);

	/* Write the address register */
	writel_relaxed(0, region_addr(region, addr));

	/* Make sure the write finishes */
	wmb();

	for (i = 0; i < count; i++)
		dest[i] = readl_relaxed(region_addr(region, data));

	/* Do one barrier at the end to make sure all the data is posted */
	rmb();
}

void kgsl_regmap_read_indexed_interleaved(struct kgsl_regmap *regmap, u32 addr,
		u32 data, u32 *dest, u32 start, int count)
{
	struct kgsl_regmap_region *region = kgsl_regmap_get_region(regmap, addr);
	int i;

	if (!region)
		return;

	/* Make sure the offset is in the same region */
	if (kgsl_regmap_get_region(regmap, data) != region)
		return;

	if (region->ops && region->ops->preaccess)
		region->ops->preaccess(region);

	for (i = 0; i < count; i++) {
		/* Write the address register */
		writel_relaxed(start + i, region_addr(region, addr));
		/* Make sure the write finishes */
		wmb();

		dest[i] = readl_relaxed(region_addr(region, data));
		/* Make sure the read finishes */
		rmb();
	}
}

/* A special helper function to work with read_poll_timeout */
int kgsl_regmap_poll_read(struct kgsl_regmap_region *region, u32 offset,
		u32 *val)
{
	/* FIXME: WARN on !region? */
	if (WARN(!region, "Out of bounds poll read: 0x%x\n", offset))
		return -ENODEV;

	*val = readl_relaxed(region_addr(region, offset));
	/* Make sure the read is finished before moving on */
	rmb();

	return 0;
}
