/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/smp.h>
#include <linux/tick.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/cpu_pm.h>
#include <linux/msm-bus.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <soc/qcom/avs.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/scm-boot.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>
#ifdef CONFIG_VFP
#include <asm/vfp.h>
#endif
#include <soc/qcom/jtag.h>
#include "idle.h"
#include "pm-boot.h"
#include "../../../arch/arm/mach-msm/clock.h"

#define SCM_CMD_TERMINATE_PC	(0x2)
#define SCM_CMD_CORE_HOTPLUGGED (0x10)
#define SCM_FLUSH_FLAG_MASK	(0x3)

#define SCLK_HZ (32768)

#define MAX_BUF_SIZE  1024

static int msm_pm_debug_mask = 1;
module_param_named(
	debug_mask, msm_pm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

enum {
	MSM_PM_DEBUG_SUSPEND = BIT(0),
	MSM_PM_DEBUG_POWER_COLLAPSE = BIT(1),
	MSM_PM_DEBUG_SUSPEND_LIMITS = BIT(2),
	MSM_PM_DEBUG_CLOCK = BIT(3),
	MSM_PM_DEBUG_RESET_VECTOR = BIT(4),
	MSM_PM_DEBUG_IDLE_CLK = BIT(5),
	MSM_PM_DEBUG_IDLE = BIT(6),
	MSM_PM_DEBUG_IDLE_LIMITS = BIT(7),
	MSM_PM_DEBUG_HOTPLUG = BIT(8),
};

enum msm_pc_count_offsets {
	MSM_PC_ENTRY_COUNTER,
	MSM_PC_EXIT_COUNTER,
	MSM_PC_FALLTHRU_COUNTER,
	MSM_PC_UNUSED,
	MSM_PC_NUM_COUNTERS,
};

static bool msm_pm_ldo_retention_enabled = true;
static bool msm_no_ramp_down_pc;
static struct msm_pm_sleep_status_data *msm_pm_slp_sts;
DEFINE_PER_CPU(struct clk *, cpu_clks);
static struct clk *l2_clk;

static long *msm_pc_debug_counters;

static cpumask_t retention_cpus;
static DEFINE_SPINLOCK(retention_lock);

static inline void msm_arch_idle(void)
{
	mb();
	wfi();
}

static bool msm_pm_is_L1_writeback(void)
{
	u32 cache_id = 0;

#if defined(CONFIG_CPU_V7)
	u32 sel = 0;
	asm volatile ("mcr p15, 2, %[ccselr], c0, c0, 0\n\t"
		      "isb\n\t"
		      "mrc p15, 1, %[ccsidr], c0, c0, 0\n\t"
		      :[ccsidr]"=r" (cache_id)
		      :[ccselr]"r" (sel)
		     );
	return cache_id & BIT(30);
#elif defined(CONFIG_ARM64)
	u32 sel = 0;
	asm volatile("msr csselr_el1, %[ccselr]\n\t"
		     "isb\n\t"
		     "mrs %[ccsidr],ccsidr_el1\n\t"
		     :[ccsidr]"=r" (cache_id)
		     :[ccselr]"r" (sel)
		    );
	return cache_id & BIT(30);
#else
#error No valid CPU arch selected
#endif
}

static bool msm_pm_swfi(bool from_idle)
{
	msm_arch_idle();
	return true;
}

static bool msm_pm_retention(bool from_idle)
{
	int ret = 0;
	unsigned int cpu = smp_processor_id();
	struct clk *cpu_clk = per_cpu(cpu_clks, cpu);

	spin_lock(&retention_lock);

	if (!msm_pm_ldo_retention_enabled)
		goto bailout;

	cpumask_set_cpu(cpu, &retention_cpus);
	spin_unlock(&retention_lock);

	clk_disable(cpu_clk);

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_RETENTION, false);
	WARN_ON(ret);

	msm_arch_idle();

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);
	WARN_ON(ret);

	if (clk_enable(cpu_clk))
		pr_err("%s(): Error restore cpu clk\n", __func__);

	spin_lock(&retention_lock);
	cpumask_clear_cpu(cpu, &retention_cpus);
bailout:
	spin_unlock(&retention_lock);
	return true;
}

