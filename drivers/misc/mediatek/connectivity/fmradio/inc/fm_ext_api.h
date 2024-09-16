/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef FM_EXT_API_H
#define FM_EXT_API_H

#include <linux/platform_device.h>

#include "fm_interface.h"

enum fm_spi_speed {
	FM_SPI_SPEED_26M,
	FM_SPI_SPEED_64M,
	FM_SPI_SPEED_MAX
};

struct fm_ext_interface {
	void (*eint_handler)(void);
	void (*eint_cb)(void);
	void (*enable_eint)(void);
	void (*disable_eint)(void);
	int (*stp_send_data)(unsigned char *buf, unsigned int len);
	int (*stp_recv_data)(unsigned char *buf, unsigned int len);
	int (*stp_register_event_cb)(void *cb);
	int (*wmt_msgcb_reg)(void *data);
	int (*wmt_func_on)(void);
	int (*wmt_func_off)(void);
	unsigned int (*wmt_ic_info_get)(void);
	int (*wmt_chipid_query)(void);
	int (*get_hw_version)(void);
	unsigned char (*get_top_index)(void);
	unsigned int (*get_get_adie)(void);
	int (*spi_clock_switch)(enum fm_spi_speed speed);
	bool (*is_bus_hang)(void);
	int (*spi_hopping)(void);
	signed int (*low_ops_register)(struct fm_callback *cb, struct fm_basic_interface *bi);
	signed int (*low_ops_unregister)(struct fm_basic_interface *bi);
	signed int (*rds_ops_register)(struct fm_basic_interface *bi, struct fm_rds_interface *ri);
	signed int (*rds_ops_unregister)(struct fm_rds_interface *ri);

	struct platform_driver *drv;
	unsigned int irq_id;
};

#endif /* FM_EXT_API_H */
