// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <asm/mman.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/sysinfo.h>
#include <linux/seq_file.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/iommu.h>
#include <linux/proc_fs.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>

#include "apusys_core.h"
#include "hw_logger.h"

#include <linux/sched/signal.h>

/* debug log level */
static unsigned char g_hw_logger_log_lv = DBG_LOG_INFO;

/* dev */
#define HW_LOGGER_DEV_NAME "apu_hw_logger"
static struct device *hw_logger_dev;

/* sysfs related */
static struct kobject *root_dir;

/* procfs related */
static struct proc_dir_entry *log_root;
static struct proc_dir_entry *log_devinfo;
static struct proc_dir_entry *log_devattr;
static struct proc_dir_entry *log_seqlog;
static struct proc_dir_entry *log_seqlogL;

/* logtop mmap address */
static void *apu_logtop;
static void *apu_logd;

/* hw log buffer related */
static char *hw_log_buf;
static dma_addr_t hw_log_buf_addr;

/* local buffer related */
DEFINE_MUTEX(hw_logger_mutex);
DEFINE_SPINLOCK(hw_logger_spinlock);
static char *local_log_buf;

/* for local buffer tracking */
static unsigned int __loc_log_w_ofs;
static unsigned int __loc_log_ov_flg;
static unsigned int __loc_log_sz;

/* for hw buffer tracking */
static unsigned int __hw_log_r_ofs;

/* for procfs normal dump */
static unsigned int g_log_w_ofs;
static unsigned int g_log_r_ofs;
static unsigned int g_log_ov_flg;
/* for procfs lock dump */
static unsigned int g_log_w_ofs_l;
static unsigned int g_log_r_ofs_l;
static unsigned int g_log_ov_flg_l;
/* for sysfs normal dump */
static unsigned int g_dump_log_r_ofs;

static bool apu_toplog_deep_idle;

struct hw_logger_seq_data {
	unsigned int w_ptr;
	unsigned int r_ptr;
	unsigned int ov_flg;
};

static void hw_logger_buf_invalidate(void)
{
	dma_sync_single_for_cpu(
		hw_logger_dev, hw_log_buf_addr,
		HWLOGR_LOG_SIZE, DMA_FROM_DEVICE);
}

static int hw_logger_buf_alloc(struct device *dev)
{
	int ret = 0;

	ret = of_dma_configure(dev, dev->of_node, true);
	if (ret) {
		HWLOGR_ERR("of_dma_configure fail (%d)\n", ret);
		ret = -ENOMEM;
		goto out;
	}

	ret = dma_set_coherent_mask(dev, DMA_BIT_MASK(64));
	if (ret) {
		HWLOGR_ERR("dma_set_coherent_mask fail (%d)\n", ret);
		ret = -ENOMEM;
		goto out;
	}

	/* local buffer for dump */
	local_log_buf = kzalloc(LOCAL_LOG_SIZE, GFP_KERNEL);
	if (!local_log_buf) {
		ret = -ENOMEM;
		goto out;
	}

	/* memory used by hw logger */
	hw_log_buf = kzalloc(HWLOGR_LOG_SIZE, GFP_KERNEL);
	if (!hw_log_buf) {
		ret = -ENOMEM;
		goto out;
	}

	hw_log_buf_addr = dma_map_single(dev, hw_log_buf,
		HWLOGR_LOG_SIZE, DMA_FROM_DEVICE);
	ret = dma_mapping_error(dev, hw_log_buf_addr);
	if (ret) {
		HWLOGR_ERR("dma_map_single fail (%d)\n", ret);
		ret = -ENOMEM;
		goto out;
	}

	HWLOGR_DBG("local_log_buf = 0x%llx\n", (unsigned long long)local_log_buf);
	HWLOGR_DBG("hw_log_buf = 0x%llx, hw_log_buf_addr = 0x%llx\n",
		(unsigned long long)hw_log_buf, hw_log_buf_addr);

out:
	return ret;
}

static bool get_ov_flag(void)
{
	unsigned int intrs = GET_MASK_BITS(APU_LOGTOP_CON_FLAG);

	return !!(intrs & OVWRITE_FLAG);
}

