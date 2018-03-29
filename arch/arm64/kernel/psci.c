/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright (C) 2013 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#define pr_fmt(fmt) "psci: " fmt

#include <linux/init.h>
#include <linux/of.h>
#include <linux/smp.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <uapi/linux/psci.h>

#include <asm/compiler.h>
#include <asm/cpu_ops.h>
#include <asm/errno.h>
#include <asm/psci.h>
#include <asm/smp_plat.h>
#include <asm/suspend.h>
#include <asm/system_misc.h>
#include <mt-plat/mtk_ram_console.h>
#ifdef CONFIG_ARCH_MT6797
#include <mt6797/da9214.h>
#include <mach/mt_freqhopping.h>
#include <mt_dcm.h>
#include <mt_clkmgr.h>
#include <mt_idvfs.h>
#include <mt_ocp.h>
#include <mt6797/mt_wdt.h>
#include <ext_wd_drv.h>
#endif

#ifdef MTK_IRQ_NEW_DESIGN
#include <linux/irqchip/mtk-gic-extend.h>
#endif

#define PSCI_POWER_STATE_TYPE_STANDBY		0
#define PSCI_POWER_STATE_TYPE_POWER_DOWN	1

#ifdef CONFIG_ARCH_MT6797
#define MT6797_SPM_BASE_ADDR		0x10006000
#define MT6797_IDVFS_BASE_ADDR		0x10222000

#define CONFIG_CL2_BUCK_CTRL	1
#define CONFIG_ARMPLL_CTRL	1
#define CONFIG_OCP_IDVFS_CTRL	1

int bypass_boot = 0;
int bypass_cl0_armpll = 3;
int bypass_cl1_armpll = 1;	/* min(4, maxcpus - 4) */
char g_cl0_online = 1;	/* cpu0 is online */
char g_cl1_online = 0;
char g_cl2_online = 0;

#ifdef CONFIG_OCP_IDVFS_CTRL
int ocp_cl0_init = 0;
int ocp_cl1_init = 0;
int idvfs_init = 0;
#endif

#ifdef CONFIG_CL2_BUCK_CTRL
DEFINE_SPINLOCK(reset_lock);
int reset_flags;
#endif

#endif

struct psci_power_state {
	u16	id;
	u8	type;
	u8	affinity_level;
};

struct psci_operations {
	int (*cpu_suspend)(struct psci_power_state state,
			   unsigned long entry_point);
	int (*cpu_off)(struct psci_power_state state);
	int (*cpu_on)(unsigned long cpuid, unsigned long entry_point);
	int (*migrate)(unsigned long cpuid);
	int (*affinity_info)(unsigned long target_affinity,
			unsigned long lowest_affinity_level);
	int (*migrate_info_type)(void);
};

static struct psci_operations psci_ops;

static int (*invoke_psci_fn)(u64, u64, u64, u64);
typedef int (*psci_initcall_t)(const struct device_node *);

asmlinkage int __invoke_psci_fn_hvc(u64, u64, u64, u64);
asmlinkage int __invoke_psci_fn_smc(u64, u64, u64, u64);

enum psci_function {
	PSCI_FN_CPU_SUSPEND,
	PSCI_FN_CPU_ON,
	PSCI_FN_CPU_OFF,
	PSCI_FN_MIGRATE,
	PSCI_FN_AFFINITY_INFO,
	PSCI_FN_MIGRATE_INFO_TYPE,
	PSCI_FN_MAX,
};

static DEFINE_PER_CPU_READ_MOSTLY(struct psci_power_state *, psci_power_state);

static u32 psci_function_id[PSCI_FN_MAX];

static int psci_to_linux_errno(int errno)
{
	switch (errno) {
	case PSCI_RET_SUCCESS:
		return 0;
	case PSCI_RET_NOT_SUPPORTED:
		return -EOPNOTSUPP;
	case PSCI_RET_INVALID_PARAMS:
		return -EINVAL;
	case PSCI_RET_DENIED:
		return -EPERM;
	};

	return -EINVAL;
}

static u32 psci_power_state_pack(struct psci_power_state state)
{
	return ((state.id << PSCI_0_2_POWER_STATE_ID_SHIFT)
			& PSCI_0_2_POWER_STATE_ID_MASK) |
		((state.type << PSCI_0_2_POWER_STATE_TYPE_SHIFT)
		 & PSCI_0_2_POWER_STATE_TYPE_MASK) |
		((state.affinity_level << PSCI_0_2_POWER_STATE_AFFL_SHIFT)
		 & PSCI_0_2_POWER_STATE_AFFL_MASK);
}

