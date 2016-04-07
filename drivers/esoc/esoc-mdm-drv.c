/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/reboot.h>
#include "esoc.h"
#include "mdm-dbg.h"

enum {
	 PWR_OFF = 0x1,
	 PWR_ON,
	 BOOT,
	 RUN,
	 CRASH,
	 IN_DEBUG,
	 SHUTDOWN,
	 RESET,
	 PEER_CRASH,
};

struct mdm_drv {
	unsigned mode;
	struct esoc_eng cmd_eng;
	struct completion boot_done;
	struct completion req_eng_wait;
	struct esoc_clink *esoc_clink;
	bool boot_fail;
	struct workqueue_struct *mdm_queue;
	struct work_struct ssr_work;
	struct notifier_block esoc_restart;
};
#define to_mdm_drv(d)	container_of(d, struct mdm_drv, cmd_eng)

static int esoc_msm_restart_handler(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct mdm_drv *mdm_drv = container_of(nb, struct mdm_drv,
					esoc_restart);
	struct esoc_clink *esoc_clink = mdm_drv->esoc_clink;
	const struct esoc_clink_ops const *clink_ops = esoc_clink->clink_ops;
	if (action == SYS_RESTART) {
		if (mdm_dbg_stall_notify(ESOC_PRIMARY_REBOOT))
			return NOTIFY_OK;
		dev_dbg(&esoc_clink->dev, "Notifying esoc of cold reboot\n");
		clink_ops->notify(ESOC_PRIMARY_REBOOT, esoc_clink);
	}
	return NOTIFY_OK;
}
static void mdm_handle_clink_evt(enum esoc_evt evt,
					struct esoc_eng *eng)
{
	struct mdm_drv *mdm_drv = to_mdm_drv(eng);
	switch (evt) {
	case ESOC_INVALID_STATE:
		mdm_drv->boot_fail = true;
		complete(&mdm_drv->boot_done);
		break;
	case ESOC_RUN_STATE:
		mdm_drv->boot_fail = false;
		mdm_drv->mode = RUN,
		complete(&mdm_drv->boot_done);
		break;
	case ESOC_UNEXPECTED_RESET:
	case ESOC_ERR_FATAL:
		if (mdm_drv->mode == CRASH)
			return;
		mdm_drv->mode = CRASH;
		queue_work(mdm_drv->mdm_queue, &mdm_drv->ssr_work);
		break;
	case ESOC_REQ_ENG_ON:
		complete(&mdm_drv->req_eng_wait);
		break;
	default:
		break;
	}
}

static void mdm_ssr_fn(struct work_struct *work)
{
	struct mdm_drv *mdm_drv = container_of(work, struct mdm_drv, ssr_work);

	/*
	 * If restarting esoc fails, the SSR framework triggers a kernel panic
	 */
	esoc_clink_request_ssr(mdm_drv->esoc_clink);
	return;
}

static void mdm_crash_shutdown(const struct subsys_desc *mdm_subsys)
{
	struct esoc_clink *esoc_clink =
					container_of(mdm_subsys,
							struct esoc_clink,
								subsys);
	const struct esoc_clink_ops const *clink_ops = esoc_clink->clink_ops;
	if (mdm_dbg_stall_notify(ESOC_PRIMARY_CRASH))
		return;
	clink_ops->notify(ESOC_PRIMARY_CRASH, esoc_clink);
}

static int mdm_subsys_shutdown(const struct subsys_desc *crashed_subsys,
							bool force_stop)
{
	int ret;
	struct esoc_clink *esoc_clink =
	 container_of(crashed_subsys, struct esoc_clink, subsys);
	struct mdm_drv *mdm_drv = esoc_get_drv_data(esoc_clink);
	const struct esoc_clink_ops const *clink_ops = esoc_clink->clink_ops;

	if (mdm_drv->mode == CRASH || mdm_drv->mode == PEER_CRASH) {
		if (mdm_dbg_stall_cmd(ESOC_PREPARE_DEBUG))
			/* We want to mask debug command.
			 * In this case return success
			 * to move to next stage
			 */
			return 0;
		ret = clink_ops->cmd_exe(ESOC_PREPARE_DEBUG,
							esoc_clink);
		if (ret) {
			dev_err(&esoc_clink->dev, "failed to enter debug\n");
			return ret;
		}
		mdm_drv->mode = IN_DEBUG;
	} else if (!force_stop) {
		if (esoc_clink->subsys.sysmon_shutdown_ret)
			ret = clink_ops->cmd_exe(ESOC_FORCE_PWR_OFF,
							esoc_clink);
		else {
			if (mdm_dbg_stall_cmd(ESOC_PWR_OFF))
				/* Since power off command is masked
				 * we return success, and leave the state
				 * of the command engine as is.
				 */
				return 0;
			ret = clink_ops->cmd_exe(ESOC_PWR_OFF, esoc_clink);
		}
		if (ret) {
			dev_err(&esoc_clink->dev, "failed to exe power off\n");
			return ret;
		}
		mdm_drv->mode = PWR_OFF;
	}
	return 0;
}

