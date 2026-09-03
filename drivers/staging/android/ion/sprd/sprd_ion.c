// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
//
// * Copyright (C) 2020 Unisoc Inc.
//

#include <asm/cacheflush.h>
#include <linux/compat.h>
#include <linux/dma-buf.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <../ion_private.h>
#include <uapi/linux/sprd_ion.h>
#include <linux/uaccess.h>
#include "linux/ion.h"

#define ION_CARVEOUT_ALLOCATE_FAIL	-1

static int num_heaps;
static struct ion_heap **heaps;

struct ion_platform_heap {
	enum ion_heap_type type;
	unsigned int id;
	const char *name;
	phys_addr_t base;
	size_t size;
	phys_addr_t align;
	void *priv;
};

struct ion_carveout_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	phys_addr_t base;
};

static int ion_heap_clear_pages(struct page **pages, int num, pgprot_t pgprot)
{
	void *addr = vmap(pages, num, VM_MAP, pgprot);

	if (!addr)
		return -ENOMEM;
	memset(addr, 0, PAGE_SIZE * num);
	vunmap(addr);

	return 0;
}

static int ion_heap_sglist_zero(struct scatterlist *sgl, unsigned int nents,
				pgprot_t pgprot)
{
	int p = 0;
	int ret = 0;
	struct sg_page_iter piter;
	struct page *pages[32];

	for_each_sg_page(sgl, &piter, nents, 0) {
		pages[p++] = sg_page_iter_page(&piter);
		if (p == ARRAY_SIZE(pages)) {
			ret = ion_heap_clear_pages(pages, p, pgprot);
			if (ret)
				return ret;
			p = 0;
		}
	}
	if (p)
		ret = ion_heap_clear_pages(pages, p, pgprot);

	return ret;
}

static int ion_heap_buffer_zero(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	pgprot_t pgprot;

	if (buffer->flags & ION_FLAG_CACHED)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	return ion_heap_sglist_zero(table->sgl, table->nents, pgprot);
}

static int ion_heap_pages_zero(struct page *page, size_t size, pgprot_t pgprot)
{
	struct scatterlist sg;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, page, size, 0);
	return ion_heap_sglist_zero(&sg, 1, pgprot);
}

static phys_addr_t ion_carveout_allocate(struct ion_heap *heap,
					 unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	unsigned long offset = gen_pool_alloc(carveout_heap->pool, size);

	if (!offset)
		return ION_CARVEOUT_ALLOCATE_FAIL;

	return offset;
}

static void ion_carveout_free(struct ion_heap *heap, phys_addr_t addr,
			      unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(carveout_heap->pool, addr, size);
}

static int ion_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size,
				      unsigned long flags)
{
	struct sg_table *table;
	phys_addr_t paddr;
	int ret;

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	paddr = ion_carveout_allocate(heap, size);
	if (paddr == ION_CARVEOUT_ALLOCATE_FAIL) {
		pr_err("%s: failed to alloc heap id: %u, size: %lu\n",
		       __func__, heap->id, size);
		ret = -ENOMEM;
		goto err_free_table;
	}
	pr_info("%s: heap id: %u, paddr: 0x%llx, size: %lu\n",
		__func__, heap->id, (u64)paddr, size);

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->sg_table = table;

	return 0;

err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_carveout_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	ion_heap_buffer_zero(buffer);

	ion_carveout_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
	pr_info("%s: heap id: %u, paddr: 0x%llx, size: %zu\n",
		__func__, heap->id, (u64)paddr, buffer->size);
}

static struct ion_heap_ops carveout_heap_ops = {
	.allocate = ion_carveout_heap_allocate,
	.free = ion_carveout_heap_free,
};

static struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_carveout_heap *carveout_heap;
	int ret;

	struct page *page;
	size_t size;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
	if (ret)
		return ERR_PTR(ret);

	carveout_heap = kzalloc(sizeof(*carveout_heap), GFP_KERNEL);
	if (!carveout_heap)
		return ERR_PTR(-ENOMEM);

	carveout_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!carveout_heap->pool) {
		kfree(carveout_heap);
		return ERR_PTR(-ENOMEM);
	}
	carveout_heap->base = heap_data->base;
	gen_pool_add(carveout_heap->pool, carveout_heap->base, heap_data->size,
		     -1);
	carveout_heap->heap.ops = &carveout_heap_ops;
	carveout_heap->heap.type = ION_HEAP_TYPE_DMA;
	carveout_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	carveout_heap->heap.id = 1 << heap_data->id;
	carveout_heap->heap.name = heap_data->name;

	return &carveout_heap->heap;
}

