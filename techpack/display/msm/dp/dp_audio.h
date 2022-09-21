/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
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
 * @tui_active: set to true if TUI is active in the system
 */
struct dp_audio {
	u32 lane_count;
	u32 bw_code;
	bool tui_active;

	/**
	 * on()
	 *
	 * Notifies user mode clients that DP is powered on, and that audio
	 * playback can start on the external display.
	 *
	 * @dp_audio: an instance of struct dp_audio.
	 *
	 * Returns the error code in case of failure, 0 in success case.
	 */
	int (*on)(struct dp_audio *dp_audio);

	/**
	 * off()
	 *
	 * Notifies user mode clients that DP is shutting down, and audio
	 * playback should be stopped on the external display.
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
