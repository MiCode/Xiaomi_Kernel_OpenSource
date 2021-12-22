/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef _DDP_DISP_BDG_H_
#define _DDP_DISP_BDG_H_

#include "spi_slave.h"
#include "mtk_dsi.h"
#include <linux/interrupt.h>

#define HW_NUM			(1)
#define RX_V12			(1700)

#ifdef _90HZ_
#define _Disable_HS_DCO_
#define _Disable_LP_TX_L023_
#else
#define _G_MODE_EN_
#endif

extern unsigned int need_6382_init;
extern atomic_t bdg_eint_wakeup;
extern unsigned int line_back_to_LP;
extern unsigned int bdg_rxtx_ratio;

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

enum DSI_PS_TYPE {
	PACKED_PS_16BIT_RGB565 = 0,
	LOOSELY_PS_24BIT_RGB666 = 1,
	PACKED_PS_18BIT_RGB666 = 2,
	PACKED_PS_24BIT_RGB888 = 3,
	PACKED_PS_30BIT_RGB101010 = 4,
	PACKED_COMPRESSION = 5
};

#define	TX_DCS_SHORT_PACKET_ID_0			(0x05)
#define	TX_DCS_SHORT_PACKET_ID_1			(0x15)
#define	TX_DCS_LONG_PACKET_ID				(0x39)
#define	TX_DCS_READ_PACKET_ID				(0x06)
#define	TX_GERNERIC_SHORT_PACKET_ID_1		(0x13)
#define	TX_GERNERIC_SHORT_PACKET_ID_2		(0x23)
#define	TX_GERNERIC_LONG_PACKET_ID			(0x29)
#define	TX_GERNERIC_READ_LONG_PACKET_ID		(0x14)
#define REG_FLAG_ESCAPE_ID					(0x00)
#define REG_FLAG_DELAY_MS_V3				(0xFF)

#define DISPSYS_BDG_SYSREG_CTRL_BASE		0x00000000UL
#define DISPSYS_BDG_TOPCKGEN_BASE			0x00003000UL
#define DISPSYS_BDG_APMIXEDSYS_BASE			0x00004000UL
#define DISPSYS_BDG_GPIO_BASE				0x00007000UL
#define DISPSYS_BDG_EFUSE_BASE				0x00009000UL
#define DISPSYS_BDG_MIPIDSI2_DEVICE_BASE	0x0000d000UL
#define DISPSYS_BDG_GCE_BASE				0x00010000UL
#define DISPSYS_BDG_OCLA_BASE				0x00014000UL
#define DISPSYS_BDG_DISP_DSC_BASE			0x00020000UL
#define DISPSYS_BDG_TX_DSI0_BASE			0x00021000UL
#define DISPSYS_BDG_MIPI_TX_BASE			0x00022000UL
#define DISPSYS_BDG_MMSYS_CONFIG_BASE		0x00023000UL
#define DISPSYS_BDG_RDMA0_REGS_BASE			0x00024000UL
#define DISPSYS_BDG_MUTEX_REGS_BASE			0x00025000UL

int bdg_tx_init(enum DISP_BDG_ENUM module,
		   struct mtk_dsi *dsi, void *cmdq);
int bdg_tx_deinit(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_common_init(enum DISP_BDG_ENUM module,
			struct mtk_dsi *dsi, void *cmdq);
int bdg_common_deinit(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_common_init_for_rx_pat(enum DISP_BDG_ENUM module,
			struct mtk_dsi *dsi, void *cmdq);
int mipi_dsi_rx_mac_init(enum DISP_BDG_ENUM module,
			struct mtk_dsi *dsi, void *cmdq);
void bdg_tx_pull_6382_reset_pin(struct mtk_dsi *dsi);
void bdg_tx_set_6382_reset_pin(unsigned int value);
void bdg_tx_set_test_pattern(void);
int bdg_tx_bist_pattern(enum DISP_BDG_ENUM module,
				void *cmdq, bool enable, unsigned int sel,
				unsigned int red, unsigned int green,
				unsigned int blue);
int bdg_tx_set_mode(enum DISP_BDG_ENUM module,
				void *cmdq, struct mtk_dsi *dsi);
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
int lcm_init(enum DISP_BDG_ENUM module);
void BDG_set_cmdq_V2_DSI0(void *cmdq, unsigned int cmd, unsigned char count,
	unsigned char *para_list, unsigned char force_update);
unsigned int get_mt6382_init(void);
unsigned int get_bdg_tx_mode(void);
int bdg_tx_cmd_mode(enum DISP_BDG_ENUM module, void *cmdq);
int bdg_mutex_trigger(enum DISP_BDG_ENUM module, void *cmdq);
unsigned int mtk_spi_read(u32 addr);
int mtk_spi_write(u32 addr, unsigned int regval);
int mtk_spi_mask_write(u32 addr, u32 msk, u32 value);
void bdg_mipi_clk_change(enum DISP_BDG_ENUM module,
			struct mtk_dsi *dsi, void *cmdq);
int bdg_tx_reset(enum DISP_BDG_ENUM module, void *cmdq);

//irqreturn_t bdg_eint_irq_handler(int irq, void *data);
void bdg_first_init(void);
irqreturn_t bdg_eint_thread_handler(int irq, void *data);
void bdg_request_eint_irq(void);
//void bdg_free_eint_irq(void);
void bdg_rx_reset(void *cmdq);

#endif
