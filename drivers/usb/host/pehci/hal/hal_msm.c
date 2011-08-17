/* 
* Copyright (C) ST-Ericsson AP Pte Ltd 2010 
*
* ISP1763 Linux HCD Controller driver : hal
* 
* This program is free software; you can redistribute it and/or modify it under the terms of 
* the GNU General Public License as published by the Free Software Foundation; version 
* 2 of the License. 
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY  
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more  
* details. 
* 
* You should have received a copy of the GNU General Public License 
* along with this program; if not, write to the Free Software 
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
* 
* This is the main hardware abstraction layer file. Hardware initialization, interupt
* processing and read/write routines are handled here.
* 
* Author : wired support <wired.support@stericsson.com>
*
*/


#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/usb.h>
#include <linux/gpio.h>
#include <mach/board.h>
#include <linux/poll.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/unaligned.h>


/*--------------------------------------------------------------*
 *               linux system include files
 *--------------------------------------------------------------*/
#include "hal_msm.h"
#include "../hal/hal_intf.h"
#include "../hal/isp1763.h"


/*--------------------------------------------------------------*
 *               Local variable Definitions
 *--------------------------------------------------------------*/
struct isp1763_dev isp1763_loc_dev[ISP1763_LAST_DEV];


/*--------------------------------------------------------------*
 *               Local # Definitions
 *--------------------------------------------------------------*/
#define         PCI_ACCESS_RETRY_COUNT  20
#define         ISP1763_DRIVER_NAME     "isp1763_usb"

/*--------------------------------------------------------------*
 *               Local Function
 *--------------------------------------------------------------*/

static int __devexit isp1763_remove(struct platform_device *pdev);
static int __devinit isp1763_probe(struct platform_device *pdev);


/*--------------------------------------------------------------*
 *               Platform Driver Interface Functions
 *--------------------------------------------------------------*/

