/* SPDX-License-Identifier: GPL-2.0 */
/*
 * drivers/staging/android/mtk_ion/ion_of.h
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author Andrew Andrianov <andrew@ncrmnt.org>
 */

#ifndef _ION_OF_H
#define _ION_OF_H

struct ion_of_heap {
	const char *compat;
	int heap_id;
	int type;
	const char *name;
	int align;
};

#define PLATFORM_HEAP(_compat, _id, _type, _name) \
{ \
	.compat = _compat, \
	.heap_id = _id, \
	.type = _type, \
	.name = _name, \
	.align = PAGE_SIZE, \
}

struct ion_platform_data *ion_parse_dt(struct platform_device *pdev,
				       struct ion_of_heap *compatible);

void ion_destroy_platform_data(struct ion_platform_data *data);

#endif
