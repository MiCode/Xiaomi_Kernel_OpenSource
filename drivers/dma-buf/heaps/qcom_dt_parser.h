/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _QCOM_DT_PARSER_H
#define _QCOM_DT_PARSER_H

#include <linux/device.h>
#include <linux/platform_device.h>

#include "heap-helpers.h"

/**
 * struct platform_heap - defines a heap in the given platform
 * @type:	type of the heap
 * @name:	used for debug purposes
 * @base:	base address of heap in physical memory if applicable
 * @size:	size of the heap in bytes if applicable
 * @dev:	the device associated with the heap's DT node
 * @is_dynamic:	indicates if memory can be added or removed from carveout heaps
 * @token:	the end points to which memory for secure carveout memory is
 *		assigned to
 *
 * Provided by the board file.
 */
struct platform_heap {
	u32 type;
	const char *name;
	phys_addr_t base;
	size_t size;
	struct device *dev;
	bool is_dynamic;
	u32 token;
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

#ifdef CONFIG_QCOM_DMABUF_HEAPS_CMA
struct platform_data *parse_heap_dt(struct platform_device *pdev);
void free_pdata(const struct platform_data *pdata);
#else
static struct platform_data *parse_heap_dt(struct platform_device *pdev)
{
	return NULL;
}
static void free_pdata(const struct platform_data *pdata)
{

}
#endif


#endif /* _QCOM_DT_PARSER_H */
