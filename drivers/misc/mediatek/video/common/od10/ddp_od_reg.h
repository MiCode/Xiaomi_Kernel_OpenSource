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

#ifndef __DDP_OD_REG_H__
#define __DDP_OD_REG_H__

#include <ddp_reg.h>

#define OD_BASE      DISPSYS_OD_BASE /* 0x14023000 */

/* Page OD */
#define OD_REG00 (OD_BASE + 0x0700)
#define BYPASS_ALL           REG_FLD(1, 31) /* [31:31] */
#define OD_CLK_INV           REG_FLD(1, 30) /* [30:30] */
#define OD_CLK_EN            REG_FLD(1, 29) /* [29:29] */
#define DC_CNT               REG_FLD(5, 24) /* [28:24] */
#define BTC_ERR_CNT          REG_FLD(5, 16) /* [20:16] */
#define C_GAT_CNT            REG_FLD(5, 8)  /* [12:8] */
#define RG_Y5_MODE           REG_FLD(1, 6)  /* [6:6] */
#define UVRUD_DISA           REG_FLD(1, 5)  /* [5:5] */
#define OD_8BIT_SEL          REG_FLD(1, 4)  /* [4:4] */
#define OD_ENABLE            REG_FLD(1, 0)  /* [0:0] */

#define OD_REG01 (OD_BASE + 0x0704)
#define OD_MONSEL            REG_FLD(4, 28) /* [31:28] */
#define PKT_DBG              REG_FLD(1, 25) /* [25:25] */
#define OD_RST               REG_FLD(1, 24) /* [24:24] */
#define DMA_RD_MODE          REG_FLD(1, 20) /* [20:20] */
#define DMA_RD_THR           REG_FLD(4, 16) /* [19:16] */
#define ALBUF2_DLY           REG_FLD(5, 8)  /* [12:8] */
#define MOTION_THR           REG_FLD(8, 0)  /* [7:0] */

#define OD_REG02 (OD_BASE + 0x0708)
#define DISABLE_8B_BTC       REG_FLD(1, 31) /* [31:31] */
#define DISABLE_8B_DC        REG_FLD(1, 30) /* [30:30] */
#define FORCE_8B_BTC         REG_FLD(1, 29) /* [29:29] */
#define FORCE_8B_DC          REG_FLD(1, 28) /* [28:28] */
#define OD255_FIX            REG_FLD(1, 22) /* [22:22] */
#define UV_MODE_MASK         REG_FLD(3, 19) /* [21:19] */
#define Y_MODE_MASK          REG_FLD(3, 16) /* [18:16] */
#define DEMO_MODE            REG_FLD(1, 14) /* [14:14] */
#define FORCE_FPIN_RED       REG_FLD(1, 13) /* [13:13] */
#define DISPLAY_COMPRESSION  REG_FLD(1, 12) /* [12:12] */
#define FORCE_ENABLE_OD_MUX  REG_FLD(1, 11) /* [11:11] */
#define FBT_BYPASS           REG_FLD(1, 10) /* [10:10] */
#define ODT_BYPASS           REG_FLD(1, 9)  /* [9:9] */

#define OD_REG03 (OD_BASE + 0x070C)
#define INK_B_VALUE          REG_FLD(8, 24) /* [31:24] */
#define INK_G_VALUE          REG_FLD(8, 16) /* [23:16] */
#define INK_R_VALUE          REG_FLD(8, 8)  /* [15:8] */
#define CR_INK_SEL           REG_FLD(3, 2)  /* [4:2] */
#define CR_INK               REG_FLD(1, 1)  /* [1:1] */
#define ODT_INK_EN           REG_FLD(1, 0)  /* [0:0] */

#define OD_REG04 (OD_BASE + 0x0710)
#define TABLE_RW_SEL_PCIDB    REG_FLD(1, 27)  /* [27:27] */
#define TABLE_RW_SEL_PCIDG    REG_FLD(1, 26)  /* [26:26] */
#define TABLE_RW_SEL_PCIDR    REG_FLD(1, 25)  /* [25:25] */
#define TABLE_RW_SEL_PCID_BGR REG_FLD(3, 25)  /* [27:25] */
#define TABLE_ONLY_W_ADR_INC  REG_FLD(1, 23)  /* [23:23] */
#define TABLE_R_SEL_FB_EVEN   REG_FLD(1, 22)  /* [22:22] */
#define TABLE_RW_SEL_OD_B     REG_FLD(1, 21)  /* [21:21] */
#define TABLE_RW_SEL_OD_G     REG_FLD(1, 20)  /* [20:20] */
#define TABLE_RW_SEL_OD_R     REG_FLD(1, 19)  /* [19:19] */
#define TABLE_RW_SEL_OD_BGR   REG_FLD(3, 19)  /* [21:19] */
#define TABLE_RW_SEL_FB_B     REG_FLD(1, 18)  /* [18:18] */
#define TABLE_RW_SEL_FB_G     REG_FLD(1, 17)  /* [17:17] */
#define TABLE_RW_SEL_FB_R     REG_FLD(1, 16)  /* [16:16] */
#define TABLE_RW_SEL_FB_BGR   REG_FLD(3, 16)  /* [18:16] */
#define ADDR_Y                REG_FLD(6, 8)   /* [13:8] */
#define ADDR_X                REG_FLD(6, 0)   /* [5:0] */
#define ADDR_YX               REG_FLD(14, 0)  /* [13:0] */

#define OD_REG05 (OD_BASE + 0x0714)
#define TABLE_RW_DATA         REG_FLD(8, 0)  /* [7:0] */

#define OD_REG06 (OD_BASE + 0x0718)
#define RG_BASE_ADR           REG_FLD(28, 0) /* [27:0] */

#define OD_REG07 (OD_BASE + 0x071C)
#define VALIDTH               REG_FLD(8, 20) /* [27:20] */
#define DEBUG_SEL             REG_FLD(3, 17) /* [19:17] */
#define RTHRE                 REG_FLD(8, 9)  /* [16:9] */
#define SWRESET               REG_FLD(1, 8)  /* [8:8] */
#define WTHRESH               REG_FLD(8, 0)  /* [7:0] */

#define OD_REG08 (OD_BASE + 0x0720)
#define OD_H_ACTIVE           REG_FLD(12, 0) /* [11:0] */
#define OD_V_ACTIVE           REG_FLD(12, 16)/* [27:16] */

#define OD_REG09 (OD_BASE + 0x0724)
#define RG_H_BLANK           REG_FLD(12, 0)  /*[11:0] */
#define RG_H_OFFSET          REG_FLD(12, 16) /*[27:16] */

#define OD_REG10 (OD_BASE + 0x0728)
#define RG_H_BLANK_MAX       REG_FLD(12, 0)  /*[11:0] */
#define RG_V_BLANK_MAX       REG_FLD(20, 12) /*[31:12] */

