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

#include <linux/cpu_pm.h>
#include <linux/irqchip/mtk-gic.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/psci.h>
#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/cpuidle.h>

#include <asm/cpuidle.h>

#include <mt-plat/mtk_secure_api.h>
#include <mtk_spm.h>
#include <mtk_cpuidle.h>

#include "mtk_cpuidle_private.h"

static ulong dbg_data[40];
static int mtk_cpuidle_initialized;

#ifdef CPUIDLE_PROFILE

#define PROFILE_EACH_CPU_ONCE

#include <asm/arch_timer.h>
#include <mach/mtk_cpufreq_api.h>

static void *ts_pool;
#define percpu_ts(cpu) ((u64 *)(ts_pool + ((cpu * NR_CPUIDLE_TS) << 3)))

static struct cpuidle_perf_time cpuidle_perf[CONFIG_NR_CPUS][NR_CPUIDLE_MODE];

static void cpuidle_ts_init(void)
{
	int rc;
	void *p;
	dma_addr_t atf_addr;
	struct device *cpu_dev;

	int cpu = smp_processor_id();

	pr_debug("%s@%d/%d: enter\n",  __func__, __LINE__, cpu);

	cpu_dev = get_cpu_device(cpu);
	arch_setup_dma_ops(cpu_dev, 0, 0, NULL, false);

	p = dma_zalloc_coherent(cpu_dev, PAGE_SIZE, &atf_addr, GFP_KERNEL);
	WARN_ON(!p);

	rc = mt_secure_call(MTK_SIP_POWER_FLOW_DEBUG, 0, 1, atf_addr, 0);
	WARN_ON(rc);

	ts_pool = p;
}

static inline void cpuidle_ts(int cpu, int hotspot)
{
	u64 *p = percpu_ts(cpu);

	p[hotspot] = arch_counter_get_cntvct();
}

static unsigned int cpu_get_freq(int cpu)
{
	/* there is only one cluster in mt6739 */
	return mt_cpufreq_get_cur_freq(0);
}

