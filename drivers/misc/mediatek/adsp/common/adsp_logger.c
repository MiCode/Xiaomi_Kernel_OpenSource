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
#include <linux/device.h>       /* needed by device_* */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/poll.h>         /* needed by poll */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include <mt-plat/sync_write.h>
#include "adsp_helper.h"
#include "adsp_logger.h"

#define ADSP_TIMER_TIMEOUT          (1 * HZ) /* 1 seconds */
#define ROUNDUP(a, b)               (((a) + ((b)-1)) & ~((b)-1))
#define PLT_LOG_ENABLE              0x504C5402 /* magic */

static unsigned int adsp_A_logger_inited;
static struct log_ctrl_s *ADSP_A_log_ctl;
static struct buffer_info_s *ADSP_A_buf_info;
static DEFINE_MUTEX(adsp_A_log_mutex);

static void adsp_logger_init_ws(struct work_struct *ws);
static DECLARE_WORK(adsp_logger_init_work, adsp_logger_init_ws);

#if ADSP_TRAX
struct trax_ctrl_s *pADSP_A_trax_ctl, ADSP_A_trax_ctl;

static void adsp_trax_init_ws(struct work_struct *ws);
static DECLARE_WORK(adsp_trax_init_work, adsp_trax_init_ws);
#endif

/*adsp last log info*/
static unsigned int dram_buf_len;

