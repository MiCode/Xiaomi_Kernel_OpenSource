// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/sysrq.h>
#include <uapi/linux/sched/types.h>

#include <mt-plat/mboot_params.h>
#include "mrdump_helper.h"

/*************************************************************************
 * Feature configure region
 *************************************************************************/
#define WK_MAX_MSG_SIZE (128)
#define SOFT_KICK_RANGE     (100*1000) // 100ms
#define WDT_LENGTH_TIMEOUT(n)   ((n) << 5)
#define WDT_LENGTH      0x04
#define WDT_LENGTH_KEY      0x8
#define WDT_RST         0x08
#define WDT_RST_RELOAD      0x1971
#define CPU_NR (nr_cpu_ids)

static int start_kicker(void);
static int g_kicker_init;
static DEFINE_SPINLOCK(lock);
struct task_struct *wk_tsk[16] = { 0 };	/* max cpu 16 */
static unsigned int wk_tsk_bind[16] = { 0 };	/* max cpu 16 */
static unsigned long long wk_tsk_bind_time[16] = { 0 };	/* max cpu 16 */
static unsigned long long wk_tsk_kick_time[16] = { 0 };	/* max cpu 16 */
static char wk_tsk_buf[128] = { 0 };
static unsigned long kick_bit;
static int g_kinterval = -1;
static struct work_struct wdk_work;
static struct workqueue_struct *wdk_workqueue;
static unsigned int lasthpg_act;
static unsigned int lasthpg_cpu;
static unsigned long long lasthpg_t;
static unsigned long long lastsuspend_t;
static unsigned long long lastresume_t;
static struct notifier_block wdt_pm_nb;
static unsigned long g_nxtKickTime;
static int g_hang_detected;
static void __iomem *toprgu_base;
static void __iomem *apxgpt_base;
static unsigned int cpus_kick_bit;

static unsigned int get_check_bit(void)
{
	return cpus_kick_bit;
}

static unsigned int get_kick_bit(void)
{
	return kick_bit;
}

static int start_kicker_thread_with_default_setting(void)
{
	g_kinterval = 15;	/* default interval: 20s */
	start_kicker();

	pr_debug("[wdk] %s done\n", __func__);
	return 0;
}

void wk_start_kick_cpu(int cpu)
{
	if (IS_ERR(wk_tsk[cpu])) {
		pr_debug("[wdk] wk_task[%d] is NULL\n", cpu);
	} else {
		kthread_bind(wk_tsk[cpu], cpu);
		pr_info("[wdk] bind thread %d to cpu %d\n",
			wk_tsk[cpu]->pid, cpu);
		wake_up_process(wk_tsk[cpu]);
	}
}

void dump_wdk_bind_info(void)
{
	int i = 0;

	snprintf(wk_tsk_buf, sizeof(wk_tsk_buf),
		"kick=0x%x,check=0x%x\n",
		get_kick_bit(), get_check_bit());

	pr_info("%s", wk_tsk_buf);
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_sram_fiq_log("\n");
	aee_sram_fiq_log(wk_tsk_buf);
#endif
	for (i = 0; i < CPU_NR; i++) {
		if (wk_tsk[i] != NULL) {
			memset(wk_tsk_buf, 0, sizeof(wk_tsk_buf));
			snprintf(wk_tsk_buf, sizeof(wk_tsk_buf),
				"[wdk]CPU %d, %d, %lld, %d, %ld, %lld\n",
				i, wk_tsk_bind[i], wk_tsk_bind_time[i],
				wk_tsk[i]->on_rq, wk_tsk[i]->state,
				wk_tsk_kick_time[i]);
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
			aee_sram_fiq_log(wk_tsk_buf);
#endif
		}
	}
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	aee_sram_fiq_log("\n");
#endif

	if (toprgu_base) {
		iowrite32((WDT_LENGTH_TIMEOUT(3) << 6) | WDT_LENGTH_KEY,
			toprgu_base + WDT_LENGTH);
		iowrite32(WDT_RST_RELOAD, toprgu_base + WDT_RST);
	}
}

void kicker_cpu_bind(int cpu)
{
	if (IS_ERR(wk_tsk[cpu]))
		pr_debug("[wdk]wk_task[%d] is NULL\n", cpu);
	else {
		/* kthread_bind(wk_tsk[cpu], cpu); */
		WARN_ON_ONCE(set_cpus_allowed_ptr(wk_tsk[cpu],
			cpumask_of(cpu)) < 0);
		wake_up_process(wk_tsk[cpu]);
		wk_tsk_bind[cpu] = 1;
		wk_tsk_bind_time[cpu] = sched_clock();
	}
}

