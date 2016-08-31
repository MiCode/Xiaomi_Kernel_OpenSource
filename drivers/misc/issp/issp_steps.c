/*
 * Copyright (c) 2006-2013, Cypress Semiconductor Corporation
 * All rights reserved.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include "issp_priv.h"

#define ISSP_RESET_ASSERT_DELAY		(14270 + 10)
#define ISSP_RESET_PULSE_LENGTH		(300 + 10)
#define ISSP_RESET_POST_DELAY		(1)
#define ISSP_DATA_TRANS_TIMEOUT		(200000)

/* vectors */

static const int bits_id_setup_1 = 594;
static const uint8_t vec_id_setup_1[] = {
	0xCA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0D, 0xEE, 0x21, 0xF7, 0xF0, 0x27, 0xDC, 0x40,
	0x9F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xE7, 0xC1,
	0xD7, 0x9F, 0x20, 0x7E, 0x7D, 0x88, 0x7D, 0xEE,
	0x21, 0xF7, 0xF0, 0x07, 0xDC, 0x40, 0x1F, 0x70,
	0x01, 0xFD, 0xEE, 0x01, 0xF7, 0xA0, 0x1F, 0xDE,
	0xA0, 0x1F, 0x7B, 0x00, 0x7D, 0xE0, 0x13, 0xF7,
	0xC0, 0x07, 0xDF, 0x28, 0x1F, 0x7D, 0x18, 0x7D,
	0xFE, 0x25, 0x80
};

static const int bits_id_setup_2 = 418;
static const uint8_t vec_id_setup_2[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81,
	0xF9, 0xF4, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40,
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF7, 0xA0,
	0x1F, 0xDE, 0xA0, 0x1F, 0x7B, 0x00, 0x7D, 0xE0,
	0x0D, 0xF7, 0xC0, 0x07, 0xDF, 0x28, 0x1F, 0x7D,
	0x18, 0x7D, 0xFE, 0x25, 0x80
};

static const int bits_set_block_num = 11;
static const uint8_t vec_set_block_num[] = {
	0x9F, 0x40
};
static const int bits_set_block_num_end = 3;
static const uint8_t vec_set_block_num_end[] = {
	0xE0
};

static const int bits_checksum_setup = 418;
static const uint8_t vec_checksum_setup[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81,
	0xF9, 0xF4, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40,
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF7, 0xA0,
	0x1F, 0xDE, 0xA0, 0x1F, 0x7B, 0x00, 0x7D, 0xE0,
	0x0F, 0xF7, 0xC0, 0x07, 0xDF, 0x28, 0x1F, 0x7D,
	0x18, 0x7D, 0xFE, 0x25, 0x80
};

static const int bits_read_checksum_v[3] = {
	11, 12, 1
};
static const uint8_t vec_read_checksum_v[3][2] = {
	{0xBF, 0x20}, {0xDF, 0x80}, {0x80}
};

static const int bits_program_and_verify = 440;
static const uint8_t vec_program_and_verify[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81,
	0xF9, 0xF7, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40,
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF6, 0xA0,
	0x0F, 0xDE, 0x80, 0x7F, 0x7A, 0x80, 0x7D, 0xEC,
	0x01, 0xF7, 0x80, 0x57, 0xDF, 0x00, 0x1F, 0x7C,
	0xA0, 0x7D, 0xF4, 0x61, 0xF7, 0xF8, 0x96
};

static const int bits_erase = 396;
static const uint8_t vec_erase[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x85,
	0xFD, 0xFC, 0x01, 0xF7, 0x10, 0x07, 0xDC, 0x00,
	0x7F, 0x7B, 0x80, 0x7D, 0xE0, 0x0B, 0xF7, 0xA0,
	0x1F, 0xDE, 0xA0, 0x1F, 0x7B, 0x04, 0x7D, 0xF0,
	0x01, 0xF7, 0xC9, 0x87, 0xDF, 0x48, 0x1F, 0x7F,
	0x89, 0x60
};

