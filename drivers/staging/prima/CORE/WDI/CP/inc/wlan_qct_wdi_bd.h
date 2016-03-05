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
#ifndef WLAN_QCT_WDI_BD_H
#define WLAN_QCT_WDI_BD_H

/*===========================================================================

         W L A N   D E V I C E   A B S T R A C T I O N   L A Y E R 
              I N T E R N A L     A P I       F O R    T H E
                B D   H E A D E R   D E F I N I T I O N 
                
                   
DESCRIPTION
  This file contains the internal BD definition exposed by the DAL Control       
  Path Core module to be used by the DAL Data Path Core. 
  
      
  Copyright (c) 2010 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Confidential and Proprietary
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
08/19/10    lti     Created module.

===========================================================================*/

#include "wlan_qct_pal_type.h"


/*=========================================================================         
     BD STRUCTURE Defines  
 =========================================================================*/
/*---------------------------------------------------------------------------         
  WDI_RxBdType - The format of the RX BD 
---------------------------------------------------------------------------*/
typedef struct 
{
        /* 0x00 */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** (Only used by the DPU)
        This routing flag indicates the WQ number to which the DPU will push the
        frame after it finished processing it. */
        wpt_uint32 dpuRF:8;
    
        /** This is DPU sig inserted by RXP. Signature on RA's DPU descriptor */
        wpt_uint32 dpuSignature:3;
    
        /** When set Sta is authenticated. SW needs to set bit
        addr2_auth_extract_enable in rxp_config2 register. Then RXP will use bit 3
        in DPU sig to say whether STA is authenticated or not. In this case only
        lower 2bits of DPU Sig is valid */
        wpt_uint32 stAuF:1;
    
        /** When set address2 is not valid */
        wpt_uint32 A2HF:1;
    
        /** When set it indicates TPE has sent the Beacon frame */
        wpt_uint32 bsf:1;
    
        /** This bit filled by rxp when set indicates if the current tsf is smaller
        than received tsf */
        wpt_uint32 rtsf:1;

#ifdef WCN_PRONTO_CSU
        /** No valid header found during parsing. Therefore no checksum was validated */
        wpt_uint32 csuNoValHd:1;

        /** 0 = CSU did not verify TCP/UDP (Transport Layer TL) checksum; 1 = CSU verified TCP/UDP checksum */
        wpt_uint32 csuVerifiedTLChksum:1;

        /** 0 = CSU did not verify IP checksum; 1 = CSU verified IP checksum */
        wpt_uint32 csuVerifiedIPChksum:1;

        /** 0 = BD field checksum is not valid; 1 = BD field checksum is valid */
        wpt_uint32 csuChksumValid:1;

        /** 0 = No TCP/UDP checksum error; 1 = Has TCP/UDP checksum error */
        wpt_uint32 csuTLChksumError:1;

        /** 0 = No IPv4/IPv6 checksum error; 1 = Has IPv4/IPv6 checksum error */
        wpt_uint32 csuIPChksumError:1;
#else /*WCN_PRONTO*/
        /** These two fields are used by SW to carry the Rx Channel number and SCAN bit in RxBD*/
        wpt_uint32 rxChannel:4;
        wpt_uint32 scanLearn:1;
        wpt_uint32 reserved0:1;
#endif /*WCN_PRONTO*/

        /** LLC Removed
        This bit is only used in Libra rsvd for Virgo1.0/Virgo2.0
        Filled by ADU when it is set LLC is removed from packet */
        wpt_uint32 llcr:1;
        
        wpt_uint32 umaByPass:1;
    
        /** This bit is only available in Virgo2.0/libra it is reserved in Virgo1.0
        Robust Management frame. This bit indicates to DPU that the packet is a
        robust management frame which requires decryption(this bit is only valid for
        management unicast encrypted frames)
        1 - Needs decryption
        0 - No decryption required */
        wpt_uint32 rmf:1;
    
        /** 
        This bit is only in Virgo2.0/libra it is reserved in Virgo 1.0
        This 1-bit field indicates to DPU Unicast/BC/MC packet
        0 - Unicast packet
        1 - Broadcast/Multicast packet
        This bit is only valid when RMF bit is 1 */
        wpt_uint32 ub:1;
    
        /** This is the KEY ID extracted from WEP packets and is used for determine
        the RX Key Index to use in the DPU Descriptror.
        This field  is 2bits for virgo 1.0
        And 3 bits in virgo2.0 and Libra
        In virgo2.0/libra it is 3bits for the BC/MC packets */
        wpt_uint32 rxKeyId:3;
        
        /**  (Only used by the DPU)    
        No encryption/decryption
        0: No action
        1: DPU will not encrypt/decrypt the frame, and discard any encryption
        related settings in the PDU descriptor. */
        wpt_uint32 dpuNE:1;
    
        /** 
        This is only available in libra/virgo2.0  it is reserved for virgo1.0
        This bit is filled by RXP and modified by ADU
        This bit indicates to ADU/UMA module that the packet requires 802.11n to
        802.3 frame translation. Once ADU/UMA is done with translation they
        overwrite it with 1'b0/1'b1 depending on how the translation resulted
        When used by ADU 
        0 - No frame translation required
        1 - Frame Translation required
        When used by SW
        0 - Frame translation not done, MPDU header offset points to 802.11 header..
        1 - Frame translation done ;  hence MPDU header offset will point to a
        802.3 header */
        wpt_uint32 ft:1;
    
        /** (Only used by the DPU)
        BD Type 
        00: 'Generic BD', as indicted above
        01: De-fragmentation format 
        10-11: Reserved for future use. */
        wpt_uint32 bdt:2;
        
#else
        wpt_uint32 bdt:2;
        wpt_uint32 ft:1;
        wpt_uint32 dpuNE:1;
        wpt_uint32 rxKeyId:3;
        wpt_uint32 ub:1;
        wpt_uint32 rmf:1;
        wpt_uint32 umaByPass:1;
        wpt_uint32 llcr:1;

#ifdef WCN_PRONTO_CSU
        wpt_uint32 csuIPChksumError:1;
        wpt_uint32 csuTLChksumError:1;
        wpt_uint32 csuChksumValid:1;
        wpt_uint32 csuVerifiedIPChksum:1;
        wpt_uint32 csuVerifiedTLChksum:1;
        wpt_uint32 csuNoValHd:1;
#else /*WCN_PRONTO*/
        wpt_uint32 reserved0:1;
        wpt_uint32 scanLearn:1;
        wpt_uint32 rxChannel:4;
#endif /*WCN_PRONTO*/

        wpt_uint32 rtsf:1;
        wpt_uint32 bsf:1;
        wpt_uint32 A2HF:1;
        wpt_uint32 stAuF:1;
        wpt_uint32 dpuSignature:3;
        wpt_uint32 dpuRF:8;
#endif
    
        /* 0x04 */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** This is used for AMSDU this is the PDU index of the PDU which is the
        one before last PDU; for all non AMSDU frames, this field SHALL be 0.
        Used in ADU (for AMSDU deaggregation) */
        wpt_uint32 penultimatePduIdx:16;
    
#ifdef WCN_PRONTO 
        wpt_uint32 aduFeedback:7;
        wpt_uint32 dpuMagicPacket: 1; 
#else
        wpt_uint32 aduFeedback:8;
#endif //WCN_PRONTO
    
        /** DPU feedback */
        wpt_uint32 dpuFeedback:8;
        
#else
        wpt_uint32 dpuFeedback:8;
#ifdef WCN_PRONTO 
        wpt_uint32 dpuMagicPacket: 1; 
        wpt_uint32 aduFeedback:7;
#else
        wpt_uint32 aduFeedback:8;
#endif //WCN_PRONTO
        wpt_uint32 penultimatePduIdx:16;
#endif
    
        /* 0x08 */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** In case PDUs are linked to the BD, this field indicates the index of
        the first PDU linked to the BD. When PDU count is zero, this field has an
        undefined value. */
        wpt_uint32 headPduIdx:16;
    
        /** In case PDUs are linked to the BD, this field indicates the index of
        the last PDU. When PDU count is zero, this field has an undefined value.*/
        wpt_uint32 tailPduIdx:16;
        
#else
        wpt_uint32 tailPduIdx:16;
        wpt_uint32 headPduIdx:16;
#endif
    
        /* 0x0c */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** The length (in number of bytes) of the MPDU header. 
        Limitation: The MPDU header offset + MPDU header length can never go beyond
        the end of the first PDU */
        wpt_uint32 mpduHeaderLength:8;
    
        /** The start byte number of the MPDU header. 
        The byte numbering is done in the BE format. Word 0x0, bits [31:24] has
        byte index 0. */
        wpt_uint32 mpduHeaderOffset:8;
    
        /** The start byte number of the MPDU data. 
        The byte numbering is done in the BE format. Word 0x0, bits [31:24] has
        byte index 0. Note that this offset can point all the way into the first
        linked PDU.
        Limitation: MPDU DATA OFFSET can not point into the 2nd linked PDU */
        wpt_uint32 mpduDataOffset:9;
    
        /** The number of PDUs linked to the BD. 
        This field should always indicate the correct amount. */
        wpt_uint32 pduCount:7;
#else
    
        wpt_uint32 pduCount:7;
        wpt_uint32 mpduDataOffset:9;
        wpt_uint32 mpduHeaderOffset:8;
        wpt_uint32 mpduHeaderLength:8;
#endif
    
        /* 0x10 */
#ifdef WPT_BIG_BYTE_ENDIAN

        /** This is the length (in number of bytes) of the entire MPDU
        (header and data). Note that the length does not include FCS field. */
        wpt_uint32 mpduLength:16;

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
        wpt_uint32 offloadScanLearn:1;
        wpt_uint32 roamCandidateInd:1;
#else
        wpt_uint32 reserved22:2;
#endif

#ifdef WCN_PRONTO
        wpt_uint32 reserved3: 1;
        wpt_uint32 rxDXEPriorityRouting:1;
#else
        wpt_uint32 reserved3:2;
#endif //WCN_PRONTO


        /** Traffic Identifier
        Indicates the traffic class the frame belongs to. For non QoS frames,
        this field is set to zero. */
        wpt_uint32 tid:4;

        wpt_uint32 reserved4:8;
#else
        wpt_uint32 reserved4:8;
        wpt_uint32 tid:4;
#ifdef WCN_PRONTO
        wpt_uint32 rxDXEPriorityRouting:1;
        wpt_uint32 reserved3: 1;
#else
        wpt_uint32 reserved3:2;
#endif //WCN_PRONTO
#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
        wpt_uint32 roamCandidateInd:1;
        wpt_uint32 offloadScanLearn:1;
#else
        wpt_uint32 reserved22:2;
#endif

        wpt_uint32 mpduLength:16;
#endif

        /* 0x14 */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** (Only used by the DPU)
        The DPU descriptor index is used to calculate where in memory the DPU can
        find the DPU descriptor related to this frame. The DPU calculates the
        address by multiplying this index with the DPU descriptor size and adding
        the DPU descriptors base address. The DPU descriptor contains information
        specifying the encryption and compression type and contains references to
        where encryption keys can be found. */
        wpt_uint32 dpuDescIdx:8;
    
        /** The result from the binary address search on the ADDR1 of the incoming
        frame. See chapter: RXP filter for encoding of this field. */
        wpt_uint32 addr1Index:8;
    
        /** The result from the binary address search on the ADDR2 of the incoming
        frame. See chapter: RXP filter for encoding of this field. */
        wpt_uint32 addr2Index:8;
    
        /** The result from the binary address search on the ADDR3 of the incoming
        frame. See chapter: RXP filter for encoding of this field. */
        wpt_uint32 addr3Index:8;
#else
        wpt_uint32 addr3Index:8;
        wpt_uint32 addr2Index:8;
        wpt_uint32 addr1Index:8;
        wpt_uint32 dpuDescIdx:8;
#endif
    
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** Indicates Rate Index of packet received  */
        wpt_uint32 rateIndex:9;
    
        /** An overview of RXP status information related to receiving the frame.*/
        wpt_uint32 rxpFlags:23; 
    
#else
    
        wpt_uint32 rxpFlags:23;                     /* RxP flags*/
        wpt_uint32 rateIndex:9;
    
#endif
        /* 0x1c, 20 */
        /** The PHY can be programmed to put all the PHY STATS received from the
        PHY when receiving a frame in the BD.  */
        wpt_uint32 phyStats0;                      /* PHY status word 0*/
        wpt_uint32 phyStats1;                      /* PHY status word 1*/
    
        /* 0x24 */
        /** The value of the TSF[31:0] bits at the moment that the RXP start
        receiving a frame from the PHY RX. */
        wpt_uint32 mclkRxTimestamp;                /* Rx timestamp, microsecond based*/
    
        /* 0x28~0x38 */
        /** The bits from the PMI command as received from the PHY RX. */
        wpt_uint32 pmiCmd4to23[5];               /* PMI cmd rcvd from RxP */
    
        /* 0x3c */
#ifdef WCN_PRONTO
#ifdef WPT_BIG_BYTE_ENDIAN
        /** The bits from the PMI command as received from the PHY RX. */
        wpt_uint32 pmiCmd24to25:16;

        /* 16-bit CSU Checksum value for the fragmented receive frames */
        wpt_uint32 csuChecksum:16;
#else
        wpt_uint32 csuChecksum:16;
        wpt_uint32 pmiCmd24to25:16;
#endif
#else /*WCN_PRONTO*/
        /** The bits from the PMI command as received from the PHY RX. */
        wpt_uint32 pmiCmd24to25;
#endif /*WCN_PRONTO*/

        /* 0x40 */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** Gives commands to software upon which host will perform some commands.
        Please refer to following RPE document for description of all different
        values for this field. */
        wpt_uint32 reorderOpcode:4;
    
        wpt_uint32 reserved6:12;
    
        /** Filled by RPE to Indicate to the host up to which slot the host needs
        to forward the packets to upper Mac layer. This field mostly used for AMDPU
        packets */
        wpt_uint32 reorderFwdIdx:6;
    
        /** Filled by RPE which indicates to the host which one of slots in the
        available 64 slots should the host Queue the packet. This field only
        applied to AMPDU packets. */
        wpt_uint32 reorderSlotIdx:6;
        
#ifdef WCN_PRONTO
        wpt_uint32 reserved7: 2;
        wpt_uint32 outOfOrderForward: 1;
        wpt_uint32 reorderEnable: 1;
#else
        wpt_uint32 reserved7:4;
#endif //WCN_PRONTO

#else

#ifdef WCN_PRONTO
        wpt_uint32 reorderEnable: 1;
        wpt_uint32 outOfOrderForward: 1;
        wpt_uint32 reserved7: 2;
#else
        wpt_uint32 reserved7:4;
#endif //WCN_PRONTO
        wpt_uint32 reorderSlotIdx:6;
        wpt_uint32 reorderFwdIdx:6;
        wpt_uint32 reserved6:12;
        wpt_uint32 reorderOpcode:4;
#endif
    
        /* 0x44 */
#ifdef WPT_BIG_BYTE_ENDIAN
        /** reserved8 from a hardware perspective.
        Used by SW to propogate frame type/subtype information */
        wpt_uint32 frameTypeSubtype:6;
        wpt_uint32 rfBand:2;
    
        /** Filled RPE gives the current sequence number in bitmap */
        wpt_uint32 currentPktSeqNo:12;
    
        /** Filled by RPE which gives the sequence number of next expected packet
        in bitmap */
        wpt_uint32 expectedPktSeqNo:12;
#else
        wpt_uint32 expectedPktSeqNo:12;
        wpt_uint32 currentPktSeqNo:12;
        wpt_uint32 rfBand:2;
        wpt_uint32 frameTypeSubtype:6;
#endif
    
        /* 0x48 */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** When set it is the AMSDU subframe */
        wpt_uint32 asf:1;
    
        /** When set it is the First subframe of the AMSDU packet */
        wpt_uint32 esf:1;
    
        /** When set it is the last subframe of the AMSDU packet */
        wpt_uint32 lsf:1;
    
        /** When set it indicates an Errored AMSDU packet */
        wpt_uint32 aef:1;
        
        wpt_uint32 reserved9:4;
    
        /** It gives the order in which the AMSDU packet is processed
        Basically this is a number which increments by one for every AMSDU frame
        received. Mainly for debugging purpose. */
        wpt_uint32 processOrder:4;
    
        /** It is the order of the subframe of AMSDU that is processed by ADU.
        This is reset to 0 when ADU deaggregates the first subframe from a new
        AMSDU and increments by 1 for every new subframe deaggregated within the
        AMSDU, after it reaches 4'hf it stops incrementing. That means host should
        not rely on this field as index for subframe queuing.  Theoretically there
        can be way more than 16 subframes in an AMSDU. This is only used for debug
        purpose, SW should use LSF and FSF bits to determine first and last
        subframes. */
        wpt_uint32 sybFrameIdx:4;
    
        /** Filled by ADU this is the total AMSDU size */
        wpt_uint32 totalMsduSize:16;
#else
        wpt_uint32 totalMsduSize:16;
        wpt_uint32 sybFrameIdx:4;
        wpt_uint32 processOrder:4;
        wpt_uint32 reserved9:4;
        wpt_uint32 aef:1;
        wpt_uint32 lsf:1;
        wpt_uint32 esf:1;
        wpt_uint32 asf:1;
#endif

} WDI_RxBdType;

