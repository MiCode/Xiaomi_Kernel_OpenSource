/*
 * Copyright (C) 2015 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
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
#include <linux/debugfs.h>

#if ENABLE_GZ_TRACE_DUMP
#if IS_BUILTIN(CONFIG_GZ_LOG)
	#include "gz_trace_builtin.h"
#else
	#include "gz_trace_module.h"
#endif
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>

struct gz_trace_dump_t {
	u64 ktime_base;
	u64 cntvct_base;
	u32 put;
	struct list_head list;
};
#endif

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
	uint32_t get_proc;

#if ENABLE_GZ_TRACE_DUMP
	uint32_t get_trace;
	struct task_struct *trace_task_fd;
	struct completion trace_dump_event;
	bool trace_exit;
	struct list_head gz_trace_dump_list;
	struct mutex gz_trace_dump_mux;
	struct notifier_block callback_notifier;
	atomic_t gz_trace_onoff;
	struct dentry *gz_log_dbg_root;
	struct dentry *sys_gz_trace_on;
#endif

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

#if ENABLE_GZ_TRACE_DUMP
static struct gz_trace_dump_t *gz_trace_add_dump_tail(
			struct list_head *head, struct mutex *mux)
{
	struct gz_trace_dump_t *entry;

	entry = vzalloc(sizeof(struct gz_trace_dump_t));
	if (entry) {
		mutex_lock(mux);
		list_add_tail(&entry->list, head);
		mutex_unlock(mux);
	}
	return entry;
}

static void gz_trace_free(struct list_head *head, struct mutex *mux)
{
	struct gz_trace_dump_t *entry, *entry_tmp;

	mutex_lock(mux);
	list_for_each_entry_safe(entry, entry_tmp, head, list) {
		list_del(&entry->list);
		vfree(entry);
		entry = NULL;
	}
	mutex_unlock(mux);
}
#endif

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

	get = gls->get_proc;
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
	get = gls->get_proc;
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
	gls->get_proc = get;

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

#if ENABLE_GZ_TRACE_DUMP
/* example */
/* ====> GZ (4)[   67.128605]GZT svc-< session 0 */
#define T_SVC_E_STR		"GZT svc->"
#define T_SVC_E_SZ		strlen(T_SVC_E_STR)
#define T_SVC_L_STR		"GZT svc-<"
#define T_SVC_L_SZ		strlen(T_SVC_L_STR)
#define T_DBG_STR		"GZT dbg"
#define T_DBG_SZ		strlen(T_DBG_STR)
#define T_OFFSET		strlen("====> GZ (4)[   53.137165]")
#define CPUSTR_OFFSET	strlen("====> GZ (")


#define SESSION_MSG		"0"
#define SESSION_SZ		strlen(SESSION_MSG)
#define SESSION_OFFSET	strlen("====> GZ (4)[   67.128605]GZT svc-< session ")


enum gz_trace_type_t {
	GZ_TRACE_DEBUG = 0,
	GZ_TRACE_SVC_ENTER,
	GZ_TRACE_SVC_LEAVE,
};

/* arch counter is 13M, mult is 161319385, shift is 21 */
static inline uint64_t arch_counter_to_ns(uint64_t cyc)
{
#define ARCH_TIMER_MULT 161319385
#define ARCH_TIMER_SHIFT 21
	return (cyc * ARCH_TIMER_MULT) >> ARCH_TIMER_SHIFT;
}

static void correct_to_kernel_time(u64 time_us_base, u64 diff_cyc,
			char *output, size_t output_sz)
{
	u64 cyc2ns_diff;
	u64 cyc2us_diff;
	u64 ktime_us;
	u64 gzlog_ktime_us;
	u64 gzlog_ktime_s;

	cyc2ns_diff = arch_counter_to_ns(diff_cyc);
	cyc2us_diff = div_u64(cyc2ns_diff, NSEC_PER_USEC);

	ktime_us = time_us_base - cyc2us_diff;

	div_u64_rem(ktime_us, USEC_PER_SEC, (u32 *)&gzlog_ktime_us);
	gzlog_ktime_s = div_u64(ktime_us, USEC_PER_SEC);

	snprintf(output, output_sz, "%llu.%06u", gzlog_ktime_s, (u32)gzlog_ktime_us);
}

static void gz_trace_svc_msg(char *msg_raw, char session, char type,
		struct gz_trace_dump_t *trace_dump_info)
{
	char *msg_cpu;
	char *msg_tmp;
	char *msg_session;
	char *msg_srvname;
	char *msg_cntvct;
	char msg_ktime[32] = {0};
	u64 gz_cntvnt;
	int err;

	if (session == '0')
		return;

	msg_cpu = msg_raw + CPUSTR_OFFSET;
	msg_tmp = msg_raw + SESSION_OFFSET;

