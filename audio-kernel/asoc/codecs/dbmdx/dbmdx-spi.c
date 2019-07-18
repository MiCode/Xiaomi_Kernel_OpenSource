/*
 * DSPG DBMDX SPI interface driver
 *
 * Copyright (C) 2014 DSP Group
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
/* #define DEBUG */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/firmware.h>

#include "dbmdx-interface.h"
#include "dbmdx-va-regmap.h"
#include "dbmdx-vqe-regmap.h"
#include "dbmdx-spi.h"

#define DEFAULT_SPI_WRITE_CHUNK_SIZE	8
#define MAX_SPI_WRITE_CHUNK_SIZE	0x40000
#define DEFAULT_SPI_READ_CHUNK_SIZE	8
#define MAX_SPI_READ_CHUNK_SIZE		8192

static DECLARE_WAIT_QUEUE_HEAD(dbmdx_wq);

int spi_set_speed(struct dbmdx_private *p, int index)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	struct spi_device *spi = spi_p->client;
	int ret = 0;
	u32 bits_per_word = 0;
	u32 spi_rate = 0;
	u16 spi_mode = SPI_MODE_0;

	if (index >= DBMDX_VA_NR_OF_SPEEDS) {
		dev_err(spi_p->dev, "%s: Invalid speed index %x\n",
			__func__, index);
		return -EINVAL;
	}

	spi_rate = p->pdata->va_speed_cfg[index].spi_rate -
		(p->pdata->va_speed_cfg[index].spi_rate % 1000);

	bits_per_word = p->pdata->va_speed_cfg[index].spi_rate % 100;

	spi_mode = (u16)(((p->pdata->va_speed_cfg[index].spi_rate % 1000) -
				bits_per_word) / 100);


	if (bits_per_word != 8 && bits_per_word != 16 && bits_per_word != 32)
		bits_per_word = 8;

	if (spi_mode == 0)
		spi_mode = SPI_MODE_0;
	else if (spi_mode == 1)
		spi_mode = SPI_MODE_1;
	else if (spi_mode == 2)
		spi_mode = SPI_MODE_2;
	else if (spi_mode == 3)
		spi_mode = SPI_MODE_3;
	else
		spi_mode = SPI_MODE_0;

	if (spi->max_speed_hz != spi_rate || spi->mode != spi_mode) {

		spi->max_speed_hz = spi_rate;
		spi->mode = spi_mode;

		spi->bits_per_word = bits_per_word;

		spi_p->pdata->bits_per_word = spi->bits_per_word;
		spi_p->pdata->bytes_per_word = spi->bits_per_word / 8;

		dev_info(spi_p->dev,
			"%s Update SPI Max Speed to %d Hz, bpw: %d, mode: %d\n",
			__func__,
			spi->max_speed_hz,
			spi->bits_per_word,
			spi->mode);

		ret = spi_setup(spi);
		if (ret < 0)
			dev_err(spi_p->dev, "%s:failed %x\n", __func__, ret);
	}

	return ret;
}

ssize_t send_spi_cmd_vqe(struct dbmdx_private *p,
	u32 command, u16 *response)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	u8 send[4];
	u8 recv[4];
	ssize_t ret = 0;
	int retries = 10;

	send[0] = (command >> 24) & 0xff;
	send[1] = (command >> 16) & 0xff;
	send[2] = (command >> 8) & 0xff;
	send[3] = command & 0xff;

	ret = send_spi_data(p, send, 4);
	if (ret < 0) {
		dev_err(spi_p->dev,
				"%s: cmd:0x%08X - send_spi_data failed ret:%zd\n",
				__func__, command, ret);
		return ret;
	}

	usleep_range(DBMDX_USLEEP_SPI_VQE_CMD_AFTER_SEND,
		DBMDX_USLEEP_SPI_VQE_CMD_AFTER_SEND + 1000);

	if ((command == (DBMDX_VQE_SET_POWER_STATE_CMD |
			DBMDX_VQE_SET_POWER_STATE_HIBERNATE)) ||
		(command == DBMDX_VQE_SET_SWITCH_TO_BOOT_CMD))
		return 0;

	/* we need additional sleep till system is ready */
	if ((command == (DBMDX_VQE_SET_SYSTEM_CONFIG_CMD |
			DBMDX_VQE_SET_SYSTEM_CONFIG_PRIMARY_CFG)))
		msleep(DBMDX_MSLEEP_SPI_VQE_SYS_CFG_CMD);

	/* read response */
	do {
		ret = spi_read(spi_p->client, recv, 4);
		if (ret < 0) {
			/* Wait before polling again */
			usleep_range(10000, 11000);

			continue;
		}
		/*
		 * Check that the first two bytes of the response match
		 * (the ack is in those bytes)
		 */
		if ((send[0] == recv[0]) && (send[1] == recv[1])) {
			if (response)
				*response = (recv[2] << 8) | recv[3];
			ret = 0;
			break;
		}

		dev_warn(spi_p->dev,
			"%s: incorrect ack (got 0x%.2x%.2x)\n",
			__func__, recv[0], recv[1]);
		ret = -EINVAL;

		/* Wait before polling again */
		usleep_range(10000, 11000);
	} while (--retries);

	if (!retries)
		dev_err(spi_p->dev,
			"%s: cmd:0x%08X - wrong ack, giving up\n",
			__func__, command);

	return ret;
}

