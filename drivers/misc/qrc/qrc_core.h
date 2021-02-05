/* SPDX-License-Identifier: GPL-2.0-only */
/* driver/misc/qrc/qrc_core.h
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _QRC_CORE_H
#define _QRC_CORE_H

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kfifo.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#define QRC_NAME_SIZE 30
#define QRC_INTERFACE_SIZE 30
#define QRC_FIFO_SIZE	0x1000

struct qrc_dev;

/* IOCTL commands */
#define QRC_IOC_MAGIC   'q'

/* Clear read fifo */
#define QRC_FIFO_CLEAR	_IO(QRC_IOC_MAGIC, 1)
/* Reboot QRC controller */
#define QRC_REBOOT	_IO(QRC_IOC_MAGIC, 2)
/* QRC boot from memory */
#define QRC_BOOT_TO_MEM	_IO(QRC_IOC_MAGIC, 3)
/* QRC boot from flash */
#define QRC_BOOT_TO_FLASH	_IO(QRC_IOC_MAGIC, 4)


enum qrcdev_state_t {
	__STATE_IDLE,
	__STATE_READING,
	__STATE_WRITING,
};

enum qrc_interface {
	UART = 0,
	SPI,
};

enum qrcdev_tx {
	__QRCDEV_TX_MIN	 = INT_MIN,	/* make sure enum is signed (-1)*/
	QRCDEV_TX_OK	 = 0x00,	/* driver took care of packet */
	QRCDEV_TX_BUSY	 = 0x10,	/* driver tx path was busy*/
};

struct qrc_device_stats {
	unsigned long	rx_bytes;
	unsigned long	tx_bytes;
	unsigned long	rx_errors;
	unsigned long	tx_errors;
	unsigned long	collisions;
	unsigned long	rx_length_errors;
	unsigned long	rx_over_errors;
	unsigned long	rx_fifo_errors;
};

struct qrc_device_ops {
	int		(*qrcops_init)(struct qrc_dev *dev);
	void		(*qrcops_uninit)(struct qrc_dev *dev);
	int		(*qrcops_open)(struct qrc_dev *dev);
	int		(*qrcops_close)(struct qrc_dev *dev);
	int		(*qrcops_setup)(struct qrc_dev *dev);
	enum qrcdev_tx	(*qrcops_xmit)(const char __user  *buf,
			size_t data_length, struct qrc_dev *dev);
	int		(*qrcops_receive)(struct qrc_dev *dev,
			char __user *buf, size_t count);
	int		(*qrcops_data_status)
			(struct qrc_dev *dev);
	int		(*qrcops_config)(struct qrc_dev *dev);
	void	(*qrcops_data_clean)(struct qrc_dev *dev);
};

/* qrc char device */
struct qrc_dev {
	struct qrc_device_stats stats;
	/* qrc dev ops */
	struct qrc_device_ops *qrc_ops;
	struct mutex mutex;
	wait_queue_head_t r_wait;
	wait_queue_head_t w_wait;
	struct fasync_struct *async_queue;
	struct cdev cdev;
	struct device *dev;
	void *data;
	int qrc_boot0_gpio;
	int qrc_reset_gpio;
};

/**
 * struct qrcuart - The qrcuart device structure.
 * @qrc_dev:  This is robotic controller device structure.
 *                    It include interface for qrcuart.
 * @lock: spinlock for transmitting lock.
 * @tx_work: Flushes transmit TX buffer.
 * @serdev: Serial device bus structure.
 * @qrc_rx_fifo: Qrcuart receive buffer.
 * @tx_head: String head in XMIT queue.
 * @tx_left: Bytes left in XMIT queue.
 * @tx_buffer: XMIT buffer.
 * This structure is used to define robotic controller uart device.
 */
struct qrcuart {
	struct qrc_dev *qrc_dev;
	spinlock_t lock;
	struct work_struct tx_work;
	struct serdev_device *serdev;
	struct kfifo qrc_rx_fifo;
	unsigned char *tx_head;
	int tx_left;
	unsigned char *tx_buffer;
};

struct qrcspi {
	struct qrc_dev *qrc_dev;
	spinlock_t lock;			/* transmit lock */
};

static inline void qrc_set_data(struct qrc_dev *dev, void *data)
{
	dev->data = data;
}
static inline void *qrc_get_data(const struct qrc_dev *dev)
{
	return dev->data;
}

int qrc_register_device(struct qrc_dev *qdev, struct device *dev);
void qrc_unregister(struct qrc_dev *qdev);

#endif

