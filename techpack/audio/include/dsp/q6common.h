/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __Q6COMMON_H__
#define __Q6COMMON_H__

#include <dsp/apr_audio-v2.h>

void q6common_update_instance_id_support(bool supported);
bool q6common_is_instance_id_supported(void);
int q6common_pack_pp_params(u8 *dest, struct param_hdr_v3 *v3_hdr,
			    u8 *param_data, u32 *total_size);
int q6common_pack_pp_params_v2(u8 *dest, struct param_hdr_v3 *v3_hdr,
			    u8 *param_data, u32 *total_size,
			    bool iid_supported);
#endif /* __Q6COMMON_H__ */
