/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
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

#include <mt-plat/sync_write.h>
#include "adsp_ipi.h"
#include "adsp_helper.h"
#include "adsp_service.h"

#define ADSP_TIMER_TIMEOUT          (1 * HZ) /* 1 seconds */
#define ROUNDUP(a, b)               (((a) + ((b)-1)) & ~((b)-1))
#define PLT_LOG_ENABLE              0x504C5402 /* magic */
#define ADSP_IPI_RETRY_TIMES        (5000)


#define ADSP_LOGGER_UT (1)

struct log_ctrl_s {
	unsigned int base;
	unsigned int size;
	unsigned int enable;
	unsigned int info_ofs;
	unsigned int buff_ofs;
	unsigned int buff_size;
	unsigned char resv1[104]; /* dummy bytes for 128-byte align */
};

struct buffer_info_s {
	unsigned int r_pos;
	unsigned char resv1[124]; /* dummy bytes for 128-byte align */
	unsigned int w_pos;
	unsigned char resv2[124]; /* dummy bytes for 128-byte align */
};

struct adsp_log_info_s {
	unsigned int adsp_log_dram_addr;
	unsigned int adsp_log_buf_addr;
	unsigned int adsp_log_start_addr;
	unsigned int adsp_log_end_addr;
	unsigned int adsp_log_buf_maxlen;
};

struct trax_ctrl_s {
	unsigned int done;
	unsigned int length;
	unsigned int enable;
	unsigned int initiated;
};

static unsigned int adsp_A_logger_inited;
static unsigned int adsp_A_logger_wakeup_ap;

static struct log_ctrl_s *ADSP_A_log_ctl;
static struct buffer_info_s *ADSP_A_buf_info;
/*static struct timer_list adsp_log_timer;*/
static DEFINE_MUTEX(adsp_A_log_mutex);
static DEFINE_SPINLOCK(adsp_A_log_buf_spinlock);
static struct adsp_work_struct adsp_logger_notify_work[ADSP_CORE_TOTAL];
#if ADSP_TRAX
static struct adsp_work_struct adsp_trax_init_work[ADSP_CORE_TOTAL];
struct trax_ctrl_s *pADSP_A_trax_ctl, ADSP_A_trax_ctl;
#endif

/*adsp last log info*/
static unsigned int adsp_A_log_dram_addr_last;
static unsigned int adsp_A_log_buf_addr_last;
static unsigned int adsp_A_log_start_addr_last;
static unsigned int adsp_A_log_end_addr_last;
static unsigned int adsp_A_log_buf_maxlen_last;
static char *adsp_A_last_log;
static wait_queue_head_t adsp_A_logwait;
static unsigned int dram_buf_len;

static DEFINE_MUTEX(adsp_logger_mutex);
static unsigned int adsp_log_buf_addr_last[ADSP_CORE_TOTAL];
static unsigned int adsp_log_start_addr_last[ADSP_CORE_TOTAL];
static unsigned int adsp_log_end_addr_last[ADSP_CORE_TOTAL];
static unsigned int adsp_log_buf_maxlen_last[ADSP_CORE_TOTAL];
/*global value*/
unsigned int adsp_r_pos_debug;
unsigned int adsp_log_ctl_debug;

/*
 * get log from adsp when received a buf full notify
 * @param id:   IPI id
 * @param data: IPI data
 * @param len:  IPI data length
 */
static void adsp_logger_wakeup_handler(int id, void *data, unsigned int len)
{
	pr_debug("[ADSP]wakeup by ADSP logger\n");
}

/*
 * get log from adsp to last_log_buf
 * @param len:  data length
 * @return:     length of log
 */
