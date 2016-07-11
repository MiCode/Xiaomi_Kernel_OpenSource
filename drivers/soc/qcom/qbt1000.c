/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/input.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <uapi/linux/qbt1000.h>
#include <soc/qcom/scm.h>
#include "qseecom_kernel.h"

#include <soc/qcom/msm_qmi_interface.h>
#define QBT1000_SNS_SERVICE_ID		0x138 /* From sns_common_v01.idl */
#define QBT1000_SNS_SERVICE_VER_ID	1
#define QBT1000_SNS_INSTANCE_INST_ID	0

static void qbt1000_qmi_clnt_svc_arrive(struct work_struct *work);
static void qbt1000_qmi_clnt_svc_exit(struct work_struct *work);
static void qbt1000_qmi_clnt_recv_msg(struct work_struct *work);

#define QBT1000_DEV "qbt1000"
#define QBT1000_IN_DEV_NAME "qbt1000_key_input"
#define QBT1000_IN_DEV_VERSION 0x0100

/* definitions for the TZ_BLSP_MODIFY_OWNERSHIP_ID system call */
#define TZ_BLSP_MODIFY_OWNERSHIP_ARGINFO    2
#define TZ_BLSP_MODIFY_OWNERSHIP_SVC_ID     4 /* resource locking service */
#define TZ_BLSP_MODIFY_OWNERSHIP_FUNC_ID    3

enum sensor_connection_types {
	SPI,
	SSC_SPI,
};

/*
 * shared buffer size - init with max value,
 * user space will provide new value upon tz app load
 */
static uint32_t g_app_buf_size = SZ_256K;

struct qbt1000_drvdata {
	struct class	*qbt1000_class;
	struct cdev	qbt1000_cdev;
	struct device	*dev;
	char		*qbt1000_node;
	enum sensor_connection_types sensor_conn_type;
	struct clk	**clocks;
	unsigned	clock_count;
	uint8_t		clock_state;
	uint8_t		ssc_state;
	unsigned	root_clk_idx;
	unsigned	frequency;
	atomic_t	available;
	struct mutex	mutex;
	struct input_dev *in_dev;
	struct work_struct qmi_svc_arrive;
	struct work_struct qmi_svc_exit;
	struct work_struct qmi_svc_rcv_msg;
	struct qmi_handle *qmi_handle;
	struct notifier_block qmi_svc_notifier;
	uint32_t	tz_subsys_id;
	uint32_t	ssc_subsys_id;
	uint32_t	ssc_spi_port;
	uint32_t	ssc_spi_port_slave_index;
	struct wakeup_source w_lock;
};
#define W_LOCK_DELAY_MS (2000)

/**
 * get_cmd_rsp_buffers() - Function sets cmd & rsp buffer pointers and
 *                         aligns buffer lengths
 * @hdl:	index of qseecom_handle
 * @cmd:	req buffer - set to qseecom_handle.sbuf
 * @cmd_len:	ptr to req buffer len
 * @rsp:	rsp buffer - set to qseecom_handle.sbuf + offset
 * @rsp_len:	ptr to rsp buffer len
 *
 * Return: 0 on success. Error code on failure.
 */
static int get_cmd_rsp_buffers(struct qseecom_handle *hdl,
	void **cmd,
	uint32_t *cmd_len,
	void **rsp,
	uint32_t *rsp_len)
{
	/* 64 bytes alignment for QSEECOM */
	*cmd_len = ALIGN(*cmd_len, 64);
	*rsp_len = ALIGN(*rsp_len, 64);

	if ((*rsp_len + *cmd_len) > g_app_buf_size)
		return -ENOMEM;

	*cmd = hdl->sbuf;
	*rsp = hdl->sbuf + *cmd_len;

	return 0;
}

/**
 * clocks_on() - Function votes for SPI and AHB clocks to be on and sets
 *               the clk rate to predetermined value for SPI.
 * @drvdata:	ptr to driver data
 *
 * Return: 0 on success. Error code on failure.
 */
static int clocks_on(struct qbt1000_drvdata *drvdata)
{
	int rc = 0;
	int index;

	mutex_lock(&drvdata->mutex);

	if (!drvdata->clock_state) {
		for (index = 0; index < drvdata->clock_count; index++) {
			rc = clk_prepare_enable(drvdata->clocks[index]);
			if (rc) {
				dev_err(drvdata->dev, "%s: Clk idx:%d fail\n",
					__func__, index);
				goto unprepare;
			}
			if (index == drvdata->root_clk_idx) {
				rc = clk_set_rate(drvdata->clocks[index],
					drvdata->frequency);
				if (rc) {
					dev_err(drvdata->dev,
					 "%s: Failed clk  set rate at idx:%d\n",
					 __func__, index);
					goto unprepare;
				}
			}
		}
		drvdata->clock_state = 1;
	}
	goto end;

unprepare:
	for (--index; index >= 0; index--)
		clk_disable_unprepare(drvdata->clocks[index]);

end:
	mutex_unlock(&drvdata->mutex);
	return rc;
}

/**
 * clocks_off() - Function votes for SPI and AHB clocks to be off
 * @drvdata:	ptr to driver data
 *
 * Return: None
 */
static void clocks_off(struct qbt1000_drvdata *drvdata)
{
	int index;

	mutex_lock(&drvdata->mutex);
	if (drvdata->clock_state) {
		for (index = 0; index < drvdata->clock_count; index++)
			clk_disable_unprepare(drvdata->clocks[index]);
		drvdata->clock_state = 0;
	}
	mutex_unlock(&drvdata->mutex);
}

