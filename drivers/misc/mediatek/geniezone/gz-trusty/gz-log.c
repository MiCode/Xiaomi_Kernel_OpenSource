// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */
/*
 * This is log driver
 *
 * Memory log is supported - a shared memory is used for memory log
 * Driver provides read function for user-space debug apps getting log
 */

#include <linux/platform_device.h>
#include <gz-trusty/smcall.h>
#include <gz-trusty/trusty.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/log2.h>
#include <linux/sched.h>
#include <linux/sched/signal.h> /* Linux kernel 4.14 */
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <asm/page.h>
#include "gz-log.h"
#include <linux/of.h>

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>

/* NOTE: log_rb will be put at the begin of the memory buffer.
 * The actual data buffer size is
 * lower_power_of_2(TRUSTY_LOG_SIZE - sizeof(struct log_rb)).
 * If LOG_SIZE is PAGE_SIZE * power of 2, it will waste half of buffer.
 * so that, set the buffer size (power_of_2 + 1) PAGES.
 **/
#define TRUSTY_LOG_SIZE (PAGE_SIZE * 65)
#define TRUSTY_LINE_BUFFER_SIZE 256

struct gz_log_state {
	struct device *dev;
	struct device *trusty_dev;
	struct proc_dir_entry *proc;

	/*
	 * This lock is here to ensure only one consumer will read
	 * from the log ring buffer at a time.
	 */
	struct mutex lock;
	/* FIXME: extend struct log_rb to uint64_t */
	struct log_rb *log;
	uint32_t get;

	enum tee_id_t tee_id;
	struct notifier_block call_notifier;
	struct notifier_block panic_notifier;

	wait_queue_head_t gz_log_wq;
	atomic_t gz_log_event_count;
	atomic_t readable;
	int poll_event;
	char line_buffer[TRUSTY_LINE_BUFFER_SIZE];
};

struct gz_log_context {
	phys_addr_t paddr;
	void *virt;
	struct page *pages;
	size_t size;
	enum {DYNAMIC, STATIC} flag;

	struct gz_log_state *gls;
};

static struct gz_log_context glctx;

static int __init gz_log_context_init(struct reserved_mem *rmem)
{
	if (!rmem) {
		pr_info("[%s] ERROR: invalid reserved memory\n", __func__);
		return -EFAULT;
	}
	glctx.paddr = rmem->base;
	glctx.size = rmem->size;
	glctx.flag = STATIC;
	pr_info("[%s] rmem:%s base(0x%llx) size(0x%zx)\n",
		__func__, rmem->name, glctx.paddr, glctx.size);
	return 0;
}
RESERVEDMEM_OF_DECLARE(gz_log, "mediatek,gz-log", gz_log_context_init);

static int gz_log_page_init(void)
{
	if (glctx.virt)
		return 0;

	if (glctx.flag == STATIC) {
		glctx.virt = ioremap(glctx.paddr, glctx.size);

		if (!glctx.virt) {
			pr_info("[%s] ERROR: ioremap failed, use dynamic\n",
				__func__);
			glctx.flag = DYNAMIC;
			goto dynamic_alloc;
		}

		pr_info("[%s] set by static, virt addr:%p, sz:0x%zx\n",
			__func__, glctx.virt, glctx.size);
	} else {
dynamic_alloc:
		glctx.size = TRUSTY_LOG_SIZE;
		glctx.virt = kzalloc(glctx.size, GFP_KERNEL);

		if (!glctx.virt)
			return -ENOMEM;

		glctx.paddr = virt_to_phys(glctx.virt);
		pr_info("[%s] set by dynamic, virt:%p, sz:0x%zx\n",
			__func__, glctx.virt, glctx.size);
	}

	return 0;
}

