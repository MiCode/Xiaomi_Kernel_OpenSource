//MIUI ADD: Performance_BoostFramework
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/printk.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/swap.h>
#include <linux/gfp.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/mmzone.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/mmzone.h>
#include <linux/limits.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/cgroup.h>
#include <linux/memcontrol.h>
#include <trace/hooks/vmscan.h>

#define MAX_RECORD_NUM		2048
#define MAX_ONE_RECORD_SIZE	128
#define BUFF_SIZE           64
#define MAX_KSWAPD_NR	8
#define MAX_KSWAPD_BUF_SIZE	1024
#define MAX_EXCEPTION_NUM 128
#define MAX_ONE_EXCEPTION_SIZE 1024
#define MAX_TRIGGER_EVENT_COUNT 10
#define RECLAIM_TARGET_SHIFT 10000000

static char reclaim_buff[BUFF_SIZE];
static struct task_struct *reclaim_task;
static struct wakeup_source *ws;
static atomic_t reclaim_target = ATOMIC_INIT(0);
static atomic_t reclaim_count = ATOMIC_INIT(0);
static cpumask_t reclaim_cpumask;

static DEFINE_SPINLOCK(plr_lock);
static DEFINE_SPINLOCK(mr_lock);
static DEFINE_SPINLOCK(ple_lock);

struct perflock_records_buff {
	char msg[MAX_ONE_RECORD_SIZE];
	struct timespec64 key_time;
};

struct perflock_exception_buff {
	char msg[MAX_ONE_EXCEPTION_SIZE];
	struct timespec64 key_time;
	int msg_count;
};

struct perflock_records_buff plr_buff[MAX_RECORD_NUM];
static u32 plr_num;
static u32 index_head;
static u32 index_tail;

struct perflock_exception_buff ple_buff[MAX_EXCEPTION_NUM];
static u32 ple_num; // real msgbuf count
static u32 ple_write_index; // should write index
static DEFINE_SPINLOCK(ml_lock);
struct mimdlog_buff {
	char msg[MAX_ONE_RECORD_SIZE];
	struct timespec64 key_time;
};

struct mimdlog_buff ml_buff[MAX_RECORD_NUM];
static u32 ml_num;
static u32 ml_index_head;
static u32 ml_index_tail;

struct trigger_event {
	struct list_head list;
	u64 data;
};
int trigger_event_count = 0;
struct list_head trigger_head;
struct kobject *mimd_kobj;
static DEFINE_MUTEX(trigger_event_lock);

static DEFINE_MUTEX(memcg_list_lock);
struct memcg_list {
	struct mem_cgroup *memcg;
	struct list_head list_node;
	int state;
};
struct list_head memcg_list_head;

enum scan_balance {
        SCAN_ANON = 2,
        SCAN_FILE,
};
static atomic_t scan_type = ATOMIC_INIT(0);
static atomic64_t notifier_data = ATOMIC64_INIT(0);

static void perflock_record(const char *perflock_msg)
{
	static int m;

	if (!perflock_msg)
		return;

	if (!spin_trylock(&plr_lock))
		return;

	index_tail = m;
	ktime_get_real_ts64(&plr_buff[m].key_time);
	snprintf(plr_buff[m++].msg, MAX_ONE_RECORD_SIZE, "%s", perflock_msg);

	if (m >= MAX_RECORD_NUM)
		m = 0;

	plr_num++;
	if (plr_num >= MAX_RECORD_NUM) {
		plr_num = MAX_RECORD_NUM;
		index_head = index_tail + 1;
		if (index_head >= MAX_RECORD_NUM)
			index_head = 0;
	}

	spin_unlock(&plr_lock);
}

static int perflock_records_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	int i = 0;

	spin_lock(&plr_lock);
	if (plr_num < MAX_RECORD_NUM) {
		for (i = 0; i < plr_num; i++) {
			rtc_time64_to_tm(plr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					plr_buff[i].msg);
		}
	} else {
		for (i = index_head; i < MAX_RECORD_NUM; i++) {
			rtc_time64_to_tm(plr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					plr_buff[i].msg);
		}

		if (index_head > index_tail) {
			for (i = 0; i <= index_tail; i++) {
				rtc_time64_to_tm(plr_buff[i].key_time.tv_sec, &tm);
				seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
						plr_buff[i].msg);
			}
		}
	}
	spin_unlock(&plr_lock);

	return 0;
}