static unsigned long long get_st_addr(void)
{
	unsigned long long st_addr;

	st_addr = (unsigned long long)
		GET_MASK_BITS(APU_LOGTOP_CON_ST_ADDR_HI) << 32;
	st_addr = st_addr | ioread32(APU_LOG_BUF_ST_ADDR);

	return st_addr;
}

static unsigned int get_t_size(void)
{
	return ioread32(APU_LOG_BUF_T_SIZE);
}

static unsigned long long get_w_ptr(void)
{
	unsigned long long st_addr, w_ptr;
	unsigned int t_size;

	st_addr = get_st_addr();
	t_size = get_t_size();

	w_ptr = ((2ULL << 34) +
		((unsigned long long)ioread32(APU_LOG_BUF_W_PTR)
		<< 2) - st_addr) % t_size + st_addr;

	return w_ptr;
}

static unsigned long long get_r_ptr(void)
{
#ifdef HW_LOG_ISR
	return (unsigned long long)ioread32(APU_LOG_BUF_R_PTR) << 2;
#else
	return (unsigned long long)__hw_log_r_ofs << 2;
#endif
}

static void set_r_ptr(unsigned long long r_ptr)
{
#ifdef HW_LOG_ISR
	iowrite32(lower_32_bits(r_ptr >> 2),
		APU_LOG_BUF_R_PTR);
#else
	__hw_log_r_ofs = lower_32_bits(r_ptr >> 2);
#endif
}

int hw_logger_config_init(struct mtk_apu *apu)
{
	unsigned long flags;
	struct logger_init_info *st_logger_init_info;

	if (!apu || !apu->conf_buf) {
		HWLOGR_ERR("invalid argument: apu\n");
		return -EINVAL;
	}

	if (!apu_logtop)
		return 0;

	st_logger_init_info = (struct logger_init_info *)
		get_apu_config_user_ptr(apu->conf_buf, eLOGGER_INIT_INFO);

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	g_log_w_ofs = 0;
	g_log_w_ofs_l = 0;
	g_log_r_ofs = U32_MAX;
	g_log_r_ofs_l = U32_MAX;
	g_log_ov_flg = 0;
	g_log_ov_flg_l = 0;
	g_dump_log_r_ofs = U32_MAX;

	__loc_log_sz = 0;

	apu_toplog_deep_idle = false;

	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	if (hw_log_buf_addr) {
		st_logger_init_info->iova =
			lower_32_bits(hw_log_buf_addr);
		st_logger_init_info->iova_h =
			upper_32_bits(hw_log_buf_addr);
	}

	HWLOGR_DBG("set st_logger_init_info iova = 0x%x, iova_h = 0x%x\n",
		st_logger_init_info->iova,
		st_logger_init_info->iova_h);

	return 0;
}
EXPORT_SYMBOL(hw_logger_config_init);

