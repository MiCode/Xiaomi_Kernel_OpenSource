/*
* Copyright (C) 2012 Invensense, Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/string.h>

#include "inv_mpu_iio.h"

static int inv_verify_firmware(struct inv_mpu_state *st, u32 memaddr);

static int inv_write_to_sram(struct inv_mpu_state *st, u32 memaddr,
					u32 image_size, u8 *source)
{
	int bank, write_size;
	int result, size, i;
	u8 *data;
	u8 rb[MPU_MEM_BANK_SIZE];

	data = source;
	size = image_size;

	for (bank = 0; size > 0; bank++, size -= write_size) {
		if (size > MPU_MEM_BANK_SIZE)
			write_size = MPU_MEM_BANK_SIZE;
		else
			write_size = size;
		result = mpu_memory_write(st, memaddr, write_size, data);
		if (result) {
			dev_err(st->dev, "error writing firmware:%d\n", bank);
			return result;
		}
		msleep(1);
		result = mpu_memory_read(st, memaddr, write_size, rb);
		if (memcmp(rb, data, write_size)) {
			dev_info(st->dev, "sram error=%x\n", memaddr);
			for (i = 0; i < write_size; i++) {
				if (rb[i] != data[i]) {
					dev_info(st->dev, "rewrite %x:rb=%x, data=%x\n", i, rb[i], data[i]);
					mpu_memory_read(st, memaddr + i , 1, rb);
					dev_info(st->dev, "readback00=%x\n", rb[0]);
					mpu_memory_write(st, memaddr + i, 1, &data[i]);
					mpu_memory_read(st, memaddr + i , 1, rb);
					dev_info(st->dev, "readback11=%x\n", rb[0]);
				}
			}
		}

		memaddr += write_size;
		data += write_size;
	}

	return 0;
}

static int inv_erase_flash(struct inv_mpu_state *st, int page)
{
	u8 d[1];
	int result;
	int erase_time;

	inv_plat_single_read(st, REG_FLASH_CFG, d);
	d[0] |= BIT_FLASH_IFM_DIS;
	dev_info(st->dev, "erase=%x\n", d[0]);
	result = inv_plat_single_write(st, REG_FLASH_CFG, d[0]);
	if (result)
		return result;
	erase_time = 0;
	if (page > MAX_FLASH_PAGE_ADDRESS) {
		inv_plat_single_write(st, REG_FLASH_ERASE,
						BIT_FLASH_ERASE_MASS_EN);
		inv_plat_single_read(st, REG_FLASH_ERASE, d);
		dev_info(st->dev, "erase=%x\n", d[0]);
		while (d[0] & BIT_FLASH_ERASE_MASS_EN) {
			msleep(1);
			result = inv_plat_single_read(st, REG_FLASH_ERASE, d);
			if (result)
				return result;
			erase_time++;
		}
		dev_info(st->dev, "mass flash erase time=%d\n", erase_time);
	}

	return 0;
}
static void inv_int32_to_little8(u32 d, u8 *out)
{
	int i;

	for (i = 0; i < 4; i++)
		out[i] = ((d >> (i * 8) ) & 0xff);
}
static int inv_init_dma(struct inv_mpu_state *st, u8 dma_channel,
				u32 source_addr, u32 dest_addr, u32 num_bytes)
{
	u8 control_reg_dma_bytes[] = {
		GARNET_DMA_CONTROL_REGISTER_BYTE_0_WORD_SIZE_BITS,
		GARNET_DMA_CONTROL_REGISTER_BYTE_1_MAX_BURST_BITS,
		(GARNET_DMA_CONTROL_REGISTER_BYTE_2_CHG_BIT |
		GARNET_DMA_CONTROL_REGISTER_BYTE_2_STRT_BIT),
		(GARNET_DMA_CONTROL_REGISTER_BYTE_3_INT_BIT |
		GARNET_DMA_CONTROL_REGISTER_BYTE_3_TC_BIT |
		GARNET_DMA_CONTROL_REGISTER_BYTE_3_SINC_BIT |
		GARNET_DMA_CONTROL_REGISTER_BYTE_3_DINC_BIT)};
	u32 dma_addr = GARNET_DMA_CH_0_START_ADDR + dma_channel *
					GARNET_DMA_CHANNEL_ADDRESS_OFFSET;
	u8 dma_source_dest_addrs[8] = {0};
	u8 dma_length[4] = {0};
	u8 int_stat[2];
	u8 tmp[4];
	u32 result;

	/* Form DMA configuration message
	write source and dest addresses to dma registers */
	inv_int32_to_little8(source_addr, dma_source_dest_addrs);
	inv_int32_to_little8(dest_addr, &dma_source_dest_addrs[4]);

	dev_info(st->dev, "addr=%x, %x, %x, %x,$$ %x, %x, %x, %x\n",
		dma_source_dest_addrs[0],
		dma_source_dest_addrs[1],
		dma_source_dest_addrs[2],
		dma_source_dest_addrs[3],
		dma_source_dest_addrs[4],
		dma_source_dest_addrs[5],
		dma_source_dest_addrs[6],
		dma_source_dest_addrs[7]
		);

	/* Write Source and Destination Addresses to the
	dma controller registers.
	NOTE:  memory writes are always through Bank_0,
	so write_mem function handles that */
	inv_write_to_sram(st, dma_addr, 8, dma_source_dest_addrs);

	/* write the length (dmaAddr + 0x0C) */
	inv_int32_to_little8(num_bytes, dma_length);
	dev_info(st->dev, "len=%x, %x, %x, %x\n", dma_length[0],
					dma_length[1],
					dma_length[2],
					dma_length[3]);
	inv_write_to_sram(st, dma_addr + GARNET_DMA_TRANSFER_COUNT_OFFSET,
					4, dma_length);
	/* write the dma control register.  NOTE.  must write the 3rd byte
	(arm DMA) last
	the following writes could more efficiently be done individually...
	per Andy's code, just need to write MEM_ADDR_SEL_3 and then the bytes */
	result = inv_write_to_sram(st, dma_addr +
				GARNET_DMA_CONTROL_REGISTER_BYTE_0_OFFSET, 2,
				control_reg_dma_bytes);
	result = inv_write_to_sram(st, dma_addr +
				GARNET_DMA_CONTROL_REGISTER_BYTE_3_OFFSET, 1,
				&control_reg_dma_bytes[3]);
	mpu_memory_read(st, dma_addr, 4, tmp);
	dev_info(st->dev, "dma transfer0=%x, %x, %x, %x\n", tmp[0], tmp[1], tmp[2], tmp[3]);
	mpu_memory_read(st, dma_addr + 4, 4, tmp);
	dev_info(st->dev, "dma transfer1=%x, %x, %x, %x\n", tmp[0], tmp[1], tmp[2], tmp[3]);
	mpu_memory_read(st, dma_addr + 8, 4, tmp);
	dev_info(st->dev, "dma transfer2=%x, %x, %x, %x\n", tmp[0], tmp[1], tmp[2], tmp[3]);

	msleep(100);
	result = inv_write_to_sram(st, dma_addr +
				GARNET_DMA_CONTROL_REGISTER_BYTE_2_OFFSET,
				1, &control_reg_dma_bytes[2]);
	int_stat[0] = 0;
	while (!(int_stat[0] & (1 << dma_channel))) {
		result = mpu_memory_read(st, GARNET_DMA_INTERRUPT_REGISTER,
				1, int_stat);
		msleep(10);
		dev_info(st->dev, "int=%x\n", int_stat[0]);
	}
	do {
		inv_plat_single_read(st, REG_IDLE_STATUS, int_stat);
		dev_info(st->dev, "idle=%x\n", int_stat[0]);
		msleep(1);
	} while (!(int_stat[0] & (BIT_FLASH_IDLE | BIT_FLASH_LOAD_DONE)));

	return result;
}

