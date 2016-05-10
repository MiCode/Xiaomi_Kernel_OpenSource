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

/*
 * Woodside Networks, Inc proprietary. All rights reserved.
 * File:        $Header: //depot/software/projects/feature_branches/gen5_phase1/os/linux/classic/ap/apps/include/aniSsmEapol.h#1 $ 
 * Contains declarations of various utilities for EAPoL frame
 * parsing and creation.
 * range.
 * Author:      Mayank D. Upadhyay
 * Date:        19-June-2002
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */
#ifndef _ANI_SSM_EAPOL_H_
#define _ANI_SSM_EAPOL_H_

#include "vos_types.h"
#include "vos_trace.h"
#include "vos_packet.h"
#include <bapRsnAsfPacket.h>

#define ANI_ETH_P_EAPOL 0x0003
#define ANI_ETH_P_IP 0x0800

/**
 * The EAPOL type field is one of the following:
 */
#define ANI_EAPOL_TYPE_PACKET 0
#define ANI_EAPOL_TYPE_START 1
#define ANI_EAPOL_TYPE_LOGOFF 2
#define ANI_EAPOL_TYPE_KEY 3
#define ANI_EAPOL_TYPE_ASF_ALERT 4

#define EAPOL_VERSION_1 0x01

#define EAPOL_RX_HEADER_SIZE    18
#define EAPOL_TX_HEADER_SIZE    26  //include LLC_SNAP
#define SNAP_HEADER_SIZE   8

#define ANI_EAPOL_KEY_DESC_TYPE_LEGACY_RC4   1
// JEZ20041012 This needs to be fixed.  This needs to support BOTH 
// the older WPA Key Descriptor type of 254 AS WELL AS the newer
// Key Descriptor type of 2
#define ANI_EAPOL_KEY_DESC_TYPE_RSN        254
//#define ANI_EAPOL_KEY_DESC_TYPE_RSN          2
#define ANI_EAPOL_KEY_DESC_TYPE_RSN_NEW      2

#define ANI_EAPOL_KEY_RSN_REPLAY_CTR_SIZE  8
#define ANI_EAPOL_KEY_RSN_NONCE_SIZE      32
#define ANI_EAPOL_KEY_RSN_IV_SIZE         16
#define ANI_EAPOL_KEY_RSN_RSC_SIZE         8
#define ANI_EAPOL_KEY_RSN_ID_SIZE          8
#define ANI_EAPOL_KEY_RSN_MIC_SIZE        16
#define ANI_EAPOL_KEY_RSN_ENC_KEY_SIZE 16

#define ANI_EAPOL_KEY_DESC_VERS_RC4    1
#define ANI_EAPOL_KEY_DESC_VERS_AES    2

#define ANI_EAPOL_KEY_RC4_REPLAY_CTR_SIZE 8
#define ANI_EAPOL_KEY_RC4_IV_SIZE        16
#define ANI_EAPOL_KET_RC4_SIGN_SIZE      16

#define ANI_SSM_IE_RSN_KEY_DATA_ENCAPS_ID       0xDD
#define ANI_SSM_IE_RSN_GROUP_KEY_DATA_ENCAPS_ID 1
#define ANI_SSM_GROUP_KEY_KDE_TX_BIT            0x04

typedef struct sAniEapolLegacyRc4KeyDesc {
    v_U16_t keyLen;
    v_U8_t  replayCounter[ANI_EAPOL_KEY_RC4_REPLAY_CTR_SIZE];
    v_U8_t  keyIv[ANI_EAPOL_KEY_RC4_IV_SIZE];
    tANI_BOOLEAN unicastFlag; // The high order 1 bit of key-index
    v_U8_t  keyId; // The lower order 7 bits of key-index (but 0..3 based)
    v_U8_t  signature[ANI_EAPOL_KET_RC4_SIGN_SIZE];
    v_U8_t  *key;
} tAniEapolLegacyRc4KeyDesc;

typedef struct sAniRsnKeyInfo {
    v_U32_t keyDescVers;
    tANI_BOOLEAN unicastFlag; // Pair-wise key
    v_U16_t keyId;
    tANI_BOOLEAN installFlag;
    tANI_BOOLEAN ackFlag;
    tANI_BOOLEAN micFlag;
    tANI_BOOLEAN secureFlag;
    tANI_BOOLEAN errorFlag;
    tANI_BOOLEAN requestFlag;
    tANI_BOOLEAN encKeyDataFlag; // RSN only (Is 0 in WPA)
} tAniRsnKeyInfo;

typedef struct sAniEapolRsnKeyDesc {
    tAniRsnKeyInfo info;
    v_U16_t keyLen;
    v_U8_t  replayCounter[ANI_EAPOL_KEY_RSN_REPLAY_CTR_SIZE];
    v_U8_t  keyNonce[ANI_EAPOL_KEY_RSN_NONCE_SIZE];
    v_U8_t  keyIv[ANI_EAPOL_KEY_RSN_IV_SIZE];
    v_U8_t  keyRecvSeqCounter[ANI_EAPOL_KEY_RSN_RSC_SIZE];
    v_U8_t  keyId[ANI_EAPOL_KEY_RSN_ID_SIZE];
    v_U8_t  keyMic[ANI_EAPOL_KEY_RSN_MIC_SIZE];
    v_U16_t keyDataLen;
    v_U8_t  *keyData;
} tAniEapolRsnKeyDesc;

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
                   tAniMacAddr srcMac);

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
                       tAniMacAddr srcMac);

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
              v_U8_t **type);

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
 * generated in the packet. The key bytes are expected to be enctypted
 * is they need to be encrypted.
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
                 v_U32_t micKeyLen);

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
                 void **keyDescData);

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
                    v_U32_t micKeyLen);

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
aniEapolKeyFreeDesc(int descType, void *keyDescData);

v_U8_t *
aniEapolType2Str(v_U8_t type);

v_U8_t *
aniEapolHdr2Str(v_U8_t *hdr);

/**
 * aniEapolKeyLogDesc
 *
 * Logs information about the given EAPOL key desctiptor.
 *
 * @param descType the key descriptor type
 * @param keyDescData the key descriptor
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniEapolKeyLogDesc(int descType, void *keyDescData);

void bapRsnEapolHandler( v_PVOID_t pvFsm, tAniPacket *packet, v_BOOL_t fIsAuth );
//Transfer from pVosPacket to tAniPacket.
int bapRsnFormPktFromVosPkt( tAniPacket **ppPacket, vos_pkt_t *pVosPacket );

#endif //_ANI_SSM_EAPOL_H_
