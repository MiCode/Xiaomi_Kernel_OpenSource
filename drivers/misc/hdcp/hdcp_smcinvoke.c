// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/smcinvoke.h>
#include <soc/qcom/smci_appclient.h>
#include <soc/qcom/smci_appcontroller.h>
#include <soc/qcom/smci_apploader.h>
#include <soc/qcom/smci_clientenv.h>
#include <soc/qcom/smci_opener.h>

#include "hdcp_main.h"
#include "hdcp_smcinvoke.h"
#include "hdcp1.h"
#include "hdcp1_ops.h"
#include "hdcp2p2.h"

static int hdcp1_verify_key(struct hdcp1_smcinvoke_handle *handle)
{
	int ret = 0;

	if (!handle) {
		pr_err("invalid HDCP 1.x handle\n");
		return -EINVAL;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", HDCP1_APP_NAME);
		return -EINVAL;
	}

	ret = hdcp1_verify(handle->hdcp1_app_obj, 1);
	if (ret)
		pr_err("hdcp1_verify failed error:%d\n", ret);

	return ret;
}

static int hdcp1_key_set(struct hdcp1_smcinvoke_handle *handle,
		 uint32_t *aksv_msb, uint32_t *aksv_lsb)
{
	int ret = 0;
	uint8_t *ksv_res = NULL;
	size_t ksv_reslen = 0;

	ksv_res = kmalloc(HDCP1_AKSV_SIZE, GFP_KERNEL);
	if (!ksv_res)
		return -EINVAL;

	if (aksv_msb == NULL || aksv_lsb == NULL) {
		pr_err("invalid aksv\n");
		return -EINVAL;
	}

	if (!handle) {
		pr_err("invalid HDCP 1.x handle\n");
		return -EINVAL;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("hdcp1 app not loaded\n");
		return -EINVAL;
	}

	ret = hdcp1_set_key(handle->hdcp1_app_obj, ksv_res, HDCP1_AKSV_SIZE,
				 &ksv_reslen);
	if (ret) {
		pr_err("hdcp1_set_key failed ret=%d\n", ret);
		return -ENOKEY;
	}

	/* copy bytes into msb and lsb */
	*aksv_msb = ksv_res[0] << 24 | ksv_res[1] << 16 | ksv_res[2] << 8 | ksv_res[3];
	*aksv_lsb = ksv_res[4] << 24 | ksv_res[5] << 16 | ksv_res[6] << 8 | ksv_res[7];

	ret = hdcp1_validate_aksv(*aksv_msb, *aksv_lsb);
	if (ret)
		pr_err("aksv validation failed (%d)\n", ret);

	return ret;
}

int load_app(char *app_name, struct smci_object *app_obj,
			 struct smci_object *app_controller_obj)
{
	int ret = 0;
	uint8_t *buffer = NULL;
	struct qtee_shm shm = {0};
	size_t size = 0;
	struct smci_object client_env = {NULL, NULL};
	struct smci_object app_loader = {NULL, NULL};

	ret = get_client_env_object(&client_env);
	if (ret) {
		pr_err("get_client_env_object failed :%d\n", ret);
		client_env.invoke = NULL;
		client_env.context = NULL;
		goto error;
	}

	ret = smci_clientenv_open(client_env, SMCI_APPLOADER_UID, &app_loader);
	if (ret) {
		pr_err("smci_clientenv_open failed :%d\n", ret);
		app_loader.invoke = NULL;
		app_loader.context = NULL;
		goto error;
	}

	buffer = firmware_request_from_smcinvoke(app_name, &size, &shm);
	if (buffer == NULL) {
		pr_err("firmware_request_from_smcinvoke failed\n");
		ret = -EINVAL;
		goto error;
	}

	ret = smci_apploader_loadfrombuffer(app_loader, (const void *)buffer, size,
			app_controller_obj);
	if (ret) {
		pr_err("smci_apploader_loadfrombuffer failed :%d\n", ret);
		app_controller_obj->invoke = NULL;
		app_controller_obj->context = NULL;
		goto error;
	}

	ret = smci_appcontroller_getappobject(*app_controller_obj, app_obj);
	if (ret) {
		pr_err("smci_appcontroller_getappobject failed :%d\n", ret);
		goto error;
	}

error:
	qtee_shmbridge_free_shm(&shm);
	SMCI_OBJECT_ASSIGN_NULL(app_loader);
	SMCI_OBJECT_ASSIGN_NULL(client_env);
	return ret;
}

