// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2022 Spreadtrum, Inc.
// Author: Zhenxiong Lai <zhenxiong.lai@unisoc.com>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/cdev.h>
#include <linux/ktime.h>
#include <linux/reboot.h>
#include <linux/mmc/ioctl.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/uaccess.h>
#include <trace/hooks/mmc.h>

#include "../core/core.h"
#include "../core/mmc_ops.h"
#include "../core/card.h"
#include "../core/queue.h"
#include "sdhci.h"
#include "mmc_swcq.h"
#include <linux/mmc/sdio_func.h>

#define MAX_DEVICES 256

#define CID_MANFID_SANDISK				0x2
#define CID_MANFID_TOSHIBA				0x11
#define CID_MANFID_MICRON				0x13
#define CID_MANFID_SAMSUNG				0x15
#define CID_MANFID_SANDISK_NEW			0x45
#define CID_MANFID_KSI					0x70
#define CID_MANFID_HYNIX				0x90

/* ext csd field */
#define EXT_CSD_FFU_STATUS				26
#define EXT_CSD_MODE_OPERATION_CODES	29
#define EXT_CSD_MODE_CONFIG				30
#define EXT_CSD_NUM_OF_FW_SEC_PROG		302
#define EXT_CSD_FFU_ARG					487
#define EXT_CSD_OPERATION_CODE_TIMEOUT	491
#define EXT_CSD_FFU_FEATURES			492

/*
 * eMMC5.0 Field Firmware Update (FFU) opcodes
 */
#define MMC_FFU_DOWNLOAD_OP				302
#define MMC_FFU_INSTALL_OP				303
#define MMC_FFU_MODE_SET				0x1
#define MMC_FFU_MODE_NORMAL				0x0
#define MMC_FFU_INSTALL_SET				0x1
#define MMC_FFU_ENABLE					0x0
#define MMC_FFU_FW_CONFIG				0x1
#define MMC_FFU_SUPPORTED_MODES			0x1
#define MMC_FFU_FEATURES				0x1

struct mmc_ffu_ioc_data {
	struct mmc_ioc_cmd ic;
	unsigned char *buf;
	u64 buf_bytes;
	void *reserved;
};

struct mmc_ffu_data {
	bool inited;
	struct device dev;
	struct cdev chrdev;
	dev_t devt;

	struct mmc_host *mmc;
};
static struct mmc_ffu_data ffu_dat[1];
static struct bus_type ffu_bus_type;

static void mmc_ffu_device_release(struct device *dev)
{
}

static int mmc_ffu_chrdev_open(struct inode *inode, struct file *filp)
{
	struct mmc_ffu_data *ffu = container_of(inode->i_cdev, struct mmc_ffu_data,
							chrdev);

	get_device(&ffu->dev);
	filp->private_data = ffu->mmc;

	return nonseekable_open(inode, filp);
}

static struct mmc_ffu_ioc_data *mmc_ffu_ioctl_copy_from_user(
	struct mmc_ioc_cmd __user *user)
{
	struct mmc_ffu_ioc_data *idata;
	int err;

	idata = kmalloc(sizeof(*idata), GFP_KERNEL);
	if (!idata) {
		err = -ENOMEM;
		goto out;
	}

	if (copy_from_user(&idata->ic, user, sizeof(idata->ic))) {
		err = -EFAULT;
		goto idata_err;
	}

	idata->buf_bytes = (u64)idata->ic.blksz * idata->ic.blocks;
	if (!idata->buf_bytes) {
		err = -EINVAL;
		goto idata_err;
	}

	idata->buf = memdup_user((void __user *)(unsigned long)
				 idata->ic.data_ptr, idata->buf_bytes);
	if (IS_ERR(idata->buf)) {
		err = PTR_ERR(idata->buf);
		goto idata_err;
	}

	return idata;

idata_err:
	kfree(idata);
out:
	return ERR_PTR(err);
}

static void mmc_ffu_vh_update_cid_prv(void *data,
	struct mmc_host *host, struct mmc_card *card, u32 *cid)
{
	u8 raw_prv, prv;

