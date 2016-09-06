/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#ifndef __FPC1020_REGS_H
#define __FPC1020_REGS_H

typedef enum {
	/* --- Common registers --- */
	FPC102X_REG_FPC_STATUS			= 20,	/* RO, 1 bytes	*/
	FPC102X_REG_READ_INTERRUPT		= 24,	/* RO, 1 byte	*/
	FPC102X_REG_READ_INTERRUPT_WITH_CLEAR	= 28,	/* RO, 1 byte	*/
	FPC102X_REG_READ_ERROR_WITH_CLEAR	= 56,	/* RO, 1 byte	*/
	FPC102X_REG_MISO_EDGE_RIS_EN		= 64,	/* WO, 1 byte	*/
	FPC102X_REG_FPC_CONFIG			= 68,	/* RW, 1 byte	*/
	FPC102X_REG_IMG_SMPL_SETUP		= 76,	/* RW, 3 bytes	*/
	FPC102X_REG_CLOCK_CONFIG		= 80,	/* RW, 1 byte	*/
	FPC102X_REG_IMG_CAPT_SIZE		= 84,	/* RW, 4 bytes	*/
	FPC102X_REG_IMAGE_SETUP			= 92,	/* RW, 1 byte	*/
	FPC102X_REG_ADC_TEST_CTRL		= 96,	/* RW, 1 byte	*/
	FPC102X_REG_IMG_RD			= 100,	/* RW, 1 byte	*/
	FPC102X_REG_SAMPLE_PX_DLY		= 104,	/* RW, 8 bytes	*/
	FPC102X_REG_PXL_RST_DLY			= 108,	/* RW, 1 byte	*/
	FPC102X_REG_TST_COL_PATTERN_EN		= 120,	/* RW, 2 bytes	*/
	FPC102X_REG_CLK_BIST_RESULT		= 124,	/* RW, 4 bytes	*/
	FPC102X_REG_ADC_WEIGHT_SETUP		= 132,	/* RW, 1 byte	*/
	FPC102X_REG_ANA_TEST_MUX		= 136,	/* RW, 4 bytes	*/
	FPC102X_REG_FINGER_DRIVE_CONF		= 140,	/* RW, 1 byte	*/
	FPC102X_REG_FINGER_DRIVE_DLY		= 144,	/* RW, 1 byte	*/
	FPC102X_REG_OSC_TRIM			= 148,	/* RW, 2 bytes	*/
	FPC102X_REG_ADC_WEIGHT_TABLE		= 152,	/* RW, 10 bytes	*/
	FPC102X_REG_ADC_SETUP			= 156,	/* RW, 5 bytes	*/
	FPC102X_REG_ADC_SHIFT_GAIN		= 160,	/* RW, 2 bytes	*/
	FPC102X_REG_BIAS_TRIM			= 164,	/* RW, 1 byte	*/
	FPC102X_REG_PXL_CTRL			= 168,	/* RW, 2 bytes	*/
	FPC102X_REG_FPC_DEBUG			= 208,	/* RO, 1 bytes	*/
	FPC102X_REG_FINGER_PRESENT_STATUS	= 212,	/* RO, 2 bytes	*/
	FPC102X_REG_HWID			= 252,	/* RO, 2 bytes	*/
	/* --- fpc1020/21 specific --- */
	FPC1020_REG_FNGR_DET_THRES		= 216,	/* RW, 1 byte	*/
	FPC1020_REG_FNGR_DET_CNTR		= 220,	/* RW, 2 bytes	*/
	/* --- fpc1150 specific --- */
	FPC1150_REG_OFFSET			= 1000, /* Not a register ! */
	FPC1150_REG_FNGR_DET_THRES		= 1216,	/* RW, 4 byte	*/
	FPC1150_REG_FNGR_DET_CNTR		= 1220,	/* RW, 4 bytes	*/
	/* --- fpc1022 specific --- */
	FPC1022_REG_OFFSET			= 2000, /* Not a register ! */
	FPC1022_REG_CLOCK_CONFIG		= 2080, /* RW, 2 bytes */
	FPC1022_REG_SAMPLE_PX_DLY		= 2104, /* RW, 9 bytes */
	FPC1022_REG_CLK_BIST_RESULT		= 2124, /* RO, 8 bytes */
	FPC1022_REG_FINGER_PRESENT_STATUS	= 2212, /* RO, 4 bytes */
	FPC1022_REG_FNGR_DET_THRES		= 2216,	/* RW, 10 bytes	*/
	FPC1022_REG_FINGER_DRIVE_DLY		= 2144,	/* RW, 8 bytes	*/
	FPC1022_REG_FNGR_DET_CNTR		= 2220, /* RW, 4 bytes */

} fpc1020_reg_t;