/*---------------------------------------------------------------------------         
  WDI_RxFcBdType - The format of the RX special flow control BD 
---------------------------------------------------------------------------*/
typedef struct 
{
        /* 0x00 */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** (Only used by the DPU)
        This routing flag indicates the WQ number to which the DPU will push the
        frame after it finished processing it. */
        wpt_uint32 dpuRF:8;
    
        /** This is DPU sig inserted by RXP. Signature on RA's DPU descriptor */
        wpt_uint32 dpuSignature:3;
    
        /** When set Sta is authenticated. SW needs to set bit
        addr2_auth_extract_enable in rxp_config2 register. Then RXP will use bit 3
        in DPU sig to say whether STA is authenticated or not. In this case only
        lower 2bits of DPU Sig is valid */
        wpt_uint32 stAuF:1;
    
        /** When set address2 is not valid */
        wpt_uint32 A2HF:1;
    
        /** When set it indicates TPE has sent the Beacon frame */
        wpt_uint32 bsf:1;
    
        /** This bit filled by rxp when set indicates if the current tsf is smaller
        than received tsf */
        wpt_uint32 rtsf:1;
    
        /** These two fields are used by SW to carry the Rx Channel number and SCAN bit in RxBD*/
        wpt_uint32 rxChannel:4;
        wpt_uint32 scanLearn:1;

        wpt_uint32 reserved0:1;
    
        /** LLC Removed
        This bit is only used in Libra rsvd for Virgo1.0/Virgo2.0
        Filled by ADU when it is set LLC is removed from packet */
        wpt_uint32 llcr:1;
        
        wpt_uint32 umaByPass:1;
    
        /** This bit is only available in Virgo2.0/libra it is reserved in Virgo1.0
        Robust Management frame. This bit indicates to DPU that the packet is a
        robust management frame which requires decryption(this bit is only valid for
        management unicast encrypted frames)
        1 - Needs decryption
        0 - No decryption required */
        wpt_uint32 rmf:1;
    
        /** 
        This bit is only in Virgo2.0/libra it is reserved in Virgo 1.0
        This 1-bit field indicates to DPU Unicast/BC/MC packet
        0 - Unicast packet
        1 - Broadcast/Multicast packet
        This bit is only valid when RMF bit is 1 */
        wpt_uint32 ub:1;
    
        /** This is the KEY ID extracted from WEP packets and is used for determine
        the RX Key Index to use in the DPU Descriptror.
        This field  is 2bits for virgo 1.0
        And 3 bits in virgo2.0 and Libra
        In virgo2.0/libra it is 3bits for the BC/MC packets */
        wpt_uint32 rxKeyId:3;
        
        /**  (Only used by the DPU)    
        No encryption/decryption
        0: No action
        1: DPU will not encrypt/decrypt the frame, and discard any encryption
        related settings in the PDU descriptor. */
        wpt_uint32 dpuNE:1;
    
        /** 
        This is only available in libra/virgo2.0  it is reserved for virgo1.0
        This bit is filled by RXP and modified by ADU
        This bit indicates to ADU/UMA module that the packet requires 802.11n to
        802.3 frame translation. Once ADU/UMA is done with translation they
        overwrite it with 1'b0/1'b1 depending on how the translation resulted
        When used by ADU 
        0 - No frame translation required
        1 - Frame Translation required
        When used by SW
        0 - Frame translation not done, MPDU header offset points to 802.11 header..
        1 - Frame translation done ;  hence MPDU header offset will point to a
        802.3 header */
        wpt_uint32 ft:1;
    
        /** (Only used by the DPU)
        BD Type 
        00: 'Generic BD', as indicted above
        01: De-fragmentation format 
        10-11: Reserved for future use. */
        wpt_uint32 bdt:2;
        
#else
        wpt_uint32 bdt:2;
        wpt_uint32 ft:1;
        wpt_uint32 dpuNE:1;
        wpt_uint32 rxKeyId:3;
        wpt_uint32 ub:1;
        wpt_uint32 rmf:1;
        wpt_uint32 reserved1:1;
        wpt_uint32 llc:1;
        wpt_uint32 reserved0:1;
        wpt_uint32 scanLearn:1;
        wpt_uint32 rxChannel:4;
        wpt_uint32 rtsf:1;
        wpt_uint32 bsf:1;
        wpt_uint32 A2HF:1;
        wpt_uint32 stAuF:1;
        wpt_uint32 dpuSignature:3;
        wpt_uint32 dpuRF:8;
#endif
    
        /* 0x04 */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** This is used for AMSDU this is the PDU index of the PDU which is the
        one before last PDU; for all non AMSDU frames, this field SHALL be 0.
        Used in ADU (for AMSDU deaggregation) */
        wpt_uint32 penultimatePduIdx:16;
    
        wpt_uint32 aduFeedback:8;
    
        /** DPU feedback */
        wpt_uint32 dpuFeedback:8;
        
#else
        wpt_uint32 dpuFeedback:8;
        wpt_uint32 aduFeedback:8;
        wpt_uint32 penultimatePduIdx:16;
#endif
    
        /* 0x08 */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** In case PDUs are linked to the BD, this field indicates the index of
        the first PDU linked to the BD. When PDU count is zero, this field has an
        undefined value. */
        wpt_uint32 headPduIdx:16;
    
        /** In case PDUs are linked to the BD, this field indicates the index of
        the last PDU. When PDU count is zero, this field has an undefined value.*/
        wpt_uint32 tailPduIdx:16;
        
#else
        wpt_uint32 tailPduIdx:16;
        wpt_uint32 headPduIdx:16;
#endif
    
        /* 0x0c */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** The length (in number of bytes) of the MPDU header. 
        Limitation: The MPDU header offset + MPDU header length can never go beyond
        the end of the first PDU */
        wpt_uint32 mpduHeaderLength:8;
    
        /** The start byte number of the MPDU header. 
        The byte numbering is done in the BE format. Word 0x0, bits [31:24] has
        byte index 0. */
        wpt_uint32 mpduHeaderOffset:8;
    
        /** The start byte number of the MPDU data. 
        The byte numbering is done in the BE format. Word 0x0, bits [31:24] has
        byte index 0. Note that this offset can point all the way into the first
        linked PDU.
        Limitation: MPDU DATA OFFSET can not point into the 2nd linked PDU */
        wpt_uint32 mpduDataOffset:9;
    
        /** The number of PDUs linked to the BD. 
        This field should always indicate the correct amount. */
        wpt_uint32 pduCount:7;
#else
    
        wpt_uint32 pduCount:7;
        wpt_uint32 mpduDataOffset:9;
        wpt_uint32 mpduHeaderOffset:8;
        wpt_uint32 mpduHeaderLength:8;
#endif
    
        /* 0x10 */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** This is the length (in number of bytes) of the entire MPDU 
        (header and data). Note that the length does not include FCS field. */
        wpt_uint32 mpduLength:16;
    
        wpt_uint32 reserved3:4;
    
        /** Traffic Identifier
        Indicates the traffic class the frame belongs to. For non QoS frames,
        this field is set to zero. */
        wpt_uint32 tid:4;
        
        wpt_uint32 reserved4:7;
        wpt_uint32 fc:1;        //if set then its special flow control BD.
#else
        wpt_uint32 fc:1;        //if set then its special flow control BD.
        wpt_uint32 reserved4:7;
        wpt_uint32 tid:4;
        wpt_uint32 reserved3:4;
        wpt_uint32 mpduLength:16;
#endif
    
        /* 0x14 */
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** (Only used by the DPU)
        The DPU descriptor index is used to calculate where in memory the DPU can
        find the DPU descriptor related to this frame. The DPU calculates the
        address by multiplying this index with the DPU descriptor size and adding
        the DPU descriptors base address. The DPU descriptor contains information
        specifying the encryption and compression type and contains references to
        where encryption keys can be found. */
        wpt_uint32 dpuDescIdx:8;
    
        /** The result from the binary address search on the ADDR1 of the incoming
        frame. See chapter: RXP filter for encoding of this field. */
        wpt_uint32 addr1Index:8;
    
        /** The result from the binary address search on the ADDR2 of the incoming
        frame. See chapter: RXP filter for encoding of this field. */
        wpt_uint32 addr2Index:8;
    
        /** The result from the binary address search on the ADDR3 of the incoming
        frame. See chapter: RXP filter for encoding of this field. */
        wpt_uint32 addr3Index:8;
#else
        wpt_uint32 addr3Index:8;
        wpt_uint32 addr2Index:8;
        wpt_uint32 addr1Index:8;
        wpt_uint32 dpuDescIdx:8;
#endif
    
#ifdef WPT_BIG_BYTE_ENDIAN
    
        /** Indicates Rate Index of packet received  */
        wpt_uint32 rateIndex:9;
    
        /** An overview of RXP status information related to receiving the frame.*/
        wpt_uint32 rxpFlags:23; 
    
#else
    
        wpt_uint32 rxpFlags:23;                     /* RxP flags*/
        wpt_uint32 rateIndex:9;
    
#endif
        /* 0x1c, 20 */
        /** The PHY can be programmed to put all the PHY STATS received from the
        PHY when receiving a frame in the BD.  */
        wpt_uint32 phyStats0;                      /* PHY status word 0*/
        wpt_uint32 phyStats1;                      /* PHY status word 1*/
    
        /* 0x24 */
        /** The value of the TSF[31:0] bits at the moment that the RXP start
        receiving a frame from the PHY RX. */
        wpt_uint32 mclkRxTimestamp;                /* Rx timestamp, microsecond based*/
    
       /* 0x28 - 0x2c*/     
#ifdef WPT_BIG_BYTE_ENDIAN  
        /** One bit per STA. Bit X for STA id X, X=0~7. When set, corresponding STA is valid in FW's STA table.*/
        wpt_uint32 fcSTAValidMask:16;
        /** One bit per STA. Bit X for STA id X, X=0~7. Valid only when corresponding bit in fcSTAValisMask is set.*/
        wpt_uint32 fcSTAPwrSaveStateMask:16;
        /** One bit per STA. Bit X for STA id X, X=0~7. Valid only when corresponding bit in fcSTAValisMask is set. 
        When set, corresponding fcSTAThreshEnableMask bit in previous flow control request packet frame was enabled 
        AND the STA TxQ length is lower than configured fcSTAThresh<X> value. */        
        wpt_uint32 fcSTAThreshIndMask:16;
        /** Bit 0 unit: 1=BD count(Libra SoftAP project default). 0=packet count. Bit 7-1: Reserved */
        wpt_uint32 fcSTATxQStatus:16;
#else
        wpt_uint32 fcSTATxQStatus:16;
        wpt_uint32 fcSTAThreshIndMask:16;
        wpt_uint32 fcSTAPwrSaveStateMask:16;
        wpt_uint32 fcSTAValidMask:16;
#endif
        /* 0x30 */
#ifdef WPT_BIG_BYTE_ENDIAN 
        wpt_uint32 fcStaTxDisabledBitmap:16;
        wpt_uint32 reserved5:16;
#else   
        wpt_uint32 reserved5:16;
        wpt_uint32 fcStaTxDisabledBitmap:16;
#endif
        
        // with HAL_NUM_STA as 12 
        /* 0x34 to 0x3A*/
        wpt_uint32  fcSTATxQLen[12];            // one byte per STA. 
        wpt_uint32  fcSTACurTxRate[12];         // current Tx rate for each sta. 

} WDI_FcRxBdType; //flow control BD

