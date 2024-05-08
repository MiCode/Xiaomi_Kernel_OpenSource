#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/cgroup.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/psi_types.h>
#include <linux/sched/loadavg.h>

#include <trace/hooks/mm.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/rwsem.h>
#include <trace/hooks/cgroup.h>
#include <trace/hooks/dtask.h>

#include "io_limit.h"

/* CONFIG_PSI_IO_PRESSURE takes effect when psi_group vender hook exists. */
#ifdef CONFIG_PSI_IO_PRESSURE
#include <trace/hooks/psi.h>

/* IO pressure update interval, default 100ms. */
#define STALL_TIME_RENEW_INTERVAL (HZ / 10)

/* When this module determines that there is IO pressure,
 * the blocking time threshold per unit time.
 * unit time is 100ms, unit is ns. */
static int stall_time_threshold = 15000000;
static bool psi_group_init = false;
static struct psi_group *sys_group;
static struct work_struct psi_work;

typedef struct io_pressure_monitor {
	int io_pressure;
	u64 last_stall_time;
	u64 last_mod_time;
} iopm_t;
#endif

/* |63----->12|11------>0| */
/* |limit flag|unlimit bw| */
#define MI_IO_LIMIT_FLAG 0xCC10A55A99663000
#define MI_IO_UNLIMIT_MASK 0xFFF
#define MI_IO_UNLIMIT_BW 0xA00
#define Android_OEM_DATA_VALUE 5

/* use to get rwsem owner */
#define RWSEM_OWNER_FLAGS_MASK (1UL << 0 | 1UL << 1 | 1UL << 2)

/* Default limit period, unit ms */
#define DEFAULT_PERIOD 50;
/* Default limit bandwidth, unit MB */
#define DEFAULT_BG_BANDWIDTH 20
#define DEFAULT_NA_BANDWIDTH 20
#define DEFAULT_FG_BANDWIDTH 60
#define DEFAULT_TA_BANDWIDTH 20

typedef struct bandwidth_control {
	unsigned int period;
	unsigned int bandwidth;
	u32 total_token;
	u32 token_remains;
	u32 token_full_capacity;
	u32 token_capacity_remains;
	u32 token_recovery_interval;
	u64 last_token_dispatch_time;
	wait_queue_head_t token_waitqueue;
	bool token_ok;
	spinlock_t bwc_lock;
#ifdef CONFIG_PSI_IO_PRESSURE
	iopm_t iopm;
#endif
} bwc_t;

/* bandwidth_control types */
enum bwc_type {
	BG, /* Backgroud */
	NA, /* Native */
	FG, /* Froeground */
	TA, /* Tasks in limit_list */
	NR_TYPES,
};

static bwc_t bwc_list[NR_TYPES];

static unsigned int bwc_default_bw[NR_TYPES] = { DEFAULT_BG_BANDWIDTH,
						 DEFAULT_NA_BANDWIDTH,
						 DEFAULT_FG_BANDWIDTH,
						 DEFAULT_TA_BANDWIDTH };
static const char *str_bwc_type[NR_TYPES] = { "BG", "NA", "FG", "TA" };

struct kobject *kobj;

// path : system/core/rootdir/init.rc
enum group_type {
	ROOT = 1,
	FOREGROUND = 2,
	BACKGROUND = 4,
	SYSTEM_BACKGROUND = 5,
	TOP_APP = 7,
	CAMERA_BG = 10,
	CAMERA_LIMIT = 12,
};

enum config_type {
	BW,
	PERIOD,
	STALL_TIME_THRESHOLD,
};

#define NA_FILTER_SIZE 6
static char *NA_filter[NA_FILTER_SIZE] = {
	"init", "dex2oat", "dexopt", "dumpstate", "resize.f2fs", "uncrypt"
};
#define BOOT_TIME 300

#define BG_FILTER_SIZE 1
static char *BG_filter[BG_FILTER_SIZE] = { "P1-database" };

static bool module_switch = false;
static bool debug_switch = false;

