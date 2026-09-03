// SPDX-License-Identifier: GPL-2.0
/*
 * PCIe host controller driver for Unisoc SoCs
 *
 * Copyright (C) 2020 Unisoc Inc.
 * http://www.unisoc.com
 */

#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/pcie-rc-sprd.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/regmap.h>

#include "pcie-designware.h"
#include "pcie-sprd.h"

#define REINIT_RETRIES  200
#define REINIT_WAIT_MIN  19000
#define REINIT_WAIT_MAX  20000

static void sprd_pcie_buserr_enable(struct dw_pcie *pci)
{
	u32 val;

	val = dw_pcie_readl_dbi(pci, PCIE_SS_REG_BASE + APB_CLKFREQ_TIMEOUT);
	val |= APB_CLKFREQ | BUSERR_EN;
	dw_pcie_writel_dbi(pci, PCIE_SS_REG_BASE + APB_CLKFREQ_TIMEOUT, val);
}

#ifdef CONFIG_SPRD_IPA_INTC
static void sprd_pcie_fix_interrupt_line(struct pci_dev *dev)
{
	struct pcie_port *pp = dev->bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	if (dev->hdr_type == PCI_HEADER_TYPE_NORMAL) {
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE,
				      ctrl->interrupt_line);
		dev_info(&dev->dev,
			 "The pci legacy interrupt pin is set to: %lu\n",
			 (unsigned long)ctrl->interrupt_line);
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_ANY_ID, PCI_ANY_ID, sprd_pcie_fix_interrupt_line);
#endif

static void sprd_pcie_assert_reset(struct pcie_port *pp)
{
	/* TODO */
}

static int sprd_pcie_host_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	int ret;

	sprd_pcie_assert_reset(pp);

	sprd_pcie_buserr_enable(pci);

	if (!device_property_read_bool(pci->dev, "ep-poweron-late")) {
		ret = sprd_pcie_check_vendor_id(pci);
		if (ret)
			return ret;
	}

	sprd_pcie_ltssm_enable(pci, true);

	sprd_pcie_check_atu_viewport(pci);

	if (dw_pcie_wait_for_link(pci))
		dev_warn(pci->dev,
			 "pcie ep may has not been powered yet, ignore it\n");

	return 0;
}

static const struct dw_pcie_host_ops sprd_pcie_host_ops = {
	.host_init = sprd_pcie_host_init,
};

int sprd_pcie_register_event(struct sprd_pcie_register_event *reg)
{
	struct sprd_pcie *ctrl = platform_get_drvdata(reg->pdev);

	if (!ctrl) {
		pr_err("sprd_pcie: cannot find RC for pci endpoint device.\n");
		return -ENODEV;
	}

	ctrl->event_reg = reg;
	pr_info("sprd_pcie: event is registered for RC\n");

	return 0;
}
EXPORT_SYMBOL(sprd_pcie_register_event);

int sprd_pcie_deregister_event(struct sprd_pcie_register_event *reg)
{
	struct sprd_pcie *ctrl = platform_get_drvdata(reg->pdev);

	if (!ctrl) {
		pr_err("sprd_pcie: cannot find RC for pci endpoint device.\n");
		return -ENODEV;
	}

	ctrl->event_reg = NULL;
	pr_info("sprd_pcie:event is deregistered for RC\n");

	return 0;
}
EXPORT_SYMBOL(sprd_pcie_deregister_event);

static void sprd_pcie_notify_client(struct sprd_pcie *ctrl,
					enum sprd_pcie_event event)
{
	struct dw_pcie *pci = ctrl->pci;

	if (ctrl->event_reg && ctrl->event_reg->callback &&
		(ctrl->event_reg->events & event))
		ctrl->event_reg->callback(event, ctrl->event_reg->data);
	else
		dev_err(pci->dev,
			"client of RC doesn't have registration for event %d\n",
			event);
}

/*
 * WAKE# (low active)from endpoint to wake up AP.
 *
 * When AP is in deep state, an endpoint can wakeup AP by pulling the wake
 * signal to low. After AP is activated, the endpoint must pull the wake signal
 * to high.
 */