static int mdm_subsys_powerup(const struct subsys_desc *crashed_subsys)
{
	int ret;
	struct esoc_clink *esoc_clink =
				container_of(crashed_subsys, struct esoc_clink,
								subsys);
	struct mdm_drv *mdm_drv = esoc_get_drv_data(esoc_clink);
	const struct esoc_clink_ops const *clink_ops = esoc_clink->clink_ops;

	if (!esoc_req_eng_enabled(esoc_clink)) {
		dev_dbg(&esoc_clink->dev, "Wait for req eng registration\n");
		wait_for_completion(&mdm_drv->req_eng_wait);
	}
	if (mdm_drv->mode == PWR_OFF) {
		if (mdm_dbg_stall_cmd(ESOC_PWR_ON))
			return -EBUSY;
		ret = clink_ops->cmd_exe(ESOC_PWR_ON, esoc_clink);
		if (ret) {
			dev_err(&esoc_clink->dev, "pwr on fail\n");
			return ret;
		}
	} else if (mdm_drv->mode == IN_DEBUG) {
		ret = clink_ops->cmd_exe(ESOC_EXIT_DEBUG, esoc_clink);
		if (ret) {
			dev_err(&esoc_clink->dev, "cannot exit debug mode\n");
			return ret;
		}
		mdm_drv->mode = PWR_OFF;
		ret = clink_ops->cmd_exe(ESOC_PWR_ON, esoc_clink);
		if (ret) {
			dev_err(&esoc_clink->dev, "pwr on fail\n");
			return ret;
		}
	}
	wait_for_completion(&mdm_drv->boot_done);
	if (mdm_drv->boot_fail) {
		dev_err(&esoc_clink->dev, "booting failed\n");
		return -EIO;
	}
	return 0;
}

static int mdm_subsys_ramdumps(int want_dumps,
				const struct subsys_desc *crashed_subsys)
{
	int ret;
	struct esoc_clink *esoc_clink =
				container_of(crashed_subsys, struct esoc_clink,
								subsys);
	const struct esoc_clink_ops const *clink_ops = esoc_clink->clink_ops;

	if (want_dumps) {
		ret = clink_ops->cmd_exe(ESOC_EXE_DEBUG, esoc_clink);
		if (ret) {
			dev_err(&esoc_clink->dev, "debugging failed\n");
			return ret;
		}
	}
	return 0;
}

static int mdm_register_ssr(struct esoc_clink *esoc_clink)
{
	esoc_clink->subsys.shutdown = mdm_subsys_shutdown;
	esoc_clink->subsys.ramdump = mdm_subsys_ramdumps;
	esoc_clink->subsys.powerup = mdm_subsys_powerup;
	esoc_clink->subsys.crash_shutdown = mdm_crash_shutdown;
	return esoc_clink_register_ssr(esoc_clink);
}

int esoc_ssr_probe(struct esoc_clink *esoc_clink, struct esoc_drv *drv)
{
	int ret;
	struct mdm_drv *mdm_drv;
	struct esoc_eng *esoc_eng;

	mdm_drv = devm_kzalloc(&esoc_clink->dev, sizeof(*mdm_drv), GFP_KERNEL);
	if (IS_ERR(mdm_drv))
		return PTR_ERR(mdm_drv);
	esoc_eng = &mdm_drv->cmd_eng;
	esoc_eng->handle_clink_evt = mdm_handle_clink_evt;
	ret = esoc_clink_register_cmd_eng(esoc_clink, esoc_eng);
	if (ret) {
		dev_err(&esoc_clink->dev, "failed to register cmd engine\n");
		return ret;
	}
	ret = mdm_register_ssr(esoc_clink);
	if (ret)
		goto ssr_err;
	mdm_drv->mdm_queue = alloc_workqueue("mdm_drv_queue", 0, 0);
	if (!mdm_drv->mdm_queue) {
		dev_err(&esoc_clink->dev, "could not create mdm_queue\n");
		goto queue_err;
	}
	esoc_set_drv_data(esoc_clink, mdm_drv);
	init_completion(&mdm_drv->boot_done);
	init_completion(&mdm_drv->req_eng_wait);
	INIT_WORK(&mdm_drv->ssr_work, mdm_ssr_fn);
	mdm_drv->esoc_clink = esoc_clink;
	mdm_drv->mode = PWR_OFF;
	mdm_drv->boot_fail = false;
	mdm_drv->esoc_restart.notifier_call = esoc_msm_restart_handler;
	ret = register_reboot_notifier(&mdm_drv->esoc_restart);
	if (ret)
		dev_err(&esoc_clink->dev, "register for reboot failed\n");
	ret = mdm_dbg_eng_init(drv, esoc_clink);
	if (ret) {
		debug_init_done = false;
		dev_err(&esoc_clink->dev, "dbg engine failure\n");
	} else {
		dev_dbg(&esoc_clink->dev, "dbg engine initialized\n");
		debug_init_done = true;
	}
	return 0;
queue_err:
	esoc_clink_unregister_ssr(esoc_clink);
ssr_err:
	esoc_clink_unregister_cmd_eng(esoc_clink, esoc_eng);
	return ret;
}

static struct esoc_compat compat_table[] = {
	{	.name = "MDM9x25",
		.data = NULL,
	},
	{
		.name = "MDM9x35",
		.data = NULL,
	},
	{
		.name = "MDM9x55",
		.data = NULL,
	},
};

static struct esoc_drv esoc_ssr_drv = {
	.owner = THIS_MODULE,
	.probe = esoc_ssr_probe,
	.compat_table = compat_table,
	.compat_entries = ARRAY_SIZE(compat_table),
	.driver = {
		.name = "mdm-4x",
	},
};

int __init esoc_ssr_init(void)
{
	return esoc_drv_register(&esoc_ssr_drv);
}
module_init(esoc_ssr_init);
MODULE_LICENSE("GPL v2");
