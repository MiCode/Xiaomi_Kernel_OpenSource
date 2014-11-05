/* drivers/gpio/gpio-msm-smp2p.c
 *
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/bitmap.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/ipc_logging.h>
#include "../soc/qcom/smp2p_private_api.h"
#include "../soc/qcom/smp2p_private.h"

/* GPIO device - one per SMP2P entry. */
struct smp2p_chip_dev {
	struct list_head entry_list;
	char name[SMP2P_MAX_ENTRY_NAME];
	int remote_pid;
	bool is_inbound;
	bool is_open;
	bool in_shadow;
	uint32_t shadow_value;
	struct work_struct shadow_work;
	spinlock_t shadow_lock;
	struct notifier_block out_notifier;
	struct notifier_block in_notifier;
	struct msm_smp2p_out *out_handle;

	struct gpio_chip gpio;
	struct irq_domain *irq_domain;
	int irq_base;

	spinlock_t irq_lock;
	DECLARE_BITMAP(irq_enabled, SMP2P_BITS_PER_ENTRY);
	DECLARE_BITMAP(irq_rising_edge, SMP2P_BITS_PER_ENTRY);
	DECLARE_BITMAP(irq_falling_edge, SMP2P_BITS_PER_ENTRY);
};

static struct platform_driver smp2p_gpio_driver;
static struct lock_class_key smp2p_gpio_lock_class;
static struct irq_chip smp2p_gpio_irq_chip;
static DEFINE_SPINLOCK(smp2p_entry_lock_lha1);
static LIST_HEAD(smp2p_entry_list);

/* Used for mapping edge to name for logging. */
static const char * const edge_names[] = {
	"-",
	"0->1",
	"1->0",
	"-",
};

/* Used for mapping edge to value for logging. */
static const char * const edge_name_rising[] = {
	"-",
	"0->1",
};

/* Used for mapping edge to value for logging. */
static const char * const edge_name_falling[] = {
	"-",
	"1->0",
};

static int smp2p_gpio_to_irq(struct gpio_chip *cp,
	unsigned offset);

/**
 * smp2p_get_value - Retrieves GPIO value.
 *
 * @cp:      GPIO chip pointer
 * @offset:  Pin offset
 * @returns: >=0: value of GPIO Pin; < 0 for error
 *
 * Error codes:
 *   -ENODEV - chip/entry invalid
 *   -ENETDOWN - valid entry, but entry not yet created
 */
static int smp2p_get_value(struct gpio_chip *cp,
	unsigned offset)
{
	struct smp2p_chip_dev *chip;
	int ret = 0;
	uint32_t data;

	if (!cp)
		return -ENODEV;

	chip = container_of(cp, struct smp2p_chip_dev, gpio);
	if (!chip->is_open)
		return -ENETDOWN;

	if (chip->is_inbound)
		ret = msm_smp2p_in_read(chip->remote_pid, chip->name, &data);
	else
		ret = msm_smp2p_out_read(chip->out_handle, &data);

	if (!ret)
		ret = (data & (1 << offset)) ? 1 : 0;

	return ret;
}

/**
 * smp2p_set_value - Sets GPIO value.
 *
 * @cp:     GPIO chip pointer
 * @offset: Pin offset
 * @value:  New value
 */
static void smp2p_set_value(struct gpio_chip *cp, unsigned offset, int value)
{
	struct smp2p_chip_dev *chip;
	uint32_t data_set;
	uint32_t data_clear;
	bool send_irq;
	int ret;
	unsigned long flags;

	if (!cp)
		return;

	chip = container_of(cp, struct smp2p_chip_dev, gpio);

	if (chip->is_inbound) {
		SMP2P_INFO("%s: '%s':%d virq %d invalid operation\n",
			__func__, chip->name, chip->remote_pid,
			chip->irq_base + offset);
		return;
	}

	if (value & SMP2P_GPIO_NO_INT) {
		value &= ~SMP2P_GPIO_NO_INT;
		send_irq = false;
	} else {
		send_irq = true;
	}

	if (value) {
		data_set = 1 << offset;
		data_clear = 0;
	} else {
		data_set = 0;
		data_clear = 1 << offset;
	}

	spin_lock_irqsave(&chip->shadow_lock, flags);
	if (!chip->is_open) {
		chip->in_shadow = true;
		chip->shadow_value &= ~data_clear;
		chip->shadow_value |= data_set;
		spin_unlock_irqrestore(&chip->shadow_lock, flags);
		return;
	}

	if (chip->in_shadow) {
		chip->in_shadow = false;
		chip->shadow_value &= ~data_clear;
		chip->shadow_value |= data_set;
		ret = msm_smp2p_out_modify(chip->out_handle,
				chip->shadow_value, 0x0, send_irq);
		chip->shadow_value = 0x0;
	} else {
		ret = msm_smp2p_out_modify(chip->out_handle,
				data_set, data_clear, send_irq);
	}
	spin_unlock_irqrestore(&chip->shadow_lock, flags);

	if (ret)
		SMP2P_GPIO("'%s':%d gpio %d set to %d failed (%d)\n",
			chip->name, chip->remote_pid,
			chip->gpio.base + offset, value, ret);
	else
		SMP2P_GPIO("'%s':%d gpio %d set to %d\n",
			chip->name, chip->remote_pid,
			chip->gpio.base + offset, value);
}

