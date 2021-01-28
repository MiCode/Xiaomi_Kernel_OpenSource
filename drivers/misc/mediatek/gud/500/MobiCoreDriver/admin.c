// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2019 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/sched/signal.h>
#include <linux/freezer.h>

#include "public/mc_user.h"
#include "public/mc_admin.h"

#include "main.h"
#include "mci/mcloadformat.h"		/* struct mc_blob_len_info */
#include "mmu.h"			/* For load_check and load_token */
#include "mcp.h"
#include "nq.h"
#include "protocol.h"
#include "client.h"
#include "admin.h"

static struct {
	struct mutex admin_tgid_mutex;  /* Lock for admin_tgid below */
	pid_t admin_tgid;
	int (*tee_start_cb)(void);
	void (*tee_stop_cb)(void);
	int last_tee_ret;
	struct notifier_block tee_stop_notifier;
	/* Interface not initialised means no local registry, front-end case */
	bool is_initialised;
} l_ctx;

static struct mc_admin_driver_request {
	/* Global */
	struct mutex mutex;		/* Protects access to this struct */
	struct mutex states_mutex;	/* Protect access to the states */
	enum client_state {
		IDLE,
		REQUEST_SENT,
		BUFFERS_READY,
	} client_state;
	enum server_state {
		NOT_CONNECTED,		/* Device not open */
		READY,			/* Waiting for requests */
		REQUEST_RECEIVED,	/* Got a request, is working */
		RESPONSE_SENT,		/* Has sent a response header */
		DATA_SENT,		/* Blocked until data is consumed */
	} server_state;
	/* Request */
	u32 request_id;
	struct mc_admin_request request;
	struct completion client_complete;
	/* Response */
	struct mc_admin_response response;
	struct completion server_complete;
	void *buffer;			/* Reception buffer (pre-allocated) */
	size_t size;			/* Size of the reception buffer */
	bool lock_channel_during_freeze;/* Is freezing ongoing ? */
} g_request;

/* The mutex around the channel communication has to be wrapped in order
 * to handle this use case :
 * client 1 calls request_send()
 *	    wait on wait_for_completion_interruptible (with channel mutex taken)
 * client 2 calls request_send()
 *	    waits on mutex_lock(channel mutex)
 * kernel starts freezing tasks (suspend or reboot ongoing)
 * if we do nothing, then the freezing will be aborted because client 1
 * and 2 have to enter the refrigerator by themselves.
 * Note : mutex cannot be held during freezing, so client 1 has release it
 * => step 1 : client 1 sets a bool that says that the channel is still in use
 * => step 2 : client 1 release the lock and enter the refrigerator
 * => now any client trying to use the channel will face the bool preventing
 * to use the channel. They also have to enter the refrigerator.
 *
 * These 3 functions handle this
 */
static void check_freezing_ongoing(void)
{
	/* We don't want to let the channel be used. Let everyone know
	 * that we're using it
	 */
	g_request.lock_channel_during_freeze = 1;
	/* Now we can safely release the lock */
	mutex_unlock(&g_request.mutex);
	/* Let's try to freeze */
	try_to_freeze();
	/* Either freezing happened or was canceled.
	 * In both cases, reclaim the lock
	 */
	mutex_lock(&g_request.mutex);
	g_request.lock_channel_during_freeze = 0;
}

static void channel_lock(void)
{
	while (1) {
		mutex_lock(&g_request.mutex);
		/* We took the lock, but is there any freezing ongoing? */
		if (g_request.lock_channel_during_freeze == 0)
			break;

		/* yes, so let's freeze */
		mutex_unlock(&g_request.mutex);
		try_to_freeze();
		/* Either freezing succeeded or was canceled.
		 * In both case, try again to get the lock.
		 * Give some CPU time to let the contender
		 * finish his channel operation
		 */
		msleep(500);
	};
}

static void channel_unlock(void)
{
	mutex_unlock(&g_request.mutex);
}

static inline void reinit_completion_local(struct completion *x)
{
	reinit_completion(x);
}

static struct tee_object *tee_object_alloc(size_t length)
{
	struct tee_object *obj;
	size_t size = sizeof(*obj) + length;

	/* Check size for overflow */
	if (size < length || size > OBJECT_LENGTH_MAX) {
		mc_dev_err(-ENOMEM, "cannot allocate object of size %zu",
			   length);
		return NULL;
	}

	/* Allocate memory */
	obj = vzalloc(size);
	if (!obj)
		return NULL;

	obj->length = (u32)length;
	return obj;
}

