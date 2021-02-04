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

#ifndef SDSP_SMEM_PLAT_H
#define SDSP_SMEM_PLAT_H

#include "private/tmem_utils.h"

/**********************************************************/
/**********************************************************/
/**********************************************************/
#define SDSP_SMEM_TEE_SESSION_KEEP_ALIVE_SUPPORT (0)
#if SDSP_SMEM_TEE_SESSION_KEEP_ALIVE_SUPPORT
#define SDSP_SMEM_TEE_SESSION_KEEP_ALIVE
#endif

#define SDSP_SMEM_ALIGNMENT_CHECK_ON_CLIENT_SIDE (1)
#if SDSP_SMEM_ALIGNMENT_CHECK_ON_CLIENT_SIDE
#define SDSP_SMEM_ALIGNMENT_CHECK
#endif

#define SDSP_SMEM_MINIMAL_SIZE_CHECK_ON_CLIENT_SIDE (0)
#if SDSP_SMEM_MINIMAL_SIZE_CHECK_ON_CLIENT_SIDE
#define SDSP_SMEM_MIN_SIZE_CHECK
#endif

/* for TEE SDSP secure memory */
#define SDSP_SMEM_MIN_ALLOC_CHUNK_SIZE SIZE_64K

#define SDSP_SMEM_64BIT_PHYS_SHIFT (6)
#define SDSP_SMEM_PHYS_LIMIT_MIN_ALLOC_SIZE (1 << SDSP_SMEM_64BIT_PHYS_SHIFT)

#endif /* end of SDSP_SMEM_PLAT_H */
