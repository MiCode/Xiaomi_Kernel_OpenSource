/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*
 * File: $Header: //depot/software/projects/feature_branches/gen5_phase1/os/linux/classic/ap/apps/ssm/lib/aniSsmEapol.c#2 $ 
 *
 * Contains definitions of various utilities for EAPoL frame
 * parsing and creation.
 *
 * Author:      Mayank D. Upadhyay
 * Date:        19-June-2002
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */
#include "vos_utils.h"
#include <bapRsnAsfPacket.h>
#include <bapRsnErrors.h>
#include <bapRsnSsmEapol.h>
#include "bapRsn8021xFsm.h"
#include "vos_memory.h"

//#include "aniSsmUtils.h"


//TODO: Put these in an array after EAPOL_TYPE is made an enum
#define ANI_EAPOL_TYPE_PACKET_STR    "EAP"
#define ANI_EAPOL_TYPE_START_STR     "START"
#define ANI_EAPOL_TYPE_LOGOFF_STR    "LOGOFF"
#define ANI_EAPOL_TYPE_KEY_STR       "KEY"
#define ANI_EAPOL_TYPE_ASF_ALERT_STR "ALERT"
#define ANI_EAPOL_TYPE_UNKNOWN_STR   "UNKNOWN"

/**
 * The EAPOL packet is structured as follows:
 */
#define DST_MAC_POS 0
#define SRC_MAC_POS 6
#define ETHER_PROTO_POS 12
#define EAPOL_VERSION_POS 14
#define ANI_EAPOL_TYPE_POS 15
#define EAPOL_BODY_LEN_POS 16
#define EAPOL_BODY_POS EAPOL_RX_HEADER_SIZE

#define EAPOL_BODY_LEN_SIZE 2

#define ANI_SSM_LEGACY_RC4_KEY_SIGN_OFFSET (EAPOL_BODY_POS + 28)

/**
 * Bitmasks for the RSN Key Information field
 */
#define ANI_SSM_RSN_KEY_DESC_VERS_MASK 0x0007
#define ANI_SSM_RSN_UNICAST_MASK       0x0008
#define ANI_SSM_RSN_KEY_INDEX_MASK     0x0030
#define ANI_SSM_RSN_INSTALL_MASK       0x0040
#define ANI_SSM_RSN_ACK_MASK           0x0080
#define ANI_SSM_RSN_MIC_MASK           0x0100
#define ANI_SSM_RSN_SECURE_MASK        0x0200
#define ANI_SSM_RSN_ERROR_MASK         0x0400
#define ANI_SSM_RSN_REQUEST_MASK       0x0800
#define ANI_SSM_RSN_ENC_KEY_DATA_MASK  0x1000

#define ANI_SSM_RSN_KEY_DESC_VERS_OFFSET  0
#define ANI_SSM_RSN_KEY_INDEX_OFFSET      4

#define ANI_SSM_RSN_KEY_MIC_OFFSET (EAPOL_BODY_POS + 77)

/**
 * Other hard coded values for convenience:
 */