static void psci_power_state_unpack(u32 power_state,
				    struct psci_power_state *state)
{
	state->id = (power_state & PSCI_0_2_POWER_STATE_ID_MASK) >>
			PSCI_0_2_POWER_STATE_ID_SHIFT;
	state->type = (power_state & PSCI_0_2_POWER_STATE_TYPE_MASK) >>
			PSCI_0_2_POWER_STATE_TYPE_SHIFT;
	state->affinity_level =
			(power_state & PSCI_0_2_POWER_STATE_AFFL_MASK) >>
			PSCI_0_2_POWER_STATE_AFFL_SHIFT;
}

static int psci_get_version(void)
{
	int err;

	err = invoke_psci_fn(PSCI_0_2_FN_PSCI_VERSION, 0, 0, 0);
	return err;
}

static int psci_cpu_suspend(struct psci_power_state state,
			    unsigned long entry_point)
{
	int err;
	u32 fn, power_state;

	fn = psci_function_id[PSCI_FN_CPU_SUSPEND];
	power_state = psci_power_state_pack(state);
	err = invoke_psci_fn(fn, power_state, entry_point, 0);
	return psci_to_linux_errno(err);
}

static int psci_cpu_off(struct psci_power_state state)
{
	int err;
	u32 fn, power_state;

	fn = psci_function_id[PSCI_FN_CPU_OFF];
	power_state = psci_power_state_pack(state);
	err = invoke_psci_fn(fn, power_state, 0, 0);
	return psci_to_linux_errno(err);
}

static int psci_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	int err;
	u32 fn;

	fn = psci_function_id[PSCI_FN_CPU_ON];
	err = invoke_psci_fn(fn, cpuid, entry_point, 0);
	return psci_to_linux_errno(err);
}

static int psci_migrate(unsigned long cpuid)
{
	int err;
	u32 fn;

	fn = psci_function_id[PSCI_FN_MIGRATE];
	err = invoke_psci_fn(fn, cpuid, 0, 0);
	return psci_to_linux_errno(err);
}

static int psci_affinity_info(unsigned long target_affinity,
		unsigned long lowest_affinity_level)
{
	int err;
	u32 fn;

	fn = psci_function_id[PSCI_FN_AFFINITY_INFO];
	err = invoke_psci_fn(fn, target_affinity, lowest_affinity_level, 0);
	return err;
}

static int psci_migrate_info_type(void)
{
	int err;
	u32 fn;

	fn = psci_function_id[PSCI_FN_MIGRATE_INFO_TYPE];
	err = invoke_psci_fn(fn, 0, 0, 0);
	return err;
}

static int __maybe_unused cpu_psci_cpu_init_idle(struct device_node *cpu_node,
						 unsigned int cpu)
{
	int i, ret, count = 0;
	struct psci_power_state *psci_states;
	struct device_node *state_node;

	/*
	 * If the PSCI cpu_suspend function hook has not been initialized
	 * idle states must not be enabled, so bail out
	 */
	if (!psci_ops.cpu_suspend)
		return -EOPNOTSUPP;

	/* Count idle states */
	while ((state_node = of_parse_phandle(cpu_node, "cpu-idle-states",
					      count))) {
		count++;
		of_node_put(state_node);
	}

	if (!count)
		return -ENODEV;

	psci_states = kcalloc(count, sizeof(*psci_states), GFP_KERNEL);
	if (!psci_states)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		u32 psci_power_state;

		state_node = of_parse_phandle(cpu_node, "cpu-idle-states", i);

		ret = of_property_read_u32(state_node,
					   "arm,psci-suspend-param",
					   &psci_power_state);
		if (ret) {
			pr_warn(" * %s missing arm,psci-suspend-param property\n",
				state_node->full_name);
			of_node_put(state_node);
			goto free_mem;
		}

		of_node_put(state_node);
		pr_debug("psci-power-state %#x index %d\n", psci_power_state,
							    i);
		psci_power_state_unpack(psci_power_state, &psci_states[i]);
	}
	/* Idle states parsed correctly, initialize per-cpu pointer */
	per_cpu(psci_power_state, cpu) = psci_states;
	return 0;

free_mem:
	kfree(psci_states);
	return ret;
}

