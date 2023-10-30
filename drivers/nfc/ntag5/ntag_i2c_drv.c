

#include "ntag_common.h"

struct ntag_dev *ntag_dev = NULL;
void read_wire_data_test(void);

int ntag5_read_data(u8* buf, int count, int flags) {
	int ret;

	struct i2c_msg msg = {
		.addr = 0x54,
		.flags = flags | I2C_M_RD,
		.len = count,
		.buf = buf,
	};

	pr_info("%s count = %d: ntag5_read_data enter \n", __func__, count);
	pr_info("%s flags = %d: ntag5_read_data enter \n", __func__, flags);
	pr_info("%s buf = %d: ntag5_read_data enter \n", __func__, buf[0]);
	ret = i2c_transfer(ntag_dev->i2c_dev.client->adapter, &msg, 1);
	if (ret < 0) {
		pr_err("%s ret = %d: error!\n", __func__, ret);
	}

	pr_info("%s buf = %d: ntag5_read_data exit \n", __func__, buf[0]);
	return ret;
}

int ntag5_write_data(u8* buf, int count, int flags) {
	int ret;
	struct i2c_msg msg = {
		.addr = 0x54,
		.flags = flags,
		.len = count,
		.buf = buf,
	};

	pr_info("%s count = %d: ntag5_write_data enter \n", __func__, count);
	pr_info("%s flags = %d: ntag5_write_data enter \n", __func__, flags);
	pr_info("%s buf = %d: ntag5_write_data enter \n", __func__, buf[0]);


	ret = i2c_transfer(ntag_dev->i2c_dev.client->adapter, &msg, 1);
	if (ret < 0) {
		pr_err("%s ret = %d: error!\n", __func__,ret);
	}

	pr_info("%s buf = %d: ntag5_write_data exit \n", __func__, buf[0]);
	pr_info("%s ret = %d: ntag5_write_data exit Happy ending\n", __func__, ret);

	return ret;
}


int read_register(int add, u8 reg, u8 *val)
{
	u8 buffer[3];
	buffer[0] = add >> 8;
	buffer[1] = add & 0xFF;
	buffer[2] = reg;
	if (ntag5_write_data(buffer, 3, 0) != 3) {
		return 1;
	}

	if (ntag5_read_data(val, 1, 0) != 1) {
		return 1;
	}
	return 0;
}

int write_register(int add, char reg, char mask, char *val)
{
	u8 buffer[5];
	buffer[0] = add >> 8;
	buffer[1] = add & 0xFF;
	buffer[2] = reg;
	buffer[3] = mask;
	buffer[4] = val[0];
	if (ntag5_write_data(buffer, 5, 0) != 5) {
		return 1;
	}
	/* Wait for write operation completion */
	mdelay(10);
	return 0;
}

/**************************************************************************************************
** IN:     unsigned short iicBlockAddr    block number to be written
**         unsigned char *wtBuf           data to be written
**         unsigned char wtBuf_size       data buffer size
** NOTE    IMPORTANT - host shall wait for ~4ms after stop_condition to have eeprom programming finished
**         write 4 bytes to EEPROM or Configuration area
**************************************************************************************************/
unsigned char ntag5_i2c_blockWrite(unsigned short iicBlockAddr, unsigned char *wtBuf, unsigned char wtBuf_size)
{
	unsigned char status = 1;
	unsigned char buf[7];

	pr_info("%s : ntag5_i2c_blockWrite iicBlockAddr = %d!\n", __func__, iicBlockAddr);
	pr_info("%s : ntag5_i2c_blockWrite wtBuf_size = %d!\n", __func__, wtBuf_size);

	if (wtBuf_size < NTAG5_BLOCK_SIZE) {
		pr_err("%s : ntag5_i2c_blockWrite STATUS_BUFFER_SIZE_NOT_SUPPORT!\n", __func__);
		return STATUS_BUFFER_SIZE_NOT_SUPPORT;
	}

	// write one I2C Block: 1 + 4bytes
	buf[0] = iicBlockAddr >> 8;	// block address: memAddr H
	buf[1] = (unsigned char)iicBlockAddr;	// block address: memAddr L
	memcpy(&buf[2], wtBuf, 4);	// data to be written

	status = ntag5_write_data(buf, 6, 0);
	pr_info("%s : ntag5_i2c_blockWrite status = %d!\n", __func__, status);

	//host shall wait for ~4ms after stop_condition
	mdelay(4);

	return status;
}

