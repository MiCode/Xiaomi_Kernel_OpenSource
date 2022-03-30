/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MRAW_REGS_H
#define _MRAW_REGS_H

#define REG_MRAW_MRAWCTL_MOD_EN					0x0000
#define REG_MRAW_MRAWCTL_MOD2_EN				0x0004
union MRAW_MRAWCTL_MOD2_EN {
	struct {
		unsigned int MRAWCTL_IMGO_M1_EN  :  1;    /*  0.. 0, 0x00000001 */
		unsigned int MRAWCTL_IMGBO_M1_EN :  1;    /*  1.. 1, 0x00000002 */
		unsigned int MRAWCTL_CPIO_M1_EN  :  1;    /*  2.. 2, 0x00000004 */
		unsigned int MRAWCTL_AFO_M1_EN   :  1;    /*  3.. 3, 0x00000008 */
		unsigned int MRAWCTL_TSFSO_M1_EN :  1;    /*  4.. 4, 0x00000010 */
		unsigned int MRAWCTL_TSFSO_M2_EN :  1;    /*  5.. 5, 0x00000020 */
		unsigned int MRAWCTL_LSCI_M1_EN  :  1;    /*  6.. 6, 0x00000040 */
		unsigned int MRAWCTL_CQI_M1_EN   :  1;    /*  7.. 7, 0x00000080 */
		unsigned int MRAWCTL_CQI_M2_EN   :  1;    /*  8.. 8, 0x00000100 */
		unsigned int MRAWCTL_CQI_M3_EN   :  1;    /*  9.. 9, 0x00000200 */
		unsigned int MRAWCTL_CQI_M4_EN   :  1;    /* 10..10, 0x00000400 */
		unsigned int rsv_11              : 21;    /* 11..31, 0xfffff800 */
	} Bits;
	unsigned int Raw;
};
#define REG_MRAW_CTL_INT_EN			0x0100
#define REG_MRAW_CTL_INT_STATUS		0x0104
#define REG_MRAW_CTL_INT2_EN		0x0110
#define REG_MRAW_CTL_INT2_STATUS	0x0114
#define REG_MRAW_CTL_INT3_EN		0x0120
#define REG_MRAW_CTL_INT3_STATUS	0x0124
#define REG_MRAW_CTL_INT4_EN		0x0130
#define REG_MRAW_CTL_INT4_STATUS	0x0134
#define REG_MRAW_CTL_INT5_EN		0x0140
#define REG_MRAW_CTL_INT5_STATUS	0x0144
#define REG_MRAW_CTL_INT5_STATUSX	0X0148
#define REG_MRAW_CTL_INT6_EN		0x0150
#define REG_MRAW_CTL_INT6_STATUS	0x0154

#define REG_MRAW_CQ_EN					0x0400
#define REG_MRAW_SCQ_START_PERIOD		0x0408
#define REG_MRAW_CQ_THR0_CTL			0x0410
#define REG_MRAW_CQ_SUB_CQ_EN			0x04A0
#define REG_MRAW_CQ_SUB_THR0_CTL		0x04B0

#define REG_MRAW_CTL_SW_CTL				0x0048
#define REG_MRAW_CTL_START				0x0040
#define REG_MRAW_CTL_MOD_EN				0x0000


#define REG_MRAW_CQ_THR0_BASEADDR			0x0414
#define REG_MRAW_CQ_THR0_BASEADDR_MSB		0x0418
#define REG_MRAW_CQ_THR0_DESC_SIZE			0x041C
#define REG_MRAW_SCQ_CQ_TRIG_TIME			0x04AC
#define SCQ_EN						BIT(20)
#define SCQ_STAGGER_MODE			BIT(12)
#define SCQ_SUBSAMPLE_EN			BIT(21)
#define CQ_DB_EN					BIT(4)
#define CQ_DB_LOAD_MODE				BIT(8)
#define CQ_THR0_MODE_IMMEDIATE		BIT(4)
#define CQ_THR0_MODE_CONTINUOUS		BIT(5)
#define CQ_THR0_EN					BIT(0)
#define SCQ_SUB_RESET				BIT(16)