static int hdcp1_app_load(struct hdcp1_smcinvoke_handle *handle)
{
	int ret = 0;

	if (!handle) {
		pr_err("invalid input\n");
		ret = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_APP_LOADED)
		goto error;

	ret = load_app(HDCP1_APP_NAME, &(handle->hdcp1_app_obj),
		   &(handle->hdcp1_appcontroller_obj));
	if (ret) {
		pr_err("hdcp1 TA load failed :%d\n", ret);
		goto error;
	}

	if (SMCI_OBJECT_IS_NULL(handle->hdcp1_app_obj)) {
		pr_err("hdcp1_app_obj is NULL\n");
		ret = -EINVAL;
		goto error;
	}

	ret = load_app(HDCP1OPS_APP_NAME, &(handle->hdcp1ops_app_obj),
		   &(handle->hdcp1ops_appcontroller_obj));
	if (ret) {
		pr_warn("hdcp1ops TA load failed :%d\n", ret);
		SMCI_OBJECT_ASSIGN_NULL(handle->hdcp1ops_app_obj);
	}

	if (SMCI_OBJECT_IS_NULL(handle->hdcp1ops_app_obj)) {
		pr_warn("hdcp1ops_app_obj is NULL\n");
		ret = 0;
	}

	handle->hdcp_state |= HDCP_STATE_APP_LOADED;

error:
	return ret;
}

static void hdcp1_app_unload(struct hdcp1_smcinvoke_handle *handle)
{
	if (!handle || !handle->hdcp1_app_obj.context) {
		pr_err("invalid handle\n");
		return;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_warn("hdcp1 app not loaded\n");
		return;
	}

	SMCI_OBJECT_ASSIGN_NULL(handle->hdcp1_app_obj);
	SMCI_OBJECT_ASSIGN_NULL(handle->hdcp1_appcontroller_obj);
	SMCI_OBJECT_ASSIGN_NULL(handle->hdcp1ops_app_obj);
	SMCI_OBJECT_ASSIGN_NULL(handle->hdcp1ops_appcontroller_obj);

	handle->hdcp_state &= ~HDCP_STATE_APP_LOADED;
	pr_debug("%s app unloaded\n", HDCP1_APP_NAME);
}

void *hdcp1_init_smcinvoke(void)
{
	struct hdcp1_smcinvoke_handle *handle =
		kzalloc(sizeof(struct hdcp1_smcinvoke_handle), GFP_KERNEL);

	if (!handle)
		goto error;

error:
	return handle;
}

bool hdcp1_feature_supported_smcinvoke(void *data)
{
	int ret = 0;
	bool supported = false;
	struct hdcp1_smcinvoke_handle *handle = data;

	if (!handle) {
		pr_err("invalid handle\n");
		goto error;
	}

	if (handle->feature_supported) {
		supported = true;
		goto error;
	}

	ret = hdcp1_app_load(handle);
	if (!ret && (handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		if (!hdcp1_verify_key(handle)) {
			pr_debug("HDCP 1.x supported\n");
			handle->feature_supported = true;
			supported = true;
		}
		hdcp1_app_unload(handle);
	}
error:
	return supported;
}

int hdcp1_set_enc_smcinvoke(void *data, bool enable)
{
	int ret = 0;
	struct hdcp1_smcinvoke_handle *handle = data;

	if (!handle || !handle->hdcp1_app_obj.context) {
		pr_err("invalid HDCP 1.x handle\n");
		return -EINVAL;
	}

	if (!handle->feature_supported) {
		pr_err("HDCP 1.x not supported\n");
		return -EINVAL;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", HDCP1_APP_NAME);
		return -EINVAL;
	}

	ret = hdcp1_set_encryption(handle->hdcp1_app_obj, enable);
	if (ret)
		pr_err("hdcp1_set_encryption failed :%d\n", ret);

	return ret;
}

int hdcp1_ops_notify_smcinvoke(void *data, void *topo, bool is_authenticated)
{
	int ret = 0;
	struct hdcp1_smcinvoke_handle *handle = data;

	if (!handle || !handle->hdcp1ops_app_obj.context) {
		pr_err("invalid HDCP 1.x ops handle\n");
		return -EINVAL;
	}

	if (!handle->feature_supported) {
		pr_err("HDCP 1.x not supported\n");
		return -EINVAL;
	}

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", HDCP1OPS_APP_NAME);
		return -EINVAL;
	}

	ret = hdcp1_ops_notify_topology_change(handle->hdcp1ops_app_obj);
	if (ret)
		pr_err("hdcp1_ops_notify_topology_change failed, ret=%d\n", ret);

	return ret;
}

