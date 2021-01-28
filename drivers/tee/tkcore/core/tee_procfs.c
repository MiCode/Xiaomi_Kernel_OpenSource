// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/version.h>

#include <asm/barrier.h>

#include <linux/tee_core.h>
#include <linux/tee_kernel_lowlevel_api.h>

#include <arm_common/teesmc_st.h>
#include <arm_common/teesmc.h>

#include "tee_procfs.h"
#include "tee_core_priv.h"

#ifdef CONFIG_OF
#include <linux/of_irq.h>
#endif

#define PROC_DBG(fmt, ...) do {} while (0)

struct tkcore_trace {
	int level;
	const int idx;
	const char *desc;
};

#define xstr(s) str(s)
#define str(s) #s

#define TKCORE_DEFINE_TRACE(__x, __idx) \
	{ \
		.level = 0, \
		.idx = __idx, \
		.desc = xstr(__x) \
	}

#define TEE_LOG_TIMEOUT_MS	(500)

static struct mutex trace_mutex;

struct proc_dir_entry *tee_proc_dir;

struct proc_dir_entry *tee_proc_log_file;
struct proc_dir_entry *tee_proc_trace;

struct proc_dir_entry *tee_proc_drv_version;
struct proc_dir_entry *tee_proc_tee_version;
struct proc_dir_entry *tee_proc_teed_version;

union tee_log_ctrl {
	struct {
		unsigned int tee_buf_addr;
		unsigned int tee_buf_size;
		unsigned int tee_write_pos;
		unsigned int tee_read_pos;

		unsigned int tee_buf_unread_size;

		unsigned int tee_irq_count;
		unsigned int tee_reader_alive;

		unsigned int tee_write_seq;
		unsigned int tee_read_seq;
	} info;
	unsigned char data[TEE_LOG_CTL_BUF_SIZE];
};

struct klog {
	/* shm for log ctl */
	union tee_log_ctrl *log_ctl;

	/* tee ringbuffer for log */
	char *tee_rb;
	/* tee ring buffer length */
	uint32_t tee_rb_len;

	/*
	 * whether write_pos
	 * has restarted from
	 * one
	 */
	bool overwrite;
	/*
	 * served as notifier
	 * when there's new
	 * log
	 */
	wait_queue_head_t wq;

	struct task_struct *ts;

	/*
	 * irq for tee to notify
	 * nsdrv to fetch log
	 */
	int notify_irq;
};

struct ulog {
	/*
	 * local read sequence,
	 * only updated by user
	 * apps open this proc
	 * entry
	 */
	uint32_t rseq;

	/*
	 * buffer containing
	 * temporary str to
	 * pass to CA
	 */
	const char *tmpbuf;

	/*
	 * ptr to global
	 * klog
	 */
	struct klog *klog;

};

static struct klog klog;

static inline bool rb_overrun(uint32_t rseq,
							uint32_t wseq,
							uint32_t rb_len)
{
	return wseq - rseq > rb_len;
}

static size_t ulog_rb(struct klog *klog,
					struct ulog *ulog,
					char __user *buf,
					size_t count)
{
	size_t len = 0;
	uint32_t wseq;

	union tee_log_ctrl *log_ctl = klog->log_ctl;

	static const char ulog_flag_intr[] =
		"------ interrupted\n";

	wseq = READ_ONCE(log_ctl->info.tee_write_seq);

	while ((len != count) && ((ulog->rseq != wseq) ||
		(ulog->tmpbuf && ulog->tmpbuf[0] != '\0'))) {
		size_t copy_len;
		unsigned long n;

		if (ulog->tmpbuf == NULL) {
			if (rb_overrun(ulog->rseq, wseq,
						klog->tee_rb_len)) {
				ulog->tmpbuf = ulog_flag_intr;
			}
		} else if (ulog->tmpbuf[0] == '\0') {
			if (klog->overwrite ||
				wseq >= klog->tee_rb_len) {
				ulog->rseq = wseq - klog->tee_rb_len;
			} else {
				ulog->rseq = 0;
			}
			ulog->tmpbuf = NULL;
		}

		if (ulog->tmpbuf) {
			size_t tmpbuf_len;
			const char *tmpbuf;

			tmpbuf = ulog->tmpbuf;
			tmpbuf_len = strlen(tmpbuf);

			copy_len = (uint32_t) min(tmpbuf_len, count - len);
			n = copy_to_user(&buf[len], tmpbuf, copy_len);

			if (copy_len == n) {
				pr_warn("tkcoredrv: failed to copy flag to user");
				return len;
			}

			ulog->tmpbuf = &tmpbuf[copy_len - n];
			len += (copy_len - n);
		} else {
			copy_len = min((uint32_t) (count - len),
					min(klog->tee_rb_len -
						ulog->rseq % klog->tee_rb_len,
						wseq - ulog->rseq));
			n = copy_to_user(&buf[len],
				&klog->tee_rb[ulog->rseq % klog->tee_rb_len],
				copy_len);
			if (copy_len == n) {
				pr_warn("tkcoredrv: failed to copy klog to user\n");
				return len;
			}

			ulog->rseq += (copy_len - n);
			len += (copy_len - n);
		}

		wseq = READ_ONCE(log_ctl->info.tee_write_seq);
	}

	return len;
}