/**************************************************************************************************
** IN:          unsigned short iicBlockAddr     starting block number to be read
**              unsigned char nrOfBlocks        number of blocks to be read, 4 bytes per block
**              unsigned char *rdBuf            buffer to store read data
**              unsigned short rdBuf_size       buffer size
** OUT:         ntag5 will continuely send back data unless receive NACK or Stop from Host
** NOTE         make sure the read buffer does not overflow
**************************************************************************************************/
unsigned char ntag5_i2c_blockRead(unsigned short iicBlockAddr, unsigned short nrOfBlocks, unsigned char *rdBuf, unsigned short rdBuf_size)
{
	unsigned char status = 1;
	unsigned char addr[2];

	pr_info("%s : ntag5_i2c_blockRead iicBlockAddr = %d!\n", __func__, iicBlockAddr);
	pr_info("%s : ntag5_i2c_blockRead nrOfBlocks = %d!\n", __func__, nrOfBlocks);
	pr_info("%s : ntag5_i2c_blockRead rdBuf_size = %d!\n", __func__, rdBuf_size);

	if (rdBuf_size < NTAG5_BLOCK_SIZE * nrOfBlocks) {
		pr_err("%s : ntag5_i2c_blockRead STATUS_BUFFER_SIZE_NOT_SUPPORT!\n", __func__);
		return STATUS_BUFFER_SIZE_NOT_SUPPORT;
	}

	addr[0] = iicBlockAddr >> 8;	// addr High
	addr[1] = (unsigned char)iicBlockAddr;

	// write Block Address: 2bytes
	status = ntag5_write_data(addr, 2, 0);
	if (status)
	{
		// read block data, total bytes = nrOfBlocks * 4
		status = ntag5_read_data(rdBuf, nrOfBlocks * 4, 0);
		if (true) {
			pr_err("%s : ntag5_i2c_blockRead status = %d!\n", __func__, status);
		}
	}

	mdelay(4);
	return status;
}

int i2c_disable_irq()
{
	unsigned long flags;
	pr_info("%s : i2c_disable_irq enter  !\n", __func__);
	spin_lock_irqsave(&ntag_dev->i2c_dev.irq_enabled_lock, flags);
	if (ntag_dev->i2c_dev.irq_enabled) {
		disable_irq_nosync(ntag_dev->i2c_dev.client->irq);
		ntag_dev->i2c_dev.irq_enabled = false;
		pr_err("%s : i2c_disable_irq success !\n", __func__);
	}
	spin_unlock_irqrestore(&ntag_dev->i2c_dev.irq_enabled_lock, flags);
	pr_info("%s : i2c_disable_irq exit   !\n", __func__);
	return 0;
}

/**
 * i2c_enable_irq()
 *
 * Check if interrupt is enabled or not
 * and enable interrupt
 *
 * Return: int
 */
int i2c_enable_irq()
{
	unsigned long flags;
	pr_info("%s : i2c_enable_irq enter  !\n", __func__);
	spin_lock_irqsave(&ntag_dev->i2c_dev.irq_enabled_lock, flags);
	if (!ntag_dev->i2c_dev.irq_enabled) {
		ntag_dev->i2c_dev.irq_enabled = true;
		enable_irq(ntag_dev->i2c_dev.client->irq);
		pr_err("%s : i2c_enable_irq success !\n", __func__);

	}
	spin_unlock_irqrestore(&ntag_dev->i2c_dev.irq_enabled_lock, flags);
	pr_info("%s : i2c_enable_irq exit   !\n", __func__);
	return 0;
}


int i2c_read(struct ntag_dev *ntag_dev, char *buf, size_t count, int timeout)
{
	int ret= 0;
	return ret;
}

ssize_t nfc_i2c_dev_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *offset)
{
	int ret = 0;
	return ret;
}

