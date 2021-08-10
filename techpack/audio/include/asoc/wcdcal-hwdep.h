/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014, 2018, 2020 The Linux Foundation. All rights reserved.
 */
#ifndef __WCD9XXX_HWDEP_H__
#define __WCD9XXX_HWDEP_H__
#include <audio/sound/msmcal-hwdep.h>

enum wcd_cal_states {
	WCDCAL_UNINITIALISED,
	WCDCAL_INITIALISED,
	WCDCAL_RECIEVED
};

struct fw_info {
	struct firmware_cal *fw[WCD9XXX_MAX_CAL];
	DECLARE_BITMAP(cal_bit, WCD9XXX_MAX_CAL);
	/* for calibration tracking */
	unsigned long wcdcal_state[WCD9XXX_MAX_CAL];
	struct mutex lock;
};

struct firmware_cal {
	u8 *data;
	size_t size;
};

struct snd_soc_codec;
int wcd_cal_create_hwdep(void *fw, int node,
			 struct snd_soc_component *component);
struct firmware_cal *wcdcal_get_fw_cal(struct fw_info *fw_data,
					enum wcd_cal_type type);
#endif /* __WCD9XXX_HWDEP_H__ */
