// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Authors:
 *	Stanley Chu <stanley.chu@mediatek.com>
 *	Peter Wang <peter.wang@mediatek.com>
 */

#include <asm/unaligned.h>
#include <linux/arm-smccc.h>
#include <linux/bitfield.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/rpmb.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "ufshcd.h"
#include "ufshcd-crypto.h"
#include "ufshcd-pltfrm.h"
#include "ufs_quirks.h"
#include "unipro.h"
#include "ufs-mediatek.h"
#include "ufs-mediatek-dbg.h"
#include "ufs-mtk-block.h"

#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif

#define ufs_mtk_smc(cmd, val, res) \
	arm_smccc_smc(MTK_SIP_UFS_CONTROL, \
		      cmd, val, 0, 0, 0, 0, 0, &(res))

#define ufs_mtk_va09_pwr_ctrl(res, on) \
	ufs_mtk_smc(UFS_MTK_SIP_VA09_PWR_CTRL, on, res)

#define ufs_mtk_crypto_ctrl(res, enable) \
	ufs_mtk_smc(UFS_MTK_SIP_CRYPTO_CTRL, enable, res)

#define ufs_mtk_ref_clk_notify(on, res) \
	ufs_mtk_smc(UFS_MTK_SIP_REF_CLK_NOTIFICATION, on, res)

#define ufs_mtk_device_reset_ctrl(high, res) \
	ufs_mtk_smc(UFS_MTK_SIP_DEVICE_RESET, high, res)

int ufsdbg_perf_dump = 0;
static struct ufs_hba *ufs_mtk_hba;

static const struct ufs_mtk_host_cfg ufs_mtk_mt8183_cfg = {
	.quirks = UFS_MTK_HOST_QUIRK_BROKEN_AUTO_HIBERN8
};

static const struct of_device_id ufs_mtk_of_match[] = {
	{
		.compatible = "mediatek,mt8183-ufshci",
		.data = &ufs_mtk_mt8183_cfg
	},
	{},
};

struct rpmb_dev *ufs_mtk_rpmb_get_raw_dev()
{
	struct ufs_mtk_host *host = ufshcd_get_variant(ufs_mtk_hba);

	return host->rawdev_ufs_rpmb;
}

/* Read Geometry Descriptor for RPMB initialization */
static inline int ufshcd_read_geometry_desc_param(struct ufs_hba *hba,
				enum geometry_desc_param param_offset,
				u8 *param_read_buf, u32 param_size)
{
	return ufshcd_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0,
				      param_offset, param_read_buf, param_size);
}

/*
 * RPMB feature
 */
#define SEC_PROTOCOL_UFS  0xEC
#define SEC_SPECIFIC_UFS_RPMB 0x0001

