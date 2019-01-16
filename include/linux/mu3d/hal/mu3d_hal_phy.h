#ifndef MTK_PHY_H
#define MTK_PHY_H

#include <linux/mu3d/hal/mu3d_hal_comm.h>
#include <linux/mu3phy/mtk-phy.h>

#undef EXTERN

#define ENTER_U0_TH		 		10
#define MAX_PHASE_RANGE 		31
#define MAX_TIMEOUT_COUNT 		100

#ifdef _MTK_PHY_EXT_
#define EXTERN
#else
#define EXTERN extern
#endif

#define U3_PHY_I2C_PCLK_DRV_REG	    0x0A
#define U3_PHY_I2C_PCLK_PHASE_REG	0x0B

EXTERN DEV_INT32 mu3d_hal_phy_scan(DEV_INT32 latch_val, DEV_UINT8 driving);
EXTERN PHY_INT32 _U3Read_Reg(PHY_INT32 address);
EXTERN PHY_INT32 _U3Write_Reg(PHY_INT32 address, PHY_INT32 value);

#undef EXTERN

#endif
