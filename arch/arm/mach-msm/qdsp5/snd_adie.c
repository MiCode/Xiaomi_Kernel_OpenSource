/* Copyright (c) 2009,2013 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <mach/msm_rpcrouter.h>
#include <linux/debugfs.h>
#include <mach/qdsp5/snd_adie.h>
#include <mach/debug_mm.h>

static struct adie_svc_client adie_client[ADIE_SVC_MAX_CLIENTS];
static DEFINE_MUTEX(adie_client_lock);

static int adie_svc_process_cb(struct msm_rpc_client *client,
				 void *buffer, int in_size)
{
	int rc, id;
	uint32_t accept_status;
	struct rpc_request_hdr *req;
	struct adie_svc_client_register_cb_cb_args arg, *buf_ptr;

	req = (struct rpc_request_hdr *)buffer;
	for (id = 0; id < ADIE_SVC_MAX_CLIENTS; id++) {
		if (adie_client[id].rpc_client == client)
			break;
	}
	if (id == ADIE_SVC_MAX_CLIENTS) {
		MM_ERR("RPC reply with invalid rpc client\n");
		accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;
		goto err;
	}

	buf_ptr = (struct adie_svc_client_register_cb_cb_args *)(req + 1);
	arg.cb_id		= be32_to_cpu(buf_ptr->cb_id);
	arg.size		= be32_to_cpu(buf_ptr->size);
	arg.client_id		= be32_to_cpu(buf_ptr->client_id);
	arg.adie_block		= be32_to_cpu(buf_ptr->adie_block);
	arg.status		= be32_to_cpu(buf_ptr->status);
	arg.client_operation	= be32_to_cpu(buf_ptr->client_operation);

	if (arg.cb_id != adie_client[id].cb_id) {
		MM_ERR("RPC reply with invalid invalid cb_id\n");
		accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;
		goto err;
	}

	mutex_lock(&adie_client[id].lock);
	switch (arg.client_operation) {
	case ADIE_SVC_REGISTER_CLIENT:
		MM_DBG("ADIE_SVC_REGISTER_CLIENT callback\n");
		adie_client[id].client_id = arg.client_id;
		break;
	case ADIE_SVC_DEREGISTER_CLIENT:
		MM_DBG("ADIE_SVC_DEREGISTER_CLIENT callback\n");
		break;
	case ADIE_SVC_CONFIG_ADIE_BLOCK:
		MM_DBG("ADIE_SVC_CONFIG_ADIE_BLOCK callback\n");
		if (adie_client[id].client_id != arg.client_id) {
			mutex_unlock(&adie_client[id].lock);
			accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;
			goto err;
		}
		break;
	default:
		accept_status = RPC_ACCEPTSTAT_SYSTEM_ERR;
		goto err;
	}

	adie_client[id].status = arg.status;
	adie_client[id].adie_svc_cb_done = 1;
	mutex_unlock(&adie_client[id].lock);
	wake_up(&adie_client[id].wq);
	accept_status = RPC_ACCEPTSTAT_SUCCESS;

err:
	msm_rpc_start_accepted_reply(client, be32_to_cpu(req->xid),
				     accept_status);
	rc = msm_rpc_send_accepted_reply(client, 0);
	if (rc)
		MM_ERR("%s: send accepted reply failed: %d\n", __func__, rc);

	return rc;
}

static int adie_svc_rpc_cb_func(struct msm_rpc_client *client,
			    void *buffer, int in_size)
{
	int rc = 0;
	struct rpc_request_hdr *req;

	req = (struct rpc_request_hdr *)buffer;

	MM_DBG("procedure received to rpc cb %d\n",
			be32_to_cpu(req->procedure));
	switch (be32_to_cpu(req->procedure)) {
	case ADIE_SVC_CLIENT_STATUS_FUNC_PTR_TYPE_PROC:
		rc = adie_svc_process_cb(client, buffer, in_size);
		break;
	default:
		MM_ERR("%s: procedure not supported %d\n", __func__,
		       be32_to_cpu(req->procedure));
		msm_rpc_start_accepted_reply(client, be32_to_cpu(req->xid),
					     RPC_ACCEPTSTAT_PROC_UNAVAIL);
		rc = msm_rpc_send_accepted_reply(client, 0);
		if (rc)
			MM_ERR("%s: sending reply failed: %d\n", __func__, rc);
		break;
	}
	return rc;
}

static int adie_svc_client_register_arg(struct msm_rpc_client *client,
		void *buf, void *data)
{
	struct adie_svc_client_register_cb_args *arg;

	arg = (struct adie_svc_client_register_cb_args *)data;

	*((int *)buf) = cpu_to_be32((int)arg->cb_id);
	return sizeof(int);
}

static int adie_svc_client_deregister_arg(struct msm_rpc_client *client,
		void *buf, void *data)
{
	struct adie_svc_client_deregister_cb_args *arg;

	arg = (struct adie_svc_client_deregister_cb_args *)data;

	*((int *)buf) = cpu_to_be32(arg->client_id);
	return sizeof(int);
}

static int adie_svc_config_adie_block_arg(struct msm_rpc_client *client,
		void *buf, void *data)
{
	struct adie_svc_config_adie_block_cb_args *arg;
	int size = 0;

	arg = (struct adie_svc_config_adie_block_cb_args *)data;

	*((int *)buf) = cpu_to_be32(arg->client_id);
	size += sizeof(int);
	buf += sizeof(int);

	*((int *)buf) = cpu_to_be32(arg->adie_block);
	size += sizeof(int);
	buf += sizeof(int);

	*((int *)buf) = cpu_to_be32(arg->config);
	size += sizeof(int);

	return size;
}

/* Returns : client id on success
 *           and -1 on failure
 */
