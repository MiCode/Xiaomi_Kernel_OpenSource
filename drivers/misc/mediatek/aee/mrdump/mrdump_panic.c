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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>
#include <asm/memory.h>
#include <asm/stacktrace.h>
#include <asm/traps.h>
#include <asm/system_misc.h>
#include <linux/kdebug.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <mrdump.h>
#include <linux/reboot.h>
#include "mrdump_panic.h"
#ifdef CONFIG_MTK_WATCHDOG
#include <mtk_wd_api.h>
#endif
#include "mrdump_private.h"
#include "mrdump_mini.h"

int __weak ipanic_atflog_buffer(void *data, unsigned char *buffer,
		size_t sz_buf)
{
	return 0;
}

void __weak sysrq_sched_debug_show_at_AEE(void)
{
	pr_notice("%s weak function at %s\n", __func__, __FILE__);
}

__weak void console_unlock(void)
{
	pr_notice("%s weak function\n", __func__);
}

static inline unsigned long get_linear_memory_size(void)
{
	return (unsigned long)high_memory - PAGE_OFFSET;
}


/* no export symbol to aee_exception_reboot, only used in exception flow */
static void aee_exception_reboot(void)
{
#ifdef CONFIG_MTK_WATCHDOG
	int res;
	struct wd_api *wd_api = NULL;

	/* config reset mode */
	int mode = WD_SW_RESET_BYPASS_PWR_KEY;

	res = get_wd_api(&wd_api);
	if (res < 0) {
		pr_info("arch_reset, get wd api error %d\n", res);
		while (1)
			cpu_relax();
	} else {
		pr_info("exception reboot\n");
		mode += WD_SW_RESET_KEEP_DDR_RESERVE;
		wd_api->wd_sw_reset(mode);
	}
#else
	emergency_restart();
#endif
}

/*save stack as binary into buf,
 *return value

    -1: bottom unaligned
    -2: bottom out of kernel addr space
    -3 top out of kernel addr addr
    -4: buff len not enough
    >0: used length of the buf
 */
int aee_dump_stack_top_binary(char *buf, int buf_len, unsigned long bottom,
		unsigned long top)
{
	/*should check stack address in kernel range */
	if (bottom & 3)
		return -1;
	if (!((bottom >= (PAGE_OFFSET + THREAD_SIZE)) &&
	      (bottom <= (PAGE_OFFSET + get_linear_memory_size())))) {
		return -2;
	}

	if (!((top >= (PAGE_OFFSET + THREAD_SIZE)) &&
	      (top <= (PAGE_OFFSET + get_linear_memory_size())))) {
		return -3;
	}

	if (buf_len < top - bottom)
		return -4;

	memcpy((void *)buf, (void *)bottom, top - bottom);

	return top - bottom;
}

void ipanic_recursive_ke(struct pt_regs *regs, struct pt_regs *excp_regs,
		int cpu)
{
	struct pt_regs saved_regs;

	show_kaslr();
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_exp_type(AEE_EXP_TYPE_NESTED_PANIC);
#endif
	aee_nested_printf("minidump\n");
	bust_spinlocks(1);
	if (excp_regs != NULL) {
		__mrdump_create_oops_dump(AEE_REBOOT_MODE_NESTED_EXCEPTION,
				excp_regs, "Kernel NestedPanic");
	} else if (regs != NULL) {
		aee_nested_printf("previous excp_regs NULL\n");
		__mrdump_create_oops_dump(AEE_REBOOT_MODE_NESTED_EXCEPTION,
				regs, "Kernel NestedPanic");
	} else {
		aee_nested_printf("both NULL\n");
		mrdump_mini_save_regs(&saved_regs);
		__mrdump_create_oops_dump(AEE_REBOOT_MODE_NESTED_EXCEPTION,
				&saved_regs, "Kernel NestedPanic");
	}
	mrdump_mini_ke_cpu_regs(excp_regs);
	mrdump_mini_per_cpu_regs(cpu, regs, current);
	dis_D_inner_flush_all();
	aee_exception_reboot();
}
EXPORT_SYMBOL(ipanic_recursive_ke);

