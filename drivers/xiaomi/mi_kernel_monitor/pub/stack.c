

#include <linux/module.h>
#include <linux/stacktrace.h>
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
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/file.h>
#include <linux/pid_namespace.h>
#include <linux/blk-mq.h>
#include <linux/bitmap.h>
#include <linux/cpumask.h>
#include <linux/mm.h>

#include "symbol.h"
#include "mm_tree.h"
#include "internal.h"
#include "pub/stack.h"
#include <asm/stacktrace.h>


extern struct mm_struct *get_task_mm(struct task_struct *task);
extern void mmput(struct mm_struct *);

void perfect_save_stack_trace_user(struct stack_trace *trace);

struct stackframe {
	unsigned long fp;
	unsigned long sp;
	unsigned long pc;
};

struct stack_trace_data {
	struct stack_trace *trace;
	unsigned int skip;
};

static int
copy_stack_frame(const void __user *fp, struct stackframe *frame)
{
	int ret = 0;
	unsigned long data[2];

	if (!access_ok(fp, sizeof(*frame)))
		goto out;

	pagefault_disable();
	ret = __copy_from_user_inatomic(data, fp, 16);
	pagefault_enable();
	if (ret)
		ret = -EFAULT;
	else
		ret = 16;

	if (ret <= 0) {
		ret = 0;
		goto out;
	}

	frame->fp = data[0];
	frame->pc = data[1];

out:
	return ret;
}

/*
 * AArch64 PCS assigns the frame pointer to x29.
 *
 * A simple function prologue looks like this:
 * 	sub	sp, sp, #0x10
 *   	stp	x29, x30, [sp]
 *	mov	x29, sp
 *
 * A simple function epilogue looks like this:
 *	mov	sp, x29
 *	ldp	x29, x30, [sp]
 *	add	sp, sp, #0x10
 */
int unwind_frame(struct stackframe *frame)
{
	unsigned long high, low;
	unsigned long fp = frame->fp;

	low  = frame->sp;
	high = ALIGN(low, 1024 * 1024);

	if (fp < low || fp > high || fp & 0xf)
		return -EINVAL;

	frame->sp = fp + 0x10;
	if (!copy_stack_frame((void *)fp, frame))
		return -EINVAL;

	return 0;
}

static int save_trace(struct stackframe *frame, void *d)
{
	struct stack_trace_data *data = d;
	struct stack_trace *trace = data->trace;
	unsigned long addr = frame->pc;

	if (data->skip) {
		data->skip--;
		return 0;
	}

	trace->entries[trace->nr_entries++] = addr;

	return trace->nr_entries >= trace->max_entries;
}

void walk_stackframe(struct stackframe *frame,
		     int (*fn)(struct stackframe *, void *), void *data)
{
	while (1) {
		int ret;

		if (fn(frame, data))
			break;
		ret = unwind_frame(frame);
		if (ret < 0)
			break;
	}
}

static inline void xiaomi__save_stack_trace_user(int orig, struct task_struct *tsk, struct stack_trace *trace)
{
	const struct pt_regs *regs = task_pt_regs(tsk);
	struct stack_trace_data data;
	struct stackframe frame;

	if (regs == NULL) {
		if (trace->nr_entries < trace->max_entries)
			trace->entries[trace->nr_entries++] = ULONG_MAX;
		return;
	}

	data.trace = trace;
	data.skip = trace->skip;

	frame.fp = regs->user_regs.regs[29];
	frame.sp = regs->user_regs.sp;
	frame.pc = regs->user_regs.pc;

	walk_stackframe(&frame, save_trace, &data);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;
}

void perfect_save_stack_trace_user(struct stack_trace *trace)
{
	xiaomi__save_stack_trace_user(1, current, trace);
}

int xiaomi_stack_trace_cmp(unsigned long *backtrace1, unsigned long *backtrace2)
{
	int i;

	for (i = 0; i < BACKTRACE_DEPTH; i++) {
		if (backtrace1[i] < backtrace2[i])
			return -1;
		else if (backtrace1[i] > backtrace2[i])
			return 1;
		else if ((backtrace1[i] == 0) && (backtrace2[i] == 0))
			return 0;
	}

	return 0;
}

