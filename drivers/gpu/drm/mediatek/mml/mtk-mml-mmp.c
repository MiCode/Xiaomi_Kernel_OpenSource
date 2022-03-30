// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk-mml-mmp.h"

static struct mml_mmp_events_t mml_mmp_events;

struct mml_mmp_events_t *mml_mmp_get_event(void)
{
	return &mml_mmp_events;
}

void mml_mmp_init(void)
{
	mmp_event mml;

	if (mml_mmp_events.mml)
		return;

	mmprofile_enable(1);
	mml = mmprofile_register_event(MMP_ROOT_EVENT, "MML");
	mml_mmp_events.mml = mml;
	mml_mmp_events.submit = mmprofile_register_event(mml, "submit");
	mml_mmp_events.config = mmprofile_register_event(mml, "config");
	mml_mmp_events.buf_map = mmprofile_register_event(mml, "buf_map");
	mml_mmp_events.fence = mmprofile_register_event(mml, "fence");
	mml_mmp_events.fence_timeout = mmprofile_register_event(mml, "fence_timeout");
	mml_mmp_events.flush = mmprofile_register_event(mml, "flush");
	mml_mmp_events.submit_cb = mmprofile_register_event(mml, "submit_cb");
	mml_mmp_events.stop_racing = mmprofile_register_event(mml, "stop_racing");
	mml_mmp_events.irq_loop = mmprofile_register_event(mml, "irq_loop");
	mml_mmp_events.irq_err = mmprofile_register_event(mml, "irq_err");
	mml_mmp_events.irq_done = mmprofile_register_event(mml, "irq_done");
	mml_mmp_events.irq_stop = mmprofile_register_event(mml, "irq_stop");
	mml_mmp_events.fence_sig = mmprofile_register_event(mml, "fence_sig");
	mml_mmp_events.exec = mmprofile_register_event(mml, "exec");

	mmprofile_enable_event_recursive(mml, 1);
	mmprofile_start(1);
}
