/* opyright (C) 2018 XiaoMi, Inc.
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

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/time64.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/buffer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/of_irq.h>
#include <../../../arch/mips/include/asm/bootinfo.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#define US_PROX_IIO_NAME		"distance"

#include "scp.h"
#include "scp_helper.h"
#include "scp_ipi.h"
#include "scp_ipi_wrapper.h"

#include "scp_ipi_pin.h"
#include <audio_task_manager.h>
#include <audio_ipi_queue.h>

/*
extern void iio_device_unregister(struct iio_dev *indio_dev);
extern void iio_device_free(struct iio_dev *dev);
extern int __iio_device_register(struct iio_dev *indio_dev, struct module *this_mod);
extern struct iio_dev *iio_device_alloc(struct device *parent, int sizeof_priv);
*/
struct iio_buffer *iio_kefifo_allocate(void);
void iio_kefifo_free(struct iio_buffer *r);
struct iio_poll_func
*iio_alloc_pollfunc(irqreturn_t (*h)(int irq, void *p),
        irqreturn_t (*thread)(int irq, void *p),
        int type,
        struct iio_dev *indio_dev,
        const char *fmt,
        ...);
int iio_trigger_buffer_setup(struct iio_dev *indio_dev,
        irqreturn_t (*h)(int irq, void *p),
        irqreturn_t (*thread)(int irq, void *p),
        const struct iio_buffer_setup_ops *setup_ops);
void iio_trigger_buffer_cleanup(struct iio_dev *indio_dev);

void iio_dealloc_pollfunc(struct iio_poll_func *pf);

//extern struct mtk_ipi_device scp_ipidev;
//extern int mtk_ipi_register(struct mtk_ipi_device *ipidev, int ipi_id,
//		mbox_pin_cb_t cb, void *prdata, void *msg);

#define ELLIPTIC_ULTRASOUND_PARAM_ID_ENGINE_DATA 3
struct mius_ipi_scp_to_host_message_header {
	uint32_t parameter_id;
	uint16_t dram_payload_offset;
	uint16_t data_size;
};
typedef
	struct mius_ipi_scp_to_host_message_header
	mius_ipi_scp_to_host_message_header_t;

#define MIUS_IPI_SCP_TO_AP_DATA_SIZE 40
#define MIUS_DRAM_PAYLOAD_MAX_OFFSET ((uint16_t) 8)
struct mius_ipi_scp_to_host_message {
	mius_ipi_scp_to_host_message_header_t header;
	uint8_t data[MIUS_IPI_SCP_TO_AP_DATA_SIZE -
			sizeof(mius_ipi_scp_to_host_message_header_t)];
};
typedef
	struct mius_ipi_scp_to_host_message
	mius_ipi_scp_to_host_message_t;
static mius_ipi_scp_to_host_message_t usnd_ipi_receive;

struct scp_mius_reserved_mem_t {
	phys_addr_t phys;
	phys_addr_t virt;
	phys_addr_t size;
	int reserved;
};
static struct scp_mius_reserved_mem_t debug_segment;

irqreturn_t iio_pollfunc_us(int irq, void *p)
{
	return IRQ_NONE;
}

int32_t mius_debug_io_open(void)
{

	pr_info("[MIUS] %s()", __func__);
	if (debug_segment.reserved == 0) {
		debug_segment.phys =
			scp_get_reserve_mem_phys(SCP_ELLIPTIC_DEBUG_MEM);
		debug_segment.virt =
			scp_get_reserve_mem_virt(SCP_ELLIPTIC_DEBUG_MEM);
		debug_segment.size =
			scp_get_reserve_mem_size(SCP_ELLIPTIC_DEBUG_MEM);
		debug_segment.reserved = 1;
	}
	return 1;

}

#define MIUS_DEBUG_DATA_SIZE 512
struct mius_dram_payload {
	uint8_t data[MIUS_DEBUG_DATA_SIZE];
};
typedef
	struct mius_dram_payload
	mius_dram_payload_t;

