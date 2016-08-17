/*
 * Copyright (c) 2011 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 *
 * This driver adds support for generic RMI4 devices from Synpatics. It
 * implements the mandatory f01 RMI register and depends on the presence of
 * other required RMI functions.
 *
 * The RMI4 specification can be found here (URL split after files/ for
 * style reasons):
 * http://www.synaptics.com/sites/default/files/
 *           511-000136-01-Rev-E-RMI4%20Intrfacing%20Guide.pdf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/rmi.h>
#include "rmi_driver.h"

#define DELAY_DEBUG 0
#define REGISTER_DEBUG 0

#define PDT_END_SCAN_LOCATION	0x0005
#define PDT_PROPERTIES_LOCATION 0x00EF
#define BSR_LOCATION 0x00FE
#define HAS_BSR_MASK 0x20
#define HAS_NONSTANDARD_PDT_MASK 0x40
#define RMI4_END_OF_PDT(id) ((id) == 0x00 || (id) == 0xff)
#define RMI4_MAX_PAGE 0xff
#define RMI4_PAGE_SIZE 0x100

#define RMI_DEVICE_RESET_CMD	0x01
#define INITIAL_RESET_WAIT_MS	20

/* sysfs files for attributes for driver values. */
static ssize_t rmi_driver_hasbsr_show(struct device *dev,
				      struct device_attribute *attr, char *buf);

static ssize_t rmi_driver_bsr_show(struct device *dev,
				   struct device_attribute *attr, char *buf);

static ssize_t rmi_driver_bsr_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count);

static ssize_t rmi_driver_enabled_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);

static ssize_t rmi_driver_enabled_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

static ssize_t rmi_driver_phys_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);

static ssize_t rmi_driver_version_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);

#if REGISTER_DEBUG
static ssize_t rmi_driver_reg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
#endif

#if DELAY_DEBUG
static ssize_t rmi_delay_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);

static ssize_t rmi_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
#endif

static struct device_attribute attrs[] = {
	__ATTR(hasbsr, RMI_RO_ATTR,
	       rmi_driver_hasbsr_show, rmi_store_error),
	__ATTR(bsr, RMI_RO_ATTR,
	       rmi_driver_bsr_show, rmi_driver_bsr_store),
	__ATTR(enabled, RMI_RO_ATTR,
	       rmi_driver_enabled_show, rmi_driver_enabled_store),
	__ATTR(phys, RMI_RO_ATTR,
	       rmi_driver_phys_show, rmi_store_error),
#if REGISTER_DEBUG
	__ATTR(reg, RMI_WO_ATTR,
	       rmi_show_error, rmi_driver_reg_store),
#endif
#if DELAY_DEBUG
	__ATTR(delay, RMI_RW_ATTR,
	       rmi_delay_show, rmi_delay_store),
#endif
	__ATTR(version, RMI_RO_ATTR,
	       rmi_driver_version_show, rmi_store_error),
};


/*
** ONLY needed for POLLING mode of the driver
*/
struct rmi_device *polled_synaptics_rmi_device = NULL;
EXPORT_SYMBOL(polled_synaptics_rmi_device);

/* Useful helper functions for u8* */

void u8_set_bit(u8 *target, int pos)
{
	target[pos/8] |= 1<<pos%8;
}

void u8_clear_bit(u8 *target, int pos)
{
	target[pos/8] &= ~(1<<pos%8);
}

bool u8_is_set(u8 *target, int pos)
{
	return target[pos/8] & 1<<pos%8;
}

bool u8_is_any_set(u8 *target, int size)
{
	int i;
	for (i = 0; i < size; i++) {
		if (target[i])
			return true;
	}
	return false;
}

void u8_or(u8 *dest, u8 *target1, u8 *target2, int size)
{
	int i;
	for (i = 0; i < size; i++)
		dest[i] = target1[i] | target2[i];
}

void u8_and(u8 *dest, u8 *target1, u8 *target2, int size)
{
	int i;
	for (i = 0; i < size; i++)
		dest[i] = target1[i] & target2[i];
}

/* Helper fn to convert a byte array representing a short in the RMI
 * endian-ness to a short in the native processor's specific endianness.
 * We don't use ntohs/htons here because, well, we're not dealing with
 * a pair of shorts. And casting dest to short* wouldn't work, because
 * that would imply knowing the byte order of short in the first place.
 */
void batohs(unsigned short *dest, unsigned char *src)
{
	*dest = src[1] * 0x100 + src[0];
}

/* Helper function to convert a short (in host processor endianess) to
 * a byte array in the RMI endianess for shorts.  See above comment for
 * why we dont us htons or something like that.
 */
void hstoba(unsigned char *dest, unsigned short src)
{
	dest[0] = src % 0x100;
	dest[1] = src / 0x100;
}

static bool has_bsr(struct rmi_driver_data *data)
{
	return (data->pdt_props & HAS_BSR_MASK) != 0;
}

