/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2014, 2017-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_DTS_SRS_TM_CONFIG_H_
#define _MSM_DTS_SRS_TM_CONFIG_H_

#include <sound/soc.h>
#include <dsp/apr_audio-v2.h>

struct param_outband;

#ifdef CONFIG_DTS_SRS_TM

union srs_trumedia_params_u {
	struct srs_trumedia_params srs_params;
	__u16 raw_params[1];
};

void msm_dts_srs_tm_ion_memmap(struct param_outband *po_);
void msm_dts_srs_tm_init(int port_id, int copp_idx);
void msm_dts_srs_tm_deinit(int port_id);
void msm_dts_srs_tm_add_controls(struct snd_soc_component *component);
#else
static inline void msm_dts_srs_tm_ion_memmap(struct param_outband *po_) { }
static inline void msm_dts_srs_tm_init(int port_id, int copp_idx) { }
static inline void msm_dts_srs_tm_deinit(int port_id) { }
static inline void msm_dts_srs_tm_add_controls(
					struct snd_soc_component *component) { }

#endif

#endif
