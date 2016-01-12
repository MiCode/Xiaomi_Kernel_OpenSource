/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#include <linux/io.h>
#include <linux/of.h>
#include <linux/blkdev.h>
#include <crypto/ice.h>

#include "ufs-qcom-ice.h"
#include "ufs-qcom-debugfs.h"
#include "ufshcd.h"

#define UFS_QCOM_CRYPTO_LABEL "ufs-qcom-crypto"
/* Timeout waiting for ICE initialization, that requires TZ access */
#define UFS_QCOM_ICE_COMPLETION_TIMEOUT_MS 500

#define UFS_QCOM_ICE_DEFAULT_DBG_PRINT_EN	0

static void ufs_qcom_ice_dump_regs(struct ufs_qcom_host *qcom_host, int offset,
					int len, char *prefix)
{
	print_hex_dump(KERN_ERR, prefix,
			len > 4 ? DUMP_PREFIX_OFFSET : DUMP_PREFIX_NONE,
			16, 4, qcom_host->hba->mmio_base + offset, len * 4,
			false);
}

void ufs_qcom_ice_print_regs(struct ufs_qcom_host *qcom_host)
{
	int i;

	if (!(qcom_host->dbg_print_en & UFS_QCOM_DBG_PRINT_ICE_REGS_EN))
		return;

	ufs_qcom_ice_dump_regs(qcom_host, REG_UFS_QCOM_ICE_CFG, 1,
			"REG_UFS_QCOM_ICE_CFG ");
	for (i = 0; i < NUM_QCOM_ICE_CTRL_INFO_n_REGS; i++) {
		pr_err("REG_UFS_QCOM_ICE_CTRL_INFO_1_%d = 0x%08X\n", i,
			ufshcd_readl(qcom_host->hba,
				(REG_UFS_QCOM_ICE_CTRL_INFO_1_n + 8 * i)));

		pr_err("REG_UFS_QCOM_ICE_CTRL_INFO_2_%d = 0x%08X\n", i,
			ufshcd_readl(qcom_host->hba,
				(REG_UFS_QCOM_ICE_CTRL_INFO_2_n + 8 * i)));
	}

}

static void ufs_qcom_ice_error_cb(void *host_ctrl, u32 error)
{
	struct ufs_qcom_host *qcom_host = (struct ufs_qcom_host *)host_ctrl;

	dev_err(qcom_host->hba->dev, "%s: Error in ice operation 0x%x",
		__func__, error);

	if (qcom_host->ice.state == UFS_QCOM_ICE_STATE_ACTIVE)
		qcom_host->ice.state = UFS_QCOM_ICE_STATE_DISABLED;
}

static struct platform_device *ufs_qcom_ice_get_pdevice(struct device *ufs_dev)
{
	struct device_node *node;
	struct platform_device *ice_pdev = NULL;

	node = of_parse_phandle(ufs_dev->of_node, UFS_QCOM_CRYPTO_LABEL, 0);

	if (!node) {
		dev_err(ufs_dev, "%s: ufs-qcom-crypto property not specified\n",
			__func__);
		goto out;
	}

	ice_pdev = qcom_ice_get_pdevice(node);
out:
	return ice_pdev;
}

static
struct qcom_ice_variant_ops *ufs_qcom_ice_get_vops(struct device *ufs_dev)
{
	struct qcom_ice_variant_ops *ice_vops = NULL;
	struct device_node *node;

	node = of_parse_phandle(ufs_dev->of_node, UFS_QCOM_CRYPTO_LABEL, 0);

	if (!node) {
		dev_err(ufs_dev, "%s: ufs-qcom-crypto property not specified\n",
			__func__);
		goto out;
	}

	ice_vops = qcom_ice_get_variant_ops(node);

	if (!ice_vops)
		dev_err(ufs_dev, "%s: invalid ice_vops\n", __func__);

	of_node_put(node);
out:
	return ice_vops;
}

/**
 * ufs_qcom_ice_get_dev() - sets pointers to ICE data structs in UFS QCom host
 * @qcom_host:	Pointer to a UFS QCom internal host structure.
 *
 * Sets ICE platform device pointer and ICE vops structure
 * corresponding to the current UFS device.
 *
 * Return: -EINVAL in-case of invalid input parameters:
 *  qcom_host, qcom_host->hba or qcom_host->hba->dev
 *         -ENODEV in-case ICE device is not required
 *         -EPROBE_DEFER in-case ICE is required and hasn't been probed yet
 *         0 otherwise
 */
