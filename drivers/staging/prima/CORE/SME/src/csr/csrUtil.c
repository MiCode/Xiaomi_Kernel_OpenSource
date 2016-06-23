/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
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

/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *


    \file csrUtil.c

    Implementation supporting routines for CSR.
   ========================================================================== */


#include "aniGlobal.h"

#include "palApi.h"
#include "csrSupport.h"
#include "csrInsideApi.h"
#include "smsDebug.h"
#include "smeQosInternal.h"
#include "wlan_qct_wda.h"

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
#include "vos_utils.h"
#include "csrEse.h"
#endif /* FEATURE_WLAN_ESE && !FEATURE_WLAN_ESE_UPLOAD*/
tANI_U8 csrWpaOui[][ CSR_WPA_OUI_SIZE ] = {
    { 0x00, 0x50, 0xf2, 0x00 },
    { 0x00, 0x50, 0xf2, 0x01 },
    { 0x00, 0x50, 0xf2, 0x02 },
    { 0x00, 0x50, 0xf2, 0x03 },
    { 0x00, 0x50, 0xf2, 0x04 },
    { 0x00, 0x50, 0xf2, 0x05 },
#ifdef FEATURE_WLAN_ESE
    { 0x00, 0x40, 0x96, 0x00 }, // CCKM
#endif /* FEATURE_WLAN_ESE */
};

tANI_U8 csrRSNOui[][ CSR_RSN_OUI_SIZE ] = {
    { 0x00, 0x0F, 0xAC, 0x00 }, // group cipher
    { 0x00, 0x0F, 0xAC, 0x01 }, // WEP-40 or RSN
    { 0x00, 0x0F, 0xAC, 0x02 }, // TKIP or RSN-PSK
    { 0x00, 0x0F, 0xAC, 0x03 }, // Reserved
    { 0x00, 0x0F, 0xAC, 0x04 }, // AES-CCMP
    { 0x00, 0x0F, 0xAC, 0x05 }, // WEP-104
    { 0x00, 0x40, 0x96, 0x00 }, // CCKM
    { 0x00, 0x0F, 0xAC, 0x06 },  // BIP (encryption type) or RSN-PSK-SHA256 (authentication type)
    /* RSN-8021X-SHA256 (authentication type) */
    { 0x00, 0x0F, 0xAC, 0x05 }
};

#ifdef FEATURE_WLAN_WAPI
tANI_U8 csrWapiOui[CSR_WAPI_OUI_ROW_SIZE][ CSR_WAPI_OUI_SIZE ] = {
    { 0x00, 0x14, 0x72, 0x00 }, // Reserved
    { 0x00, 0x14, 0x72, 0x01 }, // WAI certificate or SMS4
    { 0x00, 0x14, 0x72, 0x02 } // WAI PSK
};
#endif /* FEATURE_WLAN_WAPI */
tANI_U8 csrWmeInfoOui[ CSR_WME_OUI_SIZE ] = { 0x00, 0x50, 0xf2, 0x02 };
tANI_U8 csrWmeParmOui[ CSR_WME_OUI_SIZE ] = { 0x00, 0x50, 0xf2, 0x02 };

static tCsrIELenInfo gCsrIELengthTable[] = {
/* 000 */ { SIR_MAC_SSID_EID_MIN, SIR_MAC_SSID_EID_MAX },
/* 001 */ { SIR_MAC_RATESET_EID_MIN, SIR_MAC_RATESET_EID_MAX },
/* 002 */ { SIR_MAC_FH_PARAM_SET_EID_MIN, SIR_MAC_FH_PARAM_SET_EID_MAX },
/* 003 */ { SIR_MAC_DS_PARAM_SET_EID_MIN, SIR_MAC_DS_PARAM_SET_EID_MAX },
/* 004 */ { SIR_MAC_CF_PARAM_SET_EID_MIN, SIR_MAC_CF_PARAM_SET_EID_MAX },
/* 005 */ { SIR_MAC_TIM_EID_MIN, SIR_MAC_TIM_EID_MAX },
/* 006 */ { SIR_MAC_IBSS_PARAM_SET_EID_MIN, SIR_MAC_IBSS_PARAM_SET_EID_MAX },
/* 007 */ { SIR_MAC_COUNTRY_EID_MIN, SIR_MAC_COUNTRY_EID_MAX },
/* 008 */ { SIR_MAC_FH_PARAMS_EID_MIN, SIR_MAC_FH_PARAMS_EID_MAX },
/* 009 */ { SIR_MAC_FH_PATTERN_EID_MIN, SIR_MAC_FH_PATTERN_EID_MAX },
/* 010 */ { SIR_MAC_REQUEST_EID_MIN, SIR_MAC_REQUEST_EID_MAX },
/* 011 */ { SIR_MAC_QBSS_LOAD_EID_MIN, SIR_MAC_QBSS_LOAD_EID_MAX },
/* 012 */ { SIR_MAC_EDCA_PARAM_SET_EID_MIN, SIR_MAC_EDCA_PARAM_SET_EID_MAX },
/* 013 */ { SIR_MAC_TSPEC_EID_MIN, SIR_MAC_TSPEC_EID_MAX },
/* 014 */ { SIR_MAC_TCLAS_EID_MIN, SIR_MAC_TCLAS_EID_MAX },
/* 015 */ { SIR_MAC_QOS_SCHEDULE_EID_MIN, SIR_MAC_QOS_SCHEDULE_EID_MAX },
/* 016 */ { SIR_MAC_CHALLENGE_TEXT_EID_MIN, SIR_MAC_CHALLENGE_TEXT_EID_MAX },
/* 017 */ { 0, 255 },
/* 018 */ { 0, 255 },
/* 019 */ { 0, 255 },
/* 020 */ { 0, 255 },
/* 021 */ { 0, 255 },
/* 022 */ { 0, 255 },
/* 023 */ { 0, 255 },
/* 024 */ { 0, 255 },
/* 025 */ { 0, 255 },
/* 026 */ { 0, 255 },
/* 027 */ { 0, 255 },
/* 028 */ { 0, 255 },
/* 029 */ { 0, 255 },
/* 030 */ { 0, 255 },
/* 031 */ { 0, 255 },
/* 032 */ { SIR_MAC_PWR_CONSTRAINT_EID_MIN, SIR_MAC_PWR_CONSTRAINT_EID_MAX },
/* 033 */ { SIR_MAC_PWR_CAPABILITY_EID_MIN, SIR_MAC_PWR_CAPABILITY_EID_MAX },
/* 034 */ { SIR_MAC_TPC_REQ_EID_MIN, SIR_MAC_TPC_REQ_EID_MAX },
/* 035 */ { SIR_MAC_TPC_RPT_EID_MIN, SIR_MAC_TPC_RPT_EID_MAX },
/* 036 */ { SIR_MAC_SPRTD_CHNLS_EID_MIN, SIR_MAC_SPRTD_CHNLS_EID_MAX },
/* 037 */ { SIR_MAC_CHNL_SWITCH_ANN_EID_MIN, SIR_MAC_CHNL_SWITCH_ANN_EID_MAX },
/* 038 */ { SIR_MAC_MEAS_REQ_EID_MIN, SIR_MAC_MEAS_REQ_EID_MAX },
/* 039 */ { SIR_MAC_MEAS_RPT_EID_MIN, SIR_MAC_MEAS_RPT_EID_MAX },
/* 040 */ { SIR_MAC_QUIET_EID_MIN, SIR_MAC_QUIET_EID_MAX },
/* 041 */ { SIR_MAC_IBSS_DFS_EID_MIN, SIR_MAC_IBSS_DFS_EID_MAX },
/* 042 */ { SIR_MAC_ERP_INFO_EID_MIN, SIR_MAC_ERP_INFO_EID_MAX },
/* 043 */ { SIR_MAC_TS_DELAY_EID_MIN, SIR_MAC_TS_DELAY_EID_MAX },
/* 044 */ { SIR_MAC_TCLAS_PROC_EID_MIN, SIR_MAC_TCLAS_PROC_EID_MAX },
/* 045 */ { SIR_MAC_QOS_ACTION_EID_MIN, SIR_MAC_QOS_ACTION_EID_MAX },
/* 046 */ { SIR_MAC_QOS_CAPABILITY_EID_MIN, SIR_MAC_QOS_CAPABILITY_EID_MAX },
/* 047 */ { 0, 255 },
/* 048 */ { SIR_MAC_RSN_EID_MIN, SIR_MAC_RSN_EID_MAX },
/* 049 */ { 0, 255 },
/* 050 */ { SIR_MAC_EXTENDED_RATE_EID_MIN, SIR_MAC_EXTENDED_RATE_EID_MAX },
/* 051 */ { 0, 255 },
/* 052 */ { 0, 255 },
/* 053 */ { 0, 255 },
/* 054 */ { 0, 255 },
/* 055 */ { 0, 255 },
/* 056 */ { 0, 255 },
/* 057 */ { 0, 255 },
/* 058 */ { 0, 255 },
/* 059 */ { 0, 255 },
/* 060 */ { 0, 255 },
/* 061 */ { 0, 255 },
/* 062 */ { 0, 255 },
/* 063 */ { 0, 255 },
/* 064 */ { 0, 255 },
/* 065 */ { 0, 255 },
/* 066 */ { 0, 255 },
/* 067 */ { 0, 255 },
#ifdef FEATURE_WLAN_WAPI
/* 068 */ { DOT11F_EID_WAPI, DOT11F_IE_WAPI_MAX_LEN },
#else
/* 068 */ { 0, 255 },
#endif /* FEATURE_WLAN_WAPI */
/* 069 */ { 0, 255 },
/* 070 */ { 0, 255 },
/* 071 */ { 0, 255 },
/* 072 */ { 0, 255 },
/* 073 */ { 0, 255 },
/* 074 */ { 0, 255 },
/* 075 */ { 0, 255 },
/* 076 */ { 0, 255 },
/* 077 */ { 0, 255 },
/* 078 */ { 0, 255 },
/* 079 */ { 0, 255 },
/* 080 */ { 0, 255 },
/* 081 */ { 0, 255 },
/* 082 */ { 0, 255 },
/* 083 */ { 0, 255 },
/* 084 */ { 0, 255 },
/* 085 */ { 0, 255 },
/* 086 */ { 0, 255 },
/* 087 */ { 0, 255 },
/* 088 */ { 0, 255 },
/* 089 */ { 0, 255 },
/* 090 */ { 0, 255 },
/* 091 */ { 0, 255 },
/* 092 */ { 0, 255 },
/* 093 */ { 0, 255 },
/* 094 */ { 0, 255 },
/* 095 */ { 0, 255 },
/* 096 */ { 0, 255 },
/* 097 */ { 0, 255 },
/* 098 */ { 0, 255 },
/* 099 */ { 0, 255 },
/* 100 */ { 0, 255 },
/* 101 */ { 0, 255 },
/* 102 */ { 0, 255 },
/* 103 */ { 0, 255 },
/* 104 */ { 0, 255 },
/* 105 */ { 0, 255 },
/* 106 */ { 0, 255 },
/* 107 */ { 0, 255 },
/* 108 */ { 0, 255 },
/* 109 */ { 0, 255 },
/* 110 */ { 0, 255 },
/* 111 */ { 0, 255 },
/* 112 */ { 0, 255 },
/* 113 */ { 0, 255 },
/* 114 */ { 0, 255 },
/* 115 */ { 0, 255 },
/* 116 */ { 0, 255 },
/* 117 */ { 0, 255 },
/* 118 */ { 0, 255 },
/* 119 */ { 0, 255 },
/* 120 */ { 0, 255 },
/* 121 */ { 0, 255 },
/* 122 */ { 0, 255 },
/* 123 */ { 0, 255 },
/* 124 */ { 0, 255 },
/* 125 */ { 0, 255 },
/* 126 */ { 0, 255 },
/* 127 */ { 0, 255 },
/* 128 */ { 0, 255 },
/* 129 */ { 0, 255 },
/* 130 */ { 0, 255 },
/* 131 */ { 0, 255 },
/* 132 */ { 0, 255 },
/* 133 */ { 0, 255 },
/* 134 */ { 0, 255 },
/* 135 */ { 0, 255 },
/* 136 */ { 0, 255 },
/* 137 */ { 0, 255 },
/* 138 */ { 0, 255 },
/* 139 */ { 0, 255 },
/* 140 */ { 0, 255 },
/* 141 */ { 0, 255 },
/* 142 */ { 0, 255 },
/* 143 */ { 0, 255 },
/* 144 */ { 0, 255 },
/* 145 */ { 0, 255 },
/* 146 */ { 0, 255 },
/* 147 */ { 0, 255 },
/* 148 */ { 0, 255 },
/* 149 */ { 0, 255 },
/* 150 */ { 0, 255 },
/* 151 */ { 0, 255 },
/* 152 */ { 0, 255 },
/* 153 */ { 0, 255 },
/* 154 */ { 0, 255 },
/* 155 */ { 0, 255 },
/* 156 */ { 0, 255 },
/* 157 */ { 0, 255 },
/* 158 */ { 0, 255 },
/* 159 */ { 0, 255 },
/* 160 */ { 0, 255 },
/* 161 */ { 0, 255 },
/* 162 */ { 0, 255 },
/* 163 */ { 0, 255 },
/* 164 */ { 0, 255 },
/* 165 */ { 0, 255 },
/* 166 */ { 0, 255 },
/* 167 */ { 0, 255 },
/* 168 */ { 0, 255 },
/* 169 */ { 0, 255 },
/* 170 */ { 0, 255 },
/* 171 */ { 0, 255 },
/* 172 */ { 0, 255 },
/* 173 */ { 0, 255 },
/* 174 */ { 0, 255 },
/* 175 */ { 0, 255 },
/* 176 */ { 0, 255 },
/* 177 */ { 0, 255 },
/* 178 */ { 0, 255 },
/* 179 */ { 0, 255 },
/* 180 */ { 0, 255 },
/* 181 */ { 0, 255 },
/* 182 */ { 0, 255 },
/* 183 */ { 0, 255 },
/* 184 */ { 0, 255 },
/* 185 */ { 0, 255 },
/* 186 */ { 0, 255 },
/* 187 */ { 0, 255 },
/* 188 */ { 0, 255 },
/* 189 */ { 0, 255 },
/* 190 */ { 0, 255 },
/* 191 */ { 0, 255 },
/* 192 */ { 0, 255 },
/* 193 */ { 0, 255 },
/* 194 */ { 0, 255 },
/* 195 */ { 0, 255 },
/* 196 */ { 0, 255 },
/* 197 */ { 0, 255 },
/* 198 */ { 0, 255 },
/* 199 */ { 0, 255 },
/* 200 */ { 0, 255 },
/* 201 */ { 0, 255 },
/* 202 */ { 0, 255 },
/* 203 */ { 0, 255 },
/* 204 */ { 0, 255 },
/* 205 */ { 0, 255 },
/* 206 */ { 0, 255 },
/* 207 */ { 0, 255 },
/* 208 */ { 0, 255 },
/* 209 */ { 0, 255 },
/* 210 */ { 0, 255 },
/* 211 */ { 0, 255 },
/* 212 */ { 0, 255 },
/* 213 */ { 0, 255 },
/* 214 */ { 0, 255 },
/* 215 */ { 0, 255 },
/* 216 */ { 0, 255 },
/* 217 */ { 0, 255 },
/* 218 */ { 0, 255 },
/* 219 */ { 0, 255 },
/* 220 */ { 0, 255 },
/* 221 */ { SIR_MAC_WPA_EID_MIN, SIR_MAC_WPA_EID_MAX },
/* 222 */ { 0, 255 },
/* 223 */ { 0, 255 },
/* 224 */ { 0, 255 },
/* 225 */ { 0, 255 },
/* 226 */ { 0, 255 },
/* 227 */ { 0, 255 },
/* 228 */ { 0, 255 },
/* 229 */ { 0, 255 },
/* 230 */ { 0, 255 },
/* 231 */ { 0, 255 },
/* 232 */ { 0, 255 },
/* 233 */ { 0, 255 },
/* 234 */ { 0, 255 },
/* 235 */ { 0, 255 },
/* 236 */ { 0, 255 },
/* 237 */ { 0, 255 },
/* 238 */ { 0, 255 },
/* 239 */ { 0, 255 },
/* 240 */ { 0, 255 },
/* 241 */ { 0, 255 },
/* 242 */ { 0, 255 },
/* 243 */ { 0, 255 },
/* 244 */ { 0, 255 },
/* 245 */ { 0, 255 },
/* 246 */ { 0, 255 },
/* 247 */ { 0, 255 },
/* 248 */ { 0, 255 },
/* 249 */ { 0, 255 },
/* 250 */ { 0, 255 },
/* 251 */ { 0, 255 },
/* 252 */ { 0, 255 },
/* 253 */ { 0, 255 },
/* 254 */ { 0, 255 },
/* 255 */ { SIR_MAC_ANI_WORKAROUND_EID_MIN, SIR_MAC_ANI_WORKAROUND_EID_MAX }
};

#if 0
//Don't not insert entry into the table, put it to the end. If you have to insert, make sure it is also
//reflected in eCsrCountryIndex
static tCsrCountryInfo gCsrCountryInfo[eCSR_NUM_COUNTRY_INDEX] =
{
    {REG_DOMAIN_FCC, {'U', 'S', ' '}},       //USA/******The "US" MUST be at index 0*******/
    {REG_DOMAIN_WORLD, {'A', 'D', ' '}},     //ANDORRA
    {REG_DOMAIN_WORLD, {'A', 'E', ' '}},       //UAE
    {REG_DOMAIN_WORLD, {'A', 'F', ' '}},     //AFGHANISTAN
    {REG_DOMAIN_WORLD, {'A', 'G', ' '}},     //ANTIGUA AND BARBUDA
    {REG_DOMAIN_WORLD, {'A', 'I', ' '}},     //ANGUILLA
    {REG_DOMAIN_HI_5GHZ, {'A', 'L', ' '}},     //ALBANIA
    {REG_DOMAIN_WORLD, {'A', 'M', ' '}},     //ARMENIA
    {REG_DOMAIN_WORLD, {'A', 'N', ' '}},     //NETHERLANDS ANTILLES
    {REG_DOMAIN_WORLD, {'A', 'O', ' '}},     //ANGOLA
    {REG_DOMAIN_WORLD, {'A', 'Q', ' '}},     //ANTARCTICA
    {REG_DOMAIN_HI_5GHZ, {'A', 'R', ' '}},    //ARGENTINA
    {REG_DOMAIN_FCC, {'A', 'S', ' '}},     //AMERICAN SOMOA
    {REG_DOMAIN_ETSI, {'A', 'T', ' '}},      //AUSTRIA
    {REG_DOMAIN_ETSI, {'A', 'U', ' '}},      //AUSTRALIA
    {REG_DOMAIN_WORLD, {'A', 'W', ' '}},     //ARUBA
    {REG_DOMAIN_WORLD, {'A', 'X', ' '}},     //ALAND ISLANDS
    {REG_DOMAIN_WORLD, {'A', 'Z', ' '}},     //AZERBAIJAN
    {REG_DOMAIN_WORLD, {'B', 'A', ' '}},     //BOSNIA AND HERZEGOVINA
    {REG_DOMAIN_WORLD, {'B', 'B', ' '}},     //BARBADOS
    {REG_DOMAIN_WORLD, {'B', 'D', ' '}},     //BANGLADESH
    {REG_DOMAIN_ETSI, {'B', 'E', ' '}},      //BELGIUM
    {REG_DOMAIN_WORLD, {'B', 'F', ' '}},     //BURKINA FASO
    {REG_DOMAIN_HI_5GHZ, {'B', 'G', ' '}},      //BULGARIA
    {REG_DOMAIN_WORLD, {'B', 'H', ' '}},     //BAHRAIN
    {REG_DOMAIN_WORLD, {'B', 'I', ' '}},     //BURUNDI
    {REG_DOMAIN_WORLD, {'B', 'J', ' '}},     //BENIN
    {REG_DOMAIN_WORLD, {'B', 'L', ' '}},     //SAINT BARTHELEMY
    {REG_DOMAIN_ETSI, {'B', 'M', ' '}},     //BERMUDA
    {REG_DOMAIN_WORLD, {'B', 'N', ' '}},     //BRUNEI DARUSSALAM
    {REG_DOMAIN_WORLD, {'B', 'O', ' '}},     //BOLIVIA
    {REG_DOMAIN_WORLD, {'B', 'R', ' '}},       //BRAZIL
    {REG_DOMAIN_WORLD, {'B', 'S', ' '}},     //BAHAMAS
    {REG_DOMAIN_WORLD, {'B', 'T', ' '}},     //BHUTAN
    {REG_DOMAIN_WORLD, {'B', 'V', ' '}},     //BOUVET ISLAND
    {REG_DOMAIN_WORLD, {'B', 'W', ' '}},     //BOTSWANA
    {REG_DOMAIN_WORLD, {'B', 'Y', ' '}},     //BELARUS
    {REG_DOMAIN_WORLD, {'B', 'Z', ' '}},     //BELIZE
    {REG_DOMAIN_FCC, {'C', 'A', ' '}},       //CANADA
    {REG_DOMAIN_WORLD, {'C', 'C', ' '}},     //COCOS (KEELING) ISLANDS
    {REG_DOMAIN_WORLD, {'C', 'D', ' '}},     //CONGO, THE DEMOCRATIC REPUBLIC OF THE
    {REG_DOMAIN_WORLD, {'C', 'F', ' '}},     //CENTRAL AFRICAN REPUBLIC
    {REG_DOMAIN_WORLD, {'C', 'G', ' '}},     //CONGO
    {REG_DOMAIN_ETSI, {'C', 'H', ' '}},      //SWITZERLAND
    {REG_DOMAIN_WORLD, {'C', 'I', ' '}},     //COTE D'IVOIRE
    {REG_DOMAIN_WORLD, {'C', 'K', ' '}},     //COOK ISLANDS
    {REG_DOMAIN_WORLD, {'C', 'L', ' '}},       //CHILE
    {REG_DOMAIN_WORLD, {'C', 'M', ' '}},     //CAMEROON
    {REG_DOMAIN_HI_5GHZ, {'C', 'N', ' '}},   //CHINA
    {REG_DOMAIN_WORLD, {'C', 'O', ' '}},       //COLOMBIA
    {REG_DOMAIN_WORLD, {'C', 'R', ' '}},       //COSTA RICA
    {REG_DOMAIN_WORLD, {'C', 'U', ' '}},     //CUBA  
    {REG_DOMAIN_WORLD, {'C', 'V', ' '}},     //CAPE VERDE
    {REG_DOMAIN_WORLD, {'C', 'X', ' '}},     //CHRISTMAS ISLAND
    {REG_DOMAIN_WORLD, {'C', 'Y', ' '}},      //CYPRUS
    {REG_DOMAIN_HI_5GHZ, {'C', 'Z', ' '}},      //CZECH REPUBLIC
    {REG_DOMAIN_ETSI, {'D', 'E', ' '}},      //GERMANY
    {REG_DOMAIN_WORLD, {'D', 'J', ' '}},     //DJIBOUTI
    {REG_DOMAIN_ETSI, {'D', 'K', ' '}},      //DENMARK
    {REG_DOMAIN_WORLD, {'D', 'M', ' '}},     //DOMINICA
    {REG_DOMAIN_WORLD, {'D', 'O', ' '}},       //DOMINICAN REPUBLIC
    {REG_DOMAIN_WORLD, {'D', 'Z', ' '}},     //ALGERIA
    {REG_DOMAIN_WORLD, {'E', 'C', ' '}},       //ECUADOR
    {REG_DOMAIN_HI_5GHZ, {'E', 'E', ' '}},      //ESTONIA
    {REG_DOMAIN_WORLD, {'E', 'G', ' '}},     //EGYPT
    {REG_DOMAIN_WORLD, {'E', 'H', ' '}},     //WESTERN SAHARA
    {REG_DOMAIN_WORLD, {'E', 'R', ' '}},     //ERITREA
    {REG_DOMAIN_ETSI, {'E', 'S', ' '}},      //SPAIN
    {REG_DOMAIN_WORLD, {'E', 'T', ' '}},     //ETHIOPIA
    {REG_DOMAIN_WORLD, {'F', 'I', ' '}},      //FINLAND
    {REG_DOMAIN_WORLD, {'F', 'J', ' '}},     //FIJI
    {REG_DOMAIN_WORLD, {'F', 'K', ' '}},     //FALKLAND ISLANDS (MALVINAS)
    {REG_DOMAIN_WORLD, {'F', 'M', ' '}},     //MICRONESIA, FEDERATED STATES OF
    {REG_DOMAIN_WORLD, {'F', 'O', ' '}},     //FAROE ISLANDS
    {REG_DOMAIN_ETSI, {'F', 'R', ' '}},      //FRANCE
    {REG_DOMAIN_WORLD, {'G', 'A', ' '}},     //GABON
    {REG_DOMAIN_ETSI, {'G', 'B', ' '}},      //UNITED KINGDOM
    {REG_DOMAIN_WORLD, {'G', 'D', ' '}},     //GRENADA
    {REG_DOMAIN_HI_5GHZ, {'G', 'E', ' '}},     //GEORGIA
    {REG_DOMAIN_WORLD, {'G', 'F', ' '}},     //FRENCH GUIANA
    {REG_DOMAIN_ETSI, {'G', 'G', ' '}},      //GUERNSEY
    {REG_DOMAIN_WORLD, {'G', 'H', ' '}},     //GHANA
    {REG_DOMAIN_WORLD, {'G', 'I', ' '}},      //GIBRALTAR
    {REG_DOMAIN_WORLD, {'G', 'L', ' '}},     //GREENLAND
    {REG_DOMAIN_WORLD, {'G', 'M', ' '}},     //GAMBIA
    {REG_DOMAIN_WORLD, {'G', 'N', ' '}},     //GUINEA
    {REG_DOMAIN_WORLD, {'G', 'P', ' '}},     //GUADELOUPE
    {REG_DOMAIN_WORLD, {'G', 'Q', ' '}},     //EQUATORIAL GUINEA
    {REG_DOMAIN_ETSI, {'G', 'R', ' '}},      //GREECE
    {REG_DOMAIN_WORLD, {'G', 'S', ' '}},     //SOUTH GEORGIA AND THE SOUTH SANDWICH ISLANDS
    {REG_DOMAIN_WORLD, {'G', 'T', ' '}},       //GUATEMALA
    {REG_DOMAIN_WORLD, {'G', 'U', ' '}},     //GUAM
    {REG_DOMAIN_WORLD, {'G', 'W', ' '}},     //GUINEA-BISSAU
    {REG_DOMAIN_WORLD, {'G', 'Y', ' '}},     //GUYANA
    {REG_DOMAIN_WORLD, {'H', 'K', ' '}},      //HONGKONG
    {REG_DOMAIN_WORLD, {'H', 'M', ' '}},     //HEARD ISLAND AND MCDONALD ISLANDS
    {REG_DOMAIN_WORLD, {'H', 'N', ' '}},       //HONDURAS
    {REG_DOMAIN_HI_5GHZ, {'H', 'R', ' '}},      //CROATIA
    {REG_DOMAIN_WORLD, {'H', 'T', ' '}},     //HAITI
    {REG_DOMAIN_HI_5GHZ, {'H', 'U', ' '}},      //HUNGARY
    {REG_DOMAIN_APAC, {'I', 'D', ' '}},     //INDONESIA
    {REG_DOMAIN_ETSI, {'I', 'E', ' '}},     //IRELAND        
    {REG_DOMAIN_WORLD, {'I', 'L', ' '}},        //ISREAL
    {REG_DOMAIN_ETSI, {'I', 'M', ' '}},      //ISLE OF MAN
    {REG_DOMAIN_WORLD, {'I', 'N', ' '}},      //INDIA
    {REG_DOMAIN_ETSI, {'I', 'O', ' '}},     //BRITISH INDIAN OCEAN TERRITORY
    {REG_DOMAIN_WORLD, {'I', 'Q', ' '}},     //IRAQ
    {REG_DOMAIN_WORLD, {'I', 'R', ' '}},     //IRAN, ISLAMIC REPUBLIC OF
    {REG_DOMAIN_WORLD, {'I', 'S', ' '}},      //ICELAND
    {REG_DOMAIN_ETSI, {'I', 'T', ' '}},      //ITALY
    {REG_DOMAIN_ETSI, {'J', 'E', ' '}},      //JERSEY
    {REG_DOMAIN_WORLD, {'J', 'M', ' '}},     //JAMAICA
    {REG_DOMAIN_WORLD, {'J', 'O', ' '}},     //JORDAN
    {REG_DOMAIN_JAPAN, {'J', 'P', ' '}},     //JAPAN
    {REG_DOMAIN_WORLD, {'K', 'E', ' '}},     //KENYA
    {REG_DOMAIN_WORLD, {'K', 'G', ' '}},     //KYRGYZSTAN
    {REG_DOMAIN_WORLD, {'K', 'H', ' '}},     //CAMBODIA
    {REG_DOMAIN_WORLD, {'K', 'I', ' '}},     //KIRIBATI
    {REG_DOMAIN_WORLD, {'K', 'M', ' '}},     //COMOROS
    {REG_DOMAIN_WORLD, {'K', 'N', ' '}},     //SAINT KITTS AND NEVIS
    {REG_DOMAIN_KOREA, {'K', 'P', ' '}},     //KOREA, DEMOCRATIC PEOPLE'S REPUBLIC OF
    {REG_DOMAIN_KOREA, {'K', 'R', ' '}},     //KOREA, REPUBLIC OF 
    {REG_DOMAIN_WORLD, {'K', 'W', ' '}},     //KUWAIT
    {REG_DOMAIN_WORLD, {'K', 'Y', ' '}},     //CAYMAN ISLANDS
    {REG_DOMAIN_WORLD, {'K', 'Z', ' '}},     //KAZAKHSTAN
    {REG_DOMAIN_WORLD, {'L', 'A', ' '}},     //LAO PEOPLE'S DEMOCRATIC REPUBLIC
    {REG_DOMAIN_WORLD, {'L', 'B', ' '}},     //LEBANON
    {REG_DOMAIN_WORLD, {'L', 'C', ' '}},     //SAINT LUCIA
    {REG_DOMAIN_ETSI, {'L', 'I', ' '}},      //LIECHTENSTEIN
    {REG_DOMAIN_WORLD, {'L', 'K', ' '}},     //SRI LANKA
    {REG_DOMAIN_WORLD, {'L', 'R', ' '}},     //LIBERIA
    {REG_DOMAIN_WORLD, {'L', 'S', ' '}},     //LESOTHO
    {REG_DOMAIN_HI_5GHZ, {'L', 'T', ' '}},      //LITHUANIA
    {REG_DOMAIN_ETSI, {'L', 'U', ' '}},      //LUXEMBOURG
    {REG_DOMAIN_HI_5GHZ, {'L', 'V', ' '}},      //LATVIA
    {REG_DOMAIN_WORLD, {'L', 'Y', ' '}},     //LIBYAN ARAB JAMAHIRIYA
    {REG_DOMAIN_WORLD, {'M', 'A', ' '}},     //MOROCCO
    {REG_DOMAIN_ETSI, {'M', 'C', ' '}},      //MONACO
    {REG_DOMAIN_WORLD, {'M', 'D', ' '}},     //MOLDOVA, REPUBLIC OF
    {REG_DOMAIN_WORLD, {'M', 'E', ' '}},     //MONTENEGRO
    {REG_DOMAIN_WORLD, {'M', 'G', ' '}},     //MADAGASCAR
    {REG_DOMAIN_WORLD, {'M', 'H', ' '}},     //MARSHALL ISLANDS
    {REG_DOMAIN_WORLD, {'M', 'K', ' '}},     //MACEDONIA, THE FORMER YUGOSLAV REPUBLIC OF
    {REG_DOMAIN_WORLD, {'M', 'L', ' '}},     //MALI
    {REG_DOMAIN_WORLD, {'M', 'M', ' '}},     //MYANMAR
    {REG_DOMAIN_HI_5GHZ, {'M', 'N', ' '}},     //MONGOLIA
    {REG_DOMAIN_WORLD, {'M', 'O', ' '}},     //MACAO
    {REG_DOMAIN_WORLD, {'M', 'P', ' '}},     //NORTHERN MARIANA ISLANDS
    {REG_DOMAIN_WORLD, {'M', 'Q', ' '}},     //MARTINIQUE
    {REG_DOMAIN_WORLD, {'M', 'R', ' '}},     //MAURITANIA
    {REG_DOMAIN_WORLD, {'M', 'S', ' '}},     //MONTSERRAT
    {REG_DOMAIN_WORLD, {'M', 'T', ' '}},      //MALTA     
    {REG_DOMAIN_WORLD, {'M', 'U', ' '}},     //MAURITIUS
    {REG_DOMAIN_WORLD, {'M', 'V', ' '}},     //MALDIVES
    {REG_DOMAIN_WORLD, {'M', 'W', ' '}},     //MALAWI
    {REG_DOMAIN_WORLD, {'M', 'X', ' '}},       //MEXICO
    {REG_DOMAIN_HI_5GHZ, {'M', 'Y', ' '}},       //MALAYSIA
    {REG_DOMAIN_WORLD, {'M', 'Z', ' '}},     //MOZAMBIQUE
    {REG_DOMAIN_WORLD, {'N', 'A', ' '}},     //NAMIBIA
    {REG_DOMAIN_WORLD, {'N', 'C', ' '}},     //NEW CALEDONIA
    {REG_DOMAIN_WORLD, {'N', 'E', ' '}},     //NIGER
    {REG_DOMAIN_WORLD, {'N', 'F', ' '}},     //NORFOLD ISLAND
    {REG_DOMAIN_WORLD, {'N', 'G', ' '}},     //NIGERIA
    {REG_DOMAIN_WORLD, {'N', 'I', ' '}},       //NICARAGUA
    {REG_DOMAIN_ETSI, {'N', 'L', ' '}},      //NETHERLANDS
    {REG_DOMAIN_WORLD, {'N', 'O', ' '}},      //NORWAY
    {REG_DOMAIN_WORLD, {'N', 'P', ' '}},     //NEPAL
    {REG_DOMAIN_WORLD, {'N', 'R', ' '}},     //NAURU
    {REG_DOMAIN_WORLD, {'N', 'U', ' '}},     //NIUE
    {REG_DOMAIN_ETSI, {'N', 'Z', ' '}},      //NEW ZEALAND
    {REG_DOMAIN_WORLD, {'O', 'M', ' '}},     //OMAN
    {REG_DOMAIN_WORLD, {'P', 'A', ' '}},       //PANAMA
    {REG_DOMAIN_WORLD, {'P', 'E', ' '}},       //PERU
    {REG_DOMAIN_WORLD, {'P', 'F', ' '}},     //FRENCH POLYNESIA
    {REG_DOMAIN_WORLD, {'P', 'G', ' '}},     //PAPUA NEW GUINEA
    {REG_DOMAIN_WORLD, {'P', 'H', ' '}},      //PHILIPPINES
    {REG_DOMAIN_WORLD, {'P', 'K', ' '}},     //PAKISTAN
    {REG_DOMAIN_WORLD, {'P', 'L', ' '}},      //POLAND
    {REG_DOMAIN_WORLD, {'P', 'M', ' '}},     //SAINT PIERRE AND MIQUELON
    {REG_DOMAIN_WORLD, {'P', 'N', ' '}},     //PITCAIRN
    {REG_DOMAIN_FCC, {'P', 'R', ' '}},       //PUERTO RICO
    {REG_DOMAIN_WORLD, {'P', 'S', ' '}},        //PALESTINIAN TERRITORY, OCCUPIED
    {REG_DOMAIN_ETSI, {'P', 'T', ' '}},      //PORTUGAL
    {REG_DOMAIN_WORLD, {'P', 'W', ' '}},     //PALAU
    {REG_DOMAIN_WORLD, {'P', 'Y', ' '}},     //PARAGUAY
    {REG_DOMAIN_WORLD, {'Q', 'A', ' '}},     //QATAR
    {REG_DOMAIN_WORLD, {'R', 'E', ' '}},     //REUNION
    {REG_DOMAIN_HI_5GHZ, {'R', 'O', ' '}},      //ROMANIA
    {REG_DOMAIN_HI_5GHZ, {'R', 'S', ' '}},      //SERBIA
    {REG_DOMAIN_WORLD, {'R', 'U', ' '}},       //RUSSIA
    {REG_DOMAIN_WORLD, {'R', 'W', ' '}},     //RWANDA
    {REG_DOMAIN_WORLD, {'S', 'A', ' '}},      //SAUDI ARABIA
    {REG_DOMAIN_WORLD, {'S', 'B', ' '}},     //SOLOMON ISLANDS
    {REG_DOMAIN_ETSI, {'S', 'C', ' '}},      //SEYCHELLES
    {REG_DOMAIN_WORLD, {'S', 'D', ' '}},     //SUDAN
    {REG_DOMAIN_ETSI, {'S', 'E', ' '}},      //SWEDEN
    {REG_DOMAIN_APAC, {'S', 'G', ' '}},      //SINGAPORE
    {REG_DOMAIN_WORLD, {'S', 'H', ' '}},     //SAINT HELENA
    {REG_DOMAIN_HI_5GHZ, {'S', 'I', ' '}},      //SLOVENNIA
    {REG_DOMAIN_WORLD, {'S', 'J', ' '}},     //SVALBARD AND JAN MAYEN
    {REG_DOMAIN_HI_5GHZ, {'S', 'K', ' '}},      //SLOVAKIA
    {REG_DOMAIN_WORLD, {'S', 'L', ' '}},     //SIERRA LEONE      
    {REG_DOMAIN_WORLD, {'S', 'M', ' '}},     //SAN MARINO
    {REG_DOMAIN_WORLD, {'S', 'N', ' '}},     //SENEGAL
    {REG_DOMAIN_WORLD, {'S', 'O', ' '}},     //SOMALIA
    {REG_DOMAIN_WORLD, {'S', 'R', ' '}},     //SURINAME
    {REG_DOMAIN_WORLD, {'S', 'T', ' '}},     //SAO TOME AND PRINCIPE
    {REG_DOMAIN_WORLD, {'S', 'V', ' '}},       //EL SALVADOR
    {REG_DOMAIN_WORLD, {'S', 'Y', ' '}},     //SYRIAN ARAB REPUBLIC
    {REG_DOMAIN_WORLD, {'S', 'Z', ' '}},     //SWAZILAND
    {REG_DOMAIN_WORLD, {'T', 'C', ' '}},     //TURKS AND CAICOS ISLANDS
    {REG_DOMAIN_WORLD, {'T', 'D', ' '}},     //CHAD
    {REG_DOMAIN_WORLD, {'T', 'F', ' '}},     //FRENCH SOUTHERN TERRITORIES
    {REG_DOMAIN_WORLD, {'T', 'G', ' '}},     //TOGO
    {REG_DOMAIN_WORLD, {'T', 'H', ' '}},       //THAILAND
    {REG_DOMAIN_WORLD, {'T', 'J', ' '}},     //TAJIKISTAN
    {REG_DOMAIN_WORLD, {'T', 'K', ' '}},     //TOKELAU
    {REG_DOMAIN_WORLD, {'T', 'L', ' '}},     //TIMOR-LESTE
    {REG_DOMAIN_WORLD, {'T', 'M', ' '}},     //TURKMENISTAN
    {REG_DOMAIN_WORLD, {'T', 'N', ' '}},     //TUNISIA
    {REG_DOMAIN_WORLD, {'T', 'O', ' '}},     //TONGA
    {REG_DOMAIN_WORLD, {'T', 'R', ' '}},      //TURKEY
    {REG_DOMAIN_WORLD, {'T', 'T', ' '}},     //TRINIDAD AND TOBAGO
    {REG_DOMAIN_WORLD, {'T', 'V', ' '}},     //TUVALU
    {REG_DOMAIN_HI_5GHZ, {'T', 'W', ' '}},       //TAIWAN, PROVINCE OF CHINA
    {REG_DOMAIN_WORLD, {'T', 'Z', ' '}},     //TANZANIA, UNITED REPUBLIC OF
    {REG_DOMAIN_HI_5GHZ, {'U', 'A', ' '}},       //UKRAINE
    {REG_DOMAIN_WORLD, {'U', 'G', ' '}},     //UGANDA
    {REG_DOMAIN_FCC, {'U', 'M', ' '}},       //UNITED STATES MINOR OUTLYING ISLANDS
    {REG_DOMAIN_WORLD, {'U', 'Y', ' '}},       //URUGUAY
    {REG_DOMAIN_HI_5GHZ, {'U', 'Z', ' '}},     //UZBEKISTAN
    {REG_DOMAIN_ETSI, {'V', 'A', ' '}},      //HOLY SEE (VATICAN CITY STATE)
    {REG_DOMAIN_WORLD, {'V', 'C', ' '}},     //SAINT VINCENT AND THE GRENADINES
    {REG_DOMAIN_HI_5GHZ, {'V', 'E', ' '}},       //VENEZUELA
    {REG_DOMAIN_ETSI, {'V', 'G', ' '}},       //VIRGIN ISLANDS, BRITISH
    {REG_DOMAIN_FCC, {'V', 'I', ' '}},       //VIRGIN ISLANDS, US
    {REG_DOMAIN_WORLD, {'V', 'N', ' '}},      //VIET NAM
    {REG_DOMAIN_WORLD, {'V', 'U', ' '}},     //VANUATU
    {REG_DOMAIN_WORLD, {'W', 'F', ' '}},     //WALLIS AND FUTUNA
    {REG_DOMAIN_WORLD, {'W', 'S', ' '}},     //SOMOA
    {REG_DOMAIN_WORLD, {'Y', 'E', ' '}},     //YEMEN
    {REG_DOMAIN_WORLD, {'Y', 'T', ' '}},     //MAYOTTE
    {REG_DOMAIN_WORLD, {'Z', 'A', ' '}},      //SOUTH AFRICA
    {REG_DOMAIN_WORLD, {'Z', 'M', ' '}},     //ZAMBIA
    {REG_DOMAIN_WORLD, {'Z', 'W', ' '}},     //ZIMBABWE

    {REG_DOMAIN_KOREA, {'K', '1', ' '}},    //Korea alternate 1
    {REG_DOMAIN_KOREA, {'K', '2', ' '}},    //Korea alternate 2
    {REG_DOMAIN_KOREA, {'K', '3', ' '}},    //Korea alternate 3
    {REG_DOMAIN_KOREA, {'K', '4', ' '}},    //Korea alternate 4
};


