// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/device.h>       /* needed by device_* */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>

#include "scp_ipi.h"
#include "scp_helper.h"

#define DRAM_BUF_LEN				(1 * 1024 * 1024)
#define SCP_TIMER_TIMEOUT	        (1 * HZ) /* 1 seconds*/
#define ROUNDUP(a, b)		        (((a) + ((b)-1)) & ~((b)-1))
#define PLT_LOG_ENABLE              0x504C5402 /*magic*/
#define SCP_IPI_RETRY_TIMES         (5000)

/* bit0 = 1, logger is on, else off*/
#define SCP_LOGGER_ON_BIT       (1<<0)
/* bit1 = 1, logger_dram_use is on, else off*/
#define SCP_LOGGER_DRAM_ON_BIT  (1<<1)
/* bit8 = 1, enable function (logger/logger dram use) */
#define SCP_LOGGER_ON_CTRL_BIT    (1<<8)
/* bit8 = 0, disable function */
#define SCP_LOGGER_OFF_CTRL_BIT		(0<<8)
/* let logger on */
#define SCP_LOGGER_ON       (SCP_LOGGER_ON_CTRL_BIT | SCP_LOGGER_ON_BIT)
/* let logger off */
#define SCP_LOGGER_OFF      (SCP_LOGGER_OFF_CTRL_BIT | SCP_LOGGER_ON_BIT)
/* let logger dram use on */
#define SCP_LOGGER_DRAM_ON  (SCP_LOGGER_ON_CTRL_BIT | SCP_LOGGER_DRAM_ON_BIT)
/* let logger dram use off */
#define SCP_LOGGER_DRAM_OFF (SCP_LOGGER_OFF_CTRL_BIT | SCP_LOGGER_DRAM_ON_BIT)
#define SCP_LOGGER_UT (1)

struct log_ctrl_s {
	unsigned int base;
	unsigned int size;
	unsigned int enable;
	unsigned int info_ofs;
	unsigned int buff_ofs;
	unsigned int buff_size;
};

struct buffer_info_s {
	unsigned int r_pos;
	unsigned int w_pos;
};

struct SCP_LOG_INFO {
	unsigned int scp_log_dram_addr;
	unsigned int scp_log_buf_addr;
	unsigned int scp_log_start_addr;
	unsigned int scp_log_end_addr;
	unsigned int scp_log_buf_maxlen;
};


static unsigned int scp_A_logger_inited;
static unsigned int scp_A_logger_wakeup_ap;

static struct log_ctrl_s *SCP_A_log_ctl;
static struct buffer_info_s *SCP_A_buf_info;
/*static struct timer_list scp_log_timer;*/
static DEFINE_MUTEX(scp_A_log_mutex);
static DEFINE_SPINLOCK(scp_A_log_buf_spinlock);
static struct scp_work_struct scp_logger_notify_work[SCP_CORE_TOTAL];

/*scp last log info*/
#define LAST_LOG_BUF_SIZE  4095
static struct SCP_LOG_INFO last_log_info;

static char *scp_A_last_log;
static wait_queue_head_t scp_A_logwait;

static DEFINE_MUTEX(scp_logger_mutex);
static char *scp_last_logger;
/*global value*/
unsigned int r_pos_debug;
unsigned int log_ctl_debug;
static struct mutex scp_logger_mutex;

/*
 * get log from scp when received a buf full notify
 * @param id:   IPI id
 * @param data: IPI data
 * @param len:  IPI data length
 */
static void scp_logger_wakeup_handler(int id, void *data, unsigned int len)
{
	pr_debug("[SCP]wakeup by SCP logger\n");
}

/*
 * get log from scp to last_log_buf
 * @param len:  data length
 * @return:     length of log
 */
