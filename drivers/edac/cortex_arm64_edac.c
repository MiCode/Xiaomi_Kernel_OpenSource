/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/edac.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/perf_event.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/percpu.h>
#include <soc/qcom/cti-pmu-irq.h>
#include <linux/msm_rtb.h>

#include <asm/cputype.h>
#include <asm/esr.h>

#include "edac_core.h"

#define A53_CPUMERRSR_FATAL(a)	((a) & (1LL << 63))
#define A53_CPUMERRSR_OTHER(a)	(((a) >> 40) & 0xff)
#define A53_CPUMERRSR_REPT(a)	(((a) >> 32) & 0xff)
#define A53_CPUMERRSR_VALID(a)	((a) & (1 << 31))
#define A53_CPUMERRSR_RAMID(a)	(((a) >> 24) & 0x7f)
#define A53_CPUMERRSR_CPUID(a)	(((a) >> 18) & 0x07)
#define A53_CPUMERRSR_ADDR(a)	((a) & 0xfff)

#define A53_L2MERRSR_FATAL(a)	((a) & (1LL << 63))
#define A53_L2MERRSR_OTHER(a)	(((a) >> 40) & 0xff)
#define A53_L2MERRSR_REPT(a)	(((a) >> 32) & 0xff)
#define A53_L2MERRSR_VALID(a)	((a) & (1 << 31))
#define A53_L2MERRSR_RAMID(a)	(((a) >> 24) & 0x7f)
#define A53_L2MERRSR_CPUID(a)	(((a) >> 18) & 0x0f)
#define A53_L2MERRSR_INDEX(a)	(((a) >> 3) & 0x3fff)

#define A57_CPUMERRSR_FATAL(a)	((a) & (1LL << 63))
#define A57_CPUMERRSR_OTHER(a)	(((a) >> 40) & 0xff)
#define A57_CPUMERRSR_REPT(a)	(((a) >> 32) & 0xff)
#define A57_CPUMERRSR_VALID(a)	((a) & (1 << 31))
#define A57_CPUMERRSR_RAMID(a)	(((a) >> 24) & 0x7f)
#define A57_CPUMERRSR_BANK(a)	(((a) >> 18) & 0x1f)
#define A57_CPUMERRSR_INDEX(a)	((a) & 0x1ffff)

#define A57_L2MERRSR_FATAL(a)	((a) & (1LL << 63))
#define A57_L2MERRSR_OTHER(a)	(((a) >> 40) & 0xff)
#define A57_L2MERRSR_REPT(a)	(((a) >> 32) & 0xff)
#define A57_L2MERRSR_VALID(a)	((a) & (1 << 31))
#define A57_L2MERRSR_RAMID(a)	(((a) >> 24) & 0x7f)
#define A57_L2MERRSR_CPUID(a)	(((a) >> 18) & 0x0f)
#define A57_L2MERRSR_INDEX(a)	((a) & 0x1ffff)

#define L2ECTLR_INT_ERR		(1 << 30)
#define L2ECTLR_EXT_ERR		(1 << 29)

#define ESR_SERROR(a)	((a) >> ESR_EL1_EC_SHIFT == ESR_EL1_EC_SERROR)
#define ESR_VALID(a)	((a) & BIT(24))
#define ESR_L2_DBE(a) (ESR_SERROR(a) && ESR_VALID(a) && \
			(((a) & 0x00C00003) == 0x1))

#define CCI_IMPRECISEERROR_REG	0x10

#define L1_CACHE		0
#define L2_CACHE		1
#define CCI			2

#define A53_L1_CE			0
#define A53_L1_UE			1
#define A53_L2_CE			2
#define A53_L2_UE			3
#define A57_L1_CE			4
#define A57_L1_UE			5
#define A57_L2_CE			6
#define A57_L2_UE			7
#define L2_EXT_UE			8
#define CCI_UE				9

#ifdef CONFIG_EDAC_CORTEX_ARM64_PANIC_ON_UE
#define ARM64_ERP_PANIC_ON_UE 1
#else
#define ARM64_ERP_PANIC_ON_UE 0
#endif

#ifdef CONFIG_EDAC_CORTEX_ARM64_PANIC_ON_CE
static int panic_on_ce = 1;
#else
static int panic_on_ce;
#endif
module_param(panic_on_ce, int, 0);

#define EDAC_CPU	"arm64"

enum error_type {
	SBE,
	DBE,
};

const char *err_name[] = {
	"Single-bit",
	"Double-bit",
};

struct erp_drvdata {
	struct edac_device_ctl_info *edev_ctl;
	void __iomem *cci_base;
	u32 mem_perf_counter;
	struct notifier_block nb_pm;
	struct notifier_block nb_cpu_wa;
	struct notifier_block nb_cpu;
	struct notifier_block nb_panic;
	struct work_struct work;
	int apply_cti_pmu_wa;
	unsigned int sbe_irq;
};

