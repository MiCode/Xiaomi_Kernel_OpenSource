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

#include <mach/camera2.h>
#include "msm_camera_spi.h"

#undef SPIDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define SPIDBG(fmt, args...) pr_debug(fmt, ##args)
#define S_I2C_DBG(fmt, args...) pr_debug(fmt, ##args)
#else
#define SPIDBG(fmt, args...) do { } while (0)
#define S_I2C_DBG(fmt, args...) do { } while (0)
#endif

static int msm_camera_spi_txfr(struct spi_device *spi, char *txbuf,
			       char *rxbuf, int num_byte)
{
	struct spi_transfer t;
	struct spi_message m;

	memset(&t, 0, sizeof(t));
	t.tx_buf = txbuf;
	t.rx_buf = rxbuf;
	t.len = num_byte;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	return spi_sync(spi, &m);
}

/**
  * msm_camera_set_addr() - helper function to set transfer address
  * @addr:	device address
  * @addr_len:	the addr field length of an instruction
  * @type:	type (i.e. byte-length) of @addr
  * @str:	shifted address output, must be zeroed when passed in
  *
  * This helper function sets @str based on the addr field length of an
  * instruction and the data length.
  */
static void msm_camera_set_addr(uint32_t addr, uint8_t addr_len,
				enum msm_camera_i2c_reg_addr_type type,
				char *str)
{
	int i, len;

	if (addr_len < type)
		SPIDBG("%s: omitting higher bits in address\n", __func__);

	/* only support transfer MSB first for now */
	len = addr_len - type;
	for (i = len; i < addr_len; i++) {
		if (i >= 0)
			str[i] = (addr >> (BITS_PER_BYTE * (addr_len - i - 1)))
				& 0xFF;
	}

}

static int32_t msm_camera_spi_read_helper(struct msm_camera_i2c_client *client,
		struct msm_camera_spi_inst *inst, uint32_t addr, uint8_t *data,
		uint16_t num_byte)
{
	int32_t rc = -EFAULT;
	struct spi_device *spi = client->spi_client->spi_master;
	char *tx, *rx;
	uint16_t len;
	int8_t retries = client->spi_client->retries;

	if ((client->addr_type != MSM_CAMERA_I2C_BYTE_ADDR)
	    && (client->addr_type != MSM_CAMERA_I2C_WORD_ADDR)
	    && (client->addr_type != MSM_CAMERA_I2C_3B_ADDR))
		return rc;

	len = sizeof(inst->opcode) + inst->addr_len + inst->dummy_len
		+ num_byte;

	tx = kmalloc(len, GFP_KERNEL | GFP_DMA);
	if (!tx)
		return -ENOMEM;
	rx = kmalloc(len, GFP_KERNEL | GFP_DMA);
	if (!rx) {
		kfree(tx);
		return -ENOMEM;
	}
	memset(tx, 0, len);
	memset(rx, 0, len);

	tx[0] = inst->opcode;
	msm_camera_set_addr(addr, inst->addr_len, client->addr_type, tx + 1);
	while ((rc = msm_camera_spi_txfr(spi, tx, rx, len)) && retries) {
		retries--;
		msleep(client->spi_client->retry_delay);
	}
	if (rc) {
		SPIDBG("%s: failed %d\n", __func__, rc);
		goto out;
	}
	len = sizeof(inst->opcode) + inst->addr_len + inst->dummy_len;
	memcpy(data, rx + len, num_byte);
out:
	kfree(tx);
	kfree(rx);
	return rc;
}

int32_t msm_camera_spi_read(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type)
{
	int32_t rc = -EFAULT;
	uint8_t temp[2];

	if ((data_type != MSM_CAMERA_I2C_BYTE_DATA)
	    && (data_type != MSM_CAMERA_I2C_WORD_DATA))
		return rc;

	rc = msm_camera_spi_read_helper(client,
		&client->spi_client->cmd_tbl.read, addr, &temp[0], data_type);
	if (rc)
		return rc;

	if (data_type == MSM_CAMERA_I2C_BYTE_DATA)
		*data = temp[0];
	else
		*data = (temp[0] << BITS_PER_BYTE) | temp[1];

	SPIDBG("%s: addr 0x%x, data %u\n", __func__, addr, *data);
	return rc;
}

int32_t msm_camera_spi_read_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint16_t num_byte)
{
	return msm_camera_spi_read_helper(client,
		&client->spi_client->cmd_tbl.read_seq, addr, data, num_byte);
}

int32_t msm_camera_spi_query_id(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint16_t num_byte)
{
	return msm_camera_spi_read_helper(client,
		&client->spi_client->cmd_tbl.query_id, addr, data, num_byte);
}