static const v_U8_t 
ANI_ETH_P_EAPOL_BYTES[2] = {0x00, 0x03};//BT-AMP security type{0x88, 0x8e};
static const v_U8_t 
EAPOL_VERSION_BYTES[1] = {EAPOL_VERSION_1};
static const v_U8_t 
ANI_EAPOL_TYPE_PACKET_BYTES[1] = {ANI_EAPOL_TYPE_PACKET};
static const v_U8_t 
ANI_EAPOL_TYPE_START_BYTES[1] = {ANI_EAPOL_TYPE_START};
static const v_U8_t 
ANI_EAPOL_TYPE_LOGOFF_BYTES[1] = {ANI_EAPOL_TYPE_LOGOFF};
static const v_U8_t 
ANI_EAPOL_TYPE_KEY_BYTES[1] = {ANI_EAPOL_TYPE_KEY};
static const v_U8_t 
ANI_EAPOL_TYPE_ASF_ALERT_BYTES[1] = {ANI_EAPOL_TYPE_ASF_ALERT};
static const v_U8_t 
ZERO_BYTES[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static v_U8_t BAP_RSN_LLC_HEADER[] =  {0xAA, 0xAA, 0x03, 0x00, 0x19, 0x58 };



static int
parseRsnKeyDesc(tAniPacket *packet, 
                tAniEapolRsnKeyDesc **rsnDescPtr);

static int
parseRsnKeyInfo(tAniPacket *packet, 
                tAniRsnKeyInfo *info);

static int
writeRsnKeyDesc(tAniPacket *packet,
                tAniEapolRsnKeyDesc *rsnDesc,
                v_U8_t keyDescType);

static int
writeRsnKeyInfo(tAniPacket *packet, tAniRsnKeyInfo *info);

static int
writeRsnKeyMic(v_U32_t cryptHandle,
               tAniPacket *eapolFrame, 
               tAniEapolRsnKeyDesc *rsnDesc,
               v_U8_t *micKey, 
               v_U32_t micKeyLen);

static int
checkRsnKeyMic(v_U32_t cryptHandle,
               tAniPacket *eapolFrame,
               tAniEapolRsnKeyDesc *rsnDesc,
               v_U8_t *micKey,
               v_U32_t micKeyLen);

extern void authEapolHandler( tAuthRsnFsm *fsm, tAniPacket *eapolFrame,
                 tAniMacAddr dstMac, 
                 tAniMacAddr srcMac,
                 v_U8_t *type);
extern void suppEapolHandler( tSuppRsnFsm *fsm, tAniPacket *eapolFrame,
                 tAniMacAddr dstMac, 
                 tAniMacAddr srcMac,
                 v_U8_t *type);

/**
 * addEapolHeaders
 *
 * FUNCTION:
 * Prepends the EAPOL header to a packet.
 *
 * ASSUMPTIONS:
 * The packet has enough space available for prepending the EAPOL
 * header.
 *
 * @param packet the packet to prepend to
 * @param dstMac the MAC address of the destination (authenticator)
 * @param srcMac the MAC address of the source (supplicant)
 * @param eapolType the EAPOL-Type field
 *
 * @return ANI_OK if the operation succeeds
 */
static int
addEapolHeaders(tAniPacket *packet, 
                tAniMacAddr dstMac, 
                tAniMacAddr srcMac, 
                v_U8_t eapolType)
{
    int retVal;
    v_U16_t len;
  
    do
    {
        retVal = aniAsfPacketGetLen(packet);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        len = retVal;
        retVal = aniAsfPacketPrepend16(packet, len);

        retVal = aniAsfPacketPrepend8(packet, eapolType);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = aniAsfPacketPrependBuffer(packet, EAPOL_VERSION_BYTES, 1);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = aniAsfPacketPrependBuffer(packet, ANI_ETH_P_EAPOL_BYTES, 2);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        //Since TL expects SNAP header in all packets we send, put it in
        retVal = aniAsfPacketPrependBuffer(packet, BAP_RSN_LLC_HEADER, sizeof(BAP_RSN_LLC_HEADER));
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        //packet length
        len += 6/*length + eapolType+version + eth_type*/ + sizeof(BAP_RSN_LLC_HEADER);
        retVal = aniAsfPacketPrepend16(packet, len);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = aniAsfPacketPrependBuffer(packet, srcMac, sizeof(tAniMacAddr));
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = aniAsfPacketPrependBuffer(packet, dstMac, sizeof(tAniMacAddr));
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;
    }while( 0 );

    return retVal;
}

/**
 * aniEapolWriteStart
 *
 * FUNCTION:
 * Writes an EAPOL-Start frame to the packet. It is only used by the
 * supplicant.
 *
 * LOGIC:
 * Prepend the appropriate EAPOL header to the packet. There is no
 * EAPOL payload for this kind of frame.
 *
 * ASSUMPTIONS:
 * The packet has enough space available for prepending the header.
 *
 * @param packet the packet to which the frame should be written
 * @param dstMac the MAC address of the destination (authenticator)
 * @param srcMac the MAC address of the source (supplicant)
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniEapolWriteStart(tAniPacket *packet, 
                   tAniMacAddr dstMac,
                   tAniMacAddr srcMac)
{
    return ( addEapolHeaders(packet, dstMac, srcMac, ANI_EAPOL_TYPE_START) );
}

/**
 * aniEapolWriteEapPacket
 *
 * FUNCTION:
 * Writes the EAPOL/EAP-Packet frame headers. It is used
 * by both the authenticator and the supplicant. This creates an EAPOL
 * frame that is carrying an EAP message as its payload.
 *
 * LOGIC:
 * Prepend the appropriate EAPOL header to the packet.
 *
 * ASSUMPTIONS:
 * The EAP message (ie., the payload) is already available in the
 * packet and that the packet has enough space available for
 * prepending the EAPOL header.
 *
 * @param packet the packet containing the EAP message
 * @param dstMac the MAC address of the destination (authenticator)
 * @param srcMac the MAC address of the source (supplicant)
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniEapolWriteEapPacket(tAniPacket *eapPacket, 
                       tAniMacAddr dstMac, 
                       tAniMacAddr srcMac)
{
    return( addEapolHeaders(eapPacket, dstMac, srcMac, ANI_EAPOL_TYPE_PACKET) );
}

/**
 * aniEapolParse
 *
 * FUNCTION:
 * Parses an EAPoL frame to the first level of headers (no EAP
 * headers are parsed). 
 *
 * NOTE: This is a non-destructive read, that is the
 * headers are not stripped off the packet. However, any additional
 * data at  the end of the packet, beyond what the EAPoL headers encode
 * will be stripped off.
 *
 * @param packet the packet containing the EAPoL frame to parse
 * @param dstMac a pointer to set to the location of the destination
 * MAC address
 * @param srcMac a pointer to set to the location of the source
 * MAC address
 * @param type a pointer to set to the location of the EAPOL type
 * field.
 *
 * @return the non-negative length of the EAPOL payload if the operation
 * succeeds
 */
int 
aniEapolParse(tAniPacket *packet,
              v_U8_t **dstMac, 
              v_U8_t **srcMac, 
              v_U8_t **type)
{
    v_U16_t frameType;
    v_U8_t *ptr;
    int retVal;
    int tmp;

    if (aniAsfPacketGetLen(packet) < EAPOL_BODY_POS)
        return ANI_E_ILLEGAL_ARG;

    retVal = aniAsfPacketGetBytes(packet, &ptr);
    if( !ANI_IS_STATUS_SUCCESS( retVal ) )
    {
        return retVal;
    }

    VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO,
            "Supp parsing EAPOL packet of len %d: \n",
                retVal);

    frameType = (ptr[ETHER_PROTO_POS] << 8) + ptr[ETHER_PROTO_POS+1];

    /*
     * Validate the EAPOL-FRAME
     */

    if (frameType != ANI_ETH_P_EAPOL)
        return ANI_E_ILLEGAL_ARG;

    *dstMac = ptr + DST_MAC_POS;
    *srcMac = ptr + SRC_MAC_POS;

    //    if (ptr[EAPOL_VERSION_POS] != EAPOL_VERSION_1)
    //        return ANI_E_ILLEGAL_ARG;

    *type = ptr + ANI_EAPOL_TYPE_POS;
    retVal = (ptr[EAPOL_BODY_LEN_POS] << 8) + ptr[EAPOL_BODY_LEN_POS + 1];
 
    /* 
     * Validate the length of the body. Allow for longer 
     * packets than encoded, but encoding should not be larger than
     * packet.
     * Note: EAPOL body len does not include headers
     */
    tmp = aniAsfPacketGetLen(packet) - EAPOL_RX_HEADER_SIZE;
    if (retVal > tmp) 
    {
        retVal = ANI_E_ILLEGAL_ARG;
    } 
    else {
        if (retVal < tmp) 
        {
            retVal = aniAsfPacketTruncateFromRear(packet, tmp - retVal);
        }
    }

    return retVal;
}

