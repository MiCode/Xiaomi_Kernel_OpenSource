// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2022 Spreadtrum, Inc.
// Author: Wei Zheng <wei.zheng@unisoc.com>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/cdev.h>
#include <linux/ktime.h>
#include <linux/reboot.h>
#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/scatterlist.h>
#include <linux/swap.h>
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include <linux/mmc/ioctl.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio_func.h>

#include "../core/core.h"
#include "../core/mmc_ops.h"
#include "../core/card.h"
#include "../core/queue.h"
#include "mmc_swcq.h"
#include "sdhci-sprd-powp.h"

struct mmc_wp_ioc_data {
	struct mmc_ioc_cmd ic;
	unsigned char *buf;
	u64 buf_bytes;
	void *reserved;
};

struct mmc_wp_data {
	bool inited;
	struct device dev;
	struct cdev chrdev;
	dev_t devt;
	struct mmc_host *mmc;
	unsigned int enabled;
	unsigned int grp_cnt;
	unsigned int grp_size;
	unsigned long long start_addr;
	unsigned long long prot_size;
};

#define MMC_SEND_WR_PROT_TYPE			31
#define EXT_CSD_CLASS_6_CTRL			59
#define EXT_CSD_USR_WP			171
#define EXT_CSD_CMD_SET_NORMAL			(1<<0)
#define EXT_CSD_USR_WP_EN_PERM_WP			(1<<2)
#define EXT_CSD_USR_WP_EN_PWR_WP			(1)

#define MAX_DEVICES 256
#define SPRD_MAGIC_KEY 0x53505244

static struct mmc_wp_data wp_dat[1];
static struct bus_type wp_bus_type;

static int mmc_enable_usr_wp(struct mmc_card *card, unsigned char wp)
{
	struct mmc_host *host = card->host;
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {NULL};

	mrq.cmd = &cmd;
	/* set class6 ctrl */
	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_CLEAR_BITS << 24) | (EXT_CSD_CLASS_6_CTRL << 16) |
			EXT_CSD_CMD_SET_NORMAL;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;
	mmc_wait_for_req(card->host, &mrq);
	if (cmd.error) {
		pr_err("%s:(WP) set class6 ctrl, cmd error %d\n", mmc_hostname(host), cmd.error);
		return cmd.error;
	}

	/* set erase group def */
	cmd.arg = (MMC_SWITCH_MODE_CLEAR_BITS << 24) | (EXT_CSD_ERASE_GROUP_DEF << 16) |
			(1 << 8) | EXT_CSD_CMD_SET_NORMAL;
	mmc_wait_for_req(card->host, &mrq);
	if (cmd.error) {
		pr_err("%s:(WP) set erase group def fail, cmd error %d\n",
				mmc_hostname(host), cmd.error);
		return cmd.error;
	}

	/* set usr_wp */
	cmd.arg = (MMC_SWITCH_MODE_SET_BITS << 24) |
		(EXT_CSD_USR_WP << 16) | (wp << 8) | EXT_CSD_CMD_SET_NORMAL;
	mmc_wait_for_req(card->host, &mrq);
	if (cmd.error) {
		pr_err("%s:(WP) set usr_wp fail, cmd error %d\n", mmc_hostname(host), cmd.error);
		return cmd.error;
	}

	return 0;
}

static int mmc_set_usr_wp(struct mmc_card *card, unsigned char wp_type)
{
	struct mmc_host *host = card->host;
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {NULL};
	unsigned int i;
	int err;

	err = mmc_enable_usr_wp(card, wp_type);
	if (err)
		return err;

	mrq.cmd = &cmd;
	cmd.opcode = MMC_SET_WRITE_PROT;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	for (i = 0; i < wp_dat[host->index].grp_cnt; i++) {
		cmd.arg = wp_dat[host->index].start_addr + i * wp_dat[host->index].grp_size;
		mmc_wait_for_req(card->host, &mrq);
		if (cmd.error) {
			pr_err("%s:(WP) set fail, cmd error %d\n", mmc_hostname(host), cmd.error);
			return cmd.error;
		}
	}

	return 0;
}

int mmc_check_wp_fn(struct mmc_host *host)
{
	struct device *dev = host->parent;
	struct device_node *np = dev->of_node;
	int ret;
	const char *name;

	ret = of_property_read_string(np, "sprd,wp_fn", &name);
	if (ret)
		return ret;

	if (strcmp(name, "enable") == 0)
		return 0;

	return -1;
}

