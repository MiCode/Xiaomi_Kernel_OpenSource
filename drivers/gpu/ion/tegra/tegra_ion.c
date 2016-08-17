/*
 * drivers/gpu/tegra/tegra_ion.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2011, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s():%d: " fmt, __func__, __LINE__

#include <linux/err.h>
#include <linux/ion.h>
#include <linux/tegra_ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/io.h>
#include "../ion_priv.h"

#define CLIENT_HEAP_MASK 0xFFFFFFFF
#define HEAP_FLAGS 0xFF

#if !defined(CONFIG_TEGRA_NVMAP)
#include "linux/nvmap.h"
struct nvmap_device *nvmap_dev;
#endif

static struct ion_device *idev;
static int num_heaps;
static struct ion_heap **heaps;

static int tegra_ion_pin(struct ion_client *client,
				unsigned int cmd,
				unsigned long arg)
{
	struct tegra_ion_pin_data data;
	int ret;
	struct ion_handle *on_stack[16];
	struct ion_handle **refs = on_stack;
	int i;
	bool valid_handle;

	if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
		return -EFAULT;
	if (data.count) {
		size_t bytes = data.count * sizeof(struct ion_handle *);

		if (data.count > ARRAY_SIZE(on_stack))
			refs = kmalloc(data.count * sizeof(*refs), GFP_KERNEL);
		else
			refs = on_stack;
		if (!refs)
			return -ENOMEM;
		if (copy_from_user(refs, (void *)data.handles, bytes)) {
			ret = -EFAULT;
			goto err;
		}
	} else
		return -EINVAL;

	mutex_lock(&client->lock);
	for (i = 0; i < data.count; i++) {
		/* Ignore NULL pointers during unpin operation. */
		if (!refs[i] && cmd == TEGRA_ION_UNPIN)
			continue;
		valid_handle = ion_handle_validate(client, refs[i]);
		if (!valid_handle) {
			WARN(1, "invalid handle passed h=0x%x", (u32)refs[i]);
			mutex_unlock(&client->lock);
			ret = -EINVAL;
			goto err;
		}
	}
	mutex_unlock(&client->lock);

	if (cmd == TEGRA_ION_PIN) {
		ion_phys_addr_t addr;
		size_t len;

		for (i = 0; i < data.count; i++) {
			ret = ion_phys(client, refs[i], &addr, &len);
			if (ret)
				goto err;
			ion_handle_get(refs[i]);
			ret = put_user(addr, &data.addr[i]);
			if (ret)
				return ret;
		}
	} else if (cmd == TEGRA_ION_UNPIN) {
		for (i = 0; i < data.count; i++) {
			if (refs[i])
				ion_handle_put(refs[i]);
		}
	}

err:
	if (ret) {
		pr_err("error, ret=0x%x", ret);
		/* FIXME: undo pinning. */
	}
	if (refs != on_stack)
		kfree(refs);
	return ret;
}

static int tegra_ion_alloc_from_id(struct ion_client *client,
				    unsigned int cmd,
				    unsigned long arg)
{
	struct tegra_ion_id_data data;
	struct ion_buffer *buffer;
	struct tegra_ion_id_data *user_data = (struct tegra_ion_id_data *)arg;

	if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
		return -EFAULT;
	buffer = (struct ion_buffer *)data.id;
	data.handle = ion_import(client, buffer);
	data.size = buffer->size;
	if (put_user(data.handle, &user_data->handle))
		return -EFAULT;
	if (put_user(data.size, &user_data->size))
		return -EFAULT;
	return 0;
}

static int tegra_ion_get_id(struct ion_client *client,
					    unsigned int cmd,
					    unsigned long arg)
{
	bool valid_handle;
	struct tegra_ion_id_data data;
	struct tegra_ion_id_data *user_data = (struct tegra_ion_id_data *)arg;

	if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
		return -EFAULT;

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, data.handle);
	mutex_unlock(&client->lock);

	if (!valid_handle) {
		WARN(1, "invalid handle passed\n");
		return -EINVAL;
	}

	pr_debug("h=0x%x, b=0x%x, bref=%d",
		(u32)data.handle, (u32)data.handle->buffer,
		atomic_read(&data.handle->buffer->ref.refcount));
	if (put_user((unsigned long)ion_handle_buffer(data.handle),
			&user_data->id))
		return -EFAULT;
	return 0;
}

