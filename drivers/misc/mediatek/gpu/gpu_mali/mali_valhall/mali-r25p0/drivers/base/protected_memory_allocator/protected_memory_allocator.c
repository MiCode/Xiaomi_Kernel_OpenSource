/*
 *
 * (C) COPYRIGHT 2019-2020 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <linux/version.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/protected_memory_allocator.h>

/**
 * struct simple_pma_device - Simple implementation of a protected memory
 *                            allocator device
 *
 * @pma_dev:        Protected memory allocator device pointer
 * @dev:            Device pointer
 * @alloc_pages:    Status of all the physical memory pages within the
 *                  protected memory region; true for allocated pages
 * @rmem_base:      Base address of the reserved memory region
 * @rmem_size:      Size of the reserved memory region, in pages
 * @free_pa_offset: Offset of the lowest physical address within the protected
 *                  memory region that is currently associated with a free page
 * @num_free_pages: Number of free pages in the memory region
 */
struct simple_pma_device {
	struct protected_memory_allocator_device pma_dev;
	struct device *dev;
	bool *alloc_pages;
	phys_addr_t rmem_base;
	size_t rmem_size;
	size_t free_pa_offset;
	size_t num_free_pages;
};

static struct protected_memory_allocation *simple_pma_alloc_page(
	struct protected_memory_allocator_device *pma_dev, unsigned int order)
{
	struct simple_pma_device *const epma_dev =
		container_of(pma_dev, struct simple_pma_device, pma_dev);
	struct protected_memory_allocation *pma;
	size_t num_pages;
	size_t i;

	dev_dbg(epma_dev->dev, "%s(pma_dev=%px, order=%u\n",
		__func__, (void *)pma_dev, order);

	/* This is an example function that follows an extremely simple logic
	 * and is very likely to fail to allocate memory if put under stress.
	 *
	 * The simple_pma_device maintains an array of booleans to track
	 * the status of every page and an offset to the free page to use
	 * for the next allocation. The offset starts from 0 and can only grow,
	 * and be reset when the end of the memory region is reached.
	 *
	 * In order to create a memory allocation, the allocator simply looks
	 * at the offset and verifies whether there are enough free pages
	 * after it to accommodate the allocation request. If successful,
	 * the allocator shall mark all the pages as allocated and increment
	 * the offset accordingly.
	 *
	 * The allocator does not look for any other free pages inside the
	 * memory region, even if plenty of free memory is available.
	 * Free memory pages are counted and the offset is ignored if the
	 * memory region is fully allocated.
	 */

	/* The only candidate for allocation is the sub-region starting
	 * from the free_pa_offset. Verify that enough contiguous pages
	 * are available and that they are all free.
	 */
	num_pages = (size_t)1 << order;

	if (epma_dev->num_free_pages < num_pages)
		dev_err(epma_dev->dev, "not enough free pages\n");

	if (epma_dev->free_pa_offset + num_pages > epma_dev->rmem_size) {
		dev_err(epma_dev->dev, "not enough contiguous pages\n");
		return NULL;
	}

	for (i = 0; i < num_pages; i++)
		if (epma_dev->alloc_pages[epma_dev->free_pa_offset + i])
			break;

	if (i < num_pages) {
		dev_err(epma_dev->dev, "free pages are not contiguous\n");
		return NULL;
	}

	/* Memory allocation is successful. Mark pages as allocated.
	 * Update the free_pa_offset if free pages are still available:
	 * increment the free_pa_offset accordingly, and then making sure
	 * that it points at the next free page, potentially wrapping over
	 * the end of the memory region.
	 */
	pma = devm_kzalloc(epma_dev->dev, sizeof(*pma), GFP_KERNEL);
	if (!pma)
		return NULL;

	pma->pa = epma_dev->rmem_base + (epma_dev->free_pa_offset << PAGE_SHIFT);
	pma->order = order;

	for (i = 0; i < num_pages; i++)
		epma_dev->alloc_pages[epma_dev->free_pa_offset + i] = true;

	epma_dev->num_free_pages -= num_pages;

	if (epma_dev->num_free_pages) {
		epma_dev->free_pa_offset += num_pages;
		i = 0;
		while (epma_dev->alloc_pages[epma_dev->free_pa_offset + i]) {
			epma_dev->free_pa_offset++;
			if (epma_dev->free_pa_offset > epma_dev->rmem_size)
				epma_dev->free_pa_offset = 0;
		}
	}

	return pma;
}

static phys_addr_t simple_pma_get_phys_addr(
	struct protected_memory_allocator_device *pma_dev,
	struct protected_memory_allocation *pma)
{
	struct simple_pma_device *const epma_dev =
		container_of(pma_dev, struct simple_pma_device, pma_dev);

	dev_dbg(epma_dev->dev, "%s(pma_dev=%px, pma=%px, pa=%llx\n",
		__func__, (void *)pma_dev, (void *)pma, pma->pa);

	return pma->pa;
}