static size_t adsp_A_get_last_log(size_t b_len)
{
	size_t ret = 0;
	unsigned int log_start_idx;
	unsigned int log_end_idx;
	unsigned int update_start_idx;
	char *pre_adsp_last_log_buf;
	unsigned char *adsp_last_log_buf = (unsigned char *)(ADSP_A_SYS_DRAM +
						      adsp_A_log_buf_addr_last);

	/* pr_debug("[ADSP] %s\n", __func__); */

	if (!adsp_A_logger_inited) {
		pr_info("[ADSP] %s(): logger has not been init\n", __func__);
		return 0;
	}

	log_start_idx = readl((void __iomem *)(ADSP_A_SYS_DRAM +
					       adsp_A_log_start_addr_last));
	log_end_idx = readl((void __iomem *)(ADSP_A_SYS_DRAM +
					     adsp_A_log_end_addr_last));

	if (b_len > adsp_A_log_buf_maxlen_last) {
		pr_debug("[ADSP] b_len %zu > adsp_log_buf_maxlen %d\n", b_len,
			 adsp_A_log_buf_maxlen_last);
		b_len = adsp_A_log_buf_maxlen_last;
	}

	if (log_end_idx >= b_len)
		update_start_idx = log_end_idx - b_len;
	else
		update_start_idx = adsp_A_log_buf_maxlen_last -
				   (b_len - log_end_idx) + 1;

	pre_adsp_last_log_buf = adsp_A_last_log;
	adsp_A_last_log = vmalloc(adsp_A_log_buf_maxlen_last + 1);

	/* read log from adsp buffer */
	ret = 0;
	if (adsp_A_last_log) {
		while ((update_start_idx != log_end_idx) && ret < b_len) {
			adsp_A_last_log[ret] =
					    adsp_last_log_buf[update_start_idx];
			update_start_idx++;
			ret++;
			if (update_start_idx >= adsp_A_log_buf_maxlen_last)
				update_start_idx = update_start_idx -
						   adsp_A_log_buf_maxlen_last;

			adsp_A_last_log[ret] = '\0';
		}
	} else {
		/* no buffer, just skip logs*/
		update_start_idx = log_end_idx;
	}

	vfree(pre_adsp_last_log_buf);
	return ret;
}

ssize_t adsp_A_log_read(char __user *data, size_t len)
{
	unsigned int w_pos, r_pos, datalen;
	char *buf, *to_user_buf;

	if (!adsp_A_logger_inited)
		return 0;

	datalen = 0;

	mutex_lock(&adsp_A_log_mutex);

	r_pos = ADSP_A_buf_info->r_pos;
	w_pos = ADSP_A_buf_info->w_pos;

	if (r_pos == w_pos)
		goto error;

	if (r_pos > w_pos)
		datalen = dram_buf_len - r_pos; /* not wrap */
	else
		datalen = w_pos - r_pos;

	if (datalen > len)
		datalen = len;

	/* debug for logger pos fail */
	adsp_r_pos_debug = r_pos;
	adsp_log_ctl_debug = ADSP_A_log_ctl->buff_ofs;
	if (r_pos >= dram_buf_len) {
		pr_info("[ADSP] %s(): r_pos >= dram_buf_len,%x,%x\n", __func__,
		       adsp_r_pos_debug, adsp_log_ctl_debug);
		return 0;
	}

	buf = ((char *) ADSP_A_log_ctl) + ADSP_A_log_ctl->buff_ofs + r_pos;
	to_user_buf = vmalloc(len);
	len = datalen;
	/* memory copy from log buf */
	if (to_user_buf) {
		memcpy_fromio(to_user_buf, buf, len);
		if (copy_to_user(data, to_user_buf, len))
			pr_debug("[ADSP]copy to user buf failed..\n");

		vfree(to_user_buf);
	} else {
		pr_debug("[ADSP]create log buffer failed..\n");
		goto error;
	}

	r_pos += datalen;
	if (r_pos >= dram_buf_len)
		r_pos -= dram_buf_len;

	ADSP_A_buf_info->r_pos = r_pos;

error:
	mutex_unlock(&adsp_A_log_mutex);

	return datalen;
}

