// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include "SCP_power_monitor.h"

static LIST_HEAD(power_monitor_list);
static DEFINE_SPINLOCK(pm_lock);
static atomic_t power_status = ATOMIC_INIT(SENSOR_POWER_DOWN);
void scp_power_monitor_notify(uint8_t action, void *data)
{
	struct scp_power_monitor *c;
	unsigned long flags;

	spin_lock_irqsave(&pm_lock, flags);
	list_for_each_entry(c, &power_monitor_list, list) {
		if (c->notifier_call == NULL) {
			WARN_ON(true);
			continue;
		}
		c->notifier_call(action, data);
		pr_debug("%s, module name:%s notify\n", __func__, c->name);
	}
	switch (action) {
	case SENSOR_POWER_DOWN:
		atomic_set(&power_status, SENSOR_POWER_DOWN);
		break;
	case SENSOR_POWER_UP:
		atomic_set(&power_status, SENSOR_POWER_UP);
		break;
	}
	spin_unlock_irqrestore(&pm_lock, flags);
}
EXPORT_SYMBOL(scp_power_monitor_notify);

int scp_power_monitor_register(struct scp_power_monitor *monitor)
{
	int err = 0;
	struct scp_power_monitor *c;

	if (monitor->name == NULL || monitor->notifier_call == NULL) {
		WARN_ON(true);
		return -1;
	}

	spin_lock_irq(&pm_lock);
	list_for_each_entry(c, &power_monitor_list, list) {
		if (!strcmp(c->name, monitor->name)) {
			err = -1;
			goto out;
		}
	}

	list_add_tail(&monitor->list, &power_monitor_list);
	if (atomic_read(&power_status) == SENSOR_POWER_UP) {
		pr_debug("scp_power_monitor_notify, module name:%s notify\n",
			monitor->name);
		monitor->notifier_call(SENSOR_POWER_UP, NULL);
	}
	spin_unlock_irq(&pm_lock);
	return err;
 out:
	pr_err("%s %s register fail\n", __func__, monitor->name);
	spin_unlock_irq(&pm_lock);
	return err;
}
EXPORT_SYMBOL(scp_power_monitor_register);

int scp_power_monitor_deregister(struct scp_power_monitor *monitor)
{
	if (WARN_ON(list_empty(&monitor->list)))
		return -1;

	spin_lock_irq(&pm_lock);
	list_del(&monitor->list);
	spin_unlock_irq(&pm_lock);
	return 0;
}
EXPORT_SYMBOL(scp_power_monitor_deregister);
