/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hardirq.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#include <linux/sched/rt.h>
#include <linux/sched/task.h>
#include <uapi/linux/sched/types.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <mt-plat/aee.h>
#include <linux/seq_file.h>
#include <linux/jiffies.h>
#include <linux/ptrace.h>
#include <asm/stacktrace.h>
#include <asm/traps.h>
#include "aed.h"
#include "../common/aee-common.h"
#include "../mrdump/mrdump_mini.h"
#include <linux/pid.h>
#ifdef CONFIG_MTK_BOOT
#include <mt-plat/mtk_boot_common.h>
#endif
#ifdef CONFIG_MTK_ION
#include <mtk/ion_drv.h>
#endif
#ifdef CONFIG_MTK_GPU_SUPPORT
#include <mt-plat/mtk_gpu_utility.h>
#endif
#ifdef CONFIG_MTK_RAM_CONSOLE
#include <mt-plat/mtk_ram_console.h>
#endif
#include "../mrdump/mrdump_private.h"
#include <mrdump.h>

#ifndef TASK_STATE_TO_CHAR_STR
#define TASK_STATE_TO_CHAR_STR "RSDTtXZxKWPNn"
#endif

//#define HANG_LOW_MEM
#ifdef HANG_LOW_MEM
#define MAX_HANG_INFO_SIZE (512*1024) /* 512 K info for low mem*/
#else
#define MAX_HANG_INFO_SIZE (2*1024*1024) /* 2M info */
#endif

/* #define BOOT_UP_HANG */

static int MaxHangInfoSize = MAX_HANG_INFO_SIZE;
#define MAX_STRING_SIZE 256
char hang_buff[MAX_HANG_INFO_SIZE];
char *Hang_Info = hang_buff;
static int Hang_Info_Size;
static bool Hang_Detect_first;


#define HD_PROC "hang_detect"
#define	COUNT_SWT_INIT	0
#define	COUNT_SWT_NORMAL	10
#define	COUNT_SWT_FIRST		12
#define	COUNT_ANDROID_REBOOT	11
#define	COUNT_SWT_CREATE_DB	14
#define	COUNT_NE_EXCEPION	20
#define	COUNT_AEE_COREDUMP	40
#define	COUNT_COREDUMP_DONE	19

/* static DEFINE_SPINLOCK(hd_locked_up); */
#define HD_INTER 30

static int hd_detect_enabled;
static int hd_timeout = 0x7fffffff;
static int hang_detect_counter = 0x7fffffff;
static int dump_bt_done;
#ifdef CONFIG_MTK_ENG_BUILD
static int hang_aee_warn = 2;
#else
static int hang_aee_warn;
#endif
static int system_server_pid;
static bool watchdog_thread_exist;
static bool reboot_flag;
DECLARE_WAIT_QUEUE_HEAD(dump_bt_start_wait);
DECLARE_WAIT_QUEUE_HEAD(dump_bt_done_wait);

/* bleow code is added by QHQ  for hang detect */
/* For the condition, where kernel is still alive,
 * but system server is not scheduled.
 */
static void ShowStatus(int flag);
static void reset_hang_info(void);

static long monitor_hang_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg);
#ifdef CONFIG_MTK_ENG_BUILD
static int monit_hang_flag = 1;
#define SEQ_printf(m, x...) \
do {                \
	if (m)          \
		seq_printf(m, x);   \
	else            \
		pr_debug(x);        \
} while (0)



static int monitor_hang_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "[Hang_Detect] show Hang_info size %d\n ",
			(int)strlen(Hang_Info));
	SEQ_printf(m, "%s", Hang_Info);
	return 0;
}

static int monitor_hang_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, monitor_hang_show, inode->i_private);
}


static ssize_t monitor_hang_proc_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[64];
	long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, (unsigned long *)&val);

	if (ret < 0)
		return ret;

	if (val == 1) {
		monit_hang_flag = 1;
		pr_debug("[hang_detect] enable ke.\n");
	} else if (val == 0) {
		monit_hang_flag = 0;
		pr_debug("[hang_detect] disable ke.\n");
	} else if (val == 2) {
		reset_hang_info();
		ShowStatus(0);
	} else if (val == 3) {
		reset_hang_info();
		ShowStatus(1);
	} else if (val == 4) {
		hang_aee_warn = 0;
		pr_info("[hang_detect] disable coredump.\n");
	} else if (val == 5) {
		hang_aee_warn = 2;
		pr_info("[hang_detect] denable coredump.\n");
	} else if (val > 10) {
		show_native_bt_by_pid((int)val);
	}

	return cnt;
}

static const struct file_operations monitor_hang_fops = {
	.open = monitor_hang_proc_open,
	.write = monitor_hang_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif
/******************************************************************************
 * hang detect File operations
 *****************************************************************************/
static int monitor_hang_open(struct inode *inode, struct file *filp)
{
	/* LOGV("%s\n", __func__); */
	/* aee_kernel_RT_Monitor_api (600) ; */
	return 0;
}

static int monitor_hang_release(struct inode *inode, struct file *filp)
{
	/* LOGV("%s\n", __func__); */
	return 0;
}

static unsigned int monitor_hang_poll(struct file *file,
		struct poll_table_struct *ptable)
{
	/* LOGV("%s\n", __func__); */
	return 0;
}

static ssize_t monitor_hang_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	/* LOGV("%s\n", __func__); */
	return 0;
}

static ssize_t monitor_hang_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{

	/* LOGV("%s\n", __func__); */
	return 0;
}



/*
 * aed process daemon and other command line may access me
 * concurrently
 */
static long monitor_hang_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	static long long monitor_status;

	/* QHQ RT Monitor */
	if (cmd == AEEIOCTL_RT_MON_Kick) {
		pr_info("AEEIOCTL_RT_MON_Kick ( %d)\n", (int)arg);
		aee_kernel_RT_Monitor_api((int)arg);
		return ret;
	}
	/* QHQ RT Monitor end */

	if ((cmd == AEEIOCTL_SET_SF_STATE) &&
		(!strncmp(current->comm, "surfaceflinger", 10) ||
		!strncmp(current->comm, "SWWatchDog", 10))) {
		if (copy_from_user(&monitor_status, (void __user *)arg,
					sizeof(long long)))
			ret = -1;
		return ret;
	} else if (cmd == AEEIOCTL_GET_SF_STATE) {
		if (copy_to_user((void __user *)arg, &monitor_status,
					sizeof(long long)))
			ret = -1;
		return ret;
	}

	if ((cmd == AEEIOCTL_SET_HANG_FLAG) &&
		(!strncmp(current->comm, "aee_aed", 7))) {
		const struct cred *cred = current_cred();

		if (!uid_eq(cred->euid, GLOBAL_ROOT_UID))
			return -EACCES;

		if ((int)arg == 1) {
			hang_aee_warn = 2;
			pr_info("hang_detect: aee enable system_server coredump.\n");
		}
		return ret;
	}

	if (cmd == AEEIOCTL_SET_HANG_REBOOT &&
		(!strncmp(current->comm, "init", 4))) {
		reboot_flag = true;
#ifdef CONFIG_MTK_ENG_BUILD
		hang_detect_counter = 3;
#else
		hang_detect_counter = 1;
#endif
		hd_timeout = 3;
		pr_info("hang_detect: %s set reboot command.\n", current->comm);
		return ret;
	}

	return ret;
}