static int perflock_records_open(struct inode *inode, struct file *file)
{
	return single_open(file, perflock_records_show, NULL);
}

static ssize_t perflock_records_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[MAX_ONE_RECORD_SIZE] = {0};

	if (count > MAX_ONE_RECORD_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	perflock_record(buf);

	return count;
}

static const struct proc_ops perflock_records_ops = {
	.proc_open           = perflock_records_open,
	.proc_read           = seq_read,
	.proc_write		= perflock_records_write,
	.proc_lseek         = seq_lseek,
	.proc_release        = single_release,
};

static void mimd_record(const char *mimdlog_msg)
{
	static int m;

	if (!mimdlog_msg)
		return;

	if (!spin_trylock(&ml_lock))
		return;

	ml_index_tail = m;
	ktime_get_real_ts64(&ml_buff[m].key_time);
	snprintf(ml_buff[m++].msg, MAX_ONE_RECORD_SIZE, "%s", mimdlog_msg);

	if (m >= MAX_RECORD_NUM)
		m = 0;

	ml_num++;
	if (ml_num >= MAX_RECORD_NUM) {
		ml_num = MAX_RECORD_NUM;
		ml_index_head = ml_index_tail + 1;
		if (ml_index_head >= MAX_RECORD_NUM)
			ml_index_head = 0;
	}

	spin_unlock(&ml_lock);
}

static int mimd_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	int i = 0;

	spin_lock(&ml_lock);
	if (ml_num < MAX_RECORD_NUM) {
		for (i = 0; i < ml_num; i++) {
			rtc_time64_to_tm(ml_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					ml_buff[i].msg);
		}
	} else {
		for (i = index_head; i < MAX_RECORD_NUM; i++) {
			rtc_time64_to_tm(ml_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					ml_buff[i].msg);
		}

		if (index_head > index_tail) {
			for (i = 0; i <= index_tail; i++) {
				rtc_time64_to_tm(ml_buff[i].key_time.tv_sec, &tm);
				seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s }\n",
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
						ml_buff[i].msg);
			}
		}
	}
	spin_unlock(&ml_lock);

	return 0;
}

static int mimdlog_open(struct inode *inode, struct file *file)
{
	return single_open(file, mimd_show, NULL);
}

static ssize_t mimdlog_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[MAX_ONE_RECORD_SIZE] = {0};

	if (count > MAX_ONE_RECORD_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	mimd_record(buf);

	return count;
}

