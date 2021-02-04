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

#include <linux/interrupt.h>
#include <pinctrl-mtk-common.h>

#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
#include <mtk_6306_gpio.h>
#endif

#ifdef CONNADP_HAS_CLOCK_BUF_CTRL
#include <mtk_clkbuf_ctl.h>
#endif

/* PMIC */
#if defined(CONFIG_MTK_PMIC_CHIP_MT6359)
#include <mtk_pmic_api_buck.h>
#endif
#include <upmu_common.h>

/* MMC */
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <sdio_ops.h>

#include "mtk_spm_resource_req.h"
#include <mtk_sleep.h>

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

phys_addr_t gConEmiPhyBase;
EXPORT_SYMBOL(gConEmiPhyBase);
unsigned long long gConEmiSize;
EXPORT_SYMBOL(gConEmiSize);

phys_addr_t gWifiRsvMemPhyBase;
EXPORT_SYMBOL(gWifiRsvMemPhyBase);
unsigned long long gWifiRsvMemSize;
EXPORT_SYMBOL(gWifiRsvMemSize);

/*Reserved memory by device tree!*/

int reserve_memory_consys_fn(struct reserved_mem *rmem)
{
	pr_info(DFT_TAG "[W]%s: name: %s,base: 0x%llx,size: 0x%llx\n",
		__func__, rmem->name, (unsigned long long)rmem->base,
		(unsigned long long)rmem->size);
	gConEmiPhyBase = rmem->base;
	gConEmiSize = rmem->size;
	return 0;
}

RESERVEDMEM_OF_DECLARE(reserve_memory_test, "mediatek,consys-reserve-memory",
			reserve_memory_consys_fn);

int reserve_memory_wifi_fn(struct reserved_mem *rmem)
{
	pr_info(DFT_TAG "[W]%s: name: %s,base: 0x%llx,size: 0x%llx\n",
		__func__, rmem->name, (unsigned long long)rmem->base,
		(unsigned long long)rmem->size);
	gWifiRsvMemPhyBase = rmem->base;
	gWifiRsvMemSize = rmem->size;
	return 0;
}
RESERVEDMEM_OF_DECLARE(reserve_memory_wifi, "mediatek,wifi-reserve-memory",
		       reserve_memory_wifi_fn);

void connectivity_export_show_stack(struct task_struct *tsk, unsigned long *sp)
{
	show_stack(tsk, sp);
}
EXPORT_SYMBOL(connectivity_export_show_stack);

void connectivity_export_tracing_record_cmdline(struct task_struct *tsk)
{
	tracing_record_cmdline(tsk);
}
EXPORT_SYMBOL(connectivity_export_tracing_record_cmdline);

unsigned int connectivity_export_slp_get_wake_reason(void)
{
	return slp_get_wake_reason();
}
EXPORT_SYMBOL(connectivity_export_slp_get_wake_reason);

unsigned int connectivity_export_spm_get_last_wakeup_src(void)
{
	return spm_get_last_wakeup_src();
}
EXPORT_SYMBOL(connectivity_export_spm_get_last_wakeup_src);

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

bool connectivity_export_is_clk_buf_from_pmic(void)
{
	return is_clk_buf_from_pmic();
}
EXPORT_SYMBOL(connectivity_export_is_clk_buf_from_pmic);

void connectivity_export_clk_buf_show_status_info(void)
{
#if defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT6779)
	clk_buf_show_status_info();
#endif
}
EXPORT_SYMBOL(connectivity_export_clk_buf_show_status_info);

int connectivity_export_clk_buf_get_xo_en_sta(/*enum xo_id id*/ int id)
{
#if defined(CONFIG_MACH_MT6765) || \
	defined(CONFIG_MACH_MT6761) || \
	defined(CONFIG_MACH_MT6779)
	return clk_buf_get_xo_en_sta(id);
#else
	return KERNEL_CLK_BUF_CHIP_NOT_SUPPORT;
#endif
}
EXPORT_SYMBOL(connectivity_export_clk_buf_get_xo_en_sta);
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
 * PMIC
 ******************************************************************************/
#ifdef CONNADP_HAS_PMIC_API
void connectivity_export_pmic_config_interface(unsigned int RegNum,
		unsigned int val, unsigned int MASK, unsigned int SHIFT)
{
	pmic_config_interface(RegNum, val, MASK, SHIFT);
}
EXPORT_SYMBOL(connectivity_export_pmic_config_interface);

void connectivity_export_pmic_read_interface(unsigned int RegNum,
		unsigned int *val, unsigned int MASK, unsigned int SHIFT)
{
	pmic_read_interface(RegNum, val, MASK, SHIFT);
}
EXPORT_SYMBOL(connectivity_export_pmic_read_interface);