ssize_t send_spi_cmd_va(struct dbmdx_private *p, u32 command,
				   u16 *response)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	char tmp[3];
	u8 send[7];
	u8 recv[6] = {0, 0, 0, 0, 0, 0};
	int ret;

	dev_dbg(spi_p->dev, "%s: Send 0x%02x\n", __func__, command);
	if (response) {

		ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
		send[0] = 0;
		send[1] = tmp[0];
		send[2] = tmp[1];
		send[3] = 'r';

		ret = send_spi_data(p, send, 4);
		if (ret != 4)
			goto out;

		usleep_range(DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND,
			DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND + 100);

		if (p->va_debug_mode)
			msleep(DBMDX_MSLEEP_DBG_MODE_CMD_RX);

		ret = 0;

		/* The sleep command cannot be acked before the device
		 * goes to sleep
		 */
		ret = read_spi_data(p, recv, 5);
		if (ret < 0) {
			dev_err(spi_p->dev, "%s:spi_read failed =%d\n",
				__func__, ret);
			return ret;
		}
		recv[5] = 0;
		ret = kstrtou16((const char *)&recv[1], 16, response);
		if (ret < 0) {
			dev_err(spi_p->dev, "%s failed -%d\n", __func__, ret);
			dev_err(spi_p->dev, "%s: %x:%x:%x:%x:\n",
				__func__, recv[1], recv[2],
				recv[3], recv[4]);
			return ret;
		}

		dev_dbg(spi_p->dev, "%s: Received 0x%02x\n", __func__,
			*response);

		ret = 0;
	} else {
		ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
		if (ret < 0)
			goto out;
		send[0] = tmp[0];
		send[1] = tmp[1];
		send[2] = 'w';

		ret = snprintf(tmp, 3, "%02x", (command >> 8) & 0xff);
		if (ret < 0)
			goto out;
		send[3] = tmp[0];
		send[4] = tmp[1];

		ret = snprintf(tmp, 3, "%02x", command & 0xff);
		if (ret < 0)
			goto out;
		send[5] = tmp[0];
		send[6] = tmp[1];

		ret = send_spi_data(p, send, 7);
		if (ret != 7)
			goto out;
		ret = 0;

	}
out:
	usleep_range(DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND_2,
		DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND_2 + 100);

	if (p->va_debug_mode)
		msleep(DBMDX_MSLEEP_DBG_MODE_CMD_TX);

	return ret;
}

