/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
 * If not, see <http://www.gnu.org/licenses/>.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Ana.h
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Ana
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 *   Chipeng Chang (mtk02308)
 *
 *------------------------------------------------------------------------------
 *
 *******************************************************************************/

#ifndef _AUDDRV_ANA_H_
#define _AUDDRV_ANA_H_

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"


/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/


/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
#define PMIC_REG_BASE                    (0x0000)
/* 6323 pmic reg */
/* ---------------digital pmic  register define ------------------------------------------- */
#define ABB_AFE_CON0             (PMIC_REG_BASE+0x4000 + 0x0000)
#define ABB_AFE_CON1             (PMIC_REG_BASE+0x4000 + 0x0002)
#define ABB_AFE_CON2             (PMIC_REG_BASE+0x4000 + 0x0004)
#define ABB_AFE_CON3             (PMIC_REG_BASE+0x4000 + 0x0006)
#define ABB_AFE_CON4             (PMIC_REG_BASE+0x4000 + 0x0008)
#define ABB_AFE_CON5             (PMIC_REG_BASE+0x4000 + 0x000A)
#define ABB_AFE_CON6             (PMIC_REG_BASE+0x4000 + 0x000C)
#define ABB_AFE_CON7             (PMIC_REG_BASE+0x4000 + 0x000E)
#define ABB_AFE_CON8             (PMIC_REG_BASE+0x4000 + 0x0010)
#define ABB_AFE_CON9             (PMIC_REG_BASE+0x4000 + 0x0012)
#define ABB_AFE_CON10            (PMIC_REG_BASE+0x4000 + 0x0014)
#define ABB_AFE_CON11            (PMIC_REG_BASE+0x4000 + 0x0016)
#define ABB_AFE_STA0             (PMIC_REG_BASE+0x4000 + 0x0018)
#define ABB_AFE_STA1             (PMIC_REG_BASE+0x4000 + 0x001A)
#define ABB_AFE_STA2             (PMIC_REG_BASE+0x4000 + 0x001C)
#define AFE_UP8X_FIFO_CFG0       (PMIC_REG_BASE+0x4000 + 0x001E)
#define AFE_UP8X_FIFO_LOG_MON0   (PMIC_REG_BASE+0x4000 + 0x0020)
#define AFE_UP8X_FIFO_LOG_MON1   (PMIC_REG_BASE+0x4000 + 0x0022)
#define AFE_PMIC_NEWIF_CFG0      (PMIC_REG_BASE+0x4000 + 0x0024)
#define AFE_PMIC_NEWIF_CFG1      (PMIC_REG_BASE+0x4000 + 0x0026)
#define AFE_PMIC_NEWIF_CFG2      (PMIC_REG_BASE+0x4000 + 0x0028)
#define AFE_PMIC_NEWIF_CFG3      (PMIC_REG_BASE+0x4000 + 0x002A)
#define ABB_AFE_TOP_CON0         (PMIC_REG_BASE+0x4000 + 0x002C)
#define ABB_MON_DEBUG0           (PMIC_REG_BASE+0x4000 + 0x002E)

/* ---------------digital pmic  register define end --------------------------------------- */

/* ---------------analog pmic  register define start -------------------------------------- */
#define TOP_CKPDN0                  (PMIC_REG_BASE + 0x102)
#define TOP_CKPDN0_SET              (PMIC_REG_BASE + 0x104)
#define TOP_CKPDN0_CLR              (PMIC_REG_BASE + 0x106)
#define TOP_CKPDN1                  (PMIC_REG_BASE + 0x108)
#define TOP_CKPDN1_SET              (PMIC_REG_BASE + 0x10A)
#define TOP_CKPDN1_CLR              (PMIC_REG_BASE + 0x10C)
#define TOP_CKPDN2                  (PMIC_REG_BASE + 0x10E)
#define TOP_CKPDN2_SET              (PMIC_REG_BASE + 0x110)
#define TOP_CKPDN2_CLR              (PMIC_REG_BASE + 0x112)
#define TOP_RST_CON_SET             (PMIC_REG_BASE + 0x116)
#define TOP_RST_CON_CLR             (PMIC_REG_BASE + 0x118)
#define TOP_CKCON1                  (PMIC_REG_BASE + 0x126)

#define SPK_CON0                    (PMIC_REG_BASE + 0x052)
#define SPK_CON1                    (PMIC_REG_BASE + 0x054)
#define SPK_CON2                    (PMIC_REG_BASE + 0x056)
#define SPK_CON6                    (PMIC_REG_BASE + 0x05E)
#define SPK_CON7                    (PMIC_REG_BASE + 0x060)
#define SPK_CON8                    (PMIC_REG_BASE + 0x062)
#define SPK_CON9                    (PMIC_REG_BASE + 0x064)
#define SPK_CON10                   (PMIC_REG_BASE + 0x066)
#define SPK_CON11                   (PMIC_REG_BASE + 0x068)
#define SPK_CON12                   (PMIC_REG_BASE + 0x06A)
#define CID                         (PMIC_REG_BASE + 0x100)

#define AUDTOP_CON0                 (PMIC_REG_BASE + 0x700)
#define AUDTOP_CON1                 (PMIC_REG_BASE + 0x702)
#define AUDTOP_CON2                 (PMIC_REG_BASE + 0x704)
#define AUDTOP_CON3                 (PMIC_REG_BASE + 0x706)
#define AUDTOP_CON4                 (PMIC_REG_BASE + 0x708)
#define AUDTOP_CON5                 (PMIC_REG_BASE + 0x70A)
#define AUDTOP_CON6                 (PMIC_REG_BASE + 0x70C)
#define AUDTOP_CON7                 (PMIC_REG_BASE + 0x70E)
#define AUDTOP_CON8                 (PMIC_REG_BASE + 0x710)
#define AUDTOP_CON9                 (PMIC_REG_BASE + 0x712)

/* ---------------analog pmic  register define end --------------------------------------- */

void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask);
uint32 Ana_Get_Reg(uint32 offset);

/* for debug usage */
void Ana_Log_Print(void);

#endif
