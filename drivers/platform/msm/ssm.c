/* Copyright (c) 2013-2014, 2016 The Linux Foundation. All rights reserved.
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
/*
 * QTI Secure Service Module(SSM) driver
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/ion.h>
#include <linux/types.h>
#include <linux/elf.h>
#include <linux/platform_device.h>
#include <linux/msm_ion.h>
#include <linux/platform_data/qcom_ssm.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/smd.h>

#include "../../misc/qseecom_kernel.h"
#include "ssm.h"

/* Macros */
#define SSM_DEV_NAME			"ssm"
#define MPSS_SUBSYS			0
#define SSM_INFO_CMD_ID			1
#define MAX_APP_NAME_SIZE		32
#define SSM_MSG_LEN			200
#define SSM_MSG_FIELD_LEN		11
#define ATOM_MSG_LEN			(SSM_MSG_FIELD_LEN + SSM_MSG_LEN + 40)

#define TZAPP_NAME			"SsmApp"
#define CHANNEL_NAME			"SSM_RTR_MODEM_APPS"

/* SSM driver structure.*/
struct ssm_driver {
	int32_t app_status;
	int32_t update_status;
	unsigned char *channel_name;
	unsigned char *smd_buffer;
	struct device *dev;
	smd_channel_t *ch;
	struct work_struct ipc_work;
	struct mutex mutex;
	struct qseecom_handle *qseecom_handle;
	struct tzapp_get_mode_info_rsp *resp;
	bool key_status;
	bool ready;
};

static struct ssm_driver *ssm_drv;

static unsigned int getint(char *buff, unsigned long *res)
{
	char value[SSM_MSG_FIELD_LEN];

	memcpy(value, buff, SSM_MSG_FIELD_LEN);
	value[SSM_MSG_FIELD_LEN - 1] = '\0';

	return kstrtoul(skip_spaces(value), 10, res);
}

/*
 * Setup CMD/RSP pointers.
 */
static void setup_cmd_rsp_buffers(struct qseecom_handle *handle, void **cmd,
		int *cmd_len, void **resp, int *resp_len)
{
	*cmd = handle->sbuf;
	if (*cmd_len & QSEECOM_ALIGN_MASK)
		*cmd_len = QSEECOM_ALIGN(*cmd_len);

	*resp = handle->sbuf + *cmd_len;
	if (*resp_len & QSEECOM_ALIGN_MASK)
		*resp_len = QSEECOM_ALIGN(*resp_len);
}

/*
 * Send packet to modem over SMD channel.
 */
static int update_modem(enum ssm_ipc_req ipc_req, struct ssm_driver *ssm,
		int length, char *data)
{
	unsigned int packet_len = length + SSM_MSG_FIELD_LEN;
	int rc = 0, count;

	snprintf(ssm->smd_buffer, SSM_MSG_FIELD_LEN + 1, "%10u|", ipc_req);
	memcpy(ssm->smd_buffer + SSM_MSG_FIELD_LEN, data, length);

	if (smd_write_avail(ssm->ch) < packet_len) {
		dev_err(ssm->dev, "Not enough space dropping request\n");
		rc = -ENOSPC;
		goto out;
	}

	count = smd_write(ssm->ch, ssm->smd_buffer, packet_len);
	if (count < packet_len) {
		dev_err(ssm->dev, "smd_write failed for %d\n", ipc_req);
		rc = -EIO;
	}

out:
	return rc;
}

/*
 * Header Format
 * Each member of header is of 10 byte (ASCII).
 * Each entry is separated by '|' delimiter.
 * |<-10 bytes->|<-10 bytes->|
 * |-------------------------|
 * | IPC code   | error code |
 * |-------------------------|
 *
 */
static int decode_packet(char *buffer, struct ssm_common_msg *pkt)
{
	int rc;

	rc =  getint(buffer, (unsigned long *)&pkt->ipc_req);
	if (rc < 0)
		return -EINVAL;

	buffer += SSM_MSG_FIELD_LEN;
	rc =  getint(buffer, (unsigned long *)&pkt->err_code);
	if (rc < 0)
		return -EINVAL;

	dev_dbg(ssm_drv->dev, "req %d error code %d\n",
			pkt->ipc_req, pkt->err_code);
	return 0;
}

static void process_message(struct ssm_common_msg pkt, struct ssm_driver *ssm)
{

	switch (pkt.ipc_req) {

	case SSM_MTOA_MODE_UPDATE_STATUS:
		if (pkt.err_code) {
			dev_err(ssm->dev, "Modem mode update failed\n");
			ssm->update_status = FAILED;
		} else
			ssm->update_status = SUCCESS;

		dev_dbg(ssm->dev, "Modem mode update status %d\n",
								pkt.err_code);
		break;

	default:
		dev_dbg(ssm->dev, "Invalid message\n");
		break;
	};
}

/*
 * Work function to handle and process packets coming from modem.
 */