/**
 * smp2p_direction_input - Sets GPIO direction to input.
 *
 * @cp:      GPIO chip pointer
 * @offset:  Pin offset
 * @returns: 0 for success; < 0 for failure
 */
static int smp2p_direction_input(struct gpio_chip *cp, unsigned offset)
{
	struct smp2p_chip_dev *chip;

	if (!cp)
		return -ENODEV;

	chip = container_of(cp, struct smp2p_chip_dev, gpio);
	if (!chip->is_inbound)
		return -EPERM;

	return 0;
}

/**
 * smp2p_direction_output - Sets GPIO direction to output.
 *
 * @cp:      GPIO chip pointer
 * @offset:  Pin offset
 * @value:   Direction
 * @returns: 0 for success; < 0 for failure
 */
static int smp2p_direction_output(struct gpio_chip *cp,
	unsigned offset, int value)
{
	struct smp2p_chip_dev *chip;

	if (!cp)
		return -ENODEV;

	chip = container_of(cp, struct smp2p_chip_dev, gpio);
	if (chip->is_inbound)
		return -EPERM;

	return 0;
}

/**
 * smp2p_gpio_to_irq - Convert GPIO pin to virtual IRQ pin.
 *
 * @cp:      GPIO chip pointer
 * @offset:  Pin offset
 * @returns: >0 for virtual irq value; < 0 for failure
 */
static int smp2p_gpio_to_irq(struct gpio_chip *cp, unsigned offset)
{
	struct smp2p_chip_dev *chip;

	chip = container_of(cp, struct smp2p_chip_dev, gpio);
	if (!cp || chip->irq_base <= 0)
		return -ENODEV;

	return chip->irq_base + offset;
}

/**
 * smp2p_gpio_irq_mask_helper - Mask/Unmask interrupt.
 *
 * @d:    IRQ data
 * @mask: true to mask (disable), false to unmask (enable)
 */
static void smp2p_gpio_irq_mask_helper(struct irq_data *d, bool mask)
{
	struct smp2p_chip_dev *chip;
	int offset;
	unsigned long flags;

	chip = (struct smp2p_chip_dev *)irq_get_chip_data(d->irq);
	if (!chip || chip->irq_base <= 0)
		return;

	offset = d->irq - chip->irq_base;
	spin_lock_irqsave(&chip->irq_lock, flags);
	if (mask)
		clear_bit(offset, chip->irq_enabled);
	else
		set_bit(offset, chip->irq_enabled);
	spin_unlock_irqrestore(&chip->irq_lock, flags);
}

/**
 * smp2p_gpio_irq_mask - Mask interrupt.
 *
 * @d: IRQ data
 */
static void smp2p_gpio_irq_mask(struct irq_data *d)
{
	smp2p_gpio_irq_mask_helper(d, true);
}

/**
 * smp2p_gpio_irq_unmask - Unmask interrupt.
 *
 * @d: IRQ data
 */
static void smp2p_gpio_irq_unmask(struct irq_data *d)
{
	smp2p_gpio_irq_mask_helper(d, false);
}

/**
 * smp2p_gpio_irq_set_type - Set interrupt edge type.
 *
 * @d:      IRQ data
 * @type:   Edge type for interrupt
 * @returns 0 for success; < 0 for failure
 */
