// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _KBASE_GATOR_HWCNT_NAMES_H_
#define _KBASE_GATOR_HWCNT_NAMES_H_

/*
 * "Short names" for hardware counters used by Streamline. Counters names are
 * stored in accordance with their memory layout in the binary counter block
 * emitted by the Mali GPU. Each "master" in the GPU emits a fixed-size block
 * of 64 counters, and each GPU implements the same set of "masters" although
 * the counters each master exposes within its block of 64 may vary.
 *
 * Counters which are an empty string are simply "holes" in the counter memory
 * where no counter exists.
 */

#include "mali_kbase_gator_hwcnt_names_ttrx.h"

#include "mali_kbase_gator_hwcnt_names_tnax.h"

#include "mali_kbase_gator_hwcnt_names_tbex.h"

#endif
