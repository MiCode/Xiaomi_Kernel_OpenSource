/* Copyright (c) 2013-2015, 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>

#include "mdss_dsi.h"
#include "mdp3_ctrl.h"

/*
 * mdp3_check_te_status() - Check the status of panel for TE based ESD.
 * @ctrl_pdata   : dsi controller data
 * @pstatus_data : dsi status data
 * @interval     : duration in milliseconds for panel TE wait
 *
 * This function waits for TE signal from the panel for a maximum
 * duration of 3 vsyncs. If timeout occurs, report the panel to be
 * dead due to ESD attack.
 * NOTE: The TE IRQ handling is linked to the ESD thread scheduling,
 * i.e. rate of TE IRQs firing is bound by the ESD interval.
 */
static int mdp3_check_te_status(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
		struct dsi_status_data *pstatus_data, uint32_t interval)
{
	int ret;

	pr_debug("%s: Checking panel TE status\n", __func__);

	atomic_set(&ctrl_pdata->te_irq_ready, 0);
	reinit_completion(&ctrl_pdata->te_irq_comp);
	enable_irq(gpio_to_irq(ctrl_pdata->disp_te_gpio));

	ret = wait_for_completion_timeout(&ctrl_pdata->te_irq_comp,
			msecs_to_jiffies(interval));

	disable_irq(gpio_to_irq(ctrl_pdata->disp_te_gpio));
	pr_debug("%s: Panel TE check done with ret = %d\n", __func__, ret);

	return ret;
}

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
	struct mipi_panel_info *mipi = NULL;
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

	mipi = &pdata->panel_info.mipi;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

	if (!ctrl_pdata || (!ctrl_pdata->check_status &&
		(ctrl_pdata->status_mode != ESD_TE))) {
		pr_err("%s: DSI ctrl or status_check callback not available\n",
								__func__);
		return;
	}

	if (!pdata->panel_info.esd_rdy) {
		pr_err("%s: unblank not complete, reschedule check status\n",
			__func__);
		schedule_delayed_work(&pdsi_status->check_status,
				msecs_to_jiffies(interval));
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

	if (mipi->mode == DSI_CMD_MODE &&
		mipi->hw_vsync_mode &&
		mdss_dsi_is_te_based_esd(ctrl_pdata)) {
		uint32_t fps = mdss_panel_get_framerate(&pdata->panel_info,
					FPS_RESOLUTION_HZ);
		uint32_t timeout = ((1000 / fps) + 1) *
					MDSS_STATUS_TE_WAIT_MAX;

		if (mdp3_check_te_status(ctrl_pdata, pdsi_status, timeout) > 0)
			goto sim;
		goto status_dead;
	}

	mutex_lock(&mdp3_session->lock);
	if (!mdp3_session->status) {
		pr_debug("%s: display off already\n", __func__);
		mutex_unlock(&mdp3_session->lock);
		return;
	}

	if (mdp3_session->wait_for_dma_done)
		ret = mdp3_session->wait_for_dma_done(mdp3_session);
	mutex_unlock(&mdp3_session->lock);

	if (!ret)
		ret = ctrl_pdata->check_status(ctrl_pdata);
	else
		pr_err("%s: wait_for_dma_done error\n", __func__);

	if (mdss_fb_is_power_on_interactive(pdsi_status->mfd)) {
		if (ret > 0)
			schedule_delayed_work(&pdsi_status->check_status,
						msecs_to_jiffies(interval));
		else
			goto status_dead;
	}
sim:
	if (pdata->panel_info.panel_force_dead) {
		pr_debug("force_dead=%d\n", pdata->panel_info.panel_force_dead);
		pdata->panel_info.panel_force_dead--;
		if (!pdata->panel_info.panel_force_dead)
			goto status_dead;
	}
	return;

status_dead:
	mdss_fb_report_panel_dead(pdsi_status->mfd);
}

