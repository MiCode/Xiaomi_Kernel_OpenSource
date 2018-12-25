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

static struct CMDQ_MMP_Events_t CMDQ_MMP_Events;

struct CMDQ_MMP_Events_t *cmdq_mmp_get_event(void)
{
	return &CMDQ_MMP_Events;
}

void cmdq_mmp_init(void)
{
#ifdef CMDQ_PROFILE_MMP
	MMProfileEnable(1);
	if (CMDQ_MMP_Events.CMDQ == 0) {
		CMDQ_MMP_Events.CMDQ =
			MMProfileRegisterEvent(MMP_RootEvent, "CMDQ");
		CMDQ_MMP_Events.thread_en = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "thread_en");
		CMDQ_MMP_Events.CMDQ_IRQ = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "CMDQ_IRQ");
		CMDQ_MMP_Events.warning =
			MMProfileRegisterEvent(CMDQ_MMP_Events.CMDQ, "warning");
		CMDQ_MMP_Events.loopBeat =
			MMProfileRegisterEvent(CMDQ_MMP_Events.CMDQ, "loopIRQ");

		CMDQ_MMP_Events.autoRelease_add = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "autoRelease_add");
		CMDQ_MMP_Events.autoRelease_done = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "autoRelease_done");
		CMDQ_MMP_Events.consume_add = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "consume_add");
		CMDQ_MMP_Events.consume_done = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "consume_done");
		CMDQ_MMP_Events.alloc_task = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "alloc_task");
		CMDQ_MMP_Events.wait_task = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "wait_task");
		CMDQ_MMP_Events.wait_thread = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "wait_thread");
		CMDQ_MMP_Events.MDP_reset = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "MDP_reset");
		CMDQ_MMP_Events.thread_suspend = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "thread_suspend");
		CMDQ_MMP_Events.thread_resume = MMProfileRegisterEvent(
			CMDQ_MMP_Events.CMDQ, "thread_resume");

		MMProfileEnableEventRecursive(CMDQ_MMP_Events.CMDQ, 1);
	}
	MMProfileStart(1);
#endif
}
