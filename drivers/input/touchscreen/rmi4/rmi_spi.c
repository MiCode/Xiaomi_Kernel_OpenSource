/*
 * Copyright (c) 2011 Synaptics Incorporated
 * Copyright (c) 2011 Unixphere
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/sched.h>
#include <linux/gpio.h>
#include <linux/rmi.h>

#define COMMS_DEBUG 0

#define RMI_PROTOCOL_VERSION_ADDRESS	0xa0fd
#define SPI_V2_UNIFIED_READ		0xc0
#define SPI_V2_WRITE			0x40
#define SPI_V2_PREPARE_SPLIT_READ	0xc8
#define SPI_V2_EXECUTE_SPLIT_READ	0xca

#define RMI_SPI_BLOCK_DELAY_US		65
#define RMI_SPI_BYTE_DELAY_US		65
#define RMI_SPI_WRITE_DELAY_US		0

#define RMI_V1_READ_FLAG		0x80

#define RMI_PAGE_SELECT_REGISTER 0x00FF
#define RMI_SPI_PAGE(addr) (((addr) >> 8) & 0x80)

static char *spi_v1_proto_name = "spi";
static char *spi_v2_proto_name = "spiv2";

struct rmi_spi_data {
	struct mutex page_mutex;
	int page;
	int (*set_page) (struct rmi_phys_device *phys, u8 page);
	bool split_read_pending;
	int enabled;
	int irq;
	int irq_flags;
	struct rmi_phys_device *phys;
	struct completion irq_comp;
};

static irqreturn_t rmi_spi_hard_irq(int irq, void *p)
{
	struct rmi_phys_device *phys = p;
	struct rmi_spi_data *data = phys->data;
	struct rmi_device_platform_data *pdata = phys->dev->platform_data;

	if (data->split_read_pending &&
		      gpio_get_value(pdata->irq) == pdata->irq_polarity) {
		phys->info.attn_count++;
		complete(&data->irq_comp);
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t rmi_spi_irq_thread(int irq, void *p)
{
	struct rmi_phys_device *phys = p;
	struct rmi_device *rmi_dev = phys->rmi_dev;
	struct rmi_driver *driver = rmi_dev->driver;
	struct rmi_device_platform_data *pdata = phys->dev->platform_data;

	if (gpio_get_value(pdata->irq) == pdata->irq_polarity) {
		phys->info.attn_count++;
		if (driver && driver->irq_handler)
			driver->irq_handler(rmi_dev, irq);
	}

	return IRQ_HANDLED;
}

static int rmi_spi_xfer(struct rmi_phys_device *phys,
		    const u8 *txbuf, unsigned n_tx, u8 *rxbuf, unsigned n_rx)
{
	struct spi_device *client = to_spi_device(phys->dev);
	struct rmi_spi_data *v2_data = phys->data;
	struct rmi_device_platform_data *pdata = phys->dev->platform_data;
	int status;
	struct spi_message message;
	struct spi_transfer *xfers;
	int total_bytes = n_tx + n_rx;
	u8 local_buf[total_bytes];
	int xfer_count = 0;
	int xfer_index = 0;
	int block_delay = n_rx > 0 ? pdata->spi_data.block_delay_us : 0;
	int byte_delay = n_rx > 1 ? pdata->spi_data.read_delay_us : 0;
	int write_delay = n_tx > 1 ? pdata->spi_data.write_delay_us : 0;
#if COMMS_DEBUG
	int i;
#endif
	// pr_info("in function ____%s____  \n", __func__);

	if (v2_data->split_read_pending) {
		block_delay =
		    n_rx > 0 ? pdata->spi_data.split_read_block_delay_us : 0;
		byte_delay =
		    n_tx > 1 ? pdata->spi_data.split_read_byte_delay_us : 0;
		write_delay = 0;
	}

	if (n_tx) {
		phys->info.tx_count++;
		phys->info.tx_bytes += n_tx;
		if (write_delay)
			xfer_count += n_tx;
		else
			xfer_count += 1;
	}

	if (n_rx) {
		phys->info.rx_count++;
		phys->info.rx_bytes += n_rx;
		if (byte_delay)
			xfer_count += n_rx;
		else
			xfer_count += 1;
	}

	xfers = kcalloc(xfer_count,
			    sizeof(struct spi_transfer), GFP_KERNEL);
	if (!xfers)
		return -ENOMEM;

	spi_message_init(&message);

	if (n_tx) {
		if (write_delay) {
			for (xfer_index = 0; xfer_index < n_tx;
					xfer_index++) {
				memset(&xfers[xfer_index], 0,
				       sizeof(struct spi_transfer));
				xfers[xfer_index].len = 1;
				xfers[xfer_index].delay_usecs = write_delay;
				xfers[xfer_index].cs_change = 1;
				xfers[xfer_index].tx_buf = txbuf + xfer_index;
				spi_message_add_tail(&xfers[xfer_index],
						     &message);
			}
		} else {
			memset(&xfers[0], 0, sizeof(struct spi_transfer));
			xfers[0].len = n_tx;
			spi_message_add_tail(&xfers[0], &message);
			memcpy(local_buf, txbuf, n_tx);
			xfers[0].tx_buf = local_buf;
			xfer_index++;
		}
		if (block_delay){
			xfers[xfer_index-1].delay_usecs = block_delay;
			xfers[xfer_index].cs_change = 1;
		}
	}
	if (n_rx) {
		if (byte_delay) {
			int buffer_offset = n_tx;
			for (; xfer_index < xfer_count; xfer_index++) {
				memset(&xfers[xfer_index], 0,
				       sizeof(struct spi_transfer));
				xfers[xfer_index].len = 1;
				xfers[xfer_index].delay_usecs = byte_delay;
				xfers[xfer_index].cs_change = 1;
				xfers[xfer_index].rx_buf =
				    local_buf + buffer_offset;
				buffer_offset++;
				spi_message_add_tail(&xfers[xfer_index],
						     &message);
			}
		} else {
			memset(&xfers[xfer_index], 0,
			       sizeof(struct spi_transfer));
			xfers[xfer_index].len = n_rx;
			xfers[xfer_index].rx_buf = local_buf + n_tx;
			spi_message_add_tail(&xfers[xfer_index], &message);
			xfer_index++;
		}
	}

#if COMMS_DEBUG
	if (n_tx) {
		dev_info(&client->dev, "SPI sends %d bytes: ", n_tx);
		for (i = 0; i < n_tx; i++)
		  // dev_info(&client->dev, "%02X ", txbuf[i]);
		  pr_info(": %02X ", txbuf[i]);
		// dev_info(&client->dev, "\n");
		pr_info("\n");
	}
#endif

	/* do the i/o */
	if (pdata->spi_data.cs_assert) {
		status = pdata->spi_data.cs_assert(
			pdata->spi_data.cs_assert_data, true);
		if (!status) {
			dev_err(phys->dev, "Failed to assert CS.");
			/* nonzero means error */
			status = -1;
			goto error_exit;
		} else
			status = 0;
	}

	if (pdata->spi_data.pre_delay_us)
		udelay(pdata->spi_data.pre_delay_us);

	status = spi_sync(client, &message);

	if (pdata->spi_data.post_delay_us)
		udelay(pdata->spi_data.post_delay_us);

	if (pdata->spi_data.cs_assert) {
		status = pdata->spi_data.cs_assert(
			pdata->spi_data.cs_assert_data, false);
		if (!status) {
			dev_err(phys->dev, "Failed to deassert CS.");
			/* nonzero means error */
			status = -1;
			goto error_exit;
		} else
			status = 0;
	}

	if (status == 0) {
		memcpy(rxbuf, local_buf + n_tx, n_rx);
		status = message.status;
	} else {
		phys->info.tx_errs++;
		phys->info.rx_errs++;
		dev_err(phys->dev, "spi_sync failed with error code %d.",
		       status);
	}

