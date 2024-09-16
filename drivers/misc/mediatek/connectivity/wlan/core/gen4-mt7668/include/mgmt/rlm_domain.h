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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/include/mgmt/rlm_domain.h#1
*/

/*! \file   "rlm_domain.h"
*    \brief
*/


#ifndef _RLM_DOMAIN_H
#define _RLM_DOMAIN_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define MAX_SUBBAND_NUM     6
#define MAX_SUBBAND_NUM_5G  8

#define COUNTRY_CODE_NULL               ((UINT_16)0x0)

/* ISO/IEC 3166-1 two-character country codes */

#define COUNTRY_CODE_AD (((UINT_16) 'A' << 8) | (UINT_16) 'D')	/* Andorra                             */
#define COUNTRY_CODE_AE (((UINT_16) 'A' << 8) | (UINT_16) 'E')	/* UAE                                 */
#define COUNTRY_CODE_AF (((UINT_16) 'A' << 8) | (UINT_16) 'F')	/* Afghanistan                         */
#define COUNTRY_CODE_AG (((UINT_16) 'A' << 8) | (UINT_16) 'G')	/* Antigua & Barbuda                   */
#define COUNTRY_CODE_AI (((UINT_16) 'A' << 8) | (UINT_16) 'I')	/* Anguilla                            */
#define COUNTRY_CODE_AL (((UINT_16) 'A' << 8) | (UINT_16) 'L')	/* Albania                             */
#define COUNTRY_CODE_AM (((UINT_16) 'A' << 8) | (UINT_16) 'M')	/* Armenia                             */
#define COUNTRY_CODE_AN (((UINT_16) 'A' << 8) | (UINT_16) 'N')	/* Netherlands Antilles                */
#define COUNTRY_CODE_AO (((UINT_16) 'A' << 8) | (UINT_16) 'O')	/* Angola                              */
#define COUNTRY_CODE_AR (((UINT_16) 'A' << 8) | (UINT_16) 'R')	/* Argentina                           */
#define COUNTRY_CODE_AS (((UINT_16) 'A' << 8) | (UINT_16) 'S')	/* American Samoa (USA)                */
#define COUNTRY_CODE_AT (((UINT_16) 'A' << 8) | (UINT_16) 'T')	/* Austria                             */
#define COUNTRY_CODE_AU (((UINT_16) 'A' << 8) | (UINT_16) 'U')	/* Australia                           */
#define COUNTRY_CODE_AW (((UINT_16) 'A' << 8) | (UINT_16) 'W')	/* Aruba                               */
#define COUNTRY_CODE_AZ (((UINT_16) 'A' << 8) | (UINT_16) 'Z')	/* Azerbaijan                          */
#define COUNTRY_CODE_BA (((UINT_16) 'B' << 8) | (UINT_16) 'A')	/* Bosnia and Herzegovina              */
#define COUNTRY_CODE_BB (((UINT_16) 'B' << 8) | (UINT_16) 'B')	/* Barbados                            */
#define COUNTRY_CODE_BD (((UINT_16) 'B' << 8) | (UINT_16) 'D')	/* Bangladesh                          */
#define COUNTRY_CODE_BE (((UINT_16) 'B' << 8) | (UINT_16) 'E')	/* Belgium                             */
#define COUNTRY_CODE_BF (((UINT_16) 'B' << 8) | (UINT_16) 'F')	/* Burkina Faso                        */
#define COUNTRY_CODE_BG (((UINT_16) 'B' << 8) | (UINT_16) 'G')	/* Bulgaria                            */
#define COUNTRY_CODE_BH (((UINT_16) 'B' << 8) | (UINT_16) 'H')	/* Bahrain                             */
#define COUNTRY_CODE_BI (((UINT_16) 'B' << 8) | (UINT_16) 'I')	/* Burundi                             */
#define COUNTRY_CODE_BJ (((UINT_16) 'B' << 8) | (UINT_16) 'J')	/* Benin                               */
#define COUNTRY_CODE_BM (((UINT_16) 'B' << 8) | (UINT_16) 'M')	/* Bermuda                             */
#define COUNTRY_CODE_BN (((UINT_16) 'B' << 8) | (UINT_16) 'N')	/* Brunei                              */
#define COUNTRY_CODE_BO (((UINT_16) 'B' << 8) | (UINT_16) 'O')	/* Bolivia                             */
#define COUNTRY_CODE_BR (((UINT_16) 'B' << 8) | (UINT_16) 'R')	/* Brazil                              */
#define COUNTRY_CODE_BS (((UINT_16) 'B' << 8) | (UINT_16) 'S')	/* Bahamas                             */
#define COUNTRY_CODE_BT (((UINT_16) 'B' << 8) | (UINT_16) 'T')	/* Bhutan                              */
#define COUNTRY_CODE_BW (((UINT_16) 'B' << 8) | (UINT_16) 'W')	/* Botswana                            */
#define COUNTRY_CODE_BY (((UINT_16) 'B' << 8) | (UINT_16) 'Y')	/* Belarus                             */
#define COUNTRY_CODE_BZ (((UINT_16) 'B' << 8) | (UINT_16) 'Z')	/* Belize                              */
#define COUNTRY_CODE_CA (((UINT_16) 'C' << 8) | (UINT_16) 'A')	/* Canada                              */
#define COUNTRY_CODE_CD (((UINT_16) 'C' << 8) | (UINT_16) 'D')	/* Congo. Democratic Republic of the   */
#define COUNTRY_CODE_CF (((UINT_16) 'C' << 8) | (UINT_16) 'F')	/* Central African Republic            */
#define COUNTRY_CODE_CG (((UINT_16) 'C' << 8) | (UINT_16) 'G')	/* Congo. Republic of the              */
#define COUNTRY_CODE_CH (((UINT_16) 'C' << 8) | (UINT_16) 'H')	/* Switzerland                         */
#define COUNTRY_CODE_CI (((UINT_16) 'C' << 8) | (UINT_16) 'I')	/* Cote d'lvoire                       */
#define COUNTRY_CODE_CK (((UINT_16) 'C' << 8) | (UINT_16) 'K')	/* Cook Island                         */
#define COUNTRY_CODE_CL (((UINT_16) 'C' << 8) | (UINT_16) 'L')	/* Chile                               */
#define COUNTRY_CODE_CM (((UINT_16) 'C' << 8) | (UINT_16) 'M')	/* Cameroon                            */
#define COUNTRY_CODE_CN (((UINT_16) 'C' << 8) | (UINT_16) 'N')	/* China                               */
#define COUNTRY_CODE_CO (((UINT_16) 'C' << 8) | (UINT_16) 'O')	/* Columbia                            */
#define COUNTRY_CODE_CR (((UINT_16) 'C' << 8) | (UINT_16) 'R')	/* Costa Rica                          */
#define COUNTRY_CODE_CU (((UINT_16) 'C' << 8) | (UINT_16) 'U')	/* Cuba                                */
#define COUNTRY_CODE_CV (((UINT_16) 'C' << 8) | (UINT_16) 'V')	/* Cape Verde                          */
#define COUNTRY_CODE_CX (((UINT_16) 'C' << 8) | (UINT_16) 'X')	/* "Christmas Island(Australia)        */
#define COUNTRY_CODE_CY (((UINT_16) 'C' << 8) | (UINT_16) 'Y')	/* Cyprus                              */
#define COUNTRY_CODE_CZ (((UINT_16) 'C' << 8) | (UINT_16) 'Z')	/* Czech                               */
#define COUNTRY_CODE_DE (((UINT_16) 'D' << 8) | (UINT_16) 'E')	/* Germany                             */
#define COUNTRY_CODE_DJ (((UINT_16) 'D' << 8) | (UINT_16) 'J')	/* Djibouti                            */
#define COUNTRY_CODE_DK (((UINT_16) 'D' << 8) | (UINT_16) 'K')	/* Denmark                             */
#define COUNTRY_CODE_DM (((UINT_16) 'D' << 8) | (UINT_16) 'M')	/* Dominica                            */
#define COUNTRY_CODE_DO (((UINT_16) 'D' << 8) | (UINT_16) 'O')	/* Dominican Republic                  */
#define COUNTRY_CODE_DZ (((UINT_16) 'D' << 8) | (UINT_16) 'Z')	/* Algeria                             */
#define COUNTRY_CODE_EC (((UINT_16) 'E' << 8) | (UINT_16) 'C')	/* Ecuador                             */
#define COUNTRY_CODE_EE (((UINT_16) 'E' << 8) | (UINT_16) 'E')	/* Estonia                             */
#define COUNTRY_CODE_EG (((UINT_16) 'E' << 8) | (UINT_16) 'G')	/* Egypt                               */
#define COUNTRY_CODE_EH (((UINT_16) 'E' << 8) | (UINT_16) 'H')	/* Western Sahara (Morocco)            */
#define COUNTRY_CODE_ER (((UINT_16) 'E' << 8) | (UINT_16) 'R')	/* Eritrea                             */
#define COUNTRY_CODE_ES (((UINT_16) 'E' << 8) | (UINT_16) 'S')	/* Spain                               */
#define COUNTRY_CODE_ET (((UINT_16) 'E' << 8) | (UINT_16) 'T')	/* Ethiopia                            */
#define COUNTRY_CODE_EU (((UINT_16) 'E' << 8) | (UINT_16) 'U')	/* Europe                              */
#define COUNTRY_CODE_FI (((UINT_16) 'F' << 8) | (UINT_16) 'I')	/* Finland                             */
#define COUNTRY_CODE_FJ (((UINT_16) 'F' << 8) | (UINT_16) 'J')	/* Fiji                                */
#define COUNTRY_CODE_FK (((UINT_16) 'F' << 8) | (UINT_16) 'K')	/* Falkland Island                     */
#define COUNTRY_CODE_FM (((UINT_16) 'F' << 8) | (UINT_16) 'M')	/* Micronesia                          */
#define COUNTRY_CODE_FO (((UINT_16) 'F' << 8) | (UINT_16) 'O')	/* Faroe Island                        */
#define COUNTRY_CODE_FR (((UINT_16) 'F' << 8) | (UINT_16) 'R')	/* France                              */
#define COUNTRY_CODE_FR (((UINT_16) 'F' << 8) | (UINT_16) 'R')	/* Wallis and Futuna (France)          */
#define COUNTRY_CODE_GA (((UINT_16) 'G' << 8) | (UINT_16) 'A')	/* Gabon                               */
#define COUNTRY_CODE_GB (((UINT_16) 'G' << 8) | (UINT_16) 'B')	/* United Kingdom                      */
#define COUNTRY_CODE_GD (((UINT_16) 'G' << 8) | (UINT_16) 'D')	/* Grenada                             */
#define COUNTRY_CODE_GE (((UINT_16) 'G' << 8) | (UINT_16) 'E')	/* Georgia                             */
#define COUNTRY_CODE_GF (((UINT_16) 'G' << 8) | (UINT_16) 'F')	/* French Guiana                       */
#define COUNTRY_CODE_GG (((UINT_16) 'G' << 8) | (UINT_16) 'G')	/* Guernsey                            */
#define COUNTRY_CODE_GH (((UINT_16) 'G' << 8) | (UINT_16) 'H')	/* Ghana                               */
#define COUNTRY_CODE_GI (((UINT_16) 'G' << 8) | (UINT_16) 'I')	/* Gibraltar                           */
#define COUNTRY_CODE_GM (((UINT_16) 'G' << 8) | (UINT_16) 'M')	/* Gambia                              */
#define COUNTRY_CODE_GN (((UINT_16) 'G' << 8) | (UINT_16) 'N')	/* Guinea                              */
#define COUNTRY_CODE_GP (((UINT_16) 'G' << 8) | (UINT_16) 'P')	/* Guadeloupe                          */
#define COUNTRY_CODE_GQ (((UINT_16) 'G' << 8) | (UINT_16) 'Q')	/* Equatorial Guinea                   */
#define COUNTRY_CODE_GR (((UINT_16) 'G' << 8) | (UINT_16) 'R')	/* Greece                              */
#define COUNTRY_CODE_GT (((UINT_16) 'G' << 8) | (UINT_16) 'T')	/* Guatemala                           */
#define COUNTRY_CODE_GU (((UINT_16) 'G' << 8) | (UINT_16) 'U')	/* Guam                                */
#define COUNTRY_CODE_GW (((UINT_16) 'G' << 8) | (UINT_16) 'W')	/* Guinea-Bissau                       */
#define COUNTRY_CODE_GY (((UINT_16) 'G' << 8) | (UINT_16) 'Y')	/* Guyana                              */
#define COUNTRY_CODE_HK (((UINT_16) 'H' << 8) | (UINT_16) 'K')	/* Hong Kong                           */
#define COUNTRY_CODE_HN (((UINT_16) 'H' << 8) | (UINT_16) 'N')	/* Honduras                            */
#define COUNTRY_CODE_HR (((UINT_16) 'H' << 8) | (UINT_16) 'R')	/* Croatia                             */
#define COUNTRY_CODE_HT (((UINT_16) 'H' << 8) | (UINT_16) 'T')	/* Haiti                               */
#define COUNTRY_CODE_HU (((UINT_16) 'H' << 8) | (UINT_16) 'U')	/* Hungary                             */
#define COUNTRY_CODE_ID (((UINT_16) 'I' << 8) | (UINT_16) 'D')	/* Indonesia                           */
#define COUNTRY_CODE_IE (((UINT_16) 'I' << 8) | (UINT_16) 'E')	/* Ireland                             */
#define COUNTRY_CODE_IL (((UINT_16) 'I' << 8) | (UINT_16) 'L')	/* Israel                              */
#define COUNTRY_CODE_IM (((UINT_16) 'I' << 8) | (UINT_16) 'M')	/* Isle of Man                         */
#define COUNTRY_CODE_IN (((UINT_16) 'I' << 8) | (UINT_16) 'N')	/* India                               */
#define COUNTRY_CODE_IQ (((UINT_16) 'I' << 8) | (UINT_16) 'Q')	/* Iraq                                */
#define COUNTRY_CODE_IR (((UINT_16) 'I' << 8) | (UINT_16) 'R')	/* Iran                                */
#define COUNTRY_CODE_IS (((UINT_16) 'I' << 8) | (UINT_16) 'S')	/* Iceland                             */
#define COUNTRY_CODE_IT (((UINT_16) 'I' << 8) | (UINT_16) 'T')	/* Italy                               */
#define COUNTRY_CODE_JE (((UINT_16) 'J' << 8) | (UINT_16) 'E')	/* Jersey                              */
#define COUNTRY_CODE_JM (((UINT_16) 'J' << 8) | (UINT_16) 'M')	/* Jameica                             */
#define COUNTRY_CODE_JO (((UINT_16) 'J' << 8) | (UINT_16) 'O')	/* Jordan                              */
#define COUNTRY_CODE_JP (((UINT_16) 'J' << 8) | (UINT_16) 'P')	/* Japan                               */
#define COUNTRY_CODE_KE (((UINT_16) 'K' << 8) | (UINT_16) 'E')	/* Kenya                               */
#define COUNTRY_CODE_KG (((UINT_16) 'K' << 8) | (UINT_16) 'G')	/* Kyrgyzstan                          */
#define COUNTRY_CODE_KH (((UINT_16) 'K' << 8) | (UINT_16) 'H')	/* Cambodia                            */
#define COUNTRY_CODE_KI (((UINT_16) 'K' << 8) | (UINT_16) 'I')	/* Kiribati                            */
#define COUNTRY_CODE_KM (((UINT_16) 'K' << 8) | (UINT_16) 'M')	/* Comoros                             */
#define COUNTRY_CODE_KN (((UINT_16) 'K' << 8) | (UINT_16) 'N')	/* Saint Kitts and Nevis               */
#define COUNTRY_CODE_KP (((UINT_16) 'K' << 8) | (UINT_16) 'P')	/* North Korea                         */
#define COUNTRY_CODE_KR (((UINT_16) 'K' << 8) | (UINT_16) 'R')	/* South Korea                         */
#define COUNTRY_CODE_KW (((UINT_16) 'K' << 8) | (UINT_16) 'W')	/* Kuwait                              */
#define COUNTRY_CODE_KY (((UINT_16) 'K' << 8) | (UINT_16) 'Y')	/* Cayman Islands                      */
#define COUNTRY_CODE_KZ (((UINT_16) 'K' << 8) | (UINT_16) 'Z')	/* Kazakhstan                          */
#define COUNTRY_CODE_LA (((UINT_16) 'L' << 8) | (UINT_16) 'A')	/* Laos                                */
#define COUNTRY_CODE_LB (((UINT_16) 'L' << 8) | (UINT_16) 'B')	/* Lebanon                             */
#define COUNTRY_CODE_LC (((UINT_16) 'L' << 8) | (UINT_16) 'C')	/* Saint Lucia                         */
#define COUNTRY_CODE_LI (((UINT_16) 'L' << 8) | (UINT_16) 'I')	/* Liechtenstein                       */
#define COUNTRY_CODE_LK (((UINT_16) 'L' << 8) | (UINT_16) 'K')	/* Sri Lanka                           */
#define COUNTRY_CODE_LR (((UINT_16) 'L' << 8) | (UINT_16) 'R')	/* Liberia                             */
#define COUNTRY_CODE_LS (((UINT_16) 'L' << 8) | (UINT_16) 'S')	/* Lesotho                             */
#define COUNTRY_CODE_LT (((UINT_16) 'L' << 8) | (UINT_16) 'T')	/* Lithuania                           */
#define COUNTRY_CODE_LU (((UINT_16) 'L' << 8) | (UINT_16) 'U')	/* Luxemburg                           */
#define COUNTRY_CODE_LV (((UINT_16) 'L' << 8) | (UINT_16) 'V')	/* Latvia                              */
#define COUNTRY_CODE_LY (((UINT_16) 'L' << 8) | (UINT_16) 'Y')	/* Libya                               */
#define COUNTRY_CODE_MA (((UINT_16) 'M' << 8) | (UINT_16) 'A')	/* Morocco                             */
#define COUNTRY_CODE_MC (((UINT_16) 'M' << 8) | (UINT_16) 'C')	/* Monaco                              */
#define COUNTRY_CODE_MD (((UINT_16) 'M' << 8) | (UINT_16) 'D')	/* Moldova                             */
#define COUNTRY_CODE_ME (((UINT_16) 'M' << 8) | (UINT_16) 'E')	/* Montenegro                          */
#define COUNTRY_CODE_MF (((UINT_16) 'M' << 8) | (UINT_16) 'F')	/* Saint Martin / Sint Marteen         */
	/*(Added on window's list)             */
