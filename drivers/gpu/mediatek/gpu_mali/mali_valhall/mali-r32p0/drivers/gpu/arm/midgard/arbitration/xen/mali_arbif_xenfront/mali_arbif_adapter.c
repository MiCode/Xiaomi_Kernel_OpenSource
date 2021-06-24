// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2019-2021 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

/*
 * Part of the Mali arbiter interface
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <mali_kbase_arbiter_interface.h>
#include "mali_arbif_adapter.h"

#define MALI_KBASE_ARBITER_INTERFACE_IMPLEMENTATION_VERSION 5
#if MALI_KBASE_ARBITER_INTERFACE_IMPLEMENTATION_VERSION != \
			MALI_KBASE_ARBITER_INTERFACE_VERSION
#error "Unsupported Mali Arbiter interface version."
#endif

/**
 * struct arbiter_if - Internal arbiter_if information
 * @arbif_dev: public arbif interface.
 * @dev: device for arbif_adapter (one per guest)
 * @vm_dev: device for VM
 * @vm_drv_ops: VM callbacks for communication with arbiter
 *              (via front/backend comms)
 * @comms_dev: Adapter to frontend comms device
 */
struct arbiter_if {
	struct arbiter_if_dev arbif_dev;
	struct device *dev;
	struct device *vm_dev;
	struct arbiter_if_arb_vm_ops vm_drv_ops;
	struct arbif_comms_dev comms_dev;
};

/**
 * arbif_from_dev() - Convert arbiter_if_dev to arbiter_if
 * @arbif_dev: arbiter_if_dev to be converted
 * Return: pointer to arbiter_if structure
 */
static inline struct arbiter_if *arbif_from_dev(
	struct arbiter_if_dev *arbif_dev)
{
	return container_of(arbif_dev, struct arbiter_if, arbif_dev);
}

/*
 * VM to Adapter
 */

/**
 * register_vm_dev() - Register VM device driver callbacks
 * @arbif_dev: The arbiter interface we are registering device callbacks
 * @dev: The device structure to supply in the callbacks.
 * @ops: The callbacks that the device driver supports
 *       (none are optional).
 *
 * Return: 0 if success or a linux error code
 */
static int register_vm_dev(struct arbiter_if_dev *arbif_dev,
		struct device *dev, struct arbiter_if_arb_vm_ops *ops)
{
	struct arbiter_if *arbif_adapter = arbif_from_dev(arbif_dev);
	struct arbif_comms_dev *comms_dev;

	if (!arbif_adapter || !dev || !ops)
		return -EFAULT;

	/* Store a copy of the VM operations (this is also used to detect
	 * if the VM is waiting on us in a deferred probe state).
	 */
	arbif_adapter->vm_dev = dev;
	memcpy(&arbif_adapter->vm_drv_ops, ops,
		sizeof(arbif_adapter->vm_drv_ops));

	/* Check if our comms device has been populated */
	comms_dev = &arbif_adapter->comms_dev;
	if (!comms_dev->ad_fe_ops.vm_arb_gpu_request)
		return -EPROBE_DEFER;

	return 0;
}

/**
 * unregister_vm_dev() - Unregister VM device driver callbacks
 * @arbif_dev: The arbiter interface we are unregistering from.
 */
static void unregister_vm_dev(struct arbiter_if_dev *arbif_dev)
{
	struct arbiter_if *arbif_adapter = arbif_from_dev(arbif_dev);

	if (WARN_ON(!arbif_adapter))
		return;

	arbif_adapter->vm_dev = NULL;
}

/**
 * gpu_request() - Sends a GPU_REQUEST message to the Arbiter
 * @arbif_dev: The arbiter interface we want to issue the request.
 *
 * Ask the arbiter interface for GPU access.
 */
static void gpu_request(struct arbiter_if_dev *arbif_dev)
{
	struct arbiter_if *arbif_adapter = arbif_from_dev(arbif_dev);
	struct arbif_comms_dev *comms_dev;

	if (WARN_ON(!arbif_adapter))
		return;

	comms_dev = &arbif_adapter->comms_dev;
	if (WARN_ON(!comms_dev->ad_fe_ops.vm_arb_gpu_request))
		return;

	comms_dev->ad_fe_ops.vm_arb_gpu_request(comms_dev->fe_priv);
}

