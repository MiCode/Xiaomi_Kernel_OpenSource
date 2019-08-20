/*
 * File: miniisp_spi.c
 * Description: Mini ISP sample codes
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 * 2017/04/11; LouisWang; Initial version
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */



/******Include File******/
/* Linux headers*/
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/buffer_head.h>
#include <linux/of_gpio.h>


#include "include/miniisp.h"
#include "include/miniisp_ctrl.h"
#include "include/miniisp_customer_define.h"
#include "include/miniisp_chip_base_define.h"

#include "include/error/miniisp_err.h"

#ifdef ALTEK_TEST
#include "include/altek_test.h"
#endif

/******Private Constant Definition******/
#define DEBUG_NODE 0

/*#define DEBUG_ALERT*/
#define MINI_ISP_LOG_TAG "[miniisp_intf_spi]"

/*drv debug defination*/
#define _SPI_DEBUG


/****************************************************************************
*                          Private Global Variable                          *
****************************************************************************/
static struct misp_data *misp_intf_spi_data;
struct file *filp[4];
static u8 *spi_bulkbuffer;
/****************************************************************************
*                        Private Function Prototype                         *
****************************************************************************/

#if DEBUG_NODE
static ssize_t
mini_isp_config_show(struct device *dev, struct device_attribute *attr,
			char *buf);
static ssize_t
mini_isp_config_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count);
static DEVICE_ATTR(mini_isp_config, S_IRUGO|S_IWUSR, mini_isp_config_show,
			mini_isp_config_store);
#endif

/**********************************************************************
*                         Public Function                             *
**********************************************************************/


/******Public Function******/
struct misp_data *get_mini_isp_intf_spi(void)
{
	if (!misp_intf_spi_data) {
		misp_err("%s - get pdata error", __func__);
		return NULL;
	} else {
		return misp_intf_spi_data;
	}
}

#ifdef _SPI_DEBUG
void spi_data_debug(const void *buf, int data_len, int dbg_len)
{
	int len = 0, pos = 0;
	unsigned char *char_buf = (unsigned char *)buf;
	   unsigned char string[100], temp[4];

	   memset(string, 0, sizeof(string));

	len = (dbg_len > data_len) ? data_len : dbg_len;

	pos = 0;
	while (len > 0) {
		if (len > 7) {
			misp_info("%02x %02x %02x %02x %02x %02x %02x %02x",
			char_buf[pos], char_buf[pos+1], char_buf[pos+2],
			char_buf[pos+3], char_buf[pos+4], char_buf[pos+5],
			char_buf[pos+6], char_buf[pos+7]);

			len -= 8;
			pos += 8;
		} else {
			for ( ; len > 0; len--) {
				snprintf(temp, 4, "%02x ", char_buf[pos++]);
				strlcat(string, temp, 24);
			}
			misp_info("%s", string);
		}
	}
}
#else
#define spi_data_debug(buf, data_len, dbg_len)
#endif

int mini_isp_dma_write(struct spi_device *spi, u8 *tx_buf, u32 len)
{
	int state;
	dma_addr_t bus_addr;

	struct spi_transfer t = {
		.tx_buf		= tx_buf,
		.len		= len,
	};
	struct spi_message	m;

	misp_info("%s - entering ", __func__);

	spi_message_init(&m);

	bus_addr = dma_map_single(&spi->dev, tx_buf, len, DMA_TO_DEVICE);

	if (!bus_addr) {
		misp_err("%s dma mapping failed.", __func__);
		state = -ENOMEM;
		goto mini_isp_dma_write_end;
	}

	t.tx_dma = bus_addr;
	m.is_dma_mapped = 1;

	spi_message_add_tail(&t, &m);

	state = spi_sync(spi, &m);

	dma_unmap_single(&spi->dev, bus_addr, len, DMA_TO_DEVICE);

	misp_info("%s - leave   ", __func__);

mini_isp_dma_write_end:

	return state;
}

/****************************************************************************
*			Private Function				*
****************************************************************************/

/*read command from device ,this function will block.
 *return 0  successful
 *others	fail
 */
