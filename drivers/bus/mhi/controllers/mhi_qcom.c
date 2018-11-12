// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2018, The Linux Foundation. All rights reserved.*/

#include <asm/arch_timer.h>
#include <linux/debugfs.h>
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
	{.dev_id = 0x306, .fw_image = "sdx55m/sbl1.mbn"},
	{.dev_id = 0x305, .fw_image = "sdx50m/sbl1.mbn"},
	{.dev_id = 0x304, .fw_image = "sbl.mbn", .edl_image = "edl.mbn"},
	/* default, set to debug.mbn */
	{.fw_image = "debug.mbn"},
};

static int debug_mode;
module_param_named(debug_mode, debug_mode, int, 0644);

void mhi_deinit_pci_dev(struct mhi_controller *mhi_cntrl)
{
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);
	struct pci_dev *pci_dev = mhi_dev->pci_dev;

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
	resource_size_t start, len;
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

	start = pci_resource_start(pci_dev, mhi_dev->resn);
	len = pci_resource_len(pci_dev, mhi_dev->resn);
	mhi_cntrl->regs = ioremap_nocache(start, len);
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
		goto exit_runtime_suspend;
	}

	ret = mhi_arch_link_off(mhi_cntrl, true);
	if (ret)
		MHI_ERR("Failed to Turn off link ret:%d\n", ret);

exit_runtime_suspend:
	mutex_unlock(&mhi_cntrl->pm_mutex);
	MHI_LOG("Exited with ret:%d\n", ret);

	return ret;
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
	ret = mhi_arch_link_on(mhi_cntrl);
	if (ret)
		goto rpm_resume_exit;

	/* enter M0 state */
	ret = mhi_pm_resume(mhi_cntrl);

rpm_resume_exit:
	mutex_unlock(&mhi_cntrl->pm_mutex);
	MHI_LOG("Exited with :%d\n", ret);

	return ret;
}

static int mhi_system_resume(struct device *dev)
{
	int ret = 0;
	struct mhi_controller *mhi_cntrl = dev_get_drvdata(dev);

	ret = mhi_runtime_resume(dev);
	if (ret) {
		MHI_ERR("Failed to resume link\n");
	} else {
		pm_runtime_set_active(dev);
		pm_runtime_enable(dev);
	}

	return ret;
}

int mhi_system_suspend(struct device *dev)
{
	struct mhi_controller *mhi_cntrl = dev_get_drvdata(dev);
	int ret;

	MHI_LOG("Entered\n");

	/* if rpm status still active then force suspend */
	if (!pm_runtime_status_suspended(dev)) {
		ret = mhi_runtime_suspend(dev);
		if (ret) {
			MHI_LOG("suspend failed ret:%d\n", ret);
			return ret;
		}
	}

	pm_runtime_set_suspended(dev);
	pm_runtime_disable(dev);

	MHI_LOG("Exit\n");
	return 0;
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

	if (reason == MHI_CB_IDLE) {
		MHI_LOG("Schedule runtime suspend 1\n");
		pm_runtime_mark_last_busy(dev);
		pm_request_autosuspend(dev);
	}
}

/* capture host SoC XO time in ticks */
static u64 mhi_time_get(struct mhi_controller *mhi_cntrl, void *priv)
{
	return arch_counter_get_cntvct();
}

int mhi_debugfs_trigger_m0(void *data, u64 val)
{
	struct mhi_controller *mhi_cntrl = data;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);

	MHI_LOG("Trigger M3 Exit\n");
	pm_runtime_get(&mhi_dev->pci_dev->dev);
	pm_runtime_put(&mhi_dev->pci_dev->dev);

	return 0;
}

int mhi_debugfs_trigger_m3(void *data, u64 val)
{
	struct mhi_controller *mhi_cntrl = data;
	struct mhi_dev *mhi_dev = mhi_controller_get_devdata(mhi_cntrl);

	MHI_LOG("Trigger M3 Entry\n");
	pm_runtime_mark_last_busy(&mhi_dev->pci_dev->dev);
	pm_request_autosuspend(&mhi_dev->pci_dev->dev);

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(debugfs_trigger_m0_fops, NULL,
			 mhi_debugfs_trigger_m0, "%llu\n");

DEFINE_DEBUGFS_ATTRIBUTE(debugfs_trigger_m3_fops, NULL,
			 mhi_debugfs_trigger_m3, "%llu\n");

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

	ret = mhi_async_power_up(mhi_cntrl);
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

	/* setup power management apis */
	mhi_cntrl->status_cb = mhi_status_cb;
	mhi_cntrl->runtime_get = mhi_runtime_get;
	mhi_cntrl->runtime_put = mhi_runtime_put;
	mhi_cntrl->link_status = mhi_link_status;

	mhi_cntrl->time_get = mhi_time_get;

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

	sysfs_create_group(&mhi_cntrl->mhi_dev->dev.kobj, &mhi_qcom_group);

	return mhi_cntrl;

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
		ret = mhi_async_power_up(mhi_cntrl);
		if (ret)
			goto error_power_up;
	}

	pm_runtime_mark_last_busy(&pci_dev->dev);
	pm_runtime_allow(&pci_dev->dev);

	if (mhi_cntrl->dentry) {
		debugfs_create_file_unsafe("m0", 0444, mhi_cntrl->dentry,
					   mhi_cntrl, &debugfs_trigger_m0_fops);
		debugfs_create_file_unsafe("m3", 0444, mhi_cntrl->dentry,
					   mhi_cntrl, &debugfs_trigger_m3_fops);
	}

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
