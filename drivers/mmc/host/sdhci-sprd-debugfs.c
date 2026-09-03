// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2023 Spreadtrum, Inc.
// Author: Wenchao Chen <wenchao.chen@unisoc.com>
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/fault-inject.h>
#include <linux/proc_fs.h>
#include <linux/profile.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>

#include "../core/core.h"
#include "../core/card.h"
#include "../core/host.h"
#include "../core/mmc_ops.h"
#include "sdhci.h"
#include "mmc_swcq.h"

#define SPRD_SPEED_INFO_VALID 2048

#define SPRD_SPEED_MODE_NAME_MAX	20
#define SPRD_SPEED_MODE_NAME_MIN	2

#define MMC_TIMING 0
#define SD_TIMING 1
#define NOT_SUPPORT 3

struct mmc_speed_config {
	char *name;
	u8 support; /* support: sd or mmc */
	u32 caps; /* sd: need modify host caps */
	u32 type; /* mmc: need modify mmc_avail_type */
};

struct mmc_through_put {
	u64 read;
	u64 write;
	u64 read_blk;
	u64 write_blk;
};

struct sprd_mmc_node_info {
	char *name;
	umode_t mode;
};

static struct mmc_through_put mmc_throughput[3];
static u32 mmc_debug_polling_times = 5;
static u32 mmc_throughput_threshold = 30;
atomic_t mmc_debug_en;
atomic_t force_err;
static u32 force_err_count;
static u32 mmc_debug_sd_anomaly;

static long atol(const char *s)
{
	unsigned long ret = 0;
	unsigned long d;
	int neg = 0;

	if (*s == '-') {
		neg = 1;
		s++;
	}

	while (1) {
		d = (*s++) - '0';
		if (d > 9)
			break;
		ret *= 10;
		ret += d;
	}

	return neg ? -ret : ret;
}

static int atoi(const char *s)
{
	return atol(s);
}

static const struct mmc_speed_config mmc_speed[] = {
	{"LEGACY", SD_TIMING, MMC_CAP_SD_HIGHSPEED, 0}, /* MMC_TIMING_LEGACY: 0 */
	{"HS", MMC_TIMING, 0, EXT_CSD_CARD_TYPE_HS}, /* MMC_TIMING_MMC_HS: 1 */
	{"HS", SD_TIMING, MMC_CAP_SD_HIGHSPEED, 0}, /* MMC_TIMING_SD_HS: 2 */
	{"SDR12", NOT_SUPPORT, MMC_CAP_UHS_SDR12, 0}, /* MMC_TIMING_UHS_SDR12: 3 */
	{"SDR25", NOT_SUPPORT, MMC_CAP_UHS_SDR25, 0}, /* MMC_TIMING_UHS_SDR25: 4 */
	{"SDR50", SD_TIMING, MMC_CAP_UHS_SDR50, 0}, /* MMC_TIMING_UHS_SDR50: 5 */
	{"SDR104", SD_TIMING, MMC_CAP_UHS_SDR104, 0}, /* MMC_TIMING_UHS_SDR104: 6 */
	{"DDR50", NOT_SUPPORT, MMC_CAP_DDR, 0}, /* MMC_TIMING_UHS_DDR50: 7 */
	{"DDR52", NOT_SUPPORT, 0, EXT_CSD_CARD_TYPE_DDR_52}, /* MMC_TIMING_MMC_DDR52: 8 */
	{"HS200", MMC_TIMING, 0, EXT_CSD_CARD_TYPE_HS200}, /* MMC_TIMING_MMC_HS200: 9 */
	{"HS400", MMC_TIMING, 0, EXT_CSD_CARD_TYPE_HS400}, /* MMC_TIMING_MMC_HS400: 10 */
	{"HS400ES", MMC_TIMING, 0, EXT_CSD_CARD_TYPE_HS400ES}, /* add new define HS400_ES: 11 */
};

static int sdhci_sprd_set_timing_show(struct seq_file *file, void *data)
{
	struct mmc_host *host = file->private;
	u8 timing = host->ios.enhanced_strobe ? host->ios.timing + 1 : host->ios.timing;
	static const char * const mmc_select_mode[] = {
		"support select HS, HS200, HS400, HS400ES\n", /* EMMC */
		"support select LEGACY, HS, SDR50, SDR104\n" /* SD */
	};

	seq_printf(file, "%s current speed is: [%s], %s\n",
		mmc_hostname(host), mmc_speed[timing].name, mmc_select_mode[host->index]);

	return 0;
}

static int sdhci_sprd_set_timing_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_sprd_set_timing_show, PDE_DATA(inode));
}

