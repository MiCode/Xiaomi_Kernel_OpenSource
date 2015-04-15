/*
 *  user_space.c - A simple user space Thermal events notifier
 *
 *  Copyright (C) 2012 Intel Corp
 *  Copyright (C) 2012 Durgadoss R <durgadoss.r@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/thermal.h>
#include <linux/slab.h>

#include "thermal_core.h"

/**
 * notify_user_space - Notifies user space about thermal events
 * @tz - thermal_zone_device
 *
 * This function notifies the user space through UEvents.
 */
static int notify_user_space(struct thermal_zone_device *tz, int trip)
{
	char *thrm_event[5];

	mutex_lock(&tz->lock);
	if (trip == tz->crossed_trip) {
		thrm_event[0] = kasprintf(GFP_KERNEL, "NAME=%s", tz->type);
		thrm_event[1] = kasprintf(GFP_KERNEL, "TEMP=%d",
				tz->temperature);
		thrm_event[2] = kasprintf(GFP_KERNEL, "TRIP=%d",
				tz->crossed_trip);
		thrm_event[3] = kasprintf(GFP_KERNEL, "EVENT=%d", tz->event);
		thrm_event[4] = NULL;
		kobject_uevent_env(&tz->device.kobj, KOBJ_CHANGE, thrm_event);
		kfree(thrm_event[0]);
		kfree(thrm_event[1]);
		kfree(thrm_event[2]);
		kfree(thrm_event[3]);
	}
	mutex_unlock(&tz->lock);

	return 0;
}

static struct thermal_governor thermal_gov_user_space = {
	.name		= "user_space",
	.throttle	= notify_user_space,
};

int thermal_gov_user_space_register(void)
{
	return thermal_register_governor(&thermal_gov_user_space);
}

void thermal_gov_user_space_unregister(void)
{
	thermal_unregister_governor(&thermal_gov_user_space);
}

