/*
 *
 * (C) COPYRIGHT 2014-2018 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
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
