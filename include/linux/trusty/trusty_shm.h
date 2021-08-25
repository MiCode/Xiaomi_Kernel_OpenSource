/*
 * Copyright 2018 GoldenRiver Technologies Co., Ltd. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_TRUSTY_SHM_H
#define __LINUX_TRUSTY_SHM_H

void *trusty_shm_alloc(size_t size, gfp_t gfp);
void trusty_shm_free(void *virt, size_t size);
int trusty_shm_init_pool(struct device *dev);
void trusty_shm_destroy_pool(struct device *dev);
phys_addr_t trusty_shm_virt_to_phys(void *vaddr);
void *trusty_shm_phys_to_virt(unsigned long addr);

#endif