static int apu_logtop_copy_buf(void)
{
	unsigned long flags;
	unsigned int w_ofs, r_ofs, t_size;
	unsigned int r_size;
	unsigned int log_w_ofs, log_ov_flg;
	bool ovwrite_flg;

	if (!apu_logtop || !hw_log_buf || !local_log_buf)
		return 0;

	ovwrite_flg = get_ov_flag();

#ifdef HW_LOG_ISR
	SET_MASK_BITS(GET_MASK_BITS(APU_LOGTOP_CON_FLAG),
		APU_LOGTOP_CON_FLAG);
#endif

	if (get_r_ptr() == 0)
		set_r_ptr(get_st_addr());

	/* offset,size is only 32bit width */
	w_ofs = get_w_ptr() - get_st_addr();
	r_ofs = get_r_ptr() - get_st_addr();
	t_size = get_t_size();

	HWLOGR_DBG("get_w_ptr() = 0x%llx, get_r_ptr() = 0x%llx\n",
		get_w_ptr(), get_r_ptr());
	HWLOGR_DBG("get_st_addr() = 0x%llx, get_t_size() = 0x%x\n",
		get_st_addr(), get_t_size());
	HWLOGR_DBG("w_ofs = 0x%x, r_ofs = 0x%x, t_size = 0x%x\n",
		w_ofs, r_ofs, t_size);

	if (w_ofs == r_ofs)
		goto out;

	/* check copy size */
	if (w_ofs > r_ofs)
		r_size = w_ofs - r_ofs;
	else
		r_size = t_size - r_ofs + w_ofs;

	/* sanity check */
	if (r_size >= t_size)
		r_size = t_size;

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	log_w_ofs = __loc_log_w_ofs;
	log_ov_flg = __loc_log_ov_flg;
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	if (w_ofs >= t_size || r_ofs >= t_size || t_size == 0) {
		HWLOGR_WARN("w_ofs = 0x%x, r_ofs = 0x%x, t_size = 0x%x\n", w_ofs, r_ofs, t_size);
		return 0;
	}

	if (log_w_ofs >= LOCAL_LOG_SIZE) {
		HWLOGR_WARN("log_w_ofs = 0x%x\n", log_w_ofs);
		return 0;
	}

	/* invalidate hw logger buf */
	hw_logger_buf_invalidate();

	while (r_size > 0) {
		HWLOGR_DBG("(%x)(%03x):(%d) %s",
			r_ofs, r_size, (int)strlen(hw_log_buf + r_ofs), hw_log_buf + r_ofs);
		if (DBG_ON())
			hwlogr_hex_dump(" ", hw_log_buf + r_ofs, HWLOG_LINE_MAX_LENS);

		/* copy hw logger buffer to local buffer */
		memcpy(local_log_buf + log_w_ofs,
			hw_log_buf + r_ofs, HWLOG_LINE_MAX_LENS);

		/* move local write pointer */
		log_w_ofs = (log_w_ofs + HWLOG_LINE_MAX_LENS) % LOCAL_LOG_SIZE;

		/* move hw logger read pointer */
		r_ofs = (r_ofs + HWLOG_LINE_MAX_LENS) % t_size;
		r_size -= HWLOG_LINE_MAX_LENS;

		__loc_log_sz += HWLOG_LINE_MAX_LENS;
		if (__loc_log_sz >= LOCAL_LOG_SIZE)
			log_ov_flg = 1;
	};

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	__loc_log_w_ofs = log_w_ofs;
	__loc_log_ov_flg = log_ov_flg;
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	/* set read pointer */
	set_r_ptr(get_st_addr() + r_ofs);

out:

	return 0;
}

int hw_logger_copy_buf(void)
{
	HWLOGR_DBG("in\n");

	if (!apu_logtop)
		return 0;

	mutex_lock(&hw_logger_mutex);

	if (apu_toplog_deep_idle)
		goto out;

	apu_logtop_copy_buf();

out:
	mutex_unlock(&hw_logger_mutex);

	return 0;
}

int hw_logger_deep_idle_enter(void)
{
	HWLOGR_DBG("in\n");

	if (!apu_logtop)
		return 0;

	mutex_lock(&hw_logger_mutex);

	apu_logtop_copy_buf();

	apu_toplog_deep_idle = true;

	mutex_unlock(&hw_logger_mutex);

	return 0;
}

int hw_logger_deep_idle_leave(void)
{
	HWLOGR_DBG("in\n");

	if (!apu_logtop)
		return 0;

	mutex_lock(&hw_logger_mutex);

	apu_toplog_deep_idle = false;

	/* clear read pointer */
	set_r_ptr(get_st_addr());

	mutex_unlock(&hw_logger_mutex);

	return 0;
}

#ifdef HW_LOG_ISR
/* copy data from ping-pong buffer to local log buffer */
static irqreturn_t apu_logtop_irq_handler(int irq, void *priv)
{
	unsigned int intrs;
	bool lbc_full_flg, ovwrite_flg;

	intrs = GET_MASK_BITS(APU_LOGTOP_CON_FLAG);
	HWLOGR_DBG("intrs = 0x%x\n", intrs);

	lbc_full_flg = !!(intrs & LBC_FULL_FLAG);
	ovwrite_flg = !!(intrs & OVWRITE_FLAG);

	if (!lbc_full_flg && !ovwrite_flg)
		HWLOGR_WARN("intr status = 0x%x should not occur\n", intrs);

	apu_logtop_copy_buf();

	SET_MASK_BITS(intrs, APU_LOGTOP_CON_FLAG);

	return IRQ_HANDLED;
}
#endif

