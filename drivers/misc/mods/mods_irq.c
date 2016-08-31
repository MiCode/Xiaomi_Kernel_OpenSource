/*
 * mods_irq.c - This file is part of NVIDIA MODS kernel driver.
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

#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/pci_regs.h>

#define PCI_VENDOR_ID_NVIDIA 0x10de
#define INDEX_IRQSTAT(irq)	(irq / BITS_NUM)
#define POS_IRQSTAT(irq)	 (irq & (BITS_NUM - 1))

/* MSI */
#define PCI_MSI_MASK_BIT	16
#define MSI_CONTROL_REG(base)		(base + PCI_MSI_FLAGS)
#define IS_64BIT_ADDRESS(control)	(!!(control & PCI_MSI_FLAGS_64BIT))
#define MSI_DATA_REG(base, is64bit) \
	((is64bit == 1) ? base + PCI_MSI_DATA_64 : base + PCI_MSI_DATA_32)

struct nv_device {
	char		  name[20];
	struct mods_priv *isr_pri;
	void		 *pri[MODS_CHANNEL_MAX];
};

/*********************
 * PRIVATE FUNCTIONS *
 *********************/
static struct mods_priv mp;
static struct nv_device nv_dev = { "nvidia mods", &mp, {0} };

static struct mods_priv *get_all_data(void)
{
	return &mp;
}

static struct nv_device *get_dev(void)
{
	return &nv_dev;
}

#ifdef CONFIG_PCI
static int mods_enable_device(struct mods_file_private_data *priv,
			      struct pci_dev *pdev)
{
	int ret = -1;
	struct en_dev_entry *entry = priv->enabled_devices;
	while (entry != 0) {
		if (entry->dev == pdev)
			return 0;
		entry = entry->next;
	}

	ret = pci_enable_device(pdev);
	if (ret == 0) {
		entry = 0;
		MODS_KMALLOC(entry, sizeof(*entry));
		if (unlikely(!entry))
			return 0;
		entry->dev = pdev;
		entry->next = priv->enabled_devices;
		priv->enabled_devices = entry;
	}
	return ret;
}
#endif

static unsigned int get_cur_time(void)
{
	/* This is not very precise, sched_clock() would be better */
	return jiffies_to_usecs(jiffies);
}

static int id_is_valid(unsigned char channel)
{
	if (channel <= 0 || channel > MODS_CHANNEL_MAX)
		return ERROR;

	return OK;
}

static inline int mods_check_interrupt(struct dev_irq_map *t)
{
	if (t->dev_irq_state && t->dev_irq_mask_reg) {
		/* GPU device */
		return *t->dev_irq_state && *t->dev_irq_mask_reg;
	} else {
		/* Non-GPU device - we can't tell */
		return true;
	}
}

static void mods_disable_interrupts(struct dev_irq_map *t)
{
	if (t->dev_irq_mask_reg) {
		if (t->irq_and_mask == 0) {
			*t->dev_irq_mask_reg = t->irq_or_mask;
		} else {
			*t->dev_irq_mask_reg =
				(*t->dev_irq_mask_reg & t->irq_and_mask)
				| t->irq_or_mask;
		}
	} else if (t->type == MODS_IRQ_TYPE_CPU) {
		disable_irq_nosync(t->apic_irq);
	}
}