static int smp2p_gpio_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct smp2p_chip_dev *chip;
	int offset;
	unsigned long flags;
	int ret = 0;

	chip = (struct smp2p_chip_dev *)irq_get_chip_data(d->irq);
	if (!chip)
		return -ENODEV;

	if (chip->irq_base <= 0) {
		SMP2P_ERR("%s: '%s':%d virqbase %d invalid\n",
			__func__, chip->name, chip->remote_pid,
			chip->irq_base);
		return -ENODEV;
	}

	offset = d->irq - chip->irq_base;

	spin_lock_irqsave(&chip->irq_lock, flags);
	clear_bit(offset, chip->irq_rising_edge);
	clear_bit(offset, chip->irq_falling_edge);
	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		set_bit(offset, chip->irq_rising_edge);
		break;

	case IRQ_TYPE_EDGE_FALLING:
		set_bit(offset, chip->irq_falling_edge);
		break;

	case IRQ_TYPE_NONE:
	case IRQ_TYPE_DEFAULT:
	case IRQ_TYPE_EDGE_BOTH:
		set_bit(offset, chip->irq_rising_edge);
		set_bit(offset, chip->irq_falling_edge);
		break;

	default:
		SMP2P_ERR("%s: unsupported interrupt type 0x%x\n",
				__func__, type);
		ret = -EINVAL;
		break;
	}
	spin_unlock_irqrestore(&chip->irq_lock, flags);
	return ret;
}

/**
 * smp2p_irq_map - Creates or updates binding of virtual IRQ
 *
 * @domain_ptr: Interrupt domain pointer
 * @virq:       Virtual IRQ
 * @hw:         Hardware IRQ (same as virq for nomap)
 * @returns:    0 for success
 */
static int smp2p_irq_map(struct irq_domain *domain_ptr, unsigned int virq,
	irq_hw_number_t hw)
{
	struct smp2p_chip_dev *chip;

	chip = domain_ptr->host_data;
	if (!chip) {
		SMP2P_ERR("%s: invalid domain ptr %p\n", __func__, domain_ptr);
		return -ENODEV;
	}

	/* map chip structures to device */
	irq_set_lockdep_class(virq, &smp2p_gpio_lock_class);
	irq_set_chip_and_handler(virq, &smp2p_gpio_irq_chip,
				 handle_level_irq);
	irq_set_chip_data(virq, chip);
	set_irq_flags(virq, IRQF_VALID);

	return 0;
}

static struct irq_chip smp2p_gpio_irq_chip = {
	.name = "smp2p_gpio",
	.irq_mask = smp2p_gpio_irq_mask,
	.irq_unmask = smp2p_gpio_irq_unmask,
	.irq_set_type = smp2p_gpio_irq_set_type,
};

/* No-map interrupt Domain */
static const struct irq_domain_ops smp2p_irq_domain_ops = {
	.map = smp2p_irq_map,
};

/**
 * msm_summary_irq_handler - Handles inbound entry change notification.
 *
 * @chip:  GPIO chip pointer
 * @entry: Change notification data
 *
 * Whenever an entry changes, this callback is triggered to determine
 * which bits changed and if the corresponding interrupts need to be
 * triggered.
 */
static void msm_summary_irq_handler(struct smp2p_chip_dev *chip,
	struct msm_smp2p_update_notif *entry)
{
	int i;
	uint32_t cur_val;
	uint32_t prev_val;
	uint32_t edge;
	unsigned long flags;
	bool trigger_interrrupt;
	bool irq_rising;
	bool irq_falling;

	cur_val = entry->current_value;
	prev_val = entry->previous_value;

	if (chip->irq_base <= 0)
		return;

	SMP2P_GPIO("'%s':%d GPIO Summary IRQ Change %08x->%08x\n",
			chip->name, chip->remote_pid, prev_val, cur_val);

	for (i = 0; i < SMP2P_BITS_PER_ENTRY; ++i) {
		spin_lock_irqsave(&chip->irq_lock, flags);
		trigger_interrrupt = false;
		edge = (prev_val & 0x1) << 1 | (cur_val & 0x1);
		irq_rising = test_bit(i, chip->irq_rising_edge);
		irq_falling = test_bit(i, chip->irq_falling_edge);

		if (test_bit(i, chip->irq_enabled)) {
			if (edge == 0x1 && irq_rising)
				/* 0->1 transition */
				trigger_interrrupt = true;
			else if (edge == 0x2 && irq_falling)
				/* 1->0 transition */
				trigger_interrrupt = true;
		} else {
			SMP2P_GPIO(
				"'%s':%d GPIO bit %d virq %d (%s,%s) - edge %s disabled\n",
				chip->name, chip->remote_pid, i,
				chip->irq_base + i,
				edge_name_rising[irq_rising],
				edge_name_falling[irq_falling],
				edge_names[edge]);
		}
		spin_unlock_irqrestore(&chip->irq_lock, flags);

		if (trigger_interrrupt) {
			SMP2P_INFO(
				"'%s':%d GPIO bit %d virq %d (%s,%s) - edge %s triggering\n",
				chip->name, chip->remote_pid, i,
				chip->irq_base + i,
				edge_name_rising[irq_rising],
				edge_name_falling[irq_falling],
				edge_names[edge]);
			(void)generic_handle_irq(chip->irq_base + i);
		}

		cur_val >>= 1;
		prev_val >>= 1;
	}
}