void tee_object_free(struct tee_object *obj)
{
	vfree(obj);
}

static inline void client_state_change(enum client_state state)
{
	mutex_lock(&g_request.states_mutex);
	mc_dev_devel("client state changes from %d to %d",
		     g_request.client_state, state);
	g_request.client_state = state;
	mutex_unlock(&g_request.states_mutex);
}

static inline bool client_state_is(enum client_state state)
{
	bool is;

	mutex_lock(&g_request.states_mutex);
	is = g_request.client_state == state;
	mutex_unlock(&g_request.states_mutex);
	return is;
}

static inline void server_state_change(enum server_state state)
{
	mutex_lock(&g_request.states_mutex);
	mc_dev_devel("server state changes from %d to %d",
		     g_request.server_state, state);
	g_request.server_state = state;
	mutex_unlock(&g_request.states_mutex);
}

static inline bool server_state_is(enum server_state state)
{
	bool is;

	mutex_lock(&g_request.states_mutex);
	is = g_request.server_state == state;
	mutex_unlock(&g_request.states_mutex);
	return is;
}

static void request_cancel(void);

static int request_send(u32 command, const struct mc_uuid_t *uuid, bool is_gp)
{
	int counter_ms = 0;
	int wait_tens = 0;
	int ret = 0;

	/* Prepare request */
	mutex_lock(&g_request.states_mutex);
	/* Wait a little for daemon to connect */
	while (g_request.server_state == NOT_CONNECTED) {
		mutex_unlock(&g_request.states_mutex);
		if (signal_pending(current))
			return -ERESTARTSYS;

		if (counter_ms == 10000) { /* print every 10s */
			wait_tens++;
			mc_dev_info("daemon not connected after %d0s, waiting",
				    wait_tens);
			counter_ms = 0;
		}

		msleep(20);
		counter_ms += 20;
		mutex_lock(&g_request.states_mutex);
	}

	WARN_ON(g_request.client_state != IDLE);
	if (g_request.server_state != READY) {
		mutex_unlock(&g_request.states_mutex);
		if (g_request.server_state != NOT_CONNECTED) {
			ret = -EPROTO;
			mc_dev_err(ret, "invalid daemon state %d",
				   g_request.server_state);
			goto end;
		} else {
			ret = -EHOSTUNREACH;
			mc_dev_err(ret, "daemon not connected");
			goto end;
		}
	}

	memset(&g_request.request, 0, sizeof(g_request.request));
	memset(&g_request.response, 0, sizeof(g_request.response));
	/*
	 * Do not update the request ID until it is dealt with, in case the
	 * daemon arrives later.
	 */
	g_request.request.request_id = g_request.request_id;
	g_request.request.command = command;
	if (uuid)
		memcpy(&g_request.request.uuid, uuid, sizeof(*uuid));
	else
		memset(&g_request.request.uuid, 0, sizeof(*uuid));

	g_request.request.is_gp = is_gp;
	g_request.client_state = REQUEST_SENT;
	mutex_unlock(&g_request.states_mutex);

	/* Send request */
	complete(&g_request.client_complete);
	mc_dev_devel("request sent");

	/* Wait for header */
	do {
		ret = wait_for_completion_interruptible(
						&g_request.server_complete);
		if (!ret)
			break;
		/* we may have to freeze now */
		check_freezing_ongoing();
		/* freezing happened or was canceled,
		 * let's sleep and try again
		 */
		msleep(500);
	} while (1);
	mc_dev_devel("response received");

	/* Server should be waiting with some data for us */
	mutex_lock(&g_request.states_mutex);
	switch (g_request.server_state) {
	case NOT_CONNECTED:
		/* Daemon gone */
		ret = -EPIPE;
		mc_dev_devel("daemon disconnected");
		break;
	case READY:
		/* No data to come, likely an error */
		ret = -g_request.response.error_no;
		mc_dev_devel("daemon ret=%d", ret);
		break;
	case RESPONSE_SENT:
	case DATA_SENT:
		/* Normal case, data to come */
		ret = 0;
		break;
	case REQUEST_RECEIVED:
		/* Should not happen as complete means the state changed */
		ret = -EPIPE;
		mc_dev_err(ret, "daemon is in a bad state: %d",
			   g_request.server_state);
		break;
	}

	mutex_unlock(&g_request.states_mutex);

end:
	if (ret)
		request_cancel();

	mc_dev_devel("ret=%d", ret);
	return ret;
}

