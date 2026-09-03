// SPDX-License-Identifier: GPL-2.0
/*
 * PCIe endpoint controller driver for Unisoc SoCs
 *
 * Copyright (C) 2020 Unisoc Inc.
 * http://www.unisoc.com
 */

#include <linux/clk.h>
#include <linux/freezer.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/soc/sprd/hwfeature.h>
#include <linux/wait.h>

#include "pcie-sprd.h"

enum {
	SPRD_PCIE_REINIT_ACTION,
	SPRD_PCIE_SHUTDOWN_ACTION,
	SPRD_PCIE_WAKE_ACTION,
};

struct sprd_pcie_action {
	unsigned int action;
	struct list_head entry;
};

static DEFINE_SPINLOCK(pcie_action_lock);

/*
 * WORKAROUND: set CX_FLT_MASK_UR_POIS bit to fix pam ipa pcie access error
 * problem.
 */
static void sprd_pcie_setup_ep(struct dw_pcie_ep *ep)
{
	u32 val;
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	val = dw_pcie_readl_dbi(pci, PCIE_SYMBOL_TIMER_FILTER_1_OFF);
	val |= CX_FLT_MASK_UR_POIS;
	dw_pcie_writel_dbi(pci, PCIE_SYMBOL_TIMER_FILTER_1_OFF, val);
}

static void sprd_pcie_ep_init(struct dw_pcie_ep *ep)
{
	sprd_pcie_setup_ep(ep);
}

static int sprd_pcie_ep_raise_legacy_irq(struct dw_pcie_ep *ep, u8 irq_num)
{
	u16 cmd;
	int ret;
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
	struct device *dev = pci->dev;
	struct platform_device *pdev = to_platform_device(dev);

	cmd = dw_pcie_readw_dbi(pci, PCI_COMMAND);
	if (cmd & PCI_COMMAND_INTX_DISABLE) {
		dev_err(dev,
			"can't generate INTx interrupts, please enable it\n");
		return -EINVAL;
	}

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-inta-assert-syscons");
	if (ret < 0) {
		dev_err(dev, "please add inta assert property in dts\n");
		return ret;
	}
	/*
	 * TODO: software can't sure the delay value, maybe it doesn't need
	 * to delay after asserting INTA
	 */
	mdelay(1);
	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-inta-deassert-syscons");
	if (ret < 0) {
		dev_err(dev, "please add inta deassert property in dts\n");
		return ret;
	}

	return 0;
}

static int sprd_pcie_ep_raise_irq(struct dw_pcie_ep *ep,
				  u8 func_no, enum pci_epc_irq_type type,
				  u16 interrupt_num)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	switch (type) {
	case PCI_EPC_IRQ_LEGACY:
		return sprd_pcie_ep_raise_legacy_irq(ep, interrupt_num);
	case  PCI_EPC_IRQ_MSI:
		return dw_pcie_ep_raise_msi_irq(ep, func_no, interrupt_num);
	default:
		dev_err(pci->dev, "UNKNOWN IRQ type\n");
	}

	return 0;
}

static struct dw_pcie_ep_ops pcie_ep_ops = {
	.ep_init = sprd_pcie_ep_init,
	.raise_irq = sprd_pcie_ep_raise_irq,
};

#ifdef CONFIG_PCIE_SPRD_SPLIT_BAR
static int sprd_pcie_split_bar(struct dw_pcie *pci)
{
	u32 val, bar_offset;
	int i;

	for (i = 0; i < PCI_BAR_NUM; i++) {
		bar_offset = PCI_BASE_ADDRESS_0 + (i * 4);
		val = dw_pcie_readl_dbi(pci, bar_offset);
		if ((val & PCI_BASE_ADDRESS_MEM_TYPE_MASK) ==
		    PCI_BASE_ADDRESS_MEM_TYPE_64) {
			val &= ~PCI_BASE_ADDRESS_MEM_TYPE_MASK;
			val |= PCI_BASE_ADDRESS_MEM_TYPE_32;
			dw_pcie_dbi_ro_wr_en(pci);
			dw_pcie_writel_dbi(pci, bar_offset, val);
			dw_pcie_dbi_ro_wr_dis(pci);
			dw_pcie_writel_dbi2(pci, bar_offset + 4, PCI_BAR_EN);
		}
		val = dw_pcie_readl_dbi(pci, bar_offset);
		if ((val & PCI_BASE_ADDRESS_MEM_TYPE_MASK) ==
		    PCI_BASE_ADDRESS_MEM_TYPE_64){
			dev_err(pci->dev, "split bar fail\n");
			return -EINVAL;
		}
	}
	return 0;
}
#endif

