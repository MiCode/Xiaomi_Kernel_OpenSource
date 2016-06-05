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
 * $File: //depot/software/projects/feature_branches/gen5_phase1/os/linux/classic/ap/apps/include/aniSsmServices.h $
 *
 * Contains definitions of common types that the SSM exports to other
 * modules. 
 *
 * Author:      Mayank D. Upadhyay
 * Date:        23-January-2003
 * History:-
 * Date         Modified by     Modification Information
 * ------------------------------------------------------
 *
 */

#ifndef _ANI_SSM_SERVICES_H_
#define _ANI_SSM_SERVICES_H_

#include "vos_types.h"
#include "sirApi.h"

#define ANI_SSM_MAX_KEYS_INFO_SIZE 512
#define ANI_SSM_MAX_GROUP_SIZE     32
#define ANI_SSM_MAX_USERID_SIZE    64

#define ANI_SSM_RSN_PMK_LEN 32
#define ANI_SSM_RSN_PSK_LEN ANI_SSM_RSN_PMK_LEN
#define ANI_SSM_RSN_PSK_LEN_HEX (ANI_SSM_RSN_PSK_LEN + ANI_SSM_RSN_PSK_LEN)
#define ANI_SSM_MAX_PASSPHRASE_LEN 128
#define ANI_SSM_MAX_AUTHZONE_LEN  32
#define ANI_SSM_MAX_LANDPG_URL_LEN 128
#define ANI_SSM_MAX_GUEST_PORTAL_PWD_LEN 32

#define ANI_SSM_IE_RSN_OUI {0x00, 0x0F, 0xAC}
#define ANI_SSM_IE_WPA_OUI {0x00, 0x50, 0xF2}

#define ANI_SSM_IE_RSN_ELEM_ID  48
#define ANI_SSM_IE_WPA_ELEM_ID  221

/*
 * The total length of an RSN IE may be no longer than these many
 * octets, including the two bytes for type and len.
 */
#define ANI_RSN_IE_MAX_LEN 257

/*
 * PMKSA ID data type
 * (PMKID is an HMAC-SHA1-128 value) 
 */
#define ANI_AAG_PMKID_SIZE 16

#define ANI_SSM_AUTH_BITMASK   0x00010000
#define ANI_SSM_IEMODE_BITMASK 0xC0000000
#define ANI_SSM_ENCR_BITMASK   0x00000001
#define ANI_SSM_IEMODE_SHIFT   (30)


// Upper level authentication types used by AA
typedef enum eAniSsmAuthType {
    eANI_SSM_AT_UNDERFLOW = -1,

    // The numbers are fixed so that they can be re-used in the XCLI
    // config file and 1x.conf.
    eANI_SSM_AT_NONE       = 0,
    eANI_SSM_AT_SHARED_KEY = 1,
    eANI_SSM_AT_LEGACY_EAP = 2,
    eANI_SSM_AT_RSN_PSK    = 3,
    eANI_SSM_AT_RSN_EAP    = 4,

    eANI_SSM_AT_OVERFLOW
} tAniSsmAuthType;

// Upper level encryption types used by AA
typedef enum eAniSsmCipherType {
    eANI_SSM_CT_UNDERFLOW = -1,

    // The numbers are fixed so that they can be re-used in the XCLI
    // config file and 1x.conf.
    eANI_SSM_CT_NONE       = 0,
    eANI_SSM_CT_WEP40      = 1,
    eANI_SSM_CT_WEP104     = 2,
    eANI_SSM_CT_WPA_WEP40  = 3,
    eANI_SSM_CT_WPA_WEP104 = 4,
    eANI_SSM_CT_TKIP       = 5,
    eANI_SSM_CT_CCMP       = 6,

    eANI_SSM_CT_OVERFLOW
} tAniSsmCipherType;


// WPA modes
typedef enum eAniSsmWpaModes {
    eANI_SSM_WPA_UNDERFLOW = -1,

    eANI_SSM_WPA_DISABLE = 0,
    eANI_SSM_WPA_1 = 1,
    eANI_SSM_WPA_2 = 2,

    eANI_SSM_WPA_OVERFLOW = ((eANI_SSM_WPA_2 | eANI_SSM_WPA_1)  + 1)
} tAniSsmWpaModes;

typedef struct sAniSsmGroup {
    v_U16_t len; // Valid range: 0..ANI_SSM_MAX_GROUP_SIZE
    v_U8_t  group[1];
} tAniSsmGroup;

typedef struct sAniSsmUserId {
    v_U16_t len; // Valid range: 0..ANI_SSM_MAX_USERID_SIZE
    v_U8_t  userId[1];
} tAniSsmUserId;

/*
 * PMKSA ID data type
 * (PMKID is an HMAC-SHA1-128 value) 
 */
typedef v_U8_t tAniSsmPmkId[ANI_AAG_PMKID_SIZE];

/**
 * aniSsmInitStaticConf
 *
 * (Re-)Initializes the SSM internal static configuration. This may be
 * from a static configuration file and will include items such as
 * local MAC-ACL lists.
 *
 * @param configFileName - an optional filename to read from. If this is
 * NULL, the default AAG static conf file is read.
 *
 * @return ANI_OK if the operation succeeds
 */
int
aniSsmInitStaticConf(char *configFileName);