/**
 * gpu_active() - Sends a GPU_ACTIVE message to the Arbiter.
 * @arbif_dev: The arbiter interface device.
 *
 * Informs the arbiter that the driver is ACTIVE
 */
static void gpu_active(struct arbiter_if_dev *arbif_dev)
{
	struct arbiter_if *arbif_adapter = arbif_from_dev(arbif_dev);
	struct arbif_comms_dev *comms_dev;

	if (WARN_ON(!arbif_adapter))
		return;

	comms_dev = &arbif_adapter->comms_dev;
	if (WARN_ON(!comms_dev->ad_fe_ops.vm_arb_gpu_active))
		return;

	comms_dev->ad_fe_ops.vm_arb_gpu_active(comms_dev->fe_priv);
}

/**
 * gpu_idle() - Sends a GPU_IDLE message to the Arbiter.
 * @arbif_dev: The arbiter interface device.
 *
 * Inform the arbiter that the driver has gone idle
 */
static void gpu_idle(struct arbiter_if_dev *arbif_dev)
{
	struct arbiter_if *arbif_adapter = arbif_from_dev(arbif_dev);
	struct arbif_comms_dev *comms_dev;

	if (WARN_ON(!arbif_adapter))
		return;

	comms_dev = &arbif_adapter->comms_dev;
	if (WARN_ON(!comms_dev->ad_fe_ops.vm_arb_gpu_idle))
		return;

	comms_dev->ad_fe_ops.vm_arb_gpu_idle(comms_dev->fe_priv);
}

/**
 * gpu_stopped() - Sends a GPU_STOPPED message to the Arbiter
 * @arbif_dev: arbif kernel module device
 * @gpu_required: GPU request flag
 *
 * Acknowledges arbiter with a GPU_STOPPED message
 */
static void gpu_stopped(struct arbiter_if_dev *arbif_dev, u8 gpu_required)
{
	struct arbiter_if *arbif_adapter = arbif_from_dev(arbif_dev);
	struct arbif_comms_dev *comms_dev;

	if (WARN_ON(!arbif_adapter))
		return;

	comms_dev = &arbif_adapter->comms_dev;
	if (WARN_ON(!comms_dev->ad_fe_ops.vm_arb_gpu_stopped))
		return;

	comms_dev->ad_fe_ops.vm_arb_gpu_stopped(comms_dev->fe_priv,
			gpu_required);
}

/*
 * Frontend to Adapter (and Adapter to VM)
 */

/**
 * gpu_stop() - Ask VM to stop using GPU
 * @ad_priv: arbif kernel module device
 *
 * Informs KBase to stop using the GPU as soon as possible.
 */
static void gpu_stop(void *ad_priv)
{
	struct arbiter_if_dev *arbif_dev = ad_priv;
	struct arbiter_if *arbif_adapter = arbif_from_dev(arbif_dev);

	if (!arbif_adapter || !arbif_adapter->vm_dev
		|| WARN_ON(!arbif_adapter->vm_drv_ops.arb_vm_gpu_stop))
		return;

	arbif_adapter->vm_drv_ops.arb_vm_gpu_stop(arbif_adapter->vm_dev);
}

/**
 * gpu_granted() - GPU has been granted to VM
 * @ad_priv: arbif kernel module device
 * @freq: Frequency reported from Arbiter
 *
 * Informs KBase that the GPU can now be used by the VM.
 */
static void gpu_granted(void *ad_priv, uint32_t freq)
{
	struct arbiter_if_dev *arbif_dev = ad_priv;
	struct arbiter_if *arbif_adapter = arbif_from_dev(arbif_dev);

	if (!arbif_adapter || !arbif_adapter->vm_dev
		|| WARN_ON(!arbif_adapter->vm_drv_ops.arb_vm_gpu_granted))
		return;

	/* updating frequency must happen before processing
	 * granted message to prevent a racing condition with
	 * the granting process that might lead to reporting
	 * old version of frequency
	 */
	if (freq != NO_FREQ)
		arbif_adapter->vm_drv_ops.arb_vm_update_freq(
				arbif_adapter->vm_dev, freq);
	arbif_adapter->vm_drv_ops.arb_vm_gpu_granted(arbif_adapter->vm_dev);
}

