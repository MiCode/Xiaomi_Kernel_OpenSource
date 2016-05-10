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

/*
 *
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __POL_DEBUG_H__
#define __POL_DEBUG_H__

#define LOGOFF  0
#define LOGP    1
#define LOGE    2
#define LOGW    3
#define LOG1    4
#define LOG2    5
#define LOG3    6
#define LOG4    7

#ifdef ANI_DEBUG
#define PMM_LOG_LEVEL LOG4
#define SCH_LOG_LEVEL LOG4
#define ARQ_LOG_LEVEL LOG4
#define LIM_LOG_LEVEL LOG4
#define HAL_LOG_LEVEL LOG4
#define SYS_LOG_LEVEL LOG4
#define CFG_LOG_LEVEL LOG4
#define DPH_LOG_LEVEL LOG4
#else
#define PMM_LOG_LEVEL LOGW
#define SCH_LOG_LEVEL LOGW
#define ARQ_LOG_LEVEL LOGW
#define LIM_LOG_LEVEL LOGW
#define HAL_LOG_LEVEL LOGW
#define SYS_LOG_LEVEL LOGW
#define CFG_LOG_LEVEL LOGW
#define DPH_LOG_LEVEL LOGW
#endif


#ifdef  WLAN_MDM_CODE_REDUCTION_OPT
#ifdef PE_DEBUG_LOGE
#define PELOGE(p) { p }
#else
#define PELOGE(p) { }
#endif

#ifdef PE_DEBUG_LOGW
#define PELOGW(p) { p }
#else
#define PELOGW(p) { }
#endif

#define PELOG1(p) { }
#define PELOG2(p) { }
#define PELOG3(p) { }
#define PELOG4(p) { }


#else /* WLAN_MDM_CODE_REDUCTION_OPT */

#ifdef PE_DEBUG_LOGE
#define PELOGE(p) { p }
#else
#define PELOGE(p) { }
#endif

#ifdef PE_DEBUG_LOGW
#define PELOGW(p) { p }
#else
#define PELOGW(p) { }
#endif

#ifdef PE_DEBUG_LOG1
#define PELOG1(p) { p }
#else
#define PELOG1(p) { }
#endif

#ifdef PE_DEBUG_LOG2
#define PELOG2(p) { p }
#else
#define PELOG2(p) { }
#endif

#ifdef PE_DEBUG_LOG3
#define PELOG3(p) { p }
#else
#define PELOG3(p) { }
#endif

#ifdef PE_DEBUG_LOG4
#define PELOG4(p) { p }
#else
#define PELOG4(p) { }
#endif

#endif /* WLAN_MDM_CODE_REDUCTION_OPT */

#define FL(x)    "%s: %d: "\
                 x, __func__, __LINE__

#define MAC_ADDR_ARRAY(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MAC_ADDRESS_STR "%02x:%02x:%02x:%02x:%02x:%02x"

#endif
