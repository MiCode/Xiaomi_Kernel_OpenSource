/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
 * Qualcomm Secure Service Module(SSM) driver
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
#include <linux/firmware.h>
#include <linux/elf.h>
#include <linux/platform_device.h>
#include <linux/msm_ion.h>
#include <linux/platform_data/qcom_ssm.h>
#include <mach/scm.h>
#include <mach/msm_smd.h>

#include "ssm.h"

/* Macros */
#define SSM_DEV_NAME			"ssm"
#define MPSS_SUBSYS			0
#define SSM_INFO_CMD_ID			1
#define QSEOS_CHECK_VERSION_CMD		0x00001803

#define MAX_APP_NAME_SIZE		32
#define SSM_MSG_LEN			(104  + 4) /* bytes + pad */
#define SSM_MSG_FIELD_LEN		11
#define SSM_HEADER_LEN			(SSM_MSG_FIELD_LEN * 4)
#define ATOM_MSG_LEN			(SSM_HEADER_LEN + SSM_MSG_LEN)
#define FIRMWARE_NAME			"ssmapp"
#define TZAPP_NAME			"SsmApp"
#define CHANNEL_NAME			"SSM_RTR"

#define ALIGN_BUFFER(size)		((size + 4095) & ~4095)

/* SSM driver structure.*/
struct ssm_driver {
	int32_t app_id;
	int32_t app_status;
	int32_t update_status;
	int32_t atom_replay;
	int32_t mtoa_replay;
	uint32_t buff_len;
	unsigned char *channel_name;
	unsigned char *smd_buffer;
	struct ion_client *ssm_ion_client;
	struct ion_handle *ssm_ion_handle;
	struct tzapp_get_mode_info_rsp *resp;
	struct device *dev;
	smd_channel_t *ch;
	ion_phys_addr_t buff_phys;
	void *buff_virt;
	dev_t ssm_device_no;
	struct work_struct ipc_work;
	struct mutex mutex;
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
 * Send packet to modem over SMD channel.
 */
static int update_modem(enum ssm_ipc_req ipc_req, struct ssm_driver *ssm,
		int length, char *data)
{
	unsigned int packet_len = SSM_HEADER_LEN + length + 1;
	int rc = 0;

	ssm->atom_replay += 1;
	snprintf(ssm->smd_buffer, SSM_HEADER_LEN + 1, "%10u|%10u|%10u|%10u|"
			, packet_len, ssm->atom_replay, ipc_req, length);
	memcpy(ssm->smd_buffer + SSM_HEADER_LEN, data, length);

	ssm->smd_buffer[packet_len - 1] = '|';

	if (smd_write_avail(ssm->ch) < packet_len) {
		dev_err(ssm->dev, "Not enough space dropping request\n");
		rc = -ENOSPC;
	}

	rc = smd_write(ssm->ch, ssm->smd_buffer, packet_len);
	if (rc < packet_len) {
		dev_err(ssm->dev, "smd_write failed for %d\n", ipc_req);
		rc = -EIO;
	}

	return rc;
}

/*
 * Header Format
 * Each member of header is of 10 byte (ASCII).
 * Each entry is separated by '|' delimiter.
 * |<-10 bytes->|<-10 bytes->|<-10 bytes->|<-10 bytes->|<-10 bytes->|
 * |-----------------------------------------------------------------
 * | length     | replay no. | request    | msg_len    | message    |
 * |-----------------------------------------------------------------
 *
 */
static int  decode_header(char *buffer, int length,
		struct ssm_common_msg *pkt)
{
	int rc;

	rc =  getint(buffer, &pkt->pktlen);
	if (rc < 0)
		return -EINVAL;

	buffer += SSM_MSG_FIELD_LEN;
	rc =  getint(buffer, &pkt->replaynum);
	if (rc < 0)
		return -EINVAL;

	buffer += SSM_MSG_FIELD_LEN;
	rc =  getint(buffer, (unsigned long *)&pkt->ipc_req);
	if (rc < 0)
		return -EINVAL;

