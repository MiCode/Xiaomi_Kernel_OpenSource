/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/async.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/device-mapper.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/qseecomi.h>
#include <crypto/ice.h>
#include "pfk_ice.h"
#include "qseecom_kernel.h"

/**********************************/
/** global definitions		 **/
/**********************************/

#define TZ_ES_INVALIDATE_ICE_KEY 0x3
#define TZ_ES_CONFIG_SET_ICE_KEY 0x4

/* index 0 and 1 is reserved for FDE */
#define MIN_ICE_KEY_INDEX 2

#define MAX_ICE_KEY_INDEX 31

#define TZ_ES_CONFIG_SET_ICE_KEY_ID \
	TZ_SYSCALL_CREATE_SMC_ID(TZ_OWNER_SIP, TZ_SVC_ES, \
	TZ_ES_CONFIG_SET_ICE_KEY)

#define TZ_ES_INVALIDATE_ICE_KEY_ID \
		TZ_SYSCALL_CREATE_SMC_ID(TZ_OWNER_SIP, \
			TZ_SVC_ES, TZ_ES_INVALIDATE_ICE_KEY)

#define TZ_ES_INVALIDATE_ICE_KEY_PARAM_ID \
	TZ_SYSCALL_CREATE_PARAM_ID_1( \
	TZ_SYSCALL_PARAM_TYPE_VAL)

#define TZ_ES_CONFIG_SET_ICE_KEY_PARAM_ID \
	TZ_SYSCALL_CREATE_PARAM_ID_5( \
	TZ_SYSCALL_PARAM_TYPE_VAL, \
	TZ_SYSCALL_PARAM_TYPE_BUF_RW, TZ_SYSCALL_PARAM_TYPE_VAL, \
	TZ_SYSCALL_PARAM_TYPE_VAL, TZ_SYSCALL_PARAM_TYPE_VAL)

#define CONTEXT_SIZE 0x1000

#define KEYMASTER_UTILS_CMD_ID 0x200UL
#define KEYMASTER_GET_VERSION (KEYMASTER_UTILS_CMD_ID + 0UL)
#define KEYMASTER_SET_ICE_KEY (KEYMASTER_UTILS_CMD_ID + 18UL)
#define KEYMASTER_CLEAR_ICE_KEY (KEYMASTER_UTILS_CMD_ID + 19UL)

#define USE_KM_MAJOR_VERSION 4
#define USE_KM_MINOR_VERSION 513
#define ICE_BUFFER_SIZE 64

static uint32_t keymaster_minor_version;
static uint32_t keymaster_major_version;

static uint8_t ice_buffer[ICE_BUFFER_SIZE];

static struct qseecom_handle *qhandle;

enum {
	ICE_CIPHER_MODE_XTS_128 = 0,
	ICE_CIPHER_MODE_CBC_128 = 1,
	ICE_CIPHER_MODE_XTS_256 = 3,
	ICE_CIPHER_MODE_CBC_256 = 4
};

static uint32_t get_keymaster_version(struct qseecom_handle *qhandle)
{
	int ret = 0;
	struct pfk_km_get_version_req *req;
	struct pfk_km_get_version_rsp *rsp;

	req = (struct pfk_km_get_version_req *) qhandle->sbuf;
	req->cmd_id = KEYMASTER_GET_VERSION;

	rsp = (struct pfk_km_get_version_rsp *) (qhandle->sbuf
		+ sizeof(struct pfk_km_get_version_req));

	ret = qseecom_send_command(qhandle,
			req, sizeof(struct pfk_km_get_version_req),
			rsp, sizeof(struct pfk_km_get_version_rsp));

	if (ret) {
		pr_err("%s: Get KM version error: Status %d\n", __func__,
						rsp->status);
		return ret;
	}

	keymaster_major_version = rsp->ta_major_version;
	keymaster_minor_version = rsp->ta_minor_version;

	return ret;
}

/*
 * This change is to make sure there are no issues when pfk calls into
 * keymaster for unwrapping keys before setting them. This can happen when
 * an older version of keymaster is used which means the unwrapping logic
 * cannot be done in trustzone.
 */

