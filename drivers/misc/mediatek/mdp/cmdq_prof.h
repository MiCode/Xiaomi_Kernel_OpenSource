/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __CMDQ_PROF_H__
#define __CMDQ_PROF_H__

#include <linux/types.h>

s32 cmdq_prof_estimate_command_exe_time(
	const u32 *pCmd, u32 commandSize);

#endif				/* __CMDQ_PROF_H__ */