#define SNS_QFP_OPEN_REQ_V01 0x0020
#define SNS_QFP_OPEN_RESP_V01 0x0020
#define SNS_QFP_KEEP_ALIVE_REQ_V01 0x0022
#define SNS_QFP_KEEP_ALIVE_RESP_V01 0x0022
#define SNS_QFP_CLOSE_REQ_V01 0x0021
#define SNS_QFP_CLOSE_RESP_V01 0x0021
#define SNS_QFP_VERSION_REQ_V01 0x0001
#define TIMEOUT_MS			(500)

struct sns_common_resp_s_v01 {
	uint8_t sns_result_t;
	uint8_t sns_err_t;
};

struct sns_qfp_open_req_msg_v01 {
	uint8_t port_id;
	uint8_t slave_index;
	uint32_t freq;
};
#define SNS_QFP_OPEN_REQ_MSG_V01_MAX_MSG_LEN 15

struct sns_qfp_open_resp_msg_v01 {
	struct sns_common_resp_s_v01 resp;
};
#define SNS_QFP_OPEN_RESP_MSG_V01_MAX_MSG_LEN 5

struct sns_qfp_close_req_msg_v01 {
	char placeholder;
};
#define SNS_QFP_CLOSE_REQ_MSG_V01_MAX_MSG_LEN 0

struct sns_qfp_close_resp_msg_v01 {
	struct sns_common_resp_s_v01 resp;
};
#define SNS_QFP_CLOSE_RESP_MSG_V01_MAX_MSG_LEN 5

struct sns_qfp_keep_alive_req_msg_v01 {
	uint8_t enable;
};
#define SNS_QFP_KEEP_ALIVE_REQ_MSG_V01_MAX_MSG_LEN 4

struct sns_qfp_keep_alive_resp_msg_v01 {
	struct sns_common_resp_s_v01 resp;
};
#define SNS_QFP_KEEP_ALIVE_RESP_MSG_V01_MAX_MSG_LEN 5

