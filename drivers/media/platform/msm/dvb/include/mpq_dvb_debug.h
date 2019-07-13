/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _MPQ_DVB_DEBUG_H
#define _MPQ_DVB_DEBUG_H

/* Enable this line if you want to output debug printouts */
#define MPG_DVB_DEBUG_ENABLE

#undef MPQ_DVB_DBG_PRINT		/* undef it, just in case */

#ifdef MPG_DVB_DEBUG_ENABLE
#define MPQ_DVB_ERR_PRINT(fmt, args...) pr_err(fmt, ## args)
#define MPQ_DVB_WARN_PRINT(fmt, args...) pr_warn(fmt, ## args)
#define MPQ_DVB_NOTICE_PRINT(fmt, args...) pr_notice(fmt, ## args)
#define MPQ_DVB_DBG_PRINT(fmt, args...) pr_debug(fmt, ## args)
#else  /* MPG_DVB_DEBUG_ENABLE */
#define MPQ_DVB_ERR_PRINT(fmt, args...)
#define MPQ_DVB_WARN_PRINT(fmt, args...)
#define MPQ_DVB_NOTICE_PRINT(fmt, args...)
#define MPQ_DVB_DBG_PRINT(fmt, args...)
#endif /* MPG_DVB_DEBUG_ENABLE */


/*
 * The following can be used to disable specific printout
 * by adding a letter to the end of MPQ_DVB_DBG_PRINT
 */
#undef MPQ_DVB_DBG_PRINTT
#define MPQ_DVB_DBG_PRINTT(fmt, args...)

#endif /* _MPQ_DVB_DEBUG_H */