static struct erp_drvdata *panic_handler_drvdata;

struct erp_local_data {
	struct erp_drvdata *drv;
	enum error_type err;
};

/* Reserving the last PMU counter for Memory Event for EDAC */
#define	ARMV8_PMCR_N_SHIFT	11	 /* Number of counters supported */
#define	ARMV8_PMCR_N_MASK	0x1f
#define ARMV8PMU_EVTYPE_NSH	BIT(27)
#define ARMV8_PMCR_E		(1 << 0) /* Enable all counters */
#define MEM_ERROR_EVENT		0x1A
#define MAX_COUNTER_VALUE	0xFFFFFFFF

static inline void sbe_enable_event(void *info);

struct errors_edac {
	const char * const msg;
	void (*func)(struct edac_device_ctl_info *edac_dev,
			int inst_nr, int block_nr, const char *msg);
};

static const struct errors_edac errors[] = {
	{"A53 L1 Correctable Error", edac_device_handle_ce },
	{"A53 L1 Uncorrectable Error", edac_device_handle_ue },
	{"A53 L2 Correctable Error", edac_device_handle_ce },
	{"A53 L2 Uncorrectable Error", edac_device_handle_ue },
	{"A57 L1 Correctable Error", edac_device_handle_ce },
	{"A57 L1 Uncorrectable Error", edac_device_handle_ue },
	{"A57 L2 Correctable Error", edac_device_handle_ce },
	{"A57 L2 Uncorrectable Error", edac_device_handle_ue },
	{"L2 External Error", edac_device_handle_ue },
	{"CCI Error", edac_device_handle_ue },
};

#define read_l2merrsr_el1 ({                                           \
	u64 __val;                                                     \
	asm("mrs %0, s3_1_c15_c2_3" : "=r" (__val));                  \
	__val;                                                         \
})

#define read_l2ectlr_el1 ({						\
	u32 __val;							\
	asm("mrs %0, s3_1_c11_c0_3" : "=r" (__val));			\
	__val;								\
})

#define read_cpumerrsr_el1 ({						\
	u64 __val;							\
	asm("mrs %0, s3_1_c15_c2_2" : "=r" (__val));			\
	__val;								\
})

#define read_esr_el1 ({							\
	u64 __val;							\
	asm("mrs %0, esr_el1" : "=r" (__val));				\
	__val;								\
})

#define write_l2merrsr_el1(val) ({					\
	asm("msr s3_1_c15_c2_3, %0" : : "r" (val));			\
})

#define write_l2ectlr_el1(val) ({					\
	asm("msr s3_1_c11_c0_3, %0" : : "r" (val));			\
})

#define write_cpumerrsr_el1(val) ({					\
	asm("msr s3_1_c15_c2_2, %0" : : "r" (val));			\
})

static void ca53_ca57_print_error_state_regs(void)
{
	u64 l2merrsr;
	u64 cpumerrsr;
	u32 esr_el1;
	u32 l2ectlr;

	cpumerrsr = read_cpumerrsr_el1;
	l2merrsr = read_l2merrsr_el1;
	esr_el1 = read_esr_el1;
	l2ectlr = read_l2ectlr_el1;

	/* store data in uncached rtb logs */
	uncached_logk_pc(LOGK_READL, __builtin_return_address(0),
				(void *)cpumerrsr);
	uncached_logk_pc(LOGK_READL, __builtin_return_address(0),
				(void *)l2merrsr);
	uncached_logk_pc(LOGK_READL, __builtin_return_address(0),
				(void *)((u64)esr_el1));
	uncached_logk_pc(LOGK_READL, __builtin_return_address(0),
				(void *)((u64)l2ectlr));

	edac_printk(KERN_CRIT, EDAC_CPU, "CPUMERRSR value = %#llx\n",
								cpumerrsr);
	edac_printk(KERN_CRIT, EDAC_CPU, "L2MERRSR value = %#llx\n", l2merrsr);

	edac_printk(KERN_CRIT, EDAC_CPU, "ESR value = %#x\n", esr_el1);
	edac_printk(KERN_CRIT, EDAC_CPU, "L2ECTLR value = %#x\n", l2ectlr);
	if (ESR_L2_DBE(esr_el1))
		edac_printk(KERN_CRIT, EDAC_CPU,
			"Double bit error on dirty L2 cacheline\n");
}