static int apusys_debug_dump(struct seq_file *s, void *unused)
{
	DBG_HWLOG_CON(s, "%d\n", g_hw_logger_log_lv);
	return 0;
}

static int hw_logger_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_debug_dump, inode->i_private);
}

static ssize_t show_debuglv(struct file *filp, char __user *buffer,
					size_t count, loff_t *ppos)
{
	char buf[512];
	unsigned int len = 0;
	unsigned long flags;

	len += scnprintf(buf + len, sizeof(buf) - len,
		"g_hw_logger_log_lv = %d:\n",
		g_hw_logger_log_lv);

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	len += scnprintf(buf + len, sizeof(buf) - len,
		"__loc_log_w_ofs = %d,__loc_log_ov_flg = %d __loc_log_sz = %d\n",
		__loc_log_w_ofs, __loc_log_ov_flg, __loc_log_sz);
	len += scnprintf(buf + len, sizeof(buf) - len,
		"g_log_w_ofs = %d,g_log_r_ofs = %d g_log_ov_flg = %d\n",
		g_log_w_ofs, g_log_r_ofs, g_log_ov_flg);
	len += scnprintf(buf + len, sizeof(buf) - len,
		"g_log_w_ofs_l = %d,g_log_r_ofs_l = %d g_log_ov_flg_l = %d\n",
		g_log_w_ofs_l, g_log_r_ofs_l, g_log_ov_flg_l);
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

static ssize_t set_debuglv(struct file *flip,
						   const char __user *buffer,
						   size_t count, loff_t *f_pos)
{
	char *tmp, *cursor;
	int ret;
	unsigned int input = 0;

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		HWLOGR_ERR("copy_from_user failed, ret=%d\n", ret);
		kfree(tmp);
		return count;
	}

	tmp[count] = '\0';
	cursor = tmp;
	ret = kstrtouint(cursor, 10, &input);

	HWLOGR_INFO("set debug lv = %d\n", input);

	if (input <= DBG_LOG_DEBUG)
		g_hw_logger_log_lv = input;

	kfree(tmp);

	return count;
}

static const struct proc_ops apusys_debug_fops = {
	.proc_open = hw_logger_open,
	.proc_read = show_debuglv,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
	.proc_write = set_debuglv,
};

static ssize_t show_debugAttr(struct file *filp, char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[512];
	unsigned int len = 0;
	unsigned long flags;

	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"hw_log_buf = 0x%llx\n",
		(unsigned long long)hw_log_buf);

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"__loc_log_w_ofs = %d\n",
		__loc_log_w_ofs);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"__loc_log_ov_flg = %d\n",
		__loc_log_ov_flg);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"__loc_log_sz = %d\n",
		__loc_log_sz);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_w_ofs = %d\n",
		g_log_w_ofs);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_r_ofs = %d\n",
		g_log_r_ofs);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_ov_flg = %d\n",
		g_log_ov_flg);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_w_ofs_l = %d\n",
		g_log_w_ofs_l);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_r_ofs_l = %d\n",
		g_log_r_ofs_l);
	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_log_ov_flg_l = %d\n",
		g_log_ov_flg_l);
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	len += scnprintf(buf + len,
		sizeof(buf) - len,
		"g_hw_logger_log_lv = %d:\n",
		g_hw_logger_log_lv);

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