static struct platform_driver isp1763_usb_driver = {
	.remove = __exit_p(isp1763_remove),
	.driver = {
		.name = ISP1763_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};


/*--------------------------------------------------------------*
 *               ISP1763 Read write routine
 *--------------------------------------------------------------*/
/*
 * EBI2 on 8660 ignores the first bit and shifts the address by
 * one bit to the right.
 * Hence, shift left all the register addresses before accessing
 * them over EBI2.
 * This logic applies only for the register read/writes, for
 * read/write from ISP memory this conversion is not needed
 * as the ISP obtains the memory address from 'memory' register
 */

/* Write a 32 bit Register of isp1763 */
void
isp1763_reg_write32(struct isp1763_dev *dev, u16 reg, u32 data)
{
	/* Write the 32bit to the register address given to us */

	reg <<= 1;
#ifdef DATABUS_WIDTH_16
	writew((u16) data, dev->baseaddress + ((reg)));
	writew((u16) (data >> 16), dev->baseaddress + (((reg + 4))));
#else
	writeb((u8) data, dev->baseaddress + (reg));
	writeb((u8) (data >> 8), dev->baseaddress + ((reg + 1)));
	writeb((u8) (data >> 16), dev->baseaddress + ((reg + 2)));
	writeb((u8) (data >> 24), dev->baseaddress + ((reg + 3)));
#endif

}
EXPORT_SYMBOL(isp1763_reg_write32);


/* Read a 32 bit Register of isp1763 */
u32
isp1763_reg_read32(struct isp1763_dev *dev, u16 reg, u32 data)
{

#ifdef DATABUS_WIDTH_16
	u16 wvalue1, wvalue2;
#else
	u8 bval1, bval2, bval3, bval4;
#endif
	data = 0;
	reg <<= 1;
#ifdef DATABUS_WIDTH_16
	wvalue1 = readw(dev->baseaddress + ((reg)));
	wvalue2 = readw(dev->baseaddress + (((reg + 4))));
	data |= wvalue2;
	data <<= 16;
	data |= wvalue1;
#else

	bval1 = readb(dev->baseaddress + (reg));
	bval2 = readb(dev->baseaddress + (reg + 1));
	bval3 = readb(dev->baseaddress + (reg + 2));
	bval4 = readb(dev->baseaddress + (reg + 3));
	data = 0;
	data |= bval4;
	data <<= 8;
	data |= bval3;
	data <<= 8;
	data |= bval2;
	data <<= 8;
	data |= bval1;

#endif

	return data;
}
EXPORT_SYMBOL(isp1763_reg_read32);


/* Read a 16 bit Register of isp1763 */
u16
isp1763_reg_read16(struct isp1763_dev * dev, u16 reg, u16 data)
{
	reg <<= 1;
#ifdef DATABUS_WIDTH_16
	data = readw(dev->baseaddress + ((reg)));
#else
	u8 bval1, bval2;
	bval1 = readb(dev->baseaddress + (reg));
	if (reg == HC_DATA_REG){
		bval2 = readb(dev->baseaddress + (reg));
	} else {
		bval2 = readb(dev->baseaddress + ((reg + 1)));
	}
	data = 0;
	data |= bval2;
	data <<= 8;
	data |= bval1;

#endif
	return data;
}
EXPORT_SYMBOL(isp1763_reg_read16);

/* Write a 16 bit Register of isp1763 */
void
isp1763_reg_write16(struct isp1763_dev *dev, u16 reg, u16 data)
{
	reg <<= 1;
#ifdef DATABUS_WIDTH_16
	writew(data, dev->baseaddress + ((reg)));
#else
	writeb((u8) data, dev->baseaddress + (reg));
	if (reg == HC_DATA_REG){
		writeb((u8) (data >> 8), dev->baseaddress + (reg));
	}else{
		writeb((u8) (data >> 8), dev->baseaddress + ((reg + 1)));
	}

#endif
}
EXPORT_SYMBOL(isp1763_reg_write16);

/* Read a 8 bit Register of isp1763 */
u8
isp1763_reg_read8(struct isp1763_dev *dev, u16 reg, u8 data)
{
	reg <<= 1;
	data = readb((dev->baseaddress + (reg)));
	return data;
}
EXPORT_SYMBOL(isp1763_reg_read8);

/* Write a 8 bit Register of isp1763 */
void
isp1763_reg_write8(struct isp1763_dev *dev, u16 reg, u8 data)
{
	reg <<= 1;
	writeb(data, (dev->baseaddress + (reg)));
}
EXPORT_SYMBOL(isp1763_reg_write8);


/*--------------------------------------------------------------*
 *
 * Module dtatils: isp1763_mem_read
 *
 * Memory read using PIO method.
 *
 *  Input: struct isp1763_driver *drv  -->  Driver structure.
 *                      u32 start_add     --> Starting address of memory
 *              u32 end_add     ---> End address
 *
 *              u32 * buffer      --> Buffer pointer.
 *              u32 length       ---> Length
 *              u16 dir          ---> Direction ( Inc or Dec)
 *
 *  Output     int Length  ----> Number of bytes read
 *
 *  Called by: system function
 *
 *
 *--------------------------------------------------------------*/
/* Memory read function PIO */

int
isp1763_mem_read(struct isp1763_dev *dev, u32 start_add,
	u32 end_add, u32 * buffer, u32 length, u16 dir)
{
	u8 *one = (u8 *) buffer;
	u16 *two = (u16 *) buffer;
	u32 a = (u32) length;
	u32 w;
	u32 w2;

	if (buffer == 0) {
		printk("Buffer address zero\n");
		return 0;
	}


	isp1763_reg_write16(dev, HC_MEM_READ_REG, start_add);
	/* This delay requirement comes from the ISP1763A programming guide */
	ndelay(100);
last:
	w = isp1763_reg_read16(dev, HC_DATA_REG, w);
	w2 = isp1763_reg_read16(dev, HC_DATA_REG, w);
	w2 <<= 16;
	w = w | w2;
	if (a == 1) {
		*one = (u8) w;
		return 0;
	}
	if (a == 2) {
		*two = (u16) w;
		return 0;
	}

	if (a == 3) {
		*two = (u16) w;
		two += 1;
		w >>= 16;
		*two = (u8) (w);
		return 0;

	}
	while (a > 0) {
		*buffer = w;
		a -= 4;
		if (a <= 0) {
			break;
		}
		if (a < 4) {
			buffer += 1;
			one = (u8 *) buffer;
			two = (u16 *) buffer;
			goto last;
		}
		buffer += 1;
		w = isp1763_reg_read16(dev, HC_DATA_REG, w);
		w2 = isp1763_reg_read16(dev, HC_DATA_REG, w);
		w2 <<= 16;
		w = w | w2;
	}
	return ((a < 0) || (a == 0)) ? 0 : (-1);

}
EXPORT_SYMBOL(isp1763_mem_read);


/*--------------------------------------------------------------*
 *
 * Module dtatils: isp1763_mem_write
 *
 * Memory write using PIO method.
 *
 *  Input: struct isp1763_driver *drv  -->  Driver structure.
 *                      u32 start_add     --> Starting address of memory
 *              u32 end_add     ---> End address
 *
 *              u32 * buffer      --> Buffer pointer.
 *              u32 length       ---> Length
 *              u16 dir          ---> Direction ( Inc or Dec)
 *
 *  Output     int Length  ----> Number of bytes read
 *
 *  Called by: system function
 *
 *
 *--------------------------------------------------------------*/

/* Memory read function IO */

int
isp1763_mem_write(struct isp1763_dev *dev,
	u32 start_add, u32 end_add, u32 * buffer, u32 length, u16 dir)
{
	int a = length;
	u8 one = (u8) (*buffer);
	u16 two = (u16) (*buffer);


	isp1763_reg_write16(dev, HC_MEM_READ_REG, start_add);
	/* This delay requirement comes from the ISP1763A programming guide */
	ndelay(100);

	if (a == 1) {
		isp1763_reg_write16(dev, HC_DATA_REG, one);
		return 0;
	}
	if (a == 2) {
		isp1763_reg_write16(dev, HC_DATA_REG, two);
		return 0;
	}

	while (a > 0) {
		isp1763_reg_write16(dev, HC_DATA_REG, (u16) (*buffer));
		if (a >= 3)
			isp1763_reg_write16(dev, HC_DATA_REG,
					    (u16) ((*buffer) >> 16));
		start_add += 4;
		a -= 4;
		if (a <= 0)
			break;
		buffer += 1;

	}

	return ((a < 0) || (a == 0)) ? 0 : (-1);

}
EXPORT_SYMBOL(isp1763_mem_write);


/*--------------------------------------------------------------*
 *
 * Module dtatils: isp1763_register_driver
 *
 * This function is used by top driver (OTG, HCD, DCD) to register
 * their communication functions (probe, remove, suspend, resume) using
 * the drv data structure.
 * This function will call the probe function of the driver if the ISP1763
 * corresponding to the driver is enabled
 *
 *  Input: struct isp1763_driver *drv  --> Driver structure.
 *  Output result
 *         0= complete
 *         1= error.
 *
 *  Called by: system function module_init
 *
 *
 *--------------------------------------------------------------*/

int
isp1763_register_driver(struct isp1763_driver *drv)
{
	struct isp1763_dev *dev;
	int result = -EINVAL;

	hal_entry("%s: Entered\n", __FUNCTION__);
	info("isp1763_register_driver(drv=%p)\n", drv);

	if (!drv) {
		return -EINVAL;
	}

	dev = &isp1763_loc_dev[drv->index];
	if (!dev->baseaddress)
		return -EINVAL;

	dev->active = 1;	/* set the driver as active*/

	if (drv->probe) {
		result = drv->probe(dev, drv->id);
	} else {
		printk("%s no probe function for indes %d \n", __FUNCTION__,
			(int)drv->index);
	}

	if (result >= 0) {
		pr_debug(KERN_INFO __FILE__ ": Registered Driver %s\n",
			drv->name);
		dev->driver = drv;
	}
	hal_entry("%s: Exit\n", __FUNCTION__);
	return result;
}				/* End of isp1763_register_driver */
EXPORT_SYMBOL(isp1763_register_driver);


/*--------------------------------------------------------------*
 *
 * Module dtatils: isp1763_unregister_driver
 *
 * This function is used by top driver (OTG, HCD, DCD) to de-register
 * their communication functions (probe, remove, suspend, resume) using
 * the drv data structure.
 * This function will check whether the driver is registered or not and
 * call the remove function of the driver if registered
 *
 *  Input: struct isp1763_driver *drv  --> Driver structure.
 *  Output result
 *         0= complete
 *         1= error.
 *
 *  Called by: system function module_init
 *
 *
 *--------------------------------------------------------------*/

void
isp1763_unregister_driver(struct isp1763_driver *drv)
{
	struct isp1763_dev *dev;
	hal_entry("%s: Entered\n", __FUNCTION__);

	info("isp1763_unregister_driver(drv=%p)\n", drv);
	dev = &isp1763_loc_dev[drv->index];
	if (dev->driver == drv) {
		/* driver registered is same as the requestig driver */
		drv->remove(dev);
		dev->driver = NULL;
		info(": De-registered Driver %s\n", drv->name);
		return;
	}
	hal_entry("%s: Exit\n", __FUNCTION__);
}				/* End of isp1763_unregister_driver */
EXPORT_SYMBOL(isp1763_unregister_driver);


/*--------------------------------------------------------------*
 *               ISP1763 Platform driver interface routine.
 *--------------------------------------------------------------*/


/*--------------------------------------------------------------*
 *
 *  Module dtatils: isp1763_module_init
 *
 *  This  is the module initialization function. It registers to
 *  driver for a isp1763 platform device. And also resets the
 *  internal data structures.
 *
 *  Input: void
 *  Output result
 *         0= complete
 *         1= error.
 *
 *  Called by: system function module_init
 *
 *
 *
 -------------------------------------------------------------------*/
static int __init
isp1763_module_init(void)
{
	int result = 0;
	hal_entry("%s: Entered\n", __FUNCTION__);
	pr_debug(KERN_NOTICE "+isp1763_module_init\n");
	memset(isp1763_loc_dev, 0, sizeof(isp1763_loc_dev));

	result = platform_driver_probe(&isp1763_usb_driver, isp1763_probe);

	pr_debug(KERN_NOTICE "-isp1763_module_init\n");
	hal_entry("%s: Exit\n", __FUNCTION__);
	return result;
}

/*--------------------------------------------------------------*
 *
 *  Module dtatils: isp1763_module_cleanup
 *
 * This  is the module cleanup function. It de-registers the
 * Platform driver and resets the internal data structures.
 *
 *  Input: void
 *  Output void
 *
 *  Called by: system function module_cleanup
 *
 *
 *
 --------------------------------------------------------------*/

static void __exit
isp1763_module_cleanup(void)
{
	pr_debug("Hal Module Cleanup\n");
	platform_driver_unregister(&isp1763_usb_driver);

	memset(isp1763_loc_dev, 0, sizeof(isp1763_loc_dev));
}

void dummy_mem_read(struct isp1763_dev *dev)
{
	u32 w = 0;
	isp1763_reg_write16(dev, HC_MEM_READ_REG, 0x0400);
	w = isp1763_reg_read16(dev, HC_DATA_REG, w);

	pr_debug("dummy_read DONE: %x\n", w);
	msleep(10);
}
/*--------------------------------------------------------------*
 *
 *  Module dtatils: isp1763_probe
 *
 * probe function of ISP1763
 * This function is called from module_init if the corresponding platform
 * device is present. This function initializes the information
 * for the Host Controller with the assigned resources and tests the register
 * access to the controller and do a software reset and makes it ready
 * for the driver to play with. It also calls setup_gpio passed from pdata
 * to setup GPIOs (e.g. used for IRQ and RST lines).
 *
 *  Input:
 *              struct platform_device *dev   ----> Platform Device structure
 *  Output void
 *
 *  Called by: system function module_cleanup
 *
 *
 *
 --------------------------------------------------------------**/

static int __devinit
isp1763_probe(struct platform_device *pdev)
{
	u32 reg_data = 0;
	struct isp1763_dev *loc_dev;
	int status = 1;
	u32 hwmodectrl = 0;
	u16 us_reset_hc = 0;
	u32 chipid = 0;
	struct isp1763_platform_data *pdata = pdev->dev.platform_data;

	hal_entry("%s: Entered\n", __FUNCTION__);

	hal_init(("isp1763_probe(dev=%p)\n", dev));

	loc_dev = &(isp1763_loc_dev[ISP1763_HC]);
	loc_dev->dev = pdev;

	/* Get the Host Controller IO and INT resources */
	loc_dev->mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!loc_dev->mem_res) {
		pr_err("%s: failed to get platform resource mem\n", __func__);
		return -ENODEV;
	}

