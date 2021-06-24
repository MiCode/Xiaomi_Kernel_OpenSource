// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT

/*
 * (C) COPYRIGHT 2021 Arm Limited or its affiliates. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/iopoll.h>
#include <linux/of_platform.h>
#include <mali_arbiter.h>

/* Arbiter version against which was implemented this file */
#define MALI_ARBITER_IMPLEMENTATION_VERSION 2
#if MALI_ARBITER_IMPLEMENTATION_VERSION != MALI_ARBITER_VERSION
#error "Unsupported Mali Arbiter version."
#endif

/* Device tree compatible ID of the Partition Control module */
#define PTM_CONTROL_DT_NAME "arm,mali-gpu-partition-control"

#define REG_POLL_SLEEP_US                              1
#define REG_POLL_TIMEOUT_US                            40
#define REG_POLL_RESET_TIMEOUT_US                      20000
#define PTM_DEVICE_ID                                  0x00
/* We check PTM_DEVICE_ID against this value to determine compatibility */
#define PTM_SUPPORTED_VER                              0x9e550000
/* Required and masked fields: */
/* Bits [3:0] VERSION_STATUS - masked */
/* Bits [11:4] VERSION_MINOR - masked */
/* Bits [15:12] VERSION_MAJOR - masked */
/* Bits [19:16] PRODUCT_MAJOR - required */
/* Bits [23:20] ARCH_REV - masked */
/* Bits [27:24] ARCH_MINOR - required */
/* Bits [31:28] ARCH_MAJOR - required */
#define PTM_VER_MASK                                   0xFF0F0000

#define PTM_RESET_STATE_OFFSET                         0x0058
#define PTM_RESET_SET_OFFSET                           0x005C
#define PTM_PARTITION_STATE_OFFSET                     0x0060
#define PTM_AW_SET                                     0x0064
#define PTM_PARTITION_STATE_VALID_RANGE                0xF
#define PTM_RESET_STATE_MASK                           0x01
#define PTM_PARTITION_STATE_LOCK_MASK                  0x3
#define PTM_PARTITION_STATE_AW_READY_MASK              0x4
#define PTM_PARTITION_STATE_AW_ERROR_MASK              0x8
#define PTM_NUM_OF_AW                                  0x10
#define PTM_RESET_SET_TRIGGER_VALUE                    0x1

/**
 * struct mali_gpu_part_ctrl - Partition manager control data
 * @mem:        Mapped memory from physical address retrieved from dt
 * @reg_resrc:  Resource contains address range retrieved from device tree
 * @dev:        Domain device device set in during probe
 * @assign_ops: Partition control functions for VM Assign interface
 */
struct  mali_gpu_part_ctrl {
	void __iomem *mem;
	struct resource *reg_resrc;
	struct device *dev;
	struct vm_assign_ops assign_ops;
};

/**
 * to_mali_gpu_part_ctrl() - gets pointer to mali_gpu_part_ctrl
 * @dev: Pointer to the partition control device.
 *
 * Gets pointer to mali_gpu_part_ctrl structure from device data
 *
 * Return: mali_gpu_part_ctrl* if successful, otherwise NULL
 */
static struct mali_gpu_part_ctrl *to_mali_gpu_part_ctrl(struct device *dev)
{
	struct mali_gpu_part_ctrl *ptm_pc;
	struct vm_assign_ops *assign_ops;

	assign_ops = dev_get_drvdata(dev);
	if (!assign_ops) {
		dev_err(dev, "Retrieve driver data failed\n");
		return NULL;
	}
	ptm_pc = container_of(assign_ops, struct mali_gpu_part_ctrl,
			      assign_ops);

	return ptm_pc;
}

/**
 * partition_control_reset() - Reset partition.
 * @partition_ctrl:       Pointer to the partition control data.
 *
 * Reset the partition
 *
 * Return:     0 if successful, otherwise a negative error code.
 */
