/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_SENSOR_SPI_H_
#define _CAM_SENSOR_SPI_H_

#include <linux/spi/spi.h>
#include <linux/cma.h>
#include <media/cam_sensor.h>
#include "cam_sensor_i2c.h"

#define MAX_SPI_SIZE 110
#define SPI_DYNAMIC_ALLOC

struct cam_camera_spi_inst {
	uint8_t opcode;
	uint8_t addr_len;
	uint8_t dummy_len;
	uint8_t delay_intv;
	uint8_t delay_count;
};

struct cam_spi_write_burst_data {
	u8 data_msb;
	u8 data_lsb;
};

struct cam_spi_write_burst_packet {
	u8 cmd;
	u8 addr_msb;
	u8 addr_lsb;
	struct cam_spi_write_burst_data data_arr[MAX_SPI_SIZE];
};

struct cam_camera_burst_info {
	uint32_t burst_addr;
	uint32_t burst_start;
	uint32_t burst_len;
	uint32_t chunk_size;
};

struct cam_camera_spi_inst_tbl {
	struct cam_camera_spi_inst read;
	struct cam_camera_spi_inst read_seq;
	struct cam_camera_spi_inst query_id;
	struct cam_camera_spi_inst page_program;
	struct cam_camera_spi_inst write_enable;
	struct cam_camera_spi_inst read_status;
	struct cam_camera_spi_inst erase;
};

struct cam_sensor_spi_client {
	struct spi_device *spi_master;
	struct cam_camera_spi_inst_tbl cmd_tbl;
	uint8_t device_id0;
	uint8_t device_id1;
	uint8_t mfr_id0;
	uint8_t mfr_id1;
	uint8_t retry_delay;
	uint8_t retries;
	uint8_t busy_mask;
	uint16_t page_size;
	uint32_t erase_size;
};
static __always_inline
uint16_t cam_camera_spi_get_hlen(struct cam_camera_spi_inst *inst)
{
	return sizeof(inst->opcode) + inst->addr_len + inst->dummy_len;
}

int cam_spi_read(struct camera_io_master *client,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type);

int cam_spi_read_seq(struct camera_io_master *client,
	uint32_t addr, uint8_t *data,
	enum camera_sensor_i2c_type addr_type,
	int32_t num_bytes);

int cam_spi_query_id(struct camera_io_master *client,
	uint32_t addr,
	enum camera_sensor_i2c_type addr_type,
	uint8_t *data, uint32_t num_byte);

int cam_spi_write(struct camera_io_master *client,
	uint32_t addr, uint32_t data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type);

int cam_spi_write_table(struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_setting);

int cam_spi_erase(struct camera_io_master *client,
	uint32_t addr, enum camera_sensor_i2c_type addr_type,
	uint32_t size);

int cam_spi_write_seq(struct camera_io_master *client,
	uint32_t addr, uint8_t *data,
	enum camera_sensor_i2c_type addr_type, uint32_t num_byte);
#endif
