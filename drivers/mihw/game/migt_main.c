#define pr_fmt(fmt) "migt: " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <linux/security.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/pm_qos.h>
#include <linux/cred.h>
#include <linux/mi_sched.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#include <linux/list.h>
#include <trace/hooks/sched.h>


static unsigned int migt_debug;
module_param(migt_debug, uint, 0644);



static int migt_init(void)
{
	return 0;
}

static void __exit migt_exit()
{
	return;
}

late_initcall(migt_init);
module_exit(migt_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("migt-driver by David");