/* Utility routine to set bits in a register. */
int rmi_set_bits(struct rmi_device *rmi_dev, unsigned short address,
		 unsigned char bits)
{
	unsigned char reg_contents;
	int retval;

	retval = rmi_read_block(rmi_dev, address, &reg_contents, 1);
	if (retval)
		return retval;
	reg_contents = reg_contents | bits;
	retval = rmi_write_block(rmi_dev, address, &reg_contents, 1);
	if (retval == 1)
		return 0;
	else if (retval == 0)
		return -EIO;
	return retval;
}
EXPORT_SYMBOL(rmi_set_bits);

/* Utility routine to clear bits in a register. */
int rmi_clear_bits(struct rmi_device *rmi_dev, unsigned short address,
		   unsigned char bits)
{
	unsigned char reg_contents;
	int retval;

	retval = rmi_read_block(rmi_dev, address, &reg_contents, 1);
	if (retval)
		return retval;
	reg_contents = reg_contents & ~bits;
	retval = rmi_write_block(rmi_dev, address, &reg_contents, 1);
	if (retval == 1)
		return 0;
	else if (retval == 0)
		return -EIO;
	return retval;
}
EXPORT_SYMBOL(rmi_clear_bits);

static void rmi_free_function_list(struct rmi_device *rmi_dev)
{
	struct rmi_function_container *entry, *n;
	struct rmi_driver_data *data = rmi_get_driverdata(rmi_dev);

	list_for_each_entry_safe(entry, n, &data->rmi_functions.list, list) {
		kfree(entry->irq_mask);
		list_del(&entry->list);
	}
}

static int init_one_function(struct rmi_device *rmi_dev,
			     struct rmi_function_container *fc)
{
	int retval;

	dev_info(&rmi_dev->dev, "Initializing F%02X.\n",
			fc->fd.function_number);
	dev_dbg(&rmi_dev->dev, "Initializing F%02X.\n",
			fc->fd.function_number);

	if (!fc->fh) {
		struct rmi_function_handler *fh =
			rmi_get_function_handler(fc->fd.function_number);
		if (!fh) {
			dev_dbg(&rmi_dev->dev, "No handler for F%02X.\n",
				fc->fd.function_number);
			return 0;
		}
		fc->fh = fh;
	}

	if (!fc->fh->init)
		return 0;

	dev_set_name(&(fc->dev), "fn%02x", fc->fd.function_number);

	fc->dev.parent = &rmi_dev->dev;

	retval = device_register(&fc->dev);
	if (retval) {
		dev_err(&rmi_dev->dev, "Failed device_register for F%02X.\n",
			fc->fd.function_number);
		return retval;
	}

	retval = fc->fh->init(fc);
	if (retval < 0) {
		dev_err(&rmi_dev->dev, "Failed to initialize function F%02x\n",
			fc->fd.function_number);
		goto error_exit;
	}

	return 0;

error_exit:
	device_unregister(&fc->dev);
	return retval;
}

static void rmi_driver_fh_add(struct rmi_device *rmi_dev,
			      struct rmi_function_handler *fh)
{
	struct rmi_function_container *entry;
	struct rmi_driver_data *data = rmi_get_driverdata(rmi_dev);

	if (fh->func == 0x01)
		data->f01_container->fh = fh;
	else {
		list_for_each_entry(entry, &data->rmi_functions.list, list)
			if (entry->fd.function_number == fh->func) {
				entry->fh = fh;
				init_one_function(rmi_dev, entry);
			}
	}

}

static void rmi_driver_fh_remove(struct rmi_device *rmi_dev,
				 struct rmi_function_handler *fh)
{
	struct rmi_function_container *entry;
	struct rmi_driver_data *data = rmi_get_driverdata(rmi_dev);

	list_for_each_entry(entry, &data->rmi_functions.list, list)
		if (entry->fd.function_number == fh->func) {
			if (fh->remove)
				fh->remove(entry);

			entry->fh = NULL;
		}
}

static void construct_mask(u8 *mask, int num, int pos)
{
	int i;

	for (i = 0; i < num; i++)
		u8_set_bit(mask, pos+i);
}

