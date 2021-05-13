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

#ifndef _MT_DPE_H
#define _MT_DPE_H

#include <linux/ioctl.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*
 *   enforce kernel log enable
 */
#define KERNEL_LOG		/* enable debug log flag if defined */

#define _SUPPORT_MAX_DPE_FRAME_REQUEST_ 12 // 6
#define _SUPPORT_MAX_DPE_REQUEST_RING_SIZE_ 4


#define SIG_ERESTARTSYS 512	/* ERESTARTSYS */
/*
 *
 */
#define DPE_DEV_MAJOR_NUMBER    302

#define DPE_MAGIC               'd'

#define DPE_REG_RANGE           (0x1000)

#define DPE_BASE_HW             0x1B100000

/*This macro is for setting irq status represnted
 * by a local variable,DPEInfo.IrqInfo.Status[DPE_IRQ_TYPE_INT_DPE_ST]
 */
#define DPE_INT_ST              (1UL<<31)

// MEDV buffer width size should be 64B align
// All other buffer width size should be 128B align
// Assume max width = 640 should meet above requirements
#define DPE_MAX_FRAME_SIZE 307200 //640x480
#define WB_INT_MEDV_SIZE DPE_MAX_FRAME_SIZE
#define WB_DCV_L_SIZE DPE_MAX_FRAME_SIZE
#define WB_ASFRM_SIZE DPE_MAX_FRAME_SIZE
#define WB_ASFRMExt_SIZE DPE_MAX_FRAME_SIZE
#define WB_WMFHF_SIZE DPE_MAX_FRAME_SIZE

#define WB_TOTAL_SIZE \
	(WB_INT_MEDV_SIZE+WB_DCV_L_SIZE+ \
	WB_ASFRM_SIZE+WB_ASFRMExt_SIZE+WB_WMFHF_SIZE)

// ----------------- DPE_DVS_ME  Grouping Definitions -------------------
struct DVS_ME_CFG {
	unsigned int                   DVS_ME_00;           //1B100300
	unsigned int                   DVS_ME_01;           //1B100304
	unsigned int                   DVS_ME_02;           //1B100308
	unsigned int                   DVS_ME_03;           //1B10030C
	unsigned int                   DVS_ME_04;           //1B100310
	unsigned int                   DVS_ME_05;           //1B100314
	unsigned int                   DVS_ME_06;           //1B100318
	unsigned int                   DVS_ME_07;           //1B10031C
	unsigned int                   DVS_ME_08;           //1B100320
	unsigned int                   DVS_ME_09;           //1B100324
	unsigned int                   DVS_ME_10;           //1B100328
	unsigned int                   DVS_ME_11;           //1B10032C
	unsigned int                   DVS_ME_12;           //1B100330
	unsigned int                   DVS_ME_13;           //1B100334
	unsigned int                   DVS_ME_14;           //1B100338
	unsigned int                   DVS_ME_15;           //1B10033C
	unsigned int                   DVS_ME_16;           //1B100340
	unsigned int                   DVS_ME_17;           //1B100344
	unsigned int                   DVS_ME_18;           //1B100348
	unsigned int                   DVS_ME_19;           //1B10034C
	unsigned int                   DVS_ME_20;           //1B100350
	unsigned int                   DVS_ME_21;           //1B100354
	unsigned int                   DVS_ME_22;           //1B100358
	unsigned int                   DVS_ME_23;           //1B10035C
	unsigned int                   DVS_ME_24;           //1B100360
};

// ----------------- DPE_DVS_OCC  Grouping Definitions -------------------
struct DVS_OCC_CFG {
	unsigned int                DVS_OCC_PQ_0;        //1B1003A0
	unsigned int                DVS_OCC_PQ_1;        //1B1003A4
	unsigned int                DVS_OCC_PQ_2;        //1B1003A8
	unsigned int                DVS_OCC_PQ_3;        //1B1003AC
	unsigned int                DVS_OCC_PQ_4;        //1B1003B0
	unsigned int                DVS_OCC_PQ_5;        //1B1003B4
};