static ssize_t tee_log_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	ssize_t ret;
	struct ulog *ulog;
	struct klog *klog;

	if (file == NULL || buf == NULL || pos == NULL)
		return -EINVAL;

	ulog = (struct ulog *) file->private_data;
	if (ulog == NULL) {
		pr_warn("tkcoredrv: file not open correctly\n");
		return -EINVAL;
	}

	klog = ulog->klog;

	if (file->f_flags & O_NONBLOCK) {
		/*
		 * currently nonblock file is
		 * not supported, since
		 * we might need to enter
		 * wait queue
		 */
		return -EAGAIN;
	}

	if (ulog->tmpbuf == NULL) {
		long r;

		do {
			r = wait_event_interruptible_timeout(klog->wq,
				ulog->rseq !=
				READ_ONCE(klog->log_ctl->info.tee_write_seq),
				msecs_to_jiffies(TEE_LOG_TIMEOUT_MS));

		} while (!r);

		if (r < 0) {
			/*
			 * woke up due to signal, e.g.
			 * calling program terminated
			 * by CTRL-C
			 */
			return -EINTR;
		}
	}

	ret = ulog_rb(klog, ulog, buf, count);
	*pos += ret;

	return ret;
}

int tee_log_open(struct inode *inode, struct file *file)
{
	int ret;
	struct ulog *ulog;

	static const char ulog_flag_begin[] =
		"------ beginning of tee\n";

	ulog = kmalloc(
			sizeof(struct ulog),
			GFP_KERNEL);
	if (ulog == NULL)
		return -ENOMEM;

	ulog->tmpbuf = ulog_flag_begin;
	ulog->klog = &klog;

	ret = nonseekable_open(inode, file);

	if (unlikely(ret)) {
		kfree(ulog);

		pr_warn("tkcoredrv: open file failed with %d\n", ret);
		return ret;
	}

	file->private_data = (void *) ulog;

	return 0;
}

int tee_log_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations log_tee_ops = {
	.read = tee_log_read,
	.open = tee_log_open,
	.release = tee_log_release,
};

/* Guarantee the idx defined here is consistent with TEE */
struct tkcore_trace tkcore_traces[] = {
	TKCORE_DEFINE_TRACE(ree_fs, 0),
	TKCORE_DEFINE_TRACE(enc_fs, 1),
	TKCORE_DEFINE_TRACE(rpmb_blk, 2),
	TKCORE_DEFINE_TRACE(rpmb_fs, 3),
	TKCORE_DEFINE_TRACE(ta_mgmt, 4),
	TKCORE_DEFINE_TRACE(tee_comm, 5),
	TKCORE_DEFINE_TRACE(tee_boot, 6),
	TKCORE_DEFINE_TRACE(core_mm, 7),
	TKCORE_DEFINE_TRACE(uart_print, 8),
	TKCORE_DEFINE_TRACE(htfat, 9),
	TKCORE_DEFINE_TRACE(spi, 10),
};

#define NTRACES ARRAY_SIZE(tkcore_traces)

static int tee_trace_open(struct inode *inode, struct file *filp)
{
	int ret;

	mutex_lock(&trace_mutex);


	ret = nonseekable_open(inode, filp);
	if (unlikely(ret)) {
		mutex_unlock(&trace_mutex);
		return ret;
	}

	filp->f_pos = 0UL;
	filp->private_data = PDE_DATA(inode);

	return 0;
}

