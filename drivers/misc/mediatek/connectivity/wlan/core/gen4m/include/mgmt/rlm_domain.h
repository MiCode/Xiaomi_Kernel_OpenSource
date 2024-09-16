/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/
 *	 include/mgmt/rlm_domain.h#1
 */

/*! \file   "rlm_domain.h"
 *    \brief
 */


#ifndef _RLM_DOMAIN_H
#define _RLM_DOMAIN_H

/*******************************************************************************
 *       C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *  E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
 #include "wsys_cmd_handler_fw.h"

/*******************************************************************************
 *   C O N S T A N T S
 *******************************************************************************
 */
#define MAX_SUBBAND_NUM     6
#define MAX_SUBBAND_NUM_5G  8

#define COUNTRY_CODE_NULL      ((uint16_t)0x0)

/* ISO/IEC 3166-1 two-character country codes */

/* Andorra */
#define COUNTRY_CODE_AD (((uint16_t) 'A' << 8) | (uint16_t) 'D')
/* UAE */
#define COUNTRY_CODE_AE (((uint16_t) 'A' << 8) | (uint16_t) 'E')
/* Afghanistan */
#define COUNTRY_CODE_AF (((uint16_t) 'A' << 8) | (uint16_t) 'F')
/* Antigua & Barbuda */
#define COUNTRY_CODE_AG (((uint16_t) 'A' << 8) | (uint16_t) 'G')
/* Anguilla */
#define COUNTRY_CODE_AI (((uint16_t) 'A' << 8) | (uint16_t) 'I')
/* Albania */
#define COUNTRY_CODE_AL (((uint16_t) 'A' << 8) | (uint16_t) 'L')
/* Armenia */
#define COUNTRY_CODE_AM (((uint16_t) 'A' << 8) | (uint16_t) 'M')
/* Netherlands Antilles */
#define COUNTRY_CODE_AN (((uint16_t) 'A' << 8) | (uint16_t) 'N')
/* Angola */
#define COUNTRY_CODE_AO (((uint16_t) 'A' << 8) | (uint16_t) 'O')
/* Argentina */
#define COUNTRY_CODE_AR (((uint16_t) 'A' << 8) | (uint16_t) 'R')
/* American Samoa (USA) */
#define COUNTRY_CODE_AS (((uint16_t) 'A' << 8) | (uint16_t) 'S')
/* Austria */
#define COUNTRY_CODE_AT (((uint16_t) 'A' << 8) | (uint16_t) 'T')
/* Australia */
#define COUNTRY_CODE_AU (((uint16_t) 'A' << 8) | (uint16_t) 'U')
/* Aruba */
#define COUNTRY_CODE_AW (((uint16_t) 'A' << 8) | (uint16_t) 'W')
/* Azerbaijan */
#define COUNTRY_CODE_AZ (((uint16_t) 'A' << 8) | (uint16_t) 'Z')
/* Bosnia and Herzegovina */
#define COUNTRY_CODE_BA (((uint16_t) 'B' << 8) | (uint16_t) 'A')
/* Barbados */
#define COUNTRY_CODE_BB (((uint16_t) 'B' << 8) | (uint16_t) 'B')
/* Bangladesh */
#define COUNTRY_CODE_BD (((uint16_t) 'B' << 8) | (uint16_t) 'D')
/* Belgium  */
#define COUNTRY_CODE_BE (((uint16_t) 'B' << 8) | (uint16_t) 'E')
/* Burkina Faso */
#define COUNTRY_CODE_BF (((uint16_t) 'B' << 8) | (uint16_t) 'F')
/* Bulgaria */
#define COUNTRY_CODE_BG (((uint16_t) 'B' << 8) | (uint16_t) 'G')
/* Bahrain */
#define COUNTRY_CODE_BH (((uint16_t) 'B' << 8) | (uint16_t) 'H')
/* Burundi */
#define COUNTRY_CODE_BI (((uint16_t) 'B' << 8) | (uint16_t) 'I')
/* Benin */
#define COUNTRY_CODE_BJ (((uint16_t) 'B' << 8) | (uint16_t) 'J')
/* Bermuda */
#define COUNTRY_CODE_BM (((uint16_t) 'B' << 8) | (uint16_t) 'M')
/* Brunei */
#define COUNTRY_CODE_BN (((uint16_t) 'B' << 8) | (uint16_t) 'N')
/* Bolivia */
#define COUNTRY_CODE_BO (((uint16_t) 'B' << 8) | (uint16_t) 'O')
/* Brazil */
#define COUNTRY_CODE_BR (((uint16_t) 'B' << 8) | (uint16_t) 'R')
/* Bahamas  */
#define COUNTRY_CODE_BS (((uint16_t) 'B' << 8) | (uint16_t) 'S')
/* Bhutan */
#define COUNTRY_CODE_BT (((uint16_t) 'B' << 8) | (uint16_t) 'T')
/* Botswana */
#define COUNTRY_CODE_BW (((uint16_t) 'B' << 8) | (uint16_t) 'W')
/* Belarus */
#define COUNTRY_CODE_BY (((uint16_t) 'B' << 8) | (uint16_t) 'Y')
/* Belize */
#define COUNTRY_CODE_BZ (((uint16_t) 'B' << 8) | (uint16_t) 'Z')
/* Canada */
#define COUNTRY_CODE_CA (((uint16_t) 'C' << 8) | (uint16_t) 'A')
/* Democratic Republic of the Congo */
#define COUNTRY_CODE_CD (((uint16_t) 'C' << 8) | (uint16_t) 'D')
/* Central African Republic */
#define COUNTRY_CODE_CF (((uint16_t) 'C' << 8) | (uint16_t) 'F')
/* Republic of the Congo */
#define COUNTRY_CODE_CG (((uint16_t) 'C' << 8) | (uint16_t) 'G')
/* Switzerland */
#define COUNTRY_CODE_CH (((uint16_t) 'C' << 8) | (uint16_t) 'H')
/* Cote d'lvoire */
#define COUNTRY_CODE_CI (((uint16_t) 'C' << 8) | (uint16_t) 'I')
/* Cook Island */
#define COUNTRY_CODE_CK (((uint16_t) 'C' << 8) | (uint16_t) 'K')
/* Chile */
#define COUNTRY_CODE_CL (((uint16_t) 'C' << 8) | (uint16_t) 'L')
/* Cameroon */
#define COUNTRY_CODE_CM (((uint16_t) 'C' << 8) | (uint16_t) 'M')
/* China */
#define COUNTRY_CODE_CN (((uint16_t) 'C' << 8) | (uint16_t) 'N')
/* Columbia */
#define COUNTRY_CODE_CO (((uint16_t) 'C' << 8) | (uint16_t) 'O')
/* Costa Rica */
#define COUNTRY_CODE_CR (((uint16_t) 'C' << 8) | (uint16_t) 'R')
/* Cuba */
#define COUNTRY_CODE_CU (((uint16_t) 'C' << 8) | (uint16_t) 'U')
/* Cape Verde */
#define COUNTRY_CODE_CV (((uint16_t) 'C' << 8) | (uint16_t) 'V')
/* "Christmas Island(Australia) */
#define COUNTRY_CODE_CX (((uint16_t) 'C' << 8) | (uint16_t) 'X')
/* Cyprus */
#define COUNTRY_CODE_CY (((uint16_t) 'C' << 8) | (uint16_t) 'Y')
/* Czech */
#define COUNTRY_CODE_CZ (((uint16_t) 'C' << 8) | (uint16_t) 'Z')
/* Germany */
#define COUNTRY_CODE_DE (((uint16_t) 'D' << 8) | (uint16_t) 'E')
/* Djibouti */
#define COUNTRY_CODE_DJ (((uint16_t) 'D' << 8) | (uint16_t) 'J')
/* Denmark */
#define COUNTRY_CODE_DK (((uint16_t) 'D' << 8) | (uint16_t) 'K')
/* Dominica */
#define COUNTRY_CODE_DM (((uint16_t) 'D' << 8) | (uint16_t) 'M')
/* Dominican Republic */
#define COUNTRY_CODE_DO (((uint16_t) 'D' << 8) | (uint16_t) 'O')
/* Algeria */
#define COUNTRY_CODE_DZ (((uint16_t) 'D' << 8) | (uint16_t) 'Z')
/* Ecuador */
#define COUNTRY_CODE_EC (((uint16_t) 'E' << 8) | (uint16_t) 'C')
/* Estonia */
#define COUNTRY_CODE_EE (((uint16_t) 'E' << 8) | (uint16_t) 'E')
/* Egypt */
#define COUNTRY_CODE_EG (((uint16_t) 'E' << 8) | (uint16_t) 'G')
/* Western Sahara (Morocco) */
#define COUNTRY_CODE_EH (((uint16_t) 'E' << 8) | (uint16_t) 'H')
/* Eritrea */
#define COUNTRY_CODE_ER (((uint16_t) 'E' << 8) | (uint16_t) 'R')
/* Spain */
#define COUNTRY_CODE_ES (((uint16_t) 'E' << 8) | (uint16_t) 'S')
/* Ethiopia */
#define COUNTRY_CODE_ET (((uint16_t) 'E' << 8) | (uint16_t) 'T')
/* Europe */
#define COUNTRY_CODE_EU (((uint16_t) 'E' << 8) | (uint16_t) 'U')
/* Finland */
#define COUNTRY_CODE_FI (((uint16_t) 'F' << 8) | (uint16_t) 'I')
/* Fiji */
#define COUNTRY_CODE_FJ (((uint16_t) 'F' << 8) | (uint16_t) 'J')
/* Falkland Island */
#define COUNTRY_CODE_FK (((uint16_t) 'F' << 8) | (uint16_t) 'K')
/* Micronesia */
#define COUNTRY_CODE_FM (((uint16_t) 'F' << 8) | (uint16_t) 'M')
/* Faroe Island */
#define COUNTRY_CODE_FO (((uint16_t) 'F' << 8) | (uint16_t) 'O')
/* France */
#define COUNTRY_CODE_FR (((uint16_t) 'F' << 8) | (uint16_t) 'R')
/* Wallis and Futuna (France) */
#define COUNTRY_CODE_FR (((uint16_t) 'F' << 8) | (uint16_t) 'R')
/* Gabon */
#define COUNTRY_CODE_GA (((uint16_t) 'G' << 8) | (uint16_t) 'A')
/* United Kingdom */
#define COUNTRY_CODE_GB (((uint16_t) 'G' << 8) | (uint16_t) 'B')
/* Grenada */
#define COUNTRY_CODE_GD (((uint16_t) 'G' << 8) | (uint16_t) 'D')
/* Georgia */
#define COUNTRY_CODE_GE (((uint16_t) 'G' << 8) | (uint16_t) 'E')
/* French Guiana */
#define COUNTRY_CODE_GF (((uint16_t) 'G' << 8) | (uint16_t) 'F')
/* Guernsey */
#define COUNTRY_CODE_GG (((uint16_t) 'G' << 8) | (uint16_t) 'G')
/* Ghana */
#define COUNTRY_CODE_GH (((uint16_t) 'G' << 8) | (uint16_t) 'H')
/* Gibraltar */
#define COUNTRY_CODE_GI (((uint16_t) 'G' << 8) | (uint16_t) 'I')
/* Gambia */
#define COUNTRY_CODE_GM (((uint16_t) 'G' << 8) | (uint16_t) 'M')
/* Guinea */
#define COUNTRY_CODE_GN (((uint16_t) 'G' << 8) | (uint16_t) 'N')
/* Guadeloupe */
#define COUNTRY_CODE_GP (((uint16_t) 'G' << 8) | (uint16_t) 'P')
/* Equatorial Guinea */
#define COUNTRY_CODE_GQ (((uint16_t) 'G' << 8) | (uint16_t) 'Q')
/* Greece */
#define COUNTRY_CODE_GR (((uint16_t) 'G' << 8) | (uint16_t) 'R')
/* Guatemala */
#define COUNTRY_CODE_GT (((uint16_t) 'G' << 8) | (uint16_t) 'T')
/* Guam */
#define COUNTRY_CODE_GU (((uint16_t) 'G' << 8) | (uint16_t) 'U')
/* Guinea-Bissau */
#define COUNTRY_CODE_GW (((uint16_t) 'G' << 8) | (uint16_t) 'W')
/* Guyana */
#define COUNTRY_CODE_GY (((uint16_t) 'G' << 8) | (uint16_t) 'Y')
/* Hong Kong */
#define COUNTRY_CODE_HK (((uint16_t) 'H' << 8) | (uint16_t) 'K')
/* Honduras */
#define COUNTRY_CODE_HN (((uint16_t) 'H' << 8) | (uint16_t) 'N')
/* Croatia */
#define COUNTRY_CODE_HR (((uint16_t) 'H' << 8) | (uint16_t) 'R')
/* Haiti */
#define COUNTRY_CODE_HT (((uint16_t) 'H' << 8) | (uint16_t) 'T')
/* Hungary */
#define COUNTRY_CODE_HU (((uint16_t) 'H' << 8) | (uint16_t) 'U')
/* Indonesia */
#define COUNTRY_CODE_ID (((uint16_t) 'I' << 8) | (uint16_t) 'D')
/* Ireland */
#define COUNTRY_CODE_IE (((uint16_t) 'I' << 8) | (uint16_t) 'E')
/* Israel */
#define COUNTRY_CODE_IL (((uint16_t) 'I' << 8) | (uint16_t) 'L')
/* Isle of Man */
#define COUNTRY_CODE_IM (((uint16_t) 'I' << 8) | (uint16_t) 'M')
/* India */
#define COUNTRY_CODE_IN (((uint16_t) 'I' << 8) | (uint16_t) 'N')
/* Iraq */
#define COUNTRY_CODE_IQ (((uint16_t) 'I' << 8) | (uint16_t) 'Q')
/* Iran */
#define COUNTRY_CODE_IR (((uint16_t) 'I' << 8) | (uint16_t) 'R')
/* Iceland */
#define COUNTRY_CODE_IS (((uint16_t) 'I' << 8) | (uint16_t) 'S')
/* Italy */
#define COUNTRY_CODE_IT (((uint16_t) 'I' << 8) | (uint16_t) 'T')
/* Jersey */
#define COUNTRY_CODE_JE (((uint16_t) 'J' << 8) | (uint16_t) 'E')
/* Jameica */
#define COUNTRY_CODE_JM (((uint16_t) 'J' << 8) | (uint16_t) 'M')
/* Jordan */
#define COUNTRY_CODE_JO (((uint16_t) 'J' << 8) | (uint16_t) 'O')
/* Japan */
#define COUNTRY_CODE_JP (((uint16_t) 'J' << 8) | (uint16_t) 'P')
/* Kenya */
#define COUNTRY_CODE_KE (((uint16_t) 'K' << 8) | (uint16_t) 'E')
/* Kyrgyzstan */
#define COUNTRY_CODE_KG (((uint16_t) 'K' << 8) | (uint16_t) 'G')
/* Cambodia */
#define COUNTRY_CODE_KH (((uint16_t) 'K' << 8) | (uint16_t) 'H')
/* Kiribati */
#define COUNTRY_CODE_KI (((uint16_t) 'K' << 8) | (uint16_t) 'I')
/* Comoros */
#define COUNTRY_CODE_KM (((uint16_t) 'K' << 8) | (uint16_t) 'M')
/* Saint Kitts and Nevis */
#define COUNTRY_CODE_KN (((uint16_t) 'K' << 8) | (uint16_t) 'N')
/* North Korea */
#define COUNTRY_CODE_KP (((uint16_t) 'K' << 8) | (uint16_t) 'P')
/* South Korea */
#define COUNTRY_CODE_KR (((uint16_t) 'K' << 8) | (uint16_t) 'R')
/* Kuwait */
#define COUNTRY_CODE_KW (((uint16_t) 'K' << 8) | (uint16_t) 'W')
/* Cayman Islands */
#define COUNTRY_CODE_KY (((uint16_t) 'K' << 8) | (uint16_t) 'Y')
/* Kazakhstan */
#define COUNTRY_CODE_KZ (((uint16_t) 'K' << 8) | (uint16_t) 'Z')
/* Laos */
#define COUNTRY_CODE_LA (((uint16_t) 'L' << 8) | (uint16_t) 'A')
/* Lebanon */
#define COUNTRY_CODE_LB (((uint16_t) 'L' << 8) | (uint16_t) 'B')
/* Saint Lucia */
#define COUNTRY_CODE_LC (((uint16_t) 'L' << 8) | (uint16_t) 'C')
/* Liechtenstein */
#define COUNTRY_CODE_LI (((uint16_t) 'L' << 8) | (uint16_t) 'I')
/* Sri Lanka */
#define COUNTRY_CODE_LK (((uint16_t) 'L' << 8) | (uint16_t) 'K')
/* Liberia */
#define COUNTRY_CODE_LR (((uint16_t) 'L' << 8) | (uint16_t) 'R')
/* Lesotho */
#define COUNTRY_CODE_LS (((uint16_t) 'L' << 8) | (uint16_t) 'S')
/* Lithuania */
#define COUNTRY_CODE_LT (((uint16_t) 'L' << 8) | (uint16_t) 'T')
/* Luxemburg */
#define COUNTRY_CODE_LU (((uint16_t) 'L' << 8) | (uint16_t) 'U')
/* Latvia */
#define COUNTRY_CODE_LV (((uint16_t) 'L' << 8) | (uint16_t) 'V')
/* Libya */
#define COUNTRY_CODE_LY (((uint16_t) 'L' << 8) | (uint16_t) 'Y')
/* Morocco */
#define COUNTRY_CODE_MA (((uint16_t) 'M' << 8) | (uint16_t) 'A')
/* Monaco */
#define COUNTRY_CODE_MC (((uint16_t) 'M' << 8) | (uint16_t) 'C')
/* Moldova */
#define COUNTRY_CODE_MD (((uint16_t) 'M' << 8) | (uint16_t) 'D')
/* Montenegro */
#define COUNTRY_CODE_ME (((uint16_t) 'M' << 8) | (uint16_t) 'E')
/* Saint Martin / Sint Marteen (Added on window's list) */
#define COUNTRY_CODE_MF (((uint16_t) 'M' << 8) | (uint16_t) 'F')
/* Madagascar */
#define COUNTRY_CODE_MG (((uint16_t) 'M' << 8) | (uint16_t) 'G')
/* Marshall Islands */
#define COUNTRY_CODE_MH (((uint16_t) 'M' << 8) | (uint16_t) 'H')
/* Macedonia */
#define COUNTRY_CODE_MK (((uint16_t) 'M' << 8) | (uint16_t) 'K')
/* Mali */
#define COUNTRY_CODE_ML (((uint16_t) 'M' << 8) | (uint16_t) 'L')
/* Myanmar */
#define COUNTRY_CODE_MM (((uint16_t) 'M' << 8) | (uint16_t) 'M')
/* Mongolia */
#define COUNTRY_CODE_MN (((uint16_t) 'M' << 8) | (uint16_t) 'N')
/* Macao */
#define COUNTRY_CODE_MO (((uint16_t) 'M' << 8) | (uint16_t) 'O')
/* Northern Mariana Islands (Rota Island Saipan and Tinian Island) */
#define COUNTRY_CODE_MP (((uint16_t) 'M' << 8) | (uint16_t) 'P')
/* Martinique (France) */
#define COUNTRY_CODE_MQ (((uint16_t) 'M' << 8) | (uint16_t) 'Q')
/* Mauritania */
#define COUNTRY_CODE_MR (((uint16_t) 'M' << 8) | (uint16_t) 'R')
/* Montserrat (UK) */
#define COUNTRY_CODE_MS (((uint16_t) 'M' << 8) | (uint16_t) 'S')
/* Malta */
#define COUNTRY_CODE_MT (((uint16_t) 'M' << 8) | (uint16_t) 'T')
/* Mauritius */
#define COUNTRY_CODE_MU (((uint16_t) 'M' << 8) | (uint16_t) 'U')
/* Maldives */
#define COUNTRY_CODE_MV (((uint16_t) 'M' << 8) | (uint16_t) 'V')
/* Malawi */
#define COUNTRY_CODE_MW (((uint16_t) 'M' << 8) | (uint16_t) 'W')
/* Mexico */
#define COUNTRY_CODE_MX (((uint16_t) 'M' << 8) | (uint16_t) 'X')
/* Malaysia */
#define COUNTRY_CODE_MY (((uint16_t) 'M' << 8) | (uint16_t) 'Y')
/* Mozambique */
#define COUNTRY_CODE_MZ (((uint16_t) 'M' << 8) | (uint16_t) 'Z')
/* Namibia */
#define COUNTRY_CODE_NA (((uint16_t) 'N' << 8) | (uint16_t) 'A')
/* New Caledonia */
#define COUNTRY_CODE_NC (((uint16_t) 'N' << 8) | (uint16_t) 'C')
/* Niger */
#define COUNTRY_CODE_NE (((uint16_t) 'N' << 8) | (uint16_t) 'E')
/* Norfolk Island */
#define COUNTRY_CODE_NF (((uint16_t) 'N' << 8) | (uint16_t) 'F')
/* Nigeria */
#define COUNTRY_CODE_NG (((uint16_t) 'N' << 8) | (uint16_t) 'G')
/* Nicaragua */
#define COUNTRY_CODE_NI (((uint16_t) 'N' << 8) | (uint16_t) 'I')
/* Netherlands */
#define COUNTRY_CODE_NL (((uint16_t) 'N' << 8) | (uint16_t) 'L')
/* Norway */
#define COUNTRY_CODE_NO (((uint16_t) 'N' << 8) | (uint16_t) 'O')
/* Nepal */
#define COUNTRY_CODE_NP (((uint16_t) 'N' << 8) | (uint16_t) 'P')
/* Nauru */
#define COUNTRY_CODE_NR (((uint16_t) 'N' << 8) | (uint16_t) 'R')
/* Niue */
#define COUNTRY_CODE_NU (((uint16_t) 'N' << 8) | (uint16_t) 'U')
/* New Zealand */
#define COUNTRY_CODE_NZ (((uint16_t) 'N' << 8) | (uint16_t) 'Z')
/* Oman */
#define COUNTRY_CODE_OM (((uint16_t) 'O' << 8) | (uint16_t) 'M')
/* Panama */
#define COUNTRY_CODE_PA (((uint16_t) 'P' << 8) | (uint16_t) 'A')
/* Peru */
#define COUNTRY_CODE_PE (((uint16_t) 'P' << 8) | (uint16_t) 'E')
/* "French Polynesia */
#define COUNTRY_CODE_PF (((uint16_t) 'P' << 8) | (uint16_t) 'F')
/* Papua New Guinea */
#define COUNTRY_CODE_PG (((uint16_t) 'P' << 8) | (uint16_t) 'G')
/* Philippines */
#define COUNTRY_CODE_PH (((uint16_t) 'P' << 8) | (uint16_t) 'H')
/* Pakistan */
#define COUNTRY_CODE_PK (((uint16_t) 'P' << 8) | (uint16_t) 'K')
/* Poland */
#define COUNTRY_CODE_PL (((uint16_t) 'P' << 8) | (uint16_t) 'L')
/* Saint Pierre and Miquelon */
#define COUNTRY_CODE_PM (((uint16_t) 'P' << 8) | (uint16_t) 'M')
/* Pitcairn Islands  */
#define COUNTRY_CODE_PN (((uint16_t) 'P' << 8) | (uint16_t) 'N')
/* Puerto Rico (USA) */
#define COUNTRY_CODE_PR (((uint16_t) 'P' << 8) | (uint16_t) 'R')
/* Palestinian Authority */
#define COUNTRY_CODE_PS (((uint16_t) 'P' << 8) | (uint16_t) 'S')
/* Portugal */
#define COUNTRY_CODE_PT (((uint16_t) 'P' << 8) | (uint16_t) 'T')
/* Palau */
#define COUNTRY_CODE_PW (((uint16_t) 'P' << 8) | (uint16_t) 'W')
/* Paraguay */
#define COUNTRY_CODE_PY (((uint16_t) 'P' << 8) | (uint16_t) 'Y')
/* Qatar */
#define COUNTRY_CODE_QA (((uint16_t) 'Q' << 8) | (uint16_t) 'A')
/* Reunion (France) */
#define COUNTRY_CODE_RE (((uint16_t) 'R' << 8) | (uint16_t) 'E')
/* Kosvo (Added on window's list) */
#define COUNTRY_CODE_RKS (((uint16_t) 'R' << 8) | (uint16_t) 'K')
/* Romania */
#define COUNTRY_CODE_RO (((uint16_t) 'R' << 8) | (uint16_t) 'O')
/* Serbia */
#define COUNTRY_CODE_RS (((uint16_t) 'R' << 8) | (uint16_t) 'S')
/* Russia */
#define COUNTRY_CODE_RU (((uint16_t) 'R' << 8) | (uint16_t) 'U')
/* Rwanda */
#define COUNTRY_CODE_RW (((uint16_t) 'R' << 8) | (uint16_t) 'W')
/* Saudi Arabia */
#define COUNTRY_CODE_SA (((uint16_t) 'S' << 8) | (uint16_t) 'A')
/* Solomon Islands */
#define COUNTRY_CODE_SB (((uint16_t) 'S' << 8) | (uint16_t) 'B')
/* Seychelles */
#define COUNTRY_CODE_SC (((uint16_t) 'S' << 8) | (uint16_t) 'C')
/* Sudan */
#define COUNTRY_CODE_SD (((uint16_t) 'S' << 8) | (uint16_t) 'D')
/* Sweden */
#define COUNTRY_CODE_SE (((uint16_t) 'S' << 8) | (uint16_t) 'E')
/* Singapole */
#define COUNTRY_CODE_SG (((uint16_t) 'S' << 8) | (uint16_t) 'G')
/* Slovenia */
#define COUNTRY_CODE_SI (((uint16_t) 'S' << 8) | (uint16_t) 'I')
/* Slovakia */
#define COUNTRY_CODE_SK (((uint16_t) 'S' << 8) | (uint16_t) 'K')
/* Sierra Leone */
#define COUNTRY_CODE_SL (((uint16_t) 'S' << 8) | (uint16_t) 'L')
/* San Marino */
#define COUNTRY_CODE_SM (((uint16_t) 'S' << 8) | (uint16_t) 'M')
/* Senegal */
#define COUNTRY_CODE_SN (((uint16_t) 'S' << 8) | (uint16_t) 'N')
/* Somalia */
#define COUNTRY_CODE_SO (((uint16_t) 'S' << 8) | (uint16_t) 'O')
/* Suriname */
#define COUNTRY_CODE_SR (((uint16_t) 'S' << 8) | (uint16_t) 'R')
/* South_Sudan */
#define COUNTRY_CODE_SS (((uint16_t) 'S' << 8) | (uint16_t) 'S')
/* Sao Tome and Principe */
#define COUNTRY_CODE_ST (((uint16_t) 'S' << 8) | (uint16_t) 'T')
/* El Salvador */
#define COUNTRY_CODE_SV (((uint16_t) 'S' << 8) | (uint16_t) 'V')
/* Syria */
#define COUNTRY_CODE_SY (((uint16_t) 'S' << 8) | (uint16_t) 'Y')
/* Swaziland */
#define COUNTRY_CODE_SZ (((uint16_t) 'S' << 8) | (uint16_t) 'Z')
/* Turks and Caicos Islands (UK) */
#define COUNTRY_CODE_TC (((uint16_t) 'T' << 8) | (uint16_t) 'C')
/* Chad */
#define COUNTRY_CODE_TD (((uint16_t) 'T' << 8) | (uint16_t) 'D')
/* French Southern and Antarctic Lands */
#define COUNTRY_CODE_TF (((uint16_t) 'T' << 8) | (uint16_t) 'F')
/* Togo */
#define COUNTRY_CODE_TG (((uint16_t) 'T' << 8) | (uint16_t) 'G')
/* Thailand */
#define COUNTRY_CODE_TH (((uint16_t) 'T' << 8) | (uint16_t) 'H')
/* Tajikistan */
#define COUNTRY_CODE_TJ (((uint16_t) 'T' << 8) | (uint16_t) 'J')
/* East Timor */
#define COUNTRY_CODE_TL (((uint16_t) 'T' << 8) | (uint16_t) 'L')
/* Turkmenistan */
#define COUNTRY_CODE_TM (((uint16_t) 'T' << 8) | (uint16_t) 'M')
/* Tunisia */
#define COUNTRY_CODE_TN (((uint16_t) 'T' << 8) | (uint16_t) 'N')
/* Tonga */
#define COUNTRY_CODE_TO (((uint16_t) 'T' << 8) | (uint16_t) 'O')
/* Turkey */
#define COUNTRY_CODE_TR (((uint16_t) 'T' << 8) | (uint16_t) 'R')
/* Trinidad and Tobago */
#define COUNTRY_CODE_TT (((uint16_t) 'T' << 8) | (uint16_t) 'T')
/* Tuvalu */
#define COUNTRY_CODE_TV (((uint16_t) 'T' << 8) | (uint16_t) 'V')
/* Taiwan */
#define COUNTRY_CODE_TW (((uint16_t) 'T' << 8) | (uint16_t) 'W')
/* Tanzania */
#define COUNTRY_CODE_TZ (((uint16_t) 'T' << 8) | (uint16_t) 'Z')
/* Ukraine */
#define COUNTRY_CODE_UA (((uint16_t) 'U' << 8) | (uint16_t) 'A')
/* Ugnada */
#define COUNTRY_CODE_UG (((uint16_t) 'U' << 8) | (uint16_t) 'G')
/* US */
#define COUNTRY_CODE_US (((uint16_t) 'U' << 8) | (uint16_t) 'S')
/* Uruguay */
#define COUNTRY_CODE_UY (((uint16_t) 'U' << 8) | (uint16_t) 'Y')
/* Uzbekistan */
#define COUNTRY_CODE_UZ (((uint16_t) 'U' << 8) | (uint16_t) 'Z')
/* Vatican (Holy See) */
#define COUNTRY_CODE_VA (((uint16_t) 'V' << 8) | (uint16_t) 'A')
/* Saint Vincent and the Grenadines */
#define COUNTRY_CODE_VC (((uint16_t) 'V' << 8) | (uint16_t) 'C')
/* Venezuela */
#define COUNTRY_CODE_VE (((uint16_t) 'V' << 8) | (uint16_t) 'E')
/* British Virgin Islands */
#define COUNTRY_CODE_VG (((uint16_t) 'V' << 8) | (uint16_t) 'G')
/* US Virgin Islands */
#define COUNTRY_CODE_VI (((uint16_t) 'V' << 8) | (uint16_t) 'I')
/* Vietnam */
#define COUNTRY_CODE_VN (((uint16_t) 'V' << 8) | (uint16_t) 'N')
/* Vanuatu */
#define COUNTRY_CODE_VU (((uint16_t) 'V' << 8) | (uint16_t) 'U')
/* Samoa */
#define COUNTRY_CODE_WS (((uint16_t) 'W' << 8) | (uint16_t) 'S')
/* Yemen */
#define COUNTRY_CODE_YE (((uint16_t) 'Y' << 8) | (uint16_t) 'E')
/* Mayotte (France) */
#define COUNTRY_CODE_YT (((uint16_t) 'Y' << 8) | (uint16_t) 'T')
/* South Africa */
#define COUNTRY_CODE_ZA (((uint16_t) 'Z' << 8) | (uint16_t) 'A')
/* Zambia */
#define COUNTRY_CODE_ZM (((uint16_t) 'Z' << 8) | (uint16_t) 'M')
/* Zimbabwe */
#define COUNTRY_CODE_ZW (((uint16_t) 'Z' << 8) | (uint16_t) 'W')
/* Default country domain */
#define COUNTRY_CODE_DF (((uint16_t) 'D' << 8) | (uint16_t) 'F')
/* World Wide */
#define COUNTRY_CODE_WW (((uint16_t) '0' << 8) | (uint16_t) '0')


