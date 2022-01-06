/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __GPU_BM_H__
#define __GPU_BM_H__

//void MTKGPUQoS_setup(uint32_t *cpuaddr, phys_addr_t phyaddr, size_t size);
void MTKGPUQoS_setup(struct v1_data *v1, phys_addr_t phyaddr, size_t size);

#endif

