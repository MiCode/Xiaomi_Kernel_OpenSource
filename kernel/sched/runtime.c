#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#define TRACE_UID_MAX  81920

#ifndef HISTORY_ITMES
#define HISTORY_ITMES 4
#endif

#ifndef HISTORY_WINDOWS
#define HISTORY_WINDOWS 6
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define TOP_TASKS 3

enum package_type {
	TOP_ALL = 0,
	TOP_ON_BCORE,
	TOP_ON_LCORE,
	PACKAGE_TYPES,
};

static struct runtime_info {
	u64 update_time;
} package_runtime_info[HISTORY_WINDOWS];

static char *top_package[PACKAGE_TYPES] = {
	"g:     top_package",
	"bcore: top_package",
	"lcore: top_package",
};

static int curr_runtime_items;
static int runtime_traced_uid = 1000;
static int uid_max_value;
static int top_package_uid[PACKAGE_TYPES];
static int runtime_window_size = 300;
static int runtime_enable;
static int traced_window = 3;
static int package_runtime_disable;
static u64 sys_update_time = 0xffffffff;
static struct user_struct user_zero;
static struct proc_dir_entry *package_rootdir;
static struct proc_dir_entry *package_showall_entry;
static struct proc_dir_entry *package_tracestat_entry;
static struct proc_dir_entry *package_top_bcore_entry;
static struct proc_dir_entry *package_top_lcore_entry;
static struct proc_dir_entry *package_top_entry;
static struct proc_dir_entry *package_trace_entry;
static struct proc_dir_entry *package_window_size_entry;
static struct proc_dir_entry *package_develop_mode_entry;
static struct proc_dir_entry *package_runtime_entry;
static struct proc_dir_entry *package_trace_window;
static struct work_struct package_runtime_roll;

/*static u64 top_app_load[TOP_TASKS] = {0};*/
static DECLARE_BITMAP(package_uid_bit, (TRACE_UID_MAX+1));
static int develop_mode;

void init_package_runtime_info(struct user_struct *user);
void package_runtime_hook(u64 now);
void update_task_runtime_info(struct task_struct *tsk, u64 delta, int cpu);
/*int is_top_app(struct task_struct *tsk);*/

int __weak big_core(int cpu)
{
	return cpu >= (num_present_cpus() >> 1);
}

