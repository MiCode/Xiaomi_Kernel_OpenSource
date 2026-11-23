

#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/timex.h>
#include <linux/tracepoint.h>
//#include <trace/events/irq.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <trace/events/napi.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/random.h>

#include <linux/cgroup.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/sort.h>
#include <linux/net.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <net/sock.h>
#include <linux/connector.h>
#include <uapi/linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/preempt.h>
#include <linux/irqflags.h>
#include <linux/bottom_half.h>

#include <asm/kdebug.h>

#include "internal.h"
#include "mm_tree.h"
#include "pub/kprobe.h"
////#include "pub/fs_utils.h"
#include <linux/proc_fs.h>

static int test_item = 0;

struct mutex test_mutex;
static DECLARE_RWSEM(test_rwsem);

extern void  async_memory_reclaime_for_cold_file_area_exit(void);
extern int async_memory_reclaime_for_cold_file_area_init(void);

extern struct proc_dir_entry *xiaomi_proc_mkdir(const char *name, struct proc_dir_entry *parent);
static int test_item_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", test_item);

	return 0;
}

static void test_mutex_func(void)
{
	mutex_lock(&test_mutex);
	mdelay(5000);
	mutex_unlock(&test_mutex);
}

static void test_rwsem_func(void)
{
	down_write(&test_rwsem);
	mdelay(5000);
	up_write(&test_rwsem);
}

static void test_softirqoff_func(void)
{
	local_bh_disable();
	mdelay(3000);
	////up_write(&test_rwsem);
	local_bh_enable();
}

static void test_hardirqoff_func(void)
{
	local_irq_disable();
	mdelay(300);
	////up_write(&test_rwsem);
	local_irq_enable();
}

static int preemptoff_test_thread(void *arg)
{
	preempt_disable();
	mdelay(210);
	preempt_enable();
	
	return 0;
}

static int preemptoff_test_init(void)
{

	struct task_struct *hd_thread;

	printk("[mi_kernel] Initialize preemptoff_test_init\n");
	hd_thread = kthread_create(preemptoff_test_thread, NULL, "preemptoff_huqh");
	if (hd_thread)
		wake_up_process(hd_thread);

	return 0;
}

static int rt_test_thread1(void *arg)
{
	////msleep(20);
	printk("[mi_kernel] rt_test_thread1 running!\n");
	mdelay(200);
        
	return 0;
}

static int rt_test_init(void)
{
	struct task_struct *hd_thread1;
	/* unsigned long flags; */
	struct sched_param param = {
		.sched_priority = 1
	};

	printk("[mi_kernel] Initialize rt_test_init\n");
	hd_thread1 = kthread_run(rt_test_thread1, NULL, "rt1_huqh");
	sched_setscheduler_nocheck(hd_thread1, SCHED_FIFO, &param);

	return 0;
}

static ssize_t test_item_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
	{
	    return -EINVAL;
	}

	test_item = val;

	switch(test_item)
	{
	case 1:
		test_mutex_func();
		break;
	case 2:
		test_rwsem_func();
		break;
	case 3:
		test_softirqoff_func();
		break;
	case 4:
		test_hardirqoff_func();
		break;
	case 5:
		preemptoff_test_init();
		break;
	case 6:
		rt_test_init();
		break;		
	default:
	    break;
	}

	return count;
}

DEFINE_PROC_ATTRIBUTE_RW(test_item);

int xiaomi_huqh_test_init(void)
{
	struct proc_dir_entry *parent_dir;

	mutex_init(&test_mutex);

	parent_dir = xiaomi_proc_mkdir(PROC_TRACE_TEST_DIR, NULL);

	if (!parent_dir)
		goto free_buf;
	if (!proc_create_data("test_item", 0666, parent_dir, &test_item_fops, NULL))
		goto remove_proc;
	
	return 0;
		
	remove_proc:
		remove_proc_subtree(PROC_TRACE_TEST_DIR, NULL);
	free_buf:
		return -ENOMEM;	
}

void xiaomi_huqh_test_exit(void)
{
	remove_proc_subtree(PROC_TRACE_TEST_DIR, NULL);
}

