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
#include "apu_trace.h"

static int ausr_get_target_freq(struct devfreq *df, unsigned long *freq)
{
	struct apu_gov_data *gov_data = (struct apu_gov_data *)df->data;
	struct apu_dev *ad = NULL;
	struct apu_req *req = NULL;

	get_datas(gov_data, NULL, &ad, NULL);
	req = list_first_entry(&gov_data->head, struct apu_req, list);
	*freq = apu_opp2freq(ad, req->value);
	if (!round_khz(*freq, df->previous_freq)) {
		apu_dump_list(gov_data);
		apupw_dbg_dvfs_tag_update(APUGOV_USR, apu_dev_name(ad->dev),
			apu_dev_name(req->dev), (u32)req->value, TOMHZ(*freq));
		advfs_info(ad->dev, "[%s] %s vote opp/freq %d/%u\n", __func__,
			   apu_dev_name(req->dev), req->value, TOMHZ(*freq));
	}

	return 0;
}

static int ausr_event_handler(struct devfreq *df,
			unsigned int event, void *data)
{
	int ret = 0;
	struct apu_gov_data *gov_data = (struct apu_gov_data *)df->data;

	switch (event) {
	case DEVFREQ_GOV_START:
		if (!gov_data->this)
			gov_data->this = df;
		break;
	case DEVFREQ_GOV_STOP:
	case DEVFREQ_GOV_UPDATE_INTERVAL:
	case DEVFREQ_GOV_RESUME:
	case DEVFREQ_GOV_SUSPEND:
	default:
		break;
	}

	return ret;
}

struct devfreq_governor agov_userspace = {
	.name = APUGOV_USR,
	.get_target_freq = ausr_get_target_freq,
	.event_handler = ausr_event_handler,
};