#if COMMS_DEBUG
	if (n_rx) {
		dev_info(&client->dev, "SPI received %d bytes: ", n_rx);
		for (i = 0; i < n_rx; i++)
		  // dev_info(&client->dev, "%02X ", rxbuf[i]);
			pr_info(": %02X ", rxbuf[i]);
		// dev_info(&client->dev, "\n");
		pr_info("\n");
	}
#endif

error_exit:
	kfree(xfers);
	return status;
}

static int rmi_spi_v2_write_block(struct rmi_phys_device *phys, u16 addr,
				  u8 *buf, int len)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[len + 4];
	int error;
	// pr_info("in function ____%s____  \n", __func__);

	txbuf[0] = SPI_V2_WRITE;
	txbuf[1] = (addr >> 8) & 0x00FF;
	txbuf[2] = addr & 0x00FF;
	txbuf[3] = len;

	memcpy(&txbuf[4], buf, len);

	mutex_lock(&data->page_mutex);

	if (RMI_SPI_PAGE(addr) != data->page) {
		error = data->set_page(phys, RMI_SPI_PAGE(addr));
		if (error < 0)
			goto exit;
	}

	error = rmi_spi_xfer(phys, buf, len + 4, NULL, 0);
	if (error < 0)
		goto exit;
	error = len;