/**
 * aniEapolWriteKey
 *
 * Writes out a complete EAPOL-Key frame. The key descriptor is
 * appended to the packet and the EAPOL header is prepended to it. If
 * a micKey is passed in, then a MIC is calculated and inserted into
 * the frame.
 *
 * @param packet the packet to write to
 * @param dstMac the destination MAC address
 * @param srcMac the source MAC address
 * @param descType the key descriptor type
 * (ANI_EAPOL_KEY_DESC_TYPE_LEGACY_RC4 or
 * ANI_EAPOL_KEY_DESC_TYPE_RSN).
 * @param keyDescData the key descriptor data corresponding to the
 * above descType. The signature field is ignored and will be
 * generated in the packet. The key bytes are expected to be encrypted
 * if they need to be encrypted.
 * @param micKey the MIC key
 * @param micKeyLen the number of bytes in the MIC key
 *
 * @return ANI_OK if the operation succeeds
 *
 */
int
aniEapolWriteKey(v_U32_t cryptHandle,
                 tAniPacket *packet,
                 tAniMacAddr dstMac, 
                 tAniMacAddr srcMac, 
                 int descType,
                 void *keyDescData,
                 v_U8_t *micKey,
                 v_U32_t micKeyLen)
{
    int retVal;

    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    do
    {
        if ((descType == ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW) 
                || (descType == ANI_EAPOL_KEY_DESC_TYPE_RSN)) 
        {

            retVal = writeRsnKeyDesc(packet,
                                     (tAniEapolRsnKeyDesc *) keyDescData,
                                     // Indicate 
                                     // ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW 
                                     // or ANI_EAPOL_KEY_DESC_TYPE_RSN
                                     descType);
            if( !ANI_IS_STATUS_SUCCESS( retVal ) )
            {
                break;
            }

            retVal = addEapolHeaders(packet, dstMac, srcMac, ANI_EAPOL_TYPE_KEY);
            if( !ANI_IS_STATUS_SUCCESS(retVal) ) break;

            retVal = writeRsnKeyMic(cryptHandle,
                                    packet, 
                                    (tAniEapolRsnKeyDesc *) keyDescData,
                                    micKey, micKeyLen);
            if( !ANI_IS_STATUS_SUCCESS(retVal) ) break;

        } 
        else {
            VOS_ASSERT( 0 );
            return ANI_E_ILLEGAL_ARG;
        }
    }while( 0 );

    return retVal;
}


