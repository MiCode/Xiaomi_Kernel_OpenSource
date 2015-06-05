/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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

#include <asm/atomic.h>
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

#include "public/mc_linux.h"
#include "public/mc_admin.h"

#include "mci/mcloadformat.h"

#include "main.h"
#include "debug.h"
#include "mmu.h"	/* For load_check and load_token */
#include "mcp.h"
#include "client.h"
#include "api.h"
#include "admin.h"

/* We need 2 devices for admin and user interface*/
#define MC_DEV_MAX 2

static struct admin_ctx {
	struct device *dev;
	atomic_t daemon_counter;
	/* Define a MobiCore device structure for use with dev_debug() etc */
	struct device_driver mc_dev_name;
	dev_t mc_dev_admin;
	struct cdev mc_admin_cdev;
	int (*tee_start_cb)(void);
} g_admin_ctx;

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
	uint32_t request_id;
	struct mc_admin_request request;
	struct completion client_complete;
	/* Response */
	struct mc_admin_response response;
	struct completion server_complete;
	void *buffer;			/* Reception buffer (pre-allocated) */
	size_t size;			/* Size of the reception buffer */
} g_request;

static struct tbase_object *tbase_object_alloc(bool is_sp_trustlet,
					       size_t length)
{
	struct tbase_object *obj;
	size_t size = sizeof(*obj) + length;
	size_t header_length = 0;

	/* Determine required size */
	if (is_sp_trustlet) {
		/* Need space for lengths info and containers */
		header_length = sizeof(struct mc_blob_len_info);
		size += header_length + 3 * MAX_SO_CONT_SIZE;
	}

	/* Allocate memory */
	obj = vzalloc(size);
	if (!obj)
		return NULL;

	/* A non-zero header_length indicates that we have a SP trustlet */
	obj->header_length = header_length;
	obj->length = length;
	return obj;
}

void tbase_object_free(struct tbase_object *robj)
{
	vfree(robj);
}

static inline void client_state_change(enum client_state state)
{
	mutex_lock(&g_request.states_mutex);
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

static int request_send(uint32_t command, const struct mc_uuid_t *uuid,
			uint32_t is_gp, uint32_t spid)
{
	struct device *dev = g_admin_ctx.dev;
	int counter = 10;
	int ret;

	/* Prepare request */
	mutex_lock(&g_request.states_mutex);
	/* Wait a little for daemon to connect */
	while ((g_request.server_state == NOT_CONNECTED) && counter--) {
		mutex_unlock(&g_request.states_mutex);
		ssleep(1);
		mutex_lock(&g_request.states_mutex);
	}

	BUG_ON(g_request.client_state != IDLE);
	if (g_request.server_state != READY) {
		mutex_unlock(&g_request.states_mutex);
		if (g_request.server_state != NOT_CONNECTED) {
			/* TODO: can we recover? */
			dev_err(dev, "%s: invalid daemon state %d\n", __func__,
				g_request.server_state);
			ret = -EPROTO;
			goto end;
		} else {
			dev_err(dev, "%s: daemon not connected\n", __func__);
			ret = -ENOTCONN;
			goto end;
		}
	}

	memset(&g_request.request, 0, sizeof(g_request.request));
	memset(&g_request.response, 0, sizeof(g_request.response));
	g_request.request.request_id = g_request.request_id++;
	g_request.request.command = command;
	if (uuid)
		memcpy(&g_request.request.uuid, uuid, sizeof(*uuid));
	else
		memset(&g_request.request.uuid, 0, sizeof(*uuid));

	g_request.request.is_gp = is_gp;
	g_request.request.spid = spid;
	g_request.client_state = REQUEST_SENT;
	mutex_unlock(&g_request.states_mutex);

	/* Send request */
	complete(&g_request.client_complete);

	/* Wait for header (could be interruptible, but then needs more work) */
	wait_for_completion(&g_request.server_complete);

	/* Server should be waiting with some data for us */
	mutex_lock(&g_request.states_mutex);
	switch (g_request.server_state) {
	case NOT_CONNECTED:
		/* Daemon gone */
		ret = -EPIPE;
		break;
	case READY:
		/* No data to come, likely an error */
		ret = -g_request.response.error_no;
		break;
	case RESPONSE_SENT:
	case DATA_SENT:
		/* Normal case, data to come */
		ret = 0;
		break;
	default:
		/* Should not happen as complete means the state changed */
		dev_err(dev, "%s: daemon is in a bad state: %d\n", __func__,
			g_request.server_state);
		ret = -EPIPE;
		break;
	}

	mutex_unlock(&g_request.states_mutex);

end:
	if (ret)
		request_cancel();

	return ret;
}

static int request_receive(void *address, uint32_t size)
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
		/* TODO: can we recover? */
		request_cancel();
		return -EPIPE;
	}

	/* Setup reception buffer */
	g_request.buffer = address;
	g_request.size = size;
	client_state_change(BUFFERS_READY);

	/* Unlock write of data */
	complete(&g_request.client_complete);

	/* Wait for data (far too late to be interruptible) */
	wait_for_completion(&g_request.server_complete);

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