ssize_t send_spi_cmd_va_padded(struct dbmdx_private *p,
				u32 command,
				u16 *response)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	char tmp[3];
	u8 send[DBMDX_VA_SPI_CMD_PADDED_SIZE] = {0};
	u8 recv[DBMDX_VA_SPI_CMD_PADDED_SIZE] = {0};
	int ret;
	u32 padded_cmd_w_size = spi_p->pdata->dma_min_buffer_size;
	u32 padded_cmd_r_size = 5;

	dev_dbg(spi_p->dev, "%s: Send 0x%02x\n", __func__, command);
	if (response) {

		ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
		send[padded_cmd_w_size - 3] = tmp[0];
		send[padded_cmd_w_size - 2] = tmp[1];
		send[padded_cmd_w_size - 1] = 'r';

		ret = send_spi_data(p, send, padded_cmd_w_size);
		if (ret != padded_cmd_w_size)
			goto out;

		usleep_range(DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND,
			DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND + 100);

		if (p->va_debug_mode)
			msleep(DBMDX_MSLEEP_DBG_MODE_CMD_RX);

		ret = 0;

		/* the sleep command cannot be acked before the device
		 * goes to sleep
		 */
		ret = read_spi_data(p, recv, padded_cmd_r_size);
		if (ret < 0) {
			dev_err(spi_p->dev, "%s:spi_read failed =%d\n",
				__func__, ret);
			return ret;
		}
		recv[5] = 0;
		ret = kstrtou16((const char *)&recv[1], 16, response);
		if (ret < 0) {
			dev_err(spi_p->dev, "%s failed -%d\n", __func__, ret);
			dev_err(spi_p->dev, "%s: %x:%x:%x:%x:\n",
				__func__, recv[1], recv[2],
				recv[3], recv[4]);
			return ret;
		}

		dev_dbg(spi_p->dev, "%s: Received 0x%02x\n", __func__,
			*response);

		ret = 0;
	} else {
		ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
		if (ret < 0)
			goto out;
		send[padded_cmd_w_size - 7] = tmp[0];
		send[padded_cmd_w_size - 6] = tmp[1];
		send[padded_cmd_w_size - 5] = 'w';

		ret = snprintf(tmp, 3, "%02x", (command >> 8) & 0xff);
		if (ret < 0)
			goto out;
		send[padded_cmd_w_size - 4] = tmp[0];
		send[padded_cmd_w_size - 3] = tmp[1];

		ret = snprintf(tmp, 3, "%02x", command & 0xff);
		if (ret < 0)
			goto out;
		send[padded_cmd_w_size - 2] = tmp[0];
		send[padded_cmd_w_size - 1] = tmp[1];

		ret = send_spi_data(p, send, padded_cmd_w_size);
		if (ret != padded_cmd_w_size)
			goto out;
		ret = 0;

	}
out:
	usleep_range(DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND_2,
		DBMDX_USLEEP_SPI_VA_CMD_AFTER_SEND_2 + 100);

	if (p->va_debug_mode)
		msleep(DBMDX_MSLEEP_DBG_MODE_CMD_TX);

	return ret;
}

ssize_t read_spi_data(struct dbmdx_private *p, void *buf, size_t len)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	size_t count = spi_p->pdata->read_chunk_size;
	u32 bytes_per_word = spi_p->pdata->bytes_per_word;
	u8 *recv = spi_p->pdata->recv;
	ssize_t i;
	size_t pad_size = 0;
	int ret;
	u8 *d = (u8 *)buf;
	/* if stuck for more than 10s, something is wrong */
	unsigned long timeout = jiffies + msecs_to_jiffies(10000);

	for (i = 0; i < len; i += count) {
		if ((i + count) > len) {
			count = len - i;
			if (count % (size_t)bytes_per_word != 0)
				pad_size = (size_t)bytes_per_word -
				(size_t)(count % (size_t)bytes_per_word);
			count = count + pad_size;
		}

		ret =  spi_read(spi_p->client, recv, count);
		if (ret < 0) {
			dev_err(spi_p->dev, "%s: spi_read failed\n",
				__func__);
			i = -EIO;
			goto out;
		}
		memcpy(d + i, recv, count - pad_size);

		if (!time_before(jiffies, timeout)) {
			dev_err(spi_p->dev,
				"%s: read data timed out after %zd bytes\n",
				__func__, i);
			i = -ETIMEDOUT;
			goto out;
		}
	}

	return len;
out:
	return i;
}


ssize_t write_spi_data(struct dbmdx_private *p, const u8 *buf,
			      size_t len)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	struct spi_device *spi = spi_p->client;
	int rc;

	rc = spi_write(spi, buf, len);
	if (rc != 0) {
		dev_err(spi_p->dev, "%s(): error %d writing SR\n",
				__func__, rc);
		return rc;
	} else
		return len;
}


ssize_t send_spi_data(struct dbmdx_private *p, const void *buf,
			      size_t len)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	u32 bytes_per_word = spi_p->pdata->bytes_per_word;
	int ret = 0;
	const u8 *cmds = (const u8 *)buf;
	size_t to_copy = len;
	size_t max_size = (size_t)(spi_p->pdata->write_chunk_size);
	size_t pad_size = 0;
	size_t cur_send_size = 0;
	u8 *send = spi_p->pdata->send;

	while (to_copy > 0) {
		if (to_copy < max_size) {
			memset(send, 0, max_size);
			memcpy(send, cmds, to_copy);

			if (to_copy % (size_t)bytes_per_word != 0)
				pad_size = (size_t)bytes_per_word -
				(size_t)(to_copy % (size_t)bytes_per_word);

			cur_send_size = to_copy + pad_size;
		} else {
			memcpy(send, cmds, max_size);
			cur_send_size = max_size;
		}

		ret = write_spi_data(p, send, cur_send_size);
		if (ret < 0) {
			dev_err(spi_p->dev, "%s: Failed ret=%d\n",
				__func__, ret);
			break;
		}
		to_copy -= (ret - pad_size);
		cmds += (ret - pad_size);
	}

	return len - to_copy;
}

