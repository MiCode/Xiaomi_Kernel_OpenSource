// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2021, The Linux Foundation. All rights reserved. */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/qrtr.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/limits.h>
#include <linux/slab.h>
#include <linux/cnss_plat_ipc_qmi.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include "cnss_plat_ipc_service_v01.h"

#define CNSS_MAX_FILE_SIZE (32 * 1024 * 1024)
#define CNSS_PLAT_IPC_MAX_CLIENTS 1
#define CNSS_PLAT_IPC_QMI_FILE_TXN_TIMEOUT 10000
#define QMI_INIT_RETRY_MAX_TIMES 240
#define QMI_INIT_RETRY_DELAY_MS 250

/**
 * struct cnss_plat_ipc_file_data: File transfer context data
 * @name: File name
 * @buf: Buffer provided for TX/RX file contents
 * @id: File ID corresponding to file name
 * @buf_size: Buffer size
 * @file_fize: File Size
 * @seg_index: Running index for buffer segments
 * @seg_len: Total number of segments
 * @end: End of transaction
 * @complete: Completion variable for file transfer
 */
struct cnss_plat_ipc_file_data {
	char *name;
	char *buf;
	u32 id;
	u32 buf_size;
	u32 file_size;
	u32 seg_index;
	u32 seg_len;
	u32 end;
	struct completion complete;
};

/**
 * struct cnss_plat_ipc_qmi_svc_ctx: Platform context for QMI IPC service
 * @svc_hdl: QMI server handle
 * @client_sq: CNSS Daemon client QRTR socket
 * @client_connected: Daemon client connection status
 * @file_idr: File ID generator
 * @flle_idr_lock: File ID generator usage lock
 * @cfg: CNSS daemon provided user config
 * @connection_update_cb: Registered user callback for daemon connection status
 * @cb_ctx: Context for registered user
 * @num_user: Number of registered users
 */
struct cnss_plat_ipc_qmi_svc_ctx {
	struct qmi_handle *svc_hdl;
	struct sockaddr_qrtr client_sq;
	bool client_connected;
	struct idr file_idr;
	struct mutex file_idr_lock; /* File ID generator usage lock */
	struct cnss_plat_ipc_user_config cfg;

	cnss_plat_ipc_connection_update
		connection_update_cb[CNSS_PLAT_IPC_MAX_CLIENTS];
	void *cb_ctx[CNSS_PLAT_IPC_MAX_CLIENTS];
	u32 num_user;
};

static struct cnss_plat_ipc_qmi_svc_ctx plat_ipc_qmi_svc;

/**
 * cnss_plat_ipc_init_file_data() - Initialize file transfer context data
 * @name: File name
 * @buf: Buffer pointer for file contents
 * @buf_size: Buffer size for download / upload
 * @file_size: File size for upload
 *
 * Return: File data pointer
 */
static
struct cnss_plat_ipc_file_data *cnss_plat_ipc_init_file_data(char *name,
							     char *buf,
							     u32 buf_size,
							     u32 file_size)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	struct cnss_plat_ipc_file_data *fd;

	fd = kmalloc(sizeof(*fd), GFP_KERNEL);
	if (!fd)
		goto end;
	fd->name = name;
	fd->buf = buf;
	fd->buf_size = buf_size;
	fd->file_size = file_size;
	fd->seg_index = 0;
	fd->end = 0;
	if (file_size)
		fd->seg_len =
			(file_size / CNSS_PLAT_IPC_QMI_MAX_DATA_SIZE_V01) +
			!!(file_size % CNSS_PLAT_IPC_QMI_MAX_DATA_SIZE_V01);
	else
		fd->seg_len = 0;
	init_completion(&fd->complete);
	mutex_lock(&svc->file_idr_lock);
	fd->id = idr_alloc_cyclic(&svc->file_idr, fd, 0, U32_MAX, GFP_KERNEL);
	if (fd->id < 0) {
		kfree(fd);
		fd = NULL;
	}
	mutex_unlock(&svc->file_idr_lock);
end:
	return fd;
}

/**
 * cnss_plat_ipc_deinit_file_data() - Release file transfer context data
 * @fd: File data pointer
 *
 * Return: 0 on success, negative error values otherwise
 */
static int cnss_plat_ipc_deinit_file_data(struct cnss_plat_ipc_file_data *fd)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	int ret = 0;

	if (unlikely(!fd))
		return -EINVAL;

	mutex_lock(&svc->file_idr_lock);
	idr_remove(&svc->file_idr, fd->id);
	mutex_unlock(&svc->file_idr_lock);

	if (!fd->end)
		ret = -EINVAL;
	kfree(fd);
	return ret;
}