union MRAW_MRAWCQ_CQ_EN {
	struct {
		unsigned int MRAWCQ_CQ_APB_2T          :  1;    /*  0.. 0, 0x00000001 */
		unsigned int MRAWCQ_CQ_DROP_FRAME_EN   :  1;    /*  1.. 1, 0x00000002 */
		unsigned int MRAWCQ_CQ_SOF_SEL         :  1;    /*  2.. 2, 0x00000004 */
		unsigned int rsv_3                     :  1;    /*  3.. 3, 0x00000008 */
		unsigned int MRAWCQ_CQ_DB_EN           :  1;    /*  4.. 4, 0x00000010 */
		unsigned int rsv_5                     :  3;    /*  5.. 7, 0x000000e0 */
		unsigned int MRAWCQ_CQ_DB_LOAD_MODE    :  1;    /*  8.. 8, 0x00000100 */
		unsigned int rsv_9                     :  3;    /*  9..11, 0x00000e00 */
		unsigned int MRAWCQ_SCQ_STAGGER_MODE   :  1;    /* 12..12, 0x00001000 */
		unsigned int rsv_13                    :  3;    /* 13..15, 0x0000e000 */
		unsigned int MRAWCQ_CQ_RESET           :  1;    /* 16..16, 0x00010000 */
		unsigned int rsv_17                    :  3;    /* 17..19, 0x000e0000 */
		unsigned int MRAWCQ_SCQ_EN             :  1;    /* 20..20, 0x00100000 */
		unsigned int MRAWCQ_SCQ_SUBSAMPLE_EN   :  1;    /* 21..21, 0x00200000 */
		unsigned int rsv_22                    :  2;    /* 22..23, 0x00c00000 */
		unsigned int MRAWCQ_SCQ_DLY_P1_DONE_EN :  1;    /* 24..24, 0x01000000 */
		unsigned int rsv_25                    :  3;    /* 25..27, 0x0e000000 */
		unsigned int MRAWCQ_CQ_DBG_SEL         :  1;    /* 28..28, 0x10000000 */
		unsigned int rsv_29                    :  3;    /* 29..31, 0xe0000000 */
	} Bits;
	unsigned int Raw;
};

union MRAW_MRAWCQ_CQ_THR0_CTL {
	struct {
		unsigned int MRAWCQ_CQ_THR0_EN       :  1;    /*  0.. 0, 0x00000001 */
		unsigned int rsv_1                   :  3;    /*  1.. 3, 0x0000000e */
		unsigned int MRAWCQ_CQ_THR0_MODE     :  2;    /*  4.. 5, 0x00000030 */
		unsigned int rsv_6                   :  2;    /*  6.. 7, 0x000000c0 */
		unsigned int MRAWCQ_CQ_THR0_DONE_SEL :  1;    /*  8.. 8, 0x00000100 */
		unsigned int rsv_9                   : 23;    /*  9..31, 0xfffffe00 */
	} Bits;
	unsigned int Raw;
};

