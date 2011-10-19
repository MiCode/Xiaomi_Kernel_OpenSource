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

/**
 * enum subsys_id - Destination subsystems for events.
 */
enum subsys_id {
	SYSMON_SS_MODEM,
	SYSMON_SS_LPASS,
	SYSMON_SS_WCNSS,
	SYSMON_SS_DSPS,
	SYSMON_SS_Q6FW,
	SYSMON_NUM_SS
};


/**
 * sysmon_send_event() - Notify a subsystem of another's state change.
 * @dest_ss:	ID of subsystem the notification should be sent to.
 * @event_ss:	String name of the subsystem that generated the notification.
 * @notif:	ID of the notification type (ex. SUBSYS_BEFORE_SHUTDOWN)
 *
 * Returns 0 for success, -EINVAL for invalid destination or notification IDs,
 * -ENODEV if the SMD channel is not open, -ETIMEDOUT if the destination
 * subsystem does not respond, and -ENOSYS if the destination subsystem
 * responds, but with something other than an acknowledgement.
 *
 * If CONFIG_MSM_SYSMON_COMM is not defined, always return success (0).
 */
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