static inline void wake_up_queues(void)
{
	int i;
	for (i = 0; i < NR_TYPES; i++) {
		wake_up_all(&bwc_list[i].token_waitqueue);
	}
}

static void reset_bwc(bwc_t *bwc)
{
	spin_lock(&bwc->bwc_lock);
	bwc->total_token = HZ / msecs_to_jiffies(bwc->period);
	bwc->token_remains = bwc->total_token;
	bwc->token_full_capacity = (bwc->bandwidth * 256) / bwc->total_token;
	bwc->token_capacity_remains = 0;
	bwc->token_recovery_interval = msecs_to_jiffies(bwc->period);
	bwc->last_token_dispatch_time = 0;
	bwc->token_ok = true;
	wake_up_all(&bwc->token_waitqueue);
	spin_unlock(&bwc->bwc_lock);
}

static bool init_bwc(int type)
{
	bwc_list[type].period = DEFAULT_PERIOD;
	bwc_list[type].bandwidth = bwc_default_bw[type];
	spin_lock_init(&bwc_list[type].bwc_lock);
	init_waitqueue_head(&bwc_list[type].token_waitqueue);
	reset_bwc(&bwc_list[type]);

	return true;
}

static ssize_t switch_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t len)
{
	int val;

	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;

	module_switch = (val == 0) ? false : true;

	return len;
}

static ssize_t switch_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n", module_switch);

	return len;
}

static ssize_t debug_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t len)
{
	int val;

	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;

	debug_switch = (val == 0) ? false : true;

	return len;
}

static ssize_t debug_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n", debug_switch);

	return len;
}

static ssize_t limit_list_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return rbtree_show(buf);
}

static ssize_t limit_list_store(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf,
				size_t len)
{
	return rbtree_store(buf, len);
}

/* Prevent buffer overflow */
static inline int get_size(int len)
{
	return (PAGE_SIZE - len) > 0 ? (PAGE_SIZE - len) : 0;
}

static ssize_t config_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	int i, len;

	len = 0;
	len += snprintf(buf, get_size(len), "[0]bandwidth:\n");
	for (i = 0; i < NR_TYPES; i++) {
		len += snprintf(buf + len, get_size(len), "\t[%d]%s:%uMB\n", i,
				str_bwc_type[i], bwc_list[i].bandwidth);
	}
	len += snprintf(buf + len, get_size(len), "[1]period:\n");
	for (i = 0; i < NR_TYPES; i++) {
		len += snprintf(buf + len, get_size(len), "\t[%d]%s:%ums\n", i,
				str_bwc_type[i], bwc_list[i].period);
	}
	len += snprintf(buf + len, get_size(len),
			"[2]stall_time_threshold:\n\t[0]global:%uns\n\n",
			stall_time_threshold);

	return len;
}

static ssize_t config_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t len)
{
	int val;
	int type;
	int config_type;

	if (sscanf(buf, "%u:%u:%u", &config_type, &type, &val) != 3)
		return -EINVAL;

	if (type >= NR_TYPES) {
		return -EINVAL;
	}

	val = (val == 0) ? 1 : val;

	switch (config_type) {
	case BW:
		bwc_list[type].bandwidth = val;
		reset_bwc(&bwc_list[type]);
		break;
	case PERIOD:
		val = (val > 200) ? 200 : val;
		bwc_list[type].period = val;
		reset_bwc(&bwc_list[type]);
		break;
	case STALL_TIME_THRESHOLD:
		val = (val > 100000000) ? 100000000 : val;
		stall_time_threshold = val;
		break;
	}

	return len;
}

static struct kobj_attribute config_attr = __ATTR_RW(config);
static struct kobj_attribute limit_list_attr = __ATTR_RW(limit_list);
static struct kobj_attribute switch_attr = __ATTR_RW(switch);
static struct kobj_attribute debug_attr = __ATTR_RW(debug);

static struct attribute *bwc_attrs[] = {
	&config_attr.attr,
	&limit_list_attr.attr,
	&switch_attr.attr,
	&debug_attr.attr,
	NULL,
};

