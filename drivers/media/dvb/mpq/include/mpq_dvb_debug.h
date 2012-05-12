/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MPQ_DVB_DEBUG_H
#define _MPQ_DVB_DEBUG_H

/* Enable this line if you want to output debug printouts */
#define MPG_DVB_DEBUG_ENABLE

#undef MPQ_DVB_DBG_PRINT		/* undef it, just in case */

#ifdef MPG_DVB_DEBUG_ENABLE
#define MPQ_DVB_DBG_PRINT(fmt, args...) pr_debug(fmt, ## args)
#define MPQ_DVB_ERR_PRINT(fmt, args...) pr_err(fmt, ## args)
#else  /* MPG_DVB_DEBUG_ENABLE */
#define MPQ_DVB_DBG_PRINT(fmt, args...)
#define MPQ_DVB_ERR_PRINT(fmt, args...)
#endif /* MPG_DVB_DEBUG_ENABLE */


/*
 * The following can be used to disable specific printout
 * by adding a letter to the end of MPQ_DVB_DBG_PRINT
 */
#undef MPQ_DVB_DBG_PRINTT
#define MPQ_DVB_DBG_PRINTT(fmt, args...)

#endif /* _MPQ_DVB_DEBUG_H */