exit:
	mutex_unlock(&data->page_mutex);
	return error;
}

static int rmi_spi_v2_write(struct rmi_phys_device *phys, u16 addr, u8 data)
{
	int error = rmi_spi_v2_write_block(phys, addr, &data, 1);
	// pr_info("in function ____%s____  \n", __func__);

	return (error == 1) ? 0 : error;
}

static int rmi_spi_v1_write_block(struct rmi_phys_device *phys, u16 addr,
				  u8 *buf, int len)
{
	struct rmi_spi_data *data = phys->data;
	unsigned char txbuf[len + 2];
	int error;
	// pr_info("in function ____%s____  \n", __func__);

	txbuf[0] = addr >> 8;
	txbuf[1] = addr;
	memcpy(txbuf+2, buf, len);

	mutex_lock(&data->page_mutex);

	if (RMI_SPI_PAGE(addr) != data->page) {
		error = data->set_page(phys, RMI_SPI_PAGE(addr));
		if (error < 0)
			goto exit;
	}

	error = rmi_spi_xfer(phys, txbuf, len + 2, NULL, 0);
	if (error < 0)
		goto exit;
	error = len;

exit:
	mutex_unlock(&data->page_mutex);
	return error;
}

static int rmi_spi_v1_write(struct rmi_phys_device *phys, u16 addr, u8 data)
{
	int error = rmi_spi_v1_write_block(phys, addr, &data, 1);
	// pr_info("in function ____%s____  \n", __func__);

	return (error == 1) ? 0 : error;
}

static int rmi_spi_v2_split_read_block(struct rmi_phys_device *phys, u16 addr,
				       u8 *buf, int len)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[4];
	u8 rxbuf[len + 1]; /* one extra byte for read length */
	int error;
	// pr_info("in function ____%s____  \n", __func__);

	txbuf[0] = SPI_V2_PREPARE_SPLIT_READ;
	txbuf[1] = (addr >> 8) & 0x00FF;
	txbuf[2] = addr & 0x00ff;
	txbuf[3] = len;

	mutex_lock(&data->page_mutex);

	if (RMI_SPI_PAGE(addr) != data->page) {
		error = data->set_page(phys, RMI_SPI_PAGE(addr));
		if (error < 0)
			goto exit;
	}

	data->split_read_pending = true;

	error = rmi_spi_xfer(phys, txbuf, 4, NULL, 0);
	if (error < 0) {
		data->split_read_pending = false;
		goto exit;
	}

	wait_for_completion(&data->irq_comp);

	txbuf[0] = SPI_V2_EXECUTE_SPLIT_READ;
	txbuf[1] = 0;

	error = rmi_spi_xfer(phys, txbuf, 2, rxbuf, len + 1);
	data->split_read_pending = false;
	if (error < 0)
		goto exit;

	/* first byte is length */
	if (rxbuf[0] != len) {
		error = -EIO;
		goto exit;
	}

	memcpy(buf, rxbuf + 1, len);
	error = len;

