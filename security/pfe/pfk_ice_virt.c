/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/habmm.h>
#include "pfk_ice.h"

/**********************************/
/** global definitions		 **/
/**********************************/

#define AES256_KEY_SIZE        (32)
#define RESERVE_SIZE           (36*sizeof(uint16_t))

/* FBE request command ids */
#define	FDE_CMD_SET_KEY           (0)
#define	FDE_CMD_UPDATE_PASSWORD   (1)
#define	FDE_CMD_CLEAR_KEY         (2)
#define	FDE_CMD_WIPE_KEY          (3)
#define	FDE_SET_ICE               (4)
#define	FBE_SET_KEY               (5)
#define	FBE_CLEAR_KEY             (6)
#define	FBE_GET_MAX_SLOTS         (7)

struct fbe_request_t {
	uint8_t  reserve[RESERVE_SIZE]; // for compatibility
	uint32_t cmd;
	uint8_t  key[AES256_KEY_SIZE];
	uint8_t  salt[AES256_KEY_SIZE];
	uint32_t virt_slot;
};

static int32_t send_fbe_req(struct fbe_request_t *req, int32_t *status)
{
	int ret = 0;
	uint32_t status_size;
	uint32_t handle;

	if (!req || !status) {
		pr_err("%s Null input\n", __func__);
		return -EINVAL;
	}

	ret = habmm_socket_open(&handle, MM_FDE_1, 0, 0);
	if (ret) {
		pr_err("habmm_socket_open failed with ret = %d\n", ret);
		return ret;
	}

	ret = habmm_socket_send(handle, req, sizeof(*req), 0);
	if (ret) {
		pr_err("habmm_socket_send failed, ret= 0x%x\n", ret);
		return ret;
	}

	do {
		status_size = sizeof(*status);
		ret = habmm_socket_recv(handle, status, &status_size, 0,
				HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	} while (-EINTR == ret);

	if (ret) {
		pr_err("habmm_socket_recv failed, ret= 0x%x\n", ret);
		return ret;
	}

	if (status_size != sizeof(*status)) {
		pr_err("habmm_socket_recv expected size: %lu, actual=%u\n",
				sizeof(*status),
				status_size);
		return -E2BIG;
	}

	ret = habmm_socket_close(handle);
	if (ret) {
		pr_err("habmm_socket_close failed with ret = %d\n", ret);
		return ret;
	}
	return 0;
}

int qti_pfk_ice_set_key(uint32_t index, uint8_t *key, uint8_t *salt,
			char *storage_type, unsigned int data_unit)
{
	struct fbe_request_t req;
	int32_t ret = 0, status = 0;

	if (!key || !salt) {
		pr_err("%s Invalid key/salt\n", __func__);
		return -EINVAL;
	}

	req.cmd = FBE_SET_KEY;
	req.virt_slot = index;
	memcpy(&(req.key[0]), key, AES256_KEY_SIZE);
	memcpy(&(req.salt[0]), salt, AES256_KEY_SIZE);

	ret = send_fbe_req(&req, &status);
	if (ret || status) {
		pr_err("send_fbe_req failed with ret = %d, status = %d\n",
				ret, status);
		return -ECOMM;
	}

	return 0;
}

int qti_pfk_ice_invalidate_key(uint32_t index, char *storage_type)
{
	struct fbe_request_t req;
	int32_t ret = 0, status = 0;

	req.cmd = FBE_CLEAR_KEY;
	req.virt_slot = index;

	ret = send_fbe_req(&req, &status);
	if (ret || status) {
		pr_err("send_fbe_req failed with ret = %d, status = %d\n",
				ret, status);
		return -ECOMM;
	}

	return 0;
}

int qti_pfk_ice_get_info(uint32_t *min_slot_index, uint32_t *total_num_slots,
		bool async)
{
	struct fbe_request_t req;
	int32_t ret = 0, max_slots = 0;

	if (!min_slot_index || !total_num_slots) {
		pr_err("%s Null input\n", __func__);
		return -EINVAL;
	}

	if (async)
		return -EAGAIN;

	req.cmd = FBE_GET_MAX_SLOTS;
	ret = send_fbe_req(&req, &max_slots);
	if (ret || max_slots < 0) {
		pr_err("send_fbe_req failed with ret = %d, max_slots = %d\n",
				ret, max_slots);
		return -ECOMM;
	}

	*min_slot_index = 0;
	*total_num_slots = (uint32_t) max_slots;

	return 0;
}
