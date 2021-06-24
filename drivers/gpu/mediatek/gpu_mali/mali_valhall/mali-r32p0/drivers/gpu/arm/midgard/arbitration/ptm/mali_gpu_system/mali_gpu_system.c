// SPDX-License-Identifier: GPL-2.0 Linux-syscall-note OR MIT WITH

/*
 * (C) COPYRIGHT 2020-2021 Arm Limited or its affiliates. All rights reserved.
 */

/*
 * Part of the Mali reference arbiter
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/iopoll.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include "mali_gpu_assign.h"

/* Device tree compatible ID of the System Module*/
#define MALI_SYSTEM_DT_NAME			"arm,mali-gpu-system"

#define PTM_DEVICE_ID				(0x0000)
#define PTM_IRQ_CLEAR				(0x004C)
#define PTM_UNCORRECTED_ERROR_IRQ_MASK		(0x0058)
#define PTM_DEFERRED_ERROR_IRQ_MASK		(0x0064)
#define PTM_UNCORRECTED_ERROR_IRQ_STATUS	(0x0070)
#define PTM_DEFERRED_ERROR_IRQ_STATUS		(0x007C)
#define PTM_AW0_STREAM_ID			(0x1000)
#define PTM_AW0_PROTECTED_STREAM_ID		(0x1004)
#define PTM_AW_STREAM_ID_STRIDE			(0x08)
#define PTM_ERROR_FINGER_PRINT			(0x0094)
#define PTM_GROUP_RESET_STATE			(0x0098)
#define PTM_GROUP_RESET_SET			(0x009C)
#define PTM_GROUP_SOFT_RESET			(0x1)
#define PTM_GROUP_HARD_RESET			(0x2)
#define PTM_GROUP_RESET_SET_BITS		(0x2)
#define PTM_GROUP_RESET_STATE_BITS		(0x4)

#define NUM_GROUPS				4
#define REG_POLL_SLEEP_US			1
#define REG_POLL_RESET_TIMEOUT_US		20000
/* We check PTM_DEVICE_ID against this value to determine compatibility */
#define PTM_SUPPORTED_VER			(0x9e550000)
/* Required and masked fields: */
/* Bits [3:0] VERSION_STATUS - masked */
/* Bits [11:4] VERSION_MINOR - masked */
/* Bits [15:12] VERSION_MAJOR - masked */
/* Bits [19:16] PRODUCT_MAJOR - required */
/* Bits [23:20] ARCH_REV - masked */
/* Bits [27:24] ARCH_MINOR - required */
/* Bits [31:28] ARCH_MAJOR - required */
#define PTM_VER_MASK				(0xFF0F0000)
#define OFFSET_4B				4
#define OFFSET_8B				8
#define NUM_IRQ_REGISTERS			6
#define NUM_IRQ_REGISTERS_PER_TYPE		3
/* note: mask doesn't have GENERAL_HARDWARE bit */
#define IRQ_ENABLE_MASK_LOW			(0x1FFFFFF7)
#define IRQ_ENABLE_MASK_MID			(0xFFFF)
#define IRQ_ENABLE_MASK_HIGH			(0xFFFF)
#define LOW_IRQS				0
#define MID_IRQS				1
#define HIGH_IRQS				2

/* IRQ names, bits 0-47 */
static const char * const irq_name_low[] = {
	"IRQ_AXI_A_PARITY",
	"IRQ_AXI_B_PARITY",
	"IRQ_AXI_C_PARITY",
	"IRQ_GENERAL_HARDWARE",
	"IRQ_SLICE0_PARITY",
	"IRQ_SLICE1_PARITY",
	"IRQ_SLICE2_PARITY",
	"IRQ_SLICE3_PARITY",
	"IRQ_SLICE4_PARITY",
	"IRQ_SLICE5_PARITY",
	"IRQ_SLICE6_PARITY",
	"IRQ_SLICE7_PARITY",
	"IRQ_SLICE0_ISOLATION",
	"IRQ_SLICE1_ISOLATION",
	"IRQ_SLICE2_ISOLATION",
	"IRQ_SLICE3_ISOLATION",
	"IRQ_SLICE4_ISOLATION",
	"IRQ_SLICE5_ISOLATION",
	"IRQ_SLICE6_ISOLATION",
	"IRQ_SLICE7_ISOLATION",
	"IRQ_SLICE0_BIST",
	"IRQ_SLICE1_BIST",
	"IRQ_SLICE2_BIST",
	"IRQ_SLICE3_BIST",
	"IRQ_SLICE4_BIST",
	"IRQ_SLICE5_BIST",
	"IRQ_SLICE6_BIST",
	"IRQ_SLICE7_BIST",
	"IRQ_INVALID_BUS"
};