static void ca53_parse_cpumerrsr(struct erp_local_data *ed)
{
	u64 cpumerrsr;
	int cpuid;

	cpumerrsr = read_cpumerrsr_el1;

	if (!A53_CPUMERRSR_VALID(cpumerrsr))
		return;

	if (A53_CPUMERRSR_FATAL(cpumerrsr))
		ed->err = DBE;

	edac_printk(KERN_CRIT, EDAC_CPU, "Cortex A53 CPU%d L1 %s Error detected\n",
					 smp_processor_id(), err_name[ed->err]);
	ca53_ca57_print_error_state_regs();
	if (ed->err == DBE)
		edac_printk(KERN_CRIT, EDAC_CPU, "Fatal error\n");

	cpuid = A53_CPUMERRSR_CPUID(cpumerrsr);

	switch (A53_CPUMERRSR_RAMID(cpumerrsr)) {
	case 0x0:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L1 Instruction tag RAM way is %d\n", cpuid);
		break;
	case 0x1:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L1 Instruction data RAM bank is %d\n", cpuid);
		break;
	case 0x8:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L1 Data tag RAM cpu %d way is %d\n",
				cpuid / 4, cpuid % 4);
		break;
	case 0x9:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L1 Data data RAM cpu %d way is %d\n",
				cpuid / 4, cpuid % 4);
		break;
	case 0xA:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L1 Data dirty RAM cpu %d way is %d\n",
				cpuid / 4, cpuid % 4);
		break;
	case 0x18:
		edac_printk(KERN_CRIT, EDAC_CPU, "TLB RAM way is %d\n", cpuid);
		break;
	default:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"Error in unknown RAM ID: %d\n",
				(int) A53_CPUMERRSR_RAMID(cpumerrsr));
		break;
	}

	edac_printk(KERN_CRIT, EDAC_CPU, "Repeated error count: %d\n",
					 (int) A53_CPUMERRSR_REPT(cpumerrsr));
	edac_printk(KERN_CRIT, EDAC_CPU, "Other error count: %d\n",
					 (int) A53_CPUMERRSR_OTHER(cpumerrsr));

	if (ed->err == SBE)
		errors[A53_L1_CE].func(ed->drv->edev_ctl, smp_processor_id(),
					L1_CACHE, errors[A53_L1_CE].msg);
	else if (ed->err == DBE)
		errors[A53_L1_UE].func(ed->drv->edev_ctl, smp_processor_id(),
					L1_CACHE, errors[A53_L1_UE].msg);
	write_cpumerrsr_el1(0);
}

static void ca53_parse_l2merrsr(struct erp_local_data *ed)
{
	u64 l2merrsr;
	u32 l2ectlr;
	int cpuid;

	l2merrsr = read_l2merrsr_el1;
	l2ectlr = read_l2ectlr_el1;

	if (!A53_L2MERRSR_VALID(l2merrsr))
		return;

	if (A53_L2MERRSR_FATAL(l2merrsr))
		ed->err = DBE;

	edac_printk(KERN_CRIT, EDAC_CPU, "CortexA53 L2 %s Error detected\n",
							err_name[ed->err]);
	ca53_ca57_print_error_state_regs();
	if (ed->err == DBE)
		edac_printk(KERN_CRIT, EDAC_CPU, "Fatal error\n");

	cpuid = A53_L2MERRSR_CPUID(l2merrsr);

	switch (A53_L2MERRSR_RAMID(l2merrsr)) {
	case 0x10:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L2 tag RAM way is %d\n", cpuid);
		break;
	case 0x11:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L2 data RAM bank is %d\n", cpuid);
		break;
	case 0x12:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"SCU snoop filter RAM cpu %d way is %d\n",
				cpuid / 4, cpuid % 4);
		break;
	default:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"Error in unknown RAM ID: %d\n",
				(int) A53_L2MERRSR_RAMID(l2merrsr));
		break;
	}

	edac_printk(KERN_CRIT, EDAC_CPU, "Repeated error count: %d\n",
					 (int) A53_L2MERRSR_REPT(l2merrsr));
	edac_printk(KERN_CRIT, EDAC_CPU, "Other error count: %d\n",
					 (int) A53_L2MERRSR_OTHER(l2merrsr));

	if (ed->err == SBE)
		errors[A53_L2_CE].func(ed->drv->edev_ctl, smp_processor_id(),
					L2_CACHE, errors[A53_L2_CE].msg);
	else if (ed->err == DBE)
		errors[A53_L2_UE].func(ed->drv->edev_ctl, smp_processor_id(),
					L2_CACHE, errors[A53_L2_UE].msg);
	write_l2merrsr_el1(0);
}