static const int bits_secure = 440;
static const uint8_t vec_secure[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81,
	0xF9, 0xF7, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40,
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF6, 0xA0,
	0x0F, 0xDE, 0x80, 0x7F, 0x7A, 0x80, 0x7D, 0xEC,
	0x01, 0xF7, 0x80, 0x27, 0xDF, 0x00, 0x1F, 0x7C,
	0xA0, 0x7D, 0xF4, 0x61, 0xF7, 0xF8, 0x96
};

static const int bits_read_security_setup = 88;
static const uint8_t vec_read_security_setup[] = {
	0xDE, 0xE2, 0x1F, 0x60, 0x88, 0x7D, 0x84, 0x21,
	0xF7, 0xB8, 0x07
};

static const int bits_read_security_pt1 = 78;
static const uint8_t vec_read_security_pt1[] = {
	0xDE, 0xE2, 0x1F, 0x72, 0x87, 0x7D, 0xCA, 0x01,
	0xF7, 0x28
};

static const int bits_read_security_pt1_end = 25;
static const uint8_t vec_read_security_pt1_end[] = {
	0xFB, 0x94, 0x03, 0x80
};

static const int bits_read_security_pt2 = 198;
static const uint8_t vec_read_security_pt2[] = {
	0xDE, 0xE0, 0x1F, 0x7A, 0x01, 0xFD, 0xEA, 0x01,
	0xF7, 0xB0, 0x07, 0xDF, 0x0B, 0xBF, 0x7C, 0xF2,
	0xFD, 0xF4, 0x61, 0xF7, 0xB8, 0x87, 0xDF, 0xE2,
	0x58
};

static const int bits_read_security_pt3 = 122;
static const uint8_t vec_read_security_pt3[] = {
	0xDE, 0xE0, 0x1F, 0x7A, 0x01, 0xFD, 0xEA, 0x01,
	0xF7, 0xB0, 0x07, 0xDF, 0x0A, 0x7F, 0x7C, 0xC0
};

static const int bits_read_security_pt3_end = 47;
static const uint8_t vec_read_security_pt3_end[] = {
	0xFB, 0xE8, 0xC3, 0xEF, 0xF1, 0x2C
};

static const int bits_read_write_setup = 66;
static const uint8_t vec_read_write_setup[] = {
	0xDE, 0xF0, 0x1F, 0x78, 0x00, 0x7D, 0xA0, 0x03,
	0xC0
};

static const int bits_write_byte_start = 4;
static const uint8_t vec_write_byte_start[] = {
	0x90
};
static const int bits_write_byte_end = 3;
static const uint8_t vec_write_byte_end[] = {
	0xE0
};

static const int bits_read_id_v[5] = {
	11, 12, 12, 12, 1
};
static const uint8_t vec_read_id_v[5][2] = {
	{0xBF, 0x00}, {0xDF, 0x90}, {0xFF, 0x30}, {0xFF, 0x00}, {0x80}
};

static const int bits_read_status = 11;
static const uint8_t vec_read_status[] = {
	0xBF, 0x00
};
static const int bits_read_status_end = 1;
static const uint8_t vec_read_status_end[] = {
	0x80
};

static const int bits_verify_setup = 440;
static const uint8_t vec_verify_setup[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0x9F, 0x07, 0x5E, 0x7C, 0x81,
	0xF9, 0xF7, 0x01, 0xF7, 0xF0, 0x07, 0xDC, 0x40,
	0x1F, 0x70, 0x01, 0xFD, 0xEE, 0x01, 0xF6, 0xA8,
	0x0F, 0xDE, 0x80, 0x7F, 0x7A, 0x80, 0x7D, 0xEC,
	0x01, 0xF7, 0x80, 0x0F, 0xDF, 0x00, 0x1F, 0x7C,
	0xA0, 0x7D, 0xF4, 0x61, 0xF7, 0xF8, 0x96
};

static const int bits_read_byte_v[2] = {
	4, 1
};
static const uint8_t vec_read_byte_v[2][1] = {
	{0xB0}, {0x80}
};

