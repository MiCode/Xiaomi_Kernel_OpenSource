// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/* MCUPM LOGGER */
#include <linux/io.h>
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/timer.h>
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/poll.h>


#include "mcupm_driver.h"
#include "mcupm_ipi_id.h"

extern int mcupm_plt_ackdata;

static wait_queue_head_t logwait;

static unsigned int mcupm_logger_inited;
static struct log_ctrl_s *log_ctl;
static struct buffer_info_s *buf_info, *lbuf_info;
static struct timer_list mcupm_log_timer;
static DEFINE_MUTEX(mcupm_log_mutex);


static ssize_t mcupm_log_if_read(struct file *file, char __user *data,
				 size_t len, loff_t *ppos)
{
	ssize_t ret;

	/* pr_debug("[MCUPM] mcupm_log_if_read\n"); */

	ret = 0;

	if (access_ok(data, len))
		ret = mcupm_log_read(data, len);

	return ret;
}

static int mcupm_log_if_open(struct inode *inode, struct file *file)
{
	/* pr_debug("[MCUPM] mcupm_log_if_open\n"); */
	return nonseekable_open(inode, file);
}

static unsigned int mcupm_log_if_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	/* pr_debug("[MCUPM] mcupm_log_if_poll\n"); */

	if (!(file->f_mode & FMODE_READ))
		return ret;

	poll_wait(file, &logwait, wait);

	ret = mcupm_log_poll();

	return ret;
}

void mcupm_log_if_wake(void)
{
	wake_up(&logwait);
}


static inline void mcupm_log_timer_add(void)
{
	if (mcupm_log_timer.expires == 0) {
		mcupm_log_timer.expires = jiffies + MCUPM_TIMER_TIMEOUT;
		add_timer(&mcupm_log_timer);
	}
}

static void mcupm_log_timeout(struct timer_list *timer)
{
	if (buf_info->r_pos != buf_info->w_pos) {
		mcupm_log_if_wake();
		mcupm_log_timer.expires = 0;
	} else {
		mcupm_log_timer_add();
	}
}

ssize_t mcupm_log_read(char __user *data, size_t len)
{
	unsigned long w_pos, r_pos, datalen;
	char *buf, *tmp_buf;

	if (!mcupm_logger_inited)
		return 0;

	datalen = 0;

	mutex_lock(&mcupm_log_mutex);

	r_pos = buf_info->r_pos;
	w_pos = buf_info->w_pos;

	if (r_pos == w_pos)
		goto error;

	if (r_pos > w_pos)
		datalen = BUF_LEN - r_pos; /* not wrap */
	else
		datalen = w_pos - r_pos;

	if (datalen > len)
		datalen = len;

	buf = ((char *) log_ctl) + log_ctl->buff_ofs + r_pos;
	tmp_buf = kmalloc((size_t)len, GFP_KERNEL);
	len = datalen;

	if (tmp_buf) {
		memcpy_fromio(tmp_buf, buf, len);
		if (copy_to_user(data, tmp_buf, len))

			pr_debug("mcupm logger: copy data failed !!!\n");

		kfree(tmp_buf);
	} else {
		pr_debug("mcupm logger: create log buffer failed !!!\n");
		goto error;
	}

	r_pos += datalen;
	if (r_pos >= BUF_LEN)
		r_pos -= BUF_LEN;

	buf_info->r_pos = r_pos;

error:
	mutex_unlock(&mcupm_log_mutex);

	return datalen;
}

unsigned int mcupm_log_poll(void)
{
	if (!mcupm_logger_inited)
		return 0;

	if (buf_info->r_pos != buf_info->w_pos)
		return POLLIN | POLLRDNORM;

	mcupm_log_timer_add();

	return 0;
}

static const struct file_operations mcupm_log_file_ops = {
	.owner = THIS_MODULE,
	.read = mcupm_log_if_read,
	.open = mcupm_log_if_open,
	.poll = mcupm_log_if_poll,
};

static struct miscdevice mcupm_log_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mcupm",
	.fops = &mcupm_log_file_ops
};

