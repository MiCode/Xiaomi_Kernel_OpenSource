/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __CMDQ_PROF_H__
#define __CMDQ_PROF_H__

#include <linux/types.h>

s32 cmdq_prof_estimate_command_exe_time(
	const u32 *pCmd, u32 commandSize);

#endif				/* __CMDQ_PROF_H__ */
