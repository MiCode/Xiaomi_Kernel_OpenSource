/*
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _DT_BINDINGS_RESET_CONTROLLER_MT7622
#define _DT_BINDINGS_RESET_CONTROLLER_MT7622

/* TOPRGU resets */
#define MT7622_TOPRGU_MM_RST      1 /* Write 1 to reset MM and its related pad macro(SPI,DPI,MIPI_CFG,MIPI_TX) */
#define MT7622_TOPRGU_MFG_RST     2 /* Write 1 to reset MFG */
#define MT7622_TOPRGU_VENC_RST    3 /* Write 1 to reset VENC */
#define MT7622_TOPRGU_VDEC_RST    4 /* Write 1 to reset VDEC */
#define MT7622_TOPRGU_IMG_RST     5 /* Write 1 to VENC & IMG and its related pad macro(cam,mipi_rx) */
#define MT7622_TOPRGU_MD_RST      7 /* Write 1 to reset MODEM system(2T 32K duration is required for TD modem) */
#define MT7622_TOPRGU_CONN_RST    9 /* Write 1 to reset CONNSYS */
#define MT7622_TOPRGU_C2K_SW_RST 14 /* Write 1 to reset the MODEM C2K system when C2K WDT is asserted */
#define MT7622_TOPRGU_C2K_RST    15 /* Write 1 to reset MODEM C2K system */

#endif  /* _DT_BINDINGS_RESET_CONTROLLER_MT7622 */