static irqreturn_t sprd_pcie_wakeup_irq(int irq, void *data)
{
	struct sprd_pcie *ctrl = data;
	struct dw_pcie *pci = ctrl->pci;

	pm_wakeup_hard_event(pci->dev);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t sprd_pcie_wakeup_thread_irq(int irq, void *data)
{
	struct sprd_pcie *ctrl = data;
	struct dw_pcie *pci = ctrl->pci;
	int value = gpiod_get_value(ctrl->gpiod_wakeup);

	ctrl->wake_down_irq_cnt++;
	dev_info(pci->dev, "wake# value:%d, wake down count:%d, %s\n",
		 value, ctrl->wake_down_irq_cnt,
		 ctrl->is_powered ? "RC has been powered" : "");

	if (!ctrl->is_powered)
		sprd_pcie_notify_client(ctrl, SPRD_PCIE_EVENT_WAKEUP);

	return IRQ_HANDLED;
}

static int sprd_add_pcie_port(struct dw_pcie *pci, struct platform_device *pdev)
{
	struct sprd_pcie *ctrl;
	struct pcie_port *pp;
	struct device *dev = &pdev->dev;
	struct fwnode_handle *child;
	int ret;
	unsigned int irq;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	pci->dbi_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!pci->dbi_base)
		return -ENOMEM;

	pci->atu_base = pci->dbi_base + SPRD_PCIE_DBI_ATU_OFFSET;
	pp = &pci->pp;
	pp->ops = &sprd_pcie_host_ops;

	ctrl = platform_get_drvdata(to_platform_device(pci->dev));

	device_for_each_child_node(dev, child) {
		if (fwnode_property_read_string(child, "label",
						&ctrl->label)) {
			dev_err(dev, "without interrupt property\n");
			fwnode_handle_put(child);
			return -EINVAL;
		}
		if (!strcmp(ctrl->label, "msi_int")) {
			irq = irq_of_parse_and_map(to_of_node(child), 0);
			if (!irq) {
				dev_err(dev, "cannot get msi irq\n");
				return -EINVAL;
			}

			pp->msi_irq = (int)irq;
		}

#ifdef CONFIG_SPRD_PCIE_AER
		if (!strcmp(ctrl->label, "aer_int")) {
			irq = irq_of_parse_and_map(to_of_node(child), 0);
			if (!irq) {
				dev_err(dev, "cannot get aer irq\n");
				return -EINVAL;
			}

			ctrl->aer_irq = irq;
			dev_info(dev,
				 "sprd itself defines aer irq is %d\n", irq);
		}
#endif

#ifdef CONFIG_SPRD_IPA_INTC
		if (!strcmp(ctrl->label, "ipa_int")) {
			irq = irq_of_parse_and_map(to_of_node(child), 0);
			if (!irq) {
				dev_err(dev, "cannot get legacy irq\n");
				return -EINVAL;
			}
			ctrl->interrupt_line = irq;
		}
#endif
	}

	ctrl->gpiod_wakeup =
		devm_gpiod_get_index(dev, "pcie-wakeup", 0, GPIOD_IN);
	if (IS_ERR(ctrl->gpiod_wakeup)) {
		dev_warn(dev, "Please set pcie-wakeup gpio in DTS\n");
		goto no_wakeup;
	}

	ctrl->wakeup_irq = gpiod_to_irq(ctrl->gpiod_wakeup);
	if (ctrl->wakeup_irq < 0) {
		dev_warn(dev, "cannot get wakeup irq\n");
		goto no_wakeup;
	}

	snprintf(ctrl->wakeup_label, ctrl->label_len,
		 "%s wakeup", dev_name(dev));
	ret = devm_request_threaded_irq(dev, ctrl->wakeup_irq,
					sprd_pcie_wakeup_irq,
					sprd_pcie_wakeup_thread_irq,
					IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,
					ctrl->wakeup_label, ctrl);
	if (ret < 0)
		dev_warn(dev, "cannot request wakeup irq\n");

	enable_irq_wake(ctrl->wakeup_irq);
	device_init_wakeup(dev, true);

no_wakeup:
	if (device_property_read_bool(pci->dev, "ep-poweron-late")) {
		dev_warn(pci->dev,
			 "pcie ep may has not been powered yet, defer init\n");
		ctrl->defer_init = 1;
		return 0;
	}

	return dw_pcie_host_init(&pci->pp);
}

static const struct of_device_id sprd_pcie_of_match[] = {
	{
		.compatible = "sprd,pcie",
	},
	{},
};