static int process_interrupt_requests(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = rmi_get_driverdata(rmi_dev);
	struct device *dev = &rmi_dev->dev;
	struct rmi_function_container *entry;
	u8 irq_status[data->num_of_irq_regs];
	u8 irq_bits[data->num_of_irq_regs];
	int error;

	error = rmi_read_block(rmi_dev,
				data->f01_container->fd.data_base_addr + 1,
				irq_status, data->num_of_irq_regs);
	if (error < 0) {
		dev_err(dev, "%s: failed to read irqs.", __func__);
		return error;
	}
	/* Device control (F01) is handled before anything else. */
	u8_and(irq_bits, irq_status, data->f01_container->irq_mask,
			data->num_of_irq_regs);
	if (u8_is_any_set(irq_bits, data->num_of_irq_regs))
		data->f01_container->fh->attention(
				data->f01_container, irq_bits);

	//dev_info(dev, "  irq_status = 0x%2x data->current_irq_mask = 0x%2x data->num_of_irq_regs = %d\n",
	//	 irq_status[0], data->current_irq_mask[0], data->num_of_irq_regs );


	u8_and(irq_status, irq_status, data->current_irq_mask,
	       data->num_of_irq_regs);

	/* At this point, irq_status has all bits that are set in the
	 * interrupt status register and are enabled.
	 */

	list_for_each_entry(entry, &data->rmi_functions.list, list){
		if (entry->irq_mask && entry->fh && entry->fh->attention) {

			u8_and(irq_bits, irq_status, entry->irq_mask,
			       data->num_of_irq_regs);
			if (u8_is_any_set(irq_bits, data->num_of_irq_regs)) {
				error = entry->fh->attention(entry, irq_bits);
				if (error < 0)
					dev_err(dev, "%s: f%.2x"
						" attention handler failed:"
						" %d\n", __func__,
						entry->fh->func, error);
			}
		}
	}
	return 0;
}


static int rmi_driver_irq_handler(struct rmi_device *rmi_dev, int irq)
{
	struct rmi_driver_data *data = rmi_get_driverdata(rmi_dev);

	/* Can get called before the driver is fully ready to deal with
	 * interrupts.
	 */
	if (!data || !data->f01_container || !data->f01_container->fh) {
		dev_warn(&rmi_dev->dev,
			 "Not ready to handle interrupts yet!\n");
		return 0;
	}

	return process_interrupt_requests(rmi_dev);
}

/*
 * Construct a function's IRQ mask. This should
 * be called once and stored.
 */
static u8 *rmi_driver_irq_get_mask(struct rmi_device *rmi_dev,
				   struct rmi_function_container *fc) {
	struct rmi_driver_data *data = rmi_get_driverdata(rmi_dev);

	/* TODO: Where should this be freed? */
	u8 *irq_mask = kzalloc(sizeof(u8) * data->num_of_irq_regs, GFP_KERNEL);
	if (irq_mask)
		construct_mask(irq_mask, fc->num_of_irqs, fc->irq_pos);

	return irq_mask;
}

/*
 * This pair of functions allows functions like function 54 to request to have
 * other interupts disabled until the restore function is called. Only one store
 * happens at a time.
 */
static int rmi_driver_irq_save(struct rmi_device *rmi_dev, u8 * new_ints)
{
	int retval = 0;
	struct rmi_driver_data *data = rmi_get_driverdata(rmi_dev);
	struct device *dev = &rmi_dev->dev;

	mutex_lock(&data->irq_mutex);
	if (!data->irq_stored) {
		/* Save current enabled interupts */
		retval = rmi_read_block(rmi_dev,
				data->f01_container->fd.control_base_addr+1,
				data->irq_mask_store, data->num_of_irq_regs);
		if (retval < 0) {
			dev_err(dev, "%s: Failed to read enabled interrupts!",
								__func__);
			goto error_unlock;
		}
		/*
		 * Disable every interupt except for function 54
		 * TODO:Will also want to not disable function 1-like functions.
		 * No need to take care of this now, since there's no good way
		 * to identify them.
		 */
		retval = rmi_write_block(rmi_dev,
				data->f01_container->fd.control_base_addr+1,
				new_ints, data->num_of_irq_regs);
		if (retval < 0) {
			dev_err(dev, "%s: Failed to change enabled interrupts!",
								__func__);
			goto error_unlock;
		}
		memcpy(data->current_irq_mask, new_ints,
					data->num_of_irq_regs * sizeof(u8));
		data->irq_stored = true;
	} else {
		retval = -ENOSPC; /* No space to store IRQs.*/
		dev_err(dev, "%s: Attempted to save values when"
						" already stored!", __func__);
	}

error_unlock:
	mutex_unlock(&data->irq_mutex);
	return retval;
}

static int rmi_driver_irq_restore(struct rmi_device *rmi_dev)
{
	int retval = 0;
	struct rmi_driver_data *data = rmi_get_driverdata(rmi_dev);
	struct device *dev = &rmi_dev->dev;
	mutex_lock(&data->irq_mutex);

	if (data->irq_stored) {
		retval = rmi_write_block(rmi_dev,
				data->f01_container->fd.control_base_addr+1,
				data->irq_mask_store, data->num_of_irq_regs);
		if (retval < 0) {
			dev_err(dev, "%s: Failed to write enabled interupts!",
								__func__);
			goto error_unlock;
		}
		memcpy(data->current_irq_mask, data->irq_mask_store,
					data->num_of_irq_regs * sizeof(u8));
		data->irq_stored = false;
	} else {
		retval = -EINVAL;
		dev_err(dev, "%s: Attempted to restore values when not stored!",
			__func__);
	}

error_unlock:
	mutex_unlock(&data->irq_mutex);
	return retval;
}

