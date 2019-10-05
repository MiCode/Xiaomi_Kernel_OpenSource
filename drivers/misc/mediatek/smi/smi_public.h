/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __SMI_PUBLIC_H__
#define __SMI_PUBLIC_H__

#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
bool smi_mm_first_check(void);
s32 smi_bus_prepare_enable(
	const u32 id, const char *user, const bool mtcmos);
s32 smi_bus_disable_unprepare(
	const u32 id, const char *user, const bool mtcmos);
#else
#define smi_mm_first_check(void) ((void)0)
#define smi_bus_prepare_enable(id, user, mtcmos) ((void)0)
#define smi_bus_disable_unprepare(id, user, mtcmos) ((void)0)
#endif

#endif
