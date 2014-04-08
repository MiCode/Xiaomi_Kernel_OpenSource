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
#include "mdss_mdp.h"

#define STATUS_CHECK_INTERVAL 5000

struct dsi_status_data {
	struct notifier_block fb_notifier;
	struct delayed_work check_status;
	struct msm_fb_data_type *mfd;
	uint32_t check_interval;
};
struct dsi_status_data *pstatus_data;
static uint32_t interval = STATUS_CHECK_INTERVAL;

/*
 * check_dsi_ctrl_status() - Check DSI controller status periodically.
 * @work  : dsi controller status data
 *
 * This function calls check_status API on DSI controller to send the BTA
 * command. If DSI controller fails to acknowledge the BTA command, it sends
 * the PANEL_ALIVE=0 status to HAL layer.
 */
static void check_dsi_ctrl_status(struct work_struct *work)
{
	struct dsi_status_data *pdsi_status = NULL;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_overlay_private *mdp5_data = NULL;
	struct mdss_mdp_ctl *ctl = NULL;
	int ret = 0;

	pdsi_status = container_of(to_delayed_work(work),
		struct dsi_status_data, check_status);
	if (!pdsi_status) {
		pr_err("%s: DSI status data not available\n", __func__);
		return;
	}

	if (!pdsi_status->mfd) {
		pr_err("%s: FB data not available\n", __func__);
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
		pr_err("%s: DSI ctrl or status_check callback not available\n",
								__func__);
		return;
	}

	mdp5_data = mfd_to_mdp5_data(pdsi_status->mfd);
	ctl = mfd_to_ctl(pdsi_status->mfd);

	if (ctl->shared_lock)
		mutex_lock(ctl->shared_lock);
	mutex_lock(&mdp5_data->ov_lock);

	if (pdsi_status->mfd->shutdown_pending) {
		mutex_unlock(&mdp5_data->ov_lock);
		if (ctl->shared_lock)
			mutex_unlock(ctl->shared_lock);
		pr_err("%s: DSI turning off, avoiding BTA status check\n",
							__func__);
		return;
	}

	/*
	 * For the command mode panels, we return pan display
	 * IOCTL on vsync interrupt. So, after vsync interrupt comes
	 * and when DMA_P is in progress, if the panel stops responding
	 * and if we trigger BTA before DMA_P finishes, then the DSI
	 * FIFO will not be cleared since the DSI data bus control
	 * doesn't come back to the host after BTA. This may cause the
	 * display reset not to be proper. Hence, wait for DMA_P done
	 * for command mode panels before triggering BTA.
	 */
	if (ctl->wait_pingpong)
		ctl->wait_pingpong(ctl, NULL);

	pr_debug("%s: DSI ctrl wait for ping pong done\n", __func__);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	ret = ctrl_pdata->check_status(ctrl_pdata);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mutex_unlock(&mdp5_data->ov_lock);
	if (ctl->shared_lock)
		mutex_unlock(ctl->shared_lock);

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

/*
 * fb_event_callback() - Call back function for the fb_register_client()
 *			 notifying events
 * @self  : notifier block
 * @event : The event that was triggered
 * @data  : Of type struct fb_event
 *
 * This function listens for FB_BLANK_UNBLANK and FB_BLANK_POWERDOWN events
 * from frame buffer. DSI status check work is either scheduled again after
 * PANEL_STATUS_CHECK_INTERVAL or cancelled based on the event.
 */
static int fb_event_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;

	if (event == FB_EVENT_BLANK && evdata) {
		int *blank = evdata->data;
		struct dsi_status_data *pdata = container_of(self,
				struct dsi_status_data, fb_notifier);
		pdata->mfd = evdata->info->par;

		switch (*blank) {
		case FB_BLANK_UNBLANK:
			schedule_delayed_work(&pdata->check_status,
				msecs_to_jiffies(pdata->check_interval));
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
	int rc = 0;

	pstatus_data = kzalloc(sizeof(struct dsi_status_data), GFP_KERNEL);
	if (!pstatus_data) {
		pr_err("%s: can't allocate memory\n", __func__);
		return -ENOMEM;
	}

	pstatus_data->fb_notifier.notifier_call = fb_event_callback;

	rc = fb_register_client(&pstatus_data->fb_notifier);
	if (rc < 0) {
		pr_err("%s: fb_register_client failed, returned with rc=%d\n",
								__func__, rc);
		kfree(pstatus_data);
		return -EPERM;
	}

	pstatus_data->check_interval = interval;
	pr_info("%s: DSI status check interval:%d\n", __func__,	interval);

	INIT_DELAYED_WORK(&pstatus_data->check_status, check_dsi_ctrl_status);

	pr_debug("%s: DSI ctrl status work queue initialized\n", __func__);

	return rc;
}

void __exit mdss_dsi_status_exit(void)
{
	fb_unregister_client(&pstatus_data->fb_notifier);
	cancel_delayed_work_sync(&pstatus_data->check_status);
	kfree(pstatus_data);
	pr_debug("%s: DSI ctrl status work queue removed\n", __func__);
}

module_param(interval, uint, 0);
MODULE_PARM_DESC(interval,
		"Duration in milliseconds to send BTA command for checking"
		"DSI status periodically");

module_init(mdss_dsi_status_init);
module_exit(mdss_dsi_status_exit);

MODULE_LICENSE("GPL v2");
