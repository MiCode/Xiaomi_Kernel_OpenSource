/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/of.h>
#include <linux/io.h>
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/delay.h>
#include <linux/kthread.h>

#ifdef CONFIG_MEDIATEK_EMI
#include <mt-plat/sync_write.h>
#include <memory/mediatek/emi.h>
#include "mcupm_emi_mpu.h"
#endif

#include "mcupm_ipi_id.h"
#include "mcupm_ipi_table.h"
#include "mcupm_driver.h"
#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>
#define MCUPM_MEM_RESERVED_KEY "mediatek,reserve-memory-mcupm_share"
#endif

/* MCUPM RESERVED MEM */
static phys_addr_t mcupm_mem_base_phys;
static phys_addr_t mcupm_mem_base_virt;
static phys_addr_t mcupm_mem_size;

#ifdef CONFIG_MEDIATEK_EMI
static unsigned long long mcupm_start;
static unsigned long long mcupm_end;
#endif

/* MCUPM PLT */
#if MCUPM_PLT_SERV_SUPPORT
struct plt_ctrl_s {
	unsigned int magic;
	unsigned int size;
	unsigned int mem_sz;
#if MCUPM_LOGGER_SUPPORT
	unsigned int logger_ofs;
#endif
};
#endif

/* MCUPM HELPER */
struct platform_device *mcupm_pdev;
int mcupm_plt_ackdata;
static atomic_t mcupm_inited = ATOMIC_INIT(0);
static atomic_t mcupm_dev_inited = ATOMIC_INIT(0);
static unsigned int mcupm_ready;
spinlock_t mcupm_mbox_lock[MCUPM_MBOX_TOTAL];

/* MCUPM SYSFS */
static wait_queue_head_t logwait;

/* MCUPM LOGGER */
static unsigned int mcupm_logger_inited;
static struct log_ctrl_s *log_ctl;
static struct buffer_info_s *buf_info, *lbuf_info;
static struct timer_list mcupm_log_timer;
static DEFINE_MUTEX(mcupm_log_mutex);

static struct mcupm_reserve_mblock mcupm_reserve_mblock[NUMS_MCUPM_MEM_ID] = {
	{
		.num = MCUPM_MEM_ID,
		.size = 0x100 + MCUPM_PLT_LOGGER_BUF_LEN,
		/* logger header + 1M log buffer */
	},
#if !defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) && !defined(CONFIG_MTK_MET_MEM_ALLOC)
	{
		.num = MCUPM_MET_ID,
		.size = MCUPM_MET_LOGGER_BUF_LEN,
	},
#endif
	{
		.num = MCUPM_EEMSN_MEM_ID,
		.size = MCUPM_PLT_EEMSN_BUF_LEN,
	},
	{
		.num = MCUPM_BRISKET_ID,
		.size = MCUPM_BRISKET_BUF_LEN,
	},
};

/* MCUPM RESERVED MEM */
#ifdef CONFIG_OF_RESERVED_MEM
static int __init mcupm_reserve_mem_of_init(struct reserved_mem *rmem)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size = 0;

	mcupm_mem_base_phys = (phys_addr_t) rmem->base;
	mcupm_mem_size = (phys_addr_t) rmem->size;

	pr_debug("[MCUPM] phys:0x%llx - 0x%llx (0x%llx)\n",
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->base +
		 (unsigned long long)rmem->size,
		 (unsigned long long)rmem->size);
	accumlate_memory_size = 0;
	for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
		mcupm_reserve_mblock[id].start_phys = mcupm_mem_base_phys +
			accumlate_memory_size;
		accumlate_memory_size += mcupm_reserve_mblock[id].size;

		pr_debug("[MCUPM][reserve_mem:%d]: ", id);
		pr_debug("phys:0x%llx - 0x%llx (0x%llx)\n",
			 mcupm_reserve_mblock[id].start_phys,
			 mcupm_reserve_mblock[id].start_phys +
			 mcupm_reserve_mblock[id].size,
			 mcupm_reserve_mblock[id].size);
	}
	return 0;
}

RESERVEDMEM_OF_DECLARE(mcupm_reservedmem, MCUPM_MEM_RESERVED_KEY,
			mcupm_reserve_mem_of_init);
#endif

