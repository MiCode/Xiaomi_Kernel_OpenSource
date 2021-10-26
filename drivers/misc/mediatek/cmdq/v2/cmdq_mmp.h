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

#ifndef __CMDQ_MMP_H__
#define __CMDQ_MMP_H__

#ifdef CMDQ_PROFILE_MMP
#include "mmprofile.h"
#include "mmprofile_function.h"
#include "cmdq_core.h"
#endif

struct CMDQ_MMP_events_t {
#ifdef CMDQ_PROFILE_MMP
	mmp_event CMDQ;
	mmp_event CMDQ_IRQ;
	mmp_event thread_en;
	mmp_event warning;
	mmp_event loopBeat;
	mmp_event autoRelease_add;
	mmp_event autoRelease_done;
	mmp_event consume_add;
	mmp_event consume_done;
	mmp_event alloc_task;
	mmp_event wait_task;
	mmp_event wait_thread;
	mmp_event MDP_reset;
	mmp_event thread_suspend;
	mmp_event thread_resume;
#endif
};

void cmdq_mmp_init(void);
struct CMDQ_MMP_events_t *cmdq_mmp_get_event(void);

#endif				/* __CMDQ_MMP_H__ */