/*---------------------------------------------------------------------------         
  WDI_TxBdType - The format of the TX BD 
---------------------------------------------------------------------------*/
typedef struct 
{
        /* 0x00 */
#ifdef WPT_BIG_BYTE_ENDIAN
        /** (Only used by the DPU) This routing flag indicates the WQ number to
        which the DPU will push the frame after it finished processing it. */
        wpt_uint32 dpuRF:8;
    
        /** DPU signature. Filled by Host in Virgo 1.0 but by ADU in Virgo 2.0 */
        wpt_uint32 dpuSignature:3;

#ifdef WCN_PRONTO        
        /** Reserved  */
        wpt_uint32 reserved0:2;

         /** Set to '1' to terminate the current AMPDU session. Added based on the 
        request for WiFi Display */
        wpt_uint32 terminateAMPDU:1;

       /** Bssid index to indicate ADU to use which of the 4 default MAC address 
        to use while 802.3 to 802.11 translation in case search in ADU UMA table 
        fails. The default MAC address should be appropriately programmed in the 
        uma_tx_default_wmacaddr_u(_1,_2,_3) and uma_tx_default_wmacaddr_l(_1,_2,_3)
         registers */
        wpt_uint32 umaBssidIdx:2;

        /** Set to 1 to enable uma filling the BD when FT is not enabled.
        Ignored when FT is enabled. */
        wpt_uint32 umaBDEnable:1;

        /** (Only used by the CSU)
        0: No action
        1: Host will indicate TCP/UPD header start location and provide pseudo header value in BD.
        */
        wpt_uint32 csuSWMode:1;

        /** Enable/Disable CSU on TX direction.
        0: Disable Checksum Unit (CSU) for Transmit.
        1: Enable 
        */
        wpt_uint32 csuTXEnable:1;

        /** Enable/Disable Transport layer Checksum in CSU
        0: Disable TCP UDP checksum generation for TX.
        1: Enable TCP UDP checksum generation for TX.
        */
        wpt_uint32 csuEnableTLCksum:1;

        /** Enable/Disable IP layer Checksum in CSU
        0: Disable IPv4/IPv6 checksum generation for TX
        1: Enable  IPv4/IPv6 checksum generation for TX
        */
        wpt_uint32 csuEnableIPCksum:1;

        /** Filled by CSU to indicate whether transport layer Checksum is generated by CSU or not
        0: TCP/UDP checksum is being generated for TX.
        1: TCP/UDP checksum is NOT being generated for TX.
         */
        wpt_uint32 csuTLCksumGenerated:1;

        /** Filled by CSU in error scenario
        1: No valid header found during parsing. Therefore no checksum was validated.
        0: Valid header found
        */
        wpt_uint32 csuNoValidHeader:1;
#else /*WCN_PRONTO*/
        wpt_uint32 reserved0:12;
#endif /*WCN_PRONTO*/

        /** Only available in Virgo 2.0 and reserved in Virgo 1.0.
        This bit indicates to DPU that the packet is a robust management frame
        which requires  encryption(this bit is only valid for certain management
        frames)
        1 - Needs encryption
        0 - No encrytion required
        It is only set when Privacy bit=1 AND type/subtype=Deauth, Action,
        Disassoc. Otherwise it should always be 0. */
        wpt_uint32 rmf:1;
    
        /** This bit is only in Virgo2.0/libra it is reserved in Virgo 1.0
        This 1-bit field indicates to DPU Unicast/BC/MC packet
        0 - Unicast packet
        1 - Broadcast/Multicast packet
        This bit is valid only if RMF bit is set */
        wpt_uint32 ub:1;
    
        wpt_uint32 reserved1:1;
    
        /**  This bit is only in Virgo2.0/libra it is reserved in Virgo 1.0
        This bit indicates TPE has to assert the TX complete interrupt.
        0 - no interrupt
        1 - generate interrupt */
        wpt_uint32 txComplete1:1;
        wpt_uint32 fwTxComplete0:1;
        
        /** (Only used by the DPU)
        No encryption/decryption
        0: No action
        1: DPU will not encrypt/decrypt the frame, and discard any encryption
        related settings in the PDU descriptor. */
        wpt_uint32 dpuNE:1;
    
        
        /** This is only available in libra/virgo2.0  it is reserved for virgo1.0
        This bit indicates to ADU/UMA module that the packet requires 802.11n
        to 802.3 frame translation. When used by ADU 
        0 - No frame translation required
        1 - Frame Translation required */
        wpt_uint32 ft:1;
    
        /** BD Type 
        00: 'Generic BD', as indicted above
        01: De-fragmentation format 
        10-11: Reserved for future use. */
        wpt_uint32 bdt:2;
#else
        wpt_uint32 bdt:2;
        wpt_uint32 ft:1;
        wpt_uint32 dpuNE:1;
        wpt_uint32 fwTxComplete0:1; 
        wpt_uint32 txComplete1:1;
        wpt_uint32 reserved1:1;
        wpt_uint32 ub:1;
        wpt_uint32 rmf:1;
#ifdef WCN_PRONTO        
        wpt_uint32 csuNoValidHeader:1;
        wpt_uint32 csuTLCksumGenerated:1;
        wpt_uint32 csuEnableIPCksum:1;
        wpt_uint32 csuEnableTLCksum:1;
        wpt_uint32 csuTXEnable:1;
        wpt_uint32 csuSWMode:1;
        wpt_uint32 umaBDEnable:1;
        wpt_uint32 umaBssidIdx:2;
        wpt_uint32 terminateAMPDU:1;
        wpt_uint32 reserved0:2;
#else /*WCN_PRONTO*/
        wpt_uint32 reserved0:12;
#endif /*WCN_PRONTO*/
        wpt_uint32 dpuSignature:3;
        wpt_uint32 dpuRF:8;
#endif
    
        /* 0x04 */
#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved2:16; /* MUST BE 0 otherwise triggers BMU error*/
        wpt_uint32 aduFeedback:8;
    
        /* DPU feedback in Tx path.*/
        wpt_uint32 dpuFeedback:8;
    
#else
        wpt_uint32 dpuFeedback:8;
        wpt_uint32 aduFeedback:8;
        wpt_uint32 reserved2:16;
#endif
    
        /* 0x08 */
#ifdef WPT_BIG_BYTE_ENDIAN
        /** It is initially filled by DXE then if encryption is on, then DPU will
        overwrite these fields. In case PDUs are linked to the BD, this field
        indicates the index of the first PDU linked to the BD. When PDU count is
        zero, this field has an undefined value. */
        wpt_uint32 headPduIdx:16;
    
        /**  It is initially filled by DXE then if encryption is on, then DPU will
        overwrite these fields.In case PDUs are linked to the BD, this field
        indicates the index of the last PDU. When PDU count is zero, this field
        has an undefined value. */
        wpt_uint32 tailPduIdx:16;
#else
        wpt_uint32 tailPduIdx:16;
        wpt_uint32 headPduIdx:16;
#endif
    
        /* 0x0c */
#ifdef WPT_BIG_BYTE_ENDIAN
        /** This is filled by Host in Virgo 1.0 but it gets changed by ADU in
        Virgo2.0/Libra. The length (in number of bytes) of the MPDU header.
        Limitation: The MPDU header offset + MPDU header length can never go beyond
        the end of the first PDU */
        wpt_uint32 mpduHeaderLength:8;
    
        /** This is filled by Host in Virgo 1.0 but it gets changed by ADU in
        Virgo2.0/Libra. The start byte number of the MPDU header. The byte numbering
        is done in the BE format. Word 0x0, bits [31:24] has byte index 0. */
        wpt_uint32 mpduHeaderOffset:8;
    
        /** This is filled by Host in Virgo 1.0 but it gets changed by ADU in
        Virgo2.0/Libra. The start byte number of the MPDU data.  The byte numbering
        is done in the BE format. Word 0x0, bits [31:24] has byte index 0.
        Note that this offset can point all the way into the first linked PDU. 
        Limitation: MPDU DATA OFFSET can not point into the 2nd linked PDU */
        wpt_uint32 mpduDataOffset:9;
    
        /** It is initially filled by DXE then if encryption is on, then DPU will
        overwrite these fields. The number of PDUs linked to the BD. This field
        should always indicate the correct amount. */
        wpt_uint32 pduCount:7;
#else
        wpt_uint32 pduCount:7;
        wpt_uint32 mpduDataOffset:9;
        wpt_uint32 mpduHeaderOffset:8;
        wpt_uint32 mpduHeaderLength:8;
#endif
    
        /* 0x10 */
#ifdef WPT_BIG_BYTE_ENDIAN
        /** This is filled by Host in Virgo 1.0 but it gets changed by ADU in
        Virgo2.0/LibraMPDU length.This covers MPDU header length + MPDU data length.
        This does not include FCS. For single frame transmission, PSDU size is
        mpduLength + 4.*/
        wpt_uint32 mpduLength:16;
    
        wpt_uint32 reserved3:2;
        /** Sequence number insertion by DPU
        00: Leave sequence number as is, as filled by host
        01: DPU to insert non TID based sequence number (If it is not TID based,
        then how does DPU know what seq to fill? Is this the non-Qos/Mgmt sequence
        number?
        10: DPU to insert a sequence number based on TID.
        11: Reserved */
        wpt_uint32 bd_ssn:2;
    
        /** Traffic Identifier
        Indicates the traffic class the frame belongs to. For non QoS frames, this
        field is set to zero. */
        wpt_uint32 tid:4;
        
        wpt_uint32 reserved4:8;
    
#else
        wpt_uint32 reserved4:8;
        wpt_uint32 tid:4;
        wpt_uint32 bd_ssn:2;
        wpt_uint32 reserved3:2;
        wpt_uint32 mpduLength:16;
#endif
    
        /* 0x14 */
#ifdef WPT_BIG_BYTE_ENDIAN
        /** (Only used by the DPU)
        This is filled by Host in Virgo 1.0 but it gets filled by ADU in
        Virgo2.0/Libra. The DPU descriptor index is used to calculate where in
        memory the DPU can find the DPU descriptor related to this frame. The DPU
        calculates the address by multiplying this index with the DPU descriptor
        size and adding the DPU descriptors base address. The DPU descriptor
        contains information specifying the encryption and compression type and
        contains references to where encryption keys can be found. */
        wpt_uint32 dpuDescIdx:8;
    
        /** This is filled by Host in Virgo 1.0 but it gets filled by ADU in
        Virgo2.0/Libra. The STAid of the RA address */
        wpt_uint32 staIndex:8;
    
        /** A field passed on to TPE which influences the ACK policy to be used for
        this frame
        00 - Iack
        01,10,11 - No Ack */
        wpt_uint32 ap:2;
    
        /** Overwrite option for the transmit rate
        00: Use rate programmed in the TPE STA descriptor
        01: Use TPE BD rate 1
        10: Use TPE BD rate 2
        11: Delayed Use TPE BD rate 3 */
        wpt_uint32 bdRate:2;
    
        /** 
        This is filled by Host in Virgo 1.0 but it gets filled by ADU in
        Virgo2.0/Libra. Queue ID */
        wpt_uint32 queueId:5;
    
        wpt_uint32 reserved5:7;
#else
        wpt_uint32 reserved5:7;
        wpt_uint32 queueId:5;
        wpt_uint32 bdRate:2;
        wpt_uint32 ap:2;
        wpt_uint32 staIndex:8;
        wpt_uint32 dpuDescIdx:8;
#endif

        wpt_uint32 txBdSignature;

        /* 0x1C */
        wpt_uint32 reserved6;
        /* 0x20 */
        /* Timestamp filled by DXE. Timestamp for current transfer */
        wpt_uint32 dxeH2BStartTimestamp;
    
        /* 0x24 */
        /* Timestamp filled by DXE. Timestamp for previous transfer */
        wpt_uint32 dxeH2BEndTimestamp;

#ifdef WCN_PRONTO
#ifdef WPT_BIG_BYTE_ENDIAN
        /** 10 bit value to indicate the start of TCP UDP frame relative to 
         * the first IP frame header */
        wpt_uint32 csuTcpUdpStartOffset:10;

        /** 16 bit pseudo header for TCP UDP used by CSU to generate TCP/UDP 
         * frame checksum */
        wpt_uint32 csuPseudoHeaderCksum:16;

        wpt_uint32 reserved7:6;
#else
        wpt_uint32 reserved7:6;
        wpt_uint32 csuPseudoHeaderCksum:16;
        wpt_uint32 csuTcpUdpStartOffset:10;
#endif
#endif /*WCN_PRONTO*/

} WDI_TxBdType;