static void rec_irq_done(struct nv_device *dev,
			 unsigned char channel,
			 struct dev_irq_map *t,
			 unsigned int irq_time)
{
	struct irq_q_info *q;
	struct mods_priv *pmp = dev->isr_pri;
	struct mods_file_private_data *private_data = dev->pri[channel];

	/* Get interrupt queue */
	q = &pmp->rec_info[channel - 1];

	/* Don't do anything if the IRQ has already been recorded */
	if (q->head != q->tail) {
		unsigned int i;
		for (i = q->head; i != q->tail; i++) {
			if (t->dev) {
				struct pci_dev *cur
					= q->data[i & (MODS_MAX_IRQS - 1)].dev;
				if (cur == t->dev)
					return;
			} else {
				NvU32 cur
					= q->data[i & (MODS_MAX_IRQS - 1)].irq;
				if (cur == t->apic_irq)
					return;
			}
		}
	}

	/* Print an error if the queue is full */
	/* This is deadly! */
	if (q->tail - q->head == MODS_MAX_IRQS) {
		mods_error_printk("IRQ queue is full\n");
		return;
	}

	/* Record the device which generated the IRQ in the queue */
	q->data[q->tail & (MODS_MAX_IRQS - 1)].dev = t->dev;
	q->data[q->tail & (MODS_MAX_IRQS - 1)].irq = t->apic_irq;
	q->data[q->tail & (MODS_MAX_IRQS - 1)].time = irq_time;
	q->tail++;

#ifdef CONFIG_PCI
	if (t->dev) {
		mods_debug_printk(DEBUG_ISR_DETAILED,
			"%s IRQ 0x%x for %x:%02x.%x, time=%uus\n",
			(t->type == MODS_IRQ_TYPE_MSI) ? "MSI" : "INTx",
			t->apic_irq,
			(unsigned)(t->dev->bus->number),
			(unsigned)PCI_SLOT(t->dev->devfn),
			(unsigned)PCI_FUNC(t->dev->devfn),
			irq_time);
	} else
#endif
		mods_debug_printk(DEBUG_ISR_DETAILED,
			"CPU IRQ 0x%x, time=%uus\n",
			t->apic_irq,
			irq_time);

	/* Wake MODS to handle the interrupt */
	if (private_data) {
		spin_unlock(&pmp->lock);
		wake_up_interruptible(&private_data->interrupt_event);
		spin_lock(&pmp->lock);
	}
}

/* mods_irq_handle - interrupt function */
static irqreturn_t mods_irq_handle(int irq, void *data
#ifndef MODS_IRQ_HANDLE_NO_REGS
			, struct pt_regs *regs
#endif
)
{
	struct nv_device *dev = (struct nv_device *)data;
	struct mods_priv *pmp = dev->isr_pri;
	struct dev_irq_map *t = NULL;
	unsigned char channel_idx;
	unsigned long flags = 0;
	int found = 0;
	unsigned int irq_time = get_cur_time();

	spin_lock_irqsave(&pmp->lock, flags);

	for (channel_idx = 0; channel_idx < MODS_CHANNEL_MAX; channel_idx++) {
		if (!(pmp->channel_flags & (1 << channel_idx)))
			continue;

		list_for_each_entry(t, &pmp->irq_head[channel_idx], list) {
			if ((t->apic_irq == irq) && mods_check_interrupt(t)) {
				/* Disable interrupts on this device to avoid
				 * interrupt storm*/
				mods_disable_interrupts(t);

				/* Record IRQ for MODS and wake MODS up */
				rec_irq_done(dev, channel_idx+1, t, irq_time);
				found |= 1;

				/* MSI and CPU interrupts are not shared,
				 * so stop looking */
				if (t->type != MODS_IRQ_TYPE_INT) {
					channel_idx = MODS_CHANNEL_MAX;
					break;
				}
			}
		}
	}

	spin_unlock_irqrestore(&pmp->lock, flags);
	return IRQ_RETVAL(found);
}

static int mods_lookup_irq(unsigned char channel, struct pci_dev *pdev,
						   unsigned int irq)
{
	unsigned char channel_idx;
	struct mods_priv *pmp = get_all_data();
	int ret = IRQ_NOT_FOUND;

	LOG_ENT();

	for (channel_idx = 0; channel_idx < MODS_CHANNEL_MAX; channel_idx++) {
		struct dev_irq_map *t = NULL;
		struct dev_irq_map *next = NULL;
		list_for_each_entry_safe(t,
					 next,
					 &pmp->irq_head[channel_idx],
					 list) {
			if ((pdev && (t->dev == pdev))
				|| (!pdev && (t->apic_irq == irq))) {

				if (channel == 0) {
					ret = IRQ_FOUND;
				} else {
					ret = (channel == channel_idx + 1)
						  ? IRQ_FOUND : IRQ_NOT_FOUND;
				}

				/* Break out of the outer loop */
				channel_idx = MODS_CHANNEL_MAX;
				break;
			}
		}
	}

	LOG_EXT();
	return ret;
}