int send_spi_cmd_boot(struct dbmdx_private *p, u32 command)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	u8 send[4];
	int ret = 0;


	dev_dbg(spi_p->dev, "%s: command = %x\n", __func__, command);
	send[0] = 0;
	send[1] = 0;
	send[2] = (command >> 16) & 0xff;
	send[3] = (command >>  8) & 0xff;

	ret = send_spi_data(p, send, 4);
	if (ret < 0) {
		dev_err(spi_p->dev, "%s: ret = %d\n", __func__, ret);
		return ret;
	}

	/* A host command received will blocked until the current audio frame
	 *  processing is finished, which can take up to 10 ms
	 */
	usleep_range(DBMDX_USLEEP_SPI_VA_CMD_AFTER_BOOT,
		DBMDX_USLEEP_SPI_VA_CMD_AFTER_BOOT + 1000);

	return ret;
}

int spi_verify_boot_checksum(struct dbmdx_private *p,
	const void *checksum, size_t chksum_len)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	int ret;
	u8 rx_checksum[11] = {0};

	if (!checksum)
		return 0;

	if (chksum_len > 8) {
		dev_err(spi_p->dev, "%s: illegal checksum length\n", __func__);
		return -EINVAL;
	}

	ret = send_spi_cmd_boot(p, DBMDX_READ_CHECKSUM);

	if (ret < 0) {
		dev_err(spi_p->dev, "%s: could not read checksum\n", __func__);
		return -EIO;
	}

	ret = read_spi_data(p, (void *)rx_checksum, chksum_len + 3);

	if (ret < 0) {
		dev_err(spi_p->dev, "%s: could not read checksum data\n",
			__func__);
		return -EIO;
	}

	ret = p->verify_checksum(p, checksum, &rx_checksum[3], chksum_len);
	if (ret) {
		dev_err(spi_p->dev, "%s: checksum mismatch\n", __func__);
		return -EILSEQ;
	}

	return 0;
}

int spi_verify_chip_id(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	int ret;
	u8 idr_read_cmd[] = {0x5A, 0x07, 0x68, 0x00, 0x00, 0x03};
	u8 idr_read_result[7] = {0};
	u8 chip_rev_id_low_a = 0;
	u8 chip_rev_id_low_b = 0;
	u8 chip_rev_id_high = 0;

	u8 recv_chip_rev_id_high = 0;
	u8 recv_chip_rev_id_low = 0;

	if (p->cur_firmware_id == DBMDX_FIRMWARE_ID_DBMD2) {
		idr_read_cmd[2] = 0x68;
		chip_rev_id_high = 0x0d;
		chip_rev_id_low_a = 0xb0;
		chip_rev_id_low_b = 0xb1;
	} else if (p->cur_firmware_id == DBMDX_FIRMWARE_ID_DBMD4) {
		idr_read_cmd[2] = 0x74;
		chip_rev_id_high = 0xdb;
		chip_rev_id_low_a = 0x40;
		chip_rev_id_low_b = 0x40;
	} else if (p->cur_firmware_id == DBMDX_FIRMWARE_ID_DBMD6) {
		idr_read_cmd[2] = 0x74;
		chip_rev_id_high = 0xdb;
		chip_rev_id_low_a = 0x60;
		chip_rev_id_low_b = 0x60;
	} else {
		idr_read_cmd[2] = 0x74;
		chip_rev_id_high = 0xdb;
		chip_rev_id_low_a = 0x80;
		chip_rev_id_low_b = 0x80;
	}

	ret = send_spi_data(p, idr_read_cmd, 6);
	if (ret < 0) {
		dev_err(spi_p->dev, "%s: idr_read_cmd ret = %d\n",
			__func__, ret);
		return ret;
	}

	usleep_range(DBMDX_USLEEP_SPI_VA_CMD_AFTER_BOOT,
		DBMDX_USLEEP_SPI_VA_CMD_AFTER_BOOT + 1000);

	ret = read_spi_data(p, (void *)idr_read_result, 7);

	if (ret < 0) {
		dev_err(spi_p->dev, "%s: could not idr register data\n",
			__func__);
		return -EIO;
	}
	/* Verify answer */
	if ((idr_read_result[1] != idr_read_cmd[0]) ||
		(idr_read_result[2] != idr_read_cmd[1]) ||
		(idr_read_result[5] != 0x00) ||
		(idr_read_result[6] != 0x00)) {
		dev_err(spi_p->dev, "%s: Wrong IDR resp: %x:%x:%x:%x:%x:%x\n",
				__func__,
				idr_read_result[1],
				idr_read_result[2],
				idr_read_result[3],
				idr_read_result[4],
				idr_read_result[5],
				idr_read_result[6]);
		return -EIO;
	}
	recv_chip_rev_id_high = idr_read_result[4];
	recv_chip_rev_id_low = idr_read_result[3];

	if ((recv_chip_rev_id_high != chip_rev_id_high) ||
		((recv_chip_rev_id_low != chip_rev_id_low_a) &&
		(recv_chip_rev_id_low != chip_rev_id_low_b))) {

		dev_err(spi_p->dev,
			"%s: Wrong chip ID: Received 0x%2x%2x Expected: 0x%2x%2x | 0x%2x%2x\n",
				__func__,
				recv_chip_rev_id_high,
				recv_chip_rev_id_low,
				chip_rev_id_high,
				chip_rev_id_low_a,
				chip_rev_id_high,
				chip_rev_id_low_b);
		return -EILSEQ;
	}

	dev_info(spi_p->dev,
			"%s: Chip ID was successfully verified: 0x%2x%2x\n",
				__func__,
				recv_chip_rev_id_high,
				recv_chip_rev_id_low);
	return 0;
}

