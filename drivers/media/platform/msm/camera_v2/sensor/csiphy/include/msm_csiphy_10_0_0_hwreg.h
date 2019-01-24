/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef MSM_CSIPHY_10_0_0_HWREG_H
#define MSM_CSIPHY_10_0_0_HWREG_H

#include <sensor/csiphy/msm_csiphy.h>

#define LANE_MASK_AGGR_MODE     0x1F
#define LANE_MASK_PHY_A         0x13
#define LANE_MASK_PHY_B         0x2C
#define mask_phy_enable_A       0x3
#define mask_phy_enable_B       0xC
#define mask_base_dir_A         0x1
#define mask_base_dir_B         0x2
#define mask_force_mode_A       0x3
#define mask_force_mode_B       0xC
#define mask_enable_clk_A       0x1
#define mask_enable_clk_B       0x2
#define mask_ctrl_1_A           0x5
#define mask_ctrl_1_B           0xA
#define mask_reset_A            0x1
#define mask_reset_B            0x7
#define mask_shutdown_A         0x3
#define mask_hs_freq_range      0x7F
#define mask_osc_freq_2         0xFF
#define mask_osc_freq_3         0xF00

static struct csiphy_reg_parms_t csiphy_v10_0_0 = {
	//ToDo: Fill these addresses from SWI
	.mipi_csiphy_interrupt_status0_addr = 0x5B4,
	.mipi_csiphy_interrupt_clear0_addr = 0x59C,
	.mipi_csiphy_glbl_irq_cmd_addr = 0x588,
	.mipi_csiphy_interrupt_clk_status0_addr = 0x5D8,
	.mipi_csiphy_interrupt_clk_clear0_addr = 0x5D0,
};

static struct csiphy_reg_snps_parms_t csiphy_v10_0_0_snps = {
	/*MIPI CSI PHY registers*/
	{0x560, 0x9},    /* mipi_csiphy_sys_ctrl */
	{0x5Ac, 0x9},    /* mipi_csiphy_sys_ctrl_1 */
	{0x564, 0xF},    /* mipi_csiphy_ctrl_1 */
	{0x568, 0x49},   /* mipi_csiphy_ctrl_2 */
	{0x56C, 0x49},   /* mipi_csiphy_ctrl_3 */
	{0x580, 0x7F},   /* mipi_csiphy_fifo_ctrl */
	{0x570, 0x0},    /* mipi_csiphy_enable */
	{0x578, 0x0},    /* mipi_csiphy_basedir */
	{0x57C, 0x0},    /* mipi_csiphy_force_mode */
	{0x574, 0x0},    /* mipi_csiphy_enable_clk */
	{0x58C, 0xFF},   /* mipi_csiphy_irq_mask_ctrl_lane_0 */
	{0x5C8, 0xFF},   /* mipi_csiphy_irq_mask_ctrl_lane_clk_0 */
	{0x20, 0x0},     /* mipi_csiphy_rx_sys_7_00 */
	{0x384, 0x0},    /* mipi_csiphy_rx_startup_ovr_1_00 */
	{0x388, 0xCC},   /* mipi_csiphy_rx_startup_ovr_2_00 */
	{0x38C, 0x1},    /* mipi_csiphy_rx_startup_ovr_3_00 */
	{0x390, 0x1},    /* mipi_csiphy_rx_startup_ovr_4_00 */
	{0x394, 0x1},    /* mipi_csiphy_rx_startup_ovr_5_00 */
	{0x324, 0x0},    /* mipi_csiphy_rx_startup_obs_2_00 */
	{0x6B0, 0x0},    /* mipi_csiphy_rx_cb_2_00 */
	{0x4CC, 0x1},    /* mipi_csiphy_rx_dual_phy_0_00 */
	{0xC0, 0x0},     /* mipi_csiphy_rx_clk_lane_3_00 */
	{0xC4, 0xA},     /* mipi_csiphy_rx_clk_lane_4_00 */
	{0xC8, 0x0},     /* mipi_csiphy_rx_clk_lane_6_00 */
	{0x12c, 0x0},    /* mipi_csiphy_rx_lane_0_7_00 */
	{0x220, 0x0},    /* mipi_csiphy_rx_lane_1_7_00 */
	{0xCC, 0x0},     /* mipi_csiphy_rx_clk_lane_7_00 */
	{0x1F8, 0x20},   /* mipi_csiphy_rx_lane0_ddl_2_00 */
	{0x1FC, 0x10},   /* mipi_csiphy_rx_lane0_ddl_3_00 */
	{0x22C, 0x80},   /* mipi_csiphy_rx_lane_1_10_00 */
	{0x230, 0x10},   /* mipi_csiphy_rx_lane_1_11_00 */
};

