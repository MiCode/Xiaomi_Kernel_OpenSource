/*
 * Copyright (C) 2016 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_data/nanohub.h>
#include <linux/vmalloc.h>

#include "main.h"
#include "bl.h"

#define MAX_BUFFER_SIZE		1024
#define MAX_FLASH_BANKS		16
#define READ_ACK_TIMEOUT	100000

static u8 write_len(struct nanohub_data *data, int len)
{
	u8 buffer[sizeof(u8) + 1];

	buffer[0] = len - 1;

	return data->bl.write_data(data, buffer, sizeof(u8));
}

static u8 write_cnt(struct nanohub_data *data, uint16_t cnt)
{
	u8 buffer[sizeof(uint16_t) + 1];

	buffer[0] = (cnt >> 8) & 0xFF;
	buffer[1] = (cnt >> 0) & 0xFF;

	return data->bl.write_data(data, buffer, sizeof(uint16_t));
}

static u8 write_addr(struct nanohub_data *data, u32 addr)
{
	u8 buffer[sizeof(u32) + 1];

	buffer[0] = (addr >> 24) & 0xFF;
	buffer[1] = (addr >> 16) & 0xFF;
	buffer[2] = (addr >> 8) & 0xFF;
	buffer[3] = addr & 0xFF;

	return data->bl.write_data(data, buffer, sizeof(u32));
}

/* write length followed by the data */
static u8 write_len_data(struct nanohub_data *data, int len,
			 const u8 *buf)
{
	u8 buffer[sizeof(u8) + 256 + sizeof(u8)];

	buffer[0] = len - 1;

	memcpy(&buffer[1], buf, len);

	return data->bl.write_data(data, buffer, sizeof(u8) + len);
}

/* keep checking for ack until we receive a ack or nack */
static u8 read_ack_loop(struct nanohub_data *data)
{
	u8 ret;
	s32 timeout = READ_ACK_TIMEOUT;

	do {
		ret = data->bl.read_ack(data);
		if (ret != CMD_ACK && ret != CMD_NACK)
			schedule();
	} while (ret != CMD_ACK && ret != CMD_NACK && timeout-- > 0);

	return ret;
}

u8 nanohub_bl_sync(struct nanohub_data *data)
{
	return data->bl.sync(data);
}

int nanohub_bl_open(struct nanohub_data *data)
{
	int ret = -1;

	data->bl.tx_buffer = kmalloc(MAX_BUFFER_SIZE, GFP_KERNEL | GFP_DMA);
	if (!data->bl.tx_buffer)
		goto out;

	data->bl.rx_buffer = kmalloc(MAX_BUFFER_SIZE, GFP_KERNEL | GFP_DMA);
	if (!data->bl.rx_buffer)
		goto free_tx;

	ret = data->bl.open(data);
	if (!ret)
		goto out;

	kfree(data->bl.rx_buffer);
free_tx:
	kfree(data->bl.tx_buffer);
out:
	return ret;
}

void nanohub_bl_close(struct nanohub_data *data)
{
	data->bl.close(data);
	kfree(data->bl.tx_buffer);
	kfree(data->bl.rx_buffer);
}

static u8 write_bank(struct nanohub_data *data, int bank, u32 addr,
		     const u8 *buf, size_t length)
{
	const struct nanohub_platform_data *pdata = data->pdata;
	u8 status = CMD_ACK;
	u32 offset;

	if (addr <= pdata->flash_banks[bank].address) {
		offset = pdata->flash_banks[bank].address - addr;
		if (addr + length >
		    pdata->flash_banks[bank].address +
		    pdata->flash_banks[bank].length)
			status =
			    nanohub_bl_write_memory(data,
						    pdata->flash_banks[bank].
						    address,
						    pdata->flash_banks[bank].
						    length, buf + offset);
		else
			status =
			    nanohub_bl_write_memory(data,
						    pdata->flash_banks[bank].
						    address, length - offset,
						    buf + offset);
	} else {
		if (addr + length >
		    pdata->flash_banks[bank].address +
		    pdata->flash_banks[bank].length)
			status =
			    nanohub_bl_write_memory(data, addr,
						    pdata->flash_banks[bank].
						    address +
						    pdata->flash_banks[bank].
						    length - addr, buf);
		else
			status =
			    nanohub_bl_write_memory(data, addr, length, buf);
	}

	return status;
}