static int add_irq_map(unsigned char channel,
		       struct pci_dev *pdev,
		       NvU32 irq,
		       unsigned int type)
{
	struct dev_irq_map *newmap = NULL;
	struct mods_priv *pmp = get_all_data();
	struct nv_device *nvdev = get_dev();

	LOG_ENT();

	/* Allocate memory for the new entry */
	MODS_KMALLOC(newmap, sizeof(*newmap));
	if (unlikely(!newmap)) {
		LOG_EXT();
		return -ENOMEM;
	}

	/* Fill out the new entry */
	newmap->apic_irq = irq;
	newmap->dev = pdev;
	newmap->channel = channel;
	newmap->dev_irq_aperture = 0;
	newmap->dev_irq_mask_reg = 0;
	newmap->dev_irq_state = 0;
	newmap->irq_and_mask = ~0U;
	newmap->irq_or_mask = 0;
	newmap->type = type;

	/* Enable IRQ for this device in the kernel */
	if (request_irq(
			irq,
			&mods_irq_handle,
			(type == MODS_IRQ_TYPE_INT) ? IRQF_SHARED : 0,
			nvdev->name,
			nvdev)) {
		mods_error_printk("unable to enable IRQ 0x%x\n", irq);
		MODS_KFREE(newmap, sizeof(*newmap));
		LOG_EXT();
		return ERROR;
	}

	/* Add the new entry to the list of all registered interrupts */
	list_add(&newmap->list, &pmp->irq_head[channel - 1]);

#ifdef CONFIG_PCI
	/* Map BAR0 of a graphics card to be able to disable interrupts */
	if (type == MODS_IRQ_TYPE_INT) {
		unsigned short class_code, vendor_id, device_id;
		pci_read_config_word(pdev, PCI_CLASS_DEVICE, &class_code);
		pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor_id);
		pci_read_config_word(pdev, PCI_DEVICE_ID, &device_id);
		if ((class_code == PCI_CLASS_DISPLAY_VGA) ||
			(class_code == PCI_CLASS_DISPLAY_3D)) {

			if (vendor_id == 0x10DE) {
				char *bar = ioremap_nocache(
						pci_resource_start(pdev, 0),
						0x200);
				newmap->dev_irq_aperture = bar;
				newmap->dev_irq_mask_reg = (NvU32 *)(bar+0x140);
				newmap->dev_irq_state    = (NvU32 *)(bar+0x100);
				newmap->irq_and_mask	 = 0;
				newmap->irq_or_mask      = 0;
			}
		}
	}
#endif

	/* Print out successful registration string */
	if (type == MODS_IRQ_TYPE_CPU)
		mods_debug_printk(DEBUG_ISR, "registered CPU IRQ 0x%x\n", irq);
#ifdef CONFIG_PCI
	else if (type == MODS_IRQ_TYPE_INT) {
		mods_debug_printk(DEBUG_ISR,
			"registered INTx IRQ 0x%x for device %x:%02x.%x\n",
			pdev->irq,
			(unsigned)(pdev->bus->number),
			(unsigned)PCI_SLOT(pdev->devfn),
			(unsigned)PCI_FUNC(pdev->devfn));
	}
#endif
#ifdef CONFIG_PCI_MSI
	else if (type == MODS_IRQ_TYPE_MSI) {
		u16 control;
		u16 data;
		int cap_pos = pci_find_capability(pdev, PCI_CAP_ID_MSI);
		pci_read_config_word(pdev, MSI_CONTROL_REG(cap_pos), &control);
		if (IS_64BIT_ADDRESS(control))
			pci_read_config_word(pdev,
					     MSI_DATA_REG(cap_pos, 1),
					     &data);
		else
			pci_read_config_word(pdev,
					     MSI_DATA_REG(cap_pos, 0),
					     &data);
		mods_debug_printk(DEBUG_ISR,
				"registered MSI IRQ 0x%x with data 0x%02x "
				"for device %x:%02x.%x\n",
				pdev->irq,
				(unsigned)data,
				(unsigned)(pdev->bus->number),
				(unsigned)PCI_SLOT(pdev->devfn),
				(unsigned)PCI_FUNC(pdev->devfn));
	}