static size_t scp_A_get_last_log(size_t b_len)
{
	size_t ret = 0;
	int scp_awake_flag;
	unsigned int log_start_idx;
	unsigned int log_end_idx;
	unsigned int update_start_idx;
	unsigned char *scp_last_log_buf =
		(unsigned char *)(SCP_TCM + last_log_info.scp_log_buf_addr);

	/*pr_debug("[SCP] %s\n", __func__);*/

	if (!scp_A_logger_inited) {
		pr_err("[SCP] %s(): logger has not been init\n", __func__);
		return 0;
	}
	mutex_lock(&scp_logger_mutex);
	/*SCP keep awake */
	scp_awake_flag = 0;
	if (scp_awake_lock(SCP_A_ID) == -1) {
		scp_awake_flag = -1;
		pr_debug("[SCP] %s: awake scp fail\n", __func__);
	}

	log_start_idx = readl((void __iomem *)(SCP_TCM +
					last_log_info.scp_log_start_addr));
	log_end_idx = readl((void __iomem *)(SCP_TCM +
					last_log_info.scp_log_end_addr));

	if (b_len > last_log_info.scp_log_buf_maxlen) {
		pr_debug("[SCP] b_len %zu > scp_log_buf_maxlen %d\n",
			b_len, last_log_info.scp_log_buf_maxlen);
		b_len = last_log_info.scp_log_buf_maxlen;
	}
	/* handle sram error */
	if (log_end_idx >= last_log_info.scp_log_buf_maxlen)
		log_end_idx = 0;

	if (log_end_idx >= b_len)
		update_start_idx = log_end_idx - b_len;
	else
		update_start_idx = last_log_info.scp_log_buf_maxlen -
					(b_len - log_end_idx) + 1;

	/* read log from scp buffer */
	ret = 0;
	if (scp_A_last_log) {
		while ((update_start_idx != log_end_idx) && ret < b_len) {
			scp_A_last_log[ret] =
				scp_last_log_buf[update_start_idx];
			update_start_idx++;
			ret++;
			if (update_start_idx >=
				last_log_info.scp_log_buf_maxlen)
				update_start_idx = update_start_idx -
					last_log_info.scp_log_buf_maxlen;

			scp_A_last_log[ret] = '\0';
		}
	} else {
		/* no buffer, just skip logs*/
		update_start_idx = log_end_idx;
	}

	/*SCP release awake */
	if (scp_awake_flag == 0) {
		if (scp_awake_unlock(SCP_A_ID) == -1)
			pr_debug("[SCP] %s: awake unlock fail\n", __func__);
	}

	mutex_unlock(&scp_logger_mutex);
	return ret;
}

ssize_t scp_A_log_read(char __user *data, size_t len)
{
	unsigned int w_pos, r_pos, datalen;
	char *buf;

	if (!scp_A_logger_inited)
		return 0;

	datalen = 0;

	mutex_lock(&scp_A_log_mutex);

	r_pos = SCP_A_buf_info->r_pos;
	w_pos = SCP_A_buf_info->w_pos;

	if (r_pos == w_pos)
		goto error;

	if (r_pos > w_pos)
		datalen = DRAM_BUF_LEN - r_pos; /* not wrap */
	else
		datalen = w_pos - r_pos;

	if (datalen > len)
		datalen = len;

	/*debug for logger pos fail*/
	r_pos_debug = r_pos;
	log_ctl_debug = SCP_A_log_ctl->buff_ofs;
	if (r_pos >= DRAM_BUF_LEN) {
		pr_err("[SCP] %s(): r_pos >= DRAM_BUF_LEN,%x,%x\n",
			__func__, r_pos_debug, log_ctl_debug);
		datalen = 0;
		goto error;
	}

	buf = ((char *) SCP_A_log_ctl) + SCP_A_log_ctl->buff_ofs + r_pos;

	len = datalen;
	/*memory copy from log buf*/
	if (copy_to_user(data, buf, len))
		pr_debug("[SCP]copy to user buf failed..\n");

	r_pos += datalen;
	if (r_pos >= DRAM_BUF_LEN)
		r_pos -= DRAM_BUF_LEN;

	SCP_A_buf_info->r_pos = r_pos;

error:
	mutex_unlock(&scp_A_log_mutex);

	return datalen;
}

unsigned int scp_A_log_poll(void)
{
	if (!scp_A_logger_inited)
		return 0;

	if (SCP_A_buf_info->r_pos != SCP_A_buf_info->w_pos)
		return POLLIN | POLLRDNORM;

	/*scp_log_timer_add();*/

	return 0;
}


static ssize_t scp_A_log_if_read(struct file *file,
		char __user *data, size_t len, loff_t *ppos)
{
	ssize_t ret = 0;

	/*if (access_ok(VERIFY_WRITE, data, len))*/
	ret = scp_A_log_read(data, len);

	return ret;
}

static int scp_A_log_if_open(struct inode *inode, struct file *file)
{
	/*pr_debug("[SCP A] scp_A_log_if_open\n");*/
	return nonseekable_open(inode, file);
}

