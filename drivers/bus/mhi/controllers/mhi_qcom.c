/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <asm/arch_timer.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mhi.h>
#include "mhi_qcom.h"

struct firmware_info {
	unsigned int dev_id;
	const char *fw_image;
	const char *edl_image;
};

static const struct firmware_info firmware_table[] = {
	{.dev_id = 0x307, .fw_image = "sdx60m/sbl1.mbn"},
	{.dev_id = 0x306, .fw_image = "sdx55m/sbl1.mbn"},
	{.dev_id = 0x305, .fw_image = "sdx50m/sbl1.mbn"},
	{.dev_id = 0x304, .fw_image = "sbl.mbn", .edl_image = "edl.mbn"},
	/* default, set to debug.mbn */
	{.fw_image = "debug.mbn"},
};

static int debug_mode;
module_param_named(debug_mode, debug_mode, int, 0644);

int mhi_debugfs_trigger_m0(void *data, u64 val)
{
	struct mhi_controller *mhi_cntrl = data;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);

	MHI_LOG("Trigger M3 Exit\n");
	pm_runtime_get(&mhi_dev->pci_dev->dev);
	pm_runtime_put(&mhi_dev->pci_dev->dev);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debugfs_trigger_m0_fops, NULL,
			mhi_debugfs_trigger_m0, "%llu\n");

int mhi_debugfs_trigger_m3(void *data, u64 val)
{
	struct mhi_controller *mhi_cntrl = data;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);

	MHI_LOG("Trigger M3 Entry\n");
	pm_runtime_mark_last_busy(&mhi_dev->pci_dev->dev);
	pm_request_autosuspend(&mhi_dev->pci_dev->dev);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(debugfs_trigger_m3_fops, NULL,
			mhi_debugfs_trigger_m3, "%llu\n");

void mhi_deinit_pci_dev(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct pci_dev *pci_dev = mhi_dev->pci_dev;

	pm_runtime_mark_last_busy(&pci_dev->dev);
	pm_runtime_dont_use_autosuspend(&pci_dev->dev);
	pm_runtime_disable(&pci_dev->dev);

	/* reset counter for lpm state changes */
	mhi_dev->lpm_disable_depth = 0;

	pci_free_irq_vectors(pci_dev);
	kfree(mhi_cntrl->irq);
	mhi_cntrl->irq = NULL;
	iounmap(mhi_cntrl->regs);
	mhi_cntrl->regs = NULL;
	pci_clear_master(pci_dev);
	pci_release_region(pci_dev, mhi_dev->resn);
	pci_disable_device(pci_dev);
}

