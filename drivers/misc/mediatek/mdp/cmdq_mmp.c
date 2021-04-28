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

static struct MDP_MMP_events_t mdp_mmp_events;

struct MDP_MMP_events_t *mdp_mmp_get_event(void)
{
	return &mdp_mmp_events;
}

void mdp_mmp_init(void)
{
#if IS_ENABLED(CONFIG_MMPROFILE)
	mmprofile_enable(1);
	if (mdp_mmp_events.CMDQ == 0) {
		mdp_mmp_events.CMDQ = mmprofile_register_event(
			MMP_ROOT_EVENT, "MDP");
		mdp_mmp_events.thread_en = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "thread_en");
		mdp_mmp_events.CMDQ_IRQ = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "CMDQ_IRQ");
		mdp_mmp_events.warning = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "warning");
		mdp_mmp_events.autoRelease_add = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "autoRelease_add");
		mdp_mmp_events.autoRelease_done = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "autoRelease_done");
		mdp_mmp_events.consume_add = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "consume_add");
		mdp_mmp_events.consume_done = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "consume_done");
		mdp_mmp_events.alloc_task = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "alloc_task");
		mdp_mmp_events.wait_task = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "wait_task");
		mdp_mmp_events.wait_task_done = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "wait_task_done");
		mdp_mmp_events.MDP_reset = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "MDP_reset");
		mdp_mmp_events.MDP_clock_on = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "MDP_clock_on");
		mdp_mmp_events.MDP_clock_off = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "MDP_clock_off");
		mdp_mmp_events.MDP_clock_smi = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "MDP_clock_smi");
		mdp_mmp_events.timeout = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "timeout");
		mdp_mmp_events.read_reg = mmprofile_register_event(
			mdp_mmp_events.CMDQ, "read_reg");

		mmprofile_enable_event_recursive(mdp_mmp_events.CMDQ, 1);
	}
	mmprofile_start(1);
#endif
}