/**
 * gpu_lost() - VM has lost the GPU
 * @ad_priv: arbif kernel module device
 *
 * This is called if KBase takes too long to respond to the arbiter
 * stop request. Once this is called, KBase will assume that access
 * to the GPU has been lost and will fail all running jobs and reset
 * its internal state.
 * If successful, will respond with a vm_arb_gpu_stopped message.
 */
static void gpu_lost(void *ad_priv)
{
	struct arbiter_if_dev *arbif_dev = ad_priv;
	struct arbiter_if *arbif_adapter = arbif_from_dev(arbif_dev);

	if (!arbif_adapter || !arbif_adapter->vm_dev
		|| WARN_ON(!arbif_adapter->vm_drv_ops.arb_vm_gpu_lost))
		return;

	arbif_adapter->vm_drv_ops.arb_vm_gpu_lost(arbif_adapter->vm_dev);
}

/**
 * arbif_adapter_probe() - Initialize the arbif_adapter device
 * @pdev: The platform device
 *
 * Called when device is matched in device tree, allocate the resources
 * for the device and set up the proper callbacks in the arbiter_if struct.
 *
 * Return: 0 if success, or a Linux error code
 */
int arbif_adapter_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct arbiter_if *arbif_adapter;

	arbif_adapter = devm_kzalloc(&pdev->dev, sizeof(struct arbiter_if),
		GFP_KERNEL);

	if (!arbif_adapter)
		return -ENOMEM;

	arbif_adapter->dev = dev;

	/* Configure VM to Adapter callbacks */
	arbif_adapter->arbif_dev.vm_ops.vm_arb_register_dev = register_vm_dev;
	arbif_adapter->arbif_dev.vm_ops.vm_arb_unregister_dev =
		unregister_vm_dev;
	arbif_adapter->arbif_dev.vm_ops.vm_arb_gpu_request = gpu_request;
	arbif_adapter->arbif_dev.vm_ops.vm_arb_gpu_active = gpu_active;
	arbif_adapter->arbif_dev.vm_ops.vm_arb_gpu_idle = gpu_idle;
	arbif_adapter->arbif_dev.vm_ops.vm_arb_gpu_stopped = gpu_stopped;

	/* Configure Frontend to Adapter callbacks */
	arbif_adapter->comms_dev.ad_priv = &arbif_adapter->arbif_dev;
	arbif_adapter->comms_dev.fe_ad_ops.arb_vm_gpu_stop = gpu_stop;
	arbif_adapter->comms_dev.fe_ad_ops.arb_vm_gpu_granted = gpu_granted;
	arbif_adapter->comms_dev.fe_ad_ops.arb_vm_gpu_lost = gpu_lost;

	platform_set_drvdata(pdev, &arbif_adapter->arbif_dev);

	return 0;
}

/**
 * arbif_adapter_remove() - Remove the arbif_adapter device
 * @pdev: The platform device
 *
 * Called when the device is being unloaded to do any cleanup
 *
 * Return: always 0
 */
int arbif_adapter_remove(struct platform_device *pdev)
{
	struct arbiter_if_dev *arbif_dev;
	struct arbiter_if *arbif_adapter;

	arbif_dev = platform_get_drvdata(pdev);
	arbif_adapter = arbif_from_dev(arbif_dev);
	platform_set_drvdata(pdev, NULL);
	if (arbif_adapter)
		devm_kfree(&pdev->dev, arbif_adapter);

	return 0;
}

/**
 * arbif_adapter_init_comms() - Initialize the comms channel.
 * @pdev: Adapter Platform Device to init comms on.
 * @fe_comms_dev: Comms device of connecting module (front-end).
 *
 * Return: 0 on success or a Linux error code
 */
