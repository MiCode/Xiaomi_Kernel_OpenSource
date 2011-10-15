/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _WFD_UTIL_H_
#define _WFD_UTIL_H_

/*#define DEBUG_WFD*/

#ifdef DEBUG_WFD
	#define WFD_MSG_INFO(fmt...) pr_info(fmt)
	#define WFD_MSG_WARN(fmt...) pr_warning(fmt)
	#define WFD_MSG_DBG(fmt...) pr_debug(fmt)
#else
	#define WFD_MSG_INFO(fmt...)
	#define WFD_MSG_WARN(fmt...)
	#define WFD_MSG_DBG(fmt...)
#endif
	#define WFD_MSG_ERR(fmt...) pr_err(KERN_ERR fmt)
	#define WFD_MSG_CRIT(fmt...) pr_crit(KERN_CRIT fmt)
#endif