// ----------------- DPE_DVP_CTRL  Grouping Definitions -------------------
struct DVP_CORE_CFG {
	unsigned int                 DVP_CORE_00;         //1B100900
	unsigned int                 DVP_CORE_01;         //1B100904
	unsigned int                 DVP_CORE_02;         //1B100908
	unsigned int                 DVP_CORE_03;         //1B10090C
	unsigned int                 DVP_CORE_04;         //1B100910
	unsigned int                 DVP_CORE_05;         //1B100914
	unsigned int                 DVP_CORE_06;         //1B100918
	unsigned int                 DVP_CORE_07;         //1B10091C
	unsigned int                 DVP_CORE_08;         //1B100920
	unsigned int                 DVP_CORE_09;         //1B100924
	unsigned int                 DVP_CORE_10;         //1B100928
	unsigned int                 DVP_CORE_11;         //1B10092C
	unsigned int                 DVP_CORE_12;         //1B100930
	unsigned int                 DVP_CORE_13;         //1B100934
	unsigned int                 DVP_CORE_14;         //1B100938
	unsigned int                 DVP_CORE_15;         //1B10093C
};

// -----------------------------------------------------

struct DPE_REG_STRUCT {
	unsigned int module;
	unsigned int Addr;	/* register's addr */
	unsigned int Val;	/* register's value */
};

struct DPE_REG_IO_STRUCT {
	struct DPE_REG_STRUCT *pData;	/* pointer to DPE_REG_STRUCT */
	unsigned int Count;	/* count */
};

/*
 *   interrupt clear type
 */
enum DPE_IRQ_CLEAR_ENUM {
	DPE_IRQ_CLEAR_NONE,	/* non-clear wait, clear after wait */
	DPE_IRQ_CLEAR_WAIT,	/* clear wait, clear before and after wait */
	DPE_IRQ_WAIT_CLEAR,
	/* wait the signal and clear it, avoid hw executime is too s hort. */
	DPE_IRQ_CLEAR_STATUS,	/* clear specific status only */
	DPE_IRQ_CLEAR_ALL	/* clear all status */
};


/*
 *   module's interrupt , each module should have its own isr.
 *   note:
 *	mapping to isr table,ISR_TABLE when using no device tree
 */
enum DPE_IRQ_TYPE_ENUM {
	DPE_IRQ_TYPE_INT_DVP_ST,	/* DVP */
	DPE_IRQ_TYPE_INT_DVS_ST,	/* DVS */
	DPE_IRQ_TYPE_AMOUNT
};

struct DPE_WAIT_IRQ_STRUCT {
	enum DPE_IRQ_CLEAR_ENUM Clear;
	enum DPE_IRQ_TYPE_ENUM Type;
	unsigned int Status;	/*IRQ Status */
	unsigned int Timeout;
	int UserKey;		/* user key for doing interrupt operation */
	int ProcessID;		/* user ProcessID (will filled in kernel) */
	unsigned int bDumpReg;	/* check dump register or not */
};

struct DPE_CLEAR_IRQ_STRUCT {
	enum DPE_IRQ_TYPE_ENUM Type;
	int UserKey;		/* user key for doing interrupt operation */
	unsigned int Status;	/* Input */
};