static unsigned int scp_A_log_if_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	/* pr_debug("[SCP A] scp_A_log_if_poll\n"); */
	if (!scp_A_logger_inited)
		return 0;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	/*poll_wait(file, &scp_A_logwait, wait);*/

	ret = scp_A_log_poll();

	return ret;
}
/*
 * ipi send to enable scp logger flag
 */
static unsigned int scp_A_log_enable_set(unsigned int enable)
{
	int ret;
	unsigned int retrytimes;

	if (scp_A_logger_inited) {
		/*
		 *send ipi to invoke scp logger
		 */
		ret = 0;
		enable = (enable) ? SCP_LOGGER_ON : SCP_LOGGER_OFF;
		retrytimes = SCP_IPI_RETRY_TIMES;
		do {
			ret = scp_ipi_send(IPI_LOGGER_ENABLE,
				&enable, sizeof(enable), 0, SCP_A_ID);
			if (ret == SCP_IPI_DONE)
				break;
			retrytimes--;
			udelay(100);
		} while (retrytimes > 0);
		/*
		 *disable/enable logger flag
		 */
		if ((ret == SCP_IPI_DONE) && (enable == 1))
			SCP_A_log_ctl->enable = 1;
		else if ((ret == SCP_IPI_DONE) && (enable == 0))
			SCP_A_log_ctl->enable = 0;

		if (ret != SCP_IPI_DONE) {
			pr_err("[SCP] %s: fail ret=%d\n", __func__, ret);
			goto error;
		}

	}

error:
	return 0;
}

/*
 *ipi send enable scp logger wake up flag
 */
static unsigned int scp_A_log_wakeup_set(unsigned int enable)
{
	int ret;
	unsigned int retrytimes;

	if (scp_A_logger_inited) {
		/*
		 *send ipi to invoke scp logger
		 */
		ret = 0;
		enable = (enable) ? 1 : 0;
		retrytimes = SCP_IPI_RETRY_TIMES;
		do {
			ret = scp_ipi_send(IPI_LOGGER_WAKEUP, &enable,
				sizeof(enable), 0, SCP_A_ID);
			if (ret == SCP_IPI_DONE)
				break;
			retrytimes--;
			udelay(100);
		} while (retrytimes > 0);
		/*
		 *disable/enable logger flag
		 */
		if ((ret == SCP_IPI_DONE) && (enable == 1))
			scp_A_logger_wakeup_ap = 1;
		else if ((ret == SCP_IPI_DONE) && (enable == 0))
			scp_A_logger_wakeup_ap = 0;

		if (ret != SCP_IPI_DONE) {
			pr_err("[SCP] %s: fail ret=%d\n", __func__, ret);
			goto error;
		}

	}

error:
	return 0;
}

/*
 * create device sysfs, scp logger status
 */
static ssize_t scp_mobile_log_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	unsigned int stat;

	stat = (scp_A_logger_inited && SCP_A_log_ctl->enable) ? 1 : 0;

	return scnprintf(buf, PAGE_SIZE, "[SCP A] mobile log is %s\n",
			(stat == 0x1) ? "enabled" : "disabled");
}

static ssize_t scp_mobile_log_store(struct device *kobj,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&scp_A_log_mutex);
	scp_A_log_enable_set(enable);
	mutex_unlock(&scp_A_log_mutex);

	return n;
}

DEVICE_ATTR_RW(scp_mobile_log);

/*
 * create device sysfs, scp ADB cmd to set SCP wakeup AP flag
 */
static ssize_t scp_A_logger_wakeup_AP_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	unsigned int stat;

	stat = (scp_A_logger_inited && scp_A_logger_wakeup_ap) ? 1 : 0;

	return scnprintf(buf, PAGE_SIZE, "[SCP A] logger wakeup AP is %s\n",
			(stat == 0x1) ? "enabled" : "disabled");
}

static ssize_t scp_A_logger_wakeup_AP_store(struct device *kobj,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&scp_A_log_mutex);
	scp_A_log_wakeup_set(enable);
	mutex_unlock(&scp_A_log_mutex);

	return n;
}

DEVICE_ATTR_RW(scp_A_logger_wakeup_AP);


/*
 * create device sysfs, scp last log show
 */
static ssize_t scp_A_get_last_log_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	scp_A_get_last_log(last_log_info.scp_log_buf_maxlen);
	return scnprintf(buf, PAGE_SIZE, "scp_log_buf_maxlen=%u, log=%s\n",
			last_log_info.scp_log_buf_maxlen,
			scp_A_last_log ? scp_A_last_log : "");
}

DEVICE_ATTR_RO(scp_A_get_last_log);