static int get_set_conduit_method(struct device_node *np)
{
	const char *method;

	pr_info("probing for conduit method from DT.\n");

	if (of_property_read_string(np, "method", &method)) {
		pr_warn("missing \"method\" property\n");
		return -ENXIO;
	}

	if (!strcmp("hvc", method)) {
		invoke_psci_fn = __invoke_psci_fn_hvc;
	} else if (!strcmp("smc", method)) {
		invoke_psci_fn = __invoke_psci_fn_smc;
	} else {
		pr_warn("invalid \"method\" property: %s\n", method);
		return -EINVAL;
	}
	return 0;
}

#ifndef CONFIG_ARCH_MT6797
static void psci_sys_reset(enum reboot_mode reboot_mode, const char *cmd)
{
	invoke_psci_fn(PSCI_0_2_FN_SYSTEM_RESET, 0, 0, 0);
}

static void psci_sys_poweroff(void)
{
	invoke_psci_fn(PSCI_0_2_FN_SYSTEM_OFF, 0, 0, 0);
}
#endif

/*
 * PSCI Function IDs for v0.2+ are well defined so use
 * standard values.
 */
static int __init psci_0_2_init(struct device_node *np)
{
	int err, ver;

	err = get_set_conduit_method(np);

	if (err)
		goto out_put_node;

	ver = psci_get_version();

	if (ver == PSCI_RET_NOT_SUPPORTED) {
		/* PSCI v0.2 mandates implementation of PSCI_ID_VERSION. */
		pr_err("PSCI firmware does not comply with the v0.2 spec.\n");
		err = -EOPNOTSUPP;
		goto out_put_node;
	} else {
		pr_info("PSCIv%d.%d detected in firmware.\n",
				PSCI_VERSION_MAJOR(ver),
				PSCI_VERSION_MINOR(ver));

		if (PSCI_VERSION_MAJOR(ver) == 0 &&
				PSCI_VERSION_MINOR(ver) < 2) {
			err = -EINVAL;
			pr_err("Conflicting PSCI version detected.\n");
			goto out_put_node;
		}
	}

	pr_info("Using standard PSCI v0.2 function IDs\n");
	psci_function_id[PSCI_FN_CPU_SUSPEND] = PSCI_0_2_FN64_CPU_SUSPEND;
	psci_ops.cpu_suspend = psci_cpu_suspend;

	psci_function_id[PSCI_FN_CPU_OFF] = PSCI_0_2_FN_CPU_OFF;
	psci_ops.cpu_off = psci_cpu_off;

	psci_function_id[PSCI_FN_CPU_ON] = PSCI_0_2_FN64_CPU_ON;
	psci_ops.cpu_on = psci_cpu_on;

	psci_function_id[PSCI_FN_MIGRATE] = PSCI_0_2_FN64_MIGRATE;
	psci_ops.migrate = psci_migrate;

	psci_function_id[PSCI_FN_AFFINITY_INFO] = PSCI_0_2_FN64_AFFINITY_INFO;
	psci_ops.affinity_info = psci_affinity_info;

	psci_function_id[PSCI_FN_MIGRATE_INFO_TYPE] =
		PSCI_0_2_FN_MIGRATE_INFO_TYPE;
	psci_ops.migrate_info_type = psci_migrate_info_type;

#ifndef CONFIG_ARCH_MT6797
	arm_pm_restart = psci_sys_reset;

	pm_power_off = psci_sys_poweroff;
#endif

out_put_node:
	of_node_put(np);
	return err;
}

/*
 * PSCI < v0.2 get PSCI Function IDs via DT.
 */
static int __init psci_0_1_init(struct device_node *np)
{
	u32 id;
	int err;

	err = get_set_conduit_method(np);

	if (err)
		goto out_put_node;

	pr_info("Using PSCI v0.1 Function IDs from DT\n");

	if (!of_property_read_u32(np, "cpu_suspend", &id)) {
		psci_function_id[PSCI_FN_CPU_SUSPEND] = id;
		psci_ops.cpu_suspend = psci_cpu_suspend;
	}

	if (!of_property_read_u32(np, "cpu_off", &id)) {
		psci_function_id[PSCI_FN_CPU_OFF] = id;
		psci_ops.cpu_off = psci_cpu_off;
	}

	if (!of_property_read_u32(np, "cpu_on", &id)) {
		psci_function_id[PSCI_FN_CPU_ON] = id;
		psci_ops.cpu_on = psci_cpu_on;
	}

	if (!of_property_read_u32(np, "migrate", &id)) {
		psci_function_id[PSCI_FN_MIGRATE] = id;
		psci_ops.migrate = psci_migrate;
	}

out_put_node:
	of_node_put(np);
	return err;
}

