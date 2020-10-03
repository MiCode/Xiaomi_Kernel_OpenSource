/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __OF_PCI_H
#define __OF_PCI_H

#include <linux/pci.h>
#include <linux/msi.h>

struct pci_dev;
struct of_phandle_args;
struct device_node;

#if IS_ENABLED(CONFIG_OF) && IS_ENABLED(CONFIG_PCI)
#ifdef CONFIG_PCI_QTI
struct device_node *of_pci_find_child_device(struct pci_dev *dev);
#else
struct device_node *of_pci_find_child_device(struct device_node *parent,
					     unsigned int devfn);
#endif
int of_pci_get_devfn(struct device_node *np);
void of_pci_check_probe_only(void);
#else
#ifdef CONFIG_PCI_QTI
static inline struct device_node *of_pci_find_child_device(struct pci_dev *dev)
{
	return NULL;
}
#else
static inline struct device_node *of_pci_find_child_device(struct device_node *parent,
					     unsigned int devfn)
{
	return NULL;
}
#endif

static inline int of_pci_get_devfn(struct device_node *np)
{
	return -EINVAL;
}

static inline void of_pci_check_probe_only(void) { }
#endif

#if IS_ENABLED(CONFIG_OF_IRQ)
int of_irq_parse_and_map_pci(const struct pci_dev *dev, u8 slot, u8 pin);
#else
static inline int
of_irq_parse_and_map_pci(const struct pci_dev *dev, u8 slot, u8 pin)
{
	return 0;
}
#endif

#endif