int arbif_adapter_init_comms(struct platform_device *pdev,
			struct arbif_comms_dev *fe_comms_dev)
{
	struct arbiter_if_dev *arbif_dev;
	struct arbiter_if *arbif_adapter;
	struct arbif_comms_dev *ad_comms_dev;

	arbif_dev = platform_get_drvdata(pdev);
	if (WARN_ON(!arbif_dev || !fe_comms_dev))
		return -ENODEV;

	arbif_adapter = arbif_from_dev(arbif_dev);
	if (WARN_ON(!arbif_adapter))
		return -ENODEV;

	ad_comms_dev = &arbif_adapter->comms_dev;

	/* Copy the adapter to frontend callbacks to the ad_comms_dev */
	ad_comms_dev->fe_priv = fe_comms_dev->fe_priv;
	memcpy(&ad_comms_dev->ad_fe_ops, &fe_comms_dev->ad_fe_ops,
		sizeof(struct arbif_adapter_comms_ad_fe_ops));

	/* Copy the frontend to adapter callbacks to the fe_comms_dev */
	fe_comms_dev->ad_priv = ad_comms_dev->ad_priv;
	memcpy(&fe_comms_dev->fe_ad_ops, &ad_comms_dev->fe_ad_ops,
		sizeof(struct arbif_adapter_comms_fe_ad_ops));

	/* If KBase is waiting on us in deferred probe, request that the
	 * kernel attaches the device.
	 */
	if (arbif_adapter->vm_dev)
		if (device_attach(arbif_adapter->vm_dev) != 1)
			return -ENODEV;

	return 0;
}

/**
 * arbif_adapter_deinit_comms() - Deinitialize the comms channel.
 * @pdev: Adapter Platform Device to deinit comms on.
 * @fe_comms_dev: Comms device of disconnecting module (front-end).
 *
 * Return: 0 if success, or a Linux error code
 */
int arbif_adapter_deinit_comms(struct platform_device *pdev,
			struct arbif_comms_dev *fe_comms_dev)
{
	struct arbiter_if_dev *arbif_dev;
	struct arbiter_if *arbif_adapter;
	struct arbif_comms_dev *ad_comms_dev;

	arbif_dev = platform_get_drvdata(pdev);
	if (WARN_ON(!arbif_dev || !fe_comms_dev))
		return -ENODEV;

	arbif_adapter = arbif_from_dev(arbif_dev);
	if (WARN_ON(!arbif_adapter))
		return -ENODEV;

	ad_comms_dev = &arbif_adapter->comms_dev;

	/* Clear the adapter to frontend callbacks from ad_comms_dev */
	ad_comms_dev->fe_priv = NULL;
	memset(&ad_comms_dev->ad_fe_ops, 0,
		sizeof(struct arbif_adapter_comms_ad_fe_ops));

	/* Clear the frontend to adapter callbacks from fe_comms_dev */
	fe_comms_dev->ad_priv = NULL;
	memset(&fe_comms_dev->fe_ad_ops, 0,
		sizeof(struct arbif_adapter_comms_fe_ad_ops));

	return 0;
}

/*
 * arbif_adapter_dt_match: Match the platform device with the Device Tree.
 */
static const struct of_device_id arbif_adapter_dt_match[] = {
	{ .compatible = ARBITER_IF_DT_NAME },
	{}
};

/*
 * arbif_adapter_driver: Platform driver data.
 */
struct platform_driver arbif_adapter_driver = {
	.probe = arbif_adapter_probe,
	.remove = arbif_adapter_remove,
	.driver = {
		.name = "mali_arbif_adapter ",
		.of_match_table = arbif_adapter_dt_match,
	},
};

/**
 * arbif_adapter_register(): Register the arbif_adapter driver
 *
 * Return: 0 if success, or a Linux error code
 */
int arbif_adapter_register(void)
{
	int ret;

	ret = platform_driver_register(&arbif_adapter_driver);
	if (ret)
		pr_err("Error while registering mali_arbif_adapter\n");

	return ret;
}
/**
 * arbif_adapter_unregister(): Unregister the arbif_adapter driver
 */
void arbif_adapter_unregister(void)
{
	platform_driver_unregister(&arbif_adapter_driver);
}