unsigned int adsp_A_log_poll(void)
{
	if (!adsp_A_logger_inited)
		return 0;

	if (ADSP_A_buf_info->r_pos != ADSP_A_buf_info->w_pos)
		return POLLIN | POLLRDNORM;

	/* adsp_log_timer_add(); */

	return 0;
}


static ssize_t adsp_A_log_if_read(struct file *file, char __user *data,
				  size_t len, loff_t *ppos)
{
	ssize_t ret;

	/* pr_debug("[ADSP A] adsp_A_log_if_read\n"); */

	/* if (access_ok(VERIFY_WRITE, data, len)) */
	ret = adsp_A_log_read(data, len);

	return ret;
}

static int adsp_A_log_if_open(struct inode *inode, struct file *file)
{
	/* pr_debug("[ADSP A] adsp_A_log_if_open\n"); */
	return nonseekable_open(inode, file);
}

static unsigned int adsp_A_log_if_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	/* pr_debug("[ADSP A] adsp_A_log_if_poll\n"); */
	if (!adsp_A_logger_inited)
		return 0;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	/* poll_wait(file, &adsp_A_logwait, wait); */

	ret = adsp_A_log_poll();

	return ret;
}
/*
 * ipi send to enable adsp logger flag
 */
static unsigned int adsp_A_log_enable_set(unsigned int enable)
{
	int ret;
	unsigned int retrytimes;

	if (adsp_A_logger_inited) {
		/*
		 *send ipi to invoke adsp logger
		 */
		adsp_register_feature(ADSP_LOGGER_FEATURE_ID);
		ret = 0;
		enable = (enable) ? 1 : 0;
		retrytimes = ADSP_IPI_RETRY_TIMES;
		do {
			ret = adsp_ipi_send(ADSP_IPI_LOGGER_ENABLE, &enable,
					    sizeof(enable), 0, ADSP_A_ID);
			pr_debug("[ADSP] %s enable=%d, ret=%d\n",
				__func__, enable, ret);
			if (ret == ADSP_IPI_DONE)
				break;
			retrytimes--;
			udelay(100);
		} while (retrytimes > 0);
		/*
		 *disable/enable logger flag
		 */
		if ((ret == ADSP_IPI_DONE) && (enable == 1))
			ADSP_A_log_ctl->enable = 1;
		else if ((ret == ADSP_IPI_DONE) && (enable == 0))
			ADSP_A_log_ctl->enable = 0;

		adsp_deregister_feature(ADSP_LOGGER_FEATURE_ID);
		if (ret != ADSP_IPI_DONE) {
			pr_err("[ADSP] %s fail ret=%d\n", __func__,
			       ret);
			goto error;
		}

	}

error:
	return 0;
}

#if ADSP_TRAX
static unsigned int adsp_A_trax_enable_set(unsigned int enable)
{
	int ret;
	unsigned int retrytimes;

	if (pADSP_A_trax_ctl->initiated) {
		/*
		 *send ipi to enable adsp trax
		 */
		adsp_register_feature(ADSP_LOGGER_FEATURE_ID);
		ret = 0;
		enable = (enable) ? 1 : 0;
		retrytimes = ADSP_IPI_RETRY_TIMES;
		do {
			ret = adsp_ipi_send(ADSP_IPI_TRAX_ENABLE, &enable,
					    sizeof(enable), 0, ADSP_A_ID);
			if (ret == ADSP_IPI_DONE)
				break;
			retrytimes--;
			udelay(100);
		} while (retrytimes > 0);
		/*
		 *trax disable/enable flag
		 */
		if ((ret == ADSP_IPI_DONE) && (enable == 1)) {
			pADSP_A_trax_ctl->enable = 1;
			pADSP_A_trax_ctl->done = 0;
		} else if ((ret == ADSP_IPI_DONE) && (enable == 0)) {
			pADSP_A_trax_ctl->enable = 0;
		}

		pr_info("[ADSP] %s enable=%d, done=%d\n", __func__,
			pADSP_A_trax_ctl->enable, pADSP_A_trax_ctl->done);

		adsp_deregister_feature(ADSP_LOGGER_FEATURE_ID);
		if (ret != ADSP_IPI_DONE) {
			pr_err("[ADSP] %s fail ret=%d\n", __func__, ret);
			goto error;
		}
	}

error:
	return 0;
}
#endif