static int spi_can_boot(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}

static int spi_prepare_boot(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	int ret = 0;

	dev_dbg(spi_p->dev, "%s\n", __func__);

	ret = spi_set_speed(p, DBMDX_VA_SPEED_MAX);

	return ret;
}

static int spi_finish_boot(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	int ret = 0;

	ret = spi_set_speed(p, DBMDX_VA_SPEED_NORMAL);
	if (ret < 0)
		dev_err(spi_p->dev, "%s:failed %x\n", __func__, ret);

	return ret;
}

static int spi_dump_state(struct chip_interface *chip, char *buf)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)chip->pdata;
	int off = 0;

	dev_dbg(spi_p->dev, "%s\n", __func__);

	off += snprintf(buf + off, PAGE_SIZE - off,
				"\t===SPI Interface  Dump====\n");
	off += snprintf(buf + off, PAGE_SIZE - off,
				"\tSPI Write Chunk Size:\t\t%d\n",
				spi_p->pdata->write_chunk_size);
	off += snprintf(buf + off, PAGE_SIZE - off,
					"\tSPI Read Chunk Size:\t\t%d\n",
				spi_p->pdata->read_chunk_size);

	off += snprintf(buf + off, PAGE_SIZE - off,
				"\tSPI DMA Min Buffer Size:\t\t%d\n",
				spi_p->pdata->dma_min_buffer_size);

	off += snprintf(buf + off, PAGE_SIZE - off,
					"\tInterface resumed:\t%s\n",
			spi_p->interface_enabled ? "ON" : "OFF");

	return off;
}

static int spi_set_va_firmware_ready(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}


static int spi_set_vqe_firmware_ready(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}

static void spi_transport_enable(struct dbmdx_private *p, bool enable)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(spi_p->dev, "%s (%s)\n", __func__, enable ? "ON" : "OFF");

	if (enable) {

#ifdef CONFIG_PM_WAKELOCKS
		__pm_stay_awake(&spi_p->ps_nosuspend_wl);
#endif
		ret = wait_event_interruptible(dbmdx_wq,
			spi_p->interface_enabled);

		if (ret)
			dev_dbg(spi_p->dev,
				"%s, waiting for interface was interrupted",
				__func__);
		else
			dev_dbg(spi_p->dev, "%s, interface is active\n",
				__func__);
	}

	if (enable) {
		p->wakeup_set(p);
		msleep(DBMDX_MSLEEP_SPI_WAKEUP);
	} else {
#ifdef CONFIG_PM_WAKELOCKS
		__pm_relax(&spi_p->ps_nosuspend_wl);
#endif
		p->wakeup_release(p);
	}
}