static const struct of_device_id psci_of_match[] __initconst = {
	{ .compatible = "arm,psci",	.data = psci_0_1_init},
	{ .compatible = "arm,psci-0.2",	.data = psci_0_2_init},
	{},
};

int __init psci_init(void)
{
	struct device_node *np;
	const struct of_device_id *matched_np;
	psci_initcall_t init_fn;

	np = of_find_matching_node_and_match(NULL, psci_of_match, &matched_np);

	if (!np)
		return -ENODEV;

	init_fn = (psci_initcall_t)matched_np->data;
	return init_fn(np);
}

#ifdef CONFIG_SMP

static int __init cpu_psci_cpu_init(struct device_node *dn, unsigned int cpu)
{
	return 0;
}

static int __init cpu_psci_cpu_prepare(unsigned int cpu)
{
	if (!psci_ops.cpu_on) {
		pr_err("no cpu_on method, not booting CPU%d\n", cpu);
		return -ENODEV;
	}

	return 0;
}

#ifdef CONFIG_ARCH_MT6797
#ifdef CONFIG_CL2_BUCK_CTRL
static int cpu_power_on_buck(unsigned int cpu, bool hotplug)
{
	static void __iomem *reg_base;
	static volatile unsigned int temp;
	int ret = 0;

	/* set reset_flags for OCP */
	spin_lock(&reset_lock);
	reset_flags = 1;
	spin_unlock(&reset_lock);

	reg_base = ioremap(MT6797_SPM_BASE_ADDR, 0x1000);
	writel_relaxed((readl(reg_base + 0x218) | (1 << 0)), reg_base + 0x218);
	iounmap(reg_base);

	reg_base = ioremap(MT6797_IDVFS_BASE_ADDR, 0x1000);	/* 0x102224a0 */
	temp = readl(reg_base + 0x4a0); /* dummy read */
	iounmap(reg_base);

	/* latch RESET */
	mtk_wdt_swsysret_config(MTK_WDT_SWSYS_RST_PWRAP_SPI_CTL_RST, 1);

	if (hotplug) {
		BUG_ON(da9214_config_interface(0x0, 0x0, 0xF, 0) < 0);
		BUG_ON(da9214_config_interface(0x5E, 0x1, 0x1, 0) < 0);

		udelay(1000);
	}

	/* EXT_BUCK_ISO */
	reg_base = ioremap(MT6797_SPM_BASE_ADDR, 0x1000);
	writel_relaxed((readl(reg_base + 0x290) & ~(0x3)), reg_base + 0x290);
	iounmap(reg_base);

	/* unlatch RESET */
	mtk_wdt_swsysret_config(MTK_WDT_SWSYS_RST_PWRAP_SPI_CTL_RST, 0);

	/* clear reset_flags for OCP */
	spin_lock(&reset_lock);
	reset_flags = 0;
	spin_unlock(&reset_lock);

	/* set VSRAM enable, cal_eFuse, rsh = 0x0f -> 0x08 */
	udelay(240);
	BigiDVFSSRAMLDOSet(110000);
	udelay(240);

	return ret;
}

static int cpu_power_off_buck(unsigned int cpu)
{
	int ret = 0;

	ret = da9214_config_interface(0x0, 0x0, 0xF, 0);
	ret = da9214_config_interface(0x5E, 0x0, 0x1, 0);

	BigiDVFSSRAMLDODisable();

	return ret;
}
#endif
#endif

