/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_QTI_PP_H_
#define _MSM_QTI_PP_H_

#include <sound/soc.h>
#define DSP_BIT_WIDTH_MIXER_CTL "ASM Bit Width"
#ifdef CONFIG_QTI_PP
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
void msm_qti_pp_add_controls(struct snd_soc_component *component);
int msm_qti_pp_send_chmix_cfg_cmd(int port_id, int copp_idx,
				  unsigned int session_id, int ip_channel_count,
				  int out_channel_cnt, int *ch_wght_coeff,
				  int session_type, int stream_type);
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

int msm_qti_pp_send_chmix_cfg_cmd(int port_id, int copp_idx,
				  unsigned int session_id, int ip_channel_count,
				  int out_channel_cnt, int *ch_wght_coeff,
				  int session_type, int stream_type)
{
	return 0;
}
#define msm_qti_pp_send_eq_values(fedai_id) do {} while (0)
#define msm_qti_pp_send_stereo_to_custom_stereo_cmd(port_id, copp_idx, \
			session_id, op_FL_ip_FL_weight, op_FL_ip_FR_weight, \
			op_FR_ip_FL_weight, op_FR_ip_FR_weight) (0)
#define msm_qti_pp_add_controls(platform) do {} while (0)
#endif /* CONFIG_QTI_PP */


#if defined(CONFIG_QTI_PP) && defined(CONFIG_QTI_PP_AUDIOSPHERE)
int msm_qti_pp_asphere_init(int port_id, int copp_idx);
void msm_qti_pp_asphere_deinit(int port_id);
#else
#define msm_qti_pp_asphere_init(port_id, copp_idx) (0)
#define msm_qti_pp_asphere_deinit(port_id) do {} while (0)
#endif

#endif /* _MSM_QTI_PP_H_ */