#define REG_MRAW_TG_SEN_MODE					0x0700
#define MRAW_TG_SEN_MODE_CMOS_EN		BIT(0)
//different: ADD TG_SOF_SUB_CNT_EXCEPTION_DIS
union MRAW_TG_SEN_MODE {
	struct {
		unsigned int TG_CMOS_EN                   :  1;    /*  0.. 0, 0x00000001 */
		unsigned int rsv_1                        :  1;    /*  1.. 1, 0x00000002 */
		unsigned int TG_SOT_MODE                  :  1;    /*  2.. 2, 0x00000004 */
		unsigned int TG_SOT_CLR_MODE              :  1;    /*  3.. 3, 0x00000008 */
		unsigned int TG_DBL_DATA_BUS              :  2;    /*  4.. 5, 0x00000030 */
		unsigned int rsv_6                        :  2;    /*  6.. 7, 0x000000c0 */
		unsigned int TG_SOF_SRC                   :  2;    /*  8.. 9, 0x00000300 */
		unsigned int TG_EOF_SRC                   :  2;    /* 10..11, 0x00000c00 */
		unsigned int TG_PXL_CNT_RST_SRC           :  1;    /* 12..12, 0x00001000 */
		unsigned int rsv_13                       :  1;    /* 13..13, 0x00002000 */
		unsigned int TG_M1_CMOS_RDY_SEL           :  1;    /* 14..14, 0x00004000 */
		unsigned int TG_FIFO_FULL_CTL_EN          :  1;    /* 15..15, 0x00008000 */
		unsigned int TG_TIME_STP_EN               :  1;    /* 16..16, 0x00010000 */
		unsigned int TG_VS_SUB_EN                 :  1;    /* 17..17, 0x00020000 */
		unsigned int TG_SOF_SUB_EN                :  1;    /* 18..18, 0x00040000 */
		unsigned int TG_VSYNC_INT_POL             :  1;    /* 19..19, 0x00080000 */
		unsigned int TG_EOF_ALS_RDY_EN            :  1;    /* 20..20, 0x00100000 */
		unsigned int rsv_21                       :  1;    /* 21..21, 0x00200000 */
		unsigned int TG_M1_STAGGER_EN             :  1;    /* 22..22, 0x00400000 */
		unsigned int TG_HDR_EN                    :  1;    /* 23..23, 0x00800000 */
		unsigned int TG_HDR_SEL                   :  1;    /* 24..24, 0x01000000 */
		unsigned int TG_SOT_DLY_EN                :  1;    /* 25..25, 0x02000000 */
		unsigned int TG_VS_IGNORE_STALL_EN        :  1;    /* 26..26, 0x04000000 */
		unsigned int TG_LINE_OK_CHECK             :  1;    /* 27..27, 0x08000000 */
		unsigned int TG_SOF_SUB_CNT_EXCEPTION_DIS :  1;    /* 28..28, 0x10000000 */
		unsigned int rsv_29                       :  3;    /* 29..31, 0xe0000000 */
	} Bits;
	unsigned int Raw;
};

