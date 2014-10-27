/*
 * i2c-wcove-pmic.c: Whiskey Cove PMIC I2C adapter driver.
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Yegnesh Iyer <yegnesh.s.iyer@intel.com>
 */

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/mfd/intel_soc_pmic.h>

#define DRIVER_NAME "wcove_pmic_i2c"

#define D7 (1 << 7)
#define D6 (1 << 6)
#define D5 (1 << 5)
#define D4 (1 << 4)
#define D3 (1 << 3)
#define D2 (1 << 2)
#define D1 (1 << 1)
#define D0 (1 << 0)


#define I2C_MSG_LEN		4

#define I2COVRCTRL_ADDR		0x5E24
#define I2COVRDADDR_ADDR	0x5E25
#define I2COVROFFSET_ADDR	0x5E26
#define I2COVRWRDATA_ADDR	0x5E27
#define I2COVRRDDATA_ADDR	0x5E28

#define MCHGRIRQ0_ADDR		0x6E17

#define PMIC_I2C_INTR_MASK	(D3|D2|D1)
#define I2COVRCTRL_I2C_RD	D1
#define I2COVRCTRL_I2C_WR	D0
#define CHGRIRQ0_ADDR		0x6E0A

#define IRQ0_I2C_BIT_POS	 1

struct pmic_i2c_dev {
	int irq;
	u32 pmic_intr_sram_addr;
	struct i2c_adapter adapter;
	int i2c_rw;
	wait_queue_head_t i2c_wait;
	struct mutex i2c_pmic_rw_lock;
	struct device *dev;
};

enum I2C_STATUS {
	I2C_WR = 1,
	I2C_RD,
	I2C_NACK = 4
};

static struct pmic_i2c_dev *pmic_dev;
struct i2c_adapter *wcove_pmic_i2c_adapter;
EXPORT_SYMBOL(wcove_pmic_i2c_adapter);

static irqreturn_t pmic_thread_handler(int id, void *data)
{
	u8 irq0_int;

	irq0_int = intel_soc_pmic_readb(CHGRIRQ0_ADDR);
	pmic_dev->i2c_rw = (irq0_int >> IRQ0_I2C_BIT_POS);
	wake_up(&(pmic_dev->i2c_wait));
	return IRQ_HANDLED;
}

/* PMIC i2c read msg */
static inline int pmic_i2c_read_xfer(struct i2c_msg msg)
{
	int ret;
	u16 i;
	u8 mask = (I2C_RD | I2C_NACK);

	for (i = 0; i < msg.len ; i++) {
		pmic_dev->i2c_rw = 0;
		ret = intel_soc_pmic_writeb(I2COVRDADDR_ADDR, msg.addr);
		if (ret)
			return ret;
		ret = intel_soc_pmic_writeb
				(I2COVROFFSET_ADDR, msg.buf[0] + i);
		if (ret)
			return  ret;

		ret =  intel_soc_pmic_writeb
				(I2COVRCTRL_ADDR, I2COVRCTRL_I2C_RD);
		if (ret)
			return ret;

		ret = wait_event_timeout(pmic_dev->i2c_wait,
				(pmic_dev->i2c_rw & mask),
				HZ);

		if (ret == 0) {
			return -ETIMEDOUT;
		} else if (pmic_dev->i2c_rw == I2C_NACK) {
			return  -EIO;
		} else {
			msg.buf[i] = intel_soc_pmic_readb(I2COVRRDDATA_ADDR);
			if (msg.buf[i] < 0)
				return -EIO;
		}
	}
	return 0;
}

/* PMIC i2c write msg */
static inline int pmic_i2c_write_xfer(struct i2c_msg msg)
{
	int ret;
	u16 i;
	u8 mask = (I2C_WR | I2C_NACK);

	for (i = 1; i <= msg.len ; i++) {
		pmic_dev->i2c_rw = 0;
		ret = intel_soc_pmic_writeb(I2COVRDADDR_ADDR, msg.addr);
		if (ret)
			return ret;

		ret = intel_soc_pmic_writeb
				(I2COVRWRDATA_ADDR, msg.buf[i]);
		if (ret)
			return ret;

		ret = intel_soc_pmic_writeb
			(I2COVROFFSET_ADDR, msg.buf[0] + i - 1);
		if (ret)
			return ret;

		ret = intel_soc_pmic_writeb
			(I2COVRCTRL_ADDR, I2COVRCTRL_I2C_WR);
		if (ret)
			return ret;

		ret = wait_event_timeout(pmic_dev->i2c_wait,
				(pmic_dev->i2c_rw & mask),
				HZ);
		if (ret == 0)
			return -ETIMEDOUT;
		else if (pmic_dev->i2c_rw == I2C_NACK)
			return -EIO;
	}
	return 0;
}