static const int bits_sync_enable = 110;
static const uint8_t vec_sync_enable[] = {
	0xDE, 0xE2, 0x1F, 0x7F, 0x02, 0x7D, 0xC4, 0x09,
	0xF7, 0x00, 0x1F, 0xDE, 0xE0, 0x1C
};

static const int bits_sync_disable = 110;
static const uint8_t vec_sync_disable[] = {
	0xDE, 0xE2, 0x1F, 0x71, 0x00, 0x7D, 0xFC, 0x01,
	0xF7, 0x00, 0x1F, 0xDE, 0xE0, 0x1C
};

static const int bits_wait_and_poll = 30;
static const uint8_t vec_wait_and_poll[] = {
	0x00, 0x00, 0x00, 0x00
};
/* pin functions */

static inline void pin_reset_lo(struct issp_host *host)
{
	gpio_set_value(host->pdata->reset_gpio, 0);
}

static inline void pin_reset_hi(struct issp_host *host)
{
	gpio_set_value(host->pdata->reset_gpio, 1);
}

static inline void pin_data_lo(struct issp_host *host)
{
	gpio_set_value(host->pdata->data_gpio, 0);
}

static inline void pin_data_hi(struct issp_host *host)
{
	gpio_set_value(host->pdata->data_gpio, 1);
}

static inline void pin_data_in(struct issp_host *host)
{
	gpio_direction_input(host->pdata->data_gpio);
}

static inline void pin_data_out(struct issp_host *host)
{
	gpio_direction_output(host->pdata->data_gpio, 0);
}

static inline void pin_data_z(struct issp_host *host)
{
	pin_data_in(host);
}

static inline int pin_data(struct issp_host *host)
{
	return gpio_get_value(host->pdata->data_gpio);
}

static inline void pin_clk_lo(struct issp_host *host)
{
	gpio_set_value(host->pdata->clk_gpio, 0);
}

static inline void pin_clk_hi(struct issp_host *host)
{
	gpio_set_value(host->pdata->clk_gpio, 1);
}

/* simulate program steps */

static void send_bits(struct issp_host *host, const uint8_t *data, int bits)
{
	int bit_cnt = 0;
	uint8_t byte = 0;

	while (bit_cnt < bits) {
		if (!(bit_cnt & 0x7))
			byte = data[bit_cnt / 8];

		if (byte & 0x80)
			pin_data_hi(host);
		else
			pin_data_lo(host);

		pin_clk_hi(host);
		pin_clk_lo(host);
		byte <<= 1;
		bit_cnt++;
	}
}

static void receive_bits(struct issp_host *host, uint8_t *data, int bits)
{
	int bit_cnt = 0;

	while (bit_cnt < bits) {
		int index = bit_cnt / 8;

		if (!(bit_cnt & 0x7))
			data[index] = 0;
		else
			data[index] <<= 1;

		pin_clk_hi(host);
		if (pin_data(host))
			data[index] |= 0x1;
		pin_clk_lo(host);

		bit_cnt++;
	}
}

static uint8_t read_byte(struct issp_host *host)
{
	uint8_t byte;
	pin_data_in(host);
	receive_bits(host, &byte, 8);
	pin_data_z(host);
	return byte;
}

static void generate_clocks(struct issp_host *host, int num)
{
	pin_data_z(host);
	while (num--) {
		pin_clk_hi(host);
		pin_clk_lo(host);
	}
}

static void send_vector(struct issp_host *host, const uint8_t *pvec, int bits)
{
	pin_data_out(host);
	send_bits(host, pvec, bits);
	pin_data_z(host);
}

static int wait_and_poll(struct issp_host *host)
{
	ktime_t start;

	pin_data_in(host);

	/* wait for data pin go to high */
	start = ktime_get();
	while (1) {
		pin_clk_lo(host);
		if (pin_data(host))
			break;
		pin_clk_hi(host);

		if (ktime_us_delta(ktime_get(), start) >=
					ISSP_DATA_TRANS_TIMEOUT) {
			pin_data_z(host);
			pin_clk_lo(host);
			dev_err(&host->pdev->dev, "Poll high timeout!\n");
			return -ETIMEDOUT;
		}
	}

	/* wait for data pin go to low to finish runing vector */
	start = ktime_get();
	while (1) {
		if (!pin_data(host))
			break;

		if (ktime_us_delta(ktime_get(), start) >=
					ISSP_DATA_TRANS_TIMEOUT) {
			pin_data_z(host);
			dev_err(&host->pdev->dev, "Poll low timeout!\n");
			return -ETIMEDOUT;
		}
	}

	send_vector(host, vec_wait_and_poll, bits_wait_and_poll);

	return 0;
}