static int admin_get_root_container(void *address)
{
	struct device *dev = g_admin_ctx.dev;
	int ret = 0;

	/* Lock communication channel */
	mutex_lock(&g_request.mutex);

	/* Send request and wait for header */
	ret = request_send(MC_DRV_GET_ROOT_CONTAINER, 0, 0, 0);
	if (ret)
		goto end;

	/* Check length against max */
	if (g_request.response.length >= MAX_SO_CONT_SIZE) {
		request_cancel();
		dev_err(dev, "%s: response length exceeds maximum\n", __func__);
		ret = EREMOTEIO;
		goto end;
	}

	/* Get data */
	ret = request_receive(address, g_request.response.length);
	if (!ret)
		ret = g_request.response.length;

end:
	mutex_unlock(&g_request.mutex);
	return ret;
}

static int admin_get_sp_container(void *address, uint32_t spid)
{
	struct device *dev = g_admin_ctx.dev;
	int ret = 0;

	/* Lock communication channel */
	mutex_lock(&g_request.mutex);

	/* Send request and wait for header */
	ret = request_send(MC_DRV_GET_SP_CONTAINER, 0, 0, spid);
	if (ret)
		goto end;

	/* Check length against max */
	if (g_request.response.length >= MAX_SO_CONT_SIZE) {
		request_cancel();
		dev_err(dev, "%s: response length exceeds maximum\n", __func__);
		ret = EREMOTEIO;
		goto end;
	}

	/* Get data */
	ret = request_receive(address, g_request.response.length);
	if (!ret)
		ret = g_request.response.length;

end:
	mutex_unlock(&g_request.mutex);
	return ret;
}

static int admin_get_trustlet_container(void *address,
					const struct mc_uuid_t *uuid,
					uint32_t spid)
{
	struct device *dev = g_admin_ctx.dev;
	int ret = 0;

	/* Lock communication channel */
	mutex_lock(&g_request.mutex);

	/* Send request and wait for header */
	ret = request_send(MC_DRV_GET_TRUSTLET_CONTAINER, uuid, 0, spid);
	if (ret)
		goto end;

	/* Check length against max */
	if (g_request.response.length >= MAX_SO_CONT_SIZE) {
		request_cancel();
		dev_err(dev, "%s: response length exceeds maximum\n", __func__);
		ret = EREMOTEIO;
		goto end;
	}

	/* Get data */
	ret = request_receive(address, g_request.response.length);
	if (!ret)
		ret = g_request.response.length;

end:
	mutex_unlock(&g_request.mutex);
	return ret;
}

