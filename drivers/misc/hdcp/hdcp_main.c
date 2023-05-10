// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "hdcp_main.h"
#include "hdcp_qseecom.h"
#include "hdcp_smcinvoke.h"

struct hdcp_ta_interface ta_interface;
static DEFINE_MUTEX(hdcp1_mutex_g);
static DEFINE_MUTEX(hdcp2_mutex_g);

void select_interface(bool use_smcinvoke)
{
	if (use_smcinvoke) {
		ta_interface.trusted_app_hdcp1_init = &hdcp1_init_smcinvoke;
		ta_interface.trusted_app_hdcp1_feature_supported =
								&hdcp1_feature_supported_smcinvoke;
		ta_interface.trusted_app_hdcp1_set_enc = &hdcp1_set_enc_smcinvoke;
		ta_interface.trusted_app_hdcp1_ops_notify = &hdcp1_ops_notify_smcinvoke;
		ta_interface.trusted_app_hdcp1_start = &hdcp1_start_smcinvoke;
		ta_interface.trusted_app_hdcp1_stop = &hdcp1_stop_smcinvoke;
		ta_interface.trusted_app_hdcp2_init = &hdcp2_init_smcinvoke;
		ta_interface.trusted_app_hdcp2_deinit = &hdcp2_deinit_smcinvoke;
		ta_interface.trusted_app_hdcp2_app_start = &hdcp2_app_start_smcinvoke;
		ta_interface.trusted_app_hdcp2_app_start_auth = &hdcp2_app_start_auth_smcinvoke;
		ta_interface.trusted_app_hdcp2_app_process_msg = &hdcp2_app_process_msg_smcinvoke;
		ta_interface.trusted_app_hdcp2_app_enable_encryption =
						&hdcp2_app_enable_encryption_smcinvoke;
		ta_interface.trusted_app_hdcp2_app_query_stream = &hdcp2_app_query_stream_smcinvoke;
		ta_interface.trusted_app_hdcp2_app_stop = &hdcp2_app_stop_smcinvoke;
		ta_interface.trusted_app_hdcp2_feature_supported =
						&hdcp2_feature_supported_smcinvoke;
		ta_interface.trusted_app_hdcp2_force_encryption = &hdcp2_force_encryption_smcinvoke;
		ta_interface.trusted_app_hdcp2_open_stream = &hdcp2_open_stream_smcinvoke;
		ta_interface.trusted_app_hdcp2_close_stream = &hdcp2_close_stream_smcinvoke;
		ta_interface.trusted_app_hdcp2_update_app_data = &hdcp2_update_app_data_smcinvoke;
	} else {
		ta_interface.trusted_app_hdcp1_init = &hdcp1_init_qseecom;
		ta_interface.trusted_app_hdcp1_feature_supported = &hdcp1_feature_supported_qseecom;
		ta_interface.trusted_app_hdcp1_set_enc = &hdcp1_set_enc_qseecom;
		ta_interface.trusted_app_hdcp1_ops_notify = &hdcp1_ops_notify_qseecom;
		ta_interface.trusted_app_hdcp1_start = &hdcp1_start_qseecom;
		ta_interface.trusted_app_hdcp1_stop = &hdcp1_stop_qseecom;
		ta_interface.trusted_app_hdcp2_init = &hdcp2_init_qseecom;
		ta_interface.trusted_app_hdcp2_deinit = &hdcp2_deinit_qseecom;
		ta_interface.trusted_app_hdcp2_app_start = &hdcp2_app_start_qseecom;
		ta_interface.trusted_app_hdcp2_app_start_auth = &hdcp2_app_start_auth_qseecom;
		ta_interface.trusted_app_hdcp2_app_process_msg = &hdcp2_app_process_msg_qseecom;
		ta_interface.trusted_app_hdcp2_app_enable_encryption =
						&hdcp2_app_enable_encryption_qseecom;
		ta_interface.trusted_app_hdcp2_app_query_stream = &hdcp2_app_query_stream_qseecom;
		ta_interface.trusted_app_hdcp2_app_stop = &hdcp2_app_stop_qseecom;
		ta_interface.trusted_app_hdcp2_feature_supported = &hdcp2_feature_supported_qseecom;
		ta_interface.trusted_app_hdcp2_force_encryption = &hdcp2_force_encryption_qseecom;
		ta_interface.trusted_app_hdcp2_open_stream = &hdcp2_open_stream_qseecom;
		ta_interface.trusted_app_hdcp2_close_stream = &hdcp2_close_stream_qseecom;
		ta_interface.trusted_app_hdcp2_update_app_data = &hdcp2_update_app_data_qseecom;
	}
}

