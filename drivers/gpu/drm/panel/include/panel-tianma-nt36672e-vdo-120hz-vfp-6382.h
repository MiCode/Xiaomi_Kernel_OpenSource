/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef PANEL_NT36672E_FHDP_DSI_VDO_120HZ_TIANMA_HFP
#define PANEL_NT36672E_FHDP_DSI_VDO_120HZ_TIANMA_HFP


#define REGFLAG_DELAY           0xFFFC
#define REGFLAG_UDELAY          0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW       0xFFFE
#define REGFLAG_RESET_HIGH      0xFFFF

#define FRAME_WIDTH                 1080
#define FRAME_HEIGHT                2408

#define PHYSICAL_WIDTH              68364
#define PHYSICAL_HEIGHT             152300

#define DATA_RATE                   1004
#define HSA                         8
#define HBP                         22
#define VSA                         8
#define VBP                         5

/*Parameter setting for mode 0 Start*/
#define MODE_0_FPS                  60
#define MODE_0_VFP                  2480
#define MODE_0_HFP                  155
#define MODE_0_DATA_RATE            1004
/*Parameter setting for mode 0 End*/

/*Parameter setting for mode 1 Start*/
#define MODE_1_FPS                  90
#define MODE_1_VFP                  810
#define MODE_1_HFP                  155
#define MODE_1_DATA_RATE            1004
/*Parameter setting for mode 1 End*/

/*Parameter setting for mode 2 Start*/
#define MODE_2_FPS                  120
#define MODE_2_VFP                  40
#define MODE_2_HFP                  155
#define MODE_2_DATA_RATE            1004
/*Parameter setting for mode 2 End*/

#define LFR_EN                      1
/* DSC RELATED */

#define DSC_ENABLE                  0
#define DSC_VER                     17
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 34
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         8
#define DSC_DSC_LINE_BUF_DEPTH      9
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
//define DSC_PIC_HEIGHT
//define DSC_PIC_WIDTH
#define DSC_SLICE_HEIGHT            8
#define DSC_SLICE_WIDTH             540
#define DSC_CHUNK_SIZE              540
#define DSC_XMIT_DELAY              170
#define DSC_DEC_DELAY               526
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      43
#define DSC_DECREMENT_INTERVAL      7
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          3511
#define DSC_SLICE_BPG_OFFSET        3255
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            7072
#define DSC_FLATNESS_MINQP          3
#define DSC_FLATNESS_MAXQP          12
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    11
#define DSC_RC_QUANT_INCR_LIMIT1    11
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3

#endif //end of PANEL_NT36672E_FHDP_DSI_VDO_120HZ_TIANMA_HFP