phys_addr_t mcupm_reserve_mem_get_phys(unsigned int id)
{
	if (id >= NUMS_MCUPM_MEM_ID) {
		pr_debug("[MCUPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return mcupm_reserve_mblock[id].start_phys;
}
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_phys);

phys_addr_t mcupm_reserve_mem_get_virt(unsigned int id)
{
	if (id >= NUMS_MCUPM_MEM_ID) {
		pr_debug("[MCUPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return mcupm_reserve_mblock[id].start_virt;
}
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_virt);

phys_addr_t mcupm_reserve_mem_get_size(unsigned int id)
{
	if (id >= NUMS_MCUPM_MEM_ID) {
		pr_debug("[MCUPM] no reserve memory for 0x%x", id);
		return 0;
	} else
		return mcupm_reserve_mblock[id].size;
}
EXPORT_SYMBOL_GPL(mcupm_reserve_mem_get_size);

int mcupm_reserve_memory_init(void)
{
	unsigned int id;
	phys_addr_t accumlate_memory_size;

	if (NUMS_MCUPM_MEM_ID == 0)
		return 0;

	if (mcupm_mem_base_phys == 0)
		return -1;

	accumlate_memory_size = 0;
	mcupm_mem_base_virt = (phys_addr_t)(uintptr_t)
		ioremap_wc(mcupm_mem_base_phys, mcupm_mem_size);

	pr_debug("[MCUPM]reserve mem: virt:0x%llx - 0x%llx (0x%llx)\n",
		 (unsigned long long)mcupm_mem_base_virt,
		 (unsigned long long)mcupm_mem_base_virt +
		 (unsigned long long)mcupm_mem_size,
		 (unsigned long long)mcupm_mem_size);

	for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
		mcupm_reserve_mblock[id].start_virt = mcupm_mem_base_virt +
			accumlate_memory_size;
		accumlate_memory_size += mcupm_reserve_mblock[id].size;
	}

	WARN_ON(accumlate_memory_size > mcupm_mem_size);
#ifdef MCUPM_RESERVED_DEBUG
	for (id = 0; id < NUMS_MCUPM_MEM_ID; id++) {
		pr_debug("[MCUPM][mem_reserve-%d] ", id);
		pr_debug("phys:0x%llx,virt:0x%llx,size:0x%llx\n",
			 (unsigned long long)mcupm_reserve_mem_get_phys(id),
			 (unsigned long long)mcupm_reserve_mem_get_virt(id),
			 (unsigned long long)mcupm_reserve_mem_get_size(id));
	}
#endif

	return 0;
}

/* MCUPM SYSFS */
static ssize_t mcupm_log_if_read(struct file *file, char __user *data,
				 size_t len, loff_t *ppos)
{
	ssize_t ret;

	/* pr_debug("[MCUPM] mcupm_log_if_read\n"); */

	ret = 0;

	if (access_ok(VERIFY_WRITE, data, len))
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

int __init mcupm_sysfs_init(void)
{
	int ret;

	init_waitqueue_head(&logwait);

	ret = misc_register(&mcupm_log_device);

	if (unlikely(ret != 0))
		return ret;

	return 0;
}

int mcupm_sysfs_create_file(const struct device_attribute *attr)
{
	return device_create_file(mcupm_log_device.this_device, attr);
}

/* MCUPM LOGGER */
static inline void mcupm_log_timer_add(void)
{
	if (mcupm_log_timer.expires == 0) {
		mcupm_log_timer.expires = jiffies + MCUPM_TIMER_TIMEOUT;
		add_timer(&mcupm_log_timer);
	}
}

static void mcupm_log_timeout(unsigned long data)
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

static unsigned int mcupm_log_enable_set(unsigned int enable)
{
	struct mcupm_ipi_data_s ipi_data;
	int ret;

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

DEVICE_ATTR(mcupm_mobile_log, 0644, mcupm_mobile_log_show,
	mcupm_mobile_log_store);

unsigned int __init mcupm_logger_init(phys_addr_t start, phys_addr_t limit)
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

int __init mcupm_logger_init_done(void)
{
	int ret;

	if (log_ctl) {
		ret = mcupm_sysfs_create_file(&dev_attr_mcupm_mobile_log);

		if (unlikely(ret != 0))
			return ret;

		setup_timer(&mcupm_log_timer, &mcupm_log_timeout, 0);
		mcupm_log_timer.expires = 0;

		mcupm_logger_inited = 1;
	}

	return 0;
}

/* MCUPM PLT */
#if MCUPM_PLT_SERV_SUPPORT
static ssize_t mcupm_alive_show(struct device *kobj,
				struct device_attribute *attr, char *buf)
{

	struct  mcupm_ipi_data_s ipi_data;
	int ret;

	ipi_data.cmd = 0xDEAD;
	mcupm_plt_ackdata = 0;

	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_PLATFORM, IPI_SEND_WAIT,
		&ipi_data,
		sizeof(struct mcupm_ipi_data_s) / MCUPM_MBOX_SLOT_SIZE,
		2000);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			mcupm_plt_ackdata ? "Alive" : "Dead");
}
DEVICE_ATTR(mcupm_alive, 0444, mcupm_alive_show, NULL);

int __init mcupm_plt_init(void)
{
	phys_addr_t phys_addr, virt_addr, mem_sz;
	struct mcupm_ipi_data_s ipi_data;
	struct plt_ctrl_s *plt_ctl;
	int ret, i;
	unsigned int last_ofs;
#if MCUPM_LOGGER_SUPPORT
	unsigned int last_sz;
#endif
	unsigned int *mark;
	unsigned char *b;

	ret = mcupm_sysfs_create_file(&dev_attr_mcupm_alive);
	if (unlikely(ret != 0))
		goto error;

	phys_addr = mcupm_reserve_mem_get_phys(MCUPM_MEM_ID);
	if (phys_addr == 0) {
		pr_debug("MCUPM: Can't get logger phys mem\n");
		goto error;
	}

	virt_addr = mcupm_reserve_mem_get_virt(MCUPM_MEM_ID);
	if (virt_addr == 0) {
		pr_debug("MCUPM: Can't get logger virt mem\n");
		goto error;
	}

	mem_sz = mcupm_reserve_mem_get_size(MCUPM_MEM_ID);
	if (mem_sz == 0) {
		pr_debug("MCUPM: Can't get logger mem size\n");
		goto error;
	}

	b = (unsigned char *) (uintptr_t)virt_addr;
	for (last_ofs = 0; last_ofs < sizeof(*plt_ctl); last_ofs++)
		b[last_ofs] = 0x0;

	mark = (unsigned int *) (uintptr_t)virt_addr;
	*mark = MCUPM_PLT_INIT;
	mark = (unsigned int *) ((unsigned char *) (uintptr_t)
				 virt_addr + mem_sz - 4);
	*mark = MCUPM_PLT_INIT;

	plt_ctl = (struct plt_ctrl_s *) (uintptr_t)virt_addr;
	plt_ctl->magic = MCUPM_PLT_INIT;
	plt_ctl->size = sizeof(*plt_ctl);
	plt_ctl->mem_sz = mem_sz;

	last_ofs = plt_ctl->size;


	pr_debug("MCUPM: %s(): after plt, ofs=0x%x\n", __func__, last_ofs);

#if MCUPM_LOGGER_SUPPORT
	plt_ctl->logger_ofs = last_ofs;
	last_sz = mcupm_logger_init(virt_addr + last_ofs, mem_sz - last_ofs);


	if (last_sz == 0) {
		pr_debug("MCUPM: mcupm_logger_init return fail\n");
		goto error;
	}

	last_ofs += last_sz;
	pr_debug("MCUPM: %s(): after logger, ofs=0x%x\n", __func__, last_ofs);
#endif

	ipi_data.cmd = MCUPM_PLT_INIT;
	ipi_data.u.ctrl.phys = phys_addr;
	ipi_data.u.ctrl.size = mem_sz;
	mcupm_plt_ackdata = 0;

	ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_PLATFORM, IPI_SEND_POLLING,
		&ipi_data,
		sizeof(struct mcupm_ipi_data_s) / MCUPM_MBOX_SLOT_SIZE,
		2000);
	if (ret) {
		pr_debug("MCUPM: plt IPI fail ret=%d, ackdata=%d\n",
			ret, mcupm_plt_ackdata);
		goto error;
	}

	if (!mcupm_plt_ackdata) {
		pr_debug("MCUPM: plt IPI init fail, ackdata=%d\n",
		       mcupm_plt_ackdata);
		goto error;
	}

	pr_info("MCUPM: plt IPI success ret=%d, ackdata=%d\n",
		ret, mcupm_plt_ackdata);

#if MCUPM_LOGGER_SUPPORT
	mcupm_logger_init_done();
#endif

	for (i = 0; i < MCUPM_MBOX_TOTAL; i++)
		spin_lock_init(&mcupm_mbox_lock[i]);

	return 0;
error:
	return -1;
}
#endif