/* IRQ names, bits 32-47 */
static const char * const irq_name_mid[] = {
	"IRQ_GROUP0_WATCHDOG",
	"IRQ_GROUP1_WATCHDOG",
	"IRQ_GROUP2_WATCHDOG",
	"IRQ_GROUP3_WATCHDOG",
	"IRQ_PARTITION0_LOCKED_ACCESS",
	"IRQ_PARTITION1_LOCKED_ACCESS",
	"IRQ_PARTITION2_LOCKED_ACCESS",
	"IRQ_PARTITION3_LOCKED_ACCESS",
	"IRQ_PARTITION0_LOCKED_BIST",
	"IRQ_PARTITION1_LOCKED_BIST",
	"IRQ_PARTITION2_LOCKED_BIST",
	"IRQ_PARTITION3_LOCKED_BIST",
	"IRQ_PARTITION0_MTCRC",
	"IRQ_PARTITION1_MTCRC",
	"IRQ_PARTITION2_MTCRC",
	"IRQ_PARTITION3_MTCRC"
};

/* IRQ names, bits 64-79 */
static const char * const irq_name_high[] = {
	"IRQ_AW0_INVALID_ACCESS",
	"IRQ_AW1_INVALID_ACCESS",
	"IRQ_AW2_INVALID_ACCESS",
	"IRQ_AW3_INVALID_ACCESS",
	"IRQ_AW4_INVALID_ACCESS",
	"IRQ_AW5_INVALID_ACCESS",
	"IRQ_AW6_INVALID_ACCESS",
	"IRQ_AW7_INVALID_ACCESS",
	"IRQ_AW8_INVALID_ACCESS",
	"IRQ_AW9_INVALID_ACCESS",
	"IRQ_AW10_INVALID_ACCESS",
	"IRQ_AW11_INVALID_ACCESS",
	"IRQ_AW12_INVALID_ACCESS",
	"IRQ_AW13_INVALID_ACCESS",
	"IRQ_AW14_INVALID_ACCESS",
	"IRQ_AW15_INVALID_ACCESS"
};

/**
 * enum irq_type - IRQ error type
 * @UNCORRECTED:	Uncorrected error
 * @DEFERRED:		Deferred error
 *
 * Definition of the possible System errors
 */
enum irq_type {
	UNCORRECTED,
	DEFERRED
};

/**
 * struct system - Structure containing system data
 * @dev:	Device info
 * @res:	Resource info
 * @base_addr:	Base address
 * @irq:	IRQ data
 * @irq.line:	IRQ line
 * @irq.flags:	IRQ flags
 * @irq.mask:	IRQ mask array
 */
struct system {
	struct device *dev;
	struct resource *res;
	void __iomem *base_addr;
	struct {
		int line;
		int flags;
		u32 mask[NUM_IRQ_REGISTERS];
	} irq;
};

/**
 * process_irq() - Process IRQ error
 * @status:	status word read by ISR
 * @reg_ind:	IRQ register index
 * @system:	Internal system device data
 *
 * Return: 0 if success, or an error code.
 */
static int process_irq(u32 status,
			int reg_ind,
			struct system *system)
{
	char *t;
	int i = 0;
	int irq_level;
	enum irq_type type;
	int arr_size;
	struct device *dev;

	if (WARN_ON(!system))
		return -EINVAL;

	dev = system->dev;
	irq_level = reg_ind % NUM_IRQ_REGISTERS_PER_TYPE;
	type = reg_ind / NUM_IRQ_REGISTERS_PER_TYPE;

