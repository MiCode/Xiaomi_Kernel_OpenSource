#include <linux/init.h>
#include <linux/smp.h>

#include <asm/cpu_ops.h>
#include <asm/psci.h>
#include <mach/mt_spm_mtcmos.h>
#ifdef CONFIG_MTK_HIBERNATION
#include <mach/mt_hibernate.h>
#endif

extern const struct cpu_operations cpu_psci_ops;
extern int spm_mtcmos_cpu_init(void);
#ifdef CONFIG_SMP

static int __init mt_psci_cpu_init(struct device_node *dn, unsigned int cpu)
{
	return 0;
}

static int __init mt_psci_cpu_prepare(unsigned int cpu)
{
	if (cpu == 1)
		spm_mtcmos_cpu_init();
	return cpu_psci_ops.cpu_prepare(cpu);
}

static int mt_psci_cpu_boot(unsigned int cpu)
{
	int ret;
	ret = cpu_psci_ops.cpu_boot(cpu);
	if (ret < 0)
		return ret;

	return spm_mtcmos_ctrl_cpu(cpu, STA_POWER_ON, 1);
}

#ifdef CONFIG_HOTPLUG_CPU
static int mt_psci_cpu_disable(unsigned int cpu)
{
	return cpu_psci_ops.cpu_disable(cpu);
}

static void mt_psci_cpu_die(unsigned int cpu)
{
	cpu_psci_ops.cpu_die(cpu);
}

static int mt_psci_cpu_kill(unsigned int cpu)
{
	int ret;
	ret = cpu_psci_ops.cpu_kill(cpu);
	if (!ret)
		pr_warn("CPU%d may not have shut down cleanly\n", cpu);

	return !spm_mtcmos_ctrl_cpu(cpu, STA_POWER_DOWN, 1);
}

#ifdef CONFIG_ARM64_CPU_SUSPEND

#if defined(MT_CPUIDLE)
extern int mt_cpu_dormant_psci(unsigned long flags);
#else //#if defined(MT_CPUIDLE)
__weak int mt_cpu_dormant_psci(unsigned long flags)
{
        return 0;
}
#endif //#if defined(MT_CPUIDLE)

static int mt_psci_cpu_suspend(unsigned long flags)
{
	int ret;

#ifdef CONFIG_MTK_HIBERNATION
	if (flags == POWERMODE_HIBERNATE) {
		pr_warn("[%s] hibernating\n", __func__);
		return swsusp_arch_save_image(0);
	}
#endif
	extern int mt_cpu_dormant_psci(unsigned long flags);
	return mt_cpu_dormant_psci(flags);
}

#endif

#endif

const struct cpu_operations mt_cpu_psci_ops = {
	.name		= "mt-boot",
	.cpu_init	= mt_psci_cpu_init,
	.cpu_prepare	= mt_psci_cpu_prepare,
	.cpu_boot	= mt_psci_cpu_boot,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable	= mt_psci_cpu_disable,
	.cpu_die	= mt_psci_cpu_die,
	.cpu_kill	= mt_psci_cpu_kill,
#endif
#ifdef CONFIG_ARM64_CPU_SUSPEND
	.cpu_suspend = mt_psci_cpu_suspend,
#endif
};

#endif