/* get_gz_log_buffer was called in arch_initcall */
void get_gz_log_buffer(unsigned long *addr, unsigned long *paddr,
		       unsigned long *size, unsigned long *start)
{
	gz_log_page_init();

	if (!glctx.virt) {
		*addr = *paddr = *size = *start = 0;
		pr_info("[%s] ERR gz_log init failed\n", __func__);
		return;
	}
	*addr = (unsigned long)glctx.virt;
	*paddr = (unsigned long)glctx.paddr;
	pr_info("[%s] virtual address:0x%lx, paddr:0x%lx\n",
		__func__, (unsigned long)*addr, *paddr);
	*size = glctx.size;
	*start = 0;
}
EXPORT_SYMBOL(get_gz_log_buffer);

/* driver functions */
static int trusty_log_call_notify(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct gz_log_state *gls = container_of(nb, struct gz_log_state,
						call_notifier);

	if (action != TRUSTY_CALL_RETURNED)
		return NOTIFY_DONE;

	atomic_inc(&gls->gz_log_event_count);
	wake_up_interruptible(&gls->gz_log_wq);
	return NOTIFY_OK;
}

static int trusty_log_panic_notify(struct notifier_block *nb,
				   unsigned long action, void *data)
{
	struct gz_log_state *gls = container_of(nb, struct gz_log_state,
						panic_notifier);

	/*
	 * Don't grab the spin lock to hold up the panic notifier, even
	 * though this is racy.
	 */
	pr_info("trusty-log panic notifier - trusty version %s",
		trusty_version_str_get(gls->trusty_dev));
	atomic_inc(&gls->gz_log_event_count);
	wake_up_interruptible(&gls->gz_log_wq);
	return NOTIFY_OK;
}

static bool trusty_supports_logging(struct device *device)
{
	int ret;

	ret = trusty_std_call32(device,
				MTEE_SMCNR(SMCF_SC_SHARED_LOG_VERSION, device),
				TRUSTY_LOG_API_VERSION, 0, 0);
	if (ret == SM_ERR_UNDEFINED_SMC) {
		pr_info("trusty-log not supported on secure side.\n");
		return false;
	} else if (ret < 0) {
		pr_info("trusty std call (GZ_SHARED_LOG_VERSION) failed: %d\n",
		       ret);
		return false;
	}

	if (ret == TRUSTY_LOG_API_VERSION) {
		pr_info("trusty-log API supported: %d\n", ret);
		return true;
	}

	pr_info("trusty-log unsupported api version: %d, supported: %d\n",
		ret, TRUSTY_LOG_API_VERSION);
	return false;
}

static int log_read_line(struct gz_log_state *s, int put, int get)
{
	struct log_rb *log = s->log;
	int i;
	char c = '\0';
	size_t max_to_read =
		min((size_t)(put - get), sizeof(s->line_buffer) - 1);
	size_t mask = log->sz - 1;

	for (i = 0; i < max_to_read && c != '\n';)
		s->line_buffer[i++] = c = log->data[get++ & mask];

	s->line_buffer[i] = '\0';

	return i;
}

static int is_buf_empty(struct gz_log_state *gls)
{
	struct log_rb *log = gls->log;
	uint32_t get, put;

	get = gls->get;
	put = log->put;
	return (get == put);
}

static int do_gz_log_read(struct gz_log_state *gls,
			  char __user *buf, size_t size)
{
	struct log_rb *log = gls->log;
	uint32_t get, put, alloc, read_chars = 0, copy_chars = 0;
	int ret = 0;

	WARN_ON(!is_power_of_2(log->sz));

	/*
	 * For this ring buffer, at any given point, alloc >= put >= get.
	 * The producer side of the buffer is not locked, so the put and alloc
	 * pointers must be read in a defined order (put before alloc) so
	 * that the above condition is maintained. A read barrier is needed
	 * to make sure the hardware and compiler keep the reads ordered.
	 */
	get = gls->get;
	put = log->put;
	/* make sure the hardware and compiler reads the correct put & alloc*/
	rmb();
	alloc = log->alloc;

	if (alloc - get > log->sz) {
		pr_notice("trusty: log overflow, lose some msg.");
		get = alloc - log->sz;
	}

	if (get > put)
		return -EFAULT;

	if (is_buf_empty(gls))
		return 0;

	while (get != put) {
		read_chars = log_read_line(gls, put, get);
		/* Force the loads from log_read_line to complete. */
		rmb();
		if (copy_chars + read_chars > (uint32_t)size)
			break;

		ret = copy_to_user(buf + copy_chars, gls->line_buffer,
				   read_chars);
		if (ret) {
			pr_notice("[%s] copy_to_user failed ret %d\n",
				  __func__, ret);
			break;
		}
		get += read_chars;
		copy_chars += read_chars;
	}
	gls->get = get;

	return copy_chars;
}