static int issp_get_id(struct issp_host *host)
{
	int i, ret;

	send_vector(host, vec_id_setup_1, bits_id_setup_1);
	ret = wait_and_poll(host);
	if (ret)
		return ret;
	send_vector(host, vec_id_setup_2, bits_id_setup_2);
	ret = wait_and_poll(host);
	if (ret)
		return ret;
	send_vector(host, vec_sync_enable, bits_sync_enable);

	for (i = 0; i < 4; i++) {
		send_vector(host, vec_read_id_v[i], bits_read_id_v[i]);
		generate_clocks(host, 2);
		host->si_id[i] = read_byte(host);
	}
	send_vector(host, vec_read_id_v[4], bits_read_id_v[4]);

	send_vector(host, vec_sync_disable, bits_sync_disable);

	return 0;
}

static int issp_erase(struct issp_host *host)
{
	send_vector(host, vec_erase, bits_erase);
	return wait_and_poll(host);
}

static void issp_write_byte(struct issp_host *host, uint8_t addr, uint8_t data)
{
	uint8_t address = addr << 1;

	send_bits(host, vec_write_byte_start, bits_write_byte_start);
	send_bits(host, &address, 7);
	send_bits(host, &data, 8);
	send_bits(host, vec_write_byte_end, bits_write_byte_end);
}

static void issp_send_block(struct issp_host *host)
{
	uint8_t addr = 0;

	send_vector(host, vec_sync_enable, bits_sync_enable);
	send_vector(host, vec_read_write_setup, bits_read_write_setup);

	pin_data_out(host);
	while (addr < host->pdata->block_size)
		issp_write_byte(host, addr++, issp_fw_get_byte(host));
	pin_data_z(host);
}

static int issp_prog_verify_block(struct issp_host *host, uint8_t idx)
{
	send_vector(host, vec_sync_enable, bits_sync_enable);
	send_vector(host, vec_set_block_num, bits_set_block_num);
	pin_data_out(host);
	send_bits(host, &idx, 8);
	send_vector(host, vec_set_block_num_end, bits_set_block_num_end);
	send_vector(host, vec_sync_disable, bits_sync_disable);
	send_vector(host, vec_program_and_verify, bits_program_and_verify);

	return wait_and_poll(host);
}

static int issp_check_status(struct issp_host *host)
{
	uint8_t status;
	int ret;

	send_vector(host, vec_sync_enable, bits_sync_enable);
	send_vector(host, vec_read_status, bits_read_status);
	generate_clocks(host, 2);
	status = read_byte(host);
	send_vector(host, vec_read_status_end, bits_read_status_end);
	send_vector(host, vec_sync_disable, bits_sync_disable);

	switch (status) {
	case 0x00:
		ret = 0;
		break;
	case 0x01:
		dev_err(&host->pdev->dev, "ReadStatus: " \
			"Not allowed because of block level protection\n");
		ret = -EACCES;
		break;
	case 0x03:
		dev_err(&host->pdev->dev, "ReadStatus: Fatal error\n");
		ret = -EIO;
		break;
	case 0x04:
		dev_err(&host->pdev->dev, "ReadStatus: Checksum failure\n");
		ret = -EIO;
		break;
	case 0x06:
		dev_err(&host->pdev->dev, "ReadStatus: Caliberate failure\n");
		ret = -EIO;
		break;
	default:
		dev_err(&host->pdev->dev,
			"ReadStatus: Failed 0x%02x\n", status);
		ret = -EIO;
		break;
	}

	return ret;
}

static int issp_program_block(struct issp_host *host, uint8_t idx)
{
	int ret;

	issp_send_block(host);

	ret = issp_prog_verify_block(host, idx);
	if (ret)
		return ret;

	return issp_check_status(host);
}

