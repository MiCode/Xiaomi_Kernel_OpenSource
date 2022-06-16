// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/uaccess.h>

#define ATF_LOG_RESERVED_MEMORY_KEY "mediatek,atf-log-reserved"
#define DEBUG_BUF_NAME_LEN 16
#define DEBUG_BUF_MAGIC (0xdb2ab1e8)
#define DEBUG_BUF_ENTRY_MAGIC (0xdbee4298)
#define RUNTIME_LOG_NAME "RUNTIME_LOG"

#define DEBUG_BUF_PROC_FOLDER_NAME              "atf_log"
#define DEBUG_BUF_ATF_LOGGER_PROC_NAME          "atf_log"
#define DEBUG_BUF_ATF_LOGGER_SNAPSHOT_PROC_NAME "runtime_snapshot"
#define DEBUG_BUF_TOTAL_RAW_PROC_NAME           "raw_dump_all"

#define MAGIC_RELEASE_LOG 0x5678

#if CONFIG_OF
static const struct of_device_id tfa_debug_of_ids[] = {
	{ .compatible = "mediatek,tfa_debug", },
	{},
};
#endif

/*
 * |--------------------------|
 * |  debug_buf_table_header  |
 * |--------------------------|
 * |    debug_buf_entry 0     |
 * |--------------------------|
 * |..........................|
 * |--------------------------|
 * |    debug_buf_entry N     |
 * |--------------------------|
 * |    debug_buf_hdr 0       |
 * |--------------------------|
 * |    debug buffer body 0   |
 * |--------------------------|
 * |..........................|
 * |--------------------------|
 * |    debug_buf_hdr N       |
 * |--------------------------|
 * |    debug buffer body N   |
 * |--------------------------|
 */
struct debug_buf_table_header {
	uint32_t magic;
	/* Structure may change*/
	uint32_t version;
	/*
	 * debug_buf_table_header + all debug_buf_entry +
	 * all debug buffer
	 */
	uint32_t total_size;
	/* sizeof(debug_buf_table_header) */
	uint32_t hdr_size;
	/* sizeof(debug_buf_entry) */
	uint32_t debug_buf_entry_size;
	/* number of debug_buf_entry */
	uint32_t debug_buf_entry_count;
	/*
	 * Offset to the first debug_buf_entry from head of
	 * debug_buf_table_header
	 */
	uint32_t debug_buf_entries_offset;
	uint32_t alignment;
};

struct debug_buf_hdr {
	uint32_t magic;
	char debug_buf_name[DEBUG_BUF_NAME_LEN];
	uint32_t debug_buf_size;
	uint32_t w_offset;
	uint32_t r_offset;
	uint32_t flag;
};

struct tfa_debug_entry_info {
	struct debug_buf_hdr *buf_hdr_p;
	void *body_head;
	uint32_t debug_buf_size;
};

struct tfa_debug_info {
	struct proc_dir_entry *proc_dir;
	phys_addr_t debug_buf_paddr;
	phys_addr_t total_size;
	void __iomem *vaddr;
	/* Debug buffer entry */
	struct tfa_debug_entry_info runtime;
	atomic_t release_log;
};

struct tfa_debug_instance {
	void __iomem *vaddr;
	phys_addr_t debug_buf_size;
	size_t read_offset;
	struct debug_buf_hdr *debug_buf_hdr_p;
	struct wait_queue_head waitq;
};
static struct tfa_debug_info info;

static int is_debug_buf_info_valid(void)
{
	if (info.debug_buf_paddr == 0)
		return 0;
	if (info.total_size == 0)
		return 0;
	if (IS_ERR(info.vaddr))
		return 0;
	return 1;
}

