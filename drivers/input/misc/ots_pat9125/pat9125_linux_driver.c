
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/miscdevice.h>

#include "pixart_ots.h"
#include "pixart_platform.h"

static int pat9125_init_input_data(void);

#define pat9125_name "pixart_pat9125"

#define pat9125_DEV_NAME     pat9125_name

static struct pat9125_linux_data_t pat9125data;

static int pat9125_i2c_write(u8 reg, u8 *data, int len);
static int pat9125_i2c_read(u8 reg, u8 *data);

/**************************************/

extern unsigned char ReadData(unsigned char addr)
{
	u8 data = 0xff;
	pat9125_i2c_read(addr, &data);
	return data;
}
extern void WriteData(unsigned char addr, unsigned char data)
{
	pat9125_i2c_write(addr, &data, 1);
}
extern void delay_ms(int ms)
{
	msleep(ms);
}
/**************************************/
static int pat9125_i2c_write(u8 reg, u8 *data, int len)
{
	u8  buf[20];
	int rc;
	int ret = 0;
	int i;

	buf[0] = reg;
	if (len >= 20) {
		pr_debug(
			"%s (%d) : FAILED: buffer size is limitted(20) %d\n",
			__func__, __LINE__, len);
		dev_err(&pat9125data.client->dev, "pat9125_i2c_write FAILED: buffer size is limitted(20)\n");
		return -ENODEV;
	}

	for (i = 0 ; i < len; i++)
		buf[i+1] = data[i];

	/* Returns negative errno, or else the number of bytes written. */
	rc = i2c_master_send(pat9125data.client, buf, len+1);

	if (rc != len+1) {
		pr_debug(
			"%s (%d) : FAILED: writing to reg 0x%x\n",
			__func__, __LINE__, reg);

		ret = -ENODEV;
	}

	return ret;
}

static int pat9125_i2c_read(u8 reg, u8 *data)
{

	u8  buf[20];
	int rc;

	buf[0] = reg;

	/* If everything went ok (i.e. 1 msg transmitted),
	return #bytes  transmitted, else error code.
	thus if transmit is ok  return value 1 */
	rc = i2c_master_send(pat9125data.client, buf, 1);
	if (rc != 1) {
		pr_debug(
			"%s (%d) : FAILED: writing to address 0x%x\n",
			__func__, __LINE__, reg);
		return -ENODEV;
	}

	/* returns negative errno, or else the number of bytes read */
	rc = i2c_master_recv(pat9125data.client, buf, 1);
	if (rc != 1) {
		pr_debug(
			"%s (%d) : FAILED: reading data\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	*data = buf[0];
	return 0;
}

void pixart_pat9125_ist(void)
{

}

static irqreturn_t pixart_pat9125_irq(int irq, void *handle)
{
/* "cat /proc/kmsg" to see kernel message */
	pixart_pat9125_ist();
	return IRQ_HANDLED;
}

static int pat9125_start(void)
{
	int err = (-1);
	 pr_debug(">>> %s (%d)\n", __func__, __LINE__);

	err = request_threaded_irq(pat9125data.irq, NULL, pixart_pat9125_irq,
				   pat9125data.irq_flags,
				   "pixart_pat9125_irq",
				   &pat9125data);
	if (err)
		pr_debug("irq %d busy?\n", pat9125data.irq);

	pat9125data.last_jiffies = jiffies_64;

	return err;
}

static void pat9125_stop(void)
{
	 pr_debug(">>> %s (%d)\n", __func__, __LINE__);
	free_irq(pat9125data.irq, &pat9125data);
}

static ssize_t pat9125_fops_read(struct file *filp,
	char *buf, size_t count, loff_t *l)
{
	 pr_debug(">>> %s (%d)\n", __func__, __LINE__);
	return 0;
}

static ssize_t pat9125_fops_write(struct file *filp,
	const char *buf, size_t count, loff_t *f_ops)
{
	 pr_debug(">>> %s (%d)\n", __func__, __LINE__);
	return 0;
}

static long pat9125_fops_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
/* static int pat9125_fops_ioctl(struct inode *inode,
	struct file *file, unsigned int cmd, unsigned long arg) */
{
	 pr_debug(">>> %s (%d)\n", __func__, __LINE__);
	return 0;
}

static int pat9125_fops_open(struct inode *inode, struct file *filp)
{
	 pr_debug(">>> %s (%d)\n", __func__, __LINE__);
	return 0;
}

static int pat9125_fops_release(struct inode *inode, struct file *filp)
{
	 pr_debug(">>> %s (%d)\n", __func__, __LINE__);
	return 0;
}
static const struct file_operations pat9125_fops = {
owner:	THIS_MODULE,
	read :	pat9125_fops_read,
	write : pat9125_fops_write,
	/* ioctl	:	pat9125_fops_ioctl, */
	unlocked_ioctl	:	pat9125_fops_ioctl,
	open	:	pat9125_fops_open,
	release	:	pat9125_fops_release,
};

/*----------------------------------------------------------------------------*/
struct miscdevice pat9125_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = pat9125_name,
	.fops = &pat9125_fops,
};
static ssize_t pat9125_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char s[256];
	char *p = s;

	pr_debug("%s (%d) : write_reg_store\n", __func__, __LINE__);

	memcpy(s, buf, sizeof(s));

	*(s+1) = '\0';
	*(s+4) = '\0';
	*(s+7) = '\0';
	/* example(in console): echo w 12 34 > rw_reg */
	if (*p == 'w') {
		long write_addr, write_data;
		p += 2;
		if (!kstrtol(p, 16, &write_addr)) {
			p += 3;
			if (!kstrtol(p, 16, &write_data)) {
				pr_debug(
					"w 0x%x 0x%x\n",
					(u8)write_addr, (u8)write_data);
				WriteData((u8)write_addr, (u8)write_data);
			}
		}
		/* example(in console): echo r 12 > rw_reg */
	}	else if (*p == 'r')	{
		long read_addr;
		p += 2;

		if (!kstrtol(p, 16, &read_addr)) {
			int data = 0;
			data = ReadData((u8)read_addr);
			pr_debug(
				"r 0x%x 0x%x\n",
				(unsigned int)read_addr, data);
		}
	}
	return count;
}

