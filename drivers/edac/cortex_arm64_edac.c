/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>
#include <asm/cputype.h>

#include "edac_core.h"

#define A53_CPMUERRSR_FATAL(a)	((a) & (1 << 63))
#define A53_CPUMERRSR_OTHER(a)	(((a) >> 40) & 0xff)
#define A53_CPUMERRSR_REPT(a)	(((a) >> 32) & 0xff)
#define A53_CPUMERRSR_VALID(a)	((a) & (1 << 31))
#define A53_CPUMERRSR_RAMID(a)	(((a) >> 24) & 0x7f)
#define A53_CPUMERRSR_CPUID(a)	(((a) >> 18) & 0x07)
#define A53_CPUMERRSR_ADDR(a)	((a) & 0xfff)

#define A53_L2MERRSR_FATAL(a)	((a) & (1 << 63))
#define A53_L2MERRSR_OTHER(a)	(((a) >> 40) & 0xff)
#define A53_L2MERRSR_REPT(a)	(((a) >> 32) & 0xff)
#define A53_L2MERRSR_VALID(a)	((a) & (1 << 31))
#define A53_L2MERRSR_RAMID(a)	(((a) >> 24) & 0x7f)
#define A53_L2MERRSR_CPUID(a)	(((a) >> 18) & 0x0f)
#define A53_L2MERRSR_INDEX(a)	(((a) >> 3) & 0x3fff)

#define A57_CPMUERRSR_FATAL(a)	((a) & (1 << 63))
#define A57_CPUMERRSR_OTHER(a)	(((a) >> 40) & 0xff)
#define A57_CPUMERRSR_REPT(a)	(((a) >> 32) & 0xff)
#define A57_CPUMERRSR_VALID(a)	((a) & (1 << 31))
#define A57_CPUMERRSR_RAMID(a)	(((a) >> 24) & 0x7f)
#define A57_CPUMERRSR_BANK(a)	(((a) >> 18) & 0x1f)
#define A57_CPUMERRSR_INDEX(a)	((a) & 0x1ffff)

#define A57_L2MERRSR_FATAL(a)	((a) & (1 << 63))
#define A57_L2MERRSR_OTHER(a)	(((a) >> 40) & 0xff)
#define A57_L2MERRSR_REPT(a)	(((a) >> 32) & 0xff)
#define A57_L2MERRSR_VALID(a)	((a) & (1 << 31))
#define A57_L2MERRSR_RAMID(a)	(((a) >> 24) & 0x7f)
#define A57_L2MERRSR_CPUID(a)	(((a) >> 18) & 0x0f)
#define A57_L2MERRSR_INDEX(a)	((a) & 0x1ffff)

#define L2ECTLR_INT_ERR		(1 << 30)
#define L2ECTLR_EXT_ERR		(1 << 29)

#define CCI_IMPRECISEERROR_REG	0x10

#define L1_CACHE		0
#define L2_CACHE		1
#define CCI		1

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

#define EDAC_CPU	"arm64"

struct erp_drvdata {
	struct edac_device_ctl_info *edev_ctl;
	void __iomem *cci_base;
};

static struct erp_drvdata *abort_handler_drvdata;

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
	u32 __val;							\
	asm("mrs %0, s3_1_c15_c2_2" : "=r" (__val));			\
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

static void ca53_parse_cpumerrsr(struct edac_device_ctl_info *edev_ctl)
{
	u64 cpumerrsr;
	int cpuid;

	cpumerrsr = read_cpumerrsr_el1;

	if (!A53_CPUMERRSR_VALID(cpumerrsr))
		return;

	edac_printk(KERN_CRIT, EDAC_CPU, "Cortex A53 CPU%d L1 Double-bit Error detected\n",
						 smp_processor_id());
	edac_printk(KERN_CRIT, EDAC_CPU, "CPUMERRSR value = %llx\n", cpumerrsr);

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

	errors[A53_L1_UE].func(edev_ctl, cpuid, L1_CACHE,
				errors[A53_L1_UE].msg);
	write_cpumerrsr_el1(0);
}

static void ca53_parse_l2merrsr(struct edac_device_ctl_info *edev_ctl)
{
	u64 l2merrsr;
	u32 l2ectlr;
	int cpuid;

	l2merrsr = read_l2merrsr_el1;
	l2ectlr = read_l2ectlr_el1;

	if (!A53_L2MERRSR_VALID(l2merrsr))
		return;

	edac_printk(KERN_CRIT, EDAC_CPU, "CortexA53 L2 Double-bit Error detected\n");
	edac_printk(KERN_CRIT, EDAC_CPU, "L2MERRSR value = %llx\n", l2merrsr);

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

	errors[A53_L2_UE].func(edev_ctl, 0, L2_CACHE, errors[A53_L2_UE].msg);
	write_l2merrsr_el1(0);
}

