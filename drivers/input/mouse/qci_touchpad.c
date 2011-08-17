/* Quanta I2C Touchpad Driver
 *
 * Copyright (C) 2009 Quanta Computer Inc.
 * Author: Hsin Wu <hsin.wu@quantatw.com>
 * Author: Austin Lai <austin.lai@quantatw.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

  /*
 *
 *  The Driver with I/O communications via the I2C Interface for ON2 of AP BU.
 *  And it is only working on the nuvoTon WPCE775x Embedded Controller.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/keyboard.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#define TOUCHPAD_ID_NAME          "qci-i2cpad"
#define TOUCHPAD_NAME                "PS2 Touchpad"
#define TOUCHPAD_DEVICE             "/i2c/input1"
#define TOUCHPAD_CMD_ENABLE             0xF4
#define TOUCHPAD_INIT_DELAY_MS    100

static int __devinit qcitp_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
static int __devexit qcitp_remove(struct i2c_client *kbd);

/* General structure to hold the driver data */
struct i2ctpad_drv_data {
	struct i2c_client *ti2c_client;
	struct work_struct work;
	struct input_dev *qcitp_dev;
	struct kobject *tp_kobj;
	unsigned int  qcitp_gpio;
	unsigned int  qcitp_irq;
	char ecdata[8];
};

static int tp_sense_val = 10;
static ssize_t tp_sensitive_show(struct kobject *kobj,
	struct kobj_attribute *attr, char * buf)
{
	return sprintf(buf, "%d\n", tp_sense_val);
}

static ssize_t tp_sensitive_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char* buf, size_t n)
{
	unsigned int val = 0;
	sscanf(buf, "%d", &val);

	if (val >= 1 && val <= 10)
		tp_sense_val = val;
	else
		return  -ENOSYS;

	return sizeof(buf);
}

static struct kobj_attribute tp_sensitivity = __ATTR(tp_sensitivity ,
						     0644 ,
						     tp_sensitive_show ,
						     tp_sensitive_store);

static struct attribute *g_tp[] = {
	&tp_sensitivity.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g_tp,
};

/*-----------------------------------------------------------------------------
 * Driver functions
 *---------------------------------------------------------------------------*/

#ifdef CONFIG_PM
static int qcitp_suspend(struct device *dev)
{
	return 0;
}

static int qcitp_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct i2c_device_id qcitp_idtable[] = {
	{ TOUCHPAD_ID_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, qcitp_idtable);
#ifdef CONFIG_PM
static const struct dev_pm_ops qcitp_pm_ops = {
	.suspend  = qcitp_suspend,
	.resume   = qcitp_resume,
};
#endif
static struct i2c_driver i2ctp_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = TOUCHPAD_ID_NAME,
#ifdef CONFIG_PM
		.pm = &qcitp_pm_ops,
#endif
	},
	.probe	  = qcitp_probe,
	.remove	  = __devexit_p(qcitp_remove),
	.id_table = qcitp_idtable,
};

static void qcitp_fetch_data(struct i2c_client *tpad_client,
	char *ec_data)
{
	struct i2c_msg tp_msg;
	int ret;
	tp_msg.addr = tpad_client->addr;
	tp_msg.flags = I2C_M_RD;
	tp_msg.len = 3;
	tp_msg.buf = (char *)&ec_data[0];
	ret = i2c_transfer(tpad_client->adapter, &tp_msg, 1);
}

static void qcitp_report_key(struct input_dev *tpad_dev, char *ec_data)
{
	int dx = 0;
	int dy = 0;

	if (ec_data[1])
		dx = (int) ec_data[1] -
		     (int) ((ec_data[0] << 4) & 0x100);

	if (ec_data[2])
		dy = (int) ((ec_data[0] << 3) & 0x100) -
		     (int) ec_data[2];

	dx = (dx * tp_sense_val)/10;
	dy = (dy * tp_sense_val)/10;

	input_report_key(tpad_dev, BTN_LEFT, ec_data[0] & 0x01);
	input_report_key(tpad_dev, BTN_RIGHT, ec_data[0] & 0x02);
	input_report_key(tpad_dev, BTN_MIDDLE, ec_data[0] & 0x04);
	input_report_rel(tpad_dev, REL_X, dx);
	input_report_rel(tpad_dev, REL_Y, dy);
	input_sync(tpad_dev);
}