#define SEC_PROTOCOL_CMD_SIZE 12
#define SEC_PROTOCOL_RETRIES 3
#define SEC_PROTOCOL_RETRIES_ON_RESET 10
#define SEC_PROTOCOL_TIMEOUT msecs_to_jiffies(30000)
int ufs_mtk_rpmb_security_out(struct scsi_device *sdev,
			 struct rpmb_frame *frames, u32 cnt)
{
	struct scsi_sense_hdr sshdr = {0};
	u32 trans_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];

	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_OUT;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16(SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;                              /* inc_512 bit 7 set to 0 */
	put_unaligned_be32(trans_len, cmd + 6);  /* transfer length */

	/* Ensure device is resumed before RPMB operation */
	scsi_autopm_get_device(sdev);

retry:
	ret = scsi_execute_req(sdev, cmd, DMA_TO_DEVICE,
				     frames, trans_len, &sshdr,
				     SEC_PROTOCOL_TIMEOUT, SEC_PROTOCOL_RETRIES,
				     NULL);

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	if (ret)
		dev_err(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (driver_byte(ret) & DRIVER_SENSE)
		scsi_print_sense_hdr(sdev, "rpmb: security out", &sshdr);

	/* Allow device to be runtime suspended */
	scsi_autopm_put_device(sdev);

	return ret;
}

int ufs_mtk_rpmb_security_in(struct scsi_device *sdev,
			struct rpmb_frame *frames, u32 cnt)
{
	struct scsi_sense_hdr sshdr = {0};
	u32 alloc_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];

	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_IN;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16(SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;                             /* inc_512 bit 7 set to 0 */
	put_unaligned_be32(alloc_len, cmd + 6); /* allocation length */

	/* Ensure device is resumed before RPMB operation */
	scsi_autopm_get_device(sdev);

retry:
	ret = scsi_execute_req(sdev, cmd, DMA_FROM_DEVICE,
				     frames, alloc_len, &sshdr,
				     SEC_PROTOCOL_TIMEOUT, SEC_PROTOCOL_RETRIES,
				     NULL);

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	/* Allow device to be runtime suspended */
	scsi_autopm_put_device(sdev);

	if (ret)
		dev_err(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (driver_byte(ret) & DRIVER_SENSE)
		scsi_print_sense_hdr(sdev, "rpmb: security in", &sshdr);

	return ret;
}

static int ufs_mtk_rpmb_cmd_seq(struct device *dev,
			       struct rpmb_cmd *cmds, u32 ncmds)
{
	unsigned long flags;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
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

	/*
	 * Send all command one by one.
	 * Use rpmb lock to prevent other rpmb read/write threads cut in line.
	 * Use mutex not spin lock because in/out function might sleep.
	 */
	mutex_lock(&host->rpmb_lock);
	for (ret = 0, i = 0; i < ncmds && !ret; i++) {
		cmd = &cmds[i];
		if (cmd->flags & RPMB_F_WRITE)
			ret = ufs_mtk_rpmb_security_out(sdev, cmd->frames,
						       cmd->nframes);
		else
			ret = ufs_mtk_rpmb_security_in(sdev, cmd->frames,
						      cmd->nframes);
	}
	mutex_unlock(&host->rpmb_lock);

	scsi_device_put(sdev);
	return ret;
}

static struct rpmb_ops ufs_mtk_rpmb_dev_ops = {
	.cmd_seq = ufs_mtk_rpmb_cmd_seq,
	.type = RPMB_TYPE_UFS,
};

void ufs_mtk_rpmb_add(struct ufs_hba *hba, struct scsi_device *sdev_rpmb)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct rpmb_dev *rdev;
	u8 rw_size;
	int ret;

	host->sdev_ufs_rpmb = sdev_rpmb;
	ret = ufshcd_read_geometry_desc_param(hba,
		GEOMETRY_DESC_PARAM_RPMB_RW_SIZE,
		&rw_size, sizeof(rw_size));
	if (ret) {
		dev_warn(hba->dev, "%s: cannot get rpmb rw limit %d\n",
			 dev_name(hba->dev), ret);
		/* fallback to singel frame write */
		rw_size = 1;
	}

	if (hba->dev_quirks & UFS_DEVICE_QUIRK_LIMITED_RPMB_MAX_RW_SIZE) {
		if (rw_size > 8)
			rw_size = 8;
	}

	dev_info(hba->dev, "rpmb rw_size: %d\n", rw_size);

	ufs_mtk_rpmb_dev_ops.reliable_wr_cnt = rw_size;

	/* MTK PATCH: Add handling for scsi_device_get */
	if (unlikely(scsi_device_get(host->sdev_ufs_rpmb)))
		goto out_put_dev;

	rdev = rpmb_dev_register(hba->dev, &ufs_mtk_rpmb_dev_ops);
	if (IS_ERR(rdev)) {
		dev_warn(hba->dev, "%s: cannot register to rpmb %ld\n",
			 dev_name(hba->dev), PTR_ERR(rdev));
		goto out_put_dev;
	}

	/*
	 * MTK PATCH: Preserve rpmb_dev to globals for connection of legacy
	 *            rpmb ioctl solution.
	 */
	host->rawdev_ufs_rpmb = rdev;

	return;

out_put_dev:
	scsi_device_put(host->sdev_ufs_rpmb);
	host->sdev_ufs_rpmb = NULL;
	host->rawdev_ufs_rpmb = NULL;
}

void ufs_mtk_rpmb_remove(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	unsigned long flags;

	if (!host->sdev_ufs_rpmb || !hba->host)
		return;

	rpmb_dev_unregister(hba->dev);

	/*
	 * MTK Bug Fix:
	 *
	 * To prevent calling schedule() with preemption disabled,
	 * spin_lock_irqsave shall be behind rpmb_dev_unregister().
	 */

	spin_lock_irqsave(hba->host->host_lock, flags);

	scsi_device_put(host->sdev_ufs_rpmb);
	host->sdev_ufs_rpmb = NULL;
	host->rawdev_ufs_rpmb = NULL;

	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

void ufs_mtk_rpmb_quiesce(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (host->sdev_ufs_rpmb)
		scsi_device_quiesce(host->sdev_ufs_rpmb);
}

/**
 * ufs_mtk_ioctl_rpmb - perform user rpmb read/write request
 * @hba: per-adapter instance
 * @buf_user: user space buffer for ioctl rpmb_cmd data
 * @return: 0 for success negative error code otherwise
 *
 * Expected/Submitted buffer structure is struct rpmb_cmd.
 * It will read/write data to rpmb
 */
int ufs_mtk_ioctl_rpmb(struct ufs_hba *hba, void __user *buf_user)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct rpmb_cmd cmd[3];
	struct rpmb_frame *frame_buf = NULL;
	struct rpmb_frame *frames = NULL;
	int size = 0;
	int nframes = 0;
	unsigned long flags;
	struct scsi_device *sdev;
	int ret;
	int i;

	/* Get scsi device */
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
	if (ret) {
		dev_info(hba->dev,
			"%s: failed get rpmb device, ret %d\n",
			__func__, ret);
		goto out;
	}

	/* Get cmd params from user buffer */
	ret = copy_from_user((void *) cmd,
		buf_user, sizeof(struct rpmb_cmd) * 3);
	if (ret) {
		dev_info(hba->dev,
			"%s: failed copying cmd buffer from user, ret %d\n",
			__func__, ret);
		goto out_put;
	}

	/* Check number of rpmb frames */
	for (i = 0; i < 3; i++) {
		ret = (int)rpmb_get_rw_size(ufs_mtk_rpmb_get_raw_dev());
		if (cmd[i].nframes > ret) {
			dev_info(hba->dev,
				"%s: number of rpmb frames %u exceeds limit %d\n",
				__func__, cmd[i].nframes, ret);
			ret = -EINVAL;
			goto out_put;
		}
	}

	/* Prepaer frame buffer */
	for (i = 0; i < 3; i++)
		nframes += cmd[i].nframes;
	frame_buf = kcalloc(nframes, sizeof(struct rpmb_frame), GFP_KERNEL);
	if (!frame_buf) {
		ret = -ENOMEM;
		goto out_put;
	}
	frames = frame_buf;

	/*
	 * Send all command one by one.
	 * Use rpmb lock to prevent other rpmb read/write threads cut in line.
	 * Use mutex not spin lock because in/out function might sleep.
	 */
	mutex_lock(&host->rpmb_lock);
	for (i = 0; i < 3; i++) {
		if (cmd[i].nframes == 0)
			break;

		/* Get frames from user buffer */
		size = sizeof(struct rpmb_frame) * cmd[i].nframes;
		ret = copy_from_user((void *) frames, cmd[i].frames, size);
		if (ret) {
			dev_err(hba->dev,
				"%s: failed from user, ret %d\n",
				__func__, ret);
			break;
		}

		/* Do rpmb in out */
		if (cmd[i].flags & RPMB_F_WRITE) {
			ret = ufs_mtk_rpmb_security_out(sdev, frames,
						       cmd[i].nframes);
			if (ret) {
				dev_err(hba->dev,
					"%s: failed rpmb out, err %d\n",
					__func__, ret);
				break;
			}

		} else {
			ret = ufs_mtk_rpmb_security_in(sdev, frames,
						      cmd[i].nframes);
			if (ret) {
				dev_err(hba->dev,
					"%s: failed rpmb in, err %d\n",
					__func__, ret);
				break;
			}

			/* Copy frames to user buffer */
			ret = copy_to_user((void *) cmd[i].frames,
				frames, size);
			if (ret) {
				dev_err(hba->dev,
					"%s: failed to user, err %d\n",
					__func__, ret);
				break;
			}
		}

		frames += cmd[i].nframes;
	}
	mutex_unlock(&host->rpmb_lock);

	kfree(frame_buf);

out_put:
	scsi_device_put(sdev);
out:
	return ret;
}

bool ufs_mtk_has_broken_auto_hibern8(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	return (ufshcd_is_auto_hibern8_supported(hba) && hba->ahit &&
		host->cfg &&
		(host->cfg->quirks & UFS_MTK_HOST_QUIRK_BROKEN_AUTO_HIBERN8));
}

bool ufs_mtk_get_unipro_lpm(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	return host->unipro_lpm;
}

static void ufs_mtk_parse_dt(struct ufs_mtk_host *host)
{
	struct ufs_hba *hba = host->hba;
	struct device *dev = hba->dev;
	int ret;

	/*
	 * Parse reference clock control setting
	 * SW mode:      0 (use external function to control ref-clk)
	 * Half-HW mode: 1 (use ufshci register to control ref-clk,
	 *                  but cannot turn off)
	 * HW mode:      2 (use ufshci register to control ref-clk)
	 */
	ret = of_property_read_u32(dev->of_node, "mediatek,refclk_ctrl",
				   &host->refclk_ctrl);
	if (ret) {
		dev_dbg(hba->dev,
			"%s: failed to read mediatek,refclk_ctrl, ret=%d\n",
			__func__, ret);
		host->refclk_ctrl = REF_CLK_SW_MODE;
	}

	/* get and enable va09 regulator */
	host->reg_va09 = regulator_get(hba->dev, "va09");
	if (!host->reg_va09) {
		dev_info(hba->dev, "%s: failed to get va09!\n",
			 __func__);
		return;
	}
}

void ufs_mtk_cfg_unipro_cg(struct ufs_hba *hba, bool enable)
{
	u32 tmp = 0;

	if (enable) {
		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
		tmp = tmp |
		      (1 << RX_SYMBOL_CLK_GATE_EN) |
		      (1 << SYS_CLK_GATE_EN) |
		      (1 << TX_CLK_GATE_EN);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp & ~(1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), tmp);
	} else {
		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
		tmp = tmp & ~((1 << RX_SYMBOL_CLK_GATE_EN) |
			      (1 << SYS_CLK_GATE_EN) |
			      (1 << TX_CLK_GATE_EN));
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

		ufshcd_dme_get(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), &tmp);
		tmp = tmp | (1 << TX_SYMBOL_CLK_REQ_FORCE);
		ufshcd_dme_set(hba,
			       UIC_ARG_MIB(VS_DEBUGCLOCKENABLE), tmp);
	}
}

