/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <linux/cpu.h>
#include <linux/hrtimer.h>


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/proc_fs.h>
#include <linux/syscore_ops.h>
#include <asm/sched_clock.h>
#include <linux/version.h>

#include <mach/mt_reg_base.h>
#include <mach/mt_gpt.h>
#include <mach/mt_timer.h>
#include <mach/irqs.h>
#include <mach/mt_cpuxgpt.h>
#include <mach/ext_wd_drv.h>

#if 0 /*this file only to do HW test*/

/* extern int nr_cpu_ids; */
/* int enable_clock(int id, unsigned char *name); */
static int test_case;

static int ts_msleep;
static int  msleep_times;

static int ts_mdelay;
static int  mdelay_times;

static int ts_udelay;
static int  udelay_times;

static long ts_hrtimer_sec;
static unsigned long ts_hrtimer_nsecs;
static int  ts_hrtimer_times;

static struct hrtimer hrtimer_test;
u64 hr_t1 = 0;
u64 hr_t2 = 0;

module_param(test_case, int, 0664);

module_param(ts_msleep, int, 0664);
module_param(msleep_times, int, 0664);

module_param(ts_mdelay, int, 0664);
module_param(mdelay_times, int, 0664);

module_param(ts_udelay, int, 0664);
module_param(udelay_times, int, 0664);

module_param(ts_hrtimer_sec, int, 0664);
module_param(ts_hrtimer_nsecs, int, 0664);
module_param(ts_hrtimer_times, int, 0664);

static DEFINE_SPINLOCK(wdt_test_lock0);
static DEFINE_SPINLOCK(wdt_test_lock1);
static struct task_struct *wk_tsk[2];/* cpu: 2 */
static int data;

static int hrtimer_test_case(void);

static int __cpuinit
cpu_callback(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int hotcpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
/* watchdog_prepare_cpu(hotcpu); */
		break;
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
/*  */
	if (hotcpu < nr_cpu_ids) {
		kthread_bind(wk_tsk[hotcpu], hotcpu);
	      wake_up_process(wk_tsk[hotcpu]);
		pr_debug("[WDK-test]cpu %d plug on ", hotcpu);
	}
		break;
#ifdef CONFIG_HOTPLUG_CPU
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		pr_debug("[WDK-test]:start Stop CPU:%d\n", hotcpu);

		break;
#endif /* CONFIG_HOTPLUG_CPU */
	}

	/*
	 * hardlockup and softlockup are not important enough
	 * to block cpu bring up.  Just always succeed and
	 * rely on printk output to flag problems.
	 */
	return NOTIFY_OK;
}

static struct notifier_block cpu_nfb __cpuinitdata = {
	.notifier_call = cpu_callback
};


static int msleep_test(int length, int times)
{
	u64 t1 = 0;
	u64 t2 = 0;
	int i = 0;

	t1 = sched_clock();
	for (i = 0; i < times; i++)
		msleep(length);

	t2 = sched_clock();
	pr_debug("msleep_test,  msleep(%d) test %d times\n", length, times);
	pr_debug("msleep_test: t1 =%lld, t2=%lld, delta=%lld\n", t1, t2, t2-t1);

}

static int udelay_test(int length, int times)
{
	u64 t1 = 0;
	u64 t2 = 0;
	int i = 0;

	t1 = sched_clock();
	for (i = 0; i < times; i++)
		udelay(length);

	t2 = sched_clock();
	pr_debug("udelay_test,  udelay_test(%d) test %d times\n", length, times);
	pr_debug("udelay_test :t1 =%lld, t2=%lld, delta=%lld\n", t1, t2, t2-t1);

}

static int mdelay_test(int length, int times)
{
	u64 t1 = 0;
	u64 t2 = 0;
	int i = 0;

	t1 = sched_clock();
	for (i = 0; i < times; i++)
		mdelay(length);

	t2 = sched_clock();
	pr_debug("mdelay_test,  mdelay_test(%d) test %d times\n", length, times);
	pr_debug("mdelay_test:t1 =%lld, t2=%lld, delta=%lld\n", t1, t2, t2-t1);
}


#define read_cntpct(cntpct_lo, cntpct_hi)   \

	__asm__ __volatile__(   \
	"MRRC p15, 0, %0, %1, c14\n"    \
	: "=r"(cntpct_lo), "=r"(cntpct_hi)   \
	:   \
	: "memory"); \

