/*
 * File:   fusb30x_driver.c
 * Company: Fairchild Semiconductor
 *
 * Created on January 8, 2018, 11:07 AM
 */

#define DEBUG

/* Standard Linux includes */
#include <linux/init.h>				// __init, __initdata, etc
#include <linux/module.h>			// Needed to be a module
#include <linux/kernel.h>			// Needed to be a kernel module
#include <linux/i2c.h>				// I2C functionality
#include <linux/slab.h>				// devm_kzalloc
#include <linux/types.h>			// Kernel datatypes
#include <linux/errno.h>			// EINVAL, ERANGE, etc
#include <linux/of_device.h>			// Device tree functionality
#include <linux/interrupt.h>
#include "vituralsar_driver.h"
#include <linux/regulator/consumer.h>
#include <linux/delay.h>




/******************************************************************************
* Driver functions
******************************************************************************/


int sar_int_gpio;
static int sar_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;

	sar_int_gpio = of_get_named_gpio(np, "vituralsar,irq-gpio", 0);
	if (sar_int_gpio == 126) {
		printk("parse irq gpio correctly\n ");
		return 0;
	} else {
		printk("parse irq gpio incorrectly\n ");
		return -ENOMEM;
	}
}

int input_data = -1;
static irqreturn_t sar_irq_handler(int irq, void *dev_id)
{
	struct vituralsar_data *sdata = dev_id;
	unsigned long irqflags = 0;


	spin_lock_irqsave(&sdata->irq_lock, irqflags);
	//sar_irq_disable(sdata);
	input_data = gpio_get_value(sar_int_gpio);


	input_report_key(sdata->input_dev, gpio_key.code, 1);
	input_report_key(sdata->input_dev, gpio_key.code, 0);
	printk("input_report_key ,status :%d\n", input_data);
	input_sync(sdata->input_dev);

	spin_unlock_irqrestore(&sdata->irq_lock, irqflags);
	//sar_irq_enable(sdata);

	printk("gpio_get_value : %d\n", input_data);

	return IRQ_HANDLED;
}

static s8 sar_request_irq(struct vituralsar_data *sdata)
{
	s32 ret = -1;
	const u8 irq_table[] = SAR_IRQ_TAB;

	ret  = request_irq(sdata->client->irq, sar_irq_handler, irq_table[sdata->int_trigger_type], sdata->client->name, sdata);
	if (ret) {
		printk("Request IRQ failed!ERRNO:%d.", ret);
	} else {
		irq = sdata->client->irq;
		return 0;
	}
	return -ENOMEM;
}

static s8 sar_request_io_port(struct vituralsar_data *sdata)
{
	s32 ret = 0;

	ret = gpio_request(sar_int_gpio, "SAR_INT_IRQ");
	if (ret < 0) {
		printk("Failed to request GPIO:%d, ERRNO:%d", (s32)sar_int_gpio, ret);
		gpio_free(sar_int_gpio);
		return ret;
	} else {
		gpio_direction_input(sar_int_gpio);
		sdata->client->irq = gpio_to_irq(sar_int_gpio);
	}

	return ret;
}

static s8 sar_request_input_dev(struct vituralsar_data *sdata)
{
	s8 ret = -1;

	sdata->input_dev = input_allocate_device();
	if (sdata->input_dev == NULL) {
		printk("Failed to allocate input device.");
		return -ENOMEM;
	}

	sdata->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;

	__set_bit(EV_REP, sdata->input_dev->evbit);


	sdata->input_dev->name = sar_name;
	sdata->input_dev->phys = sar_input_phys;
	sdata->input_dev->id.bustype = BUS_I2C;

	ret = input_register_device(sdata->input_dev);
	if (ret) {
		printk("Register %s input device failed", sdata->input_dev->name);
		return -ENODEV;
	}
	input_set_capability(sdata->input_dev, EV_KEY, gpio_key.code);
	//input_set_abs_params(sdata->input_dev, ABS_MT_POSITION_X, 0, sdata->abs_x_max, 0, 0);

	return 0;
}
static struct proc_dir_entry *gpio_status;
#define GOIP_STATUS "gpio_status"