/*
 *ipi send enable adsp logger wake up flag
 */
static unsigned int adsp_A_log_wakeup_set(unsigned int enable)
{
	int ret;
	unsigned int retrytimes;

	if (adsp_A_logger_inited) {
		/*
		 *send ipi to invoke adsp logger
		 */
		adsp_register_feature(ADSP_LOGGER_FEATURE_ID);
		ret = 0;
		enable = (enable) ? 1 : 0;
		retrytimes = ADSP_IPI_RETRY_TIMES;
		do {
			ret = adsp_ipi_send(ADSP_IPI_LOGGER_WAKEUP, &enable,
					    sizeof(enable), 0, ADSP_A_ID);
			if (ret == ADSP_IPI_DONE)
				break;
			retrytimes--;
			udelay(100);
		} while (retrytimes > 0);
		/*
		 *disable/enable logger flag
		 */
		if ((ret == ADSP_IPI_DONE) && (enable == 1))
			adsp_A_logger_wakeup_ap = 1;
		else if ((ret == ADSP_IPI_DONE) && (enable == 0))
			adsp_A_logger_wakeup_ap = 0;

		adsp_deregister_feature(ADSP_LOGGER_FEATURE_ID);
		if (ret != ADSP_IPI_DONE) {
			pr_err("[ADSP] %s fail ret=%d\n", __func__, ret);
			goto error;
		}

	}

error:
	return 0;
}

/*
 * create device sysfs, adsp logger status
 */
static ssize_t adsp_A_mobile_log_show(struct device *kobj,
				      struct device_attribute *attr, char *buf)
{
	unsigned int stat;

	stat = (adsp_A_logger_inited && ADSP_A_log_ctl->enable) ? 1 : 0;

	return sprintf(buf, "[ADSP A] mobile log is %s\n",
		       (stat == 0x1) ? "enabled" : "disabled");
}

static ssize_t adsp_A_mobile_log_store(struct device *kobj,
				       struct device_attribute *attr,
				       const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&adsp_A_log_mutex);
	adsp_A_log_enable_set(enable);
	mutex_unlock(&adsp_A_log_mutex);

	return n;
}

DEVICE_ATTR(adsp_mobile_log, 0644, adsp_A_mobile_log_show,
	    adsp_A_mobile_log_store);


/*
 * create device sysfs, adsp ADB cmd to set ADSP wakeup AP flag
 */
static ssize_t adsp_A_wakeup_show(struct device *kobj,
				  struct device_attribute *attr, char *buf)
{
	unsigned int stat;

	stat = (adsp_A_logger_inited && adsp_A_logger_wakeup_ap) ? 1 : 0;

	return sprintf(buf, "[ADSP A] logger wakeup AP is %s\n",
		       (stat == 0x1) ? "enabled" : "disabled");
}

static ssize_t adsp_A_wakeup_store(struct device *kobj,
				   struct device_attribute *attr,
				   const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&adsp_A_log_mutex);
	adsp_A_log_wakeup_set(enable);
	mutex_unlock(&adsp_A_log_mutex);

	return n;
}

DEVICE_ATTR(adsp_A_logger_wakeup_AP, 0644, adsp_A_wakeup_show,
	    adsp_A_wakeup_store);

/*
 * create device sysfs, adsp last log show
 */
