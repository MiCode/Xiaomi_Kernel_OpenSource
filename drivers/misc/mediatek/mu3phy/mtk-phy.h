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
#ifdef CONFIG_U3D_HAL_SUPPORT
#include "mu3d_hal_hw.h"
#endif
#ifdef CONFIG_U3D_HAL_SUPPORT
#define REF_CK U3D_PHY_REF_CK
#else
#define REF_CK 25
#endif

/* include system library */
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>

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

/*
0x00000100	MODULE	ssusb_sifslv_fmreg	ssusb_sifslv_fmreg
0x00000700	MODULE	ssusb_sifslv_ippc	ssusb_sifslv_ippc
0x00000800	MODULE	ssusb_sifslv_u2phy_com	ssusb_sifslv_u2_phy_com_T28
0x00000900	MODULE	ssusb_sifslv_u3phyd	ssusb_sifslv_u3phyd_T28
0x00000a00	MODULE	ssusb_sifslv_u3phyd_bank2	ssusb_sifslv_u3phyd_bank2_T28
0x00000b00	MODULE	ssusb_sifslv_u3phya	ssusb_sifslv_u3phya_T28
0x00000c00	MODULE	ssusb_sifslv_u3phya_da	ssusb_sifslv_u3phya_da_T28
*/


/* TYPE DEFINE */
typedef unsigned int PHY_UINT32;
typedef int PHY_INT32;
typedef unsigned short PHY_UINT16;
typedef short PHY_INT16;
typedef unsigned char PHY_UINT8;
typedef char PHY_INT8;

typedef PHY_UINT32 __bitwise PHY_LE32;


#ifdef CONFIG_U3_PHY_AHB_SUPPORT
#ifdef CONFIG_ARM64
typedef u64 u3phy_addr_t;
#define ALIGN_MASK 0xFFFFFFFFFFFFFFFC
#else
typedef u32 u3phy_addr_t;
#define ALIGN_MASK 0xFFFFFFFC
#endif
#else
typedef u32 u3phy_addr_t;
#define ALIGN_MASK 0xFFFFFFFC
#endif

/* MACRO DEFINE */
#define DRV_WriteReg32(addr, data)	writel(data, (void __iomem *)addr)
#define DRV_Reg32(addr)			readl((void __iomem *)addr)

/* CONSTANT DEFINE */
#define PHY_FALSE	0
#define PHY_TRUE	1

/* #define DRV_MDELAY    mdelay */
#define DRV_MSLEEP	msleep
#define DRV_UDELAY	udelay
/* #define DRV_USLEEP    usleep */

/* PHY FUNCTION DEFINE, implemented in platform files, ex. ahb, gpio */
PHY_INT32 U3PhyWriteReg32(u3phy_addr_t addr, PHY_UINT32 data);
PHY_INT32 U3PhyReadReg32(u3phy_addr_t addr);
PHY_INT32 U3PhyWriteReg8(u3phy_addr_t addr, PHY_UINT8 data);
PHY_INT8 U3PhyReadReg8(u3phy_addr_t addr);

/* PHY GENERAL USAGE FUNC, implemented in mtk-phy.c */
PHY_INT32 U3PhyWriteField8(phys_addr_t addr, PHY_INT32 offset, PHY_INT32 mask, PHY_INT32 value);
PHY_INT32 U3PhyWriteField32(phys_addr_t addr, PHY_INT32 offset, PHY_INT32 mask, PHY_INT32 value);
PHY_INT32 U3PhyReadField8(phys_addr_t addr, PHY_INT32 offset, PHY_INT32 mask);
PHY_INT32 U3PhyReadField32(phys_addr_t addr, PHY_INT32 offset, PHY_INT32 mask);