static int mhi_init_pci_dev(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct pci_dev *pci_dev = mhi_dev->pci_dev;
	int ret;
	resource_size_t len;
	int i;

	mhi_dev->resn = MHI_PCI_BAR_NUM;
	ret = pci_assign_resource(pci_dev, mhi_dev->resn);
	if (ret) {
		MHI_ERR("Error assign pci resources, ret:%d\n", ret);
		return ret;
	}

	ret = pci_enable_device(pci_dev);
	if (ret) {
		MHI_ERR("Error enabling device, ret:%d\n", ret);
		goto error_enable_device;
	}

	ret = pci_request_region(pci_dev, mhi_dev->resn, "mhi");
	if (ret) {
		MHI_ERR("Error pci_request_region, ret:%d\n", ret);
		goto error_request_region;
	}

	pci_set_master(pci_dev);

	mhi_cntrl->base_addr = pci_resource_start(pci_dev, mhi_dev->resn);
	len = pci_resource_len(pci_dev, mhi_dev->resn);
	mhi_cntrl->regs = ioremap_nocache(mhi_cntrl->base_addr, len);
	if (!mhi_cntrl->regs) {
		MHI_ERR("Error ioremap region\n");
		goto error_ioremap;
	}

	ret = pci_alloc_irq_vectors(pci_dev, mhi_cntrl->msi_required,
				    mhi_cntrl->msi_required, PCI_IRQ_MSI);
	if (IS_ERR_VALUE((ulong)ret) || ret < mhi_cntrl->msi_required) {
		MHI_ERR("Failed to enable MSI, ret:%d\n", ret);
		goto error_req_msi;
	}

	mhi_cntrl->msi_allocated = ret;
	mhi_cntrl->irq = kmalloc_array(mhi_cntrl->msi_allocated,
				       sizeof(*mhi_cntrl->irq), GFP_KERNEL);
	if (!mhi_cntrl->irq) {
		ret = -ENOMEM;
		goto error_alloc_msi_vec;
	}

	for (i = 0; i < mhi_cntrl->msi_allocated; i++) {
		mhi_cntrl->irq[i] = pci_irq_vector(pci_dev, i);
		if (mhi_cntrl->irq[i] < 0) {
			ret = mhi_cntrl->irq[i];
			goto error_get_irq_vec;
		}
	}

	dev_set_drvdata(&pci_dev->dev, mhi_cntrl);

	/* configure runtime pm */
	pm_runtime_set_autosuspend_delay(&pci_dev->dev, MHI_RPM_SUSPEND_TMR_MS);
	pm_runtime_use_autosuspend(&pci_dev->dev);
	pm_suspend_ignore_children(&pci_dev->dev, true);

	/*
	 * pci framework will increment usage count (twice) before
	 * calling local device driver probe function.
	 * 1st pci.c pci_pm_init() calls pm_runtime_forbid
	 * 2nd pci-driver.c local_pci_probe calls pm_runtime_get_sync
	 * Framework expect pci device driver to call
	 * pm_runtime_put_noidle to decrement usage count after
	 * successful probe and and call pm_runtime_allow to enable
	 * runtime suspend.
	 */
	pm_runtime_mark_last_busy(&pci_dev->dev);
	pm_runtime_put_noidle(&pci_dev->dev);

	return 0;

error_get_irq_vec:
	kfree(mhi_cntrl->irq);
	mhi_cntrl->irq = NULL;

error_alloc_msi_vec:
	pci_free_irq_vectors(pci_dev);

error_req_msi:
	iounmap(mhi_cntrl->regs);

error_ioremap:
	pci_clear_master(pci_dev);

error_request_region:
	pci_disable_device(pci_dev);

error_enable_device:
	pci_release_region(pci_dev, mhi_dev->resn);

	return ret;
}

static int mhi_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct mhi_controller *mhi_cntrl = dev_get_drvdata(dev);
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);

	MHI_LOG("Enter\n");

	mutex_lock(&mhi_cntrl->pm_mutex);

	if (!mhi_dev->powered_on) {
		MHI_LOG("Not fully powered, return success\n");
		mutex_unlock(&mhi_cntrl->pm_mutex);
		return 0;
	}

	ret = mhi_pm_suspend(mhi_cntrl);

	if (ret) {
		MHI_LOG("Abort due to ret:%d\n", ret);
		mhi_dev->suspend_mode = MHI_ACTIVE_STATE;
		goto exit_runtime_suspend;
	}

	mhi_dev->suspend_mode = MHI_DEFAULT_SUSPEND;

	ret = mhi_arch_link_suspend(mhi_cntrl);

	/* failed suspending link abort mhi suspend */
	if (ret) {
		MHI_LOG("Failed to suspend link, abort suspend\n");
		mhi_pm_resume(mhi_cntrl);
		mhi_dev->suspend_mode = MHI_ACTIVE_STATE;
	}

exit_runtime_suspend:
	mutex_unlock(&mhi_cntrl->pm_mutex);
	MHI_LOG("Exited with ret:%d\n", ret);

	return (ret < 0) ? -EBUSY : 0;
}

static int mhi_runtime_idle(struct device *dev)
{
	struct mhi_controller *mhi_cntrl = dev_get_drvdata(dev);

	MHI_LOG("Entered returning -EBUSY\n");

	/*
	 * RPM framework during runtime resume always calls
	 * rpm_idle to see if device ready to suspend.
	 * If dev.power usage_count count is 0, rpm fw will call
	 * rpm_idle cb to see if device is ready to suspend.
	 * if cb return 0, or cb not defined the framework will
	 * assume device driver is ready to suspend;
	 * therefore, fw will schedule runtime suspend.
	 * In MHI power management, MHI host shall go to
	 * runtime suspend only after entering MHI State M2, even if
	 * usage count is 0.  Return -EBUSY to disable automatic suspend.
	 */
	return -EBUSY;
}

