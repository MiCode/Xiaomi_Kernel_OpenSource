// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG "[CONNADP]"
#include "connectivity_build_in_adapter.h"

#include <kernel/sched/sched.h>

/*device tree mode*/
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/of_address.h>
#endif

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of_reserved_mem.h>
#include <linux/trace_events.h>
#include <linux/notifier.h>

#include <linux/interrupt.h>
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
#include <mtk_6306_gpio.h>
#endif

#ifdef CONNADP_HAS_CLOCK_BUF_CTRL
#include <mtk_clkbuf_ctl.h>
#endif

/* MMC */
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <sdio_ops.h>


#ifdef CONFIG_ARCH_MT6570
#define CPU_BOOST y
#endif
#ifdef CONFIG_ARCH_MT6755
#define CPU_BOOST y
#endif
#ifdef CONFIG_MACH_MT6757
#define CPU_BOOST y
#endif
#ifdef CONFIG_MACH_MT6763
#define CPU_BOOST y
#endif

#ifdef CPU_BOOST
#include "mtk_ppm_api.h"
#endif

#ifndef TASK_STATE_TO_CHAR_STR
#define TASK_STATE_TO_CHAR_STR "RSDTtXZxKWPNn"
#endif

/* conninfra init/deinit notifier */
static BLOCKING_NOTIFIER_HEAD(conn_state_notifier_list);
static DEFINE_MUTEX(conn_state_notify_mutex);
struct connsys_state_info g_connsys_state_info;

void connectivity_export_show_stack(struct task_struct *tsk, unsigned long *sp)
{
#ifdef CFG_CONNADP_BUILD_IN
	show_stack(tsk, sp, KERN_DEFAULT);
#else
	pr_info("%s not support in connadp.ko\n", __func__);
#endif
}
EXPORT_SYMBOL(connectivity_export_show_stack);

void connectivity_export_tracing_record_cmdline(struct task_struct *tsk)
{
#ifdef CONFIG_TRACING
#ifdef CFG_CONNADP_BUILD_IN
	tracing_record_cmdline(tsk);
#else
	pr_info("%s not support in connadp.ko\n", __func__);
#endif
#endif
}
EXPORT_SYMBOL(connectivity_export_tracing_record_cmdline);

void connectivity_export_conap_scp_init(unsigned int chip_info, phys_addr_t emi_phy_addr)
{
	pr_info("[%s] [%x][%llx] [%x][%llx]", __func__,
				chip_info, emi_phy_addr,
				g_connsys_state_info.chip_info, g_connsys_state_info.emi_phy_addr);

	mutex_lock(&conn_state_notify_mutex);
	g_connsys_state_info.chip_info = chip_info;
	g_connsys_state_info.emi_phy_addr = emi_phy_addr;

	blocking_notifier_call_chain(&conn_state_notifier_list, 1, &g_connsys_state_info);

	mutex_unlock(&conn_state_notify_mutex);
}
EXPORT_SYMBOL(connectivity_export_conap_scp_init);


void connectivity_export_conap_scp_deinit(void)
{

	mutex_lock(&conn_state_notify_mutex);
	blocking_notifier_call_chain(&conn_state_notifier_list, 0, NULL);
	mutex_unlock(&conn_state_notify_mutex);
}
EXPORT_SYMBOL(connectivity_export_conap_scp_deinit);


void connectivity_register_state_notifier(struct notifier_block *nb)
{
	mutex_lock(&conn_state_notify_mutex);
	blocking_notifier_chain_register(&conn_state_notifier_list, nb);

	pr_debug("[CONNADP] register conn_state notify callback..chip=[%x]\n",
				g_connsys_state_info.chip_info);

	if (g_connsys_state_info.chip_info != 0)
		nb->notifier_call(nb, 1, NULL);
	mutex_unlock(&conn_state_notify_mutex);
}
EXPORT_SYMBOL(connectivity_register_state_notifier);

void connectivity_unregister_state_notifier(struct notifier_block *nb)
{
	mutex_lock(&conn_state_notify_mutex);
	blocking_notifier_chain_unregister(&conn_state_notifier_list, nb);
	mutex_unlock(&conn_state_notify_mutex);
}
EXPORT_SYMBOL(connectivity_unregister_state_notifier);

#ifdef CPU_BOOST
bool connectivity_export_spm_resource_req(unsigned int user,
					  unsigned int req_mask)
{
	return spm_resource_req(user, req_mask);
}
EXPORT_SYMBOL(connectivity_export_spm_resource_req);

void connectivity_export_mt_ppm_sysboost_freq(enum ppm_sysboost_user user,
					      unsigned int freq)
{
	mt_ppm_sysboost_freq(user, freq);
}
EXPORT_SYMBOL(connectivity_export_mt_ppm_sysboost_freq);