/**
 * aniEapolParseKey
 *
 * Parses and verifies a complete EAPOL-Key frame. The key descriptor
 * type is returned and so is a newly allocated key descriptor structure
 * that is appropriate for the type.
 *
 * NOTE: This is a non-destructive read. That is, the packet headers
 * will be unchanged at the end of this read operation. This is so
 * that a followup MIC check may be done on the complete packet. If
 * the packet parsing fails, the packet headers are not guaranteed to
 * be unchanged.
 *
 * @param packet the packet to read from. Note that the frame is not
 * expected to contain any additional padding at the end other than
 * the exact number of key bytes. (The aniEapolParse function will
 * ensure this.)
 * @param descType is set to the key descriptor type
 * (ANI_EAPOL_KEY_DESC_TYPE_LEGACY_RC4 or
 * ANI_EAPOL_KEY_DESC_TYPE_RSN).
 * @param keyDescData is set to a newly allocated key descriptor
 * corresponding to the above descType. The signature field is
 * verified. The key bytes will be returned encrypted. It is the
 * responsibility of the caller to free this structure and the data
 * contained therein.
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniEapolParseKey(tAniPacket *packet,
                 int *descType,
                 void **keyDescData)
{
    int retVal;
    v_U8_t *bytes;
    v_U32_t eapolFrameLen;

    if (packet == NULL)
        return ANI_E_NULL_VALUE;

    do
    {
        eapolFrameLen = aniAsfPacketGetLen(packet);

        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_INFO, "Supp parsing EAPOL-Key frame of len %d\n", 
                      eapolFrameLen);

        retVal = aniAsfPacketTruncateFromFront(packet, EAPOL_RX_HEADER_SIZE);
        if( !ANI_IS_STATUS_SUCCESS(retVal) ) break;
        
        retVal = aniAsfPacketGetBytes(packet, &bytes);
        if( !ANI_IS_STATUS_SUCCESS(retVal) ) break;

        if (*bytes == ANI_EAPOL_KEY_DESC_TYPE_RSN ||
                   *bytes == ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW) 
        {
            tAniEapolRsnKeyDesc *rsnDesc = NULL;

            //*descType = ANI_EAPOL_KEY_DESC_TYPE_RSN;
            *descType = (*bytes == ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW ?  
                 ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW :  ANI_EAPOL_KEY_DESC_TYPE_RSN) ;
            retVal = parseRsnKeyDesc(packet, &rsnDesc);
            if( !ANI_IS_STATUS_SUCCESS(retVal) ) break;
            *keyDescData = rsnDesc;
        } 
        else 
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, 
                "Supp received unknown EAPOL-Key descriptor: %d\n",
                        *bytes);
            retVal = ANI_E_ILLEGAL_ARG;
            break;
        }

        aniAsfPacketMoveLeft(packet, eapolFrameLen);
    }while( 0 );
    
    return retVal;
}



static int
parseRsnKeyDesc(tAniPacket *packet, 
                tAniEapolRsnKeyDesc **rsnDescPtr)
{
    int retVal = ANI_OK;
    int len;
    v_U8_t *bytes;
    tAniEapolRsnKeyDesc *rsnDesc = NULL;

    do
    {
        aniAsfPacketTruncateFromFront(packet, 1); // Desc-Type

        rsnDesc = (tAniEapolRsnKeyDesc *) 
            vos_mem_malloc( sizeof(tAniEapolRsnKeyDesc) );

        if (rsnDesc == NULL) 
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                    "Supp could not malloc EAPOL-Key Descriptor for RSN\n");
            retVal = ANI_E_MALLOC_FAILED;
            break;
        }

        retVal = parseRsnKeyInfo(packet, &rsnDesc->info);
        if (retVal != ANI_OK) break;

        retVal = aniAsfPacketGet16(packet, &rsnDesc->keyLen);
        if (retVal != ANI_OK)
        {
            break;
        }

        len = sizeof(rsnDesc->replayCounter);
        retVal = aniAsfPacketGetN(packet, len, &bytes);
        if (retVal != ANI_OK)
        {
            break;
        }
        vos_mem_copy(rsnDesc->replayCounter, bytes, len);

        len = sizeof(rsnDesc->keyNonce);
        retVal = aniAsfPacketGetN(packet, len, &bytes);
        if (retVal != ANI_OK)
        {
            break;
        }
        vos_mem_copy(rsnDesc->keyNonce, bytes, len);

        len = sizeof(rsnDesc->keyIv);
        retVal = aniAsfPacketGetN(packet, len, &bytes);
        if (retVal != ANI_OK)
        {
            break;
        }
        vos_mem_copy(rsnDesc->keyIv, bytes, len);

        len = sizeof(rsnDesc->keyRecvSeqCounter);
        retVal = aniAsfPacketGetN(packet, len, &bytes);
        if (retVal != ANI_OK)
        {
            break;
        }
        vos_mem_copy(rsnDesc->keyRecvSeqCounter, bytes, len);    

        len = sizeof(rsnDesc->keyId);
        retVal = aniAsfPacketGetN(packet, len, &bytes);
        if (retVal != ANI_OK)
        {
            break;
        }
        vos_mem_copy(rsnDesc->keyId, bytes, len);

        len = sizeof(rsnDesc->keyMic);
        retVal = aniAsfPacketGetN(packet, len, &bytes);
        if (retVal != ANI_OK)
        {
            break;
        }
        vos_mem_copy(rsnDesc->keyMic, bytes, len);

        retVal = aniAsfPacketGet16(packet, &rsnDesc->keyDataLen);
        if (retVal != ANI_OK)
        {
            break;
        }

        len = rsnDesc->keyDataLen;
        if (len > 0) {
            // We have a key
            retVal = aniAsfPacketGetN(packet, len, &bytes);
            if (retVal != ANI_OK)
            {
             break;
            }
            rsnDesc->keyData = (v_U8_t*)vos_mem_malloc(len);
            if (rsnDesc->keyData == NULL) 
            {
                VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR, "Could not allocate RSN key bytes!\n");
                VOS_ASSERT( 0 );
                retVal = ANI_E_MALLOC_FAILED;
                break;
            }
            vos_mem_copy(rsnDesc->keyData, bytes, len);
        } 
        else {
            rsnDesc->keyData = NULL;
        }

        *rsnDescPtr = rsnDesc;

    }while( 0 );

    if( !ANI_IS_STATUS_SUCCESS( retVal ) )
    {
        vos_mem_free(rsnDesc);
    }

    return retVal;
}

static int
parseRsnKeyInfo(tAniPacket *packet, 
                tAniRsnKeyInfo *info)
{
    v_U16_t tmp;
    int retVal;

    retVal = aniAsfPacketGet16(packet, &tmp);
    if( !ANI_IS_STATUS_SUCCESS( retVal ) )
    {
        return retVal;
    }

    info->keyDescVers = (tmp & ANI_SSM_RSN_KEY_DESC_VERS_MASK) 
        >> ANI_SSM_RSN_KEY_DESC_VERS_OFFSET;
    if (info->keyDescVers != ANI_EAPOL_KEY_DESC_VERS_RC4 &&
        info->keyDescVers != ANI_EAPOL_KEY_DESC_VERS_AES)
        return ANI_E_ILLEGAL_ARG;

    info->unicastFlag = (tmp & ANI_SSM_RSN_UNICAST_MASK) ? 
        eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE;
    info->keyId    = (tmp & ANI_SSM_RSN_KEY_INDEX_MASK)
        >> ANI_SSM_RSN_KEY_INDEX_OFFSET;
    info->installFlag = (tmp & ANI_SSM_RSN_INSTALL_MASK) ?
        eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE;
    info->ackFlag     = (tmp & ANI_SSM_RSN_ACK_MASK) ?
        eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE;
    info->micFlag     = (tmp & ANI_SSM_RSN_MIC_MASK) ?
        eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE;
    info->secureFlag  = (tmp & ANI_SSM_RSN_SECURE_MASK) ?
        eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE;
    info->errorFlag   = (tmp & ANI_SSM_RSN_ERROR_MASK) ?
        eANI_BOOLEAN_TRUE: eANI_BOOLEAN_FALSE;
    info->requestFlag = (tmp & ANI_SSM_RSN_REQUEST_MASK) ?
        eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE;
    info->encKeyDataFlag = (tmp & ANI_SSM_RSN_ENC_KEY_DATA_MASK) ?
        eANI_BOOLEAN_TRUE : eANI_BOOLEAN_FALSE;

    return ANI_OK;
}


static int
writeRsnKeyDesc(tAniPacket *packet, 
                tAniEapolRsnKeyDesc *rsnDesc,
                v_U8_t keyDescType)
{
    int retVal;

    do
    {
        // This can be either ANI_EAPOL_KEY_DESC_TYPE_RSN
        // or ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW
        retVal = aniAsfPacketAppend8(packet, keyDescType);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = writeRsnKeyInfo(packet, &rsnDesc->info);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = aniAsfPacketAppend16(packet, rsnDesc->keyLen);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = aniAsfPacketAppendBuffer(packet, 
                                          rsnDesc->replayCounter,
                                          sizeof(rsnDesc->replayCounter));
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = aniAsfPacketAppendBuffer(packet, 
                                          rsnDesc->keyNonce, 
                                          sizeof(rsnDesc->keyNonce));
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = aniAsfPacketAppendBuffer(packet, 
                                          rsnDesc->keyIv, 
                                          sizeof(rsnDesc->keyIv));
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = aniAsfPacketAppendBuffer(packet, 
                                          rsnDesc->keyRecvSeqCounter, 
                                          sizeof(rsnDesc->keyRecvSeqCounter));
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = aniAsfPacketAppendBuffer(packet, 
                                          rsnDesc->keyId, 
                                          sizeof(rsnDesc->keyId));
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        // Zero out the key MIC
        retVal = aniAsfPacketAppendBuffer(packet, 
                                          ZERO_BYTES, 
                                          sizeof(rsnDesc->keyMic));
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        retVal = aniAsfPacketAppend16(packet, rsnDesc->keyDataLen);
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;

        if (rsnDesc->keyDataLen != 0) 
        {
            retVal = aniAsfPacketAppendBuffer(packet, 
                                              rsnDesc->keyData, 
                                              rsnDesc->keyDataLen);
            if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;
        }
    }while( 0 );

    return retVal;
}

static int
writeRsnKeyInfo(tAniPacket *packet, tAniRsnKeyInfo *info)
{
    int retVal;
    v_U16_t tmp;
    v_U16_t infoValue;

    infoValue = 0;

    tmp = (v_U16_t)info->keyDescVers;
    tmp = tmp << ANI_SSM_RSN_KEY_DESC_VERS_OFFSET;
    infoValue |= (tmp & ANI_SSM_RSN_KEY_DESC_VERS_MASK);

    if (info->unicastFlag)
        infoValue |= ANI_SSM_RSN_UNICAST_MASK;

    tmp = info->keyId;
    tmp = tmp << ANI_SSM_RSN_KEY_INDEX_OFFSET;
    infoValue |= (tmp & ANI_SSM_RSN_KEY_INDEX_MASK);

    if (info->installFlag)
        infoValue |= ANI_SSM_RSN_INSTALL_MASK;

    if (info->ackFlag)
        infoValue |= ANI_SSM_RSN_ACK_MASK;

    if (info->micFlag)
        infoValue |= ANI_SSM_RSN_MIC_MASK;

    if (info->secureFlag)
        infoValue |= ANI_SSM_RSN_SECURE_MASK;

    if (info->errorFlag)
        infoValue |= ANI_SSM_RSN_ERROR_MASK;

    if (info->requestFlag)
        infoValue |= ANI_SSM_RSN_REQUEST_MASK;
    
    if (info->encKeyDataFlag)
        infoValue |= ANI_SSM_RSN_ENC_KEY_DATA_MASK;
    
    retVal = aniAsfPacketAppend16(packet, infoValue);

    return retVal;
}


static int
writeRsnKeyMic(v_U32_t cryptHandle,
               tAniPacket *eapolFrame, 
               tAniEapolRsnKeyDesc *rsnDesc,
               v_U8_t *micKey, 
               v_U32_t micKeyLen)
{
    int retVal = ANI_OK;
    int len;

    v_U8_t *ptr = NULL;
    v_U8_t *micPos = NULL;
    v_U8_t result[VOS_DIGEST_SHA1_SIZE]; // Larger of the two

    // Sanity check the arguments and return if no MIC generation is
    // needed
    if (micKey != NULL) 
    {
        if (micKeyLen == 0 || !rsnDesc->info.micFlag) 
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "Supp MIC key provided but micKeyLen or micFlag is not set!\n");
            VOS_ASSERT( 0 );
            return ANI_E_ILLEGAL_ARG;
        }
    } 
    else {
        if (rsnDesc->info.micFlag) 
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "Supp micFlag is set but MIC key not provided!\n");
            VOS_ASSERT( 0 );
            return ANI_E_ILLEGAL_ARG;
        }
        // Normal condition where MIC is not desired by the caller
        return ANI_OK;
    }

    len = aniAsfPacketGetBytes(eapolFrame, &ptr);
    if( !ANI_IS_STATUS_SUCCESS( len ) )
    {
        return len;
    }

    micPos = ptr + ANI_SSM_RSN_KEY_MIC_OFFSET + SNAP_HEADER_SIZE;

    // Clear the MIC field in the packet before the MIC computation
    vos_mem_zero( micPos, VOS_DIGEST_MD5_SIZE);

    // Skip to the EAPOL version field for MIC computation
    ptr += EAPOL_VERSION_POS + SNAP_HEADER_SIZE;
    len -= (EAPOL_VERSION_POS + SNAP_HEADER_SIZE);

    if (rsnDesc->info.keyDescVers == ANI_EAPOL_KEY_DESC_VERS_AES) 
    {
        if( VOS_IS_STATUS_SUCCESS( vos_sha1_hmac_str(cryptHandle, ptr, len, micKey, micKeyLen, result) ) )
        {
            retVal = ANI_OK;
        }
        else
        {
            retVal = ANI_ERROR;
        }
    } 
    else {
        VOS_ASSERT( 0 );
        retVal = ANI_E_ILLEGAL_ARG;
    }

    if (retVal == ANI_OK) 
    {
        // Copy only 16B which is the smaller of the two and the same as
        // ANI_EAPOL_KEY_RSN_MIC_SIZE
        vos_mem_copy(micPos, result, VOS_DIGEST_MD5_SIZE);
    }

    return retVal;
}

/**
 * aniEapolKeyCheckMic
 *
 * @param eapolFrame the complete EAPOL-Key packet
 * @param descType the key descriptor type
 * @param keyDescData the key descriptor
 * @param micKey the MIC key
 * @param micKeyLen the number of bytes in the MIC key
 *
 * @return ANI_OK if the operation succeeds; ANI_E_MIC_FAILED if the
 * MIC check fails.
 */
