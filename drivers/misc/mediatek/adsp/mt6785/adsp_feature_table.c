/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>       /* needed by all modules */
#include "adsp_feature_define.h"
#include "adsp_dvfs.h"

#define ADSP_SYSTEM_UNIT(fname) {.name = fname, .freq = 0, .counter = 1}
#define ADSP_FEATURE_UNIT(fname) {.name = fname, .freq = 0, .counter = 0}

DEFINE_MUTEX(adsp_feature_mutex);

/*adsp feature list*/
struct adsp_feature_tb adsp_feature_table[ADSP_NUM_FEATURE_ID] = {
	[SYSTEM_FEATURE_ID]           = ADSP_FEATURE_UNIT("system"),
	[ADSP_LOGGER_FEATURE_ID]      = ADSP_FEATURE_UNIT("logger"),
	[AURISYS_FEATURE_ID]          = ADSP_FEATURE_UNIT("aurisys"),
	[AUDIO_CONTROLLER_FEATURE_ID] = ADSP_FEATURE_UNIT("audio_controller"),
	[PRIMARY_FEATURE_ID]          = ADSP_FEATURE_UNIT("primary"),
	[DEEPBUF_FEATURE_ID]          = ADSP_FEATURE_UNIT("deepbuf"),
	[OFFLOAD_FEATURE_ID]          = ADSP_FEATURE_UNIT("offload"),
	[AUDIO_PLAYBACK_FEATURE_ID]   = ADSP_FEATURE_UNIT("audplayback"),
	[A2DP_PLAYBACK_FEATURE_ID]    = ADSP_FEATURE_UNIT("a2dp_playback"),
	[AUDIO_DATAPROVIDER_FEATURE_ID] = ADSP_FEATURE_UNIT("dataprovider"),
	[SPK_PROTECT_FEATURE_ID]      = ADSP_FEATURE_UNIT("spk_protect"),
	[VOICE_CALL_FEATURE_ID]       = ADSP_FEATURE_UNIT("voice_call"),
	[VOIP_FEATURE_ID]             = ADSP_FEATURE_UNIT("voip"),
	[CAPTURE_UL1_FEATURE_ID]      = ADSP_FEATURE_UNIT("capture_ul1"),
	[CALL_FINAL_FEATURE_ID]       = ADSP_FEATURE_UNIT("call_final"),
};

ssize_t adsp_dump_feature_state(char *buffer, int size)
{
	int n = 0, i = 0;
	struct adsp_feature_tb *unit;

	n += scnprintf(buffer + n, size - n, "%-20s %-8s %-8s\n",
		       "Feature_name", "Freq", "Counter");
	for (i = 0; i < ADSP_NUM_FEATURE_ID; i++) {
		unit = &adsp_feature_table[i];
		if (!unit->name)
			continue;
		n += scnprintf(buffer + n, size - n, "%-20s %-8d %-3d\n",
			unit->name, unit->freq, unit->counter);
	}
	return n;
}

int adsp_get_feature_index(char *str)
{
	int i = 0;
	struct adsp_feature_tb *unit;

	if (!str)
		return -EINVAL;

	for (i = 0; i < ADSP_NUM_FEATURE_ID; i++) {
		unit = &adsp_feature_table[i];
		if (!unit->name)
			continue;
		if (strncmp(unit->name, str, strlen(unit->name)) == 0)
			break;
	}

	return i == ADSP_NUM_FEATURE_ID ? -EINVAL : i;
}

bool adsp_feature_is_active(void)
{
	uint32_t fid;

	/* not include system feature */
	for (fid = 0; fid < ADSP_NUM_FEATURE_ID ; fid++) {
		if (adsp_feature_table[fid].counter > 0)
			break;
	}
	return fid == ADSP_NUM_FEATURE_ID ? false : true;
}

int adsp_register_feature(enum adsp_feature_id id)
{
	int ret = 0;

	if (id >= ADSP_NUM_FEATURE_ID)
		return -EINVAL;

	if (!adsp_feature_table[id].name)
		return -EINVAL;

	mutex_lock(&adsp_feature_mutex);
	if (!adsp_feature_is_active()) {
		pr_debug("[%s]%s, adsp_ready=%x\n", __func__,
			 adsp_feature_table[id].name, is_adsp_ready(ADSP_A_ID));
		adsp_stop_suspend_timer();
		ret = adsp_resume();
	}
	if (ret == 0)
		adsp_feature_table[id].counter += 1;
	mutex_unlock(&adsp_feature_mutex);
	return ret;
}

int adsp_deregister_feature(enum adsp_feature_id id)
{
	if (id >= ADSP_NUM_FEATURE_ID)
		return -EINVAL;

	if (!adsp_feature_table[id].name)
		return -EINVAL;

	mutex_lock(&adsp_feature_mutex);
	if (adsp_feature_table[id].counter == 0) {
		pr_err("[%s] error to deregister id=%d\n", __func__, id);
		mutex_unlock(&adsp_feature_mutex);
		return -EINVAL;
	}
	adsp_feature_table[id].counter -= 1;

	/* no feature registered, delay 1s and then suspend adsp. */
	if (!adsp_feature_is_active() && (is_adsp_ready(ADSP_A_ID) == 1)) {
		pr_debug("[%s]%s, adsp_ready=%x\n", __func__,
			 adsp_feature_table[id].name, is_adsp_ready(ADSP_A_ID));
		adsp_start_suspend_timer();
	}
	mutex_unlock(&adsp_feature_mutex);

	return 0;
}