static int mhi_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct mhi_controller *mhi_cntrl = dev_get_drvdata(dev);
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);

	MHI_LOG("Enter\n");

	mutex_lock(&mhi_cntrl->pm_mutex);

	if (!mhi_dev->powered_on) {
		MHI_LOG("Not fully powered, return success\n");
		mutex_unlock(&mhi_cntrl->pm_mutex);
		return 0;
	}

	/* turn on link */
	ret = mhi_arch_link_resume(mhi_cntrl);
	if (ret)
		goto rpm_resume_exit;


	/* transition to M0 state */
	if (mhi_dev->suspend_mode == MHI_DEFAULT_SUSPEND)
		ret = mhi_pm_resume(mhi_cntrl);
	else
		ret = mhi_pm_fast_resume(mhi_cntrl, MHI_FAST_LINK_ON);

	mhi_dev->suspend_mode = MHI_ACTIVE_STATE;

rpm_resume_exit:
	mutex_unlock(&mhi_cntrl->pm_mutex);
	MHI_LOG("Exited with :%d\n", ret);

	return (ret < 0) ? -EBUSY : 0;
}

static int mhi_system_resume(struct device *dev)
{
	return mhi_runtime_resume(dev);
}

int mhi_system_suspend(struct device *dev)
{
	struct mhi_controller *mhi_cntrl = dev_get_drvdata(dev);
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	int ret;

	MHI_LOG("Entered\n");

	mutex_lock(&mhi_cntrl->pm_mutex);

	if (!mhi_dev->powered_on) {
		MHI_LOG("Not fully powered, return success\n");
		mutex_unlock(&mhi_cntrl->pm_mutex);
		return 0;
	}

	/*
	 * pci framework always makes a dummy vote to rpm
	 * framework to resume before calling system suspend
	 * hence usage count is minimum one
	 */
	if (atomic_read(&dev->power.usage_count) > 1) {
		/*
		 * clients have requested to keep link on, try
		 * fast suspend. No need to notify clients since
		 * we will not be turning off the pcie link
		 */
		ret = mhi_pm_fast_suspend(mhi_cntrl, false);
		mhi_dev->suspend_mode = MHI_FAST_LINK_ON;
	} else {
		/* try normal suspend */
		mhi_dev->suspend_mode = MHI_DEFAULT_SUSPEND;
		ret = mhi_pm_suspend(mhi_cntrl);

		/*
		 * normal suspend failed because we're busy, try
		 * fast suspend before aborting system suspend.
		 * this could happens if client has disabled
		 * device lpm but no active vote for PCIe from
		 * apps processor
		 */
		if (ret == -EBUSY) {
			ret = mhi_pm_fast_suspend(mhi_cntrl, true);
			mhi_dev->suspend_mode = MHI_FAST_LINK_ON;
		}
	}

	if (ret) {
		MHI_LOG("Abort due to ret:%d\n", ret);
		mhi_dev->suspend_mode = MHI_ACTIVE_STATE;
		goto exit_system_suspend;
	}

	ret = mhi_arch_link_suspend(mhi_cntrl);

	/* failed suspending link abort mhi suspend */
	if (ret) {
		MHI_LOG("Failed to suspend link, abort suspend\n");
		if (mhi_dev->suspend_mode == MHI_DEFAULT_SUSPEND)
			mhi_pm_resume(mhi_cntrl);
		else
			mhi_pm_fast_resume(mhi_cntrl, MHI_FAST_LINK_OFF);

		mhi_dev->suspend_mode = MHI_ACTIVE_STATE;
	}

exit_system_suspend:
	mutex_unlock(&mhi_cntrl->pm_mutex);

	MHI_LOG("Exit with ret:%d\n", ret);

	return ret;
}

static int mhi_force_suspend(struct mhi_controller *mhi_cntrl)
{
	int ret = -EIO;
	const u32 delayms = 100;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	int itr = DIV_ROUND_UP(mhi_cntrl->timeout_ms, delayms);

	MHI_LOG("Entered\n");

	mutex_lock(&mhi_cntrl->pm_mutex);

	for (; itr; itr--) {
		/*
		 * This function get called soon as device entered mission mode
		 * so most of the channels are still in disabled state. However,
		 * sbl channels are active and clients could be trying to close
		 * channels while we trying to suspend the link. So, we need to
		 * re-try if MHI is busy
		 */
		ret = mhi_pm_suspend(mhi_cntrl);
		if (!ret || ret != -EBUSY)
			break;

		MHI_LOG("MHI busy, sleeping and retry\n");
		msleep(delayms);
	}

	if (ret)
		goto exit_force_suspend;

	mhi_dev->suspend_mode = MHI_DEFAULT_SUSPEND;
	ret = mhi_arch_link_suspend(mhi_cntrl);

exit_force_suspend:
	MHI_LOG("Force suspend ret with %d\n", ret);

	mutex_unlock(&mhi_cntrl->pm_mutex);

	return ret;
}