int adie_svc_get(void)
{
	int id, rc = 0;
	struct adie_svc_client_register_cb_args arg;

	mutex_lock(&adie_client_lock);
	for (id = 0; id < ADIE_SVC_MAX_CLIENTS; id++) {
		if (adie_client[id].client_id == -1 &&
				adie_client[id].rpc_client == NULL)
			break;
	}
	if (id == ADIE_SVC_MAX_CLIENTS) {
		mutex_unlock(&adie_client_lock);
		return -1;
	}

	mutex_lock(&adie_client[id].lock);
	adie_client[id].rpc_client = msm_rpc_register_client("adie_client",
							ADIE_SVC_PROG,
							ADIE_SVC_VERS, 1,
							adie_svc_rpc_cb_func);
	if (IS_ERR(adie_client[id].rpc_client)) {
		MM_ERR("Failed to register RPC client\n");
		adie_client[id].rpc_client = NULL;
		mutex_unlock(&adie_client[id].lock);
		mutex_unlock(&adie_client_lock);
		return -1;
	}
	mutex_unlock(&adie_client_lock);

	adie_client[id].adie_svc_cb_done = 0;
	arg.cb_id = id;
	adie_client[id].cb_id = arg.cb_id;
	mutex_unlock(&adie_client[id].lock);
	rc = msm_rpc_client_req(adie_client[id].rpc_client,
				SND_ADIE_SVC_CLIENT_REGISTER_PROC,
				adie_svc_client_register_arg, &arg,
					NULL, NULL, -1);
	if (!rc) {
		rc = wait_event_interruptible(adie_client[id].wq,
				adie_client[id].adie_svc_cb_done);
		mutex_lock(&adie_client[id].lock);
		if (unlikely(rc < 0)) {
			if (rc == -ERESTARTSYS)
				MM_ERR("wait_event_interruptible "
						"returned -ERESTARTSYS\n");
			else
				MM_ERR("wait_event_interruptible "
						"returned error\n");
			rc = -1;
			goto err;
		}
		MM_DBG("Status %d received from CB function, id %d rc %d\n",
		       adie_client[id].status, adie_client[id].client_id, rc);
		rc = id;
		if (adie_client[id].status == ADIE_SVC_STATUS_FAILURE) {
			MM_ERR("Received failed status for register request\n");
			rc = -1;
		} else
			goto done;
	} else {
		MM_ERR("Failed to send register client request\n");
		rc = -1;
		mutex_lock(&adie_client[id].lock);
	}
err:
	msm_rpc_unregister_client(adie_client[id].rpc_client);
	adie_client[id].rpc_client = NULL;
	adie_client[id].client_id = -1;
	adie_client[id].cb_id = MSM_RPC_CLIENT_NULL_CB_ID;
	adie_client[id].adie_svc_cb_done = 0;
done:
	mutex_unlock(&adie_client[id].lock);
	return rc;
}
EXPORT_SYMBOL(adie_svc_get);

