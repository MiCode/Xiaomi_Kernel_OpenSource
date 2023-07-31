/*
 * SPI controller driver for the nordic52832 SoCs
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __APHOST_H__
#define __APHOST_H__

#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <asm-generic/gpio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/dma-buf.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/timekeeping.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>
#include <linux/of_irq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/ch11.h>
#include <linux/usb/hcd.h>
#include <linux/usb/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/timer.h>

#include <uapi/linux/sched/types.h>

#define MAX_PACK_SIZE 100 
#define MAX_DATA_SIZE 32
//#define MANUL_CONTROL_JOYSTICK_RLED

#define XFR_SIZE  190
/* Protocol commands to interact with firmware */
#define CMD_DATA_TAG  0xA6
#define CMD_CLR_BOND_TAG  0xA7
#define CMD_REQUEST_TAG   (0xA8)
#define CMD_EXTDATA_RLEDTAG  0xB6

typedef struct {
	uint64_t ts;
	uint32_t size;
	uint8_t data[MAX_DATA_SIZE];
} d_packet_t;

typedef struct {
	int8_t c_head;
	int8_t p_head;
	int8_t packDS;
	d_packet_t  data[MAX_PACK_SIZE];
} cp_buffer_t;

typedef enum _requestType_t
{
	getMasterNordicVersionRequest = 1,
	setVibStateRequest,
	bondJoyStickRequest,
	disconnectJoyStickRequest,
	getJoyStickBondStateRequest,
	hostEnterDfuStateRequest,
	getLeftJoyStickProductNameRequest,
	getRightJoyStickProductNameRequest,
	getLeftJoyStickFwVersionRequest,
	getRightJoyStickFwVersionRequest,
	setControllerSleepMode = 12,
	invalidRequest,
} requestType_t;

typedef struct _request_t
{
	struct _requestHead
	{
		unsigned char requestType:7;
		unsigned char needAck:1;  /* 1:need to ack 0:no need to ack */
	} requestHead;
	unsigned char requestData[3];
} __packed request_t;

typedef struct _acknowledge_t
{
	struct _acknowledgeHead
	{
		unsigned char requestType:7;
		unsigned char ack:1;  /* 1:ack 0:not ack */
	} acknowledgeHead;
	unsigned char acknowledgeData[3];
} __packed acknowledge_t;

struct jspinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *active;
	struct pinctrl_state *suspend;
};

struct js_spi_client {
	struct spi_device *spi_client;
	struct task_struct *kthread;
	struct mutex js_mutex; /* mutex for jsrequest node */
	struct mutex js_sm_mutex; /* dma alloc and free mutex */
	struct mutex js_rled_mutex; /* mutex for jsrled node */
	struct jspinctrl_info pinctrl_info;
	int js_irq_gpio;
	struct regulator *v1p8;
	int js_ledl_gpio; /* control left joyStick on and off */
	int js_ledr_gpio; /* control right joyStick on and off */
	int js_irq;
	atomic_t dataflag;
	atomic_t userRequest; /* request from userspace */
	atomic_t nordicAcknowledge; /* ack from nordic52832 master */
	unsigned char JoyStickBondState; /* 1:left JoyStick 2:right JoyStick */
	bool suspend;
	wait_queue_head_t  wait_queue;
	void   *vaddr;
	size_t vsize;
	struct dma_buf *js_buf;
	spinlock_t smem_lock;
	unsigned char txbuffer[255];
	unsigned char rxbuffer[255];
	uint64_t tsHost; /* linux boottime */
	unsigned char powerstate;
	bool    irqstate;
	unsigned char  js_ledl_state;
	unsigned char  js_ledr_state;
	int memfd;
	atomic_t urbstate;
};
#endif /* __APHOST_H__ */