ssize_t adsp_A_log_read(char __user *data, size_t len)
{
	unsigned int w_pos, r_pos, datalen = 0;
	char *buf, *to_user_buf;

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
	if (r_pos >= dram_buf_len) {
		pr_info("[ADSP] %s(): r_pos >= dram_buf_len,%x,%x\n", __func__,
			r_pos, ADSP_A_log_ctl->buff_ofs);
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

ssize_t adsp_A_log_if_read(struct file *file, char __user *data,
				  size_t len, loff_t *ppos)
{
	if (!adsp_A_logger_inited)
		return 0;

	return adsp_A_log_read(data, len);
}

int adsp_A_log_if_open(struct inode *inode, struct file *file)
{
	/* pr_debug("[ADSP A] adsp_A_log_if_open\n"); */
	return nonseekable_open(inode, file);
}

unsigned int adsp_A_log_if_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	if (!adsp_A_logger_inited)
		return 0;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	ret = adsp_A_log_poll();

	return ret;
}
/*
 * ipi send to enable adsp logger flag
 */
static unsigned int adsp_A_log_enable_set(unsigned int enable)
{
	int ret = 0;

	if (adsp_A_logger_inited) {
		enable = (enable) ? 1 : 0;

		adsp_register_feature(ADSP_LOGGER_FEATURE_ID);

		ret = adsp_ipi_send(ADSP_IPI_LOGGER_ENABLE, &enable,
				    sizeof(enable), 1, ADSP_A_ID);

		adsp_deregister_feature(ADSP_LOGGER_FEATURE_ID);

		if (ret != ADSP_IPI_DONE) {
			pr_err("%s(), logger enable fail ret=%d\n",
			       __func__, ret);
			goto error;
		}

		ADSP_A_log_ctl->enable = enable;
	}

error:
	return 0;
}

#if ADSP_TRAX
static unsigned int adsp_A_trax_enable_set(unsigned int enable)
{
	int ret = 0;

	if (pADSP_A_trax_ctl->initiated) {
		enable = (enable) ? 1 : 0;

		adsp_register_feature(ADSP_LOGGER_FEATURE_ID);

		ret = adsp_ipi_send(ADSP_IPI_TRAX_ENABLE, &enable,
				    sizeof(enable), 1, ADSP_A_ID);

		adsp_deregister_feature(ADSP_LOGGER_FEATURE_ID);

		if (ret != ADSP_IPI_DONE) {
			pr_err("%s(), trax enable fail ret=%d\n",
			       __func__, ret);
			goto error;
		}

		pADSP_A_trax_ctl->enable = enable;

		if (enable)
			pADSP_A_trax_ctl->done = 0;
	}

error:
	return 0;
}
#endif

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
DEVICE_ATTR_RW(adsp_A_mobile_log);

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
DEVICE_ATTR_RW(adsp_A_mobile_log_UT);
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

	memcpy(buf, adsp_get_reserve_mem_virt(ADSP_A_TRAX_MEM_ID),
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
DEVICE_ATTR_RW(adsp_A_trax);
#endif

static struct attribute *adsp_logger_attrs[] = {
	&dev_attr_adsp_A_mobile_log.attr,
#if ADSP_LOGGER_UT
	&dev_attr_adsp_A_mobile_log_UT.attr,
#endif
#if ADSP_TRAX
	&dev_attr_adsp_A_trax.attr,
#endif
	NULL,
};

struct attribute_group adsp_logger_attr_group = {
	.attrs = adsp_logger_attrs,
};

/*
 * IPI for logger init
 * @param id:   IPI id
 * @param data: IPI data
 * @param len:  IPI data length
 */
static void adsp_A_logger_init_handler(int id, void *data, unsigned int len)
{
	/* send work to initialize logger*/
	queue_work(adsp_workqueue, &adsp_logger_init_work);
}

#if ADSP_TRAX
static void adsp_A_trax_init_handler(int id, void *data, unsigned int len)
{
	/*send work to initialize trax */
	queue_work(adsp_workqueue, &adsp_trax_init_work);
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
static void adsp_logger_init_ws(struct work_struct *ws)
{
	enum adsp_ipi_status ret;
	unsigned int mem_info[6];

	memset(adsp_get_reserve_mem_virt(ADSP_A_CORE_DUMP_MEM_ID), 0,
		adsp_get_reserve_mem_size(ADSP_A_CORE_DUMP_MEM_ID));

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

	ret = adsp_ipi_send(ADSP_IPI_LOGGER_INIT_A, (void *)mem_info,
		sizeof(mem_info), 1, ADSP_A_ID);

	adsp_deregister_feature(ADSP_LOGGER_FEATURE_ID);

	if (ret != ADSP_IPI_DONE)
		pr_err("[ADSP]logger initial fail, ipi ret=%d\n", ret);

}

#if ADSP_TRAX
static void adsp_trax_init_ws(struct work_struct *ws)
{
	enum adsp_ipi_status ret;
	phys_addr_t reserved_phys_trax;
	unsigned int phys_u32;

	reserved_phys_trax = adsp_get_reserve_mem_phys(ADSP_A_TRAX_MEM_ID);
	pr_info("[ADSP]reserved_phys_trax=0x%llx, size=%lu\n",
		reserved_phys_trax, sizeof(reserved_phys_trax));
	phys_u32 = (unsigned int)reserved_phys_trax;
	pr_info("[ADSP]phys_u32=0x%x, size=%lu\n", phys_u32, sizeof(phys_u32));

	adsp_register_feature(ADSP_LOGGER_FEATURE_ID);

	ret = adsp_ipi_send(ADSP_IPI_TRAX_INIT_A, (void *)&phys_u32,
		sizeof(phys_u32), 1, ADSP_A_ID);

	adsp_deregister_feature(ADSP_LOGGER_FEATURE_ID);

	if (ret != ADSP_IPI_DONE)
		pr_err("[ADSP]trax initial fail, ipi ret=%d\n", ret);

	pADSP_A_trax_ctl->initiated = 1;
}
#endif

/*
 * init adsp logger dram ctrl structure
 * @return:     0: success, otherwise: fail
 */
int adsp_logger_init(void)
{
	int last_ofs;
	void *addr = adsp_get_reserve_mem_virt(ADSP_A_LOGGER_MEM_ID);
	size_t size = adsp_get_reserve_mem_size(ADSP_A_LOGGER_MEM_ID);

	/*init dram ctrl table*/
	last_ofs = 0;

	ADSP_A_log_ctl = (struct log_ctrl_s *)addr;
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

	if (last_ofs > size) {
		pr_warn("[ADSP]:%s() initial fail, last_ofs=%u, size=%zu\n",
		       __func__, last_ofs, size);
		goto error;
	}

	/* register logger ini IPI */
	adsp_ipi_registration(ADSP_IPI_LOGGER_INIT_A,
			      adsp_A_logger_init_handler,
			      "loggerA");

	adsp_A_logger_inited = 1;
	return 0;

error:
	adsp_A_logger_inited = 0;
	ADSP_A_log_ctl = NULL;
	ADSP_A_buf_info = NULL;
	return -1;

}

#if ADSP_TRAX
int adsp_trax_init(void)
{
	adsp_ipi_registration(ADSP_IPI_TRAX_INIT_A, adsp_A_trax_init_handler,
			      "trax init");
	adsp_ipi_registration(ADSP_IPI_TRAX_DONE, adsp_A_trax_done_handler,
			      "trax done");
	memset(&ADSP_A_trax_ctl, 0, sizeof(ADSP_A_trax_ctl));
	       pADSP_A_trax_ctl = &ADSP_A_trax_ctl;

	return 0;
}
#endif

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
