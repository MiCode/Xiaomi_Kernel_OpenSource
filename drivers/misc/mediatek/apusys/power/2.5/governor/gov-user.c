// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/devfreq.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/module.h>
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

static int ausr_get_target_freq(struct devfreq *df, unsigned long *freq)
{
	struct apu_gov_data *gov_data = (struct apu_gov_data *)df->data;
	struct device *dev = NULL;
	struct apu_dev *ad = NULL;

	get_datas(gov_data, NULL, &ad, &dev);
	if (gov_data->valid) {
		*freq = apu_opp2freq(ad, gov_data->n_opp);
		gov_data->valid = false;
		if (*freq != df->previous_freq) {
			advfs_info(dev, " voting %luMhz(opp%d)\n",
				TOMHZ(*freq), gov_data->n_opp);
			goto out;
		}
	}
	df->profile->get_cur_freq(dev, freq);
out:
	return 0;
}


static int ausr_event_handler(struct devfreq *devfreq,
			unsigned int event, void *data)
{
	int ret = 0;
	struct apu_gov_data *gov_data = (struct apu_gov_data *)devfreq->data;

	switch (event) {
	case DEVFREQ_GOV_START:
		if (!gov_data->this)
			gov_data->this = devfreq;
		break;
	default:
		break;
	}

	return ret;
}

struct devfreq_governor agov_userspace = {
	.name = APU_GOV_USERDEF,
	.get_target_freq = ausr_get_target_freq,
	.event_handler = ausr_event_handler,
};