ATTRIBUTE_GROUPS(bwc);

static struct kobject *sysfs_create(char *p_name)
{
	int err;
	struct kobject *temp_kobj;

	temp_kobj = kobject_create_and_add(p_name, kernel_kobj);
	if (!temp_kobj) {
		pr_err("%s limit: failed to create sysfs node.\n", p_name);
		return NULL;
	}

	err = sysfs_create_groups(temp_kobj, bwc_groups);
	if (err) {
		pr_err("%s limit: failed to create sysfs attrs.\n", p_name);
		kobject_del(temp_kobj);
		kobject_put(temp_kobj);
		return NULL;
	}

	return temp_kobj;
}

static inline unsigned long elapsed_jiffies(unsigned long start)
{
	unsigned long end = jiffies;

	if (end >= start)
		return (unsigned long)(end - start);

	return (unsigned long)(end + (MAX_JIFFY_OFFSET - start) + 1);
}

static inline bool native_need_limit(char *list[], int size)
{
	int index;

	for (index = 0; index < size; index++) {
		if (0 ==
		    strncmp(current->comm, list[index], strlen(list[index])))
			return false;
	}
	return true;
}

#ifdef CONFIG_PSI_IO_PRESSURE
static void renew_io_pressure(bwc_t *bwc, int type)
{
	u64 stall_time;
	u64 diff;
	int nr_interval;

	if (sys_group) {
		stall_time = sys_group->total[PSI_POLL][PSI_IO_SOME];
		nr_interval = elapsed_jiffies(bwc->iopm.last_mod_time) /
			      STALL_TIME_RENEW_INTERVAL;
		diff = (stall_time - bwc->iopm.last_stall_time) / nr_interval;
		bwc->iopm.last_stall_time = stall_time;
		bwc->iopm.last_mod_time = jiffies;

		if (diff < stall_time_threshold && bwc->token_ok) {
			bwc->iopm.io_pressure =
				bwc->iopm.io_pressure > nr_interval ?
					(bwc->iopm.io_pressure - nr_interval) :
					0;
		} else {
			/* To ensure that the system's stall time remains below
             * the threshold for at least 1 second, the io_pressure
             * will be reduced to 0. */
			bwc->iopm.io_pressure = HZ / STALL_TIME_RENEW_INTERVAL;
		}
		if (debug_switch) {
			pr_info("[iolimit][pressure] type:%d pressure:%d diff:%llu token_ok:%d",
				type, bwc->iopm.io_pressure, diff,
				bwc->token_ok);
		}
	}
}

static void init_iopm(void)
{
	int i;
	for (i = 0; i < NR_TYPES; i++) {
		bwc_list[i].iopm.io_pressure = 0;
		bwc_list[i].iopm.last_stall_time =
			sys_group->total[PSI_POLL][PSI_IO_SOME];
		bwc_list[i].iopm.last_mod_time = jiffies;
	}
}

static void init_psi_group(void *data, struct psi_group *group)
{
	if (!psi_group_init) {
		psi_group_init = true;
		sys_group = group;
		init_iopm();
		schedule_work(&psi_work);
	}
}

static void psi_work_handler(struct work_struct *work)
{
	unregister_trace_android_vh_psi_group(init_psi_group, NULL);
}
#endif

static inline int get_cpuset_cgroup(struct task_struct *task)
{
	struct cgroup_subsys_state *css;
	int id = -1;

	css = task_get_css(task, cpuset_cgrp_id);
	if (css) {
		id = css->id;
		css_put(css);
	}
	return id;
}