static struct us_prox_data *g_us_prox;

struct us_prox_data {
	struct platform_device	*pdev;
	/* common state */
	struct mutex		mutex;
	/* for proximity sensor */
	struct iio_dev		*prox_idev;
	bool			prox_enabled;
	int				raw_data;
};

struct us_prox_el_data {
	uint16_t data1;
	uint16_t data2;
	int64_t timestamp;
};

#define US_SENSORS_CHANNELS(device_type, mask, index, mod, \
					ch2, s, endian, rbits, sbits, addr) \
{ \
	.type = device_type, \
	.modified = mod, \
	.info_mask_separate = mask, \
	.scan_index = index, \
	.channel2 = ch2, \
	.address = addr, \
	.scan_type = { \
		.sign = s, \
		.realbits = rbits, \
		.shift = sbits - rbits, \
		.storagebits = sbits, \
		.endianness = endian, \
	}, \
}

static const struct iio_chan_spec us_proximity_channels[] = {
	US_SENSORS_CHANNELS(IIO_PROXIMITY,
		BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SAMP_FREQ),
		0, 0, IIO_NO_MOD, 'u', IIO_LE, 16, 16, 0),
	IIO_CHAN_SOFT_TIMESTAMP(2)
};

static int us_buffer_postenable(struct iio_dev *indio_dev)
{
	int ret = 0;
	return ret;
}

static int us_buffer_predisable(struct iio_dev *indio_dev)
{
	int ret = 0;
	return ret;
}

static const struct iio_buffer_setup_ops us_buffer_setup_ops = {
	.postenable = us_buffer_postenable,
	.predisable = us_buffer_predisable,
};

static const struct iio_trigger_ops us_sensor_trigger_ops = {
	//.owner = THIS_MODULE,
};

int us_afe_callback(int data)
{
	int ret;
	struct us_prox_el_data el_data;
	struct timespec64 ts;
	ktime_get_ts64(&ts);
	el_data.timestamp = timespec64_to_ns(&ts);
	pr_info("%s: data = %d\n", __func__, data);

	if (!data)
		el_data.data1 = 0;
	else
		el_data.data1 = 5;

	if (g_us_prox) {
		ret = iio_push_to_buffers(g_us_prox->prox_idev, (unsigned char *)&el_data);
		if (ret < 0)
			pr_err("%s: failed to push us prox data to buffer, err=%d\n", __func__, ret);
	}

	return 0;
}

EXPORT_SYMBOL(us_afe_callback);


static mius_ipi_scp_to_host_message_header_t *get_header(
	mius_dram_payload_t *dram_payload, uint16_t header_id)
{
	size_t i;

	if (dram_payload == NULL)
		return NULL;
	for (i = 0; i < MIUS_DRAM_PAYLOAD_MAX_OFFSET; ++i) {
		mius_ipi_scp_to_host_message_header_t *header =
			(mius_ipi_scp_to_host_message_header_t *)
			(dram_payload + i);
		if (header->dram_payload_offset == header_id)
			return header;
	}
	return NULL;
}