struct u3phy_info {
	PHY_INT32 phy_version;
	PHY_INT32 phyd_version_addr;

#ifdef CONFIG_PROJECT_PHY
	struct u2phy_reg_e *u2phy_regs_e;
	struct u3phya_reg_e *u3phya_regs_e;
	struct u3phya_da_reg_e *u3phya_da_regs_e;
	struct u3phyd_reg_e *u3phyd_regs_e;
	struct u3phyd_bank2_reg_e *u3phyd_bank2_regs_e;
	struct sifslv_chip_reg_e *sifslv_chip_regs_e;
	struct spllc_reg_e *spllc_regs_e;
	struct sifslv_fm_feg_e *sifslv_fm_regs_e;
#else
#ifdef CONFIG_C60802_SUPPORT
	/* c60802 regs reference */
	struct u2phy_reg_c *u2phy_regs_c;
	struct u3phya_reg_c *u3phya_regs_c;
	struct u3phya_da_reg_c *u3phya_da_regs_c;
	struct u3phyd_reg_c *u3phyd_regs_c;
	struct u3phyd_bank2_reg_c *u3phyd_bank2_regs_c;
	struct sifslv_chip_reg_c *sifslv_chip_regs_c;
	struct sifslv_fm_feg_c *sifslv_fm_regs_c;
#endif
#ifdef CONFIG_D60802_SUPPORT
	/* d60802 regs reference */
	struct u2phy_reg_d *u2phy_regs_d;
	struct u3phya_reg_d *u3phya_regs_d;
	struct u3phya_da_reg_d *u3phya_da_regs_d;
	struct u3phyd_reg_d *u3phyd_regs_d;
	struct u3phyd_bank2_reg_d *u3phyd_bank2_regs_d;
	struct sifslv_chip_reg_d *sifslv_chip_regs_d;
	struct sifslv_fm_feg_d *sifslv_fm_regs_d;
#endif
#ifdef CONFIG_E60802_SUPPORT
	/* e60802 regs reference */
	struct u2phy_reg_e *u2phy_regs_e;
	struct u3phya_reg_e *u3phya_regs_e;
	struct u3phya_da_reg_e *u3phya_da_regs_e;
	struct u3phyd_reg_e *u3phyd_regs_e;
	struct u3phyd_bank2_reg_e *u3phyd_bank2_regs_e;
	struct sifslv_chip_reg_e *sifslv_chip_regs_e;
	struct spllc_reg_e *spllc_regs_e;
	struct sifslv_fm_feg_e *sifslv_fm_regs_e;
#endif
#ifdef CONFIG_A60810_SUPPORT
	/* A60810 regs reference */
	struct u2phy_reg_a *u2phy_regs_a;
	struct u3phya_reg_a *u3phya_regs_a;
	struct u3phya_da_reg_a *u3phya_da_regs_a;
	struct u3phyd_reg_a *u3phyd_regs_a;
	struct u3phyd_bank2_reg_a *u3phyd_bank2_regs_a;
	struct sifslv_chip_reg_a *sifslv_chip_regs_a;
	struct spllc_reg_a *spllc_regs_a;
	struct sifslv_fm_reg_a *sifslv_fm_regs_a;
#endif
#endif
};

struct u3phy_operator {
	PHY_INT32(*init)(struct u3phy_info *info);
	PHY_INT32(*change_pipe_phase)(struct u3phy_info *info, PHY_INT32 phy_drv,
				       PHY_INT32 pipe_phase);
	PHY_INT32(*eyescan_init)(struct u3phy_info *info);
	PHY_INT32(*eyescan)(struct u3phy_info *info, PHY_INT32 x_t1, PHY_INT32 y_t1,
			     PHY_INT32 x_br, PHY_INT32 y_br, PHY_INT32 delta_x, PHY_INT32 delta_y,
			     PHY_INT32 eye_cnt, PHY_INT32 num_cnt, PHY_INT32 PI_cal_en,
			     PHY_INT32 num_ignore_cnt);
	PHY_INT32(*u2_connect)(struct u3phy_info *info);
	PHY_INT32(*u2_disconnect)(struct u3phy_info *info);
	PHY_INT32(*u2_save_current_entry)(struct u3phy_info *info);
	PHY_INT32(*u2_save_current_recovery)(struct u3phy_info *info);
	PHY_INT32(*u2_slew_rate_calibration)(struct u3phy_info *info);
};

#undef EXTERN
#ifdef U3_PHY_LIB
#define EXTERN
#else
#define EXTERN \
extern
#endif

EXTERN struct u3phy_info *u3phy;
EXTERN struct u3phy_operator *u3phy_ops;

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

PHY_INT32 u3phy_init(void);

EXTERN struct strucScanRegion _rEye1;
EXTERN struct strucScanRegion _rEye2;
EXTERN struct strucTestCycle _rTestCycle;
EXTERN PHY_UINT8 _bXcurr;
EXTERN PHY_UINT8 _bYcurr;
EXTERN enumScanDir _eScanDir;
EXTERN PHY_INT8 _fgXChged;
EXTERN unsigned int _bPIResult;

/* Comment for saving the kernel size. This's only used at external PHY*/
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
EXTERN PHY_UINT32 pwErrCnt0[CYCLE_COUNT_MAX][ERRCNT_MAX][ERRCNT_MAX];
EXTERN PHY_UINT32 pwErrCnt1[CYCLE_COUNT_MAX][ERRCNT_MAX][ERRCNT_MAX];
#endif

extern void phy_hsrx_set(void);

/***********************************/
extern void __iomem *ap_uart0_base;
#ifdef CONFIG_FPGA_EARLY_PORTING
extern void __iomem *i2c1_base;
#endif
/***********************************/
#endif
