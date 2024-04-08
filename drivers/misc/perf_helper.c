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
#include <linux/cgroup.h>
#include <linux/memcontrol.h>

#define MAX_RECORD_NUM		2048
#define MAX_ONE_RECORD_SIZE	128
#define BUFF_SIZE           64
#define MAX_KSWAPD_NR	8
#define MAX_KSWAPD_BUF_SIZE	1024
#define MAX_TRIGGER_EVENT_COUNT 10

static DEFINE_SPINLOCK(plr_lock);
static DEFINE_SPINLOCK(mr_lock);

static char reclaim_buff[BUFF_SIZE];

struct perflock_records_buff {
	char msg[MAX_ONE_RECORD_SIZE];
	struct timespec64 key_time;
};

struct perflock_records_buff plr_buff[MAX_RECORD_NUM];
static u32 plr_num;
static u32 index_head;
static u32 index_tail;

static DEFINE_SPINLOCK(ml_lock);
struct mimdlog_buff {
	char msg[MAX_ONE_RECORD_SIZE];
	struct timespec64 key_time;
};

struct mimdlog_buff ml_buff[MAX_RECORD_NUM];
static u32 ml_num;
static u32 ml_index_head;
static u32 ml_index_tail;

struct vote_priority{
	struct list_head list;
	u32 prio;
	u32 cmd;
};

struct trigger_event {
	struct list_head list;
	u64 data;
};
int trigger_event_count = 0;

struct list_head trigger_head;
struct list_head vote_list;

struct kobject *mimd_kobj;

struct task_struct *kswapd_array[MAX_KSWAPD_NR];
static DEFINE_MUTEX(kswapd_lock);
static DEFINE_MUTEX(trigger_event_lock);
static DECLARE_WAIT_QUEUE_HEAD(kswapd_c_poll_wait);
static atomic_t kswapd_c_trigger = ATOMIC_INIT(0);

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

