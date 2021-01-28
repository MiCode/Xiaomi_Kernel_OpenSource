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
enum {
	SMI_LARB0, SMI_LARB1, SMI_LARB2, SMI_LARB3, SMI_LARB4,
	SMI_LARB5, SMI_LARB6, SMI_LARB7, SMI_LARB8, SMI_LARB9,
};

bool smi_mm_first_get(void);
s32 smi_bus_prepare_enable(const u32 id, const char *user);
s32 smi_bus_disable_unprepare(const u32 id, const char *user);
s32 smi_debug_bus_hang_detect(const bool gce, const char *user);
#else
#define smi_mm_first_get(void) ((void)0)
#define smi_bus_prepare_enable(id, user) ((void)0)
#define smi_bus_disable_unprepare(id, user) ((void)0)
#define smi_debug_bus_hang_detect(gce, user) ((void)0)
#endif

#endif