int hdcp1_start_smcinvoke(void *data, u32 *aksv_msb, u32 *aksv_lsb)
{
	int ret = 0;
	struct hdcp1_smcinvoke_handle *handle = data;

	if (!aksv_msb || !aksv_lsb) {
		pr_err("invalid aksv output buffer\n");
		ret = -EINVAL;
		goto error;
	}

	if (!handle) {
		pr_err("invalid handle\n");
		ret = -EINVAL;
		goto error;
	}

	if (!handle->feature_supported) {
		pr_err("feature not supported\n");
		ret = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_APP_LOADED) {
		pr_debug("%s app already loaded\n", HDCP1_APP_NAME);
		goto error;
	}

	ret = hdcp1_app_load(handle);
	if (ret)
		goto error;

	ret = hdcp1_key_set(handle, aksv_msb, aksv_lsb);
	if (ret)
		goto key_error;

	pr_debug("success\n");
	return ret;

key_error:
	hdcp1_app_unload(handle);
error:
	return ret;
}

void hdcp1_stop_smcinvoke(void *data)
{
	struct hdcp1_smcinvoke_handle *hdcp1_handle = data;

	if (!hdcp1_handle) {
		pr_err("invalid HDCP 1.x handle\n");
		return;
	}

	if (!(hdcp1_handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("hdcp1 app not loaded\n");
		return;
	}

	SMCI_OBJECT_ASSIGN_NULL(hdcp1_handle->hdcp1_app_obj);
	SMCI_OBJECT_ASSIGN_NULL(hdcp1_handle->hdcp1_appcontroller_obj);
	SMCI_OBJECT_ASSIGN_NULL(hdcp1_handle->hdcp1ops_app_obj);
	SMCI_OBJECT_ASSIGN_NULL(hdcp1_handle->hdcp1ops_appcontroller_obj);

	hdcp1_handle->hdcp_state &= ~HDCP_STATE_APP_LOADED;
}

void *hdcp2_init_smcinvoke(u32 device_type)
{
	struct hdcp2_smcinvoke_handle *handle =
		kzalloc(sizeof(struct hdcp2_smcinvoke_handle), GFP_KERNEL);

	if (!handle)
		goto error;

	handle->device_type = device_type;

error:
	return handle;
}

void hdcp2_deinit_smcinvoke(void *ctx)
{
	kfree_sensitive(ctx);
}

int hdcp_get_version(struct hdcp2_smcinvoke_handle *handle)
{
	int ret = 0;
	uint32_t app_major_version = 0;
	uint32_t appversion = 0;

	if (handle->hdcp_state & HDCP_STATE_APP_LOADED) {
		pr_err("hdcp2p2 TA already loaded\n");
		goto error;
	}

	ret = hdcp2p2_version(handle->hdcp2_app_obj, &appversion);
	if (ret) {
		pr_err("hdcp2p2_version failed :%d\n", ret);
		goto error;
	}
	app_major_version = HCDP_TXMTR_GET_MAJOR_VERSION(appversion);

	pr_debug("hdp2p2 app major version %d, app version %d\n", app_major_version,
			 appversion);
error:
	return ret;
}

int hdcp2_app_init(struct hdcp2_smcinvoke_handle *handle)
{
	int ret = 0;
	uint32_t app_minor_version = 0;
	uint32_t clientversion = 0;
	uint32_t appversion = 0;

	if (handle->hdcp_state & HDCP_STATE_APP_LOADED) {
		pr_err("hdcp2p2 TA already loaded\n");
		goto error;
	}

	clientversion = HDCP_CLIENT_MAKE_VERSION(HDCP_CLIENT_MAJOR_VERSION,
					HDCP_CLIENT_MINOR_VERSION,
					HDCP_CLIENT_PATCH_VERSION);

	ret = hdcp2p2_init(handle->hdcp2_app_obj, clientversion, &appversion);
	if (ret) {
		pr_err("hdcp2p2_init failed:%d\n", ret);
		goto error;
	}

	app_minor_version = HCDP_TXMTR_GET_MINOR_VERSION(appversion);
	if (app_minor_version != HDCP_CLIENT_MINOR_VERSION) {
		pr_err("client-app minor version mismatch app(%d), client(%d)\n",
			   app_minor_version, HDCP_CLIENT_MINOR_VERSION);
		ret = -1;
		goto error;
	}

	pr_err("client version major(%d), minor(%d), patch(%d)\n",
		   HDCP_CLIENT_MAJOR_VERSION, HDCP_CLIENT_MINOR_VERSION,
		   HDCP_CLIENT_PATCH_VERSION);

	pr_err("app version major(%d), minor(%d), patch(%d)\n",
		   HCDP_TXMTR_GET_MAJOR_VERSION(appversion),
		   HCDP_TXMTR_GET_MINOR_VERSION(appversion),
		   HCDP_TXMTR_GET_PATCH_VERSION(appversion));
error:
	return ret;
}

int hdcp2_app_tx_init(struct hdcp2_smcinvoke_handle *handle)
{
	int ret = 0;
	uint32_t ctxhandle = 0;

	if (!(handle->hdcp_state & HDCP_STATE_SESSION_INIT)) {
		pr_err("session not initialized\n");
		ret = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_TXMTR_INIT) {
		pr_err("txmtr already initialized\n");
		goto error;
	}

	ret = hdcp2p2_tx_init(handle->hdcp2_app_obj, handle->session_id, &ctxhandle);
	if (ret) {
		pr_err("hdcp2p2_tx_init failed :%d\n", ret);
		goto error;
	}

	handle->tz_ctxhandle = ctxhandle;
	handle->hdcp_state |= HDCP_STATE_TXMTR_INIT;

error:
	return ret;
}

int hdcp2_app_tx_deinit(struct hdcp2_smcinvoke_handle *handle)
{
	int ret = 0;

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("hdcp2p2 TA not loaded\n");
		ret = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		pr_err("txmtr not initialized\n");
		ret = -EINVAL;
		goto error;
	}

	ret = hdcp2p2_tx_deinit(handle->hdcp2_app_obj, handle->tz_ctxhandle);
	if (ret) {
		pr_err("hdcp2p2_tx_deinit failed :%d\n", ret);
		goto error;
	}
	handle->hdcp_state &= ~HDCP_STATE_TXMTR_INIT;

error:
	return ret;
}