static int mius_data_io_ipi_handler(
	unsigned int id, void *prdata, void *data, unsigned int len)
{
	pr_info("%s: start\n", __func__);
	static uint16_t current_ipi_counter;
	int32_t ret = -1;

	mius_dram_payload_t *dram_payload =
		(mius_dram_payload_t *)debug_segment.virt;

	mius_ipi_scp_to_host_message_t *ipi_msg = data;
	uint16_t target_ipi_message_count =
		ipi_msg->header.dram_payload_offset;

	mius_ipi_scp_to_host_message_header_t *header;
	void *payload = NULL;

	header = &ipi_msg->header;
	payload = ipi_msg->data;
	pr_info("MIUS Got data via ipi payload, addr: %p", payload);

	if ((uint16_t) (target_ipi_message_count - current_ipi_counter) >
		MIUS_DRAM_PAYLOAD_MAX_OFFSET) {
		pr_warn("[MIUS] Offset mismatch, cur ipi counter:%u, target:%u!\n",
			current_ipi_counter, target_ipi_message_count);
		current_ipi_counter = target_ipi_message_count - MIUS_DRAM_PAYLOAD_MAX_OFFSET;
	}

	while (current_ipi_counter != target_ipi_message_count) {
		++current_ipi_counter;
		// check if payload is in dram buffer
		header = get_header(dram_payload, current_ipi_counter);
		if (header != NULL) {
			payload = header + 1;
		} else if (current_ipi_counter == target_ipi_message_count) {
			// if not in dram buffer, it might be a small message in the ipi buffer
			header = &ipi_msg->header;
			payload = ipi_msg->data;
		} else if (header == NULL) {
			// message seems to be lost
			pr_err("[MIUS] did not find payload with id %u",
			       (unsigned int)current_ipi_counter);
			continue;
		}
		switch (header->parameter_id) {
			case ELLIPTIC_ULTRASOUND_PARAM_ID_ENGINE_DATA:
				pr_info("MIUS engine data get %u",
						header->data_size);
				if (((uint8_t *)payload)[23] == (uint8_t)66) {
					pr_debug("MIUS receive far signal");
					us_afe_callback(0);
				} else {
					pr_debug("MIUS receive near signal");
					us_afe_callback(5);
				}
				break;
			default:
				pr_debug("MIUS illegal param id: %u",
					header->parameter_id);
				break;
		}
	}
	return 0;
}

static ssize_t us_show_dump_output(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct us_prox_data *data;
	unsigned long value;

	data = *((struct us_prox_data **)iio_priv(dev_get_drvdata(dev)));

	mutex_lock(&data->mutex);
	value = data->raw_data;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", value);
}

static ssize_t us_store_dump_output(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return 0;
}

static DEVICE_ATTR(dump_output, S_IWUSR | S_IRUGO,
		us_show_dump_output, us_store_dump_output);


static struct attribute *us_prox_attributes[] = {
	&dev_attr_dump_output.attr,
	NULL,
};

static struct attribute_group us_prox_attribute_group = {
	.attrs = us_prox_attributes,
};

static const struct iio_info us_proximity_info = {
	//.driver_module = THIS_MODULE,
	.attrs = &us_prox_attribute_group,
};

int iio_trigger_buffer_setup(struct iio_dev *indio_dev,
        irqreturn_t (*h)(int irq, void *p),
        irqreturn_t (*thread)(int irq, void *p),
        const struct iio_buffer_setup_ops *setup_ops)
{
    struct iio_buffer *buffer;
    int ret;

    buffer = iio_kefifo_allocate();
    if (!buffer) {
        ret = -ENOMEM;
        goto error_ret;
    }

    iio_device_attach_buffer(indio_dev, buffer);

    indio_dev->pollfunc = iio_alloc_pollfunc(h,
            thread,
            IRQF_ONESHOT,
            indio_dev,
            "%s_consumer%d",
            indio_dev->name,
            indio_dev->id);
    if (indio_dev->pollfunc == NULL) {
        ret = -ENOMEM;
        goto error_kfifo_free;
    }

    /* Ring buffer functions - here trigger setup related */
/*
    if (setup_ops)
        indio_dev->setup_ops = setup_ops;
*/
/*
    else
        indio_dev->setup_ops = &iio_trigger_buffer_setup_ops;
*/

    /* Flag that polled ring buffering is possible */
    indio_dev->modes |= INDIO_BUFFER_SOFTWARE;

    return 0;

error_kfifo_free:
    iio_kefifo_free(indio_dev->buffer);
error_ret:
    return ret;
}


