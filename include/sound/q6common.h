/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef __Q6COMMON_H__
#define __Q6COMMON_H__

#include <sound/apr_audio-v2.h>

void q6common_update_instance_id_support(bool supported);
bool q6common_is_instance_id_supported(void);
int q6common_pack_pp_params(u8 *dest, struct param_hdr_v3 *v3_hdr,
			    u8 *param_data, u32 *total_size);

#endif /* __Q6COMMON_H__ */