static int sprd_pcie_host_uninit(struct platform_device *pdev)
{
	int ret;
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);
	struct dw_pcie *pci = ctrl->pci;

	sprd_pcie_clear_unhandled_msi(pci);
	sprd_pcie_save_dwc_reg(pci);

	if (ctrl->is_suspended) {
		ret = sprd_pcie_enter_pcipm_l2(pci);
		if (ret < 0)
			dev_info(&pdev->dev, "NOTE: RC can't enter l2\n");
	}

	sprd_pcie_ltssm_enable(pci, false);
	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-suspend-syscons");
	if (ret < 0)
		dev_warn(&pdev->dev,
			"set pcie uninit syscons fail, return %d\n", ret);

	ret = sprd_pcie_syscon_cache_setting(pdev, "sprd,pcie-syscons", true);
	if (ret < 0)
		dev_err(&pdev->dev,
			"set pcie syscons cache fail, return %d\n", ret);

	if (!ctrl->is_suspended) {
		ctrl->is_powered = 0;
		ret = pm_runtime_put_sync(&pdev->dev);
		if (ret < 0)
			dev_warn(&pdev->dev,
				 "pm runtime put fail: %d, usage_count:%d\n",
				 ret,
				 atomic_read(&pdev->dev.power.usage_count));
	}

	return ret;
}

static struct pci_bus *to_root_bus_from_pdev(struct platform_device *pdev)
{
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);
	struct dw_pcie *pci = ctrl->pci;
	struct pcie_port *pp = &pci->pp;

	return pp->bridge->bus;
}

static void sprd_pcie_rescan_bus(struct pci_bus *bus)
{
	struct pci_bus *child;

	pci_scan_child_bus(bus);

	pci_assign_unassigned_bus_resources(bus);

	list_for_each_entry(child, &bus->children, node)
		pcie_bus_configure_settings(child);

	pci_lock_rescan_remove();
	pci_bus_add_devices(bus);
	pci_unlock_rescan_remove();
}

static void sprd_pcie_remove_bus(struct pci_bus *bus)
{
	struct pci_dev *pci_dev;

	list_for_each_entry(pci_dev, &bus->devices, bus_list) {
		struct pci_bus *child_bus = pci_dev->subordinate;

		pci_lock_rescan_remove();
		pci_stop_and_remove_bus_device(pci_dev);
		pci_unlock_rescan_remove();

		if (child_bus) {
			dev_info(&bus->dev,
				"all pcie devices have been removed\n");
			return;
		}
	}
}

static int sprd_pcie_host_shutdown(struct platform_device *pdev)
{
	int ret;
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);
	struct dw_pcie *pci = ctrl->pci;
	struct pci_bus *root_bus;

	if (ctrl->defer_init)
		goto power_off;

	root_bus = to_root_bus_from_pdev(pdev);

	/*
	 * Before disabled pcie controller, it's better to remove pcie devices.
	 * pci_sysfs_init is called by late_initcall(fn). When it is called,
	 * pcie controller may be disabled and its EB is 0. In this case,
	 * it will cause kernel panic if a pcie device reads its owner
	 * configuration spaces.
	 */
	sprd_pcie_remove_bus(root_bus);
	sprd_pcie_save_dwc_reg(pci);
	sprd_pcie_ltssm_enable(pci, false);

power_off:
	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-shutdown-syscons");
	if (ret < 0)
		dev_err(&pdev->dev,
			"set pcie shutdown syscons fail, return %d\n", ret);

	ret = sprd_pcie_syscon_cache_setting(pdev, "sprd,pcie-syscons", true);
	if (ret < 0)
		dev_err(&pdev->dev,
			"set pcie syscons cache fail, return %d\n", ret);

	ret = pm_runtime_put_sync(&pdev->dev);
	if (ret < 0)
		dev_warn(&pdev->dev,
			"pm runtime put fail,ret:%d, usage_count:%d\n", ret,
			 atomic_read(&pdev->dev.power.usage_count));

	return ret;
}

static int sprd_pcie_link_up(struct dw_pcie *pci)
{
	u16 val;
	int ret;

	val = readw(pci->dbi_base + PCIE_PORT_DEBUG1);
	ret = (val & PCIE_PORT_DEBUG1_LINK_UP);

	val = readw(pci->dbi_base + SPRD_PCI_EXP_CAP  + PCI_EXP_LNKSTA);
	ret = (ret && (val & PCI_EXP_LNKSTA_DLLLA));

	if (!ret)
		dev_err(pci->dev, "link status don't ready!\n");

	return ret;
}

