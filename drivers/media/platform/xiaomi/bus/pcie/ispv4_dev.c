/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/aer.h>
#include <linux/prefetch.h>
#include <linux/msi.h>
#include <linux/mfd/core.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include "linux/mfd/ispv4_defs.h"
#include "ispv4_debugfs.h"
// #include "ispv4_pcie_iatu.h"
#include "ispv4_pcie_hdma.h"
#include "ispv4_boot.h"
#include <linux/component.h>
#include "ispv4_ctrl_ext.h"
#include "ispv4_notify.h"

#define DRIVER_NAME "xiaomi_ispv4"
#define ISPV4_DEBUGFS
#define ISPV4_AP_HDMA

static int ispv4_pcie_event(struct notifier_block *this, unsigned long action, void *data)
{
	(void)data;
	_pci_linkdown_event();
	pr_info("%s action:%d", __func__, action);
	return 0;
}

static struct notifier_block ispv4_pcie_notifier = {
	.notifier_call = ispv4_pcie_event,
};

extern struct platform_device *ispv4_ctrl_pdev;

static struct resource ispv4_rproc_res[RPROC_RES_NUM];
static struct resource ispv4_mailbox_res[MAILBOX_RES_NUM];
static struct resource ispv4_memdump_res[MEMDUMP_RES_NUM];
static struct resource ispv4_timealign_res[TIMEALIGN_RES_NUM];

static struct mfd_cell ispv4_mailbox_cell = {
	.name = "xm-ispv4-mbox",
	.num_resources = ARRAY_SIZE(ispv4_mailbox_res),
	.resources = ispv4_mailbox_res,
	.ignore_resource_conflicts = true,
};

static struct mfd_cell ispv4_rproc_pci_cell = {
	.name = "xm-ispv4-rproc-pci",
	.num_resources = ARRAY_SIZE(ispv4_rproc_res),
	.resources = ispv4_rproc_res,
	.ignore_resource_conflicts = true,
};

static struct mfd_cell ispv4_ionmap_cell = {
	.name = "ispv4-ionmap",
};

static struct mfd_cell ispv4_memdump_cell = {
	.name = "ispv4-memdump",
	.num_resources = ARRAY_SIZE(ispv4_memdump_res),
	.resources = ispv4_memdump_res,
	.ignore_resource_conflicts = true,
};

__maybe_unused
static struct mfd_cell ispv4_timealign_cell = {
	.name = "ispv4-time_align",
	.num_resources = ARRAY_SIZE(ispv4_timealign_res),
	.resources = ispv4_timealign_res,
	.ignore_resource_conflicts = true,
};

static pci_ers_result_t ispv4_io_error_detected(struct pci_dev *pdev,
						pci_channel_state_t state)
{
	if (state == pci_channel_io_perm_failure)
		return PCI_ERS_RESULT_DISCONNECT;

	pci_disable_device(pdev);

	/* Request a slot reset. */
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t ispv4_io_slot_reset(struct pci_dev *pdev)
{
	pci_ers_result_t result;
	int err;

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev,
			"Cannot re-enable PCI device after reset.\n");
		result = PCI_ERS_RESULT_DISCONNECT;
	} else {
		pdev->state_saved = true;
		pci_restore_state(pdev);
		pci_set_master(pdev);

		result = PCI_ERS_RESULT_RECOVERED;
	}

	return result;
}

static const struct pci_error_handlers ispv4_err_handler = {
	.error_detected = ispv4_io_error_detected,
	.slot_reset = ispv4_io_slot_reset,
};