int
aniEapolKeyCheckMic(v_U32_t cryptHandle,
                    tAniPacket *eapolFrame,
                    int descType,
                    void *keyDescData,
                    v_U8_t *micKey,
                    v_U32_t micKeyLen)
{
    if (descType == ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW) 
    {
        return checkRsnKeyMic(cryptHandle, eapolFrame, keyDescData, micKey, micKeyLen);
    } 
    else {
        VOS_ASSERT( 0 );
        return ANI_E_ILLEGAL_ARG;
    }
}


static int
checkRsnKeyMic(v_U32_t cryptHandle,
               tAniPacket *eapolFrame,
               tAniEapolRsnKeyDesc *rsnDesc,
               v_U8_t *micKey,
               v_U32_t micKeyLen) 
{
    int retVal = ANI_ERROR;
    int len;

    v_U8_t *ptr = NULL;
    v_U8_t *micPos = NULL;

    v_U8_t result[VOS_DIGEST_SHA1_SIZE]; // Larger of the two
    v_U8_t incomingMic[ANI_EAPOL_KEY_RSN_MIC_SIZE];

    if (!rsnDesc->info.micFlag) 
    {
        VOS_ASSERT( 0 );
        return ANI_E_ILLEGAL_ARG;
    }

    len = aniAsfPacketGetBytes(eapolFrame, &ptr);
    if( ANI_IS_STATUS_SUCCESS( len ) )
    {
        micPos = ptr + ANI_SSM_RSN_KEY_MIC_OFFSET;

        // Skip to the EAPOL version field for MIC computation
        ptr += EAPOL_VERSION_POS;
        len -= EAPOL_VERSION_POS;

        // Copy existing MIC to temporary location and zero it out
        vos_mem_copy( incomingMic, micPos, ANI_EAPOL_KEY_RSN_MIC_SIZE );
        vos_mem_zero( micPos, ANI_EAPOL_KEY_RSN_MIC_SIZE );

        if (rsnDesc->info.keyDescVers == ANI_EAPOL_KEY_DESC_VERS_AES) 
        {
            if( VOS_IS_STATUS_SUCCESS( vos_sha1_hmac_str(cryptHandle, ptr, len, micKey, micKeyLen, result) ) )
            {
                retVal = ANI_OK;
            }
        } 
        else {
            VOS_ASSERT( 0 );
            retVal = ANI_E_ILLEGAL_ARG;
        }

        if (retVal == ANI_OK) 
        {
            if ( !vos_mem_compare(incomingMic, result, ANI_EAPOL_KEY_RSN_MIC_SIZE))
            {
                retVal = ANI_E_MIC_FAILED;
            }
        }
    }

    return retVal;
}

