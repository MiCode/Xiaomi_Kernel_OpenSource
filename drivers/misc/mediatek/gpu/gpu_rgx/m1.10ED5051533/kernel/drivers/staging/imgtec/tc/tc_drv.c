/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

/*
 * This is a device driver for the testchip framework. It creates platform
 * devices for the pdp and ext sub-devices, and exports functions to manage the
 * shared interrupt handling
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/delay.h>

#if defined(CONFIG_MTRR)
#include <asm/mtrr.h>
#endif

#include "pvrmodule.h"

#include "tc_apollo.h"
#include "tc_odin.h"

/* How much memory to give to the PDP heap (used for pdp buffers). */
#define TC_PDP_MEM_SIZE_BYTES           ((TC_DISPLAY_MEM_SIZE)*1024*1024)

#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
/* How much memory to give to the secure heap. */
#define TC_SECURE_MEM_SIZE_BYTES        ((TC_SECURE_MEM_SIZE)*1024*1024)
#endif

#define PCI_VENDOR_ID_POWERVR		0x1010
#define DEVICE_ID_PCI_APOLLO_FPGA	0x1CF1
#define DEVICE_ID_PCIE_APOLLO_FPGA	0x1CF2

MODULE_DESCRIPTION("PowerVR testchip framework driver");

static int tc_core_clock = RGX_TC_CORE_CLOCK_SPEED;
module_param(tc_core_clock, int, 0444);
MODULE_PARM_DESC(tc_core_clock, "TC core clock speed");

static int tc_mem_clock = RGX_TC_MEM_CLOCK_SPEED;
module_param(tc_mem_clock, int, 0444);
MODULE_PARM_DESC(tc_mem_clock, "TC memory clock speed");

static int tc_sys_clock = RGX_TC_SYS_CLOCK_SPEED;
module_param(tc_sys_clock, int, 0444);
MODULE_PARM_DESC(tc_sys_clock, "TC system clock speed (TCF5 only)");

static int tc_mem_latency;
module_param(tc_mem_latency, int, 0444);
MODULE_PARM_DESC(tc_mem_latency, "TC memory read latency in cycles (TCF5 only)");

static int tc_wresp_latency;
module_param(tc_wresp_latency, int, 0444);
MODULE_PARM_DESC(tc_wresp_latency, "TC memory write response latency in cycles (TCF5 only)");

static unsigned long tc_pdp_mem_size = TC_PDP_MEM_SIZE_BYTES;
module_param(tc_pdp_mem_size, ulong, 0444);
MODULE_PARM_DESC(tc_pdp_mem_size,
	"TC PDP reserved memory size in bytes");

#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
static unsigned long tc_secure_mem_size = TC_SECURE_MEM_SIZE_BYTES;
module_param(tc_secure_mem_size, ulong, 0444);
MODULE_PARM_DESC(tc_secure_mem_size,
	"TC secure reserved memory size in bytes");
#endif

static struct debugfs_blob_wrapper tc_debugfs_rogue_name_blobs[] = {
	[APOLLO_VERSION_TCF_2] = {
		.data = "hood", /* probably */
		.size = sizeof("hood") - 1,
	},
	[APOLLO_VERSION_TCF_5] = {
		.data = "fpga (unknown)",
		.size = sizeof("fpga (unknown)") - 1,
	},
	[APOLLO_VERSION_TCF_BONNIE] = {
		.data = "bonnie",
		.size = sizeof("bonnie") - 1,
	},
	[ODIN_VERSION_TCF_BONNIE] = {
		.data = "bonnie",
		.size = sizeof("bonnie") - 1,
	},
	[ODIN_VERSION_FPGA] = {
		.data = "fpga (unknown)",
		.size = sizeof("fpga (unknown)") - 1,
	},
};

#if defined(CONFIG_MTRR) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0))
/*
 * A return value of:
 *      0 or more means success
 *     -1 means we were unable to add an mtrr but we should continue
 *     -2 means we were unable to add an mtrr but we shouldn't continue
 */