static int ispv4_init_mailbox(struct ispv4_data *pdata)
{
	int ret = 0, idx = 0;
	struct pci_dev *pdev = pdata->pci;
	struct ispv4_ctrl_data *ctrl_priv;
	/* ispv4_ctrl_dev has been init on this time. */
	ctrl_priv = platform_get_drvdata(ispv4_ctrl_pdev);

	for (idx = 0; idx < ARRAY_SIZE(ispv4_mailbox_res); idx++) {
		switch (idx) {
		case MAILBOX_REG:
			ispv4_mailbox_res[idx].flags = IORESOURCE_MEM;
			ispv4_mailbox_res[idx].start =
				pci_resource_start(pdev, BAR_0) +
				MAILBOX_OFFSET;
			ispv4_mailbox_res[idx].end =
				pci_resource_start(pdev, BAR_0) +
				MAILBOX_OFFSET + MAILBOX_SIZE - 1;
			break;
		case MAILBOX_INTC:
			ispv4_mailbox_res[idx].flags = IORESOURCE_MEM;
			ispv4_mailbox_res[idx].start =
				pci_resource_start(pdev, BAR_0) + 0xD42C000;
			ispv4_mailbox_res[idx].end =
				pci_resource_end(pdev, BAR_0);
			break;
		case MAILBOX_IRQ:
			ispv4_mailbox_res[idx].flags = IORESOURCE_IRQ;
#ifdef FORCE_NOT_USE_HW_MSI
			ispv4_mailbox_res[idx].start =
				ctrl_priv->irq_info.gpio_irq[ISPV4_MBOX_IRQ];
			ispv4_mailbox_res[idx].end =
				ctrl_priv->irq_info.gpio_irq[ISPV4_MBOX_IRQ];
#else
			ispv4_mailbox_res[idx].start = pdev->irq;
			ispv4_mailbox_res[idx].end = pdev->irq;
#endif
			break;

		default:
			dev_err(&pdev->dev, "Invalid parameter!\n");
			break;
		}
	}

	ret = mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
			      &ispv4_mailbox_cell, 1, NULL, 0, NULL);
	if (ret) {
		dev_err(&pdev->dev, "MFD add mailbox devices failed: %d\n",
			ret);
		return ret;
	}
	dev_info(&pdev->dev, "ispv4 mailbox device registered.\n");

	return 0;
}

static int ispv4_init_memdump(struct ispv4_data *pdata)
{
	int ret = 0;
	struct pci_dev *pdev = pdata->pci;

	ispv4_memdump_res[ISPV4_DUMP_MEM].flags = IORESOURCE_MEM;
	ispv4_memdump_res[ISPV4_DUMP_MEM].start =
		pci_resource_start(pdev, BAR_4);
	ispv4_memdump_res[ISPV4_DUMP_MEM].end = pci_resource_end(pdev, BAR_4);

	ispv4_memdump_cell.platform_data = (void *)pdata;
	ret = mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
			      &ispv4_memdump_cell, 1, NULL, 0, NULL);
	if (ret) {
		dev_err(&pdev->dev, "MFD add memdump devices failed: %d\n",
			ret);
		return ret;
	}
	dev_info(&pdev->dev, "ispv4 memdump device registered.\n");

	return 0;
}

static int ispv4_init_timealign(struct ispv4_data *pdata)
{
	int ret = 0;
	struct pci_dev *pdev = pdata->pci;
	ispv4_timealign_res[ISPV4_TIMEALIGN_REG].flags = IORESOURCE_MEM;
	ispv4_timealign_res[ISPV4_TIMEALIGN_REG].start =
		pci_resource_start(pdev, BAR_0) + TIMER_OFFSET;
	ispv4_timealign_res[ISPV4_TIMEALIGN_REG].end =
		pci_resource_start(pdev, BAR_0) + TIMER_OFFSET + TIMER_SIZE;
	// ret = mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
	// 		      &ispv4_timealign_cell, 1, NULL, 0, NULL);

	spin_lock_init(&pdata->timer_lock);

	pdata->timer_reg_base = devm_ioremap(pdata->dev,ispv4_timealign_res[ISPV4_TIMEALIGN_REG].start,
				resource_size(&ispv4_timealign_res[ISPV4_TIMEALIGN_REG]));

	if (ret) {
		dev_err(&pdev->dev, "MFD add timealign devices failed:%d\n",
			ret);
		return ret;
	}
	dev_info(&pdev->dev, "ispv4 timealign device registered.\n");
	return 0;
}

__maybe_unused
static int write_isp_time_pci(struct ispv4_data *pdata, u32 addr, u32 data)
{
    writel_relaxed(data, pdata->timer_reg_base + addr);
    return 0;
}

__maybe_unused
static int read_isp_time_pci(struct ispv4_data *pdata, u32 addr, u32 *data)
{
    *data = readl_relaxed(pdata->timer_reg_base + addr);
    return 0;
}