static const struct dw_pcie_ops dw_pcie_ops = {
	.link_up = sprd_pcie_link_up,
};

static int sprd_pcie_host_reinit(struct platform_device *pdev)
{
	int ret, err;
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);
	struct dw_pcie *pci = ctrl->pci;
	struct pcie_port *pp = &pci->pp;
	struct pci_bus *root_bus;

	if (!ctrl->is_suspended) {
		ret = pm_runtime_get_sync(&pdev->dev);
		if (ret) {
			dev_err(&pdev->dev,
				"ret = %d dd = %d is = %d rs = %d uc = %d\n",
				ret, pdev->dev.power.disable_depth,
				pdev->dev.power.is_suspended,
				pdev->dev.power.runtime_status,
				atomic_read(&pdev->dev.power.usage_count));
			goto err_get_sync;
		}
	}

	ret = sprd_pcie_syscon_cache_setting(pdev, "sprd,pcie-syscons", false);
	if (ret < 0)
		dev_err(&pdev->dev,
			"set pcie syscons cache fail, return %d\n", ret);

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-resume-syscons");
	if (ret < 0) {
		dev_err(&pdev->dev,
			"set pcie reinit syscons fail, return %d\n", ret);
		goto power_off;
	}
	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-aspml1p2-syscons");
	if (ret < 0)
		dev_err(&pdev->dev, "get pcie aspml1.2 syscons fail\n");

	ret = sprd_pcie_check_vendor_id(pci);
	if (ret)
		goto power_off;

	if (ctrl->defer_init) {
		ret = dw_pcie_host_init(pp);
		if (ret || (!dw_pcie_link_up(pci))) {
			root_bus = to_root_bus_from_pdev(pdev);
			sprd_pcie_remove_bus(root_bus);
			sprd_pcie_save_dwc_reg(pci);
			sprd_pcie_ltssm_enable(pci, false);
			ctrl->defer_init = 0;
			ret = -EAGAIN;

			goto power_off;
		}
		return 0;
	}

	sprd_pcie_buserr_enable(pci);
	dw_pcie_setup_rc(pp);

	sprd_pcie_ltssm_enable(pci, true);
	ret = dw_pcie_wait_for_link(pci);
	if (ret < 0) {
		dev_err(&pdev->dev, "reinit fail,command register[0x%x]:0x%x\n",
			PCI_COMMAND, dw_pcie_readl_dbi(pci, PCI_COMMAND));
		goto power_off;
	}

	sprd_pcie_restore_dwc_reg(pci);

	return 0;

power_off:
	sprd_pcie_syscon_setting(pdev, "sprd,pcie-suspend-syscons");

	err = sprd_pcie_syscon_cache_setting(pdev, "sprd,pcie-syscons", true);
	if (err < 0)
		dev_err(&pdev->dev,
			"set pcie syscons cache fail, return %d\n", err);
err_get_sync:
	if (!ctrl->is_suspended) {
		err = pm_runtime_put_sync(&pdev->dev);
		dev_err(&pdev->dev,
				"err = %d dd = %d is = %d rs = %d uc = %d\n",
				err, pdev->dev.power.disable_depth,
				pdev->dev.power.is_suspended,
				pdev->dev.power.runtime_status,
				atomic_read(&pdev->dev.power.usage_count));
	}
	return ret;
}

