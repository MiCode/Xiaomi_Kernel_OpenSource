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

#ifndef __CMDQ_TEST_H__
#define __CMDQ_TEST_H__

#define MAXLINESIZE 20
extern struct ContextStruct gCmdqContext;
extern unsigned long msleep_interruptible(unsigned int msecs);

extern unsigned long msleep_interruptible(unsigned int msecs);
extern int32_t cmdq_core_suspend_HW_thread(int32_t thread);

extern int32_t cmdq_append_command(cmdqRecHandle handle,
				   enum CMDQ_CODE_ENUM code, uint32_t argA, uint32_t argB);
extern int32_t cmdq_rec_finalize_command(cmdqRecHandle handle, bool loop);
extern void cmdq_core_reset_hw_events(void);


#endif				/* __CMDQ_TEST_H__ */