exit:
	mutex_unlock(&data->page_mutex);
	return error;
}

static int rmi_spi_v2_read_block(struct rmi_phys_device *phys, u16 addr,
				 u8 *buf, int len)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[4];
	int error;
	// pr_info("in function ____%s____  \n", __func__);

	txbuf[0] = SPI_V2_UNIFIED_READ;
	txbuf[1] = (addr >> 8) & 0x00FF;
	txbuf[2] = addr & 0x00ff;
	txbuf[3] = len;

	mutex_lock(&data->page_mutex);

	if (RMI_SPI_PAGE(addr) != data->page) {
		error = data->set_page(phys, RMI_SPI_PAGE(addr));
		if (error < 0)
			goto exit;
	}

	error = rmi_spi_xfer(phys, txbuf, 4, buf, len);
	if (error < 0)
		goto exit;
	error = len;

exit:
	mutex_unlock(&data->page_mutex);
	return error;
}

static int rmi_spi_v2_read(struct rmi_phys_device *phys, u16 addr, u8 *buf)
{
	int error = rmi_spi_v2_read_block(phys, addr, buf, 1);

	return (error == 1) ? 0 : error;
}

static int rmi_spi_v1_read_block(struct rmi_phys_device *phys, u16 addr,
				 u8 *buf, int len)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[2];
	int error;
	// pr_info("in function ____%s____  \n", __func__);

	txbuf[0] = (addr >> 8) | RMI_V1_READ_FLAG;
	txbuf[1] = addr;

	mutex_lock(&data->page_mutex);

	if (RMI_SPI_PAGE(addr) != data->page) {
		error = data->set_page(phys, RMI_SPI_PAGE(addr));
		if (error < 0)
			goto exit;
	}

	error = rmi_spi_xfer(phys, txbuf, 2, buf, len);
	// pr_info("  back in function %s, rmi_spi_xfer returned %d\n", __func__, error);

	if (error < 0)
		goto exit;
	error = len;

exit:
	mutex_unlock(&data->page_mutex);
	return error;
}

static int rmi_spi_v1_read(struct rmi_phys_device *phys, u16 addr, u8 *buf)
{
	int error = rmi_spi_v1_read_block(phys, addr, buf, 1);
	pr_info("in function ____%s____  \n", __func__);

	return (error == 1) ? 0 : error;
}

#define RMI_SPI_PAGE_SELECT_WRITE_LENGTH 1

static int rmi_spi_v1_set_page(struct rmi_phys_device *phys, u8 page)
{
	struct rmi_spi_data *data = phys->data;
	u8 txbuf[] = {RMI_PAGE_SELECT_REGISTER >> 8,
		RMI_PAGE_SELECT_REGISTER & 0xFF, page};
	int error;
	pr_info("in function ____%s____  \n", __func__);

	error = rmi_spi_xfer(phys, txbuf, sizeof(txbuf), NULL, 0);
	if (error < 0) {
		dev_err(phys->dev, "Failed to set page select, code: %d.\n",
			error);
		return error;
	}

	data->page = page;

	return RMI_SPI_PAGE_SELECT_WRITE_LENGTH;
}

static int rmi_spi_v2_set_page(struct rmi_phys_device *phys, u8 page)
{
	struct rmi_spi_data *data = phys->data;

	u8 txbuf[] = {SPI_V2_WRITE, RMI_PAGE_SELECT_REGISTER >> 8,
		RMI_PAGE_SELECT_REGISTER & 0xFF,
		RMI_SPI_PAGE_SELECT_WRITE_LENGTH, page};
	int error;
	pr_info("in function ____%s____  \n", __func__);

	error = rmi_spi_xfer(phys, txbuf, sizeof(txbuf), NULL, 0);
	if (error < 0) {
		dev_err(phys->dev, "Failed to set page select, code: %d.\n",
			error);
		return error;
	}

	data->page = page;

	return RMI_SPI_PAGE_SELECT_WRITE_LENGTH;
}