static void simple_pma_free_page(
	struct protected_memory_allocator_device *pma_dev,
	struct protected_memory_allocation *pma)
{
	struct simple_pma_device *const epma_dev =
		container_of(pma_dev, struct simple_pma_device, pma_dev);
	size_t num_pages;
	size_t offset;
	size_t i;

	dev_dbg(epma_dev->dev, "%s(pma_dev=%px, pma=%px, pa=%llx\n",
		__func__, (void *)pma_dev, (void *)pma, pma->pa);

	/* This is an example function that follows an extremely simple logic
	 * and is vulnerable to abuse. For instance, double frees won't be
	 * detected.
	 *
	 * If memory is full, must update the free_pa_offset that is currently
	 * pointing at an allocated page.
	 *
	 * Increase the number of free pages and mark them as free.
	 */
	offset = (pma->pa - epma_dev->rmem_base) >> PAGE_SHIFT;
	num_pages = (size_t)1 << pma->order;

	if (epma_dev->num_free_pages == 0)
		epma_dev->free_pa_offset = offset;

	epma_dev->num_free_pages += num_pages;
	for (i = 0; i < num_pages; i++)
		epma_dev->alloc_pages[offset + i] = false;

	devm_kfree(epma_dev->dev, pma);
}

static int protected_memory_allocator_probe(struct platform_device *pdev)
{
	struct simple_pma_device *epma_dev;
	struct device_node *np;
	phys_addr_t rmem_base;
	size_t rmem_size;
#if (KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE)
	struct reserved_mem *rmem;
#endif

	np = pdev->dev.of_node;

	if (!np) {
		dev_err(&pdev->dev, "device node pointer not set\n");
		return -ENODEV;
	}

	np = of_parse_phandle(np, "memory-region", 0);
	if (!np) {
		dev_err(&pdev->dev, "memory-region node not set\n");
		return -ENODEV;
	}

#if (KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE)
	rmem = of_reserved_mem_lookup(np);
	if (rmem) {
		rmem_base = rmem->base;
		rmem_size = rmem->size >> PAGE_SHIFT;
	} else
#endif
	{
		of_node_put(np);
		dev_err(&pdev->dev, "could not read reserved memory-region\n");
		return -ENODEV;
	}

	of_node_put(np);
	epma_dev = devm_kzalloc(&pdev->dev, sizeof(*epma_dev), GFP_KERNEL);
	if (!epma_dev)
		return -ENOMEM;

	epma_dev->pma_dev.ops.pma_alloc_page = simple_pma_alloc_page;
	epma_dev->pma_dev.ops.pma_get_phys_addr = simple_pma_get_phys_addr;
	epma_dev->pma_dev.ops.pma_free_page = simple_pma_free_page;
	epma_dev->pma_dev.owner = THIS_MODULE;
	epma_dev->dev = &pdev->dev;
	epma_dev->rmem_base = rmem_base;
	epma_dev->rmem_size = rmem_size;
	epma_dev->free_pa_offset = 0;
	epma_dev->num_free_pages = rmem_size;

	epma_dev->alloc_pages =	devm_kzalloc(&pdev->dev,
		sizeof(bool) * epma_dev->rmem_size, GFP_KERNEL);

	if (!epma_dev->alloc_pages) {
		dev_err(&pdev->dev, "failed to allocate resources\n");
		devm_kfree(&pdev->dev, epma_dev);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, &epma_dev->pma_dev);
	dev_info(&pdev->dev,
		"Protected memory allocator probed successfully\n");
	dev_info(&pdev->dev, "Protected memory region: base=%llx num pages=%zu\n",
		rmem_base, rmem_size);

	return 0;
}

static int protected_memory_allocator_remove(struct platform_device *pdev)
{
	struct protected_memory_allocator_device *pma_dev =
		platform_get_drvdata(pdev);
	struct simple_pma_device *epma_dev;
	struct device *dev;

	if (!pma_dev)
		return -EINVAL;

	epma_dev = container_of(pma_dev, struct simple_pma_device, pma_dev);
	dev = epma_dev->dev;

	if (epma_dev->num_free_pages < epma_dev->rmem_size) {
		dev_warn(&pdev->dev, "Leaking %zu pages of protected memory\n",
			epma_dev->rmem_size - epma_dev->num_free_pages);
	}

	platform_set_drvdata(pdev, NULL);
	devm_kfree(dev, epma_dev->alloc_pages);
	devm_kfree(dev, epma_dev);

	dev_info(&pdev->dev,
		"Protected memory allocator removed successfully\n");

	return 0;
}

static const struct of_device_id protected_memory_allocator_dt_ids[] = {
	{ .compatible = "arm,protected-memory-allocator" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, protected_memory_allocator_dt_ids);

static struct platform_driver protected_memory_allocator_driver = {
	.probe = protected_memory_allocator_probe,
	.remove = protected_memory_allocator_remove,
	.driver = {
		.name = "simple_protected_memory_allocator",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(protected_memory_allocator_dt_ids),
	}
};

module_platform_driver(protected_memory_allocator_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ARM Ltd.");
MODULE_VERSION("1.0");