static int mmc_get_wp_current_status(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {NULL};
	struct mmc_data data = {0};
	struct scatterlist sg;
	unsigned long long size;
	unsigned long long wp_bits_sts = 0;
	unsigned int i;
	int err = 0;
	u8 *status_buf;

	mrq.cmd = &cmd;
	mrq.data = &data;
	size = 4;
	status_buf = kzalloc(size, GFP_KERNEL);
	if (!status_buf) {
		pr_err("%s:(WP) status_buf create false\n", mmc_hostname(host));
		return -ENOMEM;
	}

	/*send CMD30 to get write protect status*/
	cmd.opcode = MMC_SEND_WRITE_PROT;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
	cmd.arg = wp_dat[host->index].start_addr;
	data.blksz = size;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.blk_addr = 0;

	data.sg = &sg;
	data.sg_len = 1;
	sg_init_one(&sg, status_buf, size);

	mmc_wait_for_req(host, &mrq);
	if (cmd.error) {
		pr_err("%s:(WP) status cmd error %d\n", mmc_hostname(host), cmd.error);
		err = cmd.error;
		goto out;
	}
	if (data.error) {
		pr_err("%s:(WP) status data error %d\n", mmc_hostname(host), data.error);
		err = data.error;
		goto out;
	}
	for (i = 0; i < 4; i++)
		wp_bits_sts = (wp_bits_sts << 8) + (u8)status_buf[i];
	pr_info("%s:(WP) the status of write protection bits: 0x%llx\n",
			mmc_hostname(host), wp_bits_sts);
out:
	kfree(status_buf);
	return err;
}

static int mmc_get_wp_current_type(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {NULL};
	struct mmc_data data = {0};
	struct scatterlist sg;
	unsigned long long size;
	unsigned long long actual_type = 0;
	unsigned long long expected_type = 0;
	unsigned int i;
	int err = 0;
	u8 *type_buf;

	mrq.cmd = &cmd;
	mrq.data = &data;
	size = 8;
	type_buf = kzalloc(size, GFP_KERNEL);
	if (!type_buf) {
		pr_err("%s:(WP) type_buf create false\n", mmc_hostname(host));
		return -ENOMEM;
	}

	/*send CMD31 to get write protect type*/
	cmd.opcode = MMC_SEND_WR_PROT_TYPE;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
	cmd.arg = wp_dat[host->index].start_addr;
	data.blksz = size;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.blk_addr = 0;

	data.sg = &sg;
	data.sg_len = 1;
	sg_init_one(&sg, type_buf, size);

	mmc_wait_for_req(card->host, &mrq);
	if (cmd.error) {
		pr_err("%s:(WP) status cmd error %d\n", mmc_hostname(host), cmd.error);
		err = cmd.error;
		goto out;
	}
	if (data.error) {
		pr_err("%s:(WP) status data error %d\n", mmc_hostname(host), data.error);
		err = cmd.error;
		goto out;
	}

	for (i = 0; i < 8; i++)
		actual_type = (actual_type << 8) + (u8)type_buf[i];
	pr_info("%s:(WP) the really type of write protection grout: 0x%llx\n",
			mmc_hostname(host), actual_type);

	for (i = 0; i < wp_dat[host->index].grp_cnt; i++) {
		if (wp_dat[host->index].enabled)
			expected_type = (expected_type << 2) + 0x2;
		else
			expected_type = (expected_type << 2) + 0;
	}
	pr_info("%s:(WP) the expected type of write protection group: 0x%llx\n",
			mmc_hostname(host), expected_type);

	if (actual_type != expected_type) {
		pr_err("%s:(WP) actual type not equal to expected type\n", mmc_hostname(host));
		err = -1;
	}

out:
	kfree(type_buf);
	return err;
}

static void mmc_get_wp_current_status_and_type(struct mmc_card *card)
{
	mmc_get_wp_current_status(card);
	mmc_get_wp_current_type(card);
}

static int mmc_get_wp_params(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	struct device *dev = host->parent;
	struct device_node *np = dev->of_node;
	int index = host->index;
	u32 data[2];
	unsigned long long temp64;
	int err = 0;

	err = of_property_read_u32_array(np, "sprd,part-address-size", data, 2);
	if (err) {
		pr_err("%s:(WP) no sprd,part-address-size setting in node\n", mmc_hostname(host));
		return err;
	}

	wp_dat[index].start_addr = (u64)data[0];
	wp_dat[index].prot_size = (u64)data[1];

	wp_dat[index].grp_size = card->ext_csd.hc_erase_size * card->ext_csd.raw_hc_erase_gap_size;

	temp64 = wp_dat[index].prot_size;
	if (do_div(temp64, wp_dat[index].grp_size)) {/* adapt armv7 */
		pr_err("%s:(WP) partition is not aligned\n", mmc_hostname(host));
		return -1;
	}
	wp_dat[index].grp_cnt = (unsigned int)temp64;

	pr_info("%s:(WP) start address: 0x%llx, size: 0x%llx, group cnt: 0x%x\n",
		mmc_hostname(host), wp_dat[index].start_addr, wp_dat[index].prot_size,
		wp_dat[index].grp_cnt);

	return 0;
}

