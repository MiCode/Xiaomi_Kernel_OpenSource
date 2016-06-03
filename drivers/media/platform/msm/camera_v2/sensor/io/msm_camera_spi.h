/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_CAMERA_SPI_H
#define __MSM_CAMERA_SPI_H

#include <linux/spi/spi.h>
#include <media/msm_cam_sensor.h>
#include "msm_camera_i2c.h"

/**
  * Common SPI communication scheme
  * tx: <opcode>[addr][wait][write buffer]
  * rx: [read buffer]
  * Some inst require polling busy reg until it's done
  */
struct msm_camera_spi_inst {
	uint8_t opcode;		/* one-byte opcode */
	uint8_t addr_len;	/* addr len in bytes */
	uint8_t dummy_len;	/* setup cycles */
};

struct msm_camera_spi_inst_tbl {
	struct msm_camera_spi_inst read;
	struct msm_camera_spi_inst read_seq;
	struct msm_camera_spi_inst query_id;
};

struct msm_camera_spi_client {
	struct spi_device *spi_master;
	struct msm_camera_spi_inst_tbl cmd_tbl;
	uint8_t device_id;
	uint8_t mfr_id;
	uint8_t retry_delay;	/* ms */
	uint8_t retries;	/* retry times upon failure */
};

static __always_inline
uint16_t msm_camera_spi_get_hlen(struct msm_camera_spi_inst *inst)
{
	return sizeof(inst->opcode) + inst->addr_len + inst->dummy_len;
}

int32_t msm_camera_spi_read(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_spi_read_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte);

int32_t msm_camera_spi_read_seq_l(struct msm_camera_i2c_client *client,
	uint32_t addr, uint32_t num_byte, char *tx, char *rx);

int32_t msm_camera_spi_query_id(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte);

#endif
