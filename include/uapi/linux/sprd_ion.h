/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _UAPI_SPRD_ION_H
#define _UAPI_SPRD_ION_H
#include <linux/ion.h>
#include <linux/sprd_ion.h>

enum sprd_ion_heap_ids {
	ION_HEAP_ID_SYSTEM = 0,
	ION_HEAP_ID_MM,
	ION_HEAP_ID_OVERLAY,
	ION_HEAP_ID_FB,
	ION_HEAP_ID_CAM,
	ION_HEAP_ID_VDSP,
};

#define ION_HEAP_ID_MASK_SYSTEM        (1<<ION_HEAP_ID_SYSTEM)
#define ION_HEAP_ID_MASK_MM            (1<<ION_HEAP_ID_MM)
#define ION_HEAP_ID_MASK_OVERLAY       (1<<ION_HEAP_ID_OVERLAY)
#define ION_HEAP_ID_MASK_FB            (1<<ION_HEAP_ID_FB)
#define ION_HEAP_ID_MASK_CAM           (1<<ION_HEAP_ID_CAM)
#define ION_HEAP_ID_MASK_VDSP          (1<<ION_HEAP_ID_VDSP)

#define ION_FLAG_SECURE  (1<<31)
#define ION_FLAG_NO_CLEAR (1 << 16)

#define ION_IOC_PHY           _IOWR(ION_IOC_MAGIC, 11, \
					struct ion_phy_data)
#endif /* _UAPI_SPRD_ION_H */
