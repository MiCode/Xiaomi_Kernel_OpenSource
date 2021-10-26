// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2021, The Linux Foundation. All rights reserved. */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/qrtr.h>
#include <linux/soc/qcom/qmi.h>
#if IS_ENABLED(CONFIG_IPC_LOGGING)
#include <linux/ipc_logging.h>
#endif
#include <linux/sched.h>
#include <asm/current.h>
#include <linux/limits.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include "cnss_plat_ipc_qmi.h"
#include "cnss_plat_ipc_service_v01.h"

#define CNSS_MAX_FILE_SIZE (32 * 1024 * 1024)
#define CNSS_PLAT_IPC_MAX_USER 1
#define CNSS_PLAT_IPC_QMI_FILE_TXN_TIMEOUT 10000
#define QMI_INIT_RETRY_MAX_TIMES 240
#define QMI_INIT_RETRY_DELAY_MS 250
#define NUM_LOG_PAGES			10

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
 * struct cnss_plat_ipc_qmi_client_ctx: Context for QMI IPC client
 * @client_sq: QMI IPC client QRTR socket
 * @client_connected: QMI IPC client connection status
 * @connection_update_cb: Registered user callback for QMI connection status
 * @cb_ctx: Context for registered user
 * @num_user: Number of registered users
 */
struct cnss_plat_ipc_qmi_client_ctx {
	struct sockaddr_qrtr client_sq;
	bool client_connected;

	cnss_plat_ipc_connection_update
		connection_update_cb[CNSS_PLAT_IPC_MAX_USER];
	void *cb_ctx[CNSS_PLAT_IPC_MAX_USER];
	u32 num_user;
};

/**
 * struct cnss_plat_ipc_qmi_svc_ctx: Platform context for QMI IPC service
 * @svc_hdl: QMI server handle
 * @file_idr: File ID generator
 * @flle_idr_lock: File ID generator usage lock
 * @qmi_client_ctx: ontext for QMI IPC client
 */
struct cnss_plat_ipc_qmi_svc_ctx {
	struct qmi_handle *svc_hdl;
	struct idr file_idr;
	struct mutex file_idr_lock; /* File ID generator usage lock */
	struct cnss_plat_ipc_qmi_client_ctx
		qmi_client_ctx[CNSS_PLAT_IPC_MAX_QMI_CLIENTS + 1];
};

static struct cnss_plat_ipc_qmi_svc_ctx plat_ipc_qmi_svc;
static struct cnss_plat_ipc_daemon_config daemon_cfg;

#if IS_ENABLED(CONFIG_IPC_LOGGING)
static void *cnss_plat_ipc_log_context;

static void cnss_plat_ipc_logging_init(void)
{
	cnss_plat_ipc_log_context = ipc_log_context_create(NUM_LOG_PAGES,
							   "cnss_plat", 0);
	if (!cnss_plat_ipc_log_context)
		pr_err("cnss_plat: Unable to create log context\n");
}

static void cnss_plat_ipc_logging_deinit(void)
{
	if (cnss_plat_ipc_log_context) {
		ipc_log_context_destroy(cnss_plat_ipc_log_context);
		cnss_plat_ipc_log_context = NULL;
	}
}

void cnss_plat_ipc_debug_log_print(void *log_ctx, char *process, const char *fn,
				   const char *log_level, char *fmt, ...)
{
	struct va_format vaf;
	va_list va_args;

	va_start(va_args, fmt);
	vaf.fmt = fmt;
	vaf.va = &va_args;

	if (log_level)
		printk("%scnss_plat: %pV", log_level, &vaf);

	ipc_log_string(log_ctx, "[%s] %s: %pV", process, fn, &vaf);

	va_end(va_args);
}

#define cnss_plat_ipc_log_print(_x...) \
		cnss_plat_ipc_debug_log_print(cnss_plat_ipc_log_context, _x)
#else
static void cnss_plat_ipc_logging_init(void) {};
static void cnss_plat_ipc_logging_deinit(void) {};

void cnss_plat_ipc_debug_log_print(void *log_ctx, char *process, const char *fn,
				   const char *log_level, char *fmt, ...)
{
	struct va_format vaf;
	va_list va_args;

	va_start(va_args, fmt);
	vaf.fmt = fmt;
	vaf.va = &va_args;

	if (log_level)
		printk("%scnss_plat: %pV", log_level, &vaf);

	va_end(va_args);
}