static void ca57_parse_cpumerrsr(struct erp_local_data *ed)
{
	u64 cpumerrsr;
	int bank;

	cpumerrsr = read_cpumerrsr_el1;

	if (!A57_CPUMERRSR_VALID(cpumerrsr))
		return;

	if (A57_CPUMERRSR_FATAL(cpumerrsr))
		ed->err = DBE;

	edac_printk(KERN_CRIT, EDAC_CPU, "Cortex A57 CPU%d L1 %s Error detected\n",
					 smp_processor_id(), err_name[ed->err]);
	ca53_ca57_print_error_state_regs();
	if (ed->err == DBE)
		edac_printk(KERN_CRIT, EDAC_CPU, "Fatal error\n");

	bank = A57_CPUMERRSR_BANK(cpumerrsr);

	switch (A57_CPUMERRSR_RAMID(cpumerrsr)) {
	case 0x0:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L1 Instruction tag RAM bank %d\n", bank);
		break;
	case 0x1:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L1 Instruction data RAM bank %d\n", bank);
		break;
	case 0x8:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L1 Data tag RAM bank %d\n", bank);
		break;
	case 0x9:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L1 Data data RAM bank %d\n", bank);
		break;
	case 0x18:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"TLB RAM bank %d\n", bank);
		break;
	default:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"Error in unknown RAM ID: %d\n",
				(int) A57_CPUMERRSR_RAMID(cpumerrsr));
		break;
	}

	edac_printk(KERN_CRIT, EDAC_CPU, "Repeated error count: %d\n",
					 (int) A57_CPUMERRSR_REPT(cpumerrsr));
	edac_printk(KERN_CRIT, EDAC_CPU, "Other error count: %d\n",
					 (int) A57_CPUMERRSR_OTHER(cpumerrsr));

	if (ed->err == SBE)
		errors[A57_L1_CE].func(ed->drv->edev_ctl, smp_processor_id(),
					L1_CACHE, errors[A57_L1_CE].msg);
	else if (ed->err == DBE)
		errors[A57_L1_UE].func(ed->drv->edev_ctl, smp_processor_id(),
					L1_CACHE, errors[A57_L1_UE].msg);
	write_cpumerrsr_el1(0);
}

static void ca57_parse_l2merrsr(struct erp_local_data *ed)
{
	u64 l2merrsr;
	u32 l2ectlr;
	int cpuid;

	l2merrsr = read_l2merrsr_el1;
	l2ectlr = read_l2ectlr_el1;

	if (!A57_L2MERRSR_VALID(l2merrsr))
		return;

	if (A57_L2MERRSR_FATAL(l2merrsr))
		ed->err = DBE;

	edac_printk(KERN_CRIT, EDAC_CPU, "CortexA57 L2 %s Error detected\n",
							err_name[ed->err]);
	ca53_ca57_print_error_state_regs();
	if (ed->err == DBE)
		edac_printk(KERN_CRIT, EDAC_CPU, "Fatal error\n");

	cpuid = A57_L2MERRSR_CPUID(l2merrsr);

	switch (A57_L2MERRSR_RAMID(l2merrsr)) {
	case 0x10:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L2 tag RAM cpu %d way is %d\n",
				cpuid / 2, cpuid % 2);
		break;
	case 0x11:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L2 data RAM cpu %d bank is %d\n",
				cpuid / 2, cpuid % 2);
		break;
	case 0x12:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"SCU snoop tag RAM bank is %d\n", cpuid);
		break;
	case 0x14:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L2 dirty RAM cpu %d bank is %d\n",
				cpuid / 2, cpuid % 2);
		break;
	case 0x18:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"L2 inclusion PF RAM bank is %d\n", cpuid);
		break;
	default:
		edac_printk(KERN_CRIT, EDAC_CPU,
				"Error in unknown RAM ID: %d\n",
				(int) A57_L2MERRSR_RAMID(l2merrsr));
		break;
	}

	edac_printk(KERN_CRIT, EDAC_CPU, "Repeated error count: %d\n",
					 (int) A57_L2MERRSR_REPT(l2merrsr));
	edac_printk(KERN_CRIT, EDAC_CPU, "Other error count: %d\n",
					 (int) A57_L2MERRSR_OTHER(l2merrsr));

	if (ed->err == SBE) {
		errors[A57_L2_CE].func(ed->drv->edev_ctl, smp_processor_id(),
					L2_CACHE, errors[A57_L2_CE].msg);
	} else if (ed->err == DBE) {
		errors[A57_L2_UE].func(ed->drv->edev_ctl, smp_processor_id(),
					L2_CACHE, errors[A57_L2_UE].msg);
	}
	write_l2merrsr_el1(0);
}

static DEFINE_SPINLOCK(local_handler_lock);
static DEFINE_SPINLOCK(l2ectlr_lock);