static void ufs_mtk_crypto_enable(struct ufs_hba *hba)
{
	struct arm_smccc_res res;

	ufs_mtk_crypto_ctrl(res, 1);
	if (res.a0) {
		dev_info(hba->dev, "%s: crypto enable failed, err: %lu\n",
			 __func__, res.a0);
	}
}

static void ufs_mtk_host_reset(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	reset_control_assert(host->hci_reset);
	reset_control_assert(host->crypto_reset);
	reset_control_assert(host->unipro_reset);

	usleep_range(100, 110);

	reset_control_deassert(host->unipro_reset);
	reset_control_deassert(host->crypto_reset);
	reset_control_deassert(host->hci_reset);
}

static int ufs_mtk_init_reset_control(struct ufs_hba *hba,
				      struct reset_control **rc,
				      char *str)
{
	*rc = devm_reset_control_get(hba->dev, str);
	if (IS_ERR(*rc)) {
		dev_info(hba->dev, "Failed to get %s: %d\n", str,
			PTR_ERR(*rc));
		*rc = NULL;
		return PTR_ERR(*rc);
	}

	return 0;
}

static void ufs_mtk_init_reset(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ufs_mtk_init_reset_control(hba, &host->hci_reset,
				   "hci_rst");
	ufs_mtk_init_reset_control(hba, &host->unipro_reset,
				   "unipro_rst");
	ufs_mtk_init_reset_control(hba, &host->crypto_reset,
				   "crypto_rst");
}