static int mtrr_setup(struct pci_dev *pdev,
		      resource_size_t mem_start,
		      resource_size_t mem_size)
{
	int err;
	int mtrr;

	/* Reset MTRR */
	mtrr = mtrr_add(mem_start, mem_size, MTRR_TYPE_UNCACHABLE, 0);
	if (mtrr < 0) {
		dev_err(&pdev->dev, "%d - %s: mtrr_add failed (%d)\n",
			__LINE__, __func__, mtrr);
		mtrr = -2;
		goto err_out;
	}

	err = mtrr_del(mtrr, mem_start, mem_size);
	if (err < 0) {
		dev_err(&pdev->dev, "%d - %s: mtrr_del failed (%d)\n",
			__LINE__, __func__, err);
		mtrr = -2;
		goto err_out;
	}

	mtrr = mtrr_add(mem_start, mem_size, MTRR_TYPE_WRBACK, 0);
	if (mtrr < 0) {
		/* Stop, but not an error as this may be already be setup */
		dev_dbg(&pdev->dev,
			"%d - %s: mtrr_add failed (%d) - probably means the mtrr is already setup\n",
			__LINE__, __func__, mtrr);
		mtrr = -1;
		goto err_out;
	}

	err = mtrr_del(mtrr, mem_start, mem_size);
	if (err < 0) {
		dev_err(&pdev->dev, "%d - %s: mtrr_del failed (%d)\n",
			__LINE__, __func__, err);
		mtrr = -2;
		goto err_out;
	}

	if (mtrr == 0) {
		/* Replace 0 with a non-overlapping WRBACK mtrr */
		err = mtrr_add(0, mem_start, MTRR_TYPE_WRBACK, 0);
		if (err < 0) {
			dev_err(&pdev->dev, "%d - %s: mtrr_add failed (%d)\n",
				__LINE__, __func__, err);
			mtrr = -2;
			goto err_out;
		}
	}

	mtrr = mtrr_add(mem_start, mem_size, MTRR_TYPE_WRCOMB, 0);
	if (mtrr < 0) {
		dev_err(&pdev->dev, "%d - %s: mtrr_add failed (%d)\n",
			__LINE__, __func__, mtrr);
		mtrr = -1;
	}

err_out:
	return mtrr;
}
#endif /* defined(CONFIG_MTRR) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)) */

int tc_mtrr_setup(struct tc_device *tc)
{
	int err = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	/* Register the LMA as write combined */
	err = arch_io_reserve_memtype_wc(tc->tc_mem.base,
					 tc->tc_mem.size);
	if (err)
		return -ENODEV;
#endif
	/* Enable write combining */
	tc->mtrr = arch_phys_wc_add(tc->tc_mem.base,
				    tc->tc_mem.size);
	if (tc->mtrr < 0) {
		err = -ENODEV;
		goto err_out;
	}

#elif defined(CONFIG_MTRR)
	/* Enable mtrr region caching */
	tc->mtrr = mtrr_setup(tc->pdev,
			      tc->tc_mem.base,
			      tc->tc_mem.size);
	if (tc->mtrr == -2) {
		err = -ENODEV;
		goto err_out;
	}
#endif
	return err;

err_out:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
	arch_io_free_memtype_wc(tc->tc_mem.base,
				tc->tc_mem.size);
#endif
	return err;
}