static void arm64_erp_local_handler(void *info)
{
	struct erp_local_data *errdata = info;
	unsigned int cpuid = read_cpuid_id();
	unsigned int partnum = read_cpuid_part_number();
	unsigned long flags, flags2;
	u32 l2ectlr;

	spin_lock_irqsave(&local_handler_lock, flags);
	edac_printk(KERN_CRIT, EDAC_CPU, "%s error information from CPU %d, MIDR=%#08x:\n",
		       err_name[errdata->err], raw_smp_processor_id(), cpuid);

	switch (partnum) {
	case ARM_CPU_PART_CORTEX_A53:
		ca53_parse_cpumerrsr(errdata);
		ca53_parse_l2merrsr(errdata);
	break;

	case ARM_CPU_PART_CORTEX_A57:
		ca57_parse_cpumerrsr(errdata);
		ca57_parse_l2merrsr(errdata);
	break;

	default:
		edac_printk(KERN_CRIT, EDAC_CPU, "Unknown CPU Part Number in MIDR: %#04x (%#08x)\n",
						 partnum, cpuid);
	};

	/* Acklowledge internal error in L2ECTLR */
	spin_lock_irqsave(&l2ectlr_lock, flags2);

	l2ectlr = read_l2ectlr_el1;

	if (l2ectlr & L2ECTLR_INT_ERR) {
		l2ectlr &= ~L2ECTLR_INT_ERR;
		write_l2ectlr_el1(l2ectlr);
	}

	spin_unlock_irqrestore(&l2ectlr_lock, flags2);
	spin_unlock_irqrestore(&local_handler_lock, flags);
}

static irqreturn_t arm64_dbe_handler(int irq, void *drvdata)
{
	struct erp_local_data errdata;

	errdata.drv = drvdata;
	errdata.err = DBE;
	edac_printk(KERN_CRIT, EDAC_CPU, "ARM64 CPU ERP: Double-bit error interrupt received!\n");

	on_each_cpu(arm64_erp_local_handler, &errdata, 1);

	return IRQ_HANDLED;
}

static void arm64_ext_local_handler(void *info)
{
	struct erp_drvdata *drv = info;
	unsigned long flags, flags2;
	u32 l2ectlr;

	spin_lock_irqsave(&local_handler_lock, flags);

	/* TODO: Shared locking for L2ECTLR access */
	spin_lock_irqsave(&l2ectlr_lock, flags2);

	l2ectlr = read_l2ectlr_el1;

	if (l2ectlr & L2ECTLR_EXT_ERR) {
		edac_printk(KERN_CRIT, EDAC_CPU,
		    "L2 external error detected by CPU%d\n",
		    smp_processor_id());

		errors[L2_EXT_UE].func(drv->edev_ctl, smp_processor_id(),
				       L2_CACHE, errors[L2_EXT_UE].msg);

		l2ectlr &= ~L2ECTLR_EXT_ERR;
		write_l2ectlr_el1(l2ectlr);
	}

	spin_unlock_irqrestore(&l2ectlr_lock, flags2);
	spin_unlock_irqrestore(&local_handler_lock, flags);
}

static irqreturn_t arm64_ext_handler(int irq, void *drvdata)
{
	edac_printk(KERN_CRIT, EDAC_CPU, "External error interrupt received!\n");

	on_each_cpu(arm64_ext_local_handler, drvdata, 1);

	return IRQ_HANDLED;
}

static irqreturn_t arm64_cci_handler(int irq, void *drvdata)
{
	struct erp_drvdata *drv = drvdata;
	u32 cci_err_reg;

	edac_printk(KERN_CRIT, EDAC_CPU, "CCI error interrupt received!\n");

	if (drv->cci_base) {
		cci_err_reg = readl_relaxed(drv->cci_base +
							CCI_IMPRECISEERROR_REG);

		edac_printk(KERN_CRIT, EDAC_CPU, "CCI imprecise error register: %#08x.\n",
						 cci_err_reg);

		/* This register has write-clear semantics */
		writel_relaxed(cci_err_reg, drv->cci_base +
							CCI_IMPRECISEERROR_REG);

		/* Ensure error bits cleared before exiting ISR */
		mb();
	} else {
		edac_printk(KERN_CRIT, EDAC_CPU, "CCI registers not available.\n");
	}

	errors[CCI_UE].func(drv->edev_ctl, 0, CCI, errors[CCI_UE].msg);

	return IRQ_HANDLED;
}

static inline u32 armv8pmu_pmcr_read(void)
{
	u32 val;
	asm volatile("mrs %0, pmcr_el0" : "=r" (val));
	return val;
}

static inline u32 arm64_pmu_get_last_counter(void)
{
	u32 pmcr, cntr;

	asm volatile("mrs %0, pmcr_el0" : "=r" (pmcr));
	cntr = (pmcr >> ARMV8_PMCR_N_SHIFT) & ARMV8_PMCR_N_MASK;

	return cntr-1;
}

static inline void arm64pmu_select_mem_counter(u32 cntr)
{
	asm volatile("msr pmselr_el0, %0" : : "r" (cntr));
	isb();
}