/* dot11RegDomainsSupportValue */
#define MIB_REG_DOMAIN_FCC     0x10	/* FCC (US) */
#define MIB_REG_DOMAIN_IC      0x20	/* IC or DOC (Canada) */
#define MIB_REG_DOMAIN_ETSI    0x30	/* ETSI (Europe) */
#define MIB_REG_DOMAIN_SPAIN   0x31	/* Spain */
#define MIB_REG_DOMAIN_FRANCE  0x32	/* France */
#define MIB_REG_DOMAIN_JAPAN   0x40	/* MPHPT (Japan) */
#define MIB_REG_DOMAIN_OTHER   0x00	/* other */

/*2.4G*/
#define BAND_2G4_LOWER_BOUND 1
#define BAND_2G4_UPPER_BOUND 14
/*5G SubBand FCC spec*/
#define UNII1_LOWER_BOUND    36
#define UNII1_UPPER_BOUND    48
#define UNII2A_LOWER_BOUND   52
#define UNII2A_UPPER_BOUND   64
#define UNII2C_LOWER_BOUND   100
#define UNII2C_UPPER_BOUND   144
#define UNII3_LOWER_BOUND    149
#define UNII3_UPPER_BOUND    165

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
#define POWER_LIMIT_TABLE_NULL			0xFFFF
#define MAX_TX_POWER				63
#define MIN_TX_POWER				-64
/*align Frimware Max Power Limit CH Num*/
#define MAX_CMD_SUPPORT_CHANNEL_NUM			64
#endif