static void spi_resume(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);

	spi_interface_resume(spi_p);
}


void spi_interface_resume(struct dbmdx_spi_private *spi_p)
{
	dev_dbg(spi_p->dev, "%s\n", __func__);

	spi_p->interface_enabled = 1;
	wake_up_interruptible(&dbmdx_wq);
}

static void spi_suspend(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);

	spi_interface_suspend(spi_p);
}


void spi_interface_suspend(struct dbmdx_spi_private *spi_p)
{
	dev_dbg(spi_p->dev, "%s\n", __func__);

	spi_p->interface_enabled = 0;
}

static int spi_prepare_buffering(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}

#define DBMDX_AUDIO_DEBUG_EXT 0
#define DBMDX_SAMPLE_CHUNK_VERIFICATION_FLAG	0xdbd0

static int spi_read_audio_data(struct dbmdx_private *p,
	void *buf,
	size_t samples,
	bool to_read_metadata,
	size_t *available_samples,
	size_t *data_offset)
{
	size_t bytes_to_read;
	int ret;
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);

	ret = send_spi_cmd_va(p,
			DBMDX_VA_READ_AUDIO_BUFFER | samples, NULL);

	if (ret) {
		dev_err(p->dev, "%s: failed to request %zu audio samples\n",
			__func__, samples);
		ret = -1;
		goto out;
	}

	*available_samples = 0;

	if (to_read_metadata)
		*data_offset = 8;
	else
		*data_offset = 2;

	bytes_to_read = samples * 8 * p->bytes_per_sample + *data_offset;

	ret = read_spi_data(p, buf, bytes_to_read);

	if (ret != bytes_to_read) {
		dev_err(p->dev,
			"%s: read audio failed, %zu bytes to read, res(%d)\n",
			__func__,
			bytes_to_read,
			ret);
		ret = -1;
		goto out;
	}

	/* Word #4 contains current number of available samples */
	if (to_read_metadata) {
		u16 verif_flag;
		*available_samples = (size_t)(((u16 *)buf)[3]);
		verif_flag = (u16)(((u16 *)buf)[2]);

#if DBMDX_AUDIO_DEBUG_EXT
		dev_err(spi_p->dev, "%s: %x:%x:%x:%x:%x:%x:%x:%x\n",
				__func__,
				((u8 *)buf)[0],
				((u8 *)buf)[1],
				((u8 *)buf)[2],
				((u8 *)buf)[3],
				((u8 *)buf)[4],
				((u8 *)buf)[5],
				((u8 *)buf)[6],
				((u8 *)buf)[7]);
#endif

		if (verif_flag != DBMDX_SAMPLE_CHUNK_VERIFICATION_FLAG) {

			*available_samples = 0;

			dev_err(p->dev,
				"%s: Flag verificaiton failed %x:%x\n",
				__func__,
				((u8 *)buf)[4],
				((u8 *)buf)[5]);

			ret = -1;
			goto out;
		}
	} else
		*available_samples = samples;

	ret = samples;

	/* FW performes SPI reset after each chunk transaction
	 * Thus delay is required
	 */
	usleep_range(DBMDX_USLEEP_SPI_AFTER_CHUNK_READ,
		DBMDX_USLEEP_SPI_AFTER_CHUNK_READ + 100);
out:
	return ret;
}

static int spi_finish_buffering(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(spi_p->dev, "%s\n", __func__);


	return ret;
}

static int spi_prepare_amodel_loading(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	return 0;
}

static int spi_finish_amodel_loading(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s\n", __func__);
	/* do the same as for finishing buffering */

	return 0;
}

static u32 spi_get_read_chunk_size(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s SPI read chunk is %u\n",
		__func__, spi_p->pdata->read_chunk_size);

	return spi_p->pdata->read_chunk_size;
}

static u32 spi_get_write_chunk_size(struct dbmdx_private *p)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	dev_dbg(spi_p->dev, "%s SPI write chunk is %u\n",
		__func__, spi_p->pdata->write_chunk_size);

	return spi_p->pdata->write_chunk_size;
}

