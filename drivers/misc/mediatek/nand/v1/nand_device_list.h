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


#ifndef __NAND_DEVICE_LIST_H__
#define __NAND_DEVICE_LIST_H__

struct tag_nand_number {
	u32 number;
};

#define MAX_FLASH 20

#define NAND_MAX_ID		6
#define CHIP_CNT		21
#define P_SIZE		16384
#define P_PER_BLK		256
#define C_SIZE		8192
#define RAMDOM_READ		(1<<0)
#define CACHE_READ		(1<<1)
#define RAND_TYPE_SAMSUNG 0
#define RAND_TYPE_TOSHIBA 1
#define RAND_TYPE_NONE 2

#ifndef FALSE
  #define FALSE (0)
#endif

#ifndef TRUE
  #define TRUE  (1)
#endif

#define READ_RETRY_MAX 10
struct gFeature {
	u32 address;
	u32 feature;
};

enum readRetryType {
	RTYPE_MICRON,
	RTYPE_SANDISK,
	RTYPE_SANDISK_19NM,
	RTYPE_TOSHIBA,
	RTYPE_HYNIX,
	RTYPE_HYNIX_16NM,
	RTYPE_SANDISK_TLC_1YNM,
	RTYPE_TOSHIBA_TLC
};

struct gFeatureSet {
	u8 sfeatureCmd;
	u8 gfeatureCmd;
	u8 readRetryPreCmd;
	u8 readRetryCnt;
	u32 readRetryAddress;
	u32 readRetryDefault;
	u32 readRetryStart;
	enum readRetryType rtype;
	struct gFeature Interface;
	struct gFeature Async_timing;
};

struct gRandConfig {
	u8 type;
	u32 seed[6];
};

enum pptbl {
	MICRON_8K,
	HYNIX_8K,
	SANDISK_16K,
	PPTBL_NONE,
};

struct MLC_feature_set {
	enum pptbl ptbl_idx;
	struct gFeatureSet	FeatureSet;
	struct gRandConfig	randConfig;
};

enum flashdev_vendor {
	VEND_SAMSUNG,
	VEND_MICRON,
	VEND_TOSHIBA,
	VEND_HYNIX,
	VEND_SANDISK,
	VEND_BIWIN,
	VEND_NONE,
};

enum flashdev_IOWidth {
	IO_8BIT = 8,
	IO_16BIT = 16,
	IO_TOGGLEDDR = 9,
	IO_TOGGLESDR = 10,
	IO_ONFI = 12,
};

#define NAND_FLASH_SLC	(0x0000)
#define NAND_FLASH_MLC	(0x0001)
#define NAND_FLASH_TLC	(0x0002)
#define NAND_FLASH_MLC_HYBER	(0x0003)
#define NAND_FLASH_MASK	(0x00FF)

struct NFI_TLC_CTRL {
	bool		slcopmodeEn;				/*TRUE: slc mode	FALSE: tlc mode*/
	bool		pPlaneEn;					/*this chip has pseudo plane*/
	bool		needchangecolumn;			/*read page with change column address command*/
	bool		normaltlc;					/*whether need 09/0d 01/02/03*/
	u16		en_slc_mode_cmd;			/*enable slc mode cmd*/
	u16		dis_slc_mode_cmd;			/*disable slc mode cmd: 0xff is invalid*/
	bool		ecc_recalculate_en;		/*for nfi config*/
	u8		ecc_required;				/*required ecc bit*/
	u8		block_bit;					/*block address start bit;*/
	u8		pPlane_bit;				/*pesudo plane bit;*/
};

enum NFI_TLC_PG_CYCLE {
	PROGRAM_1ST_CYCLE = 1,
	PROGRAM_2ND_CYCLE = 2,
	PROGRAM_3RD_CYCLE = 3
};

enum NFI_TLC_WL_PRE {
	WL_LOW_PAGE = 0,
	WL_MID_PAGE = 1,
	WL_HIGH_PAGE = 2,
};

struct NFI_TLC_WL_INFO {
	u32 word_line_idx;
	enum NFI_TLC_WL_PRE wl_pre;
};
#endif

struct flashdev_info_t {
	u8 id[NAND_MAX_ID];
	u8 id_length;
	u8 addr_cycle;
	enum flashdev_IOWidth iowidth;
	u32 totalsize;
	u16 blocksize;
	u16 pagesize;
	u16 sparesize;
	u32 timmingsetting;
	u32 dqs_delay_ctrl;
	u32 s_acccon;
	u32 s_acccon1;
	u32 freq;
	enum flashdev_vendor vendor;
	u16 sectorsize;
	u8 devciename[30];
	u32 advancedmode;
	struct MLC_feature_set feature_set;
	u16 NAND_FLASH_TYPE;
	struct NFI_TLC_CTRL tlcControl;
};