static int hdcp2_app_load(struct hdcp2_smcinvoke_handle *handle)
{
	int ret = 0;

	if (!handle) {
		pr_err("invalid input\n");
		ret = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_APP_LOADED) {
		pr_err("hdcp2p2 TA already loaded\n");
		goto error;
	}

	ret = load_app(HDCP2P2_APP_NAME, &(handle->hdcp2_app_obj),
				   &(handle->hdcp2_appcontroller_obj));
	if (ret) {
		pr_err("hdcp2p2 TA load_app failed :%d\n", ret);
		goto error;
	}

	if (SMCI_OBJECT_IS_NULL(handle->hdcp2_app_obj)) {
		pr_err("hdcp2p2 app object is NULL\n");
		ret = -EINVAL;
		goto error;
	}

	ret = load_app(HDCPSRM_APP_NAME, &(handle->hdcpsrm_app_obj),
		   &(handle->hdcpsrm_appcontroller_obj));
	if (ret) {
		pr_err("hdcpsrm TA load failed :%d\n", ret);
		goto error;
	}

	if (SMCI_OBJECT_IS_NULL(handle->hdcpsrm_app_obj)) {
		pr_err("hdcpsrm app object is NULL\n");
		ret = -EINVAL;
		goto error;
	}

	ret = hdcp_get_version(handle);
	if (ret) {
		pr_err("library get version failed\n");
		goto error;
	}

	ret = hdcp2_app_init(handle);
	if (ret) {
		pr_err("app init failed\n");
		goto error;
	}

	handle->hdcp_state |= HDCP_STATE_APP_LOADED;

error:
	return ret;
}

static int hdcp2_app_unload(struct hdcp2_smcinvoke_handle *handle)
{
	int ret = 0;

	ret = hdcp2p2_deinit(handle->hdcp2_app_obj);
	if (ret) {
		pr_err("hdcp2p2_deinit failed:%d\n", ret);
		goto error;
	}

	SMCI_OBJECT_ASSIGN_NULL(handle->hdcp2_app_obj);
	SMCI_OBJECT_ASSIGN_NULL(handle->hdcp2_appcontroller_obj);
	SMCI_OBJECT_ASSIGN_NULL(handle->hdcpsrm_app_obj);
	SMCI_OBJECT_ASSIGN_NULL(handle->hdcpsrm_appcontroller_obj);

	handle->hdcp_state &= ~HDCP_STATE_APP_LOADED;

error:
	return ret;
}