/* QHQ RT Monitor */
static const struct file_operations aed_wdt_RT_Monitor_fops = {
	.owner = THIS_MODULE,
	.open = monitor_hang_open,
	.release = monitor_hang_release,
	.poll = monitor_hang_poll,
	.read = monitor_hang_read,
	.write = monitor_hang_write,
	.unlocked_ioctl = monitor_hang_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = monitor_hang_ioctl,
#endif
};


static struct miscdevice aed_wdt_RT_Monitor_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "RT_Monitor",
	.fops = &aed_wdt_RT_Monitor_fops,
};



/* bleow code is added for monitor_hang_init */
static int monitor_hang_init(void);

static int hang_detect_init(void);
/* bleow code is added for hang detect */



static int __init monitor_hang_init(void)
{
	int err = 0;
#ifdef CONFIG_MTK_ENG_BUILD
	struct proc_dir_entry *pe;
#endif
	/* bleow code is added by QHQ  for hang detect */
	err = misc_register(&aed_wdt_RT_Monitor_dev);
	if (unlikely(err)) {
		pr_notice("aee: failed to register aed_wdt_RT_Monitor_dev device!\n");
		return err;
	}
	hang_detect_init();
	/* bleow code is added by QHQ  for hang detect */
	/* end */
#ifdef CONFIG_MTK_ENG_BUILD
	pe = proc_create("monitor_hang", 0664, NULL, &monitor_hang_fops);
	if (!pe)
		return -ENOMEM;
#endif
	return err;
}

static void __exit monitor_hang_exit(void)
{
	misc_deregister(&aed_wdt_RT_Monitor_dev);
}




static int FindTaskByName(char *name)
{
	struct task_struct *task;
	int ret = -1;

	if (!name)
		return ret;

	read_lock(&tasklist_lock);
	for_each_process(task) {
		if (task && (strncmp(task->comm, name, strlen(name)) == 0)) {
			pr_info("[Hang_Detect] %s found pid:%d.\n",
					task->comm, task->pid);
			ret = task->pid;
			break;
		}
	}
	read_unlock(&tasklist_lock);
	return ret;
}

static void Log2HangInfo(const char *fmt, ...)
{
	unsigned long len = 0;
	va_list ap;

	if ((Hang_Info_Size + MAX_STRING_SIZE) >=
			(unsigned long)MaxHangInfoSize)
		return;

	va_start(ap, fmt);
	len = vscnprintf(&Hang_Info[Hang_Info_Size], MAX_STRING_SIZE, fmt, ap);
	va_end(ap);
	Hang_Info_Size += len;
}

static void Buff2HangInfo(const char *buff, unsigned long size)
{
	if (((unsigned long)Hang_Info_Size + size)
		>= (unsigned long)MaxHangInfoSize)
		return;

	memcpy(&Hang_Info[Hang_Info_Size], buff, size);
	Hang_Info_Size += size;

}

static void DumpMsdc2HangInfo(void)
{
	char *buff_add = NULL;
	unsigned long buff_size = 0;

	if (get_msdc_aee_buffer) {
		get_msdc_aee_buffer((unsigned long *)&buff_add, &buff_size);
		if (buff_size != 0 && buff_add != NULL) {
			if (buff_size > 30 * 1024) {
				buff_add = buff_add + buff_size - 30 * 1024;
				buff_size = 30*1024;
			}
			Buff2HangInfo(buff_add, buff_size);
		}
	}
}

/* copy from arch/armxx/kernel/stacktrace.c file */
/* Linux will skip shed and lock function address */
/* We need this information for hand issue although it have some risk*/
#ifdef __aarch64__		/* 64bit */
struct stack_trace_data {
	struct stack_trace *trace;
	unsigned int no_sched_functions;
	unsigned int skip;
};

static int save_trace(struct stackframe *frame, void *d)
{
	struct stack_trace_data *data = d;
	struct stack_trace *trace = data->trace;
	unsigned long addr = frame->pc;

	if (data->no_sched_functions && in_sched_functions(addr))
		return 0;
	if (data->skip) {
		data->skip--;
		return 0;
	}

	trace->entries[trace->nr_entries++] = addr;

	return trace->nr_entries >= trace->max_entries;
}

static void save_stack_trace_tsk_me(struct task_struct *tsk,
	struct stack_trace *trace)
{
	struct stack_trace_data data;
	struct stackframe frame;

	data.trace = trace;
	data.skip = trace->skip;

	if (tsk != current) {
		data.no_sched_functions = 0; /* modify to 0 */
		frame.fp = thread_saved_fp(tsk);
		/* frame.sp = thread_saved_sp(tsk); */
		frame.pc = thread_saved_pc(tsk);
	} else {
		data.no_sched_functions = 0;
		frame.fp = (unsigned long)__builtin_frame_address(0);
		/* frame.sp = current_stack_pointer; */
		frame.pc = (unsigned long)save_stack_trace_tsk_me;
	}
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	frame.graph = tsk->curr_ret_stack;
#endif

	walk_stackframe(tsk, &frame, save_trace, &data);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}


#else
struct stack_trace_data {
	struct stack_trace *trace;
	unsigned long last_pc;
	unsigned int no_sched_functions;
	unsigned int skip;
};

static int save_trace(struct stackframe *frame, void *d)
{
	struct stack_trace_data *data = d;
	struct stack_trace *trace = data->trace;
	struct pt_regs *regs;
	unsigned long addr = frame->pc;

	if (data->no_sched_functions && in_sched_functions(addr))
		return 0;
	if (data->skip) {
		data->skip--;
		return 0;
	}

	trace->entries[trace->nr_entries++] = addr;

	if (trace->nr_entries >= trace->max_entries)
		return 1;

	/*
	 * in_exception_text() is designed to test if the PC is one of
	 * the functions which has an exception stack above it, but
	 * unfortunately what is in frame->pc is the return LR value,
	 * not the saved PC value.  So, we need to track the previous
	 * frame PC value when doing this.
	 */
	addr = data->last_pc;
	data->last_pc = frame->pc;
	if (!in_exception_text(addr))
		return 0;

	regs = (struct pt_regs *)frame->sp;

	trace->entries[trace->nr_entries++] = regs->ARM_pc;

	return trace->nr_entries >= trace->max_entries;
}

/* This must be noinline to so that our skip calculation works correctly */
static noinline void __save_stack_trace(struct task_struct *tsk,
	struct stack_trace *trace, unsigned int nosched)
{
	struct stack_trace_data data;
	struct stackframe frame;

	data.trace = trace;
	data.last_pc = ULONG_MAX;
	data.skip = trace->skip;
	data.no_sched_functions = nosched;

	if (tsk != current) {
#if 0
#ifdef CONFIG_SMP
		/*
		 * What guarantees do we have here that 'tsk' is not
		 * running on another CPU?  For now, ignore it as we
		 * can't guarantee we won't explode.
		 */
		if (trace->nr_entries < trace->max_entries)
			trace->entries[trace->nr_entries++] = ULONG_MAX;
		return;
#endif
#else
		frame.fp = thread_saved_fp(tsk);
		frame.sp = thread_saved_sp(tsk);
		frame.lr = 0;		/* recovered from the stack */
		frame.pc = thread_saved_pc(tsk);
#endif
	} else {
		/* We don't want this function nor the caller */
		data.skip += 2;
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_stack_pointer;
		frame.lr = (unsigned long)__builtin_return_address(0);
		frame.pc = (unsigned long)__save_stack_trace;
	}