static int mini_isp_intf_spi_read(void *dev, u8 *tx_buf,
			u32 tx_len, u8 *rx_buf, u32 rx_len)
{
	struct spi_transfer t = {
		.tx_buf		= tx_buf,
		.rx_buf		= rx_buf,
		.len		= tx_len + rx_len,
		.delay_usecs = 1,
		.speed_hz	= SPI_BUS_SPEED,
		};
	struct spi_message m;
	struct misp_global_variable *misp_drv_global;
	struct misp_data *devdata = (struct misp_data *)dev;
	int status;

	misp_drv_global = get_mini_isp_global_variable();

	if (!devdata) {
		misp_err("%s - invalid arg devdata = %p, len = %d",
			__func__, devdata, tx_len + rx_len);
		return -EINVAL;
	}

	if (misp_drv_global->before_booting) {
		devdata->cfg.spi->max_speed_hz = SPI_BUS_SPEED_BOOT;
		t.speed_hz = SPI_BUS_SPEED_BOOT;
	} else if (misp_drv_global->spi_low_speed_mode) {
		devdata->cfg.spi->max_speed_hz = SPI_BUS_SPEED_LOW;
		t.speed_hz = SPI_BUS_SPEED_LOW;
	} else {
		/* Set to SPI high speed after booting */
		devdata->cfg.spi->max_speed_hz = SPI_BUS_SPEED;
	}

	status = spi_setup(devdata->cfg.spi);
	if (status < 0)
		misp_err("%s : setup spi error", __func__);

	if (tx_len + rx_len <= 32)
		return spi_write_then_read(devdata->cfg.spi,
			tx_buf, tx_len, rx_buf, rx_len);


	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	return spi_sync(devdata->cfg.spi, &m);
}


/*write commadn to device ,this fucntion will block.
 *return 0  successful
 *others	fail
 */
static int mini_isp_intf_spi_write(void *dev, u8 *tx_buf,
				u8 *rx_buf, u32 len)
{
	struct spi_transfer t = {
		.tx_buf		= tx_buf,
		.rx_buf		= rx_buf,
		.len		= len,
		.delay_usecs = 1,
		.speed_hz	= SPI_BUS_SPEED,
		};
	struct spi_message m;
	struct misp_global_variable *misp_drv_global;
	struct misp_data *devdata = (struct misp_data *)dev;

	misp_drv_global = get_mini_isp_global_variable();
	if (!devdata) {
		misp_err("%s - invalid arg devdata = %p, len = %d",
			__func__, devdata, len);
		return -EINVAL;
	}

	if (misp_drv_global->before_booting)
		t.speed_hz = SPI_BUS_SPEED_BOOT;
	else if (misp_drv_global->spi_low_speed_mode)
		t.speed_hz = SPI_BUS_SPEED_LOW;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	return spi_sync(devdata->cfg.spi, &m);
}

/*
 *write command data to spi
 *return 0  successful
 *others	fail
 */
static int mini_isp_intf_spi_send(void *dev, u32 len)
{
	int status;
	u8 check = USPICTRL_MS_CB_ORG;
	u8 spi_databuffer[TX_BUF_SIZE + 1] = {0};
	struct misp_data *devdata = (struct misp_data *)dev;

	if ((!devdata) || (len > TX_BUF_SIZE)) {
		misp_err("%s - invalid arg devdata = %p, len = %d", __func__,
			devdata, len);
		return -EINVAL;
	}

	/*put the ctrl byte in the first byte, then put the following data.*/
	spi_databuffer[0] = check;

	memcpy(spi_databuffer + 1, devdata->tx_buf, len);

	status = mini_isp_intf_spi_write(devdata, spi_databuffer,
			NULL, len + 1);
	if (status) {
		misp_err("%s - sync error: status = %d", __func__, status);
		return status;
	}

	return status;
}

/* read miniISP using spi ,this fucntion will block.
 *return 0  successful
 *others	fail
 */