static const struct proc_ops hw_logger_attr_fops = {
	.proc_open = hw_logger_open,
	.proc_read = show_debugAttr,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/**
 * seq_start() takes a position as an argument and returns an iterator which
 * will start reading at that position.
 * start->show->next->show...->next->show->next->stop->start->stop
 */
static void *seq_start(struct seq_file *s, loff_t *pos)
{
	struct hw_logger_seq_data *pSData;
	unsigned int w_ptr, r_ptr, ov_flg;
	unsigned long flags;

	HWLOGR_DBG("in");

	hw_logger_copy_buf();

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	if (g_log_r_ofs == U32_MAX) {
		w_ptr = __loc_log_w_ofs;
		ov_flg = __loc_log_ov_flg;
		g_log_w_ofs = w_ptr;
		g_log_ov_flg = ov_flg;
		if (ov_flg)
			/* avoid stuck at while loop move one forward */
			r_ptr = (w_ptr + HWLOG_LINE_MAX_LENS) % HWLOGR_LOG_SIZE;
		else
			r_ptr = 0;
		g_log_r_ofs = r_ptr;
	} else {
		w_ptr = g_log_w_ofs;
		r_ptr = g_log_r_ofs;
	}
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	HWLOGR_DBG("w_ptr = %d, r_ptr = %d, ov_flg = %d, *pos = %d\n",
		w_ptr, r_ptr, ov_flg, (unsigned int)*pos);

	if (w_ptr == r_ptr) {
		spin_lock_irqsave(&hw_logger_spinlock, flags);
		g_log_r_ofs = U32_MAX;
		spin_unlock_irqrestore(&hw_logger_spinlock, flags);
		return NULL;
	}

	pSData = kzalloc(sizeof(struct hw_logger_seq_data),
		GFP_KERNEL);
	if (!pSData)
		return NULL;

	pSData->w_ptr = w_ptr;
	pSData->ov_flg = ov_flg;
	if (ov_flg == 0)
		pSData->r_ptr = r_ptr;
	else
		pSData->r_ptr = w_ptr;

	return pSData;
}

/**
 * seq_start() takes a position as an argument and returns an iterator which
 * will start reading at that position.
 */
static void *seq_startl(struct seq_file *s, loff_t *pos)
{
	uint32_t w_ptr, r_ptr, ov_flg;
	struct hw_logger_seq_data *pSData;
	unsigned long flags;

	HWLOGR_DBG("in");

	pSData = kzalloc(sizeof(struct hw_logger_seq_data),
		GFP_KERNEL);
	if (!pSData)
		return NULL;

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	if (g_log_r_ofs_l == U32_MAX) {
		w_ptr = __loc_log_w_ofs;
		ov_flg = __loc_log_ov_flg;
		g_log_w_ofs_l = w_ptr;
		g_log_ov_flg_l = ov_flg;
		if (ov_flg)
			/* avoid stuck at while loop move one forward */
			r_ptr = (w_ptr + HWLOG_LINE_MAX_LENS) % HWLOGR_LOG_SIZE;
		else
			r_ptr = 0;
		g_log_r_ofs_l = r_ptr;
	} else {
		w_ptr = g_log_w_ofs_l;
		r_ptr = g_log_r_ofs_l;
	}
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	/* for ctrl-c to force exit the loop */
	do {
		/* force flush last hw buffer */
		hw_logger_copy_buf();

		spin_lock_irqsave(&hw_logger_spinlock, flags);
		w_ptr = __loc_log_w_ofs;
		ov_flg = __loc_log_ov_flg;
		spin_unlock_irqrestore(&hw_logger_spinlock, flags);
		msleep_interruptible(100);
	} while (!signal_pending(current) && w_ptr == r_ptr);

	HWLOGR_DBG("w_ptr = %d, r_ptr = %d, ov_flg = %d, *pos = %d\n",
		w_ptr, r_ptr, ov_flg, (unsigned int)*pos);

	pSData->w_ptr = w_ptr;
	pSData->r_ptr = r_ptr;
	pSData->ov_flg = ov_flg;

	if (signal_pending(current)) {
		HWLOGR_DBG("BREAK w_ptr = %d, r_ptr = %d, ov_flg = %d\n",
			w_ptr, r_ptr, ov_flg);
		spin_lock_irqsave(&hw_logger_spinlock, flags);
		g_log_r_ofs_l = U32_MAX;
		spin_unlock_irqrestore(&hw_logger_spinlock, flags);
		return NULL;
	}

	return pSData;
}

/**
 * move the iterator forward to the next position in the sequence
 */
static void *seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct hw_logger_seq_data *pSData = v;
	unsigned long flags;

	HWLOGR_DBG(
		"w_ptr = %d, r_ptr = %d, ov_flg = %d, *pos = %d\n",
		pSData->w_ptr, pSData->r_ptr,
		pSData->ov_flg, (unsigned int)*pos);

	pSData->r_ptr = (pSData->r_ptr + HWLOG_LINE_MAX_LENS) % LOCAL_LOG_SIZE;
	spin_lock_irqsave(&hw_logger_spinlock, flags);
	g_log_r_ofs = pSData->r_ptr;
	/* just prevent warning */
	*pos = pSData->r_ptr + 1;
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	if (pSData->r_ptr != pSData->w_ptr)
		return pSData;

	HWLOGR_DBG(
		"END g_log_w_ofs = %d, g_log_r_ofs = %d, g_log_ov_flg = %d\n",
		g_log_w_ofs, g_log_r_ofs,
		g_log_ov_flg);

	return NULL;
}

