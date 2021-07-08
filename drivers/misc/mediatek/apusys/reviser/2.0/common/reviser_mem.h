/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_REVISER_MEM_H__
#define __APUSYS_REVISER_MEM_H__
#include <linux/types.h>

#include "reviser_mem_def.h"


void reviser_print_dram(void *drvinfo, void *s_file);
void reviser_print_tcm(void *drvinfo, void *s_file);
int reviser_dram_remap_init(void *drvinfo);
int reviser_dram_remap_destroy(void *drvinfo);
int reviser_mem_free(struct device *dev, struct reviser_mem *mem, bool fix);
int reviser_mem_alloc(struct device *dev, struct reviser_mem *mem, bool fix);
int reviser_mem_invalidate(struct device *dev, struct reviser_mem *mem);


#endif