extern unsigned int (*stack_trace_save_skip_hardirq)(struct pt_regs *regs,
						     unsigned long *store,
						     unsigned int size,
						     unsigned int skipnr);

void xm_store_stack_trace(struct pt_regs *regs,
				     struct stack_entry *stack_entry,
				     unsigned long *entries,
				     unsigned int max_entries, int skip)
{
	stack_entry->entries = entries;
	if (regs && stack_trace_save_skip_hardirq)
	{
		stack_entry->nr_entries = stack_trace_save_skip_hardirq(regs,
				entries, max_entries, skip);
	}
	else
	{
		stack_entry->nr_entries = stack_trace_save(entries, max_entries,
							   skip);
	}
}

bool xm_stack_trace_record_for_task(struct task_struct* tsk, struct xm_stack_trace_task *stack_trace, struct pt_regs *regs, int duration)
{
	unsigned int nr_stack_entries;

	nr_stack_entries = stack_trace->nr_tasks;

	if (nr_stack_entries >= MAX_STACE_TRACE_ENTRIES)
		return false;

	////strlcpy(stack_trace->curr_comms[nr_stack_entries], tsk->comm, (unsigned long)TASK_COMM_LEN);
	////strlcpy(stack_trace->parent_comms[nr_stack_entries], tsk->group_leader->comm, (unsigned long)TASK_COMM_LEN);
	strcpy(stack_trace->curr_comms[nr_stack_entries], current->comm);
	stack_trace->curr_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;
	strcpy(stack_trace->parent_comms[nr_stack_entries], current->group_leader->comm);
	stack_trace->parent_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;

	stack_trace->pids[nr_stack_entries] = tsk->pid;
	stack_trace->duration[nr_stack_entries] = duration;
	stack_trace->timestamp[nr_stack_entries] = sched_clock();

	////stack_entry = stack_trace->stack_entries + nr_stack_entries;
	////stack_entry->entries = stack_trace->entries + nr_entries;
	stack_trace->stack_entries[nr_stack_entries].nr_entries = xiaomi_save_stack_trace(tsk, stack_trace->stack_entries[nr_stack_entries].entries);
	if ((stack_trace->stack_entries[nr_stack_entries].nr_entries >= BACKTRACE_DEPTH)) {
		printk("BUG: xiaomi cpu_util MAX_TRACE_ENTRIES too low nr_entries=%u\n",  stack_trace->stack_entries[nr_stack_entries].nr_entries);

		return false;
	}	
	///stack_trace->nr_entries += stack_entry->nr_entries;

	smp_store_release(&stack_trace->nr_tasks, nr_stack_entries + 1);

	return true;
}

int xiaomi_save_stack_trace(struct task_struct *tsk, unsigned long *backtrace)
{
    ////int i = 0;
	int nr_entries = 0;

	nr_entries = orig_stack_trace_save_tsk(tsk, backtrace, BACKTRACE_DEPTH, 0);
	/*for (i = 0; i < nr_entries; i++) 
	{
		printk("mi_kernel sys_xiaomi <%d> -> %pS\n", i, (void *)backtrace[i]);
	}*/

	return nr_entries;
}				 

void xiaomi_save_stack_trace_user(unsigned long *backtrace)
{
	struct stack_trace trace;

	memset(&trace, 0, sizeof(trace));
	memset(backtrace, 0, BACKTRACE_DEPTH * sizeof(unsigned long));
	trace.max_entries = BACKTRACE_DEPTH;
	trace.entries = backtrace;
	perfect_save_stack_trace_user(&trace);
}

void xiaomi_task_kern_stack(struct task_struct *tsk, struct xm_kern_stack_detail *detail)
{
	xiaomi_save_stack_trace(tsk, detail->stack);
}

static void xiaomi_save_stack_trace_user_remote(struct task_struct *tsk, unsigned long *backtrace)
{
	//
}

static int xiaomi_task_raw_stack_remote(struct task_struct *tsk,
	void *to, const void __user *from, unsigned long n)
{
	return 0;
}

