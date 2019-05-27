/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MSM_QTI_PP_H_
#define _MSM_QTI_PP_H_

#include <sound/soc.h>
#define DSP_BIT_WIDTH_MIXER_CTL "ASM Bit Width"
#if IS_ENABLED(CONFIG_QTI_PP)
int msm_adsp_inform_mixer_ctl(struct snd_soc_pcm_runtime *rtd,
			uint32_t *payload);
int msm_adsp_init_mixer_ctl_pp_event_queue(struct snd_soc_pcm_runtime *rtd);
int msm_adsp_clean_mixer_ctl_pp_event_queue(struct snd_soc_pcm_runtime *rtd);
int msm_adsp_stream_cmd_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo);
int msm_adsp_stream_callback_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int msm_adsp_stream_callback_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo);
void msm_qti_pp_send_eq_values(int fedai_id);
int msm_qti_pp_send_stereo_to_custom_stereo_cmd(int port_id, int copp_idx,
						unsigned int session_id,
						uint16_t op_FL_ip_FL_weight,
						uint16_t op_FL_ip_FR_weight,
						uint16_t op_FR_ip_FL_weight,
						uint16_t op_FR_ip_FR_weight);
void msm_qti_pp_add_controls(struct snd_soc_platform *platform);
#else /* CONFIG_QTI_PP */
static inline int msm_adsp_inform_mixer_ctl(struct snd_soc_pcm_runtime *rtd,
			uint32_t *payload)
{
	return 0;
}

static inline int msm_adsp_init_mixer_ctl_pp_event_queue(
			struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static inline int msm_adsp_clean_mixer_ctl_pp_event_queue(
			struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static inline int msm_adsp_stream_cmd_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	return 0;
}

static inline int msm_adsp_stream_callback_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static inline int msm_adsp_stream_callback_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	return 0;
}

#define msm_qti_pp_send_eq_values(fedai_id) do {} while (0)
#define msm_qti_pp_send_stereo_to_custom_stereo_cmd(port_id, copp_idx, \
			session_id, op_FL_ip_FL_weight, op_FL_ip_FR_weight, \
			op_FR_ip_FL_weight, op_FR_ip_FR_weight) (0)
#define msm_qti_pp_add_controls(platform) do {} while (0)
#endif /* CONFIG_QTI_PP */

#if IS_ENABLED(CONFIG_QTI_PP) && IS_ENABLED(CONFIG_QTI_PP_AUDIOSPHERE)
int msm_qti_pp_asphere_init(int port_id, int copp_idx);
void msm_qti_pp_asphere_deinit(int port_id);
#else
#define msm_qti_pp_asphere_init(port_id, copp_idx) (0)
#define msm_qti_pp_asphere_deinit(port_id) do {} while (0)
#endif

#endif /* _MSM_QTI_PP_H_ */