/* MCUPM HELPER */
int mcupm_mbox_read(unsigned int mbox, unsigned int slot, void *buf,
			unsigned int len)
{
	if (WARN_ON(len > (mcupm_mboxdev.pin_send_table[mbox]).msg_size)) {
		pr_debug("mbox:%u warning\n", mbox);
		return -EINVAL;
	}

	return mtk_mbox_read(&mcupm_mboxdev, mbox, slot,
				buf, len * MBOX_SLOT_SIZE);
}

int mcupm_mbox_write(unsigned int mbox, unsigned int slot, void *buf,
			unsigned int len)
{
	unsigned long flags;
	unsigned int status;
	int ret;

	if (WARN_ON(len > (mcupm_mboxdev.pin_send_table[mbox]).msg_size) ||
		WARN_ON(!buf)) {
		pr_debug("mbox:%u warning\n", mbox);
		return -EINVAL;
	}

	spin_lock_irqsave(&mcupm_mbox_lock[mbox], flags);
	status = mtk_mbox_check_send_irq(&mcupm_mboxdev, mbox,
				(mcupm_mboxdev.pin_send_table[mbox]).pin_index);
	if (status != 0) {
		spin_unlock_irqrestore(&mcupm_mbox_lock[mbox], flags);
		return MBOX_PIN_BUSY;
	}
	spin_unlock_irqrestore(&mcupm_mbox_lock[mbox], flags);

	ret = mtk_mbox_write(&mcupm_mboxdev, mbox, slot
				, buf, len * MBOX_SLOT_SIZE);
	if (ret != MBOX_DONE)
		return ret;
	/* Ensure that all writes to SRAM are committed */
	mb();

	return 0;
}