int sprd_pcie_configure_device(struct platform_device *pdev)
{
	int ret;
	int retries = 0;
	struct pci_bus *root_bus;
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s++\n", __func__);
	/*
	 * If sprd_pcie_probe() return before mallocing memory for struct
	 * sprd_pcie, no one can access any memory of this struct.
	 */
	if (!ctrl) {
		dev_err(&pdev->dev, "pcie RC host controller is NULL\n");
		return -EINVAL;
	}
	mutex_lock(&ctrl->sprd_pcie_mutex);
	/* In order to avoid someone rescanning PCIe once more */
	if (ctrl->is_powered) {
		dev_info(&pdev->dev, "PCIe has alreadly scanned\n");
		mutex_unlock(&ctrl->sprd_pcie_mutex);
		return 0;
	}

	ctrl->retries = 0;
	while (ctrl->reinit_disable && ctrl->retries++ < REINIT_RETRIES)
		usleep_range(REINIT_WAIT_MIN, REINIT_WAIT_MAX);
	if (ctrl->retries >= REINIT_RETRIES)
		dev_warn(&pdev->dev, "system in sleep, cannot reinit\n");

	for (retries = 0; retries < 3; retries++) {
		ret = sprd_pcie_host_reinit(pdev);
		if (!ret)
			break;
		usleep_range(150000, 200000);
		if (retries == 2) {
			dev_err(&pdev->dev, "pcie reinit failed three times\n");
			mutex_unlock(&ctrl->sprd_pcie_mutex);
			return ret;
		}
	}

	if (ctrl->defer_init)
		ctrl->defer_init = 0;
	else {
		root_bus = to_root_bus_from_pdev(pdev);
		sprd_pcie_rescan_bus(root_bus);
	}

	ctrl->is_powered = 1;
	mutex_unlock(&ctrl->sprd_pcie_mutex);

	return 0;
}
EXPORT_SYMBOL(sprd_pcie_configure_device);

int sprd_pcie_unconfigure_device(struct platform_device *pdev)
{
	int ret;
	struct pci_bus *root_bus;
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s++\n", __func__);
	mutex_lock(&ctrl->sprd_pcie_mutex);
	/* In order to avoid someone removing PCIe before establishing link */
	if (!ctrl->is_powered) {
		dev_err(&pdev->dev, "PCIe hasn't been scanned yet!\n");
		mutex_unlock(&ctrl->sprd_pcie_mutex);
		return -ENODEV;
	}

	root_bus = to_root_bus_from_pdev(pdev);
	sprd_pcie_remove_bus(root_bus);

	ret = sprd_pcie_host_uninit(pdev);
	if (ret < 0)
		dev_err(&pdev->dev,
			 "please ignore pcie unconfigure failure\n");
	mutex_unlock(&ctrl->sprd_pcie_mutex);

	return 0;
}
EXPORT_SYMBOL(sprd_pcie_unconfigure_device);

#ifdef CONFIG_PM_SLEEP
static int sprd_pcie_pm_notify(struct notifier_block *notify_block,
			unsigned long mode, void *unused)
{
	struct sprd_pcie *ctrl = container_of(
		notify_block, struct sprd_pcie, pm_notify);
	unsigned long flags;

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
	case PM_RESTORE_PREPARE:
		spin_lock_irqsave(&ctrl->lock, flags);
		ctrl->reinit_disable = 1;
		spin_unlock_irqrestore(&ctrl->lock, flags);
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		spin_lock_irqsave(&ctrl->lock, flags);
		ctrl->reinit_disable = 0;
		spin_unlock_irqrestore(&ctrl->lock, flags);
		break;
	default:
		break;
	}

	return 0;
}

void sprd_pcie_register_pm_notifier(struct sprd_pcie *ctrl)
{
	ctrl->pm_notify.notifier_call = sprd_pcie_pm_notify;
	register_pm_notifier(&ctrl->pm_notify);
}

void sprd_pcie_unregister_pm_notifier(struct sprd_pcie *ctrl)
{
	unregister_pm_notifier(&ctrl->pm_notify);
}
#endif