static void ssm_app_modem_work_fn(struct work_struct *work)
{
	int sz, rc;
	struct ssm_common_msg pkt;
	struct ssm_driver *ssm;

	ssm = container_of(work, struct ssm_driver, ipc_work);

	mutex_lock(&ssm->mutex);
	sz = smd_cur_packet_size(ssm->ch);
	if ((sz < SSM_MSG_FIELD_LEN) || (sz > ATOM_MSG_LEN)) {
		dev_dbg(ssm_drv->dev, "Garbled message size\n");
		goto unlock;
	}

	if (smd_read_avail(ssm->ch) < sz) {
		dev_err(ssm_drv->dev, "SMD error data in channel\n");
		goto unlock;
	}

	if (smd_read(ssm->ch, ssm->smd_buffer, sz) != sz) {
		dev_err(ssm_drv->dev, "Incomplete data\n");
		goto unlock;
	}

	rc = decode_packet(ssm->smd_buffer, &pkt);
	if (rc < 0) {
		dev_err(ssm_drv->dev, "Corrupted header\n");
		goto unlock;
	}

	process_message(pkt, ssm);

unlock:
	mutex_unlock(&ssm->mutex);
}

/*
 * MODEM-APPS smd channel callback function.
 */
static void modem_request(void *ctxt, unsigned event)
{
	struct ssm_driver *ssm;

	ssm = (struct ssm_driver *)ctxt;

	switch (event) {
	case SMD_EVENT_OPEN:
	case SMD_EVENT_CLOSE:
		dev_dbg(ssm->dev, "SMD port status changed\n");
		break;
	case SMD_EVENT_DATA:
		if (smd_read_avail(ssm->ch) > 0)
			schedule_work(&ssm->ipc_work);
		break;
	};
}

/*
 * Load SSM application in TZ and start application:
 */
static int ssm_load_app(struct ssm_driver *ssm)
{
	int rc;

	/* Load the APP */
	rc = qseecom_start_app(&ssm->qseecom_handle, TZAPP_NAME, SZ_4K);
	if (rc < 0) {
		dev_err(ssm->dev, "Unable to load SSM app\n");
		ssm->app_status = FAILED;
		return -EIO;
	}

	ssm->app_status = SUCCESS;
	return 0;
}

static struct ssm_platform_data *populate_ssm_pdata(struct device *dev)
{
	struct ssm_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(struct ssm_platform_data),
								GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->need_key_exchg =
		of_property_read_bool(dev->of_node, "qcom,need-keyexhg");

	pdata->channel_name = CHANNEL_NAME;

	return pdata;
}

static int ssm_probe(struct platform_device *pdev)
{
	int rc;
	struct ssm_platform_data *pdata;
	struct ssm_driver *drv;

	if (pdev->dev.of_node)
		pdata = populate_ssm_pdata(&pdev->dev);
	else
		pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "Empty platform data\n");
		return -ENOMEM;
	}

	drv = devm_kzalloc(&pdev->dev, sizeof(struct ssm_driver),
								GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	/* Allocate response buffer */
	drv->resp = devm_kzalloc(&pdev->dev,
			sizeof(struct tzapp_get_mode_info_rsp),
			GFP_KERNEL);
	if (!drv->resp) {
		devm_kfree(&pdev->dev, drv);
		rc = -ENOMEM;
		goto exit;
	}

	/* Initialize the driver structure */
	drv->app_status = RETRY;
	drv->ready = false;
	drv->update_status = FAILED;
	mutex_init(&drv->mutex);
	drv->key_status = !pdata->need_key_exchg;
	drv->channel_name = (char *)pdata->channel_name;
	INIT_WORK(&drv->ipc_work, ssm_app_modem_work_fn);

	/* Allocate memory for smd buffer */
	drv->smd_buffer = devm_kzalloc(&pdev->dev,
			(sizeof(char) * ATOM_MSG_LEN), GFP_KERNEL);
	if (!drv->smd_buffer) {
		devm_kfree(&pdev->dev, drv->resp);
		devm_kfree(&pdev->dev, drv);
		rc = -ENOMEM;
		goto exit;
	}

	drv->dev = &pdev->dev;
	ssm_drv = drv;
	platform_set_drvdata(pdev, ssm_drv);

	dev_dbg(&pdev->dev, "probe success\n");
	return 0;

exit:
	mutex_destroy(&drv->mutex);
	platform_set_drvdata(pdev, NULL);
	return rc;

}

static int ssm_remove(struct platform_device *pdev)
{

	if (!ssm_drv)
		return 0;
	/*
	 * Step to exit
	 * 1. set ready to 0 (oem access closed).
	 * 2. Close SMD modem connection closed.
	 * 3. cleanup ion.
	 */
	ssm_drv->ready = false;
	smd_close(ssm_drv->ch);
	flush_work(&ssm_drv->ipc_work);

	/* Shutdown tzapp */
	dev_dbg(&pdev->dev, "Shutting down TZapp\n");
	qseecom_shutdown_app(&ssm_drv->qseecom_handle);

	/* freeing the memory allocations
	for the driver and the buffer */
	devm_kfree(&pdev->dev, ssm_drv->smd_buffer);
	devm_kfree(&pdev->dev, ssm_drv->resp);
	devm_kfree(&pdev->dev, ssm_drv);

	return 0;
}

