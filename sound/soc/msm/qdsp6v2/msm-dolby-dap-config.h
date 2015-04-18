/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MSM_DOLBY_DAP_CONFIG_H_
#define _MSM_DOLBY_DAP_CONFIG_H_

#include <sound/soc.h>
#include "msm-dolby-common.h"

#ifdef CONFIG_DOLBY_DAP
/* DOLBY DOLBY GUIDS */
#define DOLBY_ADM_COPP_TOPOLOGY_ID	0x0001033B
#define NUM_DOLBY_ENDP_DEVICE                 23

#define DOLBY_NUM_ENDP_DEPENDENT_PARAMS	  3
#define DOLBY_ENDDEP_PARAM_DVLO_OFFSET	  0
#define DOLBY_ENDDEP_PARAM_DVLO_LENGTH	  1
#define DOLBY_ENDDEP_PARAM_DVLI_OFFSET    (DOLBY_ENDDEP_PARAM_DVLO_OFFSET + \
						DOLBY_ENDDEP_PARAM_DVLO_LENGTH)
#define DOLBY_ENDDEP_PARAM_DVLI_LENGTH    1
#define DOLBY_ENDDEP_PARAM_VMB_OFFSET     (DOLBY_ENDDEP_PARAM_DVLI_OFFSET + \
						DOLBY_ENDDEP_PARAM_DVLI_LENGTH)
#define DOLBY_ENDDEP_PARAM_VMB_LENGTH     1
#define DOLBY_ENDDEP_PARAM_LENGTH         (DOLBY_ENDDEP_PARAM_DVLO_LENGTH + \
		DOLBY_ENDDEP_PARAM_DVLI_LENGTH + DOLBY_ENDDEP_PARAM_VMB_LENGTH)

#define MAX_DOLBY_PARAMS			47
#define MAX_DOLBY_CTRL_PARAMS			5
#define ALL_DOLBY_PARAMS			(MAX_DOLBY_PARAMS + \
							MAX_DOLBY_CTRL_PARAMS)
#define DOLBY_COMMIT_ALL_IDX			MAX_DOLBY_PARAMS
#define DOLBY_COMMIT_IDX			(MAX_DOLBY_PARAMS+1)
#define DOLBY_USE_CACHE_IDX			(MAX_DOLBY_PARAMS+2)
#define DOLBY_AUTO_ENDP_IDX			(MAX_DOLBY_PARAMS+3)
#define DOLBY_AUTO_ENDDEP_IDX			(MAX_DOLBY_PARAMS+4)

/* DOLBY device definitions */
enum {
	DOLBY_ENDP_INT_SPEAKERS = 0,
	DOLBY_ENDP_EXT_SPEAKERS,
	DOLBY_ENDP_HEADPHONES,
	DOLBY_ENDP_HDMI,
	DOLBY_ENDP_SPDIF,
	DOLBY_ENDP_DLNA,
	DOLBY_ENDP_ANALOG,
};

/* DOLBY device definitions end */

struct dolby_dap_params {
	uint32_t value[TOTAL_LENGTH_DOLBY_PARAM + MAX_DOLBY_PARAMS];
} __packed;

int msm_dolby_dap_init(int port_id, int copp_idx, int channels,
		       bool is_custom_stereo_on);
void msm_dolby_dap_deinit(int port_id);
void msm_dolby_dap_add_controls(struct snd_soc_platform *platform);
int dolby_dap_set_custom_stereo_onoff(int port_id, int copp_idx,
				      bool is_custom_stereo_enabled);
/* Dolby DOLBY end */
#else
int msm_dolby_dap_init(int port_id, int copp_idx, int channels,
		       bool is_custom_stereo_on)
{
	return 0;
}
void msm_dolby_dap_deinit(int port_id) { }
void msm_dolby_dap_add_controls(struct snd_soc_platform *platform) { }
int dolby_dap_set_custom_stereo_onoff(int port_id, int copp_idx,
				      bool is_custom_stereo_enabled)
{
	return 0;
}
#endif

#endif