struct DPE_Kernel_Config {
	unsigned int	DVS_CTRL00;
	unsigned int	DVS_CTRL01;
	unsigned int	DVS_CTRL02;
	unsigned int	DVS_CTRL03;
//	unsigned int	DVS_CTRL04;
//	unsigned int	DVS_CTRL05;
	unsigned int	DVS_CTRL06;
	unsigned int	DVS_CTRL07;
	unsigned int	DVS_IRQ_00;
	unsigned int	DVS_CTRL_STATUS0;
//	unsigned int	DVS_CTRL_STATUS1;
	unsigned int	DVS_CTRL_STATUS2;
	unsigned int	DVS_IRQ_STATUS;
	unsigned int	DVS_FRM_STATUS0;
	unsigned int	DVS_FRM_STATUS1;
//	unsigned int	DVS_FRM_STATUS2;
//	unsigned int	DVS_FRM_STATUS3;
	unsigned int	DVS_CUR_STATUS;
	unsigned int	DVS_SRC_CTRL;
	unsigned int	DVS_CRC_CTRL;
	unsigned int	DVS_CRC_IN;
	unsigned int	DVS_DRAM_STA0;
	unsigned int	DVS_DRAM_STA1;
	unsigned int	DVS_DRAM_ULT;
	unsigned int	DVS_DRAM_PITCH;
	unsigned int	DVS_SRC_00;
	unsigned int	DVS_SRC_01;
	unsigned int	DVS_SRC_02;
	unsigned int	DVS_SRC_03;
	unsigned int	DVS_SRC_04;
	unsigned int	DVS_SRC_05_L_FRM0;
	unsigned int	DVS_SRC_06_L_FRM1;
	unsigned int	DVS_SRC_07_L_FRM2;
	unsigned int	DVS_SRC_08_L_FRM3;
	unsigned int	DVS_SRC_09_R_FRM0;
	unsigned int	DVS_SRC_10_R_FRM1;
	unsigned int	DVS_SRC_11_R_FRM2;
	unsigned int	DVS_SRC_12_R_FRM3;
	unsigned int	DVS_SRC_13_L_VMAP0;
	unsigned int	DVS_SRC_14_L_VMAP1;
	unsigned int	DVS_SRC_15_L_VMAP2;
	unsigned int	DVS_SRC_16_L_VMAP3;
	unsigned int	DVS_SRC_17_R_VMAP0;
	unsigned int	DVS_SRC_18_R_VMAP1;
	unsigned int	DVS_SRC_19_R_VMAP2;
	unsigned int	DVS_SRC_20_R_VMAP3;
	unsigned int	DVS_SRC_21_INTER_MEDV;
	unsigned int	DVS_SRC_26_OCCDV0;
	unsigned int	DVS_SRC_27_OCCDV1;
	unsigned int	DVS_SRC_28_OCCDV2;
	unsigned int	DVS_SRC_29_OCCDV3;
	unsigned int	DVS_SRC_30_DCV_CONF0;
	unsigned int	DVS_SRC_31_DCV_CONF1;
	unsigned int	DVS_SRC_32_DCV_CONF2;
	unsigned int	DVS_SRC_33_DCV_CONF3;
	unsigned int	DVS_SRC_34_DCV_L_FRM0;
	unsigned int	DVS_SRC_42_OCCDV_EXT0;
	unsigned int	DVS_SRC_43_OCCDV_EXT1;
	unsigned int	DVS_SRC_44_OCCDV_EXT2;
	unsigned int	DVS_SRC_45_OCCDV_EXT3;
	unsigned int	DVS_CRC_OUT_0;
	unsigned int	DVS_CRC_OUT_1;
	unsigned int	DVS_CRC_OUT_2;
	unsigned int	DVS_CRC_OUT_3;
	unsigned int	DVS_PD_SRC_00_L_FRM0;
	unsigned int	DVS_PD_SRC_01_L_FRM1;
	unsigned int	DVS_PD_SRC_02_L_FRM2;
	unsigned int	DVS_PD_SRC_03_L_FRM3;
	unsigned int	DVS_PD_SRC_04_R_FRM0;
	unsigned int	DVS_PD_SRC_05_R_FRM1;
	unsigned int	DVS_PD_SRC_06_R_FRM2;
	unsigned int	DVS_PD_SRC_07_R_FRM3;
	unsigned int	DVS_PD_SRC_08_OCCDV0;
	unsigned int	DVS_PD_SRC_09_OCCDV1;
	unsigned int	DVS_PD_SRC_10_OCCDV2;
	unsigned int	DVS_PD_SRC_11_OCCDV3;
	unsigned int	DVS_PD_SRC_12_OCCDV_EXT0;
	unsigned int	DVS_PD_SRC_13_OCCDV_EXT1;
	unsigned int	DVS_PD_SRC_14_OCCDV_EXT2;
	unsigned int	DVS_PD_SRC_15_OCCDV_EXT3;
	unsigned int	DVS_PD_SRC_16_DCV_CONF0;
	unsigned int	DVS_PD_SRC_17_DCV_CONF1;
	unsigned int	DVS_PD_SRC_18_DCV_CONF2;
	unsigned int	DVS_PD_SRC_19_DCV_CONF3;
	unsigned int	DVS_CTRL_RESERVED;
	unsigned int	DVS_CTRL_ATPG;
	unsigned int	DVS_ME_00;
	unsigned int	DVS_ME_01;
	unsigned int	DVS_ME_02;
	unsigned int	DVS_ME_03;
	unsigned int	DVS_ME_04;
	unsigned int	DVS_ME_05;
	unsigned int	DVS_ME_06;
	unsigned int	DVS_ME_07;
	unsigned int	DVS_ME_08;
	unsigned int	DVS_ME_09;
	unsigned int	DVS_ME_10;
	unsigned int	DVS_ME_11;
	unsigned int	DVS_ME_12;
	unsigned int	DVS_ME_13;
	unsigned int	DVS_ME_14;
	unsigned int	DVS_ME_15;
	unsigned int	DVS_ME_16;
	unsigned int	DVS_ME_17;
	unsigned int	DVS_ME_18;
	unsigned int	DVS_ME_19;
	unsigned int	DVS_ME_20;
	unsigned int	DVS_ME_21;
	unsigned int	DVS_ME_22;
	unsigned int	DVS_ME_23;
	unsigned int	DVS_ME_24;
	unsigned int	DVS_DEBUG;
	unsigned int	DVS_ME_RESERVED;
	unsigned int	DVS_ME_ATPG;
	unsigned int	DVS_OCC_PQ_0;
	unsigned int	DVS_OCC_PQ_1;
	unsigned int	DVS_OCC_PQ_2;
	unsigned int	DVS_OCC_PQ_3;
	unsigned int	DVS_OCC_PQ_4;
	unsigned int	DVS_OCC_PQ_5;
	unsigned int   DVS_OCC_ATPG;

