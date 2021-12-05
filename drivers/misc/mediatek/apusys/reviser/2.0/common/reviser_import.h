/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_REVISER_IMPORT_H__
#define __APUSYS_REVISER_IMPORT_H__
#include <linux/types.h>

#include "reviser_mem_def.h"

int reviser_alloc_slb(uint32_t type, uint32_t size, uint64_t *ret_addr, uint64_t *ret_size);
int reviser_free_slb(uint32_t type, uint64_t addr);
#endif