#define MRAW_TG_CMOS_RDY_SEL					BIT(14)
#define MRAW_TG_VF_CON_VFDATA_EN				BIT(0)
#define REG_MRAW_TG_VF_CON					0x0704
union MRAW_TG_VF_CON {
	struct {
		unsigned int TG_M1_VFDATA_EN        :  1;    /*  0.. 0, 0x00000001 */
		unsigned int TG_SINGLE_MODE         :  1;    /*  1.. 1, 0x00000002 */
		unsigned int rsv_2                  :  2;    /*  2.. 3, 0x0000000c */
		unsigned int TG_FR_CON              :  3;    /*  4.. 6, 0x00000070 */
		unsigned int rsv_7                  :  1;    /*  7.. 7, 0x00000080 */
		unsigned int TG_SP_DELAY            :  3;    /*  8..10, 0x00000700 */
		unsigned int rsv_11                 :  1;    /* 11..11, 0x00000800 */
		unsigned int TG_SPDELAY_MODE        :  1;    /* 12..12, 0x00001000 */
		unsigned int TG_VFDATA_EN_MUX_0_SEL :  1;    /* 13..13, 0x00002000 */
		unsigned int TG_VFDATA_EN_MUX_1_SEL :  1;    /* 14..14, 0x00004000 */
		unsigned int rsv_15                 : 17;    /* 15..31, 0xffff8000 */
	} Bits;
	unsigned int Raw;
};
#define REG_MRAW_TG_SEN_GRAB_PXL				0x0708
#define REG_MRAW_TG_SEN_GRAB_LIN				0x070C
#define MRAW_TG_PATH_TG_FULL_SEL				BIT(15)
#define REG_MRAW_TG_PATH_CFG					0x0710
//[todo]: implement LOAD_HOLD
union MRAW_TG_PATH_CFG // only for db_load test
{
	struct {
		unsigned int TG_SEN_IN_LSB        :  3;    /*  0.. 2, 0x00000007 */
		unsigned int rsv_3                :  1;    /*  3.. 3, 0x00000008 */
		unsigned int TG_JPGINF_EN         :  1;    /*  4.. 4, 0x00000010 */
		unsigned int TG_MEMIN_EN          :  1;    /*  5.. 5, 0x00000020 */
		unsigned int rsv_6                :  1;    /*  6.. 6, 0x00000040 */
		unsigned int TG_JPG_LINEND_EN     :  1;    /*  7.. 7, 0x00000080 */
		unsigned int TG_M1_DB_LOAD_DIS    :  1;    /*  8.. 8, 0x00000100 */
		unsigned int TG_DB_LOAD_SRC       :  1;    /*  9.. 9, 0x00000200 */
		unsigned int TG_DB_LOAD_VSPOL     :  1;    /* 10..10, 0x00000400 */
		unsigned int TG_DB_LOAD_HOLD      :  1;    /* 11..11, 0x00000800 */
		unsigned int TG_YUV_U2S_DIS       :  1;    /* 12..12, 0x00001000 */
		unsigned int TG_YUV_BIN_EN        :  1;    /* 13..13, 0x00002000 */
		unsigned int TG_ERR_SEL           :  1;    /* 14..14, 0x00004000 */
		unsigned int TG_FULL_SEL          :  1;    /* 15..15, 0x00008000 */
		unsigned int TG_FULL_SEL2         :  1;    /* 16..16, 0x00010000 */
		unsigned int TG_FLUSH_DISABLE     :  1;    /* 17..17, 0x00020000 */
		unsigned int TG_INT_BLANK_DISABLE :  1;    /* 18..18, 0x00040000 */
		unsigned int TG_EXP_ESC           :  1;    /* 19..19, 0x00080000 */
		unsigned int TG_SUB_SOF_SRC_SEL   :  2;    /* 20..21, 0x00300000 */
		unsigned int rsv_22               : 10;    /* 22..31, 0xffc00000 */
	} Bits;
	unsigned int Raw;
};

#define REG_MRAW_TG_INTER_ST				0x073C
/* use this MASK to extract TG_CAM_CS from TG_INTER_ST */
#define MRAW_TG_CS_MASK						0x3F00
#define MRAW_TG_IDLE_ST						BIT(8)

#define REG_MRAW_MRAWCTL_FMT_SEL			0x0024
union MRAW_FMT_SEL {
	struct {
		unsigned int MRAWCTL_PIX_ID       :  2;    /*  0.. 1, 0x00000003 */
		unsigned int rsv_2                :  2;    /*  2.. 3, 0x0000000c */
		unsigned int MRAWCTL_IMGO_M1_FMT  :  8;    /*  4..11, 0x00000ff0 */
		unsigned int MRAWCTL_TG_FMT       :  3;    /* 12..14, 0x00007000 */
		unsigned int rsv_15               :  1;    /* 15..15, 0x00008000 */
		unsigned int MRAWCTL_TG_SWAP      :  2;    /* 16..17, 0x00030000 */
		unsigned int MRAWCTL_PIX_BUS_TGO  :  2;    /* 18..19, 0x000c0000 */
		unsigned int MRAWCTL_PIX_BUS_SEPO :  2;    /* 20..21, 0x00300000 */
		unsigned int rsv_22               : 10;    /* 22..31, 0xffc00000 */
	} Bits;
	unsigned int Raw;
};

#define REG_MRAW_TG_FRMSIZE_ST				0x0738
#define REG_MRAW_TG_FRMSIZE_ST_R			0x076C
#define REG_MRAW_TG_TIME_STAMP				0x0778
#define REG_MRAW_TG_TIME_STAMP_CNT			0x077C

/* use spare register FH_SPARE_3 */
#define REG_MRAW_FRAME_SEQ_NUM				0x238C

