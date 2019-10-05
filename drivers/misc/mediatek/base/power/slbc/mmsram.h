/*
 * Copyright (C) 2019 MediaTek Inc.
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
#ifndef MMSRAM_H
#define MMSRAM_H

struct mmsram_data {
	void __iomem *paddr;
	void __iomem *vaddr;
	ssize_t size;
};

#ifdef CONFIG_MTK_SLBC
extern int enable_mmsram(void);
extern void disable_mmsram(void);
extern void mmsram_get_info(struct mmsram_data *data);
extern int mmsram_power_on(void);
extern void mmsram_power_off(void);
extern void mmsram_set_secure(bool secure_on);
#else
static inline int enable_mmsram(void) { return 0; }
static inline void disable_mmsram(void) {}
static inline void mmsram_get_info(struct mmsram_data *data) {}
static inline int mmsram_power_on(void) { return 0; }
static inline void mmsram_power_off(void) {}
static inline void mmsram_set_secure(bool secure_on) {}
#endif /* CONFIG_MTK_SLBC */

#endif

