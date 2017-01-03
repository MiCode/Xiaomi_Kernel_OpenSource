/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Code was copied and modified from
 * drivers/iio/imu/inv_mpu6050/inv_mpu_trigger.c
 *
 * Copyright (C) 2012 Invensense, Inc.
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

#include <linux/spi/spi.h>
#include <linux/sched/rt.h>
#include <linux/delay.h>
#include "inv_mpu9250_iio.h"

static const struct iio_trigger_ops inv_mpu9250_trigger_ops = {
	.owner = THIS_MODULE,
};

int inv_mpu9250_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct inv_mpu9250_state *st = iio_priv(indio_dev);

	st->trig = iio_trigger_alloc("%s-dev%d",
					indio_dev->name,
					indio_dev->id);
	if (st->trig == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	ret = request_irq(st->spi->irq, &iio_trigger_generic_data_rdy_poll,
				IRQF_TRIGGER_RISING,
				"inv_mpu",
				st->trig);
	if (ret)
		goto error_free_trig;

	st->trig->dev.parent = &st->spi->dev;
	st->trig->ops = &inv_mpu9250_trigger_ops;
	iio_trigger_set_drvdata(st->trig, indio_dev);
	ret = iio_trigger_register(st->trig);
	if (ret)
		goto error_free_irq;
	indio_dev->trig = st->trig;

	return 0;

error_free_irq:
	free_irq(st->spi->irq, st->trig);
error_free_trig:
	iio_trigger_free(st->trig);
error_ret:
	return ret;
}

void inv_mpu9250_remove_trigger(struct inv_mpu9250_state *st)
{
	iio_trigger_unregister(st->trig);
	free_irq(st->spi->irq, st->trig);
	iio_trigger_free(st->trig);
}