	msg_session = strsep(&msg_tmp, " ");
	if (!msg_session)
		return;

	msg_srvname = strsep(&msg_tmp, " ");
	if (!msg_srvname)
		return;

	msg_cntvct = strsep(&msg_tmp, " ");
	if (!msg_cntvct)
		return;
	err = kstrtou64(msg_cntvct, 10, &gz_cntvnt);
	if (err || gz_cntvnt > trace_dump_info->cntvct_base)
		return;

	correct_to_kernel_time(
		div_u64(trace_dump_info->ktime_base, NSEC_PER_USEC),
		trace_dump_info->cntvct_base - gz_cntvnt,
		msg_ktime, sizeof(msg_ktime));

#define GZ_TRACE_DUMP_RAW_DATA 0
#if !GZ_TRACE_DUMP_RAW_DATA
	GZ_TRUSTY_TRACE_INJECTION("%c|%d|%s|%s|%c|%s",
			type,
			get_current()->pid,
			strlen(msg_srvname) > 8 ?
				msg_srvname + strlen(msg_srvname) - 8 :
				msg_srvname,
			msg_session,
			*msg_cpu,
			msg_ktime);
#else
	GZ_TRUSTY_TRACE_INJECTION("%c|%d|%s|%s|%c|%s|%lu|%lu|%s",
			type,
			get_current()->pid,
			strlen(msg_srvname) > 8 ?
				msg_srvname + strlen(msg_srvname) - 8 :
				msg_srvname,
			msg_session,
			*msg_cpu,
			msg_ktime,
			trace_dump_info->ktime_base,
			trace_dump_info->cntvct_base,
			msg_cntvct);
#endif
}


static void gz_trace_parse(struct gz_log_state *gls, u32 get, u32 put,
			struct gz_trace_dump_t *trace_dump_info)
{
	int i = 0;
	size_t mask;
	char msg_raw[TRUSTY_LINE_BUFFER_SIZE];
	size_t max_to_read;

	mask = gls->log->sz - 1;

	dev_dbg(gls->dev, "%s get(%d) put(%d), mask=0x%zx\n",
			__func__, get, put, mask);

	while (get < put) {
		max_to_read = min((size_t)(put - get), sizeof(msg_raw) - 1);
		for (i = 0 ; i < max_to_read ; i++) {
			msg_raw[i] = gls->log->data[get++ & mask];
			if (msg_raw[i] == '\n')
				break;
		}
		msg_raw[i] = '\0';

		if (strncmp(msg_raw + T_OFFSET, T_SVC_E_STR, T_SVC_E_SZ) == 0)
			gz_trace_svc_msg(msg_raw, *(char *)(msg_raw + SESSION_OFFSET),
					'S', trace_dump_info);
		else if (strncmp(msg_raw + T_OFFSET, T_SVC_L_STR, T_SVC_L_SZ) == 0)
			gz_trace_svc_msg(msg_raw, *(char *)(msg_raw + SESSION_OFFSET),
					'F', trace_dump_info);
		//else if (strncmp(msg_raw + T_OFFSET, T_DBG_STR, T_DBG_SZ) == 0)
		//	trace_type = GZ_TRACE_DEBUG;
		else
			continue;

	}
}

static int gz_trace_task_entry(void *data)
{
	struct gz_log_state *gls = (struct gz_log_state *)data;
	struct gz_trace_dump_t *trace_dump_info_src;
	struct gz_trace_dump_t trace_dump_info_use;
	uint32_t get;
	uint32_t put;
	long timeout = MAX_SCHEDULE_TIMEOUT;


	if (!gls || !gls->log)
		return -ENOMEM;

	dev_info(gls->dev, "%s->\n", __func__);

	while (!kthread_should_stop()) {
		wait_for_completion_timeout(&gls->trace_dump_event, timeout);

		memset((void *)&trace_dump_info_use, 0, sizeof(trace_dump_info_use));
		mutex_lock(&gls->gz_trace_dump_mux);
		trace_dump_info_src = list_first_entry_or_null(&gls->gz_trace_dump_list,
					struct gz_trace_dump_t, list);
		if (trace_dump_info_src) {
			memcpy((void *)&trace_dump_info_use, trace_dump_info_src,
					sizeof(trace_dump_info_use));
			list_del(&trace_dump_info_src->list);
			vfree(trace_dump_info_src);
		}
		mutex_unlock(&gls->gz_trace_dump_mux);

		if (trace_dump_info_use.put) {
			get = gls->get_trace;
			put = trace_dump_info_use.put;

			if (get > put) {
				dev_info(gls->dev, "%s get(%u)>put(%u)\n", __func__, get, put);
				break;
			} else if (get < put  && atomic_read(&gls->gz_trace_onoff))
				gz_trace_parse(gls, get, put, &trace_dump_info_use);

			gls->get_trace = put;
		}
		if (gls->trace_exit)
			timeout = msecs_to_jiffies(1000);
	}
	dev_info(gls->dev, "%s<-\n", __func__);
	return 0;
}

