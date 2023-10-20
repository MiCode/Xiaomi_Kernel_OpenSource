/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_PDA_HW_H__
#define __MTK_PDA_HW_H__

#include <linux/completion.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>

#include <linux/firmware.h>

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/io.h>

#define PDA_MAGIC               'P'

#define KERNEL_VERSION 1000

#define PDAROIARRAYMAX 128
#define FIXROIARRAYMAX 10

#define OUT_BYTE_PER_ROI 1408

#define PDA_MAXROI_PER_ROUND 45

#define PDA_MAX_QUANTITY 4

#define PDA_CFG_0_REG (0x000)
#define PDA_CFG_1_REG (0x004)
#define PDA_CFG_2_REG (0x008)
#define PDA_CFG_3_REG (0x00c)
#define PDA_CFG_4_REG (0x010)
#define PDA_CFG_5_REG (0x014)
#define PDA_CFG_6_REG (0x018)
#define PDA_CFG_7_REG (0x01c)
#define PDA_CFG_8_REG (0x020)
#define PDA_CFG_9_REG (0x024)
#define PDA_CFG_10_REG (0x028)
#define PDA_CFG_11_REG (0x02c)
#define PDA_CFG_12_REG (0x030)
#define PDA_CFG_13_REG (0x034)
#define PDA_CFG_14_REG (0x038)
#define PDA_CFG_15_REG (0x03c)
#define PDA_CFG_16_REG (0x040)
#define PDA_CFG_17_REG (0x044)
#define PDA_CFG_18_REG (0x048)
#define PDA_CFG_19_REG (0x04c)
#define PDA_CFG_20_REG (0x050)
#define PDA_CFG_21_REG (0x054)
#define PDA_CFG_22_REG (0x058)
#define PDA_CFG_23_REG (0x05c)
#define PDA_CFG_24_REG (0x060)
#define PDA_CFG_25_REG (0x064)
#define PDA_CFG_26_REG (0x068)
#define PDA_CFG_27_REG (0x06c)
#define PDA_CFG_28_REG (0x070)
#define PDA_CFG_29_REG (0x074)
#define PDA_CFG_30_REG (0x078)
#define PDA_CFG_31_REG (0x07c)
#define PDA_CFG_32_REG (0x080)
#define PDA_CFG_33_REG (0x084)
#define PDA_CFG_34_REG (0x088)
#define PDA_CFG_35_REG (0x08c)
#define PDA_CFG_36_REG (0x090)
#define PDA_CFG_37_REG (0x094)
#define PDA_CFG_38_REG (0x098)
#define PDA_CFG_39_REG (0x09c)
#define PDA_CFG_40_REG (0x0a0)
#define PDA_CFG_41_REG (0x0a4)
#define PDA_CFG_42_REG (0x0a8)
#define PDA_CFG_43_REG (0x0ac)
#define PDA_CFG_44_REG (0x0b0)
#define PDA_CFG_45_REG (0x0b4)
#define PDA_CFG_46_REG (0x0b8)
#define PDA_CFG_47_REG (0x0bc)
#define PDA_CFG_48_REG (0x0c0)
#define PDA_CFG_49_REG (0x0c4)
#define PDA_CFG_50_REG (0x0c8)
#define PDA_CFG_51_REG (0x0cc)
#define PDA_CFG_52_REG (0x0d0)
#define PDA_CFG_53_REG (0x0d4)
#define PDA_CFG_54_REG (0x0d8)
#define PDA_CFG_55_REG (0x0dc)
#define PDA_CFG_56_REG (0x0e0)
#define PDA_CFG_57_REG (0x0e4)
#define PDA_CFG_58_REG (0x0e8)
#define PDA_CFG_59_REG (0x0ec)
#define PDA_CFG_60_REG (0x0f0)
#define PDA_CFG_61_REG (0x0f4)
#define PDA_CFG_62_REG (0x0f8)
#define PDA_CFG_63_REG (0x0fc)
#define PDA_CFG_64_REG (0x100)
#define PDA_CFG_65_REG (0x104)
#define PDA_CFG_66_REG (0x108)
#define PDA_CFG_67_REG (0x10c)
#define PDA_CFG_68_REG (0x110)
#define PDA_CFG_69_REG (0x114)
#define PDA_CFG_70_REG (0x118)
#define PDA_CFG_71_REG (0x11c)
#define PDA_CFG_72_REG (0x120)
#define PDA_CFG_73_REG (0x124)
#define PDA_CFG_74_REG (0x128)
#define PDA_CFG_75_REG (0x12c)
#define PDA_CFG_76_REG (0x130)
#define PDA_CFG_77_REG (0x134)
#define PDA_CFG_78_REG (0x138)
#define PDA_CFG_79_REG (0x13c)
#define PDA_CFG_80_REG (0x140)
#define PDA_CFG_81_REG (0x144)
#define PDA_CFG_82_REG (0x148)
#define PDA_CFG_83_REG (0x14c)
#define PDA_CFG_84_REG (0x150)
#define PDA_CFG_85_REG (0x154)
#define PDA_CFG_86_REG (0x158)
#define PDA_CFG_87_REG (0x15c)
#define PDA_CFG_88_REG (0x160)
#define PDA_CFG_89_REG (0x164)
#define PDA_CFG_90_REG (0x168)
#define PDA_CFG_91_REG (0x16c)
#define PDA_CFG_92_REG (0x170)
#define PDA_CFG_93_REG (0x174)
#define PDA_CFG_94_REG (0x178)
#define PDA_CFG_95_REG (0x17c)
#define PDA_CFG_96_REG (0x180)
#define PDA_CFG_97_REG (0x184)
#define PDA_CFG_98_REG (0x188)
#define PDA_CFG_99_REG (0x18c)
#define PDA_CFG_100_REG (0x190)
#define PDA_CFG_101_REG (0x194)
#define PDA_CFG_102_REG (0x198)
#define PDA_CFG_103_REG (0x19c)
#define PDA_CFG_104_REG (0x1a0)
#define PDA_CFG_105_REG (0x1a4)
#define PDA_CFG_106_REG (0x1a8)
#define PDA_CFG_107_REG (0x1ac)
#define PDA_CFG_108_REG (0x1b0)
#define PDA_CFG_109_REG (0x1b4)
#define PDA_CFG_110_REG (0x1b8)
#define PDA_CFG_111_REG (0x1bc)
#define PDA_CFG_112_REG (0x1c0)
#define PDA_CFG_113_REG (0x1c4)
#define PDA_CFG_114_REG (0x1c8)
#define PDA_CFG_115_REG (0x1cc)
#define PDA_CFG_116_REG (0x1d0)
#define PDA_CFG_117_REG (0x1d4)
#define PDA_CFG_118_REG (0x1d8)
#define PDA_CFG_119_REG (0x1dc)
#define PDA_CFG_120_REG (0x1e0)
#define PDA_CFG_121_REG (0x1e4)
#define PDA_CFG_122_REG (0x1e8)
#define PDA_CFG_123_REG (0x1ec)
#define PDA_CFG_124_REG (0x1f0)
#define PDA_CFG_125_REG (0x1f4)
#define PDA_CFG_126_REG (0x1f8)
#define PDA_PDAI_P1_BASE_ADDR_REG (0x300)
#define PDA_PDATI_P1_BASE_ADDR_REG (0x304)
#define PDA_PDAI_P2_BASE_ADDR_REG (0x308)
#define PDA_PDATI_P2_BASE_ADDR_REG (0x30c)
#define PDA_PDAI_STRIDE_REG (0x310)
#define PDA_PDAI_P1_CON0_REG (0x314)
#define PDA_PDAI_P1_CON1_REG (0x318)
#define PDA_PDAI_P1_CON2_REG (0x31c)
#define PDA_PDAI_P1_CON3_REG (0x320)
#define PDA_PDAI_P1_CON4_REG (0x324)
#define PDA_PDATI_P1_CON0_REG (0x328)
#define PDA_PDATI_P1_CON1_REG (0x32c)
#define PDA_PDATI_P1_CON2_REG (0x330)
#define PDA_PDATI_P1_CON3_REG (0x334)
#define PDA_PDATI_P1_CON4_REG (0x338)
#define PDA_PDAI_P2_CON0_REG (0x33c)
#define PDA_PDAI_P2_CON1_REG (0x340)
#define PDA_PDAI_P2_CON2_REG (0x344)
#define PDA_PDAI_P2_CON3_REG (0x348)
#define PDA_PDAI_P2_CON4_REG (0x34c)
#define PDA_PDATI_P2_CON0_REG (0x350)
#define PDA_PDATI_P2_CON1_REG (0x354)
#define PDA_PDATI_P2_CON2_REG (0x358)
#define PDA_PDATI_P2_CON3_REG (0x35c)
#define PDA_PDATI_P2_CON4_REG (0x360)
#define PDA_PDAO_P1_BASE_ADDR_REG (0x364)
#define PDA_PDAO_P1_XSIZE_REG (0x368)
#define PDA_PDAO_P1_CON0_REG (0x36c)
#define PDA_PDAO_P1_CON1_REG (0x370)
#define PDA_PDAO_P1_CON2_REG (0x374)
#define PDA_PDAO_P1_CON3_REG (0x378)
#define PDA_PDAO_P1_CON4_REG (0x37c)
#define PDA_PDA_DMA_EN_REG (0x380)
#define PDA_PDA_DMA_RST_REG (0x384)
#define PDA_PDA_DMA_TOP_REG (0x388)
#define PDA_PDA_SECURE_REG (0x38c)
#define PDA_PDA_TILE_STATUS_REG (0x390)
#define PDA_PDA_DCM_DIS_REG (0x394)
#define PDA_PDA_DCM_ST_REG (0x398)
#define PDA_PDAI_P1_ERR_STAT_REG (0x39c)
#define PDA_PDATI_P1_ERR_STAT_REG (0x3a0)
#define PDA_PDAI_P2_ERR_STAT_REG (0x3a4)
#define PDA_PDATI_P2_ERR_STAT_REG (0x3a8)
#define PDA_PDAO_P1_ERR_STAT_REG (0x3ac)
#define PDA_PDA_ERR_STAT_EN_REG (0x3b0)
#define PDA_PDA_ERR_STAT_REG (0x3b4)
#define PDA_PDA_TOP_CTL_REG (0x3b8)
#define PDA_PDA_DEBUG_SEL_REG (0x3bc)
#define PDA_PDA_IRQ_TRIG_REG (0x3c0)
#define PDA_PDAI_P1_BASE_ADDR_MSB_REG (0x3c4)
#define PDA_PDATI_P1_BASE_ADDR_MSB_REG (0x3c8)
#define PDA_PDAI_P2_BASE_ADDR_MSB_REG (0x3cc)
#define PDA_PDATI_P2_BASE_ADDR_MSB_REG (0x3d0)
#define PDA_PDAO_P1_BASE_ADDR_MSB_REG (0x3d4)
#define PDA_PDAO_DMA_EXISTED_ECO_REG (0x3d8)
#define PDA_PDA_DEBUG_DATA_REG (0x3dc)