void tc_mtrr_cleanup(struct tc_device *tc)
{
	if (tc->mtrr >= 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
		arch_phys_wc_del(tc->mtrr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
		arch_io_free_memtype_wc(tc->tc_mem.base,
					tc->tc_mem.size);
#endif
#elif defined(CONFIG_MTRR)
		int err;

		err = mtrr_del(tc->mtrr,
			       tc->tc_mem.base,
			       tc->tc_mem.size);
		if (err < 0)
			dev_err(&tc->pdev->dev,
				"mtrr_del failed (%d)\n", err);
#endif
	}
}

int tc_is_interface_aligned(u32 eyes, u32 clk_taps, u32 train_ack)
{
	u32	max_eye_start = eyes >> 16;
	u32	min_eye_end   = eyes & 0xffff;

	/* If either the training or training ack failed, we haven't aligned */
	if (!(clk_taps & 0x10000) || !(train_ack & 0x100))
		return 0;

	/* If the max eye >= min eye it means the readings are nonsense */
	if (max_eye_start >= min_eye_end)
		return 0;

	/* If we failed the ack pattern more than 4 times */
	if (((train_ack & 0xf0) >> 4) > 4)
		return 0;

	/* If there is less than 7 taps (240ps @40ps/tap, this number should be
	 * lower for the fpga, since its taps are bigger We should really
	 * calculate the "7" based on the interface clock speed.
	 */
	if ((min_eye_end - max_eye_start) < 7)
		return 0;

	return 1;
}

int tc_iopol32_nonzero(u32 mask, void __iomem *addr)
{
	int polnum;
	u32 read_value;

	for (polnum = 0; polnum < 50; polnum++) {
		read_value = ioread32(addr) & mask;
		if (read_value != 0)
			break;
		msleep(20);
	}
	if (polnum == 50) {
		pr_err(DRV_NAME " iopol32_nonzero timeout\n");
		return -ETIME;
	}
	return 0;
}

int request_pci_io_addr(struct pci_dev *pdev, u32 index,
	resource_size_t offset, resource_size_t length)
{
	resource_size_t start, end;

	start = pci_resource_start(pdev, index);
	end = pci_resource_end(pdev, index);

	if ((start + offset + length - 1) > end)
		return -EIO;
	if (pci_resource_flags(pdev, index) & IORESOURCE_IO) {
		if (request_region(start + offset, length, DRV_NAME) == NULL)
			return -EIO;
	} else {
		if (request_mem_region(start + offset, length, DRV_NAME)
			== NULL)
			return -EIO;
	}
	return 0;
}

void release_pci_io_addr(struct pci_dev *pdev, u32 index,
	resource_size_t start, resource_size_t length)
{
	if (pci_resource_flags(pdev, index) & IORESOURCE_IO)
		release_region(start, length);
	else
		release_mem_region(start, length);
}

int setup_io_region(struct pci_dev *pdev,
	struct tc_io_region *region, u32 index,
	resource_size_t offset,	resource_size_t size)
{
	int err;
	resource_size_t pci_phys_addr;

	err = request_pci_io_addr(pdev, index, offset, size);
	if (err) {
		dev_err(&pdev->dev,
			"Failed to request tc registers (err=%d)\n", err);
		return -EIO;
	}
	pci_phys_addr = pci_resource_start(pdev, index);
	region->region.base = pci_phys_addr + offset;
	region->region.size = size;

	region->registers
		= ioremap_nocache(region->region.base, region->region.size);

	if (!region->registers) {
		dev_err(&pdev->dev, "Failed to map tc registers\n");
		release_pci_io_addr(pdev, index,
			region->region.base, region->region.size);
		return -EIO;
	}
	return 0;
}

#if defined(TC_FAKE_INTERRUPTS)
void tc_irq_fake_wrapper(unsigned long data)
{
	struct tc_device *tc = (struct tc_device *)data;

	if (tc->odin)
		odin_irq_handler(0, tc);
	else
		apollo_irq_handler(0, tc);

	mod_timer(&tc->timer,
		jiffies + msecs_to_jiffies(FAKE_INTERRUPT_TIME_MS));
}
#endif

static int tc_register_pdp_device(struct tc_device *tc)
{
	int err = 0;

	if (tc->odin)
		err = odin_register_pdp_device(tc);
	else
		err = apollo_register_pdp_device(tc);

	return err;
}

static int tc_register_ext_device(struct tc_device *tc)
{
	int err = 0;

	if (tc->odin)
		err = odin_register_ext_device(tc);
	else
		err = apollo_register_ext_device(tc);

	return err;
}

static void tc_devres_release(struct device *dev, void *res)
{
	/* No extra cleanup needed */
}

static int tc_cleanup(struct pci_dev *pdev)
{
	struct tc_device *tc = devres_find(&pdev->dev,
					   tc_devres_release, NULL, NULL);
	int i, err = 0;

	if (!tc) {
		dev_err(&pdev->dev, "No tc device resources found\n");
		return -ENODEV;
	}

	debugfs_remove(tc->debugfs_rogue_name);

	for (i = 0; i < TC_INTERRUPT_COUNT; i++)
		if (tc->interrupt_handlers[i].enabled)
			tc_disable_interrupt(&pdev->dev, i);

	if (tc->odin)
		err = odin_cleanup(tc);
	else
		err = apollo_cleanup(tc);

	debugfs_remove(tc->debugfs_tc_dir);

	return err;
}

static int tc_init(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct tc_device *tc;
	int err = 0;
#if defined(SUPPORT_FAKE_SECURE_ION_HEAP)
	int sec_mem_size = TC_SECURE_MEM_SIZE_BYTES;
#else /* defined(SUPPORT_FAKE_SECURE_ION_HEAP) */
	int sec_mem_size = 0;
#endif /* defined(SUPPORT_FAKE_SECURE_ION_HEAP) */

	if (!devres_open_group(&pdev->dev, NULL, GFP_KERNEL))
		return -ENOMEM;

	tc = devres_alloc(tc_devres_release,
		sizeof(*tc), GFP_KERNEL);
	if (!tc) {
		err = -ENOMEM;
		goto err_out;
	}

	devres_add(&pdev->dev, tc);

	err = tc_enable(&pdev->dev);
	if (err) {
		dev_err(&pdev->dev,
			"tc_enable failed %d\n", err);
		goto err_release;
	}

	tc->pdev = pdev;

	spin_lock_init(&tc->interrupt_handler_lock);
	spin_lock_init(&tc->interrupt_enable_lock);

	tc->debugfs_tc_dir = debugfs_create_dir(DRV_NAME, NULL);

	if (pdev->vendor == PCI_VENDOR_ID_ODIN &&
	    pdev->device == DEVICE_ID_ODIN) {

		dev_info(&pdev->dev, "Odin detected");
		tc->odin = true;

		err = odin_init(tc, pdev,
				tc_core_clock, tc_mem_clock,
				tc_pdp_mem_size, sec_mem_size,
				tc_mem_latency, tc_wresp_latency);
		if (err)
			goto err_dev_cleanup;

	} else {
		dev_info(&pdev->dev, "Apollo detected");
		tc->odin = false;

		err = apollo_init(tc, pdev,
				  tc_core_clock, tc_mem_clock, tc_sys_clock,
				  tc_pdp_mem_size, sec_mem_size,
				  tc_mem_latency, tc_wresp_latency);
		if (err)
			goto err_dev_cleanup;
	}

	/* Add the rogue name debugfs entry */
	tc->debugfs_rogue_name =
		debugfs_create_blob("rogue-name", 0444,
			tc->debugfs_tc_dir,
			&tc_debugfs_rogue_name_blobs[tc->version]);

#if defined(TC_FAKE_INTERRUPTS)
	dev_warn(&pdev->dev, "WARNING: Faking interrupts every %d ms",
		FAKE_INTERRUPT_TIME_MS);
#endif

	/* Register pdp and ext platform devices */
	err = tc_register_pdp_device(tc);
	if (err)
		goto err_dev_cleanup;

	err = tc_register_ext_device(tc);
	if (err)
		goto err_dev_cleanup;

	devres_remove_group(&pdev->dev, NULL);

err_out:
	if (err)
		dev_err(&pdev->dev, "tc_init failed\n");

	return err;

err_dev_cleanup:
	tc_cleanup(pdev);
	tc_disable(&pdev->dev);
err_release:
	devres_release_group(&pdev->dev, NULL);
	goto err_out;
}

static void tc_exit(struct pci_dev *pdev)
{
	struct tc_device *tc = devres_find(&pdev->dev,
					   tc_devres_release, NULL, NULL);

	if (!tc) {
		dev_err(&pdev->dev, "No tc device resources found\n");
		return;
	}

	if (tc->pdp_dev)
		platform_device_unregister(tc->pdp_dev);

	if (tc->ext_dev)
		platform_device_unregister(tc->ext_dev);

	tc_cleanup(pdev);

	tc_disable(&pdev->dev);
}

static struct pci_device_id tc_pci_tbl[] = {
	{ PCI_VDEVICE(POWERVR, DEVICE_ID_PCI_APOLLO_FPGA) },
	{ PCI_VDEVICE(POWERVR, DEVICE_ID_PCIE_APOLLO_FPGA) },
	{ PCI_VDEVICE(ODIN, DEVICE_ID_ODIN) },
	{ },
};

static struct pci_driver tc_pci_driver = {
	.name		= DRV_NAME,
	.id_table	= tc_pci_tbl,
	.probe		= tc_init,
	.remove		= tc_exit,
};

module_pci_driver(tc_pci_driver);

MODULE_DEVICE_TABLE(pci, tc_pci_tbl);

int tc_enable(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	return pci_enable_device(pdev);
}
EXPORT_SYMBOL(tc_enable);

void tc_disable(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);

	pci_disable_device(pdev);
}
EXPORT_SYMBOL(tc_disable);

