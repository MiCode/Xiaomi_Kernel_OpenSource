// SPDX-License-Identifier: GPL-2.0-only
/*
 * Crypto virtual library for storage encryption.
 *
 * Copyright (c) 2021, Linux Foundation. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/habmm.h>
#include <linux/crypto_qti_virt.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/completion.h>

/**********************************/
/** global definitions		 **/
/**********************************/

#define RESERVE_SIZE           (36*sizeof(uint16_t))
/* This macro is aligned to actual definition present
 * in bio crypt context header file.
 */
#define BLK_CRYPTO_MAX_WRAPPED_KEY_SIZE	128
#define HAB_TIMEOUT_MS	(3000)
/* FBE request command ids */
#define	FBE_GET_MAX_SLOTS         (7)
#define	FBE_SET_KEY_V2            (8)
#define	FBE_CLEAR_KEY_V2          (9)

struct fbe_request_v2_t {
	uint8_t reserve[RESERVE_SIZE];//for compatibility
	uint32_t cmd;
	uint8_t  key[BLK_CRYPTO_MAX_WRAPPED_KEY_SIZE];
	uint32_t key_size;
	uint32_t virt_slot;
};

struct fbe_req_args {
	struct fbe_request_v2_t req;
	int32_t status;
	int32_t ret;
};

static struct completion send_fbe_req_done;

static int32_t send_fbe_req_hab(void *arg)
{
	int ret = 0;
	uint32_t status_size;
	uint32_t handle;
	struct fbe_req_args *req_args = (struct fbe_req_args *)arg;

	do {
		if (!req_args) {
			pr_err("%s Null input\n", __func__);
			ret = -EINVAL;
			break;
		}

		ret = habmm_socket_open(&handle, MM_FDE_1, 0, 0);
		if (ret) {
			pr_err("habmm_socket_open failed with ret = %d\n", ret);
			break;
		}

		ret = habmm_socket_send(handle, &req_args->req, sizeof(struct fbe_request_v2_t), 0);
		if (ret) {
			pr_err("habmm_socket_send failed, ret= 0x%x\n", ret);
			break;
		}

		do {
			status_size = sizeof(int32_t);
			ret = habmm_socket_recv(handle, &req_args->status, &status_size, 0,
					HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
		} while (-EINTR == ret);

		if (ret) {
			pr_err("habmm_socket_recv failed, ret= 0x%x\n", ret);
			break;
		}

		if (status_size != sizeof(int32_t)) {
			pr_err("habmm_socket_recv expected size: %lu, actual=%u\n",
					sizeof(int32_t),
					status_size);
			ret = -E2BIG;
			break;
		}

		ret = habmm_socket_close(handle);
		if (ret) {
			pr_err("habmm_socket_close failed with ret = %d\n", ret);
			break;
		}
	} while (0);

	if (req_args)
		req_args->ret = ret;

	complete(&send_fbe_req_done);

	return 0;
}

static void send_fbe_req(struct fbe_req_args *arg)
{
	struct task_struct *thread;

	init_completion(&send_fbe_req_done);
	arg->status  = 0;

	thread = kthread_run(send_fbe_req_hab, arg, "send_fbe_req");
	if (IS_ERR(thread)) {
		arg->ret = -1;
		return;
	}

	if (wait_for_completion_interruptible_timeout(
		&send_fbe_req_done, msecs_to_jiffies(HAB_TIMEOUT_MS)) <= 0) {
		pr_err("%s: timeout hit\n", __func__);
		kthread_stop(thread);
		arg->ret = -ETIME;
		return;
	}
}

int crypto_qti_virt_ice_get_info(uint32_t *total_num_slots)
{
	struct fbe_req_args arg;

	if (!total_num_slots) {
		pr_err("%s Null input\n", __func__);
		return -EINVAL;
	}

	arg.req.cmd = FBE_GET_MAX_SLOTS;
	send_fbe_req(&arg);

	if (arg.ret || arg.status < 0) {
		pr_err("send_fbe_req_v2 failed with ret = %d, max_slots = %d\n",
		       arg.ret, arg.status);
		return -ECOMM;
	}

	*total_num_slots = (uint32_t) arg.status;

	return 0;
}

int crypto_qti_virt_program_key(const struct blk_crypto_key *key,
						unsigned int slot)
{
	struct fbe_req_args arg;

	if (!key)
		return -EINVAL;

	arg.req.cmd = FBE_SET_KEY_V2;
	arg.req.virt_slot = slot;
	arg.req.key_size = key->size;
	memcpy(&(arg.req.key[0]), key->raw, key->size);

	send_fbe_req(&arg);

	if (arg.ret || arg.status) {
		pr_err("send_fbe_req_v2 failed with ret = %d, status = %d\n",
		       arg.ret, arg.status);
		return -ECOMM;
	}

	return 0;
}
EXPORT_SYMBOL(crypto_qti_virt_program_key);

int crypto_qti_virt_invalidate_key(unsigned int slot)
{
	struct fbe_req_args arg;

	arg.req.cmd = FBE_CLEAR_KEY_V2;
	arg.req.virt_slot = slot;

	send_fbe_req(&arg);

	if (arg.ret || arg.status) {
		pr_err("send_fbe_req_v2 failed with ret = %d, status = %d\n",
		       arg.ret, arg.status);
		return -ECOMM;
	}

	return 0;
}
EXPORT_SYMBOL(crypto_qti_virt_invalidate_key);

int crypto_qti_virt_derive_raw_secret_platform(const u8 *wrapped_key,
					       unsigned int wrapped_key_size,
					       u8 *secret,
					       unsigned int secret_size)
{
	memcpy(secret, wrapped_key, secret_size);
	return 0;
}
EXPORT_SYMBOL(crypto_qti_virt_derive_raw_secret_platform);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Crypto Virtual library for storage encryption");