static int cpu_psci_cpu_boot(unsigned int cpu)
{
#ifdef CONFIG_ARCH_MT6797
	int err = 0;


#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
	TIMESTAMP_REC(hotplug_ts_rec, TIMESTAMP_FILTER,  cpu, 0, 0, 0);
#endif

	if ((cpu == 0) || (cpu == 1) || (cpu == 2) || (cpu == 3)) {
		if (bypass_cl0_armpll > 0) {
			bypass_cl0_armpll--;
		} else {
			if (!g_cl0_online) {
#ifdef CONFIG_ARMPLL_CTRL
				/* turn on arm pll */
				enable_armpll_ll();
				/* non-pause FQHP function */
				mt_pause_armpll(MCU_FH_PLL0, 0);
				/* switch to HW mode */
				switch_armpll_ll_hwmode(1);
#endif
			}
		}
	} else if ((cpu == 4) || (cpu == 5) || (cpu == 6) || (cpu == 7)) {
		if (bypass_cl1_armpll > 0) {
			bypass_cl1_armpll--;
		} else {
			if (!g_cl1_online) {
#ifdef CONFIG_ARMPLL_CTRL
				/* turn on arm pll */
				enable_armpll_l();
				/* non-pause FQHP function */
				mt_pause_armpll(MCU_FH_PLL1, 0);
				/* switch to HW mode */
				switch_armpll_l_hwmode(1);
#endif
			}
		}
	} else if ((cpu == 8) || (cpu == 9)) {
		if (bypass_boot > 0) {
#ifdef CONFIG_CL2_BUCK_CTRL
			if (!g_cl2_online)
				cpu_power_on_buck(cpu, 0);
#endif
			bypass_boot--;
		} else {
			if (!g_cl2_online) {
#ifdef CONFIG_CL2_BUCK_CTRL
				cpu_power_on_buck(cpu, 1);
#endif
			}
		}
	}

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
	TIMESTAMP_REC(hotplug_ts_rec, TIMESTAMP_FILTER,  cpu, 0, 0, 0);
#endif

	err = psci_ops.cpu_on(cpu_logical_map(cpu), __pa(secondary_entry));

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
	TIMESTAMP_REC(hotplug_ts_rec, TIMESTAMP_FILTER,  cpu, 0, 0, 0);
#endif

	if ((cpu == 8) || (cpu == 9)) {
		if (!g_cl2_online) {
			/* enable MP2 Sync DCM */
			dcm_mcusys_mp2_sync_dcm(1);
		}
	}
#ifdef CONFIG_OCP_IDVFS_CTRL
	if ((cpu == 0) || (cpu == 1) || (cpu == 2) || (cpu == 3)) {
		if (!ocp_cl0_init) {
			Cluster0_OCP_ON();
			ocp_cl0_init = 1;
		}
	} else if ((cpu == 4) || (cpu == 5) || (cpu == 6) || (cpu == 7)) {
		if (!ocp_cl1_init) {
			Cluster1_OCP_ON();
			ocp_cl1_init = 1;
		}
	} else if ((cpu == 8) || (cpu == 9)) {
		if (!idvfs_init) {
			BigiDVFSEnable_hp();
			idvfs_init = 1;
		}
	}
#endif

	if (err)
		pr_err("failed to boot CPU%d (%d)\n", cpu, err);
	else {
		if ((cpu == 0) || (cpu == 1) || (cpu == 2) || (cpu == 3))
			g_cl0_online |= (1 << cpu);
		else if ((cpu == 4) || (cpu == 5) || (cpu == 6) || (cpu == 7))
			g_cl1_online |= (1 << (cpu - 4));
		else if ((cpu == 8) || (cpu == 9))
			g_cl2_online |= (1 << (cpu - 8));
	}

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
	TIMESTAMP_REC(hotplug_ts_rec, TIMESTAMP_FILTER,  cpu, 0, 0, 0);
#endif
	/* shrink kernel log
	pr_info("boot CPU%d (0x%x, 0x%x, 0x%x)\n",
		cpu, g_cl0_online, g_cl1_online, g_cl2_online);
	*/
#else
	int err = psci_ops.cpu_on(cpu_logical_map(cpu), __pa(secondary_entry));
	if (err)
		pr_err("failed to boot CPU%d (%d)\n", cpu, err);
#endif

#ifdef MTK_IRQ_NEW_DESIGN
	gic_clear_primask();
#endif

	return err;
}

#ifdef CONFIG_HOTPLUG_CPU
static int cpu_psci_cpu_disable(unsigned int cpu)
{
	/* Fail early if we don't have CPU_OFF support */
	if (!psci_ops.cpu_off)
		return -EOPNOTSUPP;
	return 0;
}

static void cpu_psci_cpu_die(unsigned int cpu)
{
	int ret;
	/*
	 * There are no known implementations of PSCI actually using the
	 * power state field, pass a sensible default for now.
	 */
	struct psci_power_state state = {
		.type = PSCI_POWER_STATE_TYPE_POWER_DOWN,
	};
#ifdef MTK_IRQ_NEW_DESIGN
	gic_set_primask();
#endif
	ret = psci_ops.cpu_off(state);

	pr_crit("unable to power off CPU%u (%d)\n", cpu, ret);
}

