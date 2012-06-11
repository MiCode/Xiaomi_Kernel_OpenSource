/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MSM_CSIPHY_HWREG_H
#define MSM_CSIPHY_HWREG_H

/*MIPI CSI PHY registers*/
#define MIPI_CSIPHY_LNn_CFG1_ADDR                0x0
#define MIPI_CSIPHY_LNn_CFG2_ADDR                0x4
#define MIPI_CSIPHY_LNn_CFG3_ADDR                0x8
#define MIPI_CSIPHY_LNn_CFG4_ADDR                0xC
#define MIPI_CSIPHY_LNn_CFG5_ADDR                0x10
#define MIPI_CSIPHY_LNCK_CFG1_ADDR               0x100
#define MIPI_CSIPHY_LNCK_CFG2_ADDR               0x104
#define MIPI_CSIPHY_LNCK_CFG3_ADDR               0x108
#define MIPI_CSIPHY_LNCK_CFG4_ADDR               0x10C
#define MIPI_CSIPHY_LNCK_CFG5_ADDR               0x110
#define MIPI_CSIPHY_LNCK_MISC1_ADDR              0x128
#define MIPI_CSIPHY_GLBL_RESET_ADDR              0x140
#define MIPI_CSIPHY_GLBL_PWR_CFG_ADDR            0x144
#define MIPI_CSIPHY_HW_VERSION_ADDR              0x188
#define MIPI_CSIPHY_INTERRUPT_STATUS0_ADDR       0x18C
#define MIPI_CSIPHY_INTERRUPT_MASK0_ADDR         0x1AC
#define MIPI_CSIPHY_INTERRUPT_MASK_VAL           0x3F
#define MIPI_CSIPHY_INTERRUPT_MASK_ADDR          0x1B0
#define MIPI_CSIPHY_INTERRUPT_CLEAR0_ADDR        0x1CC
#define MIPI_CSIPHY_INTERRUPT_CLEAR_ADDR         0x1D0
#define MIPI_CSIPHY_MODE_CONFIG_SHIFT            0x4
#define MIPI_CSIPHY_GLBL_T_INIT_CFG0_ADDR        0x1EC
#define MIPI_CSIPHY_T_WAKEUP_CFG0_ADDR           0x1F4

#endif