#endif

	LOG_EXT();
	return OK;
}

static void mods_free_map(struct dev_irq_map *del)
{
	LOG_ENT();

	/* Disable interrupts on the device */
	mods_disable_interrupts(del);

	/* Unmap aperture used for masking irqs */
	if (del->dev_irq_aperture)
		iounmap(del->dev_irq_aperture);

	/* Unhook interrupts in the kernel */
	free_irq(del->apic_irq, get_dev());

	/* Disable MSI */
#ifdef CONFIG_PCI_MSI
	if (del->type == MODS_IRQ_TYPE_MSI)
		pci_disable_msi(del->dev);
#endif

	/* Free memory */
	MODS_KFREE(del, sizeof(*del));

	LOG_EXT();
}

/*******************
* PUBLIC FUNCTIONS *
********************/
void mods_init_irq(void)
{
	int i;
	struct mods_priv *pmp = get_all_data();

	LOG_ENT();

	memset(pmp, 0, sizeof(struct mods_priv));
	for (i = 0; i < MODS_CHANNEL_MAX; i++)
		INIT_LIST_HEAD(&pmp->irq_head[i]);

	spin_lock_init(&pmp->lock);
	LOG_EXT();
}

void mods_cleanup_irq(void)
{
	int i;
	struct mods_priv *pmp = get_all_data();

	LOG_ENT();
	for (i = 0; i < MODS_CHANNEL_MAX; i++) {
		if (pmp->channel_flags && (1 << i))
			mods_free_channel(i + 1);
	}
	LOG_EXT();
}

void mods_irq_dev_set_pri(unsigned char id, void *pri)
{
	struct nv_device *dev = get_dev();
	dev->pri[id] = pri;
}

void mods_irq_dev_clr_pri(unsigned char id)
{
	struct nv_device *dev = get_dev();
	dev->pri[id] = 0;
}

int mods_irq_event_check(unsigned char channel)
{
	struct mods_priv *pmp = get_all_data();
	struct irq_q_info *q = &pmp->rec_info[channel - 1];
	unsigned int pos = (1 << (channel - 1));

	if (!(pmp->channel_flags & pos))
		return POLLERR; /* irq has quit */

	if (q->head != q->tail)
		return POLLIN; /* irq generated */

	return 0;
}

unsigned char mods_alloc_channel(void)
{
	struct mods_priv *pmp = get_all_data();
	int i = 0;
	unsigned char channel = MODS_CHANNEL_MAX + 1;
	unsigned char max_channels = mods_get_multi_instance()
				     ? MODS_CHANNEL_MAX : 1;

	LOG_ENT();

	for (i = 0; i < max_channels; i++) {
		if (!test_and_set_bit(i, &pmp->channel_flags)) {
			channel = i + 1;
			mods_debug_printk(DEBUG_IOCTL,
					  "open channel %u (bit mask 0x%lx)\n",
					  (unsigned)(i+1), pmp->channel_flags);
			break;
		}

	}

	LOG_EXT();
	return channel;
}

void mods_free_channel(unsigned char channel)
{
	struct mods_priv *pmp = get_all_data();
	struct dev_irq_map *del = NULL;
	struct dev_irq_map *next = NULL;
	struct irq_q_info *q = &pmp->rec_info[channel - 1];

	LOG_ENT();

	/* Release all interrupts */
	list_for_each_entry_safe(del, next, &pmp->irq_head[channel - 1], list) {
		list_del(&del->list);
		if (del->type == MODS_IRQ_TYPE_CPU) {
			mods_warning_printk(
				"CPU IRQ 0x%x is still hooked, unhooking\n",
				del->apic_irq);
		}
#ifdef CONFIG_PCI
		else {
			mods_warning_printk(
	"%s IRQ 0x%x for device %x:%02x.%x is still hooked, unhooking\n",
				(del->type == MODS_IRQ_TYPE_MSI)
					? "MSI" : "INTx",
				del->dev->irq,
				(unsigned)(del->dev->bus->number),
				(unsigned)PCI_SLOT(del->dev->devfn),
				(unsigned)PCI_FUNC(del->dev->devfn));
		}
#endif
		mods_free_map(del);
	}

	/* Clear queue */
	memset(q, 0, sizeof(*q));

	/* Indicate the channel is free */
	clear_bit(channel - 1, &pmp->channel_flags);

	mods_debug_printk(DEBUG_IOCTL, "closed channel %u\n",
			  (unsigned)channel);
	LOG_EXT();
}