	loc_dev->baseaddress = ioremap_nocache(loc_dev->mem_res->start,
					resource_size(loc_dev->mem_res));
	if (!loc_dev->baseaddress) {
		pr_err("%s: ioremap failed\n", __func__);
		status = -ENOMEM;
		goto put_mem_res;
	}
	pr_info("%s: ioremap done at: %x\n", __func__,
					(int)loc_dev->baseaddress);
	loc_dev->irq = platform_get_irq(pdev, 0);
	if (!loc_dev->irq) {
		pr_err("%s: platform_get_irq failed\n", __func__);
		status = -ENODEV;
		goto free_regs;
	}

	loc_dev->index = ISP1763_HC;	/*zero */
	loc_dev->length = resource_size(loc_dev->mem_res);

	hal_init(("isp1763 HC MEM Base= %p irq = %d\n",
		loc_dev->baseaddress, loc_dev->irq));

	/* Setup GPIOs and isssue RESET_N to Controller */
	if (pdata->setup_gpio)
		if (pdata->setup_gpio(1))
			pr_err("%s: Failed to setup GPIOs for isp1763\n",
								 __func__);
	if (pdata->reset_gpio) {
		gpio_set_value(pdata->reset_gpio, 0);
		msleep(10);
		gpio_set_value(pdata->reset_gpio, 1);
	} else {
		pr_err("%s: Failed to issue RESET_N to isp1763\n", __func__);
	}