static void set_isp_time(void *priv, u64 ap_time_ns)
{
    u64 ap_time_count,ap_time_s,ap_time_rest_ns;
    u32 high,low;
    unsigned long flags;
    struct ispv4_data *pdata = (struct ispv4_data *)priv;

    spin_lock_irqsave(&pdata->timer_lock,flags);
    ap_time_s = ap_time_ns / NSEC_PER_SEC_S;
    ap_time_rest_ns = ap_time_ns % NSEC_PER_SEC_S;
    ap_time_count = ap_time_s * ISPV4_TIMER64_FREQ + ((ap_time_rest_ns * ISPV4_TIMER64_FREQ << 7) / NSEC_PER_SEC_S >> 7);
    low  = 0xffffffff & ap_time_count;
    high = ap_time_count >> 32;
    write_isp_time_pci(pdata, TIMER64_LDCNT_LO, low);
    write_isp_time_pci(pdata, TIMER64_LDCNT_HI, high);
    write_isp_time_pci(pdata, TIMER64_LDCNT_EN,LDREG_TIM64_ENABLE);
    spin_unlock_irqrestore(&pdata->timer_lock,flags);

    pr_info("ispv4 timestamp_align:ap_time_s =%lu ,ap_time_rest_ns =%lu ,ap_time_count=%llu,ap_time_ns=%llu\n", ap_time_s,ap_time_rest_ns,ap_time_count,ap_time_ns);
}

static int ispv4_init_rproc(struct ispv4_data *priv)
{
	int ret = 0, idx = 0;
	struct pci_dev *pdev = priv->pci;
	struct ispv4_data **plat_data;

	for (idx = 0; idx < ARRAY_SIZE(ispv4_rproc_res); idx++) {
		switch (idx) {
		case RPROC_RAM:
			ispv4_rproc_res[idx].flags = IORESOURCE_MEM;
			ispv4_rproc_res[idx].start =
				pci_resource_start(pdev, BAR_0);
			ispv4_rproc_res[idx].end =
				pci_resource_start(pdev, BAR_0) + RPROC_SIZE -
				1;
			break;
		case RPROC_DDR:
			ispv4_rproc_res[idx].flags = IORESOURCE_MEM;
			ispv4_rproc_res[idx].start =
				pci_resource_start(pdev, BAR_2);
			ispv4_rproc_res[idx].end =
				pci_resource_end(pdev, BAR_2);
			break;
		case RPROC_ATTACH:
			ispv4_rproc_res[idx].flags = IORESOURCE_MEM;
			ispv4_rproc_res[idx].start =
				pci_resource_start(pdev, BAR_0) +
				RPROC_ATTACH_OFFSET;
			ispv4_rproc_res[idx].end =
				pci_resource_start(pdev, BAR_0) +
				RPROC_ATTACH_OFFSET + RPROC_ATTACH_SIZE - 1;
			break;
		case RPROC_IPC_IRQ:
			ispv4_rproc_res[idx].flags = IORESOURCE_IRQ;
			ispv4_rproc_res[idx].start = 0;
			ispv4_rproc_res[idx].end = 0;
			break;
		case RPROC_CRASH_IRQ:
			ispv4_rproc_res[idx].flags = IORESOURCE_IRQ;
			ispv4_rproc_res[idx].start = pdev->irq + 25;
			ispv4_rproc_res[idx].end = pdev->irq + 25;
			break;
		default:
			dev_err(&pdev->dev, "Invalid parameter!\n");
			break;
		}
	}

	plat_data = kmalloc(sizeof(struct ispv4_data *), GFP_KERNEL);
	*plat_data = priv;
	ispv4_rproc_pci_cell.platform_data = plat_data;
	ispv4_rproc_pci_cell.pdata_size = sizeof(struct ispv4_data *);
	ret = mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
			      &ispv4_rproc_pci_cell, 1, NULL, 0, NULL);
	kfree(plat_data);
	if (ret) {
		dev_err(&pdev->dev, "MFD add rproc devices failed: %d\n", ret);
		return ret;
	}
	dev_info(&pdev->dev, "ispv4 rproc device registered.\n");

	return 0;
}

static int ispv4_init_ionmap(struct ispv4_data *pdata)
{
	int ret = 0;
	struct pci_dev *pdev = pdata->pci;
	ret = mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
			      &ispv4_ionmap_cell, 1, NULL, 0, NULL);
	if (ret) {
		dev_err(&pdev->dev, "MFD add ionmap devices failed: %d\n", ret);
		return ret;
	}
	dev_info(&pdev->dev, "ispv4 ionmap device registered.\n");

	return 0;
}