static int generic_open(struct inode *inode, struct file *file)
{
	int ret;
	struct tfa_debug_instance *inst_p = NULL;

	ret = nonseekable_open(inode, file);
	if (unlikely(ret))
		return ret;
	inst_p = kmalloc(sizeof(struct tfa_debug_instance),
		GFP_KERNEL);
	if (IS_ERR(inst_p)) {
		file->private_data = NULL;
		pr_notice("Fail to kmalloc:%ld\n", PTR_ERR(inst_p));
		return -ENOMEM;
	}
	init_waitqueue_head(&inst_p->waitq);
	file->private_data = inst_p;

	pr_info("%s %u process name:%s, pid:%d\n",
		__func__, __LINE__, current->comm,
		task_pid_nr(current));
	return 0;
}

static int raw_open(struct inode *inode, struct file *file)
{
	int ret;
	struct tfa_debug_instance *inst_p;

	ret = generic_open(inode, file);
	if (ret) {
		pr_notice("[%s] Fail to open proc file\n", __func__);
		return ret;
	}
	inst_p = (struct tfa_debug_instance *)file->private_data;
	inst_p->vaddr = info.vaddr;
	inst_p->debug_buf_size = info.total_size;
	inst_p->read_offset = 0;

	return 0;
}

static int raw_release(struct inode *node, struct file *file)
{
	pr_info("%s:vaddr:0x%llx\n", __func__, (uint64_t)file->private_data);
	if (file->private_data) {
		struct tfa_debug_instance *inst_p = file->private_data;

		kfree(inst_p);
		file->private_data = NULL;
	}
	return 0;
}

static ssize_t raw_read(struct file *file,
	char __user *buf, size_t count, loff_t *ppos)
{
	struct tfa_debug_instance *inst_p =
		(struct tfa_debug_instance *)file->private_data;
	size_t remain_len = inst_p->debug_buf_size - inst_p->read_offset;
	size_t copy_len;

	if (IS_ERR(info.vaddr))
		return -ENOMEM;

	if (count > remain_len)
		copy_len = remain_len;
	else
		copy_len = count;
	if (copy_to_user(buf,
		(void *)(inst_p->vaddr + inst_p->read_offset), copy_len))
		return -EFAULT;

	/* Update the read pos */
	inst_p->read_offset += copy_len;
	pr_info("vaddr:0x%llx, count:%zu, read offset:%zu\n",
		(uint64_t)inst_p->vaddr, count, inst_p->read_offset);
	return copy_len;
}

static const struct proc_ops tfa_debug_raw_fops = {
	.proc_open = raw_open,
	.proc_read = raw_read,
	.proc_release = raw_release,
	.proc_lseek = no_llseek,
};

static int runtime_snap_open(struct inode *inode, struct file *file)
{
	int ret;
	struct tfa_debug_instance *inst_p;

	ret = generic_open(inode, file);
	if (ret) {
		pr_notice("[%s] Fail to open proc file\n", __func__);
		return ret;
	}
	inst_p = (struct tfa_debug_instance *)file->private_data;
	inst_p->vaddr = info.runtime.body_head;
	inst_p->debug_buf_size = info.runtime.debug_buf_size;
	inst_p->read_offset = 0;

	return 0;
}

static const struct proc_ops tfa_debug_runtime_snap_fops = {
	.proc_open = runtime_snap_open,
	.proc_read = raw_read,
	.proc_release = raw_release,
	.proc_lseek = no_llseek,
};

static int is_runtime_empty_for_read(struct file *file)
{
	struct tfa_debug_instance *inst_p =
		(struct tfa_debug_instance *)file->private_data;
	struct debug_buf_hdr *buf_hdr_p;

	if (IS_ERR(inst_p))
		return 1;
	buf_hdr_p = inst_p->debug_buf_hdr_p;
	if (IS_ERR(buf_hdr_p))
		return 1;
	return (inst_p->read_offset == buf_hdr_p->w_offset);
}

static int getc_for_read(struct file *file, char *p)
{
	char *body;
	struct tfa_debug_instance *inst_p =
		(struct tfa_debug_instance *)file->private_data;

	if (is_runtime_empty_for_read(file))
		return -1;
	body = inst_p->vaddr;
	if (inst_p->read_offset >= inst_p->debug_buf_size) {
		pr_notice("%s read offset is invalid, :%lu\n",
			__func__, inst_p->read_offset);
		inst_p->read_offset = 0;
	}
	*p = body[inst_p->read_offset];
	inst_p->read_offset = (inst_p->read_offset + 1) % inst_p->debug_buf_size;
	dmb(ish);
	return 0;
}

