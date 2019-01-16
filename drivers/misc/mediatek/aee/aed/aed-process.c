#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/ptrace.h>
#include <linux/ratelimit.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/stacktrace.h>
#include <linux/linkage.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <asm/memory.h>
#include <asm/stacktrace.h>
#include <asm/traps.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/mrdump.h>
#include "aed.h"

struct bt_sync {
	atomic_t cpus_report;
	atomic_t cpus_lock;
};

static void per_cpu_get_bt(void *info)
{
	int timeout_max = 500000;
	struct bt_sync *s = (struct bt_sync *)info;

	if (atomic_read(&s->cpus_lock) == 0)
		return;

	atomic_dec(&s->cpus_report);
	while (atomic_read(&s->cpus_lock) == 1) {
		if (timeout_max-- > 0) {
			udelay(1);
		} else {
			break;
		}
	}
	atomic_dec(&s->cpus_report);
}

static int aed_save_trace(struct stackframe *frame, void *d)
{
	struct aee_process_bt *trace = d;
	unsigned long addr = frame->pc;
	unsigned int id = trace->nr_entries;
	struct pt_regs *excp_regs;
	/* use static var, not support concurrency */
	static unsigned long stack;
	int ret = 0;
	if (id >= AEE_NR_FRAME)
		return -1;
	if (id == 0)
		stack = frame->sp;
	if (frame->fp < stack || frame->fp > ALIGN(stack, THREAD_SIZE))
		ret = -1;
#if 0
	if (ret == 0 && in_exception_text(addr)) {
#ifdef __aarch64__
		excp_regs = (void *)(frame->fp + 0x10);
		frame->pc = excp_regs->reg_pc - 4;
#else
		excp_regs = (void *)(frame->fp + 4);
		frame->pc = excp_regs->reg_pc;
		frame->lr = excp_regs->reg_lr;
#endif
		frame->sp = excp_regs->reg_sp;
		frame->fp = excp_regs->reg_fp;
	}
#endif
	trace->entries[id].pc = frame->pc;
	snprintf(trace->entries[id].pc_symbol, AEE_SZ_SYMBOL_S, "%pS", (void *)frame->pc);
#ifndef __aarch64__
	trace->entries[id].lr = frame->lr;
	snprintf(trace->entries[id].lr_symbol, AEE_SZ_SYMBOL_L, "%pS", (void *)frame->lr);
#endif
	++trace->nr_entries;
	return ret;
}

static void aed_get_bt(struct task_struct *tsk, struct aee_process_bt *bt)
{
	struct stackframe frame;
	unsigned long stack_address;
	static struct aee_bt_frame aed_backtrace_buffer[AEE_NR_FRAME];

	bt->nr_entries = 0;
	bt->entries = aed_backtrace_buffer;

	memset(&frame, 0, sizeof(struct stackframe));
	if (tsk != current) {
		frame.fp = thread_saved_fp(tsk);
		frame.sp = thread_saved_sp(tsk);	
#ifdef __aarch64__	
		frame.pc = thread_saved_pc(tsk);
#else
		frame.lr = thread_saved_pc(tsk);
		frame.pc = 0xffffffff;
#endif
	} else {
		register unsigned long current_sp asm("sp");

		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_sp;
#ifdef __aarch64__
		frame.pc = (unsigned long)__builtin_return_address(0);
#else
		frame.lr = (unsigned long)__builtin_return_address(0);
		frame.pc = (unsigned long)aed_get_bt;
#endif
	}
	stack_address = ALIGN(frame.sp, THREAD_SIZE);
	if ((stack_address >= (PAGE_OFFSET + THREAD_SIZE)) && virt_addr_valid(stack_address)) {
		walk_stackframe(&frame, aed_save_trace, bt);
	} else {
		LOGD("%s: Invalid sp value %lx\n", __func__, frame.sp);
	}
}

static DEFINE_SEMAPHORE(process_bt_sem);

int aed_get_process_bt(struct aee_process_bt *bt)
{
	int nr_cpus, err;
	struct bt_sync s;
	struct task_struct *task;
	int timeout_max = 500000;

	if (down_interruptible(&process_bt_sem) < 0) {
		return -ERESTARTSYS;
	}

	err = 0;
	if (bt->pid > 0) {
		task = find_task_by_vpid(bt->pid);
		if (task == NULL) {
			err = -EINVAL;
			goto exit;
		}
	} else {
		err = -EINVAL;
		goto exit;
	}

	err = mutex_lock_killable(&task->signal->cred_guard_mutex);
	if (err)
		goto exit;
	if (!ptrace_may_access(task, PTRACE_MODE_ATTACH)) {
		mutex_unlock(&task->signal->cred_guard_mutex);
		err = -EPERM;
		goto exit;
	}

	mutex_unlock(&task->signal->cred_guard_mutex);

	get_online_cpus();
	preempt_disable();

	nr_cpus = num_online_cpus();
	atomic_set(&s.cpus_report, nr_cpus - 1);
	atomic_set(&s.cpus_lock, 1);

	smp_call_function(per_cpu_get_bt, &s, 0);

	while (atomic_read(&s.cpus_report) != 0) {
		if (timeout_max-- > 0) {
			udelay(1);
		} else {
			break;
		}
	}

	aed_get_bt(task, bt);

	atomic_set(&s.cpus_report, nr_cpus - 1);
	atomic_set(&s.cpus_lock, 0);
	timeout_max = 500000;
	while (atomic_read(&s.cpus_report) != 0) {
		if (timeout_max-- > 0) {
			udelay(1);
		} else {
			break;
		}
	}

	preempt_enable();
	put_online_cpus();

 exit:
	up(&process_bt_sem);
	return err;

}
