/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _NFC_I2C_DRV_H_
#define _NFC_I2C_DRV_H_
#include <linux/i2c.h>

#define NFC_I2C_DRV_STR   "qcom,sn-nci"	/*kept same as dts */
#define NFC_I2C_DEV_ID		"sn-i2c"

//Interface specific parameters
struct i2c_dev {
	struct i2c_client *client;
	// IRQ parameters
	bool irq_enabled;
	spinlock_t irq_enabled_lock;
	// NFC_IRQ wake-up state
	bool irq_wake_up;
};
long nfc_i2c_dev_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg);
int nfc_i2c_dev_probe(struct i2c_client *client,
		      const struct i2c_device_id *id);
int nfc_i2c_dev_remove(struct i2c_client *client);
int nfc_i2c_dev_suspend(struct device *device);
int nfc_i2c_dev_resume(struct device *device);

#ifdef CONFIG_NFC_QTI_I2C

void i2c_enable_irq(struct i2c_dev *i2c_dev);
void i2c_disable_irq(struct i2c_dev *i2c_dev);
int i2c_write(struct i2c_dev *i2c_dev, char *buf, size_t count,
						int max_retry_cnt);
int i2c_read(struct i2c_dev *i2c_dev, char *buf, size_t count);

#else

static inline void i2c_enable_irq(struct i2c_dev *i2c_dev)
{
}

static inline void i2c_disable_irq(struct i2c_dev *i2c_dev)
{
}

static inline int i2c_write(struct i2c_dev *i2c_dev, char *buf, size_t count,
						int max_retry_cnt)
{
	return -ENXIO;
}

static inline int i2c_read(struct i2c_dev *i2c_dev, char *buf, size_t count)
{
	return -ENXIO;
}

#endif

#endif //_NFC_I2C_DRV_H_