#if (CFG_SUPPORT_SINGLE_SKU == 1)
/* ARRAY_SIZE(mtk_2ghz_channels) + ARRAY_SIZE(mtk_5ghz_channels) */
#define MAX_SUPPORTED_CH_COUNT (MAX_CHN_NUM)
#define REG_RULE_LIGHT(start, end, bw, reg_flags)	\
		REG_RULE(start, end, bw, 0, 0, reg_flags)
#endif

/*******************************************************************************
 *  D A T A   T Y P E S
 *******************************************************************************
 */

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY

/* Define Tx Power Control Channel Type */
#define MAX_TX_PWR_CTRL_ELEMENT_NAME_SIZE	16
#define PWR_CTRL_CHNL_TYPE_KEY_ALL		"ALL"
#define PWR_CTRL_CHNL_TYPE_KEY_2G4		"2G4"
#define PWR_CTRL_CHNL_TYPE_KEY_5G		"5G"
#define PWR_CTRL_CHNL_TYPE_KEY_BANDEDGE_2G4	"BANDEDGE2G4"
#define PWR_CTRL_CHNL_TYPE_KEY_BANDEDGE_5G	"BANDEDGE5G"
#define PWR_CTRL_CHNL_TYPE_KEY_5G_BAND1		"5GBAND1"
#define PWR_CTRL_CHNL_TYPE_KEY_5G_BAND2		"5GBAND2"
#define PWR_CTRL_CHNL_TYPE_KEY_5G_BAND3		"5GBAND3"
#define PWR_CTRL_CHNL_TYPE_KEY_5G_BAND4		"5GBAND4"

