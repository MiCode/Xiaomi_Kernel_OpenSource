/*
 * Copyright (C) 2012-2017 InvenSense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _INV_COUNTERS_H_
#define _INV_COUNTERS_H_

#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>

#ifdef CONFIG_INV_TESTING

enum irqtype {
	IRQ_MPU,
	IRQ_ACCEL,
	IRQ_COMPASS
};

#define INV_I2C_INC_MPUREAD(x)		inv_iio_counters_mpuread(x)
#define INV_I2C_INC_MPUWRITE(x)		inv_iio_counters_mpuwrite(x)
#define INV_I2C_INC_ACCELREAD(x)	inv_iio_counters_accelread(x)
#define INV_I2C_INC_ACCELWRITE(x)	inv_iio_counters_accelwrite(x)
#define INV_I2C_INC_COMPASSREAD(x)	inv_iio_counters_compassread(x)
#define INV_I2C_INC_COMPASSWRITE(x)	inv_iio_counters_compasswrite(x)

#define INV_I2C_INC_TEMPREAD(x)		inv_iio_counters_tempread(x)

#define INV_I2C_SETIRQ(type, irq)	inv_iio_counters_set_i2cirq(type, irq)
#define INV_I2C_INC_COMPASSIRQ()	inv_iio_counters_compassirq()
#define INV_I2C_INC_ACCELIRQ()		inv_iio_counters_accelirq()

void inv_iio_counters_mpuread(int count);
void inv_iio_counters_mpuwrite(int count);
void inv_iio_counters_accelread(int count);
void inv_iio_counters_accelwrite(int count);
void inv_iio_counters_compassread(int count);
void inv_iio_counters_compasswrite(int count);

void inv_iio_counters_tempread(int count);

void inv_iio_counters_set_i2cirq(enum irqtype type, int irq);
void inv_iio_counters_compassirq(void);
void inv_iio_counters_accelirq(void);

#else

#define INV_I2C_INC_MPUREAD(x)
#define INV_I2C_INC_MPUWRITE(x)
#define INV_I2C_INC_ACCELREAD(x)
#define INV_I2C_INC_ACCELWRITE(x)
#define INV_I2C_INC_COMPASSREAD(x)
#define INV_I2C_INC_COMPASSWRITE(x)

#define INV_I2C_INC_TEMPREAD(x)

#define INV_I2C_SETIRQ(type, irq)
#define INV_I2C_INC_COMPASSIRQ()
#define INV_I2C_INC_ACCELIRQ()

#endif /* CONFIG_INV_TESTING */

#endif /* _INV_COUNTERS_H_ */