/*---------------------------------------------------------------------------         
  WDI_RxDeFragBdType - The format of the RX BD Defragmented 
---------------------------------------------------------------------------*/
typedef struct 
{
        /* 0x00 */
#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved1:30;
        wpt_uint32 bdt:2;
#else
        wpt_uint32 bdt:2;
        wpt_uint32 reserved1:30;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved2:24;
        wpt_uint32 dpuFeedBack:8;
#else
        wpt_uint32 dpuFeedBack:8;
        wpt_uint32 reserved2:24;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved3:16;
        wpt_uint32 frag0BdIdx:16;
#else
        wpt_uint32 frag0BdIdx:16;
        wpt_uint32 reserved3:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved4:16;
        wpt_uint32 frag1BdIdx:16;
#else
        wpt_uint32 frag1BdIdx:16;
        wpt_uint32 reserved4:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 frag2BdIdx:16;
        wpt_uint32 reserved5:16;
#else
        wpt_uint32 frag2BdIdx:16;
        wpt_uint32 reserved5:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved6:16;
        wpt_uint32 frag3BdIdx:16;
#else
        wpt_uint32 frag3BdIdx:16;
        wpt_uint32 reserved6:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved7:16;
        wpt_uint32 frag4BdIdx:16;
#else
        wpt_uint32 frag4BdIdx:16;
        wpt_uint32 reserved7:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved8:16;
        wpt_uint32 frag5BdIdx:16;
#else
        wpt_uint32 frag5BdIdx:16;
        wpt_uint32 reserved8:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved9:16;
        wpt_uint32 frag6BdIdx:16;
#else
        wpt_uint32 frag6BdIdx:16;
        wpt_uint32 reserved9:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved10:16;
        wpt_uint32 frag7BdIdx:16;
#else
        wpt_uint32 frag7BdIdx:16;
        wpt_uint32 reserved10:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved11:16;
        wpt_uint32 frag8BdIdx:16;
#else
        wpt_uint32 frag8BdIdx:16;
        wpt_uint32 reserved11:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved12:16;
        wpt_uint32 frag9BdIdx:16;
#else
        wpt_uint32 frag9BdIdx:16;
        wpt_uint32 reserved12:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved13:16;
        wpt_uint32 frag10BdIdx:16;
#else
        wpt_uint32 frag10BdIdx:16;
        wpt_uint32 reserved13:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved14:16;
        wpt_uint32 frag11BdIdx:16;
#else
        wpt_uint32 frag11BdIdx:16;
        wpt_uint32 reserved14:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved15:16;
        wpt_uint32 frag12BdIdx:16;
#else
        wpt_uint32 frag12BdIdx:16;
        wpt_uint32 reserved15:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 reserved16:16;
        wpt_uint32 frag13BdIdx:16;
#else
        wpt_uint32 frag13BdIdx:16;
        wpt_uint32 reserved16:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 frag14BdIdx:16;
        wpt_uint32 reserved17:16;
#else
        wpt_uint32 frag14BdIdx:16;
        wpt_uint32 reserved17:16;
#endif

#ifdef WPT_BIG_BYTE_ENDIAN
        wpt_uint32 frag15BdIdx:16;
        wpt_uint32 reserved18:16;
#else
        wpt_uint32 frag15BdIdx:16;
        wpt_uint32 reserved18:16;
#endif

} WDI_RxDeFragBdType;

#endif /*WLAN_QCT_WDI_BD_H*/
