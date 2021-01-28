// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/utsname.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/smp.h>
#ifdef CONFIG_MTK_SCHED_MONITOR
#include "mtk_sched_mon.h"
#endif
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/hardirq.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <mrdump.h>
#include <linux/uaccess.h>
#include <linux/sched/clock.h>
#include <linux/stacktrace.h>
#include <asm/stacktrace.h>
#include <asm/memory.h>
#include <asm/traps.h>
#include <linux/compiler.h>
#include <linux/reboot.h>
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */
#ifdef CONFIG_MTK_WATCHDOG
#include <mtk_wd_api.h>
#include <ext_wd_drv.h>
#endif
#ifdef CONFIG_MTK_EIC_HISTORY_DUMP
#include <linux/irqchip/mtk-eic.h>
#endif
#include <mrdump_private.h>
#include <mt-plat/mboot_params.h>

#define THREAD_INFO(sp) ((struct thread_info *) \
				((unsigned long)(sp) & ~(THREAD_SIZE - 1)))

#define WDT_PERCPU_LOG_SIZE	2048
#define WDT_LOG_DEFAULT_SIZE	4096
#define WDT_SAVE_STACK_SIZE	256
#define MAX_EXCEPTION_FRAME	16
#define LOG_BUFFER_SIZE	512

/* AEE_MTK_CPU_NUMS may not eaqual to real cpu numbers,
 * alloc buffer at initialization
 */
static char *wdt_percpu_log_buf[AEE_MTK_CPU_NUMS];
static int wdt_percpu_log_length[AEE_MTK_CPU_NUMS];
static char wdt_log_buf[WDT_LOG_DEFAULT_SIZE];
static int wdt_percpu_preempt_cnt[AEE_MTK_CPU_NUMS];
static unsigned long
wdt_percpu_stackframe[AEE_MTK_CPU_NUMS][MAX_EXCEPTION_FRAME];
static int wdt_log_length;
static atomic_t wdt_enter_fiq;
static char str_buf[AEE_MTK_CPU_NUMS][LOG_BUFFER_SIZE];

#define ATF_AEE_DEBUG_BUF_LENGTH	0x4000
static void *atf_aee_debug_virt_addr;

static atomic_t aee_wdt_zap_lock;
int no_zap_locks;

struct atf_aee_regs {
	__u64 regs[31];
	__u64 sp;
	__u64 pc;
	__u64 pstate;
};

struct stacks_buffer {
	char bin_buf[WDT_SAVE_STACK_SIZE];
	int real_len;
	unsigned long top;
	unsigned long bottom;
};
static struct stacks_buffer stacks_buffer_bin[AEE_MTK_CPU_NUMS];

struct regs_buffer {
	struct pt_regs regs;
	int real_len;
	struct task_struct *tsk;
};
static struct regs_buffer regs_buffer_bin[AEE_MTK_CPU_NUMS];


int in_fiq_handler(void)
{
	return atomic_read(&wdt_enter_fiq);
}

/* debug EMI */
__weak void dump_emi_outstanding(void) {}

void __weak sysrq_sched_debug_show_at_AEE(void)
{
	pr_notice("%s weak function at %s\n", __func__, __FILE__);
}

void aee_wdt_percpu_printf(int cpu, const char *fmt, ...)
{
	va_list args;

	if (!wdt_percpu_log_buf[cpu])
		return;

	va_start(args, fmt);
	wdt_percpu_log_length[cpu] +=
	    vsnprintf((wdt_percpu_log_buf[cpu] + wdt_percpu_log_length[cpu]),
		      (WDT_PERCPU_LOG_SIZE - wdt_percpu_log_length[cpu]),
		      fmt, args);
	va_end(args);
}

void aee_wdt_printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	wdt_log_length += vsnprintf((wdt_log_buf + wdt_log_length),
			(sizeof(wdt_log_buf) - wdt_log_length), fmt, args);
	va_end(args);
}