static int phycical_count_test(void)
{
	unsigned int cntpct_lo1 = 0;
	unsigned int cntpct_hi1 = 0;
	unsigned int cntpct_lo2 = 0;
	unsigned int cntpct_hi2 = 0;

	pr_debug("phycical_count_test start\n");

	while (1) {
		read_cntpct(cntpct_lo1, cntpct_hi1);
		read_cntpct(cntpct_lo2, cntpct_hi2);
		if (cntpct_hi2 == cntpct_hi1) {
			if (cntpct_lo2 < cntpct_lo1) {
				if (0xffff == cntpct_lo1) {
					pr_debug("fwq 0 by pass bug cntpct_hi1=%u,cntpct_lo1=%u, cntpct_hi2=%u,cntpct_lo2=%u\n",
						cntpct_hi1, cntpct_lo1, cntpct_hi2, cntpct_lo2);
					continue;
				}
				pr_debug("fwq 1 cntpct_hi1=%d,cntpct_lo1=%d, cntpct_hi2=%d,cntpct_lo2=%d\n",
					cntpct_hi1, cntpct_lo1, cntpct_hi2, cntpct_lo2);
				/* break; */
			}
		}
		if (cntpct_hi2 < cntpct_hi1) {
			pr_debug("fwq 2 cntpct_hi1=%d,cntpct_lo1=%d, cntpct_hi2=%d,cntpct_lo2=%d\n",
				cntpct_hi1, cntpct_lo1, cntpct_hi2, cntpct_lo2);
		/* break; */
		}
	}
	pr_debug("phycical_count_test end\n");

}
#define GPT_IRQEN           (APXGPT_BASE + 0x0000)
#define GPT_IRQSTA          (APXGPT_BASE + 0x0004)
#define GPT_IRQACK          (APXGPT_BASE + 0x0008)
static int dump_gpt_reg(void)
{
	pr_debug("IRQ STA %x\n", DRV_Reg32(GPT_IRQSTA));
	pr_debug("IRQ EN  %x\n", DRV_Reg32(GPT_IRQEN));
	pr_debug("IRQ ACK %x\n", DRV_Reg32(GPT_IRQACK));
	pr_debug("GPT3 clk %x\n", DRV_Reg32(0xF0008034));
	pr_debug("GPT3 con %x\n", DRV_Reg32(0xF0008030));
	pr_debug("GPT3 count %x\n", DRV_Reg32(0xF0008038));
	pr_debug("GPT3 cmp %x\n", DRV_Reg32(0xF000803c));
}

static int dump_gpt1_reg(void)
{
	pr_debug("IRQ STA %x\n", DRV_Reg32(GPT_IRQSTA));
	pr_debug("IRQ EN  %x\n", DRV_Reg32(GPT_IRQEN));
	pr_debug("IRQ ACK %x\n", DRV_Reg32(GPT_IRQACK));
	pr_debug("GPT3 clk %x\n", DRV_Reg32(0xF0008014));
	pr_debug("GPT3 con %x\n", DRV_Reg32(0xF0008010));
	pr_debug("GPT3 count %x\n", DRV_Reg32(0xF0008018));
	pr_debug("GPT3 cmp %x\n", DRV_Reg32(0xF000801c));
}


u64 cpuxgpt_t1 = 0;
u64 cpuxgpt_t2 = 0;
int g_cpuxgpt0_called = 0;
int g_cpuxgpt1_called = 0;
int g_cpuxgpt2_called = 0;
static irqreturn_t cpuxgpt_test_irq_handler(int irq, void *dev_id)
{
	cpuxgpt_t2 = sched_clock();
	g_cpuxgpt0_called = 1;
	pr_debug("cpuxgpt irq:%d called\n", irq);
	pr_debug("cpuxgpt t2(%lld),t1(%lld),delta(%lld)\n", cpuxgpt_t2, cpuxgpt_t1, cpuxgpt_t2-cpuxgpt_t1);
	return IRQ_HANDLED;
}

#if 1
static irqreturn_t cpuxgpt1_test_irq_handler(int irq, void *dev_id)
{
	g_cpuxgpt1_called = 1;
	pr_debug("cpuxgpt1 irq called\n");
	return IRQ_HANDLED;
}

static irqreturn_t cpuxgpt2_test_irq_handler(int irq, void *dev_id)
{
	g_cpuxgpt2_called = 1;
	pr_debug("cpuxgpt2 irq called\n");
	return IRQ_HANDLED;
}

static irqreturn_t cpuxgpt3_test_irq_handler(int irq, void *dev_id)
{
	pr_debug("cpuxgpt3 irq called\n");
	return IRQ_HANDLED;
}

