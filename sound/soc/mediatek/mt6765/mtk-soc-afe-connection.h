/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
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