static bool sdhci_sprd_raw_cfg(struct mmc_host *host, u32 caps, u32 mmc_type)
{
	static u32 sd_caps, mmc_avail_type; /* record mmc default config */

	if (mmc_card_sd(host->card)) {
		sd_caps |= host->caps;
		host->caps = sd_caps;
	} else if (mmc_card_mmc(host->card)) {
		mmc_avail_type |= host->card->mmc_avail_type;
		host->card->mmc_avail_type = mmc_avail_type;
	}

	if (!(sd_caps & caps) && !(mmc_avail_type & mmc_type))
		return false;

	return true;
}

static ssize_t sdhci_sprd_set_timing_write(struct file *filp, const char __user *ubuf,
				   size_t cnt, loff_t *ppos)
{
	struct mmc_host *host = PDE_DATA(file_inode(filp));
	struct mmc_card *card = host->card;
	struct mmc_blk_data *md;
	struct mmc_queue *mq;
	const char *name = mmc_hostname(host);
	bool cmdq_dis = false;
	int busy = 1000;
	char temp[SPRD_SPEED_MODE_NAME_MAX] = {0};
	bool flag = false;
	int i, err = 0;

	if (!card || mmc_card_sdio(card) ||
		cnt > SPRD_SPEED_MODE_NAME_MAX || cnt < SPRD_SPEED_MODE_NAME_MIN)
		return cnt;

	if (copy_from_user(temp, ubuf, cnt - 1))
		return -EFAULT;

	for (i = 0; i < ARRAY_SIZE(mmc_speed); i++) {
		if (mmc_speed[i].support != host->index)
			continue;
		if (flag) {
			card->mmc_avail_type &= ~mmc_speed[i].type;
			host->caps &= ~mmc_speed[i].caps;
		} else if (!strcmp(mmc_speed[i].name, temp)) {
			if (!sdhci_sprd_raw_cfg(host, mmc_speed[i].caps, mmc_speed[i].type))
				break;
			flag = true;
		}
	}

	if (!flag) {
		pr_err("%s:(set timing) does not support %s, set fail!\n", name, temp);
		return cnt;
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
		pr_err("%s:(set timing) mq busy\n", name);
		return err;
	}

	mmc_claim_host(host);

	if (card->ext_csd.cmdq_en) {
		err = mmc_cmdq_disable(card);
		if (err) {
			pr_err("%s:(set timing) cmdq disable fail,err=%d\n", name, err);
			goto fail;
		}
		cmdq_dis = true;
	}

	err = mmc_hw_reset(host);
	if (err >= 0)
		pr_info("%s:(set timing) current speed is: [%s], set success!\n",
			mmc_hostname(host), temp);

fail:
	if (cmdq_dis) {
		err = mmc_cmdq_enable(card);
		if (err) {
			pr_err("%s:(set timing) cmdq enable fail, err=%d\n", name, err);
			mmc_hw_reset(host);
		}
	}

	spin_lock_irq(&mq->lock);
	mq->busy = false;
	spin_unlock_irq(&mq->lock);

	mmc_release_host(host);

	return cnt;
}

static const struct proc_ops sdhci_sprd_set_timing_fops = {
	.proc_open = sdhci_sprd_set_timing_open,
	.proc_read = seq_read,
	.proc_write = sdhci_sprd_set_timing_write,
	.proc_release = single_release,
};

static int sdhci_sprd_reset_show(struct seq_file *file, void *data)
{
	seq_printf(file, "reset triger: %d\n", force_err_count);

	return 0;
}

static int sdhci_sprd_reset_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_sprd_reset_show, inode->i_private);
}

static bool decode_state(const char *buf, size_t n)
{
	char *p;
	int len;

	p = memchr(buf, '\n', n);
	len = p ? p - buf : n;

	if (len == 6 && str_has_prefix(buf, "triger"))
		return true;

	return false;
}

static ssize_t sdhci_sprd_reset_write(struct file *filp, const char __user *ubuf,
				   size_t cnt, loff_t *ppos)
{
	struct mmc_host *host = PDE_DATA(file_inode(filp));
	char temp[7];

	if (copy_from_user(&temp, ubuf, sizeof(temp)))
		return -EFAULT;

	if (!decode_state(temp, sizeof(temp)))
		return -EINVAL;

	atomic_set(&force_err, true);
	force_err_count++;
	pr_info("%s: triger hw_reset\n", mmc_hostname(host));

	return cnt;
}

static const struct proc_ops sdhci_sprd_reset_fops = {
	.proc_open = sdhci_sprd_reset_open,
	.proc_read = seq_read,
	.proc_write = sdhci_sprd_reset_write,
	.proc_release = single_release,
};