	buffer += SSM_MSG_FIELD_LEN;
	rc =  getint(buffer, &pkt->msg_len);
	if ((rc < 0) || (pkt->msg_len > SSM_MSG_LEN))
		return -EINVAL;

	pkt->msg = buffer + SSM_MSG_FIELD_LEN;

	dev_dbg(ssm_drv->dev, "len %lu rep %lu req %d msg_len %lu\n",
			pkt->pktlen, pkt->replaynum, pkt->ipc_req,
			pkt->msg_len);
	return 0;
}

/*
 * Decode address for storing the decryption key.
 * Only for Key Exchange
 * Message Format
 * |Length@Address|
 */
static int decode_message(char *msg, unsigned int len, unsigned long *length,
		unsigned long *address)
{
	int i = 0, rc = 0;
	char *buff;

	buff = kzalloc(len, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;
	while (i < len) {
		if (msg[i] == '@')
			break;
		i++;
	}
	if ((i < len) && (msg[i] == '@')) {
		memcpy(buff, msg, i);
		buff[i] = '\0';
		rc = kstrtoul(skip_spaces(buff), 10, length);
		if (rc || (length <= 0)) {
			rc = -EINVAL;
			goto exit;
		}
		memcpy(buff, &msg[i + 1], len - (i + 1));
		buff[len - i] = '\0';
		rc = kstrtoul(skip_spaces(buff), 10, address);
	} else
		rc = -EINVAL;

exit:
	kfree(buff);
	return rc;
}

static void process_message(int cmd, char *msg, int len,
		struct ssm_driver *ssm)
{
	int rc;
	unsigned long key_len = 0, key_add = 0, val;
	struct ssm_keyexchg_req req;

	switch (cmd) {
	case SSM_MTOA_KEY_EXCHANGE:
		if (len < 3) {
			dev_err(ssm->dev, "Invalid message\n");
			break;
		}

		if (ssm->key_status) {
			dev_err(ssm->dev, "Key exchange already done\n");
			break;
		}

		rc = decode_message(msg, len, &key_len, &key_add);
		if (rc) {
			rc = update_modem(SSM_ATOM_KEY_STATUS, ssm,
					1, "1");
			break;
		}

		/*
		 * We are doing key-exchange part here as it is very
		 * specific for this case. For all other tz
		 * communication we have generic function.
		 */
		req.ssid = MPSS_SUBSYS;
		req.address = (void *)key_add;
		req.length = key_len;
		req.status = (uint32_t *)ssm->buff_phys;

		*(unsigned int *)ssm->buff_virt = -1;
		rc = scm_call(KEY_EXCHANGE, 0x1, &req,
				sizeof(struct ssm_keyexchg_req), NULL, 0);
		if (rc) {
			dev_err(ssm->dev, "Call for key exchg failed %d", rc);
			rc = update_modem(SSM_ATOM_KEY_STATUS, ssm,
								1, "1");
		} else {
			/* Success encode packet and update modem */
			rc = update_modem(SSM_ATOM_KEY_STATUS, ssm,
					1, "0");
			ssm->key_status = true;
		}
		break;

	case SSM_MTOA_MODE_UPDATE_STATUS:
		msg[len] = '\0';
		rc = kstrtoul(skip_spaces(msg), 10, &val);
		if (val) {
			dev_err(ssm->dev, "Modem mode update failed\n");
			ssm->update_status = FAILED;
		} else
			ssm->update_status = SUCCESS;

		dev_dbg(ssm->dev, "Modem mode update status %lu\n", val);
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
	if ((sz <= 0) || (sz > ATOM_MSG_LEN)) {
		dev_dbg(ssm_drv->dev, "Garbled message size\n");
		goto unlock;
	}

	if (smd_read_avail(ssm->ch) < sz) {
		dev_err(ssm_drv->dev, "SMD error data in channel\n");
		goto unlock;
	}

	if (sz < SSM_HEADER_LEN) {
		dev_err(ssm_drv->dev, "Invalid packet\n");
		goto unlock;
	}

	if (smd_read(ssm->ch, ssm->smd_buffer, sz) != sz) {
		dev_err(ssm_drv->dev, "Incomplete data\n");
		goto unlock;
	}

	rc = decode_header(ssm->smd_buffer, sz, &pkt);
	if (rc < 0) {
		dev_err(ssm_drv->dev, "Corrupted header\n");
		goto unlock;
	}

	/* Check validity of message */
	if (ssm->mtoa_replay >= (int)pkt.replaynum) {
		dev_err(ssm_drv->dev, "Replay attack...\n");
		goto unlock;
	}

	if (pkt.msg[pkt.msg_len] != '|') {
		dev_err(ssm_drv->dev, "Garbled message\n");
		goto unlock;
	}

	ssm->mtoa_replay = pkt.replaynum;
	process_message(pkt.ipc_req, pkt.msg, pkt.msg_len, ssm);

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
		dev_info(ssm->dev, "Port %s\n",
			(event == SMD_EVENT_OPEN) ? "opened" : "closed");
		break;
	case SMD_EVENT_DATA:
		if (smd_read_avail(ssm->ch) > 0)
			schedule_work(&ssm->ipc_work);
		break;
	};
}

/*
 * Communication interface between ssm driver and TZ.
 */
static int tz_scm_call(struct ssm_driver *ssm, void *tz_req, int tz_req_len,
			void **tz_resp, int tz_resp_len)
{
	int rc;
	struct common_req req;
	struct common_resp resp;

	memcpy((void *)ssm->buff_virt, tz_req, tz_req_len);

	req.cmd_id = CLIENT_SEND_DATA_COMMAND;
	req.app_id = ssm->app_id;
	req.req_ptr = (void *)ssm->buff_phys;
	req.req_len = tz_req_len;
	req.resp_ptr = (void *)(ssm->buff_phys + tz_req_len);
	req.resp_len = tz_resp_len;

	rc = scm_call(SCM_SVC_TZSCHEDULER, 1, (const void *) &req,
			sizeof(req), (void *)&resp, sizeof(resp));
	if (rc) {
		dev_err(ssm->dev, "SCM call failed for data command\n");
		return rc;
	}

	if (resp.result != RESULT_SUCCESS) {
		dev_err(ssm->dev, "Data command response failure %d\n",
				resp.result);
		return -EINVAL;
	}

	*tz_resp = (void *)(ssm->buff_virt + tz_req_len);

	return rc;
}

/*
 * Load SSM application in TZ and start application:
 * 1. Check if SSM application is already loaded.
 * 2. Load SSM application firmware.
 * 3. Start SSM application in TZ.
 */
static int ssm_load_app(struct ssm_driver *ssm)
{
	unsigned char name[MAX_APP_NAME_SIZE], *pos;
	int rc, i, fw_count;
	uint32_t buff_len, size = 0, ion_len;
	struct check_app_req app_req;
	struct scm_resp app_resp;
	struct load_app app_img_info;
	const struct firmware **fw, *fw_mdt;
	const struct elf32_hdr *ehdr;
	const struct elf32_phdr *phdr;
	struct ion_handle *ion_handle;
	ion_phys_addr_t buff_phys;
	void *buff_virt;

	/* Check if TZ app already loaded */
	app_req.cmd_id = APP_LOOKUP_COMMAND;
	memcpy(app_req.app_name, TZAPP_NAME, MAX_APP_NAME_SIZE);

	rc = scm_call(SCM_SVC_TZSCHEDULER, 1, &app_req,
				sizeof(struct check_app_req),
				&app_resp, sizeof(app_resp));
	if (rc) {
		dev_err(ssm->dev, "SCM call failed for LOOKUP COMMAND\n");
		return -EINVAL;
	}

	if (app_resp.result == RESULT_FAILURE)
		ssm->app_id = 0;
	else
		ssm->app_id = app_resp.data;

	if (ssm->app_id) {
		rc = 0;
		dev_info(ssm->dev, "TZAPP already loaded...\n");
		goto out;
	}

	/* APP not loaded get the firmware */
	/* Get .mdt first */
	rc =  request_firmware(&fw_mdt, FIRMWARE_NAME".mdt", ssm->dev);
	if (rc) {
		dev_err(ssm->dev, "Unable to get mdt file %s\n",
						FIRMWARE_NAME".mdt");
		rc = -EIO;
		goto out;
	}

	if (fw_mdt->size < sizeof(*ehdr)) {
		dev_err(ssm->dev, "Not big enough to be an elf header\n");
		rc = -EIO;
		goto release_mdt;
	}

	ehdr = (struct elf32_hdr *)fw_mdt->data;
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		dev_err(ssm->dev, "Not an elf header\n");
		rc = -EIO;
		goto release_mdt;
	}

	if (ehdr->e_phnum == 0) {
		dev_err(ssm->dev, "No loadable segments\n");
		rc = -EIO;
		goto release_mdt;
	}

	phdr = (const struct elf32_phdr *)(fw_mdt->data +
					sizeof(struct elf32_hdr));

	fw = kzalloc((sizeof(struct firmware *) * ehdr->e_phnum), GFP_KERNEL);
	if (!fw) {
		rc = -ENOMEM;
		goto release_mdt;
	}

	/* Valid .mdt now we need to load other parts .b0* */
	for (fw_count = 0; fw_count < ehdr->e_phnum ; fw_count++) {
		snprintf(name, MAX_APP_NAME_SIZE, FIRMWARE_NAME".b%02d",
								fw_count);
		rc = request_firmware(&fw[fw_count], name, ssm->dev);
		if (rc < 0) {
			rc = -EIO;
			dev_err(ssm->dev, "Unable to get blob file\n");
			goto release_blob;
		}

		if (fw[fw_count]->size != phdr->p_filesz) {
			dev_err(ssm->dev, "Blob size %u doesn't match %u\n",
					fw[fw_count]->size, phdr->p_filesz);
			rc = -EIO;
			goto release_blob;
		}

		phdr++;
		size += fw[fw_count]->size;
	}

	/* Ion allocation for loading tzapp */
	/* ION buffer size 4k aligned */
	ion_len = ALIGN_BUFFER(size);
	ion_handle = ion_alloc(ssm_drv->ssm_ion_client,
			ion_len, SZ_4K, ION_HEAP(ION_QSECOM_HEAP_ID), 0);
	if (IS_ERR_OR_NULL(ion_handle)) {
		rc = PTR_ERR(ion_handle);
		dev_err(ssm->dev, "Unable to get ion handle\n");
		goto release_blob;
	}

	rc = ion_phys(ssm_drv->ssm_ion_client, ion_handle,
			&buff_phys, &buff_len);
	if (rc < 0) {
		dev_err(ssm->dev, "Unable to get ion physical address\n");
		goto ion_free;
	}

	if (buff_len < size) {
		rc = -ENOMEM;
		goto ion_free;
	}

	buff_virt = ion_map_kernel(ssm_drv->ssm_ion_client,
				ion_handle);
	if (IS_ERR_OR_NULL((void *)buff_virt)) {
		rc = PTR_ERR((void *)buff_virt);
		dev_err(ssm->dev, "Unable to get ion virtual address\n");
		goto ion_free;
	}

	/* Copy firmware to ION memory */
	memcpy((unsigned char *)buff_virt, fw_mdt->data, fw_mdt->size);
	pos = (unsigned char *)buff_virt + fw_mdt->size;
	for (i = 0; i < ehdr->e_phnum; i++) {
		memcpy(pos, fw[i]->data, fw[i]->size);
		pos += fw[i]->size;
	}

	/* Loading app */
	app_img_info.cmd_id = APP_START_COMMAND;
	app_img_info.mdt_len = fw_mdt->size;
	app_img_info.img_len = size;
	app_img_info.phy_addr = buff_phys;

	/* SCM call to load the TZ APP */
	rc = scm_call(SCM_SVC_TZSCHEDULER, 1, &app_img_info,
		sizeof(struct load_app), &app_resp, sizeof(app_resp));
	if (rc) {
		rc = -EIO;
		dev_err(ssm->dev, "SCM call to load APP failed\n");
		goto ion_unmap;
	}

	if (app_resp.result == RESULT_FAILURE) {
		rc = -EIO;
		dev_err(ssm->dev, "SCM command to load TzAPP failed\n");
		goto ion_unmap;
	}

	ssm->app_id = app_resp.data;
	ssm->app_status = SUCCESS;

ion_unmap:
	ion_unmap_kernel(ssm_drv->ssm_ion_client, ion_handle);
ion_free:
	ion_free(ssm_drv->ssm_ion_client, ion_handle);
release_blob:
	while (--fw_count >= 0)
		release_firmware(fw[fw_count]);
	kfree(fw);
release_mdt:
	release_firmware(fw_mdt);
out:
	return rc;
}

/*
 * Allocate buffer for transactions.
 */
static int ssm_setup_ion(struct ssm_driver *ssm)
{
	int rc = 0;
	unsigned int size;

	size = ALIGN_BUFFER(ATOM_MSG_LEN);

	/* ION client for communicating with TZ */
	ssm->ssm_ion_client = msm_ion_client_create(UINT_MAX,
							"ssm-kernel");
	if (IS_ERR_OR_NULL(ssm->ssm_ion_client)) {
		rc = PTR_ERR(ssm->ssm_ion_client);
		dev_err(ssm->dev, "Ion client not created\n");
		return rc;
	}

	/* Setup a small ION buffer for tz communication */
	ssm->ssm_ion_handle = ion_alloc(ssm->ssm_ion_client,
				size, SZ_4K, ION_HEAP(ION_QSECOM_HEAP_ID), 0);
	if (IS_ERR_OR_NULL(ssm->ssm_ion_handle)) {
		rc = PTR_ERR(ssm->ssm_ion_handle);
		dev_err(ssm->dev, "Unable to get ion handle\n");
		goto out;
	}

	rc = ion_phys(ssm->ssm_ion_client, ssm->ssm_ion_handle,
			&ssm->buff_phys, &ssm->buff_len);
	if (rc < 0) {
		dev_err(ssm->dev,
			"Unable to get ion buffer physical address\n");
		goto ion_free;
	}

	if (ssm->buff_len < size) {
		rc = -ENOMEM;
		goto ion_free;
	}

	ssm->buff_virt = ion_map_kernel(ssm->ssm_ion_client,
				ssm->ssm_ion_handle);
	if (IS_ERR_OR_NULL((void *)ssm->buff_virt)) {
		rc = PTR_ERR((void *)ssm->buff_virt);
		dev_err(ssm->dev,
			"Unable to get ion buffer virtual address\n");
		goto ion_free;
	}

	return rc;

ion_free:
	ion_free(ssm->ssm_ion_client, ssm->ssm_ion_handle);
out:
	ion_client_destroy(ssm_drv->ssm_ion_client);
	return rc;
}

static struct ssm_platform_data *populate_ssm_pdata(struct device *dev)
{
	struct ssm_platform_data *pdata;
	int rc;