static int acquire_attn_irq(struct rmi_spi_data *data)
{
        int retval = 0;
	pr_info("in function ____%s____\n", __func__);
	pr_info("irq = %d\n", data->irq);
	pr_info("data->irq_flags = 0x%8x\n", data->irq_flags);
	pr_info("dev_name(data->phys->dev) = %s\n", dev_name(data->phys->dev));

	retval =  request_threaded_irq(data->irq, rmi_spi_hard_irq,
			rmi_spi_irq_thread, data->irq_flags,
			dev_name(data->phys->dev), data->phys);

	pr_info("retval = %d\n", retval);
	return retval;
}

static int enable_device(struct rmi_phys_device *phys)
{
	int retval = 0;

	struct rmi_spi_data *data = phys->data;

	if (data->enabled) {
		dev_info(phys->dev, "Physical device already enabled.\n");
		return 0;
	}

	retval = acquire_attn_irq(data);
	if (retval)
		goto error_exit;

	data->enabled = true;
	dev_info(phys->dev, "Physical device enabled.\n");
	return 0;

error_exit:
	dev_err(phys->dev, "Failed to enable physical device. Code=%d.\n",
		retval);
	return retval;
}


static void disable_device(struct rmi_phys_device *phys)
{
	struct rmi_spi_data *data = phys->data;

	pr_info("in function ____%s____  \n", __func__);
	if (!data->enabled) {
		dev_warn(phys->dev, "Physical device already disabled.\n");
		return;
	}
	disable_irq(data->irq);
	free_irq(data->irq, data->phys);

	dev_info(phys->dev, "Physical device disabled.\n");
	data->enabled = false;
}



#define DUMMY_READ_SLEEP_US 10

static int rmi_spi_check_device(struct rmi_phys_device *rmi_phys)
{
	u8 buf[6];
	int error;
	int i;

	pr_info("in function ____%s____  \n", __func__);

	/* Some SPI subsystems return 0 for the very first read you do.  So
	 * we use this dummy read to get that out of the way.
	 */
	error = rmi_spi_v1_read_block(rmi_phys, PDT_START_SCAN_LOCATION,
				      buf, sizeof(buf));
	if (error < 0) {
		dev_err(rmi_phys->dev, "dummy read failed with %d.\n", error);
		return error;
	}
	udelay(DUMMY_READ_SLEEP_US);

	/* Force page select to 0.
	 */
	error = rmi_spi_v1_set_page(rmi_phys, 0x00);
	if (error < 0)
		return error;

	/* Now read the first PDT entry.  We know where this is, and if the
	 * RMI4 device is out there, these 6 bytes will be something other
	 * than all 0x00 or 0xFF.  We need to check for 0x00 and 0xFF,
	 * because many (maybe all) SPI implementations will return all 0x00
	 * or all 0xFF on read if the device is not connected.
	 */
	error = rmi_spi_v1_read_block(rmi_phys, PDT_START_SCAN_LOCATION,
				      buf, sizeof(buf));
	if (error < 0) {
		dev_err(rmi_phys->dev, "probe read failed with %d.\n", error);
		return error;
	}

	dev_info(rmi_phys->dev, "probe read succeeded  with %d.\n", error);
	for (i = 0; i < sizeof(buf); i++) {
		if (buf[i] != 0x00 && buf[i] != 0xFF)
			return error;
	}

	dev_err(rmi_phys->dev, "probe read returned invalid block.\n");
	return -ENODEV;
}