/* checks if link is down */
static int mhi_link_status(struct mhi_controller *mhi_cntrl, void *priv)
{
	struct mhi_dev *mhi_dev = priv;
	u16 dev_id;
	int ret;

	/* try reading device id, if dev id don't match, link is down */
	ret = pci_read_config_word(mhi_dev->pci_dev, PCI_DEVICE_ID, &dev_id);

	return (ret || dev_id != mhi_cntrl->dev_id) ? -EIO : 0;
}

/* disable PCIe L1 */
static int mhi_lpm_disable(struct mhi_controller *mhi_cntrl, void *priv)
{
	struct mhi_dev *mhi_dev = priv;
	struct pci_dev *pci_dev = mhi_dev->pci_dev;
	int lnkctl = pci_dev->pcie_cap + PCI_EXP_LNKCTL;
	u8 val;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&mhi_dev->lpm_lock, flags);

	/* L1 is already disabled */
	if (mhi_dev->lpm_disable_depth) {
		mhi_dev->lpm_disable_depth++;
		goto lpm_disable_exit;
	}

	ret = pci_read_config_byte(pci_dev, lnkctl, &val);
	if (ret) {
		MHI_ERR("Error reading LNKCTL, ret:%d\n", ret);
		goto lpm_disable_exit;
	}

	/* L1 is not supported, do not increment lpm_disable_depth */
	if (unlikely(!(val & PCI_EXP_LNKCTL_ASPM_L1)))
		goto lpm_disable_exit;

	val &= ~PCI_EXP_LNKCTL_ASPM_L1;
	ret = pci_write_config_byte(pci_dev, lnkctl, val);
	if (ret) {
		MHI_ERR("Error writing LNKCTL to disable LPM, ret:%d\n", ret);
		goto lpm_disable_exit;
	}

	mhi_dev->lpm_disable_depth++;

lpm_disable_exit:
	spin_unlock_irqrestore(&mhi_dev->lpm_lock, flags);

	return ret;
}

/* enable PCIe L1 */
static int mhi_lpm_enable(struct mhi_controller *mhi_cntrl, void *priv)
{
	struct mhi_dev *mhi_dev = priv;
	struct pci_dev *pci_dev = mhi_dev->pci_dev;
	int lnkctl = pci_dev->pcie_cap + PCI_EXP_LNKCTL;
	u8 val;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&mhi_dev->lpm_lock, flags);

	/*
	 * Exit if L1 is not supported or is already disabled or
	 * decrementing lpm_disable_depth still keeps it above 0
	 */
	if (!mhi_dev->lpm_disable_depth)
		goto lpm_enable_exit;

	if (mhi_dev->lpm_disable_depth > 1) {
		mhi_dev->lpm_disable_depth--;
		goto lpm_enable_exit;
	}

	ret = pci_read_config_byte(pci_dev, lnkctl, &val);
	if (ret) {
		MHI_ERR("Error reading LNKCTL, ret:%d\n", ret);
		goto lpm_enable_exit;
	}

	val |= PCI_EXP_LNKCTL_ASPM_L1;
	ret = pci_write_config_byte(pci_dev, lnkctl, val);
	if (ret) {
		MHI_ERR("Error writing LNKCTL to enable LPM, ret:%d\n", ret);
		goto lpm_enable_exit;
	}

	mhi_dev->lpm_disable_depth = 0;

lpm_enable_exit:
	spin_unlock_irqrestore(&mhi_dev->lpm_lock, flags);

	return ret;
}