static void *seq_nextl(struct seq_file *s, void *v, loff_t *pos)
{
	struct hw_logger_seq_data *pSData = v;
	unsigned long flags;

	HWLOGR_DBG(
		"w_ptr = %d, r_ptr = %d, ov_flg = %d, *pos = %d\n",
		pSData->w_ptr, pSData->r_ptr,
		pSData->ov_flg, (unsigned int)*pos);

	pSData->r_ptr = (pSData->r_ptr + HWLOG_LINE_MAX_LENS) % LOCAL_LOG_SIZE;
	spin_lock_irqsave(&hw_logger_spinlock, flags);
	g_log_r_ofs_l = pSData->r_ptr;
	/* just prevent warning */
	*pos = pSData->r_ptr + 1;
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	if (pSData->r_ptr != pSData->w_ptr)
		return pSData;

	HWLOGR_DBG(
		"END g_log_w_ofs_l = %d, g_log_r_ofs_l = %d, g_log_ov_flg_l = %d\n",
		g_log_w_ofs_l, g_log_r_ofs_l,
		g_log_ov_flg_l);

	return NULL;
}

/**
 * stop() is called when iteration is complete (clean up)
 */
static void seq_stop(struct seq_file *s, void *v)
{
	HWLOGR_DBG("in");
	kfree(v);
}

/**
 * success return 0, otherwise return error code
 */
static int seq_show(struct seq_file *s, void *v)
{
	struct hw_logger_seq_data *pSData = v;

	HWLOGR_DBG("(%04d)(%d) %s", pSData->r_ptr,
		(int)strlen(local_log_buf + pSData->r_ptr), local_log_buf + pSData->r_ptr);

	seq_printf(s, "%s",
		local_log_buf + pSData->r_ptr);

	return 0;
}

static const struct seq_operations seq_ops = {
	.start = seq_start,
	.next  = seq_next,
	.stop  = seq_stop,
	.show  = seq_show
};

static const struct seq_operations seq_ops_lock = {
	.start = seq_startl,
	.next  = seq_nextl,
	.stop  = seq_stop,
	.show  = seq_show
};

static int debug_sqopen_lock(struct inode *inode, struct file *file)
{
	return seq_open(file, &seq_ops_lock);
}

static int debug_sqopen(struct inode *inode, struct file *file)
{
	return seq_open(file, &seq_ops);
}

static const struct proc_ops hw_loggerSeqLog_ops = {
	.proc_open    = debug_sqopen,
	.proc_read    = seq_read,
	.proc_lseek  = seq_lseek,
	.proc_release = seq_release
};

static const struct proc_ops hw_loggerSeqLogL_ops = {
	.proc_open    = debug_sqopen_lock,
	.proc_read    = seq_read,
	.proc_lseek  = seq_lseek,
	.proc_release = seq_release
};

