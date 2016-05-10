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

/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *


    \file pttFrameGen.h

    \brief Definitions for PTT frame generation

    $Id$

    Copyright (C) 2006 Airgo Networks, Incorporated


   ========================================================================== */

#ifndef PTTFRAMEGEN_H
#define PTTFRAMEGEN_H


//#define MAX_PKT_GEN_BUF_ENTRY  (HAL_HIF_MAX_TX_RING_ENTRY >> 1)



#define MAX_PAYLOAD_SIZE 2400
#define MAX_TX_PAYLOAD_SIZE 4096

typedef enum {
   TEST_PAYLOAD_NONE,
   TEST_PAYLOAD_FILL_BYTE,
   TEST_PAYLOAD_RANDOM,
   TEST_PAYLOAD_RAMP,
   TEST_PAYLOAD_TEMPLATE,
   TEST_PAYLOAD_MAX = 0X3FFFFFFF,   //dummy value to set enum to 4 bytes
} ePayloadContents;


#define MAC_ADDR_SIZE ( 6 )

typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 numTestPackets;
   tANI_U32 interFrameSpace;
   eHalPhyRates rate;
   ePayloadContents payloadContents;
   tANI_U16 payloadLength;
   tANI_U8 payloadFillByte;
   tANI_BOOLEAN pktAutoSeqNum;  //seq num setting (hw or not) for packet gen

   tANI_U8 addr1[MAC_ADDR_SIZE];
   tANI_U8 addr2[MAC_ADDR_SIZE];
   tANI_U8 addr3[MAC_ADDR_SIZE];
   tANI_U8 tx_mode;
   tANI_BOOLEAN crc;            //0 = no FCS calculated = power detector works = receive won't work?,
   //1 = crc calculated = receive works, but power detector workaround doesn't

   ePhyDbgPreamble preamble;
} sPttFrameGenParams;


typedef PACKED_PRE struct PACKED_POST {
   tANI_U32 legacy;             //11g OFDM preamble
   tANI_U32 gfSimo20;           //greenfield preamble
   tANI_U32 mmSimo20;           //mixed mode preamble
   tANI_U32 gfSimo40;           //greenfield preamble
   tANI_U32 mmSimo40;           //mixed mode preamble
   tANI_U32 txbShort;           //11b short
   tANI_U32 txbLong;	        //11b long
   tANI_U32 acSimo204080;       //11ac SIMO 20,40,80
   tANI_U32 total;
} sTxFrameCounters;

#endif