static bool ispv4_alloc_irq_vectors(struct pci_dev *pdev,
				    struct ispv4_data *priv)
{
	struct device *dev = &pdev->dev;
	bool res = true;
	int irq = -1;

	switch (priv->pci_irq_type) {
	case IRQ_TYPE_LEGACY:
		irq = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_LEGACY);
		if (irq < 0)
			dev_err(dev, "Failed to get Legacy interrupt!\n");
		break;
	case IRQ_TYPE_MSI:
		irq = pci_alloc_irq_vectors(pdev, 1, 32, PCI_IRQ_MSI);
		if (irq < 0)
			dev_err(dev, "Failed to get MSI interrupts!\n");
		break;
	default:
		dev_err(dev, "Invalid IRQ type selected.\n");
		break;
	}

	if (irq < 0) {
		irq = 0;
		res = false;
	}

	return res;
}

static void ispv4_free_irq_vectors(struct pci_dev *pdev)
{
	pci_free_irq_vectors(pdev);
}

void ispv4_pci_event_cb(struct msm_pcie_notify *notify)
{
	struct pci_dev *pci_dev;
	struct ispv4_data *pci_priv;
	struct device *dev;

	if (!notify)
		return;

	pci_dev = notify->user;
	if (!pci_dev)
		return;

	pci_priv = pci_get_drvdata(pci_dev);
	if (!pci_priv)
		return;
	dev = &pci_priv->pci->dev;

	switch (notify->event) {
	case MSM_PCIE_EVENT_LINK_RECOVER:
		dev_warn(dev, "ispv4 pci link recover\n");
		break;
	case MSM_PCIE_EVENT_LINKDOWN:
		dev_err(dev, "ispv4 pci link down\n");
		//schedule_work(&pci_priv->linkdown_work);
		break;
	case MSM_PCIE_EVENT_WAKEUP:
		break;
	default:
		dev_err(dev, "ispv4 pci received invalid PCI event: %d\n",
			notify->event);
	}
}

static int ispv4_reg_pci_event(struct ispv4_data *pci_priv)
{
	int ret = 0;
	struct msm_pcie_register_event *pci_event;

	pci_event = &pci_priv->msm_pci_event;
	pci_event->events = MSM_PCIE_EVENT_LINKDOWN;
	// pci_event->events = MSM_PCIE_EVENT_LINK_RECOVER |
	// 		    MSM_PCIE_EVENT_LINKDOWN |
	// 		    MSM_PCIE_EVENT_WAKEUP;

	pci_event->user = pci_priv->pci;
	pci_event->mode = MSM_PCIE_TRIGGER_CALLBACK;
	pci_event->callback = ispv4_pci_event_cb;
	pci_event->options = MSM_PCIE_CONFIG_NO_RECOVERY;

	ret = msm_pcie_register_event(pci_event);
	return ret;
}

static void ispv4_pci_linkdown_workfn(struct work_struct *work)
{
	struct ispv4_data *priv;
	struct device *dev;
	priv = container_of(work, struct ispv4_data, linkdown_work);
	dev = &priv->pci->dev;
	// For FPGA test.
	mfd_remove_devices(dev);
	dev_warn(dev, "ispv4 pci remove all mfd.\n");
}

static int suspend_pci(void *priv)
{
	struct pci_dev *pcidev;
	int ret;

	if (priv == NULL)
		return -ENODEV;
	pcidev = priv;

	ret = ispv4_suspend_pci_link(pci_get_drvdata(pcidev));
	return ret;
}

static int suspend_pci_force(void *priv)
{
	struct pci_dev *pcidev;
	int ret;

	if (priv == NULL)
		return -ENODEV;
	pcidev = priv;

	ret = ispv4_suspend_pci_force(pci_get_drvdata(pcidev));
	return ret;
}

static int resume_pci(void *priv)
{
	struct pci_dev *pcidev;
	int ret;

	if (priv == NULL)
		return -ENODEV;
	pcidev = priv;

	ret = ispv4_resume_pci_link(pci_get_drvdata(pcidev));
	return ret;
}

static int ispv4_hdma_single_tran(void *priv, int dir, void *data, int len,
				  int ep_addr)
{
	struct pcie_hdma *hdma = ((struct ispv4_data *)priv)->pdma;
	return ispv4_hdma_single_trans(hdma, (enum pcie_hdma_dir)dir, data, len,
				       ep_addr);
}