	pdata = devm_kzalloc(dev, sizeof(struct ssm_platform_data),
								GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->need_key_exchg =
		of_property_read_bool(dev->of_node, "qcom,need-keyexhg");

	rc = of_property_read_string(dev->of_node, "qcom,channel-name",
							&pdata->channel_name);
	if (rc && rc != -EINVAL) {
		dev_err(dev, "Error reading channel_name property %d\n", rc);
		return NULL;
	} else if (rc == -EINVAL)
		pdata->channel_name = CHANNEL_NAME;

	return pdata;
}

static int __devinit ssm_probe(struct platform_device *pdev)
{
	int rc;
	uint32_t system_call_id;
	char legacy = '\0';
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
	if (!drv) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	/* Initialize the driver structure */
	drv->atom_replay = -1;
	drv->mtoa_replay = -1;
	drv->app_id = -1;
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
		rc = -ENOMEM;
		goto exit;
	}

	/* Allocate response buffer */
	drv->resp = devm_kzalloc(&pdev->dev,
				sizeof(struct tzapp_get_mode_info_rsp),
				GFP_KERNEL);
	if (!drv->resp) {
		rc = -ENOMEM;
		goto exit;
	}


	/* Check for TZ version */
	system_call_id = QSEOS_CHECK_VERSION_CMD;
	rc = scm_call(SCM_SVC_INFO, SSM_INFO_CMD_ID, &system_call_id,
			sizeof(system_call_id), &legacy, sizeof(legacy));
	if (rc) {
		dev_err(&pdev->dev, "Get version failed %d\n", rc);
		rc = -EINVAL;
		goto exit;
	}

