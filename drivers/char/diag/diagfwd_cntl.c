/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/slab.h>
#include <linux/diagchar.h>
#include <linux/platform_device.h>
#include <linux/kmemleak.h>
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_cntl.h"
/* tracks which peripheral is undergoing SSR */
static uint16_t reg_dirty;
#define HDR_SIZ 8

void diag_clean_reg_fn(struct work_struct *work)
{
	struct diag_smd_info *smd_info = container_of(work,
						struct diag_smd_info,
						diag_notify_update_smd_work);
	if (!smd_info)
		return;

	pr_debug("diag: clean registration for peripheral: %d\n",
		smd_info->peripheral);

	reg_dirty |= smd_info->peripheral_mask;
	diag_clear_reg(smd_info->peripheral);
	reg_dirty ^= smd_info->peripheral_mask;

	smd_info->notify_context = 0;
}

/* Process the data read from the smd control channel */
int diag_process_smd_cntl_read_data(struct diag_smd_info *smd_info, void *buf,
								int total_recd)
{
	int data_len = 0, type = -1, count_bytes = 0, j, flag = 0;
	int feature_mask_len;
	struct bindpkt_params_per_process *pkt_params =
		kzalloc(sizeof(struct bindpkt_params_per_process), GFP_KERNEL);
	struct diag_ctrl_msg *msg;
	struct cmd_code_range *range;
	struct bindpkt_params *temp;

	if (pkt_params == NULL) {
		pr_alert("diag: In %s, Memory allocation failure\n",
			__func__);
		return 0;
	}

	if (!smd_info) {
		pr_err("diag: In %s, No smd info. Not able to read.\n",
			__func__);
		kfree(pkt_params);
		return 0;
	}

	while (count_bytes + HDR_SIZ <= total_recd) {
		type = *(uint32_t *)(buf);
		data_len = *(uint32_t *)(buf + 4);
		if (type < DIAG_CTRL_MSG_REG ||
				 type > DIAG_CTRL_MSG_F3_MASK_V2) {
			pr_alert("diag: In %s, Invalid Msg type %d proc %d",
				 __func__, type, smd_info->peripheral);
			break;
		}
		if (data_len < 0 || data_len > total_recd) {
			pr_alert("diag: In %s, Invalid data len %d, total_recd: %d, proc %d",
				 __func__, data_len, total_recd,
				 smd_info->peripheral);
			break;
		}
		count_bytes = count_bytes+HDR_SIZ+data_len;
		if (type == DIAG_CTRL_MSG_REG && total_recd >= count_bytes) {
			msg = buf+HDR_SIZ;
			range = buf+HDR_SIZ+
					sizeof(struct diag_ctrl_msg);
			pkt_params->count = msg->count_entries;
			temp = kzalloc(pkt_params->count * sizeof(struct
					 bindpkt_params), GFP_KERNEL);
			if (temp == NULL) {
				pr_alert("diag: In %s, Memory alloc fail\n",
					__func__);
				kfree(pkt_params);
				return flag;
			}
			for (j = 0; j < pkt_params->count; j++) {
				temp->cmd_code = msg->cmd_code;
				temp->subsys_id = msg->subsysid;
				temp->client_id = smd_info->peripheral;
				temp->proc_id = NON_APPS_PROC;
				temp->cmd_code_lo = range->cmd_code_lo;
				temp->cmd_code_hi = range->cmd_code_hi;
				range++;
				temp++;
			}
			temp -= pkt_params->count;
			pkt_params->params = temp;
			flag = 1;
			/* peripheral undergoing SSR should not
			 * record new registration
			 */
			if (!(reg_dirty & smd_info->peripheral_mask))
				diagchar_ioctl(NULL, DIAG_IOCTL_COMMAND_REG,
						(unsigned long)pkt_params);
			else
				pr_err("diag: drop reg proc %d\n",
						smd_info->peripheral);
			kfree(temp);
		} else if ((type == DIAG_CTRL_MSG_FEATURE) &&
				(smd_info->peripheral == MODEM_DATA)) {
			feature_mask_len = *(int *)(buf + 8);
			driver->log_on_demand_support = (*(uint8_t *)
							 (buf + 12)) & 0x04;
		} else if (type != DIAG_CTRL_MSG_REG) {
			flag = 1;
		}
		buf = buf + HDR_SIZ + data_len;
	}
	kfree(pkt_params);

	return flag;
}