static struct ion_buffer *get_ion_buffer(int fd, struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer;

	if (fd < 0 && !dmabuf) {
		pr_err("%s, input fd: %d, dmabuf: %p error\n", __func__, fd,
		       dmabuf);
		return ERR_PTR(-EINVAL);
	}

	if (fd >= 0) {
		dmabuf = dma_buf_get(fd);
		if (IS_ERR_OR_NULL(dmabuf)) {
			pr_err("%s, dmabuf=%p dma_buf_get error!\n", __func__,
			       dmabuf);
			return ERR_PTR(-EBADF);
		}
		buffer = dmabuf->priv;
		dma_buf_put(dmabuf);
	} else {
		buffer = dmabuf->priv;
	}

	return buffer;
}

int sprd_ion_get_buffer(int fd, struct dma_buf *dmabuf,
			void **buf, size_t *size)
{
	struct ion_buffer *buffer;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	*buf = (void *)buffer;
	*size = buffer->size;

	return 0;
}
EXPORT_SYMBOL(sprd_ion_get_buffer);

int sprd_ion_get_phys_addr(int fd, struct dma_buf *dmabuf,
			   unsigned long *phys_addr, size_t *size)
{
	int ret = 0;
	struct ion_buffer *buffer;
	struct sg_table *table = NULL;
	struct scatterlist *sgl = NULL;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	if (buffer->heap->type != ION_HEAP_TYPE_SYSTEM ||
	    buffer->size == 0x1000) {
		table = buffer->sg_table;
		if (table && table->sgl) {
			sgl = table->sgl;
		} else {
			if (!table)
				pr_err("invalid table\n");
			else if (!table->sgl)
				pr_err("invalid table->sgl\n");
			return -EINVAL;
		}

		*phys_addr = sg_phys(sgl);
		*size = buffer->size;
	} else {
		pr_err("%s, buffer heap type:%d error\n", __func__,
		       buffer->heap->type);
		return -EPERM;
	}

	return ret;
}
EXPORT_SYMBOL(sprd_ion_get_phys_addr);

void *sprd_ion_map_kernel(struct dma_buf *dmabuf, unsigned long offset)
{
	void *vaddr = NULL;
	struct ion_buffer *buffer = dmabuf->priv;

	if (!dmabuf)
		return ERR_PTR(-EINVAL);

	dmabuf->ops->begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	vaddr = ion_buffer_kmap_get(buffer) + offset * PAGE_SIZE;

	return vaddr;
}
EXPORT_SYMBOL(sprd_ion_map_kernel);

int sprd_ion_unmap_kernel(struct dma_buf *dmabuf, unsigned long offset)
{

	struct ion_buffer *buffer = dmabuf->priv;

	if (!dmabuf)
		return -EINVAL;

	ion_buffer_kmap_put(buffer);
	dmabuf->ops->end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);

	return 0;
}
EXPORT_SYMBOL(sprd_ion_unmap_kernel);

static long sprd_ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct ion_phy_data data;

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	/*
	 * The copy_from_user is unconditional here for both read and write
	 * to do the validate. If there is no write for the ioctl, the
	 * buffer is cleared
	 */
	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	if (!(_IOC_DIR(cmd) & _IOC_WRITE))
		memset(&data, 0, sizeof(data));

	switch (cmd) {
	case ION_IOC_PHY:
	{
		int fd = data.fd;

		ret = sprd_ion_get_phys_addr(fd, NULL, (unsigned long *)&data.addr,
			      (size_t *)&data.len);
		break;
	}
	default:
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
	}
	return ret;
}
static const struct file_operations sprd_ion_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = sprd_ion_ioctl,
	.compat_ioctl = sprd_ion_ioctl,
};

static int sprd_ion_device_create(void)
{
	struct ion_device *sprd_ion_dev;
	int ret;

	sprd_ion_dev = kzalloc(sizeof(*sprd_ion_dev), GFP_KERNEL);
	if (!sprd_ion_dev)
		return -ENOMEM;

	sprd_ion_dev->dev.minor = MISC_DYNAMIC_MINOR;
	sprd_ion_dev->dev.name = "sprd_ion";
	sprd_ion_dev->dev.fops = &sprd_ion_fops;
	sprd_ion_dev->dev.parent = NULL;
	ret = misc_register(&sprd_ion_dev->dev);
	if (ret) {
		pr_err("ion: failed to register misc device.\n");
		goto err_reg;
	}

	init_rwsem(&sprd_ion_dev->lock);
	plist_head_init(&sprd_ion_dev->heaps);
	return 0;

err_reg:
	kfree(sprd_ion_dev);
	return ret;
}

static struct ion_platform_heap *sprd_ion_parse_dt(struct platform_device *pdev)
{
	int i = 0, ret = 0;
	const struct device_node *parent = pdev->dev.of_node;
	struct device_node *child = NULL;
	struct ion_platform_heap *ion_heaps = NULL;
	struct platform_device *new_dev = NULL;
	u32 val = 0, type = 0;
	const char *name;
	u32 out_values[4];
	struct device_node *np_memory;

