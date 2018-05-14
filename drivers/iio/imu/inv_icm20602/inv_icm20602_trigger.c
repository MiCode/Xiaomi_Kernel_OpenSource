/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/spi/spi.h>
#include <linux/sched/rt.h>
#include <linux/delay.h>
#include "inv_icm20602_iio.h"
#include <linux/i2c.h>

static const struct iio_trigger_ops inv_icm20602_trigger_ops = {
	.owner = THIS_MODULE,
};

int inv_icm20602_validate_trigger(struct iio_dev *indio_dev,
					struct iio_trigger *trig)
{
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	if (st->trig != trig)
		return -EINVAL;

	return MPU_SUCCESS;
}

int inv_icm20602_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct inv_icm20602_state *st = iio_priv(indio_dev);

	st->trig = iio_trigger_alloc("%s-dev%d",
					indio_dev->name,
					indio_dev->id);
	if (st->trig == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	ret = request_irq(st->client->irq, &iio_trigger_generic_data_rdy_poll,
				IRQF_TRIGGER_RISING,
				"inv_icm20602",
				st->trig);
	if (ret) {
		dev_dbgerr("request_irq\n");
		goto error_free_trig;
	}
	st->trig->dev.parent = &st->client->dev;
	st->trig->ops = &inv_icm20602_trigger_ops;
	iio_trigger_set_drvdata(st->trig, indio_dev);
	ret = iio_trigger_register(st->trig);
	if (ret) {
		dev_dbgerr("iio_trigger_register\n");
		goto error_free_irq;
	}
	indio_dev->trig = st->trig;

	return 0;

error_free_irq:
	free_irq(st->client->irq, st->trig);
error_free_trig:
	iio_trigger_free(st->trig);
error_ret:
	return ret;
}

void inv_icm20602_remove_trigger(struct inv_icm20602_state *st)
{
	iio_trigger_unregister(st->trig);
	free_irq(st->client->irq, st->trig);
	iio_trigger_free(st->trig);
}