int mrdump_common_die(int fiq_step, int reboot_reason, const char *msg,
		      struct pt_regs *regs)
{
	bust_spinlocks(1);
	aee_disable_api();

	show_kaslr();
	print_modules();
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_fiq_step(fiq_step);

	aee_rr_rec_scp();
#endif
	__mrdump_create_oops_dump(reboot_reason, regs, msg);

	switch (reboot_reason) {
	case AEE_REBOOT_MODE_KERNEL_OOPS:
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_rr_rec_exp_type(AEE_EXP_TYPE_KE);
#endif
		__show_regs(regs);
		dump_stack();
		break;
	case AEE_REBOOT_MODE_KERNEL_PANIC:
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_rr_rec_exp_type(AEE_EXP_TYPE_KE);
#endif
#ifndef CONFIG_DEBUG_BUGVERBOSE
		dump_stack();
#endif
		break;
	case AEE_REBOOT_MODE_HANG_DETECT:
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_rr_rec_exp_type(AEE_EXP_TYPE_HANG_DETECT);
#endif
		break;
	default:
		/* Don't print anything */
		break;
	}

	mrdump_mini_ke_cpu_regs(regs);
	dis_D_inner_flush_all();
	console_unlock();
	aee_exception_reboot();
	return NOTIFY_DONE;
}

int ipanic(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct pt_regs saved_regs;
	int fiq_step = 0;

#ifdef CONFIG_MTK_RAM_CONSOLE
	fiq_step = AEE_FIQ_STEP_KE_IPANIC_START;
#endif
	mrdump_mini_save_regs(&saved_regs);
	return mrdump_common_die(fiq_step,
				 AEE_REBOOT_MODE_KERNEL_PANIC,
				 "Kernel Panic", &saved_regs);
}

static int ipanic_die(struct notifier_block *self, unsigned long cmd, void *ptr)
{
	struct die_args *dargs = (struct die_args *)ptr;
	int fiq_step = 0;

#ifdef CONFIG_MTK_RAM_CONSOLE
	fiq_step = AEE_FIQ_STEP_KE_IPANIC_DIE;
#endif
	return mrdump_common_die(fiq_step,
				 AEE_REBOOT_MODE_KERNEL_OOPS,
				 "Kernel Oops", dargs->regs);
}

static struct notifier_block panic_blk = {
	.notifier_call = ipanic,
};

static struct notifier_block die_blk = {
	.notifier_call = ipanic_die,
};

static int __init mrdump_panic_init(void)
{
	mrdump_hw_init();
	mrdump_cblock_init();
	mrdump_mini_init();

	mrdump_full_init();
	mrdump_wdt_init();

	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	register_die_notifier(&die_blk);
	pr_debug("ipanic: startup\n");
	return 0;
}

arch_initcall(mrdump_panic_init);

static char nested_panic_buf[1024];
int aee_nested_printf(const char *fmt, ...)
{
	va_list args;
	static int total_len;

	va_start(args, fmt);
	total_len += vsnprintf(nested_panic_buf, sizeof(nested_panic_buf),
			fmt, args);
	va_end(args);

#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_sram_fiq_log(nested_panic_buf);
#endif

	return total_len;
}

static void print_error_msg(int len)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	static char error_msg[][50] = { "Bottom unaligned",
		"Bottom out of kernel addr",
		"Top out of kernel addr", "Buf len not enough"
	};
	int tmp = (-len) - 1;

	aee_sram_fiq_log(error_msg[tmp]);
#endif
}

/* extern void mt_fiq_printf(const char *fmt, ...); */
void *aee_excp_regs;
static atomic_t nested_panic_time = ATOMIC_INIT(0);

#ifdef __aarch64__
#define FORMAT_LONG "%016lx "
#else
#define FORMAT_LONG "%08lx "
#endif
inline void aee_print_regs(struct pt_regs *regs)
{
	int i;

	aee_nested_printf("[pt_regs]");
	for (i = 0; i < ELF_NGREG; i++)
		aee_nested_printf(FORMAT_LONG, ((unsigned long *)regs)[i]);
	aee_nested_printf("\n");
}

