// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPU_BM_H__
#define __GPU_BM_H__

void MTKGPUQoS_mode(void);
void MTKGPUQoS_setup(struct v1_data *v1, phys_addr_t phyaddr, size_t size);
int MTKGPUQoS_is_inited(void);

#endif

