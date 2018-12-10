/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_SYSMON_H
#define __MSM_SYSMON_H

#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>

/**
 * enum ssctl_ssr_event_enum_type - Subsystem notification type.
 */
enum ssctl_ssr_event_enum_type {
	SSCTL_SSR_EVENT_ENUM_TYPE_MIN_ENUM_VAL = -2147483647,
	SSCTL_SSR_EVENT_INVALID = -1,
	SSCTL_SSR_EVENT_BEFORE_POWERUP = 0,
	SSCTL_SSR_EVENT_AFTER_POWERUP = 1,
	SSCTL_SSR_EVENT_BEFORE_SHUTDOWN = 2,
	SSCTL_SSR_EVENT_AFTER_SHUTDOWN = 3,
	SSCTL_SSR_EVENT_ENUM_TYPE_MAX_ENUM_VAL = 2147483647
};

/**
 * enum ssctl_ssr_event_driven_enum_type - Subsystem shutdown type.
 */
enum ssctl_ssr_event_driven_enum_type {
	SSCTL_SSR_EVENT_DRIVEN_ENUM_TYPE_MIN_ENUM_VAL = -2147483647,
	SSCTL_SSR_EVENT_FORCED = 0,
	SSCTL_SSR_EVENT_GRACEFUL = 1,
	SSCTL_SSR_EVENT_DRIVEN_ENUM_TYPE_MAX_ENUM_VAL = 2147483647
};

#if defined(CONFIG_MSM_SYSMON_COMM) || defined(CONFIG_MSM_SYSMON_QMI_COMM)
extern int sysmon_send_event(struct subsys_desc *dest_desc,
			struct subsys_desc *event_desc,
			enum subsys_notif_type notif);
extern int sysmon_get_reason(struct subsys_desc *dest_desc, char *buf,
				size_t len);
extern int sysmon_send_shutdown(struct subsys_desc *dest_desc);
extern int sysmon_notifier_register(struct subsys_desc *desc);
extern void sysmon_notifier_unregister(struct subsys_desc *desc);
#else
static inline int sysmon_send_event(struct subsys_desc *dest_desc,
					struct subsys_desc *event_desc,
					enum subsys_notif_type notif)
{
	return 0;
}
static inline int sysmon_get_reason(struct subsys_desc *dest_desc,
					char *buf, size_t len)
{
	return 0;
}
static inline int sysmon_send_shutdown(struct subsys_desc *dest_desc)
{
	return 0;
}
static inline int sysmon_notifier_register(struct subsys_desc *desc)
{
	return 0;
}
static inline void sysmon_notifier_unregister(struct subsys_desc *desc)
{
}
#endif

#if defined(CONFIG_MSM_SYSMON_GLINK_COMM)
extern int sysmon_glink_register(struct subsys_desc *desc);
extern void sysmon_glink_unregister(struct subsys_desc *desc);
extern int sysmon_send_shutdown_no_qmi(struct subsys_desc *dest_desc);
extern int sysmon_get_reason_no_qmi(struct subsys_desc *dest_desc,
				char *buf, size_t len);
extern int sysmon_send_event_no_qmi(struct subsys_desc *dest_desc,
				struct subsys_desc *event_desc,
				enum subsys_notif_type notif);
#else
static inline int sysmon_get_reason_no_qmi(struct subsys_desc *dest_desc,
						char *buf, size_t len)
{
	return 0;
}
static inline int sysmon_send_shutdown_no_qmi(struct subsys_desc *dest_desc)
{
	return 0;
}
static inline int sysmon_glink_register(struct subsys_desc *desc)
{
	return 0;
}
static inline void sysmon_glink_unregister(struct subsys_desc *desc)
{
}
static inline int sysmon_send_event_no_qmi(struct subsys_desc *dest_desc,
						struct subsys_desc *event_desc,
						enum subsys_notif_type notif)
{
	return 0;
}
#endif
#endif