static int runtime_show_all(struct seq_file *m, void *v)
{
	int bcluster_usage = 0;
	int lcluster_usage = 0;
	int bcluster_bias = 0;
	int app_run_on_bcluster, app_run_on_lcluster;
	int top_package_load[PACKAGE_TYPES];
	u64 package_runtime;
	struct user_struct *user;
	kuid_t uid;
	int i, bit;
	int last_uid = MIN(uid_max_value + 1, TRACE_UID_MAX-1);
	int print_user_zero = 0;
#if 0
	int prev_index = (curr_runtime_items + 3)%HISTORY_ITMES;
	int pprev_index = (curr_runtime_items + 2)%HISTORY_ITMES;
	u64 delta_exec = jiffies_to_nsecs(package_runtime_info[prev_index].update_time -
			package_runtime_info[pprev_index].update_time);
#else
	int pprev_index = HISTORY_ITMES + 1;
	int prev_index = HISTORY_ITMES;
	u64 delta_exec = jiffies_to_nsecs(get_jiffies_64() -
		       package_runtime_info[pprev_index].update_time);
	package_runtime_info[prev_index].update_time = get_jiffies_64();
#endif

	if (develop_mode) {
		pprev_index = HISTORY_ITMES + 1;
		prev_index = HISTORY_ITMES;
		package_runtime_info[prev_index].update_time = get_jiffies_64();
		delta_exec = jiffies_to_nsecs(get_jiffies_64() -
			       package_runtime_info[pprev_index].update_time);
	}

	if (delta_exec <= 0)
		return 0;

	memset(top_package_load, 0, PACKAGE_TYPES * sizeof(int));
	seq_printf(m, "time info: from [%llu]s to [%llu]s ago\n",
			(get_jiffies_64() - package_runtime_info[prev_index].update_time)/HZ,
			(get_jiffies_64() - package_runtime_info[pprev_index].update_time)/HZ);

	for_each_set_bit(bit, package_uid_bit, last_uid) {
		uid.val = bit;
		user = find_user(uid);

print_user_zero:
		if (user) {
			package_runtime = (user->big_cluster_runtime[prev_index] +
					user->little_cluster_runtime[prev_index]) -
					(user->big_cluster_runtime[pprev_index] +
					user->little_cluster_runtime[pprev_index]);

			if (!package_runtime)
				continue;

			if ((user->big_cluster_runtime[prev_index]
				< user->big_cluster_runtime[pprev_index])
				||  (user->little_cluster_runtime[prev_index]
				< user->little_cluster_runtime[pprev_index])) {

				/*seq_printf(m, "WR:curr_runtime_items %d  prev_index %d,
				pprev_index %d %llu %llu prev big %llu,
				pprev big %llu, prev little %llu, pprev little %llu \n",
				curr_runtime_items, prev_index, pprev_index,
				user->big_cluster_runtime[HISTORY_ITMES],
				user->little_cluster_runtime[HISTORY_ITMES],
				user->big_cluster_runtime[prev_index],
				user->big_cluster_runtime[pprev_index],
				user->little_cluster_runtime[prev_index],
				user->little_cluster_runtime[pprev_index]);*/
				continue;
			}

			app_run_on_bcluster = (int)((user->big_cluster_runtime[prev_index] -
					user->big_cluster_runtime[pprev_index])*10000/delta_exec);
			if (app_run_on_bcluster > top_package_load[TOP_ON_BCORE]) {
				/*top load package in big cluster*/
				top_package_load[TOP_ON_BCORE] = app_run_on_bcluster;
				top_package_uid[TOP_ON_BCORE] = bit;
			}

			if (app_run_on_bcluster > 0)
				bcluster_usage += app_run_on_bcluster;

			app_run_on_lcluster = (int)((user->little_cluster_runtime[prev_index] -
					user->little_cluster_runtime[pprev_index])*10000/delta_exec);
			if (app_run_on_lcluster > top_package_load[TOP_ON_LCORE]) {
				/*top load package in little cluster*/
				top_package_load[TOP_ON_LCORE] = app_run_on_lcluster;
				top_package_uid[TOP_ON_LCORE] = bit;
			}

			if (app_run_on_lcluster > 0)
				lcluster_usage += app_run_on_lcluster;

			if (app_run_on_bcluster + app_run_on_lcluster > top_package_load[TOP_ALL]) {
				/*top load package*/
				top_package_load[TOP_ALL] = app_run_on_bcluster + app_run_on_lcluster;
				top_package_uid[TOP_ALL] = bit;
			}

			bcluster_bias = (int)((user->big_cluster_runtime[prev_index]
					- user->big_cluster_runtime[pprev_index])*10000/package_runtime);
			if (bcluster_bias < 0)
				bcluster_bias = 0;

			seq_printf(m,
				"%5d: <b>%16llu [%3d.%02d]\t<l>: %16llu [%3d.%02d]\t<b/b+l>: [%3d.%02d],\n", bit,
				(user->big_cluster_runtime[prev_index] -
				user->big_cluster_runtime[pprev_index]),
				app_run_on_bcluster/100, app_run_on_bcluster%100,
				(user->little_cluster_runtime[prev_index]
				- user->little_cluster_runtime[pprev_index]),
				app_run_on_lcluster/100, app_run_on_lcluster%100,
				bcluster_bias/100, bcluster_bias%100);

			free_uid(user);
		}

		if (bit == last_uid && !print_user_zero) {
			user = &user_zero;
			print_user_zero = 1;
			printk("-----------non user info -----------------\n");
			goto print_user_zero;
		}
	}

	seq_printf(m, "\nusage--- big cluster:[%d.%d] little cluster :[%d.%d]\n",
			bcluster_usage/100, bcluster_usage%100,
			lcluster_usage/100, lcluster_usage%100);

	seq_printf(m, "top load package info--- 0 for b/l cluster,1 for b cluster, 2 for l cluster\n");

	for (i = 0; i < PACKAGE_TYPES; i++)
		seq_printf(m, "%s: %d %3d.%02d\n", top_package[i], top_package_uid[i],
				top_package_load[i]/100, top_package_load[i]%100);

	return 0;
}

static int runtime_open_all(struct inode *inode, struct file *file)
{
	return single_open(file, runtime_show_all, NULL);
}

