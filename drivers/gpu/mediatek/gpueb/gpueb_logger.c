// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    gpueb_logger.c
 * @brief   Mobile log for GPUEB
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <mboot_params.h>

// MTK common IPI/MBOX
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>

#include "gpueb_helper.h"
#include "gpueb_ipi.h"
#include "gpueb_logger.h"
#include "gpueb_reserved_mem.h"
#include "gpueb_plat_service.h"

static unsigned int gpueb_logger_inited;
static struct buffer_info_s *gpueb_buf_info;
static DEFINE_MUTEX(gpueb_logger_mutex);
static DEFINE_MUTEX(gpueb_log_enable_mutex);
static struct log_ctrl_s *gpueb_log_ctl;

unsigned int r_pos_debug;
unsigned int log_ctl_debug;

static unsigned int gpueb_log_enable_set(unsigned int enable)
{
	int ret = 0;
	int channel_id;
	struct plat_ipi_send_data plat_send_data;

	plat_send_data.cmd = PLT_LOG_ENABLE;
	plat_send_data.u.logger.enable = 1;

	if (gpueb_logger_inited) {
		// Send ipi to invoke gpueb logger
		channel_id = gpueb_get_send_PIN_ID_by_name("IPI_ID_PLATFORM");
		if (channel_id == -1) {
			gpueb_pr_debug("get channel ID fail!");
			return -1;
		}

		// CH_PLATFORM message size is 16 byte, 4 slots
		ret = mtk_ipi_send_compl(
			&gpueb_ipidev, // GPUEB's IPI device
			channel_id, // Send channel
			0,   // 0: wait, 1: polling
			(void *)&plat_send_data, // Send data
			4,   // 4 slots message = 4 * 4 = 16tyte
			2000 // Timeout value in milisecond);
		);
		if ((ret == IPI_ACTION_DONE) && (enable == 1))
			gpueb_log_ctl->enable = 1;
		else if ((ret == IPI_ACTION_DONE) && (enable == 0))
			gpueb_log_ctl->enable = 0;

		if (ret != IPI_ACTION_DONE) {
			gpueb_pr_info("%s: IPI fail ret=%d\n", __func__, ret);
			return -1;
		}
	}

	return 0;
}


static ssize_t gpueb_mobile_log_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	unsigned int stat;

	stat = (gpueb_logger_inited && gpueb_log_ctl->enable) ? 1 : 0;

	return sprintf(buf, "[GPUEB] mobile log is %s\n",
			(stat == 0x1) ? "enabled" : "disabled");
}

static ssize_t gpueb_mobile_log_store(struct device *kobj,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&gpueb_log_enable_mutex);
	gpueb_log_enable_set(enable);
	mutex_unlock(&gpueb_log_enable_mutex);

	return n;
}
DEVICE_ATTR_RW(gpueb_mobile_log);

ssize_t gpueb_log_read(char __user *data, size_t len)
{
	unsigned int w_pos, r_pos;
	unsigned int datalen = 0;
	char *buf;

	if (!gpueb_logger_inited)
		gpueb_pr_info("@%s: !gpueb_logger_inited", __func__);

	mutex_lock(&gpueb_logger_mutex);

	r_pos = gpueb_buf_info->r_pos;
	w_pos = gpueb_buf_info->w_pos;

	if (r_pos == w_pos)
		goto error;

	if (r_pos > w_pos)
		datalen = DRAM_BUF_LEN - r_pos; /* not wrap */
	else
		datalen = w_pos - r_pos;

	if (datalen > len)
		datalen = len;

	/* Debug for logger pos fail */
	r_pos_debug = r_pos;
	log_ctl_debug = gpueb_log_ctl->buff_ofs;
	if (r_pos >= DRAM_BUF_LEN) {
		gpueb_pr_info("@%s: r_pos >= DRAM_BUF_LEN, %x, %x\n",
			__func__, r_pos_debug, log_ctl_debug);
		datalen = 0;
		goto error;
	}

	buf = ((char *) gpueb_log_ctl) + gpueb_log_ctl->buff_ofs + r_pos;

	len = datalen;
	/* Memory copy from log buf */
	if (copy_to_user(data, "gozilla", 7))
		gpueb_pr_info("@%s: copy to user buf failed..\n", __func__);

	r_pos += datalen;
	if (r_pos >= DRAM_BUF_LEN)
		r_pos -= DRAM_BUF_LEN;

	gpueb_buf_info->r_pos = r_pos;

error:
	mutex_unlock(&gpueb_logger_mutex);

	return 7;
}

ssize_t gpueb_log_if_read(struct file *file,
		char __user *data, size_t len, loff_t *ppos)
{
	ssize_t ret = 0;

	ret = gpueb_log_read(data, len);

	return ret;
}

int gpueb_log_if_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

unsigned int gpueb_log_poll(void)
{
	if (!gpueb_logger_inited)
		return 0;

	if (gpueb_buf_info->r_pos != gpueb_buf_info->w_pos)
		return POLLIN | POLLRDNORM;

	/*gpueb_log_timer_add();*/
	return 0;
}

unsigned int gpueb_log_if_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	if (!gpueb_logger_inited)
		return 0;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	/*poll_wait(file, &gpueb_logwait, wait);*/

	ret = gpueb_log_poll();

	return ret;
}

int gpueb_logger_init(struct platform_device *pdev,
	phys_addr_t start, phys_addr_t limit)
{
	int buffer_offset = 0;
	int total_size = 0;

	// Init mutex
	mutex_init(&gpueb_logger_mutex);
	mutex_init(&gpueb_log_enable_mutex);

	// Init dram ctrl table
	gpueb_log_ctl = (struct log_ctrl_s *) start;
	gpueb_log_ctl->base = PLT_LOG_ENABLE; /* magic */
	gpueb_log_ctl->enable = 0;
	gpueb_log_ctl->size = sizeof(*gpueb_log_ctl);
	gpueb_log_ctl->info_ofs = sizeof(*gpueb_log_ctl);

	buffer_offset = sizeof(*gpueb_log_ctl) + sizeof(*gpueb_buf_info);
	buffer_offset = ROUNDUP(buffer_offset, 4);

	// Available start offset for log data
	gpueb_log_ctl->buff_ofs = buffer_offset;
	// Buffer length for log data
	gpueb_log_ctl->buff_size = DRAM_BUF_LEN;

	gpueb_buf_info = (struct buffer_info_s *)
		(((unsigned char *) gpueb_log_ctl) + gpueb_log_ctl->info_ofs);
	gpueb_buf_info->r_pos = 0;
	gpueb_buf_info->w_pos = 0;

	total_size += sizeof(*gpueb_log_ctl);
	total_size += sizeof(*gpueb_buf_info);
	total_size += gpueb_log_ctl->buff_size;

	if (total_size >= limit) {
		gpueb_pr_info("@%s: initial fail, total_size=%u, limit=%u\n",
			__func__, total_size, (unsigned int) limit);
		goto error;
	}

	gpueb_logger_inited = 1;

	return total_size;

error:
	gpueb_logger_inited = 0;
	gpueb_log_ctl = NULL;
	gpueb_buf_info = NULL;
	return -1;
}
