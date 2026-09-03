// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2023 Spreadtrum, Inc.
// Author: Hangming Zheng <hangming.zheng@unisoc.com>
#include <linux/module.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/proc_ns.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/mmc/sdio_func.h>
#include <linux/ktime.h>
#include <linux/mmc/sdio.h>
#include <linux/kernel.h>
#include "../core/queue.h"
#include "../core/core.h"
#include "../core/mmc_ops.h"
#include "../core/card.h"
#include "mmc_swcq.h"
#include "sdhci.h"
#include "sdhci-sprd-health.h"

struct health_field {
	const char *name;
	u16 size;
	u16 offset;
	u32 data;
	bool parsing_type;
};

struct mmc_health_data {
	u8 health_data[TOTAL_HEALTH_BYTE];
	struct mmc_card *card;
	struct health_field filed[HEALTH_FILED_NUM];
};

static const u16 health_offset[] = {
	0,	2,	16,	20,	24,	28,	32,	36,	40,	44,
	80,	100,	118,	192,	256,	258,	272,	274,	392,
};

static const u16 health_size[] = {
	2,	2,	4,	4,	4,	4,	4,	4,	4,	4,
	4,	4,	2,	4,	2,	2,	2,	2,	4,
};

static const bool health_parsing_type[] = {
	true,	true,	true,	true,	true,	true,	true,	true,	true,	true,
	false,	false,	false,	false,	false,	false,	true,	true,	false,
};

static const char *const health_name[] = {
	"factory_bad_block_count",
	"run_time_bad_block_count",
	"min_ec_num_tlc",
	"max_ec_num_tlc",
	"ave_ec_num_tlc",
	"total_ec_num_tlc",
	"min_ec_num_slc",
	"max_ec_num_slc",
	"ave_ec_num_slc",
	"total_ec_num_slc",
	"total_initialization_count",
	"total_write_size",
	"ffu_successful_count",
	"power_loss_count",
	"total_ce_count",
	"plane_count_per_ce",
	"plane0_total_bad_block_count",
	"plane1_total_bad_block_count",
	"total_read_size",
};

static struct mmc_health_data *mmc_health;

static void mmc_health_data_update(struct health_field *filed, u8 *health_data)
{
	int i;
	/* use the corresponding method to analyze health data */
	filed->data = 0;
	for (i = 0; i < filed->size; i++) {
		if (filed->parsing_type == LITTLE_ENDIAN)
			filed->data |= health_data[filed->offset + i] << (i * 8);
		else
			filed->data |= health_data[filed->offset + filed->size - i - 1] << (i * 8);
	}
}

static void mmc_health_update(struct mmc_health_data *mmc_health, u8 *health_data)
{
	struct health_field *filed;
	int i;

	filed = &mmc_health->filed[0];
	memcpy(&mmc_health->health_data[0], health_data, TOTAL_HEALTH_BYTE);
	for (i = 0; i < HEALTH_FILED_NUM; i++)
		mmc_health_data_update(&filed[i], health_data);
}

static int mmc_health_init(struct mmc_card *card)
{
	struct device *dev = card->host->parent;
	int i;

	mmc_health = devm_kzalloc(dev, sizeof(*mmc_health), GFP_KERNEL);
	if (!mmc_health)
		return -ENOMEM;

	mmc_health->card = card;
	for (i = 0; i < HEALTH_FILED_NUM; i++) {
		mmc_health->filed[i].size = health_size[i];
		mmc_health->filed[i].name = health_name[i];
		mmc_health->filed[i].offset = health_offset[i];
		mmc_health->filed[i].parsing_type = health_parsing_type[i];
	}

	return 0;
}

static int mmc_send_health_cmd(struct mmc_card *card,
		u32 opcode, void *buf, unsigned int len, u32 arg)
{
	struct mmc_request mrq = {};
	struct mmc_command cmd = {};
	struct mmc_data data = {};
	struct scatterlist sg;

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = opcode;
	cmd.arg = arg;

	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = len;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, buf, len);

	mmc_set_data_timeout(&data, card);

	mmc_wait_for_req(card->host, &mrq);

	if (cmd.error) {
		pr_err("%s: health cmd%d, cmd error: %d\n",
			mmc_hostname(card->host), opcode, cmd.error);
		return cmd.error;
	}

	if (data.error) {
		pr_err("%s: health cmd%d, data error: %d\n",
			mmc_hostname(card->host), opcode, data.error);
		return data.error;
	}

	return 0;
}

