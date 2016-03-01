/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#ifdef CONFIG_QTI_PP

void msm_qti_pp_send_eq_values(int fedai_id);
int msm_qti_pp_send_stereo_to_custom_stereo_cmd(int port_id, int copp_idx,
						unsigned int session_id,
						uint16_t op_FL_ip_FL_weight,
						uint16_t op_FL_ip_FR_weight,
						uint16_t op_FR_ip_FL_weight,
						uint16_t op_FR_ip_FR_weight);
void msm_qti_pp_add_controls(struct snd_soc_platform *platform);

int msm_qti_pp_asphere_init(int port_id, int copp_idx);
void msm_qti_pp_asphere_deinit(int port_id);

#else

void msm_qti_pp_send_eq_values(int fedai_id) { }
int msm_qti_pp_send_stereo_to_custom_stereo_cmd(int port_id, int copp_idx,
						unsigned int session_id,
						uint16_t op_FL_ip_FL_weight,
						uint16_t op_FL_ip_FR_weight,
						uint16_t op_FR_ip_FL_weight,
						uint16_t op_FR_ip_FR_weight)
{
	return 0;
}

void msm_qti_pp_add_controls(struct snd_soc_platform *platform) { }

int msm_qti_pp_asphere_init(int port_id, int copp_idx)
{
	return 0;
}
void msm_qti_pp_asphere_deinit(int port_id) { }

#endif

#endif

