/*
 * STMicroelectronics st_sensor_hub ymodem protocol
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Author: MCD Application Team,
 * Denis Ciocca <denis.ciocca@st.com>
 *
 * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License").
 * http://www.st.com/software_license_agreement_liberty_v2
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include "st_hub_ymodem.h"

u8 packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD];

static u8 i2c_buffer_read(u8 *data, struct i2c_client *client)
{
	int err;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = client->flags | I2C_M_RD;
	msg.len = 1;
	msg.buf = data;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0)
		dev_err(&client->dev, "i2c_buffer_read error.\n");

	return 1;
}

static int i2c_byte_write(u8 *data, struct i2c_client *client)
{
	int err;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = 1;
	msg.buf = data;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0)
		dev_err(&client->dev, "i2c_byte_write error.\n");

	return err;
}

static int i2c_byte_multi_write(u8 *data, u8 len, struct i2c_client *client)
{
	int err;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = len;
	msg.buf = data;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0)
		dev_err(&client->dev, "i2c_byte_write error.\n");

	return err;
}

static void st_hub_delay(uint32_t n_count)
{
	msleep(ST_HUB_YMODEM_SLEEP_MS);
}

static u16 usart_receive_data(struct i2c_client *client)
{
	u8 c = 0;

	i2c_buffer_read(&c, client);

	return c;
}

static int serial_put_char(struct i2c_client *client, u8 c)
{
	return i2c_byte_write(&c, client);
}


static int32_t receive_byte(u8 *c, uint32_t timeout, struct i2c_client *client)
{

	if (i2c_buffer_read(c, client) == 1)
		return 0;

	/*
	 * return from this function is always 1
	 */
	return -EPERM;
}

static uint32_t send_byte(u8 c, struct i2c_client *client)
{
	i2c_byte_write(&c, client);
	return 0;
}

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

static u8 cal_checksum(const u8 *data, uint32_t size)
{
	uint32_t sum = 0;
	const u8 *data_end = data + size;

	while (data < data_end)
		sum += *data++;

	return sum & 0xffu;
}

static void ymodem_prepare_intial_packet(u8 *data,
					const char *filename, uint32_t *length)
{
	u16 i, j;
	u8 file_ptr[10];

	/* Make first three packet */
	data[0] = SOH;
	data[1] = 0x00;
	data[2] = 0xff;

	/* Filename packet has valid data */
	for (i = 0; (filename[i] != '\0') && (i < FILE_NAME_LENGTH); i++)
		data[i + PACKET_HEADER] = filename[i];

	data[i + PACKET_HEADER] = 0x00;

	sprintf(file_ptr, "%d", *length); /* anup */
	for (j = 0, i = i + PACKET_HEADER + 1; file_ptr[j] != '\0';)
		data[i++] = file_ptr[j++];

	for (j = i; j < PACKET_SIZE + PACKET_HEADER; j++)
		data[j] = 0;
}

static void ymodem_prepare_packet(u8 *source_buf, u8 *data,
						u8 pktno, uint32_t size_block)
{
	u16 i, size, packet_size;
	u8 *file_ptr;

	/* Make first three packet */
	packet_size = size_block >=
				PACKET_1K_SIZE ? PACKET_1K_SIZE : PACKET_SIZE;
	size = size_block < packet_size ? size_block : packet_size;

	if (packet_size == PACKET_1K_SIZE)
		data[0] = STX;
	else
		data[0] = SOH;

	data[1] = pktno;
	data[2] = (~pktno);
	file_ptr = source_buf;

	/* Filename packet has valid data */
	for (i = PACKET_HEADER; i < size + PACKET_HEADER; i++)
		data[i] = *file_ptr++;

	if (size  <= packet_size) {
		for (i = size + PACKET_HEADER;
					i < packet_size + PACKET_HEADER; i++)
			data[i] = 0x1A; /* EOF (0x1A) or 0x00 */
	}
}

