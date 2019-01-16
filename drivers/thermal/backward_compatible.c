#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/thermal.h>

#include "thermal_core.h"

/**
 * backward_compatible_throttle
 * @tz - thermal_zone_device
 *
 * This function update the cooler state by monitoring the current temperature and trip points
 */
static int backward_compatible_throttle(struct thermal_zone_device *tz, int trip)
{
	long trip_temp;
	struct thermal_instance *instance;

	if (trip == THERMAL_TRIPS_NONE) {
		trip_temp = tz->forced_passive;
	} else {
		tz->ops->get_trip_temp(tz, trip, &trip_temp);
	}

	/* mutex_lock(&tz->lock); */

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (instance->trip != trip)
			continue;

		if (tz->temperature >= trip_temp)
			instance->target = 1;
		else
			instance->target = 0;
		instance->cdev->updated = false;
		thermal_cdev_update(instance->cdev);
	}

	/* mutex_unlock(&tz->lock); */

	return 0;
}

static struct thermal_governor thermal_gov_backward_compatible = {
	.name = "backward_compatible",
	.throttle = backward_compatible_throttle,
	/* .owner                = THIS_MODULE, */
};

static int __init thermal_gov_backward_compatible_init(void)
{
	return thermal_register_governor(&thermal_gov_backward_compatible);
}

static void __exit thermal_gov_backward_compatible_exit(void)
{
	thermal_unregister_governor(&thermal_gov_backward_compatible);
}

/* This should load after thermal framework */
fs_initcall(thermal_gov_backward_compatible_init);
module_exit(thermal_gov_backward_compatible_exit);

MODULE_AUTHOR("Weiyi Lu");
MODULE_DESCRIPTION("A backward compatible Thermal governor");
MODULE_LICENSE("GPL");