int hdcp1_count_ones(u8 *array, u8 len)
{
	int i, j, count = 0;

	for (i = 0; i < len; i++)
		for (j = 0; j < 8; j++)
			count += (((array[i] >> j) & 0x1) ? 1 : 0);

	return count;
}

int hdcp1_validate_aksv(u32 aksv_msb, u32 aksv_lsb)
{
	int const number_of_ones = 20;
	u8 aksv[5] = {0};

	pr_debug("AKSV=%02x%08x\n", aksv_msb, aksv_lsb);

	aksv[0] = aksv_lsb & 0xFF;
	aksv[1] = (aksv_lsb >> 8) & 0xFF;
	aksv[2] = (aksv_lsb >> 16) & 0xFF;
	aksv[3] = (aksv_lsb >> 24) & 0xFF;
	aksv[4] = aksv_msb & 0xFF;

	/* check there are 20 ones in AKSV */
	if (hdcp1_count_ones(aksv, 5) != number_of_ones) {
		pr_err("AKSV bit count failed\n");
		return -EINVAL;
	}

	return 0;
}

bool hdcp2_feature_supported(void *data)
{
	int ret = 0;

	mutex_lock(&hdcp2_mutex_g);
	ret = ta_interface.trusted_app_hdcp2_feature_supported(data);
	mutex_unlock(&hdcp2_mutex_g);

	return ret;
}
EXPORT_SYMBOL(hdcp2_feature_supported);

int hdcp2_force_encryption(void *ctx, uint32_t enable)
{
	int ret = 0;

	mutex_lock(&hdcp2_mutex_g);
	ret = ta_interface.trusted_app_hdcp2_force_encryption(ctx, enable);
	mutex_unlock(&hdcp2_mutex_g);

	return ret;
}
EXPORT_SYMBOL(hdcp2_force_encryption);

int hdcp2_app_comm(void *ctx, enum hdcp2_app_cmd cmd,
				   struct hdcp2_app_data *app_data)
{
	int ret = 0;
	uint32_t req_len = 0;

	if (!ctx || !app_data) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	req_len = app_data->request.length;

	mutex_lock(&hdcp2_mutex_g);
	switch (cmd) {
	case HDCP2_CMD_START:
		ret = ta_interface.trusted_app_hdcp2_app_start(ctx, req_len);
		break;
	case HDCP2_CMD_START_AUTH:
		ret = ta_interface.trusted_app_hdcp2_app_start_auth(ctx, req_len);
		break;
	case HDCP2_CMD_PROCESS_MSG:
		ret = ta_interface.trusted_app_hdcp2_app_process_msg(ctx, req_len);
		break;
	case HDCP2_CMD_TIMEOUT:
		ret = ta_interface.trusted_app_hdcp2_app_timeout(ctx, req_len);
		break;
	case HDCP2_CMD_EN_ENCRYPTION:
		ret = ta_interface.trusted_app_hdcp2_app_enable_encryption(ctx, req_len);
		break;
	case HDCP2_CMD_QUERY_STREAM:
		ret = ta_interface.trusted_app_hdcp2_app_query_stream(ctx, req_len);
		break;
	case HDCP2_CMD_STOP:
		ret = ta_interface.trusted_app_hdcp2_app_stop(ctx);
	default:
		goto error;
	}

	if (ret)
		goto error;

	ret = ta_interface.trusted_app_hdcp2_update_app_data(ctx, app_data);

error:
	mutex_unlock(&hdcp2_mutex_g);
	return ret;
}
EXPORT_SYMBOL(hdcp2_app_comm);

