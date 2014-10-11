/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include "vmem.h"
#include "vmem_debugfs.h"

/* Registers */
#define OCIMEM_BASE(v)               ((uint8_t *)(v)->reg.base)
#define OCIMEM_HW_VERSION(v)         (OCIMEM_BASE(v) + 0x00)
#define OCIMEM_HW_PROFILE(v)         (OCIMEM_BASE(v) + 0x04)
#define OCIMEM_GEN_CTL(v)            (OCIMEM_BASE(v) + 0x08)
#define OCIMEM_GEN_STAT(v)           (OCIMEM_BASE(v) + 0x0C)
#define OCIMEM_INTC_CLR(v)           (OCIMEM_BASE(v) + 0x10)
#define OCIMEM_INTC_MASK(v)          (OCIMEM_BASE(v) + 0x14)
#define OCIMEM_INTC_STAT(v)          (OCIMEM_BASE(v) + 0x18)
#define OCIMEM_OSW_STATUS(v)         (OCIMEM_BASE(v) + 0x1C)
#define OCIMEM_PSCGC_TIMERS(v)       (OCIMEM_BASE(v) + 0x34)
#define OCIMEM_PSCGC_STAT(v)         (OCIMEM_BASE(v) + 0x38)
#define OCIMEM_PSCGC_M0_M7_CTL(v)    (OCIMEM_BASE(v) + 0x3C)
#define OCIMEM_ERR_ADDRESS(v)        (OCIMEM_BASE(v) + 0x60)
#define OCIMEM_AXI_ERR_SYNDROME(v)   (OCIMEM_BASE(v) + 0x64)
#define OCIMEM_DEBUG_CTL(v)          (OCIMEM_BASE(v) + 0x68)

/*
 * Helper macro to help out with masks and shifts for values packed into
 * registers.
 */