void connectivity_export_mt_ppm_sysboost_core(enum ppm_sysboost_user user,
					      unsigned int core_num)
{
	mt_ppm_sysboost_core(user, core_num);
}
EXPORT_SYMBOL(connectivity_export_mt_ppm_sysboost_core);

void connectivity_export_mt_ppm_sysboost_set_core_limit(
				enum ppm_sysboost_user user,
				unsigned int cluster,
				int min_core, int max_core)
{
	mt_ppm_sysboost_set_core_limit(user, cluster, min_core, max_core);
}
EXPORT_SYMBOL(connectivity_export_mt_ppm_sysboost_set_core_limit);

void connectivity_export_mt_ppm_sysboost_set_freq_limit(
				enum ppm_sysboost_user user,
				unsigned int cluster,
				int min_freq, int max_freq)
{
	mt_ppm_sysboost_set_freq_limit(user, cluster, min_freq, max_freq);
}
EXPORT_SYMBOL(connectivity_export_mt_ppm_sysboost_set_freq_limit);
#endif

/*******************************************************************************
 * Clock Buffer Control
 ******************************************************************************/
#ifdef CONNADP_HAS_CLOCK_BUF_CTRL
void connectivity_export_clk_buf_ctrl(enum clk_buf_id id, bool onoff)
{
	clk_buf_ctrl(id, onoff);
}
EXPORT_SYMBOL(connectivity_export_clk_buf_ctrl);
#endif

/*******************************************************************************
 * MT6306 I2C-based GPIO Expander
 ******************************************************************************/
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
void connectivity_export_mt6306_set_gpio_out(unsigned long pin,
					unsigned long output)
{
	mt6306_set_gpio_out(MT6306_GPIO_01, MT6306_GPIO_OUT_LOW);
}
EXPORT_SYMBOL(connectivity_export_mt6306_set_gpio_out);

void connectivity_export_mt6306_set_gpio_dir(unsigned long pin,
					unsigned long dir)
{
	mt6306_set_gpio_dir(MT6306_GPIO_01, MT6306_GPIO_DIR_OUT);
}
EXPORT_SYMBOL(connectivity_export_mt6306_set_gpio_dir);
#endif

/*******************************************************************************
 * MMC
 ******************************************************************************/
int connectivity_export_mmc_io_rw_direct(struct mmc_card *card,
				int write, unsigned int fn,
				unsigned int addr, u8 in, u8 *out)
{
	/* TODO: porting this function if sdio is used */
	/* return mmc_io_rw_direct(card, write, fn, addr, in, out); */
	return 0;
}
EXPORT_SYMBOL(connectivity_export_mmc_io_rw_direct);

/******************************************************************************
 * GPIO dump information
 ******************************************************************************/
#ifndef CONFIG_MTK_GPIO
void __weak gpio_dump_regs_range(int start, int end)
{
	pr_info(DFT_TAG "[W]%s: is not define!\n", __func__);
}
#endif
#ifndef CONFIG_MTK_GPIO
void connectivity_export_dump_gpio_info(int start, int end)
{
	gpio_dump_regs_range(start, end);
}
EXPORT_SYMBOL(connectivity_export_dump_gpio_info);
#endif

void connectivity_export_dump_thread_state(const char *name)
{
#ifdef CFG_CONNADP_BUILD_IN
	static const char stat_nam[] = TASK_STATE_TO_CHAR_STR;
	struct task_struct *p;
	int cpu;
	struct rq *rq;
	struct task_struct *curr;
	struct thread_info *ti;

	if (name == NULL || strlen(name) > 255) {
		pr_info("invalid name:%p or thread name too long\n", name);
		return;
	}

	pr_info("start to show debug info of %s\n", name);

	rcu_read_lock();
	for_each_process(p) {
		unsigned long state;

		if (strncmp(p->comm, name, strlen(name)) != 0)
			continue;
		state = p->state;
		cpu = task_cpu(p);
		rq = cpu_rq(cpu);
		curr = rq->curr;
		ti = task_thread_info(curr);
		if (state)
			state = __ffs(state) + 1;
		pr_info("%d:%-15.15s %c", p->pid, p->comm,
			state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
		pr_info("cpu=%d on_cpu=%d ", cpu, p->on_cpu);
		show_stack(p, NULL, KERN_DEFAULT);
		pr_info("CPU%d curr=%d:%-15.15s preempt_count=0x%x", cpu,
			curr->pid, curr->comm, ti->preempt_count);

		if (state == TASK_RUNNING && curr != p)
			show_stack(curr, NULL, KERN_DEFAULT);

		break;
	}
	rcu_read_unlock();

#else
	pr_info("%s not support in connadp.ko\n", __func__);
#endif
}
EXPORT_SYMBOL(connectivity_export_dump_thread_state);

int connectivity_export_gpio_get_tristate_input(unsigned int pin)
{
	return 0;
}
EXPORT_SYMBOL(connectivity_export_gpio_get_tristate_input);

MODULE_LICENSE("GPL");