ssize_t nfc_i2c_dev_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *offset)
{
	int ret= 0;

	return ret;
}

static irqreturn_t i2c_irq_handler(int irq, void *dev_id)
{

	unsigned long flags;
	struct ntag_dev *ntag_dev = dev_id;
	spin_lock_irqsave(&ntag_dev->i2c_dev.irq_enabled_lock, flags);
	if(ntag_dev->fasync_queue) {
		/* 在中断服务函数中向应用层发送SIGIO信号-异步通知 */
		pr_info("%s kill_fasync, inform upper layer", __func__);
		kill_fasync(&(ntag_dev->fasync_queue), SIGIO, POLL_IN);
	}
	spin_unlock_irqrestore(&ntag_dev->i2c_dev.irq_enabled_lock, flags);
	wake_up(&ntag_dev->read_wq);
	return IRQ_HANDLED;
}

int ntag5_irq_fasync(int fd, struct file *file, int on)
{
	int ret = 0;
	// struct ntag_dev *ntag_dev = (struct ntag_dev *)file->private_data;
	// struct ntag_dev *ntag_dev = file->private_data;

	if (!ntag_dev) {
		pr_info("Ntag5_dev_Error! \n");
		return -ENODEV;
	}

	/*将该设备登记到fasync_queue队列中去*/
	pr_info("%s  enter register with the fasync_queue", __func__);
	ret = fasync_helper(fd, file, on, &(ntag_dev->fasync_queue));
	if (ret<0) {
		pr_err("fasync_helper failed! \n");
		return ret;
	}
	return 0;
}

int ntag5_fasync_release(struct inode *inode, struct file *filp)
{
	return ntag5_irq_fasync(-1, filp, 0);
}

static const struct file_operations ntag_i2c_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = nfc_i2c_dev_read,
	.write = nfc_i2c_dev_write,
	.fasync = ntag5_irq_fasync,
	.release = ntag5_fasync_release,
};

void read_wire_data_test()
{
	int count = 10;

	int ret = -1;

	while (count--) {
		i2c_enable_irq();
		if (gpio_get_value(ntag_dev->configs.gpio.pu)) {
			ret = wait_event_interruptible(ntag_dev->read_wq,
				!ntag_dev->i2c_dev.irq_enabled);
			if (ret) {
				pr_err("%s unexpeted wakeup of read wq, try wait again.\n", __func__);
				return;
			}
		}
		if (!gpio_get_value(ntag_dev->configs.gpio.pu)) {
			pr_err("%s : gpio_get_value = 0!\n", __func__);
			break;
		}
		i2c_disable_irq();
		pr_err("%s unexpeted wakeup of read wq, try wait again, count = %d\n", __func__, count);
	}

}