int ufs_qcom_ice_get_dev(struct ufs_qcom_host *qcom_host)
{
	struct device *ufs_dev;
	int err = 0;

	if (!qcom_host || !qcom_host->hba || !qcom_host->hba->dev) {
		pr_err("%s: invalid qcom_host %p or qcom_host->hba or qcom_host->hba->dev\n",
			__func__, qcom_host);
		err = -EINVAL;
		goto out;
	}

	ufs_dev = qcom_host->hba->dev;

	qcom_host->ice.vops  = ufs_qcom_ice_get_vops(ufs_dev);
	qcom_host->ice.pdev = ufs_qcom_ice_get_pdevice(ufs_dev);

	if (qcom_host->ice.pdev == ERR_PTR(-EPROBE_DEFER)) {
		dev_err(ufs_dev, "%s: ICE device not probed yet\n",
			__func__);
		qcom_host->ice.pdev = NULL;
		qcom_host->ice.vops = NULL;
		err = -EPROBE_DEFER;
		goto out;
	}

	if (!qcom_host->ice.pdev || !qcom_host->ice.vops) {
		dev_err(ufs_dev, "%s: invalid platform device %p or vops %p\n",
			__func__, qcom_host->ice.pdev, qcom_host->ice.vops);
		qcom_host->ice.pdev = NULL;
		qcom_host->ice.vops = NULL;
		err = -ENODEV;
		goto out;
	}

	qcom_host->ice.state = UFS_QCOM_ICE_STATE_DISABLED;

out:
	return err;
}

static void ufs_qcom_ice_cfg_work(struct work_struct *work)
{
	struct ice_data_setting ice_set;
	struct ufs_qcom_host *qcom_host =
		container_of(work, struct ufs_qcom_host, ice_cfg_work);

	if (!qcom_host->ice.vops->config_start || !qcom_host->req_pending)
		return;

	memset(&ice_set, 0, sizeof(ice_set));

	/*
	 * config_start is called again as previous attempt returned -EAGAIN,
	 * this call shall now take care of the necessary key setup.
	 * 'ice_set' will not actually be used, instead the next call to
	 * config_start() for this request, in the normal call flow, will
	 * succeed as the key has now been setup.
	 */
	qcom_host->ice.vops->config_start(qcom_host->ice.pdev,
		qcom_host->req_pending, &ice_set, false);

	/*
	 * Resume with requests processing. We assume config_start has been
	 * successful, but even if it wasn't we still must resume in order to
	 * allow for the request to be retried.
	 */
	ufshcd_scsi_unblock_requests(qcom_host->hba);
}

/**
 * ufs_qcom_ice_init() - initializes the ICE-UFS interface and ICE device
 * @qcom_host:	Pointer to a UFS QCom internal host structure.
 *		qcom_host, qcom_host->hba and qcom_host->hba->dev should all
 *		be valid pointers.
 *
 * Return: -EINVAL in-case of an error
 *         0 otherwise
 */
int ufs_qcom_ice_init(struct ufs_qcom_host *qcom_host)
{
	struct device *ufs_dev = qcom_host->hba->dev;
	int err;

	err = qcom_host->ice.vops->init(qcom_host->ice.pdev,
				qcom_host,
				ufs_qcom_ice_error_cb);
	if (err) {
		dev_err(ufs_dev, "%s: ice init failed. err = %d\n",
			__func__, err);
		goto out;
	} else {
		qcom_host->ice.state = UFS_QCOM_ICE_STATE_ACTIVE;
	}

	qcom_host->dbg_print_en |= UFS_QCOM_ICE_DEFAULT_DBG_PRINT_EN;
	INIT_WORK(&qcom_host->ice_cfg_work, ufs_qcom_ice_cfg_work);

out:
	return err;
}

static inline bool ufs_qcom_is_data_cmd(char cmd_op, bool is_write)
{
	if (is_write) {
		if (cmd_op == WRITE_6 || cmd_op == WRITE_10 ||
		    cmd_op == WRITE_16)
			return true;
	} else {
		if (cmd_op == READ_6 || cmd_op == READ_10 ||
		    cmd_op == READ_16)
			return true;
	}

	return false;
}

int ufs_qcom_ice_req_setup(struct ufs_qcom_host *qcom_host,
		struct scsi_cmnd *cmd, u8 *cc_index, bool *enable)
{
	struct ice_data_setting ice_set;
	char cmd_op = cmd->cmnd[0];
	int err;

	if (!qcom_host->ice.pdev || !qcom_host->ice.vops) {
		dev_dbg(qcom_host->hba->dev, "%s: ice device is not enabled\n",
			__func__);
		return 0;
	}

