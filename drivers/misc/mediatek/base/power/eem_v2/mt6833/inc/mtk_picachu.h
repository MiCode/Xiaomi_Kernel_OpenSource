/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#ifndef __MTK_PICACHU_H__
#define __MTK_PICACHU_H__
#include <linux/types.h> //for phys_addr_t

phys_addr_t picachu_reserve_mem_get_virt(unsigned int id);
phys_addr_t picachu_reserve_mem_get_size(unsigned int id);
#endif