static bool should_use_keymaster(void)
{
	int ret = -1;
	if (!qhandle) {
		ret = qseecom_start_app(&qhandle, "keymaster64",
			CONTEXT_SIZE);
		if (ret) {
			pr_err("Qseecom start app failed\n");
			return false;
		}
	}

	if (keymaster_major_version == 0 || keymaster_minor_version == 0) {
		ret = get_keymaster_version(qhandle);
		if (ret) {
			pr_err("Error in getting keymaster version\n");
			return false;
		}
	}

	if (keymaster_major_version == USE_KM_MAJOR_VERSION &&
			keymaster_minor_version < USE_KM_MINOR_VERSION)
		return true;
	else
		return false;
}

static int set_wrapped_key(uint32_t index, const uint8_t *key,
				const uint8_t *salt)
{
	int ret = 0;
	u32 set_req_len = 0;
	u32 set_rsp_len = 0;
	u32 size = ICE_BUFFER_SIZE / 2;
	struct pfk_ice_key_req *set_req_buf;
	struct pfk_ice_key_rsp *set_rsp_buf;

	memcpy(ice_buffer, key, size);
	memcpy(ice_buffer + size, salt, size);

	set_req_buf = (struct pfk_ice_key_req *) qhandle->sbuf;
	set_req_buf->cmd_id = KEYMASTER_SET_ICE_KEY;
	set_req_buf->index = index;
	set_req_buf->ice_key_offset = sizeof(struct pfk_ice_key_req);
	set_req_buf->ice_key_size = size;
	set_req_buf->ice_salt_offset = set_req_buf->ice_key_offset +
					set_req_buf->ice_key_size;
	set_req_buf->ice_salt_size = size;

	memcpy((uint8_t *) set_req_buf + set_req_buf->ice_key_offset,
			ice_buffer, set_req_buf->ice_key_size);
	memcpy((uint8_t *) set_req_buf + set_req_buf->ice_salt_offset,
			ice_buffer + set_req_buf->ice_key_size,
			set_req_buf->ice_salt_size);

	set_req_len = sizeof(struct pfk_ice_key_req) + set_req_buf->ice_key_size
			+ set_req_buf->ice_salt_size;

	set_rsp_buf = (struct pfk_ice_key_rsp *) (qhandle->sbuf +
			set_req_len);
	set_rsp_len = sizeof(struct pfk_ice_key_rsp);

	ret = qseecom_send_command(qhandle,
					set_req_buf, set_req_len,
					set_rsp_buf, set_rsp_len);

	if (ret)
		pr_err("%s: Set wrapped key  error: Status %d\n", __func__,
						set_rsp_buf->ret);

	return ret;
}

static int clear_wrapped_key(uint32_t index)
{
	int ret = 0;

	u32 clear_req_len = 0;
	u32 clear_rsp_len = 0;
	struct pfk_ice_key_req *clear_req_buf;
	struct pfk_ice_key_rsp *clear_rsp_buf;

	clear_req_buf = (struct pfk_ice_key_req *) qhandle->sbuf;
	memset(clear_req_buf, 0, sizeof(qhandle->sbuf));
	clear_req_buf->cmd_id = KEYMASTER_CLEAR_ICE_KEY;
	clear_req_buf->index = index;
	clear_req_len = sizeof(struct pfk_ice_key_req);
	clear_rsp_buf = (struct pfk_ice_key_rsp *) (qhandle->sbuf +
			QSEECOM_ALIGN(clear_req_len));
	clear_rsp_len = sizeof(struct pfk_ice_key_rsp);

	ret = qseecom_send_command(qhandle, clear_req_buf, clear_req_len,
			clear_rsp_buf, clear_rsp_len);
	if (ret)
		pr_err("%s: Clear wrapped key error: Status %d\n", __func__,
					clear_rsp_buf->ret);

	return ret;
}

static int set_key(uint32_t index, const uint8_t *key, const uint8_t *salt,
		unsigned int data_unit)
{
	struct scm_desc desc = {0};
	int ret = 0;
	uint32_t smc_id = 0;
	char *tzbuf = (char *)ice_buffer;
	uint32_t size = ICE_BUFFER_SIZE / 2;

	if (!tzbuf) {
		pr_err("%s No Memory\n", __func__);
		return -ENOMEM;
	}

	memset(tzbuf, 0, ICE_BUFFER_SIZE);

	memcpy(ice_buffer, key, size);
	memcpy(ice_buffer+size, salt, size);

	dmac_flush_range(tzbuf, tzbuf + ICE_BUFFER_SIZE);

	smc_id = TZ_ES_CONFIG_SET_ICE_KEY_ID;

	desc.arginfo = TZ_ES_CONFIG_SET_ICE_KEY_PARAM_ID;
	desc.args[0] = index;
	desc.args[1] = virt_to_phys(tzbuf);
	desc.args[2] = ICE_BUFFER_SIZE;
	desc.args[3] = ICE_CIPHER_MODE_XTS_256;
	desc.args[4] = data_unit;

	ret = scm_call2_noretry(smc_id, &desc);
	if (ret)
		pr_err("%s:SCM call Error: 0x%x\n", __func__, ret);

	return ret;
}