#define OD_REG11 (OD_BASE + 0x072C)
#define RG_V_BLANK           REG_FLD(18, 0)  /* [17:0] */
#define RG_FRAME_SET         REG_FLD(4, 18)  /* [21:18] */

#define OD_REG12 (OD_BASE + 0x0730)
#define RG_OD_START          REG_FLD(1, 0)   /* [0:0] */
#define RG_OD_DRAM_MSB       REG_FLD(6, 1)   /* [6:1] */
#define RG_PAT_START         REG_FLD(1, 7)   /* [7:7] */

#define OD_REG13 (OD_BASE + 0x0734)
#define DMA_WDATA_MON        REG_FLD(32, 0)/* [31:0] */

#define OD_REG14 (OD_BASE + 0x0738)
#define RD_DBG              REG_FLD(8, 24) /* [31:24] */
#define RD_ADR              REG_FLD(18, 0) /* [17:0] */

#define OD_REG15 (OD_BASE + 0x073C)
#define WR_DBG              REG_FLD(8, 24) /* [31:24] */
#define WR_ADR              REG_FLD(18, 0) /* [17:0] */

#define OD_REG16 (OD_BASE + 0x0740)
#define DMA_RDATA0          REG_FLD(32, 0) /* [31:0] */

#define OD_REG17 (OD_BASE + 0x0744)
#define DMA_RDATA1          REG_FLD(32, 0)/* [31:0] */

#define OD_REG18 (OD_BASE + 0x0748)
#define DMA_RDATA2          REG_FLD(32, 0)/* [31:0] */

#define OD_REG19 (OD_BASE + 0x074C)
#define DMA_RDATA3          REG_FLD(32, 0)/* [31:0] */

#define OD_REG20 (OD_BASE + 0x0750)
#define DMA_WR_CNT          REG_FLD(32, 0)/* [31:0] */

#define OD_REG21 (OD_BASE + 0x0754)
#define RG_PAT_H_START       REG_FLD(12, 0)/* [11:0] */
#define RG_PAT_V_START       REG_FLD(12, 16)/* [27:16] */

#define OD_REG22 (OD_BASE + 0x0758)
#define RG_PAT_H_OFFSET      REG_FLD(12, 0)/* [11:0] */
#define RG_PAT_V_OFFSET      REG_FLD(12, 16)/* [27:16] */

#define OD_REG23 (OD_BASE + 0x075C)
#define RG_PAT_LENGTH        REG_FLD(12, 0)/* [11:0] */
#define RG_PAT_WIDTH         REG_FLD(12, 16)/* [27:16] */

#define OD_REG24 (OD_BASE + 0x0760)
#define RG_PAT_YIN0          REG_FLD(10, 0)/* [9:0] */
#define RG_PAT_CIN0          REG_FLD(10, 10)/* [19:10] */
#define RG_PAT_VIN0          REG_FLD(10, 20)/* [29:20] */

#define OD_REG25 (OD_BASE + 0x0764)
#define RG_PAT_YIN1          REG_FLD(10, 0)/* [9:0] */
#define RG_PAT_CIN1          REG_FLD(10, 10)/* [19:10] */
#define RG_PAT_VIN1          REG_FLD(10, 20)/* [29:20] */

#define OD_REG26 (OD_BASE + 0x0768)
#define RG_AGENT_FREQ        REG_FLD(9, 0)/*[8:0] */
#define RG_BLACK_AGENT       REG_FLD(1, 9)/*[9:9] */
#define RG_NO_BLACK          REG_FLD(1, 10)/*[10:10] */
#define RG_BLACK_PAT         REG_FLD(10, 16)/*[25:16] */

#define OD_REG28 (OD_BASE + 0x0770)
#define RG_TABLE_DMA_ADR_ST  REG_FLD(27, 0)/*[26:0] */

#define OD_REG29 (OD_BASE + 0x0774)
#define RG_TABLE_DMA_EN            REG_FLD(1, 0)/*[0:0] */
#define RG_TABLE_RGB_SEQ           REG_FLD(1, 1)/*[1:1] */
#define RG_TABLE_DMA_DONE_CLR      REG_FLD(1, 2)/*[2:2] */
#define RG_ODT_SIZE                REG_FLD(2, 3)/*[4:3] */

#define OD_REG30 (OD_BASE + 0x0778)
#define MANU_CPR                   REG_FLD(8, 0)/*[7:0] */

#define OD_REG31 (OD_BASE + 0x077C)
#define SYNC_V_EDGE                REG_FLD(1, 4)/*[4:4] */
#define SYNC_V_SRC                 REG_FLD(1, 5)/*[5:5] */
#define OD_H_DELAY                 REG_FLD(6, 6)/*[11:6] */
#define OD_V_DELAY                 REG_FLD(6, 12)/*[17:12] */
#define HI_POL                     REG_FLD(1, 18)/*[18:18] */
#define VI_POL                     REG_FLD(1, 19)/*[19:19] */
#define HO_POL                     REG_FLD(1, 20)/*[20:20] */
#define VO_POL                     REG_FLD(1, 21)/*[21:21] */
#define FORCE_INT                  REG_FLD(1, 22)/*[22:22] */
#define OD_SYNC_FEND               REG_FLD(1, 23)/*[23:23] */
#define OD_SYNC_POS                REG_FLD(8, 24)/*[31:24] */

#define OD_REG32 (OD_BASE + 0x0788)
#define OD_EXT_FP_EN               REG_FLD(1, 0)/*[0:0] */
#define OD_USE_EXT_DE_TOTAL        REG_FLD(1, 1)/*[1:1] */
#define OD255_FIX2_SEL             REG_FLD(2, 2)/*[3:2] */
#define FBT_BYPASS_FREQ            REG_FLD(4, 4)/*[7:4] */
#define OD_DE_WIDTH                REG_FLD(13, 8)/*[20:8] */
#define OD_LNR_DISABLE             REG_FLD(1, 21)/*[21:21] */
#define OD_IDX_17                  REG_FLD(1, 23)/*[23:23] */
#define OD_IDX_41                  REG_FLD(1, 24)/*[24:24] */
#define OD_IDX_41_SEL              REG_FLD(1, 25)/*[25:25] */
#define MERGE_RGB_LUT_EN           REG_FLD(1, 26)/*[26:26] */
#define OD_RDY_SYNC_V              REG_FLD(1, 27)/*[27:27] */
#define OD_CRC_START               REG_FLD(1, 28)/*[28:28] */
#define OD_CRC_CLR                 REG_FLD(1, 29)/*[29:29] */
#define OD_CRC_SEL                 REG_FLD(2, 30)/*[31:30] */

