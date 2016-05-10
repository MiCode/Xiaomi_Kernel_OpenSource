/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 *
 * Airgo Networks, Inc proprietary.
 * All Rights Reserved, Copyright 2005
 * This program is the confidential and proprietary product of Airgo Networks Inc.
 * Any Unauthorized use, reproduction or transfer of this program is strictly prohibited.
 * 
 * phyGlobal.h: Holds all globals for the phy, rf, and asic layers in hal
 * Author:  Mark Nelson
 * Date:    4/9/05
 *
 * History -
 * Date        Modified by              Modification Information
  --------------------------------------------------------------------------
 */

#ifndef PHYGLOBAL_H
#define PHYGLOBAL_H

#include "halPhyVos.h"
#include "wlan_rf.h"
#include "wlan_phy.h"
#include "phyTxPower.h"
#include <qwlanhw_volans.h>
#include "asic.h"
#include "wlan_nv.h"



//#define ANI_MANF_DIAG       //temporary until this is part of manfDiag build - aniGlobal.h needs this to build the ptt globals

#ifdef VOLANS_VSWR_WORKAROUND
#define OPEN_LOOP_TX_HIGH_GAIN_OVERRIDE     6  //used for RVR tests in open loop mode
#else
#define OPEN_LOOP_TX_HIGH_GAIN_OVERRIDE     10  //used for RVR tests in open loop mode
#endif
#define OPEN_LOOP_TX_LOW_GAIN_OVERRIDE      3   //used for throughput tests in open loop mode

// Function pointer for to the CB function after set channel response from FW
typedef void (*funcHalSetChanCB)(tpAniSirGlobal, void*, tANI_U32, tANI_U16);

// Structure to save the context from where the set channel is called
typedef struct sPhySetChanCntx {
    tANI_U8 newChannel;
    tANI_U8 newRfBand;
    tANI_U8 newCbState;
    tANI_U8 newCalReqd;
    void*   pData;
    funcHalSetChanCB pFunc;
    tANI_U16 dialog_token;
} tPhySetChanCntx, *tpPhySetChanCntx;


typedef struct
{
    sHalNv nvCache;
    void *nvTables[NUM_NV_TABLE_IDS];

    //event object for blocked wait around halPhySetChannel
    HAL_PHY_SET_CHAN_EVENT_TYPE setChanEvent;
    tANI_U32 fwSetChannelStatus;

    tPhySetChanCntx setChanCntx;

    //physical layer data - corresponds to individual modules
    tPhy        phy;
    tAsicTxFir  txfir;
    //tPhyTxPower phyTPC;
    tRF         rf;
    tAsicAgc    agc;

    tANI_BOOLEAN wfm_clk80; //=ON if 20MHZ clock samples, =OFF for 80MHZ clock samples
    tANI_U8     calPeriodTicks;     //counts peiodic interrupts since last periodic calibration
    tANI_BOOLEAN densityEnabled;
    ePhyNwDensity nwDensity20MHz;     // Network density value for 20MHz channel width
    ePhyNwDensity nwDensity40MHz;     // Network density value for 40MHz channel width
    ePhyRxDisabledPktTypes modTypes;  //current disabled packet types
    volatile tANI_BOOLEAN setPhyMsgEvent;
    tANI_U32 hdetResidualDCO;
}tAniSirPhy;

#endif /* PHYGLOBAL_H */