int mcupm_sysfs_create_file(struct device_attribute *attr)
{
	return device_create_file(mcupm_log_device.this_device, attr);
}
int mcupm_sysfs_init(void)
{
	int ret;

	init_waitqueue_head(&logwait);

	ret = misc_register(&mcupm_log_device);

	if (unlikely(ret != 0))
		return ret;

	return 0;
}

static unsigned int mcupm_log_enable_set(unsigned int enable)
{
	struct mcupm_ipi_data_s ipi_data;
	int ret = 0;

	if (mcupm_logger_inited) {
		ipi_data.cmd = MCUPM_PLT_LOG_ENABLE;
		ipi_data.u.logger.enable = enable ? ENABLE : DISABLE;
		mcupm_plt_ackdata = -1;

		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_PLATFORM,
			IPI_SEND_WAIT, &ipi_data,
			sizeof(struct mcupm_ipi_data_s) / MCUPM_MBOX_SLOT_SIZE,
			2000);

		if (ret) {
			pr_debug("MCUPM: logger IPI fail ret=%d\n", ret);
			goto error;
		}

		if (enable != mcupm_plt_ackdata) {
			pr_debug("MCUPM: %s fail enable=%d ackdata=%d\n",
				__func__, enable, mcupm_plt_ackdata);
			goto error;
		}

		log_ctl->enable = enable;
		pr_info("MCUPM: logger IPI success ret=%d, ackdata = %d\n",
			ret, mcupm_plt_ackdata);
	}

error:
	return 0;
}

static ssize_t mcupm_mobile_log_show(struct device *kobj,
	struct device_attribute *attr, char *buf)
{
	unsigned int stat;

	stat = (mcupm_logger_inited && log_ctl->enable) ? 1 : 0;

	return snprintf(buf, PAGE_SIZE, "MCUPM mobile log is %s\n",
		(stat == 0x1) ? "enabled" : "disabled");
}

static ssize_t mcupm_mobile_log_store(struct device *kobj,
	struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable = 0;


	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&mcupm_log_mutex);
	mcupm_log_enable_set(enable);
	mutex_unlock(&mcupm_log_mutex);

	return n;
}

DEVICE_ATTR_RW(mcupm_mobile_log);

unsigned int mcupm_logger_init(phys_addr_t start, phys_addr_t limit)
{
	unsigned int last_ofs;

	last_ofs = 0;

	log_ctl = (struct log_ctrl_s *)(uintptr_t) start;
	log_ctl->base = MCUPM_PLT_LOG_ENABLE; /* magic */
	log_ctl->enable = 0;
	log_ctl->size = sizeof(*log_ctl);

	last_ofs += log_ctl->size;
	log_ctl->info_ofs = last_ofs;

	last_ofs += sizeof(*buf_info);
	last_ofs = ROUNDUP(last_ofs, 4);
	log_ctl->buff_ofs = last_ofs;
	log_ctl->buff_size = BUF_LEN;

	buf_info = (struct buffer_info_s *) (((unsigned char *) log_ctl) +
					     log_ctl->info_ofs);
	buf_info->r_pos = 0;
	buf_info->w_pos = 0;

	last_ofs += log_ctl->buff_size;

	if (last_ofs >= limit) {
		pr_debug("MCUPM:%s() initial fail, last_ofs=%u, limit=%u\n",
		       __func__, last_ofs, (unsigned int) limit);
		goto error;
	}

	return last_ofs;

error:
	mcupm_logger_inited = 0;
	log_ctl = NULL;
	buf_info = lbuf_info = NULL;
	return 0;
}

int mcupm_logger_init_done(void)
{
	int ret;

	if (log_ctl) {
		ret = mcupm_sysfs_create_file(&dev_attr_mcupm_mobile_log);

		if (unlikely(ret != 0))
			return ret;

		timer_setup(&mcupm_log_timer, &mcupm_log_timeout, 0);
		mcupm_log_timer.expires = 0;

		mcupm_logger_inited = 1;
	}

	return 0;
}

