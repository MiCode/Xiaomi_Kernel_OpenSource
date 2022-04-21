// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
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
//#include <mt-plat/sync_write.h>
#include "vcp_helper.h"
#include "vcp_ipi_pin.h"
#include "vcp_mbox_layout.h"
#include "vcp.h"

#define DRAM_BUF_LEN			(1 * 1024 * 1024)
#define VCP_TIMER_TIMEOUT	        (1 * HZ) /* 1 seconds*/
#define ROUNDUP(a, b)		        (((a) + ((b)-1)) & ~((b)-1))
#define PLT_LOG_ENABLE              0x504C5402 /*magic*/
#define VCP_IPI_RETRY_TIMES         (100)

/* bit0 = 1, logger is on, else off*/
#define VCP_LOGGER_ON_BIT       (1<<0)
/* bit1 = 1, logger_dram_use is on, else off*/
#define VCP_LOGGER_DRAM_ON_BIT  (1<<1)
/* bit8 = 1, enable function (logger/logger dram use) */
#define VCP_LOGGER_ON_CTRL_BIT    (1<<8)
/* bit8 = 0, disable function */
#define VCP_LOGGER_OFF_CTRL_BIT		(0<<8)
/* let logger on */
#define VCP_LOGGER_ON       (VCP_LOGGER_ON_CTRL_BIT | VCP_LOGGER_ON_BIT)
/* let logger off */
#define VCP_LOGGER_OFF      (VCP_LOGGER_OFF_CTRL_BIT | VCP_LOGGER_ON_BIT)
/* let logger dram use on */
#define VCP_LOGGER_DRAM_ON  (VCP_LOGGER_ON_CTRL_BIT | VCP_LOGGER_DRAM_ON_BIT)
/* let logger dram use off */
#define VCP_LOGGER_DRAM_OFF (VCP_LOGGER_OFF_CTRL_BIT | VCP_LOGGER_DRAM_ON_BIT)
#define VCP_LOGGER_UT (1)

#undef pr_debug
#define pr_debug(fmt, arg...) do { \
		if (vcp_dbg_log) \
			pr_info(fmt, ##arg); \
	} while (0)

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

struct VCP_LOG_INFO {
	uint32_t vcp_log_dram_addr;
	uint32_t vcp_log_buf_addr;
	uint32_t vcp_log_start_addr;
	uint32_t vcp_log_end_addr;
	uint32_t vcp_log_buf_maxlen;
};

struct vcp_logger_ctrl_msg {
	unsigned int cmd;
	union {
		struct {
			unsigned int addr;
			unsigned int size;
			unsigned int ap_time_h;
			unsigned int ap_time_l;
		} init;
		struct {
			unsigned int enable;
		} flag;
		struct VCP_LOG_INFO info;
	} u;
};

#define VCP_LOGGER_IPI_INIT       0x4C4F4701
#define VCP_LOGGER_IPI_ENABLE     0x4C4F4702
#define VCP_LOGGER_IPI_WAKEUP     0x4C4F4703
#define VCP_LOGGER_IPI_SET_FILTER 0x4C4F4704

static unsigned int vcp_A_logger_inited;
static unsigned int vcp_A_logger_wakeup_ap;

static struct log_ctrl_s *VCP_A_log_ctl;
static struct buffer_info_s *VCP_A_buf_info;
/*static struct timer_list vcp_log_timer;*/
static DEFINE_MUTEX(vcp_A_log_mutex);
static DEFINE_SPINLOCK(vcp_A_log_buf_spinlock);
static struct vcp_work_struct vcp_logger_notify_work[VCP_CORE_TOTAL];

/*vcp last log info*/
#define LAST_LOG_BUF_SIZE  4095
static struct VCP_LOG_INFO last_log_info;

static char *vcp_A_last_log;
static wait_queue_head_t vcp_A_logwait;

static DEFINE_MUTEX(vcp_logger_mutex);
static char *vcp_last_logger;
/*global value*/
unsigned int r_pos_debug;
unsigned int log_ctl_debug;
static struct mutex vcp_logger_mutex;

/* ipi message buffer */
struct vcp_logger_ctrl_msg msg_vcp_logger_ctrl;

/*
 * get log from vcp when received a buf full notify
 * @param id:   IPI id
 * @param prdata: IPI handler parameter
 * @param data: IPI data
 * @param len:  IPI data length
 */
static int vcp_logger_wakeup_handler(unsigned int id, void *prdata, void *data,
				      unsigned int len)
{
	pr_debug("[VCP]wakeup by VCP logger\n");

	return 0;
}

/*
 * get log from vcp to last_log_buf
 * @param len:  data length
 * @return:     length of log
 */