#define COUNTRY_CODE_MG (((UINT_16) 'M' << 8) | (UINT_16) 'G')	/* Madagascar                          */
#define COUNTRY_CODE_MH (((UINT_16) 'M' << 8) | (UINT_16) 'H')	/* Marshall Islands                    */
#define COUNTRY_CODE_MK (((UINT_16) 'M' << 8) | (UINT_16) 'K')	/* Macedonia                           */
#define COUNTRY_CODE_ML (((UINT_16) 'M' << 8) | (UINT_16) 'L')	/* Mali                                */
#define COUNTRY_CODE_MM (((UINT_16) 'M' << 8) | (UINT_16) 'M')	/* Myanmar                             */
#define COUNTRY_CODE_MN (((UINT_16) 'M' << 8) | (UINT_16) 'N')	/* Mongolia                            */
#define COUNTRY_CODE_MO (((UINT_16) 'M' << 8) | (UINT_16) 'O')	/* Macao                               */
#define COUNTRY_CODE_MP (((UINT_16) 'M' << 8) | (UINT_16) 'P')	/* Northern Mariana Islands (Rota Island*/
	/*Saipan and Tinian Island)            */
#define COUNTRY_CODE_MQ (((UINT_16) 'M' << 8) | (UINT_16) 'Q')	/* Martinique (France)                 */
#define COUNTRY_CODE_MR (((UINT_16) 'M' << 8) | (UINT_16) 'R')	/* Mauritania                          */
#define COUNTRY_CODE_MS (((UINT_16) 'M' << 8) | (UINT_16) 'S')	/* Montserrat (UK)                     */
#define COUNTRY_CODE_MT (((UINT_16) 'M' << 8) | (UINT_16) 'T')	/* Malta                               */
#define COUNTRY_CODE_MU (((UINT_16) 'M' << 8) | (UINT_16) 'U')	/* Mauritius                           */
#define COUNTRY_CODE_MV (((UINT_16) 'M' << 8) | (UINT_16) 'V')	/* Maldives                            */
#define COUNTRY_CODE_MW (((UINT_16) 'M' << 8) | (UINT_16) 'W')	/* Malawi                              */
#define COUNTRY_CODE_MX (((UINT_16) 'M' << 8) | (UINT_16) 'X')	/* Mexico                              */
#define COUNTRY_CODE_MY (((UINT_16) 'M' << 8) | (UINT_16) 'Y')	/* Malaysia                            */
#define COUNTRY_CODE_MZ (((UINT_16) 'M' << 8) | (UINT_16) 'Z')	/* Mozambique                          */
#define COUNTRY_CODE_NA (((UINT_16) 'N' << 8) | (UINT_16) 'A')	/* Namibia                             */
#define COUNTRY_CODE_NC (((UINT_16) 'N' << 8) | (UINT_16) 'C')	/* New Caledonia                       */
#define COUNTRY_CODE_NE (((UINT_16) 'N' << 8) | (UINT_16) 'E')	/* Niger                               */
#define COUNTRY_CODE_NF (((UINT_16) 'N' << 8) | (UINT_16) 'F')	/* Norfolk Island                      */
#define COUNTRY_CODE_NG (((UINT_16) 'N' << 8) | (UINT_16) 'G')	/* Nigeria                             */
#define COUNTRY_CODE_NI (((UINT_16) 'N' << 8) | (UINT_16) 'I')	/* Nicaragua                           */
#define COUNTRY_CODE_NL (((UINT_16) 'N' << 8) | (UINT_16) 'L')	/* Netherlands                         */
#define COUNTRY_CODE_NO (((UINT_16) 'N' << 8) | (UINT_16) 'O')	/* Norway                              */
#define COUNTRY_CODE_NP (((UINT_16) 'N' << 8) | (UINT_16) 'P')	/* Nepal                               */
#define COUNTRY_CODE_NR (((UINT_16) 'N' << 8) | (UINT_16) 'R')	/* Nauru                               */
#define COUNTRY_CODE_NU (((UINT_16) 'N' << 8) | (UINT_16) 'U')	/* Niue                                */
#define COUNTRY_CODE_NZ (((UINT_16) 'N' << 8) | (UINT_16) 'Z')	/* New Zealand                         */
#define COUNTRY_CODE_OM (((UINT_16) 'O' << 8) | (UINT_16) 'M')	/* Oman                                */
#define COUNTRY_CODE_PA (((UINT_16) 'P' << 8) | (UINT_16) 'A')	/* Panama                              */
#define COUNTRY_CODE_PE (((UINT_16) 'P' << 8) | (UINT_16) 'E')	/* Peru                                */
#define COUNTRY_CODE_PF (((UINT_16) 'P' << 8) | (UINT_16) 'F')	/* "French Polynesia                   */
#define COUNTRY_CODE_PG (((UINT_16) 'P' << 8) | (UINT_16) 'G')	/* Papua New Guinea                    */
#define COUNTRY_CODE_PH (((UINT_16) 'P' << 8) | (UINT_16) 'H')	/* Philippines                         */
#define COUNTRY_CODE_PK (((UINT_16) 'P' << 8) | (UINT_16) 'K')	/* Pakistan                            */
#define COUNTRY_CODE_PL (((UINT_16) 'P' << 8) | (UINT_16) 'L')	/* Poland                              */
#define COUNTRY_CODE_PM (((UINT_16) 'P' << 8) | (UINT_16) 'M')	/* Saint Pierre and Miquelon           */
#define COUNTRY_CODE_PN (((UINT_16) 'P' << 8) | (UINT_16) 'N')	/* Pitcairn Islands                    */
#define COUNTRY_CODE_PR (((UINT_16) 'P' << 8) | (UINT_16) 'R')	/* Puerto Rico (USA)                   */
#define COUNTRY_CODE_PS (((UINT_16) 'P' << 8) | (UINT_16) 'S')	/* Palestinian Authority               */
#define COUNTRY_CODE_PT (((UINT_16) 'P' << 8) | (UINT_16) 'T')	/* Portugal                            */
#define COUNTRY_CODE_PW (((UINT_16) 'P' << 8) | (UINT_16) 'W')	/* Palau                               */
#define COUNTRY_CODE_PY (((UINT_16) 'P' << 8) | (UINT_16) 'Y')	/* Paraguay                            */
#define COUNTRY_CODE_QA (((UINT_16) 'Q' << 8) | (UINT_16) 'A')	/* Qatar                               */
#define COUNTRY_CODE_RE (((UINT_16) 'R' << 8) | (UINT_16) 'E')	/* Reunion (France)                    */
#define COUNTRY_CODE_RKS (((UINT_16) 'R' << 8) | (UINT_16) 'K')	/* Kosvo (Added on window's list)      */
#define COUNTRY_CODE_RO (((UINT_16) 'R' << 8) | (UINT_16) 'O')	/* Romania                             */
#define COUNTRY_CODE_RS (((UINT_16) 'R' << 8) | (UINT_16) 'S')	/* Serbia                              */
#define COUNTRY_CODE_RU (((UINT_16) 'R' << 8) | (UINT_16) 'U')	/* Russia                              */
#define COUNTRY_CODE_RW (((UINT_16) 'R' << 8) | (UINT_16) 'W')	/* Rwanda                              */
#define COUNTRY_CODE_SA (((UINT_16) 'S' << 8) | (UINT_16) 'A')	/* Saudi Arabia                        */
#define COUNTRY_CODE_SB (((UINT_16) 'S' << 8) | (UINT_16) 'B')	/* Solomon Islands                     */
#define COUNTRY_CODE_SC (((UINT_16) 'S' << 8) | (UINT_16) 'C')	/* Seychelles                          */
#define COUNTRY_CODE_SD (((UINT_16) 'S' << 8) | (UINT_16) 'D')	/* Sudan                               */
#define COUNTRY_CODE_SE (((UINT_16) 'S' << 8) | (UINT_16) 'E')	/* Sweden                              */
#define COUNTRY_CODE_SG (((UINT_16) 'S' << 8) | (UINT_16) 'G')	/* Singapole                           */
#define COUNTRY_CODE_SI (((UINT_16) 'S' << 8) | (UINT_16) 'I')	/* Slovenia                            */
#define COUNTRY_CODE_SK (((UINT_16) 'S' << 8) | (UINT_16) 'K')	/* Slovakia                            */
#define COUNTRY_CODE_SL (((UINT_16) 'S' << 8) | (UINT_16) 'L')	/* Sierra Leone                        */
#define COUNTRY_CODE_SM (((UINT_16) 'S' << 8) | (UINT_16) 'M')	/* San Marino                          */
#define COUNTRY_CODE_SN (((UINT_16) 'S' << 8) | (UINT_16) 'N')	/* Senegal                             */
#define COUNTRY_CODE_SO (((UINT_16) 'S' << 8) | (UINT_16) 'O')	/* Somalia                             */
#define COUNTRY_CODE_SR (((UINT_16) 'S' << 8) | (UINT_16) 'R')	/* Suriname                            */
#define COUNTRY_CODE_SS (((UINT_16) 'S' << 8) | (UINT_16) 'S')	/* South_Sudan                         */
#define COUNTRY_CODE_ST (((UINT_16) 'S' << 8) | (UINT_16) 'T')	/* Sao Tome and Principe               */
#define COUNTRY_CODE_SV (((UINT_16) 'S' << 8) | (UINT_16) 'V')	/* El Salvador                         */
#define COUNTRY_CODE_SY (((UINT_16) 'S' << 8) | (UINT_16) 'Y')	/* Syria                               */
#define COUNTRY_CODE_SZ (((UINT_16) 'S' << 8) | (UINT_16) 'Z')	/* Swaziland                           */
#define COUNTRY_CODE_TC (((UINT_16) 'T' << 8) | (UINT_16) 'C')	/* Turks and Caicos Islands (UK)       */
#define COUNTRY_CODE_TD (((UINT_16) 'T' << 8) | (UINT_16) 'D')	/* Chad                                */
#define COUNTRY_CODE_TF (((UINT_16) 'T' << 8) | (UINT_16) 'F')	/* French Southern and Antarctic Lands */
#define COUNTRY_CODE_TG (((UINT_16) 'T' << 8) | (UINT_16) 'G')	/* Togo                                */
#define COUNTRY_CODE_TH (((UINT_16) 'T' << 8) | (UINT_16) 'H')	/* Thailand                            */
#define COUNTRY_CODE_TJ (((UINT_16) 'T' << 8) | (UINT_16) 'J')	/* Tajikistan                          */
#define COUNTRY_CODE_TL (((UINT_16) 'T' << 8) | (UINT_16) 'L')	/* East Timor                          */
#define COUNTRY_CODE_TM (((UINT_16) 'T' << 8) | (UINT_16) 'M')	/* Turkmenistan                        */
#define COUNTRY_CODE_TN (((UINT_16) 'T' << 8) | (UINT_16) 'N')	/* Tunisia                             */
#define COUNTRY_CODE_TO (((UINT_16) 'T' << 8) | (UINT_16) 'O')	/* Tonga                               */
#define COUNTRY_CODE_TR (((UINT_16) 'T' << 8) | (UINT_16) 'R')	/* Turkey                              */
#define COUNTRY_CODE_TT (((UINT_16) 'T' << 8) | (UINT_16) 'T')	/* Trinidad and Tobago                 */
#define COUNTRY_CODE_TV (((UINT_16) 'T' << 8) | (UINT_16) 'V')	/* Tuvalu                              */
#define COUNTRY_CODE_TW (((UINT_16) 'T' << 8) | (UINT_16) 'W')	/* Taiwan                              */
#define COUNTRY_CODE_TZ (((UINT_16) 'T' << 8) | (UINT_16) 'Z')	/* Tanzania                            */
#define COUNTRY_CODE_UA (((UINT_16) 'U' << 8) | (UINT_16) 'A')	/* Ukraine                             */
#define COUNTRY_CODE_UG (((UINT_16) 'U' << 8) | (UINT_16) 'G')	/* Ugnada                              */
#define COUNTRY_CODE_US (((UINT_16) 'U' << 8) | (UINT_16) 'S')	/* US                                  */
#define COUNTRY_CODE_UY (((UINT_16) 'U' << 8) | (UINT_16) 'Y')	/* Uruguay                             */
#define COUNTRY_CODE_UZ (((UINT_16) 'U' << 8) | (UINT_16) 'Z')	/* Uzbekistan                          */
#define COUNTRY_CODE_VA (((UINT_16) 'V' << 8) | (UINT_16) 'A')	/* Vatican (Holy See)                  */
#define COUNTRY_CODE_VC (((UINT_16) 'V' << 8) | (UINT_16) 'C')	/* Saint Vincent and the Grenadines    */
#define COUNTRY_CODE_VE (((UINT_16) 'V' << 8) | (UINT_16) 'E')	/* Venezuela                           */
#define COUNTRY_CODE_VG (((UINT_16) 'V' << 8) | (UINT_16) 'G')	/* British Virgin Islands              */
#define COUNTRY_CODE_VI (((UINT_16) 'V' << 8) | (UINT_16) 'I')	/* US Virgin Islands                   */
#define COUNTRY_CODE_VN (((UINT_16) 'V' << 8) | (UINT_16) 'N')	/* Vietnam                             */
#define COUNTRY_CODE_VU (((UINT_16) 'V' << 8) | (UINT_16) 'U')	/* Vanuatu                             */
#define COUNTRY_CODE_WS (((UINT_16) 'W' << 8) | (UINT_16) 'S')	/* Samoa                               */
#define COUNTRY_CODE_YE (((UINT_16) 'Y' << 8) | (UINT_16) 'E')	/* Yemen                               */
#define COUNTRY_CODE_YT (((UINT_16) 'Y' << 8) | (UINT_16) 'T')	/* Mayotte (France)                    */
#define COUNTRY_CODE_ZA (((UINT_16) 'Z' << 8) | (UINT_16) 'A')	/* South Africa                        */
#define COUNTRY_CODE_ZM (((UINT_16) 'Z' << 8) | (UINT_16) 'M')	/* Zambia                              */
#define COUNTRY_CODE_ZW (((UINT_16) 'Z' << 8) | (UINT_16) 'W')	/* Zimbabwe                            */
#define COUNTRY_CODE_DF (((UINT_16) 'D' << 8) | (UINT_16) 'F')	/* Default country domain  */
#define COUNTRY_CODE_WW (((UINT_16) '0' << 8) | (UINT_16) '0')	/* World Wide  */