	unsigned int	DVP_CTRL00;
	unsigned int	DVP_CTRL01;
	unsigned int	DVP_CTRL02;
	unsigned int	DVP_CTRL03;
	unsigned int	DVP_CTRL04;
//	unsigned int	DVP_CTRL05;
//	unsigned int	DVP_CTRL06;
	unsigned int	DVP_CTRL07;
	unsigned int	DVP_IRQ_00;
	unsigned int	DVP_CTRL_STATUS0;
	unsigned int	DVP_CTRL_STATUS1;
	unsigned int	DVP_CTRL_STATUS2;
	unsigned int	DVP_IRQ_STATUS;
	unsigned int	DVP_FRM_STATUS0;
//	unsigned int	DVP_FRM_STATUS1;
	unsigned int	DVP_FRM_STATUS2;
//	unsigned int	DVP_FRM_STATUS3;
	unsigned int	DVP_CUR_STATUS;
	unsigned int	DVP_SRC_00;
	unsigned int	DVP_SRC_01;
	unsigned int	DVP_SRC_02;
	unsigned int	DVP_SRC_03;
	unsigned int	DVP_SRC_04;
	unsigned int	DVP_SRC_05_Y_FRM0;
	unsigned int	DVP_SRC_06_Y_FRM1;
	unsigned int	DVP_SRC_07_Y_FRM2;
	unsigned int	DVP_SRC_08_Y_FRM3;
	unsigned int	DVP_SRC_09_C_FRM0;
	unsigned int	DVP_SRC_10_C_FRM1;
	unsigned int	DVP_SRC_11_C_FRM2;
	unsigned int	DVP_SRC_12_C_FRM3;
	unsigned int	DVP_SRC_13_OCCDV0;
	unsigned int	DVP_SRC_14_OCCDV1;
	unsigned int	DVP_SRC_15_OCCDV2;
	unsigned int	DVP_SRC_16_OCCDV3;
	unsigned int	DVP_SRC_17_CRM;
	unsigned int	DVP_SRC_18_ASF_RMDV;
	unsigned int DVP_SRC_19_ASF_RDDV;
	unsigned int DVP_SRC_20_ASF_DV0;
	unsigned int DVP_SRC_21_ASF_DV1;
	unsigned int DVP_SRC_22_ASF_DV2;
	unsigned int DVP_SRC_23_ASF_DV3;
	unsigned int DVP_SRC_24_WMF_HFDV;
	unsigned int DVP_SRC_25_WMF_DV0;
	unsigned int DVP_SRC_26_WMF_DV1;
	unsigned int DVP_SRC_27_WMF_DV2;
	unsigned int DVP_SRC_28_WMF_DV3;
	unsigned int DVP_CORE_00;
	unsigned int DVP_CORE_01;
	unsigned int DVP_CORE_02;
	unsigned int DVP_CORE_03;
	unsigned int DVP_CORE_04;
	unsigned int DVP_CORE_05;
	unsigned int DVP_CORE_06;
	unsigned int DVP_CORE_07;
	unsigned int DVP_CORE_08;
	unsigned int DVP_CORE_09;
	unsigned int DVP_CORE_10;
	unsigned int DVP_CORE_11;
	unsigned int DVP_CORE_12;
	unsigned int DVP_CORE_13;
	unsigned int DVP_CORE_14;
	unsigned int DVP_CORE_15;
	unsigned int DVP_SRC_CTRL;
	unsigned int DVP_CTRL_RESERVED;
	unsigned int DVP_CTRL_ATPG;
	unsigned int DVP_CRC_OUT_0;
	unsigned int DVP_CRC_OUT_1;
	unsigned int DVP_CRC_OUT_2;
	unsigned int DVP_CRC_CTRL;
	unsigned int DVP_CRC_OUT;
	unsigned int DVP_CRC_IN;
	unsigned int DVP_DRAM_STA;
	unsigned int DVP_DRAM_ULT;
	unsigned int DVP_DRAM_PITCH;
	unsigned int DVP_CORE_CRC_IN;
	unsigned int DVP_EXT_SRC_13_OCCDV0;
	unsigned int DVP_EXT_SRC_14_OCCDV1;
	unsigned int DVP_EXT_SRC_15_OCCDV2;
	unsigned int DVP_EXT_SRC_16_OCCDV3;
	unsigned int DVP_EXT_SRC_18_ASF_RMDV;
	unsigned int DVP_EXT_SRC_19_ASF_RDDV;
	unsigned int DVP_EXT_SRC_20_ASF_DV0;
	unsigned int DVP_EXT_SRC_21_ASF_DV1;
	unsigned int DVP_EXT_SRC_22_ASF_DV2;
	unsigned int DVP_EXT_SRC_23_ASF_DV3;
	//unsigned int	USERDUMP_EN;
	unsigned int DPE_MODE;
};

