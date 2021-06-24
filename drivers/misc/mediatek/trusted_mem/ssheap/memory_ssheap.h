/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MEMORY_SSHEAP_H__
#define __MEMORY_SSHEAP_H__

#include <linux/platform_device.h>
#include "../private/tmem_device.h"

#define WITH_SSHEAP_PROC 1

int ssheap_init(struct platform_device *pdev);
int ssheap_exit(struct platform_device *pdev);

#endif