int nfc_i2c_dev_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	int ret = 0;
	unsigned char wtBuf[64] = {0};
	unsigned char rdBuf[256] = {0};
	struct i2c_dev *i2c_dev = NULL;
	struct platform_configs nfc_configs;
	struct platform_gpio *nfc_gpio = &nfc_configs.gpio;

	pr_info("Enter the probe function, %s !\n", __func__);

	ret = nfc_parse_dt(&client->dev, &nfc_configs);
	if (ret) {
		pr_err("%s : failed to parse dt\n", __func__);
		goto err;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto err;
	}

	ntag_dev = devm_kzalloc(&client->dev, sizeof(struct ntag_dev), GFP_KERNEL);
	if (ntag_dev == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	// init the structure
	ntag_dev->nfc_state = NTAG5_DEFAULT_STATE;
	ntag_dev->i2c_dev.client = client;
	i2c_dev = &ntag_dev->i2c_dev;
	ntag_dev->nfc_device = &client->dev;

	ntag_dev->nfc_enable_intr = i2c_enable_irq;
	ntag_dev->nfc_disable_intr = i2c_disable_irq;
	init_waitqueue_head(&ntag_dev->read_wq);

	//initial the pinctrl system
	ret = ntag5_gpio_init(ntag_dev);
	pr_info("GPIO initialize of the ntag5__ %d:\n", ret);

	ret = configure_gpio(nfc_gpio->pu, GPIO_IRQ);
	if (ret < 0) {
		pr_err("%s: unable to request nfc irq gpio [%d]\n",
			__func__, nfc_gpio->pu);
		goto err_free_pu;
	}

	client->irq = ret;
	spin_lock_init(&i2c_dev->irq_enabled_lock);
	i2c_dev->irq_enabled = true;
	ret = request_irq(client->irq, i2c_irq_handler,
			  IRQF_TRIGGER_FALLING, client->name, ntag_dev);
	if (ret) {
		pr_err("%s: request_irq failed\n", __func__);
		goto err;
	}

	i2c_enable_irq();

	i2c_set_clientdata(client, ntag_dev);

	pr_info("nfc_i2c_dev_probe  client->irq = %d:\n", client->irq);

	ret = configure_gpio(nfc_gpio->hpd, GPIO_HPD);
	if (ret < 0) {
		pr_err("%s: unable to request nfc hpd gpio [%d]\n",
			__func__, nfc_gpio->hpd);
		goto err_free_hpd;
	}

	/*copy the retrieved gpio details from DT */
	memcpy(&ntag_dev->configs, &nfc_configs, sizeof(struct platform_configs));

	// register
	ret = nfc_misc_register(ntag_dev, &ntag_i2c_dev_fops, DEV_COUNT,
				NFC_CHAR_DEV_NAME, CLASS_NAME);
	if (ret) {
		pr_err("%s: nfc_misc_register failed\n", __func__);
		goto err;
	}

	// change ED config to "Field Present"
	read_register(0x10A8, 0, rdBuf);
	wtBuf[0] = 0x1;
	write_register(0x10A8, 0, 0xFF, wtBuf);
	pr_info("%s success\n", __func__);

	return 0;

err_free_pu:
	gpio_free(nfc_gpio->pu);
err_free_hpd:
	gpio_free(nfc_gpio->hpd);
err:
	pr_err("%s: failed\n", __func__);

	return ret;

}

int nfc_i2c_dev_remove(struct i2c_client *client)
{
	int ret = 0;
	pr_info("nfc_i2c_dev_remove NTAG5 I2C driver\n");
	ntag_dev = i2c_get_clientdata(client);
	if (!ntag_dev) {
		pr_err("%s: device doesn't exist anymore\n", __func__);
		ret = -ENODEV;
		return ret;
	}

	free_irq(client->irq, ntag_dev);
	nfc_misc_unregister(ntag_dev,DEV_COUNT);

	if (gpio_is_valid(ntag_dev->configs.gpio.hpd)) {
		gpio_free(ntag_dev->configs.gpio.hpd);
	}

	if (gpio_is_valid(ntag_dev->configs.gpio.pu)) {
		gpio_free(ntag_dev->configs.gpio.pu);
	}

	kfree(ntag_dev);
	ntag_dev = NULL;

	return ret;
}

static const struct of_device_id nfc_i2c_dev_match_table[] = {
	{.compatible = NTAG5_I2C_DRV_STR,},
	{}
};

static struct i2c_driver nfc_i2c_dev_driver = {
	.probe = nfc_i2c_dev_probe,
	.remove = nfc_i2c_dev_remove,
	.driver = {
		.name = NTAG5_I2C_DRV_STR,
		.of_match_table = nfc_i2c_dev_match_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};

static int __init my_i2c_drv_init(void)
{

	int ret = 0;
	pr_info("loading NTAG5 I2C driver\n");
	ret = i2c_add_driver(&nfc_i2c_dev_driver);
	if (ret != 0) {
		pr_err("NFC I2C add driver error ret %d\n", ret);
	}
	return ret;
}

module_init(my_i2c_drv_init);

static void __exit my_i2c_drv_exit(void) {

	pr_info("my_i2c_drv_exit NTAG5 I2C driver\n");
	i2c_del_driver(&nfc_i2c_dev_driver);
	return;
}

module_exit(my_i2c_drv_exit);

MODULE_DESCRIPTION("NFC NTAG5 I2C driver");
MODULE_LICENSE("GPL v2");