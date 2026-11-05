#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/tracepoint.h>
#include <trace/events/initcall.h>
#include <linux/mutex.h>
#include "boottime.h"

/**
 * Data structures to store tracepoints information
 */
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	void *data;
	bool init;
};

/**
 * Data structures to store initcall start time info
 */
struct initcall_list_t {
	pid_t pid;
	pid_t tid;
	u64 timestamp;
	struct list_head dev_entry;
};

static void tp_deinit(void);

/* Parameters */
struct log_t *bootprof[BUF_COUNT];
unsigned long log_count;
DEFINE_SPINLOCK(bootprof_lock);
unsigned long boottime_ok;

static bool enabled;
static u64 timestamp_on, timestamp_off;
static bool boot_finish;

static struct list_head initcall_list;
static DEFINE_SPINLOCK(initcall_lock);
atomic_t initcall_num = ATOMIC_INIT(0);

static char pbl_time[32] = {0};
module_param_string(pblt, pbl_time, 32, 0644);

static char xbl_time[32] = {0};
module_param_string(xblt, xbl_time, 32, 0644);

static char abl_time[32] = {0};
module_param_string(ablt, abl_time, 32, 0644);

EXPORT_SYMBOL_GPL(bootprof_lock);
EXPORT_SYMBOL_GPL(boottime_ok);

unsigned long get_log_count(void)
{
	return log_count;
}
EXPORT_SYMBOL_GPL(get_log_count);

struct log_t **get_bootprof_pointer(void)
{
	return bootprof;
}
EXPORT_SYMBOL_GPL(get_bootprof_pointer);

bool mt_boot_finish(void)
{
	return boot_finish;
}
EXPORT_SYMBOL_GPL(mt_boot_finish);

static long long msec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static unsigned long msec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000);
}

void bootprof_log_boot(char *str)
{
	unsigned long long ts;
	struct log_t *p = NULL;
	size_t n;
	int err = 0;

	if (!str) {
		return;
	}
	n = strlen(str) + 1;
	ts = sched_clock();

	spin_lock(&bootprof_lock);
	if (!enabled) {
		spin_unlock(&bootprof_lock);
		return;
	}
	if (log_count >= (LOGS_PER_BUF * BUF_COUNT)) {
		enabled = false;
		err = 1;
		goto out;
	} else if (log_count && !(log_count % LOGS_PER_BUF)) {
		bootprof[log_count / LOGS_PER_BUF] =
			kcalloc(LOGS_PER_BUF, sizeof(struct log_t),
				GFP_ATOMIC | __GFP_NORETRY | __GFP_NOWARN);
	}
	if (!bootprof[log_count / LOGS_PER_BUF]) {
		err = 2;
		goto out;
	}
	p = &bootprof[log_count / LOGS_PER_BUF][log_count % LOGS_PER_BUF];

	p->timestamp = ts;
	p->pid = current->pid;
	n += TASK_COMM_LEN;

	p->comm_event = kzalloc(n, GFP_ATOMIC | __GFP_NORETRY |
			  __GFP_NOWARN);
	if (!p->comm_event) {
		enabled = false;
		err = 3;
		goto out;
	}

	memcpy(p->comm_event, current->comm, TASK_COMM_LEN);
	memcpy(p->comm_event + TASK_COMM_LEN, str, n - TASK_COMM_LEN);
	log_count++;
out:
	spin_unlock(&bootprof_lock);

	pr_info("BOOTTIME:%10lld.%06ld:%s\n", msec_high(ts), msec_low(ts), str);
	if (err > 0)
		pr_err("[BOOTTIME] Error(Ret:%d): Skip log.\n", err);
}
EXPORT_SYMBOL_GPL(bootprof_log_boot);

void bootprof_initcall(initcall_t fn, unsigned long long ts)
{
	/* log more than threshold initcalls */
	unsigned long msec_rem;
	char msgbuf[MSG_SIZE];
	int len;

	atomic_inc(&initcall_num);

	if (ts > BOOTPROF_THRESHOLD) {
		msec_rem = do_div(ts, NSEC_PER_MSEC);
		len = scnprintf(msgbuf, sizeof(msgbuf),
			"initcall: %ps %5llu.%06lums",
			fn, ts, msec_rem);
		if (len < 0)
			pr_err("BOOTTIME: initcall - Invalid argument.\n");
		bootprof_log_boot(msgbuf);
	}
}