int mmc_set_powp(struct mmc_card *card)
{
	int err;
	struct mmc_host *mmc = card->host;
	int index = mmc->index;

	if (index != 0) {
		pr_err("%s:(WP) mmc index err\n", mmc_hostname(mmc));
		return -1;
	}

	err = mmc_get_wp_params(card);
	if (err)
		pr_err("%s:(WP) get params err\n", mmc_hostname(mmc));

	if (wp_dat[index].enabled == 0) {
		mmc_get_wp_current_status_and_type(card);
		pr_info("%s:(WP) status is disabled\n", mmc_hostname(mmc));
		return 0;
	}

	err = mmc_set_usr_wp(card, EXT_CSD_USR_WP_EN_PWR_WP);
	if (err)
		return err;

	mmc_get_wp_current_status_and_type(card);
	pr_info("%s:(WP) status is enabled\n", mmc_hostname(mmc));

	return 0;
}

static struct mmc_wp_ioc_data *mmc_wp_ioctl_copy_from_user(struct mmc_ioc_cmd __user *user)
{
	struct mmc_wp_ioc_data *idata;
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

static int mmc_wp_ioctl_cmd(struct mmc_host *host, struct mmc_ioc_cmd __user *ic_ptr)
{
	struct mmc_wp_ioc_data *idata;
	struct mmc_card *card = host->card;
	struct mmc_blk_data *md;
	struct mmc_queue *mq;
	struct sdio_func sf;
	int err = 0;
	bool cmdq_dis = false;
	int busy = 1000;

	if (!card || mmc_card_removed(card))
		return -ENODEV;

	idata = mmc_wp_ioctl_copy_from_user(ic_ptr);
	if (IS_ERR(idata)) {
		pr_err("%s:(WP) copy from user failed\n", mmc_hostname(host));
		err = (int)PTR_ERR(idata);
		return err;
	}

	if (idata->ic.opcode != SPRD_MAGIC_KEY) {
		pr_err("%s:(WP) user opcode invalid\n", mmc_hostname(host));
		err = -EINVAL;
		goto cmd_exit;
	}

	if (idata->ic.arg == wp_dat[host->index].enabled) {
		if (wp_dat[host->index].enabled == 1)
			pr_info("%s:(WP) no need to reset, status is enabled\n",
					mmc_hostname(host));
		else if (wp_dat[host->index].enabled == 0)
			pr_info("%s:(WP) no need to reset, status is disabled\n",
					mmc_hostname(host));
		err = 0;
		goto cmd_exit;
	}

	md = dev_get_drvdata(&card->dev);
	mq = &md->queue;

	err = -EBUSY;
	while (busy--) {
		spin_lock_irq(&mq->lock);
		if (mq->recovery_needed || mq->busy) {
			spin_unlock_irq(&mq->lock);
			usleep_range(3000, 5000);
			continue;
		}

		mq->busy = true;
		spin_unlock_irq(&mq->lock);
		err = 0;
		break;
	}

	if (err) {
		pr_err("%s:(WP) mq busy\n", mmc_hostname(host));
		goto cmd_exit;
	}

	if (host->cqe_enabled || host->hsq_enabled) {
		err = host->cqe_ops->cqe_wait_for_idle(host);
		if (err) {
			pr_err("%s:(WP) wait for idle fail\n", mmc_hostname(host));
			goto cmd_exit;
		}
	}

	sf.card = card;
	sdio_claim_host(&sf);
	if (card->ext_csd.cmdq_en) {
		err = mmc_cmdq_disable(card);
		if (err) {
			pr_err("%s:(WP) cmdq disable fail, err=%d\n", mmc_hostname(host), err);
			goto cmd_done;
		}
		cmdq_dis = true;
	}

	/* reset emmc enable power-on write protect */
	wp_dat[host->index].enabled = idata->ic.arg;
	mmc_hw_reset(host);

cmd_done:
	if (cmdq_dis) {
		err = mmc_cmdq_enable(card);
		if (err) {
			pr_err("%s:(WP) cmdq enable fail, err=%d\n", mmc_hostname(host), err);
			mmc_hw_reset(host);
		}
	}

	spin_lock_irq(&mq->lock);
	mq->busy = false;
	spin_unlock_irq(&mq->lock);

	sdio_release_host(&sf);

cmd_exit:
	if (!IS_ERR(idata)) {
		kfree(idata->buf);
		kfree(idata);
	}

	return err;
}

static long mmc_wp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mmc_host *host = file->private_data;
	long ret;