//The channels listed here doesn't mean they are valid channels for certain domain. They are here only to present
//whether they should be passive scanned.
tCsrDomainChnInfo gCsrDomainChnInfo[NUM_REG_DOMAINS] =
{
    //REG_DOMAIN_FCC
    {
        REG_DOMAIN_FCC,
        45, //Num channels
        //Channels
        {
            //5GHz
            //5180 - 5240
            {36, eSIR_ACTIVE_SCAN},
            {40, eSIR_ACTIVE_SCAN},
            {44, eSIR_ACTIVE_SCAN},
            {48, eSIR_ACTIVE_SCAN},
            //5250 to 5350
            {52, eSIR_PASSIVE_SCAN},
            {56, eSIR_PASSIVE_SCAN},
            {60, eSIR_PASSIVE_SCAN},
            {64, eSIR_PASSIVE_SCAN},
            //5470 to 5725
            {100, eSIR_PASSIVE_SCAN},
            {104, eSIR_PASSIVE_SCAN},
            {108, eSIR_PASSIVE_SCAN},
            {112, eSIR_PASSIVE_SCAN},
            {116, eSIR_PASSIVE_SCAN},
            {120, eSIR_PASSIVE_SCAN},
            {124, eSIR_PASSIVE_SCAN},
            {128, eSIR_PASSIVE_SCAN},
            {132, eSIR_PASSIVE_SCAN},
            {136, eSIR_PASSIVE_SCAN},
            {140, eSIR_PASSIVE_SCAN},
            //5745 - 5825
            {149, eSIR_ACTIVE_SCAN},
            {153, eSIR_ACTIVE_SCAN},
            {157, eSIR_ACTIVE_SCAN},
            {161, eSIR_ACTIVE_SCAN},
            {165, eSIR_ACTIVE_SCAN},
            //4.9GHz
            //4920 - 5080
            {240, eSIR_ACTIVE_SCAN},
            {244, eSIR_ACTIVE_SCAN},
            {248, eSIR_ACTIVE_SCAN},
            {252, eSIR_ACTIVE_SCAN},
            {208, eSIR_ACTIVE_SCAN},
            {212, eSIR_ACTIVE_SCAN},
            {216, eSIR_ACTIVE_SCAN},
            //2,4GHz
            {1, eSIR_ACTIVE_SCAN},
            {2, eSIR_ACTIVE_SCAN},
            {3, eSIR_ACTIVE_SCAN},
            {4, eSIR_ACTIVE_SCAN},
            {5, eSIR_ACTIVE_SCAN},
            {6, eSIR_ACTIVE_SCAN},
            {7, eSIR_ACTIVE_SCAN},
            {8, eSIR_ACTIVE_SCAN},
            {9, eSIR_ACTIVE_SCAN},
            {10, eSIR_ACTIVE_SCAN},
            {11, eSIR_ACTIVE_SCAN},
            {12, eSIR_ACTIVE_SCAN},
            {13, eSIR_ACTIVE_SCAN},
            {14, eSIR_ACTIVE_SCAN},
        }
    },
    //REG_DOMAIN_ETSI
    {
        REG_DOMAIN_ETSI,
        45, //Num channels
        //Channels
        {
            //5GHz
            //5180 - 5240
            {36, eSIR_ACTIVE_SCAN},
            {40, eSIR_ACTIVE_SCAN},
            {44, eSIR_ACTIVE_SCAN},
            {48, eSIR_ACTIVE_SCAN},
            //5250 to 5350
            {52, eSIR_PASSIVE_SCAN},
            {56, eSIR_PASSIVE_SCAN},
            {60, eSIR_PASSIVE_SCAN},
            {64, eSIR_PASSIVE_SCAN},
            //5470 to 5725
            {100, eSIR_PASSIVE_SCAN},
            {104, eSIR_PASSIVE_SCAN},
            {108, eSIR_PASSIVE_SCAN},
            {112, eSIR_PASSIVE_SCAN},
            {116, eSIR_PASSIVE_SCAN},
            {120, eSIR_PASSIVE_SCAN},
            {124, eSIR_PASSIVE_SCAN},
            {128, eSIR_PASSIVE_SCAN},
            {132, eSIR_PASSIVE_SCAN},
            {136, eSIR_PASSIVE_SCAN},
            {140, eSIR_PASSIVE_SCAN},
            //5745 - 5825
            {149, eSIR_ACTIVE_SCAN},
            {153, eSIR_ACTIVE_SCAN},
            {157, eSIR_ACTIVE_SCAN},
            {161, eSIR_ACTIVE_SCAN},
            {165, eSIR_ACTIVE_SCAN},
            //4.9GHz
            //4920 - 5080
            {240, eSIR_ACTIVE_SCAN},
            {244, eSIR_ACTIVE_SCAN},
            {248, eSIR_ACTIVE_SCAN},
            {252, eSIR_ACTIVE_SCAN},
            {208, eSIR_ACTIVE_SCAN},
            {212, eSIR_ACTIVE_SCAN},
            {216, eSIR_ACTIVE_SCAN},
            //2,4GHz
            {1, eSIR_ACTIVE_SCAN},
            {2, eSIR_ACTIVE_SCAN},
            {3, eSIR_ACTIVE_SCAN},
            {4, eSIR_ACTIVE_SCAN},
            {5, eSIR_ACTIVE_SCAN},
            {6, eSIR_ACTIVE_SCAN},
            {7, eSIR_ACTIVE_SCAN},
            {8, eSIR_ACTIVE_SCAN},
            {9, eSIR_ACTIVE_SCAN},
            {10, eSIR_ACTIVE_SCAN},
            {11, eSIR_ACTIVE_SCAN},
            {12, eSIR_ACTIVE_SCAN},
            {13, eSIR_ACTIVE_SCAN},
            {14, eSIR_ACTIVE_SCAN},
        }
    },
    //REG_DOMAIN_JAPAN
    {
        REG_DOMAIN_JAPAN,
        45, //Num channels
        //Channels
        {
            //5GHz
            //5180 - 5240
            {36, eSIR_ACTIVE_SCAN},
            {40, eSIR_ACTIVE_SCAN},
            {44, eSIR_ACTIVE_SCAN},
            {48, eSIR_ACTIVE_SCAN},
            //5250 to 5350
            {52, eSIR_PASSIVE_SCAN},
            {56, eSIR_PASSIVE_SCAN},
            {60, eSIR_PASSIVE_SCAN},
            {64, eSIR_PASSIVE_SCAN},
            //5470 to 5725
            {100, eSIR_PASSIVE_SCAN},
            {104, eSIR_PASSIVE_SCAN},
            {108, eSIR_PASSIVE_SCAN},
            {112, eSIR_PASSIVE_SCAN},
            {116, eSIR_PASSIVE_SCAN},
            {120, eSIR_PASSIVE_SCAN},
            {124, eSIR_PASSIVE_SCAN},
            {128, eSIR_PASSIVE_SCAN},
            {132, eSIR_PASSIVE_SCAN},
            {136, eSIR_PASSIVE_SCAN},
            {140, eSIR_PASSIVE_SCAN},
            //5745 - 5825
            {149, eSIR_ACTIVE_SCAN},
            {153, eSIR_ACTIVE_SCAN},
            {157, eSIR_ACTIVE_SCAN},
            {161, eSIR_ACTIVE_SCAN},
            {165, eSIR_ACTIVE_SCAN},
            //4.9GHz
            //4920 - 5080
            {240, eSIR_ACTIVE_SCAN},
            {244, eSIR_ACTIVE_SCAN},
            {248, eSIR_ACTIVE_SCAN},
            {252, eSIR_ACTIVE_SCAN},
            {208, eSIR_ACTIVE_SCAN},
            {212, eSIR_ACTIVE_SCAN},
            {216, eSIR_ACTIVE_SCAN},
            //2,4GHz
            {1, eSIR_ACTIVE_SCAN},
            {2, eSIR_ACTIVE_SCAN},
            {3, eSIR_ACTIVE_SCAN},
            {4, eSIR_ACTIVE_SCAN},
            {5, eSIR_ACTIVE_SCAN},
            {6, eSIR_ACTIVE_SCAN},
            {7, eSIR_ACTIVE_SCAN},
            {8, eSIR_ACTIVE_SCAN},
            {9, eSIR_ACTIVE_SCAN},
            {10, eSIR_ACTIVE_SCAN},
            {11, eSIR_ACTIVE_SCAN},
            {12, eSIR_ACTIVE_SCAN},
            {13, eSIR_ACTIVE_SCAN},
            {14, eSIR_ACTIVE_SCAN},
        }
    },
    //REG_DOMAIN_WORLD
    {
        REG_DOMAIN_WORLD,
        45, //Num channels
        //Channels
        {
            //5GHz
            //5180 - 5240
            {36, eSIR_ACTIVE_SCAN},
            {40, eSIR_ACTIVE_SCAN},
            {44, eSIR_ACTIVE_SCAN},
            {48, eSIR_ACTIVE_SCAN},
            //5250 to 5350
            {52, eSIR_ACTIVE_SCAN},
            {56, eSIR_ACTIVE_SCAN},
            {60, eSIR_ACTIVE_SCAN},
            {64, eSIR_ACTIVE_SCAN},
            //5470 to 5725
            {100, eSIR_ACTIVE_SCAN},
            {104, eSIR_ACTIVE_SCAN},
            {108, eSIR_ACTIVE_SCAN},
            {112, eSIR_ACTIVE_SCAN},
            {116, eSIR_ACTIVE_SCAN},
            {120, eSIR_ACTIVE_SCAN},
            {124, eSIR_ACTIVE_SCAN},
            {128, eSIR_ACTIVE_SCAN},
            {132, eSIR_ACTIVE_SCAN},
            {136, eSIR_ACTIVE_SCAN},
            {140, eSIR_ACTIVE_SCAN},
            //5745 - 5825
            {149, eSIR_ACTIVE_SCAN},
            {153, eSIR_ACTIVE_SCAN},
            {157, eSIR_ACTIVE_SCAN},
            {161, eSIR_ACTIVE_SCAN},
            {165, eSIR_ACTIVE_SCAN},
            //4.9GHz
            //4920 - 5080
            {240, eSIR_ACTIVE_SCAN},
            {244, eSIR_ACTIVE_SCAN},
            {248, eSIR_ACTIVE_SCAN},
            {252, eSIR_ACTIVE_SCAN},
            {208, eSIR_ACTIVE_SCAN},
            {212, eSIR_ACTIVE_SCAN},
            {216, eSIR_ACTIVE_SCAN},
            //2,4GHz
            {1, eSIR_ACTIVE_SCAN},
            {2, eSIR_ACTIVE_SCAN},
            {3, eSIR_ACTIVE_SCAN},
            {4, eSIR_ACTIVE_SCAN},
            {5, eSIR_ACTIVE_SCAN},
            {6, eSIR_ACTIVE_SCAN},
            {7, eSIR_ACTIVE_SCAN},
            {8, eSIR_ACTIVE_SCAN},
            {9, eSIR_ACTIVE_SCAN},
            {10, eSIR_ACTIVE_SCAN},
            {11, eSIR_ACTIVE_SCAN},
            {12, eSIR_ACTIVE_SCAN},
            {13, eSIR_ACTIVE_SCAN},
            {14, eSIR_ACTIVE_SCAN},
        }
    },
    //REG_DOMAIN_N_AMER_EXC_FCC
    {
        REG_DOMAIN_N_AMER_EXC_FCC,
        45, //Num channels
        //Channels
        {
            //5GHz
            //5180 - 5240
            {36, eSIR_ACTIVE_SCAN},
            {40, eSIR_ACTIVE_SCAN},
            {44, eSIR_ACTIVE_SCAN},
            {48, eSIR_ACTIVE_SCAN},
            //5250 to 5350
            {52, eSIR_PASSIVE_SCAN},
            {56, eSIR_PASSIVE_SCAN},
            {60, eSIR_PASSIVE_SCAN},
            {64, eSIR_PASSIVE_SCAN},
            //5470 to 5725
            {100, eSIR_ACTIVE_SCAN},
            {104, eSIR_ACTIVE_SCAN},
            {108, eSIR_ACTIVE_SCAN},
            {112, eSIR_ACTIVE_SCAN},
            {116, eSIR_ACTIVE_SCAN},
            {120, eSIR_ACTIVE_SCAN},
            {124, eSIR_ACTIVE_SCAN},
            {128, eSIR_ACTIVE_SCAN},
            {132, eSIR_ACTIVE_SCAN},
            {136, eSIR_ACTIVE_SCAN},
            {140, eSIR_ACTIVE_SCAN},
            //5745 - 5825
            {149, eSIR_ACTIVE_SCAN},
            {153, eSIR_ACTIVE_SCAN},
            {157, eSIR_ACTIVE_SCAN},
            {161, eSIR_ACTIVE_SCAN},
            {165, eSIR_ACTIVE_SCAN},
            //4.9GHz
            //4920 - 5080
            {240, eSIR_ACTIVE_SCAN},
            {244, eSIR_ACTIVE_SCAN},
            {248, eSIR_ACTIVE_SCAN},
            {252, eSIR_ACTIVE_SCAN},
            {208, eSIR_ACTIVE_SCAN},
            {212, eSIR_ACTIVE_SCAN},
            {216, eSIR_ACTIVE_SCAN},
            //2,4GHz
            {1, eSIR_ACTIVE_SCAN},
            {2, eSIR_ACTIVE_SCAN},
            {3, eSIR_ACTIVE_SCAN},
            {4, eSIR_ACTIVE_SCAN},
            {5, eSIR_ACTIVE_SCAN},
            {6, eSIR_ACTIVE_SCAN},
            {7, eSIR_ACTIVE_SCAN},
            {8, eSIR_ACTIVE_SCAN},
            {9, eSIR_ACTIVE_SCAN},
            {10, eSIR_ACTIVE_SCAN},
            {11, eSIR_ACTIVE_SCAN},
            {12, eSIR_ACTIVE_SCAN},
            {13, eSIR_ACTIVE_SCAN},
            {14, eSIR_ACTIVE_SCAN},
        }
    },
    //REG_DOMAIN_APAC
    {
        REG_DOMAIN_APAC,
        45, //Num channels
        //Channels
        {
            //5GHz
            //5180 - 5240
            {36, eSIR_ACTIVE_SCAN},
            {40, eSIR_ACTIVE_SCAN},
            {44, eSIR_ACTIVE_SCAN},
            {48, eSIR_ACTIVE_SCAN},
            //5250 to 5350
            {52, eSIR_PASSIVE_SCAN},
            {56, eSIR_PASSIVE_SCAN},
            {60, eSIR_PASSIVE_SCAN},
            {64, eSIR_PASSIVE_SCAN},
            //5470 to 5725
            {100, eSIR_ACTIVE_SCAN},
            {104, eSIR_ACTIVE_SCAN},
            {108, eSIR_ACTIVE_SCAN},
            {112, eSIR_ACTIVE_SCAN},
            {116, eSIR_ACTIVE_SCAN},
            {120, eSIR_ACTIVE_SCAN},
            {124, eSIR_ACTIVE_SCAN},
            {128, eSIR_ACTIVE_SCAN},
            {132, eSIR_ACTIVE_SCAN},
            {136, eSIR_ACTIVE_SCAN},
            {140, eSIR_ACTIVE_SCAN},
            //5745 - 5825
            {149, eSIR_ACTIVE_SCAN},
            {153, eSIR_ACTIVE_SCAN},
            {157, eSIR_ACTIVE_SCAN},
            {161, eSIR_ACTIVE_SCAN},
            {165, eSIR_ACTIVE_SCAN},
            //4.9GHz
            //4920 - 5080
            {240, eSIR_ACTIVE_SCAN},
            {244, eSIR_ACTIVE_SCAN},
            {248, eSIR_ACTIVE_SCAN},
            {252, eSIR_ACTIVE_SCAN},
            {208, eSIR_ACTIVE_SCAN},
            {212, eSIR_ACTIVE_SCAN},
            {216, eSIR_ACTIVE_SCAN},
            //2,4GHz
            {1, eSIR_ACTIVE_SCAN},
            {2, eSIR_ACTIVE_SCAN},
            {3, eSIR_ACTIVE_SCAN},
            {4, eSIR_ACTIVE_SCAN},
            {5, eSIR_ACTIVE_SCAN},
            {6, eSIR_ACTIVE_SCAN},
            {7, eSIR_ACTIVE_SCAN},
            {8, eSIR_ACTIVE_SCAN},
            {9, eSIR_ACTIVE_SCAN},
            {10, eSIR_ACTIVE_SCAN},
            {11, eSIR_ACTIVE_SCAN},
            {12, eSIR_ACTIVE_SCAN},
            {13, eSIR_ACTIVE_SCAN},
            {14, eSIR_ACTIVE_SCAN},
        }
    },
    //REG_DOMAIN_KOREA
    {
        REG_DOMAIN_KOREA,
        45, //Num channels
        //Channels
        {
            //5GHz
            //5180 - 5240
            {36, eSIR_ACTIVE_SCAN},
            {40, eSIR_ACTIVE_SCAN},
            {44, eSIR_ACTIVE_SCAN},
            {48, eSIR_ACTIVE_SCAN},
            //5250 to 5350
            {52, eSIR_PASSIVE_SCAN},
            {56, eSIR_PASSIVE_SCAN},
            {60, eSIR_PASSIVE_SCAN},
            {64, eSIR_PASSIVE_SCAN},
            //5470 to 5725
            {100, eSIR_PASSIVE_SCAN},
            {104, eSIR_PASSIVE_SCAN},
            {108, eSIR_PASSIVE_SCAN},
            {112, eSIR_PASSIVE_SCAN},
            {116, eSIR_PASSIVE_SCAN},
            {120, eSIR_PASSIVE_SCAN},
            {124, eSIR_PASSIVE_SCAN},
            {128, eSIR_PASSIVE_SCAN},
            {132, eSIR_PASSIVE_SCAN},
            {136, eSIR_PASSIVE_SCAN},
            {140, eSIR_PASSIVE_SCAN},
            //5745 - 5825
            {149, eSIR_ACTIVE_SCAN},
            {153, eSIR_ACTIVE_SCAN},
            {157, eSIR_ACTIVE_SCAN},
            {161, eSIR_ACTIVE_SCAN},
            {165, eSIR_ACTIVE_SCAN},
            //4.9GHz
            //4920 - 5080
            {240, eSIR_ACTIVE_SCAN},
            {244, eSIR_ACTIVE_SCAN},
            {248, eSIR_ACTIVE_SCAN},
            {252, eSIR_ACTIVE_SCAN},
            {208, eSIR_ACTIVE_SCAN},
            {212, eSIR_ACTIVE_SCAN},
            {216, eSIR_ACTIVE_SCAN},
            //2,4GHz
            {1, eSIR_ACTIVE_SCAN},
            {2, eSIR_ACTIVE_SCAN},
            {3, eSIR_ACTIVE_SCAN},
            {4, eSIR_ACTIVE_SCAN},
            {5, eSIR_ACTIVE_SCAN},
            {6, eSIR_ACTIVE_SCAN},
            {7, eSIR_ACTIVE_SCAN},
            {8, eSIR_ACTIVE_SCAN},
            {9, eSIR_ACTIVE_SCAN},
            {10, eSIR_ACTIVE_SCAN},
            {11, eSIR_ACTIVE_SCAN},
            {12, eSIR_ACTIVE_SCAN},
            {13, eSIR_ACTIVE_SCAN},
            {14, eSIR_ACTIVE_SCAN},
        }
    },
    //REG_DOMAIN_HI_5GHZ
    {
        REG_DOMAIN_HI_5GHZ,
        45, //Num channels
        //Channels
        {
            //5GHz
            //5180 - 5240
            {36, eSIR_ACTIVE_SCAN},
            {40, eSIR_ACTIVE_SCAN},
            {44, eSIR_ACTIVE_SCAN},
            {48, eSIR_ACTIVE_SCAN},
            //5250 to 5350
            {52, eSIR_ACTIVE_SCAN},
            {56, eSIR_ACTIVE_SCAN},
            {60, eSIR_ACTIVE_SCAN},
            {64, eSIR_ACTIVE_SCAN},
            //5470 to 5725
            {100, eSIR_ACTIVE_SCAN},
            {104, eSIR_ACTIVE_SCAN},
            {108, eSIR_ACTIVE_SCAN},
            {112, eSIR_ACTIVE_SCAN},
            {116, eSIR_ACTIVE_SCAN},
            {120, eSIR_ACTIVE_SCAN},
            {124, eSIR_ACTIVE_SCAN},
            {128, eSIR_ACTIVE_SCAN},
            {132, eSIR_ACTIVE_SCAN},
            {136, eSIR_ACTIVE_SCAN},
            {140, eSIR_ACTIVE_SCAN},
            //5745 - 5825
            {149, eSIR_ACTIVE_SCAN},
            {153, eSIR_ACTIVE_SCAN},
            {157, eSIR_ACTIVE_SCAN},
            {161, eSIR_ACTIVE_SCAN},
            {165, eSIR_ACTIVE_SCAN},
            //4.9GHz
            //4920 - 5080
            {240, eSIR_ACTIVE_SCAN},
            {244, eSIR_ACTIVE_SCAN},
            {248, eSIR_ACTIVE_SCAN},
            {252, eSIR_ACTIVE_SCAN},
            {208, eSIR_ACTIVE_SCAN},
            {212, eSIR_ACTIVE_SCAN},
            {216, eSIR_ACTIVE_SCAN},
            //2,4GHz
            {1, eSIR_ACTIVE_SCAN},
            {2, eSIR_ACTIVE_SCAN},
            {3, eSIR_ACTIVE_SCAN},
            {4, eSIR_ACTIVE_SCAN},
            {5, eSIR_ACTIVE_SCAN},
            {6, eSIR_ACTIVE_SCAN},
            {7, eSIR_ACTIVE_SCAN},
            {8, eSIR_ACTIVE_SCAN},
            {9, eSIR_ACTIVE_SCAN},
            {10, eSIR_ACTIVE_SCAN},
            {11, eSIR_ACTIVE_SCAN},
            {12, eSIR_ACTIVE_SCAN},
            {13, eSIR_ACTIVE_SCAN},
            {14, eSIR_ACTIVE_SCAN},
        }
    },
    //REG_DOMAIN_NO_5GHZ
    {
        REG_DOMAIN_NO_5GHZ,
        45, //Num channels
        //Channels
        {
            //5GHz
            //5180 - 5240
            {36, eSIR_ACTIVE_SCAN},
            {40, eSIR_ACTIVE_SCAN},
            {44, eSIR_ACTIVE_SCAN},
            {48, eSIR_ACTIVE_SCAN},
            //5250 to 5350
            {52, eSIR_ACTIVE_SCAN},
            {56, eSIR_ACTIVE_SCAN},
            {60, eSIR_ACTIVE_SCAN},
            {64, eSIR_ACTIVE_SCAN},
            //5470 to 5725
            {100, eSIR_ACTIVE_SCAN},
            {104, eSIR_ACTIVE_SCAN},
            {108, eSIR_ACTIVE_SCAN},
            {112, eSIR_ACTIVE_SCAN},
            {116, eSIR_ACTIVE_SCAN},
            {120, eSIR_ACTIVE_SCAN},
            {124, eSIR_ACTIVE_SCAN},
            {128, eSIR_ACTIVE_SCAN},
            {132, eSIR_ACTIVE_SCAN},
            {136, eSIR_ACTIVE_SCAN},
            {140, eSIR_ACTIVE_SCAN},
            //5745 - 5825
            {149, eSIR_ACTIVE_SCAN},
            {153, eSIR_ACTIVE_SCAN},
            {157, eSIR_ACTIVE_SCAN},
            {161, eSIR_ACTIVE_SCAN},
            {165, eSIR_ACTIVE_SCAN},
            //4.9GHz
            //4920 - 5080
            {240, eSIR_ACTIVE_SCAN},
            {244, eSIR_ACTIVE_SCAN},
            {248, eSIR_ACTIVE_SCAN},
            {252, eSIR_ACTIVE_SCAN},
            {208, eSIR_ACTIVE_SCAN},
            {212, eSIR_ACTIVE_SCAN},
            {216, eSIR_ACTIVE_SCAN},
            //2,4GHz
            {1, eSIR_ACTIVE_SCAN},
            {2, eSIR_ACTIVE_SCAN},
            {3, eSIR_ACTIVE_SCAN},
            {4, eSIR_ACTIVE_SCAN},
            {5, eSIR_ACTIVE_SCAN},
            {6, eSIR_ACTIVE_SCAN},
            {7, eSIR_ACTIVE_SCAN},
            {8, eSIR_ACTIVE_SCAN},
            {9, eSIR_ACTIVE_SCAN},
            {10, eSIR_ACTIVE_SCAN},
            {11, eSIR_ACTIVE_SCAN},
            {12, eSIR_ACTIVE_SCAN},
            {13, eSIR_ACTIVE_SCAN},
            {14, eSIR_ACTIVE_SCAN},
        }
    },
};
#endif

