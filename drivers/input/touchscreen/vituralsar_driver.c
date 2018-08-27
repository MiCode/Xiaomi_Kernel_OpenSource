/*
 * File:   fusb30x_driver.c
 * Company: Fairchild Semiconductor
 *
 * Created on September 2, 2015, 10:22 AM
 */
#define DEBUG
/* Standard Linux includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include "vituralsar_driver.h"
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
int sar_int_gpio;
static int sar_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	sar_int_gpio = of_get_named_gpio(np, "vituralsar,irq-gpio", 0);
	if (sar_int_gpio == 130) {
		printk("parse irq gpio correctly\n ");
		return 0;
	} else {
		printk("parse irq gpio incorrectly\n ");
		return -1;
	}
}
void sar_irq_enable(struct vituralsar_data *sdata)
{
	unsigned long irqflags = 0;
	spin_lock_irqsave(&sdata->irq_lock, irqflags);
	if (sdata->irq_is_disable) {
		 enable_irq_wake(sdata->client->irq);
		 sdata->irq_is_disable = 0;
	}
	spin_unlock_irqrestore(&sdata->irq_lock, irqflags);
}
void sar_irq_disable(struct vituralsar_data *sdata)
{
	unsigned long irqflags;
	spin_lock_irqsave(&sdata->irq_lock, irqflags);
	if (!sdata->irq_is_disable) {
		 disable_irq_wake(sdata->client->irq);
	sdata->irq_is_disable = 1;
	}
	spin_unlock_irqrestore(&sdata->irq_lock, irqflags);
}
int input_data = -1;
static irqreturn_t sar_irq_handler(int irq, void *dev_id)
{
	struct vituralsar_data *sdata = dev_id;
	unsigned long irqflags = 0;
	spin_lock_irqsave(&sdata->irq_lock, irqflags);
	input_data = gpio_get_value(sar_int_gpio);
	input_report_key(sdata->input_dev,gpio_key.code, 1);
	input_report_key(sdata->input_dev,gpio_key.code, 0);
	printk("input_report_key ,status :%d\n",input_data);
	input_sync(sdata->input_dev);
	spin_unlock_irqrestore(&sdata->irq_lock, irqflags);
	printk("gpio_get_value : %d\n",input_data);
	return IRQ_HANDLED;
}
static s8 sar_request_irq(struct vituralsar_data *sdata)
{
	s32 ret = -1;
	const u8 irq_table[] = SAR_IRQ_TAB;
	ret  = request_irq(sdata->client->irq,
				 sar_irq_handler,
				 irq_table[sdata->int_trigger_type],
				 sdata->client->name,
				 sdata);
	if (ret) {
		 printk("Request IRQ failed!ERRNO:%d.", ret);
	} else {
		 sdata->use_irq = 1;
		 return 0;
	}
	return -1;
}
static s8 sar_request_io_port(struct vituralsar_data *sdata)
{
	s32 ret = 0;
	ret = gpio_request(sar_int_gpio, "SAR_INT_IRQ");
	if (ret < 0) {
		 printk("Failed to request GPIO:%d, ERRNO:%d", (s32)sar_int_gpio, ret);
	gpio_free(sar_int_gpio);
	return ret;
	}
	else {
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
	return 0;
}
static struct proc_dir_entry *gpio_status = NULL;
#define GOIP_STATUS "gpio_status"
static int gpio_proc_show(struct seq_file *file, void*data)
{
	seq_printf(file, "%d\n", input_data);
	return 0;
}
static int gpio_proc_open (struct inode*inode, struct file*file)
{
	return single_open(file, gpio_proc_show, inode->i_private);
}
static const struct file_operations gpio_status_ops = {
	.open = gpio_proc_open,
	.read = seq_read,
};
static int virtualsar_probe (struct i2c_client*client,
		const struct i2c_device_id*id)
{
	int ret = 0;
	struct vituralsar_data *sdata;
	if (!client) {
		pr_err("SAR  %s - Error: Client structure is NULL!\n", __func__);
		return -EINVAL;
	}
	dev_info(&client->dev, "%s\n", __func__);
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
		 if (client->dev.of_node) {
		ret = sar_parse_dt(&client->dev);
		if (!ret)
			printk("sar_parse_dt success\n");
		 }
	sdata->int_trigger_type = SAR_INT_TRIGGER;
	sdata->client = client;
	spin_lock_init(&sdata->irq_lock);
	if (gpio_is_valid(sar_int_gpio)) {
		ret = sar_request_io_port(sdata);
		if (ret < 0) {
			printk("SAR %s -request io port fail\n",__func__);
			return -ENOMEM;
		}
	} else {
		printk("SAR %s -gpio is not valid\n",__func__);
		return -ENOMEM;
	}
	ret = sar_request_input_dev(sdata);
	if (ret < 0) {
		printk("SAR request input dev failed");
	}
	ret = sar_request_irq(sdata);
	if (ret < 0) {
		printk("SAR %s -request irq fail\n",__func__);
	}
	__set_bit(EV_REP, sdata->input_dev->evbit);
	if (sdata->use_irq)
		sar_irq_enable(sdata);
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
	printk("ret : %d\n",ret);
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
module_init(virtualsar_init);
module_exit(virtualsar_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("VirtualSAR Driver");
MODULE_AUTHOR("VirtualSar");