static int mcupm_device_probe(struct platform_device *pdev)
{
	int i, ret;
	void __iomem *base;
	struct device *dev = &pdev->dev;
	struct resource *res;
	char name[32];

	if (atomic_inc_return(&mcupm_dev_inited) != 1)
		return -1;

	mcupm_pdev = pdev;

	pr_debug("[MCUPM] mbox probe\n");

	for (i = 0; i < MCUPM_MBOX_TOTAL; i++) {
		pr_info("[MCUPM]  mbox-%d, probe\n", i);
		mcupm_mbox_table[i].mbdev = &mcupm_mboxdev;
		if (i == 0) {
			ret = mtk_mbox_probe(pdev, mcupm_mbox_table[i].mbdev,
						i);
			if (ret) {
				pr_debug("[MCUPM] mbox(%d) probe fail on mbox-0, ret %d\n",
					i, ret);
				return -1;
			}
			continue;
		}
		snprintf(name, sizeof(name), "mbox%d_base", i);
		res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, name);
		base = devm_ioremap_resource(dev, res);
		if (IS_ERR((void const *) base))
			pr_debug("mbox-%d can't remap base\n", i);
		ret = mtk_smem_init(pdev, mcupm_mbox_table[i].mbdev, i, base,
		mcupm_mbox_table[0].mbdev->info_table[0].set_irq_reg,
		mcupm_mbox_table[0].mbdev->info_table[0].clr_irq_reg,
		mcupm_mbox_table[0].mbdev->info_table[0].send_status_reg,
		mcupm_mbox_table[0].mbdev->info_table[0].recv_status_reg);
		if (ret) {
			pr_debug("[MCUPM] mbox probe fail on mbox-%d, ret %d\n",
				i, ret);
			return -1;
		}
	}

	pr_debug("[MCUPM] ipi register\n");
	ret = mtk_ipi_device_register(&mcupm_ipidev, pdev, &mcupm_mboxdev,
				      MCUPM_IPI_COUNT);
	if (ret) {
		pr_debug("[MCUPM] ipi_dev_register fail, ret %d\n", ret);
		return -1;
	}

	ret = mtk_ipi_register(&mcupm_ipidev, CH_S_PLATFORM, NULL, NULL,
			       (void *) &mcupm_plt_ackdata);
	if (ret) {
		pr_debug("[MCUPM] ipi_register fail, ret %d\n", ret);
		return -1;
	}

	pr_info("MCUPM is ready to service IPI\n");

	return 0;
}