/* dot11RegDomainsSupportValue */
#define MIB_REG_DOMAIN_FCC              0x10	/* FCC (US) */
#define MIB_REG_DOMAIN_IC               0x20	/* IC or DOC (Canada) */
#define MIB_REG_DOMAIN_ETSI             0x30	/* ETSI (Europe) */
#define MIB_REG_DOMAIN_SPAIN            0x31	/* Spain */
#define MIB_REG_DOMAIN_FRANCE           0x32	/* France */
#define MIB_REG_DOMAIN_JAPAN            0x40	/* MPHPT (Japan) */
#define MIB_REG_DOMAIN_OTHER            0x00	/* other */

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
#define UNII3_UPPER_BOUND    173

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY

#define POWER_LIMIT_TABLE_NULL			0xFFFF
#define MAX_TX_POWER					63
#define MIN_TX_POWER					-64
#define MAX_CMD_SUPPORT_CHANNEL_NUM	64

#endif

#if (CFG_SUPPORT_SINGLE_SKU == 1)
#define MAX_SUPPORTED_CH_COUNT MAX_CHN_NUM
#define REG_RULE_LIGHT(start, end, bw, reg_flags) REG_RULE(start, end, bw, 0, 0, reg_flags)
#define TX_PWR_LIMIT_CMD_CH_NUM_THRESHOLD 32
#endif
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY

typedef enum _ENUM_POWER_LIMIT_T {
	PWR_LIMIT_CCK = 0,
	PWR_LIMIT_20M = 1,
	PWR_LIMIT_40M = 2,
	PWR_LIMIT_80M = 3,
	PWR_LIMIT_160M = 4,
	PWR_LIMIT_NUM
} ENUM_POWER_LIMIT_T, *P_ENUM_POWER_LIMIT_T;

#endif

typedef enum _ENUM_POWER_LIMIT_SUBBAND_T {
	POWER_LIMIT_2G4 = 0,
	POWER_LIMIT_UNII1 = 1,
	POWER_LIMIT_UNII2A = 2,
	POWER_LIMIT_UNII2C = 3,
	POWER_LIMIT_UNII3 = 4,
	POWER_LIMIT_SUBAND_NUM
} ENUM_POWER_LIMIT_SUBBAND_T, *P_ENUM_POWER_LIMIT_SUBBAND_T;

/* Define channel offset in unit of 5MHz bandwidth */
typedef enum _ENUM_CHNL_SPAN_T {
	CHNL_SPAN_5 = 1,
	CHNL_SPAN_10 = 2,
	CHNL_SPAN_20 = 4,
	CHNL_SPAN_40 = 8
} ENUM_CHNL_SPAN_T, *P_ENUM_CHNL_SPAN_T;