	if (irq_level == LOW_IRQS)
		arr_size = ARRAY_SIZE(irq_name_low);
	else if (irq_level == MID_IRQS)
		arr_size = ARRAY_SIZE(irq_name_mid);
	else
		arr_size = ARRAY_SIZE(irq_name_high);

	if (type == UNCORRECTED)
		t = "Detected UNCORRECTED_";
	else if (type == DEFERRED)
		t = "Detected DEFERRED_";
	else {
		dev_err(dev, "IRQ type not known");
		return -EINVAL;
	}

	while (status) {
		if (i >= arr_size) {
			dev_err(dev, "Max IRQ number exceeded");
			return -EINVAL;
		}
		if (status & 0x1) {
			if (irq_level == LOW_IRQS)
				dev_dbg(dev, "%s%s", t, irq_name_low[i]);
			else if (irq_level == MID_IRQS)
				dev_dbg(dev, "%s%s", t, irq_name_mid[i]);
			else
				dev_dbg(dev, "%s%s", t, irq_name_high[i]);
		}
		status >>= 1;
		i++;
	}
	return 0;
}

/**
 * set_system_irq_masks() - Writes the IRQ masks to the registers
 * @system: Internal system device data
 */
static void set_system_irq_masks(struct system *system)
{
	int i;

	if (WARN_ON(!system))
		return;

	for (i = 0; i < NUM_IRQ_REGISTERS; ++i) {
		iowrite32(system->irq.mask[i],
			system->base_addr +
			PTM_UNCORRECTED_ERROR_IRQ_MASK + (OFFSET_4B * i));
	}
}

/**
 * clear_system_irqs() - Clears the IRQs and masks persistent IRQs
 * @system:		Internal system device data
 * @status_bef_clear:	Status of the IRQs before attempting to clear it
 */
static void clear_system_irqs(struct system *system,
			u32 status_bef_clear[NUM_IRQ_REGISTERS])
{
	u32 clear_irqs;
	bool set_irq_masks = false;
	int i;

	if (WARN_ON(!system))
		return;

	/* Clear all the IRQs */
	for (i = 0; i < NUM_IRQ_REGISTERS_PER_TYPE; ++i) {
		int def_i = i + NUM_IRQ_REGISTERS_PER_TYPE;
		/* Clear uncorrected or deferred erros as they are cleared from
		 * the same register.
		 */
		clear_irqs = status_bef_clear[i] | status_bef_clear[def_i];
		iowrite32(clear_irqs,
			  system->base_addr + PTM_IRQ_CLEAR + (OFFSET_4B*i));
		/* Mask any uncorrelated IRQs */
		if (status_bef_clear[i]) {
			system->irq.mask[i] &= ~(status_bef_clear[i]);
			set_irq_masks = true;
		}
		/* Mask any deferred IRQs */
		if (status_bef_clear[def_i]) {
			system->irq.mask[def_i] &= ~(status_bef_clear[def_i]);
			set_irq_masks = true;
		}
	}

	/* Set again the IRQ masks if needed to mask persistent IRQs */
	if (set_irq_masks) {
		dev_dbg(system->dev,
			"The IRQs of the issues above will be masked.\n");
		set_system_irq_masks(system);
	}
}

/**
 * system_isr() - Handles the IRQ raised in system
 * @irq:	The IRQ ID according to the DT.
 * @data:	The data of the device where the IRQ was raised.
 *
 * Return: 0 if success, or an error code.
 */
static irqreturn_t system_isr(int irq, void *data)
{
	u32 error_finger_print = 0;
	struct system *system = data;
	struct device *dev = system->dev;
	u32 irq_status[NUM_IRQ_REGISTERS];
	bool irq_found = false;
	int i;
	int err;

	if (system == NULL) {
		pr_err("Can't access IRQ data");
		return IRQ_NONE;
	}

	if (irq != system->irq.line) {
		dev_err(dev, "Unknown irq %d\n", irq);
		return IRQ_NONE;
	}

	for (i = 0; i < NUM_IRQ_REGISTERS; ++i) {
		irq_status[i] = readl(system->base_addr +
			PTM_UNCORRECTED_ERROR_IRQ_STATUS + (OFFSET_4B * i));

		if (!irq_status[i])
			continue;

		irq_found = true;
		err = process_irq(irq_status[i], i, system);
		if (err)
			goto fail;
	}

	if (!irq_found)
		return IRQ_NONE;

	/* In case we have any errors the firger print is very important to data
	 * to be printed as well.
	 */
	error_finger_print = readl(system->base_addr + PTM_ERROR_FINGER_PRINT);
	dev_dbg(dev, "Finger print of the error: 0x%x\n", error_finger_print);

	/* Clear all the IRQs */
	clear_system_irqs(system, irq_status);

	return IRQ_HANDLED;

fail:
	dev_err(dev, "IRQ processing failed");
	return IRQ_NONE;
}