static int clear_key(uint32_t index)
{
	struct scm_desc desc = {0};
	int ret = 0;
	uint32_t smc_id = 0;

	smc_id = TZ_ES_INVALIDATE_ICE_KEY_ID;

	desc.arginfo = TZ_ES_INVALIDATE_ICE_KEY_PARAM_ID;
	desc.args[0] = index;

	ret = scm_call2_noretry(smc_id, &desc);
	if (ret)
		pr_err("%s:SCM call Error: 0x%x\n", __func__, ret);
	return ret;
}

int qti_pfk_ice_set_key(uint32_t index, uint8_t *key, uint8_t *salt,
			char *storage_type, unsigned int data_unit)
{
	int ret = 0, ret1 = 0;
	char *s_type = storage_type;

	if (index < MIN_ICE_KEY_INDEX || index > MAX_ICE_KEY_INDEX) {
		pr_err("%s Invalid index %d\n", __func__, index);
		return -EINVAL;
	}
	if (!key || !salt) {
		pr_err("%s Invalid key/salt\n", __func__);
		return -EINVAL;
	}

	if (s_type == NULL) {
		pr_err("%s Invalid Storage type\n", __func__);
		return -EINVAL;
	}

	ret = qcom_ice_setup_ice_hw((const char *)s_type, true);
	if (ret) {
		pr_err("%s: could not enable clocks: %d\n", __func__, ret);
		goto out;
	}

	if (should_use_keymaster()) {
		pr_debug("%s: Setting wrapped key\n", __func__);
		ret = set_wrapped_key(index, key, salt);
	} else {
		pr_debug("%s: Setting keys with QSEE kernel\n", __func__);
		ret = set_key(index, key, salt, data_unit);
	}

	if (ret) {
		pr_err("%s: Set Key Error: %d\n", __func__, ret);
		if (ret == -EBUSY) {
			if (qcom_ice_setup_ice_hw((const char *)s_type, false))
				pr_err("%s: clock disable failed\n", __func__);
				goto out;
		}
		/* Try to invalidate the key to keep ICE in proper state */
		if (should_use_keymaster())
			ret1 = clear_wrapped_key(index);
		else
			ret1 = clear_key(index);

		if (ret1)
			pr_err("%s: Invalidate key error: %d\n", __func__, ret);
	}

	ret1 = qcom_ice_setup_ice_hw((const char *)s_type, false);
	if (ret)
		pr_err("%s: Error %d disabling clocks\n", __func__, ret);

out:
	return ret;
}

int qti_pfk_ice_invalidate_key(uint32_t index, char *storage_type)
{
	int ret = 0;

	if (index < MIN_ICE_KEY_INDEX || index > MAX_ICE_KEY_INDEX) {
		pr_err("%s Invalid index %d\n", __func__, index);
		return -EINVAL;
	}

	if (storage_type == NULL) {
		pr_err("%s Invalid Storage type\n", __func__);
		return -EINVAL;
	}

	ret = qcom_ice_setup_ice_hw((const char *)storage_type, true);
	if (ret) {
		pr_err("%s: could not enable clocks: 0x%x\n", __func__, ret);
		return ret;
	}

	if (should_use_keymaster()) {
		ret = clear_wrapped_key(index);
		pr_debug("%s: Clearing wrapped key\n", __func__);
	} else {
		pr_debug("%s: Clearing keys with QSEE kernel\n", __func__);
		ret = clear_key(index);
	}

	if (ret)
		pr_err("%s: Invalidate key error: %d\n", __func__, ret);

	if (qcom_ice_setup_ice_hw((const char *)storage_type, false))
		pr_err("%s: could not disable clocks\n", __func__);

	return ret;
}