union _REG_PDA_CFG_0_ {
	struct /* 0x0000 */
	{
		unsigned int  PDA_WIDTH        :  16;   /*  0.. 16, 0x0000FFFF */
		unsigned int  PDA_HEIGHT       :  16;   /*  16.. 32, 0xFFFF0080 */
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_1_ {
	struct /* 0x0004 */
	{
		unsigned int  PDA_EN           :  1;    /*  0.. 0, 0x00000001 */
		unsigned int  PDA_PR_XNUM      :  5;    /*  1.. 5, 0x0000003E */
		unsigned int  PDA_PR_YNUM      :  5;    /*  6.. 10, 0x00007C0 */
		unsigned int  PDA_PAT_WIDTH    :  10;   /*  11.. 20, 0x001FF800 */
		unsigned int  PDA_BIN_FCTR     :  2;    /*  21.. 22, 0x00600000 */
		unsigned int  PDA_RNG_ST       :  6;    /*  23.. 28, 0x1F800080 */
		unsigned int  PDA_SHF_0        :  3;    /*  29.. 31, 0xE0000000 */
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_2_ {
	struct /* 0x0008 */
	{
		unsigned int  PDA_SHF_1        :  3;
		unsigned int  PDA_SHF_2        :  3;
		unsigned int  PDA_RGN_NUM      :  6;
		unsigned int  PDA_TBL_STRIDE   :  16;
		unsigned int  rsv_28           :  4;
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_3_ {
	struct /* 0x000C */
	{
		unsigned int  PDA_POS_L_0      :  9;
		unsigned int  PDA_POS_L_1      :  9;
		unsigned int  PDA_POS_L_2      :  9;
		unsigned int  rsv_27           :  5;
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_4_ {
	struct /* 0x0010 */
	{
		unsigned int  PDA_POS_L_3      :  9;
		unsigned int  PDA_POS_L_4      :  9;
		unsigned int  PDA_POS_L_5      :  9;
		unsigned int  rsv_27           :  5;
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_5_ {
	struct /* 0x0014 */
	{
		unsigned int  PDA_POS_L_6      :  9;
		unsigned int  PDA_POS_L_7      :  9;
		unsigned int  PDA_POS_L_8      :  9;
		unsigned int  rsv_27           :  5;
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_6_ {
	struct /* 0x0018 */
	{
		unsigned int  PDA_POS_L_9      :  9;
		unsigned int  PDA_POS_L_10     :  9;
		unsigned int  PDA_POS_L_11     :  9;
		unsigned int  rsv_27           :  5;
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_7_ {
	struct /* 0x001C */
	{
		unsigned int  PDA_POS_L_12     :  9;
		unsigned int  PDA_POS_L_13     :  9;
		unsigned int  PDA_POS_L_14     :  9;
		unsigned int  rsv_27           :  5;
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_8_ {
	struct /* 0x0020 */
	{
		unsigned int  PDA_POS_L_15     :  9;
		unsigned int  PDA_POS_R_0      :  9;
		unsigned int  PDA_POS_R_1      :  9;
		unsigned int  rsv_27           :  5;
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_9_ {
	struct /* 0x0024 */
	{
		unsigned int  PDA_POS_R_2      :  9;
		unsigned int  PDA_POS_R_3      :  9;
		unsigned int  PDA_POS_R_4      :  9;
		unsigned int  rsv_27           :  5;
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_10_ {
	struct /* 0x0028 */
	{
		unsigned int  PDA_POS_R_5      :  9;
		unsigned int  PDA_POS_R_6      :  9;
		unsigned int  PDA_POS_R_7      :  9;
		unsigned int  rsv_27           :  5;
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_11_ {
	struct /* 0x002C */
	{
		unsigned int  PDA_POS_R_8      :  9;
		unsigned int  PDA_POS_R_9      :  9;
		unsigned int  PDA_POS_R_10     :  9;
		unsigned int  rsv_27           :  5;
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_12_ {
	struct /* 0x0030 */
	{
		unsigned int  PDA_POS_R_11     :  9;
		unsigned int  PDA_POS_R_12     :  9;
		unsigned int  PDA_POS_R_13     :  9;
		unsigned int  rsv_27           :  5;
	} Bits;
	unsigned int Raw;
};

union _REG_PDA_CFG_13_ {
	struct /* 0x0034 */
	{
		unsigned int  PDA_POS_R_14     :  9;
		unsigned int  PDA_POS_R_15     :  9;
		unsigned int  rsv_18           :  14;
	} Bits;
	unsigned int Raw;
};

struct _pda_a_reg_t_ {
	union _REG_PDA_CFG_0_            PDA_CFG_0;
	union _REG_PDA_CFG_1_            PDA_CFG_1;
	union _REG_PDA_CFG_2_            PDA_CFG_2;
	union _REG_PDA_CFG_3_            PDA_CFG_3;
	union _REG_PDA_CFG_4_            PDA_CFG_4;
	union _REG_PDA_CFG_5_            PDA_CFG_5;
	union _REG_PDA_CFG_6_            PDA_CFG_6;
	union _REG_PDA_CFG_7_            PDA_CFG_7;
	union _REG_PDA_CFG_8_            PDA_CFG_8;
	union _REG_PDA_CFG_9_            PDA_CFG_9;
	union _REG_PDA_CFG_10_           PDA_CFG_10;
	union _REG_PDA_CFG_11_           PDA_CFG_11;
	union _REG_PDA_CFG_12_           PDA_CFG_12;
	union _REG_PDA_CFG_13_           PDA_CFG_13;
};

struct PDA_Init_Data {
	unsigned int Kversion;
};

//Datastructure for 1024 ROI
struct PDA_Data_t {
	unsigned int rgn_x[PDAROIARRAYMAX];
	unsigned int rgn_y[PDAROIARRAYMAX];
	unsigned int rgn_h[PDAROIARRAYMAX];
	unsigned int rgn_w[PDAROIARRAYMAX];
	unsigned int rgn_iw[PDAROIARRAYMAX];

	unsigned int xnum;
	unsigned int ynum;
	unsigned int woverlap;
	unsigned int hoverlap;

	unsigned int ImageSize;
	unsigned int TableSize;
	unsigned int OutputSize;
	unsigned int ROInumber;

	int FD_L_Image;
	int FD_R_Image;
	int FD_L_Table;
	int FD_R_Table;
	int FD_Output;

	struct _pda_a_reg_t_ PDA_FrameSetting;

	unsigned int PDA_PDAI_P1_BASE_ADDR;
	unsigned int PDA_PDATI_P1_BASE_ADDR;
	unsigned int PDA_PDAI_P2_BASE_ADDR;
	unsigned int PDA_PDATI_P2_BASE_ADDR;
	unsigned int PDA_PDAO_P1_BASE_ADDR;

	int Status;
	unsigned int Timeout;

	unsigned int nNumerousROI;
};

struct PDA_Data_t_v2 {
	// flexible roi
	unsigned int rgn_x[PDAROIARRAYMAX];
	unsigned int rgn_y[PDAROIARRAYMAX];
	unsigned int rgn_h[PDAROIARRAYMAX];
	unsigned int rgn_w[PDAROIARRAYMAX];
	unsigned int rgn_iw[PDAROIARRAYMAX];

	unsigned int ROInumber;

	// fix roi
	unsigned int fix_rgn_x[FIXROIARRAYMAX];
	unsigned int fix_rgn_y[FIXROIARRAYMAX];
	unsigned int fix_rgn_h[FIXROIARRAYMAX];
	unsigned int fix_rgn_w[FIXROIARRAYMAX];
	unsigned int fix_rgn_iw[FIXROIARRAYMAX];

	unsigned int xnum[FIXROIARRAYMAX];
	unsigned int ynum[FIXROIARRAYMAX];
	unsigned int woverlap[FIXROIARRAYMAX];
	unsigned int hoverlap[FIXROIARRAYMAX];

	unsigned int nNumerousROI;

	// common data
	unsigned int ImageSize;
	unsigned int TableSize;
	unsigned int OutputSize;

	int FD_L_Image;
	int FD_R_Image;
	int FD_L_Table;
	int FD_R_Table;
	int FD_Output;

	struct _pda_a_reg_t_ PDA_FrameSetting;

	unsigned int PDA_PDAI_P1_BASE_ADDR;
	unsigned int PDA_PDATI_P1_BASE_ADDR;
	unsigned int PDA_PDAI_P2_BASE_ADDR;
	unsigned int PDA_PDATI_P2_BASE_ADDR;
	unsigned int PDA_PDAO_P1_BASE_ADDR;

	int Status;
	unsigned int Timeout;
};

struct pda_mmu {
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attach;
	struct sg_table			*sgt;
};

// PDA clock info.
struct PDA_CLK_STRUCT {
	struct clk *CG_PDA_TOP_MUX;
};

// pda device information
struct PDA_device {
	int HWstatus;
	void __iomem *m_pda_base;
	int irq;
};

enum PDA_CMD_ENUM {
	PDA_CMD_RESET,		/* Reset */
	PDA_CMD_ENQUE_WAITIRQ,	/* PDA Enque And Wait Irq */
	PDA_CMD_GET_VERSION,	/* PDA Enque And Wait Irq */
	PDA_CMD_TOTAL,
};

#define PDA_RESET	_IO(PDA_MAGIC, PDA_CMD_RESET)
#define PDA_ENQUE_WAITIRQ    \
	_IOWR(PDA_MAGIC, PDA_CMD_ENQUE_WAITIRQ, struct PDA_Data_t)
#define PDA_ENQUE_WAITIRQ_V2    \
	_IOWR(PDA_MAGIC, PDA_CMD_ENQUE_WAITIRQ, struct PDA_Data_t_v2)
#define PDA_GET_VERSION    \
	_IOWR(PDA_MAGIC, PDA_CMD_GET_VERSION, struct PDA_Init_Data)

#endif/*__MTK_PDA_HW_H__*/