static size_t vcp_A_get_last_log(size_t b_len)
{
	size_t ret = 0;
	int vcp_awake_flag;
	unsigned int log_end_idx;
	unsigned int update_start_idx;
	unsigned char *vcp_last_log_buf =
		(unsigned char *)(VCP_TCM + last_log_info.vcp_log_buf_addr);

	/*pr_debug("[VCP] %s\n", __func__);*/

	if (!vcp_A_logger_inited) {
		pr_notice("[VCP] %s(): logger has not been init\n", __func__);
		return 0;
	}

	mutex_lock(&vcp_logger_mutex);

	/* VCP keep awake */
	vcp_awake_flag = 0;
	if (vcp_awake_lock((void *)VCP_A_ID) == -1) {
		vcp_awake_flag = -1;
		pr_debug("[VCP] %s: awake vcp fail\n", __func__);
	}
	/*cofirm last log information is less than tcm size*/
	if (last_log_info.vcp_log_end_addr > vcpreg.total_tcmsize) {
		pr_notice("[VCP] %s: last_log_info.vcp_log_end_addr %x is over tcm_size %x\n",
			__func__, last_log_info.vcp_log_end_addr, vcpreg.total_tcmsize);
		goto exit;
	}
	if (last_log_info.vcp_log_buf_addr + last_log_info.vcp_log_buf_maxlen >
		vcpreg.total_tcmsize) {
		pr_debug("[VCP] %s: end of last_log_info.vcp_last_log_buf %x is over tcm_size %x\n",
			__func__, last_log_info.vcp_log_buf_addr + last_log_info.vcp_log_buf_maxlen,
				vcpreg.total_tcmsize);
		goto exit;
	}

	log_end_idx = readl((void __iomem *)(VCP_TCM +
					last_log_info.vcp_log_end_addr));

	if (b_len > last_log_info.vcp_log_buf_maxlen) {
		pr_debug("[VCP] b_len %zu > vcp_log_buf_maxlen %d\n",
			b_len, last_log_info.vcp_log_buf_maxlen);
		b_len = last_log_info.vcp_log_buf_maxlen;
	}
	/* handle sram error */
	if (log_end_idx >= last_log_info.vcp_log_buf_maxlen)
		log_end_idx = 0;

	if (log_end_idx >= b_len)
		update_start_idx = log_end_idx - b_len;
	else
		update_start_idx = last_log_info.vcp_log_buf_maxlen -
					(b_len - log_end_idx) + 1;

	/* read log from vcp buffer */
	ret = 0;
	if (vcp_A_last_log) {
		while ((update_start_idx != log_end_idx) && ret < b_len) {
			vcp_A_last_log[ret] =
				vcp_last_log_buf[update_start_idx];
			update_start_idx++;
			ret++;
			if (update_start_idx >=
				last_log_info.vcp_log_buf_maxlen)
				update_start_idx = update_start_idx -
					last_log_info.vcp_log_buf_maxlen;

			vcp_A_last_log[ret] = '\0';
		}
	} else {
		/* no buffer, just skip logs*/
		update_start_idx = log_end_idx;
	}
exit:
	/*VCP release awake */
	if (vcp_awake_flag == 0) {
		if (vcp_awake_unlock((void *)VCP_A_ID) == -1)
			pr_debug("[VCP] %s: awake unlock fail\n", __func__);
	}

	mutex_unlock(&vcp_logger_mutex);
	return ret;
}

ssize_t vcp_A_log_read(char __user *data, size_t len)
{
	unsigned int w_pos, r_pos, datalen;
	char *buf;

	if (!vcp_A_logger_inited)
		return 0;

	datalen = 0;

	mutex_lock(&vcp_A_log_mutex);

	r_pos = VCP_A_buf_info->r_pos;
	w_pos = VCP_A_buf_info->w_pos;

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
	log_ctl_debug = VCP_A_log_ctl->buff_ofs;
	if (r_pos >= DRAM_BUF_LEN) {
		pr_notice("[VCP] %s(): r_pos >= DRAM_BUF_LEN,%x,%x\n",
			__func__, r_pos_debug, log_ctl_debug);
		datalen = 0;
		goto error;
	}

	buf = ((char *) VCP_A_log_ctl) + VCP_A_log_ctl->buff_ofs + r_pos;

	len = datalen;
	/*memory copy from log buf*/
	if (copy_to_user(data, buf, len))
		pr_debug("[VCP]copy to user buf failed..\n");

	r_pos += datalen;
	if (r_pos >= DRAM_BUF_LEN)
		r_pos -= DRAM_BUF_LEN;

	VCP_A_buf_info->r_pos = r_pos;

error:
	mutex_unlock(&vcp_A_log_mutex);

	return datalen;
}

unsigned int vcp_A_log_poll(void)
{
	if (!vcp_A_logger_inited)
		return 0;

	if (VCP_A_buf_info->r_pos != VCP_A_buf_info->w_pos)
		return POLLIN | POLLRDNORM;

	/*vcp_log_timer_add();*/

	return 0;
}


