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

#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/irqchip/mtk-gic.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/psci.h>

#include <asm/arch_timer.h>
#include <asm/cacheflush.h>
#include <asm/cpuidle.h>
#include <asm/irqflags.h>
#include <asm/neon.h>
#include <asm/suspend.h>

#include <mt-plat/mtk_dbg.h>
#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>

#include <mtk_cpuidle.h>
#if defined(CONFIG_MTK_RAM_CONSOLE) || defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
#include <mt-plat/mtk_secure_api.h>
#endif
#include <mtk_spm.h>
#include <mtk_spm_misc.h>

#ifdef CONFIG_MTK_RAM_CONSOLE
static void __iomem *mtk_cpuidle_aee_phys_addr;
static void __iomem *mtk_cpuidle_aee_virt_addr;
#endif

#if MTK_CPUIDLE_TIME_PROFILING
static u64 mtk_cpuidle_timestamp[CONFIG_NR_CPUS][MTK_CPUIDLE_TIMESTAMP_COUNT];
static struct mtk_cpuidle_time_profile report[CONFIG_NR_CPUS];
static char mtk_cpuidle_timestamp_buf[1024] = { 0 };
#endif

#define CPU_IDLE_STA_OFFSET 10

static unsigned long dbg_data[40];
static int mtk_cpuidle_initialized;

static void mtk_spm_wakeup_src_restore(void)
{
	int i;

	for (i = 0; i < IRQ_NR_MAX; i++) {
		if (readl_relaxed(SPM_SW_RSV_0) & wake_src_irq[i])
			mt_irq_set_pending(irq_nr[i]);
	}
}

static void mtk_cpuidle_timestamp_init(void)
{
#if MTK_CPUIDLE_TIME_PROFILING
	kernel_smc_msg(0, 1, virt_to_phys(mtk_cpuidle_timestamp));
#endif
}

