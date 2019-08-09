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

#ifdef PMEM_MOCK_OBJECT_SUPPORT
/**********************************************************/
/***************** MOCK SUPPORT ***************************/
/**********************************************************/

/* Use in development phase only (SSMR is mock) */
#define PMEM_FORCE_MOCK_SSMR_SUPPORT (0)
#if PMEM_FORCE_MOCK_SSMR_SUPPORT
#define PMEM_MOCK_SSMR
#endif

/* Use in development phase only (MTEE is mock) */
#define PMEM_FORCE_MOCK_MTEE_SUPPORT (0)
#if PMEM_FORCE_MOCK_MTEE_SUPPORT
#define PMEM_MOCK_MTEE
#else
#if !defined(CONFIG_MTK_GZ_KREE)
#define PMEM_MOCK_MTEE
#endif /* end of #if !defined(CONFIG_MTK_GZ_KREE) */
#endif /* end of #if PMEM_FORCE_MOCK_MTEE_SUPPORT */

#endif /* end of #ifdef PMEM_MOCK_OBJECT_SUPPORT */

/**********************************************************/
/**********************************************************/
/**********************************************************/
#define PMEM_MTEE_SESSION_KEEP_ALIVE_SUPPORT (0)
#if PMEM_MTEE_SESSION_KEEP_ALIVE_SUPPORT
#define PMEM_MTEE_SESSION_KEEP_ALIVE
#endif

#define PMEM_ALIGNMENT_CHECK_ON_CLIENT_SIDE (1)
#if PMEM_ALIGNMENT_CHECK_ON_CLIENT_SIDE
#define PMEM_ALIGNMENT_CHECK
#endif

#define PMEM_MINIMAL_SIZE_CHECK_ON_CLIENT_SIDE (1)
#if PMEM_MINIMAL_SIZE_CHECK_ON_CLIENT_SIDE
#define PMEM_MIN_SIZE_CHECK
#endif

#define PMEM_64BIT_PHYS_SHIFT (10)
#define PMEM_PHYS_LIMIT_MIN_ALLOC_SIZE (1 << PMEM_64BIT_PHYS_SHIFT)

#if defined(PMEM_MOCK_MTEE)
/* for mock buddy memmgr */
#define PMEM_MIN_ALLOC_CHUNK_SIZE SIZE_1K
#else
/* for GZ */
#define PMEM_MIN_ALLOC_CHUNK_SIZE SIZE_4K
#endif

#endif /* end of PMEM_PLAT_H */
