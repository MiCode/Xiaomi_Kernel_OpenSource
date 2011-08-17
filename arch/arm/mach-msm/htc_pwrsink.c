/* arch/arm/mach-msm/htc_pwrsink.c
 *
 * Copyright (C) 2008 HTC Corporation
 * Copyright (C) 2008 Google, Inc.
 * Author: San Mehat <san@google.com>
 *         Kant Kang <kant_kang@htc.com>
 *         Eiven Peng <eiven_peng@htc.com>
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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/earlysuspend.h>
#include <mach/msm_smd.h>
#include <mach/htc_pwrsink.h>

#include "smd_private.h"

enum {
	PWRSINK_DEBUG_CURR_CHANGE = 1U << 0,
	PWRSINK_DEBUG_CURR_CHANGE_AUDIO = 1U << 1,
};
static int pwrsink_debug_mask;
module_param_named(debug_mask, pwrsink_debug_mask, int,
		S_IRUGO | S_IWUSR | S_IWGRP);

static int initialized;
static unsigned audio_path = 1;	/* HTC_SND_DEVICE_SPEAKER = 1 */
static struct pwr_sink_audio audio_sink_array[PWRSINK_AUDIO_LAST + 1];
static struct pwr_sink *sink_array[PWRSINK_LAST + 1];
static DEFINE_SPINLOCK(sink_lock);
static DEFINE_SPINLOCK(audio_sink_lock);
static unsigned long total_sink;
static uint32_t *smem_total_sink;

int htc_pwrsink_set(pwrsink_id_type id, unsigned percent_utilized)
{
	unsigned long flags;

	if (!smem_total_sink)
		smem_total_sink = smem_alloc(SMEM_ID_VENDOR0, sizeof(uint32_t));

	if (!initialized)
		return -EAGAIN;

	if (id < 0 || id > PWRSINK_LAST)
		return -EINVAL;

	spin_lock_irqsave(&sink_lock, flags);

	if (!sink_array[id]) {
		spin_unlock_irqrestore(&sink_lock, flags);
		return -ENOENT;
	}

	if (sink_array[id]->percent_util == percent_utilized) {
		spin_unlock_irqrestore(&sink_lock, flags);
		return 0;
	}

	total_sink -= (sink_array[id]->ua_max *
		       sink_array[id]->percent_util / 100);
	sink_array[id]->percent_util = percent_utilized;
	total_sink += (sink_array[id]->ua_max *
		       sink_array[id]->percent_util / 100);

	if (smem_total_sink)
		*smem_total_sink = total_sink / 1000;

	pr_debug("htc_pwrsink: ID %d, Util %d%%, Total %lu uA %s\n",
		 id, percent_utilized, total_sink,
		 smem_total_sink ? "SET" : "");

	spin_unlock_irqrestore(&sink_lock, flags);

	return 0;
}
EXPORT_SYMBOL(htc_pwrsink_set);

static void compute_audio_current(void)
{
	/* unsigned long flags; */
	unsigned max_percent = 0;
	int i, active_audio_sinks = 0;
	pwrsink_audio_id_type last_active_audio_sink = 0;

	/* Make sure this segment will be spinlocked
	before computing by calling function. */
	/* spin_lock_irqsave(&audio_sink_lock, flags); */
	for (i = 0; i <= PWRSINK_AUDIO_LAST; ++i) {
		max_percent = (audio_sink_array[i].percent > max_percent) ?
				audio_sink_array[i].percent : max_percent;
		if (audio_sink_array[i].percent > 0) {
			active_audio_sinks++;
			last_active_audio_sink = i;
		}
	}
	if (active_audio_sinks == 0)
		htc_pwrsink_set(PWRSINK_AUDIO, 0);
	else if (active_audio_sinks == 1) {
		pwrsink_audio_id_type laas =  last_active_audio_sink;
		/* TODO: add volume and routing path current. */
		if (audio_path == 1)	/* Speaker */
			htc_pwrsink_set(PWRSINK_AUDIO,
				audio_sink_array[laas].percent);
		else
			htc_pwrsink_set(PWRSINK_AUDIO,
				audio_sink_array[laas].percent * 9 / 10);
	} else if (active_audio_sinks > 1) {
		/* TODO: add volume and routing path current. */
		if (audio_path == 1)	/* Speaker */
			htc_pwrsink_set(PWRSINK_AUDIO, max_percent);
		else
			htc_pwrsink_set(PWRSINK_AUDIO, max_percent * 9 / 10);
	}
	/* spin_unlock_irqrestore(&audio_sink_lock, flags); */

	if (pwrsink_debug_mask & PWRSINK_DEBUG_CURR_CHANGE_AUDIO)
		pr_info("%s: active_audio_sinks=%d, audio_path=%d\n", __func__,
				active_audio_sinks, audio_path);
}