#if MCUPM_ALIVE_THREAD
static struct task_struct *mcupm_task;
int mcupm_thread(void *data)
{
	struct  mcupm_ipi_data_s ipi_data;
	int ret;

	ipi_data.cmd = 0xDEAD;
	mcupm_plt_ackdata = 0;

	/* an endless loop in which we are doing our work */
	do {
		ret = mtk_ipi_send_compl(&mcupm_ipidev, CH_S_PLATFORM,
			IPI_SEND_WAIT, &ipi_data,
			sizeof(struct mcupm_ipi_data_s) / MCUPM_MBOX_SLOT_SIZE,
			2000);
		if (ret) {
			pr_info("MCUPM: alive ret=%d, ackdata=%d\n",
				ret, mcupm_plt_ackdata);
		} else {
			pr_info("MCUPM is %s\n",
				mcupm_plt_ackdata ? "Alive" : "Dead");
		}
		msleep(20000);
	} while (!kthread_should_stop());
	return 0;
}
#endif

static const struct of_device_id mcupm_of_match[] = {
	{ .compatible = "mediatek,mcupm", },
	{},
};

static const struct platform_device_id mcupm_id_table[] = {
	{ "mcupm", 0},
	{ },
};

static struct platform_driver mtk_mcupm_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.probe = mcupm_device_probe,
	.driver = {
		.name = "mcupm",
		.owner = THIS_MODULE,
		.of_match_table = mcupm_of_match,
	},
	.id_table = mcupm_id_table,
};

static int __init mcupm_init(void)
{
	if (atomic_inc_return(&mcupm_inited) != 1)
		return 0;

	/* static initialise */
	mcupm_ready = 0;

	if (platform_driver_register(&mtk_mcupm_driver)) {
		pr_debug("[MCUPM] Device Init Failed\n");
		goto error;
	}

#ifdef CONFIG_OF_RESERVED_MEM
	if (mcupm_reserve_memory_init()) {
		pr_debug("[MCUPM] Reserved Memory Failed\n");
		goto error;
	}
#endif

#if MCUPM_ALIVE_THREAD
	mcupm_task = kthread_run(mcupm_thread, NULL,
					"mcupm_task");
#endif

	pr_debug("[MCUPM] Helper Init\n");

	mcupm_ready = 1;

	atomic_set(&mcupm_inited, 1);
	return 0;

error:
	atomic_set(&mcupm_inited, 1);
	return -1;
}

#ifdef CONFIG_MEDIATEK_EMI
static void mcupm_set_emi_mpu(phys_addr_t base, phys_addr_t size)
{
	mcupm_start = base;
	mcupm_end = base + size - 1;
}

static void mcupm_lock_emi_mpu(void)
{
	if (mcupm_mem_size > 0)
		mcupm_set_emi_mpu(mcupm_mem_base_phys, mcupm_mem_size);
}
#endif

static int __init mcupm_module_init(void)
{
	if (mcupm_sysfs_init()) {
		pr_debug("[MCUPM] Sysfs Init Failed\n");
		return -1;
	}

#if MCUPM_PLT_SERV_SUPPORT
	if (mcupm_plt_init()) {
		pr_debug("[MCUPM] Platform Init Failed\n");
		return -1;
	}
	pr_info("MCUPM platform service is ready\n");
#endif

#ifdef CONFIG_MEDIATEK_EMI
	mcupm_lock_emi_mpu();
#endif
	return 0;
}


#ifdef CONFIG_MEDIATEK_EMI
static int __init post_mcupm_set_emi_mpu(void)
{
	struct emimpu_region_t rg_info;

	mtk_emimpu_init_region(&rg_info, MUCPM_MPU_REGION_ID);

	mtk_emimpu_set_addr(&rg_info, mcupm_start, mcupm_end);

	mtk_emimpu_set_apc(&rg_info, 0, MTK_EMIMPU_NO_PROTECTION);

	mtk_emimpu_set_apc(&rg_info, MUCPM_MPU_DOMAIN_ID,
						MTK_EMIMPU_NO_PROTECTION);

	mtk_emimpu_set_protection(&rg_info);

	mtk_emimpu_free_region(&rg_info);

	return 0;
}
#endif

arch_initcall(mcupm_init);
module_init(mcupm_module_init);

#ifdef CONFIG_MEDIATEK_EMI
late_initcall(post_mcupm_set_emi_mpu);
#endif