static int tegra_ion_cache_maint(struct ion_client *client,
				 unsigned int cmd,
				 unsigned long arg)
{
	wmb();
	return 0;
}

static int tegra_ion_rw(struct ion_client *client,
				unsigned int cmd,
				unsigned long arg)
{
	bool valid_handle;
	struct tegra_ion_rw_data data;
	char *kern_addr, *src;
	int ret = 0;
	size_t copied = 0;

	if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
		return -EFAULT;

	if (!data.handle || !data.addr || !data.count || !data.elem_size)
		return -EINVAL;

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, data.handle);
	mutex_unlock(&client->lock);

	if (!valid_handle) {
		WARN(1, "%s: invalid handle passed to get id.\n", __func__);
		return -EINVAL;
	}

	if (data.elem_size == data.mem_stride &&
	    data.elem_size == data.user_stride) {
		data.elem_size *= data.count;
		data.mem_stride = data.elem_size;
		data.user_stride = data.elem_size;
		data.count = 1;
	}

	kern_addr = ion_map_kernel(client, data.handle);

	while (data.count--) {
		if (data.offset + data.elem_size > data.handle->buffer->size) {
			WARN(1, "read/write outside of handle\n");
			ret = -EFAULT;
			break;
		}

		src = kern_addr + data.offset;
		if (cmd == TEGRA_ION_READ)
			ret = copy_to_user((void *)data.addr,
					    src, data.elem_size);
		else
			ret = copy_from_user(src,
					    (void *)data.addr, data.elem_size);

		if (ret)
			break;

		copied += data.elem_size;
		data.addr += data.user_stride;
		data.offset += data.mem_stride;
	}

	ion_unmap_kernel(client, data.handle);
	return ret;
}

static int tegra_ion_get_param(struct ion_client *client,
					unsigned int cmd,
					unsigned long arg)
{
	bool valid_handle;
	struct tegra_ion_get_params_data data;
	struct tegra_ion_get_params_data *user_data =
				(struct tegra_ion_get_params_data *)arg;
	struct ion_buffer *buffer;

	if (copy_from_user(&data, (void __user *)arg, sizeof(data)))
		return -EFAULT;

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, data.handle);
	mutex_unlock(&client->lock);

	if (!valid_handle) {
		WARN(1, "%s: invalid handle passed to get id.\n", __func__);
		return -EINVAL;
	}

	buffer = ion_handle_buffer(data.handle);
	data.align = 4096;
	data.heap = 1;
	ion_phys(client, data.handle, &data.addr, &data.size);

	if (copy_to_user(user_data, &data, sizeof(data)))
		return -EFAULT;

	return 0;
}

static long tegra_ion_ioctl(struct ion_client *client,
				   unsigned int cmd,
				   unsigned long arg)
{
	int ret = -ENOTTY;

	switch (cmd) {
	case TEGRA_ION_ALLOC_FROM_ID:
		ret = tegra_ion_alloc_from_id(client, cmd, arg);
		break;
	case TEGRA_ION_GET_ID:
		ret = tegra_ion_get_id(client, cmd, arg);
		break;
	case TEGRA_ION_PIN:
	case TEGRA_ION_UNPIN:
		ret = tegra_ion_pin(client, cmd, arg);
		break;
	case TEGRA_ION_CACHE_MAINT:
		ret = tegra_ion_cache_maint(client, cmd, arg);
		break;
	case TEGRA_ION_READ:
	case TEGRA_ION_WRITE:
		ret = tegra_ion_rw(client, cmd, arg);
		break;
	case TEGRA_ION_GET_PARAM:
		ret = tegra_ion_get_param(client, cmd, arg);
		break;
	default:
		WARN(1, "Unknown custom ioctl\n");
		return -ENOTTY;
	}
	return ret;
}

