/**
 *
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MACH_ION_H_
#define __MACH_ION_H_

enum ion_memory_types {
	ION_EBI_TYPE,
	ION_SMI_TYPE,
};

enum ion_permission_type {
	IPT_TYPE_MM_CARVEOUT = 0,
	IPT_TYPE_MFC_SHAREDMEM = 1,
	IPT_TYPE_MDP_WRITEBACK = 2,
};

#endif