/**
 * cnss_plat_ipc_qmi_update_clients() - Inform registered clients for status
 *                                      update
 *
 * Return: None
 */
static void cnss_plat_ipc_qmi_update_clients(void)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	int i;

	for (i = 0; i < CNSS_PLAT_IPC_MAX_CLIENTS; i++) {
		if (svc->connection_update_cb[i])
			svc->connection_update_cb[i](svc->cb_ctx[i],
						     svc->client_connected);
	}
}

/**
 * cnss_plat_ipc_qmi_file_upload() - Upload data as platform accessible file
 * @file_mame: File name to store in platform data location
 * @file_buf: Pointer to buffer with file contents
 * @file_size: Provides the size of buffer / file size
 *
 * Return: 0 on success, negative error values otherwise
 */
int cnss_plat_ipc_qmi_file_upload(char *file_name, u8 *file_buf,
				  u32 file_size)
{
	struct cnss_plat_ipc_qmi_file_upload_ind_msg_v01 ind;
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	int ret;
	struct cnss_plat_ipc_file_data *fd;

	if (!svc->client_connected || !file_name || !file_buf)
		return -EINVAL;

	pr_info("%s: File name: %s Size: %d\n", __func__, file_name, file_size);

	if (file_size == 0 || file_size > CNSS_MAX_FILE_SIZE)
		return -EINVAL;

	fd = cnss_plat_ipc_init_file_data(file_name, file_buf, file_size,
					  file_size);
	if (!fd) {
		pr_err("%s: Unable to initialize file transfer data\n",
		       __func__);
		return -EINVAL;
	}
	scnprintf(ind.file_name, CNSS_PLAT_IPC_QMI_MAX_FILE_NAME_LEN_V01, "%s",
		  fd->name);
	ind.file_size = fd->file_size;
	ind.file_id = fd->id;

	ret = qmi_send_indication
			(svc->svc_hdl, &svc->client_sq,
			 CNSS_PLAT_IPC_QMI_FILE_UPLOAD_IND_V01,
			 CNSS_PLAT_IPC_QMI_FILE_UPLOAD_IND_MSG_V01_MAX_MSG_LEN,
			 cnss_plat_ipc_qmi_file_upload_ind_msg_v01_ei, &ind);

	if (ret < 0) {
		pr_err("%s: QMI failed: %d\n", __func__, ret);
		goto end;
	}
	ret = wait_for_completion_timeout(&fd->complete,
					  msecs_to_jiffies
					  (CNSS_PLAT_IPC_QMI_FILE_TXN_TIMEOUT));
	if (!ret)
		pr_err("%s: Timeout Uploading file: %s\n", __func__, fd->name);

end:
	ret = cnss_plat_ipc_deinit_file_data(fd);
	pr_debug("%s: Status: %d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(cnss_plat_ipc_qmi_file_upload);

/**
 * cnss_plat_ipc_qmi_file_upload_req_handler() - QMI Upload data request handler
 * @handle: Pointer to QMI handle
 * @sq: QMI socket
 * @txn: QMI transaction pointer
 * @decoded_msg: Pointer to decoded QMI message
 *
 * Handles the QMI upload sequence from userspace. It uses the file descriptor
 * ID to upload buffer contents to QMI messages as segments.
 *
 * Return: None
 */
static void
cnss_plat_ipc_qmi_file_upload_req_handler(struct qmi_handle *handle,
					  struct sockaddr_qrtr *sq,
					  struct qmi_txn *txn,
					  const void *decoded_msg)
{
	struct cnss_plat_ipc_qmi_file_upload_req_msg_v01 *req_msg;
	struct cnss_plat_ipc_qmi_file_upload_resp_msg_v01 *resp;
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	int ret = 0;
	struct cnss_plat_ipc_file_data *fd;

	req_msg = (struct cnss_plat_ipc_qmi_file_upload_req_msg_v01 *)
		   decoded_msg;
	if (!req_msg)
		return;
	pr_debug("%s: File ID: %d Seg Index: %d\n", __func__, req_msg->file_id,
		 req_msg->seg_index);

	mutex_lock(&svc->file_idr_lock);
	fd = idr_find(&svc->file_idr, req_msg->file_id);
	mutex_unlock(&svc->file_idr_lock);
	if (!fd) {
		pr_err("%s: Invalid File ID %d\n", __func__, req_msg->file_id);
		return;
	}

	if (req_msg->seg_index != fd->seg_index) {
		pr_err("%s: File %s transfer segment failure\n", __func__,
		       fd->name);
		complete(&fd->complete);
	}

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp)
		return;

	resp->file_id = fd->id;
	resp->seg_index = fd->seg_index++;
	resp->seg_buf_len =
		(fd->buf_size > CNSS_PLAT_IPC_QMI_MAX_DATA_SIZE_V01 ?
		 CNSS_PLAT_IPC_QMI_MAX_DATA_SIZE_V01 : fd->buf_size);
	resp->end = (fd->seg_index == fd->seg_len);
	memcpy(resp->seg_buf, fd->buf, resp->seg_buf_len);

	pr_debug("%s: ID: %d Seg ID: %d Len: %d End: %d\n", __func__,
		 resp->file_id, resp->seg_index, resp->seg_buf_len, resp->end);

	ret = qmi_send_response
		(svc->svc_hdl, sq, txn,
		CNSS_PLAT_IPC_QMI_FILE_UPLOAD_RESP_V01,
		CNSS_PLAT_IPC_QMI_FILE_UPLOAD_RESP_MSG_V01_MAX_MSG_LEN,
		cnss_plat_ipc_qmi_file_upload_resp_msg_v01_ei,
		resp);

	if (ret < 0) {
		pr_err("%s: QMI failed: %d\n", __func__, ret);
		goto end;
	}

	fd->buf_size -= resp->seg_buf_len;
	fd->buf += resp->seg_buf_len;
	if (resp->end) {
		fd->end = true;
		complete(&fd->complete);
	}
end:
	kfree(resp);
}

/**
 * cnss_plat_ipc_qmi_file_download() - Download platform accessible file
 * @file_mame: File name to get from platform data location
 * @buf: Pointer of the buffer to store file contents
 * @size: Provides the size of buffer. It is updated to reflect the file size
 *        at the end of file download.
 */
int cnss_plat_ipc_qmi_file_download(char *file_name, char *buf, u32 *size)
{
	struct cnss_plat_ipc_qmi_file_download_ind_msg_v01 ind;
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	int ret;
	struct cnss_plat_ipc_file_data *fd;

	if (!svc->client_connected || !file_name || !buf)
		return -EINVAL;

	fd = cnss_plat_ipc_init_file_data(file_name, buf, *size, 0);
	if (!fd) {
		pr_err("%s: Unable to initialize file transfer data\n",
		       __func__);
		return -EINVAL;
	}

	scnprintf(ind.file_name, CNSS_PLAT_IPC_QMI_MAX_FILE_NAME_LEN_V01, "%s",
		  file_name);
	ind.file_id = fd->id;

	ret = qmi_send_indication
		(svc->svc_hdl, &svc->client_sq,
		 CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_IND_V01,
		 CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_IND_MSG_V01_MAX_MSG_LEN,
		 cnss_plat_ipc_qmi_file_download_ind_msg_v01_ei, &ind);

	if (ret < 0) {
		pr_err("%s: QMI failed: %d\n", __func__, ret);
		goto end;
	}
	ret = wait_for_completion_timeout(&fd->complete,
					  msecs_to_jiffies
					  (CNSS_PLAT_IPC_QMI_FILE_TXN_TIMEOUT));
	if (!ret)
		pr_err("%s: Timeout downloading file:%s\n", __func__, fd->name);

end:
	*size = fd->file_size;
	ret = cnss_plat_ipc_deinit_file_data(fd);
	pr_debug("%s: Status: %d Size: %d\n", __func__, ret, *size);

	return ret;
}
EXPORT_SYMBOL(cnss_plat_ipc_qmi_file_download);

/**
 * cnss_plat_ipc_qmi_file_download_req_handler() - QMI download request handler
 * @handle: Pointer to QMI handle
 * @sq: QMI socket
 * @txn: QMI transaction pointer
 * @decoded_msg: Pointer to decoded QMI message
 *
 * Handles the QMI download request sequence to userspace. It uses the file
 * descriptor ID to download QMI message buffer segment to file descriptor
 * buffer.
 *
 * Return: None
 */
static void
cnss_plat_ipc_qmi_file_download_req_handler(struct qmi_handle *handle,
					    struct sockaddr_qrtr *sq,
					    struct qmi_txn *txn,
					    const void *decoded_msg)
{
	struct cnss_plat_ipc_qmi_file_download_req_msg_v01 *req_msg;
	struct cnss_plat_ipc_qmi_file_download_resp_msg_v01 resp = {0};
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	int ret = 0;
	struct cnss_plat_ipc_file_data *fd;

	req_msg = (struct cnss_plat_ipc_qmi_file_download_req_msg_v01 *)
		   decoded_msg;
	if (!req_msg)
		return;
	pr_debug("%s: File ID: %d Size: %d Seg Len: %d Index: %d End: %d\n",
		 __func__, req_msg->file_id, req_msg->file_size,
		 req_msg->seg_buf_len, req_msg->seg_index, req_msg->end);

	mutex_lock(&svc->file_idr_lock);
	fd = idr_find(&svc->file_idr, req_msg->file_id);
	mutex_unlock(&svc->file_idr_lock);
	if (!fd) {
		pr_err("%s: Invalid File ID: %d\n", __func__, req_msg->file_id);
		return;
	}

	if (req_msg->file_size > fd->buf_size) {
		pr_err("%s: File %s size %d larger than buffer size %d\n",
		       __func__, fd->name, req_msg->file_size, fd->buf_size);
		goto file_error;
	}
	if (req_msg->seg_buf_len > CNSS_PLAT_IPC_QMI_MAX_DATA_SIZE_V01 ||
	    ((req_msg->seg_buf_len + fd->file_size) > fd->buf_size)) {
		pr_err("%s: Segment buf ID: %d buffer size %d not allowed\n",
		       __func__, req_msg->seg_index, req_msg->seg_buf_len);
		goto file_error;
	}
	if (req_msg->seg_index != fd->seg_index) {
		pr_err("%s: File %s transfer segment failure\n", __func__,
		       fd->name);
		goto file_error;
	}

	memcpy(fd->buf, req_msg->seg_buf, req_msg->seg_buf_len);
	fd->seg_index++;
	fd->buf += req_msg->seg_buf_len;
	fd->file_size += req_msg->seg_buf_len;

	resp.file_id = fd->id;
	resp.seg_index = fd->seg_index;
	ret = qmi_send_response
		(svc->svc_hdl, sq, txn,
		CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_RESP_V01,
		CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_RESP_MSG_V01_MAX_MSG_LEN,
		cnss_plat_ipc_qmi_file_download_resp_msg_v01_ei,
		&resp);

	if (ret < 0)
		pr_err("%s: QMI failed: %d\n", __func__, ret);

	if (req_msg->end) {
		fd->end = true;
		complete(&fd->complete);
	}

	return;
file_error:
	complete(&fd->complete);
}

/**
 * cnss_plat_ipc_qmi_init_setup_req_handler() - Init_Setup QMI message handler
 * @handle: Pointer to QMI handle
 * @sq: QMI socket
 * @txn: QMI transaction pointer
 * @decoded_msg: Pointer to decoded QMI message
 *
 * Handles the QMI Init setup handshake message from userspace.
 * buffer.
 *
 * Return: None
 */
static void
cnss_plat_ipc_qmi_init_setup_req_handler(struct qmi_handle *handle,
					 struct sockaddr_qrtr *sq,
					 struct qmi_txn *txn,
					 const void *decoded_msg)
{
	struct cnss_plat_ipc_qmi_init_setup_req_msg_v01 *req_msg;
	struct cnss_plat_ipc_qmi_init_setup_resp_msg_v01 resp = {0};
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	int ret = 0;

	if (!svc->client_connected) {
		pr_info("%s: CNSS Daemon Connected. QMI Socket Node: %d Port: %d\n",
			__func__, sq->sq_node, sq->sq_port);
		svc->client_sq = *sq;
		svc->client_connected = true;
		cnss_plat_ipc_qmi_update_clients();
	} else {
		pr_err("CNSS Daemon already connected. Invalid new client\n");
		return;
	}

	req_msg =
		(struct cnss_plat_ipc_qmi_init_setup_req_msg_v01 *)decoded_msg;
	pr_debug("%s: MAC: %d HW_TRC: %d CAL: %d\n", __func__,
		 req_msg->dms_mac_addr_supported,
		 req_msg->qdss_hw_trace_override,
		 req_msg->cal_file_available_bitmask);

	svc->cfg.dms_mac_addr_supported = req_msg->dms_mac_addr_supported;
	svc->cfg.qdss_hw_trace_override = req_msg->qdss_hw_trace_override;
	svc->cfg.cal_file_available_bitmask =
					req_msg->cal_file_available_bitmask;

	ret = qmi_send_response
			(svc->svc_hdl, sq, txn,
			CNSS_PLAT_IPC_QMI_INIT_SETUP_RESP_V01,
			CNSS_PLAT_IPC_QMI_INIT_SETUP_RESP_MSG_V01_MAX_MSG_LEN,
			cnss_plat_ipc_qmi_init_setup_resp_msg_v01_ei, &resp);
	if (ret < 0)
		pr_err("%s: QMI failed: %d\n", __func__, ret);
}

/**
 * cnss_plat_ipc_qmi_disconnect_cb() - Handler for QMI node disconnect specific
 *                                     to node and port
 * @handle: Pointer to QMI handle
 * @node: QMI node that is disconnected
 * @port: QMI port that is disconnected
 *
 * Return: None
 */
static void cnss_plat_ipc_qmi_disconnect_cb(struct qmi_handle *handle,
					    unsigned int node,
					    unsigned int port)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	struct cnss_plat_ipc_file_data *fd;
	u32 file_id;

	if (svc->svc_hdl != handle) {
		pr_err("%s: Invalid QMI Handle\n", __func__);
		return;
	}

	if (svc->client_connected && svc->client_sq.sq_node == node &&
	    svc->client_sq.sq_port == port) {
		pr_err("%s: CNSS Daemon disconnected. QMI Socket Node:%d Port:%d\n",
		       __func__, node, port);
		svc->client_sq.sq_node = 0;
		svc->client_sq.sq_port = 0;
		svc->client_sq.sq_family = 0;
		svc->client_connected = false;

		/* Daemon killed. Fail any download / upload in progress. This
		 * will also free stale fd
		 */
		mutex_lock(&svc->file_idr_lock);
		idr_for_each_entry(&svc->file_idr, fd, file_id)
			complete(&fd->complete);
		mutex_unlock(&svc->file_idr_lock);
		cnss_plat_ipc_qmi_update_clients();
	}
}