static void ca57_parse_cpumerrsr(struct edac_device_ctl_info *edev_ctl)
{
	u64 cpumerrsr;
	int bank;

	cpumerrsr = read_cpumerrsr_el1;

	if (!A57_CPUMERRSR_VALID(cpumerrsr))
		return;

	edac_printk(KERN_CRIT, EDAC_CPU, "Cortex A57 CPU%d L1 Double-bit Error detected\n",
						 smp_processor_id());
	edac_printk(KERN_CRIT, EDAC_CPU, "CPUMERRSR value = %llx\n", cpumerrsr);

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

	errors[A57_L1_UE].func(edev_ctl, bank, L1_CACHE,
				errors[A57_L1_UE].msg);
	write_cpumerrsr_el1(0);
}

static void ca57_parse_l2merrsr(struct edac_device_ctl_info *edev_ctl)
{
	u64 l2merrsr;
	u32 l2ectlr;
	int cpuid;

	l2merrsr = read_l2merrsr_el1;
	l2ectlr = read_l2ectlr_el1;

	if (!A57_L2MERRSR_VALID(l2merrsr))
		return;

	edac_printk(KERN_CRIT, EDAC_CPU, "CortexA57 L2 Double-bit Error detected\n");
	edac_printk(KERN_CRIT, EDAC_CPU, "L2MERRSR value = %llx\n", l2merrsr);

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

	errors[A57_L2_UE].func(edev_ctl, 0, L2_CACHE, errors[A57_L2_UE].msg);
	write_l2merrsr_el1(0);
}

static DEFINE_SPINLOCK(local_handler_lock);
static DEFINE_SPINLOCK(l2ectlr_lock);

static void arm64_dbe_local_handler(void *info)
{
	struct erp_drvdata *drv = info;
	unsigned int cpuid = read_cpuid_id();
	unsigned int partnum = read_cpuid_part_number();
	unsigned long flags, flags2;
	u32 l2ectlr;

	spin_lock_irqsave(&local_handler_lock, flags);
	edac_printk(KERN_CRIT, EDAC_CPU, "Double-bit error information from CPU %d, MIDR=%08x:\n",
	       raw_smp_processor_id(), cpuid);

	switch (partnum) {
	case ARM_CPU_PART_CORTEX_A53:
		ca53_parse_cpumerrsr(drv->edev_ctl);
		ca53_parse_l2merrsr(drv->edev_ctl);
	break;

	case ARM_CPU_PART_CORTEX_A57:
		ca57_parse_cpumerrsr(drv->edev_ctl);
		ca57_parse_l2merrsr(drv->edev_ctl);
	break;

	default:
		edac_printk(KERN_CRIT, EDAC_CPU, "Unknown CPU Part Number in MIDR: %04x (%08x)\n",
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
	edac_printk(KERN_CRIT, EDAC_CPU, "Double-bit error interrupt received!\n");

	on_each_cpu(arm64_dbe_local_handler, drvdata, 1);

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

		errors[L2_EXT_UE].func(drv->edev_ctl, 0, L2_CACHE,
				errors[L2_EXT_UE].msg);

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

		edac_printk(KERN_CRIT, EDAC_CPU, "CCI imprecise error register: %08x.\n",
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

void arm64_erp_local_dbe_handler(void)
{
	if (abort_handler_drvdata)
		arm64_dbe_local_handler(abort_handler_drvdata);
}

static int request_erp_irq(struct platform_device *pdev, const char *propname,
			   const char *desc, irq_handler_t handler,
			   void *edac_dev)
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
				       edac_dev);

	if (rc) {
		pr_err("ARM64 CPU ERP: Failed to request IRQ %d: %d (%s / %s). Proceeding anyway.\n",
		       (int) r->start, rc, propname, desc);
		return -EINVAL;
	}

	return 0;
}

static int arm64_cpu_erp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct erp_drvdata *drv;
	struct resource *r;

	int rc, fail = 0;

	drv = devm_kzalloc(dev, sizeof(*drv), GFP_KERNEL);

	if (!drv)
		return -ENOMEM;

	drv->edev_ctl = edac_device_alloc_ctl_info(0, "cpu", ARRAY_SIZE(errors),
				"L", 3, 1, NULL, 0, edac_device_alloc_index());

	if (!drv->edev_ctl)
		return -ENOMEM;

	drv->edev_ctl->dev = dev;
	drv->edev_ctl->mod_name = dev_name(dev);
	drv->edev_ctl->dev_name = dev_name(dev);
	drv->edev_ctl->ctl_name = "cache";

	rc = edac_device_add_device(drv->edev_ctl);
	if (rc)
		goto out_mem;

	drv->edev_ctl->panic_on_ue = ARM64_ERP_PANIC_ON_UE;

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

	if (fail == 5) {
		pr_err("ARM64 CPU ERP: Could not request any IRQs. Giving up.\n");
		rc = -ENODEV;
		goto out_mem;
	}

	/*
	 * abort_handler_drvdata points to erp_drvdata structure used for
	 * reporting information on double-bit errors. There should only ever
	 * be one.
	 * */
	WARN_ON(abort_handler_drvdata);

	abort_handler_drvdata = drv;
	return 0;

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

subsys_initcall(arm64_cpu_erp_init);