static int diag_smd_cntl_probe(struct platform_device *pdev)
{
	int r = 0;
	int index = -1;

	/* open control ports only on 8960 & newer targets */
	if (chk_apps_only()) {
		if (pdev->id == SMD_APPS_MODEM) {
			index = MODEM_DATA;
			r = smd_open("DIAG_CNTL",
					&driver->smd_cntl[index].ch,
					&driver->smd_cntl[index],
					diag_smd_notify);
			driver->smd_cntl[index].ch_save =
					driver->smd_cntl[index].ch;
		} else if (pdev->id == SMD_APPS_QDSP) {
			index = LPASS_DATA;
			r = smd_named_open_on_edge("DIAG_CNTL",
					SMD_APPS_QDSP,
					&driver->smd_cntl[index].ch,
					&driver->smd_cntl[index],
					diag_smd_notify);
			driver->smd_cntl[index].ch_save =
					driver->smd_cntl[index].ch;
		} else if (pdev->id == SMD_APPS_WCNSS) {
			index = WCNSS_DATA;
			r = smd_named_open_on_edge("APPS_RIVA_CTRL",
					SMD_APPS_WCNSS,
					&driver->smd_cntl[index].ch,
					&driver->smd_cntl[index],
					diag_smd_notify);
			driver->smd_cntl[index].ch_save =
					driver->smd_cntl[index].ch;
		}

		pr_debug("diag: open CNTL port, ID = %d,r = %d\n", pdev->id, r);
	}
	return 0;
}

static int diagfwd_cntl_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int diagfwd_cntl_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops diagfwd_cntl_dev_pm_ops = {
	.runtime_suspend = diagfwd_cntl_runtime_suspend,
	.runtime_resume = diagfwd_cntl_runtime_resume,
};

static struct platform_driver msm_smd_ch1_cntl_driver = {

	.probe = diag_smd_cntl_probe,
	.driver = {
			.name = "DIAG_CNTL",
			.owner = THIS_MODULE,
			.pm   = &diagfwd_cntl_dev_pm_ops,
		   },
};

static struct platform_driver diag_smd_lite_cntl_driver = {

	.probe = diag_smd_cntl_probe,
	.driver = {
			.name = "APPS_RIVA_CTRL",
			.owner = THIS_MODULE,
			.pm   = &diagfwd_cntl_dev_pm_ops,
		   },
};

void diagfwd_cntl_init(void)
{
	int success;
	int i;

	reg_dirty = 0;
	driver->polling_reg_flag = 0;
	driver->log_on_demand_support = 1;
	driver->diag_cntl_wq = create_singlethread_workqueue("diag_cntl_wq");

	success = diag_smd_constructor(&driver->smd_cntl[MODEM_DATA],
					MODEM_DATA, SMD_CNTL_TYPE);
	if (!success)
		goto err;

	success = diag_smd_constructor(&driver->smd_cntl[LPASS_DATA],
					LPASS_DATA, SMD_CNTL_TYPE);
	if (!success)
		goto err;

	success = diag_smd_constructor(&driver->smd_cntl[WCNSS_DATA],
					WCNSS_DATA, SMD_CNTL_TYPE);
	if (!success)
		goto err;

	platform_driver_register(&msm_smd_ch1_cntl_driver);
	platform_driver_register(&diag_smd_lite_cntl_driver);

	return;
err:
	pr_err("diag: Could not initialize diag buffers");

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_cntl[i]);

	if (driver->diag_cntl_wq)
		destroy_workqueue(driver->diag_cntl_wq);
}

void diagfwd_cntl_exit(void)
{
	int i;

	for (i = 0; i < NUM_SMD_CONTROL_CHANNELS; i++)
		diag_smd_destructor(&driver->smd_cntl[i]);

	destroy_workqueue(driver->diag_cntl_wq);

	platform_driver_unregister(&msm_smd_ch1_cntl_driver);
	platform_driver_unregister(&diag_smd_lite_cntl_driver);
}