static int inv_load_firmware(struct inv_mpu_state *st)
{
	int result;


	result = inv_write_to_sram(st, SRAM_START_ADDR, DMP_IMAGE_SIZE,
				st->firmware);
	dev_info(st->dev, "sss1122=%d\n", result);
	if (result)
		return result;
	result = inv_erase_flash(st, 65);
	dev_info(st->dev, "erase11=%d\n", result);
	if (result)
		return result;
	result = inv_soft_reset(st);
	if (result)
		return result;

	result = inv_init_dma(st, 1, SRAM_START_ADDR,
					FLASH_START_ADDR, DMP_IMAGE_SIZE);
	if (result)
		return result;

	return 0;
}

static int inv_write_dmp_start_address(struct inv_mpu_state *st)
{
	int result;
	u8 address[3] = {0, 0xFF, 0};

	inv_set_bank(st, 1);
	result = inv_plat_write(st, REG_PRGRM_STRT_ADDR_DRDY_0, 3, address);
	if (result)
		return result;
	address[2] = 0x08;
	inv_plat_write(st, REG_PRGRM_STRT_ADDR_TIMER_0, 3, address);
	address[2] = 0x10;
	inv_plat_write(st, REG_PRGRM_STRT_ADDR_DEMAND_0, 3, address);
	inv_set_bank(st, 0);
	dev_info(st->dev, "dmp start done\n");

	return 0;
}