/**
 * cnss_plat_ipc_qmi_bye_cb() - Handler for QMI node disconnect for all port of
 *                              the given node.
 * @handle: Pointer to QMI handle
 * @node: QMI node that is disconnected
 *
 * Return: None
 */
static void cnss_plat_ipc_qmi_bye_cb(struct qmi_handle *handle,
				     unsigned int node)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;

	cnss_plat_ipc_qmi_disconnect_cb(handle, node, svc->client_sq.sq_port);
}

static struct qmi_ops cnss_plat_ipc_qmi_ops = {
	/* inform a client that all clients from a node are gone */
	.bye = cnss_plat_ipc_qmi_bye_cb,
	.del_client = cnss_plat_ipc_qmi_disconnect_cb,
};

static struct qmi_msg_handler cnss_plat_ipc_qmi_req_handlers[] = {
	{
		.type = QMI_REQUEST,
		.msg_id = CNSS_PLAT_IPC_QMI_INIT_SETUP_REQ_V01,
		.ei = cnss_plat_ipc_qmi_init_setup_req_msg_v01_ei,
		.decoded_size =
			CNSS_PLAT_IPC_QMI_INIT_SETUP_REQ_MSG_V01_MAX_MSG_LEN,
		.fn = cnss_plat_ipc_qmi_init_setup_req_handler,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_REQ_V01,
		.ei = cnss_plat_ipc_qmi_file_download_req_msg_v01_ei,
		.decoded_size =
			CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
		.fn = cnss_plat_ipc_qmi_file_download_req_handler,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = CNSS_PLAT_IPC_QMI_FILE_UPLOAD_REQ_V01,
		.ei = cnss_plat_ipc_qmi_file_upload_req_msg_v01_ei,
		.decoded_size =
			CNSS_PLAT_IPC_QMI_FILE_UPLOAD_REQ_MSG_V01_MAX_MSG_LEN,
		.fn = cnss_plat_ipc_qmi_file_upload_req_handler,
	}
};