extern const tRfChannelProps rfChannels[NUM_RF_CHANNELS];

////////////////////////////////////////////////////////////////////////

/**
 * \var gPhyRatesSuppt
 *
 * \brief Rate support lookup table
 *
 *
 * This is a  lookup table indexing rates &  configuration parameters to
 * support.  Given a rate (in  unites of 0.5Mpbs) & three booleans (MIMO
 * Enabled, Channel  Bonding Enabled, & Concatenation  Enabled), one can
 * determine  whether  the given  rate  is  supported  by computing  two
 * indices.  The  first maps  the rate to  table row as  indicated below
 * (i.e. eHddSuppRate_6Mbps maps to  row zero, eHddSuppRate_9Mbps to row
 * 1, and so on).  Index two can be computed like so:
 *
 * \code
   idx2 = ( fEsf  ? 0x4 : 0x0 ) |
          ( fCb   ? 0x2 : 0x0 ) |
          ( fMimo ? 0x1 : 0x0 );
 * \endcode
 *
 *
 * Given that:
 *
 \code
   fSupported = gPhyRatesSuppt[idx1][idx2];
 \endcode
 *
 *
 * This table is based on  the document "PHY Supported Rates.doc".  This
 * table is  permissive in that a  rate is reflected  as being supported
 * even  when turning  off an  enabled feature  would be  required.  For
 * instance, "PHY Supported Rates"  lists 42Mpbs as unsupported when CB,
 * ESF, &  MIMO are all  on.  However,  if we turn  off either of  CB or
 * MIMO, it then becomes supported.   Therefore, we mark it as supported
 * even in index 7 of this table.
 *
 *
 */

static const tANI_BOOLEAN gPhyRatesSuppt[24][8] = {

    // SSF   SSF    SSF    SSF    ESF    ESF    ESF    ESF
    // SIMO  MIMO   SIMO   MIMO   SIMO   MIMO   SIMO   MIMO
    // No CB No CB  CB     CB     No CB  No CB  CB     CB
    { TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE  }, // 6Mbps
    { TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE  }, // 9Mbps
    { TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE  }, // 12Mbps
    { TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE  }, // 18Mbps
    { FALSE, FALSE, TRUE,  TRUE,  FALSE, FALSE, TRUE,  TRUE  }, // 20Mbps
    { TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE  }, // 24Mbps
    { TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE  }, // 36Mbps
    { FALSE, FALSE, TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE  }, // 40Mbps
    { FALSE, FALSE, TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE  }, // 42Mbps
    { TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE  }, // 48Mbps
    { TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE  }, // 54Mbps
    { FALSE, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE  }, // 72Mbps
    { FALSE, FALSE, TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE  }, // 80Mbps
    { FALSE, FALSE, TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE  }, // 84Mbps
    { FALSE, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE  }, // 96Mbps
    { FALSE, TRUE,  TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE  }, // 108Mbps
    { FALSE, FALSE, TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE  }, // 120Mbps
    { FALSE, FALSE, TRUE,  TRUE,  FALSE, TRUE,  TRUE,  TRUE  }, // 126Mbps
    { FALSE, FALSE, FALSE, TRUE,  FALSE, FALSE, FALSE, TRUE  }, // 144Mbps
    { FALSE, FALSE, FALSE, TRUE,  FALSE, FALSE, FALSE, TRUE  }, // 160Mbps
    { FALSE, FALSE, FALSE, TRUE,  FALSE, FALSE, FALSE, TRUE  }, // 168Mbps
    { FALSE, FALSE, FALSE, TRUE,  FALSE, FALSE, FALSE, TRUE  }, // 192Mbps
    { FALSE, FALSE, FALSE, TRUE,  FALSE, FALSE, FALSE, TRUE  }, // 216Mbps
    { FALSE, FALSE, FALSE, TRUE,  FALSE, FALSE, FALSE, TRUE  }, // 240Mbps

};

#define CASE_RETURN_STR(n) case (n): return (#n)

const char *
get_eRoamCmdStatus_str(eRoamCmdStatus val)
{
    switch (val)
    {
        CASE_RETURN_STR(eCSR_ROAM_CANCELLED);
        CASE_RETURN_STR(eCSR_ROAM_ROAMING_START);
        CASE_RETURN_STR(eCSR_ROAM_ROAMING_COMPLETION);
        CASE_RETURN_STR(eCSR_ROAM_ASSOCIATION_START);
        CASE_RETURN_STR(eCSR_ROAM_ASSOCIATION_COMPLETION);
        CASE_RETURN_STR(eCSR_ROAM_DISASSOCIATED);
        CASE_RETURN_STR(eCSR_ROAM_SHOULD_ROAM);
        CASE_RETURN_STR(eCSR_ROAM_SCAN_FOUND_NEW_BSS);
        CASE_RETURN_STR(eCSR_ROAM_LOSTLINK);
    default:
        return "unknown";
    }
}

const char *
get_eCsrRoamResult_str(eCsrRoamResult val)
{
    switch (val)
    {
        CASE_RETURN_STR(eCSR_ROAM_RESULT_NONE);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_FAILURE);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_ASSOCIATED);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_NOT_ASSOCIATED);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_MIC_FAILURE);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_FORCED);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_DISASSOC_IND);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_DEAUTH_IND);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_CAP_CHANGED);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_IBSS_CONNECT);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_IBSS_INACTIVE);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_IBSS_NEW_PEER);
        CASE_RETURN_STR(eCSR_ROAM_RESULT_IBSS_COALESCED);
    default:
        return "unknown";
    }
}



tANI_BOOLEAN csrGetBssIdBssDesc( tHalHandle hHal, tSirBssDescription *pSirBssDesc, tCsrBssid *pBssId )
{
    vos_mem_copy(pBssId, &pSirBssDesc->bssId[ 0 ], sizeof(tCsrBssid));
    return( TRUE );
}


tANI_BOOLEAN csrIsBssIdEqual( tHalHandle hHal, tSirBssDescription *pSirBssDesc1, tSirBssDescription *pSirBssDesc2 )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_BOOLEAN fEqual = FALSE;
    tCsrBssid bssId1;
    tCsrBssid bssId2;

    do {
        if ( !pSirBssDesc1 ) break;
        if ( !pSirBssDesc2 ) break;

        if ( !csrGetBssIdBssDesc( pMac, pSirBssDesc1, &bssId1 ) ) break;
        if ( !csrGetBssIdBssDesc( pMac, pSirBssDesc2, &bssId2 ) ) break;

        //sirCompareMacAddr
        fEqual = csrIsMacAddressEqual(pMac, &bssId1, &bssId2);

    } while( 0 );

    return( fEqual );
}

tANI_BOOLEAN csrIsConnStateConnectedIbss( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    return( eCSR_ASSOC_STATE_TYPE_IBSS_CONNECTED == pMac->roam.roamSession[sessionId].connectState );
}

tANI_BOOLEAN csrIsConnStateDisconnectedIbss( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    return( eCSR_ASSOC_STATE_TYPE_IBSS_DISCONNECTED == pMac->roam.roamSession[sessionId].connectState );
}

tANI_BOOLEAN csrIsConnStateConnectedInfra( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    return( eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED == pMac->roam.roamSession[sessionId].connectState );
}

tANI_BOOLEAN csrIsConnStateConnected( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    if( csrIsConnStateConnectedIbss( pMac, sessionId ) || csrIsConnStateConnectedInfra( pMac, sessionId ) || csrIsConnStateConnectedWds( pMac, sessionId) )
        return TRUE;
    else
        return FALSE;
}

tANI_BOOLEAN csrIsConnStateInfra( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    return( csrIsConnStateConnectedInfra( pMac, sessionId ) );
}

tANI_BOOLEAN csrIsConnStateIbss( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    return( csrIsConnStateConnectedIbss( pMac, sessionId ) || csrIsConnStateDisconnectedIbss( pMac, sessionId ) );
}


tANI_BOOLEAN csrIsConnStateConnectedWds( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    return( eCSR_ASSOC_STATE_TYPE_WDS_CONNECTED == pMac->roam.roamSession[sessionId].connectState );
}

tANI_BOOLEAN csrIsConnStateConnectedInfraAp( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    return( (eCSR_ASSOC_STATE_TYPE_INFRA_CONNECTED == pMac->roam.roamSession[sessionId].connectState) ||
        (eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED == pMac->roam.roamSession[sessionId].connectState ) );
}

tANI_BOOLEAN csrIsConnStateDisconnectedWds( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    return( eCSR_ASSOC_STATE_TYPE_WDS_DISCONNECTED == pMac->roam.roamSession[sessionId].connectState );
}

tANI_BOOLEAN csrIsConnStateWds( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    return( csrIsConnStateConnectedWds( pMac, sessionId ) ||
        csrIsConnStateDisconnectedWds( pMac, sessionId ) );
}

tANI_BOOLEAN csrIsConnStateAp( tpAniSirGlobal pMac,  tANI_U32 sessionId )
{
    tCsrRoamSession *pSession;
    pSession = CSR_GET_SESSION(pMac, sessionId);
    if (!pSession)
        return eANI_BOOLEAN_FALSE;
    if ( CSR_IS_INFRA_AP(&pSession->connectedProfile) )
    {
        return eANI_BOOLEAN_TRUE;
    }
    return eANI_BOOLEAN_FALSE;
}

tANI_BOOLEAN csrIsAnySessionInConnectState( tpAniSirGlobal pMac )
{
    tANI_U32 i;
    tANI_BOOLEAN fRc = eANI_BOOLEAN_FALSE;

    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) &&
            ( csrIsConnStateInfra( pMac, i )
            || csrIsConnStateIbss( pMac, i )
            || csrIsConnStateAp( pMac, i) ) )
        {
            fRc = eANI_BOOLEAN_TRUE;
            break;
        }
    }

    return ( fRc );
}

tANI_S8 csrGetInfraSessionId( tpAniSirGlobal pMac )
{
    tANI_U8 i;
    tANI_S8 sessionid = -1;

    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) && csrIsConnStateInfra( pMac, i )  )
        {
            sessionid = i;
            break;
        }
    }

    return ( sessionid );
}

tANI_U8 csrGetInfraOperationChannel( tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    tANI_U8 channel;

    if( CSR_IS_SESSION_VALID( pMac, sessionId ))
    {
        channel = pMac->roam.roamSession[sessionId].connectedProfile.operationChannel;
    }
    else
    {
        channel = 0;
    }
    return channel;
}

tANI_BOOLEAN csrIsSessionClientAndConnected(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    tCsrRoamSession *pSession = NULL;
    if ( CSR_IS_SESSION_VALID( pMac, sessionId) && csrIsConnStateInfra( pMac, sessionId))
    {
        pSession = CSR_GET_SESSION( pMac, sessionId);
        if (NULL != pSession->pCurRoamProfile)
        {
            if ((pSession->pCurRoamProfile->csrPersona == VOS_STA_MODE) ||
                (pSession->pCurRoamProfile->csrPersona == VOS_P2P_CLIENT_MODE))
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}
//This routine will return operating channel on FIRST BSS that is active/operating to be used for concurrency mode.
//If other BSS is not up or not connected it will return 0 

tANI_U8 csrGetConcurrentOperationChannel( tpAniSirGlobal pMac )
{
  tCsrRoamSession *pSession = NULL;
  tANI_U8 i = 0;

  for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
  {
      if( CSR_IS_SESSION_VALID( pMac, i ) )
      {
          pSession = CSR_GET_SESSION( pMac, i );

          if (NULL != pSession->pCurRoamProfile)
          {
              if (
                      (((pSession->pCurRoamProfile->csrPersona == VOS_STA_MODE) ||
                        (pSession->pCurRoamProfile->csrPersona == VOS_P2P_CLIENT_MODE)) &&
                       (pSession->connectState == eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED)) 
                      || 
                      (((pSession->pCurRoamProfile->csrPersona == VOS_P2P_GO_MODE) ||
                        (pSession->pCurRoamProfile->csrPersona == VOS_STA_SAP_MODE)) &&
                       (pSession->connectState != eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED))
                 )
                  return (pSession->connectedProfile.operationChannel);
          }

      }
  }
  return 0;
}

tANI_BOOLEAN csrIsAllSessionDisconnected( tpAniSirGlobal pMac )
{
    tANI_U32 i;
    tANI_BOOLEAN fRc = eANI_BOOLEAN_TRUE;

    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) && !csrIsConnStateDisconnected( pMac, i ) )
        {
            fRc = eANI_BOOLEAN_FALSE;
            break;
        }
    }

    return ( fRc );
}

tANI_BOOLEAN csrIsStaSessionConnected( tpAniSirGlobal pMac )
{
    tANI_U32 i;
    tANI_BOOLEAN fRc = eANI_BOOLEAN_FALSE;
    tCsrRoamSession *pSession = NULL;
    tANI_U32 countSta = 0;

    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) && !csrIsConnStateDisconnected( pMac, i ) )
        {
            pSession = CSR_GET_SESSION( pMac, i );

            if (NULL != pSession->pCurRoamProfile)
            {
                if (pSession->pCurRoamProfile->csrPersona == VOS_STA_MODE) {
                    countSta++;
                }
            }
        }
    }

    /* return TRUE if one of the following conditions is TRUE:
     * - more than one STA session connected
     */
    if ( countSta > 0) {
        fRc = eANI_BOOLEAN_TRUE;
    }

    return( fRc );
}

tANI_BOOLEAN csrIsP2pSessionConnected( tpAniSirGlobal pMac )
{
    tANI_U32 i;
    tANI_BOOLEAN fRc = eANI_BOOLEAN_FALSE;
    tCsrRoamSession *pSession = NULL;
    tANI_U32 countP2pCli = 0;
    tANI_U32 countP2pGo = 0;
    tANI_U32 countSAP = 0;

    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) && !csrIsConnStateDisconnected( pMac, i ) )
        {
            pSession = CSR_GET_SESSION( pMac, i );

            if (NULL != pSession->pCurRoamProfile)
            {
                if (pSession->pCurRoamProfile->csrPersona == VOS_P2P_CLIENT_MODE) {
                    countP2pCli++;
                }

                if (pSession->pCurRoamProfile->csrPersona == VOS_P2P_GO_MODE) {
                    countP2pGo++;
                }

                if (pSession->pCurRoamProfile->csrPersona == VOS_STA_SAP_MODE) {
                    countSAP++;
                }
            }
        }
    }

    /* return TRUE if one of the following conditions is TRUE:
     * - at least one P2P CLI session is connected
     * - at least one P2P GO session is connected
     */
    if ( (countP2pCli > 0) || (countP2pGo > 0 ) || (countSAP > 0 ) ) {
        fRc = eANI_BOOLEAN_TRUE;
    }

    return( fRc );
}

tANI_BOOLEAN csrIsAnySessionConnected( tpAniSirGlobal pMac )
{
    tANI_U32 i, count;
    tANI_BOOLEAN fRc = eANI_BOOLEAN_FALSE;

    count = 0;
    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) && !csrIsConnStateDisconnected( pMac, i ) )
        {
            count++;
        }
    }

    if (count > 0)
    {
        fRc = eANI_BOOLEAN_TRUE;
    }
    return( fRc );
}

tANI_BOOLEAN csrIsInfraConnected( tpAniSirGlobal pMac )
{
    tANI_U32 i;
    tANI_BOOLEAN fRc = eANI_BOOLEAN_FALSE;

    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) && csrIsConnStateConnectedInfra( pMac, i ) )
        {
            fRc = eANI_BOOLEAN_TRUE;
            break;
        }
    }

    return ( fRc );
}

tANI_BOOLEAN csrIsConcurrentInfraConnected( tpAniSirGlobal pMac )
{
    tANI_U32 i, noOfConnectedInfra = 0;

    tANI_BOOLEAN fRc = eANI_BOOLEAN_FALSE;

    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) && csrIsConnStateConnectedInfra( pMac, i ) )
        {
            ++noOfConnectedInfra;
        }
    }

    // More than one Infra Sta Connected
    if(noOfConnectedInfra > 1)
    {
        fRc = eANI_BOOLEAN_TRUE;
    }

    return ( fRc );
}

tANI_BOOLEAN csrIsIBSSStarted( tpAniSirGlobal pMac )
{
    tANI_U32 i;
    tANI_BOOLEAN fRc = eANI_BOOLEAN_FALSE;

    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) && csrIsConnStateIbss( pMac, i ) )
        {
            fRc = eANI_BOOLEAN_TRUE;
            break;
        }
    }

    return ( fRc );
}


tANI_BOOLEAN csrIsBTAMPStarted( tpAniSirGlobal pMac )
{
    tANI_U32 i;
    tANI_BOOLEAN fRc = eANI_BOOLEAN_FALSE;

    for( i = 0; i < CSR_ROAM_SESSION_MAX; i++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) && csrIsConnStateConnectedWds( pMac, i ) )
        {
            fRc = eANI_BOOLEAN_TRUE;
            break;
        }
    }

    return ( fRc );
}

tANI_BOOLEAN csrIsConcurrentSessionRunning( tpAniSirGlobal pMac )
{
    tANI_U32 sessionId, noOfCocurrentSession = 0;
    eCsrConnectState connectState;

    tANI_BOOLEAN fRc = eANI_BOOLEAN_FALSE;

    for( sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
        {
           connectState =  pMac->roam.roamSession[sessionId].connectState;
           if( (eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED == connectState) ||
               (eCSR_ASSOC_STATE_TYPE_INFRA_CONNECTED == connectState) ||
               (eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED == connectState) )
           {
              ++noOfCocurrentSession;
           }
        }
    }

    // More than one session is Up and Running
    if(noOfCocurrentSession > 1)
    {
        fRc = eANI_BOOLEAN_TRUE;
    }

    return ( fRc );
}

tANI_BOOLEAN csrIsInfraApStarted( tpAniSirGlobal pMac )
{
    tANI_U32 sessionId;
    tANI_BOOLEAN fRc = eANI_BOOLEAN_FALSE;

    for( sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++ )
    {
        if( CSR_IS_SESSION_VALID( pMac, sessionId ) && (csrIsConnStateConnectedInfraAp(pMac, sessionId)) )
        {
            fRc = eANI_BOOLEAN_TRUE;
            break;
        }
    }

    return ( fRc );

}

tANI_BOOLEAN csrIsBTAMP( tpAniSirGlobal pMac, tANI_U32 sessionId )
{
    return ( csrIsConnStateConnectedWds( pMac, sessionId ) );
}


tANI_BOOLEAN csrIsConnStateDisconnected(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    return (eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED == pMac->roam.roamSession[sessionId].connectState);
}

tANI_BOOLEAN csrIsValidMcConcurrentSession(tpAniSirGlobal pMac, tANI_U32 sessionId,
                                                  tSirBssDescription *pBssDesc)
{
    tCsrRoamSession *pSession = NULL;
    eAniBoolean status = eANI_BOOLEAN_FALSE;

    //Check for MCC support
    if (!pMac->roam.configParam.fenableMCCMode)
    {
        return status;
    }

    //Validate BeaconInterval
    if( CSR_IS_SESSION_VALID( pMac, sessionId ) )
    {
        pSession = CSR_GET_SESSION( pMac, sessionId );
        if (NULL != pSession->pCurRoamProfile)
        {
            if (csrIsconcurrentsessionValid (pMac, sessionId,
                                       pSession->pCurRoamProfile->csrPersona)
                                       == eHAL_STATUS_SUCCESS )
            {
                if (csrValidateMCCBeaconInterval( pMac, pBssDesc->channelId,
                               &pBssDesc->beaconInterval, sessionId,
                               pSession->pCurRoamProfile->csrPersona)
                               != eHAL_STATUS_SUCCESS)
                {
                    status = eANI_BOOLEAN_FALSE;
                }
                else
                {
                    status = eANI_BOOLEAN_TRUE;
                }
            }
            else
            {
                status = eANI_BOOLEAN_FALSE;
            }
         }
     }
    return status;
}

static tSirMacCapabilityInfo csrGetBssCapabilities( tSirBssDescription *pSirBssDesc )
{
    tSirMacCapabilityInfo dot11Caps;

    //tSirMacCapabilityInfo is 16-bit
    pal_get_U16( (tANI_U8 *)&pSirBssDesc->capabilityInfo, (tANI_U16 *)&dot11Caps );

    return( dot11Caps );
}

tANI_BOOLEAN csrIsInfraBssDesc( tSirBssDescription *pSirBssDesc )
{
    tSirMacCapabilityInfo dot11Caps = csrGetBssCapabilities( pSirBssDesc );

    return( (tANI_BOOLEAN)dot11Caps.ess );
}


tANI_BOOLEAN csrIsIbssBssDesc( tSirBssDescription *pSirBssDesc )
{
    tSirMacCapabilityInfo dot11Caps = csrGetBssCapabilities( pSirBssDesc );

    return( (tANI_BOOLEAN)dot11Caps.ibss );
}

tANI_BOOLEAN csrIsQoSBssDesc( tSirBssDescription *pSirBssDesc )
{
    tSirMacCapabilityInfo dot11Caps = csrGetBssCapabilities( pSirBssDesc );

    return( (tANI_BOOLEAN)dot11Caps.qos );
}

tANI_BOOLEAN csrIsPrivacy( tSirBssDescription *pSirBssDesc )
{
    tSirMacCapabilityInfo dot11Caps = csrGetBssCapabilities( pSirBssDesc );

    return( (tANI_BOOLEAN)dot11Caps.privacy );
}


tANI_BOOLEAN csrIs11dSupported(tpAniSirGlobal pMac)
{
    return(pMac->roam.configParam.Is11dSupportEnabled);
}


tANI_BOOLEAN csrIs11hSupported(tpAniSirGlobal pMac)
{
    return(pMac->roam.configParam.Is11hSupportEnabled);
}


tANI_BOOLEAN csrIs11eSupported(tpAniSirGlobal pMac)
{
    return(pMac->roam.configParam.Is11eSupportEnabled);
}

tANI_BOOLEAN csrIsMCCSupported ( tpAniSirGlobal pMac )
{
   return(pMac->roam.configParam.fenableMCCMode);

}

tANI_BOOLEAN csrIsWmmSupported(tpAniSirGlobal pMac)
{
    if(eCsrRoamWmmNoQos == pMac->roam.configParam.WMMSupportMode)
    {
       return eANI_BOOLEAN_FALSE;
    }
    else
    {
       return eANI_BOOLEAN_TRUE;
    }
}




//pIes is the IEs for pSirBssDesc2
tANI_BOOLEAN csrIsSsidEqual( tHalHandle hHal, tSirBssDescription *pSirBssDesc1, 
                             tSirBssDescription *pSirBssDesc2, tDot11fBeaconIEs *pIes2 )
{
    tANI_BOOLEAN fEqual = FALSE;
    tSirMacSSid Ssid1, Ssid2;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tDot11fBeaconIEs *pIes1 = NULL;
    tDot11fBeaconIEs *pIesLocal = pIes2;

    do {
        if( ( NULL == pSirBssDesc1 ) || ( NULL == pSirBssDesc2 ) ) break;
        if( !pIesLocal && !HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pSirBssDesc2, &pIesLocal)) )
        {
            smsLog(pMac, LOGE, FL("  fail to parse IEs"));
            break;
        }
        if(!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pSirBssDesc1, &pIes1)))
        {
            break;
        }
        if( ( !pIes1->SSID.present ) || ( !pIesLocal->SSID.present ) ) break;
        if ( pIes1->SSID.num_ssid != pIesLocal->SSID.num_ssid ) break;
        vos_mem_copy(Ssid1.ssId, pIes1->SSID.ssid, pIes1->SSID.num_ssid);
        vos_mem_copy(Ssid2.ssId, pIesLocal->SSID.ssid, pIesLocal->SSID.num_ssid);

        fEqual = vos_mem_compare(Ssid1.ssId, Ssid2.ssId, pIesLocal->SSID.num_ssid);

    } while( 0 );
    if(pIes1)
    {
        vos_mem_free(pIes1);
    }
    if( pIesLocal && !pIes2 )
    {
        vos_mem_free(pIesLocal);
    }

    return( fEqual );
}

tANI_BOOLEAN csrIsAniWmeSupported(tDot11fIEAirgo *pIeAirgo)
{
    tANI_BOOLEAN fRet = eANI_BOOLEAN_FALSE;

    if(pIeAirgo && pIeAirgo->present && pIeAirgo->PropCapability.present)
    {
        fRet = (tANI_BOOLEAN)(PROP_CAPABILITY_GET( WME, pIeAirgo->PropCapability.capability ));
    }

    return fRet;
}




//pIes can be passed in as NULL if the caller doesn't have one prepared
tANI_BOOLEAN csrIsBssDescriptionWme( tHalHandle hHal, tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    // Assume that WME is found...
    tANI_BOOLEAN fWme = TRUE;
    tDot11fBeaconIEs *pIesTemp = pIes;

    do
    {
        if(pIesTemp == NULL)
        {
            if( !HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pSirBssDesc, &pIesTemp)) )
            {
                fWme = FALSE;
                break;
            }
        }
        // if the AirgoProprietary indicator is found, then WME is supported...
        if ( csrIsAniWmeSupported(&pIesTemp->Airgo) ) break;
        // if the Wme Info IE is found, then WME is supported...
        if ( CSR_IS_QOS_BSS(pIesTemp) ) break;
        // if none of these are found, then WME is NOT supported...
        fWme = FALSE;
    } while( 0 );
    if( !csrIsWmmSupported( pMac ) && fWme)
    {
        if( !pIesTemp->HTCaps.present )
        {
            fWme = FALSE;
        }
    }
    if( ( pIes == NULL ) && ( NULL != pIesTemp ) )
    {
        //we allocate memory here so free it before returning
        vos_mem_free(pIesTemp);
    }

    return( fWme );
}

tANI_BOOLEAN csrIsHcfEnabled( tDot11fIEAirgo *pIeAirgo )
{
    tANI_BOOLEAN fHcfSupported = FALSE;

    fHcfSupported = ((tANI_BOOLEAN)(PROP_CAPABILITY_GET( WME, pIeAirgo->PropCapability.capability )) ||
        (pIeAirgo->present && pIeAirgo->HCF.present && pIeAirgo->HCF.enabled));

    return( fHcfSupported );
}


eCsrMediaAccessType csrGetQoSFromBssDesc( tHalHandle hHal, tSirBssDescription *pSirBssDesc, 
                                          tDot11fBeaconIEs *pIes )
{
    eCsrMediaAccessType qosType = eCSR_MEDIUM_ACCESS_DCF;

    VOS_ASSERT( pIes != NULL );

    do
   {
        // if we find WMM in the Bss Description, then we let this
        // override and use WMM.
        if ( csrIsBssDescriptionWme( hHal, pSirBssDesc, pIes ) )
        {
            qosType = eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP;
        }
        else
        {
            // if the QoS bit is on, then the AP is advertising 11E QoS...
            if ( csrIsQoSBssDesc( pSirBssDesc ) )
            {
                // which could be HCF or eDCF.
                    if ( csrIsHcfEnabled( &pIes->Airgo ) )
                {
                    qosType = eCSR_MEDIUM_ACCESS_11e_HCF;
                }
                else
                {
                    qosType = eCSR_MEDIUM_ACCESS_11e_eDCF;
                }
            }
            else
            {
                qosType = eCSR_MEDIUM_ACCESS_DCF;
            }
            // scale back based on the types turned on for the adapter...
            if ( eCSR_MEDIUM_ACCESS_11e_eDCF == qosType && !csrIs11eSupported( hHal ) )
            {
                qosType = eCSR_MEDIUM_ACCESS_DCF;
            }
        }

    } while(0);

    return( qosType );
}




//Caller allocates memory for pIEStruct
eHalStatus csrParseBssDescriptionIEs(tHalHandle hHal, tSirBssDescription *pBssDesc, tDot11fBeaconIEs *pIEStruct)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    int ieLen = (int)(pBssDesc->length + sizeof( pBssDesc->length ) - GET_FIELD_OFFSET( tSirBssDescription, ieFields ));

    if(ieLen > 0 && pIEStruct)
    {
        if(!DOT11F_FAILED(dot11fUnpackBeaconIEs( pMac, (tANI_U8 *)pBssDesc->ieFields, ieLen, pIEStruct )))
        {
            status = eHAL_STATUS_SUCCESS;
        }
    }

    return (status);
}


//This function will allocate memory for the parsed IEs to the caller. Caller must free the memory
//after it is done with the data only if this function succeeds
eHalStatus csrGetParsedBssDescriptionIEs(tHalHandle hHal, tSirBssDescription *pBssDesc, tDot11fBeaconIEs **ppIEStruct)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    if(pBssDesc && ppIEStruct)
    {
        *ppIEStruct = vos_mem_malloc(sizeof(tDot11fBeaconIEs));
        if ( (*ppIEStruct) != NULL)
        {
            vos_mem_set((void *)*ppIEStruct, sizeof(tDot11fBeaconIEs), 0);
            status = csrParseBssDescriptionIEs(hHal, pBssDesc, *ppIEStruct);
            if(!HAL_STATUS_SUCCESS(status))
            {
                vos_mem_free(*ppIEStruct);
                *ppIEStruct = NULL;
            }
        }
        else
        {
            smsLog( pMac, LOGE, FL(" failed to allocate memory") );
            VOS_ASSERT( 0 );
            return eHAL_STATUS_FAILURE;
        }
    }

    return (status);
}




tANI_BOOLEAN csrIsNULLSSID( tANI_U8 *pBssSsid, tANI_U8 len )
{
    tANI_BOOLEAN fNullSsid = FALSE;

    tANI_U32 SsidLength;
    tANI_U8 *pSsidStr;

    do
    {
        if ( 0 == len )
        {
            fNullSsid = TRUE;
            break;
        }

        //Consider 0 or space for hidden SSID
        if ( 0 == pBssSsid[0] )
        {
             fNullSsid = TRUE;
             break;
        }

        SsidLength = len;
        pSsidStr = pBssSsid;

        while ( SsidLength )
        {
            if( *pSsidStr )
                break;

            pSsidStr++;
            SsidLength--;
        }

        if( 0 == SsidLength )
        {
            fNullSsid = TRUE;
            break;
        }
    }
    while( 0 );

    return fNullSsid;
}


tANI_U32 csrGetFragThresh( tHalHandle hHal )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    return pMac->roam.configParam.FragmentationThreshold;
}

tANI_U32 csrGetRTSThresh( tHalHandle hHal )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    return pMac->roam.configParam.RTSThreshold;
}

eCsrPhyMode csrTranslateToPhyModeFromBssDesc( tSirBssDescription *pSirBssDesc )
{
    eCsrPhyMode phyMode;

    switch ( pSirBssDesc->nwType )
    {
        case eSIR_11A_NW_TYPE:
            phyMode = eCSR_DOT11_MODE_11a;
            break;

        case eSIR_11B_NW_TYPE:
            phyMode = eCSR_DOT11_MODE_11b;
            break;

        case eSIR_11G_NW_TYPE:
            phyMode = eCSR_DOT11_MODE_11g;
            break;

        case eSIR_11N_NW_TYPE:
            phyMode = eCSR_DOT11_MODE_11n;
            break;
#ifdef WLAN_FEATURE_11AC
        case eSIR_11AC_NW_TYPE:
        default:
            phyMode = eCSR_DOT11_MODE_11ac;
#else
        default:
            phyMode = eCSR_DOT11_MODE_11n;
#endif
            break;
    }
    return( phyMode );
}