static ssize_t apusys_log_dump(struct file *filep,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;
	uint32_t w_ptr, r_ptr, ov_flg, pr_ptr;
	uint32_t print_sz;
	unsigned long flags;

	spin_lock_irqsave(&hw_logger_spinlock, flags);
	w_ptr = __loc_log_w_ofs;
	ov_flg = __loc_log_ov_flg;
	if (g_dump_log_r_ofs == U32_MAX) {
		if (ov_flg)
			r_ptr = (w_ptr + HWLOG_LINE_MAX_LENS) % LOCAL_LOG_SIZE;
		else
			r_ptr = 0;
	} else {
		r_ptr = g_dump_log_r_ofs;
	}

	pr_ptr = r_ptr;
	spin_unlock_irqrestore(&hw_logger_spinlock, flags);

	if (w_ptr == r_ptr) {
		g_dump_log_r_ofs = U32_MAX;
		HWLOGR_DBG("end\n");
		return length;
	}

	do {
		print_sz = strlen(local_log_buf + pr_ptr);
		if ((length + print_sz + 1) <= size) {
			scnprintf(buf + length, print_sz, "%s", local_log_buf + pr_ptr);
			/* replace trailing null character with new line
			 * for log readability
			 */
			buf[length + print_sz] = '\n';
		} else
			break;
		length += (print_sz + 1); /* include '\n' */
		pr_ptr = (pr_ptr + HWLOG_LINE_MAX_LENS) % LOCAL_LOG_SIZE;
		spin_lock_irqsave(&hw_logger_spinlock, flags);
		g_log_r_ofs = pr_ptr;
		spin_unlock_irqrestore(&hw_logger_spinlock, flags);
	} while (pr_ptr != w_ptr);

	g_dump_log_r_ofs = pr_ptr;

	HWLOGR_DBG("length = %d, g_dump_log_r_ofs = %d\n", length, g_dump_log_r_ofs);

	return length;
}

struct bin_attribute bin_attr_apusys_hwlog = {
	.attr = {
		.name = "apusys_hwlog.txt",
		.mode = 0444,
	},
	.size = 0,
	.read = apusys_log_dump,
};

static int hw_logger_create_sysfs(struct device *dev)
{
	int ret = 0;

	/* create /sys/kernel/apusys_logger */
	root_dir = kobject_create_and_add("apusys_hwlogger",
		kernel_kobj);
	if (!root_dir) {
		HWLOGR_ERR("kobject_create_and_add fail (%d)\n",
			ret);
		return -EINVAL;
	}

	ret = sysfs_create_bin_file(root_dir,
		&bin_attr_apusys_hwlog);
	if (ret)
		HWLOGR_ERR("sysfs create fail (%d)\n",
			ret);

	return ret;
}

static void hw_logger_remove_sysfs(struct device *dev)
{
	sysfs_remove_bin_file(root_dir, &bin_attr_apusys_hwlog);
}

static void hw_logger_remove_procfs(struct device *dev)
{
	remove_proc_entry("log", log_root);
	remove_proc_entry("seq_log", log_root);
	remove_proc_entry("seq_logl", log_root);
	remove_proc_entry("attr", log_root);
	remove_proc_entry(APUSYS_HWLOGR_DIR, NULL);
}

static int hw_logger_create_procfs(struct device *dev)
{
	int ret = 0;

	log_root = proc_mkdir(APUSYS_HWLOGR_DIR, NULL);
	ret = IS_ERR_OR_NULL(log_root);
	if (ret) {
		HWLOGR_ERR("create dir fail (%d)\n", ret);
		goto out;
	}

	/* create device table info */
	log_devinfo = proc_create("log", 0444,
		log_root, &apusys_debug_fops);
	ret = IS_ERR_OR_NULL(log_devinfo);
	if (ret) {
		HWLOGR_ERR("create devinfo fail (%d)\n", ret);
		goto out;
	}

	log_seqlog = proc_create("seq_log", 0444,
		log_root, &hw_loggerSeqLog_ops);
	ret = IS_ERR_OR_NULL(log_seqlog);
	if (ret) {
		HWLOGR_ERR("create seqlog fail (%d)\n", ret);
		goto out;
	}

	log_seqlogL = proc_create("seq_logl", 0444,
		log_root, &hw_loggerSeqLogL_ops);
	ret = IS_ERR_OR_NULL(log_seqlogL);
	if (ret) {
		HWLOGR_ERR("create seqlogL fail (%d)\n", ret);
		goto out;
	}

	log_devattr = proc_create("attr", 0444,
		log_root, &hw_logger_attr_fops);

	ret = IS_ERR_OR_NULL(log_devattr);
	if (ret) {
		HWLOGR_ERR("create attr fail (%d)\n", ret);
		goto out;
	}

	return 0;

out:
	hw_logger_remove_procfs(dev);
	return ret;
}

