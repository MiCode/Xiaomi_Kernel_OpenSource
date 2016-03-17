/*
 * STMicroelectronics st_sensor_hub ymodem protocol
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Denis Ciocca <denis.ciocca@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/iio/iio.h>
#include <linux/device.h>

#include "st_hub_ymodem.h"

static u16 update_crc16(u16 crc_in, u8 byte)
{
	uint32_t crc = crc_in;
	uint32_t in = byte | 0x100;

	do {
		crc <<= 1;
		in <<= 1;

		if (in & 0x100)
			++crc;

		if (crc & 0x10000)
			crc ^= 0x1021;

	} while (!(in & 0x10000));

	return crc & 0xffffu;
}

static u16 cal_crc16(const u8 *data, uint32_t size)
{
	uint32_t crc = 0;
	const u8 *data_end = data + size;

	while (data < data_end)
		crc = update_crc16(crc, *data++);

	crc = update_crc16(crc, 0);
	crc = update_crc16(crc, 0);

	return crc & 0xffffu;
}

static int st_hub_ymodem_crc_check(struct st_hub_data *hdata,
					u8 *packet_data, size_t packet_size)
{
	int err;
	u8 data;
	u16 temp_crc;

	temp_crc = cal_crc16(packet_data, packet_size);

	data = temp_crc >> 8;
	err = hdata->tf->write_rl(hdata->dev, 1, &data);
	if (err < 0)
		return err;

	data = temp_crc & 0xff;
	err = hdata->tf->write_rl(hdata->dev, 1, &data);
	if (err < 0)
		return err;

	msleep(ST_HUB_YMODEM_SLEEP_MS);

	return 0;
}

static int st_hub_ymodem_send_data(struct st_hub_data *hdata,
							u8 *data, size_t len)
{
	int err;

	while (len > 0) {
		if (len > YMODEM_PACKET_SPLIT_SIZE) {
			err = hdata->tf->write_rl(hdata->dev,
						YMODEM_PACKET_SPLIT_SIZE, data);
			if (err < 0)
				return err;

			data += YMODEM_PACKET_SPLIT_SIZE;
			len -= YMODEM_PACKET_SPLIT_SIZE;
		} else {
				err = hdata->tf->write_rl(hdata->dev,
								len, data);
			if (err < 0)
				return err;

			len = 0;
		}
	}

	return 0;
}

static int st_hub_ymodem_send_packetn(struct st_hub_data *hdata,
			u8 *data, size_t len, int num, size_t packet_size)
{
	int err, errors = 0;
	u8 *packet_data, ack_data;

	packet_data = kzalloc(packet_size +
					YMODEM_PACKET_HEADER_SIZE, GFP_KERNEL);
	if (!packet_data)
		return -ENOMEM;

	if (packet_size == YMODEM_PACKET_1K_SIZE)
		packet_data[0] = YMODEM_STX_CODE;
	else
		packet_data[0] = YMODEM_SOH_CODE;

	packet_data[1] = (u8)num;
	packet_data[2] = ~(u8)num;

	memcpy(&packet_data[YMODEM_PACKET_HEADER_SIZE], data, len);

	do {
		err = st_hub_ymodem_send_data(hdata, packet_data,
				packet_size + YMODEM_PACKET_HEADER_SIZE);
		if (err < 0)
			goto st_hub_ymodem_free_packet_data;

		err = st_hub_ymodem_crc_check(hdata,
			&packet_data[YMODEM_PACKET_HEADER_SIZE], packet_size);
		if (err < 0)
			goto st_hub_ymodem_free_packet_data;

		err = hdata->tf->read_rl(hdata->dev, 1, &ack_data);
		if (err < 0)
			goto st_hub_ymodem_free_packet_data;

		if (ack_data != YMODEM_ACK_CODE)
			errors++;

	} while ((ack_data != YMODEM_ACK_CODE) &&
					(errors < ST_HUB_YMODEM_NUM_RETRY));

	if (errors >= ST_HUB_YMODEM_NUM_RETRY)
		err = -EINVAL;

st_hub_ymodem_free_packet_data:
	kfree(packet_data);
	return err;
}

static int st_hub_ymodem_send_packet0(struct st_hub_data *hdata, size_t len)
{
	int err, errors = 0;
	u8 *packet_data, ack_data;
	size_t offset, filename_len;

	filename_len = strlen(hdata->get_dev_name(hdata->dev));

	if (filename_len > 9)
		return -EINVAL;

	packet_data = kzalloc(YMODEM_PACKET_128_SIZE +
					YMODEM_PACKET_HEADER_SIZE, GFP_KERNEL);
	if (!packet_data)
		return -ENOMEM;

	packet_data[0] = YMODEM_SOH_CODE;
	packet_data[1] = 0x00;
	packet_data[2] = 0xff;

	memcpy(&packet_data[YMODEM_PACKET_HEADER_SIZE],
				hdata->get_dev_name(hdata->dev), filename_len);
	offset = YMODEM_PACKET_HEADER_SIZE + filename_len;

	packet_data[offset++] = 0x00;

	sprintf(&packet_data[offset], "%ld", len);

	do {
		err = st_hub_ymodem_send_data(hdata, packet_data,
			YMODEM_PACKET_128_SIZE + YMODEM_PACKET_HEADER_SIZE);
		if (err < 0)
			goto st_hub_ymodem_free_packet_data;

		err = st_hub_ymodem_crc_check(hdata,
					&packet_data[YMODEM_PACKET_HEADER_SIZE],
					YMODEM_PACKET_128_SIZE);
		if (err < 0)
			goto st_hub_ymodem_free_packet_data;

		err = hdata->tf->read_rl(hdata->dev, 1, &ack_data);
		if (err < 0)
			goto st_hub_ymodem_free_packet_data;

		if (ack_data != YMODEM_ACK_CODE)
					errors++;

		} while ((ack_data != YMODEM_ACK_CODE) &&
					(errors < ST_HUB_YMODEM_NUM_RETRY));

		if (errors >= ST_HUB_YMODEM_NUM_RETRY)
			err = -EINVAL;

st_hub_ymodem_free_packet_data:
	kfree(packet_data);
	return err;
}

static int st_hub_ymodem_send_eot(struct st_hub_data *hdata)
{
	u8 data;
	int err, errors = 0;

	do {
		data = YMODEM_EOT_CODE;

		err = hdata->tf->write_rl(hdata->dev, 1, &data);
		if (err < 0)
			return err;

		msleep(ST_HUB_YMODEM_SLEEP_MS);

		err = hdata->tf->read_rl(hdata->dev, 1, &data);
		if (err < 0)
			return err;

		if (data != YMODEM_ACK_CODE)
			errors++;

	} while ((data != YMODEM_ACK_CODE) &&
					(errors < ST_HUB_YMODEM_NUM_RETRY));

	if (errors >= ST_HUB_YMODEM_NUM_RETRY)
		return -EINVAL;

	return 0;
}

static int st_hub_ymodem_send_last_packet(struct st_hub_data *hdata)
{
	int err, errors = 0;
	u8 *packet_data, ack_data;

	packet_data = kzalloc(YMODEM_PACKET_128_SIZE +
					YMODEM_PACKET_HEADER_SIZE, GFP_KERNEL);
	if (!packet_data)
		return -ENOMEM;

	packet_data[0] = YMODEM_SOH_CODE;
	packet_data[1] = 0x00;
	packet_data[2] = 0xff;

	do {
		err = st_hub_ymodem_send_data(hdata, packet_data,
			YMODEM_PACKET_128_SIZE + YMODEM_PACKET_HEADER_SIZE);
		if (err < 0)
			goto st_hub_ymodem_free_packet_data;

		err = st_hub_ymodem_crc_check(hdata,
					&packet_data[YMODEM_PACKET_HEADER_SIZE],
					YMODEM_PACKET_128_SIZE);
		if (err < 0)
			goto st_hub_ymodem_free_packet_data;

		err = hdata->tf->read_rl(hdata->dev, 1, &ack_data);
		if (err < 0)
			goto st_hub_ymodem_free_packet_data;

		if (ack_data != YMODEM_ACK_CODE)
			errors++;

	} while ((ack_data != YMODEM_ACK_CODE) &&
					(errors < ST_HUB_YMODEM_NUM_RETRY));

	if (errors >= ST_HUB_YMODEM_NUM_RETRY)
		err = -EINVAL;

st_hub_ymodem_free_packet_data:
	kfree(packet_data);
	return err;
}

static int st_hub_ymodem_transmit(struct st_hub_data *hdata,
						u8 *fw_data, size_t fw_len)
{
	int err, packet_num = 1;
	size_t packet_size = YMODEM_PACKET_128_SIZE;
	size_t byte_sent = 0, byte_remaining, data_send;

	err = st_hub_ymodem_send_packet0(hdata, fw_len);
	if (err < 0) {
		dev_err(hdata->dev, "Failed to send y-modem packet-0.\n");
		return err;
	}

	byte_remaining = fw_len;

	if (fw_len > YMODEM_PACKET_1K_SIZE)
		packet_size = YMODEM_PACKET_1K_SIZE;

	while (byte_remaining > 0) {
		if (byte_remaining >= packet_size)
			data_send = packet_size;
		else
			data_send = byte_remaining;

		err = st_hub_ymodem_send_packetn(hdata,
						&fw_data[byte_sent],
						data_send, packet_num,
						packet_size);
		if (err < 0) {
			dev_err(hdata->dev,
				"Failed to send y-modem packet %d.\n",
								packet_num);
			return err;
		}

		byte_sent += data_send;
		byte_remaining -= data_send;
		packet_num++;
	}

	err = st_hub_ymodem_send_eot(hdata);
	if (err < 0) {
		dev_err(hdata->dev, "Failed to send y-modem EOT.\n");
		return err;
	}

	err = st_hub_ymodem_send_last_packet(hdata);
	if (err < 0) {
		dev_err(hdata->dev, "Failed to send y-modem last packet.\n");
		return err;
	}

	return 0;
}

int st_sensor_hub_send_firmware_ymodem(struct st_hub_data *hdata,
						const struct firmware *fw)
{
	int err;
	u8 command;
	u32 ram2_addr = ST_HUB_YMODEM_RAM2_ADDR;

	if (fw->size > (ST_HUB_RAM1_SIZE + ST_HUB_RAM2_SIZE)) {
		dev_err(hdata->dev, "FW size too big. (MAX: %d byte)\n",
					ST_HUB_RAM1_SIZE + ST_HUB_RAM2_SIZE);
		return -ENOMEM;
	}

	command = (u8)ST_HUB_YMODEM_START_COMMAND;

	err = hdata->tf->write_rl(hdata->dev, 1, &command);
	if (err < 0) {
		dev_err(hdata->dev, "Failed to send y-modem start command.\n");
		return err;
	}

	err = st_hub_ymodem_transmit(hdata, (u8 *)fw->data,
		fw->size > ST_HUB_RAM1_SIZE ? ST_HUB_RAM1_SIZE : fw->size);
	if (err < 0)
		return err;

	msleep(ST_HUB_YMODEM_SLEEP_MS);

	if (fw->size > ST_HUB_RAM1_SIZE) {
		command = (u8)ST_HUB_YMODEM_SET_RAM_ADDR;

		err = hdata->tf->write_rl(hdata->dev, 1, &command);
		if (err < 0) {
			dev_err(hdata->dev,
				"Failed to send y-modem ram address command.\n");
			return err;
		}

		err = hdata->tf->write_rl(hdata->dev, 4, (u8 *)&ram2_addr);
		if (err < 0) {
			dev_err(hdata->dev, "Failed to send y-modem ram address.\n");
			return err;
		}

		command = (u8)ST_HUB_YMODEM_START_COMMAND;

		err = hdata->tf->write_rl(hdata->dev, 1, &command);
		if (err < 0) {
			dev_err(hdata->dev, "Failed to send y-modem start command.\n");
			return err;
		}

		err = st_hub_ymodem_transmit(hdata,
					(u8 *)&fw->data[ST_HUB_RAM1_SIZE],
					fw->size - ST_HUB_RAM1_SIZE);
		if (err < 0)
			return err;

		msleep(ST_HUB_YMODEM_SLEEP_MS);
	}

	command = (u8)ST_HUB_YMODEM_RUN_COMMAND;

	err = hdata->tf->write_rl(hdata->dev, 1, &command);
	if (err < 0) {
		dev_err(hdata->dev, "Failed to send y-modem run command.\n");
		return err;
	}

	return 0;
}
EXPORT_SYMBOL(st_sensor_hub_send_firmware_ymodem);

int st_sensor_hub_read_ramloader_version(struct st_hub_data *hdata)
{
	int err;
	u8 command;

	command = (u8)ST_HUB_YMODEM_FW_VERSION_COMMAND;

	err = hdata->tf->write_rl(hdata->dev, 1, &command);
	if (err < 0) {
		dev_err(hdata->dev,
				"Failed to send ram loader version command.\n");
		return err;
	}

	msleep(ST_HUB_YMODEM_SLEEP_MS);

	err = hdata->tf->read_rl(hdata->dev, 1, &hdata->ram_loader_version[0]);
	if (err < 0) {
		dev_err(hdata->dev,
				"Failed to read ram loader version LSB.\n");
		return err;
	}

	err = hdata->tf->read_rl(hdata->dev, 1, &hdata->ram_loader_version[1]);
	if (err < 0) {
		dev_err(hdata->dev,
				"Failed to read ram loader version MSB.\n");
		return err;
	}
    dev_info(hdata->dev, "ram_loader_version=%d,%d\n", hdata->ram_loader_version[1], hdata->ram_loader_version[0]);
	return 0;
}
EXPORT_SYMBOL(st_sensor_hub_read_ramloader_version);