/*
 * logger UT test
 *
 */
#if SCP_LOGGER_UT
static ssize_t scp_A_mobile_log_UT_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	unsigned int w_pos, r_pos, datalen;
	char *logger_buf;
	size_t len = 1024;

	if (!scp_A_logger_inited)
		return 0;

	datalen = 0;

	mutex_lock(&scp_A_log_mutex);

	r_pos = SCP_A_buf_info->r_pos;
	w_pos = SCP_A_buf_info->w_pos;

	if (r_pos == w_pos)
		goto error;

	if (r_pos > w_pos)
		datalen = DRAM_BUF_LEN - r_pos; /* not wrap */
	else
		datalen = w_pos - r_pos;

	if (datalen > len)
		datalen = len;

	logger_buf = ((char *) SCP_A_log_ctl) +
			SCP_A_log_ctl->buff_ofs + r_pos;

	len = datalen;
	/*memory copy from log buf*/
	memcpy_fromio(buf, logger_buf, len);

	r_pos += datalen;
	if (r_pos >= DRAM_BUF_LEN)
		r_pos -= DRAM_BUF_LEN;

	SCP_A_buf_info->r_pos = r_pos;

error:
	mutex_unlock(&scp_A_log_mutex);

	return len;
}

static ssize_t scp_A_mobile_log_UT_store(struct device *kobj,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&scp_A_log_mutex);
	scp_A_log_enable_set(enable);
	mutex_unlock(&scp_A_log_mutex);

	return n;
}

DEVICE_ATTR_RW(scp_A_mobile_log_UT);
#endif

/*
 * IPI for logger init
 * @param id:   IPI id
 * @param data: IPI data
 * @param len:  IPI data length
 */
static void scp_A_logger_init_handler(int id, void *data, unsigned int len)
{
	unsigned long flags;
	struct SCP_LOG_INFO *log_info = (struct SCP_LOG_INFO *)data;

	pr_debug("[SCP]scp_get_reserve_mem_phys=%llx\n",
		(uint64_t)scp_get_reserve_mem_phys(SCP_A_LOGGER_MEM_ID));
	spin_lock_irqsave(&scp_A_log_buf_spinlock, flags);
	/* sync scp last log information*/
	last_log_info.scp_log_dram_addr = log_info->scp_log_dram_addr;
	last_log_info.scp_log_buf_addr = log_info->scp_log_buf_addr;
	last_log_info.scp_log_start_addr = log_info->scp_log_start_addr;
	last_log_info.scp_log_end_addr = log_info->scp_log_end_addr;
	last_log_info.scp_log_buf_maxlen = log_info->scp_log_buf_maxlen;
	/* setting dram ctrl config to scp*/
	/* scp side get wakelock, AP to write info to scp sram*/
	writel(scp_get_reserve_mem_phys(SCP_A_LOGGER_MEM_ID),
	(SCP_TCM + last_log_info.scp_log_dram_addr));
	/* set init flag here*/
	scp_A_logger_inited = 1;
	spin_unlock_irqrestore(&scp_A_log_buf_spinlock, flags);

	/*set a wq to enable scp logger*/
	scp_logger_notify_work[SCP_A_ID].id = SCP_A_ID;
#if SCP_LOGGER_ENABLE
	scp_schedule_logger_work(&scp_logger_notify_work[SCP_A_ID]);
#endif
}

/*
 * callback function for work struct
 * notify apps to start their tasks or generate an exception according to flag
 * NOTE: this function may be blocked and should not be called in interrupt
 *       context
 * @param ws:   work struct
 */
static void scp_logger_notify_ws(struct work_struct *ws)
{
	struct scp_work_struct *sws =
			container_of(ws, struct scp_work_struct, work);
	unsigned int magic = 0x5A5A5A5A;
	unsigned int retrytimes;
	unsigned int scp_core_id = sws->id;
	enum scp_ipi_status ret;
	enum ipi_id scp_ipi_id;

	scp_ipi_id = IPI_LOGGER_INIT_A;

	pr_notice("[SCP] %s: id=%u\n", __func__, scp_ipi_id);
	/*
	 *send ipi to invoke scp logger
	 */
	retrytimes = SCP_IPI_RETRY_TIMES;
	do {
		ret = scp_ipi_send(scp_ipi_id,
			&magic, sizeof(magic), 0, scp_core_id);
		if ((retrytimes % 500) == 0)
			pr_debug("[SCP] %s: ipi ret=%d\n", __func__, ret);

		if ((ret == SCP_IPI_DONE) || (ret == SCP_IPI_ERROR))
			break;
		retrytimes--;
		udelay(2000);
	} while (retrytimes > 0);

	/*enable logger flag*/
	if (ret == SCP_IPI_DONE)
		SCP_A_log_ctl->enable = 1;
	else {
		/* initialization of scp logger fails, but still raise
		 * the flag to dump scp's previous log for debugging.
		 */
		SCP_A_log_ctl->enable = 1;
		pr_err("[SCP]logger initial fail, ipi ret=%d\n", ret);
	}
}


