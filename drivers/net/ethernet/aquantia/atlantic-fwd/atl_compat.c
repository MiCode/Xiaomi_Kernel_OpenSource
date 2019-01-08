/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2017 aQuantia Corporation. All rights reserved
 *
 * Portions Copyright (C) various contributors (see specific commit references)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include "atl_common.h"
#include "atl_ring.h"
#include <linux/msi.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>

#ifdef ATL_COMPAT_PCI_IRQ_VECTOR
/* From commit aff171641d181ea573380efc3f559c9de4741fc5 */
int atl_compat_pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
	if (dev->msix_enabled) {
		struct msi_desc *entry;
		int i = 0;

		for_each_pci_msi_entry(entry, dev) {
			if (i == nr)
				return entry->irq;
			i++;
		}
		WARN_ON_ONCE(1);
		return -EINVAL;
	}

	if (dev->msi_enabled) {
		struct msi_desc *entry = first_pci_msi_entry(dev);

		if (WARN_ON_ONCE(nr >= entry->nvec_used))
			return -EINVAL;
	} else {
		if (WARN_ON_ONCE(nr > 0))
			return -EINVAL;
	}

	return dev->irq + nr;
}

#endif

#ifdef ATL_COMPAT_PCI_ALLOC_IRQ_VECTORS_AFFINITY

void atl_compat_set_affinity(int vector, struct atl_queue_vec *qvec)
{
	cpumask_t *cpumask = qvec ? &qvec->affinity_hint : NULL;

	irq_set_affinity_hint(vector, cpumask);
}

void atl_compat_calc_affinities(struct atl_nic *nic)
{
	struct pci_dev *pdev = nic->hw.pdev;
	int i;
	unsigned int cpu;

	get_online_cpus();
	cpu = cpumask_first(cpu_online_mask);

	for (i = 0; i < nic->nvecs; i++) {
		cpumask_t *cpumask = &nic->qvecs[i].affinity_hint;
		int vector;

		/* If some cpus went offline since allocating
		 * vectors, leave the remaining vectors' affininty
		 * unset.
		 */
		if (cpu >= nr_cpumask_bits)
			break;

		cpumask_clear(cpumask);
		cpumask_set_cpu(cpu, cpumask);
		cpu = cpumask_next(cpu, cpu_online_mask);
		vector = pci_irq_vector(pdev, i + ATL_NUM_NON_RING_IRQS);
	}
	put_online_cpus();
}

/* from commit 6f9a22bc5775d231ab8fbe2c2f3c88e45e3e7c28 */
static int irq_calc_affinity_vectors(int minvec, int maxvec,
	const struct irq_affinity *affd)
{
	int resv = affd->pre_vectors + affd->post_vectors;
	int vecs = maxvec - resv;
	int cpus;

	if (resv > minvec)
		return 0;

	/* Stabilize the cpumasks */
	get_online_cpus();
	cpus = cpumask_weight(cpu_online_mask);
	put_online_cpus();

	return min(cpus, vecs) + resv;
}

/* based on commit 402723ad5c625ee052432698ae5e56b02d38d4ec */
int atl_compat_pci_alloc_irq_vectors_affinity(struct pci_dev *dev,
	unsigned int min_vecs, unsigned int max_vecs, unsigned int flags,
	const struct irq_affinity *affd)
{
	static const struct irq_affinity msi_default_affd;
	int vecs = -ENOSPC;

	if (flags & PCI_IRQ_AFFINITY) {
		if (!affd)
			affd = &msi_default_affd;
	} else {
		if (WARN_ON(affd))
			affd = NULL;
	}

	if (affd)
		max_vecs = irq_calc_affinity_vectors(min_vecs, max_vecs, affd);

	if (flags & PCI_IRQ_MSIX) {
		struct msix_entry *entries;
		int i;

		entries = kcalloc(max_vecs, sizeof(*entries), GFP_KERNEL);
		if (!entries)
			return -ENOMEM;

		for (i = 0; i < max_vecs; i++)
			entries[i].entry = i;

		vecs = pci_enable_msix_range(dev, entries, min_vecs, max_vecs);
		kfree(entries);
		if (vecs > 0)
			return vecs;
	}

	if (flags & PCI_IRQ_MSI) {
		vecs = pci_enable_msi_range(dev, min_vecs, max_vecs);
		if (vecs > 0)
			return vecs;
	}

	/* use legacy irq if allowed */
	if ((flags & PCI_IRQ_LEGACY) && min_vecs == 1) {
		pci_intx(dev, 1);
		return 1;
	}

	return vecs;
}

#endif