/*
 * SPRD PCIe wake# pin related registers cannot be operated after PCIe
 * controller EB is disabled. So this func set the PCIe controller EB before
 * operate the wakeup pin.
 * Set the SPRD_SOFT_WAKE bit will pull down wake# pin in order to wakeup RC.
 */
static int sprd_pcie_wake_down(struct sprd_pcie *ep)
{
	u32 val;
	int ret;
	struct dw_pcie *pci = ep->pci;
	struct platform_device *pdev = to_platform_device(pci->dev);

	mutex_lock(&ep->sprd_pcie_mutex);
	if (ep->is_powered) {
		dev_warn(&pdev->dev, "pcie has been powered, do nothing\n");
		ret = -EPERM;
		goto out;
	}
	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-startup-syscons");
	if (ret < 0) {
		dev_err(&pdev->dev,
			"set pcie startup syscons fail, return %d\n", ret);
		goto out;
	}
	ep->is_powered = true;

	val = dw_pcie_readl_dbi(pci, SPRD_PCIE_RST_CTRL);
	val |= SPRD_SOFT_WAKE;
	dw_pcie_writel_dbi(pci, SPRD_PCIE_RST_CTRL, val);

	sprd_pcie_ltssm_enable(pci, true);

	mod_timer(&ep->timer, jiffies + msecs_to_jiffies(5000));
	ep->is_wakedown = true;
	ep->wake_down_cnt++;
	dev_info(&pdev->dev, "wake down count:%d\n", ep->wake_down_cnt);
out:
	mutex_unlock(&ep->sprd_pcie_mutex);
	return ret;
}

 /*
  * Wake# pin will be pulled up automatically after PCIe established link.
  * Whatever the hardware does, pull it down here.
  */
static int sprd_pcie_wake_up(struct sprd_pcie *ep)
{
	u32 val;
	struct dw_pcie *pci = ep->pci;

	dev_info(pci->dev, "wake up++\n");
	val = dw_pcie_readl_dbi(pci, SPRD_PCIE_RST_CTRL);
	val &= ~SPRD_SOFT_WAKE;
	dw_pcie_writel_dbi(pci, SPRD_PCIE_RST_CTRL, val);

	return 0;
}

static int sprd_pcie_ep_reinit(struct sprd_pcie *ep)
{
	struct dw_pcie *pci = ep->pci;
	struct platform_device *pdev = to_platform_device(pci->dev);
	int ret;
#ifdef CONFIG_PCIE_SPRD_SPLIT_BAR
	int val;
#endif

	dev_info(pci->dev, "reinit++\n");
	mutex_lock(&ep->sprd_pcie_mutex);

	/* check whether pcie has been powered on in the wake down action */
	if (!ep->is_powered) {
		ret = sprd_pcie_syscon_setting(pdev,
				"sprd,pcie-startup-syscons");
		if (ret < 0) {
			dev_err(&pdev->dev,
				"set pcie startup syscons fail, return %d\n",
				ret);
			goto out;
		}
		ep->is_powered = true;
	}

	/* wait 1ms until pcie establish link */
	usleep_range(1000, 1100);
	ret = sprd_pcie_check_vendor_id(pci);
	if (ret) {
		mutex_unlock(&ep->sprd_pcie_mutex);
		return ret;
	}
	sprd_pcie_setup_ep(&pci->ep);
	usleep_range(1000, 1100);
	dw_pcie_setup(pci);

#ifdef CONFIG_PCIE_SPRD_SPLIT_BAR
	if (sprd_kproperty_chipid("UD710-AB") == 0) {
		val = dw_pcie_readl_dbi(ep->pci,
					PCIE_SS_REG_BASE + PE0_GEN_CTRL_3);
		dw_pcie_writel_dbi(ep->pci, PCIE_SS_REG_BASE + PE0_GEN_CTRL_3,
				   (val & ~LTSSM_EN));

		ret = sprd_pcie_split_bar(ep->pci);
		if (ret) {
			dev_err(&pdev->dev, "failed to split bar\n");
			goto out;
		}

		val = dw_pcie_readl_dbi(ep->pci,
					PCIE_SS_REG_BASE + PE0_GEN_CTRL_3);
		dw_pcie_writel_dbi(ep->pci, PCIE_SS_REG_BASE + PE0_GEN_CTRL_3,
				   (val | LTSSM_EN));
	}
#endif

	sprd_pcie_ltssm_enable(pci, true);

	ret = dw_pcie_wait_for_link(pci);
	if (ret < 0)
		dev_err(pci->dev, "link fail after perst\n");
	else
		dw_pcie_ep_linkup(&pci->ep);

out:
	if (ep->is_wakedown) {
		del_timer(&ep->timer);
		ep->is_wakedown = false;
	}
	sprd_pcie_wake_up(ep);
	mutex_unlock(&ep->sprd_pcie_mutex);
	return ret;
}

