// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include "sspm_define.h"
#include "sspm_ipi.h"
#include "sspm_sysfs.h"
#include "sspm_logger.h"

#ifdef SSPM_PLT_LOGGER_BUF_LEN
/* use platform-defined buffer length */
#define BUF_LEN				SSPM_PLT_LOGGER_BUF_LEN
#else
/* otherwise use default buffer length */
#define BUF_LEN				(1 * 1024 * 1024)
#endif
#define LBUF_LEN			(4 * 1024)
#define SSPM_TIMER_TIMEOUT	(1 * HZ) /* 1 seconds*/

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

static unsigned int sspm_logger_inited;
static struct log_ctrl_s *log_ctl;
static struct buffer_info_s *buf_info, *lbuf_info;
static struct timer_list sspm_log_timer;
static DEFINE_MUTEX(sspm_log_mutex);

static inline void sspm_log_timer_add(void)
{
	if (sspm_log_timer.expires == 0) {
		sspm_log_timer.expires = jiffies + SSPM_TIMER_TIMEOUT;
		add_timer(&sspm_log_timer);
	}
}

static void sspm_log_timeout(struct timer_list *unused)
{
	if (buf_info->r_pos != buf_info->w_pos) {
		sspm_log_if_wake();
		sspm_log_timer.expires = 0;
	} else {
		sspm_log_timer_add();
	}
}

ssize_t sspm_log_read(char __user *data, size_t len)
{
	unsigned long w_pos, r_pos, datalen;
	char *buf, *tmp_buf;

	if (!sspm_logger_inited)
		return 0;

	datalen = 0;

	mutex_lock(&sspm_log_mutex);

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
			pr_debug("sspm logger: copy data failed !!!\n");

		kfree(tmp_buf);
	} else {
		pr_debug("sspm logger: create log buffer failed !!!\n");
		goto error;
	}

	r_pos += datalen;
	if (r_pos >= BUF_LEN)
		r_pos -= BUF_LEN;

	buf_info->r_pos = r_pos;

error:
	mutex_unlock(&sspm_log_mutex);

	return datalen;
}

unsigned int sspm_log_poll(void)
{
	if (!sspm_logger_inited)
		return 0;

	if (buf_info->r_pos != buf_info->w_pos)
		return POLLIN | POLLRDNORM;

	sspm_log_timer_add();

	return 0;
}

static unsigned int sspm_log_enable_set(unsigned int enable)
{
	struct plt_ipi_data_s ipi_data;
	int ret, ackdata;

	if (sspm_logger_inited) {
		ipi_data.cmd = PLT_LOG_ENABLE;
		ipi_data.u.logger.enable = enable;

		ret = sspm_ipi_send_sync(IPI_ID_PLATFORM, IPI_OPT_WAIT,
		    &ipi_data, sizeof(ipi_data) / MBOX_SLOT_SIZE, &ackdata, 1);
		if (ret != 0) {
			pr_err("SSPM: logger IPI fail ret=%d\n", ret);
			goto error;
		}

		if (enable != ackdata) {
			pr_err("SSPM: %s fail ret=%d\n", __func__, ackdata);
			goto error;
		}

		log_ctl->enable = enable;
	}

error:
	return 0;
}

static ssize_t sspm_mobile_log_show(struct device *kobj,
	struct device_attribute *attr, char *buf)
{
	unsigned int stat;

	stat = (sspm_logger_inited && log_ctl->enable) ? 1 : 0;

	return snprintf(buf, PAGE_SIZE, "SSPM mobile log is %s\n",
		(stat == 0x1) ? "enabled" : "disabled");
}

static ssize_t sspm_mobile_log_store(struct device *kobj,
	struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&sspm_log_mutex);
	sspm_log_enable_set(enable);
	mutex_unlock(&sspm_log_mutex);

	return n;
}
static DEVICE_ATTR_RW(sspm_mobile_log);

unsigned int __init sspm_logger_init(phys_addr_t start, phys_addr_t limit)
{
	unsigned int last_ofs;

	last_ofs = 0;

	log_ctl = (struct log_ctrl_s *)(uintptr_t) start;
	log_ctl->base = PLT_LOG_ENABLE; /* magic */
	log_ctl->enable = 0;
	log_ctl->size = sizeof(*log_ctl);

	last_ofs += log_ctl->size;
	log_ctl->info_ofs = last_ofs;

	last_ofs += sizeof(*buf_info);
	last_ofs = roundup(last_ofs, 4);
	log_ctl->buff_ofs = last_ofs;
	log_ctl->buff_size = BUF_LEN;

	buf_info = (struct buffer_info_s *) (((unsigned char *) log_ctl) +
						log_ctl->info_ofs);
	buf_info->r_pos = 0;
	buf_info->w_pos = 0;

	last_ofs += log_ctl->buff_size;

	if (last_ofs >= limit) {
		pr_err("SSPM:%s() initial fail, last_ofs=%u, limit=%u\n",
			__func__, last_ofs, (unsigned int) limit);
		goto error;
	}

	return last_ofs;

error:
	sspm_logger_inited = 0;
	log_ctl = NULL;
	buf_info = lbuf_info = NULL;
	return 0;
}

int __init sspm_logger_init_done(void)
{
	int ret;

	if (log_ctl) {
		ret = sspm_sysfs_create_file(&dev_attr_sspm_mobile_log);

		if (ret)
			return ret;

		timer_setup(&sspm_log_timer, &sspm_log_timeout, 0);
		sspm_log_timer.expires = 0;

		sspm_logger_inited = 1;
	}

	return 0;
}