enum DPEMODE {
	MODE_DVS_DVP_BOTH = 0,
	MODE_DVS_ONLY,
	MODE_DVP_ONLY
};

enum DPE_MAINEYE_SEL {
	LEFT = 0,
	RIGHT = 1
};

struct DVS_SubModule_EN {
	bool sbf_en;
	bool conf_en;
	bool occ_en;
};

struct DVP_SubModule_EN {
	bool asf_crm_en;
	bool asf_rm_en;
	bool asf_rd_en;
	bool asf_hf_en;
	bool wmf_hf_en;
	bool wmf_filt_en;
	unsigned int asf_hf_rounds;
	unsigned int asf_nb_rounds;
	unsigned int wmf_filt_rounds;
	bool asf_recursive_en;
};

struct DVS_Iteration {
	unsigned int y_IterTimes;
	unsigned int y_IterStartDirect_0;
	unsigned int y_IterStartDirect_1;
	unsigned int x_IterStartDirect_0;
	unsigned int x_IterStartDirect_1;
};

struct DPE_feedback {
	unsigned int reg1;
	unsigned int reg2;
};

struct DVS_Settings {
	enum DPE_MAINEYE_SEL mainEyeSel;
	struct DVS_ME_CFG TuningBuf_ME;
	struct DVS_OCC_CFG TuningBuf_OCC;
	struct DVS_SubModule_EN SubModule_EN;
	struct DVS_Iteration Iteration;
	bool is_pd_mode;
	unsigned int pd_frame_num; // set by driver
	unsigned int pd_st_x; // set by driver
	unsigned int frmWidth;
	unsigned int frmHeight;
	unsigned int L_engStart_x;
	unsigned int R_engStart_x;
	unsigned int engStart_y;
	unsigned int engWidth;
	unsigned int engHeight;
	unsigned int occWidth;
	unsigned int occStart_x;
	unsigned int pitch;
};