static ssize_t pat9125_test_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{

	/* cat */
	pr_debug("%s (%d) :\n", __func__, __LINE__);

	return 0;
}
static DEVICE_ATTR(
	test,
	S_IRUGO | S_IWUGO , pat9125_test_show, pat9125_test_store);
static struct device_attribute *pat9125_attr_list[] = {
	&dev_attr_test,
};


/*----------------------------------------------------------------------------*/
static int pat9125_create_attr(struct device *dev)
{
	int idx, err = 0;
	int num = (int)(sizeof(pat9125_attr_list)/sizeof(pat9125_attr_list[0]));
	if (!dev)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)	{
		err = device_create_file(dev, pat9125_attr_list[idx]);
		if (err) {
			pr_debug(
				"device_create_file (%s) = %d\n",
				pat9125_attr_list[idx]->attr.name, err);
			break;
		}
	}

	return err;
}
/*----------------------------------------------------------------------------*/
static int pat9125_delete_attr(struct device *dev)
{

	int idx , err = 0;
	int num = (int)(sizeof(pat9125_attr_list)/sizeof(pat9125_attr_list[0]));
	if (!dev)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		device_remove_file(dev, pat9125_attr_list[idx]);

	return err;
}

static int pat9125_i2c_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = 0;
	struct device_node *np;

	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	pr_debug("%s (%d) : probe module....\n", __func__, __LINE__);

	memset(&pat9125data, 0, sizeof(pat9125data));
	err = i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE);
	if (err < 0)
		goto error_return;

	pat9125data.client = client;
	err = misc_register(&pat9125_device);
	if (err)	{
		pr_debug("pat9125_device register failed\n");
		goto error_return;
	}

	pat9125data.pat9125_device = pat9125_device.this_device;
	err = pat9125_create_attr(pat9125data.pat9125_device);
	if (err) {
		pr_debug("create attribute err = %d\n", err);
		goto error_return;
	}

	if (pat9125_init_input_data() < 0)
		goto error_return;

	/* interrupt initialization */
	pat9125data.i2c_dev = &client->dev;

	np = pat9125data.i2c_dev->of_node;
	pat9125data.irq_gpio = of_get_named_gpio_flags(np,
			"pixart_pat9125,irq-gpio", 0, &pat9125data.irq_flags);

	pr_debug(
		"irq_gpio: %d, irq_flags: 0x%x\n",
		pat9125data.irq_gpio, pat9125data.irq_flags);

	if (!gpio_is_valid(pat9125data.irq_gpio)) {
		err = (-1);
		pr_debug(
			"invalid irq_gpio: %d\n",
			pat9125data.irq_gpio);
		goto error_return;
	}

	err = gpio_request(pat9125data.irq_gpio, "pixart_pat9125_irq_gpio");
	if (err) {
		pr_debug(
			"unable to request gpio [%d], [%d]\n",
			pat9125data.irq_gpio, err);
		goto error_return;
	}

	err = gpio_direction_input(pat9125data.irq_gpio);
	if (err)	{
		pr_debug("unable to set dir for gpio[%d], [%d]\n",
		pat9125data.irq_gpio, err);
		goto error_return;
	}

	pat9125data.irq = gpio_to_irq(pat9125data.irq_gpio);

	if (!OTS_Sensor_Init())
		goto error_return;

	if (!pat9125_start())
			goto error_return;

	return 0;