	raw_prv = (card->raw_cid[2] >> 16) & 0xff;
	prv = (cid[2] >> 16) & 0xff;
	if (unlikely((card->csd.mmca_vsn >= 2) && (raw_prv != prv))) {
		pr_info("%s:(FFU) raw prv %x, prv %x\n", mmc_hostname(card->host),
			raw_prv, prv);
		card->raw_cid[2] &= 0xff00ffff;
		card->raw_cid[2] |= prv << 16;

		card->cid.prv = prv;
	}
}

static void mmc_ffu_hw_reset(struct mmc_card *card)
{
	static int reg;

	if (!reg) {
		reg = 1;
		register_trace_android_vh_mmc_ffu_update_cid(mmc_ffu_vh_update_cid_prv,
			NULL);
	}

	pr_info("%s:(FFU) hw reset\n", mmc_hostname(card->host));
	mmc_hw_reset(card->host);
}

static int mmc_ffu_cache_off(struct mmc_card *card)
{
	int err = 0;

	if (card->ext_csd.cache_size > 0 && card->ext_csd.cache_ctrl) {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_CACHE_CTRL, 0, 0);
		if (err)
			pr_err("%s: cache off error %d\n", mmc_hostname(card->host), err);
		else
			card->ext_csd.cache_ctrl = 0;
	}

	return err;
}

static int mmc_ffu_wait_busy(struct mmc_card *card,
	unsigned int timeout_ms)
{
	struct mmc_command cmd = {0};
	int err;
	unsigned long timeout;
	unsigned int udelay = 32, udelay_max = 32768;
	bool expired = false;
	bool busy = false;

	memset(&cmd, 0, sizeof(struct mmc_command));
	cmd.opcode = MMC_SEND_STATUS;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	timeout = jiffies + msecs_to_jiffies(timeout_ms) + 1;
	do {
		expired = time_after(jiffies, timeout);

		err = mmc_wait_for_cmd(card->host, &cmd, 0);
		if (err)
			return err;

		/* Timeout if the device still remains busy. */
		busy = !(cmd.resp[0] & R1_READY_FOR_DATA) ||
				(R1_CURRENT_STATE(cmd.resp[0]) == R1_STATE_PRG);
		if (expired && busy) {
			pr_err("%s: Card stuck being busy! %s\n",
				mmc_hostname(card->host), __func__);
			return -ETIMEDOUT;
		}

		/* Throttle the polling rate to avoid hogging the CPU. */
		if (busy) {
			usleep_range(udelay, udelay * 2);
			if (udelay < udelay_max)
				udelay *= 2;
		}
	} while (busy);

	return 0;
}

static int mmc_ffu_download(struct mmc_card *card,
	struct mmc_ffu_ioc_data *idata,
	u8 *ext_csd)
{
	const char *name = mmc_hostname(card->host);
	struct mmc_command cmd = {}, stop = {};
	struct mmc_data data = {};
	struct mmc_request mrq = {};
	struct scatterlist sg;
	int err, err2;

	cmd.arg = ext_csd[EXT_CSD_FFU_ARG] | ext_csd[EXT_CSD_FFU_ARG + 1] << 8
				| ext_csd[EXT_CSD_FFU_ARG + 2] << 16
				| ext_csd[EXT_CSD_FFU_ARG + 3] << 24;
	cmd.flags = MMC_CMD_ADTC | MMC_RSP_R1;

	if (idata->buf_bytes) {
		data.sg = &sg;
		data.sg_len = 1;
		data.blksz = idata->ic.blksz;
		data.blocks = idata->ic.blocks;
		data.flags = MMC_DATA_WRITE;

		sg_init_one(data.sg, idata->buf, idata->buf_bytes);
		mmc_set_data_timeout(&data, card);
		if (idata->ic.data_timeout_ns)
			data.timeout_ns = idata->ic.data_timeout_ns;
		if ((cmd.flags & MMC_RSP_R1B) == MMC_RSP_R1B)
			data.timeout_ns = idata->ic.cmd_timeout_ms * 1000000;
		mrq.data = &data;
	}

	mrq.cmd = &cmd;
	mrq.cmd->opcode = MMC_WRITE_MULTIPLE_BLOCK;
	mrq.stop = &stop;
	mrq.stop->opcode = MMC_STOP_TRANSMISSION;
	mrq.stop->arg = 0;
	mrq.stop->flags = MMC_RSP_R1B | MMC_CMD_AC;

	/* set MODE_CONFIG[30] to "FFU Mode" */
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_CONFIG,
			MMC_FFU_MODE_SET, card->ext_csd.generic_cmd6_time);
	if (err) {
		pr_err("%s:(FFU) switch to FFU mode failed, err%d\n", name, err);
		goto exit_err;
	}

	pr_info("eMMC cache originally %s -> %s\n",
		((card->ext_csd.cache_ctrl) ? "on" : "off"),
		((card->ext_csd.cache_ctrl) ? "turn off" : "keep"));
	if (card->ext_csd.cache_ctrl) {
		(void)mmc_flush_cache(card->host);
		(void)mmc_ffu_cache_off(card);
	}

	mmc_wait_for_req(card->host, &mrq);
	err = mmc_ffu_wait_busy(card, 20 * 1000);

	if (err)
		err = -EBUSY;
	else if (cmd.error)
		err = cmd.error;
	else if (mrq.data->error)
		err = mrq.data->error;
	else if (stop.error)
		err = stop.error;
	else if (mrq.data->bytes_xfered != mrq.data->blocks * mrq.data->blksz)
		err = -EIO;
	else
		err = 0;

	if (err) {
		err2 = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_CONFIG,
				MMC_FFU_MODE_NORMAL, card->ext_csd.generic_cmd6_time);
		if (err2) {
			pr_err("%s:(FFU) switch to normal mode fail, err%d\n", name, err2);
			mmc_ffu_hw_reset(card);
		}
	}

