/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __TEEI_DEBUG_H_
#define __TEEI_DEBUG_H_

/* #define TEEI_DEBUG */
/* #define TEEI_INFO */

#undef TDEBUG
#undef TINFO
#undef TERR

#define TZDebug(fmt, args...) \
		pr_info("\033[;34m[TZDriver]"fmt"\033[0m\n", ##args)
#ifdef TEEI_DEBUG
/*
 * #define TDEBUG(fmt, args...) pr_info("%s(%i, %s): " fmt "\n", \
 *	__func__, current->pid, current->comm, ##args)
 */
#define TDEBUG(fmt, args...) pr_info("tz driver"fmt"\n", ##args)
#else
#define TDEBUG(fmt, args...)
#endif

#ifdef TEEI_INFO
#define TINFO(fmt, args...) pr_info("%s(%i, %s): " fmt "\n", \
	__func__, current->pid, current->comm, ##args)
#else
#define TINFO(fmt, args...)
#endif

#define TERR(fmt, args...) pr_notice("%s(%i, %s): " fmt "\n", \
	__func__, current->pid, current->comm, ##args)

#endif
