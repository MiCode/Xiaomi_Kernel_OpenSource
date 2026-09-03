// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS RPMB driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#include <asm/unaligned.h>
#include <linux/rpmb.h>

#include "ufshcd.h"
#include "ufs-sprd.h"
#include "ufs-sprd-rpmb.h"

#define PWRON_OR_RST_OCCURED_ASC	0x29
#define PWRON_OR_RST_OCCURED_ASCQ	0x00

static inline u16 ufs_sprd_wlun_to_scsi_lun(u8 upiu_wlun_id)
{
	return (upiu_wlun_id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE;
}

static int ufs_sprd_get_sdev(struct ufs_hba *hba, uint channel,
			     uint id, u64 lun)
{
	struct scsi_device *sdev_rpmb;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	sdev_rpmb = __scsi_add_device(hba->host, channel, id, lun, NULL);
	if (IS_ERR(sdev_rpmb)) {
		ret = PTR_ERR(sdev_rpmb);
		return ret;
	}
	host->sdev_ufs_rpmb = sdev_rpmb;

	return ret;
}

static inline int ufs_sprd_read_geometry_desc_param(struct ufs_hba *hba,
			enum geometry_desc_param param_offset,
			u8 *param_read_buf, u32 param_size)
{
	return ufshcd_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0,
				      param_offset, param_read_buf, param_size);
}

static int ufs_sprd_rpmb_security_out(struct scsi_device *sdev,
				      struct rpmb_frame *frames, u32 cnt)
{
	struct scsi_sense_hdr sshdr;
	u32 trans_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];
	char *sense = NULL;

	sense = kzalloc(SCSI_SENSE_BUFFERSIZE, GFP_NOIO);
	if (!sense) {
		pr_err("%s sense alloc failed\n", __func__);
		return -1;
	}

	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_OUT;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16(SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;
	put_unaligned_be32(trans_len, cmd + 6);

	ret = scsi_test_unit_ready(sdev, SEC_PROTOCOL_TIMEOUT,
				   SEC_PROTOCOL_RETRIES, &sshdr);
	if (ret)
		dev_err(&sdev->sdev_gendev,
			"%s: rpmb scsi_test_unit_ready, ret=%d\n",
			__func__, ret);

retry:
	ret = __scsi_execute(sdev, cmd, DMA_TO_DEVICE, frames, trans_len,
			     sense, &sshdr, SEC_PROTOCOL_TIMEOUT,
			     SEC_PROTOCOL_RETRIES, 0, 0, NULL);

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION &&
	    sshdr.asc == PWRON_OR_RST_OCCURED_ASC &&
	    sshdr.ascq == PWRON_OR_RST_OCCURED_ASCQ)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	if (ret)
		dev_err(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (scsi_status_is_check_condition(ret))
		scsi_print_sense_hdr(sdev, "rpmb: security out", &sshdr);

	kfree(sense);
	return ret;
}

static int ufs_sprd_rpmb_security_in(struct scsi_device *sdev,
				      struct rpmb_frame *frames, u32 cnt)
{
	struct scsi_sense_hdr sshdr;
	u32 alloc_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];
	char *sense = NULL;

	sense = kzalloc(SCSI_SENSE_BUFFERSIZE, GFP_NOIO);
	if (!sense) {
		pr_err("%s sense alloc failed\n", __func__);
		return -1;
	}
	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_IN;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16(SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;
	put_unaligned_be32(alloc_len, cmd + 6);

	ret = scsi_test_unit_ready(sdev, SEC_PROTOCOL_TIMEOUT,
				   SEC_PROTOCOL_RETRIES, &sshdr);
	if (ret)
		dev_err(&sdev->sdev_gendev,
			"%s: rpmb scsi_test_unit_ready, ret=%d\n",
			__func__, ret);

retry:
	ret = __scsi_execute(sdev, cmd, DMA_FROM_DEVICE, frames, alloc_len,
			     sense, &sshdr, SEC_PROTOCOL_TIMEOUT,
			     SEC_PROTOCOL_RETRIES, 0, 0, NULL);

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION &&
	    sshdr.asc == PWRON_OR_RST_OCCURED_ASC &&
	    sshdr.ascq == PWRON_OR_RST_OCCURED_ASCQ)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	if (ret)
		dev_err(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (scsi_status_is_check_condition(ret))
		scsi_print_sense_hdr(sdev, "rpmb: security in", &sshdr);

	kfree(sense);
	return ret;
}

static int ufs_rpmb_cmd_seq(struct device *dev,
			    struct rpmb_cmd *cmds, u32 ncmds)
{
	unsigned long flags;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	struct scsi_device *sdev;
	struct rpmb_cmd *cmd;
	int i;
	int ret;

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdev = host->sdev_ufs_rpmb;
	if (sdev) {
		ret = scsi_device_get(sdev);
		if (!ret && !scsi_device_online(sdev)) {
			ret = -ENODEV;
			scsi_device_put(sdev);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret)
		return ret;

	for (ret = 0, i = 0; i < ncmds && !ret; i++) {
		cmd = &cmds[i];
		if (cmd->flags & RPMB_F_WRITE)
			ret = ufs_sprd_rpmb_security_out(sdev, cmd->frames,
							 cmd->nframes);
		else
			ret = ufs_sprd_rpmb_security_in(sdev, cmd->frames,
							cmd->nframes);
	}
	scsi_device_put(sdev);

	return ret;
}

static struct rpmb_ops ufshcd_rpmb_dev_ops = {
	.cmd_seq = ufs_rpmb_cmd_seq,
	.type = RPMB_TYPE_UFS,
};

void ufs_sprd_rpmb_add(struct ufs_hba *hba)
{
	struct rpmb_dev *rdev;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);
	u8 rw_size;
	int ret;

	ret = ufs_sprd_get_sdev(hba, 0, 0,
			ufs_sprd_wlun_to_scsi_lun(UFS_UPIU_RPMB_WLUN));
	if (ret) {
		dev_warn(hba->dev, "Cannot get rpmb dev!\n");
		return;
	}

	ret = ufs_sprd_read_geometry_desc_param(hba, GEOMETRY_DESC_PARAM_RPMB_RW_SIZE,
					&rw_size, sizeof(rw_size));
	if (ret) {
		dev_warn(hba->dev, "%s: cannot get rpmb rw limit %d\n",
			dev_name(hba->dev), ret);
		rw_size = 1;
	}

	ufshcd_rpmb_dev_ops.reliable_wr_cnt = rw_size;

	ret = scsi_device_get(host->sdev_ufs_rpmb);
	rdev = rpmb_dev_register(hba->dev, &ufshcd_rpmb_dev_ops);
	if (IS_ERR(rdev)) {
		dev_warn(hba->dev, "%s: cannot register to rpmb %ld\n",
			 dev_name(hba->dev), PTR_ERR(rdev));
		goto out_put_dev;
	}

	scsi_device_put(host->sdev_ufs_rpmb);
	return;

out_put_dev:
	scsi_device_put(host->sdev_ufs_rpmb);
	host->sdev_ufs_rpmb = NULL;
}

void ufs_sprd_rpmb_remove(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (!host || !host->sdev_ufs_rpmb)
		return;

	rpmb_dev_unregister(hba->dev);
	scsi_device_put(host->sdev_ufs_rpmb);
	host->sdev_ufs_rpmb = NULL;
}