static ssize_t vcp_A_log_if_read(struct file *file,
		char __user *data, size_t len, loff_t *ppos)
{
	ssize_t ret;

	/*pr_debug("[VCP A] vcp_A_log_if_read\n");*/

	ret = 0;

	/*if (access_ok(VERIFY_WRITE, data, len))*/
		ret = vcp_A_log_read(data, len);

	return ret;
}

static int vcp_A_log_if_open(struct inode *inode, struct file *file)
{
	/*pr_debug("[VCP A] vcp_A_log_if_open\n");*/
	return nonseekable_open(inode, file);
}

static unsigned int vcp_A_log_if_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	/* pr_debug("[VCP A] vcp_A_log_if_poll\n"); */
	if (!vcp_A_logger_inited)
		return 0;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	/*poll_wait(file, &vcp_A_logwait, wait);*/

	ret = vcp_A_log_poll();

	return ret;
}
/*
 * ipi send to enable vcp logger flag
 */
static unsigned int vcp_A_log_enable_set(unsigned int enable)
{
	int ret;
	unsigned int retrytimes;
	struct vcp_logger_ctrl_msg msg;

	if (vcp_A_logger_inited) {
		/*
		 *send ipi to invoke vcp logger
		 */
		ret = 0;
		enable = (enable) ? VCP_LOGGER_ON : VCP_LOGGER_OFF;
		retrytimes = VCP_IPI_RETRY_TIMES;
		do {
			msg.cmd = VCP_LOGGER_IPI_ENABLE;
			msg.u.flag.enable = enable;
			if (is_vcp_ready(VCP_A_ID)) {
				ret = mtk_ipi_send(&vcp_ipidev, IPI_OUT_LOGGER_CTRL,
					0, &msg, sizeof(msg)/MBOX_SLOT_SIZE, 0);

				if (ret == IPI_ACTION_DONE)
					break;
			}
			retrytimes--;
			udelay(1000);
		} while (retrytimes > 0 && vcp_A_logger_inited);
		/*
		 *disable/enable logger flag
		 */
		if ((ret == IPI_ACTION_DONE) && (enable == VCP_LOGGER_ON)) {
			VCP_A_log_ctl->enable = 1;
			pr_notice("[VCP] %s: turn on logger\n", __func__);
		} else if ((ret == IPI_ACTION_DONE) && (enable == VCP_LOGGER_OFF)) {
			VCP_A_log_ctl->enable = 0;
			pr_notice("[VCP] %s: turn off logger or default off\n", __func__);
		}

		if (ret != IPI_ACTION_DONE) {
			pr_notice("[VCP] %s: fail ret=%d\n", __func__, ret);
			goto error;
		}

	} else if (driver_init_done) {
		enable = (enable) ? VCP_LOGGER_ON : VCP_LOGGER_OFF;
		if (enable == VCP_LOGGER_ON)
			VCP_A_log_ctl->enable = 1;
		else if (enable == VCP_LOGGER_OFF)
			VCP_A_log_ctl->enable = 0;
	}

error:
	return 0;
}

/*
 *ipi send enable vcp logger wake up flag
 */
static unsigned int vcp_A_log_wakeup_set(unsigned int enable)
{
	int ret;
	unsigned int retrytimes;
	struct vcp_logger_ctrl_msg msg;

	if (vcp_A_logger_inited) {
		/*
		 *send ipi to invoke vcp logger
		 */
		ret = 0;
		enable = (enable) ? 1 : 0;
		retrytimes = VCP_IPI_RETRY_TIMES;
		do {
			msg.cmd = VCP_LOGGER_IPI_WAKEUP;
			msg.u.flag.enable = enable;
			if (is_vcp_ready(VCP_A_ID)) {
				ret = mtk_ipi_send(&vcp_ipidev, IPI_OUT_LOGGER_CTRL,
					0, &msg, sizeof(msg)/MBOX_SLOT_SIZE, 0);

				if (ret == IPI_ACTION_DONE)
					break;
			}
			retrytimes--;
			udelay(1000);
		} while (retrytimes > 0 && vcp_A_logger_inited);
		/*
		 *disable/enable logger flag
		 */
		if ((ret == IPI_ACTION_DONE) && (enable == 1))
			vcp_A_logger_wakeup_ap = 1;
		else if ((ret == IPI_ACTION_DONE) && (enable == 0))
			vcp_A_logger_wakeup_ap = 0;

		if (ret != IPI_ACTION_DONE) {
			pr_notice("[VCP] %s: fail ret=%d\n", __func__, ret);
			goto error;
		}

	}

error:
	return 0;
}

/*
 * create device sysfs, vcp logger status
 */
static ssize_t vcp_mobile_log_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	unsigned int stat;

	stat = (vcp_A_logger_inited && VCP_A_log_ctl->enable) ? 1 : 0;

	return sprintf(buf, "[VCP A] mobile log is %s\n",
			(stat == 0x1) ? "enabled" : "disabled");
}

