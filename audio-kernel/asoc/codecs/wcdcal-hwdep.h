/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#ifndef __WCD9XXX_HWDEP_H__
#define __WCD9XXX_HWDEP_H__
#include <sound/msmcal-hwdep.h>

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
int wcd_cal_create_hwdep(void *fw, int node, struct snd_soc_codec *codec);
struct firmware_cal *wcdcal_get_fw_cal(struct fw_info *fw_data,
					enum wcd_cal_type type);
#endif /* __WCD9XXX_HWDEP_H__ */
