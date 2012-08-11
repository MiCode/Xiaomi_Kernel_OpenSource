/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __WCD9310_SLIMSLAVE_H_
#define __WCD9310_SLIMSLAVE_H_

#include <linux/slimbus/slimbus.h>
#include <linux/mfd/wcd9xxx/core.h>

/* Channel numbers to be used for each port */
enum {
	SLIM_TX_1   = 128,
	SLIM_TX_2   = 129,
	SLIM_TX_3   = 130,
	SLIM_TX_4   = 131,
	SLIM_TX_5   = 132,
	SLIM_TX_6   = 133,
	SLIM_TX_7   = 134,
	SLIM_TX_8   = 135,
	SLIM_TX_9   = 136,
	SLIM_TX_10  = 137,
	SLIM_RX_1   = 138,
	SLIM_RX_2   = 139,
	SLIM_RX_3   = 140,
	SLIM_RX_4   = 141,
	SLIM_RX_5   = 142,
	SLIM_RX_6   = 143,
	SLIM_RX_7   = 144,
	SLIM_MAX    = 145
};

/*
 *  client is expected to give port ids in the range of
 *  1-10 for pre Taiko Tx ports and 1-16 for Taiko
 *  1-7 for pre Taiko Rx ports and 1-16 for Tako,
 *  we need to add offset for getting the absolute slave
 *  port id before configuring the HW
 */
#define TABLA_SB_PGD_MAX_NUMBER_OF_TX_SLAVE_DEV_PORTS 10
#define TAIKO_SB_PGD_MAX_NUMBER_OF_TX_SLAVE_DEV_PORTS 16

#define SLIM_MAX_TX_PORTS TAIKO_SB_PGD_MAX_NUMBER_OF_TX_SLAVE_DEV_PORTS

#define TABLA_SB_PGD_OFFSET_OF_RX_SLAVE_DEV_PORTS \
	TABLA_SB_PGD_MAX_NUMBER_OF_TX_SLAVE_DEV_PORTS
#define TAIKO_SB_PGD_OFFSET_OF_RX_SLAVE_DEV_PORTS \
	TAIKO_SB_PGD_MAX_NUMBER_OF_TX_SLAVE_DEV_PORTS

#define TABLA_SB_PGD_MAX_NUMBER_OF_RX_SLAVE_DEV_PORTS 7
#define TAIKO_SB_PGD_MAX_NUMBER_OF_RX_SLAVE_DEV_PORTS 13

#define SLIM_MAX_RX_PORTS TAIKO_SB_PGD_MAX_NUMBER_OF_RX_SLAVE_DEV_PORTS

#define TABLA_SB_PGD_RX_PORT_MULTI_CHANNEL_0_START_PORT_ID \
	TABLA_SB_PGD_OFFSET_OF_RX_SLAVE_DEV_PORTS
#define TAIKO_SB_PGD_RX_PORT_MULTI_CHANNEL_0_START_PORT_ID \
	TAIKO_SB_PGD_OFFSET_OF_RX_SLAVE_DEV_PORTS

#define TABLA_SB_PGD_RX_PORT_MULTI_CHANNEL_0_END_PORT_ID 16
#define TAIKO_SB_PGD_RX_PORT_MULTI_CHANNEL_0_END_PORT_ID 31

#define TABLA_SB_PGD_TX_PORT_MULTI_CHANNEL_1_END_PORT_ID 9
#define TAIKO_SB_PGD_TX_PORT_MULTI_CHANNEL_1_END_PORT_ID 15

/* below details are taken from SLIMBUS slave SWI */
#define SB_PGD_PORT_BASE 0x000

#define SB_PGD_PORT_CFG_BYTE_ADDR(offset, port_num) \
		(SB_PGD_PORT_BASE + offset + (1 * port_num))

#define SB_PGD_TX_PORT_MULTI_CHANNEL_0(port_num) \
		(SB_PGD_PORT_BASE + 0x100 + 4*port_num)
#define SB_PGD_TX_PORT_MULTI_CHANNEL_0_START_PORT_ID   0
#define SB_PGD_TX_PORT_MULTI_CHANNEL_0_END_PORT_ID     7

#define SB_PGD_TX_PORT_MULTI_CHANNEL_1(port_num) \
		(SB_PGD_PORT_BASE + 0x101 + 4*port_num)
#define SB_PGD_TX_PORT_MULTI_CHANNEL_1_START_PORT_ID   8

#define SB_PGD_RX_PORT_MULTI_CHANNEL_0(offset, port_num) \
		(SB_PGD_PORT_BASE + offset + (4 * port_num))

/* slave port water mark level
 *   (0: 6bytes, 1: 9bytes, 2: 12 bytes, 3: 15 bytes)
 */
#define SLAVE_PORT_WATER_MARK_VALUE 2
#define SLAVE_PORT_WATER_MARK_SHIFT 1
#define SLAVE_PORT_ENABLE           1
#define SLAVE_PORT_DISABLE          0

#define BASE_CH_NUM 128


int wcd9xxx_init_slimslave(struct wcd9xxx *wcd9xxx, u8 wcd9xxx_pgd_la);

int wcd9xxx_deinit_slimslave(struct wcd9xxx *wcd9xxx);

int wcd9xxx_cfg_slim_sch_rx(struct wcd9xxx *wcd9xxx, unsigned int *ch_num,
				unsigned int tot_ch, unsigned int rate);
int wcd9xxx_cfg_slim_sch_tx(struct wcd9xxx *wcd9xxx, unsigned int *ch_num,
				unsigned int tot_ch, unsigned int rate);
int wcd9xxx_close_slim_sch_rx(struct wcd9xxx *wcd9xxx, unsigned int *ch_num,
				unsigned int tot_ch);
int wcd9xxx_close_slim_sch_tx(struct wcd9xxx *wcd9xxx, unsigned int *ch_num,
				unsigned int tot_ch);
int wcd9xxx_get_channel(struct wcd9xxx *wcd9xxx,
			unsigned int *rx_ch,
			unsigned int *tx_ch);
int wcd9xxx_get_slave_port(unsigned int ch_num);
int wcd9xxx_disconnect_port(struct wcd9xxx *wcd9xxx, unsigned int *ch_num,
				unsigned int tot_ch, unsigned int rx_tx);
#endif /* __WCD9310_SLIMSLAVE_H_ */