#define PWR_CFG_PRAM_NUM_ALL_RATE	1

#define PWR_CFG_PRAM_NUM_AX		   18
#if (CFG_SUPPORT_DYNA_TX_PWR_CTRL_OFDM_SETTING == 1)
#define	PWR_CFG_PRAM_NUM_AC			11
#else
#define	PWR_CFG_PRAM_NUM_AC			9
#endif /* CFG_SUPPORT_DYNA_TX_PWR_CTRL_OFDM_SETTING */

enum ENUM_TX_POWER_CTRL_LIST_TYPE {
	PWR_CTRL_TYPE_DEFAULT_LIST = 0,
	PWR_CTRL_TYPE_DYNAMIC_LIST,
	PWR_CTRL_TYPE_ALL_LIST,
};

enum ENUM_TX_POWER_CTRL_APPLIED_WAY {
	PWR_CTRL_TYPE_APPLIED_WAY_WIFION = 1,
	PWR_CTRL_TYPE_APPLIED_WAY_IOCTL,
};

enum ENUM_TX_POWER_CTRL_OPERATION {
	PWR_CTRL_TYPE_OPERATION_POWER_LEVEL = 1,
	PWR_CTRL_TYPE_OPERATION_POWER_OFFSET,
};

enum ENUM_TX_POWER_CTRL_TYPE {
	PWR_CTRL_TYPE_WIFION_POWER_LEVEL = 1,
	PWR_CTRL_TYPE_WIFION_POWER_OFFSET,
	PWR_CTRL_TYPE_IOCTL_POWER_LEVEL,
	PWR_CTRL_TYPE_IOCTL_POWER_OFFSET,
};