static int rmi_driver_fn_01_specific(struct rmi_device *rmi_dev,
				     struct pdt_entry *pdt_ptr,
				     int *current_irq_count,
				     u16 page_start)
{
	struct rmi_driver_data *data = NULL;
	struct rmi_function_container *fc = NULL;
	int retval = 0;
	struct device *dev = &rmi_dev->dev;
	struct rmi_function_handler *fh =
		rmi_get_function_handler(0x01);

	data = rmi_get_driverdata(rmi_dev);

	dev_info(dev, "%s: Found F01, initializing.\n", __func__);
	if (!fh)
		dev_dbg(dev, "%s: No function handler for F01?!", __func__);

	fc = kzalloc(sizeof(struct rmi_function_container), GFP_KERNEL);
	if (!fc) {
		retval = -ENOMEM;
		return retval;
	}

	copy_pdt_entry_to_fd(pdt_ptr, &fc->fd, page_start);
	fc->num_of_irqs = pdt_ptr->interrupt_source_count;
	fc->irq_pos = *current_irq_count;

	*current_irq_count += fc->num_of_irqs;

	fc->rmi_dev        = rmi_dev;
	fc->dev.parent     = &fc->rmi_dev->dev;
	fc->fh = fh;

	dev_set_name(&(fc->dev), "fn%02x", fc->fd.function_number);

	retval = device_register(&fc->dev);
	if (retval) {
		dev_err(dev, "%s: Failed device_register for F01.\n", __func__);
		goto error_free_data;
	}

	data->f01_container = fc;

	return retval;

error_free_data:
	kfree(fc);
	return retval;
}

/*
 * Scan the PDT for F01 so we can force a reset before anything else
 * is done.  This forces the sensor into a known state, and also
 * forces application of any pending updates from reflashing the
 * firmware or configuration.  We have to do this before actually
 * building the PDT because the reflash might cause various registers
 * to move around.
 */
static int do_initial_reset(struct rmi_device* rmi_dev)
{
	struct pdt_entry pdt_entry;
	int page;
	struct device *dev = &rmi_dev->dev;
	bool done = false;
	int i;
	int retval;

	pr_info("in function ____%s____  \n", __func__);

	polled_synaptics_rmi_device = rmi_dev;

	for (page = 0; (page <= RMI4_MAX_PAGE) && !done; page++) {

		u16 page_start = RMI4_PAGE_SIZE * page;
		u16 pdt_start = page_start + PDT_START_SCAN_LOCATION;
		u16 pdt_end = page_start + PDT_END_SCAN_LOCATION;
		pr_info("               reading page = %d\n", page );
		done = true;
		for (i = pdt_start; i >= pdt_end; i -= sizeof(pdt_entry)) {

		        pr_info("               reading PDT entry %3d (block %3d)\n",
				i%sizeof(pdt_entry), i);

		        retval = rmi_read_block(rmi_dev, i, (u8 *)&pdt_entry,
					       sizeof(pdt_entry));
			if (retval != sizeof(pdt_entry)) {
				dev_err(dev, "Read PDT entry at 0x%04x"
					"failed, code = %d.\n", i, retval);
				return retval;
			}

			if (RMI4_END_OF_PDT(pdt_entry.function_number))
				break;
			done = false;

			if (pdt_entry.function_number == 0x01) {
				u16 cmd_addr = page_start +
					pdt_entry.command_base_addr;
				u8 cmd_buf = RMI_DEVICE_RESET_CMD;
				retval = rmi_write_block(rmi_dev, cmd_addr,
						&cmd_buf, 1);
				if (retval < 0) {
					dev_err(dev, "Initial reset failed. "
						"Code = %d.\n", retval);
					return retval;
				}
				mdelay(INITIAL_RESET_WAIT_MS);
				return 0;
			}
		}
	}

	dev_warn(dev, "WARNING: Failed to find F01 for initial reset.\n");
	return -ENODEV;
}