static const struct file_operations package_show_all_fops = {
	.open      = runtime_open_all,
	.read      = seq_read,
	.llseek    = seq_lseek,
	.release   = single_release,
};

static int package_runtime_show(struct seq_file *m, void *v)
{
	struct task_struct *p, *tsk;
	int app_run_on_bcluster, app_run_on_lcluster, bcluster_bias;
	u64 app_run_btime, app_run_ltime;
	int bcluster_usage = 0;
	int lcluster_usage = 0;
	int traced_uid = *((int *)m->private);

#if 0
	int prev_index = (curr_runtime_items + 3)%HISTORY_ITMES;
	int pprev_index = (curr_runtime_items + 2)%HISTORY_ITMES;
	u64 delta_exec = jiffies_to_nsecs(package_runtime_info[prev_index].update_time
			- package_runtime_info[pprev_index].update_time);
#else
	int	pprev_index = HISTORY_ITMES + 1;
	int	prev_index = HISTORY_ITMES;
	u64	delta_exec = jiffies_to_nsecs(get_jiffies_64() -
				package_runtime_info[pprev_index].update_time);
	package_runtime_info[prev_index].update_time = get_jiffies_64();
#endif

	if (develop_mode) {
		pprev_index = HISTORY_ITMES + 1;
		prev_index = HISTORY_ITMES;
		package_runtime_info[prev_index].update_time = get_jiffies_64();
		delta_exec = jiffies_to_nsecs(get_jiffies_64() -
				package_runtime_info[pprev_index].update_time);
	}

	seq_printf(m, "traced uid %d  time delta is %llu\n", traced_uid, delta_exec);
	seq_printf(m, "time info: from [%llu]s to [%llu]s ago\n",
			(get_jiffies_64() - package_runtime_info[prev_index].update_time)/HZ,
			(get_jiffies_64() - package_runtime_info[pprev_index].update_time)/HZ);

	rcu_read_lock();
	for_each_process(tsk) {
		if (task_uid(tsk).val != traced_uid)
			continue;

		for_each_thread (tsk, p) {
			app_run_btime = p->big_cluster_runtime[prev_index]
				- p->big_cluster_runtime[pprev_index];
			app_run_ltime = p->little_cluster_runtime[prev_index]
				- p->little_cluster_runtime[pprev_index];

			if (!(app_run_btime + app_run_btime))
				continue;

			if ((p->big_cluster_runtime[prev_index]
				< p->big_cluster_runtime[pprev_index])
				|| (p->little_cluster_runtime[prev_index]
				< p->little_cluster_runtime[pprev_index])) {
				/*seq_printf(m, "WR:curr_runtime_items %d prev_index %d, pprev_index %d
				%llu %llu prev big %llu, pprev big %llu, prev little %llu, pprev little
				%llu \n", curr_runtime_items, prev_index, pprev_index,
				p->big_cluster_runtime[HISTORY_ITMES],
				p->little_cluster_runtime[HISTORY_ITMES],
				p->big_cluster_runtime[prev_index],
				p->big_cluster_runtime[pprev_index],
				p->little_cluster_runtime[prev_index],
				p->little_cluster_runtime[pprev_index]);*/

				continue;
			}

			app_run_on_bcluster = (int)(app_run_btime*10000/delta_exec);
			if (app_run_on_bcluster > 0)
				bcluster_usage += app_run_on_bcluster;
			else
				app_run_on_bcluster = 0;


			app_run_on_lcluster = (int)(app_run_ltime*10000/delta_exec);
			if (app_run_on_lcluster > 0)
				lcluster_usage += app_run_on_lcluster;
			else
				app_run_on_lcluster = 0;

			bcluster_bias = (int)(app_run_btime*10000/(app_run_btime + app_run_ltime));
			if (bcluster_bias < 0)
				bcluster_bias = 0;
			seq_printf(m, "%5d,%16s,b:%16llu [%3d.%02d],l:%16llu [%3d.%02d], \
					b/b+l:[%3d.%03d],\n", p->pid,
					p->comm, app_run_btime, app_run_on_bcluster/100,
					app_run_on_bcluster%100,
					app_run_ltime,  app_run_on_lcluster/100,
					app_run_on_lcluster%100, bcluster_bias/100,
					bcluster_bias%100);
		}
	}

	seq_printf(m, "usage total big:[%d.%d] little:[%d.%d] \n", bcluster_usage/100,
			bcluster_usage%100, lcluster_usage/100, lcluster_usage%100);
	rcu_read_unlock();
	return 0;
}