int tc_set_interrupt_handler(struct device *dev, int interrupt_id,
	void (*handler_function)(void *), void *data)
{
	struct tc_device *tc = devres_find(dev, tc_devres_release,
		NULL, NULL);
	int err = 0;
	unsigned long flags;

	if (!tc) {
		dev_err(dev, "No tc device resources found\n");
		err = -ENODEV;
		goto err_out;
	}

	if (interrupt_id < 0 || interrupt_id >= TC_INTERRUPT_COUNT) {
		dev_err(dev, "Invalid interrupt ID (%d)\n", interrupt_id);
		err = -EINVAL;
		goto err_out;
	}

	spin_lock_irqsave(&tc->interrupt_handler_lock, flags);

	tc->interrupt_handlers[interrupt_id].handler_function =
		handler_function;
	tc->interrupt_handlers[interrupt_id].handler_data = data;

	spin_unlock_irqrestore(&tc->interrupt_handler_lock, flags);

err_out:
	return err;
}
EXPORT_SYMBOL(tc_set_interrupt_handler);

int tc_enable_interrupt(struct device *dev, int interrupt_id)
{
	struct tc_device *tc = devres_find(dev, tc_devres_release,
		NULL, NULL);
	int err = 0;
	unsigned long flags;

	if (!tc) {
		dev_err(dev, "No tc device resources found\n");
		err = -ENODEV;
		goto err_out;
	}
	if (interrupt_id < 0 || interrupt_id >= TC_INTERRUPT_COUNT) {
		dev_err(dev, "Invalid interrupt ID (%d)\n", interrupt_id);
		err = -EINVAL;
		goto err_out;
	}
	spin_lock_irqsave(&tc->interrupt_enable_lock, flags);

	if (tc->interrupt_handlers[interrupt_id].enabled) {
		dev_warn(dev, "Interrupt ID %d already enabled\n",
			interrupt_id);
		err = -EEXIST;
		goto err_unlock;
	}
	tc->interrupt_handlers[interrupt_id].enabled = true;

	if (tc->odin)
		odin_enable_interrupt_register(tc, interrupt_id);
	else
		apollo_enable_interrupt_register(tc, interrupt_id);

err_unlock:
	spin_unlock_irqrestore(&tc->interrupt_enable_lock, flags);
err_out:
	return err;
}
EXPORT_SYMBOL(tc_enable_interrupt);

