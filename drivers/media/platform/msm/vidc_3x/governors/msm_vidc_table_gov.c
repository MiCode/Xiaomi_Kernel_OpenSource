// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2016, 2018 The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include "governor.h"
#include "../msm_vidc_debug.h"
#include "../msm_vidc_res_parse.h"
#include "../msm_vidc_internal.h"
#include "../venus_hfi.h"
#include "../vidc_hfi_api.h"


static int __get_bus_freq(struct msm_vidc_bus_table_gov *gov,
		struct vidc_bus_vote_data *data,
		enum bus_profile profile)
{
	int i = 0, load = 0, freq = 0;
	enum vidc_vote_data_session sess_type = 0;
	struct bus_profile_entry *entry = NULL;
	bool found = false;

	load = NUM_MBS_PER_SEC(data->width, data->height, data->fps);
	sess_type = VIDC_VOTE_DATA_SESSION_VAL(data->codec, data->domain);

	/* check if ubwc bus profile is present */
	for (i = 0; i < gov->count; i++) {
		entry = &gov->bus_prof_entries[i];
		if (!entry->bus_table || !entry->bus_table_size)
			continue;
		if (!venus_hfi_is_session_supported(
				entry->codec_mask, sess_type))
			continue;
		if (entry->profile == profile) {
			found = true;
			break;
		}
	}

	if (found) {
		 /* loop over bus table and select frequency */
		for (i = entry->bus_table_size - 1; i >= 0; --i) {
			 /*load is arranged in descending order */
			freq = entry->bus_table[i].freq;
			if (load <= entry->bus_table[i].load)
				break;
		}
	}

	return freq;
}


int msm_vidc_table_get_target_freq(struct msm_vidc_bus_table_gov *gov,
				struct msm_vidc_gov_data *vidc_data,
				unsigned long *frequency)
{
	enum bus_profile profile = 0;
	int i = 0;

	if (!frequency || !gov || !vidc_data)  {
		dprintk(VIDC_ERR, "%s: Invalid params %pK\n",
			__func__, frequency);
		return -EINVAL;
	}

	*frequency = 0;
	for (i = 0; i < vidc_data->data_count; i++) {
		struct vidc_bus_vote_data *data = &vidc_data->data[i];
		int freq = 0;

		if (data->power_mode == VIDC_POWER_TURBO) {
			dprintk(VIDC_DBG, "bus: found turbo session[%d] %#x\n",
				i, VIDC_VOTE_DATA_SESSION_VAL(data->codec,
					data->domain));
			*frequency = INT_MAX;
			goto exit;
		}

		profile = VIDC_BUS_PROFILE_NORMAL;
		if (data->color_formats[0] == HAL_COLOR_FORMAT_NV12_TP10_UBWC ||
			data->color_formats[0] == HAL_COLOR_FORMAT_NV12_UBWC)
			profile = VIDC_BUS_PROFILE_UBWC;

		freq = __get_bus_freq(gov, data, profile);

		/* chose frequency from normal profile
		 * if specific profile frequency was not found.
		 */
		if (!freq)
			freq = __get_bus_freq(gov, data,
				VIDC_BUS_PROFILE_NORMAL);

		*frequency += (unsigned long)freq;

		dprintk(VIDC_DBG,
			"session[%d] %#x: wxh %dx%d, fps %d, bus_profile %#x, freq %d, total_freq %ld KBps\n",
			i, VIDC_VOTE_DATA_SESSION_VAL(
			data->codec, data->domain), data->width,
			data->height, data->fps, profile,
			freq, *frequency);
	}
exit:
	return 0;
}
