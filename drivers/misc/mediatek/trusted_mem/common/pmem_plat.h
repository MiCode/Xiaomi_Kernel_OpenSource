/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef PMEM_PLAT_H
#define PMEM_PLAT_H

#include "private/tmem_utils.h"

/**********************************************************/
/**********************************************************/
/**********************************************************/
#define PMEM_MTEE_SESSION_KEEP_ALIVE_SUPPORT (0)
#if PMEM_MTEE_SESSION_KEEP_ALIVE_SUPPORT
#define PMEM_MTEE_SESSION_KEEP_ALIVE
#endif

#define PMEM_64BIT_PHYS_SHIFT (10)
#define PMEM_PHYS_LIMIT_MIN_ALLOC_SIZE (1 << PMEM_64BIT_PHYS_SHIFT)

#endif /* end of PMEM_PLAT_H */