static ssize_t gz_log_read(struct file *file, char __user *buf, size_t size,
			   loff_t *ppos)
{
	struct gz_log_state *gls = PDE_DATA(file_inode(file));
	int ret = 0;

	/* sanity check */
	if (!buf)
		return -EINVAL;

	if (atomic_xchg(&gls->readable, 0)) {
		ret = do_gz_log_read(gls, buf, size);
		gls->poll_event = atomic_read(&gls->gz_log_event_count);
		atomic_set(&gls->readable, 1);
	}
	return ret;
}

static int gz_log_open(struct inode *inode, struct file *file)
{
	struct gz_log_state *gls = PDE_DATA(inode);
	int ret;

	ret = nonseekable_open(inode, file);
	if (unlikely(ret))
		return ret;
	gls->poll_event = atomic_read(&gls->gz_log_event_count);
	return 0;
}

static int gz_log_release(struct inode *inode, struct file *file)
{
	return 0;
}

static unsigned int gz_log_poll(struct file *file, poll_table *wait)
{
	struct gz_log_state *gls = PDE_DATA(file_inode(file));
	int mask = 0;

	if (!is_buf_empty(gls))
		return POLLIN | POLLRDNORM;

	poll_wait(file, &gls->gz_log_wq, wait);

	if (gls->poll_event != atomic_read(&gls->gz_log_event_count))
		mask |= POLLIN | POLLRDNORM;
	return mask;
}

static const struct file_operations proc_gz_log_fops = {
	.owner = THIS_MODULE,
	.open = gz_log_open,
	.read = gz_log_read,
	.release = gz_log_release,
	.poll = gz_log_poll,
};