tANI_U32 csrTranslateToWNICfgDot11Mode(tpAniSirGlobal pMac, eCsrCfgDot11Mode csrDot11Mode)
{
    tANI_U32 ret;

    switch(csrDot11Mode)
    {
    case eCSR_CFG_DOT11_MODE_AUTO:
        smsLog(pMac, LOGW, FL("  Warning: sees eCSR_CFG_DOT11_MODE_AUTO "));
        //We cannot decide until now.
        if(pMac->roam.configParam.ProprietaryRatesEnabled)
        {
            ret = WNI_CFG_DOT11_MODE_TAURUS;
        }
        else
        {
            ret = WNI_CFG_DOT11_MODE_11AC;
        }
        break;
    case eCSR_CFG_DOT11_MODE_TAURUS:
        ret = WNI_CFG_DOT11_MODE_TAURUS;
        break;
    case eCSR_CFG_DOT11_MODE_11A:
        ret = WNI_CFG_DOT11_MODE_11A;
        break;
    case eCSR_CFG_DOT11_MODE_11B:
        ret = WNI_CFG_DOT11_MODE_11B;
        break;
    case eCSR_CFG_DOT11_MODE_11G:
        ret = WNI_CFG_DOT11_MODE_11G;
        break;
    case eCSR_CFG_DOT11_MODE_11N:
        ret = WNI_CFG_DOT11_MODE_11N;
        break;
    case eCSR_CFG_DOT11_MODE_POLARIS:
        ret = WNI_CFG_DOT11_MODE_POLARIS;
        break;
    case eCSR_CFG_DOT11_MODE_TITAN:
        ret = WNI_CFG_DOT11_MODE_TITAN;
        break;
    case eCSR_CFG_DOT11_MODE_11G_ONLY:
       ret = WNI_CFG_DOT11_MODE_11G_ONLY;
       break;
    case eCSR_CFG_DOT11_MODE_11N_ONLY:
       ret = WNI_CFG_DOT11_MODE_11N_ONLY;
       break;

#ifdef WLAN_FEATURE_11AC
     case eCSR_CFG_DOT11_MODE_11AC_ONLY:
        ret = WNI_CFG_DOT11_MODE_11AC_ONLY;
        break;
     case eCSR_CFG_DOT11_MODE_11AC:
        ret = WNI_CFG_DOT11_MODE_11AC;
       break;
#endif
    default:
        smsLog(pMac, LOGW, FL("doesn't expect %d as csrDo11Mode"), csrDot11Mode);
        if(eCSR_BAND_24 == pMac->roam.configParam.eBand)
        {
            ret = WNI_CFG_DOT11_MODE_11G;
        }
        else
        {
            ret = WNI_CFG_DOT11_MODE_11A;
        }
        break;
    }

    return (ret);
}


//This function should only return the super set of supported modes. 11n implies 11b/g/a/n.
eHalStatus csrGetPhyModeFromBss(tpAniSirGlobal pMac, tSirBssDescription *pBSSDescription, 
                                eCsrPhyMode *pPhyMode, tDot11fBeaconIEs *pIes)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    eCsrPhyMode phyMode = csrTranslateToPhyModeFromBssDesc(pBSSDescription);

    if( pIes )
    {
        if(pIes->Airgo.present)
        {
            if(pIes->Airgo.PropCapability.present)
            {
                if( PROP_CAPABILITY_GET( TAURUS, pIes->Airgo.PropCapability.capability ))
                {
                    phyMode = eCSR_DOT11_MODE_TAURUS;
                }
                }
                }
        if(pIes->HTCaps.present && (eCSR_DOT11_MODE_TAURUS != phyMode))
        {
            phyMode = eCSR_DOT11_MODE_11n;
        }

#ifdef WLAN_FEATURE_11AC
        if ( pIes->VHTCaps.present && (eCSR_DOT11_MODE_TAURUS != phyMode))
        {
             phyMode = eCSR_DOT11_MODE_11ac;
        }
#endif
        *pPhyMode = phyMode;
    }

    return (status);

}


//This function returns the correct eCSR_CFG_DOT11_MODE is the two phyModes matches
//bssPhyMode is the mode derived from the BSS description
//f5GhzBand is derived from the channel id of BSS description
tANI_BOOLEAN csrGetPhyModeInUse( eCsrPhyMode phyModeIn, eCsrPhyMode bssPhyMode, tANI_BOOLEAN f5GhzBand,
                                 eCsrCfgDot11Mode *pCfgDot11ModeToUse )
{
    tANI_BOOLEAN fMatch = FALSE;
    eCsrCfgDot11Mode cfgDot11Mode;

    cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N; // to suppress compiler warning

    switch( phyModeIn )
    {
        case eCSR_DOT11_MODE_abg:   //11a or 11b or 11g
            if( f5GhzBand )
            {
                fMatch = TRUE;
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
            }
            else if( eCSR_DOT11_MODE_11b == bssPhyMode )
            {
                fMatch = TRUE;
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
            }
            else
            {
                fMatch = TRUE;
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11G;
            }
            break;

        case eCSR_DOT11_MODE_11a:   //11a
            if( f5GhzBand )
            {
                fMatch = TRUE;
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
            }
            break;

        case eCSR_DOT11_MODE_11a_ONLY:   //11a
            if( eCSR_DOT11_MODE_11a == bssPhyMode )
            {
                fMatch = TRUE;
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
            }
            break;

        case eCSR_DOT11_MODE_11g:
            if(!f5GhzBand)
            {
                if( eCSR_DOT11_MODE_11b == bssPhyMode )
                {
                    fMatch = TRUE;
                    cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
                }
                else
                {
                    fMatch = TRUE;
                    cfgDot11Mode = eCSR_CFG_DOT11_MODE_11G;
                }
            }
            break;

        case eCSR_DOT11_MODE_11g_ONLY:
            if( eCSR_DOT11_MODE_11g == bssPhyMode )
            {
                fMatch = TRUE;
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11G;
            }
            break;

        case eCSR_DOT11_MODE_11b:
            if( !f5GhzBand )
            {
                fMatch = TRUE;
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
            }
            break;

        case eCSR_DOT11_MODE_11b_ONLY:
            if( eCSR_DOT11_MODE_11b == bssPhyMode )
            {
                fMatch = TRUE;
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
            }
            break;

        case eCSR_DOT11_MODE_11n:
            fMatch = TRUE;
            switch(bssPhyMode)
            {
            case eCSR_DOT11_MODE_11g:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11G;
                break;
            case eCSR_DOT11_MODE_11b:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
                break;
            case eCSR_DOT11_MODE_11a:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
                break;
            case eCSR_DOT11_MODE_11n:
#ifdef WLAN_FEATURE_11AC
            case eCSR_DOT11_MODE_11ac:
#endif
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
                break;

            case eCSR_DOT11_MODE_TAURUS:
            default:
#ifdef WLAN_FEATURE_11AC
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11AC;
#else
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
#endif
                break;
            }
            break;

        case eCSR_DOT11_MODE_11n_ONLY:
            if((eCSR_DOT11_MODE_11n == bssPhyMode) || (eCSR_DOT11_MODE_TAURUS == bssPhyMode))
            {
                fMatch = TRUE;
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;

            }

            break;
#ifdef WLAN_FEATURE_11AC
        case eCSR_DOT11_MODE_11ac:
            fMatch = TRUE;
            switch(bssPhyMode)
            {
            case eCSR_DOT11_MODE_11g:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11G;
                break;
            case eCSR_DOT11_MODE_11b:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
                break;
            case eCSR_DOT11_MODE_11a:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
                break;
            case eCSR_DOT11_MODE_11n:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
                break;
            case eCSR_DOT11_MODE_11ac:
            case eCSR_DOT11_MODE_TAURUS:
            default:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11AC;
                break;
            }
            break;

        case eCSR_DOT11_MODE_11ac_ONLY:
            if((eCSR_DOT11_MODE_11ac == bssPhyMode) || (eCSR_DOT11_MODE_TAURUS == bssPhyMode))
            {
                fMatch = TRUE;
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11AC;
            }
            break;
#endif

        case eCSR_DOT11_MODE_TAURUS:
        default:
            fMatch = TRUE;
            switch(bssPhyMode)
            {
            case eCSR_DOT11_MODE_11g:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11G;
                break;
            case eCSR_DOT11_MODE_11b:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
                break;
            case eCSR_DOT11_MODE_11a:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
                break;
            case eCSR_DOT11_MODE_11n:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
                break;
#ifdef WLAN_FEATURE_11AC
            case eCSR_DOT11_MODE_11ac:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_11AC;
                break;
#endif
            case eCSR_DOT11_MODE_TAURUS:
            default:
                cfgDot11Mode = eCSR_CFG_DOT11_MODE_TAURUS;
                break;
            }
            break;
    }

    if ( fMatch && pCfgDot11ModeToUse )
    {
#ifdef WLAN_FEATURE_11AC
        if(cfgDot11Mode == eCSR_CFG_DOT11_MODE_11AC && (!IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)))
        {
            *pCfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_11N;
        }
        else
#endif
        {
            *pCfgDot11ModeToUse = cfgDot11Mode;
        }
    }
    return( fMatch );
}


//This function decides whether the one of the bit of phyMode is matching the mode in the BSS and allowed by the user
//setting, pMac->roam.configParam.uCfgDot11Mode. It returns the mode that fits the criteria.
tANI_BOOLEAN csrIsPhyModeMatch( tpAniSirGlobal pMac, tANI_U32 phyMode,
                                tSirBssDescription *pSirBssDesc, tCsrRoamProfile *pProfile,
                                eCsrCfgDot11Mode *pReturnCfgDot11Mode,
                                tDot11fBeaconIEs *pIes)
{
    tANI_BOOLEAN fMatch = FALSE;
    eCsrPhyMode phyModeInBssDesc, phyMode2;
    eCsrCfgDot11Mode cfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_TAURUS;
    tANI_U32 bitMask, loopCount;

    if(HAL_STATUS_SUCCESS(csrGetPhyModeFromBss(pMac, pSirBssDesc, &phyModeInBssDesc, pIes )))
    {
        //In case some change change eCSR_DOT11_MODE_TAURUS to non-0
        if ( (0 == phyMode) || (eCSR_DOT11_MODE_AUTO & phyMode) || (eCSR_DOT11_MODE_TAURUS & phyMode))
        {
            //Taurus means anything
            if ( eCSR_CFG_DOT11_MODE_ABG == pMac->roam.configParam.uCfgDot11Mode )
            {
                phyMode = eCSR_DOT11_MODE_abg;
            }
            else if(eCSR_CFG_DOT11_MODE_AUTO == pMac->roam.configParam.uCfgDot11Mode)
            {
                if(pMac->roam.configParam.ProprietaryRatesEnabled)
                {
                    phyMode = eCSR_DOT11_MODE_TAURUS;
                }
                else
                {

#ifdef WLAN_FEATURE_11AC
                    phyMode = eCSR_DOT11_MODE_11ac;
#else
                    phyMode = eCSR_DOT11_MODE_11n;
#endif

                }
            }
            else
            {
                //user's pick
                phyMode = pMac->roam.configParam.phyMode;
            }
        }
        if ( (0 == phyMode) || (eCSR_DOT11_MODE_AUTO & phyMode) || (eCSR_DOT11_MODE_TAURUS & phyMode) )
        {
            if(0 != phyMode)
            {
                if(eCSR_DOT11_MODE_AUTO & phyMode)
                {
                    phyMode2 = eCSR_DOT11_MODE_AUTO & phyMode;
                }
                else
                {
                    phyMode2 = eCSR_DOT11_MODE_TAURUS & phyMode;
                }
            }
            else
            {
                phyMode2 = phyMode;
            }
            fMatch = csrGetPhyModeInUse( phyMode2, phyModeInBssDesc, CSR_IS_CHANNEL_5GHZ(pSirBssDesc->channelId),
                                                &cfgDot11ModeToUse );
        }
        else
        {
            bitMask = 1;
            loopCount = 0;
            while(loopCount < eCSR_NUM_PHY_MODE)   
            {
                if(0 != ( phyMode2 = (phyMode & (bitMask << loopCount++)) ))
                {
                    fMatch = csrGetPhyModeInUse( phyMode2, phyModeInBssDesc, CSR_IS_CHANNEL_5GHZ(pSirBssDesc->channelId),
                                        &cfgDot11ModeToUse );
                    if(fMatch) break;
                }
            }
        }
        if ( fMatch && pReturnCfgDot11Mode )
        {
            if( pProfile )
            {
                /* IEEE 11n spec (8.4.3): HT STA shall eliminate TKIP as a 
                 * choice for the pairwise cipher suite if CCMP is advertised 
                 * by the AP or if the AP included an HT capabilities element 
                 * in its Beacons and Probe Response.
                 */
                if( (!CSR_IS_11n_ALLOWED( pProfile->negotiatedUCEncryptionType )) &&
                    ((eCSR_CFG_DOT11_MODE_11N == cfgDot11ModeToUse) ||
#ifdef WLAN_FEATURE_11AC
                     (eCSR_CFG_DOT11_MODE_11AC == cfgDot11ModeToUse) ||
#endif
                     (eCSR_CFG_DOT11_MODE_TAURUS == cfgDot11ModeToUse)) )
                {
                    //We cannot do 11n here
                    if( !CSR_IS_CHANNEL_5GHZ(pSirBssDesc->channelId) )
                    {
                        cfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_11G;
                    }
                    else
                    {
                        cfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_11A;
                    }
                }
            }
            *pReturnCfgDot11Mode = cfgDot11ModeToUse;
        }
    }

    return( fMatch );
}


eCsrCfgDot11Mode csrFindBestPhyMode( tpAniSirGlobal pMac, tANI_U32 phyMode )
{
    eCsrCfgDot11Mode cfgDot11ModeToUse;
    eCsrBand eBand = pMac->roam.configParam.eBand;


    if ((0 == phyMode) ||
#ifdef WLAN_FEATURE_11AC
        (eCSR_DOT11_MODE_11ac & phyMode) ||
#endif
        (eCSR_DOT11_MODE_AUTO & phyMode))
    {
#ifdef WLAN_FEATURE_11AC
        if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC))
        {
           cfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_11AC;
        }
        else
#endif
        {
           /* Default to 11N mode if user has configured 11ac mode
            * and FW doesn't supports 11ac mode .
            */
           cfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_11N;
        }
    }
    else
    {
        if( ( eCSR_DOT11_MODE_11n | eCSR_DOT11_MODE_11n_ONLY ) & phyMode )
        {
            cfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_11N;
        }
        else if ( eCSR_DOT11_MODE_abg & phyMode )
        {
            if( eCSR_BAND_24 != eBand )
            {
                cfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_11A;
            }
            else
            {
                cfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_11G;
            }
        }
        else if( ( eCSR_DOT11_MODE_11a | eCSR_DOT11_MODE_11a_ONLY ) & phyMode )
        {
            cfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_11A;
        }
        else if( ( eCSR_DOT11_MODE_11g | eCSR_DOT11_MODE_11g_ONLY ) & phyMode )
        {
            cfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_11G;
        }
        else
        {
            cfgDot11ModeToUse = eCSR_CFG_DOT11_MODE_11B;
        }
    }

    return ( cfgDot11ModeToUse );
}




tANI_U32 csrGet11hPowerConstraint( tHalHandle hHal, tDot11fIEPowerConstraints *pPowerConstraint )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_U32 localPowerConstraint = 0;

    // check if .11h support is enabled, if not, the power constraint is 0.
    if(pMac->roam.configParam.Is11hSupportEnabled && pPowerConstraint->present)
    {
        localPowerConstraint = pPowerConstraint->localPowerConstraints;
    }

    return( localPowerConstraint );
}


tANI_BOOLEAN csrIsProfileWpa( tCsrRoamProfile *pProfile )
{
    tANI_BOOLEAN fWpaProfile = FALSE;

    switch ( pProfile->negotiatedAuthType )
    {
        case eCSR_AUTH_TYPE_WPA:
        case eCSR_AUTH_TYPE_WPA_PSK:
        case eCSR_AUTH_TYPE_WPA_NONE:
#ifdef FEATURE_WLAN_ESE
        case eCSR_AUTH_TYPE_CCKM_WPA:
#endif
            fWpaProfile = TRUE;
            break;

        default:
            fWpaProfile = FALSE;
            break;
    }

    if ( fWpaProfile )
    {
        switch ( pProfile->negotiatedUCEncryptionType )
        {
            case eCSR_ENCRYPT_TYPE_WEP40:
            case eCSR_ENCRYPT_TYPE_WEP104:
            case eCSR_ENCRYPT_TYPE_TKIP:
            case eCSR_ENCRYPT_TYPE_AES:
                fWpaProfile = TRUE;
                break;

            default:
                fWpaProfile = FALSE;
                break;
        }
    }
    return( fWpaProfile );
}

tANI_BOOLEAN csrIsProfileRSN( tCsrRoamProfile *pProfile )
{
    tANI_BOOLEAN fRSNProfile = FALSE;

    switch ( pProfile->negotiatedAuthType )
    {
        case eCSR_AUTH_TYPE_RSN:
        case eCSR_AUTH_TYPE_RSN_PSK:
#ifdef WLAN_FEATURE_VOWIFI_11R
        case eCSR_AUTH_TYPE_FT_RSN:
        case eCSR_AUTH_TYPE_FT_RSN_PSK:
#endif 
#ifdef FEATURE_WLAN_ESE
        case eCSR_AUTH_TYPE_CCKM_RSN:
#endif 
#ifdef WLAN_FEATURE_11W
        case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
        case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
#endif
            fRSNProfile = TRUE;
            break;

        default:
            fRSNProfile = FALSE;
            break;
    }

    if ( fRSNProfile )
    {
        switch ( pProfile->negotiatedUCEncryptionType )
        {
            // !!REVIEW - For WPA2, use of RSN IE mandates
            // use of AES as encryption. Here, we qualify
            // even if encryption type is WEP or TKIP
            case eCSR_ENCRYPT_TYPE_WEP40:
            case eCSR_ENCRYPT_TYPE_WEP104:
            case eCSR_ENCRYPT_TYPE_TKIP:
            case eCSR_ENCRYPT_TYPE_AES:
                fRSNProfile = TRUE;
                break;

            default:
                fRSNProfile = FALSE;
                break;
        }
    }
    return( fRSNProfile );
}

eHalStatus
csrIsconcurrentsessionValid(tpAniSirGlobal pMac,tANI_U32 cursessionId,
                                 tVOS_CON_MODE currBssPersona)
{
    tANI_U32 sessionId = 0;

    for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++ )
    {
        if (cursessionId != sessionId )
        {
            if (!CSR_IS_SESSION_VALID( pMac, sessionId ))
            {
                continue;
            }

            switch (currBssPersona)
            {
                case VOS_STA_MODE:
                    {
                        smsLog(pMac, LOG4, FL(" Second session for persona %d"), currBssPersona);
                        return eHAL_STATUS_SUCCESS;
                    }
                    break;

                case VOS_STA_SAP_MODE:
                    if((pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_STA_SAP_MODE)&&
                       (pMac->roam.roamSession[sessionId].connectState
                                      != eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED))
                    {
                        smsLog(pMac, LOGE, FL(" ****SoftAP mode already exists ****"));
                        return eHAL_STATUS_FAILURE;
                    }
                    else if( (pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_P2P_GO_MODE &&
                              pMac->roam.roamSession[sessionId].connectState
                                      != eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED) ||
                             (pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_IBSS_MODE &&
                              pMac->roam.roamSession[sessionId].connectState
                                      != eCSR_ASSOC_STATE_TYPE_IBSS_DISCONNECTED))
                    {
                        smsLog(pMac, LOGE, FL(" ****Cannot start Multiple Beaconing Role ****"));
                        return eHAL_STATUS_FAILURE;
                    }
                    break;

                case VOS_P2P_CLIENT_MODE:
                    if(pMac->roam.roamSession[sessionId].pCurRoamProfile &&
                      (pMac->roam.roamSession[sessionId].pCurRoamProfile->csrPersona
                                                  == VOS_P2P_CLIENT_MODE)) //check for P2P client mode
                    {
                        smsLog(pMac, LOGE, FL(" ****CLIENT mode already exists ****"));
                        return eHAL_STATUS_FAILURE;
                    }
                    break;

                case VOS_P2P_GO_MODE:
                    if((pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_P2P_GO_MODE) &&
                       (pMac->roam.roamSession[sessionId].connectState
                                      != eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED))
                    {
                        smsLog(pMac, LOGE, FL(" ****P2P GO mode already exists ****"));
                        return eHAL_STATUS_FAILURE;
                    }
                    else if( (pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_STA_SAP_MODE &&
                              pMac->roam.roamSession[sessionId].connectState
                                      != eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED) ||
                             (pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_IBSS_MODE &&
                              pMac->roam.roamSession[sessionId].connectState
                                      != eCSR_ASSOC_STATE_TYPE_IBSS_DISCONNECTED) )
                    {
                        smsLog(pMac, LOGE, FL(" ****Cannot start Multiple Beaconing Role ****"));
                        return eHAL_STATUS_FAILURE;
                    }
                    break;
                case VOS_IBSS_MODE:
                    if((pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_IBSS_MODE) &&
                       (pMac->roam.roamSession[sessionId].connectState
                                      != eCSR_ASSOC_STATE_TYPE_IBSS_CONNECTED))
                    {
                        smsLog(pMac, LOGE, FL(" ****IBSS mode already exists ****"));
                        return eHAL_STATUS_FAILURE;
                    }
                    else if( (pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_P2P_GO_MODE ||
                              pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_STA_SAP_MODE) &&
                              pMac->roam.roamSession[sessionId].connectState
                                     != eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED)
                    {
                        smsLog(pMac, LOGE, FL(" ****Cannot start Multiple Beaconing Role ****"));
                        return eHAL_STATUS_FAILURE;
                    }
                    break;
                default :
                    smsLog(pMac, LOGE, FL("***Persona not handled = %d*****"),currBssPersona);
                    break;
            }
        }
    }
    return eHAL_STATUS_SUCCESS;

}

eHalStatus csrUpdateMCCp2pBeaconInterval(tpAniSirGlobal pMac)
{
    tANI_U32 sessionId = 0;

    //If MCC is not supported just break and return SUCCESS
    if ( !pMac->roam.configParam.fenableMCCMode){
        return eHAL_STATUS_FAILURE;
    }

    for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++ )
    {
        /* If GO in MCC support different beacon interval, 
         * change the BI of the P2P-GO */
        if (pMac->roam.roamSession[sessionId].bssParams.bssPersona
                              == VOS_P2P_GO_MODE)
        {
           /* Handle different BI scneario based on the configuration set.
            * If Config is set to 0x02 then Disconnect all the P2P clients
            * associated. If config is set to 0x04 then update the BI
            * without disconnecting all the clients
            */
           if ((pMac->roam.configParam.fAllowMCCGODiffBI == 0x04) &&
               (pMac->roam.roamSession[sessionId].bssParams.updatebeaconInterval))
           {
               return csrSendChngMCCBeaconInterval( pMac, sessionId);
           }
           //If the configuration of fAllowMCCGODiffBI is set to other than 0x04
           else if ( pMac->roam.roamSession[sessionId].bssParams.updatebeaconInterval)
           {
               return csrRoamCallCallback(pMac, sessionId, NULL, 0, eCSR_ROAM_DISCONNECT_ALL_P2P_CLIENTS, eCSR_ROAM_RESULT_NONE);
           }
        }
    }
    return eHAL_STATUS_FAILURE;
}

tANI_U16 csrCalculateMCCBeaconInterval(tpAniSirGlobal pMac, tANI_U16 sta_bi, tANI_U16 go_gbi)
{
    tANI_U8 num_beacons = 0;
    tANI_U8 is_multiple = 0;
    tANI_U16 go_cbi = 0;
    tANI_U16 go_fbi = 0;
    tANI_U16 sta_cbi = 0;

    //If GO's given beacon Interval is less than 100 
    if(go_gbi < 100)
       go_cbi = 100;
    //if GO's given beacon Interval is greater than or equal to 100
    else
       go_cbi = 100 + (go_gbi % 100);

      if ( sta_bi == 0 )
    {
        /* There is possibility to receive zero as value.
           Which will cause divide by zero. Hence initialise with 100
        */
        sta_bi =  100;
        smsLog(pMac, LOGW,
            FL("sta_bi 2nd parameter is zero, initialise to %d"), sta_bi);
    }

    // check, if either one is multiple of another
    if (sta_bi > go_cbi)
    {
        is_multiple = !(sta_bi % go_cbi);
    }
    else
    {
        is_multiple = !(go_cbi % sta_bi);
    }
    // if it is multiple, then accept GO's beacon interval range [100,199] as it  is
    if (is_multiple)
    {
        return go_cbi;
    }
    //else , if it is not multiple, then then check for number of beacons to be 
    //inserted based on sta BI
    num_beacons = sta_bi / 100;
    if (num_beacons)
    { 
        // GO's final beacon interval will be aligned to sta beacon interval, but 
        //in the range of [100, 199].
        sta_cbi = sta_bi / num_beacons;
        go_fbi = sta_cbi;
    }
    else
    {
        // if STA beacon interval is less than 100, use GO's change bacon interval 
        //instead of updating to STA's beacon interval.
        go_fbi = go_cbi;
    }
    return go_fbi;
}

eHalStatus csrValidateMCCBeaconInterval(tpAniSirGlobal pMac, tANI_U8 channelId,
                                     tANI_U16 *beaconInterval, tANI_U32 cursessionId,
                                     tVOS_CON_MODE currBssPersona)
{
    tANI_U32 sessionId = 0;
    tANI_U16 new_beaconInterval = 0;
  
    //If MCC is not supported just break
    if (!pMac->roam.configParam.fenableMCCMode){
        return eHAL_STATUS_FAILURE;
    }

    for (sessionId = 0; sessionId < CSR_ROAM_SESSION_MAX; sessionId++ )
    {
        if (cursessionId != sessionId )
        {
            if (!CSR_IS_SESSION_VALID( pMac, sessionId ))
            {
                continue;
            }

            switch (currBssPersona)
            {
                case VOS_STA_MODE:
                    if (pMac->roam.roamSession[sessionId].pCurRoamProfile &&
                       (pMac->roam.roamSession[sessionId].pCurRoamProfile->csrPersona
                                      == VOS_P2P_CLIENT_MODE)) //check for P2P client mode
                    {
                        smsLog(pMac, LOG1, FL(" Beacon Interval Validation not required for STA/CLIENT"));
                    }
                    //IF SAP has started and STA wants to connect on different channel MCC should
                    //MCC should not be enabled so making it false to enforce on same channel
                    else if (pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_STA_SAP_MODE)
                    {
                        if (pMac->roam.roamSession[sessionId].bssParams.operationChn 
                                                        != channelId )
                        {
                            smsLog(pMac, LOGE, FL("*** MCC with SAP+STA sessions ****"));
                            return eHAL_STATUS_SUCCESS;
                        }
                    }
                    else if (pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_P2P_GO_MODE) //Check for P2P go scenario
                    {
                        /* if GO in MCC support different beacon interval, 
                         * change the BI of the P2P-GO */
                       if ((pMac->roam.roamSession[sessionId].bssParams.operationChn 
                                != channelId ) &&
                           (pMac->roam.roamSession[sessionId].bssParams.beaconInterval 
                                != *beaconInterval))
                       {
                           /* if GO in MCC support different beacon interval, return success */
                           if ( pMac->roam.configParam.fAllowMCCGODiffBI == 0x01)
                           {
                               return eHAL_STATUS_SUCCESS;
                           }
                           // Send only Broadcast disassoc and update beaconInterval
                           //If configuration is set to 0x04 then dont
                           // disconnect all the station
                           else if ((pMac->roam.configParam.fAllowMCCGODiffBI == 0x02) ||
                                   (pMac->roam.configParam.fAllowMCCGODiffBI == 0x04))
                           {
                               //Check to pass the right beacon Interval
                               new_beaconInterval = csrCalculateMCCBeaconInterval(pMac, *beaconInterval, 
                                                         pMac->roam.roamSession[sessionId].bssParams.beaconInterval);
                               smsLog(pMac, LOG1, FL(" Peer AP BI : %d, new Beacon Interval: %d"),*beaconInterval,new_beaconInterval );
                               //Update the becon Interval
                               if (new_beaconInterval != pMac->roam.roamSession[sessionId].bssParams.beaconInterval)
                               {
                                   //Update the beaconInterval now
                                   smsLog(pMac, LOGE, FL(" Beacon Interval got changed config used: %d\n"),
                                                 pMac->roam.configParam.fAllowMCCGODiffBI);

                                   pMac->roam.roamSession[sessionId].bssParams.beaconInterval = new_beaconInterval;
                                   pMac->roam.roamSession[sessionId].bssParams.updatebeaconInterval = eANI_BOOLEAN_TRUE;
                                    return csrUpdateMCCp2pBeaconInterval(pMac);
                               }
                               return eHAL_STATUS_SUCCESS;
                           }
                           //Disconnect the P2P session
                           else if (pMac->roam.configParam.fAllowMCCGODiffBI == 0x03)
                           {
                               pMac->roam.roamSession[sessionId].bssParams.updatebeaconInterval =  eANI_BOOLEAN_FALSE;
                               return csrRoamCallCallback(pMac, sessionId, NULL, 0, eCSR_ROAM_SEND_P2P_STOP_BSS, eCSR_ROAM_RESULT_NONE);
                           }
                           else
                           {
                               smsLog(pMac, LOGE, FL("BeaconInterval is different cannot connect to preferred AP..."));
                               return eHAL_STATUS_FAILURE;
                           }
                        }
                    }
                    break;

                case VOS_P2P_CLIENT_MODE:
                    if (pMac->roam.roamSession[sessionId].pCurRoamProfile &&
                      (pMac->roam.roamSession[sessionId].pCurRoamProfile->csrPersona
                                                                == VOS_STA_MODE)) //check for P2P client mode
                    {
                        smsLog(pMac, LOG1, FL(" Ignore Beacon Interval Validation..."));
                    }
                    //IF SAP has started and STA wants to connect on different channel MCC should
                    //MCC should not be enabled so making it false to enforce on same channel
                    else if (pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                      == VOS_STA_SAP_MODE)
                    {
                        if (pMac->roam.roamSession[sessionId].bssParams.operationChn 
                                                        != channelId )
                        {
                            smsLog(pMac, LOGE, FL("***MCC is not enabled for SAP + CLIENT****"));
                            return eHAL_STATUS_FAILURE;
                        }
                    }
                    else if (pMac->roam.roamSession[sessionId].bssParams.bssPersona
                                    == VOS_P2P_GO_MODE) //Check for P2P go scenario
                    {
                        if ((pMac->roam.roamSession[sessionId].bssParams.operationChn 
                                != channelId ) &&
                            (pMac->roam.roamSession[sessionId].bssParams.beaconInterval 
                                != *beaconInterval))
                        {
                            smsLog(pMac, LOGE, FL("BeaconInterval is different cannot connect to P2P_GO network ..."));
                            return eHAL_STATUS_FAILURE;
                        }
                    }
                    break;

                case VOS_P2P_GO_MODE :
                {
                    if (pMac->roam.roamSession[sessionId].pCurRoamProfile  &&
                      ((pMac->roam.roamSession[sessionId].pCurRoamProfile->csrPersona
                            == VOS_P2P_CLIENT_MODE) ||
                      (pMac->roam.roamSession[sessionId].pCurRoamProfile->csrPersona
                            == VOS_STA_MODE))) //check for P2P_client scenario
                    {
                        if ((pMac->roam.roamSession[sessionId].connectedProfile.operationChannel
                               == 0 )&&
                           (pMac->roam.roamSession[sessionId].connectedProfile.beaconInterval
                               == 0))
                        {
                            continue;
                        }

                        //Assert if connected profile beacon internal is ZERO
                        if(!pMac->roam.roamSession[sessionId].\
                            connectedProfile.beaconInterval)
                        {
                            smsLog( pMac, LOGE, FL(" Connected profile "
                                "beacon interval is zero") );
                        }

                            
                        if (csrIsConnStateConnectedInfra(pMac, sessionId) &&
                           (pMac->roam.roamSession[sessionId].connectedProfile.operationChannel
                                != channelId ) &&
                           (pMac->roam.roamSession[sessionId].connectedProfile.beaconInterval
                                != *beaconInterval))
                        {
                            /*
                             * Updated beaconInterval should be used only when we are starting a new BSS 
                             * not incase of client or STA case
                             */
                            //Calculate beacon Interval for P2P-GO incase of MCC
                            new_beaconInterval = csrCalculateMCCBeaconInterval(pMac, 
                                                pMac->roam.roamSession[sessionId].bssParams.beaconInterval,
                                                *beaconInterval );
                            if(*beaconInterval != new_beaconInterval)
                                *beaconInterval = new_beaconInterval;
                            return eHAL_STATUS_SUCCESS;
                         }
                    }
                }
                break;

                default :
                    smsLog(pMac, LOGE, FL(" Persona not supported : %d"),currBssPersona);
                    return eHAL_STATUS_FAILURE;
            }
        }
    }

    return eHAL_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_VOWIFI_11R
/* Function to return TRUE if the authtype is 11r */
tANI_BOOLEAN csrIsAuthType11r( eCsrAuthType AuthType, tANI_U8 mdiePresent)
{
    switch ( AuthType )
    {
        case eCSR_AUTH_TYPE_OPEN_SYSTEM:
            if(mdiePresent)
                return TRUE;
            break; 
        case eCSR_AUTH_TYPE_FT_RSN_PSK:
        case eCSR_AUTH_TYPE_FT_RSN:
            return TRUE;
            break;
        default:
            break;
    }
    return FALSE;
}

/* Function to return TRUE if the profile is 11r */
tANI_BOOLEAN csrIsProfile11r( tCsrRoamProfile *pProfile )
{
    return csrIsAuthType11r( pProfile->negotiatedAuthType, pProfile->MDID.mdiePresent );
}

#endif

#ifdef FEATURE_WLAN_ESE

/* Function to return TRUE if the authtype is ESE */
tANI_BOOLEAN csrIsAuthTypeESE( eCsrAuthType AuthType )
{
    switch ( AuthType )
    {
        case eCSR_AUTH_TYPE_CCKM_WPA:
        case eCSR_AUTH_TYPE_CCKM_RSN:
            return TRUE;
            break;
        default:
            break;
    }
    return FALSE;
}

/* Function to return TRUE if the profile is ESE */
tANI_BOOLEAN csrIsProfileESE( tCsrRoamProfile *pProfile )
{
    return (csrIsAuthTypeESE( pProfile->negotiatedAuthType ));
}

#endif

#ifdef FEATURE_WLAN_WAPI
tANI_BOOLEAN csrIsProfileWapi( tCsrRoamProfile *pProfile )
{
    tANI_BOOLEAN fWapiProfile = FALSE;

    switch ( pProfile->negotiatedAuthType )
    {
        case eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE:
        case eCSR_AUTH_TYPE_WAPI_WAI_PSK:
            fWapiProfile = TRUE;
            break;

        default:
            fWapiProfile = FALSE;
            break;
    }

    if ( fWapiProfile )
    {
        switch ( pProfile->negotiatedUCEncryptionType )
        {
            case eCSR_ENCRYPT_TYPE_WPI:
                fWapiProfile = TRUE;
                break;

            default:
                fWapiProfile = FALSE;
                break;
        }
    }
    return( fWapiProfile );
}

static tANI_BOOLEAN csrIsWapiOuiEqual( tpAniSirGlobal pMac, tANI_U8 *Oui1, tANI_U8 *Oui2 )
{
    return (vos_mem_compare(Oui1, Oui2, CSR_WAPI_OUI_SIZE));
}

static tANI_BOOLEAN csrIsWapiOuiMatch( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_WAPI_OUI_SIZE],
                                     tANI_U8 cAllCyphers,
                                     tANI_U8 Cypher[],
                                     tANI_U8 Oui[] )
{
    tANI_BOOLEAN fYes = FALSE;
    tANI_U8 idx;

    for ( idx = 0; idx < cAllCyphers; idx++ )
    {
        if ( csrIsWapiOuiEqual( pMac, AllCyphers[ idx ], Cypher ) )
        {
            fYes = TRUE;
            break;
        }
    }

    if ( fYes && Oui )
    {
        vos_mem_copy(Oui, AllCyphers[ idx ], CSR_WAPI_OUI_SIZE);
    }

    return( fYes );
}
#endif /* FEATURE_WLAN_WAPI */