static int hdcp2_verify_key(struct hdcp2_smcinvoke_handle *handle)
{
	int ret = 0;

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("%s app not loaded\n", HDCP2P2_APP_NAME);
		ret = -EINVAL;
		goto error;
	}

	ret = hdcp2p2_verify_key(handle->hdcp2_app_obj);
	if (ret) {
		pr_err("hdcp2p2_verify_key failed:%d\n", ret);
		goto error;
	}

error:
	return ret;
}

static int hdcp2_app_session_init(struct hdcp2_smcinvoke_handle *handle)
{
	int ret = 0;
	uint32_t session_id = 0;

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("hdcp2p2 app not loaded\n");
		ret = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state & HDCP_STATE_SESSION_INIT) {
		pr_err("session already initialized\n");
		goto error;
	}

	if (SMCI_OBJECT_IS_NULL(handle->hdcp2_app_obj)) {
		pr_err("hdcp2_app_obj is NULL\n");
		goto error;
	}

	ret = hdcp2p2_session_init(handle->hdcp2_app_obj, handle->device_type,
			&session_id);
	if (ret) {
		pr_err("hdcp2p2_session_init failed ret:%d\n", ret);
		goto error;
	}

	handle->session_id = session_id;
	handle->hdcp_state |= HDCP_STATE_SESSION_INIT;
error:
	return ret;
}

static int hdcp2_app_session_deinit(struct hdcp2_smcinvoke_handle *handle)
{
	int ret = 0;

	if (!(handle->hdcp_state & HDCP_STATE_APP_LOADED)) {
		pr_err("hdcp2p2 app not loaded\n");
		ret = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_SESSION_INIT)) {
		pr_err("session not initialized\n");
		ret = -EINVAL;
		goto error;
	}

	ret = hdcp2p2_session_deinit(handle->hdcp2_app_obj, handle->session_id);
	if (ret) {
		pr_err("hdcp2p2_session_deinit failed:%d\n", ret);
		goto error;
	}

	handle->hdcp_state &= ~HDCP_STATE_SESSION_INIT;
error:
	return ret;
}

int hdcp2_app_start_smcinvoke(void *ctx, uint32_t req_len)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;

	handle = (struct hdcp2_smcinvoke_handle *)ctx;

	if (!handle) {
		pr_err("Invalid handle\n");
		ret = -EINVAL;
		goto error;
	}

	handle->app_data.request.data = kmalloc(MAX_RX_MESSAGE_SIZE, GFP_KERNEL);
	if (!handle->app_data.request.data) {
		ret = -EINVAL;
		goto error;
	}

	handle->app_data.response.data = kmalloc(MAX_TX_MESSAGE_SIZE, GFP_KERNEL);
	if (!handle->app_data.response.data) {
		ret = -EINVAL;
		goto error;
	}

	ret = hdcp2_app_load(handle);
	if (ret)
		goto error;

	ret = hdcp2_app_session_init(handle);
	if (ret)
		goto error;

	ret = hdcp2_app_tx_init(handle);
	if (ret)
		goto error;

error:
	return ret;
}

int hdcp2_app_start_auth_smcinvoke(void *ctx, uint32_t req_len)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;
	size_t res_msgout = 0;
	uint32_t timeout = 0;
	uint32_t flag = 0;
	uint32_t ctxhandle = 0;

	uint8_t res_msg[MAX_TX_MESSAGE_SIZE] = {0};

	handle = ctx;

	if (!handle) {
		pr_err("Invalid handle\n");
		ret = -EINVAL;
		goto error;
	}

	handle->app_data.request.length = req_len;

	if (!(handle->hdcp_state & HDCP_STATE_SESSION_INIT)) {
		pr_err("session not initialized\n");
		ret = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		pr_err("txmtr not initialized\n");
		ret = -EINVAL;
		goto error;
	}

	ret = hdcp2p2_start_auth(handle->hdcp2_app_obj, handle->tz_ctxhandle,
	  res_msg, MAX_TX_MESSAGE_SIZE, &res_msgout, &timeout,
	  &flag, &ctxhandle);
	if (ret) {
		pr_err("hdcp2p2_start_auth failed :%d\n", ret);
		goto error;
	}

	memcpy(handle->app_data.response.data, res_msg, res_msgout);

	handle->app_data.response.length = res_msgout;
	handle->app_data.timeout = timeout;
	handle->app_data.repeater_flag = false;

	handle->tz_ctxhandle = ctxhandle;