int htc_pwrsink_audio_set(pwrsink_audio_id_type id, unsigned percent_utilized)
{
	unsigned long flags;

	if (id < 0 || id > PWRSINK_AUDIO_LAST)
		return -EINVAL;

	if (pwrsink_debug_mask & PWRSINK_DEBUG_CURR_CHANGE_AUDIO)
		pr_info("%s: id=%d, percent=%d, percent_old=%d\n", __func__,
			id, percent_utilized, audio_sink_array[id].percent);

	spin_lock_irqsave(&audio_sink_lock, flags);
	if (audio_sink_array[id].percent == percent_utilized) {
		spin_unlock_irqrestore(&audio_sink_lock, flags);
		return 0;
	}
	audio_sink_array[id].percent = percent_utilized;
	spin_unlock_irqrestore(&audio_sink_lock, flags);
	compute_audio_current();
	return 0;
}
EXPORT_SYMBOL(htc_pwrsink_audio_set);

int htc_pwrsink_audio_volume_set(pwrsink_audio_id_type id, unsigned volume)
{
	unsigned long flags;

	if (id < 0 || id > PWRSINK_AUDIO_LAST)
		return -EINVAL;

	if (pwrsink_debug_mask & PWRSINK_DEBUG_CURR_CHANGE_AUDIO)
		pr_info("%s: id=%d, volume=%d, volume_old=%d\n", __func__,
			id, volume, audio_sink_array[id].volume);

	spin_lock_irqsave(&audio_sink_lock, flags);
	if (audio_sink_array[id].volume == volume) {
		spin_unlock_irqrestore(&audio_sink_lock, flags);
		return 0;
	}
	audio_sink_array[id].volume = volume;
	spin_unlock_irqrestore(&audio_sink_lock, flags);
	compute_audio_current();
	return 0;
}
EXPORT_SYMBOL(htc_pwrsink_audio_volume_set);

int htc_pwrsink_audio_path_set(unsigned path)
{
	unsigned long flags;

	if (pwrsink_debug_mask & PWRSINK_DEBUG_CURR_CHANGE_AUDIO)
		pr_info("%s: path=%d, path_old=%d\n",
			__func__, path, audio_path);

	spin_lock_irqsave(&audio_sink_lock, flags);
	if (audio_path == path) {
		spin_unlock_irqrestore(&audio_sink_lock, flags);
		return 0;
	}
	audio_path = path;
	spin_unlock_irqrestore(&audio_sink_lock, flags);
	compute_audio_current();
	return 0;
}
EXPORT_SYMBOL(htc_pwrsink_audio_path_set);

void htc_pwrsink_suspend_early(struct early_suspend *h)
{
	htc_pwrsink_set(PWRSINK_SYSTEM_LOAD, 70);
}

int htc_pwrsink_suspend_late(struct platform_device *pdev, pm_message_t state)
{
	struct pwr_sink_platform_data *pdata = pdev->dev.platform_data;

	if (pdata && pdata->suspend_late)
		pdata->suspend_late(pdev, state);
	else
		htc_pwrsink_set(PWRSINK_SYSTEM_LOAD, 13);
	return 0;
}

int htc_pwrsink_resume_early(struct platform_device *pdev)
{
	struct pwr_sink_platform_data *pdata = pdev->dev.platform_data;

	if (pdata && pdata->resume_early)
		pdata->resume_early(pdev);
	else
		htc_pwrsink_set(PWRSINK_SYSTEM_LOAD, 70);
	return 0;
}

void htc_pwrsink_resume_late(struct early_suspend *h)
{
	htc_pwrsink_set(PWRSINK_SYSTEM_LOAD, 100);
}

struct early_suspend htc_pwrsink_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1,
	.suspend = htc_pwrsink_suspend_early,
	.resume = htc_pwrsink_resume_late,
};

static int __init htc_pwrsink_probe(struct platform_device *pdev)
{
	struct pwr_sink_platform_data *pdata = pdev->dev.platform_data;
	int i;

	if (!pdata)
		return -EINVAL;

	total_sink = 0;
	for (i = 0; i < pdata->num_sinks; i++) {
		sink_array[pdata->sinks[i].id] = &pdata->sinks[i];
		total_sink += (pdata->sinks[i].ua_max *
			       pdata->sinks[i].percent_util / 100);
	}

	initialized = 1;

	if (pdata->suspend_early)
		htc_pwrsink_early_suspend.suspend = pdata->suspend_early;
	if (pdata->resume_late)
		htc_pwrsink_early_suspend.resume = pdata->resume_late;
	register_early_suspend(&htc_pwrsink_early_suspend);

	return 0;
}

static struct platform_driver htc_pwrsink_driver = {
	.probe = htc_pwrsink_probe,
	.suspend_late = htc_pwrsink_suspend_late,
	.resume_early = htc_pwrsink_resume_early,
	.driver = {
		.name = "htc_pwrsink",
		.owner = THIS_MODULE,
	},
};

static int __init htc_pwrsink_init(void)
{
	initialized = 0;
	memset(sink_array, 0, sizeof(sink_array));
	return platform_driver_register(&htc_pwrsink_driver);
}

module_init(htc_pwrsink_init);
