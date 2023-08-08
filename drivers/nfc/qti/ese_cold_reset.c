// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include "nfc_common.h"

/**
 * send_ese_cmd() - Send eSE command to NFC controller.
 * @nfc_dev: NFC device handle.
 *
 * Return: 0 on pass and negative value on failure.
 */
static int send_ese_cmd(struct nfc_dev *nfc_dev)
{
	int ret;

	if (nfc_dev->nfc_state == NFC_STATE_FW_DWL) {
		dev_err(nfc_dev->nfc_device,
			"cannot send ese cmd as FW download is in-progress\n");
		return -EBUSY;
	}
	if (!gpio_get_value(nfc_dev->configs.gpio.ven)) {
		dev_err(nfc_dev->nfc_device,
				"cannot send ese cmd as NFCC powered off\n");
		return -ENODEV;
	}
	if (nfc_dev->cold_reset.cmd_buf == NULL)
		return -EFAULT;
	ret = nfc_dev->nfc_write(nfc_dev, nfc_dev->cold_reset.cmd_buf,
						nfc_dev->cold_reset.cmd_len,
						MAX_RETRY_COUNT);
	if (ret <= 0)
		dev_err(nfc_dev->nfc_device,
				"%s: write failed after max retry, ret %d\n",
							__func__, ret);

	return ret;
}

/**
 * read_cold_reset_rsp() - Read response of the cold reset command.
 * @nfc_dev: NFC device handle.
 * @header:  Pointer to NCI header if it is already read.
 *
 * Return: 0 on pass and negative value on failure.
 */
