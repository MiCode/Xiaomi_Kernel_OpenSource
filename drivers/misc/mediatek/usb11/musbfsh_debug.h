/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MUSBFSH_LINUX_DEBUG_H__
#define __MUSBFSH_LINUX_DEBUG_H__

/* for normal log, very detail, impact performance a lot*/
extern int musbfsh_debug;
#define yprintk(facility, format, args...)	do { \
		if (musbfsh_debug) { \
			pr_notice("[MUSBFSH] %s %d: " format, \
					__func__, __LINE__, ## args); \
		} \
	} while (0)

#define INFO(fmt, args...) yprintk(KERN_NOTICE, fmt, ## args)

/* for critical log */
#define zprintk(facility, format, args...) \
		pr_notice("[MUSBFSH] %s %d: " format, \
				__func__, __LINE__, ## args)

#define WARNING(fmt, args...) zprintk(KERN_WARNING, fmt, ## args)
#define ERR(fmt, args...) zprintk(KERN_ERR, fmt, ## args)
extern int musbfsh_init_debugfs(struct musbfsh *musb);
extern void musbfsh_exit_debugfs(struct musbfsh *musb);

#endif

