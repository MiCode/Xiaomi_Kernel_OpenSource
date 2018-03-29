/*
* Copyright (C) 2016 MediaTek Inc.
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef __MT_CACHE_DUMP_H__
#define __MT_CACHE_DUMP_H__

#ifdef CONFIG_MTK_CACHE_DUMP
extern int mt_icache_dump(void);
#else
static inline unsigned int mt_icache_dump(void)
{
	return 0;
}
#endif


#endif
