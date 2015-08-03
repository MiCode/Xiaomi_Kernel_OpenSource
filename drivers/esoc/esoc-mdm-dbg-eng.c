/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/atomic.h>
#include <linux/device.h>
#include "esoc.h"

/*
 * cmd_mask : Specifies if a command/notifier is masked, and
 * whats the trigger value for mask to take effect.
 * @mask_trigger: trigger value for mask.
 * @mask: boolean to determine if command should be masked.
 */
struct esoc_mask {
	atomic_t mask_trigger;
	bool mask;
};

/*
 * manual_to_esoc_cmd: Converts a user provided command
 * to a corresponding esoc command.
 * @cmd: ESOC command
 * @manual_cmd: user specified command string.
 */
struct manual_to_esoc_cmd {
	unsigned int cmd;
	char manual_cmd[20];
};

/*
 * manual_to_esoc_notify: Converts a user provided notification
 * to corresponding esoc notification for Primary SOC.
 * @notfication: ESOC notification.
 * @manual_notifier: user specified notification string.
 */
struct manual_to_esoc_notify {
	unsigned int notify;
	char manual_notify[20];
};

static const struct manual_to_esoc_cmd cmd_map[] = {
	{
		.cmd = ESOC_PWR_ON,
		.manual_cmd = "PON",
	},
	{
		.cmd = ESOC_PREPARE_DEBUG,
		.manual_cmd = "ENTER_DLOAD",
	},
	{	.cmd = ESOC_PWR_OFF,
		.manual_cmd = "POFF",
	},
	{
		.cmd = ESOC_FORCE_PWR_OFF,
		.manual_cmd = "FORCE_POFF",
	},
};

static struct esoc_mask cmd_mask[] = {
	[ESOC_PWR_ON] = {
		.mask = false,
		.mask_trigger = ATOMIC_INIT(1),
	},
	[ESOC_PREPARE_DEBUG] = {
		.mask = false,
		.mask_trigger = ATOMIC_INIT(0),
	},
	[ESOC_PWR_OFF] = {
		.mask = false,
		.mask_trigger = ATOMIC_INIT(0),
	},
	[ESOC_FORCE_PWR_OFF] = {
		.mask = false,
		.mask_trigger = ATOMIC_INIT(0),
	},
};

static const struct manual_to_esoc_notify notify_map[] = {
	{
		.notify = ESOC_PRIMARY_REBOOT,
		.manual_notify = "REBOOT",
	},
	{
		.notify = ESOC_PRIMARY_CRASH,
		.manual_notify = "PANIC",
	},
};

static struct esoc_mask notify_mask[] = {
	[ESOC_PRIMARY_REBOOT] = {
		.mask = false,
		.mask_trigger = ATOMIC_INIT(0),
	},
	[ESOC_PRIMARY_CRASH] = {
		.mask = false,
		.mask_trigger = ATOMIC_INIT(0),
	},
};

bool dbg_check_cmd_mask(unsigned int cmd)
{
	pr_debug("command to mask %d\n", cmd);
	if (cmd_mask[cmd].mask)
		return atomic_add_negative(-1, &cmd_mask[cmd].mask_trigger);
	else
		return false;
}
EXPORT_SYMBOL(dbg_check_cmd_mask);

bool dbg_check_notify_mask(unsigned int notify)
{
	pr_debug("notifier to mask %d\n", notify);
	if (notify_mask[notify].mask)
		return atomic_add_negative(-1,
					&notify_mask[notify].mask_trigger);
	else
		return false;
}
EXPORT_SYMBOL(dbg_check_notify_mask);
/*
 * Create driver attributes that let you mask
 * specific commands.
 */
static ssize_t cmd_mask_store(struct device_driver *drv, const char *buf,
							size_t count)
{
	unsigned int cmd, i;

	pr_debug("user input command %s", buf);
	for (i = 0; i < ARRAY_SIZE(cmd_map); i++) {
		if (!strcmp(cmd_map[i].manual_cmd, buf)) {
			/*
			 * Map manual command string to ESOC command
			 * set mask for ESOC command
			 */
			cmd = cmd_map[i].cmd;
			cmd_mask[cmd].mask = true;
			pr_debug("Setting mask for manual command %s\n",
								buf);
			break;
		}
	}
	if (i >= ARRAY_SIZE(cmd_map))
		pr_err("invalid command specified\n");
	return count;
}
static DRIVER_ATTR(command_mask, S_IWUSR, NULL, cmd_mask_store);

static ssize_t notifier_mask_store(struct device_driver *drv, const char *buf,
							size_t count)
{
	unsigned int notify, i;

	pr_debug("user input notifier %s", buf);
	for (i = 0; i < ARRAY_SIZE(notify_map); i++) {
		if (!strcmp(buf, notify_map[i].manual_notify)) {
			/*
			 * Map manual notifier string to primary soc
			 * notifier. Also set mask for the notifier.
			 */
			notify = notify_map[i].notify;
			notify_mask[notify].mask = true;
			pr_debug("Setting mask for manual notification %s\n",
									buf);
			break;
		}
	}
	if (i >= ARRAY_SIZE(notify_map))
		pr_err("invalid notifier specified\n");
	return count;
}
static DRIVER_ATTR(notifier_mask, S_IWUSR, NULL, notifier_mask_store);

int mdm_dbg_eng_init(struct esoc_drv *esoc_drv)
{
	int ret;
	struct device_driver *drv = &esoc_drv->driver;

	ret = driver_create_file(drv, &driver_attr_command_mask);
	if (ret) {
		pr_err("Unable to create command mask file\n");
		goto cmd_mask_err;
	}
	ret = driver_create_file(drv, &driver_attr_notifier_mask);
	if (ret) {
		pr_err("Unable to create notify mask file\n");
		goto notify_mask_err;
	}
	return 0;
notify_mask_err:
	driver_remove_file(drv, &driver_attr_command_mask);
cmd_mask_err:
	return ret;
}
EXPORT_SYMBOL(mdm_dbg_eng_init);
MODULE_LICENSE("GPL V2");
