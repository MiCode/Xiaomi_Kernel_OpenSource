// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#include "ufshcd.h"
#include "ufs-sprd.h"
#include "ufs-sprd-ioctl.h"

#define uptr64(val) ((void __user *)(uintptr_t)(val))

/**
 * ufs_sprd_ffu_send_cmd - sends WRITE BUFFER command to do FFU
 * @hba: per adapter instance
 * @idata: ioctl data for ffu
 *
 * Returns 0 if ffu operation is sccessfull
 * Returns non-zero if failed to do ffu
 */
static int sprd_ufs_ffu_send_cmd(struct scsi_device *dev,
	struct ufs_ioctl_ffu_data *idata, void *buf_ptr)
{
	struct ufs_hba *hba;
	unsigned char cmd[10] = {0};
	struct scsi_sense_hdr sshdr;
	unsigned long flags;
	unsigned int fw_size = idata->buf_byte;
	unsigned int chunk_size = idata->chunk_byte;
	unsigned int write_buf_count = 0;
	unsigned int buf_offset = 0;
	int ret = 0;

	if (dev)
		hba = shost_priv(dev->host);
	else
		return -ENODEV;

	spin_lock_irqsave(hba->host->host_lock, flags);

	ret = scsi_device_get(dev);
	if (!ret && !scsi_device_online(dev)) {
		ret = -ENODEV;
		scsi_device_put(dev);
	}

	spin_unlock_irqrestore(hba->host->host_lock, flags);

	if (ret)
		return ret;

	/*
	 * If scsi commands fail, the scsi mid-layer schedules scsi error-
	 * handling, which would wait for host to be resumed. Since we know
	 * we are functional while we are here, skip host resume in error
	 * handling context.
	 */
	hba->host->eh_noresume = 1;

	while (fw_size > 0) {
		if (fw_size > chunk_size)
			write_buf_count = chunk_size;
		else
			write_buf_count = fw_size;

		cmd[0] = WRITE_BUFFER;                   /* Opcode */
		cmd[1] = 0xE;                            /* 0xE: Download firmware */
		cmd[2] = 0;                              /* Buffer ID = 0 */
		cmd[3] = (buf_offset >> 16) & 0xff;      /* Buffer Offset[23:16]*/
		cmd[4] = (buf_offset >> 8) & 0xff;       /* Buffer Offset[15:08]*/
		cmd[5] = (buf_offset) & 0xff;            /* Buffer Offset[07:00]*/
		cmd[6] = (write_buf_count >> 16) & 0xff; /* Length[23:16] */
		cmd[7] = (write_buf_count >> 8) & 0xff;  /* Length[15:08] */
		cmd[8] = (write_buf_count) & 0xff;       /* Length[07:00] */
		cmd[9] = 0x0;                            /* Control = 0 */

		/*
		 * Current function would be generally called from the power management
		 * callbacks hence set the RQF_PM flag so that it doesn't resume the
		 * already suspended children.
		 */

		ret = scsi_execute(dev, cmd, DMA_TO_DEVICE,
				   buf_ptr + buf_offset, write_buf_count, NULL, &sshdr,
				   msecs_to_jiffies(1000), 0, 0, RQF_PM, NULL);
		if (ret) {
			sdev_printk(KERN_ERR, dev,
				"WRITE BUFFER failed for firmware upgrade\n");

			goto out;
		}
		buf_offset = buf_offset + write_buf_count;
		fw_size = fw_size - write_buf_count;
	}

out:
	scsi_device_put(dev);
	hba->host->eh_noresume = 0;
	return ret;
}

/**
 * sprd_ufs_ioctl_ffu - update device firmware through ioctl
 * @hba: per-adapter instance
 * @buffer: user space buffer for ffu ioctl data
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_ffu_data.
 * It will read the buffer information of new firmware.
 */
