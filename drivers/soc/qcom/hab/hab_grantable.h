/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#ifndef __HAB_GRANTABLE_H
#define __HAB_GRANTABLE_H

/* Grantable should be common between exporter and importer */
struct grantable {
	unsigned long pfn;
};

struct compressed_pfns {
	unsigned long first_pfn;
	int nregions;
	struct region {
		int size;
		int space;
	} region[];
};
#endif /* __HAB_GRANTABLE_H */