static void ymodem_send_packet(u8 *data, u16 length, struct i2c_client *client)
{
	u16 i = 0;
	u8 len = 12;


	while (length > 0) {
		if (len < length) {
			/* i2c_byte_multi_write(u8* pBuffer,
			 * u8 deviceAddress, u8 WriteAddr, u8 len); */
			i2c_byte_multi_write((data + i), len, client);
			i += len;
			length -= len; /* data remaining */
		} else {
			i2c_byte_multi_write((data + i), length, client);

			length = 0;
		}
	}
}

static int ymodem_transmit(struct i2c_client *client, const char *filename,
						u8 *data, size_t filesize)
{
	u8 filenamenew[FILE_NAME_LENGTH];
	u8 *buf_ptr, temp_check_sum;
	u16 temp_crc, block_number;
	u8 receivedC[2], CRC16_F = 0, i;
	uint32_t err = 0, ack_received = 0, size = 0, pkt_size;

	for (i = 0; (filename[i] != '\0') && (i < (FILE_NAME_LENGTH - 1)); i++)
		filenamenew[i] = filename[i];

	filenamenew[i] = '\0';
	CRC16_F = 1;

	ymodem_prepare_intial_packet(&packet_data[0], filenamenew, &filesize);

	do {
		ymodem_send_packet(packet_data,
					PACKET_SIZE + PACKET_HEADER, client);

		/* Send CRC or Check Sum based on CRC16_F */
		if (CRC16_F) {
			temp_crc = cal_crc16(&packet_data[3], PACKET_SIZE);
			send_byte(temp_crc >> 8, client);
			send_byte(temp_crc & 0xFF, client);
		} else {
			temp_check_sum =
				cal_checksum(&packet_data[3], PACKET_SIZE);
			send_byte(temp_check_sum, client);
		}

		st_hub_delay(0x30000000);

		/* Wait for Ack and 'C' */
		if (receive_byte(&receivedC[0], 1000000, client) == 0) {
			if (receivedC[0] == ACK) {
				/* Packet transfered correctly */
				ack_received = 1;
			}
		} else
			err++;

	} while (!ack_received && (err < 10));
	dev_info(&client->dev, "ymodem_transmit: initial packet has been sent, err=%d\n", err);

	if (err >= 10)
		return -EIO;

	/* Here 1024 bytes is used to send the packets */
	/****************************************************************/
	buf_ptr = data;
	size = filesize;
	block_number = 0x01;

	while (size) {
		/* Prepare next packet */
		ymodem_prepare_packet(buf_ptr,
					&packet_data[0], block_number, size);
		ack_received = 0;
		receivedC[0] = 0;
		err = 0;

		do {
			/* Send next packet */
			if (size >= PACKET_1K_SIZE)
				pkt_size = PACKET_1K_SIZE;
			else
				pkt_size = PACKET_SIZE;

			ymodem_send_packet(packet_data,
					pkt_size + PACKET_HEADER, client);
			/* Send CRC or Check Sum based on CRC16_F */
			if (CRC16_F) {
				temp_crc = cal_crc16(&packet_data[3], pkt_size);
				send_byte(temp_crc >> 8, client);
				send_byte(temp_crc & 0xFF, client);
			} else {
				temp_check_sum =
					cal_checksum(&packet_data[3], pkt_size);
				send_byte(temp_check_sum, client);
			}

			st_hub_delay(0x1000000);

			/* Wait for Ack */
			if (receive_byte(&receivedC[0], 1000000, client) == 0) {
				if (receivedC[0] == ACK) {
					ack_received = 1;
					if (size > pkt_size) {
						buf_ptr += pkt_size;
						size -= pkt_size;
	/*if (block_number == (USER_FLASH_SIZE/1024)) {
		printk("error block_number == (USER_FLASH_SIZE/1024).\n");
		return 0xFF;
	} else {*/
							block_number++;
	/*}*/

					} else {
						buf_ptr += pkt_size;
						size = 0;
					}
				}
			} else {
				err++;
			}
		} while (!ack_received && (err < 10));




		if (err >=  10)
			return -EIO;
	}

	/* Send (EOT); */
	ack_received = 0;
	receivedC[0] = 0x00;
	receivedC[1] = 0x00;
	err = 0;

	do {
		send_byte(EOT, client);
		/* Send (EOT); */
		/* Wait for Ack */

		st_hub_delay(0x1000000);

		receivedC[0] = usart_receive_data(client);
		if (receivedC[0] == ACK)
			ack_received = 1;
		else
			err++;

	} while (!ack_received && (err < 10));



	if (err >= 10)
		return -EIO;

	/* Last packet preparation */
	ack_received = 0;
	receivedC[0] = 0x00;
	receivedC[1] = 0x00;
	err = 0;

	packet_data[0] = SOH;
	packet_data[1] = 0;
	packet_data[2] = 0xFF;

	for (i = PACKET_HEADER; i < (PACKET_SIZE + PACKET_HEADER); i++)
		packet_data[i] = 0x00;

	do {
		/* Send Packet */
		ymodem_send_packet(packet_data,
					PACKET_SIZE + PACKET_HEADER, client);

		/* Send CRC or Check Sum based on CRC16_F */
		temp_crc = cal_crc16(&packet_data[3], PACKET_SIZE);
		send_byte(temp_crc >> 8, client);
		send_byte(temp_crc & 0xFF, client);

		st_hub_delay(0x1000000);

		/* Wait for Ack and 'C' */
		if (receive_byte(&receivedC[1], 1000000, client) == 0) {
			if (receivedC[1] == ACK)
				ack_received = 1;

		} else {
			err++;
		}
	} while (!ack_received && (err < 10));

	dev_info(&client->dev, "ymodem_transmit: the last zere-packet sent, err=%d\n", err);

	/* Resend packet if NAK  for a count of 10  else end of commuincation */
	if (err >= 10)
		return -EIO;

	return 0;
}