#define OD_REG33 (OD_BASE + 0x078C)
#define FORCE_Y_MODE               REG_FLD(4, 0)/*[3:0] */
#define FORCE_C_MODE               REG_FLD(4, 8)/*[11:8] */
#define ODR_DM_REQ_EN              REG_FLD(1, 12)/*[12:12] */
#define ODR_DM_TIM                 REG_FLD(4, 13)/*[16:13] */
#define ODW_DM_REQ_EN              REG_FLD(1, 17)/*[17:17] */
#define ODW_DM_TIM                 REG_FLD(4, 18)/*[21:18] */
#define DMA_DRAM_REQ_EN            REG_FLD(1, 22)/*[22:22] */
#define DMA_DRAM_TIM               REG_FLD(4, 23)/*[26:23] */

#define OD_REG34 (OD_BASE + 0x0790)
#define ODT_SB_TH0                 REG_FLD(5, 0)/*[4:0] */
#define ODT_SB_TH1                 REG_FLD(5, 5)/*[9:5] */
#define ODT_SB_TH2                 REG_FLD(5, 10)/*[14:10] */
#define ODT_SB_TH3                 REG_FLD(5, 15)/*[19:15] */
#define ODT_SOFT_BLEND_EN          REG_FLD(1, 20)/*[20:20] */
#define DET8B_BLK_NBW              REG_FLD(11, 21)/*[31:21] */

#define OD_REG35 (OD_BASE + 0x0794)
#define DET8B_DC_NUM               REG_FLD(18, 0)/*[17:0] */
#define DET8B_DC_THR               REG_FLD(3, 18)/*[20:18] */

#define OD_REG36 (OD_BASE + 0x0798)
#define DET8B_BTC_NUM              REG_FLD(18, 0)/*[17:0] */
#define DET8B_BTC_THR              REG_FLD(3, 18)/*[20:18] */
#define DET8B_SYNC_POS             REG_FLD(8, 21)/*[28:21] */
#define DET8B_HYST                 REG_FLD(3, 29)/*[31:29] */

#define OD_REG37 (OD_BASE + 0x079C)
#define ODT_MAX_RATIO              REG_FLD(4, 0)/*[3:0] */
#define DET8B_BIT_MGN              REG_FLD(20, 6)/*[25:6] */
#define DET8B_DC_OV_ALL            REG_FLD(1, 26)/*[26:26] */

#define OD_REG38 (OD_BASE + 0x07A0)
#define WR_BURST_LEN               REG_FLD(2, 0)/* [1:0] */
#define WR_PAUSE_LEN               REG_FLD(8, 2)/* [9:2] */
#define WFF_EMP_OPT                REG_FLD(1, 10)/* [10:10] */
#define RD_BURST_LEN               REG_FLD(2, 11)/* [12:11] */
#define LINE_SIZE                  REG_FLD(9, 13)/* [21:13] */
#define DRAM_CRC_CLR               REG_FLD(1, 22)/* [22:22] */
#define RD_PAUSE_LEN               REG_FLD(8, 23)/* [30:23] */

#define OD_REG39 (OD_BASE + 0x07A4)
#define OD_PAGE_MASK               REG_FLD(16, 0)/* [15:0] */
#define DRAM_CRC_CNT               REG_FLD(9, 16)/* [24:16] */
#define WDRAM_ZERO                 REG_FLD(1, 25)/* [25:25] */
#define WDRAM_FF                   REG_FLD(1, 26)/* [26:26] */
#define RDRAM_LEN_X4               REG_FLD(1, 27)/* [27:27] */
#define WDRAM_DIS                  REG_FLD(1, 28)/* [28:28] */
#define RDRAM_DIS                  REG_FLD(1, 29)/* [29:29] */
#define W_CHDEC_RST                REG_FLD(1, 30)/* [30:30] */
#define R_CHDEC_RST                REG_FLD(1, 31)/* [31:31] */

#define OD_REG40 (OD_BASE + 0x07A8)
#define GM_FORCE_VEC               REG_FLD(3, 0)  /*[2:0] */
#define GM_VEC_RST                 REG_FLD(1, 3)  /*[3:3] */
#define GM_EN                      REG_FLD(1, 4)  /*[4:4] */
#define GM_FORCE_EN                REG_FLD(1, 5)  /*[5:5] */
#define GM_AUTO_SHIFT              REG_FLD(1, 6)  /*[6:6] */
#define REP22_0                    REG_FLD(2, 7)  /*[8:7] */
#define REP22_1                    REG_FLD(2, 9)  /*[10:9] */
#define GM_R0_CENTER               REG_FLD(7, 11) /*[17:11] */
#define REP22_2                    REG_FLD(2, 18) /*[19:18] */
#define REP22_3                    REG_FLD(2, 20) /*[21:20] */
#define GM_R1_CENTER               REG_FLD(7, 22) /*[28:22] */
#define GM_TRACK_SEL               REG_FLD(1, 29) /*[29:29] */

#define OD_REG41 (OD_BASE + 0x07AC)
#define REP22_4                    REG_FLD(2, 0)  /*[1:0] */
#define REP22_5                    REG_FLD(2, 2)  /*[3:2] */
#define GM_R2_CENTER               REG_FLD(7, 4)  /*[10:4] */
#define REP22_6                    REG_FLD(2, 11) /*[12:11] */
#define REP22_7                    REG_FLD(2, 13) /*[14:13] */
#define GM_R3_CENTER               REG_FLD(7, 15) /*[21:15] */

#define OD_REG42 (OD_BASE + 0x07B0)
#define REP32_0                    REG_FLD(2, 0)  /*[1:0] */
#define REP32_1                    REG_FLD(2, 2)  /*[3:2] */
#define GM_R4_CENTER               REG_FLD(7, 4)  /*[10:4] */
#define GM_HYST_SEL                REG_FLD(4, 11) /*[14:11] */
#define GM_LGMIN_DIFF              REG_FLD(12, 16)/*[27:16] */
#define REP32_6                    REG_FLD(2, 28) /*[29:28] */
#define REP32_7                    REG_FLD(2, 30) /*[31:30] */

#define OD_REG43 (OD_BASE + 0x07B4)
#define GM_REP_MODE_DET            REG_FLD(1, 0)/* [0:0] */
#define RPT_MODE_EN                REG_FLD(1, 1)/* [1:1] */
#define GM_V_ST                    REG_FLD(9, 2)/* [10:2] */
#define RPT_MODE_HYST              REG_FLD(2, 11)/* [12:11] */
#define GM_V_END                   REG_FLD(9, 13)/* [21:13] */

#define OD_REG44 (OD_BASE + 0x07B8)
#define GM_LMIN_THR                REG_FLD(12, 0)/* [11:0] */
#define REP32_2                    REG_FLD(2, 12)/* [13:12] */
#define REP32_3                    REG_FLD(2, 14)/* [15:14] */
#define GM_GMIN_THR                REG_FLD(12, 16)/* [27:16] */
#define REP32_4                    REG_FLD(2, 28)/* [29:28] */
#define REP32_5                    REG_FLD(2, 30)/* [31:30] */