static tANI_BOOLEAN csrIsWpaOuiEqual( tpAniSirGlobal pMac, tANI_U8 *Oui1, tANI_U8 *Oui2 )
{
    return(vos_mem_compare(Oui1, Oui2, CSR_WPA_OUI_SIZE));
}

static tANI_BOOLEAN csrIsOuiMatch( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_WPA_OUI_SIZE],
                                     tANI_U8 cAllCyphers,
                                     tANI_U8 Cypher[],
                                     tANI_U8 Oui[] )
{
    tANI_BOOLEAN fYes = FALSE;
    tANI_U8 idx;

    for ( idx = 0; idx < cAllCyphers; idx++ )
    {
        if ( csrIsWpaOuiEqual( pMac, AllCyphers[ idx ], Cypher ) )
        {
            fYes = TRUE;
            break;
        }
    }

    if ( fYes && Oui )
    {
        vos_mem_copy(Oui, AllCyphers[ idx ], CSR_WPA_OUI_SIZE);
    }

    return( fYes );
}

static tANI_BOOLEAN csrMatchRSNOUIIndex( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_RSN_OUI_SIZE],
                                            tANI_U8 cAllCyphers, tANI_U8 ouiIndex,
                                            tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrRSNOui[ouiIndex], Oui ) );

}

#ifdef FEATURE_WLAN_WAPI
static tANI_BOOLEAN csrMatchWapiOUIIndex( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_WAPI_OUI_SIZE],
                                            tANI_U8 cAllCyphers, tANI_U8 ouiIndex,
                                            tANI_U8 Oui[] )
{
    if (ouiIndex < CSR_WAPI_OUI_ROW_SIZE)// since csrWapiOui row size is 3 .
          return( csrIsWapiOuiMatch( pMac, AllCyphers, cAllCyphers,
                                     csrWapiOui[ouiIndex], Oui ) );
    else
          return FALSE ;

}
#endif /* FEATURE_WLAN_WAPI */

static tANI_BOOLEAN csrMatchWPAOUIIndex( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_RSN_OUI_SIZE],
                                            tANI_U8 cAllCyphers, tANI_U8 ouiIndex,
                                            tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrWpaOui[ouiIndex], Oui ) );

}

#if 0
static tANI_BOOLEAN csrIsRSNUnicastNone( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_RSN_OUI_SIZE],
                                            tANI_U8 cAllCyphers,
                                            tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrRSNOui00, Oui ) );
}

static tANI_BOOLEAN csrIsRSNMulticastWep( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_RSN_OUI_SIZE],
                                           tANI_U8 cAllCyphers,
                                           tANI_U8 Oui[] )
{
    tANI_BOOLEAN fYes = FALSE;

    // Check Wep 104 first, if fails, then check Wep40.
    fYes = csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrRSNOui05, Oui );

    if ( !fYes )
    {
        // if not Wep-104, check Wep-40
        fYes = csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrRSNOui01, Oui );
    }

    return( fYes );
}


static tANI_BOOLEAN csrIsRSNUnicastTkip( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_RSN_OUI_SIZE],
                                    tANI_U8 cAllCyphers,
                                    tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrRSNOui02, Oui ) );
}


static tANI_BOOLEAN csrIsRSNMulticastTkip( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_RSN_OUI_SIZE],
                                              tANI_U8 cAllCyphers,
                                              tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrRSNOui02, Oui ) );
}

static tANI_BOOLEAN csrIsRSNUnicastAes( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_RSN_OUI_SIZE],
                                              tANI_U8 cAllCyphers,
                                              tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrRSNOui04, Oui ) );
}

static tANI_BOOLEAN csrIsRSNMulticastAes( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_RSN_OUI_SIZE],
                                              tANI_U8 cAllCyphers,
                                              tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrRSNOui04, Oui ) );
}
#endif
#ifdef FEATURE_WLAN_WAPI
static tANI_BOOLEAN csrIsAuthWapiCert( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_WAPI_OUI_SIZE],
                                  tANI_U8 cAllSuites,
                                  tANI_U8 Oui[] )
{
    return( csrIsWapiOuiMatch( pMac, AllSuites, cAllSuites, csrWapiOui[1], Oui ) );
}
static tANI_BOOLEAN csrIsAuthWapiPsk( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_WAPI_OUI_SIZE],
                                      tANI_U8 cAllSuites,
                                      tANI_U8 Oui[] )
{
    return( csrIsWapiOuiMatch( pMac, AllSuites, cAllSuites, csrWapiOui[2], Oui ) );
}
#endif /* FEATURE_WLAN_WAPI */

#ifdef WLAN_FEATURE_VOWIFI_11R

/* 
 * Function for 11R FT Authentication. We match the FT Authentication Cipher suite
 * here. This matches for FT Auth with the 802.1X exchange.
 *
 */
static tANI_BOOLEAN csrIsFTAuthRSN( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_RSN_OUI_SIZE],
                                  tANI_U8 cAllSuites,
                                  tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllSuites, cAllSuites, csrRSNOui[03], Oui ) );
}

/* 
 * Function for 11R FT Authentication. We match the FT Authentication Cipher suite
 * here. This matches for FT Auth with the PSK.
 *
 */
static tANI_BOOLEAN csrIsFTAuthRSNPsk( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_RSN_OUI_SIZE],
                                      tANI_U8 cAllSuites,
                                      tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllSuites, cAllSuites, csrRSNOui[04], Oui ) );
}

#endif

#ifdef FEATURE_WLAN_ESE

/*
 * Function for ESE CCKM AKM Authentication. We match the CCKM AKM Authentication Key Management suite
 * here. This matches for CCKM AKM Auth with the 802.1X exchange.
 *
 */
static tANI_BOOLEAN csrIsEseCckmAuthRSN( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_RSN_OUI_SIZE],
                                  tANI_U8 cAllSuites,
                                  tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllSuites, cAllSuites, csrRSNOui[06], Oui ) );
}

static tANI_BOOLEAN csrIsEseCckmAuthWpa( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_WPA_OUI_SIZE],
                                tANI_U8 cAllSuites,
                                tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllSuites, cAllSuites, csrWpaOui[06], Oui ) );
}

#endif

static tANI_BOOLEAN csrIsAuthRSN( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_RSN_OUI_SIZE],
                                  tANI_U8 cAllSuites,
                                  tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllSuites, cAllSuites, csrRSNOui[01], Oui ) );
}
static tANI_BOOLEAN csrIsAuthRSNPsk( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_RSN_OUI_SIZE],
                                      tANI_U8 cAllSuites,
                                      tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllSuites, cAllSuites, csrRSNOui[02], Oui ) );
}

#ifdef WLAN_FEATURE_11W
static tANI_BOOLEAN csrIsAuthRSNPskSha256( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_RSN_OUI_SIZE],
                                      tANI_U8 cAllSuites,
                                      tANI_U8 Oui[] )
{
    return csrIsOuiMatch( pMac, AllSuites, cAllSuites, csrRSNOui[07], Oui );
}
static tANI_BOOLEAN csrIsAuthRSN8021xSha256(tpAniSirGlobal pMac,
                                            tANI_U8 AllSuites[][CSR_RSN_OUI_SIZE],
                                            tANI_U8 cAllSuites,
                                            tANI_U8 Oui[] )
{
    return csrIsOuiMatch( pMac, AllSuites, cAllSuites, csrRSNOui[8], Oui );
}
#endif

static tANI_BOOLEAN csrIsAuthWpa( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_WPA_OUI_SIZE],
                                tANI_U8 cAllSuites,
                                tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllSuites, cAllSuites, csrWpaOui[01], Oui ) );
}

#ifdef NOT_CURRENTLY_USED
static tANI_BOOLEAN csrIsAuth802_1x( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_WPA_OUI_SIZE],
                                tANI_U8 cAllSuites,
                                tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllSuites, cAllSuites, csrWpaOui[00], Oui ) );
}
#endif // NOT_CURRENTLY_USED

static tANI_BOOLEAN csrIsAuthWpaPsk( tpAniSirGlobal pMac, tANI_U8 AllSuites[][CSR_WPA_OUI_SIZE],
                                tANI_U8 cAllSuites,
                                tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllSuites, cAllSuites, csrWpaOui[02], Oui ) );
}
#if 0
static tANI_BOOLEAN csrIsUnicastNone( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_WPA_OUI_SIZE],
                                      tANI_U8 cAllCyphers,
                                      tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrWpaOui00, Oui ) );
}

static tANI_BOOLEAN csrIsUnicastTkip( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_WPA_OUI_SIZE],
                                      tANI_U8 cAllCyphers,
                                      tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrWpaOui02, Oui ) );
}

static tANI_BOOLEAN csrIsUnicastAes( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_WPA_OUI_SIZE],
                                      tANI_U8 cAllCyphers,
                                      tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrWpaOui04, Oui ) );
}


static tANI_BOOLEAN csrIsMulticastWep( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_WPA_OUI_SIZE],
                                          tANI_U8 cAllCyphers,
                                          tANI_U8 Oui[] )
{
    tANI_BOOLEAN fYes = FALSE;

    // Check Wep 104 first, if fails, then check Wep40.
    fYes = csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrWpaOui05, Oui );

    if ( !fYes )
    {
        // if not Wep-104, check Wep-40
        fYes = csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrWpaOui01, Oui );
    }

    return( fYes );
}


static tANI_BOOLEAN csrIsMulticastTkip( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_WPA_OUI_SIZE],
                                          tANI_U8 cAllCyphers,
                                          tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrWpaOui02, Oui ) );
}


static tANI_BOOLEAN csrIsMulticastAes( tpAniSirGlobal pMac, tANI_U8 AllCyphers[][CSR_WPA_OUI_SIZE],
                                          tANI_U8 cAllCyphers,
                                          tANI_U8 Oui[] )
{
    return( csrIsOuiMatch( pMac, AllCyphers, cAllCyphers, csrWpaOui04, Oui ) );
}

#endif

tANI_U8 csrGetOUIIndexFromCipher( eCsrEncryptionType enType )
{
    tANI_U8 OUIIndex;

        switch ( enType )
        {
            case eCSR_ENCRYPT_TYPE_WEP40:
            case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
                OUIIndex = CSR_OUI_WEP40_OR_1X_INDEX;
                break;
            case eCSR_ENCRYPT_TYPE_WEP104:
            case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
                OUIIndex = CSR_OUI_WEP104_INDEX;
                break;
            case eCSR_ENCRYPT_TYPE_TKIP:
                OUIIndex = CSR_OUI_TKIP_OR_PSK_INDEX;
                break;
            case eCSR_ENCRYPT_TYPE_AES:
                OUIIndex = CSR_OUI_AES_INDEX;
                break;
            case eCSR_ENCRYPT_TYPE_NONE:
                OUIIndex = CSR_OUI_USE_GROUP_CIPHER_INDEX;
                break;
#ifdef FEATURE_WLAN_WAPI
           case eCSR_ENCRYPT_TYPE_WPI:
               OUIIndex = CSR_OUI_WAPI_WAI_CERT_OR_SMS4_INDEX;
               break;
#endif /* FEATURE_WLAN_WAPI */
            default: //HOWTO handle this?
                OUIIndex = CSR_OUI_RESERVED_INDEX;
                break;
        }//switch

        return OUIIndex;
}

tANI_BOOLEAN csrGetRSNInformation( tHalHandle hHal, tCsrAuthList *pAuthType, eCsrEncryptionType enType, tCsrEncryptionList *pMCEncryption,
                                   tDot11fIERSN *pRSNIe,
                           tANI_U8 *UnicastCypher,
                           tANI_U8 *MulticastCypher,
                           tANI_U8 *AuthSuite,
                           tCsrRSNCapabilities *Capabilities,
                           eCsrAuthType *pNegotiatedAuthtype,
                           eCsrEncryptionType *pNegotiatedMCCipher )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_BOOLEAN fAcceptableCyphers = FALSE;
    tANI_U8 cUnicastCyphers = 0;
    tANI_U8 cMulticastCyphers = 0;
    tANI_U8 cAuthSuites = 0, i;
    tANI_U8 Unicast[ CSR_RSN_OUI_SIZE ];
    tANI_U8 Multicast[ CSR_RSN_OUI_SIZE ];
    tANI_U8 AuthSuites[ CSR_RSN_MAX_AUTH_SUITES ][ CSR_RSN_OUI_SIZE ];
    tANI_U8 Authentication[ CSR_RSN_OUI_SIZE ];
    tANI_U8 MulticastCyphers[ CSR_RSN_MAX_MULTICAST_CYPHERS ][ CSR_RSN_OUI_SIZE ];
    eCsrAuthType negAuthType = eCSR_AUTH_TYPE_UNKNOWN;

    do{
        if ( pRSNIe->present )
        {
            cMulticastCyphers++;
            vos_mem_copy(MulticastCyphers, pRSNIe->gp_cipher_suite, CSR_RSN_OUI_SIZE);
            cUnicastCyphers = (tANI_U8)(pRSNIe->pwise_cipher_suite_count);
            cAuthSuites = (tANI_U8)(pRSNIe->akm_suite_count);
            for(i = 0; i < cAuthSuites && i < CSR_RSN_MAX_AUTH_SUITES; i++)
            {
                vos_mem_copy((void *)&AuthSuites[i],
                             (void *)&pRSNIe->akm_suites[i],
                             CSR_RSN_OUI_SIZE);
            }

            //Check - Is requested Unicast Cipher supported by the BSS.
            fAcceptableCyphers = csrMatchRSNOUIIndex( pMac, pRSNIe->pwise_cipher_suites, cUnicastCyphers, 
                    csrGetOUIIndexFromCipher( enType ), Unicast ); 

            if( !fAcceptableCyphers ) break;


            //Unicast is supported. Pick the first matching Group cipher, if any.
            for( i = 0 ; i < pMCEncryption->numEntries ; i++ )
            {
                fAcceptableCyphers = csrMatchRSNOUIIndex( pMac, MulticastCyphers,  cMulticastCyphers, 
                            csrGetOUIIndexFromCipher( pMCEncryption->encryptionType[i] ), Multicast );
                if(fAcceptableCyphers)
                {
                    break;
                }
            }
            if( !fAcceptableCyphers ) break;

            if( pNegotiatedMCCipher )
                *pNegotiatedMCCipher = pMCEncryption->encryptionType[i];
            
            /* Initializing with FALSE as it has TRUE value already */
            fAcceptableCyphers = FALSE;
            for (i = 0 ; i < pAuthType->numEntries; i++)
            {
                //Ciphers are supported, Match authentication algorithm and pick first matching authtype.
 #ifdef WLAN_FEATURE_VOWIFI_11R
                /* Changed the AKM suites according to order of preference */
                if ( csrIsFTAuthRSN( pMac, AuthSuites, cAuthSuites, Authentication ) )
                {
                    if (eCSR_AUTH_TYPE_FT_RSN == pAuthType->authType[i])
                        negAuthType = eCSR_AUTH_TYPE_FT_RSN;
                }
                if ( (negAuthType == eCSR_AUTH_TYPE_UNKNOWN) && csrIsFTAuthRSNPsk( pMac, AuthSuites, cAuthSuites, Authentication ) )
                {
                    if (eCSR_AUTH_TYPE_FT_RSN_PSK == pAuthType->authType[i])
                        negAuthType = eCSR_AUTH_TYPE_FT_RSN_PSK;
                }
#endif
#ifdef FEATURE_WLAN_ESE
                /* ESE only supports 802.1X.  No PSK. */
                if ( (negAuthType == eCSR_AUTH_TYPE_UNKNOWN) && csrIsEseCckmAuthRSN( pMac, AuthSuites, cAuthSuites, Authentication ) )
                {
                    if (eCSR_AUTH_TYPE_CCKM_RSN == pAuthType->authType[i])
                        negAuthType = eCSR_AUTH_TYPE_CCKM_RSN;
                }
#endif
                if ( (negAuthType == eCSR_AUTH_TYPE_UNKNOWN) && csrIsAuthRSN( pMac, AuthSuites, cAuthSuites, Authentication ) )
                {
                    if (eCSR_AUTH_TYPE_RSN == pAuthType->authType[i])
                        negAuthType = eCSR_AUTH_TYPE_RSN;
                }
                if ((negAuthType == eCSR_AUTH_TYPE_UNKNOWN) && csrIsAuthRSNPsk( pMac, AuthSuites, cAuthSuites, Authentication ) )
                {
                    if (eCSR_AUTH_TYPE_RSN_PSK == pAuthType->authType[i])
                        negAuthType = eCSR_AUTH_TYPE_RSN_PSK;
                }
#ifdef WLAN_FEATURE_11W
                if ((negAuthType == eCSR_AUTH_TYPE_UNKNOWN) && csrIsAuthRSNPskSha256( pMac, AuthSuites, cAuthSuites, Authentication ) )
                {
                    if (eCSR_AUTH_TYPE_RSN_PSK_SHA256 == pAuthType->authType[i])
                        negAuthType = eCSR_AUTH_TYPE_RSN_PSK_SHA256;
                }
                if ((negAuthType == eCSR_AUTH_TYPE_UNKNOWN) &&
                    csrIsAuthRSN8021xSha256(pMac, AuthSuites,
                                             cAuthSuites, Authentication)) {
                    if (eCSR_AUTH_TYPE_RSN_8021X_SHA256 ==
                                                     pAuthType->authType[i])
                        negAuthType = eCSR_AUTH_TYPE_RSN_8021X_SHA256;
                }
#endif

                // The 1st auth type in the APs RSN IE, to match stations connecting
                // profiles auth type will cause us to exit this loop
                // This is added as some APs advertise multiple akms in the RSN IE.
                if (eCSR_AUTH_TYPE_UNKNOWN != negAuthType)
                {
                    fAcceptableCyphers = TRUE;
                    break;
                }
            } // for
        }

    }while (0);

    if ( fAcceptableCyphers )
    {
        if ( MulticastCypher )
        {
            vos_mem_copy(MulticastCypher, Multicast, CSR_RSN_OUI_SIZE);
        }

        if ( UnicastCypher )
        {
            vos_mem_copy(UnicastCypher, Unicast, CSR_RSN_OUI_SIZE);
        }

        if ( AuthSuite )
        {
            vos_mem_copy(AuthSuite, Authentication, CSR_RSN_OUI_SIZE);
        }

        if ( pNegotiatedAuthtype )
        {
            *pNegotiatedAuthtype = negAuthType;
        }
        if ( Capabilities )
        {
            Capabilities->PreAuthSupported = (pRSNIe->RSN_Cap[0] >> 0) & 0x1 ; // Bit 0 PreAuthentication
            Capabilities->NoPairwise = (pRSNIe->RSN_Cap[0] >> 1) & 0x1 ; // Bit 1 No Pairwise
            Capabilities->PTKSAReplayCounter = (pRSNIe->RSN_Cap[0] >> 2) & 0x3 ; // Bit 2, 3 PTKSA Replay Counter
            Capabilities->GTKSAReplayCounter = (pRSNIe->RSN_Cap[0] >> 4) & 0x3 ; // Bit 4, 5 GTKSA Replay Counter
            Capabilities->MFPRequired = (pRSNIe->RSN_Cap[0] >> 6) & 0x1 ; // Bit 6 MFPR
            Capabilities->MFPCapable = (pRSNIe->RSN_Cap[0] >> 7) & 0x1 ; // Bit 7 MFPC
            Capabilities->Reserved = pRSNIe->RSN_Cap[1]  & 0xff ; // remaining reserved
        }
    }
    return( fAcceptableCyphers );
}

#ifdef WLAN_FEATURE_11W
/* ---------------------------------------------------------------------------
    \fn csrIsPMFCapabilitiesInRSNMatch

    \brief this function is to match our current capabilities with the AP
           to which we are expecting make the connection.

    \param hHal               - HAL Pointer
           pFilterMFPEnabled  - given by supplicant to us to specify what kind
                                of connection supplicant is expecting to make
                                if it is enabled then make PMF connection.
                                if it is disabled then make normal connection.
           pFilterMFPRequired - given by supplicant based on our configuration
                                if it is 1 then we will require mandatory
                                PMF connection and if it is 0 then we PMF
                                connection is optional.
           pFilterMFPCapable  - given by supplicant based on our configuration
                                if it 1 then we are PMF capable and if it 0
                                then we are not PMF capable.
           pRSNIe             - RSNIe from Beacon/probe response of
                                neighbor AP against which we will compare
                                our capabilities.

    \return tANI_BOOLEAN      - if our PMF capabilities matches with AP then we
                                will return true to indicate that we are good
                                to make connection with it. Else we will return
                                false.
  -------------------------------------------------------------------------------*/
static tANI_BOOLEAN
csrIsPMFCapabilitiesInRSNMatch( tHalHandle hHal,
                                tANI_BOOLEAN *pFilterMFPEnabled,
                                tANI_U8 *pFilterMFPRequired,
                                tANI_U8 *pFilterMFPCapable,
                                tDot11fIERSN *pRSNIe)
{
    tANI_U8 apProfileMFPCapable  = 0;
    tANI_U8 apProfileMFPRequired = 0;
    if (pRSNIe && pFilterMFPEnabled && pFilterMFPCapable && pFilterMFPRequired)
    {
       /* Extracting MFPCapable bit from RSN Ie */
       apProfileMFPCapable  = (pRSNIe->RSN_Cap[0] >> 7) & 0x1;
       apProfileMFPRequired = (pRSNIe->RSN_Cap[0] >> 6) & 0x1;
       if (*pFilterMFPEnabled && *pFilterMFPCapable && *pFilterMFPRequired
           && (apProfileMFPCapable == 0))
       {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                     "AP is not capable to make PMF connection");
           return VOS_FALSE;
       }
       else if (*pFilterMFPEnabled && *pFilterMFPCapable &&
                !(*pFilterMFPRequired) && (apProfileMFPCapable == 0))
       {
           /*
            * This is tricky, because supplicant asked us to make mandatory
            * PMF connection eventhough PMF connection is optional here.
            * so if AP is not capable of PMF then drop it. Don't try to
            * connect with it.
            */
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
           "we need PMF connection & AP isn't capable to make PMF connection");
           return VOS_FALSE;
       }
       else if (!(*pFilterMFPCapable) &&
                apProfileMFPCapable && apProfileMFPRequired)
       {
           /*
            * In this case, AP with whom we trying to connect requires
            * mandatory PMF connections and we are not capable so this AP
            * is not good choice to connect
            */
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
           "AP needs PMF connection and we are not capable of pmf connection");
           return VOS_FALSE;
       }
       else if (!(*pFilterMFPEnabled) && *pFilterMFPCapable &&
                (apProfileMFPCapable == 1))
       {
           VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
           "we don't need PMF connection eventhough both parties are capable");
           return VOS_FALSE;
       }
    }
    return VOS_TRUE;
}
#endif

tANI_BOOLEAN csrIsRSNMatch( tHalHandle hHal, tCsrAuthList *pAuthType,
                            eCsrEncryptionType enType,
                            tCsrEncryptionList *pEnMcType,
                            tANI_BOOLEAN *pMFPEnabled, tANI_U8 *pMFPRequired,
                            tANI_U8 *pMFPCapable,
                            tDot11fBeaconIEs *pIes,
                            eCsrAuthType *pNegotiatedAuthType,
                            eCsrEncryptionType *pNegotiatedMCCipher )
{
    tANI_BOOLEAN fRSNMatch = FALSE;

        // See if the cyphers in the Bss description match with the settings in the profile.
    fRSNMatch = csrGetRSNInformation( hHal, pAuthType, enType, pEnMcType, &pIes->RSN, NULL, NULL, NULL, NULL, 
                                      pNegotiatedAuthType, pNegotiatedMCCipher );
#ifdef WLAN_FEATURE_11W
    /* If all the filter matches then finally checks for PMF capabilities */
    if (fRSNMatch)
    {
        fRSNMatch = csrIsPMFCapabilitiesInRSNMatch( hHal, pMFPEnabled,
                                                    pMFPRequired, pMFPCapable,
                                                    &pIes->RSN);
    }
#endif

    return( fRSNMatch );
}


tANI_BOOLEAN csrLookupPMKID( tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U8 *pBSSId, tANI_U8 *pPMKId )
{
    tANI_BOOLEAN fRC = FALSE, fMatchFound = FALSE;
    tANI_U32 Index;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return FALSE;
    }
   /* to force the AP initiate fresh 802.1x authentication after re-association should not 
    * fill the PMKID from cache  this is needed 
    * by the HS 2.0 passpoint certification 5.2.a and b testcases */ 
    
    if(pSession->fIgnorePMKIDCache)
    {
        pSession->fIgnorePMKIDCache = FALSE;
        return fRC;
    }
    
    do
    {
        for( Index=0; Index < CSR_MAX_PMKID_ALLOWED; Index++ )
        {
            if( vos_mem_compare(pBSSId, pSession->PmkidCacheInfo[Index].BSSID, sizeof(tCsrBssid)) )
            {
                // match found
                fMatchFound = TRUE;
                break;
            }
        }

        if( !fMatchFound ) break;

        vos_mem_copy(pPMKId, pSession->PmkidCacheInfo[Index].PMKID, CSR_RSN_PMKID_SIZE);

        fRC = TRUE;
    }
    while( 0 );
    smsLog(pMac, LOGW, "csrLookupPMKID called return match = %d pMac->roam.NumPmkidCache = %d",
        fRC, pSession->NumPmkidCache);

    return fRC;
}


tANI_U8 csrConstructRSNIe( tHalHandle hHal, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                            tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes, tCsrRSNIe *pRSNIe )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_BOOLEAN fRSNMatch;
    tANI_U8 cbRSNIe = 0;
    tANI_U8 UnicastCypher[ CSR_RSN_OUI_SIZE ];
    tANI_U8 MulticastCypher[ CSR_RSN_OUI_SIZE ];
    tANI_U8 AuthSuite[ CSR_RSN_OUI_SIZE ];
    tCsrRSNAuthIe *pAuthSuite;
    tCsrRSNCapabilities RSNCapabilities;
    tCsrRSNPMKIe        *pPMK;
    tANI_U8 PMKId[CSR_RSN_PMKID_SIZE];
#ifdef WLAN_FEATURE_11W
    tANI_U8 *pGroupMgmtCipherSuite;
#endif
    tDot11fBeaconIEs *pIesLocal = pIes;
    eCsrAuthType negAuthType = eCSR_AUTH_TYPE_UNKNOWN;

    smsLog(pMac, LOGW, "%s called...", __func__);

    do
    {
        if ( !csrIsProfileRSN( pProfile ) ) break;

        if( !pIesLocal && (!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pSirBssDesc, &pIesLocal))) )
        {
            break;
        }

        // See if the cyphers in the Bss description match with the settings in the profile.
        fRSNMatch = csrGetRSNInformation( hHal, &pProfile->AuthType, pProfile->negotiatedUCEncryptionType, 
                                            &pProfile->mcEncryptionType, &pIesLocal->RSN,
                                            UnicastCypher, MulticastCypher, AuthSuite, &RSNCapabilities, &negAuthType, NULL );
        if ( !fRSNMatch ) break;

        pRSNIe->IeHeader.ElementID = SIR_MAC_RSN_EID;

        pRSNIe->Version = CSR_RSN_VERSION_SUPPORTED;

        vos_mem_copy(pRSNIe->MulticastOui, MulticastCypher, sizeof( MulticastCypher ));

        pRSNIe->cUnicastCyphers = 1;

        vos_mem_copy(&pRSNIe->UnicastOui[ 0 ], UnicastCypher, sizeof( UnicastCypher ));

        pAuthSuite = (tCsrRSNAuthIe *)( &pRSNIe->UnicastOui[ pRSNIe->cUnicastCyphers ] );

        pAuthSuite->cAuthenticationSuites = 1;
        vos_mem_copy(&pAuthSuite->AuthOui[ 0 ], AuthSuite, sizeof( AuthSuite ));

        // RSN capabilities follows the Auth Suite (two octects)
        // !!REVIEW - What should STA put in RSN capabilities, currently
        // just putting back APs capabilities
        // For one, we shouldn't EVER be sending out "pre-auth supported".  It is an AP only capability
        // For another, we should use the Management Frame Protection values given by the supplicant
        RSNCapabilities.PreAuthSupported = 0;
#ifdef WLAN_FEATURE_11W
        RSNCapabilities.MFPRequired = pProfile->MFPRequired;
        RSNCapabilities.MFPCapable = pProfile->MFPCapable;
#endif
        *(tANI_U16 *)( &pAuthSuite->AuthOui[ 1 ] ) = *((tANI_U16 *)(&RSNCapabilities));

        pPMK = (tCsrRSNPMKIe *)( ((tANI_U8 *)(&pAuthSuite->AuthOui[ 1 ])) + sizeof(tANI_U16) );

        if (
#ifdef FEATURE_WLAN_ESE
        (eCSR_AUTH_TYPE_CCKM_RSN != negAuthType) &&
#endif
        csrLookupPMKID( pMac, sessionId, pSirBssDesc->bssId, &(PMKId[0]) ) )
        {
            pPMK->cPMKIDs = 1;

            vos_mem_copy(pPMK->PMKIDList[0].PMKID, PMKId, CSR_RSN_PMKID_SIZE);
        }
        else
        {
            pPMK->cPMKIDs = 0;
        }

#ifdef WLAN_FEATURE_11W
        if ( pProfile->MFPEnabled )
        {
            pGroupMgmtCipherSuite = (tANI_U8 *) pPMK + sizeof ( tANI_U16 ) +
                ( pPMK->cPMKIDs * CSR_RSN_PMKID_SIZE );
            vos_mem_copy(pGroupMgmtCipherSuite, csrRSNOui[07], CSR_WPA_OUI_SIZE);
        }
#endif

        // Add in the fixed fields plus 1 Unicast cypher, less the IE Header length
        // Add in the size of the Auth suite (count plus a single OUI)
        // Add in the RSN caps field.
        // Add PMKID count and PMKID (if any)
        // Add group management cipher suite
        pRSNIe->IeHeader.Length = (tANI_U8) (sizeof( *pRSNIe ) - sizeof ( pRSNIe->IeHeader ) +
                                  sizeof( *pAuthSuite ) +
                                  sizeof( tCsrRSNCapabilities ));
        if(pPMK->cPMKIDs)
        {
            pRSNIe->IeHeader.Length += (tANI_U8)(sizeof( tANI_U16 ) +
                                        (pPMK->cPMKIDs * CSR_RSN_PMKID_SIZE));
        }
#ifdef WLAN_FEATURE_11W
        if ( pProfile->MFPEnabled )
        {
            if ( 0 == pPMK->cPMKIDs )
                pRSNIe->IeHeader.Length += sizeof( tANI_U16 );
            pRSNIe->IeHeader.Length += CSR_WPA_OUI_SIZE;
        }
#endif

        // return the size of the IE header (total) constructed...
        cbRSNIe = pRSNIe->IeHeader.Length + sizeof( pRSNIe->IeHeader );

    } while( 0 );

    if( !pIes && pIesLocal )
    {
        //locally allocated
        vos_mem_free(pIesLocal);
    }

    return( cbRSNIe );
}


