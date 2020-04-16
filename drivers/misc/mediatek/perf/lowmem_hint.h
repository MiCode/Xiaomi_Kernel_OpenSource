/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _LOWMEM_HINT_H
#define _LOWMEM_HINT_H

#ifdef CONFIG_MTK_LOWMEM_HINT
extern void trigger_lowmem_hint(long *out_avail_mem,
				long *out_free_mem);
#else
static inline void trigger_lowmem_hint(long *out_avail_mem,
				       long *out_free_mem) {}
#endif
#endif /* _LOWMEM_HINT_H */