static irqreturn_t cpuxgpt4_test_irq_handler(int irq, void *dev_id)
{
	pr_debug("cpuxgpt4 irq called\n");
	return IRQ_HANDLED;
}

static irqreturn_t cpuxgpt5_test_irq_handler(int irq, void *dev_id)
{
	pr_debug("cpuxgpt5 irq called\n");
	return IRQ_HANDLED;
}

static irqreturn_t cpuxgpt6_test_irq_handler(int irq, void *dev_id)
{
	pr_debug("cpuxgpt6 irq called\n");
	return IRQ_HANDLED;
}

static irqreturn_t cpuxgpt7_test_irq_handler(int irq, void *dev_id)
{
	pr_debug("cpuxgpt7 irq called\n");
	return IRQ_HANDLED;
}
#endif

void generic_timer_ppi_check(void)
{
	disable_cpuxgpt();
	cpu_xgpt_set_init_count(0x80000000, 0xffffff00);
	enable_cpuxgpt();
}

static cpuxgpt_X_interrupt_test(int cpu)
{
	disable_cpuxgpt();
	cpu_xgpt_set_init_count(0x80000000, 0xffffff00);

	/* cpu_xgpt_register_timer(cpu,cpuxgpt_test_irq_handler); */
	cpu_xgpt_register_timer(1, cpuxgpt1_test_irq_handler);
	cpu_xgpt_register_timer(2, cpuxgpt2_test_irq_handler);
	cpu_xgpt_register_timer(3, cpuxgpt3_test_irq_handler);
	cpu_xgpt_register_timer(4, cpuxgpt4_test_irq_handler);
	cpu_xgpt_register_timer(5, cpuxgpt5_test_irq_handler);
	cpu_xgpt_register_timer(6, cpuxgpt6_test_irq_handler);
	cpu_xgpt_register_timer(7, cpuxgpt7_test_irq_handler);

	/*  */
	cpu_xgpt_set_cmp_HL(cpu, 0x80000001, 0x00000100);
	cpuxgpt_t1 = sched_clock();
	enable_cpuxgpt();
	/*wait for interrupt trigger, timeout:(0,0x80000001,0x00000100) - (0x80000000,0xffffff00)*/
	msleep(20);
}

static cpuxgpt_interrupt_test(void)
{
	int i = 0;
	/* for(i = 0; i < 8; i++) */
	/* { */
		cpuxgpt_X_interrupt_test(i);
	/* } */
	/* test_case = 0; */
}

static void cpuxgpt_frequency_test(void)
{
    /* long long delta1 = 0; */
	/* long long delta2 = 0; */
	/* div1 */
	#define TIMEOUT (13000000) /* 1s = 13M */
	set_cpuxgpt_clk(CLK_DIV2);
	disable_cpuxgpt();
	cpu_xgpt_set_init_count(0x00000000, 0x00000000);
	/* set_cpuxgpt_clk(CLK_DIV1); */
	cpu_xgpt_register_timer(0, cpuxgpt_test_irq_handler);
	cpu_xgpt_set_cmp_HL(0, 0x00000000, TIMEOUT);
	cpuxgpt_t1 = sched_clock();
	set_cpuxgpt_clk(CLK_DIV1);
	enable_cpuxgpt();

	while (0 == g_cpuxgpt0_called) {
		msleep(20);
		pr_debug("busy wait\n");
	}
	g_cpuxgpt0_called = 0;
	pr_debug("cpuxgpt0-div1 t2(%lld),t1(%lld),delta(%lld)\n", cpuxgpt_t2, cpuxgpt_t1, cpuxgpt_t2-cpuxgpt_t1);

#if 1
	msleep(1000);
	/* div2 */
	disable_cpuxgpt();
	cpu_xgpt_set_init_count(0x00000000, 0x00000000);
	/* set_cpuxgpt_clk(CLK_DIV2); */
	cpu_xgpt_set_cmp_HL(0, 0x00000000, TIMEOUT);
	cpuxgpt_t1 = sched_clock();
	set_cpuxgpt_clk(CLK_DIV2);
	enable_cpuxgpt();
	while (0 == g_cpuxgpt0_called) {
		msleep(20);
		pr_debug("busy wait\n");
	}
	g_cpuxgpt0_called = 0;
	pr_debug("cpuxgpt0-div2 t2(%lld),t1(%lld),delta(%lld)\n", cpuxgpt_t2, cpuxgpt_t1, cpuxgpt_t2-cpuxgpt_t1);

	msleep(1000);
	/* div3 */
	disable_cpuxgpt();
	cpu_xgpt_set_init_count(0x00000000, 0x00000000);
	/* set_cpuxgpt_clk(CLK_DIV4); */
	cpu_xgpt_set_cmp_HL(0, 0x00000000, TIMEOUT);
	cpuxgpt_t1 = sched_clock();
	set_cpuxgpt_clk(CLK_DIV4);
	enable_cpuxgpt();
	while (0 == g_cpuxgpt0_called) {
		msleep(20);
		pr_debug("busy wait\n");
	}
	g_cpuxgpt0_called = 0;
	pr_debug("cpuxgpt0-div4 t2(%lld),t1(%lld),delta(%lld)\n", cpuxgpt_t2, cpuxgpt_t1, cpuxgpt_t2-cpuxgpt_t1);
#endif
}