static void mtk_cpuidle_timestamp_report(int cpu)
{
#if MTK_CPUIDLE_TIME_PROFILING
	int i;
	char *p;

	request_uart_to_wakeup();

	p = mtk_cpuidle_timestamp_buf;

	p += sprintf(p, "CPU%d", cpu);
	for (i = 0; i < MTK_CPUIDLE_TIMESTAMP_COUNT; i++)
		p += sprintf(p, ",%llu", mtk_cpuidle_timestamp[cpu][i]);

	pr_debug("%s\n", mtk_cpuidle_timestamp_buf);

	report[cpu].count++;

	report[cpu].kernel_plat_backup += (mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_BEFORE_ATF] -
		mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_ENTER_CPUIDLE]);
	report[cpu].kernel_to_atf += (mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_ENTER_ATF] -
		mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_BEFORE_ATF]);
	report[cpu].atf_l2_flush += (mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_AFTER_L2_FLUSH] -
		mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_BEFORE_L2_FLUSH]);
	report[cpu].atf_spm_suspend += (mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_LEAVE_SPM_SUSPEND] -
		mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_ENTER_SPM_SUSPEND]);
	report[cpu].atf_gic_backup +=
		(mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_GIC_P2] -
		mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_GIC_P1]);
	report[cpu].atf_plat_backup +=
		(mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_GIC_P1] -
		mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_LEAVE_SPM_SUSPEND]);
	report[cpu].atf_setup += (mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_BEFORE_L2_FLUSH] -
		mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_ENTER_ATF]) +
		(mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_ENTER_SPM_SUSPEND] -
		mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_AFTER_L2_FLUSH]) +
		(mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_BEFORE_WFI] -
		mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_GIC_P2]);

	report[cpu].atf_cpu_init +=
		(mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_GIC_P3] -
		mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_AFTER_WFI]);
	report[cpu].atf_gic_restore +=
		(mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_GIC_P4] -
		mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_GIC_P3]);
	report[cpu].atf_spm_suspend_finish += (mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_LEAVE_SPM_SUSPEND_FINISH] -
		mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_ENTER_SPM_SUSPEND_FINISH]);
	report[cpu].atf_plat_restore += (mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_ENTER_SPM_SUSPEND_FINISH] -
		mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_GIC_P4]);
	report[cpu].atf_to_kernel +=
		(mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_AFTER_ATF] -
		mtk_cpuidle_timestamp[cpu]
		[MTK_SUSPEND_TIMESTAMP_LEAVE_SPM_SUSPEND_FINISH]);
	report[cpu].kernel_plat_restore +=
		(mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_LEAVE_CPUIDLE]
		- mtk_cpuidle_timestamp[cpu][MTK_SUSPEND_TIMESTAMP_AFTER_ATF]);

	if (report[cpu].count == 1000) {
		pr_info("======== MTK_CPUIDLE Time Profiling Start ========\n");
		pr_info(",CPU,%d,CPU Freq,%d\n",
			cpu, mt_cpufreq_get_cur_freq(cpu >> 2));
		pr_info(",Kernel Platform Backup,%u\n",
			report[cpu].kernel_plat_backup / report[cpu].count);
		pr_info(",Kernel to ATF,%u\n",
			report[cpu].kernel_to_atf / report[cpu].count);
		pr_info(",ATF Setup,%u\n",
			report[cpu].atf_setup / report[cpu].count);
		pr_info(",ATF L2 Flush,%u\n",
			report[cpu].atf_l2_flush / report[cpu].count);
		pr_info(",ATF SPM Suspend,%u\n",
		report[cpu].atf_spm_suspend / report[cpu].count);
		pr_info(",ATF GIC Backup,%u\n",
		report[cpu].atf_gic_backup / report[cpu].count);
		pr_info(",ATF Platform Backup,%u\n",
		report[cpu].atf_plat_backup / report[cpu].count);
		pr_info("ATF CPU Init,%u\n",
			report[cpu].atf_cpu_init / report[cpu].count);
		pr_info("ATF GIC Restore,%u\n",
			report[cpu].atf_gic_restore / report[cpu].count);
		pr_info("ATF SPM Suspend Finish,%u\n",
			report[cpu].atf_spm_suspend_finish / report[cpu].count);
		pr_info("ATF Platform Restore,%u\n",
			report[cpu].atf_plat_restore / report[cpu].count);
		pr_info("ATF to Kernel,%u\n",
			report[cpu].atf_to_kernel / report[cpu].count);
		pr_info("Kernel Platform Restore,%u\n",
			report[cpu].kernel_plat_restore / report[cpu].count);
		pr_info("======== MTK_CPUIDLE Time Profiling Done ========\n");

		report[cpu].count = 0;
		report[cpu].kernel_plat_backup = 0;
		report[cpu].kernel_to_atf = 0;
		report[cpu].atf_setup = 0;
		report[cpu].atf_l2_flush = 0;
		report[cpu].atf_plat_backup = 0;
		report[cpu].atf_spm_suspend = 0;
		report[cpu].atf_gic_backup = 0;
		report[cpu].atf_cpu_init = 0;
		report[cpu].atf_spm_suspend_finish = 0;
		report[cpu].atf_gic_restore = 0;
		report[cpu].atf_plat_restore = 0;
		report[cpu].atf_to_kernel = 0;
		report[cpu].kernel_plat_restore = 0;
	}
#endif
}

static void mtk_cpuidle_ram_console_init(void)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	mtk_cpuidle_aee_virt_addr = aee_rr_rec_mtk_cpuidle_footprint_va();
	mtk_cpuidle_aee_phys_addr = aee_rr_rec_mtk_cpuidle_footprint_pa();

	WARN_ON(!mtk_cpuidle_aee_virt_addr || !mtk_cpuidle_aee_phys_addr);

	kernel_smc_msg(0, 2, (long)mtk_cpuidle_aee_phys_addr);
#endif
}

static u32 get_dts_node_irq_nr(const char *irq_match, int index)
{
	struct device_node *node;
	unsigned int irq_nr;

	node = of_find_compatible_node(NULL, NULL, irq_match);
	if (!node)
		pr_info("error: cannot find node [%s]\n", irq_match);

	irq_nr = irq_of_parse_and_map(node, index);
	if (!irq_nr)
		pr_info("error: cannot property_read [%s]\n", irq_match);

	of_node_put(node);
	pr_debug("compatible = %s, irq_nr = %u\n", irq_match, irq_nr);

	return irq_nr;
}