enum ENUM_TX_POWER_CTRL_VALUE_SIGN {
	PWR_CTRL_TYPE_NO_ACTION = 0,
	PWR_CTRL_TYPE_POSITIVE,
	PWR_CTRL_TYPE_NEGATIVE,
};

enum ENUM_TX_POWER_CTRL_CHANNEL_TYPE {
	PWR_CTRL_CHNL_TYPE_NORMAL = 0,
	PWR_CTRL_CHNL_TYPE_ALL,
	PWR_CTRL_CHNL_TYPE_RANGE,
	PWR_CTRL_CHNL_TYPE_2G4,
	PWR_CTRL_CHNL_TYPE_5G,
	PWR_CTRL_CHNL_TYPE_BANDEDGE_2G4,
	PWR_CTRL_CHNL_TYPE_BANDEDGE_5G,
	PWR_CTRL_CHNL_TYPE_5G_BAND1,
	PWR_CTRL_CHNL_TYPE_5G_BAND2,
	PWR_CTRL_CHNL_TYPE_5G_BAND3,
	PWR_CTRL_CHNL_TYPE_5G_BAND4,
};

enum ENUM_POWER_LIMIT {
	PWR_LIMIT_CCK,
#if (CFG_SUPPORT_DYNA_TX_PWR_CTRL_OFDM_SETTING == 1)
	PWR_LIMIT_OFDM_L,
	PWR_LIMIT_OFDM_H,
#endif /* CFG_SUPPORT_DYNA_TX_PWR_CTRL_OFDM_SETTING */
	PWR_LIMIT_20M_L,
	PWR_LIMIT_20M_H,
	PWR_LIMIT_40M_L,
	PWR_LIMIT_40M_H,
	PWR_LIMIT_80M_L,
	PWR_LIMIT_80M_H,
	PWR_LIMIT_160M_L,
	PWR_LIMIT_160M_H,
	PWR_LIMIT_NUM
};
enum ENUM_POWER_LIMIT_HE {
	PWR_LIMIT_RU26_L, /* MCS0~4 */
	PWR_LIMIT_RU26_H, /* MCS5~9 */
	PWR_LIMIT_RU26_U, /* MCS10~11 */