static inline u32 arm64pmu_getreset_flags(u32 cntr)
{
	u32 value;
	u32 write_val;

	/* Read */
	asm volatile("mrs %0, pmovsclr_el0" : "=r" (value));

	/* Write to clear flags */
	write_val = value & BIT(cntr);
	asm volatile("msr pmovsclr_el0, %0" : : "r" (write_val));

	return value;
}

static inline int arm64pmu_mem_counter_has_overflowed(u32 pmnc, u32 cntr)
{
	int ret = pmnc & BIT(cntr);
	return ret;
}

static inline void arm64pmu_disable_mem_counter(u32 cntr)
{
	asm volatile("msr pmcntenclr_el0, %0" : : "r" (BIT(cntr)));
}

static inline void arm64pmu_enable_mem_counter(u32 cntr)
{
	asm volatile("msr pmcntenset_el0, %0" : : "r" (BIT(cntr)));
}

static inline void arm64pmu_write_mem_counter(u32 value, u32 cntr)
{
	arm64pmu_select_mem_counter(cntr);
	asm volatile("msr pmxevcntr_el0, %0" : : "r" (value));
}

static inline void arm64pmu_set_mem_evtype(u32 cntr)
{
	u32 val = ARMV8PMU_EVTYPE_NSH | MEM_ERROR_EVENT;
	arm64pmu_select_mem_counter(cntr);
	asm volatile("msr pmxevtyper_el0, %0" : : "r" (val));
}

static inline void arm64pmu_enable_mem_irq(u32 cntr)
{
	asm volatile("msr pmintenset_el1, %0" : : "r" (BIT(cntr)));
}

static inline void arm64pmu_pmcr_enable(void)
{
	u32 val = armv8pmu_pmcr_read();
	val |= ARMV8_PMCR_E;
	asm volatile("msr pmcr_el0, %0" : : "r" (val));
	isb();
}

/*
 * This function follows the sequence to enable
 * the memory event counter. Steps done here include
 * programming the counter for memory error perf event,
 * setting the initial counter value and enabling the interrupt
 * associated with the counter.
 */
static inline void sbe_enable_event(void *info)
{
	struct erp_drvdata *drv = info;
	unsigned long flags;
	u32 cntr = drv->mem_perf_counter;

	arm64_pmu_lock(NULL, &flags);
	arm64pmu_disable_mem_counter(cntr);
	arm64pmu_set_mem_evtype(cntr);
	arm64pmu_enable_mem_irq(cntr);
	arm64pmu_write_mem_counter(MAX_COUNTER_VALUE, cntr);
	arm64pmu_enable_mem_counter(cntr);
	arm64pmu_pmcr_enable();
	arm64_pmu_unlock(NULL, &flags);
}

static irqreturn_t arm64_sbe_handler(int irq, void *drvdata)
{
	u32 pmovsr, cntr;
	struct erp_local_data errdata;
	unsigned long flags;
	int overflow = 0, ret = IRQ_HANDLED;
	int cpu = raw_smp_processor_id();

	errdata.drv = *((struct erp_drvdata **)drvdata);

	if (errdata.drv->apply_cti_pmu_wa)
		msm_cti_pmu_irq_ack(cpu);

	cntr = errdata.drv->mem_perf_counter;
	arm64_pmu_lock(NULL, &flags);
	pmovsr = arm64pmu_getreset_flags(cntr);
	arm64_pmu_unlock(NULL, &flags);
	overflow = arm64pmu_mem_counter_has_overflowed(pmovsr, cntr);

	if (overflow) {
		errdata.err = SBE;
		edac_printk(KERN_CRIT, EDAC_CPU, "ARM64 CPU ERP: Single-bit error interrupt received on CPU %d!\n",
						cpu);
		WARN_ON(!panic_on_ce);
		arm64_erp_local_handler(&errdata);
		sbe_enable_event(errdata.drv);
	} else {
		ret = armv8pmu_handle_irq(irq, NULL);
	}

	return ret;
}

static int request_erp_irq(struct platform_device *pdev, const char *propname,
			   const char *desc, irq_handler_t handler,
			   void *ed)
{
	int rc;
	struct resource *r;

	r = platform_get_resource_byname(pdev, IORESOURCE_IRQ, propname);

	if (!r) {
		pr_err("ARM64 CPU ERP: Could not find <%s> IRQ property. Proceeding anyway.\n",
			propname);
		return -EINVAL;
	}

	rc = devm_request_threaded_irq(&pdev->dev, r->start, NULL,
				       handler,
				       IRQF_ONESHOT | IRQF_TRIGGER_RISING,
				       desc,
				       ed);

	if (rc) {
		pr_err("ARM64 CPU ERP: Failed to request IRQ %d: %d (%s / %s). Proceeding anyway.\n",
		       (int) r->start, rc, propname, desc);
		return -EINVAL;
	}

	return 0;
}