static void __limit_by_group(int type)
{
	u64 interval_time;
	DECLARE_WAITQUEUE(wait, current);
	bwc_t *bwc = &bwc_list[type];

	spin_lock(&bwc->bwc_lock);
#ifdef CONFIG_PSI_IO_PRESSURE
	if (elapsed_jiffies(bwc->iopm.last_mod_time) >
	    STALL_TIME_RENEW_INTERVAL) {
		renew_io_pressure(bwc, type);
	}
#endif

	/* push token */
	if (0 != bwc->last_token_dispatch_time) {
		interval_time = elapsed_jiffies(bwc->last_token_dispatch_time);
		if (interval_time >= bwc->token_recovery_interval &&
		    bwc->token_remains < bwc->total_token) {
			bwc->token_remains++;
			bwc->token_ok = true;
			bwc->last_token_dispatch_time = jiffies;
			spin_unlock(&bwc->bwc_lock);
			wake_up_all(&bwc->token_waitqueue);
			spin_lock(&bwc->bwc_lock);
		}
	}

#ifdef CONFIG_PSI_IO_PRESSURE
	if (!bwc->iopm.io_pressure) {
		spin_unlock(&bwc->bwc_lock);
		return;
	}
#endif

	spin_unlock(&bwc->bwc_lock);
	if ((current->android_oem_data1[Android_OEM_DATA_VALUE] &
	     MI_IO_LIMIT_FLAG) == MI_IO_LIMIT_FLAG) {
		current->android_oem_data1[Android_OEM_DATA_VALUE] =
			(current->android_oem_data1[Android_OEM_DATA_VALUE] &
			 MI_IO_UNLIMIT_MASK) > 0 ?
				(current->android_oem_data1
					 [Android_OEM_DATA_VALUE] -
				 1) :
				0;
		return;
	}
	spin_lock(&bwc->bwc_lock);

	/* wait token */
	if (0 == bwc->token_remains) {
		spin_unlock(&bwc->bwc_lock);
		current->android_oem_data1[Android_OEM_DATA_VALUE] =
			MI_IO_LIMIT_FLAG;
		if (debug_switch) {
			pr_info("[iolimit][wait] type:%d tgid:%d pid:%d comm:%s adj:%d cgroup_id:%d",
				type, current->tgid, current->pid,
				current->comm, current->signal->oom_score_adj,
				get_cpuset_cgroup(current));
		}
		add_wait_queue(&bwc->token_waitqueue, &wait);
		schedule_timeout_interruptible(bwc->token_recovery_interval);
		remove_wait_queue(&bwc->token_waitqueue, &wait);
		if ((current->android_oem_data1[Android_OEM_DATA_VALUE] &
		     MI_IO_UNLIMIT_MASK) <= 1) {
			current->android_oem_data1[Android_OEM_DATA_VALUE] = 0;
		}
		spin_lock(&bwc->bwc_lock);
	}

	/* token consume */
	bwc->token_capacity_remains++;
	if (bwc->token_capacity_remains > bwc->token_full_capacity) {
		if (bwc->token_remains > 0) {
			bwc->token_remains--;
			if (0 == bwc->last_token_dispatch_time) {
				bwc->last_token_dispatch_time = jiffies;
			}
		}
		if (0 == bwc->token_remains) {
			bwc->token_ok = false;
		}
		bwc->token_capacity_remains = 0;
	}
	spin_unlock(&bwc->bwc_lock);
}

static void limit_by_group(short adj, int cgroup_id)
{
	if (cgroup_id == FOREGROUND && adj > PERCEPTIBLE_APP_ADJ) {
		__limit_by_group(FG);
	} else if ((adj >= PREVIOUS_APP_ADJ || cgroup_id == BACKGROUND ||
		    cgroup_id == CAMERA_BG || cgroup_id == CAMERA_LIMIT) &&
		   native_need_limit(BG_filter, BG_FILTER_SIZE)) {
		__limit_by_group(BG);
	} else if (cgroup_id == ROOT && adj == NATIVE_ADJ &&
		   native_need_limit(NA_filter, NA_FILTER_SIZE)) {
		__limit_by_group(NA);
	} else if (check_pid_in_limit_list(current->pid)) {
		__limit_by_group(TA);
	}
}

static bool is_need_limit(short adj, int cgroup_id)
{
	if (adj == FOREGROUND_APP_ADJ || adj == SERVICE_ADJ ||
	    adj == SERVICE_B_ADJ || rt_task(current) || cgroup_id == TOP_APP ||
	    cgroup_id == SYSTEM_BACKGROUND) {
		return false;
	}

	if (fatal_signal_pending(current)) {
		return false;
	}

	if (unlikely(current->flags & PF_KTHREAD)) {
		return false;
	}

	return true;
}