int st_sensor_hub_send_firmware_ymodem(struct i2c_client *client,
							u8 *data, size_t size)
{
	int err;
/*
	if (size > USER_FLASH_SIZE)
		return -ENOMEM;
*/
	err = serial_put_char(client, (u8)'X');
	if (err < 0) {
		dev_err(&client->dev, "serial_put_char failed.\n");
		return err;
	}

	err = ymodem_transmit(client, (const char *)client->name, data, size);
	dev_info(&client->dev, "ymodem_transmit: err=%d\n", err);

	if (err < 0)
		return err;

	msleep(ST_HUB_YMODEM_SLEEP_MS);

	err = serial_put_char(client, (u8)'N');
	if (err < 0) {
		dev_err(&client->dev, "serial_put_char run command failed.\n");
		return err;
	}

	return err;
}
EXPORT_SYMBOL(st_sensor_hub_send_firmware_ymodem);

int st_sensor_hub_read_ramloader_version(struct i2c_client *client,
								u8 *fw_version)
{
	int err;
	u8 command;

	command = (u8)ST_HUB_YMODEM_FW_VERSION_COMMAND;

	err = i2c_byte_write(&command, client);
	if (err < 0) {
		dev_err(&client->dev,
				"Failed to send ram loader version command.\n");
		return err;
	}

	msleep(ST_HUB_YMODEM_SLEEP_MS);

	err = i2c_buffer_read(&fw_version[0], client);
	if (err < 0) {
		dev_err(&client->dev,
				"Failed to read ram loader version LSB.\n");
		return err;
	}

	err = i2c_buffer_read(&fw_version[1], client);
	if (err < 0) {
		dev_err(&client->dev,
				"Failed to read ram loader version MSB.\n");
		return err;
	}

	return 0;
}
EXPORT_SYMBOL(st_sensor_hub_read_ramloader_version);