#define OD_REG45 (OD_BASE + 0x07BC)
#define REPEAT_HALF_SHIFT          REG_FLD(1, 0)/* [0:0] */
#define REPEAT_MODE_SEL            REG_FLD(1, 1)/* [1:1] */
#define GM_CENTER_OFFSET           REG_FLD(6, 2)/* [7:2] */
#define DET422_HYST                REG_FLD(2, 8)/* [9:8] */
#define DET422_FORCE               REG_FLD(1, 10)/* [10:10] */
#define DET422_EN                  REG_FLD(1, 11)/* [11:11] */
#define FORCE_RPT_MOTION           REG_FLD(1, 12)/* [12:12] */
#define FORCE_RPT_SEQ              REG_FLD(1, 13)/* [13:13] */
#define FORCE_32                   REG_FLD(1, 14)/* [14:14] */
#define FORCE_22                   REG_FLD(1, 15)/* [15:15] */
#define OD_PCID_ALIG_SEL           REG_FLD(1, 16)/* [16:16] */
#define OD_PCID_ALIG_SEL2          REG_FLD(1, 17)/* [17:17] */
#define OD_PCID_EN                 REG_FLD(1, 18)/* [18:18] */
#define OD_PCID_CSB                REG_FLD(1, 19)/* [19:19] */
#define OD_PCID255_FIX             REG_FLD(1, 20)/* [20:20] */
#define OD_PCID_BYPASS             REG_FLD(1, 21)/* [21:21] */
#define OD_PCID_SWAP_LINE          REG_FLD(1, 22)/* [22:22] */
#define MON_DATA_SEL               REG_FLD(3, 24)/* [26:24] */
#define MON_TIM_SEL                REG_FLD(4, 27)/* [30:27] */

#define OD_REG46 (OD_BASE + 0x07C0)
#define AUTO_Y5_MODE               REG_FLD(1, 0)/* [0:0] */
#define AUTO_Y5_HYST               REG_FLD(4, 1)/* [4:1] */
#define AUTO_Y5_SEL                REG_FLD(2, 5)/* [6:5] */
#define NO_MOTION_DET              REG_FLD(1, 8)/* [8:8] */
#define AUTO_Y5_NO_8B              REG_FLD(1, 9)/* [9:9] */
#define OD_OSD_SEL                 REG_FLD(3, 12)/* [14:12] */
 #define OD_OSD_LINE_EN             REG_FLD(1, 15)/* [15:15] */
#define AUTO_Y5_NUM                REG_FLD(16, 16)/* [31:16] */

#define OD_REG47 (OD_BASE + 0x07C4)
#define ODT_SB_TH4                 REG_FLD(5, 0)/* [4:0] */
#define ODT_SB_TH5                 REG_FLD(5, 5)/* [9:5] */
#define ODT_SB_TH6                 REG_FLD(5, 10)/* [14:10] */
#define ODT_SB_TH7                 REG_FLD(5, 15)/* [19:15] */
#define WOV_CLR                    REG_FLD(1, 20)/* [20:20] */
#define ROV_CLR                    REG_FLD(1, 21)/* [21:21] */
#define SB_INDV_ALPHA              REG_FLD(1, 22)/* [22:22] */
#define PRE_BW                     REG_FLD(6, 24)/* [29:24] */
#define ABTC_POST_FIX              REG_FLD(1, 30)/* [30:30] */
#define OD255_FIX2                 REG_FLD(1, 31)/* [31:31] */

#define OD_REG48 (OD_BASE + 0x07C8)
#define ODT_INDIFF_TH              REG_FLD(8, 0)/* [7:0] */
#define FBT_INDIFF_TH              REG_FLD(8, 8)/* [15:8] */
#define FP_RST_DISABLE             REG_FLD(1, 16)/* [16:16] */
#define FP_POST_RST_DISABLE        REG_FLD(1, 17)/* [17:17] */
#define FP_BYPASS_BLOCK            REG_FLD(1, 18)/* [18:18] */
#define ODT_CSB                    REG_FLD(1, 19)/* [19:19] */
#define FBT_CSB                    REG_FLD(1, 20)/* [20:20] */
#define ODT_FORCE_EN               REG_FLD(1, 22)/* [22:22] */
#define BLOCK_STA_SEL              REG_FLD(4, 23)/* [26:23] */
#define RDY_DELAY_1F               REG_FLD(1, 27)/* [27:27] */
#define HEDGE_SEL                  REG_FLD(1, 28)/* [28:28] */
#define ODT_255_TO_1023            REG_FLD(1, 29)/* [29:29] */
#define NO_RD_FIRST_BYPASS         REG_FLD(1, 30)/* [30:30] */

#define OD_REG49 (OD_BASE + 0x07CC)
#define ASYNC_ECO_DISABLE          REG_FLD(2, 0)/* [1:0] */
#define RDRAM_MODEL                REG_FLD(1, 14)/* [14:14] */
#define DE_PROTECT_EN              REG_FLD(1, 15)/* [15:15] */
#define VDE_PROTECT_EN             REG_FLD(1, 16)/* [16:16] */
#define INT_FP_MON_DE              REG_FLD(1, 17)/* [17:17] */
#define TABLE_MODEL                REG_FLD(1, 18)/* [18:18] */
#define STA_INT_WAIT_HEDGE         REG_FLD(1, 19)/* [19:19] */
#define STA_INT_WAIT_VEDGE         REG_FLD(1, 20)/* [20:20] */
#define ASYNC_OPT                  REG_FLD(1, 21)/* [21:21] */
#define LINE_BUF_AUTO_CLR          REG_FLD(1, 22)/* [22:22] */
#define FIX_INSERT_DE              REG_FLD(1, 23)/* [23:23] */
#define TOGGLE_DE_ERROR            REG_FLD(1, 25)/* [25:25] */
#define SM_ERR_RST_EN              REG_FLD(1, 26)/* [26:26] */
#define ODT_BYPASS_FSYNC           REG_FLD(1, 27)/* [27:27] */
#define FBT_BYPASS_FSYNC           REG_FLD(1, 28)/* [28:28] */
#define PCLK_EN                    REG_FLD(1, 30)/* [30:30] */
#define MCLK_EN                    REG_FLD(1, 31)/* [31:31] */

#define OD_REG50 (OD_BASE + 0x07D0)
#define DUMP_STADR_A               REG_FLD(16, 0)/* [15:0] */
#define DUMP_STADR_B               REG_FLD(16, 16)/* [31:16] */