static int spi_set_read_chunk_size(struct dbmdx_private *p, u32 size)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	if (size > MAX_SPI_READ_CHUNK_SIZE) {
		dev_err(spi_p->dev,
			"%s Error setting SPI read chunk. Max chunk size: %u\n",
		__func__, MAX_SPI_READ_CHUNK_SIZE);
		return -EINVAL;
	} else if ((size % 4) != 0) {
		dev_err(spi_p->dev,
			"%s Error SPI read chunk should be multiply of 4\n",
		__func__);
		return -EINVAL;
	} else if (size == 0)
		spi_p->pdata->read_chunk_size = DEFAULT_SPI_READ_CHUNK_SIZE;
	else
		spi_p->pdata->read_chunk_size = size;

	dev_dbg(spi_p->dev, "%s SPI read chunk was set to %u\n",
		__func__, spi_p->pdata->read_chunk_size);

	return 0;
}

static int spi_set_write_chunk_size(struct dbmdx_private *p, u32 size)
{
	struct dbmdx_spi_private *spi_p =
				(struct dbmdx_spi_private *)p->chip->pdata;

	if (size > MAX_SPI_WRITE_CHUNK_SIZE) {
		dev_err(spi_p->dev,
			"%s Error setting SPI write chunk. Max chunk size: %u\n",
		__func__, MAX_SPI_WRITE_CHUNK_SIZE);
		return -EINVAL;
	} else if ((size % 4) != 0) {
		dev_err(spi_p->dev,
			"%s Error SPI write chunk should be multiply of 4\n",
		__func__);
		return -EINVAL;
	} else if (size == 0)
		spi_p->pdata->write_chunk_size = DEFAULT_SPI_WRITE_CHUNK_SIZE;
	else
		spi_p->pdata->write_chunk_size = size;

	dev_dbg(spi_p->dev, "%s SPI write chunk was set to %u\n",
		__func__, spi_p->pdata->write_chunk_size);

	return 0;
}

int spi_common_probe(struct spi_device *client)
{
#ifdef CONFIG_OF
	struct  device_node *np;
#endif
	int ret;
	struct dbmdx_spi_private *p;
	struct dbmdx_spi_data *pdata;

	dev_dbg(&client->dev, "%s(): dbmdx\n", __func__);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return -ENOMEM;

	p->client = client;
	p->dev = &client->dev;

	p->chip.pdata = p;
#ifdef CONFIG_OF
	np = p->dev->of_node;
	if (!np) {
		dev_err(p->dev, "%s: no devicetree entry\n", __func__);
		ret = -EINVAL;
		goto out_err_kfree;
	}

	pdata = kzalloc(sizeof(struct dbmdx_spi_data), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto out_err_kfree;
	}
#else
	pdata = dev_get_platdata(&client->dev);
	if (pdata == NULL) {
		dev_err(p->dev, "%s: dbmdx, no platform data found\n",
			__func__);
		return -ENODEV;
	}
#endif

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "spi-max-frequency",
		&(pdata->spi_speed));
	if (ret && ret != -EINVAL)
		pdata->spi_speed = 2000000;
#endif
	dev_dbg(p->dev, "%s: spi speed is %u\n", __func__, pdata->spi_speed);

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "read-chunk-size",
		&pdata->read_chunk_size);
	if (ret != 0) {
		/*
		 * read-chunk-size not set, set it to default
		 */
		pdata->read_chunk_size = DEFAULT_SPI_READ_CHUNK_SIZE;
		dev_info(p->dev,
			"%s: Setting spi read chunk to default val: %u bytes\n",
			__func__, pdata->read_chunk_size);
	}
#endif
	if (pdata->read_chunk_size % 4 != 0)
		pdata->read_chunk_size += (4 - (pdata->read_chunk_size % 4));

	if (pdata->read_chunk_size > MAX_SPI_READ_CHUNK_SIZE)
		pdata->read_chunk_size = MAX_SPI_READ_CHUNK_SIZE;
	if (pdata->read_chunk_size == 0)
		pdata->read_chunk_size = DEFAULT_SPI_READ_CHUNK_SIZE;

	dev_info(p->dev, "%s: Setting spi read chunk to %u bytes\n",
			__func__, pdata->read_chunk_size);

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "write-chunk-size",
		&pdata->write_chunk_size);
	if (ret != 0) {
		/*
		 * write-chunk-size not set, set it to default
		 */
		pdata->write_chunk_size = DEFAULT_SPI_WRITE_CHUNK_SIZE;
		dev_info(p->dev,
			"%s: Setting spi write chunk to default val: %u bytes\n",
			__func__, pdata->write_chunk_size);
	}