void connectivity_export_pmic_set_register_value(int flagname, unsigned int val)
{
	pmic_set_register_value(flagname, val);
}
EXPORT_SYMBOL(connectivity_export_pmic_set_register_value);

unsigned short connectivity_export_pmic_get_register_value(int flagname)
{
	return pmic_get_register_value(flagname);
}
EXPORT_SYMBOL(connectivity_export_pmic_get_register_value);

void connectivity_export_upmu_set_reg_value(unsigned int reg,
		unsigned int reg_val)
{
	upmu_set_reg_value(reg, reg_val);
}
EXPORT_SYMBOL(connectivity_export_upmu_set_reg_value);

#if defined(CONFIG_MTK_PMIC_CHIP_MT6359)
int connectivity_export_pmic_ldo_vcn13_lp(int user,
		int op_mode, unsigned char op_en, unsigned char op_cfg)
{
	return pmic_ldo_vcn13_lp(user, op_mode, op_en, op_cfg);
}
EXPORT_SYMBOL(connectivity_export_pmic_ldo_vcn13_lp);

int connectivity_export_pmic_ldo_vcn18_lp(int user,
		int op_mode, unsigned char op_en, unsigned char op_cfg)
{
	return pmic_ldo_vcn18_lp(user, op_mode, op_en, op_cfg);
}
EXPORT_SYMBOL(connectivity_export_pmic_ldo_vcn18_lp);

int connectivity_export_pmic_ldo_vcn33_1_lp(int user,
		int op_mode, unsigned char op_en, unsigned char op_cfg)
{
	return pmic_ldo_vcn33_1_lp(user, op_mode, op_en, op_cfg);
}
EXPORT_SYMBOL(connectivity_export_pmic_ldo_vcn33_1_lp);

int connectivity_export_pmic_ldo_vcn33_2_lp(int user,
		int op_mode, unsigned char op_en, unsigned char op_cfg)
{
	return pmic_ldo_vcn33_2_lp(user, op_mode, op_en, op_cfg);
}
EXPORT_SYMBOL(connectivity_export_pmic_ldo_vcn33_2_lp);
#endif
#endif
#ifdef CONNADP_HAS_UPMU_VCN_CTRL
void connectivity_export_upmu_set_vcn_1v8_lp_mode_set(unsigned int val)
{
	upmu_set_vcn_1v8_lp_mode_set(val);
}
EXPORT_SYMBOL(connectivity_export_upmu_set_vcn_1v8_lp_mode_set);

void connectivity_export_upmu_set_vcn28_on_ctrl(unsigned int val)
{
	upmu_set_vcn28_on_ctrl(val);
}
EXPORT_SYMBOL(connectivity_export_upmu_set_vcn28_on_ctrl);

void connectivity_export_upmu_set_vcn33_on_ctrl_bt(unsigned int val)
{
	upmu_set_vcn33_on_ctrl_bt(val);
}
EXPORT_SYMBOL(connectivity_export_upmu_set_vcn33_on_ctrl_bt);

void connectivity_export_upmu_set_vcn33_on_ctrl_wifi(unsigned int val)
{
	upmu_set_vcn33_on_ctrl_wifi(val);
}
EXPORT_SYMBOL(connectivity_export_upmu_set_vcn33_on_ctrl_wifi);
#endif
/*******************************************************************************
 * MMC
 ******************************************************************************/
int connectivity_export_mmc_io_rw_direct(struct mmc_card *card,
				int write, unsigned int fn,
				unsigned int addr, u8 in, u8 *out)
{
	return mmc_io_rw_direct(card, write, fn, addr, in, out);
}
EXPORT_SYMBOL(connectivity_export_mmc_io_rw_direct);

void connectivity_flush_dcache_area(void *addr, size_t len)
{
#ifdef CONFIG_ARM64
	__flush_dcache_area(addr, len);
#else
	v7_flush_kern_dcache_area(addr, len);
#endif
}
EXPORT_SYMBOL(connectivity_flush_dcache_area);

void connectivity_arch_setup_dma_ops(struct device *dev, u64 dma_base, u64 size,
				     struct iommu_ops *iommu, bool coherent)
{
	arch_setup_dma_ops(dev, dma_base, size, iommu, coherent);
}
EXPORT_SYMBOL(connectivity_arch_setup_dma_ops);

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
		show_stack(p, NULL);
		pr_info("CPU%d curr=%d:%-15.15s preempt_count=0x%x", cpu,
			curr->pid, curr->comm, ti->preempt_count);

		if (state == TASK_RUNNING && curr != p)
			show_stack(curr, NULL);

		break;
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL(connectivity_export_dump_thread_state);

int connectivity_export_gpio_get_tristate_input(unsigned int pin)
{
	return gpio_get_tristate_input(pin);
}
EXPORT_SYMBOL(connectivity_export_gpio_get_tristate_input);