int sprd_ufs_ioctl_ffu(struct scsi_device *dev, void __user *buf_user)
{
	struct ufs_hba *hba = shost_priv(dev->host);
	struct ufs_sprd_host *sprd_ufs = ufshcd_get_variant(hba);
	struct ufs_ioctl_ffu_data *idata = NULL;
	void *buf_ptr = NULL;
	int rst_retries = 5;
	int err = 0;
	u32 attr = 0;

	idata = kzalloc(sizeof(struct ufs_ioctl_ffu_data), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	err = copy_from_user(idata, buf_user, sizeof(struct ufs_ioctl_ffu_data));
	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out;
	}

	if (idata->buf_byte > UFS_IOCTL_FFU_MAX_FW_SIZE) {
		dev_err(hba->dev,
			"%s: fw size(%d) exceeds max limit 2MB, stop FFU!\n",
			__func__, idata->buf_byte);
		err = -EFBIG;
		goto out;
	}

	buf_ptr = kzalloc(idata->buf_byte, GFP_KERNEL);
	if (!buf_ptr) {
		err = -ENOMEM;
		goto out;
	}

	err = copy_from_user(buf_ptr, uptr64(idata->buf_ptr), idata->buf_byte);
	if (err) {
		dev_err(hba->dev,
			"%s: failed copying FW from user, err %d\n",
			__func__, err);
		goto out;
	}

	sprd_ufs->ffu_is_process = TRUE;

	err = sprd_ufs_ffu_send_cmd(dev, idata, buf_ptr);
	if (err)
		dev_err(hba->dev, "%s: ffu failed, err 0x%x\n", __func__, err);
	else
		dev_info(hba->dev, "%s: ffu send success\n", __func__);

	sprd_ufs->ffu_is_process = FALSE;

	/*
	 * To achieve fw update during boot step, new FW is activated
	 * directly using reset UFS.
	 */
	do {
		err = ufshcd_link_recovery(hba);
		if (err)
			dev_err(hba->dev, "%s: ufs reset failed, err %d\n",
				__func__, err);
	} while (err && --rst_retries);

	if (err)
		goto out;

	/*
	 * Check bDeviceFFUStatus attribute
	 *
	 * For reference only since UFS spec. said the status is valid after
	 * device power cycle.
	 */
	err = ufshcd_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
		QUERY_ATTR_IDN_FFU_STATUS, 0, 0, &attr);
	if (err) {
		dev_err(hba->dev, "%s: query bDeviceFFUStatus failed, err %d\n",
			__func__, err);
		goto out;
	}

	if (attr != UFS_FFU_STATUS_SUCCESSFUL_UPDATE) {
		dev_err(hba->dev, "%s: bDeviceFFUStatus shows fail %d (ref only)\n",
			__func__, attr);
		err = -EFAULT;
	} else {
		dev_notice(hba->dev, "%s: UFS FFU UPDATE SUCCESS!!!\n", __func__);
	}

out:
	kfree(buf_ptr);
	kfree(idata);

	/*
	 * UFS might not be used normally after FFU.
	 * Just reboot system (including device) to avoid following
	 * false alarm. For example, I/O errors.
	 */

	return err;
}

/**
 * sprd_ufs_ioctl_get_dev_info - perform user request: query fw ver
 * @hba: per-adapter instance
 * @buffer: user space buffer for ffu ioctl data
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct ufs_ioctl_ffu_data.
 * It will read the buffer information of new firmware.
 */