#ifdef FEATURE_WLAN_WAPI
tANI_BOOLEAN csrGetWapiInformation( tHalHandle hHal, tCsrAuthList *pAuthType, eCsrEncryptionType enType, tCsrEncryptionList *pMCEncryption,
                                   tDot11fIEWAPI *pWapiIe,
                                    tANI_U8 *UnicastCypher,
                                    tANI_U8 *MulticastCypher,
                                    tANI_U8 *AuthSuite,
                                    eCsrAuthType *pNegotiatedAuthtype,
                                    eCsrEncryptionType *pNegotiatedMCCipher )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_BOOLEAN fAcceptableCyphers = FALSE;
    tANI_U8 cUnicastCyphers = 0;
    tANI_U8 cMulticastCyphers = 0;
    tANI_U8 cAuthSuites = 0, i;
    tANI_U8 Unicast[ CSR_WAPI_OUI_SIZE ];
    tANI_U8 Multicast[ CSR_WAPI_OUI_SIZE ];
    tANI_U8 AuthSuites[ CSR_WAPI_MAX_AUTH_SUITES ][ CSR_WAPI_OUI_SIZE ];
    tANI_U8 Authentication[ CSR_WAPI_OUI_SIZE ];
    tANI_U8 MulticastCyphers[ CSR_WAPI_MAX_MULTICAST_CYPHERS ][ CSR_WAPI_OUI_SIZE ];
    eCsrAuthType negAuthType = eCSR_AUTH_TYPE_UNKNOWN;

    do{
        if ( pWapiIe->present )
        {
            cMulticastCyphers++;
            vos_mem_copy(MulticastCyphers, pWapiIe->multicast_cipher_suite,
                         CSR_WAPI_OUI_SIZE);
            cUnicastCyphers = (tANI_U8)(pWapiIe->unicast_cipher_suite_count);
            cAuthSuites = (tANI_U8)(pWapiIe->akm_suite_count);
            for(i = 0; i < cAuthSuites && i < CSR_WAPI_MAX_AUTH_SUITES; i++)
            {
                vos_mem_copy((void *)&AuthSuites[i], (void *)&pWapiIe->akm_suites[i],
                             CSR_WAPI_OUI_SIZE);
            }

            //Check - Is requested Unicast Cipher supported by the BSS.
            fAcceptableCyphers = csrMatchWapiOUIIndex( pMac, pWapiIe->unicast_cipher_suites, cUnicastCyphers, 
                    csrGetOUIIndexFromCipher( enType ), Unicast ); 

            if( !fAcceptableCyphers ) break;


            //Unicast is supported. Pick the first matching Group cipher, if any.
            for( i = 0 ; i < pMCEncryption->numEntries ; i++ )
            {
                fAcceptableCyphers = csrMatchWapiOUIIndex( pMac, MulticastCyphers,  cMulticastCyphers, 
                csrGetOUIIndexFromCipher( pMCEncryption->encryptionType[i] ), Multicast );
                if(fAcceptableCyphers)
                {
                    break;
                }
            }
            if( !fAcceptableCyphers ) break;

            if( pNegotiatedMCCipher )
                *pNegotiatedMCCipher = pMCEncryption->encryptionType[i];

            //Ciphers are supported, Match authentication algorithm and pick first matching authtype.
            if ( csrIsAuthWapiCert( pMac, AuthSuites, cAuthSuites, Authentication ) )
            {
                negAuthType = eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE;
            }
            else if ( csrIsAuthWapiPsk( pMac, AuthSuites, cAuthSuites, Authentication ) )
            {
                negAuthType = eCSR_AUTH_TYPE_WAPI_WAI_PSK;
            }
            else
            {
                fAcceptableCyphers = FALSE;
                negAuthType = eCSR_AUTH_TYPE_UNKNOWN;
            }
            if( ( 0 == pAuthType->numEntries ) || ( FALSE == fAcceptableCyphers ) )
            {
                //Caller doesn't care about auth type, or BSS doesn't match
                break;
            }
            fAcceptableCyphers = FALSE;
            for( i = 0 ; i < pAuthType->numEntries; i++ )
            {
                if( pAuthType->authType[i] == negAuthType )
                {
                    fAcceptableCyphers = TRUE;
                    break;
                }
            }
        }
    }while (0);

    if ( fAcceptableCyphers )
    {
        if ( MulticastCypher )
        {
           vos_mem_copy(MulticastCypher, Multicast, CSR_WAPI_OUI_SIZE);
        }

        if ( UnicastCypher )
        {
            vos_mem_copy(UnicastCypher, Unicast, CSR_WAPI_OUI_SIZE);
        }

        if ( AuthSuite )
        {
            vos_mem_copy(AuthSuite, Authentication, CSR_WAPI_OUI_SIZE);
        }

        if ( pNegotiatedAuthtype )
        {
            *pNegotiatedAuthtype = negAuthType;
        }
    }
    return( fAcceptableCyphers );
}

tANI_BOOLEAN csrIsWapiMatch( tHalHandle hHal, tCsrAuthList *pAuthType, eCsrEncryptionType enType, tCsrEncryptionList *pEnMcType, 
                            tDot11fBeaconIEs *pIes, eCsrAuthType *pNegotiatedAuthType, eCsrEncryptionType *pNegotiatedMCCipher )
{
    tANI_BOOLEAN fWapiMatch = FALSE;

        // See if the cyphers in the Bss description match with the settings in the profile.
    fWapiMatch = csrGetWapiInformation( hHal, pAuthType, enType, pEnMcType, &pIes->WAPI, NULL, NULL, NULL, 
                                      pNegotiatedAuthType, pNegotiatedMCCipher );

    return( fWapiMatch );
}

tANI_BOOLEAN csrLookupBKID( tpAniSirGlobal pMac, tANI_U32 sessionId, tANI_U8 *pBSSId, tANI_U8 *pBKId )
{
    tANI_BOOLEAN fRC = FALSE, fMatchFound = FALSE;
    tANI_U32 Index;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

    if(!pSession)
    {
        smsLog(pMac, LOGE, FL("  session %d not found "), sessionId);
        return FALSE;
    }

    do
    {
        for( Index=0; Index < pSession->NumBkidCache; Index++ )
        {
            smsLog(pMac, LOGW, "match BKID "MAC_ADDRESS_STR" to ",
                   MAC_ADDR_ARRAY(pBSSId));
            if (vos_mem_compare(pBSSId, pSession->BkidCacheInfo[Index].BSSID, sizeof(tCsrBssid) ) )
            {
                // match found
                fMatchFound = TRUE;
                break;
            }
        }

        if( !fMatchFound ) break;

        vos_mem_copy(pBKId, pSession->BkidCacheInfo[Index].BKID, CSR_WAPI_BKID_SIZE);

        fRC = TRUE;
    }
    while( 0 );
    smsLog(pMac, LOGW, "csrLookupBKID called return match = %d pMac->roam.NumBkidCache = %d", fRC, pSession->NumBkidCache);

    return fRC;
}

tANI_U8 csrConstructWapiIe( tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pProfile,
                            tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes, tCsrWapiIe *pWapiIe )
{
    tANI_BOOLEAN fWapiMatch = FALSE;
    tANI_U8 cbWapiIe = 0;
    tANI_U8 UnicastCypher[ CSR_WAPI_OUI_SIZE ];
    tANI_U8 MulticastCypher[ CSR_WAPI_OUI_SIZE ];
    tANI_U8 AuthSuite[ CSR_WAPI_OUI_SIZE ];
    tANI_U8 BKId[CSR_WAPI_BKID_SIZE];
    tANI_U8 *pWapi = NULL;
    tANI_BOOLEAN fBKIDFound = FALSE;
    tDot11fBeaconIEs *pIesLocal = pIes;

    do
    {
        if ( !csrIsProfileWapi( pProfile ) ) break;

        if( !pIesLocal && (!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pSirBssDesc, &pIesLocal))) )
        {
            break;
        }

        // See if the cyphers in the Bss description match with the settings in the profile.
        fWapiMatch = csrGetWapiInformation( pMac, &pProfile->AuthType, pProfile->negotiatedUCEncryptionType, 
                                            &pProfile->mcEncryptionType, &pIesLocal->WAPI,
                                            UnicastCypher, MulticastCypher, AuthSuite, NULL, NULL );
        if ( !fWapiMatch ) break;

        vos_mem_set(pWapiIe, sizeof(tCsrWapiIe), 0);

        pWapiIe->IeHeader.ElementID = DOT11F_EID_WAPI;

        pWapiIe->Version = CSR_WAPI_VERSION_SUPPORTED;

        pWapiIe->cAuthenticationSuites = 1;
        vos_mem_copy(&pWapiIe->AuthOui[ 0 ], AuthSuite, sizeof( AuthSuite ));

        pWapi = (tANI_U8 *) (&pWapiIe->AuthOui[ 1 ]);

        *pWapi = (tANI_U16)1; //cUnicastCyphers
        pWapi+=2;
        vos_mem_copy(pWapi, UnicastCypher, sizeof( UnicastCypher ));
        pWapi += sizeof( UnicastCypher );

        vos_mem_copy(pWapi, MulticastCypher, sizeof( MulticastCypher ));
        pWapi += sizeof( MulticastCypher );


        // WAPI capabilities follows the Auth Suite (two octects)
        // we shouldn't EVER be sending out "pre-auth supported".  It is an AP only capability
        // & since we already did a memset pWapiIe to 0, skip these fields
        pWapi +=2;

        fBKIDFound = csrLookupBKID( pMac, sessionId, pSirBssDesc->bssId, &(BKId[0]) );


        if( fBKIDFound )
        {
            /* Do we need to change the endianness here */
            *pWapi = (tANI_U16)1; //cBKIDs
            pWapi+=2;
            vos_mem_copy(pWapi, BKId, CSR_WAPI_BKID_SIZE);
        }
        else
        {
            *pWapi = 0;
            pWapi+=1;
            *pWapi = 0;
            pWapi+=1;
        }

        // Add in the IE fields except the IE header
        // Add BKID count and BKID (if any)
        pWapiIe->IeHeader.Length = (tANI_U8) (sizeof( *pWapiIe ) - sizeof ( pWapiIe->IeHeader ));

        /*2 bytes for BKID Count field*/
        pWapiIe->IeHeader.Length += sizeof( tANI_U16 );

        if(fBKIDFound)
        {
            pWapiIe->IeHeader.Length += CSR_WAPI_BKID_SIZE;
        }
        // return the size of the IE header (total) constructed...
        cbWapiIe = pWapiIe->IeHeader.Length + sizeof( pWapiIe->IeHeader );

    } while( 0 );

    if( !pIes && pIesLocal )
    {
        //locally allocated
        vos_mem_free(pIesLocal);
    }

    return( cbWapiIe );
}
#endif /* FEATURE_WLAN_WAPI */

tANI_BOOLEAN csrGetWpaCyphers( tpAniSirGlobal pMac, tCsrAuthList *pAuthType, eCsrEncryptionType enType, tCsrEncryptionList *pMCEncryption,
                               tDot11fIEWPA *pWpaIe,
                           tANI_U8 *UnicastCypher,
                           tANI_U8 *MulticastCypher,
                           tANI_U8 *AuthSuite,
                           eCsrAuthType *pNegotiatedAuthtype,
                           eCsrEncryptionType *pNegotiatedMCCipher )
{
    tANI_BOOLEAN fAcceptableCyphers = FALSE;
    tANI_U8 cUnicastCyphers = 0;
    tANI_U8 cMulticastCyphers = 0;
    tANI_U8 cAuthSuites = 0;
    tANI_U8 Unicast[ CSR_WPA_OUI_SIZE ];
    tANI_U8 Multicast[ CSR_WPA_OUI_SIZE ];
    tANI_U8 Authentication[ CSR_WPA_OUI_SIZE ];
    tANI_U8 MulticastCyphers[ 1 ][ CSR_WPA_OUI_SIZE ];
    tANI_U8 i;
    eCsrAuthType negAuthType = eCSR_AUTH_TYPE_UNKNOWN;

    do
    {
        if ( pWpaIe->present )
        {
            cMulticastCyphers = 1;
            vos_mem_copy(MulticastCyphers, pWpaIe->multicast_cipher, CSR_WPA_OUI_SIZE);
            cUnicastCyphers = (tANI_U8)(pWpaIe->unicast_cipher_count);
            cAuthSuites = (tANI_U8)(pWpaIe->auth_suite_count);

            //Check - Is requested Unicast Cipher supported by the BSS.
            fAcceptableCyphers = csrMatchWPAOUIIndex( pMac, pWpaIe->unicast_ciphers, cUnicastCyphers, 
                    csrGetOUIIndexFromCipher( enType ), Unicast );

            if( !fAcceptableCyphers ) break;


            //Unicast is supported. Pick the first matching Group cipher, if any.
            for( i = 0 ; i < pMCEncryption->numEntries ; i++ )
            {
                fAcceptableCyphers = csrMatchWPAOUIIndex( pMac, MulticastCyphers,  cMulticastCyphers, 
                            csrGetOUIIndexFromCipher( pMCEncryption->encryptionType[i]), Multicast );
                if(fAcceptableCyphers)
                {
                    break;
                }
            }
            if( !fAcceptableCyphers ) break;
            
            if( pNegotiatedMCCipher )
                *pNegotiatedMCCipher = pMCEncryption->encryptionType[i];

                /* Initializing with FALSE as it has TRUE value already */
            fAcceptableCyphers = FALSE;
            for (i = 0 ; i < pAuthType->numEntries; i++)
            {
            //Ciphers are supported, Match authentication algorithm and pick first matching authtype.
                if ( csrIsAuthWpa( pMac, pWpaIe->auth_suites, cAuthSuites, Authentication ) )
                {
                    if (eCSR_AUTH_TYPE_WPA == pAuthType->authType[i])
                    negAuthType = eCSR_AUTH_TYPE_WPA;
                }
                if ( (negAuthType == eCSR_AUTH_TYPE_UNKNOWN) && csrIsAuthWpaPsk( pMac, pWpaIe->auth_suites, cAuthSuites, Authentication ) )
                {
                    if (eCSR_AUTH_TYPE_WPA_PSK == pAuthType->authType[i])
                    negAuthType = eCSR_AUTH_TYPE_WPA_PSK;
                }
#ifdef FEATURE_WLAN_ESE
                if ( (negAuthType == eCSR_AUTH_TYPE_UNKNOWN) && csrIsEseCckmAuthWpa( pMac, pWpaIe->auth_suites, cAuthSuites, Authentication ) )
                {
                    if (eCSR_AUTH_TYPE_CCKM_WPA == pAuthType->authType[i])
                        negAuthType = eCSR_AUTH_TYPE_CCKM_WPA;
                }
#endif /* FEATURE_WLAN_ESE */

                // The 1st auth type in the APs WPA IE, to match stations connecting
                // profiles auth type will cause us to exit this loop
                // This is added as some APs advertise multiple akms in the WPA IE.
                if (eCSR_AUTH_TYPE_UNKNOWN != negAuthType)
                {
                        fAcceptableCyphers = TRUE;
                        break;
                    }
            } // for
            }
    }while(0);

    if ( fAcceptableCyphers )
    {
        if ( MulticastCypher )
        {
            vos_mem_copy((tANI_U8 **)MulticastCypher, Multicast, CSR_WPA_OUI_SIZE);
        }

        if ( UnicastCypher )
        {
            vos_mem_copy((tANI_U8 **)UnicastCypher, Unicast, CSR_WPA_OUI_SIZE);
        }

        if ( AuthSuite )
        {
            vos_mem_copy((tANI_U8 **)AuthSuite, Authentication, CSR_WPA_OUI_SIZE);
        }

        if( pNegotiatedAuthtype )
        {
            *pNegotiatedAuthtype = negAuthType;
        }
    }

    return( fAcceptableCyphers );
}



tANI_BOOLEAN csrIsWpaEncryptionMatch( tpAniSirGlobal pMac, tCsrAuthList *pAuthType, eCsrEncryptionType enType, tCsrEncryptionList *pEnMcType,
                                        tDot11fBeaconIEs *pIes, eCsrAuthType *pNegotiatedAuthtype, eCsrEncryptionType *pNegotiatedMCCipher )
{
    tANI_BOOLEAN fWpaMatch = eANI_BOOLEAN_FALSE;

        // See if the cyphers in the Bss description match with the settings in the profile.
    fWpaMatch = csrGetWpaCyphers( pMac, pAuthType, enType, pEnMcType, &pIes->WPA, NULL, NULL, NULL, pNegotiatedAuthtype, pNegotiatedMCCipher );

    return( fWpaMatch );
}


tANI_U8 csrConstructWpaIe( tHalHandle hHal, tCsrRoamProfile *pProfile, tSirBssDescription *pSirBssDesc, 
                           tDot11fBeaconIEs *pIes, tCsrWpaIe *pWpaIe )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_BOOLEAN fWpaMatch;
    tANI_U8 cbWpaIe = 0;
    tANI_U8 UnicastCypher[ CSR_WPA_OUI_SIZE ];
    tANI_U8 MulticastCypher[ CSR_WPA_OUI_SIZE ];
    tANI_U8 AuthSuite[ CSR_WPA_OUI_SIZE ];
    tCsrWpaAuthIe *pAuthSuite;
    tDot11fBeaconIEs *pIesLocal = pIes;

    do
    {
        if ( !csrIsProfileWpa( pProfile ) ) break;

        if( !pIesLocal && (!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pSirBssDesc, &pIesLocal))) )
        {
            break;
        }
        // See if the cyphers in the Bss description match with the settings in the profile.
        fWpaMatch = csrGetWpaCyphers( hHal, &pProfile->AuthType, pProfile->negotiatedUCEncryptionType, &pProfile->mcEncryptionType,
                                      &pIesLocal->WPA, UnicastCypher, MulticastCypher, AuthSuite, NULL, NULL );
        if ( !fWpaMatch ) break;

        pWpaIe->IeHeader.ElementID = SIR_MAC_WPA_EID;

        vos_mem_copy(pWpaIe->Oui, csrWpaOui[01], sizeof( pWpaIe->Oui ));

        pWpaIe->Version = CSR_WPA_VERSION_SUPPORTED;

        vos_mem_copy(pWpaIe->MulticastOui, MulticastCypher, sizeof( MulticastCypher ));

        pWpaIe->cUnicastCyphers = 1;

        vos_mem_copy(&pWpaIe->UnicastOui[ 0 ], UnicastCypher, sizeof( UnicastCypher ));

        pAuthSuite = (tCsrWpaAuthIe *)( &pWpaIe->UnicastOui[ pWpaIe->cUnicastCyphers ] );

        pAuthSuite->cAuthenticationSuites = 1;
        vos_mem_copy(&pAuthSuite->AuthOui[ 0 ], AuthSuite, sizeof( AuthSuite ));

        // The WPA capabilities follows the Auth Suite (two octects)--
        // this field is optional, and we always "send" zero, so just
        // remove it.  This is consistent with our assumptions in the
        // frames compiler; c.f. bug 15234:
        // http://gold.woodsidenet.com/bugzilla/show_bug.cgi?id=15234

        // Add in the fixed fields plus 1 Unicast cypher, less the IE Header length
        // Add in the size of the Auth suite (count plus a single OUI)
        pWpaIe->IeHeader.Length = sizeof( *pWpaIe ) - sizeof ( pWpaIe->IeHeader ) +
                                  sizeof( *pAuthSuite );

        // return the size of the IE header (total) constructed...
        cbWpaIe = pWpaIe->IeHeader.Length + sizeof( pWpaIe->IeHeader );

    } while( 0 );

    if( !pIes && pIesLocal )
    {
        //locally allocated
        vos_mem_free(pIesLocal);
    }

    return( cbWpaIe );
}


tANI_BOOLEAN csrGetWpaRsnIe( tHalHandle hHal, tANI_U8 *pIes, tANI_U32 len,
                             tANI_U8 *pWpaIe, tANI_U8 *pcbWpaIe, tANI_U8 *pRSNIe, tANI_U8 *pcbRSNIe)
{
    tDot11IEHeader *pIEHeader;
    tSirMacPropIE *pSirMacPropIE;
    tANI_U32 cbParsed;
    tANI_U32 cbIE;
    int cExpectedIEs = 0;
    int cFoundIEs = 0;
    int cbPropIETotal;

    pIEHeader = (tDot11IEHeader *)pIes;
    if(pWpaIe) cExpectedIEs++;
    if(pRSNIe) cExpectedIEs++;

    // bss description length includes all fields other than the length itself
    cbParsed  = 0;

    // Loop as long as there is data left in the IE of the Bss Description
    // and the number of Expected IEs is NOT found yet.
    while( ( (cbParsed + sizeof( *pIEHeader )) <= len ) && ( cFoundIEs < cExpectedIEs  ) )
    {
        cbIE = sizeof( *pIEHeader ) + pIEHeader->Length;

        if ( ( cbIE + cbParsed ) > len ) break;

        if ( ( pIEHeader->Length >= gCsrIELengthTable[ pIEHeader->ElementID ].min ) &&
             ( pIEHeader->Length <= gCsrIELengthTable[ pIEHeader->ElementID ].max ) )
        {
            switch( pIEHeader->ElementID )
            {
                // Parse the 221 (0xdd) Proprietary IEs here...
                // Note that the 221 IE is overloaded, containing the WPA IE, WMM/WME IE, and the
                // Airgo proprietary IE information.
                case SIR_MAC_WPA_EID:
                {
                    tANI_U32 aniOUI;
                    tANI_U8 *pOui = (tANI_U8 *)&aniOUI;

                    pOui++;
                    aniOUI = ANI_OUI;
                    aniOUI = i_ntohl( aniOUI );

                    pSirMacPropIE = ( tSirMacPropIE *)pIEHeader;
                    cbPropIETotal = pSirMacPropIE->length;

                    // Validate the ANI OUI is in the OUI field in the proprietary IE...
                    if ( ( pSirMacPropIE->length >= WNI_CFG_MANUFACTURER_OUI_LEN ) &&
                          pOui[ 0 ] == pSirMacPropIE->oui[ 0 ] &&
                          pOui[ 1 ] == pSirMacPropIE->oui[ 1 ] &&
                          pOui[ 2 ] == pSirMacPropIE->oui[ 2 ]  )
                    {
                    }
                    else
                    {
                        tCsrWpaIe     *pIe        = ( tCsrWpaIe *    )pIEHeader;

                        if(!pWpaIe || !pcbWpaIe) break;
                        // Check if this is a valid WPA IE.  Then check that the
                        // WPA OUI is in place and the version is one that we support.
                        if ( ( pIe->IeHeader.Length >= SIR_MAC_WPA_IE_MIN_LENGTH )   &&
                             ( vos_mem_compare( pIe->Oui, (void *)csrWpaOui[1],
                                                sizeof( pIe->Oui ) ) ) &&
                             ( pIe->Version <= CSR_WPA_VERSION_SUPPORTED ) )
                        {
                            vos_mem_copy(pWpaIe, pIe,
                                  pIe->IeHeader.Length + sizeof( pIe->IeHeader ));
                            *pcbWpaIe = pIe->IeHeader.Length + sizeof( pIe->IeHeader );
                            cFoundIEs++;

                            break;
                        }
                    }

                    break;
                }

                case SIR_MAC_RSN_EID:
                {
                    tCsrRSNIe *pIe;

                    if(!pcbRSNIe || !pRSNIe) break;
                    pIe = (tCsrRSNIe *)pIEHeader;

                    // Check the length of the RSN Ie to assure it is valid.  Then check that the
                    // version is one that we support.

                    if ( pIe->IeHeader.Length < SIR_MAC_RSN_IE_MIN_LENGTH ) break;
                    if ( pIe->Version > CSR_RSN_VERSION_SUPPORTED ) break;

                    cFoundIEs++;

                    // if there is enough room in the WpaIE passed in, then copy the Wpa IE into
                    // the buffer passed in.
                    if ( *pcbRSNIe < pIe->IeHeader.Length + sizeof( pIe->IeHeader ) ) break;
                    vos_mem_copy(pRSNIe, pIe,
                                 pIe->IeHeader.Length + sizeof( pIe->IeHeader ));
                    *pcbRSNIe = pIe->IeHeader.Length + sizeof( pIe->IeHeader );

                    break;
                }

                // Add support for other IE here...
                default:
                    break;
            }
        }

        cbParsed += cbIE;

        pIEHeader = (tDot11IEHeader *)( ((tANI_U8 *)pIEHeader) + cbIE );

    }

    // return a BOOL that tells if all of the IEs asked for were found...
    return( cFoundIEs == cExpectedIEs );
}


//If a WPAIE exists in the profile, just use it. Or else construct one from the BSS
//Caller allocated memory for pWpaIe and guarrantee it can contain a max length WPA IE
tANI_U8 csrRetrieveWpaIe( tHalHandle hHal, tCsrRoamProfile *pProfile, tSirBssDescription *pSirBssDesc, 
                          tDot11fBeaconIEs *pIes, tCsrWpaIe *pWpaIe )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_U8 cbWpaIe = 0;

    do
    {
        if ( !csrIsProfileWpa( pProfile ) ) break;
        if(pProfile->nWPAReqIELength && pProfile->pWPAReqIE)
        {
            if(SIR_MAC_WPA_IE_MAX_LENGTH >= pProfile->nWPAReqIELength)
            {
                cbWpaIe = (tANI_U8)pProfile->nWPAReqIELength;
                vos_mem_copy(pWpaIe, pProfile->pWPAReqIE, cbWpaIe);
            }
            else
            {
                smsLog(pMac, LOGW, "  csrRetrieveWpaIe detect invalid WPA IE length (%d) ", pProfile->nWPAReqIELength);
            }
        }
        else
        {
            cbWpaIe = csrConstructWpaIe(pMac, pProfile, pSirBssDesc, pIes, pWpaIe);
        }
    }while(0);

    return (cbWpaIe);
}


//If a RSNIE exists in the profile, just use it. Or else construct one from the BSS
//Caller allocated memory for pWpaIe and guarrantee it can contain a max length WPA IE
tANI_U8 csrRetrieveRsnIe( tHalHandle hHal, tANI_U32 sessionId, tCsrRoamProfile *pProfile, 
                         tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes, tCsrRSNIe *pRsnIe )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_U8 cbRsnIe = 0;

    do
    {
        if ( !csrIsProfileRSN( pProfile ) ) break;
#ifdef FEATURE_WLAN_LFR
        if (csrRoamIsFastRoamEnabled(pMac, sessionId))
        {
            // If "Legacy Fast Roaming" is enabled ALWAYS rebuild the RSN IE from 
            // scratch. So it contains the current PMK-IDs
            cbRsnIe = csrConstructRSNIe(pMac, sessionId, pProfile, pSirBssDesc, pIes, pRsnIe);
        }
        else 
#endif
        if(pProfile->nRSNReqIELength && pProfile->pRSNReqIE)
        {
            // If you have one started away, re-use it. 
            if(SIR_MAC_WPA_IE_MAX_LENGTH >= pProfile->nRSNReqIELength)
            {
                cbRsnIe = (tANI_U8)pProfile->nRSNReqIELength;
                vos_mem_copy(pRsnIe, pProfile->pRSNReqIE, cbRsnIe);
            }
            else
            {
                smsLog(pMac, LOGW, "  csrRetrieveRsnIe detect invalid RSN IE length (%d) ", pProfile->nRSNReqIELength);
            }
        }
        else
        {
            cbRsnIe = csrConstructRSNIe(pMac, sessionId, pProfile, pSirBssDesc, pIes, pRsnIe);
        }
    }while(0);

    return (cbRsnIe);
}


#ifdef FEATURE_WLAN_WAPI
//If a WAPI IE exists in the profile, just use it. Or else construct one from the BSS
//Caller allocated memory for pWapiIe and guarrantee it can contain a max length WAPI IE
tANI_U8 csrRetrieveWapiIe( tHalHandle hHal, tANI_U32 sessionId, 
                          tCsrRoamProfile *pProfile, tSirBssDescription *pSirBssDesc, 
                          tDot11fBeaconIEs *pIes, tCsrWapiIe *pWapiIe )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_U8 cbWapiIe = 0;

    do
    {
        if ( !csrIsProfileWapi( pProfile ) ) break;
        if(pProfile->nWAPIReqIELength && pProfile->pWAPIReqIE)
        {
            if(DOT11F_IE_WAPI_MAX_LEN >= pProfile->nWAPIReqIELength)
            {
                cbWapiIe = (tANI_U8)pProfile->nWAPIReqIELength;
                vos_mem_copy(pWapiIe, pProfile->pWAPIReqIE, cbWapiIe);
            }
            else
            {
                smsLog(pMac, LOGW, "  csrRetrieveWapiIe detect invalid WAPI IE length (%d) ", pProfile->nWAPIReqIELength);
            }
        }
        else
        {
            cbWapiIe = csrConstructWapiIe(pMac, sessionId, pProfile, pSirBssDesc, pIes, pWapiIe);
        }
    }while(0);

    return (cbWapiIe);
}
#endif /* FEATURE_WLAN_WAPI */

tANI_BOOLEAN csrSearchChannelListForTxPower(tHalHandle hHal, tSirBssDescription *pBssDescription, tCsrChannelSet *returnChannelGroup)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tListElem *pEntry;
    tANI_U16 i;
    tANI_U16 startingChannel;
    tANI_BOOLEAN found = FALSE;
    tCsrChannelSet *pChannelGroup;

    pEntry = csrLLPeekHead( &pMac->roam.channelList5G, LL_ACCESS_LOCK );

    while ( pEntry )
    {
        pChannelGroup = GET_BASE_ADDR( pEntry, tCsrChannelSet, channelListLink );
        startingChannel = pChannelGroup->firstChannel;
        for ( i = 0; i < pChannelGroup->numChannels; i++ )
        {
            if ( startingChannel + i * pChannelGroup->interChannelOffset == pBssDescription->channelId )
            {
                found = TRUE;
                break;
            }
        }

        if ( found )
        {
            vos_mem_copy(returnChannelGroup, pChannelGroup, sizeof(tCsrChannelSet));
            break;
        }
        else
        {
            pEntry = csrLLNext( &pMac->roam.channelList5G, pEntry, LL_ACCESS_LOCK );
        }
    }

    return( found );
}

tANI_BOOLEAN csrRatesIsDot11Rate11bSupportedRate( tANI_U8 dot11Rate )
{
    tANI_BOOLEAN fSupported = FALSE;
    tANI_U16 nonBasicRate = (tANI_U16)( BITS_OFF( dot11Rate, CSR_DOT11_BASIC_RATE_MASK ) );

    switch ( nonBasicRate )
    {
        case eCsrSuppRate_1Mbps:
        case eCsrSuppRate_2Mbps:
        case eCsrSuppRate_5_5Mbps:
        case eCsrSuppRate_11Mbps:
            fSupported = TRUE;
            break;

        default:
            break;
    }

    return( fSupported );
}

tANI_BOOLEAN csrRatesIsDot11Rate11aSupportedRate( tANI_U8 dot11Rate )
{
    tANI_BOOLEAN fSupported = FALSE;
    tANI_U16 nonBasicRate = (tANI_U16)( BITS_OFF( dot11Rate, CSR_DOT11_BASIC_RATE_MASK ) );

    switch ( nonBasicRate )
    {
        case eCsrSuppRate_6Mbps:
        case eCsrSuppRate_9Mbps:
        case eCsrSuppRate_12Mbps:
        case eCsrSuppRate_18Mbps:
        case eCsrSuppRate_24Mbps:
        case eCsrSuppRate_36Mbps:
        case eCsrSuppRate_48Mbps:
        case eCsrSuppRate_54Mbps:
            fSupported = TRUE;
            break;

        default:
            break;
    }

    return( fSupported );
}



tAniEdType csrTranslateEncryptTypeToEdType( eCsrEncryptionType EncryptType )
{
    tAniEdType edType;

    switch ( EncryptType )
    {
        default:
        case eCSR_ENCRYPT_TYPE_NONE:
            edType = eSIR_ED_NONE;
            break;

        case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
        case eCSR_ENCRYPT_TYPE_WEP40:
            edType = eSIR_ED_WEP40;
            break;

        case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
        case eCSR_ENCRYPT_TYPE_WEP104:
            edType = eSIR_ED_WEP104;
            break;

        case eCSR_ENCRYPT_TYPE_TKIP:
            edType = eSIR_ED_TKIP;
            break;

        case eCSR_ENCRYPT_TYPE_AES:
            edType = eSIR_ED_CCMP;
            break;
#ifdef FEATURE_WLAN_WAPI
        case eCSR_ENCRYPT_TYPE_WPI:
            edType = eSIR_ED_WPI;
            break ;
#endif
#ifdef WLAN_FEATURE_11W
        //11w BIP
        case eCSR_ENCRYPT_TYPE_AES_CMAC:
            edType = eSIR_ED_AES_128_CMAC;
            break;
#endif
    }

    return( edType );
}