/**
 * check_ptm_version() - Check that PTM ID is the expected one
 * @ptm_device_id: 32bit integer read out of PTM_DEVICE_ID register.
 *
 * Return: 0 if version ok or 1 for version mismatch.
 */
static int check_ptm_version(u32 ptm_device_id)
{
	/* Mask the status, minor, major versions and arch_rev. */
	ptm_device_id &= PTM_VER_MASK;

	if (ptm_device_id != (PTM_SUPPORTED_VER & PTM_VER_MASK))
		return 1;
	else
		return 0;
}

/**
 * initialize_irq_masks() - Initializes the internal IRQs mask data
 * @system:		Internal system device data
 */
static void initialize_irq_masks(struct system *system)
{
	u32 i;
	u32 reg_level;

	if (WARN_ON(!system))
		return;

	for (i = 0; i < NUM_IRQ_REGISTERS; ++i) {
		reg_level = i % NUM_IRQ_REGISTERS_PER_TYPE;
		if (reg_level == 0)
			system->irq.mask[i] = IRQ_ENABLE_MASK_LOW;
		else if (reg_level == 1)
			system->irq.mask[i] = IRQ_ENABLE_MASK_MID;
		else
			system->irq.mask[i] = IRQ_ENABLE_MASK_HIGH;
	}
}

/**
 * gpu_system_probe() - Called when device is matched in device tree
 * @pdev: Platform device
 *
 * Copy stream ID's from the device tree into the register block,
 * for other modules to use.
 *
 * Return: 0 if success or a Linux error code
 */