static struct of_device_id ssm_match_table[] = {
	{
		.compatible = "qcom,ssm",
	},
	{}
};

static struct platform_driver ssm_pdriver = {
	.probe          = ssm_probe,
	.remove         = ssm_remove,
	.driver = {
		.name   = SSM_DEV_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = ssm_match_table,
	},
};
module_platform_driver(ssm_pdriver);

/*
 * Interface for external OEM driver.
 * This interface supports following functionalities:
 * 1. Set mode (encrypted mode and it's length is passed as parameter).
 * 2. Set mode from TZ (read encrypted mode from TZ)
 * 3. Get status of mode update.
 *
 */
int ssm_oem_driver_intf(int cmd, char *mode, int len)
{
	int rc, req_len, resp_len;
	struct tzapp_get_mode_info_req *get_mode_req;
	struct tzapp_get_mode_info_rsp *get_mode_resp;

	/* If ssm_drv is NULL, probe failed */
	if (!ssm_drv)
		return -ENODEV;

	mutex_lock(&ssm_drv->mutex);

	if (ssm_drv->app_status == RETRY) {
		/* Load TZAPP */
		rc = ssm_load_app(ssm_drv);
		if (rc) {
			rc = -ENODEV;
			goto unlock;
		}
	} else if (ssm_drv->app_status == FAILED) {
		rc = -ENODEV;
		goto unlock;
	}

	/* Open modem SMD interface */
	if (!ssm_drv->ready) {
		rc = smd_named_open_on_edge(ssm_drv->channel_name,
							SMD_APPS_MODEM,
							&ssm_drv->ch,
							ssm_drv,
							modem_request);
		if (rc) {
			rc = -EAGAIN;
			goto unlock;
		} else
			ssm_drv->ready = true;
	}

	/* Try again modem key-exchange not yet done.*/
	if (!ssm_drv->key_status) {
		rc = -EAGAIN;
		goto unlock;
	}

	/* Set return status to success */
	rc = 0;

	switch (cmd) {
	case SSM_READY:
		break;

	case SSM_MODE_INFO_READY:
		ssm_drv->update_status = RETRY;
		/* Fill command structure */
		req_len = sizeof(struct tzapp_get_mode_info_req);
		resp_len = sizeof(struct tzapp_get_mode_info_rsp);
		setup_cmd_rsp_buffers(ssm_drv->qseecom_handle,
				(void **)&get_mode_req, &req_len,
				(void **)&get_mode_resp, &resp_len);
		get_mode_req->tzapp_ssm_cmd = GET_ENC_MODE;

		rc = qseecom_set_bandwidth(ssm_drv->qseecom_handle, 1);
		if (rc) {
			ssm_drv->update_status = FAILED;
			dev_err(ssm_drv->dev, "set bandwidth failed\n");
			rc = -EIO;
			break;
		}
		rc = qseecom_send_command(ssm_drv->qseecom_handle,
				(void *)get_mode_req, req_len,
				(void *)get_mode_resp, resp_len);
		if (rc || get_mode_resp->status) {
			ssm_drv->update_status = FAILED;
			break;
		}
		rc = qseecom_set_bandwidth(ssm_drv->qseecom_handle, 0);
		if (rc) {
			ssm_drv->update_status = FAILED;
			dev_err(ssm_drv->dev, "clear bandwidth failed\n");
			rc = -EIO;
			break;
		}

		if (get_mode_resp->enc_mode_len > ENC_MODE_MAX_SIZE) {
			ssm_drv->update_status = FAILED;
			rc = -EINVAL;
			break;
		}
		/* Send mode_info to modem */
		rc = update_modem(SSM_ATOM_MODE_UPDATE, ssm_drv,
				get_mode_resp->enc_mode_len,
				get_mode_resp->enc_mode_info);
		if (rc)
			ssm_drv->update_status = FAILED;
		break;

	case SSM_SET_MODE:
		ssm_drv->update_status = RETRY;

		if (len > ENC_MODE_MAX_SIZE) {
			ssm_drv->update_status = FAILED;
			rc = -EINVAL;
			break;
		}
		memcpy(ssm_drv->resp->enc_mode_info, mode, len);
		ssm_drv->resp->enc_mode_len = len;

		/* Send mode_info to modem */
		rc = update_modem(SSM_ATOM_MODE_UPDATE, ssm_drv,
				ssm_drv->resp->enc_mode_len,
				ssm_drv->resp->enc_mode_info);
		if (rc)
			ssm_drv->update_status = FAILED;
		break;

	case SSM_GET_MODE_STATUS:
		rc = ssm_drv->update_status;
		break;

	default:
		rc = -EINVAL;
		dev_err(ssm_drv->dev, "Invalid command\n");
		break;
	};

unlock:
	mutex_unlock(&ssm_drv->mutex);
	return rc;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QTI Secure Service Module");