	/* This driver only support 1.4 TZ and QSEOS */
	if (!legacy) {
		dev_err(&pdev->dev,
				"Driver doesn't support legacy version\n");
		rc = -EINVAL;
		goto exit;

	}

	/* Setup the ion buffer for transaction */
	rc = ssm_setup_ion(drv);
	if (rc < 0)
		goto exit;

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

static int __devexit ssm_remove(struct platform_device *pdev)
{
	int rc;

	struct scm_shutdown_req req;
	struct scm_resp resp;

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
	flush_work_sync(&ssm_drv->ipc_work);

	/* ION clean up*/
	ion_unmap_kernel(ssm_drv->ssm_ion_client, ssm_drv->ssm_ion_handle);
	ion_free(ssm_drv->ssm_ion_client, ssm_drv->ssm_ion_handle);
	ion_client_destroy(ssm_drv->ssm_ion_client);

	/* Shutdown tzapp */
	req.app_id = ssm_drv->app_id;
	req.cmd_id = APP_SHUTDOWN_COMMAND;
	rc = scm_call(SCM_SVC_TZSCHEDULER, 1, &req, sizeof(req),
			&resp, sizeof(resp));
	if (rc)
		dev_err(&pdev->dev, "TZ_app Unload failed\n");

	return rc;
}

static struct of_device_id ssm_match_table[] = {
	{
		.compatible = "qcom,ssm",
	},
	{}
};

static struct platform_driver ssm_pdriver = {
	.probe          = ssm_probe,
	.remove         = __devexit_p(ssm_remove),
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
 * 1. Get TZAPP ID.
 * 2. Set default mode.
 * 3. Set mode (encrypted mode and it's length is passed as parameter).
 * 4. Set mode from TZ.
 * 5. Get status of mode update.
 *
 */
int ssm_oem_driver_intf(int cmd, char *mode, int len)
{
	int rc, req_len, resp_len;
	struct tzapp_get_mode_info_req get_mode_req;
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
			ssm_drv->app_status = FAILED;
			goto unlock;
		}
	} else if (ssm_drv->app_status == FAILED) {
		rc = -ENODEV;
		goto unlock;
	}