static int hw_logger_memmap(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_logtop");
	if (res == NULL) {
		HWLOGR_ERR("apu_logtop get resource fail\n");
		ret = -ENODEV;
		goto out;
	}

	apu_logtop = ioremap(res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu_logtop)) {
		HWLOGR_ERR("apu_logtop remap base fail\n");
		ret = -ENOMEM;
		goto out;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_logd");
	if (res == NULL) {
		HWLOGR_ERR("apu_logd get resource fail\n");
		ret = -ENODEV;
		goto out;
	}

	apu_logd = ioremap(res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu_logd)) {
		HWLOGR_ERR("apu_logd remap base fail\n");
		ret = -ENOMEM;
		goto out;
	}

out:
	return ret;
}

static int hw_logger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	HWLOGR_INFO("hw_logger probe start\n");

	hw_logger_dev = dev;

	spin_lock_init(&hw_logger_spinlock);
	mutex_init(&hw_logger_mutex);

	ret = hw_logger_create_procfs(dev);
	if (ret) {
		HWLOGR_ERR("hw_logger_create_procfs fail\n");
		goto remove_procfs;
	}

	ret = hw_logger_create_sysfs(dev);
	if (ret) {
		HWLOGR_ERR("hw_logger_create_sysfs fail\n");
		goto remove_sysfs;
	}

	ret = hw_logger_memmap(pdev);
	if (ret) {
		HWLOGR_ERR("hw_logger_ioremap fail\n");
		goto remove_ioremap;
	}

	ret = hw_logger_buf_alloc(dev);
	if (ret) {
		HWLOGR_ERR("hw_logger_buf_alloc fail\n");
		goto remove_hw_log_buf;
	}
#ifdef HW_LOG_ISR
	ret = devm_request_threaded_irq(dev,
			platform_get_irq_byname(pdev, "apu_logtop"),
			NULL, apu_logtop_irq_handler, IRQF_ONESHOT,
			pdev->name, NULL);
	if (ret) {
		HWLOGR_ERR("failed to request IRQ (%d)\n", ret);
		goto remove_hw_log_buf;
	}
#endif
	HWLOGR_INFO("hw_logger probe done\n");

	return 0;

remove_hw_log_buf:
	kfree(hw_log_buf);
#ifdef HW_LOG_ISR
	kfree(local_log_buf);
#endif

remove_ioremap:
	if (apu_logtop)
		iounmap(apu_logtop);
	if (apu_logd)
		iounmap(apu_logd);

remove_sysfs:
	hw_logger_remove_sysfs(dev);

remove_procfs:
	hw_logger_remove_procfs(dev);

	HWLOGR_ERR("hw_logger probe error!!\n");

	return ret;
}

static int hw_logger_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	hw_logger_remove_procfs(dev);
	hw_logger_remove_sysfs(dev);
	dma_unmap_single(dev, hw_log_buf_addr, HWLOGR_LOG_SIZE, DMA_FROM_DEVICE);
	kfree(hw_log_buf);
#ifdef HW_LOG_ISR
	kfree(local_log_buf);
#endif
	return 0;
}

static const struct of_device_id apusys_hw_logger_of_match[] = {
	{ .compatible = "mediatek,apusys_hw_logger"},
	{},
};

static struct platform_driver hw_logger_driver = {
	.probe = hw_logger_probe,
	.remove = hw_logger_remove,
	.driver = {
		.name = HW_LOGGER_DEV_NAME,
		.of_match_table = of_match_ptr(apusys_hw_logger_of_match),
	}
};

int hw_logger_init(struct apusys_core_info *info)
{
	int ret = 0;

	HWLOGR_INFO("in\n");

	allow_signal(SIGKILL);

	ret = platform_driver_register(&hw_logger_driver);
	if (ret != 0) {
		HWLOGR_ERR("failed to register hw_logger driver");
		return -ENODEV;
	}

	return ret;
}

void hw_logger_exit(void)
{
	disallow_signal(SIGKILL);
	platform_driver_unregister(&hw_logger_driver);
}