static int partition_control_reset(struct mali_gpu_part_ctrl *partition_ctrl)
{
	int error;
	uint32_t reset_state;
	uint32_t part_state;

	if (!partition_ctrl)
		return -EINVAL;

	iowrite32(PTM_RESET_SET_TRIGGER_VALUE,
			partition_ctrl->mem + PTM_RESET_SET_OFFSET);
	error = readx_poll_timeout(ioread32,
			partition_ctrl->mem + PTM_RESET_STATE_OFFSET,
			reset_state, (reset_state & PTM_RESET_STATE_MASK),
			REG_POLL_SLEEP_US, REG_POLL_RESET_TIMEOUT_US);

	if (error)
		return error;

	iowrite32(0x0, partition_ctrl->mem + PTM_RESET_SET_OFFSET);
	error = readx_poll_timeout(ioread32,
			partition_ctrl->mem + PTM_RESET_STATE_OFFSET,
			reset_state,
			((reset_state & PTM_RESET_STATE_MASK) == 0),
			REG_POLL_SLEEP_US, REG_POLL_RESET_TIMEOUT_US);
	if (error)
		return error;

	/* Wait for the partition state to be unlocked after
	 * the reset
	 */
	error = readx_poll_timeout(
		ioread32, partition_ctrl->mem +
			PTM_PARTITION_STATE_OFFSET, part_state,
			((part_state & PTM_PARTITION_STATE_LOCK_MASK) == 0),
		REG_POLL_SLEEP_US, REG_POLL_TIMEOUT_US);

	if (error) {
		dev_err(partition_ctrl->dev,
			"Partition locked after reset! Error: %d",
			error);
		return error;
	}

	return error;
}

/**
 * assign_partition_to_aw() - Assign Partition to a specific AW.
 * @dev:   Pointer to the partition control module device.
 * @aw_id: Access window ID to assign the Partition to.
 *
 * Reset and set the active Access Window for this partition. Return when
 * partition is ready.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
int assign_partition_to_aw(struct device *dev, uint32_t aw_id)
{
	struct mali_gpu_part_ctrl *partition_ctrl;
	int error;
	uint32_t val;
	uint32_t state;

	if (!dev)
		return -EINVAL;

	partition_ctrl = to_mali_gpu_part_ctrl(dev);
	if (!partition_ctrl) {
		dev_err(dev, "Retrieve partition control failed\n");
		return -EIO;
	}

	if (aw_id > PTM_NUM_OF_AW - 1)
		return -EINVAL;

	/* Reset the partition if necessary */
	state = ioread32(partition_ctrl->mem + PTM_PARTITION_STATE_OFFSET);
	if (state & PTM_PARTITION_STATE_LOCK_MASK) {
		error = partition_control_reset(partition_ctrl);
		if (error) {
			dev_err(dev, "Error resetting partition: %d\n", error);
			return error;
		}
	}

	val = 0x1 << aw_id;
	iowrite32(val, partition_ctrl->mem + PTM_AW_SET);

	state = ioread32(partition_ctrl->mem + PTM_PARTITION_STATE_OFFSET);
	if (state & PTM_PARTITION_STATE_AW_ERROR_MASK)
		return -EINVAL;

	error = readx_poll_timeout(ioread32,
			partition_ctrl->mem + PTM_PARTITION_STATE_OFFSET,
			state, (state & PTM_PARTITION_STATE_AW_READY_MASK),
			REG_POLL_SLEEP_US, REG_POLL_TIMEOUT_US);

	return error;
}

/**
 * unassign_partition() - Unassign Partition from any AW.
 * @dev: Pointer to the partition control module device.
 *
 * Reset this partition. Return when partition is ready.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
int unassign_partition(struct device *dev)
{
	struct mali_gpu_part_ctrl *partition_ctrl;
	int error;
	uint32_t state;

	if (!dev)
		return -EINVAL;

	partition_ctrl = to_mali_gpu_part_ctrl(dev);
	if (!partition_ctrl) {
		dev_err(dev, "Retrieve partition control failed\n");
		return -EIO;
	}

	/* Reset the partition if necessary */
	state = ioread32(partition_ctrl->mem + PTM_PARTITION_STATE_OFFSET);
	if (state & PTM_PARTITION_STATE_LOCK_MASK) {
		error = partition_control_reset(partition_ctrl);
		if (error) {
			dev_err(dev, "Error resetting partition: %d\n", error);
			return error;
		}
	}

	return 0;
}

/**
 * get_assigned_aw() - Get the active AW.
 * @dev:   Pointer to the partition control module device.
 * @aw_id: Pointer to variable to receive the active Access Window ID or
 *         VM_UNASSIGNED (see Arbiter.h) if no AWs active.
 *
 * Get the active Access Window for this partition.
 *
 * Return:     0 if successful, otherwise a negative error code.
 */
int get_assigned_aw(struct device *dev, int32_t *aw_id)
{
	struct mali_gpu_part_ctrl *partition_ctrl;
	uint32_t val;
	int i;

	if (!dev || !aw_id)
		return -EINVAL;

	partition_ctrl = to_mali_gpu_part_ctrl(dev);
	if (!partition_ctrl) {
		dev_err(dev, "Retrieve partition control failed\n");
		return -EIO;
	}

	val = ioread32(partition_ctrl->mem + PTM_AW_SET);
	if (val) {
		for (i = 0; i < PTM_NUM_OF_AW; i++) {
			val = val >> 1;
			if (!val) {
				*aw_id = i;
				dev_dbg(dev, "aw_id: %d\n", i);
				return 0;
			}
		}

		/* Unexpected result */
		dev_err(dev, "Could not find active AW\n");
		return -EIO;
	}

	*aw_id = VM_UNASSIGNED;
	dev_dbg(dev, "AW unassigned\n");
	return 0;
}

