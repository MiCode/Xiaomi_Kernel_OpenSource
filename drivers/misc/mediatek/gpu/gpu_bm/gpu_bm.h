/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __GPU_BM_H__
#define __GPU_BM_H__

//void MTKGPUQoS_setup(uint32_t *cpuaddr, phys_addr_t phyaddr, size_t size);
void MTKGPUQoS_setup(struct v1_data *v1, phys_addr_t phyaddr, size_t size);

#endif