static int request_receive(void *address, u32 size)
{
	/*
	 * At this point we have received the header and prepared some buffers
	 * to receive data that we know are coming from the server.
	 */

	/* Check server state */
	bool server_ok;

	mutex_lock(&g_request.states_mutex);
	server_ok = (g_request.server_state == RESPONSE_SENT) ||
		    (g_request.server_state == DATA_SENT);
	mutex_unlock(&g_request.states_mutex);
	if (!server_ok) {
		int ret = -EPIPE;

		mc_dev_err(ret, "expected server state %d or %d, not %d",
			   RESPONSE_SENT, DATA_SENT, g_request.server_state);
		request_cancel();
		return ret;
	}

	/* Setup reception buffer */
	g_request.buffer = address;
	g_request.size = size;
	client_state_change(BUFFERS_READY);

	/* Unlock write of data */
	complete(&g_request.client_complete);

	/* Wait for data */
	do {
		int ret = 0;

		ret = wait_for_completion_interruptible(
					     &g_request.server_complete);
		if (!ret)
			break;
		/* We may have to freeze now */
		check_freezing_ongoing();
		/* freezing happened or was canceled,
		 * let's sleep and try again
		 */
		msleep(500);
	} while (1);

	/* Reset reception buffer */
	g_request.buffer = NULL;
	g_request.size = 0;

	/* Return to idle state */
	client_state_change(IDLE);
	return 0;
}

/* Must be called instead of request_receive() to cancel a pending request */
static void request_cancel(void)
{
	/* Unlock write of data */
	mutex_lock(&g_request.states_mutex);
	if (g_request.server_state == DATA_SENT)
		complete(&g_request.client_complete);

	/* Return to idle state */
	g_request.client_state = IDLE;
	mutex_unlock(&g_request.states_mutex);
}

struct tee_object *tee_object_get(const struct mc_uuid_t *uuid, bool is_gp)
{
	struct tee_object *obj = NULL;
	int ret = 0;

	if (!l_ctx.is_initialised)
		return ERR_PTR(-ENOPROTOOPT);

	/* Lock communication channel */
	channel_lock();

	/* Send request and wait for header */
	ret = request_send(MC_DRV_GET_TRUSTLET, uuid, is_gp);
	if (ret)
		goto end;

	/* Allocate memory */
	obj = tee_object_alloc(g_request.response.length);
	if (!obj) {
		request_cancel();
		ret = -ENOMEM;
		goto end;
	}

	/* Get data */
	ret = request_receive(&obj->data, obj->length);

end:
	channel_unlock();
	if (ret)
		return ERR_PTR(ret);

	return obj;
}

static void mc_admin_sendcrashdump(void)
{
	int ret = 0;

	/* Lock communication channel */
	channel_lock();

	/* Send request and wait for header */
	ret = request_send(MC_DRV_SIGNAL_CRASH, NULL, false);
	if (ret)
		goto end;

	/* Done */
	request_cancel();

end:
	channel_unlock();
}

static int tee_stop_notifier_fn(struct notifier_block *nb, unsigned long event,
				void *data)
{
	mc_admin_sendcrashdump();
	l_ctx.last_tee_ret = -EHOSTUNREACH;
	return 0;
}

struct tee_object *tee_object_copy(uintptr_t address, size_t length)
{
	struct tee_object *obj;

	/* Allocate memory */
	obj = tee_object_alloc(length);
	if (!obj)
		return ERR_PTR(-ENOMEM);

	/* Copy trustlet */
	memcpy(obj->data, (void *)address, length);
	return obj;
}

struct tee_object *tee_object_read(uintptr_t address, size_t length)
{
	char __user *addr = (char __user *)address;
	struct tee_object *obj;
	u8 *data;
	struct mclf_header_v2 thdr;
	int ret;

	/* Check length */
	if (length < sizeof(thdr)) {
		ret = -EFAULT;
		mc_dev_err(ret, "buffer shorter than header size");
		return ERR_PTR(ret);
	}

	/* Read header */
	if (copy_from_user(&thdr, addr, sizeof(thdr))) {
		ret = -EFAULT;
		mc_dev_err(ret, "header: copy_from_user failed");
		return ERR_PTR(ret);
	}

	/* Check header */
	if ((thdr.intro.magic != MC_SERVICE_HEADER_MAGIC_BE) &&
	    (thdr.intro.magic != MC_SERVICE_HEADER_MAGIC_LE)) {
		ret = -EINVAL;
		mc_dev_err(ret, "header: invalid magic");
		return ERR_PTR(ret);
	}