static int sdhci_sprd_debug_en_show(struct seq_file *file, void *data)
{
	seq_printf(file, "%d\n", atomic_read(&mmc_debug_en));

	return 0;
}

static int sdhci_sprd_debug_en_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_sprd_debug_en_show, inode->i_private);
}

static ssize_t sdhci_sprd_debug_en_write(struct file *filp, const char __user *ubuf,
				   size_t cnt, loff_t *ppos)
{
	char val;

	if (cnt <= 0)
		goto end;

	if (get_user(val, ubuf))
		return -EFAULT;

	if (val == '1')
		atomic_set(&mmc_debug_en, true);
	else
		atomic_set(&mmc_debug_en, false);

end:
	return cnt;
}

static const struct proc_ops sdhci_sprd_debug_en_fops = {
	.proc_open = sdhci_sprd_debug_en_open,
	.proc_read = seq_read,
	.proc_write = sdhci_sprd_debug_en_write,
	.proc_release = single_release,
};

static int sdhci_sprd_debug_polling_times_show(struct seq_file *file, void *data)
{
	seq_printf(file, "%d\n", mmc_debug_polling_times);

	return 0;
}

static int sdhci_sprd_debug_polling_times_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_sprd_debug_polling_times_show, inode);
}

static ssize_t sdhci_sprd_debug_polling_times_write(struct file *filp,
				const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char val[10];
	int pt;

	if (cnt <= 0)
		goto end;

	if (copy_from_user(val, ubuf, 10))
		return -EFAULT;

	pt = atoi(val);
	if (pt > 0)
		mmc_debug_polling_times = pt;

end:
	return cnt;
}

static const struct proc_ops sdhci_sprd_debug_polling_times_fops = {
	.proc_open = sdhci_sprd_debug_polling_times_open,
	.proc_read = seq_read,
	.proc_write = sdhci_sprd_debug_polling_times_write,
	.proc_release = single_release,
};

static int sdhci_sprd_throughput_show(struct seq_file *file, void *data)
{
	struct mmc_host *mmc;

	mmc = PDE_DATA(file->private);
	if (!mmc)
		return 0;

	seq_printf(file, "%lld\n", mmc_throughput[mmc->index].read);
	seq_printf(file, "%lld\n", mmc_throughput[mmc->index].write);
	seq_printf(file, "%lld\n", mmc_throughput[mmc->index].read_blk);
	seq_printf(file, "%lld\n", mmc_throughput[mmc->index].write_blk);
	return 0;
}

static int sdhci_sprd_throughput_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_sprd_throughput_show, inode);
}

static ssize_t sdhci_sprd_throughput_write(struct file *filp, const char __user *ubuf,
					size_t cnt, loff_t *ppos)
{
	char val[10];
	int tt;

	if (cnt <= 0)
		goto end;

	if (copy_from_user(val, ubuf, 10))
		return -EFAULT;

	tt = atoi(val);
	if (tt > 0)
		mmc_throughput_threshold = tt;

	pr_info("%s: debug polling times %d\n", __func__, mmc_throughput_threshold);

end:
	return cnt;
}

static const struct proc_ops sdhci_sprd_throughput_fops = {
	.proc_open = sdhci_sprd_throughput_open,
	.proc_read = seq_read,
	.proc_write = sdhci_sprd_throughput_write,
	.proc_release = single_release,
};

static int sdhci_sprd_debug_sd_anomaly_show(struct seq_file *file, void *data)
{
	seq_printf(file, "%d\n", mmc_debug_sd_anomaly);

	return 0;
}

static int sdhci_sprd_debug_sd_anomaly_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_sprd_debug_sd_anomaly_show, inode);
}

static ssize_t sdhci_sprd_debug_sd_anomaly_write(struct file *filp,
				const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char val[10];
	int pt;

	if (cnt < 0)
		goto end;

	if (copy_from_user(val, ubuf, 10))
		return -EFAULT;

	pt = atoi(val);
	if (pt >= 0)
		mmc_debug_sd_anomaly = pt;

end:
	return cnt;
}

static const struct proc_ops sdhci_sprd_debug_sd_anomaly_fops = {
	.proc_open = sdhci_sprd_debug_sd_anomaly_open,
	.proc_read = seq_read,
	.proc_write = sdhci_sprd_debug_sd_anomaly_write,
	.proc_release = single_release,
};

static const struct proc_ops *proc_fops_mmc_list[] = {
	&sdhci_sprd_reset_fops,
	&sdhci_sprd_debug_en_fops,
	&sdhci_sprd_debug_polling_times_fops,
	&sdhci_sprd_throughput_fops,
	&sdhci_sprd_set_timing_fops,
	&sdhci_sprd_debug_sd_anomaly_fops,
};