static inline void msm_pc_inc_debug_count(uint32_t cpu,
		enum msm_pc_count_offsets offset)
{
	int cntr_offset;
	uint32_t cluster_id = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
	uint32_t cpu_id = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 0);

	if (cluster_id >= MAX_NUM_CLUSTER || cpu_id >= MAX_CPUS_PER_CLUSTER)
		BUG();

	cntr_offset = (cluster_id * MAX_CPUS_PER_CLUSTER * MSM_PC_NUM_COUNTERS)
			 + (cpu_id * MSM_PC_NUM_COUNTERS) + offset;

	if (!msm_pc_debug_counters)
		return;

	msm_pc_debug_counters[cntr_offset]++;
}

static bool msm_pm_pc_hotplug(void)
{
	uint32_t cpu = smp_processor_id();
	enum msm_pm_l2_scm_flag flag;

	flag = lpm_cpu_pre_pc_cb(cpu);

	if (flag == MSM_SCM_L2_OFF)
		flush_cache_all();
	else if (msm_pm_is_L1_writeback())
		flush_cache_louis();

	msm_pc_inc_debug_count(cpu, MSM_PC_ENTRY_COUNTER);

	scm_call_atomic1(SCM_SVC_BOOT, SCM_CMD_TERMINATE_PC,
		SCM_CMD_CORE_HOTPLUGGED | (flag & SCM_FLUSH_FLAG_MASK));

	/* Should not return here */
	msm_pc_inc_debug_count(cpu, MSM_PC_FALLTHRU_COUNTER);
	return 0;
}

int msm_pm_collapse(unsigned long unused)
{
	uint32_t cpu = smp_processor_id();
	enum msm_pm_l2_scm_flag flag;

	flag = lpm_cpu_pre_pc_cb(cpu);

	if (flag == MSM_SCM_L2_OFF)
		flush_cache_all();
	else if (msm_pm_is_L1_writeback())
		flush_cache_louis();

	msm_pc_inc_debug_count(cpu, MSM_PC_ENTRY_COUNTER);

	scm_call_atomic1(SCM_SVC_BOOT, SCM_CMD_TERMINATE_PC, flag);

	msm_pc_inc_debug_count(cpu, MSM_PC_FALLTHRU_COUNTER);

	return 0;
}
EXPORT_SYMBOL(msm_pm_collapse);

static bool __ref msm_pm_spm_power_collapse(
	unsigned int cpu, bool from_idle, bool notify_rpm)
{
	void *entry;
	bool collapsed = 0;
	int ret;
	bool save_cpu_regs = (num_online_cpus() == 1) || from_idle;

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: notify_rpm %d\n",
			cpu, __func__, (int) notify_rpm);

	if (from_idle)
		cpu_pm_enter();

	ret = msm_spm_set_low_power_mode(
			MSM_SPM_MODE_POWER_COLLAPSE, notify_rpm);
	WARN_ON(ret);

	entry = save_cpu_regs ?  cpu_resume : msm_secondary_startup;

	msm_pm_boot_config_before_pc(cpu, virt_to_phys(entry));

	if (MSM_PM_DEBUG_RESET_VECTOR & msm_pm_debug_mask)
		pr_info("CPU%u: %s: program vector to %p\n",
			cpu, __func__, entry);

	msm_jtag_save_state();

#ifdef CONFIG_CPU_V7
	collapsed = save_cpu_regs ?
		!cpu_suspend(0, msm_pm_collapse) : msm_pm_pc_hotplug();
#else
	collapsed = save_cpu_regs ?
		!cpu_suspend(0) : msm_pm_pc_hotplug();
#endif

	msm_jtag_restore_state();

	if (collapsed)
		local_fiq_enable();

	msm_pm_boot_config_after_pc(cpu);

	if (from_idle)
		cpu_pm_exit();

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: msm_pm_collapse returned, collapsed %d\n",
			cpu, __func__, collapsed);

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);
	WARN_ON(ret);
	return collapsed;
}

static bool msm_pm_power_collapse_standalone(
		bool from_idle)
{
	unsigned int cpu = smp_processor_id();
	unsigned int avsdscr;
	unsigned int avscsr;
	bool collapsed;

	avsdscr = avs_get_avsdscr();
	avscsr = avs_get_avscsr();
	avs_set_avscsr(0); /* Disable AVS */

	collapsed = msm_pm_spm_power_collapse(cpu, from_idle, false);

	avs_set_avsdscr(avsdscr);
	avs_set_avscsr(avscsr);
	return collapsed;
}