exit_err:
	return err;
}

static int mmc_ffu_install(struct mmc_card *card, u8 *ext_csd)
{
	const char *name = mmc_hostname(card->host);
	u8 *ext_csd_new = NULL;
	u32 timeout;
	u8 set = 1;
	u8 retry = 10;
	u32 fw_sectors;
	int err, err2;

	if (ext_csd[EXT_CSD_FFU_FEATURES] & MMC_FFU_FEATURES) {
		fw_sectors = ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG]
						| ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 1] << 8
						| ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 2] << 16
						| ext_csd[EXT_CSD_NUM_OF_FW_SEC_PROG + 3] << 24;
		if (!fw_sectors) {
			pr_err("%s:(FFU) firmware were not download into the device", name);
			err = -EIO;
			goto restore_normal;
		}

		timeout = ext_csd[EXT_CSD_OPERATION_CODE_TIMEOUT];
		if (timeout == 0 || timeout > 0x17) {
			timeout = 0x17;
			pr_info("%s:(FFU) use default OPERATION_CODES_TIMEOUT value", name);
		}
		timeout = (100 * (1 << timeout)) / 1000 + 1;

		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_MODE_OPERATION_CODES, MMC_FFU_INSTALL_SET, timeout);
		if (err) {
			pr_err("%s:(FFU) install failed", name);
			goto restore_normal;
		}
	} else {
		if ((card->cid.manfid == CID_MANFID_HYNIX) && (card->cid.prv == 0x03))
			set = 0;

		pr_info("%s:(FFU) Device not support MODE_OPERATION_CODES\n", name);
		err = mmc_switch(card, set, EXT_CSD_MODE_CONFIG,
				MMC_FFU_MODE_NORMAL, card->ext_csd.generic_cmd6_time);
		if (err) {
			pr_err("%s:(FFU) error %d:exit FFU mode\n", name, err);
			goto restore_normal;
		}

		mmc_ffu_hw_reset(card);
	}

	while (retry--) {
		err = mmc_get_ext_csd(card, &ext_csd_new);
		if (!err)
			break;
		pr_info("%s:(FFU) read ext_csd_new failed %d times", name, retry);
	}

	if (err) {
		pr_err("%s:(FFU) read ext_csd_new failed 10 times, reboot!!!", name);
		goto reboot_sys;
	}

	if (!ext_csd_new[EXT_CSD_FFU_STATUS]) {
		pr_info("%s:(FFU) install success!!", name);
		if (memcmp(card->ext_csd.fwrev,
					&ext_csd_new[EXT_CSD_FIRMWARE_VERSION], MMC_FIRMWARE_LEN)) {
			memcpy(card->ext_csd.fwrev, &ext_csd_new[EXT_CSD_FIRMWARE_VERSION],
				   MMC_FIRMWARE_LEN);
			pr_info("%s:(FFU) update ext_csd fwrev", name);
		}
		goto exit;
	} else {
		pr_err("%s:(FFU) install failed!!,%x", name,
			ext_csd_new[EXT_CSD_FFU_STATUS]);
		err = -EIO;
	}
	goto exit;

