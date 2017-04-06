/* Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
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

#ifdef CONFIG_MDM_DBG_REQ_ENG
static struct esoc_clink *dbg_clink;
/* Last recorded request from esoc */
static enum esoc_req last_req;
static DEFINE_SPINLOCK(req_lock);
/*
 * esoc_to_user: Conversion of esoc ids to user visible strings
 * id: esoc request, command, notifier, event id
 * str: string equivalent of the above
 */
struct esoc_to_user {
	unsigned int id;
	char str[20];
};

static struct esoc_to_user in_to_resp[] = {
	{
		.id = ESOC_IMG_XFER_DONE,
		.str = "XFER_DONE",
	},
	{
		.id = ESOC_BOOT_DONE,
		.str = "BOOT_DONE",
	},
	{
		.id = ESOC_BOOT_FAIL,
		.str = "BOOT_FAIL",
	},
	{
		.id = ESOC_IMG_XFER_RETRY,
		.str = "XFER_RETRY",
	},
	{	.id = ESOC_IMG_XFER_FAIL,
		.str = "XFER_FAIL",
	},
	{
		.id = ESOC_UPGRADE_AVAILABLE,
		.str = "UPGRADE",
	},
	{	.id = ESOC_DEBUG_DONE,
		.str = "DEBUG_DONE",
	},
	{
		.id = ESOC_DEBUG_FAIL,
		.str = "DEBUG_FAIL",
	},
};

static struct esoc_to_user req_to_str[] = {
	{
		.id = ESOC_REQ_IMG,
		.str = "REQ_IMG",
	},
	{
		.id = ESOC_REQ_DEBUG,
		.str = "REQ_DEBUG",
	},
	{
		.id = ESOC_REQ_SHUTDOWN,
		.str = "REQ_SHUTDOWN",
	},
};

static ssize_t req_eng_resp_store(struct device_driver *drv, const char *buf,
							size_t count)
{
	unsigned int i;
	const struct esoc_clink_ops *const clink_ops = dbg_clink->clink_ops;

	dev_dbg(&dbg_clink->dev, "user input req eng response %s\n", buf);
	for (i = 0; i < ARRAY_SIZE(in_to_resp); i++) {
		size_t len1 = strlen(buf);
		size_t len2 = strlen(in_to_resp[i].str);

		if (len1 == len2 && !strcmp(buf, in_to_resp[i].str)) {
			clink_ops->notify(in_to_resp[i].id, dbg_clink);
			break;
		}
	}
	if (i > ARRAY_SIZE(in_to_resp))
		dev_err(&dbg_clink->dev, "Invalid resp %s, specified\n", buf);
	return count;
}

static DRIVER_ATTR(req_eng_resp, S_IWUSR, NULL, req_eng_resp_store);

static ssize_t last_esoc_req_show(struct device_driver *drv, char *buf)
{
	unsigned int i;
	unsigned long flags;
	size_t count;

	spin_lock_irqsave(&req_lock, flags);
	for (i = 0; i < ARRAY_SIZE(req_to_str); i++) {
		if (last_req == req_to_str[i].id) {
			count = snprintf(buf, PAGE_SIZE, "%s\n",
					req_to_str[i].str);
			break;
		}
	}
	spin_unlock_irqrestore(&req_lock, flags);
	return count;
}
static DRIVER_ATTR(last_esoc_req, S_IRUSR, last_esoc_req_show, NULL);

static void esoc_handle_req(enum esoc_req req, struct esoc_eng *eng)
{
	unsigned long flags;

	spin_lock_irqsave(&req_lock, flags);
	last_req = req;
	spin_unlock_irqrestore(&req_lock, flags);
}

static void esoc_handle_evt(enum esoc_evt evt, struct esoc_eng *eng)
{
}

static struct esoc_eng dbg_req_eng = {
	.handle_clink_req = esoc_handle_req,
	.handle_clink_evt = esoc_handle_evt,
};

int register_dbg_req_eng(struct esoc_clink *clink,
					struct device_driver *drv)
{
	int ret;

	dbg_clink = clink;
	ret = driver_create_file(drv, &driver_attr_req_eng_resp);
	if (ret)
		return ret;
	ret = driver_create_file(drv, &driver_attr_last_esoc_req);
	if (ret) {
		dev_err(&clink->dev, "Unable to create last esoc req\n");
		goto last_req_err;
	}
	ret = esoc_clink_register_req_eng(clink, &dbg_req_eng);
	if (ret) {
		pr_err("Unable to register req eng\n");
		goto req_eng_fail;
	}
	spin_lock_init(&req_lock);
	return 0;
last_req_err:
	driver_remove_file(drv, &driver_attr_last_esoc_req);
req_eng_fail:
	driver_remove_file(drv, &driver_attr_req_eng_resp);
	return ret;
}
#else
int register_dbg_req_eng(struct esoc_clink *clink, struct device_driver *d)
{
	return 0;
}
#endif

int mdm_dbg_eng_init(struct esoc_drv *esoc_drv,
			struct esoc_clink *clink)
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
	ret = register_dbg_req_eng(clink, drv);
	if (ret) {
		pr_err("Failed to register esoc dbg req eng\n");
		goto dbg_req_fail;
	}
	return 0;
dbg_req_fail:
	driver_remove_file(drv, &driver_attr_notifier_mask);
notify_mask_err:
	driver_remove_file(drv, &driver_attr_command_mask);
cmd_mask_err:
	return ret;
}
EXPORT_SYMBOL(mdm_dbg_eng_init);
MODULE_LICENSE("GPL V2");
