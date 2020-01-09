/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "hab.h"
#include "hab_qvm.h"

#include <linux/highmem.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <linux/of.h>
#include <linux/of_platform.h>

int hab_hypervisor_register_os(void)
{
	int ret = 0;

	hab_driver.b_server_dom = 0;

	return ret;
}

void habhyp_commdev_dealloc_os(void *commdev)
{
	struct physical_channel *pchan = (struct physical_channel *)commdev;
	struct qvm_channel *dev = pchan->hyp_data;

	dev->guest_ctrl->detach = 0;
}

static irqreturn_t shm_irq_handler(int irq, void *_pchan)
{
	irqreturn_t rc = IRQ_NONE;
	struct physical_channel *pchan = (struct physical_channel *) _pchan;
	struct qvm_channel *dev =
		(struct qvm_channel *) (pchan ? pchan->hyp_data : NULL);

	if (dev && dev->guest_ctrl) {
		int status = dev->guest_ctrl->status;

		if (status & 0xffff) {/*source bitmask indicator*/
			rc = IRQ_HANDLED;
			tasklet_hi_schedule(&dev->os_data->task);
		}
	}
	return rc;
}

int habhyp_commdev_create_dispatcher(struct physical_channel *pchan)
{
	struct qvm_channel *dev = (struct qvm_channel *)pchan->hyp_data;
	int ret;

	tasklet_init(&dev->os_data->task, physical_channel_rx_dispatch,
		(unsigned long) pchan);

	pr_debug("request_irq: irq = %d, pchan name = %s",
			dev->irq, pchan->name);
	ret = request_irq(dev->irq, shm_irq_handler, IRQF_SHARED |
			IRQF_NO_SUSPEND, pchan->name, pchan);
	if (ret)
		pr_err("request_irq for %s failed: %d\n",
			pchan->name, ret);

	return ret;
}

/* The input is already va now */
inline unsigned long hab_shmem_factory_va(unsigned long factory_addr)
{
	return factory_addr;
}

/* to get the shmem data region virtual address */
char *hab_shmem_attach(struct qvm_channel *dev, const char *name,
	uint32_t pipe_alloc_pages)
{
	struct qvm_plugin_info *qvm_priv = hab_driver.hyp_priv;
	uint64_t paddr;
	char *shmdata;
	int temp;
	int total_pages;
	struct page **pages;
	int ret = 0;

	/* no more vdev-shmem for more pchan considering the 1:1 rule */
	if (qvm_priv->curr >= qvm_priv->probe_cnt) {
		pr_err("pchan guest factory setting %d overflow probed cnt %d\n",
					qvm_priv->curr, qvm_priv->probe_cnt);
		ret = -1;
		goto err;
	}

	paddr = get_guest_ctrl_paddr(dev,
			qvm_priv->pchan_settings[qvm_priv->curr].factory_addr,
			qvm_priv->pchan_settings[qvm_priv->curr].irq,
			name,
			pipe_alloc_pages);

	total_pages = dev->guest_factory->size + 1;
	pages = kmalloc_array(total_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		goto err;
	}

	for (temp = 0; temp < total_pages; temp++)
		pages[temp] = pfn_to_page((paddr / PAGE_SIZE) + temp);

	dev->guest_ctrl = vmap(pages, total_pages, VM_MAP, PAGE_KERNEL);
	if (!dev->guest_ctrl) {
		ret = -ENOMEM;
		kfree(pages);
		goto err;
	}

	shmdata = (char *)dev->guest_ctrl + PAGE_SIZE;

	pr_debug("ctrl page 0x%llx mapped at 0x%pK, idx %d\n",
			paddr, dev->guest_ctrl, dev->guest_ctrl->idx);
	pr_debug("data buffer mapped at 0x%pK\n", shmdata);
	dev->idx = dev->guest_ctrl->idx;

	kfree(pages);

	qvm_priv->curr++;
	return shmdata;

err:
	return ERR_PTR(ret);
}

/* this happens before hypervisor register */
static int hab_shmem_probe(struct platform_device *pdev)
{
	int irq = 0;
	struct resource *mem;
	void __iomem *shmem_base = NULL;
	int ret = 0;

	/* hab in one GVM will not have pchans more than one VM could allowed */
	if (qvm_priv_info.probe_cnt >= hab_driver.ndevices) {
		pr_err("no more channel, current %d, maximum %d\n",
			qvm_priv_info.probe_cnt, hab_driver.ndevices);
		return -ENODEV;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		pr_err("no interrupt for the channel %d, error %d\n",
			qvm_priv_info.probe_cnt, irq);
		return irq;
	}
	qvm_priv_info.pchan_settings[qvm_priv_info.probe_cnt].irq = irq;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		pr_err("can not get io mem resource for channel %d\n",
					qvm_priv_info.probe_cnt);
		return -EINVAL;
	}
	shmem_base = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(shmem_base)) {
		pr_err("ioremap failed for channel %d, mem %pK\n",
					qvm_priv_info.probe_cnt, mem);
		return -EINVAL;
	}
	qvm_priv_info.pchan_settings[qvm_priv_info.probe_cnt].factory_addr
			= (unsigned long)((uintptr_t)shmem_base);

	pr_debug("pchan idx %d, hab irq=%d shmem_base=%pK, mem %pK\n",
			 qvm_priv_info.probe_cnt, irq, shmem_base, mem);

	qvm_priv_info.probe_cnt++;

	return ret;
}

static int hab_shmem_remove(struct platform_device *pdev)
{
	return 0;
}

static void hab_shmem_shutdown(struct platform_device *pdev)
{
}

static const struct of_device_id hab_shmem_match_table[] = {
	{.compatible = "qvm,guest_shm"},
	{},
};

static struct platform_driver hab_shmem_driver = {
	.probe = hab_shmem_probe,
	.remove = hab_shmem_remove,
	.shutdown = hab_shmem_shutdown,
	.driver = {
		.name = "hab_shmem",
		.of_match_table = of_match_ptr(hab_shmem_match_table),
	},
};

static int __init hab_shmem_init(void)
{
	qvm_priv_info.probe_cnt = 0;
	return platform_driver_register(&hab_shmem_driver);
}

static void __exit hab_shmem_exit(void)
{
	platform_driver_unregister(&hab_shmem_driver);
	qvm_priv_info.probe_cnt = 0;
}

core_initcall(hab_shmem_init);
module_exit(hab_shmem_exit);

MODULE_DESCRIPTION("Hypervisor shared memory driver");
MODULE_LICENSE("GPL v2");