#ifndef MODULE
/*Build-in*/
void bootprof_probe(unsigned long long ts, struct device *dev,
			   struct device_driver *drv, unsigned long probe)
{
	/* log more than threshold probes*/
	unsigned long msec_rem;
	char msgbuf[MSG_SIZE];
	int pos, len;

	if (ts <= BOOTPROF_THRESHOLD)
		return;
	msec_rem = do_div(ts, NSEC_PER_MSEC);

	pos = scnprintf(msgbuf, sizeof(msgbuf), "probe: probe=%ps",
					(void *)probe);
	if (pos < 0)
		pos = 0;

	if (drv) {
		len = scnprintf(msgbuf + pos, sizeof(msgbuf) - pos,
				" drv=%s(%ps)", drv->name ? drv->name : "",
				(void *)drv);
		if (len >= 0)
			pos += len;
	}

	if (dev && dev->init_name) {
		len = scnprintf(msgbuf + pos, sizeof(msgbuf) - pos,
				" dev=%s(%ps)", dev->init_name, (void *)dev);
		if (len >= 0)
			pos += len;
	}

	scnprintf(msgbuf + pos, sizeof(msgbuf) - pos,
			" %5llu.%06lums", ts, msec_rem);
	bootprof_log_boot(msgbuf);
}
EXPORT_SYMBOL_GPL(bootprof_probe);

void bootprof_pdev_register(unsigned long long ts, struct platform_device *pdev)
{
	/* log more than threshold register*/
	unsigned long msec_rem;
	char msgbuf[MSG_SIZE];
	int len;

	if (ts <= BOOTPROF_THRESHOLD || !pdev)
		return;
	msec_rem = do_div(ts, NSEC_PER_MSEC);
	len = scnprintf(msgbuf, sizeof(msgbuf),
			"probe: pdev=%s(%ps) %5llu.%06lums",
			pdev->name, (void *)pdev, ts, msec_rem);
	if (len < 0)
		pr_err("BOOTTIME: pdev - Invalid argument.\n");

	bootprof_log_boot(msgbuf);
}
EXPORT_SYMBOL_GPL(bootprof_pdev_register);
#endif /*MODULE END*/

/*  initcalls tracepoint cb while initcall_debug=1 */
static __init_or_module void
tp_initcall_start_cb(void *data, initcall_t fn)
{
	struct initcall_list_t *obj;
	struct initcall_list_t *pos, *next;
	struct list_head err_list;

	INIT_LIST_HEAD(&err_list);

	obj = kzalloc(sizeof(struct initcall_list_t),
			GFP_ATOMIC | __GFP_NORETRY | __GFP_NOWARN);
	if (!obj)
		return;

	obj->pid = task_pid_nr(current);
	obj->tid = task_pid_vnr(current);
	obj->timestamp = sched_clock();

	/*Check if there is duplicated enrty.*/
	spin_lock(&initcall_lock);
	if (!list_empty(&initcall_list)) {
		list_for_each_entry_safe(pos, next, &initcall_list, dev_entry) {
			if ((pos->pid == obj->pid) && (pos->tid == obj->tid)) {
				list_del(&pos->dev_entry);
				/*Add duplicated enrty into err list*/
				list_add_tail(&pos->dev_entry, &err_list);
			}
		}
	}
	list_add_tail(&obj->dev_entry, &initcall_list);
	spin_unlock(&initcall_lock);

	/*release entry of err list*/
	if (!list_empty(&err_list)) {
		list_for_each_entry_safe(pos, next, &err_list, dev_entry) {
			pr_err("[BOOTTIME] Warn:duplicated entry.(pid:%d, tid:%d)\n",
				pos->pid, pos->tid);
			list_del(&pos->dev_entry);
			kfree(pos);
		}
	}
}