#define REG_MRAW_MRAWCTL_FBC_GROUP			0x0038
union MRAW_MRAWCTL_FBC_GROUP {
	struct {
		unsigned int MRAWCTL_IMGO_M1_FBC_SEL  :  1;    /*  0.. 0, 0x00000001 */
		unsigned int MRAWCTL_IMGBO_M1_FBC_SEL :  1;    /*  1.. 1, 0x00000002 */
		unsigned int MRAWCTL_CPIO_M1_FBC_SEL  :  1;    /*  2.. 2, 0x00000004 */
		unsigned int MRAWCTL_AFO_M1_FBC_SEL   :  1;    /*  3.. 3, 0x00000008 */
		unsigned int MRAWCTL_TSFSO_M1_FBC_SEL :  1;    /*  4.. 4, 0x00000010 */
		unsigned int MRAWCTL_TSFSO_M2_FBC_SEL :  1;    /*  5.. 5, 0x00000020 */
		unsigned int rsv_6                    : 26;    /*  6..31, 0xffffffc0 */
	} Bits;
	unsigned int Raw;
};

#define REG_MRAW_CAMCTL_FBC_RCNT_INC				0x003C

#define MRAWCTL_IMGO_RCNT_INC			BIT(0)
#define MRAWCTL_IMGBO_RCNT_INC			BIT(1)
#define MRAWCTL_CPIO_RCNT_INC			BIT(2)
#define MRAWCTL_CQ_THR0_DONE_ST			BIT(0)
#define MRAWCTL_CQ_THR0_START			BIT(0)

#define REG_MRAW_FBC_IMGO_CTL1			0x1480
#define REG_MRAW_FBC_IMGO_CTL2			0x1484
#define REG_MRAW_IMGO_BASE_ADDR			0x2330
#define REG_MRAW_IMGO_BASE_ADDR_MSB		0x2334
#define REG_MRAW_IMGO_OFST_ADDR			0x2338
#define REG_MRAW_IMGO_OFST_ADDR_MSB		0x233C
#define REG_MRAW_IMGO_XSIZE				0x2340
#define REG_MRAW_IMGO_YSIZE				0x2344
#define REG_MRAW_IMGO_STRIDE			0x2348
#define REG_MRAW_IMGO_ERR_STAT			0x2364

#define REG_MRAW_FBC_IMGBO_CTL1			0x1488
#define REG_MRAW_FBC_IMGBO_CTL2			0x148C
#define REG_MRAW_IMGBO_BASE_ADDR		0x23E0
#define REG_MRAW_IMGBO_BASE_ADDR_MSB	0x23E4
#define REG_MRAW_IMGBO_OFST_ADDR		0x23E8
#define REG_MRAW_IMGBO_OFST_ADDR_MSB	0x23EC
#define REG_MRAW_IMGBO_XSIZE			0x23F0
#define REG_MRAW_IMGBO_YSIZE			0x23F4
#define REG_MRAW_IMGBO_STRIDE			0x23F8
#define REG_MRAW_IMGBO_ERR_STAT			0x2414

#define REG_MRAW_FBC_CPIO_CTL1			0x1490
#define REG_MRAW_FBC_CPIO_CTL2			0x1494
#define REG_MRAW_CPIO_BASE_ADDR			0x2490
#define REG_MRAW_CPIO_BASE_ADDR_MSB		0x2494
#define REG_MRAW_CPIO_OFST_ADDR			0x2498
#define REG_MRAW_CPIO_OFST_ADDR_MSB		0x249C
#define REG_MRAW_CPIO_XSIZE				0x24A0
#define REG_MRAW_CPIO_YSIZE				0x24A4
#define REG_MRAW_CPIO_STRIDE			0x24A8
#define REG_MRAW_CPIO_ERR_STAT			0x24C4