#define OD_REG51 (OD_BASE + 0x07D4)
#define DUMP_STLINE                REG_FLD(11, 0)/* [10:0] */
#define DUMP_ENDLINE               REG_FLD(11, 11)/* [21:11] */
#define DUMP_DRAM_EN               REG_FLD(1, 22)/* [22:22] */
#define DUMP_BURST_LEN             REG_FLD(2, 23)/* [24:23] */
#define DUMP_OV_CLR                REG_FLD(1, 25)/* [25:25] */
#define DUMP_UD_CLR                REG_FLD(1, 26)/* [26:26] */
#define DUMP_12B_EXT               REG_FLD(2, 27)/* [28:27] */
#define DUMP_ONCE                  REG_FLD(1, 29)/* [29:29] */

#define OD_REG52 (OD_BASE + 0x07D8)
#define LINE_END_DLY               REG_FLD(2, 0)/* [1:0] */
#define AUTO_DET_CSKIP             REG_FLD(1, 2)/* [2:2] */
#define SKIP_COLOR_MODE_EN         REG_FLD(1, 3)/* [3:3] */
#define SKIP_COLOR_HYST            REG_FLD(4, 4)/* [7:4] */
#define DUMP_FIFO_DEPTH            REG_FLD(10, 8)/* [17:8] */
#define DUMP_FIFO_LAST_ADDR        REG_FLD(9, 18)/* [26:18] */
#define DUMP_FSYNC_SEL             REG_FLD(2, 27)/* [28:27] */
#define DUMP_WFF_FULL_CONF         REG_FLD(3, 29)/* [31:29] */

#define OD_REG53 (OD_BASE + 0x07DC)
#define AUTO_Y5_NUM_1              REG_FLD(16, 0)/* [15:0] */
#define FRAME_ERR_CON              REG_FLD(12, 16)/* [27:16] */
#define FP_ERR_STA_CLR             REG_FLD(1, 28)/* [28:28] */
#define FP_X_H                     REG_FLD(1, 29)/* [29:29] */
#define OD_START_SYNC_V            REG_FLD(1, 30)/* [30:30] */
#define OD_EN_SYNC_V               REG_FLD(1, 31)/* [31:31] */

#define OD_REG54 (OD_BASE + 0x07E0)
#define DET8B_BIT_MGN2             REG_FLD(20, 0)/* [19:0] */
#define BYPASS_ALL_SYNC_V          REG_FLD(1, 20)/* [20:20] */
#define OD_INT_MASK                REG_FLD(5, 21)/* [25:21] */
#define OD_STA_INT_CLR             REG_FLD(5, 26)/* [30:26] */
#define OD_NEW_YUV2RGB             REG_FLD(1, 31)/* [31:31] */

#define OD_REG55 (OD_BASE + 0x07E4)
#define OD_ECP_WD_RATIO            REG_FLD(10, 0)/* [9:0] */
#define OD_ECP_THR_HIG             REG_FLD(10, 10)/* [19:10] */
#define OD_ECP_THR_LOW             REG_FLD(10, 20)/* [29:20] */
#define OD_ECP_SEL                 REG_FLD(2, 30)/* [31:30] */

#define OD_REG56 (OD_BASE + 0x07E8)
#define DRAM_UPBOUND               REG_FLD(28, 0)/* [27:0] */
#define DRAM_PROT                  REG_FLD(1, 28)/* [28:28] */
#define OD_TRI_INTERP              REG_FLD(1, 29)/* [29:29] */
#define OD_ECP_EN                  REG_FLD(1, 30)/* [30:30] */
#define OD_ECP_ALL_ON              REG_FLD(1, 31)/* [31:31] */

#define OD_REG57 (OD_BASE + 0x07EC)
#define SKIP_COLOR_THR                REG_FLD(16, 0)/* [15:0] */
#define SKIP_COLOR_THR_1              REG_FLD(16, 16)/* [31:16] */

#define OD_REG62 (OD_BASE + 0x05C0)
#define RG_2CH_PTGEN_COLOR_BAR_TH     REG_FLD(12, 0)/* [11:0] */
#define RG_2CH_PTGEN_TYPE             REG_FLD(3, 12)/* [14:12] */
#define RG_2CH_PTGEN_START            REG_FLD(1, 15)/* [15:15] */
#define RG_2CH_PTGEN_HMOTION          REG_FLD(8, 16)/* [23:16] */
#define RG_2CH_PTGEN_VMOTION          REG_FLD(8, 24)/* [31:24] */

#define OD_REG63 (OD_BASE + 0x05C4)
#define RG_2CH_PTGEN_H_TOTAL          REG_FLD(13, 0)/* [12:0] */
#define RG_2CH_PTGEN_MIRROR           REG_FLD(1, 13)/* [13:13] */
#define RG_2CH_PTGEN_SEQ              REG_FLD(1, 14)/* [14:14] */
#define RG_2CH_PTGEN_2CH_EN           REG_FLD(1, 15)/* [15:15] */
#define RG_2CH_PTGEN_H_ACTIVE         REG_FLD(13, 16)/* [28:16] */
#define RG_2CH_PTGEN_OD_COLOR         REG_FLD(1, 29)/* [29:29] */

#define OD_REG64 (OD_BASE + 0x05C8)
#define RG_2CH_PTGEN_V_TOTAL          REG_FLD(12, 0)/* [11:0] */
#define RG_2CH_PTGEN_V_ACTIVE         REG_FLD(12, 12)/* [23:12] */

#define OD_REG65 (OD_BASE + 0x05CC)
#define RG_2CH_PTGEN_H_START          REG_FLD(13, 0)/* [12:0] */
#define RG_2CH_PTGEN_H_WIDTH          REG_FLD(13, 16)/* [28:16] */

#define OD_REG66 (OD_BASE + 0x05D0)
#define RG_2CH_PTGEN_V_START          REG_FLD(12, 0)/* [11:0] */
#define RG_2CH_PTGEN_V_WIDTH          REG_FLD(12, 12)/* [23:12] */

#define OD_REG67 (OD_BASE + 0x05D4)
#define RG_2CH_PTGEN_B                REG_FLD(10, 0)/* [9:0] */
#define RG_2CH_PTGEN_G                REG_FLD(10, 10)/* [19:10] */
#define RG_2CH_PTGEN_R                REG_FLD(10, 20)/* [29:20] */

#define OD_REG68 (OD_BASE + 0x05D8)
#define RG_2CH_PTGEN_B_BG             REG_FLD(10, 0)/* [9:0] */
#define RG_2CH_PTGEN_G_BG             REG_FLD(10, 10)/* [19:10] */
#define RG_2CH_PTGEN_R_BG             REG_FLD(10, 20)/* [29:20] */

#define OD_REG69 (OD_BASE + 0x05DC)
#define RG_2CH_PTGEN_H_BLOCK_WIDTH    REG_FLD(10, 0)/* [9:0] */
#define RG_2CH_PTGEN_V_BLOCK_WIDTH    REG_FLD(10, 10)/* [19:10] */