static int gpu_system_probe(struct platform_device *pdev)
{
	int ret = 0;
	int aw, group;
	struct resource *res;
	unsigned long remap_size;
	void __iomem *base_addr;
	struct device *dev;
	u32 value, reset_status;
	struct resource *irq_res;
	int irq_req;
	struct system *system;

	dev = &pdev->dev;
	system = devm_kzalloc(dev, sizeof(*system), GFP_KERNEL);
	if (system == NULL)
		return -ENOMEM;

	/* Get the memory region of the registers. */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Invalid I/O resource");
		ret = -ENOENT;
		goto fail_resources;
	}

	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq_res) {
		dev_err(dev, "Invalid IRQ resource.\n");
		return -ENOENT;
		goto fail_resources;
	}

	remap_size = resource_size(res);

	if (!request_mem_region(res->start, remap_size, pdev->name)) {
		dev_err(dev, "Memory region request failed\n");
		ret = -EIO;
		goto fail_resources;
	}

	base_addr = ioremap(res->start, remap_size);
	if (!base_addr) {
		dev_err(dev, "Mapping memory failed\n");
		ret = -ENODEV;
		goto fail_ioremap;
	}

	value = ioread32(base_addr + PTM_DEVICE_ID);
	if (check_ptm_version(value)) {
		dev_err(dev, "Unsupported partition manager version.\n");
		ret = -ENODEV;
		goto fail_ver;
	}

	value = 0;
	reset_status = 0;
	/* Reset the GPU before performing any operation */
	for (group = 0; group < NUM_GROUPS; group++) {
		/* perform a soft reset */
		value |= PTM_GROUP_SOFT_RESET <<
				(group * PTM_GROUP_RESET_SET_BITS);
		reset_status |= PTM_GROUP_SOFT_RESET <<
				(group * PTM_GROUP_RESET_STATE_BITS);
	}

	iowrite32(value, base_addr + PTM_GROUP_RESET_SET);
	ret = readx_poll_timeout(ioread32,
			base_addr + PTM_GROUP_RESET_STATE,
			value,
			(value == reset_status),
			REG_POLL_SLEEP_US, REG_POLL_RESET_TIMEOUT_US);
	if (ret) {
		dev_err(dev, "Failed GPU soft reset\n");
		goto fail_ver;
	}

	/* release reset */
	iowrite32(0, base_addr + PTM_GROUP_RESET_SET);
	ret = readx_poll_timeout(ioread32,
			base_addr + PTM_GROUP_RESET_STATE,
			value,
			(value == 0),
			REG_POLL_SLEEP_US, REG_POLL_RESET_TIMEOUT_US);
	if (ret) {
		dev_err(dev, "Failed to release GPU reset\n");
		goto fail_ver;
	}

	/* Set StreamIDs to hard coded sequential values.
	 * The LSB of the protected stream is always '1', and is used
	 * to set the PROTMODE signal in hardware.
	 */
	for (aw = 0; aw < MALI_PTM_ACCESS_WINDOW_COUNT; ++aw) {
		uint32_t offset = aw * PTM_AW_STREAM_ID_STRIDE;

		iowrite32(2 * aw, base_addr + PTM_AW0_STREAM_ID + offset);
		iowrite32((2 * aw) + 1,
			base_addr + PTM_AW0_PROTECTED_STREAM_ID + offset);
	}

	system->irq.line = irq_res->start;
	system->irq.flags = irq_res->flags & IRQF_TRIGGER_MASK;
	system->dev = dev;
	system->res = res;
	system->base_addr = base_addr;
	initialize_irq_masks(system);
	dev_set_drvdata(dev, system);

	/* request IRQ. */
	irq_req = request_irq(system->irq.line, system_isr,
				system->irq.flags | IRQF_SHARED,
				dev_name(dev), system);
	if (irq_req) {
		dev_err(dev, "Can't request interrupt.\n");
		ret = irq_req;
		goto fail_irq;
	}

	/* Enable all uncorrected and deferred IRQs during probe */
	set_system_irq_masks(system);

	dev_info(dev, "Probed\n");
	return 0;

fail_irq:
	free_irq(system->irq.line, system);
fail_ver:
	iounmap(base_addr);
fail_ioremap:
	release_mem_region(res->start, remap_size);
fail_resources:
	devm_kfree(dev, system);
	return ret;
}

/**
 * gpu_system_remove() - Called when device is removed
 * @pdev: Platform device
 *
 * Return: 0 always
 */
static int gpu_system_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct system *system;

	system = dev_get_drvdata(dev);
	if (system) {
		/* disable IRQs */
		iowrite32(0, system->base_addr + PTM_IRQ_CLEAR);
		iounmap(system->base_addr);
		release_mem_region(system->res->start,
					resource_size(system->res));
		free_irq(system->irq.line, system);
		dev_set_drvdata(dev, NULL);
		devm_kfree(dev, system);
		return 0;
	}
	dev_err(dev, "Can't properly release resources");
	return -EINVAL;
}

/*
 * struct gpu_system_dt_match: Match the platform device with the Device Tree.
 */
static const struct of_device_id gpu_system_dt_match[] = {
	{ .compatible = MALI_SYSTEM_DT_NAME },
	{}
};

/*
 * struct gpu_system_driver - Platform driver data.
 */
static struct platform_driver gpu_system_driver = {
	.probe = gpu_system_probe,
	.remove = gpu_system_remove,
	.driver = {
		.name = "mali_gpu_system",
		.of_match_table = gpu_system_dt_match,
	},
};

/**
 * gpu_system_init - Register platform driver
 *
 * Return: struct platform_device pointer on success, or ERR_PTR()
 */
static int __init gpu_system_init(void)
{
	return platform_driver_register(&gpu_system_driver);
}
module_init(gpu_system_init);

/**
 * gpu_system_exit - Unregister platform driver
 */
static void __exit gpu_system_exit(void)
{
	platform_driver_unregister(&gpu_system_driver);
}
module_exit(gpu_system_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("mali-gpu-system");
