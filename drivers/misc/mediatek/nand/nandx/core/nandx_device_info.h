/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */

#ifndef __NANDX_DEVICE_INFO_H__
#define __NANDX_DEVICE_INFO_H__

#include "nandx_util.h"

#define TABLE_MAX_NUM	(32)
#define NAME_MAX_LEN	(32)
#define ID_MAX_NUM	(6)
#define NONE		(0xff)

enum VENDOR_TYPE {
	VENDOR_TOSHIBA,
	VENDOR_SANDISK,
	VENDOR_HYNIX,
	VENDOR_MICRON,
	VENDOR_SAMSUNG,
	VENDOR_NUM
};

enum NAND_TYPE {
	NAND_SLC,
	NAND_MLC,
	NAND_TLC,
	NAND_VMLC,
	NAND_VTLC,
};

enum IO_WIDTH {
	IO_8BIT,
	IO_16BIT
};

enum MODE_TYPE {
	MODE_SLC_A2 = 1,
	MODE_SLC_DA = 2,
	MODE_LOWER_PAGE_HYNIX = 4,
	MODE_LOWER_PAGE_SANDISK = 8,
	MODE_LOWER_PAGE_MICRON = 16,
	MODE_SLC_FEATURE_MICRON = 32,
	MODE_LOWER_PAGE_L95B = 64,
};

enum ADDR_CYCLE {
	ADDR_CYCLE5 = 5,
	ADDR_CYCLE6
};

enum FEATURE_ADDRESS {
	FEATURE_ODT,
	FEATURE_DRIVE_STRENGTH,
	FEATURE_EXTERNAL_VPP,
	FEATURE_INTERFACE_CHANGE,
	FEATURE_DQS_LATENCY,
	FEATURE_SLEW_RATE,
	FEATURE_TIMING_MODE,
	FEATURE_RB_STRENGTH,
	FEATURE_NV_DDR,
	FEATURE_READ_RETRY,
	FEATURE_ADDRESS_NUM
};

enum INTERFACE_FEATURE_VALUE {
	INFTYPE,		/* interface type bit */
	TMODE,			/* timing mode bit */
};

enum ADDRESS_TABLE {
	ADDRESSING_A,
	ADDRESSING_B,
	ADDRESSING_C,
	ADDRESSING_D,
	ADDRESSING_E,
	ADDRESSING_F,
	ADDRESSING_G,
	ADDRESSING_H,
	ADDRESSING_I,
	ADDRESSING_J,
	ADDRESSING_K,
	ADDRESSING_L,
	ADDRESSING_M,
	ADDRESSING_N,
	ADDRESSING_TABLE_NUM
};

enum ADDRESSING_INDEX {
	ADDR_ROW_START,
	ADDR_ROW_LEN,
	ADDR_LOGICAL_PLANE_START,
	ADDR_LOGICAL_PLANE_LEN,
	ADDR_PLANE_START,
	ADDR_PLANE_LEN,
	ADDR_BLOCK_START,
	ADDR_BLOCK_LEN,
	ADDR_LUN_START,
	ADDR_LUN_LEN,
	ADDR_INDEX_NUM
};

enum BASIC_CMD_SETS {
	CMD_RESET,
	CMD_RESET_LUN,
	CMD_RESET_SYNC,
	CMD_READ_ID,
	CMD_READ_STATUS,
	CMD_READ_ENHANCE_STATUS,
	CMD_READ_PARAMETERS_PAGE,
	CMD_SET_FEATURE,
	CMD_GET_FEATURE,
	CMD_SET_LUN_FEATURE,
	CMD_GET_LUN_FEATURE,
	CMD_READ_1ST,
	CMD_READ_2ND,
	CMD_CACHE_READ_2ND,
	CMD_CACHE_READ_LAST,
	CMD_MULTI_READ_2ND,
	CMD_RANDOM_OUT_1ST,
	CMD_RANDOM_OUT_2ND,
	CMD_PROGRAM_1ST,
	CMD_PROGRAM_2ND,
	CMD_CACHE_PROGRAM_2ND,
	CMD_MULTI_PROGRAM_2ND,
	CMD_MULTI2_PROGRAM_1ST,
	CMD_MULTI2_PROGRAM_2ND,
	CMD_BLOCK_ERASE0,
	CMD_BLOCK_ERASE1,
	CMD_NUM
};