	PWR_LIMIT_RU52_L, /* MCS0~4 */
	PWR_LIMIT_RU52_H, /* MCS5~9 */
	PWR_LIMIT_RU52_U, /* MCS10~11 */

	PWR_LIMIT_RU106_L, /* MCS0~4 */
	PWR_LIMIT_RU106_H, /* MCS5~9 */
	PWR_LIMIT_RU106_U, /* MCS10~11 */

	PWR_LIMIT_RU242_L, /* MCS0~4 */
	PWR_LIMIT_RU242_H, /* MCS5~9 */
	PWR_LIMIT_RU242_U, /* MCS10~11 */

	PWR_LIMIT_RU484_L, /* MCS0~4 */
	PWR_LIMIT_RU484_H, /* MCS5~9 */
	PWR_LIMIT_RU484_U, /* MCS10~11 */

	PWR_LIMIT_RU996_L, /* MCS0~4 */
	PWR_LIMIT_RU996_H, /* MCS5~9 */
	PWR_LIMIT_RU996_U, /* MCS10~11 */
	PWR_LIMIT_HE_NUM
};

struct TX_PWR_CTRL_CHANNEL_SETTING {
	enum ENUM_TX_POWER_CTRL_CHANNEL_TYPE eChnlType;
	uint8_t channelParam[2];

	enum ENUM_TX_POWER_CTRL_VALUE_SIGN op[PWR_LIMIT_NUM];
	int8_t i8PwrLimit[PWR_LIMIT_NUM];

	enum ENUM_TX_POWER_CTRL_VALUE_SIGN opHE[PWR_LIMIT_HE_NUM];
	int8_t i8PwrLimitHE[PWR_LIMIT_HE_NUM];
};

struct TX_PWR_CTRL_ELEMENT {
	struct LINK_ENTRY node;
	u_int8_t fgApplied;
	char name[MAX_TX_PWR_CTRL_ELEMENT_NAME_SIZE]; /* scenario name */
	uint8_t index; /* scenario index */
	enum ENUM_TX_POWER_CTRL_TYPE eCtrlType;
	uint8_t settingCount;
	struct TX_PWR_CTRL_CHANNEL_SETTING rChlSettingList[1];
};

struct PARAM_TX_PWR_CTRL_IOCTL {
	u_int8_t fgApplied;
	uint8_t *name;
	uint8_t index;
	uint8_t *newSetting;
};

#endif

enum ENUM_POWER_LIMIT_SUBBAND {
	POWER_LIMIT_2G4 = 0,
	POWER_LIMIT_UNII1 = 1,
	POWER_LIMIT_UNII2A = 2,
	POWER_LIMIT_UNII2C = 3,
	POWER_LIMIT_UNII3 = 4,
	POWER_LIMIT_SUBAND_NUM
};

/* Define channel offset in unit of 5MHz bandwidth */
enum ENUM_CHNL_SPAN {
	CHNL_SPAN_5 = 1,
	CHNL_SPAN_10 = 2,
	CHNL_SPAN_20 = 4,
	CHNL_SPAN_40 = 8
};

/* Define BSS operating bandwidth */
enum ENUM_CHNL_BW {
	CHNL_BW_20,
	CHNL_BW_20_40,
	CHNL_BW_10,
	CHNL_BW_5
};

#if 0
/* If channel width is CHNL_BW_20_40, the first channel will be SCA and
 * the second channel is SCB, then iteratively.
 * Note the final channel will not be SCA.
 */
struct DOMAIN_SUBBAND_INFO {
	uint8_t ucRegClass;
	enum ENUM_BAND eBand;
	enum ENUM_CHNL_SPAN eChannelSpan;
	uint8_t ucFirstChannelNum;
	uint8_t ucNumChannels;
	enum ENUM_CHNL_BW eChannelBw;
	u_int8_t fgDfsNeeded;
	u_int8_t fgIbssProhibited;
};

/* Use it as all available channel list for STA */
struct DOMAIN_INFO_ENTRY {
	uint16_t u2CountryCode;
	uint16_t u2MibRegDomainValue;

	/* If different attributes, put them into different rSubBands.
	 * For example, DFS shall be used or not.
	 */
	struct DOMAIN_SUBBAND_INFO rSubBand[MAX_SUBBAND_NUM];
};

#else /* New definition 20110830 */

/* In all bands, the first channel will be SCA and the second channel is SCB,
 * then iteratively.
 * Note the final channel will not be SCA.
 */