static int mhi_qcom_power_up(struct mhi_controller *mhi_cntrl)
{
	enum mhi_dev_state dev_state = mhi_get_mhi_state(mhi_cntrl);
	const u32 delayus = 10;
	int itr = DIV_ROUND_UP(mhi_cntrl->timeout_ms * 1000, delayus);
	int ret;

	/*
	 * It's possible device did not go thru a cold reset before
	 * power up and still in error state. If device in error state,
	 * we need to trigger a soft reset before continue with power
	 * up
	 */
	if (dev_state == MHI_STATE_SYS_ERR) {
		mhi_set_mhi_state(mhi_cntrl, MHI_STATE_RESET);
		while (itr--) {
			dev_state = mhi_get_mhi_state(mhi_cntrl);
			if (dev_state != MHI_STATE_SYS_ERR)
				break;
			usleep_range(delayus, delayus << 1);
		}
		/* device still in error state, abort power up */
		if (dev_state == MHI_STATE_SYS_ERR)
			return -EIO;
	}

	/* when coming out of SSR, initial ee state is not valid */
	mhi_cntrl->ee = 0;

	ret = mhi_arch_power_up(mhi_cntrl);
	if (ret)
		return ret;

	ret = mhi_async_power_up(mhi_cntrl);

	/* power up create the dentry */
	if (mhi_cntrl->dentry) {
		debugfs_create_file("m0", 0444, mhi_cntrl->dentry, mhi_cntrl,
				    &debugfs_trigger_m0_fops);
		debugfs_create_file("m3", 0444, mhi_cntrl->dentry, mhi_cntrl,
				    &debugfs_trigger_m3_fops);
	}

	return ret;
}

static int mhi_runtime_get(struct mhi_controller *mhi_cntrl, void *priv)
{
	struct mhi_dev *mhi_dev = priv;
	struct device *dev = &mhi_dev->pci_dev->dev;

	return pm_runtime_get(dev);
}

static void mhi_runtime_put(struct mhi_controller *mhi_cntrl, void *priv)
{
	struct mhi_dev *mhi_dev = priv;
	struct device *dev = &mhi_dev->pci_dev->dev;

	pm_runtime_put_noidle(dev);
}

static void mhi_status_cb(struct mhi_controller *mhi_cntrl,
			  void *priv,
			  enum MHI_CB reason)
{
	struct mhi_dev *mhi_dev = priv;
	struct device *dev = &mhi_dev->pci_dev->dev;
	int ret;

	switch (reason) {
	case MHI_CB_IDLE:
		MHI_LOG("Schedule runtime suspend\n");
		pm_runtime_mark_last_busy(dev);
		pm_request_autosuspend(dev);
		break;
	case MHI_CB_EE_MISSION_MODE:
		/*
		 * we need to force a suspend so device can switch to
		 * mission mode pcie phy settings.
		 */
		pm_runtime_get(dev);
		ret = mhi_force_suspend(mhi_cntrl);
		if (!ret)
			mhi_runtime_resume(dev);
		pm_runtime_put(dev);
		break;
	default:
		MHI_ERR("Unhandled cb:0x%x\n", reason);
	}
}

/* capture host SoC XO time in ticks */
static u64 mhi_time_get(struct mhi_controller *mhi_cntrl, void *priv)
{
	return arch_counter_get_cntvct();
}

static ssize_t timeout_ms_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct mhi_device *mhi_dev = to_mhi_device(dev);
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;

	/* buffer provided by sysfs has a minimum size of PAGE_SIZE */
	return snprintf(buf, PAGE_SIZE, "%u\n", mhi_cntrl->timeout_ms);
}

static ssize_t timeout_ms_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	struct mhi_device *mhi_dev = to_mhi_device(dev);
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;
	u32 timeout_ms;

	if (kstrtou32(buf, 0, &timeout_ms) < 0)
		return -EINVAL;

	mhi_cntrl->timeout_ms = timeout_ms;

	return count;
}
static DEVICE_ATTR_RW(timeout_ms);

static ssize_t power_up_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf,
			      size_t count)
{
	int ret;
	struct mhi_device *mhi_dev = to_mhi_device(dev);
	struct mhi_controller *mhi_cntrl = mhi_dev->mhi_cntrl;

	ret = mhi_qcom_power_up(mhi_cntrl);
	if (ret)
		return ret;

	return count;
}
static DEVICE_ATTR_WO(power_up);

static struct attribute *mhi_qcom_attrs[] = {
	&dev_attr_timeout_ms.attr,
	&dev_attr_power_up.attr,
	NULL
};