/* save registers in bin buffer, may comes from various cpu */
static void aee_dump_cpu_reg_bin(int cpu, struct pt_regs *regs)
{
#ifdef CONFIG_ARM64
	int i;

	aee_wdt_percpu_printf(cpu,
			"pc : %016llx, lr : %016llx, pstate : %016llx\n",
			      regs->pc, regs->regs[30], regs->pstate);
	aee_wdt_percpu_printf(cpu, "sp : %016llx\n", regs->sp);
	for (i = 29; i >= 0; i--) {
		aee_wdt_percpu_printf(cpu, "x%-2d: %016llx ", i, regs->regs[i]);
		if (i % 2 == 0)
			aee_wdt_percpu_printf(cpu, "\n");
	}
	aee_wdt_percpu_printf(cpu, "\n");
#else
	aee_wdt_percpu_printf(cpu, "pc  : %08lx, lr : %08lx, cpsr : %08lx\n",
			      regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr);
	aee_wdt_percpu_printf(cpu, "sp  : %08lx, ip : %08lx, fp : %08lx\n",
			      regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	aee_wdt_percpu_printf(cpu, "r10 : %08lx, r9 : %08lx, r8 : %08lx\n",
			      regs->ARM_r10, regs->ARM_r9, regs->ARM_r8);
	aee_wdt_percpu_printf(cpu, "r7  : %08lx, r6 : %08lx, r5 : %08lx\n",
			      regs->ARM_r7, regs->ARM_r6, regs->ARM_r5);
	aee_wdt_percpu_printf(cpu, "r4  : %08lx, r3 : %08lx, r2 : %08lx\n",
			      regs->ARM_r4, regs->ARM_r3, regs->ARM_r2);
	aee_wdt_percpu_printf(cpu, "r1  : %08lx, r0 : %08lx\n", regs->ARM_r1,
			regs->ARM_r0);
#endif
	memcpy(&(regs_buffer_bin[cpu].regs), regs, sizeof(struct pt_regs));
	regs_buffer_bin[cpu].real_len = sizeof(struct pt_regs);
}

/* dump the stack into per CPU buffer */
static void aee_wdt_dump_stack_bin(unsigned int cpu, unsigned long bottom,
		unsigned long top)
{
	int i;
	unsigned long p;
	unsigned long first;
	unsigned int val;
	char str[sizeof(" 12345678") * 8 + 1];

	stacks_buffer_bin[cpu].real_len =
	    aee_dump_stack_top_binary(stacks_buffer_bin[cpu].bin_buf,
			sizeof(stacks_buffer_bin[cpu].bin_buf), bottom, top);
	stacks_buffer_bin[cpu].top = top;
	stacks_buffer_bin[cpu].bottom = bottom;

	/* should check stack address in kernel range */
	if (bottom & 3) {
		aee_wdt_percpu_printf(cpu, "%s bottom unaligned %08lx\n",
				__func__, bottom);
		return;
	}
	if (!((bottom >= (PAGE_OFFSET + THREAD_SIZE))
			&& mrdump_virt_addr_valid(bottom))) {
		aee_wdt_percpu_printf(cpu,
				"%s bottom out of kernel addr space %08lx\n",
				__func__, bottom);
		return;
	}
	if (!((top >= (PAGE_OFFSET + THREAD_SIZE))
			&& mrdump_virt_addr_valid(bottom))) {
		aee_wdt_percpu_printf(cpu,
				"%s top out of kernel addr space %08lx\n",
				__func__, top);
		return;
	}
#ifdef CONFIG_ARM64
	aee_wdt_percpu_printf(cpu, "stack (0x%016lx to 0x%016lx)\n",
				bottom, top);
#else
	aee_wdt_percpu_printf(cpu, "stack (0x%08lx to 0x%08lx)\n", bottom, top);
#endif

	for (first = bottom & ~31; first < top; first += 32) {
		memset(str, ' ', sizeof(str));
		str[sizeof(str) - 1] = '\0';

		for (p = first, i = 0; i < 8 && p < top; i++, p += 4) {
			if (p >= bottom && p < top) {
				if (!__get_user(val, (unsigned int *)p))
					sprintf(str + i * 9, " %08x", val);
				else
					sprintf(str + i * 9, " ????????");
			}
		}
		aee_wdt_percpu_printf(cpu, "%04lx:%s\n", first & 0xffff, str);
	}
}
#ifndef CONFIG_ARM64
static inline int in_exception_text(unsigned long ptr)
{
	char *die_ptr = NULL;

	pr_notice("FIXME!!!\n");
	*die_ptr = 5;
	return 0;
}
#endif

/* dump the backtrace into per CPU buffer */
static void aee_wdt_dump_backtrace(unsigned int cpu, struct pt_regs *regs)
{
	int i;
	unsigned long high, bottom, fp;
	struct stackframe cur_frame;
	struct pt_regs *excp_regs;

	bottom = regs->reg_sp;
	if (!mrdump_virt_addr_valid(bottom)) {
		aee_wdt_percpu_printf(cpu, "invalid sp[%lx]\n", bottom);
		return;
	}
	high = ALIGN(bottom, THREAD_SIZE);
	cur_frame.fp = regs->reg_fp;
	cur_frame.pc = regs->reg_pc;
#ifndef CONFIG_ARM64
	cur_frame.sp = regs->reg_sp;
#endif
	wdt_percpu_stackframe[cpu][0] = regs->reg_pc;
	for (i = 1; i < MAX_EXCEPTION_FRAME; i++) {
		fp = cur_frame.fp;
		if ((fp < bottom) || (fp >= (high + THREAD_SIZE))) {
			aee_wdt_percpu_printf(cpu, "i=%d, fp=%lx, bottom=%lx\n",
					i, fp, bottom);
			break;
		}
#ifdef CONFIG_ARM64
		if (unwind_frame(current, &cur_frame) < 0) {
			aee_wdt_percpu_printf(cpu, "unwind_frame < 0\n");
			break;
		}
#else
		if (unwind_frame(&cur_frame) < 0) {
			aee_wdt_percpu_printf(cpu, "unwind_frame < 0\n");
			break;
		}
#endif
		if (!mrdump_virt_addr_valid(cur_frame.pc)) {
			aee_wdt_percpu_printf(cpu,
				"i=%d, mrdump_virt_addr_valid fail\n", i);
			break;
		}
		if (in_exception_text(cur_frame.pc)) {
#ifdef CONFIG_ARM64
			/* work around for unknown reason do_mem_abort stack
			 * abnormal
			 */
			excp_regs = (void *)(cur_frame.fp + 0x10 + 0xa0);
			if (unwind_frame(current, &cur_frame) < 0) {
				/* skip do_mem_abort & el1_da */
				aee_wdt_percpu_printf(cpu,
					"in_exception_text unwind_frame < 0\n");
			}
#else
			excp_regs = (void *)(cur_frame.fp + 4);
#endif
			cur_frame.pc = excp_regs->reg_pc;
		}

		/* pc -4: bug fixed for add2line */
		wdt_percpu_stackframe[cpu][i] = cur_frame.pc - 4;
	}
}

/* save binary register and stack value into ram console */
static void aee_save_reg_stack_sram(int cpu)
{
	int i;
	int len = 0;

	if (regs_buffer_bin[cpu].real_len) {
		memset(str_buf[cpu], 0, sizeof(str_buf[cpu]));
		snprintf(str_buf[cpu], sizeof(str_buf[cpu]),
			 "\n\ncpu %d preempt=%lx, softirq=%lx, hardirq=%lx ",
			 cpu,
			 ((wdt_percpu_preempt_cnt[cpu] & PREEMPT_MASK)
				>> PREEMPT_SHIFT),
			 ((wdt_percpu_preempt_cnt[cpu] & SOFTIRQ_MASK)
				>> SOFTIRQ_SHIFT),
			 ((wdt_percpu_preempt_cnt[cpu] & HARDIRQ_MASK)
				>> HARDIRQ_SHIFT));
		aee_sram_fiq_log(str_buf[cpu]);

		memset(str_buf[cpu], 0, sizeof(str_buf[cpu]));
#ifdef CONFIG_ARM64
		snprintf(str_buf[cpu], sizeof(str_buf[cpu]),
				"\ncpu %d x0->x30 sp pc pstate\n", cpu);
#else
		snprintf(str_buf[cpu], sizeof(str_buf[cpu]),
			 "\ncpu %d r0->r10 fp ip sp lr pc cpsr orig_r0\n", cpu);
#endif
		aee_sram_fiq_log(str_buf[cpu]);
		aee_sram_fiq_save_bin((char *)&(regs_buffer_bin[cpu].regs),
						regs_buffer_bin[cpu].real_len);
	}

	if (stacks_buffer_bin[cpu].real_len > 0) {
		memset(str_buf[cpu], 0, sizeof(str_buf[cpu]));
#ifdef CONFIG_ARM64
		snprintf(str_buf[cpu], sizeof(str_buf[cpu]),
			"cpu %d stack [%016lx %016lx]\n",
			 cpu, stacks_buffer_bin[cpu].bottom,
			 stacks_buffer_bin[cpu].top);
#else
		snprintf(str_buf[cpu], sizeof(str_buf[cpu]),
			"cpu %d stack [%08lx %08lx]\n",
			 cpu, stacks_buffer_bin[cpu].bottom,
			 stacks_buffer_bin[cpu].top);
#endif
		aee_sram_fiq_log(str_buf[cpu]);
		aee_sram_fiq_save_bin(stacks_buffer_bin[cpu].bin_buf,
				      stacks_buffer_bin[cpu].real_len);

		memset(str_buf[cpu], 0, sizeof(str_buf[cpu]));
		len = snprintf(str_buf[cpu], sizeof(str_buf[cpu]),
				"cpu %d backtrace : ", cpu);

		for (i = 0; i < MAX_EXCEPTION_FRAME; i++) {
			if (!wdt_percpu_stackframe[cpu][i])
				break;
			len += snprintf((str_buf[cpu] + len),
				(sizeof(str_buf[cpu]) - len), "%08lx, ",
				wdt_percpu_stackframe[cpu][i]);

		}
		aee_sram_fiq_log(str_buf[cpu]);
		memset(str_buf[cpu], 0, sizeof(str_buf[cpu]));
	}

	mrdump_mini_per_cpu_regs(cpu, &regs_buffer_bin[cpu].regs,
			regs_buffer_bin[cpu].tsk);

	mrdump_save_per_cpu_reg(cpu, &regs_buffer_bin[cpu].regs);
}

__weak void aee_wdt_zap_locks(void)
{
	pr_notice("%s:weak function\n", __func__);
}

void aee_wdt_atf_info(unsigned int cpu, struct pt_regs *regs)
{
	unsigned long long t;
	unsigned long nanosec_rem;
#ifdef CONFIG_MTK_WATCHDOG
	int res = 0;
	struct wd_api *wd_api = NULL;
#endif

	aee_wdt_percpu_printf(cpu, "===> %s : cpu %d\n", __func__, cpu);
	if (!cpu_possible(cpu)) {
		aee_wdt_printf("FIQ: Watchdog time out at incorrect CPU %d ?\n",
				cpu);
		cpu = 0;
	}

	aee_wdt_percpu_printf(cpu, "CPU %d FIQ: Watchdog time out\n", cpu);
	wdt_percpu_preempt_cnt[cpu] = preempt_count();
	aee_wdt_percpu_printf(cpu, "preempt=%lx, softirq=%lx, hardirq=%lx\n",
	  ((wdt_percpu_preempt_cnt[cpu] & PREEMPT_MASK) >> PREEMPT_SHIFT),
	  ((wdt_percpu_preempt_cnt[cpu] & SOFTIRQ_MASK) >> SOFTIRQ_SHIFT),
	  ((wdt_percpu_preempt_cnt[cpu] & HARDIRQ_MASK) >> HARDIRQ_SHIFT));

	if (regs) {
		aee_dump_cpu_reg_bin(cpu, regs);
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_WDT_FIQ_STACK);
		aee_wdt_dump_stack_bin(cpu, regs->reg_sp,
				regs->reg_sp + WDT_SAVE_STACK_SIZE);
		aee_wdt_dump_backtrace(cpu, regs);
	}
	regs_buffer_bin[cpu].tsk = current;
	if (atomic_xchg(&wdt_enter_fiq, 1)) {
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_WDT_FIQ_LOOP);
		aee_wdt_percpu_printf(cpu,
				"Other CPU already enter WDT FIQ handler\n");
		dis_D_inner_flush_all();
		local_irq_disable();

		while (1)
			cpu_relax();
	}

	show_kaslr();
	/* Wait for other cpu dump */
	mdelay(1000);

	/* kernel print lock: exec aee_wdt_zap_lock() only one time */
	if (atomic_xchg(&aee_wdt_zap_lock, 0)) {
		if (!no_zap_locks) {
			aee_wdt_zap_locks();
			snprintf(str_buf[cpu], sizeof(str_buf[cpu]),
				"\nCPU%d: zap kernel print locks\n", cpu);
			aee_sram_fiq_log(str_buf[cpu]);
			memset(str_buf[cpu], 0, sizeof(str_buf[cpu]));
		}
	}

	aee_rr_rec_fiq_step(AEE_FIQ_STEP_WDT_IRQ_KICK);