/* Define BSS operating bandwidth */
typedef enum _ENUM_CHNL_BW_T {
	CHNL_BW_20,
	CHNL_BW_20_40,
	CHNL_BW_10,
	CHNL_BW_5
} ENUM_CHNL_BW_T, *P_ENUM_CHNL_BW_T;

#if 0
/* If channel width is CHNL_BW_20_40, the first channel will be SCA and
 * the second channel is SCB, then iteratively.
 * Note the final channel will not be SCA.
 */
typedef struct _DOMAIN_SUBBAND_INFO {
	UINT_8 ucRegClass;
	ENUM_BAND_T eBand;
	ENUM_CHNL_SPAN_T eChannelSpan;
	UINT_8 ucFirstChannelNum;
	UINT_8 ucNumChannels;
	ENUM_CHNL_BW_T eChannelBw;
	BOOLEAN fgDfsNeeded;
	BOOLEAN fgIbssProhibited;
} DOMAIN_SUBBAND_INFO, *P_DOMAIN_SUBBAND_INFO;

/* Use it as all available channel list for STA */
typedef struct _DOMAIN_INFO_ENTRY {
	UINT_16 u2CountryCode;
	UINT_16 u2MibRegDomainValue;

	/* If different attributes, put them into different rSubBands.
	 * For example, DFS shall be used or not.
	 */
	DOMAIN_SUBBAND_INFO rSubBand[MAX_SUBBAND_NUM];
} DOMAIN_INFO_ENTRY, *P_DOMAIN_INFO_ENTRY;

