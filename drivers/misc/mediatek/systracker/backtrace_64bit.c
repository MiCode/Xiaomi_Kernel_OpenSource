#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/traps.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>
#include <linux/mtk_ram_console.h>

static inline void aee_print_ip_sym(unsigned long ip)
{
	char buf[256];

	snprintf(buf, sizeof(buf), "[<%p>] %pS\n", (void *) ip, (void *) ip);
	aee_sram_fiq_log(buf);
}

static void dump_backtrace_entry(unsigned long where, unsigned long stack)
{
        aee_print_ip_sym(where);
}

void aee_dump_backtrace(struct pt_regs *regs, struct task_struct *tsk)
{
        struct stackframe frame;
        const register unsigned long current_sp asm ("sp");

        pr_debug("%s(regs = %p tsk = %p)\n", __func__, regs, tsk);

        if (!tsk)
                tsk = current;

        if (regs) {
                frame.fp = regs->regs[29];
                frame.sp = regs->sp;
                frame.pc = regs->pc;
        } else if (tsk == current) {
                frame.fp = (unsigned long)__builtin_frame_address(0);
                frame.sp = current_sp;
                frame.pc = (unsigned long)aee_dump_backtrace;
        } else {
                /*
                 * task blocked in __switch_to
                 */
                frame.fp = thread_saved_fp(tsk);
                frame.sp = thread_saved_sp(tsk);
                frame.pc = thread_saved_pc(tsk);
        }

        aee_sram_fiq_log("Call trace:\n");
        while (1) {
                unsigned long where = frame.pc;
                int ret;

                ret = unwind_frame(&frame);
                if (ret < 0)
                        break;
                dump_backtrace_entry(where, frame.sp);
        }
}
