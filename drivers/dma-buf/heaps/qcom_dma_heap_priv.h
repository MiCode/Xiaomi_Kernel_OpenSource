/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _QCOM_DMA_HEAP_PRIV_H
#define _QCOM_DMA_HEAP_PRIV_H

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/device.h>

/**
 * struct platform_heap - defines a heap in the given platform
 * @type:	type of the heap
 * @name:	used for debug purposes
 * @base:	base address of heap in physical memory if applicable
 * @size:	size of the heap in bytes if applicable
 * @priv:	private info passed from the board file
 *
 * Provided by the board file.
 */
struct platform_heap {
	u32 type;
	const char *name;
	phys_addr_t base;
	size_t size;
	struct device *dev;
};

/**
 * struct platform_data - array of platform heaps passed from board file
 * @nr:    number of structures in the array
 * @heaps: array of platform_heap structions
 *
 * Provided by the board file in the form of platform data to a platform device.
 */
struct platform_data {
	int nr;
	struct platform_heap *heaps;
};

struct platform_data *parse_heap_dt(struct platform_device *pdev);

void free_pdata(const struct platform_data *pdata);

int qcom_system_heap_create(void);

#endif /* _QCOM_DMA_HEAP_PRIV_H */