#define DECLARE_TYPE(__type, __end, __start)                                   \
	static const unsigned int __type##_BITS = (__end) - (__start) + 1;     \
	static const unsigned int __type##_SHIFT = (__start);                  \
	static const unsigned int __type##_MASK = GENMASK((__end), (__start)); \
	static inline unsigned int __type(uint32_t val)                        \
	{                                                                      \
		return (val & __type##_MASK) >> __type##_SHIFT;                \
	}                                                                      \
	static inline uint32_t __type##_UPDATE(unsigned int val)               \
	{                                                                      \
		return (val << __type##_SHIFT) & __type##_MASK;                \
	}

/* Register masks */
/* OCIMEM_PSCGC_M0_M7_CTL */
DECLARE_TYPE(BANK0_STATE, 3, 0)
DECLARE_TYPE(BANK1_STATE, 7, 4)
DECLARE_TYPE(BANK2_STATE, 11, 8)
DECLARE_TYPE(BANK3_STATE, 15, 12)
/* OCIMEM_PSCGC_TIMERS */
DECLARE_TYPE(TIMERS_WAKEUP, 3, 0)
DECLARE_TYPE(TIMERS_SLEEP, 11, 8)
/* OCIMEM_HW_VERSION */
DECLARE_TYPE(VERSION_STEP, 15, 0)
DECLARE_TYPE(VERSION_MINOR, 27, 16)
DECLARE_TYPE(VERSION_MAJOR, 31, 28)
/* OCIMEM_HW_PROFILE */
DECLARE_TYPE(PROFILE_BANKS, 16, 12)
/* OCIMEM_AXI_ERR_SYNDROME */
DECLARE_TYPE(ERR_SYN_ATID, 14, 8);
DECLARE_TYPE(ERR_SYN_AMID, 23, 16);
DECLARE_TYPE(ERR_SYN_APID, 28, 24);
DECLARE_TYPE(ERR_SYN_ABID, 31, 29);

/* Internal stuff */
#define MAX_BANKS 4
#define BYTES_PER_BANK SZ_256K

enum bank_state {
	BANK_STATE_NORM_PASSTHRU = 0b000,
	BANK_STATE_NORM_FORCE_CORE_ON = 0b010,
	BANK_STATE_NORM_FORCE_PERIPH_ON = 0b001,
	BANK_STATE_NORM_FORCE_ALL_ON = 0b011,
	BANK_STATE_SLEEP_RET = 0b110,
	BANK_STATE_SLEEP_RET_PERIPH_ON = 0b111,
	BANK_STATE_SLEEP_NO_RET = 0b100,
};

struct vmem {
	int irq;
	int num_banks;
	struct {
		struct resource *resource;
		void __iomem *base;
	} reg, mem;
	struct clk *maxi;
	struct clk *ahb;
	atomic_t alloc_count;
	struct dentry *debugfs_root;
};

static struct vmem *vmem;

static inline u32 __readl(void * __iomem addr)
{
	u32 value = 0;

	pr_debug("read %p ", addr);
	value = readl_relaxed(addr);
	pr_debug("-> %08x\n", value);

	return value;
}

static inline void __writel(u32 val, void * __iomem addr)
{
	pr_debug("write %08x -> %p\n", val, addr);
	writel_relaxed(val, addr);
	/*
	 * Commit all writes via a mem barrier, as subsequent __readl()
	 * will depend on the state that's set via __writel().
	 */
	mb();
}

static inline void __wait_timer(struct vmem *v, bool wakeup)
{
	uint32_t ticks = 0;
	unsigned int (*timer)(uint32_t) = wakeup ?
		TIMERS_WAKEUP : TIMERS_SLEEP;

	ticks = timer(__readl(OCIMEM_PSCGC_TIMERS(v)));

	/* Sleep for `ticks` nanoseconds as per h/w spec */
	ndelay(ticks);
}

static inline void __wait_wakeup(struct vmem *v)
{
	return __wait_timer(v, true);
}

static inline void __wait_sleep(struct vmem *v)
{
	return __wait_timer(v, false);
}

static inline int __enable_clocks(struct vmem *v)
{
	int rc = 0;

	rc = clk_prepare_enable(v->maxi);
	if (rc) {
		pr_err("Failed to enable vmem axi clock: %d\n", rc);
		goto exit;
	}

	rc = clk_prepare_enable(v->ahb);
	if (rc) {
		pr_err("Failed to enable vmem ahb clock: %d\n", rc);
		goto disable_ahb;
	}

	return 0;

disable_ahb:
	clk_disable_unprepare(v->maxi);
exit:
	return rc;
}

static inline int __disable_clocks(struct vmem *v)
{
	clk_disable_unprepare(v->ahb);
	clk_disable_unprepare(v->maxi);
	return 0;
}

static inline enum bank_state __bank_get_state(struct vmem *v,
		unsigned int bank)
{
	unsigned int (*func[MAX_BANKS])(uint32_t) = {
		BANK0_STATE, BANK1_STATE, BANK2_STATE, BANK3_STATE
	};

	BUG_ON(bank >= ARRAY_SIZE(func));
	return func[bank](__readl(OCIMEM_PSCGC_M0_M7_CTL(v)));
}

static inline void __bank_set_state(struct vmem *v, unsigned int bank,
		enum bank_state state)
{
	uint32_t bank_state = 0;
	struct {
		uint32_t (*update)(unsigned int);
		uint32_t mask;
	} banks[MAX_BANKS] = {
		{BANK0_STATE_UPDATE, BANK0_STATE_MASK},
		{BANK1_STATE_UPDATE, BANK1_STATE_MASK},
		{BANK2_STATE_UPDATE, BANK2_STATE_MASK},
		{BANK3_STATE_UPDATE, BANK3_STATE_MASK},
	};

	BUG_ON(bank >= ARRAY_SIZE(banks));

	bank_state = __readl(OCIMEM_PSCGC_M0_M7_CTL(v));
	bank_state &= ~banks[bank].mask;
	bank_state |= banks[bank].update(state);

	__writel(bank_state, OCIMEM_PSCGC_M0_M7_CTL(v));
}

/**
 * vmem_allocate: - Allocates memory from VMEM.  Allocations have a few
 * restrictions: only allocations of the entire VMEM memory are allowed, and
 * , as a result, only single outstanding allocations are allowed.
 *
 * @size: amount of bytes to allocate
 * @addr: A pointer to phys_addr_t where the physical address of the memory
 * allocated is stored.
 *
 * Return: 0 in case of successful allocation (i.e. *addr != NULL). -ENOTSUPP,
 * if platform doesn't support VMEM. -EEXIST, if there are outstanding VMEM
 * allocations.  -ENOMEM, if platform can't support allocation of `size` bytes.
 * -EAGAIN, if `size` does not allocate the entire VMEM region.  -EIO in case of
 * internal errors.
 */
int vmem_allocate(size_t size, phys_addr_t *addr)
{
	int rc = 0, c = 0;
	resource_size_t max_size = 0;

	if (!vmem) {
		pr_err("No vmem, try rebooting your device\n");
		rc = -ENOTSUPP;
		goto exit;
	}

	max_size = resource_size(vmem->mem.resource);

	if (atomic_read(&vmem->alloc_count)) {
		pr_err("Only single allocations allowed for vmem\n");
		rc = -EEXIST;
		goto exit;
	} else if (size > max_size) {
		pr_err("Out of memory, have max %pa\n", &max_size);
		rc = -ENOMEM;
		goto exit;
	} else if (size != max_size) {
		pr_err("Only support allocations of size %pa\n", &max_size);
		rc = -EAGAIN;
		goto exit;
	}

	rc = __enable_clocks(vmem);
	if (rc) {
		pr_err("Failed to enable axi clock\n");
		goto exit;
	}

	/* Make sure all the banks are sleeping (default) */
	BUG_ON(vmem->num_banks != DIV_ROUND_UP(size, BYTES_PER_BANK));
	for (c = 0; c < vmem->num_banks; ++c) {
		enum bank_state curr_bank_state = __bank_get_state(vmem, c);

		if (curr_bank_state != BANK_STATE_SLEEP_NO_RET) {
			pr_err("Found bank %d in a wrong state, expected %d, was %d\n",
					c, BANK_STATE_SLEEP_NO_RET,
					curr_bank_state);
			rc = -EIO;
			goto disable_clocks;
		}
	}

	/* Turn on the necessary banks */
	for (c = DIV_ROUND_UP(size, BYTES_PER_BANK) - 1; c >= 0; --c) {
		__bank_set_state(vmem, c, BANK_STATE_NORM_FORCE_CORE_ON);
		__wait_wakeup(vmem);
	}

	atomic_inc(&vmem->alloc_count);
	*addr = (phys_addr_t)vmem->mem.resource->start;
	return 0;

disable_clocks:
	__disable_clocks(vmem);
exit:
	return rc;
}

/**
 * vmem_free: - Frees the memory allocated via vmem_allocate.  Undefined
 * behaviour if to_free is a not a pointer returned via vmem_allocate
 */
void vmem_free(phys_addr_t to_free)
{
	int c = 0;
	if (!to_free || !vmem)
		return;

	BUG_ON(atomic_read(&vmem->alloc_count) == 0);

	for (c = 0; c < vmem->num_banks; ++c) {
		enum bank_state curr_state = __bank_get_state(vmem, c);

		if (curr_state != BANK_STATE_NORM_FORCE_CORE_ON) {
			pr_warn("When freeing, expected bank state to be %d, was instead %d\n",
					BANK_STATE_NORM_FORCE_CORE_ON,
					curr_state);
		}

		__bank_set_state(vmem, c, BANK_STATE_SLEEP_NO_RET);
	}

	__disable_clocks(vmem);
	atomic_dec(&vmem->alloc_count);
}

struct vmem_interrupt_cookie {
	struct vmem *vmem;
	struct work_struct work;
};

static void __irq_helper(struct work_struct *work)
{
	struct vmem_interrupt_cookie *cookie = container_of(work,
			struct vmem_interrupt_cookie, work);
	struct vmem *v = cookie->vmem;
	unsigned int stat, gen_stat, pscgc_stat, err_addr_abs,
		err_addr_rel, err_syn;

	stat = __readl(OCIMEM_INTC_STAT(v));
	gen_stat = __readl(OCIMEM_GEN_CTL(v));
	pscgc_stat = __readl(OCIMEM_PSCGC_STAT(v));

	err_addr_abs = __readl(OCIMEM_ERR_ADDRESS(v));
	err_addr_rel = v->mem.resource->start - err_addr_abs;

	err_syn = __readl(OCIMEM_AXI_ERR_SYNDROME(v));

	pr_crit("Detected a fault on VMEM:\n");
	pr_cont("\tinterrupt status: %x\n", stat);
	pr_cont("\tgeneral status: %x\n", gen_stat);
	pr_cont("\tmemory status: %x\n", pscgc_stat);
	pr_cont("\tfault address: %x (absolute), %x (relative)\n",
			err_addr_abs, err_addr_rel);
	pr_cont("\tfault bank: %x\n", err_addr_rel / BYTES_PER_BANK);
	pr_cont("\tfault core: %u (mid), %u (pid), %u (bid)\n",
			ERR_SYN_AMID(err_syn), ERR_SYN_APID(err_syn),
			ERR_SYN_ABID(err_syn));

	/* Clear the interrupt */
	__writel(0, OCIMEM_INTC_CLR(v));
	enable_irq(v->irq);

	kfree(cookie);
}

static struct vmem_interrupt_cookie interrupt_cookie;

static irqreturn_t __irq_handler(int irq, void *cookie)
{
	struct vmem *v = cookie;
	irqreturn_t status = __readl(OCIMEM_INTC_STAT(vmem)) ?
		IRQ_HANDLED : IRQ_NONE;

	if (status != IRQ_NONE) {
		disable_irq(v->irq);

		interrupt_cookie.vmem = v;
		INIT_WORK(&interrupt_cookie.work, __irq_helper);
		schedule_work(&interrupt_cookie.work);
	}

	return status;
}

static inline int __init_resources(struct vmem *v,
		struct platform_device *pdev)
{
	int rc = 0;

	v->irq = platform_get_irq(pdev, 0);
	if (v->irq < 0) {
		rc = v->irq;
		pr_err("Failed to get irq: %d\n", rc);
		v->irq = 0;
		goto exit;
	}

	/* Registers and memory */
	v->reg.resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"reg-base");
	if (!v->reg.resource) {
		pr_err("Failed to find register base\n");
		rc = -ENOENT;
		goto exit;
	}

	v->reg.base = devm_ioremap_resource(&pdev->dev, v->reg.resource);
	if (IS_ERR_OR_NULL(v->reg.base)) {
		rc = PTR_ERR(v->reg.base) ?: -EIO;
		pr_err("Failed to map register base into kernel: %d\n", rc);
		v->reg.base = NULL;
		goto exit;
	}

	pr_debug("Register range: %pa -> %pa\n", &v->reg.resource->start,
			&v->reg.resource->end);

	v->mem.resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"mem-base");
	if (!v->mem.resource) {
		pr_err("Failed to find memory base\n");
		rc = -ENOENT;
		goto exit;
	}

	v->mem.base = NULL;
	pr_debug("Memory range: %pa -> %pa\n", &v->mem.resource->start,
			&v->mem.resource->end);

	/* Clocks */
	v->maxi = devm_clk_get(&pdev->dev, "maxi");
	if (!v->maxi) {
		pr_err("Failed to find maxi clock\n");
		rc = -ENOENT;
		goto exit;
	}

	v->ahb = devm_clk_get(&pdev->dev, "ahb");
	if (!v->ahb) {
		pr_err("Failed to find ahb clock\n");
		rc = -ENOENT;
		goto exit;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "qcom,banks",
			&v->num_banks);
	if (rc || !v->num_banks) {
		pr_err("Failed reading (or found invalid) qcom,banks in %s (%d)\n",
				of_node_full_name(pdev->dev.of_node), rc);
		rc = -ENOENT;
		goto exit;
	}

	pr_debug("Found configuration with %d banks\n", v->num_banks);
exit:
	return rc;
}

