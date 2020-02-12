/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 48764 $
 * $Date: 2019-08-08 14:52:12 +0800 (Thu, 08 Aug 2019) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

struct nvt_ts_mem_map {
	uint32_t EVENT_BUF_ADDR;
	uint32_t RAW_PIPE0_ADDR;
	uint32_t RAW_PIPE1_ADDR;
	uint32_t BASELINE_ADDR;
	uint32_t BASELINE_BTN_ADDR;
	uint32_t DIFF_PIPE0_ADDR;
	uint32_t DIFF_PIPE1_ADDR;
	uint32_t RAW_BTN_PIPE0_ADDR;
	uint32_t RAW_BTN_PIPE1_ADDR;
	uint32_t DIFF_BTN_PIPE0_ADDR;
	uint32_t DIFF_BTN_PIPE1_ADDR;
	uint32_t READ_FLASH_CHECKSUM_ADDR;
	uint32_t RW_FLASH_DATA_ADDR;
};

struct nvt_ts_hw_info {
	uint8_t carrier_system;
	uint8_t hw_crc;
};

static const struct nvt_ts_mem_map NT36526_memory_map = {
	.EVENT_BUF_ADDR           = 0x22D00,
	.RAW_PIPE0_ADDR           = 0x24000,
	.RAW_PIPE1_ADDR           = 0x24000,
	.BASELINE_ADDR            = 0x21758,
	.BASELINE_BTN_ADDR        = 0,
	.DIFF_PIPE0_ADDR          = 0x20AB0,
	.DIFF_PIPE1_ADDR          = 0x24AB0,
	.RAW_BTN_PIPE0_ADDR       = 0,
	.RAW_BTN_PIPE1_ADDR       = 0,
	.DIFF_BTN_PIPE0_ADDR      = 0,
	.DIFF_BTN_PIPE1_ADDR      = 0,
	.READ_FLASH_CHECKSUM_ADDR = 0x24000,
	.RW_FLASH_DATA_ADDR       = 0x24002,
};

static const struct nvt_ts_mem_map NT36675_memory_map = {
	.EVENT_BUF_ADDR           = 0x22D00,
	.RAW_PIPE0_ADDR           = 0x24000,
	.RAW_PIPE1_ADDR           = 0x24000,
	.BASELINE_ADDR            = 0x21B90,
	.BASELINE_BTN_ADDR        = 0,
	.DIFF_PIPE0_ADDR          = 0x20C60,
	.DIFF_PIPE1_ADDR          = 0x24C60,
	.RAW_BTN_PIPE0_ADDR       = 0,
	.RAW_BTN_PIPE1_ADDR       = 0,
	.DIFF_BTN_PIPE0_ADDR      = 0,
	.DIFF_BTN_PIPE1_ADDR      = 0,
	.READ_FLASH_CHECKSUM_ADDR = 0x24000,
	.RW_FLASH_DATA_ADDR       = 0x24002,
};

static const struct nvt_ts_mem_map NT36672A_memory_map = {
	.EVENT_BUF_ADDR           = 0x21C00,
	.RAW_PIPE0_ADDR           = 0x20000,
	.RAW_PIPE1_ADDR           = 0x23000,
	.BASELINE_ADDR            = 0x20BFC,
	.BASELINE_BTN_ADDR        = 0x23BFC,
	.DIFF_PIPE0_ADDR          = 0x206DC,
	.DIFF_PIPE1_ADDR          = 0x236DC,
	.RAW_BTN_PIPE0_ADDR       = 0x20510,
	.RAW_BTN_PIPE1_ADDR       = 0x23510,
	.DIFF_BTN_PIPE0_ADDR      = 0x20BF0,
	.DIFF_BTN_PIPE1_ADDR      = 0x23BF0,
	.READ_FLASH_CHECKSUM_ADDR = 0x24000,
	.RW_FLASH_DATA_ADDR       = 0x24002,
};

static const struct nvt_ts_mem_map NT36772_memory_map = {
	.EVENT_BUF_ADDR           = 0x11E00,
	.RAW_PIPE0_ADDR           = 0x10000,
	.RAW_PIPE1_ADDR           = 0x12000,
	.BASELINE_ADDR            = 0x10E70,
	.BASELINE_BTN_ADDR        = 0x12E70,
	.DIFF_PIPE0_ADDR          = 0x10830,
	.DIFF_PIPE1_ADDR          = 0x12830,
	.RAW_BTN_PIPE0_ADDR       = 0x10E60,
	.RAW_BTN_PIPE1_ADDR       = 0x12E60,
	.DIFF_BTN_PIPE0_ADDR      = 0x10E68,
	.DIFF_BTN_PIPE1_ADDR      = 0x12E68,
	.READ_FLASH_CHECKSUM_ADDR = 0x14000,
	.RW_FLASH_DATA_ADDR       = 0x14002,
};