#ifdef CONFIG_ARCH_MT6797
static int cpu_kill_pll_buck_ctrl(unsigned int cpu)
{
	int ret = 0;

	if ((cpu == 0) || (cpu == 1) || (cpu == 2) || (cpu == 3)) {
		g_cl0_online &= ~(1 << cpu);
		if (!g_cl0_online) {
#ifdef CONFIG_ARMPLL_CTRL
			/* switch to SW mode */
			switch_armpll_ll_hwmode(0);
			/* pause FQHP function */
			mt_pause_armpll(MCU_FH_PLL0, 1);
			/* turn off arm pll */
			disable_armpll_ll();
#endif
		}
	} else if ((cpu == 4) || (cpu == 5) || (cpu == 6) || (cpu == 7)) {
		g_cl1_online &= ~(1 << (cpu - 4));
		if (!g_cl1_online) {
#ifdef CONFIG_ARMPLL_CTRL
			/* switch to SW mode */
			switch_armpll_l_hwmode(0);
			/* pause FQHP function */
			mt_pause_armpll(MCU_FH_PLL1, 1);
			/* turn off arm pll */
			disable_armpll_l();
#endif
		}
	} else if ((cpu == 8) || (cpu == 9)) {
		/* update g_cl2_online before dcm_mcusys_mp2_sync_dcm(0) */
		/* g_cl2_online &= ~(1 << (cpu - 8)); */
		if (!g_cl2_online) {
#ifdef CONFIG_CL2_BUCK_CTRL
			ret = cpu_power_off_buck(cpu);
#endif
		}
	}

	return ret;
}
#endif

#ifdef CONFIG_ARCH_MT6797
unsigned int last_cl0_online_cpus(unsigned int cpu)
{
	int ret = 0;

	if (((cpu == 0) && (g_cl0_online == 1)) ||
	    ((cpu == 1) && (g_cl0_online == 2)) ||
	    ((cpu == 2) && (g_cl0_online == 4)) ||
	    ((cpu == 3) && (g_cl0_online == 8)))
			ret = 1;

	return ret;
}

unsigned int last_cl1_online_cpus(unsigned int cpu)
{
	int ret = 0;

	if (((cpu == 4) && (g_cl1_online == 1)) ||
	    ((cpu == 5) && (g_cl1_online == 2)) ||
	    ((cpu == 6) && (g_cl1_online == 4)) ||
	    ((cpu == 7) && (g_cl1_online == 8)))
			ret = 1;

	return ret;
}

unsigned int last_cl2_online_cpus(unsigned int cpu)
{
	int ret = 0;

	if (((cpu == 8) && (g_cl2_online == 1)) ||
	    ((cpu == 9) && (g_cl2_online == 2)))
			ret = 1;

	return ret;
}
#endif

