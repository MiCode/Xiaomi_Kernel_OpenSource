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

#ifndef WFD_SMEM_PLAT_H
#define WFD_SMEM_PLAT_H

#include "private/tmem_utils.h"

/**********************************************************/
/**********************************************************/
/**********************************************************/
#define WFD_SMEM_TEE_SESSION_KEEP_ALIVE_SUPPORT (0)
#if WFD_SMEM_TEE_SESSION_KEEP_ALIVE_SUPPORT
#define WFD_SMEM_TEE_SESSION_KEEP_ALIVE
#endif

#define WFD_SMEM_ALIGNMENT_CHECK_ON_CLIENT_SIDE (1)
#if WFD_SMEM_ALIGNMENT_CHECK_ON_CLIENT_SIDE
#define WFD_SMEM_ALIGNMENT_CHECK
#endif

#define WFD_SMEM_MINIMAL_SIZE_CHECK_ON_CLIENT_SIDE (0)
#if WFD_SMEM_MINIMAL_SIZE_CHECK_ON_CLIENT_SIDE
#define WFD_SMEM_MIN_SIZE_CHECK
#endif

/* for TEE WFD secure memory */
#define WFD_SMEM_MIN_ALLOC_CHUNK_SIZE SIZE_64K

#define WFD_SMEM_64BIT_PHYS_SHIFT (6)
#define WFD_SMEM_PHYS_LIMIT_MIN_ALLOC_SIZE (1 << WFD_SMEM_64BIT_PHYS_SHIFT)

#endif /* end of WFD_SMEM_PLAT_H */
