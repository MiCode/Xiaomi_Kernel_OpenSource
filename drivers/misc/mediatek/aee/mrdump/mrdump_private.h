/*
 * Copyright (C) 2016 MediaTek Inc.
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

#if !defined(__MRDUMP_PRIVATE_H__)
#define __MRDUMP_PRIVATE_H__

struct mrdump_platform {
	void (*hw_enable)(bool enabled);
	void (*reboot)(void);
};

struct pt_regs;

int mrdump_platform_init(const struct mrdump_platform *plat);

void mrdump_save_current_backtrace(struct pt_regs *regs);

extern void __disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2(void);
extern void __inner_flush_dcache_all(void);

#endif /* __MRDUMP_PRIVATE_H__ */