/**
 * pm_control_probe() - Initialize the pm_control device
 * @pdev: The platform device
 *
 * Called when device is matched in device tree, allocate the resources
 * for the device and make necessary mapping to access registers.
 *
 * Return: 0 if success, or a Linux error code
 */
static int pm_control_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mali_gpu_part_ctrl *partition_ctrl;
	struct resource *reg_resrc;
	uint32_t ptm_device_id;

	if (!pdev)
		return -EINVAL;

	reg_resrc = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!reg_resrc) {
		dev_err(&pdev->dev, "Invalid register resource\n");
		return -ENOENT;
	}

	partition_ctrl = devm_kzalloc(&pdev->dev,
				sizeof(struct mali_gpu_part_ctrl), GFP_KERNEL);

	if (!partition_ctrl)
		return -ENOMEM;

	if  (!request_mem_region(reg_resrc->start, resource_size(reg_resrc),
			"mali_pm_control")) {
		dev_err(&pdev->dev, "Requesting Memory region failed\n");
		ret = -EIO;
		goto req_memory_fail;
	}

	partition_ctrl->mem = ioremap(reg_resrc->start,
				resource_size(reg_resrc));

	if (partition_ctrl->mem == NULL) {
		dev_err(&pdev->dev, "Remapping Memory region failed\n");
		ret = -EIO;
		goto ioremap_fail;
	}

	ptm_device_id = ioread32(partition_ctrl->mem + PTM_DEVICE_ID);
	/* Mask the status, minor, major versions and arch_rev. */
	ptm_device_id &= PTM_VER_MASK;
	if (ptm_device_id != (PTM_SUPPORTED_VER & PTM_VER_MASK)) {
		dev_err(&pdev->dev, "Unsupported partition manager version.\n");
		ret = -ENODEV;
		goto unsupported_version_fail;
	}
	partition_ctrl->dev = &pdev->dev;
	partition_ctrl->reg_resrc = reg_resrc;
	partition_ctrl->assign_ops.assign_vm = assign_partition_to_aw;
	partition_ctrl->assign_ops.unassign_vm = unassign_partition;
	partition_ctrl->assign_ops.get_assigned_vm = get_assigned_aw;
	dev_set_drvdata(&pdev->dev, &partition_ctrl->assign_ops);

	dev_info(&pdev->dev, "Probed\n");
	return 0;

unsupported_version_fail:
	iounmap(partition_ctrl->mem);
ioremap_fail:
	release_mem_region(reg_resrc->start, resource_size(reg_resrc));
req_memory_fail:
	devm_kfree(&pdev->dev, partition_ctrl);

	return ret;
}

/**
 * pm_control_remove() - Remove the pm_control device
 * @pdev: The platform device
 *
 * Called when the device is being unloaded to do any cleanup
 *
 * Return: Always returns 0
 */
static int pm_control_remove(struct platform_device *pdev)
{
	struct mali_gpu_part_ctrl *partition_ctrl;

	if (!pdev)
		return -EINVAL;

	partition_ctrl = to_mali_gpu_part_ctrl(&pdev->dev);
	if (!partition_ctrl) {
		dev_err(&pdev->dev, "Retrieve partition control failed\n");
		return -EIO;
	}

	release_mem_region(partition_ctrl->reg_resrc->start,
			resource_size(partition_ctrl->reg_resrc));
	iounmap(partition_ctrl->mem);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, partition_ctrl);

	return 0;
}

static const struct of_device_id pm_control_dt_match[] = {
		{ .compatible = PTM_CONTROL_DT_NAME },
		{}
};

struct platform_driver pm_control_driver = {
	.probe = pm_control_probe,
	.remove = pm_control_remove,
	.driver = {
		.name = "mali_pm_control",
		.of_match_table = pm_control_dt_match,
	},
};

/**
 * mali_pm_control_init() - Register platform driver
 *
 * Return: See definition of platform_driver_register()
 */
static int __init mali_pm_control_init(void)
{
	return platform_driver_register(&pm_control_driver);
}
module_init(mali_pm_control_init);


static void __exit mali_pm_control_exit(void)
{
	platform_driver_unregister(&pm_control_driver);
}
module_exit(mali_pm_control_exit);

MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Mali Partition Manager control module");
MODULE_AUTHOR("ARM Ltd.");
MODULE_LICENSE("Dual MIT/GPL");