#define AEE_MAX_EXCP_FRAME	64
inline void aee_print_bt(struct pt_regs *regs)
{
	int i;
	int ret;
	unsigned long high, bottom, fp;
	struct stackframe cur_frame;
	struct pt_regs *excp_regs;

	bottom = regs->reg_sp;
	if (!mrdump_virt_addr_valid(bottom)) {
		aee_nested_printf("invalid sp[%lx]\n", (unsigned long)regs);
		return;
	}
	high = ALIGN(bottom, THREAD_SIZE);
	cur_frame.fp = regs->reg_fp;
	cur_frame.pc = regs->reg_pc;
	cur_frame.sp = regs->reg_sp;
	aee_nested_printf("\n[<%px>] %pS\n", (void *)cur_frame.pc,
			(void *)cur_frame.pc);
	aee_nested_printf("[<%px>] %pS\n", (void *)regs->reg_lr,
			(void *)regs->reg_lr);
	for (i = 0; i < AEE_MAX_EXCP_FRAME; i++) {
		fp = cur_frame.fp;
		if ((fp < bottom) || (fp >= (high + THREAD_SIZE))) {
			if (fp != 0)
				aee_nested_printf("fp(%lx)", fp);
			break;
		}
#ifdef __aarch64__
		ret = unwind_frame(current, &cur_frame);
#else
		ret = unwind_frame(&cur_frame);
#endif
		if (ret < 0 || !mrdump_virt_addr_valid(cur_frame.pc))
			break;
		if (in_exception_text(cur_frame.pc)) {
#ifdef __aarch64__
			/* work around for unknown reason
			 * do_mem_abort stack abnormal
			 */
			excp_regs = (void *)(cur_frame.fp + 0x10 + 0xa0);
			/* skip do_mem_abort & el1_da */
			ret = unwind_frame(current, &cur_frame);
			if (ret < 0)
				break;
#else
			excp_regs = (void *)(cur_frame.fp + 4);
#endif
			cur_frame.pc = excp_regs->reg_pc;
		}
		aee_nested_printf("[<%px>] %pS\n", (void *)cur_frame.pc,
				(void *)cur_frame.pc);

	}
	aee_nested_printf("\n");
}

inline int aee_nested_save_stack(struct pt_regs *regs)
{
	int len = 0;

	if (!mrdump_virt_addr_valid(regs->reg_sp))
		return -1;
	aee_nested_printf("[%lx %lx]\n", (unsigned long)regs->reg_sp,
			(unsigned long)regs->reg_sp + 256);

	len = aee_dump_stack_top_binary(nested_panic_buf,
		sizeof(nested_panic_buf), regs->reg_sp, regs->reg_sp + 256);
	if (len > 0)
#ifdef CONFIG_MTK_RAM_CONSOLE
		aee_sram_fiq_save_bin(nested_panic_buf, len);
#else
		;
#endif
	else
		print_error_msg(len);
	return len;
}

#ifdef CONFIG_MTK_RAM_CONSOLE
int aee_in_nested_panic(void)
{
	return (atomic_read(&nested_panic_time) &&
		((aee_rr_curr_fiq_step() & ~(AEE_FIQ_STEP_KE_NESTED_PANIC - 1))
		 == AEE_FIQ_STEP_KE_NESTED_PANIC));
}
static inline void aee_rec_step_nested_panic(int step)
{
	if (step < 64)
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_KE_NESTED_PANIC + step);
}
#else
int aee_in_nested_panic(void)
{
	return -1;
}
static inline void aee_rec_step_nested_panic(int step)
{
}
#endif

#define TS_MAX_LEN 64
static const char *get_timestamp_string(char *buf, int bufsize)
{
	u64 ts;
	unsigned long rem_nsec;

	ts = local_clock();
	rem_nsec = do_div(ts, 1000000000);
	snprintf(buf, bufsize, "[%5lu.%06lu]",
		       (unsigned long)ts, rem_nsec / 1000);
	return buf;
}