/**
 * aniSsmIsStaMacAllowed
 *
 * Determines if a given STA passes the local MAC-ACL check. If
 * MAC-ACL lookup is enabled, it may be either positive (whitelist) or
 * negative (blacklist). If positive MAC-ACLs are on, then only those
 * STAs that are in the whitelist are allowed in. If negative MAC-ACLs
 * are on, then those STAs that are in the blacklist are not allowed in.
 *
 * Note that local MAC-ACLs may be maintained per SSID.
 *
 * @param staMac - the MAC address of the STA
 * @param ssid - the SSID that the STA is associating on
 *
 * @return ANI_OK if the operation succeeds
 */
v_BOOL_t
aniSsmIsStaMacAllowed(const tAniMacAddr staMac, const tAniSSID *ssid);

/**
 * aniSsmIsSecModeAllowed
 *
 * Determines if the security suites requested by an RSN station or
 * non-RSN station are allowed under the security mode in force at the
 * moment.
 *
 * An RSN IE needs to be passed in if RSN is being used. Otherwise the
 * ieLen field should be set to 0 or ieData set to NULL to indicate
 * that no IE is present. If the RSN IE is present it is used to check
 * both the authentication type and the cipher type for the group and
 * pairwise keys. Special rules might apply in the case of a
 * BP. Therefore, a separate flag indicates if the STA is a BP.
 *
 * If the station is not using RSN, the authentication type is
 * tightly bound to the cipher type. For instance, when using
 * shared-key MAC authentication, the cipher type will be assumed to
 * be WEP. (Both WEP-40 and WEP-104 fall under the same security
 * level.) When using open-system MAC authentication, the cipher type
 * will be assumed to be WEP if the security level requires WEP,
 * otherwise the cipher will be determined later. (When performing
 * open-auth in the lowest security level, the STA is required to
 * initiate EAPOL in order to establish WEP keys, or WEP cannot be not
 * used.)
 * 
 * @param secMode the security mode that is in force
 * @param macAuthType the MAC-level authentication type to check
 * @param ieLen is set 0 if no RSN IE is present, or to the number of
 * octets in the RSN IE.
 * @param ieData the optional IE data bytes, or NULL if no IE is
 * present.
 * @param bpIndicator eANI_BOOLEAN_TRUE if the STA is a BP,
 * eANI_BOOLEAN_FALSE otherwise.
 *
 * @return eANI_BOOLEAN_TRUE if the authentication type is allowed,
 * eANI_BOOLEAN_FALSE if not.
 *
 * @see aniSsmIsRsnSuiteAllowed
 */
v_BOOL_t
aniSsmIsSecModeAllowed(v_U32_t secMode,
                       tAniAuthType macAuthType, 
                       v_U8_t ieLen,
                       v_U8_t *ieData,
                       v_BOOL_t bpIndicator,
                       v_BOOL_t wpsEnabled);

/**
 * aniSsmGenRsnSuiteList
 *
 * Generates a RSN information element containing a list of RSN suites
 * that conform to the specified security level. This is generally
 * used on the AP to generate the RSN information element it
 * advertizes.
 *
 * @param secMode the security mode in force
 * @param ieData the buffer in which to store the generated IE
 *
 * @return the non-negative number of bytes written into the buffer if
 * the operation succeeds, or a negative error code.
 */
int
aniSsmGenRsnSuiteList(v_U32_t secMode,
                      v_U8_t ieData[ANI_RSN_IE_MAX_LEN]);

/**
 * aniSsmGenRsnSuiteForBp
 *
 * Generates a RSN information element containing exactly one RSN
 * suite selector for authentication and exactly one for the
 * cipher. This is generally used on the BP side while associating
 * with an upstream AP.
 *
 * If RSN is turned off on the BP, then the IE is of length 0.
 *
 * NOTE: As per 802.11/D3.0, the BP has to send back the exact group
 * key cipher that the AP indicated in its IE.
 *
 * @param apIeData contains the IE sent by the AP and is used to read
 * the group key cipher that the AP wants us to use.
 * @param apIeLen the length of the AP's IE
 * @param bpRsnFlag should be 0 for no RSN, 1 for AES, 2 for TKIP
 * @param bpPskFlag should be eANI_BOOLEAN_TRUE if RSN with PSK is
 * desired. This is only relevant if bpRsnFlag is not zero.
 * @param ieData the buffer in which to store the generated IE
 *
 * @return the non-negative number of bytes written into the buffer if
 * the operation succeeds, or a negative error code.
 */
int
aniSsmGenRsnSuiteForBp(const v_U8_t *apIeData,
                       v_U8_t apIeLen,
                       v_U32_t bpRsnFlag,
                       v_BOOL_t bpPskFlag,
                       v_U8_t ieData[ANI_RSN_IE_MAX_LEN]);

/**
 * aniSsmSecMode2Str
 *
 * Returns a descriptive string that can be used for logging the
 * security mode.
 *
 * @param secMode the secMode to be printed
 *
 * @return a printable ASCII string representing the secMode
 */
v_U8_t *
aniSsmSecMode2Str(v_U32_t secMode);

/**
 * aniSsmIe2Str
 *
 * Parses and returns a printable form of the IE (WPA/RSN).
 *
 * @param ieData the IE bytes
 * @param ieLen the length of the IE
 *
 * @return ANI_OK if the operation succeeds
 */
v_U8_t *
aniSsmIe2Str(const v_U8_t *ieData, v_U8_t ieLen);

#endif /* _ANI_SSM_SERVICES_H_ */