#define REG_MRAW_CTL_MOD2_EN			0x0004
union MRAW_FBC_IMGO_CTL1 {
	struct {
		unsigned int rsv_0                  :  8;    /*  0.. 7, 0x000000ff */
		unsigned int FBC_IMGO_FBC_RESET     :  1;    /*  8.. 8, 0x00000100 */
		unsigned int FBC_IMGO_FBC_DB_EN     :  1;    /*  9.. 9, 0x00000200 */
		unsigned int rsv_10                 :  2;    /* 10..11, 0x00000c00 */
		unsigned int FBC_IMGO_LOCK_EN       :  1;    /* 12..12, 0x00001000 */
		unsigned int FBC_IMGO_DROP_TIMING   :  1;    /* 13..13, 0x00002000 */
		unsigned int FBC_IMGO_FBC_SUB_EN    :  1;    /* 14..14, 0x00004000 */
		unsigned int FBC_IMGO_FBC_EN        :  1;    /* 15..15, 0x00008000 */
		unsigned int FBC_IMGO_VALID_NUM     :  8;    /* 16..23, 0x00ff0000 */
		unsigned int FBC_IMGO_SUB_RATIO     :  8;    /* 24..31, 0xff000000 */
	} Bits;
	unsigned int Raw;
};

union MRAW_FBC_IMGBO_CTL1 {
	struct {
		unsigned int rsv_0                  :  8;    /*  0.. 7, 0x000000ff */
		unsigned int FBC_IMGBO_FBC_RESET    :  1;    /*  8.. 8, 0x00000100 */
		unsigned int FBC_IMGBO_FBC_DB_EN    :  1;    /*  9.. 9, 0x00000200 */
		unsigned int rsv_10                 :  2;    /* 10..11, 0x00000c00 */
		unsigned int FBC_IMGBO_LOCK_EN      :  1;    /* 12..12, 0x00001000 */
		unsigned int FBC_IMGBO_DROP_TIMING  :  1;    /* 13..13, 0x00002000 */
		unsigned int FBC_IMGBO_FBC_SUB_EN   :  1;    /* 14..14, 0x00004000 */
		unsigned int FBC_IMGBO_FBC_EN       :  1;    /* 15..15, 0x00008000 */
		unsigned int FBC_IMGBO_VALID_NUM    :  8;    /* 16..23, 0x00ff0000 */
		unsigned int FBC_IMGBO_SUB_RATIO    :  8;    /* 24..31, 0xff000000 */
	} Bits;
	unsigned int Raw;
};

union MRAW_FBC_CPIO_CTL1 {
	struct {
		unsigned int rsv_0                :  8;    /*  0.. 7, 0x000000ff */
		unsigned int FBC_CPIO_FBC_RESET   :  1;    /*  8.. 8, 0x00000100 */
		unsigned int FBC_CPIO_FBC_DB_EN   :  1;    /*  9.. 9, 0x00000200 */
		unsigned int rsv_10               :  2;    /* 10..11, 0x00000c00 */
		unsigned int FBC_CPIO_LOCK_EN     :  1;    /* 12..12, 0x00001000 */
		unsigned int FBC_CPIO_DROP_TIMING :  1;    /* 13..13, 0x00002000 */
		unsigned int FBC_CPIO_FBC_SUB_EN  :  1;    /* 14..14, 0x00004000 */
		unsigned int FBC_CPIO_FBC_EN      :  1;    /* 15..15, 0x00008000 */
		unsigned int FBC_CPIO_VALID_NUM   :  8;    /* 16..23, 0x00ff0000 */
		unsigned int FBC_CPIO_SUB_RATIO   :  8;    /* 24..31, 0xff000000 */
	} Bits;
	unsigned int Raw;
};

