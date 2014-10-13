/*
*Copyright (c) 2014, The Linux Foundation. All rights reserved.
*
*This program is free software; you can redistribute it and/or modify
*it under the terms of the GNU General Public License version 2 and
*only version 2 as published by the Free Software Foundation.
*
*This program is distributed in the hope that it will be useful,
*but WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*GNU General Public License for more details.
*/

#include <linux/devfreq.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/hvc.h>
#include "governor.h"
#include "devfreq_spdm.h"

enum msm_spdm_rt_res {
	SPDM_RES_ID = 1,
	SPDM_RES_TYPE = 0x63707362,
	SPDM_KEY = 0x00006e65,
	SPDM_SIZE = 4,
};

static LIST_HEAD(devfreqs);
static DEFINE_MUTEX(devfreqs_lock);

static int enable_clocks(void)
{
	struct msm_rpm_request *rpm_req;
	int id;
	const int one = 1;
	rpm_req = msm_rpm_create_request(MSM_RPM_CTX_ACTIVE_SET, SPDM_RES_TYPE,
					 SPDM_RES_ID, 1);
	if (!rpm_req)
		return -ENODEV;
	msm_rpm_add_kvp_data(rpm_req, SPDM_KEY, (const uint8_t *)&one,
			     sizeof(int));
	id = msm_rpm_send_request(rpm_req);
	msm_rpm_wait_for_ack(id);
	msm_rpm_free_request(rpm_req);

	return 0;
}

static int disable_clocks(void)
{
	struct msm_rpm_request *rpm_req;
	int id;
	const int zero = 0;
	rpm_req = msm_rpm_create_request(MSM_RPM_CTX_ACTIVE_SET, SPDM_RES_TYPE,
					 SPDM_RES_ID, 1);
	if (!rpm_req)
		return -ENODEV;
	msm_rpm_add_kvp_data(rpm_req, SPDM_KEY, (const uint8_t *)&zero,
			     sizeof(int));
	id = msm_rpm_send_request(rpm_req);
	msm_rpm_wait_for_ack(id);
	msm_rpm_free_request(rpm_req);

	return 0;
}

static irqreturn_t threaded_isr(int irq, void *dev_id)
{
	struct spdm_data *data;
	struct hvc_desc desc = { { 0 } };
	int hvc_status = 0;

	/* call hyp to get bw_vote */
	desc.arg[0] = SPDM_CMD_GET_BW_ALL;
	hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
	if (hvc_status)
		pr_err("HVC command %u failed with error %u", (int)desc.arg[0],
			hvc_status);
	mutex_lock(&devfreqs_lock);
	list_for_each_entry(data, &devfreqs, list) {
		if (data->spdm_client == desc.ret[0]) {
			devfreq_monitor_suspend(data->devfreq);
			mutex_lock(&data->devfreq->lock);
			data->action = SPDM_UP;
			data->new_bw = desc.ret[1] >> 6;
			update_devfreq(data->devfreq);
			data->action = SPDM_DOWN;
			mutex_unlock(&data->devfreq->lock);
			devfreq_monitor_resume(data->devfreq);
			break;
		}
	}
	mutex_unlock(&devfreqs_lock);
	return IRQ_HANDLED;
}

static irqreturn_t isr(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

static int gov_spdm_hyp_target_bw(struct devfreq *devfreq, unsigned long *freq,
				  u32 *flag)
{
	struct devfreq_dev_status status;
	int ret = -EINVAL;
	int usage;
	struct hvc_desc desc = { { 0 } };
	int hvc_status = 0;

	if (!devfreq || !devfreq->profile || !devfreq->profile->get_dev_status)
		return ret;

	ret = devfreq->profile->get_dev_status(devfreq->dev.parent, &status);
	if (ret)
		return ret;

	usage = (status.busy_time * 100) / status.total_time;

	if (usage > 0) {
		/* up was already called as part of hyp, so just use the
		 * already stored values */
		*freq = ((struct spdm_data *)devfreq->data)->new_bw;
	} else {
		desc.arg[0] = SPDM_CMD_GET_BW_SPECIFIC;
		desc.arg[1] = ((struct spdm_data *)devfreq->data)->spdm_client;
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);
		*freq = desc.ret[0] >> 6;
	}

	return 0;
}

