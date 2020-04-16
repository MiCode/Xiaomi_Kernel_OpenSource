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
