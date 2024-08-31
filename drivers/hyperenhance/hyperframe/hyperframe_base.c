// MIUI ADD: Performance_FramePredictBoost
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/preempt.h>
#include <linux/sched/cputime.h>
#include <linux/sched/task.h>
#include <linux/string.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <trace/events/power.h>
#include <linux/tracepoint.h>
#include <linux/kallsyms.h>
#include <uapi/linux/sched/types.h>
#include <trace/hooks/cpufreq.h>

void* hyperframe_alloc_atomic(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE)
		pvBuf = kmalloc(i32Size, GFP_KERNEL);
	else
		pvBuf = vmalloc(i32Size);

	return pvBuf;
}

void hyperframe_free(void *pvBuf, int i32Size)
{
	if (!pvBuf)
		return;

	if (i32Size <= PAGE_SIZE)
		kfree(pvBuf);
	else
		vfree(pvBuf);
}
// END Performance_FramePredictBoost