//pIes can be NULL
tANI_BOOLEAN csrValidateWep( tpAniSirGlobal pMac, eCsrEncryptionType ucEncryptionType, 
                             tCsrAuthList *pAuthList, tCsrEncryptionList *pMCEncryptionList, 
                             eCsrAuthType *pNegotiatedAuthType, eCsrEncryptionType *pNegotiatedMCEncryption,
                             tSirBssDescription *pSirBssDesc, tDot11fBeaconIEs *pIes )
{
    tANI_U32 idx;
    tANI_BOOLEAN fMatch = FALSE;
    eCsrAuthType negotiatedAuth = eCSR_AUTH_TYPE_OPEN_SYSTEM;
    eCsrEncryptionType negotiatedMCCipher = eCSR_ENCRYPT_TYPE_UNKNOWN;

    //This function just checks whether HDD is giving correct values for Multicast cipher and Auth.
    
    do
    {
        //If privacy bit is not set, consider no match
        if ( !csrIsPrivacy( pSirBssDesc ) ) break;

        for( idx = 0; idx < pMCEncryptionList->numEntries; idx++ )
        {
            switch( pMCEncryptionList->encryptionType[idx] )
            {
                case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
                case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
                case eCSR_ENCRYPT_TYPE_WEP40:
                case eCSR_ENCRYPT_TYPE_WEP104:
                    /* Multicast list may contain WEP40/WEP104. Check whether it matches UC.
                    */
                    if( ucEncryptionType == pMCEncryptionList->encryptionType[idx] )
                    {
                        fMatch = TRUE;
                        negotiatedMCCipher = pMCEncryptionList->encryptionType[idx];
                    }
                    break;
                default:
                    fMatch = FALSE;
                    break;
            }
            if(fMatch) break; 
        }

        if(!fMatch) break;

        for( idx = 0; idx < pAuthList->numEntries; idx++ )
        {
            switch( pAuthList->authType[idx] )
            {
                case eCSR_AUTH_TYPE_OPEN_SYSTEM:
                case eCSR_AUTH_TYPE_SHARED_KEY:
                case eCSR_AUTH_TYPE_AUTOSWITCH:
                    fMatch = TRUE;
                    negotiatedAuth = pAuthList->authType[idx];
                    break;
                default:
                    fMatch = FALSE;
            }
            if (fMatch) break;
        }

        if(!fMatch) break;
        //In case of WPA / WPA2, check whether it supports WEP as well
        if(pIes)
        {
            //Prepare the encryption type for WPA/WPA2 functions
            if( eCSR_ENCRYPT_TYPE_WEP40_STATICKEY == ucEncryptionType )
            {
                ucEncryptionType = eCSR_ENCRYPT_TYPE_WEP40;
            }
            else if( eCSR_ENCRYPT_TYPE_WEP104 == ucEncryptionType )
            {
                ucEncryptionType = eCSR_ENCRYPT_TYPE_WEP104;
            }
            //else we can use the encryption type directly
            if ( pIes->WPA.present )
            {
                fMatch = vos_mem_compare(pIes->WPA.multicast_cipher,
                                         csrWpaOui[csrGetOUIIndexFromCipher( ucEncryptionType )],
                                         CSR_WPA_OUI_SIZE);
                if( fMatch ) break;
            }
            if ( pIes->RSN.present )
            {
                fMatch = vos_mem_compare(pIes->RSN.gp_cipher_suite,
                                         csrRSNOui[csrGetOUIIndexFromCipher( ucEncryptionType )],
                                         CSR_RSN_OUI_SIZE);
            }
        }

    }while(0);

    if( fMatch )
    {
        if( pNegotiatedAuthType )
            *pNegotiatedAuthType = negotiatedAuth;

        if( pNegotiatedMCEncryption )
            *pNegotiatedMCEncryption = negotiatedMCCipher;
    }    


    return fMatch;
}


//pIes shall contain IEs from pSirBssDesc. It shall be returned from function csrGetParsedBssDescriptionIEs
tANI_BOOLEAN csrIsSecurityMatch( tHalHandle hHal, tCsrAuthList *authType,
                                 tCsrEncryptionList *pUCEncryptionType,
                                 tCsrEncryptionList *pMCEncryptionType,
                                 tANI_BOOLEAN *pMFPEnabled,
                                 tANI_U8 *pMFPRequired, tANI_U8 *pMFPCapable,
                                 tSirBssDescription *pSirBssDesc,
                                 tDot11fBeaconIEs *pIes,
                                 eCsrAuthType *negotiatedAuthtype,
                                 eCsrEncryptionType *negotiatedUCCipher,
                                 eCsrEncryptionType *negotiatedMCCipher )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_BOOLEAN fMatch = FALSE;
    tANI_U8 i,idx;
    eCsrEncryptionType mcCipher = eCSR_ENCRYPT_TYPE_UNKNOWN, ucCipher = eCSR_ENCRYPT_TYPE_UNKNOWN;
    eCsrAuthType negAuthType = eCSR_AUTH_TYPE_UNKNOWN;

    for( i = 0 ; ((i < pUCEncryptionType->numEntries) && (!fMatch)) ; i++ )
    {
        ucCipher = pUCEncryptionType->encryptionType[i];
        // If the Bss description shows the Privacy bit is on, then we must have some sort of encryption configured
        // for the profile to work.  Don't attempt to join networks with Privacy bit set when profiles say NONE for
        // encryption type.
        switch ( ucCipher )
        {
            case eCSR_ENCRYPT_TYPE_NONE:
                {
                    // for NO encryption, if the Bss description has the Privacy bit turned on, then encryption is
                    // required so we have to reject this Bss.
                    if ( csrIsPrivacy( pSirBssDesc ) )
                    {
                        fMatch = FALSE;
                    }
                    else
                    {
                        fMatch = TRUE;
                    }

                    if ( fMatch )
                    {
                        fMatch = FALSE;
                        //Check Multicast cipher requested and Auth type requested.
                        for( idx = 0 ; idx < pMCEncryptionType->numEntries ; idx++ )
                        {
                            if( eCSR_ENCRYPT_TYPE_NONE == pMCEncryptionType->encryptionType[idx] )
                            {
                                fMatch = TRUE; //Multicast can only be none.
                                mcCipher = pMCEncryptionType->encryptionType[idx];
                                break;
                            }
                        }
                        if (!fMatch) break;

                        fMatch = FALSE;
                        //Check Auth list. It should contain AuthOpen.
                        for( idx = 0 ; idx < authType->numEntries ; idx++ )
                        {
                            if( eCSR_AUTH_TYPE_OPEN_SYSTEM == authType->authType[idx] )
                            {
                               fMatch = TRUE;
                               negAuthType = eCSR_AUTH_TYPE_OPEN_SYSTEM;
                               break;
                            }
                        } 
                        if (!fMatch) break;

                    }
                    break;
                }

            case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
            case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
                // !! might want to check for WEP keys set in the Profile.... ?
                // !! don't need to have the privacy bit in the Bss description.  Many AP policies make legacy
                // encryption 'optional' so we don't know if we can associate or not.  The AP will reject if
                // encryption is not allowed without the Privacy bit turned on.
                fMatch = csrValidateWep( pMac, ucCipher, authType, pMCEncryptionType, &negAuthType, &mcCipher, pSirBssDesc, pIes);

                break;

                // these are all of the WPA encryption types...
            case eCSR_ENCRYPT_TYPE_WEP40:
            case eCSR_ENCRYPT_TYPE_WEP104:
                fMatch = csrValidateWep( pMac, ucCipher, authType, pMCEncryptionType, &negAuthType, &mcCipher, pSirBssDesc, pIes);
                break;

            case eCSR_ENCRYPT_TYPE_TKIP:
            case eCSR_ENCRYPT_TYPE_AES:
                {
                    if(pIes)
                    {
                        // First check if there is a RSN match
                        fMatch = csrIsRSNMatch( pMac, authType, ucCipher,
                                                pMCEncryptionType, pMFPEnabled,
                                                pMFPRequired, pMFPCapable,
                                                pIes, &negAuthType, &mcCipher );
                        if( !fMatch )
                        {
                            // If not RSN, then check if there is a WPA match
                            fMatch = csrIsWpaEncryptionMatch( pMac, authType, ucCipher, pMCEncryptionType, pIes, 
                                                              &negAuthType, &mcCipher );
                        }
                    }
                    else
                    {
                        fMatch = FALSE;
                    }
                    break;
                }
#ifdef FEATURE_WLAN_WAPI
           case eCSR_ENCRYPT_TYPE_WPI://WAPI
               {
                   if(pIes)
                   {
                       fMatch = csrIsWapiMatch( hHal, authType, ucCipher, pMCEncryptionType, pIes, &negAuthType, &mcCipher );
                   }
                   else
                   {
                       fMatch = FALSE;
                   }
                   break;
               }
#endif /* FEATURE_WLAN_WAPI */
            case eCSR_ENCRYPT_TYPE_ANY: 
            default: 
            {
                tANI_BOOLEAN fMatchAny = eANI_BOOLEAN_FALSE;

                fMatch = eANI_BOOLEAN_TRUE;
                //It is allowed to match anything. Try the more secured ones first.
                if(pIes)
                {
                    //Check AES first
                    ucCipher = eCSR_ENCRYPT_TYPE_AES;
                    fMatchAny = csrIsRSNMatch( hHal, authType, ucCipher,
                                               pMCEncryptionType, pMFPEnabled,
                                               pMFPRequired, pMFPCapable, pIes,
                                               &negAuthType, &mcCipher );
                    if(!fMatchAny)
                    {
                        //Check TKIP
                        ucCipher = eCSR_ENCRYPT_TYPE_TKIP;
                        fMatchAny = csrIsRSNMatch( hHal, authType, ucCipher,
                                                   pMCEncryptionType,
                                                   pMFPEnabled, pMFPRequired,
                                                   pMFPCapable, pIes,
                                                   &negAuthType, &mcCipher );
                    }
#ifdef FEATURE_WLAN_WAPI
                    if(!fMatchAny)
                    {
                        //Check WAPI
                        ucCipher = eCSR_ENCRYPT_TYPE_WPI;
                        fMatchAny = csrIsWapiMatch( hHal, authType, ucCipher, pMCEncryptionType, pIes, &negAuthType, &mcCipher );
                    }
#endif /* FEATURE_WLAN_WAPI */
                }
                if(!fMatchAny)
                {
                    ucCipher = eCSR_ENCRYPT_TYPE_WEP104;
                    if(!csrValidateWep( pMac, ucCipher, authType, pMCEncryptionType, &negAuthType, &mcCipher, pSirBssDesc, pIes))
                    {
                        ucCipher = eCSR_ENCRYPT_TYPE_WEP40;
                        if(!csrValidateWep( pMac, ucCipher, authType, pMCEncryptionType, &negAuthType, &mcCipher, pSirBssDesc, pIes))
                        {
                            ucCipher = eCSR_ENCRYPT_TYPE_WEP104_STATICKEY;
                            if(!csrValidateWep( pMac, ucCipher, authType, pMCEncryptionType, &negAuthType, &mcCipher, pSirBssDesc, pIes))
                            {
                                ucCipher = eCSR_ENCRYPT_TYPE_WEP40_STATICKEY;
                                if(!csrValidateWep( pMac, ucCipher, authType, pMCEncryptionType, &negAuthType, &mcCipher, pSirBssDesc, pIes))
                                {
                                    //It must be open and no encryption
                                    if ( csrIsPrivacy( pSirBssDesc ) )
                                    {
                                        //This is not right
                                        fMatch = eANI_BOOLEAN_FALSE;
                                    }
                                    else
                                    {
                                        negAuthType = eCSR_AUTH_TYPE_OPEN_SYSTEM;
                                        mcCipher = eCSR_ENCRYPT_TYPE_NONE;
                                        ucCipher = eCSR_ENCRYPT_TYPE_NONE;
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            }
        }

    }

    if( fMatch ) 
    {
        if( negotiatedUCCipher )
            *negotiatedUCCipher = ucCipher;
   
        if( negotiatedMCCipher )
            *negotiatedMCCipher = mcCipher;
   
        if( negotiatedAuthtype )
            *negotiatedAuthtype = negAuthType;
    }

    return( fMatch );
}


tANI_BOOLEAN csrIsSsidMatch( tpAniSirGlobal pMac, tANI_U8 *ssid1, tANI_U8 ssid1Len, tANI_U8 *bssSsid,
                            tANI_U8 bssSsidLen, tANI_BOOLEAN fSsidRequired )
{
    tANI_BOOLEAN fMatch = FALSE;

    do {

        // There are a few special cases.  If the Bss description has a Broadcast SSID,
        // then our Profile must have a single SSID without Wildcards so we can program
        // the SSID.
        // SSID could be suppressed in beacons. In that case SSID IE has valid length
        // but the SSID value is all NULL characters. That condition is trated same
        // as NULL SSID
        if ( csrIsNULLSSID( bssSsid, bssSsidLen ) )
        {
            if ( eANI_BOOLEAN_FALSE == fSsidRequired )
            {
                fMatch = TRUE;
            }
            break;
        }

        // Check for the specification of the Broadcast SSID at the beginning of the list.
        // If specified, then all SSIDs are matches (broadcast SSID means accept all SSIDs).
        if ( ssid1Len == 0 )
        {
            fMatch = TRUE;
            break;
        }

        if(ssid1Len != bssSsidLen) break;
        if (vos_mem_compare(bssSsid, ssid1, bssSsidLen))
        {
            fMatch = TRUE;
            break;
        }

    } while( 0 );

    return( fMatch );
}


//Null ssid means match
tANI_BOOLEAN csrIsSsidInList( tHalHandle hHal, tSirMacSSid *pSsid, tCsrSSIDs *pSsidList )
{
    tANI_BOOLEAN fMatch = FALSE;
    tANI_U32 i;

    if ( pSsidList && pSsid )
    {
        for(i = 0; i < pSsidList->numOfSSIDs; i++)
        {
            if(csrIsNULLSSID(pSsidList->SSIDList[i].SSID.ssId, pSsidList->SSIDList[i].SSID.length) ||
              ((pSsidList->SSIDList[i].SSID.length == pSsid->length) &&
               vos_mem_compare(pSsid->ssId, pSsidList->SSIDList[i].SSID.ssId, pSsid->length)))
            {
                fMatch = TRUE;
                break;
            }
        }
    }

    return (fMatch);
}

//like to use sirCompareMacAddr
tANI_BOOLEAN csrIsMacAddressZero( tpAniSirGlobal pMac, tCsrBssid *pMacAddr )
{
    tANI_U8 bssid[WNI_CFG_BSSID_LEN] = {0, 0, 0, 0, 0, 0};

    return (vos_mem_compare(bssid, pMacAddr, WNI_CFG_BSSID_LEN));
}

//like to use sirCompareMacAddr
tANI_BOOLEAN csrIsMacAddressBroadcast( tpAniSirGlobal pMac, tCsrBssid *pMacAddr )
{
    tANI_U8 bssid[WNI_CFG_BSSID_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    return(vos_mem_compare(bssid, pMacAddr, WNI_CFG_BSSID_LEN));
}


//like to use sirCompareMacAddr
tANI_BOOLEAN csrIsMacAddressEqual( tpAniSirGlobal pMac, tCsrBssid *pMacAddr1, tCsrBssid *pMacAddr2 )
{
    return(vos_mem_compare(pMacAddr1, pMacAddr2, sizeof(tCsrBssid)));
}


tANI_BOOLEAN csrIsBssidMatch( tHalHandle hHal, tCsrBssid *pProfBssid, tCsrBssid *BssBssid )
{
    tANI_BOOLEAN fMatch = FALSE;
    tCsrBssid ProfileBssid;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    // for efficiency of the MAC_ADDRESS functions, move the
    // Bssid's into MAC_ADDRESS structs.
    vos_mem_copy(&ProfileBssid, pProfBssid, sizeof(tCsrBssid));

    do {

        // Give the profile the benefit of the doubt... accept either all 0 or
        // the real broadcast Bssid (all 0xff) as broadcast Bssids (meaning to
        // match any Bssids).
        if ( csrIsMacAddressZero( pMac, &ProfileBssid ) ||
             csrIsMacAddressBroadcast( pMac, &ProfileBssid ) )
        {
             fMatch = TRUE;
             break;
        }

        if ( csrIsMacAddressEqual( pMac, BssBssid, &ProfileBssid ) )
        {
            fMatch = TRUE;
            break;
        }

    } while( 0 );

    return( fMatch );
}


tANI_BOOLEAN csrIsBSSTypeMatch(eCsrRoamBssType bssType1, eCsrRoamBssType bssType2)
{
    if((eCSR_BSS_TYPE_ANY != bssType1 && eCSR_BSS_TYPE_ANY != bssType2) && (bssType1 != bssType2))
        return eANI_BOOLEAN_FALSE;
    else
        return eANI_BOOLEAN_TRUE;
}


tANI_BOOLEAN csrIsBssTypeIBSS(eCsrRoamBssType bssType)
{
    return((tANI_BOOLEAN)(eCSR_BSS_TYPE_START_IBSS == bssType || eCSR_BSS_TYPE_IBSS == bssType));
}

tANI_BOOLEAN csrIsBssTypeWDS(eCsrRoamBssType bssType)
{
    return((tANI_BOOLEAN)(eCSR_BSS_TYPE_WDS_STA == bssType || eCSR_BSS_TYPE_WDS_AP == bssType));
}

tANI_BOOLEAN csrIsBSSTypeCapsMatch( eCsrRoamBssType bssType, tSirBssDescription *pSirBssDesc )
{
    tANI_BOOLEAN fMatch = TRUE;

    do
    {
        switch( bssType )
        {
            case eCSR_BSS_TYPE_ANY:
                break;

            case eCSR_BSS_TYPE_INFRASTRUCTURE:
            case eCSR_BSS_TYPE_WDS_STA:
                if( !csrIsInfraBssDesc( pSirBssDesc ) )
                    fMatch = FALSE;

                break;

            case eCSR_BSS_TYPE_IBSS:
            case eCSR_BSS_TYPE_START_IBSS:
                if( !csrIsIbssBssDesc( pSirBssDesc ) )
                    fMatch = FALSE;

                break;

            case eCSR_BSS_TYPE_WDS_AP: //For WDS AP, no need to match anything
            default:
                fMatch = FALSE;
                break;
        }
    }
    while( 0 );


    return( fMatch );
}

static tANI_BOOLEAN csrIsCapabilitiesMatch( tpAniSirGlobal pMac, eCsrRoamBssType bssType, tSirBssDescription *pSirBssDesc )
{
  return( csrIsBSSTypeCapsMatch( bssType, pSirBssDesc ) );
}



static tANI_BOOLEAN csrIsSpecificChannelMatch( tpAniSirGlobal pMac, tSirBssDescription *pSirBssDesc, tANI_U8 Channel )
{
    tANI_BOOLEAN fMatch = TRUE;

    do
    {
        // if the channel is ANY, then always match...
        if ( eCSR_OPERATING_CHANNEL_ANY == Channel ) break;
        if ( Channel == pSirBssDesc->channelId ) break;

        // didn't match anything.. so return NO match
        fMatch = FALSE;

    } while( 0 );

    return( fMatch );
}


tANI_BOOLEAN csrIsChannelBandMatch( tpAniSirGlobal pMac, tANI_U8 channelId, tSirBssDescription *pSirBssDesc )
{
    tANI_BOOLEAN fMatch = TRUE;

    do
    {
        // if the profile says Any channel AND the global settings says ANY channel, then we
        // always match...
        if ( eCSR_OPERATING_CHANNEL_ANY == channelId ) break;

        if ( eCSR_OPERATING_CHANNEL_ANY != channelId )
        {
            fMatch = csrIsSpecificChannelMatch( pMac, pSirBssDesc, channelId );
        }

    } while( 0 );

    return( fMatch );
}


/**
 * \brief Enquire as to whether a given rate is supported by the
 * adapter as currently configured
 *
 *
 * \param nRate A rate in units of 500kbps
 *
 * \return TRUE if  the adapter is currently capable  of supporting this
 * rate, FALSE else
 *
 *
 * The rate encoding  is just as in 802.11  Information Elements, except
 * that the high bit is \em  not interpreted as indicating a Basic Rate,
 * and proprietary rates are allowed, too.
 *
 * Note  that if the  adapter's dot11Mode  is g,  we don't  restrict the
 * rates.  According to hwReadEepromParameters, this will happen when:
 *
 *   ... the  card is  configured for ALL  bands through  the property
 *   page.  If this occurs, and the card is not an ABG card ,then this
 *   code  is  setting the  dot11Mode  to  assume  the mode  that  the
 *   hardware can support.   For example, if the card  is an 11BG card
 *   and we  are configured to support  ALL bands, then  we change the
 *   dot11Mode  to 11g  because  ALL in  this  case is  only what  the
 *   hardware can support.
 *
 *
 */

static tANI_BOOLEAN csrIsAggregateRateSupported( tpAniSirGlobal pMac, tANI_U16 rate )
{
    tANI_BOOLEAN fSupported = eANI_BOOLEAN_FALSE;
    tANI_U16 idx, newRate;

    //In case basic rate flag is set
    newRate = BITS_OFF(rate, CSR_DOT11_BASIC_RATE_MASK);
    if ( eCSR_CFG_DOT11_MODE_11A == pMac->roam.configParam.uCfgDot11Mode )
    {
        switch ( newRate )
        {
        case eCsrSuppRate_6Mbps:
        case eCsrSuppRate_9Mbps:
        case eCsrSuppRate_12Mbps:
        case eCsrSuppRate_18Mbps:
        case eCsrSuppRate_24Mbps:
        case eCsrSuppRate_36Mbps:
        case eCsrSuppRate_48Mbps:
        case eCsrSuppRate_54Mbps:
            fSupported = TRUE;
            break;
        default:
            fSupported = FALSE;
            break;
        }

    }
    else if( eCSR_CFG_DOT11_MODE_11B == pMac->roam.configParam.uCfgDot11Mode )
    {
        switch ( newRate )
        {
        case eCsrSuppRate_1Mbps:
        case eCsrSuppRate_2Mbps:
        case eCsrSuppRate_5_5Mbps:
        case eCsrSuppRate_11Mbps:
            fSupported = TRUE;
            break;
        default:
            fSupported = FALSE;
            break;
        }
    }
    else if ( !pMac->roam.configParam.ProprietaryRatesEnabled )
    {

        switch ( newRate )
        {
        case eCsrSuppRate_1Mbps:
        case eCsrSuppRate_2Mbps:
        case eCsrSuppRate_5_5Mbps:
        case eCsrSuppRate_6Mbps:
        case eCsrSuppRate_9Mbps:
        case eCsrSuppRate_11Mbps:
        case eCsrSuppRate_12Mbps:
        case eCsrSuppRate_18Mbps:
        case eCsrSuppRate_24Mbps:
        case eCsrSuppRate_36Mbps:
        case eCsrSuppRate_48Mbps:
        case eCsrSuppRate_54Mbps:
            fSupported = TRUE;
            break;
        default:
            fSupported = FALSE;
            break;
        }

    }
    else {

        if ( eCsrSuppRate_1Mbps   == newRate ||
             eCsrSuppRate_2Mbps   == newRate ||
             eCsrSuppRate_5_5Mbps == newRate ||
             eCsrSuppRate_11Mbps  == newRate )
        {
            fSupported = TRUE;
        }
        else {
            idx = 0x1;

            switch ( newRate )
            {
            case eCsrSuppRate_6Mbps:
                fSupported = gPhyRatesSuppt[0][idx];
                break;
            case eCsrSuppRate_9Mbps:
                fSupported = gPhyRatesSuppt[1][idx];
                break;
            case eCsrSuppRate_12Mbps:
                fSupported = gPhyRatesSuppt[2][idx];
                break;
            case eCsrSuppRate_18Mbps:
                fSupported = gPhyRatesSuppt[3][idx];
                break;
            case eCsrSuppRate_20Mbps:
                fSupported = gPhyRatesSuppt[4][idx];
                break;
            case eCsrSuppRate_24Mbps:
                fSupported = gPhyRatesSuppt[5][idx];
                break;
            case eCsrSuppRate_36Mbps:
                fSupported = gPhyRatesSuppt[6][idx];
                break;
            case eCsrSuppRate_40Mbps:
                fSupported = gPhyRatesSuppt[7][idx];
                break;
            case eCsrSuppRate_42Mbps:
                fSupported = gPhyRatesSuppt[8][idx];
                break;
            case eCsrSuppRate_48Mbps:
                fSupported = gPhyRatesSuppt[9][idx];
                break;
            case eCsrSuppRate_54Mbps:
                fSupported = gPhyRatesSuppt[10][idx];
                break;
            case eCsrSuppRate_72Mbps:
                fSupported = gPhyRatesSuppt[11][idx];
                break;
            case eCsrSuppRate_80Mbps:
                fSupported = gPhyRatesSuppt[12][idx];
                break;
            case eCsrSuppRate_84Mbps:
                fSupported = gPhyRatesSuppt[13][idx];
                break;
            case eCsrSuppRate_96Mbps:
                fSupported = gPhyRatesSuppt[14][idx];
                break;
            case eCsrSuppRate_108Mbps:
                fSupported = gPhyRatesSuppt[15][idx];
                break;
            case eCsrSuppRate_120Mbps:
                fSupported = gPhyRatesSuppt[16][idx];
                break;
            case eCsrSuppRate_126Mbps:
                fSupported = gPhyRatesSuppt[17][idx];
                break;
            case eCsrSuppRate_144Mbps:
                fSupported = gPhyRatesSuppt[18][idx];
                break;
            case eCsrSuppRate_160Mbps:
                fSupported = gPhyRatesSuppt[19][idx];
                break;
            case eCsrSuppRate_168Mbps:
                fSupported = gPhyRatesSuppt[20][idx];
                break;
            case eCsrSuppRate_192Mbps:
                fSupported = gPhyRatesSuppt[21][idx];
                break;
            case eCsrSuppRate_216Mbps:
                fSupported = gPhyRatesSuppt[22][idx];
                break;
            case eCsrSuppRate_240Mbps:
                fSupported = gPhyRatesSuppt[23][idx];
                break;
            default:
                fSupported = FALSE;
                break;
            }
        }
    }

    return fSupported;
}



static tANI_BOOLEAN csrIsRateSetMatch( tpAniSirGlobal pMac,
                                     tDot11fIESuppRates *pBssSuppRates,
                                     tDot11fIEExtSuppRates *pBssExtSuppRates )
{
    tANI_BOOLEAN fMatch = TRUE;
    tANI_U32 i;


    // Validate that all of the Basic rates advertised in the Bss description are supported.
    if ( pBssSuppRates )
    {
        for( i = 0; i < pBssSuppRates->num_rates; i++ )
        {
            if ( CSR_IS_BASIC_RATE( pBssSuppRates->rates[ i ] ) )
            {
                if ( !csrIsAggregateRateSupported( pMac, pBssSuppRates->rates[ i ] ) )
                {
                    fMatch = FALSE;
                    break;
                }
            }
        }
    }

    if ( fMatch && pBssExtSuppRates )
    {
        for( i = 0; i < pBssExtSuppRates->num_rates; i++ )
        {
            if ( CSR_IS_BASIC_RATE( pBssExtSuppRates->rates[ i ] ) )
            {
                if ( !csrIsAggregateRateSupported( pMac, pBssExtSuppRates->rates[ i ] ) )
                {
                    fMatch = FALSE;
                    break;
                }
            }
        }
    }

    return( fMatch );

}


//ppIes can be NULL. If caller want to get the *ppIes allocated by this function, pass in *ppIes = NULL
tANI_BOOLEAN csrMatchBSS( tHalHandle hHal, tSirBssDescription *pBssDesc, tCsrScanResultFilter *pFilter, 
                          eCsrAuthType *pNegAuth, eCsrEncryptionType *pNegUc, eCsrEncryptionType *pNegMc,
                          tDot11fBeaconIEs **ppIes)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_BOOLEAN fRC = eANI_BOOLEAN_FALSE, fCheck;
    tANI_U32 i;
    tDot11fBeaconIEs *pIes = NULL;
    tANI_U8 *pb;

    do {
        if( ( NULL == ppIes ) || ( *ppIes ) == NULL )
        {
            //If no IEs passed in, get our own.
            if(!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pBssDesc, &pIes)))
            {
                break;
            }
        }
        else
        {
            //Save the one pass in for local use
            pIes = *ppIes;
        }
        
        //Check if caller wants P2P
        fCheck = (!pFilter->p2pResult || pIes->P2PBeaconProbeRes.present);
        if(!fCheck) break;

        if(pIes->SSID.present)
        {
            for(i = 0; i < pFilter->SSIDs.numOfSSIDs; i++)
            {
                fCheck = csrIsSsidMatch( pMac, pFilter->SSIDs.SSIDList[i].SSID.ssId, pFilter->SSIDs.SSIDList[i].SSID.length,
                                        pIes->SSID.ssid,
                                        pIes->SSID.num_ssid, eANI_BOOLEAN_TRUE );
                if ( fCheck ) break;
            }
            if(!fCheck) break;
        }
        fCheck = eANI_BOOLEAN_TRUE;
        for(i = 0; i < pFilter->BSSIDs.numOfBSSIDs; i++)
        {
            fCheck = csrIsBssidMatch( pMac, (tCsrBssid *)&pFilter->BSSIDs.bssid[i], (tCsrBssid *)pBssDesc->bssId );
            if ( fCheck ) break;

            if (pFilter->p2pResult && pIes->P2PBeaconProbeRes.present)
            {
               fCheck = csrIsBssidMatch( pMac, (tCsrBssid *)&pFilter->BSSIDs.bssid[i], 
                              (tCsrBssid *)pIes->P2PBeaconProbeRes.P2PDeviceInfo.P2PDeviceAddress );

               if ( fCheck ) break;
            }
        }
        if(!fCheck) break;

        fCheck = eANI_BOOLEAN_TRUE;
        for(i = 0; i < pFilter->ChannelInfo.numOfChannels; i++)
        {
            fCheck = csrIsChannelBandMatch( pMac, pFilter->ChannelInfo.ChannelList[i], pBssDesc );
            if ( fCheck ) break;
        }
        if(!fCheck)
            break;
#if defined WLAN_FEATURE_VOWIFI
        /* If this is for measurement filtering */
        if( pFilter->fMeasurement )
        {
           fRC = eANI_BOOLEAN_TRUE;
           break;
        }
#endif
        if ( !csrIsPhyModeMatch( pMac, pFilter->phyMode, pBssDesc, NULL, NULL, pIes ) ) break;
        if ( (!pFilter->bWPSAssociation) && (!pFilter->bOSENAssociation) &&
#ifdef WLAN_FEATURE_11W
             !csrIsSecurityMatch( pMac, &pFilter->authType,
                                  &pFilter->EncryptionType,
                                  &pFilter->mcEncryptionType,
                                  &pFilter->MFPEnabled,
                                  &pFilter->MFPRequired,
                                  &pFilter->MFPCapable,
                                  pBssDesc, pIes, pNegAuth,
                                  pNegUc, pNegMc )
#else
             !csrIsSecurityMatch( pMac, &pFilter->authType,
                                  &pFilter->EncryptionType,
                                  &pFilter->mcEncryptionType,
                                  NULL, NULL, NULL,
                                  pBssDesc, pIes, pNegAuth,
                                  pNegUc, pNegMc )
#endif
                                                   ) break;
        if ( !csrIsCapabilitiesMatch( pMac, pFilter->BSSType, pBssDesc ) ) break;
        if ( !csrIsRateSetMatch( pMac, &pIes->SuppRates, &pIes->ExtSuppRates ) ) break;
        //Tush-QoS: validate first if asked for APSD or WMM association
        if ( (eCsrRoamWmmQbssOnly == pMac->roam.configParam.WMMSupportMode) &&
             !CSR_IS_QOS_BSS(pIes) )
             break;
        //Check country. check even when pb is NULL because we may want to make sure
        //AP has a country code in it if fEnforceCountryCodeMatch is set.
        pb = ( pFilter->countryCode[0] ) ? ( pFilter->countryCode) : NULL;

        fCheck = csrMatchCountryCode( pMac, pb, pIes );
        if(!fCheck)
            break;

#ifdef WLAN_FEATURE_VOWIFI_11R
        if (pFilter->MDID.mdiePresent)
        {
            if (pBssDesc->mdiePresent)
            {
                if (pFilter->MDID.mobilityDomain != (pBssDesc->mdie[1] << 8 | pBssDesc->mdie[0]))
                    break;
            }
            else
                break;
        }
#endif
        fRC = eANI_BOOLEAN_TRUE;

    } while( 0 );
    if( ppIes )
    {
        *ppIes = pIes;
    }
    else if( pIes )
    {
        vos_mem_free(pIes);
    }

    return( fRC );
}

tANI_BOOLEAN csrMatchConnectedBSSSecurity( tpAniSirGlobal pMac, tCsrRoamConnectedProfile *pProfile, 
                                           tSirBssDescription *pBssDesc, tDot11fBeaconIEs *pIes)
{
    tCsrEncryptionList ucEncryptionList, mcEncryptionList;
    tCsrAuthList authList;

    ucEncryptionList.numEntries = 1;
    ucEncryptionList.encryptionType[0] = pProfile->EncryptionType;

    mcEncryptionList.numEntries = 1;
    mcEncryptionList.encryptionType[0] = pProfile->mcEncryptionType;

    authList.numEntries = 1;
    authList.authType[0] = pProfile->AuthType;

    return( csrIsSecurityMatch( pMac, &authList, &ucEncryptionList,
                                &mcEncryptionList, NULL, NULL, NULL,
                                pBssDesc, pIes, NULL, NULL, NULL ));

}


tANI_BOOLEAN csrMatchBSSToConnectProfile( tHalHandle hHal, tCsrRoamConnectedProfile *pProfile,
                                          tSirBssDescription *pBssDesc, tDot11fBeaconIEs *pIes )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_BOOLEAN fRC = eANI_BOOLEAN_FALSE, fCheck;
    tDot11fBeaconIEs *pIesLocal = pIes;

    do {
        if( !pIes )
        {
            if(!HAL_STATUS_SUCCESS(csrGetParsedBssDescriptionIEs(pMac, pBssDesc, &pIesLocal)))
            {
                break;
            }
        }
        fCheck = eANI_BOOLEAN_TRUE;
        if(pIesLocal->SSID.present)
        {
            tANI_BOOLEAN fCheckSsid = eANI_BOOLEAN_FALSE;
            if(pProfile->SSID.length)
            {
                fCheckSsid = eANI_BOOLEAN_TRUE;
            }
            fCheck = csrIsSsidMatch( pMac, pProfile->SSID.ssId, pProfile->SSID.length,
                                        pIesLocal->SSID.ssid, pIesLocal->SSID.num_ssid, fCheckSsid );
            if(!fCheck) break;
        }
        if ( !csrMatchConnectedBSSSecurity( pMac, pProfile, pBssDesc, pIesLocal) ) break;
        if ( !csrIsCapabilitiesMatch( pMac, pProfile->BSSType, pBssDesc ) ) break;
        if ( !csrIsRateSetMatch( pMac, &pIesLocal->SuppRates, &pIesLocal->ExtSuppRates ) ) break;
        fCheck = csrIsChannelBandMatch( pMac, pProfile->operationChannel, pBssDesc );
        if(!fCheck)
            break;

        fRC = eANI_BOOLEAN_TRUE;

    } while( 0 );

    if( !pIes && pIesLocal )
    {
        //locally allocated
        vos_mem_free(pIesLocal);
    }

    return( fRC );
}



tANI_BOOLEAN csrRatesIsDot11RateSupported( tHalHandle hHal, tANI_U8 rate )
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_U16 n = BITS_OFF( rate, CSR_DOT11_BASIC_RATE_MASK );

    return csrIsAggregateRateSupported( pMac, n );
}


tANI_U16 csrRatesMacPropToDot11( tANI_U16 Rate )
{
    tANI_U16 ConvertedRate = Rate;

    switch( Rate )
    {
        case SIR_MAC_RATE_1:
            ConvertedRate = 2;
            break;
        case SIR_MAC_RATE_2:
            ConvertedRate = 4;
            break;
        case SIR_MAC_RATE_5_5:
            ConvertedRate = 11;
            break;
        case SIR_MAC_RATE_11:
            ConvertedRate = 22;
            break;

        case SIR_MAC_RATE_6:
            ConvertedRate = 12;
            break;
        case SIR_MAC_RATE_9:
            ConvertedRate = 18;
            break;
        case SIR_MAC_RATE_12:
            ConvertedRate = 24;
            break;
        case SIR_MAC_RATE_18:
            ConvertedRate = 36;
            break;
        case SIR_MAC_RATE_24:
            ConvertedRate = 48;
            break;
        case SIR_MAC_RATE_36:
            ConvertedRate = 72;
            break;
        case SIR_MAC_RATE_42:
            ConvertedRate = 84;
            break;
        case SIR_MAC_RATE_48:
            ConvertedRate = 96;
            break;
        case SIR_MAC_RATE_54:
            ConvertedRate = 108;
            break;

        case SIR_MAC_RATE_72:
            ConvertedRate = 144;
            break;
        case SIR_MAC_RATE_84:
            ConvertedRate = 168;
            break;
        case SIR_MAC_RATE_96:
            ConvertedRate = 192;
            break;
        case SIR_MAC_RATE_108:
            ConvertedRate = 216;
            break;
        case SIR_MAC_RATE_126:
            ConvertedRate = 252;
            break;
        case SIR_MAC_RATE_144:
            ConvertedRate = 288;
            break;
        case SIR_MAC_RATE_168:
            ConvertedRate = 336;
            break;
        case SIR_MAC_RATE_192:
            ConvertedRate = 384;
            break;
        case SIR_MAC_RATE_216:
            ConvertedRate = 432;
            break;
        case SIR_MAC_RATE_240:
            ConvertedRate = 480;
            break;

        case 0xff:
            ConvertedRate = 0;
            break;
    }

    return ConvertedRate;
}


tANI_U16 csrRatesFindBestRate( tSirMacRateSet *pSuppRates, tSirMacRateSet *pExtRates, tSirMacPropRateSet *pPropRates )
{
    tANI_U8 i;
    tANI_U16 nBest;

    nBest = pSuppRates->rate[ 0 ] & ( ~CSR_DOT11_BASIC_RATE_MASK );

    if(pSuppRates->numRates > SIR_MAC_RATESET_EID_MAX)
    {
        pSuppRates->numRates = SIR_MAC_RATESET_EID_MAX;
    }

    for ( i = 1U; i < pSuppRates->numRates; ++i )
    {
        nBest = (tANI_U16)CSR_MAX( nBest, pSuppRates->rate[ i ] & ( ~CSR_DOT11_BASIC_RATE_MASK ) );
    }

    if ( NULL != pExtRates )
    {
        for ( i = 0U; i < pExtRates->numRates; ++i )
        {
            nBest = (tANI_U16)CSR_MAX( nBest, pExtRates->rate[ i ] & ( ~CSR_DOT11_BASIC_RATE_MASK ) );
        }
    }

    if ( NULL != pPropRates )
    {
        for ( i = 0U; i < pPropRates->numPropRates; ++i )
        {
            nBest = (tANI_U16)CSR_MAX( nBest,  csrRatesMacPropToDot11( pPropRates->propRate[ i ] ) );
        }
    }

    return nBest;
}


void csrReleaseProfile(tpAniSirGlobal pMac, tCsrRoamProfile *pProfile)
{
    if(pProfile)
    {
        if(pProfile->BSSIDs.bssid)
        {
            vos_mem_free(pProfile->BSSIDs.bssid);
            pProfile->BSSIDs.bssid = NULL;
        }
        if(pProfile->SSIDs.SSIDList)
        {
            vos_mem_free(pProfile->SSIDs.SSIDList);
            pProfile->SSIDs.SSIDList = NULL;
        }
        if(pProfile->pWPAReqIE)
        {
            vos_mem_free(pProfile->pWPAReqIE);
            pProfile->pWPAReqIE = NULL;
        }
        if(pProfile->pRSNReqIE)
        {
            vos_mem_free(pProfile->pRSNReqIE);
            pProfile->pRSNReqIE = NULL;
        }
#ifdef FEATURE_WLAN_WAPI
        if(pProfile->pWAPIReqIE)
        {
            vos_mem_free(pProfile->pWAPIReqIE);
            pProfile->pWAPIReqIE = NULL;
        }
#endif /* FEATURE_WLAN_WAPI */

        if (pProfile->nAddIEScanLength)
        {
           memset(pProfile->addIEScan, 0 , SIR_MAC_MAX_IE_LENGTH+2);
           pProfile->nAddIEScanLength = 0;
        }

        if(pProfile->pAddIEAssoc)
        {
            vos_mem_free(pProfile->pAddIEAssoc);
            pProfile->pAddIEAssoc = NULL;
        }
        if(pProfile->ChannelInfo.ChannelList)
        {
            vos_mem_free(pProfile->ChannelInfo.ChannelList);
            pProfile->ChannelInfo.ChannelList = NULL;
        }
        vos_mem_set(pProfile, sizeof(tCsrRoamProfile), 0);
    }
}

void csrFreeScanFilter(tpAniSirGlobal pMac, tCsrScanResultFilter *pScanFilter)
{
    if(pScanFilter->BSSIDs.bssid)
    {
        vos_mem_free(pScanFilter->BSSIDs.bssid);
        pScanFilter->BSSIDs.bssid = NULL;
    }
    if(pScanFilter->ChannelInfo.ChannelList)
    {
        vos_mem_free(pScanFilter->ChannelInfo.ChannelList);
        pScanFilter->ChannelInfo.ChannelList = NULL;
    }
    if(pScanFilter->SSIDs.SSIDList)
    {
        vos_mem_free(pScanFilter->SSIDs.SSIDList);
        pScanFilter->SSIDs.SSIDList = NULL;
    }
}


void csrFreeRoamProfile(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    tCsrRoamSession *pSession = &pMac->roam.roamSession[sessionId];

    if(pSession->pCurRoamProfile)
    {
        csrReleaseProfile(pMac, pSession->pCurRoamProfile);
        vos_mem_free(pSession->pCurRoamProfile);
        pSession->pCurRoamProfile = NULL;
    }
}


void csrFreeConnectBssDesc(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    tCsrRoamSession *pSession = &pMac->roam.roamSession[sessionId];

    if(pSession->pConnectBssDesc)
    {
        vos_mem_free(pSession->pConnectBssDesc);
        pSession->pConnectBssDesc = NULL;
    }
}



tSirResultCodes csrGetDisassocRspStatusCode( tSirSmeDisassocRsp *pSmeDisassocRsp )
{
    tANI_U8 *pBuffer = (tANI_U8 *)pSmeDisassocRsp;
    tANI_U32 ret;

    pBuffer += (sizeof(tANI_U16) + sizeof(tANI_U16) + sizeof(tSirMacAddr));
    //tSirResultCodes is an enum, assuming is 32bit
    //If we cannot make this assumption, use copymemory
    pal_get_U32( pBuffer, &ret );

    return( ( tSirResultCodes )ret );
}


tSirResultCodes csrGetDeAuthRspStatusCode( tSirSmeDeauthRsp *pSmeRsp )
{
    tANI_U8 *pBuffer = (tANI_U8 *)pSmeRsp;
    tANI_U32 ret;

    pBuffer += (sizeof(tANI_U16) + sizeof(tANI_U16) + sizeof(tANI_U8) + sizeof(tANI_U16));
    //tSirResultCodes is an enum, assuming is 32bit
    //If we cannot make this assumption, use copymemory
    pal_get_U32( pBuffer, &ret );

    return( ( tSirResultCodes )ret );
}

#if 0
tSirScanType csrGetScanType(tANI_U8 chnId, eRegDomainId domainId, tANI_U8 *countryCode)
{
    tSirScanType scanType = eSIR_PASSIVE_SCAN;
    tANI_U8 cc = 0;

    while (cc++ < gCsrDomainChnInfo[domainId].numChannels)
    {
        if(chnId == gCsrDomainChnInfo[domainId].chnInfo[cc].chnId)
        {
            scanType = gCsrDomainChnInfo[domainId].chnInfo[cc].scanType;
            break;
        }
    }

    return (scanType);
}
#endif

tSirScanType csrGetScanType(tpAniSirGlobal pMac, tANI_U8 chnId)
{
    tSirScanType scanType = eSIR_PASSIVE_SCAN;
    eNVChannelEnabledType channelEnabledType;

    channelEnabledType = vos_nv_getChannelEnabledState(chnId);
    if( NV_CHANNEL_ENABLE ==  channelEnabledType)
    {
         scanType = eSIR_ACTIVE_SCAN;
    }
    return (scanType);
}


tANI_U8 csrToUpper( tANI_U8 ch )
{
    tANI_U8 chOut;

    if ( ch >= 'a' && ch <= 'z' )
    {
        chOut = ch - 'a' + 'A';
    }
    else
    {
        chOut = ch;
    }
    return( chOut );
}


tSirBssType csrTranslateBsstypeToMacType(eCsrRoamBssType csrtype)
{
    tSirBssType ret;

    switch(csrtype)
    {
    case eCSR_BSS_TYPE_INFRASTRUCTURE:
        ret = eSIR_INFRASTRUCTURE_MODE;
        break;
    case eCSR_BSS_TYPE_IBSS:
    case eCSR_BSS_TYPE_START_IBSS:
        ret = eSIR_IBSS_MODE;
        break;
    case eCSR_BSS_TYPE_WDS_AP:
        ret = eSIR_BTAMP_AP_MODE;
        break;
    case eCSR_BSS_TYPE_WDS_STA:
        ret = eSIR_BTAMP_STA_MODE;
        break;
    case eCSR_BSS_TYPE_INFRA_AP:
        ret = eSIR_INFRA_AP_MODE;
        break;
    case eCSR_BSS_TYPE_ANY:
    default:
        ret = eSIR_AUTO_MODE;
        break;
    }

    return (ret);
}


//This function use the parameters to decide the CFG value.
//CSR never sets WNI_CFG_DOT11_MODE_ALL to the CFG
//So PE should not see WNI_CFG_DOT11_MODE_ALL when it gets the CFG value
eCsrCfgDot11Mode csrGetCfgDot11ModeFromCsrPhyMode(tCsrRoamProfile *pProfile, eCsrPhyMode phyMode, tANI_BOOLEAN fProprietary)
{
    tANI_U32 cfgDot11Mode = eCSR_CFG_DOT11_MODE_ABG;

    switch(phyMode)
    {
    case eCSR_DOT11_MODE_11a:
    case eCSR_DOT11_MODE_11a_ONLY:
        cfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
        break;
    case eCSR_DOT11_MODE_11b:
    case eCSR_DOT11_MODE_11b_ONLY:
        cfgDot11Mode = eCSR_CFG_DOT11_MODE_11B;
        break;
    case eCSR_DOT11_MODE_11g:
    case eCSR_DOT11_MODE_11g_ONLY:
        if(pProfile && (CSR_IS_INFRA_AP(pProfile)) && (phyMode == eCSR_DOT11_MODE_11g_ONLY))
            cfgDot11Mode = eCSR_CFG_DOT11_MODE_11G_ONLY;
        else
        cfgDot11Mode = eCSR_CFG_DOT11_MODE_11G;
        break;
    case eCSR_DOT11_MODE_11n:
        if(fProprietary)
        {
            cfgDot11Mode = eCSR_CFG_DOT11_MODE_TAURUS;
        }
        else
        {
            cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
        }
        break;
    case eCSR_DOT11_MODE_11n_ONLY:
       if(pProfile && CSR_IS_INFRA_AP(pProfile))
           cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N_ONLY;
       else
       cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
       break;
    case eCSR_DOT11_MODE_TAURUS:
        cfgDot11Mode = eCSR_CFG_DOT11_MODE_TAURUS;
        break;
    case eCSR_DOT11_MODE_abg:
        cfgDot11Mode = eCSR_CFG_DOT11_MODE_ABG;
        break;
    case eCSR_DOT11_MODE_AUTO:
        cfgDot11Mode = eCSR_CFG_DOT11_MODE_AUTO;
        break;

#ifdef WLAN_FEATURE_11AC
    case eCSR_DOT11_MODE_11ac:
        if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC))
        {
            cfgDot11Mode = eCSR_CFG_DOT11_MODE_11AC;
        }
        else
        {
            cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
        }
        break;
    case eCSR_DOT11_MODE_11ac_ONLY:
        if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC))
        {
            cfgDot11Mode = eCSR_CFG_DOT11_MODE_11AC_ONLY;
        }
        else
        {
            cfgDot11Mode = eCSR_CFG_DOT11_MODE_11N;
        }
        break;