#ifdef CONFIG_MTK_WATCHDOG
	res = get_wd_api(&wd_api);
	if (res) {
		aee_wdt_printf("\naee_wdt_irq_info, get wd api error\n");
	} else {
		wd_api->wd_restart(WD_TYPE_NOLOCK);
		aee_wdt_printf("\nkick=0x%08x,check=0x%08x",
				wd_api->wd_get_kick_bit(),
				wd_api->wd_get_check_bit());
	}
#else
	aee_wdt_printf("\n mtk watchdog not enable\n");
#endif
	aee_rr_rec_fiq_step(AEE_FIQ_STEP_WDT_IRQ_TIME);
	t = cpu_clock(cpu);
	nanosec_rem = do_div(t, 1000000000);
	aee_wdt_printf("\nQwdt at [%5lu.%06lu]\n", (unsigned long)t,
			nanosec_rem / 1000);
	aee_sram_fiq_log(wdt_log_buf);

#ifdef CONFIG_MTK_WATCHDOG
#ifdef CONFIG_MTK_WD_KICKER
	/* dump bind info */
	dump_wdk_bind_info();
#endif
#endif

	if (regs) {
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_WDT_IRQ_STACK);
		for (cpu = 0; cpu < AEE_MTK_CPU_NUMS; cpu++)
			aee_save_reg_stack_sram(cpu);
		aee_sram_fiq_log("\n\n");
	} else {
		aee_wdt_printf(
			"Invalid atf_aee_debug_virt_addr, no register dump\n");
	}

	/* add __per_cpu_offset */
	mrdump_mini_add_entry((unsigned long)__per_cpu_offset,
			MRDUMP_MINI_SECTION_SIZE);