static int rmi_scan_pdt(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data;
	struct rmi_function_container *fc;
	struct pdt_entry pdt_entry;
	int page;
	struct device *dev = &rmi_dev->dev;
	int irq_count = 0;
	bool done = false;
	int i;
	int retval;
        pr_info("in function ____%s____  \n", __func__);
        pr_info("   doing initial reset  \n");

	retval = do_initial_reset(rmi_dev);
        pr_info("   back in %s  \n", __func__);

	if (retval)
		dev_err(dev, "WARNING: Initial reset failed! Soldiering on.\n");

	data = rmi_get_driverdata(rmi_dev);

	INIT_LIST_HEAD(&data->rmi_functions.list);

	/* parse the PDT */
	for (page = 0; (page <= RMI4_MAX_PAGE) && !done; page++) {
		u16 page_start = RMI4_PAGE_SIZE * page;
		u16 pdt_start = page_start + PDT_START_SCAN_LOCATION;
		u16 pdt_end = page_start + PDT_END_SCAN_LOCATION;

		done = true;
		for (i = pdt_start; i >= pdt_end; i -= sizeof(pdt_entry)) {
			retval = rmi_read_block(rmi_dev, i, (u8 *)&pdt_entry,
					       sizeof(pdt_entry));
			if (retval != sizeof(pdt_entry)) {
				dev_err(dev, "Read PDT entry at 0x%04x"
					"failed.\n", i);
				goto error_exit;
			}

			if (RMI4_END_OF_PDT(pdt_entry.function_number))
				break;

			dev_dbg(dev, "%s: Found F%.2X on page 0x%02X\n",
				__func__, pdt_entry.function_number, page);
			done = false;

			/*
			 * F01 is handled by rmi_driver. Hopefully we will get
			 * rid of the special treatment of f01 at some point
			 * in time.
			 */
			if (pdt_entry.function_number == 0x01) {
				retval = rmi_driver_fn_01_specific(rmi_dev,
						&pdt_entry, &irq_count,
						page_start);
				if (retval)
					goto error_exit;
				continue;
			}

			fc = kzalloc(sizeof(struct rmi_function_container),
				     GFP_KERNEL);
			if (!fc) {
				dev_err(dev, "Failed to allocate function "
					"container for F%02X.\n",
					pdt_entry.function_number);
				retval = -ENOMEM;
				goto error_exit;
			}

			copy_pdt_entry_to_fd(&pdt_entry, &fc->fd, page_start);

			fc->rmi_dev = rmi_dev;
			fc->num_of_irqs = pdt_entry.interrupt_source_count;
			fc->irq_pos = irq_count;
			irq_count += fc->num_of_irqs;

			retval = init_one_function(rmi_dev, fc);
			if (retval < 0) {
				dev_err(dev, "Failed to initialize F%.2x\n",
					pdt_entry.function_number);
				kfree(fc);
				goto error_exit;
			}

			list_add_tail(&fc->list, &data->rmi_functions.list);
		}
	}
	data->num_of_irq_regs = (irq_count + 7) / 8;
	dev_dbg(dev, "%s: Done with PDT scan.\n", __func__);
	return 0;

error_exit:
	return retval;
}


#ifdef SYNAPTICS_SENSOR_POLL
void synaptics_sensor_poller(unsigned long data){
  pr_info("in function ____%s____ ,  rmi_device= 0x%8x \n", __func__, polled_synaptics_rmi_device);
  // msleep(10000);
  for (;;) {
    msleep(100);
    rmi_driver_irq_handler(polled_synaptics_rmi_device, 0);
  }

  return;
}

struct workqueue_struct *synaptics_rmi_polling_queue = NULL;
struct delayed_work synaptics_rmi_polling_work;

#endif


static int rmi_driver_probe(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data;
	struct rmi_function_container *fc;
	struct rmi_device_platform_data *pdata;
	int error = 0;
	struct device *dev = &rmi_dev->dev;
	int attr_count = 0;

	dev_dbg(dev, "%s: Starting probe.\n", __func__);

	pdata = to_rmi_platform_data(rmi_dev);

	data = kzalloc(sizeof(struct rmi_driver_data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "%s: Failed to allocate driver data.\n", __func__);
		return -ENOMEM;
	}

	rmi_set_driverdata(rmi_dev, data);

	error = rmi_scan_pdt(rmi_dev);
	if (error) {
		dev_err(dev, "PDT scan failed with code %d.\n", error);
		goto err_free_data;
	}

	if (!data->f01_container) {
		dev_err(dev, "missing f01 function!\n");
		error = -EINVAL;
		goto err_free_data;
	}

	data->f01_container->irq_mask = kzalloc(
			sizeof(u8) * data->num_of_irq_regs, GFP_KERNEL);
	if (!data->f01_container->irq_mask) {
		dev_err(dev, "Failed to allocate F01 IRQ mask.\n");
		error = -ENOMEM;
		goto err_free_data;
	}
	construct_mask(data->f01_container->irq_mask,
		       data->f01_container->num_of_irqs,
		       data->f01_container->irq_pos);
	list_for_each_entry(fc, &data->rmi_functions.list, list)
		fc->irq_mask = rmi_driver_irq_get_mask(rmi_dev, fc);

	error = rmi_driver_f01_init(rmi_dev);
	if (error < 0) {
		dev_err(dev, "Failed to initialize F01.\n");
		goto err_free_data;
	}

	error = rmi_read(rmi_dev, PDT_PROPERTIES_LOCATION,
			 (char *) &data->pdt_props);
	if (error < 0) {
		/* we'll print out a warning and continue since
		 * failure to get the PDT properties is not a cause to fail
		 */
		dev_warn(dev, "Could not read PDT properties from 0x%04x. "
			 "Assuming 0x00.\n", PDT_PROPERTIES_LOCATION);
	}

	dev_dbg(dev, "%s: Creating sysfs files.", __func__);
	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		error = device_create_file(dev, &attrs[attr_count]);
		if (error < 0) {
			dev_err(dev, "%s: Failed to create sysfs file %s.\n",
				__func__, attrs[attr_count].attr.name);
			goto err_free_data;
		}
	}

	__mutex_init(&data->irq_mutex, "irq_mutex", &data->irq_key);
	data->current_irq_mask = kzalloc(sizeof(u8) * data->num_of_irq_regs,
					 GFP_KERNEL);
	if (!data->current_irq_mask) {
		dev_err(dev, "Failed to allocate current_irq_mask.\n");
		error = -ENOMEM;
		goto err_free_data;
	}
	error = rmi_read_block(rmi_dev,
				data->f01_container->fd.control_base_addr+1,
				data->current_irq_mask, data->num_of_irq_regs);
	if (error < 0) {
		dev_err(dev, "%s: Failed to read current IRQ mask.\n",
			__func__);
		goto err_free_data;
	}
	data->irq_mask_store = kzalloc(sizeof(u8) * data->num_of_irq_regs,
				       GFP_KERNEL);
	if (!data->irq_mask_store) {
		dev_err(dev, "Failed to allocate mask store.\n");
		error = -ENOMEM;
		goto err_free_data;
	}