#define OD_REG70 (OD_BASE + 0x05E0)
#define RG_2CH_PTGEN_H_BLOCK_OFFSET   REG_FLD(13, 0)/* [12:0] */
#define RG_2CH_PTGEN_V_BLOCK_OFFSET   REG_FLD(12, 16)/* [27:16] */
#define RG_2CH_PTGEN_DIR              REG_FLD(1, 29)/* [29:29] */

#define OD_REG71 (OD_BASE + 0x05E4)
#define RG_WR_HIGH					REG_FLD(6, 0)
						/* [0:5] */
#define RG_WR_PRE_HIGH				REG_FLD(6, 6) /* [6:11] */
#define RG_WRULTRA_EN				REG_FLD(1, 12)/* [12:12] */
#define RG_WR_LOW					REG_FLD(6, 16)
					/* [16:21] */
#define RG_WR_PRELOW				REG_FLD(6, 22)/* [22:27] */
#define RG_WGPREULTRA_EN              REG_FLD(1, 28)/* [28:28] */
#define RG_WDRAM_HOLD_EN              REG_FLD(1, 29)/* [29:29] */
#define RG_WDRAM_LEN_X8               REG_FLD(1, 30)/* [30:30] */

#define OD_REG72 (OD_BASE + 0x05E8)
#define RG_RD_HIGH					REG_FLD(6, 0)
							/* [0:5] */
#define RG_RD_PRE_HIGH				REG_FLD(6, 6) /* [6:11] */
#define RG_RDULTRA_EN				REG_FLD(1, 12)/* [12:12] */
#define RG_RD_LOW					REG_FLD(6, 16)
					/* [16:21] */
#define RG_RD_PRELOW				REG_FLD(6, 22)/* [22:27] */
#define RG_RGPREULTRA_EN              REG_FLD(1, 28)/* [28:28] */
#define RG_RDRAM_HOLD_EN              REG_FLD(1, 29)/* [29:29] */
#define RG_RDRAM_LEN_X8               REG_FLD(1, 30)/* [30:30] */

#define OD_REG73 (OD_BASE + 0x05EC)
#define RG_WDRAM_HOLD_THR             REG_FLD(9, 0)/* [8:0] */
#define RG_RDRAM_HOLD_THR             REG_FLD(12, 16)/* [27:16] */

#define OD_REG74 (OD_BASE + 0x05F0)
#define OD_REG75 (OD_BASE + 0x05F4)

#define OD_REG76 (OD_BASE + 0x05F8)
#define CHG_Q_FREQ                    REG_FLD(2, 0)/* [1:0] */
#define IP_BTC_ERROR_CNT              REG_FLD(6, 2)/* [7:2] */
#define CURR_Q_UV                     REG_FLD(3, 9)/* [11:9] */
#define CURR_Q_BYPASS                 REG_FLD(3, 12)/* [14:12] */
#define RC_Y_SEL                      REG_FLD(1, 15)/* [15:15] */
#define RC_C_SEL                      REG_FLD(1, 16)/* [16:16] */
#define DUMMY                         REG_FLD(1, 17)/* [17:17] */
#define CURR_Q_UB                     REG_FLD(3, 18)/* [20:18] */
#define CURR_Q_LB                     REG_FLD(3, 21)/* [23:21] */
#define FRAME_INIT_Q                  REG_FLD(3, 24)/* [26:24] */
#define FORCE_CURR_Q_EN               REG_FLD(1, 27)/* [27:27] */
#define FORCE_CURR_Q                  REG_FLD(3, 28)/* [30:28] */
#define SRAM_ALWAYS_ON                REG_FLD(1, 31)/* [31:31] */

#define OD_REG77 (OD_BASE + 0x05FC)
#define RC_U_RATIO                    REG_FLD(9, 0)/* [8:0] */
#define RC_L_RATIO                    REG_FLD(9, 9)/* [17:9] */
#define IP_SAD_TH                     REG_FLD(7, 18)/* [24:18] */
#define VOTE_CHG                      REG_FLD(1, 28)/* [28:28] */
#define NO_CONSECUTIVE_CHG            REG_FLD(1, 29)/* [29:29] */
#define VOTE_THR_SEL                  REG_FLD(2, 30)/* [31:30] */

#define OD_REG78 (OD_BASE + 0x06C0)
#define IP_MODE_MASK                  REG_FLD(8, 0)/* [7:0] */
#define RC_U_RATIO_FIRST2             REG_FLD(9, 8)/* [16:8] */
#define RC_L_RATIO_FIRST2             REG_FLD(9, 17)/* [25:17] */
#define FORCE_1ST_FRAME_END           REG_FLD(2, 26)/* [27:26] */
#define OD_DEC_ECO_DISABLE            REG_FLD(1, 28)/* [28:28] */
#define OD_COMP_1ROW_MODE             REG_FLD(1, 29)/* [29:29] */
#define OD_IP_DATA_SEL                REG_FLD(2, 30)/* [31:30] */

#define OD_REG79 (OD_BASE + 0x06C4)
#define ROD_VIDX0                     REG_FLD(10, 1)/* [10:1] */
#define ROD_EN                        REG_FLD(1, 11)/* [11:11] */
#define ROD_VGAIN_DEC                 REG_FLD(1, 12)/* [12:12] */
#define ROD_VIDX1                     REG_FLD(10, 13)/* [22:13] */
#define ROD_VR2_BASE                  REG_FLD(8, 24)/* [31:24] */

#define OD_REG80 (OD_BASE + 0x06C8)
#define ROD_VR0_BASE                  REG_FLD(8, 0)/* [7:0] */
#define ROD_VR1_BASE                  REG_FLD(8, 8)/* [15:8] */
#define ROD_VR0_STEP                  REG_FLD(8, 16)/* [23:16] */
#define ROD_VR1_STEP                  REG_FLD(8, 24)/* [31:24] */

#define OD_REG81 (OD_BASE + 0x06CC)
#define ROD_IDX0                      REG_FLD(8, 0)/* [7:0] */
#define ROD_IDX1                      REG_REG_FLD(8, 8)/* [15:8] */
#define ROD_IDX2                      REG_FLD(8, 16)/* [23:16] */
#define ROD_VR2_STEP                  REG_FLD(8, 24)/* [31:24] */

#define OD_REG82 (OD_BASE + 0x06D0)
#define ROD_R0_GAIN                   REG_FLD(8, 0)/* [7:0] */
#define ROD_R1_GAIN                   REG_FLD(8, 8)/* [15:8] */
#define ROD_R2_GAIN                   REG_FLD(8, 16)/* [23:16] */
#define ROD_R3_GAIN                   REG_FLD(8, 24)/* [31:24] */

#define OD_REG83 (OD_BASE + 0x06D4)
#define ROD_R0_OFFSET                 REG_FLD(8, 0)/* [7:0] */
#define ROD_R1_OFFSET                 REG_FLD(8, 8)/* [15:8] */
#define ROD_R2_OFFSET                 REG_FLD(8, 16)/* [23:16] */
#define ROD_R3_OFFSET                 REG_FLD(8, 24)/* [31:24] */

