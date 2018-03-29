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

#ifndef __MTK_PHY_NEW_H
#define __MTK_PHY_NEW_H

#define CONFIG_U3D_HAL_SUPPORT
/* U3D_PHY_REF_CK = U3D_MAC_REF_CK on ASIC */
/* On FPGA, these two clocks are separated */
#ifdef CONFIG_U3D_HAL_SUPPORT
#define REF_CK 48
#else
#define REF_CK 25
#endif

/* include system library */
#include <linux/slab.h>
#include <linux/delay.h>

/* BASE ADDRESS DEFINE, should define this on ASIC */
#define PHY_BASE           0x0
#define SIFSLV_SPLLC_BASE  (PHY_BASE+0x0)
#define SIFSLV_FM_FEG_BASE  (PHY_BASE+0x100)
#define SIFSLV_CHIP_BASE    (PHY_BASE+0x700)
#define U2_PHY_BASE        (PHY_BASE+0x800)
#define U3_PHYD_BASE       (PHY_BASE+0x900)
#define U3_PHYD_B2_BASE    (PHY_BASE+0xa00)
#define U3_PHYA_BASE       (PHY_BASE+0xb00)
#define U3_PHYA_DA_BASE    (PHY_BASE+0xc00)


/* TYPE DEFINE */
typedef unsigned int PHY_UINT32;
typedef int PHY_INT32;
typedef unsigned short PHY_UINT16;
typedef short PHY_INT16;
typedef unsigned char PHY_UINT8;
typedef char PHY_INT8;

typedef PHY_UINT32 __bitwise PHY_LE32;

/* CONSTANT DEFINE */
#define PHY_FALSE	0
#define PHY_TRUE	1


/* #define DRV_MDELAY    mdelay */
#define DRV_MSLEEP	msleep
#define DRV_UDELAY	udelay
/* #define DRV_USLEEP    usleep */

/* PHY FUNCTION DEFINE, implemented in platform files, ex. ahb, gpio */
PHY_INT32 U3PhyWriteReg32(PHY_UINT32 addr, PHY_UINT32 data);
PHY_INT32 U3PhyReadReg32(PHY_UINT32 addr);
PHY_INT32 U3PhyWriteReg8(PHY_UINT32 addr, PHY_UINT8 data);
PHY_INT8 U3PhyReadReg8(PHY_UINT32 addr);

/* PHY GENERAL USAGE FUNC, implemented in mtk-phy.c */
PHY_INT32 U3PhyWriteField8(PHY_INT32 addr, PHY_INT32 offset, PHY_INT32 mask, PHY_INT32 value);
PHY_INT32 U3PhyWriteField32(PHY_INT32 addr, PHY_INT32 offset, PHY_INT32 mask, PHY_INT32 value);
PHY_INT32 U3PhyReadField8(PHY_INT32 addr, PHY_INT32 offset, PHY_INT32 mask);
PHY_INT32 U3PhyReadField32(PHY_INT32 addr, PHY_INT32 offset, PHY_INT32 mask);
void u3phy_writel(void __iomem *base, u32 reg, u32 offset, u32 mask, u32 value);
u32 u3phy_readl(void __iomem *base, u32 reg, u32 offset, u32 mask);

struct u3phy_reg_base {
	void __iomem *sif_base;
	/* void __iomem *sif2_base; */
	int phy_num;
};

struct u3phy_operator;

struct u3phy_info {
	/* used by non-project env. */
	PHY_INT32 phy_version;
	PHY_INT32 phyd_version_addr;

	/* used by project env. */
	struct u3phy_reg_base phy_regs;

	void *reg_info;
	const struct u3phy_operator *u3p_ops;

};

struct u3phy_operator {
	PHY_INT32 (*init)(struct u3phy_info *info);
	PHY_INT32 (*change_pipe_phase)(struct u3phy_info *info, PHY_INT32 phy_drv,
				       PHY_INT32 pipe_phase);
	PHY_INT32 (*eyescan_init)(struct u3phy_info *info);
	PHY_INT32 (*eyescan)(struct u3phy_info *info, PHY_INT32 x_t1, PHY_INT32 y_t1,
			     PHY_INT32 x_br, PHY_INT32 y_br, PHY_INT32 delta_x, PHY_INT32 delta_y,
			     PHY_INT32 eye_cnt, PHY_INT32 num_cnt, PHY_INT32 PI_cal_en,
			     PHY_INT32 num_ignore_cnt);
	PHY_INT32 (*u2_connect)(struct u3phy_info *info);
	PHY_INT32 (*u2_disconnect)(struct u3phy_info *info);
	PHY_INT32 (*u2_save_current_entry)(struct u3phy_info *info);
	PHY_INT32 (*u2_save_current_recovery)(struct u3phy_info *info);
	PHY_INT32 (*u2_slew_rate_calibration)(struct u3phy_info *info);
	void (*usb_phy_savecurrent)(struct u3phy_info *info, unsigned int clk_on);
	void (*usb_phy_recover)(struct u3phy_info *info, unsigned int clk_on);
};

#undef EXTERN
#ifdef U3_PHY_LIB
#define EXTERN
#else
#define EXTERN			/*extern */
#endif

#ifdef U3_PHY_LIB
struct u3phy_info *u3phy;
#else
extern struct u3phy_info *u3phy;
#endif



/*********eye scan required*********/

#define LO_BYTE(x)                   ((PHY_UINT8)((x) & 0xFF))
#define HI_BYTE(x)                   ((PHY_UINT8)(((x) & 0xFF00) >> 8))

typedef enum {
	SCAN_UP,
	SCAN_DN
} enumScanDir;

struct strucScanRegion {
	PHY_INT8 bX_tl;
	PHY_INT8 bY_tl;
	PHY_INT8 bX_br;
	PHY_INT8 bY_br;
	PHY_INT8 bDeltaX;
	PHY_INT8 bDeltaY;
};

struct strucTestCycle {
	PHY_UINT16 wEyeCnt;
	PHY_INT8 bNumOfEyeCnt;
	PHY_INT8 bPICalEn;
	PHY_INT8 bNumOfIgnoreCnt;
};

#define ERRCNT_MAX		128
#define CYCLE_COUNT_MAX	15

/* / the map resolution is 128 x 128 pts */
#define MAX_X                 127
#define MAX_Y                 127
#define MIN_X                 0
#define MIN_Y                 0

PHY_INT32 u3phy_init(struct u3phy_reg_base *regs);
void u3phy_exit(struct u3phy_reg_base *regs);


extern struct strucScanRegion _rEye1;
extern struct strucScanRegion _rEye2;
extern struct strucTestCycle _rTestCycle;
extern PHY_UINT8 _bXcurr;
extern PHY_UINT8 _bYcurr;
extern enumScanDir _eScanDir;
extern PHY_INT8 _fgXChged;
extern unsigned int _bPIResult;
extern PHY_UINT32 pwErrCnt0[CYCLE_COUNT_MAX][ERRCNT_MAX][ERRCNT_MAX];
extern PHY_UINT32 pwErrCnt1[CYCLE_COUNT_MAX][ERRCNT_MAX][ERRCNT_MAX];

/***********************************/
#endif