static void arm64_enable_pmu_irq(void *data)
{
	unsigned int irq = *(unsigned int *)data;
	enable_percpu_irq(irq, IRQ_TYPE_NONE);
}

static void arm64_disable_pmu_irq(void *data)
{
	unsigned int irq = *(unsigned int *)data;
	disable_percpu_irq(irq);
}

static void check_sbe_event(struct erp_drvdata *drv)
{
	unsigned int partnum = read_cpuid_part_number();
	struct erp_local_data errdata;
	unsigned long flags;

	errdata.drv = drv;
	errdata.err = SBE;

	spin_lock_irqsave(&local_handler_lock, flags);
	switch (partnum) {
	case ARM_CPU_PART_CORTEX_A53:
		ca53_parse_cpumerrsr(&errdata);
		ca53_parse_l2merrsr(&errdata);
	break;

	case ARM_CPU_PART_CORTEX_A57:
		ca57_parse_cpumerrsr(&errdata);
		ca57_parse_l2merrsr(&errdata);
	break;
	};
	spin_unlock_irqrestore(&local_handler_lock, flags);
}

static int arm64_pmu_cpu_pm_notify(struct notifier_block *self,
					   unsigned long action, void *v)
{
	struct erp_drvdata *drv = container_of(self, struct erp_drvdata, nb_pm);

	switch (action) {
	case CPU_PM_EXIT:
		check_sbe_event(drv);
		sbe_enable_event(drv);
		break;
	}

	return NOTIFY_OK;
}

static int msm_cti_pmu_wa_cpu_notify(struct notifier_block *self,
					unsigned long action, void *hcpu)
{
	struct erp_drvdata *drv = container_of(self, struct erp_drvdata,
								nb_cpu_wa);
	switch (action) {
	case CPU_ONLINE:
		schedule_work_on((unsigned long)hcpu, &drv->work);
		break;
	};

	return NOTIFY_OK;
}

static int arm64_edac_pmu_cpu_notify(struct notifier_block *self,
					unsigned long action, void *hcpu)
{
	struct erp_drvdata *drv = container_of(self, struct erp_drvdata,
								nb_cpu);
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_STARTING:
		sbe_enable_event(drv);
		arm64_enable_pmu_irq(&drv->sbe_irq);
		break;
	case CPU_DYING:
		arm64_disable_pmu_irq(&drv->sbe_irq);
		break;
	};

	return NOTIFY_OK;
}

#ifndef CONFIG_EDAC_CORTEX_ARM64_DBE_IRQ_ONLY
void arm64_check_cache_ecc(void *info)
{
	if (panic_handler_drvdata)
		check_sbe_event(panic_handler_drvdata);
}
#else
static inline void arm64_check_cache_ecc(void *info) {}
#endif

static int arm64_erp_panic_notify(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	arm64_check_cache_ecc(NULL);

	return NOTIFY_OK;
}

static void arm64_monitor_cache_errors(struct edac_device_ctl_info *edev)
{
	struct cpumask cluster_mask, old_mask;
	int cpu;

	cpumask_clear(&cluster_mask);
	cpumask_clear(&old_mask);

	for_each_possible_cpu(cpu) {
		cpumask_copy(&cluster_mask, topology_core_cpumask(cpu));
		if (cpumask_equal(&cluster_mask, &old_mask))
			continue;
		cpumask_copy(&old_mask, &cluster_mask);
		smp_call_function_any(&cluster_mask,
				arm64_check_cache_ecc, NULL, 0);
	}
}