static __init_or_module void
tp_initcall_finish_cb(void *data, initcall_t fn, int ret)
{
	struct initcall_list_t *pos, *next;
	unsigned long long start_ts = 0;
	struct list_head memfree_list;
	unsigned long long end_ts = sched_clock();
	unsigned long long duration;

	INIT_LIST_HEAD(&memfree_list);

	spin_lock(&initcall_lock);
	list_for_each_entry_safe(pos, next, &initcall_list, dev_entry) {
		if ((pos->pid == task_pid_nr(current)) &&
		    (pos->tid == task_pid_vnr(current))) {
			start_ts = pos->timestamp;
			list_del(&pos->dev_entry);
			list_add_tail(&pos->dev_entry, &memfree_list);
			break;
		}
	}
	spin_unlock(&initcall_lock);

	/*release entry*/
	if (!list_empty(&memfree_list)) {
		list_for_each_entry_safe(pos, next, &memfree_list, dev_entry) {
			list_del(&pos->dev_entry);
			kfree(pos);
		}
	}

	/* start time of current module is 0.*/
	if (start_ts == 0) {
		#ifdef MODULE
		/* if bootprof is first loading module.*/
		bootprof_log_boot("Kernel_init_done");
		#endif
		return;
	}
	duration = end_ts - start_ts;
	bootprof_initcall(fn, duration);
}

static struct tracepoints_table interests[] = {
	{.name = "initcall_start", .func = tp_initcall_start_cb},
	{.name = "initcall_finish", .func = tp_initcall_finish_cb},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(interests) / \
	     sizeof(struct tracepoints_table); i++)

/* Find the struct tracepoint associated */
/* with a given tracepointname.          */
static void tp_lookup(struct tracepoint *tp, void *ignore)
{
	unsigned int i;

	if (!tp || !tp->name)
		return;

	FOR_EACH_INTEREST(i) {
		if (strcmp(interests[i].name, tp->name) == 0)
			interests[i].tp = tp;
	}
}

/* Unregister initcalls tracepoints */
static void tp_deinit(void)
{
	unsigned int i;
	struct initcall_list_t *pos, *next;

	FOR_EACH_INTEREST(i) {
		if (interests[i].init) {
			tracepoint_probe_unregister(interests[i].tp,
				interests[i].func, interests[i].data);
			interests[i].init = false;
		}
	}

	spin_lock(&initcall_lock);
	if (!list_empty(&initcall_list)) {
		list_for_each_entry_safe(pos, next, &initcall_list, dev_entry) {
			if (pos) {
				list_del(&pos->dev_entry);
				kfree(pos);
			}
		}
	}
	spin_unlock(&initcall_lock);
	pr_info("BOOTTIME: Unregister initcalls tracepoint.\n");
}

/* Register initcalls tracepoints */
static void tp_init(void)
{
	unsigned int i;

	INIT_LIST_HEAD(&initcall_list);

	/* Install the tracepoints */
	for_each_kernel_tracepoint(tp_lookup, NULL);

	FOR_EACH_INTEREST(i) {
		if (!interests[i].tp) {
			pr_err("[BOOTTIME]TP: %s not found\n",
					interests[i].name);
			/* Unload previously loaded */
			tp_deinit();
			return;
		}
		tracepoint_probe_register(interests[i].tp, interests[i].func,
						interests[i].data);
		interests[i].init = true;
	}
}

static void mt_bootprof_switch(int on)
{
	bool tmp;
	unsigned long long ts = sched_clock();

	spin_lock(&bootprof_lock);
	tmp = enabled ^ on;
	if (tmp) {
		if (on)
			enabled = 1;
		else
			enabled = 0;
	}
	spin_unlock(&bootprof_lock);

	if (tmp) {
		pr_info("BOOTTIME:%10lld.%06ld: %s%lld)\n",
			msec_high(ts), msec_low(ts), on ? "ON (TH:" : "OFF (KO:",
			on ? msec_high(BOOTPROF_THRESHOLD) : (long long)atomic_read(&initcall_num));

		if (on) {
			timestamp_on = ts;
		} else {
			timestamp_off = ts;
			if (!boot_finish) {
				boot_finish = true;
				/* Unregister Initcall tracepointsk while boot finish */
				tp_deinit();
			}
		}
	}
}