static int cpu_psci_cpu_kill(unsigned int cpu)
{
	int err, i;

	if (!psci_ops.affinity_info)
		return 1;
	/*
	 * cpu_kill could race with cpu_die and we can
	 * potentially end up declaring this cpu undead
	 * while it is dying. So, try again a few times.
	 */

	for (i = 0; i < 10; i++) {
#ifdef CONFIG_ARCH_MT6797
#ifdef CONFIG_OCP_IDVFS_CTRL
#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
		TIMESTAMP_REC(hotplug_ts_rec, TIMESTAMP_FILTER,  cpu, 0, 0, 0);
#endif

		aee_rr_rec_hotplug_footprint(cpu, 81);
		if (idvfs_init && last_cl2_online_cpus(cpu)) {
			BigiDVFSDisable_hp();
			idvfs_init = 0;
		}

		aee_rr_rec_hotplug_footprint(cpu, 82);
		if (ocp_cl0_init && last_cl0_online_cpus(cpu)) {
			Cluster0_OCP_OFF();
			ocp_cl0_init = 0;
		}

		aee_rr_rec_hotplug_footprint(cpu, 83);
		if (ocp_cl1_init && last_cl1_online_cpus(cpu)) {
			Cluster1_OCP_OFF();
			ocp_cl1_init = 0;
		}

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
		TIMESTAMP_REC(hotplug_ts_rec, TIMESTAMP_FILTER,  cpu, 0, 0, 0);
#endif
#endif
#endif

#ifdef CONFIG_ARCH_MT6797
		if ((cpu == 8) || (cpu == 9)) {
			g_cl2_online &= ~(1 << (cpu - 8));
			if (!g_cl2_online) {
				aee_rr_rec_hotplug_footprint(cpu, 84);
				/* disable MP2 Sync DCM */
				dcm_mcusys_mp2_sync_dcm(0);
			}
		}
#endif

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
		TIMESTAMP_REC(hotplug_ts_rec, TIMESTAMP_FILTER,  cpu, 0, 0, 0);
#endif

		aee_rr_rec_hotplug_footprint(cpu, 85);
		err = psci_ops.affinity_info(cpu_logical_map(cpu), 0);

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
		TIMESTAMP_REC(hotplug_ts_rec, TIMESTAMP_FILTER,  cpu, 0, 0, 0);
#endif

		aee_rr_rec_hotplug_footprint(cpu, 86);
		if (err == PSCI_0_2_AFFINITY_LEVEL_OFF) {
#ifndef CONFIG_ARCH_MT6797
			aee_rr_rec_hotplug_footprint(cpu, 87);
			pr_info("CPU%d killed.\n", cpu);
#endif
#ifdef CONFIG_ARCH_MT6797

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
		TIMESTAMP_REC(hotplug_ts_rec, TIMESTAMP_FILTER,  cpu, 0, 0, 0);
#endif
			cpu_kill_pll_buck_ctrl(cpu);

#ifdef CONFIG_MTK_CPU_HOTPLUG_DEBUG_3
		TIMESTAMP_REC(hotplug_ts_rec, TIMESTAMP_FILTER,  cpu, 0, 0, 0);
#endif
#endif
			return 1;
		}

		aee_rr_rec_hotplug_footprint(cpu, 88);
		msleep(10);
		pr_info("Retrying again to check for CPU kill\n");
	}

	aee_rr_rec_hotplug_footprint(cpu, 89);
	pr_warn("CPU%d may not have shut down cleanly (AFFINITY_INFO reports %d)\n",
			cpu, err);
	/* Make op_cpu_kill() fail. */
	return 0;
}
#endif
#endif

static int psci_suspend_finisher(unsigned long index)
{
	struct psci_power_state *state = __get_cpu_var(psci_power_state);

#ifdef CONFIG_MTK_HIBERNATION
	if (index == POWERMODE_HIBERNATE) {
		int ret;

		pr_warn("%s: hibernating\n", __func__);
		ret = swsusp_arch_save_image(0);
		if (ret)
			pr_err("%s: swsusp_arch_save_image fail: %d", __func__, ret);
		return ret;
	}
#endif
	return psci_ops.cpu_suspend(state[index - 1],
				    virt_to_phys(cpu_resume));
}

static int __maybe_unused cpu_psci_cpu_suspend(unsigned long index)
{
	int ret;
	struct psci_power_state *state = __get_cpu_var(psci_power_state);
	/*
	 * idle state index 0 corresponds to wfi, should never be called
	 * from the cpu_suspend operations
	 */
	if (WARN_ON_ONCE(!index))
		return -EINVAL;

#ifdef CONFIG_MTK_HIBERNATION
	if (index == POWERMODE_HIBERNATE)
		return __cpu_suspend(index, psci_suspend_finisher);
#endif
	if (state[index - 1].type == PSCI_POWER_STATE_TYPE_STANDBY)
		ret = psci_ops.cpu_suspend(state[index - 1], 0);
	else
		ret = __cpu_suspend(index, psci_suspend_finisher);

	return ret;
}

const struct cpu_operations cpu_psci_ops = {
	.name		= "psci",
#ifdef CONFIG_CPU_IDLE
	.cpu_init_idle	= cpu_psci_cpu_init_idle,
	.cpu_suspend	= cpu_psci_cpu_suspend,
#endif
#ifdef CONFIG_SMP
	.cpu_init	= cpu_psci_cpu_init,
	.cpu_prepare	= cpu_psci_cpu_prepare,
	.cpu_boot	= cpu_psci_cpu_boot,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable	= cpu_psci_cpu_disable,
	.cpu_die	= cpu_psci_cpu_die,
	.cpu_kill	= cpu_psci_cpu_kill,
#endif
#endif
};

