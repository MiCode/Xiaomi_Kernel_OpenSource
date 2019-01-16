/*
 * drivers/mmc/card/cbp_sdio.h
 *
 * VIA CBP SDIO driver for Linux
 *
 * Copyright (C) 2009 VIA TELECOM Corporation, Inc.
 * Author: VIA TELECOM Corporation, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef CBP_SDIO_H
#define CBP_SDIO_H

#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/mmc/host.h>
#include "viatel.h"

struct cbp_wait_event{
	wait_queue_head_t wait_q;
	atomic_t state;
	int wait_gpio;
	int wait_polar;
};

struct cbp_reset{
	struct mmc_host *host;
	const char  *name;
	struct workqueue_struct *reset_wq;
	struct work_struct	reset_work;
	struct timer_list timer_gpio;
	int rst_ind_gpio;
	int rst_ind_polar;
};

struct cbp_platform_data {
	char *bus;
	char *host_id;
	
	bool ipc_enable;
	bool data_ack_enable;
	bool rst_ind_enable;
	bool flow_ctrl_enable;
	bool tx_disable_irq;
	struct asc_config *tx_handle;
	
	int gpio_ap_wkup_cp;
	int gpio_cp_ready;
	int gpio_cp_wkup_ap;
	int gpio_ap_ready;
	int gpio_sync_polar;
	
	int gpio_data_ack;
	int gpio_data_ack_polar;
	
	int gpio_rst_ind;
	int gpio_rst_ind_polar;
	
	int gpio_flow_ctrl;
	int gpio_flow_ctrl_polar;
	
	int gpio_pwr_on;
	int gpio_rst;
	//for the level transfor chip fssd06
	int gpio_sd_select;
	int gpio_mc3_enable;
	
	struct cbp_wait_event *cbp_data_ack;
	void (*data_ack_wait_event)(struct cbp_wait_event *pdata_ack);
	struct cbp_wait_event *cbp_flow_ctrl;
	void (*flow_ctrl_wait_event)(struct cbp_wait_event *pflow_ctrl);
	
	int (*detect_host)(const char *host_id);
	int (*cbp_setup)(struct cbp_platform_data *pdata);
	void (*cbp_destroy)(void);
};

typedef enum{
	MODEM_ST_READY = 0, /*modem ready*/
	MODEM_ST_TX_RX,
	MODEM_ST_UNKNOW
}data_ack_state;

typedef enum{
	FLOW_CTRL_DISABLE = 0,
	FLOW_CTRL_ENABLE
}flow_ctrl_state;
#endif