int tegra_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	int i;

	num_heaps = pdata->nr;

	heaps = kzalloc(sizeof(struct ion_heap *) * pdata->nr, GFP_KERNEL);

	idev = ion_device_create(tegra_ion_ioctl);
	if (IS_ERR_OR_NULL(idev)) {
		kfree(heaps);
		return PTR_ERR(idev);
	}

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];

		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			pr_warn("%s(type:%d id:%d) isn't supported\n",
				heap_data->name,
				heap_data->type, heap_data->id);
			continue;
		}
		ion_device_add_heap(idev, heaps[i]);
	}
	platform_set_drvdata(pdev, idev);
#if !defined(CONFIG_TEGRA_NVMAP)
	nvmap_dev = (struct nvmap_device *)idev;
#endif
	return 0;
}

int tegra_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	ion_device_destroy(idev);
	for (i = 0; i < num_heaps; i++)
		ion_heap_destroy(heaps[i]);
	kfree(heaps);
	return 0;
}

static struct platform_driver ion_driver = {
	.probe = tegra_ion_probe,
	.remove = tegra_ion_remove,
	.driver = { .name = "ion-tegra" }
};

static int __init ion_init(void)
{
	return platform_driver_register(&ion_driver);
}

static void __exit ion_exit(void)
{
	platform_driver_unregister(&ion_driver);
}

fs_initcall(ion_init);
module_exit(ion_exit);

#if !defined(CONFIG_TEGRA_NVMAP)
struct nvmap_client *nvmap_create_client(struct nvmap_device *dev,
					 const char *name)
{
	return ion_client_create(dev, CLIENT_HEAP_MASK, name);
}

struct nvmap_handle_ref *nvmap_alloc(struct nvmap_client *client, size_t size,
				     size_t align, unsigned int flags,
				     unsigned int heap_mask)
{
	return ion_alloc(client, size, align, HEAP_FLAGS);
}

void nvmap_free(struct nvmap_client *client, struct nvmap_handle_ref *r)
{
	ion_free(client, r);
}

void *nvmap_mmap(struct nvmap_handle_ref *r)
{
	return ion_map_kernel(r->client, r);
}

void nvmap_munmap(struct nvmap_handle_ref *r, void *addr)
{
	ion_unmap_kernel(r->client, r);
}

struct nvmap_client *nvmap_client_get_file(int fd)
{
	return ion_client_get_file(fd);
}

struct nvmap_client *nvmap_client_get(struct nvmap_client *client)
{
	ion_client_get(client);
	return client;
}

void nvmap_client_put(struct nvmap_client *c)
{
	ion_client_put(c);
}

phys_addr_t nvmap_pin(struct nvmap_client *c, struct nvmap_handle_ref *r)
{
	ion_phys_addr_t addr;
	size_t len;

	ion_handle_get(r);
	ion_phys(c, r, &addr, &len);
	wmb();
	return addr;
}

phys_addr_t nvmap_handle_address(struct nvmap_client *c, unsigned long id)
{
	struct ion_handle *handle;
	ion_phys_addr_t addr;
	size_t len;

	handle = nvmap_convert_handle_u2k(id);
	ion_phys(c, handle, &addr, &len);
	return addr;
}

void nvmap_unpin(struct nvmap_client *client, struct nvmap_handle_ref *r)
{
	if (r)
		ion_handle_put(r);
}

static int nvmap_reloc_pin_array(struct ion_client *client,
				 const struct nvmap_pinarray_elem *arr,
				 int nr, struct ion_handle *gather)
{
	struct ion_handle *last_patch = NULL;
	void *patch_addr;
	ion_phys_addr_t pin_addr;
	size_t len;
	int i;

	for (i = 0; i < nr; i++) {
		struct ion_handle *patch;
		struct ion_handle *pin;
		ion_phys_addr_t reloc_addr;

		/* all of the handles are validated and get'ted prior to
		 * calling this function, so casting is safe here */
		pin = (struct ion_handle *)arr[i].pin_mem;

		if (arr[i].patch_mem == (unsigned long)last_patch) {
			patch = last_patch;
		} else if (arr[i].patch_mem == (unsigned long)gather) {
			patch = gather;
		} else {
			if (last_patch)
				ion_handle_put(last_patch);

			ion_handle_get((struct ion_handle *)arr[i].patch_mem);
			patch = (struct ion_handle *)arr[i].patch_mem;
			if (!patch)
				return -EPERM;
			last_patch = patch;
		}

		patch_addr = ion_map_kernel(client, patch);
		patch_addr = patch_addr + arr[i].patch_offset;

		ion_phys(client, pin, &pin_addr, &len);
		reloc_addr = pin_addr + arr[i].pin_offset;
		__raw_writel(reloc_addr, patch_addr);
		ion_unmap_kernel(client, patch);
	}

	if (last_patch)
		ion_handle_put(last_patch);

	wmb();
	return 0;
}

