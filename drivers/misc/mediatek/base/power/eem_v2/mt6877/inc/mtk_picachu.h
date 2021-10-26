/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef __MTK_PICACHU_H__
#define __MTK_PICACHU_H__
#include <linux/types.h> //for phys_addr_t

phys_addr_t picachu_reserve_mem_get_virt(unsigned int id);
phys_addr_t picachu_reserve_mem_get_size(unsigned int id);
#endif