static ssize_t
mt_bootprof_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[BOOT_STR_SIZE];
	size_t copy_size = cnt;

	if (cnt >= sizeof(buf))
		copy_size = BOOT_STR_SIZE - 1;

	if (copy_from_user(&buf, ubuf, copy_size))
		return -EFAULT;

	if (cnt == 1 && buf[0] == '1') {
		mt_bootprof_switch(1);
		return 1;
	} else if (cnt == 1 && buf[0] == '0') {
		mt_bootprof_switch(0);
		return 1;
	}

	buf[copy_size] = 0;
	bootprof_log_boot(buf);

	return cnt;
}

static int mt_bootprof_show(struct seq_file *m, void *v)
{
	unsigned long i;
	struct log_t *p;

	if (!m) {
		pr_err("seq_file is Null.\n");
		return 0;
	}
	seq_puts(m, "----------------------------------------\n");
	seq_printf(m, "BOOT PROF (unit:msec) %-10d\n", enabled);
	seq_printf(m, "Kernel Module Total %-10d\n", atomic_read(&initcall_num));
	seq_puts(m, "----------------------------------------\n");

	seq_printf(m, "bootloader time consume\n");
	seq_printf(m, "%s: %-10sms\n", "plb time", pbl_time);
	seq_printf(m, "%s: %-10sms\n", "xbl time", xbl_time);
	seq_printf(m, "%s: %-10sms\n", "abl time", abl_time);
	seq_puts(m, "----------------------------------------\n");

	seq_printf(m, "%10lld.%06ld : ON (Threshold:%5lldms)\n",
		   msec_high(timestamp_on), msec_low(timestamp_on),
		   msec_high(BOOTPROF_THRESHOLD));

	for (i = 0; i < log_count; i++) {
		p = &bootprof[i / LOGS_PER_BUF][i % LOGS_PER_BUF];
		if (!p->comm_event)
			continue;

		seq_printf(m, "%10llu.%06lu :%5d-%-16s: %s\n",
			   msec_high(p->timestamp),
			   msec_low(p->timestamp),
			   p->pid, p->comm_event,
			   p->comm_event + TASK_COMM_LEN);
	}

	seq_printf(m, "%10lld.%06ld : OFF\n",
		   msec_high(timestamp_off), msec_low(timestamp_off));
	seq_puts(m, "----------------------------------------\n");
	return 0;
}

/*** Seq operation of mtprof ****/
static int mt_bootprof_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_bootprof_show, inode->i_private);
}

static const struct proc_ops mt_bootprof_fops = {
	.proc_open = mt_bootprof_open,
	.proc_write = mt_bootprof_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init bootprof_init(void)
{
	struct proc_dir_entry *pe;
	memset(bootprof, 0, sizeof(struct log_t *) * BUF_COUNT);
	bootprof[0] = kcalloc(LOGS_PER_BUF, sizeof(struct log_t),
			GFP_ATOMIC | __GFP_NORETRY | __GFP_NOWARN);
	if (!bootprof[0]) {
		pr_err("[BOOTTIME] fail to allocate memory\n");
		return 0;
	}
	pr_info("BOOTTIME:pbl_time is %s\n", pbl_time);
	pr_info("BOOTTIME:xbl_time is %s\n", xbl_time);
	pr_info("BOOTTIME:abl_time is %s\n", abl_time);
	pe = proc_create("bootprof", 0664, NULL, &mt_bootprof_fops);
	if (!pe) {
		pr_err("[BOOTTIME] fail to create file node\n");
		return 0;
	}
	tp_init();
	pr_info("[BOOTTIME] start mt_bootprof_switch\n");
	mt_bootprof_switch(1);
	boottime_ok = 1;

	return 0;
}

static void __exit bootprof_exit(void)
{
	struct log_t *p = NULL;
	unsigned int i;

	tp_deinit();

	if (log_count > 0) {
		spin_lock(&bootprof_lock);
		enabled = 0;
		for (i = 0; i < log_count; i++) {
			p = &bootprof[i / LOGS_PER_BUF][i % LOGS_PER_BUF];
			kfree(p->comm_event);
		}

		for (i = 0; i < ((log_count / LOGS_PER_BUF) + 1); i++)
			kfree(bootprof[i]);

		spin_unlock(&bootprof_lock);
	}
	remove_proc_entry("bootprof", NULL);
	pr_info("bootprof module exit.\n");
}

early_initcall(bootprof_init);
module_exit(bootprof_exit);
MODULE_LICENSE("GPL v2");