#else /* New definition 20110830 */

/* In all bands, the first channel will be SCA and the second channel is SCB,
 * then iteratively.
 * Note the final channel will not be SCA.
 */
typedef struct _DOMAIN_SUBBAND_INFO {
	/* Note1: regulation class depends on operation bandwidth and RF band.
	 *  For example: 2.4GHz, 1~13, 20MHz ==> regulation class = 81
	 *               2.4GHz, 1~13, SCA   ==> regulation class = 83
	 *               2.4GHz, 1~13, SCB   ==> regulation class = 84
	 * Note2: TX power limit is not specified here because path loss is unknown
	 */
	UINT_8 ucRegClass;	/* Regulation class for 20MHz */
	UINT_8 ucBand;		/* Type: ENUM_BAND_T */
	UINT_8 ucChannelSpan;	/* Type: ENUM_CHNL_SPAN_T */
	UINT_8 ucFirstChannelNum;
	UINT_8 ucNumChannels;
	UINT_8 fgDfs;	/* Type: BOOLEAN*/
} DOMAIN_SUBBAND_INFO, *P_DOMAIN_SUBBAND_INFO;

/* Use it as all available channel list for STA */
typedef struct _DOMAIN_INFO_ENTRY {
	PUINT_16 pu2CountryGroup;
	UINT_32 u4CountryNum;

	/* If different attributes, put them into different rSubBands.
	 * For example, DFS shall be used or not.
	 */
	DOMAIN_SUBBAND_INFO rSubBand[MAX_SUBBAND_NUM];
} DOMAIN_INFO_ENTRY, *P_DOMAIN_INFO_ENTRY;
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
#define WLAN_TX_PWR_LIMIT_FILE_NAME "TxPwrLimit_MT76x8.dat"


struct tx_pwr_element {
	UINT_16 prefix;
	UINT_8 channel_num;
	UINT_8 reserved;