static struct tbase_object *admin_get_trustlet(const struct mc_uuid_t *uuid,
					       uint32_t is_gp, uint32_t *spid)
{
	struct tbase_object *obj = NULL;
	bool is_sp_tl;
	int ret = 0;

	/* Lock communication channel */
	mutex_lock(&g_request.mutex);

	/* Send request and wait for header */
	ret = request_send(MC_DRV_GET_TRUSTLET, uuid, is_gp, 0);
	if (ret)
		goto end;

	/* Allocate memory */
	is_sp_tl = g_request.response.service_type == SERVICE_TYPE_SP_TRUSTLET;
	obj = tbase_object_alloc(is_sp_tl, g_request.response.length);
	if (!obj) {
		request_cancel();
		ret = -ENOMEM;
		goto end;
	}

	/* Get data */
	ret = request_receive(&obj->data[obj->header_length], obj->length);
	*spid = g_request.response.spid;

end:
	mutex_unlock(&g_request.mutex);
	if (ret)
		return ERR_PTR(ret);

	return obj;
}

static void mc_admin_sendcrashdump(void)
{
	int ret = 0;

	/* Lock communication channel */
	mutex_lock(&g_request.mutex);

	/* Send request and wait for header */
	ret = request_send(MC_DRV_SIGNAL_CRASH, NULL, false, 0);
	if (ret)
		goto end;

	/* Done */
	request_cancel();

end:
	mutex_unlock(&g_request.mutex);
}

static int tbase_object_make(uint32_t spid, struct tbase_object *obj)
{
	struct mc_blob_len_info *l_info = (struct mc_blob_len_info *)obj->data;
	uint8_t *address = &obj->data[obj->header_length + obj->length];
	struct mclf_header_v2 *thdr;
	int ret;

	/* Get root container */
	ret = admin_get_root_container(address);
	if (ret < 0)
		goto err;

	l_info->root_size = ret;
	address += ret;

	/* Get SP container */
	ret = admin_get_sp_container(address, spid);
	if (ret < 0)
		goto err;

	l_info->sp_size = ret;
	address += ret;

	/* Get trustlet container */
	thdr = (struct mclf_header_v2 *)&obj->data[obj->header_length];
	ret = admin_get_trustlet_container(address, &thdr->uuid, spid);
	if (ret < 0)
		goto err;

	l_info->ta_size = ret;
	address += ret;

	/* Setup lengths information */
	l_info->magic = MC_TLBLOBLEN_MAGIC;
	obj->length += sizeof(*l_info);
	obj->length += l_info->root_size + l_info->sp_size + l_info->ta_size;
	ret = 0;

err:
	return ret;
}

struct tbase_object *tbase_object_read(uint32_t spid, uintptr_t address,
				       size_t length)
{
	struct device *dev = g_admin_ctx.dev;
	char __user *addr = (char __user *)address;
	struct tbase_object *obj;
	uint8_t *data;
	struct mclf_header_v2 thdr;
	int ret;

	/* Check length */
	if (length < sizeof(thdr)) {
		dev_err(dev, "%s: buffer shorter than header size\n", __func__);
		return ERR_PTR(-EFAULT);
	}

	/* Read header */
	if (copy_from_user(&thdr, addr, sizeof(thdr))) {
		dev_err(dev, "%s: header: copy_from_user failed\n", __func__);
		return ERR_PTR(-EFAULT);
	}

	/* Allocate memory */
	obj = tbase_object_alloc(thdr.service_type == SERVICE_TYPE_SP_TRUSTLET,
				 length);
	if (!obj)
		return ERR_PTR(-ENOMEM);

	/* Copy header */
	data = &obj->data[obj->header_length];
	memcpy(data, &thdr, sizeof(thdr));
	/* Copy the rest of the data */
	data += sizeof(thdr);
	if (copy_from_user(data, &addr[sizeof(thdr)], length - sizeof(thdr))) {
		dev_err(dev, "%s: data: copy_from_user failed\n", __func__);
		vfree(obj);
		return ERR_PTR(-EFAULT);
	}

	if (obj->header_length) {
		ret = tbase_object_make(spid, obj);
		if (ret) {
			vfree(obj);
			return ERR_PTR(ret);
		}
	}

	return obj;
}

