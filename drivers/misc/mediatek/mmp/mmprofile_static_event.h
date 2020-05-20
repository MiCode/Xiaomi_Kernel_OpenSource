/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __MMPROFILE_STATIC_EVENT_H__
#define __MMPROFILE_STATIC_EVENT_H__

enum mmp_static_events {
	MMP_INVALID_EVENT = 0,
	MMP_ROOT_EVENT = 1,
	/* User defined static events begin */
	MMP_TOUCH_PANEL_EVENT,
	/* User defined static events end. */
	MMP_MAX_STATIC_EVENT
};

#ifdef MMPROFILE_INTERNAL

struct mmp_static_event_t {
	enum mmp_static_events event;
	char *name;
	enum mmp_static_events parent;
};

static struct mmp_static_event_t mmprofile_static_events[] = {
	{MMP_ROOT_EVENT, "Root_Event", MMP_INVALID_EVENT},
	{MMP_TOUCH_PANEL_EVENT, "TouchPanel_Event", MMP_ROOT_EVENT},
};

#endif

#endif