reboot_sys:
	emergency_restart();
	goto exit;

restore_normal:
	err2 = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_MODE_CONFIG,
			MMC_FFU_MODE_NORMAL, card->ext_csd.generic_cmd6_time);
	if (err2) {
		pr_err("%s:(FFU) switch to normal mode failed, err%d\n", name, err2);
		mmc_ffu_hw_reset(card);
	}

exit:
	kfree(ext_csd_new);
	return err;
}

static int mmc_ffu_ioctl_cmd(struct mmc_host *host,
	struct mmc_ioc_cmd __user *ic_ptr)
{
	const char *name = mmc_hostname(host);
	struct mmc_ffu_ioc_data *idata;
	struct mmc_card *card = host->card;
	struct mmc_blk_data *md;
	struct mmc_queue *mq;
	struct sdio_func sf;
	int err = 0, ioc_err = 0;
	u8 *ext_csd = NULL;
	bool cmdq_dis = false;
	int busy = 1000;

	if (!card || mmc_card_removed(card))
		return -ENODEV;

	if ((card->ext_csd.rev < 7) || !card->ext_csd.ffu_capable) {
		pr_err("%s:(FFU) ffu operation not permitted, ver%d\n", name,
			card->ext_csd.rev);
		return -EPERM;
	}

	idata = mmc_ffu_ioctl_copy_from_user(ic_ptr);
	if (IS_ERR(idata) || idata->ic.opcode != MMC_FFU_DOWNLOAD_OP) {
		pr_err("%s:(FFU) copy from user failed\n", name);
		err = (int)PTR_ERR(idata);
		return err;
	}

	md = dev_get_drvdata(&card->dev);
	mq = &md->queue;

	err = -EBUSY;
	while (busy--) {
		spin_lock_irq(&mq->lock);
		if (mq->recovery_needed || mq->busy) {
			spin_unlock_irq(&mq->lock);
			usleep_range_state(3000, 5000, TASK_UNINTERRUPTIBLE);
			continue;
		}

		mq->busy = true;
		spin_unlock_irq(&mq->lock);
		err = 0;
		break;
	}

	if (err) {
		pr_err("%s:(FFU) mq busy\n", name);
		return err;
	}

	if (host->cqe_enabled || host->hsq_enabled) {
		err = host->cqe_ops->cqe_wait_for_idle(host);
		if (err) {
			pr_err("%s:(FFU) wait for idle fail\n", name);
			return err;
		}
	}

	sf.card = card;
	sdio_claim_host(&sf);
	if (card->ext_csd.cmdq_en) {
		err = mmc_cmdq_disable(card);
		if (err) {
			pr_err("%s:(FFU) cmdq disable fail,err=%d\n", name, err);
			goto cmd_done;
		}
		cmdq_dis = true;
	}

	err = mmc_get_ext_csd(card, &ext_csd);
	if (err) {
		pr_err("%s:(FFU) read ext_csd failed,err=%d\n", name, err);
		goto cmd_done;
	}

	err = mmc_ffu_download(card, idata, ext_csd);
	if (err) {
		pr_err("%s:(FFU) download FW failed, err%d\n", name, err);
		goto cmd_done;
	}

	err = mmc_ffu_install(card, ext_csd);
	if (err)
		pr_err("%s:(FFU) install FW failed, err%d\n", name, err);

cmd_done:
	if (cmdq_dis) {
		err = mmc_cmdq_enable(card);
		if (err) {
			pr_err("%s:(FFU) cmdq enable fail,err=%d\n", name, err);
			mmc_ffu_hw_reset(card);
		}
	}

	spin_lock_irq(&mq->lock);
	mq->busy = false;
	spin_unlock_irq(&mq->lock);

	sdio_release_host(&sf);

	if (!IS_ERR(idata)) {
		kfree(idata->buf);
		kfree(idata);
	}