#ifdef CONFIG_MTK_SCHED_MONITOR
	aee_rr_rec_fiq_step(AEE_FIQ_STEP_WDT_IRQ_SCHED);
	mt_aee_dump_sched_traces();
#endif

#ifdef CONFIG_SCHED_DEBUG
	sysrq_sched_debug_show_at_AEE();
#endif

	/* avoid lock prove to dump_stack in __debug_locks_off() */
	xchg(&debug_locks, 0);
	aee_rr_rec_fiq_step(AEE_FIQ_STEP_WDT_IRQ_DONE);

	dump_emi_outstanding();

	if (aee_rr_curr_exp_type() == AEE_EXP_TYPE_HWT)
		mrdump_common_die(AEE_FIQ_STEP_WDT_IRQ_DONE,
				  AEE_REBOOT_MODE_WDT,
				  "WDT/HWT", regs);
	else
		mrdump_common_die(AEE_FIQ_STEP_WDT_IRQ_DONE,
				  AEE_REBOOT_MODE_MRDUMP_KEY,
				  "MRDUMP_KEY", regs);
}

void notrace aee_wdt_atf_entry(void)
{
#ifdef CONFIG_ARM64
	int i;
#endif
	void *regs;
	struct pt_regs pregs;
	int cpu = get_HW_cpuid();
#ifdef CONFIG_MTK_WATCHDOG
	if (mtk_rgu_status_is_sysrst() || mtk_rgu_status_is_eintrst()) {
#ifdef CONFIG_MTK_PMIC_COMMON
		if (pmic_get_register_value(PMIC_JUST_SMART_RST) == 1) {
			pr_notice("SMART RESET: TRUE\n");
			aee_sram_fiq_log("SMART RESET: TRUE\n");
		} else {
			pr_notice("SMART RESET: FALSE\n");
			aee_sram_fiq_log("SMART RESET: FALSE\n");
		}
#endif
		aee_rr_rec_exp_type(AEE_EXP_TYPE_SMART_RESET);
	} else
		aee_rr_rec_exp_type(AEE_EXP_TYPE_HWT);
#else
	aee_rr_rec_exp_type(AEE_EXP_TYPE_HWT);
#endif

	/* for per-cpu control registers */
	mrdump_save_ctrlreg(cpu);


	if (atf_aee_debug_virt_addr && cpu >= 0) {
		regs = (void *)(atf_aee_debug_virt_addr +
				(cpu * sizeof(struct atf_aee_regs)));

#ifdef CONFIG_ARM64
		pregs.pstate = ((struct atf_aee_regs *)regs)->pstate;
		pregs.pc = ((struct atf_aee_regs *)regs)->pc;
		pregs.sp = ((struct atf_aee_regs *)regs)->sp;
		for (i = 0; i < 31; i++)
			pregs.regs[i] = ((struct atf_aee_regs *)regs)->regs[i];

		snprintf(str_buf[cpu], sizeof(str_buf[cpu]),
			"WDT_CPU%d: PState=%llx, PC=%llx, SP=%llx, LR=%llx\n",
			cpu, pregs.pstate, pregs.pc, pregs.sp, pregs.regs[30]);
		aee_sram_fiq_log(str_buf[cpu]);
		memset(str_buf[cpu], 0, sizeof(str_buf[cpu]));

#else
		pregs.ARM_cpsr =
			(unsigned long)((struct atf_aee_regs *)regs)->pstate;
		pregs.ARM_pc =
			(unsigned long)((struct atf_aee_regs *)regs)->pc;
		pregs.ARM_sp =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[19];
		pregs.ARM_lr =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[18];
		pregs.ARM_ip =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[12];
		pregs.ARM_fp =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[11];
		pregs.ARM_r10 =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[10];
		pregs.ARM_r9 =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[9];
		pregs.ARM_r8 =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[8];
		pregs.ARM_r7 =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[7];
		pregs.ARM_r6 =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[6];
		pregs.ARM_r5 =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[5];
		pregs.ARM_r4 =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[4];
		pregs.ARM_r3 =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[3];
		pregs.ARM_r2 =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[2];
		pregs.ARM_r1 =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[1];
		pregs.ARM_r0 =
			(unsigned long)((struct atf_aee_regs *)regs)->regs[0];

		snprintf(str_buf[cpu], sizeof(str_buf[cpu]),
			"WDT_CPU%d: PState=%lx, PC=%lx, SP=%lx, LR=%lx\n",
			cpu, pregs.ARM_cpsr, pregs.ARM_pc, pregs.ARM_sp,
			pregs.ARM_lr);
		aee_sram_fiq_log(str_buf[cpu]);
		memset(str_buf[cpu], 0, sizeof(str_buf[cpu]));

#endif
		aee_wdt_atf_info(cpu, &pregs);
	} else {
		if (cpu >= 0)
			aee_wdt_atf_info(cpu, 0);
	}
}