#define TRACE_BUF_SIZE 128

static char trace_buf[TRACE_BUF_SIZE + 1];

static ssize_t tee_trace_read(struct file *file, char __user *buf, size_t count,
				loff_t *pos)
{
	size_t i;
	ssize_t r;

	char *p = trace_buf;

	loff_t __pos;

	size_t len = count > TRACE_BUF_SIZE - 1 ? TRACE_BUF_SIZE - 1 : count;

	if (buf == NULL)
		return -EINVAL;

	for (i = 0; i < NTRACES; i++) {
		int l = snprintf(p, trace_buf + len - p, "%s %d ",
				 tkcore_traces[i].desc, tkcore_traces[i].level);

		if (l <= 0)
			return -EINVAL;

		p += l;
	}

	*p++ = '\n';

	if (pos == NULL)
		__pos = 0;
	else
		__pos = *pos;

	if (__pos >= p - trace_buf)
		return 0;

	r = copy_to_user(buf, trace_buf, (p - trace_buf) - __pos);

	if (r < 0)
		return r;

	*pos += ((p - trace_buf) - __pos) - r;

	return ((p - trace_buf) - __pos) - r;
}

static ssize_t tee_trace_write(struct file *filp, const char __user *buf,
				   size_t count, loff_t *pos)
{
	char *p;
	ssize_t r;
	uint32_t level;
	size_t i, len = count > TRACE_BUF_SIZE ? TRACE_BUF_SIZE : count;

	struct smc_param param = { 0 };

	struct tee *tee = filp->private_data;

	if (tee == NULL) {
		pr_warn("bad proc fp\n");
		return -EINVAL;
	}

	(void) pos;

	PROC_DBG("Count %lu Actual Count %lu\n", count, len);

	r = copy_from_user(trace_buf, buf, len);
	if (r < 0)
		return r;

	len = len - r;
	trace_buf[len] = '\0';

	PROC_DBG("Buffer: %s\n", trace_buf);

	p = strchr(trace_buf, '=');
	if (p == NULL) {
		PROC_DBG("Expecting format: <trace_item>=<loglevel>[0|1]\n");
		return -EINVAL;
	}

	*p = '\0';

	r = kstrtouint(p + 1, 10, &level);
	if (r < 0) {
		PROC_DBG("Expecting format: <trace_item>=<loglevel>[0|1]\n");
		return r;
	}

	for (i = 0; i < NTRACES; i++) {
		if (strcmp(tkcore_traces[i].desc, trace_buf) == 0) {
			param.a0 = TKCORE_FASTCALL_TRACE_CONFIG;
			param.a1 = tkcore_traces[i].idx;
			param.a2 = level;

			if (tkcore_traces[i].level != level) {
				tee->ops->call_tee(&param);

				PROC_DBG("TRACE_CONFIG return value: 0x%x\n",
					 param.a0);

				if (param.a0 == TEESMC_RETURN_OK) {
					tkcore_traces[i].level = level;
					return len;
				}

				pr_warn(
					"trace config Failed with 0x%llx\n",
					(uint64_t) param.a0);

				return -EINVAL;
			}

			PROC_DBG("Request level same with current level: %d\n",
				tkcore_traces[i].level);

			return len;
		}
	}

	PROC_DBG("Can't find a matching trace_item\n");

	return -EINVAL;
}

static int tee_trace_release(struct inode *inode, struct file *file)
{
	mutex_unlock(&trace_mutex);

	return 0;
}

static const struct file_operations log_tee_trace_ops = {
	.read = tee_trace_read,
	.write = tee_trace_write,
	.open = tee_trace_open,
	.release = tee_trace_release
};

#include <version.h>

static ssize_t copy_to_user_str(char __user *buf, ssize_t count, loff_t *pos,
				const char *version)
{
	ssize_t r;
	size_t cnt;
	loff_t __pos;

	__pos = *pos;
	if (__pos > strlen(version) + 1) {
		pr_warn("invalid pos: %lld len: %zu\n",
			__pos, strlen(version));
		return -EINVAL;
	}

	cnt = count < strlen(version) + 1 - __pos ?
		count : strlen(version) + 1 - __pos;

	r = copy_to_user(buf, version + __pos, cnt);

	if (r < 0)
		return r;

	*pos += cnt - r;

	return cnt - r;
}

