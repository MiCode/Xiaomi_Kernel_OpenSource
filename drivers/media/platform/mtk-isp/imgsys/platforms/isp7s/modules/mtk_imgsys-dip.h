/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_DIP_DIP_H_
#define _MTK_DIP_DIP_H_

#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-cmdq.h"
#include "./../mtk_imgsys-engine.h"
/* DIP */
#define DIP_TOP_ADDR	0x15100000
#define TOP_CTL_OFT	0x0000
#define TOP_CTL_SZ	0x0334
#define DMATOP_OFT	0x1200
#define DMATOP_SZ	0x2540
#define NR3D_CTL_OFT	0x5000
#define NR3D_CTL_SZ	0x1AEC
#define SNRS_CTL_OFT	0x7400
#define SNRS_CTL_SZ	0xE0
#define UNP_D1_CTL_OFT	0x8000
#define UNP_D1_CTL_SZ	0x20C
#define SMT_D1_CTL_OFT	0x83C0
#define SMT_D1_CTL_SZ	0x3C4

#define DIPCTL_DBG_SEL	0x1D8
#define DIPCTL_DBG_OUT	0x1DC
#define NR3D_DBG_SEL	0x501C

/* DIP NR1 */
#define DIP_NR1_ADDR		0x15150000
#define SNR_D1_CTL_OFT		0x4400
#define SNR_D1_CTL_SZ		0x348
#define EE_D1_CTL_OFT		0x4C40
#define EE_D1_CTL_SZ		0x138
#define TNC_BCE_CTL_OFT		0x7000
#define TNC_BCE_CTL_SZ		0x10
#define TNC_TILE_CTL_OFT	0x7D90
#define TNC_TILE_CTL_SZ		0xC
#define TNC_C2G_CTL_OFT		0x8004
#define TNC_C2G_CTL_SZ		0x124
#define TNC_C3D_CTL_OFT		0xA000
#define TNC_C3D_CTL_SZ		0x84

/* DIP NR2 */
#define DIP_NR2_ADDR		0x15160000
#define VIPI_D1_CTL_OFT		0x1440
#define VIPI_D1_CTL_SZ		0x800
#define SNRCSI_D1_CTL_OFT	0x2240
#define SNRCSI_D1_CTL_SZ	0x1140
#define SMTCO_D4_CTL_OFT	0x3640
#define SMTCO_D4_CTL_SZ		0x1FE4
#define DRZH2N_D2_CTL_OFT	0x6D00
#define DRZH2N_D2_CTL_SZ	0x54


void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dip_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dip_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);

void imgsys_dip_uninit(struct mtk_imgsys_dev *imgsys_dev);

#endif /* _MTK_DIP_DIP_H_ */