static int ufs_mtk_hce_enable_notify(struct ufs_hba *hba,
				     enum ufs_notify_change_status status)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (status == PRE_CHANGE) {
		if (host->unipro_lpm) {
			hba->hba_enable_delay_us = 0;
		} else {
			hba->hba_enable_delay_us = 600;
			ufs_mtk_host_reset(hba);
		}

		if (ufshcd_hba_is_crypto_supported(hba))
			ufs_mtk_crypto_enable(hba);
	}

	return 0;
}

static int ufs_mtk_bind_mphy(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;
	int err = 0;

	host->mphy = devm_of_phy_get_by_index(dev, np, 0);

	if (host->mphy == ERR_PTR(-EPROBE_DEFER)) {
		/*
		 * UFS driver might be probed before the phy driver does.
		 * In that case we would like to return EPROBE_DEFER code.
		 */
		err = -EPROBE_DEFER;
		dev_info(dev,
			 "%s: required phy hasn't probed yet. err = %d\n",
			__func__, err);
	} else if (IS_ERR(host->mphy)) {
		err = PTR_ERR(host->mphy);
		if (err != -ENODEV)
			dev_info(dev, "%s: PHY get failed %d\n", __func__,
				 err);
	}

	if (err)
		host->mphy = NULL;
	/*
	 * Allow unbound mphy because not every platform needs specific
	 * mphy control
	 */
	if (err == -ENODEV)
		err = 0;

	return err;
}

static int ufs_mtk_setup_ref_clk(struct ufs_hba *hba, bool on)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct arm_smccc_res res;
	ktime_t timeout, time_checked;
	u32 value;

	if (host->ref_clk_enabled == on)
		return 0;

	if (on) {
		ufs_mtk_ref_clk_notify(on, res);
		ufshcd_delay_us(host->ref_clk_ungating_wait_us, 10);
	}

	/* This is HW and Half-HW flow, SW flow should ignore */
	if (host->refclk_ctrl == REF_CLK_SW_MODE)
		goto out;

	/* Half-HW mode cannot turn off ref-clk, release xoufs spm req only */
	if (host->refclk_ctrl == REF_CLK_HALF_HW_MODE) {
		ufshcd_writel(hba, REFCLK_RELEASE, REG_UFS_REFCLK_CTRL);
		goto out;
	}

	/*
	 * REG_UFS_REFCLK_CTRL[0] is xoufs_req_s
	 * REG_UFS_REFCLK_CTRL[1] is xoufs_ack_s
	 * xoufs_req_s is used for XOUFS Clock request to SPM
	 * SW sets xoufs_ack_s to trigger Clock Request for XOUFS, and
	 * check xoufs_ack_s set for clock avialable.
	 * SW clears xoufs_ack_s to trigger Clock Release for XOUFS, and
	 * check xoufs_ack_s clear for clock off.
	 */
	if (on)
		ufshcd_writel(hba, REFCLK_REQUEST, REG_UFS_REFCLK_CTRL);
	else
		ufshcd_writel(hba, REFCLK_RELEASE, REG_UFS_REFCLK_CTRL);

	/* Wait for ack */
	timeout = ktime_add_us(ktime_get(), REFCLK_REQ_TIMEOUT_US);
	do {
		time_checked = ktime_get();
		value = ufshcd_readl(hba, REG_UFS_REFCLK_CTRL);

		/* Wait until ack bit equals to req bit */
		if (((value & REFCLK_ACK) >> 1) == (value & REFCLK_REQUEST))
			goto out;

		usleep_range(100, 200);
	} while (ktime_before(time_checked, timeout));

	dev_err(hba->dev, "missing ack of refclk req, reg: 0x%x\n", value);

	ufs_mtk_ref_clk_notify(host->ref_clk_enabled, res);

	return -ETIMEDOUT;

out:
	host->ref_clk_enabled = on;
	if (!on) {
		ufshcd_delay_us(host->ref_clk_gating_wait_us, 10);
		ufs_mtk_ref_clk_notify(on, res);
	}

	return 0;
}

static void ufs_mtk_setup_ref_clk_wait_us(struct ufs_hba *hba,
					  u16 gating_us, u16 ungating_us)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (hba->dev_info.clk_gating_wait_us) {
		host->ref_clk_gating_wait_us =
			hba->dev_info.clk_gating_wait_us;
	} else {
		host->ref_clk_gating_wait_us = gating_us;
	}

	host->ref_clk_ungating_wait_us = ungating_us;
}

int ufs_mtk_wait_link_state(struct ufs_hba *hba, u32 state,
			    unsigned long max_wait_ms)
{
	ktime_t timeout, time_checked;
	u32 val;

	timeout = ktime_add_ms(ktime_get(), max_wait_ms);
	do {
		time_checked = ktime_get();
		ufshcd_writel(hba, 0x20, REG_UFS_DEBUG_SEL);
		val = ufshcd_readl(hba, REG_UFS_PROBE);
		val = val >> 28;

		if (val == state)
			return 0;

		/* Sleep for max. 200us */
		usleep_range(100, 200);
	} while (ktime_before(time_checked, timeout));

	if (val == state)
		return 0;

	return -ETIMEDOUT;
}