static const struct nvt_ts_mem_map NT36525_memory_map = {
	.EVENT_BUF_ADDR           = 0x11A00,
	.RAW_PIPE0_ADDR           = 0x10000,
	.RAW_PIPE1_ADDR           = 0x12000,
	.BASELINE_ADDR            = 0x10B08,
	.BASELINE_BTN_ADDR        = 0x12B08,
	.DIFF_PIPE0_ADDR          = 0x1064C,
	.DIFF_PIPE1_ADDR          = 0x1264C,
	.RAW_BTN_PIPE0_ADDR       = 0x10634,
	.RAW_BTN_PIPE1_ADDR       = 0x12634,
	.DIFF_BTN_PIPE0_ADDR      = 0x10AFC,
	.DIFF_BTN_PIPE1_ADDR      = 0x12AFC,
	.READ_FLASH_CHECKSUM_ADDR = 0x14000,
	.RW_FLASH_DATA_ADDR       = 0x14002,
};

static const struct nvt_ts_mem_map NT36676F_memory_map = {
	.EVENT_BUF_ADDR           = 0x11A00,
	.RAW_PIPE0_ADDR           = 0x10000,
	.RAW_PIPE1_ADDR           = 0x12000,
	.BASELINE_ADDR            = 0x10B08,
	.BASELINE_BTN_ADDR        = 0x12B08,
	.DIFF_PIPE0_ADDR          = 0x1064C,
	.DIFF_PIPE1_ADDR          = 0x1264C,
	.RAW_BTN_PIPE0_ADDR       = 0x10634,
	.RAW_BTN_PIPE1_ADDR       = 0x12634,
	.DIFF_BTN_PIPE0_ADDR      = 0x10AFC,
	.DIFF_BTN_PIPE1_ADDR      = 0x12AFC,
	.READ_FLASH_CHECKSUM_ADDR = 0x14000,
	.RW_FLASH_DATA_ADDR       = 0x14002,
};

static struct nvt_ts_hw_info NT36526_hw_info = {
	.carrier_system = 2,
	.hw_crc         = 2,
};

static struct nvt_ts_hw_info NT36675_hw_info = {
	.carrier_system = 2,
	.hw_crc         = 2,
};

static struct nvt_ts_hw_info NT36672A_hw_info = {
	.carrier_system = 0,
	.hw_crc         = 1,
};

static struct nvt_ts_hw_info NT36772_hw_info = {
	.carrier_system = 0,
	.hw_crc         = 0,
};

static struct nvt_ts_hw_info NT36525_hw_info = {
	.carrier_system = 0,
	.hw_crc         = 0,
};

static struct nvt_ts_hw_info NT36676F_hw_info = {
	.carrier_system = 0,
	.hw_crc         = 0,
};

#define NVT_ID_BYTE_MAX 6
struct nvt_ts_trim_id_table {
	uint8_t id[NVT_ID_BYTE_MAX];
	uint8_t mask[NVT_ID_BYTE_MAX];
	const struct nvt_ts_mem_map *mmap;
	const struct nvt_ts_hw_info *hwinfo;
};

static const struct nvt_ts_trim_id_table trim_id_table[] = {
	{.id = {0x0C, 0xFF, 0xFF, 0x72, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36675_memory_map,  .hwinfo = &NT36675_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x26, 0x65, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36526_memory_map,  .hwinfo = &NT36526_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x75, 0x66, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36675_memory_map,  .hwinfo = &NT36675_hw_info},
	{.id = {0x0B, 0xFF, 0xFF, 0x72, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0B, 0xFF, 0xFF, 0x82, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0B, 0xFF, 0xFF, 0x25, 0x65, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0A, 0xFF, 0xFF, 0x72, 0x65, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0A, 0xFF, 0xFF, 0x72, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0A, 0xFF, 0xFF, 0x82, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0A, 0xFF, 0xFF, 0x70, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0B, 0xFF, 0xFF, 0x70, 0x66, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x0A, 0xFF, 0xFF, 0x72, 0x67, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36672A_memory_map, .hwinfo = &NT36672A_hw_info},
	{.id = {0x55, 0x00, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0x55, 0x72, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xAA, 0x00, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xAA, 0x72, 0xFF, 0x00, 0x00, 0x00}, .mask = {1, 1, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x72, 0x67, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x70, 0x66, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x70, 0x67, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x72, 0x66, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36772_memory_map,  .hwinfo = &NT36772_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x25, 0x65, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36525_memory_map,  .hwinfo = &NT36525_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x76, 0x66, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36676F_memory_map, .hwinfo = &NT36676F_hw_info}
};