void cpuxgpt_halt_on_debug_test(void)
{
	unsigned int cntpct_lo1 = 0;
	unsigned int cntpct_hi1 = 0;

	disable_cpuxgpt();
	cpu_xgpt_set_init_count(0x00000000, 0x00000000);
	set_cpuxgpt_clk(CLK_DIV1);
	enable_cpuxgpt();
	cpu_xgpt_halt_on_debug_en(1);

/*
	read_cntpct(cntpct_lo1, cntpct_hi1);
	localtimer_get_phy_count();
	read_cntpct(cntpct_lo1, cntpct_hi1);
	localtimer_get_phy_count();
	pr_debug("11111 ca7 counter(%u,%u) == %lld\n", cntpct_hi1,cntpct_lo1,localtimer_get_phy_count() );


	read_cntpct(cntpct_lo1, cntpct_hi1);
	pr_debug("22222 ca7 counter(%u,%u) == %lld\n", cntpct_hi1,cntpct_lo1,localtimer_get_phy_count() );
	read_cntpct(cntpct_lo1, cntpct_hi1);
	pr_debug("33333 ca7 counter(%u,%u) == %lld\n", cntpct_hi1,cntpct_lo1,localtimer_get_phy_count() );
	read_cntpct(cntpct_lo1, cntpct_hi1);
	pr_debug("44444 ca7 counter(%u,%u) == %lld\n", cntpct_hi1,cntpct_lo1,localtimer_get_phy_count() );
*/

}

static void  cpuxgpt_test_case_help(void)
{
	pr_debug("1:cpuxgpt_interrupt_test\n");
	pr_debug("2:cpuxgpt_frequency_test\n");
	pr_debug("3: cpuxgpt_halt_on_debug_test\n");

}

void cpuxgpt_run_case(int testcase)
{
	switch (testcase) {
	case 0:
			cpuxgpt_test_case_help();
			break;
	case 1:
			cpuxgpt_interrupt_test();
			break;
	case 2:
			cpuxgpt_frequency_test();
			break;
	case 3:
			cpuxgpt_halt_on_debug_test();
			break;
	case 4:
			generic_timer_ppi_check();
			break;
	default:
			pr_debug("[%s] invalid testcase\n", __func__);
	}
	test_case = 0;
}