static void ufs_mtk_mphy_power_on(struct ufs_hba *hba, bool on)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct phy *mphy = host->mphy;
	struct arm_smccc_res res;
	int ret;

	if (!mphy)
		return;

	if (on && !host->mphy_powered_on) {
		ret = regulator_enable(host->reg_va09);
		if (ret < 0) {
			dev_info(hba->dev,
				"%s: failed to enable va09: %d\n",
				__func__, ret);
		}
		/* wait 200 us to stablize VA09 */
		udelay(200);
		ufs_mtk_va09_pwr_ctrl(res, 1);
		phy_power_on(mphy);
	} else if (!on && host->mphy_powered_on) {
		phy_power_off(mphy);
		ufs_mtk_va09_pwr_ctrl(res, 0);
		ret = regulator_disable(host->reg_va09);
		if (ret < 0) {
			dev_info(hba->dev,
				 "%s: failed to disable va09: %d\n",
				 __func__, ret);
		}
	}
	else
		return;
	host->mphy_powered_on = on;
}

/**
 * ufs_mtk_setup_clocks - enables/disable clocks
 * @hba: host controller instance
 * @on: If true, enable clocks else disable them.
 * @status: PRE_CHANGE or POST_CHANGE notify
 *
 * Returns 0 on success, non-zero on failure.
 */
static int ufs_mtk_setup_clocks(struct ufs_hba *hba, bool on,
				enum ufs_notify_change_status status)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	int ret = 0;

	/*
	 * In case ufs_mtk_init() is not yet done, simply ignore.
	 * This ufs_mtk_setup_clocks() shall be called from
	 * ufs_mtk_init() after init is done.
	 */
	if (!host)
		return 0;

	if (!on && status == PRE_CHANGE) {
		/*
		 * Gate ref-clk and poweroff mphy if link state is in OFF
		 * or Hibern8 by either ufshcd_link_state_transition() or
		 * Auto-Hibern8.
		 */
		if (!ufshcd_is_link_active(hba) ||
			(!ufshcd_can_hibern8_during_gating(hba) &&
			ufshcd_is_auto_hibern8_enabled(hba))) {
			ret = ufs_mtk_wait_link_state(hba,
						      VS_LINK_HIBERN8,
						      15);
			if (!ret) {
				ufs_mtk_setup_ref_clk(hba, on);
				ufs_mtk_mphy_power_on(hba, on);
			}
		}
	} else if (on && status == POST_CHANGE) {
		ufs_mtk_mphy_power_on(hba, on);
		ufs_mtk_setup_ref_clk(hba, on);
	}

	return ret;
}

/**
 * ufs_mtk_init - find other essential mmio bases
 * @hba: host controller instance
 *
 * Binds PHY with controller and powers up PHY enabling clocks
 * and regulators.
 *
 * Returns -EPROBE_DEFER if binding fails, returns negative error
 * on phy power up failure and returns zero on success.
 */
static int ufs_mtk_init(struct ufs_hba *hba)
{
	const struct of_device_id *id;
	struct device *dev = hba->dev;
	struct ufs_mtk_host *host;
	int err = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host) {
		err = -ENOMEM;
		dev_info(dev, "%s: no memory for mtk ufs host\n", __func__);
		goto out;
	}

	ufs_mtk_hba = hba;
	host->hba = hba;
	ufshcd_set_variant(hba, host);

	/* Get host quirks */
	id = of_match_device(ufs_mtk_of_match, dev);
	if (!id) {
		err = -EINVAL;
		goto out;
	}

	if (id->data) {
		host->cfg = (struct ufs_mtk_host_cfg *)id->data;
		if (host->cfg->quirks & UFS_MTK_HOST_QUIRK_BROKEN_AUTO_HIBERN8)
			host->auto_hibern_enabled = true;
	}

	err = ufs_mtk_bind_mphy(hba);
	if (err)
		goto out_variant_clear;

	ufs_mtk_init_reset(hba);

	ufs_mtk_parse_dt(host);

	/* Enable runtime autosuspend */
	hba->caps |= UFSHCD_CAP_RPM_AUTOSUSPEND;

	/* Enable clock-gating */
	hba->caps |= UFSHCD_CAP_CLK_GATING;

	/* Allow auto bkops to enabled during runtime suspend */
	/* Need to fix VCCQ2 issue first */
	/* hba->caps |= UFSHCD_CAP_AUTO_BKOPS_SUSPEND; */

	/*
	 * ufshcd_vops_init() is invoked after
	 * ufshcd_setup_clock(true) in ufshcd_hba_init() thus
	 * phy clock setup is skipped.
	 *
	 * Enable phy clocks specifically here.
	 */
	ufs_mtk_setup_clocks(hba, true, POST_CHANGE);

	ufsdbg_register(hba->dev);

	goto out;

out_variant_clear:
	ufshcd_set_variant(hba, NULL);
out:
	return err;
}

static void _ufs_mtk_auto_hibern8_update(struct ufs_hba *hba, bool enable)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	/*
	 * To prevent dummy "enable" while multiple slots are finished in
	 * the same loop in __ufshcd_transfer_req_compl().
	 */
	if (enable && host->auto_hibern_enabled)
		return;

	ufshcd_writel(hba, (enable) ? hba->ahit : 0,
		REG_AUTO_HIBERNATE_IDLE_TIMER);
	host->auto_hibern_enabled = enable;
}