enum EXTEND_CMD_TYPE {
	EXTEND_TLC_PRE_CMD,
	EXTEND_TLC_PROGRAM_ORDER_CMD,
	EXTEND_CMD_TYPE_NUM,
	EXTEND_CMD_NONE,
	EXTEND_CMD_SETS_NUM = EXTEND_CMD_NONE,
};

enum DRIVE_STRENGTH_LEVEL {
	DRIVE_LEVEL_DEFAULT,
	DRIVE_LEVEL_LOW,
	DRIVE_LEVEL_HIGH,
	DRIVE_LEVEL_MORE,
	DRIVE_LEVEL_NUM,
};

enum DRIVE_STRENGTH_TYPE {
	DRIVE_STRENGTH_MICRON,
	DRIVE_STRENGTH_TOSHIBA,
	DRIVE_STRENGTH_HYNIX,
	DRIVE_STRENGTH_TYPE_NUM
};

enum BAD_BLOCK_TYPE {
	BAD_BLOCK_READ_OOB,
	BAD_BLOCK_READ_PAGE,
	BAD_BLOCK_SLC_PROGRAM,
	BAD_BLOCK_READ_UPPER_PAGE
};

enum READ_RETRY_TYPE {
	RR_TYPE_NONE,
	RR_MLC_MICRON,
	RR_MLC_MICRON_L95B,
	RR_MLC_SANDISK,
	RR_MLC_SANDISK_19NM,
	RR_MLC_SANDISK_1ZNM,
	RR_MLC_TOSHIBA_19NM,
	RR_MLC_TOSHIBA_15NM,
	RR_MLC_HYNIX_20NM,
	RR_MLC_HYNIX_1XNM,
	RR_MLC_HYNIX_16NM,
	RR_TLC_SANDISK_1YNM,
	RR_TLC_SANDISK_1ZNM,
	RR_TLC_TOSHIBA_15NM,
	RR_TYPE_NUM
};

enum INTERFACE_TYPE {
	INTERFACE_LEGACY,
	INTERFACE_ONFI,
	INTERFACE_TOGGLE,
	INTERFACE_NUM,
};

enum SDR_TIMING_TYPE {
	SDR_TIMING_SANDISK_TLC,
	SDR_TIMING_TOSHIBA_TLC,
	SDR_TIMING_MICRON_VMLC,
	SDR_TIMING_MICRON_VMLC_MODE2,
	SDR_TIMING_TYPE_NUM
};

enum ONFI_TIMING_TYPE {
	ONFI_TIMING_MICRON_VMLC,
	ONFI_TIMING_TYPE_NUM
};

enum ONFI_CLOCK_MODE {
	ONFI_CLOCK_MODE0,
	ONFI_CLOCK_MODE1,
	ONFI_CLOCK_MODE2,
	ONFI_CLOCK_MODE3,
	ONFI_CLOCK_MODE4,
	ONFI_CLOCK_MODE5,
	ONFI_CLOCK_NUM
};

enum TOGGLE_TIMING_TYPE {
	TOGGLE_TIMING_SANDISK_TLC,
	TOGGLE_TIMING_TOSHIBA_TLC,
	TOGGLE_TIMING_TYPE_NUM
};

enum TOGGLE_CLOCK_MODE {
	TOGGLE_CLOCK_MODE0,
	TOGGLE_CLOCK_MODE1,
	TOGGLE_CLOCK_MODE2,
	TOGGLE_CLOCK_MODE3,
	TOGGLE_CLOCK_MODE4,
	TOGGLE_CLOCK_NUM
};

struct nand_replace_cmd {
	enum VENDOR_TYPE type;
	enum BASIC_CMD_SETS bcmd;
	u8 rcmd;
};

