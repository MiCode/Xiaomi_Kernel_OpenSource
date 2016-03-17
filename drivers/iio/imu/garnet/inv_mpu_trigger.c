/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>

#include "inv_mpu_iio.h"

/*
 * inv_mpu_data_rdy_trigger_set_state() set data ready interrupt state
 */
static int inv_mpu_data_rdy_trigger_set_state(struct iio_trigger *trig,
						bool state)
{
	struct inv_mpu_state *st = iio_trigger_get_drvdata(trig);

	atomic_set(&st->data_enable, state);

	return 0;
}

static const struct iio_trigger_ops inv_mpu_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &inv_mpu_data_rdy_trigger_set_state,
};

int inv_mpu_probe_trigger(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_trigger *trig;
	int ret;

	trig = iio_trigger_alloc("%s-dev%d", indio_dev->name, indio_dev->id);
	if (trig == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	trig->dev.parent = indio_dev->dev.parent;
	iio_trigger_set_drvdata(trig, st);
	trig->ops = &inv_mpu_trigger_ops;

	ret = iio_trigger_register(trig);
	if (ret)
		goto error_free_trig;

	st->trig = trig;
	return 0;
error_free_trig:
	iio_trigger_free(trig);
error:
	return ret;
}

void inv_mpu_remove_trigger(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);

	indio_dev->trig = NULL;
	iio_trigger_unregister(st->trig);
	iio_trigger_free(st->trig);
}
