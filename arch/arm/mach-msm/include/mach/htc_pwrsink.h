/* include/asm/mach-msm/htc_pwrsink.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (C) 2008 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _ARCH_ARM_MACH_MSM_HTC_PWRSINK_H_
#define _ARCH_ARM_MACH_MSM_HTC_PWRSINK_H_

#include <linux/platform_device.h>
#include <linux/earlysuspend.h>

typedef enum {
	PWRSINK_AUDIO_PCM = 0,
	PWRSINK_AUDIO_MP3,
	PWRSINK_AUDIO_AAC,

	PWRSINK_AUDIO_LAST = PWRSINK_AUDIO_AAC,
	PWRSINK_AUDIO_INVALID
} pwrsink_audio_id_type;

struct pwr_sink_audio {
	unsigned volume;
	unsigned percent;
};

typedef enum {
	PWRSINK_SYSTEM_LOAD = 0,
	PWRSINK_AUDIO,
	PWRSINK_BACKLIGHT,
	PWRSINK_LED_BUTTON,
	PWRSINK_LED_KEYBOARD,
	PWRSINK_GP_CLK,
	PWRSINK_BLUETOOTH,
	PWRSINK_CAMERA,
	PWRSINK_SDCARD,
	PWRSINK_VIDEO,
	PWRSINK_WIFI,

	PWRSINK_LAST = PWRSINK_WIFI,
	PWRSINK_INVALID
} pwrsink_id_type;

struct pwr_sink {
	pwrsink_id_type	id;
	unsigned	ua_max;
	unsigned	percent_util;
};

struct pwr_sink_platform_data {
	unsigned	num_sinks;
	struct pwr_sink	*sinks;
	int (*suspend_late)(struct platform_device *, pm_message_t state);
	int (*resume_early)(struct platform_device *);
	void (*suspend_early)(struct early_suspend *);
	void (*resume_late)(struct early_suspend *);
};

#ifndef CONFIG_HTC_PWRSINK
static inline int htc_pwrsink_set(pwrsink_id_type id, unsigned percent)
{
	return 0;
}
static inline int htc_pwrsink_audio_set(pwrsink_audio_id_type id,
	unsigned percent_utilized) { return 0; }
static inline int htc_pwrsink_audio_volume_set(
	pwrsink_audio_id_type id, unsigned volume) { return 0; }
static inline int htc_pwrsink_audio_path_set(unsigned path) { return 0; }
#else
extern int htc_pwrsink_set(pwrsink_id_type id, unsigned percent);
extern int htc_pwrsink_audio_set(pwrsink_audio_id_type id,
	unsigned percent_utilized);
extern int htc_pwrsink_audio_volume_set(pwrsink_audio_id_type id,
	unsigned volume);
extern int htc_pwrsink_audio_path_set(unsigned path);
#endif

#endif