static ssize_t adsp_A_last_log_show(struct device *kobj,
				    struct device_attribute *attr, char *buf)
{
	adsp_A_get_last_log(adsp_A_log_buf_maxlen_last);
	return sprintf(buf, "adsp_A_log_buf_maxlen=%u, log=%s\n",
		       adsp_A_log_buf_maxlen_last, adsp_A_last_log);
}
static ssize_t adsp_A_last_log_store(struct device *kobj,
				     struct device_attribute *attr,
				     const char *buf, size_t n)
{
	adsp_A_get_last_log(adsp_A_log_buf_maxlen_last);
	return n;
}

DEVICE_ATTR(adsp_A_get_last_log, 0644, adsp_A_last_log_show,
	    adsp_A_last_log_store);

/*
 * logger UT test
 *
 */
#if ADSP_LOGGER_UT
static ssize_t adsp_A_mobile_log_UT_show(struct device *kobj,
					 struct device_attribute *attr,
					 char *buf)
{
	unsigned int w_pos, r_pos, datalen;
	char *logger_buf;
	size_t len = 1024;

	if (!adsp_A_logger_inited)
		return 0;

	datalen = 0;

	mutex_lock(&adsp_A_log_mutex);

	r_pos = ADSP_A_buf_info->r_pos;
	w_pos = ADSP_A_buf_info->w_pos;

	pr_debug("%s r_pos=%d, w_pos=%d\n", __func__, r_pos, w_pos);

	if (r_pos == w_pos)
		goto error;

	if (r_pos > w_pos)
		datalen = dram_buf_len - r_pos; /* not wrap */
	else
		datalen = w_pos - r_pos;

	if (datalen > len)
		datalen = len;

	logger_buf = ((char *)ADSP_A_log_ctl) +
		     ADSP_A_log_ctl->buff_ofs + r_pos;

	pr_debug("%s buff_ofs=%d, logger_buf=%p\n", __func__,
		ADSP_A_log_ctl->buff_ofs, logger_buf);

	len = datalen;
	/*memory copy from log buf*/
	memcpy_fromio(buf, logger_buf, len);

	r_pos += datalen;
	if (r_pos >= dram_buf_len)
		r_pos -= dram_buf_len;

	ADSP_A_buf_info->r_pos = r_pos;

error:
	mutex_unlock(&adsp_A_log_mutex);

	return len;
}

static ssize_t adsp_A_mobile_log_UT_store(struct device *kobj,
					  struct device_attribute *attr,
					  const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&adsp_A_log_mutex);
	adsp_A_log_enable_set(enable);
	mutex_unlock(&adsp_A_log_mutex);

	return n;
}

DEVICE_ATTR(adsp_A_mobile_log_UT, 0644, adsp_A_mobile_log_UT_show,
	    adsp_A_mobile_log_UT_store);
#endif

#if ADSP_TRAX
static ssize_t adsp_A_trax_show(struct device *kobj,
					 struct device_attribute *attr,
					 char *buf)
{
	pr_info("[ADSP] %s initiated=%d, done=%d, length=%d\n",
		__func__, pADSP_A_trax_ctl->initiated, pADSP_A_trax_ctl->done,
		pADSP_A_trax_ctl->length);

	if (!pADSP_A_trax_ctl->initiated || !pADSP_A_trax_ctl->done)
		return 0;

	memcpy(buf, (void *)adsp_get_reserve_mem_virt(ADSP_A_TRAX_MEM_ID),
		pADSP_A_trax_ctl->length);

	return pADSP_A_trax_ctl->length;
}

static ssize_t adsp_A_trax_store(struct device *kobj,
					  struct device_attribute *attr,
					  const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	adsp_A_trax_enable_set(enable);
	return n;
}

DEVICE_ATTR(adsp_A_trax, 0644, adsp_A_trax_show,
	    adsp_A_trax_store);
#endif

/*
 * IPI for logger init
 * @param id:   IPI id
 * @param data: IPI data
 * @param len:  IPI data length
 */