static ssize_t runtime_log_read(struct file *file,
	char __user *buf, size_t count, loff_t *ppos)
{
	int error = 0;
	size_t copy_len = 0;
	char c;
	struct tfa_debug_instance *inst_p = file->private_data;
	signed long wait_event_ret;

	while (1) {
		if ((file->f_flags & O_NONBLOCK) &&
			is_runtime_empty_for_read(file))
			return -EAGAIN;
		wait_event_ret = wait_event_timeout(inst_p->waitq,
			!is_runtime_empty_for_read(file), HZ);
		if (wait_event_ret != 0)
			break;
		if (atomic_read(&info.release_log))
			break;
	}

	while (copy_len < count) {
		if (getc_for_read(file, &c))
			break;
		error = __put_user(c, buf);
		if (error) {
			pr_notice("%s __put_user fail:%d\n", __func__, error);
			break;
		}
		buf++;
		copy_len++;
		dmb(ish);
	}
	if (!error)
		error = copy_len;

	return error;
}

static int runtime_log_open(struct inode *inode, struct file *file)
{
	int ret;
	struct tfa_debug_instance *inst_p;

	ret = generic_open(inode, file);
	if (ret) {
		pr_notice("[%s] Fail to open proc file\n", __func__);
		return ret;
	}
	inst_p = (struct tfa_debug_instance *)file->private_data;
	inst_p->vaddr = info.runtime.body_head;
	inst_p->debug_buf_size = info.runtime.debug_buf_size;
	inst_p->read_offset = 0;
	inst_p->debug_buf_hdr_p = info.runtime.buf_hdr_p;

	return 0;
}

static ssize_t runtime_log_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long ret = -1;
	unsigned long param = -1;
	struct arm_smccc_res res;

	if (count < 12) {
		/* use kstrtoul_from_user() instead of */
		/* copy_from_user() and kstrtoul() */
		ret = kstrtoul_from_user(buf, count, 16, &param);
	}

	pr_info("[%s]param:0x%lx, count:%zu, ret:%ld\n",
		__func__, param, count, ret);

	if (param == MAGIC_RELEASE_LOG)
		atomic_set(&info.release_log, 1);
	else
		atomic_set(&info.release_log, 0);

	if (!ret) {
		arm_smccc_smc(MTK_SIP_KERNEL_ATF_DEBUG,
			param, 0, 0, 0, 0, 0, 0, &res);
		*ppos = count;
	} else
		return 0;

	return count;
}

static unsigned int runtime_log_poll(struct file *file,
				poll_table *wait)
{
	unsigned int ret = 0;
	struct tfa_debug_instance *inst_p = file->private_data;

	poll_wait(file, &(inst_p->waitq), wait);

	if (!is_runtime_empty_for_read(file))
		ret = POLLIN | POLLRDNORM;

	return ret;
}

static const struct proc_ops tfa_debug_runtime_fops = {
	.proc_open = runtime_log_open,
	.proc_read = runtime_log_read,
	.proc_write = runtime_log_write,
	.proc_poll = runtime_log_poll,
	.proc_release = raw_release,
	.proc_lseek = no_llseek,
};