static const struct attribute_group mhi_qcom_group = {
	.attrs = mhi_qcom_attrs,
};

static struct mhi_controller *mhi_register_controller(struct pci_dev *pci_dev)
{
	struct mhi_controller *mhi_cntrl;
	struct mhi_dev *mhi_dev;
	struct device_node *of_node = pci_dev->dev.of_node;
	const struct firmware_info *firmware_info;
	bool use_bb;
	u64 addr_win[2];
	int ret, i;

	if (!of_node)
		return ERR_PTR(-ENODEV);

	mhi_cntrl = mhi_alloc_controller(sizeof(*mhi_dev));
	if (!mhi_cntrl)
		return ERR_PTR(-ENOMEM);

	mhi_dev = mhi_controller_get_devdata(mhi_cntrl);

	mhi_cntrl->domain = pci_domain_nr(pci_dev->bus);
	mhi_cntrl->dev_id = pci_dev->device;
	mhi_cntrl->bus = pci_dev->bus->number;
	mhi_cntrl->slot = PCI_SLOT(pci_dev->devfn);

	ret = of_property_read_u32(of_node, "qcom,smmu-cfg",
				   &mhi_dev->smmu_cfg);
	if (ret)
		goto error_register;

	use_bb = of_property_read_bool(of_node, "mhi,use-bb");
	mhi_dev->allow_m1 = of_property_read_bool(of_node, "mhi,allow-m1");

	/*
	 * if s1 translation enabled or using bounce buffer pull iova addr
	 * from dt
	 */
	if (use_bb || (mhi_dev->smmu_cfg & MHI_SMMU_ATTACH &&
		       !(mhi_dev->smmu_cfg & MHI_SMMU_S1_BYPASS))) {
		ret = of_property_count_elems_of_size(of_node, "qcom,addr-win",
						      sizeof(addr_win));
		if (ret != 1)
			goto error_register;
		ret = of_property_read_u64_array(of_node, "qcom,addr-win",
						 addr_win, 2);
		if (ret)
			goto error_register;
	} else {
		addr_win[0] = memblock_start_of_DRAM();
		addr_win[1] = memblock_end_of_DRAM();
	}

	mhi_dev->iova_start = addr_win[0];
	mhi_dev->iova_stop = addr_win[1];

	/*
	 * If S1 is enabled, set MHI_CTRL start address to 0 so we can use low
	 * level mapping api to map buffers outside of smmu domain
	 */
	if (mhi_dev->smmu_cfg & MHI_SMMU_ATTACH &&
	    !(mhi_dev->smmu_cfg & MHI_SMMU_S1_BYPASS))
		mhi_cntrl->iova_start = 0;
	else
		mhi_cntrl->iova_start = addr_win[0];

	mhi_cntrl->iova_stop = mhi_dev->iova_stop;
	mhi_cntrl->of_node = of_node;

	mhi_dev->pci_dev = pci_dev;
	spin_lock_init(&mhi_dev->lpm_lock);

	/* setup power management apis */
	mhi_cntrl->status_cb = mhi_status_cb;
	mhi_cntrl->runtime_get = mhi_runtime_get;
	mhi_cntrl->runtime_put = mhi_runtime_put;
	mhi_cntrl->link_status = mhi_link_status;

	mhi_cntrl->lpm_disable = mhi_lpm_disable;
	mhi_cntrl->lpm_enable = mhi_lpm_enable;
	mhi_cntrl->time_get = mhi_time_get;
	mhi_cntrl->remote_timer_freq = 19200000;
	mhi_cntrl->local_timer_freq = 19200000;

	ret = of_register_mhi_controller(mhi_cntrl);
	if (ret)
		goto error_register;

	for (i = 0; i < ARRAY_SIZE(firmware_table); i++) {
		firmware_info = firmware_table + i;

		/* debug mode always use default */
		if (!debug_mode && mhi_cntrl->dev_id == firmware_info->dev_id)
			break;
	}

	mhi_cntrl->fw_image = firmware_info->fw_image;
	mhi_cntrl->edl_image = firmware_info->edl_image;

	ret = sysfs_create_group(&mhi_cntrl->mhi_dev->dev.kobj,
				 &mhi_qcom_group);
	if (ret)
		goto error_register;

	if (mhi_dev->allow_m1)
		goto skip_offload;

