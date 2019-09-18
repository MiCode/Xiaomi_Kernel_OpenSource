/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#include <dsp/q6common.h>

struct q6common_ctl {
	bool instance_id_supported;
};

static struct q6common_ctl common;

/**
 * q6common_update_instance_id_support
 *
 * Update instance ID support flag to true/false
 *
 * @supported: enable/disable for instance ID support
 */
void q6common_update_instance_id_support(bool supported)
{
	common.instance_id_supported = supported;
}
EXPORT_SYMBOL(q6common_update_instance_id_support);

/**
 * q6common_is_instance_id_supported
 *
 * Returns true/false for instance ID support
 */
bool q6common_is_instance_id_supported(void)
{
	return common.instance_id_supported;
}
EXPORT_SYMBOL(q6common_is_instance_id_supported);

/**
 * q6common_pack_pp_params
 *
 * Populate params header based on instance ID support and pack
 * it with payload.
 * Instance ID support -
 *     yes - param_hdr_v3 + payload
 *     no  - param_hdr_v1 + payload
 *
 * @dest: destination data pointer to be packed into
 * @v3_hdr: param header v3
 * @param_data: param payload
 * @total_size: total size of packed data (hdr + payload)
 *
 * Returns 0 on success or error on failure
 */
int q6common_pack_pp_params(u8 *dest, struct param_hdr_v3 *v3_hdr,
			    u8 *param_data, u32 *total_size)
{
	struct param_hdr_v1 *v1_hdr = NULL;
	u32 packed_size = 0;
	u32 param_size = 0;
	bool iid_supported = q6common_is_instance_id_supported();

	if (dest == NULL) {
		pr_err("%s: Received NULL pointer for destination\n", __func__);
		return -EINVAL;
	} else if (v3_hdr == NULL) {
		pr_err("%s: Received NULL pointer for header\n", __func__);
		return -EINVAL;
	} else if (total_size == NULL) {
		pr_err("%s: Received NULL pointer for total size\n", __func__);
		return -EINVAL;
	}

	param_size = v3_hdr->param_size;
	packed_size = iid_supported ? sizeof(struct param_hdr_v3) :
				      sizeof(struct param_hdr_v1);

	if (iid_supported) {
		memcpy(dest, v3_hdr, packed_size);
	} else {
		v1_hdr = (struct param_hdr_v1 *) dest;
		v1_hdr->module_id = v3_hdr->module_id;
		v1_hdr->param_id = v3_hdr->param_id;

		if (param_size > U16_MAX) {
			pr_err("%s: Invalid param size for V1 %d\n", __func__,
			       param_size);
			return -EINVAL;
		}
		v1_hdr->param_size = param_size;
		v1_hdr->reserved = 0;
	}

	/*
	 * Make param_data optional for cases when there is no data
	 * present as in some set cases and all get cases.
	 */
	if (param_data != NULL) {
		memcpy(dest + packed_size, param_data, param_size);
		packed_size += param_size;
	}

	*total_size = packed_size;

	return 0;
}
EXPORT_SYMBOL(q6common_pack_pp_params);

/**
 * q6common_pack_pp_params_v2
 *
 * Populate params header based on instance ID support and pack
 * it with payload.
 * Instance ID support -
 *     yes - param_hdr_v3 + payload
 *     no  - param_hdr_v1 + payload
 *
 * @dest: destination data pointer to be packed into
 * @v3_hdr: param header v3
 * @param_data: param payload
 * @total_size: total size of packed data (hdr + payload)
 * @iid_supported: Instance ID supported or not
 *
 * Returns 0 on success or error on failure
 */
int q6common_pack_pp_params_v2(u8 *dest, struct param_hdr_v3 *v3_hdr,
			    u8 *param_data, u32 *total_size,
			    bool iid_supported)
{
	struct param_hdr_v1 *v1_hdr = NULL;
	u32 packed_size = 0;
	u32 param_size = 0;

	if (dest == NULL) {
		pr_err("%s: Received NULL pointer for destination\n", __func__);
		return -EINVAL;
	} else if (v3_hdr == NULL) {
		pr_err("%s: Received NULL pointer for header\n", __func__);
		return -EINVAL;
	} else if (total_size == NULL) {
		pr_err("%s: Received NULL pointer for total size\n", __func__);
		return -EINVAL;
	}

	param_size = v3_hdr->param_size;
	packed_size = iid_supported ? sizeof(struct param_hdr_v3) :
				      sizeof(struct param_hdr_v1);

	if (iid_supported) {
		memcpy(dest, v3_hdr, packed_size);
	} else {
		v1_hdr = (struct param_hdr_v1 *) dest;
		v1_hdr->module_id = v3_hdr->module_id;
		v1_hdr->param_id = v3_hdr->param_id;

		if (param_size > U16_MAX) {
			pr_err("%s: Invalid param size for V1 %d\n", __func__,
			       param_size);
			return -EINVAL;
		}
		v1_hdr->param_size = param_size;
		v1_hdr->reserved = 0;
	}

	/*
	 * Make param_data optional for cases when there is no data
	 * present as in some set cases and all get cases.
	 */
	if (param_data != NULL) {
		memcpy(dest + packed_size, param_data, param_size);
		packed_size += param_size;
	}

	*total_size = packed_size;

	return 0;
}
EXPORT_SYMBOL(q6common_pack_pp_params_v2);