struct DVP_Settings {
	enum DPE_MAINEYE_SEL	mainEyeSel;
	bool Y_only;
	struct DVP_CORE_CFG TuningBuf_CORE;
	struct DVP_SubModule_EN	SubModule_EN;
	bool disp_guide_en;
	unsigned int frmWidth;
	unsigned int frmHeight;
	unsigned int engStart_x;
	unsigned int engStart_y;
	unsigned int engWidth;
	unsigned int engHeight;
};
struct DPE_Config_map {
	unsigned int Dpe_InBuf_SrcImg_YL_fd;
	unsigned int Dpe_InBuf_SrcImg_YL_Ofs;
	unsigned int Dpe_InBuf_SrcImg_YR_fd;
	unsigned int Dpe_InBuf_SrcImg_YR_Ofs;
	unsigned int Dpe_InBuf_SrcImg_Yfd;
	unsigned int Dpe_InBuf_SrcImg_YOfs;
	unsigned int Dpe_InBuf_SrcImg_Cfd;
	unsigned int Dpe_InBuf_SrcImg_COfs;
	unsigned int Dpe_InBuf_ValidMapL_fd;
	unsigned int Dpe_InBuf_ValidMapL_Ofs;
	unsigned int Dpe_InBuf_ValidMapR_fd;
	unsigned int Dpe_InBuf_ValidMapR_Ofs;
	unsigned int Dpe_OutBuf_CONF_fd;
	unsigned int Dpe_OutBuf_CONF_Ofs;
	unsigned int Dpe_OutBuf_OCC_fd;
	unsigned int Dpe_OutBuf_OCC_Ofs;
	unsigned int Dpe_OutBuf_OCCExt_fd;
	unsigned int Dpe_OutBuf_OCCExt_Ofs;
	unsigned int Dpe_InBufOCC_fd;
	unsigned int Dpe_InBufOCC_Ofs;
	unsigned int Dpe_InBuf_OCCExt_fd;
	unsigned int Dpe_InBuf_OCCExt_Ofs;
	unsigned int Dpe_OutBufCRM_fd;
	unsigned int Dpe_OutBufCRM_Ofs;
	unsigned int Dpe_OutBuf_ASFRD_fd;
	unsigned int Dpe_OutBuf_ASFRD_Ofs;
	unsigned int Dpe_OutBuf_ASFRD_Ext_fd;
	unsigned int Dpe_OutBuf_ASFRD_Ext_Ofs;
	unsigned int Dpe_OutBuf_ASFHF_fd;
	unsigned int Dpe_OutBuf_ASFHF_Ofs;
	unsigned int Dpe_OutBuf_ASF_HFExt_fd;
	unsigned int Dpe_OutBuf_ASF_HFExt_Ofs;
	unsigned int Dpe_OutBuf_WMFFILT_fd;
	unsigned int Dpe_OutBuf_WMFFILT_Ofs;
	unsigned int DVS_SRC_21_INTER_MEDV_fd;
	unsigned int DVS_SRC_21_INTER_MEDV_Ofs;
	unsigned int DVS_SRC_34_DCV_L_FRM0_fd;
	unsigned int DVS_SRC_34_DCV_L_FRM0_Ofs;
	unsigned int DVP_SRC_18_ASF_RMDV_fd;
	unsigned int DVP_SRC_18_ASF_RMDV_Ofs;
	unsigned int DVP_SRC_24_WMF_HFDV_fd;
	unsigned int DVP_SRC_24_WMF_HFDV_Ofs;
	unsigned int DVP_EXT_SRC_18_ASF_RMDV_fd;
	unsigned int DVP_EXT_SRC_18_ASF_RMDV_Ofs;
};
struct DPE_Config {
	enum DPEMODE Dpe_engineSelect;
	unsigned int Dpe_is16BitMode;
	struct DVS_Settings	Dpe_DVSSettings;
	struct DVP_Settings	Dpe_DVPSettings;
	struct DPE_Config_map DPE_DMapSettings;
	unsigned int use_fd;
	unsigned int Dpe_InBuf_SrcImg_Y_L;
	unsigned int Dpe_InBuf_SrcImg_Y_R;
	unsigned int Dpe_InBuf_SrcImg_Y;
	unsigned int Dpe_InBuf_SrcImg_C;
	unsigned int Dpe_InBuf_ValidMap_L;
	unsigned int Dpe_InBuf_ValidMap_R;
	unsigned int Dpe_OutBuf_CONF;
	unsigned int Dpe_OutBuf_OCC;
	unsigned int Dpe_OutBuf_OCC_Ext;