/**
 * Adds an interrupt domain based upon the DT node.
 *
 * @chip: pointer to GPIO chip
 * @node: pointer to Device Tree node
 */
static void smp2p_add_irq_domain(struct smp2p_chip_dev *chip,
	struct device_node *node)
{
	int ret;
	int irq_base;

	/* map GPIO pins to interrupts */
	chip->irq_domain = irq_domain_add_linear(node, 0,
			&smp2p_irq_domain_ops, chip);
	if (!chip->irq_domain) {
		SMP2P_ERR("%s: unable to create interrupt domain '%s':%d\n",
				__func__, chip->name, chip->remote_pid);
		goto domain_fail;
	}

	/* alloc a contiguous set of virt irqs from anywhere in the irq space */
	irq_base = irq_alloc_descs_from(0, SMP2P_BITS_PER_ENTRY,
				of_node_to_nid(chip->irq_domain->of_node));
	if (irq_base < 0) {
		SMP2P_ERR("alloc virt irqs failed:%d name:%s pid%d\n", irq_base,
						chip->name, chip->remote_pid);
		goto irq_alloc_fail;
	}

	/* map the allocated irqs to gpios */
	ret = irq_domain_associate_many(chip->irq_domain, irq_base, 0,
							SMP2P_BITS_PER_ENTRY);
	if (ret < 0) {
		SMP2P_ERR("map virt irqs failed:%d name:%s pid:%d\n", ret,
						chip->name, chip->remote_pid);
		goto irq_map_fail;
	}

	chip->irq_base = irq_base;
	SMP2P_DBG("create mapping:%d naem:%s pid:%d\n", chip->irq_base,
						chip->name, chip->remote_pid);
	return;

irq_map_fail:
	irq_free_descs(irq_base, SMP2P_BITS_PER_ENTRY);
irq_alloc_fail:
	irq_domain_remove(chip->irq_domain);
domain_fail:
	return;
}

/**
 * Notifier function passed into smp2p API for out bound entries.
 *
 * @self:       Pointer to calling notifier block
 * @event:	    Event
 * @data:       Event-specific data
 * @returns:    0
 */
static int smp2p_gpio_out_notify(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct smp2p_chip_dev *chip;

	chip = container_of(self, struct smp2p_chip_dev, out_notifier);

	switch (event) {
	case SMP2P_OPEN:
		chip->is_open = 1;
		SMP2P_GPIO("%s: Opened out '%s':%d in_shadow[%d]\n", __func__,
				chip->name, chip->remote_pid, chip->in_shadow);
		if (chip->in_shadow)
			schedule_work(&chip->shadow_work);
		break;
	case SMP2P_ENTRY_UPDATE:
		break;
	default:
		SMP2P_ERR("%s: Unknown event\n", __func__);
		break;
	}
	return 0;
}

/**
 * Notifier function passed into smp2p API for in bound entries.
 *
 * @self:       Pointer to calling notifier block
 * @event:	    Event
 * @data:       Event-specific data
 * @returns:    0
 */
static int smp2p_gpio_in_notify(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct smp2p_chip_dev *chip;

	chip = container_of(self, struct smp2p_chip_dev, in_notifier);

	switch (event) {
	case SMP2P_OPEN:
		chip->is_open = 1;
		SMP2P_GPIO("%s: Opened in '%s':%d\n", __func__,
				chip->name, chip->remote_pid);
		break;
	case SMP2P_ENTRY_UPDATE:
		msm_summary_irq_handler(chip, data);
		break;
	default:
		SMP2P_ERR("%s: Unknown event\n", __func__);
		break;
	}
	return 0;
}

/**
 * smp2p_gpio_shadow_worker - Handles shadow updates of an entry.
 *
 * @work: Work Item scheduled to handle the shadow updates.
 */