	switch (cmd) {
	case MMC_IOC_CMD:
		ret = (long)mmc_wp_ioctl_cmd(host, (struct mmc_ioc_cmd __user *)arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long mmc_wp_ioctl_compat(struct file *file, unsigned int cmd, unsigned long arg)
{
	return mmc_wp_ioctl(file, cmd, arg);
}
#endif

static int mmc_wp_chrdev_open(struct inode *inode, struct file *filp)
{
	struct mmc_wp_data *wp = container_of(inode->i_cdev, struct mmc_wp_data, chrdev);

	get_device(&wp->dev);
	filp->private_data = wp->mmc;

	return nonseekable_open(inode, filp);
}

static int mmc_wp_chrdev_release(struct inode *inode, struct file *filp)
{
	struct mmc_wp_data *wp = container_of(inode->i_cdev, struct mmc_wp_data, chrdev);

	put_device(&wp->dev);
	return 0;
}

static const struct file_operations wp_fileops = {
	.release = mmc_wp_chrdev_release,
	.open = mmc_wp_chrdev_open,
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.unlocked_ioctl = mmc_wp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mmc_wp_ioctl_compat,
#endif
};

int mmc_wp_init(struct mmc_host *mmc)
{
	int index = mmc->index;
	char name[64];
	int res;
	int i;

	/* sanity check for mmc */
	if (index != 0 || wp_dat[index].inited)
		return 0;

	if (!wp_bus_type.name) {
		wp_bus_type.name = "mmc_wp";
		res  = bus_register(&wp_bus_type);
		if (res < 0) {
			pr_err("%s:(WP) could not register WP bus type\n", mmc_hostname(mmc));
			wp_bus_type.name = NULL;
			return res;
		}
	}

	snprintf(name, sizeof(name), "%s_wp", mmc_hostname(mmc));
	res = alloc_chrdev_region(&wp_dat[index].devt, 0, MAX_DEVICES, name);
	if (res < 0) {
		pr_err("%s:(WP) failed to allocate chrdev region\n", mmc_hostname(mmc));
		goto out_bus_unreg;
	}

	wp_dat[index].dev.init_name = name;
	wp_dat[index].dev.bus = &wp_bus_type;
	wp_dat[index].dev.devt = MKDEV(MAJOR(wp_dat[index].devt), 1);/* fixme */
	wp_dat[index].dev.parent = &mmc->class_dev;
	device_initialize(&wp_dat[index].dev);

	dev_set_drvdata(&wp_dat[index].dev, &wp_dat[index]);

	cdev_init(&wp_dat[index].chrdev, &wp_fileops);

	wp_dat[index].chrdev.owner = THIS_MODULE;
	res = cdev_device_add(&wp_dat[index].chrdev, &wp_dat[index].dev);
	if (res) {
		pr_err("%s:(WP) could not add character device\n", mmc_hostname(mmc));
		goto out_put_device;
	}
	wp_dat[index].mmc = mmc;
	pr_info("%s:(WP) create chardev(%d:1)\n", mmc_hostname(mmc), MAJOR(wp_dat[index].devt));
	wp_dat[index].inited = true;
	wp_dat[index].enabled = 1;

	return 0;

out_put_device:
	put_device(&wp_dat[index].dev);
	unregister_chrdev_region(wp_dat[index].devt, MAX_DEVICES);
out_bus_unreg:
	for (i = 0; i < ARRAY_SIZE(wp_dat); i++) {
		if (wp_dat[i].inited)
			break;
	}

	if (i == ARRAY_SIZE(wp_dat)) {
		bus_unregister(&wp_bus_type);
		wp_bus_type.name = NULL;
	}

	return res;
}

void mmc_wp_remove(struct mmc_host *mmc)
{
	int index = mmc->index;
	int i;

	if (index != 0 || !wp_dat[index].inited)
		return;

	put_device(&wp_dat[index].dev);
	unregister_chrdev_region(wp_dat[index].devt, MAX_DEVICES);

	for (i = 0; i < ARRAY_SIZE(wp_dat); i++) {
		if (wp_dat[i].inited)
			break;
	}

	if (i == ARRAY_SIZE(wp_dat)) {
		bus_unregister(&wp_bus_type);
		wp_bus_type.name = NULL;
	}

	wp_dat[index].inited = false;
	wp_dat[index].enabled = 0;
}