static int __devinit rmi_spi_probe(struct spi_device *spi)
{
	struct rmi_phys_device *rmi_phys;
	struct rmi_spi_data *data;
	struct rmi_device_platform_data *pdata = spi->dev.platform_data;
	u8 buf[2];
	int error;

        pr_info("%s: probe for rmi_spi device\n", __func__);

	if (!pdata) {
		dev_err(&spi->dev, "no platform data\n");
		return -EINVAL;
	}

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX)
		return -EINVAL;

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_3;
	error = spi_setup(spi);
	if (error < 0) {
		dev_err(&spi->dev, "spi_setup failed!\n");
		return error;
	}

	rmi_phys = kzalloc(sizeof(struct rmi_phys_device), GFP_KERNEL);
	if (!rmi_phys)
		return -ENOMEM;

	data = kzalloc(sizeof(struct rmi_spi_data), GFP_KERNEL);
	if (!data) {
		error = -ENOMEM;
		goto err_phys;
	}
	data->enabled = true;	/* We plan to come up enabled. */
	data->irq = gpio_to_irq(pdata->irq);
	data->irq_flags = (pdata->irq_polarity == RMI_IRQ_ACTIVE_HIGH) ?
		IRQF_TRIGGER_RISING : IRQF_TRIGGER_FALLING;
	data->phys = rmi_phys;

	rmi_phys->data = data;
	rmi_phys->dev = &spi->dev;

	rmi_phys->write = rmi_spi_v1_write;
	rmi_phys->write_block = rmi_spi_v1_write_block;
	rmi_phys->read = rmi_spi_v1_read;
	rmi_phys->read_block = rmi_spi_v1_read_block;
	rmi_phys->enable_device = enable_device;
	rmi_phys->disable_device = disable_device;
	data->set_page = rmi_spi_v1_set_page;

	rmi_phys->info.proto = spi_v1_proto_name;

	mutex_init(&data->page_mutex);

	pr_info("%s:    setting the driverdata on the device\n", __func__);

	dev_set_drvdata(&spi->dev, rmi_phys);

	pr_info("%s:    done setting driverdata %s\n", __func__, dev_name(&spi->dev));


	pdata->spi_data.block_delay_us = pdata->spi_data.block_delay_us ?
			pdata->spi_data.block_delay_us : RMI_SPI_BLOCK_DELAY_US;
	pdata->spi_data.read_delay_us = pdata->spi_data.read_delay_us ?
			pdata->spi_data.read_delay_us : RMI_SPI_BYTE_DELAY_US;
	pdata->spi_data.write_delay_us = pdata->spi_data.write_delay_us ?
			pdata->spi_data.write_delay_us : RMI_SPI_BYTE_DELAY_US;
	pdata->spi_data.split_read_block_delay_us =
			pdata->spi_data.split_read_block_delay_us ?
			pdata->spi_data.split_read_block_delay_us :
			RMI_SPI_BLOCK_DELAY_US;
	pdata->spi_data.split_read_byte_delay_us =
			pdata->spi_data.split_read_byte_delay_us ?
			pdata->spi_data.split_read_byte_delay_us :
			RMI_SPI_BYTE_DELAY_US;

	pr_info("%s     configuring GPIOs\n", __func__);

	if (pdata->gpio_config) {
		error = pdata->gpio_config(&spi->dev, true);
		if (error < 0) {
			dev_err(&spi->dev, "Failed to setup GPIOs, code: %d.\n",
				error);
			goto err_data;
		}
	}

	error = rmi_spi_check_device(rmi_phys);
	if (error < 0)
		goto err_data;

	/* check if this is an SPI v2 device */
	dev_info(&spi->dev, "%s:    checking SPI version on RMI device\n", __func__);
	error = rmi_spi_v1_read_block(rmi_phys, RMI_PROTOCOL_VERSION_ADDRESS,
				      buf, 2);

	if (error < 0) {
	        dev_info(&spi->dev, "failed to get SPI version number!\n");
		dev_err(&spi->dev, "failed to get SPI version number!\n");
		goto err_data;
	}

	dev_info(&spi->dev, "SPI version is %d", buf[0]);

	if (buf[0] == 1) {
		/* SPIv2 */
		rmi_phys->write		= rmi_spi_v2_write;
		rmi_phys->write_block	= rmi_spi_v2_write_block;
		rmi_phys->read		= rmi_spi_v2_read;
		data->set_page		= rmi_spi_v2_set_page;

		rmi_phys->info.proto = spi_v2_proto_name;

		if (pdata->irq > 0) {
			init_completion(&data->irq_comp);
			rmi_phys->read_block = rmi_spi_v2_split_read_block;
		} else {
			rmi_phys->read_block = rmi_spi_v2_read_block;
		}
	} else if (buf[0] != 0) {
		dev_err(&spi->dev, "Unrecognized SPI version %d.\n", buf[0]);
		error = -ENODEV;
		goto err_data;
	}

	error = rmi_register_phys_device(rmi_phys);
	if (error) {
		dev_err(&spi->dev, "failed to register physical driver\n");
		goto err_data;
	}

	if (pdata->irq > 0) {
		error = acquire_attn_irq(data);
		if (error < 0) {
			dev_err(&spi->dev, "request_threaded_irq failed %d\n",
				pdata->irq);
			goto err_unregister;
		}
	}