#ifdef CONFIG_PCI
static int mods_register_pci_irq(struct file *pfile,
				 struct MODS_REGISTER_IRQ *p)
{
	struct pci_dev *dev;
	unsigned int devfn;
	unsigned char channel;
	MODS_PRIVATE_DATA(private_data, pfile);

	LOG_ENT();

	/* Identify the caller */
	channel = MODS_GET_FILE_PRIVATE_ID(pfile);
	BUG_ON(id_is_valid(channel) != OK);

	/* Get the PCI device structure for the specified device from kernel */
	devfn = PCI_DEVFN(p->dev.device, p->dev.function);
	dev = MODS_PCI_GET_SLOT(p->dev.bus, devfn);
	if (!dev) {
		LOG_EXT();
		return ERROR;
	}

	/* Determine if the interrupt is already hooked */
	if (mods_lookup_irq(0, dev, 0) == IRQ_FOUND) {
		mods_error_printk(
		    "IRQ for device %x:%02x.%x has already been registered\n",
		    (unsigned)p->dev.bus,
		    (unsigned)p->dev.device,
		    (unsigned)p->dev.function);
		LOG_EXT();
		return ERROR;
	}

	/* Determine if the device supports MSI */
	if (p->type == MODS_IRQ_TYPE_MSI) {
#ifdef CONFIG_PCI_MSI
		if (0 == pci_find_capability(dev, PCI_CAP_ID_MSI)) {
			mods_error_printk(
				"device %x:%02x.%x does not support MSI\n",
				(unsigned)p->dev.bus,
				(unsigned)p->dev.device,
				(unsigned)p->dev.function);
			LOG_EXT();
			return ERROR;
		}
#else
		mods_error_printk("the kernel does not support MSI!\n");
		return ERROR;
#endif
	}

	/* Enable device on the PCI bus */
	if (mods_enable_device(private_data, dev)) {
		mods_error_printk("unable to enable device %x:%02x.%x\n",
				  (unsigned)p->dev.bus,
				  (unsigned)p->dev.device,
				  (unsigned)p->dev.function);
		LOG_EXT();
		return ERROR;
	}

	/* Enable MSI */
#ifdef CONFIG_PCI_MSI
	if (p->type == MODS_IRQ_TYPE_MSI) {
		if (0 != pci_enable_msi(dev)) {
			mods_error_printk(
				"unable to enable MSI on device %x:%02x.%x\n",
				(unsigned)p->dev.bus,
				(unsigned)p->dev.device,
				(unsigned)p->dev.function);
			return ERROR;
		}
	}
#endif

	/* Register interrupt */
	if (add_irq_map(channel, dev, dev->irq, p->type) != OK) {
#ifdef CONFIG_PCI_MSI
		if (p->type == MODS_IRQ_TYPE_MSI)
			pci_disable_msi(dev);
#endif
		LOG_EXT();
		return ERROR;
	}

	return OK;
}
#endif /* CONFIG_PCI */

static int mods_register_cpu_irq(struct file *pfile,
				 struct MODS_REGISTER_IRQ *p)
{
	unsigned char channel;
	unsigned int irq;

	LOG_ENT();

	irq = p->dev.bus;

	/* Identify the caller */
	channel = MODS_GET_FILE_PRIVATE_ID(pfile);
	BUG_ON(id_is_valid(channel) != OK);

	/* Determine if the interrupt is already hooked */
	if (mods_lookup_irq(0, 0, irq) == IRQ_FOUND) {
		mods_error_printk("CPU IRQ 0x%x has already been registered\n",
				  irq);
		LOG_EXT();
		return ERROR;
	}

