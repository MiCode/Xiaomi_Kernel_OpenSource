/*
 *  stmvl53l0x.h - Linux kernel modules for
 *  STM VL53L0 FlightSense TOF sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division.
 *  Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef STMVL_H
#define STMVL_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>


#define STMVL_DRV_NAME	"stmvl53l0"
#define STMVL_SLAVE_ADDR	(0x52>>1)

#define DRIVER_VERSION		"1.0.5"
#define I2C_M_WR			0x00
/* #define INT_POLLING_DELAY	20 */

/* if don't want to have output from dbg, comment out #DEBUG macro */
#define DEBUG
#ifdef DEBUG
#define dbg(fmt, ...)	\
	printk(fmt, ##__VA_ARGS__)
#else
#define dbg(fmt, ...)
#endif

#define err(fmt, ...) \
	printk(fmt, ##__VA_ARGS__)

#define VL_VDD_MIN      2600000
#define VL_VDD_MAX      3000000

enum init_mode_e {
	NORMAL_MODE = 0,
	OFFSETCALIB_MODE = 1,
	XTALKCALIB_MODE = 2,
	SPADCALIB_MODE = 3,
	REFCALIB_MODE = 4,
};

enum parameter_name_e {
	OFFSET_PAR = 0,
	XTALKRATE_PAR = 1,
	XTALKENABLE_PAR = 2,
	GPIOFUNC_PAR = 3,
	LOWTHRESH_PAR = 4,
	HIGHTHRESH_PAR = 5,
	DEVICEMODE_PAR = 6,
	INTERMEASUREMENT_PAR = 7,
	REFERENCESPADS_PAR = 8,
	REFCALIBRATION_PAR = 9,
};

enum {
	CCI_BUS = 0,
	I2C_BUS = 1,
};

/*
 *  IOCTL register data structs
 */
struct stmvl53l0x_register {
	uint32_t is_read; /*1: read 0: write*/
	uint32_t reg_index;
	uint32_t reg_bytes;
	uint32_t reg_data;
	int32_t status;
};

/*
 *  IOCTL parameter structs
 */
struct stmvl53l0x_parameter {
	uint32_t is_read; /*1: Get 0: Set*/
	enum parameter_name_e name;
	int32_t value;
	int32_t value2;
	int32_t status;
};

/*
 *  driver data structs
 */
struct vl_data {

	struct VL_DevData_t Data; /* !<embed ST VL53L0 Dev data as "dev_data" */
	uint8_t   I2cDevAddr;	/* !< i2c device address user specific field */
	uint8_t   comms_type;	/* !< Type of comms : */
				/* VL_COMMS_I2C or VL_COMMS_SPI */
	uint16_t  comms_speed_khz;	/*!< Comms speed [kHz] : */
					/*typically 400kHz for I2C */
	uint8_t   bus_type;		/* CCI_BUS; I2C_BUS */

	void *client_object; /* cci or i2c client */

	struct mutex update_lock;
	struct delayed_work	dwork;		/* for PS  work handler */
	struct input_dev *input_dev_ps;
	struct kobject *range_kobj;

	const char *dev_name;
	/* function pointer */

	/* misc device */
	struct miscdevice miscdev;

	int irq;
	unsigned int reset;

	/* control flag from HAL */
	unsigned int enable_ps_sensor;

	/* PS parameters */
	unsigned int ps_data;			/* to store PS data */

	/* Calibration parameters */
	unsigned int offsetCalDistance;
	unsigned int xtalkCalDistance;


	/* Range Data */
	struct VL_RangingMeasurementData_t rangeData;

	/* Device parameters */
	uint8_t			deviceMode;
	uint32_t				interMeasurems;
	uint8_t gpio_function;
	uint8_t gpio_polarity;
	unsigned int low_threshold;
	unsigned int high_threshold;

	/* delay time in miniseconds*/
	uint8_t delay_ms;

	/* Timing Budget */
	uint32_t	timingBudget;
	/* Use this threshold to force restart ranging */
	uint32_t	noInterruptCount;
	/* Use this flag to use long ranging*/
	int			  useLongRange;

	/* Polling thread */
	struct task_struct *poll_thread;
	/* Wait Queue on which the poll thread blocks */
	wait_queue_head_t poll_thread_wq;

	/* Recent interrupt status */
	uint32_t		interruptStatus;

	struct mutex work_mutex;

	/* Debug */
	unsigned int enableDebug;
	uint8_t interrupt_received;
	int32_t default_offset_calibration;
	unsigned int default_xtalk_Compensation;
};

/*
 *  function pointer structs
 */
struct stmvl53l0x_module_fn_t {
	int (*init)(void);
	void (*deinit)(void *);
	int (*power_up)(void *, unsigned int *);
	int (*power_down)(void *);
};



int stmvl53l0x_setup(struct vl_data *data);

#endif /* STMVL_H */
