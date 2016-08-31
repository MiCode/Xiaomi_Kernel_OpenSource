/*
 * mods_pci.c - This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2008-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"

#include <linux/io.h>

/************************
 * PCI ESCAPE FUNCTIONS *
 ************************/

int esc_mods_find_pci_dev(struct file *pfile, struct MODS_FIND_PCI_DEVICE *p)
{
	struct pci_dev *dev;
	int index = 0;

	mods_debug_printk(DEBUG_PCICFG,
			  "find pci dev %04x:%04x, index %d\n",
			  (int) p->vendor_id,
			  (int) p->device_id,
			  (int) p->index);

	dev = pci_get_device(p->vendor_id, p->device_id, NULL);

	while (dev) {
		if (index == p->index) {
			p->bus_number	  = dev->bus->number;
			p->device_number   = PCI_SLOT(dev->devfn);
			p->function_number = PCI_FUNC(dev->devfn);
			return OK;
		}
		dev = pci_get_device(p->vendor_id, p->device_id, dev);
		index++;
	}

	return -EINVAL;
}

int esc_mods_find_pci_class_code(struct file *pfile,
				 struct MODS_FIND_PCI_CLASS_CODE *p)
{
	struct pci_dev *dev;
	int index = 0;

	mods_debug_printk(DEBUG_PCICFG, "find pci class code %04x, index %d\n",
			  (int) p->class_code, (int) p->index);

	dev = pci_get_class(p->class_code, NULL);

	while (dev) {
		if (index == p->index) {
			p->bus_number		= dev->bus->number;
			p->device_number		= PCI_SLOT(dev->devfn);
			p->function_number	= PCI_FUNC(dev->devfn);
			return OK;
		}
		dev = pci_get_class(p->class_code, dev);
		index++;
	}

	return -EINVAL;
}

int esc_mods_pci_read(struct file *pfile, struct MODS_PCI_READ *p)
{
	struct pci_dev *dev;
	unsigned int devfn;

	devfn = PCI_DEVFN(p->device_number, p->function_number);
	dev = MODS_PCI_GET_SLOT(p->bus_number, devfn);

	if (dev == NULL)
		return -EINVAL;

	mods_debug_printk(DEBUG_PCICFG,
			  "pci read %x:%02x.%x, addr 0x%04x, size %d\n",
			  (int) p->bus_number, (int) p->device_number,
			  (int) p->function_number, (int) p->address,
			  (int) p->data_size);

	p->data = 0;
	switch (p->data_size) {
	case 1:
		pci_read_config_byte(dev, p->address, (u8 *) &p->data);
		break;
	case 2:
		pci_read_config_word(dev, p->address, (u16 *) &p->data);
		break;
	case 4:
		pci_read_config_dword(dev, p->address, (u32 *) &p->data);
		break;
	default:
		return -EINVAL;
	}
	return OK;
}

int esc_mods_pci_write(struct file *pfile, struct MODS_PCI_WRITE *p)
{
	struct pci_dev *dev;
	unsigned int devfn;

	mods_debug_printk(DEBUG_PCICFG,
			  "pci write %x:%02x.%x, addr 0x%04x, size %d, "
			  "data 0x%x\n",
			  (int) p->bus_number, (int) p->device_number,
			  (int) p->function_number,
			  (int) p->address, (int) p->data_size, (int) p->data);

	devfn = PCI_DEVFN(p->device_number, p->function_number);
	dev = MODS_PCI_GET_SLOT(p->bus_number, devfn);

	if (dev == NULL) {
		mods_error_printk(
		    "pci write to %x:%02x.%x, addr 0x%04x, size %d failed\n",
		    (unsigned)p->bus_number,
		    (unsigned)p->device_number,
		    (unsigned)p->function_number,
		    (unsigned)p->address,
		    (int)p->data_size);
		return -EINVAL;
	}

	switch (p->data_size) {
	case 1:
		pci_write_config_byte(dev, p->address, p->data);
		break;
	case 2:
		pci_write_config_word(dev, p->address, p->data);
		break;
	case 4:
		pci_write_config_dword(dev, p->address, p->data);
		break;
	default:
		return -EINVAL;
	}
	return OK;
}

int esc_mods_pci_bus_add_dev(struct file *pfile,
			     struct MODS_PCI_BUS_ADD_DEVICES *scan)
{
#if defined(CONFIG_PCI)
	mods_info_printk("scanning pci bus %x\n", scan->bus);

	/* initiate a PCI bus scan to find hotplugged PCI devices in domain 0 */
	pci_scan_child_bus(pci_find_bus(0, scan->bus));

	/* add newly found devices */
	pci_bus_add_devices(pci_find_bus(0, scan->bus));

	return OK;
#else
	return -EINVAL;
#endif
}

/************************
 * PIO ESCAPE FUNCTIONS *
 ************************/

int esc_mods_pio_read(struct file *pfile, struct MODS_PIO_READ *p)
{
	LOG_ENT();
	switch (p->data_size) {
	case 1:
		p->data = inb(p->port);
		break;
	case 2:
		p->data = inw(p->port);
		break;
	case 4:
		p->data = inl(p->port);
		break;
	default:
		return -EINVAL;
	}
	LOG_EXT();
	return OK;
}

int esc_mods_pio_write(struct file *pfile, struct MODS_PIO_WRITE  *p)
{
	LOG_ENT();
	switch (p->data_size) {
	case 1:
		outb(p->data, p->port);
		break;
	case 2:
		outw(p->data, p->port);
		break;
	case 4:
		outl(p->data, p->port);
		break;
	default:
		return -EINVAL;
	}
	LOG_EXT();
	return OK;
}

int esc_mods_device_numa_info(struct file *fp, struct MODS_DEVICE_NUMA_INFO *p)
{
#ifdef MODS_HAS_WC
	unsigned int devfn = PCI_DEVFN(p->pci_device.device,
				       p->pci_device.function);
	struct pci_dev *dev = MODS_PCI_GET_SLOT(p->pci_device.bus, devfn);

	LOG_ENT();

	if (dev == NULL) {
		mods_error_printk("PCI device %u:%u.%u not found\n",
				  p->pci_device.bus, p->pci_device.device,
				  p->pci_device.function);
		LOG_EXT();
		return -EINVAL;
	}

	p->node = dev_to_node(&dev->dev);
	if (-1 != p->node) {
		const unsigned long *maskp
			= cpumask_bits(cpumask_of_node(p->node));
		unsigned int i, word, bit, maskidx;

		if (((nr_cpumask_bits + 31) / 32) > MAX_CPU_MASKS) {
			mods_error_printk("too many CPUs (%d) for mask bits\n",
					  nr_cpumask_bits);
			LOG_EXT();
			return -EINVAL;
		}

		for (i = 0, maskidx = 0;
		     i < nr_cpumask_bits;
		     i += 32, maskidx++) {
			word = i / BITS_PER_LONG;
			bit = i % BITS_PER_LONG;
			p->node_cpu_mask[maskidx]
				= (maskp[word] >> bit) & 0xFFFFFFFFUL;
		}
	}
	p->node_count = num_possible_nodes();
	p->cpu_count = num_possible_cpus();

	LOG_EXT();
	return OK;
#else
	return -EINVAL;
#endif
}
