/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#ifndef _WRITEBACK_UTIL_H_
#define _WRITEBACK_UTIL_H_

#define DEBUG

#ifdef DEBUG
	#define WRITEBACK_MSG_INFO(fmt...) pr_info(fmt)
	#define WRITEBACK_MSG_WARN(fmt...) pr_warning(fmt)
#else
	#define WRITEBACK_MSG_INFO(fmt...)
	#define WRITEBACK_MSG_WARN(fmt...)
#endif
	#define WRITEBACK_MSG_ERR(fmt...) pr_err(fmt)
	#define WRITEBACK_MSG_CRIT(fmt...) pr_crit(fmt)
#endif