/**
 * aniEapolKeyFreeDesc
 *
 * Frees the EAPOL key descriptor and the key bytes contained within it.
 *
 * @param descType the key descriptor type
 * @param keyDescData the key descriptor
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniEapolKeyFreeDesc(int descType, void *keyDescData)
{
    tAniEapolRsnKeyDesc *rsnDesc;

    if( keyDescData )
    {
        if ((descType == ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW) 
                || (descType == ANI_EAPOL_KEY_DESC_TYPE_RSN)) 
        {

            rsnDesc = (tAniEapolRsnKeyDesc *) keyDescData;
            if (rsnDesc->keyData != NULL)
                vos_mem_free(rsnDesc->keyData);

        } 
        else {

            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                "Supp asked to free illegal type: %d\n", descType);

        }

        vos_mem_free(keyDescData);
    }

    return ANI_OK;
}

v_U8_t *
aniEapolType2Str(v_U8_t type)
{
    switch (type) 
    {
    case ANI_EAPOL_TYPE_PACKET:
        return (v_U8_t *)ANI_EAPOL_TYPE_PACKET_STR;
        break;
    case ANI_EAPOL_TYPE_START:
        return (v_U8_t *)ANI_EAPOL_TYPE_START_STR;
        break;
    case ANI_EAPOL_TYPE_LOGOFF:
        return (v_U8_t *)ANI_EAPOL_TYPE_LOGOFF_STR;
        break;
    case ANI_EAPOL_TYPE_KEY:
        return (v_U8_t *)ANI_EAPOL_TYPE_KEY_STR;
        break;
    case ANI_EAPOL_TYPE_ASF_ALERT:
        return (v_U8_t *)ANI_EAPOL_TYPE_ASF_ALERT_STR;
        break;
    default:
        return (v_U8_t *)ANI_EAPOL_TYPE_UNKNOWN_STR;
        break;
    }
}


void bapRsnEapolHandler( v_PVOID_t pvFsm, tAniPacket *packet, v_BOOL_t fIsAuth )
{
    int retVal;
    v_U8_t *dstMac = NULL;
    v_U8_t *srcMac = NULL;
    v_U8_t *type = NULL;

    retVal = aniEapolParse(packet, &dstMac, &srcMac, &type);
    if ( retVal >= 0 )
    {
        retVal = ANI_OK;

        // Sanity check that a PAE role has been assigned to it,
        // and then dispatch to the appropriate handler

        if( fIsAuth )
        {
            tAuthRsnFsm *fsm = (tAuthRsnFsm *)pvFsm;
            authEapolHandler( fsm, packet, dstMac, srcMac, type );
        }
        else
        {
            tSuppRsnFsm *fsm = (tSuppRsnFsm *)pvFsm;
            suppEapolHandler(fsm, packet, dstMac, srcMac, type);
        } // switch statement
    } // Successfully parsed EAPOL
    else
    {
        VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
            "eapolHandler Received bad EAPOL message of len %d (status=%d)\n",
                      aniAsfPacketGetLen(packet), retVal );
    } 
    aniAsfPacketFree( packet );
}


int bapRsnFormPktFromVosPkt( tAniPacket **ppPacket, vos_pkt_t *pVosPacket )
{
    int retVal = ANI_ERROR;
    VOS_STATUS status;
    v_U16_t uPktLen;
#define BAP_RSN_SNAP_TYPE_OFFSET 20
#define BAP_RSN_ETHERNET_3_HEADER_LEN   22
    v_U8_t *pFrame;
    tAniPacket *pAniPacket = NULL;

    do
    {
        status = vos_pkt_get_packet_length( pVosPacket, &uPktLen );
        if( !VOS_IS_STATUS_SUCCESS(status) ) break;
        if( (uPktLen < BAP_RSN_ETHERNET_3_HEADER_LEN) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                " authRsnRxFrameHandler receive eapol packet size (%d) too small (%d)\n", 
                uPktLen, BAP_RSN_ETHERNET_3_HEADER_LEN );
            break;
        }
        status = vos_pkt_peek_data( pVosPacket, 0, (v_VOID_t *)&pFrame, uPktLen );
        if( !VOS_IS_STATUS_SUCCESS(status) ) break;
        retVal = aniAsfPacketAllocateExplicit(&pAniPacket, uPktLen, 0 );
        if( !ANI_IS_STATUS_SUCCESS( retVal ) )
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                " authRsnRxFrameHandler failed to get buffer size (%d) \n", 
                uPktLen );
            break;
        }
        aniAsfPacketEmptyExplicit( pAniPacket, 0 );
        pFrame[ETHER_PROTO_POS] = pFrame[BAP_RSN_SNAP_TYPE_OFFSET];
        pFrame[ETHER_PROTO_POS + 1] = pFrame[BAP_RSN_SNAP_TYPE_OFFSET + 1];
        //push ethernet II header in
        retVal = aniAsfPacketAppendBuffer( pAniPacket, pFrame, ETHER_PROTO_POS + 2 );
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) break;
        //Get the rest of the data in
        uPktLen -= BAP_RSN_ETHERNET_3_HEADER_LEN;
        VOS_ASSERT( uPktLen > 0 );
        retVal = aniAsfPacketAppendBuffer( pAniPacket, pFrame + BAP_RSN_ETHERNET_3_HEADER_LEN, 
                            uPktLen );
        if( !ANI_IS_STATUS_SUCCESS( retVal ) ) 
        {
            VOS_TRACE( VOS_MODULE_ID_BAP, VOS_TRACE_LEVEL_ERROR,
                " authRsnRxFrameHandler cannot retrieve eapol payload size (%d)\n", 
                uPktLen );
            break;
        }
    }while( 0 );

    if( ANI_IS_STATUS_SUCCESS( retVal ) )
    {
        *ppPacket = pAniPacket;
    }
    else if( pAniPacket )
    {
        aniAsfPacketFree( pAniPacket );
    }

    return retVal;
}