#define OD_REG84 (OD_BASE + 0x06D8)
#define ROD_R4_GAIN                   REG_FLD(8, 0)/* [7:0] */
#define ROD_R5_GAIN                   REG_FLD(8, 8)/* [15:8] */
#define ROD_R6_GAIN                   REG_FLD(8, 16)/* [23:16] */
#define ROD_R7_GAIN                   REG_FLD(8, 24)/* [31:24] */

#define OD_REG85 (OD_BASE + 0x06DC)
#define ROD_R4_OFFSET                 REG_FLD(8, 0)/* [7:0] */
#define ROD_R5_OFFSET                 REG_FLD(8, 8)/* [15:8] */
#define ROD_R6_OFFSET                 REG_FLD(8, 16)/* [23:16] */
#define ROD_R7_OFFSET                 REG_FLD(8, 24)/* [31:24] */

#define OD_REG86 (OD_BASE + 0x06E0)
#define RFB_VIDX0                     REG_FLD(10, 1)/* [10:1] */
#define RFB_EN                        REG_FLD(1, 11)/* [11:11] */
#define RFB_VGAIN_DEC                 REG_FLD(1, 12)/* [12:12] */
#define RFB_VIDX1                     REG_FLD(10, 13)/* [22:13] */
#define RFB_VR2_BASE                  REG_FLD(8, 24)/* [31:24] */

#define OD_REG87 (OD_BASE + 0x06E4)
#define RFB_VR0_BASE                  REG_FLD(8, 0)/* [7:0] */
#define RFB_VR1_BASE                  REG_FLD(8, 8)/* [15:8] */
#define RFB_VR0_STEP                  REG_FLD(8, 16)/* [23:16] */
#define RFB_VR1_STEP                  REG_FLD(8, 24)/* [31:24] */

#define OD_REG88 (OD_BASE + 0x06E8)
#define IP_SAD_TH2                    REG_FLD(7, 0)/* [6:0] */
#define IP_SAD_TH2_UB                 REG_FLD(8, 8)/* [15:8] */
#define IP_SAD_TH2_LB                 REG_FLD(8, 16)/* [23:16] */
#define RFB_VR2_STEP                  REG_FLD(8, 24)/* [31:24] */

#define OD_REG89 (OD_BASE + 0x06EC)
#define IP_SAD_TH3                    REG_FLD(7, 0)/* [6:0] */
#define IP_SAD_TH3_UB                 REG_FLD(8, 8)/* [15:8] */
#define IP_SAD_TH3_LB                 REG_FLD(8, 16)/* [23:16] */

#define OD_REG_CRC32_0 (OD_BASE + 0x0580)
#define OD_CRC32_TOP_L_EN             REG_FLD(1, 0)/* [0:0] */
#define OD_CRC32_TOP_R_EN             REG_FLD(1, 1)/* [1:1] */
#define OD_CRC32_EVEN_LINE_EN         REG_FLD(1, 2)/* [2:2] */
#define OD_CRC32_ODD_LINE_EN          REG_FLD(1, 3)/* [3:3] */
#define OD_CRC32_CHECK_SUM_MODE       REG_FLD(1, 4)/* [4:4] */
#define OD_CRC32_STILL_CHECK_TRIG     REG_FLD(1, 5)/* [5:5] */
#define OD_CRC32_CLEAR_READY          REG_FLD(1, 6)/* [6:6] */
#define OD_CRC32_VSYNC_INV            REG_FLD(1, 7)/* [7:7] */
#define OD_CRC32_STILL_CHECK_MAX      REG_FLD(8, 8)/* [15:8] */

#define OD_REG_CRC32_1 (OD_BASE + 0x0584)
#define OD_CRC32_CLIP_H_START         REG_FLD(13, 0)/* [12:0] */
#define OD_CRC32_CLIP_V_START         REG_FLD(12, 16)/* [27:16] */

#define OD_REG_CRC32_2 (OD_BASE + 0x0588)
#define OD_CRC32_CLIP_H_END           REG_FLD(13, 0)/* [12:0] */
#define OD_CRC32_CLIP_V_END           REG_FLD(12, 16)/* [27:16] */

#define OD_REG_LT_00 (OD_BASE + 0x0500)
#define REGION_0_POS                  REG_FLD(13, 0)/* [12:0] */
#define REGION_1_POS                  REG_FLD(13, 13)/* [25:13] */
#define LOCA_TABLE_EN                 REG_FLD(1, 26)/* [26:26] */
#define REGION_H_BLEND_SEL            REG_FLD(2, 27)/* [28:27] */
#define LT_USE_PCID                   REG_FLD(1, 29)/* [29:29] */
#define LT_READ_NEW_TABLE             REG_FLD(1, 30)/* [30:30] */
#define SWITCH_TABLE_DMA_SRC          REG_FLD(1, 31)/* [31:31] */

#define OD_REG_LT_01 (OD_BASE + 0x0504)
#define REGION_2_POS                  REG_FLD(13, 0)/* [12:0] */
#define ROW_0_POS                     REG_FLD(12, 13)/* [24:13] */
#define LOCAL_TABLE_DELAY             REG_FLD(3, 25)/* [27:25] */
#define LT_DRAM_WAIT_SEL              REG_FLD(1, 28)/* [28:28] */
#define LT_DONE_MASK_SEL              REG_FLD(1, 29)/* [29:29] */

#define OD_REG_LT_02 (OD_BASE + 0x0508)
#define ROW_1_POS                     REG_FLD(12, 0)/* [11:0] */
#define ROW_2_POS                     REG_FLD(12, 12)/* [23:12] */

#define OD_REG_LT_03 (OD_BASE + 0x050C)
#define ROW_3_POS                     REG_FLD(12, 0)/* [11:0] */
#define LT_PAUSE_LEN                  REG_FLD(8, 24)/* [31:24] */

#define OD_REG_LT_04 (OD_BASE + 0x0510)
#define LT_BASE_ADDR                  REG_FLD(28, 0)/* [27:0] */
#define LT_BURST_LEN                  REG_FLD(2, 28)/* [29:28] */
#define LT_CLR_UNDERFLOW              REG_FLD(1, 30)/* [30:30] */
#define LT_RDRAM_X1                   REG_FLD(1, 31)/* [31:31] */