static void cpuidle_perf_print(int cpu, int mode)
{
	struct cpuidle_perf_time *percpu_perf;
	unsigned int cpufreq, count;
	u64 *cpu_ts = percpu_ts(cpu);

	if (mode == MTK_MCDI_CLUSTER_MODE &&
	    !cpu_ts[CPUIDLE_TS_AFTER_L2_FLUSH])
		return;

	/* update the time spent of each hotspot to cpuidle_perf_time */
#define __time(v, begin, end) (percpu_perf->v += (cpu_ts[end] - cpu_ts[begin]))

	percpu_perf = &cpuidle_perf[cpu][mode];
	percpu_perf->count++;

	__time(kernel_plat_backup,
	       CPUIDLE_TS_ENTER_CPUIDLE, CPUIDLE_TS_BEFORE_ATF);
	__time(kernel_to_atf,
	       CPUIDLE_TS_BEFORE_ATF, CPUIDLE_TS_ENTER_ATF);
	__time(atf_l2_flush,
	       CPUIDLE_TS_BEFORE_L2_FLUSH, CPUIDLE_TS_AFTER_L2_FLUSH);
	__time(atf_spm_suspend,
	       CPUIDLE_TS_ENTER_SPM_SUSPEND, CPUIDLE_TS_LEAVE_SPM_SUSPEND);
	__time(atf_gic_backup,
	       CPUIDLE_TS_GIC_P1, CPUIDLE_TS_GIC_P2);
	__time(atf_plat_backup,
	       CPUIDLE_TS_LEAVE_SPM_SUSPEND, CPUIDLE_TS_GIC_P1);

	 /*
	  * the time spent by atf_setup was accumulated with multiple
	  * sectuions
	  */
	__time(atf_setup,
	       CPUIDLE_TS_ENTER_ATF, CPUIDLE_TS_BEFORE_L2_FLUSH);
	__time(atf_setup,
	       CPUIDLE_TS_AFTER_L2_FLUSH, CPUIDLE_TS_ENTER_SPM_SUSPEND);
	__time(atf_setup,
	       CPUIDLE_TS_GIC_P2, CPUIDLE_TS_BEFORE_WFI);

	__time(atf_cpu_init,
	       CPUIDLE_TS_AFTER_WFI, CPUIDLE_TS_GIC_P3);
	__time(atf_gic_restore,
	       CPUIDLE_TS_GIC_P3, CPUIDLE_TS_GIC_P4);
	__time(atf_spm_suspend_finish,
	       CPUIDLE_TS_ENTER_SPM_SUSPEND_FINISH,
	       CPUIDLE_TS_LEAVE_SPM_SUSPEND_FINISH);

	__time(atf_plat_restore,
	       CPUIDLE_TS_GIC_P4, CPUIDLE_TS_ENTER_SPM_SUSPEND_FINISH);
	__time(atf_to_kernel,
	       CPUIDLE_TS_LEAVE_SPM_SUSPEND_FINISH, CPUIDLE_TS_AFTER_ATF);
	__time(kernel_plat_restore,
	       CPUIDLE_TS_AFTER_ATF, CPUIDLE_TS_LEAVE_CPUIDLE);

	__time(k2atf, CPUIDLE_TS_ENTER_CPUIDLE, CPUIDLE_TS_BEFORE_ATF);
	__time(atf2wfi, CPUIDLE_TS_ENTER_ATF, CPUIDLE_TS_BEFORE_WFI);
	__time(wfi2k, CPUIDLE_TS_AFTER_WFI, CPUIDLE_TS_LEAVE_ATF);
	__time(k2leave, CPUIDLE_TS_AFTER_ATF, CPUIDLE_TS_LEAVE_CPUIDLE);

#undef __time

	/*
	 * if the numbers of this idle mode executed by this core were
	 * over 1000, print out the result then reset it.
	 */

	count = percpu_perf->count;
	cpufreq =  cpu_get_freq(cpu);

	if (count < 1000) {
		pr_debug("count:%d ,CPU,%d,CPU Freq,%d, idlemode:%d\n",
			 count, cpu, cpufreq, mode);
		return;
	} else if (count > 1000)
		return;

#ifdef PROFILE_EACH_CPU_ONCE
	/*
	 * set it up over 1000 to make this mode/cpu not show up again
	 */
	percpu_perf->count = 1100;
#endif

	request_uart_to_wakeup();

	pr_debug("======== MTK_CPUIDLE Time Profiling Start ========\n");
	pr_debug(",CPU,%d,CPU Freq,%d, idlemode:%d\n", cpu, cpufreq, mode);

	pr_debug(",Kernel Platform Backup,%u\n",
		 percpu_perf->kernel_plat_backup / count);
	pr_debug(",Kernel to ATF,%u\n", percpu_perf->kernel_to_atf / count);
	pr_debug(",ATF Setup,%u\n", percpu_perf->atf_setup / count);
	pr_debug(",ATF L2 Flush,%u\n", percpu_perf->atf_l2_flush / count);
	pr_debug(",ATF SPM Suspend,%u\n", percpu_perf->atf_spm_suspend / count);
	pr_debug(",ATF GIC Backup,%u\n", percpu_perf->atf_gic_backup / count);
	pr_debug(",ATF Platform Backup,%u\n",
		 percpu_perf->atf_plat_backup / count);

	pr_debug("ATF CPU Init,%u\n", percpu_perf->atf_cpu_init / count);
	pr_debug("ATF GIC Restore,%u\n", percpu_perf->atf_gic_restore / count);
	pr_debug("ATF SPM Suspend Finish,%u\n",
		 percpu_perf->atf_spm_suspend_finish / count);
	pr_debug("ATF Platform Restore,%u\n",
		 percpu_perf->atf_plat_restore / count);
	pr_debug("ATF to Kernel,%u\n", percpu_perf->atf_to_kernel / count);
	pr_debug("Kernel Platform Restore,%u\n",
		 percpu_perf->kernel_plat_restore / count);

	pr_debug("Kernel to ATF before,%u\n", percpu_perf->k2atf / count);
	pr_debug("ATF to wfi before,%u\n", percpu_perf->atf2wfi / count);
	pr_debug("wfi to kernel before,%u\n", percpu_perf->wfi2k / count);
	pr_debug("kernel to leave,%u\n", percpu_perf->k2leave / count);
	pr_debug("======== MTK_CPUIDLE Percpu_Perf Profiling Done ========\n");

#ifndef PROFILE_EACH_CPU_ONCE
	memset(percpu_perf, 0, sizeof(*percpu_perf));
#endif
}