static int gpio_proc_show(struct seq_file *file, void *data)
{
	input_data = gpio_get_value(sar_int_gpio);
	seq_printf(file, "%d\n", input_data);

	return 0;
}


static int gpio_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, gpio_proc_show, inode->i_private);
}


static const struct file_operations gpio_status_ops = {
	.open = gpio_proc_open,
	.read = seq_read,
};

static int virtualsar_suspend(struct device *dev)
{
	int ret;

	ret = enable_irq_wake(irq);
	if (ret) {
		printk("virtualsar_suspend enable_irq_wake failed!\n");
		return -ENODEV;
	}

	return 0;
}

static int virtualsar_resume(struct device *dev)
{
	int ret;

	ret = disable_irq_wake(irq);
	if (ret) {
		printk("virtualsar_resume disable_irq_wake failed!\n");
		return -ENODEV;
	}

	return 0;
}

static int virtualsar_probe (struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct vituralsar_data *sdata;//fusb30x_chip* chip;

	if (!client) {
		pr_err("SAR  %s - Error: Client structure is NULL!\n", __func__);
		return -EINVAL;
	}
	dev_info(&client->dev, "%s\n", __func__);

	/* Make sure probe was called on a compatible device */
	if (!of_match_device(virtualsar_dt_match, &client->dev)) {
		dev_err(&client->dev, "FUSB  %s - Error: Device tree mismatch!\n", __func__);
		return -EINVAL;
	}
	pr_debug("SAR  %s - Device tree matched!\n", __func__);

	sdata = kzalloc(sizeof(*sdata), GFP_KERNEL);
	if (sdata == NULL) {
		printk("Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}
	//parse dt for irq
	if (client->dev.of_node) {
		ret = sar_parse_dt(&client->dev);
		if (!ret)
			printk("sar_parse_dt success\n");
	}
	//set irq Trigger mode
	sdata->int_trigger_type = SAR_INT_TRIGGER;

	sdata->client = client;
	spin_lock_init(&sdata->irq_lock);

	//request io port
	if (gpio_is_valid(sar_int_gpio)) {
		ret = sar_request_io_port(sdata);
		if (ret < 0) {
			printk("SAR %s -request io port fail\n", __func__);
			return -ENOMEM;
		}
	} else {
		printk("SAR %s -gpio is not valid\n", __func__);
		return -ENOMEM;
	}

	//request input dev
	ret = sar_request_input_dev(sdata);
	if (ret < 0) {
		printk("SAR request input dev failed");
	}

	//request irq
	ret = sar_request_irq(sdata);
	if (ret < 0) {
		printk("SAR %s -request irq fail\n", __func__);
	}

	__set_bit(EV_REP, sdata->input_dev->evbit);

	//enable irq
	printk("after sar_irq_enable,probe end \n");

	gpio_status = proc_create(GOIP_STATUS, 0644, NULL, &gpio_status_ops);
	if (gpio_status == NULL) {
		printk("tpd, create_proc_entry gpio_status_ops failed\n");
	}

	return 0;
}


static int __init virtualsar_init(void)
{
	int ret = 0;
	pr_debug("SAR  %s - Start driver initialization...\n", __func__);

	ret = i2c_add_driver(&virtualsar_driver);
	printk("ret : %d\n", ret);
	return ret;
}

static void __exit virtualsar_exit(void)
{
	i2c_del_driver(&virtualsar_driver);
	pr_debug("SAR  %s - Driver deleted...\n", __func__);
}


/*******************************************************************************
 * Driver macros
 ******************************************************************************/
//#if ADDED_BY_HQ_WWM
//late_initcall(fusb30x_init);			// Defines the module's entrance function
//#else
module_init(virtualsar_init);			// Defines the module's entrance function
//#endif
module_exit(virtualsar_exit);			// Defines the module's exit function

MODULE_LICENSE("GPL");				// Exposed on call to modinfo
MODULE_DESCRIPTION("VirtualSAR Driver");	// Exposed on call to modinfo
MODULE_AUTHOR("VirtualSar");			// Exposed on call to modinfo