	/* Register interrupt */
	if (add_irq_map(channel, 0, irq, p->type) != OK) {
		LOG_EXT();
		return ERROR;
	}

	return OK;
}

#ifdef CONFIG_PCI
static int mods_unregister_pci_irq(struct file *pfile,
				   struct MODS_REGISTER_IRQ *p)
{
	struct mods_priv *pmp = get_all_data();
	struct dev_irq_map *del = NULL;
	struct dev_irq_map *next;
	struct pci_dev *dev;
	unsigned int devfn;
	unsigned char channel;

	LOG_ENT();

	/* Identify the caller */
	channel = MODS_GET_FILE_PRIVATE_ID(pfile);
	BUG_ON(id_is_valid(channel) != OK);

	/* Get the PCI device structure for the specified device from kernel */
	devfn = PCI_DEVFN(p->dev.device, p->dev.function);
	dev = MODS_PCI_GET_SLOT(p->dev.bus, devfn);
	if (!dev) {
		LOG_EXT();
		return ERROR;
	}

	/* Determine if the interrupt is already hooked by this client */
	if (mods_lookup_irq(channel, dev, 0) == IRQ_NOT_FOUND) {
		mods_error_printk(
			"IRQ for device %x:%02x.%x not hooked, can't unhook\n",
			(unsigned)p->dev.bus,
			(unsigned)p->dev.device,
			(unsigned)p->dev.function);
		LOG_EXT();
		return ERROR;
	}

	/* Delete device interrupt from the list */
	list_for_each_entry_safe(del, next, &pmp->irq_head[channel - 1], list) {
		if (dev == del->dev) {
			if (del->type != p->type) {
				mods_error_printk("wrong IRQ type passed\n");
				LOG_EXT();
				return ERROR;
			}
			list_del(&del->list);
			mods_debug_printk(DEBUG_ISR,
				"unregistered %s IRQ 0x%x for device "
				"%x:%02x.%x\n",
				(del->type == MODS_IRQ_TYPE_MSI)
					? "MSI" : "INTx",
				del->dev->irq,
				(unsigned)p->dev.bus,
				(unsigned)p->dev.device,
				(unsigned)p->dev.function);
			mods_free_map(del);
			break;
		}
	}

	LOG_EXT();
	return OK;
}
#endif

static int mods_unregister_cpu_irq(struct file *pfile,
				   struct MODS_REGISTER_IRQ *p)
{
	struct mods_priv *pmp = get_all_data();
	struct dev_irq_map *del = NULL;
	struct dev_irq_map *next;
	unsigned int irq;
	unsigned char channel;

	LOG_ENT();

	irq = p->dev.bus;

	/* Identify the caller */
	channel = MODS_GET_FILE_PRIVATE_ID(pfile);
	BUG_ON(id_is_valid(channel) != OK);

	/* Determine if the interrupt is already hooked by this client */
	if (mods_lookup_irq(channel, 0, irq) == IRQ_NOT_FOUND) {
		mods_error_printk(
			"IRQ 0x%x not hooked, can't unhook\n",
			irq);
		LOG_EXT();
		return ERROR;
	}

	/* Delete device interrupt from the list */
	list_for_each_entry_safe(del, next, &pmp->irq_head[channel - 1], list) {
		if ((irq == del->apic_irq) && (del->dev == 0)) {
			if (del->type != p->type) {
				mods_error_printk("wrong IRQ type passed\n");
				LOG_EXT();
				return ERROR;
			}
			list_del(&del->list);
			mods_debug_printk(DEBUG_ISR,
					  "unregistered CPU IRQ 0x%x\n",
					  irq);
			mods_free_map(del);
			break;
		}
	}

	LOG_EXT();
	return OK;
}

/*************************
 * ESCAPE CALL FUNCTIONS *
 *************************/

int esc_mods_register_irq(struct file *pfile, struct MODS_REGISTER_IRQ *p)
{
	if (p->type == MODS_IRQ_TYPE_CPU) {
		return mods_register_cpu_irq(pfile, p);
	} else {
#ifdef CONFIG_PCI
		return mods_register_pci_irq(pfile, p);
#else
		mods_error_printk("PCI not available\n");
		return -EINVAL;
#endif
	}
}