	/*the followings are in unit: 0.5 dbm*/

	UINT_8 tx_pwr_dsss_cck;
	UINT_8 tx_pwr_dsss_bpsk;

	UINT_8 tx_pwr_ofdm_bpsk; /* 6M, 9M */
	UINT_8 tx_pwr_ofdm_qpsk; /* 12M, 18M */
	UINT_8 tx_pwr_ofdm_16qam; /* 24M, 36M */
	UINT_8 tx_pwr_ofdm_48m;
	UINT_8 tx_pwr_ofdm_54m;

	UINT_8 tx_pwr_ht20_bpsk; /* MCS0*/
	UINT_8 tx_pwr_ht20_qpsk; /* MCS1, MCS2*/
	UINT_8 tx_pwr_ht20_16qam; /* MCS3, MCS4*/
	UINT_8 tx_pwr_ht20_mcs5; /* MCS5*/
	UINT_8 tx_pwr_ht20_mcs6; /* MCS6*/
	UINT_8 tx_pwr_ht20_mcs7; /* MCS7*/

	UINT_8 tx_pwr_ht40_bpsk; /* MCS0*/
	UINT_8 tx_pwr_ht40_qpsk; /* MCS1, MCS2*/
	UINT_8 tx_pwr_ht40_16qam; /* MCS3, MCS4*/
	UINT_8 tx_pwr_ht40_mcs5; /* MCS5*/
	UINT_8 tx_pwr_ht40_mcs6; /* MCS6*/
	UINT_8 tx_pwr_ht40_mcs7; /* MCS7*/

	UINT_8 tx_pwr_vht20_bpsk; /* MCS0*/
	UINT_8 tx_pwr_vht20_qpsk; /* MCS1, MCS2*/
	UINT_8 tx_pwr_vht20_16qam; /* MCS3, MCS4*/
	UINT_8 tx_pwr_vht20_64qam; /* MCS5, MCS6*/
	UINT_8 tx_pwr_vht20_mcs7;
	UINT_8 tx_pwr_vht20_mcs8;
	UINT_8 tx_pwr_vht20_mcs9;

	UINT_8 tx_pwr_vht_40;
	UINT_8 tx_pwr_vht_80;
	UINT_8 tx_pwr_vht_160nc;
	UINT_8 tx_pwr_lg_40;
	UINT_8 tx_pwr_lg_80;

	UINT_8 tx_pwr_1ss_delta;
	UINT_8 reserved_3[3];
};

struct tx_pwr_section {
	UINT_32 prefix;
	UINT_32 country_code;
};
#endif /*#if (CFG_SUPPORT_SINGLE_SKU == 1)*/

/* CMD_SET_PWR_LIMIT_TABLE */
typedef struct _CHANNEL_POWER_LIMIT {
	UINT_8 ucCentralCh;
	INT_8 cPwrLimitCCK;
	INT_8 cPwrLimit20;
	INT_8 cPwrLimit40;
	INT_8 cPwrLimit80;
	INT_8 cPwrLimit160;
	UINT_8 ucFlag;
	UINT_8 aucReserved[1];
} CHANNEL_POWER_LIMIT, *P_CHANNEL_POWER_LIMIT;

typedef struct _COUNTRY_CHANNEL_POWER_LIMIT {
	UINT_8 aucCountryCode[2];
	UINT_8 ucCountryFlag;
	UINT_8 ucChannelNum;
	UINT_8 aucReserved[4];
	CHANNEL_POWER_LIMIT rChannelPowerLimit[80];
} COUNTRY_CHANNEL_POWER_LIMIT, *P_COUNTRY_CHANNEL_POWER_LIMIT;

#define CHANNEL_PWR_LIMIT(_channel, _pwrLimit_cck, _pwrLimit_bw20,	\
	_pwrLimit_bw40, _pwrLimit_bw80, _pwrLimit_bw160, _ucFlag)	\
	{                                                  \
	.ucCentralCh           = (_channel),               \
	.cPwrLimitCCK          = (_pwrLimit_cck),          \
	.cPwrLimit20           = (_pwrLimit_bw20),         \
	.cPwrLimit40           = (_pwrLimit_bw40),         \
	.cPwrLimit80           = (_pwrLimit_bw80),         \
	.cPwrLimit160          = (_pwrLimit_bw160),        \
	.ucFlag                = (_ucFlag),                \
	.aucReserved           = {0}                       \
}

typedef struct _COUNTRY_POWER_LIMIT_TABLE_DEFAULT {
	UINT_8 aucCountryCode[2];
	/* 0: ch 1 ~14 , 1: ch 36 ~48, 2: ch 52 ~64, 3: ch 100 ~144, 4: ch 149 ~165 */
	INT_8 aucPwrLimitSubBand[POWER_LIMIT_SUBAND_NUM];
	/* bit0: cPwrLimit2G4, bit1: cPwrLimitUnii1; bit2: cPwrLimitUnii2A;*/
	/* bit3: cPwrLimitUnii2C; bit4: cPwrLimitUnii3; mW: 0, mW\MHz : 1 */
	UINT_8 ucPwrUnit;
} COUNTRY_POWER_LIMIT_TABLE_DEFAULT, *P_COUNTRY_POWER_LIMIT_TABLE_DEFAULT;

typedef struct _COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION {
	UINT_8 aucCountryCode[2];
	UINT_8 ucCentralCh;
	INT_8 aucPwrLimit[PWR_LIMIT_NUM];
} COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION, *P_COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION;

typedef struct _SUBBAND_CHANNEL_T {
	UINT_8 ucStartCh;
	UINT_8 ucEndCh;
	UINT_8 ucInterval;
	UINT_8 ucReserved;
} SUBBAND_CHANNEL_T, *P_SUBBAND_CHANNEL_T;

#endif

#if (CFG_SUPPORT_SINGLE_SKU == 1)
/*
 * Event from chip for single sku
 */
typedef struct _SINGLE_SKU_INFO {
	UINT_32 u4EfuseCountryCode;
	UINT_8  isEfuseValid;
	UINT_8  ucReserved[7];
} SINGLE_SKU_INFO, *P_SINGLE_SKU_INFO;


/*
 * channel structure
 */
struct channel {
	u16 chNum;
	u8	reserved[2];
	u32 flags; /*enum ieee80211_channel_flags*/
};