int tc_disable_interrupt(struct device *dev, int interrupt_id)
{
	struct tc_device *tc = devres_find(dev, tc_devres_release,
		NULL, NULL);
	int err = 0;
	unsigned long flags;

	if (!tc) {
		dev_err(dev, "No tc device resources found\n");
		err = -ENODEV;
		goto err_out;
	}
	if (interrupt_id < 0 || interrupt_id >= TC_INTERRUPT_COUNT) {
		dev_err(dev, "Invalid interrupt ID (%d)\n", interrupt_id);
		err = -EINVAL;
		goto err_out;
	}
	spin_lock_irqsave(&tc->interrupt_enable_lock, flags);

	if (!tc->interrupt_handlers[interrupt_id].enabled) {
		dev_warn(dev, "Interrupt ID %d already disabled\n",
			interrupt_id);
	}
	tc->interrupt_handlers[interrupt_id].enabled = false;

	if (tc->odin)
		odin_disable_interrupt_register(tc, interrupt_id);
	else
		apollo_disable_interrupt_register(tc, interrupt_id);

	spin_unlock_irqrestore(&tc->interrupt_enable_lock, flags);
err_out:
	return err;
}
EXPORT_SYMBOL(tc_disable_interrupt);

int tc_sys_info(struct device *dev, u32 *tmp, u32 *pll)
{
	int err = -ENODEV;
	struct tc_device *tc = devres_find(dev, tc_devres_release,
		NULL, NULL);

	if (!tc) {
		dev_err(dev, "No tc device resources found\n");
		goto err_out;
	}

	if (tc->odin)
		err = odin_sys_info(tc, tmp, pll);
	else
		err = apollo_sys_info(tc, tmp, pll);

err_out:
	return err;
}
EXPORT_SYMBOL(tc_sys_info);