static ssize_t vcp_mobile_log_store(struct device *kobj,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&vcp_A_log_mutex);
	vcp_A_log_enable_set(enable);
	mutex_unlock(&vcp_A_log_mutex);

	return n;
}
DEVICE_ATTR_RW(vcp_mobile_log);


/*
 * create device sysfs, vcp ADB cmd to set VCP wakeup AP flag
 */
static ssize_t vcp_A_logger_wakeup_AP_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	unsigned int stat;

	stat = (vcp_A_logger_inited && vcp_A_logger_wakeup_ap) ? 1 : 0;

	return sprintf(buf, "[VCP A] logger wakeup AP is %s\n",
			(stat == 0x1) ? "enabled" : "disabled");
}

static ssize_t vcp_A_logger_wakeup_AP_store(struct device *kobj,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&vcp_A_log_mutex);
	vcp_A_log_wakeup_set(enable);
	mutex_unlock(&vcp_A_log_mutex);

	return n;
}

DEVICE_ATTR_RW(vcp_A_logger_wakeup_AP);

/*
 * create device sysfs, vcp last log show
 */
static ssize_t vcp_A_get_last_log_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	vcp_A_get_last_log(last_log_info.vcp_log_buf_maxlen);
	return sprintf(buf, "vcp_log_buf_maxlen=%u, log=%s\n",
			last_log_info.vcp_log_buf_maxlen,
			vcp_A_last_log ? vcp_A_last_log : "");
}

DEVICE_ATTR_RO(vcp_A_get_last_log);

/*
 * logger UT test
 *
 */
#if VCP_LOGGER_UT
static ssize_t vcp_A_mobile_log_UT_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	unsigned int w_pos, r_pos, datalen;
	char *logger_buf;
	size_t len = 1024;

	if (!vcp_A_logger_inited)
		return 0;

	datalen = 0;

	mutex_lock(&vcp_A_log_mutex);

	r_pos = VCP_A_buf_info->r_pos;
	w_pos = VCP_A_buf_info->w_pos;

	if (r_pos == w_pos)
		goto error;

	if (r_pos > w_pos)
		datalen = DRAM_BUF_LEN - r_pos; /* not wrap */
	else
		datalen = w_pos - r_pos;

	if (datalen > len)
		datalen = len;

	logger_buf = ((char *) VCP_A_log_ctl) +
			VCP_A_log_ctl->buff_ofs + r_pos;

	len = datalen;
	/*memory copy from log buf*/
	memcpy_fromio(buf, logger_buf, len);

	r_pos += datalen;
	if (r_pos >= DRAM_BUF_LEN)
		r_pos -= DRAM_BUF_LEN;

	VCP_A_buf_info->r_pos = r_pos;

error:
	mutex_unlock(&vcp_A_log_mutex);

	return len;
}

static ssize_t vcp_A_mobile_log_UT_store(struct device *kobj,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&vcp_A_log_mutex);
	vcp_A_log_enable_set(enable);
	mutex_unlock(&vcp_A_log_mutex);

	return n;
}

DEVICE_ATTR_RW(vcp_A_mobile_log_UT);
#endif

static ssize_t log_filter_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	uint32_t filter;
	struct vcp_logger_ctrl_msg msg;

	if (sscanf(buf, "0x%08x", &filter) != 1)
		return -EINVAL;

	msg.cmd = VCP_LOGGER_IPI_SET_FILTER;
	msg.u.flag.enable = filter;
	ret = mtk_ipi_send(&vcp_ipidev, IPI_OUT_LOGGER_CTRL, 0, &msg,
			   sizeof(msg)/MBOX_SLOT_SIZE, 0);
	switch (ret) {
	case IPI_ACTION_DONE:
		pr_notice("[VCP] Set log filter to 0x%08x\n", filter);
		return count;

	case IPI_PIN_BUSY:
		pr_notice("[VCP] IPI busy. Set log filter failed!\n");
		return -EBUSY;

	default:
		pr_notice("[VCP] IPI error. Set log filter failed!\n");
		return -EIO;
	}
}
DEVICE_ATTR_WO(log_filter);


/*
 * IPI for logger init
 */
