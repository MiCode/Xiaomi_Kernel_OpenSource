/*
 *
 * (C) COPYRIGHT 2011-2020 ARM Limited. All rights reserved.
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



/**
 * @file mali_kbase_js.h
 * Job Scheduler APIs.
 */

#ifndef _KBASE_JS_H_
#define _KBASE_JS_H_

#include "mali_kbase_js_defs.h"
#include "context/mali_kbase_context.h"
#include "mali_kbase_defs.h"
#include "mali_kbase_debug.h"
#include <mali_kbase_ctx_sched.h>

#include <jm/mali_kbase_jm_js.h>

/**
 * kbasep_js_runpool_retain_ctx_nolock - Refcount a context as being busy,
 *                                       preventing it from being scheduled
 *                                       out.
 *
 * This function can safely be called from IRQ context.
 *
 * The following locks must be held by the caller:
 * * mmu_hw_mutex, hwaccess_lock
 *
 * Return: value true if the retain succeeded, and the context will not be
 * scheduled out, otherwise false if the retain failed (because the context
 * is being/has been scheduled out).
 */
bool kbasep_js_runpool_retain_ctx_nolock(struct kbase_device *kbdev,
		struct kbase_context *kctx);

/**
 * kbasep_js_runpool_retain_ctx - Refcount a context as being busy, preventing
 *                                it from being scheduled out.
 *
 * This function can safely be called from IRQ context.
 *
 * The following locking conditions are made on the caller:
 * * it must not hold mmu_hw_mutex and hwaccess_lock, because they will be
 *   used internally.
 *
 * Return: value true if the retain succeeded, and the context will not be
 * scheduled out, otherwise false if the retain failed (because the context
 * is being/has been scheduled out).
 */
bool kbasep_js_runpool_retain_ctx(struct kbase_device *kbdev,
		struct kbase_context *kctx);

/**
 * kbasep_js_runpool_lookup_ctx - Lookup a context in the Run Pool based upon
 *                                its current address space and ensure that
 *                                is stays scheduled in.
 *
 * The context is refcounted as being busy to prevent it from scheduling
 * out. It must be released with kbasep_js_runpool_release_ctx() when it is no
 * longer required to stay scheduled in.
 *
 * This function can safely be called from IRQ context.
 *
 * The following locking conditions are made on the caller:
 * * it must not hold the hwaccess_lock, because it will be used internally.
 *   If the hwaccess_lock is already held, then the caller should use
 *   kbasep_js_runpool_lookup_ctx_nolock() instead.
 *
 * Return: a valid struct kbase_context on success, which has been refcounted
 * as being busy or return NULL on failure, indicating that no context was found
 * in as_nr.
 */
struct kbase_context *kbasep_js_runpool_lookup_ctx(struct kbase_device *kbdev,
		int as_nr);

/**
 * kbasep_js_runpool_lookup_ctx_noretain - Variant of
 * kbasep_js_runpool_lookup_ctx() that can be used when the
 * context is guaranteed to be already previously retained.
 *
 * It is a programming error to supply the as_nr of a context that has not
 * been previously retained/has a busy refcount of zero. The only exception is
 * when there is no ctx in as_nr (NULL returned).
 *
 * The following locking conditions are made on the caller:
 * * it must not hold the hwaccess_lock, because it will be used internally.
 *
 * Return: a valid struct kbase_context on success, with a refcount that is
 * guaranteed to be non-zero and unmodified by this function or
 * return NULL on failure, indicating that no context was found in as_nr.
 */
static inline struct kbase_context *kbasep_js_runpool_lookup_ctx_noretain(
		struct kbase_device *kbdev, int as_nr)
{
	struct kbase_context *found_kctx;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	KBASE_DEBUG_ASSERT(0 <= as_nr && as_nr < BASE_MAX_NR_AS);

	found_kctx = kbdev->as_to_kctx[as_nr];
	KBASE_DEBUG_ASSERT(found_kctx == NULL ||
			atomic_read(&found_kctx->refcount) > 0);

	return found_kctx;
}

#endif	/* _KBASE_JS_H_ */