	/* Allocate memory */
	obj = tee_object_alloc(length);
	if (!obj)
		return ERR_PTR(-ENOMEM);

	/* Copy header */
	data = (u8 *)&obj->data;
	memcpy(data, &thdr, sizeof(thdr));
	/* Copy the rest of the data */
	data += sizeof(thdr);
	if (copy_from_user(data, &addr[sizeof(thdr)], length - sizeof(thdr))) {
		ret = -EFAULT;
		mc_dev_err(ret, "data: copy_from_user failed");
		vfree(obj);
		return ERR_PTR(ret);
	}

	return obj;
}

struct tee_object *tee_object_select(const struct mc_uuid_t *uuid)
{
	struct tee_object *obj;
	struct mclf_header_v2 *thdr;

	obj = tee_object_alloc(sizeof(*thdr));
	if (!obj)
		return ERR_PTR(-ENOMEM);

	thdr = (struct mclf_header_v2 *)&obj->data;
	memcpy(&thdr->uuid, uuid, sizeof(thdr->uuid));
	return obj;
}

static inline int load_driver(struct tee_client *client,
			      struct mc_admin_load_info *load_info)
{
	struct mcp_open_info info = {
		.va = load_info->address,
		.len = load_info->length,
		.uuid = &load_info->uuid,
		.tci_len = PAGE_SIZE,
		.user = true,
	};

	u32 session_id = 0;
	int ret;

	if (info.va)
		info.type = TEE_MC_DRIVER;
	else
		info.type = TEE_MC_DRIVER_UUID;

	/* Create DCI in case it's needed */
	ret = client_cbuf_create(client, info.tci_len, &info.tci_va, NULL);
	if (ret)
		return ret;

	/* Open session */
	ret = client_mc_open_common(client, &info, &session_id);
	if (!ret)
		mc_dev_devel("driver loaded with session id %x", session_id);

	/*
	 * Always 'free' the buffer (will remain as long as used), never freed
	 * otherwise
	 */
	client_cbuf_free(client, info.tci_va);

	return ret;
}

static inline int load_token(struct mc_admin_load_info *token)
{
	struct tee_mmu *mmu;
	struct mcp_buffer_map map;
	struct mc_ioctl_buffer buf;
	int ret;

	buf.va = (uintptr_t)token->address;
	buf.len = token->length;
	buf.flags = MC_IO_MAP_INPUT;
	mmu = tee_mmu_create(current->mm, &buf);
	if (IS_ERR(mmu))
		return PTR_ERR(mmu);

	tee_mmu_buffer(mmu, &map);
	ret = mcp_load_token(token->address, &map);
	tee_mmu_put(mmu);
	return ret;
}

static inline int load_key_so(struct mc_admin_load_info *key_so)
{
	struct tee_mmu *mmu;
	struct mcp_buffer_map map;
	struct mc_ioctl_buffer buf;
	int ret;

	buf.va = (uintptr_t)key_so->address;
	buf.len = key_so->length;
	buf.flags = MC_IO_MAP_INPUT;
	mmu = tee_mmu_create(current->mm, &buf);
	if (IS_ERR(mmu))
		return PTR_ERR(mmu);

	tee_mmu_buffer(mmu, &map);
	ret = mcp_load_key_so(key_so->address, &map);
	tee_mmu_put(mmu);
	return ret;
}