static int arm64_cpu_erp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct erp_drvdata *drv;
	struct resource *r;
	int cpu;
	u32 poll_msec;
	struct erp_drvdata * __percpu *drv_cpu =
					alloc_percpu(struct erp_drvdata *);

	int rc, sbe_irq, fail = 0;

	drv = devm_kzalloc(dev, sizeof(*drv), GFP_KERNEL);

	if (!drv)
		return -ENOMEM;

	drv->edev_ctl = edac_device_alloc_ctl_info(0, "cpu",
					num_possible_cpus(), "L", 3, 1, NULL, 0,
					edac_device_alloc_index());

	if (!drv->edev_ctl)
		return -ENOMEM;

	rc = of_property_read_u32(pdev->dev.of_node, "poll-delay-ms",
							&poll_msec);
	if (!rc && !IS_ENABLED(CONFIG_EDAC_CORTEX_ARM64_DBE_IRQ_ONLY)) {
		drv->edev_ctl->edac_check = arm64_monitor_cache_errors;
		drv->edev_ctl->poll_msec = poll_msec;
		drv->edev_ctl->defer_work = 1;
	}
	drv->edev_ctl->dev = dev;
	drv->edev_ctl->mod_name = dev_name(dev);
	drv->edev_ctl->dev_name = dev_name(dev);
	drv->edev_ctl->ctl_name = "cache";
	drv->edev_ctl->panic_on_ce = panic_on_ce;
	drv->edev_ctl->panic_on_ue = ARM64_ERP_PANIC_ON_UE;

	rc = edac_device_add_device(drv->edev_ctl);
	if (rc)
		goto out_mem;

	r = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cci");
	if (r)
		drv->cci_base = devm_ioremap_resource(dev, r);

	if (request_erp_irq(pdev, "pri-dbe-irq", "ARM64 primary DBE IRQ",
			    arm64_dbe_handler, drv))
		fail++;

	if (request_erp_irq(pdev, "sec-dbe-irq", "ARM64 secondary DBE IRQ",
			    arm64_dbe_handler, drv))
		fail++;

	if (request_erp_irq(pdev, "pri-ext-irq", "ARM64 primary ext IRQ",
			    arm64_ext_handler, drv))
		fail++;

	if (request_erp_irq(pdev, "sec-ext-irq", "ARM64 secondary ext IRQ",
			    arm64_ext_handler, drv))
		fail++;

	/*
	 * We still try to register a handler for CCI errors even if we don't
	 * have access to cci_base, but error reporting becomes best-effort in
	 * that case.
	 */
	if (request_erp_irq(pdev, "cci-irq", "CCI error IRQ",
			    arm64_cci_handler, drv))
		fail++;

	if (!drv_cpu)
		goto out_irq;

	sbe_irq = platform_get_irq_byname(pdev, "sbe-irq");
	if (sbe_irq < 0 || IS_ENABLED(CONFIG_EDAC_CORTEX_ARM64_DBE_IRQ_ONLY)) {
		pr_err("ARM64 CPU ERP: Could not find sbe-irq IRQ property. Proceeding anyway.\n");
		fail++;
		goto out_irq;
	}

	for_each_possible_cpu(cpu)
		*per_cpu_ptr(drv_cpu, cpu) = drv;

	rc = request_percpu_irq(sbe_irq, arm64_sbe_handler,
			"ARM64 Single-Bit Error PMU IRQ",
			drv_cpu);
	if (rc) {
		pr_err("ARM64 CPU ERP: Failed to request IRQ %d: %d. Proceeding anyway.\n",
								sbe_irq, rc);
		goto out_irq;
	}
	drv->sbe_irq = sbe_irq;

	drv->apply_cti_pmu_wa = of_property_read_bool(pdev->dev.of_node,
						"qcom,apply-cti-pmu-wa");

	drv->nb_pm.notifier_call = arm64_pmu_cpu_pm_notify;
	drv->mem_perf_counter = arm64_pmu_get_last_counter();
	cpu_pm_register_notifier(&(drv->nb_pm));
	drv->nb_panic.notifier_call = arm64_erp_panic_notify;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &drv->nb_panic);
	arm64_pmu_irq_handled_externally();
	if (drv->apply_cti_pmu_wa) {
		drv->nb_cpu_wa.notifier_call = msm_cti_pmu_wa_cpu_notify;
		register_cpu_notifier(&drv->nb_cpu_wa);
		schedule_on_each_cpu(msm_enable_cti_pmu_workaround);
		INIT_WORK(&drv->work, msm_enable_cti_pmu_workaround);
	}
	drv->nb_cpu.notifier_call = arm64_edac_pmu_cpu_notify;
	register_cpu_notifier(&drv->nb_cpu);
	on_each_cpu(sbe_enable_event, drv, 1);
	on_each_cpu(arm64_enable_pmu_irq, &sbe_irq, 1);

out_irq:
	if (fail == of_irq_count(dev->of_node)) {
		pr_err("ARM64 CPU ERP: Could not request any IRQs. Giving up.\n");
		rc = -ENODEV;
		goto out_dev;
	}

	panic_handler_drvdata = drv;

	return 0;

out_dev:
	edac_device_del_device(dev);
out_mem:
	edac_device_free_ctl_info(drv->edev_ctl);
	return rc;
}

static const struct of_device_id arm64_cpu_erp_match_table[] = {
	{ .compatible = "arm,arm64-cpu-erp" },
	{ }
};

static struct platform_driver arm64_cpu_erp_driver = {
	.probe = arm64_cpu_erp_probe,
	.driver = {
		.name = "arm64_cpu_cache_erp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(arm64_cpu_erp_match_table),
	},
};

static int __init arm64_cpu_erp_init(void)
{
	return platform_driver_register(&arm64_cpu_erp_driver);
}
device_initcall_sync(arm64_cpu_erp_init);