static int inv_verify_firmware(struct inv_mpu_state *st, u32 memaddr)
{
	int bank, write_size, size;
	int result;
	u8 firmware[MPU_MEM_BANK_SIZE], d[2];
	u8 *data;

	inv_plat_single_read(st, REG_FLASH_CFG, d);
	d[0] |= BIT_FLASH_CACHE_BYPASS;
	dev_info(st->dev, "flashCFG11=%x\n", d[0]);
	inv_plat_single_write(st, REG_FLASH_CFG, d[0]);
	msleep(100);
	data = st->firmware;
	size = DMP_IMAGE_SIZE;
	dev_info(st->dev, "verify=%d, addr=%x\n", size, memaddr);
	for (bank = 0; size > 0; bank++, size -= write_size) {
		msleep(10);
		//dev_info(st->dev, "bank=%d\n", bank);
		if (size > MPU_MEM_BANK_SIZE)
			write_size = MPU_MEM_BANK_SIZE;
		else
			write_size = size;

		result = mpu_memory_read(st, memaddr, write_size, firmware);
		if (result)
			return result;
		if (0 != memcmp(firmware, data, write_size)) {
			dev_err(st->dev, "load data error, bank=%d\n", bank);
			result = mpu_memory_read(st, memaddr, write_size, firmware);
			dev_info(st->dev, "read, %x, %x, %x, %x\n", firmware[0], firmware[1], firmware[2], firmware[3]);
			dev_info(st->dev, "orig, %x, %x, %x, %x\n", data[0], data[1], data[2], data[3]);
			if (0 != memcmp(firmware, data, write_size)) {
				dev_err(st->dev, "readtwickerror\n");
			} else {
				dev_err(st->dev, "OK twice\n");
			}
		}
		memaddr += write_size;
		data += write_size;
	}
	inv_plat_single_read(st, REG_FLASH_CFG, d);
	d[0] &= (~BIT_FLASH_CACHE_BYPASS);
	dev_info(st->dev, "flashCFG22=%x\n", d[0]);
	inv_plat_single_write(st, REG_FLASH_CFG, d[0]);

	return 0;
}

/*
 * inv_firmware_load() -  calling this function will load the firmware.
 */
int inv_firmware_load(struct inv_mpu_state *st)
{
	int result;

	result = inv_load_firmware(st);
	if (result) {
		dev_err(st->dev, "load firmware:load firmware eror\n");
		goto firmware_write_fail;
	}

	result = inv_verify_firmware(st, FLASH_START_ADDR);
	if (result) {
		dev_err(st->dev, "load firmware:verify firmware error\n");
		goto firmware_write_fail;
	}
	inv_write_dmp_start_address(st);
	result = inv_fifo_config(st);
	if (result)
		return result;

firmware_write_fail:

	st->firmware_loaded = 1;

	return 0;
}

int inv_dmp_read(struct inv_mpu_state *st, int off, int size, u8 *buf)
{
	int read_size, data, result;
	u32 memaddr;

	data = 0;
	memaddr = FLASH_START_ADDR + off;
	while (size > 0) {
		if (size > MPU_MEM_BANK_SIZE)
			read_size = MPU_MEM_BANK_SIZE;
		else
			read_size = size;
		result = mpu_memory_read(st, memaddr, read_size, &buf[data]);
		if (result)
			return result;
		memaddr += read_size;
		data    += read_size;
		size    -= read_size;
	}

	return 0;
}