static void smp2p_gpio_shadow_worker(struct work_struct *work)
{
	struct smp2p_chip_dev *chip;
	int ret;
	unsigned long flags;

	chip = container_of(work, struct smp2p_chip_dev, shadow_work);
	spin_lock_irqsave(&chip->shadow_lock, flags);
	if (chip->in_shadow) {
		ret = msm_smp2p_out_modify(chip->out_handle,
					chip->shadow_value, 0x0, true);

		if (ret)
			SMP2P_GPIO("'%s':%d shadow val[0x%x] failed(%d)\n",
					chip->name, chip->remote_pid,
					chip->shadow_value, ret);
		else
			SMP2P_GPIO("'%s':%d shadow val[0x%x]\n",
					chip->name, chip->remote_pid,
					chip->shadow_value);
		chip->shadow_value = 0;
		chip->in_shadow = false;
	}
	spin_unlock_irqrestore(&chip->shadow_lock, flags);
}

/**
 * Device tree probe function.
 *
 * @pdev:	 Pointer to device tree data.
 * @returns: 0 on success; -ENODEV otherwise
 *
 * Called for each smp2pgpio entry in the device tree.
 */
static int smp2p_gpio_probe(struct platform_device *pdev)
{
	struct device_node *node;
	char *key;
	struct smp2p_chip_dev *chip;
	const char *name_tmp;
	unsigned long flags;
	bool is_test_entry = false;
	int ret;

	chip = kzalloc(sizeof(struct smp2p_chip_dev), GFP_KERNEL);
	if (!chip) {
		SMP2P_ERR("%s: out of memory\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}
	spin_lock_init(&chip->irq_lock);
	spin_lock_init(&chip->shadow_lock);
	INIT_WORK(&chip->shadow_work, smp2p_gpio_shadow_worker);

	/* parse device tree */
	node = pdev->dev.of_node;
	key = "qcom,entry-name";
	ret = of_property_read_string(node, key, &name_tmp);
	if (ret) {
		SMP2P_ERR("%s: missing DT key '%s'\n", __func__, key);
		goto fail;
	}
	strlcpy(chip->name, name_tmp, sizeof(chip->name));

	key = "qcom,remote-pid";
	ret = of_property_read_u32(node, key, &chip->remote_pid);
	if (ret) {
		SMP2P_ERR("%s: missing DT key '%s'\n", __func__, key);
		goto fail;
	}

	key = "qcom,is-inbound";
	chip->is_inbound = of_property_read_bool(node, key);

	/* create virtual GPIO controller */
	chip->gpio.label = chip->name;
	chip->gpio.dev = &pdev->dev;
	chip->gpio.owner = THIS_MODULE;
	chip->gpio.direction_input	= smp2p_direction_input,
	chip->gpio.get = smp2p_get_value;
	chip->gpio.direction_output = smp2p_direction_output,
	chip->gpio.set = smp2p_set_value;
	chip->gpio.to_irq = smp2p_gpio_to_irq,
	chip->gpio.base = -1;	/* use dynamic GPIO pin allocation */
	chip->gpio.ngpio = SMP2P_BITS_PER_ENTRY;
	ret = gpiochip_add(&chip->gpio);
	if (ret) {
		SMP2P_ERR("%s: unable to register GPIO '%s' ret %d\n",
				__func__, chip->name, ret);
		goto fail;
	}

	/*
	 * Test entries opened by GPIO Test conflict with loopback
	 * support, so the test entries must be explicitly opened
	 * in the unit test framework.
	 */
	if (strncmp("smp2p", chip->name, SMP2P_MAX_ENTRY_NAME) == 0)
		is_test_entry = true;

	if (!chip->is_inbound)	{
		chip->out_notifier.notifier_call = smp2p_gpio_out_notify;
		if (!is_test_entry) {
			ret = msm_smp2p_out_open(chip->remote_pid, chip->name,
					   &chip->out_notifier,
					   &chip->out_handle);
			if (ret < 0)
				goto error;
		}
	} else {
		chip->in_notifier.notifier_call = smp2p_gpio_in_notify;
		if (!is_test_entry) {
			ret = msm_smp2p_in_register(chip->remote_pid,
					chip->name,
					&chip->in_notifier);
			if (ret < 0)
				goto error;
		}
	}

	spin_lock_irqsave(&smp2p_entry_lock_lha1, flags);
	list_add(&chip->entry_list, &smp2p_entry_list);
	spin_unlock_irqrestore(&smp2p_entry_lock_lha1, flags);

	/*
	 * Create interrupt domain - note that chip can't be removed from the
	 * interrupt domain, so chip cannot be deleted after this point.
	 */
	if (chip->is_inbound)
		smp2p_add_irq_domain(chip, node);
	else
		chip->irq_base = -1;

	SMP2P_GPIO("%s: added %s%s entry '%s':%d gpio %d irq %d",
			__func__,
			is_test_entry ? "test " : "",
			chip->is_inbound ? "in" : "out",
			chip->name, chip->remote_pid,
			chip->gpio.base, chip->irq_base);

	return 0;
error:
	if (gpiochip_remove(&chip->gpio))
		SMP2P_ERR("%s: unable to Remove GPIO '%s'\n",
				__func__, chip->name);

fail:
	kfree(chip);
	return ret;
}

/**
 * smp2p_gpio_open_close - Opens or closes entry.
 *
 * @entry:   Entry to open or close
 * @do_open: true = open port; false = close
 */
static void smp2p_gpio_open_close(struct smp2p_chip_dev *entry,
	bool do_open)
{
	int ret;

	if (do_open) {
		/* open entry */
		if (entry->is_inbound)
			ret = msm_smp2p_in_register(entry->remote_pid,
					entry->name, &entry->in_notifier);
		else
			ret = msm_smp2p_out_open(entry->remote_pid,
					entry->name, &entry->out_notifier,
					&entry->out_handle);
		SMP2P_GPIO("%s: opened %s '%s':%d ret %d\n",
				__func__,
				entry->is_inbound ? "in" : "out",
				entry->name, entry->remote_pid,
				ret);
	} else {
		/* close entry */
		if (entry->is_inbound)
			ret = msm_smp2p_in_unregister(entry->remote_pid,
					entry->name, &entry->in_notifier);
		else
			ret = msm_smp2p_out_close(&entry->out_handle);
		entry->is_open = false;
		SMP2P_GPIO("%s: closed %s '%s':%d ret %d\n",
				__func__,
				entry->is_inbound ? "in" : "out",
				entry->name, entry->remote_pid, ret);
	}
}

/**
 * smp2p_gpio_open_test_entry - Opens or closes test entries for unit testing.
 *
 * @name:       Name of the entry
 * @remote_pid: Remote processor ID
 * @do_open:    true = open port; false = close
 */
void smp2p_gpio_open_test_entry(const char *name, int remote_pid, bool do_open)
{
	struct smp2p_chip_dev *entry;
	struct smp2p_chip_dev *start_entry;
	unsigned long flags;

	spin_lock_irqsave(&smp2p_entry_lock_lha1, flags);
	if (list_empty(&smp2p_entry_list)) {
		spin_unlock_irqrestore(&smp2p_entry_lock_lha1, flags);
		return;
	}
	start_entry = list_first_entry(&smp2p_entry_list,
					struct smp2p_chip_dev,
					entry_list);
	entry = start_entry;
	do {
		if (!strncmp(entry->name, name, SMP2P_MAX_ENTRY_NAME)
				&& entry->remote_pid == remote_pid) {
			/* found entry to change */
			spin_unlock_irqrestore(&smp2p_entry_lock_lha1, flags);
			smp2p_gpio_open_close(entry, do_open);
			spin_lock_irqsave(&smp2p_entry_lock_lha1, flags);
		}
		list_rotate_left(&smp2p_entry_list);
		entry = list_first_entry(&smp2p_entry_list,
						struct smp2p_chip_dev,
						entry_list);
	} while (entry != start_entry);
	spin_unlock_irqrestore(&smp2p_entry_lock_lha1, flags);
}

static struct of_device_id msm_smp2p_match_table[] = {
	{.compatible = "qcom,smp2pgpio", },
	{},
};

static struct platform_driver smp2p_gpio_driver = {
	.probe = smp2p_gpio_probe,
	.driver = {
		.name = "smp2pgpio",
		.owner = THIS_MODULE,
		.of_match_table = msm_smp2p_match_table,
	},
};

static int smp2p_init(void)
{
	INIT_LIST_HEAD(&smp2p_entry_list);
	return platform_driver_register(&smp2p_gpio_driver);
}
module_init(smp2p_init);

static void __exit smp2p_exit(void)
{
	platform_driver_unregister(&smp2p_gpio_driver);
}
module_exit(smp2p_exit);

MODULE_DESCRIPTION("SMP2P GPIO");
MODULE_LICENSE("GPL v2");