int nvmap_pin_array(struct nvmap_client *client, struct nvmap_handle *gather,
		    const struct nvmap_pinarray_elem *arr, int nr,
		    struct nvmap_handle **unique)
{
	int i;
	int count = 0;

	/* FIXME: take care of duplicate ones & validation. */
	for (i = 0; i < nr; i++) {
		unique[i] = (struct nvmap_handle *)arr[i].pin_mem;
		nvmap_pin(client, (struct nvmap_handle_ref *)unique[i]);
		count++;
	}
	nvmap_reloc_pin_array((struct ion_client *)client,
		arr, nr, (struct ion_handle *)gather);
	return nr;
}

void nvmap_unpin_handles(struct nvmap_client *client,
			 struct nvmap_handle **h, int nr)
{
	int i;

	for (i = 0; i < nr; i++)
		nvmap_unpin(client, h[i]);
}

int nvmap_patch_word(struct nvmap_client *client,
		     struct nvmap_handle *patch,
		     u32 patch_offset, u32 patch_value)
{
	void *vaddr;
	u32 *patch_addr;

	vaddr = ion_map_kernel(client, patch);
	patch_addr = vaddr + patch_offset;
	__raw_writel(patch_value, patch_addr);
	wmb();
	ion_unmap_kernel(client, patch);
	return 0;
}

struct nvmap_handle *nvmap_handle_get(struct nvmap_handle *h);
struct nvmap_handle *nvmap_get_handle_id(struct nvmap_client *client,
					 unsigned long id)
{
	struct ion_handle *handle;

	handle = (struct ion_handle *)nvmap_convert_handle_u2k(id);
	pr_debug("id=0x%x, h=0x%x,c=0x%x",
		(u32)id, (u32)handle, (u32)client);
	nvmap_handle_get(handle);
	return handle;
}

struct nvmap_handle_ref *nvmap_duplicate_handle_id(struct nvmap_client *client,
						   unsigned long id)
{
	struct ion_buffer *buffer;
	struct ion_handle *handle;
	struct ion_client *ion_client = client;

	handle = (struct ion_handle *)nvmap_convert_handle_u2k(id);
	pr_debug("id=0x%x, h=0x%x,c=0x%x",
		(u32)id, (u32)handle, (u32)client);
	buffer = handle->buffer;

	handle = ion_handle_create(client, buffer);

	mutex_lock(&ion_client->lock);
	ion_handle_add(ion_client, handle);
	mutex_unlock(&ion_client->lock);

	pr_debug("dup id=0x%x, h=0x%x", (u32)id, (u32)handle);
	return handle;
}

void _nvmap_handle_free(struct nvmap_handle *h)
{
	ion_handle_put(h);
}

struct nvmap_handle_ref *nvmap_alloc_iovm(struct nvmap_client *client,
	size_t size, size_t align, unsigned int flags, unsigned int iova_start)
{
	struct ion_handle *h;

	h = ion_alloc(client, size, align, 0xFF);
	ion_remap_dma(client, h, iova_start);
	return h;
}

void nvmap_free_iovm(struct nvmap_client *client, struct nvmap_handle_ref *r)
{
	ion_free(client, r);
}

struct nvmap_handle *nvmap_handle_get(struct nvmap_handle *h)
{
	ion_handle_get(h);
	return h;
}

void nvmap_handle_put(struct nvmap_handle *h)
{
	ion_handle_put(h);
}

#endif
