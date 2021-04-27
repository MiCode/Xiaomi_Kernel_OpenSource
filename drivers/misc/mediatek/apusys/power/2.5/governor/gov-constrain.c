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
	struct apu_req *req_parent, *req;

	get_datas(gov_data, &parent_gov, &ad, NULL);
	req = list_first_entry(&gov_data->head, struct apu_req, list);
	advfs_info(ad->dev, "n_opp/threshold/child_limit %d/%d/%d",
		   req->value, gov_data->threshold_opp, parent_gov->child_opp_limit);

	if (gov_data->threshold_opp < 0 || parent_gov->child_opp_limit < 0) {
		advfs_err(ad->dev, "[%s] wrong threshold/child_limit\n", __func__);
		return -EINVAL;
	}
	/* Lock parent's mutex, update child's opp and get max of them */
	mutex_lock_nested(&parent_gov->this->lock, parent_gov->depth);

	/*
	 * The fastst opp that child can vote is child_opp_limit
	 * and if child's opp not faster than the threshold, put child's
	 * voting as parent's slowest opp.
	 * That means there are only 2 possibilities for voting parent
	 *  1. vote to parent as parent->child_opp_limit
	 *  2. vote to parent ad parent->max_opp
	 */
	if (req->value <= gov_data->threshold_opp)
		req->value = parent_gov->child_opp_limit;
	else
		req->value = parent_gov->max_opp;

	gov_data->req_parent.value = req->value;
	list_sort(NULL, &parent_gov->head, apu_cmp);
	req_parent = list_first_entry(&parent_gov->head, struct apu_req, list);

	/* child will leave when parent already faster than child_opp_limit */
	if (req_parent->value < parent_gov->child_opp_limit)
		goto out;

	ret = update_devfreq(gov_data->parent);
	if (ret < 0 && ret != -EPROBE_DEFER)
		advfs_err(ad->dev, "[%s] update \"%s\" freq fail, ret %d\n",
			  __func__, apu_dev_name(gov_data->parent->dev.parent), ret);
out:
	mutex_unlock(&parent_gov->this->lock);

	return ret;
}

static int aconstrain_get_target_freq(struct devfreq *df, unsigned long *freq)
{
	struct apu_gov_data *gov_data = (struct apu_gov_data *)df->data;
	struct apu_dev *ad = NULL;
	struct apu_req *req = NULL;

	get_datas(gov_data, NULL, &ad, NULL);
	req = list_first_entry(&gov_data->head, struct apu_req, list);
	*freq = apu_opp2freq(ad, req->value);
	if (!round_khz(*freq, df->previous_freq)) {
		apu_dump_list(gov_data);
		apupw_dbg_dvfs_tag_update(APUGOV_CONSTRAIN, apu_dev_name(ad->dev),
			apu_dev_name(req->dev), (u32)req->value, TOMHZ(*freq));
		advfs_info(ad->dev, "[%s] %s vote opp/freq %d/%u\n", __func__,
			   apu_dev_name(req->dev), req->value, TOMHZ(*freq));
	}
	return 0;
}

static int aconstrain_notifier_call(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct apu_gov_data *gov_data =
		container_of(nb, struct apu_gov_data, nb);
	struct devfreq_freqs *freqs = NULL;
	int ret = 0;

	/* no parrent, no need to send PRE/POST CHANGE to any one */
	if (!gov_data->parent)
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

static int aconstrain_event_handler(struct devfreq *df,
				    unsigned int event, void *data)
{
	struct apu_gov_data *gov_data = (struct apu_gov_data *)df->data;
	struct notifier_block *nb = &gov_data->nb;
	int ret = 0;
	struct apu_dev *ad = NULL;
	struct apu_gov_data *parent_gov = NULL;
	struct device *dev = NULL;

	switch (event) {
	case DEVFREQ_GOV_START:
		if (!gov_data->this)
			gov_data->this = df;
		get_datas(gov_data, NULL, NULL, &dev);
		nb->notifier_call = aconstrain_notifier_call;
		ret = devm_devfreq_register_notifier(dev, df, nb,
						     DEVFREQ_TRANSITION_NOTIFIER);
		break;
	case DEVFREQ_GOV_STOP:
		get_datas(gov_data, NULL, NULL, &dev);
		devm_devfreq_unregister_notifier(dev, df, nb,
						 DEVFREQ_TRANSITION_NOTIFIER);
		break;
	case DEVFREQ_GOV_SUSPEND:
		get_datas(gov_data, &parent_gov, &ad, &dev);

		/* restore req to default opp */
		gov_data->req.value = gov_data->max_opp;
		apu_dump_list(gov_data);

		/* restore parent's req as default opp */
		if (!IS_ERR_OR_NULL(parent_gov)) {
			mutex_lock_nested(&parent_gov->this->lock, parent_gov->depth);
			gov_data->req_parent.value = parent_gov->max_opp;
			apu_dump_list(parent_gov);
			list_sort(NULL, &parent_gov->head, apu_cmp);
			ret = update_devfreq(gov_data->parent);
			mutex_unlock(&parent_gov->this->lock);
		}
		break;
	case DEVFREQ_GOV_UPDATE_INTERVAL:
	case DEVFREQ_GOV_RESUME:
	default:
		break;
	}

	return ret;
}

struct devfreq_governor agov_constrain = {
	.name = APUGOV_CONSTRAIN,
	.get_target_freq = aconstrain_get_target_freq,
	.event_handler = aconstrain_event_handler,
};

