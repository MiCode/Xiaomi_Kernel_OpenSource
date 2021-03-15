/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <dt-bindings/soc/qcom,ipcc.h>

#ifdef VERIFY_PRINT_ERROR
#define VERIFY_EPRINTF(format, ...) pr_err(format, ##__VA_ARGS__)
#else
#define VERIFY_EPRINTF(format, args) ((void)0)
#endif

#ifndef VERIFY_PRINT_INFO
#define VERIFY_IPRINTF(args) ((void)0)
#endif

#ifndef VERIFY
#define __STR__(x) #x ":"
#define __TOSTR__(x) __STR__(x)
#define __FILE_LINE__ __FILE__ ":" __TOSTR__(__LINE__)
#define __IPC_LITE_LINE__ "adsprpc:" __TOSTR__(__LINE__)

#define VERIFY(err, val)                                                     \
	do {                                                                     \
		VERIFY_IPRINTF(__FILE_LINE__"info: calling: " #val "\n");            \
		if ((val) == 0) {                                                    \
			(err) = (err) == 0 ? -1 : (err);                                 \
			VERIFY_EPRINTF(__IPC_LITE_LINE__" error: %d: "#val "\n", (err)); \
		} else {                                                             \
			VERIFY_IPRINTF(__FILE_LINE__"info: passed: " #val "\n");         \
		}                                                                    \
	} while (0)
#endif

typedef int (*IPCC_Client)(uint32_t client_id,  uint64_t data,  void *priv);

/**
 * struct ipc_lite_client - depiction of each ipc_lite client
 *
 * @callback  : function pointer to client callback function
 * @priv_data : pointer to client's private data
 * @recv_lock : guard for callback
 */
struct ipc_lite_client {
	IPCC_Client callback;
	void *priv_data;
	spinlock_t recv_lock;
};

/**
 * struct ipc_lite_channel - depiction of each ipc_lite channel
 *
 * @rpdev       : rpdev reference
 * @rpmsg_mutex : guard for rpdev
 */
struct ipc_lite_channel {
	struct rpmsg_device *rpdev;
	struct mutex rpmsg_mutex;
};

/**
 * struct qcom_ipc_lite - ipc_lite context for each subsystem
 *
 * @channel : pointer to ipc_lite_channel
 * @client  : pointer to ipc_lite_client
 */
struct qcom_ipc_lite {
	struct ipc_lite_channel *channel;
	struct ipc_lite_client *client;
};

/**
 * ipc_lite_msg_send() - Sends message to remote client.
 *
 * @client_id  : Identifier for remote client or subsystem.
 * @data       : 64 bit message value.
 * @allow_wait : Whether or not to allow waiting for transport to be
 *               available.
 *
 * @return Zero on successful registration, negative on failure.
 */
int ipc_lite_msg_send(int32_t client_id, uint64_t data, int allow_wait);

/**
 * ipc_lite_register_client() - Registers client callback with framework.
 *
 * @cb_func_ptr : Client callback function to be called on message receive.
 * @priv        : Private data required by client for handling callback.
 *
 * @return Zero on successful registration, negative on failure.
 */
int ipc_lite_register_client(IPCC_Client cb_func_ptr, void *priv);