union MRAW_CTL_MOD2_EN {
	struct {
		unsigned int MRAWCTL_IMGO_M1_EN  :  1;    /*  0.. 0, 0x00000001 */
		unsigned int MRAWCTL_IMGBO_M1_EN :  1;    /*  1.. 1, 0x00000002 */
		unsigned int MRAWCTL_CPIO_M1_EN  :  1;    /*  2.. 2, 0x00000004 */
		unsigned int MRAWCTL_AFO_M1_EN   :  1;    /*  3.. 3, 0x00000008 */
		unsigned int MRAWCTL_TSFSO_M1_EN :  1;    /*  4.. 4, 0x00000010 */
		unsigned int MRAWCTL_TSFSO_M2_EN :  1;    /*  5.. 5, 0x00000020 */
		unsigned int MRAWCTL_LSCI_M1_EN  :  1;    /*  6.. 6, 0x00000040 */
		unsigned int MRAWCTL_CQI_M1_EN   :  1;    /*  7.. 7, 0x00000080 */
		unsigned int MRAWCTL_CQI_M2_EN   :  1;    /*  8.. 8, 0x00000100 */
		unsigned int MRAWCTL_CQI_M3_EN   :  1;    /*  9.. 9, 0x00000200 */
		unsigned int MRAWCTL_CQI_M4_EN   :  1;    /* 10..10, 0x00000400 */
		unsigned int rsv_11              : 21;    /* 11..31, 0xfffff800 */
	} Bits;
	unsigned int Raw;
};

#define REG_MRAW_M_MRAWCTL_MISC 0x0060
//[todo]: implement MRAWCTL_DB_LOAD_FORCE
union MRAW_CTL_MISC {
	struct {
		unsigned int MRAWCTL_DB_LOAD_HOLD        :  1; /*  0.. 0, 0x00000001 */
		unsigned int MRAWCTL_DB_LOAD_HOLD_SUB    :  1; /*  1.. 1, 0x00000002 */
		unsigned int MRAWCTL_DB_LOAD_SRC         :  2; /*  2.. 3, 0x0000000c */
		unsigned int MRAWCTL_DB_EN               :  1; /*  4.. 4, 0x00000010 */
		unsigned int rsv_5                       :  3; /*  5.. 7, 0x000000e0 */
		unsigned int MRAWCTL_APB_CLK_GATE_BYPASS :  1; /*  8.. 8, 0x00000100 */
		unsigned int rsv_9                       : 23; /*  9..31, 0xfffffe00 */
		unsigned int MRAWCTL_DB_LOAD_FORCE       :  1; /* 12..12, 0x00001000 */
		unsigned int rsv_13                      : 19; /* 13..31, 0xffffe000 */
	} Bits;
	unsigned int Raw;
};

#define REG_MRAW_M_IMGO_ORIWDMA_CON0        0x2350
#define REG_MRAW_M_IMGO_ORIWDMA_CON1        0x2354
#define REG_MRAW_M_IMGO_ORIWDMA_CON2        0x2358
#define REG_MRAW_M_IMGO_ORIWDMA_CON3        0x235c
#define REG_MRAW_M_IMGO_ORIWDMA_CON4        0x2360

#define REG_MRAW_M_IMGBO_ORIWDMA_CON0       0x2400
#define REG_MRAW_M_IMGBO_ORIWDMA_CON1       0x2404
#define REG_MRAW_M_IMGBO_ORIWDMA_CON2       0x2408
#define REG_MRAW_M_IMGBO_ORIWDMA_CON3       0x240c
#define REG_MRAW_M_IMGBO_ORIWDMA_CON4       0x2410

#define REG_MRAW_M_CPIO_ORIWDMA_CON0        0x24b0
#define REG_MRAW_M_CPIO_ORIWDMA_CON1        0x24b4
#define REG_MRAW_M_CPIO_ORIWDMA_CON2        0x24b8
#define REG_MRAW_M_CPIO_ORIWDMA_CON3        0x24bc
#define REG_MRAW_M_CPIO_ORIWDMA_CON4        0x24c0

#define REG_MRAW_M_LSCI_ORIRDMA_CON0        0x2120
#define REG_MRAW_M_LSCI_ORIRDMA_CON1        0x2124
#define REG_MRAW_M_LSCI_ORIRDMA_CON2        0x2128
#define REG_MRAW_M_LSCI_ORIRDMA_CON3        0x212c
#define REG_MRAW_M_LSCI_ORIRDMA_CON4        0x2130