static int package_runtime_open(struct inode *inode, struct file *file)
{
	struct seq_file *m;
	int ret = single_open(file, package_runtime_show, NULL);
	if (!ret) {
			m = file->private_data;
			m->private = &runtime_traced_uid;
	}
	return ret;
}

static ssize_t trace_package_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *f_ops)
{
	unsigned char buffer[32] = {0};
	int value = 0;
	if (copy_from_user(buffer, buf, (count > 32 ? 32 : count)))
		return count;

	value = simple_strtol(buffer, NULL, 10);
	printk(KERN_ERR "got user info: %d....\n", value);
	runtime_traced_uid = value;
	return count;
}

static const struct file_operations package_trace_fops = {
	.open      = package_runtime_open,
	.read      = seq_read,
	.write     = trace_package_write,
	.llseek    = seq_lseek,
	.release   = single_release,
};

static int top_package_on_bcore_open(struct inode *inode, struct file *file)
{
	struct seq_file *m;
	int ret = single_open(file, package_runtime_show, NULL);
	if (!ret) {
			m = file->private_data;
			m->private = &top_package_uid[TOP_ON_BCORE];
	}
	return ret;
}

static const struct file_operations package_top_bcore_fops = {
	.open      = top_package_on_bcore_open,
	.read      = seq_read,
	.llseek    = seq_lseek,
	.release   = single_release,
};

static int top_package_on_lcore_open(struct inode *inode, struct file *file)
{
	struct seq_file *m;
	int ret = single_open(file, package_runtime_show, NULL);
	if (!ret) {
			m = file->private_data;
			m->private = &top_package_uid[TOP_ON_LCORE];
	}
	return ret;
}

static const struct file_operations package_top_lcore_fops = {
	.open      = top_package_on_lcore_open,
	.read      = seq_read,
	.llseek    = seq_lseek,
	.release   = single_release,
};

static int top_package_open(struct inode *inode, struct file *file)
{
	struct seq_file *m;
	int ret = single_open(file, package_runtime_show, NULL);
	if (!ret) {
			m = file->private_data;
			m->private = &top_package_uid[TOP_ALL];
	}
	return ret;
}

static const struct file_operations package_top_fops = {
	.open      = top_package_open,
	.read      = seq_read,
	.llseek    = seq_lseek,
	.release   = single_release,
};

static int package_trace_stat(struct seq_file *m, void *v)
{
	seq_printf(m, "traced uid  is %d\n", runtime_traced_uid);
	seq_printf(m, "traced max uid is %d\n", uid_max_value);
	return 0;
}

static int package_tracestat_open(struct inode *inode, struct file *file)
{
	return single_open(file, package_trace_stat, NULL);
}

static const struct file_operations package_tracestat_fops = {
	.open      = package_tracestat_open,
	.read      = seq_read,
	.llseek    = seq_lseek,
	.release   = single_release,
};

static void set_traced_window(int window_idx)
{
	struct user_struct *user;
	struct task_struct *tsk, *ptsk;
	kuid_t uid;
	int bit;

	if (window_idx < 0 || window_idx >= HISTORY_ITMES)
		window_idx = (curr_runtime_items + 2)%HISTORY_ITMES;
	else
		window_idx = (HISTORY_ITMES + curr_runtime_items - window_idx)
			% HISTORY_ITMES;

	if (develop_mode)
		window_idx = HISTORY_ITMES;

	for_each_set_bit(bit, package_uid_bit,
			MIN(uid_max_value + 1, TRACE_UID_MAX - 1)) {
		uid.val = bit;
		user = find_user(uid);

		if (user) {
			user->big_cluster_runtime[HISTORY_ITMES+1]
				= user->big_cluster_runtime[window_idx];
			user->little_cluster_runtime[HISTORY_ITMES+1]
				= user->little_cluster_runtime[window_idx];
			free_uid(user);
		} else {
			user_zero.big_cluster_runtime[HISTORY_ITMES+1]
				= user_zero.big_cluster_runtime[window_idx];
			user_zero.little_cluster_runtime[HISTORY_ITMES+1]
				= user_zero.little_cluster_runtime[window_idx];
		}

		if (atomic_read(&user->__count) == 1) {
			clear_bit(user->uid.val, package_uid_bit);
			free_uid(user);
		}
	}
	package_runtime_info[HISTORY_ITMES+1].update_time
		= package_runtime_info[window_idx].update_time;

	rcu_read_lock();
	for_each_process(ptsk) {
		for_each_thread(ptsk, tsk) {
			tsk->big_cluster_runtime[HISTORY_ITMES+1]
				= tsk->big_cluster_runtime[window_idx];
			tsk->little_cluster_runtime[HISTORY_ITMES+1]
				= tsk->little_cluster_runtime[window_idx];

		}
	}
	rcu_read_unlock();
}

