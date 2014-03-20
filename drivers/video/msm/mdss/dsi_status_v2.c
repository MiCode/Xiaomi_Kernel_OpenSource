/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/iopoll.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include "mdss_fb.h"
#include "mdss_dsi.h"
#include "mdss_panel.h"
#include "mdp3_ctrl.h"

#define STATUS_CHECK_INTERVAL 5000

/**
 * dsi_status_data - Stores all the data necessary for this module
 * @fb_notif: Used to egister for the fb events
 * @live_status: Delayed worker structure, used to associate the
 * delayed worker function
 * @mfd: Used to store the msm_fb_data_type received when the notifier
 * call back happens
 * @root: Stores the dir created by debuugfs
 * @debugfs_reset_panel: The debugfs variable used to inject errors
 */

struct dsi_status_data {
	struct notifier_block fb_notifier;
	struct delayed_work check_status;
	struct msm_fb_data_type *mfd;
	uint32_t check_interval;
};
struct dsi_status_data *pstatus_data;
static uint32_t interval = STATUS_CHECK_INTERVAL;

void check_dsi_ctrl_status(struct work_struct *work)
{
	struct dsi_status_data *pdsi_status = NULL;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdp3_session_data *mdp3_session = NULL;
	int ret = 0;

	pdsi_status = container_of(to_delayed_work(work),
		struct dsi_status_data, check_status);
	if (!pdsi_status) {
		pr_err("%s: DSI status data not available\n", __func__);
		return;
	}

	pdata = dev_get_platdata(&pdsi_status->mfd->pdev->dev);
	if (!pdata) {
		pr_err("%s: Panel data not available\n", __func__);
		return;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);
	if (!ctrl_pdata || !ctrl_pdata->check_status) {
		pr_err("%s: DSI ctrl or status_check callback not avilable\n",
								__func__);
		return;
	}
	mdp3_session = pdsi_status->mfd->mdp.private1;
	mutex_lock(&mdp3_session->lock);

	if (!mdp3_session->status) {
		pr_info("display off already\n");
		mutex_unlock(&mdp3_session->lock);
		return;
	}

	if (mdp3_session->wait_for_dma_done)
		ret = mdp3_session->wait_for_dma_done(mdp3_session);

	if (!ret)
		ret = ctrl_pdata->check_status(ctrl_pdata);
	else
		pr_err("wait_for_dma_done error\n");

	mutex_unlock(&mdp3_session->lock);

	if ((pdsi_status->mfd->panel_power_on)) {
		if (ret > 0) {
			schedule_delayed_work(&pdsi_status->check_status,
				msecs_to_jiffies(pdsi_status->check_interval));
		} else {
			char *envp[2] = {"PANEL_ALIVE=0", NULL};
			pdata->panel_info.panel_dead = true;
			ret = kobject_uevent_env(
				&pdsi_status->mfd->fbi->dev->kobj,
							KOBJ_CHANGE, envp);
			pr_err("%s: Panel has gone bad, sending uevent - %s\n",
							__func__, envp[0]);
		}
	}
}

/**
 * fb_notifier_callback() - Call back function for the fb_register_client()
 * notifying events
 * @self  : notifier block
 * @event : The event that was triggered
 * @data  : Of type struct fb_event
 *
 * - This function listens for FB_BLANK_UNBLANK and FB_BLANK_POWERDOWN events
 * - Based on the event the delayed work is either scheduled again after
 * PANEL_STATUS_CHECK_INTERVAL or cancelled
 */
static int fb_event_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = (struct fb_event *)data;
	struct dsi_status_data *pdata = container_of(self,
				struct dsi_status_data, fb_notifier);
	pdata->mfd = (struct msm_fb_data_type *)evdata->info->par;

	if (event == FB_EVENT_BLANK && evdata) {
		int *blank = evdata->data;
		switch (*blank) {
		case FB_BLANK_UNBLANK:
			schedule_delayed_work(&pdata->check_status,
			msecs_to_jiffies(STATUS_CHECK_INTERVAL));
			break;
		case FB_BLANK_POWERDOWN:
			cancel_delayed_work(&pdata->check_status);
			break;
		}
	}
	return 0;
}

int __init mdss_dsi_status_init(void)
{
	int rc;

	pstatus_data = kzalloc(sizeof(struct dsi_status_data),	GFP_KERNEL);
	if (!pstatus_data) {
		pr_err("%s: can't alloc mem\n", __func__);
		rc = -ENOMEM;
		return rc;
	}

	memset(pstatus_data, 0, sizeof(struct dsi_status_data));

	pstatus_data->fb_notifier.notifier_call = fb_event_callback;

	rc = fb_register_client(&pstatus_data->fb_notifier);
	if (rc < 0) {
		pr_err("%s: fb_register_client failed, returned with rc=%d\n",
								__func__, rc);
		kfree(pstatus_data);
		return -EPERM;
	}

	pstatus_data->check_interval = interval;
	pr_info("%s: DSI status check interval:%d\n", __func__, interval);

	INIT_DELAYED_WORK(&pstatus_data->check_status, check_dsi_ctrl_status);

	pr_debug("%s: DSI ctrl status thread initialized\n", __func__);

	return rc;
}

void __exit mdss_dsi_status_exit(void)
{
	fb_unregister_client(&pstatus_data->fb_notifier);
	cancel_delayed_work_sync(&pstatus_data->check_status);
	kfree(pstatus_data);
	pr_debug("%s: DSI ctrl status thread removed\n", __func__);
}

module_param(interval, uint, 0);
MODULE_PARM_DESC(interval,
	"Duration in milliseconds to send BTA command for checking"
	"DSI status periodically");

module_init(mdss_dsi_status_init);
module_exit(mdss_dsi_status_exit);

MODULE_LICENSE("GPL v2");