static int mini_isp_intf_spi_recv(void *dev, u32 len, bool waitINT)
{
	int status;
	u8 ctrlbyte = USPICTRL_MS_CB_RSP;
	int i = 0;
	int original_altek_spi_mode;
	u32 altek_event_state;
	u8 spi_databuffer[TX_BUF_SIZE + 1] = {0};
	u8 *spi_txbuf = NULL;
	u8 *spi_rxbuf = NULL;
	struct misp_global_variable *dev_global_variable;
	struct misp_data *devdata = (struct misp_data *)dev;

	misp_err("%s - enter", __func__);

	dev_global_variable = get_mini_isp_global_variable();
	original_altek_spi_mode = dev_global_variable->altek_spi_mode;
	if ((!devdata) || (len > RX_BUF_SIZE)) {
		misp_err("%s - invalid arg devdata = %p, len = %d",
			__func__, devdata, len);
		status = -EINVAL;
		goto mini_isp_intf_spi_recv_end;
	}
	spi_txbuf = &spi_databuffer[0];
	spi_rxbuf = &spi_databuffer[1];

	 memset(spi_databuffer, 0, RX_BUF_SIZE + 1);

	if (waitINT) {
		/*wait for the interrupt*/
		status = mini_isp_wait_for_event(MINI_ISP_RCV_CMD_READY);
	} else {
		if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
			(original_altek_spi_mode == ALTEK_SPI_MODE_A))
			mini_isp_a_to_e();
		for (i = 0; i < 200; i++) {
			status = mini_isp_get_altek_status(devdata,
				&altek_event_state);
			if (altek_event_state & COMMAND_COMPLETE) {
				altek_event_state = (altek_event_state &
					~((~0) << 1));
				mini_isp_register_write(
					INTERRUPT_STATUS_REGISTER_ADDR,
					altek_event_state);
				break;
			}
			msleep(20); /* mdelay(5); */
		}
		if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
			(original_altek_spi_mode !=
			dev_global_variable->altek_spi_mode))
			mini_isp_e_to_a();
		if (i >= 200) {
			misp_err("%s time out.", __func__);
			status = ERR_MINIISP_GETDATA_TIMEOUT;
			goto mini_isp_intf_spi_recv_end;
		}
	}

	if (status) {
		misp_err("%s - irq error: status = %d", __func__, status);
		goto mini_isp_intf_spi_recv_end;
	}

	/*send the ctrl byte and get the data in full duplex mode*/
	/*the data is stored from the 2nd byte*/
	*spi_txbuf = ctrlbyte;
	status = mini_isp_intf_spi_read(devdata, spi_txbuf, 1, spi_rxbuf, len);
	if (status) {
		misp_err("%s - sync error: status = %d", __func__, status);
		goto mini_isp_intf_spi_recv_end;
	}

	/* full duplex mode, skip tx_len */
	if (len+1 > 32) {
		if (len > RX_BUF_SIZE - 1)
			misp_err("%s - invalid len = %d",
				__func__, len);
		else
			memcpy(devdata->rx_buf, spi_rxbuf + 1, len);
	} else {
		/* half duplex mode, no need skip tx_len */
		memcpy(devdata->rx_buf, spi_rxbuf, len);
	}

	misp_info("%s - recv buf len = %d:", __func__, len);
	spi_data_debug(devdata->rx_buf, RX_BUF_SIZE, len);

mini_isp_intf_spi_recv_end:
	return status;
}

#if ENABLE_LINUX_FW_LOADER
/*used to send the firmware*/
static int mini_isp_intf_spi_send_bulk(void *dev,
	u32 total_size, u32 block_size, bool is_raw,
	const u8 *spi_Sendbulkbuffer)
{
	int status = 0, count = 0;
	int remain_size, one_size;
	u8 ctrlbyte = USPICTRL_MS_CB_DIS, ack[2];
	int shift = 0;
	struct misp_data *devdata = (struct misp_data *)dev;

	misp_info("%s - entering", __func__);

	if (spi_Sendbulkbuffer != NULL) {
		misp_info("%s start. Total size: %d.", __func__, total_size);
		if (!is_raw) {
			/*send the ctrl byte*/
			status = mini_isp_intf_spi_read(devdata, &ctrlbyte,
						1, ack, 2);
			if (status) {
				misp_err("%s send ctrlbyte fail status: %d",
					__func__, status);
				status = -EINVAL;
				goto T_EXIT;
			}
		}

		/* Allocate basic code bulk buffer*/
		if (total_size > SPI_TX_BULK_SIZE)
			spi_bulkbuffer = kzalloc(SPI_TX_BULK_SIZE, GFP_DMA);
		/* Allocate boot code bulk buffer*/
		else
			spi_bulkbuffer = kzalloc(total_size, GFP_DMA);

		if (!spi_bulkbuffer) {
			status = -EINVAL;
			misp_err("%s - Can not alloc SPI bulk buffer",
				__func__);
			goto T_EXIT;
		}

		for (remain_size = total_size; remain_size > 0;
			remain_size -= one_size) {

			one_size = (remain_size > block_size) ?
				block_size : remain_size;

			misp_info("remain size: %d one_size: %d.",
				remain_size, one_size);

			memcpy(spi_bulkbuffer, (spi_Sendbulkbuffer + shift),
				one_size);
			shift += one_size;

			/*send the data*/
			status = mini_isp_intf_spi_write(devdata,
					spi_bulkbuffer, NULL, one_size);

			if (status != 0) {
				misp_err(
				"%s failed! block:%d status:%d",
				__func__, count, status);
				break;
			}

			count++;
		}

	}
T_EXIT:

	/* Free SPI bulk buffer*/
	if (spi_bulkbuffer != NULL) {
		kfree(spi_bulkbuffer);
		spi_bulkbuffer = NULL;
	}

	if (status != ERR_SUCCESS)
		misp_err("%s error: %d", __func__, status);
	else
		misp_info("%s success", __func__);

	return status;
}