static int ispv4_pcie_msi_register(void *priv, enum pcie_msi msi,
				   irq_handler_t thread_fn, const char *name,
				   void *data)
{
	struct pci_dev *pcidev;
	int ret;
	if (priv == NULL)
		return -ENODEV;
	if (msi < 0 || msi > MSI_MAX_NUM)
		return -EINVAL;
	pcidev = priv;
	ret = devm_request_threaded_irq(&pcidev->dev, pcidev->irq + msi, NULL,
					thread_fn,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT, name,
					data);
	if (ret)
		dev_err(&pcidev->dev, "%s msi[%d] request fail!\n", name, msi);
	return ret;
}

static void ispv4_pcie_msi_unregister(void *priv, enum pcie_msi msi, void *data)
{
	struct pci_dev *pcidev;
	if (priv == NULL)
		return;
	if (msi < 0 || msi > MSI_MAX_NUM)
		return;
	pcidev = priv;
	devm_free_irq(&pcidev->dev, pcidev->irq + msi, data);
}

static int ispv4_comp_bind(struct device *comp, struct device *master,
			   void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	struct ispv4_data *pdata;

	pdata = container_of(comp, typeof(*pdata), comp_dev);

	priv->v4l2_pci.pcidata = pdata;
	priv->v4l2_pci.pcidev = pdata->pci;
	priv->v4l2_pci.resume_pci = resume_pci;
	priv->v4l2_pci.suspend_pci = suspend_pci;
	priv->v4l2_pci.suspend_pci_force = suspend_pci_force;
	priv->v4l2_pci.hdma_trans = ispv4_hdma_single_tran;
	priv->v4l2_pci.pcie_msi_register = ispv4_pcie_msi_register;
	priv->v4l2_pci.pcie_msi_unregister = ispv4_pcie_msi_unregister;
	priv->v4l2_pci.linkup = true;
	priv->v4l2_pci.sof_registered = false;
	priv->v4l2_pci.eof_registered = false;
	priv->v4l2_pci.thermal_registered = false;
	priv->v4l2_pci.set_isp_time = set_isp_time;
	priv->v4l2_pci.avalid = true;
	dev_info(comp, "avalid!!\n");
	return 0;
}

static void ispv4_comp_unbind(struct device *comp, struct device *master,
			      void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	priv->v4l2_pci.avalid = false;
}

__maybe_unused static const struct component_ops comp_ops = {
	.bind = ispv4_comp_bind,
	.unbind = ispv4_comp_unbind
};

void _pci_enable_dbi_by_cs(struct pci_dev *pdev, bool s)
{
	u32 dbi, ndbi;

	if (s)
		pr_info("ispv4 pci enable dbi...\n");
	else
		pr_info("ispv4 pci disable dbi...\n");

	pci_read_config_dword(pdev, 0x8bc, &dbi);
	dev_info(&pdev->dev, "pci read dbi = 0x%x", dbi);
	ndbi = dbi;

	if (s)
		ndbi |= DBI_RO_WR_EN;
	else
		ndbi &= ~DBI_RO_WR_EN;

	if (ndbi != dbi) {
		dev_err(&pdev->dev, "pci write dbi = 0x%x", ndbi);
		pci_write_config_dword(pdev, 0x8bc, ndbi);
	}
}

void ispv4_pci_debugbar(void *addr)
{
	u32 val;
	static struct u32 *addr_pin = NULL;
	if (addr_pin == NULL && addr != NULL) {
		addr_pin = addr;
		pr_info("pin debugbar pin\n");
	}

	writel(0xdeadeeee, addr_pin);
	val = readl(addr_pin);
	pr_err("pin debugbar to dump val=%x\n", val);
}
EXPORT_SYMBOL_GPL(ispv4_pci_debugbar);
#if 0
void ispv4_pci_read_configsp(void *pdev)
{
	u32 val;
	static struct pci_dev *pdev_pin = NULL;
	if (pdev_pin == NULL && pdev != NULL) {
		pdev_pin = pdev;
		dev_info(&pdev_pin->dev, "pin pdev to dump\n");
	}

	pci_read_config_dword(pdev_pin, PCI_VENDOR_ID, &val);
	//dev_err(&pdev_pin->dev, "pin pdev to dump vid => 0x%08x\n", val);
	pci_read_config_dword(pdev_pin, PCI_DEVICE_ID, &val);
	//dev_err(&pdev_pin->dev, "pin pdev to dump did => 0x%08x\n", val);
	pci_read_config_dword(pdev_pin, PCI_COMMAND, &val);
	//dev_err(&pdev_pin->dev, "pin pdev to dump com => 0x%08x\n", val);
}
EXPORT_SYMBOL_GPL(ispv4_pci_read_configsp);
#endif

