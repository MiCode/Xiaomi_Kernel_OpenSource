// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/thermal.h>
#include "mtk_monitor_tz.h"

static int monitor_tz_debug_log;
static struct delayed_work g_poll_queue;
static unsigned long g_polling_interval = MON_TZ_DEFAULT_POLLING_DELAY_MS;
static int thermal_zone_temp_array[MAX_MON_TZ_NUM];
static struct thermal_zone_device *thermal_zone_device_array[MAX_MON_TZ_NUM];
static char thermal_sensor_name[MAX_MON_TZ_NUM][20] = {
	"ap_ntc",
	"mdpa_ntc",
	"battery"
};

int get_monitor_thermal_zone_temp(enum monitor_thermal_zone tz_id)
{
	if (tz_id < 0 || tz_id >= MAX_MON_TZ_NUM)
		return MON_TZ_DEFAULT_TEMP;

	return thermal_zone_temp_array[tz_id];
}

static void update_temperature_from_tz(enum monitor_thermal_zone tz_id)
{
	struct thermal_zone_device *zone;
	char *tz_name;
	int temperature, ret;

	zone = thermal_zone_device_array[tz_id];

	if (zone == NULL) {
		tz_name = thermal_sensor_name[tz_id];
		zone = thermal_zone_get_zone_by_name(tz_name);
		if (IS_ERR(zone)) {
			monitor_tz_dprintk("%s, TZ %s is not ready\n",
				__func__, tz_name);
			return;
		}

		thermal_zone_device_array[tz_id] = zone;
	}


	ret = thermal_zone_get_temp(zone, &temperature);

	if (ret != 0) {
		monitor_tz_dprintk("%s, TZ %s get temperature error\n",
			__func__, tz_name);
		return;
	}

	thermal_zone_temp_array[tz_id] = temperature;
	monitor_tz_dprintk("%s, TZ %s temp = %d\n", __func__, tz_name,
		temperature);
}

static void workqueue_set_polling(int delay)
{
	if (delay > 1000)
		mod_delayed_work(system_freezable_wq, &g_poll_queue,
				 round_jiffies(msecs_to_jiffies(delay)));
	else if (delay)
		mod_delayed_work(system_freezable_wq, &g_poll_queue,
				 msecs_to_jiffies(delay));
	else
		cancel_delayed_work(&g_poll_queue);
}

static void get_temp_loop(struct work_struct *work)
{
	int i;

	for (i = 0; i < MAX_MON_TZ_NUM; i++)
		update_temperature_from_tz(i);

	workqueue_set_polling(g_polling_interval);
}

static int __init mtk_monitor_tz_init(void)
{
	int i;

	for (i = 0; i < MAX_MON_TZ_NUM; i++)
		thermal_zone_temp_array[i] = MON_TZ_DEFAULT_TEMP;

	INIT_DELAYED_WORK(&g_poll_queue, get_temp_loop);
	workqueue_set_polling(g_polling_interval);

	return 0;
}

static void __exit mtk_monitor_tz_exit(void)
{
	cancel_delayed_work(&g_poll_queue);
}
module_init(mtk_monitor_tz_init);
module_exit(mtk_monitor_tz_exit);