/* Returns: 0 on succes and
 *         -1 on failure
 */
int adie_svc_put(int id)
{
	int rc = 0;
	struct adie_svc_client_deregister_cb_args arg;

	if (id < 0 || id >= ADIE_SVC_MAX_CLIENTS)
		return -1;

	mutex_lock(&adie_client[id].lock);
	if (adie_client[id].client_id == -1 ||
			adie_client[id].rpc_client == NULL) {
		mutex_unlock(&adie_client[id].lock);
		return -1;
	}
	arg.client_id = adie_client[id].client_id;
	adie_client[id].adie_svc_cb_done = 0;
	mutex_unlock(&adie_client[id].lock);
	rc = msm_rpc_client_req(adie_client[id].rpc_client,
					SND_ADIE_SVC_CLIENT_DEREGISTER_PROC,
					adie_svc_client_deregister_arg, &arg,
					NULL, NULL, -1);
	if (!rc) {
		rc = wait_event_interruptible(adie_client[id].wq,
				adie_client[id].adie_svc_cb_done);
		if (unlikely(rc < 0)) {
			if (rc == -ERESTARTSYS)
				MM_ERR("wait_event_interruptible "
						"returned -ERESTARTSYS\n");
			else
				MM_ERR("wait_event_interruptible "
						"returned error\n");
			rc = -1;
			goto err;
		}
		MM_DBG("Status received from CB function\n");
		mutex_lock(&adie_client[id].lock);
		if (adie_client[id].status == ADIE_SVC_STATUS_FAILURE) {
			rc = -1;
		} else {
			msm_rpc_unregister_client(adie_client[id].rpc_client);
			adie_client[id].rpc_client = NULL;
			adie_client[id].client_id = -1;
			adie_client[id].cb_id = MSM_RPC_CLIENT_NULL_CB_ID;
			adie_client[id].adie_svc_cb_done = 0;
		}
		mutex_unlock(&adie_client[id].lock);
	} else {
		MM_ERR("Failed to send deregister client request\n");
		rc = -1;
	}
err:
	return rc;
}
EXPORT_SYMBOL(adie_svc_put);

/* Returns: 0 on success
 *          2 already in use
 *         -1 on failure
 */
int adie_svc_config_adie_block(int id,
		enum adie_block_enum_type adie_block_type, bool enable)
{
	int rc = 0;
	struct adie_svc_config_adie_block_cb_args arg;

	if (id < 0 || id >= ADIE_SVC_MAX_CLIENTS)
		return -1;

	mutex_lock(&adie_client[id].lock);
	if (adie_client[id].client_id == -1 ||
			adie_client[id].rpc_client == NULL) {
		mutex_unlock(&adie_client[id].lock);
		return -1;
	}
	arg.client_id 	= adie_client[id].client_id;
	arg.adie_block	= adie_block_type;
	arg.config	= (enum adie_config_enum_type)enable;
	adie_client[id].adie_svc_cb_done = 0;
	mutex_unlock(&adie_client[id].lock);
	rc = msm_rpc_client_req(adie_client[id].rpc_client,
					SND_ADIE_SVC_CONFIG_ADIE_BLOCK_PROC,
					adie_svc_config_adie_block_arg, &arg,
					NULL, NULL, -1);
	if (!rc) {
		rc = wait_event_interruptible(adie_client[id].wq,
				adie_client[id].adie_svc_cb_done);
		if (unlikely(rc < 0)) {
			if (rc == -ERESTARTSYS)
				MM_ERR("wait_event_interruptible "
						"returned -ERESTARTSYS\n");
			else
				MM_ERR("wait_event_interruptible "
						"returned error\n");
			rc = -1;
			goto err;
		}
		MM_DBG("Status received from CB function\n");
		mutex_lock(&adie_client[id].lock);
		if (adie_client[id].status == ADIE_SVC_STATUS_FAILURE)
			rc = -1;
		else
			rc = adie_client[id].status;
		mutex_unlock(&adie_client[id].lock);
	} else {
		MM_ERR("Failed to send adie block config request\n");
		rc = -1;
	}
err:
	return rc;
}
EXPORT_SYMBOL(adie_svc_config_adie_block);

#ifdef CONFIG_DEBUG_FS

struct dentry *dentry;
static char l_buf[100];