int esc_mods_unregister_irq(struct file *pfile, struct MODS_REGISTER_IRQ *p)
{
	if (p->type == MODS_IRQ_TYPE_CPU) {
		return mods_unregister_cpu_irq(pfile, p);
	} else {
#ifdef CONFIG_PCI
		return mods_unregister_pci_irq(pfile, p);
#else
		return -EINVAL;
#endif
	}
}

int esc_mods_query_irq(struct file *pfile, struct MODS_QUERY_IRQ *p)
{
	unsigned char channel;
	struct irq_q_info *q = NULL;
	struct mods_priv *pmp = get_all_data();
	unsigned int i = 0;
	unsigned long flags = 0;
	unsigned int cur_time = get_cur_time();

	/* Lock IRQ queue */
	spin_lock_irqsave(&pmp->lock, flags);
	LOG_ENT();

	/* Identify the caller */
	channel = MODS_GET_FILE_PRIVATE_ID(pfile);
	BUG_ON(id_is_valid(channel) != OK);

	/* Clear return array */
	memset(p->irq_list, 0xFF, sizeof(p->irq_list));

	/* Fill in return array with IRQ information */
	q = &pmp->rec_info[channel - 1];
	for (i = 0;
	     (q->head != q->tail) && (i < MODS_MAX_IRQS);
	     q->head++, i++) {
		unsigned int index = q->head & (MODS_MAX_IRQS - 1);
		struct pci_dev *dev = q->data[index].dev;
		if (dev) {
			p->irq_list[i].dev.bus = dev->bus->number;
			p->irq_list[i].dev.device = PCI_SLOT(dev->devfn);
			p->irq_list[i].dev.function = PCI_FUNC(dev->devfn);
		} else {
			p->irq_list[i].dev.bus = q->data[index].irq;
			p->irq_list[i].dev.device = 0xFFU;
			p->irq_list[i].dev.function = 0xFFU;
		}
		p->irq_list[i].delay = cur_time - q->data[index].time;

		/* Print info about IRQ status returned */
		if (dev) {
			mods_debug_printk(DEBUG_ISR_DETAILED,
				"retrieved IRQ for %x:%02x.%x, time=%uus, "
				"delay=%uus\n",
				(unsigned)p->irq_list[i].dev.bus,
				(unsigned)p->irq_list[i].dev.device,
				(unsigned)p->irq_list[i].dev.function,
				q->data[index].time,
				p->irq_list[i].delay);
		} else {
			mods_debug_printk(DEBUG_ISR_DETAILED,
				"retrieved IRQ 0x%x, time=%uus, delay=%uus\n",
				(unsigned)p->irq_list[i].dev.bus,
				q->data[index].time,
				p->irq_list[i].delay);
		}
	}

	/* Indicate if there are more IRQs pending */
	if (q->head != q->tail)
		p->more = 1;

	/* Unlock IRQ queue */
	LOG_EXT();
	spin_unlock_irqrestore(&pmp->lock, flags);

	return OK;
}

