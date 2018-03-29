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


typedef enum {
	MMP_InvalidEvent = 0,
	MMP_RootEvent = 1,
	/* User defined static events begin */
	MMP_TouchPanelEvent,
	/* User defined static events end. */
	MMP_MaxStaticEvent
} MMP_StaticEvents;

#ifdef MMPROFILE_INTERNAL
typedef struct {
	MMP_StaticEvents event;
	char *name;
	MMP_StaticEvents parent;
} MMP_StaticEvent_t;

static MMP_StaticEvent_t MMProfileStaticEvents[] = {
	{MMP_RootEvent, "Root_Event", MMP_InvalidEvent},
	{MMP_TouchPanelEvent, "TouchPanel_Event", MMP_RootEvent},
};

#endif

#endif