error_return:

	return err;

}


static int pat9125_i2c_remove(struct i2c_client *client)
{

	return 0;
}

static int pat9125_suspend(struct device *dev)
{    pr_debug("%s (%d) : pat9125 suspend\n", __func__, __LINE__);
	return 0;
}

static int pat9125_resume(struct device *dev)
{
	 pr_debug("%s (%d) : pat9125 resume\n", __func__, __LINE__);
	return 0;
}

static const struct i2c_device_id pat9125_device_id[] = {
	{pat9125_DEV_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, pat9125_device_id);

static const struct dev_pm_ops pat9125_pm_ops = {
	.suspend = pat9125_suspend,
	.resume = pat9125_resume
};

static struct of_device_id pixart_pat9125_match_table[] = {
	{ .compatible = "pixart,pat9125",},
	{ },
};

static struct i2c_driver pat9125_i2c_driver = {
	.driver = {
		   .name = pat9125_DEV_NAME,
		   .owner = THIS_MODULE,
		   .pm = &pat9125_pm_ops,
		   .of_match_table = pixart_pat9125_match_table,
		   },
	.probe = pat9125_i2c_probe,
	.remove = pat9125_i2c_remove,
	.id_table = pat9125_device_id,
};
static int pat9125_open(struct input_dev *dev)
{
	 pr_debug(">>> %s (%d)\n", __func__, __LINE__);
	return 0;
}

static void pat9125_close(struct input_dev *dev)
{
	 pr_debug(">>> %s (%d)\n", __func__, __LINE__);
}

static int pat9125_init_input_data(void)
{
	int ret = 0;

	 pr_debug("%s (%d) : initialize data\n", __func__, __LINE__);

	pat9125data.pat9125_input_dev = input_allocate_device();

	if (!pat9125data.pat9125_input_dev) {
			pr_debug(
			"%s (%d) : could not allocate mouse input device\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	input_set_drvdata(pat9125data.pat9125_input_dev, &pat9125data);
	pat9125data.pat9125_input_dev->name = "Pixart pat9125";

	pat9125data.pat9125_input_dev->open = pat9125_open;
	pat9125data.pat9125_input_dev->close = pat9125_close;

	ret = input_register_device(pat9125data.pat9125_input_dev);
	if (ret < 0) {
		input_free_device(pat9125data.pat9125_input_dev);
		 pr_debug(
			"%s (%d) : could not register input device\n",
			__func__, __LINE__);
		return ret;
	}

	return 0;
}

static int __init pat9125_linux_init(void)
{
	 pr_debug("%s (%d) :init module\n", __func__, __LINE__);
	 pr_debug("Date : %s\n", __DATE__);
	 pr_debug("Time : %s\n", __TIME__);

	return i2c_add_driver(&pat9125_i2c_driver);
}




static void __exit pat9125_linux_exit(void)
{
	 pr_debug("%s (%d) : exit module\n", __func__, __LINE__);
	pat9125_stop();
	misc_register(&pat9125_device);
	pat9125_delete_attr(pat9125data.pat9125_device);
	i2c_del_driver(&pat9125_i2c_driver);
}


module_init(pat9125_linux_init);
module_exit(pat9125_linux_exit);
MODULE_AUTHOR("pixart");
MODULE_DESCRIPTION("pixart pat9125 driver");
MODULE_LICENSE("GPL");