struct DOMAIN_SUBBAND_INFO {
	/* Note1: regulation class depends on operation bandwidth and RF band.
	 *  For example: 2.4GHz, 1~13, 20MHz ==> regulation class = 81
	 *      2.4GHz, 1~13, SCA   ==> regulation class = 83
	 *      2.4GHz, 1~13, SCB   ==> regulation class = 84
	 * Note2: TX power limit is not specified here because path loss
	 *        is unknown.
	 */
	uint8_t ucRegClass;	/* Regulation class for 20MHz */
	uint8_t ucBand;		/* Type: ENUM_BAND_T */
	uint8_t ucChannelSpan;	/* Type: ENUM_CHNL_SPAN_T */
	uint8_t ucFirstChannelNum;
	uint8_t ucNumChannels;
	uint8_t fgDfs;	/* Type: BOOLEAN*/
};

/* Use it as all available channel list for STA */
struct DOMAIN_INFO_ENTRY {
	uint16_t *pu2CountryGroup;
	uint32_t u4CountryNum;

	/* If different attributes, put them into different rSubBands.
	 * For example, DFS shall be used or not.
	 */
	struct DOMAIN_SUBBAND_INFO rSubBand[MAX_SUBBAND_NUM];
};
#endif

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY

#if (CFG_SUPPORT_SINGLE_SKU == 1)
/*
 * MT_TxPwrLimit.dat format
 */
#define SECTION_PREFIX (0x23232323)
#define ELEMENT_PREFIX (0xffff)
#define VERSION (0x00000001)
#define SIZE_OF_VERSION 4
#define WLAN_TX_PWR_LIMIT_FILE_BUF_SIZE 204800

struct tx_pwr_element {
	uint16_t prefix;
	uint8_t channel_num;
	uint8_t reserved;

	/*the followings are in unit: 0.5 dbm*/

	uint8_t tx_pwr_dsss_cck;
	uint8_t tx_pwr_dsss_bpsk;

	uint8_t tx_pwr_ofdm_bpsk; /* 6M, 9M */
	uint8_t tx_pwr_ofdm_qpsk; /* 12M, 18M */
	uint8_t tx_pwr_ofdm_16qam; /* 24M, 36M */
	uint8_t tx_pwr_ofdm_48m;
	uint8_t tx_pwr_ofdm_54m;

	uint8_t tx_pwr_ht20_bpsk; /* MCS0*/
	uint8_t tx_pwr_ht20_qpsk; /* MCS1, MCS2*/
	uint8_t tx_pwr_ht20_16qam; /* MCS3, MCS4*/
	uint8_t tx_pwr_ht20_mcs5; /* MCS5*/
	uint8_t tx_pwr_ht20_mcs6; /* MCS6*/
	uint8_t tx_pwr_ht20_mcs7; /* MCS7*/

	uint8_t tx_pwr_ht40_bpsk; /* MCS0*/
	uint8_t tx_pwr_ht40_qpsk; /* MCS1, MCS2*/
	uint8_t tx_pwr_ht40_16qam; /* MCS3, MCS4*/
	uint8_t tx_pwr_ht40_mcs5; /* MCS5*/
	uint8_t tx_pwr_ht40_mcs6; /* MCS6*/
	uint8_t tx_pwr_ht40_mcs7; /* MCS7*/

	uint8_t tx_pwr_vht20_bpsk; /* MCS0*/
	uint8_t tx_pwr_vht20_qpsk; /* MCS1, MCS2*/
	uint8_t tx_pwr_vht20_16qam; /* MCS3, MCS4*/
	uint8_t tx_pwr_vht20_64qam; /* MCS5, MCS6*/
	uint8_t tx_pwr_vht20_mcs7;
	uint8_t tx_pwr_vht20_mcs8;
	uint8_t tx_pwr_vht20_mcs9;

	uint8_t tx_pwr_vht_40;
	uint8_t tx_pwr_vht_80;
	uint8_t tx_pwr_vht_160nc;
	uint8_t tx_pwr_lg_40;
	uint8_t tx_pwr_lg_80;

	uint8_t tx_pwr_1ss_delta;
	uint8_t reserved_3[3];
};

struct tx_pwr_section {
	uint32_t prefix;
	uint32_t country_code;
};
#endif /*#if (CFG_SUPPORT_SINGLE_SKU == 1)*/

struct COUNTRY_POWER_LIMIT_TABLE_DEFAULT {
	uint8_t aucCountryCode[2];
	/* 0: ch 1 ~14
	 * 1: ch 36 ~48
	 * 2: ch 52 ~64
	 * 3: ch 100 ~144
	 * 4: ch 149 ~165
	 */
	int8_t aucPwrLimitSubBand[POWER_LIMIT_SUBAND_NUM];
	/* bit0: cPwrLimit2G4, bit1: cPwrLimitUnii1; bit2: cPwrLimitUnii2A;*/
	/* bit3: cPwrLimitUnii2C; bit4: cPwrLimitUnii3; mW: 0, mW\MHz : 1 */
	uint8_t ucPwrUnit;
};

struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION {
	uint8_t aucCountryCode[2];
	uint8_t ucCentralCh;
	/* Note: this array doesn't include cPwrLimitOFDM_L & cPwrLimitOFDM_H */
	int8_t aucPwrLimit[PWR_LIMIT_NUM];
};

struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION_HE {
	uint8_t aucCountryCode[2];
	uint8_t ucCentralCh;
	int8_t aucPwrLimit[PWR_LIMIT_HE_NUM];
};


struct SUBBAND_CHANNEL {
	uint8_t ucStartCh;
	uint8_t ucEndCh;
	uint8_t ucInterval;
	uint8_t ucReserved;
};

#endif /* CFG_SUPPORT_PWR_LIMIT_COUNTRY */

#if (CFG_SUPPORT_SINGLE_SKU == 1)
/*
 * Event from chip for single sku
 */
struct SINGLE_SKU_INFO {
	uint32_t u4EfuseCountryCode;
	uint8_t  isEfuseValid;
	uint8_t  ucReserved[7];
};

/*
 * single sku control structure
 */
enum regd_state {
	REGD_STATE_UNDEFINED,
	REGD_STATE_INIT,
	REGD_STATE_SET_WW_CORE,
	REGD_STATE_SET_COUNTRY_USER,
	REGD_STATE_SET_COUNTRY_DRIVER,
	REGD_STATE_SET_COUNTRY_IE,
	REGD_STATE_INVALID
};

enum regd_control_flag {
	REGD_CTRL_FLAG_SUPPORT_LOCAL_REGD_DB = (0x1 << 0)
};

struct mtk_regd_control {
	u_int8_t en;
	u_int8_t isEfuseCountryCodeUsed;
	enum regd_state state;
	u32 alpha2;
	u32 tmp_alpha2; /*store country code set by iwpriv "country XX"*/
	u32 flag; /*enum regd_control_flag*/
	struct GLUE_INFO *pGlueInfo; /*wlan GlueInfo*/
	u8 n_channel_active_2g;
	u8 n_channel_active_5g;
	struct CMD_DOMAIN_CHANNEL channels[MAX_SUPPORTED_CH_COUNT];
	enum nl80211_dfs_regions dfs_region;
};

#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
struct mtk_regdomain {
	char country_code[4];
	const struct ieee80211_regdomain *prRegdRules;
};
#endif

#endif
/*******************************************************************************
 * P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *         P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *      M A C R O S
 *******************************************************************************
 */