static const struct proc_ops mimdlog_ops = {
	.proc_open           = mimdlog_open,
	.proc_read           = seq_read,
	.proc_write          = mimdlog_write,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static void set_scan_type(int type)
{
	switch(type) {
	case SCAN_ANON:
		atomic_set(&scan_type, type);
		break;
	case SCAN_FILE:
		atomic_set(&scan_type, type);
		break;
	default:
		atomic_set(&scan_type, 0);
	}
}

static void restore_scan_type(void)
{
	atomic_set(&scan_type, 0);
}

static void android_vh_tune_scan_type(void *unused, enum scan_balance *scan_balance)
{
	int scan = atomic_read(&scan_type);
	if (scan == 0) {
		return;
	} else {
		*scan_balance = scan;
	}
}

static int global_reclaim_show(struct seq_file *seq, void *v)
{
	spin_lock(&ml_lock);

	seq_printf(seq, "%s", reclaim_buff);

	spin_unlock(&ml_lock);

	return 0;
}

static int global_reclaim_open(struct inode *inode, struct file *file)
{
	return single_open(file, global_reclaim_show, NULL);
}

static void global_reclaim_record(unsigned long nr_reclaim)
{
	if (!spin_trylock(&mr_lock))
		return;

	snprintf(reclaim_buff, BUFF_SIZE, "reclaim %lu pages", nr_reclaim);

	spin_unlock(&mr_lock);
}

static int g_recliam_func(void *data)
{
	unsigned int reclaim_options = MEMCG_RECLAIM_MAY_SWAP;

	while (!kthread_should_stop()) {
		unsigned long nr_reclaim = 0;
		unsigned long reclaim_size = 0;
		unsigned int nr_retries = 10;
		int reclaim_type = 0;

		reclaim_size = atomic_read(&reclaim_target);
		reclaim_type = reclaim_size / RECLAIM_TARGET_SHIFT;
		reclaim_size = reclaim_size % RECLAIM_TARGET_SHIFT;

		pm_wakeup_ws_event(ws, 30000, false);
		set_scan_type(reclaim_type);
		while (nr_reclaim < reclaim_size) {
			unsigned long reclaimed = 0;

			reclaimed = try_to_free_mem_cgroup_pages(NULL,
					reclaim_size - nr_reclaim,
					GFP_KERNEL, reclaim_options);

			if (!nr_retries--)
				break;

			nr_reclaim += reclaimed;
		}
		restore_scan_type();
		printk(KERN_ERR "perf_helper %s reclaimed: %d kbytes\n", __func__, nr_reclaim * 4);
		__pm_relax(ws);

		if (!atomic_dec_and_test(&reclaim_count)) {
			printk(KERN_ERR "%s	%d\n", __func__, atomic_read(&reclaim_count));
		} else {
			global_reclaim_record(nr_reclaim);
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule();
			set_current_state(TASK_RUNNING);
		}
	}

	return 0;
}

static ssize_t global_reclaim_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[BUFF_SIZE] = {0};
	unsigned long reclaim_size = 0;
	int err = 0;

	if (count > BUFF_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	err = kstrtoul(buf, 10, &reclaim_size);
	if (err != 0)
		return err;

	if (reclaim_size <= 0)
		return 0;

	atomic_set(&reclaim_target, reclaim_size);
	atomic_inc(&reclaim_count);

	if (!IS_ERR(reclaim_task))
		wake_up_process(reclaim_task);

	return count;
}

static const struct proc_ops global_reclaim_ops = {
	.proc_open           = global_reclaim_open,
	.proc_read           = seq_read,
	.proc_write          = global_reclaim_write,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static int kswapd_pid_show(struct seq_file *seq, void *v)
{
	struct task_struct *p = NULL;
	pid_t pid = 0;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		if (!p->mm && !strncmp(p->comm, "kswapd0", strlen("kswapd0"))) {
			pid = p->pid;
			break;
		}
	}
	read_unlock(&tasklist_lock);

	seq_printf(seq, "%d", pid);

	return 0;
}

static int kswapd_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, kswapd_pid_show, NULL);
}

static const struct proc_ops kswapd_pid_ops = {
	.proc_open           = kswapd_pid_open,
	.proc_read           = seq_read,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static int kcompactd_pid_show(struct seq_file *seq, void *v)
{
	struct task_struct *p = NULL;
	pid_t pid = 0;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		if (!p->mm && !strncmp(p->comm, "kcompactd0", strlen("kcompactd0"))) {
			pid = p->pid;
			break;
		}
	}
	read_unlock(&tasklist_lock);

	seq_printf(seq, "%d", pid);

	return 0;
}

static int kcompactd_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, kcompactd_pid_show, NULL);
}

static const struct proc_ops kcompactd_pid_ops = {
	.proc_open	= kcompactd_pid_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static ssize_t mimdnotifier_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	ssize_t res = 0;
	s64 data;
	data = atomic64_read(&notifier_data);
	res = snprintf(buf, PAGE_SIZE, "%lld\n", data);

	return res;
}

static ssize_t mimdnotifier_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	s64 data;
	int err;

	err = kstrtoll(buf, 10, &data);
	if (err != 0)
		return err;
	atomic64_set(&notifier_data, data);

	return count;
}

static ssize_t mimdtrigger_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	ssize_t res = 0;

	mutex_lock(&trigger_event_lock);
	if (!list_empty(&trigger_head)) {
		struct trigger_event* event = list_first_entry(&trigger_head, struct trigger_event, list);
		res = snprintf(buf, PAGE_SIZE, "%ld\n", event->data);
		// printk(KERN_ERR "%s: trigger event data=%ld\n", "mimdtrigger", event->data);
		list_del(&(event->list));
		kfree(event);
		--trigger_event_count;
	}
	mutex_unlock(&trigger_event_lock);
	return res;
}