static int lookup_reserved_memory(void)
{
	struct device_node *tfa_debug_node;
	struct reserved_mem *rmem;

	tfa_debug_node = of_find_compatible_node(NULL, NULL,
			ATF_LOG_RESERVED_MEMORY_KEY);

	if (!tfa_debug_node) {
		pr_info("compatible:%s not found\n", ATF_LOG_RESERVED_MEMORY_KEY);
		return -EINVAL;
	}
	rmem = of_reserved_mem_lookup(tfa_debug_node);
	if (!rmem) {
		pr_info("atf_logger reserved mem not found\n");
		return -EINVAL;
	}

	info.debug_buf_paddr = rmem->base;
	info.total_size = rmem->size;

	pr_info("%s, start: 0x%llx, size: 0x%llx\n",
		ATF_LOG_RESERVED_MEMORY_KEY,
		info.debug_buf_paddr, info.total_size);
	if ((info.debug_buf_paddr == 0) ||
		(info.total_size == 0)) {
		pr_notice("debug buf physical addr or size is zero:0x%llx 0x%llx\n",
		info.debug_buf_paddr, info.total_size);
		return -EINVAL;
	}
	/* remap reserved memory as cacheale */
	info.vaddr = ioremap(info.debug_buf_paddr, info.total_size);
	if (IS_ERR(info.vaddr)) {
		pr_notice("Fail to remap debug buf vaddr:%ld\n",
			PTR_ERR(info.vaddr));
		return -ENOMEM;
	}
	pr_info("debug buf vaddr:0x%llx\n", (uint64_t)info.vaddr);
	return 0;
}

/* So far we only export runtime log */
static int lookup_runtime_log(void)
{
	struct debug_buf_table_header *debug_buf_table_p;
	struct debug_buf_hdr *debug_buf_hdr_p;
	/* The first debug_buf_hdr */
	u32 debug_buf_hdr_offset;
	u32 i;

	debug_buf_table_p = (struct debug_buf_table_header *)info.vaddr;
	if (debug_buf_table_p->magic != DEBUG_BUF_MAGIC) {
		pr_notice("The debug buf magic is invalid:0x%x\n", debug_buf_table_p->magic);
		return -EINVAL;
	}
	debug_buf_hdr_offset = debug_buf_table_p->hdr_size +
		(debug_buf_table_p->debug_buf_entry_size *
		debug_buf_table_p->debug_buf_entry_count);
	debug_buf_hdr_p = (struct debug_buf_hdr *)((char *)debug_buf_table_p +
		debug_buf_hdr_offset);
	for (i = 0 ; i < debug_buf_table_p->debug_buf_entry_count
			; i++) {
		if (debug_buf_hdr_p->magic != DEBUG_BUF_ENTRY_MAGIC) {
			pr_notice("Invalid debug buffer entry header:0x%x\n",
				debug_buf_hdr_p->magic);
			return -EINVAL;
		}
		if (strncmp(debug_buf_hdr_p->debug_buf_name,
			RUNTIME_LOG_NAME, DEBUG_BUF_NAME_LEN) == 0)
			break;

		debug_buf_hdr_p = (struct debug_buf_hdr *)
			((char *)debug_buf_hdr_p +
			debug_buf_hdr_p->debug_buf_size +
			sizeof(struct debug_buf_hdr));
	}
	if (i == debug_buf_table_p->debug_buf_entry_count) {
		pr_notice("Fail to find %s\n", RUNTIME_LOG_NAME);
		return -ENXIO;
	}
	info.runtime.buf_hdr_p = debug_buf_hdr_p;
	info.runtime.body_head = (void *)((char *)debug_buf_hdr_p +
							sizeof(struct debug_buf_hdr));
	info.runtime.debug_buf_size = debug_buf_hdr_p->debug_buf_size;
	return 0;
}

static int tfa_time_sync_suspend(struct device *dev)
{
	struct arm_smccc_res res;

	/* Notify tf-a logger to clear indicator */
	arm_smccc_smc(MTK_SIP_KERNEL_TIME_SYNC,
		0, 0, 1, 0, 0, 0, 0, &res);

	return 0;
}

static int tfa_time_sync_resume(struct device *dev)
{
	/* Get local_clock and sync to TF-A */
	u64 time_to_sync = local_clock();
	struct arm_smccc_res res;

	pr_info("[%s]time_to_sync:0x%llx\n", __func__, time_to_sync);
	/* Separate time_to_sync into two 32 bits args */
	arm_smccc_smc(MTK_SIP_KERNEL_TIME_SYNC,
		(u32)time_to_sync, (u32)(time_to_sync >> 32), 0, 0,
		0, 0, 0, &res);

	return 0;
}

