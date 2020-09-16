// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/qcom-iommu-util.h>

struct device_node *qcom_iommu_group_parse_phandle(struct device *dev)
{
	struct device_node *np;

	if (!dev->of_node)
		return NULL;

	np = of_parse_phandle(dev->of_node, "qcom,iommu-group", 0);
	return np ? np : dev->of_node;
}

static bool check_overlap(struct iommu_resv_region *region, u64 start, u64 end)
{
	u64 region_end = region->start + region->length - 1;

	return end >= region->start && start <= region_end;
}

static int insert_range(struct list_head *head, u64 start, u64 end)
{
	struct iommu_resv_region *region, *new;

	list_for_each_entry(region, head, list) {
		if (check_overlap(region, start, end))
			return -EINVAL;

		if (start < region->start)
			break;
	}

	new = iommu_alloc_resv_region(start, end - start + 1,
					0, IOMMU_RESV_RESERVED);
	if (!new)
		return -ENOMEM;
	list_add_tail(&new->list, &region->list);
	return 0;
}

/*
 * Returns a sorted list of all regions described by the
 * "qcom,iommu-dma-addr-pool" property.
 *
 * Caller is responsible for freeing the entries on the list via
 * generic_iommu_put_resv_regions
 */
int qcom_iommu_generate_dma_regions(struct device *dev,
		struct list_head *head)
{
	char *propname = "qcom,iommu-dma-addr-pool";
	const __be32 *p, *property_end;
	struct device_node *np;
	int len, naddr, nsize;
	u64 window_start, window_end, window_size;

	np = qcom_iommu_group_parse_phandle(dev);
	if (!np)
		return -EINVAL;

	p = of_get_property(np, propname, &len);
	if (!p)
		return -ENODEV;

	len /= sizeof(u32);
	naddr = of_n_addr_cells(np);
	nsize = of_n_size_cells(np);
	if (!naddr || !nsize || len % (naddr + nsize)) {
		dev_err(dev, "%s Invalid length %d. Address cells %d. Size cells %d\n",
			propname, len, naddr, nsize);
		return -EINVAL;
	}
	property_end = p + len;

	while (p < property_end) {
		window_start = of_read_number(p, naddr);
		p += naddr;
		window_size = of_read_number(p, nsize);
		p += nsize;
		window_end = window_start + window_size - 1;

		if (insert_range(head, window_start, window_end)) {
			dev_err(dev, "%s Invalid range %llx - %llx\n",
				propname, window_start, window_end);
			generic_iommu_put_resv_regions(dev, head);
			return -EINVAL;
		}
	}
	return 0;
}
EXPORT_SYMBOL(qcom_iommu_generate_dma_regions);

static int invert_regions(struct list_head *head, struct list_head *inverted)
{
	struct iommu_resv_region *prev, *curr, *new;
	phys_addr_t rsv_start;
	size_t rsv_size;
	int ret = 0;

	/*
	 * Since its not possible to express start 0, size 1<<64 return
	 * an error instead. Also an iova allocator without any iovas doesn't
	 * make sense.
	 */
	if (list_empty(head))
		return -EINVAL;

	/*
	 * Handle case where there is a non-zero sized area between
	 * iommu_resv_regions A & B.
	 */
	prev = NULL;
	list_for_each_entry(curr, head, list) {
		if (!prev)
			goto next;

		rsv_start = prev->start + prev->length;
		rsv_size = curr->start - rsv_start;
		if (!rsv_size)
			goto next;

		new = iommu_alloc_resv_region(rsv_start, rsv_size,
						0, IOMMU_RESV_RESERVED);
		if (!new) {
			ret = -ENOMEM;
			goto out_err;
		}
		list_add_tail(&new->list, inverted);
next:
		prev = curr;
	}

	/* Now handle the beginning */
	curr = list_first_entry(head, struct iommu_resv_region, list);
	rsv_start = 0;
	rsv_size = curr->start;
	if (rsv_size) {
		new = iommu_alloc_resv_region(rsv_start, rsv_size,
						0, IOMMU_RESV_RESERVED);
		if (!new) {
			ret = -ENOMEM;
			goto out_err;
		}
		list_add(&new->list, inverted);
	}

	/* Handle the end - checking for overflow */
	rsv_start = prev->start + prev->length;
	rsv_size = -rsv_start;

	if (rsv_size && (U64_MAX - prev->start > prev->length)) {
		new = iommu_alloc_resv_region(rsv_start, rsv_size,
						0, IOMMU_RESV_RESERVED);
		if (!new) {
			ret = -ENOMEM;
			goto out_err;
		}
		list_add_tail(&new->list, inverted);
	}

	return 0;

out_err:
	list_for_each_entry_safe(curr, prev, inverted, list)
		kfree(curr);
	return ret;
}