	unsigned int Dpe_InBuf_OCC;
	unsigned int Dpe_InBuf_OCC_Ext;
	unsigned int Dpe_OutBuf_CRM;
	//unsigned int Dpe_OutBuf_ASF_RM;
	//unsigned int Dpe_OutBuf_ASF_RM_Ext;
	unsigned int Dpe_OutBuf_ASF_RD;
	unsigned int Dpe_OutBuf_ASF_RD_Ext;
	unsigned int Dpe_OutBuf_ASF_HF;
	unsigned int Dpe_OutBuf_ASF_HF_Ext;
	//MUINT32 Dpe_OutBuf_WMF_HF;
	unsigned int Dpe_OutBuf_WMF_FILT;

	unsigned int DVS_SRC_21_INTER_MEDV;
	unsigned int DVS_SRC_34_DCV_L_FRM0;
	unsigned int DVP_SRC_18_ASF_RMDV;
	unsigned int DVP_SRC_24_WMF_HFDV;
	unsigned int DVP_EXT_SRC_18_ASF_RMDV;
	struct DPE_feedback	Dpe_feedback;
};

/*
 *
 */
enum DPE_CMD_ENUM {
	DPE_CMD_RESET,		/* Reset */
	DPE_CMD_DUMP_REG,	/* Dump DPE Register */
	DPE_CMD_DUMP_ISR_LOG,	/* Dump DPE ISR log */
	DPE_CMD_READ_REG,	/* Read register from driver */
	DPE_CMD_WRITE_REG,	/* Write register to driver */
	DPE_CMD_WAIT_IRQ,	/* Wait IRQ */
	DPE_CMD_CLEAR_IRQ,	/* Clear IRQ */
	DPE_CMD_ENQUE_NUM,	/* DPE Enque Number */
	DPE_CMD_ENQUE,		/* DPE Enque */
	DPE_CMD_ENQUE_REQ,	/* DPE Enque Request */
	DPE_CMD_DEQUE_NUM,	/* DPE Deque Number */
	DPE_CMD_DEQUE,		/* DPE Deque */
	DPE_CMD_DEQUE_REQ,	/* DPE Deque Request */
	DPE_CMD_TOTAL,
};
/*  */

struct DPE_Request {
	unsigned int m_ReqNum;
	struct DPE_Config *m_pDpeConfig;
};

#ifdef CONFIG_COMPAT
struct compat_DPE_REG_IO_STRUCT {
	compat_uptr_t pData;
	unsigned int Count;	/* count */
};

struct compat_DPE_Request {
	unsigned int m_ReqNum;
	compat_uptr_t m_pDpeConfig;
};

#endif

#define DPE_RESET	       _IO(DPE_MAGIC, DPE_CMD_RESET)
#define DPE_DUMP_REG        _IO(DPE_MAGIC, DPE_CMD_DUMP_REG)
#define DPE_DUMP_ISR_LOG    _IO(DPE_MAGIC, DPE_CMD_DUMP_ISR_LOG)

#define DPE_WAIT_IRQ							\
	_IOW(DPE_MAGIC, DPE_CMD_WAIT_IRQ, struct DPE_WAIT_IRQ_STRUCT)
#define DPE_CLEAR_IRQ							\
	_IOW(DPE_MAGIC, DPE_CMD_CLEAR_IRQ, struct DPE_CLEAR_IRQ_STRUCT)

#define DPE_ENQNUE_NUM  _IOW(DPE_MAGIC, DPE_CMD_ENQUE_NUM,    int)
#define DPE_ENQUE      _IOWR(DPE_MAGIC, DPE_CMD_ENQUE,      struct DPE_Config)
#define DPE_ENQUE_REQ  _IOWR(DPE_MAGIC, DPE_CMD_ENQUE_REQ,  struct DPE_Request)

#define DPE_DEQUE_NUM  _IOR(DPE_MAGIC, DPE_CMD_DEQUE_NUM,    int)
#define DPE_DEQUE      _IOWR(DPE_MAGIC, DPE_CMD_DEQUE,      struct DPE_Config)
#define DPE_DEQUE_REQ  _IOWR(DPE_MAGIC, DPE_CMD_DEQUE_REQ,  struct DPE_Request)

#ifdef CONFIG_COMPAT

#define COMPAT_DPE_ENQUE_REQ						\
	_IOWR(DPE_MAGIC, DPE_CMD_ENQUE_REQ, struct compat_DPE_Request)
#define COMPAT_DPE_DEQUE_REQ						\
	_IOWR(DPE_MAGIC, DPE_CMD_DEQUE_REQ, struct compat_DPE_Request)
#endif

/*  */
#endif