static const struct dev_pm_ops tfa_pm_ops = {
	.suspend_noirq = tfa_time_sync_suspend,
	.resume_noirq = tfa_time_sync_resume,
};

static int __init tfa_debug_probe(struct platform_device *pdev)
{
	struct proc_dir_entry *runtime_log_snapshot_proc_file = NULL;
	struct proc_dir_entry *runtime_log_proc_file = NULL;
	struct proc_dir_entry *debug_raw_proc_file = NULL;
	int ret;

	pr_notice("%s\n", __func__);

	/* Synchronize timestamp in Kernel and ATF */
	tfa_time_sync_resume(NULL);
	ret = lookup_reserved_memory();
	if (ret) {
		pr_notice("Fail to look up atf_logger reserved memory:%d\n",
			ret);
		return ret;
	}
	info.proc_dir = proc_mkdir(DEBUG_BUF_PROC_FOLDER_NAME, NULL);
	if (info.proc_dir == NULL) {
		pr_info("tfa_debug proc_mkdir failed\n");
		return -ENOMEM;
	}
	atomic_set(&info.release_log, 0);
	if (lookup_runtime_log() == 0) {
		runtime_log_snapshot_proc_file =
			proc_create(DEBUG_BUF_ATF_LOGGER_SNAPSHOT_PROC_NAME, 0440,
			info.proc_dir, &tfa_debug_runtime_snap_fops);
		if (runtime_log_snapshot_proc_file == NULL) {
			pr_info("Fail to create %s proc file\n",
				DEBUG_BUF_ATF_LOGGER_SNAPSHOT_PROC_NAME);
			return -ENOMEM;
		}
		runtime_log_proc_file =
			proc_create(DEBUG_BUF_ATF_LOGGER_PROC_NAME, 0440,
			info.proc_dir, &tfa_debug_runtime_fops);
		if (runtime_log_proc_file == NULL) {
			pr_info("Fail to create %s proc file\n",
				DEBUG_BUF_ATF_LOGGER_PROC_NAME);
			return -ENOMEM;
		}
	}
	debug_raw_proc_file =
		proc_create(DEBUG_BUF_TOTAL_RAW_PROC_NAME, 0440,
		info.proc_dir, &tfa_debug_raw_fops);
	if (debug_raw_proc_file == NULL) {
		pr_info("Fail to create %s proc file\n",
			DEBUG_BUF_TOTAL_RAW_PROC_NAME);
		return -ENOMEM;
	}
	return 0;
}

static int tfa_debug_remove(struct platform_device *pdev)
{
	pr_notice("%s\n", __func__);
	if (!is_debug_buf_info_valid())
		return -EINVAL;
	if (info.proc_dir == NULL)
		return -ENOENT;
	remove_proc_entry(DEBUG_BUF_ATF_LOGGER_SNAPSHOT_PROC_NAME,
		info.proc_dir);
	remove_proc_entry(DEBUG_BUF_TOTAL_RAW_PROC_NAME,
		info.proc_dir);
	remove_proc_entry(DEBUG_BUF_PROC_FOLDER_NAME, NULL);
	iounmap(info.vaddr);
	return 0;
}

static struct platform_driver tfa_debug_driver_probe = {
	.probe = tfa_debug_probe,
	.remove = tfa_debug_remove,
	.driver = {
		.name = "tfa_debug",
		.owner = THIS_MODULE,
#if CONFIG_OF
		.of_match_table = tfa_debug_of_ids,
#endif
		.pm = &tfa_pm_ops,
	},
};

int tfa_debug_drv_register(void)
{
	int ret;

	pr_notice("%s\n", __func__);
	ret = platform_driver_register(&tfa_debug_driver_probe);
	if (ret)
		pr_notice("%s fail, ret 0x%x\n", __func__, ret);
	return ret;
}

int tfa_debug_drv_unregister(void)
{
	pr_notice("%s\n", __func__);
	platform_driver_unregister(&tfa_debug_driver_probe);
	return 0;
}