	if (qcom_host->ice.vops->config_start) {
		memset(&ice_set, 0, sizeof(ice_set));
		err = qcom_host->ice.vops->config_start(qcom_host->ice.pdev,
			cmd->request, &ice_set, true);
		if (err) {
			dev_err(qcom_host->hba->dev,
				"%s: error in ice_vops->config %d\n",
				__func__, err);
			return err;
		}

		if (ufs_qcom_is_data_cmd(cmd_op, true))
			*enable = !ice_set.encr_bypass;
		else if (ufs_qcom_is_data_cmd(cmd_op, false))
			*enable = !ice_set.decr_bypass;

		if (ice_set.crypto_data.key_index >= 0)
			*cc_index = (u8)ice_set.crypto_data.key_index;
	}
	return 0;
}

/**
 * ufs_qcom_ice_cfg_start() - starts configuring UFS's ICE registers
 *							  for an ICE transaction
 * @qcom_host:	Pointer to a UFS QCom internal host structure.
 *		qcom_host, qcom_host->hba and qcom_host->hba->dev should all
 *		be valid pointers.
 * @cmd:	Pointer to a valid scsi command. cmd->request should also be
 *              a valid pointer.
 *
 * Return: -EINVAL in-case of an error
 *         0 otherwise
 */
int ufs_qcom_ice_cfg_start(struct ufs_qcom_host *qcom_host,
		struct scsi_cmnd *cmd)
{
	struct device *dev = qcom_host->hba->dev;
	int err = 0;
	struct ice_data_setting ice_set;
	unsigned int slot = 0;
	sector_t lba = 0;
	unsigned int ctrl_info_val = 0;
	unsigned int bypass = 0;
	struct request *req;
	char cmd_op;

	if (!qcom_host->ice.pdev || !qcom_host->ice.vops) {
		dev_dbg(dev, "%s: ice device is not enabled\n", __func__);
		goto out;
	}

	if (qcom_host->ice.state != UFS_QCOM_ICE_STATE_ACTIVE) {
		dev_err(dev, "%s: ice state (%d) is not active\n",
			__func__, qcom_host->ice.state);
		return -EINVAL;
	}

	req = cmd->request;
	if (req->bio)
		lba = req->bio->bi_iter.bi_sector;

	slot = req->tag;
	if (slot < 0 || slot > qcom_host->hba->nutrs) {
		dev_err(dev, "%s: slot (%d) is out of boundaries (0...%d)\n",
			__func__, slot, qcom_host->hba->nutrs);
		return -EINVAL;
	}

	memset(&ice_set, 0, sizeof(ice_set));
	if (qcom_host->ice.vops->config_start) {
		err = qcom_host->ice.vops->config_start(qcom_host->ice.pdev,
							req, &ice_set, true);
		if (err) {
			/*
			 * config_start() returns -EAGAIN when a key slot is
			 * available but still not configured. As configuration
			 * requires a non-atomic context, this means we should
			 * call the function again from the worker thread to do
			 * the configuration. For this request the error will
			 * propagate so it will be re-queued and until the
			 * configuration is is completed we block further
			 * request processing.
			 */
			if (err == -EAGAIN) {
				qcom_host->req_pending = req;
				if (schedule_work(&qcom_host->ice_cfg_work))
					ufshcd_scsi_block_requests(
							qcom_host->hba);
			}
			goto out;
		}
	}

	cmd_op = cmd->cmnd[0];

#define UFS_QCOM_DIR_WRITE	true
#define UFS_QCOM_DIR_READ	false
	/* if non data command, bypass shall be enabled */
	if (!ufs_qcom_is_data_cmd(cmd_op, UFS_QCOM_DIR_WRITE) &&
	    !ufs_qcom_is_data_cmd(cmd_op, UFS_QCOM_DIR_READ))
		bypass = UFS_QCOM_ICE_ENABLE_BYPASS;
	/* if writing data command */
	else if (ufs_qcom_is_data_cmd(cmd_op, UFS_QCOM_DIR_WRITE))
		bypass = ice_set.encr_bypass ? UFS_QCOM_ICE_ENABLE_BYPASS :
						UFS_QCOM_ICE_DISABLE_BYPASS;
	/* if reading data command */
	else if (ufs_qcom_is_data_cmd(cmd_op, UFS_QCOM_DIR_READ))
		bypass = ice_set.decr_bypass ? UFS_QCOM_ICE_ENABLE_BYPASS :
						UFS_QCOM_ICE_DISABLE_BYPASS;

