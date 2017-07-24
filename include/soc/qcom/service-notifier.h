/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
 * Process Domain Service Notifier API header
 */

#ifndef _SERVICE_NOTIFIER_H
#define _SERVICE_NOTIFIER_H

enum qmi_servreg_notif_service_state_enum_type_v01 {
	QMI_SERVREG_NOTIF_SERVICE_STATE_ENUM_TYPE_MIN_VAL_V01 = INT_MIN,
	QMI_SERVREG_NOTIF_SERVICE_STATE_ENUM_TYPE_MAX_VAL_V01 = INT_MAX,
	SERVREG_NOTIF_SERVICE_STATE_DOWN_V01 = 0x0FFFFFFF,
	SERVREG_NOTIF_SERVICE_STATE_UP_V01 = 0x1FFFFFFF,
	SERVREG_NOTIF_SERVICE_STATE_UNINIT_V01 = 0x7FFFFFFF,
};

enum pd_subsys_state {
	ROOT_PD_DOWN,
	ROOT_PD_UP,
	ROOT_PD_ERR_FATAL,
	ROOT_PD_WDOG_BITE,
	ROOT_PD_SHUTDOWN,
	USER_PD_STATE_CHANGE,
};
#if defined(CONFIG_MSM_SERVICE_NOTIFIER)

/* service_notif_register_notifier() - Register a notifier for a service
 * On success, it returns back a handle. It takes the following arguments:
 * service_path: Individual service identifier path for which a client
 *		registers for notifications.
 * instance_id: Instance id specific to a subsystem.
 * current_state: Current state of service returned by the registration
 *		 process.
 * notifier block: notifier callback for service events.
 */
void *service_notif_register_notifier(const char *service_path, int instance_id,
				struct notifier_block *nb, int *curr_state);

/* service_notif_unregister_notifier() - Unregister a notifier for a service.
 * service_notif_handle - The notifier handler that was provided by the
 *			  service_notif_register_notifier function when the
 *			  client registered for notifications.
 * nb - The notifier block that was previously used during the registration.
 */
int service_notif_unregister_notifier(void *service_notif_handle,
					struct notifier_block *nb);

int service_notif_pd_restart(const char *service_path, int instance_id);

#else

static inline void *service_notif_register_notifier(const char *service_path,
				int instance_id, struct notifier_block *nb,
				int *curr_state)
{
	return ERR_PTR(-ENODEV);
}

static inline int service_notif_unregister_notifier(void *service_notif_handle,
					struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int service_notif_pd_restart(const char *service_path,
						int instance_id)
{
	return -ENODEV;
}

#endif /* CONFIG_MSM_SERVICE_NOTIFIER */

#endif