static ssize_t admin_write(struct file *file, const char __user *user,
			   size_t len, loff_t *off)
{
	int ret;

	/* No offset allowed */
	if (*off) {
		ret = -ECOMM;
		mc_dev_err(ret, "offset not supported");
		g_request.response.error_no = EPIPE;
		goto err;
	}

	if (server_state_is(REQUEST_RECEIVED)) {
		/* Check client state */
		if (!client_state_is(REQUEST_SENT)) {
			ret = -EPIPE;
			mc_dev_err(ret, "expected client state %d, not %d",
				   REQUEST_SENT, g_request.client_state);
			g_request.response.error_no = EPIPE;
			goto err;
		}

		/* Receive response header */
		if (copy_from_user(&g_request.response, user,
				   sizeof(g_request.response))) {
			ret = -ECOMM;
			mc_dev_err(ret, "failed to get response from daemon");
			g_request.response.error_no = EPIPE;
			goto err;
		}

		/* Check request ID */
		if (g_request.request.request_id !=
						g_request.response.request_id) {
			ret = -EBADE;
			mc_dev_err(ret, "expected id %d, not %d",
				   g_request.request.request_id,
				   g_request.response.request_id);
			g_request.response.error_no = EPIPE;
			goto err;
		}

		/* Response header is acceptable */
		ret = sizeof(g_request.response);
		if (g_request.response.length)
			server_state_change(RESPONSE_SENT);
		else
			server_state_change(READY);

		goto end;
	} else if (server_state_is(RESPONSE_SENT)) {
		/* Server is waiting */
		server_state_change(DATA_SENT);

		/* Get data */
		ret = wait_for_completion_interruptible(
						&g_request.client_complete);

		/* Server received a signal, let see if it tries again */
		if (ret) {
			server_state_change(RESPONSE_SENT);
			return ret;
		}

		/* Check client state */
		if (!client_state_is(BUFFERS_READY)) {
			ret = -EPIPE;
			mc_dev_err(ret, "expected client state %d, not %d",
				   BUFFERS_READY, g_request.client_state);
			g_request.response.error_no = EPIPE;
			goto err;
		}

		/* We do not deal with several writes */
		if (len != g_request.size)
			len = g_request.size;

		ret = copy_from_user(g_request.buffer, user, len);
		if (ret) {
			ret = -ECOMM;
			mc_dev_err(ret, "failed to get data from daemon");
			g_request.response.error_no = EPIPE;
			goto err;
		}

		ret = len;
		server_state_change(READY);
		goto end;
	} else {
		ret = -ECOMM;
		goto err;
	}

err:
	server_state_change(READY);
end:
	complete(&g_request.server_complete);
	return ret;
}

static ssize_t admin_read(struct file *file, char __user *user, size_t len,
			  loff_t *off)
{
	/* No offset allowed */
	if (*off) {
		int ret = -ECOMM;

		mc_dev_err(ret, "offset not supported");
		return ret;
	}

	return nq_get_stop_message(user, len);
}

static long admin_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	int ret = -EINVAL;

	mc_dev_devel("%u from %s", _IOC_NR(cmd), current->comm);

	switch (cmd) {
	case MC_ADMIN_IO_GET_DRIVER_REQUEST: {
		/* Update TGID as it may change (when becoming a daemon) */
		if (l_ctx.admin_tgid != current->tgid) {
			l_ctx.admin_tgid = current->tgid;
			mc_dev_info("daemon PID changed to %d",
				    l_ctx.admin_tgid);
		}

		/* Block until a request is available */
		server_state_change(READY);
		ret = wait_for_completion_interruptible(
						&g_request.client_complete);
		if (ret)
			/* Interrupted by signal */
			break;

		/* Check client state */
		if (!client_state_is(REQUEST_SENT)) {
			ret = -EPIPE;
			mc_dev_err(ret, "expected client state %d, not %d",
				   REQUEST_SENT, g_request.client_state);
			g_request.response.error_no = EPIPE;
			complete(&g_request.server_complete);
			break;
		}

		/* Send request (the driver request mutex is held) */
		ret = copy_to_user(uarg, &g_request.request,
				   sizeof(g_request.request));
		if (ret) {
			server_state_change(READY);
			complete(&g_request.server_complete);
			ret = -EPROTO;
			break;
		}

		/* Now that the daemon got it, update the request ID */
		g_request.request_id++;

		server_state_change(REQUEST_RECEIVED);
		break;
	}
	case MC_ADMIN_IO_GET_INFO: {
		struct mc_admin_driver_info info;

		info.drv_version = MC_VERSION(MCDRVMODULEAPI_VERSION_MAJOR,
					      MCDRVMODULEAPI_VERSION_MINOR);
		info.initial_cmd_id = g_request.request_id;
		ret = copy_to_user(uarg, &info, sizeof(info));
		break;
	}
	case MC_ADMIN_IO_LOAD_DRIVER: {
		struct tee_client *client = file->private_data;
		struct mc_admin_load_info info;

		if (copy_from_user(&info, uarg, sizeof(info))) {
			ret = -EFAULT;
			break;
		}

		/* Make sure we have a local client */
		if (!client) {
			client = client_create(true, protocol_vm_id());
			/* Store client for future use/close */
			file->private_data = client;
		}

		if (!client) {
			ret = -ENOMEM;
			break;
		}

		ret = load_driver(client, &info);
		break;
	}
	case MC_ADMIN_IO_LOAD_TOKEN: {
		struct mc_admin_load_info info;

		if (copy_from_user(&info, uarg, sizeof(info))) {
			ret = -EFAULT;
			break;
		}

		ret = load_token(&info);
		break;
	}
	case MC_ADMIN_IO_LOAD_KEY_SO: {
		struct mc_admin_load_info info;

		if (copy_from_user(&info, uarg, sizeof(info))) {
			ret = -EFAULT;
			break;
		}

		ret = load_key_so(&info);
		break;
	}
	default:
		ret = -ENOIOCTLCMD;
	}

	return ret;
}