static ssize_t mimdtrigger_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned long trigger_type;
	int err;
	struct trigger_event* event = NULL;

	err = kstrtoul(buf, 10, &trigger_type);
	
	if (err != 0)
		return err;

	mutex_lock(&trigger_event_lock);
	if (trigger_event_count > MAX_TRIGGER_EVENT_COUNT){
		mutex_unlock(&trigger_event_lock);
		printk(KERN_ERR "%s: trigger_event so manay that discard\n", "mimdtrigger");
		return count;
	}

	event = (struct trigger_event*) kmalloc(sizeof(struct trigger_event), GFP_KERNEL);
	if (NULL == event){
		mutex_unlock(&trigger_event_lock);
		printk(KERN_ERR "%s: kmalloc struct trigger_event failed\n", "mimdtrigger");
		return count;
	}
	event->data = trigger_type;
	INIT_LIST_HEAD(&(event->list));
	list_add_tail(&(event->list), &trigger_head); 
	++trigger_event_count;
	mutex_unlock(&trigger_event_lock);

	return count;
}

static void remove_triggerevent_list(struct list_head *head)
{
	struct list_head *pos = NULL, *n = NULL;

	mutex_lock(&trigger_event_lock);
	list_for_each_safe(pos, n, head) {
		struct trigger_event *entry = list_entry(pos, struct trigger_event, list);
		list_del(pos);
		kfree(entry);
		--trigger_event_count;
	}
	mutex_unlock(&trigger_event_lock);
	return;
}

static struct kobj_attribute mimdtrigger_attr = __ATTR(mimdtrigger, 0664, mimdtrigger_show, mimdtrigger_store);
static struct kobj_attribute mimdnotifier_attr = __ATTR(mimdnotifier, 0664, mimdnotifier_show, mimdnotifier_store);

static struct attribute *mimd_attrs[] = {
	&mimdtrigger_attr.attr,
	&mimdnotifier_attr.attr,
	NULL,
};

static const struct attribute_group mimd_attr_group = {
	.attrs = mimd_attrs,
};

// write function
static void perflock_exception(const char *perflock_msg)
{
	if (!perflock_msg)
		return;
	if (!spin_trylock(&ple_lock))
		return;
	// find same request change msgcount
	for (int i = 0; i < ple_num; i++) {
		if (strcmp(perflock_msg, ple_buff[i].msg) == 0 && ple_buff[i].msg_count < INT_MAX) {
			ple_buff[i].msg_count++;
			spin_unlock(&ple_lock);
			return;
		}
	}
	// cant find same request
	ktime_get_real_ts64(&ple_buff[ple_write_index].key_time);
	snprintf(ple_buff[ple_write_index].msg, MAX_ONE_EXCEPTION_SIZE, "%s", perflock_msg);
	// renew write index
	ple_write_index = (ple_write_index + 1) % MAX_EXCEPTION_NUM;
	ple_num++;

	if (ple_num >= MAX_EXCEPTION_NUM) {
		ple_num = MAX_EXCEPTION_NUM;
	}

	spin_unlock(&ple_lock);
}

static int perflock_exception_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	int i = 0;

	spin_lock(&ple_lock);

	if (ple_num < MAX_EXCEPTION_NUM) {
		for (i = 0; i < ple_num; i++) {
			rtc_time64_to_tm(ple_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s count = %d }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					ple_buff[i].msg, ple_buff[i].msg_count + 1);
		}
	} else {
		for (i = ple_write_index; i < MAX_EXCEPTION_NUM; i++) {
			rtc_time64_to_tm(ple_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s count = %d }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					ple_buff[i].msg, ple_buff[i].msg_count + 1);
		}

		for (i = 0; i < ple_write_index; i++) {
			rtc_time64_to_tm(ple_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC { %s count = %d }\n",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
				ple_buff[i].msg, ple_buff[i].msg_count + 1);
		}
	}
	spin_unlock(&ple_lock);

	return 0;
}
static int perflock_exception_open(struct inode *inode, struct file *file)
{
	return single_open(file, perflock_exception_show, NULL);
}

static ssize_t perflock_exception_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	char buf[MAX_ONE_EXCEPTION_SIZE] = {0};

	// printk(KERN_ERR "userbuf size is %d",count);
	if (count > MAX_ONE_EXCEPTION_SIZE) {
		printk(KERN_ERR "perflock_exception userbuf size longer than 1024");
		return -EINVAL;
	}

	if (copy_from_user(buf, userbuf, count)) {
		printk(KERN_ERR "perflock_exception cant copy_from_userbuf");
		return -EFAULT;
	}

	perflock_exception(buf);

	return count;
}