int sprd_ufs_ioctl_get_dev_info(struct scsi_device *dev, void __user *buf_user)
{
	struct ufs_hba *hba;
	struct ufs_ioctl_query_device_info *idata = NULL;
	int err = 0;

	if (dev)
		hba = shost_priv(dev->host);
	else
		return -ENODEV;

	/* check scsi device instance */
	if (!dev->rev) {
		dev_err(hba->dev, "%s: scsi_device or rev is NULL\n", __func__);
		err = -ENOENT;
		goto out;
	}

	idata = kzalloc(sizeof(struct ufs_ioctl_query_device_info), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	/* extract params from user buffer */
	err = copy_from_user(idata, buf_user, sizeof(struct ufs_ioctl_query_device_info));
	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

	memcpy(idata->vendor, dev->vendor, UFS_IOCTL_FFU_MAX_VENDOR_BYTES);
	memcpy(idata->model, dev->model, UFS_IOCTL_FFU_MAX_MODEL_BYTES);
	memcpy(idata->fw_rev, dev->rev, UFS_IOCTL_FFU_MAX_FW_VER_BYTES);
	idata->manid = hba->dev_info.wmanufacturerid;
	idata->max_hw_sectors_size = (dev->request_queue->limits.max_hw_sectors << 9);

	err = copy_to_user(buf_user, idata, sizeof(struct ufs_ioctl_query_device_info));
	if (err) {
		dev_err(hba->dev, "%s: err %d copying to user.\n", __func__, err);
		goto out_release_mem;
	}

out_release_mem:
	kfree(idata);
out:

	return err;
}

int sprd_ufs_ioctl_get_pwr_info(struct scsi_device *dev, void __user *buf_user, unsigned int cmd)
{
	struct ufs_hba *hba;
	unsigned int *idata = NULL;
	struct ufs_sprd_host *host = NULL;
	int err;

	if (dev)
		hba = shost_priv(dev->host);
	else
		return -ENODEV;

	/* check scsi device instance */
	if (!dev->rev) {
		dev_err(hba->dev, "%s: scsi_device or rev is NULL\n", __func__);
		err = -ENOENT;
		goto out;
	}
	host = ufshcd_get_variant(hba);
	idata = kzalloc(sizeof(unsigned int), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	/* extract params from user buffer */
	err = copy_from_user(idata, buf_user, sizeof(unsigned int));
	if (err) {
		dev_err(hba->dev,
			"%s: failed copying buffer from user, err %d\n",
			__func__, err);
		goto out_release_mem;
	}

	if (cmd == UFS_IOCTL_ENTER_MODE) {
		if ((((hba->pwr_info.pwr_tx) << 4)|
		      (hba->pwr_info.pwr_rx)) == PWM_MODE_VAL)
			*idata = 1;
		else
			*idata = 0;
	}

	if (cmd == UFS_IOCTL_AFC_EXIT) {
		if ((((hba->pwr_info.pwr_tx) << 4)|
		      (hba->pwr_info.pwr_rx)) == HS_MODE_VAL)
			*idata = 1;
		else
			*idata = 0;
	}

	dev_err(hba->dev,
		"%s:gear[0x%x:0x%x],lane[0x%x:0x%x],pwr[0x%x:0x%x],hs_rate=0x%x,idata=0x%x!\n",
		__func__, hba->pwr_info.gear_rx, hba->pwr_info.gear_tx,
		hba->pwr_info.lane_rx, hba->pwr_info.lane_tx,
		hba->pwr_info.pwr_rx, hba->pwr_info.pwr_tx,
		hba->pwr_info.hs_rate, *idata);

	err = copy_to_user(buf_user, idata, sizeof(unsigned int));
	if (err) {
		dev_err(hba->dev, "%s: err %d copying to user.\n",
				__func__, err);
		goto out_release_mem;
	}

out_release_mem:
	kfree(idata);
out:
	return err;
}

static int ffu_wait_for_all_cmd_complete(struct ufs_hba *hba, int timeout_ms)
{
	if (hba == NULL)
		return -EINVAL;
	while (timeout_ms-- > 0) {
		if (!hba->outstanding_reqs)
			return 0;
		/* wait 1 ms */
		udelay(1000);
	}
	dev_err(hba->dev, "%s: outstanding req(0x%lx) during UFS FFU process\n", __func__,
		hba->outstanding_reqs);
	return -ETIMEDOUT;
}

void prepare_command_send_in_ffu_state(struct ufs_hba *hba,
				       struct ufshcd_lrb *lrbp, int *err)
{
	if (lrbp->cmd->cmnd[0] == WRITE_BUFFER &&
	    (lrbp->cmd->cmnd[1] & WB_MODE_MASK) == DOWNLOAD_MODE) {
		/* wait for ufs all complete timeout time 1s */
		ffu_wait_for_all_cmd_complete(hba, 1000);
	} else {
		dev_dbg(hba->dev, "%s: ffu in progress\n", __func__);
		*err = SCSI_MLQUEUE_HOST_BUSY;
	}
}

int ufshcd_sprd_ioctl(struct scsi_device *dev, unsigned int cmd, void __user *buffer)
{
	struct ufs_hba *hba = shost_priv(dev->host);
	struct ufs_sprd_host *host = NULL;
	int err = 0;

	if (!buffer) {
		dev_err(hba->dev, "%s: user buffer is NULL\n", __func__);
		return -EINVAL;
	}

	host = ufshcd_get_variant(hba);

	switch (cmd) {
	case UFS_IOCTL_FFU:
		ufshcd_rpm_get_sync(hba);
		err = sprd_ufs_ioctl_ffu(dev, buffer);
		ufshcd_rpm_put_sync(hba);
		break;
	case UFS_IOCTL_GET_DEVICE_INFO:
		err = sprd_ufs_ioctl_get_dev_info(dev, buffer);
		break;
	case UFS_IOCTL_ENTER_MODE:
		host->ioctl_status = UFS_IOCTL_ENTER_MODE;
		init_completion(&host->pwm_async_done);
		if (!wait_for_completion_timeout(&host->pwm_async_done, 50000)) {
			pr_err("pwm mode time out!\n");
			host->ioctl_status = 0;
			return -ETIMEDOUT;
		}
		err = sprd_ufs_ioctl_get_pwr_info(dev, buffer, cmd);
		break;
	case UFS_IOCTL_AFC_EXIT:
		host->ioctl_status = UFS_IOCTL_AFC_EXIT;
		init_completion(&host->hs_async_done);
		if (!wait_for_completion_timeout(&host->hs_async_done, 50000)) {
			pr_err("hs mode time out!\n");
			host->ioctl_status = 0;
			return -ETIMEDOUT;
		}
		err = sprd_ufs_ioctl_get_pwr_info(dev, buffer, cmd);
		host->ioctl_status = 0;
		break;
	default:
		err = -ENOIOCTLCMD;
		dev_dbg(hba->dev, "%s: Unsupported ioctl cmd %d\n", __func__, cmd);
		break;
	}

	return err;
}