static int (*xfer_fn[]) (struct i2c_msg) = {
	pmic_i2c_write_xfer,
	pmic_i2c_read_xfer
};

/* PMIC I2C Master transfer algorithm function */
static int pmic_master_xfer(struct i2c_adapter *adap,
				struct i2c_msg msgs[],
				int num)
{
	int ret = 0;
	int i;
	u8 index;

	mutex_lock(&pmic_dev->i2c_pmic_rw_lock);
	for (i = 0 ; i < num ; i++) {
		index = msgs[i].flags & I2C_M_RD;
		ret = (xfer_fn[index])(msgs[i]);

		if (ret == -EACCES)
			dev_info(pmic_dev->dev, "Blocked Access!\n");

		/* If access is restricted, return true to
		*  avoid extra error handling in client
		*/

		if (ret != 0 && ret != -EACCES)
			goto transfer_err_exit;
	}

	ret = num;

transfer_err_exit:
	mutex_unlock(&pmic_dev->i2c_pmic_rw_lock);
	return ret;
}

/* PMIC I2C adapter capability function */
static u32 pmic_master_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA;
}

static int pmic_smbus_xfer(struct i2c_adapter *adap, u16 addr,
				unsigned short flags, char read_write,
				u8 command, int size,
				union i2c_smbus_data *data)
{
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	msg.addr = addr;
	msg.flags = flags & I2C_M_TEN;
	msg.buf = buf;
	msg.buf[0] = command;
	if (read_write == I2C_SMBUS_WRITE) {
		msg.len = 1;
		msg.buf[1] = data->byte;
	} else {
		msg.flags |= I2C_M_RD;
		msg.len = 1;
	}

	ret = pmic_master_xfer(adap, &msg, 1);
	if (ret == 1) {
		if (read_write == I2C_SMBUS_READ)
			data->byte = msg.buf[0];
		return 0;
	}
	return ret;
}


static const struct i2c_algorithm pmic_i2c_algo = {
	.master_xfer = pmic_master_xfer,
	.functionality = pmic_master_func,
	.smbus_xfer = pmic_smbus_xfer,
};

static int pmic_i2c_probe(struct platform_device *pdev)
{
	int ret;

	pmic_dev = kzalloc(sizeof(struct pmic_i2c_dev), GFP_KERNEL);
	if (!pmic_dev)
		return -ENOMEM;

	pmic_dev->dev = &pdev->dev;
	pmic_dev->irq = platform_get_irq(pdev, 0);

	mutex_init(&pmic_dev->i2c_pmic_rw_lock);
	init_waitqueue_head(&(pmic_dev->i2c_wait));

	ret = request_threaded_irq(pmic_dev->irq, NULL,
					pmic_thread_handler, IRQF_ONESHOT,
					DRIVER_NAME, pmic_dev);
	if (ret)
		goto err_irq_request;

	ret = intel_soc_pmic_update(MCHGRIRQ0_ADDR, 0x00,
			PMIC_I2C_INTR_MASK);
	if (unlikely(ret))
		goto unmask_irq_failed;

	wcove_pmic_i2c_adapter = &pmic_dev->adapter;
	wcove_pmic_i2c_adapter->owner = THIS_MODULE;
	wcove_pmic_i2c_adapter->class = I2C_CLASS_HWMON;
	wcove_pmic_i2c_adapter->algo = &pmic_i2c_algo;
	strcpy(wcove_pmic_i2c_adapter->name, "PMIC I2C Adapter");
	wcove_pmic_i2c_adapter->nr = pdev->id;
	ret = i2c_add_numbered_adapter(wcove_pmic_i2c_adapter);

	if (ret) {
		dev_err(&pdev->dev, "Error adding the adapter\n");
		goto err_adap_add;
	}

	return 0;

err_adap_add:
unmask_irq_failed:
	free_irq(pmic_dev->irq, pmic_dev);
err_irq_request:
	kfree(pmic_dev);
	return ret;
}

static int pmic_i2c_remove(struct platform_device *pdev)
{
	free_irq(pmic_dev->irq, pmic_dev);
	kfree(pmic_dev);
	return 0;
}

struct platform_driver pmic_i2c_driver = {
	.probe = pmic_i2c_probe,
	.remove = pmic_i2c_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};
module_platform_driver(pmic_i2c_driver);

MODULE_AUTHOR("Yegnesh Iyer <yegnesh.s.iyer@intel.com");
MODULE_DESCRIPTION("WCove PMIC I2C Master driver");
MODULE_LICENSE("GPL");
