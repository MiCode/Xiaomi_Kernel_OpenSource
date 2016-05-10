/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DOT11FDEFS_H_82A7B72E_C36C_465D_82A7_139EA5322582
#define DOT11FDEFS_H_82A7B72E_C36C_465D_82A7_139EA5322582
/**
 * \file dot11fdefs.h
 *
 * \brief C defines customizing our framesc-generated code
 *
 *
 *
 * Copyright (C) 2006 Airgo Networks, Incorporated
 *
 * 'framesc' generates code written in terms of a number of macros
 * intended for customization.
 *
 *
 */

#include "parserApi.h"

// This controls how the "dot11f" code copies memory
#define DOT11F_MEMCPY(ctx, dst, src, len) \
    vos_mem_copy( ( tANI_U8* )( dst ), ( tANI_U8* )( src ), ( len ) )

// This controls how the "dot11f" code compares memory
#define DOT11F_MEMCMP(ctx, lhs, rhs, len) \
    ( ! vos_mem_compare( ( tANI_U8* )( lhs ), ( tANI_U8* )( rhs ), ( len ) ) )

#   if defined ( DBG ) && ( DBG != 0 )

#       //define DOT11F_ENABLE_LOGGING
#       //define DOT11F_DUMP_FRAMES
#       define DOT11F_LOG_GATE ( 4 )
#       define FRAMES_SEV_FOR_FRAME(ctx, sig) \
            ( DOT11F_ASSOCREQUEST == (sig) ? 3 : 5 )

        #if defined( DOT11F_ENABLE_LOGGING )

#       define DOT11F_HAVE_LOG_MACROS

#       define FRAMES_LOG0(ctx, sev, fmt) \
            dot11fLog((ctx), (sev), (fmt));

#       define FRAMES_LOG1(ctx, sev, fmt, p1) \
            dot11fLog((ctx), (sev), (fmt), (p1));

#       define FRAMES_LOG2(ctx, sev, fmt, p1, p2) \
            dot11fLog((ctx), (sev), (fmt), (p1), (p2));

#       define FRAMES_LOG3(ctx, sev, fmt, p1, p2, p3) \
            dot11fLog((ctx), (sev), (fmt), (p1), (p2), (p3));

#       define FRAMES_DUMP(ctx, sev, p, n) \
            sirDumpBuf((pCtx), SIR_DBG_MODULE_ID, (sev), (p), (n));

        #endif //#if defined( DOT11F_ENABLE_LOGGING )

#   else

#       undef DOT11F_ENABLE_LOGGING
#       undef DOT11F_DUMP_FRAMES
#       define DOT11F_LOG_GATE ( 1 )

#   endif


// #define DOT11F_ENABLE_DBG_BREAK ( 1 )

// Local Variables:
// fill-column: 72
// indent-tabs-mode: nil
// show-trailing-whitespace: t
// End:

#endif // DOT11FDEFS_H_82A7B72E_C36C_465D_82A7_139EA5322582