#define cnss_plat_ipc_log_print(_x...) \
		cnss_plat_ipc_debug_log_print((void *)NULL, _x)
#endif

#define proc_name (in_irq() ? "irq" : \
		(in_softirq() ? "soft_irq" : current->comm))
#define cnss_plat_ipc_err(_fmt, ...) \
		cnss_plat_ipc_log_print(proc_name, __func__, \
		KERN_ERR, _fmt, ##__VA_ARGS__)

#define cnss_plat_ipc_info(_fmt, ...) \
		cnss_plat_ipc_log_print(proc_name, __func__, \
		KERN_INFO, _fmt, ##__VA_ARGS__)

#define cnss_plat_ipc_dbg(_fmt, ...) \
		cnss_plat_ipc_log_print(proc_name, __func__, \
		KERN_DEBUG, _fmt, ##__VA_ARGS__)
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
 * cnss_plat_ipc_qmi_update_user() - Inform registered users about QMI
 *                                      client status
 * @client_id: User space QMI IPC client ID. Also works as
 *		array index for QMI client context
 *
 * Return: None
 */
static void
cnss_plat_ipc_qmi_update_user(enum cnss_plat_ipc_qmi_client_id_v01 client_id)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	struct cnss_plat_ipc_qmi_client_ctx *qmi_client =
			&svc->qmi_client_ctx[client_id];
	int i;

	for (i = 0; i < qmi_client->num_user; i++) {
		if (qmi_client->connection_update_cb[i])
			qmi_client->connection_update_cb[i]
						(qmi_client->cb_ctx[i],
						qmi_client->client_connected);
	}
}

/**
 * cnss_plat_ipc_qmi_file_upload() - Upload data as platform accessible file
 * @client_id: User space QMI IPC client ID. Also works as
 *		array index for QMI client context
 * @file_mame: File name to store in platform data location
 * @file_buf: Pointer to buffer with file contents
 * @file_size: Provides the size of buffer / file size
 *
 * Return: 0 on success, negative error values otherwise
 */
int cnss_plat_ipc_qmi_file_upload(enum cnss_plat_ipc_qmi_client_id_v01
				  client_id, char *file_name, u8 *file_buf,
				  u32 file_size)
{
	struct cnss_plat_ipc_qmi_file_upload_ind_msg_v01 ind;
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	struct cnss_plat_ipc_qmi_client_ctx *qmi_client;
	int ret;
	struct cnss_plat_ipc_file_data *fd;

	if (client_id > CNSS_PLAT_IPC_MAX_QMI_CLIENTS) {
		cnss_plat_ipc_err("Invalid Client ID: %d\n", client_id);
		return -EINVAL;
	}

	qmi_client = &svc->qmi_client_ctx[client_id];

	if (!qmi_client->client_connected || !file_name || !file_buf)
		return -EINVAL;

	cnss_plat_ipc_info("File name: %s Size: %d\n", file_name, file_size);

	if (file_size == 0 || file_size > CNSS_MAX_FILE_SIZE)
		return -EINVAL;

	fd = cnss_plat_ipc_init_file_data(file_name, file_buf, file_size,
					  file_size);
	if (!fd) {
		cnss_plat_ipc_err("Unable to initialize file transfer data\n");
		return -EINVAL;
	}
	scnprintf(ind.file_name, CNSS_PLAT_IPC_QMI_MAX_FILE_NAME_LEN_V01, "%s",
		  fd->name);
	ind.file_size = fd->file_size;
	ind.file_id = fd->id;

	ret = qmi_send_indication
			(svc->svc_hdl, &qmi_client->client_sq,
			 CNSS_PLAT_IPC_QMI_FILE_UPLOAD_IND_V01,
			 CNSS_PLAT_IPC_QMI_FILE_UPLOAD_IND_MSG_V01_MAX_MSG_LEN,
			 cnss_plat_ipc_qmi_file_upload_ind_msg_v01_ei, &ind);

	if (ret < 0) {
		cnss_plat_ipc_err("QMI failed: %d\n", ret);
		goto end;
	}
	ret = wait_for_completion_timeout(&fd->complete,
					  msecs_to_jiffies
					  (CNSS_PLAT_IPC_QMI_FILE_TXN_TIMEOUT));
	if (!ret)
		cnss_plat_ipc_err("Timeout Uploading file: %s\n", fd->name);

end:
	ret = cnss_plat_ipc_deinit_file_data(fd);
	cnss_plat_ipc_dbg("Status: %d\n", ret);

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
	cnss_plat_ipc_dbg("File ID: %d Seg Index: %d\n", req_msg->file_id,
			  req_msg->seg_index);

	mutex_lock(&svc->file_idr_lock);
	fd = idr_find(&svc->file_idr, req_msg->file_id);
	mutex_unlock(&svc->file_idr_lock);
	if (!fd) {
		cnss_plat_ipc_err("Invalid File ID %d\n", req_msg->file_id);
		return;
	}

	if (req_msg->seg_index != fd->seg_index) {
		cnss_plat_ipc_err("File %s transfer segment failure\n", fd->name);
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

	cnss_plat_ipc_dbg("ID: %d Seg ID: %d Len: %d End: %d\n", resp->file_id,
			  resp->seg_index, resp->seg_buf_len, resp->end);

	ret = qmi_send_response
		(svc->svc_hdl, sq, txn,
		CNSS_PLAT_IPC_QMI_FILE_UPLOAD_RESP_V01,
		CNSS_PLAT_IPC_QMI_FILE_UPLOAD_RESP_MSG_V01_MAX_MSG_LEN,
		cnss_plat_ipc_qmi_file_upload_resp_msg_v01_ei,
		resp);

	if (ret < 0) {
		cnss_plat_ipc_err("QMI failed: %d\n", ret);
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
 * @client_id: User space QMI IPC client ID. Also works as
 *		array index for QMI client context
 * @file_mame: File name to get from platform data location
 * @buf: Pointer of the buffer to store file contents
 * @size: Provides the size of buffer. It is updated to reflect the file size
 *        at the end of file download.
 */
int cnss_plat_ipc_qmi_file_download(enum cnss_plat_ipc_qmi_client_id_v01
				    client_id, char *file_name, char *buf,
				    u32 *size)
{
	struct cnss_plat_ipc_qmi_file_download_ind_msg_v01 ind;
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	struct cnss_plat_ipc_qmi_client_ctx *qmi_client;
	int ret;
	struct cnss_plat_ipc_file_data *fd;

	if (client_id > CNSS_PLAT_IPC_MAX_QMI_CLIENTS) {
		cnss_plat_ipc_err("Invalid Client ID: %d\n", client_id);
		return -EINVAL;
	}

	qmi_client = &svc->qmi_client_ctx[client_id];

	if (!qmi_client->client_connected || !file_name || !buf)
		return -EINVAL;

	fd = cnss_plat_ipc_init_file_data(file_name, buf, *size, 0);
	if (!fd) {
		cnss_plat_ipc_err("Unable to initialize file transfer data\n");
		return -EINVAL;
	}

	scnprintf(ind.file_name, CNSS_PLAT_IPC_QMI_MAX_FILE_NAME_LEN_V01, "%s",
		  file_name);
	ind.file_id = fd->id;

	ret = qmi_send_indication
		(svc->svc_hdl, &qmi_client->client_sq,
		 CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_IND_V01,
		 CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_IND_MSG_V01_MAX_MSG_LEN,
		 cnss_plat_ipc_qmi_file_download_ind_msg_v01_ei, &ind);

	if (ret < 0) {
		cnss_plat_ipc_err("QMI failed: %d\n", ret);
		goto end;
	}
	ret = wait_for_completion_timeout(&fd->complete,
					  msecs_to_jiffies
					  (CNSS_PLAT_IPC_QMI_FILE_TXN_TIMEOUT));
	if (!ret)
		cnss_plat_ipc_err("Timeout downloading file:%s\n", fd->name);

end:
	*size = fd->file_size;
	ret = cnss_plat_ipc_deinit_file_data(fd);
	cnss_plat_ipc_dbg("Status: %d Size: %d\n", ret, *size);

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
	cnss_plat_ipc_dbg("File ID: %d Size: %d Seg Len: %d Index: %d End: %d\n",
			  req_msg->file_id, req_msg->file_size,
			  req_msg->seg_buf_len, req_msg->seg_index,
			  req_msg->end);

	mutex_lock(&svc->file_idr_lock);
	fd = idr_find(&svc->file_idr, req_msg->file_id);
	mutex_unlock(&svc->file_idr_lock);
	if (!fd) {
		cnss_plat_ipc_err("Invalid File ID: %d\n", req_msg->file_id);
		return;
	}

	if (req_msg->file_size > fd->buf_size) {
		cnss_plat_ipc_err("File %s size %d larger than buffer size %d\n",
				  fd->name, req_msg->file_size, fd->buf_size);
		goto file_error;
	}
	if (req_msg->seg_buf_len > CNSS_PLAT_IPC_QMI_MAX_DATA_SIZE_V01 ||
	    ((req_msg->seg_buf_len + fd->file_size) > fd->buf_size)) {
		cnss_plat_ipc_err("Segment buf ID: %d buffer size %d not allowed\n",
				  req_msg->seg_index, req_msg->seg_buf_len);
		goto file_error;
	}
	if (req_msg->seg_index != fd->seg_index) {
		cnss_plat_ipc_err("File %s transfer segment failure\n",
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
		cnss_plat_ipc_err("QMI failed: %d\n", ret);

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
	struct cnss_plat_ipc_daemon_config *cfg = &daemon_cfg;
	int ret = 0;

	req_msg =
		(struct cnss_plat_ipc_qmi_init_setup_req_msg_v01 *)decoded_msg;
	cnss_plat_ipc_dbg("MAC: %d HW_TRC: %d CAL: %d\n",
			  req_msg->dms_mac_addr_supported,
			  req_msg->qdss_hw_trace_override,
			  req_msg->cal_file_available_bitmask);

	cfg->dms_mac_addr_supported = req_msg->dms_mac_addr_supported;
	cfg->qdss_hw_trace_override = req_msg->qdss_hw_trace_override;
	cfg->cal_file_available_bitmask = req_msg->cal_file_available_bitmask;

	ret = qmi_send_response
		(svc->svc_hdl, sq, txn,
		 CNSS_PLAT_IPC_QMI_INIT_SETUP_RESP_V01,
		 CNSS_PLAT_IPC_QMI_INIT_SETUP_RESP_MSG_V01_MAX_MSG_LEN,
		 cnss_plat_ipc_qmi_init_setup_resp_msg_v01_ei, &resp);
	if (ret < 0)
		cnss_plat_ipc_err("%s: QMI failed: %d\n", __func__, ret);
}

/**
 * cnss_plat_ipc_qmi_reg_client_req_handler() - Register QMI client
 * @handle: Pointer to QMI handle
 * @sq: QMI socket
 * @txn: QMI transaction pointer
 * @decoded_msg: Pointer to decoded QMI message
 *
 * Handles the userspace QMI client registration.
 *
 * Return: None
 */
static void
cnss_plat_ipc_qmi_reg_client_req_handler(struct qmi_handle *handle,
					 struct sockaddr_qrtr *sq,
					 struct qmi_txn *txn,
					 const void *decoded_msg)
{
	struct cnss_plat_ipc_qmi_reg_client_req_msg_v01 *req_msg;
	struct cnss_plat_ipc_qmi_reg_client_resp_msg_v01 resp = {0};
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	struct cnss_plat_ipc_qmi_client_ctx *qmi_client = svc->qmi_client_ctx;
	int ret = 0;

	req_msg =
		(struct cnss_plat_ipc_qmi_reg_client_req_msg_v01 *)decoded_msg;

	if (req_msg->client_id_valid) {
		if (req_msg->client_id <= CNSS_PLAT_IPC_MAX_QMI_CLIENTS &&
		    !qmi_client[req_msg->client_id].client_connected) {
			cnss_plat_ipc_info
				("%s: QMI Client Connected. QMI Socket Node: %d Port: %d ID: %d\n",
				 __func__, sq->sq_node, sq->sq_port,
				 req_msg->client_id);
			qmi_client[req_msg->client_id].client_sq = *sq;
			qmi_client[req_msg->client_id].client_connected = true;
			cnss_plat_ipc_qmi_update_user
					((enum cnss_plat_ipc_qmi_client_id_v01)
					req_msg->client_id);
		} else {
			cnss_plat_ipc_err("QMI client already connected or Invalid client id\n");
			return;
		}
	}

	ret = qmi_send_response
		(svc->svc_hdl, sq, txn,
		 CNSS_PLAT_IPC_QMI_REG_CLIENT_RESP_V01,
		 CNSS_PLAT_IPC_QMI_REG_CLIENT_RESP_MSG_V01_MAX_MSG_LEN,
		 cnss_plat_ipc_qmi_reg_client_resp_msg_v01_ei, &resp);

	if (ret < 0)
		cnss_plat_ipc_err("QMI failed: %d\n", ret);
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
	struct cnss_plat_ipc_qmi_client_ctx *qmi_client =
					svc->qmi_client_ctx;
	struct cnss_plat_ipc_file_data *fd;
	u32 file_id;
	int i;

	if (svc->svc_hdl != handle) {
		cnss_plat_ipc_err("Invalid QMI Handle\n");
		return;
	}

	for (i = 0; i <= CNSS_PLAT_IPC_MAX_QMI_CLIENTS; i++) {
		if (qmi_client[i].client_connected &&
		    qmi_client[i].client_sq.sq_node == node &&
		    qmi_client[i].client_sq.sq_port == port) {
			cnss_plat_ipc_err
				("%s: QMI client disconnect. QMI Socket Node:%d Port:%d ID: %d\n",
				 __func__, node, port, i);
			qmi_client[i].client_sq.sq_node = 0;
			qmi_client[i].client_sq.sq_port = 0;
			qmi_client[i].client_sq.sq_family = 0;
			qmi_client[i].client_connected = false;

			/* Daemon killed. Fail any download / upload in progress. This
			 * will also free stale fd
			 */
			mutex_lock(&svc->file_idr_lock);
			idr_for_each_entry(&svc->file_idr, fd, file_id)
				complete(&fd->complete);
			mutex_unlock(&svc->file_idr_lock);
			cnss_plat_ipc_qmi_update_user(i);
		}
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
	struct cnss_plat_ipc_qmi_client_ctx *qmi_client =
					svc->qmi_client_ctx;
	int i;

	for (i = 0; i <= CNSS_PLAT_IPC_MAX_QMI_CLIENTS; i++) {
		cnss_plat_ipc_qmi_disconnect_cb
					(handle, node,
					 qmi_client[i].client_sq.sq_port);
	}
}

static struct qmi_ops cnss_plat_ipc_qmi_ops = {
	/* inform a client that all clients from a node are gone */
	.bye = cnss_plat_ipc_qmi_bye_cb,
	.del_client = cnss_plat_ipc_qmi_disconnect_cb,
};

static struct qmi_msg_handler cnss_plat_ipc_qmi_req_handlers[] = {
	{
		.type = QMI_REQUEST,
		.msg_id = CNSS_PLAT_IPC_QMI_REG_CLIENT_REQ_V01,
		.ei = cnss_plat_ipc_qmi_reg_client_req_msg_v01_ei,
		.decoded_size =
		sizeof(struct cnss_plat_ipc_qmi_reg_client_req_msg_v01),
		.fn = cnss_plat_ipc_qmi_reg_client_req_handler,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = CNSS_PLAT_IPC_QMI_INIT_SETUP_REQ_V01,
		.ei = cnss_plat_ipc_qmi_init_setup_req_msg_v01_ei,
		.decoded_size =
		sizeof(struct cnss_plat_ipc_qmi_init_setup_req_msg_v01),
		.fn = cnss_plat_ipc_qmi_init_setup_req_handler,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_REQ_V01,
		.ei = cnss_plat_ipc_qmi_file_download_req_msg_v01_ei,
		.decoded_size =
		sizeof(struct cnss_plat_ipc_qmi_file_download_req_msg_v01),
		.fn = cnss_plat_ipc_qmi_file_download_req_handler,
	},
	{
		.type = QMI_REQUEST,
		.msg_id = CNSS_PLAT_IPC_QMI_FILE_UPLOAD_REQ_V01,
		.ei = cnss_plat_ipc_qmi_file_upload_req_msg_v01_ei,
		.decoded_size =
		sizeof(struct cnss_plat_ipc_qmi_file_upload_req_msg_v01),
		.fn = cnss_plat_ipc_qmi_file_upload_req_handler,
	},
	{}
};

/**
 * cnss_plat_ipc_qmi_daemon_config() - Get daemon config for CNSS platform
 *
 * Return: Pointer to daemon client config
 */
struct cnss_plat_ipc_daemon_config *cnss_plat_ipc_qmi_daemon_config(void)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	struct cnss_plat_ipc_qmi_client_ctx *qmi_client =
		&svc->qmi_client_ctx[CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01];

	if (!qmi_client->client_connected)
		return NULL;

	return &daemon_cfg;
}
EXPORT_SYMBOL(cnss_plat_ipc_qmi_daemon_config);

/**
 * cnss_plat_ipc_register() - Register for QMI IPC client status update
 * @client_id: User space QMI IPC client ID. Also works as
 *		array index for QMI client context
 * @connect_update_cb: Function pointer for callback
 * @cb_ctx: Callback context
 *
 * Return: 0 on success, negative error value otherwise
 */
int cnss_plat_ipc_register(enum cnss_plat_ipc_qmi_client_id_v01 client_id,
			   cnss_plat_ipc_connection_update
			   connection_update_cb, void *cb_ctx)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	struct cnss_plat_ipc_qmi_client_ctx *qmi_client;
	int num_user;

	if (client_id > CNSS_PLAT_IPC_MAX_QMI_CLIENTS) {
		cnss_plat_ipc_err("Invalid Client ID: %d\n", client_id);
		return -EINVAL;
	}

	qmi_client = &svc->qmi_client_ctx[client_id];
	num_user = qmi_client->num_user;

	if (num_user >= CNSS_PLAT_IPC_MAX_USER) {
		cnss_plat_ipc_err("Max Service users reached\n");
		return -EINVAL;
	}

	qmi_client->connection_update_cb[num_user] = connection_update_cb;
	qmi_client->cb_ctx[num_user] = cb_ctx;
	qmi_client->num_user++;

	return 0;
}
EXPORT_SYMBOL(cnss_plat_ipc_register);

/**
 * cnss_plat_ipc_unregister() - Unregister QMI IPC client status callback
 * @client_id: User space QMI IPC client ID. Also works as
 *		array index for QMI client context
 * @cb_cbt: Callback context provided during registration
 *
 * Return: None
 */
void cnss_plat_ipc_unregister(enum cnss_plat_ipc_qmi_client_id_v01 client_id,
			      void *cb_ctx)
{
	struct cnss_plat_ipc_qmi_svc_ctx *svc = &plat_ipc_qmi_svc;
	struct cnss_plat_ipc_qmi_client_ctx *qmi_client;
	int i;

	if (client_id > CNSS_PLAT_IPC_MAX_QMI_CLIENTS) {
		cnss_plat_ipc_err("Invalid Client ID: %d\n", client_id);
		return;
	}

	qmi_client = &svc->qmi_client_ctx[client_id];

	for (i = 0; i < qmi_client->num_user; i++) {
		if (qmi_client->cb_ctx[i] == cb_ctx) {
			qmi_client->cb_ctx[i] = NULL;
			qmi_client->connection_update_cb[i] = NULL;
			qmi_client->num_user--;
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
		cnss_plat_ipc_err("Failed to init QMI handle after %d ms * %d, err = %d\n",
				  ret, QMI_INIT_RETRY_DELAY_MS,
				  QMI_INIT_RETRY_MAX_TIMES);
		goto free_svc_hdl;
	}

	ret = qmi_add_server(svc->svc_hdl,
			     CNSS_PLATFORM_SERVICE_ID_V01,
			     CNSS_PLATFORM_SERVICE_VERS_V01, 0);
	if (ret < 0) {
		cnss_plat_ipc_err("Server add fail: %d\n", ret);
		goto release_svc_hdl;
	}

	cnss_plat_ipc_info("CNSS Platform IPC QMI Service is started\n");
	idr_init(&svc->file_idr);
	mutex_init(&svc->file_idr_lock);
	return;

release_svc_hdl:
	qmi_handle_release(svc->svc_hdl);
free_svc_hdl:
	kfree(svc->svc_hdl);
}

/**
 * cnss_plat_ipc_is_valid_dt_node_found - Check if valid device tree node
 *                                        present
 *
 * Valid device tree node means a node with "qcom,wlan" property present
 * and "status" property not disabled.
 *
 * Return: true if valid device tree node found, false if not found
 */
static bool cnss_plat_ipc_is_valid_dt_node_found(void)
{
	struct device_node *dn = NULL;

	for_each_node_with_property(dn, "qcom,wlan") {
		if (of_device_is_available(dn))
			break;
	}

	if (dn)
		return true;

	return false;
}

static DECLARE_WORK(cnss_plat_ipc_init_work, cnss_plat_ipc_init_fn);

static int __init cnss_plat_ipc_qmi_svc_init(void)
{
	if (!cnss_plat_ipc_is_valid_dt_node_found())
		return -ENODEV;

	/* Schedule a work to do real init to avoid blocking here */
	cnss_plat_ipc_logging_init();
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

	cnss_plat_ipc_logging_deinit();
}

module_init(cnss_plat_ipc_qmi_svc_init);
module_exit(cnss_plat_ipc_qmi_svc_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CNSS Platform IPC QMI Service");