static int issp_block_verify_setup(struct issp_host *host, uint8_t idx)
{
	send_vector(host, vec_read_write_setup, bits_read_write_setup);
	send_vector(host, vec_sync_enable, bits_sync_enable);
	send_vector(host, vec_set_block_num, bits_set_block_num);
	pin_data_out(host);
	send_bits(host, &idx, 8);
	send_vector(host, vec_set_block_num_end, bits_set_block_num_end);
	send_vector(host, vec_sync_disable, bits_sync_disable);
	send_vector(host, vec_verify_setup, bits_verify_setup);

	return wait_and_poll(host);
}

static uint8_t issp_read_byte_at(struct issp_host *host, uint8_t addr)
{
	uint8_t address = addr << 1, data;

	send_vector(host, vec_read_byte_v[0], bits_read_byte_v[0]);
	pin_data_out(host);
	send_bits(host, &address, 7);
	generate_clocks(host, 2);
	data = read_byte(host);
	send_vector(host, vec_read_byte_v[1], bits_read_byte_v[1]);

	return data;
}

static int issp_block_read_compare(struct issp_host *host)
{
	uint8_t addr = 0;
	int ret = 0;

	send_vector(host, vec_sync_enable, bits_sync_enable);
	send_vector(host, vec_read_write_setup, bits_read_write_setup);

	while (addr < host->pdata->block_size) {
		uint8_t data = issp_read_byte_at(host, addr++);
		if (data != issp_fw_get_byte(host)) {
			dev_err(&host->pdev->dev, "Data compare failed!\n");
			ret =  -EIO;
			break;
		}
	}

	send_vector(host, vec_sync_disable, bits_sync_disable);

	return ret;
}

static int issp_verify_block(struct issp_host *host, uint8_t idx)
{
	int ret;

	ret = issp_block_verify_setup(host, idx);
	if (ret)
		return ret;

	ret = issp_check_status(host);
	if (ret)
		return ret;

	return issp_block_read_compare(host);
}

static int issp_set_security(struct issp_host *host)
{
	uint8_t addr = 0;

	send_vector(host, vec_sync_enable, bits_sync_enable);
	send_vector(host, vec_read_write_setup, bits_read_write_setup);

	pin_data_out(host);
	while (addr < host->pdata->security_size)
		issp_write_byte(host, addr++, issp_fw_get_byte(host));
	pin_data_z(host);

	send_vector(host, vec_secure, bits_secure);

	return wait_and_poll(host);
}

static int issp_verify_security(struct issp_host *host)
{
	uint8_t addr = 0;
	int ret = 0;

	send_vector(host, vec_read_security_setup, bits_read_security_setup);
	while (addr < host->pdata->security_size) {
		uint8_t address = addr << 1;
		addr++;

		send_vector(host, vec_sync_enable, bits_sync_enable);

		send_vector(host, vec_read_security_pt1,
				bits_read_security_pt1);
		pin_data_out(host);
		send_bits(host, &address, 7);
		send_vector(host, vec_read_security_pt1_end,
				bits_read_security_pt1_end);

		send_vector(host, vec_sync_disable, bits_sync_disable);

		send_vector(host, vec_read_security_pt2,
				bits_read_security_pt2);
		send_vector(host, vec_wait_and_poll, bits_wait_and_poll);

		send_vector(host, vec_read_security_pt3,
				bits_read_security_pt3);
		pin_data_out(host);
		send_bits(host, &address, 7);
		send_vector(host, vec_read_security_pt3_end,
				bits_read_security_pt3_end);
		send_vector(host, vec_wait_and_poll, bits_wait_and_poll);
	}

	addr = 0;
	send_vector(host, vec_sync_enable, bits_sync_enable);
	while (addr < host->pdata->security_size) {
		uint8_t data = issp_read_byte_at(host, addr++);
		if (data != issp_fw_get_byte(host)) {
			dev_err(&host->pdev->dev, "Data compare failed!\n");
			ret =  -EIO;
			break;
		}
	}

	send_vector(host, vec_sync_disable, bits_sync_disable);

	return ret;
}