/*
 * mc_fd_release() - This function will be called from user space as close(...)
 * The client data are freed and the associated memory pages are unreserved.
 *
 * @inode
 * @file
 *
 * Returns 0
 */
static int admin_release(struct inode *inode, struct file *file)
{
	/* Close client if any */
	if (file->private_data)
		client_close((struct tee_client *)file->private_data);

	/* Requests from driver to daemon */
	mutex_lock(&g_request.states_mutex);
	mc_dev_devel("server state changes from %d to %d",
		     g_request.server_state, NOT_CONNECTED);
	g_request.server_state = NOT_CONNECTED;
	/* A non-zero command indicates that a thread is waiting */
	if (g_request.client_state != IDLE) {
		g_request.response.error_no = ESHUTDOWN;
		complete(&g_request.server_complete);
	}
	mutex_unlock(&g_request.states_mutex);
	mc_dev_info("daemon connection closed, TGID %d", l_ctx.admin_tgid);
	l_ctx.admin_tgid = 0;

	/*
	 * ret is quite irrelevant here as most apps don't care about the
	 * return value from close() and it's quite difficult to recover
	 */
	return 0;
}

static int admin_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	/* Only one connection allowed to admin interface */
	mutex_lock(&l_ctx.admin_tgid_mutex);
	if (l_ctx.admin_tgid) {
		ret = -EBUSY;
		mc_dev_err(ret, "daemon connection already open, PID %d",
			   l_ctx.admin_tgid);
	} else {
		l_ctx.admin_tgid = current->tgid;
	}
	mutex_unlock(&l_ctx.admin_tgid_mutex);
	if (ret)
		return ret;

	/* Setup the usual variables */
	mc_dev_devel("accept %s as daemon", current->comm);

	/*
	 * daemon is connected so now we can safely suppose
	 * the secure world is loaded too
	 */
	if (l_ctx.last_tee_ret == TEE_START_NOT_TRIGGERED)
		l_ctx.last_tee_ret = l_ctx.tee_start_cb();

	/* Failed to start the TEE, either now or before */
	if (l_ctx.last_tee_ret) {
		mutex_lock(&l_ctx.admin_tgid_mutex);
		l_ctx.admin_tgid = 0;
		mutex_unlock(&l_ctx.admin_tgid_mutex);
		return l_ctx.last_tee_ret;
	}

	reinit_completion_local(&g_request.client_complete);
	reinit_completion_local(&g_request.server_complete);
	/* Requests from driver to daemon */
	mc_dev_info("daemon connection open, TGID %d", l_ctx.admin_tgid);
	return 0;
}

/* function table structure of this device driver. */
static const struct file_operations mc_admin_fops = {
	.owner = THIS_MODULE,
	.open = admin_open,
	.release = admin_release,
	.unlocked_ioctl = admin_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = admin_ioctl,
#endif
	.write = admin_write,
	.read = admin_read,
};

int mc_admin_init(struct cdev *cdev, int (*tee_start_cb)(void),
		  void (*tee_stop_cb)(void))
{
	mutex_init(&l_ctx.admin_tgid_mutex);
	/* Requests from driver to daemon */
	mutex_init(&g_request.mutex);
	mutex_init(&g_request.states_mutex);
	g_request.request_id = 42;
	init_completion(&g_request.client_complete);
	init_completion(&g_request.server_complete);
	l_ctx.tee_stop_notifier.notifier_call = tee_stop_notifier_fn;
	nq_register_tee_stop_notifier(&l_ctx.tee_stop_notifier);
	/* Create char device */
	cdev_init(cdev, &mc_admin_fops);
	/* Register the call back for starting the secure world */
	l_ctx.tee_start_cb = tee_start_cb;
	l_ctx.tee_stop_cb = tee_stop_cb;
	l_ctx.last_tee_ret = TEE_START_NOT_TRIGGERED;
	l_ctx.is_initialised = true;
	return 0;
}

void mc_admin_exit(void)
{
	nq_unregister_tee_stop_notifier(&l_ctx.tee_stop_notifier);
	if (!l_ctx.last_tee_ret)
		l_ctx.tee_stop_cb();
}
