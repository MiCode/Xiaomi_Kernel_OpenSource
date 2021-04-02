// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/list_sort.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/devfreq.h>
#include <linux/pm_runtime.h>
#include "governor.h"

#include "apu_devfreq.h"
#include "apu_common.h"
#include "apu_log.h"
#include "apu_clk.h"
#include "apu_trace.h"

static int update_parent(struct apu_gov_data *gov_data)
{
	int ret = 0;
	struct apu_dev *ad = NULL;
	struct apu_gov_data *parent_gov = NULL;
	struct apu_req *req = NULL;
	struct apu_req *req_parent = NULL;

	get_datas(gov_data, &parent_gov, &ad, NULL);

	req = list_first_entry(&gov_data->head, struct apu_req, list);
	/* Lock parent's mutex, update child's opp and get max of them */
	mutex_lock_nested(&parent_gov->this->lock, parent_gov->depth);

	gov_data->req_parent.value = req->value;
	list_sort(NULL, &parent_gov->head, apu_cmp);
	req_parent = list_first_entry(&parent_gov->head, struct apu_req, list);
	if (req_parent->value < req->value)
		goto out;

	/* if !parent->polling_ms, let parent's devfreq_monitor update */
	if (!parent_gov->this->profile->polling_ms) {
		ret = update_devfreq(gov_data->parent);
		if (ret < 0 && ret != -EPROBE_DEFER)
			advfs_err(ad->dev, "[%s] update \"%s\" freq fail, ret %d\n",
				__func__, apu_dev_name(gov_data->parent->dev.parent), ret);
	}

out:
	mutex_unlock(&parent_gov->this->lock);

	return ret;
}


static int agov_get_target_freq(struct devfreq *df, unsigned long *freq)
{
	struct apu_gov_data *gov_data = (struct apu_gov_data *)df->data;
	struct apu_dev *ad = NULL;
	struct apu_req *req = NULL;

	get_datas(gov_data, NULL, &ad, NULL);
	req = list_first_entry(&gov_data->head, struct apu_req, list);
	*freq = apu_opp2freq(ad, req->value);
	if (!round_khz(*freq, df->previous_freq)) {
		apu_dump_list(gov_data);
		apupw_dbg_dvfs_tag_update(APUGOV_PASSIVE, apu_dev_name(ad->dev),
			apu_dev_name(req->dev), (u32)req->value, TOMHZ(*freq));
		advfs_info(ad->dev, "[%s] %s vote opp/freq %d/%u\n", __func__,
			   apu_dev_name(req->dev), req->value, TOMHZ(*freq));
	}
	return 0;
}

static int agov_notifier_call(struct notifier_block *nb,
				unsigned long event, void *ptr)
{
	struct apu_gov_data *gov_data
			= container_of(nb, struct apu_gov_data, nb);
	struct devfreq_freqs *freqs = NULL;
	int ret = 0;

	/* no parrent, no need to send PRE/POST CHANGE to any one */
	if (IS_ERR_OR_NULL(gov_data->parent))
		goto out;

	/* Preparing parameters needed for notifing parents */
	freqs = (struct devfreq_freqs *)ptr;
	switch (event) {
	case DEVFREQ_PRECHANGE:
		/* if freq up, update parent in PRECHANGE */
		if (freqs->new > freqs->old)
			ret = update_parent(gov_data);
		break;
	case DEVFREQ_POSTCHANGE:
		/* if freq down, update parent in POSTCHANGE */
		if (freqs->new < freqs->old)
			ret = update_parent(gov_data);
		break;
	}

	if (ret)
		return ret;
out:
	return NOTIFY_DONE;
}

static int agov_event_handler(struct devfreq *df,
				unsigned int event, void *data)
{
	struct apu_gov_data *gov_data = (struct apu_gov_data *)df->data;
	struct notifier_block *nb = &gov_data->nb;
	struct apu_dev *ad = NULL;
	struct apu_gov_data *parent_gov = NULL;
	struct device *dev = NULL;
	int ret = 0;

	switch (event) {
	case DEVFREQ_GOV_START:
		if (!gov_data->this)
			gov_data->this = df;
		get_datas(gov_data, NULL, NULL, &dev);
		nb->notifier_call = agov_notifier_call;
		ret = devm_devfreq_register_notifier(dev, df, nb,
					DEVFREQ_TRANSITION_NOTIFIER);
		/* register notifier ok but just keep monitor suspend */
		if (!ret) {
			devfreq_monitor_start(df);
			devfreq_monitor_suspend(df);
		}
		break;

	case DEVFREQ_GOV_STOP:
		get_datas(gov_data, NULL, NULL, &dev);
		devm_devfreq_unregister_notifier(dev, df,
						 nb,
						 DEVFREQ_TRANSITION_NOTIFIER);
		devfreq_monitor_stop(df);
		break;

	case DEVFREQ_GOV_UPDATE_INTERVAL:
		devfreq_update_interval(df, (unsigned int *)data);
		break;

	case DEVFREQ_GOV_SUSPEND:
		get_datas(gov_data, &parent_gov, &ad, &dev);

		/* cancel devfreq work queue */
		devfreq_monitor_suspend(df);

		/* restore to default opp */
		gov_data->req.value = gov_data->max_opp;
		apu_dump_list(gov_data);

		/* only allow leaf to update parent's opp */
		if (!gov_data->depth) {
			mutex_lock_nested(&gov_data->this->lock, gov_data->depth);
			list_sort(NULL, &gov_data->head, apu_cmp);
			update_parent(gov_data);
			mutex_unlock(&gov_data->this->lock);
		}
		break;

	case DEVFREQ_GOV_RESUME:
		devfreq_monitor_resume(df);
		break;

	default:
		break;
	}

	return ret;
}

struct devfreq_governor agov_passive = {
	.name = APUGOV_PASSIVE,
	.get_target_freq = agov_get_target_freq,
	.event_handler = agov_event_handler,
};

