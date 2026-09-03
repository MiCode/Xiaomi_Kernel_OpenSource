/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_OSAL_H_
#define _SPRD_OSAL_H_

#include "../inc/sprd_defs.h"

void *sprd_malloc(u32 size);
void sprd_free(void *addr);
void sprd_memset(void *addr, u32 balue, u32 size);
void sprd_memset_orig(void *addr, u32 balue, u32 size);
void sprd_memcpy(void *dstaddr, void *srcaddr, u32 size);
void *sprd_aligned_malloc(u32 size, u32 align);
void sprd_aligned_free(void *addr);

#endif