static int idx;
static int ktimer_thread_test(void *arg)
{
	struct sched_param param = { .sched_priority = RTPM_PRIO_WDT};
	int cpu;
	unsigned int flags;
	int ret = 0;
	unsigned int cntpct_lo1 = 0;
	unsigned int cntpct_hi1 = 0;
	static unsigned int last_cntpct_lo1;
	static unsigned int last_cntpct_hi1;
	int i = 0;

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

for (;;) {
		spin_lock(&wdt_test_lock0);
		cpu = smp_processor_id();
		spin_unlock(&wdt_test_lock0);
		pr_debug("timer test debug start, cpu:%d\n", cpu);
		/* phycical_count_test(); */
		cpuxgpt_run_case(test_case);

	if (ts_msleep != 0) {
		/* pr_debug("CPU:%d, msleep(%d) test\n", cpu,ts_msleep); */
		if (0 != msleep_times) {
			msleep_test(ts_msleep, msleep_times);
		else
			msleep_test(ts_msleep, 1);
	}

	if (ts_mdelay != 0) {
		/* pr_debug("CPU:%d, mdelay(%d) test\n", cpu,ts_mdelay); */
		if (0 != mdelay_times) {
			mdelay_test(ts_mdelay, mdelay_times);
		else
			mdelay_test(ts_mdelay, 1);
	}

	if (ts_udelay != 0) {
		/* pr_debug("CPU:%d, udelay(%d) test\n", cpu,ts_udelay); */
		if (0 != udelay_times)
			udelay_test(ts_udelay, udelay_times);
		else
			udelay_test(ts_udelay, 1);
	}

	if (ts_hrtimer_nsecs != 0 || ts_hrtimer_sec != 0)
		hrtimer_test_case();

	msleep(5*1000);/* 5s */

	read_cntpct(cntpct_lo1, cntpct_hi1);
	pr_debug("ca7 counter(%u,%u) == %lld\n", cntpct_hi1, cntpct_lo1, localtimer_get_phy_count());

	/* pr_debug("cpuxgpt(%d) set 100ms timer,idx=%d\n" ,idx%8,idx); */
	/* cpu_xgpt_set_timer(idx%8,100000000); */
	/* idx++; */

	pr_debug("timer test debug end, cpu:%d\n", cpu);
	#if 0
	pr_debug("fwq2 before set\n");
	dump_gpt_reg();
	pr_debug("set gpt3 5s oneshot\n");

	DRV_WriteReg32(0xF0008000, 0x7);
	DRV_WriteReg32(0xF0008030, 0x0);

	DRV_WriteReg32(0xF0008038, 0x0);
	DRV_WriteReg32(0xF0008034, 0x0);
	DRV_WriteReg32(0xF000803c, 0x1c9c380);

	DRV_WriteReg32(0xF0008030, 0x3);

	read_cntpct(cntpct_lo1, cntpct_hi1);
	if (cntpct_hi1 == last_cntpct_hi1 && cntpct_lo1 == last_cntpct_lo1)
		pr_debug("fwq local count error !!!\n");
	last_cntpct_hi1 = cntpct_hi1;
	last_cntpct_lo1 = cntpct_lo1;
	pr_debug("ca7 counter(%u,%u), cpu:%d\n", cntpct_hi1, cntpct_lo1);
	pr_debug("timer test debug end, cpu:%d\n", cpu);
	#endif
	}
	return 0;
}

static enum hrtimer_restart hrtimer_test_func(struct hrtimer *timer)
{
	hr_t2 = sched_clock();
	pr_debug("[hrtimer_test_func] t1=%lld,t2 =%lld\n", hr_t1, hr_t2);
	pr_debug("[hrtimer_test_func] delta=%lld\n", hr_t2-hr_t1);
	return HRTIMER_NORESTART;
}

static int hrtimer_test_case(void)
{
	int ret = 0;
	int value = 0;

	pr_debug("hrtimer_test_case:ts_hrtimer_sec=%ld, ts_hrtimer_nsecs=%ld\n", ts_hrtimer_sec, ts_hrtimer_nsecs);
	hr_t1 = sched_clock();
	hrtimer_start(&hrtimer_test, ktime_set(ts_hrtimer_sec, ts_hrtimer_nsecs), HRTIMER_MODE_REL);

}
static int start_kicker(void)
{

	int i;
	unsigned char name[15] = {0};

	hrtimer_init(&hrtimer_test, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_test.function = hrtimer_test_func;

	for (i = 0; i < 1; i++) {
		sprintf(name, "timer-test%d", i);
		pr_debug("[Timer]:thread name: %s\n", name);
		wk_tsk[i] = kthread_create(ktimer_thread_test, &data, name);
		if (IS_ERR(wk_tsk[i])) {
			int ret = PTR_ERR(wk_tsk[i]);

		wk_tsk[i] = NULL;
			return ret;
		}
		kthread_bind(wk_tsk[i], i);
		wake_up_process(wk_tsk[i]);
	}
	return 0;
}
static int __init test_init(void)
{
	/* enable_clock(12, "Vfifo"); */

	/* register_cpu_notifier(&cpu_nfb); */
	start_kicker();
	return 0;
}

static void __init test_exit(void)
{

}
#if 0
int TimerUT_proc_init(void)
{

	struct proc_dir_entry *de = create_proc_entry(PROC_WK, 0660, 0);

	pr_debug("[WDK] Initialize proc\n");

	de->read_proc = wk_proc_cmd_read;
	de->write_proc = wk_proc_cmd_write;

	return 0;
}


void TimerUT_proc_exit(void)
{

	remove_proc_entry(PROC_WK, NULL);

}
#endif
module_init(test_init);
module_exit(test_exit);
#endif

