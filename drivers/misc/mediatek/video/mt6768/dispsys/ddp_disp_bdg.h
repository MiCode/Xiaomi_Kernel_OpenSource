/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _DDP_DISP_BDG_H_
#define _DDP_DISP_BDG_H_

#include "../../../hifi4dsp_spi/hifi4dsp_spi.h"
#include "ddp_hal.h"
#include "ddp_info.h"
#include "lcm_drv.h"

#define SPI_SPEED		(10000000)
#define HW_NUM			(1)

enum DISP_BDG_ENUM {
	DISP_BDG_DSI0 = 0,
	DISP_BDG_DSI1,
	DISP_BDG_DSIDUAL,
	DISP_BDG_NUM
};

enum MIPI_TX_PAD_VALUE {
	PAD_D2P = 0,
	PAD_D2N,
	PAD_D0P,
	PAD_D0N,
	PAD_CKP,
	PAD_CKN,
	PAD_D1P,
	PAD_D1N,
	PAD_D3P,
	PAD_D3N,
	PAD_MAX_NUM
};

#define	TX_DCS_SHORT_PACKET_ID_0			(0x05)
#define	TX_DCS_SHORT_PACKET_ID_1			(0x15)
#define	TX_DCS_LONG_PACKET_ID				(0x39)
#define	TX_DCS_READ_PACKET_ID				(0x06)
#define	TX_GERNERIC_SHORT_PACKET_ID_1			(0x13)
#define	TX_GERNERIC_SHORT_PACKET_ID_2			(0x23)
#define	TX_GERNERIC_LONG_PACKET_ID			(0x29)
#define	TX_GERNERIC_READ_LONG_PACKET_ID			(0x14)

#define REG_FLAG_ESCAPE_ID				(0x00)
#define REG_FLAG_DELAY_MS_V3				(0xFF)

int bdg_tx_init(enum DISP_BDG_ENUM module,
		   struct disp_ddp_path_config *config, void *cmdq);
int bdg_tx_deinit(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_common_deinit(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_common_init(enum DISP_BDG_ENUM module,
			struct disp_ddp_path_config *config, void *cmdq);
int bdg_common_init_for_rx_pat(enum DISP_BDG_ENUM module,
			struct disp_ddp_path_config *config, void *cmdq);
int mipi_dsi_rx_mac_init(enum DISP_BDG_ENUM module,
			struct disp_ddp_path_config *config, void *cmdq);
void bdg_tx_pull_6382_reset_pin(void);
void bdg_tx_set_6382_reset_pin(unsigned int value);
void bdg_tx_set_test_pattern(void);
int bdg_tx_bist_pattern(enum DISP_BDG_ENUM module,
				void *cmdq, bool enable, unsigned int sel,
				unsigned int red, unsigned int green,
				unsigned int blue);
int bdg_tx_set_mode(enum DISP_BDG_ENUM module,
				void *cmdq, unsigned int mode);

int bdg_tx_start(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_tx_stop(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_tx_wait_for_idle(enum DISP_BDG_ENUM module);
int bdg_dsi_dump_reg(enum DISP_BDG_ENUM module);

unsigned int get_ap_data_rate(void);
unsigned int get_bdg_data_rate(void);
int set_bdg_data_rate(unsigned int data_rate);
unsigned int get_bdg_line_cycle(void);
unsigned int get_dsc_state(void);
int check_stopstate(void *cmdq);

unsigned int mtk_spi_read(u32 addr);
int mtk_spi_write(u32 addr, unsigned int regval);
int mtk_spi_mask_write(u32 addr, u32 msk, u32 value);

#endif