int __init mrdump_wdt_init(void)
{
	int i;
	phys_addr_t atf_aee_debug_phy_addr;
	struct arm_smccc_res res;

	atomic_set(&wdt_enter_fiq, 0);
	atomic_set(&aee_wdt_zap_lock, 1);

	for (i = 0; i < AEE_MTK_CPU_NUMS; i++) {
		wdt_percpu_log_buf[i] = kzalloc(WDT_PERCPU_LOG_SIZE,
				GFP_KERNEL);
		wdt_percpu_log_length[i] = 0;
		wdt_percpu_preempt_cnt[i] = 0;
		if (!wdt_percpu_log_buf[i])
			pr_notice("\n aee_common_init : kmalloc fail\n");
	}
	memset(wdt_log_buf, 0, sizeof(wdt_log_buf));
	memset(regs_buffer_bin, 0, sizeof(regs_buffer_bin));
	memset(stacks_buffer_bin, 0, sizeof(stacks_buffer_bin));
	memset(wdt_percpu_stackframe, 0, sizeof(wdt_percpu_stackframe));
	memset(str_buf, 0, sizeof(str_buf));

	/* send SMC to ATF to register call back function
	 * Notes: return phys_addr of arm_smccc_smc() from atf will always < 4G
	 */
#ifdef CONFIG_ARM64
	arm_smccc_smc(MTK_SIP_KERNEL_WDT,
			(u64) &aee_wdt_atf_entry, 0, 0, 0, 0, 0, 0, &res);
#else
	arm_smccc_smc(MTK_SIP_KERNEL_WDT,
			(u32) &aee_wdt_atf_entry, 0, 0, 0, 0, 0, 0, &res);
#endif
	pr_notice("\n MTK_SIP_KERNEL_WDT - 0x%p\n", &aee_wdt_atf_entry);

	atf_aee_debug_phy_addr = (phys_addr_t) (0x00000000FFFFFFFFULL & res.a0);
	if (!atf_aee_debug_phy_addr
			|| (atf_aee_debug_phy_addr == 0xFFFFFFFF)) {
		pr_notice("\n invalid atf_aee_debug_phy_addr\n");
	} else {
		/* use the last 16KB in ATF log buffer */
		atf_aee_debug_virt_addr = ioremap(atf_aee_debug_phy_addr,
				ATF_AEE_DEBUG_BUF_LENGTH);
		pr_notice("\n atf_aee_debug_virt_addr = 0x%p\n",
				atf_aee_debug_virt_addr);
		if (atf_aee_debug_virt_addr)
			memset_io(atf_aee_debug_virt_addr, 0,
					ATF_AEE_DEBUG_BUF_LENGTH);
	}
	return 0;
}
