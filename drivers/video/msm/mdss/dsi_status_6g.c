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

#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include "mdss_dsi.h"
#include "mdss_mdp.h"

/*
 * mdss_check_dsi_ctrl_status() - Check MDP5 DSI controller status periodically.
 * @work     : dsi controller status data
 * @interval : duration in milliseconds to schedule work queue
 *
 * This function calls check_status API on DSI controller to send the BTA
 * command. If DSI controller fails to acknowledge the BTA command, it sends
 * the PANEL_ALIVE=0 status to HAL layer.
 */
void mdss_check_dsi_ctrl_status(struct work_struct *work, uint32_t interval)
{
	struct dsi_status_data *pstatus_data = NULL;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_overlay_private *mdp5_data = NULL;
	struct mdss_mdp_ctl *ctl = NULL;
	int ret = 0;

	pstatus_data = container_of(to_delayed_work(work),
		struct dsi_status_data, check_status);
	if (!pstatus_data || !(pstatus_data->mfd)) {
		pr_err("%s: mfd not available\n", __func__);
		return;
	}

	pdata = dev_get_platdata(&pstatus_data->mfd->pdev->dev);
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

	mdp5_data = mfd_to_mdp5_data(pstatus_data->mfd);
	ctl = mfd_to_ctl(pstatus_data->mfd);

	if (!ctl) {
		pr_err("%s: Display is off\n", __func__);
		return;
	}

	if (ctl->power_state == MDSS_PANEL_POWER_OFF) {
		schedule_delayed_work(&pstatus_data->check_status,
			msecs_to_jiffies(interval));
		pr_err("%s: ctl not powered on\n", __func__);
		return;
	}

	mutex_lock(&ctrl_pdata->mutex);
	if (ctl->shared_lock)
		mutex_lock(ctl->shared_lock);
	mutex_lock(&mdp5_data->ov_lock);

	if (pstatus_data->mfd->shutdown_pending) {
		mutex_unlock(&mdp5_data->ov_lock);
		if (ctl->shared_lock)
			mutex_unlock(ctl->shared_lock);
		mutex_unlock(&ctrl_pdata->mutex);
		pr_err("%s: DSI turning off, avoiding panel status check\n",
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

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	ret = ctrl_pdata->check_status(ctrl_pdata);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	mutex_unlock(&mdp5_data->ov_lock);
	if (ctl->shared_lock)
		mutex_unlock(ctl->shared_lock);
	mutex_unlock(&ctrl_pdata->mutex);

	if ((pstatus_data->mfd->panel_power_state == MDSS_PANEL_POWER_ON)) {
		if (ret > 0) {
			schedule_delayed_work(&pstatus_data->check_status,
				msecs_to_jiffies(interval));
		} else {
			char *envp[2] = {"PANEL_ALIVE=0", NULL};
			pdata->panel_info.panel_dead = true;
			ret = kobject_uevent_env(
				&pstatus_data->mfd->fbi->dev->kobj,
							KOBJ_CHANGE, envp);
			pr_err("%s: Panel has gone bad, sending uevent - %s\n",
							__func__, envp[0]);
		}
	}
}
