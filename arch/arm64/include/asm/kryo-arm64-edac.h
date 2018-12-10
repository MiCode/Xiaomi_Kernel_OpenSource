/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef ASM_KRYO_EDAC_H
#define ASM_KRYO_EDAC_H

#if defined(CONFIG_EDAC_KRYO_ARM64)
void kryo_poll_cache_errors(void *info);
#else
static inline void kryo_poll_cache_errors(void *info) { }
#endif

#endif