static int ramp_down_last_cpu(int cpu)
{
	struct clk *cpu_clk = per_cpu(cpu_clks, cpu);
	int ret = 0;

	clk_disable(cpu_clk);
	clk_disable(l2_clk);

	return ret;
}

static int ramp_up_first_cpu(int cpu, int saved_rate)
{
	struct clk *cpu_clk = per_cpu(cpu_clks, cpu);
	int rc = 0;

	if (MSM_PM_DEBUG_CLOCK & msm_pm_debug_mask)
		pr_info("CPU%u: %s: restore clock rate\n",
				cpu, __func__);

	clk_enable(l2_clk);

	if (cpu_clk) {
		int ret = clk_enable(cpu_clk);

		if (ret) {
			pr_err("%s(): Error restoring cpu clk\n",
					__func__);
			return ret;
		}
	}

	return rc;
}

static bool msm_pm_power_collapse(bool from_idle)
{
	unsigned int cpu = smp_processor_id();
	unsigned long saved_acpuclk_rate = 0;
	unsigned int avsdscr;
	unsigned int avscsr;
	bool collapsed;

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: idle %d\n",
			cpu, __func__, (int)from_idle);

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: pre power down\n", cpu, __func__);

	/* This spews a lot of messages when a core is hotplugged. This
	 * information is most useful from last core going down during
	 * power collapse
	 */
	if ((!from_idle && cpu_online(cpu))
			|| (MSM_PM_DEBUG_IDLE_CLK & msm_pm_debug_mask))
		clock_debug_print_enabled();

	avsdscr = avs_get_avsdscr();
	avscsr = avs_get_avscsr();
	avs_set_avscsr(0); /* Disable AVS */

	if (cpu_online(cpu) && !msm_no_ramp_down_pc)
		saved_acpuclk_rate = ramp_down_last_cpu(cpu);

	collapsed = msm_pm_spm_power_collapse(cpu, from_idle, true);

	if (cpu_online(cpu) && !msm_no_ramp_down_pc)
		ramp_up_first_cpu(cpu, saved_acpuclk_rate);

	avs_set_avsdscr(avsdscr);
	avs_set_avscsr(avscsr);

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: post power up\n", cpu, __func__);

	if (MSM_PM_DEBUG_POWER_COLLAPSE & msm_pm_debug_mask)
		pr_info("CPU%u: %s: return\n", cpu, __func__);
	return collapsed;
}
/******************************************************************************
 * External Idle/Suspend Functions
 *****************************************************************************/

void arch_idle(void)
{
	return;
}

static bool (*execute[MSM_PM_SLEEP_MODE_NR])(bool idle) = {
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT] = msm_pm_swfi,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE] =
		msm_pm_power_collapse_standalone,
	[MSM_PM_SLEEP_MODE_RETENTION] = msm_pm_retention,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = msm_pm_power_collapse,
};

/**
 * msm_cpu_pm_enter_sleep(): Enter a low power mode on current cpu
 *
 * @mode - sleep mode to enter
 * @from_idle - bool to indicate that the mode is exercised during idle/suspend
 *
 * returns none
 *
 * The code should be with interrupts disabled and on the core on which the
 * low power is to be executed.
 *
 */
bool msm_cpu_pm_enter_sleep(enum msm_pm_sleep_mode mode, bool from_idle)
{
	bool exit_stat = false;
	unsigned int cpu = smp_processor_id();

	if ((!from_idle  && cpu_online(cpu))
			|| (MSM_PM_DEBUG_IDLE & msm_pm_debug_mask))
		pr_info("CPU%u:%s mode:%d during %s\n", cpu, __func__,
				mode, from_idle ? "idle" : "suspend");

	if (execute[mode])
		exit_stat = execute[mode](from_idle);

	return exit_stat;
}

/**
 * msm_pm_wait_cpu_shutdown() - Wait for a core to be power collapsed during
 *				hotplug
 *
 * @ cpu - cpu to wait on.
 *
 * Blocking function call that waits on the core to be power collapsed. This
 * function is called from platform_cpu_die to ensure that a core is power
 * collapsed before sending the CPU_DEAD notification so the drivers could
 * remove the resource votes for this CPU(regulator and clock)
 */
