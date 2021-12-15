/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _DDP_DISP_BDG_H_
#define _DDP_DISP_BDG_H_

#include "../../../spi_slave_drv/spi_slave.h"
#include "ddp_hal.h"
#include "ddp_info.h"
#include "lcm_drv.h"
#include <linux/interrupt.h>

#define HW_NUM			(2)
#define RX_V12			(1720)
#define _n36672c_
#define _Disable_HS_DCO_
#define _Disable_LP_TX_L023_
//#define _G_MODE_EN_
//#define _HIGH_FRM_
#ifdef _HIGH_FRM_	 //for cmd 120Hz
#define RXTX_RATIO		(299)
#else
#define RXTX_RATIO		(225) //for vdo 90Hz
//#define RXTX_RATIO		(230) //for vdo 120Hz
#endif

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
int bdg_common_init(enum DISP_BDG_ENUM module,
			struct disp_ddp_path_config *config, void *cmdq);
int bdg_common_deinit(enum DISP_BDG_ENUM module, void *cmdq);
void bdg_register_init(void);
int bdg_is_bdg_connected(void);
int bdg_common_init_for_rx_pat(enum DISP_BDG_ENUM module,
			struct disp_ddp_path_config *config, void *cmdq);
int mipi_dsi_rx_mac_init(enum DISP_BDG_ENUM module,
			struct disp_ddp_path_config *config, void *cmdq);
void bdg_tx_pull_6382_reset_pin(void);
void bdg_tx_set_6382_reset_pin(unsigned int value);
int bdg_tx_bist_pattern(enum DISP_BDG_ENUM module,
				void *cmdq, bool enable, unsigned int sel,
				unsigned int red, unsigned int green,
				unsigned int blue);
int bdg_tx_set_mode(enum DISP_BDG_ENUM module,
				void *cmdq, unsigned int mode);
int bdg_mipi_clk_change(int msg, int en);
int bdg_mipi_clk_change_for_resume(int msg, int en);
int bdg_tx_start(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_tx_stop(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_tx_cmd_mode(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_mutex_trigger(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_tx_reset(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_vm_mode_set(enum DISP_BDG_ENUM module, bool enable,
			unsigned int long_pkt, void *cmdq); /* not use */
int bdg_tx_wait_for_idle(enum DISP_BDG_ENUM module);
int bdg_dsi_dump_reg(enum DISP_BDG_ENUM module, unsigned int level);
int bdg_set_dcs_read_cmd(bool enable, void *cmdq);
int bdg_tx_clr_sta(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_dsi_stop_vdo_gce(void);

unsigned int get_ap_data_rate(void);
unsigned int get_bdg_data_rate(void);
int set_bdg_data_rate(unsigned int data_rate);
unsigned int get_bdg_line_cycle(void);
unsigned int get_dsc_state(void);
void set_mt6382_init(unsigned int value);
unsigned int get_mt6382_init(void);
unsigned int get_bdg_tx_mode(void);
void set_bdg_tx_mode(unsigned int value);
int check_stopstate(void *cmdq);
int polling_status(void);

void BDG_set_cmdq_V2_DSI0(void *cmdq, unsigned int cmd, unsigned char count,
	unsigned char *para_list, unsigned char force_update);
unsigned int mtk_spi_read(u32 addr);
int mtk_spi_write(u32 addr, unsigned int regval);
int mtk_spi_mask_write(u32 addr, u32 msk, u32 value);

//irqreturn_t bdg_eint_irq_handler(int irq, void *data);
irqreturn_t bdg_eint_thread_handler(int irq, void *data);
void bdg_request_eint_irq(void);
void bdg_first_init(void);
//void bdg_free_eint_irq(void);

#endif
