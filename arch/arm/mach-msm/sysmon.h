/*
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#include <mach/msm_smd.h>
#include <mach/subsystem_notif.h>

/**
 * enum subsys_id - Destination subsystems for events.
 */
enum subsys_id {
	/* SMD subsystems */
	SYSMON_SS_MODEM     = SMD_APPS_MODEM,
	SYSMON_SS_LPASS     = SMD_APPS_QDSP,
	SYSMON_SS_WCNSS     = SMD_APPS_WCNSS,
	SYSMON_SS_DSPS      = SMD_APPS_DSPS,
	SYSMON_SS_Q6FW      = SMD_APPS_Q6FW,

	/* Non-SMD subsystems */
	SYSMON_SS_EXT_MODEM = SMD_NUM_TYPE,
	SYSMON_NUM_SS
};

#ifdef CONFIG_MSM_SYSMON_COMM
int sysmon_send_event(enum subsys_id dest_ss, const char *event_ss,
		      enum subsys_notif_type notif);
int sysmon_get_reason(enum subsys_id dest_ss, char *buf, size_t len);
#else
static inline int sysmon_send_event(enum subsys_id dest_ss,
				    const char *event_ss,
				    enum subsys_notif_type notif)
{
	return 0;
}
static inline int sysmon_get_reason(enum subsys_id dest_ss, char *buf,
				    size_t len)
{
	return 0;
}
#endif

#endif