int msm_pm_wait_cpu_shutdown(unsigned int cpu)
{
	int timeout = 10;

	if (!msm_pm_slp_sts)
		return 0;
	if (!msm_pm_slp_sts[cpu].base_addr)
		return 0;
	while (1) {
		/*
		 * Check for the SPM of the core being hotplugged to set
		 * its sleep state.The SPM sleep state indicates that the
		 * core has been power collapsed.
		 */
		int acc_sts = __raw_readl(msm_pm_slp_sts[cpu].base_addr);

		if (acc_sts & msm_pm_slp_sts[cpu].mask)
			return 0;

		udelay(100);
		WARN(++timeout == 20, "CPU%u didn't collapse in 2ms\n", cpu);
	}

	return -EBUSY;
}

static void msm_pm_ack_retention_disable(void *data)
{
	/*
	 * This is a NULL function to ensure that the core has woken up
	 * and is safe to disable retention.
	 */
}
/**
 * msm_pm_enable_retention() - Disable/Enable retention on all cores
 * @enable: Enable/Disable retention
 *
 */
void msm_pm_enable_retention(bool enable)
{
	if (enable == msm_pm_ldo_retention_enabled)
		return;

	msm_pm_ldo_retention_enabled = enable;

	/*
	 * If retention is being disabled, wakeup all online core to ensure
	 * that it isn't executing retention. Offlined cores need not be woken
	 * up as they enter the deepest sleep mode, namely RPM assited power
	 * collapse
	 */
	if (!enable) {
		preempt_disable();
		smp_call_function_many(&retention_cpus,
				msm_pm_ack_retention_disable,
				NULL, true);
		preempt_enable();
	}
}
EXPORT_SYMBOL(msm_pm_enable_retention);

/**
 * msm_pm_retention_enabled() - Check if retention is enabled
 *
 * returns true if retention is enabled
 */
bool msm_pm_retention_enabled(void)
{
	return msm_pm_ldo_retention_enabled;
}
EXPORT_SYMBOL(msm_pm_retention_enabled);

static int msm_pm_snoc_client_probe(struct platform_device *pdev)
{
	int rc = 0;
	static struct msm_bus_scale_pdata *msm_pm_bus_pdata;
	static uint32_t msm_pm_bus_client;

	msm_pm_bus_pdata = msm_bus_cl_get_pdata(pdev);

	if (msm_pm_bus_pdata) {
		msm_pm_bus_client =
			msm_bus_scale_register_client(msm_pm_bus_pdata);

		if (!msm_pm_bus_client) {
			pr_err("%s: Failed to register SNOC client", __func__);
			rc = -ENXIO;
			goto snoc_cl_probe_done;
		}

		rc = msm_bus_scale_client_update_request(msm_pm_bus_client, 1);

		if (rc)
			pr_err("%s: Error setting bus rate", __func__);
	}

snoc_cl_probe_done:
	return rc;
}

static int msm_cpu_status_probe(struct platform_device *pdev)
{
	struct msm_pm_sleep_status_data *pdata;
	char *key;
	u32 cpu;

	if (!pdev)
		return -EFAULT;

	msm_pm_slp_sts = devm_kzalloc(&pdev->dev,
			sizeof(*msm_pm_slp_sts) * num_possible_cpus(),
			GFP_KERNEL);

	if (!msm_pm_slp_sts)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		struct resource *res;
		u32 offset;
		int rc;
		u32 mask;
		bool offset_available = true;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			return -ENODEV;

		key = "qcom,cpu-alias-addr";
		rc = of_property_read_u32(pdev->dev.of_node, key, &offset);

		if (rc)
			offset_available = false;

		key = "qcom,sleep-status-mask";
		rc = of_property_read_u32(pdev->dev.of_node, key, &mask);

		if (rc)
			return -ENODEV;

		for_each_possible_cpu(cpu) {
			phys_addr_t base_c;

			if (offset_available)
				base_c = res->start + cpu * offset;
			else {
				res = platform_get_resource(pdev,
							IORESOURCE_MEM, cpu);
				if (!res)
					return -ENODEV;
				base_c = res->start;
			}

			msm_pm_slp_sts[cpu].base_addr =
				devm_ioremap(&pdev->dev, base_c,
						resource_size(res));
			msm_pm_slp_sts[cpu].mask = mask;

			if (!msm_pm_slp_sts[cpu].base_addr)
				return -ENOMEM;
		}
	} else {
		pdata = pdev->dev.platform_data;
		if (!pdev->dev.platform_data)
			return -EINVAL;

		for_each_possible_cpu(cpu) {
			msm_pm_slp_sts[cpu].base_addr =
				pdata->base_addr + cpu * pdata->cpu_offset;
			msm_pm_slp_sts[cpu].mask = pdata->mask;
		}
	}

	return 0;
};