u8 nanohub_bl_download(struct nanohub_data *data, u32 addr,
		       const u8 *image, size_t length)
{
	const struct nanohub_platform_data *pdata = data->pdata;
	u8 *ptr;
	int i, j;
	u8 status;
	u32 offset;
	u8 erase_mask[MAX_FLASH_BANKS] = { 0 };
	u8 erase_write_mask[MAX_FLASH_BANKS] = { 0 };
	u8 write_mask[MAX_FLASH_BANKS] = { 0 };

	if (pdata->num_flash_banks > MAX_FLASH_BANKS) {
		status = CMD_NACK;
		goto out;
	}

	status = nanohub_bl_sync(data);

	if (status != CMD_ACK) {
		pr_err("nanohub_bl_download: sync=%02x\n", status);
		goto out;
	}

	ptr = vmalloc(length);
	if (!ptr) {
		status = CMD_NACK;
		goto out;
	}

	status = nanohub_bl_read_memory(data, addr, length, ptr);
	pr_info(
	    "nanohub: nanohub_bl_read_memory: status=%02x, addr=%08x, length=%zd\n",
	    status, addr, length);

	for (i = 0; i < pdata->num_flash_banks; i++) {
		if (addr >= pdata->flash_banks[i].address &&
		    addr <
		    pdata->flash_banks[i].address +
		    pdata->flash_banks[i].length) {
			break;
		}
	}

	offset = (u32)(addr - pdata->flash_banks[i].address);
	j = 0;
	while (j < length && i < pdata->num_flash_banks) {
		if (image[j] != 0xFF)
			erase_write_mask[i] = true;

		if ((ptr[j] & image[j]) != image[j]) {
			erase_mask[i] = true;
			if (erase_write_mask[i]) {
				j += pdata->flash_banks[i].length - offset;
				offset = pdata->flash_banks[i].length;
			} else {
				j++;
				offset++;
			}
		} else {
			if (ptr[j] != image[j])
				write_mask[i] = true;
			j++;
			offset++;
		}

		if (offset == pdata->flash_banks[i].length) {
			i++;
			offset = 0;
			if (i < pdata->num_flash_banks)
				j += (pdata->flash_banks[i].address -
				      pdata->flash_banks[i - 1].address -
				      pdata->flash_banks[i - 1].length);
			else
				j = length;
		}
	}

	for (i = 0; status == CMD_ACK && i < pdata->num_flash_banks; i++) {
		pr_info("nanohub: i=%d, erase=%d, erase_write=%d, write=%d\n",
			i, erase_mask[i], erase_write_mask[i], write_mask[i]);
		if (erase_mask[i]) {
			status =
			    nanohub_bl_erase_sector(data,
						    pdata->flash_banks[i].bank);
			if (status == CMD_ACK && erase_write_mask[i])
				status =
				    write_bank(data, i, addr, image, length);
		} else if (write_mask[i]) {
			status = write_bank(data, i, addr, image, length);
		}
	}

	vfree(ptr);
out:
	return status;
}

u8 nanohub_bl_erase_shared(struct nanohub_data *data)
{
	const struct nanohub_platform_data *pdata = data->pdata;
	int i;
	u8 status;

	if (pdata->num_shared_flash_banks > MAX_FLASH_BANKS) {
		status = CMD_NACK;
		goto out;
	}

	status = nanohub_bl_sync(data);

	if (status != CMD_ACK) {
		pr_err("nanohub_bl_erase_shared: sync=%02x\n", status);
		goto out;
	}

	for (i = 0;
	     status == CMD_ACK && i < pdata->num_shared_flash_banks;
	     i++) {
		status = nanohub_bl_erase_sector(data,
			pdata->shared_flash_banks[i].bank);
	}
out:
	return status;
}

/* erase a single sector */
u8 nanohub_bl_erase_sector(struct nanohub_data *data, uint16_t sector)
{
	u8 ret;

	data->bl.write_cmd(data, data->bl.cmd_erase);
	ret = data->bl.read_ack(data);
	if (ret == CMD_ACK)
		ret = write_cnt(data, 0x0000);
	if (ret != CMD_NACK)
		ret = read_ack_loop(data);
	if (ret == CMD_ACK)
		ret = write_cnt(data, sector);
	if (ret != CMD_NACK)
		ret = read_ack_loop(data);

	return ret;
}

/* read memory - this will chop the request into 256 byte reads */
u8 nanohub_bl_read_memory(struct nanohub_data *data, u32 addr,
			  u32 length, u8 *buffer)
{
	u8 ret = CMD_ACK;
	u32 offset = 0;

	while (ret == CMD_ACK && length > offset) {
		data->bl.write_cmd(data, data->bl.cmd_read_memory);
		ret = data->bl.read_ack(data);
		if (ret == CMD_ACK) {
			write_addr(data, addr + offset);
			ret = read_ack_loop(data);
			if (ret == CMD_ACK) {
				if (length - offset >= 256) {
					write_len(data, 256);
					ret = read_ack_loop(data);
					if (ret == CMD_ACK) {
						data->bl.read_data(data,
								   &buffer
								   [offset],
								   256);
						offset += 256;
					}
				} else {
					write_len(data, length - offset);
					ret = read_ack_loop(data);
					if (ret == CMD_ACK) {
						data->bl.read_data(data,
								   &buffer
								   [offset],
								   length -
								   offset);
						offset = length;
					}
				}
			}
		}
	}

	return ret;
}

/* write memory - this will chop the request into 256 byte writes */
u8 nanohub_bl_write_memory(struct nanohub_data *data, u32 addr,
			   u32 length, const u8 *buffer)
{
	u8 ret = CMD_ACK;
	u32 offset = 0;

	while (ret == CMD_ACK && length > offset) {
		data->bl.write_cmd(data, data->bl.cmd_write_memory);
		ret = data->bl.read_ack(data);
		if (ret == CMD_ACK) {
			write_addr(data, addr + offset);
			ret = read_ack_loop(data);
			if (ret == CMD_ACK) {
				if (length - offset >= 256) {
					write_len_data(data, 256,
						       &buffer[offset]);
					offset += 256;
				} else {
					write_len_data(data, length - offset,
						       &buffer[offset]);
					offset = length;
				}
				ret = read_ack_loop(data);
			}
		}
	}

	return ret;
}