static int vcp_logger_init_handler(struct VCP_LOG_INFO *log_info)
{
	unsigned long flags;
	phys_addr_t dma_addr;

	dma_addr = vcp_get_reserve_mem_phys(VCP_A_LOGGER_MEM_ID);
	pr_debug("[VCP] vcp_get_reserve_mem_phys=%llx\n", (uint64_t)dma_addr);
	spin_lock_irqsave(&vcp_A_log_buf_spinlock, flags);
	/* sync vcp last log information*/
	last_log_info.vcp_log_dram_addr = log_info->vcp_log_dram_addr;
	last_log_info.vcp_log_buf_addr = log_info->vcp_log_buf_addr;
	last_log_info.vcp_log_start_addr = log_info->vcp_log_start_addr;
	last_log_info.vcp_log_end_addr = log_info->vcp_log_end_addr;
	last_log_info.vcp_log_buf_maxlen = log_info->vcp_log_buf_maxlen;
	/*cofirm last log information is less than tcm size*/
	if (last_log_info.vcp_log_dram_addr > vcpreg.total_tcmsize)
		pr_notice("[VCP] last_log_info.vcp_log_dram_addr %x is over tcm_size %x\n",
			last_log_info.vcp_log_dram_addr, vcpreg.total_tcmsize);
	if (last_log_info.vcp_log_buf_addr > vcpreg.total_tcmsize)
		pr_debug("[VCP] last_log_info.vcp_log_buf_addr %x is over tcm_size %x\n",
			last_log_info.vcp_log_buf_addr, vcpreg.total_tcmsize);
	if (last_log_info.vcp_log_start_addr > vcpreg.total_tcmsize)
		pr_debug("[VCP] last_log_info.vcp_log_start_addr %x is over tcm_size %x\n",
			last_log_info.vcp_log_start_addr, vcpreg.total_tcmsize);
	if (last_log_info.vcp_log_end_addr > vcpreg.total_tcmsize)
		pr_debug("[VCP] last_log_info.vcp_log_end_addr %x is over tcm_size %x\n",
			last_log_info.vcp_log_end_addr, vcpreg.total_tcmsize);
	if (last_log_info.vcp_log_buf_addr + last_log_info.vcp_log_buf_maxlen >
		vcpreg.total_tcmsize)
		pr_debug("[VCP]  end of last_log_info.vcp_last_log_buf %x is over tcm_size %x\n",
			last_log_info.vcp_log_buf_addr + last_log_info.vcp_log_buf_maxlen,
				vcpreg.total_tcmsize);

	/* setting dram ctrl config to vcp*/
	/* vcp side get wakelock, AP to write info to vcp sram*/
	mt_reg_sync_writel((uint32_t)VCP_PACK_IOVA(dma_addr),
			(VCP_TCM + last_log_info.vcp_log_dram_addr));
	/* set init flag here*/
	vcp_A_logger_inited = 1;
	spin_unlock_irqrestore(&vcp_A_log_buf_spinlock, flags);

	/*set a wq to enable vcp logger*/
	vcp_logger_notify_work[VCP_A_ID].id = VCP_A_ID;
#if VCP_LOGGER_ENABLE
	vcp_logger_notify_work[VCP_A_ID].flags = 0;
	vcp_schedule_logger_work(&vcp_logger_notify_work[VCP_A_ID]);
#endif

	return 0;
}

/*
 * IPI for logger control
 * @param id:   IPI id
 * @param prdata: callback function parameter
 * @param data:  IPI data
 * @param len: IPI data length
 */
static int vcp_logger_ctrl_handler(unsigned int id, void *prdata, void *data,
				    unsigned int len)
{
	struct vcp_logger_ctrl_msg msg = *(struct vcp_logger_ctrl_msg *)data;

	switch (msg.cmd) {
	case VCP_LOGGER_IPI_INIT:
		vcp_logger_init_handler(&msg.u.info);
		break;
	case VCP_LOGGER_IPI_WAKEUP:
		vcp_logger_wakeup_handler(id, prdata, &msg.u.flag, len);
		break;
	default:
		break;
	}

	return 0;
}

/*
 * callback function for work struct
 * notify apps to start their tasks or generate an exception according to flag
 * NOTE: this function may be blocked and should not be called in interrupt
 *       context
 * @param ws:   work struct
 */
static void vcp_logger_notify_ws(struct work_struct *ws)
{
	int ret;
	unsigned int retrytimes;
	unsigned int vcp_ipi_id;
	unsigned long long ap_time;
	struct vcp_logger_ctrl_msg msg;
	phys_addr_t dma_addr;
	struct vcp_work_struct *sws =
		container_of(ws, struct vcp_work_struct, work);

	vcp_ipi_id = IPI_OUT_LOGGER_CTRL;
	msg.cmd = VCP_LOGGER_IPI_INIT;
	dma_addr = vcp_get_reserve_mem_phys(VCP_A_LOGGER_MEM_ID);
	msg.u.init.addr = (uint32_t)(VCP_PACK_IOVA(dma_addr));
	msg.u.init.size = vcp_get_reserve_mem_size(VCP_A_LOGGER_MEM_ID);
	ap_time = (unsigned long long)ktime_to_ns(ktime_get());
	msg.u.init.ap_time_l = (unsigned int)(ap_time & 0xFFFFFFFF);
	msg.u.init.ap_time_h = (unsigned int)((ap_time >> 32) & 0xFFFFFFFF);

	pr_debug("[VCP] %s: id=%u, ap_time %llu flag %x\n",
		__func__, vcp_ipi_id, ap_time, sws->flags);
	/*
	 *send ipi to invoke vcp logger
	 */
	retrytimes = VCP_IPI_RETRY_TIMES;
	do {
		if (is_vcp_ready(VCP_A_ID)) {
			ret = mtk_ipi_send(&vcp_ipidev, vcp_ipi_id, 0, &msg,
					   sizeof(msg)/MBOX_SLOT_SIZE, 0);

			if ((retrytimes % 500) == 0)
				pr_debug("[VCP] %s: ipi ret=%d\n", __func__, ret);
			if (ret == IPI_ACTION_DONE)
				break;
		}
		retrytimes--;
		udelay(1000);
	} while (retrytimes > 0 && vcp_A_logger_inited);

	udelay(1000);
	vcp_A_log_enable_set(VCP_A_log_ctl->enable);
}