static int traced_window_show(struct seq_file *m, void *v)
{
	seq_printf(m, "traced window is last %d\n", traced_window);
	set_traced_window(traced_window);
	return 0;
}

static int package_open_traced_window(struct inode *inode, struct file *file)
{
	return single_open(file, traced_window_show, NULL);
}

static ssize_t traced_window_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_ops)
{
	int window_idx = 0;
	unsigned char buffer[32] = {0};

	if (copy_from_user(buffer, buf, (count > 32 ? 32 : count)))
		return count;

	window_idx = simple_strtol(buffer, NULL, 10);
	traced_window = window_idx;

	printk(KERN_ERR "traced window %d\n", window_idx);
	set_traced_window(traced_window);

	return count;
}

static const struct file_operations package_tracedwindow_fops = {
	.open      = package_open_traced_window,
	.read      = seq_read,
	.write     = traced_window_write,
	.llseek    = seq_lseek,
	.release   = single_release,
};

static int window_size_show(struct seq_file *m, void *v)
{
	seq_printf(m, "window size is %d\n", runtime_window_size);
	return 0;
}

static int package_open_window_size(struct inode *inode, struct file *file)
{
	return single_open(file, window_size_show, NULL);
}

static ssize_t window_size_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_ops)
{
	unsigned char buffer[32] = {0};
	int value = 0;
	if (copy_from_user(buffer, buf, (count > 32 ? 32 : count)))
		return count;

	value = simple_strtol(buffer, NULL, 10);
	printk(KERN_ERR "got user info: %d....\n", value);
	runtime_window_size = value;
	return count;
}

static const struct file_operations package_windowsize_fops = {
	.open      = package_open_window_size,
	.read      = seq_read,
	.write     = window_size_write,
	.llseek    = seq_lseek,
	.release   = single_release,
};

static int develop_mode_show(struct seq_file *m, void *v)
{
	seq_printf(m, "develop mode %s\n", develop_mode ? "open" : "close");
	return 0;
}

static int develop_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, develop_mode_show, NULL);
}

static ssize_t develop_mode_set(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_ops)
{
	unsigned char buffer[32] = {0};
	int value = 0;
	if (copy_from_user(buffer, buf, (count > 32 ? 32 : count)))
		return count;

	value = simple_strtol(buffer, NULL, 10);
	develop_mode = value;
	return count;
}

static const struct file_operations develop_mode_fops = {
	.open      = develop_mode_open,
	.read      = seq_read,
	.write     = develop_mode_set,
	.llseek    = seq_lseek,
	.release   = single_release,
};

static int show_package_runtime(struct seq_file *m, void *v)
{
	seq_printf(m, "package runtime is %s\n",
			package_runtime_disable ? "disable " : "enable");
	return 0;
}

static int open_package_runtime(struct inode *inode, struct file *file)
{
	return single_open(file, show_package_runtime, NULL);
}

static ssize_t enable_package_runtime(struct file *filp,
		const char __user *buf, size_t count, loff_t *f_ops)
{
	unsigned char buffer[32] = {0};
	int value = 0;
	if (copy_from_user(buffer, buf, (count > 32 ? 32 : count)))
		return count;

	value = simple_strtol(buffer, NULL, 10);
	package_runtime_disable = value;
	return count;
}

static const struct file_operations package_runtime = {
	.open      = open_package_runtime,
	.read      = seq_read,
	.write     = enable_package_runtime,
	.llseek    = seq_lseek,
	.release   = single_release,
};