#define CAL_CH_OFFSET_80M(_PRIMARY_CH, _CENTRAL_CH) \
			(((_PRIMARY_CH - _CENTRAL_CH) + 6) >> 2)

#define CAL_CH_OFFSET_160M(_PRIMARY_CH, _CENTRAL_CH) \
			(((_PRIMARY_CH - _CENTRAL_CH) + 14) >> 2)

/*******************************************************************************
 * F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
struct DOMAIN_INFO_ENTRY *rlmDomainGetDomainInfo(
	struct ADAPTER *prAdapter);

u_int8_t rlmIsValidChnl(struct ADAPTER *prAdapter, uint8_t ucNumOfChannel,
			enum ENUM_BAND eBand);

void
rlmDomainGetChnlList(struct ADAPTER *prAdapter,
		     enum ENUM_BAND eSpecificBand, u_int8_t fgNoDfs,
		     uint8_t ucMaxChannelNum, uint8_t *pucNumOfChannel,
		     struct RF_CHANNEL_INFO *paucChannelList);

void rlmDomainGetDfsChnls(struct ADAPTER *prAdapter,
			  uint8_t ucMaxChannelNum, uint8_t *pucNumOfChannel,
			  struct RF_CHANNEL_INFO *paucChannelList);

void rlmDomainSendCmd(struct ADAPTER *prAdapter);

void rlmDomainSendDomainInfoCmd(struct ADAPTER *prAdapter);

void rlmDomainSendPassiveScanInfoCmd(struct ADAPTER
				     *prAdapter);

uint32_t rlmDomainSupOperatingClassIeFill(uint8_t *pBuf);

u_int8_t rlmDomainCheckChannelEntryValid(struct ADAPTER
		*prAdapter, uint8_t ucCentralCh);

uint8_t rlmDomainGetCenterChannel(enum ENUM_BAND eBand,
				  uint8_t ucPriChannel,
				  enum ENUM_CHNL_EXT eExtend);

u_int8_t rlmDomainIsValidRfSetting(struct ADAPTER *prAdapter,
				   enum ENUM_BAND eBand, uint8_t ucPriChannel,
				   enum ENUM_CHNL_EXT eExtend,
				   enum ENUM_CHANNEL_WIDTH eChannelWidth,
				   uint8_t ucChannelS1, uint8_t ucChannelS2);

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY

u_int8_t
rlmDomainCheckPowerLimitValid(struct ADAPTER *prAdapter,
			      struct COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION
			      rPowerLimitTableConfiguration,
			      uint8_t ucPwrLimitNum);

void rlmDomainCheckCountryPowerLimitTable(
	struct ADAPTER *prAdapter);

uint16_t rlmDomainPwrLimitDefaultTableDecision(
	struct ADAPTER *prAdapter, uint16_t u2CountryCode);

void rlmDomainSendPwrLimitCmd(struct ADAPTER *prAdapter);
#endif

#if (CFG_SUPPORT_SINGLE_SKU == 1)
extern struct ieee80211_supported_band mtk_band_2ghz;
extern struct ieee80211_supported_band mtk_band_5ghz;

u_int8_t rlmDomainIsCtrlStateEqualTo(enum regd_state state);
u_int8_t rlmDomainIsUsingLocalRegDomainDataBase(void);
enum regd_state rlmDomainStateTransition(enum regd_state
		request_state, struct regulatory_request *pRequest);
void rlmDomainSetCountryCode(char *alpha2,
			     u8 size_of_alpha2);
void rlmDomainSetDfsRegion(enum nl80211_dfs_regions
			   dfs_region);
enum nl80211_dfs_regions rlmDomainGetDfsRegion(void);
void rlmDomainResetCtrlInfo(u_int8_t force);
void rlmDomainAddActiveChannel(u8 band);
u8 rlmDomainGetActiveChannelCount(u8 band);
void rlmDomainParsingChannel(IN struct wiphy *pWiphy);
struct CMD_DOMAIN_CHANNEL *rlmDomainGetActiveChannels(void);
void rlmExtractChannelInfo(u32 max_ch_count,
			   struct CMD_DOMAIN_ACTIVE_CHANNEL_LIST *prBuff);
void regd_set_using_local_regdomain_db(void);
void rlmDomainSetDefaultCountryCode(void);
enum regd_state rlmDomainGetCtrlState(void);
bool rlmDomainIsSameCountryCode(char *alpha2,
				u8 size_of_alpha2);
const struct ieee80211_regdomain
*rlmDomainSearchRegdomainFromLocalDataBase(char *alpha2);
struct GLUE_INFO *rlmDomainGetGlueInfo(void);
bool rlmDomainIsEfuseUsed(void);
uint8_t rlmDomainGetChannelBw(uint8_t channelNum);

#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
extern const struct mtk_regdomain *g_prRegRuleTable[];
#endif

#endif

const struct ieee80211_regdomain
*rlmDomainGetLocalDefaultRegd(void);
void rlmDomainSendInfoToFirmware(IN struct ADAPTER
				 *prAdapter);
uint32_t rlmDomainExtractSingleSkuInfoFromFirmware(
	IN struct ADAPTER *prAdapter, IN uint8_t *pucEventBuf);
u_int8_t regd_is_single_sku_en(void);
u_int8_t rlmDomainIsLegalChannel(struct ADAPTER *prAdapter,
				 enum ENUM_BAND eBand, uint8_t ucChannel);
u_int8_t rlmDomainIsLegalDfsChannel(struct ADAPTER *prAdapter,
		enum ENUM_BAND eBand, uint8_t ucChannel);
enum ENUM_CHNL_EXT rlmSelectSecondaryChannelType(
	struct ADAPTER *prAdapter, enum ENUM_BAND band,
	u8 primary_ch);
void rlmDomainOidSetCountry(IN struct ADAPTER *prAdapter,
			    char *country, u8 size_of_country);
u32 rlmDomainGetCountryCode(void);
u32 rlmDomainGetTempCountryCode(void);
void rlmDomainAssert(u_int8_t cond);

#if CFG_SUPPORT_DYNAMIC_PWR_LIMIT
/* dynamic tx power control */
void txPwrCtrlInit(struct ADAPTER *prAdapter);
void txPwrCtrlLoadConfig(struct ADAPTER *prAdapter);
void txPwrCtrlUninit(struct ADAPTER *prAdapter);
void txPwrCtrlShowList(struct ADAPTER *prAdapter,
				uint8_t filterType,
				char *message);
void txPwrCtrlDeleteElement(struct ADAPTER *prAdapter,
				uint8_t *name, uint32_t index,
				enum ENUM_TX_POWER_CTRL_LIST_TYPE eListType);
struct TX_PWR_CTRL_ELEMENT *txPwrCtrlStringToStruct(char *pcContent,
				u_int8_t fgSkipHeader);
struct TX_PWR_CTRL_ELEMENT *txPwrCtrlFindElement(
				struct ADAPTER *prAdapter,
				uint8_t *name,
				uint32_t index,
				u_int8_t fgCheckIsApplied,
				enum ENUM_TX_POWER_CTRL_LIST_TYPE eListType);
void txPwrCtrlAddElement(struct ADAPTER *prAdapter,
				struct TX_PWR_CTRL_ELEMENT *prElement);
#endif
/*******************************************************************************
 *   F U N C T I O N S
 *******************************************************************************
 */

#endif /* _RLM_DOMAIN_H */
