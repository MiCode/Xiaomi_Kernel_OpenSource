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

/**
 * get_datas() - return governor data that may needs
 * @gov_data:	governor data with child_freq (input)
 * @pgov_data:	parent's governor data		  (output)
 * @adev:		apu_dev of this devfreq		  (output)
 * @dev:		struct device of this defreq  (output)
 *
 * This funciton will based on inputparamter, gov_data, to output
 * pgov_data, adev, dev with call by reference.
 *
 */
static void get_datas(struct apu_gov_data *gov_data,
			struct apu_gov_data **pgov_data, struct apu_dev **adev,
			struct device **dev)
{
	struct device *pdev = NULL;

	if (!gov_data) {
		pr_info("%s null gov_data\n", __func__);
		return;
	}

	pdev = gov_data->this->dev.parent;

	/* return pgov_data */
	if (pgov_data)
		if (gov_data->parent)
			*pgov_data = (struct apu_gov_data *)gov_data->parent->data;

	/* return apu_dev */
	if (adev)
		*adev = dev_get_drvdata(pdev);

	/* return struct device */
	if (dev)
		*dev = pdev;
}

/**
 * getmin() - return max freq child
 * @gov_data:	governor data with child_freq
 *
 * Return max element of child_opp array.
 * Here use for loop for comparision, since we expect
 * the number of child will less than 10.
 * If the child grows up, it need to change sorting algorithem
 * with maybe O(nlogn) time.
 */
static u32 getmin(struct apu_gov_data *gov_data, enum DVFS_USER *min_child)
{
	int i = 0;
	struct apu_dev *adev = NULL;
	u32 min = 0;
	struct device *dev = NULL;

	get_datas(gov_data, NULL, &adev, &dev);

	/* assue the dvfs_user 0 is the smallest opp */
	min = *(u32 *)gov_data->child_opp[0];
	*min_child = 0;

	for (i = 0; i < APUSYS_POWER_USER_NUM; i++) {
		if (*(u32 *)gov_data->child_opp[i] < min) {
			if (!IS_ERR_OR_NULL(apu_find_device(i))) {
				min = *(u32 *)gov_data->child_opp[i];
				*min_child = i;
			}
		}
	}
	return min;
}

static int update_parent(struct apu_gov_data *gov_data)
{
	int ret = 0;
	enum DVFS_USER min_child;
	struct apu_dev *adev = NULL;
	u32 *pchild_opp = NULL;
	u32 parent_min = 100;
	struct apu_gov_data *parent_gov = NULL;

	get_datas(gov_data, &parent_gov, &adev, NULL);
	pchild_opp = parent_gov->child_opp[adev->user];

	/* Lock parent's mutex, update child's opp and get max of them */
	mutex_lock_nested(&parent_gov->this->lock, parent_gov->depth);
	/*
	 * Set next opp to parent's child_freq array and call getmin
	 * to pick min opp, fast freq, that children want parent to be.
	 */
	*pchild_opp = gov_data->n_opp;
	/* get parent's min with new voter */
	parent_min = (unsigned long) getmin(parent_gov, &min_child);
	/* parent is faster than voter, let voter out */
	if (parent_min < gov_data->n_opp)
		goto out;

	advfs_info(parent_gov->this->dev.parent,
			"[%s] final opp(%d) voted from child[%s](%d)\n",
			__func__, parent_min, apu_dev_string(min_child), min_child);
	parent_gov->n_opp = parent_min;
	parent_gov->valid = true;

	/* if !parent->polling_ms, let parent's devfreq_monitor update */
	if (!parent_gov->this->profile->polling_ms) {
		ret = update_devfreq(gov_data->parent);
		if (ret < 0 && ret != -EPROBE_DEFER)
			advfs_err(adev->dev, "[%s] update \"%s\" freq fail, ret %d\n",
				__func__, dev_name(gov_data->parent->dev.parent), ret);
	}

out:
	mutex_unlock(&parent_gov->this->lock);

	return ret;
}


static int agov_get_target_freq(struct devfreq *devfreq,
					unsigned long *freq)
{
	struct apu_gov_data *gov_data = (struct apu_gov_data *)devfreq->data;
	struct device *dev = NULL;
	struct apu_dev *ad = NULL;

	get_datas(gov_data, NULL, &ad, &dev);
	if (gov_data->valid) {
		*freq = apu_opp2freq(ad, gov_data->n_opp);
		gov_data->valid = false;
		if (*freq != devfreq->previous_freq)
			goto out;
	}
	devfreq->profile->get_cur_freq(dev, freq);
out:
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
	if (!gov_data->parent)
		return -EPROBE_DEFER;

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
	return NOTIFY_DONE;
}

static int agov_event_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	struct apu_gov_data *gov_data = (struct apu_gov_data *)devfreq->data;
	struct notifier_block *nb = &gov_data->nb;
	int ret = 0;
	struct apu_dev *adev = NULL;
	struct apu_gov_data *pgov_data = NULL;
	struct device *dev = NULL;

	switch (event) {
	case DEVFREQ_GOV_START:
		if (!gov_data->this)
			gov_data->this = devfreq;
		get_datas(gov_data, NULL, NULL, &dev);
		nb->notifier_call = agov_notifier_call;
		ret = devm_devfreq_register_notifier(dev, devfreq, nb,
					DEVFREQ_TRANSITION_NOTIFIER);
		/* register notifier ok but just keep monitor suspend */
		if (!ret) {
			devfreq_monitor_start(devfreq);
			devfreq_monitor_suspend(devfreq);
		}

		break;

	case DEVFREQ_GOV_STOP:
		get_datas(gov_data, NULL, NULL, &dev);
		devm_devfreq_unregister_notifier(dev, devfreq,
						 nb,
						 DEVFREQ_TRANSITION_NOTIFIER);
		devfreq_monitor_stop(devfreq);
		break;

	case DEVFREQ_GOV_INTERVAL:
		devfreq_interval_update(devfreq, (unsigned int *)data);
		break;

	case DEVFREQ_GOV_SUSPEND:
		get_datas(gov_data, &pgov_data, &adev, &dev);

		/* cancel devfreq work queue */
		devfreq_monitor_suspend(devfreq);

		/* restore back this apu_dev's default opp */
		gov_data->n_opp = gov_data->this->profile->max_state - 1;

		/* only allow leaf to update parent's opp */
		if (!gov_data->depth) {
			mutex_lock_nested(&gov_data->this->lock, gov_data->depth);
			update_parent(gov_data);
			mutex_unlock(&gov_data->this->lock);
		}
		break;

	case DEVFREQ_GOV_RESUME:
		devfreq_monitor_resume(devfreq);
		break;

	default:
		break;
	}

	return ret;
}

struct devfreq_governor agov_passive = {
	.name = APU_GOV_PASSIVE,
	.get_target_freq = agov_get_target_freq,
	.event_handler = agov_event_handler,
};