static void create_runtime_proc(void)
{
	package_rootdir = proc_mkdir("trace_package", NULL);

	if (!package_rootdir)
		return;

	package_showall_entry = proc_create("show_all", 0664,
			package_rootdir, &package_show_all_fops);

	package_tracestat_entry = proc_create("show_tracestat", 0664,
			package_rootdir, &package_tracestat_fops);

	package_top_bcore_entry = proc_create("top_package_on_bcore", 0664,
			package_rootdir, &package_top_bcore_fops);

	package_top_lcore_entry = proc_create("top_package_on_lcore", 0664,
			package_rootdir, &package_top_lcore_fops);

	package_top_entry = proc_create("top_package", 0664,
			package_rootdir, &package_top_fops);

	package_trace_entry = proc_create("show_traced_package", 0664,
			package_rootdir, &package_trace_fops);

	package_trace_window = proc_create("show_traced_window", 0664,
			package_rootdir, &package_tracedwindow_fops);

	package_window_size_entry = proc_create("show_windowsize", 0664,
			package_rootdir, &package_windowsize_fops);

	package_runtime_entry = proc_create("package_runtime_disable", 0664,
			package_rootdir, &package_runtime);

	package_develop_mode_entry = proc_create("develop_mode", 0664,
			package_rootdir, &develop_mode_fops);
}

static void delete_runtimeproc(void)
{
	if (package_rootdir) {
		if (package_showall_entry)
			proc_remove(package_showall_entry);

		if (package_tracestat_entry)
			proc_remove(package_tracestat_entry);

		if (package_top_bcore_entry)
			proc_remove(package_top_bcore_entry);

		if (package_top_lcore_entry)
			proc_remove(package_top_lcore_entry);

		if (package_top_entry)
			proc_remove(package_top_entry);

		if (package_trace_entry)
			proc_remove(package_trace_entry);

		if (package_trace_window)
			proc_remove(package_trace_window);

		if (package_window_size_entry)
			proc_remove(package_window_size_entry);

		if (package_develop_mode_entry)
			proc_remove(package_develop_mode_entry);

		if (package_runtime_entry)
			proc_remove(package_runtime_entry);
	}

	if (package_rootdir)
		proc_remove(package_rootdir);
}

static void package_runtime_roll_wk(struct work_struct *work)
{
	struct user_struct *user;
	kuid_t uid;
	int bit;

	for_each_set_bit(bit, package_uid_bit, MIN(uid_max_value + 1, TRACE_UID_MAX - 1)) {
		uid.val = bit;
		user = find_user(uid);

		if (user) {
			user->big_cluster_runtime[curr_runtime_items] =
				user->big_cluster_runtime[HISTORY_ITMES];
			user->little_cluster_runtime[curr_runtime_items] =
				user->little_cluster_runtime[HISTORY_ITMES];
			free_uid(user);
		} else {
			user_zero.big_cluster_runtime[curr_runtime_items] =
				user_zero.big_cluster_runtime[HISTORY_ITMES];
			user_zero.little_cluster_runtime[curr_runtime_items] =
				user_zero.little_cluster_runtime[HISTORY_ITMES];
		}

		if (atomic_read(&user->__count) == 1) {
			clear_bit(user->uid.val, package_uid_bit);
			free_uid(user);
		}
	}

	package_runtime_info[curr_runtime_items].update_time = sys_update_time;
	/*memset(top_app_load, 0, sizeof(u64)*TOP_TASKS);*/
	sys_update_time = get_jiffies_64();
	++curr_runtime_items;
	curr_runtime_items %= HISTORY_ITMES;
}

static int __init runtime_init(void)
{
	int i;

	printk(KERN_ERR "in %s\n", __func__);
	create_runtime_proc();
	for (i = 0; i < HISTORY_WINDOWS; i++)
		package_runtime_info[i].update_time = 0;
	init_package_runtime_info(&user_zero);
	INIT_WORK(&package_runtime_roll, package_runtime_roll_wk);
	sys_update_time = get_jiffies_64();
	runtime_enable = 1;

	return 0;
}

static void __exit runtime_exit(void)
{
	printk(KERN_ERR "in %s\n", __func__);
	delete_runtimeproc();
}