static struct elem_info sns_common_resp_s_v01_ei[] = {
	{
		.data_type    = QMI_UNSIGNED_1_BYTE,
		.elem_len     = 1,
		.elem_size    = sizeof(uint8_t),
		.is_array     = NO_ARRAY,
		.tlv_type     = 0,
		.offset       = offsetof(struct sns_common_resp_s_v01,
					 sns_result_t),
	},
	{
		.data_type    = QMI_UNSIGNED_1_BYTE,
		.elem_len     = 1,
		.elem_size    = sizeof(uint8_t),
		.is_array     = NO_ARRAY,
		.tlv_type     = 0,
		.offset       = offsetof(struct sns_common_resp_s_v01,
					 sns_err_t),
	},
	{
		.data_type    = QMI_EOTI,
		.is_array     = NO_ARRAY,
		.is_array     = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info sns_qfp_open_req_msg_v01_ei[] = {
	{
		.data_type    = QMI_UNSIGNED_1_BYTE,
		.elem_len     = 1,
		.elem_size    = sizeof(uint8_t),
		.is_array     = NO_ARRAY,
		.tlv_type     = 0x01,
		.offset       = offsetof(struct sns_qfp_open_req_msg_v01,
					 port_id),
	},
	{
		.data_type    = QMI_UNSIGNED_1_BYTE,
		.elem_len     = 1,
		.elem_size    = sizeof(uint8_t),
		.is_array     = NO_ARRAY,
		.tlv_type     = 0x02,
		.offset       = offsetof(struct sns_qfp_open_req_msg_v01,
					 slave_index),
	},
	{
		.data_type    = QMI_UNSIGNED_4_BYTE,
		.elem_len     = 1,
		.elem_size    = sizeof(uint32_t),
		.is_array     = NO_ARRAY,
		.tlv_type     = 0x03,
		.offset       = offsetof(struct sns_qfp_open_req_msg_v01,
					 freq),
	},
	{
		.data_type    = QMI_EOTI,
		.is_array     = NO_ARRAY,
		.is_array     = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info sns_qfp_open_resp_msg_v01_ei[] = {
	{
		.data_type    = QMI_STRUCT,
		.elem_len     = 1,
		.elem_size    = sizeof(struct sns_common_resp_s_v01),
		.is_array     = NO_ARRAY,
		.tlv_type     = 0x01,
		.offset       = offsetof(struct sns_qfp_open_resp_msg_v01,
					 resp),
		.ei_array     = sns_common_resp_s_v01_ei,
	},
	{
		.data_type    = QMI_EOTI,
		.is_array     = NO_ARRAY,
		.is_array     = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info sns_qfp_close_req_msg_v01_ei[] = {
	{
		.data_type    = QMI_EOTI,
		.is_array     = NO_ARRAY,
		.is_array     = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info sns_qfp_close_resp_msg_v01_ei[] = {
	{
		.data_type    = QMI_STRUCT,
		.elem_len     = 1,
		.elem_size    = sizeof(struct sns_common_resp_s_v01),
		.is_array     = NO_ARRAY,
		.tlv_type     = 0x01,
		.offset       = offsetof(struct sns_qfp_close_resp_msg_v01,
					 resp),
		.ei_array     = sns_common_resp_s_v01_ei,
	},
	{
		.data_type    = QMI_EOTI,
		.is_array     = NO_ARRAY,
		.is_array     = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info sns_qfp_keep_alive_req_msg_v01_ei[] = {
	{
		.data_type    = QMI_UNSIGNED_1_BYTE,
		.elem_len     = 1,
		.elem_size    = sizeof(uint8_t),
		.is_array     = NO_ARRAY,
		.tlv_type     = 0x01,
		.offset       = offsetof(struct sns_qfp_keep_alive_req_msg_v01,
					 enable),
	},
	{
		.data_type    = QMI_EOTI,
		.is_array     = NO_ARRAY,
		.is_array     = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info sns_qfp_keep_alive_resp_msg_v01_ei[] = {
	{
		.data_type    = QMI_STRUCT,
		.elem_len     = 1,
		.elem_size    = sizeof(struct sns_common_resp_s_v01),
		.is_array     = NO_ARRAY,
		.tlv_type     = 0x01,
		.offset       = offsetof(struct sns_qfp_keep_alive_resp_msg_v01,
					 resp),
		.ei_array     = sns_common_resp_s_v01_ei,
	},
	{
		.data_type    = QMI_EOTI,
		.is_array     = NO_ARRAY,
		.is_array     = QMI_COMMON_TLV_TYPE,
	},
};

static int qbt1000_sns_open_req(struct qbt1000_drvdata *drvdata)
{
	struct sns_qfp_open_req_msg_v01 req;
	struct sns_qfp_open_resp_msg_v01 resp = { { 0, 0 } };

	struct msg_desc req_desc, resp_desc;
	int ret = -EINVAL;

	mutex_lock(&drvdata->mutex);

	if (!drvdata->qmi_handle) {
		dev_info(drvdata->dev,
			 "%s: QMI service unavailable. Skipping QMI requests\n",
			 __func__);
		goto err;
	}

	req.port_id = drvdata->ssc_spi_port;
	req.slave_index = drvdata->ssc_spi_port_slave_index;
	req.freq = drvdata->frequency;

	req_desc.msg_id = SNS_QFP_OPEN_REQ_V01;
	req_desc.max_msg_len = SNS_QFP_OPEN_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.ei_array = sns_qfp_open_req_msg_v01_ei;

	resp_desc.msg_id = SNS_QFP_OPEN_RESP_V01;
	resp_desc.max_msg_len = SNS_QFP_OPEN_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.ei_array = sns_qfp_open_resp_msg_v01_ei;

	ret = qmi_send_req_wait(drvdata->qmi_handle,
				&req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				TIMEOUT_MS);

	if (ret < 0) {
		dev_err(drvdata->dev, "%s: QMI send req failed %d\n", __func__,
			ret);
		goto err;
	}

	if (resp.resp.sns_result_t != 0) {
		dev_err(drvdata->dev, "%s: QMI request failed %d %d\n",
			__func__, resp.resp.sns_result_t, resp.resp.sns_err_t);
		ret = -EREMOTEIO;
		goto err;
	}

err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}

static int qbt1000_sns_keep_alive_req(struct qbt1000_drvdata *drvdata,
				      uint8_t enable)
{
	struct sns_qfp_keep_alive_req_msg_v01 req;
	struct sns_qfp_keep_alive_resp_msg_v01 resp = { { 0, 0 } };

	struct msg_desc req_desc, resp_desc;
	int ret = -EINVAL;

	mutex_lock(&drvdata->mutex);

	if (!drvdata->qmi_handle) {
		dev_info(drvdata->dev,
			 "%s: QMI service unavailable. Skipping QMI requests\n",
			 __func__);
		goto err;
	}

	req.enable = enable;

	req_desc.msg_id = SNS_QFP_KEEP_ALIVE_REQ_V01;
	req_desc.max_msg_len = SNS_QFP_KEEP_ALIVE_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.ei_array = sns_qfp_keep_alive_req_msg_v01_ei;

	resp_desc.msg_id = SNS_QFP_KEEP_ALIVE_RESP_V01;
	resp_desc.max_msg_len = SNS_QFP_KEEP_ALIVE_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.ei_array = sns_qfp_keep_alive_resp_msg_v01_ei;

	ret = qmi_send_req_wait(drvdata->qmi_handle,
				&req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				TIMEOUT_MS);

	if (ret < 0) {
		dev_err(drvdata->dev, "%s: QMI send req failed %d\n", __func__,
			ret);
		goto err;
	}

	if (resp.resp.sns_result_t != 0) {
		dev_err(drvdata->dev, "%s: QMI request failed %d %d\n",
			__func__, resp.resp.sns_result_t, resp.resp.sns_err_t);
		ret = -EREMOTEIO;
		goto err;
	}

err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}

static int qbt1000_sns_close_req(struct qbt1000_drvdata *drvdata)
{
	struct sns_qfp_close_req_msg_v01 req;
	struct sns_qfp_close_resp_msg_v01 resp = { { 0, 0 } };

	struct msg_desc req_desc, resp_desc;
	int ret = -EINVAL;

	mutex_lock(&drvdata->mutex);

	if (!drvdata->qmi_handle) {
		dev_info(drvdata->dev,
			 "%s: QMI service unavailable. Skipping QMI requests\n",
			 __func__);
		goto err;
	}

	req.placeholder = 1;

	req_desc.msg_id = SNS_QFP_CLOSE_REQ_V01;
	req_desc.max_msg_len = SNS_QFP_CLOSE_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.ei_array = sns_qfp_close_req_msg_v01_ei;

	resp_desc.msg_id = SNS_QFP_CLOSE_RESP_V01;
	resp_desc.max_msg_len = SNS_QFP_CLOSE_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.ei_array = sns_qfp_close_resp_msg_v01_ei;

	ret = qmi_send_req_wait(drvdata->qmi_handle,
				&req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				TIMEOUT_MS);

	if (ret < 0) {
		dev_err(drvdata->dev, "%s: QMI send req failed %d\n", __func__,
			ret);
		goto err;
	}

	if (resp.resp.sns_result_t != 0) {
		dev_err(drvdata->dev, "%s: QMI request failed %d %d\n",
			__func__, resp.resp.sns_result_t, resp.resp.sns_err_t);
		ret = -EREMOTEIO;
		goto err;
	}

err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}

static void qbt1000_sns_notify(struct qmi_handle *handle,
			     enum qmi_event_type event, void *notify_priv)
{

	struct qbt1000_drvdata *drvdata =
					(struct qbt1000_drvdata *)notify_priv;

	switch (event) {
	case QMI_RECV_MSG:
		schedule_work(&drvdata->qmi_svc_rcv_msg);
		break;
	default:
		break;
	}
}

static int qbt1000_qmi_svc_event_notify(struct notifier_block *this,
					unsigned long code,
					void *_cmd)
{
	struct qbt1000_drvdata *drvdata = container_of(this,
						struct qbt1000_drvdata,
						qmi_svc_notifier);

	switch (code) {
	case QMI_SERVER_ARRIVE:
		schedule_work(&drvdata->qmi_svc_arrive);
		break;
	case QMI_SERVER_EXIT:
		schedule_work(&drvdata->qmi_svc_exit);
		break;
	default:
		break;
	}
	return 0;
}

static void qbt1000_qmi_ind_cb(struct qmi_handle *handle, unsigned int msg_id,
			void *msg, unsigned int msg_len, void *ind_cb_priv)
{
}

static void qbt1000_qmi_connect_to_service(struct qbt1000_drvdata *drvdata)
{
	int rc = 0;

	/* Create a Local client port for QMI communication */
	drvdata->qmi_handle = qmi_handle_create(qbt1000_sns_notify, drvdata);
	if (!drvdata->qmi_handle) {
		dev_err(drvdata->dev, "%s: QMI client handle alloc failed\n",
			__func__);
		return;
	}

	rc = qmi_connect_to_service(drvdata->qmi_handle,
				    QBT1000_SNS_SERVICE_ID,
				    QBT1000_SNS_SERVICE_VER_ID,
				    QBT1000_SNS_INSTANCE_INST_ID);
	if (rc < 0) {
		dev_err(drvdata->dev, "%s: Could not connect to SNS service\n",
			__func__);
		goto err;
	}

	rc = qmi_register_ind_cb(drvdata->qmi_handle, qbt1000_qmi_ind_cb,
							(void *)drvdata);
	if (rc < 0) {
		dev_err(drvdata->dev, "%s: Could not register the QMI ind cb\n",
			__func__);
		goto err;
	}

	return;

err:
	qmi_handle_destroy(drvdata->qmi_handle);
	drvdata->qmi_handle = NULL;
}

static void qbt1000_qmi_clnt_svc_arrive(struct work_struct *work)
{
	struct qbt1000_drvdata *drvdata = container_of(work,
					struct qbt1000_drvdata, qmi_svc_arrive);

	qbt1000_qmi_connect_to_service(drvdata);
}

static void qbt1000_qmi_clnt_svc_exit(struct work_struct *work)
{
	struct qbt1000_drvdata *drvdata = container_of(work,
					struct qbt1000_drvdata, qmi_svc_exit);

	qmi_handle_destroy(drvdata->qmi_handle);
	drvdata->qmi_handle = NULL;
}

static void qbt1000_qmi_clnt_recv_msg(struct work_struct *work)
{
	struct qbt1000_drvdata *drvdata = container_of(work,
				struct qbt1000_drvdata, qmi_svc_rcv_msg);

	if (qmi_recv_msg(drvdata->qmi_handle) < 0)
		dev_err(drvdata->dev, "%s: Error receiving QMI message\n",
		 __func__);
}

static int qbt1000_set_blsp_ownership(struct qbt1000_drvdata *drvdata,
				      uint8_t owner_id)
{
	int rc = 0;
	struct scm_desc desc = {0};

	desc.arginfo = TZ_BLSP_MODIFY_OWNERSHIP_ARGINFO;
	desc.args[0] = drvdata->ssc_spi_port + 11;
	desc.args[1] = owner_id;


	rc = scm_call2(SCM_SIP_FNID(TZ_BLSP_MODIFY_OWNERSHIP_SVC_ID,
				    TZ_BLSP_MODIFY_OWNERSHIP_FUNC_ID),
				    &desc);

	if (rc < 0)
		dev_err(drvdata->dev, "%s: Error blsp ownership switch to %d\n",
			__func__, owner_id);

	return rc;
}

/**
 * qbt1000_open() - Function called when user space opens device.
 * Successful if driver not currently open and clocks turned on.
 * @inode:	ptr to inode object
 * @file:	ptr to file object
 *
 * Return: 0 on success. Error code on failure.
 */
static int qbt1000_open(struct inode *inode, struct file *file)
{
	int rc = 0;

	struct qbt1000_drvdata *drvdata = container_of(inode->i_cdev,
						   struct qbt1000_drvdata,
						   qbt1000_cdev);
	file->private_data = drvdata;

	/* disallowing concurrent opens */
	if (!atomic_dec_and_test(&drvdata->available)) {
		atomic_inc(&drvdata->available);
		return -EBUSY;
	}

	if (drvdata->sensor_conn_type == SPI) {
		rc = clocks_on(drvdata);
	} else if (drvdata->sensor_conn_type == SSC_SPI) {
		rc = qbt1000_sns_open_req(drvdata);
		if (rc < 0) {
			dev_err(drvdata->dev, "%s: Error sensor open request\n",
				__func__);
			goto out;
		}
		rc = qbt1000_sns_keep_alive_req(drvdata, 1);
		if (rc < 0) {
			dev_err(drvdata->dev,
				"%s: Error sensor keep-alive request\n",
				 __func__);
			qbt1000_sns_close_req(drvdata);
			goto out;
		}
		rc = qbt1000_set_blsp_ownership(drvdata, drvdata->tz_subsys_id);
		if (rc < 0) {
			dev_err(drvdata->dev,
				"%s: Error setting blsp ownership\n", __func__);
			qbt1000_sns_keep_alive_req(drvdata, 0);
			qbt1000_sns_close_req(drvdata);
			goto out;
		}
		drvdata->ssc_state = 1;
	}

out:
	/* increment atomic counter on failure */
	if (rc)
		atomic_inc(&drvdata->available);

	return rc;
}

/**
 * qbt1000_release() - Function called when user space closes device.
 *                     SPI Clocks turn off.
 * @inode:	ptr to inode object
 * @file:	ptr to file object
 *
 * Return: 0 on success. Error code on failure.
 */
static int qbt1000_release(struct inode *inode, struct file *file)
{
	struct qbt1000_drvdata *drvdata = file->private_data;

	if (drvdata->sensor_conn_type == SPI) {
		clocks_off(drvdata);
	} else if (drvdata->sensor_conn_type == SSC_SPI) {
		qbt1000_set_blsp_ownership(drvdata, drvdata->ssc_subsys_id);
		qbt1000_sns_keep_alive_req(drvdata, 0);
		qbt1000_sns_close_req(drvdata);
		drvdata->ssc_state = 0;
	}

	atomic_inc(&drvdata->available);
	return 0;
}

/**
 * qbt1000_ioctl() - Function called when user space calls ioctl.
 * @file:	struct file - not used
 * @cmd:	cmd identifier:QBT1000_LOAD_APP,QBT1000_UNLOAD_APP,
 *              QBT1000_SEND_TZCMD
 * @arg:	ptr to relevant structe: either qbt1000_app or
 *              qbt1000_send_tz_cmd depending on which cmd is passed
 *
 * Return: 0 on success. Error code on failure.
 */
static long qbt1000_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int rc = 0;
	void __user *priv_arg = (void __user *)arg;
	struct qbt1000_drvdata *drvdata;

	if (IS_ERR(priv_arg)) {
		dev_err(drvdata->dev, "%s: invalid user space pointer %lu\n",
			__func__, arg);
		return -EINVAL;
	}

	drvdata = file->private_data;
	pm_runtime_get_sync(drvdata->dev);
	mutex_lock(&drvdata->mutex);
	if (((drvdata->sensor_conn_type == SPI) && (!drvdata->clock_state)) ||
	    ((drvdata->sensor_conn_type == SSC_SPI) && (!drvdata->ssc_state))) {
		rc = -EPERM;
		dev_err(drvdata->dev, "%s: IOCTL call in invalid state\n",
			__func__);
		goto end;
	}

	switch (cmd) {
	case QBT1000_LOAD_APP:
	{
		struct qbt1000_app app;
		struct qseecom_handle *app_handle;

		if (copy_from_user(&app, priv_arg,
			sizeof(app)) != 0) {
			rc = -ENOMEM;
			dev_err(drvdata->dev,
				"%s: Failed copy from user space-LOAD\n",
				__func__);
			goto end;
		}

		if (!app.app_handle) {
			dev_err(drvdata->dev, "%s: LOAD app_handle is null\n",
				__func__);
			rc = -EINVAL;
			goto end;
		}

		/* start the TZ app */
		rc = qseecom_start_app(&app_handle, app.name, app.size);
		if (rc == 0) {
			g_app_buf_size = app.size;
		} else {
			dev_err(drvdata->dev, "%s: App %s failed to load\n",
				__func__, app.name);
			goto end;
		}

		/* copy the app handle to user */
		rc = copy_to_user((void __user *)app.app_handle, &app_handle,
			sizeof(*app.app_handle));

		if (rc != 0) {
			dev_err(drvdata->dev,
				"%s: Failed copy 2us LOAD rc:%d\n",
				 __func__, rc);
			rc = -ENOMEM;
			goto end;
		}

		break;
	}
	case QBT1000_UNLOAD_APP:
	{
		struct qbt1000_app app;
		struct qseecom_handle *app_handle;

		if (copy_from_user(&app, priv_arg,
			sizeof(app)) != 0) {
			rc = -ENOMEM;
			dev_err(drvdata->dev,
				"%s: Failed copy from user space-UNLOAD\n",
				 __func__);
			goto end;
		}

		if (!app.app_handle) {
			dev_err(drvdata->dev, "%s: UNLOAD app_handle is null\n",
				__func__);
			rc = -EINVAL;
			goto end;
		}

		rc = copy_from_user(&app_handle, app.app_handle,
			sizeof(app_handle));

		if (rc != 0) {
			dev_err(drvdata->dev,
				"%s: Failed copy from user space-UNLOAD handle rc:%d\n",
				 __func__, rc);
			rc = -ENOMEM;
			goto end;
		}

		/* if the app hasn't been loaded already, return err */
		if (!app_handle) {
			dev_err(drvdata->dev, "%s: App not loaded\n",
				__func__);
			rc = -EINVAL;
			goto end;
		}

		rc = qseecom_shutdown_app(&app_handle);
		if (rc != 0) {
			dev_err(drvdata->dev, "%s: App failed to shutdown\n",
				__func__);
			goto end;
		}

		/* copy the app handle (should be null) to user */
		rc = copy_to_user((void __user *)app.app_handle, &app_handle,
			sizeof(*app.app_handle));

		if (rc != 0) {
			dev_err(drvdata->dev,
				"%s: Failed copy 2us UNLOAD rc:%d\n",
				 __func__, rc);
			rc = -ENOMEM;
			goto end;
		}

		break;
	}
	case QBT1000_SEND_TZCMD:
	{
		void *aligned_cmd;
		void *aligned_rsp;
		uint32_t aligned_cmd_len;
		uint32_t aligned_rsp_len;

		struct qbt1000_send_tz_cmd tzcmd;

		if (copy_from_user(&tzcmd, priv_arg,
			sizeof(tzcmd))
				!= 0) {
			rc = -ENOMEM;
			dev_err(drvdata->dev,
				"%s: Failed copy from user space-LOAD\n",
				 __func__);
			goto end;
		}

		/* if the app hasn't been loaded already, return err */
		if (!tzcmd.app_handle) {
			dev_err(drvdata->dev, "%s: App not loaded\n",
				__func__);
			rc = -EINVAL;
			goto end;
		}

		/* init command and response buffers and align lengths */
		aligned_cmd_len = tzcmd.req_buf_len;
		aligned_rsp_len = tzcmd.rsp_buf_len;
		rc = get_cmd_rsp_buffers(tzcmd.app_handle,
			(void **)&aligned_cmd,
			&aligned_cmd_len,
			(void **)&aligned_rsp,
			&aligned_rsp_len);
		if (rc != 0)
			goto end;

		rc = copy_from_user(aligned_cmd, (void __user *)tzcmd.req_buf,
				tzcmd.req_buf_len);
		if (rc != 0) {
			dev_err(drvdata->dev,
				"%s: Failure to copy user space buf %d\n",
				 __func__, rc);
			goto end;
		}

		/* send cmd to TZ */
		rc = qseecom_send_command(tzcmd.app_handle,
			aligned_cmd,
			aligned_cmd_len,
			aligned_rsp,
			aligned_rsp_len);

		if (rc == 0) {
			/* copy rsp buf back to user space unaligned buffer */
			rc = copy_to_user((void __user *)tzcmd.rsp_buf,
				 aligned_rsp, tzcmd.rsp_buf_len);
			if (rc != 0) {
				dev_err(drvdata->dev,
					"%s: Failed copy 2us rc:%d bytes %d:\n",
					 __func__, rc, tzcmd.rsp_buf_len);
				goto end;
			}
		} else {
			dev_err(drvdata->dev, "%s: Failure to send tz cmd %d\n",
				__func__, rc);
			goto end;
		}

		break;
	}
	default:
		dev_err(drvdata->dev, "%s: Invalid cmd %d\n", __func__, cmd);
		rc = -EINVAL;
		goto end;
	}

end:
	pm_runtime_mark_last_busy(drvdata->dev);
	pm_runtime_put_autosuspend(drvdata->dev);
	mutex_unlock(&drvdata->mutex);
	return rc;
}

static const struct file_operations qbt1000_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = qbt1000_ioctl,
	.open = qbt1000_open,
	.release = qbt1000_release
};

static int qbt1000_dev_register(struct qbt1000_drvdata *drvdata)
{
	dev_t dev_no;
	int ret = 0;
	size_t node_size;
	char *node_name = QBT1000_DEV;
	struct device *dev = drvdata->dev;
	struct device *device;

	node_size = strlen(node_name) + 1;

	drvdata->qbt1000_node = devm_kzalloc(dev, node_size, GFP_KERNEL);
	if (!drvdata->qbt1000_node) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	strlcpy(drvdata->qbt1000_node, node_name, node_size);

	ret = alloc_chrdev_region(&dev_no, 0, 1, drvdata->qbt1000_node);
	if (ret) {
		dev_err(drvdata->dev, "%s: alloc_chrdev_region failed %d\n",
			__func__, ret);
		goto err_alloc;
	}

	cdev_init(&drvdata->qbt1000_cdev, &qbt1000_fops);

	drvdata->qbt1000_cdev.owner = THIS_MODULE;
	ret = cdev_add(&drvdata->qbt1000_cdev, dev_no, 1);
	if (ret) {
		dev_err(drvdata->dev, "%s: cdev_add failed %d\n", __func__,
			ret);
		goto err_cdev_add;
	}

	drvdata->qbt1000_class = class_create(THIS_MODULE,
					   drvdata->qbt1000_node);
	if (IS_ERR(drvdata->qbt1000_class)) {
		ret = PTR_ERR(drvdata->qbt1000_class);
		dev_err(drvdata->dev, "%s: class_create failed %d\n", __func__,
			ret);
		goto err_class_create;
	}

	device = device_create(drvdata->qbt1000_class, NULL,
			       drvdata->qbt1000_cdev.dev, drvdata,
			       drvdata->qbt1000_node);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		dev_err(drvdata->dev, "%s: device_create failed %d\n",
			__func__, ret);
		goto err_dev_create;
	}

	return 0;

err_dev_create:
	class_destroy(drvdata->qbt1000_class);
err_class_create:
	cdev_del(&drvdata->qbt1000_cdev);
err_cdev_add:
	unregister_chrdev_region(drvdata->qbt1000_cdev.dev, 1);
err_alloc:
	return ret;
}

/**
 * qbt1000_create_input_device() - Function allocates an input
 * device, configures it for key events and registers it
 *
 * @drvdata:	ptr to driver data
 *
 * Return: 0 on success. Error code on failure.
 */
int qbt1000_create_input_device(struct qbt1000_drvdata *drvdata)
{
	int rc = 0;

	drvdata->in_dev = input_allocate_device();
	if (drvdata->in_dev == NULL) {
		dev_err(drvdata->dev, "%s: input_allocate_device() failed\n",
			__func__);
		rc = -ENOMEM;
		goto end;
	}

	drvdata->in_dev->name = QBT1000_IN_DEV_NAME;
	drvdata->in_dev->phys = NULL;
	drvdata->in_dev->id.bustype = BUS_HOST;
	drvdata->in_dev->id.vendor  = 0x0001;
	drvdata->in_dev->id.product = 0x0001;
	drvdata->in_dev->id.version = QBT1000_IN_DEV_VERSION;

	drvdata->in_dev->evbit[0] = BIT_MASK(EV_KEY) |  BIT_MASK(EV_ABS);
	drvdata->in_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	/* enable all 256 key events except to key 00 which is "KEY_RESERVED" */
	memset(drvdata->in_dev->keybit, 0xFE,
		   BIT_WORD(0x100)*sizeof(unsigned long));

	input_set_abs_params(drvdata->in_dev, ABS_X,
			     0,
			     1000,
			     0, 0);
	input_set_abs_params(drvdata->in_dev, ABS_Y,
			     0,
			     1000,
			     0, 0);

	rc = input_register_device(drvdata->in_dev);
	if (rc) {
		dev_err(drvdata->dev, "%s: input_reg_dev() failed %d\n",
			__func__, rc);
		goto end;
	}

end:
	if (rc)
		input_free_device(drvdata->in_dev);
	return rc;
}

static int qbt1000_read_spi_conn_properties(struct device_node *node,
					    struct qbt1000_drvdata *drvdata)
{
	int rc = 0;
	int index = 0;
	uint32_t rate;
	uint8_t clkcnt = 0;
	const char *clock_name;

	if ((node == NULL) || (drvdata == NULL))
		return -EINVAL;

	drvdata->sensor_conn_type = SPI;

	/* obtain number of clocks from hw config */
	clkcnt = of_property_count_strings(node, "clock-names");
	if (IS_ERR_VALUE(drvdata->clock_count)) {
			dev_err(drvdata->dev, "%s: Failed to get clock names\n",
				__func__);
			return -EINVAL;
	}

	/* sanity check for max clock count */
	if (clkcnt > 16) {
			dev_err(drvdata->dev, "%s: Invalid clock count %d\n",
				__func__, clkcnt);
			return -EINVAL;
	}

	/* alloc mem for clock array - auto free if probe fails */
	drvdata->clock_count = clkcnt;
	drvdata->clocks = devm_kzalloc(drvdata->dev,
				sizeof(struct clk *) * drvdata->clock_count,
				GFP_KERNEL);
	if (!drvdata->clocks) {
			dev_err(drvdata->dev,
				"%s: Failed to alloc memory for clocks\n",
				__func__);
			return -ENOMEM;
	}

	/* load clock names */
	for (index = 0; index < drvdata->clock_count; index++) {
			of_property_read_string_index(node,
					"clock-names",
					index, &clock_name);
			drvdata->clocks[index] = devm_clk_get(drvdata->dev,
							      clock_name);
			if (IS_ERR(drvdata->clocks[index])) {
				rc = PTR_ERR(drvdata->clocks[index]);
				if (rc != -EPROBE_DEFER)
					dev_err(drvdata->dev,
						"%s: Failed get %s\n",
						__func__, clock_name);
					return rc;
			}

			if (!strcmp(clock_name, "spi_clk"))
				drvdata->root_clk_idx = index;
	}

	/* read clock frequency */
	if (of_property_read_u32(node, "clock-frequency", &rate) == 0)
		drvdata->frequency = rate;

	return 0;
}

static int qbt1000_read_ssc_spi_conn_properties(struct device_node *node,
						struct qbt1000_drvdata *drvdata)
{
	int rc = 0;
	uint32_t rate;

	if ((node == NULL) || (drvdata == NULL))
		return -EINVAL;

	drvdata->sensor_conn_type = SSC_SPI;

	/* read SPI port id */
	if (of_property_read_u32(node, "qcom,spi-port-id",
				 &drvdata->ssc_spi_port) != 0)
			return -EINVAL;

	/* read SPI port slave index */
	if (of_property_read_u32(node, "qcom,spi-port-slave-index",
				 &drvdata->ssc_spi_port_slave_index) != 0)
		return -EINVAL;

	/* read TZ subsys id */
	if (of_property_read_u32(node, "qcom,tz-subsys-id",
				 &drvdata->tz_subsys_id) != 0)
		return -EINVAL;

	/* read SSC subsys id */
	if (of_property_read_u32(node, "qcom,ssc-subsys-id",
				 &drvdata->ssc_subsys_id) != 0)
		return -EINVAL;

	/* read clock frequency */
	if (of_property_read_u32(node, "clock-frequency", &rate) != 0)
		return -EINVAL;

	drvdata->frequency = rate;

	INIT_WORK(&drvdata->qmi_svc_arrive, qbt1000_qmi_clnt_svc_arrive);
	INIT_WORK(&drvdata->qmi_svc_exit, qbt1000_qmi_clnt_svc_exit);
	INIT_WORK(&drvdata->qmi_svc_rcv_msg, qbt1000_qmi_clnt_recv_msg);

	drvdata->qmi_svc_notifier.notifier_call = qbt1000_qmi_svc_event_notify;
	drvdata->qmi_handle = NULL;
	rc = qmi_svc_event_notifier_register(QBT1000_SNS_SERVICE_ID,
					     QBT1000_SNS_SERVICE_VER_ID,
					     QBT1000_SNS_INSTANCE_INST_ID,
					     &drvdata->qmi_svc_notifier);
	if (rc < 0)
		dev_err(drvdata->dev, "%s: QMI service notifier reg failed\n",
			__func__);

	return rc;
}

/**
 * qbt1000_probe() - Function loads hardware config from device tree
 * @pdev:	ptr to platform device object
 *
 * Return: 0 on success. Error code on failure.
 */
static int qbt1000_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct qbt1000_drvdata *drvdata;
	int rc = 0;
	int child_node_cnt = 0;
	struct device_node *child_node;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	/* get child count */
	child_node_cnt = of_get_child_count(pdev->dev.of_node);
	if (child_node_cnt != 1) {
		dev_err(drvdata->dev, "%s: Invalid number of child nodes %d\n",
			__func__, child_node_cnt);
		rc = -EINVAL;
		goto end;
	}

	for_each_child_of_node(pdev->dev.of_node, child_node) {
		if (!of_node_cmp(child_node->name,
				 "qcom,fingerprint-sensor-spi-conn")) {
			/* sensor connected to regular SPI port */
			rc = qbt1000_read_spi_conn_properties(child_node,
							      drvdata);
			if (rc != 0) {
				dev_err(drvdata->dev,
					"%s: Failed to read SPI conn prop\n",
					__func__);
				goto end;
			}
		} else if (!of_node_cmp(child_node->name,
				"qcom,fingerprint-sensor-ssc-spi-conn")) {
			/* sensor connected to SSC SPI port */
			rc = qbt1000_read_ssc_spi_conn_properties(child_node,
								  drvdata);
			if (rc != 0) {
				dev_err(drvdata->dev,
					"%s: Failed to read SPI conn prop\n",
					__func__);
				goto end;
			}
		} else {
			dev_err(drvdata->dev, "%s: Invalid child node %s\n",
				__func__, child_node->name);
			rc = -EINVAL;
			goto end;
		}
	}

	atomic_set(&drvdata->available, 1);

	mutex_init(&drvdata->mutex);
	wakeup_source_init(&drvdata->w_lock, "qbt_wake_source");
	pm_runtime_set_autosuspend_delay(dev, W_LOCK_DELAY_MS);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_enable(dev);

	rc = qbt1000_dev_register(drvdata);
	if (rc < 0)
		goto end;

	rc = qbt1000_create_input_device(drvdata);
	if (rc < 0)
		goto end;

end:
	return rc;
}

static int qbt1000_remove(struct platform_device *pdev)
{
	struct qbt1000_drvdata *drvdata = platform_get_drvdata(pdev);

	input_unregister_device(drvdata->in_dev);

	if (drvdata->sensor_conn_type == SPI) {
		clocks_off(drvdata);
	} else if (drvdata->sensor_conn_type == SSC_SPI) {
		qbt1000_set_blsp_ownership(drvdata, drvdata->ssc_subsys_id);
		qbt1000_sns_keep_alive_req(drvdata, 0);
		qbt1000_sns_close_req(drvdata);
		qmi_handle_destroy(drvdata->qmi_handle);
		qmi_svc_event_notifier_unregister(QBT1000_SNS_SERVICE_ID,
						  QBT1000_SNS_SERVICE_VER_ID,
						  QBT1000_SNS_INSTANCE_INST_ID,
						  &drvdata->qmi_svc_notifier);
	}
	mutex_destroy(&drvdata->mutex);

	device_destroy(drvdata->qbt1000_class, drvdata->qbt1000_cdev.dev);
	class_destroy(drvdata->qbt1000_class);
	cdev_del(&drvdata->qbt1000_cdev);
	unregister_chrdev_region(drvdata->qbt1000_cdev.dev, 1);
	return 0;
}

static int qbt1000_suspend(struct device *dev)
{
	int rc = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct qbt1000_drvdata *drvdata = platform_get_drvdata(pdev);

	/*
	 * Returning an error code if driver currently making a TZ call.
	 * Note: The purpose of this driver is to ensure that the clocks are on
	 * while making a TZ call. Hence the clock check to determine if the
	 * driver will allow suspend to occur.
	 */
	if (!mutex_trylock(&drvdata->mutex))
		return -EBUSY;
	if (((drvdata->sensor_conn_type == SPI) && (drvdata->clock_state)) ||
	    ((drvdata->sensor_conn_type == SSC_SPI) && (drvdata->ssc_state)))
		rc = -EBUSY;
	mutex_unlock(&drvdata->mutex);

	return rc;
}

static int qbt1000_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct qbt1000_drvdata *drvdata = platform_get_drvdata(pdev);

	__pm_relax(&drvdata->w_lock);

	return 0;
};

static int qbt1000_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct qbt1000_drvdata *drvdata = platform_get_drvdata(pdev);

	__pm_stay_awake(&drvdata->w_lock);

	return 0;
};

static struct of_device_id qbt1000_match[] = {
	{ .compatible = "qcom,qbt1000" },
	{}
};

static const struct dev_pm_ops qbt1000_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(qbt1000_suspend, NULL)
	SET_RUNTIME_PM_OPS(qbt1000_runtime_suspend,
			   qbt1000_runtime_resume, NULL)
};

static struct platform_driver qbt1000_plat_driver = {
	.probe = qbt1000_probe,
	.remove = qbt1000_remove,
	.driver = {
		.name = "qbt1000",
		.owner = THIS_MODULE,
		.pm = &qbt1000_dev_pm_ops,
		.of_match_table = qbt1000_match,
	},
};

static int qbt1000_init(void)
{
	return platform_driver_register(&qbt1000_plat_driver);
}
module_init(qbt1000_init);

static void qbt1000_exit(void)
{
	platform_driver_unregister(&qbt1000_plat_driver);
}
module_exit(qbt1000_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. QBT1000 driver");
