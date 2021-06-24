// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include <linux/debugfs.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/sched/signal.h>


#include "apu_ctrl_rpmsg.h"
#include "sw_logger.h"

#define SW_LOGGER_DEV_NAME "apu_sw_logger"

DEFINE_SPINLOCK(sw_logger_spinlock);

static u8 g_sw_logger_log_lv;

static struct proc_dir_entry *log_root;
static struct proc_dir_entry *log_devinfo;
static struct proc_dir_entry *log_devattr;
static struct proc_dir_entry *log_seqlog;
static struct proc_dir_entry *log_seqlogL;

static struct sw_logger_seq_data *pSeqData;
static bool startl_first_enter_session;

static struct device *sw_logger_dev;
static struct kobject *root_dir;
static dma_addr_t handle;
static char *sw_log_buf;

static void *apu_mbox;

#define APU_MBOX_BASE (0x19000000)
#define LOG_W_PTR (apu_mbox + 0x40)
#define LOG_R_PTR (apu_mbox + 0x44)
#define LOG_OV_FLG (apu_mbox + 0x4c)

struct sw_logger_seq_data {
	uint32_t w_ptr;
	uint32_t r_ptr;
	uint32_t overflow_flg;
	int i;
	int is_finished;
	char *data;
};

static struct sw_logger_seq_data pSeqData_lock_obj;

static int apusys_debug_dump(struct seq_file *s, void *unused)
{
	DBG_LOG_CON(s, "%d\n", g_sw_logger_log_lv);
	return 0;
}

static int sw_logger_open(struct inode *inode, struct file *file)
{
	return single_open(file, apusys_debug_dump, inode->i_private);
}