static int tee_version_major, tee_version_minor;

static ssize_t tee_version_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	char tee_version[20];

	if (buf == NULL || pos == NULL)
		return -EINVAL;

	snprintf(tee_version, sizeof(tee_version),
		 "0.%d.%d-gp\n", tee_version_major, tee_version_minor);

	return copy_to_user_str(buf, count, pos, tee_version);
}

static const struct file_operations tee_version_ops = {
	.read = tee_version_read,
	.write = NULL,
	.open = NULL,
	.release = NULL
};


static ssize_t drv_version_read(struct file *file, char __user *buf,
				size_t count, loff_t *pos)
{
	if (buf == NULL || pos == NULL)
		return -EINVAL;

	return copy_to_user_str(buf, count, pos, tkcore_nsdrv_version);
}

static const struct file_operations drv_version_ops = {
	.read = drv_version_read,
	.write = NULL,
	.open = NULL,
	.release = NULL
};

#define TEED_VERSION_SIZE 50
char teed_version[TEED_VERSION_SIZE + 1] = "unknown\n";

static ssize_t teed_version_read(struct file *file, char __user *buf,
				 size_t count, loff_t *pos)
{
	if (buf == NULL || pos == NULL)
		return -EINVAL;

	return copy_to_user_str(buf, count, pos, teed_version);
}

static ssize_t teed_version_write(struct file *filp, const char __user *buf,
				  size_t count, loff_t *pos)
{
	ssize_t r;

	if (count > TEED_VERSION_SIZE)
		return -ENOMEM;

	r = copy_from_user(teed_version, buf, count);
	if (r < 0)
		return r;

	teed_version[count + 1] = '\0';

	return count;
}

static const struct file_operations teed_version_ops = {
	.read = teed_version_read,
	.write = teed_version_write,
	.open = NULL,
	.release = NULL
};

static void remove_entry(void)
{
	proc_remove(tee_proc_dir);

	tee_proc_dir = NULL;
	tee_proc_log_file = NULL;
	tee_proc_trace = NULL;
	tee_proc_drv_version = NULL;
	tee_proc_tee_version = NULL;
}

static int create_entry(struct tee *tee)
{

	tee_proc_dir = proc_mkdir("tkcore", NULL);
	if (tee_proc_dir == NULL) {
		pr_err("proc_mkdir tkcore failed\n");
		return -1;
	}

	tee_proc_log_file = proc_create_data("tkcore_log",
		0444, tee_proc_dir, &log_tee_ops, (void *) tee);

	if (tee_proc_log_file == NULL) {
		pr_err("proc_create failed\n");
		goto err;
	}

	tee_proc_trace = proc_create_data("tkcore_trace",
			0444, tee_proc_dir,
			&log_tee_trace_ops, (void *) tee);

	if (tee_proc_trace == NULL) {
		pr_err("proc_create tkcore_trace failed\n");
		goto err;
	}

	tee_proc_drv_version = proc_create_data(
			"tkcore_drv_version",
			0444, tee_proc_dir,
			&drv_version_ops, (void *) tee);

	if (tee_proc_drv_version == NULL) {
		pr_err("proc_create tkcore_drv_version failed\n");
		goto err;
	}

	tee_proc_tee_version = proc_create_data("tkcore_os_version",
			0444, tee_proc_dir,
			&tee_version_ops, (void *) tee);

	if (tee_proc_tee_version == NULL) {
		pr_err("proc_create tkcore_os_version failed\n");
		goto err;
	}

	tee_proc_teed_version = proc_create_data("tkcore_teed_version",
			0666, tee_proc_dir, &teed_version_ops, (void *) tee);

	if (tee_proc_teed_version == NULL) {
		pr_err("proc_create tkcore_teed_version failed\n");
		goto err;
	}

	return 0;

err:
	remove_entry();
	return -1;
}

static irqreturn_t tkcore_log_irq_handler(int irq, void *dev_id)
{
	wake_up_all(&(klog.wq));
	return IRQ_HANDLED;
}

#define LINE_LENGTH	120U