#define OD_STA00 (OD_BASE + 0x0680)
#define STA_GM_GMIN_422               REG_FLD(4, 0)/* [3:0] */
#define STA_GM_XV                     REG_FLD(3, 4)/* [6:4] */
#define OD_RDY                        REG_FLD(1, 7)/* [7:7] */
#define STA_GM_LMIN                   REG_FLD(4, 8)/* [11:8] */
#define STA_GM_GMIN                   REG_FLD(4, 12)/* [15:12] */
#define STA_GM_MISC                   REG_FLD(4, 16)/* [19:16] */
#define STA_CSKIP_DET                 REG_FLD(1, 20)/* [20:20] */
#define EFP_BYPASS                    REG_FLD(1, 21)/* [21:21] */
#define RD_ASF_UFLOW                  REG_FLD(1, 22)/* [22:22] */
#define DRAM_CRC_ERROR                REG_FLD(1, 23)/* [23:23] */
#define FP_BYPASS                     REG_FLD(1, 24)/* [24:24] */
#define FP_BYPASS_INT                 REG_FLD(1, 25)/* [25:25] */
#define COMP_Y5_MODE                  REG_FLD(1, 26)/* [26:26] */
#define BTC_8B                        REG_FLD(1, 27)/* [27:27] */
#define DE_8B                         REG_FLD(1, 28)/* [28:28] */
#define DET_Y5_MODE                   REG_FLD(1, 29)/* [29:29] */
#define DET_BTC_8B                    REG_FLD(1, 30)/* [30:30] */
#define DET_DC_8B                     REG_FLD(1, 31)/* [31:31] */

#define OD_STA01 (OD_BASE + 0x0684)
#define BLOCK_STA_CNT                 REG_FLD(18, 0)/* [17:0] */
#define DUMP_UD_FLAG                  REG_FLD(1, 18)/* [18:18] */
#define DUMP_OV_FLAG                  REG_FLD(1, 19)/* [19:19] */
#define STA_TIMING_H                  REG_FLD(12, 20)/* [31:20] */

#define OD_STA02 (OD_BASE + 0x0688)
#define STA_FRAME_BIT                 REG_FLD(13, 0)/* [12:0] */
#define R_UNDERFLOW                   REG_FLD(1, 13)/* [13:13] */
#define W_UNDERFLOW                   REG_FLD(1, 14)/* [14:14] */
#define MOT_DEBUG                     REG_FLD(1, 15)/* [15:15] */
#define DISP_MISMATCH                 REG_FLD(1, 16)/* [16:16] */
#define DE_MISMATCH                   REG_FLD(1, 17)/* [17:17] */
#define V_MISMATCH                    REG_FLD(1, 18)/* [18:18] */
#define H_MISMATCH                    REG_FLD(1, 19)/* [19:19] */
#define STA_TIMING_L                  REG_FLD(12, 20)/* [31:20] */

#define OD_STA03 (OD_BASE + 0x068C)
#define STA_LINE_BIT                  REG_FLD(15, 0)/* [14:0] */
#define SRAM_POOL_ACCESS_ERR          REG_FLD(1, 15)/* [15:15] */
#define RW_TABLE_ERROR                REG_FLD(1, 16)/* [16:16] */
#define RW_TABLE_RDRAM_UNDERFLOW      REG_FLD(1, 17)/* [17:17] */

#define OD_STA04 (OD_BASE + 0x0690)
#define STA_TIMING                    REG_FLD(1, 8)/* [8:8] */
#define STA_DATA                      REG_FLD(8, 0)/* [7:0] */
#define STA_DUMP_WDRAM_ADDR           REG_FLD(10, 9)/* [18:9] */
#define STA_DUMP_WDRAM_WDATA          REG_FLD(10, 19)/* [28:19] */
#define DUMP_WDRAM_REQ                REG_FLD(1, 29)/* [29:29] */
#define DUMP_WDRAM_ALE                REG_FLD(1, 30)/* [30:30] */
#define DUMP_WDRAM_SWITCH             REG_FLD(1, 31)/* [31:31] */

#define OD_STA05 (OD_BASE + 0x0694)
#define STA_IFM                       REG_FLD(32, 0)/* [31:0] */

#define OD_STA06 (OD_BASE + 0x0698)
#define STA_FP_ERR                    REG_FLD(12, 0)/* [11:0] */
#define STA_INT_WAIT_LCNT             REG_FLD(8, 12)/* [19:12] */
#define CURR_Q                        REG_FLD(3, 20)/* [22:20] */
#define TABLE_DMA_PERIOD_CONF         REG_FLD(1, 29)/* [29:29] */
#define TABLE_DMA_PERIOD              REG_FLD(1, 30)/* [30:30] */
#define TABLE_DMA_DONE                REG_FLD(1, 31)/* [31:31] */

#define OD_STA_CRC32_0 (OD_BASE + 0x0540)
#define STA_CRC32_CRC_OUT_H           REG_FLD(32, 0)  /*[31:0] */

#define OD_STA_CRC32_1 (OD_BASE + 0x0544)
#define STA_CRC32_CRC_OUT_V           REG_FLD(32, 0)  /*[31:0] */

#define OD_STA_CRC32_2 (OD_BASE + 0x0548)
#define STA_CRC32_NON_STILL_CNT       REG_FLD(4, 0)   /*[3:0] */
#define STA_CRC32_STILL_CHECK_DONE    REG_FLD(1, 4)   /*[4:4] */
#define STA_CRC32_CRC_READY           REG_FLD(1, 5)   /*[5:5] */
#define STA_CRC32_CRC_OUT_V_READY     REG_FLD(1, 6)   /*[6:6] */
#define STA_CRC32_CRC_OUT_H_READY     REG_FLD(1, 7)   /*[7:7] */
#define LOCAL_TABLE_READ_DRAM_DONE    REG_FLD(1, 8)   /*[8:8] */
#define LOCAL_TABLE_READ_DRAM         REG_FLD(1, 9)   /*[9:9] */
#define LOCAL_TABLE_CURRENT_ROW       REG_FLD(3, 10)  /*[12:10] */
#define FIFO_FULL                     REG_FLD(1, 13)  /*[13:13] */
#define FIFO_EMPTY                    REG_FLD(1, 14)  /*[14:14] */
#define WRITE_TABLE                   REG_FLD(1, 15)  /*[15:15] */
#define START_READ_DRAM               REG_FLD(1, 16)  /*[16:16] */
#define WAIT_FIFO_M                   REG_FLD(1, 17)  /*[17:17] */
#define WAIT_FIFO                     REG_FLD(1, 18)  /*[18:18] */
#define READ_DRAM_DONE_MASK           REG_FLD(1, 19)  /*[19:19] */
#define WAIT_NEXT_START_READ_DRAM     REG_FLD(1, 20)  /*[20:20] */
#define RDRAM_REQ                     REG_FLD(1, 21)  /*[21:21] */
#define LOCAL_TABLE_DRAM_FSM          REG_FLD(2, 22)  /*[23:22] */
#define LOCAL_TABLE_TABLE_FSM         REG_FLD(6, 24)  /*[29:24] */
#define WRITE_TABLE_DONE              REG_FLD(1, 30)  /*[30:30] */
#define PKT_CRC_ERROR                 REG_FLD(1, 31)  /*[31:31] */
#define OD_FLD_ALL                    REG_FLD(32, 0)  /*[31:0] */

#endif