static void adsp_A_logger_init_handler(int id, void *data, unsigned int len)
{
	unsigned long flags;
	struct adsp_log_info_s *log_info = (struct adsp_log_info_s *)data;

	spin_lock_irqsave(&adsp_A_log_buf_spinlock, flags);
	/* sync adsp last log information*/
	adsp_A_log_dram_addr_last = log_info->adsp_log_dram_addr;
	adsp_A_log_buf_addr_last = log_info->adsp_log_buf_addr;
	adsp_A_log_start_addr_last = log_info->adsp_log_start_addr;
	adsp_A_log_end_addr_last = log_info->adsp_log_end_addr;
	adsp_A_log_buf_maxlen_last = log_info->adsp_log_buf_maxlen;
	adsp_log_buf_addr_last[ADSP_A_ID] = log_info->adsp_log_buf_addr;
	adsp_log_buf_maxlen_last[ADSP_A_ID] = log_info->adsp_log_buf_maxlen;
	adsp_log_start_addr_last[ADSP_A_ID] = log_info->adsp_log_start_addr;
	adsp_log_end_addr_last[ADSP_A_ID] = log_info->adsp_log_end_addr;

	spin_unlock_irqrestore(&adsp_A_log_buf_spinlock, flags);

	/*set a wq to enable adsp logger*/
	adsp_logger_notify_work[ADSP_A_ID].id = ADSP_A_ID;
	adsp_schedule_work(&adsp_logger_notify_work[ADSP_A_ID]);
}

#if ADSP_TRAX
static void adsp_A_trax_init_handler(int id, void *data, unsigned int len)
{
	/*set a wq for adsp trax init*/
	adsp_trax_init_work[ADSP_A_ID].id = ADSP_A_ID;
	adsp_schedule_work(&adsp_trax_init_work[ADSP_A_ID]);
}

static void adsp_A_trax_done_handler(int id, void *data, unsigned int len)
{
	/* sync adsp trax length information*/
	pADSP_A_trax_ctl->length = *((int *)data);
	pADSP_A_trax_ctl->done = 1;
	pr_debug("[ADSP] %s length=%d\n", __func__,
		pADSP_A_trax_ctl->length);
}
#endif


/*
 * callback function for work struct
 * notify apps to start their tasks or generate an exception according to flag
 * NOTE: this function may be blocked
 * and should not be called in interrupt context
 * @param ws:   work struct
 */
static void adsp_logger_notify_ws(struct work_struct *ws)
{
	struct adsp_work_struct *sws = container_of(ws, struct adsp_work_struct,
						    work);
	unsigned int retrytimes;
	unsigned int adsp_core_id = sws->id;
	enum adsp_ipi_status ret;
	enum adsp_ipi_id adsp_ipi_id;
	unsigned int mem_info[6];
	u64 memaddr;

	adsp_ipi_id = ADSP_IPI_LOGGER_INIT_A;

	pr_debug("[ADSP]%s id=%u\n", __func__, adsp_ipi_id);
	memaddr = adsp_get_reserve_mem_virt(ADSP_A_CORE_DUMP_MEM_ID);
	memset((void *)memaddr, 0,
			adsp_get_reserve_mem_size(ADSP_A_CORE_DUMP_MEM_ID));
	/*
	 *send ipi to invoke adsp logger
	 */
	retrytimes = ADSP_IPI_RETRY_TIMES;

	/* 0: logger buf addr, 1: logger buf size */
	/* 2: coredump buf addr, 3: coredump buf size */
	mem_info[0] = adsp_get_reserve_mem_phys(ADSP_A_LOGGER_MEM_ID);
	mem_info[1] = adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID);
	mem_info[2] = adsp_get_reserve_mem_phys(ADSP_A_CORE_DUMP_MEM_ID);
	mem_info[3] = adsp_get_reserve_mem_size(ADSP_A_CORE_DUMP_MEM_ID);
	mem_info[4] = adsp_get_reserve_mem_phys(ADSP_A_DEBUG_DUMP_MEM_ID);
	mem_info[5] = adsp_get_reserve_mem_size(ADSP_A_DEBUG_DUMP_MEM_ID);
	pr_info("[ADSP] logger addr 0x%x, size 0x%x\n",
			mem_info[0], mem_info[1]);
	pr_info("[ADSP] coredump addr 0x%x, size 0x%x\n",
			mem_info[2], mem_info[3]);
	pr_info("[ADSP] debugdump addr 0x%x, size 0x%x\n",
			mem_info[4], mem_info[5]);

	adsp_register_feature(ADSP_LOGGER_FEATURE_ID);

	do {
		ret = adsp_ipi_send(adsp_ipi_id, (void *)mem_info,
			sizeof(mem_info), 0, adsp_core_id);
		pr_debug("[ADSP]%s ipi ret=%u\n", __func__, ret);
		if (ret == ADSP_IPI_DONE)
			break;
		retrytimes--;
		udelay(2000);
	} while (retrytimes > 0);

	adsp_deregister_feature(ADSP_LOGGER_FEATURE_ID);

	if (ret != ADSP_IPI_DONE)
		pr_err("[ADSP]logger initial fail, ipi ret=%d\n", ret);

}