	/* Configure ICE index */
	ctrl_info_val =
		(ice_set.crypto_data.key_index &
		 MASK_UFS_QCOM_ICE_CTRL_INFO_KEY_INDEX)
		 << OFFSET_UFS_QCOM_ICE_CTRL_INFO_KEY_INDEX;

	/* Configure data unit size of transfer request */
	ctrl_info_val |=
		(UFS_QCOM_ICE_TR_DATA_UNIT_4_KB &
		 MASK_UFS_QCOM_ICE_CTRL_INFO_CDU)
		 << OFFSET_UFS_QCOM_ICE_CTRL_INFO_CDU;

	/* Configure ICE bypass mode */
	ctrl_info_val |=
		(bypass & MASK_UFS_QCOM_ICE_CTRL_INFO_BYPASS)
		 << OFFSET_UFS_QCOM_ICE_CTRL_INFO_BYPASS;

	if (qcom_host->hw_ver.major < 0x2) {
		ufshcd_writel(qcom_host->hba, lba,
			     (REG_UFS_QCOM_ICE_CTRL_INFO_1_n + 8 * slot));

		ufshcd_writel(qcom_host->hba, ctrl_info_val,
			     (REG_UFS_QCOM_ICE_CTRL_INFO_2_n + 8 * slot));
	} else {
		ufshcd_writel(qcom_host->hba, (lba & 0xFFFFFFFF),
			     (REG_UFS_QCOM_ICE_CTRL_INFO_1_n + 16 * slot));

		ufshcd_writel(qcom_host->hba, ((lba >> 32) & 0xFFFFFFFF),
			     (REG_UFS_QCOM_ICE_CTRL_INFO_2_n + 16 * slot));

		ufshcd_writel(qcom_host->hba, ctrl_info_val,
			     (REG_UFS_QCOM_ICE_CTRL_INFO_3_n + 16 * slot));
	}

	/*
	 * Ensure UFS-ICE registers are being configured
	 * before next operation, otherwise UFS Host Controller might
	 * set get errors
	 */
	mb();
out:
	return err;
}

/**
 * ufs_qcom_ice_cfg_end() - finishes configuring UFS's ICE registers
 *							for an ICE transaction
 * @qcom_host:	Pointer to a UFS QCom internal host structure.
 *				qcom_host, qcom_host->hba and
 *				qcom_host->hba->dev should all
 *				be valid pointers.
 * @cmd:	Pointer to a valid scsi command. cmd->request should also be
 *              a valid pointer.
 *
 * Return: -EINVAL in-case of an error
 *         0 otherwise
 */
int ufs_qcom_ice_cfg_end(struct ufs_qcom_host *qcom_host, struct request *req)
{
	int err = 0;
	struct device *dev = qcom_host->hba->dev;

	if (qcom_host->ice.vops->config_end) {
		err = qcom_host->ice.vops->config_end(req);
		if (err) {
			dev_err(dev, "%s: error in ice_vops->config_end %d\n",
				__func__, err);
			return err;
		}
	}

	return 0;
}

/**
 * ufs_qcom_ice_reset() - resets UFS-ICE interface and ICE device
 * @qcom_host:	Pointer to a UFS QCom internal host structure.
 *		qcom_host, qcom_host->hba and qcom_host->hba->dev should all
 *		be valid pointers.
 *
 * Return: -EINVAL in-case of an error
 *         0 otherwise
 */
int ufs_qcom_ice_reset(struct ufs_qcom_host *qcom_host)
{
	struct device *dev = qcom_host->hba->dev;
	int err = 0;

	if (!qcom_host->ice.pdev) {
		dev_dbg(dev, "%s: ice device is not enabled\n", __func__);
		goto out;
	}

	if (!qcom_host->ice.vops) {
		dev_err(dev, "%s: invalid ice_vops\n", __func__);
		return -EINVAL;
	}

	if (qcom_host->ice.state != UFS_QCOM_ICE_STATE_ACTIVE)
		goto out;

	if (qcom_host->ice.vops->reset) {
		err = qcom_host->ice.vops->reset(qcom_host->ice.pdev);
		if (err) {
			dev_err(dev, "%s: ice_vops->reset failed. err %d\n",
				__func__, err);
			goto out;
		}
	}

	if (qcom_host->ice.state != UFS_QCOM_ICE_STATE_ACTIVE) {
		dev_err(qcom_host->hba->dev,
			"%s: error. ice.state (%d) is not in active state\n",
			__func__, qcom_host->ice.state);
		err = -EINVAL;
	}

out:
	return err;
}