static int trusty_log_callback_notify(struct notifier_block *nb,
			      unsigned long action, void *data)
{
	if (action == TRUSTY_CALLBACK_SYSTRACE) {
		struct gz_log_state *gls = container_of(nb, struct gz_log_state,
						callback_notifier);
		struct gz_trace_dump_t *trace_dump_info;

		trace_dump_info = gz_trace_add_dump_tail(&gls->gz_trace_dump_list,
				&gls->gz_trace_dump_mux);
		if (trace_dump_info) {
			trace_dump_info->ktime_base = ktime_get();
			trace_dump_info->cntvct_base = arch_counter_get_cntvct();
			trace_dump_info->put = gls->log->put;
			complete(&gls->trace_dump_event);
		}
	}

	return NOTIFY_OK;
}

static ssize_t gz_trace_on_write(struct file *filp, const char __user *ubuf,
				   size_t cnt, loff_t *fpos)
{
	struct seq_file *s = filp->private_data;
	struct gz_log_state *gls = s->private;
	char buf[2];

	if (cnt > sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt-1] = 0;

	if (buf[0] == '0')
		atomic_set(&gls->gz_trace_onoff, 0);
	else if (buf[0] == '1')
		atomic_set(&gls->gz_trace_onoff, 1);
	else
		return -EFAULT;

	return cnt;
}

static int gz_trace_on_read(struct seq_file *s, void *unused)
{
	struct gz_log_state *gls = s->private;

	seq_printf(s, "%d\n", atomic_read(&gls->gz_trace_onoff));

	return 0;
}

static int gz_trace_on_open(struct inode *inode, struct file *file)
{
	return single_open(file, gz_trace_on_read, inode->i_private);
}


static const struct file_operations gz_trace_on_fops = {
	.open		= gz_trace_on_open,
	.write		= gz_trace_on_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif /* ENABLE_GZ_TRACE_DUMP */

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
	gls->get_proc = 0;

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

#if ENABLE_GZ_TRACE_DUMP
	gls->callback_notifier.notifier_call = trusty_log_callback_notify;
	ret = trusty_callback_notifier_register(gls->trusty_dev,
					       &gls->callback_notifier);
	if (ret < 0) {
		dev_info(&pdev->dev,
			 "can not register trusty callback notifier\n");
		goto error_callback_notifier;
	}

	INIT_LIST_HEAD(&gls->gz_trace_dump_list);
	mutex_init(&gls->gz_trace_dump_mux);
	gls->trace_exit = false;
	gls->get_trace = 0;
	atomic_set(&gls->gz_trace_onoff, 0);
	init_completion(&gls->trace_dump_event);
	gls->trace_task_fd =
			kthread_run(gz_trace_task_entry, (void *)gls, "gz_trace");
	if (IS_ERR(gls->trace_task_fd)) {
		dev_info(&pdev->dev, "%s unable create kthread\n", __func__);
		ret = PTR_ERR(gls->trace_task_fd);
		goto error_trace_task_run;
	}
	set_user_nice(gls->trace_task_fd, 5);
#endif

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
	gls->proc = proc_create_data("gz_log", 0440, NULL, &proc_gz_log_fops,
				     gls);
	if (!gls->proc) {
		dev_info(&pdev->dev, "gz_log proc_create failed!\n");
		return -ENOMEM;
	}

#if ENABLE_GZ_TRACE_DUMP
	gls->gz_log_dbg_root = debugfs_create_dir("gz_log", NULL);
	gls->sys_gz_trace_on =
		debugfs_create_file("gz_trace_on", 0644, gls->gz_log_dbg_root,
							gls, &gz_trace_on_fops);
	if (!gls->sys_gz_trace_on) {
		dev_info(&pdev->dev, "gz_trace_on node failed!\n");
		return -ENOMEM;
	}
#endif

	return 0;

error_panic_notifier:
	trusty_call_notifier_unregister(gls->trusty_dev, &gls->call_notifier);
error_call_notifier:
#if ENABLE_GZ_TRACE_DUMP
	gls->trace_exit = true;
	kthread_stop(gls->trace_task_fd);
error_trace_task_run:
	trusty_callback_notifier_unregister(gls->trusty_dev,
			&gls->callback_notifier);
error_callback_notifier:
#endif
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

#if ENABLE_GZ_TRACE_DUMP
	gz_trace_free(&gls->gz_trace_dump_list, &gls->gz_trace_dump_mux);
	gls->trace_exit = true;
	complete_all(&gls->trace_dump_event);
	kthread_stop(gls->trace_task_fd);
	trusty_callback_notifier_unregister(gls->trusty_dev,
			&gls->callback_notifier);
#endif

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