static void ctl_dirty_rate(void *data, void *extra)
{
	short adj;
	int cgroup_id;
	static bool already_booted = false;

	if (!module_switch) {
		return;
	}

	if (!already_booted) {
		if (ktime_get_boottime_seconds() < BOOT_TIME) {
			return;
		} else {
			already_booted = true;
		}
	}

	cgroup_id = get_cpuset_cgroup(current);
	adj = current->signal->oom_score_adj;

	if (debug_switch) {
		pr_info("[iolimit][dirty] tgid:%d pid:%d comm:%s adj:%d cgroup_id:%d",
			current->tgid, current->pid, current->comm, adj,
			cgroup_id);
	}

	if (is_need_limit(adj, cgroup_id)) {
		limit_by_group(adj, cgroup_id);
	}
}

static inline void init_bwcs(void)
{
	int i;
	for (i = 0; i < NR_TYPES; i++) {
		init_bwc(i);
	}
}

static void cgroup_renew(void *data, int ret, struct task_struct *leader)
{
	wake_up_queues();
}

static void rwsem_avoid_slow_path(void *ignore, struct rw_semaphore *sem)
{
	short adj;
	int cgroup_id;
	struct task_struct *owner;

	owner = (struct task_struct *)(atomic_long_read(&sem->owner) &
				       ~RWSEM_OWNER_FLAGS_MASK);
	if (owner && owner->android_oem_data1[Android_OEM_DATA_VALUE] ==
			     MI_IO_LIMIT_FLAG) {
		cgroup_id = get_cpuset_cgroup(current);
		adj = current->signal->oom_score_adj;
		if (!is_need_limit(adj, cgroup_id)) {
			owner->android_oem_data1[Android_OEM_DATA_VALUE] =
				MI_IO_LIMIT_FLAG | MI_IO_UNLIMIT_BW;
			wake_up_queues();
		}
	}
}

static void delete_pid(void *nouse, struct task_struct *task)
{
	del_limit_node(task->tgid);
}

static void sysfs_destory(void)
{
	if (!kobj)
		return;

	sysfs_remove_groups(kobj, bwc_groups);
	kobject_del(kobj);
	kobject_put(kobj);
	kobj = NULL;
}

int __init mi_io_limit_init(void)
{
	if (!init_limit_node_cachep()) {
		pr_err("mi_io_limit: module init error!");
		return 0;
	}

	init_bwcs();
	kobj = sysfs_create("mi_io_limit");
	register_trace_android_rvh_ctl_dirty_rate(ctl_dirty_rate, NULL);
	register_trace_android_vh_free_task(delete_pid, NULL);
	register_trace_android_vh_cgroup_set_task(cgroup_renew, NULL);
	register_trace_android_vh_rwsem_wake(rwsem_avoid_slow_path, NULL);
#ifdef CONFIG_PSI_IO_PRESSURE
	INIT_WORK(&psi_work, psi_work_handler);
	register_trace_android_vh_psi_group(init_psi_group, NULL);
	pr_info("mi_io_limit: PSI register!");
#endif
	module_switch = true;
	pr_info("mi_io_limit: module init!");
	return 0;
}

void __exit mi_io_limit_exit(void)
{
	/* The vendor hook of type "rvh" cannot be unload,
	 * only disable the feature switch here. */
	module_switch = false;
	unregister_trace_android_vh_free_task(delete_pid, NULL);
	unregister_trace_android_vh_cgroup_set_task(cgroup_renew, NULL);
	unregister_trace_android_vh_rwsem_wake(rwsem_avoid_slow_path, NULL);
	sysfs_destory();
	wake_up_queues();
	pr_info("mi_io_limit: module exit!");
}

module_init(mi_io_limit_init);
module_exit(mi_io_limit_exit);
MODULE_LICENSE("GPL");