/******************************************************************************
 * init vcp logger dram ctrl structure
 * @return:     -1: fail, otherwise: end of buffer
 *****************************************************************************/
int vcp_logger_init(phys_addr_t start, phys_addr_t limit)
{
	int last_ofs;

	/*init wait queue*/
	init_waitqueue_head(&vcp_A_logwait);
	vcp_A_logger_wakeup_ap = 0;
	mutex_init(&vcp_logger_mutex);
	/*init work queue*/
	INIT_WORK(&vcp_logger_notify_work[VCP_A_ID].work, vcp_logger_notify_ws);

	/*init dram ctrl table*/
	last_ofs = 0;
#if IS_ENABLED(CONFIG_ARM64)
	VCP_A_log_ctl = (struct log_ctrl_s *) start;
#else
	/* plz fix origial ptr to phys_addr flow */
	VCP_A_log_ctl = (struct log_ctrl_s *) (u32) start;
#endif
	VCP_A_log_ctl->base = PLT_LOG_ENABLE; /* magic */
	VCP_A_log_ctl->enable = 0;
	VCP_A_log_ctl->size = sizeof(*VCP_A_log_ctl);

	last_ofs += VCP_A_log_ctl->size;
	VCP_A_log_ctl->info_ofs = last_ofs;

	last_ofs += sizeof(*VCP_A_buf_info);
	last_ofs = ROUNDUP(last_ofs, 4);
	VCP_A_log_ctl->buff_ofs = last_ofs;
	VCP_A_log_ctl->buff_size = DRAM_BUF_LEN;

	VCP_A_buf_info = (struct buffer_info_s *)
		(((unsigned char *) VCP_A_log_ctl) + VCP_A_log_ctl->info_ofs);
	VCP_A_buf_info->r_pos = 0;
	VCP_A_buf_info->w_pos = 0;

	last_ofs += VCP_A_log_ctl->buff_size;

	if (last_ofs >= limit) {
		pr_notice("[VCP]:%s() initial fail, last_ofs=%u, limit=%u\n",
			__func__, last_ofs, (unsigned int) limit);
		goto error;
	}

	/* init last log buffer*/
	last_log_info.vcp_log_buf_maxlen = LAST_LOG_BUF_SIZE;
	if (!vcp_A_last_log) {
		/* Allocate one more byte for the NULL character. */
		vcp_A_last_log = vmalloc(last_log_info.vcp_log_buf_maxlen + 1);
	}

	/* register logger ctrl IPI */
	mtk_ipi_register(&vcp_ipidev, IPI_IN_LOGGER_CTRL,
			(void *)vcp_logger_ctrl_handler, NULL,
			&msg_vcp_logger_ctrl);

	return last_ofs;

error:
	vcp_A_logger_inited = 0;
	VCP_A_log_ctl = NULL;
	VCP_A_buf_info = NULL;
	return -1;
}

void vcp_logger_uninit(void)
{
	char *tmp = vcp_A_last_log;

	vcp_A_logger_inited = 0;
	vcp_A_last_log = NULL;
	if (tmp)
		vfree(tmp);
}

const struct file_operations vcp_A_log_file_ops = {
	.owner = THIS_MODULE,
	.read = vcp_A_log_if_read,
	.open = vcp_A_log_if_open,
	.poll = vcp_A_log_if_poll,
};


/*
 * move vcp last log from sram to dram
 * NOTE: this function may be blocked
 * @param vcp_core_id:  fill vcp id to get last log
 */
