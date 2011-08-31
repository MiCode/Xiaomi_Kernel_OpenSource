/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MSM_SYSMON_H
#define __MSM_SYSMON_H

#include <mach/subsystem_notif.h>

enum subsys_id {
	SYSMON_SS_MODEM,
	SYSMON_SS_LPASS,
	SYSMON_SS_WCNSS,
	SYSMON_SS_DSPS,
	SYSMON_SS_Q6FW,
	SYSMON_NUM_SS
};

#ifdef CONFIG_MSM_SYSMON_COMM
int sysmon_send_event(enum subsys_id dest_ss, const char *event_ss,
		      enum subsys_notif_type notif);
#else
static inline int sysmon_send_event(enum subsys_id dest_ss,
				    const char *event_ss,
				    enum subsys_notif_type notif)
{
	return 0;
}
#endif

#endif
