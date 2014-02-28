/*
 * ACPI Ambient Light Sensor Driver
 *
 * Based on ALS driver:
 * Copyright (C) 2009 Zhang Rui <rui.zhang@intel.com>
 *
 * Rework for IIO subsystem:
 * Copyright (C) 2012-2013 Martin Liska <marxin.liska@gmail.com>
 *
 * Final cleanup and debugging:
 * Copyright (C) 2013 Marek Vasut <marex@denx.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/acpi.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/irq_work.h>

#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#define ACPI_ALS_CLASS			"als"
#define ACPI_ALS_DEVICE_NAME		"acpi-als"
#define ACPI_ALS_NOTIFY_ILLUMINANCE	0x80
#define ACPI_ALS_NOTIFY_COLOR_TEMP	0x81

ACPI_MODULE_NAME("acpi-als");

struct acpi_als {
	struct acpi_device	*device;
	struct iio_dev		*iio;
	struct irq_work		work;

	uint32_t		evt_event;
	uint16_t		*evt_buffer;
	unsigned int		evt_buffer_len;
};

/*
 * All types of properties the ACPI0008 block can report. The ALI, ALC, ALT
 * and ALP can all be handled by als_read_value() below, while the ALR is
 * special.
 *
 * The _ALR property returns tables that can be used to fine-tune the values
 * reported by the other props based on the particular hardware type and it's
 * location (it contains tables for "rainy", "bright inhouse lighting" etc.).
 *
 * So far, we support only ALI (illuminance).
 */
#define ACPI_ALS_ILLUMINANCE	"_ALI"
#define ACPI_ALS_CHROMATICITY	"_ALC"
#define ACPI_ALS_COLOR_TEMP	"_ALT"
#define ACPI_ALS_POLLING	"_ALP"
#define ACPI_ALS_TABLES		"_ALR"

static int32_t als_read_value(struct acpi_als *als, char *prop)
{
	unsigned long long illuminance;
	acpi_status status;

	status = acpi_evaluate_integer(als->device->handle,
				prop, NULL, &illuminance);

	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status,
				"Error reading ALS illuminance"));
		/* Something went wrong, it's pitch black outside! */
		illuminance = 0;
	}

	return illuminance;
}

static void acpi_als_irq_work(struct irq_work *work)
{
	struct acpi_als *als = container_of(work, struct acpi_als, work);

	iio_trigger_poll(als->iio->trig, 0);
}

static void acpi_als_notify(struct acpi_device *device, u32 event)
{
	struct iio_dev *iio = acpi_driver_data(device);
	struct acpi_als *als = iio_priv(iio);

	if (!iio_buffer_enabled(iio))
		return;

	if (event != ACPI_ALS_NOTIFY_ILLUMINANCE)
		return; /* Only handle illuminance changes for now */

	als->evt_event = event;
	irq_work_queue(&als->work);
}

static int acpi_als_read_raw(struct iio_dev *iio,
			struct iio_chan_spec const *chan, int *val,
			int *val2, long mask)
{
	struct acpi_als *als = iio_priv(iio);

	if (mask != IIO_CHAN_INFO_RAW)
		return -EINVAL;

	/* we support only illumination (_ALI) so far. */
	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	*val = als_read_value(als, ACPI_ALS_ILLUMINANCE);

	return IIO_VAL_INT;
}

/*
 * So far, there's only one channel in here, but the specification for
 * ACPI0008 says there can be more to what the block can report. Like
 * chromaticity and such. We are ready for incoming additions!
 */
static const struct iio_chan_spec acpi_als_channels[] = {
	{
		.type		= IIO_LIGHT,
		.indexed	= 0,
		.channel	= 0,
		.scan_index	= 0,
		.scan_type	= {
			.sign 		= 'u',
			.realbits	= 10,
			.storagebits	= 16,
		},
		.info_mask_shared_by_type= 	BIT(IIO_CHAN_INFO_RAW) |
						BIT(IIO_CHAN_INFO_SAMP_FREQ),
	},
};

static const struct iio_info acpi_als_info = {
	.driver_module	= THIS_MODULE,
	.read_raw	= &acpi_als_read_raw,
};

static irqreturn_t acpi_als_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *iio = pf->indio_dev;
	struct acpi_als *als = iio_priv(iio);
	struct device *dev = &als->device->dev;
	uint16_t *buffer = als->evt_buffer;

	switch (als->evt_event) {
	case ACPI_ALS_NOTIFY_ILLUMINANCE:
		*buffer++ = als_read_value(als, ACPI_ALS_ILLUMINANCE);
		break;

	case ACPI_ALS_NOTIFY_COLOR_TEMP:
		/* Color temperature */;
		goto skip;

	default:
		/* Unhandled event */
		dev_dbg(dev, "Unhandled ACPI ALS event (%08x)!\n",
			als->evt_event);
		goto skip;
	}

	iio_push_to_buffers_with_timestamp(iio, als->evt_buffer,
		iio_get_time_ns());

