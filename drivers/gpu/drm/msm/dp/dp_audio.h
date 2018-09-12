/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _DP_AUDIO_H_
#define _DP_AUDIO_H_

#include <linux/platform_device.h>

#include "dp_panel.h"
#include "dp_catalog.h"

/**
 * struct dp_audio
 * @lane_count: number of lanes configured in current session
 * @bw_code: link rate's bandwidth code for current session
 */
struct dp_audio {
	u32 lane_count;
	u32 bw_code;

	/**
	 * on()
	 *
	 * Enables the audio by notifying the user module.
	 *
	 * @dp_audio: an instance of struct dp_audio.
	 *
	 * Returns the error code in case of failure, 0 in success case.
	 */
	int (*on)(struct dp_audio *dp_audio);

	/**
	 * off()
	 *
	 * Disables the audio by notifying the user module.
	 *
	 * @dp_audio: an instance of struct dp_audio.
	 *
	 * Returns the error code in case of failure, 0 in success case.
	 */
	int (*off)(struct dp_audio *dp_audio);
};

/**
 * dp_audio_get()
 *
 * Creates and instance of dp audio.
 *
 * @pdev: caller's platform device instance.
 * @panel: an instance of dp_panel module.
 * @catalog: an instance of dp_catalog_audio module.
 *
 * Returns the error code in case of failure, otherwize
 * an instance of newly created dp_module.
 */
struct dp_audio *dp_audio_get(struct platform_device *pdev,
			struct dp_panel *panel,
			struct dp_catalog_audio *catalog);

/**
 * dp_audio_put()
 *
 * Cleans the dp_audio instance.
 *
 * @dp_audio: an instance of dp_audio.
 */
void dp_audio_put(struct dp_audio *dp_audio);
#endif /* _DP_AUDIO_H_ */