static void ufs_mtk_auto_hibern8_update(struct ufs_hba *hba, bool enable)
{
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	_ufs_mtk_auto_hibern8_update(hba, enable);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

static int ufs_mtk_pre_pwr_change(struct ufs_hba *hba,
				  struct ufs_pa_layer_attr *dev_max_params,
				  struct ufs_pa_layer_attr *dev_req_params)
{
	struct ufs_dev_params host_cap;
	int ret;

	if (ufs_mtk_has_broken_auto_hibern8(hba))
		ufs_mtk_auto_hibern8_update(hba, false);

	host_cap.tx_lanes = UFS_MTK_LIMIT_NUM_LANES_TX;
	host_cap.rx_lanes = UFS_MTK_LIMIT_NUM_LANES_RX;
	host_cap.hs_rx_gear = UFS_MTK_LIMIT_HSGEAR_RX;
	host_cap.hs_tx_gear = UFS_MTK_LIMIT_HSGEAR_TX;
	host_cap.pwm_rx_gear = UFS_MTK_LIMIT_PWMGEAR_RX;
	host_cap.pwm_tx_gear = UFS_MTK_LIMIT_PWMGEAR_TX;
	host_cap.rx_pwr_pwm = UFS_MTK_LIMIT_RX_PWR_PWM;
	host_cap.tx_pwr_pwm = UFS_MTK_LIMIT_TX_PWR_PWM;
	host_cap.rx_pwr_hs = UFS_MTK_LIMIT_RX_PWR_HS;
	host_cap.tx_pwr_hs = UFS_MTK_LIMIT_TX_PWR_HS;
	host_cap.hs_rate = UFS_MTK_LIMIT_HS_RATE;
	host_cap.desired_working_mode =
				UFS_MTK_LIMIT_DESIRED_MODE;

	ret = ufshcd_get_pwr_dev_param(&host_cap,
				       dev_max_params,
				       dev_req_params);
	if (ret) {
		pr_info("%s: failed to determine capabilities\n",
			__func__);
	}

	return ret;
}

static int ufs_mtk_pwr_change_notify(struct ufs_hba *hba,
				     enum ufs_notify_change_status stage,
				     struct ufs_pa_layer_attr *dev_max_params,
				     struct ufs_pa_layer_attr *dev_req_params)
{
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		ret = ufs_mtk_pre_pwr_change(hba, dev_max_params,
					     dev_req_params);
		break;
	case POST_CHANGE:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ufs_mtk_unipro_set_pm(struct ufs_hba *hba, bool lpm)
{
	int ret;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	ret = ufshcd_dme_set(hba,
			     UIC_ARG_MIB_SEL(VS_UNIPROPOWERDOWNCONTROL, 0),
			     lpm);
	if (!ret || !lpm) {
		/*
		 * Forcibly set as non-LPM mode if UIC commands is failed
		 * to use default hba_enable_delay_us value for re-enabling
		 * the host.
		 */
		host->unipro_lpm = lpm;
	}

	return ret;
}

static int ufs_mtk_pre_link(struct ufs_hba *hba)
{
	int ret;
	u32 tmp;

	ret = ufs_mtk_unipro_set_pm(hba, false);
	if (ret)
		return ret;

	/*
	 * Setting PA_Local_TX_LCC_Enable to 0 before link startup
	 * to make sure that both host and device TX LCC are disabled
	 * once link startup is completed.
	 */
	ret = ufshcd_disable_host_tx_lcc(hba);
	if (ret)
		return ret;

	/* disable deep stall */
	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(VS_SAVEPOWERCONTROL), &tmp);
	if (ret)
		return ret;

	tmp &= ~(1 << 6);

	ret = ufshcd_dme_set(hba, UIC_ARG_MIB(VS_SAVEPOWERCONTROL), tmp);

	return ret;
}

static void ufs_mtk_setup_clk_gating(struct ufs_hba *hba)
{
	unsigned long flags;
	u32 delay;

	if (ufshcd_is_clkgating_allowed(hba)) {
		if (ufshcd_is_auto_hibern8_supported(hba) && hba->ahit)
			delay = FIELD_GET(UFSHCI_AHIBERN8_TIMER_MASK,
					  hba->ahit) + 5;
		else
			delay = 15;
		spin_lock_irqsave(hba->host->host_lock, flags);
		hba->clk_gating.delay_ms = delay;
		spin_unlock_irqrestore(hba->host->host_lock, flags);
	}
}

static int ufs_mtk_post_link(struct ufs_hba *hba)
{
	/* enable unipro clock gating feature */
	ufs_mtk_cfg_unipro_cg(hba, true);

	/* configure auto-hibern8 timer to 10 ms */
	if (ufshcd_is_auto_hibern8_supported(hba)) {
		ufshcd_auto_hibern8_update(hba,
			FIELD_PREP(UFSHCI_AHIBERN8_TIMER_MASK, 10) |
			FIELD_PREP(UFSHCI_AHIBERN8_SCALE_MASK, 3));
	}

	ufs_mtk_setup_clk_gating(hba);

	return 0;
}

static int ufs_mtk_link_startup_notify(struct ufs_hba *hba,
				       enum ufs_notify_change_status stage)
{
	int ret = 0;

	switch (stage) {
	case PRE_CHANGE:
		ret = ufs_mtk_pre_link(hba);
		break;
	case POST_CHANGE:
		ret = ufs_mtk_post_link(hba);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void ufs_mtk_device_reset(struct ufs_hba *hba)
{
	struct arm_smccc_res res;

	ufs_mtk_device_reset_ctrl(0, res);

	/*
	 * The reset signal is active low. UFS devices shall detect
	 * more than or equal to 1us of positive or negative RST_n
	 * pulse width.
	 *
	 * To be on safe side, keep the reset low for at least 10us.
	 */
	usleep_range(10, 15);

	ufs_mtk_device_reset_ctrl(1, res);

	/* Some devices may need time to respond to rst_n */
	usleep_range(10000, 15000);

	dev_info(hba->dev, "device reset done\n");
}

static int ufs_mtk_link_set_hpm(struct ufs_hba *hba)
{
	int err;

	err = ufshcd_hba_enable(hba);
	if (err)
		goto out;

	err = ufs_mtk_unipro_set_pm(hba, false);
	if (err)
		goto out;

	err = ufshcd_uic_hibern8_exit(hba);
	if (!err)
		ufshcd_set_link_active(hba);
	else
		goto out;

	err = ufshcd_make_hba_operational(hba);
out:
	if (err)
		ufshcd_print_info(hba, UFS_INFO_HOST_STATE |
				  UFS_INFO_HOST_REGS | UFS_INFO_PWR);
	return err;
}

static int ufs_mtk_link_set_lpm(struct ufs_hba *hba)
{
	int err;

	err = ufs_mtk_unipro_set_pm(hba, true);
	if (err) {
		ufshcd_print_info(hba, UFS_INFO_HOST_STATE |
				  UFS_INFO_HOST_REGS | UFS_INFO_PWR);

		/* Resume UniPro state for following error recovery */
		ufs_mtk_unipro_set_pm(hba, false);
		return err;
	}

	return 0;
}

static void ufs_mtk_vreg_set_lpm(struct ufs_hba *hba, bool lpm)
{
	if (!hba->vreg_info.vccq2)
		return;

	if (lpm & !hba->vreg_info.vcc->enabled)
		regulator_set_mode(hba->vreg_info.vccq2->reg,
				   REGULATOR_MODE_IDLE);
	else if (!lpm)
		regulator_set_mode(hba->vreg_info.vccq2->reg,
				   REGULATOR_MODE_NORMAL);
}

static int ufs_mtk_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int err;

	if (ufshcd_is_link_hibern8(hba)) {
		err = ufs_mtk_link_set_lpm(hba);
		if (err) {
			/*
			 * Set link as off state enforcedly to trigger
			 * ufshcd_host_reset_and_restore() in ufshcd_suspend()
			 * for completed host reset.
			 */
			ufshcd_set_link_off(hba);
			return -EAGAIN;
		}
		/*
		 * Make sure no error will be returned to prevent
		 * ufshcd_suspend() re-enabling regulators while vreg is still
		 * in low-power mode.
		 */
		ufs_mtk_vreg_set_lpm(hba, true);
	}

	return 0;
}

static int ufs_mtk_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	int err;

	if (ufshcd_is_link_hibern8(hba)) {
		ufs_mtk_vreg_set_lpm(hba, false);
		err = ufs_mtk_link_set_hpm(hba);
		if (err) {
			err = ufshcd_link_recovery(hba);
			return err;
		}
	}

	return 0;
}

static void ufs_mtk_dbg_register_dump(struct ufs_hba *hba)
{
	ufshcd_dump_regs(hba, REG_UFS_REFCLK_CTRL, 0x4, "Ref-Clk Ctrl ");

	ufshcd_dump_regs(hba, REG_UFS_EXTREG, 0x4, "Ext Reg ");

	ufshcd_dump_regs(hba, REG_UFS_MPHYCTRL,
			 REG_UFS_REJECT_MON - REG_UFS_MPHYCTRL + 4,
			 "MPHY Ctrl ");

	/* Direct debugging information to REG_MTK_PROBE */
	ufshcd_writel(hba, 0x20, REG_UFS_DEBUG_SEL);
	ufshcd_dump_regs(hba, REG_UFS_PROBE, 0x4, "Debug Probe ");
}

static int ufs_mtk_apply_dev_quirks(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	u16 mid = dev_info->wmanufacturerid;

	if (mid == UFS_VENDOR_SAMSUNG)
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE), 6);

	/*
	 * Decide waiting time before gating reference clock and
	 * after ungating reference clock according to vendors'
	 * requirements.
	 */
	if (mid == UFS_VENDOR_SAMSUNG)
		ufs_mtk_setup_ref_clk_wait_us(hba, 1, 1);
	else if (mid == UFS_VENDOR_SKHYNIX)
		ufs_mtk_setup_ref_clk_wait_us(hba, 30, 30);
	else if (mid == UFS_VENDOR_TOSHIBA)
		ufs_mtk_setup_ref_clk_wait_us(hba, 100, 32);

	return 0;
}

