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

#ifndef __DSI_DRV_H__
#define __DSI_DRV_H__

#include "lcm_drv.h"
#include "ddp_hal.h"
#include "fbconfig_kdebug_x.h"
#include "disp_drv_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UINT8	unsigned char
#define UINT16	unsigned short
#define UINT32	unsigned int
#define INT32	int

/* ------------------------------------------------------------------------- */

#define DSI_CHECK_RET(expr)			\
	do {					\
		enum DSI_STATUS ret = (expr);	\
		ASSERT(ret == DSI_STATUS_OK);	\
	} while (0)

/* ------------------------------------------------------------------------- */

#define	DSI_DCS_SHORT_PACKET_ID_0	0x05
#define	DSI_DCS_SHORT_PACKET_ID_1	0x15
#define	DSI_DCS_LONG_PACKET_ID		0x39
#define	DSI_DCS_READ_PACKET_ID		0x06

#define	DSI_GERNERIC_SHORT_PACKET_ID_1	0x13
#define	DSI_GERNERIC_SHORT_PACKET_ID_2	0x23
#define	DSI_GERNERIC_LONG_PACKET_ID	0x29
#define	DSI_GERNERIC_READ_LONG_PACKET_ID	0x14

#define	DSI_WMEM_CONTI	(0x3C)
#define	DSI_RMEM_CONTI	(0x3E)

/* ESD recovery method for video mode LCM */
#define	METHOD_NONCONTINUOUS_CLK	(0x1)
#define	METHOD_BUS_TURN_AROUND		(0x2)

/* State of DSI engine */
#define	DSI_VDO_VSA_VS_STATE		(0x008)
#define	DSI_VDO_VSA_HS_STATE		(0x010)
#define	DSI_VDO_VSA_VE_STATE		(0x020)
#define	DSI_VDO_VBP_STATE		(0x040)
#define	DSI_VDO_VACT_STATE		(0x080)
#define	DSI_VDO_VFP_STATE		(0x100)

/* ------------------------------------------------------------------------- */
enum DSI_STATUS {
	DSI_STATUS_OK = 0,
	DSI_STATUS_ERROR,
};

enum DSI_INS_TYPE {
	SHORT_PACKET_RW = 0,
	FB_WRITE = 1,
	LONG_PACKET_W = 2,
	FB_READ = 3,
};

enum DSI_CMDQ_BTA {
	DISABLE_BTA = 0,
	ENABLE_BTA = 1,
};

enum DSI_CMDQ_HS {
	LOW_POWER = 0,
	HIGH_SPEED = 1,
};

enum DSI_CMDQ_CL {
	CL_8BITS = 0,
	CL_16BITS = 1,
};

enum DSI_CMDQ_TE {
	DISABLE_TE = 0,
	ENABLE_TE = 1,
};

enum DSI_CMDQ_RPT {
	DISABLE_RPT = 0,
	ENABLE_RPT = 1,
};

struct DSI_CMDQ_CONFG {
	unsigned type:2;
	unsigned BTA:1;
	unsigned HS:1;
	unsigned CL:1;
	unsigned TE:1;
	unsigned Rsv:1;
	unsigned RPT:1;
};

struct DSI_T0_INS {
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned Data0:8;
	unsigned Data1:8;
};

struct DSI_T1_INS {
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned mem_start0:8;
	unsigned mem_start1:8;
};

struct DSI_T2_INS {
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned WC16:16;
	unsigned int *pdata;
};

struct DSI_T3_INS {
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned mem_start0:8;
	unsigned mem_start1:8;
};

struct DSI_PLL_CONFIG {
	UINT8 TXDIV0;
	UINT8 TXDIV1;
	UINT32 SDM_PCW;
	UINT8 SSC_PH_INIT;
	UINT16 SSC_PRD;
	UINT16 SSC_DELTA1;
	UINT16 SSC_DELTA;
};

enum DSI_INTERFACE_ID {
	DSI_INTERFACE_0 = 0,
	DSI_INTERFACE_1 = 1,
	DSI_INTERFACE_NUM,
};

enum DSI_PORCH_TYPE {
	DSI_VFP = 0,
	DSI_VSA,
	DSI_VBP,
	DSI_VACT,
	DSI_HFP,
	DSI_HSA,
	DSI_HBP,
	DSI_BLLP,
	DSI_PORCH_NUM,
};

extern const struct LCM_UTIL_FUNCS PM_lcm_utils_dsi0;
/* defined in mtkfb.c */
extern bool is_ipoh_bootup;

void DSI_ChangeClk(enum DISP_MODULE_ENUM module, UINT32 clk);
INT32 DSI_ssc_enable(UINT32 dsi_idx, UINT32 en);
UINT32 PanelMaster_get_CC(UINT32 dsi_idx);
void PanelMaster_set_CC(UINT32 dsi_index, UINT32 enable);
UINT32 PanelMaster_get_dsi_timing(UINT32 dsi_index,
				  enum MIPI_SETTING_TYPE type);
UINT32 PanelMaster_get_TE_status(UINT32 dsi_idx);
void PanelMaster_DSI_set_timing(UINT32 dsi_index, struct MIPI_TIMING timing);
unsigned int PanelMaster_set_PM_enable(unsigned int value);
UINT32 DSI_dcs_read_lcm_reg_v2(enum DISP_MODULE_ENUM module,
			       struct cmdqRecStruct *cmdq, UINT8 cmd,
			       UINT8 *buffer, UINT8 buffer_size);
void *get_dsi_params_handle(UINT32 dsi_idx);
void dsi_analysis(enum DISP_MODULE_ENUM module);
void DSI_LFR_UPDATE(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq);
void DSI_Set_LFR(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq,
		 unsigned int mode, unsigned int type, unsigned int enable,
		 unsigned int skip_num);
enum DSI_STATUS DSI_BIST_Pattern_Test(enum DISP_MODULE_ENUM module,
				      struct cmdqRecStruct *cmdq, bool enable,
				      unsigned int color);
int ddp_dsi_start(enum DISP_MODULE_ENUM module, void *cmdq);
enum DSI_STATUS DSI_DumpRegisters(enum DISP_MODULE_ENUM module, int level);
void DSI_ForceConfig(int forceconfig);
int DSI_set_roi(int x, int y);
int DSI_check_roi(void);
int ddp_dsi_trigger(enum DISP_MODULE_ENUM module, void *cmdq);
void DSI_set_cmdq_V2(enum DISP_MODULE_ENUM module, struct cmdqRecStruct *cmdq,
		     unsigned int cmd, unsigned char count,
		     unsigned char *para_list, unsigned char force_update);

int dsi_enable_irq(enum DISP_MODULE_ENUM module, void *handle,
		   unsigned int enable);
int ddp_dsi_power_on(enum DISP_MODULE_ENUM module, void *cmdq_handle);
void ddp_dump_and_reset_dsi0(void);

#ifdef __cplusplus
}
#endif

#endif /* __DSI_DRV_H__ */