static ssize_t show_debuglv(struct file *filp, char __user *buffer,
					size_t count, loff_t *ppos)
{
	char buf[512];
	unsigned int len = 0;
	unsigned long flags;

	if (sw_log_buf) {
		len += scnprintf(buf + len, sizeof(buf) - len,
				"g_sw_logger_log_lv = %d:\n",
				g_sw_logger_log_lv);

		spin_lock_irqsave(&sw_logger_spinlock, flags);
		len += scnprintf(buf + len, sizeof(buf) - len,
				"w_ptr = %d,r_ptr = %d overflow_flg = %d\n",
				ioread32(LOG_W_PTR), ioread32(LOG_R_PTR),
				ioread32(LOG_OV_FLG));
		spin_unlock_irqrestore(&sw_logger_spinlock, flags);
	}

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

static void sw_logger_buf_invalidate(void)
{
	if (sw_logger_dev)
		dma_sync_single_for_cpu(
			sw_logger_dev, handle, APU_LOG_SIZE, DMA_FROM_DEVICE);
}

static int sw_logger_buf_alloc(struct device *dev)
{
	int ret;

	ret = of_dma_configure(dev, dev->of_node, true);
	if (ret) {
		pr_info("%s: of_dma_configure fail(%d)\n", __func__, ret);
		return -ENOMEM;
	}

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
	if (ret) {
		pr_info("%s: dma_set_coherent_mask fail(%d)\n", __func__, ret);
		return -ENOMEM;
	}

	sw_log_buf = kmalloc(APU_LOG_SIZE, GFP_KERNEL);
	pr_info("%s: sw_log_buf = 0x%x\n", __func__, sw_log_buf);
	if (!sw_log_buf)
		return -ENOMEM;
	memset(sw_log_buf, 0, APU_LOG_SIZE);

	handle = dma_map_single(dev, sw_log_buf, APU_LOG_SIZE,
		DMA_FROM_DEVICE);
	pr_info("handle = 0x%x\n", handle);
	if (dma_mapping_error(dev, handle) != 0) {
		pr_info("%s: dma_map_single fail\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

int sw_logger_config_init(struct mtk_apu *apu)
{
	unsigned long flags;
	struct logger_init_info *st_logger_init_info;

	if (!apu) {
		LOGGER_ERR("invalid argument: apu\n");
		return -EINVAL;
	}
	if (!(apu->conf_buf)) {
		LOGGER_ERR("invalid argument: apu->conf_buf\n");
		return -EINVAL;
	}

	/* sw logger not enabled */
	if (!apu_mbox)
		return 0;

	spin_lock_irqsave(&sw_logger_spinlock, flags);
	/* fixme: if reset ptrs necessary for each power on? */
	iowrite32(0, LOG_W_PTR);
	iowrite32(0, LOG_R_PTR);
	iowrite32(0, LOG_OV_FLG);
	spin_unlock_irqrestore(&sw_logger_spinlock, flags);

	st_logger_init_info = (struct logger_init_info *)
		get_apu_config_user_ptr(apu->conf_buf, eLOGGER_INIT_INFO);

	st_logger_init_info->iova = handle;
	//	log_mm_data.get_phys_param.phy_addr;

	LOGGER_INFO("%s: set st_logger_init_info.iova = 0x%x\n",
		__func__, st_logger_init_info->iova);

	return 0;
}
EXPORT_SYMBOL(sw_logger_config_init);

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
		LOGGER_ERR("copy_from_user failed, ret=%d\n", ret);
		kfree(tmp);
		return count;
	}

	tmp[count] = '\0';
	cursor = tmp;
	ret = kstrtouint(cursor, 10, &input);

	LOGGER_INFO("set debug lv = %d\n", input);

	if (input <= DEBUG_LOG_DEBUG)
		g_sw_logger_log_lv = input;

	kfree(tmp);

	return count;
}

static const struct proc_ops apusys_debug_fops = {
	.proc_open = sw_logger_open,
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

	if (sw_log_buf) {
		len += scnprintf(buf + len, sizeof(buf) - len,
						"sw_log_buf = 0x%llx\n",
						sw_log_buf);

		spin_lock_irqsave(&sw_logger_spinlock, flags);
		len += scnprintf(buf + len, sizeof(buf) - len,
						"w_ptr = %d\n",
						ioread32(LOG_W_PTR));
		len += scnprintf(buf + len, sizeof(buf) - len,
						"r_ptr = %d\n",
						ioread32(LOG_R_PTR));
		len += scnprintf(buf + len, sizeof(buf) - len,
						"overflow_flg = %d\n",
						ioread32(LOG_OV_FLG));
		spin_unlock_irqrestore(&sw_logger_spinlock, flags);

		len += scnprintf(buf + len, sizeof(buf) - len,
						"g_sw_logger_log_lv = %d:\n",
						g_sw_logger_log_lv);
	}

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

static const struct proc_ops sw_logger_attr_fops = {
	.proc_open = sw_logger_open,
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
	uint32_t w_ptr, r_ptr, overflow_flg;
	unsigned long flags;

	if (sw_log_buf == NULL) {
		LOGGER_ERR("%s: sw_log_buf == NULL\n", __func__);
		return NULL;
	}

	spin_lock_irqsave(&sw_logger_spinlock, flags);
	w_ptr = ioread32(LOG_W_PTR);
	r_ptr = ioread32(LOG_R_PTR);
	overflow_flg = ioread32(LOG_OV_FLG);
	spin_unlock_irqrestore(&sw_logger_spinlock, flags);

	sw_logger_buf_invalidate();

	LOGGER_INFO("w_ptr = %d, r_ptr = %d, overflow_flg = %d\n",
		w_ptr, r_ptr, overflow_flg);

	if (w_ptr == r_ptr && overflow_flg == 0)
		return NULL;

	if (pSeqData == NULL) {
		pSeqData = kzalloc(sizeof(struct sw_logger_seq_data),
			GFP_KERNEL);
		if (pSeqData != NULL) {
			pSeqData->w_ptr = w_ptr;
			pSeqData->r_ptr = r_ptr;
			pSeqData->overflow_flg = overflow_flg;
			if (overflow_flg == 0)
				pSeqData->i = r_ptr;
			else
				pSeqData->i = w_ptr;
			pSeqData->is_finished = 0;
		}
	}
	LOGGER_INFO("%s v = 0x%x\n", __func__, pSeqData);

	return pSeqData;
}

/**
 * seq_start() takes a position as an argument and returns an iterator which
 * will start reading at that position.
 */
static void *seq_startl(struct seq_file *s, loff_t *pos)
{
	uint32_t w_ptr, r_ptr, overflow_flg;
	struct sw_logger_seq_data *pSeqData_lock = &pSeqData_lock_obj;
	unsigned long flags;

	if (sw_log_buf == NULL)
		return NULL;

	spin_lock_irqsave(&sw_logger_spinlock, flags);
	w_ptr = ioread32(LOG_W_PTR);
	r_ptr = ioread32(LOG_R_PTR);
	overflow_flg = ioread32(LOG_OV_FLG);
	spin_unlock_irqrestore(&sw_logger_spinlock, flags);

	sw_logger_buf_invalidate();

	LOGGER_INFO("w_ptr = %d, r_ptr = %d, overflow_flg = %d\n",
		w_ptr, r_ptr, overflow_flg);

	/* for ctrl-c to force exit the loop */
	while (!signal_pending(current) && w_ptr == r_ptr) {
		usleep_range(10000, 12000);

		spin_lock_irqsave(&sw_logger_spinlock, flags);
		w_ptr = ioread32(LOG_W_PTR);
		r_ptr = ioread32(LOG_R_PTR);
		overflow_flg = ioread32(LOG_OV_FLG);
		spin_unlock_irqrestore(&sw_logger_spinlock, flags);

		sw_logger_buf_invalidate();

		pSeqData_lock->w_ptr = w_ptr;
		pSeqData_lock->r_ptr = r_ptr;
		pSeqData_lock->overflow_flg = overflow_flg;
		pSeqData_lock->i = r_ptr;
	}

	if (startl_first_enter_session ||
		pSeqData_lock->i == pSeqData_lock->w_ptr) {
		startl_first_enter_session = false;
		pSeqData_lock->w_ptr = w_ptr;
		pSeqData_lock->r_ptr = r_ptr;
		pSeqData_lock->overflow_flg = overflow_flg;
		pSeqData_lock->i = r_ptr;
	}

	if (signal_pending(current))
		startl_first_enter_session = true;

	LOGGER_INFO("%s v = 0x%x\n", __func__, pSeqData_lock);

	return pSeqData_lock;
}

/**
 * move the iterator forward to the next position in the sequence
 */
static void *seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct sw_logger_seq_data *pSData = v;

	if (pSData == NULL) {
		LOGGER_ERR("%s: pSData == NULL\n", __func__);
		return NULL;
	}

	LOGGER_INFO(
		"%s in, w_ptr = %d, r_ptr = %d,i = %d, overflow_flg = %d\n",
		__func__, pSData->w_ptr, pSData->r_ptr, pSData->i,
		pSData->overflow_flg);

	pSData->i = (pSData->i + LOG_LINE_MAX_LENS) % APU_LOG_SIZE;

	if (pSData->i != pSData->w_ptr)
		return v;

	pSData->is_finished = 1;
	return NULL;
}

/**
 * move the iterator forward to the next position in the sequence
 */
static void *seq_next_lock(struct seq_file *s, void *v, loff_t *pos)
{
	struct sw_logger_seq_data *pSData = v;

	if (pSData == NULL) {
		LOGGER_ERR("%s: pSData == NULL\n", __func__);
		return NULL;
	}

	LOGGER_INFO(
		"%s in, w_ptr = %d, r_ptr = %d, i = %d, overflow_flg = %d\n",
		__func__, pSData->w_ptr, pSData->r_ptr, pSData->i,
		pSData->overflow_flg);

	pSData->i = (pSData->i + LOG_LINE_MAX_LENS) % APU_LOG_SIZE;

	if (pSData->i != pSData->w_ptr)
		return v;

	iowrite32(pSData->i, LOG_R_PTR);
	return NULL;
}

/**
 * stop() is called when iteration is complete (clean up)
 */
static void seq_stop(struct seq_file *s, void *v)
{
	unsigned long flags;

	LOGGER_INFO("%s v = 0x%x\n", __func__, v);

	if (pSeqData != NULL) {
		if (pSeqData->is_finished == 1) {
			spin_lock_irqsave(&sw_logger_spinlock, flags);
			iowrite32(pSeqData->i, LOG_R_PTR);
			/* fixme: assume next overflow won't happen
			 * until next seq_start
			 */
			iowrite32(0, LOG_OV_FLG);
			spin_unlock_irqrestore(&sw_logger_spinlock, flags);

			if (v != NULL)
				kfree(v);
			else {
				LOGGER_INFO(" %s free v FAIL!\n", __func__, v);
				if (pSeqData != NULL) {
					LOGGER_INFO(
						"%s free pSeqData = 0x%x\n",
						__func__, pSeqData);
					kfree(pSeqData);
				} else
					LOGGER_ERR(
						"%s free pSeqData = 0x%x FAIL!\n",
						__func__, pSeqData);
			}
			pSeqData = NULL;
		}
	}
}

/**
 * stop() is called when iteration is complete (clean up)
 */
static void seq_stopl(struct seq_file *s, void *v)
{
	LOGGER_INFO("%s v = 0x%x\n", __func__, v);
}

/**
 * success return 0, otherwise return error code
 */
static int seq_show(struct seq_file *s, void *v)
{
	struct sw_logger_seq_data *pSData = v;
#ifdef SW_LOGGER_DEBUG
	unsigned int i;
#else
	static unsigned int prevIsBinary = 0;
#endif

	LOGGER_INFO("%s in", __func__);

#ifdef SW_LOGGER_DEBUG
	if ((sw_log_buf[pSData->i] == 0xA5) &&
		(sw_log_buf[pSData->i+1] == 0xA5)) {
		seq_printf(s, "dbglog[%d,%d,%d] = ",
			pSData->w_ptr, pSData->r_ptr, pSData->i);
		for (i=0; i<LOG_LINE_MAX_LENS; i++)
			seq_printf(s, "%02X", sw_log_buf + pSData->i + i);
		seq_printf(s, "\n");
	} else
		seq_printf(s, "dbglog[%d,%d,%d] = %s",
			pSData->w_ptr, pSData->r_ptr, pSData->i,
			(sw_log_buf + pSData->i));
#else
	if ((sw_log_buf[pSData->i] == 0xA5) &&
		(sw_log_buf[pSData->i+1] == 0xA5)) {
		prevIsBinary = 1;
		seq_write(s, sw_log_buf + pSData->i, LOG_LINE_MAX_LENS);
	}
	else {
		if (prevIsBinary)
			seq_printf(s, "\n");
		prevIsBinary = 0;
		seq_printf(s, "%s",
			sw_log_buf + pSData->i);
	}
#endif

	return 0;
}

static int seq_showl(struct seq_file *s, void *v)
{
	struct sw_logger_seq_data *pSData = v;
#ifdef SW_LOGGER_DEBUG
	unsigned int i;
#else
	static unsigned int prevIsBinary = 0;
#endif

	LOGGER_INFO("%s in: %s", __func__,
		sw_log_buf + pSData->i);

#ifdef SW_LOGGER_DEBUG
	if (pSData->i != pSData->w_ptr) {
		if ((sw_log_buf[pSData->i] == 0xA5) &&
			(sw_log_buf[pSData->i+1] == 0xA5)) {
			seq_printf(s, "dbglog[%d,%d,%d] = ",
				pSData->w_ptr, pSData->r_ptr, pSData->i);
			for (i=0; i<LOG_LINE_MAX_LENS; i++)
				seq_printf(s, "%02X", sw_log_buf + pSData->i + i);
			seq_printf(s, "\n");
		} else
			seq_printf(s, "dbglog[%d,%d,%d] = %s",
				pSData->w_ptr, pSData->r_ptr, pSData->i,
				(sw_log_buf + pSData->i));
	}
#else
	if (pSData->i != pSData->w_ptr) {
		if ((sw_log_buf[pSData->i] == 0xA5) &&
			(sw_log_buf[pSData->i+1] == 0xA5)) {
			prevIsBinary = 1;
			seq_write(s, sw_log_buf + pSData->i, LOG_LINE_MAX_LENS);
		}
		else {
			if (prevIsBinary)
				seq_printf(s, "\n");
			prevIsBinary = 0;
			seq_printf(s, "%s",
				sw_log_buf + pSData->i);
		}
	}
#endif

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
	.next  = seq_next_lock,
	.stop  = seq_stopl,
	.show  = seq_showl
};

static int debug_sqopen_lock(struct inode *inode, struct file *file)
{
	return seq_open(file, &seq_ops_lock);
}

static int debug_sqopen(struct inode *inode, struct file *file)
{
	return seq_open(file, &seq_ops);
}

static const struct proc_ops sw_loggerSeqLog_ops = {
	.proc_open    = debug_sqopen,
	.proc_read    = seq_read,    // system
	.proc_lseek  = seq_lseek,   // system
	.proc_release = seq_release  // system
};

static const struct proc_ops sw_loggerSeqLogL_ops = {
	.proc_open    = debug_sqopen_lock,
	.proc_read    = seq_read,    // system
	.proc_lseek  = seq_lseek,   // system
	.proc_release = seq_release  // system
};

/* must ensure uP no longer print log */
static ssize_t apusys_log_dump(struct file *filep,
		struct kobject *kobj, struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;
	uint32_t w_ptr, r_ptr, overflow_flg, i, print_sz;
	unsigned long flags;

	spin_lock_irqsave(&sw_logger_spinlock, flags);
	w_ptr = ioread32(LOG_W_PTR);
	r_ptr = ioread32(LOG_R_PTR);
	overflow_flg = ioread32(LOG_OV_FLG);
	iowrite32(0, LOG_OV_FLG);
	spin_unlock_irqrestore(&sw_logger_spinlock, flags);

	sw_logger_buf_invalidate();

	if (w_ptr == r_ptr && overflow_flg == 0)
		return length;

	if (overflow_flg == 0)
		i = r_ptr;
	else
		i = w_ptr;

	do {
		print_sz = (strlen(sw_log_buf + i));
		if ((length + print_sz) <= size) {
			scnprintf(buf + length, print_sz, "%s", sw_log_buf + i);
			/* replace trailing null character with new line
			 * for log readability
			 */
			buf[length + print_sz - 1] = '\n';
		} else
			break;
		length += print_sz;
		i = (i + LOG_LINE_MAX_LENS) % APU_LOG_SIZE;
		iowrite32(i, LOG_R_PTR);
	} while (i != w_ptr);

	return length;
}

struct bin_attribute bin_attr_apusys_log = {
	.attr = {
		.name = "apusys_log.txt",
		.mode = 0444,
	},
	.size = 0,
	.read = apusys_log_dump,
};

static int sw_logger_create_sysfs(struct device *dev)
{
	int ret = 0;

	/* create /sys/kernel/apusys_logger */
	root_dir = kobject_create_and_add("apusys_logger", kernel_kobj);
	if (!root_dir) {
		pr_info("%s kobject_create_and_add fail for apusys_logger, ret %d\n",
			__func__, ret);
		return -EINVAL;
	}

	ret = sysfs_create_bin_file(root_dir, &bin_attr_apusys_log);
	if (ret)
		pr_info("%s sysfs create fail for apusys_log, ret %d\n",
			__func__, ret);

	return ret;
}

static void sw_logger_remove_sysfs(struct device *dev)
{
	sysfs_remove_bin_file(root_dir, &bin_attr_apusys_log);
}

static void sw_logger_remove_procfs(struct device *dev)
{
	remove_proc_entry("log", log_root);
	remove_proc_entry("seq_log", log_root);
	remove_proc_entry("seq_logl", log_root);
	remove_proc_entry("attr", log_root);
	remove_proc_entry(APUSYS_LOGGER_DIR, NULL);
}

static int sw_logger_create_procfs(struct device *dev)
{
	int ret = 0;

	log_root = proc_mkdir(APUSYS_LOGGER_DIR, NULL);
	ret = IS_ERR_OR_NULL(log_root);
	if (ret) {
		LOGGER_ERR("(%d)failed to create apusys_logger dir\n", ret);
		goto out;
	}

	/* create device table info */
	log_devinfo = proc_create("log", 0444,
		log_root, &apusys_debug_fops);
	ret = IS_ERR_OR_NULL(log_devinfo);
	if (ret) {
		LOGGER_ERR("(%d)failed to create apusys_logger node(devinfo)\n",
			ret);
		goto out;
	}

	log_seqlog = proc_create("seq_log", 0444,
		log_root, &sw_loggerSeqLog_ops);
	ret = IS_ERR_OR_NULL(log_seqlog);
	if (ret) {
		LOGGER_ERR("(%d)failed to create apusys_logger node(seqlog)\n",
			ret);
		goto out;
	}

	log_seqlogL = proc_create("seq_logl", 0444,
		log_root, &sw_loggerSeqLogL_ops);
	ret = IS_ERR_OR_NULL(log_seqlogL);
	if (ret) {
		LOGGER_ERR("(%d)failed to create apusys_logger node(seqlogL)\n",
			ret);
		goto out;
	}

	log_devattr = proc_create("attr", 0444,
		log_root, &sw_logger_attr_fops);

	ret = IS_ERR_OR_NULL(log_devattr);
	if (ret) {
		LOGGER_ERR(
			"(%d)failed to create apusys_logger attr node(devinfo)\n",
			ret);
		goto out;
	}

	return 0;

out:
	sw_logger_remove_procfs(dev);
	return ret;
}

static int sw_logger_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	int ret = 0;

	pr_info("%s in", __func__);

	sw_logger_dev = dev;

	spin_lock_init(&sw_logger_spinlock);

	ret = sw_logger_create_procfs(dev);
	if (ret) {
		LOGGER_ERR("%s: sw_logger_create_procfs fail\n", __func__);
		goto remove_procfs;
	}

	ret = sw_logger_create_sysfs(dev);
	if (ret) {
		LOGGER_ERR("%s: sw_logger_create_sysfs fail\n", __func__);
		goto remove_sysfs;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apu_mbox");
	if (res == NULL) {
		pr_info("%s: apu_mbox get resource fail\n", __func__);
		ret = -ENODEV;
		goto remove_ioremap;
	}
	apu_mbox = ioremap(res->start, res->end - res->start + 1);
	if (IS_ERR((void const *)apu_mbox)) {
		pr_info("%s: apu_mbox remap base fail\n", __func__);
		ret = -ENOMEM;
		goto remove_ioremap;
	}

	ret = sw_logger_buf_alloc(dev);
	if (ret) {
		LOGGER_ERR("%s: sw_logger_buf_alloc fail\n", __func__);
		goto remove_sw_log_buf;
	}

	pr_info("apu_sw_logger probe done, sw_log_buf= 0x%p\n",
		sw_log_buf);

	return 0;

remove_sw_log_buf:
	kfree(sw_log_buf);

remove_ioremap:
	if (apu_mbox != NULL)
		iounmap(apu_mbox);

remove_sysfs:
	sw_logger_remove_sysfs(dev);

remove_procfs:
	sw_logger_remove_procfs(dev);

	LOGGER_ERR("apu_sw_logger probe error!!\n");

	return ret;
}

static int sw_logger_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sw_logger_remove_procfs(dev);
	sw_logger_remove_sysfs(dev);
	dma_unmap_single(dev, handle, APU_LOG_SIZE, DMA_FROM_DEVICE);
	kfree(sw_log_buf);

	return 0;
}

static const struct of_device_id apusys_sw_logger_of_match[] = {
	{ .compatible = "mediatek,apusys_sw_logger"},
	{},
};

MODULE_DEVICE_TABLE(of, apusys_sw_logger_of_match);

static struct platform_driver sw_logger_driver = {
	.probe = sw_logger_probe,
	.remove = sw_logger_remove,
	.driver = {
		.name = SW_LOGGER_DEV_NAME,
		.of_match_table = of_match_ptr(apusys_sw_logger_of_match),
	}
};

int sw_logger_init(void)
{
	int ret = 0;

	pr_info("%s in", __func__);

	allow_signal(SIGKILL);

	sw_log_buf = NULL;
	g_sw_logger_log_lv = DEBUG_LOG_WARN;
	startl_first_enter_session = true;

	ret = platform_driver_register(&sw_logger_driver);
	if (ret != 0) {
		LOGGER_ERR("failed to register sw_logger driver");
		return -ENODEV;
	}

	return ret;
}

void sw_logger_exit(void)
{
	disallow_signal(SIGKILL);
	platform_driver_unregister(&sw_logger_driver);
}
