/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023-2028, The Linux Foundation. All rights reserved.
 */

#ifndef _NTAG_COMMON_H_
#define _NTAG_COMMON_H_

#include <linux/types.h>
#include <linux/version.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/ipc_logging.h>
#include "ntag_i2c_drv.h"

// Max device count for this driver
#define DEV_COUNT	1

// NTAG device class
#define CLASS_NAME	"ntag"

// NTAG character device name, this will be in /dev/
#define NTAG_CHAR_DEV_NAME		"fm-ntag"

#define DTS_HPD_GPIO_STR		"qcom,ntag-hpd"
#define DTS_IRQ_GPIO_STR		"qcom,ntag-irq"
#define NTAG_LDO_SUPPLY_DT_NAME		"qcom,ntag-vdd"
#define NTAG_LDO_SUPPLY_NAME		"qcom,ntag-vdd-supply"
#define NTAG_LDO_VOL_DT_NAME		"qcom,ntag-vdd-voltage"
#define NTAG_LDO_CUR_DT_NAME		"qcom,ntag-vdd-current"

#define NTAG_VDDIO_MIN			3300000 //in uV
#define NTAG_VDDIO_MAX			3300000 //in uV
#define NTAG_CURRENT_MAX		100000  //in uA

#define WAKEUP_SRC_TIMEOUT		(2000)
#define MAX_BUFFER_SIZE			(256)
#define NTAG_BLOCK_SIZE			(4)

#define CHIP_CTRL_REG			0xFFE0
#define LOW_POWER_CFG_REG		0xFFE1
#define MASK_MAIN_IRQ_REG		0xFFFA
#define MASK_FIFO_IRQ_REG		0xFFFB
#define MASK_AUX_IRQ_REG		0xFFFC
#define IRQ_CFG_REG			0xFFFD

#define LOW_POWER_CFG_REG_EE		0x01C8
#define MASK_MAIN_IRQ_REG_EE		0x01DA
#define MASK_FIFO_IRQ_REG_EE		0x01DB
#define MASK_AUX_IRQ_REG_EE		0x01DC
#define IRQ_CFG_REG_EE			0x01DD

#define CHIP_CTRL_REG_VAL		0x0C  // bit3: Soft_Reset, bit2: CFG_Reload
#define LOW_POWER_CFG_REG_VAL		0x09  // bit3: AutoStandby enable, bit0: timeout 15ms
#define MASK_MAIN_IRQ_REG_VAL		0x3E  // RF_On + RF_Active + Aux
#define MASK_AUX_IRQ_REG_VAL		0xBF  // RF_Off
#define IRQ_CFG_REG_VAL			0x02  // 读写均可清除中断, 中断有效极性取反，开漏输出

#define MAIN_IRQ_REG			0xFFF7
#define FIFO_IRQ_REG			0xFFF8
#define AUX_IRQ_REG			0xFFF9

#define IRQ_CLEAR_DELAY			150 // ms

/* Enum for GPIO values*/
enum gpio_values {
	GPIO_INPUT = 0x0,
	GPIO_OUTPUT = 0x1,
	GPIO_HIGH = 0x2,
	GPIO_OUTPUT_HIGH = 0x3,
	GPIO_IRQ = 0x4,
};

// NTAG GPIO variables
struct platform_gpio {
	unsigned int irq;
	unsigned int hpd;
};

// NTAG LDO entries from DT
struct platform_ldo {
	int vdd_levels[2];
	int max_current;
};

// NTAG Struct to get all the required configs from DTS
struct platform_configs {
	struct platform_gpio gpio;
	struct platform_ldo ldo;
};

/* Device specific structure */
struct ntag_dev {
	struct mutex read_mutex;
	struct mutex write_mutex;
	uint8_t *read_kbuf;
	uint8_t *write_kbuf;

	struct mutex dev_ref_mutex;
	unsigned int dev_ref_count;

	dev_t devno;
	struct class *ntag_class;
	struct cdev c_dev;
	struct device *ntag_device;

	struct i2c_dev i2c_dev;
	struct platform_configs configs;

	struct regulator *reg;
	bool is_vreg_enabled;

	/* 异步通知队列 */
	struct fasync_struct *fasync_queue;

	struct delayed_work clear_irq_work;

	int (*ntag_read)(struct ntag_dev *dev, char *buf, size_t count);
	int (*ntag_write)(struct ntag_dev *dev, const char *buf, const size_t count);
	int (*ntag_enable_intr)(struct ntag_dev *dev);
	int (*ntag_disable_intr)(struct ntag_dev *dev);
};

int ntag_parse_dt(struct device *dev, struct platform_configs *ntag_configs);
int configure_gpio(unsigned int gpio, int flag);

int ntag_misc_register(struct ntag_dev *ntag_dev,
		const struct file_operations *ntag_fops, int count, char *devname,
		char *classname);
void ntag_misc_unregister(struct ntag_dev *ntag_dev, int count);

int ntag_ldo_config(struct device *dev, struct ntag_dev *ntag_dev);
int ntag_ldo_vote(struct ntag_dev *ntag_dev);
int ntag_ldo_unvote(struct ntag_dev *ntag_dev);

int ntag_dev_open(struct inode *inode, struct file *filp);
int ntag_dev_close(struct inode *inode, struct file *filp);
int ntag_irq_fasync(int fd, struct file *file, int on);

int ntag_init(struct ntag_dev *ntag_dev);
void ntag_clear_irq_register(struct ntag_dev *ntag_dev);
#endif //_NTAG_COMMON_H_