/******************************************************************************
 * init scp logger dram ctrl structure
 * @return:     -1: fail, otherwise: end of buffer
 *****************************************************************************/
int scp_logger_init(phys_addr_t start, phys_addr_t limit)
{
	int last_ofs;

	/*init wait queue*/
	init_waitqueue_head(&scp_A_logwait);
	scp_A_logger_wakeup_ap = 0;
	mutex_init(&scp_logger_mutex);
	/*init work queue*/
	INIT_WORK(&scp_logger_notify_work[SCP_A_ID].work, scp_logger_notify_ws);

	/*init dram ctrl table*/
	last_ofs = 0;
#ifdef CONFIG_ARM64
	SCP_A_log_ctl = (struct log_ctrl_s *) start;
#else
	/* plz fix origial ptr to phys_addr flow */
	SCP_A_log_ctl = (struct log_ctrl_s *) (u32) start;
#endif
	SCP_A_log_ctl->base = PLT_LOG_ENABLE; /* magic */
	SCP_A_log_ctl->enable = 0;
	SCP_A_log_ctl->size = sizeof(*SCP_A_log_ctl);

	last_ofs += SCP_A_log_ctl->size;
	SCP_A_log_ctl->info_ofs = last_ofs;

	last_ofs += sizeof(*SCP_A_buf_info);
	last_ofs = ROUNDUP(last_ofs, 4);
	SCP_A_log_ctl->buff_ofs = last_ofs;
	SCP_A_log_ctl->buff_size = DRAM_BUF_LEN;

	SCP_A_buf_info = (struct buffer_info_s *)
		(((unsigned char *) SCP_A_log_ctl) + SCP_A_log_ctl->info_ofs);
	SCP_A_buf_info->r_pos = 0;
	SCP_A_buf_info->w_pos = 0;

	last_ofs += SCP_A_log_ctl->buff_size;

	if (last_ofs >= limit) {
		pr_err("[SCP]:%s() initial fail, last_ofs=%u, limit=%u\n",
			__func__, last_ofs, (unsigned int) limit);
		goto error;
	}

	/* init last log buffer*/
	last_log_info.scp_log_buf_maxlen = LAST_LOG_BUF_SIZE;
	if (!scp_A_last_log) {
		/* Allocate one more byte for the NULL character. */
		scp_A_last_log = vmalloc(last_log_info.scp_log_buf_maxlen + 1);
		if (scp_A_last_log == NULL)
			goto error;
	}

	/* register logger ini IPI */
	scp_ipi_registration(IPI_LOGGER_INIT_A, scp_A_logger_init_handler,
			"loggerA");
	/* register log wakeup IPI */
	scp_ipi_registration(IPI_LOGGER_WAKEUP, scp_logger_wakeup_handler,
			"log wakeup");

	scp_A_logger_inited = 1;
	return last_ofs;

error:
	scp_A_logger_inited = 0;
	SCP_A_log_ctl = NULL;
	SCP_A_buf_info = NULL;
	return -1;
}

void scp_logger_uninit(void)
{
	char *tmp = scp_A_last_log;

	scp_A_logger_inited = 0;
	scp_A_last_log = NULL;
	vfree(tmp);
}

const struct file_operations scp_A_log_file_ops = {
	.owner = THIS_MODULE,
	.read = scp_A_log_if_read,
	.open = scp_A_log_if_open,
	.poll = scp_A_log_if_poll,
};


/*
 * move scp last log from sram to dram
 * NOTE: this function may be blocked
 * @param scp_core_id:  fill scp id to get last log
 */