static void ufs_mtk_abort_handler(struct ufs_hba *hba, int tag,
				  char *file, int line)
{
#ifdef CONFIG_MTK_AEE_FEATURE
	u8 cmd = 0;

	if (hba->lrb[tag].cmd)
		cmd = hba->lrb[tag].cmd->cmnd[0];

	cmd_hist_disable();
	ufs_mediatek_dbg_dump();
	aee_kernel_warning_api(file, line, DB_OPT_FS_IO_LOG,
		"[UFS] Command Timeout", "Command 0x%x timeout, %s:%d", cmd,
		file, line);
	cmd_hist_enable();
#endif
}

static void ufs_mtk_handle_broken_auto_hibern8(struct ufs_hba *hba,
					       unsigned long out_reqs,
					       bool enable)
{
	/*
	 * Always allow "disable" and allow "enable" in non-PM scenario
	 * only. For PM scenario, auto-hibern8 will be enabled by core
	 * driver, e.g., ufshcd_resume().
	 */
	if (!out_reqs && !hba->outstanding_tasks &&
		(!enable || (enable && !hba->pm_op_in_progress)))
		_ufs_mtk_auto_hibern8_update(hba, enable);
}

static void ufs_mtk_setup_xfer_req(struct ufs_hba *hba, int tag,
				   bool is_scsi_cmd)
{
	if (!ufs_mtk_has_broken_auto_hibern8(hba))
		return;
	ufs_mtk_handle_broken_auto_hibern8(hba, hba->outstanding_reqs, false);
}