static struct snps_freq_value snps_v100_freq_values[] = {
	{80,   0x0,  460 },  /*        80 - 97.125*/
	{90,   0x10, 460 },  /*       80 - 107.625*/
	{100,  0x20, 460 },  /*   83.125 - 118.125*/
	{110,  0x30, 460 },  /*   92.625 - 128.625*/
	{120,  0x1,  460 },  /*  102.125 - 139.125*/
	{130,  0x11, 460 },  /*  111.625 - 149.625*/
	{140,  0x21, 460 },  /*  121.125 - 160.125*/
	{150,  0x31, 460 },  /*  130.625 - 170.625*/
	{160,  0x2,  460 },  /*  140.125 - 181.125*/
	{170,  0x12, 460 },  /*  149.625 - 191.625*/
	{180,  0x22, 460 },  /*  159.125 - 202.125*/
	{190,  0x32, 460 },  /*  168.625 - 212.625*/
	{205,  0x3,  460 },  /*  182.875 - 228.375*/
	{220,  0x13, 460 },  /*  197.125 - 244.125*/
	{235,  0x23, 460 },  /*  211.375 - 259.875*/
	{250,  0x33, 460 },  /*  225.625 - 275.625*/
	{275,  0x4,  460 },  /*  249.375 - 301.875*/
	{300,  0x14, 460 },  /*  273.125 - 328.125*/
	{325,  0x25, 460 },  /*  296.875 - 354.375*/
	{350,  0x35, 460 },  /*  320.625 - 380.625*/
	{400,  0x5,  460 },  /*  368.125 - 433.125*/
	{450,  0x16, 460 },  /*  415.625 - 485.625*/
	{500,  0x26, 460 },  /*  463.125 - 538.125*/
	{550,  0x37, 460 },  /*  510.625 - 590.625*/
	{600,  0x7,  460 },  /*  558.125 - 643.125*/
	{650,  0x18, 460 },  /*  605.625 - 695.625*/
	{700,  0x28, 460 },  /*  653.125 - 748.125*/
	{750,  0x39, 460 },  /*  700.625 - 800.625*/
	{800,  0x9,  460 },  /*  748.125 - 853.125*/
	{850,  0x19, 460 },  /*  795.625 - 905.625*/
	{900,  0x29, 460 },  /*  843.125 - 958.125*/
	{950,  0x3a, 460 },  /* 890.625 - 1010.625*/
	{1000, 0xa,  460 },  /* 938.125 - 1063.125*/
	{1050, 0x1a, 460 },  /* 985.625 - 1115.625*/
	{1100, 0x2a, 460 },  /*1033.125 - 1168.125*/
	{1150, 0x3b, 460 },  /*1080.625 - 1220.625*/
	{1200, 0xb,  460 },  /*1128.125 - 1273.125*/
	{1250, 0x1b, 460 },  /*1175.625 - 1325.625*/
	{1300, 0x2b, 460 },  /*1223.125 - 1378.125*/
	{1350, 0x3c, 460 },  /*1270.625 - 1430.625*/
	{1400, 0xc,  460 },  /*1318.125 - 1483.125*/
	{1450, 0x1c, 460},   /*1365.625 - 1535.625*/
	{1500, 0x2c, 460 },  /*1413.125 - 1588.125*/
	{1550, 0x3d, 285 },  /*1460.625 - 1640.625*/
	{1600, 0xd,  295 },  /*1508.125 - 1693.125*/
	{1650, 0x1d, 304 },  /*1555.625 - 1745.625*/
	{1700, 0x2e, 313 },  /*1603.125 - 1798.125*/
	{1750, 0x3e, 322 },  /*1650.625 - 1850.625*/
	{1800, 0xe,  331 },  /*1698.125 - 1903.125*/
	{1850, 0x1e, 341 },  /*1745.625 - 1955.625*/
	{1900, 0x2f, 350 },  /*1793.125 - 2008.125*/
	{1950, 0x3f, 359 },  /*1840.625 - 2060.625*/
	{2000, 0xf,  368 },  /*1888.125 - 2113.125*/
	{2050, 0x40, 377 },  /*1935.625 - 2165.625*/
	{2100, 0x41, 387 },  /*1983.125 - 2218.125*/
	{2150, 0x42, 396 },  /*2030.625 - 2270.625*/
	{2200, 0x43, 405 },  /*2078.125 - 2323.125*/
	{2250, 0x44, 414 },  /*2125.625 - 2375.625*/
	{2300, 0x45, 423 },  /*2173.125 - 2428.125*/
	{2350, 0x46, 432 },  /*2220.625 - 2480.625*/
	{2400, 0x47, 442 },  /*    2268.125 - 2500*/
	{2450, 0x48, 451 },  /*    2315.625 - 2500*/
	{2500, 0x49, 460 },  /*    2363.125 - 2500*/
};

#endif