/**
 * ufs_qcom_ice_resume() - resumes UFS-ICE interface and ICE device from power
 * collapse
 * @qcom_host:	Pointer to a UFS QCom internal host structure.
 *		qcom_host, qcom_host->hba and qcom_host->hba->dev should all
 *		be valid pointers.
 *
 * Return: -EINVAL in-case of an error
 *         0 otherwise
 */
int ufs_qcom_ice_resume(struct ufs_qcom_host *qcom_host)
{
	struct device *dev = qcom_host->hba->dev;
	int err = 0;

	if (!qcom_host->ice.pdev) {
		dev_dbg(dev, "%s: ice device is not enabled\n", __func__);
		goto out;
	}

	if (qcom_host->ice.state !=
			UFS_QCOM_ICE_STATE_SUSPENDED) {
		goto out;
	}

	if (!qcom_host->ice.vops) {
		dev_err(dev, "%s: invalid ice_vops\n", __func__);
		return -EINVAL;
	}

	if (qcom_host->ice.vops->resume) {
		err = qcom_host->ice.vops->resume(qcom_host->ice.pdev);
		if (err) {
			dev_err(dev, "%s: ice_vops->resume failed. err %d\n",
				__func__, err);
			return err;
		}
	}
	qcom_host->ice.state = UFS_QCOM_ICE_STATE_ACTIVE;
out:
	return err;
}

/**
 * ufs_qcom_ice_suspend() - suspends UFS-ICE interface and ICE device
 * @qcom_host:	Pointer to a UFS QCom internal host structure.
 *		qcom_host, qcom_host->hba and qcom_host->hba->dev should all
 *		be valid pointers.
 *
 * Return: -EINVAL in-case of an error
 *         0 otherwise
 */
int ufs_qcom_ice_suspend(struct ufs_qcom_host *qcom_host)
{
	struct device *dev = qcom_host->hba->dev;
	int err = 0;

	if (!qcom_host->ice.pdev) {
		dev_dbg(dev, "%s: ice device is not enabled\n", __func__);
		goto out;
	}

	if (qcom_host->ice.vops->suspend) {
		err = qcom_host->ice.vops->suspend(qcom_host->ice.pdev);
		if (err) {
			dev_err(qcom_host->hba->dev,
				"%s: ice_vops->suspend failed. err %d\n",
				__func__, err);
			return -EINVAL;
		}
	}

	if (qcom_host->ice.state == UFS_QCOM_ICE_STATE_ACTIVE) {
		qcom_host->ice.state = UFS_QCOM_ICE_STATE_SUSPENDED;
	} else if (qcom_host->ice.state == UFS_QCOM_ICE_STATE_DISABLED) {
		dev_err(qcom_host->hba->dev,
				"%s: ice state is invalid: disabled\n",
				__func__);
		err = -EINVAL;
	}

out:
	return err;
}

/**
 * ufs_qcom_ice_get_status() - returns the status of an ICE transaction
 * @qcom_host:	Pointer to a UFS QCom internal host structure.
 *		qcom_host, qcom_host->hba and qcom_host->hba->dev should all
 *		be valid pointers.
 * @ice_status:	Pointer to a valid output parameter.
 *		< 0 in case of ICE transaction failure.
 *		0 otherwise.
 *
 * Return: -EINVAL in-case of an error
 *         0 otherwise
 */
int ufs_qcom_ice_get_status(struct ufs_qcom_host *qcom_host, int *ice_status)
{
	struct device *dev = NULL;
	int err = 0;
	int stat = -EINVAL;

	*ice_status = 0;

	dev = qcom_host->hba->dev;
	if (!dev) {
		err = -EINVAL;
		goto out;
	}

	if (!qcom_host->ice.pdev) {
		dev_dbg(dev, "%s: ice device is not enabled\n", __func__);
		goto out;
	}

	if (qcom_host->ice.state != UFS_QCOM_ICE_STATE_ACTIVE) {
		err = -EINVAL;
		goto out;
	}

	if (!qcom_host->ice.vops) {
		dev_err(dev, "%s: invalid ice_vops\n", __func__);
		return -EINVAL;
	}

	if (qcom_host->ice.vops->status) {
		stat = qcom_host->ice.vops->status(qcom_host->ice.pdev);
		if (stat < 0) {
			dev_err(dev, "%s: ice_vops->status failed. stat %d\n",
				__func__, stat);
			err = -EINVAL;
			goto out;
		}

		*ice_status = stat;
	}

out:
	return err;
}
