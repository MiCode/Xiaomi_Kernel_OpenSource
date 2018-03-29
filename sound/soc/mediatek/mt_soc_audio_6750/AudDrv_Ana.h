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
#include "AudDrv_Def.h"
/*#include <mach/mt_typedefs.h>*/

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/


/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
/*
 #ifndef int8
typedef signed char int8;
#endif

#ifndef uint8
typedef unsigned char uint8;
#endif

#ifndef int16
typedef short int16;
#endif

#ifndef uint16
typedef unsigned short uint16;
#endif

#ifndef int32
typedef int int32;
#endif

#ifndef uint32
typedef unsigned int uint32;
#endif

#ifndef int64
typedef long long int64;
#endif

#ifndef uint64
typedef unsigned long long uint64;
#endif
*/

typedef	uint8_t kal_uint8;
typedef	int8_t kal_int8;
typedef	uint32_t kal_uint32;
typedef	int32_t kal_int32;
typedef	uint64_t kal_uint64;
typedef	int64_t kal_int64;


#define PMIC_REG_BASE                    (0x0000)
#define AFE_UL_DL_CON0                                   ((UINT32)(PMIC_REG_BASE+0x2000+0x0))
#define AFE_DL_SRC2_CON0_H                         ((UINT32)(PMIC_REG_BASE+0x2000+0x2))
#define AFE_DL_SRC2_CON0_L                          ((UINT32)(PMIC_REG_BASE+0x2000+0x4))
#define AFE_DL_SDM_CON0                                ((UINT32)(PMIC_REG_BASE+0x2000+0x6))
#define AFE_DL_SDM_CON1                                ((UINT32)(PMIC_REG_BASE+0x2000+0x8))
#define AFE_UL_SRC0_CON0_H                         ((UINT32)(PMIC_REG_BASE+0x2000+0xa))
#define AFE_UL_SRC0_CON0_L                          ((UINT32)(PMIC_REG_BASE+0x2000+0xc))
#define AFE_UL_SRC1_CON0_H                         ((UINT32)(PMIC_REG_BASE+0x2000+0xe))
#define AFE_UL_SRC1_CON0_L                          ((UINT32)(PMIC_REG_BASE+0x2000+0x10))
#define PMIC_AFE_TOP_CON0                            ((UINT32)(PMIC_REG_BASE+0x2000+0x12))
#define AFE_AUDIO_TOP_CON0                         ((UINT32)(PMIC_REG_BASE+0x2000+0x14))
#define AFE_DL_SRC_MON0                                ((UINT32)(PMIC_REG_BASE+0x2000+0x16))
#define AFE_DL_SDM_TEST0                               ((UINT32)(PMIC_REG_BASE+0x2000+0x18))
#define AFE_MON_DEBUG0                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x1a))
#define AFUNC_AUD_CON0                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x1c))
#define AFUNC_AUD_CON1                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x1e))
#define AFUNC_AUD_CON2                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x20))
#define AFUNC_AUD_CON3                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x22))
#define AFUNC_AUD_CON4                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x24))
#define AFUNC_AUD_MON0                                ((UINT32)(PMIC_REG_BASE+0x2000+0x26))
#define AFUNC_AUD_MON1                                ((UINT32)(PMIC_REG_BASE+0x2000+0x28))
#define AUDRC_TUNE_MON0                              ((UINT32)(PMIC_REG_BASE+0x2000+0x2a))
#define AFE_UP8X_FIFO_CFG0                         ((UINT32)(PMIC_REG_BASE+0x2000+0x2c))
#define AFE_UP8X_FIFO_LOG_MON0              ((UINT32)(PMIC_REG_BASE+0x2000+0x2e))
#define AFE_UP8X_FIFO_LOG_MON1              ((UINT32)(PMIC_REG_BASE+0x2000+0x30))
#define AFE_DL_DC_COMP_CFG0                    ((UINT32)(PMIC_REG_BASE+0x2000+0x32))
#define AFE_DL_DC_COMP_CFG1                    ((UINT32)(PMIC_REG_BASE+0x2000+0x34))
#define AFE_DL_DC_COMP_CFG2                    ((UINT32)(PMIC_REG_BASE+0x2000+0x36))
#define AFE_PMIC_NEWIF_CFG0                     ((UINT32)(PMIC_REG_BASE+0x2000+0x38))
#define AFE_PMIC_NEWIF_CFG1                     ((UINT32)(PMIC_REG_BASE+0x2000+0x3a))
#define AFE_PMIC_NEWIF_CFG2                     ((UINT32)(PMIC_REG_BASE+0x2000+0x3c))
#define AFE_PMIC_NEWIF_CFG3                     ((UINT32)(PMIC_REG_BASE+0x2000+0x3e))
#define AFE_SGEN_CFG0                                   ((UINT32)(PMIC_REG_BASE+0x2000+0x40))
#define AFE_SGEN_CFG1                                   ((UINT32)(PMIC_REG_BASE+0x2000+0x42))
#define AFE_VOW_TOP                                        ((UINT32)(PMIC_REG_BASE+0x2000+0x70))
#define AFE_VOW_CFG0                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x72))
#define AFE_VOW_CFG1                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x74))
#define AFE_VOW_CFG2                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x76))
#define AFE_VOW_CFG3                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x78))
#define AFE_VOW_CFG4                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x7a))
#define AFE_VOW_CFG5                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x7c))
#define AFE_VOW_MON0                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x7e))
#define AFE_VOW_MON1                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x80))
#define AFE_VOW_MON2                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x82))
#define AFE_VOW_MON3                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x84))
#define AFE_VOW_MON4                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x86))
#define AFE_VOW_MON5                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x88))
#define AFE_VOW_TGEN_CFG0                               ((UINT32)(PMIC_REG_BASE+0x2000+0x8A))
#define AFE_VOW_POSDIV_CFG0                             ((UINT32)(PMIC_REG_BASE+0x2000+0x8C))
#define AFE_DCCLK_CFG0                                  ((UINT32)(PMIC_REG_BASE+0x2000+0x90))
#define AFE_DCCLK_CFG1                                  ((UINT32)(PMIC_REG_BASE+0x2000+0x92))