static void log_rb(struct klog *klog)
{
	char *p;
	uint32_t rseq, wseq;

	const char tail[] = "<...>";
	char line[LINE_LENGTH + sizeof(tail) + 1];

	union tee_log_ctrl *log_ctl = klog->log_ctl;
	char *rb = klog->tee_rb;

	strcpy(&line[LINE_LENGTH], tail);

	rseq = log_ctl->info.tee_read_seq;
	wseq = READ_ONCE(log_ctl->info.tee_write_seq);

	p = line;

	do {
		uint32_t copy_len, i, k;

		if (rb_overrun(rseq, wseq, klog->tee_rb_len)) {
			pr_info("---- interrupted\n");
			rseq = log_ctl->info.tee_read_seq =
				wseq - klog->tee_rb_len;
		}

		k = rseq % klog->tee_rb_len;

		copy_len = min(LINE_LENGTH - (uint32_t) (p - line),
				min(wseq - rseq, klog->tee_rb_len - k));

		for (i = 0; i < copy_len &&
				rb[k + i] != '\n' && rb[k + i] != '\0';
				i++, p++) {
			*p = rb[k + i];
		}

		rseq += i;

		if (i != copy_len) {
			/*
			 * find an '\n' in buffer
			 * we skip it
			 */
			++rseq;
			*p = '\0';
		}

		if (((i == copy_len) && (p - line == LINE_LENGTH))
				|| (i != copy_len)) {
			pr_info("%s\n", line);
			p = line;
			log_ctl->info.tee_read_seq = rseq;
		}

		wseq = READ_ONCE(log_ctl->info.tee_write_seq);
		if (wseq >= klog->tee_rb_len)
			klog->overwrite = true;
	} while (rseq != wseq);
}

static int logd(void *args)
{
	struct klog *klog = (struct klog *) args;
	union tee_log_ctrl *log_ctl = klog->log_ctl;

	++log_ctl->info.tee_reader_alive;

	while (!kthread_should_stop()) {
		/*
		 * a memory barrier is implied by
		 * wait_event_interruptible_timeout(..)
		 */
		if (wait_event_interruptible_timeout(klog->wq,
			log_ctl->info.tee_read_seq !=
			READ_ONCE(log_ctl->info.tee_write_seq),
			msecs_to_jiffies(TEE_LOG_TIMEOUT_MS)) <= 0) {
			/*
			 * interrupted /
			 * timeout and condition
			 * evaluated to false
			 */

			continue;
		}

		log_rb(klog);
	}

	--log_ctl->info.tee_reader_alive;
	return 0;
}

static int register_klog_irq(struct klog *klog)
{
	int r;
	int irq_num;

#ifdef CONFIG_OF
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL,
						"trustkernel,tkcore");
	if (node) {
		irq_num = irq_of_parse_and_map(node, 0);
	} else {
		pr_err("tkcoredrv: node not found\n");
		irq_num = -1;
	}
#else
	irq_num = TEE_LOG_IRQ;
#endif

	if (irq_num < 0) {
		pr_warn("tkcoredrv: unknown tee_log_irq id\n");
		return -1;
	}

	pr_info("tkcoredrv: tee_log_irq id = %d\n",
			irq_num);

	r = request_irq(irq_num,
		(irq_handler_t) tkcore_log_irq_handler,
		IRQF_TRIGGER_RISING,
		"tee_log_irq", NULL);

	if (r != 0) {
		pr_err("tkcoredrv: failed to register klog_irq with %d\n",
			r);
		return -1;
	}

	klog->notify_irq = irq_num;

	return 0;
}

static int init_tos_version(struct tee *tee)
{
	struct smc_param param;

	memset(&param, 0, sizeof(param));

	/* get os revision */
	param.a0 = TEESMC32_CALL_GET_OS_REVISION;
	tee->ops->raw_call_tee(&param);

	tee_version_major = param.a0;
	tee_version_minor = param.a1;

	pr_info("tkcoreos-rev: 0.%d.%d-gp\n",
		tee_version_major, tee_version_minor);

	return 0;
}