#endif
	if (pdata->write_chunk_size % 4 != 0)
		pdata->write_chunk_size += (4 - (pdata->write_chunk_size % 4));

	if (pdata->write_chunk_size > MAX_SPI_WRITE_CHUNK_SIZE)
		pdata->write_chunk_size = MAX_SPI_WRITE_CHUNK_SIZE;
	if (pdata->write_chunk_size == 0)
		pdata->write_chunk_size = DEFAULT_SPI_WRITE_CHUNK_SIZE;

	dev_info(p->dev, "%s: Setting spi write chunk to %u bytes\n",
			__func__, pdata->write_chunk_size);

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "dma_min_buffer_size",
		&pdata->dma_min_buffer_size);
	if (ret != 0) {
		/*
		 * read-chunk-size not set, set it to default
		 */
		pdata->dma_min_buffer_size = 0;
		dev_info(p->dev,
			"%s: Setting Min DMA Cmd Size to default: %u bytes\n",
			__func__, pdata->dma_min_buffer_size);
	}
#endif
	if (pdata->dma_min_buffer_size > DBMDX_VA_SPI_CMD_PADDED_SIZE)
		pdata->dma_min_buffer_size = DBMDX_VA_SPI_CMD_PADDED_SIZE;
	if (pdata->dma_min_buffer_size < 7 && pdata->dma_min_buffer_size > 0)
		pdata->dma_min_buffer_size = 7;

	dev_info(p->dev, "%s: Setting Min DMA Cmd Size to default: %u bytes\n",
			__func__, pdata->dma_min_buffer_size);

	p->pdata = pdata;

	pdata->send = kmalloc(MAX_SPI_WRITE_CHUNK_SIZE, GFP_KERNEL | GFP_DMA);
	if (!pdata->send)
		goto out_err_mem_free;

	pdata->recv = kmalloc(MAX_SPI_READ_CHUNK_SIZE, GFP_KERNEL | GFP_DMA);
	if (!pdata->recv)
		goto out_err_mem_free1;

#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_init(&p->ps_nosuspend_wl, "dbmdx_nosuspend_wakelock_spi");
#endif

	/* fill in chip interface functions */
	p->chip.can_boot = spi_can_boot;
	p->chip.prepare_boot = spi_prepare_boot;
	p->chip.finish_boot = spi_finish_boot;
	p->chip.dump = spi_dump_state;
	p->chip.set_va_firmware_ready = spi_set_va_firmware_ready;
	p->chip.set_vqe_firmware_ready = spi_set_vqe_firmware_ready;
	p->chip.transport_enable = spi_transport_enable;
	p->chip.read = read_spi_data;
	p->chip.write = send_spi_data;
	p->chip.send_cmd_vqe = send_spi_cmd_vqe;
	p->chip.send_cmd_va = send_spi_cmd_va;
	p->chip.send_cmd_boot = send_spi_cmd_boot;
	p->chip.verify_boot_checksum = spi_verify_boot_checksum;
	p->chip.prepare_buffering = spi_prepare_buffering;
	p->chip.read_audio_data = spi_read_audio_data;
	p->chip.finish_buffering = spi_finish_buffering;
	p->chip.prepare_amodel_loading = spi_prepare_amodel_loading;
	p->chip.finish_amodel_loading = spi_finish_amodel_loading;
	p->chip.get_write_chunk_size = spi_get_write_chunk_size;
	p->chip.get_read_chunk_size = spi_get_read_chunk_size;
	p->chip.set_write_chunk_size = spi_set_write_chunk_size;
	p->chip.set_read_chunk_size = spi_set_read_chunk_size;
	p->chip.resume = spi_resume;
	p->chip.suspend = spi_suspend;

	p->interface_enabled = 1;

	spi_set_drvdata(client,  &p->chip);

	dev_info(&client->dev, "%s: successfully probed\n", __func__);
	ret = 0;
	goto out;
out_err_mem_free1:
	kfree(pdata->send);
#ifdef CONFIG_OF
out_err_kfree:
#endif
out_err_mem_free:
	kfree(p);
out:
	return ret;
}

int spi_common_remove(struct spi_device *client)
{
	struct chip_interface *ci = spi_get_drvdata(client);
	struct dbmdx_spi_private *p = (struct dbmdx_spi_private *)ci->pdata;

#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_trash(&p->ps_nosuspend_wl);
#endif
	kfree(p->pdata->send);
	kfree(p->pdata->recv);
	kfree(p);

	spi_set_drvdata(client, NULL);

	return 0;
}