#else
/*used to send the firmware*/
static int mini_isp_intf_spi_send_bulk(void *dev, struct file *filp,
	u32 total_size, u32 block_size, bool is_raw, u8 *spi_Sendbulkbuffer)
{
	int status = 0, count = 0;
	int remain_size, one_size;
	u8 ctrlbyte = USPICTRL_MS_CB_DIS, ack[2];
	mm_segment_t oldfs;
	loff_t  offset;
	int shift = 0;
	struct misp_data *devdata = (struct misp_data *)dev;

	misp_info("%s - entering", __func__);

	if (spi_Sendbulkbuffer != NULL) {
		misp_info("%s start. Total size: %d.", __func__, total_size);
		if (!is_raw) {
			/*send the ctrl byte*/
			status = mini_isp_intf_spi_read(devdata, &ctrlbyte,
						1, ack, 2);
			if (status) {
				misp_err("%s send ctrlbyte fail status: %d",
					__func__, status);
				status = -EINVAL;
				goto T_EXIT;
			}
		}

		/* Allocate basic code bulk buffer*/
		if (total_size > SPI_TX_BULK_SIZE)
			spi_bulkbuffer = kzalloc(SPI_TX_BULK_SIZE, GFP_DMA);
		/* Allocate boot code bulk buffer*/
		else
			spi_bulkbuffer = kzalloc(total_size, GFP_DMA);

		if (!spi_bulkbuffer) {
			status = -EINVAL;
			misp_err("%s - Can not alloc SPI bulk buffer",
				__func__);
			goto T_EXIT;
		}

		for (remain_size = total_size; remain_size > 0;
			remain_size -= one_size) {
			one_size = (remain_size > block_size) ?
				block_size : remain_size;


			/* misp_info("remain size: %d one_size: %d.", */
			/* remain_size, one_size); */

			memcpy(spi_bulkbuffer, (spi_Sendbulkbuffer + shift),
				one_size);
			shift += one_size;

			/*send the data*/
			status = mini_isp_intf_spi_write(devdata,
					spi_bulkbuffer, NULL, one_size);

			if (status != 0) {
				misp_err(
				"%s failed! block:%d status:%d",
				__func__, count, status);
				break;
			}

			/* misp_info("%s write block %d success", */
			/* __func__, count); */

			count++;
		}

	} else {
		oldfs = get_fs();

		set_fs(get_ds());

		misp_info("%s start. Total size: %d", __func__, total_size);

		if (!is_raw) {
			/*send the ctrl byte*/
			status = mini_isp_intf_spi_read(devdata, &ctrlbyte,
						1, ack, 2);
			misp_info("%s is not raw", __func__);
			if (status) {
				misp_err("%s ctrl byte fail status: %d",
				__func__, status);
				status = -EINVAL;
				goto T_EXIT;
			}
		}

		/* Allocate basic code bulk buffer*/
		if (total_size > SPI_TX_BULK_SIZE_BOOT)
			spi_bulkbuffer = kzalloc(SPI_TX_BULK_SIZE, GFP_DMA);
		/* Allocate boot code bulk buffer*/
		else
			spi_bulkbuffer = kzalloc(SPI_TX_BULK_SIZE_BOOT,
						GFP_DMA);


		if (!spi_bulkbuffer) {
			misp_err("%s - Can not alloc SPI bulk buffer",
				__func__);
			status = -EINVAL;
			goto T_EXIT;
		}

		for (remain_size = total_size; remain_size > 0;
			remain_size -= one_size) {
			one_size = (remain_size > block_size) ?
				block_size : remain_size;

			/* misp_info("remain size: %d one_size: %d", */
			/* remain_size, one_size); */

			/*copy the firmware to the buffer*/
			offset = filp->f_pos;
			status = vfs_read(filp, spi_bulkbuffer, one_size,
					&offset);

			if (status == -1) {
				misp_info("%s Read file failed.", __func__);
				break;
			}

			filp->f_pos = offset;

			/*send the data*/
			status = mini_isp_intf_spi_write(devdata,
				spi_bulkbuffer, NULL, one_size);
			if (status != 0) {
				misp_err("%s send fail, block:%d status: %d",
					__func__, count, status);
				break;
			}

			/* misp_info("%s write block %d success", */
			/* __func__, count); */

			count++;
		}
	}
T_EXIT:
	if (filp && (spi_Sendbulkbuffer == NULL))
		set_fs(oldfs);
	/* Free SPI bulk buffer*/
	if (spi_bulkbuffer != NULL) {
		kfree(spi_bulkbuffer);
		spi_bulkbuffer = NULL;
	}

	if (status != ERR_SUCCESS)
		misp_err("%s error: %d", __func__, status);
	else
		misp_info("%s success", __func__);

	return status;
}
#endif