#if ADSP_TRAX
static void adsp_trax_init_ws(struct work_struct *ws)
{
	struct adsp_work_struct *sws = container_of(ws, struct adsp_work_struct,
						    work);
	//unsigned int magic = 0x5A5A5A5A;
	unsigned int retrytimes;
	unsigned int adsp_core_id = sws->id;
	enum adsp_ipi_status ret;
	enum adsp_ipi_id adsp_ipi_id;
	phys_addr_t reserved_phys_trax;
	unsigned int phys_u32;

	adsp_ipi_id = ADSP_IPI_TRAX_INIT_A;
	pr_debug("[ADSP]%s id=%u\n", __func__, adsp_ipi_id);
	/*
	 *send ipi to invoke adsp trax
	 */
	retrytimes = ADSP_IPI_RETRY_TIMES;
	reserved_phys_trax = adsp_get_reserve_mem_phys(ADSP_A_TRAX_MEM_ID);
	pr_info("[ADSP]reserved_phys_trax=0x%llx, size=%lu\n",
		reserved_phys_trax, sizeof(reserved_phys_trax));
	phys_u32 = (unsigned int)reserved_phys_trax;
	pr_info("[ADSP]phys_u32=0x%x, size=%lu\n", phys_u32, sizeof(phys_u32));

	adsp_register_feature(ADSP_LOGGER_FEATURE_ID);

	do {
		ret = adsp_ipi_send(adsp_ipi_id, (void *)&phys_u32,
			sizeof(phys_u32), 0, adsp_core_id);
		pr_info("[ADSP]%s ipi ret=%u\n", __func__, ret);
		if (ret == ADSP_IPI_DONE)
			break;
		retrytimes--;
		udelay(2000);
	} while (retrytimes > 0);

	adsp_deregister_feature(ADSP_LOGGER_FEATURE_ID);

	pADSP_A_trax_ctl->initiated = 1;
}
#endif

/*
 * init adsp logger dram ctrl structure
 * @return:     0: success, otherwise: fail
 */