struct tbase_object *tbase_object_select(const struct mc_uuid_t *uuid)
{
	struct tbase_object *obj;
	struct mclf_header_v2 *thdr;

	obj = tbase_object_alloc(false, sizeof(*thdr));
	if (!obj)
		return ERR_PTR(-ENOMEM);

	thdr = (struct mclf_header_v2 *)&obj->data[obj->header_length];
	memcpy(&thdr->uuid, uuid, sizeof(thdr->uuid));
	return obj;
}

struct tbase_object *tbase_object_get(const struct mc_uuid_t *uuid,
				      uint32_t is_gp_uuid)
{
	struct tbase_object *obj;
	uint32_t spid = 0;

	/* admin_get_trustlet creates the right object based on service type */
	obj = admin_get_trustlet(uuid, is_gp_uuid, &spid);
	if (IS_ERR(obj))
		return obj;

	/* SP trustlet: create full secure object with all containers */
	if (obj->header_length) {
		int ret;

		/* Do not return EINVAL in this case as SPID was not found */
		if (!spid) {
			vfree(obj);
			return ERR_PTR(-ENOENT);
		}

		ret = tbase_object_make(spid, obj);
		if (ret) {
			vfree(obj);
			return ERR_PTR(ret);
		}
	}

	return obj;
}

static inline int load_driver(struct tbase_client *client,
			      struct mc_admin_load_info *info)
{
	struct tbase_object *obj;
	struct mclf_header_v2 *thdr;
	struct mc_identity identity = {
		.login_type = TEEC_LOGIN_PUBLIC,
	};
	uintptr_t dci = 0;
	uint32_t dci_len = 0;
	uint32_t sid;
	int ret;

	obj = tbase_object_read(info->spid, info->address, info->length);
	if (IS_ERR(obj))
		return PTR_ERR(obj);

	thdr = (struct mclf_header_v2 *)&obj->data[obj->header_length];
	if (!(thdr->flags & MC_SERVICE_HEADER_FLAGS_NO_CONTROL_INTERFACE)) {
		/*
		 * The driver requires a DCI, although we won't be able to use
		 * it to communicate.
		 */
		dci_len = PAGE_SIZE;
		ret = api_malloc_cbuf(client, dci_len, &dci, NULL);
		if (ret)
			goto end;
	}

	/* Open session */
	ret = client_add_session(client, obj, dci, dci_len, &sid, false,
				 &identity);
	if (ret)
		api_free_cbuf(client, dci);
	else
		dev_dbg(g_admin_ctx.dev, "driver loaded with sid %x", sid);

end:
	vfree(obj);
	return ret;
}

static inline int load_token(struct mc_admin_load_info *token)
{
	struct tbase_mmu *mmu;
	struct mcp_buffer_map map;
	int ret;

	mmu = tbase_mmu_create(current, (void *)(uintptr_t)token->address,
			       token->length);
	if (IS_ERR(mmu))
		return PTR_ERR(mmu);

	tbase_mmu_buffer(mmu, &map);
	ret = mcp_load_token(token->address, &map);
	tbase_mmu_delete(mmu);
	return ret;
}

static inline int load_check(struct mc_admin_load_info *info)
{
	struct tbase_object *obj;
	struct tbase_mmu *mmu;
	struct mcp_buffer_map map;
	int ret;

	obj = tbase_object_read(info->spid, info->address, info->length);
	if (IS_ERR(obj))
		return PTR_ERR(obj);

	mmu = tbase_mmu_create(NULL, obj->data, obj->length);
	if (IS_ERR(mmu))
		return PTR_ERR(mmu);

	tbase_mmu_buffer(mmu, &map);
	ret = mcp_load_check(obj, &map);
	tbase_mmu_delete(mmu);
	return ret;
}