void scp_crash_log_move_to_buf(enum scp_core_id scp_id)
{
	int pos;
	unsigned int ret;
	unsigned int length;
	unsigned int log_start_idx;  /* SCP log start pointer */
	unsigned int log_end_idx;    /* SCP log end pointer */
	unsigned int w_pos;          /* buf write pointer */
	char *pre_scp_logger_buf;
	char *dram_logger_buf;       /* dram buffer */

	char *crash_message = "****SCP EE LOG DUMP****\n";
	unsigned char *scp_logger_buf = (unsigned char *)(SCP_TCM +
					last_log_info.scp_log_buf_addr);

	if (!scp_A_logger_inited && scp_id == SCP_A_ID) {
		pr_err("[SCP] %s(): logger has not been init\n", __func__);
		return;
	}

	/* SCP keep awake */
	mutex_lock(&scp_logger_mutex);
	if (scp_awake_lock(scp_id) == -1) {
		pr_debug("[SCP] %s: awake scp fail\n", __func__);
		mutex_unlock(&scp_logger_mutex);
		return;
	}

	log_start_idx = readl((void __iomem *)(SCP_TCM +
				last_log_info.scp_log_start_addr));
	log_end_idx = readl((void __iomem *)(SCP_TCM +
				last_log_info.scp_log_end_addr));

	if (log_end_idx >= log_start_idx)
		length = log_end_idx - log_start_idx;
	else
		length = last_log_info.scp_log_buf_maxlen -
				(log_start_idx - log_end_idx);

	if (length >= last_log_info.scp_log_buf_maxlen) {
		pr_err("[SCP] %s: length >= max\n", __func__);
		length = last_log_info.scp_log_buf_maxlen;
	}

	pre_scp_logger_buf = scp_last_logger;
	scp_last_logger = vmalloc(length + strlen(crash_message) + 1);
	/* read log from scp buffer */
	ret = 0;
	if (scp_last_logger) {
		ret += snprintf(scp_last_logger, strlen(crash_message),
			crash_message);
		ret--;
		while ((log_start_idx != log_end_idx) &&
			ret <= (length + strlen(crash_message))) {
			scp_last_logger[ret] = scp_logger_buf[log_start_idx];
			log_start_idx++;
			ret++;
			if (log_start_idx >= last_log_info.scp_log_buf_maxlen)
				log_start_idx = log_start_idx -
					last_log_info.scp_log_buf_maxlen;

			scp_last_logger[ret] = '\0';
		}
	} else {
		/* no buffer, just skip logs */
		log_start_idx = log_end_idx;
	}

	if (ret != 0) {
		/* get buffer w pos */
		w_pos = SCP_A_buf_info->w_pos;

		if (w_pos >= DRAM_BUF_LEN) {
			pr_err("[SCP] %s(): w_pos >= DRAM_BUF_LEN, w_pos=%u",
				__func__, w_pos);
			return;
		}

		/* copy to dram buffer */
		dram_logger_buf = ((char *) SCP_A_log_ctl) +
			SCP_A_log_ctl->buff_ofs + w_pos;

		/* memory copy from log buf */
		pos = 0;
		while ((pos != ret) && pos <= ret) {
			*dram_logger_buf = scp_last_logger[pos];
			pos++;
			w_pos++;
			dram_logger_buf++;
			if (w_pos >= DRAM_BUF_LEN) {
				/* warp */
				pr_err("[SCP] %s: dram warp\n", __func__);
				w_pos = 0;

				dram_logger_buf = ((char *) SCP_A_log_ctl) +
					SCP_A_log_ctl->buff_ofs;
			}
		}
		/* update write pointer */
		SCP_A_buf_info->w_pos = w_pos;
	}

	/* SCP release awake */
	if (scp_awake_unlock(scp_id) == -1)
		pr_debug("[SCP] %s: awake unlock fail\n", __func__);

	mutex_unlock(&scp_logger_mutex);
	vfree(pre_scp_logger_buf);
}



/*
 * get log from scp and optionally save it
 * NOTE: this function may be blocked
 * @param scp_core_id:  fill scp id to get last log
 */
void scp_get_log(enum scp_core_id scp_id)
{
	pr_debug("[SCP] %s\n", __func__);
#if SCP_LOGGER_ENABLE
	scp_A_get_last_log(SCP_AED_STR_LEN - 200);
	/*move last log to dram*/
	scp_crash_log_move_to_buf(scp_id);
#endif
}

/*
 * return scp last log
 */
char *scp_get_last_log(enum scp_core_id id)
{
	char *last_log;

	last_log = scp_A_last_log;

	return last_log;
}

/*
 * set scp_A_logger_inited
 */
void scp_logger_init_set(unsigned int value)
{
	/*scp_A_logger_inited
	 *  0: logger not init
	 *  1: logger inited
	 */
	scp_A_logger_inited = value;
}