void vcp_crash_log_move_to_buf(enum vcp_core_id vcp_id)
{
	int pos;
	unsigned int ret;
	unsigned int length;
	unsigned int log_buf_idx;    /* VCP log buf pointer */
	unsigned int log_start_idx;  /* VCP log start pointer */
	unsigned int log_end_idx;    /* VCP log end pointer */
	unsigned int w_pos;          /* buf write pointer */
	char *dram_logger_limit = /* VCP log reserve limitation */
		(char *)(vcp_get_reserve_mem_virt(VCP_A_LOGGER_MEM_ID)
		+ vcp_get_reserve_mem_size(VCP_A_LOGGER_MEM_ID));
	char *pre_vcp_logger_buf = NULL;
	char *dram_logger_buf;       /* dram buffer */
	int vcp_awake_flag;

	char *crash_message = "****VCP EE LOG DUMP****\n";
	unsigned char *vcp_logger_buf = (unsigned char *)(VCP_TCM +
					last_log_info.vcp_log_buf_addr);

	if (!vcp_A_logger_inited && vcp_id == VCP_A_ID) {
		pr_notice("[VCP] %s(): logger has not been init\n", __func__);
		return;
	}

	mutex_lock(&vcp_logger_mutex);

	/* VCP keep awake */
	vcp_awake_flag = 0;
	if (vcp_awake_lock((void *)vcp_id) == -1) {
		vcp_awake_flag = -1;
		pr_debug("[VCP] %s: awake vcp fail\n", __func__);
	}

	/*cofirm last log information is less than tcm size*/
	if (last_log_info.vcp_log_buf_addr > vcpreg.total_tcmsize) {
		pr_notice("[VCP] %s: last_log_info.vcp_log_buf_addr %x is over tcm_size %x\n",
			__func__, last_log_info.vcp_log_buf_addr, vcpreg.total_tcmsize);
		goto exit;
	}
	if (last_log_info.vcp_log_start_addr > vcpreg.total_tcmsize) {
		pr_notice("[VCP] %s: last_log_info.vcp_log_start_addr %x is over tcm_size %x\n",
			__func__, last_log_info.vcp_log_start_addr, vcpreg.total_tcmsize);
		goto exit;
	}
	if (last_log_info.vcp_log_end_addr > vcpreg.total_tcmsize) {
		pr_notice("[VCP] %s: last_log_info.vcp_log_end_addr %x is over tcm_size %x\n",
			__func__, last_log_info.vcp_log_end_addr, vcpreg.total_tcmsize);
		goto exit;
	}
	if (last_log_info.vcp_log_buf_addr + last_log_info.vcp_log_buf_maxlen >
		vcpreg.total_tcmsize) {
		pr_debug("[VCP] %s: end of last_log_info.vcp_last_log_buf %x is over tcm_size %x\n",
			__func__, last_log_info.vcp_log_buf_addr + last_log_info.vcp_log_buf_maxlen,
				vcpreg.total_tcmsize);
		goto exit;
	}
	log_buf_idx = readl((void __iomem *)(VCP_TCM +
				last_log_info.vcp_log_buf_addr));
	log_start_idx = readl((void __iomem *)(VCP_TCM +
				last_log_info.vcp_log_start_addr));
	log_end_idx = readl((void __iomem *)(VCP_TCM +
				last_log_info.vcp_log_end_addr));

	/* if loggger_r/w_pos was messed up, dump all message in logger_buf */
	if (((log_start_idx < log_buf_idx + last_log_info.vcp_log_buf_maxlen)
	    || (log_start_idx >= log_buf_idx))
	    && ((log_end_idx < log_buf_idx + last_log_info.vcp_log_buf_maxlen)
	    || (log_end_idx >= log_buf_idx))) {

		if (log_end_idx >= log_start_idx)
			length = log_end_idx - log_start_idx;
		else
			length = last_log_info.vcp_log_buf_maxlen -
				(log_start_idx - log_end_idx);
	} else {
		length = last_log_info.vcp_log_buf_maxlen;
		log_start_idx = log_buf_idx;
		log_end_idx = log_buf_idx + length - 1;
	}

	if (length >= last_log_info.vcp_log_buf_maxlen) {
		pr_notice("[VCP] %s: length >= max\n", __func__);
		length = last_log_info.vcp_log_buf_maxlen;
	}

	pre_vcp_logger_buf = vcp_last_logger;
	vcp_last_logger = vmalloc(length + strlen(crash_message) + 1);
	if (log_start_idx > last_log_info.vcp_log_buf_maxlen) {
		pr_debug("[VCP] %s: vcp_logger_buf +log_start_idx %x is over tcm_size %x\n",
			__func__, last_log_info.vcp_log_buf_addr + log_start_idx,
				vcpreg.total_tcmsize);
		goto exit;
	}
	/* read log from vcp buffer */
	ret = 0;
	if (vcp_last_logger) {
		ret += snprintf(vcp_last_logger, strlen(crash_message),
			crash_message);
		ret--;
		while ((log_start_idx != log_end_idx) &&
			ret <= (length + strlen(crash_message))) {
			vcp_last_logger[ret] = vcp_logger_buf[log_start_idx];
			log_start_idx++;
			ret++;
			if (log_start_idx >= last_log_info.vcp_log_buf_maxlen)
				log_start_idx = log_start_idx -
					last_log_info.vcp_log_buf_maxlen;

			vcp_last_logger[ret] = '\0';
		}
	} else {
		/* no buffer, just skip logs */
		log_start_idx = log_end_idx;
	}

	if (ret != 0) {
		/* get buffer w pos */
		w_pos = VCP_A_buf_info->w_pos;

		if (w_pos >= DRAM_BUF_LEN) {
			pr_notice("[VCP] %s(): w_pos >= DRAM_BUF_LEN, w_pos=%u",
				__func__, w_pos);
			return;
		}

		/* copy to dram buffer */
		dram_logger_buf = ((char *) VCP_A_log_ctl) +
		    VCP_A_log_ctl->buff_ofs + w_pos;
		/* check write address don't over logger reserve memory */
		if (dram_logger_buf > dram_logger_limit) {
			pr_debug("[VCP] %s: dram_logger_buf %x oversize reserve mem %x\n",
			__func__, dram_logger_buf, dram_logger_limit);
		goto exit;
		}

		/* memory copy from log buf */
		pos = 0;
		while ((pos != ret) && pos <= ret) {
			*dram_logger_buf = vcp_last_logger[pos];
			pos++;
			w_pos++;
			dram_logger_buf++;
			if (w_pos >= DRAM_BUF_LEN) {
				/* warp */
				pr_notice("[VCP] %s: dram warp\n", __func__);
				w_pos = 0;

				dram_logger_buf = ((char *) VCP_A_log_ctl) +
					VCP_A_log_ctl->buff_ofs;
			}
		}
		/* update write pointer */
		VCP_A_buf_info->w_pos = w_pos;
	}
exit:
	/* VCP release awake */
	if (vcp_awake_flag == 0) {
		if (vcp_awake_unlock((void *)vcp_id) == -1)
			pr_debug("[VCP] %s: awake unlock fail\n", __func__);
	}

	mutex_unlock(&vcp_logger_mutex);
	if (pre_vcp_logger_buf != NULL)
		vfree(pre_vcp_logger_buf);
}