/* method is only for model "HFCS 32G eMMC" */
static int mmc_get_health_data(struct mmc_card *card)
{
	int err;
	u8 *health_data = kzalloc(TOTAL_HEALTH_BYTE, GFP_KERNEL);

	if (!health_data)
		return -ENOMEM;

	/* send health cmd to get the data */
	err = mmc_send_health_cmd(card, MMC_GEN_CMD,
				health_data, TOTAL_HEALTH_BYTE, HEALTH_CMD_ARG1);
	if (err)
		goto out;

	err = mmc_send_status(card, NULL);
	if (err)
		goto out;

	err = mmc_send_health_cmd(card, MMC_GEN_CMD,
				health_data, TOTAL_HEALTH_BYTE, HEALTH_CMD_ARG2);
	if (err)
		goto out;

	err = mmc_send_status(card, NULL);
	if (err)
		goto out;

	mmc_health_update(mmc_health, health_data);
out:
	kfree(health_data);
	health_data = NULL;
	return err;
}

static int mmc_health_handle(struct mmc_card *card)
{
	struct mmc_blk_data *md;
	struct mmc_queue *mq;
	struct sdio_func sf;
	bool cmdq_dis = false;
	int busy = 1000;
	int err = -EBUSY;

	if (!card)
		return -EINVAL;

	/* wait mq idle*/
	md = dev_get_drvdata(&card->dev);
	mq = &md->queue;
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
		pr_err("%s:mmc mq busy\n", __func__);
		return err;
	}

	sf.card = card;
	sdio_claim_host(&sf);
	if (card->ext_csd.cmdq_en) {
		err = mmc_cmdq_disable(card);
		if (err) {
			pr_err("%s: cmdq disable fail,err=%d\n", __func__, err);
			goto cmd_done;
		}
		cmdq_dis = true;
	}

	/* send health cmd to get the data */
	err = mmc_get_health_data(card);

	if (cmdq_dis) {
		err = mmc_cmdq_enable(card);
		if (err) {
			pr_err("%s: cmdq enable fail,err=%d\n", __func__, err);
			mmc_hw_reset(card->host);
		}
	}
cmd_done:
	spin_lock_irq(&mq->lock);
	mq->busy = false;
	spin_unlock_irq(&mq->lock);
	sdio_release_host(&sf);

	return err;
}

static int sprd_health_data_show(struct seq_file *m, void *v)
{
	int i;

	if (!mmc_health)
		return -EINVAL;

	mmc_health_handle(mmc_health->card);

	seq_puts(m, "The data applies only to device <HFCS 32G eMMC>\n");
	for (i = 0; i < HEALTH_FILED_NUM; i++)
		seq_printf(m, "%s: 0x%x\n", mmc_health->filed[i].name, mmc_health->filed[i].data);

	return 0;
}

static int sprd_health_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_health_data_show, inode->i_private);
}

static const struct proc_ops health_data_fops = {
	.proc_open = sprd_health_data_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int sprd_original_health_data_show(struct seq_file *m, void *v)
{
	int i;

	if (!mmc_health)
		return -EINVAL;

	mmc_health_handle(mmc_health->card);

	seq_puts(m, "The data applies only to device <HFCS 32G eMMC>\n");
	for (i = 0; i < TOTAL_HEALTH_BYTE; i++)
		seq_printf(m, "%02x", mmc_health->health_data[i]);

	return 0;
}

static int sprd_original_health_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, sprd_original_health_data_show, inode->i_private);
}

static const struct proc_ops original_health_data_fops = {
	.proc_open = sprd_original_health_data_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops *proc_fops_list[] = {
	&health_data_fops,
	&original_health_data_fops,
};

static char * const sprd_health_node_info[] = {
	"health_data",
	"original_health_data",
};

static int sprd_create_mmc_health_init(void)
{
	struct proc_dir_entry *mmchealthdir;
	struct proc_dir_entry *prentry;
	int i, node;

	mmchealthdir = proc_mkdir("mmc_health", NULL);
	if (!mmchealthdir) {
		pr_err("%s: failed to create /proc/mmc_health\n",
			__func__);
		return -ENOMEM;
	}

	node = ARRAY_SIZE(sprd_health_node_info);
	for (i = 0; i < node; i++) {
		prentry = proc_create(sprd_health_node_info[i], PROC_MODE,
					mmchealthdir, proc_fops_list[i]);
		if (!prentry) {
			pr_err("%s,failed to create node: /proc/mmc_health/%s\n",
				__func__, sprd_health_node_info[i]);
			goto error_tree;
		}
	}
	return 0;

error_tree:
	proc_remove(mmchealthdir);
	return -ENOMEM;
}

int sprd_mmc_health_init(struct mmc_card *card)
{
	int err = 0;

	if (!card || mmc_health)
		return -EINVAL;

	/* mmc health init*/
	err = mmc_health_init(card);
	if (err) {
		pr_err("%s: mmc health init failed, err=%d\n",
			mmc_hostname(card->host), err);
		goto out;
	}

	/* create mmc health node */
	err = sprd_create_mmc_health_init();
	if (err && mmc_health)
		devm_kfree(mmc_health->card->host->parent, mmc_health);
out:
	return err;
}