static int sprd_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct sprd_pcie *ctrl;
	int ret, err;
	size_t len = strlen(dev_name(dev)) + 10;

	if (device_property_read_bool(dev, "no-pcie")) {
		dev_info(dev, "no pcie device\n");
		sprd_pcie_syscon_setting(pdev, "sprd,pcie-shutdown-syscons");

		ret = sprd_pcie_syscon_cache_setting(pdev, "sprd,pcie-syscons", true);
		if (ret < 0)
			dev_err(&pdev->dev,
				"set pcie syscons cache fail, return %d\n", ret);
		return 0;
	}

	ctrl = devm_kzalloc(dev, sizeof(*ctrl) + len, GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	ctrl->pci = pci;
	ctrl->label_len = len;

	platform_set_drvdata(pdev, ctrl);

	pm_runtime_enable(dev);
	ret = pm_runtime_get_sync(dev);
	if (ret) {
		dev_err(dev,
			"pm_runtime_get_sync failed, ret:%d\n", ret);
		goto err_get_sync;
	}

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-startup-syscons");
	if (ret < 0) {
		dev_err(dev, "get pcie syscons fail, return %d\n", ret);
		goto power_off;
	}

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-aspml1p2-syscons");
	if (ret < 0)
		dev_warn(&pdev->dev, "get pcie aspml1.2 syscons fail\n");

	ret = sprd_add_pcie_port(pci, pdev);
	if (ret) {
		dev_err(dev, "cannot initialize rc host\n");
		goto power_off;
	}

	mutex_init(&ctrl->sprd_pcie_mutex);

	ctrl->is_powered = 1;

	if (!dw_pcie_link_up(pci)) {
		dev_info(dev,
			 "the EP has not been ready yet, power off the RC\n");
		sprd_pcie_host_shutdown(pdev);
		ctrl->is_powered = 0;
	}

	sprd_pcie_register_pm_notifier(ctrl);
	ctrl->reinit_disable = 0;

	return 0;

power_off:
	sprd_pcie_syscon_setting(pdev, "sprd,pcie-shutdown-syscons");

	err = sprd_pcie_syscon_cache_setting(pdev, "sprd,pcie-syscons", true);
	if (err < 0)
		dev_err(&pdev->dev,
			"set pcie syscons cache fail, return %d\n", err);
err_get_sync:
	err = pm_runtime_put_sync(&pdev->dev);
	if (err < 0)
		dev_warn(&pdev->dev,
			 "pm runtime put fail: %d, usage_count:%d\n",
			 err, atomic_read(&pdev->dev.power.usage_count));
	pm_runtime_disable(dev);

	return ret;
}

static int sprd_pcie_remove(struct platform_device *pdev)
{
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	sprd_pcie_unregister_pm_notifier(ctrl);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

/* While pcie gen3 is working, AP can't suspend */
static int sprd_pcie_pm_prepare(struct device *dev)
{
	struct sprd_pcie *ctrl;

	if (device_property_read_bool(dev, "no-pcie")) {
		dev_info(dev, "no pcie device, do nothing in pcie prepare\n");
		return 0;
	}

	ctrl = dev_get_drvdata(dev);

	if (ctrl->is_powered &&
	    device_property_read_bool(dev, "no-suspend")) {
		dev_info(dev, "can't suspend now, pcie is running\n");
		return -EBUSY;
	}

	return 0;
}

static int sprd_pcie_suspend_noirq(struct device *dev)
{
	int ret;
	struct platform_device *pdev;
	struct sprd_pcie *ctrl;

	if (device_property_read_bool(dev, "no-pcie")) {
		dev_info(dev, "no pcie device, do nothing in pcie suspend\n");
		return 0;
	}

	pdev = to_platform_device(dev);
	ctrl = platform_get_drvdata(pdev);

	if (!ctrl->is_powered)
		return 0;

	ctrl->is_suspended = 1;
	ret = sprd_pcie_host_uninit(pdev);
	if (ret < 0)
		dev_err(dev, "suspend noirq warning\n");

	return 0;
}

static int sprd_pcie_resume_noirq(struct device *dev)
{
	int ret;
	struct platform_device *pdev;
	struct sprd_pcie *ctrl;

	if (device_property_read_bool(dev, "no-pcie")) {
		dev_info(dev, "no pcie device, do nothing in pcie resume\n");
		return 0;
	}

	pdev = to_platform_device(dev);
	ctrl = platform_get_drvdata(pdev);

	if (!ctrl->is_powered)
		return 0;

	ret = sprd_pcie_host_reinit(pdev);
	if (ret < 0)
		dev_err(dev, "resume noirq warning\n");

	ctrl->is_suspended = 0;

	return 0;
}

static const struct dev_pm_ops sprd_pcie_pm_ops = {
	.prepare = sprd_pcie_pm_prepare,
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(sprd_pcie_suspend_noirq,
				      sprd_pcie_resume_noirq)
};

static struct platform_driver sprd_pcie_driver = {
	.probe = sprd_pcie_probe,
	.remove = sprd_pcie_remove,
	.driver = {
		.name = "sprd-pcie",
		.suppress_bind_attrs = true,
		.of_match_table = sprd_pcie_of_match,
		.pm	= &sprd_pcie_pm_ops,
	},
};

module_platform_driver(sprd_pcie_driver);

MODULE_DESCRIPTION("Unisoc PCIe host controller driver");
MODULE_LICENSE("GPL");