void xiaomi_task_user_stack(struct task_struct *tsk, struct xm_user_stack_detail *detail)
{
	struct pt_regs *regs;
	unsigned long sp, ip, bp;
	struct task_struct *leader;

	if (!detail)
		return;

	detail->stack[0] = 0;
	if (!tsk || !tsk->mm)
		return;

	leader = tsk->group_leader;
	if (!leader || !leader->mm || leader->exit_state == EXIT_ZOMBIE){
		return;
	}

	sp = 0;
	ip = 0;
	bp = 0;
	regs = task_pt_regs(tsk);

	if (regs) {
		sp = regs->sp;
		ip = regs->pc;
		bp = regs->sp;
	}

	detail->regs = regs->user_regs;
	detail->sp = sp;
	detail->ip = ip;
	detail->bp = bp;

	if (tsk == current) {
		xiaomi_save_stack_trace_user(detail->stack);
	} else {
		xiaomi_save_stack_trace_user_remote(tsk, detail->stack);
	}
}

void printk_task_user_stack(struct xm_user_stack_detail *user_stack)
{
	int i;

	printk("    用户态堆栈：\n");
	for (i = 0; i < BACKTRACE_DEPTH; i++) {
		if (user_stack->stack[i] == (size_t)-1 || user_stack->stack[i] == 0) {
			break;
		}
		printk("#~        0x%lx\n", user_stack->stack[i]);
	}
}

void xiaomi_task_raw_stack(struct task_struct *tsk, struct xm_raw_stack_detail *detail)
{
	struct pt_regs *regs;
	int i;
	int ret;
	unsigned long sp, ip, bp;
	char *stack;

	memset(detail->stack, 0, XM_USER_STACK_SIZE);
	detail->stack_size = 0;

	if (!tsk || !tsk->mm)
		return;

	regs = task_pt_regs(tsk);
	if (!regs)
		return;

	sp = regs->sp;
	ip = regs->pc;
	bp = regs->sp;

	detail->regs = regs->user_regs;
	detail->sp = sp;
	detail->ip = ip;
	detail->bp = bp;
	stack = (char *)&detail->stack[0];
	for (i = 0; i < (XM_USER_STACK_SIZE / 1024); i++) {
		if (tsk == current) {
			pagefault_disable();
			ret = __copy_from_user_inatomic(stack,
				(void __user *)sp + detail->stack_size, 1024);
			pagefault_enable();
		} else {
			ret = xiaomi_task_raw_stack_remote(tsk, stack,
				(void __user *)sp + detail->stack_size, 1024);
		}
		if (ret)
			break;
		else
			detail->stack_size += 1024;

		stack += 1024;
	}
}

#if 0
static unsigned int hang_kernel_trace(struct task_struct *tsk,
					unsigned long *store, unsigned int size)
{
	struct unwind_state frame;
	unsigned long fp;
	unsigned int store_len = 1;

	if (tsk == current)
		fp = (unsigned long)__builtin_frame_address(0);
	else
		fp = thread_saved_fp(tsk);
	frame.fp = fp;
	frame.pc = thread_saved_pc(tsk);
	if (!frame.pc) {
		pr_info("err stack:%lx\n", thread_saved_sp(tsk));
		return 0;
	}
	*store = frame.pc;
	while(store_len < size) {
		if (!on_task_stack(tsk, fp, 16) || !IS_ALIGNED(fp, 8))
			break;
		frame.fp = READ_ONCE_NOCHECK(*(unsigned long *)(fp));
		frame.pc = READ_ONCE_NOCHECK(*(unsigned long *)(fp + 8));
		fp = frame.fp;
		if (!frame.pc)
			continue;
#ifdef __aarch64__
		frame.pc = ptrauth_strip_insn_pac(frame.pc);
#endif
		*(++store) = frame.pc;
		store_len += 1;
	}
	return store_len;
}
					

void get_kernel_bt(struct task_struct *tsk, int delta, char* name)
{
	unsigned long stacks[32];
	int nr_entries;
	int i;

    ////if (1 == 1) return;

    printk("xiaomi_km tsk->comm=%s, tsk->pid=%d, state=%c, duration=%d ms\n", tsk->comm, tsk->pid, task_state_to_char(tsk), delta);
	nr_entries = hang_kernel_trace(tsk, stacks, ARRAY_SIZE(stacks));
	for (i = 0; i < nr_entries; i++) {
		printk("xiaomi_km: %s <%lx> %pS\n", name, (long)stacks[i], (void *)stacks[i]);
	}
}
#endif

