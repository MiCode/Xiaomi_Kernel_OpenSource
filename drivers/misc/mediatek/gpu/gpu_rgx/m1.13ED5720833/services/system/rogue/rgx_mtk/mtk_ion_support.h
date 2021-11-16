/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifdef MTK_USE_IMG_ION_IMPLEMENTATION
#else
extern struct ion_device *g_ion_device;
#endif /* MTK_USE_IMG_ION_IMPLEMENTATION */

struct ion_client *MTKGetIonClient(void);