#else

#define cpuidle_ts_init()
#define cpuidle_ts(cpu, hotspot)
#define cpuidle_perf_print(cpu, mode)

#endif /* CPUIDLE_PROFILE */


#ifdef CONFIG_MTK_RAM_CONSOLE

static u32 *cpuidle_fp_pa;
static u32 *cpuidle_fp_va;

static void cpuidle_fp_init(void)
{
	cpuidle_fp_va = (u32 *) aee_rr_rec_mtk_cpuidle_footprint_va();
	cpuidle_fp_pa = (u32 *) aee_rr_rec_mtk_cpuidle_footprint_pa();

	if (cpuidle_fp_va && cpuidle_fp_pa) {
		mt_secure_call(MTK_SIP_POWER_FLOW_DEBUG,
			       0, 2, (ulong) cpuidle_fp_pa, 0);
		return;
	}

	WARN(1, "Invalid footprint address va(%p), pa(%p)\n",
	     cpuidle_fp_va, cpuidle_fp_pa);
}

static inline void cpuidle_fp(int cpu, int checkpoint)
{
	cpuidle_fp_va[cpu] |= (1 << checkpoint);
}

static inline void cpuidle_fp_reset(int cpu)
{
	cpuidle_fp_va[cpu] = 0;
}

#else

#define cpuidle_fp_init()
#define cpuidle_fp(cpu, checkpoint)
#define cpuidle_fp_reset(cpu)

#endif /* CONFIG_MTK_RAM_CONSOLE */

static void mtk_platform_save(int cpu)
{
	mt_save_dbg_regs(dbg_data, cpu);
	dpm_mcsi_mtcmos_on_flow(0);
}

static void mtk_platform_restore(int cpu)
{
	dpm_mcsi_mtcmos_on_flow(1);
	mt_restore_dbg_regs(dbg_data, cpu);
}

int mtk_enter_idle_state(int mode)
{
	int cpu, ret;

	if (!mtk_cpuidle_initialized)
		return -EOPNOTSUPP;

	cpu = smp_processor_id();

	cpuidle_fp(cpu, CPUIDLE_FP_ENTER_CPUIDLE);
	cpuidle_ts(cpu, CPUIDLE_TS_ENTER_CPUIDLE);

	ret = cpu_pm_enter();
	if (!ret) {
		mtk_platform_save(cpu);

		cpuidle_fp(cpu, CPUIDLE_FP_BEFORE_ATF);
		cpuidle_ts(cpu, CPUIDLE_TS_BEFORE_ATF);

		/*
		 * Pass idle state index to cpu_suspend which in turn will
		 * call the CPU ops suspend protocol with idle index as a
		 * parameter.
		 */
		ret = arm_cpuidle_suspend(mode);

		cpuidle_fp(cpu, CPUIDLE_FP_AFTER_ATF);
		cpuidle_ts(cpu, CPUIDLE_TS_AFTER_ATF);

		mtk_platform_restore(cpu);

		cpu_pm_exit();

		cpuidle_fp_reset(cpu);
		cpuidle_ts(cpu, CPUIDLE_TS_LEAVE_CPUIDLE);

		cpuidle_perf_print(cpu, mode);
	}

	return ret ? -1 : mode;
}

int mtk_cpuidle_init(void)
{
	if (mtk_cpuidle_initialized == 1)
		return 0;

	/* cpuidle footprint init */
	cpuidle_fp_init();

	/* cpuidle timestamp init */
	cpuidle_ts_init();

	mtk_cpuidle_initialized = 1;

	return 0;
}
