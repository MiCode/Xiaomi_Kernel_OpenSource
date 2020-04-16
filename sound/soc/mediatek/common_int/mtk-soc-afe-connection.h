/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
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
 *  mt_sco_afe_connection.c
 *
 * Project:
 * --------
 *   MT6797  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */
#ifndef _MT_SOC_AFE_CONNECTION_H_
#define _MT_SOC_AFE_CONNECTION_H_

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E
 *****************************************************************************/
bool SetConnectionState(unsigned int ConnectionState, unsigned int Input,
			unsigned int Output);
bool SetIntfConnectionFormat(unsigned int ConnectionFormat,
			     unsigned int Aud_block);
bool SetIntfConnectionState(unsigned int ConnectionState,
			    unsigned int Aud_block_In,
			    unsigned int Aud_block_Out);

#endif