static int ispv4_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct ispv4_data *priv;
	int idx, ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct ispv4_data), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto out;
	}

	priv->dev = &pdev->dev;

	for (idx = PCI_STD_RESOURCES; idx < (PCI_STD_RESOURCE_END + 1); idx++) {
		ret = pci_assign_resource(pdev, idx);
		if (ret)
			dev_err(&pdev->dev, "assign pci resource failed!\n");

		if (pci_resource_flags(pdev, idx) & IORESOURCE_MEM_64)
			idx = idx + 1;
	}

	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "enable pci device failed: %d\n", ret);
		goto out;
	}

	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(&pdev->dev, "DMA configuration failed: 0x%x\n", ret);
		goto err_disable;
	}

#ifdef ISPV4_PCIE_BAR_SELECT
	/* set up pci connections */
	ret = pci_request_selected_regions(pdev, ((1 << 6) - 1), DRIVER_NAME);
	if (ret) {
		dev_err(&pdev->dev, "pci_request_selected_regions failed %d\n",
			ret);
		goto err_disable;
	}

	if (pci_resource_flags(pdev, BAR_5) & IORESOURCE_MEM) {
		priv->base = pci_ioremap_bar(pdev, BAR_5);

		if (!priv->base) {
			dev_err(&pdev->dev,
				"Failed to map the register of BAR5!\n");
			goto err_release_region;
		}
	}
#endif

	pci_set_master(pdev);
	pci_enable_pcie_error_reporting(pdev);

	pci_save_state(pdev);
	priv->default_state = pci_store_saved_state(pdev);

	priv->pci = pdev;
	priv->pci_irq_type = IRQ_TYPE_MSI;
	priv->pci_link_state = ISPV4_PCI_LINK_UP;
	pci_set_drvdata(pdev, priv);

	if (!ispv4_alloc_irq_vectors(pdev, priv))
		goto err_disable_irq;

#ifdef ISPV4_BAR_MAP
	ispv4_init_bar_resource(pdev, priv);
#endif

	priv->base_bar[3] = pci_ioremap_bar(pdev, BAR_5);
	priv->debug_bar = ioremap(pci_resource_start(pdev, BAR_0), PAGE_SIZE);
	_pci_config_iatu_fast(priv);
	ispv4_pci_debugbar(priv->debug_bar);

#ifdef ISPV4_AP_HDMA
	priv->pdma = ispv4_pcie_hdma_init(priv);
	if (IS_ERR_OR_NULL(priv->pdma)) {
		ret = PTR_ERR(priv->pdma);
		dev_err(&pdev->dev, "pcie hdma init failed: %d\n", ret);
		goto err_disable_irq;
	}
#endif

#ifdef ISPV4_DEBUGFS
	ispv4_debugfs_add_pcie();
	ispv4_debugfs_add_pcie_reset(priv);
	ispv4_debugfs_add_pcie_pm(priv);
	ispv4_debugfs_add_pcie_bandwidth(priv);
	// ret = ispv4_debugfs_add_pcie_dump_iatu_hdma(priv);
	// if (ret) {
	// 	dev_err(&pdev->dev, "create iatu and hdma regdump failed: %d\n", ret);
	// 	goto err_disable_irq;
	// }
