/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MICROVISOR_H_
#define _MICROVISOR_H_

/**
 * @defgroup lib_microvisor The Microvisor Library
 *
 * @{
 *
 * The Microvisor Library is the primary low-level API between the OKL4
 * Microvisor and a Cell application or guest-OS. It also provides certain
 * common data types such as structure definitions used in these interactions.
 *
 */

/**
 * Temporarily define _Bool to allow C++ compilation of
 * OKL code that makes use of it.
 */
#if defined(__cplusplus) && !defined(_Bool)
#define _OKL4_CPP_BOOL
#define _Bool bool
#endif

#define OKL4_INLINE static inline

#if defined(_lint) || defined(_splint)
#define OKL4_FORCE_INLINE static
#else
#define OKL4_FORCE_INLINE static inline __attribute__((always_inline))
#endif

#include <microvisor/kernel/types.h>
#include <microvisor/kernel/microvisor.h>
#include <microvisor/kernel/syscalls.h>
#include <microvisor/kernel/offsets.h>

/** @} */

/**
 * Remove temporary definition of _Bool if it was defined
 */
#if defined(_OKL4_CPP_BOOL)
#undef _Bool
#undef _OKL4_CPP_BOOL
#endif

#endif /* _MICROVISOR_H_ */
