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

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include "mdss_dsi.h"
#include "mdp3_ctrl.h"

/*
 * mdp3_check_dsi_ctrl_status() - Check MDP3 DSI controller status periodically.
 * @work     : dsi controller status data
 * @interval : duration in milliseconds to schedule work queue
 *
 * This function calls check_status API on DSI controller to send the BTA
 * command. If DSI controller fails to acknowledge the BTA command, it sends
 * the PANEL_ALIVE=0 status to HAL layer.
 */
void mdp3_check_dsi_ctrl_status(struct work_struct *work,
				uint32_t interval)
{
	struct dsi_status_data *pdsi_status = NULL;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdp3_session_data *mdp3_session = NULL;
	int ret = 0;

	pdsi_status = container_of(to_delayed_work(work),
	struct dsi_status_data, check_status);

	if (!pdsi_status || !(pdsi_status->mfd)) {
		pr_err("%s: mfd not available\n", __func__);
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

	mdp3_session = pdsi_status->mfd->mdp.private1;
	if (!mdp3_session) {
		pr_err("%s: Display is off\n", __func__);
		return;
	}

	if (mdp3_session->in_splash_screen) {
		schedule_delayed_work(&pdsi_status->check_status,
			msecs_to_jiffies(interval));
		pr_debug("%s: cont splash is on\n", __func__);
		return;
	}

	mutex_lock(&mdp3_session->lock);
	if (!mdp3_session->status) {
		pr_debug("%s: display off already\n", __func__);
		mutex_unlock(&mdp3_session->lock);
		return;
	}

	if (mdp3_session->wait_for_dma_done)
		ret = mdp3_session->wait_for_dma_done(mdp3_session);

	if (!ret)
		ret = ctrl_pdata->check_status(ctrl_pdata);
	else
		pr_err("%s: wait_for_dma_done error\n", __func__);
	mutex_unlock(&mdp3_session->lock);

	if (mdss_fb_is_power_on_interactive(pdsi_status->mfd)) {
		if (ret > 0) {
			schedule_delayed_work(&pdsi_status->check_status,
						msecs_to_jiffies(interval));
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