	kfree(ext_csd);
	return ioc_err ? ioc_err : err;
}

static long mmc_ffu_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct mmc_host *host = file->private_data;
	int ret;

	switch (cmd) {
	case MMC_IOC_CMD:
		ret = mmc_ffu_ioctl_cmd(host, (struct mmc_ioc_cmd __user *)arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int mmc_ffu_chrdev_release(struct inode *inode, struct file *filp)
{
	struct mmc_ffu_data *ffu = container_of(inode->i_cdev, struct mmc_ffu_data,
							chrdev);

	put_device(&ffu->dev);
	return 0;
}

static const struct file_operations ffu_fileops = {
	.release = mmc_ffu_chrdev_release,
	.open = mmc_ffu_chrdev_open,
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.unlocked_ioctl = mmc_ffu_ioctl,
};

int mmc_ffu_init(struct sdhci_host *host)
{
	struct mmc_host *mmc = host->mmc;
	int index = mmc->index;
	char name[64];
	int res;
	int i;

	//sanity check for mmc
	if (index != 0 || ffu_dat[index].inited)
		return 0;

	if (!ffu_bus_type.name) {
		ffu_bus_type.name = "mmc_ffu";
		res  = bus_register(&ffu_bus_type);
		if (res < 0) {
			pr_err("%s:(FFU) could not register ffu bus type\n", ffu_bus_type.name);
			ffu_bus_type.name = NULL;
			return res;
		}
	}

	snprintf(name, sizeof(name), "%s_ffu", mmc_hostname(mmc));
	res = alloc_chrdev_region(&ffu_dat[index].devt, 0, MAX_DEVICES, name);
	if (res < 0) {
		pr_err("%s:(FFU) failed to allocate chrdev region\n", name);
		goto out_bus_unreg;
	}

	ffu_dat[index].dev.init_name = name;
	ffu_dat[index].dev.bus = &ffu_bus_type;
	ffu_dat[index].dev.devt = MKDEV(MAJOR(ffu_dat[index].devt), 1);/* fixme */
	ffu_dat[index].dev.parent = &mmc->class_dev;
	ffu_dat[index].dev.release = mmc_ffu_device_release;
	device_initialize(&ffu_dat[index].dev);
	dev_set_drvdata(&ffu_dat[index].dev, &ffu_dat[index]);

	cdev_init(&ffu_dat[index].chrdev, &ffu_fileops);
	ffu_dat[index].chrdev.owner = THIS_MODULE;
	res = cdev_device_add(&ffu_dat[index].chrdev, &ffu_dat[index].dev);
	if (res) {
		pr_err("%s:(FFU) could not add character device\n", name);
		goto out_put_device;
	}
	ffu_dat[index].mmc = mmc;
	pr_info("%s:(FFU) create chardev(%d:1)\n", name, MAJOR(ffu_dat[index].devt));
	ffu_dat[index].inited = true;
	return 0;

out_put_device:
	put_device(&ffu_dat[index].dev);
	unregister_chrdev_region(ffu_dat[index].devt, MAX_DEVICES);
out_bus_unreg:
	for (i = 0; i < ARRAY_SIZE(ffu_dat); i++) {
		if (ffu_dat[i].inited)
			break;
	}

	if (i == ARRAY_SIZE(ffu_dat)) {
		bus_unregister(&ffu_bus_type);
		ffu_bus_type.name = NULL;
	}
	return res;
}
EXPORT_SYMBOL(mmc_ffu_init);

void mmc_ffu_remove(struct sdhci_host *host)
{
	struct mmc_host *mmc = host->mmc;
	int index = mmc->index;
	int i;

	if (index != 0 || !ffu_dat[index].inited)
		return;

	put_device(&ffu_dat[index].dev);
	unregister_chrdev_region(ffu_dat[index].devt, MAX_DEVICES);

	for (i = 0; i < ARRAY_SIZE(ffu_dat); i++) {
		if (ffu_dat[i].inited)
			break;
	}

	if (i == ARRAY_SIZE(ffu_dat)) {
		bus_unregister(&ffu_bus_type);
		ffu_bus_type.name = NULL;
	}

	ffu_dat[index].inited = false;
}
EXPORT_SYMBOL(mmc_ffu_remove);