static struct user_struct *task_user(struct task_struct *tsk)
{
	if (tsk->cred && tsk->cred->user)
		return tsk->cred->user;

	return &user_zero;
}
#if 0
static int et_top_apps(struct task_struct *tsk, int cmd)
{
	int prev_index = (curr_runtime_items + 3)%HISTORY_ITMES;
	int pprev_index = (curr_runtime_items + 2)%HISTORY_ITMES;
	u64 app_run_btime = tsk->big_cluster_runtime[prev_index]
		- tsk->big_cluster_runtime[pprev_index];
	u64 app_run_ltime = tsk->little_cluster_runtime[prev_index]
		- tsk->little_cluster_runtime[pprev_index];
	u64 app_run_time = app_run_btime + app_run_ltime;
	int top_apps = TOP_TASKS-1;

	if (tsk->big_cluster_runtime[prev_index] < tsk->big_cluster_runtime[pprev_index]
		||  tsk->little_cluster_runtime[prev_index] < tsk->little_cluster_runtime[pprev_index])
		return 0;

	if (app_run_time > top_app_load[TOP_TASKS-1]) {
		if (!cmd)
			return 1;

		top_apps--;
		while (app_run_time > top_app_load[top_apps]) {
			top_app_load[top_apps+1] = top_app_load[top_apps];
			if (--top_apps < 0)
				break;
		}
		top_app_load[top_apps+1] = app_run_time;
	}

	return 0;

}

static void update_top_app(struct task_struct *tsk)
{
	et_top_apps(tsk, 1);
}

int is_top_app(struct task_struct *tsk)
{
	return et_top_apps(tsk, 0);
}
EXPORT_SYMBOL(is_top_app);
#endif

void update_task_runtime_info(struct task_struct *tsk, u64 delta, int cpu)
{
	int run_on_bcore = big_core(cpu);
	struct user_struct *user;
	int curr_items = (curr_runtime_items % HISTORY_ITMES);

	if (!runtime_enable || package_runtime_disable)
		return;

	if (is_idle_task(tsk))
		return;

	user = task_user(tsk);
	if (!user)
		user = &user_zero;	/* No processes for this user,assign it as user_zero */

	if (run_on_bcore) {
		tsk->big_cluster_runtime[HISTORY_ITMES] += delta;
		user->big_cluster_runtime[HISTORY_ITMES] += delta;
	} else {
		tsk->little_cluster_runtime[HISTORY_ITMES] += delta;
		user->little_cluster_runtime[HISTORY_ITMES] += delta;
	}

	/*update_top_app(tsk);*/
	tsk->big_cluster_runtime[curr_items] = tsk->big_cluster_runtime[HISTORY_ITMES];
	tsk->little_cluster_runtime[curr_items] = tsk->little_cluster_runtime[HISTORY_ITMES];
}
EXPORT_SYMBOL(update_task_runtime_info);

void package_runtime_monitor(u64 now)/*should run in timer interrupt*/
{
	if (!runtime_enable || package_runtime_disable)
		return;

	if ((now - sys_update_time) < runtime_window_size * HZ)
		return;

	queue_work_on(0, system_long_wq, &package_runtime_roll);
}
EXPORT_SYMBOL(package_runtime_monitor);

void init_task_runtime_info(struct task_struct *tsk)
{
	int i;

	if (tsk) {
		for (i = 0; i < HISTORY_WINDOWS; i++)
			tsk->little_cluster_runtime[i] = tsk->big_cluster_runtime[i] = 0;
	}
}
EXPORT_SYMBOL(init_task_runtime_info);

void init_package_runtime_info(struct user_struct *user)
{
	int i;

	if (!user)
		return;

	for (i = 0; i < HISTORY_WINDOWS; i++)
		user->little_cluster_runtime[i] = user->big_cluster_runtime[i] = 0;

	if (user->uid.val > TRACE_UID_MAX-1)
		set_bit(TRACE_UID_MAX - 1, package_uid_bit);
	else
		set_bit(user->uid.val, package_uid_bit);

	if (user->uid.val > uid_max_value)
		uid_max_value = user->uid.val;

	atomic_inc(&user->__count);
}
EXPORT_SYMBOL(init_package_runtime_info);

module_init(runtime_init);
module_exit(runtime_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("runtime test driver by David");