	dummy_mem_read(loc_dev);

	chipid = isp1763_reg_read32(loc_dev, DC_CHIPID, chipid);
	pr_info("START: chip id:%x\n", chipid);

	/*reset the host controller  */
	pr_debug("RESETTING\n");
	us_reset_hc |= 0x1;
	isp1763_reg_write16(loc_dev, 0xB8, us_reset_hc);
	msleep(20);
	us_reset_hc = 0;
	us_reset_hc |= 0x2;
	isp1763_reg_write16(loc_dev, 0xB8, us_reset_hc);

	chipid = isp1763_reg_read32(loc_dev, DC_CHIPID, chipid);
	pr_info("after HC reset, chipid:%x\n", chipid);

	msleep(20);
	hwmodectrl = isp1763_reg_read16(loc_dev, HC_HWMODECTRL_REG, hwmodectrl);
	pr_debug("Mode Ctrl Value b4 setting buswidth: %x\n", hwmodectrl);
#ifdef DATABUS_WIDTH_16
	hwmodectrl &= 0xFFEF;	/*enable the 16 bit bus */
#else
	pr_debug("Setting 8-BIT mode\n");
	hwmodectrl |= 0x0010;	/*enable the 8 bit bus */
#endif
	isp1763_reg_write16(loc_dev, HC_HWMODECTRL_REG, hwmodectrl);
	pr_debug("writing 0x%x to hw mode reg\n", hwmodectrl);