skip:
	iio_trigger_notify_done(iio->trig);

	return IRQ_HANDLED;
}

static const struct iio_trigger_ops acpi_als_trigger_ops = {
	.owner = THIS_MODULE,
};

static int acpi_als_trigger_init(struct iio_dev *iio)
{
	struct iio_trigger *trig;
	int ret;

	trig = iio_trigger_alloc("%s-dev%i", iio->name, iio->id);
	if (!trig)
		return -ENOMEM;

	trig->dev.parent = iio->dev.parent;
	iio_trigger_set_drvdata(trig, iio);
	trig->ops = &acpi_als_trigger_ops;

	ret = iio_trigger_register(trig);
	if (ret) {
		iio_trigger_free(trig);
		return ret;
	}

	iio->trig = trig;

	return 0;
}

static void acpi_als_trigger_remove(struct iio_dev *iio)
{
	iio_trigger_unregister(iio->trig);
	iio_trigger_free(iio->trig);
}

static int acpi_als_add(struct acpi_device *device)
{
	struct acpi_als *als;
	struct iio_dev *iio;
	struct device *dev = &device->dev;
	int ret;

	/*
	 * The event buffer contains timestamp and all the data from
	 * the ACPI0008 block. There are multiple, but so far we only
	 * support _ALI (illuminance). Yes, we're ready for more!
	 */
	const unsigned int evt_sources = ARRAY_SIZE(acpi_als_channels);
	const unsigned int evt_buffer_size = sizeof(int64_t) +
				(sizeof(uint16_t) * evt_sources);

	iio = devm_iio_device_alloc(dev, sizeof(*als));
	if (!iio) {
		dev_err(dev, "Failed to allocate IIO device\n");
		return -ENOMEM;
	}

	als = iio_priv(iio);

	device->driver_data = iio;
	als->device = device;
	als->iio = iio;
	als->evt_buffer = devm_kzalloc(dev, evt_buffer_size, GFP_KERNEL);

	if (!als->evt_buffer) {
		ret = -ENOMEM;
		goto err_iio;
	}

	init_irq_work(&als->work, acpi_als_irq_work);

	iio->name = ACPI_ALS_DEVICE_NAME;
	iio->dev.parent = dev;
	iio->info = &acpi_als_info;
	iio->modes = INDIO_DIRECT_MODE;
	iio->channels = acpi_als_channels;
	iio->num_channels = ARRAY_SIZE(acpi_als_channels);

	ret = iio_triggered_buffer_setup(iio, &iio_pollfunc_store_time,
					&acpi_als_trigger_handler, NULL);
	if (ret)
		goto err_iio;

	ret = acpi_als_trigger_init(iio);

	if (ret)
		goto err_trig;

	ret = iio_device_register(iio);
	if (ret < 0)
		goto err_dev;

	return 0;

err_dev:
	acpi_als_trigger_remove(iio);
err_trig:
	iio_triggered_buffer_cleanup(iio);
err_iio:
	devm_iio_device_free(dev, iio);
	return ret;
}

static int acpi_als_remove(struct acpi_device *device)
{
	struct iio_dev *iio = acpi_driver_data(device);
	struct device *dev = &device->dev;

	iio_device_unregister(iio);
	iio_triggered_buffer_cleanup(iio);
	acpi_als_trigger_remove(iio);
	devm_iio_device_free(dev, iio);

	return 0;
}

static const struct acpi_device_id acpi_als_device_ids[] = {
	{"ACPI0008", 0},
	{"", 0},
};

MODULE_DEVICE_TABLE(acpi, acpi_als_device_ids);

static struct acpi_driver acpi_als_driver = {
	.name	= "acpi_als",
	.class	= ACPI_ALS_CLASS,
	.ids	= acpi_als_device_ids,
	.ops = {
		.add	= acpi_als_add,
		.remove	= acpi_als_remove,
		.notify	= acpi_als_notify,
	},
};

module_acpi_driver(acpi_als_driver);

MODULE_AUTHOR("Zhang Rui <rui.zhang@intel.com");
MODULE_AUTHOR("Martin Liska <marxin.liska@gmail.com>");
MODULE_AUTHOR("Marek Vasut <marex@denx.de>");
MODULE_DESCRIPTION("ACPI Ambient Light Sensor Driver");
MODULE_LICENSE("GPL");
