// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2018, The Linux Foundation. All rights reserved.*/

#include <linux/interrupt.h>
#include <linux/iommu.h>
#include <linux/ipc_logging.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/msi.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_pci.h>
#include <linux/pci.h>

struct msm_msi_irq {
	unsigned int hwirq; /* MSI controller hwirq */
	unsigned int virq; /* MSI controller virq */
};

struct msm_msi {
	struct list_head clients;
	struct device *dev;
	struct device_node *of_node;
	int nr_irqs;
	struct msm_msi_irq *irqs;
	unsigned long *bitmap; /* tracks used/unused MSI */
	struct mutex mutex; /* mutex for modifying MSI client list and bitmap */
	struct irq_domain *inner_domain; /* parent domain; gen irq related */
	struct irq_domain *msi_domain; /* child domain; pci related */
	phys_addr_t msi_addr;
};

/* structure for each client of MSI controller */
struct msm_msi_client {
	struct list_head node;
	struct msm_msi *msi;
	struct device *dev; /* client's dev of pci_dev */
	u32 nr_irqs; /* nr_irqs allocated for client */
	dma_addr_t msi_addr;
};

static void msm_msi_handler(struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct msm_msi *msi;
	unsigned int virq;

	chained_irq_enter(chip, desc);

	msi = irq_desc_get_handler_data(desc);
	virq = irq_find_mapping(msi->inner_domain, irq_desc_get_irq(desc));

	generic_handle_irq(virq);

	chained_irq_exit(chip, desc);
}

static struct irq_chip msm_msi_irq_chip = {
	.name = "msm_pci_msi",
	.irq_mask = pci_msi_mask_irq,
	.irq_unmask = pci_msi_unmask_irq,
};