/*
 * get log from vcp and optionally save it
 * NOTE: this function may be blocked
 * @param vcp_core_id:  fill vcp id to get last log
 */
void vcp_get_log(enum vcp_core_id vcp_id)
{
	pr_debug("[VCP] %s\n", __func__);
#if VCP_LOGGER_ENABLE
	vcp_A_get_last_log(last_log_info.vcp_log_buf_maxlen);
	/*move last log to dram*/
	vcp_crash_log_move_to_buf(vcp_id);
#endif
}

/*
 * return useful log for aee issue dispatch
 */
#define CMP_SAFT_RANGE	176
#define DEFAULT_IDX (last_log_info.vcp_log_buf_maxlen/3)
char *vcp_pickup_log_for_aee(void)
{
	char *last_log;
	int i;
	char keyword1[] = "coredump";
	char keyword2[] = "exception";

	if (vcp_A_last_log == NULL)
		return NULL;
	last_log = &vcp_A_last_log[DEFAULT_IDX]; /* default value */

	for (i = last_log_info.vcp_log_buf_maxlen; i >= CMP_SAFT_RANGE; i--) {
		if (vcp_A_last_log[i-0] != keyword1[7])
			continue;
		if (vcp_A_last_log[i-1] != keyword1[6])
			continue;
		if (vcp_A_last_log[i-2] != keyword1[5])
			continue;
		if (vcp_A_last_log[i-3] != keyword1[4])
			continue;
		if (vcp_A_last_log[i-4] != keyword1[3])
			continue;
		if (vcp_A_last_log[i-5] != keyword1[2])
			continue;
		if (vcp_A_last_log[i-6] != keyword1[1])
			continue;
		if (vcp_A_last_log[i-7] != keyword1[0])
			continue;
		last_log = &vcp_A_last_log[i-CMP_SAFT_RANGE];
		return last_log;
	}

	for (i = last_log_info.vcp_log_buf_maxlen; i >= CMP_SAFT_RANGE; i--) {
		if (vcp_A_last_log[i-0] != keyword2[8])
			continue;
		if (vcp_A_last_log[i-1] != keyword2[7])
			continue;
		if (vcp_A_last_log[i-2] != keyword2[6])
			continue;
		if (vcp_A_last_log[i-3] != keyword2[5])
			continue;
		if (vcp_A_last_log[i-4] != keyword2[4])
			continue;
		if (vcp_A_last_log[i-5] != keyword2[3])
			continue;
		if (vcp_A_last_log[i-6] != keyword2[2])
			continue;
		if (vcp_A_last_log[i-7] != keyword2[1])
			continue;
		if (vcp_A_last_log[i-8] != keyword2[0])
			continue;
		last_log = &vcp_A_last_log[i-CMP_SAFT_RANGE];
		return last_log;
	}
	return last_log;
}

/*
 * set vcp_A_logger_inited
 */
void vcp_logger_init_set(unsigned int value)
{
	/*vcp_A_logger_inited
	 *  0: logger not init
	 *  1: logger inited
	 */
	vcp_A_logger_inited = value;
}