static ssize_t snd_adie_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t snd_adie_debug_write(struct file *file, const char __user *buf,
	       size_t count, loff_t *ppos)
{
	int rc = 0, op = 0;
	int id = 0, adie_block = 0, config = 1;
	size_t len;

	len = count > (sizeof(l_buf) - 1) ?
			(sizeof(l_buf) - 1) : count;
	l_buf[len] = 0;
	if (copy_from_user(l_buf, buf, len)) {
		pr_info("Unable to copy data from user space\n");
		return -EFAULT;
	}
	if (sscanf(l_buf, "%d %d %d %d", &op, &id, &adie_block, &config) != 4)
		return -EINVAL;
	MM_INFO("\nUser input: op %d id %d block %d config %d\n", op, id,
			adie_block, config);
	switch (op) {
	case ADIE_SVC_REGISTER_CLIENT:
		MM_INFO("ADIE_SVC_REGISTER_CLIENT\n");
		rc = adie_svc_get();
		if (rc >= 0)
			MM_INFO("Client registered: %d\n", rc);
		else
			MM_ERR("Failed registering client\n");
		break;
	case ADIE_SVC_DEREGISTER_CLIENT:
		MM_INFO("ADIE_SVC_DEREGISTER_CLIENT: %d\n", id);
		rc = adie_svc_put(id);
		if (!rc)
			MM_INFO("Client %d deregistered\n", id);
		else
			MM_ERR("Failed unregistering the client: %d\n",	id);
		break;
	case ADIE_SVC_CONFIG_ADIE_BLOCK:
		MM_INFO("ADIE_SVC_CONFIG_ADIE_BLOCK: id %d adie_block %d \
				config %d\n", id, adie_block, config);
		rc =  adie_svc_config_adie_block(id,
			(enum adie_block_enum_type)adie_block, (bool)config);
		if (!rc)
			MM_INFO("ADIE block %d %s", adie_block,
					config ? "enabled\n" : "disabled\n");
		else if (rc == 2)
			MM_INFO("ADIE block %d already in use\n", adie_block);
		else
			MM_ERR("ERROR configuring the ADIE block\n");
		break;
	default:
		MM_INFO("Invalid operation\n");
	}
	return count;
}

static ssize_t snd_adie_debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	static char buffer[1024];
	const int debug_bufmax = sizeof(buffer);
	int id, n = 0;

	n += scnprintf(buffer + n, debug_bufmax - n,
			"LIST OF CLIENTS\n");
	for (id = 0; id < ADIE_SVC_MAX_CLIENTS ; id++) {
		if (adie_client[id].client_id != -1 &&
				adie_client[id].rpc_client != NULL) {
			n += scnprintf(buffer + n, debug_bufmax - n,
				"id %d rpc client 0x%08x\n", id,
				(uint32_t)adie_client[id].rpc_client);
		}
	}
	buffer[n] = 0;
	return simple_read_from_buffer(buf, count, ppos, buffer, n);
}

static const struct file_operations snd_adie_debug_fops = {
	.read = snd_adie_debug_read,
	.open = snd_adie_debug_open,
	.write = snd_adie_debug_write,
};
#endif

static void __exit snd_adie_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	if (dentry)
		debugfs_remove(dentry);
#endif
}

static int __init snd_adie_init(void)
{
	int id;
#ifdef CONFIG_DEBUG_FS
	char name[sizeof "msm_snd_adie"];

	snprintf(name, sizeof name, "msm_snd_adie");
	dentry = debugfs_create_file(name, S_IFREG | S_IRUGO | S_IWUGO,
			NULL, NULL, &snd_adie_debug_fops);
	if (IS_ERR(dentry))
		MM_DBG("debugfs_create_file failed\n");
#endif
	for (id = 0; id < ADIE_SVC_MAX_CLIENTS; id++) {
		adie_client[id].client_id = -1;
		adie_client[id].cb_id = MSM_RPC_CLIENT_NULL_CB_ID;
		adie_client[id].status = 0;
		adie_client[id].adie_svc_cb_done = 0;
		mutex_init(&adie_client[id].lock);
		init_waitqueue_head(&adie_client[id].wq);
		adie_client[id].rpc_client = NULL;
	}
	return 0;
}

module_init(snd_adie_init);
module_exit(snd_adie_exit);