	hwmodectrl = isp1763_reg_read16(loc_dev, HC_HWMODECTRL_REG, hwmodectrl);
	msleep(100);

	pr_debug("Mode Ctrl Value after setting buswidth: %x\n", hwmodectrl);


	chipid = isp1763_reg_read32(loc_dev, DC_CHIPID, chipid);
	pr_debug("after setting HW MODE to 8bit, chipid:%x\n", chipid);



	hal_init(("isp1763 DC MEM Base= %lx irq = %d\n",
		loc_dev->io_base, loc_dev->irq));
	reg_data = isp1763_reg_read16(loc_dev, HC_SCRATCH_REG, reg_data);
	pr_debug("Scratch register is 0x%x\n", reg_data);
	reg_data = 0xABCD;
	isp1763_reg_write16(loc_dev, HC_SCRATCH_REG, reg_data);
	reg_data = isp1763_reg_read16(loc_dev, HC_SCRATCH_REG, reg_data);
	pr_debug("After write, Scratch register is 0x%x\n", reg_data);

	if (reg_data != 0xABCD) {
		pr_err("%s: Scratch register write mismatch!!\n", __func__);
		status = -ENODEV;
		goto free_gpios;
	}

	memcpy(loc_dev->name, ISP1763_DRIVER_NAME, sizeof(ISP1763_DRIVER_NAME));
	loc_dev->name[sizeof(ISP1763_DRIVER_NAME)] = 0;

	pr_debug(KERN_NOTICE "-isp1763_pci_probe\n");
	hal_entry("%s: Exit\n", __FUNCTION__);
	return 0;

free_gpios:
	if (pdata->setup_gpio)
		pdata->setup_gpio(0);
free_regs:
	iounmap(loc_dev->baseaddress);
put_mem_res:
	loc_dev->baseaddress = NULL;
	hal_entry("%s: Exit\n", __FUNCTION__);
	return status;
}				/* End of isp1763_probe */


/*--------------------------------------------------------------*
 *
 *  Module details: isp1763_remove
 *
 * cleanup function of ISP1763
 * This functions de-initializes the local variables, frees GPIOs
 * and releases memory resource.
 *
 *  Input:
 *              struct platform_device *dev    ----> Platform Device structure
 *
 *  Output void
 *
 *  Called by: system function module_cleanup
 *
 *
 *
 --------------------------------------------------------------*/
static int __devexit
isp1763_remove(struct platform_device *pdev)
{
	struct isp1763_dev *loc_dev;
	struct isp1763_platform_data *pdata = pdev->dev.platform_data;

	hal_init(("isp1763_pci_remove(dev=%p)\n", dev));

	loc_dev = &isp1763_loc_dev[ISP1763_HC];
	iounmap(loc_dev->baseaddress);
	loc_dev->baseaddress = NULL;
	if (pdata->setup_gpio)
		return pdata->setup_gpio(0);

	return 0;
}				/* End of isp1763_remove */


MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

module_init(isp1763_module_init);
module_exit(isp1763_module_cleanup);
