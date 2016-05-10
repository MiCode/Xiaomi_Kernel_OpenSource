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

#if !defined( __WLAN_QCT_PAL_TYPE_H )
#define __WLAN_QCT_PAL_TYPE_H

/**=========================================================================
  
  \file  wlan_qct_pal_type.h
  
  \brief define basic types PAL exports. wpt = (Wlan Pal Type)
               
   Definitions for platform independent. 
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/
#include "wlan_qct_os_type.h"

typedef wpt_uint8 wpt_macAddr[6];

enum
{
   eWLAN_PAL_FALSE = 0,
   eWLAN_PAL_TRUE = 1,
};

typedef enum
{
   eWLAN_MODULE_DAL,
   eWLAN_MODULE_DAL_CTRL,
   eWLAN_MODULE_DAL_DATA,
   eWLAN_MODULE_PAL,
   
   //Always the last one
   eWLAN_MODULE_COUNT
} wpt_moduleid;


typedef struct
{
   //BIT order is most likely little endian. 
   //This structure is for netowkr-order byte array (or big-endian byte order)
#ifndef WLAN_PAL_BIG_ENDIAN_BIT
   wpt_byte protVer :2;
   wpt_byte type :2;
   wpt_byte subType :4;

   wpt_byte toDS :1;
   wpt_byte fromDS :1;
   wpt_byte moreFrag :1;
   wpt_byte retry :1;
   wpt_byte powerMgmt :1;
   wpt_byte moreData :1;
   wpt_byte wep :1;
   wpt_byte order :1;

#else

   wpt_byte subType :4;
   wpt_byte type :2;
   wpt_byte protVer :2;

   wpt_byte order :1;
   wpt_byte wep :1;
   wpt_byte moreData :1;
   wpt_byte powerMgmt :1;
   wpt_byte retry :1;
   wpt_byte moreFrag :1;
   wpt_byte fromDS :1;
   wpt_byte toDS :1;

#endif

} wpt_FrameCtrl;

typedef struct
{
   /* Frame control field */
   wpt_FrameCtrl frameCtrl;
   /* Duration ID */
   wpt_uint16 usDurationId;
   /* Address 1 field  */
   wpt_macAddr vA1;
   /* Address 2 field */
   wpt_macAddr vA2;
   /* Address 3 field */
   wpt_macAddr vA3;
   /* Sequence control field */
   wpt_uint16 sSeqCtrl;
   /* Optional A4 address */
   wpt_macAddr optvA4;
   /* Optional QOS control field */
   wpt_uint16  usQosCtrl;
}wpt_80211Header;

typedef struct
{
   wpt_macAddr dest;
   wpt_macAddr sec;
   wpt_uint16 lenOrType;
} wpt_8023Header;


#endif // __WLAN_QCT_PAL_TYPE_H