static void qcitp_work_handler(struct work_struct *_work)
{
	struct i2ctpad_drv_data *itpad_drv_data =
		container_of(_work, struct i2ctpad_drv_data, work);

	struct i2c_client *itpad_client = itpad_drv_data->ti2c_client;
	struct input_dev *itpad_dev = itpad_drv_data->qcitp_dev;

	qcitp_fetch_data(itpad_client, itpad_drv_data->ecdata);
	qcitp_report_key(itpad_dev, itpad_drv_data->ecdata);
}

static irqreturn_t qcitp_interrupt(int irq, void *dev_id)
{
	struct i2ctpad_drv_data *itpad_drv_data = dev_id;
	schedule_work(&itpad_drv_data->work);
	return IRQ_HANDLED;
}

static int __devinit qcitp_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	int err = -ENOMEM;
	struct i2ctpad_drv_data *context = 0;

	context = kzalloc(sizeof(struct i2ctpad_drv_data), GFP_KERNEL);
	if (!context)
		return err;
	i2c_set_clientdata(client, context);
	context->ti2c_client = client;
	context->qcitp_gpio = client->irq;

	/* Enable mouse */
	i2c_smbus_write_byte(client, TOUCHPAD_CMD_ENABLE);
	msleep(TOUCHPAD_INIT_DELAY_MS);
	i2c_smbus_read_byte(client);
	/*allocate and register input device*/
	context->qcitp_dev = input_allocate_device();
	if (!context->qcitp_dev) {
		pr_err("[TouchPad] allocting memory fail\n");
		err = -ENOMEM;
		goto allocate_fail;
	}
	context->qcitp_dev->name        = TOUCHPAD_NAME;
	context->qcitp_dev->phys         = TOUCHPAD_DEVICE;
	context->qcitp_dev->id.bustype = BUS_I2C;
	context->qcitp_dev->id.vendor  = 0x1050;
	context->qcitp_dev->id.product = 0x1;
	context->qcitp_dev->id.version = 0x1;
	context->qcitp_dev->evbit[0]  = BIT_MASK(EV_KEY) |
					BIT_MASK(EV_REL);
	context->qcitp_dev->relbit[0] = BIT_MASK(REL_X) |
					BIT_MASK(REL_Y);
	context->qcitp_dev->keybit[BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) |
							 BIT_MASK(BTN_MIDDLE) |
							 BIT_MASK(BTN_RIGHT);

	input_set_drvdata(context->qcitp_dev, context);
	err = input_register_device(context->qcitp_dev);
	if (err) {
		pr_err("[TouchPad] register device fail\n");
		goto register_fail;
	}

	/*request intterrupt*/
	INIT_WORK(&context->work, qcitp_work_handler);

	err = gpio_request(context->qcitp_gpio, "qci-pad");
	if (err) {
		pr_err("[TouchPad]err gpio request\n");
		goto gpio_request_fail;
	}

	context->qcitp_irq = gpio_to_irq(context->qcitp_gpio);
	err = request_irq(context->qcitp_irq,
			  qcitp_interrupt,
			  IRQF_TRIGGER_FALLING,
			  TOUCHPAD_ID_NAME,
			  context);
	if (err) {
		pr_err("[TouchPad] unable to get IRQ\n");
		goto request_irq_fail;
	}
	/*create touchpad kobject*/
	context->tp_kobj = kobject_create_and_add("touchpad", NULL);

	err = sysfs_create_group(context->tp_kobj, &attr_group);
	if (err)
		pr_warning("[TouchPad] sysfs create fail\n");

	tp_sense_val = 10;

	return 0;

request_irq_fail:
	gpio_free(context->qcitp_gpio);

gpio_request_fail:
	input_unregister_device(context->qcitp_dev);

register_fail:
	input_free_device(context->qcitp_dev);

allocate_fail:
	i2c_set_clientdata(client, NULL);
	kfree(context);
	return err;
}

static int __devexit qcitp_remove(struct i2c_client *dev)
{
	struct i2ctpad_drv_data *context = i2c_get_clientdata(dev);

	free_irq(context->qcitp_irq, context);
	gpio_free(context->qcitp_gpio);
	input_free_device(context->qcitp_dev);
	input_unregister_device(context->qcitp_dev);
	kfree(context);

	return 0;
}

static int __init qcitp_init(void)
{
	return i2c_add_driver(&i2ctp_driver);
}


static void __exit qcitp_exit(void)
{
	i2c_del_driver(&i2ctp_driver);
}

module_init(qcitp_init);
module_exit(qcitp_exit);

MODULE_AUTHOR("Quanta Computer Inc.");
MODULE_DESCRIPTION("Quanta Embedded Controller I2C Touch Pad Driver");
MODULE_LICENSE("GPL v2");