/* Used by iommu drivers to generate reserved regions for qcom,iommu-dma-addr-pool property */
void qcom_iommu_generate_resv_regions(struct device *dev,
				      struct list_head *head)
{
	struct iommu_resv_region *region;
	LIST_HEAD(dma_regions);
	LIST_HEAD(resv_regions);
	int ret;

	ret = qcom_iommu_generate_dma_regions(dev, &dma_regions);
	if (ret)
		return;

	ret = invert_regions(&dma_regions, &resv_regions);
	generic_iommu_put_resv_regions(dev, &dma_regions);
	if (ret)
		return;

	list_for_each_entry(region, &resv_regions, list) {
		dev_dbg(dev, "Reserved region %llx-%llx\n",
			(u64)region->start,
			(u64)(region->start + region->length - 1));
	}

	list_splice(&resv_regions, head);
}
EXPORT_SYMBOL(qcom_iommu_generate_resv_regions);

void qcom_iommu_get_resv_regions(struct device *dev, struct list_head *list)
{
	const struct iommu_ops *ops = dev->bus->iommu_ops;

	if (ops && ops->get_resv_regions)
		ops->get_resv_regions(dev, list);
}
EXPORT_SYMBOL(qcom_iommu_get_resv_regions);

void qcom_iommu_put_resv_regions(struct device *dev, struct list_head *list)
{
	const struct iommu_ops *ops = dev->bus->iommu_ops;

	if (ops && ops->put_resv_regions)
		ops->put_resv_regions(dev, list);
}
EXPORT_SYMBOL(qcom_iommu_put_resv_regions);

/*
 * These tables must have the same length.
 * It is allowed to have a NULL exitcall corresponding to a non-NULL initcall.
 */
static initcall_t init_table[] __initdata = {
	NULL
};

static exitcall_t exit_table[] = {
	NULL
};

static int __init qcom_iommu_util_init(void)
{
	initcall_t *init_fn;
	exitcall_t *exit_fn;
	int ret;

	if (ARRAY_SIZE(init_table) != ARRAY_SIZE(exit_table)) {
		pr_err("qcom-iommu-util: Invalid initcall/exitcall table\n");
		return -EINVAL;
	}

	for (init_fn = init_table; *init_fn; init_fn++) {
		ret = (**init_fn)();
		if (ret) {
			pr_err("%ps returned %d\n", *init_fn, ret);
			goto out_undo;
		}
	}

	return 0;

out_undo:
	exit_fn = exit_table + (init_fn - init_table);
	for (exit_fn--; exit_fn >= exit_table; exit_fn--) {
		if (!*exit_fn)
			continue;
		(**exit_fn)();
	}
	return ret;
}
module_init(qcom_iommu_util_init);

static void qcom_iommu_util_exit(void)
{
	exitcall_t *exit_fn;

	exit_fn = exit_table + ARRAY_SIZE(exit_table) - 2;
	for (; exit_fn >= exit_table; exit_fn--) {
		if (!*exit_fn)
			continue;
		(**exit_fn)();
	}
}
module_exit(qcom_iommu_util_exit);

MODULE_LICENSE("GPL v2");