static const struct proc_ops perflock_exception_ops = {
	.proc_open           = perflock_exception_open,
	.proc_read           = seq_read,
	.proc_write		= perflock_exception_write,
	.proc_lseek         = seq_lseek,
	.proc_release        = single_release,
};

static int get_process_state_from_memcg(struct mem_cgroup *memcg) {
	struct memcg_list *memcg_pair = NULL;
	int ret = -1;
	mutex_lock(&memcg_list_lock);
	list_for_each_entry(memcg_pair, &memcg_list_head, list_node) {
		if (memcg_pair->memcg == memcg) {
			ret = memcg_pair->state;
			break;
		}
	}
	mutex_unlock(&memcg_list_lock);
	return ret;
}

static void set_process_state_from_memcg(struct mem_cgroup *memcg, int state) {
	struct memcg_list *memcg_pair = NULL;
	mutex_lock(&memcg_list_lock);
	list_for_each_entry(memcg_pair, &memcg_list_head, list_node) {
		if (memcg_pair && memcg && (memcg_pair->memcg == memcg)) {
			memcg_pair->state = state;
			break;
		}
	}
	mutex_unlock(&memcg_list_lock);
	return;
}

static ssize_t memcg_reclaim_once(struct kernfs_open_file *of,
				char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned long reclaimed_pages;
	unsigned long nr_reclaim = 0;
	int ret;
	int retry = 10;
	unsigned long need_reclaim_pages = 0;
	int reclaim_type = 0;
	buf = strstrip(buf);
	ret = kstrtoul(buf, 10, &reclaimed_pages);
	if (ret)
		return ret;

	need_reclaim_pages = reclaimed_pages;
	reclaimed_pages = 0;
	reclaim_type = need_reclaim_pages / RECLAIM_TARGET_SHIFT;
	need_reclaim_pages = need_reclaim_pages % RECLAIM_TARGET_SHIFT;

	set_process_state_from_memcg(memcg, 0);

	while (retry--) {
		if (need_reclaim_pages == 0)
			break;

		set_scan_type(reclaim_type);
		nr_reclaim = try_to_free_mem_cgroup_pages(memcg, need_reclaim_pages, GFP_KERNEL, true);
		restore_scan_type();
		reclaimed_pages += nr_reclaim;
		if (!nr_reclaim)
			break;
		if (1 == get_process_state_from_memcg(memcg)) {
			printk(KERN_ERR "memcg reclaim once stop!\n");
			break;
		}

		if (need_reclaim_pages >= nr_reclaim) {
			need_reclaim_pages -= nr_reclaim;
		} else {
			need_reclaim_pages = 0;
		}
	}

	printk(KERN_ERR "perf_helper %s reclaimed: %lu kbytes\n", __func__, reclaimed_pages * 4);
	return nbytes;
}

static ssize_t memcg_process_fg(struct kernfs_open_file *of,
				char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	int ret;
	struct memcg_list *memcg_pair = NULL;
	int val = 0;
	buf = strstrip(buf);
	ret = kstrtoint(buf, 10, &val);
	if (ret)
		return ret;
	mutex_lock(&memcg_list_lock);
	list_for_each_entry(memcg_pair, &memcg_list_head, list_node) {
		if (memcg_pair->memcg == memcg) {
			memcg_pair->state = val > 0 ? 1 : 0;
			mutex_unlock(&memcg_list_lock);
			return nbytes;
		}
	}
	mutex_unlock(&memcg_list_lock);

	memcg_pair = (struct memcg_list*) kmalloc(sizeof(struct memcg_list), GFP_KERNEL);
	memcg_pair->memcg = memcg;
	memcg_pair->state = val > 0 ? 1 : 0;
	INIT_LIST_HEAD(&(memcg_pair->list_node));

	mutex_lock(&memcg_list_lock);
	list_add_tail(&(memcg_pair->list_node), &memcg_list_head);
	mutex_unlock(&memcg_list_lock);

	return nbytes;
}

static void remove_memcg_list(struct list_head *head) {
	struct list_head *pos = NULL, *n = NULL;

	mutex_lock(&memcg_list_lock);
	list_for_each_safe(pos, n, head) {
		struct memcg_list *entry = list_entry(pos, struct memcg_list, list_node);
		list_del(pos);
		kfree(entry);
	}
	mutex_unlock(&memcg_list_lock);
	return;
}