static int trusty_gz_log_probe(struct platform_device *pdev)
{
	int ret;
	struct gz_log_state *gls = NULL;
	struct device_node *pnode = pdev->dev.parent->of_node;
	int tee_id = 0;

	if (!trusty_supports_logging(pdev->dev.parent))
		return -ENXIO;

	ret = of_property_read_u32(pnode, "tee-id", &tee_id);
	if (ret != 0)
		dev_info(&pdev->dev, "tee_id is not set\n");
	else
		dev_info(&pdev->dev, "--- init gz-log for MTEE %d ---\n",
			 tee_id);

	gz_log_page_init();
	gls = kzalloc(sizeof(*gls), GFP_KERNEL);
	if (!gls) {
		ret = -ENOMEM;
		goto error_alloc_state;
	}

	mutex_init(&gls->lock);
	gls->dev = &pdev->dev;
	gls->trusty_dev = gls->dev->parent;
	gls->tee_id = tee_id;
	gls->get = 0;

	/* STATIC: memlog already is added at preloader stage.
	 * DYNAMIC: add memlog as usual.
	 */
	if (glctx.flag == DYNAMIC) {
		ret = trusty_std_call32(gls->trusty_dev,
			MTEE_SMCNR(SMCF_SC_SHARED_LOG_ADD, gls->trusty_dev),
			(u32)(glctx.paddr), (u32)((u64)glctx.paddr >> 32),
			glctx.size);
		if (ret < 0) {
			dev_info(&pdev->dev,
				"std call(GZ_SHARED_LOG_ADD) failed: %d %pa\n",
				ret, &glctx.paddr);
			goto error_std_call;
		}
	}

	gls->log = glctx.virt;
	dev_info(&pdev->dev, "gls->log virtual address:%p\n", gls->log);
	if (!gls->log) {
		ret = -ENOMEM;
		goto error_alloc_log;
	}
	glctx.gls = gls;

	gls->call_notifier.notifier_call = trusty_log_call_notify;
	ret = trusty_call_notifier_register(gls->trusty_dev,
					       &gls->call_notifier);
	if (ret < 0) {
		dev_info(&pdev->dev,
			 "can not register trusty call notifier\n");
		goto error_call_notifier;
	}

	gls->panic_notifier.notifier_call = trusty_log_panic_notify;
	ret = atomic_notifier_chain_register(&panic_notifier_list,
					     &gls->panic_notifier);
	if (ret < 0) {
		dev_info(&pdev->dev, "failed to register panic notifier\n");
		goto error_panic_notifier;
	}
	init_waitqueue_head(&gls->gz_log_wq);
	atomic_set(&gls->gz_log_event_count, 0);
	atomic_set(&gls->readable, 1);
	platform_set_drvdata(pdev, gls);

	/* create /proc/gz_log */
	gls->proc = proc_create_data("gz_log", 0444, NULL, &proc_gz_log_fops,
				     gls);
	if (!gls->proc) {
		dev_info(&pdev->dev, "gz_log proc_create failed!\n");
		return -ENOMEM;
	}

	return 0;

error_panic_notifier:
	trusty_call_notifier_unregister(gls->trusty_dev, &gls->call_notifier);
error_call_notifier:
	trusty_std_call32(gls->trusty_dev,
			  MTEE_SMCNR(SMCF_SC_SHARED_LOG_RM, gls->trusty_dev),
			  (u32)glctx.paddr, (u32)((u64)glctx.paddr >> 32), 0);
error_std_call:
	if (glctx.flag == STATIC)
		iounmap(glctx.virt);
	else
		kfree(glctx.virt);
error_alloc_log:
	mutex_destroy(&gls->lock);
	kfree(gls);
error_alloc_state:
	return ret;
}

static int trusty_gz_log_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct gz_log_state *gls = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s\n", __func__);

	proc_remove(gls->proc);
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &gls->panic_notifier);
	trusty_call_notifier_unregister(gls->trusty_dev, &gls->call_notifier);

	ret = trusty_std_call32(gls->trusty_dev,
			MTEE_SMCNR(SMCF_SC_SHARED_LOG_RM, gls->trusty_dev),
			(u32)glctx.paddr, (u32)((u64)glctx.paddr >> 32), 0);
	if (ret)
		pr_info("std call(GZ_SHARED_LOG_RM) failed: %d\n", ret);

	if (glctx.flag == STATIC)
		iounmap(glctx.virt);
	else
		kfree(glctx.virt);

	mutex_destroy(&gls->lock);
	kfree(gls);
	memset(&glctx, 0, sizeof(glctx));

	return 0;
}

static const struct of_device_id trusty_gz_of_match[] = {
	{ .compatible = "android,trusty-gz-log-v1", },
	{},
};

static struct platform_driver trusty_gz_log_driver = {
	.probe = trusty_gz_log_probe,
	.remove = trusty_gz_log_remove,
	.driver = {
		.name = "trusty-gz-log",
		.owner = THIS_MODULE,
		.of_match_table = trusty_gz_of_match,
	},
};

static __init int trusty_gz_log_init(void)
{
	return platform_driver_register(&trusty_gz_log_driver);
}

static void __exit trusty_gz_log_exit(void)
{
	platform_driver_unregister(&trusty_gz_log_driver);
}

arch_initcall(trusty_gz_log_init);
module_exit(trusty_gz_log_exit);
/*module_platform_driver(trusty_gz_log_driver);*/
MODULE_LICENSE("GPL");


