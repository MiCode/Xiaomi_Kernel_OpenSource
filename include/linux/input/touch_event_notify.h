/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#ifndef _TOUCHEVENTNOTIFY_H
#define _TOUCHEVENTNOTIFY_H

struct touch_event {
	struct timeval time;
	int x;
	int y;
	int fid;       /* Finger ID */
	char type;     /* 'D' - Down, 'M' - Move, 'U' - Up, */
};

#define EVENT_TYPE_DOWN    'D'
#define EVENT_TYPE_MOVE    'M'
#define EVENT_TYPE_UP      'U'

/* caller API */
int touch_event_register_notifier(struct notifier_block *nb);
int touch_event_unregister_notifier(struct notifier_block *nb);

/* callee API */
void touch_event_call_notifier(unsigned long action, void *data);

#endif