#if defined(CONFIG_RMI4_DEV)
	if (rmi_char_dev_register(rmi_dev->phys))
		pr_err("%s: error register char device", __func__);
#endif /*CONFIG_RMI4_DEV*/

#ifdef	CONFIG_PM
	data->pm_data = pdata->pm_data;
	data->pre_suspend = pdata->pre_suspend;
	data->post_resume = pdata->post_resume;

	mutex_init(&data->suspend_mutex);

#endif /* CONFIG_PM */
	data->enabled = true;

	dev_info(dev, "connected RMI device manufacturer: %s product: %s\n",
		 data->manufacturer_id == 1 ? "synaptics" : "unknown",
		 data->product_id);

#ifdef SYNAPTICS_SENSOR_POLL
	synaptics_rmi_polling_queue = create_singlethread_workqueue("rmi_poll_work");
	INIT_DELAYED_WORK_DEFERRABLE(&synaptics_rmi_polling_work, synaptics_sensor_poller);
	pr_info("%s: setting up POLLING mode, rmi_device= 0x%8x \n", __func__, polled_synaptics_rmi_device);
	queue_delayed_work(synaptics_rmi_polling_queue, &synaptics_rmi_polling_work, 1000);
#endif
	return 0;

 err_free_data:
	rmi_free_function_list(rmi_dev);
	for (attr_count--; attr_count >= 0; attr_count--)
		device_remove_file(dev, &attrs[attr_count]);
	kfree(data->f01_container->irq_mask);
	kfree(data->irq_mask_store);
	kfree(data->current_irq_mask);
	kfree(data);
	return error;
}

#ifdef CONFIG_PM
static int rmi_driver_suspend(struct device *dev)
{
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;
	struct rmi_function_container *entry;
	int retval = 0;

	rmi_dev = to_rmi_device(dev);
	data = rmi_get_driverdata(rmi_dev);

	mutex_lock(&data->suspend_mutex);
	if (data->suspended)
		goto exit;

	if (data->pre_suspend) {
		retval = data->pre_suspend(data->pm_data);
		if (retval)
			goto exit;
	}

	list_for_each_entry(entry, &data->rmi_functions.list, list)
		if (entry->fh && entry->fh->suspend) {
			retval = entry->fh->suspend(entry);
			if (retval < 0)
				goto exit;
		}

	if (data->f01_container && data->f01_container->fh
				&& data->f01_container->fh->suspend) {
		retval = data->f01_container->fh->suspend(data->f01_container);
		if (retval < 0)
			goto exit;
	}
	data->suspended = true;

exit:
	mutex_unlock(&data->suspend_mutex);
	return retval;
}

static int rmi_driver_resume(struct device *dev)
{
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;
	struct rmi_function_container *entry;
	int retval = 0;

	rmi_dev = to_rmi_device(dev);
	data = rmi_get_driverdata(rmi_dev);

	mutex_lock(&data->suspend_mutex);
	if (!data->suspended)
		goto exit;

	if (data->f01_container && data->f01_container->fh
				&& data->f01_container->fh->resume) {
		retval = data->f01_container->fh->resume(data->f01_container);
		if (retval < 0)
			goto exit;
	}

	list_for_each_entry(entry, &data->rmi_functions.list, list)
		if (entry->fh && entry->fh->resume) {
			retval = entry->fh->resume(entry);
			if (retval < 0)
				goto exit;
		}

	if (data->post_resume) {
		retval = data->post_resume(data->pm_data);
		if (retval)
			goto exit;
	}

	data->suspended = false;

exit:
	mutex_unlock(&data->suspend_mutex);
	return retval;
}

#endif /* CONFIG_PM */

static int __devexit rmi_driver_remove(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = rmi_get_driverdata(rmi_dev);
	struct rmi_function_container *entry;
	int i;

	list_for_each_entry(entry, &data->rmi_functions.list, list)
		if (entry->fh && entry->fh->remove)
			entry->fh->remove(entry);

	rmi_free_function_list(rmi_dev);
	for (i = 0; i < ARRAY_SIZE(attrs); i++)
		device_remove_file(&rmi_dev->dev, &attrs[i]);
	kfree(data->f01_container->irq_mask);
	kfree(data->irq_mask_store);
	kfree(data->current_irq_mask);
	kfree(data);

	return 0;
}