static struct of_device_id msm_slp_sts_match_tbl[] = {
	{.compatible = "qcom,cpu-sleep-status"},
	{},
};

static struct platform_driver msm_cpu_status_driver = {
	.probe = msm_cpu_status_probe,
	.driver = {
		.name = "cpu_slp_status",
		.owner = THIS_MODULE,
		.of_match_table = msm_slp_sts_match_tbl,
	},
};

static struct of_device_id msm_snoc_clnt_match_tbl[] = {
	{.compatible = "qcom,pm-snoc-client"},
	{},
};

static struct platform_driver msm_cpu_pm_snoc_client_driver = {
	.probe = msm_pm_snoc_client_probe,
	.driver = {
		.name = "pm_snoc_client",
		.owner = THIS_MODULE,
		.of_match_table = msm_snoc_clnt_match_tbl,
	},
};

struct msm_pc_debug_counters_buffer {
	int *reg;
	u32 len;
	char buf[MAX_BUF_SIZE];
};

char *counter_name[MSM_PC_NUM_COUNTERS] = {
		"PC Entry Counter",
		"Warmboot Entry Counter",
		"PC Bailout Counter"
};

static int msm_pc_debug_counters_copy(
		struct msm_pc_debug_counters_buffer *data)
{
	int j;
	u32 stat;
	unsigned int cpu;
	unsigned int len;
	uint32_t cluster_id;
	uint32_t cpu_id;
	uint32_t offset;

	for_each_possible_cpu(cpu) {
		len = scnprintf(data->buf + data->len,
				sizeof(data->buf)-data->len,
				"CPU%d\n", cpu);
		cluster_id = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
		cpu_id = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 0);
		offset = (cluster_id * MAX_CPUS_PER_CLUSTER
				 * MSM_PC_NUM_COUNTERS)
				 + (cpu_id * MSM_PC_NUM_COUNTERS);

		data->len += len;

		for (j = 0; j < MSM_PC_NUM_COUNTERS; j++) {
			stat = data->reg[offset + j];
			len = scnprintf(data->buf + data->len,
					 sizeof(data->buf) - data->len,
					"\t%s: %d", counter_name[j], stat);

			data->len += len;
		}

	}

	return data->len;
}

static ssize_t msm_pc_debug_counters_file_read(struct file *file,
		char __user *bufu, size_t count, loff_t *ppos)
{
	struct msm_pc_debug_counters_buffer *data;

	data = file->private_data;

	if (!data)
		return -EINVAL;

	if (!bufu)
		return -EINVAL;

	if (!access_ok(VERIFY_WRITE, bufu, count))
		return -EFAULT;

	if (*ppos >= data->len && data->len == 0)
		data->len = msm_pc_debug_counters_copy(data);

	return simple_read_from_buffer(bufu, count, ppos,
			data->buf, data->len);
}

static int msm_pc_debug_counters_file_open(struct inode *inode,
		struct file *file)
{
	struct msm_pc_debug_counters_buffer *buf;


	if (!inode->i_private)
		return -EINVAL;

	file->private_data = kzalloc(
		sizeof(struct msm_pc_debug_counters_buffer), GFP_KERNEL);

	if (!file->private_data) {
		pr_err("%s: ERROR kmalloc failed to allocate %zu bytes\n",
		__func__, sizeof(struct msm_pc_debug_counters_buffer));

		return -ENOMEM;
	}

	buf = file->private_data;
	buf->reg = (int *)inode->i_private;

	return 0;
}