static ssize_t global_reclaim_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{

	char buf[BUFF_SIZE] = {0};
	unsigned long reclaim_size = 0;
	unsigned long nr_reclaim = 0;
	int err = 0;

	if (count > BUFF_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	err = kstrtoul(buf, 10, &reclaim_size);
	if (err != 0)
		return err;

	if(reclaim_size <= 0)
		return 0;

	nr_reclaim = try_to_free_mem_cgroup_pages(NULL, reclaim_size, GFP_KERNEL, true);
	global_reclaim_record(nr_reclaim);

	return count;
}

static const struct proc_ops mimdlog_ops = {
	.proc_open           = mimdlog_open,
	.proc_read           = seq_read,
	.proc_write          = mimdlog_write,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static const struct proc_ops global_reclaim_ops = {
	.proc_open           = global_reclaim_open,
	.proc_read           = seq_read,
	.proc_write          = global_reclaim_write,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static int kswapd_c_show(struct seq_file *seq, void *v)
{
	char buf[MAX_KSWAPD_BUF_SIZE] = {0};
	int i = 0;

	mutex_lock(&kswapd_lock);
	for (i = 0; i < MAX_KSWAPD_NR; i++) {
		if (kswapd_array[i])
			snprintf(buf + strlen(buf), MAX_KSWAPD_BUF_SIZE - strlen(buf), "%d ", kswapd_array[i]->pid);
	}
	buf[strlen(buf) - 1] = '\0';
	mutex_unlock(&kswapd_lock);

	seq_printf(seq, "%s\n", buf);

	return 0;
}

static int kswapd_c_open(struct inode *inode, struct file *file)
{
	struct task_struct *p = NULL;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		if (!p->mm && !strncmp(p->comm, "kswapd0", strlen("kswapd0"))) {
			kswapd_array[0] = p;
			break;
		}
	}
	read_unlock(&tasklist_lock);

	return single_open(file, kswapd_c_show, NULL);
}

static ssize_t kswapd_c_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	pg_data_t *pgdat = NODE_DATA(0);
	struct task_struct *tmp = NULL;
	char buf[BUFF_SIZE] = {0};
	unsigned long nr = 0;
	unsigned long i;

	if (count > BUFF_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	if (kstrtoul(buf, 10, &nr))
		return -EFAULT;

	if (nr < 1 || nr > MAX_KSWAPD_NR) {
		printk(KERN_ERR "extend kswapd nr need in (1 - 8), nr is %d\n", nr);
		return -EINVAL;
	}

	mutex_lock(&kswapd_lock);
	nr--;
	for (i = 1; i < MAX_KSWAPD_NR; i++) {
		if (i <= nr) {
			if (!kswapd_array[i]) {
				tmp = kthread_run(kswapd, pgdat, "kswapd%d", i);
				if (!IS_ERR(tmp))
					kswapd_array[i] = tmp;
			}
		} else {
			if (kswapd_array[i]) {
				kthread_stop(kswapd_array[i]);
				kswapd_array[i] = NULL;
			}
		}
	}
	mutex_unlock(&kswapd_lock);


	atomic_set(&kswapd_c_trigger, 1);

	wake_up(&kswapd_c_poll_wait);

	return count;
}

static __poll_t kswapd_c_poll(struct file *file, poll_table *wait)
{
	__poll_t ret = DEFAULT_POLLMASK;

	poll_wait(file, &kswapd_c_poll_wait, wait);

	if (atomic_read(&kswapd_c_trigger)) {
		atomic_set(&kswapd_c_trigger, 0);
		return ret | EPOLLPRI;
	}

	return ret;
}

static const struct proc_ops kswapd_c_ops = {
	.proc_open           = kswapd_c_open,
	.proc_read           = seq_read,
	.proc_write          = kswapd_c_write,
	.proc_poll           = kswapd_c_poll,
	.proc_lseek          = seq_lseek,
	.proc_release        = single_release,
};

static int vote_priority_init(struct seq_file *seq, void *v) {
	return 0;
}

static int vote_priority_open(struct inode *inode, struct file *file)
{
	return single_open(file, vote_priority_init, NULL);
}

static void kswapd_bind(unsigned int cmd) {
	cpumask_t mask;
	int i = 0;
	strncpy((char *)&mask, (char *)&cmd, sizeof(char));

	mutex_lock(&kswapd_lock);
	for (i = 0; i < MAX_KSWAPD_NR; i++) {
		if (kswapd_array[i])
		    if (set_cpus_allowed_ptr(kswapd_array[i], &mask))
			    printk(KERN_ERR "bind kswapd fail\n");
	}
	mutex_unlock(&kswapd_lock);
}

static ssize_t vote_priority_write(struct file *file, const char __user *userbuf,
		size_t count, loff_t *data)
{
	struct vote_priority *head;
	struct list_head *pos_sort;
	struct list_head *pos_update;
	char buf[BUFF_SIZE] = {0};
	u32 cpu_mask;
	u32 prio = 0;
	u32 cmd = 0;

	if (count > BUFF_SIZE)
		return -EINVAL;

	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	if (sscanf(buf, "%d", &cpu_mask) != 1) {
		pr_err("input param error, can not prase param\n");
		return -EINVAL;
	}

	prio = cpu_mask & 0x00ff;
	cmd = cpu_mask >> 8 & 0xff;

	list_for_each(pos_sort, &vote_list) {
		if (prio >= ((struct vote_priority*)pos_sort)->prio)
			break;
	}

	if (pos_sort == &vote_list || prio > ((struct vote_priority*)pos_sort)->prio) {
		head = (struct vote_priority*)kmalloc(sizeof(struct vote_priority), GFP_KERNEL);
		head->prio = prio;
		head->cmd = cmd;
		list_add_tail(&head->list, pos_sort);
	} else {
		((struct vote_priority*)pos_sort)->cmd = cmd;
	}

	list_for_each_prev(pos_update, &vote_list) {
		if (((struct vote_priority*)pos_update)->cmd != 0xff)
			break;
	}

	if (pos_update == &vote_list)
		kswapd_bind(0xff);
	else
		kswapd_bind(((struct vote_priority*)pos_update)->cmd);

	return count;
}

static const struct proc_ops vote_priority_ops = {
	.proc_open	= vote_priority_open,
	.proc_write	= vote_priority_write,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
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
static ssize_t mimdtrigger_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
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

static struct kobj_attribute mimdtrigger_attr = __ATTR(mimdtrigger, 0664, mimdtrigger_show, mimdtrigger_store);

static struct attribute *mimd_attrs[] = {
	&mimdtrigger_attr.attr,
	NULL,
};

static const struct attribute_group mimd_attr_group = {
	.attrs = mimd_attrs,
};

/* idx can be of type enum memcg_stat_item or node_stat_item. */
static unsigned long memcg_page_state_local(struct mem_cgroup *memcg, int idx)
{
	long x = 0;
	int cpu;

	for_each_possible_cpu(cpu)
		x += per_cpu(memcg->vmstats_percpu->state[idx], cpu);
#ifdef CONFIG_SMP
	if (x < 0)
		x = 0;
#endif
	return x;
}

static ssize_t mem_cgroup_write(struct kernfs_open_file *of,
				char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned long nr_pages;
	unsigned long nr_reclaim = 0;
	int ret;
	int retry = 10;
	unsigned long nr_cache_pages = 0;
	unsigned long nr_anon_pages = 0;
	unsigned long now_anon_pages = 0;
	buf = strstrip(buf);
	ret = kstrtoul(buf, 10, &nr_pages);
	if (ret)
		return ret;

	nr_anon_pages = memcg_page_state_local(memcg, NR_ANON_MAPPED);
	nr_cache_pages = memcg_page_state_local(memcg, NR_FILE_PAGES);

	while (retry--) {
		if (nr_cache_pages == 0)
			break;

		// printk(KERN_ERR "memcg reclaim once nr_cache_pages=%u !\n", nr_cache_pages * PAGE_SIZE);
		nr_reclaim = try_to_free_mem_cgroup_pages(memcg, nr_cache_pages, GFP_KERNEL, true);
		if (!nr_reclaim)
			break;

		now_anon_pages = memcg_page_state_local(memcg, NR_ANON_MAPPED);
		if (now_anon_pages > nr_anon_pages && now_anon_pages - nr_anon_pages > 64) {
			printk(KERN_ERR "force stop memcg reclaim once = %u!\n", now_anon_pages - nr_anon_pages);
			break;
		}

		nr_anon_pages = now_anon_pages;
		if (nr_cache_pages >= nr_reclaim) {
			nr_cache_pages -= nr_reclaim;
		} else {
			nr_cache_pages = 0;
		}
	}

	// printk(KERN_ERR "memcg reclaim once times =%d !\n", 10 - retry);
	return nbytes;
}

static struct cftype memcg_ctrl_files[] = {
	{
		.name = "reclaim_once",
		.write = mem_cgroup_write,
	},
	{},
};

static int __init perf_helper_init(void)
{
	struct proc_dir_entry *entry;
	struct proc_dir_entry *mimd_entry;
	struct proc_dir_entry *global_reclaim_entry;
	struct proc_dir_entry *kswapd_c_entry;
	struct proc_dir_entry *vote_priority_action;
	struct proc_dir_entry *kcompactd_pid;

	INIT_LIST_HEAD(&vote_list);
	INIT_LIST_HEAD(&trigger_head);

	entry = proc_create("perflock_records", 0664, NULL, &perflock_records_ops);
	if (!entry)
		printk(KERN_ERR "%s: create perflock_records node failed\n");

	mimd_entry = proc_create("mimdlog", 0664, NULL, &mimdlog_ops);
	if (!mimd_entry)
		printk(KERN_ERR "%s: create mimdlog node failed\n");

	global_reclaim_entry = proc_create("global_reclaim", 0664, NULL, &global_reclaim_ops);
	if (!global_reclaim_entry)
		printk(KERN_ERR "%s: create global_reclaim node failed\n");

	kswapd_c_entry = proc_create("kswapd_c", 0664, NULL, &kswapd_c_ops);
	if (!kswapd_c_entry)
		printk(KERN_ERR "%s: create kswapd_c node failed\n");

	vote_priority_action = proc_create("vote_priority", 0664, NULL, &vote_priority_ops);
	if(!vote_priority_action)
		printk(KERN_ERR "%s: create vote_priority node failed\n");

	kcompactd_pid = proc_create("kcompactd_pid", 0664, NULL, &kcompactd_pid_ops);
	if(!kcompactd_pid)
		printk(KERN_ERR "%s: create kcompactd_pid node failed\n");

	mimd_kobj = kobject_create_and_add("mimd", &THIS_MODULE->mkobj.kobj);
	if (mimd_kobj) {
		if (sysfs_create_group(mimd_kobj, &mimd_attr_group))
			printk(KERN_ERR "create mimd sysfs nodes group failed\n");
	}

	cgroup_add_legacy_cftypes(&memory_cgrp_subsys, memcg_ctrl_files);

	return 0;
}

static void __exit perf_helper_exit(void)
{
	remove_proc_entry("perflock_records", NULL);
	remove_proc_entry("mimdlog", NULL);
	remove_proc_entry("global_reclaim", NULL);
	remove_proc_entry("kswapd_c", NULL);
	remove_proc_entry("vote_priority", NULL);
	remove_proc_entry("kcompactd_pid", NULL);
	if (!mimd_kobj) {
		sysfs_remove_group(mimd_kobj, &mimd_attr_group);
		kobject_put(mimd_kobj);
	}
}

MODULE_LICENSE("GPL");

module_init(perf_helper_init);
module_exit(perf_helper_exit);
