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

#include "cmdq_mmp.h"

static struct CMDQ_MMP_events_t CMDQ_MMP_events;

struct CMDQ_MMP_events_t *cmdq_mmp_get_event(void)
{
	return &CMDQ_MMP_events;
}

void cmdq_mmp_init(void)
{
#ifdef CMDQ_PROFILE_MMP
	mmprofile_enable(1);
	if (CMDQ_MMP_events.CMDQ == 0) {
		CMDQ_MMP_events.CMDQ = mmprofile_register_event(
			MMP_ROOT_EVENT, "CMDQ");
		CMDQ_MMP_events.thread_en = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "thread_en");
		CMDQ_MMP_events.CMDQ_IRQ = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "CMDQ_IRQ");
		CMDQ_MMP_events.warning = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "warning");
		CMDQ_MMP_events.loopBeat = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "loopIRQ");

		CMDQ_MMP_events.autoRelease_add = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "autoRelease_add");
		CMDQ_MMP_events.autoRelease_done = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "autoRelease_done");
		CMDQ_MMP_events.consume_add = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "consume_add");
		CMDQ_MMP_events.consume_done = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "consume_done");
		CMDQ_MMP_events.alloc_task = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "alloc_task");
		CMDQ_MMP_events.wait_task = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "wait_task");
		CMDQ_MMP_events.wait_task_done = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "wait_task_done");
		CMDQ_MMP_events.wait_thread = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "wait_thread");
		CMDQ_MMP_events.MDP_reset = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "MDP_reset");
		CMDQ_MMP_events.thread_suspend = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "thread_suspend");
		CMDQ_MMP_events.thread_resume = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "thread_resume");
		CMDQ_MMP_events.alloc_buffer = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "alloc_buffer");
		CMDQ_MMP_events.timeout = mmprofile_register_event(
			CMDQ_MMP_events.CMDQ, "timeout");

		mmprofile_enable_event_recursive(CMDQ_MMP_events.CMDQ, 1);
	}
	mmprofile_start(1);
#endif
}