	walk_stackframe(&frame, save_trace, &data);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}

static void save_stack_trace_tsk_me(struct task_struct *tsk,
	struct stack_trace *trace)
{
	__save_stack_trace(tsk, trace, 0); /* modify to 0*/
}

#endif

static void get_kernel_bt(struct task_struct *tsk)
{
	struct stack_trace trace;
	unsigned long stacks[32];
	int i;

	trace.entries = stacks;
	/*save backtraces */
	trace.nr_entries = 0;
	trace.max_entries = 32;
	trace.skip = 0;
	save_stack_trace_tsk_me(tsk, &trace);
	for (i = 0; i < trace.nr_entries; i++)
		Log2HangInfo("<%lx> %pS\n", (long)trace.entries[i],
				(void *)trace.entries[i]);
}

static void DumpMemInfo(void)
{
	char *buff_add = NULL;
	int buff_size = 0;

	if (mlog_get_buffer) {
		mlog_get_buffer(&buff_add, &buff_size);
		if (buff_size <= 0 || buff_add == NULL) {
			pr_info("hang_detect: mlog_get_buffer size %d.\n",
				buff_size);
			return;
		}

		if (buff_size > 3*1024) {
			buff_add = buff_add + buff_size - 3*1024;
			buff_size = 3*1024;
		}

		Buff2HangInfo(buff_add, buff_size);
	}
}

static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000);
}