static void mtk_cpuidle_dts_map(void)
{
	int i;

	for (i = 0; i < IRQ_NR_MAX; i++)
		irq_nr[i] = get_dts_node_irq_nr(irq_match[i], irq_offset[i]);
}

static void mtk_switch_armpll(int cpu, int hw_mode)
{
	if (cpu < 4)
		switch_armpll_ll_hwmode(hw_mode);
	else if (cpu < 8)
		switch_armpll_l_hwmode(hw_mode);
}

#if 0
static void mtk_dbg_save_restore(int cpu, int save)
{
	unsigned int cpu_idle_sta;
	int nr_cpu_bit = (1 << CONFIG_NR_CPUS) - 1;

	cpu_idle_sta =
	    (spm_read(CPU_IDLE_STA) >> CPU_IDLE_STA_OFFSET) | (1 << cpu);

	if ((cpu_idle_sta & nr_cpu_bit) == nr_cpu_bit) {
		if (save)
			mt_save_dbg_regs(dbg_data, cpu);
		else
			mt_restore_dbg_regs(dbg_data, cpu);
	} else {
		if (!save)
			mt_copy_dbg_regs(cpu, __builtin_ffs(~cpu_idle_sta) - 1);
	}
}
#endif

static void mtk_dbg_save_restore(int cpu, int save)
{
	if (save)
		mt_save_dbg_regs(dbg_data, cpu);
	else
		mt_restore_dbg_regs(dbg_data, cpu);
}

static void mtk_platform_save_context(int cpu, int idx)
{
	mtk_switch_armpll(cpu, 1);
	mtk_dbg_save_restore(cpu, 1);
	dpm_mcsi_mtcmos_on_flow(0);
}

static void mtk_platform_restore_context(int cpu, int idx)
{
	if (idx > MTK_MCDI_CLUSTER_MODE)
		mtk_spm_wakeup_src_restore();

	mtk_dbg_save_restore(cpu, 0);
	mtk_switch_armpll(cpu, 0);
	dpm_mcsi_mtcmos_on_flow(1);
}

int mtk_enter_idle_state(int idx)
{
	int cpu, ret;

	if (!mtk_cpuidle_initialized)
		return -EOPNOTSUPP;

	cpu = smp_processor_id();

	mtk_cpuidle_footprint_log(cpu, MTK_SUSPEND_FOOTPRINT_ENTER_CPUIDLE);
	mtk_cpuidle_timestamp_log(cpu, MTK_SUSPEND_TIMESTAMP_ENTER_CPUIDLE);
	ret = cpu_pm_enter();
	if (!ret) {
		mtk_platform_save_context(cpu, idx);

		mtk_cpuidle_footprint_log(cpu,
			MTK_SUSPEND_FOOTPRINT_BEFORE_ATF);
		mtk_cpuidle_timestamp_log(cpu,
			MTK_SUSPEND_TIMESTAMP_BEFORE_ATF);
		/*
		 * Pass idle state index to cpu_suspend which in turn will
		 * call the CPU ops suspend protocol with idle index as a
		 * parameter.
		 */
		ret = arm_cpuidle_suspend(idx);
		mtk_cpuidle_footprint_log(cpu, MTK_SUSPEND_FOOTPRINT_AFTER_ATF);
		mtk_cpuidle_timestamp_log(cpu, MTK_SUSPEND_TIMESTAMP_AFTER_ATF);

		mtk_platform_restore_context(cpu, idx);

		cpu_pm_exit();

		mtk_cpuidle_footprint_clr(cpu);
		mtk_cpuidle_timestamp_log(cpu,
			MTK_SUSPEND_TIMESTAMP_LEAVE_CPUIDLE);

		mtk_cpuidle_timestamp_report(cpu);
	}

	return ret ? -1 : idx;
}

int mtk_cpuidle_init(void)
{
	if (mtk_cpuidle_initialized == 1)
		return 0;

	mtk_cpuidle_dts_map();

	mtk_cpuidle_timestamp_init();

	mtk_cpuidle_ram_console_init();

	mtk_cpuidle_initialized = 1;

	return 0;
}
