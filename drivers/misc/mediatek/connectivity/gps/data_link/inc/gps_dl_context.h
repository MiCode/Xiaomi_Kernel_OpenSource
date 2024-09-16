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
#ifndef _GPS_DL_CONTEXT_H
#define _GPS_DL_CONTEXT_H

#include "gps_dl_config.h"
#include "gps_each_link.h"
#include "gps_dl_isr.h"

#if GPS_DL_ON_LINUX
#include "gps_each_device.h"
#include "gps_dl_linux.h"
#include "gps_dl_ctrld.h"
#define GPS_DL_TX_BUF_SIZE	(8 * 1024)
#define GPS_DL_RX_BUF_SIZE	(8 * 1024)
#else
#define GPS_DL_TX_BUF_SIZE	(4 * 1024)
#define GPS_DL_RX_BUF_SIZE	(4 * 1024)
#endif

struct gps_dl_remap_ctx {
	unsigned int gps_emi_phy_high20;
};

struct gps_dl_ctx {
	int major;
	int minor;
#if GPS_DL_ON_LINUX
	struct gps_each_device devices[GPS_DATA_LINK_NUM];
#endif
	struct gps_each_link links[GPS_DATA_LINK_NUM];
	struct gps_each_irq irqs[GPS_DL_IRQ_NUM];
	struct gps_dl_remap_ctx remap_ctx;
};

struct gps_dl_remap_ctx *gps_dl_remap_ctx_get(void);

struct gps_dl_runtime_cfg {
	bool dma_is_1byte_mode;
	bool dma_is_enabled;
	bool show_reg_rw_log;
	bool show_reg_wait_log;
	bool only_show_wait_done_log;
	enum gps_dl_log_level_enum log_level;
	unsigned int log_mod_bitmask;
	unsigned int log_reg_rw_bitmask;
};

bool gps_dl_is_1byte_mode(void);
bool gps_dl_is_dma_enabled(void);

bool gps_dl_show_reg_rw_log(void);
bool gps_dl_show_reg_wait_log(void);
bool gps_dl_only_show_wait_done_log(void);

bool gps_dl_set_1byte_mode(bool is_1byte_mode);
bool gps_dl_set_dma_enabled(bool to_enable);

bool gps_dl_set_show_reg_rw_log(bool on);
void gps_dl_set_show_reg_wait_log(bool on);

int gps_dl_set_rx_transfer_max(enum gps_dl_link_id_enum link_id, int max);
int gps_dl_set_tx_transfer_max(enum gps_dl_link_id_enum link_id, int max);

#endif /* _GPS_DL_CONTEXT_H */

