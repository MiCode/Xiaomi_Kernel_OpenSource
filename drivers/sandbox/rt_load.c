/* The rt_load is used to get the number of rt processes per CPU
 * The calculation method is to get all rt processes and determine their respective CPUs
 * User process obtains results through ioctl.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/percpu-defs.h>
#include <linux/sched/rt.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/sched/signal.h>
#include <linux/fs.h>

#include "sandbox.h"

/*Define ioctl and  user process needs to be consistent with it*/
#define RTLOAD_IOCTL_MAGIC         'L'
#define RTLOAD_IOCTL_LOAD_GET      _IO(RTLOAD_IOCTL_MAGIC,  1)

#define ERROR_SUCCESS             0
#define RTLOAD_DEBUG              0

#ifndef task_is_running
#define task_is_running(task)	(READ_ONCE((task)->state) == TASK_RUNNING)
#endif

/*CPU RT process load query results*/
struct rtload_stat {
	int topprio;
	int num;
};

/*
*destriptor: find the number of rt processes on each CPU
*/
static int rtload_get(unsigned long  user_args)
{
	struct rtload_stat  *stat = NULL;
	struct task_struct *tsk;
	unsigned int cpu;
	unsigned int cpu_count;
	unsigned int i = 0;
	int ret = ERROR_SUCCESS;
	void __user *argp = (void __user *)user_args;

	cpu_count = num_possible_cpus();
	stat = kzalloc(cpu_count * sizeof(struct rtload_stat),  GFP_ATOMIC);
	if(stat == NULL){
		pr_err("rtmod load stat malloc failure.\n");
		return -EFAULT;
	}

	for_each_process(tsk) {
		i += 1;
		if ((task_is_realtime(tsk) == true) /*&& task_is_running(tsk)*/) {
			cpu = task_cpu(tsk);
			stat[cpu].num += 1;
			stat[cpu].topprio = (((tsk->prio) < stat[cpu].topprio) ? (tsk->prio) : (stat[cpu].topprio));
		}
	}

#if RTLOAD_DEBUG
	printk("Total process num: %d\n", i);
	for (i = 0; i < cpu_count; i++) {
		printk("stat[%d] = [num=%d, topprio=%d]\n", i, stat[i].num, stat[i].topprio);
	}
#endif

	ret = copy_to_user(argp,  stat,  cpu_count * sizeof(struct rtload_stat));
	if (ret < 0){
		pr_err("%s : copy_to_user failed!\n", __func__);
	}
	kfree(stat);

	return (ret > 0 ?  ERROR_SUCCESS : ret) ;
}
static int rtload_open(struct inode *inode,  struct file *file)
{
	__module_get(THIS_MODULE);
	return ERROR_SUCCESS;
}
static long rtload_ioctl(struct file *file,  unsigned int cmd,  unsigned long args)
{
	int ret = ERROR_SUCCESS;

	if (file == NULL){
		pr_err("invalid file!\n");
		return -EFAULT;
	}

	switch(cmd){
		case RTLOAD_IOCTL_LOAD_GET:
			ret = rtload_get(args);
			break;
		default:
			break;
	}

	return ret;
}
static int rtload_release(struct inode *inode,  struct file *file)
{
	module_put(THIS_MODULE);
	return ERROR_SUCCESS;
}

const struct file_operations rtload_fops = {
		.owner                = THIS_MODULE,
		.read                = NULL,
		.write                = NULL,
		.poll                    = NULL,
		.unlocked_ioctl = rtload_ioctl,
		.open                = rtload_open,
		.release        =  rtload_release,
};

static struct miscdevice rtload_miscdev = {
		.minor                = MISC_DYNAMIC_MINOR,
		.name                = "rt_load",
		.nodename        = "rt_load",
		.fops                = &rtload_fops,
};

void rtload_miscdev_init(void)
{
	int ret = ERROR_SUCCESS;

	ret = misc_register(&rtload_miscdev);
	if (ret != ERROR_SUCCESS){
		pr_err("%s : register error.\n", __func__);
	}
	else{
		printk("%s : register complete.\n", __func__);
	}

	return;
}

void rtload_miscdev_exit(void)
{
	misc_deregister(&rtload_miscdev);
	printk("%s : exit complete.\n", __func__);
	return;
}