static void ufs_mtk_compl_xfer_req(struct ufs_hba *hba, int tag,
				   unsigned long completed_reqs,
				   bool is_scsi_cmd)
{
	if (!ufs_mtk_has_broken_auto_hibern8(hba))
		return;
	ufs_mtk_handle_broken_auto_hibern8(hba,
				hba->outstanding_reqs ^ completed_reqs,
				true);
}

static void ufs_mtk_setup_task_mgmt(struct ufs_hba *hba,
				    int tag, u8 tm_function)
{
	if (!ufs_mtk_has_broken_auto_hibern8(hba))
		return;
	ufs_mtk_handle_broken_auto_hibern8(hba, hba->outstanding_reqs, false);
}

static void ufs_mtk_compl_task_mgmt(struct ufs_hba *hba,
				    int tag, int err)
{
	if (!ufs_mtk_has_broken_auto_hibern8(hba))
		return;
	ufs_mtk_handle_broken_auto_hibern8(hba, hba->outstanding_reqs, true);
}

static void ufs_mtk_hibern8_notify(struct ufs_hba *hba, enum uic_cmd_dme cmd,
				   enum ufs_notify_change_status status)
{
	int ret;

	if (!ufs_mtk_has_broken_auto_hibern8(hba))
		return;

	if (cmd == UIC_CMD_DME_HIBER_ENTER && status == PRE_CHANGE) {
		ufs_mtk_auto_hibern8_update(hba, false);

		ret = ufs_mtk_wait_link_state(hba, VS_LINK_UP, 100);
		if (ret)
			ufshcd_link_recovery(hba);
	}
}

/**
 * struct ufs_hba_mtk_vops - UFS MTK specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static struct ufs_hba_variant_ops ufs_hba_mtk_vops = {
	.name                = "mediatek.ufshci",
	.init                = ufs_mtk_init,
	.setup_clocks        = ufs_mtk_setup_clocks,
	.hce_enable_notify   = ufs_mtk_hce_enable_notify,
	.link_startup_notify = ufs_mtk_link_startup_notify,
	.pwr_change_notify   = ufs_mtk_pwr_change_notify,
	.setup_xfer_req      = ufs_mtk_setup_xfer_req,
	.compl_xfer_req      = ufs_mtk_compl_xfer_req,
	.setup_task_mgmt     = ufs_mtk_setup_task_mgmt,
	.compl_task_mgmt     = ufs_mtk_compl_task_mgmt,
	.hibern8_notify      = ufs_mtk_hibern8_notify,
	.apply_dev_quirks    = ufs_mtk_apply_dev_quirks,
	.suspend             = ufs_mtk_suspend,
	.resume              = ufs_mtk_resume,
	.dbg_register_dump   = ufs_mtk_dbg_register_dump,
	.device_reset        = ufs_mtk_device_reset,
	.abort_handler       = ufs_mtk_abort_handler,
};

/**
 * ufs_mtk_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_mtk_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;

	ufs_mtk_biolog_init();

	/* perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_mtk_vops);
	if (err)
		dev_info(dev, "probe failed %d\n", err);

	return err;
}

struct ufs_hba *ufs_mtk_get_hba(void)
{
	return ufs_mtk_hba;
}
EXPORT_SYMBOL_GPL(ufs_mtk_get_hba);

/**
 * ufs_mtk_remove - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always return 0
 */
static int ufs_mtk_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	pm_runtime_get_sync(&(pdev)->dev);
	ufshcd_remove(hba);
	ufs_mtk_biolog_exit();
	return 0;
}

static const struct dev_pm_ops ufs_mtk_pm_ops = {
	.suspend         = ufshcd_pltfrm_suspend,
	.resume          = ufshcd_pltfrm_resume,
	.runtime_suspend = ufshcd_pltfrm_runtime_suspend,
	.runtime_resume  = ufshcd_pltfrm_runtime_resume,
	.runtime_idle    = ufshcd_pltfrm_runtime_idle,
};

static struct platform_driver ufs_mtk_pltform = {
	.probe      = ufs_mtk_probe,
	.remove     = ufs_mtk_remove,
	.shutdown   = ufshcd_pltfrm_shutdown,
	.driver = {
		.name   = "ufshcd-mtk",
		.pm     = &ufs_mtk_pm_ops,
		.of_match_table = ufs_mtk_of_match,
	},
};

MODULE_AUTHOR("Stanley Chu <stanley.chu@mediatek.com>");
MODULE_AUTHOR("Peter Wang <peter.wang@mediatek.com>");
MODULE_DESCRIPTION("MediaTek UFS Host Driver");
MODULE_LICENSE("GPL v2");

module_platform_driver(ufs_mtk_pltform);
