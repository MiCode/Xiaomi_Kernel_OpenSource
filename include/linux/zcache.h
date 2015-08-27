/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#ifndef _LINUX_ZCACHE_H
#define _LINUX_ZCACHE_H

#ifdef CONFIG_ZCACHE
extern u64 zcache_pages(void);
#else
u64 zcache_pages(void) { return 0; }
#endif

#endif /* _LINUX_ZCACHE_H */
