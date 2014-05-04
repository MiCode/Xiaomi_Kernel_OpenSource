/*!
 * @section LICENSE
 * (C) Copyright 2011~2014 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename    bmm050_trigger.c
 * @date        "Mon Sep 30 15:07:11 2013 +0800"
 * @id          "60d8c14"
 * @version     v1.0
 *
 * @brief       BMM050 Linux Driver
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>
#include <linux/interrupt.h>

#include "bmm050_iio.h"


static const struct iio_trigger_ops bmm_trigger_ops = {
	  .owner = THIS_MODULE,
};

int bmm_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct bmm_client_data *client_data = iio_priv(indio_dev);
	struct i2c_client *client = client_data->client;

	client_data->trig = devm_iio_trigger_alloc(&client->dev,
					"%s-trigger%d",
					indio_dev->name,
					indio_dev->id);

	if (client_data->trig == NULL) {
		dev_err(&indio_dev->dev,
			"bmm failed to allocate iio trigger.\n");
		return -ENOMEM;
	}

	iio_trigger_set_drvdata(client_data->trig, indio_dev);
	client_data->trig->dev.parent = &client_data->client->dev;
	client_data->trig->ops = &bmm_trigger_ops;

	ret = iio_trigger_register(client_data->trig);
	if (ret < 0) {
		dev_err(&indio_dev->dev,
			"bmm iio trigger failed to register.\n");
		return -ENODEV;
	}

	indio_dev->trig = client_data->trig;

	return 0;
}

void bmm_remove_trigger(struct iio_dev *indio_dev)
{
	struct bmm_client_data *client_data = iio_priv(indio_dev);
	iio_trigger_unregister(client_data->trig);
}