	/* Open modem SMD interface */
	if (!ssm_drv->ready) {
		rc = smd_open(ssm_drv->channel_name, &ssm_drv->ch, ssm_drv,
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

	case SSM_GET_APP_ID:
		rc = ssm_drv->app_id;
		break;

	case SSM_MODE_INFO_READY:
		ssm_drv->update_status = RETRY;
		/* Fill command structure */
		req_len = sizeof(struct tzapp_get_mode_info_req);
		resp_len = sizeof(struct tzapp_get_mode_info_rsp);
		get_mode_req.tzapp_ssm_cmd = GET_ENC_MODE;
		rc = tz_scm_call(ssm_drv, (void *)&get_mode_req,
				req_len, (void **)&get_mode_resp, resp_len);
		if (rc) {
			ssm_drv->update_status = FAILED;
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

	case SSM_SET_DEFAULT_MODE:
		/* Modem does not send response for this */
		ssm_drv->update_status = RETRY;
		rc = update_modem(SSM_ATOM_SET_DEFAULT_MODE, ssm_drv,
				1, "0");
		if (rc)
			ssm_drv->update_status = FAILED;
		else
			/* For default mode we don't get any resp
			 * from modem.
			 */
			ssm_drv->update_status = SUCCESS;
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
EXPORT_SYMBOL_GPL(ssm_oem_driver_intf);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Secure Service Module");