/* TODO: 6353 digital part - plus */
#define AFE_ADDA2_UP8X_FIFO_LOG_MON0                    ((UINT32)(PMIC_REG_BASE+0x2000+0x4C))
#define AFE_ADDA2_UP8X_FIFO_LOG_MON1                    ((UINT32)(PMIC_REG_BASE+0x2000+0x4E))
#define AFE_ADDA2_PMIC_NEWIF_CFG0                       ((UINT32)(PMIC_REG_BASE+0x2000+0x50))
#define AFE_ADDA2_PMIC_NEWIF_CFG1                       ((UINT32)(PMIC_REG_BASE+0x2000+0x52))
#define AFE_ADDA2_PMIC_NEWIF_CFG2                       ((UINT32)(PMIC_REG_BASE+0x2000+0x54))
#define AFE_HPANC_CFG0                                  ((UINT32)(PMIC_REG_BASE+0x2000+0x94))
#define AFE_NCP_CFG0                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x96))
#define AFE_NCP_CFG1                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x98))

/*
#define LDO_CON1                                        ((UINT32)(PMIC_REG_BASE + 0x0A02))
#define LDO_CON2                                        ((UINT32)(PMIC_REG_BASE + 0x0A04))
#define LDO_VCON1                                       ((UINT32)(PMIC_REG_BASE + 0x0A40))
*/

#define GPIO_MODE3          ((UINT32)(0x60D0))

#if 1
/* register number */

#else
#include <mach/upmu_hw.h>
#endif

void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask);
uint32 Ana_Get_Reg(uint32 offset);
/* for debug usage */
void Ana_Log_Print(void);

#endif