static int msm_pc_debug_counters_file_close(struct inode *inode,
		struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static const struct file_operations msm_pc_debug_counters_fops = {
	.open = msm_pc_debug_counters_file_open,
	.read = msm_pc_debug_counters_file_read,
	.release = msm_pc_debug_counters_file_close,
	.llseek = no_llseek,
};

static int msm_pm_clk_init(struct platform_device *pdev)
{
	bool synced_clocks;
	u32 cpu;
	char clk_name[] = "cpu??_clk";
	char *key;

	key = "qcom,synced-clocks";
	synced_clocks = of_property_read_bool(pdev->dev.of_node, key);

	for_each_possible_cpu(cpu) {
		struct clk *clk;
		snprintf(clk_name, sizeof(clk_name), "cpu%d_clk", cpu);
		clk = devm_clk_get(&pdev->dev, clk_name);
		if (IS_ERR(clk)) {
			if (cpu && synced_clocks)
				return 0;
			else
				return PTR_ERR(clk);
		}
		per_cpu(cpu_clks, cpu) = clk;
	}

	if (synced_clocks)
		return 0;

	l2_clk = devm_clk_get(&pdev->dev, "l2_clk");
	if (IS_ERR(l2_clk))
		pr_warn("%s: Could not get l2_clk (-%ld)\n", __func__,
			PTR_ERR(l2_clk));

	return 0;
}

static int msm_cpu_pm_probe(struct platform_device *pdev)
{
	struct dentry *dent = NULL;
	struct resource *res = NULL;
	int ret = 0;
	void __iomem *msm_pc_debug_counters_imem;
	int alloc_size = (MAX_NUM_CLUSTER * MAX_CPUS_PER_CLUSTER
					* MSM_PC_NUM_COUNTERS
					* sizeof(*msm_pc_debug_counters));

	msm_pc_debug_counters = dma_alloc_coherent(&pdev->dev, alloc_size,
				&msm_pc_debug_counters_phys, GFP_KERNEL);

	if (msm_pc_debug_counters) {
		memset(msm_pc_debug_counters, 0, alloc_size);
		dent = debugfs_create_file("pc_debug_counter", S_IRUGO, NULL,
				msm_pc_debug_counters,
				&msm_pc_debug_counters_fops);
		if (!dent)
			pr_err("%s: ERROR debugfs_create_file failed\n",
					__func__);
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			goto skip_save_imem;
		msm_pc_debug_counters_imem = devm_ioremap(&pdev->dev,
						res->start, resource_size(res));
		if (msm_pc_debug_counters_imem) {
			writel_relaxed(msm_pc_debug_counters_phys,
					msm_pc_debug_counters_imem);
			mb();
			devm_iounmap(&pdev->dev,
					msm_pc_debug_counters_imem);
		}
	} else {
		msm_pc_debug_counters = 0;
		msm_pc_debug_counters_phys = 0;
	}
skip_save_imem:
	if (pdev->dev.of_node) {
		ret = msm_pm_clk_init(pdev);
		if (ret) {
			pr_info("msm_pm_clk_init returned error\n");
			return ret;
		}
	}

	if (pdev->dev.of_node)
		of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);

	return ret;
}

static struct of_device_id msm_cpu_pm_table[] = {
	{.compatible = "qcom,pm"},
	{},
};

static struct platform_driver msm_cpu_pm_driver = {
	.probe = msm_cpu_pm_probe,
	.driver = {
		.name = "msm-pm",
		.owner = THIS_MODULE,
		.of_match_table = msm_cpu_pm_table,
	},
};

static int __init msm_pm_drv_init(void)
{
	int rc;

	cpumask_clear(&retention_cpus);

	rc = platform_driver_register(&msm_cpu_pm_snoc_client_driver);

	if (rc) {
		pr_err("%s(): failed to register driver %s\n", __func__,
				msm_cpu_pm_snoc_client_driver.driver.name);
		return rc;
	}

	return platform_driver_register(&msm_cpu_pm_driver);
}
late_initcall(msm_pm_drv_init);

int __init msm_pm_sleep_status_init(void)
{
	static bool registered;

	if (registered)
		return 0;
	registered = true;

	return platform_driver_register(&msm_cpu_status_driver);
}
arch_initcall(msm_pm_sleep_status_init);