int read_cold_reset_rsp(struct nfc_dev *nfc_dev, char *header)
{
	int ret = -EPERM;
	struct cold_reset *cold_rst = &nfc_dev->cold_reset;
	char *rsp_buf = NULL;

	if (cold_rst->rsp_len < COLD_RESET_RSP_LEN) {
		dev_err(nfc_dev->nfc_device,
			"%s: received cold reset rsp buffer length is invalid\n",
			__func__);
		return -EINVAL;
	}

	rsp_buf = kzalloc(cold_rst->rsp_len, GFP_DMA | GFP_KERNEL);
	if (!rsp_buf)
		return -ENOMEM;

	/*
	 * read header if NFC is disabled
	 * for enable case, header is read by nfc read thread(for i2c)
	 */
	if ((!cold_rst->is_nfc_enabled) &&
			(nfc_dev->interface == PLATFORM_IF_I2C)) {
		ret = i2c_master_recv(nfc_dev->i2c_dev.client, rsp_buf, NCI_HDR_LEN);
		if (ret <= 0) {
			dev_err(nfc_dev->nfc_device,
				"%s: failure to read cold reset rsp header\n",
				 __func__);
			ret = -EIO;
			goto error;
		}
		/*
		 * return failure, if packet is not a response packet or
		 * if response's OID doesn't match with the CMD's OID
		 */
		if (!(rsp_buf[0] & NCI_RSP_PKT_TYPE) ||
			(!cold_rst->cmd_buf) ||
			(rsp_buf[1] != cold_rst->cmd_buf[1])) {

			dev_err(nfc_dev->nfc_device,
				"%s: - invalid cold reset response 0x%x 0x%x\n",
					__func__, rsp_buf[0], rsp_buf[1]);
			ret = -EINVAL;
			goto error;
		}
	} else if (header) {
		memcpy(rsp_buf, header, NCI_HDR_LEN);
	} else {
		dev_err(nfc_dev->nfc_device,
				"%s: - invalid or NULL header\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	if ((NCI_HDR_LEN + rsp_buf[NCI_PAYLOAD_LEN_IDX]) >
						cold_rst->rsp_len) {
		dev_err(nfc_dev->nfc_device,
			"%s: - no space for cold_reset resp\n", __func__);
		ret = -ENOMEM;
		goto error;
	}

	if (nfc_dev->interface == PLATFORM_IF_I2C) {
		ret = nfc_dev->nfc_read(nfc_dev,
			     &rsp_buf[NCI_PAYLOAD_IDX],
			     rsp_buf[NCI_PAYLOAD_LEN_IDX],
			     NCI_CMD_RSP_TIMEOUT);

		if (ret <= 0) {
			dev_err(nfc_dev->nfc_device,
				"%s: failure to read cold reset rsp payload\n",
				__func__);
			ret = -EIO;
			goto error;
		}
		ret = cold_rst->status = rsp_buf[NCI_PAYLOAD_IDX];

		pr_debug("nfc ese rsp hdr 0x%x 0x%x 0x%x, payload byte0 0x%x\n",
				rsp_buf[0], rsp_buf[1], rsp_buf[2], rsp_buf[3]);
	}

error:
	kfree(rsp_buf);

	return ret;
}


/**
 * ese_cold_reset_ioctl() - This function handles the eSE cold reset ioctls.
 * @nfc_dev: NFC device handle.
 * @arg: ioctl argument.
 *
 * Return: 0 on pass and negative value on failure.
 */

int ese_cold_reset_ioctl(struct nfc_dev *nfc_dev, unsigned long arg)
{
	int ret;
	struct ese_ioctl_arg ioctl_arg;
	struct ese_cold_reset_arg *cold_reset_arg = NULL;

	if (!arg) {
		dev_err(nfc_dev->nfc_device, "arg is invalid\n");
		return -EINVAL;
	}

	ret = copy_from_user((void *)&ioctl_arg, (const void *)arg,
						sizeof(ioctl_arg));
	if (ret) {
		dev_err(nfc_dev->nfc_device,
				"ese ioctl arg copy from user failed\n");
		return -EFAULT;
	}

	cold_reset_arg = kzalloc(sizeof(struct ese_cold_reset_arg), GFP_KERNEL);
	if (!cold_reset_arg)
		return -ENOMEM;

	mutex_lock(&nfc_dev->write_mutex);
	ret = copy_struct_from_user(cold_reset_arg,
				sizeof(struct ese_cold_reset_arg),
				u64_to_user_ptr(ioctl_arg.buf),
				sizeof(struct ese_cold_reset_arg));
	if (ret) {
		dev_err(nfc_dev->nfc_device,
			"ese ioctl arg buffer copy from user failed\n");

		ret = -EFAULT;
		goto err;
	}

	switch (cold_reset_arg->sub_cmd) {

	case ESE_COLD_RESET_DO:

		/*
		 * cold reset allowed during protection enable, only if the
		 * source is same as the one which enabled protection.
		 */
		if (nfc_dev->cold_reset.is_crp_en &&
			(cold_reset_arg->src !=
				nfc_dev->cold_reset.last_src_ese_prot)) {
			dev_err(nfc_dev->nfc_device,
				"cold reset from %d denied, protection is on\n",
				cold_reset_arg->src);
			ret = -EACCES;
			goto err;
		}

		nfc_dev->cold_reset.cmd_buf = kzalloc(COLD_RESET_CMD_LEN,
							GFP_DMA | GFP_KERNEL);
		if (!nfc_dev->cold_reset.cmd_buf) {
			ret = -ENOMEM;
			goto err;
		}

		nfc_dev->cold_reset.cmd_buf[0] = PROP_NCI_CMD_GID;
		nfc_dev->cold_reset.cmd_buf[1] = COLD_RESET_OID;
		nfc_dev->cold_reset.cmd_buf[2] = COLD_RESET_CMD_PL_LEN;
		nfc_dev->cold_reset.cmd_len = NCI_HDR_LEN +
						COLD_RESET_CMD_PL_LEN;
		nfc_dev->cold_reset.rsp_len = COLD_RESET_RSP_LEN;
		break;

	case ESE_COLD_RESET_PROTECT_EN:

		if (nfc_dev->cold_reset.is_crp_en) {
			if (cold_reset_arg->src !=
				nfc_dev->cold_reset.last_src_ese_prot) {
				dev_err(nfc_dev->nfc_device,
					"ese protection enable denied\n");
				ret = -EACCES;
				goto err;
			}
			pr_warn("ese protection already enabled\n");

			ret = 0;
			/* free buffers and exit with pass */
			goto err;
		}

	case ESE_COLD_RESET_PROTECT_DIS:

		if (nfc_dev->cold_reset.is_crp_en &&
			cold_reset_arg->src !=
				nfc_dev->cold_reset.last_src_ese_prot) {
			pr_err("ese cold reset protection disable denied\n");
			ret = -EACCES;
			goto err;
		}
		nfc_dev->cold_reset.cmd_buf = kzalloc(COLD_RESET_PROT_CMD_LEN,
							GFP_DMA | GFP_KERNEL);
		if (!nfc_dev->cold_reset.cmd_buf) {
			ret = -ENOMEM;
			goto err;
		}

		nfc_dev->cold_reset.cmd_buf[0] = PROP_NCI_CMD_GID;
		nfc_dev->cold_reset.cmd_buf[1] = COLD_RESET_PROT_OID;
		nfc_dev->cold_reset.cmd_buf[2] = COLD_RESET_PROT_CMD_PL_LEN;
		nfc_dev->cold_reset.cmd_len = NCI_HDR_LEN +
						COLD_RESET_PROT_CMD_PL_LEN;
		nfc_dev->cold_reset.rsp_len = COLD_RESET_PROT_RSP_LEN;
		if (cold_reset_arg->sub_cmd == ESE_COLD_RESET_PROTECT_EN)
			nfc_dev->cold_reset.cmd_buf[3] = 0x1;
		else
			nfc_dev->cold_reset.cmd_buf[3] = 0x0;

		break;

	default:
		pr_err("%s invalid ese ioctl sub cmd %d\n", __func__,
					cold_reset_arg->sub_cmd);
		ret = -ENOIOCTLCMD;
		goto err;
	}

	pr_debug("nfc ese cmd hdr 0x%x 0x%x 0x%x\n",
				nfc_dev->cold_reset.cmd_buf[0],
				nfc_dev->cold_reset.cmd_buf[1],
				nfc_dev->cold_reset.cmd_buf[2]);

	ret = send_ese_cmd(nfc_dev);
	if (ret <= 0) {
		pr_err("failed to send ese command\n");
		goto err;
	}

	nfc_dev->cold_reset.rsp_pending = true;

	/* check if NFC is enabled */
	if (nfc_dev->cold_reset.is_nfc_enabled) {
		/*
		 * nfc_read thread will initiate cold reset response
		 * and it will signal for data available
		 */
		wait_event_interruptible(nfc_dev->cold_reset.read_wq,
			!nfc_dev->cold_reset.rsp_pending);
	} else {

		/*
		 * Read data as NFC read thread is not active
		 */

		if (nfc_dev->interface == PLATFORM_IF_I2C) {
			ret =  is_nfc_data_available_for_read(nfc_dev);
			if (ret <= 0) {
				nfc_dev->nfc_disable_intr(nfc_dev);
				nfc_dev->cold_reset.rsp_pending = false;
				goto err;
			}

			ret = read_cold_reset_rsp(nfc_dev, NULL);
			nfc_dev->cold_reset.rsp_pending = false;
			if (ret < 0) {
				pr_err("%s rsp read err\n", __func__);
				goto err;
			}
		} else {
			/*
			 * Enable intr as it is disabled when NFC is in disable
			 * state
			 */
			nfc_dev->nfc_enable_intr(nfc_dev);

			wait_event_interruptible(
				nfc_dev->cold_reset.read_wq,
				!nfc_dev->cold_reset.rsp_pending);
		}

		nfc_dev->nfc_disable_intr(nfc_dev);
	}

	if (cold_reset_arg->sub_cmd == ESE_COLD_RESET_PROTECT_EN) {
		nfc_dev->cold_reset.is_crp_en = true;
		nfc_dev->cold_reset.last_src_ese_prot = cold_reset_arg->src;
	} else if (cold_reset_arg->sub_cmd == ESE_COLD_RESET_PROTECT_DIS) {
		nfc_dev->cold_reset.is_crp_en = false;
		nfc_dev->cold_reset.last_src_ese_prot =
						ESE_COLD_RESET_ORIGIN_NONE;
	} else
		pr_debug("ese cmd is %d\n", cold_reset_arg->sub_cmd);

	ret = nfc_dev->cold_reset.status;

err:
	if (nfc_dev->cold_reset.cmd_buf != NULL) {
		kfree(nfc_dev->cold_reset.cmd_buf);
		nfc_dev->cold_reset.cmd_buf = NULL;
	}
	if (cold_reset_arg != NULL) {
		kfree(cold_reset_arg);
		cold_reset_arg = NULL;
	}

	mutex_unlock(&nfc_dev->write_mutex);

	return ret;
}
