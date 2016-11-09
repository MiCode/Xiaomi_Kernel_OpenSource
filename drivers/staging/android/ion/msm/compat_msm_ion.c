/* Copyright (c) 2014,2016, The Linux Foundation. All rights reserved.
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

#include <linux/compat.h>
#include <linux/ion.h>
#include <linux/msm_ion.h>
#include <linux/uaccess.h>
#include "../ion_priv.h"
#include "../compat_ion.h"

struct compat_ion_flush_data {
	compat_ion_user_handle_t handle;
	compat_int_t fd;
	compat_uptr_t vaddr;
	compat_uint_t offset;
	compat_uint_t length;
};

struct compat_ion_prefetch_regions {
	compat_uint_t vmid;
	compat_uptr_t sizes;
	compat_uint_t nr_sizes;
};

struct compat_ion_prefetch_data {
	compat_int_t heap_id;
	compat_ulong_t len;
	compat_uptr_t regions;
	compat_uint_t nr_regions;
};

#define COMPAT_ION_IOC_CLEAN_CACHES    _IOWR(ION_IOC_MSM_MAGIC, 0, \
						struct compat_ion_flush_data)
#define COMPAT_ION_IOC_INV_CACHES      _IOWR(ION_IOC_MSM_MAGIC, 1, \
						struct compat_ion_flush_data)
#define COMPAT_ION_IOC_CLEAN_INV_CACHES        _IOWR(ION_IOC_MSM_MAGIC, 2, \
						struct compat_ion_flush_data)
#define COMPAT_ION_IOC_PREFETCH                _IOWR(ION_IOC_MSM_MAGIC, 3, \
						struct compat_ion_prefetch_data)
#define COMPAT_ION_IOC_DRAIN                   _IOWR(ION_IOC_MSM_MAGIC, 4, \
						struct compat_ion_prefetch_data)

static int compat_get_ion_flush_data(
			struct compat_ion_flush_data __user *data32,
			struct ion_flush_data __user *data)
{
	compat_ion_user_handle_t h;
	compat_int_t i;
	compat_uptr_t u;
	compat_ulong_t l;
	int err;

	err = get_user(h, &data32->handle);
	err |= put_user(h, &data->handle);
	err |= get_user(i, &data32->fd);
	err |= put_user(i, &data->fd);
	err |= get_user(u, &data32->vaddr);
	/* upper bits won't get set, zero them */
	err |= put_user(NULL, &data->vaddr);
	err |= put_user(u, (compat_uptr_t *)&data->vaddr);
	err |= get_user(l, &data32->offset);
	err |= put_user(l, &data->offset);
	err |= get_user(l, &data32->length);
	err |= put_user(l, &data->length);

	return err;
}

static int compat_get_ion_prefetch_data(
			struct compat_ion_prefetch_data __user *data32,
			struct ion_prefetch_data __user *data,
			size_t stack_offset)
{
	compat_int_t i;
	compat_ulong_t l;
	compat_uint_t u;
	int err, j, k;
	compat_uint_t nr_regions, nr_sizes;
	struct compat_ion_prefetch_regions __user *regions32;
	struct ion_prefetch_regions __user *regions;
	compat_uptr_t ptr;

	err = get_user(i, &data32->heap_id);
	err |= put_user(i, &data->heap_id);
	err |= get_user(l, &data32->len);
	err |= put_user(l, &data->len);
	err |= get_user(nr_regions, &data32->nr_regions);
	err |= put_user(nr_regions, &data->nr_regions);
	err |= get_user(ptr, &data32->regions);
	regions32 = compat_ptr(ptr);
	if (err)
		return err;

	stack_offset += nr_regions * sizeof(*regions);
	regions = compat_alloc_user_space(stack_offset);
	if (!regions)
		return -EFAULT;
	err |= put_user(regions, &data->regions);

	for (k = 0; k < nr_regions; k++) {
		compat_size_t __user *sizes32;
		size_t __user *sizes;

		err |= get_user(u, &regions32[k].vmid);
		err |= put_user(u, &regions[k].vmid);
		err |= get_user(nr_sizes, &regions32[k].nr_sizes);
		err |= put_user(nr_sizes, &regions[k].nr_sizes);
		err |= get_user(ptr, &regions32[k].sizes);
		sizes32 = compat_ptr(ptr);
		if (err)
			return -EFAULT;

		stack_offset += nr_sizes * sizeof(*sizes);
		sizes = compat_alloc_user_space(stack_offset);
		if (!sizes)
			return -EFAULT;
		err |= put_user(sizes, &regions[k].sizes);

		for (j = 0; j < nr_sizes; j++) {
			compat_size_t s;

			err |= get_user(s, &sizes32[j]);
			err |= put_user(s, &sizes[j]);
		}
	}

	return err;
}

static unsigned int convert_cmd(unsigned int cmd)
{
	switch (cmd) {
	case COMPAT_ION_IOC_CLEAN_CACHES:
		return ION_IOC_CLEAN_CACHES;
	case COMPAT_ION_IOC_INV_CACHES:
		return ION_IOC_INV_CACHES;
	case COMPAT_ION_IOC_CLEAN_INV_CACHES:
		return ION_IOC_CLEAN_INV_CACHES;
	case COMPAT_ION_IOC_PREFETCH:
		return ION_IOC_PREFETCH;
	case COMPAT_ION_IOC_DRAIN:
		return ION_IOC_DRAIN;
	default:
		return cmd;
	}
}

long compat_msm_ion_ioctl(struct ion_client *client, unsigned int cmd,
			  unsigned long arg)
{
	switch (cmd) {
	case COMPAT_ION_IOC_CLEAN_CACHES:
	case COMPAT_ION_IOC_INV_CACHES:
	case COMPAT_ION_IOC_CLEAN_INV_CACHES:
	{
		struct compat_ion_flush_data __user *data32;
		struct ion_flush_data __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (!data)
			return -EFAULT;

		err = compat_get_ion_flush_data(data32, data);
		if (err)
			return err;

		return msm_ion_custom_ioctl(client, convert_cmd(cmd),
						(unsigned long)data);
	}
	case COMPAT_ION_IOC_PREFETCH:
	case COMPAT_ION_IOC_DRAIN:
	{
		struct compat_ion_prefetch_data __user *data32;
		struct ion_prefetch_data __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (!data)
			return -EFAULT;

		err = compat_get_ion_prefetch_data(data32, data, sizeof(*data));
		if (err)
			return err;

		return msm_ion_custom_ioctl(client, convert_cmd(cmd),
						(unsigned long)data);
	}
	default:
		if (is_compat_task())
			return -ENOIOCTLCMD;
		else
			return msm_ion_custom_ioctl(client, cmd, arg);
	}
}