void show_thread_info(struct task_struct *p, bool dump_bt)
{
	unsigned int state;
	char stat_nam[] = TASK_STATE_TO_CHAR_STR;

	state = p->state ? __ffs(p->state) + 1 : 0;

	Log2HangInfo("%-15.15s %c ", p->comm,
		state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
	Log2HangInfo("%lld.%06ld %d %lu %lu 0x%x 0x%lx %d ",
		nsec_high(p->se.sum_exec_runtime),
		nsec_low(p->se.sum_exec_runtime),
		task_pid_nr(p), p->nvcsw, p->nivcsw, p->flags,
		(unsigned long)task_thread_info(p)->flags,
		p->tgid);
#ifdef CONFIG_SCHED_INFO
	Log2HangInfo("%llu", p->sched_info.last_arrival);
#endif
	Log2HangInfo("\n");

	/* nvscw: voluntary context switch.  */
	/* requires a resource that is unavailable. */
	/* nivcsw: involuntary context switch. */
	/* time slice out or when higher-priority thread to run*/

	if (strcmp(p->comm, "watchdog") == 0)
		watchdog_thread_exist = true;

	if (dump_bt || ((p->state == TASK_RUNNING ||
			p->state & TASK_UNINTERRUPTIBLE) &&
			!strstr(p->comm, "wdtk")))
	/* Catch kernel-space backtrace */
		get_kernel_bt(p);
}

static int DumpThreadNativeMaps_log(pid_t pid, struct task_struct *current_task)
{
	struct vm_area_struct *vma;
	int mapcount = 0;
	struct file *file;
	int flags;
	struct mm_struct *mm;
	struct pt_regs *user_ret;
	char tpath[512];
	char *path_p = NULL;
	struct path base_path;

	if (current_task == NULL)
		return -ESRCH;
	user_ret = task_pt_regs(current_task);

	if (!user_mode(user_ret)) {
		pr_info(" %s,%d:%s: in user_mode",
			__func__, pid, current_task->comm);
		return -1;
	}

	if (current_task->mm == NULL) {
		pr_info(" %s,%d:%s: current_task->mm == NULL",
			__func__, pid, current_task->comm);
		return -1;
	}

	down_read(&current_task->mm->mmap_sem);
	vma = current_task->mm->mmap;
	pr_info("Dump native maps files:\n");
	while (vma && (mapcount < current_task->mm->map_count)) {
		file = vma->vm_file;
		flags = vma->vm_flags;
		if (file) {	/* !!!!!!!!only dump 1st mmaps!!!!!!!!!!!! */
			if (flags & VM_EXEC) {
			/* we only catch code section for reduce maps space */
				base_path = file->f_path;
				path_p = d_path(&base_path, tpath, 512);
				pr_info("%08lx-%08lx %c%c%c%c    %s\n",
				vma->vm_start,
				vma->vm_end, flags & VM_READ ? 'r' : '-',
				flags & VM_WRITE ? 'w' : '-',
				flags & VM_EXEC ? 'x' : '-',
				flags & VM_MAYSHARE ? 's' : 'p', path_p);
			}
		} else {
			const char *name = arch_vma_name(vma);

			mm = vma->vm_mm;
			if (!name) {
				if (mm) {
					if (vma->vm_start <= mm->start_brk &&
					    vma->vm_end >= mm->brk) {
						name = "[heap]";
				} else if (vma->vm_start <= mm->start_stack &&
					vma->vm_end >= mm->start_stack) {
					name = "[stack]";
				}
				} else {
					name = "[vdso]";
				}
			}

			if (flags & VM_EXEC) {
				pr_info("%08lx-%08lx %c%c%c%c %s\n",
				vma->vm_start,
				vma->vm_end, flags & VM_READ ? 'r' : '-',
				flags & VM_WRITE ? 'w' : '-',
				flags & VM_EXEC ? 'x' : '-',
				flags & VM_MAYSHARE ? 's' : 'p', name);
			}
		}
		vma = vma->vm_next;
		mapcount++;
	}
	up_read(&current_task->mm->mmap_sem);

	return 0;
}

static int DumpThreadNativeInfo_By_tid_log(pid_t tid,
	struct task_struct *current_task)
{

	struct pt_regs *user_ret;
	struct vm_area_struct *vma;
	int ret = -1;

	if (current_task == NULL)
		return -ESRCH;
	user_ret = task_pt_regs(current_task);

	if (!user_mode(user_ret)) {
		pr_info(" %s,%d:%s,fail in user_mode",
			__func__, tid, current_task->comm);
		return ret;
	}

	if (current_task->mm == NULL) {
		pr_info(" %s,%d:%s, current_task->mm == NULL",
			__func__, tid, current_task->comm);
		return ret;
	}

#ifndef __aarch64__		/* 32bit */
{
	unsigned int tmpfp, tmp, tmpLR;
	unsigned int native_bt[16];
	unsigned int userstack_start = 0;
	unsigned int userstack_end = 0;
	int copied, frames;

	pr_info(" pc/lr/sp 0x%lx/0x%lx/0x%lx\n",
		(long)(user_ret->ARM_pc), (long)(user_ret->ARM_lr),
	     (long)(user_ret->ARM_sp));
	pr_info("r12-r0 0x%lx/0x%lx/0x%lx/0x%lx\n",
		(long)(user_ret->ARM_ip), (long)(user_ret->ARM_fp),
		(long)(user_ret->ARM_r10), (long)(user_ret->ARM_r9));
	pr_info("0x%lx/0x%lx/0x%lx/0x%lx/0x%lx\n",
		(long)(user_ret->ARM_r8), (long)(user_ret->ARM_r7),
		(long)(user_ret->ARM_r6), (long)(user_ret->ARM_r5),
		(long)(user_ret->ARM_r4));
	pr_info("0x%lx/0x%lx/0x%lx/0x%lx\n",
		(long)(user_ret->ARM_r3), (long)(user_ret->ARM_r2),
		(long)(user_ret->ARM_r1), (long)(user_ret->ARM_r0));

	userstack_start = (unsigned long)user_ret->ARM_sp;

	down_read(&current_task->mm->mmap_sem);
	vma = current_task->mm->mmap;
	while (vma != NULL) {
		if (vma->vm_start <= userstack_start &&
			vma->vm_end >= userstack_start) {
			userstack_end = vma->vm_end;
			break;
		}
		vma = vma->vm_next;
		if (vma == current_task->mm->mmap)
			break;
	}
	up_read(&current_task->mm->mmap_sem);

	if (userstack_end == 0) {
		pr_info(" %s,%d:%s,userstack_end == 0",
			__func__, tid, current_task->comm);
		return ret;
	}

	native_bt[0] = user_ret->ARM_pc;
	native_bt[1] = user_ret->ARM_lr;	/* lr */
	frames = 2;
	tmpfp = user_ret->ARM_fp;
	while ((unsigned long)tmpfp < userstack_end &&
		(unsigned long)tmpfp > userstack_start) {
		copied = access_process_vm(current_task,
			(unsigned long)tmpfp, &tmp,
			sizeof(tmp), 0);
		if (copied != sizeof(tmp)) {
			pr_info("access_process_vm	fp error\n");
			return -EIO;
		}
		if (((unsigned long)tmp >= userstack_start) &&
			((unsigned long)tmp <= userstack_end - 4)) {
			/* CLANG */
			copied = access_process_vm(current_task,
				(unsigned long)tmpfp + 4,
				&tmpLR, sizeof(tmpLR), 0);
			if (copied != sizeof(tmpLR)) {
				pr_info("access_process_vm	pc error\n");
				return -EIO;
			}
			tmpfp = tmp;
			native_bt[frames] = tmpLR - 4;
			frames++;
		} else {
			copied = access_process_vm(current_task,
				(unsigned long)tmpfp - 4,
				&tmpLR, sizeof(tmpLR), 0);
			if (copied != sizeof(tmpLR)) {
				pr_info("access_process_vm	pc error\n");
				return -EIO;
			}
			tmpfp = tmpLR;
			native_bt[frames] = tmp - 4;
			frames++;
		}
		if (frames >= 16)
			break;
	}
	for (copied = 0; copied < frames; copied++)
		pr_info("#%d pc %x\n", copied, native_bt[copied]);

	pr_info("tid(%d:%s), frame %d. tmpfp(0x%x),userstack_start(0x%x),userstack_end(0x%x)\n",
		tid, current_task->comm, frames, tmpfp,
		userstack_start, userstack_end);
}
#else
	/* K64_U32 for current task */
	if (compat_user_mode(user_ret)) {	/* K64_U32 for check reg */
		unsigned int tmpfp, tmp, tmpLR;
		unsigned long native_bt[16];
		unsigned long userstack_start = 0;
		unsigned long userstack_end = 0;
		int copied, frames;

		pr_info("K64+ U32 pc/lr/sp 0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.pc),
			(long)(user_ret->user_regs.regs[14]),
			(long)(user_ret->user_regs.regs[13]));
		pr_info("r12-r0 0x%lx/0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.regs[12]),
			(long)(user_ret->user_regs.regs[11]),
		    (long)(user_ret->user_regs.regs[10]),
		    (long)(user_ret->user_regs.regs[9]));
		pr_info("0x%lx/0x%lx/0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.regs[8]),
			(long)(user_ret->user_regs.regs[7]),
		    (long)(user_ret->user_regs.regs[6]),
		    (long)(user_ret->user_regs.regs[5]),
		    (long)(user_ret->user_regs.regs[4]));
		pr_info("0x%lx/0x%lx/0x%lx/0x%lx\n",
		    (long)(user_ret->user_regs.regs[3]),
		    (long)(user_ret->user_regs.regs[2]),
		    (long)(user_ret->user_regs.regs[1]),
		    (long)(user_ret->user_regs.regs[0]));
		userstack_start = (unsigned long)user_ret->user_regs.regs[13];
		down_read(&current_task->mm->mmap_sem);
		vma = current_task->mm->mmap;
		while (vma != NULL) {
			if (vma->vm_start <= userstack_start &&
				vma->vm_end >= userstack_start) {
				userstack_end = vma->vm_end;
				break;
			}
			vma = vma->vm_next;
			if (vma == current_task->mm->mmap)
				break;
		}
		up_read(&current_task->mm->mmap_sem);

		if (userstack_end == 0) {
			pr_info("Dump native stack failed:\n");
			return ret;
		}

		native_bt[0] = user_ret->user_regs.pc;
		native_bt[1] = user_ret->user_regs.regs[14] - 4;	/* lr */
		tmpfp = user_ret->user_regs.regs[11];
		frames = 2;
		while ((unsigned long)tmpfp < userstack_end &&
			(unsigned long)tmpfp > userstack_start) {
			copied = access_process_vm(current_task,
				(unsigned long)tmpfp, &tmp,
				sizeof(tmp), 0);
			if (copied != sizeof(tmp)) {
				pr_info("access_process_vm	fp error\n");
				return -EIO;
			}
			if (((unsigned long)tmp >= userstack_start) &&
				((unsigned long)tmp <= userstack_end - 4)) {
				/* CLANG */
				copied = access_process_vm(current_task,
					(unsigned long)tmpfp + 4,
					&tmpLR, sizeof(tmpLR), 0);
				if (copied != sizeof(tmpLR)) {
					pr_info("access_process_vm	pc error\n");
					return -EIO;
				}
				tmpfp = tmp;
				native_bt[frames] = tmpLR - 4;
				frames++;
			} else {
				copied = access_process_vm(current_task,
					(unsigned long)tmpfp - 4,
					&tmpLR, sizeof(tmpLR), 0);
				if (copied != sizeof(tmpLR)) {
					pr_info("access_process_vm	pc error\n");
					return -EIO;
				}
				tmpfp = tmpLR;
				native_bt[frames] = tmp - 4;
				frames++;
			}
			if (frames >= 16)
				break;
		}
		for (copied = 0; copied < frames; copied++)
			pr_info("#%d pc %lx\n", copied, native_bt[copied]);

		pr_info("tid(%d:%s), frame %d. tmpfp(0x%x),userstack_start(0x%lx),userstack_end(0x%lx)\n",
			tid, current_task->comm, frames,
			tmpfp, userstack_start, userstack_end);
	} else {		/*K64+U64 */
		unsigned long userstack_start = 0;
		unsigned long userstack_end = 0;
		unsigned long tmpfp, tmp, tmpLR;
		unsigned long native_bt[16];
		int copied, frames;

		pr_info(" K64+ U64 pc/lr/sp 0x%16lx/0x%16lx/0x%16lx\n",
		     (long)(user_ret->user_regs.pc),
		     (long)(user_ret->user_regs.regs[30]),
		     (long)(user_ret->user_regs.sp));

		userstack_start = (unsigned long)user_ret->user_regs.sp;

		down_read(&current_task->mm->mmap_sem);
		vma = current_task->mm->mmap;
		while (vma != NULL) {
			if (vma->vm_start <= userstack_start &&
				vma->vm_end >= userstack_start) {
				userstack_end = vma->vm_end;
				break;
			}
			vma = vma->vm_next;
			if (vma == current_task->mm->mmap)
				break;
		}
		up_read(&current_task->mm->mmap_sem);
		if (userstack_end == 0) {
			pr_info("Dump native stack failed:\n");
			return ret;
		}

		native_bt[0] = user_ret->user_regs.pc;
		native_bt[1] = user_ret->user_regs.regs[30];
		tmpfp = user_ret->user_regs.regs[29];
		frames = 2;
		while (tmpfp < userstack_end && tmpfp > userstack_start) {
			copied = access_process_vm(current_task,
				(unsigned long)tmpfp, &tmp, sizeof(tmp), 0);
			if (copied != sizeof(tmp)) {
				pr_info("access_process_vm  fp error\n");
				return -EIO;
			}
			copied = access_process_vm(current_task,
				(unsigned long)tmpfp + 0x08, &tmpLR,
				sizeof(tmpLR), 0);
			if (copied != sizeof(tmpLR)) {
				pr_info("access_process_vm  pc error\n");
				return -EIO;
			}
			tmpfp = tmp;
			native_bt[frames] = tmpLR;
			frames++;
			if (frames >= 16)
				break;
		}
		for (copied = 0; copied < frames; copied++)
			pr_info("#%d pc %lx\n", copied, native_bt[copied]);

		pr_info("tid(%d:%s),frame %d. tmpfp(0x%lx),userstack_start(0x%lx),userstack_end(0x%lx)\n",
			tid, current_task->comm, frames, tmpfp,
			userstack_start, userstack_end);
	}
#endif

	return 0;
}

void show_native_bt_by_pid(int task_pid)
{
	struct task_struct *t, *p;
	struct pid *pid;
	int count = 0;
	unsigned int state = 0;
	char stat_nam[] = TASK_STATE_TO_CHAR_STR;

	pid = find_get_pid(task_pid);
	t = p = get_pid_task(pid, PIDTYPE_PID);

	if (p != NULL && try_get_task_stack(p)) {
		pr_info("show_bt_by_pid: %d: %s.\n", task_pid, t->comm);

		DumpThreadNativeMaps_log(task_pid, p);
		/* catch maps to Userthread_maps */
		/* change send ptrace_stop to send signal stop */
		do_send_sig_info(SIGSTOP, SEND_SIG_FORCED, p, true);
		do {
			if (t && try_get_task_stack(t)) {
				pid_t tid = 0;

				get_task_struct(t);
				tid = task_pid_vnr(t);
				state = t->state ? __ffs(t->state) + 1 : 0;
				pr_info("%s sysTid=%d, pid=%d\n",
					t->comm, tid, task_pid);
				DumpThreadNativeInfo_By_tid_log(tid, t);
				/* catch user-space bt */
				put_task_stack(t);
				put_task_struct(t);
			}
			if ((++count) % 5 == 4)
				msleep(20);
		} while_each_thread(p, t);
		/* change send ptrace_stop to send signal stop */
		if (stat_nam[state] != 'T')
			do_send_sig_info(SIGCONT, SEND_SIG_FORCED, p, true);
		put_task_stack(p);
		put_task_struct(p);
	} else if (p != NULL)
		put_task_struct(p);
	put_pid(pid);
}
EXPORT_SYMBOL(show_native_bt_by_pid);



static int DumpThreadNativeMaps(pid_t pid, struct task_struct *current_task)
{
	struct vm_area_struct *vma;
	int mapcount = 0;
	struct file *file;
	int flags;
	struct mm_struct *mm;
	struct pt_regs *user_ret;
	char tpath[512];
	char *path_p = NULL;
	struct path base_path;

	if (current_task == NULL)
		return -ESRCH;
	user_ret = task_pt_regs(current_task);

	if (!user_mode(user_ret)) {
		pr_info(" %s,%d:%s: in user_mode", __func__, pid,
				current_task->comm);
		return -1;
	}

	if (current_task->mm == NULL) {
		pr_info(" %s,%d:%s: current_task->mm == NULL", __func__, pid,
				current_task->comm);
		return -1;
	}

	down_read(&current_task->mm->mmap_sem);
	vma = current_task->mm->mmap;
	Log2HangInfo("Dump native maps files:\n");
	while (vma && (mapcount < current_task->mm->map_count)) {
		file = vma->vm_file;
		flags = vma->vm_flags;
		if (file) {	/* !!!!!!!!only dump 1st mmaps!!!!!!!!!!!! */
			if (flags & VM_EXEC) {
				/* we only catch code section for reduce
				 * maps space
				 */
				base_path = file->f_path;
				path_p = d_path(&base_path, tpath, 512);
				Log2HangInfo("%08lx-%08lx %c%c%c%c    %s\n",
					vma->vm_start, vma->vm_end,
					flags & VM_READ ? 'r' : '-',
					flags & VM_WRITE ? 'w' : '-',
					flags & VM_EXEC ? 'x' : '-',
					flags & VM_MAYSHARE ? 's' : 'p',
					path_p);
			}
		} else {
			const char *name = arch_vma_name(vma);

			mm = vma->vm_mm;
			if (!name) {
				if (mm) {
					if (vma->vm_start <= mm->start_brk &&
					    vma->vm_end >= mm->brk) {
						name = "[heap]";
					} else if (vma->vm_start <=
							mm->start_stack &&
						   vma->vm_end >=
							mm->start_stack) {
						name = "[stack]";
					}
				} else {
					name = "[vdso]";
				}
			}

			if (flags & VM_EXEC) {
				Log2HangInfo("%08lx-%08lx %c%c%c%c %s\n",
					vma->vm_start, vma->vm_end,
					flags & VM_READ ? 'r' : '-',
					flags & VM_WRITE ? 'w' : '-',
					flags & VM_EXEC ? 'x' : '-',
					flags & VM_MAYSHARE ? 's' : 'p', name);
			}
		}
		vma = vma->vm_next;
		mapcount++;
	}
	up_read(&current_task->mm->mmap_sem);

	return 0;
}



static int DumpThreadNativeInfo_By_tid(pid_t tid,
	struct task_struct *current_task)
{
	struct pt_regs *user_ret;
	struct vm_area_struct *vma;
	unsigned long userstack_start = 0;
	unsigned long userstack_end = 0, length = 0;
	int ret = -1;

	if (current_task == NULL)
		return -ESRCH;
	user_ret = task_pt_regs(current_task);

	if (!user_mode(user_ret)) {
		pr_info(" %s,%d:%s,fail in user_mode", __func__, tid,
				current_task->comm);
		return ret;
	}

	if (current_task->mm == NULL) {
		pr_info(" %s,%d:%s, current_task->mm == NULL", __func__, tid,
				current_task->comm);
		return ret;
	}
#ifndef __aarch64__		/* 32bit */
	Log2HangInfo(" pc/lr/sp 0x%08lx/0x%08lx/0x%08lx\n", user_ret->ARM_pc,
			user_ret->ARM_lr, user_ret->ARM_sp);
	Log2HangInfo("r12-r0 0x%lx/0x%x/0x%lx/0x%lx\n",
		(long)(user_ret->ARM_ip), (long)(user_ret->ARM_fp),
		(long)(user_ret->ARM_r10), (long)(user_ret->ARM_r9));
	Log2HangInfo("0x%lx/0x%lx/0x%lx/0x%lx/0x%lx\n",
		(long)(user_ret->ARM_r8), (long)(user_ret->ARM_r7),
		(long)(user_ret->ARM_r6), (long)(user_ret->ARM_r5),
		(long)(user_ret->ARM_r4));
	Log2HangInfo("0x%lx/0x%lx/0x%lx/0x%lx\n",
		(long)(user_ret->ARM_r3), (long)(user_ret->ARM_r2),
		(long)(user_ret->ARM_r1), (long)(user_ret->ARM_r0));

	userstack_start = (unsigned long)user_ret->ARM_sp;

	down_read(&current_task->mm->mmap_sem);
	vma = current_task->mm->mmap;
	while (vma != NULL) {
		if (vma->vm_start <= userstack_start &&
			vma->vm_end >= userstack_start) {
			userstack_end = vma->vm_end;
			break;
		}
		vma = vma->vm_next;
		if (vma == current_task->mm->mmap)
			break;
	}
	up_read(&current_task->mm->mmap_sem);

	if (userstack_end == 0) {
		pr_info(" %s,%d:%s,userstack_end == 0", __func__,
				tid, current_task->comm);
		return ret;
	}
	length = userstack_end - userstack_start;


	/* dump native stack to buffer */
	{
		unsigned long SPStart = 0, SPEnd = 0;
		int tempSpContent[4], copied;

		SPStart = userstack_start;
		SPEnd = SPStart + length;
		Log2HangInfo("UserSP_start:%x,Length:%x,End:%x\n",
				SPStart, length, SPEnd);
		while (SPStart < SPEnd) {
			copied =
			    access_process_vm(current_task, SPStart,
					&tempSpContent, sizeof(tempSpContent),
					0);
			if (copied != sizeof(tempSpContent)) {
				pr_info(
				  "access_process_vm  SPStart error,sizeof(tempSpContent)=%x\n"
				  , (unsigned int)sizeof(tempSpContent));
				/* return -EIO; */
			}
			if (tempSpContent[0] != 0 ||
				tempSpContent[1] != 0 ||
				tempSpContent[2] != 0 ||
				tempSpContent[3] != 0) {
				Log2HangInfo("%08x:%x %x %x %x\n", SPStart,
						tempSpContent[0],
						tempSpContent[1],
						tempSpContent[2],
						tempSpContent[3]);
			}
			SPStart += 4 * 4;
		}
	}
#else	/* 64bit, First deal with K64+U64, the last time to deal with K64+U32 */
	/* K64_U32 for current task */
	if (compat_user_mode(user_ret)) {	/* K64_U32 for check reg */
		Log2HangInfo("K64+ U32 pc/lr/sp 0x%16lx/0x%16lx/0x%16lx\n",
			(long)(user_ret->user_regs.pc),
			(long)(user_ret->user_regs.regs[14]),
			(long)(user_ret->user_regs.regs[13]));
		Log2HangInfo("r12-r0 0x%lx/0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.regs[12]),
			(long)(user_ret->user_regs.regs[11]),
			(long)(user_ret->user_regs.regs[10]),
			(long)(user_ret->user_regs.regs[9]));
		Log2HangInfo("0x%lx/0x%lx/0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.regs[8]),
			(long)(user_ret->user_regs.regs[7]),
			(long)(user_ret->user_regs.regs[6]),
			(long)(user_ret->user_regs.regs[5]),
			(long)(user_ret->user_regs.regs[4]));
		Log2HangInfo("0x%lx/0x%lx/0x%lx/0x%lx\n",
			(long)(user_ret->user_regs.regs[3]),
			(long)(user_ret->user_regs.regs[2]),
			(long)(user_ret->user_regs.regs[1]),
			(long)(user_ret->user_regs.regs[0]));
		userstack_start = (unsigned long)user_ret->user_regs.regs[13];
		down_read(&current_task->mm->mmap_sem);
		vma = current_task->mm->mmap;
		while (vma != NULL) {
			if (vma->vm_start <= userstack_start &&
				vma->vm_end >= userstack_start) {
				userstack_end = vma->vm_end;
				break;
			}
			vma = vma->vm_next;
			if (vma == current_task->mm->mmap)
				break;
		}
		up_read(&current_task->mm->mmap_sem);

		if (userstack_end == 0) {
			pr_info("Dump native stack failed:\n");
			return ret;
		}

		length = userstack_end - userstack_start;

		/*  dump native stack to buffer */
		{
			unsigned long SPStart = 0, SPEnd = 0;
			int tempSpContent[4], copied;

			SPStart = userstack_start;
			SPEnd = SPStart + length;
			Log2HangInfo("UserSP_start:%x,Length:%x,End:%x\n",
				SPStart, length, SPEnd);
			while (SPStart < SPEnd) {
				copied = access_process_vm(current_task,
						SPStart, &tempSpContent,
						sizeof(tempSpContent), 0);
				if (copied != sizeof(tempSpContent)) {
					pr_info(
					  "access_process_vm  SPStart error,sizeof(tempSpContent)=%x\n",
					  (unsigned int)sizeof(tempSpContent));
					/* return -EIO; */
				}
				if (tempSpContent[0] != 0 ||
					tempSpContent[1] != 0 ||
					tempSpContent[2] != 0 ||
					tempSpContent[3] != 0) {
					Log2HangInfo("%08x:%x %x %x %x\n",
							SPStart,
							tempSpContent[0],
							tempSpContent[1],
							tempSpContent[2],
							tempSpContent[3]);
				}
				SPStart += 4 * 4;
			}
		}
	} else {		/*K64+U64 */
		userstack_start = (unsigned long)user_ret->user_regs.sp;

		down_read(&current_task->mm->mmap_sem);
		vma = current_task->mm->mmap;
		while (vma != NULL) {
			if (vma->vm_start <= userstack_start &&
					vma->vm_end >= userstack_start) {
				userstack_end = vma->vm_end;
				break;
			}
			vma = vma->vm_next;
			if (vma == current_task->mm->mmap)
				break;
		}
		up_read(&current_task->mm->mmap_sem);
		if (userstack_end == 0) {
			pr_info("Dump native stack failed:\n");
			return ret;
		}

		{
			unsigned long tmpfp, tmp, tmpLR;
			int copied, frames;
			unsigned long native_bt[16];

			native_bt[0] = user_ret->user_regs.pc;
			native_bt[1] = user_ret->user_regs.regs[30];
			tmpfp = user_ret->user_regs.regs[29];
			frames = 2;
			while (tmpfp < userstack_end &&
					tmpfp > userstack_start) {
				copied =
				    access_process_vm(current_task,
						    (unsigned long)tmpfp, &tmp,
						      sizeof(tmp), 0);
				if (copied != sizeof(tmp)) {
					pr_info("access_process_vm  fp error\n");
					return -EIO;
				}
				copied =
				    access_process_vm(current_task,
						    (unsigned long)tmpfp + 0x08,
						      &tmpLR, sizeof(tmpLR), 0);
				if (copied != sizeof(tmpLR)) {
					pr_info("access_process_vm  pc error\n");
					return -EIO;
				}
				tmpfp = tmp;
				native_bt[frames] = tmpLR;
				frames++;
				if (frames >= 16)
					break;
			}
			for (copied = 0; copied < frames; copied++) {
				/*  #00 pc 000000000006c760
				 *  /system/lib64/ libc.so (__epoll_pwait+8)
				 */
				Log2HangInfo("#%d pc %lx\n", copied,
						native_bt[copied]);
			}
		}
	}
#endif

	return 0;
}

static void show_bt_by_pid(int task_pid)
{
	struct task_struct *t, *p;
	struct pid *pid;
#ifdef __aarch64__
	struct pt_regs *user_ret;
#endif
	int count = 0, dump_native = 0;
	unsigned int state = 0;

	pid = find_get_pid(task_pid);
	t = p = get_pid_task(pid, PIDTYPE_PID);

	if (p != NULL && try_get_task_stack(p)) {
		Log2HangInfo("%s: %d: %s.\n", __func__, task_pid, t->comm);
#ifndef __aarch64__	 /* 32bit */
		if (strcmp(t->comm, "system_server") == 0)
			dump_native = 1;
		else
			dump_native = 0;
#else
		user_ret = task_pt_regs(t);

		if (!user_mode(user_ret)) {
			pr_info(" %s,%d:%s,fail in user_mode", __func__,
					task_pid, t->comm);
			dump_native = 0;
		} else	if (t->mm == NULL) {
			pr_info(" %s,%d:%s, current_task->mm == NULL", __func__,
					task_pid, t->comm);
			dump_native = 0;
		} else if (compat_user_mode(user_ret)) {
			/* K64_U32 for check reg */
			if (strcmp(t->comm, "system_server") == 0)
				dump_native = 1;
			else
				dump_native = 0;
		} else
			dump_native = 1;
#endif
		if (dump_native == 1)
			/* catch maps to Userthread_maps */
			DumpThreadNativeMaps(task_pid, p);
		do {
			if (t && try_get_task_stack(t)) {
				pid_t tid = 0;

				get_task_struct(t);
				tid = task_pid_vnr(t);
				state = t->state ? __ffs(t->state) + 1 : 0;
				/* catch kernel bt */
				show_thread_info(t, true);

				Log2HangInfo("%s sysTid=%d, pid=%d\n", t->comm,
						tid, task_pid);

				if (dump_native == 1)
					DumpThreadNativeInfo_By_tid(tid, t);
				put_task_stack(t);
				put_task_struct(t);
			}
			if ((++count) % 5 == 4)
				msleep(20);
			Log2HangInfo("-\n");
		} while_each_thread(p, t);
		put_task_stack(p);
		put_task_struct(p);
	} else if (p != NULL) {
		put_task_struct(p);
		Log2HangInfo("%s pid %d state %d, flags %d. stack is null.\n",
			t->comm, task_pid, t->state, t->flags);
	}
	put_pid(pid);
}

static void hang_dump_backtrace(void)
{
	struct task_struct *p, *t, *system_server_task = NULL;
	struct task_struct *monkey_task = NULL;

	watchdog_thread_exist = false;
	Log2HangInfo("dump backtrace start: %llu\n", local_clock());

	read_lock(&tasklist_lock);
	for_each_process(p) {
		get_task_struct(p);
		if (Hang_Detect_first == false) {
			if (strcmp(p->comm, "system_server") == 0)
				system_server_task = p;
			if (strstr(p->comm, "monkey") != NULL)
				monkey_task = p;
		}
		/* specify process, need dump maps file and native backtrace */
		if ((strcmp(p->comm, "surfaceflinger") == 0) ||
			(strcmp(p->comm, "init") == 0) ||
			(strcmp(p->comm, "system_server") == 0) ||
			(strcmp(p->comm, "mmcqd/0") == 0) ||
			(strcmp(p->comm, "debuggerd64") == 0) ||
			(strcmp(p->comm, "mmcqd/1") == 0) ||
			(strcmp(p->comm, "vdc") == 0) ||
			(strcmp(p->comm, "vold") == 0) ||
			(strcmp(p->comm, "debuggerd") == 0)) {
			read_unlock(&tasklist_lock);
			show_bt_by_pid(p->pid);
			read_lock(&tasklist_lock);
			put_task_struct(p);
			continue;
		}
		for_each_thread(p, t) {
			if (try_get_task_stack(t)) {
				get_task_struct(t);
				show_thread_info(t, false);
				put_task_stack(t);
				put_task_struct(t);
			}
		}
		put_task_struct(p);
	}
	read_unlock(&tasklist_lock);
	Log2HangInfo("dump backtrace end.\n");

	if (Hang_Detect_first == false) {
		if (system_server_task != NULL)
			do_send_sig_info(SIGQUIT, SEND_SIG_FORCED,
				system_server_task, true);
		if (monkey_task != NULL)
			do_send_sig_info(SIGQUIT, SEND_SIG_FORCED,
				monkey_task, true);
	}
}

static void ShowStatus(int flag)
{
	if (Hang_Detect_first == true)	{ /* the last dump */
		DumpMemInfo();
		DumpMsdc2HangInfo();
	}

	hang_dump_backtrace();

	if (Hang_Detect_first == true)	{ /* the last dump */
		/* debug_locks = 1; */
		debug_show_all_locks();
		show_free_areas(0, NULL);
		if (show_task_mem)
			show_task_mem();
#ifdef CONFIG_MTK_ION
		ion_mm_heap_memory_detail();
#endif
#ifdef CONFIG_MTK_GPU_SUPPORT
		mtk_dump_gpu_memory_usage();
#endif
#ifdef CONFIG_MTK_WQ_DEBUG
		wq_debug_dump();
#endif

	}
}

static void reset_hang_info(void)
{
	Hang_Detect_first = false;
	memset(Hang_Info, 0, MaxHangInfoSize);
	Hang_Info_Size = 0;
}

static int hang_detect_warn_thread(void *arg)
{

	/* unsigned long flags; */
	struct sched_param param = {
		.sched_priority = 99
	};

	char string_tmp[30];

	sched_setscheduler(current, SCHED_FIFO, &param);
	snprintf(string_tmp, 30, "hang_detect:[pid:%d]\n", system_server_pid);
	pr_notice("hang_detect create warning api: %s.", string_tmp);
#ifdef __aarch64__
		aee_kernel_warning_api(__FILE__, __LINE__,
		DB_OPT_PROCESS_COREDUMP | DB_OPT_AARCH64 | DB_OPT_FTRACE,
		"maybe have other hang_detect KE DB, please send together!!\n",
		string_tmp);
#else
		aee_kernel_warning_api(__FILE__, __LINE__,
		DB_OPT_PROCESS_COREDUMP | DB_OPT_FTRACE,
		"maybe have other hang_detect KE DB, please send together!!\n",
		string_tmp);
#endif
	return 0;
}

static int dump_last_thread(void *arg)
{
	/* unsigned long flags; */
	struct sched_param param = {
		.sched_priority = 99
	};
	sched_setscheduler(current, SCHED_FIFO, &param);
	pr_info("[Hang_Detect] dump last thread.\n");
	ShowStatus(1);
	dump_bt_done = 1;
	wake_up_interruptible(&dump_bt_done_wait);
	return 0;
}

static int hang_detect_dump_thread(void *arg)
{
	struct task_struct *hd_thread;


	/* unsigned long flags; */
	struct sched_param param = {
		.sched_priority = 99
	};

	sched_setscheduler(current, SCHED_FIFO, &param);
	msleep(120 * 1000);
	dump_bt_done = 1;
	while (1) {
		wait_event_interruptible(dump_bt_start_wait, dump_bt_done == 0);
		if (hang_aee_warn == 1) {
			hd_thread = kthread_create(hang_detect_warn_thread,
				NULL, "hang_detect2");
			if (hd_thread != NULL)
				wake_up_process(hd_thread);
		} else
			ShowStatus(0);

		dump_bt_done = 1;
		wake_up_interruptible(&dump_bt_done_wait);
	}
	pr_notice("[Hang_Detect] hang_detect dump thread exit.\n");
	return 0;
}

void wake_up_dump(void)
{
	dump_bt_done = 0;
	wake_up_interruptible(&dump_bt_start_wait);
	if (dump_bt_done != 1)
		wait_event_interruptible_timeout(dump_bt_done_wait,
			dump_bt_done == 1, HZ*10);
}

static int hang_detect_thread(void *arg)
{

	/* unsigned long flags; */
	struct sched_param param = {
		.sched_priority = 99
	};
	struct task_struct *hd_thread;

	sched_setscheduler(current, SCHED_FIFO, &param);
	reset_hang_info();
	msleep(120 * 1000);
	pr_debug("[Hang_Detect] hang_detect thread starts.\n");

#ifdef BOOT_UP_HANG
	hd_timeout = 9;
	hang_detect_counter = 9;
	hd_detect_enabled = 1;
#endif

	while (1) {
		pr_info(
			"[Hang_Detect] hang_detect thread counts down %d:%d, status %d.\n",
			hang_detect_counter, hd_timeout, hd_detect_enabled);
#ifdef BOOT_UP_HANG
		if (reboot_flag || (hd_detect_enabled == 1))
#else
		system_server_pid = FindTaskByName("system_server");
		if (reboot_flag || ((hd_detect_enabled == 1) &&
			(system_server_pid != -1)))
#endif
		{
#ifdef CONFIG_MTK_RAM_CONSOLE
			aee_rr_rec_hang_detect_timeout_count(hd_timeout);
#endif

			if (hang_detect_counter == 1 && hang_aee_warn == 2
				&& hd_timeout != 11 && reboot_flag == false) {
				hang_detect_counter = hd_timeout / 2;
				hang_aee_warn = 1;
				wake_up_dump();
				hang_aee_warn = 0;
			}

			if (hang_detect_counter <= 0) {
				Log2HangInfo(
					"[Hang_detect]Dump the %d time process bt.\n",
					Hang_Detect_first ? 2 : 1);
				if (Hang_Detect_first == true
					&& dump_bt_done != 1) {
		/* some time dump thread will block in dumping native bt */
		/* so create new thread to dump enough kernel bt */
					hd_thread = kthread_create(
						dump_last_thread,
						NULL, "hang_detect2");
					if (hd_thread != NULL)
						wake_up_process(hd_thread);
				if (dump_bt_done != 1)
					wait_event_interruptible_timeout(
							dump_bt_done_wait,
							dump_bt_done == 1,
							HZ*10);
				} else
					wake_up_dump();

				if (Hang_Detect_first == true) {
					pr_notice(
						"[Hang_Detect] aee mode is %d, we should triger KE...\n",
						aee_mode);
#ifdef CONFIG_MTK_RAM_CONSOLE
	if (watchdog_thread_exist == false && reboot_flag == false)
		aee_rr_rec_hang_detect_timeout_count(COUNT_ANDROID_REBOOT);
#endif
#ifdef CONFIG_MTK_ENG_BUILD
					if (monit_hang_flag == 1) {
					/* eng load can detect whether KE*/
#endif
						/* BUG(); */
						show_kaslr();
						mrdump_mini_add_hang_raw(
						(unsigned long)Hang_Info,
							MaxHangInfoSize);
						mrdump_mini_add_extra_misc();
#ifdef CONFIG_MTK_RAM_CONSOLE
						mrdump_common_die(
						AEE_FIQ_STEP_HANG_DETECT,
						AEE_REBOOT_MODE_HANG_DETECT,
						"Hang Detect", NULL);
#else
					mrdump_common_die(0,
						AEE_REBOOT_MODE_HANG_DETECT,
						"Hang Detect", NULL);
#endif
#ifdef CONFIG_MTK_ENG_BUILD
					}
#endif
				} else
					Hang_Detect_first = true;
			}
			hang_detect_counter--;
		} else {
			/* incase of system_server restart, we give
			 * 2 mins more.(4*HD_INTER)
			 */
			if (hd_detect_enabled == 1) {
				hang_detect_counter = hd_timeout + 4;
				hd_detect_enabled = 0;
			}
			reset_hang_info();
		}

		msleep((HD_INTER) * 1000);
	}
	return 0;
}

void hd_test(void)
{
	hang_detect_counter = 0;
	hd_timeout = 0;
}


void aee_kernel_RT_Monitor_api(int lParam)
{
	reset_hang_info();
	if (reboot_flag) {
		pr_info("[Hang_Detect] in reboot flow.\n");
		return;
	}

	if (lParam == 0) {
		hd_detect_enabled = 0;
		hang_detect_counter = hd_timeout;
		pr_info("[Hang_Detect] hang_detect disabled\n");
	} else if (lParam > 0) {
		/* lParem=0x1000|timeout,only set in aee call when NE in
		 *  system_server so only change hang_detect_counter when
		 *  call from AEE
		 * Others ioctl, will change
		 * hd_detect_enabled & hang_detect_counter
		 */
		if (lParam & 0x1000) {
			hang_detect_counter = hd_timeout =
			  ((long)(lParam & 0x0fff) + HD_INTER - 1) / (HD_INTER);
		} else {
			hd_detect_enabled = 1;
			hang_detect_counter = hd_timeout =
			    ((long)lParam + HD_INTER - 1) / (HD_INTER);
		}
		if (hd_timeout < 10) {
			/* hang detect min timeout is 10 (5min) */
			hang_detect_counter = 10;
			hd_timeout = 10;
		}
		pr_info("[Hang_Detect] hang_detect enabled %d\n", hd_timeout);
	}
}

int hang_detect_init(void)
{

	struct task_struct *hd_thread;

	pr_debug("[Hang_Detect] Initialize proc\n");
	hd_thread = kthread_create(hang_detect_thread, NULL, "hang_detect");
	if (hd_thread != NULL)
		wake_up_process(hd_thread);

	hd_thread = kthread_create(hang_detect_dump_thread, NULL,
			"hang_detect1");
	if (hd_thread != NULL)
		wake_up_process(hd_thread);

	return 0;
}

void get_hang_detect_buffer(unsigned long *addr, unsigned long *size,
			    unsigned long *start)
{
	*addr = (unsigned long)Hang_Info;
	*start = 0;
	*size = MaxHangInfoSize;
}


module_init(monitor_hang_init);
module_exit(monitor_hang_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek AED Driver");
MODULE_AUTHOR("MediaTek Inc.");