error:
	return ret;
}

int hdcp2_app_process_msg_smcinvoke(void *ctx, uint32_t req_len)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;
	size_t res_msglen = 0;
	uint32_t timeout = 0;
	uint32_t flag = 0;

	uint8_t res_msg[MAX_TX_MESSAGE_SIZE] = {0};

	handle = ctx;

	if (!handle) {
		pr_err("Invalid handle\n");
		ret = -EINVAL;
		goto error;
	}

	handle->app_data.request.length = req_len;

	if (!handle->app_data.request.data) {
		pr_err("invalid request buffer\n");
		ret = -EINVAL;
		goto error;
	}

	ret = hdcp2p2_rcvd_msg(
		handle->hdcp2_app_obj, handle->app_data.request.data,
		handle->app_data.request.length, handle->tz_ctxhandle, res_msg,
		MAX_TX_MESSAGE_SIZE, &res_msglen, &timeout, &flag);
	if (ret) {
		pr_err("hdcp2p2_rcvd_msg failed :%d\n", ret);
		goto error;
	}

	memcpy(handle->app_data.response.data, res_msg, res_msglen);

	/* check if it's a repeater */
	if (flag == HDCP_TXMTR_SUBSTATE_WAITING_FOR_RECIEVERID_LIST)
		handle->app_data.repeater_flag = true;

	handle->app_data.response.length = res_msglen;
	handle->app_data.timeout = timeout;

error:
	return ret;
}

int hdcp2_app_timeout_smcinvoke(void *ctx, uint32_t req_len)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;
	uint32_t timeout = 0;
	size_t res_msglen_out = 0;

	uint8_t res_msg[MAX_TX_MESSAGE_SIZE] = {0};

	handle = ctx;

	if (!handle) {
		pr_err("Invalid handle\n");
		ret = -EINVAL;
		goto error;
	}

	handle->app_data.request.length = req_len;

	ret = hdcp2p2_send_timeout(handle->hdcp2_app_obj, handle->tz_ctxhandle,
		res_msg, MAX_TX_MESSAGE_SIZE, &res_msglen_out,
		&timeout);
	if (ret) {
		pr_err("hdcp2p2_send_timeout failed :%d\n", ret);
		goto error;
	}

	memcpy(handle->app_data.response.data, res_msg, res_msglen_out);

	handle->app_data.response.length = res_msglen_out;
	handle->app_data.timeout = timeout;

error:
	return ret;
}

int hdcp2_app_enable_encryption_smcinvoke(void *ctx, uint32_t req_len)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;

	handle = ctx;

	if (!handle) {
		pr_err("Invalid handle\n");
		ret = -EINVAL;
		goto error;
	}

	handle->app_data.request.length = req_len;

	/*
	 * wait at least 200ms before enabling encryption
	 * as per hdcp2p2 specifications.
	 */
	msleep(SLEEP_SET_HW_KEY_MS);

	ret = hdcp2p2_set_hw_key(handle->hdcp2_app_obj, handle->tz_ctxhandle);
	if (ret) {
		pr_err("hdcp2p2_set_hw_key failed:%d\n", ret);
		goto error;
	}

	handle->hdcp_state |= HDCP_STATE_AUTHENTICATED;
error:
	return ret;
}

int hdcp2_app_query_stream_smcinvoke(void *ctx, uint32_t req_len)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;

	uint32_t timeout = 0;
	size_t res_msglen_out = 0;

	uint8_t res_msg[MAX_TX_MESSAGE_SIZE] = {0};

	handle = ctx;

	if (!handle) {
		pr_err("Invalid handle\n");
		ret = -EINVAL;
		goto error;
	}

	handle->app_data.request.length = req_len;

	ret = hdcp2p2_query_stream_type(
		handle->hdcp2_app_obj, handle->tz_ctxhandle, res_msg,
		MAX_TX_MESSAGE_SIZE, &res_msglen_out, &timeout);
	if (ret) {
		pr_err("hdcp2p2_query_stream_type failed :%d\n", ret);
		goto error;
	}

	memcpy(handle->app_data.response.data, res_msg, res_msglen_out);

	handle->app_data.response.length = res_msglen_out;
	handle->app_data.timeout = timeout;