static int sprd_pcie_ep_shutdown(struct sprd_pcie *ep)
{
	int ret;
	struct dw_pcie *pci = ep->pci;
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct device *dev = &pdev->dev;

	dev_info(dev, "shutdown ++\n");
	mutex_lock(&ep->sprd_pcie_mutex);
	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-shutdown-syscons");
	if (ret < 0) {
		dev_err(dev,
			"set pcie uninit syscons fail, return %d\n", ret);
	} else {
		ep->is_powered = false;
		//dw_pcie_ep_unlink(&pci->ep);
	}

	mutex_unlock(&ep->sprd_pcie_mutex);
	return ret;
}

static void sprd_pcie_do_actions(struct sprd_pcie *ep)
{
	struct sprd_pcie_action *pos, *tmp;

	list_for_each_entry_safe(pos, tmp, &ep->action_list, entry) {
		switch (pos->action) {
		case SPRD_PCIE_REINIT_ACTION:
			sprd_pcie_ep_reinit(ep);
			break;
		case SPRD_PCIE_SHUTDOWN_ACTION:
			sprd_pcie_ep_shutdown(ep);
			break;
		case SPRD_PCIE_WAKE_ACTION:
			sprd_pcie_wake_down(ep);
			break;
		default:
			break;
		}

		list_del(&pos->entry);
		kfree(pos);
	}
}

static int sprd_pcie_thread(void *data)
{
	struct sprd_pcie *ep = data;
	struct dw_pcie *pci = ep->pci;
	struct platform_device *pdev = to_platform_device(pci->dev);
	unsigned long flags;
	int ret;

	while (!kthread_should_stop()) {
		spin_lock_irqsave(&pcie_action_lock, flags);
		ret = wait_event_interruptible_lock_irq(ep->action_wait,
			!list_empty(&ep->action_list), pcie_action_lock);
		spin_unlock_irqrestore(&pcie_action_lock, flags);

		if (!ret)
			sprd_pcie_do_actions(ep);
		else
			dev_err(&pdev->dev,
				"pcie wait action return %d\n", ret);
	}

	return 0;
}

static int sprd_pcie_add_action(struct sprd_pcie *ep, unsigned int action)
{
	struct sprd_pcie_action *pos;
	unsigned long flags;

	dev_info(ep->pci->dev, "sprd pcie add action:%d\n", action);

	pos = kzalloc(sizeof(*pos), GFP_KERNEL);
	if (!pos)
		return -ENOMEM;

	pos->action = action;
	spin_lock_irqsave(&pcie_action_lock, flags);
	list_add_tail(&pos->entry, &ep->action_list);
	spin_unlock_irqrestore(&pcie_action_lock, flags);

	wake_up_interruptible_all(&ep->action_wait);

	return 0;
}

static void sprd_pcie_timeout(struct timer_list *timer)
{
	struct sprd_pcie *ep = container_of(timer, struct sprd_pcie, timer);

	dev_err(ep->pci->dev, "sprd pcie wait for link timeout\n");
	del_timer(timer);
	sprd_pcie_wake_up(ep);
	ep->is_wakedown = false;
}

/*
 * PERST# (high active)from root complex to wake up CP.
 *
 * When CP is in deep state, an root complex can wakeup CP by pulling the perst
 * signal.
 */
