/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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


struct shmem_irq_config {
	unsigned long factory_addr; /* from gvm settings when provided */
	int irq; /* from gvm settings when provided */
};

/*
 * this is for platform does not provide probe features. the size should match
 * hab device side (all mmids)
 */
static struct shmem_irq_config pchan_factory_settings[] = {
	{0x1b000000, 7},
	{0x1b001000, 8},
	{0x1b002000, 9},
	{0x1b003000, 10},
	{0x1b004000, 11},
	{0x1b005000, 12},
	{0x1b006000, 13},
	{0x1b007000, 14},
	{0x1b008000, 15},
	{0x1b009000, 16},
	{0x1b00a000, 17},
	{0x1b00b000, 18},
	{0x1b00c000, 19},
	{0x1b00d000, 20},
	{0x1b00e000, 21},
	{0x1b00f000, 22},
	{0x1b010000, 23},
	{0x1b011000, 24},
	{0x1b012000, 25},
	{0x1b013000, 26},
	{0x1b014000, 27},
	{0x1b015000, 28},
};

static struct qvm_plugin_info {
	struct shmem_irq_config *pchan_settings;
	int setting_size;
	int curr;
	int probe_cnt;
} qvm_priv_info = {
	pchan_factory_settings,
	ARRAY_SIZE(pchan_factory_settings),
	0,
	ARRAY_SIZE(pchan_factory_settings)
};

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
			tasklet_schedule(&dev->task);
		}
	}
	return rc;
}

/*
 * this is only for guest
 */
static uint64_t get_guest_factory_paddr(struct qvm_channel *dev,
	unsigned long factory_addr, int irq, const char *name, uint32_t pages)
{
	int i;

	pr_debug("name = %s, factory paddr = 0x%lx, irq %d, pages %d\n",
		name, factory_addr, irq, pages);
	dev->guest_factory = (struct guest_shm_factory *)factory_addr;

	if (dev->guest_factory->signature != GUEST_SHM_SIGNATURE) {
		pr_err("signature error: %ld != %llu, factory addr %lx\n",
			GUEST_SHM_SIGNATURE, dev->guest_factory->signature,
			factory_addr);
		iounmap(dev->guest_factory);
		return 0;
	}

	dev->guest_intr = dev->guest_factory->vector;

	/*
	 * Set the name field on the factory page to identify the shared memory
	 * region
	 */
	for (i = 0; i < strlen(name) && i < GUEST_SHM_MAX_NAME - 1; i++)
		dev->guest_factory->name[i] = name[i];
	dev->guest_factory->name[i] = (char) 0;

	guest_shm_create(dev->guest_factory, pages);

	/* See if we successfully created/attached to the region. */
	if (dev->guest_factory->status != GSS_OK) {
		pr_err("create failed: %d\n", dev->guest_factory->status);
		iounmap(dev->guest_factory);
		return 0;
	}

	pr_debug("shm creation size %x, paddr=%llx, vector %d, dev %pK\n",
		dev->guest_factory->size,
		dev->guest_factory->shmem,
		dev->guest_intr,
		dev);

	dev->factory_addr = factory_addr;
	dev->irq = irq;

	return dev->guest_factory->shmem;
}

