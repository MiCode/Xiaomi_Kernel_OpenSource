/*
 * include/linux/tegra_ion.h
 *
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

#include <linux/types.h>
#include <linux/ion.h>

#if !defined(__KERNEL__)
#define __user
#endif

#ifndef _LINUX_TEGRA_ION_H
#define _LINUX_TEGRA_ION_H

struct tegra_ion_id_data {
	struct ion_handle *handle;
	unsigned long id;
	size_t size;
};

struct tegra_ion_pin_data {
	struct ion_handle **handles; /* array of handles to pin/unpin */
	unsigned long *addr; /* array pf addresses to return */
	unsigned long count; /* number of entries in handles */
};

/* Cache operations. */
enum {
	TEGRA_ION_CACHE_OP_WB = 0,
	TEGRA_ION_CACHE_OP_INV,
	TEGRA_ION_CACHE_OP_WB_INV,
};

struct tegra_ion_cache_maint_data {
	unsigned long addr;
	struct ion_handle *handle;
	size_t len;
	unsigned int op;
};

struct tegra_ion_rw_data {
	unsigned long addr; /* user pointer*/
	struct ion_handle *handle;
	unsigned int offset; /* offset into handle mem */
	unsigned int elem_size; /* individual atome size */
	unsigned int mem_stride; /*delta in bytes between atoms in handle mem*/
	unsigned int user_stride; /* delta in bytes between atoms in user */
	unsigned int count; /* number of atoms to copy */
};

struct tegra_ion_get_params_data {
	struct ion_handle *handle;
	size_t size;
	unsigned int align;
	unsigned int heap;
	unsigned long addr;
};

/* Custom Ioctl's. */
enum {
	TEGRA_ION_ALLOC_FROM_ID = 0,
	TEGRA_ION_GET_ID,
	TEGRA_ION_PIN,
	TEGRA_ION_UNPIN,
	TEGRA_ION_CACHE_MAINT,
	TEGRA_ION_READ,
	TEGRA_ION_WRITE,
	TEGRA_ION_GET_PARAM,
};

/* List of heaps in the system. */
enum {
	TEGRA_ION_HEAP_CARVEOUT = 0,
	TEGRA_ION_HEAP_IRAM,
	TEGRA_ION_HEAP_VPR,
	TEGRA_ION_HEAP_IOMMU
};

#endif /* _LINUX_TEGRA_ION_H */