error:
	return ret;
}

int hdcp2_app_stop_smcinvoke(void *ctx)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;

	handle = ctx;

	if (!handle) {
		pr_err("Invalid handle\n");
		ret = -EINVAL;
		goto end;
	}

	ret = hdcp2_app_tx_deinit(handle);
	if (ret)
		goto end;

	ret = hdcp2_app_session_deinit(handle);
	if (ret)
		goto end;

	ret = hdcp2_app_unload(handle);

	kfree(handle->app_data.request.data);
	kfree(handle->app_data.response.data);

end:
	return ret;
}

bool hdcp2_feature_supported_smcinvoke(void *ctx)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;
	bool supported = false;

	handle = ctx;

	if (!handle) {
		pr_err("invalid input\n");
		ret = -EINVAL;
		goto error;
	}

	if (handle->feature_supported) {
		supported = true;
		goto error;
	}

	ret = hdcp2_app_load(handle);
	if (!ret) {
		if (!hdcp2_verify_key(handle)) {
			pr_debug("HDCP 2.2 supported\n");
			handle->feature_supported = true;
			supported = true;
		}
		hdcp2_app_unload(handle);
	}
error:
	return supported;
}

int hdcp2_force_encryption_smcinvoke(void *ctx, uint32_t enable)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;

	handle = ctx;

	if (!handle) {
		pr_err("Invalid handle\n");
		ret = -EINVAL;
		goto error;
	}

	if (handle->hdcp_state == HDCP_STATE_AUTHENTICATED)
		msleep(SLEEP_FORCE_ENCRYPTION_MS);

	ret = hdcp2p2_force_encryption(handle->hdcp2_app_obj, handle->tz_ctxhandle,
		enable);
	if (ret) {
		pr_err("hdcp2p2_force_encryption failed :%d\n", ret);
		goto error;
	}

error:
	return ret;
}

int hdcp2_open_stream_smcinvoke(void *ctx, uint8_t vc_payload_id,
		uint8_t stream_number, uint32_t *stream_id)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;
	uint32_t streamid = 0;

	handle = ctx;

	if (!handle) {
		pr_err("Invalid handle\n");
		ret = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_SESSION_INIT)) {
		pr_err("session not initialized\n");
		ret = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		pr_err("txmtr not initialized\n");
		ret = -EINVAL;
		goto error;
	}

	ret = hdcp2p2_session_open_stream(handle->hdcp2_app_obj,
		   handle->session_id, vc_payload_id,
		   stream_number, 0, &streamid);
	if (ret) {
		pr_err("hdcp2p2_session_open_stream failed :%d\n", ret);
		goto error;
	}

	*stream_id = streamid;

error:
	return ret;
}

int hdcp2_close_stream_smcinvoke(void *ctx, uint32_t stream_id)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;

	handle = ctx;

	if (!handle) {
		pr_err("Invalid handle\n");
		ret = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_SESSION_INIT)) {
		pr_err("session not initialized\n");
		ret = -EINVAL;
		goto error;
	}

	if (!(handle->hdcp_state & HDCP_STATE_TXMTR_INIT)) {
		pr_err("txmtr not initialized\n");
		ret = -EINVAL;
		goto error;
	}

	ret = hdcp2p2_session_close_stream(handle->hdcp2_app_obj,
		handle->session_id, stream_id);
	if (ret) {
		pr_err("hdcp2p2_session_close_stream failed :%d\n", ret);
		goto error;
	}

error:
	return ret;
}

int hdcp2_update_app_data_smcinvoke(void *ctx, struct hdcp2_app_data *app_data)
{
	struct hdcp2_smcinvoke_handle *handle = NULL;
	int ret = 0;

	handle = ctx;

	if (!handle || !app_data) {
		pr_err("Invalid handle\n");
		return -EINVAL;
	}

	app_data->request.data = handle->app_data.request.data;
	app_data->request.length = handle->app_data.request.length;
	app_data->response.data = handle->app_data.response.data;
	app_data->response.length = handle->app_data.response.length;
	app_data->timeout = handle->app_data.timeout;
	app_data->repeater_flag = handle->app_data.repeater_flag;
	return ret;
}