/**
 * cnss_plat_ipc_qmi_user_config() - Get User space config for CNSS platform
 *
 * Return: Pointer to user space client config
 */
struct cnss_plat_ipc_user_config *cnss_plat_ipc_qmi_user_config(void)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;

	if (!svc->client_connected)
		return NULL;

	return &svc->cfg;
}
EXPORT_SYMBOL(cnss_plat_ipc_qmi_user_config);

/**
 * cnss_plat_ipc_register() - Register for QMI IPC client status update
 * @connect_update_cb: Function pointer for callback
 * @cb_cbt: Callback context
 *
 * Return: 0 on success, negative error value otherwise
 */
int cnss_plat_ipc_register(cnss_plat_ipc_connection_update
			   connection_update_cb, void *cb_ctx)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;

	if (svc->num_user >= CNSS_PLAT_IPC_MAX_CLIENTS) {
		pr_err("Max Service users reached\n");
		return -EINVAL;
	}

	svc->connection_update_cb[svc->num_user] = connection_update_cb;
	svc->cb_ctx[svc->num_user] = cb_ctx;
	svc->num_user++;

	return 0;
}
EXPORT_SYMBOL(cnss_plat_ipc_register);

/**
 * cnss_plat_ipc_register() - Unregister QMI IPC client status callback
 * @cb_cbt: Callback context provided during registration
 *
 * Return: None
 */