static int init_klog_shm_args(struct tee *tee,
							unsigned long *shm_pa,
							unsigned int *shm_len)
{
	struct smc_param param;

	unsigned long pa;
	unsigned int len;

	if (shm_pa == NULL || shm_len == NULL)
		return -1;

	memset(&param, 0, sizeof(param));

	param.a0 = TEESMC32_ST_FASTCALL_GET_LOGM_CONFIG;
	tee->ops->raw_call_tee(&param);

	if (param.a0 != TEESMC_RETURN_OK) {
		pr_err("Log service not available: 0x%x",
			(uint) param.a0);
		return -1;
	}

	pa = param.a1;
	len = param.a2;

	if (len <= TEE_LOG_CTL_BUF_SIZE) {
		pr_err("tkcoredrv: invalid shm_len: %u\n",
				len);
		return -1;
	}

	if ((pa & (PAGE_SIZE - 1)) ||
		(len & (PAGE_SIZE - 1))) {
		pr_err("tkcoredrv: invalid klog args\n");
		pr_err("tkcoredrv: pa=0x%lx len=0x%x\n",
				pa, len);
		return -1;
	}

	*shm_pa = pa;
	*shm_len = len;

	return 0;
}

static int init_klog_shm(struct klog *klog,
						unsigned long shm_pa,
						unsigned int shm_len)
{
	char *rb;
	uint32_t rb_len;
	union tee_log_ctrl *log_ctl;

	log_ctl = tee_map_cached_shm(shm_pa,
								shm_len);

	if (log_ctl == NULL) {
		pr_err("tkcoredrv: failed to map shm\n");
		pr_err("tkcoredrv: pa=0x%lx len=%u\n",
				shm_pa, shm_len);
		return -1;
	}

	rb = (char *) log_ctl + TEE_LOG_CTL_BUF_SIZE;
	rb_len = log_ctl->info.tee_buf_size;

	if (rb_len != shm_len - TEE_LOG_CTL_BUF_SIZE) {
		pr_err("tkcoredrv:Unexpected shm length: %u\n",
				shm_len);
		tee_unmap_cached_shm(log_ctl);
		return -1;
	}

	log_ctl->info.tee_reader_alive = 0;

	klog->log_ctl = log_ctl;

	klog->tee_rb = rb;
	klog->tee_rb_len = rb_len;

	return 0;
}

static int init_klog(struct klog *klog, struct tee *tee)
{
	unsigned long shm_pa;
	unsigned int shm_len;

	BUILD_BUG_ON(sizeof(union tee_log_ctrl)
			!= TEE_LOG_CTL_BUF_SIZE);

	klog->notify_irq = -1;

	if (init_klog_shm_args(tee, &shm_pa,
						&shm_len) < 0) {
		return -1;
	}

	if (init_klog_shm(klog,
					shm_pa,
					shm_len) < 0) {
		return -1;
	}

	klog->overwrite = false;

	init_waitqueue_head(&klog->wq);

	if (register_klog_irq(klog) < 0)
		goto err_unmap_shm;


	klog->ts = kthread_run(logd,
				(void *) klog, "tee-log");
	if (klog->ts == NULL) {
		pr_err("tkcoredrv: Failed to create kthread\n");
		goto err_free_irq;
	}

	return 0;

err_free_irq:
	if (klog->notify_irq > 0)
		free_irq(klog->notify_irq, NULL);

err_unmap_shm:
	tee_unmap_cached_shm(klog->log_ctl);

	memset(klog, 0, sizeof(*klog));
	/*
	 * set notify_irq to
	 * un-initialized state
	 */
	klog->notify_irq = -1;

	return -1;
}

/* TODO wait for kthread logwq
 * to exit
 */
static void free_klog(struct klog *klog)
{
	if (klog->notify_irq > 0) {
		free_irq(klog->notify_irq, NULL);
		klog->notify_irq = -1;
	}

	kthread_stop(klog->ts);
	klog->ts = NULL;

	/*
	 * wake up all waiters
	 * in wq, since we're
	 * about to leave
	 */
	wake_up_all(&(klog->wq));

	/*
	 * we don't unmap klog->tee_rb,
	 * because it's not
	 * quite easy to check whether
	 * all user process using
	 * procfs has finished
	 */
}

int tee_init_procfs(struct tee *tee)
{
	mutex_init(&trace_mutex);

	init_tos_version(tee);

	if (create_entry(tee) < 0)
		return -1;

	if (init_klog(&klog, tee) < 0)
		goto out_remove_entry;

	return 0;

out_remove_entry:
	remove_entry();

	return -1;
}

void tee_exit_procfs(void)
{
	remove_entry();
	free_klog(&klog);
}