int esc_mods_set_irq_mask(struct file *pfile, struct MODS_SET_IRQ_MASK *p)
{
	struct mods_priv *pmp = get_all_data();
	unsigned long flags = 0;
	unsigned char channel;
	struct pci_dev *dev = 0;
	NvU32 irq = ~0U;
	struct dev_irq_map *t = NULL;
	struct dev_irq_map *next = NULL;
	int ret = -EINVAL;

	/* Lock IRQ queue */
	spin_lock_irqsave(&pmp->lock, flags);
	LOG_ENT();

	/* Identify the caller */
	channel = MODS_GET_FILE_PRIVATE_ID(pfile);
	BUG_ON(id_is_valid(channel) != OK);

	/* Print info */
	if (p->irq_type == MODS_IRQ_TYPE_CPU) {
		mods_debug_printk(
			DEBUG_ISR,
			"set CPU IRQ 0x%x mask &0x%x |0x%x addr=0x%llx\n",
			(unsigned)p->dev.bus,
			p->and_mask, p->or_mask,
			p->aperture_addr + p->reg_offset);
	} else {
		mods_debug_printk(
			DEBUG_ISR,
			"set dev %x:%02x.%x IRQ mask &0x%x |0x%x addr=0x%llx\n",
			(unsigned)p->dev.bus,
			(unsigned)p->dev.device,
			(unsigned)p->dev.function,
			  p->and_mask,
			  p->or_mask,
			  p->aperture_addr + p->reg_offset);
	}

	/* Verify mask type */
	if (p->mask_type != MODS_MASK_TYPE_IRQ_DISABLE) {
		mods_error_printk("invalid mask type\n");
		LOG_EXT();
		spin_unlock_irqrestore(&pmp->lock, flags);
		return -EINVAL;
	}

	/* Determine which interrupt is referenced */
	if (p->irq_type == MODS_IRQ_TYPE_CPU) {
		irq = p->dev.bus;
	} else {
#ifdef CONFIG_PCI
		/* Get the PCI dev struct for the specified device from kernel*/
		unsigned int devfn = PCI_DEVFN(p->dev.device, p->dev.function);
		dev = MODS_PCI_GET_SLOT(p->dev.bus, devfn);
		if (!dev) {
			LOG_EXT();
			spin_unlock_irqrestore(&pmp->lock, flags);
			return -EINVAL;
		}
#else
		mods_error_printk("PCI not available\n");
		LOG_EXT();
		spin_unlock_irqrestore(&pmp->lock, flags);
		return -EINVAL;
#endif
	}

	list_for_each_entry_safe(t, next, &pmp->irq_head[channel-1], list) {
		if ((dev && (t->dev == dev))
			|| (!dev && (t->apic_irq == irq))) {

			if (t->type != p->irq_type) {
				mods_error_printk(
				"IRQ type does not match registered IRQ\n");
			} else {
				char *bar = 0;

				if (t->dev_irq_aperture) {
					iounmap(t->dev_irq_aperture);
					t->dev_irq_aperture = 0;
					t->dev_irq_mask_reg = 0;
					t->dev_irq_state = 0;
					mods_warning_printk(
						    "resetting IRQ mask\n");
				}

				bar = ioremap_nocache(p->aperture_addr,
						      p->aperture_size);
				if (bar) {
					t->dev_irq_aperture = bar;
					t->dev_irq_mask_reg
					    = (NvU32 *)(bar + p->reg_offset);
					t->dev_irq_state = 0;
					t->irq_and_mask = p->and_mask;
					t->irq_or_mask = p->or_mask;
					ret = OK;
				} else {
					mods_error_printk(
					"unable to remap specified aperture\n");
				}
			}
			break;
		}
	}

	/* Unlock IRQ queue */
	LOG_EXT();
	spin_unlock_irqrestore(&pmp->lock, flags);

	return ret;
}

int esc_mods_irq_handled(struct file *pfile, struct MODS_REGISTER_IRQ *p)
{
	struct mods_priv *pmp = get_all_data();
	unsigned long flags = 0;
	unsigned char channel;
	NvU32 irq = p->dev.bus;
	struct dev_irq_map *t = NULL;
	struct dev_irq_map *next = NULL;
	int ret = -EINVAL;

	if (p->type != MODS_IRQ_TYPE_CPU)
		return -EINVAL;

	/* Lock IRQ queue */
	spin_lock_irqsave(&pmp->lock, flags);
	LOG_ENT();

	/* Identify the caller */
	channel = MODS_GET_FILE_PRIVATE_ID(pfile);
	BUG_ON(id_is_valid(channel) != OK);

	/* Print info */
	mods_debug_printk(DEBUG_ISR_DETAILED,
			  "mark CPU IRQ 0x%x handled\n", irq);

	list_for_each_entry_safe(t, next, &pmp->irq_head[channel-1], list) {
		if (t->apic_irq == irq) {
			if (t->type != p->type) {
				mods_error_printk(
				    "IRQ type does not match registered IRQ\n");
			} else {
				enable_irq(irq);
				ret = OK;
			}
			break;
		}
	}

	/* Unlock IRQ queue */
	LOG_EXT();
	spin_unlock_irqrestore(&pmp->lock, flags);

	return ret;
}
