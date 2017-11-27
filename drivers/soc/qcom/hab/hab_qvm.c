/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define DEFAULT_HAB_SHMEM_IRQ 7

#define SHMEM_PHYSICAL_ADDR 0x1c050000

static irqreturn_t shm_irq_handler(int irq, void *_pchan)
{
	irqreturn_t rc = IRQ_NONE;
	struct physical_channel *pchan = _pchan;
	struct qvm_channel *dev =
		(struct qvm_channel *) (pchan ? pchan->hyp_data : NULL);

	if (dev && dev->guest_ctrl) {
		int status = dev->guest_ctrl->status;

		if (status & dev->idx) {
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
		const char *name, uint32_t pages)
{
	int i;

	dev->guest_factory = ioremap(SHMEM_PHYSICAL_ADDR, PAGE_SIZE);

	if (!dev->guest_factory) {
		pr_err("Couldn't map guest_factory\n");
		return 0;
	}

	if (dev->guest_factory->signature != GUEST_SHM_SIGNATURE) {
		pr_err("shmem factory signature incorrect: %ld != %llu\n",
			GUEST_SHM_SIGNATURE, dev->guest_factory->signature);
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

	pr_debug("shm creation size %x\n", dev->guest_factory->size);

	return dev->guest_factory->shmem;
}

static int create_dispatcher(struct physical_channel *pchan)
{
	struct qvm_channel *dev = (struct qvm_channel *)pchan->hyp_data;
	int ret;

	tasklet_init(&dev->task, physical_channel_rx_dispatch,
		(unsigned long) pchan);

	ret = request_irq(hab_driver.irq, shm_irq_handler, IRQF_SHARED,
		pchan->name, pchan);

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
	struct physical_channel **pchan = (struct physical_channel **)commdev;
	int ret = 0, coid = 0, channel = 0;
	char *shmdata;
	uint32_t pipe_alloc_size =
		hab_pipe_calc_required_bytes(PIPE_SHMEM_SIZE);
	uint32_t pipe_alloc_pages =
		(pipe_alloc_size + PAGE_SIZE - 1) / PAGE_SIZE;
	uint64_t paddr;
	int temp;
	int total_pages;
	struct page **pages;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&dev->io_lock);

	paddr = get_guest_factory_paddr(dev,
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
	dev->idx = dev->guest_ctrl->idx;

	kfree(pages);

	dev->pipe = (struct hab_pipe *) shmdata;
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
	if (ret)
		goto err;

	return ret;

err:
	kfree(dev);

	if (*pchan)
		hab_pchan_put(*pchan);
	pr_err("habhyp_commdev_alloc failed: %d\n", ret);
	return ret;
}

int habhyp_commdev_dealloc(void *commdev)
{
	struct physical_channel *pchan = (struct physical_channel *)commdev;
	struct qvm_channel *dev = pchan->hyp_data;


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

	return ret;
}

void hab_hypervisor_unregister(void)
{
	int status, i;

	for (i = 0; i < hab_driver.ndevices; i++) {
		struct hab_device *dev = &hab_driver.devp[i];
		struct physical_channel *pchan;

		list_for_each_entry(pchan, &dev->pchannels, node) {
			status = habhyp_commdev_dealloc(pchan);
			if (status) {
				pr_err("failed to free pchan %pK, i %d, ret %d\n",
					pchan, i, status);
			}
		}
	}
}

static int hab_shmem_probe(struct platform_device *pdev)
{
	int irq = platform_get_irq(pdev, 0);

	if (irq > 0)
		hab_driver.irq = irq;
	else
		hab_driver.irq = DEFAULT_HAB_SHMEM_IRQ;

	return 0;
}

static int hab_shmem_remove(struct platform_device *pdev)
{
	return 0;
}

static void hab_shmem_shutdown(struct platform_device *pdev)
{
	int i;
	struct qvm_channel *dev;
	struct physical_channel *pchan;
	struct hab_device hab_dev;

	for (i = 0; i < hab_driver.ndevices; i++) {
		hab_dev = hab_driver.devp[i];
		pr_debug("detaching %s\n", hab_dev.name);
		list_for_each_entry(pchan, &hab_dev.pchannels, node) {
			dev = (struct qvm_channel *)pchan->hyp_data;
			dev->guest_ctrl->detach = 0;
		}
	}
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
	return platform_driver_register(&hab_shmem_driver);
}

static void __exit hab_shmem_exit(void)
{
	platform_driver_unregister(&hab_shmem_driver);
}

core_initcall(hab_shmem_init);
module_exit(hab_shmem_exit);

MODULE_DESCRIPTION("Hypervisor shared memory driver");
MODULE_LICENSE("GPL v2");