static irqreturn_t sprd_pcie_perst_irq(int irq, void *data)
{
	struct sprd_pcie *ep = data;
	struct dw_pcie *pci = ep->pci;

	pm_wakeup_dev_event(pci->dev, 1000, true);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t sprd_pcie_perst_thread_irq(int irq, void *data)
{
	struct sprd_pcie *ep = data;
	int val = gpiod_get_value(ep->gpiod_perst);
	struct dw_pcie *pci = ep->pci;

	if (val)
		sprd_pcie_add_action(ep, SPRD_PCIE_REINIT_ACTION);
	else
		sprd_pcie_add_action(ep, SPRD_PCIE_SHUTDOWN_ACTION);

	dev_info(pci->dev, "perst val: %d, is_wakedown:%d, is_powerd:%d\n",
		 val, ep->is_wakedown, ep->is_powered);

	return IRQ_HANDLED;
}

static int sprd_add_pcie_ep(struct sprd_pcie *sprd_pcie,
			    struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci = sprd_pcie->pci;
	struct dw_pcie_ep *ep = &pci->ep;

	ep->ops = &pcie_ep_ops;

	ret = dw_pcie_ep_init(ep);
	if (ret) {
		dev_err(dev, "failed to initialize endpoint\n");
		return ret;
	}

	sprd_pcie_ltssm_enable(pci, true);

	ret = sprd_pcie_check_vendor_id(pci);
	if (ret)
		return ret;

	return 0;
}

static int sprd_pcie_establish_link(struct dw_pcie *pci)
{
	struct sprd_pcie *ep = dev_get_drvdata(pci->dev);

	dev_info(pci->dev, "%s: ep is powered? %s\n",
		  __func__, ep->is_powered ? "yes, return now" : "no");
	/* if ep is already powered on,  unallowed operation */
	if (ep->is_powered)
		return -EPERM;

	dev_info(pci->dev, "add action: wake\n");
	sprd_pcie_add_action(ep, SPRD_PCIE_WAKE_ACTION);

	return 0;
}

static void sprd_pcie_stop_link(struct dw_pcie *pci)
{
	/* TODO */
}

static const struct dw_pcie_ops dw_pcie_ops = {
	.start_link = sprd_pcie_establish_link,
	.stop_link = sprd_pcie_stop_link,
};

static int sprd_pcie_ep_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct sprd_pcie *ep;
	int ret;

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-startup-syscons");
	if (ret < 0) {
		dev_err(dev, "get pcie syscons fail, return %d\n", ret);
		return ret;
	}

	ep = devm_kzalloc(dev, sizeof(*ep), GFP_KERNEL);
	if (!ep)
		return -ENOMEM;

	ep->is_powered = true;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	ep->pci = pci;

	platform_set_drvdata(pdev, ep);

	ep->gpiod_perst = devm_gpiod_get(dev, "pcie-perst", GPIOD_IN);
	if (IS_ERR(ep->gpiod_perst)) {
		dev_err(dev, "Please set pcie-perst gpio in DTS\n");
		return PTR_ERR(ep->gpiod_perst);
	}

	ep->perst_irq = gpiod_to_irq(ep->gpiod_perst);
	if (ep->perst_irq < 0) {
		dev_err(dev, "cannot get perst irq\n");
		return ep->perst_irq;
	}

	ret = devm_request_threaded_irq(dev, ep->perst_irq,
					sprd_pcie_perst_irq,
					sprd_pcie_perst_thread_irq,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND,
					"pcie_perst", ep);
	if (ret < 0) {
		dev_err(dev, "cannot request perst irq\n");
		return ret;
	}
	enable_irq_wake(ep->perst_irq);

	ret = sprd_add_pcie_ep(ep, pdev);
	if (ret) {
		dev_err(dev, "cannot initialize ep host\n");
		return ret;
	}

	device_init_wakeup(dev, true);

	init_waitqueue_head(&ep->action_wait);
	INIT_LIST_HEAD(&ep->action_list);
	mutex_init(&ep->sprd_pcie_mutex);

	timer_setup(&ep->timer, sprd_pcie_timeout, 0);
	ep->action_thread = kthread_run(sprd_pcie_thread, ep,
					"%s", "sprd_pcie_ep");
	if (IS_ERR(ep->action_thread)) {
		dev_err(dev, "Failed to create kthread\n");
		return PTR_ERR(ep->action_thread);
	}
	wake_up_process(ep->action_thread);

	return 0;
}

static int sprd_pcie_ep_remove(struct platform_device *pdev)
{
	struct sprd_pcie *ep = platform_get_drvdata(pdev);
	struct sprd_pcie_action *pos, *tmp;

	if (!IS_ERR(ep->action_thread))
		kthread_stop(ep->action_thread);

	list_for_each_entry_safe(pos, tmp, &ep->action_list, entry) {
		list_del(&pos->entry);
		kfree(pos);
	}

	device_init_wakeup(&pdev->dev, false);

	return 0;
}

static const struct of_device_id sprd_pcie_ep_of_match[] = {
	{
		.compatible = "sprd,pcie-ep",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pcie_ep_of_match);

static struct platform_driver sprd_pcie_ep_driver = {
	.probe = sprd_pcie_ep_probe,
	.remove = sprd_pcie_ep_remove,
	.driver = {
		.name = "sprd-pcie-ep",
		.suppress_bind_attrs = true,
		.of_match_table = sprd_pcie_ep_of_match,
	},
};

module_platform_driver(sprd_pcie_ep_driver);

MODULE_DESCRIPTION("Unisoc pcie ep controller driver");
MODULE_LICENSE("GPL");