	mhi_cntrl->offload_wq = alloc_ordered_workqueue("offload_wq",
			WQ_MEM_RECLAIM | WQ_HIGHPRI);
	if (!mhi_cntrl->offload_wq)
		goto error_register;

	INIT_WORK(&mhi_cntrl->reg_write_work, mhi_reg_write_work);

	mhi_cntrl->reg_write_q = kcalloc(REG_WRITE_QUEUE_LEN,
					sizeof(*mhi_cntrl->reg_write_q),
					GFP_KERNEL);
	if (!mhi_cntrl->reg_write_q)
		goto error_free_wq;

	atomic_set(&mhi_cntrl->write_idx, -1);

skip_offload:
	return mhi_cntrl;

error_free_wq:
	destroy_workqueue(mhi_cntrl->offload_wq);
error_register:
	mhi_free_controller(mhi_cntrl);

	return ERR_PTR(-EINVAL);
}

int mhi_pci_probe(struct pci_dev *pci_dev,
		  const struct pci_device_id *device_id)
{
	struct mhi_controller *mhi_cntrl;
	u32 domain = pci_domain_nr(pci_dev->bus);
	u32 bus = pci_dev->bus->number;
	u32 dev_id = pci_dev->device;
	u32 slot = PCI_SLOT(pci_dev->devfn);
	struct mhi_dev *mhi_dev;
	int ret;

	/* see if we already registered */
	mhi_cntrl = mhi_bdf_to_controller(domain, bus, slot, dev_id);
	if (!mhi_cntrl)
		mhi_cntrl = mhi_register_controller(pci_dev);

	if (IS_ERR(mhi_cntrl))
		return PTR_ERR(mhi_cntrl);

	mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	mhi_dev->powered_on = true;

	ret = mhi_arch_pcie_init(mhi_cntrl);
	if (ret)
		return ret;

	ret = mhi_arch_iommu_init(mhi_cntrl);
	if (ret)
		goto error_iommu_init;

	ret = mhi_init_pci_dev(mhi_cntrl);
	if (ret)
		goto error_init_pci;

	/* start power up sequence */
	if (!debug_mode) {
		ret = mhi_qcom_power_up(mhi_cntrl);
		if (ret)
			goto error_power_up;
	}

	pm_runtime_mark_last_busy(&pci_dev->dev);

	MHI_LOG("Return successful\n");

	return 0;

error_power_up:
	mhi_deinit_pci_dev(mhi_cntrl);

error_init_pci:
	mhi_arch_iommu_deinit(mhi_cntrl);

error_iommu_init:
	mhi_arch_pcie_deinit(mhi_cntrl);

	return ret;
}

static const struct dev_pm_ops pm_ops = {
	SET_RUNTIME_PM_OPS(mhi_runtime_suspend,
			   mhi_runtime_resume,
			   mhi_runtime_idle)
	SET_SYSTEM_SLEEP_PM_OPS(mhi_system_suspend, mhi_system_resume)
};

static struct pci_device_id mhi_pcie_device_id[] = {
	{PCI_DEVICE(MHI_PCIE_VENDOR_ID, 0x0300)},
	{PCI_DEVICE(MHI_PCIE_VENDOR_ID, 0x0301)},
	{PCI_DEVICE(MHI_PCIE_VENDOR_ID, 0x0302)},
	{PCI_DEVICE(MHI_PCIE_VENDOR_ID, 0x0303)},
	{PCI_DEVICE(MHI_PCIE_VENDOR_ID, 0x0304)},
	{PCI_DEVICE(MHI_PCIE_VENDOR_ID, 0x0305)},
	{PCI_DEVICE(MHI_PCIE_VENDOR_ID, 0x0306)},
	{PCI_DEVICE(MHI_PCIE_VENDOR_ID, 0x0307)},
	{PCI_DEVICE(MHI_PCIE_VENDOR_ID, MHI_PCIE_DEBUG_ID)},
	{0},
};

static struct pci_driver mhi_pcie_driver = {
	.name = "mhi",
	.id_table = mhi_pcie_device_id,
	.probe = mhi_pci_probe,
	.driver = {
		.pm = &pm_ops
	}
};

module_pci_driver(mhi_pcie_driver);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("MHI_CORE");
MODULE_DESCRIPTION("MHI Host Driver");