#ifdef UNIVERSAL_DEV_PM_OPS
static UNIVERSAL_DEV_PM_OPS(rmi_driver_pm, rmi_driver_suspend,
			    rmi_driver_resume, NULL);
#endif

static struct rmi_driver sensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		// .name = "rmi-generic",
		.name = "rmi_generic",
#ifdef UNIVERSAL_DEV_PM_OPS
		.pm = &rmi_driver_pm,
#endif
	},
	.probe = rmi_driver_probe,
	.irq_handler = rmi_driver_irq_handler,
	.fh_add = rmi_driver_fh_add,
	.fh_remove = rmi_driver_fh_remove,
	.get_func_irq_mask = rmi_driver_irq_get_mask,
	.store_irq_mask = rmi_driver_irq_save,
	.restore_irq_mask = rmi_driver_irq_restore,
	.remove = __devexit_p(rmi_driver_remove)
};

/* Utility routine to handle writes to read-only attributes.  Hopefully
 * this will never happen, but if the user does something stupid, we don't
 * want to accept it quietly (which is what can happen if you just put NULL
 * for the attribute's store function).
 */
ssize_t rmi_store_error(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	dev_warn(dev,
		 "RMI4 WARNING: Attempt to write %d characters to read-only "
		 "attribute %s.", count, attr->attr.name);
	return -EPERM;
}

/* Utility routine to handle reads of write-only attributes.  Hopefully
 * this will never happen, but if the user does something stupid, we don't
 * want to accept it quietly (which is what can happen if you just put NULL
 * for the attribute's show function).
 */
ssize_t rmi_show_error(struct device *dev,
		       struct device_attribute *attr,
		       char *buf)
{
	dev_warn(dev,
		 "RMI4 WARNING: Attempt to read from write-only attribute %s.",
		 attr->attr.name);
	return -EPERM;
}

/* sysfs show and store fns for driver attributes */
static ssize_t rmi_driver_hasbsr_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;
	rmi_dev = to_rmi_device(dev);
	data = rmi_get_driverdata(rmi_dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", has_bsr(data));
}

static ssize_t rmi_driver_bsr_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;
	rmi_dev = to_rmi_device(dev);
	data = rmi_get_driverdata(rmi_dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->bsr);
}

static ssize_t rmi_driver_bsr_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int retval;
	unsigned long val;
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;

	rmi_dev = to_rmi_device(dev);
	data = rmi_get_driverdata(rmi_dev);

	/* need to convert the string data to an actual value */
	retval = strict_strtoul(buf, 10, &val);
	if (retval < 0) {
		dev_err(dev, "Invalid value '%s' written to BSR.\n", buf);
		return -EINVAL;
	}

	retval = rmi_write(rmi_dev, BSR_LOCATION, (unsigned char)val);
	if (retval) {
		dev_err(dev, "%s : failed to write bsr %u to 0x%x\n",
			__func__, (unsigned int)val, BSR_LOCATION);
		return retval;
	}

	data->bsr = val;

	return count;
}

static void disable_sensor(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = rmi_get_driverdata(rmi_dev);

	rmi_dev->phys->disable_device(rmi_dev->phys);

	data->enabled = false;
}

static int enable_sensor(struct rmi_device *rmi_dev)
{
	struct rmi_driver_data *data = rmi_get_driverdata(rmi_dev);
	int retval = 0;
	pr_info("in function ____%s____  \n", __func__);
	retval = rmi_dev->phys->enable_device(rmi_dev->phys);
	/* non-zero means error occurred */
	if (retval)
		return retval;

	data->enabled = true;

	return 0;
}

static ssize_t rmi_driver_enabled_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;

	rmi_dev = to_rmi_device(dev);
	data = rmi_get_driverdata(rmi_dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->enabled);
}

static ssize_t rmi_driver_enabled_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int retval;
	int new_value;
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;

	rmi_dev = to_rmi_device(dev);
	data = rmi_get_driverdata(rmi_dev);

	if (sysfs_streq(buf, "0"))
		new_value = false;
	else if (sysfs_streq(buf, "1"))
		new_value = true;
	else
		return -EINVAL;

	if (new_value) {
		retval = enable_sensor(rmi_dev);
		if (retval) {
			dev_err(dev, "Failed to enable sensor, code=%d.\n",
				retval);
			return -EIO;
		}
	} else {
		disable_sensor(rmi_dev);
	}

	return count;
}

