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
#include <linux/slab.h>

#include "governor.h"

#include "apu_devfreq.h"
#include "apu_common.h"
#include "apu_log.h"
#include "apu_clk.h"
#include "apu_trace.h"

/*
 * apu_pe_head: use to link leaf's current opp
 * apu_pe_mutex: protect apu_pe_head list
 */
static LIST_HEAD(apu_pe_head);
static DEFINE_MUTEX(apu_pe_mutex);

/**
 * update_parent() - update gov_data->req_parent.value.
 * @gov_data: apu_dev's governor data structure.
 *
 * 1. update gov_data->req_parent.value.
 * 2. list_sort(parent's head)
 * 3. update parent's opp if parent's opp is higher then apu_dev's opp
 *
 * Context: Process context.
 * Return:
 * * 0          - OK
 * * < 0        - if update parnt's opp get some error
 */
static int update_parent(struct apu_gov_data *gov_data)
{
	int ret = 0;
	struct apu_dev *ad = NULL;
	struct apu_gov_data *parent_gov = NULL;
	struct apu_req *req = NULL, *req_parent = NULL;

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
	struct apu_req *pe_req = NULL;

	get_datas(gov_data, NULL, &ad, NULL);
	req = list_first_entry(&gov_data->head, struct apu_req, list);

	/*
	 * 1. put user request into pe's list,
	 * 2. sorting pe's list
	 * 3. pick out 1st of pe's list
	 * 4. overwrite user's req->value, if pe's 1st req->value faster
	 */
	mutex_lock(&apu_pe_mutex);
	gov_data->gov_pe.req->value = req->value;
	list_sort(NULL, &apu_pe_head, apu_cmp);
	pe_req = list_first_entry(&apu_pe_head, struct apu_req, list);
	if (pe_req->value < req->value) {
		apu_dump_pe_gov(ad, &apu_pe_head);
		req->value = pe_req->value;
	}
	mutex_unlock(&apu_pe_mutex);

	*freq = apu_opp2freq(ad, req->value);
	if (!round_khz(*freq, df->previous_freq)) {
		apu_dump_list(gov_data);
		apupw_dbg_dvfs_tag_update(APUGOV_PASSIVE_PE, apu_dev_name(ad->dev),
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
	struct apu_req *req = NULL;

	switch (event) {
	case DEVFREQ_GOV_START:
		if (!gov_data->this)
			gov_data->this = df;
		get_datas(gov_data, NULL, &ad, &dev);
		nb->notifier_call = agov_notifier_call;
		ret = devm_devfreq_register_notifier(dev, df, nb,
					DEVFREQ_TRANSITION_NOTIFIER);
		/* register notifier ok but just keep monitor suspend */
		if (!ret) {
			devfreq_monitor_start(df);
			devfreq_monitor_suspend(df);
		}

		/* Initialize power efficiency request */
		req = kmalloc(sizeof(*req), GFP_KERNEL);
		if (!req)
			return -ENOMEM;

		req->dev = ad->dev;
		req->value = df->profile->max_state - 1;
		gov_data->gov_pe.req = req;
		mutex_lock(&apu_pe_mutex);
		list_add(&req->list, &apu_pe_head);
		mutex_unlock(&apu_pe_mutex);
		break;

	case DEVFREQ_GOV_STOP:
		get_datas(gov_data, NULL, NULL, &dev);
		devm_devfreq_unregister_notifier(dev, df, nb,
						 DEVFREQ_TRANSITION_NOTIFIER);
		devfreq_monitor_stop(df);
		mutex_lock(&apu_pe_mutex);
		list_del(&gov_data->gov_pe.req->list);
		mutex_unlock(&apu_pe_mutex);
		kfree(gov_data->gov_pe.req);
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
		mutex_lock(&apu_pe_mutex);
		gov_data->gov_pe.req->value = gov_data->max_opp;
		mutex_unlock(&apu_pe_mutex);

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

struct devfreq_governor agov_passive_pe = {
	.name = APUGOV_PASSIVE_PE,
	.get_target_freq = agov_get_target_freq,
	.event_handler = agov_event_handler,
};

