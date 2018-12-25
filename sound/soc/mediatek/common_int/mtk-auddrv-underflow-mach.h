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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_devtree_parser.c
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   AudDrv_devtree_parser
 *
 * Author:
 * -------
 *   Chipeng Chang (mtk02308)
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */
#include <mt-plat/aee.h>

void SetUnderFlowThreshold(unsigned int Threshold);
void Auddrv_Aee_Dump(void);
void Auddrv_Set_UnderFlow(void);
void Auddrv_Reset_Dump_State(void);
void Auddrv_Set_Interrupt_Changed(bool bChange);
void Auddrv_CheckInterruptTiming(void);
void RefineInterrruptInterval(void);
bool Auddrv_Set_DlSamplerate(unsigned int Samplerate);
bool Auddrv_Set_InterruptSample(unsigned int count);
bool Auddrv_Enable_dump(bool bEnable);