static const struct sprd_mmc_node_info mmc_node_info[] = {
	{"hw_reset", 0600},
	{"debug_en", 0666},
	{"debug_polling_times", 0666},
	{"throughput", 0666},
	{"set_timing", 0660},
};

void sdhci_sprd_add_host_debugfs(struct sdhci_host *host)
{
	struct proc_dir_entry *debug_parent;
	struct proc_dir_entry *debug_data;
	struct mmc_host *mmc = host->mmc;
	int i, node;

	debug_parent = proc_mkdir(mmc_hostname(mmc), NULL);
	if (!debug_parent) {
		pr_err("%s: failed to create sprd_host_debug proc entry\n",
			__func__);
		goto err;
	}

	if (mmc->index == 1) {
		debug_data = proc_create_data("set_timing", 0660,
			debug_parent, &sdhci_sprd_set_timing_fops, mmc);
		if (!debug_data) {
			pr_err("%s: failed to create node: /proc/%s/set_timing\n",
				__func__, mmc_hostname(mmc));
			goto err;
		}

		debug_data = proc_create_data("debug_sd_anomaly", 0660,
			debug_parent, &sdhci_sprd_debug_sd_anomaly_fops, mmc);
		if (!debug_data) {
			pr_err("%s: failed to create node: /proc/%s/set_timing\n",
				__func__, mmc_hostname(mmc));
			goto err;
		}
	}

	if (mmc->index > 0)
		return;

	atomic_set(&mmc_debug_en, true);
	atomic_set(&force_err, false);

	node = ARRAY_SIZE(mmc_node_info);
	for (i = 0; i < node; i++) {
		debug_data = proc_create_data(mmc_node_info[i].name, mmc_node_info[i].mode,
			debug_parent, proc_fops_mmc_list[i], mmc);
		if (!debug_data) {
			pr_err("%s: failed to create node: /proc/%s/%s\n",
				__func__, mmc_hostname(mmc), mmc_node_info[i].name);
			goto err;
		}
	}

	return;

err:
	remove_proc_subtree(mmc_hostname(mmc), NULL);
}
EXPORT_SYMBOL(sdhci_sprd_add_host_debugfs);

bool sdhci_sprd_mmc_debug_judge(void)
{
	return atomic_read(&mmc_debug_en);
}
EXPORT_SYMBOL(sdhci_sprd_mmc_debug_judge);

void sdhci_sprd_force_error(bool enable)
{
	atomic_set(&force_err, enable);
}
EXPORT_SYMBOL(sdhci_sprd_force_error);

int sdhci_sprd_should_force_error(void)
{
	return atomic_read(&force_err);
}
EXPORT_SYMBOL(sdhci_sprd_should_force_error);

void sdhci_sprd_mmc_update_throughput(struct sdhci_host *host,
		u64 read, u64 write, u64 read_blk, u64 write_blk)
{
	struct mmc_host *mmc = host->mmc;
	char type[20];
	char event[30];
	char *envp[3] = {type, event, NULL};

	if (mmc->index > 0)
		return;

	if ((read < mmc_throughput_threshold && read_blk > SPRD_SPEED_INFO_VALID) ||
		(write < mmc_throughput_threshold && write_blk > SPRD_SPEED_INFO_VALID)) {
		snprintf(type, ARRAY_SIZE(type), "MMCTYPE=%s", "EMMC");
		snprintf(event, ARRAY_SIZE(event), "SPEED=%lld, %lld, BLOCKS=%lld, %lld",
			read, write, read_blk, write_blk);
		kobject_uevent_env(&host->mmc->class_dev.kobj, KOBJ_CHANGE, envp);
	}

	mmc_throughput[mmc->index].read_blk = read_blk;
	mmc_throughput[mmc->index].write_blk = write_blk;

	if (read_blk <= SPRD_SPEED_INFO_VALID)
		mmc_throughput[mmc->index].read = -1;
	else
		mmc_throughput[mmc->index].read = read;

	if (write_blk <= SPRD_SPEED_INFO_VALID)
		mmc_throughput[mmc->index].write = -1;
	else
		mmc_throughput[mmc->index].write = write;
}
EXPORT_SYMBOL(sdhci_sprd_mmc_update_throughput);

u32 sdhci_sprd_mmc_update_polling_times(void)
{
	return mmc_debug_polling_times;
}
EXPORT_SYMBOL(sdhci_sprd_mmc_update_polling_times);

u32 sdhci_sprd_mmc_update_sd_anomaly(void)
{
	return mmc_debug_sd_anomaly;
}
EXPORT_SYMBOL(sdhci_sprd_mmc_update_sd_anomaly);