static int us_proximity_iio_setup(struct device *pdev, struct us_prox_data *data)
{
	struct iio_dev *idev;
	struct us_prox_data **priv_data;
	int ret = 0;

	idev = iio_device_alloc(pdev, sizeof(*priv_data));
	if (!idev) {
		pr_err("us prox IIO memory alloc fail\n");
		return -ENOMEM;
	}

	data->prox_idev = idev;

	idev->channels = us_proximity_channels;
	idev->num_channels = ARRAY_SIZE(us_proximity_channels);
	idev->dev.parent = &(data->pdev->dev);
	idev->info = &us_proximity_info;
	idev->name = US_PROX_IIO_NAME;
	idev->modes = INDIO_DIRECT_MODE;

	priv_data = iio_priv(idev);
	*priv_data = data;
	ret = iio_trigger_buffer_setup(idev, iio_pollfunc_us, NULL,
					&us_buffer_setup_ops);
	if (ret < 0)
		goto free_iio_p;

	ret = iio_device_register(idev);
	if (ret) {
		pr_err("Proximity IIO register fail\n");
		goto free_trigger_p;
	}
	return ret;

free_trigger_p:
	iio_trigger_buffer_cleanup(idev);
free_iio_p:
	data->prox_idev = NULL;
	iio_device_free(idev);
	return ret;
}


void iio_trigger_buffer_cleanup(struct iio_dev *indio_dev)
{
    iio_dealloc_pollfunc(indio_dev->pollfunc);
    iio_kefifo_free(indio_dev->buffer);
}

static int us_proximity_teardown(struct us_prox_data *data)
{
	iio_device_unregister(data->prox_idev);
	iio_trigger_buffer_cleanup(data->prox_idev);
	iio_device_free(data->prox_idev);

	return 0;
}

static int us_prox_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct us_prox_data *us_prox;

	pr_info("%s: start\n", __func__);

	us_prox = kzalloc(sizeof(*us_prox), GFP_KERNEL);
	if (!us_prox)
		return -ENOMEM;

	us_prox->pdev = pdev;
	dev_set_drvdata(&pdev->dev, us_prox);

	g_us_prox = us_prox;

	mutex_init(&us_prox->mutex);
	ret = us_proximity_iio_setup(&pdev->dev, us_prox);

	ret = mius_debug_io_open();

	if (ret < 0) {
		pr_err("%s: iio setup failed ret = %d\n", __func__, ret);
		return ret;
	}
	if (mtk_ipi_register(&scp_ipidev, IPI_IN_ELLIPTIC_ULTRA_0,
			(mbox_pin_cb_t)mius_data_io_ipi_handler,
			NULL, &usnd_ipi_receive) != SCP_IPI_DONE) {
		pr_err("%s: scp registration failed ret = %d\n", __func__, ret);
	}
	pr_info("%s: end\n", __func__);
	return ret;
}

static int us_prox_remove(struct platform_device *pdev)
{
	struct us_prox_data *us_prox;

	us_prox = dev_get_drvdata(&pdev->dev);

	dev_set_drvdata(&pdev->dev, NULL);

	if (us_prox) {
		us_proximity_teardown(us_prox);
		kfree(us_prox);
	}

	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "us_prox" },
	{}
};

static struct platform_driver us_prox_driver = {
	.probe		= us_prox_probe,
	.remove		= us_prox_remove,
	.driver		= {
		.name		= "us_prox",
		.of_match_table	= dt_match,
	},
};

static struct platform_device us_prox_dev = {
	.name = "us_prox",
	.dev = {
		.platform_data = NULL,
	},
};

static int __init us_prox_init(void)
{
	platform_device_register(&us_prox_dev);
	return platform_driver_register(&us_prox_driver);
}
module_init(us_prox_init);

static void __exit us_prox_exit(void)
{
	platform_driver_unregister(&us_prox_driver);
	platform_device_unregister(&us_prox_dev);
}
module_exit(us_prox_exit);

MODULE_AUTHOR("xiaomi");
MODULE_DESCRIPTION("Ultrasound IIO driver");
MODULE_LICENSE("GPL");