struct acctive_channel_list {
	u8 n_channels_2g;
	u8 n_channels_5g;
	u8 ucReserved[2];
	struct channel channels[0];
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

typedef struct mtk_regd_control_t {
	BOOLEAN en;
	BOOLEAN isEfuseCountryCodeUsed;
	enum regd_state state;
	u32 alpha2;
	u32 tmp_alpha2; /*store country code set by iwpriv "country XX"*/
	u32 flag; /*enum regd_control_flag*/
	struct wiphy *pRefWiphy; /*log the referenced wiphy*/
	P_GLUE_INFO_T pGlueInfo; /*wlan GlueInfo*/
	u8 n_channel_active_2g;
	u8 n_channel_active_5g;
	struct channel channels[MAX_SUPPORTED_CH_COUNT];
	enum nl80211_dfs_regions dfs_region;
} mtk_regd_control;

#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
struct mtk_regdomain {
	char country_code[4];
	const struct ieee80211_regdomain *prRegdRules;
};
#endif

#endif
/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define CAL_CH_OFFSET_80M(_PRIMARY_CH, _CENTRAL_CH) \
			(((_PRIMARY_CH - _CENTRAL_CH) + 6) >> 2)

#define CAL_CH_OFFSET_160M(_PRIMARY_CH, _CENTRAL_CH) \
			(((_PRIMARY_CH - _CENTRAL_CH) + 14) >> 2)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
P_DOMAIN_INFO_ENTRY rlmDomainGetDomainInfo(P_ADAPTER_T prAdapter);

VOID
rlmDomainGetChnlList(P_ADAPTER_T prAdapter,
		     ENUM_BAND_T eSpecificBand, BOOLEAN fgNoDfs,
		     UINT_8 ucMaxChannelNum, PUINT_8 pucNumOfChannel, P_RF_CHANNEL_INFO_T paucChannelList);

VOID rlmDomainSendCmd(P_ADAPTER_T prAdapter, BOOLEAN fgIsOid);

VOID rlmDomainSendDomainInfoCmd(P_ADAPTER_T prAdapter, BOOLEAN fgIsOid);

VOID rlmDomainSendPassiveScanInfoCmd(P_ADAPTER_T prAdapter, BOOLEAN fgIsOid);

UINT_32 rlmDomainSupOperatingClassIeFill(PUINT_8 pBuf);

BOOLEAN rlmDomainCheckChannelEntryValid(P_ADAPTER_T prAdapter, UINT_8 ucCentralCh);

UINT_8 rlmDomainGetCenterChannel(ENUM_BAND_T eBand, UINT_8 ucPriChannel, ENUM_CHNL_EXT_T eExtend);

BOOLEAN rlmDomainIsValidRfSetting(P_ADAPTER_T prAdapter, ENUM_BAND_T eBand,
				  UINT_8 ucPriChannel, ENUM_CHNL_EXT_T eExtend,
				  ENUM_CHANNEL_WIDTH_T eChannelWidth, UINT_8 ucChannelS1, UINT_8 ucChannelS2);

#if CFG_SUPPORT_PWR_LIMIT_COUNTRY

BOOLEAN
rlmDomainCheckPowerLimitValid(P_ADAPTER_T prAdapter,
			      COUNTRY_POWER_LIMIT_TABLE_CONFIGURATION rPowerLimitTableConfiguration,
			      UINT_8 ucPwrLimitNum);

VOID rlmDomainCheckCountryPowerLimitTable(P_ADAPTER_T prAdapter);

UINT_16 rlmDomainPwrLimitDefaultTableDecision(P_ADAPTER_T prAdapter, UINT_16 u2CountryCode);

VOID rlmDomainSendPwrLimitCmd(P_ADAPTER_T prAdapter);
#endif

#if (CFG_SUPPORT_SINGLE_SKU == 1)
extern struct ieee80211_supported_band mtk_band_2ghz;
extern struct ieee80211_supported_band mtk_band_5ghz;

BOOLEAN rlmDomainIsCtrlStateEqualTo(enum regd_state state);
BOOLEAN rlmDomainIsUsingLocalRegDomainDataBase(void);
enum regd_state rlmDomainStateTransition(enum regd_state request_state, struct regulatory_request *pRequest);
void rlmDomainSetCountryCode(char *alpha2, u8 size_of_alpha2);
void rlmDomainSetDfsRegion(enum nl80211_dfs_regions dfs_region);
enum nl80211_dfs_regions rlmDomainGetDfsRegion(void);
void rlmDomainResetCtrlInfo(void);
void rlmDomainAddActiveChannel(u8 band);
u8 rlmDomainGetActiveChannelCount(u8 band);
void rlmDomainParsingChannel(IN struct wiphy *pWiphy);
struct channel *rlmDomainGetActiveChannels(void);
void rlmExtractChannelInfo(u32 max_ch_count, struct acctive_channel_list *prBuff);
void regd_set_using_local_regdomain_db(void);
void rlmDomainSetDefaultCountryCode(void);
struct wiphy *rlmDomainGetRefWiphy(void);
void rlmDomainSetRefWiphy(struct wiphy *pWiphy);
enum regd_state rlmDomainGetCtrlState(void);
bool rlmDomainIsSameCountryCode(char *alpha2, u8 size_of_alpha2);
const struct ieee80211_regdomain *rlmDomainSearchRegdomainFromLocalDataBase(char *alpha2);
P_GLUE_INFO_T rlmDomainGetGlueInfo(void);
bool rlmDomainIsEfuseUsed(void);
UINT_8 rlmDomainGetChannelBw(UINT_8 channelNum);

#if (CFG_SUPPORT_SINGLE_SKU_LOCAL_DB == 1)
extern const struct mtk_regdomain *g_prRegRuleTable[];
#endif

#endif

const struct ieee80211_regdomain *rlmDomainGetLocalDefaultRegd(void);
void rlmDomainSendInfoToFirmware(IN P_ADAPTER_T prAdapter);
WLAN_STATUS rlmDomainExtractSingleSkuInfoFromFirmware(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf);
BOOLEAN regd_is_single_sku_en(void);
BOOLEAN rlmDomainIsLegalChannel(P_ADAPTER_T prAdapter, ENUM_BAND_T eBand, UINT_8 ucChannel);
ENUM_CHNL_EXT_T rlmSelectSecondaryChannelType(P_ADAPTER_T prAdapter, ENUM_BAND_T band, u8 primary_ch);
extern void mtk_reg_notify(IN struct wiphy *pWiphy,
			   IN struct regulatory_request *pRequest);
void rlmDomainOidSetCountry(IN P_ADAPTER_T prAdapter, char *country, u8 size_of_country);
u32 rlmDomainGetCountryCode(void);
u32 rlmDomainGetTempCountryCode(void);
void rlmDomainAssert(BOOLEAN cond);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _RLM_DOMAIN_H */
