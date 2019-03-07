/*
 * dbmdx-spi.h  --  DBMDX SPI interface common functions
 *
 * Copyright (C) 2014 DSP Group
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DBMDX_SPI_H
#define _DBMDX_SPI_H

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#endif

#define RETRY_COUNT				5

struct dbmdx_spi_private;
#define DBMDX_VA_SPI_CMD_PADDED_SIZE 150

struct dbmdx_spi_data {
	u32		spi_speed;
	u32		read_chunk_size;
	u32		write_chunk_size;
	u32		dma_min_buffer_size;
	u8		*send;
	u8		*recv;
	u32		bits_per_word;
	u32		bytes_per_word;
};



struct dbmdx_spi_private {
	struct device			*dev;
	struct dbmdx_spi_data		*pdata;
	struct spi_device		*client;
	struct chip_interface		chip;
#ifdef CONFIG_PM_WAKELOCKS
	struct wakeup_source		ps_nosuspend_wl;
#endif
	u16				post_pll_div;
	u32				interface_enabled;
};



ssize_t read_spi_data(struct dbmdx_private *p, void *buf, size_t len);
ssize_t send_spi_data(struct dbmdx_private *p, const void *buf,
			      size_t len);
ssize_t write_spi_data(struct dbmdx_private *p, const u8 *buf,
			      size_t len);
int send_spi_cmd_boot(struct dbmdx_private *p, u32 command);
ssize_t send_spi_cmd_va(struct dbmdx_private *p, u32 command,
				   u16 *response);
ssize_t send_spi_cmd_vqe(struct dbmdx_private *p,
	u32 command, u16 *response);
int spi_verify_boot_checksum(struct dbmdx_private *p,
	const void *checksum, size_t chksum_len);
int spi_verify_chip_id(struct dbmdx_private *p);
int spi_common_probe(struct spi_device *client);
int spi_common_remove(struct spi_device *client);
int spi_set_speed(struct dbmdx_private *p, int index);
void spi_interface_resume(struct dbmdx_spi_private *spi_p);
void spi_interface_suspend(struct dbmdx_spi_private *spi_p);

#endif