	for_each_child_of_node(parent, child)
		num_heaps++;

	pr_info("%s: num_heaps=%d\n", __func__, num_heaps);

	if (!num_heaps)
		return NULL;

	ion_heaps = kcalloc(num_heaps, sizeof(struct ion_platform_heap),
			    GFP_KERNEL);
	if (!ion_heaps)
		return ERR_PTR(-ENOMEM);

	for_each_child_of_node(parent, child) {
		new_dev = of_platform_device_create(child, NULL, &pdev->dev);
		if (!new_dev) {
			pr_err("Failed to create device %s\n", child->name);
			goto out;
		}

		ion_heaps[i].priv = &new_dev->dev;

		ret = of_property_read_u32(child, "reg", &val);
		if (ret) {
			pr_err("%s: Unable to find reg key, ret=%d", __func__,
			       ret);
			goto out;
		}
		ion_heaps[i].id = val;

		ret = of_property_read_string(child, "label", &name);
		if (ret) {
			pr_err("%s: Unable to find label key, ret=%d", __func__,
			       ret);
			goto out;
		}
		ion_heaps[i].name = name;

		ret = of_property_read_u32(child, "type", &type);
		if (ret) {
			pr_err("%s: Unable to find type key, ret=%d", __func__,
			       ret);
			goto out;
		}
		ion_heaps[i].type = type;

		np_memory = of_parse_phandle(child, "memory-region", 0);

		if (!np_memory) {
			ion_heaps[i].base = 0;
			ion_heaps[i].size = 0;
		} else {
#ifdef CONFIG_64BIT
			ret = of_property_read_u32_array(np_memory, "reg",
							 out_values, 4);
			if (!ret) {
				ion_heaps[i].base = out_values[0];
				ion_heaps[i].base = ion_heaps[i].base << 32;
				ion_heaps[i].base |= out_values[1];

				ion_heaps[i].size = out_values[2];
				ion_heaps[i].size = ion_heaps[i].size << 32;
				ion_heaps[i].size |= out_values[3];
			} else {
				ion_heaps[i].base = 0;
				ion_heaps[i].size = 0;
			}
#else
			ret = of_property_read_u32_array(np_memory, "reg",
							 out_values, 2);
			if (!ret) {
				ion_heaps[i].base = out_values[0];
				ion_heaps[i].size = out_values[1];
			} else {
				ion_heaps[i].base = 0;
				ion_heaps[i].size = 0;
			}
#endif
		}

		pr_info("%s: heaps[%d]: id: %u %s type: %d base: 0x%llx size 0x%zx\n",
			__func__, i, ion_heaps[i].id, ion_heaps[i].name,
			ion_heaps[i].type, (u64)(ion_heaps[i].base),
			ion_heaps[i].size);
		++i;
	}
	return ion_heaps;
out:
	kfree(ion_heaps);
	return ERR_PTR(ret);
}

static int sprd_ion_probe(struct platform_device *pdev)
{
	int i = 0, ret = -1;
	struct ion_platform_heap *ion_heaps = NULL;

	ion_heaps = sprd_ion_parse_dt(pdev);
	if (IS_ERR(ion_heaps)) {
		pr_err("%s: parse dt failed with err %ld\n",
		       __func__, PTR_ERR(ion_heaps));
		return PTR_ERR(ion_heaps);
	}

	heaps = kcalloc(num_heaps, sizeof(struct ion_heap *), GFP_KERNEL);
	if (!heaps) {
		ret = -ENOMEM;
		goto out1;
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &ion_heaps[i];

		if (!pdev->dev.of_node)
			heap_data->priv = &pdev->dev;
		heaps[i] = ion_carveout_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			pr_err("%s,heaps is null, i:%d\n", __func__, i);
			ret = PTR_ERR(heaps[i]);
			goto out;
		}
		ion_device_add_heap(heaps[i]);
	}

	if (ion_heaps)
		goto out1;

out:
	kfree(heaps);
out1:
	kfree(ion_heaps);

	return sprd_ion_device_create();
}

static int sprd_ion_remove(struct platform_device *pdev)
{
	kfree(heaps);

	return 0;
}

static const struct of_device_id sprd_ion_ids[] = {
	{ .compatible = "sprd,ion"},
	{},
};

static struct platform_driver sprd_ion_driver = {
	.probe = sprd_ion_probe,
	.remove = sprd_ion_remove,
	.driver = {
		.name = "ion",
		.of_match_table = of_match_ptr(sprd_ion_ids),
	},
};

module_platform_driver(sprd_ion_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Unisoc ION Driver");
MODULE_AUTHOR("Sheng Xu <sheng.xu@unisoc.com>");