#if defined(CONFIG_RMI4_DEV)
	pr_info("          CONFIG_RMI4_DEV is defined\n");

	error = gpio_export(pdata->irq, false);
	if (error) {
		dev_warn(&spi->dev, "WARNING: Failed to export ATTN gpio!\n");
		error = 0;
	} else {
		error = gpio_export_link(&(rmi_phys->rmi_dev->dev), "attn",
					pdata->irq);
		if (error) {
			dev_warn(&(rmi_phys->rmi_dev->dev), "WARNING: "
				"Failed to symlink ATTN gpio!\n");
			error = 0;
		} else {
			dev_info(&(rmi_phys->rmi_dev->dev),
				"%s: Exported GPIO %d.", __func__, pdata->irq);
		}
	}
#endif /* CONFIG_RMI4_DEV */

	dev_info(&spi->dev, "registered RMI SPI driver\n");
	return 0;

err_unregister:
	rmi_unregister_phys_device(rmi_phys);
err_data:
	kfree(data);
err_phys:
	kfree(rmi_phys);
	return error;
}

static int __devexit rmi_spi_remove(struct spi_device *spi)
{
	struct rmi_phys_device *phys = dev_get_drvdata(&spi->dev);
	struct rmi_device_platform_data *pd = spi->dev.platform_data;
	pr_info("in function ____%s____  \n", __func__);

	rmi_unregister_phys_device(phys);
	kfree(phys->data);
	kfree(phys);

	if (pd->gpio_config)
		pd->gpio_config(&spi->dev, false);

	return 0;
}

static const struct spi_device_id rmi_id[] = {
	{ "rmi", 0 },
	{ "rmi_spi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, rmi_id);

static struct spi_driver rmi_spi_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "rmi_spi",
		//.mod_name = "rmi_spi",
		//.bus    = &spi_bus_type,
	},
	.id_table	= rmi_id,
	.probe		= rmi_spi_probe,
	.remove		= __devexit_p(rmi_spi_remove),
};

static int __init rmi_spi_init(void)
{
        pr_info("%s: registering synaptics spi driver (ref=124)\n", __func__);
        pr_info("             driver.owner        =  0x%x\n", (unsigned int)rmi_spi_driver.driver.owner);
        pr_info("             driver.name         =  %s\n", rmi_spi_driver.driver.name);
        pr_info("             id_table[0].name    =  %s\n", rmi_spi_driver.id_table[0].name );
        pr_info("             id_table[1].name    =  %s\n", rmi_spi_driver.id_table[1].name );
        pr_info("             probe function ptr  =  0x%x\n", (unsigned int)rmi_spi_driver.probe );


	return spi_register_driver(&rmi_spi_driver);
}

static void __exit rmi_spi_exit(void)
{
	spi_unregister_driver(&rmi_spi_driver);
}

MODULE_AUTHOR("Christopher Heiny <cheiny@synaptics.com>");
MODULE_DESCRIPTION("RMI SPI driver");
MODULE_LICENSE("GPL");

module_init(rmi_spi_init);
module_exit(rmi_spi_exit);
