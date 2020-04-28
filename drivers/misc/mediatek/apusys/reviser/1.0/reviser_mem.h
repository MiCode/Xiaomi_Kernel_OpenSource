/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_REVISER_MEM_H__
#define __APUSYS_REVISER_MEM_H__
#include <linux/types.h>

#include "reviser_mem_def.h"




int reviser_mem_free(struct reviser_mem *mem);
int reviser_mem_alloc(struct device *dev, struct reviser_mem *mem);
int reviser_mem_invalidate(struct device *dev, struct reviser_mem *mem);


#endif