#endif

	if ((id->driver_data & BOOT_MB_BY_PCI) != 0) {
		ret = ispv4_init_mailbox(priv);
		if (ret) {
			dev_err(&pdev->dev, "init mailbox mfd failed: %d\n",
				ret);
			goto err_disable_irq;
		}
	}

	if ((id->driver_data & BOOT_RP_BY_PCI) != 0) {
		ret = ispv4_init_rproc(priv);
		if (ret) {
			dev_err(&pdev->dev, "init rpmsg mfd failed: %d\n", ret);
			goto err_init_mfd;
		}
	}

	ret = ispv4_init_memdump(priv);
	if (ret) {
		dev_err(&pdev->dev, "init memdump mfd failed: %d\n", ret);
		goto err_init_mfd;
	}

	ret = ispv4_init_timealign(priv);
	if (ret) {
		dev_err(&pdev->dev, "init timealign mfd failed:%d\n", ret);
		goto err_init_mfd;
	}

	ret = ispv4_init_ionmap(priv);
	if (ret) {
		dev_err(&pdev->dev, "init ionmap mfd failed: %d\n", ret);
		goto err_init_mfd;
	}

	dev_info(&pdev->dev, "ispv4 all ispv4 mfd devices registered.\n");

	ret = ispv4_reg_pci_event(priv);
	if (ret) {
		dev_err(&pdev->dev, "register event failed: %d\n", ret);
		goto err_init_mfd;
	}

	ret = ispv4_register_notifier(&ispv4_pcie_notifier);
	if (ret != 0) {
		pr_err("ispv4_register_notifier failed.\n");
		goto err_comp;
	}

	INIT_WORK(&priv->linkdown_work, ispv4_pci_linkdown_workfn);
	dev_info(&pdev->dev, "ispv4 pci event registered %d\n", ret);

	device_initialize(&priv->comp_dev);
	dev_set_name(&priv->comp_dev, "ispv4_pci");
	pr_err("comp add %s! priv = %x, comp_name = %s\n", __FUNCTION__, priv,
	       dev_name(&priv->comp_dev));
	ret = component_add(&priv->comp_dev, &comp_ops);
	if (ret != 0) {
		pr_err("register ispv4_boot component failed.\n");
		goto err_unregister_notifer;
	}

	//ispv4_pci_read_configsp(pdev);
	priv->pci_link_state = ISPV4_PCI_LINK_UP;

	return 0;

err_unregister_notifer:
	ispv4_unregister_notifier(&ispv4_pcie_notifier);
err_comp:
	msm_pcie_deregister_event(&priv->msm_pci_event);
err_init_mfd:
	mfd_remove_devices(&pdev->dev);

err_disable_irq:
	// BOG ON THIS LINE
	// IRQ has action
	ispv4_free_irq_vectors(pdev);

#ifdef ISPV4_PCIE_BAR_SELECT
err_release_region:
	pci_release_selected_regions(pdev, ((1 << 6) - 1));
#endif

err_disable:
	pci_disable_device(pdev);

out:
	return ret;
}

static void ispv4_pci_remove(struct pci_dev *pdev)
{
	struct ispv4_data *priv = pci_get_drvdata(pdev);
	component_del(&priv->comp_dev, &comp_ops);

	ispv4_unregister_notifier(&ispv4_pcie_notifier);
	msm_pcie_deregister_event(&priv->msm_pci_event);

	if (priv->saved_state)
		pci_load_and_free_saved_state(pdev, &priv->saved_state);
	pci_load_and_free_saved_state(pdev, &priv->default_state);
	mfd_remove_devices(&pdev->dev);
	pci_disable_pcie_error_reporting(pdev);
	pci_release_selected_regions(pdev, ((1 << 1) - 1));
	pci_disable_device(pdev);
}

static void ispv4_shutdown(struct pci_dev *pdev)
{
	pci_disable_device(pdev);
}

static struct pci_device_id ispv4_pci_tbl[] = {
	{ PCI_DEVICE(ISPV4_PCI_VENDOR_ID, ISPV4_PCI_DEVICE_ID) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, ispv4_pci_tbl);

static struct pci_driver ispv4_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = ispv4_pci_tbl,
	.probe = ispv4_pci_probe,
	.remove = ispv4_pci_remove,
	.shutdown = ispv4_shutdown,
	.err_handler = &ispv4_err_handler,

};

#define BOOT_FAILED_MAGIC_NUM

extern int msm_pcie_enable_rc(u32 rc_idx);

int ispv4_pci_init(int boot_param)
{
	int ret = 0;

	msm_pcie_enable_rc(1);
	ispv4_boot_config_pci();

	/* enumerate it on PCIE */
	ret = msm_pcie_enumerate(1);
	if (ret < 0) {
		ispv4_pci_tbl->driver_data = 0xdead1234;
		return ret;
	}

	ispv4_pci_tbl->driver_data = boot_param;
	ret = pci_register_driver(&ispv4_pci_driver);

	return ret;
}

void ispv4_pci_exit(void)
{
	if (ispv4_pci_tbl->driver_data == 0xdead1234)
		return;

	pci_unregister_driver(&ispv4_pci_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform driver for Xiaomi, Inc. ZISP V4");