#endif
    default:
        //No need to assign anything here
        break;
    }

    return (cfgDot11Mode);
}


eHalStatus csrSetRegulatoryDomain(tpAniSirGlobal pMac, v_REGDOMAIN_t domainId, tANI_BOOLEAN *pfRestartNeeded)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_BOOLEAN fRestart;

    if(pMac->scan.domainIdCurrent == domainId)
    {
        //no change
        fRestart = eANI_BOOLEAN_FALSE;
    }
    else if( !pMac->roam.configParam.fEnforceDefaultDomain )
    {
        pMac->scan.domainIdCurrent = domainId;
        fRestart = eANI_BOOLEAN_TRUE;
    }
    else
    {
        //We cannot change the domain
        status = eHAL_STATUS_CSR_WRONG_STATE;
        fRestart = eANI_BOOLEAN_FALSE;
    }
    if(pfRestartNeeded)
    {
        *pfRestartNeeded = fRestart;
    }

    return (status);
}


v_REGDOMAIN_t csrGetCurrentRegulatoryDomain(tpAniSirGlobal pMac)
{
    return (pMac->scan.domainIdCurrent);
}


eHalStatus csrGetRegulatoryDomainForCountry
(
tpAniSirGlobal pMac,
tANI_U8 *pCountry,
v_REGDOMAIN_t *pDomainId,
v_CountryInfoSource_t source
)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    VOS_STATUS vosStatus;
    v_COUNTRYCODE_t countryCode;
    v_REGDOMAIN_t domainId;

    if(pCountry)
    {
        countryCode[0] = pCountry[0];
        countryCode[1] = pCountry[1];
        vosStatus = vos_nv_getRegDomainFromCountryCode(&domainId,
                                                       countryCode,
                                                       source);

        if( VOS_IS_STATUS_SUCCESS(vosStatus) )
        {
            if( pDomainId )
            {
                *pDomainId = domainId;
            }
            status = eHAL_STATUS_SUCCESS;
        }
        else
        {
            smsLog(pMac, LOGW, FL(" Couldn't find domain for country code  %c%c"), pCountry[0], pCountry[1]);
            status = eHAL_STATUS_INVALID_PARAMETER;
        }
    }

    return (status);
}

//To check whether a country code matches the one in the IE
//Only check the first two characters, ignoring in/outdoor
//pCountry -- caller allocated buffer contain the country code that is checking against
//the one in pIes. It can be NULL.
//caller must provide pIes, it cannot be NULL
//This function always return TRUE if 11d support is not turned on.
tANI_BOOLEAN csrMatchCountryCode( tpAniSirGlobal pMac, tANI_U8 *pCountry, tDot11fBeaconIEs *pIes )
{
    tANI_BOOLEAN fRet = eANI_BOOLEAN_TRUE;
    v_REGDOMAIN_t domainId = REGDOMAIN_COUNT;   //This is init to invalid value
    eHalStatus status;

    do
    {
        if( !csrIs11dSupported( pMac) )
        {
            break;
        }
        if( !pIes )
        {
            smsLog(pMac, LOGE, FL("  No IEs"));
            break;
        }
        if( pMac->roam.configParam.fEnforceDefaultDomain ||
            pMac->roam.configParam.fEnforceCountryCodeMatch )
        {
            //Make sure this country is recognizable
            if( pIes->Country.present )
            {
                status = csrGetRegulatoryDomainForCountry(pMac,
                                           pIes->Country.country,
                                           &domainId, COUNTRY_QUERY);
                if( !HAL_STATUS_SUCCESS( status ) )
                {
                     status = csrGetRegulatoryDomainForCountry(pMac,
                                                 pMac->scan.countryCode11d,
                                                 (v_REGDOMAIN_t *) &domainId,
                                                 COUNTRY_QUERY);
                     if( !HAL_STATUS_SUCCESS( status ) )
                     {
                           fRet = eANI_BOOLEAN_FALSE;
                           break;
                     }
                }
            }
            //check whether it is needed to enforce to the default regulatory domain first
            if( pMac->roam.configParam.fEnforceDefaultDomain )
            {
                if( domainId != pMac->scan.domainIdCurrent )
                {
                    fRet = eANI_BOOLEAN_FALSE;
                    break;
                }
            }
            if( pMac->roam.configParam.fEnforceCountryCodeMatch )
            {
            if( domainId >= REGDOMAIN_COUNT )
                {
                    fRet = eANI_BOOLEAN_FALSE;
                    break;
                }
            }
        }
        if( pCountry )
        {
            tANI_U32 i;

            if( !pIes->Country.present )
            {
                fRet = eANI_BOOLEAN_FALSE;
                break;
            }
            // Convert the CountryCode characters to upper
            for ( i = 0; i < WNI_CFG_COUNTRY_CODE_LEN - 1; i++ )
            {
                pCountry[i] = csrToUpper( pCountry[i] );
            }
            if (!vos_mem_compare(pIes->Country.country, pCountry,
                                WNI_CFG_COUNTRY_CODE_LEN - 1))
            {
                fRet = eANI_BOOLEAN_FALSE;
                break;
            }
        }
    } while(0);

    return (fRet);
}

#if 0
eHalStatus csrSetCountryDomainMapping(tpAniSirGlobal pMac, tCsrCountryDomainMapping *pCountryDomainMapping)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tANI_U32 i, j;
    tANI_BOOLEAN fDomainChanged = eANI_BOOLEAN_FALSE;
    tANI_U8 countryCode[WNI_CFG_COUNTRY_CODE_LEN];

    i = WNI_CFG_COUNTRY_CODE_LEN;
    //Get the currently used country code
    status = ccmCfgGetStr(pMac, WNI_CFG_COUNTRY_CODE, countryCode, &i);
    if(HAL_STATUS_SUCCESS(status))
    {
        if(pCountryDomainMapping && pCountryDomainMapping->numEntry)
        {
            for(i = 0; i < pCountryDomainMapping->numEntry; i++)
            {
                for(j = 0; j < eCSR_NUM_COUNTRY_INDEX; j++)
                {
                    if (vos_mem_compare(gCsrCountryInfo[j].countryCode,
                                        pCountryDomainMapping->pCountryInfo[i].countryCode,
                                        2))
                    {
                        if(gCsrCountryInfo[j].domainId != pCountryDomainMapping->pCountryInfo[i].domainId)
                        {
                            gCsrCountryInfo[j].domainId = pCountryDomainMapping->pCountryInfo[i].domainId;
                            //Check whether it matches the currently used country code
                            //If matching, need to update base on the new domain setting.
                            if (vos_mem_compare(countryCode,
                                                pCountryDomainMapping->pCountryInfo[i].countryCode,
                                                2))
                            {
                                fDomainChanged = eANI_BOOLEAN_TRUE;
                            }
                        }
                        break;
                    }
                }
            }
            status = eHAL_STATUS_SUCCESS;
            if(fDomainChanged)
            {
                tCsrChannel *pChannelList;

                if(pMac->scan.f11dInfoApplied)
                {
                    //11d info already applied. Let's reapply with the new domain setting
                    if(pMac->scan.channels11d.numChannels)
                    {
                        pChannelList = &pMac->scan.channels11d;
                    }
                    else
                    {
                        pChannelList = &pMac->scan.base20MHzChannels;
                    }
                }
                else
                {
                    //no 11d so we use the base channelist from EEPROM
                    pChannelList = &pMac->scan.base20MHzChannels;
                }
                //set the new domain's scan requirement to CFG
                csrSetCfgScanControlList(pMac, countryCode, pChannelList);
            }
        }
    }

    return (status);
}

eHalStatus csrSetDomainScanSetting(tpAniSirGlobal pMac, tCsrDomainFreqInfo *pDomainFreqInfo)
{
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;
    tANI_U32 i, j;
    tANI_U16 freq;

    if(pDomainFreqInfo && pDomainFreqInfo->numEntry && (pDomainFreqInfo->domainId < NUM_REG_DOMAINS))
    {
        tCsrDomainChnInfo *pDomainChnInfo = &gCsrDomainChnInfo[pDomainFreqInfo->domainId];

        for(j = 0; j < pDomainChnInfo->numChannels; j++)
        {
            if(HAL_STATUS_SUCCESS(halPhyChIdToFreqConversion(pDomainChnInfo->chnInfo[j].chnId, &freq)))
            {
                for(i = 0; i < pDomainFreqInfo->numEntry; i++)
                {
                    if((pDomainFreqInfo->pCsrScanFreqInfo[i].nStartFreq <= freq) &&
                        (freq <= pDomainFreqInfo->pCsrScanFreqInfo[i].nEndFreq))
                    {
                        pDomainChnInfo->chnInfo[j].scanType = pDomainFreqInfo->pCsrScanFreqInfo[i].scanType;
                        break;
                    }
                }
            }
            else
            {
                smsLog(pMac, LOGW, "   Failed to get frequency of channel %d", pDomainChnInfo->chnInfo[j].chnId);
            }
        }
        status = eHAL_STATUS_SUCCESS;
    }

    return (status);
}
#endif

eHalStatus csrGetModifyProfileFields(tpAniSirGlobal pMac, tANI_U32 sessionId,
                                     tCsrRoamModifyProfileFields *pModifyProfileFields)
{

   if(!pModifyProfileFields)
   {
      return eHAL_STATUS_FAILURE;
   }

   vos_mem_copy(pModifyProfileFields,
                &pMac->roam.roamSession[sessionId].connectedProfile.modifyProfileFields,
                sizeof(tCsrRoamModifyProfileFields));

   return eHAL_STATUS_SUCCESS;
}

eHalStatus csrSetModifyProfileFields(tpAniSirGlobal pMac, tANI_U32 sessionId,
                                     tCsrRoamModifyProfileFields *pModifyProfileFields)
{
   tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId );

   vos_mem_copy(&pSession->connectedProfile.modifyProfileFields,
                 pModifyProfileFields,
                 sizeof(tCsrRoamModifyProfileFields));

   return eHAL_STATUS_SUCCESS;
}


#if 0
/* ---------------------------------------------------------------------------
    \fn csrGetSupportedCountryCode
    \brief this function is to get a list of the country code current being supported
    \param pBuf - Caller allocated buffer with at least 3 bytes, upon success return, 
    this has the country code list. 3 bytes for each country code. This may be NULL if
    caller wants to know the needed bytes.
    \param pbLen - Caller allocated, as input, it indicates the length of pBuf. Upon success return,
    this contains the length of the data in pBuf
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrGetSupportedCountryCode(tpAniSirGlobal pMac, tANI_U8 *pBuf, tANI_U32 *pbLen)
{
    tANI_U32 numOfCountry = sizeof( gCsrCountryInfo ) / sizeof( gCsrCountryInfo[0] );
    tANI_U32 numBytes = 0;
    eHalStatus status = eHAL_STATUS_INVALID_PARAMETER;

    if( pbLen )
    {
        numBytes = *pbLen;
        //Consider it ok, at least we can return the number of bytes needed;
        *pbLen = numOfCountry * WNI_CFG_COUNTRY_CODE_LEN;
        status = eHAL_STATUS_SUCCESS;
        if( pBuf && ( numBytes >= *pbLen ) )
        {
            //The ugly part starts.
            //We may need to alter the data structure and find a way to make this faster.
            tANI_U32 i;

            for ( i = 0; i < numOfCountry; i++ )
            {
                vos_mem_copy(pBuf + ( i * WNI_CFG_COUNTRY_CODE_LEN ),
                             gCsrCountryInfo[i].countryCode,
                             WNI_CFG_COUNTRY_CODE_LEN);
            }
        }
    }

    return ( status );
}
#endif

/* ---------------------------------------------------------------------------
    \fn csrGetSupportedCountryCode
    \brief this function is to get a list of the country code current being supported
    \param pBuf - Caller allocated buffer with at least 3 bytes, upon success return, 
    this has the country code list. 3 bytes for each country code. This may be NULL if
    caller wants to know the needed bytes.
    \param pbLen - Caller allocated, as input, it indicates the length of pBuf. Upon success return,
    this contains the length of the data in pBuf
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus csrGetSupportedCountryCode(tpAniSirGlobal pMac, tANI_U8 *pBuf, tANI_U32 *pbLen)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    VOS_STATUS vosStatus;
    v_SIZE_t size = (v_SIZE_t)*pbLen;

    vosStatus = vos_nv_getSupportedCountryCode( pBuf, &size, 1 );
    //eiter way, return the value back
    *pbLen = (tANI_U32)size;

    //If pBuf is NULL, caller just want to get the size, consider it success
    if(pBuf)
    {
        if( VOS_IS_STATUS_SUCCESS( vosStatus ) )
        {
            tANI_U32 i, n = *pbLen / 3;

            for( i = 0; i < n; i++ )
            {
                pBuf[i*3 + 2] = ' ';
            }
        }
        else
        {
            status = eHAL_STATUS_FAILURE;
        }
    }

    return (status);
}



//Upper layer to get the list of the base channels to scan for passively 11d info from csr
eHalStatus csrScanGetBaseChannels( tpAniSirGlobal pMac, tCsrChannelInfo * pChannelInfo )
{
    eHalStatus status = eHAL_STATUS_FAILURE;

    do
    {
    
       if(!pMac->scan.baseChannels.numChannels || !pChannelInfo)
       {
          break;
       }
       pChannelInfo->ChannelList = vos_mem_malloc(pMac->scan.baseChannels.numChannels);
       if ( NULL == pChannelInfo->ChannelList )
       {
          smsLog( pMac, LOGE, FL("csrScanGetBaseChannels: fail to allocate memory") );
          return eHAL_STATUS_FAILURE;
       }
       vos_mem_copy(pChannelInfo->ChannelList,
                    pMac->scan.baseChannels.channelList,
                    pMac->scan.baseChannels.numChannels);
       pChannelInfo->numOfChannels = pMac->scan.baseChannels.numChannels;

    }while(0);

    return ( status );
}


tANI_BOOLEAN csrIsSetKeyAllowed(tpAniSirGlobal pMac, tANI_U32 sessionId)
{
    tANI_BOOLEAN fRet = eANI_BOOLEAN_TRUE;
    tCsrRoamSession *pSession;

    pSession =CSR_GET_SESSION(pMac, sessionId);

    /*This condition is not working for infra state. When infra is in not-connected state
    * the pSession->pCurRoamProfile is NULL. And this function returns TRUE, that is incorrect.
    * Since SAP requires to set key without any BSS started, it needs this condition to be met.
    * In other words, this function is useless.
    * The current work-around is to process setcontext_rsp and removekey_rsp no matter what the 
    * state is.
    */
    smsLog( pMac, LOG2, FL(" is not what it intends to. Must be revisit or removed") );
    if( (NULL == pSession) || 
        ( csrIsConnStateDisconnected( pMac, sessionId ) && 
        (pSession->pCurRoamProfile != NULL) &&
        (!(CSR_IS_INFRA_AP(pSession->pCurRoamProfile))) )
        )
    {
        fRet = eANI_BOOLEAN_FALSE;
    }

    return ( fRet );
}

//no need to acquire lock for this basic function
tANI_U16 sme_ChnToFreq(tANI_U8 chanNum)
{
   int i;

   for (i = 0; i < NUM_RF_CHANNELS; i++) 
   {
      if (rfChannels[i].channelNum == chanNum) 
      {
         return rfChannels[i].targetFreq;
      }
   }

   return (0);
}

/* Disconnect all active sessions by sending disassoc. This is mainly used to disconnect the remaining session when we 
 * transition from concurrent sessions to a single session. The use case is Infra STA and wifi direct multiple sessions are up and 
 * P2P session is removed. The Infra STA session remains and should resume BMPS if BMPS is enabled by default. However, there
 * are some issues seen with BMPS resume during this transition and this is a workaround which will allow the Infra STA session to
 * disconnect and auto connect back and enter BMPS this giving the same effect as resuming BMPS
 */
 
//Remove this code once SLM_Sessionization is supported 
//BMPS_WORKAROUND_NOT_NEEDED
void csrDisconnectAllActiveSessions(tpAniSirGlobal pMac)
{
    tANI_U8 i;

    /* Disconnect all the active sessions */
    for (i=0; i<CSR_ROAM_SESSION_MAX; i++)
    {
        if( CSR_IS_SESSION_VALID( pMac, i ) && !csrIsConnStateDisconnected( pMac, i ) )
        {
            csrRoamDisconnectInternal(pMac, i, eCSR_DISCONNECT_REASON_UNSPECIFIED);
        }
    }
}

#ifdef FEATURE_WLAN_LFR
tANI_BOOLEAN csrIsChannelPresentInList(
        tANI_U8 *pChannelList,
        int  numChannels,
        tANI_U8   channel
        )
{
    int i = 0;

    // Check for NULL pointer
    if (!pChannelList || (numChannels == 0))
    {
       return FALSE;
    }

    // Look for the channel in the list
    for (i = 0; i < numChannels; i++)
    {
        if (pChannelList[i] == channel)
            return TRUE;
    }

    return FALSE;
}

VOS_STATUS csrAddToChannelListFront(
        tANI_U8 *pChannelList,
        int  numChannels,
        tANI_U8   channel
        )
{
    int i = 0;

    // Check for NULL pointer
    if (!pChannelList) return eHAL_STATUS_E_NULL_VALUE;

    // Make room for the addition.  (Start moving from the back.)
    for (i = numChannels; i > 0; i--)
    {
        pChannelList[i] = pChannelList[i-1];
    }

    // Now add the NEW channel...at the front
    pChannelList[0] = channel;

    return eHAL_STATUS_SUCCESS;
}
#endif
const char * sme_requestTypetoString(const v_U8_t requestType)
{
    switch (requestType)
    {
        CASE_RETURN_STRING( eCSR_SCAN_REQUEST_11D_SCAN );
        CASE_RETURN_STRING( eCSR_SCAN_REQUEST_FULL_SCAN );
        CASE_RETURN_STRING( eCSR_SCAN_IDLE_MODE_SCAN );
        CASE_RETURN_STRING( eCSR_SCAN_HO_BG_SCAN );
        CASE_RETURN_STRING( eCSR_SCAN_HO_PROBE_SCAN );
        CASE_RETURN_STRING( eCSR_SCAN_HO_NT_BG_SCAN );
        CASE_RETURN_STRING( eCSR_SCAN_P2P_DISCOVERY );
        CASE_RETURN_STRING( eCSR_SCAN_SOFTAP_CHANNEL_RANGE );
        CASE_RETURN_STRING( eCSR_SCAN_P2P_FIND_PEER );
        default:
            return "Unknown Scan Request Type";
    }
}