#define FPC1020_REG_MAX_SIZE	10

#define FPC1020_REG_SIZE(reg) (	\
	((reg) == FPC102X_REG_FPC_STATUS) ?			1 : \
	((reg) == FPC102X_REG_READ_INTERRUPT) ?			1 : \
	((reg) == FPC102X_REG_READ_INTERRUPT_WITH_CLEAR) ?	1 : \
	((reg) == FPC102X_REG_READ_ERROR_WITH_CLEAR) ?		1 : \
	((reg) == FPC102X_REG_MISO_EDGE_RIS_EN) ?		1 : \
	((reg) == FPC102X_REG_FPC_CONFIG) ?			1 : \
	((reg) == FPC102X_REG_IMG_SMPL_SETUP) ?			3 : \
	((reg) == FPC102X_REG_CLOCK_CONFIG) ?			1 : \
	((reg) == FPC102X_REG_IMG_CAPT_SIZE) ?			4 : \
	((reg) == FPC102X_REG_IMAGE_SETUP) ?			1 : \
	((reg) == FPC102X_REG_ADC_TEST_CTRL) ?			1 : \
	((reg) == FPC102X_REG_IMG_RD) ?				1 : \
	((reg) == FPC102X_REG_SAMPLE_PX_DLY) ?			8 : \
	((reg) == FPC102X_REG_PXL_RST_DLY) ?			1 : \
	((reg) == FPC102X_REG_TST_COL_PATTERN_EN) ?		2 : \
	((reg) == FPC102X_REG_CLK_BIST_RESULT) ?		4 : \
	((reg) == FPC102X_REG_ADC_WEIGHT_SETUP) ?		1 : \
	((reg) == FPC102X_REG_ANA_TEST_MUX) ?			4 : \
	((reg) == FPC102X_REG_FINGER_DRIVE_CONF) ?		1 : \
	((reg) == FPC102X_REG_FINGER_DRIVE_DLY) ?		1 : \
	((reg) == FPC102X_REG_OSC_TRIM) ?			2 : \
	((reg) == FPC102X_REG_ADC_WEIGHT_TABLE) ?		10 :\
	((reg) == FPC102X_REG_ADC_SETUP) ?			5 : \
	((reg) == FPC102X_REG_ADC_SHIFT_GAIN) ?			2 : \
	((reg) == FPC102X_REG_BIAS_TRIM) ?			1 : \
	((reg) == FPC102X_REG_PXL_CTRL) ?			2 : \
	((reg) == FPC102X_REG_FPC_DEBUG) ?			2 : \
	((reg) == FPC102X_REG_FINGER_PRESENT_STATUS) ?		2 : \
	((reg) == FPC102X_REG_HWID) ?				2 : \
								    \
	((reg) == FPC1020_REG_FNGR_DET_THRES) ?			1 : \
	((reg) == FPC1020_REG_FNGR_DET_CNTR) ?			2 : \
								    \
	((reg) == FPC1150_REG_FNGR_DET_THRES) ?			4 : \
	((reg) == FPC1150_REG_FNGR_DET_CNTR) ?			4 : \
								    \
	((reg) == FPC1022_REG_CLOCK_CONFIG) ?			2 : \
	((reg) == FPC1022_REG_CLK_BIST_RESULT) ?		8 : \
	((reg) == FPC1022_REG_SAMPLE_PX_DLY) ?			9 : \
	((reg) == FPC1022_REG_FINGER_PRESENT_STATUS) ?		4 : \
	((reg) == FPC1022_REG_FINGER_DRIVE_DLY) ?		8 : \
	((reg) == FPC1022_REG_FNGR_DET_THRES) ?			10 : \
	((reg) == FPC1022_REG_FNGR_DET_CNTR) ?			4 : \
								0)

#define FPC1020_REG_ACCESS_DUMMY_BYTES(reg) (			\
	((reg) == FPC102X_REG_FPC_STATUS) ?			1 : \
	((reg) == FPC102X_REG_FPC_DEBUG) ?			1 : 0)

#define FPC1020_REG_TO_ACTUAL(reg) (				\
	((reg) >= FPC1022_REG_OFFSET) ? ((reg) - FPC1022_REG_OFFSET) : \
	((reg) >= FPC1150_REG_OFFSET) ? ((reg) - FPC1150_REG_OFFSET) : (reg))

#endif /* __FPC1020_REGS_H */