static ssize_t admin_write(struct file *file, const char __user *user,
			   size_t len, loff_t *off)
{
	int ret;

	/* No offset allowed [yet] */
	if (*off) {
		g_request.response.error_no = EPIPE;
		ret = -ECOMM;
		goto err;
	}

	if (server_state_is(REQUEST_RECEIVED)) {
		/* Check client state */
		if (!client_state_is(REQUEST_SENT)) {
			g_request.response.error_no = EPIPE;
			ret = -EPIPE;
			goto err;
		}

		/* Receive response header */
		if (copy_from_user(&g_request.response, user,
				   sizeof(g_request.response))) {
			g_request.response.error_no = EPIPE;
			ret = -ECOMM;
			goto err;
		}

		/* Check request ID */
		if (g_request.request.request_id !=
						g_request.response.request_id) {
			g_request.response.error_no = EPIPE;
			ret = -EBADE;
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
			g_request.response.error_no = EPIPE;
			ret = -EPIPE;
			goto err;
		}

		/* TODO deal with several writes */
		if (len != g_request.size)
			len = g_request.size;

		ret = copy_from_user(g_request.buffer, user, len);
		if (ret) {
			g_request.response.error_no = EPIPE;
			ret = -ECOMM;
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

static long admin_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	struct tbase_client *client = file->private_data;
	void __user *uarg = (void __user *)arg;
	int ret = -EINVAL;

	MCDRV_DBG("%u from %s", _IOC_NR(cmd), current->comm);

	if (WARN(!client, "No client data available"))
		return -EFAULT;

	switch (cmd) {
	case MC_ADMIN_IO_GET_DRIVER_REQUEST: {
		/* Block until a request is available */
		ret = wait_for_completion_interruptible(
						&g_request.client_complete);
		if (ret)
			/* Interrupted by signal */
			break;

		/* Check client state */
		if (!client_state_is(REQUEST_SENT)) {
			g_request.response.error_no = EPIPE;
			complete(&g_request.server_complete);
			ret = -EPIPE;
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
		struct mc_admin_load_info info;

		ret = copy_from_user(&info, uarg, sizeof(info));
		if (ret)
			ret = -EFAULT;
		else
			ret = load_driver(client, &info);

		break;
	}
	case MC_ADMIN_IO_LOAD_TOKEN: {
		struct mc_admin_load_info info;

		ret = copy_from_user(&info, uarg, sizeof(info));
		if (ret)
			ret = -EFAULT;
		else
			ret = load_token(&info);

		break;
	}
	case MC_ADMIN_IO_LOAD_CHECK: {
		struct mc_admin_load_info info;

		ret = copy_from_user(&info, uarg, sizeof(info));
		if (ret)
			ret = -EFAULT;
		else
			ret = load_check(&info);

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
	struct tbase_client *client = file->private_data;
	struct device *dev = g_admin_ctx.dev;

	if (!client)
		return -EPROTO;

	api_close_device(client);
	file->private_data = NULL;

	/* Requests from driver to daemon */
	mutex_lock(&g_request.states_mutex);
	dev_warn(dev, "%s: daemon disconnected\n", __func__);
	g_request.server_state = NOT_CONNECTED;
	/* A non-zero command indicates that a thread is waiting */
	if (g_request.client_state != IDLE) {
		g_request.response.error_no = ESHUTDOWN;
		complete(&g_request.server_complete);
	}

	mutex_unlock(&g_request.states_mutex);
	atomic_set(&g_admin_ctx.daemon_counter, 0);
	/*
	 * ret is quite irrelevant here as most apps don't care about the
	 * return value from close() and it's quite difficult to recover
	 */
	return 0;
}

static int admin_open(struct inode *inode, struct file *file)
{
	struct device *dev = g_admin_ctx.dev;
	struct tbase_client *client;
	int err;

	/*
	 * If the daemon is already set we can't allow anybody else to open
	 * the admin interface.
	 */
	if (atomic_cmpxchg(&g_admin_ctx.daemon_counter, 0, 1) != 0) {
		MCDRV_ERROR("Daemon is already connected");
		return -EPROTO;
	}

	/* Any value will do */
	g_request.request_id = 42;

	/* Setup the usual variables */
	MCDRV_DBG("accept %s as tbase daemon", current->comm);

	/*
	* daemon is connected so now we can safely suppose
	* the secure world is loaded too
	*/
	if (!IS_ERR_OR_NULL(g_admin_ctx.tee_start_cb))
		g_admin_ctx.tee_start_cb = ERR_PTR(g_admin_ctx.tee_start_cb());
	if (IS_ERR(g_admin_ctx.tee_start_cb)) {
		MCDRV_ERROR("Failed initializing the SW");
		err = PTR_ERR(g_admin_ctx.tee_start_cb);
		goto fail_connection;
}

	/* Create client */
	client = api_open_device(true);
	if (!client) {
		err = -ENOMEM;
		goto fail_connection;
	}

	/* Store client in user file */
	file->private_data = client;

	/* Requests from driver to daemon */
	server_state_change(READY);
	dev_info(dev, "%s: daemon connected\n", __func__);

	return 0;

fail_connection:
	atomic_set(&g_admin_ctx.daemon_counter, 0);
	return err;
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
};

int mc_admin_init(struct class *mc_device_class, dev_t *out_dev,
		  int (*tee_start_cb)(void))
{
	int err = 0;

	if (!out_dev || !mc_device_class)
		return -EINVAL;

	atomic_set(&g_admin_ctx.daemon_counter, 0);

	/* Requests from driver to daemon */
	mutex_init(&g_request.mutex);
	mutex_init(&g_request.states_mutex);
	init_completion(&g_request.client_complete);
	init_completion(&g_request.server_complete);
	mcp_register_crashhandler(mc_admin_sendcrashdump);

	/* Create char device */
	cdev_init(&g_admin_ctx.mc_admin_cdev, &mc_admin_fops);
	err = alloc_chrdev_region(&g_admin_ctx.mc_dev_admin, 0, MC_DEV_MAX,
				  "trustonic_tee");
	if (err < 0) {
		MCDRV_ERROR("failed to allocate char dev region");
		goto fail_alloc_chrdev_region;
	}

	err = cdev_add(&g_admin_ctx.mc_admin_cdev, g_admin_ctx.mc_dev_admin, 1);
	if (err) {
		MCDRV_ERROR("admin device register failed");
		goto fail_cdev_add;
	}

	g_admin_ctx.mc_admin_cdev.owner = THIS_MODULE;
	g_admin_ctx.dev = device_create(mc_device_class, NULL,
					g_admin_ctx.mc_dev_admin, NULL,
					MC_ADMIN_DEVNODE);
	if (IS_ERR(g_admin_ctx.dev)) {
		err = PTR_ERR(g_admin_ctx.dev);
		goto fail_dev_create;
	}

	g_admin_ctx.mc_dev_name.name = "<t-base";
	g_admin_ctx.dev->driver = &g_admin_ctx.mc_dev_name;
	*out_dev = g_admin_ctx.mc_dev_admin;

	/* Register the call back for starting the secure world */
	g_admin_ctx.tee_start_cb = tee_start_cb;

	MCDRV_DBG("done");
	return 0;

fail_dev_create:
	cdev_del(&g_admin_ctx.mc_admin_cdev);

fail_cdev_add:
	unregister_chrdev_region(g_admin_ctx.mc_dev_admin, MC_DEV_MAX);

fail_alloc_chrdev_region:
	MCDRV_ERROR("fail with %d", err);
	return err;
}

void mc_admin_exit(struct class *mc_device_class)
{
	device_destroy(mc_device_class, g_admin_ctx.mc_dev_admin);
	cdev_del(&g_admin_ctx.mc_admin_cdev);
	unregister_chrdev_region(g_admin_ctx.mc_dev_admin, MC_DEV_MAX);
	/* Requests from driver to daemon */
	mutex_destroy(&g_request.states_mutex);
	MCDRV_DBG("done");
}