void cnss_plat_ipc_unregister(void *cb_ctx)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	int i;

	for (i = 0; i < svc->num_user; i++) {
		if (svc->cb_ctx[i] == cb_ctx) {
			svc->cb_ctx[i] = NULL;
			svc->connection_update_cb[i] = NULL;
			svc->num_user--;
			break;
		}
	}
}
EXPORT_SYMBOL(cnss_plat_ipc_unregister);

/**
 * cnss_plat_ipc_init_fn() - CNSS Platform qmi service init function
 *
 * Initialize a QMI client handle and register new QMI service for CNSS Platform
 *
 * Return: None
 */
static void cnss_plat_ipc_init_fn(struct work_struct *work)
{
	int ret = 0, retry = 0;
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;

	svc->svc_hdl = kzalloc(sizeof(*svc->svc_hdl), GFP_KERNEL);
	if (!svc->svc_hdl)
		return;

retry:
	ret = qmi_handle_init(svc->svc_hdl,
			      CNSS_PLAT_IPC_QMI_MAX_MSG_SIZE_V01,
			      &cnss_plat_ipc_qmi_ops,
			      cnss_plat_ipc_qmi_req_handlers);
	if (ret < 0) {
		/* If QMI fails to init, retry for total
		 * QMI_INIT_RETRY_DELAY_MS * QMI_INIT_RETRY_MAX_TIMES ms.
		 */
		if (retry++ < QMI_INIT_RETRY_MAX_TIMES) {
			msleep(QMI_INIT_RETRY_DELAY_MS);
			goto retry;
		}
		pr_err("%s: Failed to init QMI handle after %d ms * %d, err = %d\n",
		       __func__, ret, QMI_INIT_RETRY_DELAY_MS,
		       QMI_INIT_RETRY_MAX_TIMES);
		goto free_svc_hdl;
	}

	ret = qmi_add_server(svc->svc_hdl,
			     CNSS_PLATFORM_SERVICE_ID_V01,
			     CNSS_PLATFORM_SERVICE_VERS_V01, 0);
	if (ret < 0) {
		pr_err("%s: Server add fail: %d\n", __func__, ret);
		goto release_svc_hdl;
	}

	pr_info("%s: CNSS Platform IPC QMI Service is started\n", __func__);
	idr_init(&svc->file_idr);
	mutex_init(&svc->file_idr_lock);
	return;

release_svc_hdl:
	qmi_handle_release(svc->svc_hdl);
free_svc_hdl:
	kfree(svc->svc_hdl);
}

static DECLARE_WORK(cnss_plat_ipc_init_work, cnss_plat_ipc_init_fn);

static int __init cnss_plat_ipc_qmi_svc_init(void)
{
	/* Schedule a work to do real init to avoid blocking here */
	schedule_work(&cnss_plat_ipc_init_work);
	return 0;
}

/**
 * cnss_plat_ipc_qmi_svc_exit() - CNSS Platform qmi service exit
 *
 * Release all resources during exit
 *
 * Return: None
 */
static void __exit cnss_plat_ipc_qmi_svc_exit(void)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;

	cancel_work_sync(&cnss_plat_ipc_init_work);

	if (svc->svc_hdl) {
		qmi_handle_release(svc->svc_hdl);
		kfree(svc->svc_hdl);
		idr_destroy(&svc->file_idr);
	}
}

module_init(cnss_plat_ipc_qmi_svc_init);
module_exit(cnss_plat_ipc_qmi_svc_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CNSS Platform IPC QMI Service");