static int create_dispatcher(struct physical_channel *pchan)
{
	struct qvm_channel *dev = (struct qvm_channel *)pchan->hyp_data;
	int ret;

	tasklet_init(&dev->task, physical_channel_rx_dispatch,
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

void hab_pipe_reset(struct physical_channel *pchan)
{
	struct hab_pipe_endpoint *pipe_ep;
	struct qvm_channel *dev  = (struct qvm_channel *)pchan->hyp_data;

	pipe_ep = hab_pipe_init(dev->pipe, PIPE_SHMEM_SIZE,
				pchan->is_be ? 0 : 1);
	if (dev->pipe_ep != pipe_ep)
		pr_warn("The pipe endpoint must not change\n");
}

/*
 * allocate hypervisor plug-in specific resource for pchan, and call hab pchan
 * alloc common function. hab driver struct is directly accessed.
 * commdev: pointer to store the pchan address
 * id: index to hab_device (mmids)
 * is_be: pchan local endpoint role
 * name: pchan name
 * return: status 0: success, otherwise: failures
 */
int habhyp_commdev_alloc(void **commdev, int is_be, char *name,
		int vmid_remote, struct hab_device *mmid_device)
{
	struct qvm_channel *dev = NULL;
	struct qvm_plugin_info *qvm_priv = hab_driver.hyp_priv;
	uint64_t paddr;
	struct physical_channel **pchan = (struct physical_channel **)commdev;
	int ret = 0, coid = 0, channel = 0;
	char *shmdata;
	uint32_t pipe_alloc_size =
		hab_pipe_calc_required_bytes(PIPE_SHMEM_SIZE);
	uint32_t pipe_alloc_pages =
		(pipe_alloc_size + PAGE_SIZE - 1) / PAGE_SIZE;
	int temp;
	int total_pages;
	struct page **pages;

	pr_debug("%s: pipe_alloc_size is %d\n", __func__, pipe_alloc_size);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		goto err;
	}

	spin_lock_init(&dev->io_lock);

	paddr = get_guest_factory_paddr(dev,
			qvm_priv->pchan_settings[qvm_priv->curr].factory_addr,
			qvm_priv->pchan_settings[qvm_priv->curr].irq,
			name,
			pipe_alloc_pages);
	qvm_priv->curr++;
	if (qvm_priv->curr > qvm_priv->probe_cnt) {
		pr_err("pchan guest factory setting %d overflow probed cnt %d\n",
					qvm_priv->curr, qvm_priv->probe_cnt);
		ret = -1;
		goto err;
	}

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

	dev->pipe = (struct hab_pipe *) shmdata;
	pr_debug("\"%s\": pipesize %d, addr 0x%pK, be %d\n", name,
				 pipe_alloc_size, dev->pipe, is_be);
	dev->pipe_ep = hab_pipe_init(dev->pipe, PIPE_SHMEM_SIZE,
		is_be ? 0 : 1);
	/* newly created pchan is added to mmid device list */
	*pchan = hab_pchan_alloc(mmid_device, vmid_remote);
	if (!(*pchan)) {
		ret = -ENOMEM;
		goto err;
	}

	(*pchan)->closed = 0;
	(*pchan)->hyp_data = (void *)dev;
	strlcpy((*pchan)->name, name, MAX_VMID_NAME_SIZE);
	(*pchan)->is_be = is_be;

	dev->channel = channel;
	dev->coid = coid;

	ret = create_dispatcher(*pchan);
	if (ret < 0)
		goto err;

	return ret;

err:
	pr_err("%s failed\n", __func__);

	kfree(dev);

	if (*pchan)
		hab_pchan_put(*pchan);
	return ret;
}

int habhyp_commdev_dealloc(void *commdev)
{
	struct physical_channel *pchan = (struct physical_channel *)commdev;
	struct qvm_channel *dev = pchan->hyp_data;

	dev->guest_ctrl->detach = 0;

	if (get_refcnt(pchan->refcount) > 1) {
		pr_warn("potential leak pchan %s vchans %d refcnt %d\n",
				pchan->name, pchan->vcnt,
				get_refcnt(pchan->refcount));
	}

	kfree(dev);
	hab_pchan_put(pchan);
	return 0;
}

int hab_hypervisor_register(void)
{
	int ret = 0;

	hab_driver.b_server_dom = 0;

	pr_info("initializing for %s VM\n", hab_driver.b_server_dom ?
		"host" : "guest");

	hab_driver.hyp_priv = &qvm_priv_info;

	return ret;
}

void hab_hypervisor_unregister(void)
{
	pr_info("unregistration is called, but do nothing\n");
}

/* this happens before hypervisor register */
static int hab_shmem_probe(struct platform_device *pdev)
{
	int irq = 0;
	struct resource *mem;
	void *shmem_base = NULL;
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