static int gov_spdm_hyp_eh(struct devfreq *devfreq, unsigned int event,
			   void *data)
{
	struct hvc_desc desc = { { 0 } };
	int hvc_status = 0;
	struct spdm_data *spdm_data = (struct spdm_data *)devfreq->data;
	int i;

	switch (event) {
	case DEVFREQ_GOV_START:
		mutex_lock(&devfreqs_lock);
		list_add(&spdm_data->list, &devfreqs);
		mutex_unlock(&devfreqs_lock);
		/* call hyp with config data */
		desc.arg[0] = SPDM_CMD_CFG_PORTS;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.num_ports;
		for (i = 0; i < spdm_data->config_data.num_ports; i++)
			desc.arg[i+3] = spdm_data->config_data.ports[i];
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);

		desc.arg[0] = SPDM_CMD_CFG_FLTR;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.aup;
		desc.arg[3] = spdm_data->config_data.adown;
		desc.arg[4] = spdm_data->config_data.bucket_size;
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);

		desc.arg[0] = SPDM_CMD_CFG_PL;
		desc.arg[1] = spdm_data->spdm_client;
		for (i = 0; i < SPDM_PL_COUNT - 1; i++)
			desc.arg[i+2] = spdm_data->config_data.pl_freqs[i];
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);

		desc.arg[0] = SPDM_CMD_CFG_REJRATE_LOW;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.reject_rate[0];
		desc.arg[3] = spdm_data->config_data.reject_rate[1];
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);
		desc.arg[0] = SPDM_CMD_CFG_REJRATE_MED;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.reject_rate[2];
		desc.arg[3] = spdm_data->config_data.reject_rate[3];
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);
		desc.arg[0] = SPDM_CMD_CFG_REJRATE_HIGH;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.reject_rate[4];
		desc.arg[3] = spdm_data->config_data.reject_rate[5];
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);

		desc.arg[0] = SPDM_CMD_CFG_RESPTIME_LOW;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.response_time_us[0];
		desc.arg[3] = spdm_data->config_data.response_time_us[1];
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);
		desc.arg[0] = SPDM_CMD_CFG_RESPTIME_MED;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.response_time_us[2];
		desc.arg[3] = spdm_data->config_data.response_time_us[3];
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);
		desc.arg[0] = SPDM_CMD_CFG_RESPTIME_HIGH;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.response_time_us[4];
		desc.arg[3] = spdm_data->config_data.response_time_us[5];
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);

		desc.arg[0] = SPDM_CMD_CFG_CCIRESPTIME_LOW;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.cci_response_time_us[0];
		desc.arg[3] = spdm_data->config_data.cci_response_time_us[1];
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);
		desc.arg[0] = SPDM_CMD_CFG_CCIRESPTIME_MED;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.cci_response_time_us[2];
		desc.arg[3] = spdm_data->config_data.cci_response_time_us[3];
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);
		desc.arg[0] = SPDM_CMD_CFG_CCIRESPTIME_HIGH;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.cci_response_time_us[4];
		desc.arg[3] = spdm_data->config_data.cci_response_time_us[5];
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);

		desc.arg[0] = SPDM_CMD_CFG_MAXCCI;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.max_cci_freq;
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);

		desc.arg[0] = SPDM_CMD_CFG_VOTES;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = spdm_data->config_data.upstep;
		desc.arg[3] = spdm_data->config_data.downstep;
		desc.arg[4] = spdm_data->config_data.max_vote;
		desc.arg[5] = spdm_data->config_data.up_step_multp;
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);

		/* call hyp enable/commit */
		desc.arg[0] = SPDM_CMD_ENABLE;
		desc.arg[1] = spdm_data->spdm_client;
		desc.arg[2] = 0;
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status) {
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);
			return -EINVAL;
		}
		devfreq_monitor_start(devfreq);
		break;

	case DEVFREQ_GOV_STOP:
		devfreq_monitor_stop(devfreq);
		/* find devfreq in list and remove it */
		mutex_lock(&devfreqs_lock);
		list_del(&spdm_data->list);
		mutex_unlock(&devfreqs_lock);

		/* call hypvervisor to disable */
		desc.arg[0] = SPDM_CMD_DISABLE;
		desc.arg[1] = spdm_data->spdm_client;
		hvc_status = hvc(HVC_FN_SIP(SPDM_HYP_FNID), &desc);
		if (hvc_status)
			pr_err("HVC command %u failed with error %u",
				(int)desc.arg[0], hvc_status);
		break;

	case DEVFREQ_GOV_INTERVAL:
		devfreq_interval_update(devfreq, (unsigned int *)data);
		break;

	case DEVFREQ_GOV_SUSPEND:
		devfreq_monitor_suspend(devfreq);
		break;

	case DEVFREQ_GOV_RESUME:
		devfreq_monitor_resume(devfreq);
		break;

	default:
		break;
	}

	return 0;
}

static struct devfreq_governor spdm_hyp_gov = {
	.name = "spdm_bw_hyp",
	.get_target_freq = gov_spdm_hyp_target_bw,
	.event_handler = gov_spdm_hyp_eh,
};

static int probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	int *irq = 0;

	irq = devm_kzalloc(&pdev->dev, sizeof(int), GFP_KERNEL);
	if (!irq)
		return -ENOMEM;
	platform_set_drvdata(pdev, irq);

	ret = devfreq_add_governor(&spdm_hyp_gov);
	if (ret)
		goto nogov;

	*irq = platform_get_irq_byname(pdev, "spdm-irq");
	ret = request_threaded_irq(*irq, isr, threaded_isr,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   spdm_hyp_gov.name, pdev);
	if (ret)
		goto no_irq;

	enable_clocks();
	return 0;

no_irq:
	devfreq_remove_governor(&spdm_hyp_gov);
nogov:
	devm_kfree(&pdev->dev, irq);
	return ret;
}

static int remove(struct platform_device *pdev)
{
	int *irq = 0;

	disable_clocks();
	irq = platform_get_drvdata(pdev);
	free_irq(*irq, pdev);
	devfreq_remove_governor(&spdm_hyp_gov);
	devm_kfree(&pdev->dev, irq);
	return 0;
}

static const struct of_device_id gov_spdm_match[] = {
	{.compatible = "qcom,gov_spdm_hyp"},
	{}
};

static struct platform_driver gov_spdm_hyp_drvr = {
	.driver = {
		   .name = "gov_spdm_hyp",
		   .owner = THIS_MODULE,
		   .of_match_table = gov_spdm_match,
		   },
	.probe = probe,
	.remove = remove,
};

static int __init governor_spdm_bw_hyp(void)
{
	return platform_driver_register(&gov_spdm_hyp_drvr);
}

module_init(governor_spdm_bw_hyp);

MODULE_LICENSE("GPL v2");