static struct cftype memcg_ctrl_files[] = {
	{
		.name = "reclaim_once",
		.write = memcg_reclaim_once,
	},
	{
		.name = "process_fg",
		.write = memcg_process_fg,
	},
	{}
};

static int __init perf_helper_init(void)
{
	struct proc_dir_entry *entry;
	struct proc_dir_entry *mimd_entry;
	struct proc_dir_entry *global_reclaim_entry;
	struct proc_dir_entry *kswapd_pid_entry;
	struct proc_dir_entry *kcompactd_pid;
	struct proc_dir_entry *perflock_exception_entry;

	register_trace_android_vh_tune_scan_type(android_vh_tune_scan_type, NULL);
	reclaim_task = kthread_create(g_recliam_func, NULL, "g_reclaim_thread");
	if (IS_ERR(reclaim_task)) {
		printk(KERN_ERR "%s: create reclaim thread failed\n", __func__);
	} else {
		cpumask_clear(&reclaim_cpumask);
		cpumask_set_cpu(0, &reclaim_cpumask);
		cpumask_set_cpu(1, &reclaim_cpumask);
		cpumask_set_cpu(5, &reclaim_cpumask);
		cpumask_set_cpu(6, &reclaim_cpumask);
		set_cpus_allowed_ptr(reclaim_task, &reclaim_cpumask);

		ws = wakeup_source_register(NULL, "reclaim_wakeup_source");
		if (!ws)
			printk(KERN_ERR "%s: register reclaim wakeup source failed\n", __func__);
	}
	INIT_LIST_HEAD(&trigger_head);
	INIT_LIST_HEAD(&memcg_list_head);

	entry = proc_create("perflock_records", 0664, NULL, &perflock_records_ops);
	if (!entry)
		printk(KERN_ERR "%s: create perflock_records node failed\n", __func__);

	mimd_entry = proc_create("mimdlog", 0664, NULL, &mimdlog_ops);
	if (!mimd_entry)
		printk(KERN_ERR "%s: create mimdlog node failed\n", __func__);

	global_reclaim_entry = proc_create("global_reclaim", 0664, NULL, &global_reclaim_ops);
	if (!global_reclaim_entry)
		printk(KERN_ERR "%s: create global_reclaim node failed\n", __func__);

	kswapd_pid_entry = proc_create("kswapd_pid", 0440, NULL, &kswapd_pid_ops);
	if (!kswapd_pid_entry)
		printk(KERN_ERR "%s: create kswapd_pid node failed\n", __func__);

	kcompactd_pid = proc_create("kcompactd_pid", 0664, NULL, &kcompactd_pid_ops);
	if(!kcompactd_pid)
		printk(KERN_ERR "%s: create kcompactd_pid node failed\n", __func__);

	mimd_kobj = kobject_create_and_add("mimd", &THIS_MODULE->mkobj.kobj);
	if (mimd_kobj) {
		if (sysfs_create_group(mimd_kobj, &mimd_attr_group))
			printk(KERN_ERR "%s create mimd sysfs nodes group failed\n", __func__);
	}

	perflock_exception_entry = proc_create("perflock_exception", 0664, NULL, &perflock_exception_ops);
	if (!perflock_exception_entry)
		printk(KERN_ERR "%s: create perflock_exception node failed\n", __func__);

	cgroup_add_legacy_cftypes(&memory_cgrp_subsys, memcg_ctrl_files);

	return 0;
}

static void __exit perf_helper_exit(void)
{
	remove_proc_entry("perflock_records", NULL);
	remove_proc_entry("mimdlog", NULL);
	remove_proc_entry("global_reclaim", NULL);
	remove_proc_entry("kswapd_pid", NULL);
	remove_proc_entry("kcompactd_pid", NULL);
	if (!mimd_kobj) {
		sysfs_remove_group(mimd_kobj, &mimd_attr_group);
		kobject_put(mimd_kobj);
	}

	wakeup_source_unregister(ws);
	if (!IS_ERR(reclaim_task))
		kthread_stop(reclaim_task);
	remove_triggerevent_list(&trigger_head);
	remove_memcg_list(&memcg_list_head);
}

MODULE_LICENSE("GPL");

module_init(perf_helper_init);
module_exit(perf_helper_exit);
//END Performance_BoostFramework
