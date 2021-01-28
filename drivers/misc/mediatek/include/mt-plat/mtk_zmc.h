/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/cma.h>
#include <linux/of_reserved_mem.h>

extern phys_addr_t zmc_movable_min;
extern phys_addr_t zmc_movable_max;

extern bool zmc_reserved_mem_inited;
extern bool is_zmc_inited(void);
extern void zmc_get_range(phys_addr_t *base, phys_addr_t *size);
