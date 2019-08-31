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

#ifndef __GED_GE_H__
#define __GED_GE_H__

#include <linux/types.h>
#include <ged_type.h>

/* Must be the same as region_num in gralloc_extra/ge_config.h */
#define GE_ALLOC_STRUCT_NUM 10

int ged_ge_init(void);
int ged_ge_exit(void);
int ged_ge_alloc(int region_num, uint32_t *region_sizes);
int ged_ge_get(int ge_fd, int region_id, int u32_offset, int u32_size, uint32_t *output_data);
int ged_ge_set(int ge_fd, int region_id, int u32_offset, int u32_size, uint32_t *input_data);

#endif