#define REG_MRAW_M1_CQI_ORIRDMA_CON0        0x2190
#define REG_MRAW_M1_CQI_ORIRDMA_CON1        0x2194
#define REG_MRAW_M1_CQI_ORIRDMA_CON2        0x2198
#define REG_MRAW_M1_CQI_ORIRDMA_CON3        0x219c
#define REG_MRAW_M1_CQI_ORIRDMA_CON4        0x21A0


#define REG_MRAW_M2_CQI_ORIRDMA_CON0        0x2200
#define REG_MRAW_M2_CQI_ORIRDMA_CON1        0x2204
#define REG_MRAW_M2_CQI_ORIRDMA_CON2        0x2208
#define REG_MRAW_M2_CQI_ORIRDMA_CON3        0x220c
#define REG_MRAW_M2_CQI_ORIRDMA_CON4        0x2210



#define MRAWCTL_VS_INT_ST                        BIT(0)
#define MRAWCTL_TG_INT1_ST                       BIT(1)
#define MRAWCTL_TG_INT2_ST                       BIT(2)
#define MRAWCTL_TG_INT3_ST                       BIT(3)
#define MRAWCTL_TG_INT4_ST                       BIT(4)
#define MRAWCTL_EXPDON_ST                        BIT(5)
#define MRAWCTL_TG_ERR_ST                        BIT(6)
#define MRAWCTL_TG_GBERR_ST                      BIT(7)
#define MRAWCTL_SOF_INT_ST                       BIT(8)
#define MRAWCTL_SOF_WAIT_ST                      BIT(9)
#define MRAWCTL_SOF_DROP_ST                      BIT(10)
#define MRAWCTL_VS_INT_ORG_ST                    BIT(11)
#define MRAWCTL_CQ_DB_LOAD_ERR_ST                BIT(12)
#define MRAWCTL_CQ_MAIN_MAX_START_DLY_ERR_INT_ST BIT(13)
#define MRAWCTL_CQ_MAIN_CODE_ERR_ST              BIT(14)
#define MRAWCTL_CQ_MAIN_VS_ERR_ST                BIT(15)
#define MRAWCTL_CQ_MAIN_TRIG_DLY_INT_ST          BIT(16)
#define MRAWCTL_CQ_SUB_CODE_ERR_ST               BIT(17)
#define MRAWCTL_CQ_SUB_VS_ERR_ST                 BIT(18)
#define MRAWCTL_CQ_SUB_TRIG_DLY_INT_ST           BIT(19)
#define MRAWCTL_PASS1_DONE_ST                    BIT(20)
#define MRAWCTL_SW_PASS1_DONE_ST                 BIT(21)
#define MRAWCTL_SUB_PASS1_DONE_ST                BIT(22)
#define MRAWCTL_LSC_M1_FIFO_UFLOW_ST             BIT(23)
#define MRAWCTL_DMA_ERR_ST                       BIT(24)
#define MRAWCTL_SW_ENQUE_ERR_ST                  BIT(25)

#define MRAWCTL_IMGO_M1_OTF_OVERFLOW_ST          BIT(0)
#define MRAWCTL_IMGBO_M1_OTF_OVERFLOW_ST         BIT(1)
#define MRAWCTL_CPIO_M1_OTF_OVERFLOW_ST          BIT(2)
#define MRAWCTL_AFO_M1_OTF_OVERFLOW_ST           BIT(3)
#define MRAWCTL_TSFSO_M1_OTF_OVERFLOW_ST         BIT(4)
#define MRAWCTL_TSFSO_M2_OTF_OVERFLOW_ST         BIT(5)

/* IRQ Error Mask */
#define INT_ST_MASK_MRAW_ERR (\
					MRAWCTL_TG_ERR_ST |\
					MRAWCTL_TG_GBERR_ST |\
					MRAWCTL_CQ_DB_LOAD_ERR_ST)

/* Dma Error Mask */
#define DMA_ST_MASK_MRAW_ERR (\
					MRAWCTL_DMA_ERR_ST)

#endif	/* _MRAW_REGS_H */