void wk_cpu_update_bit_flag(int cpu, int plug_status)
{
	if (plug_status == 1) {	/* plug on */
		spin_lock(&lock);
		cpus_kick_bit |= (1 << cpu);
		kick_bit = 0;
		lasthpg_cpu = cpu;
		lasthpg_act = plug_status;
		lasthpg_t = sched_clock();
		spin_unlock(&lock);
	}
	if (plug_status == 0) {	/* plug off */
		spin_lock(&lock);
		cpus_kick_bit &= (~(1 << cpu));
		kick_bit = 0;
		lasthpg_cpu = cpu;
		lasthpg_act = plug_status;
		lasthpg_t = sched_clock();
		wk_tsk_bind[cpu] = 0;
		spin_unlock(&lock);
	}
}

static void (*p_mt_aee_dump_irq_info)(void);
void kwdt_regist_irq_info(void (*fn)(void))
{
	p_mt_aee_dump_irq_info = fn;
}
EXPORT_SYMBOL_GPL(kwdt_regist_irq_info);

static void kwdt_process_kick(int local_bit, int cpu,
				unsigned long curInterval, char msg_buf[])
{
	unsigned int dump_timeout = 0, tmp = 0;

	local_bit = kick_bit;
	if ((local_bit & (1 << cpu)) == 0) {
		/* pr_debug("[wdk] set kick_bit\n"); */
		local_bit |= (1 << cpu);
		/* aee_rr_rec_wdk_kick_jiffies(jiffies); */
	} else if (g_hang_detected == 0) {
		g_hang_detected = 1;
		dump_timeout = 1;
	}

	wk_tsk_kick_time[cpu] = sched_clock();
	snprintf(msg_buf, WK_MAX_MSG_SIZE,
	 "[wdk-c] cpu=%d,lbit=0x%x,cbit=0x%x,%d,%d,%lld,%lld,%lld,[%lld,%ld]\n",
	 cpu, local_bit, get_check_bit(), lasthpg_cpu, lasthpg_act,
	 lasthpg_t, lastsuspend_t, lastresume_t, wk_tsk_kick_time[cpu],
	 curInterval);

	if (local_bit == get_check_bit()) {
		msg_buf[5] = 'k';
		local_bit = 0;
	}

	kick_bit = local_bit;
	if (apxgpt_base) {
		/* "DB" signature */
		tmp = 0x4442 << 16;
		tmp |= (local_bit & 0xFF) << 8;
		tmp |= get_check_bit() & 0xFF;
		__raw_writel(tmp, apxgpt_base + 0x7c);
	}

	spin_unlock(&lock);

	pr_info("%s", msg_buf);

	if (dump_timeout) {
		dump_wdk_bind_info();
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR)
		if (p_mt_aee_dump_irq_info)
			p_mt_aee_dump_irq_info();
#endif
		sysrq_sched_debug_show_at_AEE();
		//panic("cpu hang detected!\n");
	}
}

static int kwdt_thread(void *arg)
{
	struct sched_param param = {.sched_priority = 99 };
	int cpu = 0;
	int local_bit = 0;
	unsigned long curInterval = 0;
	char msg_buf[WK_MAX_MSG_SIZE];

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	for (;;) {
		if (kthread_should_stop()) {
			pr_info("[wdk] kthread_should_stop do !!\n");
			break;
		}
		msg_buf[0] = '\0';
		/*
		 * pr_debug("[wdk] loc_wk_wdt(%x),loc_wk_wdt->ready(%d)\n",
		 * loc_wk_wdt ,loc_wk_wdt->ready);
		 */
		curInterval = g_kinterval*1000*1000;
		spin_lock(&lock);
		/* smp_processor_id does not
		 * allowed preemptible context
		 */
		cpu = smp_processor_id();

		/* to avoid wk_tsk[cpu] had not created out */
		if (wk_tsk[cpu] != 0) {
			/* only process kicking info
			 * if thread-x is on cpu-x
			 */
			if (wk_tsk[cpu]->pid == current->pid) {
				if (kick_bit == 0) {
					g_nxtKickTime =
						ktime_to_us(ktime_get())
						+ g_kinterval*1000*1000;
					curInterval =
						g_kinterval*1000*1000;
				} else {
					curInterval =	g_nxtKickTime
					- ktime_to_us(ktime_get());
				}
				/* to avoid interval too long */
				if (curInterval > g_kinterval*1000*1000)
					curInterval =
						g_kinterval*1000*1000;
				kwdt_process_kick(local_bit, cpu,
					curInterval, msg_buf);
			} else {
				spin_unlock(&lock);
			}
		} else {
			spin_unlock(&lock);
		}

		usleep_range(curInterval, curInterval + SOFT_KICK_RANGE);
	}
	pr_debug("[wdk] wdk thread stop, cpu:%d, pid:%d\n", cpu, current->pid);
	return 0;
}