static int vmem_probe(struct platform_device *pdev)
{
	uint32_t version = 0, num_banks = 0, rc = 0;

	if (vmem) {
		pr_err("Only one instance of %s allowed", pdev->name);
		return -EEXIST;
	}

	vmem = devm_kzalloc(&pdev->dev, sizeof(*vmem), GFP_KERNEL);
	if (!vmem) {
		pr_err("Failed allocate context memory in probe\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, vmem);

	rc = __init_resources(vmem, pdev);
	if (rc) {
		pr_err("Failed to read resources\n");
		goto exit;
	}

	/*
	 * For now, only support up to 4 banks. It's unrealistic that VMEM has
	 * more banks than that (even in the future).
	 */
	if (vmem->num_banks > MAX_BANKS) {
		pr_err("Number of banks (%d) exceeds what's supported (%d)\n",
			vmem->num_banks, MAX_BANKS);
		rc = -ENOTSUPP;
		goto exit;
	}

	/* Cross check the platform resources with what's available on chip */
	rc = __enable_clocks(vmem);
	if (rc) {
		pr_err("Failed to enable clocks: %d\n", rc);
		goto exit;
	}

	version = __readl(OCIMEM_HW_VERSION(vmem));
	pr_debug("v%d.%d.%d\n", VERSION_MAJOR(version), VERSION_MINOR(version),
			VERSION_STEP(version));

	num_banks = PROFILE_BANKS(__readl(OCIMEM_HW_PROFILE(vmem)));
	pr_debug("Found %d banks on chip\n", num_banks);
	if (vmem->num_banks > num_banks) {
		pr_err("Platform configuration of %d banks exceeds what's available on chip (%d)\n",
				vmem->num_banks, num_banks);
		rc = -EINVAL;
		goto disable_clocks;
	}

	if (vmem->num_banks * BYTES_PER_BANK >
			resource_size(vmem->mem.resource)) {
		pr_err("Too many banks in configuration\n");
		rc = -E2BIG;
		goto disable_clocks;
	}

	/* Everything good so far, set up debugfs */
	rc = devm_request_irq(&pdev->dev, vmem->irq, __irq_handler,
			IRQF_TRIGGER_HIGH, "vmem", vmem);
	if (rc) {
		pr_err("Failed to setup irq: %d\n", rc);
		goto disable_clocks;
	}

	vmem->debugfs_root = vmem_debugfs_init(pdev);
disable_clocks:
	__disable_clocks(vmem);
exit:
	return rc;
}

static int vmem_remove(struct platform_device *pdev)
{
	struct vmem *v = platform_get_drvdata(pdev);

	vmem_debugfs_deinit(v->debugfs_root);
	return 0;
}

static const struct of_device_id vmem_of_match[] = {
	{.compatible = "qcom,msm-vmem"},
	{}
};

MODULE_DEVICE_TABLE(of, vmem_of_match);

static struct platform_driver vmem_driver = {
	.probe = vmem_probe,
	.remove = vmem_remove,
	.driver = {
		.name = "msm_vidc_vmem",
		.owner = THIS_MODULE,
		.of_match_table = vmem_of_match,
	},
};

static int __init vmem_init(void)
{
	return platform_driver_register(&vmem_driver);
}

static void __exit vmem_exit(void)
{
	platform_driver_unregister(&vmem_driver);
}

module_init(vmem_init);
module_exit(vmem_exit);