asmlinkage void aee_save_excp_regs(struct pt_regs *regs)
{
	if (!user_mode(regs))
		aee_excp_regs = regs;
}

asmlinkage void aee_stop_nested_panic(struct pt_regs *regs)
{
	struct thread_info *thread = current_thread_info();
	int len = 0;
	int timeout = 1000000;
	int cpu;
#ifdef CONFIG_MTK_WATCHDOG
	int res = 0;
	struct wd_api *wd_api = NULL;
#endif
	struct pt_regs *excp_regs = NULL;
	int prev_fiq_step = aee_rr_curr_fiq_step();
	/* everytime enter nested_panic flow, add 8 */
	static int step_base = -8;
	char tsbuf[TS_MAX_LEN] = {0};

	step_base = step_base < 48 ? step_base + 8 : 56;

	aee_rec_step_nested_panic(step_base);
	local_irq_disable();
	aee_rec_step_nested_panic(step_base + 1);
	cpu = get_HW_cpuid();
	aee_rec_step_nested_panic(step_base + 2);
	/*nested panic may happens more than once on many/single cpus */
	if (atomic_read(&nested_panic_time) < 3)
		aee_nested_printf("\nCPU%dpanic%d@%d-%s\n", cpu,
				nested_panic_time.counter, prev_fiq_step,
				get_timestamp_string(tsbuf, TS_MAX_LEN));
	atomic_inc(&nested_panic_time);

	switch (atomic_read(&nested_panic_time)) {
	case 2:
		aee_print_regs(regs);
		aee_nested_printf("backtrace:");
		aee_print_bt(regs);
		break;

		/* must guarantee Only one cpu can run here */
		/* first check if thread valid */
	case 1:
		if (mrdump_virt_addr_valid(thread)
			&& mrdump_virt_addr_valid(thread->regs_on_excp)) {
			excp_regs = thread->regs_on_excp;
		} else {
			/* if thread invalid, which means wrong sp or
			 * thread_info corrupted,
			 * check global aee_excp_regs instead
			 */
			aee_nested_printf(
				"invalid thread [%lx], excp_regs [%lx]\n",
				(unsigned long)thread,
				(unsigned long)aee_excp_regs);
			excp_regs = aee_excp_regs;
		}
		aee_nested_printf("Nested panic\n");
		if (excp_regs) {
			aee_nested_printf("Previous\n");
			aee_print_regs(excp_regs);
		}
		aee_nested_printf("Current\n");
		aee_print_regs(regs);

		/*should not print stack info.
		 * this may overwhelms ram console used by fiq
		 */
		if (in_fiq_handler() != 0) {
			aee_nested_printf("in fiq handler\n");
		} else {
			/*Dump first panic stack */
			aee_nested_printf("Previous\n");
			if (excp_regs) {
				len = aee_nested_save_stack(excp_regs);
				aee_nested_printf("\nbacktrace:");
				aee_print_bt(excp_regs);
			}

			/*Dump second panic stack */
			aee_nested_printf("Current\n");
			if (mrdump_virt_addr_valid(regs)) {
				len = aee_nested_save_stack(regs);
				aee_nested_printf("\nbacktrace:");
				aee_print_bt(regs);
			}
		}
		aee_rec_step_nested_panic(step_base + 5);
		ipanic_recursive_ke(regs, excp_regs, cpu);

		aee_rec_step_nested_panic(step_base + 6);
		break;
	default:
		break;
	}

	/* we donot want a FIQ after this, so disable hwt */
#ifdef CONFIG_MTK_WATCHDOG
	res = get_wd_api(&wd_api);

	if (res)
		aee_nested_printf("get_wd_api error\n");
	else
		wd_api->wd_aee_confirm_hwreboot();
#else
	aee_nested_printf("mtk watchdog not enable.\n");
#endif
	aee_rec_step_nested_panic(step_base + 7);

	/* waiting for the WDT timeout */
	while (1) {
		/* output to UART directly to avoid printk nested panic */
		/* mt_fiq_printf("%s hang here%d\t", __func__, i++); */
		while (timeout--)
			udelay(1);
		timeout = 1000000;
	}
}