int tc_sys_strings(struct device *dev,
		   char *str_fpga_rev, size_t size_fpga_rev,
		   char *str_tcf_core_rev, size_t size_tcf_core_rev,
		   char *str_tcf_core_target_build_id,
		   size_t size_tcf_core_target_build_id,
		   char *str_pci_ver, size_t size_pci_ver,
		   char *str_macro_ver, size_t size_macro_ver)
{
	int err = -ENODEV;

	struct tc_device *tc = devres_find(dev, tc_devres_release,
		NULL, NULL);

	if (!tc) {
		dev_err(dev, "No tc device resources found\n");
		goto err_out;
	}

	if (!str_fpga_rev ||
	    !size_fpga_rev ||
	    !str_tcf_core_rev ||
	    !size_tcf_core_rev ||
	    !str_tcf_core_target_build_id ||
	    !size_tcf_core_target_build_id ||
	    !str_pci_ver ||
	    !size_pci_ver ||
	    !str_macro_ver ||
	    !size_macro_ver) {

		err = -EINVAL;
		goto err_out;
	}

	if (tc->odin) {
		err = odin_sys_strings(tc,
				 str_fpga_rev, size_fpga_rev,
				 str_tcf_core_rev, size_tcf_core_rev,
				 str_tcf_core_target_build_id,
				 size_tcf_core_target_build_id,
				 str_pci_ver, size_pci_ver,
				 str_macro_ver, size_macro_ver);
	} else {
		err = apollo_sys_strings(tc,
				 str_fpga_rev, size_fpga_rev,
				 str_tcf_core_rev, size_tcf_core_rev,
				 str_tcf_core_target_build_id,
				 size_tcf_core_target_build_id,
				 str_pci_ver, size_pci_ver,
				 str_macro_ver, size_macro_ver);
	}

err_out:
	return err;
}
EXPORT_SYMBOL(tc_sys_strings);

int tc_core_clock_speed(struct device *dev)
{
	return tc_core_clock;
}
EXPORT_SYMBOL(tc_core_clock_speed);