int adsp_logger_init(phys_addr_t start, phys_addr_t limit)
{
	int last_ofs;

	/*init wait queue*/
	init_waitqueue_head(&adsp_A_logwait);
	adsp_A_logger_wakeup_ap = 0;

	/*init work queue*/
	INIT_WORK(&adsp_logger_notify_work[ADSP_A_ID].work,
		  adsp_logger_notify_ws);

	/*init dram ctrl table*/
	last_ofs = 0;
#ifdef CONFIG_ARM64
	ADSP_A_log_ctl = (struct log_ctrl_s *) start;
#else
	ADSP_A_log_ctl = (struct log_ctrl_s *)(u32)
			 start;  /* plz fix origial ptr to phys_addr flow */
#endif
	ADSP_A_log_ctl->base = PLT_LOG_ENABLE; /* magic */
	ADSP_A_log_ctl->enable = 0;
	ADSP_A_log_ctl->size = sizeof(*ADSP_A_log_ctl);

	last_ofs += ADSP_A_log_ctl->size;
	ADSP_A_log_ctl->info_ofs = last_ofs;

	last_ofs += sizeof(*ADSP_A_buf_info);
	last_ofs = ROUNDUP(last_ofs, 4);
	ADSP_A_log_ctl->buff_ofs = last_ofs;
	dram_buf_len =
		adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID) - last_ofs;
	ADSP_A_log_ctl->buff_size = dram_buf_len;
	pr_info("[ADSP] logger dram_buf_len=0x%x\n", dram_buf_len);

	ADSP_A_buf_info = (struct buffer_info_s *)
			  (((unsigned char *) ADSP_A_log_ctl) +
			  ADSP_A_log_ctl->info_ofs);
	ADSP_A_buf_info->r_pos = 0;
	ADSP_A_buf_info->w_pos = 0;

	last_ofs += ADSP_A_log_ctl->buff_size;

	if (last_ofs > limit) {
		pr_warn("[ADSP]:%s() initial fail, last_ofs=%u, limit=%u\n",
		       __func__, last_ofs,
		       (unsigned int) limit);
		goto error;
	}

	/* init last log buffer*/
	adsp_A_last_log = NULL;
	/* register logger ini IPI */
	adsp_ipi_registration(ADSP_IPI_LOGGER_INIT_A,
			      adsp_A_logger_init_handler,
			      "loggerA");
	/* register log wakeup IPI */
	adsp_ipi_registration(ADSP_IPI_LOGGER_WAKEUP,
			      adsp_logger_wakeup_handler,
			      "log wakeup");

	adsp_A_logger_inited = 1;
	return last_ofs;

error:
	adsp_A_logger_inited = 0;
	ADSP_A_log_ctl = NULL;
	ADSP_A_buf_info = NULL;
	return -1;

}

#if ADSP_TRAX
int adsp_trax_init(void)
{
	INIT_WORK(&adsp_trax_init_work[ADSP_A_ID].work,
		  adsp_trax_init_ws);

	adsp_ipi_registration(ADSP_IPI_TRAX_INIT_A, adsp_A_trax_init_handler,
			      "trax init");
	adsp_ipi_registration(ADSP_IPI_TRAX_DONE, adsp_A_trax_done_handler,
			      "trax done");
	memset(&ADSP_A_trax_ctl, 0, sizeof(ADSP_A_trax_ctl));
	pADSP_A_trax_ctl = &ADSP_A_trax_ctl;

	return 0;
}
#endif

const struct file_operations adsp_A_drv_file_ops = {
	.owner = THIS_MODULE,
	.read = adsp_A_log_if_read,
	.open = adsp_A_log_if_open,
	.poll = adsp_A_log_if_poll,
	.unlocked_ioctl = adsp_driver_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = adsp_driver_compat_ioctl,
#endif
};


/*
 * get log from adsp and optionally save it
 * NOTE: this function may be blocked
 * @param adsp_core_id:  fill adsp id to get last log
 */
void adsp_get_log(enum adsp_core_id adsp_id)
{
	pr_debug("[ADSP] %s\n", __func__);
#if 0 /* no need for hifi3 using dram */
	adsp_A_get_last_log(ADSP_AED_STR_LEN - 200);
#endif
}

/*
 * return adsp last log
 */
char *adsp_get_last_log(enum adsp_core_id id)
{
	char *last_log = NULL;

	if (id == ADSP_A_ID)
		last_log = adsp_A_last_log;

	return last_log;
}

#if ADSP_TRAX
int adsp_get_trax_initiated(void)
{
	return pADSP_A_trax_ctl->initiated;
}

int adsp_get_trax_done(void)
{
	return pADSP_A_trax_ctl->done;
}

int adsp_get_trax_length(void)
{
	return pADSP_A_trax_ctl->length;
}
#endif
