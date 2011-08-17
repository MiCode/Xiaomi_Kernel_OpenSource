/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#include <linux/pm_qos_params.h>
#include <linux/err.h>
#include <mach/msm_reqs.h>

struct pm_qos_request_list {
        struct list_head list;
        union {
                s32 value;
                s32 usec;
                s32 kbps;
        };
        int pm_qos_class;
};


struct pm_qos_object {
        struct pm_qos_request_list requests;
        struct blocking_notifier_head *notifiers;
        struct miscdevice pm_qos_power_miscdev;
        char *name;
        s32 default_value;
        atomic_t target_value;
        s32 (*comparitor)(s32, s32);
};


int msm_pm_qos_add(struct pm_qos_object *class, char *request_name,
		   s32 value, void **request_data)
{
	char *resource_name = class->name;

	/* Non-default requirements are not allowed since, if the resource
	 * isn't available yet, the request can't be honoured. */
	BUG_ON(value != class->default_value);

	/* Create an client for a resource. Store the client pointer
	 * where request_data points when the the resource is available. */
	*request_data = msm_req_add(resource_name, request_name);
	if (IS_ERR(*request_data))
		return PTR_ERR(*request_data);

	return 0;
}

int msm_pm_qos_update(struct pm_qos_object *class, char *request_name,
		      s32 value, void **request_data)
{
	return msm_req_update(*request_data, value);
}

int msm_pm_qos_remove(struct pm_qos_object *class, char *request_name,
		      s32 value, void **request_data)
{
	return msm_req_remove(*request_data);
}