static int issp_verify_checksum(struct issp_host *host)
{
	uint16_t uc_checksum;
	int ret;

	ret = issp_get_checksum(host, &uc_checksum);
	if (ret)
		return ret;

	if (uc_checksum == host->checksum_fw)
		return 0;
	else
		return -EIO;
}

/* global functions */

int issp_uc_program(struct issp_host *host)
{
	pin_data_z(host);
	pin_clk_lo(host);
	pin_reset_hi(host);

	/* reset */
	usleep_range(ISSP_RESET_ASSERT_DELAY, ISSP_RESET_ASSERT_DELAY);
	pin_reset_lo(host);
	udelay(ISSP_RESET_PULSE_LENGTH);
	pin_reset_hi(host);
	udelay(ISSP_RESET_POST_DELAY);

	return issp_get_id(host);
}

int issp_uc_run(struct issp_host *host)
{
	pin_reset_lo(host);
	udelay(ISSP_RESET_PULSE_LENGTH);
	pin_reset_hi(host);

	return 0;
}

int issp_get_checksum(struct issp_host *host, uint16_t *checksum)
{
	int ret;

	send_vector(host, vec_checksum_setup, bits_checksum_setup);
	ret = wait_and_poll(host);
	if (ret)
		return ret;
	send_vector(host, vec_sync_enable, bits_sync_enable);

	send_vector(host, vec_read_checksum_v[0], bits_read_checksum_v[0]);
	generate_clocks(host, 2);
	*checksum = read_byte(host);
	*checksum <<= 8;
	send_vector(host, vec_read_checksum_v[1], bits_read_checksum_v[1]);
	generate_clocks(host, 2);
	*checksum |= read_byte(host);
	send_vector(host, vec_read_checksum_v[2], bits_read_checksum_v[2]);

	send_vector(host, vec_sync_disable, bits_sync_disable);

	return 0;
}

int issp_program(struct issp_host *host)
{
	int i, ret;

	ret = issp_erase(host);
	if (ret) {
		dev_err(&host->pdev->dev, "Erase failed!\n");
		return ret;
	}

	issp_fw_rewind(host);
	for (i = 0; i < host->pdata->blocks; i++) {
		ret = issp_program_block(host, i);
		if (ret) {
			dev_err(&host->pdev->dev,
				"Program block %d failed!\n", i);
			return ret;
		}
	}

	issp_fw_rewind(host);
	for (i = 0; i < host->pdata->blocks; i++) {
		ret = issp_verify_block(host, i);
		if (ret) {
			dev_err(&host->pdev->dev,
				"Verify block %d failed!\n", i);
			return ret;
		}
	}

	issp_fw_seek_security(host);
	ret = issp_set_security(host);
	if (ret) {
		dev_err(&host->pdev->dev, "Set security failed!\n");
		return ret;
	}

	issp_fw_seek_security(host);
	ret = issp_verify_security(host);
	if (ret) {
		dev_err(&host->pdev->dev, "Verify security failed!\n");
		return ret;
	}

	ret = issp_verify_checksum(host);
	if (ret) {
		dev_err(&host->pdev->dev, "Verify checksum failed!\n");
		return ret;
	}

	return 0;
}

int issp_read_block(struct issp_host *host, uint8_t block_idx, uint8_t addr,
			uint8_t *buf, int len)
{
	int blk_size, i;
	int ret;

	blk_size = host->pdata->block_size;
	len = len + addr > blk_size ? blk_size - addr : len;
	if (!len)
		return 0;

	ret = issp_block_verify_setup(host, block_idx);
	if (ret)
		return ret;

	ret = issp_check_status(host);
	if (ret)
		return ret;

	send_vector(host, vec_sync_enable, bits_sync_enable);
	send_vector(host, vec_read_write_setup, bits_read_write_setup);

	for (i = 0; i < len; i++)
		buf[i] = issp_read_byte_at(host, addr++);

	send_vector(host, vec_sync_disable, bits_sync_disable);

	return len;
}