static int msm_msi_domain_prepare(struct irq_domain *domain, struct device *dev,
				int nvec, msi_alloc_info_t *arg)
{
	struct msm_msi *msi = domain->parent->host_data;
	struct msm_msi_client *client;

	client = devm_kzalloc(msi->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->msi = msi;
	client->dev = dev;
	client->msi_addr = dma_map_resource(client->dev, msi->msi_addr,
					PAGE_SIZE, DMA_FROM_DEVICE, 0);
	if (dma_mapping_error(client->dev, client->msi_addr)) {
		dev_err(msi->dev, "MSI: failed to map msi address\n");
		client->msi_addr = 0;
		return -ENOMEM;
	}

	mutex_lock(&msi->mutex);
	list_add_tail(&client->node, &msi->clients);
	mutex_unlock(&msi->mutex);

	/* zero out struct for framework */
	memset(arg, 0, sizeof(*arg));

	return 0;
}

void msm_msi_domain_finish(msi_alloc_info_t *arg, int retval)
{
	struct device *dev = arg->desc->dev;
	struct irq_domain *domain = dev_get_msi_domain(dev);
	struct msm_msi *msi = domain->parent->host_data;

	/* if prepare or alloc fails, then clean up */
	if (retval) {
		struct msm_msi_client *tmp, *client = NULL;

		mutex_lock(&msi->mutex);
		list_for_each_entry(tmp, &msi->clients, node) {
			if (tmp->dev == dev) {
				client = tmp;
				list_del(&client->node);
				break;
			}
		}
		mutex_unlock(&msi->mutex);

		if (!client)
			return;

		if (client->msi_addr)
			dma_unmap_resource(client->dev, client->msi_addr,
					PAGE_SIZE, DMA_FROM_DEVICE, 0);

		devm_kfree(msi->dev, client);

		return;
	}
}

static struct msi_domain_ops msm_msi_domain_ops = {
	.msi_prepare = msm_msi_domain_prepare,
	.msi_finish = msm_msi_domain_finish,
};

static struct msi_domain_info msm_msi_domain_info = {
	.flags = MSI_FLAG_USE_DEF_DOM_OPS | MSI_FLAG_USE_DEF_CHIP_OPS |
		MSI_FLAG_MULTI_PCI_MSI | MSI_FLAG_PCI_MSIX,
	.ops = &msm_msi_domain_ops,
	.chip = &msm_msi_irq_chip,
};

static int msm_msi_irq_set_affinity(struct irq_data *data,
				      const struct cpumask *mask, bool force)
{
	struct irq_data *parent_data = irq_get_irq_data(irqd_to_hwirq(data));

	/* set affinity for MSM MSI HW IRQ */
	if (parent_data->chip->irq_set_affinity)
		return parent_data->chip->irq_set_affinity(parent_data,
				mask, force);

	return -EINVAL;
}

static void msm_msi_irq_compose_msi_msg(struct irq_data *data,
					  struct msi_msg *msg)
{
	struct msm_msi_client *client = irq_data_get_irq_chip_data(data);
	struct irq_data *parent_data = irq_get_irq_data(irqd_to_hwirq(data));

	msg->address_lo = lower_32_bits(client->msi_addr);
	msg->address_hi = upper_32_bits(client->msi_addr);

	/* GIC HW IRQ */
	msg->data = irqd_to_hwirq(parent_data);
}

static struct irq_chip msm_msi_bottom_irq_chip = {
	.name = "msm_msi",
	.irq_set_affinity = msm_msi_irq_set_affinity,
	.irq_compose_msi_msg = msm_msi_irq_compose_msi_msg,
};

static int msm_msi_irq_domain_alloc(struct irq_domain *domain,
				      unsigned int virq, unsigned int nr_irqs,
				      void *args)
{
	struct msm_msi *msi = domain->host_data;
	struct msm_msi_client *tmp, *client = NULL;
	struct device *dev = ((msi_alloc_info_t *)args)->desc->dev;
	int i, ret = 0;
	int pos;

	mutex_lock(&msi->mutex);
	list_for_each_entry(tmp, &msi->clients, node) {
		if (tmp->dev == dev) {
			client = tmp;
			break;
		}
	}

	if (!client) {
		dev_err(msi->dev, "MSI: failed to find client dev\n");
		ret = -ENODEV;
		goto out;
	}

	pos = bitmap_find_next_zero_area(msi->bitmap, msi->nr_irqs, 0,
					nr_irqs, 0);
	if (pos < msi->nr_irqs) {
		bitmap_set(msi->bitmap, pos, nr_irqs);
	} else {
		ret = -ENOSPC;
		goto out;
	}

	for (i = 0; i < nr_irqs; i++) {
		msi->irqs[pos].virq = virq + i;
		irq_domain_set_info(domain, msi->irqs[pos].virq,
				msi->irqs[pos].hwirq,
				&msm_msi_bottom_irq_chip, client,
				handle_simple_irq, NULL, NULL);
		client->nr_irqs++;
		pos++;
	}

out:
	mutex_unlock(&msi->mutex);
	return ret;
}

static void msm_msi_irq_domain_free(struct irq_domain *domain,
				      unsigned int virq, unsigned int nr_irqs)
{
	struct irq_data *data = irq_domain_get_irq_data(domain, virq);
	struct msm_msi_client *client = irq_data_get_irq_chip_data(data);
	struct msm_msi *msi = client->msi;
	int i;

	mutex_lock(&msi->mutex);
	for (i = 0; i < nr_irqs; i++)
		if (msi->irqs[i].virq == virq)
			break;

	bitmap_clear(msi->bitmap, i, nr_irqs);
	client->nr_irqs -= nr_irqs;

	if (!client->nr_irqs) {
		dma_unmap_resource(client->dev, client->msi_addr, PAGE_SIZE,
				DMA_FROM_DEVICE, 0);
		list_del(&client->node);
		devm_kfree(msi->dev, client);
	}

	mutex_unlock(&msi->mutex);

	irq_domain_free_irqs_parent(domain, virq, nr_irqs);
}

static const struct irq_domain_ops msi_domain_ops = {
	.alloc = msm_msi_irq_domain_alloc,
	.free = msm_msi_irq_domain_free,
};

static int msm_msi_alloc_domains(struct msm_msi *msi)
{
	msi->inner_domain = irq_domain_add_linear(NULL, msi->nr_irqs,
						  &msi_domain_ops, msi);
	if (!msi->inner_domain) {
		dev_err(msi->dev, "MSI: failed to create IRQ domain\n");
		return -ENOMEM;
	}

	msi->msi_domain = pci_msi_create_irq_domain(
					of_node_to_fwnode(msi->of_node),
					&msm_msi_domain_info,
					msi->inner_domain);
	if (!msi->msi_domain) {
		dev_err(msi->dev, "MSI: failed to create MSI domain\n");
		irq_domain_remove(msi->inner_domain);
		return -ENOMEM;
	}

	return 0;
}

int msm_msi_init(struct device *dev)
{
	int i, ret;
	struct msm_msi *msi;
	struct device_node *of_node;
	const __be32 *prop_val;

	if (!dev->of_node) {
		dev_err(dev, "MSI: missing DT node\n");
		return -EINVAL;
	}

	of_node = of_parse_phandle(dev->of_node, "msi-parent", 0);
	if (!of_node) {
		dev_err(dev, "MSI: no phandle for MSI found\n");
		return -ENODEV;
	}

	if (!of_device_is_compatible(of_node, "qcom,pci-msi")) {
		dev_err(dev, "MSI: no compatible qcom,pci-msi found\n");
		return -ENODEV;
	}

	if (!of_find_property(of_node, "msi-controller", NULL))
		return -ENODEV;

	msi = devm_kzalloc(dev, sizeof(*msi), GFP_KERNEL);
	if (!msi)
		return -ENOMEM;

	msi->dev = dev;
	msi->of_node = of_node;
	mutex_init(&msi->mutex);
	INIT_LIST_HEAD(&msi->clients);

	prop_val = of_get_address(msi->of_node, 0, NULL, NULL);
	if (!prop_val) {
		dev_err(msi->dev, "MSI: missing 'reg' devicetree\n");
		return -EINVAL;
	}

	msi->msi_addr = be32_to_cpup(prop_val);
	if (!msi->msi_addr) {
		dev_err(msi->dev, "MSI: failed to get MSI address\n");
		return -EINVAL;
	}

	msi->nr_irqs = of_irq_count(msi->of_node);
	if (!msi->nr_irqs) {
		dev_err(msi->dev, "MSI: found no MSI interrupts\n");
		return -ENODEV;
	}

	msi->irqs = devm_kcalloc(msi->dev, msi->nr_irqs,
				sizeof(*msi->irqs), GFP_KERNEL);
	if (!msi->irqs)
		return -ENOMEM;

	msi->bitmap = devm_kcalloc(msi->dev, BITS_TO_LONGS(msi->nr_irqs),
				   sizeof(*msi->bitmap), GFP_KERNEL);
	if (!msi->bitmap)
		return -ENOMEM;

	ret = msm_msi_alloc_domains(msi);
	if (ret) {
		dev_err(msi->dev, "MSI: failed to allocate MSI domains\n");
		return ret;
	}

	for (i = 0; i < msi->nr_irqs; i++) {
		unsigned int irq = irq_of_parse_and_map(msi->of_node, i);

		if (!irq) {
			dev_err(msi->dev,
				"MSI: failed to parse/map interrupt\n");
			ret = -ENODEV;
			goto free_irqs;
		}

		msi->irqs[i].hwirq = irq;
		irq_set_chained_handler_and_data(msi->irqs[i].hwirq,
						msm_msi_handler, msi);
	}

	return 0;

free_irqs:
	for (--i; i >= 0; i--) {
		irq_set_chained_handler_and_data(msi->irqs[i].hwirq,
						NULL, NULL);
		irq_dispose_mapping(msi->irqs[i].hwirq);
	}

	irq_domain_remove(msi->msi_domain);
	irq_domain_remove(msi->inner_domain);

	return ret;
}
EXPORT_SYMBOL(msm_msi_init);