#if REGISTER_DEBUG
static ssize_t rmi_driver_reg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int retval;
	unsigned int address;
	unsigned int bytes;
	struct rmi_device *rmi_dev;
	struct rmi_driver_data *data;
	u8 readbuf[128];
	unsigned char outbuf[512];
	unsigned char *bufptr = outbuf;
	int i;

	rmi_dev = to_rmi_device(dev);
	data = rmi_get_driverdata(rmi_dev);

	retval = sscanf(buf, "%x %u", &address, &bytes);
	if (retval != 2) {
		dev_err(dev, "Invalid input (code %d) for reg store: %s",
			retval, buf);
		return -EINVAL;
	}
	if (address < 0 || address > 0xFFFF) {
		dev_err(dev, "Invalid address for reg store '%#06x'.\n",
			address);
		return -EINVAL;
	}
	if (bytes < 0 || bytes >= sizeof(readbuf) || address+bytes > 65535) {
		dev_err(dev, "Invalid byte count for reg store '%d'.\n",
			bytes);
		return -EINVAL;
	}

	retval = rmi_read_block(rmi_dev, address, readbuf, bytes);
	if (retval != bytes) {
		dev_err(dev, "Failed to read %d registers at %#06x, code %d.\n",
			bytes, address, retval);
		return retval;
	}

	dev_info(dev, "Reading %d bytes from %#06x.\n", bytes, address);
	for (i = 0; i < bytes; i++) {
		retval = snprintf(bufptr, 4, "%02X ", readbuf[i]);
		if (retval < 0) {
			dev_err(dev, "Failed to format string. Code: %d",
				retval);
			return retval;
		}
		bufptr += retval;
	}
	dev_info(dev, "%s\n", outbuf);

	return count;
}
#endif

#if DELAY_DEBUG
static ssize_t rmi_delay_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int retval;
	struct rmi_device *rmi_dev;
	struct rmi_device_platform_data *pdata;
	unsigned int new_read_delay;
	unsigned int new_write_delay;
	unsigned int new_block_delay;
	unsigned int new_pre_delay;
	unsigned int new_post_delay;

	retval = sscanf(buf, "%u %u %u %u %u", &new_read_delay,
			&new_write_delay, &new_block_delay,
			&new_pre_delay, &new_post_delay);
	if (retval != 5) {
		dev_err(dev, "Incorrect number of values provided for delay.");
		return -EINVAL;
	}
	if (new_read_delay < 0) {
		dev_err(dev, "Byte delay must be positive microseconds.\n");
		return -EINVAL;
	}
	if (new_write_delay < 0) {
		dev_err(dev, "Write delay must be positive microseconds.\n");
		return -EINVAL;
	}
	if (new_block_delay < 0) {
		dev_err(dev, "Block delay must be positive microseconds.\n");
		return -EINVAL;
	}
	if (new_pre_delay < 0) {
		dev_err(dev,
			"Pre-transfer delay must be positive microseconds.\n");
		return -EINVAL;
	}
	if (new_post_delay < 0) {
		dev_err(dev,
			"Post-transfer delay must be positive microseconds.\n");
		return -EINVAL;
	}

	rmi_dev = to_rmi_device(dev);
	pdata = rmi_dev->phys->dev->platform_data;

	dev_info(dev, "Setting delays to %u %u %u %u %u.\n", new_read_delay,
		 new_write_delay, new_block_delay, new_pre_delay,
		 new_post_delay);
	pdata->spi_data.read_delay_us = new_read_delay;
	pdata->spi_data.write_delay_us = new_write_delay;
	pdata->spi_data.block_delay_us = new_block_delay;
	pdata->spi_data.pre_delay_us = new_pre_delay;
	pdata->spi_data.post_delay_us = new_post_delay;

	return count;
}

static ssize_t rmi_delay_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct rmi_device *rmi_dev;
	struct rmi_device_platform_data *pdata;

	rmi_dev = to_rmi_device(dev);
	pdata = rmi_dev->phys->dev->platform_data;

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d %d\n",
		pdata->spi_data.read_delay_us, pdata->spi_data.write_delay_us,
		pdata->spi_data.block_delay_us,
		pdata->spi_data.pre_delay_us, pdata->spi_data.post_delay_us);
}
#endif

static ssize_t rmi_driver_phys_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct rmi_device *rmi_dev;
	struct rmi_phys_info *info;

	rmi_dev = to_rmi_device(dev);
	info = &rmi_dev->phys->info;

	return snprintf(buf, PAGE_SIZE, "%-5s %ld %ld %ld %ld %ld %ld %ld\n",
		 info->proto ? info->proto : "unk",
		 info->tx_count, info->tx_bytes, info->tx_errs,
		 info->rx_count, info->rx_bytes, info->rx_errs,
		 info->attn_count);
}

static ssize_t rmi_driver_version_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n",
		 RMI_DRIVER_VERSION_STRING);
}

static int __init rmi_driver_init(void)
{
	return rmi_register_driver(&sensor_driver);
}

static void __exit rmi_driver_exit(void)
{
	rmi_unregister_driver(&sensor_driver);
}

module_init(rmi_driver_init);
module_exit(rmi_driver_exit);

MODULE_AUTHOR("Eric Andersson <eric.andersson@unixphere.com>");
MODULE_DESCRIPTION("RMI generic driver");