struct nand_life_info {
	u32 pe_cycle;
	u32 ecc_required;
	u32 bitflips_threshold;
};

/* All these timings are expressed in nanoseconds. */
struct nandx_legacy_timing {
	u16 tREA;
	u16 tREH;
	u16 tCR;
	u16 tRP;
	u16 tWP;
	u16 tWH;
	u16 tWHR;
	u16 tCLS;
	u16 tALS;
	u16 tCLH;
	u16 tALH;
	u16 tWC;
	u16 tRC;
};

struct nandx_onfi_timing {
	u16 tCAD;
	u16 tWPRE;
	u16 tWPST;
	u16 tWRCK;
	u16 tDQSCK;
	u16 tWHR;
};

struct nandx_toggle_timing {
	u16 tCS;
	u16 tCH;
	u16 tCAS;
	u16 tCAH;
	u16 tCALS;
	u16 tCALH;
	u16 tWP;
	u16 tWPRE;
	u16 tWPST;
	u16 tWPSTH;
	u16 tCR;
	u16 tRPRE;
	u16 tRPST;
	u16 tRPSTH;
	u16 tCDQSS;
	u16 tWHR;
};

struct nand_timing {
	struct nandx_legacy_timing *legacy;
	union {
		struct nandx_onfi_timing *onfi;
		struct nandx_toggle_timing *toggle;
	} ddr;
	u32 ddr_clock;
};

/*
 * Notice:
 * 1. Please always add item at first line
 * 2. Must comment the modified_date: yours_name when
 *    you add a new Nand information
 * 3. Please check the max item numbers:TABLE_MAX_NUM
 */
struct nandx_device_info {
	char name[NAME_MAX_LEN];
	u8 id[ID_MAX_NUM];
	u8 type;		/* enum NAND_TYPE */
	u8 io_width;		/* enum IO_WIDTH */
	u8 addr_cycle;
	u8 target_num;
	u8 lun_num;
	u8 plane_num;
	u32 block_num;		/* single plane */
	u32 block_size;
	u32 page_size;
	u32 spare_size;
	u32 mode_type;		/* enum MODE_TYPE, bitmap */
	u8 program_order_type;	/* enum PROGRAM_ORDER */
	u8 address_table_type;	/* enum ADDRESS_TABLE */
	u8 bad_block_type;	/* enum BAD_BLOCK_TYPE */
	u8 read_retry_type;	/* enum READ_RETRY_TYPE */
	u8 interface_type;	/* enum INTERFACE_TYPE */
	u8 sdr_timing_type;	/* enum SDR_TIMING_TYPE */
	u8 ddr_timing_type;	/* enum ONFI_TIMING_TYPE TOGGLE_TIMING_TYPE */
	u8 ddr_clock_type;	/* enum ONFI_CLOCK_MODE TOGGLE_CLOCK_MODE */
	struct nand_life_info slc_life;
	struct nand_life_info xlc_life;
};

/*
 ********************************
 * operations
 ********************************
 */
struct nandx_device_info *get_nandx_device_info(u8 *id, int num);
u8 *get_basic_cmd_sets(void);
u8 get_extend_cmd(enum EXTEND_CMD_TYPE type, int cycle);
void set_replace_cmd(enum VENDOR_TYPE type);
u32 get_physical_row_address(u8 *addr, u32 lun, u32 plane, u32 block, u32 wl);
u8 *get_nandx_feature_table(enum VENDOR_TYPE vtype);
u8 *get_nandx_addressing_table(enum ADDRESS_TABLE type);
u8 *get_nandx_drive_strength_table(enum DRIVE_STRENGTH_TYPE type);
u8 get_vendor_type(u8 id);
void *get_nandx_timing(enum INTERFACE_TYPE type, u8 timing_type);
u8 get_nandx_interface_value(enum VENDOR_TYPE vtype, u8 timing_mode,
			     enum INTERFACE_TYPE itype);
u32 get_nandx_ddr_clock(enum INTERFACE_TYPE type, u8 clock_type);

#endif				/* __NANDX_DEVICE_INFO_H__ */