int hdcp2_open_stream(void *ctx, uint8_t vc_payload_id, uint8_t stream_number,
		  uint32_t *stream_id)
{
	int ret = 0;

	mutex_lock(&hdcp2_mutex_g);
	ret = ta_interface.trusted_app_hdcp2_open_stream(ctx, vc_payload_id, stream_number,
		   stream_id);
	mutex_unlock(&hdcp2_mutex_g);

	return ret;
}
EXPORT_SYMBOL(hdcp2_open_stream);

int hdcp2_close_stream(void *ctx, uint32_t stream_id)
{
	int ret = 0;

	mutex_lock(&hdcp2_mutex_g);
	ret = ta_interface.trusted_app_hdcp2_close_stream(ctx, stream_id);
	mutex_unlock(&hdcp2_mutex_g);

	return ret;
}
EXPORT_SYMBOL(hdcp2_close_stream);

void *hdcp2_init(u32 device_type)
{
	void *data = NULL;

	mutex_lock(&hdcp2_mutex_g);
	data = ta_interface.trusted_app_hdcp2_init(device_type);
	mutex_unlock(&hdcp2_mutex_g);

	return data;
}
EXPORT_SYMBOL(hdcp2_init);

void hdcp2_deinit(void *ctx)
{
	ta_interface.trusted_app_hdcp2_deinit(ctx);
}
EXPORT_SYMBOL(hdcp2_deinit);

void *hdcp1_init(void)
{
	void *data = NULL;

	mutex_lock(&hdcp1_mutex_g);
	data = ta_interface.trusted_app_hdcp1_init();
	mutex_unlock(&hdcp1_mutex_g);

	return data;
}
EXPORT_SYMBOL(hdcp1_init);

void hdcp1_deinit(void *data)
{
	kfree(data);
}
EXPORT_SYMBOL(hdcp1_deinit);

bool hdcp1_feature_supported(void *data)
{
	bool supported = false;

	mutex_lock(&hdcp1_mutex_g);
	supported = ta_interface.trusted_app_hdcp1_feature_supported(data);
	mutex_unlock(&hdcp1_mutex_g);

	return supported;
}
EXPORT_SYMBOL(hdcp1_feature_supported);

int hdcp1_set_enc(void *data, bool enable)
{
	int ret = 0;

	mutex_lock(&hdcp1_mutex_g);
	ret = ta_interface.trusted_app_hdcp1_set_enc(data, enable);
	mutex_unlock(&hdcp1_mutex_g);

	return ret;
}
EXPORT_SYMBOL(hdcp1_set_enc);

int hdcp1_ops_notify(void *data, void *topo, bool is_authenticated)
{
	int ret = 0;

	ret = ta_interface.trusted_app_hdcp1_ops_notify(data, topo, is_authenticated);

	return ret;
}
EXPORT_SYMBOL(hdcp1_ops_notify);

int hdcp1_start(void *data, u32 *aksv_msb, u32 *aksv_lsb)
{
	int ret = 0;

	mutex_lock(&hdcp1_mutex_g);
	ret = ta_interface.trusted_app_hdcp1_start(data, aksv_msb, aksv_lsb);
	mutex_unlock(&hdcp1_mutex_g);

	return ret;
}
EXPORT_SYMBOL(hdcp1_start);

void hdcp1_stop(void *data)
{
	mutex_lock(&hdcp1_mutex_g);
	ta_interface.trusted_app_hdcp1_stop(data);
	mutex_unlock(&hdcp1_mutex_g);
}
EXPORT_SYMBOL(hdcp1_stop);

static int __init hdcp_module_init(void)
{
	struct device_node *np = NULL;
	bool use_smcinvoke = false;

	np = of_find_compatible_node(NULL, NULL, "qcom,hdcp");
	if (!np) {
		/*select qseecom interface as default if hdcp node
		 *is not present in dtsi
		 */
		select_interface(use_smcinvoke);
		return 0;
	}

	use_smcinvoke = of_property_read_bool(np, "qcom,use-smcinvoke");

	select_interface(use_smcinvoke);

	return 0;
}
static void __exit hdcp_module_exit(void)
{
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("HDCP driver");

module_init(hdcp_module_init);
module_exit(hdcp_module_exit);