#define flashdev_info struct flashdev_info_t
#define gen_FlashTable_p gen_FlashTable

static const struct flashdev_info_t gen_FlashTable[] = {
	{{0x45, 0xDE, 0x94, 0x93, 0x76, 0x57}, 6, 5, IO_8BIT, 0x800000, 4096, 16384, 1280,
		0x10401011, 0xC03222, 0x101, 0, 80, VEND_SANDISK, 1024, "SDTNQGAMA008G ", 0 ,
		{SANDISK_16K, {0xEF, 0xEE, 0xFF, 16, 0x11, 0, 1, RTYPE_SANDISK_19NM, {0x80, 0x00}, {0x80, 0x01} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_MLC, {FALSE, FALSE, FALSE, FALSE, 0xFF, 0xFF, FALSE, 0xFF, 8, 0xFF} },
	{{0x98, 0xD7, 0x84, 0x93, 0x72, 0x00}, 5, 5, IO_8BIT, 0x400000, 4096, 16384, 1280,
		0x10401011, 0xC03222, 0x101, 0, 80, VEND_TOSHIBA, 1024, "TC58TEG5DCKTA00", 0 ,
		{SANDISK_16K, {0xEF, 0xEE, 0xFF, 7, 0xFF, 7, 0, RTYPE_TOSHIBA, {0x80, 0x00}, {0x80, 0x01} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_MLC, {FALSE, FALSE, FALSE, FALSE, 0xFF, 0xFF, FALSE, 0xFF, 8, 0xFF} },
	{{0x45, 0xDE, 0x94, 0x93, 0x76, 0x00}, 5, 5, IO_8BIT, 0x800000, 4096, 16384, 1280,
		0x10804222, 0x60000, 0x33418010, 0x01010100, 100, VEND_SANDISK, 1024, "SDTNRGAMA008GK ", 0 ,
		{SANDISK_16K, {0xEF, 0xEE, 0x5D, 36, 0x11, 0, 0xFFFFFFFF, RTYPE_SANDISK, {0x80, 0x00}, {0x80, 0x01} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_MLC_HYBER, {FALSE, FALSE, FALSE, FALSE, 0xA2, 0xFF, FALSE, 0xFF, 8, 0xFF} },
	{{0xAD, 0xDE, 0x14, 0xA7, 0x42, 0x00}, 5, 5, IO_TOGGLESDR, 0x800000, 4096, 16384, 1600,
		0x10804222, 0x60000, 0x33418010, 0x01010100, 100, VEND_HYNIX, 1024, "H27UCG8T2ETR", 0 ,
		{SANDISK_16K, {0xEF, 0xEE, 0xFF, 8, 0xFF, 0, 0, RTYPE_HYNIX_16NM, {0x01, 0x20}, {0X01, 0x00} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_MLC_HYBER, {FALSE, FALSE, FALSE, FALSE, 0xBF, 0xBE, FALSE, 0xFF, 8, 0xFF} },
	{{0x2C, 0x44, 0x44, 0x4B, 0xA9, 0x00}, 5, 5, IO_8BIT, 0x400000, 2048, 8192, 640,
		0x10401011, 0xC03222, 0x101, 0, 80, VEND_MICRON, 1024, "MT29F32G08CBADB ", 0 ,
		{MICRON_8K, {0xEF, 0xEE, 0xFF, 7, 0x89, 0, 1, RTYPE_MICRON, {0x1, 0x14}, {0x1, 0x5} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_MLC, {FALSE, FALSE, FALSE, FALSE, 0xFF, 0xFF, FALSE, 0xFF, 8, 0xFF} },
	{{0xAD, 0xDE, 0x94, 0xA7, 0x42, 0x00}, 5, 5, IO_8BIT, 0x800000, 4096, 16384, 1280,
		0x10401011, 0xC03222, 0x101, 0, 80, VEND_BIWIN, 1024, "BW27UCG8T2ETR", 0 ,
		{SANDISK_16K, {0xFF, 0xFF, 0xFF, 7, 0xFF, 0, 1, RTYPE_HYNIX_16NM, {0XFF, 0xFF}, {0XFF, 0xFF} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_MLC, {FALSE, FALSE, FALSE, FALSE, 0xFF, 0xFF, FALSE, 0xFF, 8, 0xFF} },
	{{0x45, 0xD7, 0x84, 0x93, 0x72, 0x00}, 5, 5, IO_8BIT, 0x400000, 4096, 16384, 1280,
		0x10401011, 0xC03222, 0x101, 0, 80, VEND_SANDISK, 1024, "SDTNRGAMA004GK ", 0 ,
		{SANDISK_16K, {0xEF, 0xEE, 0x5D, 36, 0x11, 0, 0xFFFFFFFF, RTYPE_SANDISK, {0x80, 0x00}, {0x80, 0x01} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_MLC_HYBER, {FALSE, FALSE, FALSE, FALSE, 0xA2, 0xFF, FALSE, 0xFF, 8, 0xFF} },
	{{0x2C, 0x64, 0x44, 0x4B, 0xA9, 0x00}, 5, 5, IO_8BIT, 0x800000, 2048, 8192, 640,
		0x10401011, 0xC03222, 0x101, 0, 80, VEND_MICRON, 1024, "MT29F128G08CFABA ", 0 ,
		{MICRON_8K, {0xEF, 0xEE, 0xFF, 7, 0x89, 0, 1, RTYPE_MICRON, {0x1, 0x14}, {0x1, 0x5} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_MLC, {FALSE, FALSE, FALSE, FALSE, 0xFF, 0xFF, FALSE, 0xFF, 8, 0xFF} },
	{{0xAD, 0xD7, 0x94, 0x91, 0x60, 0x00}, 5, 5, IO_8BIT, 0x400000, 2048, 8192, 640,
		0x10401011, 0xC03222, 0x101, 0, 80, VEND_HYNIX, 1024, "H27UBG8T2CTR", 0 ,
		{HYNIX_8K, {0xFF, 0xFF, 0xFF, 7, 0xFF, 0, 1, RTYPE_HYNIX, {0XFF, 0xFF}, {0XFF, 0xFF} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_MLC, {FALSE, FALSE, FALSE, FALSE, 0xFF, 0xFF, FALSE, 0xFF, 8, 0xFF} },
	{{0x98, 0xDE, 0x94, 0x93, 0x76, 0x00}, 5, 5, IO_8BIT, 0x800000, 4096, 16384, 1280,
		0x10401011, 0xC03222, 0x101, 0, 80, VEND_TOSHIBA, 1024, "TC58TEG6DDKTA00", 0 ,
		{SANDISK_16K, {0xEF, 0xEE, 0xFF, 7, 0xFF, 7, 0, RTYPE_TOSHIBA, {0x80, 0x00}, {0x80, 0x01} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_MLC_HYBER, {FALSE, FALSE, FALSE, FALSE, 0xA2, 0xFF, FALSE, 0xFF, 8, 0xFF} },
	{{0x45, 0xDE, 0xA8, 0x92, 0x76, 0x00}, 5, 5, IO_8BIT, 0x83C580, 4128, 16384, 1952,
		0x10401011, 0xC03222, 0x101, 0, 80, VEND_SANDISK, 1024, "SDTNRCAMAOO8G", 0 ,
		{PPTBL_NONE, {0xEF, 0xEE, 0x5D, 39, 0x11, 0, 0, RTYPE_SANDISK_TLC_1YNM, {0x80, 0x00}, {0x80, 0x01} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_TLC, {TRUE, TRUE, TRUE, TRUE, 0xA2, 0xFF, TRUE, 68, 9, 8} },
	{{0x98, 0xDE, 0x88, 0xA3, 0x72, 0x00}, 5, 5, IO_8BIT, 0x85E000, 6144, 16384, 1952,
		0x10818022, 0x00110000, 0x101, 0, 100, VEND_TOSHIBA, 1024, "TC58TEG6TGLTA00", 0 ,
		{PPTBL_NONE, {0xEF, 0xEE, 0x5D, 39, 0x11, 0, 0, RTYPE_TOSHIBA_TLC, {0x80, 0x00}, {0x80, 0x01} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_TLC, {TRUE, FALSE, TRUE, TRUE, 0xA2, 0xFF, TRUE, 68, 8, 0} },
	{{0xAD, 0xDE, 0x18, 0x03, 0x00, 0x40}, 5, 5, IO_8BIT, 0x858000, 6144, 16384, 1952,
		0x10818022, 0xC03222, 0x101, 0, 80, VEND_HYNIX, 1024, "H27QCG882M5R", 0 ,
		{PPTBL_NONE, {0xEF, 0xEE, 0x5D, 39, 0x11, 0, 0, RTYPE_TOSHIBA_TLC, {0x80, 0x00}, {0x80, 0x01} },
		{RAND_TYPE_SAMSUNG, {0x2D2D, 1, 1, 1, 1, 1} } },
		NAND_FLASH_TLC, {TRUE, FALSE, FALSE, TRUE, 0xA2, 0xFF, TRUE, 68, 8, 0} },
};