static struct misp_intf_fn_t intf_spi_fn = {
	.send = mini_isp_intf_spi_send,
	.recv = mini_isp_intf_spi_recv,
	.read = mini_isp_intf_spi_read,
	.write = mini_isp_intf_spi_write,
	.send_bulk = mini_isp_intf_spi_send_bulk,
};

static int mini_isp_intf_spi_probe(struct spi_device *spi)
{
	int status = 0;

	misp_info("%s - start", __func__);

	/*step 1: alloc driver data struct*/
	misp_intf_spi_data = kzalloc(sizeof(struct misp_data), GFP_KERNEL);
	if (!misp_intf_spi_data) {
		status = -EINVAL;
		misp_err("%s step1. probe - alloc misp_intf_spi_data error",
			__func__);
		return -ENOMEM;
	}
	misp_info("%s - step1 done.", __func__);

	/*step 2: init driver data*/
	misp_intf_spi_data->cfg.spi = spi;
	misp_intf_spi_data->intf_fn = &intf_spi_fn;
	misp_intf_spi_data->bulk_cmd_blocksize = SPI_TX_BULK_SIZE;

	misp_info("%s - step2 done.", __func__);

	/*step 3: setup spi*/
	spi->mode = SPI_MODE;
	spi->max_speed_hz = SPI_BUS_SPEED_BOOT;
	spi->bits_per_word = 8;
	status = spi_setup(spi);
	if (status < 0) {
		misp_err("%s step3. probe - setup spi error", __func__);
		goto setup_spi_error;
	}
	misp_info("%s - step3 done.", __func__);

	/*step 3: setup recource : gpio, sem*/
	status = mini_isp_setup_resource(&spi->dev, misp_intf_spi_data);
	if (status < 0) {
		misp_err("%s step4. probe - setup resource error", __func__);
		goto setup_spi_error;
	}
	misp_info("%s - step4 done.", __func__);

	/*setp last: set driver_data to device*/
	spi_set_drvdata(spi, misp_intf_spi_data);
	set_mini_isp_data(misp_intf_spi_data, INTF_SPI_READY);

	misp_info("%s -success", __func__);
	mini_isp_get_chip_id();
	goto probe_done;

setup_spi_error:
	kfree(misp_intf_spi_data);
	misp_intf_spi_data = NULL;

probe_done:
	return status;
}

static int mini_isp_intf_spi_remove(struct spi_device *spi)
{
	kfree(misp_intf_spi_data);
	return 0;
}

/*Compatible node must match dts*/
static const struct of_device_id mini_isp_dt_spi_match[] = {
	{  .compatible = "altek,isp",},
	{  },
};
MODULE_DEVICE_TABLE(of, mini_isp_dt_spi_match);


struct spi_driver mini_isp_intf_spi = {
	.driver = {
		.name   = "altek_isp",
		.owner  = THIS_MODULE,
		.of_match_table = mini_isp_dt_spi_match,
	},
	.probe = mini_isp_intf_spi_probe,
	.remove = mini_isp_intf_spi_remove,
};