static int start_kicker(void)
{

	int i;

	for (i = 0; i < CPU_NR; i++) {
		wk_tsk[i] = kthread_create(kwdt_thread,
			(void *)(unsigned long)i, "wdtk-%d", i);
		if (IS_ERR(wk_tsk[i])) {
			int ret = PTR_ERR(wk_tsk[i]);

			wk_tsk[i] = NULL;
			pr_info("[wdk]kthread_create failed, wdtk-%d\n", i);
			return ret;
		}
		/* wk_cpu_update_bit_flag(i,1); */
		wk_start_kick_cpu(i);
	}
	g_kicker_init = 1;
	pr_info("[wdk] WDT start kicker done CPU_NR=%d\n", CPU_NR);
	return 0;
}

static int wk_cpu_callback_online(unsigned int cpu)
{
	wk_cpu_update_bit_flag(cpu, 1);

	/*
	 * Bind WDK thread to this CPU.
	 * NOTE: Thread binding must be executed after CPU is ready
	 * (online).
	 */
	if (g_kicker_init == 1)
		kicker_cpu_bind(cpu);
	else
		pr_info("kicker was not bound to CPU%d\n", cpu);

	return 0;
}

static int wk_cpu_callback_offline(unsigned int cpu)
{
	wk_cpu_update_bit_flag(cpu, 0);

	return 0;
}

static void wdk_work_callback(struct work_struct *work)
{
	int res = 0;
	int i = 0;

	cpu_hotplug_disable();

	res = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
		"watchdog:wdkctrl:online", wk_cpu_callback_online, NULL);
	if (res < 0)
		pr_info("[wdk]setup CPUHP_AP_ONLINE_DYN fail %d\n", res);

	res = cpuhp_setup_state_nocalls(CPUHP_BP_PREPARE_DYN,
		"watchdog:wdkctrl:offline", NULL, wk_cpu_callback_offline);
	if (res < 0)
		pr_info("[wdk]setup CPUHP_BP_PREPARE_DYN fail %d\n", res);

	for (i = 0; i < CPU_NR; i++) {
		if (cpu_online(i)) {
			wk_cpu_update_bit_flag(i, 1);
			pr_debug("[wdk]init cpu online %d\n", i);
		} else {
			wk_cpu_update_bit_flag(i, 0);
			pr_debug("[wdk]init cpu offline %d\n", i);
		}
	}

	start_kicker_thread_with_default_setting();

	cpu_hotplug_enable();

	pr_info("[wdk]init_wk done late_initcall cpus_kick_bit=0x%x -----\n",
		cpus_kick_bit);

}

static int wdt_pm_notify(struct notifier_block *notify_block,
			unsigned long mode, void *unused)
{
	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
	case PM_RESTORE_PREPARE:
		lastsuspend_t = sched_clock();
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		lastresume_t = sched_clock();
		break;
	}

	return 0;
}

static int __init init_wk_check_bit(void)
{
	int i = 0;

	pr_debug("[wdk]arch init check_bit=0x%x+++++\n", cpus_kick_bit);
	for (i = 0; i < CPU_NR; i++)
		wk_cpu_update_bit_flag(i, 1);

	pr_debug("[wdk]arch init check_bit=0x%x-----\n", cpus_kick_bit);
	return 0;
}

static const struct of_device_id toprgu_of_match[] = {
	{ .compatible = "mediatek,mt6589-wdt" },
	{},
};

static const struct of_device_id apxgpt_of_match[] = {
	{ .compatible = "mediatek,apxgpt", },
	{},
};

static int __init hangdet_init(void)
{
	int res = 0;
	struct device_node *np_toprgu;
	struct device_node *np_apxgpt;

	for_each_matching_node(np_toprgu, toprgu_of_match) {
		pr_info("%s: compatible node found: %s\n",
			 __func__, np_toprgu->name);
		break;
	}

	toprgu_base = of_iomap(np_toprgu, 0);
	if (!toprgu_base)
		pr_debug("apxgpt iomap failed\n");

	init_wk_check_bit();

	wdk_workqueue = create_singlethread_workqueue("mt-wdk");
	INIT_WORK(&wdk_work, wdk_work_callback);

	res = queue_work(wdk_workqueue, &wdk_work);

	if (!res)
		pr_info("[wdk]wdk_work start return:%d!\n", res);

	wdt_pm_nb.notifier_call = wdt_pm_notify;
	register_pm_notifier(&wdt_pm_nb);

	/*
	 * In order to dump kick and check bit mask in ATF, the two value
	 * is kept in apxgpt registers
	 */
	for_each_matching_node(np_apxgpt, apxgpt_of_match) {
		pr_info("%s: compatible node found: %s\n",
			 __func__, np_apxgpt->name);
		break;
	}

	apxgpt_base = of_iomap(np_apxgpt, 0);
	if (!apxgpt_base)
		pr_debug("apxgpt iomap failed\n");

	return 0;
}

static void __exit hangdet_exit(void)
{
	unregister_pm_notifier(&wdt_pm_nb);
	kthread_stop((struct task_struct *)wk_tsk);
}

module_init(hangdet_init);
module_exit(hangdet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mediatek inc.");
MODULE_DESCRIPTION("The cpu hang detector");
