/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _MT_PMIC_6336_UPMU_HW_H_
#define _MT_PMIC_6336_UPMU_HW_H_

#define MT6336_PMIC_REG_BASE (0x0000)

#define MT6336_PMIC_CID                                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x0000))
#define MT6336_PMIC_SWCID                               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0001))
#define MT6336_PMIC_HWCID                               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0002))
#define MT6336_PMIC_TOP_CON                             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0003))
#define MT6336_PMIC_TEST_OUT0                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0004))
#define MT6336_PMIC_TEST_OUT1                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0005))
#define MT6336_PMIC_TEST_CON0                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0006))
#define MT6336_PMIC_TEST_CON1                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0007))
#define MT6336_PMIC_TEST_CON2                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0008))
#define MT6336_PMIC_TEST_CON3                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0009))
#define MT6336_PMIC_TEST_RST_CON                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x000A))
#define MT6336_PMIC_TESTMODE_SW                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x000B))
#define MT6336_PMIC_TOPSTATUS                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x000C))
#define MT6336_PMIC_TDSEL_CON                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x000D))
#define MT6336_PMIC_RDSEL_CON                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x000E))
#define MT6336_PMIC_SMT_CON0                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x000F))
#define MT6336_PMIC_SMT_CON1                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0010))
#define MT6336_PMIC_DRV_CON0                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0011))
#define MT6336_PMIC_DRV_CON1                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0012))
#define MT6336_PMIC_DRV_CON2                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0013))
#define MT6336_PMIC_DRV_CON3                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0014))
#define MT6336_PMIC_DRV_CON4                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0015))
#define MT6336_PMIC_DRV_CON5                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0016))
#define MT6336_PMIC_DRV_CON6                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0017))
#define MT6336_PMIC_TOP_STATUS                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0018))
#define MT6336_PMIC_TOP_STATUS_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0019))
#define MT6336_PMIC_TOP_STATUS_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x001A))
#define MT6336_PMIC_TOP_RSV0                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x001B))
#define MT6336_PMIC_TOP_RSV1                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x001C))
#define MT6336_PMIC_TOP_RSV2                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x001D))
#define MT6336_PMIC_CLK_RSV_CON0                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x001E))
#define MT6336_PMIC_CLK_RSV_CON1                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x001F))
#define MT6336_PMIC_TOP_CLKSQ                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0020))
#define MT6336_PMIC_TOP_CLKSQ_SET                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0021))
#define MT6336_PMIC_TOP_CLKSQ_CLR                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0022))
#define MT6336_PMIC_TOP_CLK300K_TRIM                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0023))
#define MT6336_PMIC_TOP_CLK300K_TRIM_CON0               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0024))
#define MT6336_PMIC_TOP_CLK300K_TRIM_CON1               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0025))
#define MT6336_PMIC_TOP_CLK6M_TRIM                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0026))
#define MT6336_PMIC_TOP_CLK6M_TRIM_CON0                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x0027))
#define MT6336_PMIC_TOP_CLK6M_TRIM_CON1                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x0028))
#define MT6336_PMIC_CLK_CKROOTTST_CON0                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x0029))
#define MT6336_PMIC_CLK_CKROOTTST_CON1                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x002A))
#define MT6336_PMIC_CLK_CKPDN_CON0                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x002B))
#define MT6336_PMIC_CLK_CKPDN_CON0_SET                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x002C))
#define MT6336_PMIC_CLK_CKPDN_CON0_CLR                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x002D))
#define MT6336_PMIC_CLK_CKPDN_CON1                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x002E))
#define MT6336_PMIC_CLK_CKPDN_CON1_SET                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x002F))
#define MT6336_PMIC_CLK_CKPDN_CON1_CLR                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x0030))
#define MT6336_PMIC_CLK_CKPDN_CON2                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0031))
#define MT6336_PMIC_CLK_CKPDN_CON2_SET                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x0032))
#define MT6336_PMIC_CLK_CKPDN_CON2_CLR                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x0033))
#define MT6336_PMIC_CLK_CKPDN_CON3                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0034))
#define MT6336_PMIC_CLK_CKPDN_CON3_SET                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x0035))
#define MT6336_PMIC_CLK_CKPDN_CON3_CLR                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x0036))
#define MT6336_PMIC_CLK_CKPDN_HWEN_CON0                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x0037))
#define MT6336_PMIC_CLK_CKPDN_HWEN_CON0_SET             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0038))
#define MT6336_PMIC_CLK_CKPDN_HWEN_CON0_CLR             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0039))
#define MT6336_PMIC_CLK_CKSEL_CON0                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x003A))
#define MT6336_PMIC_CLK_CKSEL_CON0_SET                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x003B))
#define MT6336_PMIC_CLK_CKSEL_CON0_CLR                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x003C))
#define MT6336_PMIC_CLK_CKDIVSEL_CON0                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x003D))
#define MT6336_PMIC_CLK_CKDIVSEL_CON0_SET               ((unsigned int)(MT6336_PMIC_REG_BASE+0x003E))
#define MT6336_PMIC_CLK_CKDIVSEL_CON0_CLR               ((unsigned int)(MT6336_PMIC_REG_BASE+0x003F))
#define MT6336_PMIC_CLK_CKTSTSEL_CON0                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0040))
#define MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0041))
#define MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0_SET           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0042))
#define MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0_CLR           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0043))
#define MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0044))
#define MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1_SET           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0045))
#define MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1_CLR           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0046))
#define MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0047))
#define MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2_SET           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0048))
#define MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2_CLR           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0049))
#define MT6336_PMIC_CLOCK_RSV0                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x004A))
#define MT6336_PMIC_CLOCK_RSV1                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x004B))
#define MT6336_PMIC_CLOCK_RSV2                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x004C))
#define MT6336_PMIC_TOP_RST_CON0                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x004D))
#define MT6336_PMIC_TOP_RST_CON0_SET                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x004E))
#define MT6336_PMIC_TOP_RST_CON0_CLR                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x004F))
#define MT6336_PMIC_TOP_RST_CON1                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0050))
#define MT6336_PMIC_TOP_RST_CON1_SET                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0051))
#define MT6336_PMIC_TOP_RST_CON1_CLR                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0052))
#define MT6336_PMIC_TOP_RST_CON2                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0053))
#define MT6336_PMIC_TOP_RST_CON2_SET                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0054))
#define MT6336_PMIC_TOP_RST_CON2_CLR                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0055))
#define MT6336_PMIC_TOP_RST_CON3                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0056))
#define MT6336_PMIC_TOP_RST_CON3_SET                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0057))
#define MT6336_PMIC_TOP_RST_CON3_CLR                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0058))
#define MT6336_PMIC_TOP_RST_STATUS                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0059))
#define MT6336_PMIC_TOP_RST_STATUS_SET                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x005A))
#define MT6336_PMIC_TOP_RST_STATUS_CLR                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x005B))
#define MT6336_PMIC_TOP_RST_RSV0                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x005C))
#define MT6336_PMIC_TOP_RST_RSV1                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x005D))
#define MT6336_PMIC_TOP_RST_RSV2                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x005E))
#define MT6336_PMIC_TOP_RST_RSV3                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x005F))
#define MT6336_PMIC_INT_CON0                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0060))
#define MT6336_PMIC_INT_CON0_SET                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0061))
#define MT6336_PMIC_INT_CON0_CLR                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0062))
#define MT6336_PMIC_INT_CON1                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0063))
#define MT6336_PMIC_INT_CON1_SET                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0064))
#define MT6336_PMIC_INT_CON1_CLR                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0065))
#define MT6336_PMIC_INT_CON2                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0066))
#define MT6336_PMIC_INT_CON2_SET                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0067))
#define MT6336_PMIC_INT_CON2_CLR                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0068))
#define MT6336_PMIC_INT_CON3                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0069))
#define MT6336_PMIC_INT_CON3_SET                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x006A))
#define MT6336_PMIC_INT_CON3_CLR                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x006B))
#define MT6336_PMIC_INT_CON4                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x006C))
#define MT6336_PMIC_INT_CON4_SET                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x006D))
#define MT6336_PMIC_INT_CON4_CLR                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x006E))
#define MT6336_PMIC_INT_CON5                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x006F))
#define MT6336_PMIC_INT_CON5_SET                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0070))
#define MT6336_PMIC_INT_CON5_CLR                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0071))
#define MT6336_PMIC_INT_CON6                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0072))
#define MT6336_PMIC_INT_CON6_SET                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0073))
#define MT6336_PMIC_INT_CON6_CLR                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0074))
#define MT6336_PMIC_INT_CON7                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0075))
#define MT6336_PMIC_INT_CON7_SET                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0076))
#define MT6336_PMIC_INT_CON7_CLR                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0077))
#define MT6336_PMIC_INT_CON8                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0078))
#define MT6336_PMIC_INT_CON8_SET                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0079))
#define MT6336_PMIC_INT_CON8_CLR                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x007A))
#define MT6336_PMIC_INT_CON9                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x007B))
#define MT6336_PMIC_INT_CON9_SET                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x007C))
#define MT6336_PMIC_INT_CON9_CLR                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x007D))
#define MT6336_PMIC_INT_MASK_CON0                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x007E))
#define MT6336_PMIC_INT_MASK_CON0_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x007F))
#define MT6336_PMIC_INT_MASK_CON0_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0080))
#define MT6336_PMIC_INT_MASK_CON1                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0081))
#define MT6336_PMIC_INT_MASK_CON1_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0082))
#define MT6336_PMIC_INT_MASK_CON1_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0083))
#define MT6336_PMIC_INT_MASK_CON2                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0084))
#define MT6336_PMIC_INT_MASK_CON2_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0085))
#define MT6336_PMIC_INT_MASK_CON2_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0086))
#define MT6336_PMIC_INT_MASK_CON3                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0087))
#define MT6336_PMIC_INT_MASK_CON3_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0088))
#define MT6336_PMIC_INT_MASK_CON3_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0089))
#define MT6336_PMIC_INT_MASK_CON4                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x008A))
#define MT6336_PMIC_INT_MASK_CON4_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x008B))
#define MT6336_PMIC_INT_MASK_CON4_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x008C))
#define MT6336_PMIC_INT_MASK_CON5                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x008D))
#define MT6336_PMIC_INT_MASK_CON5_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x008E))
#define MT6336_PMIC_INT_MASK_CON5_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x008F))
#define MT6336_PMIC_INT_MASK_CON6                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0090))
#define MT6336_PMIC_INT_MASK_CON6_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0091))
#define MT6336_PMIC_INT_MASK_CON6_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0092))
#define MT6336_PMIC_INT_MASK_CON7                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0093))
#define MT6336_PMIC_INT_MASK_CON7_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0094))
#define MT6336_PMIC_INT_MASK_CON7_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0095))
#define MT6336_PMIC_INT_MASK_CON8                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0096))
#define MT6336_PMIC_INT_MASK_CON8_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0097))
#define MT6336_PMIC_INT_MASK_CON8_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0098))
#define MT6336_PMIC_INT_MASK_CON9                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0099))
#define MT6336_PMIC_INT_MASK_CON9_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x009A))
#define MT6336_PMIC_INT_MASK_CON9_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x009B))
#define MT6336_PMIC_INT_STATUS0                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x009C))
#define MT6336_PMIC_INT_STATUS1                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x009D))
#define MT6336_PMIC_INT_STATUS2                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x009E))
#define MT6336_PMIC_INT_STATUS3                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x009F))
#define MT6336_PMIC_INT_STATUS4                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00A0))
#define MT6336_PMIC_INT_STATUS5                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00A1))
#define MT6336_PMIC_INT_STATUS6                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00A2))
#define MT6336_PMIC_INT_STATUS7                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00A3))
#define MT6336_PMIC_INT_STATUS8                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00A4))
#define MT6336_PMIC_INT_STATUS9                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00A5))
#define MT6336_PMIC_INT_RAW_STATUS0                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x00A6))
#define MT6336_PMIC_INT_RAW_STATUS1                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x00A7))
#define MT6336_PMIC_INT_RAW_STATUS2                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x00A8))
#define MT6336_PMIC_INT_RAW_STATUS3                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x00A9))
#define MT6336_PMIC_INT_RAW_STATUS4                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x00AA))
#define MT6336_PMIC_INT_RAW_STATUS5                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x00AB))
#define MT6336_PMIC_INT_RAW_STATUS6                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x00AC))
#define MT6336_PMIC_INT_RAW_STATUS7                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x00AD))
#define MT6336_PMIC_INT_RAW_STATUS8                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x00AE))
#define MT6336_PMIC_INT_RAW_STATUS9                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x00AF))
#define MT6336_PMIC_FQMTR_CON0                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x00B0))
#define MT6336_PMIC_FQMTR_WIN_L                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00B1))
#define MT6336_PMIC_FQMTR_WIN_U                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00B2))
#define MT6336_PMIC_FQMTR_DAT_L                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00B3))
#define MT6336_PMIC_FQMTR_DAT_U                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00B4))
#define MT6336_PMIC_TOP_RSV3                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x00B5))
#define MT6336_PMIC_TOP_RSV4                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x00B6))
#define MT6336_PMIC_TOP_RSV5                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x00B7))
#define MT6336_PMIC_TOP_RSV6                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x00B8))
#define MT6336_PMIC_TOP_RSV7                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x00B9))
#define MT6336_PMIC_TOP_RSV8                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x00BA))
#define MT6336_PMIC_TOP_RSV9                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x00BB))
#define MT6336_PMIC_TOP_RSV10                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x00BC))
#define MT6336_PMIC_TOP_RSV11                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x00BD))
#define MT6336_PMIC_TOP_RSV12                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x00BE))
#define MT6336_PMIC_TOP_RSV13                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x00BF))
#define MT6336_PMIC_TOP_RSV14                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x00C0))
#define MT6336_PMIC_TOP_RSV15                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x00C1))
#define MT6336_PMIC_ISINKA_ANA_CON_0                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x00C2))
#define MT6336_PMIC_ISINK_CON0                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x00C3))
#define MT6336_PMIC_CHRIND_CON0                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00C4))
#define MT6336_PMIC_CHRIND_CON0_1                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x00C5))
#define MT6336_PMIC_CHRIND_CON1                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00C6))
#define MT6336_PMIC_CHRIND_CON1_1                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x00C7))
#define MT6336_PMIC_CHRIND_CON2                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00C8))
#define MT6336_PMIC_CHRIND_CON2_1                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x00C9))
#define MT6336_PMIC_CHRIND_CON3                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x00CA))
#define MT6336_PMIC_CHRIND_EN_CTRL                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x00CB))
#define MT6336_PMIC_CHRIND_EN_CTRL_1                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x00CC))
#define MT6336_PMIC_CHRIND_EN_CTRL_2                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x00CD))
#define MT6336_PMIC_TYPE_C_PHY_RG_0_0                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0100))
#define MT6336_PMIC_TYPE_C_PHY_RG_0_1                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0101))
#define MT6336_PMIC_TYPE_C_PHY_RG_CC_RESERVE_CSR        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0102))
#define MT6336_PMIC_TYPE_C_VCMP_CTRL                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0104))
#define MT6336_PMIC_TYPE_C_CTRL_0                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0106))
#define MT6336_PMIC_TYPE_C_CTRL_1                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0107))
#define MT6336_PMIC_TYPE_C_CC_SW_CTRL_0                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x010a))
#define MT6336_PMIC_TYPE_C_CC_SW_CTRL_1                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x010b))
#define MT6336_PMIC_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_0   ((unsigned int)(MT6336_PMIC_REG_BASE+0x010c))
#define MT6336_PMIC_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_1   ((unsigned int)(MT6336_PMIC_REG_BASE+0x010d))
#define MT6336_PMIC_TYPE_C_CC_VOL_CC_DEBOUCE_CNT_VAL    ((unsigned int)(MT6336_PMIC_REG_BASE+0x010e))
#define MT6336_PMIC_TYPE_C_CC_VOL_PD_DEBOUCE_CNT_VAL    ((unsigned int)(MT6336_PMIC_REG_BASE+0x010f))
#define MT6336_PMIC_TYPE_C_DRP_SRC_CNT_VAL_0_0          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0110))
#define MT6336_PMIC_TYPE_C_DRP_SRC_CNT_VAL_0_1          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0111))
#define MT6336_PMIC_TYPE_C_DRP_SNK_CNT_VAL_0_0          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0114))
#define MT6336_PMIC_TYPE_C_DRP_SNK_CNT_VAL_0_1          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0115))
#define MT6336_PMIC_TYPE_C_DRP_TRY_CNT_VAL_0_0          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0118))
#define MT6336_PMIC_TYPE_C_DRP_TRY_CNT_VAL_0_1          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0119))
#define MT6336_PMIC_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_0     ((unsigned int)(MT6336_PMIC_REG_BASE+0x011c))
#define MT6336_PMIC_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_1     ((unsigned int)(MT6336_PMIC_REG_BASE+0x011d))
#define MT6336_PMIC_TYPE_C_DRP_TRY_WAIT_CNT_VAL_1       ((unsigned int)(MT6336_PMIC_REG_BASE+0x011e))
#define MT6336_PMIC_TYPE_C_CC_SRC_VOPEN_DEFAULT_DAC_VAL ((unsigned int)(MT6336_PMIC_REG_BASE+0x0120))
#define MT6336_PMIC_TYPE_C_CC_SRC_VRD_DEFAULT_DAC_VAL   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0121))
#define MT6336_PMIC_TYPE_C_CC_SRC_VOPEN_15_DAC_VAL      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0122))
#define MT6336_PMIC_TYPE_C_CC_SRC_VRD_15_DAC_VAL        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0123))
#define MT6336_PMIC_TYPE_C_CC_SRC_VOPEN_30_DAC_VAL      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0124))
#define MT6336_PMIC_TYPE_C_CC_SRC_VRD_30_DAC_VAL        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0125))
#define MT6336_PMIC_TYPE_C_CC_SNK_VRP30_DAC_VAL         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0128))
#define MT6336_PMIC_TYPE_C_CC_SNK_VRP15_DAC_VAL         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0129))
#define MT6336_PMIC_TYPE_C_CC_SNK_VRPUSB_DAC_VAL        ((unsigned int)(MT6336_PMIC_REG_BASE+0x012a))
#define MT6336_PMIC_TYPE_C_INTR_EN_0                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0130))
#define MT6336_PMIC_TYPE_C_INTR_EN_1                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0131))
#define MT6336_PMIC_TYPE_C_INTR_EN_2                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0134))
#define MT6336_PMIC_TYPE_C_INTR_0                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0138))
#define MT6336_PMIC_TYPE_C_INTR_1                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0139))
#define MT6336_PMIC_TYPE_C_INTR_2                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x013C))
#define MT6336_PMIC_TYPE_C_CC_STATUS_0                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x0140))
#define MT6336_PMIC_TYPE_C_CC_STATUS_1                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x0141))
#define MT6336_PMIC_TYPE_C_PWR_STATUS                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0142))
#define MT6336_PMIC_TYPE_C_PHY_RG_CC1_RESISTENCE_RPDE   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0144))
#define MT6336_PMIC_TYPE_C_PHY_RG_CC1_RESISTENCE_RP15   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0145))
#define MT6336_PMIC_TYPE_C_PHY_RG_CC1_RESISTENCE_RP3    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0146))
#define MT6336_PMIC_TYPE_C_PHY_RG_CC1_RESISTENCE_RD     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0147))
#define MT6336_PMIC_TYPE_C_PHY_RG_CC2_RESISTENCE_RPDE   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0148))
#define MT6336_PMIC_TYPE_C_PHY_RG_CC2_RESISTENCE_RP15   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0149))
#define MT6336_PMIC_TYPE_C_PHY_RG_CC2_RESISTENCE_RP3    ((unsigned int)(MT6336_PMIC_REG_BASE+0x014a))
#define MT6336_PMIC_TYPE_C_PHY_RG_CC2_RESISTENCE_RD     ((unsigned int)(MT6336_PMIC_REG_BASE+0x014b))
#define MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_0    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0160))
#define MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_1    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0161))
#define MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_0       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0164))
#define MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_1       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0165))
#define MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_2       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0166))
#define MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_3       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0167))
#define MT6336_PMIC_TYPE_C_CC_DAC_CALI_CTRL             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0170))
#define MT6336_PMIC_TYPE_C_CC_DAC_CALI_RESULT           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0172))
#define MT6336_PMIC_TYPE_C_DEBUG_PORT_SELECT_0_0        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0180))
#define MT6336_PMIC_TYPE_C_DEBUG_PORT_SELECT_0_1        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0181))
#define MT6336_PMIC_TYPE_C_DEBUG_PORT_SELECT_1_0        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0182))
#define MT6336_PMIC_TYPE_C_DEBUG_PORT_SELECT_1_1        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0183))
#define MT6336_PMIC_TYPE_C_DEBUG_MODE_SELECT_0          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0184))
#define MT6336_PMIC_TYPE_C_DEBUG_MODE_SELECT_1          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0185))
#define MT6336_PMIC_TYPE_C_DEBUG_OUT_READ_0_0           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0188))
#define MT6336_PMIC_TYPE_C_DEBUG_OUT_READ_0_1           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0189))
#define MT6336_PMIC_TYPE_C_DEBUG_OUT_READ_1_0           ((unsigned int)(MT6336_PMIC_REG_BASE+0x018a))
#define MT6336_PMIC_TYPE_C_DEBUG_OUT_READ_1_1           ((unsigned int)(MT6336_PMIC_REG_BASE+0x018b))
#define MT6336_PMIC_TYPE_C_SW_DEBUG_PORT_0_0            ((unsigned int)(MT6336_PMIC_REG_BASE+0x018c))
#define MT6336_PMIC_TYPE_C_SW_DEBUG_PORT_0_1            ((unsigned int)(MT6336_PMIC_REG_BASE+0x018d))
#define MT6336_PMIC_TYPE_C_SW_DEBUG_PORT_1_0            ((unsigned int)(MT6336_PMIC_REG_BASE+0x018e))
#define MT6336_PMIC_TYPE_C_SW_DEBUG_PORT_1_1            ((unsigned int)(MT6336_PMIC_REG_BASE+0x018f))
#define MT6336_PMIC_PD_TX_PARAMETER_0                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0204))
#define MT6336_PMIC_PD_TX_PARAMETER_1                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0205))
#define MT6336_PMIC_PD_TX_DATA_OBJECT0_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0210))
#define MT6336_PMIC_PD_TX_DATA_OBJECT0_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0211))
#define MT6336_PMIC_PD_TX_DATA_OBJECT0_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0212))
#define MT6336_PMIC_PD_TX_DATA_OBJECT0_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0213))
#define MT6336_PMIC_PD_TX_DATA_OBJECT1_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0214))
#define MT6336_PMIC_PD_TX_DATA_OBJECT1_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0215))
#define MT6336_PMIC_PD_TX_DATA_OBJECT1_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0216))
#define MT6336_PMIC_PD_TX_DATA_OBJECT1_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0217))
#define MT6336_PMIC_PD_TX_DATA_OBJECT2_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0218))
#define MT6336_PMIC_PD_TX_DATA_OBJECT2_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0219))
#define MT6336_PMIC_PD_TX_DATA_OBJECT2_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x021a))
#define MT6336_PMIC_PD_TX_DATA_OBJECT2_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x021b))
#define MT6336_PMIC_PD_TX_DATA_OBJECT3_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x021c))
#define MT6336_PMIC_PD_TX_DATA_OBJECT3_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x021d))
#define MT6336_PMIC_PD_TX_DATA_OBJECT3_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x021e))
#define MT6336_PMIC_PD_TX_DATA_OBJECT3_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x021f))
#define MT6336_PMIC_PD_TX_DATA_OBJECT4_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0220))
#define MT6336_PMIC_PD_TX_DATA_OBJECT4_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0221))
#define MT6336_PMIC_PD_TX_DATA_OBJECT4_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0222))
#define MT6336_PMIC_PD_TX_DATA_OBJECT4_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0223))
#define MT6336_PMIC_PD_TX_DATA_OBJECT5_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0224))
#define MT6336_PMIC_PD_TX_DATA_OBJECT5_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0225))
#define MT6336_PMIC_PD_TX_DATA_OBJECT5_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0226))
#define MT6336_PMIC_PD_TX_DATA_OBJECT5_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0227))
#define MT6336_PMIC_PD_TX_DATA_OBJECT6_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0228))
#define MT6336_PMIC_PD_TX_DATA_OBJECT6_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0229))
#define MT6336_PMIC_PD_TX_DATA_OBJECT6_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x022a))
#define MT6336_PMIC_PD_TX_DATA_OBJECT6_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x022b))
#define MT6336_PMIC_PD_TX_HEADER_0                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x022c))
#define MT6336_PMIC_PD_TX_HEADER_1                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x022d))
#define MT6336_PMIC_PD_TX_CTRL                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x022e))
#define MT6336_PMIC_PD_RX_PARAMETER_0                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0244))
#define MT6336_PMIC_PD_RX_PARAMETER_1                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0245))
#define MT6336_PMIC_PD_RX_PREAMBLE_PROTECT_PARAMETER_0  ((unsigned int)(MT6336_PMIC_REG_BASE+0x0248))
#define MT6336_PMIC_PD_RX_PREAMBLE_PROTECT_PARAMETER_1  ((unsigned int)(MT6336_PMIC_REG_BASE+0x024c))
#define MT6336_PMIC_PD_RX_PREAMBLE_PROTECT_PARAMETER_2  ((unsigned int)(MT6336_PMIC_REG_BASE+0x024d))
#define MT6336_PMIC_PD_RX_STATUS                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x024f))
#define MT6336_PMIC_PD_RX_DATA_OBJECT0_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0250))
#define MT6336_PMIC_PD_RX_DATA_OBJECT0_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0251))
#define MT6336_PMIC_PD_RX_DATA_OBJECT0_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0252))
#define MT6336_PMIC_PD_RX_DATA_OBJECT0_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0253))
#define MT6336_PMIC_PD_RX_DATA_OBJECT1_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0254))
#define MT6336_PMIC_PD_RX_DATA_OBJECT1_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0255))
#define MT6336_PMIC_PD_RX_DATA_OBJECT1_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0256))
#define MT6336_PMIC_PD_RX_DATA_OBJECT1_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0257))
#define MT6336_PMIC_PD_RX_DATA_OBJECT2_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0258))
#define MT6336_PMIC_PD_RX_DATA_OBJECT2_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0259))
#define MT6336_PMIC_PD_RX_DATA_OBJECT2_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x025a))
#define MT6336_PMIC_PD_RX_DATA_OBJECT2_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x025b))
#define MT6336_PMIC_PD_RX_DATA_OBJECT3_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x025c))
#define MT6336_PMIC_PD_RX_DATA_OBJECT3_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x025d))
#define MT6336_PMIC_PD_RX_DATA_OBJECT3_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x025e))
#define MT6336_PMIC_PD_RX_DATA_OBJECT3_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x025f))
#define MT6336_PMIC_PD_RX_DATA_OBJECT4_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0260))
#define MT6336_PMIC_PD_RX_DATA_OBJECT4_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0261))
#define MT6336_PMIC_PD_RX_DATA_OBJECT4_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0262))
#define MT6336_PMIC_PD_RX_DATA_OBJECT4_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0263))
#define MT6336_PMIC_PD_RX_DATA_OBJECT5_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0264))
#define MT6336_PMIC_PD_RX_DATA_OBJECT5_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0265))
#define MT6336_PMIC_PD_RX_DATA_OBJECT5_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0266))
#define MT6336_PMIC_PD_RX_DATA_OBJECT5_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0267))
#define MT6336_PMIC_PD_RX_DATA_OBJECT6_0_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0268))
#define MT6336_PMIC_PD_RX_DATA_OBJECT6_0_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0269))
#define MT6336_PMIC_PD_RX_DATA_OBJECT6_1_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x026a))
#define MT6336_PMIC_PD_RX_DATA_OBJECT6_1_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x026b))
#define MT6336_PMIC_PD_RX_HEADER_0                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x026c))
#define MT6336_PMIC_PD_RX_HEADER_1                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x026d))
#define MT6336_PMIC_PD_RX_1P25X_UI_TRAIN_RESULT_0       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0270))
#define MT6336_PMIC_PD_RX_1P25X_UI_TRAIN_RESULT_1       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0271))
#define MT6336_PMIC_PD_RX_RCV_BUF_SW_RST                ((unsigned int)(MT6336_PMIC_REG_BASE+0x0274))
#define MT6336_PMIC_PD_HR_CTRL                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0280))
#define MT6336_PMIC_PD_AD_STATUS                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0284))
#define MT6336_PMIC_PD_CRC_RCV_TIMEOUT_VAL_0_0          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0290))
#define MT6336_PMIC_PD_CRC_RCV_TIMEOUT_VAL_0_1          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0291))
#define MT6336_PMIC_PD_CRC_RCV_TIMEOUT_VAL_1_0          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0292))
#define MT6336_PMIC_PD_CRC_RCV_TIMEOUT_VAL_1_1          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0293))
#define MT6336_PMIC_PD_HR_COMPLETE_TIMEOUT_VAL_0_0      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0294))
#define MT6336_PMIC_PD_HR_COMPLETE_TIMEOUT_VAL_0_1      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0295))
#define MT6336_PMIC_PD_HR_COMPLETE_TIMEOUT_VAL_1_0      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0296))
#define MT6336_PMIC_PD_HR_COMPLETE_TIMEOUT_VAL_1_1      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0297))
#define MT6336_PMIC_PD_IDLE_DETECTION_0_0               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0298))
#define MT6336_PMIC_PD_IDLE_DETECTION_0_1               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0299))
#define MT6336_PMIC_PD_IDLE_DETECTION_1_0               ((unsigned int)(MT6336_PMIC_REG_BASE+0x029a))
#define MT6336_PMIC_PD_IDLE_DETECTION_1_1               ((unsigned int)(MT6336_PMIC_REG_BASE+0x029b))
#define MT6336_PMIC_PD_INTERFRAMEGAP_VAL_0              ((unsigned int)(MT6336_PMIC_REG_BASE+0x029c))
#define MT6336_PMIC_PD_INTERFRAMEGAP_VAL_1              ((unsigned int)(MT6336_PMIC_REG_BASE+0x029d))
#define MT6336_PMIC_PD_RX_GLITCH_MASK_WINDOW            ((unsigned int)(MT6336_PMIC_REG_BASE+0x029e))
#define MT6336_PMIC_PD_RX_UI_1P25_ADJ                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x029f))
#define MT6336_PMIC_PD_TIMER0_VAL_0_0                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x02a0))
#define MT6336_PMIC_PD_TIMER0_VAL_0_1                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x02a1))
#define MT6336_PMIC_PD_TIMER0_VAL_1_0                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x02a2))
#define MT6336_PMIC_PD_TIMER0_VAL_1_1                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x02a3))
#define MT6336_PMIC_PD_TIMER0_ENABLE                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x02a4))
#define MT6336_PMIC_PD_TIMER1_VAL_0                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x02a8))
#define MT6336_PMIC_PD_TIMER1_VAL_1                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x02a9))
#define MT6336_PMIC_PD_TIMER1_TICK_CNT_0                ((unsigned int)(MT6336_PMIC_REG_BASE+0x02aa))
#define MT6336_PMIC_PD_TIMER1_TICK_CNT_1                ((unsigned int)(MT6336_PMIC_REG_BASE+0x02ab))
#define MT6336_PMIC_PD_TIMER1_ENABLE                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x02ac))
#define MT6336_PMIC_PD_TX_SLEW_RATE_CALI_CTRL           ((unsigned int)(MT6336_PMIC_REG_BASE+0x02b0))
#define MT6336_PMIC_PD_TX_SLEW_CK_STABLE_TIME_0         ((unsigned int)(MT6336_PMIC_REG_BASE+0x02b2))
#define MT6336_PMIC_PD_TX_SLEW_CK_STABLE_TIME_1         ((unsigned int)(MT6336_PMIC_REG_BASE+0x02b3))
#define MT6336_PMIC_PD_TX_MON_CK_TARGET_0               ((unsigned int)(MT6336_PMIC_REG_BASE+0x02b4))
#define MT6336_PMIC_PD_TX_MON_CK_TARGET_1               ((unsigned int)(MT6336_PMIC_REG_BASE+0x02b5))
#define MT6336_PMIC_PD_TX_SLEW_CK_TARGET                ((unsigned int)(MT6336_PMIC_REG_BASE+0x02b6))
#define MT6336_PMIC_PD_TX_SLEW_RATE_CALI_RESULT         ((unsigned int)(MT6336_PMIC_REG_BASE+0x02b8))
#define MT6336_PMIC_PD_TX_SLEW_RATE_FM_OUT_0            ((unsigned int)(MT6336_PMIC_REG_BASE+0x02ba))
#define MT6336_PMIC_PD_TX_SLEW_RATE_FM_OUT_1            ((unsigned int)(MT6336_PMIC_REG_BASE+0x02bb))
#define MT6336_PMIC_PD_LOOPBACK_CTRL                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x02bc))
#define MT6336_PMIC_PD_LOOPBACK_ERR_CNT                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x02bd))
#define MT6336_PMIC_PD_MSG_ID_SW_MODE                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x02be))
#define MT6336_PMIC_PD_INTR_EN_0                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x02c0))
#define MT6336_PMIC_PD_INTR_EN_1                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x02c1))
#define MT6336_PMIC_PD_INTR_EN_2                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x02c2))
#define MT6336_PMIC_PD_INTR_EN_3                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x02c3))
#define MT6336_PMIC_PD_INTR_0                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x02c8))
#define MT6336_PMIC_PD_INTR_1                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x02c9))
#define MT6336_PMIC_PD_INTR_2                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x02ca))
#define MT6336_PMIC_PD_INTR_3                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x02cb))
#define MT6336_PMIC_PD_PHY_RG_PD_0                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x02f0))
#define MT6336_PMIC_PD_PHY_RG_PD_1                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x02f1))
#define MT6336_PMIC_PD_PHY_RG_PD_2                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x02f2))
#define MT6336_PMIC_PD_PHY_RG_PD_3                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x02f3))
#define MT6336_PMIC_PD_PHY_SW_FORCE_MODE_ENABLE         ((unsigned int)(MT6336_PMIC_REG_BASE+0x02f4))
#define MT6336_PMIC_PD_PHY_SW_FORCE_MODE_VAL_0          ((unsigned int)(MT6336_PMIC_REG_BASE+0x02f8))
#define MT6336_PMIC_AUXADC_ADC0                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0300))
#define MT6336_PMIC_AUXADC_ADC0_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0301))
#define MT6336_PMIC_AUXADC_ADC1                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0302))
#define MT6336_PMIC_AUXADC_ADC1_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0303))
#define MT6336_PMIC_AUXADC_ADC2                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0304))
#define MT6336_PMIC_AUXADC_ADC2_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0305))
#define MT6336_PMIC_AUXADC_ADC3                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0306))
#define MT6336_PMIC_AUXADC_ADC3_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0307))
#define MT6336_PMIC_AUXADC_ADC4                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0308))
#define MT6336_PMIC_AUXADC_ADC4_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0309))
#define MT6336_PMIC_AUXADC_ADC5                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x030A))
#define MT6336_PMIC_AUXADC_ADC5_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x030B))
#define MT6336_PMIC_AUXADC_ADC6                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x030C))
#define MT6336_PMIC_AUXADC_ADC6_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x030D))
#define MT6336_PMIC_AUXADC_ADC7                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x030E))
#define MT6336_PMIC_AUXADC_ADC7_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x030F))
#define MT6336_PMIC_AUXADC_ADC8                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0310))
#define MT6336_PMIC_AUXADC_ADC8_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0311))
#define MT6336_PMIC_AUXADC_ADC9                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0312))
#define MT6336_PMIC_AUXADC_ADC9_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0313))
#define MT6336_PMIC_AUXADC_ADC10                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0314))
#define MT6336_PMIC_AUXADC_ADC10_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0315))
#define MT6336_PMIC_AUXADC_ADC11                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0316))
#define MT6336_PMIC_AUXADC_ADC11_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0317))
#define MT6336_PMIC_AUXADC_ADC12                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0318))
#define MT6336_PMIC_AUXADC_ADC12_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0319))
#define MT6336_PMIC_AUXADC_ADC13                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x031A))
#define MT6336_PMIC_AUXADC_ADC13_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x031B))
#define MT6336_PMIC_AUXADC_ADC14                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x031C))
#define MT6336_PMIC_AUXADC_ADC14_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x031D))
#define MT6336_PMIC_AUXADC_ADC15                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x031E))
#define MT6336_PMIC_AUXADC_ADC15_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x031F))
#define MT6336_PMIC_AUXADC_ADC16                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0320))
#define MT6336_PMIC_AUXADC_ADC16_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0321))
#define MT6336_PMIC_AUXADC_ADC17                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0322))
#define MT6336_PMIC_AUXADC_ADC17_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0323))
#define MT6336_PMIC_AUXADC_ADC18                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0324))
#define MT6336_PMIC_AUXADC_ADC18_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0325))
#define MT6336_PMIC_AUXADC_ADC19                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0326))
#define MT6336_PMIC_AUXADC_ADC19_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0327))
#define MT6336_PMIC_AUXADC_ADC20                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0328))
#define MT6336_PMIC_AUXADC_ADC20_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0329))
#define MT6336_PMIC_AUXADC_ADC21                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x032A))
#define MT6336_PMIC_AUXADC_ADC21_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x032B))
#define MT6336_PMIC_AUXADC_ADC22                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x032C))
#define MT6336_PMIC_AUXADC_ADC22_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x032D))
#define MT6336_PMIC_AUXADC_ADC23                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x032E))
#define MT6336_PMIC_AUXADC_ADC23_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x032F))
#define MT6336_PMIC_AUXADC_ADC24                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0330))
#define MT6336_PMIC_AUXADC_ADC24_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0331))
#define MT6336_PMIC_AUXADC_ADC25                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0332))
#define MT6336_PMIC_AUXADC_ADC25_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0333))
#define MT6336_PMIC_AUXADC_ADC26                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0334))
#define MT6336_PMIC_AUXADC_ADC26_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0335))
#define MT6336_PMIC_AUXADC_ADC27                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0336))
#define MT6336_PMIC_AUXADC_ADC27_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0337))
#define MT6336_PMIC_AUXADC_ADC28                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0338))
#define MT6336_PMIC_AUXADC_ADC28_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0339))
#define MT6336_PMIC_AUXADC_STA0                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x033A))
#define MT6336_PMIC_AUXADC_STA0_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x033B))
#define MT6336_PMIC_AUXADC_STA1                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x033C))
#define MT6336_PMIC_AUXADC_STA1_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x033D))
#define MT6336_PMIC_AUXADC_STA2_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x033E))
#define MT6336_PMIC_AUXADC_RQST0                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x033F))
#define MT6336_PMIC_AUXADC_RQST0_SET                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0340))
#define MT6336_PMIC_AUXADC_RQST0_CLR                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0341))
#define MT6336_PMIC_AUXADC_RQST0_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0342))
#define MT6336_PMIC_AUXADC_RQST0_H_SET                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x0343))
#define MT6336_PMIC_AUXADC_RQST0_H_CLR                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x0344))
#define MT6336_PMIC_AUXADC_CON0                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0345))
#define MT6336_PMIC_AUXADC_CON0_SET                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0346))
#define MT6336_PMIC_AUXADC_CON0_CLR                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0347))
#define MT6336_PMIC_AUXADC_CON0_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0348))
#define MT6336_PMIC_AUXADC_CON0_H_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0349))
#define MT6336_PMIC_AUXADC_CON0_H_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x034A))
#define MT6336_PMIC_AUXADC_CON1                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x034B))
#define MT6336_PMIC_AUXADC_CON1_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x034C))
#define MT6336_PMIC_AUXADC_CON2                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x034D))
#define MT6336_PMIC_AUXADC_CON2_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x034E))
#define MT6336_PMIC_AUXADC_CON3                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x034F))
#define MT6336_PMIC_AUXADC_CON3_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0350))
#define MT6336_PMIC_AUXADC_CON4                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0351))
#define MT6336_PMIC_AUXADC_CON4_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0352))
#define MT6336_PMIC_AUXADC_CON5                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0353))
#define MT6336_PMIC_AUXADC_CON5_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0354))
#define MT6336_PMIC_AUXADC_CON6                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0355))
#define MT6336_PMIC_AUXADC_CON6_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0356))
#define MT6336_PMIC_AUXADC_CON7                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0357))
#define MT6336_PMIC_AUXADC_CON7_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0358))
#define MT6336_PMIC_AUXADC_CON8                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0359))
#define MT6336_PMIC_AUXADC_CON8_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x035A))
#define MT6336_PMIC_AUXADC_CON9                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x035B))
#define MT6336_PMIC_AUXADC_CON9_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x035C))
#define MT6336_PMIC_AUXADC_CON10                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x035D))
#define MT6336_PMIC_AUXADC_CON10_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x035E))
#define MT6336_PMIC_AUXADC_CON11                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x035F))
#define MT6336_PMIC_AUXADC_CON11_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0360))
#define MT6336_PMIC_AUXADC_CON12                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0361))
#define MT6336_PMIC_AUXADC_CON12_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0362))
#define MT6336_PMIC_AUXADC_CON13                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0363))
#define MT6336_PMIC_AUXADC_CON13_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0364))
#define MT6336_PMIC_AUXADC_CON14                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0365))
#define MT6336_PMIC_AUXADC_CON14_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0366))
#define MT6336_PMIC_AUXADC_CON15_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0368))
#define MT6336_PMIC_AUXADC_CON16                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0369))
#define MT6336_PMIC_AUXADC_CON16_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x036A))
#define MT6336_PMIC_AUXADC_CON17                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x036B))
#define MT6336_PMIC_AUXADC_CON17_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x036C))
#define MT6336_PMIC_AUXADC_ANA_0                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x036D))
#define MT6336_PMIC_AUXADC_AUTORPT0                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x036E))
#define MT6336_PMIC_AUXADC_AUTORPT0_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x036F))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP0               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0370))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP0_H             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0371))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP1               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0372))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP1_H             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0373))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP2               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0374))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP3               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0375))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP3_H             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0376))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP4               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0377))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP4_H             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0378))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP5               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0379))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP5_H             ((unsigned int)(MT6336_PMIC_REG_BASE+0x037A))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP6               ((unsigned int)(MT6336_PMIC_REG_BASE+0x037B))
#define MT6336_PMIC_AUXADC_VBUS_SOFT_OVP6_H             ((unsigned int)(MT6336_PMIC_REG_BASE+0x037C))
#define MT6336_PMIC_AUXADC_TYPEC_H0                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x037D))
#define MT6336_PMIC_AUXADC_TYPEC_H0_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x037E))
#define MT6336_PMIC_AUXADC_TYPEC_H1                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x037F))
#define MT6336_PMIC_AUXADC_TYPEC_H1_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0380))
#define MT6336_PMIC_AUXADC_TYPEC_H2                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0381))
#define MT6336_PMIC_AUXADC_TYPEC_H3                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0382))
#define MT6336_PMIC_AUXADC_TYPEC_H3_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0383))
#define MT6336_PMIC_AUXADC_TYPEC_H4                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0384))
#define MT6336_PMIC_AUXADC_TYPEC_H4_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0385))
#define MT6336_PMIC_AUXADC_TYPEC_H5                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0386))
#define MT6336_PMIC_AUXADC_TYPEC_H5_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0387))
#define MT6336_PMIC_AUXADC_TYPEC_H6                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0388))
#define MT6336_PMIC_AUXADC_TYPEC_H6_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0389))
#define MT6336_PMIC_AUXADC_TYPEC_L0                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x038A))
#define MT6336_PMIC_AUXADC_TYPEC_L0_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x038B))
#define MT6336_PMIC_AUXADC_TYPEC_L1                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x038C))
#define MT6336_PMIC_AUXADC_TYPEC_L1_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x038D))
#define MT6336_PMIC_AUXADC_TYPEC_L2                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x038E))
#define MT6336_PMIC_AUXADC_TYPEC_L3                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x038F))
#define MT6336_PMIC_AUXADC_TYPEC_L3_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0390))
#define MT6336_PMIC_AUXADC_TYPEC_L4                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0391))
#define MT6336_PMIC_AUXADC_TYPEC_L4_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0392))
#define MT6336_PMIC_AUXADC_TYPEC_L5                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0393))
#define MT6336_PMIC_AUXADC_TYPEC_L5_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0394))
#define MT6336_PMIC_AUXADC_TYPEC_L6                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0395))
#define MT6336_PMIC_AUXADC_TYPEC_L6_H                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0396))
#define MT6336_PMIC_AUXADC_THR0                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0397))
#define MT6336_PMIC_AUXADC_THR0_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0398))
#define MT6336_PMIC_AUXADC_THR1                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0399))
#define MT6336_PMIC_AUXADC_THR1_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x039A))
#define MT6336_PMIC_AUXADC_THR2                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x039B))
#define MT6336_PMIC_AUXADC_THR3                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x039C))
#define MT6336_PMIC_AUXADC_THR3_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x039D))
#define MT6336_PMIC_AUXADC_THR4                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x039E))
#define MT6336_PMIC_AUXADC_THR4_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x039F))
#define MT6336_PMIC_AUXADC_THR5                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x03A0))
#define MT6336_PMIC_AUXADC_THR5_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x03A1))
#define MT6336_PMIC_AUXADC_THR6                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x03A2))
#define MT6336_PMIC_AUXADC_THR6_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x03A3))
#define MT6336_PMIC_AUXADC_EFUSE0                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x03A4))
#define MT6336_PMIC_AUXADC_EFUSE0_H                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x03A5))
#define MT6336_PMIC_AUXADC_EFUSE1                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x03A6))
#define MT6336_PMIC_AUXADC_EFUSE1_H                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x03A7))
#define MT6336_PMIC_AUXADC_EFUSE2                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x03A8))
#define MT6336_PMIC_AUXADC_EFUSE2_H                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x03A9))
#define MT6336_PMIC_AUXADC_EFUSE3                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x03AA))
#define MT6336_PMIC_AUXADC_EFUSE3_H                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x03AB))
#define MT6336_PMIC_AUXADC_EFUSE4                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x03AC))
#define MT6336_PMIC_AUXADC_EFUSE4_H                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x03AD))
#define MT6336_PMIC_AUXADC_EFUSE5                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x03AE))
#define MT6336_PMIC_AUXADC_EFUSE5_H                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x03AF))
#define MT6336_PMIC_AUXADC_DBG0                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x03B0))
#define MT6336_PMIC_AUXADC_DBG0_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x03B1))
#define MT6336_PMIC_AUXADC_IMP0                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x03B2))
#define MT6336_PMIC_AUXADC_IMP0_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x03B3))
#define MT6336_PMIC_AUXADC_IMP1                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x03B4))
#define MT6336_PMIC_AUXADC_IMP1_H                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x03B5))
#define MT6336_PMIC_AUXADC_BAT_TEMP_0                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x03B6))
#define MT6336_PMIC_AUXADC_BAT_TEMP_1                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x03B7))
#define MT6336_PMIC_AUXADC_BAT_TEMP_1_H                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x03B8))
#define MT6336_PMIC_AUXADC_BAT_TEMP_2                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x03B9))
#define MT6336_PMIC_AUXADC_BAT_TEMP_2_H                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x03BA))
#define MT6336_PMIC_AUXADC_BAT_TEMP_3                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x03BB))
#define MT6336_PMIC_AUXADC_BAT_TEMP_4                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x03BD))
#define MT6336_PMIC_AUXADC_BAT_TEMP_4_H                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x03BE))
#define MT6336_PMIC_AUXADC_BAT_TEMP_5                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x03BF))
#define MT6336_PMIC_AUXADC_BAT_TEMP_5_H                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x03C0))
#define MT6336_PMIC_AUXADC_BAT_TEMP_6                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x03C1))
#define MT6336_PMIC_AUXADC_BAT_TEMP_6_H                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x03C2))
#define MT6336_PMIC_AUXADC_BAT_TEMP_7                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x03C3))
#define MT6336_PMIC_AUXADC_BAT_TEMP_7_H                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x03C4))
#define MT6336_PMIC_MAIN_CON0                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0400))
#define MT6336_PMIC_MAIN_CON1                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0401))
#define MT6336_PMIC_MAIN_CON2                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0402))
#define MT6336_PMIC_MAIN_CON3                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0403))
#define MT6336_PMIC_MAIN_CON4                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0404))
#define MT6336_PMIC_MAIN_CON5                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0405))
#define MT6336_PMIC_MAIN_RSV0                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0406))
#define MT6336_PMIC_MAIN_CON6                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0407))
#define MT6336_PMIC_MAIN_CON7                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0408))
#define MT6336_PMIC_MAIN_CON8                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0409))
#define MT6336_PMIC_MAIN_CON9                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x040A))
#define MT6336_PMIC_MAIN_CON10                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x040B))
#define MT6336_PMIC_MAIN_CON11                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x040C))
#define MT6336_PMIC_MAIN_CON12                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x040D))
#define MT6336_PMIC_MAIN_CON13                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x040E))
#define MT6336_PMIC_OTG_CTRL0                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x040F))
#define MT6336_PMIC_OTG_CTRL1                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0410))
#define MT6336_PMIC_OTG_CTRL2                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0411))
#define MT6336_PMIC_FLASH_CTRL0                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0412))
#define MT6336_PMIC_FLASH_CTRL1                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0413))
#define MT6336_PMIC_FLASH_CTRL2                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0414))
#define MT6336_PMIC_FLASH_CTRL3                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0415))
#define MT6336_PMIC_FLASH_CTRL4                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0416))
#define MT6336_PMIC_FLASH_CTRL5                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0417))
#define MT6336_PMIC_TORCH_CTRL0                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0418))
#define MT6336_PMIC_TORCH_CTRL1                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0419))
#define MT6336_PMIC_TORCH_CTRL2                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x041A))
#define MT6336_PMIC_TORCH_CTRL3                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x041B))
#define MT6336_PMIC_BOOST_CTRL0                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x041C))
#define MT6336_PMIC_BOOST_CTRL1                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x041D))
#define MT6336_PMIC_BOOST_RSV0                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x041E))
#define MT6336_PMIC_BOOST_RSV1                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x041F))
#define MT6336_PMIC_PE_CON0                             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0420))
#define MT6336_PMIC_PE_CON1                             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0421))
#define MT6336_PMIC_JEITA_CON0                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0422))
#define MT6336_PMIC_JEITA_CON1                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0423))
#define MT6336_PMIC_JEITA_CON2                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0424))
#define MT6336_PMIC_JEITA_CON3                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0425))
#define MT6336_PMIC_ICC_CON0                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0426))
#define MT6336_PMIC_VCV_CON0                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0427))
#define MT6336_PMIC_JEITA_CON4                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0428))
#define MT6336_PMIC_JEITA_CON5                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0429))
#define MT6336_PMIC_JEITA_CON6                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x042A))
#define MT6336_PMIC_BC12_CON0                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x042B))
#define MT6336_PMIC_GER_CON0                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x042C))
#define MT6336_PMIC_GER_CON1                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x042D))
#define MT6336_PMIC_GER_CON2                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x042E))
#define MT6336_PMIC_GER_CON3                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x042F))
#define MT6336_PMIC_GER_CON4                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0430))
#define MT6336_PMIC_GER_CON5                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0431))
#define MT6336_PMIC_LONG_PRESS_CON0                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0432))
#define MT6336_PMIC_SHIP_MODE_CON0                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0433))
#define MT6336_PMIC_WDT_CON0                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0434))
#define MT6336_PMIC_WDT_CON1                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0435))
#define MT6336_PMIC_DB_WRAPPER_CON0                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0436))
#define MT6336_PMIC_ICL_CON0                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0437))
#define MT6336_PMIC_ICL_CON1                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0438))
#define MT6336_PMIC_BACK_BOOST_CON0                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0439))
#define MT6336_PMIC_SFSTR_CLK_CON0                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x043A))
#define MT6336_PMIC_SAFE_TIMER_CON0                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x043B))
#define MT6336_PMIC_SFSTR_CLK_CON1                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x043C))
#define MT6336_PMIC_OTP_CON0                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x043D))
#define MT6336_PMIC_OTP_CON1                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x043E))
#define MT6336_PMIC_OTP_CON2                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x043F))
#define MT6336_PMIC_OTP_CON3                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0440))
#define MT6336_PMIC_OTP_CON4                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0441))
#define MT6336_PMIC_OTP_CON5                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0442))
#define MT6336_PMIC_OTP_CON6                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0443))
#define MT6336_PMIC_OTP_CON7                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0444))
#define MT6336_PMIC_OTP_CON8                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0445))
#define MT6336_PMIC_OTP_CON9                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0446))
#define MT6336_PMIC_OTP_CON10                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0447))
#define MT6336_PMIC_OTP_CON11                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0448))
#define MT6336_PMIC_OTP_CON12                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0449))
#define MT6336_PMIC_OTP_CON13                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x044A))
#define MT6336_PMIC_OTP_CON14                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x044B))
#define MT6336_PMIC_OTP_DOUT_0_7                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x044C))
#define MT6336_PMIC_OTP_DOUT_8_15                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x044D))
#define MT6336_PMIC_OTP_DOUT_16_23                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x044E))
#define MT6336_PMIC_OTP_DOUT_24_31                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x044F))
#define MT6336_PMIC_OTP_DOUT_32_39                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0450))
#define MT6336_PMIC_OTP_DOUT_40_47                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0451))
#define MT6336_PMIC_OTP_DOUT_48_55                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0452))
#define MT6336_PMIC_OTP_DOUT_56_63                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0453))
#define MT6336_PMIC_OTP_DOUT_64_71                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0454))
#define MT6336_PMIC_OTP_DOUT_72_79                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0455))
#define MT6336_PMIC_OTP_DOUT_80_87                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0456))
#define MT6336_PMIC_OTP_DOUT_88_95                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0457))
#define MT6336_PMIC_OTP_DOUT_96_103                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0458))
#define MT6336_PMIC_OTP_DOUT_104_111                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0459))
#define MT6336_PMIC_OTP_DOUT_112_119                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x045A))
#define MT6336_PMIC_OTP_DOUT_120_127                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x045B))
#define MT6336_PMIC_OTP_DOUT_128_135                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x045C))
#define MT6336_PMIC_OTP_DOUT_136_143                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x045D))
#define MT6336_PMIC_OTP_DOUT_144_151                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x045E))
#define MT6336_PMIC_OTP_DOUT_152_159                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x045F))
#define MT6336_PMIC_OTP_DOUT_160_167                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0460))
#define MT6336_PMIC_OTP_DOUT_168_175                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0461))
#define MT6336_PMIC_OTP_DOUT_176_183                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0462))
#define MT6336_PMIC_OTP_DOUT_184_191                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0463))
#define MT6336_PMIC_OTP_DOUT_192_199                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0464))
#define MT6336_PMIC_OTP_DOUT_200_207                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0465))
#define MT6336_PMIC_OTP_DOUT_208_215                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0466))
#define MT6336_PMIC_OTP_DOUT_216_223                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0467))
#define MT6336_PMIC_OTP_DOUT_224_231                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0468))
#define MT6336_PMIC_OTP_DOUT_232_239                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0469))
#define MT6336_PMIC_OTP_DOUT_240_247                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x046A))
#define MT6336_PMIC_OTP_DOUT_248_255                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x046B))
#define MT6336_PMIC_OTP_DOUT_256_263                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x046C))
#define MT6336_PMIC_OTP_DOUT_264_271                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x046D))
#define MT6336_PMIC_OTP_DOUT_272_279                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x046E))
#define MT6336_PMIC_OTP_DOUT_280_287                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x046F))
#define MT6336_PMIC_OTP_DOUT_288_295                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0470))
#define MT6336_PMIC_OTP_DOUT_296_303                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0471))
#define MT6336_PMIC_OTP_DOUT_304_311                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0472))
#define MT6336_PMIC_OTP_DOUT_312_319                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0473))
#define MT6336_PMIC_OTP_DOUT_320_327                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0474))
#define MT6336_PMIC_OTP_DOUT_328_335                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0475))
#define MT6336_PMIC_OTP_DOUT_336_343                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0476))
#define MT6336_PMIC_OTP_DOUT_344_351                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0477))
#define MT6336_PMIC_OTP_DOUT_352_359                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0478))
#define MT6336_PMIC_OTP_DOUT_360_367                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0479))
#define MT6336_PMIC_OTP_DOUT_368_375                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x047A))
#define MT6336_PMIC_OTP_DOUT_376_383                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x047B))
#define MT6336_PMIC_OTP_DOUT_384_391                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x047C))
#define MT6336_PMIC_OTP_DOUT_392_399                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x047D))
#define MT6336_PMIC_OTP_DOUT_400_407                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x047E))
#define MT6336_PMIC_OTP_DOUT_408_415                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x047F))
#define MT6336_PMIC_OTP_DOUT_416_423                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0480))
#define MT6336_PMIC_OTP_DOUT_424_431                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0481))
#define MT6336_PMIC_OTP_DOUT_432_439                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0482))
#define MT6336_PMIC_OTP_DOUT_440_447                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0483))
#define MT6336_PMIC_OTP_DOUT_448_455                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0484))
#define MT6336_PMIC_OTP_DOUT_456_463                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0485))
#define MT6336_PMIC_OTP_DOUT_464_471                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0486))
#define MT6336_PMIC_OTP_DOUT_472_479                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0487))
#define MT6336_PMIC_OTP_DOUT_480_487                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0488))
#define MT6336_PMIC_OTP_DOUT_488_495                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0489))
#define MT6336_PMIC_OTP_DOUT_496_503                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x048A))
#define MT6336_PMIC_OTP_DOUT_504_511                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x048B))
#define MT6336_PMIC_OTP_VAL_0_7                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x048C))
#define MT6336_PMIC_OTP_VAL_8_15                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x048D))
#define MT6336_PMIC_OTP_VAL_16_23                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x048E))
#define MT6336_PMIC_OTP_VAL_24_31                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x048F))
#define MT6336_PMIC_OTP_VAL_32_39                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0490))
#define MT6336_PMIC_OTP_VAL_40_47                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0491))
#define MT6336_PMIC_OTP_VAL_48_55                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0492))
#define MT6336_PMIC_OTP_VAL_56_63                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0493))
#define MT6336_PMIC_OTP_VAL_64_71                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0494))
#define MT6336_PMIC_OTP_VAL_72_79                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0495))
#define MT6336_PMIC_OTP_VAL_80_87                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0496))
#define MT6336_PMIC_OTP_VAL_88_95                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0497))
#define MT6336_PMIC_OTP_VAL_96_103                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0498))
#define MT6336_PMIC_OTP_VAL_104_111                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0499))
#define MT6336_PMIC_OTP_VAL_112_119                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x049A))
#define MT6336_PMIC_OTP_VAL_120_127                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x049B))
#define MT6336_PMIC_OTP_VAL_128_135                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x049C))
#define MT6336_PMIC_OTP_VAL_136_143                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x049D))
#define MT6336_PMIC_OTP_VAL_144_151                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x049E))
#define MT6336_PMIC_OTP_VAL_152_159                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x049F))
#define MT6336_PMIC_OTP_VAL_160_167                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04A0))
#define MT6336_PMIC_OTP_VAL_168_175                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04A1))
#define MT6336_PMIC_OTP_VAL_176_183                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04A2))
#define MT6336_PMIC_OTP_VAL_184_191                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04A3))
#define MT6336_PMIC_OTP_VAL_192_199                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04A4))
#define MT6336_PMIC_OTP_VAL_200_207                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04A5))
#define MT6336_PMIC_OTP_VAL_208_215                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04A6))
#define MT6336_PMIC_OTP_VAL_216_223                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04A7))
#define MT6336_PMIC_OTP_VAL_224_231                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04A8))
#define MT6336_PMIC_OTP_VAL_232_239                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04A9))
#define MT6336_PMIC_OTP_VAL_240_247                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04AA))
#define MT6336_PMIC_OTP_VAL_248_255                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04AB))
#define MT6336_PMIC_OTP_VAL_256_263                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04AC))
#define MT6336_PMIC_OTP_VAL_264_271                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04AD))
#define MT6336_PMIC_OTP_VAL_272_279                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04AE))
#define MT6336_PMIC_OTP_VAL_280_287                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04AF))
#define MT6336_PMIC_OTP_VAL_288_295                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04B0))
#define MT6336_PMIC_OTP_VAL_296_303                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04B1))
#define MT6336_PMIC_OTP_VAL_304_311                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04B2))
#define MT6336_PMIC_OTP_VAL_312_319                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04B3))
#define MT6336_PMIC_OTP_VAL_320_327                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04B4))
#define MT6336_PMIC_OTP_VAL_328_335                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04B5))
#define MT6336_PMIC_OTP_VAL_336_343                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04B6))
#define MT6336_PMIC_OTP_VAL_344_351                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04B7))
#define MT6336_PMIC_OTP_VAL_352_359                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04B8))
#define MT6336_PMIC_OTP_VAL_360_367                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04B9))
#define MT6336_PMIC_OTP_VAL_368_375                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04BA))
#define MT6336_PMIC_OTP_VAL_376_383                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04BB))
#define MT6336_PMIC_OTP_VAL_384_391                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04BC))
#define MT6336_PMIC_OTP_VAL_392_399                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04BD))
#define MT6336_PMIC_OTP_VAL_400_407                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04BE))
#define MT6336_PMIC_OTP_VAL_408_415                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04BF))
#define MT6336_PMIC_OTP_VAL_416_423                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04C0))
#define MT6336_PMIC_OTP_VAL_424_431                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04C1))
#define MT6336_PMIC_OTP_VAL_432_439                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04C2))
#define MT6336_PMIC_OTP_VAL_440_447                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04C3))
#define MT6336_PMIC_OTP_VAL_448_455                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04C4))
#define MT6336_PMIC_OTP_VAL_456_463                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04C5))
#define MT6336_PMIC_OTP_VAL_464_471                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04C6))
#define MT6336_PMIC_OTP_VAL_472_479                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04C7))
#define MT6336_PMIC_OTP_VAL_480_487                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04C8))
#define MT6336_PMIC_OTP_VAL_488_495                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04C9))
#define MT6336_PMIC_OTP_VAL_496_503                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04CA))
#define MT6336_PMIC_OTP_VAL_504_511                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x04CB))
#define MT6336_PMIC_CORE_ANA_CON0                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0500))
#define MT6336_PMIC_CORE_ANA_CON1                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0501))
#define MT6336_PMIC_CORE_ANA_CON2                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0502))
#define MT6336_PMIC_CORE_ANA_CON3                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0503))
#define MT6336_PMIC_CORE_ANA_CON4                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0504))
#define MT6336_PMIC_CORE_ANA_CON5                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0505))
#define MT6336_PMIC_CORE_ANA_CON6                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0506))
#define MT6336_PMIC_CORE_ANA_CON7                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0507))
#define MT6336_PMIC_CORE_ANA_CON9                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0508))
#define MT6336_PMIC_CORE_ANA_CON10                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0509))
#define MT6336_PMIC_CORE_ANA_CON11                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x050A))
#define MT6336_PMIC_CORE_ANA_CON12                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x050B))
#define MT6336_PMIC_CORE_ANA_CON13                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x050C))
#define MT6336_PMIC_CORE_ANA_CON14                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x050D))
#define MT6336_PMIC_CORE_ANA_CON15                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x050E))
#define MT6336_PMIC_CORE_ANA_CON16                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x050F))
#define MT6336_PMIC_CORE_ANA_CON17                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0510))
#define MT6336_PMIC_CORE_ANA_CON18                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0511))
#define MT6336_PMIC_CORE_ANA_CON19                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0512))
#define MT6336_PMIC_CORE_ANA_CON20                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0513))
#define MT6336_PMIC_CORE_ANA_CON21                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0514))
#define MT6336_PMIC_CORE_ANA_CON22                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0515))
#define MT6336_PMIC_CORE_ANA_CON23                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0516))
#define MT6336_PMIC_CORE_ANA_CON24                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0517))
#define MT6336_PMIC_CORE_ANA_CON25                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0518))
#define MT6336_PMIC_CORE_ANA_CON26                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0519))
#define MT6336_PMIC_CORE_ANA_CON27                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x051A))
#define MT6336_PMIC_CORE_ANA_CON28                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x051B))
#define MT6336_PMIC_CORE_ANA_CON29                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x051C))
#define MT6336_PMIC_CORE_ANA_CON30                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x051D))
#define MT6336_PMIC_CORE_ANA_CON31                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x051E))
#define MT6336_PMIC_CORE_ANA_CON32                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x051F))
#define MT6336_PMIC_CORE_ANA_CON33                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0520))
#define MT6336_PMIC_CORE_ANA_CON34                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0521))
#define MT6336_PMIC_CORE_ANA_CON35                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0522))
#define MT6336_PMIC_CORE_ANA_CON36                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0523))
#define MT6336_PMIC_CORE_ANA_CON37                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0524))
#define MT6336_PMIC_CORE_ANA_CON38                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0525))
#define MT6336_PMIC_CORE_ANA_CON39                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0526))
#define MT6336_PMIC_CORE_ANA_CON40                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0527))
#define MT6336_PMIC_CORE_ANA_CON41                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0528))
#define MT6336_PMIC_CORE_ANA_CON42                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0529))
#define MT6336_PMIC_CORE_ANA_CON43                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x052A))
#define MT6336_PMIC_CORE_ANA_CON44                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x052B))
#define MT6336_PMIC_CORE_ANA_CON45                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x052C))
#define MT6336_PMIC_CORE_ANA_CON46                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x052D))
#define MT6336_PMIC_CORE_ANA_CON47                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x052E))
#define MT6336_PMIC_CORE_ANA_CON48                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x052F))
#define MT6336_PMIC_CORE_ANA_CON49                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0530))
#define MT6336_PMIC_CORE_ANA_CON50                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0531))
#define MT6336_PMIC_CORE_ANA_CON51                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0532))
#define MT6336_PMIC_CORE_ANA_CON52                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0533))
#define MT6336_PMIC_CORE_ANA_CON53                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0534))
#define MT6336_PMIC_CORE_ANA_CON54                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0535))
#define MT6336_PMIC_CORE_ANA_CON55                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0536))
#define MT6336_PMIC_CORE_ANA_CON56                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0537))
#define MT6336_PMIC_CORE_ANA_CON57                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0538))
#define MT6336_PMIC_CORE_ANA_CON58                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0539))
#define MT6336_PMIC_CORE_ANA_CON59                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x053A))
#define MT6336_PMIC_CORE_ANA_CON60                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x053B))
#define MT6336_PMIC_CORE_ANA_CON61                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x053C))
#define MT6336_PMIC_CORE_ANA_CON63                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x053D))
#define MT6336_PMIC_CORE_ANA_CON64                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x053E))
#define MT6336_PMIC_CORE_ANA_CON65                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x053F))
#define MT6336_PMIC_CORE_ANA_CON66                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0540))
#define MT6336_PMIC_CORE_ANA_CON67                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0541))
#define MT6336_PMIC_CORE_ANA_CON68                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0542))
#define MT6336_PMIC_CORE_ANA_CON69                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0543))
#define MT6336_PMIC_CORE_ANA_CON70                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0544))
#define MT6336_PMIC_CORE_ANA_CON71                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0545))
#define MT6336_PMIC_CORE_ANA_CON72                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0546))
#define MT6336_PMIC_CORE_ANA_CON73                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0547))
#define MT6336_PMIC_CORE_ANA_CON74                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0548))
#define MT6336_PMIC_CORE_ANA_CON75                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0549))
#define MT6336_PMIC_CORE_ANA_CON76                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x054A))
#define MT6336_PMIC_CORE_ANA_CON79                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x054B))
#define MT6336_PMIC_CORE_ANA_CON80                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x054C))
#define MT6336_PMIC_CORE_ANA_CON81                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x054D))
#define MT6336_PMIC_CORE_ANA_CON82                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x054E))
#define MT6336_PMIC_CORE_ANA_CON83                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x054F))
#define MT6336_PMIC_CORE_ANA_CON84                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0550))
#define MT6336_PMIC_CORE_ANA_CON85                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0551))
#define MT6336_PMIC_CORE_ANA_CON86                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0552))
#define MT6336_PMIC_CORE_ANA_CON87                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0553))
#define MT6336_PMIC_CORE_ANA_CON88                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0554))
#define MT6336_PMIC_CORE_ANA_CON89                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0555))
#define MT6336_PMIC_CORE_ANA_CON90                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0556))
#define MT6336_PMIC_CORE_ANA_CON91                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0557))
#define MT6336_PMIC_CORE_ANA_CON92                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0558))
#define MT6336_PMIC_CORE_ANA_CON93                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0559))
#define MT6336_PMIC_CORE_ANA_CON95                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x055A))
#define MT6336_PMIC_CORE_ANA_CON96                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x055B))
#define MT6336_PMIC_CORE_ANA_CON99                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x055C))
#define MT6336_PMIC_CORE_ANA_CON100                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x055D))
#define MT6336_PMIC_CORE_ANA_CON101                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x055E))
#define MT6336_PMIC_CORE_ANA_CON102                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x055F))
#define MT6336_PMIC_CORE_ANA_CON103                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0560))
#define MT6336_PMIC_CORE_ANA_CON104                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0561))
#define MT6336_PMIC_CORE_ANA_CON105                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0562))
#define MT6336_PMIC_CORE_ANA_CON106                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0563))
#define MT6336_PMIC_CORE_ANA_CON107                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0564))
#define MT6336_PMIC_CORE_ANA_CON108                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0565))
#define MT6336_PMIC_CORE_ANA_CON109                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0566))
#define MT6336_PMIC_CORE_ANA_CON110                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0567))
#define MT6336_PMIC_CORE_ANA_CON111                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0568))
#define MT6336_PMIC_CORE_ANA_CON112                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0569))
#define MT6336_PMIC_AUXADC_LBAT2_1                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x056A))
#define MT6336_PMIC_AUXADC_LBAT2_1_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x056B))
#define MT6336_PMIC_AUXADC_LBAT2_2                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x056C))
#define MT6336_PMIC_AUXADC_LBAT2_2_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x056D))
#define MT6336_PMIC_AUXADC_LBAT2_3                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x056E))
#define MT6336_PMIC_AUXADC_LBAT2_4                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0570))
#define MT6336_PMIC_AUXADC_LBAT2_4_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0571))
#define MT6336_PMIC_AUXADC_LBAT2_5                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0572))
#define MT6336_PMIC_AUXADC_LBAT2_5_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0573))
#define MT6336_PMIC_AUXADC_LBAT2_6                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0574))
#define MT6336_PMIC_AUXADC_LBAT2_6_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0575))
#define MT6336_PMIC_AUXADC_LBAT2_7                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0576))
#define MT6336_PMIC_AUXADC_LBAT2_7_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0577))
#define MT6336_PMIC_AUXADC_JEITA_0                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0578))
#define MT6336_PMIC_AUXADC_JEITA_0_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0579))
#define MT6336_PMIC_AUXADC_JEITA_1                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x057A))
#define MT6336_PMIC_AUXADC_JEITA_1_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x057B))
#define MT6336_PMIC_AUXADC_JEITA_2                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x057C))
#define MT6336_PMIC_AUXADC_JEITA_2_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x057D))
#define MT6336_PMIC_AUXADC_JEITA_3                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x057E))
#define MT6336_PMIC_AUXADC_JEITA_3_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x057F))
#define MT6336_PMIC_AUXADC_JEITA_4                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0580))
#define MT6336_PMIC_AUXADC_JEITA_4_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0581))
#define MT6336_PMIC_AUXADC_JEITA_5                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0582))
#define MT6336_PMIC_AUXADC_JEITA_5_H                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0583))
#define MT6336_PMIC_AUXADC_NAG_0                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0584))
#define MT6336_PMIC_AUXADC_NAG_0_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0585))
#define MT6336_PMIC_AUXADC_NAG_1                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0586))
#define MT6336_PMIC_AUXADC_NAG_1_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0587))
#define MT6336_PMIC_AUXADC_NAG_2                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0588))
#define MT6336_PMIC_AUXADC_NAG_2_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0589))
#define MT6336_PMIC_AUXADC_NAG_3                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x058A))
#define MT6336_PMIC_AUXADC_NAG_3_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x058B))
#define MT6336_PMIC_AUXADC_NAG_4                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x058C))
#define MT6336_PMIC_AUXADC_NAG_4_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x058D))
#define MT6336_PMIC_AUXADC_NAG_5                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x058E))
#define MT6336_PMIC_AUXADC_NAG_5_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x058F))
#define MT6336_PMIC_AUXADC_NAG_6                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0590))
#define MT6336_PMIC_AUXADC_NAG_6_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0591))
#define MT6336_PMIC_AUXADC_NAG_7                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0592))
#define MT6336_PMIC_AUXADC_NAG_7_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0593))
#define MT6336_PMIC_AUXADC_NAG_8                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0594))
#define MT6336_PMIC_AUXADC_NAG_8_H                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0595))
#define MT6336_PMIC_AUXADC_VBAT_0_L                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0596))
#define MT6336_PMIC_AUXADC_VBAT_0_H                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0597))
#define MT6336_PMIC_AUXADC_VBAT_1_L                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0598))
#define MT6336_PMIC_AUXADC_VBAT_1_H                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x0599))
#define MT6336_PMIC_AUXADC_VBAT_2_L                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x059A))
#define MT6336_PMIC_AUXADC_VBAT_2_H                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x059B))
#define MT6336_PMIC_AUXADC_VBAT_3_L                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x059C))
#define MT6336_PMIC_AUXADC_VBAT_3_H                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x059D))
#define MT6336_PMIC_AUXADC_VBAT_4_L                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x059E))
#define MT6336_PMIC_AUXADC_VBAT_4_H                     ((unsigned int)(MT6336_PMIC_REG_BASE+0x059F))
#define MT6336_PMIC_ANA_CORE_AD_INTF0                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05A0))
#define MT6336_PMIC_ANA_CORE_AD_INTF1                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05A1))
#define MT6336_PMIC_ANA_CORE_AD_INTF2                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05A2))
#define MT6336_PMIC_ANA_CORE_AD_INTF3                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05A3))
#define MT6336_PMIC_ANA_CORE_AD_INTF4                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05A4))
#define MT6336_PMIC_ANA_CORE_AD_INTF5                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05A5))
#define MT6336_PMIC_ANA_CORE_AD_INTF6                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05A6))
#define MT6336_PMIC_ANA_CORE_AD_INTF7                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05A7))
#define MT6336_PMIC_ANA_CORE_DA_INTF0                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05A8))
#define MT6336_PMIC_ANA_CORE_DA_INTF1                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05A9))
#define MT6336_PMIC_ANA_CORE_DA_INTF2                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05AA))
#define MT6336_PMIC_ANA_CORE_DA_INTF3                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05AB))
#define MT6336_PMIC_ANA_CORE_DA_INTF4                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05AC))
#define MT6336_PMIC_ANA_CORE_DA_INTF5                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05AD))
#define MT6336_PMIC_ANA_CORE_DA_INTF6                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05AE))
#define MT6336_PMIC_ANA_CORE_DA_INTF7                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05AF))
#define MT6336_PMIC_ANA_CORE_DA_INTF8                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05B0))
#define MT6336_PMIC_ANA_CORE_DA_INTF9                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x05B1))
#define MT6336_PMIC_ANA_CORE_DA_INTF10                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05B2))
#define MT6336_PMIC_ANA_CORE_DA_INTF11                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05B3))
#define MT6336_PMIC_ANA_CORE_DA_INTF12                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05B4))
#define MT6336_PMIC_ANA_CORE_DA_INTF13                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05B5))
#define MT6336_PMIC_ANA_CORE_DA_INTF14                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05B6))
#define MT6336_PMIC_ANA_CORE_DA_INTF15                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05B7))
#define MT6336_PMIC_ANA_CORE_DA_INTF16                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05B8))
#define MT6336_PMIC_ANA_CORE_DA_INTF17                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05B9))
#define MT6336_PMIC_ANA_CORE_DA_INTF18                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05BA))
#define MT6336_PMIC_ANA_CORE_DA_INTF19                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05BB))
#define MT6336_PMIC_ANA_CORE_DA_INTF20                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05BC))
#define MT6336_PMIC_ANA_CORE_DA_INTF21                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05BD))
#define MT6336_PMIC_ANA_CORE_DA_INTF22                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05BE))
#define MT6336_PMIC_ANA_CORE_DA_INTF23                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05BF))
#define MT6336_PMIC_ANA_CORE_DA_INTF24                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05C0))
#define MT6336_PMIC_ANA_CORE_DA_INTF25                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05C1))
#define MT6336_PMIC_ANA_AUXADC_AD_INTF0                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x05C2))
#define MT6336_PMIC_ANA_AUXADC_DA_INTF0                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x05C3))
#define MT6336_PMIC_ANA_AUXADC_DA_INTF1                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x05C4))
#define MT6336_PMIC_ANA_AUXADC_DA_INTF2                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x05C5))
#define MT6336_PMIC_ANA_AUXADC_DA_INTF3                 ((unsigned int)(MT6336_PMIC_REG_BASE+0x05C6))
#define MT6336_PMIC_ANA_TYPEC_AD_INTF0                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05C7))
#define MT6336_PMIC_ANA_TYPEC_DA_INTF0                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05C8))
#define MT6336_PMIC_ANA_TYPEC_DA_INTF1                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05C9))
#define MT6336_PMIC_ANA_TYPEC_DA_INTF2                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05CA))
#define MT6336_PMIC_ANA_TYPEC_DA_INTF3                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05CB))
#define MT6336_PMIC_ANA_TYPEC_DA_INTF4                  ((unsigned int)(MT6336_PMIC_REG_BASE+0x05CC))
#define MT6336_PMIC_ANA_CORE_AD_RGS0                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0600))
#define MT6336_PMIC_ANA_CORE_AD_RGS1                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0601))
#define MT6336_PMIC_ANA_CORE_AD_RGS2                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0602))
#define MT6336_PMIC_ANA_CORE_AD_RGS3                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0603))
#define MT6336_PMIC_ANA_CORE_AD_RGS4                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0604))
#define MT6336_PMIC_ANA_CORE_AD_RGS5                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0605))
#define MT6336_PMIC_ANA_CORE_AD_RGS6                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0606))
#define MT6336_PMIC_RESERVED_1                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0607))
#define MT6336_PMIC_ANA_CORE_DA_RGS0                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0608))
#define MT6336_PMIC_ANA_CORE_DA_RGS1                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0609))
#define MT6336_PMIC_ANA_CORE_DA_RGS2                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x060A))
#define MT6336_PMIC_ANA_CORE_DA_RGS3                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x060B))
#define MT6336_PMIC_ANA_CORE_DA_RGS4                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x060C))
#define MT6336_PMIC_ANA_CORE_DA_RGS5                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x060D))
#define MT6336_PMIC_ANA_CORE_DA_RGS6                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x060E))
#define MT6336_PMIC_ANA_CORE_DA_RGS7                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x060F))
#define MT6336_PMIC_ANA_CORE_DA_RGS8                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0610))
#define MT6336_PMIC_ANA_CORE_DA_RGS9                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0611))
#define MT6336_PMIC_ANA_CORE_DA_RGS11                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0612))
#define MT6336_PMIC_ANA_CORE_DA_RGS12                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0613))
#define MT6336_PMIC_ANA_CORE_DA_RGS13                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0614))
#define MT6336_PMIC_RESERVED_2                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0615))
#define MT6336_PMIC_RESERVED_3                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0616))
#define MT6336_PMIC_RESERVED_4                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0617))
#define MT6336_PMIC_RESERVED_5                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0618))
#define MT6336_PMIC_RESERVED_6                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0619))
#define MT6336_PMIC_RESERVED_7                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x061A))
#define MT6336_PMIC_RESERVED_8                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x061B))
#define MT6336_PMIC_RESERVED_9                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x061C))
#define MT6336_PMIC_RESERVED_10                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x061D))
#define MT6336_PMIC_RESERVED_11                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x061E))
#define MT6336_PMIC_RESERVED_12                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x061F))
#define MT6336_PMIC_RESERVED_13                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0620))
#define MT6336_PMIC_RESERVED_14                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0621))
#define MT6336_PMIC_RESERVED_15                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0622))
#define MT6336_PMIC_RESERVED_16                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0623))
#define MT6336_PMIC_RESERVED_17                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0624))
#define MT6336_PMIC_RESERVED_18                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0625))
#define MT6336_PMIC_RESERVED_19                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0626))
#define MT6336_PMIC_RESERVED_20                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0627))
#define MT6336_PMIC_RESERVED_21                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0628))
#define MT6336_PMIC_RESERVED_22                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0629))
#define MT6336_PMIC_RESERVED_23                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x062A))
#define MT6336_PMIC_RESERVED_24                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x062B))
#define MT6336_PMIC_RESERVED_25                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x062C))
#define MT6336_PMIC_RESERVED_26                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x062D))
#define MT6336_PMIC_RESERVED_27                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x062E))
#define MT6336_PMIC_RESERVED_28                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x062F))
#define MT6336_PMIC_RESERVED_29                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0630))
#define MT6336_PMIC_RESERVED_30                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0631))
#define MT6336_PMIC_RESERVED_31                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0632))
#define MT6336_PMIC_RESERVED_32                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0633))
#define MT6336_PMIC_RESERVED_33                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0634))
#define MT6336_PMIC_RESERVED_34                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0635))
#define MT6336_PMIC_RESERVED_35                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0636))
#define MT6336_PMIC_RESERVED_36                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0637))
#define MT6336_PMIC_RESERVED_37                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0638))
#define MT6336_PMIC_RESERVED_38                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x0639))
#define MT6336_PMIC_RESERVED_39                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x063A))
#define MT6336_PMIC_RESERVED_40                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x063B))
#define MT6336_PMIC_RESERVED_41                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x063C))
#define MT6336_PMIC_RESERVED_42                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x063D))
#define MT6336_PMIC_RESERVED_43                         ((unsigned int)(MT6336_PMIC_REG_BASE+0x063E))
#define MT6336_PMIC_ANA_CORE_AD_INTF0_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x063F))
#define MT6336_PMIC_ANA_CORE_AD_INTF1_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0640))
#define MT6336_PMIC_ANA_CORE_AD_INTF2_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0641))
#define MT6336_PMIC_ANA_CORE_AD_INTF3_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0642))
#define MT6336_PMIC_ANA_CORE_AD_INTF4_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0643))
#define MT6336_PMIC_ANA_CORE_AD_INTF5_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0644))
#define MT6336_PMIC_ANA_CORE_AD_INTF6_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0645))
#define MT6336_PMIC_ANA_CORE_AD_INTF7_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0646))
#define MT6336_PMIC_ANA_CORE_DA_INTF0_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0647))
#define MT6336_PMIC_ANA_CORE_DA_INTF1_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0648))
#define MT6336_PMIC_ANA_CORE_DA_INTF2_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0649))
#define MT6336_PMIC_ANA_CORE_DA_INTF3_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x064A))
#define MT6336_PMIC_ANA_CORE_DA_INTF4_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x064B))
#define MT6336_PMIC_ANA_CORE_DA_INTF5_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x064C))
#define MT6336_PMIC_ANA_CORE_DA_INTF6_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x064D))
#define MT6336_PMIC_ANA_CORE_DA_INTF7_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x064E))
#define MT6336_PMIC_ANA_CORE_DA_INTF8_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x064F))
#define MT6336_PMIC_ANA_CORE_DA_INTF9_SEL               ((unsigned int)(MT6336_PMIC_REG_BASE+0x0650))
#define MT6336_PMIC_ANA_CORE_DA_INTF10_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0651))
#define MT6336_PMIC_ANA_CORE_DA_INTF11_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0652))
#define MT6336_PMIC_ANA_CORE_DA_INTF12_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0653))
#define MT6336_PMIC_ANA_CORE_DA_INTF13_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0654))
#define MT6336_PMIC_ANA_CORE_DA_INTF14_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0655))
#define MT6336_PMIC_ANA_CORE_DA_INTF15_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0656))
#define MT6336_PMIC_ANA_CORE_DA_INTF16_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0657))
#define MT6336_PMIC_ANA_CORE_DA_INTF17_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0658))
#define MT6336_PMIC_ANA_CORE_DA_INTF18_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0659))
#define MT6336_PMIC_ANA_CORE_DA_INTF19_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x065A))
#define MT6336_PMIC_ANA_CORE_DA_INTF20_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x065B))
#define MT6336_PMIC_ANA_CORE_DA_INTF21_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x065C))
#define MT6336_PMIC_ANA_CORE_DA_INTF22_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x065D))
#define MT6336_PMIC_ANA_CORE_DA_INTF23_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x065E))
#define MT6336_PMIC_ANA_CORE_DA_INTF24_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x065F))
#define MT6336_PMIC_ANA_CORE_DA_INTF25_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0660))
#define MT6336_PMIC_ANA_AUXADC_AD_INTF0_SEL             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0661))
#define MT6336_PMIC_ANA_AUXADC_DA_INTF0_SEL             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0662))
#define MT6336_PMIC_ANA_AUXADC_DA_INTF1_SEL             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0663))
#define MT6336_PMIC_ANA_AUXADC_DA_INTF2_SEL             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0664))
#define MT6336_PMIC_ANA_AUXADC_DA_INTF3_SEL             ((unsigned int)(MT6336_PMIC_REG_BASE+0x0665))
#define MT6336_PMIC_ANA_TYPEC_AD_INTF0_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0666))
#define MT6336_PMIC_ANA_TYPEC_DA_INTF0_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0667))
#define MT6336_PMIC_ANA_TYPEC_DA_INTF1_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0668))
#define MT6336_PMIC_ANA_TYPEC_DA_INTF2_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x0669))
#define MT6336_PMIC_ANA_TYPEC_DA_INTF3_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x066A))
#define MT6336_PMIC_ANA_TYPEC_DA_INTF4_SEL              ((unsigned int)(MT6336_PMIC_REG_BASE+0x066B))
#define MT6336_PMIC_ANA_CLK_SEL1                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x066C))
#define MT6336_PMIC_ANA_CLK_SEL2                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x066D))
#define MT6336_PMIC_GPIO_DIR0                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0700))
#define MT6336_PMIC_GPIO_DIR0_SET                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0701))
#define MT6336_PMIC_GPIO_DIR0_CLR                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0702))
#define MT6336_PMIC_GPIO_DIR1                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0703))
#define MT6336_PMIC_GPIO_DIR1_SET                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0704))
#define MT6336_PMIC_GPIO_DIR1_CLR                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x0705))
#define MT6336_PMIC_GPIO_PULLEN0                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0706))
#define MT6336_PMIC_GPIO_PULLEN0_SET                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0707))
#define MT6336_PMIC_GPIO_PULLEN0_CLR                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x0708))
#define MT6336_PMIC_GPIO_PULLEN1                        ((unsigned int)(MT6336_PMIC_REG_BASE+0x0709))
#define MT6336_PMIC_GPIO_PULLEN1_SET                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x070A))
#define MT6336_PMIC_GPIO_PULLEN1_CLR                    ((unsigned int)(MT6336_PMIC_REG_BASE+0x070B))
#define MT6336_PMIC_GPIO_PULLSEL0                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x070C))
#define MT6336_PMIC_GPIO_PULLSEL0_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x070D))
#define MT6336_PMIC_GPIO_PULLSEL0_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x070E))
#define MT6336_PMIC_GPIO_PULLSEL1                       ((unsigned int)(MT6336_PMIC_REG_BASE+0x070F))
#define MT6336_PMIC_GPIO_PULLSEL1_SET                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0710))
#define MT6336_PMIC_GPIO_PULLSEL1_CLR                   ((unsigned int)(MT6336_PMIC_REG_BASE+0x0711))
#define MT6336_PMIC_GPIO_DINV0                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0712))
#define MT6336_PMIC_GPIO_DINV0_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0713))
#define MT6336_PMIC_GPIO_DINV0_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0714))
#define MT6336_PMIC_GPIO_DINV1                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0715))
#define MT6336_PMIC_GPIO_DINV1_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0716))
#define MT6336_PMIC_GPIO_DINV1_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0717))
#define MT6336_PMIC_GPIO_DOUT0                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0718))
#define MT6336_PMIC_GPIO_DOUT0_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0719))
#define MT6336_PMIC_GPIO_DOUT0_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x071A))
#define MT6336_PMIC_GPIO_DOUT1                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x071B))
#define MT6336_PMIC_GPIO_DOUT1_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x071C))
#define MT6336_PMIC_GPIO_DOUT1_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x071D))
#define MT6336_PMIC_GPIO_PI0                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x071E))
#define MT6336_PMIC_GPIO_PI1                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x071F))
#define MT6336_PMIC_GPIO_POE0                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0720))
#define MT6336_PMIC_GPIO_POE1                           ((unsigned int)(MT6336_PMIC_REG_BASE+0x0721))
#define MT6336_PMIC_GPIO_MODE0                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0722))
#define MT6336_PMIC_GPIO_MODE0_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0723))
#define MT6336_PMIC_GPIO_MODE0_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0724))
#define MT6336_PMIC_GPIO_MODE1                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0725))
#define MT6336_PMIC_GPIO_MODE1_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0726))
#define MT6336_PMIC_GPIO_MODE1_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0727))
#define MT6336_PMIC_GPIO_MODE2                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0728))
#define MT6336_PMIC_GPIO_MODE2_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0729))
#define MT6336_PMIC_GPIO_MODE2_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x072A))
#define MT6336_PMIC_GPIO_MODE3                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x072B))
#define MT6336_PMIC_GPIO_MODE3_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x072C))
#define MT6336_PMIC_GPIO_MODE3_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x072D))
#define MT6336_PMIC_GPIO_MODE4                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x072E))
#define MT6336_PMIC_GPIO_MODE4_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x072F))
#define MT6336_PMIC_GPIO_MODE4_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0730))
#define MT6336_PMIC_GPIO_MODE5                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0731))
#define MT6336_PMIC_GPIO_MODE5_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0732))
#define MT6336_PMIC_GPIO_MODE5_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0733))
#define MT6336_PMIC_GPIO_MODE6                          ((unsigned int)(MT6336_PMIC_REG_BASE+0x0734))
#define MT6336_PMIC_GPIO_MODE6_SET                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0735))
#define MT6336_PMIC_GPIO_MODE6_CLR                      ((unsigned int)(MT6336_PMIC_REG_BASE+0x0736))
#define MT6336_PMIC_GPIO_RSV                            ((unsigned int)(MT6336_PMIC_REG_BASE+0x0737))

#define MT6336_CID_ADDR                                                  MT6336_PMIC_CID
#define MT6336_CID_MASK                                                  0xFF
#define MT6336_CID_SHIFT                                                 0
#define MT6336_SWCID_ADDR                                                MT6336_PMIC_SWCID
#define MT6336_SWCID_MASK                                                0xFF
#define MT6336_SWCID_SHIFT                                               0
#define MT6336_HWCID_ADDR                                                MT6336_PMIC_HWCID
#define MT6336_HWCID_MASK                                                0xFF
#define MT6336_HWCID_SHIFT                                               0
#define MT6336_I2C_CONFIG_ADDR                                           MT6336_PMIC_TOP_CON
#define MT6336_I2C_CONFIG_MASK                                           0x1
#define MT6336_I2C_CONFIG_SHIFT                                          0
#define MT6336_I2C_RD_LEN_ADDR                                           MT6336_PMIC_TOP_CON
#define MT6336_I2C_RD_LEN_MASK                                           0x7F
#define MT6336_I2C_RD_LEN_SHIFT                                          1
#define MT6336_TEST_OUT0_ADDR                                            MT6336_PMIC_TEST_OUT0
#define MT6336_TEST_OUT0_MASK                                            0xFF
#define MT6336_TEST_OUT0_SHIFT                                           0
#define MT6336_TEST_OUT1_ADDR                                            MT6336_PMIC_TEST_OUT1
#define MT6336_TEST_OUT1_MASK                                            0xFF
#define MT6336_TEST_OUT1_SHIFT                                           0
#define MT6336_RG_MON_GRP_SEL_ADDR                                       MT6336_PMIC_TEST_CON0
#define MT6336_RG_MON_GRP_SEL_MASK                                       0x1F
#define MT6336_RG_MON_GRP_SEL_SHIFT                                      0
#define MT6336_RG_MON_FLAG_SEL_ADDR                                      MT6336_PMIC_TEST_CON1
#define MT6336_RG_MON_FLAG_SEL_MASK                                      0xFF
#define MT6336_RG_MON_FLAG_SEL_SHIFT                                     0
#define MT6336_RG_SWCHR_FLAG_SEL_ADDR                                    MT6336_PMIC_TEST_CON2
#define MT6336_RG_SWCHR_FLAG_SEL_MASK                                    0xF
#define MT6336_RG_SWCHR_FLAG_SEL_SHIFT                                   0
#define MT6336_RG_NANDTREE_MODE_ADDR                                     MT6336_PMIC_TEST_CON3
#define MT6336_RG_NANDTREE_MODE_MASK                                     0x1
#define MT6336_RG_NANDTREE_MODE_SHIFT                                    0
#define MT6336_RG_TEST_AUXADC_ADDR                                       MT6336_PMIC_TEST_CON3
#define MT6336_RG_TEST_AUXADC_MASK                                       0x1
#define MT6336_RG_TEST_AUXADC_SHIFT                                      1
#define MT6336_RG_EFUSE_MODE_ADDR                                        MT6336_PMIC_TEST_CON3
#define MT6336_RG_EFUSE_MODE_MASK                                        0x1
#define MT6336_RG_EFUSE_MODE_SHIFT                                       2
#define MT6336_RG_TEST_SWCHR_ADDR                                        MT6336_PMIC_TEST_CON3
#define MT6336_RG_TEST_SWCHR_MASK                                        0x1
#define MT6336_RG_TEST_SWCHR_SHIFT                                       3
#define MT6336_RG_ANAIF_BYPASS_MODE_ADDR                                 MT6336_PMIC_TEST_CON3
#define MT6336_RG_ANAIF_BYPASS_MODE_MASK                                 0x1
#define MT6336_RG_ANAIF_BYPASS_MODE_SHIFT                                4
#define MT6336_RG_TEST_RSTB_ADDR                                         MT6336_PMIC_TEST_RST_CON
#define MT6336_RG_TEST_RSTB_MASK                                         0x1
#define MT6336_RG_TEST_RSTB_SHIFT                                        0
#define MT6336_RG_TEST_RESET_SEL_ADDR                                    MT6336_PMIC_TEST_RST_CON
#define MT6336_RG_TEST_RESET_SEL_MASK                                    0x1
#define MT6336_RG_TEST_RESET_SEL_SHIFT                                   1
#define MT6336_TESTMODE_SW_ADDR                                          MT6336_PMIC_TESTMODE_SW
#define MT6336_TESTMODE_SW_MASK                                          0x1
#define MT6336_TESTMODE_SW_SHIFT                                         0
#define MT6336_PMU_TEST_MODE_SCAN_ADDR                                   MT6336_PMIC_TOPSTATUS
#define MT6336_PMU_TEST_MODE_SCAN_MASK                                   0x1
#define MT6336_PMU_TEST_MODE_SCAN_SHIFT                                  0
#define MT6336_RG_PMU_TDSEL_ADDR                                         MT6336_PMIC_TDSEL_CON
#define MT6336_RG_PMU_TDSEL_MASK                                         0x1
#define MT6336_RG_PMU_TDSEL_SHIFT                                        0
#define MT6336_RG_I2C_TDSEL_ADDR                                         MT6336_PMIC_TDSEL_CON
#define MT6336_RG_I2C_TDSEL_MASK                                         0x1
#define MT6336_RG_I2C_TDSEL_SHIFT                                        1
#define MT6336_RG_PMU_RDSEL_ADDR                                         MT6336_PMIC_RDSEL_CON
#define MT6336_RG_PMU_RDSEL_MASK                                         0x1
#define MT6336_RG_PMU_RDSEL_SHIFT                                        0
#define MT6336_RG_I2C_RDSEL_ADDR                                         MT6336_PMIC_RDSEL_CON
#define MT6336_RG_I2C_RDSEL_MASK                                         0x1
#define MT6336_RG_I2C_RDSEL_SHIFT                                        1
#define MT6336_RG_I2C_FILTER_ADDR                                        MT6336_PMIC_RDSEL_CON
#define MT6336_RG_I2C_FILTER_MASK                                        0x3
#define MT6336_RG_I2C_FILTER_SHIFT                                       2
#define MT6336_RG_SMT_SCL_ADDR                                           MT6336_PMIC_SMT_CON0
#define MT6336_RG_SMT_SCL_MASK                                           0x1
#define MT6336_RG_SMT_SCL_SHIFT                                          0
#define MT6336_RG_SMT_SDA_ADDR                                           MT6336_PMIC_SMT_CON0
#define MT6336_RG_SMT_SDA_MASK                                           0x1
#define MT6336_RG_SMT_SDA_SHIFT                                          1
#define MT6336_RG_SMT_IRQ_ADDR                                           MT6336_PMIC_SMT_CON0
#define MT6336_RG_SMT_IRQ_MASK                                           0x1
#define MT6336_RG_SMT_IRQ_SHIFT                                          2
#define MT6336_RG_SMT_OTG_ADDR                                           MT6336_PMIC_SMT_CON0
#define MT6336_RG_SMT_OTG_MASK                                           0x1
#define MT6336_RG_SMT_OTG_SHIFT                                          3
#define MT6336_RG_SMT_STROBE_ADDR                                        MT6336_PMIC_SMT_CON0
#define MT6336_RG_SMT_STROBE_MASK                                        0x1
#define MT6336_RG_SMT_STROBE_SHIFT                                       4
#define MT6336_RG_SMT_TXMASK_ADDR                                        MT6336_PMIC_SMT_CON0
#define MT6336_RG_SMT_TXMASK_MASK                                        0x1
#define MT6336_RG_SMT_TXMASK_SHIFT                                       5
#define MT6336_RG_SMT_GPIO2_ADDR                                         MT6336_PMIC_SMT_CON1
#define MT6336_RG_SMT_GPIO2_MASK                                         0x1
#define MT6336_RG_SMT_GPIO2_SHIFT                                        0
#define MT6336_RG_SMT_GPIO3_ADDR                                         MT6336_PMIC_SMT_CON1
#define MT6336_RG_SMT_GPIO3_MASK                                         0x1
#define MT6336_RG_SMT_GPIO3_SHIFT                                        1
#define MT6336_RG_SMT_GPIO4_ADDR                                         MT6336_PMIC_SMT_CON1
#define MT6336_RG_SMT_GPIO4_MASK                                         0x1
#define MT6336_RG_SMT_GPIO4_SHIFT                                        2
#define MT6336_RG_SMT_GPIO5_ADDR                                         MT6336_PMIC_SMT_CON1
#define MT6336_RG_SMT_GPIO5_MASK                                         0x1
#define MT6336_RG_SMT_GPIO5_SHIFT                                        3
#define MT6336_RG_SMT_GPIO6_ADDR                                         MT6336_PMIC_SMT_CON1
#define MT6336_RG_SMT_GPIO6_MASK                                         0x1
#define MT6336_RG_SMT_GPIO6_SHIFT                                        4
#define MT6336_RG_SMT_GPIO7_ADDR                                         MT6336_PMIC_SMT_CON1
#define MT6336_RG_SMT_GPIO7_MASK                                         0x1
#define MT6336_RG_SMT_GPIO7_SHIFT                                        5
#define MT6336_RG_SMT_GPIO0_ADDR                                         MT6336_PMIC_SMT_CON1
#define MT6336_RG_SMT_GPIO0_MASK                                         0x1
#define MT6336_RG_SMT_GPIO0_SHIFT                                        6
#define MT6336_RG_SMT_GPIO1_ADDR                                         MT6336_PMIC_SMT_CON1
#define MT6336_RG_SMT_GPIO1_MASK                                         0x1
#define MT6336_RG_SMT_GPIO1_SHIFT                                        7
#define MT6336_RG_OCTL_SCL_ADDR                                          MT6336_PMIC_DRV_CON0
#define MT6336_RG_OCTL_SCL_MASK                                          0xF
#define MT6336_RG_OCTL_SCL_SHIFT                                         0
#define MT6336_RG_OCTL_SDA_ADDR                                          MT6336_PMIC_DRV_CON0
#define MT6336_RG_OCTL_SDA_MASK                                          0xF
#define MT6336_RG_OCTL_SDA_SHIFT                                         4
#define MT6336_RG_OCTL_IRQ_ADDR                                          MT6336_PMIC_DRV_CON1
#define MT6336_RG_OCTL_IRQ_MASK                                          0xF
#define MT6336_RG_OCTL_IRQ_SHIFT                                         0
#define MT6336_RG_OCTL_OTG_ADDR                                          MT6336_PMIC_DRV_CON1
#define MT6336_RG_OCTL_OTG_MASK                                          0xF
#define MT6336_RG_OCTL_OTG_SHIFT                                         4
#define MT6336_RG_OCTL_STROBE_ADDR                                       MT6336_PMIC_DRV_CON2
#define MT6336_RG_OCTL_STROBE_MASK                                       0xF
#define MT6336_RG_OCTL_STROBE_SHIFT                                      0
#define MT6336_RG_OCTL_TXMASK_ADDR                                       MT6336_PMIC_DRV_CON2
#define MT6336_RG_OCTL_TXMASK_MASK                                       0xF
#define MT6336_RG_OCTL_TXMASK_SHIFT                                      4
#define MT6336_RG_OCTL_GPIO0_ADDR                                        MT6336_PMIC_DRV_CON3
#define MT6336_RG_OCTL_GPIO0_MASK                                        0xF
#define MT6336_RG_OCTL_GPIO0_SHIFT                                       0
#define MT6336_RG_OCTL_GPIO1_ADDR                                        MT6336_PMIC_DRV_CON3
#define MT6336_RG_OCTL_GPIO1_MASK                                        0xF
#define MT6336_RG_OCTL_GPIO1_SHIFT                                       4
#define MT6336_RG_OCTL_GPIO2_ADDR                                        MT6336_PMIC_DRV_CON4
#define MT6336_RG_OCTL_GPIO2_MASK                                        0xF
#define MT6336_RG_OCTL_GPIO2_SHIFT                                       0
#define MT6336_RG_OCTL_GPIO3_ADDR                                        MT6336_PMIC_DRV_CON4
#define MT6336_RG_OCTL_GPIO3_MASK                                        0xF
#define MT6336_RG_OCTL_GPIO3_SHIFT                                       4
#define MT6336_RG_OCTL_GPIO4_ADDR                                        MT6336_PMIC_DRV_CON5
#define MT6336_RG_OCTL_GPIO4_MASK                                        0xF
#define MT6336_RG_OCTL_GPIO4_SHIFT                                       0
#define MT6336_RG_OCTL_GPIO5_ADDR                                        MT6336_PMIC_DRV_CON5
#define MT6336_RG_OCTL_GPIO5_MASK                                        0xF
#define MT6336_RG_OCTL_GPIO5_SHIFT                                       4
#define MT6336_RG_OCTL_GPIO6_ADDR                                        MT6336_PMIC_DRV_CON6
#define MT6336_RG_OCTL_GPIO6_MASK                                        0xF
#define MT6336_RG_OCTL_GPIO6_SHIFT                                       0
#define MT6336_RG_OCTL_GPIO7_ADDR                                        MT6336_PMIC_DRV_CON6
#define MT6336_RG_OCTL_GPIO7_MASK                                        0xF
#define MT6336_RG_OCTL_GPIO7_SHIFT                                       4
#define MT6336_TOP_STATUS_ADDR                                           MT6336_PMIC_TOP_STATUS
#define MT6336_TOP_STATUS_MASK                                           0xF
#define MT6336_TOP_STATUS_SHIFT                                          0
#define MT6336_TOP_STATUS_SET_ADDR                                       MT6336_PMIC_TOP_STATUS_SET
#define MT6336_TOP_STATUS_SET_MASK                                       0x3
#define MT6336_TOP_STATUS_SET_SHIFT                                      0
#define MT6336_TOP_STATUS_CLR_ADDR                                       MT6336_PMIC_TOP_STATUS_CLR
#define MT6336_TOP_STATUS_CLR_MASK                                       0x3
#define MT6336_TOP_STATUS_CLR_SHIFT                                      0
#define MT6336_TOP_RSV0_ADDR                                             MT6336_PMIC_TOP_RSV0
#define MT6336_TOP_RSV0_MASK                                             0xFF
#define MT6336_TOP_RSV0_SHIFT                                            0
#define MT6336_TOP_RSV1_ADDR                                             MT6336_PMIC_TOP_RSV1
#define MT6336_TOP_RSV1_MASK                                             0xFF
#define MT6336_TOP_RSV1_SHIFT                                            0
#define MT6336_TOP_RSV2_ADDR                                             MT6336_PMIC_TOP_RSV2
#define MT6336_TOP_RSV2_MASK                                             0xFF
#define MT6336_TOP_RSV2_SHIFT                                            0
#define MT6336_CLK_RSV_CON0_RSV_ADDR                                     MT6336_PMIC_CLK_RSV_CON0
#define MT6336_CLK_RSV_CON0_RSV_MASK                                     0xFF
#define MT6336_CLK_RSV_CON0_RSV_SHIFT                                    0
#define MT6336_CLK_RSV_CON1_RSV_ADDR                                     MT6336_PMIC_CLK_RSV_CON1
#define MT6336_CLK_RSV_CON1_RSV_MASK                                     0xFF
#define MT6336_CLK_RSV_CON1_RSV_SHIFT                                    0
#define MT6336_RG_CLKSQ_EN_PD_ADDR                                       MT6336_PMIC_TOP_CLKSQ
#define MT6336_RG_CLKSQ_EN_PD_MASK                                       0x1
#define MT6336_RG_CLKSQ_EN_PD_SHIFT                                      0
#define MT6336_RG_CLKSQ_EN_FQR_ADDR                                      MT6336_PMIC_TOP_CLKSQ
#define MT6336_RG_CLKSQ_EN_FQR_MASK                                      0x1
#define MT6336_RG_CLKSQ_EN_FQR_SHIFT                                     1
#define MT6336_RG_CLKSQ_EN_RSV_ADDR                                      MT6336_PMIC_TOP_CLKSQ
#define MT6336_RG_CLKSQ_EN_RSV_MASK                                      0x1F
#define MT6336_RG_CLKSQ_EN_RSV_SHIFT                                     2
#define MT6336_DA_QI_CLKSQ_EN_ADDR                                       MT6336_PMIC_TOP_CLKSQ
#define MT6336_DA_QI_CLKSQ_EN_MASK                                       0x1
#define MT6336_DA_QI_CLKSQ_EN_SHIFT                                      7
#define MT6336_TOP_CLKSQ_SET_ADDR                                        MT6336_PMIC_TOP_CLKSQ_SET
#define MT6336_TOP_CLKSQ_SET_MASK                                        0xFF
#define MT6336_TOP_CLKSQ_SET_SHIFT                                       0
#define MT6336_TOP_CLKSQ_CLR_ADDR                                        MT6336_PMIC_TOP_CLKSQ_CLR
#define MT6336_TOP_CLKSQ_CLR_MASK                                        0xFF
#define MT6336_TOP_CLKSQ_CLR_SHIFT                                       0
#define MT6336_DA_QI_BASE_CLK_TRIM_ADDR                                  MT6336_PMIC_TOP_CLK300K_TRIM
#define MT6336_DA_QI_BASE_CLK_TRIM_MASK                                  0x3F
#define MT6336_DA_QI_BASE_CLK_TRIM_SHIFT                                 0
#define MT6336_RG_BASE_CLK_TRIM_EN_ADDR                                  MT6336_PMIC_TOP_CLK300K_TRIM_CON0
#define MT6336_RG_BASE_CLK_TRIM_EN_MASK                                  0x1
#define MT6336_RG_BASE_CLK_TRIM_EN_SHIFT                                 0
#define MT6336_RG_BASE_CLK_TRIM_RATE_ADDR                                MT6336_PMIC_TOP_CLK300K_TRIM_CON0
#define MT6336_RG_BASE_CLK_TRIM_RATE_MASK                                0x3
#define MT6336_RG_BASE_CLK_TRIM_RATE_SHIFT                               6
#define MT6336_RG_BASE_CLK_TRIM_ADDR                                     MT6336_PMIC_TOP_CLK300K_TRIM_CON1
#define MT6336_RG_BASE_CLK_TRIM_MASK                                     0x3F
#define MT6336_RG_BASE_CLK_TRIM_SHIFT                                    0
#define MT6336_DA_QI_OSC_TRIM_ADDR                                       MT6336_PMIC_TOP_CLK6M_TRIM
#define MT6336_DA_QI_OSC_TRIM_MASK                                       0x3F
#define MT6336_DA_QI_OSC_TRIM_SHIFT                                      0
#define MT6336_RG_OSC_TRIM_EN_ADDR                                       MT6336_PMIC_TOP_CLK6M_TRIM_CON0
#define MT6336_RG_OSC_TRIM_EN_MASK                                       0x1
#define MT6336_RG_OSC_TRIM_EN_SHIFT                                      0
#define MT6336_RG_OSC_TRIM_RATE_ADDR                                     MT6336_PMIC_TOP_CLK6M_TRIM_CON0
#define MT6336_RG_OSC_TRIM_RATE_MASK                                     0x3
#define MT6336_RG_OSC_TRIM_RATE_SHIFT                                    6
#define MT6336_RG_OSC_CLK_TRIM_ADDR                                      MT6336_PMIC_TOP_CLK6M_TRIM_CON1
#define MT6336_RG_OSC_CLK_TRIM_MASK                                      0x3F
#define MT6336_RG_OSC_CLK_TRIM_SHIFT                                     0
#define MT6336_CLK_HF_6M_CK_TST_DIS_ADDR                                 MT6336_PMIC_CLK_CKROOTTST_CON0
#define MT6336_CLK_HF_6M_CK_TST_DIS_MASK                                 0x1
#define MT6336_CLK_HF_6M_CK_TST_DIS_SHIFT                                0
#define MT6336_CLK_BASE_300K_CK_TST_DIS_ADDR                             MT6336_PMIC_CLK_CKROOTTST_CON0
#define MT6336_CLK_BASE_300K_CK_TST_DIS_MASK                             0x1
#define MT6336_CLK_BASE_300K_CK_TST_DIS_SHIFT                            1
#define MT6336_CLK_CLK_26M_CK_TST_DIS_ADDR                               MT6336_PMIC_CLK_CKROOTTST_CON0
#define MT6336_CLK_CLK_26M_CK_TST_DIS_MASK                               0x1
#define MT6336_CLK_CLK_26M_CK_TST_DIS_SHIFT                              2
#define MT6336_CLK_PD_SLEW_CK_TST_DIS_ADDR                               MT6336_PMIC_CLK_CKROOTTST_CON0
#define MT6336_CLK_PD_SLEW_CK_TST_DIS_MASK                               0x1
#define MT6336_CLK_PD_SLEW_CK_TST_DIS_SHIFT                              3
#define MT6336_CLK_PMU_75K_CK_TST_DIS_ADDR                               MT6336_PMIC_CLK_CKROOTTST_CON0
#define MT6336_CLK_PMU_75K_CK_TST_DIS_MASK                               0x1
#define MT6336_CLK_PMU_75K_CK_TST_DIS_SHIFT                              4
#define MT6336_CLK_CKROOTTST_CON0_RSV_ADDR                               MT6336_PMIC_CLK_CKROOTTST_CON0
#define MT6336_CLK_CKROOTTST_CON0_RSV_MASK                               0x7
#define MT6336_CLK_CKROOTTST_CON0_RSV_SHIFT                              5
#define MT6336_CLK_HF_6M_CK_TSTSEL_ADDR                                  MT6336_PMIC_CLK_CKROOTTST_CON1
#define MT6336_CLK_HF_6M_CK_TSTSEL_MASK                                  0x1
#define MT6336_CLK_HF_6M_CK_TSTSEL_SHIFT                                 0
#define MT6336_CLK_BASE_300K_CK_TSTSEL_ADDR                              MT6336_PMIC_CLK_CKROOTTST_CON1
#define MT6336_CLK_BASE_300K_CK_TSTSEL_MASK                              0x1
#define MT6336_CLK_BASE_300K_CK_TSTSEL_SHIFT                             1
#define MT6336_CLK_CLK_26M_CK_TSTSEL_ADDR                                MT6336_PMIC_CLK_CKROOTTST_CON1
#define MT6336_CLK_CLK_26M_CK_TSTSEL_MASK                                0x1
#define MT6336_CLK_CLK_26M_CK_TSTSEL_SHIFT                               2
#define MT6336_CLK_PD_SLEW_CK_TSTSEL_ADDR                                MT6336_PMIC_CLK_CKROOTTST_CON1
#define MT6336_CLK_PD_SLEW_CK_TSTSEL_MASK                                0x1
#define MT6336_CLK_PD_SLEW_CK_TSTSEL_SHIFT                               3
#define MT6336_CLK_PMU_75K_CK_TSTSEL_ADDR                                MT6336_PMIC_CLK_CKROOTTST_CON1
#define MT6336_CLK_PMU_75K_CK_TSTSEL_MASK                                0x1
#define MT6336_CLK_PMU_75K_CK_TSTSEL_SHIFT                               4
#define MT6336_CLK_CKROOTTST_CON1_RSV_ADDR                               MT6336_PMIC_CLK_CKROOTTST_CON1
#define MT6336_CLK_CKROOTTST_CON1_RSV_MASK                               0x7
#define MT6336_CLK_CKROOTTST_CON1_RSV_SHIFT                              5
#define MT6336_CLK_TOP_AO_75K_CK_PDN_ADDR                                MT6336_PMIC_CLK_CKPDN_CON0
#define MT6336_CLK_TOP_AO_75K_CK_PDN_MASK                                0x1
#define MT6336_CLK_TOP_AO_75K_CK_PDN_SHIFT                               0
#define MT6336_CLK_SWCHR_6M_SW_CK_PDN_ADDR                               MT6336_PMIC_CLK_CKPDN_CON0
#define MT6336_CLK_SWCHR_6M_SW_CK_PDN_MASK                               0x1
#define MT6336_CLK_SWCHR_6M_SW_CK_PDN_SHIFT                              1
#define MT6336_CLK_SWCHR_6M_CK_PDN_ADDR                                  MT6336_PMIC_CLK_CKPDN_CON0
#define MT6336_CLK_SWCHR_6M_CK_PDN_MASK                                  0x1
#define MT6336_CLK_SWCHR_6M_CK_PDN_SHIFT                                 2
#define MT6336_CLK_SWCHR_3M_CK_PDN_ADDR                                  MT6336_PMIC_CLK_CKPDN_CON0
#define MT6336_CLK_SWCHR_3M_CK_PDN_MASK                                  0x1
#define MT6336_CLK_SWCHR_3M_CK_PDN_SHIFT                                 3
#define MT6336_CLK_SWCHR_2M_SW_CK_PDN_ADDR                               MT6336_PMIC_CLK_CKPDN_CON0
#define MT6336_CLK_SWCHR_2M_SW_CK_PDN_MASK                               0x1
#define MT6336_CLK_SWCHR_2M_SW_CK_PDN_SHIFT                              4
#define MT6336_CLK_SWCHR_2M_CK_PDN_ADDR                                  MT6336_PMIC_CLK_CKPDN_CON0
#define MT6336_CLK_SWCHR_2M_CK_PDN_MASK                                  0x1
#define MT6336_CLK_SWCHR_2M_CK_PDN_SHIFT                                 5
#define MT6336_CLK_SWCHR_AO_300K_CK_PDN_ADDR                             MT6336_PMIC_CLK_CKPDN_CON0
#define MT6336_CLK_SWCHR_AO_300K_CK_PDN_MASK                             0x1
#define MT6336_CLK_SWCHR_AO_300K_CK_PDN_SHIFT                            6
#define MT6336_CLK_SWCHR_300K_CK_PDN_ADDR                                MT6336_PMIC_CLK_CKPDN_CON0
#define MT6336_CLK_SWCHR_300K_CK_PDN_MASK                                0x1
#define MT6336_CLK_SWCHR_300K_CK_PDN_SHIFT                               7
#define MT6336_CLK_CKPDN_CON0_SET_ADDR                                   MT6336_PMIC_CLK_CKPDN_CON0_SET
#define MT6336_CLK_CKPDN_CON0_SET_MASK                                   0xFF
#define MT6336_CLK_CKPDN_CON0_SET_SHIFT                                  0
#define MT6336_CLK_CKPDN_CON0_CLR_ADDR                                   MT6336_PMIC_CLK_CKPDN_CON0_CLR
#define MT6336_CLK_CKPDN_CON0_CLR_MASK                                   0xFF
#define MT6336_CLK_CKPDN_CON0_CLR_SHIFT                                  0
#define MT6336_CLK_SWCHR_75K_CK_PDN_ADDR                                 MT6336_PMIC_CLK_CKPDN_CON1
#define MT6336_CLK_SWCHR_75K_CK_PDN_MASK                                 0x1
#define MT6336_CLK_SWCHR_75K_CK_PDN_SHIFT                                0
#define MT6336_CLK_SWCHR_AO_1K_CK_PDN_ADDR                               MT6336_PMIC_CLK_CKPDN_CON1
#define MT6336_CLK_SWCHR_AO_1K_CK_PDN_MASK                               0x1
#define MT6336_CLK_SWCHR_AO_1K_CK_PDN_SHIFT                              1
#define MT6336_CLK_SWCHR_1K_CK_PDN_ADDR                                  MT6336_PMIC_CLK_CKPDN_CON1
#define MT6336_CLK_SWCHR_1K_CK_PDN_MASK                                  0x1
#define MT6336_CLK_SWCHR_1K_CK_PDN_SHIFT                                 2
#define MT6336_CLK_AUXADC_300K_CK_PDN_ADDR                               MT6336_PMIC_CLK_CKPDN_CON1
#define MT6336_CLK_AUXADC_300K_CK_PDN_MASK                               0x1
#define MT6336_CLK_AUXADC_300K_CK_PDN_SHIFT                              3
#define MT6336_CLK_AUXADC_CK_PDN_ADDR                                    MT6336_PMIC_CLK_CKPDN_CON1
#define MT6336_CLK_AUXADC_CK_PDN_MASK                                    0x1
#define MT6336_CLK_AUXADC_CK_PDN_SHIFT                                   4
#define MT6336_CLK_DRV_CHRIND_CK_PDN_ADDR                                MT6336_PMIC_CLK_CKPDN_CON1
#define MT6336_CLK_DRV_CHRIND_CK_PDN_MASK                                0x1
#define MT6336_CLK_DRV_CHRIND_CK_PDN_SHIFT                               5
#define MT6336_CLK_DRV_75K_CK_PDN_ADDR                                   MT6336_PMIC_CLK_CKPDN_CON1
#define MT6336_CLK_DRV_75K_CK_PDN_MASK                                   0x1
#define MT6336_CLK_DRV_75K_CK_PDN_SHIFT                                  6
#define MT6336_CLK_EFUSE_CK_PDN_ADDR                                     MT6336_PMIC_CLK_CKPDN_CON1
#define MT6336_CLK_EFUSE_CK_PDN_MASK                                     0x1
#define MT6336_CLK_EFUSE_CK_PDN_SHIFT                                    7
#define MT6336_CLK_CKPDN_CON1_SET_ADDR                                   MT6336_PMIC_CLK_CKPDN_CON1_SET
#define MT6336_CLK_CKPDN_CON1_SET_MASK                                   0xFF
#define MT6336_CLK_CKPDN_CON1_SET_SHIFT                                  0
#define MT6336_CLK_CKPDN_CON1_CLR_ADDR                                   MT6336_PMIC_CLK_CKPDN_CON1_CLR
#define MT6336_CLK_CKPDN_CON1_CLR_MASK                                   0xFF
#define MT6336_CLK_CKPDN_CON1_CLR_SHIFT                                  0
#define MT6336_CLK_FQMTR_CK_PDN_ADDR                                     MT6336_PMIC_CLK_CKPDN_CON2
#define MT6336_CLK_FQMTR_CK_PDN_MASK                                     0x1
#define MT6336_CLK_FQMTR_CK_PDN_SHIFT                                    0
#define MT6336_CLK_FQMTR_26M_CK_PDN_ADDR                                 MT6336_PMIC_CLK_CKPDN_CON2
#define MT6336_CLK_FQMTR_26M_CK_PDN_MASK                                 0x1
#define MT6336_CLK_FQMTR_26M_CK_PDN_SHIFT                                1
#define MT6336_CLK_FQMTR_75K_CK_PDN_ADDR                                 MT6336_PMIC_CLK_CKPDN_CON2
#define MT6336_CLK_FQMTR_75K_CK_PDN_MASK                                 0x1
#define MT6336_CLK_FQMTR_75K_CK_PDN_SHIFT                                2
#define MT6336_CLK_INTRP_CK_PDN_ADDR                                     MT6336_PMIC_CLK_CKPDN_CON2
#define MT6336_CLK_INTRP_CK_PDN_MASK                                     0x1
#define MT6336_CLK_INTRP_CK_PDN_SHIFT                                    3
#define MT6336_CLK_TRIM_75K_CK_PDN_ADDR                                  MT6336_PMIC_CLK_CKPDN_CON2
#define MT6336_CLK_TRIM_75K_CK_PDN_MASK                                  0x1
#define MT6336_CLK_TRIM_75K_CK_PDN_SHIFT                                 4
#define MT6336_CLK_I2C_CK_PDN_ADDR                                       MT6336_PMIC_CLK_CKPDN_CON2
#define MT6336_CLK_I2C_CK_PDN_MASK                                       0x1
#define MT6336_CLK_I2C_CK_PDN_SHIFT                                      5
#define MT6336_CLK_REG_CK_PDN_ADDR                                       MT6336_PMIC_CLK_CKPDN_CON2
#define MT6336_CLK_REG_CK_PDN_MASK                                       0x1
#define MT6336_CLK_REG_CK_PDN_SHIFT                                      6
#define MT6336_CLK_REG_CK_I2C_PDN_ADDR                                   MT6336_PMIC_CLK_CKPDN_CON2
#define MT6336_CLK_REG_CK_I2C_PDN_MASK                                   0x1
#define MT6336_CLK_REG_CK_I2C_PDN_SHIFT                                  7
#define MT6336_CLK_CKPDN_CON2_SET_ADDR                                   MT6336_PMIC_CLK_CKPDN_CON2_SET
#define MT6336_CLK_CKPDN_CON2_SET_MASK                                   0xFF
#define MT6336_CLK_CKPDN_CON2_SET_SHIFT                                  0
#define MT6336_CLK_CKPDN_CON2_CLR_ADDR                                   MT6336_PMIC_CLK_CKPDN_CON2_CLR
#define MT6336_CLK_CKPDN_CON2_CLR_MASK                                   0xFF
#define MT6336_CLK_CKPDN_CON2_CLR_SHIFT                                  0
#define MT6336_CLK_REG_6M_W1C_CK_PDN_ADDR                                MT6336_PMIC_CLK_CKPDN_CON3
#define MT6336_CLK_REG_6M_W1C_CK_PDN_MASK                                0x1
#define MT6336_CLK_REG_6M_W1C_CK_PDN_SHIFT                               0
#define MT6336_CLK_RSTCTL_WDT_ALL_CK_PDN_ADDR                            MT6336_PMIC_CLK_CKPDN_CON3
#define MT6336_CLK_RSTCTL_WDT_ALL_CK_PDN_MASK                            0x1
#define MT6336_CLK_RSTCTL_WDT_ALL_CK_PDN_SHIFT                           1
#define MT6336_CLK_RSTCTL_RST_GLOBAL_CK_PDN_ADDR                         MT6336_PMIC_CLK_CKPDN_CON3
#define MT6336_CLK_RSTCTL_RST_GLOBAL_CK_PDN_MASK                         0x1
#define MT6336_CLK_RSTCTL_RST_GLOBAL_CK_PDN_SHIFT                        2
#define MT6336_CLK_TYPE_C_CC_CK_PDN_ADDR                                 MT6336_PMIC_CLK_CKPDN_CON3
#define MT6336_CLK_TYPE_C_CC_CK_PDN_MASK                                 0x1
#define MT6336_CLK_TYPE_C_CC_CK_PDN_SHIFT                                3
#define MT6336_CLK_TYPE_C_PD_CK_PDN_ADDR                                 MT6336_PMIC_CLK_CKPDN_CON3
#define MT6336_CLK_TYPE_C_PD_CK_PDN_MASK                                 0x1
#define MT6336_CLK_TYPE_C_PD_CK_PDN_SHIFT                                4
#define MT6336_CLK_TYPE_C_CSR_CK_PDN_ADDR                                MT6336_PMIC_CLK_CKPDN_CON3
#define MT6336_CLK_TYPE_C_CSR_CK_PDN_MASK                                0x1
#define MT6336_CLK_TYPE_C_CSR_CK_PDN_SHIFT                               5
#define MT6336_CLK_RSV_PDN_CON3_ADDR                                     MT6336_PMIC_CLK_CKPDN_CON3
#define MT6336_CLK_RSV_PDN_CON3_MASK                                     0x3
#define MT6336_CLK_RSV_PDN_CON3_SHIFT                                    6
#define MT6336_CLK_CKPDN_CON3_SET_ADDR                                   MT6336_PMIC_CLK_CKPDN_CON3_SET
#define MT6336_CLK_CKPDN_CON3_SET_MASK                                   0xFF
#define MT6336_CLK_CKPDN_CON3_SET_SHIFT                                  0
#define MT6336_CLK_CKPDN_CON3_CLR_ADDR                                   MT6336_PMIC_CLK_CKPDN_CON3_CLR
#define MT6336_CLK_CKPDN_CON3_CLR_MASK                                   0xFF
#define MT6336_CLK_CKPDN_CON3_CLR_SHIFT                                  0
#define MT6336_CLK_AUXADC_CK_PDN_HWEN_ADDR                               MT6336_PMIC_CLK_CKPDN_HWEN_CON0
#define MT6336_CLK_AUXADC_CK_PDN_HWEN_MASK                               0x1
#define MT6336_CLK_AUXADC_CK_PDN_HWEN_SHIFT                              0
#define MT6336_CLK_EFUSE_CK_PDN_HWEN_ADDR                                MT6336_PMIC_CLK_CKPDN_HWEN_CON0
#define MT6336_CLK_EFUSE_CK_PDN_HWEN_MASK                                0x1
#define MT6336_CLK_EFUSE_CK_PDN_HWEN_SHIFT                               1
#define MT6336_CLK_RSV_PDN_HWEN_ADDR                                     MT6336_PMIC_CLK_CKPDN_HWEN_CON0
#define MT6336_CLK_RSV_PDN_HWEN_MASK                                     0x3F
#define MT6336_CLK_RSV_PDN_HWEN_SHIFT                                    2
#define MT6336_CLK_CKPDN_HWEN_CON0_SET_ADDR                              MT6336_PMIC_CLK_CKPDN_HWEN_CON0_SET
#define MT6336_CLK_CKPDN_HWEN_CON0_SET_MASK                              0xFF
#define MT6336_CLK_CKPDN_HWEN_CON0_SET_SHIFT                             0
#define MT6336_CLK_CKPDN_HWEN_CON0_CLR_ADDR                              MT6336_PMIC_CLK_CKPDN_HWEN_CON0_CLR
#define MT6336_CLK_CKPDN_HWEN_CON0_CLR_MASK                              0xFF
#define MT6336_CLK_CKPDN_HWEN_CON0_CLR_SHIFT                             0
#define MT6336_CLK_FQMTR_CK_CKSEL_ADDR                                   MT6336_PMIC_CLK_CKSEL_CON0
#define MT6336_CLK_FQMTR_CK_CKSEL_MASK                                   0x7
#define MT6336_CLK_FQMTR_CK_CKSEL_SHIFT                                  0
#define MT6336_CLK_RSV_CKSEL_ADDR                                        MT6336_PMIC_CLK_CKSEL_CON0
#define MT6336_CLK_RSV_CKSEL_MASK                                        0x1F
#define MT6336_CLK_RSV_CKSEL_SHIFT                                       3
#define MT6336_CLK_CKSEL_CON0_SET_ADDR                                   MT6336_PMIC_CLK_CKSEL_CON0_SET
#define MT6336_CLK_CKSEL_CON0_SET_MASK                                   0xFF
#define MT6336_CLK_CKSEL_CON0_SET_SHIFT                                  0
#define MT6336_CLK_CKSEL_CON0_CLR_ADDR                                   MT6336_PMIC_CLK_CKSEL_CON0_CLR
#define MT6336_CLK_CKSEL_CON0_CLR_MASK                                   0xFF
#define MT6336_CLK_CKSEL_CON0_CLR_SHIFT                                  0
#define MT6336_CLK_AUXADC_CK_DIVSEL_ADDR                                 MT6336_PMIC_CLK_CKDIVSEL_CON0
#define MT6336_CLK_AUXADC_CK_DIVSEL_MASK                                 0x3
#define MT6336_CLK_AUXADC_CK_DIVSEL_SHIFT                                0
#define MT6336_CLK_RSV_DIVSEL_ADDR                                       MT6336_PMIC_CLK_CKDIVSEL_CON0
#define MT6336_CLK_RSV_DIVSEL_MASK                                       0x3F
#define MT6336_CLK_RSV_DIVSEL_SHIFT                                      2
#define MT6336_CLK_CKDIVSEL_CON0_SET_ADDR                                MT6336_PMIC_CLK_CKDIVSEL_CON0_SET
#define MT6336_CLK_CKDIVSEL_CON0_SET_MASK                                0xFF
#define MT6336_CLK_CKDIVSEL_CON0_SET_SHIFT                               0
#define MT6336_CLK_CKDIVSEL_CON0_CLR_ADDR                                MT6336_PMIC_CLK_CKDIVSEL_CON0_CLR
#define MT6336_CLK_CKDIVSEL_CON0_CLR_MASK                                0xFF
#define MT6336_CLK_CKDIVSEL_CON0_CLR_SHIFT                               0
#define MT6336_CLK_TOP_AO_75K_CK_TSTSEL_ADDR                             MT6336_PMIC_CLK_CKTSTSEL_CON0
#define MT6336_CLK_TOP_AO_75K_CK_TSTSEL_MASK                             0x1
#define MT6336_CLK_TOP_AO_75K_CK_TSTSEL_SHIFT                            0
#define MT6336_CLK_AUXADC_CK_TSTSEL_ADDR                                 MT6336_PMIC_CLK_CKTSTSEL_CON0
#define MT6336_CLK_AUXADC_CK_TSTSEL_MASK                                 0x1
#define MT6336_CLK_AUXADC_CK_TSTSEL_SHIFT                                1
#define MT6336_CLK_DRV_CHRIND_CK_TSTSEL_ADDR                             MT6336_PMIC_CLK_CKTSTSEL_CON0
#define MT6336_CLK_DRV_CHRIND_CK_TSTSEL_MASK                             0x1
#define MT6336_CLK_DRV_CHRIND_CK_TSTSEL_SHIFT                            2
#define MT6336_CLK_FQMTR_CK_TSTSEL_ADDR                                  MT6336_PMIC_CLK_CKTSTSEL_CON0
#define MT6336_CLK_FQMTR_CK_TSTSEL_MASK                                  0x1
#define MT6336_CLK_FQMTR_CK_TSTSEL_SHIFT                                 3
#define MT6336_CLK_REG_CK_TSTSEL_ADDR                                    MT6336_PMIC_CLK_CKTSTSEL_CON0
#define MT6336_CLK_REG_CK_TSTSEL_MASK                                    0x1
#define MT6336_CLK_REG_CK_TSTSEL_SHIFT                                   4
#define MT6336_CLK_REG_CK_I2C_TSTSEL_ADDR                                MT6336_PMIC_CLK_CKTSTSEL_CON0
#define MT6336_CLK_REG_CK_I2C_TSTSEL_MASK                                0x1
#define MT6336_CLK_REG_CK_I2C_TSTSEL_SHIFT                               5
#define MT6336_CLK_RSV_TSTSEL_ADDR                                       MT6336_PMIC_CLK_CKTSTSEL_CON0
#define MT6336_CLK_RSV_TSTSEL_MASK                                       0x3
#define MT6336_CLK_RSV_TSTSEL_SHIFT                                      6
#define MT6336_CLK_SWCHR_6M_LOWQ_PDN_DIS_ADDR                            MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0
#define MT6336_CLK_SWCHR_6M_LOWQ_PDN_DIS_MASK                            0x1
#define MT6336_CLK_SWCHR_6M_LOWQ_PDN_DIS_SHIFT                           0
#define MT6336_CLK_SWCHR_3M_LOWQ_PDN_DIS_ADDR                            MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0
#define MT6336_CLK_SWCHR_3M_LOWQ_PDN_DIS_MASK                            0x1
#define MT6336_CLK_SWCHR_3M_LOWQ_PDN_DIS_SHIFT                           1
#define MT6336_CLK_SWCHR_2M_LOWQ_PDN_DIS_ADDR                            MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0
#define MT6336_CLK_SWCHR_2M_LOWQ_PDN_DIS_MASK                            0x1
#define MT6336_CLK_SWCHR_2M_LOWQ_PDN_DIS_SHIFT                           2
#define MT6336_CLK_SWCHR_300K_LOWQ_PDN_DIS_ADDR                          MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0
#define MT6336_CLK_SWCHR_300K_LOWQ_PDN_DIS_MASK                          0x1
#define MT6336_CLK_SWCHR_300K_LOWQ_PDN_DIS_SHIFT                         3
#define MT6336_CLK_SWCHR_75K_LOWQ_PDN_DIS_ADDR                           MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0
#define MT6336_CLK_SWCHR_75K_LOWQ_PDN_DIS_MASK                           0x1
#define MT6336_CLK_SWCHR_75K_LOWQ_PDN_DIS_SHIFT                          4
#define MT6336_CLK_SWCHR_1K_LOWQ_PDN_DIS_ADDR                            MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0
#define MT6336_CLK_SWCHR_1K_LOWQ_PDN_DIS_MASK                            0x1
#define MT6336_CLK_SWCHR_1K_LOWQ_PDN_DIS_SHIFT                           5
#define MT6336_CLK_TYPE_C_CC_LOWQ_PDN_DIS_ADDR                           MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0
#define MT6336_CLK_TYPE_C_CC_LOWQ_PDN_DIS_MASK                           0x1
#define MT6336_CLK_TYPE_C_CC_LOWQ_PDN_DIS_SHIFT                          6
#define MT6336_CLK_TYPE_C_PD_LOWQ_PDN_DIS_ADDR                           MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0
#define MT6336_CLK_TYPE_C_PD_LOWQ_PDN_DIS_MASK                           0x1
#define MT6336_CLK_TYPE_C_PD_LOWQ_PDN_DIS_SHIFT                          7
#define MT6336_CLK_LOWQ_PDN_DIS_CON0_SET_ADDR                            MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0_SET
#define MT6336_CLK_LOWQ_PDN_DIS_CON0_SET_MASK                            0xFF
#define MT6336_CLK_LOWQ_PDN_DIS_CON0_SET_SHIFT                           0
#define MT6336_CLK_LOWQ_PDN_DIS_CON0_CLR_ADDR                            MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON0_CLR
#define MT6336_CLK_LOWQ_PDN_DIS_CON0_CLR_MASK                            0xFF
#define MT6336_CLK_LOWQ_PDN_DIS_CON0_CLR_SHIFT                           0
#define MT6336_CLK_TYPE_C_CSR_LOWQ_PDN_DIS_ADDR                          MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1
#define MT6336_CLK_TYPE_C_CSR_LOWQ_PDN_DIS_MASK                          0x1
#define MT6336_CLK_TYPE_C_CSR_LOWQ_PDN_DIS_SHIFT                         0
#define MT6336_CLK_AUXADC_LOWQ_PDN_DIS_ADDR                              MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1
#define MT6336_CLK_AUXADC_LOWQ_PDN_DIS_MASK                              0x1
#define MT6336_CLK_AUXADC_LOWQ_PDN_DIS_SHIFT                             1
#define MT6336_CLK_AUXADC_300K_LOWQ_PDN_DIS_ADDR                         MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1
#define MT6336_CLK_AUXADC_300K_LOWQ_PDN_DIS_MASK                         0x1
#define MT6336_CLK_AUXADC_300K_LOWQ_PDN_DIS_SHIFT                        2
#define MT6336_CLK_DRV_CHRIND_LOWQ_PDN_DIS_ADDR                          MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1
#define MT6336_CLK_DRV_CHRIND_LOWQ_PDN_DIS_MASK                          0x1
#define MT6336_CLK_DRV_CHRIND_LOWQ_PDN_DIS_SHIFT                         3
#define MT6336_CLK_DRV_75K_LOWQ_PDN_DIS_ADDR                             MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1
#define MT6336_CLK_DRV_75K_LOWQ_PDN_DIS_MASK                             0x1
#define MT6336_CLK_DRV_75K_LOWQ_PDN_DIS_SHIFT                            4
#define MT6336_CLK_FQMTR_LOWQ_PDN_DIS_ADDR                               MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1
#define MT6336_CLK_FQMTR_LOWQ_PDN_DIS_MASK                               0x1
#define MT6336_CLK_FQMTR_LOWQ_PDN_DIS_SHIFT                              5
#define MT6336_CLK_FQMTR_26M_LOWQ_PDN_DIS_ADDR                           MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1
#define MT6336_CLK_FQMTR_26M_LOWQ_PDN_DIS_MASK                           0x1
#define MT6336_CLK_FQMTR_26M_LOWQ_PDN_DIS_SHIFT                          6
#define MT6336_CLK_FQMTR_75K_LOWQ_PDN_DIS_ADDR                           MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1
#define MT6336_CLK_FQMTR_75K_LOWQ_PDN_DIS_MASK                           0x1
#define MT6336_CLK_FQMTR_75K_LOWQ_PDN_DIS_SHIFT                          7
#define MT6336_CLK_LOWQ_PDN_DIS_CON1_SET_ADDR                            MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1_SET
#define MT6336_CLK_LOWQ_PDN_DIS_CON1_SET_MASK                            0xFF
#define MT6336_CLK_LOWQ_PDN_DIS_CON1_SET_SHIFT                           0
#define MT6336_CLK_LOWQ_PDN_DIS_CON1_CLR_ADDR                            MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON1_CLR
#define MT6336_CLK_LOWQ_PDN_DIS_CON1_CLR_MASK                            0xFF
#define MT6336_CLK_LOWQ_PDN_DIS_CON1_CLR_SHIFT                           0
#define MT6336_CLK_REG_LOWQ_PDN_DIS_ADDR                                 MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2
#define MT6336_CLK_REG_LOWQ_PDN_DIS_MASK                                 0x1
#define MT6336_CLK_REG_LOWQ_PDN_DIS_SHIFT                                0
#define MT6336_CLK_REG_6M_W1C_LOWQ_PDN_DIS_ADDR                          MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2
#define MT6336_CLK_REG_6M_W1C_LOWQ_PDN_DIS_MASK                          0x1
#define MT6336_CLK_REG_6M_W1C_LOWQ_PDN_DIS_SHIFT                         1
#define MT6336_CLK_INTRP_LOWQ_PDN_DIS_ADDR                               MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2
#define MT6336_CLK_INTRP_LOWQ_PDN_DIS_MASK                               0x1
#define MT6336_CLK_INTRP_LOWQ_PDN_DIS_SHIFT                              2
#define MT6336_CLK_TRIM_75K_LOWQ_PDN_DIS_ADDR                            MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2
#define MT6336_CLK_TRIM_75K_LOWQ_PDN_DIS_MASK                            0x1
#define MT6336_CLK_TRIM_75K_LOWQ_PDN_DIS_SHIFT                           3
#define MT6336_CLK_EFUSE_LOWQ_PDN_DIS_ADDR                               MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2
#define MT6336_CLK_EFUSE_LOWQ_PDN_DIS_MASK                               0x1
#define MT6336_CLK_EFUSE_LOWQ_PDN_DIS_SHIFT                              4
#define MT6336_CLK_RSTCTL_RST_GLOBAL_LOWQ_PDN_DIS_ADDR                   MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2
#define MT6336_CLK_RSTCTL_RST_GLOBAL_LOWQ_PDN_DIS_MASK                   0x1
#define MT6336_CLK_RSTCTL_RST_GLOBAL_LOWQ_PDN_DIS_SHIFT                  5
#define MT6336_CLK_RSV_LOWQ_PDN_DIS_ADDR                                 MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2
#define MT6336_CLK_RSV_LOWQ_PDN_DIS_MASK                                 0x3
#define MT6336_CLK_RSV_LOWQ_PDN_DIS_SHIFT                                6
#define MT6336_CLK_LOWQ_PDN_DIS_CON2_SET_ADDR                            MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2_SET
#define MT6336_CLK_LOWQ_PDN_DIS_CON2_SET_MASK                            0xFF
#define MT6336_CLK_LOWQ_PDN_DIS_CON2_SET_SHIFT                           0
#define MT6336_CLK_LOWQ_PDN_DIS_CON2_CLR_ADDR                            MT6336_PMIC_CLK_LOWQ_PDN_DIS_CON2_CLR
#define MT6336_CLK_LOWQ_PDN_DIS_CON2_CLR_MASK                            0xFF
#define MT6336_CLK_LOWQ_PDN_DIS_CON2_CLR_SHIFT                           0
#define MT6336_CLOCK_RSV0_ADDR                                           MT6336_PMIC_CLOCK_RSV0
#define MT6336_CLOCK_RSV0_MASK                                           0xFF
#define MT6336_CLOCK_RSV0_SHIFT                                          0
#define MT6336_CLOCK_RSV1_ADDR                                           MT6336_PMIC_CLOCK_RSV1
#define MT6336_CLOCK_RSV1_MASK                                           0xFF
#define MT6336_CLOCK_RSV1_SHIFT                                          0
#define MT6336_CLOCK_RSV2_ADDR                                           MT6336_PMIC_CLOCK_RSV2
#define MT6336_CLOCK_RSV2_MASK                                           0xFF
#define MT6336_CLOCK_RSV2_SHIFT                                          0
#define MT6336_RG_VPWRIN_RST_ADDR                                        MT6336_PMIC_TOP_RST_CON0
#define MT6336_RG_VPWRIN_RST_MASK                                        0x1
#define MT6336_RG_VPWRIN_RST_SHIFT                                       0
#define MT6336_RG_EFUSE_MAN_RST_ADDR                                     MT6336_PMIC_TOP_RST_CON0
#define MT6336_RG_EFUSE_MAN_RST_MASK                                     0x1
#define MT6336_RG_EFUSE_MAN_RST_SHIFT                                    1
#define MT6336_RG_AUXADC_RST_ADDR                                        MT6336_PMIC_TOP_RST_CON0
#define MT6336_RG_AUXADC_RST_MASK                                        0x1
#define MT6336_RG_AUXADC_RST_SHIFT                                       2
#define MT6336_RG_AUXADC_REG_RST_ADDR                                    MT6336_PMIC_TOP_RST_CON0
#define MT6336_RG_AUXADC_REG_RST_MASK                                    0x1
#define MT6336_RG_AUXADC_REG_RST_SHIFT                                   3
#define MT6336_RG_CLK_TRIM_RST_ADDR                                      MT6336_PMIC_TOP_RST_CON0
#define MT6336_RG_CLK_TRIM_RST_MASK                                      0x1
#define MT6336_RG_CLK_TRIM_RST_SHIFT                                     4
#define MT6336_RG_CLKCTL_RST_ADDR                                        MT6336_PMIC_TOP_RST_CON0
#define MT6336_RG_CLKCTL_RST_MASK                                        0x1
#define MT6336_RG_CLKCTL_RST_SHIFT                                       5
#define MT6336_RG_DRIVER_RST_ADDR                                        MT6336_PMIC_TOP_RST_CON0
#define MT6336_RG_DRIVER_RST_MASK                                        0x1
#define MT6336_RG_DRIVER_RST_SHIFT                                       6
#define MT6336_RG_FQMTR_RST_ADDR                                         MT6336_PMIC_TOP_RST_CON0
#define MT6336_RG_FQMTR_RST_MASK                                         0x1
#define MT6336_RG_FQMTR_RST_SHIFT                                        7
#define MT6336_TOP_RST_CON0_SET_ADDR                                     MT6336_PMIC_TOP_RST_CON0_SET
#define MT6336_TOP_RST_CON0_SET_MASK                                     0xFF
#define MT6336_TOP_RST_CON0_SET_SHIFT                                    0
#define MT6336_TOP_RST_CON0_CLR_ADDR                                     MT6336_PMIC_TOP_RST_CON0_CLR
#define MT6336_TOP_RST_CON0_CLR_MASK                                     0xFF
#define MT6336_TOP_RST_CON0_CLR_SHIFT                                    0
#define MT6336_RG_BANK0_REG_RST_ADDR                                     MT6336_PMIC_TOP_RST_CON1
#define MT6336_RG_BANK0_REG_RST_MASK                                     0x1
#define MT6336_RG_BANK0_REG_RST_SHIFT                                    0
#define MT6336_RG_BANK1_REG_RST_ADDR                                     MT6336_PMIC_TOP_RST_CON1
#define MT6336_RG_BANK1_REG_RST_MASK                                     0x1
#define MT6336_RG_BANK1_REG_RST_SHIFT                                    1
#define MT6336_RG_BANK2_REG_RST_ADDR                                     MT6336_PMIC_TOP_RST_CON1
#define MT6336_RG_BANK2_REG_RST_MASK                                     0x1
#define MT6336_RG_BANK2_REG_RST_SHIFT                                    2
#define MT6336_RG_BANK3_REG_RST_ADDR                                     MT6336_PMIC_TOP_RST_CON1
#define MT6336_RG_BANK3_REG_RST_MASK                                     0x1
#define MT6336_RG_BANK3_REG_RST_SHIFT                                    3
#define MT6336_RG_BANK4_REG_RST_ADDR                                     MT6336_PMIC_TOP_RST_CON1
#define MT6336_RG_BANK4_REG_RST_MASK                                     0x1
#define MT6336_RG_BANK4_REG_RST_SHIFT                                    4
#define MT6336_RG_BANK5_REG_RST_ADDR                                     MT6336_PMIC_TOP_RST_CON1
#define MT6336_RG_BANK5_REG_RST_MASK                                     0x1
#define MT6336_RG_BANK5_REG_RST_SHIFT                                    5
#define MT6336_RG_BANK6_REG_RST_ADDR                                     MT6336_PMIC_TOP_RST_CON1
#define MT6336_RG_BANK6_REG_RST_MASK                                     0x1
#define MT6336_RG_BANK6_REG_RST_SHIFT                                    6
#define MT6336_RG_GPIO_REG_RST_ADDR                                      MT6336_PMIC_TOP_RST_CON1
#define MT6336_RG_GPIO_REG_RST_MASK                                      0x1
#define MT6336_RG_GPIO_REG_RST_SHIFT                                     7
#define MT6336_TOP_RST_CON1_SET_ADDR                                     MT6336_PMIC_TOP_RST_CON1_SET
#define MT6336_TOP_RST_CON1_SET_MASK                                     0xFF
#define MT6336_TOP_RST_CON1_SET_SHIFT                                    0
#define MT6336_TOP_RST_CON1_CLR_ADDR                                     MT6336_PMIC_TOP_RST_CON1_CLR
#define MT6336_TOP_RST_CON1_CLR_MASK                                     0xFF
#define MT6336_TOP_RST_CON1_CLR_SHIFT                                    0
#define MT6336_RG_SWCHR_RST_ADDR                                         MT6336_PMIC_TOP_RST_CON2
#define MT6336_RG_SWCHR_RST_MASK                                         0x1
#define MT6336_RG_SWCHR_RST_SHIFT                                        0
#define MT6336_RG_SWCHR_OTG_REG_RST_ADDR                                 MT6336_PMIC_TOP_RST_CON2
#define MT6336_RG_SWCHR_OTG_REG_RST_MASK                                 0x1
#define MT6336_RG_SWCHR_OTG_REG_RST_SHIFT                                1
#define MT6336_RG_SWCHR_SHIP_REG_RST_ADDR                                MT6336_PMIC_TOP_RST_CON2
#define MT6336_RG_SWCHR_SHIP_REG_RST_MASK                                0x1
#define MT6336_RG_SWCHR_SHIP_REG_RST_SHIFT                               2
#define MT6336_RG_SWCHR_LOWQ_REG_RST_ADDR                                MT6336_PMIC_TOP_RST_CON2
#define MT6336_RG_SWCHR_LOWQ_REG_RST_MASK                                0x1
#define MT6336_RG_SWCHR_LOWQ_REG_RST_SHIFT                               3
#define MT6336_RG_SWCHR_PLUGIN_REG_RST_ADDR                              MT6336_PMIC_TOP_RST_CON2
#define MT6336_RG_SWCHR_PLUGIN_REG_RST_MASK                              0x1
#define MT6336_RG_SWCHR_PLUGIN_REG_RST_SHIFT                             4
#define MT6336_RG_SWCHR_PLUGOUT_REG_RST_ADDR                             MT6336_PMIC_TOP_RST_CON2
#define MT6336_RG_SWCHR_PLUGOUT_REG_RST_MASK                             0x1
#define MT6336_RG_SWCHR_PLUGOUT_REG_RST_SHIFT                            5
#define MT6336_TOP_RST_CON2_RSV_ADDR                                     MT6336_PMIC_TOP_RST_CON2
#define MT6336_TOP_RST_CON2_RSV_MASK                                     0x1
#define MT6336_TOP_RST_CON2_RSV_SHIFT                                    6
#define MT6336_RG_SWCHR_WDT_CONFIG_ADDR                                  MT6336_PMIC_TOP_RST_CON2
#define MT6336_RG_SWCHR_WDT_CONFIG_MASK                                  0x1
#define MT6336_RG_SWCHR_WDT_CONFIG_SHIFT                                 7
#define MT6336_TOP_RST_CON2_SET_ADDR                                     MT6336_PMIC_TOP_RST_CON2_SET
#define MT6336_TOP_RST_CON2_SET_MASK                                     0xFF
#define MT6336_TOP_RST_CON2_SET_SHIFT                                    0
#define MT6336_TOP_RST_CON2_CLR_ADDR                                     MT6336_PMIC_TOP_RST_CON2_CLR
#define MT6336_TOP_RST_CON2_CLR_MASK                                     0xFF
#define MT6336_TOP_RST_CON2_CLR_SHIFT                                    0
#define MT6336_RG_TESTCTL_RST_ADDR                                       MT6336_PMIC_TOP_RST_CON3
#define MT6336_RG_TESTCTL_RST_MASK                                       0x1
#define MT6336_RG_TESTCTL_RST_SHIFT                                      0
#define MT6336_RG_INTCTL_RST_ADDR                                        MT6336_PMIC_TOP_RST_CON3
#define MT6336_RG_INTCTL_RST_MASK                                        0x1
#define MT6336_RG_INTCTL_RST_SHIFT                                       1
#define MT6336_RG_IO_RST_ADDR                                            MT6336_PMIC_TOP_RST_CON3
#define MT6336_RG_IO_RST_MASK                                            0x1
#define MT6336_RG_IO_RST_SHIFT                                           2
#define MT6336_RG_I2C_RST_ADDR                                           MT6336_PMIC_TOP_RST_CON3
#define MT6336_RG_I2C_RST_MASK                                           0x1
#define MT6336_RG_I2C_RST_SHIFT                                          3
#define MT6336_RG_TYPE_C_CC_RST_ADDR                                     MT6336_PMIC_TOP_RST_CON3
#define MT6336_RG_TYPE_C_CC_RST_MASK                                     0x1
#define MT6336_RG_TYPE_C_CC_RST_SHIFT                                    4
#define MT6336_RG_TYPE_C_PD_RST_ADDR                                     MT6336_PMIC_TOP_RST_CON3
#define MT6336_RG_TYPE_C_PD_RST_MASK                                     0x1
#define MT6336_RG_TYPE_C_PD_RST_SHIFT                                    5
#define MT6336_TOP_RST_CON3_RSV_ADDR                                     MT6336_PMIC_TOP_RST_CON3
#define MT6336_TOP_RST_CON3_RSV_MASK                                     0x3
#define MT6336_TOP_RST_CON3_RSV_SHIFT                                    6
#define MT6336_TOP_RST_MISC_SET_ADDR                                     MT6336_PMIC_TOP_RST_CON3_SET
#define MT6336_TOP_RST_MISC_SET_MASK                                     0xFF
#define MT6336_TOP_RST_MISC_SET_SHIFT                                    0
#define MT6336_TOP_RST_MISC_CLR_ADDR                                     MT6336_PMIC_TOP_RST_CON3_CLR
#define MT6336_TOP_RST_MISC_CLR_MASK                                     0xFF
#define MT6336_TOP_RST_MISC_CLR_SHIFT                                    0
#define MT6336_VPWRIN_RSTB_STATUS_ADDR                                   MT6336_PMIC_TOP_RST_STATUS
#define MT6336_VPWRIN_RSTB_STATUS_MASK                                   0x1
#define MT6336_VPWRIN_RSTB_STATUS_SHIFT                                  0
#define MT6336_SWCHR_RSTB_STATUS_ADDR                                    MT6336_PMIC_TOP_RST_STATUS
#define MT6336_SWCHR_RSTB_STATUS_MASK                                    0x1
#define MT6336_SWCHR_RSTB_STATUS_SHIFT                                   1
#define MT6336_SWCHR_OTG_REG_RSTB_STATUS_ADDR                            MT6336_PMIC_TOP_RST_STATUS
#define MT6336_SWCHR_OTG_REG_RSTB_STATUS_MASK                            0x1
#define MT6336_SWCHR_OTG_REG_RSTB_STATUS_SHIFT                           2
#define MT6336_SWCHR_SHIP_REG_RSTB_STATUS_ADDR                           MT6336_PMIC_TOP_RST_STATUS
#define MT6336_SWCHR_SHIP_REG_RSTB_STATUS_MASK                           0x1
#define MT6336_SWCHR_SHIP_REG_RSTB_STATUS_SHIFT                          3
#define MT6336_SWCHR_LOWQ_REG_RSTB_STATUS_ADDR                           MT6336_PMIC_TOP_RST_STATUS
#define MT6336_SWCHR_LOWQ_REG_RSTB_STATUS_MASK                           0x1
#define MT6336_SWCHR_LOWQ_REG_RSTB_STATUS_SHIFT                          4
#define MT6336_TOP_RST_STATUS_RSV_ADDR                                   MT6336_PMIC_TOP_RST_STATUS
#define MT6336_TOP_RST_STATUS_RSV_MASK                                   0x7
#define MT6336_TOP_RST_STATUS_RSV_SHIFT                                  5
#define MT6336_TOP_RST_STATUS_SET_ADDR                                   MT6336_PMIC_TOP_RST_STATUS_SET
#define MT6336_TOP_RST_STATUS_SET_MASK                                   0xFF
#define MT6336_TOP_RST_STATUS_SET_SHIFT                                  0
#define MT6336_TOP_RST_STATUS_CLR_ADDR                                   MT6336_PMIC_TOP_RST_STATUS_CLR
#define MT6336_TOP_RST_STATUS_CLR_MASK                                   0xFF
#define MT6336_TOP_RST_STATUS_CLR_SHIFT                                  0
#define MT6336_TOP_RST_RSV0_ADDR                                         MT6336_PMIC_TOP_RST_RSV0
#define MT6336_TOP_RST_RSV0_MASK                                         0xFF
#define MT6336_TOP_RST_RSV0_SHIFT                                        0
#define MT6336_TOP_RST_RSV1_ADDR                                         MT6336_PMIC_TOP_RST_RSV1
#define MT6336_TOP_RST_RSV1_MASK                                         0xFF
#define MT6336_TOP_RST_RSV1_SHIFT                                        0
#define MT6336_TOP_RST_RSV2_ADDR                                         MT6336_PMIC_TOP_RST_RSV2
#define MT6336_TOP_RST_RSV2_MASK                                         0xFF
#define MT6336_TOP_RST_RSV2_SHIFT                                        0
#define MT6336_TOP_RST_RSV3_ADDR                                         MT6336_PMIC_TOP_RST_RSV3
#define MT6336_TOP_RST_RSV3_MASK                                         0xFF
#define MT6336_TOP_RST_RSV3_SHIFT                                        0
#define MT6336_RG_INT_EN_CHR_VBUS_PLUGIN_ADDR                            MT6336_PMIC_INT_CON0
#define MT6336_RG_INT_EN_CHR_VBUS_PLUGIN_MASK                            0x1
#define MT6336_RG_INT_EN_CHR_VBUS_PLUGIN_SHIFT                           0
#define MT6336_RG_INT_EN_CHR_VBUS_PLUGOUT_ADDR                           MT6336_PMIC_INT_CON0
#define MT6336_RG_INT_EN_CHR_VBUS_PLUGOUT_MASK                           0x1
#define MT6336_RG_INT_EN_CHR_VBUS_PLUGOUT_SHIFT                          1
#define MT6336_RG_INT_EN_STATE_BUCK_BACKGROUND_ADDR                      MT6336_PMIC_INT_CON0
#define MT6336_RG_INT_EN_STATE_BUCK_BACKGROUND_MASK                      0x1
#define MT6336_RG_INT_EN_STATE_BUCK_BACKGROUND_SHIFT                     2
#define MT6336_RG_INT_EN_STATE_BUCK_EOC_ADDR                             MT6336_PMIC_INT_CON0
#define MT6336_RG_INT_EN_STATE_BUCK_EOC_MASK                             0x1
#define MT6336_RG_INT_EN_STATE_BUCK_EOC_SHIFT                            3
#define MT6336_RG_INT_EN_STATE_BUCK_PRECC0_ADDR                          MT6336_PMIC_INT_CON0
#define MT6336_RG_INT_EN_STATE_BUCK_PRECC0_MASK                          0x1
#define MT6336_RG_INT_EN_STATE_BUCK_PRECC0_SHIFT                         4
#define MT6336_RG_INT_EN_STATE_BUCK_PRECC1_ADDR                          MT6336_PMIC_INT_CON0
#define MT6336_RG_INT_EN_STATE_BUCK_PRECC1_MASK                          0x1
#define MT6336_RG_INT_EN_STATE_BUCK_PRECC1_SHIFT                         5
#define MT6336_RG_INT_EN_STATE_BUCK_FASTCC_ADDR                          MT6336_PMIC_INT_CON0
#define MT6336_RG_INT_EN_STATE_BUCK_FASTCC_MASK                          0x1
#define MT6336_RG_INT_EN_STATE_BUCK_FASTCC_SHIFT                         6
#define MT6336_RG_INT_EN_CHR_WEAKBUS_ADDR                                MT6336_PMIC_INT_CON0
#define MT6336_RG_INT_EN_CHR_WEAKBUS_MASK                                0x1
#define MT6336_RG_INT_EN_CHR_WEAKBUS_SHIFT                               7
#define MT6336_INT_CON0_SET_ADDR                                         MT6336_PMIC_INT_CON0_SET
#define MT6336_INT_CON0_SET_MASK                                         0xFF
#define MT6336_INT_CON0_SET_SHIFT                                        0
#define MT6336_INT_CON0_CLR_ADDR                                         MT6336_PMIC_INT_CON0_CLR
#define MT6336_INT_CON0_CLR_MASK                                         0xFF
#define MT6336_INT_CON0_CLR_SHIFT                                        0
#define MT6336_RG_INT_EN_CHR_SYS_OVP_ADDR                                MT6336_PMIC_INT_CON1
#define MT6336_RG_INT_EN_CHR_SYS_OVP_MASK                                0x1
#define MT6336_RG_INT_EN_CHR_SYS_OVP_SHIFT                               0
#define MT6336_RG_INT_EN_CHR_BAT_OVP_ADDR                                MT6336_PMIC_INT_CON1
#define MT6336_RG_INT_EN_CHR_BAT_OVP_MASK                                0x1
#define MT6336_RG_INT_EN_CHR_BAT_OVP_SHIFT                               1
#define MT6336_RG_INT_EN_CHR_VBUS_OVP_ADDR                               MT6336_PMIC_INT_CON1
#define MT6336_RG_INT_EN_CHR_VBUS_OVP_MASK                               0x1
#define MT6336_RG_INT_EN_CHR_VBUS_OVP_SHIFT                              2
#define MT6336_RG_INT_EN_CHR_VBUS_UVLO_ADDR                              MT6336_PMIC_INT_CON1
#define MT6336_RG_INT_EN_CHR_VBUS_UVLO_MASK                              0x1
#define MT6336_RG_INT_EN_CHR_VBUS_UVLO_SHIFT                             3
#define MT6336_RG_INT_EN_CHR_ICHR_ITERM_ADDR                             MT6336_PMIC_INT_CON1
#define MT6336_RG_INT_EN_CHR_ICHR_ITERM_MASK                             0x1
#define MT6336_RG_INT_EN_CHR_ICHR_ITERM_SHIFT                            4
#define MT6336_RG_INT_EN_CHIP_TEMP_OVERHEAT_ADDR                         MT6336_PMIC_INT_CON1
#define MT6336_RG_INT_EN_CHIP_TEMP_OVERHEAT_MASK                         0x1
#define MT6336_RG_INT_EN_CHIP_TEMP_OVERHEAT_SHIFT                        5
#define MT6336_RG_INT_EN_CHIP_MBATPP_DIS_OC_DIG_ADDR                     MT6336_PMIC_INT_CON1
#define MT6336_RG_INT_EN_CHIP_MBATPP_DIS_OC_DIG_MASK                     0x1
#define MT6336_RG_INT_EN_CHIP_MBATPP_DIS_OC_DIG_SHIFT                    6
#define MT6336_RG_INT_EN_OTG_BVALID_ADDR                                 MT6336_PMIC_INT_CON1
#define MT6336_RG_INT_EN_OTG_BVALID_MASK                                 0x1
#define MT6336_RG_INT_EN_OTG_BVALID_SHIFT                                7
#define MT6336_INT_CON1_SET_ADDR                                         MT6336_PMIC_INT_CON1_SET
#define MT6336_INT_CON1_SET_MASK                                         0xFF
#define MT6336_INT_CON1_SET_SHIFT                                        0
#define MT6336_INT_CON1_CLR_ADDR                                         MT6336_PMIC_INT_CON1_CLR
#define MT6336_INT_CON1_CLR_MASK                                         0xFF
#define MT6336_INT_CON1_CLR_SHIFT                                        0
#define MT6336_RG_INT_EN_OTG_VM_UVLO_ADDR                                MT6336_PMIC_INT_CON2
#define MT6336_RG_INT_EN_OTG_VM_UVLO_MASK                                0x1
#define MT6336_RG_INT_EN_OTG_VM_UVLO_SHIFT                               0
#define MT6336_RG_INT_EN_OTG_VM_OVP_ADDR                                 MT6336_PMIC_INT_CON2
#define MT6336_RG_INT_EN_OTG_VM_OVP_MASK                                 0x1
#define MT6336_RG_INT_EN_OTG_VM_OVP_SHIFT                                1
#define MT6336_RG_INT_EN_OTG_VBAT_UVLO_ADDR                              MT6336_PMIC_INT_CON2
#define MT6336_RG_INT_EN_OTG_VBAT_UVLO_MASK                              0x1
#define MT6336_RG_INT_EN_OTG_VBAT_UVLO_SHIFT                             2
#define MT6336_RG_INT_EN_OTG_VM_OLP_ADDR                                 MT6336_PMIC_INT_CON2
#define MT6336_RG_INT_EN_OTG_VM_OLP_MASK                                 0x1
#define MT6336_RG_INT_EN_OTG_VM_OLP_SHIFT                                3
#define MT6336_RG_INT_EN_FLASH_VFLA_UVLO_ADDR                            MT6336_PMIC_INT_CON2
#define MT6336_RG_INT_EN_FLASH_VFLA_UVLO_MASK                            0x1
#define MT6336_RG_INT_EN_FLASH_VFLA_UVLO_SHIFT                           4
#define MT6336_RG_INT_EN_FLASH_VFLA_OVP_ADDR                             MT6336_PMIC_INT_CON2
#define MT6336_RG_INT_EN_FLASH_VFLA_OVP_MASK                             0x1
#define MT6336_RG_INT_EN_FLASH_VFLA_OVP_SHIFT                            5
#define MT6336_RG_INT_EN_LED1_SHORT_ADDR                                 MT6336_PMIC_INT_CON2
#define MT6336_RG_INT_EN_LED1_SHORT_MASK                                 0x1
#define MT6336_RG_INT_EN_LED1_SHORT_SHIFT                                6
#define MT6336_RG_INT_EN_LED1_OPEN_ADDR                                  MT6336_PMIC_INT_CON2
#define MT6336_RG_INT_EN_LED1_OPEN_MASK                                  0x1
#define MT6336_RG_INT_EN_LED1_OPEN_SHIFT                                 7
#define MT6336_INT_CON2_SET_ADDR                                         MT6336_PMIC_INT_CON2_SET
#define MT6336_INT_CON2_SET_MASK                                         0xFF
#define MT6336_INT_CON2_SET_SHIFT                                        0
#define MT6336_INT_CON2_CLR_ADDR                                         MT6336_PMIC_INT_CON2_CLR
#define MT6336_INT_CON2_CLR_MASK                                         0xFF
#define MT6336_INT_CON2_CLR_SHIFT                                        0
#define MT6336_RG_INT_EN_LED2_SHORT_ADDR                                 MT6336_PMIC_INT_CON3
#define MT6336_RG_INT_EN_LED2_SHORT_MASK                                 0x1
#define MT6336_RG_INT_EN_LED2_SHORT_SHIFT                                0
#define MT6336_RG_INT_EN_LED2_OPEN_ADDR                                  MT6336_PMIC_INT_CON3
#define MT6336_RG_INT_EN_LED2_OPEN_MASK                                  0x1
#define MT6336_RG_INT_EN_LED2_OPEN_SHIFT                                 1
#define MT6336_RG_INT_EN_FLASH_TIMEOUT_ADDR                              MT6336_PMIC_INT_CON3
#define MT6336_RG_INT_EN_FLASH_TIMEOUT_MASK                              0x1
#define MT6336_RG_INT_EN_FLASH_TIMEOUT_SHIFT                             2
#define MT6336_RG_INT_EN_TORCH_TIMEOUT_ADDR                              MT6336_PMIC_INT_CON3
#define MT6336_RG_INT_EN_TORCH_TIMEOUT_MASK                              0x1
#define MT6336_RG_INT_EN_TORCH_TIMEOUT_SHIFT                             3
#define MT6336_RG_INT_EN_DD_VBUS_IN_VALID_ADDR                           MT6336_PMIC_INT_CON3
#define MT6336_RG_INT_EN_DD_VBUS_IN_VALID_MASK                           0x1
#define MT6336_RG_INT_EN_DD_VBUS_IN_VALID_SHIFT                          4
#define MT6336_RG_INT_EN_WDT_TIMEOUT_ADDR                                MT6336_PMIC_INT_CON3
#define MT6336_RG_INT_EN_WDT_TIMEOUT_MASK                                0x1
#define MT6336_RG_INT_EN_WDT_TIMEOUT_SHIFT                               5
#define MT6336_RG_INT_EN_SAFETY_TIMEOUT_ADDR                             MT6336_PMIC_INT_CON3
#define MT6336_RG_INT_EN_SAFETY_TIMEOUT_MASK                             0x1
#define MT6336_RG_INT_EN_SAFETY_TIMEOUT_SHIFT                            6
#define MT6336_RG_INT_EN_CHR_AICC_DONE_ADDR                              MT6336_PMIC_INT_CON3
#define MT6336_RG_INT_EN_CHR_AICC_DONE_MASK                              0x1
#define MT6336_RG_INT_EN_CHR_AICC_DONE_SHIFT                             7
#define MT6336_INT_CON3_SET_ADDR                                         MT6336_PMIC_INT_CON3_SET
#define MT6336_INT_CON3_SET_MASK                                         0xFF
#define MT6336_INT_CON3_SET_SHIFT                                        0
#define MT6336_INT_CON3_CLR_ADDR                                         MT6336_PMIC_INT_CON3_CLR
#define MT6336_INT_CON3_CLR_MASK                                         0xFF
#define MT6336_INT_CON3_CLR_SHIFT                                        0
#define MT6336_RG_INT_EN_ADC_TEMP_HT_ADDR                                MT6336_PMIC_INT_CON4
#define MT6336_RG_INT_EN_ADC_TEMP_HT_MASK                                0x1
#define MT6336_RG_INT_EN_ADC_TEMP_HT_SHIFT                               0
#define MT6336_RG_INT_EN_ADC_TEMP_LT_ADDR                                MT6336_PMIC_INT_CON4
#define MT6336_RG_INT_EN_ADC_TEMP_LT_MASK                                0x1
#define MT6336_RG_INT_EN_ADC_TEMP_LT_SHIFT                               1
#define MT6336_RG_INT_EN_ADC_JEITA_HOT_ADDR                              MT6336_PMIC_INT_CON4
#define MT6336_RG_INT_EN_ADC_JEITA_HOT_MASK                              0x1
#define MT6336_RG_INT_EN_ADC_JEITA_HOT_SHIFT                             2
#define MT6336_RG_INT_EN_ADC_JEITA_WARM_ADDR                             MT6336_PMIC_INT_CON4
#define MT6336_RG_INT_EN_ADC_JEITA_WARM_MASK                             0x1
#define MT6336_RG_INT_EN_ADC_JEITA_WARM_SHIFT                            3
#define MT6336_RG_INT_EN_ADC_JEITA_COOL_ADDR                             MT6336_PMIC_INT_CON4
#define MT6336_RG_INT_EN_ADC_JEITA_COOL_MASK                             0x1
#define MT6336_RG_INT_EN_ADC_JEITA_COOL_SHIFT                            4
#define MT6336_RG_INT_EN_ADC_JEITA_COLD_ADDR                             MT6336_PMIC_INT_CON4
#define MT6336_RG_INT_EN_ADC_JEITA_COLD_MASK                             0x1
#define MT6336_RG_INT_EN_ADC_JEITA_COLD_SHIFT                            5
#define MT6336_RG_INT_EN_VBUS_SOFT_OVP_H_ADDR                            MT6336_PMIC_INT_CON4
#define MT6336_RG_INT_EN_VBUS_SOFT_OVP_H_MASK                            0x1
#define MT6336_RG_INT_EN_VBUS_SOFT_OVP_H_SHIFT                           6
#define MT6336_RG_INT_EN_VBUS_SOFT_OVP_L_ADDR                            MT6336_PMIC_INT_CON4
#define MT6336_RG_INT_EN_VBUS_SOFT_OVP_L_MASK                            0x1
#define MT6336_RG_INT_EN_VBUS_SOFT_OVP_L_SHIFT                           7
#define MT6336_INT_CON4_SET_ADDR                                         MT6336_PMIC_INT_CON4_SET
#define MT6336_INT_CON4_SET_MASK                                         0xFF
#define MT6336_INT_CON4_SET_SHIFT                                        0
#define MT6336_INT_CON4_CLR_ADDR                                         MT6336_PMIC_INT_CON4_CLR
#define MT6336_INT_CON4_CLR_MASK                                         0xFF
#define MT6336_INT_CON4_CLR_SHIFT                                        0
#define MT6336_RG_INT_EN_CHR_BAT_RECHG_ADDR                              MT6336_PMIC_INT_CON5
#define MT6336_RG_INT_EN_CHR_BAT_RECHG_MASK                              0x1
#define MT6336_RG_INT_EN_CHR_BAT_RECHG_SHIFT                             0
#define MT6336_RG_INT_EN_BAT_TEMP_H_ADDR                                 MT6336_PMIC_INT_CON5
#define MT6336_RG_INT_EN_BAT_TEMP_H_MASK                                 0x1
#define MT6336_RG_INT_EN_BAT_TEMP_H_SHIFT                                1
#define MT6336_RG_INT_EN_BAT_TEMP_L_ADDR                                 MT6336_PMIC_INT_CON5
#define MT6336_RG_INT_EN_BAT_TEMP_L_MASK                                 0x1
#define MT6336_RG_INT_EN_BAT_TEMP_L_SHIFT                                2
#define MT6336_RG_INT_EN_TYPE_C_L_MIN_ADDR                               MT6336_PMIC_INT_CON5
#define MT6336_RG_INT_EN_TYPE_C_L_MIN_MASK                               0x1
#define MT6336_RG_INT_EN_TYPE_C_L_MIN_SHIFT                              3
#define MT6336_RG_INT_EN_TYPE_C_L_MAX_ADDR                               MT6336_PMIC_INT_CON5
#define MT6336_RG_INT_EN_TYPE_C_L_MAX_MASK                               0x1
#define MT6336_RG_INT_EN_TYPE_C_L_MAX_SHIFT                              4
#define MT6336_RG_INT_EN_TYPE_C_H_MIN_ADDR                               MT6336_PMIC_INT_CON5
#define MT6336_RG_INT_EN_TYPE_C_H_MIN_MASK                               0x1
#define MT6336_RG_INT_EN_TYPE_C_H_MIN_SHIFT                              5
#define MT6336_RG_INT_EN_TYPE_C_H_MAX_ADDR                               MT6336_PMIC_INT_CON5
#define MT6336_RG_INT_EN_TYPE_C_H_MAX_MASK                               0x1
#define MT6336_RG_INT_EN_TYPE_C_H_MAX_SHIFT                              6
#define MT6336_RG_INT_EN_TYPE_C_CC_IRQ_ADDR                              MT6336_PMIC_INT_CON5
#define MT6336_RG_INT_EN_TYPE_C_CC_IRQ_MASK                              0x1
#define MT6336_RG_INT_EN_TYPE_C_CC_IRQ_SHIFT                             7
#define MT6336_INT_CON5_SET_ADDR                                         MT6336_PMIC_INT_CON5_SET
#define MT6336_INT_CON5_SET_MASK                                         0xFF
#define MT6336_INT_CON5_SET_SHIFT                                        0
#define MT6336_INT_CON5_CLR_ADDR                                         MT6336_PMIC_INT_CON5_CLR
#define MT6336_INT_CON5_CLR_MASK                                         0xFF
#define MT6336_INT_CON5_CLR_SHIFT                                        0
#define MT6336_RG_INT_EN_TYPE_C_PD_IRQ_ADDR                              MT6336_PMIC_INT_CON6
#define MT6336_RG_INT_EN_TYPE_C_PD_IRQ_MASK                              0x1
#define MT6336_RG_INT_EN_TYPE_C_PD_IRQ_SHIFT                             0
#define MT6336_RG_INT_EN_DD_PE_STATUS_ADDR                               MT6336_PMIC_INT_CON6
#define MT6336_RG_INT_EN_DD_PE_STATUS_MASK                               0x1
#define MT6336_RG_INT_EN_DD_PE_STATUS_SHIFT                              1
#define MT6336_RG_INT_EN_BC12_V2P7_TIMEOUT_ADDR                          MT6336_PMIC_INT_CON6
#define MT6336_RG_INT_EN_BC12_V2P7_TIMEOUT_MASK                          0x1
#define MT6336_RG_INT_EN_BC12_V2P7_TIMEOUT_SHIFT                         2
#define MT6336_RG_INT_EN_BC12_V3P2_TIMEOUT_ADDR                          MT6336_PMIC_INT_CON6
#define MT6336_RG_INT_EN_BC12_V3P2_TIMEOUT_MASK                          0x1
#define MT6336_RG_INT_EN_BC12_V3P2_TIMEOUT_SHIFT                         3
#define MT6336_RG_INT_EN_DD_BC12_STATUS_ADDR                             MT6336_PMIC_INT_CON6
#define MT6336_RG_INT_EN_DD_BC12_STATUS_MASK                             0x1
#define MT6336_RG_INT_EN_DD_BC12_STATUS_SHIFT                            4
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_SW_ADDR                        MT6336_PMIC_INT_CON6
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_SW_MASK                        0x1
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_SW_SHIFT                       5
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_GLOBAL_ADDR                    MT6336_PMIC_INT_CON6
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_GLOBAL_MASK                    0x1
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_GLOBAL_SHIFT                   6
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_LONG_PRESS_ADDR                MT6336_PMIC_INT_CON6
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_LONG_PRESS_MASK                0x1
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_LONG_PRESS_SHIFT               7
#define MT6336_INT_CON6_SET_ADDR                                         MT6336_PMIC_INT_CON6_SET
#define MT6336_INT_CON6_SET_MASK                                         0xFF
#define MT6336_INT_CON6_SET_SHIFT                                        0
#define MT6336_INT_CON6_CLR_ADDR                                         MT6336_PMIC_INT_CON6_CLR
#define MT6336_INT_CON6_CLR_MASK                                         0xFF
#define MT6336_INT_CON6_CLR_SHIFT                                        0
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_WDT_ADDR                       MT6336_PMIC_INT_CON7
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_WDT_MASK                       0x1
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_WDT_SHIFT                      0
#define MT6336_RG_INT_EN_DD_SWCHR_PLUGOUT_PULSEB_RISING_ADDR             MT6336_PMIC_INT_CON7
#define MT6336_RG_INT_EN_DD_SWCHR_PLUGOUT_PULSEB_RISING_MASK             0x1
#define MT6336_RG_INT_EN_DD_SWCHR_PLUGOUT_PULSEB_RISING_SHIFT            1
#define MT6336_RG_INT_EN_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_ADDR              MT6336_PMIC_INT_CON7
#define MT6336_RG_INT_EN_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_MASK              0x1
#define MT6336_RG_INT_EN_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_SHIFT             2
#define MT6336_RG_INT_EN_DD_SWCHR_PLUGIN_PULSEB_ADDR                     MT6336_PMIC_INT_CON7
#define MT6336_RG_INT_EN_DD_SWCHR_PLUGIN_PULSEB_MASK                     0x1
#define MT6336_RG_INT_EN_DD_SWCHR_PLUGIN_PULSEB_SHIFT                    3
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_SHIP_ADDR                      MT6336_PMIC_INT_CON7
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_SHIP_MASK                      0x1
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_SHIP_SHIFT                     4
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_BAT_OC_ADDR                    MT6336_PMIC_INT_CON7
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_BAT_OC_MASK                    0x1
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_BAT_OC_SHIFT                   5
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_BAT_DEAD_ADDR                  MT6336_PMIC_INT_CON7
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_BAT_DEAD_MASK                  0x1
#define MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_BAT_DEAD_SHIFT                 6
#define MT6336_RG_INT_EN_DD_SWCHR_BUCK_MODE_ADDR                         MT6336_PMIC_INT_CON7
#define MT6336_RG_INT_EN_DD_SWCHR_BUCK_MODE_MASK                         0x1
#define MT6336_RG_INT_EN_DD_SWCHR_BUCK_MODE_SHIFT                        7
#define MT6336_INT_CON7_SET_ADDR                                         MT6336_PMIC_INT_CON7_SET
#define MT6336_INT_CON7_SET_MASK                                         0xFF
#define MT6336_INT_CON7_SET_SHIFT                                        0
#define MT6336_INT_CON7_CLR_ADDR                                         MT6336_PMIC_INT_CON7_CLR
#define MT6336_INT_CON7_CLR_MASK                                         0xFF
#define MT6336_INT_CON7_CLR_SHIFT                                        0
#define MT6336_RG_INT_EN_DD_SWCHR_LOWQ_MODE_ADDR                         MT6336_PMIC_INT_CON8
#define MT6336_RG_INT_EN_DD_SWCHR_LOWQ_MODE_MASK                         0x1
#define MT6336_RG_INT_EN_DD_SWCHR_LOWQ_MODE_SHIFT                        0
#define MT6336_RG_INT_EN_DD_SWCHR_SHIP_MODE_ADDR                         MT6336_PMIC_INT_CON8
#define MT6336_RG_INT_EN_DD_SWCHR_SHIP_MODE_MASK                         0x1
#define MT6336_RG_INT_EN_DD_SWCHR_SHIP_MODE_SHIFT                        1
#define MT6336_RG_INT_EN_DD_SWCHR_BAT_OC_MODE_ADDR                       MT6336_PMIC_INT_CON8
#define MT6336_RG_INT_EN_DD_SWCHR_BAT_OC_MODE_MASK                       0x1
#define MT6336_RG_INT_EN_DD_SWCHR_BAT_OC_MODE_SHIFT                      2
#define MT6336_RG_INT_EN_DD_SWCHR_BAT_DEAD_MODE_ADDR                     MT6336_PMIC_INT_CON8
#define MT6336_RG_INT_EN_DD_SWCHR_BAT_DEAD_MODE_MASK                     0x1
#define MT6336_RG_INT_EN_DD_SWCHR_BAT_DEAD_MODE_SHIFT                    3
#define MT6336_RG_INT_EN_DD_SWCHR_RST_SW_MODE_ADDR                       MT6336_PMIC_INT_CON8
#define MT6336_RG_INT_EN_DD_SWCHR_RST_SW_MODE_MASK                       0x1
#define MT6336_RG_INT_EN_DD_SWCHR_RST_SW_MODE_SHIFT                      4
#define MT6336_RG_INT_EN_DD_SWCHR_RST_GLOBAL_MODE_ADDR                   MT6336_PMIC_INT_CON8
#define MT6336_RG_INT_EN_DD_SWCHR_RST_GLOBAL_MODE_MASK                   0x1
#define MT6336_RG_INT_EN_DD_SWCHR_RST_GLOBAL_MODE_SHIFT                  5
#define MT6336_RG_INT_EN_DD_SWCHR_RST_WDT_MODE_ADDR                      MT6336_PMIC_INT_CON8
#define MT6336_RG_INT_EN_DD_SWCHR_RST_WDT_MODE_MASK                      0x1
#define MT6336_RG_INT_EN_DD_SWCHR_RST_WDT_MODE_SHIFT                     6
#define MT6336_RG_INT_EN_DD_SWCHR_RST_LONG_PRESS_MODE_ADDR               MT6336_PMIC_INT_CON8
#define MT6336_RG_INT_EN_DD_SWCHR_RST_LONG_PRESS_MODE_MASK               0x1
#define MT6336_RG_INT_EN_DD_SWCHR_RST_LONG_PRESS_MODE_SHIFT              7
#define MT6336_INT_CON8_SET_ADDR                                         MT6336_PMIC_INT_CON8_SET
#define MT6336_INT_CON8_SET_MASK                                         0xFF
#define MT6336_INT_CON8_SET_SHIFT                                        0
#define MT6336_INT_CON8_CLR_ADDR                                         MT6336_PMIC_INT_CON8_CLR
#define MT6336_INT_CON8_CLR_MASK                                         0xFF
#define MT6336_INT_CON8_CLR_SHIFT                                        0
#define MT6336_RG_INT_EN_DD_SWCHR_CHR_SUSPEND_STATE_ADDR                 MT6336_PMIC_INT_CON9
#define MT6336_RG_INT_EN_DD_SWCHR_CHR_SUSPEND_STATE_MASK                 0x1
#define MT6336_RG_INT_EN_DD_SWCHR_CHR_SUSPEND_STATE_SHIFT                0
#define MT6336_RG_INT_EN_DD_SWCHR_BUCK_PROTECT_STATE_ADDR                MT6336_PMIC_INT_CON9
#define MT6336_RG_INT_EN_DD_SWCHR_BUCK_PROTECT_STATE_MASK                0x1
#define MT6336_RG_INT_EN_DD_SWCHR_BUCK_PROTECT_STATE_SHIFT               1
#define MT6336_POLARITY_ADDR                                             MT6336_PMIC_INT_CON9
#define MT6336_POLARITY_MASK                                             0x1
#define MT6336_POLARITY_SHIFT                                            2
#define MT6336_INT_CON9_SET_ADDR                                         MT6336_PMIC_INT_CON9_SET
#define MT6336_INT_CON9_SET_MASK                                         0xFF
#define MT6336_INT_CON9_SET_SHIFT                                        0
#define MT6336_INT_CON9_CLR_ADDR                                         MT6336_PMIC_INT_CON9_CLR
#define MT6336_INT_CON9_CLR_MASK                                         0xFF
#define MT6336_INT_CON9_CLR_SHIFT                                        0
#define MT6336_RG_INT_MASK_CHR_VBUS_PLUGIN_ADDR                          MT6336_PMIC_INT_MASK_CON0
#define MT6336_RG_INT_MASK_CHR_VBUS_PLUGIN_MASK                          0x1
#define MT6336_RG_INT_MASK_CHR_VBUS_PLUGIN_SHIFT                         0
#define MT6336_RG_INT_MASK_CHR_VBUS_PLUGOUT_ADDR                         MT6336_PMIC_INT_MASK_CON0
#define MT6336_RG_INT_MASK_CHR_VBUS_PLUGOUT_MASK                         0x1
#define MT6336_RG_INT_MASK_CHR_VBUS_PLUGOUT_SHIFT                        1
#define MT6336_RG_INT_MASK_STATE_BUCK_BACKGROUND_ADDR                    MT6336_PMIC_INT_MASK_CON0
#define MT6336_RG_INT_MASK_STATE_BUCK_BACKGROUND_MASK                    0x1
#define MT6336_RG_INT_MASK_STATE_BUCK_BACKGROUND_SHIFT                   2
#define MT6336_RG_INT_MASK_STATE_BUCK_EOC_ADDR                           MT6336_PMIC_INT_MASK_CON0
#define MT6336_RG_INT_MASK_STATE_BUCK_EOC_MASK                           0x1
#define MT6336_RG_INT_MASK_STATE_BUCK_EOC_SHIFT                          3
#define MT6336_RG_INT_MASK_STATE_BUCK_PRECC0_ADDR                        MT6336_PMIC_INT_MASK_CON0
#define MT6336_RG_INT_MASK_STATE_BUCK_PRECC0_MASK                        0x1
#define MT6336_RG_INT_MASK_STATE_BUCK_PRECC0_SHIFT                       4
#define MT6336_RG_INT_MASK_STATE_BUCK_PRECC1_ADDR                        MT6336_PMIC_INT_MASK_CON0
#define MT6336_RG_INT_MASK_STATE_BUCK_PRECC1_MASK                        0x1
#define MT6336_RG_INT_MASK_STATE_BUCK_PRECC1_SHIFT                       5
#define MT6336_RG_INT_MASK_STATE_BUCK_FASTCC_ADDR                        MT6336_PMIC_INT_MASK_CON0
#define MT6336_RG_INT_MASK_STATE_BUCK_FASTCC_MASK                        0x1
#define MT6336_RG_INT_MASK_STATE_BUCK_FASTCC_SHIFT                       6
#define MT6336_RG_INT_MASK_CHR_WEAKBUS_ADDR                              MT6336_PMIC_INT_MASK_CON0
#define MT6336_RG_INT_MASK_CHR_WEAKBUS_MASK                              0x1
#define MT6336_RG_INT_MASK_CHR_WEAKBUS_SHIFT                             7
#define MT6336_INT_MASK_CON0_SET_ADDR                                    MT6336_PMIC_INT_MASK_CON0_SET
#define MT6336_INT_MASK_CON0_SET_MASK                                    0xFF
#define MT6336_INT_MASK_CON0_SET_SHIFT                                   0
#define MT6336_INT_MASK_CON0_CLR_ADDR                                    MT6336_PMIC_INT_MASK_CON0_CLR
#define MT6336_INT_MASK_CON0_CLR_MASK                                    0xFF
#define MT6336_INT_MASK_CON0_CLR_SHIFT                                   0
#define MT6336_RG_INT_MASK_CHR_SYS_OVP_ADDR                              MT6336_PMIC_INT_MASK_CON1
#define MT6336_RG_INT_MASK_CHR_SYS_OVP_MASK                              0x1
#define MT6336_RG_INT_MASK_CHR_SYS_OVP_SHIFT                             0
#define MT6336_RG_INT_MASK_CHR_BAT_OVP_ADDR                              MT6336_PMIC_INT_MASK_CON1
#define MT6336_RG_INT_MASK_CHR_BAT_OVP_MASK                              0x1
#define MT6336_RG_INT_MASK_CHR_BAT_OVP_SHIFT                             1
#define MT6336_RG_INT_MASK_CHR_VBUS_OVP_ADDR                             MT6336_PMIC_INT_MASK_CON1
#define MT6336_RG_INT_MASK_CHR_VBUS_OVP_MASK                             0x1
#define MT6336_RG_INT_MASK_CHR_VBUS_OVP_SHIFT                            2
#define MT6336_RG_INT_MASK_CHR_VBUS_UVLO_ADDR                            MT6336_PMIC_INT_MASK_CON1
#define MT6336_RG_INT_MASK_CHR_VBUS_UVLO_MASK                            0x1
#define MT6336_RG_INT_MASK_CHR_VBUS_UVLO_SHIFT                           3
#define MT6336_RG_INT_MASK_CHR_ICHR_ITERM_ADDR                           MT6336_PMIC_INT_MASK_CON1
#define MT6336_RG_INT_MASK_CHR_ICHR_ITERM_MASK                           0x1
#define MT6336_RG_INT_MASK_CHR_ICHR_ITERM_SHIFT                          4
#define MT6336_RG_INT_MASK_CHIP_TEMP_OVERHEAT_ADDR                       MT6336_PMIC_INT_MASK_CON1
#define MT6336_RG_INT_MASK_CHIP_TEMP_OVERHEAT_MASK                       0x1
#define MT6336_RG_INT_MASK_CHIP_TEMP_OVERHEAT_SHIFT                      5
#define MT6336_RG_INT_MASK_CHIP_MBATPP_DIS_OC_DIG_ADDR                   MT6336_PMIC_INT_MASK_CON1
#define MT6336_RG_INT_MASK_CHIP_MBATPP_DIS_OC_DIG_MASK                   0x1
#define MT6336_RG_INT_MASK_CHIP_MBATPP_DIS_OC_DIG_SHIFT                  6
#define MT6336_RG_INT_MASK_OTG_BVALID_ADDR                               MT6336_PMIC_INT_MASK_CON1
#define MT6336_RG_INT_MASK_OTG_BVALID_MASK                               0x1
#define MT6336_RG_INT_MASK_OTG_BVALID_SHIFT                              7
#define MT6336_INT_MASK_CON1_SET_ADDR                                    MT6336_PMIC_INT_MASK_CON1_SET
#define MT6336_INT_MASK_CON1_SET_MASK                                    0xFF
#define MT6336_INT_MASK_CON1_SET_SHIFT                                   0
#define MT6336_INT_MASK_CON1_CLR_ADDR                                    MT6336_PMIC_INT_MASK_CON1_CLR
#define MT6336_INT_MASK_CON1_CLR_MASK                                    0xFF
#define MT6336_INT_MASK_CON1_CLR_SHIFT                                   0
#define MT6336_RG_INT_MASK_OTG_VM_UVLO_ADDR                              MT6336_PMIC_INT_MASK_CON2
#define MT6336_RG_INT_MASK_OTG_VM_UVLO_MASK                              0x1
#define MT6336_RG_INT_MASK_OTG_VM_UVLO_SHIFT                             0
#define MT6336_RG_INT_MASK_OTG_VM_OVP_ADDR                               MT6336_PMIC_INT_MASK_CON2
#define MT6336_RG_INT_MASK_OTG_VM_OVP_MASK                               0x1
#define MT6336_RG_INT_MASK_OTG_VM_OVP_SHIFT                              1
#define MT6336_RG_INT_MASK_OTG_VBAT_UVLO_ADDR                            MT6336_PMIC_INT_MASK_CON2
#define MT6336_RG_INT_MASK_OTG_VBAT_UVLO_MASK                            0x1
#define MT6336_RG_INT_MASK_OTG_VBAT_UVLO_SHIFT                           2
#define MT6336_RG_INT_MASK_OTG_VM_OLP_ADDR                               MT6336_PMIC_INT_MASK_CON2
#define MT6336_RG_INT_MASK_OTG_VM_OLP_MASK                               0x1
#define MT6336_RG_INT_MASK_OTG_VM_OLP_SHIFT                              3
#define MT6336_RG_INT_MASK_FLASH_VFLA_UVLO_ADDR                          MT6336_PMIC_INT_MASK_CON2
#define MT6336_RG_INT_MASK_FLASH_VFLA_UVLO_MASK                          0x1
#define MT6336_RG_INT_MASK_FLASH_VFLA_UVLO_SHIFT                         4
#define MT6336_RG_INT_MASK_FLASH_VFLA_OVP_ADDR                           MT6336_PMIC_INT_MASK_CON2
#define MT6336_RG_INT_MASK_FLASH_VFLA_OVP_MASK                           0x1
#define MT6336_RG_INT_MASK_FLASH_VFLA_OVP_SHIFT                          5
#define MT6336_RG_INT_MASK_LED1_SHORT_ADDR                               MT6336_PMIC_INT_MASK_CON2
#define MT6336_RG_INT_MASK_LED1_SHORT_MASK                               0x1
#define MT6336_RG_INT_MASK_LED1_SHORT_SHIFT                              6
#define MT6336_RG_INT_MASK_LED1_OPEN_ADDR                                MT6336_PMIC_INT_MASK_CON2
#define MT6336_RG_INT_MASK_LED1_OPEN_MASK                                0x1
#define MT6336_RG_INT_MASK_LED1_OPEN_SHIFT                               7
#define MT6336_INT_MASK_CON2_SET_ADDR                                    MT6336_PMIC_INT_MASK_CON2_SET
#define MT6336_INT_MASK_CON2_SET_MASK                                    0xFF
#define MT6336_INT_MASK_CON2_SET_SHIFT                                   0
#define MT6336_INT_MASK_CON2_CLR_ADDR                                    MT6336_PMIC_INT_MASK_CON2_CLR
#define MT6336_INT_MASK_CON2_CLR_MASK                                    0xFF
#define MT6336_INT_MASK_CON2_CLR_SHIFT                                   0
#define MT6336_RG_INT_MASK_LED2_SHORT_ADDR                               MT6336_PMIC_INT_MASK_CON3
#define MT6336_RG_INT_MASK_LED2_SHORT_MASK                               0x1
#define MT6336_RG_INT_MASK_LED2_SHORT_SHIFT                              0
#define MT6336_RG_INT_MASK_LED2_OPEN_ADDR                                MT6336_PMIC_INT_MASK_CON3
#define MT6336_RG_INT_MASK_LED2_OPEN_MASK                                0x1
#define MT6336_RG_INT_MASK_LED2_OPEN_SHIFT                               1
#define MT6336_RG_INT_MASK_FLASH_TIMEOUT_ADDR                            MT6336_PMIC_INT_MASK_CON3
#define MT6336_RG_INT_MASK_FLASH_TIMEOUT_MASK                            0x1
#define MT6336_RG_INT_MASK_FLASH_TIMEOUT_SHIFT                           2
#define MT6336_RG_INT_MASK_TORCH_TIMEOUT_ADDR                            MT6336_PMIC_INT_MASK_CON3
#define MT6336_RG_INT_MASK_TORCH_TIMEOUT_MASK                            0x1
#define MT6336_RG_INT_MASK_TORCH_TIMEOUT_SHIFT                           3
#define MT6336_RG_INT_MASK_DD_VBUS_IN_VALID_ADDR                         MT6336_PMIC_INT_MASK_CON3
#define MT6336_RG_INT_MASK_DD_VBUS_IN_VALID_MASK                         0x1
#define MT6336_RG_INT_MASK_DD_VBUS_IN_VALID_SHIFT                        4
#define MT6336_RG_INT_MASK_WDT_TIMEOUT_ADDR                              MT6336_PMIC_INT_MASK_CON3
#define MT6336_RG_INT_MASK_WDT_TIMEOUT_MASK                              0x1
#define MT6336_RG_INT_MASK_WDT_TIMEOUT_SHIFT                             5
#define MT6336_RG_INT_MASK_SAFETY_TIMEOUT_ADDR                           MT6336_PMIC_INT_MASK_CON3
#define MT6336_RG_INT_MASK_SAFETY_TIMEOUT_MASK                           0x1
#define MT6336_RG_INT_MASK_SAFETY_TIMEOUT_SHIFT                          6
#define MT6336_RG_INT_MASK_CHR_AICC_DONE_ADDR                            MT6336_PMIC_INT_MASK_CON3
#define MT6336_RG_INT_MASK_CHR_AICC_DONE_MASK                            0x1
#define MT6336_RG_INT_MASK_CHR_AICC_DONE_SHIFT                           7
#define MT6336_INT_MASK_CON3_SET_ADDR                                    MT6336_PMIC_INT_MASK_CON3_SET
#define MT6336_INT_MASK_CON3_SET_MASK                                    0xFF
#define MT6336_INT_MASK_CON3_SET_SHIFT                                   0
#define MT6336_INT_MASK_CON3_CLR_ADDR                                    MT6336_PMIC_INT_MASK_CON3_CLR
#define MT6336_INT_MASK_CON3_CLR_MASK                                    0xFF
#define MT6336_INT_MASK_CON3_CLR_SHIFT                                   0
#define MT6336_RG_INT_MASK_ADC_TEMP_HT_ADDR                              MT6336_PMIC_INT_MASK_CON4
#define MT6336_RG_INT_MASK_ADC_TEMP_HT_MASK                              0x1
#define MT6336_RG_INT_MASK_ADC_TEMP_HT_SHIFT                             0
#define MT6336_RG_INT_MASK_ADC_TEMP_LT_ADDR                              MT6336_PMIC_INT_MASK_CON4
#define MT6336_RG_INT_MASK_ADC_TEMP_LT_MASK                              0x1
#define MT6336_RG_INT_MASK_ADC_TEMP_LT_SHIFT                             1
#define MT6336_RG_INT_MASK_ADC_JEITA_HOT_ADDR                            MT6336_PMIC_INT_MASK_CON4
#define MT6336_RG_INT_MASK_ADC_JEITA_HOT_MASK                            0x1
#define MT6336_RG_INT_MASK_ADC_JEITA_HOT_SHIFT                           2
#define MT6336_RG_INT_MASK_ADC_JEITA_WARM_ADDR                           MT6336_PMIC_INT_MASK_CON4
#define MT6336_RG_INT_MASK_ADC_JEITA_WARM_MASK                           0x1
#define MT6336_RG_INT_MASK_ADC_JEITA_WARM_SHIFT                          3
#define MT6336_RG_INT_MASK_ADC_JEITA_COOL_ADDR                           MT6336_PMIC_INT_MASK_CON4
#define MT6336_RG_INT_MASK_ADC_JEITA_COOL_MASK                           0x1
#define MT6336_RG_INT_MASK_ADC_JEITA_COOL_SHIFT                          4
#define MT6336_RG_INT_MASK_ADC_JEITA_COLD_ADDR                           MT6336_PMIC_INT_MASK_CON4
#define MT6336_RG_INT_MASK_ADC_JEITA_COLD_MASK                           0x1
#define MT6336_RG_INT_MASK_ADC_JEITA_COLD_SHIFT                          5
#define MT6336_RG_INT_MASK_VBUS_SOFT_OVP_H_ADDR                          MT6336_PMIC_INT_MASK_CON4
#define MT6336_RG_INT_MASK_VBUS_SOFT_OVP_H_MASK                          0x1
#define MT6336_RG_INT_MASK_VBUS_SOFT_OVP_H_SHIFT                         6
#define MT6336_RG_INT_MASK_VBUS_SOFT_OVP_L_ADDR                          MT6336_PMIC_INT_MASK_CON4
#define MT6336_RG_INT_MASK_VBUS_SOFT_OVP_L_MASK                          0x1
#define MT6336_RG_INT_MASK_VBUS_SOFT_OVP_L_SHIFT                         7
#define MT6336_INT_MASK_CON4_SET_ADDR                                    MT6336_PMIC_INT_MASK_CON4_SET
#define MT6336_INT_MASK_CON4_SET_MASK                                    0xFF
#define MT6336_INT_MASK_CON4_SET_SHIFT                                   0
#define MT6336_INT_MASK_CON4_CLR_ADDR                                    MT6336_PMIC_INT_MASK_CON4_CLR
#define MT6336_INT_MASK_CON4_CLR_MASK                                    0xFF
#define MT6336_INT_MASK_CON4_CLR_SHIFT                                   0
#define MT6336_RG_INT_MASK_CHR_BAT_RECHG_ADDR                            MT6336_PMIC_INT_MASK_CON5
#define MT6336_RG_INT_MASK_CHR_BAT_RECHG_MASK                            0x1
#define MT6336_RG_INT_MASK_CHR_BAT_RECHG_SHIFT                           0
#define MT6336_RG_INT_MASK_BAT_TEMP_H_ADDR                               MT6336_PMIC_INT_MASK_CON5
#define MT6336_RG_INT_MASK_BAT_TEMP_H_MASK                               0x1
#define MT6336_RG_INT_MASK_BAT_TEMP_H_SHIFT                              1
#define MT6336_RG_INT_MASK_BAT_TEMP_L_ADDR                               MT6336_PMIC_INT_MASK_CON5
#define MT6336_RG_INT_MASK_BAT_TEMP_L_MASK                               0x1
#define MT6336_RG_INT_MASK_BAT_TEMP_L_SHIFT                              2
#define MT6336_RG_INT_MASK_TYPE_C_L_MIN_ADDR                             MT6336_PMIC_INT_MASK_CON5
#define MT6336_RG_INT_MASK_TYPE_C_L_MIN_MASK                             0x1
#define MT6336_RG_INT_MASK_TYPE_C_L_MIN_SHIFT                            3
#define MT6336_RG_INT_MASK_TYPE_C_L_MAX_ADDR                             MT6336_PMIC_INT_MASK_CON5
#define MT6336_RG_INT_MASK_TYPE_C_L_MAX_MASK                             0x1
#define MT6336_RG_INT_MASK_TYPE_C_L_MAX_SHIFT                            4
#define MT6336_RG_INT_MASK_TYPE_C_H_MIN_ADDR                             MT6336_PMIC_INT_MASK_CON5
#define MT6336_RG_INT_MASK_TYPE_C_H_MIN_MASK                             0x1
#define MT6336_RG_INT_MASK_TYPE_C_H_MIN_SHIFT                            5
#define MT6336_RG_INT_MASK_TYPE_C_H_MAX_ADDR                             MT6336_PMIC_INT_MASK_CON5
#define MT6336_RG_INT_MASK_TYPE_C_H_MAX_MASK                             0x1
#define MT6336_RG_INT_MASK_TYPE_C_H_MAX_SHIFT                            6
#define MT6336_RG_INT_MASK_TYPE_C_CC_IRQ_ADDR                            MT6336_PMIC_INT_MASK_CON5
#define MT6336_RG_INT_MASK_TYPE_C_CC_IRQ_MASK                            0x1
#define MT6336_RG_INT_MASK_TYPE_C_CC_IRQ_SHIFT                           7
#define MT6336_INT_MASK_CON5_SET_ADDR                                    MT6336_PMIC_INT_MASK_CON5_SET
#define MT6336_INT_MASK_CON5_SET_MASK                                    0xFF
#define MT6336_INT_MASK_CON5_SET_SHIFT                                   0
#define MT6336_INT_MASK_CON5_CLR_ADDR                                    MT6336_PMIC_INT_MASK_CON5_CLR
#define MT6336_INT_MASK_CON5_CLR_MASK                                    0xFF
#define MT6336_INT_MASK_CON5_CLR_SHIFT                                   0
#define MT6336_RG_INT_MASK_TYPE_C_PD_IRQ_ADDR                            MT6336_PMIC_INT_MASK_CON6
#define MT6336_RG_INT_MASK_TYPE_C_PD_IRQ_MASK                            0x1
#define MT6336_RG_INT_MASK_TYPE_C_PD_IRQ_SHIFT                           0
#define MT6336_RG_INT_MASK_DD_PE_STATUS_ADDR                             MT6336_PMIC_INT_MASK_CON6
#define MT6336_RG_INT_MASK_DD_PE_STATUS_MASK                             0x1
#define MT6336_RG_INT_MASK_DD_PE_STATUS_SHIFT                            1
#define MT6336_RG_INT_MASK_BC12_V2P7_TIMEOUT_ADDR                        MT6336_PMIC_INT_MASK_CON6
#define MT6336_RG_INT_MASK_BC12_V2P7_TIMEOUT_MASK                        0x1
#define MT6336_RG_INT_MASK_BC12_V2P7_TIMEOUT_SHIFT                       2
#define MT6336_RG_INT_MASK_BC12_V3P2_TIMEOUT_ADDR                        MT6336_PMIC_INT_MASK_CON6
#define MT6336_RG_INT_MASK_BC12_V3P2_TIMEOUT_MASK                        0x1
#define MT6336_RG_INT_MASK_BC12_V3P2_TIMEOUT_SHIFT                       3
#define MT6336_RG_INT_MASK_DD_BC12_STATUS_ADDR                           MT6336_PMIC_INT_MASK_CON6
#define MT6336_RG_INT_MASK_DD_BC12_STATUS_MASK                           0x1
#define MT6336_RG_INT_MASK_DD_BC12_STATUS_SHIFT                          4
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_SW_ADDR                      MT6336_PMIC_INT_MASK_CON6
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_SW_MASK                      0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_SW_SHIFT                     5
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_GLOBAL_ADDR                  MT6336_PMIC_INT_MASK_CON6
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_GLOBAL_MASK                  0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_GLOBAL_SHIFT                 6
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_LONG_PRESS_ADDR              MT6336_PMIC_INT_MASK_CON6
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_LONG_PRESS_MASK              0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_LONG_PRESS_SHIFT             7
#define MT6336_INT_MASK_CON6_SET_ADDR                                    MT6336_PMIC_INT_MASK_CON6_SET
#define MT6336_INT_MASK_CON6_SET_MASK                                    0xFF
#define MT6336_INT_MASK_CON6_SET_SHIFT                                   0
#define MT6336_INT_MASK_CON6_CLR_ADDR                                    MT6336_PMIC_INT_MASK_CON6_CLR
#define MT6336_INT_MASK_CON6_CLR_MASK                                    0xFF
#define MT6336_INT_MASK_CON6_CLR_SHIFT                                   0
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_WDT_ADDR                     MT6336_PMIC_INT_MASK_CON7
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_WDT_MASK                     0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_WDT_SHIFT                    0
#define MT6336_RG_INT_MASK_DD_SWCHR_PLUGOUT_PULSEB_RISING_ADDR           MT6336_PMIC_INT_MASK_CON7
#define MT6336_RG_INT_MASK_DD_SWCHR_PLUGOUT_PULSEB_RISING_MASK           0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_PLUGOUT_PULSEB_RISING_SHIFT          1
#define MT6336_RG_INT_MASK_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_ADDR            MT6336_PMIC_INT_MASK_CON7
#define MT6336_RG_INT_MASK_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_MASK            0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_SHIFT           2
#define MT6336_RG_INT_MASK_DD_SWCHR_PLUGIN_PULSEB_ADDR                   MT6336_PMIC_INT_MASK_CON7
#define MT6336_RG_INT_MASK_DD_SWCHR_PLUGIN_PULSEB_MASK                   0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_PLUGIN_PULSEB_SHIFT                  3
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_SHIP_ADDR                    MT6336_PMIC_INT_MASK_CON7
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_SHIP_MASK                    0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_SHIP_SHIFT                   4
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_BAT_OC_ADDR                  MT6336_PMIC_INT_MASK_CON7
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_BAT_OC_MASK                  0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_BAT_OC_SHIFT                 5
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_BAT_DEAD_ADDR                MT6336_PMIC_INT_MASK_CON7
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_BAT_DEAD_MASK                0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_BAT_DEAD_SHIFT               6
#define MT6336_RG_INT_MASK_DD_SWCHR_BUCK_MODE_ADDR                       MT6336_PMIC_INT_MASK_CON7
#define MT6336_RG_INT_MASK_DD_SWCHR_BUCK_MODE_MASK                       0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_BUCK_MODE_SHIFT                      7
#define MT6336_INT_MASK_CON7_SET_ADDR                                    MT6336_PMIC_INT_MASK_CON7_SET
#define MT6336_INT_MASK_CON7_SET_MASK                                    0xFF
#define MT6336_INT_MASK_CON7_SET_SHIFT                                   0
#define MT6336_INT_MASK_CON7_CLR_ADDR                                    MT6336_PMIC_INT_MASK_CON7_CLR
#define MT6336_INT_MASK_CON7_CLR_MASK                                    0xFF
#define MT6336_INT_MASK_CON7_CLR_SHIFT                                   0
#define MT6336_RG_INT_MASK_DD_SWCHR_LOWQ_MODE_ADDR                       MT6336_PMIC_INT_MASK_CON8
#define MT6336_RG_INT_MASK_DD_SWCHR_LOWQ_MODE_MASK                       0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_LOWQ_MODE_SHIFT                      0
#define MT6336_RG_INT_MASK_DD_SWCHR_SHIP_MODE_ADDR                       MT6336_PMIC_INT_MASK_CON8
#define MT6336_RG_INT_MASK_DD_SWCHR_SHIP_MODE_MASK                       0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_SHIP_MODE_SHIFT                      1
#define MT6336_RG_INT_MASK_DD_SWCHR_BAT_OC_MODE_ADDR                     MT6336_PMIC_INT_MASK_CON8
#define MT6336_RG_INT_MASK_DD_SWCHR_BAT_OC_MODE_MASK                     0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_BAT_OC_MODE_SHIFT                    2
#define MT6336_RG_INT_MASK_DD_SWCHR_BAT_DEAD_MODE_ADDR                   MT6336_PMIC_INT_MASK_CON8
#define MT6336_RG_INT_MASK_DD_SWCHR_BAT_DEAD_MODE_MASK                   0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_BAT_DEAD_MODE_SHIFT                  3
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_SW_MODE_ADDR                     MT6336_PMIC_INT_MASK_CON8
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_SW_MODE_MASK                     0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_SW_MODE_SHIFT                    4
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_GLOBAL_MODE_ADDR                 MT6336_PMIC_INT_MASK_CON8
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_GLOBAL_MODE_MASK                 0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_GLOBAL_MODE_SHIFT                5
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_WDT_MODE_ADDR                    MT6336_PMIC_INT_MASK_CON8
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_WDT_MODE_MASK                    0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_WDT_MODE_SHIFT                   6
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_LONG_PRESS_MODE_ADDR             MT6336_PMIC_INT_MASK_CON8
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_LONG_PRESS_MODE_MASK             0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_RST_LONG_PRESS_MODE_SHIFT            7
#define MT6336_INT_MASK_CON8_SET_ADDR                                    MT6336_PMIC_INT_MASK_CON8_SET
#define MT6336_INT_MASK_CON8_SET_MASK                                    0xFF
#define MT6336_INT_MASK_CON8_SET_SHIFT                                   0
#define MT6336_INT_MASK_CON8_CLR_ADDR                                    MT6336_PMIC_INT_MASK_CON8_CLR
#define MT6336_INT_MASK_CON8_CLR_MASK                                    0xFF
#define MT6336_INT_MASK_CON8_CLR_SHIFT                                   0
#define MT6336_RG_INT_MASK_DD_SWCHR_CHR_SUSPEND_STATE_ADDR               MT6336_PMIC_INT_MASK_CON9
#define MT6336_RG_INT_MASK_DD_SWCHR_CHR_SUSPEND_STATE_MASK               0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_CHR_SUSPEND_STATE_SHIFT              0
#define MT6336_RG_INT_MASK_DD_SWCHR_BUCK_PROTECT_STATE_ADDR              MT6336_PMIC_INT_MASK_CON9
#define MT6336_RG_INT_MASK_DD_SWCHR_BUCK_PROTECT_STATE_MASK              0x1
#define MT6336_RG_INT_MASK_DD_SWCHR_BUCK_PROTECT_STATE_SHIFT             1
#define MT6336_INT_MASK_CON9_SET_ADDR                                    MT6336_PMIC_INT_MASK_CON9_SET
#define MT6336_INT_MASK_CON9_SET_MASK                                    0xFF
#define MT6336_INT_MASK_CON9_SET_SHIFT                                   0
#define MT6336_INT_MASK_CON9_CLR_ADDR                                    MT6336_PMIC_INT_MASK_CON9_CLR
#define MT6336_INT_MASK_CON9_CLR_MASK                                    0xFF
#define MT6336_INT_MASK_CON9_CLR_SHIFT                                   0
#define MT6336_RG_INT_STATUS_CHR_VBUS_PLUGIN_ADDR                        MT6336_PMIC_INT_STATUS0
#define MT6336_RG_INT_STATUS_CHR_VBUS_PLUGIN_MASK                        0x1
#define MT6336_RG_INT_STATUS_CHR_VBUS_PLUGIN_SHIFT                       0
#define MT6336_RG_INT_STATUS_CHR_VBUS_PLUGOUT_ADDR                       MT6336_PMIC_INT_STATUS0
#define MT6336_RG_INT_STATUS_CHR_VBUS_PLUGOUT_MASK                       0x1
#define MT6336_RG_INT_STATUS_CHR_VBUS_PLUGOUT_SHIFT                      1
#define MT6336_RG_INT_STATUS_STATE_BUCK_BACKGROUND_ADDR                  MT6336_PMIC_INT_STATUS0
#define MT6336_RG_INT_STATUS_STATE_BUCK_BACKGROUND_MASK                  0x1
#define MT6336_RG_INT_STATUS_STATE_BUCK_BACKGROUND_SHIFT                 2
#define MT6336_RG_INT_STATUS_STATE_BUCK_EOC_ADDR                         MT6336_PMIC_INT_STATUS0
#define MT6336_RG_INT_STATUS_STATE_BUCK_EOC_MASK                         0x1
#define MT6336_RG_INT_STATUS_STATE_BUCK_EOC_SHIFT                        3
#define MT6336_RG_INT_STATUS_STATE_BUCK_PRECC0_ADDR                      MT6336_PMIC_INT_STATUS0
#define MT6336_RG_INT_STATUS_STATE_BUCK_PRECC0_MASK                      0x1
#define MT6336_RG_INT_STATUS_STATE_BUCK_PRECC0_SHIFT                     4
#define MT6336_RG_INT_STATUS_STATE_BUCK_PRECC1_ADDR                      MT6336_PMIC_INT_STATUS0
#define MT6336_RG_INT_STATUS_STATE_BUCK_PRECC1_MASK                      0x1
#define MT6336_RG_INT_STATUS_STATE_BUCK_PRECC1_SHIFT                     5
#define MT6336_RG_INT_STATUS_STATE_BUCK_FASTCC_ADDR                      MT6336_PMIC_INT_STATUS0
#define MT6336_RG_INT_STATUS_STATE_BUCK_FASTCC_MASK                      0x1
#define MT6336_RG_INT_STATUS_STATE_BUCK_FASTCC_SHIFT                     6
#define MT6336_RG_INT_STATUS_CHR_WEAKBUS_ADDR                            MT6336_PMIC_INT_STATUS0
#define MT6336_RG_INT_STATUS_CHR_WEAKBUS_MASK                            0x1
#define MT6336_RG_INT_STATUS_CHR_WEAKBUS_SHIFT                           7
#define MT6336_RG_INT_STATUS_CHR_SYS_OVP_ADDR                            MT6336_PMIC_INT_STATUS1
#define MT6336_RG_INT_STATUS_CHR_SYS_OVP_MASK                            0x1
#define MT6336_RG_INT_STATUS_CHR_SYS_OVP_SHIFT                           0
#define MT6336_RG_INT_STATUS_CHR_BAT_OVP_ADDR                            MT6336_PMIC_INT_STATUS1
#define MT6336_RG_INT_STATUS_CHR_BAT_OVP_MASK                            0x1
#define MT6336_RG_INT_STATUS_CHR_BAT_OVP_SHIFT                           1
#define MT6336_RG_INT_STATUS_CHR_VBUS_OVP_ADDR                           MT6336_PMIC_INT_STATUS1
#define MT6336_RG_INT_STATUS_CHR_VBUS_OVP_MASK                           0x1
#define MT6336_RG_INT_STATUS_CHR_VBUS_OVP_SHIFT                          2
#define MT6336_RG_INT_STATUS_CHR_VBUS_UVLO_ADDR                          MT6336_PMIC_INT_STATUS1
#define MT6336_RG_INT_STATUS_CHR_VBUS_UVLO_MASK                          0x1
#define MT6336_RG_INT_STATUS_CHR_VBUS_UVLO_SHIFT                         3
#define MT6336_RG_INT_STATUS_CHR_ICHR_ITERM_ADDR                         MT6336_PMIC_INT_STATUS1
#define MT6336_RG_INT_STATUS_CHR_ICHR_ITERM_MASK                         0x1
#define MT6336_RG_INT_STATUS_CHR_ICHR_ITERM_SHIFT                        4
#define MT6336_RG_INT_STATUS_CHIP_TEMP_OVERHEAT_ADDR                     MT6336_PMIC_INT_STATUS1
#define MT6336_RG_INT_STATUS_CHIP_TEMP_OVERHEAT_MASK                     0x1
#define MT6336_RG_INT_STATUS_CHIP_TEMP_OVERHEAT_SHIFT                    5
#define MT6336_RG_INT_STATUS_CHIP_MBATPP_DIS_OC_DIG_ADDR                 MT6336_PMIC_INT_STATUS1
#define MT6336_RG_INT_STATUS_CHIP_MBATPP_DIS_OC_DIG_MASK                 0x1
#define MT6336_RG_INT_STATUS_CHIP_MBATPP_DIS_OC_DIG_SHIFT                6
#define MT6336_RG_INT_STATUS_OTG_BVALID_ADDR                             MT6336_PMIC_INT_STATUS1
#define MT6336_RG_INT_STATUS_OTG_BVALID_MASK                             0x1
#define MT6336_RG_INT_STATUS_OTG_BVALID_SHIFT                            7
#define MT6336_RG_INT_STATUS_OTG_VM_UVLO_ADDR                            MT6336_PMIC_INT_STATUS2
#define MT6336_RG_INT_STATUS_OTG_VM_UVLO_MASK                            0x1
#define MT6336_RG_INT_STATUS_OTG_VM_UVLO_SHIFT                           0
#define MT6336_RG_INT_STATUS_OTG_VM_OVP_ADDR                             MT6336_PMIC_INT_STATUS2
#define MT6336_RG_INT_STATUS_OTG_VM_OVP_MASK                             0x1
#define MT6336_RG_INT_STATUS_OTG_VM_OVP_SHIFT                            1
#define MT6336_RG_INT_STATUS_OTG_VBAT_UVLO_ADDR                          MT6336_PMIC_INT_STATUS2
#define MT6336_RG_INT_STATUS_OTG_VBAT_UVLO_MASK                          0x1
#define MT6336_RG_INT_STATUS_OTG_VBAT_UVLO_SHIFT                         2
#define MT6336_RG_INT_STATUS_OTG_VM_OLP_ADDR                             MT6336_PMIC_INT_STATUS2
#define MT6336_RG_INT_STATUS_OTG_VM_OLP_MASK                             0x1
#define MT6336_RG_INT_STATUS_OTG_VM_OLP_SHIFT                            3
#define MT6336_RG_INT_STATUS_FLASH_VFLA_UVLO_ADDR                        MT6336_PMIC_INT_STATUS2
#define MT6336_RG_INT_STATUS_FLASH_VFLA_UVLO_MASK                        0x1
#define MT6336_RG_INT_STATUS_FLASH_VFLA_UVLO_SHIFT                       4
#define MT6336_RG_INT_STATUS_FLASH_VFLA_OVP_ADDR                         MT6336_PMIC_INT_STATUS2
#define MT6336_RG_INT_STATUS_FLASH_VFLA_OVP_MASK                         0x1
#define MT6336_RG_INT_STATUS_FLASH_VFLA_OVP_SHIFT                        5
#define MT6336_RG_INT_STATUS_LED1_SHORT_ADDR                             MT6336_PMIC_INT_STATUS2
#define MT6336_RG_INT_STATUS_LED1_SHORT_MASK                             0x1
#define MT6336_RG_INT_STATUS_LED1_SHORT_SHIFT                            6
#define MT6336_RG_INT_STATUS_LED1_OPEN_ADDR                              MT6336_PMIC_INT_STATUS2
#define MT6336_RG_INT_STATUS_LED1_OPEN_MASK                              0x1
#define MT6336_RG_INT_STATUS_LED1_OPEN_SHIFT                             7
#define MT6336_RG_INT_STATUS_LED2_SHORT_ADDR                             MT6336_PMIC_INT_STATUS3
#define MT6336_RG_INT_STATUS_LED2_SHORT_MASK                             0x1
#define MT6336_RG_INT_STATUS_LED2_SHORT_SHIFT                            0
#define MT6336_RG_INT_STATUS_LED2_OPEN_ADDR                              MT6336_PMIC_INT_STATUS3
#define MT6336_RG_INT_STATUS_LED2_OPEN_MASK                              0x1
#define MT6336_RG_INT_STATUS_LED2_OPEN_SHIFT                             1
#define MT6336_RG_INT_STATUS_FLASH_TIMEOUT_ADDR                          MT6336_PMIC_INT_STATUS3
#define MT6336_RG_INT_STATUS_FLASH_TIMEOUT_MASK                          0x1
#define MT6336_RG_INT_STATUS_FLASH_TIMEOUT_SHIFT                         2
#define MT6336_RG_INT_STATUS_TORCH_TIMEOUT_ADDR                          MT6336_PMIC_INT_STATUS3
#define MT6336_RG_INT_STATUS_TORCH_TIMEOUT_MASK                          0x1
#define MT6336_RG_INT_STATUS_TORCH_TIMEOUT_SHIFT                         3
#define MT6336_RG_INT_STATUS_DD_VBUS_IN_VALID_ADDR                       MT6336_PMIC_INT_STATUS3
#define MT6336_RG_INT_STATUS_DD_VBUS_IN_VALID_MASK                       0x1
#define MT6336_RG_INT_STATUS_DD_VBUS_IN_VALID_SHIFT                      4
#define MT6336_RG_INT_STATUS_WDT_TIMEOUT_ADDR                            MT6336_PMIC_INT_STATUS3
#define MT6336_RG_INT_STATUS_WDT_TIMEOUT_MASK                            0x1
#define MT6336_RG_INT_STATUS_WDT_TIMEOUT_SHIFT                           5
#define MT6336_RG_INT_STATUS_SAFETY_TIMEOUT_ADDR                         MT6336_PMIC_INT_STATUS3
#define MT6336_RG_INT_STATUS_SAFETY_TIMEOUT_MASK                         0x1
#define MT6336_RG_INT_STATUS_SAFETY_TIMEOUT_SHIFT                        6
#define MT6336_RG_INT_STATUS_CHR_AICC_DONE_ADDR                          MT6336_PMIC_INT_STATUS3
#define MT6336_RG_INT_STATUS_CHR_AICC_DONE_MASK                          0x1
#define MT6336_RG_INT_STATUS_CHR_AICC_DONE_SHIFT                         7
#define MT6336_RG_INT_STATUS_ADC_TEMP_HT_ADDR                            MT6336_PMIC_INT_STATUS4
#define MT6336_RG_INT_STATUS_ADC_TEMP_HT_MASK                            0x1
#define MT6336_RG_INT_STATUS_ADC_TEMP_HT_SHIFT                           0
#define MT6336_RG_INT_STATUS_ADC_TEMP_LT_ADDR                            MT6336_PMIC_INT_STATUS4
#define MT6336_RG_INT_STATUS_ADC_TEMP_LT_MASK                            0x1
#define MT6336_RG_INT_STATUS_ADC_TEMP_LT_SHIFT                           1
#define MT6336_RG_INT_STATUS_ADC_JEITA_HOT_ADDR                          MT6336_PMIC_INT_STATUS4
#define MT6336_RG_INT_STATUS_ADC_JEITA_HOT_MASK                          0x1
#define MT6336_RG_INT_STATUS_ADC_JEITA_HOT_SHIFT                         2
#define MT6336_RG_INT_STATUS_ADC_JEITA_WARM_ADDR                         MT6336_PMIC_INT_STATUS4
#define MT6336_RG_INT_STATUS_ADC_JEITA_WARM_MASK                         0x1
#define MT6336_RG_INT_STATUS_ADC_JEITA_WARM_SHIFT                        3
#define MT6336_RG_INT_STATUS_ADC_JEITA_COOL_ADDR                         MT6336_PMIC_INT_STATUS4
#define MT6336_RG_INT_STATUS_ADC_JEITA_COOL_MASK                         0x1
#define MT6336_RG_INT_STATUS_ADC_JEITA_COOL_SHIFT                        4
#define MT6336_RG_INT_STATUS_ADC_JEITA_COLD_ADDR                         MT6336_PMIC_INT_STATUS4
#define MT6336_RG_INT_STATUS_ADC_JEITA_COLD_MASK                         0x1
#define MT6336_RG_INT_STATUS_ADC_JEITA_COLD_SHIFT                        5
#define MT6336_RG_INT_STATUS_VBUS_SOFT_OVP_H_ADDR                        MT6336_PMIC_INT_STATUS4
#define MT6336_RG_INT_STATUS_VBUS_SOFT_OVP_H_MASK                        0x1
#define MT6336_RG_INT_STATUS_VBUS_SOFT_OVP_H_SHIFT                       6
#define MT6336_RG_INT_STATUS_VBUS_SOFT_OVP_L_ADDR                        MT6336_PMIC_INT_STATUS4
#define MT6336_RG_INT_STATUS_VBUS_SOFT_OVP_L_MASK                        0x1
#define MT6336_RG_INT_STATUS_VBUS_SOFT_OVP_L_SHIFT                       7
#define MT6336_RG_INT_STATUS_CHR_BAT_RECHG_ADDR                          MT6336_PMIC_INT_STATUS5
#define MT6336_RG_INT_STATUS_CHR_BAT_RECHG_MASK                          0x1
#define MT6336_RG_INT_STATUS_CHR_BAT_RECHG_SHIFT                         0
#define MT6336_RG_INT_STATUS_BAT_TEMP_H_ADDR                             MT6336_PMIC_INT_STATUS5
#define MT6336_RG_INT_STATUS_BAT_TEMP_H_MASK                             0x1
#define MT6336_RG_INT_STATUS_BAT_TEMP_H_SHIFT                            1
#define MT6336_RG_INT_STATUS_BAT_TEMP_L_ADDR                             MT6336_PMIC_INT_STATUS5
#define MT6336_RG_INT_STATUS_BAT_TEMP_L_MASK                             0x1
#define MT6336_RG_INT_STATUS_BAT_TEMP_L_SHIFT                            2
#define MT6336_RG_INT_STATUS_TYPE_C_L_MIN_ADDR                           MT6336_PMIC_INT_STATUS5
#define MT6336_RG_INT_STATUS_TYPE_C_L_MIN_MASK                           0x1
#define MT6336_RG_INT_STATUS_TYPE_C_L_MIN_SHIFT                          3
#define MT6336_RG_INT_STATUS_TYPE_C_L_MAX_ADDR                           MT6336_PMIC_INT_STATUS5
#define MT6336_RG_INT_STATUS_TYPE_C_L_MAX_MASK                           0x1
#define MT6336_RG_INT_STATUS_TYPE_C_L_MAX_SHIFT                          4
#define MT6336_RG_INT_STATUS_TYPE_C_H_MIN_ADDR                           MT6336_PMIC_INT_STATUS5
#define MT6336_RG_INT_STATUS_TYPE_C_H_MIN_MASK                           0x1
#define MT6336_RG_INT_STATUS_TYPE_C_H_MIN_SHIFT                          5
#define MT6336_RG_INT_STATUS_TYPE_C_H_MAX_ADDR                           MT6336_PMIC_INT_STATUS5
#define MT6336_RG_INT_STATUS_TYPE_C_H_MAX_MASK                           0x1
#define MT6336_RG_INT_STATUS_TYPE_C_H_MAX_SHIFT                          6
#define MT6336_RG_INT_STATUS_TYPE_C_CC_IRQ_ADDR                          MT6336_PMIC_INT_STATUS5
#define MT6336_RG_INT_STATUS_TYPE_C_CC_IRQ_MASK                          0x1
#define MT6336_RG_INT_STATUS_TYPE_C_CC_IRQ_SHIFT                         7
#define MT6336_RG_INT_STATUS_TYPE_C_PD_IRQ_ADDR                          MT6336_PMIC_INT_STATUS6
#define MT6336_RG_INT_STATUS_TYPE_C_PD_IRQ_MASK                          0x1
#define MT6336_RG_INT_STATUS_TYPE_C_PD_IRQ_SHIFT                         0
#define MT6336_RG_INT_STATUS_DD_PE_STATUS_ADDR                           MT6336_PMIC_INT_STATUS6
#define MT6336_RG_INT_STATUS_DD_PE_STATUS_MASK                           0x1
#define MT6336_RG_INT_STATUS_DD_PE_STATUS_SHIFT                          1
#define MT6336_RG_INT_STATUS_BC12_V2P7_TIMEOUT_ADDR                      MT6336_PMIC_INT_STATUS6
#define MT6336_RG_INT_STATUS_BC12_V2P7_TIMEOUT_MASK                      0x1
#define MT6336_RG_INT_STATUS_BC12_V2P7_TIMEOUT_SHIFT                     2
#define MT6336_RG_INT_STATUS_BC12_V3P2_TIMEOUT_ADDR                      MT6336_PMIC_INT_STATUS6
#define MT6336_RG_INT_STATUS_BC12_V3P2_TIMEOUT_MASK                      0x1
#define MT6336_RG_INT_STATUS_BC12_V3P2_TIMEOUT_SHIFT                     3
#define MT6336_RG_INT_STATUS_DD_BC12_STATUS_ADDR                         MT6336_PMIC_INT_STATUS6
#define MT6336_RG_INT_STATUS_DD_BC12_STATUS_MASK                         0x1
#define MT6336_RG_INT_STATUS_DD_BC12_STATUS_SHIFT                        4
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_SW_ADDR                    MT6336_PMIC_INT_STATUS6
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_SW_MASK                    0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_SW_SHIFT                   5
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_GLOBAL_ADDR                MT6336_PMIC_INT_STATUS6
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_GLOBAL_MASK                0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_GLOBAL_SHIFT               6
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_LONG_PRESS_ADDR            MT6336_PMIC_INT_STATUS6
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_LONG_PRESS_MASK            0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_LONG_PRESS_SHIFT           7
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_WDT_ADDR                   MT6336_PMIC_INT_STATUS7
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_WDT_MASK                   0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_WDT_SHIFT                  0
#define MT6336_RG_INT_STATUS_DD_SWCHR_PLUGOUT_PULSEB_RISING_ADDR         MT6336_PMIC_INT_STATUS7
#define MT6336_RG_INT_STATUS_DD_SWCHR_PLUGOUT_PULSEB_RISING_MASK         0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_PLUGOUT_PULSEB_RISING_SHIFT        1
#define MT6336_RG_INT_STATUS_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_ADDR          MT6336_PMIC_INT_STATUS7
#define MT6336_RG_INT_STATUS_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_MASK          0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_SHIFT         2
#define MT6336_RG_INT_STATUS_DD_SWCHR_PLUGIN_PULSEB_ADDR                 MT6336_PMIC_INT_STATUS7
#define MT6336_RG_INT_STATUS_DD_SWCHR_PLUGIN_PULSEB_MASK                 0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_PLUGIN_PULSEB_SHIFT                3
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_SHIP_ADDR                  MT6336_PMIC_INT_STATUS7
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_SHIP_MASK                  0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_SHIP_SHIFT                 4
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_BAT_OC_ADDR                MT6336_PMIC_INT_STATUS7
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_BAT_OC_MASK                0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_BAT_OC_SHIFT               5
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_BAT_DEAD_ADDR              MT6336_PMIC_INT_STATUS7
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_BAT_DEAD_MASK              0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_BAT_DEAD_SHIFT             6
#define MT6336_RG_INT_STATUS_DD_SWCHR_BUCK_MODE_ADDR                     MT6336_PMIC_INT_STATUS7
#define MT6336_RG_INT_STATUS_DD_SWCHR_BUCK_MODE_MASK                     0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_BUCK_MODE_SHIFT                    7
#define MT6336_RG_INT_STATUS_DD_SWCHR_LOWQ_MODE_ADDR                     MT6336_PMIC_INT_STATUS8
#define MT6336_RG_INT_STATUS_DD_SWCHR_LOWQ_MODE_MASK                     0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_LOWQ_MODE_SHIFT                    0
#define MT6336_RG_INT_STATUS_DD_SWCHR_SHIP_MODE_ADDR                     MT6336_PMIC_INT_STATUS8
#define MT6336_RG_INT_STATUS_DD_SWCHR_SHIP_MODE_MASK                     0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_SHIP_MODE_SHIFT                    1
#define MT6336_RG_INT_STATUS_DD_SWCHR_BAT_OC_MODE_ADDR                   MT6336_PMIC_INT_STATUS8
#define MT6336_RG_INT_STATUS_DD_SWCHR_BAT_OC_MODE_MASK                   0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_BAT_OC_MODE_SHIFT                  2
#define MT6336_RG_INT_STATUS_DD_SWCHR_BAT_DEAD_MODE_ADDR                 MT6336_PMIC_INT_STATUS8
#define MT6336_RG_INT_STATUS_DD_SWCHR_BAT_DEAD_MODE_MASK                 0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_BAT_DEAD_MODE_SHIFT                3
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_SW_MODE_ADDR                   MT6336_PMIC_INT_STATUS8
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_SW_MODE_MASK                   0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_SW_MODE_SHIFT                  4
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_GLOBAL_MODE_ADDR               MT6336_PMIC_INT_STATUS8
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_GLOBAL_MODE_MASK               0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_GLOBAL_MODE_SHIFT              5
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_WDT_MODE_ADDR                  MT6336_PMIC_INT_STATUS8
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_WDT_MODE_MASK                  0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_WDT_MODE_SHIFT                 6
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_LONG_PRESS_MODE_ADDR           MT6336_PMIC_INT_STATUS8
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_LONG_PRESS_MODE_MASK           0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_RST_LONG_PRESS_MODE_SHIFT          7
#define MT6336_RG_INT_STATUS_DD_SWCHR_CHR_SUSPEND_STATE_ADDR             MT6336_PMIC_INT_STATUS9
#define MT6336_RG_INT_STATUS_DD_SWCHR_CHR_SUSPEND_STATE_MASK             0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_CHR_SUSPEND_STATE_SHIFT            0
#define MT6336_RG_INT_STATUS_DD_SWCHR_BUCK_PROTECT_STATE_ADDR            MT6336_PMIC_INT_STATUS9
#define MT6336_RG_INT_STATUS_DD_SWCHR_BUCK_PROTECT_STATE_MASK            0x1
#define MT6336_RG_INT_STATUS_DD_SWCHR_BUCK_PROTECT_STATE_SHIFT           1
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_PLUGIN_ADDR                    MT6336_PMIC_INT_RAW_STATUS0
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_PLUGIN_MASK                    0x1
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_PLUGIN_SHIFT                   0
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_PLUGOUT_ADDR                   MT6336_PMIC_INT_RAW_STATUS0
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_PLUGOUT_MASK                   0x1
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_PLUGOUT_SHIFT                  1
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_BACKGROUND_ADDR              MT6336_PMIC_INT_RAW_STATUS0
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_BACKGROUND_MASK              0x1
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_BACKGROUND_SHIFT             2
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_EOC_ADDR                     MT6336_PMIC_INT_RAW_STATUS0
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_EOC_MASK                     0x1
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_EOC_SHIFT                    3
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_PRECC0_ADDR                  MT6336_PMIC_INT_RAW_STATUS0
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_PRECC0_MASK                  0x1
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_PRECC0_SHIFT                 4
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_PRECC1_ADDR                  MT6336_PMIC_INT_RAW_STATUS0
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_PRECC1_MASK                  0x1
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_PRECC1_SHIFT                 5
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_FASTCC_ADDR                  MT6336_PMIC_INT_RAW_STATUS0
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_FASTCC_MASK                  0x1
#define MT6336_RG_INT_RAW_STATUS_STATE_BUCK_FASTCC_SHIFT                 6
#define MT6336_RG_INT_RAW_STATUS_CHR_WEAKBUS_ADDR                        MT6336_PMIC_INT_RAW_STATUS0
#define MT6336_RG_INT_RAW_STATUS_CHR_WEAKBUS_MASK                        0x1
#define MT6336_RG_INT_RAW_STATUS_CHR_WEAKBUS_SHIFT                       7
#define MT6336_RG_INT_RAW_STATUS_CHR_SYS_OVP_ADDR                        MT6336_PMIC_INT_RAW_STATUS1
#define MT6336_RG_INT_RAW_STATUS_CHR_SYS_OVP_MASK                        0x1
#define MT6336_RG_INT_RAW_STATUS_CHR_SYS_OVP_SHIFT                       0
#define MT6336_RG_INT_RAW_STATUS_CHR_BAT_OVP_ADDR                        MT6336_PMIC_INT_RAW_STATUS1
#define MT6336_RG_INT_RAW_STATUS_CHR_BAT_OVP_MASK                        0x1
#define MT6336_RG_INT_RAW_STATUS_CHR_BAT_OVP_SHIFT                       1
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_OVP_ADDR                       MT6336_PMIC_INT_RAW_STATUS1
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_OVP_MASK                       0x1
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_OVP_SHIFT                      2
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_UVLO_ADDR                      MT6336_PMIC_INT_RAW_STATUS1
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_UVLO_MASK                      0x1
#define MT6336_RG_INT_RAW_STATUS_CHR_VBUS_UVLO_SHIFT                     3
#define MT6336_RG_INT_RAW_STATUS_CHR_ICHR_ITERM_ADDR                     MT6336_PMIC_INT_RAW_STATUS1
#define MT6336_RG_INT_RAW_STATUS_CHR_ICHR_ITERM_MASK                     0x1
#define MT6336_RG_INT_RAW_STATUS_CHR_ICHR_ITERM_SHIFT                    4
#define MT6336_RG_INT_RAW_STATUS_CHIP_TEMP_OVERHEAT_ADDR                 MT6336_PMIC_INT_RAW_STATUS1
#define MT6336_RG_INT_RAW_STATUS_CHIP_TEMP_OVERHEAT_MASK                 0x1
#define MT6336_RG_INT_RAW_STATUS_CHIP_TEMP_OVERHEAT_SHIFT                5
#define MT6336_RG_INT_RAW_STATUS_CHIP_MBATPP_DIS_OC_DIG_ADDR             MT6336_PMIC_INT_RAW_STATUS1
#define MT6336_RG_INT_RAW_STATUS_CHIP_MBATPP_DIS_OC_DIG_MASK             0x1
#define MT6336_RG_INT_RAW_STATUS_CHIP_MBATPP_DIS_OC_DIG_SHIFT            6
#define MT6336_RG_INT_RAW_STATUS_OTG_BVALID_ADDR                         MT6336_PMIC_INT_RAW_STATUS1
#define MT6336_RG_INT_RAW_STATUS_OTG_BVALID_MASK                         0x1
#define MT6336_RG_INT_RAW_STATUS_OTG_BVALID_SHIFT                        7
#define MT6336_RG_INT_RAW_STATUS_OTG_VM_UVLO_ADDR                        MT6336_PMIC_INT_RAW_STATUS2
#define MT6336_RG_INT_RAW_STATUS_OTG_VM_UVLO_MASK                        0x1
#define MT6336_RG_INT_RAW_STATUS_OTG_VM_UVLO_SHIFT                       0
#define MT6336_RG_INT_RAW_STATUS_OTG_VM_OVP_ADDR                         MT6336_PMIC_INT_RAW_STATUS2
#define MT6336_RG_INT_RAW_STATUS_OTG_VM_OVP_MASK                         0x1
#define MT6336_RG_INT_RAW_STATUS_OTG_VM_OVP_SHIFT                        1
#define MT6336_RG_INT_RAW_STATUS_OTG_VBAT_UVLO_ADDR                      MT6336_PMIC_INT_RAW_STATUS2
#define MT6336_RG_INT_RAW_STATUS_OTG_VBAT_UVLO_MASK                      0x1
#define MT6336_RG_INT_RAW_STATUS_OTG_VBAT_UVLO_SHIFT                     2
#define MT6336_RG_INT_RAW_STATUS_OTG_VM_OLP_ADDR                         MT6336_PMIC_INT_RAW_STATUS2
#define MT6336_RG_INT_RAW_STATUS_OTG_VM_OLP_MASK                         0x1
#define MT6336_RG_INT_RAW_STATUS_OTG_VM_OLP_SHIFT                        3
#define MT6336_RG_INT_RAW_STATUS_FLASH_VFLA_UVLO_ADDR                    MT6336_PMIC_INT_RAW_STATUS2
#define MT6336_RG_INT_RAW_STATUS_FLASH_VFLA_UVLO_MASK                    0x1
#define MT6336_RG_INT_RAW_STATUS_FLASH_VFLA_UVLO_SHIFT                   4
#define MT6336_RG_INT_RAW_STATUS_FLASH_VFLA_OVP_ADDR                     MT6336_PMIC_INT_RAW_STATUS2
#define MT6336_RG_INT_RAW_STATUS_FLASH_VFLA_OVP_MASK                     0x1
#define MT6336_RG_INT_RAW_STATUS_FLASH_VFLA_OVP_SHIFT                    5
#define MT6336_RG_INT_RAW_STATUS_LED1_SHORT_ADDR                         MT6336_PMIC_INT_RAW_STATUS2
#define MT6336_RG_INT_RAW_STATUS_LED1_SHORT_MASK                         0x1
#define MT6336_RG_INT_RAW_STATUS_LED1_SHORT_SHIFT                        6
#define MT6336_RG_INT_RAW_STATUS_LED1_OPEN_ADDR                          MT6336_PMIC_INT_RAW_STATUS2
#define MT6336_RG_INT_RAW_STATUS_LED1_OPEN_MASK                          0x1
#define MT6336_RG_INT_RAW_STATUS_LED1_OPEN_SHIFT                         7
#define MT6336_RG_INT_RAW_STATUS_LED2_SHORT_ADDR                         MT6336_PMIC_INT_RAW_STATUS3
#define MT6336_RG_INT_RAW_STATUS_LED2_SHORT_MASK                         0x1
#define MT6336_RG_INT_RAW_STATUS_LED2_SHORT_SHIFT                        0
#define MT6336_RG_INT_RAW_STATUS_LED2_OPEN_ADDR                          MT6336_PMIC_INT_RAW_STATUS3
#define MT6336_RG_INT_RAW_STATUS_LED2_OPEN_MASK                          0x1
#define MT6336_RG_INT_RAW_STATUS_LED2_OPEN_SHIFT                         1
#define MT6336_RG_INT_RAW_STATUS_FLASH_TIMEOUT_ADDR                      MT6336_PMIC_INT_RAW_STATUS3
#define MT6336_RG_INT_RAW_STATUS_FLASH_TIMEOUT_MASK                      0x1
#define MT6336_RG_INT_RAW_STATUS_FLASH_TIMEOUT_SHIFT                     2
#define MT6336_RG_INT_RAW_STATUS_TORCH_TIMEOUT_ADDR                      MT6336_PMIC_INT_RAW_STATUS3
#define MT6336_RG_INT_RAW_STATUS_TORCH_TIMEOUT_MASK                      0x1
#define MT6336_RG_INT_RAW_STATUS_TORCH_TIMEOUT_SHIFT                     3
#define MT6336_RG_INT_RAW_STATUS_DD_VBUS_IN_VALID_ADDR                   MT6336_PMIC_INT_RAW_STATUS3
#define MT6336_RG_INT_RAW_STATUS_DD_VBUS_IN_VALID_MASK                   0x1
#define MT6336_RG_INT_RAW_STATUS_DD_VBUS_IN_VALID_SHIFT                  4
#define MT6336_RG_INT_RAW_STATUS_WDT_TIMEOUT_ADDR                        MT6336_PMIC_INT_RAW_STATUS3
#define MT6336_RG_INT_RAW_STATUS_WDT_TIMEOUT_MASK                        0x1
#define MT6336_RG_INT_RAW_STATUS_WDT_TIMEOUT_SHIFT                       5
#define MT6336_RG_INT_RAW_STATUS_SAFETY_TIMEOUT_ADDR                     MT6336_PMIC_INT_RAW_STATUS3
#define MT6336_RG_INT_RAW_STATUS_SAFETY_TIMEOUT_MASK                     0x1
#define MT6336_RG_INT_RAW_STATUS_SAFETY_TIMEOUT_SHIFT                    6
#define MT6336_RG_INT_RAW_STATUS_CHR_AICC_DONE_ADDR                      MT6336_PMIC_INT_RAW_STATUS3
#define MT6336_RG_INT_RAW_STATUS_CHR_AICC_DONE_MASK                      0x1
#define MT6336_RG_INT_RAW_STATUS_CHR_AICC_DONE_SHIFT                     7
#define MT6336_RG_INT_RAW_STATUS_ADC_TEMP_HT_ADDR                        MT6336_PMIC_INT_RAW_STATUS4
#define MT6336_RG_INT_RAW_STATUS_ADC_TEMP_HT_MASK                        0x1
#define MT6336_RG_INT_RAW_STATUS_ADC_TEMP_HT_SHIFT                       0
#define MT6336_RG_INT_RAW_STATUS_ADC_TEMP_LT_ADDR                        MT6336_PMIC_INT_RAW_STATUS4
#define MT6336_RG_INT_RAW_STATUS_ADC_TEMP_LT_MASK                        0x1
#define MT6336_RG_INT_RAW_STATUS_ADC_TEMP_LT_SHIFT                       1
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_HOT_ADDR                      MT6336_PMIC_INT_RAW_STATUS4
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_HOT_MASK                      0x1
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_HOT_SHIFT                     2
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_WARM_ADDR                     MT6336_PMIC_INT_RAW_STATUS4
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_WARM_MASK                     0x1
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_WARM_SHIFT                    3
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_COOL_ADDR                     MT6336_PMIC_INT_RAW_STATUS4
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_COOL_MASK                     0x1
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_COOL_SHIFT                    4
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_COLD_ADDR                     MT6336_PMIC_INT_RAW_STATUS4
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_COLD_MASK                     0x1
#define MT6336_RG_INT_RAW_STATUS_ADC_JEITA_COLD_SHIFT                    5
#define MT6336_RG_INT_RAW_STATUS_VBUS_SOFT_OVP_H_ADDR                    MT6336_PMIC_INT_RAW_STATUS4
#define MT6336_RG_INT_RAW_STATUS_VBUS_SOFT_OVP_H_MASK                    0x1
#define MT6336_RG_INT_RAW_STATUS_VBUS_SOFT_OVP_H_SHIFT                   6
#define MT6336_RG_INT_RAW_STATUS_VBUS_SOFT_OVP_L_ADDR                    MT6336_PMIC_INT_RAW_STATUS4
#define MT6336_RG_INT_RAW_STATUS_VBUS_SOFT_OVP_L_MASK                    0x1
#define MT6336_RG_INT_RAW_STATUS_VBUS_SOFT_OVP_L_SHIFT                   7
#define MT6336_RG_INT_RAW_STATUS_CHR_BAT_RECHG_ADDR                      MT6336_PMIC_INT_RAW_STATUS5
#define MT6336_RG_INT_RAW_STATUS_CHR_BAT_RECHG_MASK                      0x1
#define MT6336_RG_INT_RAW_STATUS_CHR_BAT_RECHG_SHIFT                     0
#define MT6336_RG_INT_RAW_STATUS_BAT_TEMP_H_ADDR                         MT6336_PMIC_INT_RAW_STATUS5
#define MT6336_RG_INT_RAW_STATUS_BAT_TEMP_H_MASK                         0x1
#define MT6336_RG_INT_RAW_STATUS_BAT_TEMP_H_SHIFT                        1
#define MT6336_RG_INT_RAW_STATUS_BAT_TEMP_L_ADDR                         MT6336_PMIC_INT_RAW_STATUS5
#define MT6336_RG_INT_RAW_STATUS_BAT_TEMP_L_MASK                         0x1
#define MT6336_RG_INT_RAW_STATUS_BAT_TEMP_L_SHIFT                        2
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_L_MIN_ADDR                       MT6336_PMIC_INT_RAW_STATUS5
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_L_MIN_MASK                       0x1
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_L_MIN_SHIFT                      3
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_L_MAX_ADDR                       MT6336_PMIC_INT_RAW_STATUS5
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_L_MAX_MASK                       0x1
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_L_MAX_SHIFT                      4
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_H_MIN_ADDR                       MT6336_PMIC_INT_RAW_STATUS5
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_H_MIN_MASK                       0x1
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_H_MIN_SHIFT                      5
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_H_MAX_ADDR                       MT6336_PMIC_INT_RAW_STATUS5
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_H_MAX_MASK                       0x1
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_H_MAX_SHIFT                      6
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_CC_IRQ_ADDR                      MT6336_PMIC_INT_RAW_STATUS5
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_CC_IRQ_MASK                      0x1
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_CC_IRQ_SHIFT                     7
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_PD_IRQ_ADDR                      MT6336_PMIC_INT_RAW_STATUS6
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_PD_IRQ_MASK                      0x1
#define MT6336_RG_INT_RAW_STATUS_TYPE_C_PD_IRQ_SHIFT                     0
#define MT6336_RG_INT_RAW_STATUS_DD_PE_STATUS_ADDR                       MT6336_PMIC_INT_RAW_STATUS6
#define MT6336_RG_INT_RAW_STATUS_DD_PE_STATUS_MASK                       0x1
#define MT6336_RG_INT_RAW_STATUS_DD_PE_STATUS_SHIFT                      1
#define MT6336_RG_INT_RAW_STATUS_BC12_V2P7_TIMEOUT_ADDR                  MT6336_PMIC_INT_RAW_STATUS6
#define MT6336_RG_INT_RAW_STATUS_BC12_V2P7_TIMEOUT_MASK                  0x1
#define MT6336_RG_INT_RAW_STATUS_BC12_V2P7_TIMEOUT_SHIFT                 2
#define MT6336_RG_INT_RAW_STATUS_BC12_V3P2_TIMEOUT_ADDR                  MT6336_PMIC_INT_RAW_STATUS6
#define MT6336_RG_INT_RAW_STATUS_BC12_V3P2_TIMEOUT_MASK                  0x1
#define MT6336_RG_INT_RAW_STATUS_BC12_V3P2_TIMEOUT_SHIFT                 3
#define MT6336_RG_INT_RAW_STATUS_DD_BC12_STATUS_ADDR                     MT6336_PMIC_INT_RAW_STATUS6
#define MT6336_RG_INT_RAW_STATUS_DD_BC12_STATUS_MASK                     0x1
#define MT6336_RG_INT_RAW_STATUS_DD_BC12_STATUS_SHIFT                    4
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_SW_ADDR                MT6336_PMIC_INT_RAW_STATUS6
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_SW_MASK                0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_SW_SHIFT               5
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_GLOBAL_ADDR            MT6336_PMIC_INT_RAW_STATUS6
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_GLOBAL_MASK            0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_GLOBAL_SHIFT           6
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_LONG_PRESS_ADDR        MT6336_PMIC_INT_RAW_STATUS6
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_LONG_PRESS_MASK        0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_LONG_PRESS_SHIFT       7
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_WDT_ADDR               MT6336_PMIC_INT_RAW_STATUS7
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_WDT_MASK               0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_WDT_SHIFT              0
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGOUT_PULSEB_RISING_ADDR     MT6336_PMIC_INT_RAW_STATUS7
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGOUT_PULSEB_RISING_MASK     0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGOUT_PULSEB_RISING_SHIFT    1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_ADDR      MT6336_PMIC_INT_RAW_STATUS7
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_MASK      0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGOUT_PULSEB_LEVEL_SHIFT     2
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGIN_PULSEB_ADDR             MT6336_PMIC_INT_RAW_STATUS7
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGIN_PULSEB_MASK             0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGIN_PULSEB_SHIFT            3
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_SHIP_ADDR              MT6336_PMIC_INT_RAW_STATUS7
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_SHIP_MASK              0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_SHIP_SHIFT             4
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_BAT_OC_ADDR            MT6336_PMIC_INT_RAW_STATUS7
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_BAT_OC_MASK            0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_BAT_OC_SHIFT           5
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_BAT_DEAD_ADDR          MT6336_PMIC_INT_RAW_STATUS7
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_BAT_DEAD_MASK          0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_BAT_DEAD_SHIFT         6
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BUCK_MODE_ADDR                 MT6336_PMIC_INT_RAW_STATUS7
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BUCK_MODE_MASK                 0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BUCK_MODE_SHIFT                7
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_LOWQ_MODE_ADDR                 MT6336_PMIC_INT_RAW_STATUS8
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_LOWQ_MODE_MASK                 0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_LOWQ_MODE_SHIFT                0
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_SHIP_MODE_ADDR                 MT6336_PMIC_INT_RAW_STATUS8
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_SHIP_MODE_MASK                 0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_SHIP_MODE_SHIFT                1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BAT_OC_MODE_ADDR               MT6336_PMIC_INT_RAW_STATUS8
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BAT_OC_MODE_MASK               0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BAT_OC_MODE_SHIFT              2
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BAT_DEAD_MODE_ADDR             MT6336_PMIC_INT_RAW_STATUS8
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BAT_DEAD_MODE_MASK             0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BAT_DEAD_MODE_SHIFT            3
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_SW_MODE_ADDR               MT6336_PMIC_INT_RAW_STATUS8
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_SW_MODE_MASK               0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_SW_MODE_SHIFT              4
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_GLOBAL_MODE_ADDR           MT6336_PMIC_INT_RAW_STATUS8
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_GLOBAL_MODE_MASK           0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_GLOBAL_MODE_SHIFT          5
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_WDT_MODE_ADDR              MT6336_PMIC_INT_RAW_STATUS8
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_WDT_MODE_MASK              0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_WDT_MODE_SHIFT             6
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_LONG_PRESS_MODE_ADDR       MT6336_PMIC_INT_RAW_STATUS8
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_LONG_PRESS_MODE_MASK       0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_LONG_PRESS_MODE_SHIFT      7
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_CHR_SUSPEND_STATE_ADDR         MT6336_PMIC_INT_RAW_STATUS9
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_CHR_SUSPEND_STATE_MASK         0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_CHR_SUSPEND_STATE_SHIFT        0
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BUCK_PROTECT_STATE_ADDR        MT6336_PMIC_INT_RAW_STATUS9
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BUCK_PROTECT_STATE_MASK        0x1
#define MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BUCK_PROTECT_STATE_SHIFT       1
#define MT6336_FQMTR_TCKSEL_ADDR                                         MT6336_PMIC_FQMTR_CON0
#define MT6336_FQMTR_TCKSEL_MASK                                         0x7
#define MT6336_FQMTR_TCKSEL_SHIFT                                        0
#define MT6336_FQMTR_BUSY_ADDR                                           MT6336_PMIC_FQMTR_CON0
#define MT6336_FQMTR_BUSY_MASK                                           0x1
#define MT6336_FQMTR_BUSY_SHIFT                                          3
#define MT6336_FQMTR_EN_ADDR                                             MT6336_PMIC_FQMTR_CON0
#define MT6336_FQMTR_EN_MASK                                             0x1
#define MT6336_FQMTR_EN_SHIFT                                            7
#define MT6336_FQMTR_WINSET_L_ADDR                                       MT6336_PMIC_FQMTR_WIN_L
#define MT6336_FQMTR_WINSET_L_MASK                                       0xFF
#define MT6336_FQMTR_WINSET_L_SHIFT                                      0
#define MT6336_FQMTR_WINSET_U_ADDR                                       MT6336_PMIC_FQMTR_WIN_U
#define MT6336_FQMTR_WINSET_U_MASK                                       0xFF
#define MT6336_FQMTR_WINSET_U_SHIFT                                      0
#define MT6336_FQMTR_DATA_L_ADDR                                         MT6336_PMIC_FQMTR_DAT_L
#define MT6336_FQMTR_DATA_L_MASK                                         0xFF
#define MT6336_FQMTR_DATA_L_SHIFT                                        0
#define MT6336_FQMTR_DATA_U_ADDR                                         MT6336_PMIC_FQMTR_DAT_U
#define MT6336_FQMTR_DATA_U_MASK                                         0xFF
#define MT6336_FQMTR_DATA_U_SHIFT                                        0
#define MT6336_TOP_RSV3_ADDR                                             MT6336_PMIC_TOP_RSV3
#define MT6336_TOP_RSV3_MASK                                             0xFF
#define MT6336_TOP_RSV3_SHIFT                                            0
#define MT6336_TOP_RSV4_ADDR                                             MT6336_PMIC_TOP_RSV4
#define MT6336_TOP_RSV4_MASK                                             0xFF
#define MT6336_TOP_RSV4_SHIFT                                            0
#define MT6336_TOP_RSV5_ADDR                                             MT6336_PMIC_TOP_RSV5
#define MT6336_TOP_RSV5_MASK                                             0xFF
#define MT6336_TOP_RSV5_SHIFT                                            0
#define MT6336_TOP_RSV6_ADDR                                             MT6336_PMIC_TOP_RSV6
#define MT6336_TOP_RSV6_MASK                                             0xFF
#define MT6336_TOP_RSV6_SHIFT                                            0
#define MT6336_TOP_RSV7_ADDR                                             MT6336_PMIC_TOP_RSV7
#define MT6336_TOP_RSV7_MASK                                             0xFF
#define MT6336_TOP_RSV7_SHIFT                                            0
#define MT6336_TOP_RSV8_ADDR                                             MT6336_PMIC_TOP_RSV8
#define MT6336_TOP_RSV8_MASK                                             0xFF
#define MT6336_TOP_RSV8_SHIFT                                            0
#define MT6336_TOP_RSV9_ADDR                                             MT6336_PMIC_TOP_RSV9
#define MT6336_TOP_RSV9_MASK                                             0xFF
#define MT6336_TOP_RSV9_SHIFT                                            0
#define MT6336_TOP_RSV10_ADDR                                            MT6336_PMIC_TOP_RSV10
#define MT6336_TOP_RSV10_MASK                                            0xFF
#define MT6336_TOP_RSV10_SHIFT                                           0
#define MT6336_TOP_RSV11_ADDR                                            MT6336_PMIC_TOP_RSV11
#define MT6336_TOP_RSV11_MASK                                            0xFF
#define MT6336_TOP_RSV11_SHIFT                                           0
#define MT6336_TOP_RSV12_ADDR                                            MT6336_PMIC_TOP_RSV12
#define MT6336_TOP_RSV12_MASK                                            0xFF
#define MT6336_TOP_RSV12_SHIFT                                           0
#define MT6336_TOP_RSV13_ADDR                                            MT6336_PMIC_TOP_RSV13
#define MT6336_TOP_RSV13_MASK                                            0xFF
#define MT6336_TOP_RSV13_SHIFT                                           0
#define MT6336_TOP_RSV14_ADDR                                            MT6336_PMIC_TOP_RSV14
#define MT6336_TOP_RSV14_MASK                                            0xFF
#define MT6336_TOP_RSV14_SHIFT                                           0
#define MT6336_TOP_RSV15_ADDR                                            MT6336_PMIC_TOP_RSV15
#define MT6336_TOP_RSV15_MASK                                            0xFF
#define MT6336_TOP_RSV15_SHIFT                                           0
#define MT6336_RG_A_ICHRSTAT_TRIM_EN_ADDR                                MT6336_PMIC_ISINKA_ANA_CON_0
#define MT6336_RG_A_ICHRSTAT_TRIM_EN_MASK                                0x1
#define MT6336_RG_A_ICHRSTAT_TRIM_EN_SHIFT                               0
#define MT6336_RG_A_ICHRSTAT_TRIM_SEL_ADDR                               MT6336_PMIC_ISINKA_ANA_CON_0
#define MT6336_RG_A_ICHRSTAT_TRIM_SEL_MASK                               0x7
#define MT6336_RG_A_ICHRSTAT_TRIM_SEL_SHIFT                              1
#define MT6336_RG_A_ICHRSTAT_RSV_ADDR                                    MT6336_PMIC_ISINK_CON0
#define MT6336_RG_A_ICHRSTAT_RSV_MASK                                    0xFF
#define MT6336_RG_A_ICHRSTAT_RSV_SHIFT                                   0
#define MT6336_CHRIND_DIM_FSEL_ADDR                                      MT6336_PMIC_CHRIND_CON0
#define MT6336_CHRIND_DIM_FSEL_MASK                                      0xFF
#define MT6336_CHRIND_DIM_FSEL_SHIFT                                     0
#define MT6336_CHRIND_DIM_FSEL_1_ADDR                                    MT6336_PMIC_CHRIND_CON0_1
#define MT6336_CHRIND_DIM_FSEL_1_MASK                                    0xFF
#define MT6336_CHRIND_DIM_FSEL_1_SHIFT                                   0
#define MT6336_CHRIND_DIM_DUTY_ADDR                                      MT6336_PMIC_CHRIND_CON1
#define MT6336_CHRIND_DIM_DUTY_MASK                                      0x1F
#define MT6336_CHRIND_DIM_DUTY_SHIFT                                     0
#define MT6336_CHRIND_STEP_ADDR                                          MT6336_PMIC_CHRIND_CON1
#define MT6336_CHRIND_STEP_MASK                                          0x7
#define MT6336_CHRIND_STEP_SHIFT                                         5
#define MT6336_CHRIND_RSV1_ADDR                                          MT6336_PMIC_CHRIND_CON1_1
#define MT6336_CHRIND_RSV1_MASK                                          0xF
#define MT6336_CHRIND_RSV1_SHIFT                                         0
#define MT6336_CHRIND_RSV0_ADDR                                          MT6336_PMIC_CHRIND_CON1_1
#define MT6336_CHRIND_RSV0_MASK                                          0x7
#define MT6336_CHRIND_RSV0_SHIFT                                         4
#define MT6336_CHRIND_BREATH_TR2_SEL_ADDR                                MT6336_PMIC_CHRIND_CON2
#define MT6336_CHRIND_BREATH_TR2_SEL_MASK                                0xF
#define MT6336_CHRIND_BREATH_TR2_SEL_SHIFT                               0
#define MT6336_CHRIND_BREATH_TR1_SEL_ADDR                                MT6336_PMIC_CHRIND_CON2
#define MT6336_CHRIND_BREATH_TR1_SEL_MASK                                0xF
#define MT6336_CHRIND_BREATH_TR1_SEL_SHIFT                               4
#define MT6336_CHRIND_BREATH_TF2_SEL_ADDR                                MT6336_PMIC_CHRIND_CON2_1
#define MT6336_CHRIND_BREATH_TF2_SEL_MASK                                0xF
#define MT6336_CHRIND_BREATH_TF2_SEL_SHIFT                               0
#define MT6336_CHRIND_BREATH_TF1_SEL_ADDR                                MT6336_PMIC_CHRIND_CON2_1
#define MT6336_CHRIND_BREATH_TF1_SEL_MASK                                0xF
#define MT6336_CHRIND_BREATH_TF1_SEL_SHIFT                               4
#define MT6336_CHRIND_BREATH_TOFF_SEL_ADDR                               MT6336_PMIC_CHRIND_CON3
#define MT6336_CHRIND_BREATH_TOFF_SEL_MASK                               0xF
#define MT6336_CHRIND_BREATH_TOFF_SEL_SHIFT                              0
#define MT6336_CHRIND_BREATH_TON_SEL_ADDR                                MT6336_PMIC_CHRIND_CON3
#define MT6336_CHRIND_BREATH_TON_SEL_MASK                                0xF
#define MT6336_CHRIND_BREATH_TON_SEL_SHIFT                               4
#define MT6336_CHRIND_CHOP_EN_ADDR                                       MT6336_PMIC_CHRIND_EN_CTRL
#define MT6336_CHRIND_CHOP_EN_MASK                                       0x1
#define MT6336_CHRIND_CHOP_EN_SHIFT                                      0
#define MT6336_CHRIND_MODE_ADDR                                          MT6336_PMIC_CHRIND_EN_CTRL
#define MT6336_CHRIND_MODE_MASK                                          0x3
#define MT6336_CHRIND_MODE_SHIFT                                         1
#define MT6336_CHRIND_CHOP_SW_ADDR                                       MT6336_PMIC_CHRIND_EN_CTRL
#define MT6336_CHRIND_CHOP_SW_MASK                                       0x1
#define MT6336_CHRIND_CHOP_SW_SHIFT                                      3
#define MT6336_CHRIND_BIAS_EN_ADDR                                       MT6336_PMIC_CHRIND_EN_CTRL
#define MT6336_CHRIND_BIAS_EN_MASK                                       0x1
#define MT6336_CHRIND_BIAS_EN_SHIFT                                      4
#define MT6336_CHRIND_SFSTR_EN_ADDR                                      MT6336_PMIC_CHRIND_EN_CTRL_1
#define MT6336_CHRIND_SFSTR_EN_MASK                                      0x1
#define MT6336_CHRIND_SFSTR_EN_SHIFT                                     0
#define MT6336_CHRIND_SFSTR_TC_ADDR                                      MT6336_PMIC_CHRIND_EN_CTRL_1
#define MT6336_CHRIND_SFSTR_TC_MASK                                      0x3
#define MT6336_CHRIND_SFSTR_TC_SHIFT                                     1
#define MT6336_CHRIND_EN_SEL_ADDR                                        MT6336_PMIC_CHRIND_EN_CTRL_1
#define MT6336_CHRIND_EN_SEL_MASK                                        0x1
#define MT6336_CHRIND_EN_SEL_SHIFT                                       3
#define MT6336_CHRIND_EN_ADDR                                            MT6336_PMIC_CHRIND_EN_CTRL_1
#define MT6336_CHRIND_EN_MASK                                            0x1
#define MT6336_CHRIND_EN_SHIFT                                           4
#define MT6336_ISINK_RSV_ADDR                                            MT6336_PMIC_CHRIND_EN_CTRL_2
#define MT6336_ISINK_RSV_MASK                                            0x1F
#define MT6336_ISINK_RSV_SHIFT                                           0
#define MT6336_TYPE_C_PHY_RG_CC_RP_SEL_ADDR                              MT6336_PMIC_TYPE_C_PHY_RG_0_0
#define MT6336_TYPE_C_PHY_RG_CC_RP_SEL_MASK                              0x3
#define MT6336_TYPE_C_PHY_RG_CC_RP_SEL_SHIFT                             0
#define MT6336_REG_TYPE_C_PHY_RG_CC_V2I_BYPASS_ADDR                      MT6336_PMIC_TYPE_C_PHY_RG_0_0
#define MT6336_REG_TYPE_C_PHY_RG_CC_V2I_BYPASS_MASK                      0x1
#define MT6336_REG_TYPE_C_PHY_RG_CC_V2I_BYPASS_SHIFT                     2
#define MT6336_REG_TYPE_C_PHY_RG_CC_MPX_SEL_ADDR                         MT6336_PMIC_TYPE_C_PHY_RG_0_1
#define MT6336_REG_TYPE_C_PHY_RG_CC_MPX_SEL_MASK                         0xFF
#define MT6336_REG_TYPE_C_PHY_RG_CC_MPX_SEL_SHIFT                        0
#define MT6336_REG_TYPE_C_PHY_RG_CC_RESERVE_ADDR                         MT6336_PMIC_TYPE_C_PHY_RG_CC_RESERVE_CSR
#define MT6336_REG_TYPE_C_PHY_RG_CC_RESERVE_MASK                         0xFF
#define MT6336_REG_TYPE_C_PHY_RG_CC_RESERVE_SHIFT                        0
#define MT6336_REG_TYPE_C_VCMP_CC2_SW_SEL_ST_CNT_VAL_ADDR                MT6336_PMIC_TYPE_C_VCMP_CTRL
#define MT6336_REG_TYPE_C_VCMP_CC2_SW_SEL_ST_CNT_VAL_MASK                0x1F
#define MT6336_REG_TYPE_C_VCMP_CC2_SW_SEL_ST_CNT_VAL_SHIFT               0
#define MT6336_REG_TYPE_C_VCMP_BIAS_EN_ST_CNT_VAL_ADDR                   MT6336_PMIC_TYPE_C_VCMP_CTRL
#define MT6336_REG_TYPE_C_VCMP_BIAS_EN_ST_CNT_VAL_MASK                   0x3
#define MT6336_REG_TYPE_C_VCMP_BIAS_EN_ST_CNT_VAL_SHIFT                  5
#define MT6336_REG_TYPE_C_VCMP_DAC_EN_ST_CNT_VAL_ADDR                    MT6336_PMIC_TYPE_C_VCMP_CTRL
#define MT6336_REG_TYPE_C_VCMP_DAC_EN_ST_CNT_VAL_MASK                    0x1
#define MT6336_REG_TYPE_C_VCMP_DAC_EN_ST_CNT_VAL_SHIFT                   7
#define MT6336_REG_TYPE_C_PORT_SUPPORT_ROLE_ADDR                         MT6336_PMIC_TYPE_C_CTRL_0
#define MT6336_REG_TYPE_C_PORT_SUPPORT_ROLE_MASK                         0x3
#define MT6336_REG_TYPE_C_PORT_SUPPORT_ROLE_SHIFT                        0
#define MT6336_REG_TYPE_C_ADC_EN_ADDR                                    MT6336_PMIC_TYPE_C_CTRL_0
#define MT6336_REG_TYPE_C_ADC_EN_MASK                                    0x1
#define MT6336_REG_TYPE_C_ADC_EN_SHIFT                                   2
#define MT6336_REG_TYPE_C_ACC_EN_ADDR                                    MT6336_PMIC_TYPE_C_CTRL_0
#define MT6336_REG_TYPE_C_ACC_EN_MASK                                    0x1
#define MT6336_REG_TYPE_C_ACC_EN_SHIFT                                   3
#define MT6336_REG_TYPE_C_AUDIO_ACC_EN_ADDR                              MT6336_PMIC_TYPE_C_CTRL_0
#define MT6336_REG_TYPE_C_AUDIO_ACC_EN_MASK                              0x1
#define MT6336_REG_TYPE_C_AUDIO_ACC_EN_SHIFT                             4
#define MT6336_REG_TYPE_C_DEBUG_ACC_EN_ADDR                              MT6336_PMIC_TYPE_C_CTRL_0
#define MT6336_REG_TYPE_C_DEBUG_ACC_EN_MASK                              0x1
#define MT6336_REG_TYPE_C_DEBUG_ACC_EN_SHIFT                             5
#define MT6336_REG_TYPE_C_TRY_SRC_ST_EN_ADDR                             MT6336_PMIC_TYPE_C_CTRL_0
#define MT6336_REG_TYPE_C_TRY_SRC_ST_EN_MASK                             0x1
#define MT6336_REG_TYPE_C_TRY_SRC_ST_EN_SHIFT                            6
#define MT6336_REG_TYPE_C_ATTACH_SRC_2_TRY_WAIT_SNK_ST_EN_ADDR           MT6336_PMIC_TYPE_C_CTRL_0
#define MT6336_REG_TYPE_C_ATTACH_SRC_2_TRY_WAIT_SNK_ST_EN_MASK           0x1
#define MT6336_REG_TYPE_C_ATTACH_SRC_2_TRY_WAIT_SNK_ST_EN_SHIFT          7
#define MT6336_REG_TYPE_C_PD2CC_DET_DISABLE_EN_ADDR                      MT6336_PMIC_TYPE_C_CTRL_1
#define MT6336_REG_TYPE_C_PD2CC_DET_DISABLE_EN_MASK                      0x1
#define MT6336_REG_TYPE_C_PD2CC_DET_DISABLE_EN_SHIFT                     0
#define MT6336_REG_TYPE_C_ATTACH_SRC_OPEN_PDDEBOUNCE_EN_ADDR             MT6336_PMIC_TYPE_C_CTRL_1
#define MT6336_REG_TYPE_C_ATTACH_SRC_OPEN_PDDEBOUNCE_EN_MASK             0x1
#define MT6336_REG_TYPE_C_ATTACH_SRC_OPEN_PDDEBOUNCE_EN_SHIFT            1
#define MT6336_REG_TYPE_C_DISABLE_ST_RD_EN_ADDR                          MT6336_PMIC_TYPE_C_CTRL_1
#define MT6336_REG_TYPE_C_DISABLE_ST_RD_EN_MASK                          0x1
#define MT6336_REG_TYPE_C_DISABLE_ST_RD_EN_SHIFT                         2
#define MT6336_REG_TYPE_C_DA_CC_SACLK_SW_EN_ADDR                         MT6336_PMIC_TYPE_C_CTRL_1
#define MT6336_REG_TYPE_C_DA_CC_SACLK_SW_EN_MASK                         0x1
#define MT6336_REG_TYPE_C_DA_CC_SACLK_SW_EN_SHIFT                        3
#define MT6336_W1_TYPE_C_SW_ENT_DISABLE_CMD_ADDR                         MT6336_PMIC_TYPE_C_CC_SW_CTRL_0
#define MT6336_W1_TYPE_C_SW_ENT_DISABLE_CMD_MASK                         0x1
#define MT6336_W1_TYPE_C_SW_ENT_DISABLE_CMD_SHIFT                        0
#define MT6336_W1_TYPE_C_SW_ENT_UNATCH_SRC_CMD_ADDR                      MT6336_PMIC_TYPE_C_CC_SW_CTRL_0
#define MT6336_W1_TYPE_C_SW_ENT_UNATCH_SRC_CMD_MASK                      0x1
#define MT6336_W1_TYPE_C_SW_ENT_UNATCH_SRC_CMD_SHIFT                     1
#define MT6336_W1_TYPE_C_SW_ENT_UNATCH_SNK_CMD_ADDR                      MT6336_PMIC_TYPE_C_CC_SW_CTRL_0
#define MT6336_W1_TYPE_C_SW_ENT_UNATCH_SNK_CMD_MASK                      0x1
#define MT6336_W1_TYPE_C_SW_ENT_UNATCH_SNK_CMD_SHIFT                     2
#define MT6336_W1_TYPE_C_SW_PR_SWAP_INDICATE_CMD_ADDR                    MT6336_PMIC_TYPE_C_CC_SW_CTRL_0
#define MT6336_W1_TYPE_C_SW_PR_SWAP_INDICATE_CMD_MASK                    0x1
#define MT6336_W1_TYPE_C_SW_PR_SWAP_INDICATE_CMD_SHIFT                   3
#define MT6336_W1_TYPE_C_SW_VCONN_SWAP_INDICATE_CMD_ADDR                 MT6336_PMIC_TYPE_C_CC_SW_CTRL_0
#define MT6336_W1_TYPE_C_SW_VCONN_SWAP_INDICATE_CMD_MASK                 0x1
#define MT6336_W1_TYPE_C_SW_VCONN_SWAP_INDICATE_CMD_SHIFT                4
#define MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_DEFAULT_CMD_ADDR          MT6336_PMIC_TYPE_C_CC_SW_CTRL_0
#define MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_DEFAULT_CMD_MASK          0x1
#define MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_DEFAULT_CMD_SHIFT         5
#define MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_15_CMD_ADDR               MT6336_PMIC_TYPE_C_CC_SW_CTRL_0
#define MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_15_CMD_MASK               0x1
#define MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_15_CMD_SHIFT              6
#define MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_30_CMD_ADDR               MT6336_PMIC_TYPE_C_CC_SW_CTRL_0
#define MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_30_CMD_MASK               0x1
#define MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_30_CMD_SHIFT              7
#define MT6336_TYPE_C_SW_VBUS_PRESENT_ADDR                               MT6336_PMIC_TYPE_C_CC_SW_CTRL_1
#define MT6336_TYPE_C_SW_VBUS_PRESENT_MASK                               0x1
#define MT6336_TYPE_C_SW_VBUS_PRESENT_SHIFT                              0
#define MT6336_TYPE_C_SW_DA_DRIVE_VCONN_EN_ADDR                          MT6336_PMIC_TYPE_C_CC_SW_CTRL_1
#define MT6336_TYPE_C_SW_DA_DRIVE_VCONN_EN_MASK                          0x1
#define MT6336_TYPE_C_SW_DA_DRIVE_VCONN_EN_SHIFT                         1
#define MT6336_TYPE_C_SW_VBUS_DET_DIS_ADDR                               MT6336_PMIC_TYPE_C_CC_SW_CTRL_1
#define MT6336_TYPE_C_SW_VBUS_DET_DIS_MASK                               0x1
#define MT6336_TYPE_C_SW_VBUS_DET_DIS_SHIFT                              2
#define MT6336_TYPE_C_SW_CC_DET_DIS_ADDR                                 MT6336_PMIC_TYPE_C_CC_SW_CTRL_1
#define MT6336_TYPE_C_SW_CC_DET_DIS_MASK                                 0x1
#define MT6336_TYPE_C_SW_CC_DET_DIS_SHIFT                                3
#define MT6336_TYPE_C_SW_PD_EN_ADDR                                      MT6336_PMIC_TYPE_C_CC_SW_CTRL_1
#define MT6336_TYPE_C_SW_PD_EN_MASK                                      0x1
#define MT6336_TYPE_C_SW_PD_EN_SHIFT                                     4
#define MT6336_W1_TYPE_C_SW_ENT_SNK_PWR_REDETECT_CMD_ADDR                MT6336_PMIC_TYPE_C_CC_SW_CTRL_1
#define MT6336_W1_TYPE_C_SW_ENT_SNK_PWR_REDETECT_CMD_MASK                0x1
#define MT6336_W1_TYPE_C_SW_ENT_SNK_PWR_REDETECT_CMD_SHIFT               7
#define MT6336_REG_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_0_ADDR                MT6336_PMIC_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_0
#define MT6336_REG_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_0_MASK                0xFF
#define MT6336_REG_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_0_SHIFT               0
#define MT6336_REG_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_1_ADDR                MT6336_PMIC_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_1
#define MT6336_REG_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_1_MASK                0x1F
#define MT6336_REG_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_1_SHIFT               0
#define MT6336_REG_TYPE_C_CC_VOL_CC_DEBOUNCE_CNT_VAL_ADDR                MT6336_PMIC_TYPE_C_CC_VOL_CC_DEBOUCE_CNT_VAL
#define MT6336_REG_TYPE_C_CC_VOL_CC_DEBOUNCE_CNT_VAL_MASK                0xFF
#define MT6336_REG_TYPE_C_CC_VOL_CC_DEBOUNCE_CNT_VAL_SHIFT               0
#define MT6336_REG_TYPE_C_CC_VOL_PD_DEBOUNCE_CNT_VAL_ADDR                MT6336_PMIC_TYPE_C_CC_VOL_PD_DEBOUCE_CNT_VAL
#define MT6336_REG_TYPE_C_CC_VOL_PD_DEBOUNCE_CNT_VAL_MASK                0x1F
#define MT6336_REG_TYPE_C_CC_VOL_PD_DEBOUNCE_CNT_VAL_SHIFT               0
#define MT6336_REG_TYPE_C_DRP_SRC_CNT_VAL_0_0_ADDR                       MT6336_PMIC_TYPE_C_DRP_SRC_CNT_VAL_0_0
#define MT6336_REG_TYPE_C_DRP_SRC_CNT_VAL_0_0_MASK                       0xFF
#define MT6336_REG_TYPE_C_DRP_SRC_CNT_VAL_0_0_SHIFT                      0
#define MT6336_REG_TYPE_C_DRP_SRC_CNT_VAL_0_1_ADDR                       MT6336_PMIC_TYPE_C_DRP_SRC_CNT_VAL_0_1
#define MT6336_REG_TYPE_C_DRP_SRC_CNT_VAL_0_1_MASK                       0xFF
#define MT6336_REG_TYPE_C_DRP_SRC_CNT_VAL_0_1_SHIFT                      0
#define MT6336_REG_TYPE_C_DRP_SNK_CNT_VAL_0_0_ADDR                       MT6336_PMIC_TYPE_C_DRP_SNK_CNT_VAL_0_0
#define MT6336_REG_TYPE_C_DRP_SNK_CNT_VAL_0_0_MASK                       0xFF
#define MT6336_REG_TYPE_C_DRP_SNK_CNT_VAL_0_0_SHIFT                      0
#define MT6336_REG_TYPE_C_DRP_SNK_CNT_VAL_0_1_ADDR                       MT6336_PMIC_TYPE_C_DRP_SNK_CNT_VAL_0_1
#define MT6336_REG_TYPE_C_DRP_SNK_CNT_VAL_0_1_MASK                       0xFF
#define MT6336_REG_TYPE_C_DRP_SNK_CNT_VAL_0_1_SHIFT                      0
#define MT6336_REG_TYPE_C_DRP_TRY_CNT_VAL_0_0_ADDR                       MT6336_PMIC_TYPE_C_DRP_TRY_CNT_VAL_0_0
#define MT6336_REG_TYPE_C_DRP_TRY_CNT_VAL_0_0_MASK                       0xFF
#define MT6336_REG_TYPE_C_DRP_TRY_CNT_VAL_0_0_SHIFT                      0
#define MT6336_REG_TYPE_C_DRP_TRY_CNT_VAL_0_1_ADDR                       MT6336_PMIC_TYPE_C_DRP_TRY_CNT_VAL_0_1
#define MT6336_REG_TYPE_C_DRP_TRY_CNT_VAL_0_1_MASK                       0xFF
#define MT6336_REG_TYPE_C_DRP_TRY_CNT_VAL_0_1_SHIFT                      0
#define MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_0_ADDR                  MT6336_PMIC_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_0
#define MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_0_MASK                  0xFF
#define MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_0_SHIFT                 0
#define MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_1_ADDR                  MT6336_PMIC_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_1
#define MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_1_MASK                  0xFF
#define MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_1_SHIFT                 0
#define MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_1_ADDR                    MT6336_PMIC_TYPE_C_DRP_TRY_WAIT_CNT_VAL_1
#define MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_1_MASK                    0xF
#define MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_1_SHIFT                   0
#define MT6336_REG_TYPE_C_CC_SRC_VOPEN_DEFAULT_DAC_VAL_ADDR              MT6336_PMIC_TYPE_C_CC_SRC_VOPEN_DEFAULT_DAC_VAL
#define MT6336_REG_TYPE_C_CC_SRC_VOPEN_DEFAULT_DAC_VAL_MASK              0x3F
#define MT6336_REG_TYPE_C_CC_SRC_VOPEN_DEFAULT_DAC_VAL_SHIFT             0
#define MT6336_REG_TYPE_C_CC_SRC_VRD_DEFAULT_DAC_VAL_ADDR                MT6336_PMIC_TYPE_C_CC_SRC_VRD_DEFAULT_DAC_VAL
#define MT6336_REG_TYPE_C_CC_SRC_VRD_DEFAULT_DAC_VAL_MASK                0x3F
#define MT6336_REG_TYPE_C_CC_SRC_VRD_DEFAULT_DAC_VAL_SHIFT               0
#define MT6336_REG_TYPE_C_CC_SRC_VOPEN_15_DAC_VAL_ADDR                   MT6336_PMIC_TYPE_C_CC_SRC_VOPEN_15_DAC_VAL
#define MT6336_REG_TYPE_C_CC_SRC_VOPEN_15_DAC_VAL_MASK                   0x3F
#define MT6336_REG_TYPE_C_CC_SRC_VOPEN_15_DAC_VAL_SHIFT                  0
#define MT6336_REG_TYPE_C_CC_SRC_VRD_15_DAC_VAL_ADDR                     MT6336_PMIC_TYPE_C_CC_SRC_VRD_15_DAC_VAL
#define MT6336_REG_TYPE_C_CC_SRC_VRD_15_DAC_VAL_MASK                     0x3F
#define MT6336_REG_TYPE_C_CC_SRC_VRD_15_DAC_VAL_SHIFT                    0
#define MT6336_REG_TYPE_C_CC_SRC_VOPEN_30_DAC_VAL_ADDR                   MT6336_PMIC_TYPE_C_CC_SRC_VOPEN_30_DAC_VAL
#define MT6336_REG_TYPE_C_CC_SRC_VOPEN_30_DAC_VAL_MASK                   0x3F
#define MT6336_REG_TYPE_C_CC_SRC_VOPEN_30_DAC_VAL_SHIFT                  0
#define MT6336_REG_TYPE_C_CC_SRC_VRD_30_DAC_VAL_ADDR                     MT6336_PMIC_TYPE_C_CC_SRC_VRD_30_DAC_VAL
#define MT6336_REG_TYPE_C_CC_SRC_VRD_30_DAC_VAL_MASK                     0x3F
#define MT6336_REG_TYPE_C_CC_SRC_VRD_30_DAC_VAL_SHIFT                    0
#define MT6336_REG_TYPE_C_CC_SNK_VRP30_DAC_VAL_ADDR                      MT6336_PMIC_TYPE_C_CC_SNK_VRP30_DAC_VAL
#define MT6336_REG_TYPE_C_CC_SNK_VRP30_DAC_VAL_MASK                      0x3F
#define MT6336_REG_TYPE_C_CC_SNK_VRP30_DAC_VAL_SHIFT                     0
#define MT6336_REG_TYPE_C_CC_SNK_VRP15_DAC_VAL_ADDR                      MT6336_PMIC_TYPE_C_CC_SNK_VRP15_DAC_VAL
#define MT6336_REG_TYPE_C_CC_SNK_VRP15_DAC_VAL_MASK                      0x3F
#define MT6336_REG_TYPE_C_CC_SNK_VRP15_DAC_VAL_SHIFT                     0
#define MT6336_REG_TYPE_C_CC_SNK_VRPUSB_DAC_VAL_ADDR                     MT6336_PMIC_TYPE_C_CC_SNK_VRPUSB_DAC_VAL
#define MT6336_REG_TYPE_C_CC_SNK_VRPUSB_DAC_VAL_MASK                     0x3F
#define MT6336_REG_TYPE_C_CC_SNK_VRPUSB_DAC_VAL_SHIFT                    0
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_SRC_INTR_EN_ADDR                 MT6336_PMIC_TYPE_C_INTR_EN_0
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_SRC_INTR_EN_MASK                 0x1
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_SRC_INTR_EN_SHIFT                0
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_SNK_INTR_EN_ADDR                 MT6336_PMIC_TYPE_C_INTR_EN_0
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_SNK_INTR_EN_MASK                 0x1
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_SNK_INTR_EN_SHIFT                1
#define MT6336_REG_TYPE_C_CC_ENT_AUDIO_ACC_INTR_EN_ADDR                  MT6336_PMIC_TYPE_C_INTR_EN_0
#define MT6336_REG_TYPE_C_CC_ENT_AUDIO_ACC_INTR_EN_MASK                  0x1
#define MT6336_REG_TYPE_C_CC_ENT_AUDIO_ACC_INTR_EN_SHIFT                 2
#define MT6336_REG_TYPE_C_CC_ENT_DBG_ACC_INTR_EN_ADDR                    MT6336_PMIC_TYPE_C_INTR_EN_0
#define MT6336_REG_TYPE_C_CC_ENT_DBG_ACC_INTR_EN_MASK                    0x1
#define MT6336_REG_TYPE_C_CC_ENT_DBG_ACC_INTR_EN_SHIFT                   3
#define MT6336_REG_TYPE_C_CC_ENT_DISABLE_INTR_EN_ADDR                    MT6336_PMIC_TYPE_C_INTR_EN_0
#define MT6336_REG_TYPE_C_CC_ENT_DISABLE_INTR_EN_MASK                    0x1
#define MT6336_REG_TYPE_C_CC_ENT_DISABLE_INTR_EN_SHIFT                   4
#define MT6336_REG_TYPE_C_CC_ENT_UNATTACH_SRC_INTR_EN_ADDR               MT6336_PMIC_TYPE_C_INTR_EN_0
#define MT6336_REG_TYPE_C_CC_ENT_UNATTACH_SRC_INTR_EN_MASK               0x1
#define MT6336_REG_TYPE_C_CC_ENT_UNATTACH_SRC_INTR_EN_SHIFT              5
#define MT6336_REG_TYPE_C_CC_ENT_UNATTACH_SNK_INTR_EN_ADDR               MT6336_PMIC_TYPE_C_INTR_EN_0
#define MT6336_REG_TYPE_C_CC_ENT_UNATTACH_SNK_INTR_EN_MASK               0x1
#define MT6336_REG_TYPE_C_CC_ENT_UNATTACH_SNK_INTR_EN_SHIFT              6
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR_EN_ADDR            MT6336_PMIC_TYPE_C_INTR_EN_0
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR_EN_MASK            0x1
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR_EN_SHIFT           7
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR_EN_ADDR            MT6336_PMIC_TYPE_C_INTR_EN_1
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR_EN_MASK            0x1
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR_EN_SHIFT           0
#define MT6336_REG_TYPE_C_CC_ENT_TRY_SRC_INTR_EN_ADDR                    MT6336_PMIC_TYPE_C_INTR_EN_1
#define MT6336_REG_TYPE_C_CC_ENT_TRY_SRC_INTR_EN_MASK                    0x1
#define MT6336_REG_TYPE_C_CC_ENT_TRY_SRC_INTR_EN_SHIFT                   1
#define MT6336_REG_TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR_EN_ADDR               MT6336_PMIC_TYPE_C_INTR_EN_1
#define MT6336_REG_TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR_EN_MASK               0x1
#define MT6336_REG_TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR_EN_SHIFT              2
#define MT6336_REG_TYPE_C_CC_ENT_UNATTACH_ACC_INTR_EN_ADDR               MT6336_PMIC_TYPE_C_INTR_EN_1
#define MT6336_REG_TYPE_C_CC_ENT_UNATTACH_ACC_INTR_EN_MASK               0x1
#define MT6336_REG_TYPE_C_CC_ENT_UNATTACH_ACC_INTR_EN_SHIFT              3
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR_EN_ADDR            MT6336_PMIC_TYPE_C_INTR_EN_1
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR_EN_MASK            0x1
#define MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR_EN_SHIFT           4
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR_EN_ADDR               MT6336_PMIC_TYPE_C_INTR_EN_2
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR_EN_MASK               0x1
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR_EN_SHIFT              0
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR_EN_ADDR            MT6336_PMIC_TYPE_C_INTR_EN_2
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR_EN_MASK            0x1
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR_EN_SHIFT           1
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_15_INTR_EN_ADDR                 MT6336_PMIC_TYPE_C_INTR_EN_2
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_15_INTR_EN_MASK                 0x1
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_15_INTR_EN_SHIFT                2
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_30_INTR_EN_ADDR                 MT6336_PMIC_TYPE_C_INTR_EN_2
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_30_INTR_EN_MASK                 0x1
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_30_INTR_EN_SHIFT                3
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR_EN_ADDR           MT6336_PMIC_TYPE_C_INTR_EN_2
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR_EN_MASK           0x1
#define MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR_EN_SHIFT          4
#define MT6336_TYPE_C_CC_ENT_ATTACH_SRC_INTR_ADDR                        MT6336_PMIC_TYPE_C_INTR_0
#define MT6336_TYPE_C_CC_ENT_ATTACH_SRC_INTR_MASK                        0x1
#define MT6336_TYPE_C_CC_ENT_ATTACH_SRC_INTR_SHIFT                       0
#define MT6336_TYPE_C_CC_ENT_ATTACH_SNK_INTR_ADDR                        MT6336_PMIC_TYPE_C_INTR_0
#define MT6336_TYPE_C_CC_ENT_ATTACH_SNK_INTR_MASK                        0x1
#define MT6336_TYPE_C_CC_ENT_ATTACH_SNK_INTR_SHIFT                       1
#define MT6336_TYPE_C_CC_ENT_AUDIO_ACC_INTR_ADDR                         MT6336_PMIC_TYPE_C_INTR_0
#define MT6336_TYPE_C_CC_ENT_AUDIO_ACC_INTR_MASK                         0x1
#define MT6336_TYPE_C_CC_ENT_AUDIO_ACC_INTR_SHIFT                        2
#define MT6336_TYPE_C_CC_ENT_DBG_ACC_INTR_ADDR                           MT6336_PMIC_TYPE_C_INTR_0
#define MT6336_TYPE_C_CC_ENT_DBG_ACC_INTR_MASK                           0x1
#define MT6336_TYPE_C_CC_ENT_DBG_ACC_INTR_SHIFT                          3
#define MT6336_TYPE_C_CC_ENT_DISABLE_INTR_ADDR                           MT6336_PMIC_TYPE_C_INTR_0
#define MT6336_TYPE_C_CC_ENT_DISABLE_INTR_MASK                           0x1
#define MT6336_TYPE_C_CC_ENT_DISABLE_INTR_SHIFT                          4
#define MT6336_TYPE_C_CC_ENT_UNATTACH_SRC_INTR_ADDR                      MT6336_PMIC_TYPE_C_INTR_0
#define MT6336_TYPE_C_CC_ENT_UNATTACH_SRC_INTR_MASK                      0x1
#define MT6336_TYPE_C_CC_ENT_UNATTACH_SRC_INTR_SHIFT                     5
#define MT6336_TYPE_C_CC_ENT_UNATTACH_SNK_INTR_ADDR                      MT6336_PMIC_TYPE_C_INTR_0
#define MT6336_TYPE_C_CC_ENT_UNATTACH_SNK_INTR_MASK                      0x1
#define MT6336_TYPE_C_CC_ENT_UNATTACH_SNK_INTR_SHIFT                     6
#define MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR_ADDR                   MT6336_PMIC_TYPE_C_INTR_0
#define MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR_MASK                   0x1
#define MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR_SHIFT                  7
#define MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR_ADDR                   MT6336_PMIC_TYPE_C_INTR_1
#define MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR_MASK                   0x1
#define MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR_SHIFT                  0
#define MT6336_TYPE_C_CC_ENT_TRY_SRC_INTR_ADDR                           MT6336_PMIC_TYPE_C_INTR_1
#define MT6336_TYPE_C_CC_ENT_TRY_SRC_INTR_MASK                           0x1
#define MT6336_TYPE_C_CC_ENT_TRY_SRC_INTR_SHIFT                          1
#define MT6336_TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR_ADDR                      MT6336_PMIC_TYPE_C_INTR_1
#define MT6336_TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR_MASK                      0x1
#define MT6336_TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR_SHIFT                     2
#define MT6336_TYPE_C_CC_ENT_UNATTACH_ACC_INTR_ADDR                      MT6336_PMIC_TYPE_C_INTR_1
#define MT6336_TYPE_C_CC_ENT_UNATTACH_ACC_INTR_MASK                      0x1
#define MT6336_TYPE_C_CC_ENT_UNATTACH_ACC_INTR_SHIFT                     3
#define MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR_ADDR                   MT6336_PMIC_TYPE_C_INTR_1
#define MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR_MASK                   0x1
#define MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR_SHIFT                  4
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR_ADDR                      MT6336_PMIC_TYPE_C_INTR_2
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR_MASK                      0x1
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR_SHIFT                     0
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR_ADDR                   MT6336_PMIC_TYPE_C_INTR_2
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR_MASK                   0x1
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR_SHIFT                  1
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_15_INTR_ADDR                        MT6336_PMIC_TYPE_C_INTR_2
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_15_INTR_MASK                        0x1
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_15_INTR_SHIFT                       2
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_30_INTR_ADDR                        MT6336_PMIC_TYPE_C_INTR_2
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_30_INTR_MASK                        0x1
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_30_INTR_SHIFT                       3
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR_ADDR                  MT6336_PMIC_TYPE_C_INTR_2
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR_MASK                  0x1
#define MT6336_TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR_SHIFT                 4
#define MT6336_RO_TYPE_C_CC_ST_ADDR                                      MT6336_PMIC_TYPE_C_CC_STATUS_0
#define MT6336_RO_TYPE_C_CC_ST_MASK                                      0xF
#define MT6336_RO_TYPE_C_CC_ST_SHIFT                                     0
#define MT6336_RO_TYPE_C_ROUTED_CC_ADDR                                  MT6336_PMIC_TYPE_C_CC_STATUS_1
#define MT6336_RO_TYPE_C_ROUTED_CC_MASK                                  0x1
#define MT6336_RO_TYPE_C_ROUTED_CC_SHIFT                                 7
#define MT6336_RO_TYPE_C_CC_SNK_PWR_ST_ADDR                              MT6336_PMIC_TYPE_C_PWR_STATUS
#define MT6336_RO_TYPE_C_CC_SNK_PWR_ST_MASK                              0x7
#define MT6336_RO_TYPE_C_CC_SNK_PWR_ST_SHIFT                             0
#define MT6336_RO_TYPE_C_CC_PWR_ROLE_ADDR                                MT6336_PMIC_TYPE_C_PWR_STATUS
#define MT6336_RO_TYPE_C_CC_PWR_ROLE_MASK                                0x1
#define MT6336_RO_TYPE_C_CC_PWR_ROLE_SHIFT                               4
#define MT6336_RO_TYPE_C_DRIVE_VCONN_CAPABLE_ADDR                        MT6336_PMIC_TYPE_C_PWR_STATUS
#define MT6336_RO_TYPE_C_DRIVE_VCONN_CAPABLE_MASK                        0x1
#define MT6336_RO_TYPE_C_DRIVE_VCONN_CAPABLE_SHIFT                       5
#define MT6336_RO_TYPE_C_AD_CC_CMP_OUT_ADDR                              MT6336_PMIC_TYPE_C_PWR_STATUS
#define MT6336_RO_TYPE_C_AD_CC_CMP_OUT_MASK                              0x1
#define MT6336_RO_TYPE_C_AD_CC_CMP_OUT_SHIFT                             6
#define MT6336_RO_AD_CC_VUSB33_RDY_ADDR                                  MT6336_PMIC_TYPE_C_PWR_STATUS
#define MT6336_RO_AD_CC_VUSB33_RDY_MASK                                  0x1
#define MT6336_RO_AD_CC_VUSB33_RDY_SHIFT                                 7
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RPDE_ADDR                           MT6336_PMIC_TYPE_C_PHY_RG_CC1_RESISTENCE_RPDE
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RPDE_MASK                           0x7
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RPDE_SHIFT                          0
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RP15_ADDR                           MT6336_PMIC_TYPE_C_PHY_RG_CC1_RESISTENCE_RP15
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RP15_MASK                           0x1F
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RP15_SHIFT                          0
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RP3_ADDR                            MT6336_PMIC_TYPE_C_PHY_RG_CC1_RESISTENCE_RP3
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RP3_MASK                            0x1F
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RP3_SHIFT                           0
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RD_ADDR                             MT6336_PMIC_TYPE_C_PHY_RG_CC1_RESISTENCE_RD
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RD_MASK                             0x1F
#define MT6336_REG_TYPE_C_PHY_RG_CC1_RD_SHIFT                            0
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RPDE_ADDR                           MT6336_PMIC_TYPE_C_PHY_RG_CC2_RESISTENCE_RPDE
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RPDE_MASK                           0x7
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RPDE_SHIFT                          0
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RP15_ADDR                           MT6336_PMIC_TYPE_C_PHY_RG_CC2_RESISTENCE_RP15
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RP15_MASK                           0x1F
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RP15_SHIFT                          0
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RP3_ADDR                            MT6336_PMIC_TYPE_C_PHY_RG_CC2_RESISTENCE_RP3
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RP3_MASK                            0x1F
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RP3_SHIFT                           0
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RD_ADDR                             MT6336_PMIC_TYPE_C_PHY_RG_CC2_RESISTENCE_RD
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RD_MASK                             0x1F
#define MT6336_REG_TYPE_C_PHY_RG_CC2_RD_SHIFT                            0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC1_ADDR               MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC1_MASK               0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC1_SHIFT              0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC2_ADDR               MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC2_MASK               0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC2_SHIFT              1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LEV_EN_ADDR             MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LEV_EN_MASK             0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LEV_EN_SHIFT            2
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SW_SEL_ADDR             MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SW_SEL_MASK             0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SW_SEL_SHIFT            3
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_BIAS_EN_ADDR            MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_BIAS_EN_MASK            0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_BIAS_EN_SHIFT           4
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LPF_EN_ADDR             MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LPF_EN_MASK             0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LPF_EN_SHIFT            5
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_ADCSW_EN_ADDR           MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_ADCSW_EN_MASK           0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_ADCSW_EN_SHIFT          6
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SASW_EN_ADDR            MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SASW_EN_MASK            0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SASW_EN_SHIFT           7
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_EN_ADDR             MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_EN_MASK             0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_EN_SHIFT            0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SACLK_ADDR              MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SACLK_MASK              0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SACLK_SHIFT             1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_IN_ADDR             MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_IN_MASK             0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_IN_SHIFT            2
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_CAL_ADDR            MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_CAL_MASK            0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_CAL_SHIFT           3
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_GAIN_CAL_ADDR       MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_ENABLE_1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_GAIN_CAL_MASK       0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_GAIN_CAL_SHIFT      4
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC1_EN_ADDR              MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC1_EN_MASK              0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC1_EN_SHIFT             0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC1_EN_ADDR              MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC1_EN_MASK              0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC1_EN_SHIFT             1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC1_EN_ADDR              MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC1_EN_MASK              0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC1_EN_SHIFT             2
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC2_EN_ADDR              MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC2_EN_MASK              0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC2_EN_SHIFT             3
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC2_EN_ADDR              MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC2_EN_MASK              0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC2_EN_SHIFT             4
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC2_EN_ADDR              MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC2_EN_MASK              0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC2_EN_SHIFT             5
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_LEV_EN_ADDR                MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_LEV_EN_MASK                0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_LEV_EN_SHIFT               6
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SW_SEL_ADDR                MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SW_SEL_MASK                0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SW_SEL_SHIFT               7
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_BIAS_EN_ADDR               MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_BIAS_EN_MASK               0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_BIAS_EN_SHIFT              0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_LPF_EN_ADDR                MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_LPF_EN_MASK                0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_LPF_EN_SHIFT               1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_ADCSW_EN_ADDR              MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_ADCSW_EN_MASK              0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_ADCSW_EN_SHIFT             2
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SASW_EN_ADDR               MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SASW_EN_MASK               0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SASW_EN_SHIFT              3
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_EN_ADDR                MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_EN_MASK                0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_EN_SHIFT               4
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SACLK_ADDR                 MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SACLK_MASK                 0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SACLK_SHIFT                5
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_IN_ADDR                MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_2
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_IN_MASK                0x3F
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_IN_SHIFT               0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_CAL_ADDR               MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_3
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_CAL_MASK               0xF
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_CAL_SHIFT              0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_GAIN_CAL_ADDR          MT6336_PMIC_TYPE_C_CC_SW_FORCE_MODE_VAL_3
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_GAIN_CAL_MASK          0xF
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_GAIN_CAL_SHIFT         4
#define MT6336_TYPE_C_DAC_CAL_START_ADDR                                 MT6336_PMIC_TYPE_C_CC_DAC_CALI_CTRL
#define MT6336_TYPE_C_DAC_CAL_START_MASK                                 0x1
#define MT6336_TYPE_C_DAC_CAL_START_SHIFT                                0
#define MT6336_REG_TYPE_C_DAC_CAL_STAGE_ADDR                             MT6336_PMIC_TYPE_C_CC_DAC_CALI_CTRL
#define MT6336_REG_TYPE_C_DAC_CAL_STAGE_MASK                             0x1
#define MT6336_REG_TYPE_C_DAC_CAL_STAGE_SHIFT                            1
#define MT6336_RO_TYPE_C_DAC_CAL_OK_ADDR                                 MT6336_PMIC_TYPE_C_CC_DAC_CALI_CTRL
#define MT6336_RO_TYPE_C_DAC_CAL_OK_MASK                                 0x1
#define MT6336_RO_TYPE_C_DAC_CAL_OK_SHIFT                                4
#define MT6336_RO_TYPE_C_DAC_CAL_FAIL_ADDR                               MT6336_PMIC_TYPE_C_CC_DAC_CALI_CTRL
#define MT6336_RO_TYPE_C_DAC_CAL_FAIL_MASK                               0x1
#define MT6336_RO_TYPE_C_DAC_CAL_FAIL_SHIFT                              5
#define MT6336_RO_DA_CC_DAC_CAL_ADDR                                     MT6336_PMIC_TYPE_C_CC_DAC_CALI_RESULT
#define MT6336_RO_DA_CC_DAC_CAL_MASK                                     0xF
#define MT6336_RO_DA_CC_DAC_CAL_SHIFT                                    0
#define MT6336_RO_DA_CC_DAC_GAIN_CAL_ADDR                                MT6336_PMIC_TYPE_C_CC_DAC_CALI_RESULT
#define MT6336_RO_DA_CC_DAC_GAIN_CAL_MASK                                0xF
#define MT6336_RO_DA_CC_DAC_GAIN_CAL_SHIFT                               4
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_0_0_ADDR                          MT6336_PMIC_TYPE_C_DEBUG_PORT_SELECT_0_0
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_0_0_MASK                          0xFF
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_0_0_SHIFT                         0
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_0_1_ADDR                          MT6336_PMIC_TYPE_C_DEBUG_PORT_SELECT_0_1
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_0_1_MASK                          0xFF
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_0_1_SHIFT                         0
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_1_0_ADDR                          MT6336_PMIC_TYPE_C_DEBUG_PORT_SELECT_1_0
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_1_0_MASK                          0xFF
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_1_0_SHIFT                         0
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_1_1_ADDR                          MT6336_PMIC_TYPE_C_DEBUG_PORT_SELECT_1_1
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_1_1_MASK                          0xFF
#define MT6336_REG_TYPE_C_DBG_PORT_SEL_1_1_SHIFT                         0
#define MT6336_REG_TYPE_C_DBG_MOD_SEL_0_ADDR                             MT6336_PMIC_TYPE_C_DEBUG_MODE_SELECT_0
#define MT6336_REG_TYPE_C_DBG_MOD_SEL_0_MASK                             0xFF
#define MT6336_REG_TYPE_C_DBG_MOD_SEL_0_SHIFT                            0
#define MT6336_REG_TYPE_C_DBG_MOD_SEL_1_ADDR                             MT6336_PMIC_TYPE_C_DEBUG_MODE_SELECT_1
#define MT6336_REG_TYPE_C_DBG_MOD_SEL_1_MASK                             0xFF
#define MT6336_REG_TYPE_C_DBG_MOD_SEL_1_SHIFT                            0
#define MT6336_RO_TYPE_C_DBG_OUT_READ_0_0_ADDR                           MT6336_PMIC_TYPE_C_DEBUG_OUT_READ_0_0
#define MT6336_RO_TYPE_C_DBG_OUT_READ_0_0_MASK                           0xFF
#define MT6336_RO_TYPE_C_DBG_OUT_READ_0_0_SHIFT                          0
#define MT6336_RO_TYPE_C_DBG_OUT_READ_0_1_ADDR                           MT6336_PMIC_TYPE_C_DEBUG_OUT_READ_0_1
#define MT6336_RO_TYPE_C_DBG_OUT_READ_0_1_MASK                           0xFF
#define MT6336_RO_TYPE_C_DBG_OUT_READ_0_1_SHIFT                          0
#define MT6336_RO_TYPE_C_DBG_OUT_READ_1_0_ADDR                           MT6336_PMIC_TYPE_C_DEBUG_OUT_READ_1_0
#define MT6336_RO_TYPE_C_DBG_OUT_READ_1_0_MASK                           0xFF
#define MT6336_RO_TYPE_C_DBG_OUT_READ_1_0_SHIFT                          0
#define MT6336_RO_TYPE_C_DBG_OUT_READ_1_1_ADDR                           MT6336_PMIC_TYPE_C_DEBUG_OUT_READ_1_1
#define MT6336_RO_TYPE_C_DBG_OUT_READ_1_1_MASK                           0xFF
#define MT6336_RO_TYPE_C_DBG_OUT_READ_1_1_SHIFT                          0
#define MT6336_REG_TYPE_C_SW_DBG_PORT_0_0_ADDR                           MT6336_PMIC_TYPE_C_SW_DEBUG_PORT_0_0
#define MT6336_REG_TYPE_C_SW_DBG_PORT_0_0_MASK                           0xFF
#define MT6336_REG_TYPE_C_SW_DBG_PORT_0_0_SHIFT                          0
#define MT6336_REG_TYPE_C_SW_DBG_PORT_0_1_ADDR                           MT6336_PMIC_TYPE_C_SW_DEBUG_PORT_0_1
#define MT6336_REG_TYPE_C_SW_DBG_PORT_0_1_MASK                           0xFF
#define MT6336_REG_TYPE_C_SW_DBG_PORT_0_1_SHIFT                          0
#define MT6336_REG_TYPE_C_SW_DBG_PORT_1_0_ADDR                           MT6336_PMIC_TYPE_C_SW_DEBUG_PORT_1_0
#define MT6336_REG_TYPE_C_SW_DBG_PORT_1_0_MASK                           0xFF
#define MT6336_REG_TYPE_C_SW_DBG_PORT_1_0_SHIFT                          0
#define MT6336_REG_TYPE_C_SW_DBG_PORT_1_1_ADDR                           MT6336_PMIC_TYPE_C_SW_DEBUG_PORT_1_1
#define MT6336_REG_TYPE_C_SW_DBG_PORT_1_1_MASK                           0xFF
#define MT6336_REG_TYPE_C_SW_DBG_PORT_1_1_SHIFT                          0
#define MT6336_REG_PD_TX_HALF_UI_CYCLE_CNT_ADDR                          MT6336_PMIC_PD_TX_PARAMETER_0
#define MT6336_REG_PD_TX_HALF_UI_CYCLE_CNT_MASK                          0xFF
#define MT6336_REG_PD_TX_HALF_UI_CYCLE_CNT_SHIFT                         0
#define MT6336_REG_PD_TX_RETRY_CNT_ADDR                                  MT6336_PMIC_PD_TX_PARAMETER_1
#define MT6336_REG_PD_TX_RETRY_CNT_MASK                                  0xF
#define MT6336_REG_PD_TX_RETRY_CNT_SHIFT                                 0
#define MT6336_REG_PD_TX_AUTO_SEND_SR_EN_ADDR                            MT6336_PMIC_PD_TX_PARAMETER_1
#define MT6336_REG_PD_TX_AUTO_SEND_SR_EN_MASK                            0x1
#define MT6336_REG_PD_TX_AUTO_SEND_SR_EN_SHIFT                           4
#define MT6336_REG_PD_TX_AUTO_SEND_HR_EN_ADDR                            MT6336_PMIC_PD_TX_PARAMETER_1
#define MT6336_REG_PD_TX_AUTO_SEND_HR_EN_MASK                            0x1
#define MT6336_REG_PD_TX_AUTO_SEND_HR_EN_SHIFT                           5
#define MT6336_REG_PD_TX_AUTO_SEND_CR_EN_ADDR                            MT6336_PMIC_PD_TX_PARAMETER_1
#define MT6336_REG_PD_TX_AUTO_SEND_CR_EN_MASK                            0x1
#define MT6336_REG_PD_TX_AUTO_SEND_CR_EN_SHIFT                           6
#define MT6336_REG_PD_TX_DATA_OBJ0_0_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT0_0_0
#define MT6336_REG_PD_TX_DATA_OBJ0_0_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ0_0_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ0_0_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT0_0_1
#define MT6336_REG_PD_TX_DATA_OBJ0_0_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ0_0_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ0_1_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT0_1_0
#define MT6336_REG_PD_TX_DATA_OBJ0_1_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ0_1_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ0_1_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT0_1_1
#define MT6336_REG_PD_TX_DATA_OBJ0_1_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ0_1_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ1_0_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT1_0_0
#define MT6336_REG_PD_TX_DATA_OBJ1_0_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ1_0_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ1_0_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT1_0_1
#define MT6336_REG_PD_TX_DATA_OBJ1_0_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ1_0_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ1_1_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT1_1_0
#define MT6336_REG_PD_TX_DATA_OBJ1_1_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ1_1_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ1_1_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT1_1_1
#define MT6336_REG_PD_TX_DATA_OBJ1_1_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ1_1_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ2_0_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT2_0_0
#define MT6336_REG_PD_TX_DATA_OBJ2_0_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ2_0_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ2_0_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT2_0_1
#define MT6336_REG_PD_TX_DATA_OBJ2_0_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ2_0_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ2_1_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT2_1_0
#define MT6336_REG_PD_TX_DATA_OBJ2_1_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ2_1_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ2_1_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT2_1_1
#define MT6336_REG_PD_TX_DATA_OBJ2_1_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ2_1_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ3_0_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT3_0_0
#define MT6336_REG_PD_TX_DATA_OBJ3_0_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ3_0_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ3_0_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT3_0_1
#define MT6336_REG_PD_TX_DATA_OBJ3_0_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ3_0_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ3_1_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT3_1_0
#define MT6336_REG_PD_TX_DATA_OBJ3_1_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ3_1_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ3_1_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT3_1_1
#define MT6336_REG_PD_TX_DATA_OBJ3_1_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ3_1_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ4_0_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT4_0_0
#define MT6336_REG_PD_TX_DATA_OBJ4_0_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ4_0_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ4_0_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT4_0_1
#define MT6336_REG_PD_TX_DATA_OBJ4_0_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ4_0_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ4_1_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT4_1_0
#define MT6336_REG_PD_TX_DATA_OBJ4_1_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ4_1_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ4_1_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT4_1_1
#define MT6336_REG_PD_TX_DATA_OBJ4_1_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ4_1_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ5_0_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT5_0_0
#define MT6336_REG_PD_TX_DATA_OBJ5_0_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ5_0_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ5_0_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT5_0_1
#define MT6336_REG_PD_TX_DATA_OBJ5_0_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ5_0_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ5_1_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT5_1_0
#define MT6336_REG_PD_TX_DATA_OBJ5_1_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ5_1_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ5_1_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT5_1_1
#define MT6336_REG_PD_TX_DATA_OBJ5_1_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ5_1_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ6_0_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT6_0_0
#define MT6336_REG_PD_TX_DATA_OBJ6_0_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ6_0_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ6_0_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT6_0_1
#define MT6336_REG_PD_TX_DATA_OBJ6_0_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ6_0_1_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ6_1_0_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT6_1_0
#define MT6336_REG_PD_TX_DATA_OBJ6_1_0_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ6_1_0_SHIFT                             0
#define MT6336_REG_PD_TX_DATA_OBJ6_1_1_ADDR                              MT6336_PMIC_PD_TX_DATA_OBJECT6_1_1
#define MT6336_REG_PD_TX_DATA_OBJ6_1_1_MASK                              0xFF
#define MT6336_REG_PD_TX_DATA_OBJ6_1_1_SHIFT                             0
#define MT6336_REG_PD_TX_HDR_MSG_TYPE_ADDR                               MT6336_PMIC_PD_TX_HEADER_0
#define MT6336_REG_PD_TX_HDR_MSG_TYPE_MASK                               0xF
#define MT6336_REG_PD_TX_HDR_MSG_TYPE_SHIFT                              0
#define MT6336_REG_PD_TX_HDR_PORT_DATA_ROLE_ADDR                         MT6336_PMIC_PD_TX_HEADER_0
#define MT6336_REG_PD_TX_HDR_PORT_DATA_ROLE_MASK                         0x1
#define MT6336_REG_PD_TX_HDR_PORT_DATA_ROLE_SHIFT                        5
#define MT6336_REG_PD_TX_HDR_SPEC_VER_ADDR                               MT6336_PMIC_PD_TX_HEADER_0
#define MT6336_REG_PD_TX_HDR_SPEC_VER_MASK                               0x3
#define MT6336_REG_PD_TX_HDR_SPEC_VER_SHIFT                              6
#define MT6336_REG_PD_TX_HDR_PORT_POWER_ROLE_ADDR                        MT6336_PMIC_PD_TX_HEADER_1
#define MT6336_REG_PD_TX_HDR_PORT_POWER_ROLE_MASK                        0x1
#define MT6336_REG_PD_TX_HDR_PORT_POWER_ROLE_SHIFT                       0
#define MT6336_REG_PD_TX_HDR_NUM_DATA_OBJ_ADDR                           MT6336_PMIC_PD_TX_HEADER_1
#define MT6336_REG_PD_TX_HDR_NUM_DATA_OBJ_MASK                           0x7
#define MT6336_REG_PD_TX_HDR_NUM_DATA_OBJ_SHIFT                          4
#define MT6336_REG_PD_TX_HDR_CABLE_PLUG_ADDR                             MT6336_PMIC_PD_TX_HEADER_1
#define MT6336_REG_PD_TX_HDR_CABLE_PLUG_MASK                             0x1
#define MT6336_REG_PD_TX_HDR_CABLE_PLUG_SHIFT                            7
#define MT6336_PD_TX_START_ADDR                                          MT6336_PMIC_PD_TX_CTRL
#define MT6336_PD_TX_START_MASK                                          0x1
#define MT6336_PD_TX_START_SHIFT                                         0
#define MT6336_PD_TX_BIST_CARRIER_MODE2_START_ADDR                       MT6336_PMIC_PD_TX_CTRL
#define MT6336_PD_TX_BIST_CARRIER_MODE2_START_MASK                       0x1
#define MT6336_PD_TX_BIST_CARRIER_MODE2_START_SHIFT                      1
#define MT6336_REG_PD_TX_OS_ADDR                                         MT6336_PMIC_PD_TX_CTRL
#define MT6336_REG_PD_TX_OS_MASK                                         0x7
#define MT6336_REG_PD_TX_OS_SHIFT                                        4
#define MT6336_REG_PD_RX_EN_ADDR                                         MT6336_PMIC_PD_RX_PARAMETER_0
#define MT6336_REG_PD_RX_EN_MASK                                         0x1
#define MT6336_REG_PD_RX_EN_SHIFT                                        0
#define MT6336_REG_PD_RX_SOP_PRIME_RCV_EN_ADDR                           MT6336_PMIC_PD_RX_PARAMETER_0
#define MT6336_REG_PD_RX_SOP_PRIME_RCV_EN_MASK                           0x1
#define MT6336_REG_PD_RX_SOP_PRIME_RCV_EN_SHIFT                          1
#define MT6336_REG_PD_RX_SOP_DPRIME_RCV_EN_ADDR                          MT6336_PMIC_PD_RX_PARAMETER_0
#define MT6336_REG_PD_RX_SOP_DPRIME_RCV_EN_MASK                          0x1
#define MT6336_REG_PD_RX_SOP_DPRIME_RCV_EN_SHIFT                         2
#define MT6336_REG_PD_RX_CABLE_RST_RCV_EN_ADDR                           MT6336_PMIC_PD_RX_PARAMETER_0
#define MT6336_REG_PD_RX_CABLE_RST_RCV_EN_MASK                           0x1
#define MT6336_REG_PD_RX_CABLE_RST_RCV_EN_SHIFT                          3
#define MT6336_REG_PD_RX_PRL_SEND_GCRC_MSG_DIS_BUS_IDLE_OPT_ADDR         MT6336_PMIC_PD_RX_PARAMETER_0
#define MT6336_REG_PD_RX_PRL_SEND_GCRC_MSG_DIS_BUS_IDLE_OPT_MASK         0x1
#define MT6336_REG_PD_RX_PRL_SEND_GCRC_MSG_DIS_BUS_IDLE_OPT_SHIFT        4
#define MT6336_REG_PD_RX_PRE_PROTECT_EN_ADDR                             MT6336_PMIC_PD_RX_PARAMETER_0
#define MT6336_REG_PD_RX_PRE_PROTECT_EN_MASK                             0x1
#define MT6336_REG_PD_RX_PRE_PROTECT_EN_SHIFT                            5
#define MT6336_REG_PD_RX_PRE_TRAIN_BIT_CNT_ADDR                          MT6336_PMIC_PD_RX_PARAMETER_0
#define MT6336_REG_PD_RX_PRE_TRAIN_BIT_CNT_MASK                          0x3
#define MT6336_REG_PD_RX_PRE_TRAIN_BIT_CNT_SHIFT                         6
#define MT6336_REG_PD_RX_PING_MSG_RCV_EN_ADDR                            MT6336_PMIC_PD_RX_PARAMETER_1
#define MT6336_REG_PD_RX_PING_MSG_RCV_EN_MASK                            0x1
#define MT6336_REG_PD_RX_PING_MSG_RCV_EN_SHIFT                           0
#define MT6336_REG_PD_RX_PRE_PROTECT_HALF_UI_CYCLE_CNT_MIN_ADDR          MT6336_PMIC_PD_RX_PREAMBLE_PROTECT_PARAMETER_0
#define MT6336_REG_PD_RX_PRE_PROTECT_HALF_UI_CYCLE_CNT_MIN_MASK          0xFF
#define MT6336_REG_PD_RX_PRE_PROTECT_HALF_UI_CYCLE_CNT_MIN_SHIFT         0
#define MT6336_REG_PD_RX_PRE_PROTECT_UI_CYCLE_CNT_MAX_0_ADDR             MT6336_PMIC_PD_RX_PREAMBLE_PROTECT_PARAMETER_1
#define MT6336_REG_PD_RX_PRE_PROTECT_UI_CYCLE_CNT_MAX_0_MASK             0xFF
#define MT6336_REG_PD_RX_PRE_PROTECT_UI_CYCLE_CNT_MAX_0_SHIFT            0
#define MT6336_REG_PD_RX_PRE_PROTECT_UI_CYCLE_CNT_MAX_1_ADDR             MT6336_PMIC_PD_RX_PREAMBLE_PROTECT_PARAMETER_2
#define MT6336_REG_PD_RX_PRE_PROTECT_UI_CYCLE_CNT_MAX_1_MASK             0x1
#define MT6336_REG_PD_RX_PRE_PROTECT_UI_CYCLE_CNT_MAX_1_SHIFT            0
#define MT6336_RO_PD_RX_OS_ADDR                                          MT6336_PMIC_PD_RX_STATUS
#define MT6336_RO_PD_RX_OS_MASK                                          0x7
#define MT6336_RO_PD_RX_OS_SHIFT                                         0
#define MT6336_RO_PD_RX_DATA_OBJ0_0_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT0_0_0
#define MT6336_RO_PD_RX_DATA_OBJ0_0_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ0_0_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ0_0_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT0_0_1
#define MT6336_RO_PD_RX_DATA_OBJ0_0_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ0_0_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ0_1_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT0_1_0
#define MT6336_RO_PD_RX_DATA_OBJ0_1_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ0_1_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ0_1_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT0_1_1
#define MT6336_RO_PD_RX_DATA_OBJ0_1_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ0_1_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ1_0_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT1_0_0
#define MT6336_RO_PD_RX_DATA_OBJ1_0_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ1_0_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ1_0_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT1_0_1
#define MT6336_RO_PD_RX_DATA_OBJ1_0_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ1_0_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ1_1_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT1_1_0
#define MT6336_RO_PD_RX_DATA_OBJ1_1_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ1_1_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ1_1_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT1_1_1
#define MT6336_RO_PD_RX_DATA_OBJ1_1_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ1_1_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ2_0_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT2_0_0
#define MT6336_RO_PD_RX_DATA_OBJ2_0_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ2_0_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ2_0_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT2_0_1
#define MT6336_RO_PD_RX_DATA_OBJ2_0_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ2_0_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ2_1_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT2_1_0
#define MT6336_RO_PD_RX_DATA_OBJ2_1_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ2_1_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ2_1_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT2_1_1
#define MT6336_RO_PD_RX_DATA_OBJ2_1_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ2_1_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ3_0_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT3_0_0
#define MT6336_RO_PD_RX_DATA_OBJ3_0_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ3_0_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ3_0_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT3_0_1
#define MT6336_RO_PD_RX_DATA_OBJ3_0_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ3_0_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ3_1_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT3_1_0
#define MT6336_RO_PD_RX_DATA_OBJ3_1_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ3_1_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ3_1_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT3_1_1
#define MT6336_RO_PD_RX_DATA_OBJ3_1_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ3_1_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ4_0_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT4_0_0
#define MT6336_RO_PD_RX_DATA_OBJ4_0_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ4_0_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ4_0_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT4_0_1
#define MT6336_RO_PD_RX_DATA_OBJ4_0_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ4_0_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ4_1_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT4_1_0
#define MT6336_RO_PD_RX_DATA_OBJ4_1_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ4_1_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ4_1_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT4_1_1
#define MT6336_RO_PD_RX_DATA_OBJ4_1_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ4_1_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ5_0_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT5_0_0
#define MT6336_RO_PD_RX_DATA_OBJ5_0_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ5_0_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ5_0_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT5_0_1
#define MT6336_RO_PD_RX_DATA_OBJ5_0_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ5_0_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ5_1_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT5_1_0
#define MT6336_RO_PD_RX_DATA_OBJ5_1_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ5_1_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ5_1_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT5_1_1
#define MT6336_RO_PD_RX_DATA_OBJ5_1_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ5_1_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ6_0_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT6_0_0
#define MT6336_RO_PD_RX_DATA_OBJ6_0_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ6_0_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ6_0_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT6_0_1
#define MT6336_RO_PD_RX_DATA_OBJ6_0_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ6_0_1_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ6_1_0_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT6_1_0
#define MT6336_RO_PD_RX_DATA_OBJ6_1_0_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ6_1_0_SHIFT                              0
#define MT6336_RO_PD_RX_DATA_OBJ6_1_1_ADDR                               MT6336_PMIC_PD_RX_DATA_OBJECT6_1_1
#define MT6336_RO_PD_RX_DATA_OBJ6_1_1_MASK                               0xFF
#define MT6336_RO_PD_RX_DATA_OBJ6_1_1_SHIFT                              0
#define MT6336_RO_PD_RX_HDR_0_ADDR                                       MT6336_PMIC_PD_RX_HEADER_0
#define MT6336_RO_PD_RX_HDR_0_MASK                                       0xFF
#define MT6336_RO_PD_RX_HDR_0_SHIFT                                      0
#define MT6336_RO_PD_RX_HDR_1_ADDR                                       MT6336_PMIC_PD_RX_HEADER_1
#define MT6336_RO_PD_RX_HDR_1_MASK                                       0xFF
#define MT6336_RO_PD_RX_HDR_1_SHIFT                                      0
#define MT6336_RO_PD_PHY_RX_UI_CYC_CNT_TRAIN_1P25X_0_ADDR                MT6336_PMIC_PD_RX_1P25X_UI_TRAIN_RESULT_0
#define MT6336_RO_PD_PHY_RX_UI_CYC_CNT_TRAIN_1P25X_0_MASK                0xFF
#define MT6336_RO_PD_PHY_RX_UI_CYC_CNT_TRAIN_1P25X_0_SHIFT               0
#define MT6336_RO_PD_PHY_RX_UI_CYC_CNT_TRAIN_1P25X_1_ADDR                MT6336_PMIC_PD_RX_1P25X_UI_TRAIN_RESULT_1
#define MT6336_RO_PD_PHY_RX_UI_CYC_CNT_TRAIN_1P25X_1_MASK                0x3
#define MT6336_RO_PD_PHY_RX_UI_CYC_CNT_TRAIN_1P25X_1_SHIFT               0
#define MT6336_PD_PRL_RCV_FIFO_SW_RST_ADDR                               MT6336_PMIC_PD_RX_RCV_BUF_SW_RST
#define MT6336_PD_PRL_RCV_FIFO_SW_RST_MASK                               0x1
#define MT6336_PD_PRL_RCV_FIFO_SW_RST_SHIFT                              0
#define MT6336_W1_PD_PE_HR_CPL_ADDR                                      MT6336_PMIC_PD_HR_CTRL
#define MT6336_W1_PD_PE_HR_CPL_MASK                                      0x1
#define MT6336_W1_PD_PE_HR_CPL_SHIFT                                     0
#define MT6336_W1_PD_PE_CR_CPL_ADDR                                      MT6336_PMIC_PD_HR_CTRL
#define MT6336_W1_PD_PE_CR_CPL_MASK                                      0x1
#define MT6336_W1_PD_PE_CR_CPL_SHIFT                                     1
#define MT6336_RO_PD_AD_PD_VCONN_UVP_STATUS_ADDR                         MT6336_PMIC_PD_AD_STATUS
#define MT6336_RO_PD_AD_PD_VCONN_UVP_STATUS_MASK                         0x1
#define MT6336_RO_PD_AD_PD_VCONN_UVP_STATUS_SHIFT                        0
#define MT6336_RO_PD_AD_PD_CC1_OVP_STATUS_ADDR                           MT6336_PMIC_PD_AD_STATUS
#define MT6336_RO_PD_AD_PD_CC1_OVP_STATUS_MASK                           0x1
#define MT6336_RO_PD_AD_PD_CC1_OVP_STATUS_SHIFT                          1
#define MT6336_RO_PD_AD_PD_CC2_OVP_STATUS_ADDR                           MT6336_PMIC_PD_AD_STATUS
#define MT6336_RO_PD_AD_PD_CC2_OVP_STATUS_MASK                           0x1
#define MT6336_RO_PD_AD_PD_CC2_OVP_STATUS_SHIFT                          2
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_0_0_ADDR                       MT6336_PMIC_PD_CRC_RCV_TIMEOUT_VAL_0_0
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_0_0_MASK                       0xFF
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_0_0_SHIFT                      0
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_0_1_ADDR                       MT6336_PMIC_PD_CRC_RCV_TIMEOUT_VAL_0_1
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_0_1_MASK                       0xFF
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_0_1_SHIFT                      0
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_1_0_ADDR                       MT6336_PMIC_PD_CRC_RCV_TIMEOUT_VAL_1_0
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_1_0_MASK                       0xFF
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_1_0_SHIFT                      0
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_1_1_ADDR                       MT6336_PMIC_PD_CRC_RCV_TIMEOUT_VAL_1_1
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_1_1_MASK                       0xFF
#define MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_1_1_SHIFT                      0
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_0_0_ADDR                        MT6336_PMIC_PD_HR_COMPLETE_TIMEOUT_VAL_0_0
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_0_0_MASK                        0xFF
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_0_0_SHIFT                       0
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_0_1_ADDR                        MT6336_PMIC_PD_HR_COMPLETE_TIMEOUT_VAL_0_1
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_0_1_MASK                        0xFF
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_0_1_SHIFT                       0
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_1_0_ADDR                        MT6336_PMIC_PD_HR_COMPLETE_TIMEOUT_VAL_1_0
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_1_0_MASK                        0xFF
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_1_0_SHIFT                       0
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_1_1_ADDR                        MT6336_PMIC_PD_HR_COMPLETE_TIMEOUT_VAL_1_1
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_1_1_MASK                        0xFF
#define MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_1_1_SHIFT                       0
#define MT6336_REG_PD_IDLE_DET_WIN_VAL_0_ADDR                            MT6336_PMIC_PD_IDLE_DETECTION_0_0
#define MT6336_REG_PD_IDLE_DET_WIN_VAL_0_MASK                            0xFF
#define MT6336_REG_PD_IDLE_DET_WIN_VAL_0_SHIFT                           0
#define MT6336_REG_PD_IDLE_DET_WIN_VAL_1_ADDR                            MT6336_PMIC_PD_IDLE_DETECTION_0_1
#define MT6336_REG_PD_IDLE_DET_WIN_VAL_1_MASK                            0xFF
#define MT6336_REG_PD_IDLE_DET_WIN_VAL_1_SHIFT                           0
#define MT6336_REG_PD_IDLE_DET_TRANS_CNT_ADDR                            MT6336_PMIC_PD_IDLE_DETECTION_1_0
#define MT6336_REG_PD_IDLE_DET_TRANS_CNT_MASK                            0x7
#define MT6336_REG_PD_IDLE_DET_TRANS_CNT_SHIFT                           0
#define MT6336_RO_PD_IDLE_DET_STATUS_ADDR                                MT6336_PMIC_PD_IDLE_DETECTION_1_1
#define MT6336_RO_PD_IDLE_DET_STATUS_MASK                                0x1
#define MT6336_RO_PD_IDLE_DET_STATUS_SHIFT                               0
#define MT6336_REG_PD_INTERFRAMEGAP_VAL_0_ADDR                           MT6336_PMIC_PD_INTERFRAMEGAP_VAL_0
#define MT6336_REG_PD_INTERFRAMEGAP_VAL_0_MASK                           0xFF
#define MT6336_REG_PD_INTERFRAMEGAP_VAL_0_SHIFT                          0
#define MT6336_REG_PD_INTERFRAMEGAP_VAL_1_ADDR                           MT6336_PMIC_PD_INTERFRAMEGAP_VAL_1
#define MT6336_REG_PD_INTERFRAMEGAP_VAL_1_MASK                           0xFF
#define MT6336_REG_PD_INTERFRAMEGAP_VAL_1_SHIFT                          0
#define MT6336_REG_PD_RX_GLITCH_MASK_WIN_VAL_ADDR                        MT6336_PMIC_PD_RX_GLITCH_MASK_WINDOW
#define MT6336_REG_PD_RX_GLITCH_MASK_WIN_VAL_MASK                        0x3F
#define MT6336_REG_PD_RX_GLITCH_MASK_WIN_VAL_SHIFT                       0
#define MT6336_REG_PD_RX_UI_1P25X_ADJ_CNT_ADDR                           MT6336_PMIC_PD_RX_UI_1P25_ADJ
#define MT6336_REG_PD_RX_UI_1P25X_ADJ_CNT_MASK                           0xF
#define MT6336_REG_PD_RX_UI_1P25X_ADJ_CNT_SHIFT                          0
#define MT6336_REG_PD_RX_UI_1P25X_ADJ_ADDR                               MT6336_PMIC_PD_RX_UI_1P25_ADJ
#define MT6336_REG_PD_RX_UI_1P25X_ADJ_MASK                               0x1
#define MT6336_REG_PD_RX_UI_1P25X_ADJ_SHIFT                              4
#define MT6336_REG_PD_TIMER0_VAL_0_0_ADDR                                MT6336_PMIC_PD_TIMER0_VAL_0_0
#define MT6336_REG_PD_TIMER0_VAL_0_0_MASK                                0xFF
#define MT6336_REG_PD_TIMER0_VAL_0_0_SHIFT                               0
#define MT6336_REG_PD_TIMER0_VAL_0_1_ADDR                                MT6336_PMIC_PD_TIMER0_VAL_0_1
#define MT6336_REG_PD_TIMER0_VAL_0_1_MASK                                0xFF
#define MT6336_REG_PD_TIMER0_VAL_0_1_SHIFT                               0
#define MT6336_REG_PD_TIMER0_VAL_1_0_ADDR                                MT6336_PMIC_PD_TIMER0_VAL_1_0
#define MT6336_REG_PD_TIMER0_VAL_1_0_MASK                                0xFF
#define MT6336_REG_PD_TIMER0_VAL_1_0_SHIFT                               0
#define MT6336_REG_PD_TIMER0_VAL_1_1_ADDR                                MT6336_PMIC_PD_TIMER0_VAL_1_1
#define MT6336_REG_PD_TIMER0_VAL_1_1_MASK                                0xFF
#define MT6336_REG_PD_TIMER0_VAL_1_1_SHIFT                               0
#define MT6336_PD_TIMER0_EN_ADDR                                         MT6336_PMIC_PD_TIMER0_ENABLE
#define MT6336_PD_TIMER0_EN_MASK                                         0x1
#define MT6336_PD_TIMER0_EN_SHIFT                                        0
#define MT6336_REG_PD_TIMER1_VAL_0_ADDR                                  MT6336_PMIC_PD_TIMER1_VAL_0
#define MT6336_REG_PD_TIMER1_VAL_0_MASK                                  0xFF
#define MT6336_REG_PD_TIMER1_VAL_0_SHIFT                                 0
#define MT6336_REG_PD_TIMER1_VAL_1_ADDR                                  MT6336_PMIC_PD_TIMER1_VAL_1
#define MT6336_REG_PD_TIMER1_VAL_1_MASK                                  0xFF
#define MT6336_REG_PD_TIMER1_VAL_1_SHIFT                                 0
#define MT6336_RO_PD_TIMER1_TICK_CNT_0_ADDR                              MT6336_PMIC_PD_TIMER1_TICK_CNT_0
#define MT6336_RO_PD_TIMER1_TICK_CNT_0_MASK                              0xFF
#define MT6336_RO_PD_TIMER1_TICK_CNT_0_SHIFT                             0
#define MT6336_RO_PD_TIMER1_TICK_CNT_1_ADDR                              MT6336_PMIC_PD_TIMER1_TICK_CNT_1
#define MT6336_RO_PD_TIMER1_TICK_CNT_1_MASK                              0xFF
#define MT6336_RO_PD_TIMER1_TICK_CNT_1_SHIFT                             0
#define MT6336_PD_TIMER1_EN_ADDR                                         MT6336_PMIC_PD_TIMER1_ENABLE
#define MT6336_PD_TIMER1_EN_MASK                                         0x1
#define MT6336_PD_TIMER1_EN_SHIFT                                        0
#define MT6336_REG_PD_TX_SLEW_CALI_AUTO_EN_ADDR                          MT6336_PMIC_PD_TX_SLEW_RATE_CALI_CTRL
#define MT6336_REG_PD_TX_SLEW_CALI_AUTO_EN_MASK                          0x1
#define MT6336_REG_PD_TX_SLEW_CALI_AUTO_EN_SHIFT                         0
#define MT6336_REG_PD_TX_SLEW_CALI_SLEW_CK_SW_EN_ADDR                    MT6336_PMIC_PD_TX_SLEW_RATE_CALI_CTRL
#define MT6336_REG_PD_TX_SLEW_CALI_SLEW_CK_SW_EN_MASK                    0x1
#define MT6336_REG_PD_TX_SLEW_CALI_SLEW_CK_SW_EN_SHIFT                   1
#define MT6336_REG_PD_TX_SLEW_CALI_LOCK_TARGET_CNT_ADDR                  MT6336_PMIC_PD_TX_SLEW_RATE_CALI_CTRL
#define MT6336_REG_PD_TX_SLEW_CALI_LOCK_TARGET_CNT_MASK                  0xF
#define MT6336_REG_PD_TX_SLEW_CALI_LOCK_TARGET_CNT_SHIFT                 4
#define MT6336_REG_PD_TX_SLEW_CK_STABLE_TIME_0_ADDR                      MT6336_PMIC_PD_TX_SLEW_CK_STABLE_TIME_0
#define MT6336_REG_PD_TX_SLEW_CK_STABLE_TIME_0_MASK                      0xFF
#define MT6336_REG_PD_TX_SLEW_CK_STABLE_TIME_0_SHIFT                     0
#define MT6336_REG_PD_TX_SLEW_CK_STABLE_TIME_1_ADDR                      MT6336_PMIC_PD_TX_SLEW_CK_STABLE_TIME_1
#define MT6336_REG_PD_TX_SLEW_CK_STABLE_TIME_1_MASK                      0xF
#define MT6336_REG_PD_TX_SLEW_CK_STABLE_TIME_1_SHIFT                     0
#define MT6336_REG_PD_TX_MON_CK_TARGET_CYC_CNT_0_ADDR                    MT6336_PMIC_PD_TX_MON_CK_TARGET_0
#define MT6336_REG_PD_TX_MON_CK_TARGET_CYC_CNT_0_MASK                    0xFF
#define MT6336_REG_PD_TX_MON_CK_TARGET_CYC_CNT_0_SHIFT                   0
#define MT6336_REG_PD_TX_MON_CK_TARGET_CYC_CNT_1_ADDR                    MT6336_PMIC_PD_TX_MON_CK_TARGET_1
#define MT6336_REG_PD_TX_MON_CK_TARGET_CYC_CNT_1_MASK                    0xFF
#define MT6336_REG_PD_TX_MON_CK_TARGET_CYC_CNT_1_SHIFT                   0
#define MT6336_REG_PD_TX_SLEW_CK_TARGET_CYC_CNT_ADDR                     MT6336_PMIC_PD_TX_SLEW_CK_TARGET
#define MT6336_REG_PD_TX_SLEW_CK_TARGET_CYC_CNT_MASK                     0xFF
#define MT6336_REG_PD_TX_SLEW_CK_TARGET_CYC_CNT_SHIFT                    0
#define MT6336_RO_PD_TX_SLEW_CALI_OK_ADDR                                MT6336_PMIC_PD_TX_SLEW_RATE_CALI_RESULT
#define MT6336_RO_PD_TX_SLEW_CALI_OK_MASK                                0x1
#define MT6336_RO_PD_TX_SLEW_CALI_OK_SHIFT                               0
#define MT6336_RO_PD_TX_SLEW_CALI_FAIL_ADDR                              MT6336_PMIC_PD_TX_SLEW_RATE_CALI_RESULT
#define MT6336_RO_PD_TX_SLEW_CALI_FAIL_MASK                              0x1
#define MT6336_RO_PD_TX_SLEW_CALI_FAIL_SHIFT                             1
#define MT6336_RO_PD_TXSLEW_I_CALI_ADDR                                  MT6336_PMIC_PD_TX_SLEW_RATE_CALI_RESULT
#define MT6336_RO_PD_TXSLEW_I_CALI_MASK                                  0x7
#define MT6336_RO_PD_TXSLEW_I_CALI_SHIFT                                 4
#define MT6336_RO_PD_FM_OUT_0_ADDR                                       MT6336_PMIC_PD_TX_SLEW_RATE_FM_OUT_0
#define MT6336_RO_PD_FM_OUT_0_MASK                                       0xFF
#define MT6336_RO_PD_FM_OUT_0_SHIFT                                      0
#define MT6336_RO_PD_FM_OUT_1_ADDR                                       MT6336_PMIC_PD_TX_SLEW_RATE_FM_OUT_1
#define MT6336_RO_PD_FM_OUT_1_MASK                                       0xFF
#define MT6336_RO_PD_FM_OUT_1_SHIFT                                      0
#define MT6336_PD_LB_EN_ADDR                                             MT6336_PMIC_PD_LOOPBACK_CTRL
#define MT6336_PD_LB_EN_MASK                                             0x1
#define MT6336_PD_LB_EN_SHIFT                                            0
#define MT6336_PD_LB_CHK_EN_ADDR                                         MT6336_PMIC_PD_LOOPBACK_CTRL
#define MT6336_PD_LB_CHK_EN_MASK                                         0x1
#define MT6336_PD_LB_CHK_EN_SHIFT                                        1
#define MT6336_RO_PD_LB_RUN_ADDR                                         MT6336_PMIC_PD_LOOPBACK_CTRL
#define MT6336_RO_PD_LB_RUN_MASK                                         0x1
#define MT6336_RO_PD_LB_RUN_SHIFT                                        4
#define MT6336_RO_PD_LB_OK_ADDR                                          MT6336_PMIC_PD_LOOPBACK_CTRL
#define MT6336_RO_PD_LB_OK_MASK                                          0x1
#define MT6336_RO_PD_LB_OK_SHIFT                                         5
#define MT6336_RO_PD_LB_ERR_CNT_ADDR                                     MT6336_PMIC_PD_LOOPBACK_ERR_CNT
#define MT6336_RO_PD_LB_ERR_CNT_MASK                                     0xFF
#define MT6336_RO_PD_LB_ERR_CNT_SHIFT                                    0
#define MT6336_REG_PD_SW_MSG_ID_ADDR                                     MT6336_PMIC_PD_MSG_ID_SW_MODE
#define MT6336_REG_PD_SW_MSG_ID_MASK                                     0x7
#define MT6336_REG_PD_SW_MSG_ID_SHIFT                                    0
#define MT6336_PD_SW_MSG_ID_SYNC_ADDR                                    MT6336_PMIC_PD_MSG_ID_SW_MODE
#define MT6336_PD_SW_MSG_ID_SYNC_MASK                                    0x1
#define MT6336_PD_SW_MSG_ID_SYNC_SHIFT                                   4
#define MT6336_REG_PD_TX_DONE_INTR_EN_ADDR                               MT6336_PMIC_PD_INTR_EN_0
#define MT6336_REG_PD_TX_DONE_INTR_EN_MASK                               0x1
#define MT6336_REG_PD_TX_DONE_INTR_EN_SHIFT                              0
#define MT6336_REG_PD_TX_RETRY_ERR_INTR_EN_ADDR                          MT6336_PMIC_PD_INTR_EN_0
#define MT6336_REG_PD_TX_RETRY_ERR_INTR_EN_MASK                          0x1
#define MT6336_REG_PD_TX_RETRY_ERR_INTR_EN_SHIFT                         1
#define MT6336_REG_PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR_EN_ADDR            MT6336_PMIC_PD_INTR_EN_0
#define MT6336_REG_PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR_EN_MASK            0x1
#define MT6336_REG_PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR_EN_SHIFT           2
#define MT6336_REG_PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR_EN_ADDR          MT6336_PMIC_PD_INTR_EN_0
#define MT6336_REG_PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR_EN_MASK          0x1
#define MT6336_REG_PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR_EN_SHIFT         3
#define MT6336_REG_PD_TX_DIS_BUS_REIDLE_INTR_EN_ADDR                     MT6336_PMIC_PD_INTR_EN_0
#define MT6336_REG_PD_TX_DIS_BUS_REIDLE_INTR_EN_MASK                     0x1
#define MT6336_REG_PD_TX_DIS_BUS_REIDLE_INTR_EN_SHIFT                    4
#define MT6336_REG_PD_TX_CRC_RCV_TIMEOUT_INTR_EN_ADDR                    MT6336_PMIC_PD_INTR_EN_0
#define MT6336_REG_PD_TX_CRC_RCV_TIMEOUT_INTR_EN_MASK                    0x1
#define MT6336_REG_PD_TX_CRC_RCV_TIMEOUT_INTR_EN_SHIFT                   5
#define MT6336_REG_PD_TX_AUTO_SR_DONE_INTR_EN_ADDR                       MT6336_PMIC_PD_INTR_EN_0
#define MT6336_REG_PD_TX_AUTO_SR_DONE_INTR_EN_MASK                       0x1
#define MT6336_REG_PD_TX_AUTO_SR_DONE_INTR_EN_SHIFT                      6
#define MT6336_REG_PD_TX_AUTO_SR_RETRY_ERR_INTR_EN_ADDR                  MT6336_PMIC_PD_INTR_EN_0
#define MT6336_REG_PD_TX_AUTO_SR_RETRY_ERR_INTR_EN_MASK                  0x1
#define MT6336_REG_PD_TX_AUTO_SR_RETRY_ERR_INTR_EN_SHIFT                 7
#define MT6336_REG_PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR_EN_ADDR    MT6336_PMIC_PD_INTR_EN_1
#define MT6336_REG_PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR_EN_MASK    0x1
#define MT6336_REG_PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR_EN_SHIFT   0
#define MT6336_REG_PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR_EN_ADDR  MT6336_PMIC_PD_INTR_EN_1
#define MT6336_REG_PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR_EN_MASK  0x1
#define MT6336_REG_PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR_EN_SHIFT 1
#define MT6336_REG_PD_RX_RCV_MSG_INTR_EN_ADDR                            MT6336_PMIC_PD_INTR_EN_1
#define MT6336_REG_PD_RX_RCV_MSG_INTR_EN_MASK                            0x1
#define MT6336_REG_PD_RX_RCV_MSG_INTR_EN_SHIFT                           4
#define MT6336_REG_PD_RX_LENGTH_MIS_INTR_EN_ADDR                         MT6336_PMIC_PD_INTR_EN_1
#define MT6336_REG_PD_RX_LENGTH_MIS_INTR_EN_MASK                         0x1
#define MT6336_REG_PD_RX_LENGTH_MIS_INTR_EN_SHIFT                        5
#define MT6336_REG_PD_RX_DUPLICATE_INTR_EN_ADDR                          MT6336_PMIC_PD_INTR_EN_1
#define MT6336_REG_PD_RX_DUPLICATE_INTR_EN_MASK                          0x1
#define MT6336_REG_PD_RX_DUPLICATE_INTR_EN_SHIFT                         6
#define MT6336_REG_PD_RX_TRANS_GCRC_FAIL_INTR_EN_ADDR                    MT6336_PMIC_PD_INTR_EN_1
#define MT6336_REG_PD_RX_TRANS_GCRC_FAIL_INTR_EN_MASK                    0x1
#define MT6336_REG_PD_RX_TRANS_GCRC_FAIL_INTR_EN_SHIFT                   7
#define MT6336_REG_PD_HR_TRANS_CPL_TIMEOUT_INTR_EN_ADDR                  MT6336_PMIC_PD_INTR_EN_2
#define MT6336_REG_PD_HR_TRANS_CPL_TIMEOUT_INTR_EN_MASK                  0x1
#define MT6336_REG_PD_HR_TRANS_CPL_TIMEOUT_INTR_EN_SHIFT                 0
#define MT6336_REG_PD_HR_TRANS_DONE_INTR_EN_ADDR                         MT6336_PMIC_PD_INTR_EN_2
#define MT6336_REG_PD_HR_TRANS_DONE_INTR_EN_MASK                         0x1
#define MT6336_REG_PD_HR_TRANS_DONE_INTR_EN_SHIFT                        1
#define MT6336_REG_PD_HR_RCV_DONE_INTR_EN_ADDR                           MT6336_PMIC_PD_INTR_EN_2
#define MT6336_REG_PD_HR_RCV_DONE_INTR_EN_MASK                           0x1
#define MT6336_REG_PD_HR_RCV_DONE_INTR_EN_SHIFT                          2
#define MT6336_REG_PD_HR_TRANS_FAIL_INTR_EN_ADDR                         MT6336_PMIC_PD_INTR_EN_2
#define MT6336_REG_PD_HR_TRANS_FAIL_INTR_EN_MASK                         0x1
#define MT6336_REG_PD_HR_TRANS_FAIL_INTR_EN_SHIFT                        3
#define MT6336_REG_PD_AD_PD_VCONN_UVP_INTR_EN_ADDR                       MT6336_PMIC_PD_INTR_EN_3
#define MT6336_REG_PD_AD_PD_VCONN_UVP_INTR_EN_MASK                       0x1
#define MT6336_REG_PD_AD_PD_VCONN_UVP_INTR_EN_SHIFT                      0
#define MT6336_REG_PD_AD_PD_CC1_OVP_INTR_EN_ADDR                         MT6336_PMIC_PD_INTR_EN_3
#define MT6336_REG_PD_AD_PD_CC1_OVP_INTR_EN_MASK                         0x1
#define MT6336_REG_PD_AD_PD_CC1_OVP_INTR_EN_SHIFT                        1
#define MT6336_REG_PD_AD_PD_CC2_OVP_INTR_EN_ADDR                         MT6336_PMIC_PD_INTR_EN_3
#define MT6336_REG_PD_AD_PD_CC2_OVP_INTR_EN_MASK                         0x1
#define MT6336_REG_PD_AD_PD_CC2_OVP_INTR_EN_SHIFT                        2
#define MT6336_REG_PD_TIMER0_TIMEOUT_INTR_EN_ADDR                        MT6336_PMIC_PD_INTR_EN_3
#define MT6336_REG_PD_TIMER0_TIMEOUT_INTR_EN_MASK                        0x1
#define MT6336_REG_PD_TIMER0_TIMEOUT_INTR_EN_SHIFT                       4
#define MT6336_REG_PD_TIMER1_TIMEOUT_INTR_EN_ADDR                        MT6336_PMIC_PD_INTR_EN_3
#define MT6336_REG_PD_TIMER1_TIMEOUT_INTR_EN_MASK                        0x1
#define MT6336_REG_PD_TIMER1_TIMEOUT_INTR_EN_SHIFT                       5
#define MT6336_PD_TX_DONE_INTR_ADDR                                      MT6336_PMIC_PD_INTR_0
#define MT6336_PD_TX_DONE_INTR_MASK                                      0x1
#define MT6336_PD_TX_DONE_INTR_SHIFT                                     0
#define MT6336_PD_TX_RETRY_ERR_INTR_ADDR                                 MT6336_PMIC_PD_INTR_0
#define MT6336_PD_TX_RETRY_ERR_INTR_MASK                                 0x1
#define MT6336_PD_TX_RETRY_ERR_INTR_SHIFT                                1
#define MT6336_PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR_ADDR                   MT6336_PMIC_PD_INTR_0
#define MT6336_PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR_MASK                   0x1
#define MT6336_PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR_SHIFT                  2
#define MT6336_PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR_ADDR                 MT6336_PMIC_PD_INTR_0
#define MT6336_PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR_MASK                 0x1
#define MT6336_PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR_SHIFT                3
#define MT6336_PD_TX_DIS_BUS_REIDLE_INTR_ADDR                            MT6336_PMIC_PD_INTR_0
#define MT6336_PD_TX_DIS_BUS_REIDLE_INTR_MASK                            0x1
#define MT6336_PD_TX_DIS_BUS_REIDLE_INTR_SHIFT                           4
#define MT6336_PD_TX_CRC_RCV_TIMEOUT_INTR_ADDR                           MT6336_PMIC_PD_INTR_0
#define MT6336_PD_TX_CRC_RCV_TIMEOUT_INTR_MASK                           0x1
#define MT6336_PD_TX_CRC_RCV_TIMEOUT_INTR_SHIFT                          5
#define MT6336_PD_TX_AUTO_SR_DONE_INTR_ADDR                              MT6336_PMIC_PD_INTR_0
#define MT6336_PD_TX_AUTO_SR_DONE_INTR_MASK                              0x1
#define MT6336_PD_TX_AUTO_SR_DONE_INTR_SHIFT                             6
#define MT6336_PD_TX_AUTO_SR_RETRY_ERR_INTR_ADDR                         MT6336_PMIC_PD_INTR_0
#define MT6336_PD_TX_AUTO_SR_RETRY_ERR_INTR_MASK                         0x1
#define MT6336_PD_TX_AUTO_SR_RETRY_ERR_INTR_SHIFT                        7
#define MT6336_PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR_ADDR           MT6336_PMIC_PD_INTR_1
#define MT6336_PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR_MASK           0x1
#define MT6336_PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR_SHIFT          0
#define MT6336_PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR_ADDR         MT6336_PMIC_PD_INTR_1
#define MT6336_PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR_MASK         0x1
#define MT6336_PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR_SHIFT        1
#define MT6336_PD_RX_RCV_MSG_INTR_ADDR                                   MT6336_PMIC_PD_INTR_1
#define MT6336_PD_RX_RCV_MSG_INTR_MASK                                   0x1
#define MT6336_PD_RX_RCV_MSG_INTR_SHIFT                                  4
#define MT6336_PD_RX_LENGTH_MIS_INTR_ADDR                                MT6336_PMIC_PD_INTR_1
#define MT6336_PD_RX_LENGTH_MIS_INTR_MASK                                0x1
#define MT6336_PD_RX_LENGTH_MIS_INTR_SHIFT                               5
#define MT6336_PD_RX_DUPLICATE_INTR_ADDR                                 MT6336_PMIC_PD_INTR_1
#define MT6336_PD_RX_DUPLICATE_INTR_MASK                                 0x1
#define MT6336_PD_RX_DUPLICATE_INTR_SHIFT                                6
#define MT6336_PD_RX_TRANS_GCRC_FAIL_INTR_ADDR                           MT6336_PMIC_PD_INTR_1
#define MT6336_PD_RX_TRANS_GCRC_FAIL_INTR_MASK                           0x1
#define MT6336_PD_RX_TRANS_GCRC_FAIL_INTR_SHIFT                          7
#define MT6336_PD_HR_TRANS_CPL_TIMEOUT_INTR_ADDR                         MT6336_PMIC_PD_INTR_2
#define MT6336_PD_HR_TRANS_CPL_TIMEOUT_INTR_MASK                         0x1
#define MT6336_PD_HR_TRANS_CPL_TIMEOUT_INTR_SHIFT                        0
#define MT6336_PD_HR_TRANS_DONE_INTR_ADDR                                MT6336_PMIC_PD_INTR_2
#define MT6336_PD_HR_TRANS_DONE_INTR_MASK                                0x1
#define MT6336_PD_HR_TRANS_DONE_INTR_SHIFT                               1
#define MT6336_PD_HR_RCV_DONE_INTR_ADDR                                  MT6336_PMIC_PD_INTR_2
#define MT6336_PD_HR_RCV_DONE_INTR_MASK                                  0x1
#define MT6336_PD_HR_RCV_DONE_INTR_SHIFT                                 2
#define MT6336_PD_HR_TRANS_FAIL_INTR_ADDR                                MT6336_PMIC_PD_INTR_2
#define MT6336_PD_HR_TRANS_FAIL_INTR_MASK                                0x1
#define MT6336_PD_HR_TRANS_FAIL_INTR_SHIFT                               3
#define MT6336_PD_AD_PD_VCONN_UVP_INTR_ADDR                              MT6336_PMIC_PD_INTR_3
#define MT6336_PD_AD_PD_VCONN_UVP_INTR_MASK                              0x1
#define MT6336_PD_AD_PD_VCONN_UVP_INTR_SHIFT                             0
#define MT6336_PD_AD_PD_CC1_OVP_INTR_ADDR                                MT6336_PMIC_PD_INTR_3
#define MT6336_PD_AD_PD_CC1_OVP_INTR_MASK                                0x1
#define MT6336_PD_AD_PD_CC1_OVP_INTR_SHIFT                               1
#define MT6336_PD_AD_PD_CC2_OVP_INTR_ADDR                                MT6336_PMIC_PD_INTR_3
#define MT6336_PD_AD_PD_CC2_OVP_INTR_MASK                                0x1
#define MT6336_PD_AD_PD_CC2_OVP_INTR_SHIFT                               2
#define MT6336_PD_TIMER0_TIMEOUT_INTR_ADDR                               MT6336_PMIC_PD_INTR_3
#define MT6336_PD_TIMER0_TIMEOUT_INTR_MASK                               0x1
#define MT6336_PD_TIMER0_TIMEOUT_INTR_SHIFT                              4
#define MT6336_PD_TIMER1_TIMEOUT_INTR_ADDR                               MT6336_PMIC_PD_INTR_3
#define MT6336_PD_TIMER1_TIMEOUT_INTR_MASK                               0x1
#define MT6336_PD_TIMER1_TIMEOUT_INTR_SHIFT                              5
#define MT6336_REG_TYPE_C_PHY_RG_PD_TXSLEW_CALEN_ADDR                    MT6336_PMIC_PD_PHY_RG_PD_0
#define MT6336_REG_TYPE_C_PHY_RG_PD_TXSLEW_CALEN_MASK                    0x1
#define MT6336_REG_TYPE_C_PHY_RG_PD_TXSLEW_CALEN_SHIFT                   0
#define MT6336_REG_TYPE_C_PHY_RG_PD_RXLPF_2ND_EN_ADDR                    MT6336_PMIC_PD_PHY_RG_PD_0
#define MT6336_REG_TYPE_C_PHY_RG_PD_RXLPF_2ND_EN_MASK                    0x1
#define MT6336_REG_TYPE_C_PHY_RG_PD_RXLPF_2ND_EN_SHIFT                   1
#define MT6336_REG_TYPE_C_PHY_RG_PD_RX_MODE_ADDR                         MT6336_PMIC_PD_PHY_RG_PD_0
#define MT6336_REG_TYPE_C_PHY_RG_PD_RX_MODE_MASK                         0x3
#define MT6336_REG_TYPE_C_PHY_RG_PD_RX_MODE_SHIFT                        2
#define MT6336_REG_TYPE_C_PHY_RG_PD_UVP_VTH_ADDR                         MT6336_PMIC_PD_PHY_RG_PD_1
#define MT6336_REG_TYPE_C_PHY_RG_PD_UVP_VTH_MASK                         0x7
#define MT6336_REG_TYPE_C_PHY_RG_PD_UVP_VTH_SHIFT                        0
#define MT6336_REG_TYPE_C_PHY_RG_PD_UVP_EN_ADDR                          MT6336_PMIC_PD_PHY_RG_PD_1
#define MT6336_REG_TYPE_C_PHY_RG_PD_UVP_EN_MASK                          0x1
#define MT6336_REG_TYPE_C_PHY_RG_PD_UVP_EN_SHIFT                         3
#define MT6336_REG_TYPE_C_PHY_RG_PD_UVP_SEL_ADDR                         MT6336_PMIC_PD_PHY_RG_PD_1
#define MT6336_REG_TYPE_C_PHY_RG_PD_UVP_SEL_MASK                         0x1
#define MT6336_REG_TYPE_C_PHY_RG_PD_UVP_SEL_SHIFT                        4
#define MT6336_REG_TYPE_C_PHY_RG_PD_TXSLEW_I_ADDR                        MT6336_PMIC_PD_PHY_RG_PD_2
#define MT6336_REG_TYPE_C_PHY_RG_PD_TXSLEW_I_MASK                        0x7
#define MT6336_REG_TYPE_C_PHY_RG_PD_TXSLEW_I_SHIFT                       0
#define MT6336_REG_TYPE_C_PHY_RG_PD_TX_AMP_ADDR                          MT6336_PMIC_PD_PHY_RG_PD_3
#define MT6336_REG_TYPE_C_PHY_RG_PD_TX_AMP_MASK                          0xF
#define MT6336_REG_TYPE_C_PHY_RG_PD_TX_AMP_SHIFT                         0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_BIAS_EN_ADDR            MT6336_PMIC_PD_PHY_SW_FORCE_MODE_ENABLE
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_BIAS_EN_MASK            0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_BIAS_EN_SHIFT           0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_EN_ADDR              MT6336_PMIC_PD_PHY_SW_FORCE_MODE_ENABLE
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_EN_MASK              0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_EN_SHIFT             1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_DATA_ADDR            MT6336_PMIC_PD_PHY_SW_FORCE_MODE_ENABLE
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_DATA_MASK            0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_DATA_SHIFT           2
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_RX_EN_ADDR              MT6336_PMIC_PD_PHY_SW_FORCE_MODE_ENABLE
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_RX_EN_MASK              0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_RX_EN_SHIFT             3
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CCSW_ADDR               MT6336_PMIC_PD_PHY_SW_FORCE_MODE_ENABLE
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CCSW_MASK               0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CCSW_SHIFT              4
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CONNSW_ADDR             MT6336_PMIC_PD_PHY_SW_FORCE_MODE_ENABLE
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CONNSW_MASK             0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CONNSW_SHIFT            5
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_BIAS_EN_ADDR               MT6336_PMIC_PD_PHY_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_BIAS_EN_MASK               0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_BIAS_EN_SHIFT              0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_EN_ADDR                 MT6336_PMIC_PD_PHY_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_EN_MASK                 0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_EN_SHIFT                1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_DATA_ADDR               MT6336_PMIC_PD_PHY_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_DATA_MASK               0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_DATA_SHIFT              2
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_RX_EN_ADDR                 MT6336_PMIC_PD_PHY_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_RX_EN_MASK                 0x1
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_RX_EN_SHIFT                3
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_CCSW_ADDR                  MT6336_PMIC_PD_PHY_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_CCSW_MASK                  0x3
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_CCSW_SHIFT                 4
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_CONNSW_ADDR                MT6336_PMIC_PD_PHY_SW_FORCE_MODE_VAL_0
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_CONNSW_MASK                0x3
#define MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_CONNSW_SHIFT               6
#define MT6336_AUXADC_ADC_OUT_CH0_L_ADDR                                 MT6336_PMIC_AUXADC_ADC0
#define MT6336_AUXADC_ADC_OUT_CH0_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_CH0_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_CH0_H_ADDR                                 MT6336_PMIC_AUXADC_ADC0_H
#define MT6336_AUXADC_ADC_OUT_CH0_H_MASK                                 0x7F
#define MT6336_AUXADC_ADC_OUT_CH0_H_SHIFT                                0
#define MT6336_AUXADC_ADC_RDY_CH0_ADDR                                   MT6336_PMIC_AUXADC_ADC0_H
#define MT6336_AUXADC_ADC_RDY_CH0_MASK                                   0x1
#define MT6336_AUXADC_ADC_RDY_CH0_SHIFT                                  7
#define MT6336_AUXADC_ADC_OUT_CH1_L_ADDR                                 MT6336_PMIC_AUXADC_ADC1
#define MT6336_AUXADC_ADC_OUT_CH1_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_CH1_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_CH1_H_ADDR                                 MT6336_PMIC_AUXADC_ADC1_H
#define MT6336_AUXADC_ADC_OUT_CH1_H_MASK                                 0x7F
#define MT6336_AUXADC_ADC_OUT_CH1_H_SHIFT                                0
#define MT6336_AUXADC_ADC_RDY_CH1_ADDR                                   MT6336_PMIC_AUXADC_ADC1_H
#define MT6336_AUXADC_ADC_RDY_CH1_MASK                                   0x1
#define MT6336_AUXADC_ADC_RDY_CH1_SHIFT                                  7
#define MT6336_AUXADC_ADC_OUT_CH2_L_ADDR                                 MT6336_PMIC_AUXADC_ADC2
#define MT6336_AUXADC_ADC_OUT_CH2_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_CH2_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_CH2_H_ADDR                                 MT6336_PMIC_AUXADC_ADC2_H
#define MT6336_AUXADC_ADC_OUT_CH2_H_MASK                                 0x7F
#define MT6336_AUXADC_ADC_OUT_CH2_H_SHIFT                                0
#define MT6336_AUXADC_ADC_RDY_CH2_ADDR                                   MT6336_PMIC_AUXADC_ADC2_H
#define MT6336_AUXADC_ADC_RDY_CH2_MASK                                   0x1
#define MT6336_AUXADC_ADC_RDY_CH2_SHIFT                                  7
#define MT6336_AUXADC_ADC_OUT_CH3_L_ADDR                                 MT6336_PMIC_AUXADC_ADC3
#define MT6336_AUXADC_ADC_OUT_CH3_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_CH3_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_CH3_H_ADDR                                 MT6336_PMIC_AUXADC_ADC3_H
#define MT6336_AUXADC_ADC_OUT_CH3_H_MASK                                 0xF
#define MT6336_AUXADC_ADC_OUT_CH3_H_SHIFT                                0
#define MT6336_AUXADC_ADC_RDY_CH3_ADDR                                   MT6336_PMIC_AUXADC_ADC3_H
#define MT6336_AUXADC_ADC_RDY_CH3_MASK                                   0x1
#define MT6336_AUXADC_ADC_RDY_CH3_SHIFT                                  7
#define MT6336_AUXADC_ADC_OUT_CH4_L_ADDR                                 MT6336_PMIC_AUXADC_ADC4
#define MT6336_AUXADC_ADC_OUT_CH4_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_CH4_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_CH4_H_ADDR                                 MT6336_PMIC_AUXADC_ADC4_H
#define MT6336_AUXADC_ADC_OUT_CH4_H_MASK                                 0xF
#define MT6336_AUXADC_ADC_OUT_CH4_H_SHIFT                                0
#define MT6336_AUXADC_ADC_RDY_CH4_ADDR                                   MT6336_PMIC_AUXADC_ADC4_H
#define MT6336_AUXADC_ADC_RDY_CH4_MASK                                   0x1
#define MT6336_AUXADC_ADC_RDY_CH4_SHIFT                                  7
#define MT6336_AUXADC_ADC_OUT_CH5_L_ADDR                                 MT6336_PMIC_AUXADC_ADC5
#define MT6336_AUXADC_ADC_OUT_CH5_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_CH5_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_CH5_H_ADDR                                 MT6336_PMIC_AUXADC_ADC5_H
#define MT6336_AUXADC_ADC_OUT_CH5_H_MASK                                 0xF
#define MT6336_AUXADC_ADC_OUT_CH5_H_SHIFT                                0
#define MT6336_AUXADC_ADC_RDY_CH5_ADDR                                   MT6336_PMIC_AUXADC_ADC5_H
#define MT6336_AUXADC_ADC_RDY_CH5_MASK                                   0x1
#define MT6336_AUXADC_ADC_RDY_CH5_SHIFT                                  7
#define MT6336_AUXADC_ADC_OUT_CH6_L_ADDR                                 MT6336_PMIC_AUXADC_ADC6
#define MT6336_AUXADC_ADC_OUT_CH6_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_CH6_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_CH6_H_ADDR                                 MT6336_PMIC_AUXADC_ADC6_H
#define MT6336_AUXADC_ADC_OUT_CH6_H_MASK                                 0xF
#define MT6336_AUXADC_ADC_OUT_CH6_H_SHIFT                                0
#define MT6336_AUXADC_ADC_RDY_CH6_ADDR                                   MT6336_PMIC_AUXADC_ADC6_H
#define MT6336_AUXADC_ADC_RDY_CH6_MASK                                   0x1
#define MT6336_AUXADC_ADC_RDY_CH6_SHIFT                                  7
#define MT6336_AUXADC_ADC_OUT_CH7_L_ADDR                                 MT6336_PMIC_AUXADC_ADC7
#define MT6336_AUXADC_ADC_OUT_CH7_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_CH7_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_CH7_H_ADDR                                 MT6336_PMIC_AUXADC_ADC7_H
#define MT6336_AUXADC_ADC_OUT_CH7_H_MASK                                 0x7F
#define MT6336_AUXADC_ADC_OUT_CH7_H_SHIFT                                0
#define MT6336_AUXADC_ADC_RDY_CH7_ADDR                                   MT6336_PMIC_AUXADC_ADC7_H
#define MT6336_AUXADC_ADC_RDY_CH7_MASK                                   0x1
#define MT6336_AUXADC_ADC_RDY_CH7_SHIFT                                  7
#define MT6336_AUXADC_ADC_OUT_CH8_L_ADDR                                 MT6336_PMIC_AUXADC_ADC8
#define MT6336_AUXADC_ADC_OUT_CH8_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_CH8_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_CH8_H_ADDR                                 MT6336_PMIC_AUXADC_ADC8_H
#define MT6336_AUXADC_ADC_OUT_CH8_H_MASK                                 0xF
#define MT6336_AUXADC_ADC_OUT_CH8_H_SHIFT                                0
#define MT6336_AUXADC_ADC_RDY_CH8_ADDR                                   MT6336_PMIC_AUXADC_ADC8_H
#define MT6336_AUXADC_ADC_RDY_CH8_MASK                                   0x1
#define MT6336_AUXADC_ADC_RDY_CH8_SHIFT                                  7
#define MT6336_AUXADC_ADC_OUT_CH9_L_ADDR                                 MT6336_PMIC_AUXADC_ADC9
#define MT6336_AUXADC_ADC_OUT_CH9_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_CH9_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_CH9_H_ADDR                                 MT6336_PMIC_AUXADC_ADC9_H
#define MT6336_AUXADC_ADC_OUT_CH9_H_MASK                                 0xF
#define MT6336_AUXADC_ADC_OUT_CH9_H_SHIFT                                0
#define MT6336_AUXADC_ADC_RDY_CH9_ADDR                                   MT6336_PMIC_AUXADC_ADC9_H
#define MT6336_AUXADC_ADC_RDY_CH9_MASK                                   0x1
#define MT6336_AUXADC_ADC_RDY_CH9_SHIFT                                  7
#define MT6336_AUXADC_ADC_OUT_CH10_L_ADDR                                MT6336_PMIC_AUXADC_ADC10
#define MT6336_AUXADC_ADC_OUT_CH10_L_MASK                                0xFF
#define MT6336_AUXADC_ADC_OUT_CH10_L_SHIFT                               0
#define MT6336_AUXADC_ADC_OUT_CH10_H_ADDR                                MT6336_PMIC_AUXADC_ADC10_H
#define MT6336_AUXADC_ADC_OUT_CH10_H_MASK                                0xF
#define MT6336_AUXADC_ADC_OUT_CH10_H_SHIFT                               0
#define MT6336_AUXADC_ADC_RDY_CH10_ADDR                                  MT6336_PMIC_AUXADC_ADC10_H
#define MT6336_AUXADC_ADC_RDY_CH10_MASK                                  0x1
#define MT6336_AUXADC_ADC_RDY_CH10_SHIFT                                 7
#define MT6336_AUXADC_ADC_OUT_CH11_L_ADDR                                MT6336_PMIC_AUXADC_ADC11
#define MT6336_AUXADC_ADC_OUT_CH11_L_MASK                                0xFF
#define MT6336_AUXADC_ADC_OUT_CH11_L_SHIFT                               0
#define MT6336_AUXADC_ADC_OUT_CH11_H_ADDR                                MT6336_PMIC_AUXADC_ADC11_H
#define MT6336_AUXADC_ADC_OUT_CH11_H_MASK                                0xF
#define MT6336_AUXADC_ADC_OUT_CH11_H_SHIFT                               0
#define MT6336_AUXADC_ADC_RDY_CH11_ADDR                                  MT6336_PMIC_AUXADC_ADC11_H
#define MT6336_AUXADC_ADC_RDY_CH11_MASK                                  0x1
#define MT6336_AUXADC_ADC_RDY_CH11_SHIFT                                 7
#define MT6336_AUXADC_ADC_OUT_CH12_15_L_ADDR                             MT6336_PMIC_AUXADC_ADC12
#define MT6336_AUXADC_ADC_OUT_CH12_15_L_MASK                             0xFF
#define MT6336_AUXADC_ADC_OUT_CH12_15_L_SHIFT                            0
#define MT6336_AUXADC_ADC_OUT_CH12_15_H_ADDR                             MT6336_PMIC_AUXADC_ADC12_H
#define MT6336_AUXADC_ADC_OUT_CH12_15_H_MASK                             0xF
#define MT6336_AUXADC_ADC_OUT_CH12_15_H_SHIFT                            0
#define MT6336_AUXADC_ADC_RDY_CH12_15_ADDR                               MT6336_PMIC_AUXADC_ADC12_H
#define MT6336_AUXADC_ADC_RDY_CH12_15_MASK                               0x1
#define MT6336_AUXADC_ADC_RDY_CH12_15_SHIFT                              7
#define MT6336_AUXADC_ADC_OUT_THR_HW_L_ADDR                              MT6336_PMIC_AUXADC_ADC13
#define MT6336_AUXADC_ADC_OUT_THR_HW_L_MASK                              0xFF
#define MT6336_AUXADC_ADC_OUT_THR_HW_L_SHIFT                             0
#define MT6336_AUXADC_ADC_OUT_THR_HW_H_ADDR                              MT6336_PMIC_AUXADC_ADC13_H
#define MT6336_AUXADC_ADC_OUT_THR_HW_H_MASK                              0xF
#define MT6336_AUXADC_ADC_OUT_THR_HW_H_SHIFT                             0
#define MT6336_AUXADC_ADC_RDY_THR_HW_ADDR                                MT6336_PMIC_AUXADC_ADC13_H
#define MT6336_AUXADC_ADC_RDY_THR_HW_MASK                                0x1
#define MT6336_AUXADC_ADC_RDY_THR_HW_SHIFT                               7
#define MT6336_AUXADC_ADC_OUT_VBUS_SOFT_OVP_L_ADDR                       MT6336_PMIC_AUXADC_ADC14
#define MT6336_AUXADC_ADC_OUT_VBUS_SOFT_OVP_L_MASK                       0xFF
#define MT6336_AUXADC_ADC_OUT_VBUS_SOFT_OVP_L_SHIFT                      0
#define MT6336_AUXADC_ADC_OUT_VBUS_SOFT_OVP_H_ADDR                       MT6336_PMIC_AUXADC_ADC14_H
#define MT6336_AUXADC_ADC_OUT_VBUS_SOFT_OVP_H_MASK                       0xF
#define MT6336_AUXADC_ADC_OUT_VBUS_SOFT_OVP_H_SHIFT                      0
#define MT6336_AUXADC_ADC_RDY_VBUS_SOFT_OVP_ADDR                         MT6336_PMIC_AUXADC_ADC14_H
#define MT6336_AUXADC_ADC_RDY_VBUS_SOFT_OVP_MASK                         0x1
#define MT6336_AUXADC_ADC_RDY_VBUS_SOFT_OVP_SHIFT                        7
#define MT6336_AUXADC_ADC_OUT_TYPEC_H_L_ADDR                             MT6336_PMIC_AUXADC_ADC15
#define MT6336_AUXADC_ADC_OUT_TYPEC_H_L_MASK                             0xFF
#define MT6336_AUXADC_ADC_OUT_TYPEC_H_L_SHIFT                            0
#define MT6336_AUXADC_ADC_OUT_TYPEC_H_H_ADDR                             MT6336_PMIC_AUXADC_ADC15_H
#define MT6336_AUXADC_ADC_OUT_TYPEC_H_H_MASK                             0xF
#define MT6336_AUXADC_ADC_OUT_TYPEC_H_H_SHIFT                            0
#define MT6336_AUXADC_ADC_RDY_TYPEC_H_ADDR                               MT6336_PMIC_AUXADC_ADC15_H
#define MT6336_AUXADC_ADC_RDY_TYPEC_H_MASK                               0x1
#define MT6336_AUXADC_ADC_RDY_TYPEC_H_SHIFT                              7
#define MT6336_AUXADC_ADC_OUT_TYPEC_L_L_ADDR                             MT6336_PMIC_AUXADC_ADC16
#define MT6336_AUXADC_ADC_OUT_TYPEC_L_L_MASK                             0xFF
#define MT6336_AUXADC_ADC_OUT_TYPEC_L_L_SHIFT                            0
#define MT6336_AUXADC_ADC_OUT_TYPEC_L_H_ADDR                             MT6336_PMIC_AUXADC_ADC16_H
#define MT6336_AUXADC_ADC_OUT_TYPEC_L_H_MASK                             0xF
#define MT6336_AUXADC_ADC_OUT_TYPEC_L_H_SHIFT                            0
#define MT6336_AUXADC_ADC_RDY_TYPEC_L_ADDR                               MT6336_PMIC_AUXADC_ADC16_H
#define MT6336_AUXADC_ADC_RDY_TYPEC_L_MASK                               0x1
#define MT6336_AUXADC_ADC_RDY_TYPEC_L_SHIFT                              7
#define MT6336_AUXADC_ADC_OUT_VBATSNS_DET_L_ADDR                         MT6336_PMIC_AUXADC_ADC17
#define MT6336_AUXADC_ADC_OUT_VBATSNS_DET_L_MASK                         0xFF
#define MT6336_AUXADC_ADC_OUT_VBATSNS_DET_L_SHIFT                        0
#define MT6336_AUXADC_ADC_OUT_VBATSNS_DET_H_ADDR                         MT6336_PMIC_AUXADC_ADC17_H
#define MT6336_AUXADC_ADC_OUT_VBATSNS_DET_H_MASK                         0xF
#define MT6336_AUXADC_ADC_OUT_VBATSNS_DET_H_SHIFT                        0
#define MT6336_AUXADC_ADC_RDY_VBATSNS_DET_ADDR                           MT6336_PMIC_AUXADC_ADC17_H
#define MT6336_AUXADC_ADC_RDY_VBATSNS_DET_MASK                           0x1
#define MT6336_AUXADC_ADC_RDY_VBATSNS_DET_SHIFT                          7
#define MT6336_AUXADC_ADC_OUT_BAT_TEMP_L_ADDR                            MT6336_PMIC_AUXADC_ADC18
#define MT6336_AUXADC_ADC_OUT_BAT_TEMP_L_MASK                            0xFF
#define MT6336_AUXADC_ADC_OUT_BAT_TEMP_L_SHIFT                           0
#define MT6336_AUXADC_ADC_OUT_BAT_TEMP_H_ADDR                            MT6336_PMIC_AUXADC_ADC18_H
#define MT6336_AUXADC_ADC_OUT_BAT_TEMP_H_MASK                            0xF
#define MT6336_AUXADC_ADC_OUT_BAT_TEMP_H_SHIFT                           0
#define MT6336_AUXADC_ADC_RDY_BAT_TEMP_ADDR                              MT6336_PMIC_AUXADC_ADC18_H
#define MT6336_AUXADC_ADC_RDY_BAT_TEMP_MASK                              0x1
#define MT6336_AUXADC_ADC_RDY_BAT_TEMP_SHIFT                             7
#define MT6336_AUXADC_ADC_OUT_FGADC1_L_ADDR                              MT6336_PMIC_AUXADC_ADC19
#define MT6336_AUXADC_ADC_OUT_FGADC1_L_MASK                              0xFF
#define MT6336_AUXADC_ADC_OUT_FGADC1_L_SHIFT                             0
#define MT6336_AUXADC_ADC_OUT_FGADC1_H_ADDR                              MT6336_PMIC_AUXADC_ADC19_H
#define MT6336_AUXADC_ADC_OUT_FGADC1_H_MASK                              0x7F
#define MT6336_AUXADC_ADC_OUT_FGADC1_H_SHIFT                             0
#define MT6336_AUXADC_ADC_RDY_FGADC1_ADDR                                MT6336_PMIC_AUXADC_ADC19_H
#define MT6336_AUXADC_ADC_RDY_FGADC1_MASK                                0x1
#define MT6336_AUXADC_ADC_RDY_FGADC1_SHIFT                               7
#define MT6336_AUXADC_ADC_OUT_FGADC2_L_ADDR                              MT6336_PMIC_AUXADC_ADC20
#define MT6336_AUXADC_ADC_OUT_FGADC2_L_MASK                              0xFF
#define MT6336_AUXADC_ADC_OUT_FGADC2_L_SHIFT                             0
#define MT6336_AUXADC_ADC_OUT_FGADC2_H_ADDR                              MT6336_PMIC_AUXADC_ADC20_H
#define MT6336_AUXADC_ADC_OUT_FGADC2_H_MASK                              0x7F
#define MT6336_AUXADC_ADC_OUT_FGADC2_H_SHIFT                             0
#define MT6336_AUXADC_ADC_RDY_FGADC2_ADDR                                MT6336_PMIC_AUXADC_ADC20_H
#define MT6336_AUXADC_ADC_RDY_FGADC2_MASK                                0x1
#define MT6336_AUXADC_ADC_RDY_FGADC2_SHIFT                               7
#define MT6336_AUXADC_ADC_OUT_IMP_L_ADDR                                 MT6336_PMIC_AUXADC_ADC21
#define MT6336_AUXADC_ADC_OUT_IMP_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_IMP_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_IMP_H_ADDR                                 MT6336_PMIC_AUXADC_ADC21_H
#define MT6336_AUXADC_ADC_OUT_IMP_H_MASK                                 0x7F
#define MT6336_AUXADC_ADC_OUT_IMP_H_SHIFT                                0
#define MT6336_AUXADC_ADC_RDY_IMP_ADDR                                   MT6336_PMIC_AUXADC_ADC21_H
#define MT6336_AUXADC_ADC_RDY_IMP_MASK                                   0x1
#define MT6336_AUXADC_ADC_RDY_IMP_SHIFT                                  7
#define MT6336_AUXADC_ADC_OUT_IMP_AVG_L_ADDR                             MT6336_PMIC_AUXADC_ADC22
#define MT6336_AUXADC_ADC_OUT_IMP_AVG_L_MASK                             0xFF
#define MT6336_AUXADC_ADC_OUT_IMP_AVG_L_SHIFT                            0
#define MT6336_AUXADC_ADC_OUT_IMP_AVG_H_ADDR                             MT6336_PMIC_AUXADC_ADC22_H
#define MT6336_AUXADC_ADC_OUT_IMP_AVG_H_MASK                             0x7F
#define MT6336_AUXADC_ADC_OUT_IMP_AVG_H_SHIFT                            0
#define MT6336_AUXADC_ADC_RDY_IMP_AVG_ADDR                               MT6336_PMIC_AUXADC_ADC22_H
#define MT6336_AUXADC_ADC_RDY_IMP_AVG_MASK                               0x1
#define MT6336_AUXADC_ADC_RDY_IMP_AVG_SHIFT                              7
#define MT6336_AUXADC_ADC_OUT_RAW_L_ADDR                                 MT6336_PMIC_AUXADC_ADC23
#define MT6336_AUXADC_ADC_OUT_RAW_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_OUT_RAW_L_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_RAW_H_ADDR                                 MT6336_PMIC_AUXADC_ADC23_H
#define MT6336_AUXADC_ADC_OUT_RAW_H_MASK                                 0x7F
#define MT6336_AUXADC_ADC_OUT_RAW_H_SHIFT                                0
#define MT6336_AUXADC_ADC_OUT_JEITA_L_ADDR                               MT6336_PMIC_AUXADC_ADC24
#define MT6336_AUXADC_ADC_OUT_JEITA_L_MASK                               0xFF
#define MT6336_AUXADC_ADC_OUT_JEITA_L_SHIFT                              0
#define MT6336_AUXADC_ADC_OUT_JEITA_H_ADDR                               MT6336_PMIC_AUXADC_ADC24_H
#define MT6336_AUXADC_ADC_OUT_JEITA_H_MASK                               0xF
#define MT6336_AUXADC_ADC_OUT_JEITA_H_SHIFT                              0
#define MT6336_AUXADC_ADC_RDY_JEITA_ADDR                                 MT6336_PMIC_AUXADC_ADC24_H
#define MT6336_AUXADC_ADC_RDY_JEITA_MASK                                 0x1
#define MT6336_AUXADC_ADC_RDY_JEITA_SHIFT                                7
#define MT6336_AUXADC_ADC_OUT_CHRGO1_L_ADDR                              MT6336_PMIC_AUXADC_ADC25
#define MT6336_AUXADC_ADC_OUT_CHRGO1_L_MASK                              0xFF
#define MT6336_AUXADC_ADC_OUT_CHRGO1_L_SHIFT                             0
#define MT6336_AUXADC_ADC_OUT_CHRGO1_H_ADDR                              MT6336_PMIC_AUXADC_ADC25_H
#define MT6336_AUXADC_ADC_OUT_CHRGO1_H_MASK                              0x7F
#define MT6336_AUXADC_ADC_OUT_CHRGO1_H_SHIFT                             0
#define MT6336_AUXADC_ADC_RDY_CHRGO1_ADDR                                MT6336_PMIC_AUXADC_ADC25_H
#define MT6336_AUXADC_ADC_RDY_CHRGO1_MASK                                0x1
#define MT6336_AUXADC_ADC_RDY_CHRGO1_SHIFT                               7
#define MT6336_AUXADC_ADC_OUT_CHRGO2_L_ADDR                              MT6336_PMIC_AUXADC_ADC26
#define MT6336_AUXADC_ADC_OUT_CHRGO2_L_MASK                              0xFF
#define MT6336_AUXADC_ADC_OUT_CHRGO2_L_SHIFT                             0
#define MT6336_AUXADC_ADC_OUT_CHRGO2_H_ADDR                              MT6336_PMIC_AUXADC_ADC26_H
#define MT6336_AUXADC_ADC_OUT_CHRGO2_H_MASK                              0x7F
#define MT6336_AUXADC_ADC_OUT_CHRGO2_H_SHIFT                             0
#define MT6336_AUXADC_ADC_RDY_CHRGO2_ADDR                                MT6336_PMIC_AUXADC_ADC26_H
#define MT6336_AUXADC_ADC_RDY_CHRGO2_MASK                                0x1
#define MT6336_AUXADC_ADC_RDY_CHRGO2_SHIFT                               7
#define MT6336_AUXADC_ADC_OUT_WAKEUP1_L_ADDR                             MT6336_PMIC_AUXADC_ADC27
#define MT6336_AUXADC_ADC_OUT_WAKEUP1_L_MASK                             0xFF
#define MT6336_AUXADC_ADC_OUT_WAKEUP1_L_SHIFT                            0
#define MT6336_AUXADC_ADC_OUT_WAKEUP1_H_ADDR                             MT6336_PMIC_AUXADC_ADC27_H
#define MT6336_AUXADC_ADC_OUT_WAKEUP1_H_MASK                             0x7F
#define MT6336_AUXADC_ADC_OUT_WAKEUP1_H_SHIFT                            0
#define MT6336_AUXADC_ADC_RDY_WAKEUP1_ADDR                               MT6336_PMIC_AUXADC_ADC27_H
#define MT6336_AUXADC_ADC_RDY_WAKEUP1_MASK                               0x1
#define MT6336_AUXADC_ADC_RDY_WAKEUP1_SHIFT                              7
#define MT6336_AUXADC_ADC_OUT_WAKEUP2_L_ADDR                             MT6336_PMIC_AUXADC_ADC28
#define MT6336_AUXADC_ADC_OUT_WAKEUP2_L_MASK                             0xFF
#define MT6336_AUXADC_ADC_OUT_WAKEUP2_L_SHIFT                            0
#define MT6336_AUXADC_ADC_OUT_WAKEUP2_H_ADDR                             MT6336_PMIC_AUXADC_ADC28_H
#define MT6336_AUXADC_ADC_OUT_WAKEUP2_H_MASK                             0x7F
#define MT6336_AUXADC_ADC_OUT_WAKEUP2_H_SHIFT                            0
#define MT6336_AUXADC_ADC_RDY_WAKEUP2_ADDR                               MT6336_PMIC_AUXADC_ADC28_H
#define MT6336_AUXADC_ADC_RDY_WAKEUP2_MASK                               0x1
#define MT6336_AUXADC_ADC_RDY_WAKEUP2_SHIFT                              7
#define MT6336_AUXADC_ADC_BUSY_IN_L_ADDR                                 MT6336_PMIC_AUXADC_STA0
#define MT6336_AUXADC_ADC_BUSY_IN_L_MASK                                 0xFF
#define MT6336_AUXADC_ADC_BUSY_IN_L_SHIFT                                0
#define MT6336_AUXADC_ADC_BUSY_IN_H_ADDR                                 MT6336_PMIC_AUXADC_STA0_H
#define MT6336_AUXADC_ADC_BUSY_IN_H_MASK                                 0xF
#define MT6336_AUXADC_ADC_BUSY_IN_H_SHIFT                                0
#define MT6336_AUXADC_ADC_BUSY_IN_VBUS_SOFT_OVP_ADDR                     MT6336_PMIC_AUXADC_STA0_H
#define MT6336_AUXADC_ADC_BUSY_IN_VBUS_SOFT_OVP_MASK                     0x1
#define MT6336_AUXADC_ADC_BUSY_IN_VBUS_SOFT_OVP_SHIFT                    4
#define MT6336_AUXADC_ADC_BUSY_IN_BAT_TEMP_ADDR                          MT6336_PMIC_AUXADC_STA0_H
#define MT6336_AUXADC_ADC_BUSY_IN_BAT_TEMP_MASK                          0x1
#define MT6336_AUXADC_ADC_BUSY_IN_BAT_TEMP_SHIFT                         6
#define MT6336_AUXADC_ADC_BUSY_IN_WAKEUP_ADDR                            MT6336_PMIC_AUXADC_STA0_H
#define MT6336_AUXADC_ADC_BUSY_IN_WAKEUP_MASK                            0x1
#define MT6336_AUXADC_ADC_BUSY_IN_WAKEUP_SHIFT                           7
#define MT6336_AUXADC_ADC_BUSY_IN_JEITA_ADDR                             MT6336_PMIC_AUXADC_STA1
#define MT6336_AUXADC_ADC_BUSY_IN_JEITA_MASK                             0x1
#define MT6336_AUXADC_ADC_BUSY_IN_JEITA_SHIFT                            4
#define MT6336_AUXADC_ADC_BUSY_IN_CHRGO1_ADDR                            MT6336_PMIC_AUXADC_STA1
#define MT6336_AUXADC_ADC_BUSY_IN_CHRGO1_MASK                            0x1
#define MT6336_AUXADC_ADC_BUSY_IN_CHRGO1_SHIFT                           5
#define MT6336_AUXADC_ADC_BUSY_IN_CHRGO2_ADDR                            MT6336_PMIC_AUXADC_STA1
#define MT6336_AUXADC_ADC_BUSY_IN_CHRGO2_MASK                            0x1
#define MT6336_AUXADC_ADC_BUSY_IN_CHRGO2_SHIFT                           6
#define MT6336_AUXADC_ADC_BUSY_IN_SHARE_ADDR                             MT6336_PMIC_AUXADC_STA1
#define MT6336_AUXADC_ADC_BUSY_IN_SHARE_MASK                             0x1
#define MT6336_AUXADC_ADC_BUSY_IN_SHARE_SHIFT                            7
#define MT6336_AUXADC_ADC_BUSY_IN_IMP_ADDR                               MT6336_PMIC_AUXADC_STA1_H
#define MT6336_AUXADC_ADC_BUSY_IN_IMP_MASK                               0x1
#define MT6336_AUXADC_ADC_BUSY_IN_IMP_SHIFT                              0
#define MT6336_AUXADC_ADC_BUSY_IN_FGADC1_ADDR                            MT6336_PMIC_AUXADC_STA1_H
#define MT6336_AUXADC_ADC_BUSY_IN_FGADC1_MASK                            0x1
#define MT6336_AUXADC_ADC_BUSY_IN_FGADC1_SHIFT                           1
#define MT6336_AUXADC_ADC_BUSY_IN_FGADC2_ADDR                            MT6336_PMIC_AUXADC_STA1_H
#define MT6336_AUXADC_ADC_BUSY_IN_FGADC2_MASK                            0x1
#define MT6336_AUXADC_ADC_BUSY_IN_FGADC2_SHIFT                           2
#define MT6336_AUXADC_ADC_BUSY_IN_GPS_AP_ADDR                            MT6336_PMIC_AUXADC_STA1_H
#define MT6336_AUXADC_ADC_BUSY_IN_GPS_AP_MASK                            0x1
#define MT6336_AUXADC_ADC_BUSY_IN_GPS_AP_SHIFT                           3
#define MT6336_AUXADC_ADC_BUSY_IN_GPS_MD_ADDR                            MT6336_PMIC_AUXADC_STA1_H
#define MT6336_AUXADC_ADC_BUSY_IN_GPS_MD_MASK                            0x1
#define MT6336_AUXADC_ADC_BUSY_IN_GPS_MD_SHIFT                           4
#define MT6336_AUXADC_ADC_BUSY_IN_GPS_ADDR                               MT6336_PMIC_AUXADC_STA1_H
#define MT6336_AUXADC_ADC_BUSY_IN_GPS_MASK                               0x1
#define MT6336_AUXADC_ADC_BUSY_IN_GPS_SHIFT                              5
#define MT6336_AUXADC_ADC_BUSY_IN_THR_HW_ADDR                            MT6336_PMIC_AUXADC_STA1_H
#define MT6336_AUXADC_ADC_BUSY_IN_THR_HW_MASK                            0x1
#define MT6336_AUXADC_ADC_BUSY_IN_THR_HW_SHIFT                           6
#define MT6336_AUXADC_ADC_BUSY_IN_THR_MD_ADDR                            MT6336_PMIC_AUXADC_STA1_H
#define MT6336_AUXADC_ADC_BUSY_IN_THR_MD_MASK                            0x1
#define MT6336_AUXADC_ADC_BUSY_IN_THR_MD_SHIFT                           7
#define MT6336_AUXADC_ADC_BUSY_IN_NAG_ADDR                               MT6336_PMIC_AUXADC_STA2_H
#define MT6336_AUXADC_ADC_BUSY_IN_NAG_MASK                               0x1
#define MT6336_AUXADC_ADC_BUSY_IN_NAG_SHIFT                              7
#define MT6336_AUXADC_RQST_CH0_ADDR                                      MT6336_PMIC_AUXADC_RQST0
#define MT6336_AUXADC_RQST_CH0_MASK                                      0x1
#define MT6336_AUXADC_RQST_CH0_SHIFT                                     0
#define MT6336_AUXADC_RQST_CH1_ADDR                                      MT6336_PMIC_AUXADC_RQST0
#define MT6336_AUXADC_RQST_CH1_MASK                                      0x1
#define MT6336_AUXADC_RQST_CH1_SHIFT                                     1
#define MT6336_AUXADC_RQST_CH2_ADDR                                      MT6336_PMIC_AUXADC_RQST0
#define MT6336_AUXADC_RQST_CH2_MASK                                      0x1
#define MT6336_AUXADC_RQST_CH2_SHIFT                                     2
#define MT6336_AUXADC_RQST_CH3_ADDR                                      MT6336_PMIC_AUXADC_RQST0
#define MT6336_AUXADC_RQST_CH3_MASK                                      0x1
#define MT6336_AUXADC_RQST_CH3_SHIFT                                     3
#define MT6336_AUXADC_RQST_CH4_ADDR                                      MT6336_PMIC_AUXADC_RQST0
#define MT6336_AUXADC_RQST_CH4_MASK                                      0x1
#define MT6336_AUXADC_RQST_CH4_SHIFT                                     4
#define MT6336_AUXADC_RQST_CH5_ADDR                                      MT6336_PMIC_AUXADC_RQST0
#define MT6336_AUXADC_RQST_CH5_MASK                                      0x1
#define MT6336_AUXADC_RQST_CH5_SHIFT                                     5
#define MT6336_AUXADC_RQST_CH6_ADDR                                      MT6336_PMIC_AUXADC_RQST0
#define MT6336_AUXADC_RQST_CH6_MASK                                      0x1
#define MT6336_AUXADC_RQST_CH6_SHIFT                                     6
#define MT6336_AUXADC_RQST_CH7_ADDR                                      MT6336_PMIC_AUXADC_RQST0
#define MT6336_AUXADC_RQST_CH7_MASK                                      0x1
#define MT6336_AUXADC_RQST_CH7_SHIFT                                     7
#define MT6336_AUXADC_RQST0_SET_ADDR                                     MT6336_PMIC_AUXADC_RQST0_SET
#define MT6336_AUXADC_RQST0_SET_MASK                                     0xFF
#define MT6336_AUXADC_RQST0_SET_SHIFT                                    0
#define MT6336_AUXADC_RQST0_CLR_ADDR                                     MT6336_PMIC_AUXADC_RQST0_CLR
#define MT6336_AUXADC_RQST0_CLR_MASK                                     0xFF
#define MT6336_AUXADC_RQST0_CLR_SHIFT                                    0
#define MT6336_AUXADC_RQST_CH8_ADDR                                      MT6336_PMIC_AUXADC_RQST0_H
#define MT6336_AUXADC_RQST_CH8_MASK                                      0x1
#define MT6336_AUXADC_RQST_CH8_SHIFT                                     0
#define MT6336_AUXADC_RQST_CH9_ADDR                                      MT6336_PMIC_AUXADC_RQST0_H
#define MT6336_AUXADC_RQST_CH9_MASK                                      0x1
#define MT6336_AUXADC_RQST_CH9_SHIFT                                     1
#define MT6336_AUXADC_RQST_CH10_ADDR                                     MT6336_PMIC_AUXADC_RQST0_H
#define MT6336_AUXADC_RQST_CH10_MASK                                     0x1
#define MT6336_AUXADC_RQST_CH10_SHIFT                                    2
#define MT6336_AUXADC_RQST_CH11_ADDR                                     MT6336_PMIC_AUXADC_RQST0_H
#define MT6336_AUXADC_RQST_CH11_MASK                                     0x1
#define MT6336_AUXADC_RQST_CH11_SHIFT                                    3
#define MT6336_AUXADC_RQST_CH12_ADDR                                     MT6336_PMIC_AUXADC_RQST0_H
#define MT6336_AUXADC_RQST_CH12_MASK                                     0x1
#define MT6336_AUXADC_RQST_CH12_SHIFT                                    4
#define MT6336_AUXADC_RQST_CH13_ADDR                                     MT6336_PMIC_AUXADC_RQST0_H
#define MT6336_AUXADC_RQST_CH13_MASK                                     0x1
#define MT6336_AUXADC_RQST_CH13_SHIFT                                    5
#define MT6336_AUXADC_RQST_CH14_ADDR                                     MT6336_PMIC_AUXADC_RQST0_H
#define MT6336_AUXADC_RQST_CH14_MASK                                     0x1
#define MT6336_AUXADC_RQST_CH14_SHIFT                                    6
#define MT6336_AUXADC_RQST_CH15_ADDR                                     MT6336_PMIC_AUXADC_RQST0_H
#define MT6336_AUXADC_RQST_CH15_MASK                                     0x1
#define MT6336_AUXADC_RQST_CH15_SHIFT                                    7
#define MT6336_AUXADC_RQST0_H_SET_ADDR                                   MT6336_PMIC_AUXADC_RQST0_H_SET
#define MT6336_AUXADC_RQST0_H_SET_MASK                                   0xFF
#define MT6336_AUXADC_RQST0_H_SET_SHIFT                                  0
#define MT6336_AUXADC_RQST0_H_CLR_ADDR                                   MT6336_PMIC_AUXADC_RQST0_H_CLR
#define MT6336_AUXADC_RQST0_H_CLR_MASK                                   0xFF
#define MT6336_AUXADC_RQST0_H_CLR_SHIFT                                  0
#define MT6336_AUXADC_CK_ON_EXTD_ADDR                                    MT6336_PMIC_AUXADC_CON0
#define MT6336_AUXADC_CK_ON_EXTD_MASK                                    0x3F
#define MT6336_AUXADC_CK_ON_EXTD_SHIFT                                   0
#define MT6336_AUXADC_SRCLKEN_SRC_SEL_ADDR                               MT6336_PMIC_AUXADC_CON0
#define MT6336_AUXADC_SRCLKEN_SRC_SEL_MASK                               0x3
#define MT6336_AUXADC_SRCLKEN_SRC_SEL_SHIFT                              6
#define MT6336_AUXADC_CON0_SET_ADDR                                      MT6336_PMIC_AUXADC_CON0_SET
#define MT6336_AUXADC_CON0_SET_MASK                                      0xFF
#define MT6336_AUXADC_CON0_SET_SHIFT                                     0
#define MT6336_AUXADC_CON0_CLR_ADDR                                      MT6336_PMIC_AUXADC_CON0_CLR
#define MT6336_AUXADC_CON0_CLR_MASK                                      0xFF
#define MT6336_AUXADC_CON0_CLR_SHIFT                                     0
#define MT6336_AUXADC_ADC_PWDB_ADDR                                      MT6336_PMIC_AUXADC_CON0_H
#define MT6336_AUXADC_ADC_PWDB_MASK                                      0x1
#define MT6336_AUXADC_ADC_PWDB_SHIFT                                     0
#define MT6336_AUXADC_ADC_PWDB_SWCTRL_ADDR                               MT6336_PMIC_AUXADC_CON0_H
#define MT6336_AUXADC_ADC_PWDB_SWCTRL_MASK                               0x1
#define MT6336_AUXADC_ADC_PWDB_SWCTRL_SHIFT                              1
#define MT6336_AUXADC_STRUP_CK_ON_ENB_ADDR                               MT6336_PMIC_AUXADC_CON0_H
#define MT6336_AUXADC_STRUP_CK_ON_ENB_MASK                               0x1
#define MT6336_AUXADC_STRUP_CK_ON_ENB_SHIFT                              2
#define MT6336_AUXADC_SRCLKEN_CK_EN_ADDR                                 MT6336_PMIC_AUXADC_CON0_H
#define MT6336_AUXADC_SRCLKEN_CK_EN_MASK                                 0x1
#define MT6336_AUXADC_SRCLKEN_CK_EN_SHIFT                                4
#define MT6336_AUXADC_CK_AON_GPS_ADDR                                    MT6336_PMIC_AUXADC_CON0_H
#define MT6336_AUXADC_CK_AON_GPS_MASK                                    0x1
#define MT6336_AUXADC_CK_AON_GPS_SHIFT                                   5
#define MT6336_AUXADC_CK_AON_MD_ADDR                                     MT6336_PMIC_AUXADC_CON0_H
#define MT6336_AUXADC_CK_AON_MD_MASK                                     0x1
#define MT6336_AUXADC_CK_AON_MD_SHIFT                                    6
#define MT6336_AUXADC_CK_AON_ADDR                                        MT6336_PMIC_AUXADC_CON0_H
#define MT6336_AUXADC_CK_AON_MASK                                        0x1
#define MT6336_AUXADC_CK_AON_SHIFT                                       7
#define MT6336_AUXADC_CON0_H_SET_ADDR                                    MT6336_PMIC_AUXADC_CON0_H_SET
#define MT6336_AUXADC_CON0_H_SET_MASK                                    0xFF
#define MT6336_AUXADC_CON0_H_SET_SHIFT                                   0
#define MT6336_AUXADC_CON0_H_CLR_ADDR                                    MT6336_PMIC_AUXADC_CON0_H_CLR
#define MT6336_AUXADC_CON0_H_CLR_MASK                                    0xFF
#define MT6336_AUXADC_CON0_H_CLR_SHIFT                                   0
#define MT6336_AUXADC_AVG_NUM_SMALL_ADDR                                 MT6336_PMIC_AUXADC_CON1
#define MT6336_AUXADC_AVG_NUM_SMALL_MASK                                 0x7
#define MT6336_AUXADC_AVG_NUM_SMALL_SHIFT                                0
#define MT6336_AUXADC_AVG_NUM_LARGE_ADDR                                 MT6336_PMIC_AUXADC_CON1
#define MT6336_AUXADC_AVG_NUM_LARGE_MASK                                 0x7
#define MT6336_AUXADC_AVG_NUM_LARGE_SHIFT                                3
#define MT6336_AUXADC_SPL_NUM_L_ADDR                                     MT6336_PMIC_AUXADC_CON1
#define MT6336_AUXADC_SPL_NUM_L_MASK                                     0x3
#define MT6336_AUXADC_SPL_NUM_L_SHIFT                                    6
#define MT6336_AUXADC_SPL_NUM_H_ADDR                                     MT6336_PMIC_AUXADC_CON1_H
#define MT6336_AUXADC_SPL_NUM_H_MASK                                     0xFF
#define MT6336_AUXADC_SPL_NUM_H_SHIFT                                    0
#define MT6336_AUXADC_AVG_NUM_SEL_L_ADDR                                 MT6336_PMIC_AUXADC_CON2
#define MT6336_AUXADC_AVG_NUM_SEL_L_MASK                                 0xFF
#define MT6336_AUXADC_AVG_NUM_SEL_L_SHIFT                                0
#define MT6336_AUXADC_AVG_NUM_SEL_H_ADDR                                 MT6336_PMIC_AUXADC_CON2_H
#define MT6336_AUXADC_AVG_NUM_SEL_H_MASK                                 0xF
#define MT6336_AUXADC_AVG_NUM_SEL_H_SHIFT                                0
#define MT6336_AUXADC_AVG_NUM_SEL_SHARE_ADDR                             MT6336_PMIC_AUXADC_CON2_H
#define MT6336_AUXADC_AVG_NUM_SEL_SHARE_MASK                             0x1
#define MT6336_AUXADC_AVG_NUM_SEL_SHARE_SHIFT                            4
#define MT6336_AUXADC_AVG_NUM_SEL_VBUS_SOFT_OVP_ADDR                     MT6336_PMIC_AUXADC_CON2_H
#define MT6336_AUXADC_AVG_NUM_SEL_VBUS_SOFT_OVP_MASK                     0x1
#define MT6336_AUXADC_AVG_NUM_SEL_VBUS_SOFT_OVP_SHIFT                    5
#define MT6336_AUXADC_AVG_NUM_SEL_BAT_TEMP_ADDR                          MT6336_PMIC_AUXADC_CON2_H
#define MT6336_AUXADC_AVG_NUM_SEL_BAT_TEMP_MASK                          0x1
#define MT6336_AUXADC_AVG_NUM_SEL_BAT_TEMP_SHIFT                         6
#define MT6336_AUXADC_AVG_NUM_SEL_WAKEUP_ADDR                            MT6336_PMIC_AUXADC_CON2_H
#define MT6336_AUXADC_AVG_NUM_SEL_WAKEUP_MASK                            0x1
#define MT6336_AUXADC_AVG_NUM_SEL_WAKEUP_SHIFT                           7
#define MT6336_AUXADC_SPL_NUM_LARGE_L_ADDR                               MT6336_PMIC_AUXADC_CON3
#define MT6336_AUXADC_SPL_NUM_LARGE_L_MASK                               0xFF
#define MT6336_AUXADC_SPL_NUM_LARGE_L_SHIFT                              0
#define MT6336_AUXADC_SPL_NUM_LARGE_H_ADDR                               MT6336_PMIC_AUXADC_CON3_H
#define MT6336_AUXADC_SPL_NUM_LARGE_H_MASK                               0x3
#define MT6336_AUXADC_SPL_NUM_LARGE_H_SHIFT                              0
#define MT6336_AUXADC_SPL_NUM_SLEEP_L_ADDR                               MT6336_PMIC_AUXADC_CON4
#define MT6336_AUXADC_SPL_NUM_SLEEP_L_MASK                               0xFF
#define MT6336_AUXADC_SPL_NUM_SLEEP_L_SHIFT                              0
#define MT6336_AUXADC_SPL_NUM_SLEEP_H_ADDR                               MT6336_PMIC_AUXADC_CON4_H
#define MT6336_AUXADC_SPL_NUM_SLEEP_H_MASK                               0x3
#define MT6336_AUXADC_SPL_NUM_SLEEP_H_SHIFT                              0
#define MT6336_AUXADC_SPL_NUM_SLEEP_SEL_ADDR                             MT6336_PMIC_AUXADC_CON4_H
#define MT6336_AUXADC_SPL_NUM_SLEEP_SEL_MASK                             0x1
#define MT6336_AUXADC_SPL_NUM_SLEEP_SEL_SHIFT                            7
#define MT6336_AUXADC_SPL_NUM_SEL_L_ADDR                                 MT6336_PMIC_AUXADC_CON5
#define MT6336_AUXADC_SPL_NUM_SEL_L_MASK                                 0xFF
#define MT6336_AUXADC_SPL_NUM_SEL_L_SHIFT                                0
#define MT6336_AUXADC_SPL_NUM_SEL_H_ADDR                                 MT6336_PMIC_AUXADC_CON5_H
#define MT6336_AUXADC_SPL_NUM_SEL_H_MASK                                 0xF
#define MT6336_AUXADC_SPL_NUM_SEL_H_SHIFT                                0
#define MT6336_AUXADC_SPL_NUM_SEL_SHARE_ADDR                             MT6336_PMIC_AUXADC_CON5_H
#define MT6336_AUXADC_SPL_NUM_SEL_SHARE_MASK                             0x1
#define MT6336_AUXADC_SPL_NUM_SEL_SHARE_SHIFT                            4
#define MT6336_AUXADC_SPL_NUM_SEL_VBUS_SOFT_OVP_ADDR                     MT6336_PMIC_AUXADC_CON5_H
#define MT6336_AUXADC_SPL_NUM_SEL_VBUS_SOFT_OVP_MASK                     0x1
#define MT6336_AUXADC_SPL_NUM_SEL_VBUS_SOFT_OVP_SHIFT                    5
#define MT6336_AUXADC_SPL_NUM_SEL_BAT_TEMP_ADDR                          MT6336_PMIC_AUXADC_CON5_H
#define MT6336_AUXADC_SPL_NUM_SEL_BAT_TEMP_MASK                          0x1
#define MT6336_AUXADC_SPL_NUM_SEL_BAT_TEMP_SHIFT                         6
#define MT6336_AUXADC_SPL_NUM_SEL_WAKEUP_ADDR                            MT6336_PMIC_AUXADC_CON5_H
#define MT6336_AUXADC_SPL_NUM_SEL_WAKEUP_MASK                            0x1
#define MT6336_AUXADC_SPL_NUM_SEL_WAKEUP_SHIFT                           7
#define MT6336_AUXADC_TRIM_CH0_SEL_ADDR                                  MT6336_PMIC_AUXADC_CON6
#define MT6336_AUXADC_TRIM_CH0_SEL_MASK                                  0x3
#define MT6336_AUXADC_TRIM_CH0_SEL_SHIFT                                 0
#define MT6336_AUXADC_TRIM_CH1_SEL_ADDR                                  MT6336_PMIC_AUXADC_CON6
#define MT6336_AUXADC_TRIM_CH1_SEL_MASK                                  0x3
#define MT6336_AUXADC_TRIM_CH1_SEL_SHIFT                                 2
#define MT6336_AUXADC_TRIM_CH2_SEL_ADDR                                  MT6336_PMIC_AUXADC_CON6
#define MT6336_AUXADC_TRIM_CH2_SEL_MASK                                  0x3
#define MT6336_AUXADC_TRIM_CH2_SEL_SHIFT                                 4
#define MT6336_AUXADC_TRIM_CH3_SEL_ADDR                                  MT6336_PMIC_AUXADC_CON6
#define MT6336_AUXADC_TRIM_CH3_SEL_MASK                                  0x3
#define MT6336_AUXADC_TRIM_CH3_SEL_SHIFT                                 6
#define MT6336_AUXADC_TRIM_CH4_SEL_ADDR                                  MT6336_PMIC_AUXADC_CON6_H
#define MT6336_AUXADC_TRIM_CH4_SEL_MASK                                  0x3
#define MT6336_AUXADC_TRIM_CH4_SEL_SHIFT                                 0
#define MT6336_AUXADC_TRIM_CH5_SEL_ADDR                                  MT6336_PMIC_AUXADC_CON6_H
#define MT6336_AUXADC_TRIM_CH5_SEL_MASK                                  0x3
#define MT6336_AUXADC_TRIM_CH5_SEL_SHIFT                                 2
#define MT6336_AUXADC_TRIM_CH6_SEL_ADDR                                  MT6336_PMIC_AUXADC_CON6_H
#define MT6336_AUXADC_TRIM_CH6_SEL_MASK                                  0x3
#define MT6336_AUXADC_TRIM_CH6_SEL_SHIFT                                 4
#define MT6336_AUXADC_TRIM_CH7_SEL_ADDR                                  MT6336_PMIC_AUXADC_CON6_H
#define MT6336_AUXADC_TRIM_CH7_SEL_MASK                                  0x3
#define MT6336_AUXADC_TRIM_CH7_SEL_SHIFT                                 6
#define MT6336_AUXADC_TRIM_CH8_SEL_ADDR                                  MT6336_PMIC_AUXADC_CON7
#define MT6336_AUXADC_TRIM_CH8_SEL_MASK                                  0x3
#define MT6336_AUXADC_TRIM_CH8_SEL_SHIFT                                 0
#define MT6336_AUXADC_TRIM_CH9_SEL_ADDR                                  MT6336_PMIC_AUXADC_CON7
#define MT6336_AUXADC_TRIM_CH9_SEL_MASK                                  0x3
#define MT6336_AUXADC_TRIM_CH9_SEL_SHIFT                                 2
#define MT6336_AUXADC_TRIM_CH10_SEL_ADDR                                 MT6336_PMIC_AUXADC_CON7
#define MT6336_AUXADC_TRIM_CH10_SEL_MASK                                 0x3
#define MT6336_AUXADC_TRIM_CH10_SEL_SHIFT                                4
#define MT6336_AUXADC_TRIM_CH11_SEL_ADDR                                 MT6336_PMIC_AUXADC_CON7
#define MT6336_AUXADC_TRIM_CH11_SEL_MASK                                 0x3
#define MT6336_AUXADC_TRIM_CH11_SEL_SHIFT                                6
#define MT6336_AUXADC_ADC_2S_COMP_ENB_ADDR                               MT6336_PMIC_AUXADC_CON7_H
#define MT6336_AUXADC_ADC_2S_COMP_ENB_MASK                               0x1
#define MT6336_AUXADC_ADC_2S_COMP_ENB_SHIFT                              6
#define MT6336_AUXADC_ADC_TRIM_COMP_ADDR                                 MT6336_PMIC_AUXADC_CON7_H
#define MT6336_AUXADC_ADC_TRIM_COMP_MASK                                 0x1
#define MT6336_AUXADC_ADC_TRIM_COMP_SHIFT                                7
#define MT6336_AUXADC_SW_GAIN_TRIM_L_ADDR                                MT6336_PMIC_AUXADC_CON8
#define MT6336_AUXADC_SW_GAIN_TRIM_L_MASK                                0xFF
#define MT6336_AUXADC_SW_GAIN_TRIM_L_SHIFT                               0
#define MT6336_AUXADC_SW_GAIN_TRIM_H_ADDR                                MT6336_PMIC_AUXADC_CON8_H
#define MT6336_AUXADC_SW_GAIN_TRIM_H_MASK                                0x7F
#define MT6336_AUXADC_SW_GAIN_TRIM_H_SHIFT                               0
#define MT6336_AUXADC_SW_OFFSET_TRIM_L_ADDR                              MT6336_PMIC_AUXADC_CON9
#define MT6336_AUXADC_SW_OFFSET_TRIM_L_MASK                              0xFF
#define MT6336_AUXADC_SW_OFFSET_TRIM_L_SHIFT                             0
#define MT6336_AUXADC_SW_OFFSET_TRIM_H_ADDR                              MT6336_PMIC_AUXADC_CON9_H
#define MT6336_AUXADC_SW_OFFSET_TRIM_H_MASK                              0x7F
#define MT6336_AUXADC_SW_OFFSET_TRIM_H_SHIFT                             0
#define MT6336_AUXADC_RNG_EN_ADDR                                        MT6336_PMIC_AUXADC_CON10
#define MT6336_AUXADC_RNG_EN_MASK                                        0x1
#define MT6336_AUXADC_RNG_EN_SHIFT                                       0
#define MT6336_AUXADC_DATA_REUSE_SEL_ADDR                                MT6336_PMIC_AUXADC_CON10
#define MT6336_AUXADC_DATA_REUSE_SEL_MASK                                0x3
#define MT6336_AUXADC_DATA_REUSE_SEL_SHIFT                               1
#define MT6336_AUXADC_TEST_MODE_ADDR                                     MT6336_PMIC_AUXADC_CON10
#define MT6336_AUXADC_TEST_MODE_MASK                                     0x1
#define MT6336_AUXADC_TEST_MODE_SHIFT                                    3
#define MT6336_AUXADC_BIT_SEL_ADDR                                       MT6336_PMIC_AUXADC_CON10
#define MT6336_AUXADC_BIT_SEL_MASK                                       0x1
#define MT6336_AUXADC_BIT_SEL_SHIFT                                      4
#define MT6336_AUXADC_START_SW_ADDR                                      MT6336_PMIC_AUXADC_CON10
#define MT6336_AUXADC_START_SW_MASK                                      0x1
#define MT6336_AUXADC_START_SW_SHIFT                                     5
#define MT6336_AUXADC_START_SWCTRL_ADDR                                  MT6336_PMIC_AUXADC_CON10
#define MT6336_AUXADC_START_SWCTRL_MASK                                  0x1
#define MT6336_AUXADC_START_SWCTRL_SHIFT                                 6
#define MT6336_AUXADC_TS_VBE_SEL_ADDR                                    MT6336_PMIC_AUXADC_CON10
#define MT6336_AUXADC_TS_VBE_SEL_MASK                                    0x1
#define MT6336_AUXADC_TS_VBE_SEL_SHIFT                                   7
#define MT6336_AUXADC_TS_VBE_SEL_SWCTRL_ADDR                             MT6336_PMIC_AUXADC_CON10_H
#define MT6336_AUXADC_TS_VBE_SEL_SWCTRL_MASK                             0x1
#define MT6336_AUXADC_TS_VBE_SEL_SWCTRL_SHIFT                            0
#define MT6336_AUXADC_VBUF_EN_ADDR                                       MT6336_PMIC_AUXADC_CON10_H
#define MT6336_AUXADC_VBUF_EN_MASK                                       0x1
#define MT6336_AUXADC_VBUF_EN_SHIFT                                      1
#define MT6336_AUXADC_VBUF_EN_SWCTRL_ADDR                                MT6336_PMIC_AUXADC_CON10_H
#define MT6336_AUXADC_VBUF_EN_SWCTRL_MASK                                0x1
#define MT6336_AUXADC_VBUF_EN_SWCTRL_SHIFT                               2
#define MT6336_AUXADC_OUT_SEL_ADDR                                       MT6336_PMIC_AUXADC_CON10_H
#define MT6336_AUXADC_OUT_SEL_MASK                                       0x1
#define MT6336_AUXADC_OUT_SEL_SHIFT                                      3
#define MT6336_AUXADC_DA_DAC_L_ADDR                                      MT6336_PMIC_AUXADC_CON11
#define MT6336_AUXADC_DA_DAC_L_MASK                                      0xFF
#define MT6336_AUXADC_DA_DAC_L_SHIFT                                     0
#define MT6336_AUXADC_DA_DAC_H_ADDR                                      MT6336_PMIC_AUXADC_CON11_H
#define MT6336_AUXADC_DA_DAC_H_MASK                                      0xF
#define MT6336_AUXADC_DA_DAC_H_SHIFT                                     0
#define MT6336_AUXADC_DA_DAC_SWCTRL_ADDR                                 MT6336_PMIC_AUXADC_CON11_H
#define MT6336_AUXADC_DA_DAC_SWCTRL_MASK                                 0x1
#define MT6336_AUXADC_DA_DAC_SWCTRL_SHIFT                                4
#define MT6336_AD_AUXADC_COMP_ADDR                                       MT6336_PMIC_AUXADC_CON11_H
#define MT6336_AD_AUXADC_COMP_MASK                                       0x1
#define MT6336_AD_AUXADC_COMP_SHIFT                                      7
#define MT6336_RG_VBUF_EXTEN_ADDR                                        MT6336_PMIC_AUXADC_CON12
#define MT6336_RG_VBUF_EXTEN_MASK                                        0x1
#define MT6336_RG_VBUF_EXTEN_SHIFT                                       4
#define MT6336_RG_VBUF_CALEN_ADDR                                        MT6336_PMIC_AUXADC_CON12
#define MT6336_RG_VBUF_CALEN_MASK                                        0x1
#define MT6336_RG_VBUF_CALEN_SHIFT                                       5
#define MT6336_RG_VBUF_BYP_ADDR                                          MT6336_PMIC_AUXADC_CON12
#define MT6336_RG_VBUF_BYP_MASK                                          0x1
#define MT6336_RG_VBUF_BYP_SHIFT                                         6
#define MT6336_RG_VBUF_EN_ADDR                                           MT6336_PMIC_AUXADC_CON12
#define MT6336_RG_VBUF_EN_MASK                                           0x1
#define MT6336_RG_VBUF_EN_SHIFT                                          7
#define MT6336_RG_AUX_RSV_ADDR                                           MT6336_PMIC_AUXADC_CON12_H
#define MT6336_RG_AUX_RSV_MASK                                           0xF
#define MT6336_RG_AUX_RSV_SHIFT                                          0
#define MT6336_RG_AUXADC_CALI_ADDR                                       MT6336_PMIC_AUXADC_CON12_H
#define MT6336_RG_AUXADC_CALI_MASK                                       0xF
#define MT6336_RG_AUXADC_CALI_SHIFT                                      4
#define MT6336_AUXADC_ADCIN_VBATSNS_EN_ADDR                              MT6336_PMIC_AUXADC_CON13
#define MT6336_AUXADC_ADCIN_VBATSNS_EN_MASK                              0x1
#define MT6336_AUXADC_ADCIN_VBATSNS_EN_SHIFT                             0
#define MT6336_AUXADC_ADCIN_VBUS_EN_ADDR                                 MT6336_PMIC_AUXADC_CON13
#define MT6336_AUXADC_ADCIN_VBUS_EN_MASK                                 0x1
#define MT6336_AUXADC_ADCIN_VBUS_EN_SHIFT                                1
#define MT6336_AUXADC_ADCIN_VBATON_EN_ADDR                               MT6336_PMIC_AUXADC_CON13
#define MT6336_AUXADC_ADCIN_VBATON_EN_MASK                               0x1
#define MT6336_AUXADC_ADCIN_VBATON_EN_SHIFT                              2
#define MT6336_AUXADC_ADCIN_VLED1_EN_ADDR                                MT6336_PMIC_AUXADC_CON13
#define MT6336_AUXADC_ADCIN_VLED1_EN_MASK                                0x1
#define MT6336_AUXADC_ADCIN_VLED1_EN_SHIFT                               3
#define MT6336_AUXADC_ADCIN_VLED2_EN_ADDR                                MT6336_PMIC_AUXADC_CON13
#define MT6336_AUXADC_ADCIN_VLED2_EN_MASK                                0x1
#define MT6336_AUXADC_ADCIN_VLED2_EN_SHIFT                               4
#define MT6336_AUXADC_DIG0_RSV0_L_ADDR                                   MT6336_PMIC_AUXADC_CON13
#define MT6336_AUXADC_DIG0_RSV0_L_MASK                                   0x1
#define MT6336_AUXADC_DIG0_RSV0_L_SHIFT                                  7
#define MT6336_AUXADC_DIG0_RSV0_H_ADDR                                   MT6336_PMIC_AUXADC_CON13_H
#define MT6336_AUXADC_DIG0_RSV0_H_MASK                                   0x7
#define MT6336_AUXADC_DIG0_RSV0_H_SHIFT                                  0
#define MT6336_AUXADC_CHSEL_ADDR                                         MT6336_PMIC_AUXADC_CON13_H
#define MT6336_AUXADC_CHSEL_MASK                                         0xF
#define MT6336_AUXADC_CHSEL_SHIFT                                        3
#define MT6336_AUXADC_SWCTRL_EN_ADDR                                     MT6336_PMIC_AUXADC_CON13_H
#define MT6336_AUXADC_SWCTRL_EN_MASK                                     0x1
#define MT6336_AUXADC_SWCTRL_EN_SHIFT                                    7
#define MT6336_AUXADC_SOURCE_VBUS_SOFT_OVP_SEL_ADDR                      MT6336_PMIC_AUXADC_CON14
#define MT6336_AUXADC_SOURCE_VBUS_SOFT_OVP_SEL_MASK                      0x1
#define MT6336_AUXADC_SOURCE_VBUS_SOFT_OVP_SEL_SHIFT                     0
#define MT6336_AUXADC_SOURCE_VBUS_SOFT_OVP2_SEL_ADDR                     MT6336_PMIC_AUXADC_CON14
#define MT6336_AUXADC_SOURCE_VBUS_SOFT_OVP2_SEL_MASK                     0x1
#define MT6336_AUXADC_SOURCE_VBUS_SOFT_OVP2_SEL_SHIFT                    1
#define MT6336_AUXADC_DIG0_RSV2_ADDR                                     MT6336_PMIC_AUXADC_CON14
#define MT6336_AUXADC_DIG0_RSV2_MASK                                     0x7
#define MT6336_AUXADC_DIG0_RSV2_SHIFT                                    2
#define MT6336_AUXADC_DIG1_RSV2_L_ADDR                                   MT6336_PMIC_AUXADC_CON14
#define MT6336_AUXADC_DIG1_RSV2_L_MASK                                   0x7
#define MT6336_AUXADC_DIG1_RSV2_L_SHIFT                                  5
#define MT6336_AUXADC_DIG1_RSV2_H_ADDR                                   MT6336_PMIC_AUXADC_CON14_H
#define MT6336_AUXADC_DIG1_RSV2_H_MASK                                   0x1
#define MT6336_AUXADC_DIG1_RSV2_H_SHIFT                                  0
#define MT6336_AUXADC_DAC_EXTD_ADDR                                      MT6336_PMIC_AUXADC_CON14_H
#define MT6336_AUXADC_DAC_EXTD_MASK                                      0xF
#define MT6336_AUXADC_DAC_EXTD_SHIFT                                     3
#define MT6336_AUXADC_DAC_EXTD_EN_ADDR                                   MT6336_PMIC_AUXADC_CON14_H
#define MT6336_AUXADC_DAC_EXTD_EN_MASK                                   0x1
#define MT6336_AUXADC_DAC_EXTD_EN_SHIFT                                  7
#define MT6336_AUXADC_PMU_THR_PDN_SW_ADDR                                MT6336_PMIC_AUXADC_CON15_H
#define MT6336_AUXADC_PMU_THR_PDN_SW_MASK                                0x1
#define MT6336_AUXADC_PMU_THR_PDN_SW_SHIFT                               2
#define MT6336_AUXADC_PMU_THR_PDN_SEL_ADDR                               MT6336_PMIC_AUXADC_CON15_H
#define MT6336_AUXADC_PMU_THR_PDN_SEL_MASK                               0x1
#define MT6336_AUXADC_PMU_THR_PDN_SEL_SHIFT                              3
#define MT6336_AUXADC_PMU_THR_PDN_STATUS_ADDR                            MT6336_PMIC_AUXADC_CON15_H
#define MT6336_AUXADC_PMU_THR_PDN_STATUS_MASK                            0x1
#define MT6336_AUXADC_PMU_THR_PDN_STATUS_SHIFT                           4
#define MT6336_AUXADC_DIG0_RSV1_ADDR                                     MT6336_PMIC_AUXADC_CON15_H
#define MT6336_AUXADC_DIG0_RSV1_MASK                                     0x7
#define MT6336_AUXADC_DIG0_RSV1_SHIFT                                    5
#define MT6336_AUXADC_START_SHADE_NUM_L_ADDR                             MT6336_PMIC_AUXADC_CON16
#define MT6336_AUXADC_START_SHADE_NUM_L_MASK                             0xFF
#define MT6336_AUXADC_START_SHADE_NUM_L_SHIFT                            0
#define MT6336_AUXADC_START_SHADE_NUM_H_ADDR                             MT6336_PMIC_AUXADC_CON16_H
#define MT6336_AUXADC_START_SHADE_NUM_H_MASK                             0x3
#define MT6336_AUXADC_START_SHADE_NUM_H_SHIFT                            0
#define MT6336_AUXADC_START_SHADE_EN_ADDR                                MT6336_PMIC_AUXADC_CON16_H
#define MT6336_AUXADC_START_SHADE_EN_MASK                                0x1
#define MT6336_AUXADC_START_SHADE_EN_SHIFT                               6
#define MT6336_AUXADC_START_SHADE_SEL_ADDR                               MT6336_PMIC_AUXADC_CON16_H
#define MT6336_AUXADC_START_SHADE_SEL_MASK                               0x1
#define MT6336_AUXADC_START_SHADE_SEL_SHIFT                              7
#define MT6336_AUXADC_ADC_RDY_WAKEUP_CLR_ADDR                            MT6336_PMIC_AUXADC_CON17
#define MT6336_AUXADC_ADC_RDY_WAKEUP_CLR_MASK                            0x1
#define MT6336_AUXADC_ADC_RDY_WAKEUP_CLR_SHIFT                           0
#define MT6336_AUXADC_ADC_RDY_FGADC_CLR_ADDR                             MT6336_PMIC_AUXADC_CON17
#define MT6336_AUXADC_ADC_RDY_FGADC_CLR_MASK                             0x1
#define MT6336_AUXADC_ADC_RDY_FGADC_CLR_SHIFT                            1
#define MT6336_AUXADC_ADC_RDY_CHRGO_CLR_ADDR                             MT6336_PMIC_AUXADC_CON17
#define MT6336_AUXADC_ADC_RDY_CHRGO_CLR_MASK                             0x1
#define MT6336_AUXADC_ADC_RDY_CHRGO_CLR_SHIFT                            2
#define MT6336_AUXADC_START_EXTD_ADDR                                    MT6336_PMIC_AUXADC_CON17_H
#define MT6336_AUXADC_START_EXTD_MASK                                    0x7F
#define MT6336_AUXADC_START_EXTD_SHIFT                                   0
#define MT6336_DA_TS_VBE_SEL_ADDR                                        MT6336_PMIC_AUXADC_ANA_0
#define MT6336_DA_TS_VBE_SEL_MASK                                        0x1
#define MT6336_DA_TS_VBE_SEL_SHIFT                                       0
#define MT6336_AUXADC_AUTORPT_PRD_L_ADDR                                 MT6336_PMIC_AUXADC_AUTORPT0
#define MT6336_AUXADC_AUTORPT_PRD_L_MASK                                 0xFF
#define MT6336_AUXADC_AUTORPT_PRD_L_SHIFT                                0
#define MT6336_AUXADC_AUTORPT_PRD_H_ADDR                                 MT6336_PMIC_AUXADC_AUTORPT0_H
#define MT6336_AUXADC_AUTORPT_PRD_H_MASK                                 0x3
#define MT6336_AUXADC_AUTORPT_PRD_H_SHIFT                                0
#define MT6336_AUXADC_AUTORPT_EN_ADDR                                    MT6336_PMIC_AUXADC_AUTORPT0_H
#define MT6336_AUXADC_AUTORPT_EN_MASK                                    0x1
#define MT6336_AUXADC_AUTORPT_EN_SHIFT                                   7
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBT_MAX_ADDR                        MT6336_PMIC_AUXADC_VBUS_SOFT_OVP0
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBT_MAX_MASK                        0xFF
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBT_MAX_SHIFT                       0
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBT_MIN_ADDR                        MT6336_PMIC_AUXADC_VBUS_SOFT_OVP0_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBT_MIN_MASK                        0xFF
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBT_MIN_SHIFT                       0
#define MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_15_0_L_ADDR                  MT6336_PMIC_AUXADC_VBUS_SOFT_OVP1
#define MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_15_0_L_MASK                  0xFF
#define MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_15_0_L_SHIFT                 0
#define MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_15_0_H_ADDR                  MT6336_PMIC_AUXADC_VBUS_SOFT_OVP1_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_15_0_H_MASK                  0xFF
#define MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_15_0_H_SHIFT                 0
#define MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_19_16_ADDR                   MT6336_PMIC_AUXADC_VBUS_SOFT_OVP2
#define MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_19_16_MASK                   0xF
#define MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_19_16_SHIFT                  0
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MAX_L_ADDR                      MT6336_PMIC_AUXADC_VBUS_SOFT_OVP3
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MAX_L_MASK                      0xFF
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MAX_L_SHIFT                     0
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MAX_H_ADDR                      MT6336_PMIC_AUXADC_VBUS_SOFT_OVP3_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MAX_H_MASK                      0xF
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MAX_H_SHIFT                     0
#define MT6336_AUXADC_VBUS_SOFT_OVP_IRQ_EN_MAX_ADDR                      MT6336_PMIC_AUXADC_VBUS_SOFT_OVP3_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_IRQ_EN_MAX_MASK                      0x1
#define MT6336_AUXADC_VBUS_SOFT_OVP_IRQ_EN_MAX_SHIFT                     4
#define MT6336_AUXADC_VBUS_SOFT_OVP_EN_MAX_ADDR                          MT6336_PMIC_AUXADC_VBUS_SOFT_OVP3_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_EN_MAX_MASK                          0x1
#define MT6336_AUXADC_VBUS_SOFT_OVP_EN_MAX_SHIFT                         5
#define MT6336_AUXADC_VBUS_SOFT_OVP_MAX_IRQ_B_ADDR                       MT6336_PMIC_AUXADC_VBUS_SOFT_OVP3_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_MAX_IRQ_B_MASK                       0x1
#define MT6336_AUXADC_VBUS_SOFT_OVP_MAX_IRQ_B_SHIFT                      7
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MIN_L_ADDR                      MT6336_PMIC_AUXADC_VBUS_SOFT_OVP4
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MIN_L_MASK                      0xFF
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MIN_L_SHIFT                     0
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MIN_H_ADDR                      MT6336_PMIC_AUXADC_VBUS_SOFT_OVP4_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MIN_H_MASK                      0xF
#define MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MIN_H_SHIFT                     0
#define MT6336_AUXADC_VBUS_SOFT_OVP_IRQ_EN_MIN_ADDR                      MT6336_PMIC_AUXADC_VBUS_SOFT_OVP4_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_IRQ_EN_MIN_MASK                      0x1
#define MT6336_AUXADC_VBUS_SOFT_OVP_IRQ_EN_MIN_SHIFT                     4
#define MT6336_AUXADC_VBUS_SOFT_OVP_EN_MIN_ADDR                          MT6336_PMIC_AUXADC_VBUS_SOFT_OVP4_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_EN_MIN_MASK                          0x1
#define MT6336_AUXADC_VBUS_SOFT_OVP_EN_MIN_SHIFT                         5
#define MT6336_AUXADC_VBUS_SOFT_OVP_MIN_IRQ_B_ADDR                       MT6336_PMIC_AUXADC_VBUS_SOFT_OVP4_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_MIN_IRQ_B_MASK                       0x1
#define MT6336_AUXADC_VBUS_SOFT_OVP_MIN_IRQ_B_SHIFT                      7
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MAX_L_ADDR            MT6336_PMIC_AUXADC_VBUS_SOFT_OVP5
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MAX_L_MASK            0xFF
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MAX_L_SHIFT           0
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MAX_H_ADDR            MT6336_PMIC_AUXADC_VBUS_SOFT_OVP5_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MAX_H_MASK            0x1
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MAX_H_SHIFT           0
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MIN_L_ADDR            MT6336_PMIC_AUXADC_VBUS_SOFT_OVP6
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MIN_L_MASK            0xFF
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MIN_L_SHIFT           0
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MIN_H_ADDR            MT6336_PMIC_AUXADC_VBUS_SOFT_OVP6_H
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MIN_H_MASK            0x1
#define MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MIN_H_SHIFT           0
#define MT6336_AUXADC_TYPEC_H_DEBT_MAX_ADDR                              MT6336_PMIC_AUXADC_TYPEC_H0
#define MT6336_AUXADC_TYPEC_H_DEBT_MAX_MASK                              0xFF
#define MT6336_AUXADC_TYPEC_H_DEBT_MAX_SHIFT                             0
#define MT6336_AUXADC_TYPEC_H_DEBT_MIN_ADDR                              MT6336_PMIC_AUXADC_TYPEC_H0_H
#define MT6336_AUXADC_TYPEC_H_DEBT_MIN_MASK                              0xFF
#define MT6336_AUXADC_TYPEC_H_DEBT_MIN_SHIFT                             0
#define MT6336_AUXADC_TYPEC_H_DET_PRD_15_0_L_ADDR                        MT6336_PMIC_AUXADC_TYPEC_H1
#define MT6336_AUXADC_TYPEC_H_DET_PRD_15_0_L_MASK                        0xFF
#define MT6336_AUXADC_TYPEC_H_DET_PRD_15_0_L_SHIFT                       0
#define MT6336_AUXADC_TYPEC_H_DET_PRD_15_0_H_ADDR                        MT6336_PMIC_AUXADC_TYPEC_H1_H
#define MT6336_AUXADC_TYPEC_H_DET_PRD_15_0_H_MASK                        0xFF
#define MT6336_AUXADC_TYPEC_H_DET_PRD_15_0_H_SHIFT                       0
#define MT6336_AUXADC_TYPEC_H_DET_PRD_19_16_ADDR                         MT6336_PMIC_AUXADC_TYPEC_H2
#define MT6336_AUXADC_TYPEC_H_DET_PRD_19_16_MASK                         0xF
#define MT6336_AUXADC_TYPEC_H_DET_PRD_19_16_SHIFT                        0
#define MT6336_AUXADC_TYPEC_H_VOLT_MAX_L_ADDR                            MT6336_PMIC_AUXADC_TYPEC_H3
#define MT6336_AUXADC_TYPEC_H_VOLT_MAX_L_MASK                            0xFF
#define MT6336_AUXADC_TYPEC_H_VOLT_MAX_L_SHIFT                           0
#define MT6336_AUXADC_TYPEC_H_VOLT_MAX_H_ADDR                            MT6336_PMIC_AUXADC_TYPEC_H3_H
#define MT6336_AUXADC_TYPEC_H_VOLT_MAX_H_MASK                            0xF
#define MT6336_AUXADC_TYPEC_H_VOLT_MAX_H_SHIFT                           0
#define MT6336_AUXADC_TYPEC_H_IRQ_EN_MAX_ADDR                            MT6336_PMIC_AUXADC_TYPEC_H3_H
#define MT6336_AUXADC_TYPEC_H_IRQ_EN_MAX_MASK                            0x1
#define MT6336_AUXADC_TYPEC_H_IRQ_EN_MAX_SHIFT                           4
#define MT6336_AUXADC_TYPEC_H_EN_MAX_ADDR                                MT6336_PMIC_AUXADC_TYPEC_H3_H
#define MT6336_AUXADC_TYPEC_H_EN_MAX_MASK                                0x1
#define MT6336_AUXADC_TYPEC_H_EN_MAX_SHIFT                               5
#define MT6336_AUXADC_TYPEC_H_MAX_IRQ_B_ADDR                             MT6336_PMIC_AUXADC_TYPEC_H3_H
#define MT6336_AUXADC_TYPEC_H_MAX_IRQ_B_MASK                             0x1
#define MT6336_AUXADC_TYPEC_H_MAX_IRQ_B_SHIFT                            7
#define MT6336_AUXADC_TYPEC_H_VOLT_MIN_L_ADDR                            MT6336_PMIC_AUXADC_TYPEC_H4
#define MT6336_AUXADC_TYPEC_H_VOLT_MIN_L_MASK                            0xFF
#define MT6336_AUXADC_TYPEC_H_VOLT_MIN_L_SHIFT                           0
#define MT6336_AUXADC_TYPEC_H_VOLT_MIN_H_ADDR                            MT6336_PMIC_AUXADC_TYPEC_H4_H
#define MT6336_AUXADC_TYPEC_H_VOLT_MIN_H_MASK                            0xF
#define MT6336_AUXADC_TYPEC_H_VOLT_MIN_H_SHIFT                           0
#define MT6336_AUXADC_TYPEC_H_IRQ_EN_MIN_ADDR                            MT6336_PMIC_AUXADC_TYPEC_H4_H
#define MT6336_AUXADC_TYPEC_H_IRQ_EN_MIN_MASK                            0x1
#define MT6336_AUXADC_TYPEC_H_IRQ_EN_MIN_SHIFT                           4
#define MT6336_AUXADC_TYPEC_H_EN_MIN_ADDR                                MT6336_PMIC_AUXADC_TYPEC_H4_H
#define MT6336_AUXADC_TYPEC_H_EN_MIN_MASK                                0x1
#define MT6336_AUXADC_TYPEC_H_EN_MIN_SHIFT                               5
#define MT6336_AUXADC_TYPEC_H_MIN_IRQ_B_ADDR                             MT6336_PMIC_AUXADC_TYPEC_H4_H
#define MT6336_AUXADC_TYPEC_H_MIN_IRQ_B_MASK                             0x1
#define MT6336_AUXADC_TYPEC_H_MIN_IRQ_B_SHIFT                            7
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MAX_L_ADDR                  MT6336_PMIC_AUXADC_TYPEC_H5
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MAX_L_MASK                  0xFF
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MAX_L_SHIFT                 0
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MAX_H_ADDR                  MT6336_PMIC_AUXADC_TYPEC_H5_H
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MAX_H_MASK                  0x1
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MAX_H_SHIFT                 0
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MIN_L_ADDR                  MT6336_PMIC_AUXADC_TYPEC_H6
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MIN_L_MASK                  0xFF
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MIN_L_SHIFT                 0
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MIN_H_ADDR                  MT6336_PMIC_AUXADC_TYPEC_H6_H
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MIN_H_MASK                  0x1
#define MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MIN_H_SHIFT                 0
#define MT6336_AUXADC_TYPEC_L_DEBT_MAX_ADDR                              MT6336_PMIC_AUXADC_TYPEC_L0
#define MT6336_AUXADC_TYPEC_L_DEBT_MAX_MASK                              0xFF
#define MT6336_AUXADC_TYPEC_L_DEBT_MAX_SHIFT                             0
#define MT6336_AUXADC_TYPEC_L_DEBT_MIN_ADDR                              MT6336_PMIC_AUXADC_TYPEC_L0_H
#define MT6336_AUXADC_TYPEC_L_DEBT_MIN_MASK                              0xFF
#define MT6336_AUXADC_TYPEC_L_DEBT_MIN_SHIFT                             0
#define MT6336_AUXADC_TYPEC_L_DET_PRD_15_0_L_ADDR                        MT6336_PMIC_AUXADC_TYPEC_L1
#define MT6336_AUXADC_TYPEC_L_DET_PRD_15_0_L_MASK                        0xFF
#define MT6336_AUXADC_TYPEC_L_DET_PRD_15_0_L_SHIFT                       0
#define MT6336_AUXADC_TYPEC_L_DET_PRD_15_0_H_ADDR                        MT6336_PMIC_AUXADC_TYPEC_L1_H
#define MT6336_AUXADC_TYPEC_L_DET_PRD_15_0_H_MASK                        0xFF
#define MT6336_AUXADC_TYPEC_L_DET_PRD_15_0_H_SHIFT                       0
#define MT6336_AUXADC_TYPEC_L_DET_PRD_19_16_ADDR                         MT6336_PMIC_AUXADC_TYPEC_L2
#define MT6336_AUXADC_TYPEC_L_DET_PRD_19_16_MASK                         0xF
#define MT6336_AUXADC_TYPEC_L_DET_PRD_19_16_SHIFT                        0
#define MT6336_AUXADC_TYPEC_L_VOLT_MAX_L_ADDR                            MT6336_PMIC_AUXADC_TYPEC_L3
#define MT6336_AUXADC_TYPEC_L_VOLT_MAX_L_MASK                            0xFF
#define MT6336_AUXADC_TYPEC_L_VOLT_MAX_L_SHIFT                           0
#define MT6336_AUXADC_TYPEC_L_VOLT_MAX_H_ADDR                            MT6336_PMIC_AUXADC_TYPEC_L3_H
#define MT6336_AUXADC_TYPEC_L_VOLT_MAX_H_MASK                            0xF
#define MT6336_AUXADC_TYPEC_L_VOLT_MAX_H_SHIFT                           0
#define MT6336_AUXADC_TYPEC_L_IRQ_EN_MAX_ADDR                            MT6336_PMIC_AUXADC_TYPEC_L3_H
#define MT6336_AUXADC_TYPEC_L_IRQ_EN_MAX_MASK                            0x1
#define MT6336_AUXADC_TYPEC_L_IRQ_EN_MAX_SHIFT                           4
#define MT6336_AUXADC_TYPEC_L_EN_MAX_ADDR                                MT6336_PMIC_AUXADC_TYPEC_L3_H
#define MT6336_AUXADC_TYPEC_L_EN_MAX_MASK                                0x1
#define MT6336_AUXADC_TYPEC_L_EN_MAX_SHIFT                               5
#define MT6336_AUXADC_TYPEC_L_MAX_IRQ_B_ADDR                             MT6336_PMIC_AUXADC_TYPEC_L3_H
#define MT6336_AUXADC_TYPEC_L_MAX_IRQ_B_MASK                             0x1
#define MT6336_AUXADC_TYPEC_L_MAX_IRQ_B_SHIFT                            7
#define MT6336_AUXADC_TYPEC_L_VOLT_MIN_L_ADDR                            MT6336_PMIC_AUXADC_TYPEC_L4
#define MT6336_AUXADC_TYPEC_L_VOLT_MIN_L_MASK                            0xFF
#define MT6336_AUXADC_TYPEC_L_VOLT_MIN_L_SHIFT                           0
#define MT6336_AUXADC_TYPEC_L_VOLT_MIN_H_ADDR                            MT6336_PMIC_AUXADC_TYPEC_L4_H
#define MT6336_AUXADC_TYPEC_L_VOLT_MIN_H_MASK                            0xF
#define MT6336_AUXADC_TYPEC_L_VOLT_MIN_H_SHIFT                           0
#define MT6336_AUXADC_TYPEC_L_IRQ_EN_MIN_ADDR                            MT6336_PMIC_AUXADC_TYPEC_L4_H
#define MT6336_AUXADC_TYPEC_L_IRQ_EN_MIN_MASK                            0x1
#define MT6336_AUXADC_TYPEC_L_IRQ_EN_MIN_SHIFT                           4
#define MT6336_AUXADC_TYPEC_L_EN_MIN_ADDR                                MT6336_PMIC_AUXADC_TYPEC_L4_H
#define MT6336_AUXADC_TYPEC_L_EN_MIN_MASK                                0x1
#define MT6336_AUXADC_TYPEC_L_EN_MIN_SHIFT                               5
#define MT6336_AUXADC_TYPEC_L_MIN_IRQ_B_ADDR                             MT6336_PMIC_AUXADC_TYPEC_L4_H
#define MT6336_AUXADC_TYPEC_L_MIN_IRQ_B_MASK                             0x1
#define MT6336_AUXADC_TYPEC_L_MIN_IRQ_B_SHIFT                            7
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MAX_L_ADDR                  MT6336_PMIC_AUXADC_TYPEC_L5
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MAX_L_MASK                  0xFF
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MAX_L_SHIFT                 0
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MAX_H_ADDR                  MT6336_PMIC_AUXADC_TYPEC_L5_H
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MAX_H_MASK                  0x1
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MAX_H_SHIFT                 0
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MIN_L_ADDR                  MT6336_PMIC_AUXADC_TYPEC_L6
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MIN_L_MASK                  0xFF
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MIN_L_SHIFT                 0
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MIN_H_ADDR                  MT6336_PMIC_AUXADC_TYPEC_L6_H
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MIN_H_MASK                  0x1
#define MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MIN_H_SHIFT                 0
#define MT6336_AUXADC_THR_DEBT_MAX_ADDR                                  MT6336_PMIC_AUXADC_THR0
#define MT6336_AUXADC_THR_DEBT_MAX_MASK                                  0xFF
#define MT6336_AUXADC_THR_DEBT_MAX_SHIFT                                 0
#define MT6336_AUXADC_THR_DEBT_MIN_ADDR                                  MT6336_PMIC_AUXADC_THR0_H
#define MT6336_AUXADC_THR_DEBT_MIN_MASK                                  0xFF
#define MT6336_AUXADC_THR_DEBT_MIN_SHIFT                                 0
#define MT6336_AUXADC_THR_DET_PRD_15_0_L_ADDR                            MT6336_PMIC_AUXADC_THR1
#define MT6336_AUXADC_THR_DET_PRD_15_0_L_MASK                            0xFF
#define MT6336_AUXADC_THR_DET_PRD_15_0_L_SHIFT                           0
#define MT6336_AUXADC_THR_DET_PRD_15_0_H_ADDR                            MT6336_PMIC_AUXADC_THR1_H
#define MT6336_AUXADC_THR_DET_PRD_15_0_H_MASK                            0xFF
#define MT6336_AUXADC_THR_DET_PRD_15_0_H_SHIFT                           0
#define MT6336_AUXADC_THR_DET_PRD_19_16_ADDR                             MT6336_PMIC_AUXADC_THR2
#define MT6336_AUXADC_THR_DET_PRD_19_16_MASK                             0xF
#define MT6336_AUXADC_THR_DET_PRD_19_16_SHIFT                            0
#define MT6336_AUXADC_THR_VOLT_MAX_L_ADDR                                MT6336_PMIC_AUXADC_THR3
#define MT6336_AUXADC_THR_VOLT_MAX_L_MASK                                0xFF
#define MT6336_AUXADC_THR_VOLT_MAX_L_SHIFT                               0
#define MT6336_AUXADC_THR_VOLT_MAX_H_ADDR                                MT6336_PMIC_AUXADC_THR3_H
#define MT6336_AUXADC_THR_VOLT_MAX_H_MASK                                0xF
#define MT6336_AUXADC_THR_VOLT_MAX_H_SHIFT                               0
#define MT6336_AUXADC_THR_IRQ_EN_MAX_ADDR                                MT6336_PMIC_AUXADC_THR3_H
#define MT6336_AUXADC_THR_IRQ_EN_MAX_MASK                                0x1
#define MT6336_AUXADC_THR_IRQ_EN_MAX_SHIFT                               4
#define MT6336_AUXADC_THR_EN_MAX_ADDR                                    MT6336_PMIC_AUXADC_THR3_H
#define MT6336_AUXADC_THR_EN_MAX_MASK                                    0x1
#define MT6336_AUXADC_THR_EN_MAX_SHIFT                                   5
#define MT6336_AUXADC_THR_MAX_IRQ_B_ADDR                                 MT6336_PMIC_AUXADC_THR3_H
#define MT6336_AUXADC_THR_MAX_IRQ_B_MASK                                 0x1
#define MT6336_AUXADC_THR_MAX_IRQ_B_SHIFT                                7
#define MT6336_AUXADC_THR_VOLT_MIN_L_ADDR                                MT6336_PMIC_AUXADC_THR4
#define MT6336_AUXADC_THR_VOLT_MIN_L_MASK                                0xFF
#define MT6336_AUXADC_THR_VOLT_MIN_L_SHIFT                               0
#define MT6336_AUXADC_THR_VOLT_MIN_H_ADDR                                MT6336_PMIC_AUXADC_THR4_H
#define MT6336_AUXADC_THR_VOLT_MIN_H_MASK                                0xF
#define MT6336_AUXADC_THR_VOLT_MIN_H_SHIFT                               0
#define MT6336_AUXADC_THR_IRQ_EN_MIN_ADDR                                MT6336_PMIC_AUXADC_THR4_H
#define MT6336_AUXADC_THR_IRQ_EN_MIN_MASK                                0x1
#define MT6336_AUXADC_THR_IRQ_EN_MIN_SHIFT                               4
#define MT6336_AUXADC_THR_EN_MIN_ADDR                                    MT6336_PMIC_AUXADC_THR4_H
#define MT6336_AUXADC_THR_EN_MIN_MASK                                    0x1
#define MT6336_AUXADC_THR_EN_MIN_SHIFT                                   5
#define MT6336_AUXADC_THR_MIN_IRQ_B_ADDR                                 MT6336_PMIC_AUXADC_THR4_H
#define MT6336_AUXADC_THR_MIN_IRQ_B_MASK                                 0x1
#define MT6336_AUXADC_THR_MIN_IRQ_B_SHIFT                                7
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MAX_L_ADDR                      MT6336_PMIC_AUXADC_THR5
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MAX_L_MASK                      0xFF
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MAX_L_SHIFT                     0
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MAX_H_ADDR                      MT6336_PMIC_AUXADC_THR5_H
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MAX_H_MASK                      0x1
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MAX_H_SHIFT                     0
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MIN_L_ADDR                      MT6336_PMIC_AUXADC_THR6
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MIN_L_MASK                      0xFF
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MIN_L_SHIFT                     0
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MIN_H_ADDR                      MT6336_PMIC_AUXADC_THR6_H
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MIN_H_MASK                      0x1
#define MT6336_AUXADC_THR_DEBOUNCE_COUNT_MIN_H_SHIFT                     0
#define MT6336_EFUSE_GAIN_CH4_TRIM_L_ADDR                                MT6336_PMIC_AUXADC_EFUSE0
#define MT6336_EFUSE_GAIN_CH4_TRIM_L_MASK                                0xFF
#define MT6336_EFUSE_GAIN_CH4_TRIM_L_SHIFT                               0
#define MT6336_EFUSE_GAIN_CH4_TRIM_H_ADDR                                MT6336_PMIC_AUXADC_EFUSE0_H
#define MT6336_EFUSE_GAIN_CH4_TRIM_H_MASK                                0xF
#define MT6336_EFUSE_GAIN_CH4_TRIM_H_SHIFT                               0
#define MT6336_EFUSE_OFFSET_CH4_TRIM_L_ADDR                              MT6336_PMIC_AUXADC_EFUSE1
#define MT6336_EFUSE_OFFSET_CH4_TRIM_L_MASK                              0xFF
#define MT6336_EFUSE_OFFSET_CH4_TRIM_L_SHIFT                             0
#define MT6336_EFUSE_OFFSET_CH4_TRIM_H_ADDR                              MT6336_PMIC_AUXADC_EFUSE1_H
#define MT6336_EFUSE_OFFSET_CH4_TRIM_H_MASK                              0x7
#define MT6336_EFUSE_OFFSET_CH4_TRIM_H_SHIFT                             0
#define MT6336_EFUSE_GAIN_CH0_TRIM_L_ADDR                                MT6336_PMIC_AUXADC_EFUSE2
#define MT6336_EFUSE_GAIN_CH0_TRIM_L_MASK                                0xFF
#define MT6336_EFUSE_GAIN_CH0_TRIM_L_SHIFT                               0
#define MT6336_EFUSE_GAIN_CH0_TRIM_H_ADDR                                MT6336_PMIC_AUXADC_EFUSE2_H
#define MT6336_EFUSE_GAIN_CH0_TRIM_H_MASK                                0xF
#define MT6336_EFUSE_GAIN_CH0_TRIM_H_SHIFT                               0
#define MT6336_EFUSE_OFFSET_CH0_TRIM_L_ADDR                              MT6336_PMIC_AUXADC_EFUSE3
#define MT6336_EFUSE_OFFSET_CH0_TRIM_L_MASK                              0xFF
#define MT6336_EFUSE_OFFSET_CH0_TRIM_L_SHIFT                             0
#define MT6336_EFUSE_OFFSET_CH0_TRIM_H_ADDR                              MT6336_PMIC_AUXADC_EFUSE3_H
#define MT6336_EFUSE_OFFSET_CH0_TRIM_H_MASK                              0x7
#define MT6336_EFUSE_OFFSET_CH0_TRIM_H_SHIFT                             0
#define MT6336_EFUSE_GAIN_CH7_TRIM_L_ADDR                                MT6336_PMIC_AUXADC_EFUSE4
#define MT6336_EFUSE_GAIN_CH7_TRIM_L_MASK                                0xFF
#define MT6336_EFUSE_GAIN_CH7_TRIM_L_SHIFT                               0
#define MT6336_EFUSE_GAIN_CH7_TRIM_H_ADDR                                MT6336_PMIC_AUXADC_EFUSE4_H
#define MT6336_EFUSE_GAIN_CH7_TRIM_H_MASK                                0xF
#define MT6336_EFUSE_GAIN_CH7_TRIM_H_SHIFT                               0
#define MT6336_EFUSE_OFFSET_CH7_TRIM_L_ADDR                              MT6336_PMIC_AUXADC_EFUSE5
#define MT6336_EFUSE_OFFSET_CH7_TRIM_L_MASK                              0xFF
#define MT6336_EFUSE_OFFSET_CH7_TRIM_L_SHIFT                             0
#define MT6336_EFUSE_OFFSET_CH7_TRIM_H_ADDR                              MT6336_PMIC_AUXADC_EFUSE5_H
#define MT6336_EFUSE_OFFSET_CH7_TRIM_H_MASK                              0x7
#define MT6336_EFUSE_OFFSET_CH7_TRIM_H_SHIFT                             0
#define MT6336_AUXADC_FGADC_START_SW_ADDR                                MT6336_PMIC_AUXADC_DBG0
#define MT6336_AUXADC_FGADC_START_SW_MASK                                0x1
#define MT6336_AUXADC_FGADC_START_SW_SHIFT                               0
#define MT6336_AUXADC_FGADC_START_SEL_ADDR                               MT6336_PMIC_AUXADC_DBG0
#define MT6336_AUXADC_FGADC_START_SEL_MASK                               0x1
#define MT6336_AUXADC_FGADC_START_SEL_SHIFT                              1
#define MT6336_AUXADC_FGADC_R_SW_ADDR                                    MT6336_PMIC_AUXADC_DBG0
#define MT6336_AUXADC_FGADC_R_SW_MASK                                    0x1
#define MT6336_AUXADC_FGADC_R_SW_SHIFT                                   2
#define MT6336_AUXADC_FGADC_R_SEL_ADDR                                   MT6336_PMIC_AUXADC_DBG0
#define MT6336_AUXADC_FGADC_R_SEL_MASK                                   0x1
#define MT6336_AUXADC_FGADC_R_SEL_SHIFT                                  3
#define MT6336_AUXADC_CHRGO_START_SW_ADDR                                MT6336_PMIC_AUXADC_DBG0
#define MT6336_AUXADC_CHRGO_START_SW_MASK                                0x1
#define MT6336_AUXADC_CHRGO_START_SW_SHIFT                               4
#define MT6336_AUXADC_CHRGO_START_SEL_ADDR                               MT6336_PMIC_AUXADC_DBG0
#define MT6336_AUXADC_CHRGO_START_SEL_MASK                               0x1
#define MT6336_AUXADC_CHRGO_START_SEL_SHIFT                              5
#define MT6336_AUXADC_DBG_DIG0_RSV2_L_ADDR                               MT6336_PMIC_AUXADC_DBG0
#define MT6336_AUXADC_DBG_DIG0_RSV2_L_MASK                               0x3
#define MT6336_AUXADC_DBG_DIG0_RSV2_L_SHIFT                              6
#define MT6336_AUXADC_DBG_DIG0_RSV2_H_ADDR                               MT6336_PMIC_AUXADC_DBG0_H
#define MT6336_AUXADC_DBG_DIG0_RSV2_H_MASK                               0x3
#define MT6336_AUXADC_DBG_DIG0_RSV2_H_SHIFT                              0
#define MT6336_AUXADC_DBG_DIG1_RSV2_ADDR                                 MT6336_PMIC_AUXADC_DBG0_H
#define MT6336_AUXADC_DBG_DIG1_RSV2_MASK                                 0x3F
#define MT6336_AUXADC_DBG_DIG1_RSV2_SHIFT                                2
#define MT6336_AUXADC_IMPEDANCE_CNT_ADDR                                 MT6336_PMIC_AUXADC_IMP0
#define MT6336_AUXADC_IMPEDANCE_CNT_MASK                                 0x3F
#define MT6336_AUXADC_IMPEDANCE_CNT_SHIFT                                0
#define MT6336_AUXADC_IMPEDANCE_CHSEL_ADDR                               MT6336_PMIC_AUXADC_IMP0
#define MT6336_AUXADC_IMPEDANCE_CHSEL_MASK                               0x1
#define MT6336_AUXADC_IMPEDANCE_CHSEL_SHIFT                              6
#define MT6336_AUXADC_IMPEDANCE_IRQ_CLR_ADDR                             MT6336_PMIC_AUXADC_IMP0
#define MT6336_AUXADC_IMPEDANCE_IRQ_CLR_MASK                             0x1
#define MT6336_AUXADC_IMPEDANCE_IRQ_CLR_SHIFT                            7
#define MT6336_AUXADC_IMPEDANCE_IRQ_STATUS_ADDR                          MT6336_PMIC_AUXADC_IMP0_H
#define MT6336_AUXADC_IMPEDANCE_IRQ_STATUS_MASK                          0x1
#define MT6336_AUXADC_IMPEDANCE_IRQ_STATUS_SHIFT                         0
#define MT6336_AUXADC_CLR_IMP_CNT_STOP_ADDR                              MT6336_PMIC_AUXADC_IMP0_H
#define MT6336_AUXADC_CLR_IMP_CNT_STOP_MASK                              0x1
#define MT6336_AUXADC_CLR_IMP_CNT_STOP_SHIFT                             6
#define MT6336_AUXADC_IMPEDANCE_MODE_ADDR                                MT6336_PMIC_AUXADC_IMP0_H
#define MT6336_AUXADC_IMPEDANCE_MODE_MASK                                0x1
#define MT6336_AUXADC_IMPEDANCE_MODE_SHIFT                               7
#define MT6336_AUXADC_IMP_AUTORPT_PRD_L_ADDR                             MT6336_PMIC_AUXADC_IMP1
#define MT6336_AUXADC_IMP_AUTORPT_PRD_L_MASK                             0xFF
#define MT6336_AUXADC_IMP_AUTORPT_PRD_L_SHIFT                            0
#define MT6336_AUXADC_IMP_AUTORPT_PRD_H_ADDR                             MT6336_PMIC_AUXADC_IMP1_H
#define MT6336_AUXADC_IMP_AUTORPT_PRD_H_MASK                             0x3
#define MT6336_AUXADC_IMP_AUTORPT_PRD_H_SHIFT                            0
#define MT6336_AUXADC_IMP_AUTORPT_EN_ADDR                                MT6336_PMIC_AUXADC_IMP1_H
#define MT6336_AUXADC_IMP_AUTORPT_EN_MASK                                0x1
#define MT6336_AUXADC_IMP_AUTORPT_EN_SHIFT                               7
#define MT6336_AUXADC_BAT_TEMP_FROZE_EN_ADDR                             MT6336_PMIC_AUXADC_BAT_TEMP_0
#define MT6336_AUXADC_BAT_TEMP_FROZE_EN_MASK                             0x1
#define MT6336_AUXADC_BAT_TEMP_FROZE_EN_SHIFT                            0
#define MT6336_AUXADC_BAT_TEMP_DEBT_MAX_ADDR                             MT6336_PMIC_AUXADC_BAT_TEMP_1
#define MT6336_AUXADC_BAT_TEMP_DEBT_MAX_MASK                             0xFF
#define MT6336_AUXADC_BAT_TEMP_DEBT_MAX_SHIFT                            0
#define MT6336_AUXADC_BAT_TEMP_DEBT_MIN_ADDR                             MT6336_PMIC_AUXADC_BAT_TEMP_1_H
#define MT6336_AUXADC_BAT_TEMP_DEBT_MIN_MASK                             0xFF
#define MT6336_AUXADC_BAT_TEMP_DEBT_MIN_SHIFT                            0
#define MT6336_AUXADC_BAT_TEMP_DET_PRD_15_0_L_ADDR                       MT6336_PMIC_AUXADC_BAT_TEMP_2
#define MT6336_AUXADC_BAT_TEMP_DET_PRD_15_0_L_MASK                       0xFF
#define MT6336_AUXADC_BAT_TEMP_DET_PRD_15_0_L_SHIFT                      0
#define MT6336_AUXADC_BAT_TEMP_DET_PRD_15_0_H_ADDR                       MT6336_PMIC_AUXADC_BAT_TEMP_2_H
#define MT6336_AUXADC_BAT_TEMP_DET_PRD_15_0_H_MASK                       0xFF
#define MT6336_AUXADC_BAT_TEMP_DET_PRD_15_0_H_SHIFT                      0
#define MT6336_AUXADC_BAT_TEMP_DET_PRD_19_16_ADDR                        MT6336_PMIC_AUXADC_BAT_TEMP_3
#define MT6336_AUXADC_BAT_TEMP_DET_PRD_19_16_MASK                        0xF
#define MT6336_AUXADC_BAT_TEMP_DET_PRD_19_16_SHIFT                       0
#define MT6336_AUXADC_BAT_TEMP_VOLT_MAX_L_ADDR                           MT6336_PMIC_AUXADC_BAT_TEMP_4
#define MT6336_AUXADC_BAT_TEMP_VOLT_MAX_L_MASK                           0xFF
#define MT6336_AUXADC_BAT_TEMP_VOLT_MAX_L_SHIFT                          0
#define MT6336_AUXADC_BAT_TEMP_VOLT_MAX_H_ADDR                           MT6336_PMIC_AUXADC_BAT_TEMP_4_H
#define MT6336_AUXADC_BAT_TEMP_VOLT_MAX_H_MASK                           0xF
#define MT6336_AUXADC_BAT_TEMP_VOLT_MAX_H_SHIFT                          0
#define MT6336_AUXADC_BAT_TEMP_IRQ_EN_MAX_ADDR                           MT6336_PMIC_AUXADC_BAT_TEMP_4_H
#define MT6336_AUXADC_BAT_TEMP_IRQ_EN_MAX_MASK                           0x1
#define MT6336_AUXADC_BAT_TEMP_IRQ_EN_MAX_SHIFT                          4
#define MT6336_AUXADC_BAT_TEMP_EN_MAX_ADDR                               MT6336_PMIC_AUXADC_BAT_TEMP_4_H
#define MT6336_AUXADC_BAT_TEMP_EN_MAX_MASK                               0x1
#define MT6336_AUXADC_BAT_TEMP_EN_MAX_SHIFT                              5
#define MT6336_AUXADC_BAT_TEMP_MAX_IRQ_B_ADDR                            MT6336_PMIC_AUXADC_BAT_TEMP_4_H
#define MT6336_AUXADC_BAT_TEMP_MAX_IRQ_B_MASK                            0x1
#define MT6336_AUXADC_BAT_TEMP_MAX_IRQ_B_SHIFT                           7
#define MT6336_AUXADC_BAT_TEMP_VOLT_MIN_L_ADDR                           MT6336_PMIC_AUXADC_BAT_TEMP_5
#define MT6336_AUXADC_BAT_TEMP_VOLT_MIN_L_MASK                           0xFF
#define MT6336_AUXADC_BAT_TEMP_VOLT_MIN_L_SHIFT                          0
#define MT6336_AUXADC_BAT_TEMP_VOLT_MIN_H_ADDR                           MT6336_PMIC_AUXADC_BAT_TEMP_5_H
#define MT6336_AUXADC_BAT_TEMP_VOLT_MIN_H_MASK                           0xF
#define MT6336_AUXADC_BAT_TEMP_VOLT_MIN_H_SHIFT                          0
#define MT6336_AUXADC_BAT_TEMP_IRQ_EN_MIN_ADDR                           MT6336_PMIC_AUXADC_BAT_TEMP_5_H
#define MT6336_AUXADC_BAT_TEMP_IRQ_EN_MIN_MASK                           0x1
#define MT6336_AUXADC_BAT_TEMP_IRQ_EN_MIN_SHIFT                          4
#define MT6336_AUXADC_BAT_TEMP_EN_MIN_ADDR                               MT6336_PMIC_AUXADC_BAT_TEMP_5_H
#define MT6336_AUXADC_BAT_TEMP_EN_MIN_MASK                               0x1
#define MT6336_AUXADC_BAT_TEMP_EN_MIN_SHIFT                              5
#define MT6336_AUXADC_BAT_TEMP_MIN_IRQ_B_ADDR                            MT6336_PMIC_AUXADC_BAT_TEMP_5_H
#define MT6336_AUXADC_BAT_TEMP_MIN_IRQ_B_MASK                            0x1
#define MT6336_AUXADC_BAT_TEMP_MIN_IRQ_B_SHIFT                           7
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MAX_L_ADDR                 MT6336_PMIC_AUXADC_BAT_TEMP_6
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MAX_L_MASK                 0xFF
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MAX_L_SHIFT                0
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MAX_H_ADDR                 MT6336_PMIC_AUXADC_BAT_TEMP_6_H
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MAX_H_MASK                 0x1
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MAX_H_SHIFT                0
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MIN_L_ADDR                 MT6336_PMIC_AUXADC_BAT_TEMP_7
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MIN_L_MASK                 0xFF
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MIN_L_SHIFT                0
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MIN_H_ADDR                 MT6336_PMIC_AUXADC_BAT_TEMP_7_H
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MIN_H_MASK                 0x1
#define MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MIN_H_SHIFT                0
#define MT6336_RG_EN_BUCK_ADDR                                           MT6336_PMIC_MAIN_CON0
#define MT6336_RG_EN_BUCK_MASK                                           0x1
#define MT6336_RG_EN_BUCK_SHIFT                                          0
#define MT6336_RG_EN_CHARGE_ADDR                                         MT6336_PMIC_MAIN_CON0
#define MT6336_RG_EN_CHARGE_MASK                                         0x1
#define MT6336_RG_EN_CHARGE_SHIFT                                        1
#define MT6336_RG_EN_OTGPIN_ADDR                                         MT6336_PMIC_MAIN_CON0
#define MT6336_RG_EN_OTGPIN_MASK                                         0x1
#define MT6336_RG_EN_OTGPIN_SHIFT                                        2
#define MT6336_RG_EN_OTG_ADDR                                            MT6336_PMIC_MAIN_CON0
#define MT6336_RG_EN_OTG_MASK                                            0x1
#define MT6336_RG_EN_OTG_SHIFT                                           3
#define MT6336_RG_EN_FLASH_ADDR                                          MT6336_PMIC_MAIN_CON0
#define MT6336_RG_EN_FLASH_MASK                                          0x1
#define MT6336_RG_EN_FLASH_SHIFT                                         4
#define MT6336_RG_EN_TORCH_ADDR                                          MT6336_PMIC_MAIN_CON0
#define MT6336_RG_EN_TORCH_MASK                                          0x1
#define MT6336_RG_EN_TORCH_SHIFT                                         5
#define MT6336_RG_EN_FLAASTOR_ADDR                                       MT6336_PMIC_MAIN_CON0
#define MT6336_RG_EN_FLAASTOR_MASK                                       0x1
#define MT6336_RG_EN_FLAASTOR_SHIFT                                      6
#define MT6336_RG_ON_VSYS_IDIS_ADDR                                      MT6336_PMIC_MAIN_CON1
#define MT6336_RG_ON_VSYS_IDIS_MASK                                      0x1
#define MT6336_RG_ON_VSYS_IDIS_SHIFT                                     0
#define MT6336_RG_EN_VSYS_IDIS_ADDR                                      MT6336_PMIC_MAIN_CON1
#define MT6336_RG_EN_VSYS_IDIS_MASK                                      0x1
#define MT6336_RG_EN_VSYS_IDIS_SHIFT                                     1
#define MT6336_RG_T_VSYS_IDIS_ADDR                                       MT6336_PMIC_MAIN_CON1
#define MT6336_RG_T_VSYS_IDIS_MASK                                       0x1
#define MT6336_RG_T_VSYS_IDIS_SHIFT                                      2
#define MT6336_RG_EN_RECHARGE_ADDR                                       MT6336_PMIC_MAIN_CON2
#define MT6336_RG_EN_RECHARGE_MASK                                       0x1
#define MT6336_RG_EN_RECHARGE_SHIFT                                      0
#define MT6336_RG_EN_TERM_ADDR                                           MT6336_PMIC_MAIN_CON2
#define MT6336_RG_EN_TERM_MASK                                           0x1
#define MT6336_RG_EN_TERM_SHIFT                                          1
#define MT6336_RG_T_TERM_EXT_ADDR                                        MT6336_PMIC_MAIN_CON2
#define MT6336_RG_T_TERM_EXT_MASK                                        0x7
#define MT6336_RG_T_TERM_EXT_SHIFT                                       2
#define MT6336_RG_DIS_REVFET_ADDR                                        MT6336_PMIC_MAIN_CON2
#define MT6336_RG_DIS_REVFET_MASK                                        0x1
#define MT6336_RG_DIS_REVFET_SHIFT                                       5
#define MT6336_RG_TSTEP_VSYSREG_ADDR                                     MT6336_PMIC_MAIN_CON3
#define MT6336_RG_TSTEP_VSYSREG_MASK                                     0x1
#define MT6336_RG_TSTEP_VSYSREG_SHIFT                                    0
#define MT6336_RG_T_BSTCHK_SEL_ADDR                                      MT6336_PMIC_MAIN_CON3
#define MT6336_RG_T_BSTCHK_SEL_MASK                                      0x1
#define MT6336_RG_T_BSTCHK_SEL_SHIFT                                     1
#define MT6336_RG_FORCE_LOWQ_MODE_ADDR                                   MT6336_PMIC_MAIN_CON3
#define MT6336_RG_FORCE_LOWQ_MODE_MASK                                   0x1
#define MT6336_RG_FORCE_LOWQ_MODE_SHIFT                                  3
#define MT6336_RG_DIS_BATOC_MODE_ADDR                                    MT6336_PMIC_MAIN_CON3
#define MT6336_RG_DIS_BATOC_MODE_MASK                                    0x1
#define MT6336_RG_DIS_BATOC_MODE_SHIFT                                   4
#define MT6336_RG_EN_CLKSQ_GLOBAL_ADDR                                   MT6336_PMIC_MAIN_CON4
#define MT6336_RG_EN_CLKSQ_GLOBAL_MASK                                   0x1
#define MT6336_RG_EN_CLKSQ_GLOBAL_SHIFT                                  0
#define MT6336_RG_EN_CLKSQ_PDTYPEC_ADDR                                  MT6336_PMIC_MAIN_CON4
#define MT6336_RG_EN_CLKSQ_PDTYPEC_MASK                                  0x1
#define MT6336_RG_EN_CLKSQ_PDTYPEC_SHIFT                                 1
#define MT6336_RG_EN_VBUS_IDIS_ADDR                                      MT6336_PMIC_MAIN_CON5
#define MT6336_RG_EN_VBUS_IDIS_MASK                                      0x1
#define MT6336_RG_EN_VBUS_IDIS_SHIFT                                     0
#define MT6336_RG_T_VBUS_IDIS_ADDR                                       MT6336_PMIC_MAIN_CON5
#define MT6336_RG_T_VBUS_IDIS_MASK                                       0x1
#define MT6336_RG_T_VBUS_IDIS_SHIFT                                      1
#define MT6336_RG_ON_VBUS_IDIS_ADDR                                      MT6336_PMIC_MAIN_CON5
#define MT6336_RG_ON_VBUS_IDIS_MASK                                      0x1
#define MT6336_RG_ON_VBUS_IDIS_SHIFT                                     2
#define MT6336_RG_MAIN_RSV0_ADDR                                         MT6336_PMIC_MAIN_RSV0
#define MT6336_RG_MAIN_RSV0_MASK                                         0xFF
#define MT6336_RG_MAIN_RSV0_SHIFT                                        0
#define MT6336_RG_CK_T_HANDSHAKE_SEL_ADDR                                MT6336_PMIC_MAIN_CON6
#define MT6336_RG_CK_T_HANDSHAKE_SEL_MASK                                0x3
#define MT6336_RG_CK_T_HANDSHAKE_SEL_SHIFT                               1
#define MT6336_RG_SWCHR_RST_SW_ADDR                                      MT6336_PMIC_MAIN_CON7
#define MT6336_RG_SWCHR_RST_SW_MASK                                      0x1
#define MT6336_RG_SWCHR_RST_SW_SHIFT                                     0
#define MT6336_RG_SWCHR_RST_GLOBAL_ADDR                                  MT6336_PMIC_MAIN_CON7
#define MT6336_RG_SWCHR_RST_GLOBAL_MASK                                  0x1
#define MT6336_RG_SWCHR_RST_GLOBAL_SHIFT                                 1
#define MT6336_RG_DIS_LOWQ_MODE_ADDR                                     MT6336_PMIC_MAIN_CON8
#define MT6336_RG_DIS_LOWQ_MODE_MASK                                     0x1
#define MT6336_RG_DIS_LOWQ_MODE_SHIFT                                    0
#define MT6336_RG_DIS_SHIP_MODE_ADDR                                     MT6336_PMIC_MAIN_CON8
#define MT6336_RG_DIS_SHIP_MODE_MASK                                     0x1
#define MT6336_RG_DIS_SHIP_MODE_SHIFT                                    1
#define MT6336_RG_DIS_PWR_PROTECTION_ADDR                                MT6336_PMIC_MAIN_CON9
#define MT6336_RG_DIS_PWR_PROTECTION_MASK                                0x1
#define MT6336_RG_DIS_PWR_PROTECTION_SHIFT                               0
#define MT6336_RG_DIS_BAT_OC_MODE_ADDR                                   MT6336_PMIC_MAIN_CON9
#define MT6336_RG_DIS_BAT_OC_MODE_MASK                                   0x1
#define MT6336_RG_DIS_BAT_OC_MODE_SHIFT                                  1
#define MT6336_RG_DIS_BAT_DEAD_MODE_ADDR                                 MT6336_PMIC_MAIN_CON9
#define MT6336_RG_DIS_BAT_DEAD_MODE_MASK                                 0x1
#define MT6336_RG_DIS_BAT_DEAD_MODE_SHIFT                                2
#define MT6336_RG_PWRON_1_RSV_ADDR                                       MT6336_PMIC_MAIN_CON10
#define MT6336_RG_PWRON_1_RSV_MASK                                       0xFF
#define MT6336_RG_PWRON_1_RSV_SHIFT                                      0
#define MT6336_RG_PWRON_0_RSV_ADDR                                       MT6336_PMIC_MAIN_CON11
#define MT6336_RG_PWRON_0_RSV_MASK                                       0xFF
#define MT6336_RG_PWRON_0_RSV_SHIFT                                      0
#define MT6336_RG_BUCK_1_RSV_ADDR                                        MT6336_PMIC_MAIN_CON12
#define MT6336_RG_BUCK_1_RSV_MASK                                        0xFF
#define MT6336_RG_BUCK_1_RSV_SHIFT                                       0
#define MT6336_RG_BUCK_0_RSV_ADDR                                        MT6336_PMIC_MAIN_CON13
#define MT6336_RG_BUCK_0_RSV_MASK                                        0xFF
#define MT6336_RG_BUCK_0_RSV_SHIFT                                       0
#define MT6336_RG_TOLP_ON_ADDR                                           MT6336_PMIC_OTG_CTRL0
#define MT6336_RG_TOLP_ON_MASK                                           0x3
#define MT6336_RG_TOLP_ON_SHIFT                                          0
#define MT6336_RG_TOLP_OFF_ADDR                                          MT6336_PMIC_OTG_CTRL0
#define MT6336_RG_TOLP_OFF_MASK                                          0x1
#define MT6336_RG_TOLP_OFF_SHIFT                                         2
#define MT6336_RG_OTG_ON_MODE_SEL_ADDR                                   MT6336_PMIC_OTG_CTRL1
#define MT6336_RG_OTG_ON_MODE_SEL_MASK                                   0x1
#define MT6336_RG_OTG_ON_MODE_SEL_SHIFT                                  0
#define MT6336_RG_OTG_IOLP_ADDR                                          MT6336_PMIC_OTG_CTRL2
#define MT6336_RG_OTG_IOLP_MASK                                          0x7
#define MT6336_RG_OTG_IOLP_SHIFT                                         0
#define MT6336_RG_OTG_VCV_ADDR                                           MT6336_PMIC_OTG_CTRL2
#define MT6336_RG_OTG_VCV_MASK                                           0xF
#define MT6336_RG_OTG_VCV_SHIFT                                          4
#define MT6336_RG_EN_STROBEPIN_ADDR                                      MT6336_PMIC_FLASH_CTRL0
#define MT6336_RG_EN_STROBEPIN_MASK                                      0x1
#define MT6336_RG_EN_STROBEPIN_SHIFT                                     0
#define MT6336_RG_EN_TXMASKPIN_ADDR                                      MT6336_PMIC_FLASH_CTRL0
#define MT6336_RG_EN_TXMASKPIN_MASK                                      0x1
#define MT6336_RG_EN_TXMASKPIN_SHIFT                                     1
#define MT6336_RG_EN_IFLA1_ADDR                                          MT6336_PMIC_FLASH_CTRL1
#define MT6336_RG_EN_IFLA1_MASK                                          0x1
#define MT6336_RG_EN_IFLA1_SHIFT                                         0
#define MT6336_RG_EN_IFLA2_ADDR                                          MT6336_PMIC_FLASH_CTRL1
#define MT6336_RG_EN_IFLA2_MASK                                          0x1
#define MT6336_RG_EN_IFLA2_SHIFT                                         1
#define MT6336_RG_FLA_WDT_ADDR                                           MT6336_PMIC_FLASH_CTRL1
#define MT6336_RG_FLA_WDT_MASK                                           0x3
#define MT6336_RG_FLA_WDT_SHIFT                                          2
#define MT6336_RG_EN_FLA_WDT_ADDR                                        MT6336_PMIC_FLASH_CTRL1
#define MT6336_RG_EN_FLA_WDT_MASK                                        0x1
#define MT6336_RG_EN_FLA_WDT_SHIFT                                       4
#define MT6336_RG_EN_LEDCS_ADDR                                          MT6336_PMIC_FLASH_CTRL1
#define MT6336_RG_EN_LEDCS_MASK                                          0x1
#define MT6336_RG_EN_LEDCS_SHIFT                                         5
#define MT6336_RG_IFLA1_ADDR                                             MT6336_PMIC_FLASH_CTRL2
#define MT6336_RG_IFLA1_MASK                                             0x7F
#define MT6336_RG_IFLA1_SHIFT                                            0
#define MT6336_RG_IFLA2_ADDR                                             MT6336_PMIC_FLASH_CTRL3
#define MT6336_RG_IFLA2_MASK                                             0x7F
#define MT6336_RG_IFLA2_SHIFT                                            0
#define MT6336_RG_TSTEP_ILED_ADDR                                        MT6336_PMIC_FLASH_CTRL4
#define MT6336_RG_TSTEP_ILED_MASK                                        0x3
#define MT6336_RG_TSTEP_ILED_SHIFT                                       0
#define MT6336_RG_VFLA_ADDR                                              MT6336_PMIC_FLASH_CTRL5
#define MT6336_RG_VFLA_MASK                                              0xF
#define MT6336_RG_VFLA_SHIFT                                             0
#define MT6336_RG_ITOR1_ADDR                                             MT6336_PMIC_TORCH_CTRL0
#define MT6336_RG_ITOR1_MASK                                             0x7F
#define MT6336_RG_ITOR1_SHIFT                                            0
#define MT6336_RG_ITOR2_ADDR                                             MT6336_PMIC_TORCH_CTRL1
#define MT6336_RG_ITOR2_MASK                                             0x7F
#define MT6336_RG_ITOR2_SHIFT                                            0
#define MT6336_RG_EN_ITOR1_ADDR                                          MT6336_PMIC_TORCH_CTRL2
#define MT6336_RG_EN_ITOR1_MASK                                          0x1
#define MT6336_RG_EN_ITOR1_SHIFT                                         0
#define MT6336_RG_EN_ITOR2_ADDR                                          MT6336_PMIC_TORCH_CTRL2
#define MT6336_RG_EN_ITOR2_MASK                                          0x1
#define MT6336_RG_EN_ITOR2_SHIFT                                         1
#define MT6336_RG_EN_TOR_WDT_ADDR                                        MT6336_PMIC_TORCH_CTRL2
#define MT6336_RG_EN_TOR_WDT_MASK                                        0x1
#define MT6336_RG_EN_TOR_WDT_SHIFT                                       2
#define MT6336_RG_OS_RESTART_ADDR                                        MT6336_PMIC_TORCH_CTRL3
#define MT6336_RG_OS_RESTART_MASK                                        0x1
#define MT6336_RG_OS_RESTART_SHIFT                                       0
#define MT6336_RG_FLA_BYTOROS_MODE_ADDR                                  MT6336_PMIC_TORCH_CTRL3
#define MT6336_RG_FLA_BYTOROS_MODE_MASK                                  0x1
#define MT6336_RG_FLA_BYTOROS_MODE_SHIFT                                 1
#define MT6336_RG_TOR_DRV_MODE0_ADDR                                     MT6336_PMIC_TORCH_CTRL3
#define MT6336_RG_TOR_DRV_MODE0_MASK                                     0x1
#define MT6336_RG_TOR_DRV_MODE0_SHIFT                                    2
#define MT6336_RG_TOR_DRV_MODE1_ADDR                                     MT6336_PMIC_TORCH_CTRL3
#define MT6336_RG_TOR_DRV_MODE1_MASK                                     0x1
#define MT6336_RG_TOR_DRV_MODE1_SHIFT                                    3
#define MT6336_RG_TOR_SS_MODE_ADDR                                       MT6336_PMIC_TORCH_CTRL3
#define MT6336_RG_TOR_SS_MODE_MASK                                       0x1
#define MT6336_RG_TOR_SS_MODE_SHIFT                                      4
#define MT6336_RG_SSCTRL_SEL_ADDR                                        MT6336_PMIC_BOOST_CTRL0
#define MT6336_RG_SSCTRL_SEL_MASK                                        0x1
#define MT6336_RG_SSCTRL_SEL_SHIFT                                       0
#define MT6336_RGS_FLA1_OS_TIMEOUT_ADDR                                  MT6336_PMIC_BOOST_CTRL1
#define MT6336_RGS_FLA1_OS_TIMEOUT_MASK                                  0x1
#define MT6336_RGS_FLA1_OS_TIMEOUT_SHIFT                                 0
#define MT6336_RGS_FLA2_OS_TIMEOUT_ADDR                                  MT6336_PMIC_BOOST_CTRL1
#define MT6336_RGS_FLA2_OS_TIMEOUT_MASK                                  0x1
#define MT6336_RGS_FLA2_OS_TIMEOUT_SHIFT                                 1
#define MT6336_RGS_TOR1_OS_TIMEOUT_ADDR                                  MT6336_PMIC_BOOST_CTRL1
#define MT6336_RGS_TOR1_OS_TIMEOUT_MASK                                  0x1
#define MT6336_RGS_TOR1_OS_TIMEOUT_SHIFT                                 2
#define MT6336_RGS_TOR2_OS_TIMEOUT_ADDR                                  MT6336_PMIC_BOOST_CTRL1
#define MT6336_RGS_TOR2_OS_TIMEOUT_MASK                                  0x1
#define MT6336_RGS_TOR2_OS_TIMEOUT_SHIFT                                 3
#define MT6336_RG_BOOST_RSV0_ADDR                                        MT6336_PMIC_BOOST_RSV0
#define MT6336_RG_BOOST_RSV0_MASK                                        0xFF
#define MT6336_RG_BOOST_RSV0_SHIFT                                       0
#define MT6336_RG_BOOST_RSV1_ADDR                                        MT6336_PMIC_BOOST_RSV1
#define MT6336_RG_BOOST_RSV1_MASK                                        0xFF
#define MT6336_RG_BOOST_RSV1_SHIFT                                       0
#define MT6336_RG_PEP_DATA_ADDR                                          MT6336_PMIC_PE_CON0
#define MT6336_RG_PEP_DATA_MASK                                          0xFF
#define MT6336_RG_PEP_DATA_SHIFT                                         0
#define MT6336_RG_PE_SEL_ADDR                                            MT6336_PMIC_PE_CON1
#define MT6336_RG_PE_SEL_MASK                                            0x1
#define MT6336_RG_PE_SEL_SHIFT                                           0
#define MT6336_RG_PE_BIT_SEL_ADDR                                        MT6336_PMIC_PE_CON1
#define MT6336_RG_PE_BIT_SEL_MASK                                        0x3
#define MT6336_RG_PE_BIT_SEL_SHIFT                                       1
#define MT6336_RG_PE_TC_SEL_ADDR                                         MT6336_PMIC_PE_CON1
#define MT6336_RG_PE_TC_SEL_MASK                                         0x3
#define MT6336_RG_PE_TC_SEL_SHIFT                                        3
#define MT6336_RG_PE_ENABLE_ADDR                                         MT6336_PMIC_PE_CON1
#define MT6336_RG_PE_ENABLE_MASK                                         0x1
#define MT6336_RG_PE_ENABLE_SHIFT                                        5
#define MT6336_RGS_PE_STATUS_ADDR                                        MT6336_PMIC_PE_CON1
#define MT6336_RGS_PE_STATUS_MASK                                        0x1
#define MT6336_RGS_PE_STATUS_SHIFT                                       6
#define MT6336_RG_ICC_JW_ADDR                                            MT6336_PMIC_JEITA_CON0
#define MT6336_RG_ICC_JW_MASK                                            0x7F
#define MT6336_RG_ICC_JW_SHIFT                                           0
#define MT6336_RG_ICC_JC_ADDR                                            MT6336_PMIC_JEITA_CON1
#define MT6336_RG_ICC_JC_MASK                                            0x7F
#define MT6336_RG_ICC_JC_SHIFT                                           0
#define MT6336_RG_DISWARMCOOL_ADDR                                       MT6336_PMIC_JEITA_CON2
#define MT6336_RG_DISWARMCOOL_MASK                                       0x1
#define MT6336_RG_DISWARMCOOL_SHIFT                                      3
#define MT6336_RG_TSTEP_ICC_ADDR                                         MT6336_PMIC_JEITA_CON2
#define MT6336_RG_TSTEP_ICC_MASK                                         0x1
#define MT6336_RG_TSTEP_ICC_SHIFT                                        4
#define MT6336_RG_EN_ICC_SFSTR_ADDR                                      MT6336_PMIC_JEITA_CON2
#define MT6336_RG_EN_ICC_SFSTR_MASK                                      0x1
#define MT6336_RG_EN_ICC_SFSTR_SHIFT                                     6
#define MT6336_RG_EN_HWJEITA_ADDR                                        MT6336_PMIC_JEITA_CON2
#define MT6336_RG_EN_HWJEITA_MASK                                        0x1
#define MT6336_RG_EN_HWJEITA_SHIFT                                       7
#define MT6336_RG_T_MASKTMR_ADDR                                         MT6336_PMIC_JEITA_CON3
#define MT6336_RG_T_MASKTMR_MASK                                         0x1
#define MT6336_RG_T_MASKTMR_SHIFT                                        0
#define MT6336_RG_RST_MASKTMR_ADDR                                       MT6336_PMIC_JEITA_CON3
#define MT6336_RG_RST_MASKTMR_MASK                                       0x1
#define MT6336_RG_RST_MASKTMR_SHIFT                                      1
#define MT6336_RG_EN_MASKTMR_ADDR                                        MT6336_PMIC_JEITA_CON3
#define MT6336_RG_EN_MASKTMR_MASK                                        0x1
#define MT6336_RG_EN_MASKTMR_SHIFT                                       2
#define MT6336_RG_EN_MASKJEITA_ADDR                                      MT6336_PMIC_JEITA_CON3
#define MT6336_RG_EN_MASKJEITA_MASK                                      0x1
#define MT6336_RG_EN_MASKJEITA_SHIFT                                     3
#define MT6336_RG_ICC_ADDR                                               MT6336_PMIC_ICC_CON0
#define MT6336_RG_ICC_MASK                                               0x7F
#define MT6336_RG_ICC_SHIFT                                              0
#define MT6336_RG_VCV_ADDR                                               MT6336_PMIC_VCV_CON0
#define MT6336_RG_VCV_MASK                                               0xFF
#define MT6336_RG_VCV_SHIFT                                              0
#define MT6336_RG_VCV_JW_ADDR                                            MT6336_PMIC_JEITA_CON4
#define MT6336_RG_VCV_JW_MASK                                            0xFF
#define MT6336_RG_VCV_JW_SHIFT                                           0
#define MT6336_RG_VCV_JC_ADDR                                            MT6336_PMIC_JEITA_CON5
#define MT6336_RG_VCV_JC_MASK                                            0xFF
#define MT6336_RG_VCV_JC_SHIFT                                           0
#define MT6336_RG_IPRECC1_JC_ADDR                                        MT6336_PMIC_JEITA_CON6
#define MT6336_RG_IPRECC1_JC_MASK                                        0x3
#define MT6336_RG_IPRECC1_JC_SHIFT                                       0
#define MT6336_RG_IPRECC1_JW_ADDR                                        MT6336_PMIC_JEITA_CON6
#define MT6336_RG_IPRECC1_JW_MASK                                        0x3
#define MT6336_RG_IPRECC1_JW_SHIFT                                       2
#define MT6336_RG_IPRECC1_ADDR                                           MT6336_PMIC_JEITA_CON6
#define MT6336_RG_IPRECC1_MASK                                           0x3
#define MT6336_RG_IPRECC1_SHIFT                                          4
#define MT6336_RG_BC12_RST_ADDR                                          MT6336_PMIC_BC12_CON0
#define MT6336_RG_BC12_RST_MASK                                          0x1
#define MT6336_RG_BC12_RST_SHIFT                                         0
#define MT6336_RG_BC12_EN_ADDR                                           MT6336_PMIC_BC12_CON0
#define MT6336_RG_BC12_EN_MASK                                           0x1
#define MT6336_RG_BC12_EN_SHIFT                                          1
#define MT6336_RG_BC12_TIMER_EN_ADDR                                     MT6336_PMIC_BC12_CON0
#define MT6336_RG_BC12_TIMER_EN_MASK                                     0x1
#define MT6336_RG_BC12_TIMER_EN_SHIFT                                    2
#define MT6336_RG_BC12_CLEAR_ADDR                                        MT6336_PMIC_BC12_CON0
#define MT6336_RG_BC12_CLEAR_MASK                                        0x1
#define MT6336_RG_BC12_CLEAR_SHIFT                                       3
#define MT6336_RG_ITERM_ADDR                                             MT6336_PMIC_GER_CON0
#define MT6336_RG_ITERM_MASK                                             0xF
#define MT6336_RG_ITERM_SHIFT                                            4
#define MT6336_RG_R_IRCOMP_ADDR                                          MT6336_PMIC_GER_CON1
#define MT6336_RG_R_IRCOMP_MASK                                          0x7
#define MT6336_RG_R_IRCOMP_SHIFT                                         0
#define MT6336_RG_THR_TTH_ADDR                                           MT6336_PMIC_GER_CON1
#define MT6336_RG_THR_TTH_MASK                                           0x3
#define MT6336_RG_THR_TTH_SHIFT                                          3
#define MT6336_RG_VPAM_ADDR                                              MT6336_PMIC_GER_CON2
#define MT6336_RG_VPAM_MASK                                              0x1F
#define MT6336_RG_VPAM_SHIFT                                             0
#define MT6336_RG_PLUG_RSTB_SEL_ADDR                                     MT6336_PMIC_GER_CON2
#define MT6336_RG_PLUG_RSTB_SEL_MASK                                     0x1
#define MT6336_RG_PLUG_RSTB_SEL_SHIFT                                    5
#define MT6336_RG_VSYSREG_ADDR                                           MT6336_PMIC_GER_CON3
#define MT6336_RG_VSYSREG_MASK                                           0xFF
#define MT6336_RG_VSYSREG_SHIFT                                          0
#define MT6336_RG_VRECHG_ADDR                                            MT6336_PMIC_GER_CON4
#define MT6336_RG_VRECHG_MASK                                            0x3
#define MT6336_RG_VRECHG_SHIFT                                           0
#define MT6336_RG_ON_VBAT_IDIS_ADDR                                      MT6336_PMIC_GER_CON5
#define MT6336_RG_ON_VBAT_IDIS_MASK                                      0x1
#define MT6336_RG_ON_VBAT_IDIS_SHIFT                                     0
#define MT6336_RG_EN_VBAT_IDIS_ADDR                                      MT6336_PMIC_GER_CON5
#define MT6336_RG_EN_VBAT_IDIS_MASK                                      0x1
#define MT6336_RG_EN_VBAT_IDIS_SHIFT                                     1
#define MT6336_RG_VBATFETON_ADDR                                         MT6336_PMIC_GER_CON5
#define MT6336_RG_VBATFETON_MASK                                         0x7
#define MT6336_RG_VBATFETON_SHIFT                                        2
#define MT6336_RG_EN_VSYSREG_SFSTR_ADDR                                  MT6336_PMIC_GER_CON5
#define MT6336_RG_EN_VSYSREG_SFSTR_MASK                                  0x1
#define MT6336_RG_EN_VSYSREG_SFSTR_SHIFT                                 5
#define MT6336_RG_EN_VSYSREG_POST_SFSTR_ADDR                             MT6336_PMIC_GER_CON5
#define MT6336_RG_EN_VSYSREG_POST_SFSTR_MASK                             0x1
#define MT6336_RG_EN_VSYSREG_POST_SFSTR_SHIFT                            6
#define MT6336_RG_DIS_PP_EN_PIN_ADDR                                     MT6336_PMIC_LONG_PRESS_CON0
#define MT6336_RG_DIS_PP_EN_PIN_MASK                                     0x1
#define MT6336_RG_DIS_PP_EN_PIN_SHIFT                                    0
#define MT6336_RG_T_LONGPRESS_SEL_ADDR                                   MT6336_PMIC_LONG_PRESS_CON0
#define MT6336_RG_T_LONGPRESS_SEL_MASK                                   0x1
#define MT6336_RG_T_LONGPRESS_SEL_SHIFT                                  1
#define MT6336_RG_FLAG_MODE_ADDR                                         MT6336_PMIC_LONG_PRESS_CON0
#define MT6336_RG_FLAG_MODE_MASK                                         0x1
#define MT6336_RG_FLAG_MODE_SHIFT                                        2
#define MT6336_DD_FLAG_OUT_ADDR                                          MT6336_PMIC_LONG_PRESS_CON0
#define MT6336_DD_FLAG_OUT_MASK                                          0x1
#define MT6336_DD_FLAG_OUT_SHIFT                                         3
#define MT6336_RG_T_SHIP_DLY_SEL_ADDR                                    MT6336_PMIC_SHIP_MODE_CON0
#define MT6336_RG_T_SHIP_DLY_SEL_MASK                                    0x1
#define MT6336_RG_T_SHIP_DLY_SEL_SHIFT                                   0
#define MT6336_RG_EN_SHIP_ADDR                                           MT6336_PMIC_SHIP_MODE_CON0
#define MT6336_RG_EN_SHIP_MASK                                           0x1
#define MT6336_RG_EN_SHIP_SHIFT                                          1
#define MT6336_RG_EN_WDT_ADDR                                            MT6336_PMIC_WDT_CON0
#define MT6336_RG_EN_WDT_MASK                                            0x1
#define MT6336_RG_EN_WDT_SHIFT                                           0
#define MT6336_RG_WDT_ADDR                                               MT6336_PMIC_WDT_CON0
#define MT6336_RG_WDT_MASK                                               0x7
#define MT6336_RG_WDT_SHIFT                                              1
#define MT6336_RG_WR_TRI_ADDR                                            MT6336_PMIC_WDT_CON0
#define MT6336_RG_WR_TRI_MASK                                            0x1
#define MT6336_RG_WR_TRI_SHIFT                                           4
#define MT6336_RG_DIS_WDT_SUSPEND_ADDR                                   MT6336_PMIC_WDT_CON1
#define MT6336_RG_DIS_WDT_SUSPEND_MASK                                   0x1
#define MT6336_RG_DIS_WDT_SUSPEND_SHIFT                                  0
#define MT6336_RG_OTG_TOLP_ON_ADDR                                       MT6336_PMIC_DB_WRAPPER_CON0
#define MT6336_RG_OTG_TOLP_ON_MASK                                       0x3
#define MT6336_RG_OTG_TOLP_ON_SHIFT                                      0
#define MT6336_RG_TDEG_ITERM_ADDR                                        MT6336_PMIC_DB_WRAPPER_CON0
#define MT6336_RG_TDEG_ITERM_MASK                                        0x3
#define MT6336_RG_TDEG_ITERM_SHIFT                                       2
#define MT6336_RG_ICL_ADDR                                               MT6336_PMIC_ICL_CON0
#define MT6336_RG_ICL_MASK                                               0x3F
#define MT6336_RG_ICL_SHIFT                                              0
#define MT6336_RG_TSTEP_ICL_ADDR                                         MT6336_PMIC_ICL_CON0
#define MT6336_RG_TSTEP_ICL_MASK                                         0x3
#define MT6336_RG_TSTEP_ICL_SHIFT                                        6
#define MT6336_RG_EN_ICL150PIN_ADDR                                      MT6336_PMIC_ICL_CON1
#define MT6336_RG_EN_ICL150PIN_MASK                                      0x1
#define MT6336_RG_EN_ICL150PIN_SHIFT                                     0
#define MT6336_RG_TSTEP_AICC_ADDR                                        MT6336_PMIC_ICL_CON1
#define MT6336_RG_TSTEP_AICC_MASK                                        0x1
#define MT6336_RG_TSTEP_AICC_SHIFT                                       1
#define MT6336_RG_EN_AICC_ADDR                                           MT6336_PMIC_ICL_CON1
#define MT6336_RG_EN_AICC_MASK                                           0x1
#define MT6336_RG_EN_AICC_SHIFT                                          2
#define MT6336_RG_TSTEP_THR_ADDR                                         MT6336_PMIC_ICL_CON1
#define MT6336_RG_TSTEP_THR_MASK                                         0x1
#define MT6336_RG_TSTEP_THR_SHIFT                                        3
#define MT6336_RG_EN_DIG_THR_GM_ADDR                                     MT6336_PMIC_ICL_CON1
#define MT6336_RG_EN_DIG_THR_GM_MASK                                     0x1
#define MT6336_RG_EN_DIG_THR_GM_SHIFT                                    4
#define MT6336_RG_EN_IBACKBOOST_ADDR                                     MT6336_PMIC_BACK_BOOST_CON0
#define MT6336_RG_EN_IBACKBOOST_MASK                                     0x1
#define MT6336_RG_EN_IBACKBOOST_SHIFT                                    0
#define MT6336_RG_FREQ_SEL_ADDR                                          MT6336_PMIC_SFSTR_CLK_CON0
#define MT6336_RG_FREQ_SEL_MASK                                          0x1
#define MT6336_RG_FREQ_SEL_SHIFT                                         0
#define MT6336_RG_SSTIME_SEL_ADDR                                        MT6336_PMIC_SFSTR_CLK_CON0
#define MT6336_RG_SSTIME_SEL_MASK                                        0x1
#define MT6336_RG_SSTIME_SEL_SHIFT                                       1
#define MT6336_RG_EN_SSBYPASS_ADDR                                       MT6336_PMIC_SFSTR_CLK_CON0
#define MT6336_RG_EN_SSBYPASS_MASK                                       0x1
#define MT6336_RG_EN_SSBYPASS_SHIFT                                      2
#define MT6336_RG_EN_CHR_SAFETMR_ADDR                                    MT6336_PMIC_SAFE_TIMER_CON0
#define MT6336_RG_EN_CHR_SAFETMR_MASK                                    0x1
#define MT6336_RG_EN_CHR_SAFETMR_SHIFT                                   0
#define MT6336_RG_EN_CHR_SAFETMRX2_ADDR                                  MT6336_PMIC_SAFE_TIMER_CON0
#define MT6336_RG_EN_CHR_SAFETMRX2_MASK                                  0x1
#define MT6336_RG_EN_CHR_SAFETMRX2_SHIFT                                 1
#define MT6336_RG_PAUSE_CHR_SAFETMR_ADDR                                 MT6336_PMIC_SAFE_TIMER_CON0
#define MT6336_RG_PAUSE_CHR_SAFETMR_MASK                                 0x1
#define MT6336_RG_PAUSE_CHR_SAFETMR_SHIFT                                2
#define MT6336_RG_CHR_SAFETMR_PRECC1_ADDR                                MT6336_PMIC_SAFE_TIMER_CON0
#define MT6336_RG_CHR_SAFETMR_PRECC1_MASK                                0x1
#define MT6336_RG_CHR_SAFETMR_PRECC1_SHIFT                               3
#define MT6336_RG_CHR_SAFETMR_FASTCC_ADDR                                MT6336_PMIC_SAFE_TIMER_CON0
#define MT6336_RG_CHR_SAFETMR_FASTCC_MASK                                0x3
#define MT6336_RG_CHR_SAFETMR_FASTCC_SHIFT                               4
#define MT6336_RG_CHR_SAFETMR_CLEAR_ADDR                                 MT6336_PMIC_SAFE_TIMER_CON0
#define MT6336_RG_CHR_SAFETMR_CLEAR_MASK                                 0x1
#define MT6336_RG_CHR_SAFETMR_CLEAR_SHIFT                                7
#define MT6336_RG_CK_SFSTR_DIV_SEL_ADDR                                  MT6336_PMIC_SFSTR_CLK_CON1
#define MT6336_RG_CK_SFSTR_DIV_SEL_MASK                                  0x1
#define MT6336_RG_CK_SFSTR_DIV_SEL_SHIFT                                 0
#define MT6336_RG_OTP_PA_ADDR                                            MT6336_PMIC_OTP_CON0
#define MT6336_RG_OTP_PA_MASK                                            0x3F
#define MT6336_RG_OTP_PA_SHIFT                                           0
#define MT6336_RG_OTP_PDIN_ADDR                                          MT6336_PMIC_OTP_CON1
#define MT6336_RG_OTP_PDIN_MASK                                          0xFF
#define MT6336_RG_OTP_PDIN_SHIFT                                         0
#define MT6336_RG_OTP_PTM_ADDR                                           MT6336_PMIC_OTP_CON2
#define MT6336_RG_OTP_PTM_MASK                                           0x3
#define MT6336_RG_OTP_PTM_SHIFT                                          0
#define MT6336_RG_OTP_PWE_ADDR                                           MT6336_PMIC_OTP_CON3
#define MT6336_RG_OTP_PWE_MASK                                           0x3
#define MT6336_RG_OTP_PWE_SHIFT                                          0
#define MT6336_RG_OTP_PPROG_ADDR                                         MT6336_PMIC_OTP_CON4
#define MT6336_RG_OTP_PPROG_MASK                                         0x1
#define MT6336_RG_OTP_PPROG_SHIFT                                        0
#define MT6336_RG_OTP_PWE_SRC_ADDR                                       MT6336_PMIC_OTP_CON5
#define MT6336_RG_OTP_PWE_SRC_MASK                                       0x1
#define MT6336_RG_OTP_PWE_SRC_SHIFT                                      0
#define MT6336_RG_OTP_PROG_PKEY_ADDR                                     MT6336_PMIC_OTP_CON6
#define MT6336_RG_OTP_PROG_PKEY_MASK                                     0xFF
#define MT6336_RG_OTP_PROG_PKEY_SHIFT                                    0
#define MT6336_RG_OTP_RD_PKEY_ADDR                                       MT6336_PMIC_OTP_CON7
#define MT6336_RG_OTP_RD_PKEY_MASK                                       0xFF
#define MT6336_RG_OTP_RD_PKEY_SHIFT                                      0
#define MT6336_RG_OTP_RD_TRIG_ADDR                                       MT6336_PMIC_OTP_CON8
#define MT6336_RG_OTP_RD_TRIG_MASK                                       0x1
#define MT6336_RG_OTP_RD_TRIG_SHIFT                                      0
#define MT6336_RG_RD_RDY_BYPASS_ADDR                                     MT6336_PMIC_OTP_CON9
#define MT6336_RG_RD_RDY_BYPASS_MASK                                     0x1
#define MT6336_RG_RD_RDY_BYPASS_SHIFT                                    0
#define MT6336_RG_SKIP_OTP_OUT_ADDR                                      MT6336_PMIC_OTP_CON10
#define MT6336_RG_SKIP_OTP_OUT_MASK                                      0x1
#define MT6336_RG_SKIP_OTP_OUT_SHIFT                                     0
#define MT6336_RG_OTP_RD_SW_ADDR                                         MT6336_PMIC_OTP_CON11
#define MT6336_RG_OTP_RD_SW_MASK                                         0x1
#define MT6336_RG_OTP_RD_SW_SHIFT                                        0
#define MT6336_RG_OTP_DOUT_SW_ADDR                                       MT6336_PMIC_OTP_CON12
#define MT6336_RG_OTP_DOUT_SW_MASK                                       0xFF
#define MT6336_RG_OTP_DOUT_SW_SHIFT                                      0
#define MT6336_RG_OTP_RD_BUSY_ADDR                                       MT6336_PMIC_OTP_CON13
#define MT6336_RG_OTP_RD_BUSY_MASK                                       0x1
#define MT6336_RG_OTP_RD_BUSY_SHIFT                                      0
#define MT6336_RG_OTP_RD_ACK_ADDR                                        MT6336_PMIC_OTP_CON13
#define MT6336_RG_OTP_RD_ACK_MASK                                        0x1
#define MT6336_RG_OTP_RD_ACK_SHIFT                                       2
#define MT6336_RG_OTP_PA_SW_ADDR                                         MT6336_PMIC_OTP_CON14
#define MT6336_RG_OTP_PA_SW_MASK                                         0x1F
#define MT6336_RG_OTP_PA_SW_SHIFT                                        0
#define MT6336_RG_OTP_DOUT_0_7_ADDR                                      MT6336_PMIC_OTP_DOUT_0_7
#define MT6336_RG_OTP_DOUT_0_7_MASK                                      0xFF
#define MT6336_RG_OTP_DOUT_0_7_SHIFT                                     0
#define MT6336_RG_OTP_DOUT_8_15_ADDR                                     MT6336_PMIC_OTP_DOUT_8_15
#define MT6336_RG_OTP_DOUT_8_15_MASK                                     0xFF
#define MT6336_RG_OTP_DOUT_8_15_SHIFT                                    0
#define MT6336_RG_OTP_DOUT_16_23_ADDR                                    MT6336_PMIC_OTP_DOUT_16_23
#define MT6336_RG_OTP_DOUT_16_23_MASK                                    0xFF
#define MT6336_RG_OTP_DOUT_16_23_SHIFT                                   0
#define MT6336_RG_OTP_DOUT_24_31_ADDR                                    MT6336_PMIC_OTP_DOUT_24_31
#define MT6336_RG_OTP_DOUT_24_31_MASK                                    0xFF
#define MT6336_RG_OTP_DOUT_24_31_SHIFT                                   0
#define MT6336_RG_OTP_DOUT_32_39_ADDR                                    MT6336_PMIC_OTP_DOUT_32_39
#define MT6336_RG_OTP_DOUT_32_39_MASK                                    0xFF
#define MT6336_RG_OTP_DOUT_32_39_SHIFT                                   0
#define MT6336_RG_OTP_DOUT_40_47_ADDR                                    MT6336_PMIC_OTP_DOUT_40_47
#define MT6336_RG_OTP_DOUT_40_47_MASK                                    0xFF
#define MT6336_RG_OTP_DOUT_40_47_SHIFT                                   0
#define MT6336_RG_OTP_DOUT_48_55_ADDR                                    MT6336_PMIC_OTP_DOUT_48_55
#define MT6336_RG_OTP_DOUT_48_55_MASK                                    0xFF
#define MT6336_RG_OTP_DOUT_48_55_SHIFT                                   0
#define MT6336_RG_OTP_DOUT_56_63_ADDR                                    MT6336_PMIC_OTP_DOUT_56_63
#define MT6336_RG_OTP_DOUT_56_63_MASK                                    0xFF
#define MT6336_RG_OTP_DOUT_56_63_SHIFT                                   0
#define MT6336_RG_OTP_DOUT_64_71_ADDR                                    MT6336_PMIC_OTP_DOUT_64_71
#define MT6336_RG_OTP_DOUT_64_71_MASK                                    0xFF
#define MT6336_RG_OTP_DOUT_64_71_SHIFT                                   0
#define MT6336_RG_OTP_DOUT_72_79_ADDR                                    MT6336_PMIC_OTP_DOUT_72_79
#define MT6336_RG_OTP_DOUT_72_79_MASK                                    0xFF
#define MT6336_RG_OTP_DOUT_72_79_SHIFT                                   0
#define MT6336_RG_OTP_DOUT_80_87_ADDR                                    MT6336_PMIC_OTP_DOUT_80_87
#define MT6336_RG_OTP_DOUT_80_87_MASK                                    0xFF
#define MT6336_RG_OTP_DOUT_80_87_SHIFT                                   0
#define MT6336_RG_OTP_DOUT_88_95_ADDR                                    MT6336_PMIC_OTP_DOUT_88_95
#define MT6336_RG_OTP_DOUT_88_95_MASK                                    0xFF
#define MT6336_RG_OTP_DOUT_88_95_SHIFT                                   0
#define MT6336_RG_OTP_DOUT_96_103_ADDR                                   MT6336_PMIC_OTP_DOUT_96_103
#define MT6336_RG_OTP_DOUT_96_103_MASK                                   0xFF
#define MT6336_RG_OTP_DOUT_96_103_SHIFT                                  0
#define MT6336_RG_OTP_DOUT_104_111_ADDR                                  MT6336_PMIC_OTP_DOUT_104_111
#define MT6336_RG_OTP_DOUT_104_111_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_104_111_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_112_119_ADDR                                  MT6336_PMIC_OTP_DOUT_112_119
#define MT6336_RG_OTP_DOUT_112_119_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_112_119_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_120_127_ADDR                                  MT6336_PMIC_OTP_DOUT_120_127
#define MT6336_RG_OTP_DOUT_120_127_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_120_127_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_128_135_ADDR                                  MT6336_PMIC_OTP_DOUT_128_135
#define MT6336_RG_OTP_DOUT_128_135_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_128_135_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_136_143_ADDR                                  MT6336_PMIC_OTP_DOUT_136_143
#define MT6336_RG_OTP_DOUT_136_143_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_136_143_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_144_151_ADDR                                  MT6336_PMIC_OTP_DOUT_144_151
#define MT6336_RG_OTP_DOUT_144_151_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_144_151_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_152_159_ADDR                                  MT6336_PMIC_OTP_DOUT_152_159
#define MT6336_RG_OTP_DOUT_152_159_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_152_159_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_160_167_ADDR                                  MT6336_PMIC_OTP_DOUT_160_167
#define MT6336_RG_OTP_DOUT_160_167_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_160_167_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_168_175_ADDR                                  MT6336_PMIC_OTP_DOUT_168_175
#define MT6336_RG_OTP_DOUT_168_175_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_168_175_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_176_183_ADDR                                  MT6336_PMIC_OTP_DOUT_176_183
#define MT6336_RG_OTP_DOUT_176_183_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_176_183_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_184_191_ADDR                                  MT6336_PMIC_OTP_DOUT_184_191
#define MT6336_RG_OTP_DOUT_184_191_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_184_191_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_192_199_ADDR                                  MT6336_PMIC_OTP_DOUT_192_199
#define MT6336_RG_OTP_DOUT_192_199_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_192_199_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_200_207_ADDR                                  MT6336_PMIC_OTP_DOUT_200_207
#define MT6336_RG_OTP_DOUT_200_207_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_200_207_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_208_215_ADDR                                  MT6336_PMIC_OTP_DOUT_208_215
#define MT6336_RG_OTP_DOUT_208_215_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_208_215_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_216_223_ADDR                                  MT6336_PMIC_OTP_DOUT_216_223
#define MT6336_RG_OTP_DOUT_216_223_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_216_223_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_224_231_ADDR                                  MT6336_PMIC_OTP_DOUT_224_231
#define MT6336_RG_OTP_DOUT_224_231_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_224_231_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_232_239_ADDR                                  MT6336_PMIC_OTP_DOUT_232_239
#define MT6336_RG_OTP_DOUT_232_239_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_232_239_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_240_247_ADDR                                  MT6336_PMIC_OTP_DOUT_240_247
#define MT6336_RG_OTP_DOUT_240_247_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_240_247_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_248_255_ADDR                                  MT6336_PMIC_OTP_DOUT_248_255
#define MT6336_RG_OTP_DOUT_248_255_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_248_255_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_256_263_ADDR                                  MT6336_PMIC_OTP_DOUT_256_263
#define MT6336_RG_OTP_DOUT_256_263_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_256_263_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_264_271_ADDR                                  MT6336_PMIC_OTP_DOUT_264_271
#define MT6336_RG_OTP_DOUT_264_271_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_264_271_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_272_279_ADDR                                  MT6336_PMIC_OTP_DOUT_272_279
#define MT6336_RG_OTP_DOUT_272_279_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_272_279_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_280_287_ADDR                                  MT6336_PMIC_OTP_DOUT_280_287
#define MT6336_RG_OTP_DOUT_280_287_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_280_287_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_288_295_ADDR                                  MT6336_PMIC_OTP_DOUT_288_295
#define MT6336_RG_OTP_DOUT_288_295_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_288_295_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_296_303_ADDR                                  MT6336_PMIC_OTP_DOUT_296_303
#define MT6336_RG_OTP_DOUT_296_303_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_296_303_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_304_311_ADDR                                  MT6336_PMIC_OTP_DOUT_304_311
#define MT6336_RG_OTP_DOUT_304_311_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_304_311_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_312_319_ADDR                                  MT6336_PMIC_OTP_DOUT_312_319
#define MT6336_RG_OTP_DOUT_312_319_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_312_319_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_320_327_ADDR                                  MT6336_PMIC_OTP_DOUT_320_327
#define MT6336_RG_OTP_DOUT_320_327_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_320_327_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_328_335_ADDR                                  MT6336_PMIC_OTP_DOUT_328_335
#define MT6336_RG_OTP_DOUT_328_335_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_328_335_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_336_343_ADDR                                  MT6336_PMIC_OTP_DOUT_336_343
#define MT6336_RG_OTP_DOUT_336_343_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_336_343_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_344_351_ADDR                                  MT6336_PMIC_OTP_DOUT_344_351
#define MT6336_RG_OTP_DOUT_344_351_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_344_351_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_352_359_ADDR                                  MT6336_PMIC_OTP_DOUT_352_359
#define MT6336_RG_OTP_DOUT_352_359_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_352_359_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_360_367_ADDR                                  MT6336_PMIC_OTP_DOUT_360_367
#define MT6336_RG_OTP_DOUT_360_367_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_360_367_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_368_375_ADDR                                  MT6336_PMIC_OTP_DOUT_368_375
#define MT6336_RG_OTP_DOUT_368_375_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_368_375_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_376_383_ADDR                                  MT6336_PMIC_OTP_DOUT_376_383
#define MT6336_RG_OTP_DOUT_376_383_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_376_383_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_384_391_ADDR                                  MT6336_PMIC_OTP_DOUT_384_391
#define MT6336_RG_OTP_DOUT_384_391_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_384_391_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_392_399_ADDR                                  MT6336_PMIC_OTP_DOUT_392_399
#define MT6336_RG_OTP_DOUT_392_399_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_392_399_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_400_407_ADDR                                  MT6336_PMIC_OTP_DOUT_400_407
#define MT6336_RG_OTP_DOUT_400_407_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_400_407_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_408_415_ADDR                                  MT6336_PMIC_OTP_DOUT_408_415
#define MT6336_RG_OTP_DOUT_408_415_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_408_415_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_416_423_ADDR                                  MT6336_PMIC_OTP_DOUT_416_423
#define MT6336_RG_OTP_DOUT_416_423_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_416_423_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_424_431_ADDR                                  MT6336_PMIC_OTP_DOUT_424_431
#define MT6336_RG_OTP_DOUT_424_431_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_424_431_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_432_439_ADDR                                  MT6336_PMIC_OTP_DOUT_432_439
#define MT6336_RG_OTP_DOUT_432_439_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_432_439_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_440_447_ADDR                                  MT6336_PMIC_OTP_DOUT_440_447
#define MT6336_RG_OTP_DOUT_440_447_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_440_447_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_448_455_ADDR                                  MT6336_PMIC_OTP_DOUT_448_455
#define MT6336_RG_OTP_DOUT_448_455_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_448_455_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_456_463_ADDR                                  MT6336_PMIC_OTP_DOUT_456_463
#define MT6336_RG_OTP_DOUT_456_463_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_456_463_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_464_471_ADDR                                  MT6336_PMIC_OTP_DOUT_464_471
#define MT6336_RG_OTP_DOUT_464_471_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_464_471_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_472_479_ADDR                                  MT6336_PMIC_OTP_DOUT_472_479
#define MT6336_RG_OTP_DOUT_472_479_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_472_479_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_480_487_ADDR                                  MT6336_PMIC_OTP_DOUT_480_487
#define MT6336_RG_OTP_DOUT_480_487_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_480_487_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_488_495_ADDR                                  MT6336_PMIC_OTP_DOUT_488_495
#define MT6336_RG_OTP_DOUT_488_495_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_488_495_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_496_503_ADDR                                  MT6336_PMIC_OTP_DOUT_496_503
#define MT6336_RG_OTP_DOUT_496_503_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_496_503_SHIFT                                 0
#define MT6336_RG_OTP_DOUT_504_511_ADDR                                  MT6336_PMIC_OTP_DOUT_504_511
#define MT6336_RG_OTP_DOUT_504_511_MASK                                  0xFF
#define MT6336_RG_OTP_DOUT_504_511_SHIFT                                 0
#define MT6336_RG_OTP_VAL_0_7_ADDR                                       MT6336_PMIC_OTP_VAL_0_7
#define MT6336_RG_OTP_VAL_0_7_MASK                                       0xFF
#define MT6336_RG_OTP_VAL_0_7_SHIFT                                      0
#define MT6336_RG_OTP_VAL_8_15_ADDR                                      MT6336_PMIC_OTP_VAL_8_15
#define MT6336_RG_OTP_VAL_8_15_MASK                                      0xFF
#define MT6336_RG_OTP_VAL_8_15_SHIFT                                     0
#define MT6336_RG_OTP_VAL_16_23_ADDR                                     MT6336_PMIC_OTP_VAL_16_23
#define MT6336_RG_OTP_VAL_16_23_MASK                                     0xFF
#define MT6336_RG_OTP_VAL_16_23_SHIFT                                    0
#define MT6336_RG_OTP_VAL_24_31_ADDR                                     MT6336_PMIC_OTP_VAL_24_31
#define MT6336_RG_OTP_VAL_24_31_MASK                                     0xFF
#define MT6336_RG_OTP_VAL_24_31_SHIFT                                    0
#define MT6336_RG_OTP_VAL_32_39_ADDR                                     MT6336_PMIC_OTP_VAL_32_39
#define MT6336_RG_OTP_VAL_32_39_MASK                                     0xFF
#define MT6336_RG_OTP_VAL_32_39_SHIFT                                    0
#define MT6336_RG_OTP_VAL_40_47_ADDR                                     MT6336_PMIC_OTP_VAL_40_47
#define MT6336_RG_OTP_VAL_40_47_MASK                                     0xFF
#define MT6336_RG_OTP_VAL_40_47_SHIFT                                    0
#define MT6336_RG_OTP_VAL_48_55_ADDR                                     MT6336_PMIC_OTP_VAL_48_55
#define MT6336_RG_OTP_VAL_48_55_MASK                                     0xFF
#define MT6336_RG_OTP_VAL_48_55_SHIFT                                    0
#define MT6336_RG_OTP_VAL_56_63_ADDR                                     MT6336_PMIC_OTP_VAL_56_63
#define MT6336_RG_OTP_VAL_56_63_MASK                                     0xFF
#define MT6336_RG_OTP_VAL_56_63_SHIFT                                    0
#define MT6336_RG_OTP_VAL_64_71_ADDR                                     MT6336_PMIC_OTP_VAL_64_71
#define MT6336_RG_OTP_VAL_64_71_MASK                                     0xFF
#define MT6336_RG_OTP_VAL_64_71_SHIFT                                    0
#define MT6336_RG_OTP_VAL_72_79_ADDR                                     MT6336_PMIC_OTP_VAL_72_79
#define MT6336_RG_OTP_VAL_72_79_MASK                                     0xFF
#define MT6336_RG_OTP_VAL_72_79_SHIFT                                    0
#define MT6336_RG_OTP_VAL_80_87_ADDR                                     MT6336_PMIC_OTP_VAL_80_87
#define MT6336_RG_OTP_VAL_80_87_MASK                                     0xFF
#define MT6336_RG_OTP_VAL_80_87_SHIFT                                    0
#define MT6336_RG_OTP_VAL_88_95_ADDR                                     MT6336_PMIC_OTP_VAL_88_95
#define MT6336_RG_OTP_VAL_88_95_MASK                                     0xFF
#define MT6336_RG_OTP_VAL_88_95_SHIFT                                    0
#define MT6336_RG_OTP_VAL_96_103_ADDR                                    MT6336_PMIC_OTP_VAL_96_103
#define MT6336_RG_OTP_VAL_96_103_MASK                                    0xFF
#define MT6336_RG_OTP_VAL_96_103_SHIFT                                   0
#define MT6336_RG_OTP_VAL_104_111_ADDR                                   MT6336_PMIC_OTP_VAL_104_111
#define MT6336_RG_OTP_VAL_104_111_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_104_111_SHIFT                                  0
#define MT6336_RG_OTP_VAL_112_119_ADDR                                   MT6336_PMIC_OTP_VAL_112_119
#define MT6336_RG_OTP_VAL_112_119_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_112_119_SHIFT                                  0
#define MT6336_RG_OTP_VAL_120_127_ADDR                                   MT6336_PMIC_OTP_VAL_120_127
#define MT6336_RG_OTP_VAL_120_127_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_120_127_SHIFT                                  0
#define MT6336_RG_OTP_VAL_128_135_ADDR                                   MT6336_PMIC_OTP_VAL_128_135
#define MT6336_RG_OTP_VAL_128_135_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_128_135_SHIFT                                  0
#define MT6336_RG_OTP_VAL_136_143_ADDR                                   MT6336_PMIC_OTP_VAL_136_143
#define MT6336_RG_OTP_VAL_136_143_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_136_143_SHIFT                                  0
#define MT6336_RG_OTP_VAL_144_151_ADDR                                   MT6336_PMIC_OTP_VAL_144_151
#define MT6336_RG_OTP_VAL_144_151_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_144_151_SHIFT                                  0
#define MT6336_RG_OTP_VAL_152_159_ADDR                                   MT6336_PMIC_OTP_VAL_152_159
#define MT6336_RG_OTP_VAL_152_159_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_152_159_SHIFT                                  0
#define MT6336_RG_OTP_VAL_160_167_ADDR                                   MT6336_PMIC_OTP_VAL_160_167
#define MT6336_RG_OTP_VAL_160_167_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_160_167_SHIFT                                  0
#define MT6336_RG_OTP_VAL_168_175_ADDR                                   MT6336_PMIC_OTP_VAL_168_175
#define MT6336_RG_OTP_VAL_168_175_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_168_175_SHIFT                                  0
#define MT6336_RG_OTP_VAL_176_183_ADDR                                   MT6336_PMIC_OTP_VAL_176_183
#define MT6336_RG_OTP_VAL_176_183_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_176_183_SHIFT                                  0
#define MT6336_RG_OTP_VAL_184_191_ADDR                                   MT6336_PMIC_OTP_VAL_184_191
#define MT6336_RG_OTP_VAL_184_191_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_184_191_SHIFT                                  0
#define MT6336_RG_OTP_VAL_192_199_ADDR                                   MT6336_PMIC_OTP_VAL_192_199
#define MT6336_RG_OTP_VAL_192_199_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_192_199_SHIFT                                  0
#define MT6336_RG_OTP_VAL_200_207_ADDR                                   MT6336_PMIC_OTP_VAL_200_207
#define MT6336_RG_OTP_VAL_200_207_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_200_207_SHIFT                                  0
#define MT6336_RG_OTP_VAL_208_215_ADDR                                   MT6336_PMIC_OTP_VAL_208_215
#define MT6336_RG_OTP_VAL_208_215_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_208_215_SHIFT                                  0
#define MT6336_RG_OTP_VAL_216_223_ADDR                                   MT6336_PMIC_OTP_VAL_216_223
#define MT6336_RG_OTP_VAL_216_223_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_216_223_SHIFT                                  0
#define MT6336_RG_OTP_VAL_224_231_ADDR                                   MT6336_PMIC_OTP_VAL_224_231
#define MT6336_RG_OTP_VAL_224_231_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_224_231_SHIFT                                  0
#define MT6336_RG_OTP_VAL_232_239_ADDR                                   MT6336_PMIC_OTP_VAL_232_239
#define MT6336_RG_OTP_VAL_232_239_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_232_239_SHIFT                                  0
#define MT6336_RG_OTP_VAL_240_247_ADDR                                   MT6336_PMIC_OTP_VAL_240_247
#define MT6336_RG_OTP_VAL_240_247_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_240_247_SHIFT                                  0
#define MT6336_RG_OTP_VAL_248_255_ADDR                                   MT6336_PMIC_OTP_VAL_248_255
#define MT6336_RG_OTP_VAL_248_255_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_248_255_SHIFT                                  0
#define MT6336_RG_OTP_VAL_256_263_ADDR                                   MT6336_PMIC_OTP_VAL_256_263
#define MT6336_RG_OTP_VAL_256_263_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_256_263_SHIFT                                  0
#define MT6336_RG_OTP_VAL_264_271_ADDR                                   MT6336_PMIC_OTP_VAL_264_271
#define MT6336_RG_OTP_VAL_264_271_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_264_271_SHIFT                                  0
#define MT6336_RG_OTP_VAL_272_279_ADDR                                   MT6336_PMIC_OTP_VAL_272_279
#define MT6336_RG_OTP_VAL_272_279_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_272_279_SHIFT                                  0
#define MT6336_RG_OTP_VAL_280_287_ADDR                                   MT6336_PMIC_OTP_VAL_280_287
#define MT6336_RG_OTP_VAL_280_287_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_280_287_SHIFT                                  0
#define MT6336_RG_OTP_VAL_288_295_ADDR                                   MT6336_PMIC_OTP_VAL_288_295
#define MT6336_RG_OTP_VAL_288_295_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_288_295_SHIFT                                  0
#define MT6336_RG_OTP_VAL_296_303_ADDR                                   MT6336_PMIC_OTP_VAL_296_303
#define MT6336_RG_OTP_VAL_296_303_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_296_303_SHIFT                                  0
#define MT6336_RG_OTP_VAL_304_311_ADDR                                   MT6336_PMIC_OTP_VAL_304_311
#define MT6336_RG_OTP_VAL_304_311_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_304_311_SHIFT                                  0
#define MT6336_RG_OTP_VAL_312_319_ADDR                                   MT6336_PMIC_OTP_VAL_312_319
#define MT6336_RG_OTP_VAL_312_319_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_312_319_SHIFT                                  0
#define MT6336_RG_OTP_VAL_320_327_ADDR                                   MT6336_PMIC_OTP_VAL_320_327
#define MT6336_RG_OTP_VAL_320_327_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_320_327_SHIFT                                  0
#define MT6336_RG_OTP_VAL_328_335_ADDR                                   MT6336_PMIC_OTP_VAL_328_335
#define MT6336_RG_OTP_VAL_328_335_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_328_335_SHIFT                                  0
#define MT6336_RG_OTP_VAL_336_343_ADDR                                   MT6336_PMIC_OTP_VAL_336_343
#define MT6336_RG_OTP_VAL_336_343_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_336_343_SHIFT                                  0
#define MT6336_RG_OTP_VAL_344_351_ADDR                                   MT6336_PMIC_OTP_VAL_344_351
#define MT6336_RG_OTP_VAL_344_351_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_344_351_SHIFT                                  0
#define MT6336_RG_OTP_VAL_352_359_ADDR                                   MT6336_PMIC_OTP_VAL_352_359
#define MT6336_RG_OTP_VAL_352_359_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_352_359_SHIFT                                  0
#define MT6336_RG_OTP_VAL_360_367_ADDR                                   MT6336_PMIC_OTP_VAL_360_367
#define MT6336_RG_OTP_VAL_360_367_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_360_367_SHIFT                                  0
#define MT6336_RG_OTP_VAL_368_375_ADDR                                   MT6336_PMIC_OTP_VAL_368_375
#define MT6336_RG_OTP_VAL_368_375_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_368_375_SHIFT                                  0
#define MT6336_RG_OTP_VAL_376_383_ADDR                                   MT6336_PMIC_OTP_VAL_376_383
#define MT6336_RG_OTP_VAL_376_383_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_376_383_SHIFT                                  0
#define MT6336_RG_OTP_VAL_384_391_ADDR                                   MT6336_PMIC_OTP_VAL_384_391
#define MT6336_RG_OTP_VAL_384_391_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_384_391_SHIFT                                  0
#define MT6336_RG_OTP_VAL_392_399_ADDR                                   MT6336_PMIC_OTP_VAL_392_399
#define MT6336_RG_OTP_VAL_392_399_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_392_399_SHIFT                                  0
#define MT6336_RG_OTP_VAL_400_407_ADDR                                   MT6336_PMIC_OTP_VAL_400_407
#define MT6336_RG_OTP_VAL_400_407_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_400_407_SHIFT                                  0
#define MT6336_RG_OTP_VAL_408_415_ADDR                                   MT6336_PMIC_OTP_VAL_408_415
#define MT6336_RG_OTP_VAL_408_415_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_408_415_SHIFT                                  0
#define MT6336_RG_OTP_VAL_416_423_ADDR                                   MT6336_PMIC_OTP_VAL_416_423
#define MT6336_RG_OTP_VAL_416_423_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_416_423_SHIFT                                  0
#define MT6336_RG_OTP_VAL_424_431_ADDR                                   MT6336_PMIC_OTP_VAL_424_431
#define MT6336_RG_OTP_VAL_424_431_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_424_431_SHIFT                                  0
#define MT6336_RG_OTP_VAL_432_439_ADDR                                   MT6336_PMIC_OTP_VAL_432_439
#define MT6336_RG_OTP_VAL_432_439_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_432_439_SHIFT                                  0
#define MT6336_RG_OTP_VAL_440_447_ADDR                                   MT6336_PMIC_OTP_VAL_440_447
#define MT6336_RG_OTP_VAL_440_447_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_440_447_SHIFT                                  0
#define MT6336_RG_OTP_VAL_448_455_ADDR                                   MT6336_PMIC_OTP_VAL_448_455
#define MT6336_RG_OTP_VAL_448_455_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_448_455_SHIFT                                  0
#define MT6336_RG_OTP_VAL_456_463_ADDR                                   MT6336_PMIC_OTP_VAL_456_463
#define MT6336_RG_OTP_VAL_456_463_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_456_463_SHIFT                                  0
#define MT6336_RG_OTP_VAL_464_471_ADDR                                   MT6336_PMIC_OTP_VAL_464_471
#define MT6336_RG_OTP_VAL_464_471_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_464_471_SHIFT                                  0
#define MT6336_RG_OTP_VAL_472_479_ADDR                                   MT6336_PMIC_OTP_VAL_472_479
#define MT6336_RG_OTP_VAL_472_479_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_472_479_SHIFT                                  0
#define MT6336_RG_OTP_VAL_480_487_ADDR                                   MT6336_PMIC_OTP_VAL_480_487
#define MT6336_RG_OTP_VAL_480_487_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_480_487_SHIFT                                  0
#define MT6336_RG_OTP_VAL_488_495_ADDR                                   MT6336_PMIC_OTP_VAL_488_495
#define MT6336_RG_OTP_VAL_488_495_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_488_495_SHIFT                                  0
#define MT6336_RG_OTP_VAL_496_503_ADDR                                   MT6336_PMIC_OTP_VAL_496_503
#define MT6336_RG_OTP_VAL_496_503_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_496_503_SHIFT                                  0
#define MT6336_RG_OTP_VAL_504_511_ADDR                                   MT6336_PMIC_OTP_VAL_504_511
#define MT6336_RG_OTP_VAL_504_511_MASK                                   0xFF
#define MT6336_RG_OTP_VAL_504_511_SHIFT                                  0
#define MT6336_RG_A_VBUS_DIS_ITH_ADDR                                    MT6336_PMIC_CORE_ANA_CON0
#define MT6336_RG_A_VBUS_DIS_ITH_MASK                                    0x1
#define MT6336_RG_A_VBUS_DIS_ITH_SHIFT                                   0
#define MT6336_RG_A_EN_IRCOMP_ADDR                                       MT6336_PMIC_CORE_ANA_CON0
#define MT6336_RG_A_EN_IRCOMP_MASK                                       0x1
#define MT6336_RG_A_EN_IRCOMP_SHIFT                                      1
#define MT6336_RG_A_EN_OTP_TESTMODE_ADDR                                 MT6336_PMIC_CORE_ANA_CON1
#define MT6336_RG_A_EN_OTP_TESTMODE_MASK                                 0x1
#define MT6336_RG_A_EN_OTP_TESTMODE_SHIFT                                0
#define MT6336_RG_A_DIS_UG_ADDR                                          MT6336_PMIC_CORE_ANA_CON1
#define MT6336_RG_A_DIS_UG_MASK                                          0x1
#define MT6336_RG_A_DIS_UG_SHIFT                                         1
#define MT6336_RG_A_DIS_LG_ADDR                                          MT6336_PMIC_CORE_ANA_CON1
#define MT6336_RG_A_DIS_LG_MASK                                          0x1
#define MT6336_RG_A_DIS_LG_SHIFT                                         2
#define MT6336_RG_A_DIS_UGFLA_ADDR                                       MT6336_PMIC_CORE_ANA_CON1
#define MT6336_RG_A_DIS_UGFLA_MASK                                       0x1
#define MT6336_RG_A_DIS_UGFLA_SHIFT                                      3
#define MT6336_RG_A_BASE_CLK_TRIM_EN_ADDR                                MT6336_PMIC_CORE_ANA_CON2
#define MT6336_RG_A_BASE_CLK_TRIM_EN_MASK                                0x1
#define MT6336_RG_A_BASE_CLK_TRIM_EN_SHIFT                               0
#define MT6336_RG_A_BASE_CLK_TRIM_ADDR                                   MT6336_PMIC_CORE_ANA_CON2
#define MT6336_RG_A_BASE_CLK_TRIM_MASK                                   0x3F
#define MT6336_RG_A_BASE_CLK_TRIM_SHIFT                                  1
#define MT6336_RG_A_BGR_TRIM_EN_ADDR                                     MT6336_PMIC_CORE_ANA_CON3
#define MT6336_RG_A_BGR_TRIM_EN_MASK                                     0x1
#define MT6336_RG_A_BGR_TRIM_EN_SHIFT                                    0
#define MT6336_RG_A_BGR_TRIM_ADDR                                        MT6336_PMIC_CORE_ANA_CON3
#define MT6336_RG_A_BGR_TRIM_MASK                                        0x1F
#define MT6336_RG_A_BGR_TRIM_SHIFT                                       1
#define MT6336_RG_A_IVGEN_TRIM_EN_ADDR                                   MT6336_PMIC_CORE_ANA_CON4
#define MT6336_RG_A_IVGEN_TRIM_EN_MASK                                   0x1
#define MT6336_RG_A_IVGEN_TRIM_EN_SHIFT                                  0
#define MT6336_RG_A_IVGEN_TRIM_ADDR                                      MT6336_PMIC_CORE_ANA_CON4
#define MT6336_RG_A_IVGEN_TRIM_MASK                                      0x1F
#define MT6336_RG_A_IVGEN_TRIM_SHIFT                                     1
#define MT6336_RG_A_BGR_RSEL_ADDR                                        MT6336_PMIC_CORE_ANA_CON5
#define MT6336_RG_A_BGR_RSEL_MASK                                        0x7
#define MT6336_RG_A_BGR_RSEL_SHIFT                                       0
#define MT6336_RG_A_BGR_UNCHOP_ADDR                                      MT6336_PMIC_CORE_ANA_CON5
#define MT6336_RG_A_BGR_UNCHOP_MASK                                      0x1
#define MT6336_RG_A_BGR_UNCHOP_SHIFT                                     3
#define MT6336_RG_A_BGR_UNCHOP_PH_ADDR                                   MT6336_PMIC_CORE_ANA_CON5
#define MT6336_RG_A_BGR_UNCHOP_PH_MASK                                   0x1
#define MT6336_RG_A_BGR_UNCHOP_PH_SHIFT                                  4
#define MT6336_RG_A_BGR_TEST_RSTB_ADDR                                   MT6336_PMIC_CORE_ANA_CON5
#define MT6336_RG_A_BGR_TEST_RSTB_MASK                                   0x1
#define MT6336_RG_A_BGR_TEST_RSTB_SHIFT                                  5
#define MT6336_RG_A_BGR_TEST_EN_ADDR                                     MT6336_PMIC_CORE_ANA_CON5
#define MT6336_RG_A_BGR_TEST_EN_MASK                                     0x1
#define MT6336_RG_A_BGR_TEST_EN_SHIFT                                    6
#define MT6336_RG_A_VPREG_TRIM_ADDR                                      MT6336_PMIC_CORE_ANA_CON6
#define MT6336_RG_A_VPREG_TRIM_MASK                                      0xF
#define MT6336_RG_A_VPREG_TRIM_SHIFT                                     1
#define MT6336_RG_A_VPREG_TCTRIM_ADDR                                    MT6336_PMIC_CORE_ANA_CON7
#define MT6336_RG_A_VPREG_TCTRIM_MASK                                    0xF
#define MT6336_RG_A_VPREG_TCTRIM_SHIFT                                   0
#define MT6336_RG_A_EN_OTG_BVALID_ADDR                                   MT6336_PMIC_CORE_ANA_CON9
#define MT6336_RG_A_EN_OTG_BVALID_MASK                                   0x1
#define MT6336_RG_A_EN_OTG_BVALID_SHIFT                                  0
#define MT6336_RG_A_OTP_SEL_ADDR                                         MT6336_PMIC_CORE_ANA_CON9
#define MT6336_RG_A_OTP_SEL_MASK                                         0x3
#define MT6336_RG_A_OTP_SEL_SHIFT                                        3
#define MT6336_RG_A_OTP_TMODE_ADDR                                       MT6336_PMIC_CORE_ANA_CON9
#define MT6336_RG_A_OTP_TMODE_MASK                                       0x1
#define MT6336_RG_A_OTP_TMODE_SHIFT                                      5
#define MT6336_RG_A_OTP_VREF_BG_ADDR                                     MT6336_PMIC_CORE_ANA_CON10
#define MT6336_RG_A_OTP_VREF_BG_MASK                                     0x7
#define MT6336_RG_A_OTP_VREF_BG_SHIFT                                    0
#define MT6336_RG_A_OTP_DET_SEL_ADDR                                     MT6336_PMIC_CORE_ANA_CON10
#define MT6336_RG_A_OTP_DET_SEL_MASK                                     0x1
#define MT6336_RG_A_OTP_DET_SEL_SHIFT                                    3
#define MT6336_RG_A_NI_VTHR_POL_ADDR                                     MT6336_PMIC_CORE_ANA_CON10
#define MT6336_RG_A_NI_VTHR_POL_MASK                                     0x7
#define MT6336_RG_A_NI_VTHR_POL_SHIFT                                    4
#define MT6336_RG_A_NI_VADC18_VOSEL_ADDR                                 MT6336_PMIC_CORE_ANA_CON11
#define MT6336_RG_A_NI_VADC18_VOSEL_MASK                                 0x7
#define MT6336_RG_A_NI_VADC18_VOSEL_SHIFT                                0
#define MT6336_RG_A_NI_VLDO33_VOSEL_ADDR                                 MT6336_PMIC_CORE_ANA_CON11
#define MT6336_RG_A_NI_VLDO33_VOSEL_MASK                                 0x7
#define MT6336_RG_A_NI_VLDO33_VOSEL_SHIFT                                3
#define MT6336_RG_A_EN_VLDO33_ADDR                                       MT6336_PMIC_CORE_ANA_CON11
#define MT6336_RG_A_EN_VLDO33_MASK                                       0x1
#define MT6336_RG_A_EN_VLDO33_SHIFT                                      6
#define MT6336_RG_A_VLDO33_READY_TRIM_ADDR                               MT6336_PMIC_CORE_ANA_CON12
#define MT6336_RG_A_VLDO33_READY_TRIM_MASK                               0xF
#define MT6336_RG_A_VLDO33_READY_TRIM_SHIFT                              0
#define MT6336_RG_A_NI_VDIG18_VOSEL_ADDR                                 MT6336_PMIC_CORE_ANA_CON12
#define MT6336_RG_A_NI_VDIG18_VOSEL_MASK                                 0x7
#define MT6336_RG_A_NI_VDIG18_VOSEL_SHIFT                                4
#define MT6336_RG_A_EN_DIG18_LOWQ_ADDR                                   MT6336_PMIC_CORE_ANA_CON12
#define MT6336_RG_A_EN_DIG18_LOWQ_MASK                                   0x1
#define MT6336_RG_A_EN_DIG18_LOWQ_SHIFT                                  7
#define MT6336_RG_A_OSC_TRIM_ADDR                                        MT6336_PMIC_CORE_ANA_CON13
#define MT6336_RG_A_OSC_TRIM_MASK                                        0x3F
#define MT6336_RG_A_OSC_TRIM_SHIFT                                       0
#define MT6336_RG_A_VSYS_IDIS_ITH_ADDR                                   MT6336_PMIC_CORE_ANA_CON14
#define MT6336_RG_A_VSYS_IDIS_ITH_MASK                                   0x1
#define MT6336_RG_A_VSYS_IDIS_ITH_SHIFT                                  0
#define MT6336_RG_A_VBAT_IDIS_ITH_ADDR                                   MT6336_PMIC_CORE_ANA_CON14
#define MT6336_RG_A_VBAT_IDIS_ITH_MASK                                   0x3
#define MT6336_RG_A_VBAT_IDIS_ITH_SHIFT                                  1
#define MT6336_RG_A_EN_ITERM_ADDR                                        MT6336_PMIC_CORE_ANA_CON14
#define MT6336_RG_A_EN_ITERM_MASK                                        0x1
#define MT6336_RG_A_EN_ITERM_SHIFT                                       3
#define MT6336_RG_A_EN_BATOC_ADDR                                        MT6336_PMIC_CORE_ANA_CON14
#define MT6336_RG_A_EN_BATOC_MASK                                        0x1
#define MT6336_RG_A_EN_BATOC_SHIFT                                       4
#define MT6336_RG_A_HIQCP_CLKSEL_ADDR                                    MT6336_PMIC_CORE_ANA_CON14
#define MT6336_RG_A_HIQCP_CLKSEL_MASK                                    0x3
#define MT6336_RG_A_HIQCP_CLKSEL_SHIFT                                   5
#define MT6336_RG_A_PPFET_ATEST_SEL_ADDR                                 MT6336_PMIC_CORE_ANA_CON15
#define MT6336_RG_A_PPFET_ATEST_SEL_MASK                                 0x7
#define MT6336_RG_A_PPFET_ATEST_SEL_SHIFT                                0
#define MT6336_RG_A_PPFET_DTEST_SEL_ADDR                                 MT6336_PMIC_CORE_ANA_CON15
#define MT6336_RG_A_PPFET_DTEST_SEL_MASK                                 0x3
#define MT6336_RG_A_PPFET_DTEST_SEL_SHIFT                                3
#define MT6336_RG_A_EN_TRIM_IBATSNS_ADDR                                 MT6336_PMIC_CORE_ANA_CON15
#define MT6336_RG_A_EN_TRIM_IBATSNS_MASK                                 0x1
#define MT6336_RG_A_EN_TRIM_IBATSNS_SHIFT                                5
#define MT6336_RG_A_EN_IBATSNS_CALIB_ADDR                                MT6336_PMIC_CORE_ANA_CON16
#define MT6336_RG_A_EN_IBATSNS_CALIB_MASK                                0x1
#define MT6336_RG_A_EN_IBATSNS_CALIB_SHIFT                               0
#define MT6336_RG_A_TRIM_IBATSNS_ADDR                                    MT6336_PMIC_CORE_ANA_CON16
#define MT6336_RG_A_TRIM_IBATSNS_MASK                                    0x7F
#define MT6336_RG_A_TRIM_IBATSNS_SHIFT                                   1
#define MT6336_RG_A_IBAT_DAC_CALIB_ADDR                                  MT6336_PMIC_CORE_ANA_CON17
#define MT6336_RG_A_IBAT_DAC_CALIB_MASK                                  0x1
#define MT6336_RG_A_IBAT_DAC_CALIB_SHIFT                                 0
#define MT6336_RG_A_IBAT_DAC_TRIM_LO_ADDR                                MT6336_PMIC_CORE_ANA_CON17
#define MT6336_RG_A_IBAT_DAC_TRIM_LO_MASK                                0x1F
#define MT6336_RG_A_IBAT_DAC_TRIM_LO_SHIFT                               1
#define MT6336_RG_A_IBAT_DAC_TRIM_HI_ADDR                                MT6336_PMIC_CORE_ANA_CON18
#define MT6336_RG_A_IBAT_DAC_TRIM_HI_MASK                                0x1F
#define MT6336_RG_A_IBAT_DAC_TRIM_HI_SHIFT                               0
#define MT6336_RG_A_OFFSET_RES_SEL_ADDR                                  MT6336_PMIC_CORE_ANA_CON19
#define MT6336_RG_A_OFFSET_RES_SEL_MASK                                  0x1
#define MT6336_RG_A_OFFSET_RES_SEL_SHIFT                                 0
#define MT6336_RG_A_IPRECHG_TRIM_ADDR                                    MT6336_PMIC_CORE_ANA_CON19
#define MT6336_RG_A_IPRECHG_TRIM_MASK                                    0xF
#define MT6336_RG_A_IPRECHG_TRIM_SHIFT                                   1
#define MT6336_RG_A_TRIM_VD_ADDR                                         MT6336_PMIC_CORE_ANA_CON20
#define MT6336_RG_A_TRIM_VD_MASK                                         0xF
#define MT6336_RG_A_TRIM_VD_SHIFT                                        0
#define MT6336_RG_A_VD_TEST_ADDR                                         MT6336_PMIC_CORE_ANA_CON20
#define MT6336_RG_A_VD_TEST_MASK                                         0x1
#define MT6336_RG_A_VD_TEST_SHIFT                                        4
#define MT6336_RG_A_BATOC_TEST_ADDR                                      MT6336_PMIC_CORE_ANA_CON21
#define MT6336_RG_A_BATOC_TEST_MASK                                      0x1
#define MT6336_RG_A_BATOC_TEST_SHIFT                                     0
#define MT6336_RG_A_TRIM_BATOC_ADDR                                      MT6336_PMIC_CORE_ANA_CON21
#define MT6336_RG_A_TRIM_BATOC_MASK                                      0xF
#define MT6336_RG_A_TRIM_BATOC_SHIFT                                     1
#define MT6336_RG_A_FEN_CP_HIQ_ADDR                                      MT6336_PMIC_CORE_ANA_CON21
#define MT6336_RG_A_FEN_CP_HIQ_MASK                                      0x1
#define MT6336_RG_A_FEN_CP_HIQ_SHIFT                                     5
#define MT6336_RG_A_ASW_BIAS_EN_ADDR                                     MT6336_PMIC_CORE_ANA_CON22
#define MT6336_RG_A_ASW_BIAS_EN_MASK                                     0x1
#define MT6336_RG_A_ASW_BIAS_EN_SHIFT                                    0
#define MT6336_RG_A_SST_SLOW_SSTART_EN_ADDR                              MT6336_PMIC_CORE_ANA_CON22
#define MT6336_RG_A_SST_SLOW_SSTART_EN_MASK                              0x1
#define MT6336_RG_A_SST_SLOW_SSTART_EN_SHIFT                             1
#define MT6336_RG_A_SST_FORCE_NON_SST_EN_ADDR                            MT6336_PMIC_CORE_ANA_CON22
#define MT6336_RG_A_SST_FORCE_NON_SST_EN_MASK                            0x1
#define MT6336_RG_A_SST_FORCE_NON_SST_EN_SHIFT                           2
#define MT6336_RG_A_SST_RAMP_SEL_ADDR                                    MT6336_PMIC_CORE_ANA_CON22
#define MT6336_RG_A_SST_RAMP_SEL_MASK                                    0x1
#define MT6336_RG_A_SST_RAMP_SEL_SHIFT                                   3
#define MT6336_RG_A_OSC_CLK_SEL_ADDR                                     MT6336_PMIC_CORE_ANA_CON22
#define MT6336_RG_A_OSC_CLK_SEL_MASK                                     0x1
#define MT6336_RG_A_OSC_CLK_SEL_SHIFT                                    4
#define MT6336_RG_A_OTG_VM_UVLO_VTH_ADDR                                 MT6336_PMIC_CORE_ANA_CON22
#define MT6336_RG_A_OTG_VM_UVLO_VTH_MASK                                 0x1
#define MT6336_RG_A_OTG_VM_UVLO_VTH_SHIFT                                5
#define MT6336_RG_A_R_IRCOMP_ADDR                                        MT6336_PMIC_CORE_ANA_CON23
#define MT6336_RG_A_R_IRCOMP_MASK                                        0x7
#define MT6336_RG_A_R_IRCOMP_SHIFT                                       0
#define MT6336_RG_A_LOOP_CCEXTR_SEL_ADDR                                 MT6336_PMIC_CORE_ANA_CON24
#define MT6336_RG_A_LOOP_CCEXTR_SEL_MASK                                 0x1
#define MT6336_RG_A_LOOP_CCEXTR_SEL_SHIFT                                3
#define MT6336_RG_A_LOOP_CCREF_SEL_ICHIN_SW_ADDR                         MT6336_PMIC_CORE_ANA_CON24
#define MT6336_RG_A_LOOP_CCREF_SEL_ICHIN_SW_MASK                         0x1
#define MT6336_RG_A_LOOP_CCREF_SEL_ICHIN_SW_SHIFT                        4
#define MT6336_RG_A_LOOP_THR_SEL_ADDR                                    MT6336_PMIC_CORE_ANA_CON24
#define MT6336_RG_A_LOOP_THR_SEL_MASK                                    0x7
#define MT6336_RG_A_LOOP_THR_SEL_SHIFT                                   5
#define MT6336_RG_A_LOOP_CCREF_SEL_ICHIN_ADDR                            MT6336_PMIC_CORE_ANA_CON25
#define MT6336_RG_A_LOOP_CCREF_SEL_ICHIN_MASK                            0xF
#define MT6336_RG_A_LOOP_CCREF_SEL_ICHIN_SHIFT                           0
#define MT6336_RG_A_LOOP_CCREF_SEL_IBAT_SW_ADDR                          MT6336_PMIC_CORE_ANA_CON25
#define MT6336_RG_A_LOOP_CCREF_SEL_IBAT_SW_MASK                          0x1
#define MT6336_RG_A_LOOP_CCREF_SEL_IBAT_SW_SHIFT                         4
#define MT6336_RG_A_LOOP_CCREF_SEL_IBAT_ADDR                             MT6336_PMIC_CORE_ANA_CON25
#define MT6336_RG_A_LOOP_CCREF_SEL_IBAT_MASK                             0x7
#define MT6336_RG_A_LOOP_CCREF_SEL_IBAT_SHIFT                            5
#define MT6336_RG_A_LOOP_CLAMP_EN_ADDR                                   MT6336_PMIC_CORE_ANA_CON26
#define MT6336_RG_A_LOOP_CLAMP_EN_MASK                                   0x1
#define MT6336_RG_A_LOOP_CLAMP_EN_SHIFT                                  0
#define MT6336_RG_A_LOOP_GM_EN_ADDR                                      MT6336_PMIC_CORE_ANA_CON26
#define MT6336_RG_A_LOOP_GM_EN_MASK                                      0x3F
#define MT6336_RG_A_LOOP_GM_EN_SHIFT                                     1
#define MT6336_RG_A_LOOP_GM_TUNE_DPM_MSB_ADDR                            MT6336_PMIC_CORE_ANA_CON27
#define MT6336_RG_A_LOOP_GM_TUNE_DPM_MSB_MASK                            0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_DPM_MSB_SHIFT                           0
#define MT6336_RG_A_LOOP_GM_TUNE_DPM_LSB_ADDR                            MT6336_PMIC_CORE_ANA_CON28
#define MT6336_RG_A_LOOP_GM_TUNE_DPM_LSB_MASK                            0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_DPM_LSB_SHIFT                           0
#define MT6336_RG_A_LOOP_GM_TUNE_IBAT_MSB_ADDR                           MT6336_PMIC_CORE_ANA_CON29
#define MT6336_RG_A_LOOP_GM_TUNE_IBAT_MSB_MASK                           0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_IBAT_MSB_SHIFT                          0
#define MT6336_RG_A_LOOP_GM_TUNE_IBAT_LSB_ADDR                           MT6336_PMIC_CORE_ANA_CON30
#define MT6336_RG_A_LOOP_GM_TUNE_IBAT_LSB_MASK                           0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_IBAT_LSB_SHIFT                          0
#define MT6336_RG_A_LOOP_GM_TUNE_ICHIN_MSB_ADDR                          MT6336_PMIC_CORE_ANA_CON31
#define MT6336_RG_A_LOOP_GM_TUNE_ICHIN_MSB_MASK                          0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_ICHIN_MSB_SHIFT                         0
#define MT6336_RG_A_LOOP_GM_TUNE_ICHIN_LSB_ADDR                          MT6336_PMIC_CORE_ANA_CON32
#define MT6336_RG_A_LOOP_GM_TUNE_ICHIN_LSB_MASK                          0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_ICHIN_LSB_SHIFT                         0
#define MT6336_RG_A_LOOP_GM_TUNE_SYS_MSB_ADDR                            MT6336_PMIC_CORE_ANA_CON33
#define MT6336_RG_A_LOOP_GM_TUNE_SYS_MSB_MASK                            0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_SYS_MSB_SHIFT                           0
#define MT6336_RG_A_LOOP_GM_TUNE_SYS_LSB_ADDR                            MT6336_PMIC_CORE_ANA_CON34
#define MT6336_RG_A_LOOP_GM_TUNE_SYS_LSB_MASK                            0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_SYS_LSB_SHIFT                           0
#define MT6336_RG_A_LOOP_GM_TUNE_THR_MSB_ADDR                            MT6336_PMIC_CORE_ANA_CON35
#define MT6336_RG_A_LOOP_GM_TUNE_THR_MSB_MASK                            0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_THR_MSB_SHIFT                           0
#define MT6336_RG_A_LOOP_GM_TUNE_THR_LSB_ADDR                            MT6336_PMIC_CORE_ANA_CON36
#define MT6336_RG_A_LOOP_GM_TUNE_THR_LSB_MASK                            0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_THR_LSB_SHIFT                           0
#define MT6336_RG_A_LOOP_GM_TUNE_BOOST_MSB_ADDR                          MT6336_PMIC_CORE_ANA_CON37
#define MT6336_RG_A_LOOP_GM_TUNE_BOOST_MSB_MASK                          0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_BOOST_MSB_SHIFT                         0
#define MT6336_RG_A_LOOP_GM_TUNE_BOOST_LSB_ADDR                          MT6336_PMIC_CORE_ANA_CON38
#define MT6336_RG_A_LOOP_GM_TUNE_BOOST_LSB_MASK                          0xFF
#define MT6336_RG_A_LOOP_GM_TUNE_BOOST_LSB_SHIFT                         0
#define MT6336_RG_A_LOOP_ICS_ADDR                                        MT6336_PMIC_CORE_ANA_CON39
#define MT6336_RG_A_LOOP_ICS_MASK                                        0xF
#define MT6336_RG_A_LOOP_ICS_SHIFT                                       0
#define MT6336_RG_A_LOOP_CLAMPEH_ADDR                                    MT6336_PMIC_CORE_ANA_CON39
#define MT6336_RG_A_LOOP_CLAMPEH_MASK                                    0xF
#define MT6336_RG_A_LOOP_CLAMPEH_SHIFT                                   4
#define MT6336_RG_A_LOOP_RC_ADDR                                         MT6336_PMIC_CORE_ANA_CON40
#define MT6336_RG_A_LOOP_RC_MASK                                         0x7
#define MT6336_RG_A_LOOP_RC_SHIFT                                        0
#define MT6336_RG_A_LOOP_CC_ADDR                                         MT6336_PMIC_CORE_ANA_CON40
#define MT6336_RG_A_LOOP_CC_MASK                                         0x7
#define MT6336_RG_A_LOOP_CC_SHIFT                                        3
#define MT6336_RG_A_LOOP_COMP_ADDR                                       MT6336_PMIC_CORE_ANA_CON41
#define MT6336_RG_A_LOOP_COMP_MASK                                       0xFF
#define MT6336_RG_A_LOOP_COMP_SHIFT                                      0
#define MT6336_RG_A_LOOP_GM_RSV_MSB_ADDR                                 MT6336_PMIC_CORE_ANA_CON42
#define MT6336_RG_A_LOOP_GM_RSV_MSB_MASK                                 0xFF
#define MT6336_RG_A_LOOP_GM_RSV_MSB_SHIFT                                0
#define MT6336_RG_A_LOOP_GM_RSV_LSB_ADDR                                 MT6336_PMIC_CORE_ANA_CON43
#define MT6336_RG_A_LOOP_GM_RSV_LSB_MASK                                 0xFF
#define MT6336_RG_A_LOOP_GM_RSV_LSB_SHIFT                                0
#define MT6336_RG_A_LOOP_RSV_TRIM_MSB_ADDR                               MT6336_PMIC_CORE_ANA_CON44
#define MT6336_RG_A_LOOP_RSV_TRIM_MSB_MASK                               0xFF
#define MT6336_RG_A_LOOP_RSV_TRIM_MSB_SHIFT                              0
#define MT6336_RG_A_LOOP_RSV_TRIM_LSB_ADDR                               MT6336_PMIC_CORE_ANA_CON45
#define MT6336_RG_A_LOOP_RSV_TRIM_LSB_MASK                               0xFF
#define MT6336_RG_A_LOOP_RSV_TRIM_LSB_SHIFT                              0
#define MT6336_RG_A_LOOP_SWCHR_CC_VREFTRIM_ADDR                          MT6336_PMIC_CORE_ANA_CON46
#define MT6336_RG_A_LOOP_SWCHR_CC_VREFTRIM_MASK                          0x3F
#define MT6336_RG_A_LOOP_SWCHR_CC_VREFTRIM_SHIFT                         0
#define MT6336_RG_A_LOOP_SWCHR_CV_VREFTRIM_ADDR                          MT6336_PMIC_CORE_ANA_CON47
#define MT6336_RG_A_LOOP_SWCHR_CV_VREFTRIM_MASK                          0x3F
#define MT6336_RG_A_LOOP_SWCHR_CV_VREFTRIM_SHIFT                         0
#define MT6336_RG_A_LOOP_SYS_DPM_VREFTRIM_ADDR                           MT6336_PMIC_CORE_ANA_CON48
#define MT6336_RG_A_LOOP_SYS_DPM_VREFTRIM_MASK                           0x3F
#define MT6336_RG_A_LOOP_SYS_DPM_VREFTRIM_SHIFT                          0
#define MT6336_RG_A_LOOP_DIS_PROT_ADDR                                   MT6336_PMIC_CORE_ANA_CON49
#define MT6336_RG_A_LOOP_DIS_PROT_MASK                                   0xFF
#define MT6336_RG_A_LOOP_DIS_PROT_SHIFT                                  0
#define MT6336_RG_A_LOOP_SWCHR_VBAT_PROT_ADDR                            MT6336_PMIC_CORE_ANA_CON50
#define MT6336_RG_A_LOOP_SWCHR_VBAT_PROT_MASK                            0x7
#define MT6336_RG_A_LOOP_SWCHR_VBAT_PROT_SHIFT                           0
#define MT6336_RG_A_LOOP_SWCHR_VSYS_PROT_ADDR                            MT6336_PMIC_CORE_ANA_CON50
#define MT6336_RG_A_LOOP_SWCHR_VSYS_PROT_MASK                            0x7
#define MT6336_RG_A_LOOP_SWCHR_VSYS_PROT_SHIFT                           3
#define MT6336_RG_A_LOOP_THR_ANA_EN_ADDR                                 MT6336_PMIC_CORE_ANA_CON50
#define MT6336_RG_A_LOOP_THR_ANA_EN_MASK                                 0x1
#define MT6336_RG_A_LOOP_THR_ANA_EN_SHIFT                                6
#define MT6336_RG_A_LOOP_USB_DL_MODE_ADDR                                MT6336_PMIC_CORE_ANA_CON50
#define MT6336_RG_A_LOOP_USB_DL_MODE_MASK                                0x1
#define MT6336_RG_A_LOOP_USB_DL_MODE_SHIFT                               7
#define MT6336_RG_A_MULTI_CAP_ADDR                                       MT6336_PMIC_CORE_ANA_CON51
#define MT6336_RG_A_MULTI_CAP_MASK                                       0x7
#define MT6336_RG_A_MULTI_CAP_SHIFT                                      4
#define MT6336_RG_A_LOOP_CS_ICL_TRIM250_ADDR                             MT6336_PMIC_CORE_ANA_CON52
#define MT6336_RG_A_LOOP_CS_ICL_TRIM250_MASK                             0x3F
#define MT6336_RG_A_LOOP_CS_ICL_TRIM250_SHIFT                            0
#define MT6336_RG_A_LOOP_CS_ICL_TRIM500_1_ADDR                           MT6336_PMIC_CORE_ANA_CON53
#define MT6336_RG_A_LOOP_CS_ICL_TRIM500_1_MASK                           0x3F
#define MT6336_RG_A_LOOP_CS_ICL_TRIM500_1_SHIFT                          0
#define MT6336_RG_A_LOOP_CS_ICL_TRIM500_2_ADDR                           MT6336_PMIC_CORE_ANA_CON54
#define MT6336_RG_A_LOOP_CS_ICL_TRIM500_2_MASK                           0x3F
#define MT6336_RG_A_LOOP_CS_ICL_TRIM500_2_SHIFT                          0
#define MT6336_RG_A_LOOP_CS_ICL_TRIM1000_ADDR                            MT6336_PMIC_CORE_ANA_CON55
#define MT6336_RG_A_LOOP_CS_ICL_TRIM1000_MASK                            0x3F
#define MT6336_RG_A_LOOP_CS_ICL_TRIM1000_SHIFT                           0
#define MT6336_RG_A_LOOP_CS_ICC_TRIM250_ADDR                             MT6336_PMIC_CORE_ANA_CON56
#define MT6336_RG_A_LOOP_CS_ICC_TRIM250_MASK                             0x3F
#define MT6336_RG_A_LOOP_CS_ICC_TRIM250_SHIFT                            0
#define MT6336_RG_A_LOOP_CS_ICC_TRIM500_ADDR                             MT6336_PMIC_CORE_ANA_CON57
#define MT6336_RG_A_LOOP_CS_ICC_TRIM500_MASK                             0x3F
#define MT6336_RG_A_LOOP_CS_ICC_TRIM500_SHIFT                            0
#define MT6336_RG_A_LOOP_CS_ICC_TRIM1000_ADDR                            MT6336_PMIC_CORE_ANA_CON58
#define MT6336_RG_A_LOOP_CS_ICC_TRIM1000_MASK                            0x3F
#define MT6336_RG_A_LOOP_CS_ICC_TRIM1000_SHIFT                           0
#define MT6336_RG_A_LOOP_100K_TRIM_ADDR                                  MT6336_PMIC_CORE_ANA_CON59
#define MT6336_RG_A_LOOP_100K_TRIM_MASK                                  0x1F
#define MT6336_RG_A_LOOP_100K_TRIM_SHIFT                                 0
#define MT6336_RG_A_LOOP_CCINTR_TRIM_EN_ADDR                             MT6336_PMIC_CORE_ANA_CON59
#define MT6336_RG_A_LOOP_CCINTR_TRIM_EN_MASK                             0x1
#define MT6336_RG_A_LOOP_CCINTR_TRIM_EN_SHIFT                            5
#define MT6336_RG_A_LOOP_100K_ICC_TRIM_ADDR                              MT6336_PMIC_CORE_ANA_CON60
#define MT6336_RG_A_LOOP_100K_ICC_TRIM_MASK                              0xFF
#define MT6336_RG_A_LOOP_100K_ICC_TRIM_SHIFT                             0
#define MT6336_RG_A_LOOP_100K_ICL_TRIM_ADDR                              MT6336_PMIC_CORE_ANA_CON61
#define MT6336_RG_A_LOOP_100K_ICL_TRIM_MASK                              0xFF
#define MT6336_RG_A_LOOP_100K_ICL_TRIM_SHIFT                             0
#define MT6336_RG_A_VRAMP_DCOS_ADDR                                      MT6336_PMIC_CORE_ANA_CON63
#define MT6336_RG_A_VRAMP_DCOS_MASK                                      0xF
#define MT6336_RG_A_VRAMP_DCOS_SHIFT                                     0
#define MT6336_RG_A_VRAMP_SLP_ADDR                                       MT6336_PMIC_CORE_ANA_CON63
#define MT6336_RG_A_VRAMP_SLP_MASK                                       0x7
#define MT6336_RG_A_VRAMP_SLP_SHIFT                                      4
#define MT6336_RG_A_VRAMP_SLP_RTUNE1_ADDR                                MT6336_PMIC_CORE_ANA_CON64
#define MT6336_RG_A_VRAMP_SLP_RTUNE1_MASK                                0x7
#define MT6336_RG_A_VRAMP_SLP_RTUNE1_SHIFT                               0
#define MT6336_RG_A_VRAMP_SLP_RTUNE2_ADDR                                MT6336_PMIC_CORE_ANA_CON64
#define MT6336_RG_A_VRAMP_SLP_RTUNE2_MASK                                0x7
#define MT6336_RG_A_VRAMP_SLP_RTUNE2_SHIFT                               3
#define MT6336_RG_A_VRAMP_VCS_RTUNE_ADDR                                 MT6336_PMIC_CORE_ANA_CON65
#define MT6336_RG_A_VRAMP_VCS_RTUNE_MASK                                 0xF
#define MT6336_RG_A_VRAMP_VCS_RTUNE_SHIFT                                0
#define MT6336_RG_A_PLIM_PWR_OCLIM_OFF_ADDR                              MT6336_PMIC_CORE_ANA_CON65
#define MT6336_RG_A_PLIM_PWR_OCLIM_OFF_MASK                              0x1
#define MT6336_RG_A_PLIM_PWR_OCLIM_OFF_SHIFT                             4
#define MT6336_RG_A_PLIM_PWR_OCLIMASYN_OFF_ADDR                          MT6336_PMIC_CORE_ANA_CON65
#define MT6336_RG_A_PLIM_PWR_OCLIMASYN_OFF_MASK                          0x1
#define MT6336_RG_A_PLIM_PWR_OCLIMASYN_OFF_SHIFT                         5
#define MT6336_RG_A_PLIM_CCEXTR_SEL_ADDR                                 MT6336_PMIC_CORE_ANA_CON65
#define MT6336_RG_A_PLIM_CCEXTR_SEL_MASK                                 0x1
#define MT6336_RG_A_PLIM_CCEXTR_SEL_SHIFT                                6
#define MT6336_RG_A_PLIM_OTG_OCTH_ADDR                                   MT6336_PMIC_CORE_ANA_CON66
#define MT6336_RG_A_PLIM_OTG_OCTH_MASK                                   0xF
#define MT6336_RG_A_PLIM_OTG_OCTH_SHIFT                                  0
#define MT6336_RG_A_PLIM_FLASH_OCTH_ADDR                                 MT6336_PMIC_CORE_ANA_CON66
#define MT6336_RG_A_PLIM_FLASH_OCTH_MASK                                 0xF
#define MT6336_RG_A_PLIM_FLASH_OCTH_SHIFT                                4
#define MT6336_RG_A_PLIM_SWCHR_OCTH_ADDR                                 MT6336_PMIC_CORE_ANA_CON67
#define MT6336_RG_A_PLIM_SWCHR_OCTH_MASK                                 0xF
#define MT6336_RG_A_PLIM_SWCHR_OCTH_SHIFT                                0
#define MT6336_RG_A_PLIM_SWCHR_ASYN_OCTH_ADDR                            MT6336_PMIC_CORE_ANA_CON67
#define MT6336_RG_A_PLIM_SWCHR_ASYN_OCTH_MASK                            0x3
#define MT6336_RG_A_PLIM_SWCHR_ASYN_OCTH_SHIFT                           4
#define MT6336_RG_A_PLIM_SWCHR_INTITH_ADDR                               MT6336_PMIC_CORE_ANA_CON68
#define MT6336_RG_A_PLIM_SWCHR_INTITH_MASK                               0xF
#define MT6336_RG_A_PLIM_SWCHR_INTITH_SHIFT                              0
#define MT6336_RG_A_PLIM_BOOST_INTITH_ADDR                               MT6336_PMIC_CORE_ANA_CON68
#define MT6336_RG_A_PLIM_BOOST_INTITH_MASK                               0xF
#define MT6336_RG_A_PLIM_BOOST_INTITH_SHIFT                              4
#define MT6336_RG_A_PLIM_SWCHR_OC_REFTRIM_ADDR                           MT6336_PMIC_CORE_ANA_CON69
#define MT6336_RG_A_PLIM_SWCHR_OC_REFTRIM_MASK                           0x1F
#define MT6336_RG_A_PLIM_SWCHR_OC_REFTRIM_SHIFT                          0
#define MT6336_RG_A_PLIM_SWCHR_ASYNOC_REFTRIM_ADDR                       MT6336_PMIC_CORE_ANA_CON70
#define MT6336_RG_A_PLIM_SWCHR_ASYNOC_REFTRIM_MASK                       0x1F
#define MT6336_RG_A_PLIM_SWCHR_ASYNOC_REFTRIM_SHIFT                      0
#define MT6336_RG_A_PLIM_BOOST_OC_REFTRIM_ADDR                           MT6336_PMIC_CORE_ANA_CON71
#define MT6336_RG_A_PLIM_BOOST_OC_REFTRIM_MASK                           0x1F
#define MT6336_RG_A_PLIM_BOOST_OC_REFTRIM_SHIFT                          0
#define MT6336_RG_A_LOGIC_BOOST_CLKSEL_ADDR                              MT6336_PMIC_CORE_ANA_CON71
#define MT6336_RG_A_LOGIC_BOOST_CLKSEL_MASK                              0x1
#define MT6336_RG_A_LOGIC_BOOST_CLKSEL_SHIFT                             5
#define MT6336_RG_A_LOGIC_BOOST_MAXDUTY_SEL_ADDR                         MT6336_PMIC_CORE_ANA_CON71
#define MT6336_RG_A_LOGIC_BOOST_MAXDUTY_SEL_MASK                         0x1
#define MT6336_RG_A_LOGIC_BOOST_MAXDUTY_SEL_SHIFT                        6
#define MT6336_RG_A_LOGIC_BOUND_ADDR                                     MT6336_PMIC_CORE_ANA_CON71
#define MT6336_RG_A_LOGIC_BOUND_MASK                                     0x1
#define MT6336_RG_A_LOGIC_BOUND_SHIFT                                    7
#define MT6336_RG_A_LOGIC_BURST_ADDR                                     MT6336_PMIC_CORE_ANA_CON72
#define MT6336_RG_A_LOGIC_BURST_MASK                                     0x3
#define MT6336_RG_A_LOGIC_BURST_SHIFT                                    0
#define MT6336_RG_A_LOGIC_DEL_TUNE1_ADDR                                 MT6336_PMIC_CORE_ANA_CON72
#define MT6336_RG_A_LOGIC_DEL_TUNE1_MASK                                 0x3
#define MT6336_RG_A_LOGIC_DEL_TUNE1_SHIFT                                2
#define MT6336_RG_A_LOGIC_DEL_TUNE2_ADDR                                 MT6336_PMIC_CORE_ANA_CON72
#define MT6336_RG_A_LOGIC_DEL_TUNE2_MASK                                 0x3
#define MT6336_RG_A_LOGIC_DEL_TUNE2_SHIFT                                4
#define MT6336_RG_A_LOGIC_DEL_TUNE3_ADDR                                 MT6336_PMIC_CORE_ANA_CON72
#define MT6336_RG_A_LOGIC_DEL_TUNE3_MASK                                 0x3
#define MT6336_RG_A_LOGIC_DEL_TUNE3_SHIFT                                6
#define MT6336_RG_A_LOGIC_DEL_TUNE4_ADDR                                 MT6336_PMIC_CORE_ANA_CON73
#define MT6336_RG_A_LOGIC_DEL_TUNE4_MASK                                 0x3
#define MT6336_RG_A_LOGIC_DEL_TUNE4_SHIFT                                0
#define MT6336_RG_A_LOGIC_ENPWM_PULSE_FEN_ADDR                           MT6336_PMIC_CORE_ANA_CON73
#define MT6336_RG_A_LOGIC_ENPWM_PULSE_FEN_MASK                           0x1
#define MT6336_RG_A_LOGIC_ENPWM_PULSE_FEN_SHIFT                          2
#define MT6336_RG_A_LOGIC_FORCE_NON_MBATPP_OC_ADDR                       MT6336_PMIC_CORE_ANA_CON73
#define MT6336_RG_A_LOGIC_FORCE_NON_MBATPP_OC_MASK                       0x1
#define MT6336_RG_A_LOGIC_FORCE_NON_MBATPP_OC_SHIFT                      3
#define MT6336_RG_A_LOGIC_FORCE_NON_SYS_OV_ADDR                          MT6336_PMIC_CORE_ANA_CON73
#define MT6336_RG_A_LOGIC_FORCE_NON_SYS_OV_MASK                          0x1
#define MT6336_RG_A_LOGIC_FORCE_NON_SYS_OV_SHIFT                         4
#define MT6336_RG_A_LOGIC_FORCE_NON_PLIM_ADDR                            MT6336_PMIC_CORE_ANA_CON73
#define MT6336_RG_A_LOGIC_FORCE_NON_PLIM_MASK                            0x1
#define MT6336_RG_A_LOGIC_FORCE_NON_PLIM_SHIFT                           5
#define MT6336_RG_A_LOGIC_FORCE_NON_ASYN_ADDR                            MT6336_PMIC_CORE_ANA_CON73
#define MT6336_RG_A_LOGIC_FORCE_NON_ASYN_MASK                            0x1
#define MT6336_RG_A_LOGIC_FORCE_NON_ASYN_SHIFT                           6
#define MT6336_RG_A_LOGIC_FREQ_HALF_ADDR                                 MT6336_PMIC_CORE_ANA_CON73
#define MT6336_RG_A_LOGIC_FREQ_HALF_MASK                                 0x1
#define MT6336_RG_A_LOGIC_FREQ_HALF_SHIFT                                7
#define MT6336_RG_A_LOGIC_GDRI_MINOFF_DIS_ADDR                           MT6336_PMIC_CORE_ANA_CON74
#define MT6336_RG_A_LOGIC_GDRI_MINOFF_DIS_MASK                           0x1
#define MT6336_RG_A_LOGIC_GDRI_MINOFF_DIS_SHIFT                          0
#define MT6336_RG_A_LOGIC_GDRI_MINOFF_SEL_ADDR                           MT6336_PMIC_CORE_ANA_CON74
#define MT6336_RG_A_LOGIC_GDRI_MINOFF_SEL_MASK                           0x1
#define MT6336_RG_A_LOGIC_GDRI_MINOFF_SEL_SHIFT                          1
#define MT6336_RG_A_LOGIC_SWCHR_FPWM_ADDR                                MT6336_PMIC_CORE_ANA_CON74
#define MT6336_RG_A_LOGIC_SWCHR_FPWM_MASK                                0x1
#define MT6336_RG_A_LOGIC_SWCHR_FPWM_SHIFT                               2
#define MT6336_RG_A_LOGIC_MINOFF_CKRST_ADDR                              MT6336_PMIC_CORE_ANA_CON74
#define MT6336_RG_A_LOGIC_MINOFF_CKRST_MASK                              0x3
#define MT6336_RG_A_LOGIC_MINOFF_CKRST_SHIFT                             3
#define MT6336_RG_A_LOGIC_RSV_TRIM_MSB_ADDR                              MT6336_PMIC_CORE_ANA_CON75
#define MT6336_RG_A_LOGIC_RSV_TRIM_MSB_MASK                              0xFF
#define MT6336_RG_A_LOGIC_RSV_TRIM_MSB_SHIFT                             0
#define MT6336_RG_A_LOGIC_RSV_TRIM_LSB_ADDR                              MT6336_PMIC_CORE_ANA_CON76
#define MT6336_RG_A_LOGIC_RSV_TRIM_LSB_MASK                              0xFF
#define MT6336_RG_A_LOGIC_RSV_TRIM_LSB_SHIFT                             0
#define MT6336_RG_A_BUSBAT_DIFF1_ADDR                                    MT6336_PMIC_CORE_ANA_CON79
#define MT6336_RG_A_BUSBAT_DIFF1_MASK                                    0x7
#define MT6336_RG_A_BUSBAT_DIFF1_SHIFT                                   0
#define MT6336_RG_A_BUSBAT_DIFF2_ADDR                                    MT6336_PMIC_CORE_ANA_CON79
#define MT6336_RG_A_BUSBAT_DIFF2_MASK                                    0x7
#define MT6336_RG_A_BUSBAT_DIFF2_SHIFT                                   3
#define MT6336_RG_A_ZC_SWCHR_ZX_TRIM_ADDR                                MT6336_PMIC_CORE_ANA_CON80
#define MT6336_RG_A_ZC_SWCHR_ZX_TRIM_MASK                                0x1F
#define MT6336_RG_A_ZC_SWCHR_ZX_TRIM_SHIFT                               0
#define MT6336_RG_A_ZC_SWCHR_ZX_TESTMODE_ADDR                            MT6336_PMIC_CORE_ANA_CON80
#define MT6336_RG_A_ZC_SWCHR_ZX_TESTMODE_MASK                            0x1
#define MT6336_RG_A_ZC_SWCHR_ZX_TESTMODE_SHIFT                           5
#define MT6336_RG_A_ZC_OTG_ZX_TRIM_ADDR                                  MT6336_PMIC_CORE_ANA_CON81
#define MT6336_RG_A_ZC_OTG_ZX_TRIM_MASK                                  0x1F
#define MT6336_RG_A_ZC_OTG_ZX_TRIM_SHIFT                                 0
#define MT6336_RG_A_ZC_OTG_ZX_TESTMODE_ADDR                              MT6336_PMIC_CORE_ANA_CON81
#define MT6336_RG_A_ZC_OTG_ZX_TESTMODE_MASK                              0x1
#define MT6336_RG_A_ZC_OTG_ZX_TESTMODE_SHIFT                             5
#define MT6336_RG_A_ZC_FLASH_ZX_TRIM_ADDR                                MT6336_PMIC_CORE_ANA_CON82
#define MT6336_RG_A_ZC_FLASH_ZX_TRIM_MASK                                0x1F
#define MT6336_RG_A_ZC_FLASH_ZX_TRIM_SHIFT                               0
#define MT6336_RG_A_ZC_FLASH_ZX_TESTMODE_ADDR                            MT6336_PMIC_CORE_ANA_CON82
#define MT6336_RG_A_ZC_FLASH_ZX_TESTMODE_MASK                            0x1
#define MT6336_RG_A_ZC_FLASH_ZX_TESTMODE_SHIFT                           5
#define MT6336_RG_A_PWR_VBSTOK_SEL_ADDR                                  MT6336_PMIC_CORE_ANA_CON82
#define MT6336_RG_A_PWR_VBSTOK_SEL_MASK                                  0x3
#define MT6336_RG_A_PWR_VBSTOK_SEL_SHIFT                                 6
#define MT6336_RG_A_PWR_LPD_ADDR                                         MT6336_PMIC_CORE_ANA_CON83
#define MT6336_RG_A_PWR_LPD_MASK                                         0x1
#define MT6336_RG_A_PWR_LPD_SHIFT                                        2
#define MT6336_RG_A_PWR_UGLG_ENPWM_FEN_ADDR                              MT6336_PMIC_CORE_ANA_CON83
#define MT6336_RG_A_PWR_UGLG_ENPWM_FEN_MASK                              0x1
#define MT6336_RG_A_PWR_UGLG_ENPWM_FEN_SHIFT                             3
#define MT6336_RG_A_PWR_RSV_MSB_ADDR                                     MT6336_PMIC_CORE_ANA_CON84
#define MT6336_RG_A_PWR_RSV_MSB_MASK                                     0xFF
#define MT6336_RG_A_PWR_RSV_MSB_SHIFT                                    0
#define MT6336_RG_A_PWR_RSV_LSB_ADDR                                     MT6336_PMIC_CORE_ANA_CON85
#define MT6336_RG_A_PWR_RSV_LSB_MASK                                     0xFF
#define MT6336_RG_A_PWR_RSV_LSB_SHIFT                                    0
#define MT6336_RG_A_PWR_UG_VTHSEL_ADDR                                   MT6336_PMIC_CORE_ANA_CON86
#define MT6336_RG_A_PWR_UG_VTHSEL_MASK                                   0x3
#define MT6336_RG_A_PWR_UG_VTHSEL_SHIFT                                  0
#define MT6336_RG_A_PWR_UG_SRC_ADDR                                      MT6336_PMIC_CORE_ANA_CON86
#define MT6336_RG_A_PWR_UG_SRC_MASK                                      0x3
#define MT6336_RG_A_PWR_UG_SRC_SHIFT                                     2
#define MT6336_RG_A_PWR_UG_SRCEH_ADDR                                    MT6336_PMIC_CORE_ANA_CON86
#define MT6336_RG_A_PWR_UG_SRCEH_MASK                                    0x3
#define MT6336_RG_A_PWR_UG_SRCEH_SHIFT                                   4
#define MT6336_RG_A_PWR_UG_DTC_ADDR                                      MT6336_PMIC_CORE_ANA_CON86
#define MT6336_RG_A_PWR_UG_DTC_MASK                                      0x3
#define MT6336_RG_A_PWR_UG_DTC_SHIFT                                     6
#define MT6336_RG_A_PWR_LG_VTHSEL_ADDR                                   MT6336_PMIC_CORE_ANA_CON87
#define MT6336_RG_A_PWR_LG_VTHSEL_MASK                                   0x3
#define MT6336_RG_A_PWR_LG_VTHSEL_SHIFT                                  0
#define MT6336_RG_A_PWR_LG_SRC_ADDR                                      MT6336_PMIC_CORE_ANA_CON87
#define MT6336_RG_A_PWR_LG_SRC_MASK                                      0x3
#define MT6336_RG_A_PWR_LG_SRC_SHIFT                                     2
#define MT6336_RG_A_PWR_LG_SRCEH_ADDR                                    MT6336_PMIC_CORE_ANA_CON87
#define MT6336_RG_A_PWR_LG_SRCEH_MASK                                    0x3
#define MT6336_RG_A_PWR_LG_SRCEH_SHIFT                                   4
#define MT6336_RG_A_PWR_LG_DTC_ADDR                                      MT6336_PMIC_CORE_ANA_CON87
#define MT6336_RG_A_PWR_LG_DTC_MASK                                      0x3
#define MT6336_RG_A_PWR_LG_DTC_SHIFT                                     6
#define MT6336_RG_A_REVFET_ICS_AZC_DIS_ADDR                              MT6336_PMIC_CORE_ANA_CON88
#define MT6336_RG_A_REVFET_ICS_AZC_DIS_MASK                              0x1
#define MT6336_RG_A_REVFET_ICS_AZC_DIS_SHIFT                             0
#define MT6336_RG_A_REVFET_ICS_CLKEXT_EN_ADDR                            MT6336_PMIC_CORE_ANA_CON88
#define MT6336_RG_A_REVFET_ICS_CLKEXT_EN_MASK                            0x1
#define MT6336_RG_A_REVFET_ICS_CLKEXT_EN_SHIFT                           1
#define MT6336_RG_A_REVFET_ICS_HFREQ_ADDR                                MT6336_PMIC_CORE_ANA_CON88
#define MT6336_RG_A_REVFET_ICS_HFREQ_MASK                                0x1
#define MT6336_RG_A_REVFET_ICS_HFREQ_SHIFT                               2
#define MT6336_RG_A_REVFET_DPH_AZC_DIS_ADDR                              MT6336_PMIC_CORE_ANA_CON88
#define MT6336_RG_A_REVFET_DPH_AZC_DIS_MASK                              0x1
#define MT6336_RG_A_REVFET_DPH_AZC_DIS_SHIFT                             3
#define MT6336_RG_A_REVFET_K_TRIM_MSB_ADDR                               MT6336_PMIC_CORE_ANA_CON89
#define MT6336_RG_A_REVFET_K_TRIM_MSB_MASK                               0xFF
#define MT6336_RG_A_REVFET_K_TRIM_MSB_SHIFT                              0
#define MT6336_RG_A_REVFET_K_TRIM_LSB_ADDR                               MT6336_PMIC_CORE_ANA_CON90
#define MT6336_RG_A_REVFET_K_TRIM_LSB_MASK                               0xFF
#define MT6336_RG_A_REVFET_K_TRIM_LSB_SHIFT                              0
#define MT6336_RG_A_REVFET_SEL_ADDR                                      MT6336_PMIC_CORE_ANA_CON91
#define MT6336_RG_A_REVFET_SEL_MASK                                      0x3
#define MT6336_RG_A_REVFET_SEL_SHIFT                                     0
#define MT6336_RG_A_IBAT_K_TRIM_MSB_ADDR                                 MT6336_PMIC_CORE_ANA_CON92
#define MT6336_RG_A_IBAT_K_TRIM_MSB_MASK                                 0xFF
#define MT6336_RG_A_IBAT_K_TRIM_MSB_SHIFT                                0
#define MT6336_RG_A_IBAT_K_TRIM_LSB_ADDR                                 MT6336_PMIC_CORE_ANA_CON93
#define MT6336_RG_A_IBAT_K_TRIM_LSB_MASK                                 0xFF
#define MT6336_RG_A_IBAT_K_TRIM_LSB_SHIFT                                0
#define MT6336_RG_A_SWCHR_RSV_TRIM_MSB_ADDR                              MT6336_PMIC_CORE_ANA_CON95
#define MT6336_RG_A_SWCHR_RSV_TRIM_MSB_MASK                              0xFF
#define MT6336_RG_A_SWCHR_RSV_TRIM_MSB_SHIFT                             0
#define MT6336_RG_A_SWCHR_RSV_TRIM_LSB_ADDR                              MT6336_PMIC_CORE_ANA_CON96
#define MT6336_RG_A_SWCHR_RSV_TRIM_LSB_MASK                              0xFF
#define MT6336_RG_A_SWCHR_RSV_TRIM_LSB_SHIFT                             0
#define MT6336_RG_A_FLASH1_TRIM_ADDR                                     MT6336_PMIC_CORE_ANA_CON99
#define MT6336_RG_A_FLASH1_TRIM_MASK                                     0x3F
#define MT6336_RG_A_FLASH1_TRIM_SHIFT                                    1
#define MT6336_RG_A_FLASH2_TRIM_ADDR                                     MT6336_PMIC_CORE_ANA_CON100
#define MT6336_RG_A_FLASH2_TRIM_MASK                                     0x3F
#define MT6336_RG_A_FLASH2_TRIM_SHIFT                                    1
#define MT6336_RG_A_FLASH_VCLAMP_SEL_ADDR                                MT6336_PMIC_CORE_ANA_CON101
#define MT6336_RG_A_FLASH_VCLAMP_SEL_MASK                                0x3
#define MT6336_RG_A_FLASH_VCLAMP_SEL_SHIFT                               0
#define MT6336_RG_A_IOS_DET_ITH_ADDR                                     MT6336_PMIC_CORE_ANA_CON101
#define MT6336_RG_A_IOS_DET_ITH_MASK                                     0x1
#define MT6336_RG_A_IOS_DET_ITH_SHIFT                                    2
#define MT6336_RG_A_SWCHR_ZCD_TRIM_EN_ADDR                               MT6336_PMIC_CORE_ANA_CON102
#define MT6336_RG_A_SWCHR_ZCD_TRIM_EN_MASK                               0x1
#define MT6336_RG_A_SWCHR_ZCD_TRIM_EN_SHIFT                              0
#define MT6336_RG_A_LOOP_FTR_DROP_EN_ADDR                                MT6336_PMIC_CORE_ANA_CON102
#define MT6336_RG_A_LOOP_FTR_DROP_EN_MASK                                0x1
#define MT6336_RG_A_LOOP_FTR_DROP_EN_SHIFT                               1
#define MT6336_RG_A_LOOP_FTR_SHOOT_EN_ADDR                               MT6336_PMIC_CORE_ANA_CON102
#define MT6336_RG_A_LOOP_FTR_SHOOT_EN_MASK                               0x1
#define MT6336_RG_A_LOOP_FTR_SHOOT_EN_SHIFT                              2
#define MT6336_RG_A_LOOP_FTR_DROP_ADDR                                   MT6336_PMIC_CORE_ANA_CON102
#define MT6336_RG_A_LOOP_FTR_DROP_MASK                                   0x7
#define MT6336_RG_A_LOOP_FTR_DROP_SHIFT                                  3
#define MT6336_RG_A_LOOP_FTR_RC_ADDR                                     MT6336_PMIC_CORE_ANA_CON102
#define MT6336_RG_A_LOOP_FTR_RC_MASK                                     0x3
#define MT6336_RG_A_LOOP_FTR_RC_SHIFT                                    6
#define MT6336_RG_A_LOOP_FTR_DELAY_ADDR                                  MT6336_PMIC_CORE_ANA_CON103
#define MT6336_RG_A_LOOP_FTR_DELAY_MASK                                  0x3
#define MT6336_RG_A_LOOP_FTR_DELAY_SHIFT                                 0
#define MT6336_RG_A_LOOP_FTR_DROP_MODE_ADDR                              MT6336_PMIC_CORE_ANA_CON103
#define MT6336_RG_A_LOOP_FTR_DROP_MODE_MASK                              0x1
#define MT6336_RG_A_LOOP_FTR_DROP_MODE_SHIFT                             2
#define MT6336_RG_A_LOOP_FTR_SHOOT_MODE_ADDR                             MT6336_PMIC_CORE_ANA_CON103
#define MT6336_RG_A_LOOP_FTR_SHOOT_MODE_MASK                             0x1
#define MT6336_RG_A_LOOP_FTR_SHOOT_MODE_SHIFT                            3
#define MT6336_RG_A_LOOP_FTR_DISCHARGE_EN_ADDR                           MT6336_PMIC_CORE_ANA_CON103
#define MT6336_RG_A_LOOP_FTR_DISCHARGE_EN_MASK                           0x1
#define MT6336_RG_A_LOOP_FTR_DISCHARGE_EN_SHIFT                          4
#define MT6336_RG_A_SWCHR_ZCD_TRIM_MODE_ADDR                             MT6336_PMIC_CORE_ANA_CON103
#define MT6336_RG_A_SWCHR_ZCD_TRIM_MODE_MASK                             0x1
#define MT6336_RG_A_SWCHR_ZCD_TRIM_MODE_SHIFT                            5
#define MT6336_RG_A_SWCHR_TESTMODE1_ADDR                                 MT6336_PMIC_CORE_ANA_CON104
#define MT6336_RG_A_SWCHR_TESTMODE1_MASK                                 0xFF
#define MT6336_RG_A_SWCHR_TESTMODE1_SHIFT                                0
#define MT6336_RG_A_SWCHR_TESTMODE2_ADDR                                 MT6336_PMIC_CORE_ANA_CON105
#define MT6336_RG_A_SWCHR_TESTMODE2_MASK                                 0xFF
#define MT6336_RG_A_SWCHR_TESTMODE2_SHIFT                                0
#define MT6336_RG_A_BASE_TESTMODE_ADDR                                   MT6336_PMIC_CORE_ANA_CON106
#define MT6336_RG_A_BASE_TESTMODE_MASK                                   0xFF
#define MT6336_RG_A_BASE_TESTMODE_SHIFT                                  0
#define MT6336_RG_A_FLA_TESTMODE_ADDR                                    MT6336_PMIC_CORE_ANA_CON107
#define MT6336_RG_A_FLA_TESTMODE_MASK                                    0xFF
#define MT6336_RG_A_FLA_TESTMODE_SHIFT                                   0
#define MT6336_RG_A_PPFET_TESTMODE_ADDR                                  MT6336_PMIC_CORE_ANA_CON108
#define MT6336_RG_A_PPFET_TESTMODE_MASK                                  0xFF
#define MT6336_RG_A_PPFET_TESTMODE_SHIFT                                 0
#define MT6336_RG_A_ANABASE_RSV_ADDR                                     MT6336_PMIC_CORE_ANA_CON109
#define MT6336_RG_A_ANABASE_RSV_MASK                                     0xFF
#define MT6336_RG_A_ANABASE_RSV_SHIFT                                    0
#define MT6336_RG_A_BC12_IBIAS_EN_ADDR                                   MT6336_PMIC_CORE_ANA_CON110
#define MT6336_RG_A_BC12_IBIAS_EN_MASK                                   0x1
#define MT6336_RG_A_BC12_IBIAS_EN_SHIFT                                  0
#define MT6336_RG_A_BC12_IPD_EN_ADDR                                     MT6336_PMIC_CORE_ANA_CON110
#define MT6336_RG_A_BC12_IPD_EN_MASK                                     0x3
#define MT6336_RG_A_BC12_IPD_EN_SHIFT                                    1
#define MT6336_RG_A_BC12_IPU_EN_ADDR                                     MT6336_PMIC_CORE_ANA_CON110
#define MT6336_RG_A_BC12_IPU_EN_MASK                                     0x3
#define MT6336_RG_A_BC12_IPU_EN_SHIFT                                    3
#define MT6336_RG_A_BC12_IPDC_EN_ADDR                                    MT6336_PMIC_CORE_ANA_CON110
#define MT6336_RG_A_BC12_IPDC_EN_MASK                                    0x3
#define MT6336_RG_A_BC12_IPDC_EN_SHIFT                                   5
#define MT6336_RG_A_BC12_VSRC_EN_ADDR                                    MT6336_PMIC_CORE_ANA_CON111
#define MT6336_RG_A_BC12_VSRC_EN_MASK                                    0x3
#define MT6336_RG_A_BC12_VSRC_EN_SHIFT                                   0
#define MT6336_RG_A_BC12_CMP_EN_ADDR                                     MT6336_PMIC_CORE_ANA_CON111
#define MT6336_RG_A_BC12_CMP_EN_MASK                                     0x3
#define MT6336_RG_A_BC12_CMP_EN_SHIFT                                    2
#define MT6336_RG_A_BC12_VREF_VTH_EN_ADDR                                MT6336_PMIC_CORE_ANA_CON111
#define MT6336_RG_A_BC12_VREF_VTH_EN_MASK                                0x3
#define MT6336_RG_A_BC12_VREF_VTH_EN_SHIFT                               4
#define MT6336_RG_A_BC12_IPD_HALF_EN_ADDR                                MT6336_PMIC_CORE_ANA_CON111
#define MT6336_RG_A_BC12_IPD_HALF_EN_MASK                                0x1
#define MT6336_RG_A_BC12_IPD_HALF_EN_SHIFT                               6
#define MT6336_RG_A_BC12_BB_CTRL_ADDR                                    MT6336_PMIC_CORE_ANA_CON111
#define MT6336_RG_A_BC12_BB_CTRL_MASK                                    0x1
#define MT6336_RG_A_BC12_BB_CTRL_SHIFT                                   7
#define MT6336_RG_A_DIS_ADC18_ADDR                                       MT6336_PMIC_CORE_ANA_CON112
#define MT6336_RG_A_DIS_ADC18_MASK                                       0x1
#define MT6336_RG_A_DIS_ADC18_SHIFT                                      0
#define MT6336_RG_A_SSCTRL_SEL_ADDR                                      MT6336_PMIC_CORE_ANA_CON112
#define MT6336_RG_A_SSCTRL_SEL_MASK                                      0x1
#define MT6336_RG_A_SSCTRL_SEL_SHIFT                                     1
#define MT6336_AUXADC_LBAT2_DEBT_MAX_ADDR                                MT6336_PMIC_AUXADC_LBAT2_1
#define MT6336_AUXADC_LBAT2_DEBT_MAX_MASK                                0xFF
#define MT6336_AUXADC_LBAT2_DEBT_MAX_SHIFT                               0
#define MT6336_AUXADC_LBAT2_DEBT_MIN_ADDR                                MT6336_PMIC_AUXADC_LBAT2_1_H
#define MT6336_AUXADC_LBAT2_DEBT_MIN_MASK                                0xFF
#define MT6336_AUXADC_LBAT2_DEBT_MIN_SHIFT                               0
#define MT6336_AUXADC_LBAT2_DET_PRD_15_0_L_ADDR                          MT6336_PMIC_AUXADC_LBAT2_2
#define MT6336_AUXADC_LBAT2_DET_PRD_15_0_L_MASK                          0xFF
#define MT6336_AUXADC_LBAT2_DET_PRD_15_0_L_SHIFT                         0
#define MT6336_AUXADC_LBAT2_DET_PRD_15_0_H_ADDR                          MT6336_PMIC_AUXADC_LBAT2_2_H
#define MT6336_AUXADC_LBAT2_DET_PRD_15_0_H_MASK                          0xFF
#define MT6336_AUXADC_LBAT2_DET_PRD_15_0_H_SHIFT                         0
#define MT6336_AUXADC_LBAT2_DET_PRD_19_16_ADDR                           MT6336_PMIC_AUXADC_LBAT2_3
#define MT6336_AUXADC_LBAT2_DET_PRD_19_16_MASK                           0xF
#define MT6336_AUXADC_LBAT2_DET_PRD_19_16_SHIFT                          0
#define MT6336_AUXADC_LBAT2_VOLT_MAX_L_ADDR                              MT6336_PMIC_AUXADC_LBAT2_4
#define MT6336_AUXADC_LBAT2_VOLT_MAX_L_MASK                              0xFF
#define MT6336_AUXADC_LBAT2_VOLT_MAX_L_SHIFT                             0
#define MT6336_AUXADC_LBAT2_VOLT_MAX_H_ADDR                              MT6336_PMIC_AUXADC_LBAT2_4_H
#define MT6336_AUXADC_LBAT2_VOLT_MAX_H_MASK                              0xF
#define MT6336_AUXADC_LBAT2_VOLT_MAX_H_SHIFT                             0
#define MT6336_AUXADC_LBAT2_IRQ_EN_MAX_ADDR                              MT6336_PMIC_AUXADC_LBAT2_4_H
#define MT6336_AUXADC_LBAT2_IRQ_EN_MAX_MASK                              0x1
#define MT6336_AUXADC_LBAT2_IRQ_EN_MAX_SHIFT                             4
#define MT6336_AUXADC_LBAT2_EN_MAX_ADDR                                  MT6336_PMIC_AUXADC_LBAT2_4_H
#define MT6336_AUXADC_LBAT2_EN_MAX_MASK                                  0x1
#define MT6336_AUXADC_LBAT2_EN_MAX_SHIFT                                 5
#define MT6336_AUXADC_LBAT2_MAX_IRQ_B_ADDR                               MT6336_PMIC_AUXADC_LBAT2_4_H
#define MT6336_AUXADC_LBAT2_MAX_IRQ_B_MASK                               0x1
#define MT6336_AUXADC_LBAT2_MAX_IRQ_B_SHIFT                              7
#define MT6336_AUXADC_LBAT2_VOLT_MIN_L_ADDR                              MT6336_PMIC_AUXADC_LBAT2_5
#define MT6336_AUXADC_LBAT2_VOLT_MIN_L_MASK                              0xFF
#define MT6336_AUXADC_LBAT2_VOLT_MIN_L_SHIFT                             0
#define MT6336_AUXADC_LBAT2_VOLT_MIN_H_ADDR                              MT6336_PMIC_AUXADC_LBAT2_5_H
#define MT6336_AUXADC_LBAT2_VOLT_MIN_H_MASK                              0xF
#define MT6336_AUXADC_LBAT2_VOLT_MIN_H_SHIFT                             0
#define MT6336_AUXADC_LBAT2_IRQ_EN_MIN_ADDR                              MT6336_PMIC_AUXADC_LBAT2_5_H
#define MT6336_AUXADC_LBAT2_IRQ_EN_MIN_MASK                              0x1
#define MT6336_AUXADC_LBAT2_IRQ_EN_MIN_SHIFT                             4
#define MT6336_AUXADC_LBAT2_EN_MIN_ADDR                                  MT6336_PMIC_AUXADC_LBAT2_5_H
#define MT6336_AUXADC_LBAT2_EN_MIN_MASK                                  0x1
#define MT6336_AUXADC_LBAT2_EN_MIN_SHIFT                                 5
#define MT6336_AUXADC_LBAT2_MIN_IRQ_B_ADDR                               MT6336_PMIC_AUXADC_LBAT2_5_H
#define MT6336_AUXADC_LBAT2_MIN_IRQ_B_MASK                               0x1
#define MT6336_AUXADC_LBAT2_MIN_IRQ_B_SHIFT                              7
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MAX_L_ADDR                    MT6336_PMIC_AUXADC_LBAT2_6
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MAX_L_MASK                    0xFF
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MAX_L_SHIFT                   0
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MAX_H_ADDR                    MT6336_PMIC_AUXADC_LBAT2_6_H
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MAX_H_MASK                    0x1
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MAX_H_SHIFT                   0
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN_L_ADDR                    MT6336_PMIC_AUXADC_LBAT2_7
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN_L_MASK                    0xFF
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN_L_SHIFT                   0
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN_H_ADDR                    MT6336_PMIC_AUXADC_LBAT2_7_H
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN_H_MASK                    0x1
#define MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN_H_SHIFT                   0
#define MT6336_AUXADC_JEITA_DEBT_ADDR                                    MT6336_PMIC_AUXADC_JEITA_0
#define MT6336_AUXADC_JEITA_DEBT_MASK                                    0xF
#define MT6336_AUXADC_JEITA_DEBT_SHIFT                                   0
#define MT6336_AUXADC_JEITA_MIPI_DIS_ADDR                                MT6336_PMIC_AUXADC_JEITA_0
#define MT6336_AUXADC_JEITA_MIPI_DIS_MASK                                0x1
#define MT6336_AUXADC_JEITA_MIPI_DIS_SHIFT                               4
#define MT6336_AUXADC_JEITA_FROZE_EN_ADDR                                MT6336_PMIC_AUXADC_JEITA_0
#define MT6336_AUXADC_JEITA_FROZE_EN_MASK                                0x1
#define MT6336_AUXADC_JEITA_FROZE_EN_SHIFT                               5
#define MT6336_AUXADC_JEITA_IRQ_EN_ADDR                                  MT6336_PMIC_AUXADC_JEITA_0_H
#define MT6336_AUXADC_JEITA_IRQ_EN_MASK                                  0x1
#define MT6336_AUXADC_JEITA_IRQ_EN_SHIFT                                 0
#define MT6336_AUXADC_JEITA_EN_ADDR                                      MT6336_PMIC_AUXADC_JEITA_0_H
#define MT6336_AUXADC_JEITA_EN_MASK                                      0x1
#define MT6336_AUXADC_JEITA_EN_SHIFT                                     1
#define MT6336_AUXADC_JEITA_DET_PRD_ADDR                                 MT6336_PMIC_AUXADC_JEITA_0_H
#define MT6336_AUXADC_JEITA_DET_PRD_MASK                                 0xF
#define MT6336_AUXADC_JEITA_DET_PRD_SHIFT                                2
#define MT6336_AUXADC_JEITA_VOLT_HOT_L_ADDR                              MT6336_PMIC_AUXADC_JEITA_1
#define MT6336_AUXADC_JEITA_VOLT_HOT_L_MASK                              0xFF
#define MT6336_AUXADC_JEITA_VOLT_HOT_L_SHIFT                             0
#define MT6336_AUXADC_JEITA_VOLT_HOT_H_ADDR                              MT6336_PMIC_AUXADC_JEITA_1_H
#define MT6336_AUXADC_JEITA_VOLT_HOT_H_MASK                              0xF
#define MT6336_AUXADC_JEITA_VOLT_HOT_H_SHIFT                             0
#define MT6336_AUXADC_JEITA_HOT_IRQ_ADDR                                 MT6336_PMIC_AUXADC_JEITA_1_H
#define MT6336_AUXADC_JEITA_HOT_IRQ_MASK                                 0x1
#define MT6336_AUXADC_JEITA_HOT_IRQ_SHIFT                                7
#define MT6336_AUXADC_JEITA_VOLT_WARM_L_ADDR                             MT6336_PMIC_AUXADC_JEITA_2
#define MT6336_AUXADC_JEITA_VOLT_WARM_L_MASK                             0xFF
#define MT6336_AUXADC_JEITA_VOLT_WARM_L_SHIFT                            0
#define MT6336_AUXADC_JEITA_VOLT_WARM_H_ADDR                             MT6336_PMIC_AUXADC_JEITA_2_H
#define MT6336_AUXADC_JEITA_VOLT_WARM_H_MASK                             0xF
#define MT6336_AUXADC_JEITA_VOLT_WARM_H_SHIFT                            0
#define MT6336_AUXADC_JEITA_WARM_IRQ_ADDR                                MT6336_PMIC_AUXADC_JEITA_2_H
#define MT6336_AUXADC_JEITA_WARM_IRQ_MASK                                0x1
#define MT6336_AUXADC_JEITA_WARM_IRQ_SHIFT                               7
#define MT6336_AUXADC_JEITA_VOLT_COOL_L_ADDR                             MT6336_PMIC_AUXADC_JEITA_3
#define MT6336_AUXADC_JEITA_VOLT_COOL_L_MASK                             0xFF
#define MT6336_AUXADC_JEITA_VOLT_COOL_L_SHIFT                            0
#define MT6336_AUXADC_JEITA_VOLT_COOL_H_ADDR                             MT6336_PMIC_AUXADC_JEITA_3_H
#define MT6336_AUXADC_JEITA_VOLT_COOL_H_MASK                             0xF
#define MT6336_AUXADC_JEITA_VOLT_COOL_H_SHIFT                            0
#define MT6336_AUXADC_JEITA_COOL_IRQ_ADDR                                MT6336_PMIC_AUXADC_JEITA_3_H
#define MT6336_AUXADC_JEITA_COOL_IRQ_MASK                                0x1
#define MT6336_AUXADC_JEITA_COOL_IRQ_SHIFT                               7
#define MT6336_AUXADC_JEITA_VOLT_COLD_L_ADDR                             MT6336_PMIC_AUXADC_JEITA_4
#define MT6336_AUXADC_JEITA_VOLT_COLD_L_MASK                             0xFF
#define MT6336_AUXADC_JEITA_VOLT_COLD_L_SHIFT                            0
#define MT6336_AUXADC_JEITA_VOLT_COLD_H_ADDR                             MT6336_PMIC_AUXADC_JEITA_4_H
#define MT6336_AUXADC_JEITA_VOLT_COLD_H_MASK                             0xF
#define MT6336_AUXADC_JEITA_VOLT_COLD_H_SHIFT                            0
#define MT6336_AUXADC_JEITA_COLD_IRQ_ADDR                                MT6336_PMIC_AUXADC_JEITA_4_H
#define MT6336_AUXADC_JEITA_COLD_IRQ_MASK                                0x1
#define MT6336_AUXADC_JEITA_COLD_IRQ_SHIFT                               7
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_WARM_ADDR                     MT6336_PMIC_AUXADC_JEITA_5
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_WARM_MASK                     0xF
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_WARM_SHIFT                    0
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_HOT_ADDR                      MT6336_PMIC_AUXADC_JEITA_5
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_HOT_MASK                      0xF
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_HOT_SHIFT                     4
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_COLD_ADDR                     MT6336_PMIC_AUXADC_JEITA_5_H
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_COLD_MASK                     0xF
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_COLD_SHIFT                    0
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_COOL_ADDR                     MT6336_PMIC_AUXADC_JEITA_5_H
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_COOL_MASK                     0xF
#define MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_COOL_SHIFT                    4
#define MT6336_AUXADC_NAG_EN_ADDR                                        MT6336_PMIC_AUXADC_NAG_0
#define MT6336_AUXADC_NAG_EN_MASK                                        0x1
#define MT6336_AUXADC_NAG_EN_SHIFT                                       0
#define MT6336_AUXADC_NAG_CLR_ADDR                                       MT6336_PMIC_AUXADC_NAG_0
#define MT6336_AUXADC_NAG_CLR_MASK                                       0x1
#define MT6336_AUXADC_NAG_CLR_SHIFT                                      1
#define MT6336_AUXADC_NAG_VBAT1_SEL_ADDR                                 MT6336_PMIC_AUXADC_NAG_0
#define MT6336_AUXADC_NAG_VBAT1_SEL_MASK                                 0x1
#define MT6336_AUXADC_NAG_VBAT1_SEL_SHIFT                                2
#define MT6336_AUXADC_NAG_IRQ_EN_ADDR                                    MT6336_PMIC_AUXADC_NAG_0
#define MT6336_AUXADC_NAG_IRQ_EN_MASK                                    0x1
#define MT6336_AUXADC_NAG_IRQ_EN_SHIFT                                   3
#define MT6336_AUXADC_NAG_C_DLTV_IRQ_ADDR                                MT6336_PMIC_AUXADC_NAG_0
#define MT6336_AUXADC_NAG_C_DLTV_IRQ_MASK                                0x1
#define MT6336_AUXADC_NAG_C_DLTV_IRQ_SHIFT                               4
#define MT6336_AUXADC_NAG_PRD_ADDR                                       MT6336_PMIC_AUXADC_NAG_0_H
#define MT6336_AUXADC_NAG_PRD_MASK                                       0x7F
#define MT6336_AUXADC_NAG_PRD_SHIFT                                      0
#define MT6336_AUXADC_NAG_ZCV_L_ADDR                                     MT6336_PMIC_AUXADC_NAG_1
#define MT6336_AUXADC_NAG_ZCV_L_MASK                                     0xFF
#define MT6336_AUXADC_NAG_ZCV_L_SHIFT                                    0
#define MT6336_AUXADC_NAG_ZCV_H_ADDR                                     MT6336_PMIC_AUXADC_NAG_1_H
#define MT6336_AUXADC_NAG_ZCV_H_MASK                                     0x7F
#define MT6336_AUXADC_NAG_ZCV_H_SHIFT                                    0
#define MT6336_AUXADC_NAG_C_DLTV_TH_15_0_L_ADDR                          MT6336_PMIC_AUXADC_NAG_2
#define MT6336_AUXADC_NAG_C_DLTV_TH_15_0_L_MASK                          0xFF
#define MT6336_AUXADC_NAG_C_DLTV_TH_15_0_L_SHIFT                         0
#define MT6336_AUXADC_NAG_C_DLTV_TH_15_0_H_ADDR                          MT6336_PMIC_AUXADC_NAG_2_H
#define MT6336_AUXADC_NAG_C_DLTV_TH_15_0_H_MASK                          0xFF
#define MT6336_AUXADC_NAG_C_DLTV_TH_15_0_H_SHIFT                         0
#define MT6336_AUXADC_NAG_C_DLTV_TH_26_16_L_ADDR                         MT6336_PMIC_AUXADC_NAG_3
#define MT6336_AUXADC_NAG_C_DLTV_TH_26_16_L_MASK                         0xFF
#define MT6336_AUXADC_NAG_C_DLTV_TH_26_16_L_SHIFT                        0
#define MT6336_AUXADC_NAG_C_DLTV_TH_26_16_H_ADDR                         MT6336_PMIC_AUXADC_NAG_3_H
#define MT6336_AUXADC_NAG_C_DLTV_TH_26_16_H_MASK                         0x7
#define MT6336_AUXADC_NAG_C_DLTV_TH_26_16_H_SHIFT                        0
#define MT6336_AUXADC_NAG_CNT_15_0_L_ADDR                                MT6336_PMIC_AUXADC_NAG_4
#define MT6336_AUXADC_NAG_CNT_15_0_L_MASK                                0xFF
#define MT6336_AUXADC_NAG_CNT_15_0_L_SHIFT                               0
#define MT6336_AUXADC_NAG_CNT_15_0_H_ADDR                                MT6336_PMIC_AUXADC_NAG_4_H
#define MT6336_AUXADC_NAG_CNT_15_0_H_MASK                                0xFF
#define MT6336_AUXADC_NAG_CNT_15_0_H_SHIFT                               0
#define MT6336_AUXADC_NAG_CNT_25_16_L_ADDR                               MT6336_PMIC_AUXADC_NAG_5
#define MT6336_AUXADC_NAG_CNT_25_16_L_MASK                               0xFF
#define MT6336_AUXADC_NAG_CNT_25_16_L_SHIFT                              0
#define MT6336_AUXADC_NAG_CNT_25_16_H_ADDR                               MT6336_PMIC_AUXADC_NAG_5_H
#define MT6336_AUXADC_NAG_CNT_25_16_H_MASK                               0x3
#define MT6336_AUXADC_NAG_CNT_25_16_H_SHIFT                              0
#define MT6336_AUXADC_NAG_DLTV_L_ADDR                                    MT6336_PMIC_AUXADC_NAG_6
#define MT6336_AUXADC_NAG_DLTV_L_MASK                                    0xFF
#define MT6336_AUXADC_NAG_DLTV_L_SHIFT                                   0
#define MT6336_AUXADC_NAG_DLTV_H_ADDR                                    MT6336_PMIC_AUXADC_NAG_6_H
#define MT6336_AUXADC_NAG_DLTV_H_MASK                                    0xFF
#define MT6336_AUXADC_NAG_DLTV_H_SHIFT                                   0
#define MT6336_AUXADC_NAG_C_DLTV_15_0_L_ADDR                             MT6336_PMIC_AUXADC_NAG_7
#define MT6336_AUXADC_NAG_C_DLTV_15_0_L_MASK                             0xFF
#define MT6336_AUXADC_NAG_C_DLTV_15_0_L_SHIFT                            0
#define MT6336_AUXADC_NAG_C_DLTV_15_0_H_ADDR                             MT6336_PMIC_AUXADC_NAG_7_H
#define MT6336_AUXADC_NAG_C_DLTV_15_0_H_MASK                             0xFF
#define MT6336_AUXADC_NAG_C_DLTV_15_0_H_SHIFT                            0
#define MT6336_AUXADC_NAG_C_DLTV_26_16_L_ADDR                            MT6336_PMIC_AUXADC_NAG_8
#define MT6336_AUXADC_NAG_C_DLTV_26_16_L_MASK                            0xFF
#define MT6336_AUXADC_NAG_C_DLTV_26_16_L_SHIFT                           0
#define MT6336_AUXADC_NAG_C_DLTV_26_16_H_ADDR                            MT6336_PMIC_AUXADC_NAG_8_H
#define MT6336_AUXADC_NAG_C_DLTV_26_16_H_MASK                            0x7
#define MT6336_AUXADC_NAG_C_DLTV_26_16_H_SHIFT                           0
#define MT6336_AUXADC_VBAT_DET_PRD_15_0_L_ADDR                           MT6336_PMIC_AUXADC_VBAT_0_L
#define MT6336_AUXADC_VBAT_DET_PRD_15_0_L_MASK                           0xFF
#define MT6336_AUXADC_VBAT_DET_PRD_15_0_L_SHIFT                          0
#define MT6336_AUXADC_VBAT_DET_PRD_15_0_H_ADDR                           MT6336_PMIC_AUXADC_VBAT_0_H
#define MT6336_AUXADC_VBAT_DET_PRD_15_0_H_MASK                           0xFF
#define MT6336_AUXADC_VBAT_DET_PRD_15_0_H_SHIFT                          0
#define MT6336_AUXADC_VBAT_DET_PRD_19_16_ADDR                            MT6336_PMIC_AUXADC_VBAT_1_L
#define MT6336_AUXADC_VBAT_DET_PRD_19_16_MASK                            0xF
#define MT6336_AUXADC_VBAT_DET_PRD_19_16_SHIFT                           0
#define MT6336_AUXADC_VBAT_EN_ADC_RECHG_ADDR                             MT6336_PMIC_AUXADC_VBAT_1_L
#define MT6336_AUXADC_VBAT_EN_ADC_RECHG_MASK                             0x1
#define MT6336_AUXADC_VBAT_EN_ADC_RECHG_SHIFT                            4
#define MT6336_AUXADC_VBAT_EN_MODE_SEL_ADDR                              MT6336_PMIC_AUXADC_VBAT_1_L
#define MT6336_AUXADC_VBAT_EN_MODE_SEL_MASK                              0x1
#define MT6336_AUXADC_VBAT_EN_MODE_SEL_SHIFT                             5
#define MT6336_AUXADC_VBAT_IRQ_EN_ADDR                                   MT6336_PMIC_AUXADC_VBAT_1_L
#define MT6336_AUXADC_VBAT_IRQ_EN_MASK                                   0x1
#define MT6336_AUXADC_VBAT_IRQ_EN_SHIFT                                  6
#define MT6336_AUXADC_VBAT_1_RSV_0_ADDR                                  MT6336_PMIC_AUXADC_VBAT_1_L
#define MT6336_AUXADC_VBAT_1_RSV_0_MASK                                  0x1
#define MT6336_AUXADC_VBAT_1_RSV_0_SHIFT                                 7
#define MT6336_AUXADC_VBAT_DET_DEBT_ADDR                                 MT6336_PMIC_AUXADC_VBAT_1_H
#define MT6336_AUXADC_VBAT_DET_DEBT_MASK                                 0xFF
#define MT6336_AUXADC_VBAT_DET_DEBT_SHIFT                                0
#define MT6336_AUXADC_VBAT_DET_VOLT_11_0_L_ADDR                          MT6336_PMIC_AUXADC_VBAT_2_L
#define MT6336_AUXADC_VBAT_DET_VOLT_11_0_L_MASK                          0xFF
#define MT6336_AUXADC_VBAT_DET_VOLT_11_0_L_SHIFT                         0
#define MT6336_AUXADC_VBAT_DET_VOLT_11_0_H_ADDR                          MT6336_PMIC_AUXADC_VBAT_2_H
#define MT6336_AUXADC_VBAT_DET_VOLT_11_0_H_MASK                          0xF
#define MT6336_AUXADC_VBAT_DET_VOLT_11_0_H_SHIFT                         0
#define MT6336_AUXADC_VBAT_2_RSV_0_ADDR                                  MT6336_PMIC_AUXADC_VBAT_2_H
#define MT6336_AUXADC_VBAT_2_RSV_0_MASK                                  0x7
#define MT6336_AUXADC_VBAT_2_RSV_0_SHIFT                                 4
#define MT6336_AUXADC_VBAT_VTH_MODE_SEL_ADDR                             MT6336_PMIC_AUXADC_VBAT_2_H
#define MT6336_AUXADC_VBAT_VTH_MODE_SEL_MASK                             0x1
#define MT6336_AUXADC_VBAT_VTH_MODE_SEL_SHIFT                            7
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_HIGH_8_0_L_ADDR                MT6336_PMIC_AUXADC_VBAT_3_L
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_HIGH_8_0_L_MASK                0xFF
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_HIGH_8_0_L_SHIFT               0
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_HIGH_8_0_H_ADDR                MT6336_PMIC_AUXADC_VBAT_3_H
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_HIGH_8_0_H_MASK                0x1
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_HIGH_8_0_H_SHIFT               0
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_LOW_8_0_L_ADDR                 MT6336_PMIC_AUXADC_VBAT_4_L
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_LOW_8_0_L_MASK                 0xFF
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_LOW_8_0_L_SHIFT                0
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_LOW_8_0_H_ADDR                 MT6336_PMIC_AUXADC_VBAT_4_H
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_LOW_8_0_H_MASK                 0x1
#define MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_LOW_8_0_H_SHIFT                0
#define MT6336_RG_AD_QI_VBAT_LT_V3P2_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF0
#define MT6336_RG_AD_QI_VBAT_LT_V3P2_MASK                                0x1
#define MT6336_RG_AD_QI_VBAT_LT_V3P2_SHIFT                               0
#define MT6336_RG_AD_QI_VBAT_LT_V2P7_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF0
#define MT6336_RG_AD_QI_VBAT_LT_V2P7_MASK                                0x1
#define MT6336_RG_AD_QI_VBAT_LT_V2P7_SHIFT                               1
#define MT6336_RG_AD_QI_OTG_BVALID_ADDR                                  MT6336_PMIC_ANA_CORE_AD_INTF0
#define MT6336_RG_AD_QI_OTG_BVALID_MASK                                  0x1
#define MT6336_RG_AD_QI_OTG_BVALID_SHIFT                                 2
#define MT6336_RG_AD_QI_PP_EN_IN_ADDR                                    MT6336_PMIC_ANA_CORE_AD_INTF0
#define MT6336_RG_AD_QI_PP_EN_IN_MASK                                    0x1
#define MT6336_RG_AD_QI_PP_EN_IN_SHIFT                                   3
#define MT6336_RG_AD_QI_ICL150PIN_LVL_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF0
#define MT6336_RG_AD_QI_ICL150PIN_LVL_MASK                               0x1
#define MT6336_RG_AD_QI_ICL150PIN_LVL_SHIFT                              4
#define MT6336_RG_AD_QI_TEMP_GT_150_ADDR                                 MT6336_PMIC_ANA_CORE_AD_INTF0
#define MT6336_RG_AD_QI_TEMP_GT_150_MASK                                 0x1
#define MT6336_RG_AD_QI_TEMP_GT_150_SHIFT                                5
#define MT6336_RG_AD_QI_VIO18_READY_ADDR                                 MT6336_PMIC_ANA_CORE_AD_INTF0
#define MT6336_RG_AD_QI_VIO18_READY_MASK                                 0x1
#define MT6336_RG_AD_QI_VIO18_READY_SHIFT                                6
#define MT6336_RG_AD_QI_VBGR_READY_ADDR                                  MT6336_PMIC_ANA_CORE_AD_INTF0
#define MT6336_RG_AD_QI_VBGR_READY_MASK                                  0x1
#define MT6336_RG_AD_QI_VBGR_READY_SHIFT                                 7
#define MT6336_RG_AD_QI_VLED2_OPEN_ADDR                                  MT6336_PMIC_ANA_CORE_AD_INTF1
#define MT6336_RG_AD_QI_VLED2_OPEN_MASK                                  0x1
#define MT6336_RG_AD_QI_VLED2_OPEN_SHIFT                                 0
#define MT6336_RG_AD_QI_VLED1_OPEN_ADDR                                  MT6336_PMIC_ANA_CORE_AD_INTF1
#define MT6336_RG_AD_QI_VLED1_OPEN_MASK                                  0x1
#define MT6336_RG_AD_QI_VLED1_OPEN_SHIFT                                 1
#define MT6336_RG_AD_QI_VLED2_SHORT_ADDR                                 MT6336_PMIC_ANA_CORE_AD_INTF1
#define MT6336_RG_AD_QI_VLED2_SHORT_MASK                                 0x1
#define MT6336_RG_AD_QI_VLED2_SHORT_SHIFT                                2
#define MT6336_RG_AD_QI_VLED1_SHORT_ADDR                                 MT6336_PMIC_ANA_CORE_AD_INTF1
#define MT6336_RG_AD_QI_VLED1_SHORT_MASK                                 0x1
#define MT6336_RG_AD_QI_VLED1_SHORT_SHIFT                                3
#define MT6336_RG_AD_QI_VUSB33_READY_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF1
#define MT6336_RG_AD_QI_VUSB33_READY_MASK                                0x1
#define MT6336_RG_AD_QI_VUSB33_READY_SHIFT                               4
#define MT6336_RG_AD_QI_VPREG_RDY_ADDR                                   MT6336_PMIC_ANA_CORE_AD_INTF1
#define MT6336_RG_AD_QI_VPREG_RDY_MASK                                   0x1
#define MT6336_RG_AD_QI_VPREG_RDY_SHIFT                                  5
#define MT6336_RG_AD_QI_VBUS_GT_POR_ADDR                                 MT6336_PMIC_ANA_CORE_AD_INTF1
#define MT6336_RG_AD_QI_VBUS_GT_POR_MASK                                 0x1
#define MT6336_RG_AD_QI_VBUS_GT_POR_SHIFT                                6
#define MT6336_RG_AD_QI_BC12_CMP_OUT_SW_ADDR                             MT6336_PMIC_ANA_CORE_AD_INTF2
#define MT6336_RG_AD_QI_BC12_CMP_OUT_SW_MASK                             0x1
#define MT6336_RG_AD_QI_BC12_CMP_OUT_SW_SHIFT                            0
#define MT6336_RG_AD_QI_BC12_CMP_OUT_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF2
#define MT6336_RG_AD_QI_BC12_CMP_OUT_MASK                                0x1
#define MT6336_RG_AD_QI_BC12_CMP_OUT_SHIFT                               1
#define MT6336_RG_AD_QI_MBATPP_DIS_OC_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF3
#define MT6336_RG_AD_QI_MBATPP_DIS_OC_MASK                               0x1
#define MT6336_RG_AD_QI_MBATPP_DIS_OC_SHIFT                              0
#define MT6336_RG_AD_NS_VPRECHG_FAIL_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF3
#define MT6336_RG_AD_NS_VPRECHG_FAIL_MASK                                0x1
#define MT6336_RG_AD_NS_VPRECHG_FAIL_SHIFT                               1
#define MT6336_RG_AD_NI_ICHR_LT_ITERM_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF3
#define MT6336_RG_AD_NI_ICHR_LT_ITERM_MASK                               0x1
#define MT6336_RG_AD_NI_ICHR_LT_ITERM_SHIFT                              2
#define MT6336_RG_AD_QI_VBAT_LT_PRECC1_ADDR                              MT6336_PMIC_ANA_CORE_AD_INTF3
#define MT6336_RG_AD_QI_VBAT_LT_PRECC1_MASK                              0x1
#define MT6336_RG_AD_QI_VBAT_LT_PRECC1_SHIFT                             3
#define MT6336_RG_AD_QI_VBAT_LT_PRECC0_ADDR                              MT6336_PMIC_ANA_CORE_AD_INTF3
#define MT6336_RG_AD_QI_VBAT_LT_PRECC0_MASK                              0x1
#define MT6336_RG_AD_QI_VBAT_LT_PRECC0_SHIFT                             4
#define MT6336_RG_AD_QI_MBATPP_DTEST2_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF3
#define MT6336_RG_AD_QI_MBATPP_DTEST2_MASK                               0x1
#define MT6336_RG_AD_QI_MBATPP_DTEST2_SHIFT                              5
#define MT6336_RG_AD_QI_MBATPP_DTEST1_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF3
#define MT6336_RG_AD_QI_MBATPP_DTEST1_MASK                               0x1
#define MT6336_RG_AD_QI_MBATPP_DTEST1_SHIFT                              6
#define MT6336_RG_AD_QI_OTG_VM_OVP_ADDR                                  MT6336_PMIC_ANA_CORE_AD_INTF4
#define MT6336_RG_AD_QI_OTG_VM_OVP_MASK                                  0x1
#define MT6336_RG_AD_QI_OTG_VM_OVP_SHIFT                                 0
#define MT6336_RG_AD_QI_OTG_VM_UVLO_ADDR                                 MT6336_PMIC_ANA_CORE_AD_INTF4
#define MT6336_RG_AD_QI_OTG_VM_UVLO_MASK                                 0x1
#define MT6336_RG_AD_QI_OTG_VM_UVLO_SHIFT                                1
#define MT6336_RG_AD_QI_OTG_VBAT_UVLO_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF4
#define MT6336_RG_AD_QI_OTG_VBAT_UVLO_MASK                               0x1
#define MT6336_RG_AD_QI_OTG_VBAT_UVLO_SHIFT                              2
#define MT6336_RG_AD_NI_VBAT_OVP_ADDR                                    MT6336_PMIC_ANA_CORE_AD_INTF4
#define MT6336_RG_AD_NI_VBAT_OVP_MASK                                    0x1
#define MT6336_RG_AD_NI_VBAT_OVP_SHIFT                                   3
#define MT6336_RG_AD_NI_VSYS_OVP_ADDR                                    MT6336_PMIC_ANA_CORE_AD_INTF4
#define MT6336_RG_AD_NI_VSYS_OVP_MASK                                    0x1
#define MT6336_RG_AD_NI_VSYS_OVP_SHIFT                                   4
#define MT6336_RG_AD_NI_WEAKBUS_ADDR                                     MT6336_PMIC_ANA_CORE_AD_INTF4
#define MT6336_RG_AD_NI_WEAKBUS_MASK                                     0x1
#define MT6336_RG_AD_NI_WEAKBUS_SHIFT                                    5
#define MT6336_RG_AD_QI_VBUS_UVLO_ADDR                                   MT6336_PMIC_ANA_CORE_AD_INTF4
#define MT6336_RG_AD_QI_VBUS_UVLO_MASK                                   0x1
#define MT6336_RG_AD_QI_VBUS_UVLO_SHIFT                                  6
#define MT6336_RG_AD_QI_VBUS_OVP_ADDR                                    MT6336_PMIC_ANA_CORE_AD_INTF4
#define MT6336_RG_AD_QI_VBUS_OVP_MASK                                    0x1
#define MT6336_RG_AD_QI_VBUS_OVP_SHIFT                                   7
#define MT6336_RG_AD_QI_PAM_MODE_ADDR                                    MT6336_PMIC_ANA_CORE_AD_INTF5
#define MT6336_RG_AD_QI_PAM_MODE_MASK                                    0x1
#define MT6336_RG_AD_QI_PAM_MODE_SHIFT                                   0
#define MT6336_RG_AD_QI_CV_MODE_ADDR                                     MT6336_PMIC_ANA_CORE_AD_INTF5
#define MT6336_RG_AD_QI_CV_MODE_MASK                                     0x1
#define MT6336_RG_AD_QI_CV_MODE_SHIFT                                    1
#define MT6336_RG_AD_QI_ICC_MODE_ADDR                                    MT6336_PMIC_ANA_CORE_AD_INTF5
#define MT6336_RG_AD_QI_ICC_MODE_MASK                                    0x1
#define MT6336_RG_AD_QI_ICC_MODE_SHIFT                                   2
#define MT6336_RG_AD_QI_ICL_MODE_ADDR                                    MT6336_PMIC_ANA_CORE_AD_INTF5
#define MT6336_RG_AD_QI_ICL_MODE_MASK                                    0x1
#define MT6336_RG_AD_QI_ICL_MODE_SHIFT                                   3
#define MT6336_RG_AD_QI_FLASH_VFLA_OVP_ADDR                              MT6336_PMIC_ANA_CORE_AD_INTF5
#define MT6336_RG_AD_QI_FLASH_VFLA_OVP_MASK                              0x1
#define MT6336_RG_AD_QI_FLASH_VFLA_OVP_SHIFT                             4
#define MT6336_RG_AD_QI_FLASH_VFLA_UVLO_ADDR                             MT6336_PMIC_ANA_CORE_AD_INTF5
#define MT6336_RG_AD_QI_FLASH_VFLA_UVLO_MASK                             0x1
#define MT6336_RG_AD_QI_FLASH_VFLA_UVLO_SHIFT                            5
#define MT6336_RG_AD_QI_SWCHR_OC_STATUS_ADDR                             MT6336_PMIC_ANA_CORE_AD_INTF6
#define MT6336_RG_AD_QI_SWCHR_OC_STATUS_MASK                             0x1
#define MT6336_RG_AD_QI_SWCHR_OC_STATUS_SHIFT                            0
#define MT6336_RG_AD_QI_BOOT_UVLO_ADDR                                   MT6336_PMIC_ANA_CORE_AD_INTF6
#define MT6336_RG_AD_QI_BOOT_UVLO_MASK                                   0x1
#define MT6336_RG_AD_QI_BOOT_UVLO_SHIFT                                  1
#define MT6336_RG_AD_QI_ZX_SWCHR_ZCD_FLAG_ADDR                           MT6336_PMIC_ANA_CORE_AD_INTF6
#define MT6336_RG_AD_QI_ZX_SWCHR_ZCD_FLAG_MASK                           0x1
#define MT6336_RG_AD_QI_ZX_SWCHR_ZCD_FLAG_SHIFT                          2
#define MT6336_RG_AD_NI_ZX_OTG_TEST_ADDR                                 MT6336_PMIC_ANA_CORE_AD_INTF6
#define MT6336_RG_AD_NI_ZX_OTG_TEST_MASK                                 0x1
#define MT6336_RG_AD_NI_ZX_OTG_TEST_SHIFT                                3
#define MT6336_RG_AD_NI_ZX_FLASH_TEST_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF6
#define MT6336_RG_AD_NI_ZX_FLASH_TEST_MASK                               0x1
#define MT6336_RG_AD_NI_ZX_FLASH_TEST_SHIFT                              4
#define MT6336_RG_AD_NI_ZX_SWCHR_TEST_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF6
#define MT6336_RG_AD_NI_ZX_SWCHR_TEST_MASK                               0x1
#define MT6336_RG_AD_NI_ZX_SWCHR_TEST_SHIFT                              5
#define MT6336_RG_AD_QI_OTG_OLP_ADDR                                     MT6336_PMIC_ANA_CORE_AD_INTF6
#define MT6336_RG_AD_QI_OTG_OLP_MASK                                     0x1
#define MT6336_RG_AD_QI_OTG_OLP_SHIFT                                    6
#define MT6336_RG_AD_QI_THR_MODE_ADDR                                    MT6336_PMIC_ANA_CORE_AD_INTF6
#define MT6336_RG_AD_QI_THR_MODE_MASK                                    0x1
#define MT6336_RG_AD_QI_THR_MODE_SHIFT                                   7
#define MT6336_RG_AD_NI_FTR_SHOOT_DB_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF7
#define MT6336_RG_AD_NI_FTR_SHOOT_DB_MASK                                0x1
#define MT6336_RG_AD_NI_FTR_SHOOT_DB_SHIFT                               0
#define MT6336_RG_AD_NI_FTR_SHOOT_ADDR                                   MT6336_PMIC_ANA_CORE_AD_INTF7
#define MT6336_RG_AD_NI_FTR_SHOOT_MASK                                   0x1
#define MT6336_RG_AD_NI_FTR_SHOOT_SHIFT                                  1
#define MT6336_RG_AD_NI_FTR_DROP_DB_ADDR                                 MT6336_PMIC_ANA_CORE_AD_INTF7
#define MT6336_RG_AD_NI_FTR_DROP_DB_MASK                                 0x1
#define MT6336_RG_AD_NI_FTR_DROP_DB_SHIFT                                2
#define MT6336_RG_AD_NI_FTR_DROP_ADDR                                    MT6336_PMIC_ANA_CORE_AD_INTF7
#define MT6336_RG_AD_NI_FTR_DROP_MASK                                    0x1
#define MT6336_RG_AD_NI_FTR_DROP_SHIFT                                   3
#define MT6336_RG_AD_QI_VADC18_READY_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF7
#define MT6336_RG_AD_QI_VADC18_READY_MASK                                0x1
#define MT6336_RG_AD_QI_VADC18_READY_SHIFT                               4
#define MT6336_RG_AD_QI_SWCHR_ENPWM_STATUS_ADDR                          MT6336_PMIC_ANA_CORE_AD_INTF7
#define MT6336_RG_AD_QI_SWCHR_ENPWM_STATUS_MASK                          0x1
#define MT6336_RG_AD_QI_SWCHR_ENPWM_STATUS_SHIFT                         5
#define MT6336_RG_AD_QI_SWCHR_OCASYN_STATUS_ADDR                         MT6336_PMIC_ANA_CORE_AD_INTF7
#define MT6336_RG_AD_QI_SWCHR_OCASYN_STATUS_MASK                         0x1
#define MT6336_RG_AD_QI_SWCHR_OCASYN_STATUS_SHIFT                        6
#define MT6336_RG_DA_QI_TORCH_MODE_ADDR                                  MT6336_PMIC_ANA_CORE_DA_INTF0
#define MT6336_RG_DA_QI_TORCH_MODE_MASK                                  0x1
#define MT6336_RG_DA_QI_TORCH_MODE_SHIFT                                 0
#define MT6336_RG_DA_QI_OTG_MODE_DB_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF0
#define MT6336_RG_DA_QI_OTG_MODE_DB_MASK                                 0x1
#define MT6336_RG_DA_QI_OTG_MODE_DB_SHIFT                                1
#define MT6336_RG_DA_QI_FLASH_MODE_DB_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF0
#define MT6336_RG_DA_QI_FLASH_MODE_DB_MASK                               0x1
#define MT6336_RG_DA_QI_FLASH_MODE_DB_SHIFT                              2
#define MT6336_RG_DA_QI_BUCK_MODE_DB_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF0
#define MT6336_RG_DA_QI_BUCK_MODE_DB_MASK                                0x1
#define MT6336_RG_DA_QI_BUCK_MODE_DB_SHIFT                               3
#define MT6336_RG_DA_QI_BASE_READY_DB_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF0
#define MT6336_RG_DA_QI_BASE_READY_DB_MASK                               0x1
#define MT6336_RG_DA_QI_BASE_READY_DB_SHIFT                              4
#define MT6336_RG_DA_QI_BASE_READY_ADDR                                  MT6336_PMIC_ANA_CORE_DA_INTF0
#define MT6336_RG_DA_QI_BASE_READY_MASK                                  0x1
#define MT6336_RG_DA_QI_BASE_READY_SHIFT                                 5
#define MT6336_RG_DA_QI_LOWQ_STAT_ADDR                                   MT6336_PMIC_ANA_CORE_DA_INTF0
#define MT6336_RG_DA_QI_LOWQ_STAT_MASK                                   0x1
#define MT6336_RG_DA_QI_LOWQ_STAT_SHIFT                                  6
#define MT6336_RG_DA_QI_SHIP_STAT_ADDR                                   MT6336_PMIC_ANA_CORE_DA_INTF0
#define MT6336_RG_DA_QI_SHIP_STAT_MASK                                   0x1
#define MT6336_RG_DA_QI_SHIP_STAT_SHIFT                                  7
#define MT6336_RG_DA_QI_TORCH_MODE_DRV_EN_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF1
#define MT6336_RG_DA_QI_TORCH_MODE_DRV_EN_MASK                           0x1
#define MT6336_RG_DA_QI_TORCH_MODE_DRV_EN_SHIFT                          0
#define MT6336_RG_DA_QI_TORCH_MODE_DB_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF1
#define MT6336_RG_DA_QI_TORCH_MODE_DB_MASK                               0x1
#define MT6336_RG_DA_QI_TORCH_MODE_DB_SHIFT                              1
#define MT6336_RG_DA_QI_BASE_CLK_TRIM_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF2
#define MT6336_RG_DA_QI_BASE_CLK_TRIM_MASK                               0x3F
#define MT6336_RG_DA_QI_BASE_CLK_TRIM_SHIFT                              0
#define MT6336_RG_DA_QI_OSC_TRIM_ADDR                                    MT6336_PMIC_ANA_CORE_DA_INTF3
#define MT6336_RG_DA_QI_OSC_TRIM_MASK                                    0x3F
#define MT6336_RG_DA_QI_OSC_TRIM_SHIFT                                   0
#define MT6336_RG_DA_QI_CLKSQ_IN_SEL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF4
#define MT6336_RG_DA_QI_CLKSQ_IN_SEL_MASK                                0x1
#define MT6336_RG_DA_QI_CLKSQ_IN_SEL_SHIFT                               0
#define MT6336_RG_DA_QI_CLKSQ_EN_ADDR                                    MT6336_PMIC_ANA_CORE_DA_INTF4
#define MT6336_RG_DA_QI_CLKSQ_EN_MASK                                    0x1
#define MT6336_RG_DA_QI_CLKSQ_EN_SHIFT                                   1
#define MT6336_RG_DA_QI_EFUSE_READ_RDY_DLY_ADDR                          MT6336_PMIC_ANA_CORE_DA_INTF4
#define MT6336_RG_DA_QI_EFUSE_READ_RDY_DLY_MASK                          0x1
#define MT6336_RG_DA_QI_EFUSE_READ_RDY_DLY_SHIFT                         2
#define MT6336_RG_DA_QI_VBGR_READY_DB_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF4
#define MT6336_RG_DA_QI_VBGR_READY_DB_MASK                               0x1
#define MT6336_RG_DA_QI_VBGR_READY_DB_SHIFT                              3
#define MT6336_RG_DA_QI_BGR_SPEEDUP_EN_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF4
#define MT6336_RG_DA_QI_BGR_SPEEDUP_EN_MASK                              0x1
#define MT6336_RG_DA_QI_BGR_SPEEDUP_EN_SHIFT                             4
#define MT6336_RG_DA_QI_ILED1_ITH_ADDR                                   MT6336_PMIC_ANA_CORE_DA_INTF5
#define MT6336_RG_DA_QI_ILED1_ITH_MASK                                   0x7F
#define MT6336_RG_DA_QI_ILED1_ITH_SHIFT                                  0
#define MT6336_RG_DA_QI_ADC18_EN_ADDR                                    MT6336_PMIC_ANA_CORE_DA_INTF5
#define MT6336_RG_DA_QI_ADC18_EN_MASK                                    0x1
#define MT6336_RG_DA_QI_ADC18_EN_SHIFT                                   7
#define MT6336_RG_DA_QI_ILED1_EN_ADDR                                    MT6336_PMIC_ANA_CORE_DA_INTF6
#define MT6336_RG_DA_QI_ILED1_EN_MASK                                    0x1
#define MT6336_RG_DA_QI_ILED1_EN_SHIFT                                   0
#define MT6336_RG_DA_QI_ILED2_ITH_ADDR                                   MT6336_PMIC_ANA_CORE_DA_INTF6
#define MT6336_RG_DA_QI_ILED2_ITH_MASK                                   0x7F
#define MT6336_RG_DA_QI_ILED2_ITH_SHIFT                                  1
#define MT6336_RG_DA_QI_EN_ADCIN_VLED1_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF7
#define MT6336_RG_DA_QI_EN_ADCIN_VLED1_MASK                              0x1
#define MT6336_RG_DA_QI_EN_ADCIN_VLED1_SHIFT                             0
#define MT6336_RG_DA_QI_EN_ADCIN_VBATON_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF7
#define MT6336_RG_DA_QI_EN_ADCIN_VBATON_MASK                             0x1
#define MT6336_RG_DA_QI_EN_ADCIN_VBATON_SHIFT                            1
#define MT6336_RG_DA_QI_EN_ADCIN_VBUS_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF7
#define MT6336_RG_DA_QI_EN_ADCIN_VBUS_MASK                               0x1
#define MT6336_RG_DA_QI_EN_ADCIN_VBUS_SHIFT                              2
#define MT6336_RG_DA_QI_EN_ADCIN_VBATSNS_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF7
#define MT6336_RG_DA_QI_EN_ADCIN_VBATSNS_MASK                            0x1
#define MT6336_RG_DA_QI_EN_ADCIN_VBATSNS_SHIFT                           3
#define MT6336_RG_DA_QI_IOSDET2_EN_ADDR                                  MT6336_PMIC_ANA_CORE_DA_INTF7
#define MT6336_RG_DA_QI_IOSDET2_EN_MASK                                  0x1
#define MT6336_RG_DA_QI_IOSDET2_EN_SHIFT                                 4
#define MT6336_RG_DA_QI_IOSDET1_EN_ADDR                                  MT6336_PMIC_ANA_CORE_DA_INTF7
#define MT6336_RG_DA_QI_IOSDET1_EN_MASK                                  0x1
#define MT6336_RG_DA_QI_IOSDET1_EN_SHIFT                                 5
#define MT6336_RG_DA_QI_OSDET_EN_ADDR                                    MT6336_PMIC_ANA_CORE_DA_INTF7
#define MT6336_RG_DA_QI_OSDET_EN_MASK                                    0x1
#define MT6336_RG_DA_QI_OSDET_EN_SHIFT                                   6
#define MT6336_RG_DA_QI_ILED2_EN_ADDR                                    MT6336_PMIC_ANA_CORE_DA_INTF7
#define MT6336_RG_DA_QI_ILED2_EN_MASK                                    0x1
#define MT6336_RG_DA_QI_ILED2_EN_SHIFT                                   7
#define MT6336_RG_DA_QI_EN_ADCIN_VLED2_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF8
#define MT6336_RG_DA_QI_EN_ADCIN_VLED2_MASK                              0x1
#define MT6336_RG_DA_QI_EN_ADCIN_VLED2_SHIFT                             0
#define MT6336_RG_DA_QI_BC12_IPDC_EN_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF9
#define MT6336_RG_DA_QI_BC12_IPDC_EN_MASK                                0x3
#define MT6336_RG_DA_QI_BC12_IPDC_EN_SHIFT                               0
#define MT6336_RG_DA_QI_BC12_IPU_EN_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF9
#define MT6336_RG_DA_QI_BC12_IPU_EN_MASK                                 0x3
#define MT6336_RG_DA_QI_BC12_IPU_EN_SHIFT                                2
#define MT6336_RG_DA_QI_BC12_IPD_EN_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF9
#define MT6336_RG_DA_QI_BC12_IPD_EN_MASK                                 0x3
#define MT6336_RG_DA_QI_BC12_IPD_EN_SHIFT                                4
#define MT6336_RG_DA_QI_BC12_IBIAS_EN_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF9
#define MT6336_RG_DA_QI_BC12_IBIAS_EN_MASK                               0x1
#define MT6336_RG_DA_QI_BC12_IBIAS_EN_SHIFT                              6
#define MT6336_RG_DA_QI_BC12_BB_CTRL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF10
#define MT6336_RG_DA_QI_BC12_BB_CTRL_MASK                                0x1
#define MT6336_RG_DA_QI_BC12_BB_CTRL_SHIFT                               0
#define MT6336_RG_DA_QI_BC12_IPD_HALF_EN_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF10
#define MT6336_RG_DA_QI_BC12_IPD_HALF_EN_MASK                            0x1
#define MT6336_RG_DA_QI_BC12_IPD_HALF_EN_SHIFT                           1
#define MT6336_RG_DA_QI_BC12_VREF_VTH_EN_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF10
#define MT6336_RG_DA_QI_BC12_VREF_VTH_EN_MASK                            0x3
#define MT6336_RG_DA_QI_BC12_VREF_VTH_EN_SHIFT                           2
#define MT6336_RG_DA_QI_BC12_CMP_EN_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF10
#define MT6336_RG_DA_QI_BC12_CMP_EN_MASK                                 0x3
#define MT6336_RG_DA_QI_BC12_CMP_EN_SHIFT                                4
#define MT6336_RG_DA_QI_BC12_VSRC_EN_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF10
#define MT6336_RG_DA_QI_BC12_VSRC_EN_MASK                                0x3
#define MT6336_RG_DA_QI_BC12_VSRC_EN_SHIFT                               6
#define MT6336_RG_DA_QI_VPREG_RDY_DB_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF11
#define MT6336_RG_DA_QI_VPREG_RDY_DB_MASK                                0x1
#define MT6336_RG_DA_QI_VPREG_RDY_DB_SHIFT                               0
#define MT6336_RG_DA_QI_IPRECC1_ITH_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF11
#define MT6336_RG_DA_QI_IPRECC1_ITH_MASK                                 0x3
#define MT6336_RG_DA_QI_IPRECC1_ITH_SHIFT                                1
#define MT6336_RG_DA_QI_EN_IBIASGEN_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF11
#define MT6336_RG_DA_QI_EN_IBIASGEN_MASK                                 0x1
#define MT6336_RG_DA_QI_EN_IBIASGEN_SHIFT                                3
#define MT6336_RG_DA_NS_VPRECHG_FAIL_DB_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF11
#define MT6336_RG_DA_NS_VPRECHG_FAIL_DB_MASK                             0x1
#define MT6336_RG_DA_NS_VPRECHG_FAIL_DB_SHIFT                            4
#define MT6336_RG_DA_QI_EN_CP_HIQ_ADDR                                   MT6336_PMIC_ANA_CORE_DA_INTF11
#define MT6336_RG_DA_QI_EN_CP_HIQ_MASK                                   0x1
#define MT6336_RG_DA_QI_EN_CP_HIQ_SHIFT                                  5
#define MT6336_RG_DA_QI_VBAT_LT_PRECC1_DB_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF11
#define MT6336_RG_DA_QI_VBAT_LT_PRECC1_DB_MASK                           0x1
#define MT6336_RG_DA_QI_VBAT_LT_PRECC1_DB_SHIFT                          6
#define MT6336_RG_DA_QI_VBAT_LT_PRECC0_DB_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF11
#define MT6336_RG_DA_QI_VBAT_LT_PRECC0_DB_MASK                           0x1
#define MT6336_RG_DA_QI_VBAT_LT_PRECC0_DB_SHIFT                          7
#define MT6336_RG_DA_QI_IPRECC1_EN_ADDR                                  MT6336_PMIC_ANA_CORE_DA_INTF12
#define MT6336_RG_DA_QI_IPRECC1_EN_MASK                                  0x1
#define MT6336_RG_DA_QI_IPRECC1_EN_SHIFT                                 0
#define MT6336_RG_DA_QI_IPRECC0_EN_ADDR                                  MT6336_PMIC_ANA_CORE_DA_INTF12
#define MT6336_RG_DA_QI_IPRECC0_EN_MASK                                  0x1
#define MT6336_RG_DA_QI_IPRECC0_EN_SHIFT                                 1
#define MT6336_RG_DA_QI_VBATFETON_VTH_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF12
#define MT6336_RG_DA_QI_VBATFETON_VTH_MASK                               0x7
#define MT6336_RG_DA_QI_VBATFETON_VTH_SHIFT                              2
#define MT6336_RG_DA_QI_BATOC_ANA_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF12
#define MT6336_RG_DA_QI_BATOC_ANA_SEL_MASK                               0x1
#define MT6336_RG_DA_QI_BATOC_ANA_SEL_SHIFT                              5
#define MT6336_RG_DA_QI_FETON_OK_LOQCP_EN_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF12
#define MT6336_RG_DA_QI_FETON_OK_LOQCP_EN_MASK                           0x1
#define MT6336_RG_DA_QI_FETON_OK_LOQCP_EN_SHIFT                          6
#define MT6336_RG_DA_QI_EN_VDC_IBIAS_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF12
#define MT6336_RG_DA_QI_EN_VDC_IBIAS_MASK                                0x1
#define MT6336_RG_DA_QI_EN_VDC_IBIAS_SHIFT                               7
#define MT6336_RG_DA_NI_CHRIND_EN_ADDR                                   MT6336_PMIC_ANA_CORE_DA_INTF13
#define MT6336_RG_DA_NI_CHRIND_EN_MASK                                   0x1
#define MT6336_RG_DA_NI_CHRIND_EN_SHIFT                                  0
#define MT6336_RG_DA_NI_CHRIND_BIAS_EN_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF13
#define MT6336_RG_DA_NI_CHRIND_BIAS_EN_MASK                              0x1
#define MT6336_RG_DA_NI_CHRIND_BIAS_EN_SHIFT                             1
#define MT6336_RG_DA_QI_VSYS_IDIS_EN_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF13
#define MT6336_RG_DA_QI_VSYS_IDIS_EN_MASK                                0x1
#define MT6336_RG_DA_QI_VSYS_IDIS_EN_SHIFT                               2
#define MT6336_RG_DA_QI_VBAT_IDIS_EN_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF13
#define MT6336_RG_DA_QI_VBAT_IDIS_EN_MASK                                0x1
#define MT6336_RG_DA_QI_VBAT_IDIS_EN_SHIFT                               3
#define MT6336_RG_DA_QI_ITERM_ITH_ADDR                                   MT6336_PMIC_ANA_CORE_DA_INTF13
#define MT6336_RG_DA_QI_ITERM_ITH_MASK                                   0xF
#define MT6336_RG_DA_QI_ITERM_ITH_SHIFT                                  4
#define MT6336_RG_DA_NI_CHRIND_STEP_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF14
#define MT6336_RG_DA_NI_CHRIND_STEP_MASK                                 0x3
#define MT6336_RG_DA_NI_CHRIND_STEP_SHIFT                                0
#define MT6336_RG_DA_QI_CHR_DET_ADDR                                     MT6336_PMIC_ANA_CORE_DA_INTF15
#define MT6336_RG_DA_QI_CHR_DET_MASK                                     0x1
#define MT6336_RG_DA_QI_CHR_DET_SHIFT                                    0
#define MT6336_RG_DA_QI_REVFET_EN_ADDR                                   MT6336_PMIC_ANA_CORE_DA_INTF15
#define MT6336_RG_DA_QI_REVFET_EN_MASK                                   0x1
#define MT6336_RG_DA_QI_REVFET_EN_SHIFT                                  1
#define MT6336_RG_DA_QI_SSFNSH_ADDR                                      MT6336_PMIC_ANA_CORE_DA_INTF15
#define MT6336_RG_DA_QI_SSFNSH_MASK                                      0x1
#define MT6336_RG_DA_QI_SSFNSH_SHIFT                                     2
#define MT6336_RG_DA_QI_IBACKBOOST_EN_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF15
#define MT6336_RG_DA_QI_IBACKBOOST_EN_MASK                               0x1
#define MT6336_RG_DA_QI_IBACKBOOST_EN_SHIFT                              3
#define MT6336_RG_DA_NI_WEAKBUS_DB_ADDR                                  MT6336_PMIC_ANA_CORE_DA_INTF15
#define MT6336_RG_DA_NI_WEAKBUS_DB_MASK                                  0x1
#define MT6336_RG_DA_NI_WEAKBUS_DB_SHIFT                                 4
#define MT6336_RG_DA_QI_VBUS_OVP_DB_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF15
#define MT6336_RG_DA_QI_VBUS_OVP_DB_MASK                                 0x1
#define MT6336_RG_DA_QI_VBUS_OVP_DB_SHIFT                                5
#define MT6336_RG_DA_QI_VBUS_UVLO_DB_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF15
#define MT6336_RG_DA_QI_VBUS_UVLO_DB_MASK                                0x1
#define MT6336_RG_DA_QI_VBUS_UVLO_DB_SHIFT                               6
#define MT6336_RG_DA_QI_ICL_ITH_ADDR                                     MT6336_PMIC_ANA_CORE_DA_INTF16
#define MT6336_RG_DA_QI_ICL_ITH_MASK                                     0x3F
#define MT6336_RG_DA_QI_ICL_ITH_SHIFT                                    0
#define MT6336_RG_DA_QI_CV_REGNODE_SEL_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF17
#define MT6336_RG_DA_QI_CV_REGNODE_SEL_MASK                              0x1
#define MT6336_RG_DA_QI_CV_REGNODE_SEL_SHIFT                             0
#define MT6336_RG_DA_QI_ICC_ITH_ADDR                                     MT6336_PMIC_ANA_CORE_DA_INTF17
#define MT6336_RG_DA_QI_ICC_ITH_MASK                                     0x7F
#define MT6336_RG_DA_QI_ICC_ITH_SHIFT                                    1
#define MT6336_RG_DA_QI_VSYSREG_VTH_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF18
#define MT6336_RG_DA_QI_VSYSREG_VTH_MASK                                 0xFF
#define MT6336_RG_DA_QI_VSYSREG_VTH_SHIFT                                0
#define MT6336_RG_DA_QI_VCV_VTH_ADDR                                     MT6336_PMIC_ANA_CORE_DA_INTF19
#define MT6336_RG_DA_QI_VCV_VTH_MASK                                     0xFF
#define MT6336_RG_DA_QI_VCV_VTH_SHIFT                                    0
#define MT6336_RG_DA_QI_OTG_VCV_VTH_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF20
#define MT6336_RG_DA_QI_OTG_VCV_VTH_MASK                                 0xF
#define MT6336_RG_DA_QI_OTG_VCV_VTH_SHIFT                                0
#define MT6336_RG_DA_QI_FLASH_MODE_DRV_EN_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF21
#define MT6336_RG_DA_QI_FLASH_MODE_DRV_EN_MASK                           0x1
#define MT6336_RG_DA_QI_FLASH_MODE_DRV_EN_SHIFT                          0
#define MT6336_RG_DA_QI_OTG_MODE_ADDR                                    MT6336_PMIC_ANA_CORE_DA_INTF21
#define MT6336_RG_DA_QI_OTG_MODE_MASK                                    0x1
#define MT6336_RG_DA_QI_OTG_MODE_SHIFT                                   1
#define MT6336_RG_DA_QI_OTG_MODE_DRV_EN_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF21
#define MT6336_RG_DA_QI_OTG_MODE_DRV_EN_MASK                             0x1
#define MT6336_RG_DA_QI_OTG_MODE_DRV_EN_SHIFT                            2
#define MT6336_RG_DA_NI_VBSTCHK_FEN_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF21
#define MT6336_RG_DA_NI_VBSTCHK_FEN_MASK                                 0x1
#define MT6336_RG_DA_NI_VBSTCHK_FEN_SHIFT                                3
#define MT6336_RG_DA_QI_VFLA_VTH_ADDR                                    MT6336_PMIC_ANA_CORE_DA_INTF21
#define MT6336_RG_DA_QI_VFLA_VTH_MASK                                    0xF
#define MT6336_RG_DA_QI_VFLA_VTH_SHIFT                                   4
#define MT6336_RG_DA_QI_FLASH_VFLA_OVP_DB_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF22
#define MT6336_RG_DA_QI_FLASH_VFLA_OVP_DB_MASK                           0x1
#define MT6336_RG_DA_QI_FLASH_VFLA_OVP_DB_SHIFT                          0
#define MT6336_RG_DA_QI_FLASH_VFLA_UVLO_DB_ADDR                          MT6336_PMIC_ANA_CORE_DA_INTF22
#define MT6336_RG_DA_QI_FLASH_VFLA_UVLO_DB_MASK                          0x1
#define MT6336_RG_DA_QI_FLASH_VFLA_UVLO_DB_SHIFT                         1
#define MT6336_RG_DA_QI_OTG_VM_UVLO_DB_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF22
#define MT6336_RG_DA_QI_OTG_VM_UVLO_DB_MASK                              0x1
#define MT6336_RG_DA_QI_OTG_VM_UVLO_DB_SHIFT                             2
#define MT6336_RG_DA_QI_OTG_VM_OVP_DB_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF22
#define MT6336_RG_DA_QI_OTG_VM_OVP_DB_MASK                               0x1
#define MT6336_RG_DA_QI_OTG_VM_OVP_DB_SHIFT                              3
#define MT6336_RG_DA_QI_OSC_REF_EN_ADDR                                  MT6336_PMIC_ANA_CORE_DA_INTF22
#define MT6336_RG_DA_QI_OSC_REF_EN_MASK                                  0x1
#define MT6336_RG_DA_QI_OSC_REF_EN_SHIFT                                 4
#define MT6336_RG_DA_QI_BUCK_MODE_ADDR                                   MT6336_PMIC_ANA_CORE_DA_INTF22
#define MT6336_RG_DA_QI_BUCK_MODE_MASK                                   0x1
#define MT6336_RG_DA_QI_BUCK_MODE_SHIFT                                  5
#define MT6336_RG_DA_QI_BUCK_MODE_DRV_EN_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF22
#define MT6336_RG_DA_QI_BUCK_MODE_DRV_EN_MASK                            0x1
#define MT6336_RG_DA_QI_BUCK_MODE_DRV_EN_SHIFT                           6
#define MT6336_RG_DA_QI_FLASH_MODE_ADDR                                  MT6336_PMIC_ANA_CORE_DA_INTF22
#define MT6336_RG_DA_QI_FLASH_MODE_MASK                                  0x1
#define MT6336_RG_DA_QI_FLASH_MODE_SHIFT                                 7
#define MT6336_RG_DA_QI_MFLAPP_EN_ADDR                                   MT6336_PMIC_ANA_CORE_DA_INTF23
#define MT6336_RG_DA_QI_MFLAPP_EN_MASK                                   0x1
#define MT6336_RG_DA_QI_MFLAPP_EN_SHIFT                                  0
#define MT6336_RG_DA_QI_OTG_IOLP_ITH_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF23
#define MT6336_RG_DA_QI_OTG_IOLP_ITH_MASK                                0x7
#define MT6336_RG_DA_QI_OTG_IOLP_ITH_SHIFT                               1
#define MT6336_RG_DA_QI_VSYS_REGV_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF23
#define MT6336_RG_DA_QI_VSYS_REGV_SEL_MASK                               0x1
#define MT6336_RG_DA_QI_VSYS_REGV_SEL_SHIFT                              4
#define MT6336_RG_DA_QI_OTG_VBAT_UVLO_DB_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF23
#define MT6336_RG_DA_QI_OTG_VBAT_UVLO_DB_MASK                            0x1
#define MT6336_RG_DA_QI_OTG_VBAT_UVLO_DB_SHIFT                           5
#define MT6336_RG_DA_NI_VBAT_OVP_DB_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF23
#define MT6336_RG_DA_NI_VBAT_OVP_DB_MASK                                 0x1
#define MT6336_RG_DA_NI_VBAT_OVP_DB_SHIFT                                6
#define MT6336_RG_DA_NI_VSYS_OVP_DB_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF23
#define MT6336_RG_DA_NI_VSYS_OVP_DB_MASK                                 0x1
#define MT6336_RG_DA_NI_VSYS_OVP_DB_SHIFT                                7
#define MT6336_RG_DA_QI_BACKGROUND_STAT_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF24
#define MT6336_RG_DA_QI_BACKGROUND_STAT_MASK                             0x1
#define MT6336_RG_DA_QI_BACKGROUND_STAT_SHIFT                            0
#define MT6336_RG_DA_QI_DEADBAT_STAT_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF24
#define MT6336_RG_DA_QI_DEADBAT_STAT_MASK                                0x1
#define MT6336_RG_DA_QI_DEADBAT_STAT_SHIFT                               1
#define MT6336_RG_DA_QI_EOC_STAT_ADDR                                    MT6336_PMIC_ANA_CORE_DA_INTF24
#define MT6336_RG_DA_QI_EOC_STAT_MASK                                    0x1
#define MT6336_RG_DA_QI_EOC_STAT_SHIFT                                   2
#define MT6336_RG_DA_QI_FASTCC_STAT_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF24
#define MT6336_RG_DA_QI_FASTCC_STAT_MASK                                 0x1
#define MT6336_RG_DA_QI_FASTCC_STAT_SHIFT                                3
#define MT6336_RG_DA_QI_POSTPRECC1_STAT_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF24
#define MT6336_RG_DA_QI_POSTPRECC1_STAT_MASK                             0x1
#define MT6336_RG_DA_QI_POSTPRECC1_STAT_SHIFT                            4
#define MT6336_RG_DA_QI_VBUS_PLUGIN_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF24
#define MT6336_RG_DA_QI_VBUS_PLUGIN_MASK                                 0x1
#define MT6336_RG_DA_QI_VBUS_PLUGIN_SHIFT                                5
#define MT6336_RG_DA_QI_VBUS_IDIS_EN_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF24
#define MT6336_RG_DA_QI_VBUS_IDIS_EN_MASK                                0x1
#define MT6336_RG_DA_QI_VBUS_IDIS_EN_SHIFT                               6
#define MT6336_RG_DA_QI_MTORPP_EN_ADDR                                   MT6336_PMIC_ANA_CORE_DA_INTF24
#define MT6336_RG_DA_QI_MTORPP_EN_MASK                                   0x1
#define MT6336_RG_DA_QI_MTORPP_EN_SHIFT                                  7
#define MT6336_RG_DA_QI_VPAM_VTH_ADDR                                    MT6336_PMIC_ANA_CORE_DA_INTF25
#define MT6336_RG_DA_QI_VPAM_VTH_MASK                                    0x1F
#define MT6336_RG_DA_QI_VPAM_VTH_SHIFT                                   0
#define MT6336_RG_AD_AUXADC_COMP_ADDR                                    MT6336_PMIC_ANA_AUXADC_AD_INTF0
#define MT6336_RG_AD_AUXADC_COMP_MASK                                    0x1
#define MT6336_RG_AD_AUXADC_COMP_SHIFT                                   0
#define MT6336_RG_DA_AUXADC_DAC_0_ADDR                                   MT6336_PMIC_ANA_AUXADC_DA_INTF0
#define MT6336_RG_DA_AUXADC_DAC_0_MASK                                   0xFF
#define MT6336_RG_DA_AUXADC_DAC_0_SHIFT                                  0
#define MT6336_RG_DA_AUXADC_DAC_1_ADDR                                   MT6336_PMIC_ANA_AUXADC_DA_INTF1
#define MT6336_RG_DA_AUXADC_DAC_1_MASK                                   0xF
#define MT6336_RG_DA_AUXADC_DAC_1_SHIFT                                  0
#define MT6336_RG_DA_AUXADC_SEL_ADDR                                     MT6336_PMIC_ANA_AUXADC_DA_INTF2
#define MT6336_RG_DA_AUXADC_SEL_MASK                                     0xF
#define MT6336_RG_DA_AUXADC_SEL_SHIFT                                    0
#define MT6336_RG_DA_TS_VBE_SEL_ADDR                                     MT6336_PMIC_ANA_AUXADC_DA_INTF3
#define MT6336_RG_DA_TS_VBE_SEL_MASK                                     0x1
#define MT6336_RG_DA_TS_VBE_SEL_SHIFT                                    0
#define MT6336_RG_DA_VBUF_EN_ADDR                                        MT6336_PMIC_ANA_AUXADC_DA_INTF3
#define MT6336_RG_DA_VBUF_EN_MASK                                        0x1
#define MT6336_RG_DA_VBUF_EN_SHIFT                                       1
#define MT6336_RG_DA_AUXADC_RNG_ADDR                                     MT6336_PMIC_ANA_AUXADC_DA_INTF3
#define MT6336_RG_DA_AUXADC_RNG_MASK                                     0x1
#define MT6336_RG_DA_AUXADC_RNG_SHIFT                                    2
#define MT6336_RG_DA_AUXADC_SPL_ADDR                                     MT6336_PMIC_ANA_AUXADC_DA_INTF3
#define MT6336_RG_DA_AUXADC_SPL_MASK                                     0x1
#define MT6336_RG_DA_AUXADC_SPL_SHIFT                                    3
#define MT6336_RG_DA_AUXADC_ADC_PWDB_ADDR                                MT6336_PMIC_ANA_AUXADC_DA_INTF3
#define MT6336_RG_DA_AUXADC_ADC_PWDB_MASK                                0x1
#define MT6336_RG_DA_AUXADC_ADC_PWDB_SHIFT                               4
#define MT6336_RG_AD_PD_CC2_OVP_ADDR                                     MT6336_PMIC_ANA_TYPEC_AD_INTF0
#define MT6336_RG_AD_PD_CC2_OVP_MASK                                     0x1
#define MT6336_RG_AD_PD_CC2_OVP_SHIFT                                    0
#define MT6336_RG_AD_PD_CC1_OVP_ADDR                                     MT6336_PMIC_ANA_TYPEC_AD_INTF0
#define MT6336_RG_AD_PD_CC1_OVP_MASK                                     0x1
#define MT6336_RG_AD_PD_CC1_OVP_SHIFT                                    1
#define MT6336_RG_AD_PD_VCONN_UVP_ADDR                                   MT6336_PMIC_ANA_TYPEC_AD_INTF0
#define MT6336_RG_AD_PD_VCONN_UVP_MASK                                   0x1
#define MT6336_RG_AD_PD_VCONN_UVP_SHIFT                                  2
#define MT6336_RG_AD_PD_RX_DATA_ADDR                                     MT6336_PMIC_ANA_TYPEC_AD_INTF0
#define MT6336_RG_AD_PD_RX_DATA_MASK                                     0x1
#define MT6336_RG_AD_PD_RX_DATA_SHIFT                                    3
#define MT6336_RG_AD_CC_VUSB33_RDY_ADDR                                  MT6336_PMIC_ANA_TYPEC_AD_INTF0
#define MT6336_RG_AD_CC_VUSB33_RDY_MASK                                  0x1
#define MT6336_RG_AD_CC_VUSB33_RDY_SHIFT                                 4
#define MT6336_RG_AD_CC_CMP_OUT_ADDR                                     MT6336_PMIC_ANA_TYPEC_AD_INTF0
#define MT6336_RG_AD_CC_CMP_OUT_MASK                                     0x1
#define MT6336_RG_AD_CC_CMP_OUT_SHIFT                                    5
#define MT6336_RG_DA_CC_LPF_EN_ADDR                                      MT6336_PMIC_ANA_TYPEC_DA_INTF0
#define MT6336_RG_DA_CC_LPF_EN_MASK                                      0x1
#define MT6336_RG_DA_CC_LPF_EN_SHIFT                                     0
#define MT6336_RG_DA_CC_BIAS_EN_ADDR                                     MT6336_PMIC_ANA_TYPEC_DA_INTF0
#define MT6336_RG_DA_CC_BIAS_EN_MASK                                     0x1
#define MT6336_RG_DA_CC_BIAS_EN_SHIFT                                    1
#define MT6336_RG_DA_CC_RACC2_EN_ADDR                                    MT6336_PMIC_ANA_TYPEC_DA_INTF0
#define MT6336_RG_DA_CC_RACC2_EN_MASK                                    0x1
#define MT6336_RG_DA_CC_RACC2_EN_SHIFT                                   2
#define MT6336_RG_DA_CC_RDCC2_EN_ADDR                                    MT6336_PMIC_ANA_TYPEC_DA_INTF0
#define MT6336_RG_DA_CC_RDCC2_EN_MASK                                    0x1
#define MT6336_RG_DA_CC_RDCC2_EN_SHIFT                                   3
#define MT6336_RG_DA_CC_RPCC2_EN_ADDR                                    MT6336_PMIC_ANA_TYPEC_DA_INTF0
#define MT6336_RG_DA_CC_RPCC2_EN_MASK                                    0x1
#define MT6336_RG_DA_CC_RPCC2_EN_SHIFT                                   4
#define MT6336_RG_DA_CC_RACC1_EN_ADDR                                    MT6336_PMIC_ANA_TYPEC_DA_INTF0
#define MT6336_RG_DA_CC_RACC1_EN_MASK                                    0x1
#define MT6336_RG_DA_CC_RACC1_EN_SHIFT                                   5
#define MT6336_RG_DA_CC_RDCC1_EN_ADDR                                    MT6336_PMIC_ANA_TYPEC_DA_INTF0
#define MT6336_RG_DA_CC_RDCC1_EN_MASK                                    0x1
#define MT6336_RG_DA_CC_RDCC1_EN_SHIFT                                   6
#define MT6336_RG_DA_CC_RPCC1_EN_ADDR                                    MT6336_PMIC_ANA_TYPEC_DA_INTF0
#define MT6336_RG_DA_CC_RPCC1_EN_MASK                                    0x1
#define MT6336_RG_DA_CC_RPCC1_EN_SHIFT                                   7
#define MT6336_RG_DA_PD_TX_DATA_ADDR                                     MT6336_PMIC_ANA_TYPEC_DA_INTF1
#define MT6336_RG_DA_PD_TX_DATA_MASK                                     0x1
#define MT6336_RG_DA_PD_TX_DATA_SHIFT                                    0
#define MT6336_RG_DA_PD_TX_EN_ADDR                                       MT6336_PMIC_ANA_TYPEC_DA_INTF1
#define MT6336_RG_DA_PD_TX_EN_MASK                                       0x1
#define MT6336_RG_DA_PD_TX_EN_SHIFT                                      1
#define MT6336_RG_DA_PD_BIAS_EN_ADDR                                     MT6336_PMIC_ANA_TYPEC_DA_INTF1
#define MT6336_RG_DA_PD_BIAS_EN_MASK                                     0x1
#define MT6336_RG_DA_PD_BIAS_EN_SHIFT                                    2
#define MT6336_RG_DA_CC_DAC_EN_ADDR                                      MT6336_PMIC_ANA_TYPEC_DA_INTF1
#define MT6336_RG_DA_CC_DAC_EN_MASK                                      0x1
#define MT6336_RG_DA_CC_DAC_EN_SHIFT                                     3
#define MT6336_RG_DA_CC_SASW_EN_ADDR                                     MT6336_PMIC_ANA_TYPEC_DA_INTF1
#define MT6336_RG_DA_CC_SASW_EN_MASK                                     0x1
#define MT6336_RG_DA_CC_SASW_EN_SHIFT                                    4
#define MT6336_RG_DA_CC_ADCSW_EN_ADDR                                    MT6336_PMIC_ANA_TYPEC_DA_INTF1
#define MT6336_RG_DA_CC_ADCSW_EN_MASK                                    0x1
#define MT6336_RG_DA_CC_ADCSW_EN_SHIFT                                   5
#define MT6336_RG_DA_CC_SW_SEL_ADDR                                      MT6336_PMIC_ANA_TYPEC_DA_INTF1
#define MT6336_RG_DA_CC_SW_SEL_MASK                                      0x1
#define MT6336_RG_DA_CC_SW_SEL_SHIFT                                     6
#define MT6336_RG_DA_CC_LEV_EN_ADDR                                      MT6336_PMIC_ANA_TYPEC_DA_INTF1
#define MT6336_RG_DA_CC_LEV_EN_MASK                                      0x1
#define MT6336_RG_DA_CC_LEV_EN_SHIFT                                     7
#define MT6336_RG_DA_CC_DAC_IN_ADDR                                      MT6336_PMIC_ANA_TYPEC_DA_INTF2
#define MT6336_RG_DA_CC_DAC_IN_MASK                                      0x3F
#define MT6336_RG_DA_CC_DAC_IN_SHIFT                                     0
#define MT6336_RG_DA_PD_RX_EN_ADDR                                       MT6336_PMIC_ANA_TYPEC_DA_INTF2
#define MT6336_RG_DA_PD_RX_EN_MASK                                       0x1
#define MT6336_RG_DA_PD_RX_EN_SHIFT                                      6
#define MT6336_RG_DA_CC_DAC_GAIN_CAL_ADDR                                MT6336_PMIC_ANA_TYPEC_DA_INTF3
#define MT6336_RG_DA_CC_DAC_GAIN_CAL_MASK                                0xF
#define MT6336_RG_DA_CC_DAC_GAIN_CAL_SHIFT                               0
#define MT6336_RG_DA_CC_DAC_CAL_ADDR                                     MT6336_PMIC_ANA_TYPEC_DA_INTF3
#define MT6336_RG_DA_CC_DAC_CAL_MASK                                     0xF
#define MT6336_RG_DA_CC_DAC_CAL_SHIFT                                    4
#define MT6336_RG_DA_PD_CONNSW_ADDR                                      MT6336_PMIC_ANA_TYPEC_DA_INTF4
#define MT6336_RG_DA_PD_CONNSW_MASK                                      0x3
#define MT6336_RG_DA_PD_CONNSW_SHIFT                                     0
#define MT6336_RG_DA_PD_CCSW_ADDR                                        MT6336_PMIC_ANA_TYPEC_DA_INTF4
#define MT6336_RG_DA_PD_CCSW_MASK                                        0x3
#define MT6336_RG_DA_PD_CCSW_SHIFT                                       2
#define MT6336_AD_QI_OTG_BVALID_ADDR                                     MT6336_PMIC_ANA_CORE_AD_RGS0
#define MT6336_AD_QI_OTG_BVALID_MASK                                     0x1
#define MT6336_AD_QI_OTG_BVALID_SHIFT                                    0
#define MT6336_AD_QI_PP_EN_IN_ADDR                                       MT6336_PMIC_ANA_CORE_AD_RGS0
#define MT6336_AD_QI_PP_EN_IN_MASK                                       0x1
#define MT6336_AD_QI_PP_EN_IN_SHIFT                                      1
#define MT6336_AD_QI_ICL150PIN_LVL_ADDR                                  MT6336_PMIC_ANA_CORE_AD_RGS0
#define MT6336_AD_QI_ICL150PIN_LVL_MASK                                  0x1
#define MT6336_AD_QI_ICL150PIN_LVL_SHIFT                                 2
#define MT6336_AD_QI_TEMP_GT_150_ADDR                                    MT6336_PMIC_ANA_CORE_AD_RGS0
#define MT6336_AD_QI_TEMP_GT_150_MASK                                    0x1
#define MT6336_AD_QI_TEMP_GT_150_SHIFT                                   3
#define MT6336_AD_QI_VIO18_READY_ADDR                                    MT6336_PMIC_ANA_CORE_AD_RGS0
#define MT6336_AD_QI_VIO18_READY_MASK                                    0x1
#define MT6336_AD_QI_VIO18_READY_SHIFT                                   4
#define MT6336_AD_QI_VDIG18_READY_ADDR                                   MT6336_PMIC_ANA_CORE_AD_RGS0
#define MT6336_AD_QI_VDIG18_READY_MASK                                   0x1
#define MT6336_AD_QI_VDIG18_READY_SHIFT                                  5
#define MT6336_AD_QI_VBGR_READY_ADDR                                     MT6336_PMIC_ANA_CORE_AD_RGS0
#define MT6336_AD_QI_VBGR_READY_MASK                                     0x1
#define MT6336_AD_QI_VBGR_READY_SHIFT                                    6
#define MT6336_AD_QI_TESTMODE_ADDR                                       MT6336_PMIC_ANA_CORE_AD_RGS0
#define MT6336_AD_QI_TESTMODE_MASK                                       0x1
#define MT6336_AD_QI_TESTMODE_SHIFT                                      7
#define MT6336_AD_QI_VLED1_OPEN_ADDR                                     MT6336_PMIC_ANA_CORE_AD_RGS1
#define MT6336_AD_QI_VLED1_OPEN_MASK                                     0x1
#define MT6336_AD_QI_VLED1_OPEN_SHIFT                                    0
#define MT6336_AD_QI_VLED2_SHORT_ADDR                                    MT6336_PMIC_ANA_CORE_AD_RGS1
#define MT6336_AD_QI_VLED2_SHORT_MASK                                    0x1
#define MT6336_AD_QI_VLED2_SHORT_SHIFT                                   1
#define MT6336_AD_QI_VLED1_SHORT_ADDR                                    MT6336_PMIC_ANA_CORE_AD_RGS1
#define MT6336_AD_QI_VLED1_SHORT_MASK                                    0x1
#define MT6336_AD_QI_VLED1_SHORT_SHIFT                                   2
#define MT6336_AD_QI_VUSB33_READY_ADDR                                   MT6336_PMIC_ANA_CORE_AD_RGS1
#define MT6336_AD_QI_VUSB33_READY_MASK                                   0x1
#define MT6336_AD_QI_VUSB33_READY_SHIFT                                  3
#define MT6336_AD_QI_VPREG_RDY_ADDR                                      MT6336_PMIC_ANA_CORE_AD_RGS1
#define MT6336_AD_QI_VPREG_RDY_MASK                                      0x1
#define MT6336_AD_QI_VPREG_RDY_SHIFT                                     4
#define MT6336_AD_QI_VBUS_GT_POR_ADDR                                    MT6336_PMIC_ANA_CORE_AD_RGS1
#define MT6336_AD_QI_VBUS_GT_POR_MASK                                    0x1
#define MT6336_AD_QI_VBUS_GT_POR_SHIFT                                   5
#define MT6336_AD_QI_VBAT_LT_V3P2_ADDR                                   MT6336_PMIC_ANA_CORE_AD_RGS1
#define MT6336_AD_QI_VBAT_LT_V3P2_MASK                                   0x1
#define MT6336_AD_QI_VBAT_LT_V3P2_SHIFT                                  6
#define MT6336_AD_QI_VBAT_LT_V2P7_ADDR                                   MT6336_PMIC_ANA_CORE_AD_RGS1
#define MT6336_AD_QI_VBAT_LT_V2P7_MASK                                   0x1
#define MT6336_AD_QI_VBAT_LT_V2P7_SHIFT                                  7
#define MT6336_AD_NI_ICHR_LT_ITERM_ADDR                                  MT6336_PMIC_ANA_CORE_AD_RGS2
#define MT6336_AD_NI_ICHR_LT_ITERM_MASK                                  0x1
#define MT6336_AD_NI_ICHR_LT_ITERM_SHIFT                                 0
#define MT6336_AD_QI_VBAT_LT_PRECC1_ADDR                                 MT6336_PMIC_ANA_CORE_AD_RGS2
#define MT6336_AD_QI_VBAT_LT_PRECC1_MASK                                 0x1
#define MT6336_AD_QI_VBAT_LT_PRECC1_SHIFT                                1
#define MT6336_AD_QI_VBAT_LT_PRECC0_ADDR                                 MT6336_PMIC_ANA_CORE_AD_RGS2
#define MT6336_AD_QI_VBAT_LT_PRECC0_MASK                                 0x1
#define MT6336_AD_QI_VBAT_LT_PRECC0_SHIFT                                2
#define MT6336_AD_QI_MBATPP_DTEST2_ADDR                                  MT6336_PMIC_ANA_CORE_AD_RGS2
#define MT6336_AD_QI_MBATPP_DTEST2_MASK                                  0x1
#define MT6336_AD_QI_MBATPP_DTEST2_SHIFT                                 3
#define MT6336_AD_QI_MBATPP_DTEST1_ADDR                                  MT6336_PMIC_ANA_CORE_AD_RGS2
#define MT6336_AD_QI_MBATPP_DTEST1_MASK                                  0x1
#define MT6336_AD_QI_MBATPP_DTEST1_SHIFT                                 4
#define MT6336_AD_QI_BC12_CMP_OUT_SW_ADDR                                MT6336_PMIC_ANA_CORE_AD_RGS2
#define MT6336_AD_QI_BC12_CMP_OUT_SW_MASK                                0x1
#define MT6336_AD_QI_BC12_CMP_OUT_SW_SHIFT                               5
#define MT6336_AD_QI_BC12_CMP_OUT_ADDR                                   MT6336_PMIC_ANA_CORE_AD_RGS2
#define MT6336_AD_QI_BC12_CMP_OUT_MASK                                   0x1
#define MT6336_AD_QI_BC12_CMP_OUT_SHIFT                                  6
#define MT6336_AD_QI_VLED2_OPEN_ADDR                                     MT6336_PMIC_ANA_CORE_AD_RGS2
#define MT6336_AD_QI_VLED2_OPEN_MASK                                     0x1
#define MT6336_AD_QI_VLED2_OPEN_SHIFT                                    7
#define MT6336_AD_QI_OTG_VBAT_UVLO_ADDR                                  MT6336_PMIC_ANA_CORE_AD_RGS3
#define MT6336_AD_QI_OTG_VBAT_UVLO_MASK                                  0x1
#define MT6336_AD_QI_OTG_VBAT_UVLO_SHIFT                                 0
#define MT6336_AD_NI_VBAT_OVP_ADDR                                       MT6336_PMIC_ANA_CORE_AD_RGS3
#define MT6336_AD_NI_VBAT_OVP_MASK                                       0x1
#define MT6336_AD_NI_VBAT_OVP_SHIFT                                      1
#define MT6336_AD_NI_VSYS_OVP_ADDR                                       MT6336_PMIC_ANA_CORE_AD_RGS3
#define MT6336_AD_NI_VSYS_OVP_MASK                                       0x1
#define MT6336_AD_NI_VSYS_OVP_SHIFT                                      2
#define MT6336_AD_NI_WEAKBUS_ADDR                                        MT6336_PMIC_ANA_CORE_AD_RGS3
#define MT6336_AD_NI_WEAKBUS_MASK                                        0x1
#define MT6336_AD_NI_WEAKBUS_SHIFT                                       3
#define MT6336_AD_QI_VBUS_UVLO_ADDR                                      MT6336_PMIC_ANA_CORE_AD_RGS3
#define MT6336_AD_QI_VBUS_UVLO_MASK                                      0x1
#define MT6336_AD_QI_VBUS_UVLO_SHIFT                                     4
#define MT6336_AD_QI_VBUS_OVP_ADDR                                       MT6336_PMIC_ANA_CORE_AD_RGS3
#define MT6336_AD_QI_VBUS_OVP_MASK                                       0x1
#define MT6336_AD_QI_VBUS_OVP_SHIFT                                      5
#define MT6336_AD_QI_MBATPP_DIS_OC_ADDR                                  MT6336_PMIC_ANA_CORE_AD_RGS3
#define MT6336_AD_QI_MBATPP_DIS_OC_MASK                                  0x1
#define MT6336_AD_QI_MBATPP_DIS_OC_SHIFT                                 6
#define MT6336_AD_NS_VPRECHG_FAIL_ADDR                                   MT6336_PMIC_ANA_CORE_AD_RGS3
#define MT6336_AD_NS_VPRECHG_FAIL_MASK                                   0x1
#define MT6336_AD_NS_VPRECHG_FAIL_SHIFT                                  7
#define MT6336_AD_QI_PAM_MODE_ADDR                                       MT6336_PMIC_ANA_CORE_AD_RGS4
#define MT6336_AD_QI_PAM_MODE_MASK                                       0x1
#define MT6336_AD_QI_PAM_MODE_SHIFT                                      0
#define MT6336_AD_QI_CV_MODE_ADDR                                        MT6336_PMIC_ANA_CORE_AD_RGS4
#define MT6336_AD_QI_CV_MODE_MASK                                        0x1
#define MT6336_AD_QI_CV_MODE_SHIFT                                       1
#define MT6336_AD_QI_ICC_MODE_ADDR                                       MT6336_PMIC_ANA_CORE_AD_RGS4
#define MT6336_AD_QI_ICC_MODE_MASK                                       0x1
#define MT6336_AD_QI_ICC_MODE_SHIFT                                      2
#define MT6336_AD_QI_ICL_MODE_ADDR                                       MT6336_PMIC_ANA_CORE_AD_RGS4
#define MT6336_AD_QI_ICL_MODE_MASK                                       0x1
#define MT6336_AD_QI_ICL_MODE_SHIFT                                      3
#define MT6336_AD_QI_FLASH_VFLA_OVP_ADDR                                 MT6336_PMIC_ANA_CORE_AD_RGS4
#define MT6336_AD_QI_FLASH_VFLA_OVP_MASK                                 0x1
#define MT6336_AD_QI_FLASH_VFLA_OVP_SHIFT                                4
#define MT6336_AD_QI_FLASH_VFLA_UVLO_ADDR                                MT6336_PMIC_ANA_CORE_AD_RGS4
#define MT6336_AD_QI_FLASH_VFLA_UVLO_MASK                                0x1
#define MT6336_AD_QI_FLASH_VFLA_UVLO_SHIFT                               5
#define MT6336_AD_QI_OTG_VM_OVP_ADDR                                     MT6336_PMIC_ANA_CORE_AD_RGS4
#define MT6336_AD_QI_OTG_VM_OVP_MASK                                     0x1
#define MT6336_AD_QI_OTG_VM_OVP_SHIFT                                    6
#define MT6336_AD_QI_OTG_VM_UVLO_ADDR                                    MT6336_PMIC_ANA_CORE_AD_RGS4
#define MT6336_AD_QI_OTG_VM_UVLO_MASK                                    0x1
#define MT6336_AD_QI_OTG_VM_UVLO_SHIFT                                   7
#define MT6336_AD_QI_SWCHR_OC_STATUS_ADDR                                MT6336_PMIC_ANA_CORE_AD_RGS5
#define MT6336_AD_QI_SWCHR_OC_STATUS_MASK                                0x1
#define MT6336_AD_QI_SWCHR_OC_STATUS_SHIFT                               0
#define MT6336_AD_QI_BOOT_UVLO_ADDR                                      MT6336_PMIC_ANA_CORE_AD_RGS5
#define MT6336_AD_QI_BOOT_UVLO_MASK                                      0x1
#define MT6336_AD_QI_BOOT_UVLO_SHIFT                                     1
#define MT6336_AD_QI_ZX_SWCHR_ZCD_FLAG_ADDR                              MT6336_PMIC_ANA_CORE_AD_RGS5
#define MT6336_AD_QI_ZX_SWCHR_ZCD_FLAG_MASK                              0x1
#define MT6336_AD_QI_ZX_SWCHR_ZCD_FLAG_SHIFT                             2
#define MT6336_AD_NI_ZX_OTG_TEST_ADDR                                    MT6336_PMIC_ANA_CORE_AD_RGS5
#define MT6336_AD_NI_ZX_OTG_TEST_MASK                                    0x1
#define MT6336_AD_NI_ZX_OTG_TEST_SHIFT                                   3
#define MT6336_AD_NI_ZX_FLASH_TEST_ADDR                                  MT6336_PMIC_ANA_CORE_AD_RGS5
#define MT6336_AD_NI_ZX_FLASH_TEST_MASK                                  0x1
#define MT6336_AD_NI_ZX_FLASH_TEST_SHIFT                                 4
#define MT6336_AD_NI_ZX_SWCHR_TEST_ADDR                                  MT6336_PMIC_ANA_CORE_AD_RGS5
#define MT6336_AD_NI_ZX_SWCHR_TEST_MASK                                  0x1
#define MT6336_AD_NI_ZX_SWCHR_TEST_SHIFT                                 5
#define MT6336_AD_QI_OTG_OLP_ADDR                                        MT6336_PMIC_ANA_CORE_AD_RGS5
#define MT6336_AD_QI_OTG_OLP_MASK                                        0x1
#define MT6336_AD_QI_OTG_OLP_SHIFT                                       6
#define MT6336_AD_QI_THR_MODE_ADDR                                       MT6336_PMIC_ANA_CORE_AD_RGS5
#define MT6336_AD_QI_THR_MODE_MASK                                       0x1
#define MT6336_AD_QI_THR_MODE_SHIFT                                      7
#define MT6336_AD_QI_VADC18_READY_ADDR                                   MT6336_PMIC_ANA_CORE_AD_RGS6
#define MT6336_AD_QI_VADC18_READY_MASK                                   0x1
#define MT6336_AD_QI_VADC18_READY_SHIFT                                  0
#define MT6336_AD_QI_SWCHR_ENPWM_STATUS_ADDR                             MT6336_PMIC_ANA_CORE_AD_RGS6
#define MT6336_AD_QI_SWCHR_ENPWM_STATUS_MASK                             0x1
#define MT6336_AD_QI_SWCHR_ENPWM_STATUS_SHIFT                            1
#define MT6336_AD_QI_SWCHR_OCASYN_STATUS_ADDR                            MT6336_PMIC_ANA_CORE_AD_RGS6
#define MT6336_AD_QI_SWCHR_OCASYN_STATUS_MASK                            0x1
#define MT6336_AD_QI_SWCHR_OCASYN_STATUS_SHIFT                           2
#define MT6336_RESERVED_1_ADDR                                           MT6336_PMIC_RESERVED_1
#define MT6336_RESERVED_1_MASK                                           0xFF
#define MT6336_RESERVED_1_SHIFT                                          0
#define MT6336_DA_QI_OTG_MODE_DB_MUX_ADDR                                MT6336_PMIC_ANA_CORE_DA_RGS0
#define MT6336_DA_QI_OTG_MODE_DB_MUX_MASK                                0x1
#define MT6336_DA_QI_OTG_MODE_DB_MUX_SHIFT                               0
#define MT6336_DA_QI_FLASH_MODE_DB_MUX_ADDR                              MT6336_PMIC_ANA_CORE_DA_RGS0
#define MT6336_DA_QI_FLASH_MODE_DB_MUX_MASK                              0x1
#define MT6336_DA_QI_FLASH_MODE_DB_MUX_SHIFT                             1
#define MT6336_DA_QI_BUCK_MODE_DB_MUX_ADDR                               MT6336_PMIC_ANA_CORE_DA_RGS0
#define MT6336_DA_QI_BUCK_MODE_DB_MUX_MASK                               0x1
#define MT6336_DA_QI_BUCK_MODE_DB_MUX_SHIFT                              2
#define MT6336_DA_QI_BASE_READY_DB_MUX_ADDR                              MT6336_PMIC_ANA_CORE_DA_RGS0
#define MT6336_DA_QI_BASE_READY_DB_MUX_MASK                              0x1
#define MT6336_DA_QI_BASE_READY_DB_MUX_SHIFT                             3
#define MT6336_DA_QI_BASE_READY_MUX_ADDR                                 MT6336_PMIC_ANA_CORE_DA_RGS0
#define MT6336_DA_QI_BASE_READY_MUX_MASK                                 0x1
#define MT6336_DA_QI_BASE_READY_MUX_SHIFT                                4
#define MT6336_DA_QI_LOWQ_STAT_MUX_ADDR                                  MT6336_PMIC_ANA_CORE_DA_RGS0
#define MT6336_DA_QI_LOWQ_STAT_MUX_MASK                                  0x1
#define MT6336_DA_QI_LOWQ_STAT_MUX_SHIFT                                 5
#define MT6336_DA_QI_SHIP_STAT_MUX_ADDR                                  MT6336_PMIC_ANA_CORE_DA_RGS0
#define MT6336_DA_QI_SHIP_STAT_MUX_MASK                                  0x1
#define MT6336_DA_QI_SHIP_STAT_MUX_SHIFT                                 6
#define MT6336_DA_QI_EN_CP_HIQ_MUX_ADDR                                  MT6336_PMIC_ANA_CORE_DA_RGS1
#define MT6336_DA_QI_EN_CP_HIQ_MUX_MASK                                  0x1
#define MT6336_DA_QI_EN_CP_HIQ_MUX_SHIFT                                 0
#define MT6336_DA_QI_VBAT_LT_PRECC1_DB_MUX_ADDR                          MT6336_PMIC_ANA_CORE_DA_RGS1
#define MT6336_DA_QI_VBAT_LT_PRECC1_DB_MUX_MASK                          0x1
#define MT6336_DA_QI_VBAT_LT_PRECC1_DB_MUX_SHIFT                         1
#define MT6336_DA_QI_VBAT_LT_PRECC0_DB_MUX_ADDR                          MT6336_PMIC_ANA_CORE_DA_RGS1
#define MT6336_DA_QI_VBAT_LT_PRECC0_DB_MUX_MASK                          0x1
#define MT6336_DA_QI_VBAT_LT_PRECC0_DB_MUX_SHIFT                         2
#define MT6336_DA_QI_EFUSE_READ_RDY_DLY_MUX_ADDR                         MT6336_PMIC_ANA_CORE_DA_RGS1
#define MT6336_DA_QI_EFUSE_READ_RDY_DLY_MUX_MASK                         0x1
#define MT6336_DA_QI_EFUSE_READ_RDY_DLY_MUX_SHIFT                        3
#define MT6336_DA_QI_VBGR_READY_DB_MUX_ADDR                              MT6336_PMIC_ANA_CORE_DA_RGS1
#define MT6336_DA_QI_VBGR_READY_DB_MUX_MASK                              0x1
#define MT6336_DA_QI_VBGR_READY_DB_MUX_SHIFT                             4
#define MT6336_DA_QI_TORCH_MODE_DRV_EN_MUX_ADDR                          MT6336_PMIC_ANA_CORE_DA_RGS1
#define MT6336_DA_QI_TORCH_MODE_DRV_EN_MUX_MASK                          0x1
#define MT6336_DA_QI_TORCH_MODE_DRV_EN_MUX_SHIFT                         5
#define MT6336_DA_QI_TORCH_MODE_DB_MUX_ADDR                              MT6336_PMIC_ANA_CORE_DA_RGS1
#define MT6336_DA_QI_TORCH_MODE_DB_MUX_MASK                              0x1
#define MT6336_DA_QI_TORCH_MODE_DB_MUX_SHIFT                             6
#define MT6336_DA_QI_TORCH_MODE_MUX_ADDR                                 MT6336_PMIC_ANA_CORE_DA_RGS1
#define MT6336_DA_QI_TORCH_MODE_MUX_MASK                                 0x1
#define MT6336_DA_QI_TORCH_MODE_MUX_SHIFT                                7
#define MT6336_DA_QI_VBAT_IDIS_EN_MUX_ADDR                               MT6336_PMIC_ANA_CORE_DA_RGS2
#define MT6336_DA_QI_VBAT_IDIS_EN_MUX_MASK                               0x1
#define MT6336_DA_QI_VBAT_IDIS_EN_MUX_SHIFT                              0
#define MT6336_DA_QI_IPRECC1_EN_MUX_ADDR                                 MT6336_PMIC_ANA_CORE_DA_RGS2
#define MT6336_DA_QI_IPRECC1_EN_MUX_MASK                                 0x1
#define MT6336_DA_QI_IPRECC1_EN_MUX_SHIFT                                1
#define MT6336_DA_QI_IPRECC0_EN_MUX_ADDR                                 MT6336_PMIC_ANA_CORE_DA_RGS2
#define MT6336_DA_QI_IPRECC0_EN_MUX_MASK                                 0x1
#define MT6336_DA_QI_IPRECC0_EN_MUX_SHIFT                                2
#define MT6336_DA_QI_FETON_OK_LOQCP_EN_MUX_ADDR                          MT6336_PMIC_ANA_CORE_DA_RGS2
#define MT6336_DA_QI_FETON_OK_LOQCP_EN_MUX_MASK                          0x1
#define MT6336_DA_QI_FETON_OK_LOQCP_EN_MUX_SHIFT                         3
#define MT6336_DA_QI_EN_VDC_IBIAS_MUX_ADDR                               MT6336_PMIC_ANA_CORE_DA_RGS2
#define MT6336_DA_QI_EN_VDC_IBIAS_MUX_MASK                               0x1
#define MT6336_DA_QI_EN_VDC_IBIAS_MUX_SHIFT                              4
#define MT6336_DA_QI_VPREG_RDY_DB_MUX_ADDR                               MT6336_PMIC_ANA_CORE_DA_RGS2
#define MT6336_DA_QI_VPREG_RDY_DB_MUX_MASK                               0x1
#define MT6336_DA_QI_VPREG_RDY_DB_MUX_SHIFT                              5
#define MT6336_DA_QI_EN_IBIASGEN_MUX_ADDR                                MT6336_PMIC_ANA_CORE_DA_RGS2
#define MT6336_DA_QI_EN_IBIASGEN_MUX_MASK                                0x1
#define MT6336_DA_QI_EN_IBIASGEN_MUX_SHIFT                               6
#define MT6336_DA_NS_VPRECHG_FAIL_DB_MUX_ADDR                            MT6336_PMIC_ANA_CORE_DA_RGS2
#define MT6336_DA_NS_VPRECHG_FAIL_DB_MUX_MASK                            0x1
#define MT6336_DA_NS_VPRECHG_FAIL_DB_MUX_SHIFT                           7
#define MT6336_DA_QI_CV_REGNODE_SEL_MUX_ADDR                             MT6336_PMIC_ANA_CORE_DA_RGS3
#define MT6336_DA_QI_CV_REGNODE_SEL_MUX_MASK                             0x1
#define MT6336_DA_QI_CV_REGNODE_SEL_MUX_SHIFT                            0
#define MT6336_DA_QI_CHR_DET_MUX_ADDR                                    MT6336_PMIC_ANA_CORE_DA_RGS3
#define MT6336_DA_QI_CHR_DET_MUX_MASK                                    0x1
#define MT6336_DA_QI_CHR_DET_MUX_SHIFT                                   1
#define MT6336_DA_QI_REVFET_EN_MUX_ADDR                                  MT6336_PMIC_ANA_CORE_DA_RGS3
#define MT6336_DA_QI_REVFET_EN_MUX_MASK                                  0x1
#define MT6336_DA_QI_REVFET_EN_MUX_SHIFT                                 2
#define MT6336_DA_QI_IBACKBOOST_EN_MUX_ADDR                              MT6336_PMIC_ANA_CORE_DA_RGS3
#define MT6336_DA_QI_IBACKBOOST_EN_MUX_MASK                              0x1
#define MT6336_DA_QI_IBACKBOOST_EN_MUX_SHIFT                             3
#define MT6336_DA_NI_WEAKBUS_DB_MUX_ADDR                                 MT6336_PMIC_ANA_CORE_DA_RGS3
#define MT6336_DA_NI_WEAKBUS_DB_MUX_MASK                                 0x1
#define MT6336_DA_NI_WEAKBUS_DB_MUX_SHIFT                                4
#define MT6336_DA_QI_VBUS_OVP_DB_MUX_ADDR                                MT6336_PMIC_ANA_CORE_DA_RGS3
#define MT6336_DA_QI_VBUS_OVP_DB_MUX_MASK                                0x1
#define MT6336_DA_QI_VBUS_OVP_DB_MUX_SHIFT                               5
#define MT6336_DA_QI_VBUS_UVLO_DB_MUX_ADDR                               MT6336_PMIC_ANA_CORE_DA_RGS3
#define MT6336_DA_QI_VBUS_UVLO_DB_MUX_MASK                               0x1
#define MT6336_DA_QI_VBUS_UVLO_DB_MUX_SHIFT                              6
#define MT6336_DA_QI_VSYS_IDIS_EN_MUX_ADDR                               MT6336_PMIC_ANA_CORE_DA_RGS3
#define MT6336_DA_QI_VSYS_IDIS_EN_MUX_MASK                               0x1
#define MT6336_DA_QI_VSYS_IDIS_EN_MUX_SHIFT                              7
#define MT6336_DA_QI_ICL_ITH_MUX_ADDR                                    MT6336_PMIC_ANA_CORE_DA_RGS4
#define MT6336_DA_QI_ICL_ITH_MUX_MASK                                    0x3F
#define MT6336_DA_QI_ICL_ITH_MUX_SHIFT                                   0
#define MT6336_DA_QI_ICC_ITH_MUX_ADDR                                    MT6336_PMIC_ANA_CORE_DA_RGS5
#define MT6336_DA_QI_ICC_ITH_MUX_MASK                                    0x7F
#define MT6336_DA_QI_ICC_ITH_MUX_SHIFT                                   0
#define MT6336_DA_QI_VSYSREG_VTH_MUX_ADDR                                MT6336_PMIC_ANA_CORE_DA_RGS6
#define MT6336_DA_QI_VSYSREG_VTH_MUX_MASK                                0xFF
#define MT6336_DA_QI_VSYSREG_VTH_MUX_SHIFT                               0
#define MT6336_DA_QI_VCV_VTH_MUX_ADDR                                    MT6336_PMIC_ANA_CORE_DA_RGS7
#define MT6336_DA_QI_VCV_VTH_MUX_MASK                                    0xFF
#define MT6336_DA_QI_VCV_VTH_MUX_SHIFT                                   0
#define MT6336_DA_QI_VPAM_VTH_MUX_ADDR                                   MT6336_PMIC_ANA_CORE_DA_RGS8
#define MT6336_DA_QI_VPAM_VTH_MUX_MASK                                   0x1F
#define MT6336_DA_QI_VPAM_VTH_MUX_SHIFT                                  0
#define MT6336_DA_QI_VFLA_VTH_MUX_ADDR                                   MT6336_PMIC_ANA_CORE_DA_RGS9
#define MT6336_DA_QI_VFLA_VTH_MUX_MASK                                   0xF
#define MT6336_DA_QI_VFLA_VTH_MUX_SHIFT                                  0
#define MT6336_DA_QI_OTG_VCV_VTH_MUX_ADDR                                MT6336_PMIC_ANA_CORE_DA_RGS9
#define MT6336_DA_QI_OTG_VCV_VTH_MUX_MASK                                0xF
#define MT6336_DA_QI_OTG_VCV_VTH_MUX_SHIFT                               4
#define MT6336_DA_QI_OTG_VM_UVLO_DB_MUX_ADDR                             MT6336_PMIC_ANA_CORE_DA_RGS11
#define MT6336_DA_QI_OTG_VM_UVLO_DB_MUX_MASK                             0x1
#define MT6336_DA_QI_OTG_VM_UVLO_DB_MUX_SHIFT                            0
#define MT6336_DA_QI_OTG_VM_OVP_DB_MUX_ADDR                              MT6336_PMIC_ANA_CORE_DA_RGS11
#define MT6336_DA_QI_OTG_VM_OVP_DB_MUX_MASK                              0x1
#define MT6336_DA_QI_OTG_VM_OVP_DB_MUX_SHIFT                             1
#define MT6336_DA_QI_BUCK_MODE_MUX_ADDR                                  MT6336_PMIC_ANA_CORE_DA_RGS11
#define MT6336_DA_QI_BUCK_MODE_MUX_MASK                                  0x1
#define MT6336_DA_QI_BUCK_MODE_MUX_SHIFT                                 2
#define MT6336_DA_QI_BUCK_MODE_DRV_EN_MUX_ADDR                           MT6336_PMIC_ANA_CORE_DA_RGS11
#define MT6336_DA_QI_BUCK_MODE_DRV_EN_MUX_MASK                           0x1
#define MT6336_DA_QI_BUCK_MODE_DRV_EN_MUX_SHIFT                          3
#define MT6336_DA_QI_FLASH_MODE_MUX_ADDR                                 MT6336_PMIC_ANA_CORE_DA_RGS11
#define MT6336_DA_QI_FLASH_MODE_MUX_MASK                                 0x1
#define MT6336_DA_QI_FLASH_MODE_MUX_SHIFT                                4
#define MT6336_DA_QI_FLASH_MODE_DRV_EN_MUX_ADDR                          MT6336_PMIC_ANA_CORE_DA_RGS11
#define MT6336_DA_QI_FLASH_MODE_DRV_EN_MUX_MASK                          0x1
#define MT6336_DA_QI_FLASH_MODE_DRV_EN_MUX_SHIFT                         5
#define MT6336_DA_QI_OTG_MODE_MUX_ADDR                                   MT6336_PMIC_ANA_CORE_DA_RGS11
#define MT6336_DA_QI_OTG_MODE_MUX_MASK                                   0x1
#define MT6336_DA_QI_OTG_MODE_MUX_SHIFT                                  6
#define MT6336_DA_QI_OTG_MODE_DRV_EN_MUX_ADDR                            MT6336_PMIC_ANA_CORE_DA_RGS11
#define MT6336_DA_QI_OTG_MODE_DRV_EN_MUX_MASK                            0x1
#define MT6336_DA_QI_OTG_MODE_DRV_EN_MUX_SHIFT                           7
#define MT6336_DA_QI_POSTPRECC1_STAT_MUX_ADDR                            MT6336_PMIC_ANA_CORE_DA_RGS12
#define MT6336_DA_QI_POSTPRECC1_STAT_MUX_MASK                            0x1
#define MT6336_DA_QI_POSTPRECC1_STAT_MUX_SHIFT                           0
#define MT6336_DA_QI_VBUS_PLUGIN_MUX_ADDR                                MT6336_PMIC_ANA_CORE_DA_RGS12
#define MT6336_DA_QI_VBUS_PLUGIN_MUX_MASK                                0x1
#define MT6336_DA_QI_VBUS_PLUGIN_MUX_SHIFT                               1
#define MT6336_DA_QI_VSYS_LT_FETON_DB_MUX_ADDR                           MT6336_PMIC_ANA_CORE_DA_RGS12
#define MT6336_DA_QI_VSYS_LT_FETON_DB_MUX_MASK                           0x1
#define MT6336_DA_QI_VSYS_LT_FETON_DB_MUX_SHIFT                          2
#define MT6336_DA_QI_OTG_VBAT_UVLO_DB_MUX_ADDR                           MT6336_PMIC_ANA_CORE_DA_RGS12
#define MT6336_DA_QI_OTG_VBAT_UVLO_DB_MUX_MASK                           0x1
#define MT6336_DA_QI_OTG_VBAT_UVLO_DB_MUX_SHIFT                          3
#define MT6336_DA_NI_VBAT_OVP_DB_MUX_ADDR                                MT6336_PMIC_ANA_CORE_DA_RGS12
#define MT6336_DA_NI_VBAT_OVP_DB_MUX_MASK                                0x1
#define MT6336_DA_NI_VBAT_OVP_DB_MUX_SHIFT                               4
#define MT6336_DA_NI_VSYS_OVP_DB_MUX_ADDR                                MT6336_PMIC_ANA_CORE_DA_RGS12
#define MT6336_DA_NI_VSYS_OVP_DB_MUX_MASK                                0x1
#define MT6336_DA_NI_VSYS_OVP_DB_MUX_SHIFT                               5
#define MT6336_DA_QI_FLASH_VFLA_OVP_DB_MUX_ADDR                          MT6336_PMIC_ANA_CORE_DA_RGS12
#define MT6336_DA_QI_FLASH_VFLA_OVP_DB_MUX_MASK                          0x1
#define MT6336_DA_QI_FLASH_VFLA_OVP_DB_MUX_SHIFT                         6
#define MT6336_DA_QI_FLASH_VFLA_UVLO_DB_MUX_ADDR                         MT6336_PMIC_ANA_CORE_DA_RGS12
#define MT6336_DA_QI_FLASH_VFLA_UVLO_DB_MUX_MASK                         0x1
#define MT6336_DA_QI_FLASH_VFLA_UVLO_DB_MUX_SHIFT                        7
#define MT6336_DA_QI_BACKGROUND_STAT_MUX_ADDR                            MT6336_PMIC_ANA_CORE_DA_RGS13
#define MT6336_DA_QI_BACKGROUND_STAT_MUX_MASK                            0x1
#define MT6336_DA_QI_BACKGROUND_STAT_MUX_SHIFT                           0
#define MT6336_DA_QI_DEADBAT_STAT_MUX_ADDR                               MT6336_PMIC_ANA_CORE_DA_RGS13
#define MT6336_DA_QI_DEADBAT_STAT_MUX_MASK                               0x1
#define MT6336_DA_QI_DEADBAT_STAT_MUX_SHIFT                              1
#define MT6336_DA_QI_EOC_STAT_MUX_ADDR                                   MT6336_PMIC_ANA_CORE_DA_RGS13
#define MT6336_DA_QI_EOC_STAT_MUX_MASK                                   0x1
#define MT6336_DA_QI_EOC_STAT_MUX_SHIFT                                  2
#define MT6336_DA_QI_FASTCC_STAT_MUX_ADDR                                MT6336_PMIC_ANA_CORE_DA_RGS13
#define MT6336_DA_QI_FASTCC_STAT_MUX_MASK                                0x1
#define MT6336_DA_QI_FASTCC_STAT_MUX_SHIFT                               3
#define MT6336_RESERVED_2_ADDR                                           MT6336_PMIC_RESERVED_2
#define MT6336_RESERVED_2_MASK                                           0xFF
#define MT6336_RESERVED_2_SHIFT                                          0
#define MT6336_RESERVED_3_ADDR                                           MT6336_PMIC_RESERVED_3
#define MT6336_RESERVED_3_MASK                                           0xFF
#define MT6336_RESERVED_3_SHIFT                                          0
#define MT6336_RESERVED_4_ADDR                                           MT6336_PMIC_RESERVED_4
#define MT6336_RESERVED_4_MASK                                           0xFF
#define MT6336_RESERVED_4_SHIFT                                          0
#define MT6336_RESERVED_5_ADDR                                           MT6336_PMIC_RESERVED_5
#define MT6336_RESERVED_5_MASK                                           0xFF
#define MT6336_RESERVED_5_SHIFT                                          0
#define MT6336_RESERVED_6_ADDR                                           MT6336_PMIC_RESERVED_6
#define MT6336_RESERVED_6_MASK                                           0xFF
#define MT6336_RESERVED_6_SHIFT                                          0
#define MT6336_RESERVED_7_ADDR                                           MT6336_PMIC_RESERVED_7
#define MT6336_RESERVED_7_MASK                                           0xFF
#define MT6336_RESERVED_7_SHIFT                                          0
#define MT6336_RESERVED_8_ADDR                                           MT6336_PMIC_RESERVED_8
#define MT6336_RESERVED_8_MASK                                           0xFF
#define MT6336_RESERVED_8_SHIFT                                          0
#define MT6336_RESERVED_9_ADDR                                           MT6336_PMIC_RESERVED_9
#define MT6336_RESERVED_9_MASK                                           0xFF
#define MT6336_RESERVED_9_SHIFT                                          0
#define MT6336_RESERVED_10_ADDR                                          MT6336_PMIC_RESERVED_10
#define MT6336_RESERVED_10_MASK                                          0xFF
#define MT6336_RESERVED_10_SHIFT                                         0
#define MT6336_RESERVED_11_ADDR                                          MT6336_PMIC_RESERVED_11
#define MT6336_RESERVED_11_MASK                                          0xFF
#define MT6336_RESERVED_11_SHIFT                                         0
#define MT6336_RESERVED_12_ADDR                                          MT6336_PMIC_RESERVED_12
#define MT6336_RESERVED_12_MASK                                          0xFF
#define MT6336_RESERVED_12_SHIFT                                         0
#define MT6336_RESERVED_13_ADDR                                          MT6336_PMIC_RESERVED_13
#define MT6336_RESERVED_13_MASK                                          0xFF
#define MT6336_RESERVED_13_SHIFT                                         0
#define MT6336_RESERVED_14_ADDR                                          MT6336_PMIC_RESERVED_14
#define MT6336_RESERVED_14_MASK                                          0xFF
#define MT6336_RESERVED_14_SHIFT                                         0
#define MT6336_RESERVED_15_ADDR                                          MT6336_PMIC_RESERVED_15
#define MT6336_RESERVED_15_MASK                                          0xFF
#define MT6336_RESERVED_15_SHIFT                                         0
#define MT6336_RESERVED_16_ADDR                                          MT6336_PMIC_RESERVED_16
#define MT6336_RESERVED_16_MASK                                          0xFF
#define MT6336_RESERVED_16_SHIFT                                         0
#define MT6336_RESERVED_17_ADDR                                          MT6336_PMIC_RESERVED_17
#define MT6336_RESERVED_17_MASK                                          0xFF
#define MT6336_RESERVED_17_SHIFT                                         0
#define MT6336_RESERVED_18_ADDR                                          MT6336_PMIC_RESERVED_18
#define MT6336_RESERVED_18_MASK                                          0xFF
#define MT6336_RESERVED_18_SHIFT                                         0
#define MT6336_RESERVED_19_ADDR                                          MT6336_PMIC_RESERVED_19
#define MT6336_RESERVED_19_MASK                                          0xFF
#define MT6336_RESERVED_19_SHIFT                                         0
#define MT6336_RESERVED_20_ADDR                                          MT6336_PMIC_RESERVED_20
#define MT6336_RESERVED_20_MASK                                          0xFF
#define MT6336_RESERVED_20_SHIFT                                         0
#define MT6336_RESERVED_21_ADDR                                          MT6336_PMIC_RESERVED_21
#define MT6336_RESERVED_21_MASK                                          0xFF
#define MT6336_RESERVED_21_SHIFT                                         0
#define MT6336_RESERVED_22_ADDR                                          MT6336_PMIC_RESERVED_22
#define MT6336_RESERVED_22_MASK                                          0xFF
#define MT6336_RESERVED_22_SHIFT                                         0
#define MT6336_RESERVED_23_ADDR                                          MT6336_PMIC_RESERVED_23
#define MT6336_RESERVED_23_MASK                                          0xFF
#define MT6336_RESERVED_23_SHIFT                                         0
#define MT6336_RESERVED_24_ADDR                                          MT6336_PMIC_RESERVED_24
#define MT6336_RESERVED_24_MASK                                          0xFF
#define MT6336_RESERVED_24_SHIFT                                         0
#define MT6336_RESERVED_25_ADDR                                          MT6336_PMIC_RESERVED_25
#define MT6336_RESERVED_25_MASK                                          0xFF
#define MT6336_RESERVED_25_SHIFT                                         0
#define MT6336_RESERVED_26_ADDR                                          MT6336_PMIC_RESERVED_26
#define MT6336_RESERVED_26_MASK                                          0xFF
#define MT6336_RESERVED_26_SHIFT                                         0
#define MT6336_RESERVED_27_ADDR                                          MT6336_PMIC_RESERVED_27
#define MT6336_RESERVED_27_MASK                                          0xFF
#define MT6336_RESERVED_27_SHIFT                                         0
#define MT6336_RESERVED_28_ADDR                                          MT6336_PMIC_RESERVED_28
#define MT6336_RESERVED_28_MASK                                          0xFF
#define MT6336_RESERVED_28_SHIFT                                         0
#define MT6336_RESERVED_29_ADDR                                          MT6336_PMIC_RESERVED_29
#define MT6336_RESERVED_29_MASK                                          0xFF
#define MT6336_RESERVED_29_SHIFT                                         0
#define MT6336_RESERVED_30_ADDR                                          MT6336_PMIC_RESERVED_30
#define MT6336_RESERVED_30_MASK                                          0xFF
#define MT6336_RESERVED_30_SHIFT                                         0
#define MT6336_RESERVED_31_ADDR                                          MT6336_PMIC_RESERVED_31
#define MT6336_RESERVED_31_MASK                                          0xFF
#define MT6336_RESERVED_31_SHIFT                                         0
#define MT6336_RESERVED_32_ADDR                                          MT6336_PMIC_RESERVED_32
#define MT6336_RESERVED_32_MASK                                          0xFF
#define MT6336_RESERVED_32_SHIFT                                         0
#define MT6336_RESERVED_33_ADDR                                          MT6336_PMIC_RESERVED_33
#define MT6336_RESERVED_33_MASK                                          0xFF
#define MT6336_RESERVED_33_SHIFT                                         0
#define MT6336_RESERVED_34_ADDR                                          MT6336_PMIC_RESERVED_34
#define MT6336_RESERVED_34_MASK                                          0xFF
#define MT6336_RESERVED_34_SHIFT                                         0
#define MT6336_RESERVED_35_ADDR                                          MT6336_PMIC_RESERVED_35
#define MT6336_RESERVED_35_MASK                                          0xFF
#define MT6336_RESERVED_35_SHIFT                                         0
#define MT6336_RESERVED_36_ADDR                                          MT6336_PMIC_RESERVED_36
#define MT6336_RESERVED_36_MASK                                          0xFF
#define MT6336_RESERVED_36_SHIFT                                         0
#define MT6336_RESERVED_37_ADDR                                          MT6336_PMIC_RESERVED_37
#define MT6336_RESERVED_37_MASK                                          0xFF
#define MT6336_RESERVED_37_SHIFT                                         0
#define MT6336_RESERVED_38_ADDR                                          MT6336_PMIC_RESERVED_38
#define MT6336_RESERVED_38_MASK                                          0xFF
#define MT6336_RESERVED_38_SHIFT                                         0
#define MT6336_RESERVED_39_ADDR                                          MT6336_PMIC_RESERVED_39
#define MT6336_RESERVED_39_MASK                                          0xFF
#define MT6336_RESERVED_39_SHIFT                                         0
#define MT6336_RESERVED_40_ADDR                                          MT6336_PMIC_RESERVED_40
#define MT6336_RESERVED_40_MASK                                          0xFF
#define MT6336_RESERVED_40_SHIFT                                         0
#define MT6336_RESERVED_41_ADDR                                          MT6336_PMIC_RESERVED_41
#define MT6336_RESERVED_41_MASK                                          0xFF
#define MT6336_RESERVED_41_SHIFT                                         0
#define MT6336_RESERVED_42_ADDR                                          MT6336_PMIC_RESERVED_42
#define MT6336_RESERVED_42_MASK                                          0xFF
#define MT6336_RESERVED_42_SHIFT                                         0
#define MT6336_RESERVED_43_ADDR                                          MT6336_PMIC_RESERVED_43
#define MT6336_RESERVED_43_MASK                                          0xFF
#define MT6336_RESERVED_43_SHIFT                                         0
#define MT6336_RG_AD_QI_VBAT_LT_V3P2_SEL_ADDR                            MT6336_PMIC_ANA_CORE_AD_INTF0_SEL
#define MT6336_RG_AD_QI_VBAT_LT_V3P2_SEL_MASK                            0x1
#define MT6336_RG_AD_QI_VBAT_LT_V3P2_SEL_SHIFT                           0
#define MT6336_RG_AD_QI_VBAT_LT_V2P7_SEL_ADDR                            MT6336_PMIC_ANA_CORE_AD_INTF0_SEL
#define MT6336_RG_AD_QI_VBAT_LT_V2P7_SEL_MASK                            0x1
#define MT6336_RG_AD_QI_VBAT_LT_V2P7_SEL_SHIFT                           1
#define MT6336_RG_AD_QI_OTG_BVALID_SEL_ADDR                              MT6336_PMIC_ANA_CORE_AD_INTF0_SEL
#define MT6336_RG_AD_QI_OTG_BVALID_SEL_MASK                              0x1
#define MT6336_RG_AD_QI_OTG_BVALID_SEL_SHIFT                             2
#define MT6336_RG_AD_QI_PP_EN_IN_SEL_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF0_SEL
#define MT6336_RG_AD_QI_PP_EN_IN_SEL_MASK                                0x1
#define MT6336_RG_AD_QI_PP_EN_IN_SEL_SHIFT                               3
#define MT6336_RG_AD_QI_ICL150PIN_LVL_SEL_ADDR                           MT6336_PMIC_ANA_CORE_AD_INTF0_SEL
#define MT6336_RG_AD_QI_ICL150PIN_LVL_SEL_MASK                           0x1
#define MT6336_RG_AD_QI_ICL150PIN_LVL_SEL_SHIFT                          4
#define MT6336_RG_AD_QI_TEMP_GT_150_SEL_ADDR                             MT6336_PMIC_ANA_CORE_AD_INTF0_SEL
#define MT6336_RG_AD_QI_TEMP_GT_150_SEL_MASK                             0x1
#define MT6336_RG_AD_QI_TEMP_GT_150_SEL_SHIFT                            5
#define MT6336_RG_AD_QI_VIO18_READY_SEL_ADDR                             MT6336_PMIC_ANA_CORE_AD_INTF0_SEL
#define MT6336_RG_AD_QI_VIO18_READY_SEL_MASK                             0x1
#define MT6336_RG_AD_QI_VIO18_READY_SEL_SHIFT                            6
#define MT6336_RG_AD_QI_VBGR_READY_SEL_ADDR                              MT6336_PMIC_ANA_CORE_AD_INTF0_SEL
#define MT6336_RG_AD_QI_VBGR_READY_SEL_MASK                              0x1
#define MT6336_RG_AD_QI_VBGR_READY_SEL_SHIFT                             7
#define MT6336_RG_AD_QI_VLED2_OPEN_SEL_ADDR                              MT6336_PMIC_ANA_CORE_AD_INTF1_SEL
#define MT6336_RG_AD_QI_VLED2_OPEN_SEL_MASK                              0x1
#define MT6336_RG_AD_QI_VLED2_OPEN_SEL_SHIFT                             0
#define MT6336_RG_AD_QI_VLED1_OPEN_SEL_ADDR                              MT6336_PMIC_ANA_CORE_AD_INTF1_SEL
#define MT6336_RG_AD_QI_VLED1_OPEN_SEL_MASK                              0x1
#define MT6336_RG_AD_QI_VLED1_OPEN_SEL_SHIFT                             1
#define MT6336_RG_AD_QI_VLED2_SHORT_SEL_ADDR                             MT6336_PMIC_ANA_CORE_AD_INTF1_SEL
#define MT6336_RG_AD_QI_VLED2_SHORT_SEL_MASK                             0x1
#define MT6336_RG_AD_QI_VLED2_SHORT_SEL_SHIFT                            2
#define MT6336_RG_AD_QI_VLED1_SHORT_SEL_ADDR                             MT6336_PMIC_ANA_CORE_AD_INTF1_SEL
#define MT6336_RG_AD_QI_VLED1_SHORT_SEL_MASK                             0x1
#define MT6336_RG_AD_QI_VLED1_SHORT_SEL_SHIFT                            3
#define MT6336_RG_AD_QI_VUSB33_READY_SEL_ADDR                            MT6336_PMIC_ANA_CORE_AD_INTF1_SEL
#define MT6336_RG_AD_QI_VUSB33_READY_SEL_MASK                            0x1
#define MT6336_RG_AD_QI_VUSB33_READY_SEL_SHIFT                           4
#define MT6336_RG_AD_QI_VPREG_RDY_SEL_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF1_SEL
#define MT6336_RG_AD_QI_VPREG_RDY_SEL_MASK                               0x1
#define MT6336_RG_AD_QI_VPREG_RDY_SEL_SHIFT                              5
#define MT6336_RG_AD_QI_VBUS_GT_POR_SEL_ADDR                             MT6336_PMIC_ANA_CORE_AD_INTF1_SEL
#define MT6336_RG_AD_QI_VBUS_GT_POR_SEL_MASK                             0x1
#define MT6336_RG_AD_QI_VBUS_GT_POR_SEL_SHIFT                            6
#define MT6336_RG_AD_QI_BC12_CMP_OUT_SW_SEL_ADDR                         MT6336_PMIC_ANA_CORE_AD_INTF2_SEL
#define MT6336_RG_AD_QI_BC12_CMP_OUT_SW_SEL_MASK                         0x1
#define MT6336_RG_AD_QI_BC12_CMP_OUT_SW_SEL_SHIFT                        0
#define MT6336_RG_AD_QI_BC12_CMP_OUT_SEL_ADDR                            MT6336_PMIC_ANA_CORE_AD_INTF2_SEL
#define MT6336_RG_AD_QI_BC12_CMP_OUT_SEL_MASK                            0x1
#define MT6336_RG_AD_QI_BC12_CMP_OUT_SEL_SHIFT                           1
#define MT6336_RG_AD_QI_MBATPP_DIS_OC_SEL_ADDR                           MT6336_PMIC_ANA_CORE_AD_INTF3_SEL
#define MT6336_RG_AD_QI_MBATPP_DIS_OC_SEL_MASK                           0x1
#define MT6336_RG_AD_QI_MBATPP_DIS_OC_SEL_SHIFT                          0
#define MT6336_RG_AD_NS_VPRECHG_FAIL_SEL_ADDR                            MT6336_PMIC_ANA_CORE_AD_INTF3_SEL
#define MT6336_RG_AD_NS_VPRECHG_FAIL_SEL_MASK                            0x1
#define MT6336_RG_AD_NS_VPRECHG_FAIL_SEL_SHIFT                           1
#define MT6336_RG_AD_NI_ICHR_LT_ITERM_SEL_ADDR                           MT6336_PMIC_ANA_CORE_AD_INTF3_SEL
#define MT6336_RG_AD_NI_ICHR_LT_ITERM_SEL_MASK                           0x1
#define MT6336_RG_AD_NI_ICHR_LT_ITERM_SEL_SHIFT                          2
#define MT6336_RG_AD_QI_VBAT_LT_PRECC1_SEL_ADDR                          MT6336_PMIC_ANA_CORE_AD_INTF3_SEL
#define MT6336_RG_AD_QI_VBAT_LT_PRECC1_SEL_MASK                          0x1
#define MT6336_RG_AD_QI_VBAT_LT_PRECC1_SEL_SHIFT                         3
#define MT6336_RG_AD_QI_VBAT_LT_PRECC0_SEL_ADDR                          MT6336_PMIC_ANA_CORE_AD_INTF3_SEL
#define MT6336_RG_AD_QI_VBAT_LT_PRECC0_SEL_MASK                          0x1
#define MT6336_RG_AD_QI_VBAT_LT_PRECC0_SEL_SHIFT                         4
#define MT6336_RG_AD_QI_MBATPP_DTEST2_SEL_ADDR                           MT6336_PMIC_ANA_CORE_AD_INTF3_SEL
#define MT6336_RG_AD_QI_MBATPP_DTEST2_SEL_MASK                           0x1
#define MT6336_RG_AD_QI_MBATPP_DTEST2_SEL_SHIFT                          5
#define MT6336_RG_AD_QI_MBATPP_DTEST1_SEL_ADDR                           MT6336_PMIC_ANA_CORE_AD_INTF3_SEL
#define MT6336_RG_AD_QI_MBATPP_DTEST1_SEL_MASK                           0x1
#define MT6336_RG_AD_QI_MBATPP_DTEST1_SEL_SHIFT                          6
#define MT6336_RG_AD_QI_OTG_VM_OVP_SEL_ADDR                              MT6336_PMIC_ANA_CORE_AD_INTF4_SEL
#define MT6336_RG_AD_QI_OTG_VM_OVP_SEL_MASK                              0x1
#define MT6336_RG_AD_QI_OTG_VM_OVP_SEL_SHIFT                             0
#define MT6336_RG_AD_QI_OTG_VM_UVLO_SEL_ADDR                             MT6336_PMIC_ANA_CORE_AD_INTF4_SEL
#define MT6336_RG_AD_QI_OTG_VM_UVLO_SEL_MASK                             0x1
#define MT6336_RG_AD_QI_OTG_VM_UVLO_SEL_SHIFT                            1
#define MT6336_RG_AD_QI_OTG_VBAT_UVLO_SEL_ADDR                           MT6336_PMIC_ANA_CORE_AD_INTF4_SEL
#define MT6336_RG_AD_QI_OTG_VBAT_UVLO_SEL_MASK                           0x1
#define MT6336_RG_AD_QI_OTG_VBAT_UVLO_SEL_SHIFT                          2
#define MT6336_RG_AD_NI_VBAT_OVP_SEL_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF4_SEL
#define MT6336_RG_AD_NI_VBAT_OVP_SEL_MASK                                0x1
#define MT6336_RG_AD_NI_VBAT_OVP_SEL_SHIFT                               3
#define MT6336_RG_AD_NI_VSYS_OVP_SEL_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF4_SEL
#define MT6336_RG_AD_NI_VSYS_OVP_SEL_MASK                                0x1
#define MT6336_RG_AD_NI_VSYS_OVP_SEL_SHIFT                               4
#define MT6336_RG_AD_NI_WEAKBUS_SEL_ADDR                                 MT6336_PMIC_ANA_CORE_AD_INTF4_SEL
#define MT6336_RG_AD_NI_WEAKBUS_SEL_MASK                                 0x1
#define MT6336_RG_AD_NI_WEAKBUS_SEL_SHIFT                                5
#define MT6336_RG_AD_QI_VBUS_UVLO_SEL_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF4_SEL
#define MT6336_RG_AD_QI_VBUS_UVLO_SEL_MASK                               0x1
#define MT6336_RG_AD_QI_VBUS_UVLO_SEL_SHIFT                              6
#define MT6336_RG_AD_QI_VBUS_OVP_SEL_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF4_SEL
#define MT6336_RG_AD_QI_VBUS_OVP_SEL_MASK                                0x1
#define MT6336_RG_AD_QI_VBUS_OVP_SEL_SHIFT                               7
#define MT6336_RG_AD_QI_PAM_MODE_SEL_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF5_SEL
#define MT6336_RG_AD_QI_PAM_MODE_SEL_MASK                                0x1
#define MT6336_RG_AD_QI_PAM_MODE_SEL_SHIFT                               0
#define MT6336_RG_AD_QI_CV_MODE_SEL_ADDR                                 MT6336_PMIC_ANA_CORE_AD_INTF5_SEL
#define MT6336_RG_AD_QI_CV_MODE_SEL_MASK                                 0x1
#define MT6336_RG_AD_QI_CV_MODE_SEL_SHIFT                                1
#define MT6336_RG_AD_QI_ICC_MODE_SEL_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF5_SEL
#define MT6336_RG_AD_QI_ICC_MODE_SEL_MASK                                0x1
#define MT6336_RG_AD_QI_ICC_MODE_SEL_SHIFT                               2
#define MT6336_RG_AD_QI_ICL_MODE_SEL_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF5_SEL
#define MT6336_RG_AD_QI_ICL_MODE_SEL_MASK                                0x1
#define MT6336_RG_AD_QI_ICL_MODE_SEL_SHIFT                               3
#define MT6336_RG_AD_QI_FLASH_VFLA_OVP_SEL_ADDR                          MT6336_PMIC_ANA_CORE_AD_INTF5_SEL
#define MT6336_RG_AD_QI_FLASH_VFLA_OVP_SEL_MASK                          0x1
#define MT6336_RG_AD_QI_FLASH_VFLA_OVP_SEL_SHIFT                         4
#define MT6336_RG_AD_QI_FLASH_VFLA_UVLO_SEL_ADDR                         MT6336_PMIC_ANA_CORE_AD_INTF5_SEL
#define MT6336_RG_AD_QI_FLASH_VFLA_UVLO_SEL_MASK                         0x1
#define MT6336_RG_AD_QI_FLASH_VFLA_UVLO_SEL_SHIFT                        5
#define MT6336_RG_AD_QI_SWCHR_OC_STATUS_SEL_ADDR                         MT6336_PMIC_ANA_CORE_AD_INTF6_SEL
#define MT6336_RG_AD_QI_SWCHR_OC_STATUS_SEL_MASK                         0x1
#define MT6336_RG_AD_QI_SWCHR_OC_STATUS_SEL_SHIFT                        0
#define MT6336_RG_AD_QI_BOOT_UVLO_SEL_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF6_SEL
#define MT6336_RG_AD_QI_BOOT_UVLO_SEL_MASK                               0x1
#define MT6336_RG_AD_QI_BOOT_UVLO_SEL_SHIFT                              1
#define MT6336_RG_AD_QI_ZX_SWCHR_ZCD_FLAG_SEL_ADDR                       MT6336_PMIC_ANA_CORE_AD_INTF6_SEL
#define MT6336_RG_AD_QI_ZX_SWCHR_ZCD_FLAG_SEL_MASK                       0x1
#define MT6336_RG_AD_QI_ZX_SWCHR_ZCD_FLAG_SEL_SHIFT                      2
#define MT6336_RG_AD_NI_ZX_OTG_TEST_SEL_ADDR                             MT6336_PMIC_ANA_CORE_AD_INTF6_SEL
#define MT6336_RG_AD_NI_ZX_OTG_TEST_SEL_MASK                             0x1
#define MT6336_RG_AD_NI_ZX_OTG_TEST_SEL_SHIFT                            3
#define MT6336_RG_AD_NI_ZX_FLASH_TEST_SEL_ADDR                           MT6336_PMIC_ANA_CORE_AD_INTF6_SEL
#define MT6336_RG_AD_NI_ZX_FLASH_TEST_SEL_MASK                           0x1
#define MT6336_RG_AD_NI_ZX_FLASH_TEST_SEL_SHIFT                          4
#define MT6336_RG_AD_NI_ZX_SWCHR_TEST_SEL_ADDR                           MT6336_PMIC_ANA_CORE_AD_INTF6_SEL
#define MT6336_RG_AD_NI_ZX_SWCHR_TEST_SEL_MASK                           0x1
#define MT6336_RG_AD_NI_ZX_SWCHR_TEST_SEL_SHIFT                          5
#define MT6336_RG_AD_QI_OTG_OLP_SEL_ADDR                                 MT6336_PMIC_ANA_CORE_AD_INTF6_SEL
#define MT6336_RG_AD_QI_OTG_OLP_SEL_MASK                                 0x1
#define MT6336_RG_AD_QI_OTG_OLP_SEL_SHIFT                                6
#define MT6336_RG_AD_QI_THR_MODE_SEL_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF6_SEL
#define MT6336_RG_AD_QI_THR_MODE_SEL_MASK                                0x1
#define MT6336_RG_AD_QI_THR_MODE_SEL_SHIFT                               7
#define MT6336_RG_AD_NI_FTR_SHOOT_DB_SEL_ADDR                            MT6336_PMIC_ANA_CORE_AD_INTF7_SEL
#define MT6336_RG_AD_NI_FTR_SHOOT_DB_SEL_MASK                            0x1
#define MT6336_RG_AD_NI_FTR_SHOOT_DB_SEL_SHIFT                           0
#define MT6336_RG_AD_NI_FTR_SHOOT_SEL_ADDR                               MT6336_PMIC_ANA_CORE_AD_INTF7_SEL
#define MT6336_RG_AD_NI_FTR_SHOOT_SEL_MASK                               0x1
#define MT6336_RG_AD_NI_FTR_SHOOT_SEL_SHIFT                              1
#define MT6336_RG_AD_NI_FTR_DROP_DB_SEL_ADDR                             MT6336_PMIC_ANA_CORE_AD_INTF7_SEL
#define MT6336_RG_AD_NI_FTR_DROP_DB_SEL_MASK                             0x1
#define MT6336_RG_AD_NI_FTR_DROP_DB_SEL_SHIFT                            2
#define MT6336_RG_AD_NI_FTR_DROP_SEL_ADDR                                MT6336_PMIC_ANA_CORE_AD_INTF7_SEL
#define MT6336_RG_AD_NI_FTR_DROP_SEL_MASK                                0x1
#define MT6336_RG_AD_NI_FTR_DROP_SEL_SHIFT                               3
#define MT6336_RG_AD_QI_VADC18_READY_SEL_ADDR                            MT6336_PMIC_ANA_CORE_AD_INTF7_SEL
#define MT6336_RG_AD_QI_VADC18_READY_SEL_MASK                            0x1
#define MT6336_RG_AD_QI_VADC18_READY_SEL_SHIFT                           4
#define MT6336_RG_AD_QI_SWCHR_ENPWM_STATUS_SEL_ADDR                      MT6336_PMIC_ANA_CORE_AD_INTF7_SEL
#define MT6336_RG_AD_QI_SWCHR_ENPWM_STATUS_SEL_MASK                      0x1
#define MT6336_RG_AD_QI_SWCHR_ENPWM_STATUS_SEL_SHIFT                     5
#define MT6336_RG_AD_QI_SWCHR_OCASYN_STATUS_SEL_ADDR                     MT6336_PMIC_ANA_CORE_AD_INTF7_SEL
#define MT6336_RG_AD_QI_SWCHR_OCASYN_STATUS_SEL_MASK                     0x1
#define MT6336_RG_AD_QI_SWCHR_OCASYN_STATUS_SEL_SHIFT                    6
#define MT6336_RG_DA_QI_TORCH_MODE_SEL_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF0_SEL
#define MT6336_RG_DA_QI_TORCH_MODE_SEL_MASK                              0x1
#define MT6336_RG_DA_QI_TORCH_MODE_SEL_SHIFT                             0
#define MT6336_RG_DA_QI_OTG_MODE_DB_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF0_SEL
#define MT6336_RG_DA_QI_OTG_MODE_DB_SEL_MASK                             0x1
#define MT6336_RG_DA_QI_OTG_MODE_DB_SEL_SHIFT                            1
#define MT6336_RG_DA_QI_FLASH_MODE_DB_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF0_SEL
#define MT6336_RG_DA_QI_FLASH_MODE_DB_SEL_MASK                           0x1
#define MT6336_RG_DA_QI_FLASH_MODE_DB_SEL_SHIFT                          2
#define MT6336_RG_DA_QI_BUCK_MODE_DB_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF0_SEL
#define MT6336_RG_DA_QI_BUCK_MODE_DB_SEL_MASK                            0x1
#define MT6336_RG_DA_QI_BUCK_MODE_DB_SEL_SHIFT                           3
#define MT6336_RG_DA_QI_BASE_READY_DB_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF0_SEL
#define MT6336_RG_DA_QI_BASE_READY_DB_SEL_MASK                           0x1
#define MT6336_RG_DA_QI_BASE_READY_DB_SEL_SHIFT                          4
#define MT6336_RG_DA_QI_BASE_READY_SEL_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF0_SEL
#define MT6336_RG_DA_QI_BASE_READY_SEL_MASK                              0x1
#define MT6336_RG_DA_QI_BASE_READY_SEL_SHIFT                             5
#define MT6336_RG_DA_QI_LOWQ_STAT_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF0_SEL
#define MT6336_RG_DA_QI_LOWQ_STAT_SEL_MASK                               0x1
#define MT6336_RG_DA_QI_LOWQ_STAT_SEL_SHIFT                              6
#define MT6336_RG_DA_QI_SHIP_STAT_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF0_SEL
#define MT6336_RG_DA_QI_SHIP_STAT_SEL_MASK                               0x1
#define MT6336_RG_DA_QI_SHIP_STAT_SEL_SHIFT                              7
#define MT6336_RG_DA_QI_TORCH_MODE_DRV_EN_SEL_ADDR                       MT6336_PMIC_ANA_CORE_DA_INTF1_SEL
#define MT6336_RG_DA_QI_TORCH_MODE_DRV_EN_SEL_MASK                       0x1
#define MT6336_RG_DA_QI_TORCH_MODE_DRV_EN_SEL_SHIFT                      0
#define MT6336_RG_DA_QI_TORCH_MODE_DB_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF1_SEL
#define MT6336_RG_DA_QI_TORCH_MODE_DB_SEL_MASK                           0x1
#define MT6336_RG_DA_QI_TORCH_MODE_DB_SEL_SHIFT                          1
#define MT6336_RG_DA_QI_BASE_CLK_TRIM_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF2_SEL
#define MT6336_RG_DA_QI_BASE_CLK_TRIM_SEL_MASK                           0x3F
#define MT6336_RG_DA_QI_BASE_CLK_TRIM_SEL_SHIFT                          0
#define MT6336_RG_DA_QI_OSC_TRIM_SEL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF3_SEL
#define MT6336_RG_DA_QI_OSC_TRIM_SEL_MASK                                0x3F
#define MT6336_RG_DA_QI_OSC_TRIM_SEL_SHIFT                               0
#define MT6336_RG_DA_QI_CLKSQ_IN_SEL_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF4_SEL
#define MT6336_RG_DA_QI_CLKSQ_IN_SEL_SEL_MASK                            0x1
#define MT6336_RG_DA_QI_CLKSQ_IN_SEL_SEL_SHIFT                           0
#define MT6336_RG_DA_QI_CLKSQ_EN_SEL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF4_SEL
#define MT6336_RG_DA_QI_CLKSQ_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_QI_CLKSQ_EN_SEL_SHIFT                               1
#define MT6336_RG_DA_QI_EFUSE_READ_RDY_DLY_SEL_ADDR                      MT6336_PMIC_ANA_CORE_DA_INTF4_SEL
#define MT6336_RG_DA_QI_EFUSE_READ_RDY_DLY_SEL_MASK                      0x1
#define MT6336_RG_DA_QI_EFUSE_READ_RDY_DLY_SEL_SHIFT                     2
#define MT6336_RG_DA_QI_VBGR_READY_DB_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF4_SEL
#define MT6336_RG_DA_QI_VBGR_READY_DB_SEL_MASK                           0x1
#define MT6336_RG_DA_QI_VBGR_READY_DB_SEL_SHIFT                          3
#define MT6336_RG_DA_QI_BGR_SPEEDUP_EN_SEL_ADDR                          MT6336_PMIC_ANA_CORE_DA_INTF4_SEL
#define MT6336_RG_DA_QI_BGR_SPEEDUP_EN_SEL_MASK                          0x1
#define MT6336_RG_DA_QI_BGR_SPEEDUP_EN_SEL_SHIFT                         4
#define MT6336_RG_DA_QI_ILED1_ITH_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF5_SEL
#define MT6336_RG_DA_QI_ILED1_ITH_SEL_MASK                               0x7F
#define MT6336_RG_DA_QI_ILED1_ITH_SEL_SHIFT                              0
#define MT6336_RG_DA_QI_ADC18_EN_SEL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF5_SEL
#define MT6336_RG_DA_QI_ADC18_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_QI_ADC18_EN_SEL_SHIFT                               7
#define MT6336_RG_DA_QI_ILED1_EN_SEL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF6_SEL
#define MT6336_RG_DA_QI_ILED1_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_QI_ILED1_EN_SEL_SHIFT                               0
#define MT6336_RG_DA_QI_ILED2_ITH_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF6_SEL
#define MT6336_RG_DA_QI_ILED2_ITH_SEL_MASK                               0x7F
#define MT6336_RG_DA_QI_ILED2_ITH_SEL_SHIFT                              1
#define MT6336_RG_DA_QI_EN_ADCIN_VLED1_SEL_ADDR                          MT6336_PMIC_ANA_CORE_DA_INTF7_SEL
#define MT6336_RG_DA_QI_EN_ADCIN_VLED1_SEL_MASK                          0x1
#define MT6336_RG_DA_QI_EN_ADCIN_VLED1_SEL_SHIFT                         0
#define MT6336_RG_DA_QI_EN_ADCIN_VBATON_SEL_ADDR                         MT6336_PMIC_ANA_CORE_DA_INTF7_SEL
#define MT6336_RG_DA_QI_EN_ADCIN_VBATON_SEL_MASK                         0x1
#define MT6336_RG_DA_QI_EN_ADCIN_VBATON_SEL_SHIFT                        1
#define MT6336_RG_DA_QI_EN_ADCIN_VBUS_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF7_SEL
#define MT6336_RG_DA_QI_EN_ADCIN_VBUS_SEL_MASK                           0x1
#define MT6336_RG_DA_QI_EN_ADCIN_VBUS_SEL_SHIFT                          2
#define MT6336_RG_DA_QI_EN_ADCIN_VBATSNS_SEL_ADDR                        MT6336_PMIC_ANA_CORE_DA_INTF7_SEL
#define MT6336_RG_DA_QI_EN_ADCIN_VBATSNS_SEL_MASK                        0x1
#define MT6336_RG_DA_QI_EN_ADCIN_VBATSNS_SEL_SHIFT                       3
#define MT6336_RG_DA_QI_IOSDET2_EN_SEL_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF7_SEL
#define MT6336_RG_DA_QI_IOSDET2_EN_SEL_MASK                              0x1
#define MT6336_RG_DA_QI_IOSDET2_EN_SEL_SHIFT                             4
#define MT6336_RG_DA_QI_IOSDET1_EN_SEL_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF7_SEL
#define MT6336_RG_DA_QI_IOSDET1_EN_SEL_MASK                              0x1
#define MT6336_RG_DA_QI_IOSDET1_EN_SEL_SHIFT                             5
#define MT6336_RG_DA_QI_OSDET_EN_SEL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF7_SEL
#define MT6336_RG_DA_QI_OSDET_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_QI_OSDET_EN_SEL_SHIFT                               6
#define MT6336_RG_DA_QI_ILED2_EN_SEL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF7_SEL
#define MT6336_RG_DA_QI_ILED2_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_QI_ILED2_EN_SEL_SHIFT                               7
#define MT6336_RG_DA_QI_EN_ADCIN_VLED2_SEL_ADDR                          MT6336_PMIC_ANA_CORE_DA_INTF8_SEL
#define MT6336_RG_DA_QI_EN_ADCIN_VLED2_SEL_MASK                          0x1
#define MT6336_RG_DA_QI_EN_ADCIN_VLED2_SEL_SHIFT                         0
#define MT6336_RG_DA_QI_BC12_IPDC_EN_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF9_SEL
#define MT6336_RG_DA_QI_BC12_IPDC_EN_SEL_MASK                            0x3
#define MT6336_RG_DA_QI_BC12_IPDC_EN_SEL_SHIFT                           0
#define MT6336_RG_DA_QI_BC12_IPU_EN_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF9_SEL
#define MT6336_RG_DA_QI_BC12_IPU_EN_SEL_MASK                             0x3
#define MT6336_RG_DA_QI_BC12_IPU_EN_SEL_SHIFT                            2
#define MT6336_RG_DA_QI_BC12_IPD_EN_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF9_SEL
#define MT6336_RG_DA_QI_BC12_IPD_EN_SEL_MASK                             0x3
#define MT6336_RG_DA_QI_BC12_IPD_EN_SEL_SHIFT                            4
#define MT6336_RG_DA_QI_BC12_IBIAS_EN_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF9_SEL
#define MT6336_RG_DA_QI_BC12_IBIAS_EN_SEL_MASK                           0x1
#define MT6336_RG_DA_QI_BC12_IBIAS_EN_SEL_SHIFT                          6
#define MT6336_RG_DA_QI_BC12_BB_CTRL_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF10_SEL
#define MT6336_RG_DA_QI_BC12_BB_CTRL_SEL_MASK                            0x1
#define MT6336_RG_DA_QI_BC12_BB_CTRL_SEL_SHIFT                           0
#define MT6336_RG_DA_QI_BC12_IPD_HALF_EN_SEL_ADDR                        MT6336_PMIC_ANA_CORE_DA_INTF10_SEL
#define MT6336_RG_DA_QI_BC12_IPD_HALF_EN_SEL_MASK                        0x1
#define MT6336_RG_DA_QI_BC12_IPD_HALF_EN_SEL_SHIFT                       1
#define MT6336_RG_DA_QI_BC12_VREF_VTH_EN_SEL_ADDR                        MT6336_PMIC_ANA_CORE_DA_INTF10_SEL
#define MT6336_RG_DA_QI_BC12_VREF_VTH_EN_SEL_MASK                        0x3
#define MT6336_RG_DA_QI_BC12_VREF_VTH_EN_SEL_SHIFT                       2
#define MT6336_RG_DA_QI_BC12_CMP_EN_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF10_SEL
#define MT6336_RG_DA_QI_BC12_CMP_EN_SEL_MASK                             0x3
#define MT6336_RG_DA_QI_BC12_CMP_EN_SEL_SHIFT                            4
#define MT6336_RG_DA_QI_BC12_VSRC_EN_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF10_SEL
#define MT6336_RG_DA_QI_BC12_VSRC_EN_SEL_MASK                            0x3
#define MT6336_RG_DA_QI_BC12_VSRC_EN_SEL_SHIFT                           6
#define MT6336_RG_DA_QI_VPREG_RDY_DB_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF11_SEL
#define MT6336_RG_DA_QI_VPREG_RDY_DB_SEL_MASK                            0x1
#define MT6336_RG_DA_QI_VPREG_RDY_DB_SEL_SHIFT                           0
#define MT6336_RG_DA_QI_IPRECC1_ITH_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF11_SEL
#define MT6336_RG_DA_QI_IPRECC1_ITH_SEL_MASK                             0x3
#define MT6336_RG_DA_QI_IPRECC1_ITH_SEL_SHIFT                            1
#define MT6336_RG_DA_QI_EN_IBIASGEN_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF11_SEL
#define MT6336_RG_DA_QI_EN_IBIASGEN_SEL_MASK                             0x1
#define MT6336_RG_DA_QI_EN_IBIASGEN_SEL_SHIFT                            3
#define MT6336_RG_DA_NS_VPRECHG_FAIL_DB_SEL_ADDR                         MT6336_PMIC_ANA_CORE_DA_INTF11_SEL
#define MT6336_RG_DA_NS_VPRECHG_FAIL_DB_SEL_MASK                         0x1
#define MT6336_RG_DA_NS_VPRECHG_FAIL_DB_SEL_SHIFT                        4
#define MT6336_RG_DA_QI_EN_CP_HIQ_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF11_SEL
#define MT6336_RG_DA_QI_EN_CP_HIQ_SEL_MASK                               0x1
#define MT6336_RG_DA_QI_EN_CP_HIQ_SEL_SHIFT                              5
#define MT6336_RG_DA_QI_VBAT_LT_PRECC1_DB_SEL_ADDR                       MT6336_PMIC_ANA_CORE_DA_INTF11_SEL
#define MT6336_RG_DA_QI_VBAT_LT_PRECC1_DB_SEL_MASK                       0x1
#define MT6336_RG_DA_QI_VBAT_LT_PRECC1_DB_SEL_SHIFT                      6
#define MT6336_RG_DA_QI_VBAT_LT_PRECC0_DB_SEL_ADDR                       MT6336_PMIC_ANA_CORE_DA_INTF11_SEL
#define MT6336_RG_DA_QI_VBAT_LT_PRECC0_DB_SEL_MASK                       0x1
#define MT6336_RG_DA_QI_VBAT_LT_PRECC0_DB_SEL_SHIFT                      7
#define MT6336_RG_DA_QI_IPRECC1_EN_SEL_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF12_SEL
#define MT6336_RG_DA_QI_IPRECC1_EN_SEL_MASK                              0x1
#define MT6336_RG_DA_QI_IPRECC1_EN_SEL_SHIFT                             0
#define MT6336_RG_DA_QI_IPRECC0_EN_SEL_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF12_SEL
#define MT6336_RG_DA_QI_IPRECC0_EN_SEL_MASK                              0x1
#define MT6336_RG_DA_QI_IPRECC0_EN_SEL_SHIFT                             1
#define MT6336_RG_DA_QI_VBATFETON_VTH_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF12_SEL
#define MT6336_RG_DA_QI_VBATFETON_VTH_SEL_MASK                           0x7
#define MT6336_RG_DA_QI_VBATFETON_VTH_SEL_SHIFT                          2
#define MT6336_RG_DA_QI_BATOC_ANA_SEL_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF12_SEL
#define MT6336_RG_DA_QI_BATOC_ANA_SEL_SEL_MASK                           0x1
#define MT6336_RG_DA_QI_BATOC_ANA_SEL_SEL_SHIFT                          5
#define MT6336_RG_DA_QI_FETON_OK_LOQCP_EN_SEL_ADDR                       MT6336_PMIC_ANA_CORE_DA_INTF12_SEL
#define MT6336_RG_DA_QI_FETON_OK_LOQCP_EN_SEL_MASK                       0x1
#define MT6336_RG_DA_QI_FETON_OK_LOQCP_EN_SEL_SHIFT                      6
#define MT6336_RG_DA_QI_EN_VDC_IBIAS_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF12_SEL
#define MT6336_RG_DA_QI_EN_VDC_IBIAS_SEL_MASK                            0x1
#define MT6336_RG_DA_QI_EN_VDC_IBIAS_SEL_SHIFT                           7
#define MT6336_RG_DA_NI_CHRIND_EN_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF13_SEL
#define MT6336_RG_DA_NI_CHRIND_EN_SEL_MASK                               0x1
#define MT6336_RG_DA_NI_CHRIND_EN_SEL_SHIFT                              0
#define MT6336_RG_DA_NI_CHRIND_BIAS_EN_SEL_ADDR                          MT6336_PMIC_ANA_CORE_DA_INTF13_SEL
#define MT6336_RG_DA_NI_CHRIND_BIAS_EN_SEL_MASK                          0x1
#define MT6336_RG_DA_NI_CHRIND_BIAS_EN_SEL_SHIFT                         1
#define MT6336_RG_DA_QI_VSYS_IDIS_EN_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF13_SEL
#define MT6336_RG_DA_QI_VSYS_IDIS_EN_SEL_MASK                            0x1
#define MT6336_RG_DA_QI_VSYS_IDIS_EN_SEL_SHIFT                           2
#define MT6336_RG_DA_QI_VBAT_IDIS_EN_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF13_SEL
#define MT6336_RG_DA_QI_VBAT_IDIS_EN_SEL_MASK                            0x1
#define MT6336_RG_DA_QI_VBAT_IDIS_EN_SEL_SHIFT                           3
#define MT6336_RG_DA_QI_ITERM_ITH_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF13_SEL
#define MT6336_RG_DA_QI_ITERM_ITH_SEL_MASK                               0xF
#define MT6336_RG_DA_QI_ITERM_ITH_SEL_SHIFT                              4
#define MT6336_RG_DA_NI_CHRIND_STEP_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF14_SEL
#define MT6336_RG_DA_NI_CHRIND_STEP_SEL_MASK                             0x3
#define MT6336_RG_DA_NI_CHRIND_STEP_SEL_SHIFT                            0
#define MT6336_RG_DA_QI_CHR_DET_SEL_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF15_SEL
#define MT6336_RG_DA_QI_CHR_DET_SEL_MASK                                 0x1
#define MT6336_RG_DA_QI_CHR_DET_SEL_SHIFT                                0
#define MT6336_RG_DA_QI_REVFET_EN_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF15_SEL
#define MT6336_RG_DA_QI_REVFET_EN_SEL_MASK                               0x1
#define MT6336_RG_DA_QI_REVFET_EN_SEL_SHIFT                              1
#define MT6336_RG_DA_QI_SSFNSH_SEL_ADDR                                  MT6336_PMIC_ANA_CORE_DA_INTF15_SEL
#define MT6336_RG_DA_QI_SSFNSH_SEL_MASK                                  0x1
#define MT6336_RG_DA_QI_SSFNSH_SEL_SHIFT                                 2
#define MT6336_RG_DA_QI_IBACKBOOST_EN_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF15_SEL
#define MT6336_RG_DA_QI_IBACKBOOST_EN_SEL_MASK                           0x1
#define MT6336_RG_DA_QI_IBACKBOOST_EN_SEL_SHIFT                          3
#define MT6336_RG_DA_NI_WEAKBUS_DB_SEL_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF15_SEL
#define MT6336_RG_DA_NI_WEAKBUS_DB_SEL_MASK                              0x1
#define MT6336_RG_DA_NI_WEAKBUS_DB_SEL_SHIFT                             4
#define MT6336_RG_DA_QI_VBUS_OVP_DB_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF15_SEL
#define MT6336_RG_DA_QI_VBUS_OVP_DB_SEL_MASK                             0x1
#define MT6336_RG_DA_QI_VBUS_OVP_DB_SEL_SHIFT                            5
#define MT6336_RG_DA_QI_VBUS_UVLO_DB_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF15_SEL
#define MT6336_RG_DA_QI_VBUS_UVLO_DB_SEL_MASK                            0x1
#define MT6336_RG_DA_QI_VBUS_UVLO_DB_SEL_SHIFT                           6
#define MT6336_RG_DA_QI_ICL_ITH_SEL_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF16_SEL
#define MT6336_RG_DA_QI_ICL_ITH_SEL_MASK                                 0x3F
#define MT6336_RG_DA_QI_ICL_ITH_SEL_SHIFT                                0
#define MT6336_RG_DA_QI_CV_REGNODE_SEL_SEL_ADDR                          MT6336_PMIC_ANA_CORE_DA_INTF17_SEL
#define MT6336_RG_DA_QI_CV_REGNODE_SEL_SEL_MASK                          0x1
#define MT6336_RG_DA_QI_CV_REGNODE_SEL_SEL_SHIFT                         0
#define MT6336_RG_DA_QI_ICC_ITH_SEL_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF17_SEL
#define MT6336_RG_DA_QI_ICC_ITH_SEL_MASK                                 0x7F
#define MT6336_RG_DA_QI_ICC_ITH_SEL_SHIFT                                1
#define MT6336_RG_DA_QI_VSYSREG_VTH_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF18_SEL
#define MT6336_RG_DA_QI_VSYSREG_VTH_SEL_MASK                             0xFF
#define MT6336_RG_DA_QI_VSYSREG_VTH_SEL_SHIFT                            0
#define MT6336_RG_DA_QI_VCV_VTH_SEL_ADDR                                 MT6336_PMIC_ANA_CORE_DA_INTF19_SEL
#define MT6336_RG_DA_QI_VCV_VTH_SEL_MASK                                 0xFF
#define MT6336_RG_DA_QI_VCV_VTH_SEL_SHIFT                                0
#define MT6336_RG_DA_QI_OTG_VCV_VTH_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF20_SEL
#define MT6336_RG_DA_QI_OTG_VCV_VTH_SEL_MASK                             0xF
#define MT6336_RG_DA_QI_OTG_VCV_VTH_SEL_SHIFT                            0
#define MT6336_RG_DA_QI_FLASH_MODE_DRV_EN_SEL_ADDR                       MT6336_PMIC_ANA_CORE_DA_INTF21_SEL
#define MT6336_RG_DA_QI_FLASH_MODE_DRV_EN_SEL_MASK                       0x1
#define MT6336_RG_DA_QI_FLASH_MODE_DRV_EN_SEL_SHIFT                      0
#define MT6336_RG_DA_QI_OTG_MODE_SEL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF21_SEL
#define MT6336_RG_DA_QI_OTG_MODE_SEL_MASK                                0x1
#define MT6336_RG_DA_QI_OTG_MODE_SEL_SHIFT                               1
#define MT6336_RG_DA_QI_OTG_MODE_DRV_EN_SEL_ADDR                         MT6336_PMIC_ANA_CORE_DA_INTF21_SEL
#define MT6336_RG_DA_QI_OTG_MODE_DRV_EN_SEL_MASK                         0x1
#define MT6336_RG_DA_QI_OTG_MODE_DRV_EN_SEL_SHIFT                        2
#define MT6336_RG_DA_NI_VBSTCHK_FEN_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF21_SEL
#define MT6336_RG_DA_NI_VBSTCHK_FEN_SEL_MASK                             0x1
#define MT6336_RG_DA_NI_VBSTCHK_FEN_SEL_SHIFT                            3
#define MT6336_RG_DA_QI_VFLA_VTH_SEL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF21_SEL
#define MT6336_RG_DA_QI_VFLA_VTH_SEL_MASK                                0xF
#define MT6336_RG_DA_QI_VFLA_VTH_SEL_SHIFT                               4
#define MT6336_RG_DA_QI_FLASH_VFLA_OVP_DB_SEL_ADDR                       MT6336_PMIC_ANA_CORE_DA_INTF22_SEL
#define MT6336_RG_DA_QI_FLASH_VFLA_OVP_DB_SEL_MASK                       0x1
#define MT6336_RG_DA_QI_FLASH_VFLA_OVP_DB_SEL_SHIFT                      0
#define MT6336_RG_DA_QI_FLASH_VFLA_UVLO_DB_SEL_ADDR                      MT6336_PMIC_ANA_CORE_DA_INTF22_SEL
#define MT6336_RG_DA_QI_FLASH_VFLA_UVLO_DB_SEL_MASK                      0x1
#define MT6336_RG_DA_QI_FLASH_VFLA_UVLO_DB_SEL_SHIFT                     1
#define MT6336_RG_DA_QI_OTG_VM_UVLO_DB_SEL_ADDR                          MT6336_PMIC_ANA_CORE_DA_INTF22_SEL
#define MT6336_RG_DA_QI_OTG_VM_UVLO_DB_SEL_MASK                          0x1
#define MT6336_RG_DA_QI_OTG_VM_UVLO_DB_SEL_SHIFT                         2
#define MT6336_RG_DA_QI_OTG_VM_OVP_DB_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF22_SEL
#define MT6336_RG_DA_QI_OTG_VM_OVP_DB_SEL_MASK                           0x1
#define MT6336_RG_DA_QI_OTG_VM_OVP_DB_SEL_SHIFT                          3
#define MT6336_RG_DA_QI_OSC_REF_EN_SEL_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF22_SEL
#define MT6336_RG_DA_QI_OSC_REF_EN_SEL_MASK                              0x1
#define MT6336_RG_DA_QI_OSC_REF_EN_SEL_SHIFT                             4
#define MT6336_RG_DA_QI_BUCK_MODE_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF22_SEL
#define MT6336_RG_DA_QI_BUCK_MODE_SEL_MASK                               0x1
#define MT6336_RG_DA_QI_BUCK_MODE_SEL_SHIFT                              5
#define MT6336_RG_DA_QI_BUCK_MODE_DRV_EN_SEL_ADDR                        MT6336_PMIC_ANA_CORE_DA_INTF22_SEL
#define MT6336_RG_DA_QI_BUCK_MODE_DRV_EN_SEL_MASK                        0x1
#define MT6336_RG_DA_QI_BUCK_MODE_DRV_EN_SEL_SHIFT                       6
#define MT6336_RG_DA_QI_FLASH_MODE_SEL_ADDR                              MT6336_PMIC_ANA_CORE_DA_INTF22_SEL
#define MT6336_RG_DA_QI_FLASH_MODE_SEL_MASK                              0x1
#define MT6336_RG_DA_QI_FLASH_MODE_SEL_SHIFT                             7
#define MT6336_RG_DA_QI_MFLAPP_EN_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF23_SEL
#define MT6336_RG_DA_QI_MFLAPP_EN_SEL_MASK                               0x1
#define MT6336_RG_DA_QI_MFLAPP_EN_SEL_SHIFT                              0
#define MT6336_RG_DA_QI_OTG_IOLP_ITH_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF23_SEL
#define MT6336_RG_DA_QI_OTG_IOLP_ITH_SEL_MASK                            0x7
#define MT6336_RG_DA_QI_OTG_IOLP_ITH_SEL_SHIFT                           1
#define MT6336_RG_DA_QI_VSYS_REGV_SEL_SEL_ADDR                           MT6336_PMIC_ANA_CORE_DA_INTF23_SEL
#define MT6336_RG_DA_QI_VSYS_REGV_SEL_SEL_MASK                           0x1
#define MT6336_RG_DA_QI_VSYS_REGV_SEL_SEL_SHIFT                          4
#define MT6336_RG_DA_QI_OTG_VBAT_UVLO_DB_SEL_ADDR                        MT6336_PMIC_ANA_CORE_DA_INTF23_SEL
#define MT6336_RG_DA_QI_OTG_VBAT_UVLO_DB_SEL_MASK                        0x1
#define MT6336_RG_DA_QI_OTG_VBAT_UVLO_DB_SEL_SHIFT                       5
#define MT6336_RG_DA_NI_VBAT_OVP_DB_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF23_SEL
#define MT6336_RG_DA_NI_VBAT_OVP_DB_SEL_MASK                             0x1
#define MT6336_RG_DA_NI_VBAT_OVP_DB_SEL_SHIFT                            6
#define MT6336_RG_DA_NI_VSYS_OVP_DB_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF23_SEL
#define MT6336_RG_DA_NI_VSYS_OVP_DB_SEL_MASK                             0x1
#define MT6336_RG_DA_NI_VSYS_OVP_DB_SEL_SHIFT                            7
#define MT6336_RG_DA_QI_BACKGROUND_STAT_SEL_ADDR                         MT6336_PMIC_ANA_CORE_DA_INTF24_SEL
#define MT6336_RG_DA_QI_BACKGROUND_STAT_SEL_MASK                         0x1
#define MT6336_RG_DA_QI_BACKGROUND_STAT_SEL_SHIFT                        0
#define MT6336_RG_DA_QI_DEADBAT_STAT_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF24_SEL
#define MT6336_RG_DA_QI_DEADBAT_STAT_SEL_MASK                            0x1
#define MT6336_RG_DA_QI_DEADBAT_STAT_SEL_SHIFT                           1
#define MT6336_RG_DA_QI_EOC_STAT_SEL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF24_SEL
#define MT6336_RG_DA_QI_EOC_STAT_SEL_MASK                                0x1
#define MT6336_RG_DA_QI_EOC_STAT_SEL_SHIFT                               2
#define MT6336_RG_DA_QI_FASTCC_STAT_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF24_SEL
#define MT6336_RG_DA_QI_FASTCC_STAT_SEL_MASK                             0x1
#define MT6336_RG_DA_QI_FASTCC_STAT_SEL_SHIFT                            3
#define MT6336_RG_DA_QI_POSTPRECC1_STAT_SEL_ADDR                         MT6336_PMIC_ANA_CORE_DA_INTF24_SEL
#define MT6336_RG_DA_QI_POSTPRECC1_STAT_SEL_MASK                         0x1
#define MT6336_RG_DA_QI_POSTPRECC1_STAT_SEL_SHIFT                        4
#define MT6336_RG_DA_QI_VBUS_PLUGIN_SEL_ADDR                             MT6336_PMIC_ANA_CORE_DA_INTF24_SEL
#define MT6336_RG_DA_QI_VBUS_PLUGIN_SEL_MASK                             0x1
#define MT6336_RG_DA_QI_VBUS_PLUGIN_SEL_SHIFT                            5
#define MT6336_RG_DA_QI_VBUS_IDIS_EN_SEL_ADDR                            MT6336_PMIC_ANA_CORE_DA_INTF24_SEL
#define MT6336_RG_DA_QI_VBUS_IDIS_EN_SEL_MASK                            0x1
#define MT6336_RG_DA_QI_VBUS_IDIS_EN_SEL_SHIFT                           6
#define MT6336_RG_DA_QI_MTORPP_EN_SEL_ADDR                               MT6336_PMIC_ANA_CORE_DA_INTF24_SEL
#define MT6336_RG_DA_QI_MTORPP_EN_SEL_MASK                               0x1
#define MT6336_RG_DA_QI_MTORPP_EN_SEL_SHIFT                              7
#define MT6336_RG_DA_QI_VPAM_VTH_SEL_ADDR                                MT6336_PMIC_ANA_CORE_DA_INTF25_SEL
#define MT6336_RG_DA_QI_VPAM_VTH_SEL_MASK                                0x1F
#define MT6336_RG_DA_QI_VPAM_VTH_SEL_SHIFT                               0
#define MT6336_RG_AD_AUXADC_COMP_SEL_ADDR                                MT6336_PMIC_ANA_AUXADC_AD_INTF0_SEL
#define MT6336_RG_AD_AUXADC_COMP_SEL_MASK                                0x1
#define MT6336_RG_AD_AUXADC_COMP_SEL_SHIFT                               0
#define MT6336_RG_DA_AUXADC_DAC_0_SEL_ADDR                               MT6336_PMIC_ANA_AUXADC_DA_INTF0_SEL
#define MT6336_RG_DA_AUXADC_DAC_0_SEL_MASK                               0xFF
#define MT6336_RG_DA_AUXADC_DAC_0_SEL_SHIFT                              0
#define MT6336_RG_DA_AUXADC_DAC_1_SEL_ADDR                               MT6336_PMIC_ANA_AUXADC_DA_INTF1_SEL
#define MT6336_RG_DA_AUXADC_DAC_1_SEL_MASK                               0xF
#define MT6336_RG_DA_AUXADC_DAC_1_SEL_SHIFT                              0
#define MT6336_RG_DA_AUXADC_SEL_SEL_ADDR                                 MT6336_PMIC_ANA_AUXADC_DA_INTF2_SEL
#define MT6336_RG_DA_AUXADC_SEL_SEL_MASK                                 0xF
#define MT6336_RG_DA_AUXADC_SEL_SEL_SHIFT                                0
#define MT6336_RG_DA_TS_VBE_SEL_SEL_ADDR                                 MT6336_PMIC_ANA_AUXADC_DA_INTF3_SEL
#define MT6336_RG_DA_TS_VBE_SEL_SEL_MASK                                 0x1
#define MT6336_RG_DA_TS_VBE_SEL_SEL_SHIFT                                0
#define MT6336_RG_DA_VBUF_EN_SEL_ADDR                                    MT6336_PMIC_ANA_AUXADC_DA_INTF3_SEL
#define MT6336_RG_DA_VBUF_EN_SEL_MASK                                    0x1
#define MT6336_RG_DA_VBUF_EN_SEL_SHIFT                                   1
#define MT6336_RG_DA_AUXADC_RNG_SEL_ADDR                                 MT6336_PMIC_ANA_AUXADC_DA_INTF3_SEL
#define MT6336_RG_DA_AUXADC_RNG_SEL_MASK                                 0x1
#define MT6336_RG_DA_AUXADC_RNG_SEL_SHIFT                                2
#define MT6336_RG_DA_AUXADC_SPL_SEL_ADDR                                 MT6336_PMIC_ANA_AUXADC_DA_INTF3_SEL
#define MT6336_RG_DA_AUXADC_SPL_SEL_MASK                                 0x1
#define MT6336_RG_DA_AUXADC_SPL_SEL_SHIFT                                3
#define MT6336_RG_DA_AUXADC_ADC_PWDB_SEL_ADDR                            MT6336_PMIC_ANA_AUXADC_DA_INTF3_SEL
#define MT6336_RG_DA_AUXADC_ADC_PWDB_SEL_MASK                            0x1
#define MT6336_RG_DA_AUXADC_ADC_PWDB_SEL_SHIFT                           4
#define MT6336_RG_AD_PD_CC2_OVP_SEL_ADDR                                 MT6336_PMIC_ANA_TYPEC_AD_INTF0_SEL
#define MT6336_RG_AD_PD_CC2_OVP_SEL_MASK                                 0x1
#define MT6336_RG_AD_PD_CC2_OVP_SEL_SHIFT                                0
#define MT6336_RG_AD_PD_CC1_OVP_SEL_ADDR                                 MT6336_PMIC_ANA_TYPEC_AD_INTF0_SEL
#define MT6336_RG_AD_PD_CC1_OVP_SEL_MASK                                 0x1
#define MT6336_RG_AD_PD_CC1_OVP_SEL_SHIFT                                1
#define MT6336_RG_AD_PD_VCONN_UVP_SEL_ADDR                               MT6336_PMIC_ANA_TYPEC_AD_INTF0_SEL
#define MT6336_RG_AD_PD_VCONN_UVP_SEL_MASK                               0x1
#define MT6336_RG_AD_PD_VCONN_UVP_SEL_SHIFT                              2
#define MT6336_RG_AD_PD_RX_DATA_SEL_ADDR                                 MT6336_PMIC_ANA_TYPEC_AD_INTF0_SEL
#define MT6336_RG_AD_PD_RX_DATA_SEL_MASK                                 0x1
#define MT6336_RG_AD_PD_RX_DATA_SEL_SHIFT                                3
#define MT6336_RG_AD_CC_VUSB33_RDY_SEL_ADDR                              MT6336_PMIC_ANA_TYPEC_AD_INTF0_SEL
#define MT6336_RG_AD_CC_VUSB33_RDY_SEL_MASK                              0x1
#define MT6336_RG_AD_CC_VUSB33_RDY_SEL_SHIFT                             4
#define MT6336_RG_AD_CC_CMP_OUT_SEL_ADDR                                 MT6336_PMIC_ANA_TYPEC_AD_INTF0_SEL
#define MT6336_RG_AD_CC_CMP_OUT_SEL_MASK                                 0x1
#define MT6336_RG_AD_CC_CMP_OUT_SEL_SHIFT                                5
#define MT6336_RG_DA_CC_LPF_EN_SEL_ADDR                                  MT6336_PMIC_ANA_TYPEC_DA_INTF0_SEL
#define MT6336_RG_DA_CC_LPF_EN_SEL_MASK                                  0x1
#define MT6336_RG_DA_CC_LPF_EN_SEL_SHIFT                                 0
#define MT6336_RG_DA_CC_BIAS_EN_SEL_ADDR                                 MT6336_PMIC_ANA_TYPEC_DA_INTF0_SEL
#define MT6336_RG_DA_CC_BIAS_EN_SEL_MASK                                 0x1
#define MT6336_RG_DA_CC_BIAS_EN_SEL_SHIFT                                1
#define MT6336_RG_DA_CC_RACC2_EN_SEL_ADDR                                MT6336_PMIC_ANA_TYPEC_DA_INTF0_SEL
#define MT6336_RG_DA_CC_RACC2_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_CC_RACC2_EN_SEL_SHIFT                               2
#define MT6336_RG_DA_CC_RDCC2_EN_SEL_ADDR                                MT6336_PMIC_ANA_TYPEC_DA_INTF0_SEL
#define MT6336_RG_DA_CC_RDCC2_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_CC_RDCC2_EN_SEL_SHIFT                               3
#define MT6336_RG_DA_CC_RPCC2_EN_SEL_ADDR                                MT6336_PMIC_ANA_TYPEC_DA_INTF0_SEL
#define MT6336_RG_DA_CC_RPCC2_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_CC_RPCC2_EN_SEL_SHIFT                               4
#define MT6336_RG_DA_CC_RACC1_EN_SEL_ADDR                                MT6336_PMIC_ANA_TYPEC_DA_INTF0_SEL
#define MT6336_RG_DA_CC_RACC1_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_CC_RACC1_EN_SEL_SHIFT                               5
#define MT6336_RG_DA_CC_RDCC1_EN_SEL_ADDR                                MT6336_PMIC_ANA_TYPEC_DA_INTF0_SEL
#define MT6336_RG_DA_CC_RDCC1_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_CC_RDCC1_EN_SEL_SHIFT                               6
#define MT6336_RG_DA_CC_RPCC1_EN_SEL_ADDR                                MT6336_PMIC_ANA_TYPEC_DA_INTF0_SEL
#define MT6336_RG_DA_CC_RPCC1_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_CC_RPCC1_EN_SEL_SHIFT                               7
#define MT6336_RG_DA_PD_TX_DATA_SEL_ADDR                                 MT6336_PMIC_ANA_TYPEC_DA_INTF1_SEL
#define MT6336_RG_DA_PD_TX_DATA_SEL_MASK                                 0x1
#define MT6336_RG_DA_PD_TX_DATA_SEL_SHIFT                                0
#define MT6336_RG_DA_PD_TX_EN_SEL_ADDR                                   MT6336_PMIC_ANA_TYPEC_DA_INTF1_SEL
#define MT6336_RG_DA_PD_TX_EN_SEL_MASK                                   0x1
#define MT6336_RG_DA_PD_TX_EN_SEL_SHIFT                                  1
#define MT6336_RG_DA_PD_BIAS_EN_SEL_ADDR                                 MT6336_PMIC_ANA_TYPEC_DA_INTF1_SEL
#define MT6336_RG_DA_PD_BIAS_EN_SEL_MASK                                 0x1
#define MT6336_RG_DA_PD_BIAS_EN_SEL_SHIFT                                2
#define MT6336_RG_DA_CC_DAC_EN_SEL_ADDR                                  MT6336_PMIC_ANA_TYPEC_DA_INTF1_SEL
#define MT6336_RG_DA_CC_DAC_EN_SEL_MASK                                  0x1
#define MT6336_RG_DA_CC_DAC_EN_SEL_SHIFT                                 3
#define MT6336_RG_DA_CC_SASW_EN_SEL_ADDR                                 MT6336_PMIC_ANA_TYPEC_DA_INTF1_SEL
#define MT6336_RG_DA_CC_SASW_EN_SEL_MASK                                 0x1
#define MT6336_RG_DA_CC_SASW_EN_SEL_SHIFT                                4
#define MT6336_RG_DA_CC_ADCSW_EN_SEL_ADDR                                MT6336_PMIC_ANA_TYPEC_DA_INTF1_SEL
#define MT6336_RG_DA_CC_ADCSW_EN_SEL_MASK                                0x1
#define MT6336_RG_DA_CC_ADCSW_EN_SEL_SHIFT                               5
#define MT6336_RG_DA_CC_SW_SEL_SEL_ADDR                                  MT6336_PMIC_ANA_TYPEC_DA_INTF1_SEL
#define MT6336_RG_DA_CC_SW_SEL_SEL_MASK                                  0x1
#define MT6336_RG_DA_CC_SW_SEL_SEL_SHIFT                                 6
#define MT6336_RG_DA_CC_LEV_EN_SEL_ADDR                                  MT6336_PMIC_ANA_TYPEC_DA_INTF1_SEL
#define MT6336_RG_DA_CC_LEV_EN_SEL_MASK                                  0x1
#define MT6336_RG_DA_CC_LEV_EN_SEL_SHIFT                                 7
#define MT6336_RG_DA_CC_DAC_IN_SEL_ADDR                                  MT6336_PMIC_ANA_TYPEC_DA_INTF2_SEL
#define MT6336_RG_DA_CC_DAC_IN_SEL_MASK                                  0x3F
#define MT6336_RG_DA_CC_DAC_IN_SEL_SHIFT                                 0
#define MT6336_RG_DA_PD_RX_EN_SEL_ADDR                                   MT6336_PMIC_ANA_TYPEC_DA_INTF2_SEL
#define MT6336_RG_DA_PD_RX_EN_SEL_MASK                                   0x1
#define MT6336_RG_DA_PD_RX_EN_SEL_SHIFT                                  6
#define MT6336_RG_DA_CC_DAC_GAIN_CAL_SEL_ADDR                            MT6336_PMIC_ANA_TYPEC_DA_INTF3_SEL
#define MT6336_RG_DA_CC_DAC_GAIN_CAL_SEL_MASK                            0xF
#define MT6336_RG_DA_CC_DAC_GAIN_CAL_SEL_SHIFT                           0
#define MT6336_RG_DA_CC_DAC_CAL_SEL_ADDR                                 MT6336_PMIC_ANA_TYPEC_DA_INTF3_SEL
#define MT6336_RG_DA_CC_DAC_CAL_SEL_MASK                                 0xF
#define MT6336_RG_DA_CC_DAC_CAL_SEL_SHIFT                                4
#define MT6336_RG_DA_PD_CONNSW_SEL_ADDR                                  MT6336_PMIC_ANA_TYPEC_DA_INTF4_SEL
#define MT6336_RG_DA_PD_CONNSW_SEL_MASK                                  0x3
#define MT6336_RG_DA_PD_CONNSW_SEL_SHIFT                                 0
#define MT6336_RG_DA_PD_CCSW_SEL_ADDR                                    MT6336_PMIC_ANA_TYPEC_DA_INTF4_SEL
#define MT6336_RG_DA_PD_CCSW_SEL_MASK                                    0x3
#define MT6336_RG_DA_PD_CCSW_SEL_SHIFT                                   2
#define MT6336_RG_DA_NI_CCLK_SEL_ADDR                                    MT6336_PMIC_ANA_CLK_SEL1
#define MT6336_RG_DA_NI_CCLK_SEL_MASK                                    0x1
#define MT6336_RG_DA_NI_CCLK_SEL_SHIFT                                   0
#define MT6336_RG_DA_NI_BGR_TEST_CKIN_SEL_ADDR                           MT6336_PMIC_ANA_CLK_SEL1
#define MT6336_RG_DA_NI_BGR_TEST_CKIN_SEL_MASK                           0x1
#define MT6336_RG_DA_NI_BGR_TEST_CKIN_SEL_SHIFT                          1
#define MT6336_RG_DA_CC_SACLK_SEL_ADDR                                   MT6336_PMIC_ANA_CLK_SEL1
#define MT6336_RG_DA_CC_SACLK_SEL_MASK                                   0x1
#define MT6336_RG_DA_CC_SACLK_SEL_SHIFT                                  2
#define MT6336_RG_AD_PD_SLEW_CK_SEL_ADDR                                 MT6336_PMIC_ANA_CLK_SEL1
#define MT6336_RG_AD_PD_SLEW_CK_SEL_MASK                                 0x1
#define MT6336_RG_AD_PD_SLEW_CK_SEL_SHIFT                                3
#define MT6336_RG_AD_NS_CLK_26M_SEL_ADDR                                 MT6336_PMIC_ANA_CLK_SEL1
#define MT6336_RG_AD_NS_CLK_26M_SEL_MASK                                 0x1
#define MT6336_RG_AD_NS_CLK_26M_SEL_SHIFT                                4
#define MT6336_RG_AD_NI_PMU_CLK75K_SEL_ADDR                              MT6336_PMIC_ANA_CLK_SEL1
#define MT6336_RG_AD_NI_PMU_CLK75K_SEL_MASK                              0x1
#define MT6336_RG_AD_NI_PMU_CLK75K_SEL_SHIFT                             5
#define MT6336_RG_AD_NI_BASE_CLK_SEL_ADDR                                MT6336_PMIC_ANA_CLK_SEL1
#define MT6336_RG_AD_NI_BASE_CLK_SEL_MASK                                0x1
#define MT6336_RG_AD_NI_BASE_CLK_SEL_SHIFT                               6
#define MT6336_RG_AD_NI_HF_CLK_SEL_ADDR                                  MT6336_PMIC_ANA_CLK_SEL1
#define MT6336_RG_AD_NI_HF_CLK_SEL_MASK                                  0x1
#define MT6336_RG_AD_NI_HF_CLK_SEL_SHIFT                                 7
#define MT6336_RG_DA_NI_HF_SSCLK_SEL_ADDR                                MT6336_PMIC_ANA_CLK_SEL2
#define MT6336_RG_DA_NI_HF_SSCLK_SEL_MASK                                0x1
#define MT6336_RG_DA_NI_HF_SSCLK_SEL_SHIFT                               0
#define MT6336_GPIO_DIR0_ADDR                                            MT6336_PMIC_GPIO_DIR0
#define MT6336_GPIO_DIR0_MASK                                            0xFF
#define MT6336_GPIO_DIR0_SHIFT                                           0
#define MT6336_GPIO_DIR0_SET_ADDR                                        MT6336_PMIC_GPIO_DIR0_SET
#define MT6336_GPIO_DIR0_SET_MASK                                        0xFF
#define MT6336_GPIO_DIR0_SET_SHIFT                                       0
#define MT6336_GPIO_DIR0_CLR_ADDR                                        MT6336_PMIC_GPIO_DIR0_CLR
#define MT6336_GPIO_DIR0_CLR_MASK                                        0xFF
#define MT6336_GPIO_DIR0_CLR_SHIFT                                       0
#define MT6336_GPIO_DIR1_ADDR                                            MT6336_PMIC_GPIO_DIR1
#define MT6336_GPIO_DIR1_MASK                                            0x3F
#define MT6336_GPIO_DIR1_SHIFT                                           0
#define MT6336_GPIO_DIR1_SET_ADDR                                        MT6336_PMIC_GPIO_DIR1_SET
#define MT6336_GPIO_DIR1_SET_MASK                                        0x3F
#define MT6336_GPIO_DIR1_SET_SHIFT                                       0
#define MT6336_GPIO_DIR1_CLR_ADDR                                        MT6336_PMIC_GPIO_DIR1_CLR
#define MT6336_GPIO_DIR1_CLR_MASK                                        0x3F
#define MT6336_GPIO_DIR1_CLR_SHIFT                                       0
#define MT6336_GPIO_PULLEN0_ADDR                                         MT6336_PMIC_GPIO_PULLEN0
#define MT6336_GPIO_PULLEN0_MASK                                         0xFF
#define MT6336_GPIO_PULLEN0_SHIFT                                        0
#define MT6336_GPIO_PULLEN0_SET_ADDR                                     MT6336_PMIC_GPIO_PULLEN0_SET
#define MT6336_GPIO_PULLEN0_SET_MASK                                     0xFF
#define MT6336_GPIO_PULLEN0_SET_SHIFT                                    0
#define MT6336_GPIO_PULLEN0_CLR_ADDR                                     MT6336_PMIC_GPIO_PULLEN0_CLR
#define MT6336_GPIO_PULLEN0_CLR_MASK                                     0xFF
#define MT6336_GPIO_PULLEN0_CLR_SHIFT                                    0
#define MT6336_GPIO_PULLEN1_ADDR                                         MT6336_PMIC_GPIO_PULLEN1
#define MT6336_GPIO_PULLEN1_MASK                                         0x3F
#define MT6336_GPIO_PULLEN1_SHIFT                                        0
#define MT6336_GPIO_PULLEN1_SET_ADDR                                     MT6336_PMIC_GPIO_PULLEN1_SET
#define MT6336_GPIO_PULLEN1_SET_MASK                                     0x3F
#define MT6336_GPIO_PULLEN1_SET_SHIFT                                    0
#define MT6336_GPIO_PULLEN1_CLR_ADDR                                     MT6336_PMIC_GPIO_PULLEN1_CLR
#define MT6336_GPIO_PULLEN1_CLR_MASK                                     0x3F
#define MT6336_GPIO_PULLEN1_CLR_SHIFT                                    0
#define MT6336_GPIO_PULLSEL0_ADDR                                        MT6336_PMIC_GPIO_PULLSEL0
#define MT6336_GPIO_PULLSEL0_MASK                                        0xFF
#define MT6336_GPIO_PULLSEL0_SHIFT                                       0
#define MT6336_GPIO_PULLSEL0_SET_ADDR                                    MT6336_PMIC_GPIO_PULLSEL0_SET
#define MT6336_GPIO_PULLSEL0_SET_MASK                                    0xFF
#define MT6336_GPIO_PULLSEL0_SET_SHIFT                                   0
#define MT6336_GPIO_PULLSEL0_CLR_ADDR                                    MT6336_PMIC_GPIO_PULLSEL0_CLR
#define MT6336_GPIO_PULLSEL0_CLR_MASK                                    0xFF
#define MT6336_GPIO_PULLSEL0_CLR_SHIFT                                   0
#define MT6336_GPIO_PULLSEL1_ADDR                                        MT6336_PMIC_GPIO_PULLSEL1
#define MT6336_GPIO_PULLSEL1_MASK                                        0x3F
#define MT6336_GPIO_PULLSEL1_SHIFT                                       0
#define MT6336_GPIO_PULLSEL1_SET_ADDR                                    MT6336_PMIC_GPIO_PULLSEL1_SET
#define MT6336_GPIO_PULLSEL1_SET_MASK                                    0x3F
#define MT6336_GPIO_PULLSEL1_SET_SHIFT                                   0
#define MT6336_GPIO_PULLSEL1_CLR_ADDR                                    MT6336_PMIC_GPIO_PULLSEL1_CLR
#define MT6336_GPIO_PULLSEL1_CLR_MASK                                    0x3F
#define MT6336_GPIO_PULLSEL1_CLR_SHIFT                                   0
#define MT6336_GPIO_DINV0_ADDR                                           MT6336_PMIC_GPIO_DINV0
#define MT6336_GPIO_DINV0_MASK                                           0xFF
#define MT6336_GPIO_DINV0_SHIFT                                          0
#define MT6336_GPIO_DINV0_SET_ADDR                                       MT6336_PMIC_GPIO_DINV0_SET
#define MT6336_GPIO_DINV0_SET_MASK                                       0xFF
#define MT6336_GPIO_DINV0_SET_SHIFT                                      0
#define MT6336_GPIO_DINV0_CLR_ADDR                                       MT6336_PMIC_GPIO_DINV0_CLR
#define MT6336_GPIO_DINV0_CLR_MASK                                       0xFF
#define MT6336_GPIO_DINV0_CLR_SHIFT                                      0
#define MT6336_GPIO_DINV1_ADDR                                           MT6336_PMIC_GPIO_DINV1
#define MT6336_GPIO_DINV1_MASK                                           0x3F
#define MT6336_GPIO_DINV1_SHIFT                                          0
#define MT6336_GPIO_DINV1_SET_ADDR                                       MT6336_PMIC_GPIO_DINV1_SET
#define MT6336_GPIO_DINV1_SET_MASK                                       0x3F
#define MT6336_GPIO_DINV1_SET_SHIFT                                      0
#define MT6336_GPIO_DINV1_CLR_ADDR                                       MT6336_PMIC_GPIO_DINV1_CLR
#define MT6336_GPIO_DINV1_CLR_MASK                                       0x3F
#define MT6336_GPIO_DINV1_CLR_SHIFT                                      0
#define MT6336_GPIO_DOUT0_ADDR                                           MT6336_PMIC_GPIO_DOUT0
#define MT6336_GPIO_DOUT0_MASK                                           0xFF
#define MT6336_GPIO_DOUT0_SHIFT                                          0
#define MT6336_GPIO_DOUT0_SET_ADDR                                       MT6336_PMIC_GPIO_DOUT0_SET
#define MT6336_GPIO_DOUT0_SET_MASK                                       0xFF
#define MT6336_GPIO_DOUT0_SET_SHIFT                                      0
#define MT6336_GPIO_DOUT0_CLR_ADDR                                       MT6336_PMIC_GPIO_DOUT0_CLR
#define MT6336_GPIO_DOUT0_CLR_MASK                                       0xFF
#define MT6336_GPIO_DOUT0_CLR_SHIFT                                      0
#define MT6336_GPIO_DOUT1_ADDR                                           MT6336_PMIC_GPIO_DOUT1
#define MT6336_GPIO_DOUT1_MASK                                           0x3F
#define MT6336_GPIO_DOUT1_SHIFT                                          0
#define MT6336_GPIO_DOUT1_SET_ADDR                                       MT6336_PMIC_GPIO_DOUT1_SET
#define MT6336_GPIO_DOUT1_SET_MASK                                       0x3F
#define MT6336_GPIO_DOUT1_SET_SHIFT                                      0
#define MT6336_GPIO_DOUT1_CLR_ADDR                                       MT6336_PMIC_GPIO_DOUT1_CLR
#define MT6336_GPIO_DOUT1_CLR_MASK                                       0x3F
#define MT6336_GPIO_DOUT1_CLR_SHIFT                                      0
#define MT6336_GPIO_PI0_ADDR                                             MT6336_PMIC_GPIO_PI0
#define MT6336_GPIO_PI0_MASK                                             0xFF
#define MT6336_GPIO_PI0_SHIFT                                            0
#define MT6336_GPIO_PI1_ADDR                                             MT6336_PMIC_GPIO_PI1
#define MT6336_GPIO_PI1_MASK                                             0x3F
#define MT6336_GPIO_PI1_SHIFT                                            0
#define MT6336_GPIO_POE0_ADDR                                            MT6336_PMIC_GPIO_POE0
#define MT6336_GPIO_POE0_MASK                                            0xFF
#define MT6336_GPIO_POE0_SHIFT                                           0
#define MT6336_GPIO_POE1_ADDR                                            MT6336_PMIC_GPIO_POE1
#define MT6336_GPIO_POE1_MASK                                            0x3F
#define MT6336_GPIO_POE1_SHIFT                                           0
#define MT6336_GPIO0_MODE_ADDR                                           MT6336_PMIC_GPIO_MODE0
#define MT6336_GPIO0_MODE_MASK                                           0x7
#define MT6336_GPIO0_MODE_SHIFT                                          0
#define MT6336_GPIO1_MODE_ADDR                                           MT6336_PMIC_GPIO_MODE0
#define MT6336_GPIO1_MODE_MASK                                           0x7
#define MT6336_GPIO1_MODE_SHIFT                                          3
#define MT6336_GPIO_MODE0_SET_ADDR                                       MT6336_PMIC_GPIO_MODE0_SET
#define MT6336_GPIO_MODE0_SET_MASK                                       0xFF
#define MT6336_GPIO_MODE0_SET_SHIFT                                      0
#define MT6336_GPIO_MODE0_CLR_ADDR                                       MT6336_PMIC_GPIO_MODE0_CLR
#define MT6336_GPIO_MODE0_CLR_MASK                                       0xFF
#define MT6336_GPIO_MODE0_CLR_SHIFT                                      0
#define MT6336_GPIO2_MODE_ADDR                                           MT6336_PMIC_GPIO_MODE1
#define MT6336_GPIO2_MODE_MASK                                           0x7
#define MT6336_GPIO2_MODE_SHIFT                                          0
#define MT6336_GPIO3_MODE_ADDR                                           MT6336_PMIC_GPIO_MODE1
#define MT6336_GPIO3_MODE_MASK                                           0x7
#define MT6336_GPIO3_MODE_SHIFT                                          3
#define MT6336_GPIO_MODE1_SET_ADDR                                       MT6336_PMIC_GPIO_MODE1_SET
#define MT6336_GPIO_MODE1_SET_MASK                                       0xFF
#define MT6336_GPIO_MODE1_SET_SHIFT                                      0
#define MT6336_GPIO_MODE1_CLR_ADDR                                       MT6336_PMIC_GPIO_MODE1_CLR
#define MT6336_GPIO_MODE1_CLR_MASK                                       0xFF
#define MT6336_GPIO_MODE1_CLR_SHIFT                                      0
#define MT6336_GPIO4_MODE_ADDR                                           MT6336_PMIC_GPIO_MODE2
#define MT6336_GPIO4_MODE_MASK                                           0x7
#define MT6336_GPIO4_MODE_SHIFT                                          0
#define MT6336_GPIO5_MODE_ADDR                                           MT6336_PMIC_GPIO_MODE2
#define MT6336_GPIO5_MODE_MASK                                           0x7
#define MT6336_GPIO5_MODE_SHIFT                                          3
#define MT6336_GPIO_MODE2_SET_ADDR                                       MT6336_PMIC_GPIO_MODE2_SET
#define MT6336_GPIO_MODE2_SET_MASK                                       0xFF
#define MT6336_GPIO_MODE2_SET_SHIFT                                      0
#define MT6336_GPIO_MODE2_CLR_ADDR                                       MT6336_PMIC_GPIO_MODE2_CLR
#define MT6336_GPIO_MODE2_CLR_MASK                                       0xFF
#define MT6336_GPIO_MODE2_CLR_SHIFT                                      0
#define MT6336_GPIO6_MODE_ADDR                                           MT6336_PMIC_GPIO_MODE3
#define MT6336_GPIO6_MODE_MASK                                           0x7
#define MT6336_GPIO6_MODE_SHIFT                                          0
#define MT6336_GPIO7_MODE_ADDR                                           MT6336_PMIC_GPIO_MODE3
#define MT6336_GPIO7_MODE_MASK                                           0x7
#define MT6336_GPIO7_MODE_SHIFT                                          3
#define MT6336_GPIO_MODE3_SET_ADDR                                       MT6336_PMIC_GPIO_MODE3_SET
#define MT6336_GPIO_MODE3_SET_MASK                                       0xFF
#define MT6336_GPIO_MODE3_SET_SHIFT                                      0
#define MT6336_GPIO_MODE3_CLR_ADDR                                       MT6336_PMIC_GPIO_MODE3_CLR
#define MT6336_GPIO_MODE3_CLR_MASK                                       0xFF
#define MT6336_GPIO_MODE3_CLR_SHIFT                                      0
#define MT6336_GPIO8_MODE_ADDR                                           MT6336_PMIC_GPIO_MODE4
#define MT6336_GPIO8_MODE_MASK                                           0x7
#define MT6336_GPIO8_MODE_SHIFT                                          0
#define MT6336_GPIO9_MODE_ADDR                                           MT6336_PMIC_GPIO_MODE4
#define MT6336_GPIO9_MODE_MASK                                           0x7
#define MT6336_GPIO9_MODE_SHIFT                                          3
#define MT6336_GPIO_MODE4_SET_ADDR                                       MT6336_PMIC_GPIO_MODE4_SET
#define MT6336_GPIO_MODE4_SET_MASK                                       0xFF
#define MT6336_GPIO_MODE4_SET_SHIFT                                      0
#define MT6336_GPIO_MODE4_CLR_ADDR                                       MT6336_PMIC_GPIO_MODE4_CLR
#define MT6336_GPIO_MODE4_CLR_MASK                                       0xFF
#define MT6336_GPIO_MODE4_CLR_SHIFT                                      0
#define MT6336_GPIO10_MODE_ADDR                                          MT6336_PMIC_GPIO_MODE5
#define MT6336_GPIO10_MODE_MASK                                          0x7
#define MT6336_GPIO10_MODE_SHIFT                                         0
#define MT6336_GPIO11_MODE_ADDR                                          MT6336_PMIC_GPIO_MODE5
#define MT6336_GPIO11_MODE_MASK                                          0x7
#define MT6336_GPIO11_MODE_SHIFT                                         3
#define MT6336_GPIO_MODE5_SET_ADDR                                       MT6336_PMIC_GPIO_MODE5_SET
#define MT6336_GPIO_MODE5_SET_MASK                                       0xFF
#define MT6336_GPIO_MODE5_SET_SHIFT                                      0
#define MT6336_GPIO_MODE5_CLR_ADDR                                       MT6336_PMIC_GPIO_MODE5_CLR
#define MT6336_GPIO_MODE5_CLR_MASK                                       0xFF
#define MT6336_GPIO_MODE5_CLR_SHIFT                                      0
#define MT6336_GPIO12_MODE_ADDR                                          MT6336_PMIC_GPIO_MODE6
#define MT6336_GPIO12_MODE_MASK                                          0x7
#define MT6336_GPIO12_MODE_SHIFT                                         0
#define MT6336_GPIO13_MODE_ADDR                                          MT6336_PMIC_GPIO_MODE6
#define MT6336_GPIO13_MODE_MASK                                          0x7
#define MT6336_GPIO13_MODE_SHIFT                                         3
#define MT6336_GPIO_MODE6_SET_ADDR                                       MT6336_PMIC_GPIO_MODE6_SET
#define MT6336_GPIO_MODE6_SET_MASK                                       0xFF
#define MT6336_GPIO_MODE6_SET_SHIFT                                      0
#define MT6336_GPIO_MODE6_CLR_ADDR                                       MT6336_PMIC_GPIO_MODE6_CLR
#define MT6336_GPIO_MODE6_CLR_MASK                                       0xFF
#define MT6336_GPIO_MODE6_CLR_SHIFT                                      0
#define MT6336_GPIO_RSV_ADDR                                             MT6336_PMIC_GPIO_RSV
#define MT6336_GPIO_RSV_MASK                                             0xFF
#define MT6336_GPIO_RSV_SHIFT                                            0

typedef enum {
	MT6336_CID,
	MT6336_SWCID,
	MT6336_HWCID,
	MT6336_I2C_CONFIG,
	MT6336_I2C_RD_LEN,
	MT6336_TEST_OUT0,
	MT6336_TEST_OUT1,
	MT6336_RG_MON_GRP_SEL,
	MT6336_RG_MON_FLAG_SEL,
	MT6336_RG_SWCHR_FLAG_SEL,
	MT6336_RG_NANDTREE_MODE,
	MT6336_RG_TEST_AUXADC,
	MT6336_RG_EFUSE_MODE,
	MT6336_RG_TEST_SWCHR,
	MT6336_RG_ANAIF_BYPASS_MODE,
	MT6336_RG_TEST_RSTB,
	MT6336_RG_TEST_RESET_SEL,
	MT6336_TESTMODE_SW,
	MT6336_PMU_TEST_MODE_SCAN,
	MT6336_RG_PMU_TDSEL,
	MT6336_RG_I2C_TDSEL,
	MT6336_RG_PMU_RDSEL,
	MT6336_RG_I2C_RDSEL,
	MT6336_RG_I2C_FILTER,
	MT6336_RG_SMT_SCL,
	MT6336_RG_SMT_SDA,
	MT6336_RG_SMT_IRQ,
	MT6336_RG_SMT_OTG,
	MT6336_RG_SMT_STROBE,
	MT6336_RG_SMT_TXMASK,
	MT6336_RG_SMT_GPIO2,
	MT6336_RG_SMT_GPIO3,
	MT6336_RG_SMT_GPIO4,
	MT6336_RG_SMT_GPIO5,
	MT6336_RG_SMT_GPIO6,
	MT6336_RG_SMT_GPIO7,
	MT6336_RG_SMT_GPIO0,
	MT6336_RG_SMT_GPIO1,
	MT6336_RG_OCTL_SCL,
	MT6336_RG_OCTL_SDA,
	MT6336_RG_OCTL_IRQ,
	MT6336_RG_OCTL_OTG,
	MT6336_RG_OCTL_STROBE,
	MT6336_RG_OCTL_TXMASK,
	MT6336_RG_OCTL_GPIO0,
	MT6336_RG_OCTL_GPIO1,
	MT6336_RG_OCTL_GPIO2,
	MT6336_RG_OCTL_GPIO3,
	MT6336_RG_OCTL_GPIO4,
	MT6336_RG_OCTL_GPIO5,
	MT6336_RG_OCTL_GPIO6,
	MT6336_RG_OCTL_GPIO7,
	MT6336_TOP_STATUS,
	MT6336_TOP_STATUS_SET,
	MT6336_TOP_STATUS_CLR,
	MT6336_TOP_RSV0,
	MT6336_TOP_RSV1,
	MT6336_TOP_RSV2,
	MT6336_CLK_RSV_CON0_RSV,
	MT6336_CLK_RSV_CON1_RSV,
	MT6336_RG_CLKSQ_EN_PD,
	MT6336_RG_CLKSQ_EN_FQR,
	MT6336_RG_CLKSQ_EN_RSV,
	MT6336_DA_QI_CLKSQ_EN,
	MT6336_TOP_CLKSQ_SET,
	MT6336_TOP_CLKSQ_CLR,
	MT6336_DA_QI_BASE_CLK_TRIM,
	MT6336_RG_BASE_CLK_TRIM_EN,
	MT6336_RG_BASE_CLK_TRIM_RATE,
	MT6336_RG_BASE_CLK_TRIM,
	MT6336_DA_QI_OSC_TRIM,
	MT6336_RG_OSC_TRIM_EN,
	MT6336_RG_OSC_TRIM_RATE,
	MT6336_RG_OSC_CLK_TRIM,
	MT6336_CLK_HF_6M_CK_TST_DIS,
	MT6336_CLK_BASE_300K_CK_TST_DIS,
	MT6336_CLK_CLK_26M_CK_TST_DIS,
	MT6336_CLK_PD_SLEW_CK_TST_DIS,
	MT6336_CLK_PMU_75K_CK_TST_DIS,
	MT6336_CLK_CKROOTTST_CON0_RSV,
	MT6336_CLK_HF_6M_CK_TSTSEL,
	MT6336_CLK_BASE_300K_CK_TSTSEL,
	MT6336_CLK_CLK_26M_CK_TSTSEL,
	MT6336_CLK_PD_SLEW_CK_TSTSEL,
	MT6336_CLK_PMU_75K_CK_TSTSEL,
	MT6336_CLK_CKROOTTST_CON1_RSV,
	MT6336_CLK_TOP_AO_75K_CK_PDN,
	MT6336_CLK_SWCHR_6M_SW_CK_PDN,
	MT6336_CLK_SWCHR_6M_CK_PDN,
	MT6336_CLK_SWCHR_3M_CK_PDN,
	MT6336_CLK_SWCHR_2M_SW_CK_PDN,
	MT6336_CLK_SWCHR_2M_CK_PDN,
	MT6336_CLK_SWCHR_AO_300K_CK_PDN,
	MT6336_CLK_SWCHR_300K_CK_PDN,
	MT6336_CLK_CKPDN_CON0_SET,
	MT6336_CLK_CKPDN_CON0_CLR,
	MT6336_CLK_SWCHR_75K_CK_PDN,
	MT6336_CLK_SWCHR_AO_1K_CK_PDN,
	MT6336_CLK_SWCHR_1K_CK_PDN,
	MT6336_CLK_AUXADC_300K_CK_PDN,
	MT6336_CLK_AUXADC_CK_PDN,
	MT6336_CLK_DRV_CHRIND_CK_PDN,
	MT6336_CLK_DRV_75K_CK_PDN,
	MT6336_CLK_EFUSE_CK_PDN,
	MT6336_CLK_CKPDN_CON1_SET,
	MT6336_CLK_CKPDN_CON1_CLR,
	MT6336_CLK_FQMTR_CK_PDN,
	MT6336_CLK_FQMTR_26M_CK_PDN,
	MT6336_CLK_FQMTR_75K_CK_PDN,
	MT6336_CLK_INTRP_CK_PDN,
	MT6336_CLK_TRIM_75K_CK_PDN,
	MT6336_CLK_I2C_CK_PDN,
	MT6336_CLK_REG_CK_PDN,
	MT6336_CLK_REG_CK_I2C_PDN,
	MT6336_CLK_CKPDN_CON2_SET,
	MT6336_CLK_CKPDN_CON2_CLR,
	MT6336_CLK_REG_6M_W1C_CK_PDN,
	MT6336_CLK_RSTCTL_WDT_ALL_CK_PDN,
	MT6336_CLK_RSTCTL_RST_GLOBAL_CK_PDN,
	MT6336_CLK_TYPE_C_CC_CK_PDN,
	MT6336_CLK_TYPE_C_PD_CK_PDN,
	MT6336_CLK_TYPE_C_CSR_CK_PDN,
	MT6336_CLK_RSV_PDN_CON3,
	MT6336_CLK_CKPDN_CON3_SET,
	MT6336_CLK_CKPDN_CON3_CLR,
	MT6336_CLK_AUXADC_CK_PDN_HWEN,
	MT6336_CLK_EFUSE_CK_PDN_HWEN,
	MT6336_CLK_RSV_PDN_HWEN,
	MT6336_CLK_CKPDN_HWEN_CON0_SET,
	MT6336_CLK_CKPDN_HWEN_CON0_CLR,
	MT6336_CLK_FQMTR_CK_CKSEL,
	MT6336_CLK_RSV_CKSEL,
	MT6336_CLK_CKSEL_CON0_SET,
	MT6336_CLK_CKSEL_CON0_CLR,
	MT6336_CLK_AUXADC_CK_DIVSEL,
	MT6336_CLK_RSV_DIVSEL,
	MT6336_CLK_CKDIVSEL_CON0_SET,
	MT6336_CLK_CKDIVSEL_CON0_CLR,
	MT6336_CLK_TOP_AO_75K_CK_TSTSEL,
	MT6336_CLK_AUXADC_CK_TSTSEL,
	MT6336_CLK_DRV_CHRIND_CK_TSTSEL,
	MT6336_CLK_FQMTR_CK_TSTSEL,
	MT6336_CLK_REG_CK_TSTSEL,
	MT6336_CLK_REG_CK_I2C_TSTSEL,
	MT6336_CLK_RSV_TSTSEL,
	MT6336_CLK_SWCHR_6M_LOWQ_PDN_DIS,
	MT6336_CLK_SWCHR_3M_LOWQ_PDN_DIS,
	MT6336_CLK_SWCHR_2M_LOWQ_PDN_DIS,
	MT6336_CLK_SWCHR_300K_LOWQ_PDN_DIS,
	MT6336_CLK_SWCHR_75K_LOWQ_PDN_DIS,
	MT6336_CLK_SWCHR_1K_LOWQ_PDN_DIS,
	MT6336_CLK_TYPE_C_CC_LOWQ_PDN_DIS,
	MT6336_CLK_TYPE_C_PD_LOWQ_PDN_DIS,
	MT6336_CLK_LOWQ_PDN_DIS_CON0_SET,
	MT6336_CLK_LOWQ_PDN_DIS_CON0_CLR,
	MT6336_CLK_TYPE_C_CSR_LOWQ_PDN_DIS,
	MT6336_CLK_AUXADC_LOWQ_PDN_DIS,
	MT6336_CLK_AUXADC_300K_LOWQ_PDN_DIS,
	MT6336_CLK_DRV_CHRIND_LOWQ_PDN_DIS,
	MT6336_CLK_DRV_75K_LOWQ_PDN_DIS,
	MT6336_CLK_FQMTR_LOWQ_PDN_DIS,
	MT6336_CLK_FQMTR_26M_LOWQ_PDN_DIS,
	MT6336_CLK_FQMTR_75K_LOWQ_PDN_DIS,
	MT6336_CLK_LOWQ_PDN_DIS_CON1_SET,
	MT6336_CLK_LOWQ_PDN_DIS_CON1_CLR,
	MT6336_CLK_REG_LOWQ_PDN_DIS,
	MT6336_CLK_REG_6M_W1C_LOWQ_PDN_DIS,
	MT6336_CLK_INTRP_LOWQ_PDN_DIS,
	MT6336_CLK_TRIM_75K_LOWQ_PDN_DIS,
	MT6336_CLK_EFUSE_LOWQ_PDN_DIS,
	MT6336_CLK_RSTCTL_RST_GLOBAL_LOWQ_PDN_DIS,
	MT6336_CLK_RSV_LOWQ_PDN_DIS,
	MT6336_CLK_LOWQ_PDN_DIS_CON2_SET,
	MT6336_CLK_LOWQ_PDN_DIS_CON2_CLR,
	MT6336_CLOCK_RSV0,
	MT6336_CLOCK_RSV1,
	MT6336_CLOCK_RSV2,
	MT6336_RG_VPWRIN_RST,
	MT6336_RG_EFUSE_MAN_RST,
	MT6336_RG_AUXADC_RST,
	MT6336_RG_AUXADC_REG_RST,
	MT6336_RG_CLK_TRIM_RST,
	MT6336_RG_CLKCTL_RST,
	MT6336_RG_DRIVER_RST,
	MT6336_RG_FQMTR_RST,
	MT6336_TOP_RST_CON0_SET,
	MT6336_TOP_RST_CON0_CLR,
	MT6336_RG_BANK0_REG_RST,
	MT6336_RG_BANK1_REG_RST,
	MT6336_RG_BANK2_REG_RST,
	MT6336_RG_BANK3_REG_RST,
	MT6336_RG_BANK4_REG_RST,
	MT6336_RG_BANK5_REG_RST,
	MT6336_RG_BANK6_REG_RST,
	MT6336_RG_GPIO_REG_RST,
	MT6336_TOP_RST_CON1_SET,
	MT6336_TOP_RST_CON1_CLR,
	MT6336_RG_SWCHR_RST,
	MT6336_RG_SWCHR_OTG_REG_RST,
	MT6336_RG_SWCHR_SHIP_REG_RST,
	MT6336_RG_SWCHR_LOWQ_REG_RST,
	MT6336_RG_SWCHR_PLUGIN_REG_RST,
	MT6336_RG_SWCHR_PLUGOUT_REG_RST,
	MT6336_TOP_RST_CON2_RSV,
	MT6336_RG_SWCHR_WDT_CONFIG,
	MT6336_TOP_RST_CON2_SET,
	MT6336_TOP_RST_CON2_CLR,
	MT6336_RG_TESTCTL_RST,
	MT6336_RG_INTCTL_RST,
	MT6336_RG_IO_RST,
	MT6336_RG_I2C_RST,
	MT6336_RG_TYPE_C_CC_RST,
	MT6336_RG_TYPE_C_PD_RST,
	MT6336_TOP_RST_CON3_RSV,
	MT6336_TOP_RST_MISC_SET,
	MT6336_TOP_RST_MISC_CLR,
	MT6336_VPWRIN_RSTB_STATUS,
	MT6336_SWCHR_RSTB_STATUS,
	MT6336_SWCHR_OTG_REG_RSTB_STATUS,
	MT6336_SWCHR_SHIP_REG_RSTB_STATUS,
	MT6336_SWCHR_LOWQ_REG_RSTB_STATUS,
	MT6336_TOP_RST_STATUS_RSV,
	MT6336_TOP_RST_STATUS_SET,
	MT6336_TOP_RST_STATUS_CLR,
	MT6336_TOP_RST_RSV0,
	MT6336_TOP_RST_RSV1,
	MT6336_TOP_RST_RSV2,
	MT6336_TOP_RST_RSV3,
	MT6336_RG_INT_EN_CHR_VBUS_PLUGIN,
	MT6336_RG_INT_EN_CHR_VBUS_PLUGOUT,
	MT6336_RG_INT_EN_STATE_BUCK_BACKGROUND,
	MT6336_RG_INT_EN_STATE_BUCK_EOC,
	MT6336_RG_INT_EN_STATE_BUCK_PRECC0,
	MT6336_RG_INT_EN_STATE_BUCK_PRECC1,
	MT6336_RG_INT_EN_STATE_BUCK_FASTCC,
	MT6336_RG_INT_EN_CHR_WEAKBUS,
	MT6336_INT_CON0_SET,
	MT6336_INT_CON0_CLR,
	MT6336_RG_INT_EN_CHR_SYS_OVP,
	MT6336_RG_INT_EN_CHR_BAT_OVP,
	MT6336_RG_INT_EN_CHR_VBUS_OVP,
	MT6336_RG_INT_EN_CHR_VBUS_UVLO,
	MT6336_RG_INT_EN_CHR_ICHR_ITERM,
	MT6336_RG_INT_EN_CHIP_TEMP_OVERHEAT,
	MT6336_RG_INT_EN_CHIP_MBATPP_DIS_OC_DIG,
	MT6336_RG_INT_EN_OTG_BVALID,
	MT6336_INT_CON1_SET,
	MT6336_INT_CON1_CLR,
	MT6336_RG_INT_EN_OTG_VM_UVLO,
	MT6336_RG_INT_EN_OTG_VM_OVP,
	MT6336_RG_INT_EN_OTG_VBAT_UVLO,
	MT6336_RG_INT_EN_OTG_VM_OLP,
	MT6336_RG_INT_EN_FLASH_VFLA_UVLO,
	MT6336_RG_INT_EN_FLASH_VFLA_OVP,
	MT6336_RG_INT_EN_LED1_SHORT,
	MT6336_RG_INT_EN_LED1_OPEN,
	MT6336_INT_CON2_SET,
	MT6336_INT_CON2_CLR,
	MT6336_RG_INT_EN_LED2_SHORT,
	MT6336_RG_INT_EN_LED2_OPEN,
	MT6336_RG_INT_EN_FLASH_TIMEOUT,
	MT6336_RG_INT_EN_TORCH_TIMEOUT,
	MT6336_RG_INT_EN_DD_VBUS_IN_VALID,
	MT6336_RG_INT_EN_WDT_TIMEOUT,
	MT6336_RG_INT_EN_SAFETY_TIMEOUT,
	MT6336_RG_INT_EN_CHR_AICC_DONE,
	MT6336_INT_CON3_SET,
	MT6336_INT_CON3_CLR,
	MT6336_RG_INT_EN_ADC_TEMP_HT,
	MT6336_RG_INT_EN_ADC_TEMP_LT,
	MT6336_RG_INT_EN_ADC_JEITA_HOT,
	MT6336_RG_INT_EN_ADC_JEITA_WARM,
	MT6336_RG_INT_EN_ADC_JEITA_COOL,
	MT6336_RG_INT_EN_ADC_JEITA_COLD,
	MT6336_RG_INT_EN_VBUS_SOFT_OVP_H,
	MT6336_RG_INT_EN_VBUS_SOFT_OVP_L,
	MT6336_INT_CON4_SET,
	MT6336_INT_CON4_CLR,
	MT6336_RG_INT_EN_CHR_BAT_RECHG,
	MT6336_RG_INT_EN_BAT_TEMP_H,
	MT6336_RG_INT_EN_BAT_TEMP_L,
	MT6336_RG_INT_EN_TYPE_C_L_MIN,
	MT6336_RG_INT_EN_TYPE_C_L_MAX,
	MT6336_RG_INT_EN_TYPE_C_H_MIN,
	MT6336_RG_INT_EN_TYPE_C_H_MAX,
	MT6336_RG_INT_EN_TYPE_C_CC_IRQ,
	MT6336_INT_CON5_SET,
	MT6336_INT_CON5_CLR,
	MT6336_RG_INT_EN_TYPE_C_PD_IRQ,
	MT6336_RG_INT_EN_DD_PE_STATUS,
	MT6336_RG_INT_EN_BC12_V2P7_TIMEOUT,
	MT6336_RG_INT_EN_BC12_V3P2_TIMEOUT,
	MT6336_RG_INT_EN_DD_BC12_STATUS,
	MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_SW,
	MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_GLOBAL,
	MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_LONG_PRESS,
	MT6336_INT_CON6_SET,
	MT6336_INT_CON6_CLR,
	MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_WDT,
	MT6336_RG_INT_EN_DD_SWCHR_PLUGOUT_PULSEB_RISING,
	MT6336_RG_INT_EN_DD_SWCHR_PLUGOUT_PULSEB_LEVEL,
	MT6336_RG_INT_EN_DD_SWCHR_PLUGIN_PULSEB,
	MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_SHIP,
	MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_BAT_OC,
	MT6336_RG_INT_EN_DD_SWCHR_TOP_RST_BAT_DEAD,
	MT6336_RG_INT_EN_DD_SWCHR_BUCK_MODE,
	MT6336_INT_CON7_SET,
	MT6336_INT_CON7_CLR,
	MT6336_RG_INT_EN_DD_SWCHR_LOWQ_MODE,
	MT6336_RG_INT_EN_DD_SWCHR_SHIP_MODE,
	MT6336_RG_INT_EN_DD_SWCHR_BAT_OC_MODE,
	MT6336_RG_INT_EN_DD_SWCHR_BAT_DEAD_MODE,
	MT6336_RG_INT_EN_DD_SWCHR_RST_SW_MODE,
	MT6336_RG_INT_EN_DD_SWCHR_RST_GLOBAL_MODE,
	MT6336_RG_INT_EN_DD_SWCHR_RST_WDT_MODE,
	MT6336_RG_INT_EN_DD_SWCHR_RST_LONG_PRESS_MODE,
	MT6336_INT_CON8_SET,
	MT6336_INT_CON8_CLR,
	MT6336_RG_INT_EN_DD_SWCHR_CHR_SUSPEND_STATE,
	MT6336_RG_INT_EN_DD_SWCHR_BUCK_PROTECT_STATE,
	MT6336_POLARITY,
	MT6336_INT_CON9_SET,
	MT6336_INT_CON9_CLR,
	MT6336_RG_INT_MASK_CHR_VBUS_PLUGIN,
	MT6336_RG_INT_MASK_CHR_VBUS_PLUGOUT,
	MT6336_RG_INT_MASK_STATE_BUCK_BACKGROUND,
	MT6336_RG_INT_MASK_STATE_BUCK_EOC,
	MT6336_RG_INT_MASK_STATE_BUCK_PRECC0,
	MT6336_RG_INT_MASK_STATE_BUCK_PRECC1,
	MT6336_RG_INT_MASK_STATE_BUCK_FASTCC,
	MT6336_RG_INT_MASK_CHR_WEAKBUS,
	MT6336_INT_MASK_CON0_SET,
	MT6336_INT_MASK_CON0_CLR,
	MT6336_RG_INT_MASK_CHR_SYS_OVP,
	MT6336_RG_INT_MASK_CHR_BAT_OVP,
	MT6336_RG_INT_MASK_CHR_VBUS_OVP,
	MT6336_RG_INT_MASK_CHR_VBUS_UVLO,
	MT6336_RG_INT_MASK_CHR_ICHR_ITERM,
	MT6336_RG_INT_MASK_CHIP_TEMP_OVERHEAT,
	MT6336_RG_INT_MASK_CHIP_MBATPP_DIS_OC_DIG,
	MT6336_RG_INT_MASK_OTG_BVALID,
	MT6336_INT_MASK_CON1_SET,
	MT6336_INT_MASK_CON1_CLR,
	MT6336_RG_INT_MASK_OTG_VM_UVLO,
	MT6336_RG_INT_MASK_OTG_VM_OVP,
	MT6336_RG_INT_MASK_OTG_VBAT_UVLO,
	MT6336_RG_INT_MASK_OTG_VM_OLP,
	MT6336_RG_INT_MASK_FLASH_VFLA_UVLO,
	MT6336_RG_INT_MASK_FLASH_VFLA_OVP,
	MT6336_RG_INT_MASK_LED1_SHORT,
	MT6336_RG_INT_MASK_LED1_OPEN,
	MT6336_INT_MASK_CON2_SET,
	MT6336_INT_MASK_CON2_CLR,
	MT6336_RG_INT_MASK_LED2_SHORT,
	MT6336_RG_INT_MASK_LED2_OPEN,
	MT6336_RG_INT_MASK_FLASH_TIMEOUT,
	MT6336_RG_INT_MASK_TORCH_TIMEOUT,
	MT6336_RG_INT_MASK_DD_VBUS_IN_VALID,
	MT6336_RG_INT_MASK_WDT_TIMEOUT,
	MT6336_RG_INT_MASK_SAFETY_TIMEOUT,
	MT6336_RG_INT_MASK_CHR_AICC_DONE,
	MT6336_INT_MASK_CON3_SET,
	MT6336_INT_MASK_CON3_CLR,
	MT6336_RG_INT_MASK_ADC_TEMP_HT,
	MT6336_RG_INT_MASK_ADC_TEMP_LT,
	MT6336_RG_INT_MASK_ADC_JEITA_HOT,
	MT6336_RG_INT_MASK_ADC_JEITA_WARM,
	MT6336_RG_INT_MASK_ADC_JEITA_COOL,
	MT6336_RG_INT_MASK_ADC_JEITA_COLD,
	MT6336_RG_INT_MASK_VBUS_SOFT_OVP_H,
	MT6336_RG_INT_MASK_VBUS_SOFT_OVP_L,
	MT6336_INT_MASK_CON4_SET,
	MT6336_INT_MASK_CON4_CLR,
	MT6336_RG_INT_MASK_CHR_BAT_RECHG,
	MT6336_RG_INT_MASK_BAT_TEMP_H,
	MT6336_RG_INT_MASK_BAT_TEMP_L,
	MT6336_RG_INT_MASK_TYPE_C_L_MIN,
	MT6336_RG_INT_MASK_TYPE_C_L_MAX,
	MT6336_RG_INT_MASK_TYPE_C_H_MIN,
	MT6336_RG_INT_MASK_TYPE_C_H_MAX,
	MT6336_RG_INT_MASK_TYPE_C_CC_IRQ,
	MT6336_INT_MASK_CON5_SET,
	MT6336_INT_MASK_CON5_CLR,
	MT6336_RG_INT_MASK_TYPE_C_PD_IRQ,
	MT6336_RG_INT_MASK_DD_PE_STATUS,
	MT6336_RG_INT_MASK_BC12_V2P7_TIMEOUT,
	MT6336_RG_INT_MASK_BC12_V3P2_TIMEOUT,
	MT6336_RG_INT_MASK_DD_BC12_STATUS,
	MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_SW,
	MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_GLOBAL,
	MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_LONG_PRESS,
	MT6336_INT_MASK_CON6_SET,
	MT6336_INT_MASK_CON6_CLR,
	MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_WDT,
	MT6336_RG_INT_MASK_DD_SWCHR_PLUGOUT_PULSEB_RISING,
	MT6336_RG_INT_MASK_DD_SWCHR_PLUGOUT_PULSEB_LEVEL,
	MT6336_RG_INT_MASK_DD_SWCHR_PLUGIN_PULSEB,
	MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_SHIP,
	MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_BAT_OC,
	MT6336_RG_INT_MASK_DD_SWCHR_TOP_RST_BAT_DEAD,
	MT6336_RG_INT_MASK_DD_SWCHR_BUCK_MODE,
	MT6336_INT_MASK_CON7_SET,
	MT6336_INT_MASK_CON7_CLR,
	MT6336_RG_INT_MASK_DD_SWCHR_LOWQ_MODE,
	MT6336_RG_INT_MASK_DD_SWCHR_SHIP_MODE,
	MT6336_RG_INT_MASK_DD_SWCHR_BAT_OC_MODE,
	MT6336_RG_INT_MASK_DD_SWCHR_BAT_DEAD_MODE,
	MT6336_RG_INT_MASK_DD_SWCHR_RST_SW_MODE,
	MT6336_RG_INT_MASK_DD_SWCHR_RST_GLOBAL_MODE,
	MT6336_RG_INT_MASK_DD_SWCHR_RST_WDT_MODE,
	MT6336_RG_INT_MASK_DD_SWCHR_RST_LONG_PRESS_MODE,
	MT6336_INT_MASK_CON8_SET,
	MT6336_INT_MASK_CON8_CLR,
	MT6336_RG_INT_MASK_DD_SWCHR_CHR_SUSPEND_STATE,
	MT6336_RG_INT_MASK_DD_SWCHR_BUCK_PROTECT_STATE,
	MT6336_INT_MASK_CON9_SET,
	MT6336_INT_MASK_CON9_CLR,
	MT6336_RG_INT_STATUS_CHR_VBUS_PLUGIN,
	MT6336_RG_INT_STATUS_CHR_VBUS_PLUGOUT,
	MT6336_RG_INT_STATUS_STATE_BUCK_BACKGROUND,
	MT6336_RG_INT_STATUS_STATE_BUCK_EOC,
	MT6336_RG_INT_STATUS_STATE_BUCK_PRECC0,
	MT6336_RG_INT_STATUS_STATE_BUCK_PRECC1,
	MT6336_RG_INT_STATUS_STATE_BUCK_FASTCC,
	MT6336_RG_INT_STATUS_CHR_WEAKBUS,
	MT6336_RG_INT_STATUS_CHR_SYS_OVP,
	MT6336_RG_INT_STATUS_CHR_BAT_OVP,
	MT6336_RG_INT_STATUS_CHR_VBUS_OVP,
	MT6336_RG_INT_STATUS_CHR_VBUS_UVLO,
	MT6336_RG_INT_STATUS_CHR_ICHR_ITERM,
	MT6336_RG_INT_STATUS_CHIP_TEMP_OVERHEAT,
	MT6336_RG_INT_STATUS_CHIP_MBATPP_DIS_OC_DIG,
	MT6336_RG_INT_STATUS_OTG_BVALID,
	MT6336_RG_INT_STATUS_OTG_VM_UVLO,
	MT6336_RG_INT_STATUS_OTG_VM_OVP,
	MT6336_RG_INT_STATUS_OTG_VBAT_UVLO,
	MT6336_RG_INT_STATUS_OTG_VM_OLP,
	MT6336_RG_INT_STATUS_FLASH_VFLA_UVLO,
	MT6336_RG_INT_STATUS_FLASH_VFLA_OVP,
	MT6336_RG_INT_STATUS_LED1_SHORT,
	MT6336_RG_INT_STATUS_LED1_OPEN,
	MT6336_RG_INT_STATUS_LED2_SHORT,
	MT6336_RG_INT_STATUS_LED2_OPEN,
	MT6336_RG_INT_STATUS_FLASH_TIMEOUT,
	MT6336_RG_INT_STATUS_TORCH_TIMEOUT,
	MT6336_RG_INT_STATUS_DD_VBUS_IN_VALID,
	MT6336_RG_INT_STATUS_WDT_TIMEOUT,
	MT6336_RG_INT_STATUS_SAFETY_TIMEOUT,
	MT6336_RG_INT_STATUS_CHR_AICC_DONE,
	MT6336_RG_INT_STATUS_ADC_TEMP_HT,
	MT6336_RG_INT_STATUS_ADC_TEMP_LT,
	MT6336_RG_INT_STATUS_ADC_JEITA_HOT,
	MT6336_RG_INT_STATUS_ADC_JEITA_WARM,
	MT6336_RG_INT_STATUS_ADC_JEITA_COOL,
	MT6336_RG_INT_STATUS_ADC_JEITA_COLD,
	MT6336_RG_INT_STATUS_VBUS_SOFT_OVP_H,
	MT6336_RG_INT_STATUS_VBUS_SOFT_OVP_L,
	MT6336_RG_INT_STATUS_CHR_BAT_RECHG,
	MT6336_RG_INT_STATUS_BAT_TEMP_H,
	MT6336_RG_INT_STATUS_BAT_TEMP_L,
	MT6336_RG_INT_STATUS_TYPE_C_L_MIN,
	MT6336_RG_INT_STATUS_TYPE_C_L_MAX,
	MT6336_RG_INT_STATUS_TYPE_C_H_MIN,
	MT6336_RG_INT_STATUS_TYPE_C_H_MAX,
	MT6336_RG_INT_STATUS_TYPE_C_CC_IRQ,
	MT6336_RG_INT_STATUS_TYPE_C_PD_IRQ,
	MT6336_RG_INT_STATUS_DD_PE_STATUS,
	MT6336_RG_INT_STATUS_BC12_V2P7_TIMEOUT,
	MT6336_RG_INT_STATUS_BC12_V3P2_TIMEOUT,
	MT6336_RG_INT_STATUS_DD_BC12_STATUS,
	MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_SW,
	MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_GLOBAL,
	MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_LONG_PRESS,
	MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_WDT,
	MT6336_RG_INT_STATUS_DD_SWCHR_PLUGOUT_PULSEB_RISING,
	MT6336_RG_INT_STATUS_DD_SWCHR_PLUGOUT_PULSEB_LEVEL,
	MT6336_RG_INT_STATUS_DD_SWCHR_PLUGIN_PULSEB,
	MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_SHIP,
	MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_BAT_OC,
	MT6336_RG_INT_STATUS_DD_SWCHR_TOP_RST_BAT_DEAD,
	MT6336_RG_INT_STATUS_DD_SWCHR_BUCK_MODE,
	MT6336_RG_INT_STATUS_DD_SWCHR_LOWQ_MODE,
	MT6336_RG_INT_STATUS_DD_SWCHR_SHIP_MODE,
	MT6336_RG_INT_STATUS_DD_SWCHR_BAT_OC_MODE,
	MT6336_RG_INT_STATUS_DD_SWCHR_BAT_DEAD_MODE,
	MT6336_RG_INT_STATUS_DD_SWCHR_RST_SW_MODE,
	MT6336_RG_INT_STATUS_DD_SWCHR_RST_GLOBAL_MODE,
	MT6336_RG_INT_STATUS_DD_SWCHR_RST_WDT_MODE,
	MT6336_RG_INT_STATUS_DD_SWCHR_RST_LONG_PRESS_MODE,
	MT6336_RG_INT_STATUS_DD_SWCHR_CHR_SUSPEND_STATE,
	MT6336_RG_INT_STATUS_DD_SWCHR_BUCK_PROTECT_STATE,
	MT6336_RG_INT_RAW_STATUS_CHR_VBUS_PLUGIN,
	MT6336_RG_INT_RAW_STATUS_CHR_VBUS_PLUGOUT,
	MT6336_RG_INT_RAW_STATUS_STATE_BUCK_BACKGROUND,
	MT6336_RG_INT_RAW_STATUS_STATE_BUCK_EOC,
	MT6336_RG_INT_RAW_STATUS_STATE_BUCK_PRECC0,
	MT6336_RG_INT_RAW_STATUS_STATE_BUCK_PRECC1,
	MT6336_RG_INT_RAW_STATUS_STATE_BUCK_FASTCC,
	MT6336_RG_INT_RAW_STATUS_CHR_WEAKBUS,
	MT6336_RG_INT_RAW_STATUS_CHR_SYS_OVP,
	MT6336_RG_INT_RAW_STATUS_CHR_BAT_OVP,
	MT6336_RG_INT_RAW_STATUS_CHR_VBUS_OVP,
	MT6336_RG_INT_RAW_STATUS_CHR_VBUS_UVLO,
	MT6336_RG_INT_RAW_STATUS_CHR_ICHR_ITERM,
	MT6336_RG_INT_RAW_STATUS_CHIP_TEMP_OVERHEAT,
	MT6336_RG_INT_RAW_STATUS_CHIP_MBATPP_DIS_OC_DIG,
	MT6336_RG_INT_RAW_STATUS_OTG_BVALID,
	MT6336_RG_INT_RAW_STATUS_OTG_VM_UVLO,
	MT6336_RG_INT_RAW_STATUS_OTG_VM_OVP,
	MT6336_RG_INT_RAW_STATUS_OTG_VBAT_UVLO,
	MT6336_RG_INT_RAW_STATUS_OTG_VM_OLP,
	MT6336_RG_INT_RAW_STATUS_FLASH_VFLA_UVLO,
	MT6336_RG_INT_RAW_STATUS_FLASH_VFLA_OVP,
	MT6336_RG_INT_RAW_STATUS_LED1_SHORT,
	MT6336_RG_INT_RAW_STATUS_LED1_OPEN,
	MT6336_RG_INT_RAW_STATUS_LED2_SHORT,
	MT6336_RG_INT_RAW_STATUS_LED2_OPEN,
	MT6336_RG_INT_RAW_STATUS_FLASH_TIMEOUT,
	MT6336_RG_INT_RAW_STATUS_TORCH_TIMEOUT,
	MT6336_RG_INT_RAW_STATUS_DD_VBUS_IN_VALID,
	MT6336_RG_INT_RAW_STATUS_WDT_TIMEOUT,
	MT6336_RG_INT_RAW_STATUS_SAFETY_TIMEOUT,
	MT6336_RG_INT_RAW_STATUS_CHR_AICC_DONE,
	MT6336_RG_INT_RAW_STATUS_ADC_TEMP_HT,
	MT6336_RG_INT_RAW_STATUS_ADC_TEMP_LT,
	MT6336_RG_INT_RAW_STATUS_ADC_JEITA_HOT,
	MT6336_RG_INT_RAW_STATUS_ADC_JEITA_WARM,
	MT6336_RG_INT_RAW_STATUS_ADC_JEITA_COOL,
	MT6336_RG_INT_RAW_STATUS_ADC_JEITA_COLD,
	MT6336_RG_INT_RAW_STATUS_VBUS_SOFT_OVP_H,
	MT6336_RG_INT_RAW_STATUS_VBUS_SOFT_OVP_L,
	MT6336_RG_INT_RAW_STATUS_CHR_BAT_RECHG,
	MT6336_RG_INT_RAW_STATUS_BAT_TEMP_H,
	MT6336_RG_INT_RAW_STATUS_BAT_TEMP_L,
	MT6336_RG_INT_RAW_STATUS_TYPE_C_L_MIN,
	MT6336_RG_INT_RAW_STATUS_TYPE_C_L_MAX,
	MT6336_RG_INT_RAW_STATUS_TYPE_C_H_MIN,
	MT6336_RG_INT_RAW_STATUS_TYPE_C_H_MAX,
	MT6336_RG_INT_RAW_STATUS_TYPE_C_CC_IRQ,
	MT6336_RG_INT_RAW_STATUS_TYPE_C_PD_IRQ,
	MT6336_RG_INT_RAW_STATUS_DD_PE_STATUS,
	MT6336_RG_INT_RAW_STATUS_BC12_V2P7_TIMEOUT,
	MT6336_RG_INT_RAW_STATUS_BC12_V3P2_TIMEOUT,
	MT6336_RG_INT_RAW_STATUS_DD_BC12_STATUS,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_SW,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_GLOBAL,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_LONG_PRESS,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_WDT,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGOUT_PULSEB_RISING,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGOUT_PULSEB_LEVEL,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_PLUGIN_PULSEB,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_SHIP,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_BAT_OC,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_TOP_RST_BAT_DEAD,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BUCK_MODE,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_LOWQ_MODE,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_SHIP_MODE,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BAT_OC_MODE,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BAT_DEAD_MODE,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_SW_MODE,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_GLOBAL_MODE,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_WDT_MODE,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_RST_LONG_PRESS_MODE,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_CHR_SUSPEND_STATE,
	MT6336_RG_INT_RAW_STATUS_DD_SWCHR_BUCK_PROTECT_STATE,
	MT6336_FQMTR_TCKSEL,
	MT6336_FQMTR_BUSY,
	MT6336_FQMTR_EN,
	MT6336_FQMTR_WINSET_L,
	MT6336_FQMTR_WINSET_U,
	MT6336_FQMTR_DATA_L,
	MT6336_FQMTR_DATA_U,
	MT6336_TOP_RSV3,
	MT6336_TOP_RSV4,
	MT6336_TOP_RSV5,
	MT6336_TOP_RSV6,
	MT6336_TOP_RSV7,
	MT6336_TOP_RSV8,
	MT6336_TOP_RSV9,
	MT6336_TOP_RSV10,
	MT6336_TOP_RSV11,
	MT6336_TOP_RSV12,
	MT6336_TOP_RSV13,
	MT6336_TOP_RSV14,
	MT6336_TOP_RSV15,
	MT6336_RG_A_ICHRSTAT_TRIM_EN,
	MT6336_RG_A_ICHRSTAT_TRIM_SEL,
	MT6336_RG_A_ICHRSTAT_RSV,
	MT6336_CHRIND_DIM_FSEL,
	MT6336_CHRIND_DIM_FSEL_1,
	MT6336_CHRIND_DIM_DUTY,
	MT6336_CHRIND_STEP,
	MT6336_CHRIND_RSV1,
	MT6336_CHRIND_RSV0,
	MT6336_CHRIND_BREATH_TR2_SEL,
	MT6336_CHRIND_BREATH_TR1_SEL,
	MT6336_CHRIND_BREATH_TF2_SEL,
	MT6336_CHRIND_BREATH_TF1_SEL,
	MT6336_CHRIND_BREATH_TOFF_SEL,
	MT6336_CHRIND_BREATH_TON_SEL,
	MT6336_CHRIND_CHOP_EN,
	MT6336_CHRIND_MODE,
	MT6336_CHRIND_CHOP_SW,
	MT6336_CHRIND_BIAS_EN,
	MT6336_CHRIND_SFSTR_EN,
	MT6336_CHRIND_SFSTR_TC,
	MT6336_CHRIND_EN_SEL,
	MT6336_CHRIND_EN,
	MT6336_ISINK_RSV,
	MT6336_TYPE_C_PHY_RG_CC_RP_SEL,
	MT6336_REG_TYPE_C_PHY_RG_CC_V2I_BYPASS,
	MT6336_REG_TYPE_C_PHY_RG_CC_MPX_SEL,
	MT6336_REG_TYPE_C_PHY_RG_CC_RESERVE,
	MT6336_REG_TYPE_C_VCMP_CC2_SW_SEL_ST_CNT_VAL,
	MT6336_REG_TYPE_C_VCMP_BIAS_EN_ST_CNT_VAL,
	MT6336_REG_TYPE_C_VCMP_DAC_EN_ST_CNT_VAL,
	MT6336_REG_TYPE_C_PORT_SUPPORT_ROLE,
	MT6336_REG_TYPE_C_ADC_EN,
	MT6336_REG_TYPE_C_ACC_EN,
	MT6336_REG_TYPE_C_AUDIO_ACC_EN,
	MT6336_REG_TYPE_C_DEBUG_ACC_EN,
	MT6336_REG_TYPE_C_TRY_SRC_ST_EN,
	MT6336_REG_TYPE_C_ATTACH_SRC_2_TRY_WAIT_SNK_ST_EN,
	MT6336_REG_TYPE_C_PD2CC_DET_DISABLE_EN,
	MT6336_REG_TYPE_C_ATTACH_SRC_OPEN_PDDEBOUNCE_EN,
	MT6336_REG_TYPE_C_DISABLE_ST_RD_EN,
	MT6336_REG_TYPE_C_DA_CC_SACLK_SW_EN,
	MT6336_W1_TYPE_C_SW_ENT_DISABLE_CMD,
	MT6336_W1_TYPE_C_SW_ENT_UNATCH_SRC_CMD,
	MT6336_W1_TYPE_C_SW_ENT_UNATCH_SNK_CMD,
	MT6336_W1_TYPE_C_SW_PR_SWAP_INDICATE_CMD,
	MT6336_W1_TYPE_C_SW_VCONN_SWAP_INDICATE_CMD,
	MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_DEFAULT_CMD,
	MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_15_CMD,
	MT6336_W1_TYPE_C_SW_ADC_RESULT_MET_VRD_30_CMD,
	MT6336_TYPE_C_SW_VBUS_PRESENT,
	MT6336_TYPE_C_SW_DA_DRIVE_VCONN_EN,
	MT6336_TYPE_C_SW_VBUS_DET_DIS,
	MT6336_TYPE_C_SW_CC_DET_DIS,
	MT6336_TYPE_C_SW_PD_EN,
	MT6336_W1_TYPE_C_SW_ENT_SNK_PWR_REDETECT_CMD,
	MT6336_REG_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_0,
	MT6336_REG_TYPE_C_CC_VOL_PERIODIC_MEAS_VAL_1,
	MT6336_REG_TYPE_C_CC_VOL_CC_DEBOUNCE_CNT_VAL,
	MT6336_REG_TYPE_C_CC_VOL_PD_DEBOUNCE_CNT_VAL,
	MT6336_REG_TYPE_C_DRP_SRC_CNT_VAL_0_0,
	MT6336_REG_TYPE_C_DRP_SRC_CNT_VAL_0_1,
	MT6336_REG_TYPE_C_DRP_SNK_CNT_VAL_0_0,
	MT6336_REG_TYPE_C_DRP_SNK_CNT_VAL_0_1,
	MT6336_REG_TYPE_C_DRP_TRY_CNT_VAL_0_0,
	MT6336_REG_TYPE_C_DRP_TRY_CNT_VAL_0_1,
	MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_0,
	MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_0_1,
	MT6336_REG_TYPE_C_DRP_TRY_WAIT_CNT_VAL_1,
	MT6336_REG_TYPE_C_CC_SRC_VOPEN_DEFAULT_DAC_VAL,
	MT6336_REG_TYPE_C_CC_SRC_VRD_DEFAULT_DAC_VAL,
	MT6336_REG_TYPE_C_CC_SRC_VOPEN_15_DAC_VAL,
	MT6336_REG_TYPE_C_CC_SRC_VRD_15_DAC_VAL,
	MT6336_REG_TYPE_C_CC_SRC_VOPEN_30_DAC_VAL,
	MT6336_REG_TYPE_C_CC_SRC_VRD_30_DAC_VAL,
	MT6336_REG_TYPE_C_CC_SNK_VRP30_DAC_VAL,
	MT6336_REG_TYPE_C_CC_SNK_VRP15_DAC_VAL,
	MT6336_REG_TYPE_C_CC_SNK_VRPUSB_DAC_VAL,
	MT6336_REG_TYPE_C_CC_ENT_ATTACH_SRC_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_ATTACH_SNK_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_AUDIO_ACC_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_DBG_ACC_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_DISABLE_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_UNATTACH_SRC_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_UNATTACH_SNK_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_TRY_SRC_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_UNATTACH_ACC_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_15_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_30_INTR_EN,
	MT6336_REG_TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR_EN,
	MT6336_TYPE_C_CC_ENT_ATTACH_SRC_INTR,
	MT6336_TYPE_C_CC_ENT_ATTACH_SNK_INTR,
	MT6336_TYPE_C_CC_ENT_AUDIO_ACC_INTR,
	MT6336_TYPE_C_CC_ENT_DBG_ACC_INTR,
	MT6336_TYPE_C_CC_ENT_DISABLE_INTR,
	MT6336_TYPE_C_CC_ENT_UNATTACH_SRC_INTR,
	MT6336_TYPE_C_CC_ENT_UNATTACH_SNK_INTR,
	MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_SRC_INTR,
	MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_SNK_INTR,
	MT6336_TYPE_C_CC_ENT_TRY_SRC_INTR,
	MT6336_TYPE_C_CC_ENT_TRY_WAIT_SNK_INTR,
	MT6336_TYPE_C_CC_ENT_UNATTACH_ACC_INTR,
	MT6336_TYPE_C_CC_ENT_ATTACH_WAIT_ACC_INTR,
	MT6336_TYPE_C_CC_ENT_SNK_PWR_IDLE_INTR,
	MT6336_TYPE_C_CC_ENT_SNK_PWR_DEFAULT_INTR,
	MT6336_TYPE_C_CC_ENT_SNK_PWR_15_INTR,
	MT6336_TYPE_C_CC_ENT_SNK_PWR_30_INTR,
	MT6336_TYPE_C_CC_ENT_SNK_PWR_REDETECT_INTR,
	MT6336_RO_TYPE_C_CC_ST,
	MT6336_RO_TYPE_C_ROUTED_CC,
	MT6336_RO_TYPE_C_CC_SNK_PWR_ST,
	MT6336_RO_TYPE_C_CC_PWR_ROLE,
	MT6336_RO_TYPE_C_DRIVE_VCONN_CAPABLE,
	MT6336_RO_TYPE_C_AD_CC_CMP_OUT,
	MT6336_RO_AD_CC_VUSB33_RDY,
	MT6336_REG_TYPE_C_PHY_RG_CC1_RPDE,
	MT6336_REG_TYPE_C_PHY_RG_CC1_RP15,
	MT6336_REG_TYPE_C_PHY_RG_CC1_RP3,
	MT6336_REG_TYPE_C_PHY_RG_CC1_RD,
	MT6336_REG_TYPE_C_PHY_RG_CC2_RPDE,
	MT6336_REG_TYPE_C_PHY_RG_CC2_RP15,
	MT6336_REG_TYPE_C_PHY_RG_CC2_RP3,
	MT6336_REG_TYPE_C_PHY_RG_CC2_RD,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC1,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_RCC2,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LEV_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SW_SEL,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_BIAS_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_LPF_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_ADCSW_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SASW_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_SACLK,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_IN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_CAL,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_CC_DAC_GAIN_CAL,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC1_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC1_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC1_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RPCC2_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RDCC2_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_RACC2_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_LEV_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SW_SEL,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_BIAS_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_LPF_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_ADCSW_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SASW_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_SACLK,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_IN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_CAL,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_CC_DAC_GAIN_CAL,
	MT6336_TYPE_C_DAC_CAL_START,
	MT6336_REG_TYPE_C_DAC_CAL_STAGE,
	MT6336_RO_TYPE_C_DAC_CAL_OK,
	MT6336_RO_TYPE_C_DAC_CAL_FAIL,
	MT6336_RO_DA_CC_DAC_CAL,
	MT6336_RO_DA_CC_DAC_GAIN_CAL,
	MT6336_REG_TYPE_C_DBG_PORT_SEL_0_0,
	MT6336_REG_TYPE_C_DBG_PORT_SEL_0_1,
	MT6336_REG_TYPE_C_DBG_PORT_SEL_1_0,
	MT6336_REG_TYPE_C_DBG_PORT_SEL_1_1,
	MT6336_REG_TYPE_C_DBG_MOD_SEL_0,
	MT6336_REG_TYPE_C_DBG_MOD_SEL_1,
	MT6336_RO_TYPE_C_DBG_OUT_READ_0_0,
	MT6336_RO_TYPE_C_DBG_OUT_READ_0_1,
	MT6336_RO_TYPE_C_DBG_OUT_READ_1_0,
	MT6336_RO_TYPE_C_DBG_OUT_READ_1_1,
	MT6336_REG_TYPE_C_SW_DBG_PORT_0_0,
	MT6336_REG_TYPE_C_SW_DBG_PORT_0_1,
	MT6336_REG_TYPE_C_SW_DBG_PORT_1_0,
	MT6336_REG_TYPE_C_SW_DBG_PORT_1_1,
	MT6336_REG_PD_TX_HALF_UI_CYCLE_CNT,
	MT6336_REG_PD_TX_RETRY_CNT,
	MT6336_REG_PD_TX_AUTO_SEND_SR_EN,
	MT6336_REG_PD_TX_AUTO_SEND_HR_EN,
	MT6336_REG_PD_TX_AUTO_SEND_CR_EN,
	MT6336_REG_PD_TX_DATA_OBJ0_0_0,
	MT6336_REG_PD_TX_DATA_OBJ0_0_1,
	MT6336_REG_PD_TX_DATA_OBJ0_1_0,
	MT6336_REG_PD_TX_DATA_OBJ0_1_1,
	MT6336_REG_PD_TX_DATA_OBJ1_0_0,
	MT6336_REG_PD_TX_DATA_OBJ1_0_1,
	MT6336_REG_PD_TX_DATA_OBJ1_1_0,
	MT6336_REG_PD_TX_DATA_OBJ1_1_1,
	MT6336_REG_PD_TX_DATA_OBJ2_0_0,
	MT6336_REG_PD_TX_DATA_OBJ2_0_1,
	MT6336_REG_PD_TX_DATA_OBJ2_1_0,
	MT6336_REG_PD_TX_DATA_OBJ2_1_1,
	MT6336_REG_PD_TX_DATA_OBJ3_0_0,
	MT6336_REG_PD_TX_DATA_OBJ3_0_1,
	MT6336_REG_PD_TX_DATA_OBJ3_1_0,
	MT6336_REG_PD_TX_DATA_OBJ3_1_1,
	MT6336_REG_PD_TX_DATA_OBJ4_0_0,
	MT6336_REG_PD_TX_DATA_OBJ4_0_1,
	MT6336_REG_PD_TX_DATA_OBJ4_1_0,
	MT6336_REG_PD_TX_DATA_OBJ4_1_1,
	MT6336_REG_PD_TX_DATA_OBJ5_0_0,
	MT6336_REG_PD_TX_DATA_OBJ5_0_1,
	MT6336_REG_PD_TX_DATA_OBJ5_1_0,
	MT6336_REG_PD_TX_DATA_OBJ5_1_1,
	MT6336_REG_PD_TX_DATA_OBJ6_0_0,
	MT6336_REG_PD_TX_DATA_OBJ6_0_1,
	MT6336_REG_PD_TX_DATA_OBJ6_1_0,
	MT6336_REG_PD_TX_DATA_OBJ6_1_1,
	MT6336_REG_PD_TX_HDR_MSG_TYPE,
	MT6336_REG_PD_TX_HDR_PORT_DATA_ROLE,
	MT6336_REG_PD_TX_HDR_SPEC_VER,
	MT6336_REG_PD_TX_HDR_PORT_POWER_ROLE,
	MT6336_REG_PD_TX_HDR_NUM_DATA_OBJ,
	MT6336_REG_PD_TX_HDR_CABLE_PLUG,
	MT6336_PD_TX_START,
	MT6336_PD_TX_BIST_CARRIER_MODE2_START,
	MT6336_REG_PD_TX_OS,
	MT6336_REG_PD_RX_EN,
	MT6336_REG_PD_RX_SOP_PRIME_RCV_EN,
	MT6336_REG_PD_RX_SOP_DPRIME_RCV_EN,
	MT6336_REG_PD_RX_CABLE_RST_RCV_EN,
	MT6336_REG_PD_RX_PRL_SEND_GCRC_MSG_DIS_BUS_IDLE_OPT,
	MT6336_REG_PD_RX_PRE_PROTECT_EN,
	MT6336_REG_PD_RX_PRE_TRAIN_BIT_CNT,
	MT6336_REG_PD_RX_PING_MSG_RCV_EN,
	MT6336_REG_PD_RX_PRE_PROTECT_HALF_UI_CYCLE_CNT_MIN,
	MT6336_REG_PD_RX_PRE_PROTECT_UI_CYCLE_CNT_MAX_0,
	MT6336_REG_PD_RX_PRE_PROTECT_UI_CYCLE_CNT_MAX_1,
	MT6336_RO_PD_RX_OS,
	MT6336_RO_PD_RX_DATA_OBJ0_0_0,
	MT6336_RO_PD_RX_DATA_OBJ0_0_1,
	MT6336_RO_PD_RX_DATA_OBJ0_1_0,
	MT6336_RO_PD_RX_DATA_OBJ0_1_1,
	MT6336_RO_PD_RX_DATA_OBJ1_0_0,
	MT6336_RO_PD_RX_DATA_OBJ1_0_1,
	MT6336_RO_PD_RX_DATA_OBJ1_1_0,
	MT6336_RO_PD_RX_DATA_OBJ1_1_1,
	MT6336_RO_PD_RX_DATA_OBJ2_0_0,
	MT6336_RO_PD_RX_DATA_OBJ2_0_1,
	MT6336_RO_PD_RX_DATA_OBJ2_1_0,
	MT6336_RO_PD_RX_DATA_OBJ2_1_1,
	MT6336_RO_PD_RX_DATA_OBJ3_0_0,
	MT6336_RO_PD_RX_DATA_OBJ3_0_1,
	MT6336_RO_PD_RX_DATA_OBJ3_1_0,
	MT6336_RO_PD_RX_DATA_OBJ3_1_1,
	MT6336_RO_PD_RX_DATA_OBJ4_0_0,
	MT6336_RO_PD_RX_DATA_OBJ4_0_1,
	MT6336_RO_PD_RX_DATA_OBJ4_1_0,
	MT6336_RO_PD_RX_DATA_OBJ4_1_1,
	MT6336_RO_PD_RX_DATA_OBJ5_0_0,
	MT6336_RO_PD_RX_DATA_OBJ5_0_1,
	MT6336_RO_PD_RX_DATA_OBJ5_1_0,
	MT6336_RO_PD_RX_DATA_OBJ5_1_1,
	MT6336_RO_PD_RX_DATA_OBJ6_0_0,
	MT6336_RO_PD_RX_DATA_OBJ6_0_1,
	MT6336_RO_PD_RX_DATA_OBJ6_1_0,
	MT6336_RO_PD_RX_DATA_OBJ6_1_1,
	MT6336_RO_PD_RX_HDR_0,
	MT6336_RO_PD_RX_HDR_1,
	MT6336_RO_PD_PHY_RX_UI_CYC_CNT_TRAIN_1P25X_0,
	MT6336_RO_PD_PHY_RX_UI_CYC_CNT_TRAIN_1P25X_1,
	MT6336_PD_PRL_RCV_FIFO_SW_RST,
	MT6336_W1_PD_PE_HR_CPL,
	MT6336_W1_PD_PE_CR_CPL,
	MT6336_RO_PD_AD_PD_VCONN_UVP_STATUS,
	MT6336_RO_PD_AD_PD_CC1_OVP_STATUS,
	MT6336_RO_PD_AD_PD_CC2_OVP_STATUS,
	MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_0_0,
	MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_0_1,
	MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_1_0,
	MT6336_REG_PD_CRC_RCV_TIMEOUT_VAL_1_1,
	MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_0_0,
	MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_0_1,
	MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_1_0,
	MT6336_REG_PD_HR_CPL_TIMEOUT_VAL_1_1,
	MT6336_REG_PD_IDLE_DET_WIN_VAL_0,
	MT6336_REG_PD_IDLE_DET_WIN_VAL_1,
	MT6336_REG_PD_IDLE_DET_TRANS_CNT,
	MT6336_RO_PD_IDLE_DET_STATUS,
	MT6336_REG_PD_INTERFRAMEGAP_VAL_0,
	MT6336_REG_PD_INTERFRAMEGAP_VAL_1,
	MT6336_REG_PD_RX_GLITCH_MASK_WIN_VAL,
	MT6336_REG_PD_RX_UI_1P25X_ADJ_CNT,
	MT6336_REG_PD_RX_UI_1P25X_ADJ,
	MT6336_REG_PD_TIMER0_VAL_0_0,
	MT6336_REG_PD_TIMER0_VAL_0_1,
	MT6336_REG_PD_TIMER0_VAL_1_0,
	MT6336_REG_PD_TIMER0_VAL_1_1,
	MT6336_PD_TIMER0_EN,
	MT6336_REG_PD_TIMER1_VAL_0,
	MT6336_REG_PD_TIMER1_VAL_1,
	MT6336_RO_PD_TIMER1_TICK_CNT_0,
	MT6336_RO_PD_TIMER1_TICK_CNT_1,
	MT6336_PD_TIMER1_EN,
	MT6336_REG_PD_TX_SLEW_CALI_AUTO_EN,
	MT6336_REG_PD_TX_SLEW_CALI_SLEW_CK_SW_EN,
	MT6336_REG_PD_TX_SLEW_CALI_LOCK_TARGET_CNT,
	MT6336_REG_PD_TX_SLEW_CK_STABLE_TIME_0,
	MT6336_REG_PD_TX_SLEW_CK_STABLE_TIME_1,
	MT6336_REG_PD_TX_MON_CK_TARGET_CYC_CNT_0,
	MT6336_REG_PD_TX_MON_CK_TARGET_CYC_CNT_1,
	MT6336_REG_PD_TX_SLEW_CK_TARGET_CYC_CNT,
	MT6336_RO_PD_TX_SLEW_CALI_OK,
	MT6336_RO_PD_TX_SLEW_CALI_FAIL,
	MT6336_RO_PD_TXSLEW_I_CALI,
	MT6336_RO_PD_FM_OUT_0,
	MT6336_RO_PD_FM_OUT_1,
	MT6336_PD_LB_EN,
	MT6336_PD_LB_CHK_EN,
	MT6336_RO_PD_LB_RUN,
	MT6336_RO_PD_LB_OK,
	MT6336_RO_PD_LB_ERR_CNT,
	MT6336_REG_PD_SW_MSG_ID,
	MT6336_PD_SW_MSG_ID_SYNC,
	MT6336_REG_PD_TX_DONE_INTR_EN,
	MT6336_REG_PD_TX_RETRY_ERR_INTR_EN,
	MT6336_REG_PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR_EN,
	MT6336_REG_PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR_EN,
	MT6336_REG_PD_TX_DIS_BUS_REIDLE_INTR_EN,
	MT6336_REG_PD_TX_CRC_RCV_TIMEOUT_INTR_EN,
	MT6336_REG_PD_TX_AUTO_SR_DONE_INTR_EN,
	MT6336_REG_PD_TX_AUTO_SR_RETRY_ERR_INTR_EN,
	MT6336_REG_PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR_EN,
	MT6336_REG_PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR_EN,
	MT6336_REG_PD_RX_RCV_MSG_INTR_EN,
	MT6336_REG_PD_RX_LENGTH_MIS_INTR_EN,
	MT6336_REG_PD_RX_DUPLICATE_INTR_EN,
	MT6336_REG_PD_RX_TRANS_GCRC_FAIL_INTR_EN,
	MT6336_REG_PD_HR_TRANS_CPL_TIMEOUT_INTR_EN,
	MT6336_REG_PD_HR_TRANS_DONE_INTR_EN,
	MT6336_REG_PD_HR_RCV_DONE_INTR_EN,
	MT6336_REG_PD_HR_TRANS_FAIL_INTR_EN,
	MT6336_REG_PD_AD_PD_VCONN_UVP_INTR_EN,
	MT6336_REG_PD_AD_PD_CC1_OVP_INTR_EN,
	MT6336_REG_PD_AD_PD_CC2_OVP_INTR_EN,
	MT6336_REG_PD_TIMER0_TIMEOUT_INTR_EN,
	MT6336_REG_PD_TIMER1_TIMEOUT_INTR_EN,
	MT6336_PD_TX_DONE_INTR,
	MT6336_PD_TX_RETRY_ERR_INTR,
	MT6336_PD_TX_RCV_NEW_MSG_DISCARD_MSG_INTR,
	MT6336_PD_TX_PHY_LAYER_RST_DISCARD_MSG_INTR,
	MT6336_PD_TX_DIS_BUS_REIDLE_INTR,
	MT6336_PD_TX_CRC_RCV_TIMEOUT_INTR,
	MT6336_PD_TX_AUTO_SR_DONE_INTR,
	MT6336_PD_TX_AUTO_SR_RETRY_ERR_INTR,
	MT6336_PD_TX_AUTO_SR_RCV_NEW_MSG_DISCARD_MSG_INTR,
	MT6336_PD_TX_AUTO_SR_PHY_LAYER_RST_DISCARD_MSG_INTR,
	MT6336_PD_RX_RCV_MSG_INTR,
	MT6336_PD_RX_LENGTH_MIS_INTR,
	MT6336_PD_RX_DUPLICATE_INTR,
	MT6336_PD_RX_TRANS_GCRC_FAIL_INTR,
	MT6336_PD_HR_TRANS_CPL_TIMEOUT_INTR,
	MT6336_PD_HR_TRANS_DONE_INTR,
	MT6336_PD_HR_RCV_DONE_INTR,
	MT6336_PD_HR_TRANS_FAIL_INTR,
	MT6336_PD_AD_PD_VCONN_UVP_INTR,
	MT6336_PD_AD_PD_CC1_OVP_INTR,
	MT6336_PD_AD_PD_CC2_OVP_INTR,
	MT6336_PD_TIMER0_TIMEOUT_INTR,
	MT6336_PD_TIMER1_TIMEOUT_INTR,
	MT6336_REG_TYPE_C_PHY_RG_PD_TXSLEW_CALEN,
	MT6336_REG_TYPE_C_PHY_RG_PD_RXLPF_2ND_EN,
	MT6336_REG_TYPE_C_PHY_RG_PD_RX_MODE,
	MT6336_REG_TYPE_C_PHY_RG_PD_UVP_VTH,
	MT6336_REG_TYPE_C_PHY_RG_PD_UVP_EN,
	MT6336_REG_TYPE_C_PHY_RG_PD_UVP_SEL,
	MT6336_REG_TYPE_C_PHY_RG_PD_TXSLEW_I,
	MT6336_REG_TYPE_C_PHY_RG_PD_TX_AMP,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_BIAS_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_TX_DATA,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_RX_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CCSW,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_EN_DA_PD_CONNSW,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_BIAS_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_TX_DATA,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_RX_EN,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_CCSW,
	MT6336_REG_TYPE_C_SW_FORCE_MODE_DA_PD_CONNSW,
	MT6336_AUXADC_ADC_OUT_CH0_L,
	MT6336_AUXADC_ADC_OUT_CH0_H,
	MT6336_AUXADC_ADC_RDY_CH0,
	MT6336_AUXADC_ADC_OUT_CH1_L,
	MT6336_AUXADC_ADC_OUT_CH1_H,
	MT6336_AUXADC_ADC_RDY_CH1,
	MT6336_AUXADC_ADC_OUT_CH2_L,
	MT6336_AUXADC_ADC_OUT_CH2_H,
	MT6336_AUXADC_ADC_RDY_CH2,
	MT6336_AUXADC_ADC_OUT_CH3_L,
	MT6336_AUXADC_ADC_OUT_CH3_H,
	MT6336_AUXADC_ADC_RDY_CH3,
	MT6336_AUXADC_ADC_OUT_CH4_L,
	MT6336_AUXADC_ADC_OUT_CH4_H,
	MT6336_AUXADC_ADC_RDY_CH4,
	MT6336_AUXADC_ADC_OUT_CH5_L,
	MT6336_AUXADC_ADC_OUT_CH5_H,
	MT6336_AUXADC_ADC_RDY_CH5,
	MT6336_AUXADC_ADC_OUT_CH6_L,
	MT6336_AUXADC_ADC_OUT_CH6_H,
	MT6336_AUXADC_ADC_RDY_CH6,
	MT6336_AUXADC_ADC_OUT_CH7_L,
	MT6336_AUXADC_ADC_OUT_CH7_H,
	MT6336_AUXADC_ADC_RDY_CH7,
	MT6336_AUXADC_ADC_OUT_CH8_L,
	MT6336_AUXADC_ADC_OUT_CH8_H,
	MT6336_AUXADC_ADC_RDY_CH8,
	MT6336_AUXADC_ADC_OUT_CH9_L,
	MT6336_AUXADC_ADC_OUT_CH9_H,
	MT6336_AUXADC_ADC_RDY_CH9,
	MT6336_AUXADC_ADC_OUT_CH10_L,
	MT6336_AUXADC_ADC_OUT_CH10_H,
	MT6336_AUXADC_ADC_RDY_CH10,
	MT6336_AUXADC_ADC_OUT_CH11_L,
	MT6336_AUXADC_ADC_OUT_CH11_H,
	MT6336_AUXADC_ADC_RDY_CH11,
	MT6336_AUXADC_ADC_OUT_CH12_15_L,
	MT6336_AUXADC_ADC_OUT_CH12_15_H,
	MT6336_AUXADC_ADC_RDY_CH12_15,
	MT6336_AUXADC_ADC_OUT_THR_HW_L,
	MT6336_AUXADC_ADC_OUT_THR_HW_H,
	MT6336_AUXADC_ADC_RDY_THR_HW,
	MT6336_AUXADC_ADC_OUT_VBUS_SOFT_OVP_L,
	MT6336_AUXADC_ADC_OUT_VBUS_SOFT_OVP_H,
	MT6336_AUXADC_ADC_RDY_VBUS_SOFT_OVP,
	MT6336_AUXADC_ADC_OUT_TYPEC_H_L,
	MT6336_AUXADC_ADC_OUT_TYPEC_H_H,
	MT6336_AUXADC_ADC_RDY_TYPEC_H,
	MT6336_AUXADC_ADC_OUT_TYPEC_L_L,
	MT6336_AUXADC_ADC_OUT_TYPEC_L_H,
	MT6336_AUXADC_ADC_RDY_TYPEC_L,
	MT6336_AUXADC_ADC_OUT_VBATSNS_DET_L,
	MT6336_AUXADC_ADC_OUT_VBATSNS_DET_H,
	MT6336_AUXADC_ADC_RDY_VBATSNS_DET,
	MT6336_AUXADC_ADC_OUT_BAT_TEMP_L,
	MT6336_AUXADC_ADC_OUT_BAT_TEMP_H,
	MT6336_AUXADC_ADC_RDY_BAT_TEMP,
	MT6336_AUXADC_ADC_OUT_FGADC1_L,
	MT6336_AUXADC_ADC_OUT_FGADC1_H,
	MT6336_AUXADC_ADC_RDY_FGADC1,
	MT6336_AUXADC_ADC_OUT_FGADC2_L,
	MT6336_AUXADC_ADC_OUT_FGADC2_H,
	MT6336_AUXADC_ADC_RDY_FGADC2,
	MT6336_AUXADC_ADC_OUT_IMP_L,
	MT6336_AUXADC_ADC_OUT_IMP_H,
	MT6336_AUXADC_ADC_RDY_IMP,
	MT6336_AUXADC_ADC_OUT_IMP_AVG_L,
	MT6336_AUXADC_ADC_OUT_IMP_AVG_H,
	MT6336_AUXADC_ADC_RDY_IMP_AVG,
	MT6336_AUXADC_ADC_OUT_RAW_L,
	MT6336_AUXADC_ADC_OUT_RAW_H,
	MT6336_AUXADC_ADC_OUT_JEITA_L,
	MT6336_AUXADC_ADC_OUT_JEITA_H,
	MT6336_AUXADC_ADC_RDY_JEITA,
	MT6336_AUXADC_ADC_OUT_CHRGO1_L,
	MT6336_AUXADC_ADC_OUT_CHRGO1_H,
	MT6336_AUXADC_ADC_RDY_CHRGO1,
	MT6336_AUXADC_ADC_OUT_CHRGO2_L,
	MT6336_AUXADC_ADC_OUT_CHRGO2_H,
	MT6336_AUXADC_ADC_RDY_CHRGO2,
	MT6336_AUXADC_ADC_OUT_WAKEUP1_L,
	MT6336_AUXADC_ADC_OUT_WAKEUP1_H,
	MT6336_AUXADC_ADC_RDY_WAKEUP1,
	MT6336_AUXADC_ADC_OUT_WAKEUP2_L,
	MT6336_AUXADC_ADC_OUT_WAKEUP2_H,
	MT6336_AUXADC_ADC_RDY_WAKEUP2,
	MT6336_AUXADC_ADC_BUSY_IN_L,
	MT6336_AUXADC_ADC_BUSY_IN_H,
	MT6336_AUXADC_ADC_BUSY_IN_VBUS_SOFT_OVP,
	MT6336_AUXADC_ADC_BUSY_IN_BAT_TEMP,
	MT6336_AUXADC_ADC_BUSY_IN_WAKEUP,
	MT6336_AUXADC_ADC_BUSY_IN_JEITA,
	MT6336_AUXADC_ADC_BUSY_IN_CHRGO1,
	MT6336_AUXADC_ADC_BUSY_IN_CHRGO2,
	MT6336_AUXADC_ADC_BUSY_IN_SHARE,
	MT6336_AUXADC_ADC_BUSY_IN_IMP,
	MT6336_AUXADC_ADC_BUSY_IN_FGADC1,
	MT6336_AUXADC_ADC_BUSY_IN_FGADC2,
	MT6336_AUXADC_ADC_BUSY_IN_GPS_AP,
	MT6336_AUXADC_ADC_BUSY_IN_GPS_MD,
	MT6336_AUXADC_ADC_BUSY_IN_GPS,
	MT6336_AUXADC_ADC_BUSY_IN_THR_HW,
	MT6336_AUXADC_ADC_BUSY_IN_THR_MD,
	MT6336_AUXADC_ADC_BUSY_IN_NAG,
	MT6336_AUXADC_RQST_CH0,
	MT6336_AUXADC_RQST_CH1,
	MT6336_AUXADC_RQST_CH2,
	MT6336_AUXADC_RQST_CH3,
	MT6336_AUXADC_RQST_CH4,
	MT6336_AUXADC_RQST_CH5,
	MT6336_AUXADC_RQST_CH6,
	MT6336_AUXADC_RQST_CH7,
	MT6336_AUXADC_RQST0_SET,
	MT6336_AUXADC_RQST0_CLR,
	MT6336_AUXADC_RQST_CH8,
	MT6336_AUXADC_RQST_CH9,
	MT6336_AUXADC_RQST_CH10,
	MT6336_AUXADC_RQST_CH11,
	MT6336_AUXADC_RQST_CH12,
	MT6336_AUXADC_RQST_CH13,
	MT6336_AUXADC_RQST_CH14,
	MT6336_AUXADC_RQST_CH15,
	MT6336_AUXADC_RQST0_H_SET,
	MT6336_AUXADC_RQST0_H_CLR,
	MT6336_AUXADC_CK_ON_EXTD,
	MT6336_AUXADC_SRCLKEN_SRC_SEL,
	MT6336_AUXADC_CON0_SET,
	MT6336_AUXADC_CON0_CLR,
	MT6336_AUXADC_ADC_PWDB,
	MT6336_AUXADC_ADC_PWDB_SWCTRL,
	MT6336_AUXADC_STRUP_CK_ON_ENB,
	MT6336_AUXADC_SRCLKEN_CK_EN,
	MT6336_AUXADC_CK_AON_GPS,
	MT6336_AUXADC_CK_AON_MD,
	MT6336_AUXADC_CK_AON,
	MT6336_AUXADC_CON0_H_SET,
	MT6336_AUXADC_CON0_H_CLR,
	MT6336_AUXADC_AVG_NUM_SMALL,
	MT6336_AUXADC_AVG_NUM_LARGE,
	MT6336_AUXADC_SPL_NUM_L,
	MT6336_AUXADC_SPL_NUM_H,
	MT6336_AUXADC_AVG_NUM_SEL_L,
	MT6336_AUXADC_AVG_NUM_SEL_H,
	MT6336_AUXADC_AVG_NUM_SEL_SHARE,
	MT6336_AUXADC_AVG_NUM_SEL_VBUS_SOFT_OVP,
	MT6336_AUXADC_AVG_NUM_SEL_BAT_TEMP,
	MT6336_AUXADC_AVG_NUM_SEL_WAKEUP,
	MT6336_AUXADC_SPL_NUM_LARGE_L,
	MT6336_AUXADC_SPL_NUM_LARGE_H,
	MT6336_AUXADC_SPL_NUM_SLEEP_L,
	MT6336_AUXADC_SPL_NUM_SLEEP_H,
	MT6336_AUXADC_SPL_NUM_SLEEP_SEL,
	MT6336_AUXADC_SPL_NUM_SEL_L,
	MT6336_AUXADC_SPL_NUM_SEL_H,
	MT6336_AUXADC_SPL_NUM_SEL_SHARE,
	MT6336_AUXADC_SPL_NUM_SEL_VBUS_SOFT_OVP,
	MT6336_AUXADC_SPL_NUM_SEL_BAT_TEMP,
	MT6336_AUXADC_SPL_NUM_SEL_WAKEUP,
	MT6336_AUXADC_TRIM_CH0_SEL,
	MT6336_AUXADC_TRIM_CH1_SEL,
	MT6336_AUXADC_TRIM_CH2_SEL,
	MT6336_AUXADC_TRIM_CH3_SEL,
	MT6336_AUXADC_TRIM_CH4_SEL,
	MT6336_AUXADC_TRIM_CH5_SEL,
	MT6336_AUXADC_TRIM_CH6_SEL,
	MT6336_AUXADC_TRIM_CH7_SEL,
	MT6336_AUXADC_TRIM_CH8_SEL,
	MT6336_AUXADC_TRIM_CH9_SEL,
	MT6336_AUXADC_TRIM_CH10_SEL,
	MT6336_AUXADC_TRIM_CH11_SEL,
	MT6336_AUXADC_ADC_2S_COMP_ENB,
	MT6336_AUXADC_ADC_TRIM_COMP,
	MT6336_AUXADC_SW_GAIN_TRIM_L,
	MT6336_AUXADC_SW_GAIN_TRIM_H,
	MT6336_AUXADC_SW_OFFSET_TRIM_L,
	MT6336_AUXADC_SW_OFFSET_TRIM_H,
	MT6336_AUXADC_RNG_EN,
	MT6336_AUXADC_DATA_REUSE_SEL,
	MT6336_AUXADC_TEST_MODE,
	MT6336_AUXADC_BIT_SEL,
	MT6336_AUXADC_START_SW,
	MT6336_AUXADC_START_SWCTRL,
	MT6336_AUXADC_TS_VBE_SEL,
	MT6336_AUXADC_TS_VBE_SEL_SWCTRL,
	MT6336_AUXADC_VBUF_EN,
	MT6336_AUXADC_VBUF_EN_SWCTRL,
	MT6336_AUXADC_OUT_SEL,
	MT6336_AUXADC_DA_DAC_L,
	MT6336_AUXADC_DA_DAC_H,
	MT6336_AUXADC_DA_DAC_SWCTRL,
	MT6336_AD_AUXADC_COMP,
	MT6336_RG_VBUF_EXTEN,
	MT6336_RG_VBUF_CALEN,
	MT6336_RG_VBUF_BYP,
	MT6336_RG_VBUF_EN,
	MT6336_RG_AUX_RSV,
	MT6336_RG_AUXADC_CALI,
	MT6336_AUXADC_ADCIN_VBATSNS_EN,
	MT6336_AUXADC_ADCIN_VBUS_EN,
	MT6336_AUXADC_ADCIN_VBATON_EN,
	MT6336_AUXADC_ADCIN_VLED1_EN,
	MT6336_AUXADC_ADCIN_VLED2_EN,
	MT6336_AUXADC_DIG0_RSV0_L,
	MT6336_AUXADC_DIG0_RSV0_H,
	MT6336_AUXADC_CHSEL,
	MT6336_AUXADC_SWCTRL_EN,
	MT6336_AUXADC_SOURCE_VBUS_SOFT_OVP_SEL,
	MT6336_AUXADC_SOURCE_VBUS_SOFT_OVP2_SEL,
	MT6336_AUXADC_DIG0_RSV2,
	MT6336_AUXADC_DIG1_RSV2_L,
	MT6336_AUXADC_DIG1_RSV2_H,
	MT6336_AUXADC_DAC_EXTD,
	MT6336_AUXADC_DAC_EXTD_EN,
	MT6336_AUXADC_PMU_THR_PDN_SW,
	MT6336_AUXADC_PMU_THR_PDN_SEL,
	MT6336_AUXADC_PMU_THR_PDN_STATUS,
	MT6336_AUXADC_DIG0_RSV1,
	MT6336_AUXADC_START_SHADE_NUM_L,
	MT6336_AUXADC_START_SHADE_NUM_H,
	MT6336_AUXADC_START_SHADE_EN,
	MT6336_AUXADC_START_SHADE_SEL,
	MT6336_AUXADC_ADC_RDY_WAKEUP_CLR,
	MT6336_AUXADC_ADC_RDY_FGADC_CLR,
	MT6336_AUXADC_ADC_RDY_CHRGO_CLR,
	MT6336_AUXADC_START_EXTD,
	MT6336_DA_TS_VBE_SEL,
	MT6336_AUXADC_AUTORPT_PRD_L,
	MT6336_AUXADC_AUTORPT_PRD_H,
	MT6336_AUXADC_AUTORPT_EN,
	MT6336_AUXADC_VBUS_SOFT_OVP_DEBT_MAX,
	MT6336_AUXADC_VBUS_SOFT_OVP_DEBT_MIN,
	MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_15_0_L,
	MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_15_0_H,
	MT6336_AUXADC_VBUS_SOFT_OVP_DET_PRD_19_16,
	MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MAX_L,
	MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MAX_H,
	MT6336_AUXADC_VBUS_SOFT_OVP_IRQ_EN_MAX,
	MT6336_AUXADC_VBUS_SOFT_OVP_EN_MAX,
	MT6336_AUXADC_VBUS_SOFT_OVP_MAX_IRQ_B,
	MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MIN_L,
	MT6336_AUXADC_VBUS_SOFT_OVP_VOLT_MIN_H,
	MT6336_AUXADC_VBUS_SOFT_OVP_IRQ_EN_MIN,
	MT6336_AUXADC_VBUS_SOFT_OVP_EN_MIN,
	MT6336_AUXADC_VBUS_SOFT_OVP_MIN_IRQ_B,
	MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MAX_L,
	MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MAX_H,
	MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MIN_L,
	MT6336_AUXADC_VBUS_SOFT_OVP_DEBOUNCE_COUNT_MIN_H,
	MT6336_AUXADC_TYPEC_H_DEBT_MAX,
	MT6336_AUXADC_TYPEC_H_DEBT_MIN,
	MT6336_AUXADC_TYPEC_H_DET_PRD_15_0_L,
	MT6336_AUXADC_TYPEC_H_DET_PRD_15_0_H,
	MT6336_AUXADC_TYPEC_H_DET_PRD_19_16,
	MT6336_AUXADC_TYPEC_H_VOLT_MAX_L,
	MT6336_AUXADC_TYPEC_H_VOLT_MAX_H,
	MT6336_AUXADC_TYPEC_H_IRQ_EN_MAX,
	MT6336_AUXADC_TYPEC_H_EN_MAX,
	MT6336_AUXADC_TYPEC_H_MAX_IRQ_B,
	MT6336_AUXADC_TYPEC_H_VOLT_MIN_L,
	MT6336_AUXADC_TYPEC_H_VOLT_MIN_H,
	MT6336_AUXADC_TYPEC_H_IRQ_EN_MIN,
	MT6336_AUXADC_TYPEC_H_EN_MIN,
	MT6336_AUXADC_TYPEC_H_MIN_IRQ_B,
	MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MAX_L,
	MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MAX_H,
	MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MIN_L,
	MT6336_AUXADC_TYPEC_H_DEBOUNCE_COUNT_MIN_H,
	MT6336_AUXADC_TYPEC_L_DEBT_MAX,
	MT6336_AUXADC_TYPEC_L_DEBT_MIN,
	MT6336_AUXADC_TYPEC_L_DET_PRD_15_0_L,
	MT6336_AUXADC_TYPEC_L_DET_PRD_15_0_H,
	MT6336_AUXADC_TYPEC_L_DET_PRD_19_16,
	MT6336_AUXADC_TYPEC_L_VOLT_MAX_L,
	MT6336_AUXADC_TYPEC_L_VOLT_MAX_H,
	MT6336_AUXADC_TYPEC_L_IRQ_EN_MAX,
	MT6336_AUXADC_TYPEC_L_EN_MAX,
	MT6336_AUXADC_TYPEC_L_MAX_IRQ_B,
	MT6336_AUXADC_TYPEC_L_VOLT_MIN_L,
	MT6336_AUXADC_TYPEC_L_VOLT_MIN_H,
	MT6336_AUXADC_TYPEC_L_IRQ_EN_MIN,
	MT6336_AUXADC_TYPEC_L_EN_MIN,
	MT6336_AUXADC_TYPEC_L_MIN_IRQ_B,
	MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MAX_L,
	MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MAX_H,
	MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MIN_L,
	MT6336_AUXADC_TYPEC_L_DEBOUNCE_COUNT_MIN_H,
	MT6336_AUXADC_THR_DEBT_MAX,
	MT6336_AUXADC_THR_DEBT_MIN,
	MT6336_AUXADC_THR_DET_PRD_15_0_L,
	MT6336_AUXADC_THR_DET_PRD_15_0_H,
	MT6336_AUXADC_THR_DET_PRD_19_16,
	MT6336_AUXADC_THR_VOLT_MAX_L,
	MT6336_AUXADC_THR_VOLT_MAX_H,
	MT6336_AUXADC_THR_IRQ_EN_MAX,
	MT6336_AUXADC_THR_EN_MAX,
	MT6336_AUXADC_THR_MAX_IRQ_B,
	MT6336_AUXADC_THR_VOLT_MIN_L,
	MT6336_AUXADC_THR_VOLT_MIN_H,
	MT6336_AUXADC_THR_IRQ_EN_MIN,
	MT6336_AUXADC_THR_EN_MIN,
	MT6336_AUXADC_THR_MIN_IRQ_B,
	MT6336_AUXADC_THR_DEBOUNCE_COUNT_MAX_L,
	MT6336_AUXADC_THR_DEBOUNCE_COUNT_MAX_H,
	MT6336_AUXADC_THR_DEBOUNCE_COUNT_MIN_L,
	MT6336_AUXADC_THR_DEBOUNCE_COUNT_MIN_H,
	MT6336_EFUSE_GAIN_CH4_TRIM_L,
	MT6336_EFUSE_GAIN_CH4_TRIM_H,
	MT6336_EFUSE_OFFSET_CH4_TRIM_L,
	MT6336_EFUSE_OFFSET_CH4_TRIM_H,
	MT6336_EFUSE_GAIN_CH0_TRIM_L,
	MT6336_EFUSE_GAIN_CH0_TRIM_H,
	MT6336_EFUSE_OFFSET_CH0_TRIM_L,
	MT6336_EFUSE_OFFSET_CH0_TRIM_H,
	MT6336_EFUSE_GAIN_CH7_TRIM_L,
	MT6336_EFUSE_GAIN_CH7_TRIM_H,
	MT6336_EFUSE_OFFSET_CH7_TRIM_L,
	MT6336_EFUSE_OFFSET_CH7_TRIM_H,
	MT6336_AUXADC_FGADC_START_SW,
	MT6336_AUXADC_FGADC_START_SEL,
	MT6336_AUXADC_FGADC_R_SW,
	MT6336_AUXADC_FGADC_R_SEL,
	MT6336_AUXADC_CHRGO_START_SW,
	MT6336_AUXADC_CHRGO_START_SEL,
	MT6336_AUXADC_DBG_DIG0_RSV2_L,
	MT6336_AUXADC_DBG_DIG0_RSV2_H,
	MT6336_AUXADC_DBG_DIG1_RSV2,
	MT6336_AUXADC_IMPEDANCE_CNT,
	MT6336_AUXADC_IMPEDANCE_CHSEL,
	MT6336_AUXADC_IMPEDANCE_IRQ_CLR,
	MT6336_AUXADC_IMPEDANCE_IRQ_STATUS,
	MT6336_AUXADC_CLR_IMP_CNT_STOP,
	MT6336_AUXADC_IMPEDANCE_MODE,
	MT6336_AUXADC_IMP_AUTORPT_PRD_L,
	MT6336_AUXADC_IMP_AUTORPT_PRD_H,
	MT6336_AUXADC_IMP_AUTORPT_EN,
	MT6336_AUXADC_BAT_TEMP_FROZE_EN,
	MT6336_AUXADC_BAT_TEMP_DEBT_MAX,
	MT6336_AUXADC_BAT_TEMP_DEBT_MIN,
	MT6336_AUXADC_BAT_TEMP_DET_PRD_15_0_L,
	MT6336_AUXADC_BAT_TEMP_DET_PRD_15_0_H,
	MT6336_AUXADC_BAT_TEMP_DET_PRD_19_16,
	MT6336_AUXADC_BAT_TEMP_VOLT_MAX_L,
	MT6336_AUXADC_BAT_TEMP_VOLT_MAX_H,
	MT6336_AUXADC_BAT_TEMP_IRQ_EN_MAX,
	MT6336_AUXADC_BAT_TEMP_EN_MAX,
	MT6336_AUXADC_BAT_TEMP_MAX_IRQ_B,
	MT6336_AUXADC_BAT_TEMP_VOLT_MIN_L,
	MT6336_AUXADC_BAT_TEMP_VOLT_MIN_H,
	MT6336_AUXADC_BAT_TEMP_IRQ_EN_MIN,
	MT6336_AUXADC_BAT_TEMP_EN_MIN,
	MT6336_AUXADC_BAT_TEMP_MIN_IRQ_B,
	MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MAX_L,
	MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MAX_H,
	MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MIN_L,
	MT6336_AUXADC_BAT_TEMP_DEBOUNCE_COUNT_MIN_H,
	MT6336_RG_EN_BUCK,
	MT6336_RG_EN_CHARGE,
	MT6336_RG_EN_OTGPIN,
	MT6336_RG_EN_OTG,
	MT6336_RG_EN_FLASH,
	MT6336_RG_EN_TORCH,
	MT6336_RG_EN_FLAASTOR,
	MT6336_RG_ON_VSYS_IDIS,
	MT6336_RG_EN_VSYS_IDIS,
	MT6336_RG_T_VSYS_IDIS,
	MT6336_RG_EN_RECHARGE,
	MT6336_RG_EN_TERM,
	MT6336_RG_T_TERM_EXT,
	MT6336_RG_DIS_REVFET,
	MT6336_RG_TSTEP_VSYSREG,
	MT6336_RG_T_BSTCHK_SEL,
	MT6336_RG_FORCE_LOWQ_MODE,
	MT6336_RG_DIS_BATOC_MODE,
	MT6336_RG_EN_CLKSQ_GLOBAL,
	MT6336_RG_EN_CLKSQ_PDTYPEC,
	MT6336_RG_EN_VBUS_IDIS,
	MT6336_RG_T_VBUS_IDIS,
	MT6336_RG_ON_VBUS_IDIS,
	MT6336_RG_MAIN_RSV0,
	MT6336_RG_CK_T_HANDSHAKE_SEL,
	MT6336_RG_SWCHR_RST_SW,
	MT6336_RG_SWCHR_RST_GLOBAL,
	MT6336_RG_DIS_LOWQ_MODE,
	MT6336_RG_DIS_SHIP_MODE,
	MT6336_RG_DIS_PWR_PROTECTION,
	MT6336_RG_DIS_BAT_OC_MODE,
	MT6336_RG_DIS_BAT_DEAD_MODE,
	MT6336_RG_PWRON_1_RSV,
	MT6336_RG_PWRON_0_RSV,
	MT6336_RG_BUCK_1_RSV,
	MT6336_RG_BUCK_0_RSV,
	MT6336_RG_TOLP_ON,
	MT6336_RG_TOLP_OFF,
	MT6336_RG_OTG_ON_MODE_SEL,
	MT6336_RG_OTG_IOLP,
	MT6336_RG_OTG_VCV,
	MT6336_RG_EN_STROBEPIN,
	MT6336_RG_EN_TXMASKPIN,
	MT6336_RG_EN_IFLA1,
	MT6336_RG_EN_IFLA2,
	MT6336_RG_FLA_WDT,
	MT6336_RG_EN_FLA_WDT,
	MT6336_RG_EN_LEDCS,
	MT6336_RG_IFLA1,
	MT6336_RG_IFLA2,
	MT6336_RG_TSTEP_ILED,
	MT6336_RG_VFLA,
	MT6336_RG_ITOR1,
	MT6336_RG_ITOR2,
	MT6336_RG_EN_ITOR1,
	MT6336_RG_EN_ITOR2,
	MT6336_RG_EN_TOR_WDT,
	MT6336_RG_OS_RESTART,
	MT6336_RG_FLA_BYTOROS_MODE,
	MT6336_RG_TOR_DRV_MODE0,
	MT6336_RG_TOR_DRV_MODE1,
	MT6336_RG_TOR_SS_MODE,
	MT6336_RG_SSCTRL_SEL,
	MT6336_RGS_FLA1_OS_TIMEOUT,
	MT6336_RGS_FLA2_OS_TIMEOUT,
	MT6336_RGS_TOR1_OS_TIMEOUT,
	MT6336_RGS_TOR2_OS_TIMEOUT,
	MT6336_RG_BOOST_RSV0,
	MT6336_RG_BOOST_RSV1,
	MT6336_RG_PEP_DATA,
	MT6336_RG_PE_SEL,
	MT6336_RG_PE_BIT_SEL,
	MT6336_RG_PE_TC_SEL,
	MT6336_RG_PE_ENABLE,
	MT6336_RGS_PE_STATUS,
	MT6336_RG_ICC_JW,
	MT6336_RG_ICC_JC,
	MT6336_RG_DISWARMCOOL,
	MT6336_RG_TSTEP_ICC,
	MT6336_RG_EN_ICC_SFSTR,
	MT6336_RG_EN_HWJEITA,
	MT6336_RG_T_MASKTMR,
	MT6336_RG_RST_MASKTMR,
	MT6336_RG_EN_MASKTMR,
	MT6336_RG_EN_MASKJEITA,
	MT6336_RG_ICC,
	MT6336_RG_VCV,
	MT6336_RG_VCV_JW,
	MT6336_RG_VCV_JC,
	MT6336_RG_IPRECC1_JC,
	MT6336_RG_IPRECC1_JW,
	MT6336_RG_IPRECC1,
	MT6336_RG_BC12_RST,
	MT6336_RG_BC12_EN,
	MT6336_RG_BC12_TIMER_EN,
	MT6336_RG_BC12_CLEAR,
	MT6336_RG_ITERM,
	MT6336_RG_R_IRCOMP,
	MT6336_RG_THR_TTH,
	MT6336_RG_VPAM,
	MT6336_RG_PLUG_RSTB_SEL,
	MT6336_RG_VSYSREG,
	MT6336_RG_VRECHG,
	MT6336_RG_ON_VBAT_IDIS,
	MT6336_RG_EN_VBAT_IDIS,
	MT6336_RG_VBATFETON,
	MT6336_RG_EN_VSYSREG_SFSTR,
	MT6336_RG_EN_VSYSREG_POST_SFSTR,
	MT6336_RG_DIS_PP_EN_PIN,
	MT6336_RG_T_LONGPRESS_SEL,
	MT6336_RG_FLAG_MODE,
	MT6336_DD_FLAG_OUT,
	MT6336_RG_T_SHIP_DLY_SEL,
	MT6336_RG_EN_SHIP,
	MT6336_RG_EN_WDT,
	MT6336_RG_WDT,
	MT6336_RG_WR_TRI,
	MT6336_RG_DIS_WDT_SUSPEND,
	MT6336_RG_OTG_TOLP_ON,
	MT6336_RG_TDEG_ITERM,
	MT6336_RG_ICL,
	MT6336_RG_TSTEP_ICL,
	MT6336_RG_EN_ICL150PIN,
	MT6336_RG_TSTEP_AICC,
	MT6336_RG_EN_AICC,
	MT6336_RG_TSTEP_THR,
	MT6336_RG_EN_DIG_THR_GM,
	MT6336_RG_EN_IBACKBOOST,
	MT6336_RG_FREQ_SEL,
	MT6336_RG_SSTIME_SEL,
	MT6336_RG_EN_SSBYPASS,
	MT6336_RG_EN_CHR_SAFETMR,
	MT6336_RG_EN_CHR_SAFETMRX2,
	MT6336_RG_PAUSE_CHR_SAFETMR,
	MT6336_RG_CHR_SAFETMR_PRECC1,
	MT6336_RG_CHR_SAFETMR_FASTCC,
	MT6336_RG_CHR_SAFETMR_CLEAR,
	MT6336_RG_CK_SFSTR_DIV_SEL,
	MT6336_RG_OTP_PA,
	MT6336_RG_OTP_PDIN,
	MT6336_RG_OTP_PTM,
	MT6336_RG_OTP_PWE,
	MT6336_RG_OTP_PPROG,
	MT6336_RG_OTP_PWE_SRC,
	MT6336_RG_OTP_PROG_PKEY,
	MT6336_RG_OTP_RD_PKEY,
	MT6336_RG_OTP_RD_TRIG,
	MT6336_RG_RD_RDY_BYPASS,
	MT6336_RG_SKIP_OTP_OUT,
	MT6336_RG_OTP_RD_SW,
	MT6336_RG_OTP_DOUT_SW,
	MT6336_RG_OTP_RD_BUSY,
	MT6336_RG_OTP_RD_ACK,
	MT6336_RG_OTP_PA_SW,
	MT6336_RG_OTP_DOUT_0_7,
	MT6336_RG_OTP_DOUT_8_15,
	MT6336_RG_OTP_DOUT_16_23,
	MT6336_RG_OTP_DOUT_24_31,
	MT6336_RG_OTP_DOUT_32_39,
	MT6336_RG_OTP_DOUT_40_47,
	MT6336_RG_OTP_DOUT_48_55,
	MT6336_RG_OTP_DOUT_56_63,
	MT6336_RG_OTP_DOUT_64_71,
	MT6336_RG_OTP_DOUT_72_79,
	MT6336_RG_OTP_DOUT_80_87,
	MT6336_RG_OTP_DOUT_88_95,
	MT6336_RG_OTP_DOUT_96_103,
	MT6336_RG_OTP_DOUT_104_111,
	MT6336_RG_OTP_DOUT_112_119,
	MT6336_RG_OTP_DOUT_120_127,
	MT6336_RG_OTP_DOUT_128_135,
	MT6336_RG_OTP_DOUT_136_143,
	MT6336_RG_OTP_DOUT_144_151,
	MT6336_RG_OTP_DOUT_152_159,
	MT6336_RG_OTP_DOUT_160_167,
	MT6336_RG_OTP_DOUT_168_175,
	MT6336_RG_OTP_DOUT_176_183,
	MT6336_RG_OTP_DOUT_184_191,
	MT6336_RG_OTP_DOUT_192_199,
	MT6336_RG_OTP_DOUT_200_207,
	MT6336_RG_OTP_DOUT_208_215,
	MT6336_RG_OTP_DOUT_216_223,
	MT6336_RG_OTP_DOUT_224_231,
	MT6336_RG_OTP_DOUT_232_239,
	MT6336_RG_OTP_DOUT_240_247,
	MT6336_RG_OTP_DOUT_248_255,
	MT6336_RG_OTP_DOUT_256_263,
	MT6336_RG_OTP_DOUT_264_271,
	MT6336_RG_OTP_DOUT_272_279,
	MT6336_RG_OTP_DOUT_280_287,
	MT6336_RG_OTP_DOUT_288_295,
	MT6336_RG_OTP_DOUT_296_303,
	MT6336_RG_OTP_DOUT_304_311,
	MT6336_RG_OTP_DOUT_312_319,
	MT6336_RG_OTP_DOUT_320_327,
	MT6336_RG_OTP_DOUT_328_335,
	MT6336_RG_OTP_DOUT_336_343,
	MT6336_RG_OTP_DOUT_344_351,
	MT6336_RG_OTP_DOUT_352_359,
	MT6336_RG_OTP_DOUT_360_367,
	MT6336_RG_OTP_DOUT_368_375,
	MT6336_RG_OTP_DOUT_376_383,
	MT6336_RG_OTP_DOUT_384_391,
	MT6336_RG_OTP_DOUT_392_399,
	MT6336_RG_OTP_DOUT_400_407,
	MT6336_RG_OTP_DOUT_408_415,
	MT6336_RG_OTP_DOUT_416_423,
	MT6336_RG_OTP_DOUT_424_431,
	MT6336_RG_OTP_DOUT_432_439,
	MT6336_RG_OTP_DOUT_440_447,
	MT6336_RG_OTP_DOUT_448_455,
	MT6336_RG_OTP_DOUT_456_463,
	MT6336_RG_OTP_DOUT_464_471,
	MT6336_RG_OTP_DOUT_472_479,
	MT6336_RG_OTP_DOUT_480_487,
	MT6336_RG_OTP_DOUT_488_495,
	MT6336_RG_OTP_DOUT_496_503,
	MT6336_RG_OTP_DOUT_504_511,
	MT6336_RG_OTP_VAL_0_7,
	MT6336_RG_OTP_VAL_8_15,
	MT6336_RG_OTP_VAL_16_23,
	MT6336_RG_OTP_VAL_24_31,
	MT6336_RG_OTP_VAL_32_39,
	MT6336_RG_OTP_VAL_40_47,
	MT6336_RG_OTP_VAL_48_55,
	MT6336_RG_OTP_VAL_56_63,
	MT6336_RG_OTP_VAL_64_71,
	MT6336_RG_OTP_VAL_72_79,
	MT6336_RG_OTP_VAL_80_87,
	MT6336_RG_OTP_VAL_88_95,
	MT6336_RG_OTP_VAL_96_103,
	MT6336_RG_OTP_VAL_104_111,
	MT6336_RG_OTP_VAL_112_119,
	MT6336_RG_OTP_VAL_120_127,
	MT6336_RG_OTP_VAL_128_135,
	MT6336_RG_OTP_VAL_136_143,
	MT6336_RG_OTP_VAL_144_151,
	MT6336_RG_OTP_VAL_152_159,
	MT6336_RG_OTP_VAL_160_167,
	MT6336_RG_OTP_VAL_168_175,
	MT6336_RG_OTP_VAL_176_183,
	MT6336_RG_OTP_VAL_184_191,
	MT6336_RG_OTP_VAL_192_199,
	MT6336_RG_OTP_VAL_200_207,
	MT6336_RG_OTP_VAL_208_215,
	MT6336_RG_OTP_VAL_216_223,
	MT6336_RG_OTP_VAL_224_231,
	MT6336_RG_OTP_VAL_232_239,
	MT6336_RG_OTP_VAL_240_247,
	MT6336_RG_OTP_VAL_248_255,
	MT6336_RG_OTP_VAL_256_263,
	MT6336_RG_OTP_VAL_264_271,
	MT6336_RG_OTP_VAL_272_279,
	MT6336_RG_OTP_VAL_280_287,
	MT6336_RG_OTP_VAL_288_295,
	MT6336_RG_OTP_VAL_296_303,
	MT6336_RG_OTP_VAL_304_311,
	MT6336_RG_OTP_VAL_312_319,
	MT6336_RG_OTP_VAL_320_327,
	MT6336_RG_OTP_VAL_328_335,
	MT6336_RG_OTP_VAL_336_343,
	MT6336_RG_OTP_VAL_344_351,
	MT6336_RG_OTP_VAL_352_359,
	MT6336_RG_OTP_VAL_360_367,
	MT6336_RG_OTP_VAL_368_375,
	MT6336_RG_OTP_VAL_376_383,
	MT6336_RG_OTP_VAL_384_391,
	MT6336_RG_OTP_VAL_392_399,
	MT6336_RG_OTP_VAL_400_407,
	MT6336_RG_OTP_VAL_408_415,
	MT6336_RG_OTP_VAL_416_423,
	MT6336_RG_OTP_VAL_424_431,
	MT6336_RG_OTP_VAL_432_439,
	MT6336_RG_OTP_VAL_440_447,
	MT6336_RG_OTP_VAL_448_455,
	MT6336_RG_OTP_VAL_456_463,
	MT6336_RG_OTP_VAL_464_471,
	MT6336_RG_OTP_VAL_472_479,
	MT6336_RG_OTP_VAL_480_487,
	MT6336_RG_OTP_VAL_488_495,
	MT6336_RG_OTP_VAL_496_503,
	MT6336_RG_OTP_VAL_504_511,
	MT6336_RG_A_VBUS_DIS_ITH,
	MT6336_RG_A_EN_IRCOMP,
	MT6336_RG_A_EN_OTP_TESTMODE,
	MT6336_RG_A_DIS_UG,
	MT6336_RG_A_DIS_LG,
	MT6336_RG_A_DIS_UGFLA,
	MT6336_RG_A_BASE_CLK_TRIM_EN,
	MT6336_RG_A_BASE_CLK_TRIM,
	MT6336_RG_A_BGR_TRIM_EN,
	MT6336_RG_A_BGR_TRIM,
	MT6336_RG_A_IVGEN_TRIM_EN,
	MT6336_RG_A_IVGEN_TRIM,
	MT6336_RG_A_BGR_RSEL,
	MT6336_RG_A_BGR_UNCHOP,
	MT6336_RG_A_BGR_UNCHOP_PH,
	MT6336_RG_A_BGR_TEST_RSTB,
	MT6336_RG_A_BGR_TEST_EN,
	MT6336_RG_A_VPREG_TRIM,
	MT6336_RG_A_VPREG_TCTRIM,
	MT6336_RG_A_EN_OTG_BVALID,
	MT6336_RG_A_OTP_SEL,
	MT6336_RG_A_OTP_TMODE,
	MT6336_RG_A_OTP_VREF_BG,
	MT6336_RG_A_OTP_DET_SEL,
	MT6336_RG_A_NI_VTHR_POL,
	MT6336_RG_A_NI_VADC18_VOSEL,
	MT6336_RG_A_NI_VLDO33_VOSEL,
	MT6336_RG_A_EN_VLDO33,
	MT6336_RG_A_VLDO33_READY_TRIM,
	MT6336_RG_A_NI_VDIG18_VOSEL,
	MT6336_RG_A_EN_DIG18_LOWQ,
	MT6336_RG_A_OSC_TRIM,
	MT6336_RG_A_VSYS_IDIS_ITH,
	MT6336_RG_A_VBAT_IDIS_ITH,
	MT6336_RG_A_EN_ITERM,
	MT6336_RG_A_EN_BATOC,
	MT6336_RG_A_HIQCP_CLKSEL,
	MT6336_RG_A_PPFET_ATEST_SEL,
	MT6336_RG_A_PPFET_DTEST_SEL,
	MT6336_RG_A_EN_TRIM_IBATSNS,
	MT6336_RG_A_EN_IBATSNS_CALIB,
	MT6336_RG_A_TRIM_IBATSNS,
	MT6336_RG_A_IBAT_DAC_CALIB,
	MT6336_RG_A_IBAT_DAC_TRIM_LO,
	MT6336_RG_A_IBAT_DAC_TRIM_HI,
	MT6336_RG_A_OFFSET_RES_SEL,
	MT6336_RG_A_IPRECHG_TRIM,
	MT6336_RG_A_TRIM_VD,
	MT6336_RG_A_VD_TEST,
	MT6336_RG_A_BATOC_TEST,
	MT6336_RG_A_TRIM_BATOC,
	MT6336_RG_A_FEN_CP_HIQ,
	MT6336_RG_A_ASW_BIAS_EN,
	MT6336_RG_A_SST_SLOW_SSTART_EN,
	MT6336_RG_A_SST_FORCE_NON_SST_EN,
	MT6336_RG_A_SST_RAMP_SEL,
	MT6336_RG_A_OSC_CLK_SEL,
	MT6336_RG_A_OTG_VM_UVLO_VTH,
	MT6336_RG_A_R_IRCOMP,
	MT6336_RG_A_LOOP_CCEXTR_SEL,
	MT6336_RG_A_LOOP_CCREF_SEL_ICHIN_SW,
	MT6336_RG_A_LOOP_THR_SEL,
	MT6336_RG_A_LOOP_CCREF_SEL_ICHIN,
	MT6336_RG_A_LOOP_CCREF_SEL_IBAT_SW,
	MT6336_RG_A_LOOP_CCREF_SEL_IBAT,
	MT6336_RG_A_LOOP_CLAMP_EN,
	MT6336_RG_A_LOOP_GM_EN,
	MT6336_RG_A_LOOP_GM_TUNE_DPM_MSB,
	MT6336_RG_A_LOOP_GM_TUNE_DPM_LSB,
	MT6336_RG_A_LOOP_GM_TUNE_IBAT_MSB,
	MT6336_RG_A_LOOP_GM_TUNE_IBAT_LSB,
	MT6336_RG_A_LOOP_GM_TUNE_ICHIN_MSB,
	MT6336_RG_A_LOOP_GM_TUNE_ICHIN_LSB,
	MT6336_RG_A_LOOP_GM_TUNE_SYS_MSB,
	MT6336_RG_A_LOOP_GM_TUNE_SYS_LSB,
	MT6336_RG_A_LOOP_GM_TUNE_THR_MSB,
	MT6336_RG_A_LOOP_GM_TUNE_THR_LSB,
	MT6336_RG_A_LOOP_GM_TUNE_BOOST_MSB,
	MT6336_RG_A_LOOP_GM_TUNE_BOOST_LSB,
	MT6336_RG_A_LOOP_ICS,
	MT6336_RG_A_LOOP_CLAMPEH,
	MT6336_RG_A_LOOP_RC,
	MT6336_RG_A_LOOP_CC,
	MT6336_RG_A_LOOP_COMP,
	MT6336_RG_A_LOOP_GM_RSV_MSB,
	MT6336_RG_A_LOOP_GM_RSV_LSB,
	MT6336_RG_A_LOOP_RSV_TRIM_MSB,
	MT6336_RG_A_LOOP_RSV_TRIM_LSB,
	MT6336_RG_A_LOOP_SWCHR_CC_VREFTRIM,
	MT6336_RG_A_LOOP_SWCHR_CV_VREFTRIM,
	MT6336_RG_A_LOOP_SYS_DPM_VREFTRIM,
	MT6336_RG_A_LOOP_DIS_PROT,
	MT6336_RG_A_LOOP_SWCHR_VBAT_PROT,
	MT6336_RG_A_LOOP_SWCHR_VSYS_PROT,
	MT6336_RG_A_LOOP_THR_ANA_EN,
	MT6336_RG_A_LOOP_USB_DL_MODE,
	MT6336_RG_A_MULTI_CAP,
	MT6336_RG_A_LOOP_CS_ICL_TRIM250,
	MT6336_RG_A_LOOP_CS_ICL_TRIM500_1,
	MT6336_RG_A_LOOP_CS_ICL_TRIM500_2,
	MT6336_RG_A_LOOP_CS_ICL_TRIM1000,
	MT6336_RG_A_LOOP_CS_ICC_TRIM250,
	MT6336_RG_A_LOOP_CS_ICC_TRIM500,
	MT6336_RG_A_LOOP_CS_ICC_TRIM1000,
	MT6336_RG_A_LOOP_100K_TRIM,
	MT6336_RG_A_LOOP_CCINTR_TRIM_EN,
	MT6336_RG_A_LOOP_100K_ICC_TRIM,
	MT6336_RG_A_LOOP_100K_ICL_TRIM,
	MT6336_RG_A_VRAMP_DCOS,
	MT6336_RG_A_VRAMP_SLP,
	MT6336_RG_A_VRAMP_SLP_RTUNE1,
	MT6336_RG_A_VRAMP_SLP_RTUNE2,
	MT6336_RG_A_VRAMP_VCS_RTUNE,
	MT6336_RG_A_PLIM_PWR_OCLIM_OFF,
	MT6336_RG_A_PLIM_PWR_OCLIMASYN_OFF,
	MT6336_RG_A_PLIM_CCEXTR_SEL,
	MT6336_RG_A_PLIM_OTG_OCTH,
	MT6336_RG_A_PLIM_FLASH_OCTH,
	MT6336_RG_A_PLIM_SWCHR_OCTH,
	MT6336_RG_A_PLIM_SWCHR_ASYN_OCTH,
	MT6336_RG_A_PLIM_SWCHR_INTITH,
	MT6336_RG_A_PLIM_BOOST_INTITH,
	MT6336_RG_A_PLIM_SWCHR_OC_REFTRIM,
	MT6336_RG_A_PLIM_SWCHR_ASYNOC_REFTRIM,
	MT6336_RG_A_PLIM_BOOST_OC_REFTRIM,
	MT6336_RG_A_LOGIC_BOOST_CLKSEL,
	MT6336_RG_A_LOGIC_BOOST_MAXDUTY_SEL,
	MT6336_RG_A_LOGIC_BOUND,
	MT6336_RG_A_LOGIC_BURST,
	MT6336_RG_A_LOGIC_DEL_TUNE1,
	MT6336_RG_A_LOGIC_DEL_TUNE2,
	MT6336_RG_A_LOGIC_DEL_TUNE3,
	MT6336_RG_A_LOGIC_DEL_TUNE4,
	MT6336_RG_A_LOGIC_ENPWM_PULSE_FEN,
	MT6336_RG_A_LOGIC_FORCE_NON_MBATPP_OC,
	MT6336_RG_A_LOGIC_FORCE_NON_SYS_OV,
	MT6336_RG_A_LOGIC_FORCE_NON_PLIM,
	MT6336_RG_A_LOGIC_FORCE_NON_ASYN,
	MT6336_RG_A_LOGIC_FREQ_HALF,
	MT6336_RG_A_LOGIC_GDRI_MINOFF_DIS,
	MT6336_RG_A_LOGIC_GDRI_MINOFF_SEL,
	MT6336_RG_A_LOGIC_SWCHR_FPWM,
	MT6336_RG_A_LOGIC_MINOFF_CKRST,
	MT6336_RG_A_LOGIC_RSV_TRIM_MSB,
	MT6336_RG_A_LOGIC_RSV_TRIM_LSB,
	MT6336_RG_A_BUSBAT_DIFF1,
	MT6336_RG_A_BUSBAT_DIFF2,
	MT6336_RG_A_ZC_SWCHR_ZX_TRIM,
	MT6336_RG_A_ZC_SWCHR_ZX_TESTMODE,
	MT6336_RG_A_ZC_OTG_ZX_TRIM,
	MT6336_RG_A_ZC_OTG_ZX_TESTMODE,
	MT6336_RG_A_ZC_FLASH_ZX_TRIM,
	MT6336_RG_A_ZC_FLASH_ZX_TESTMODE,
	MT6336_RG_A_PWR_VBSTOK_SEL,
	MT6336_RG_A_PWR_LPD,
	MT6336_RG_A_PWR_UGLG_ENPWM_FEN,
	MT6336_RG_A_PWR_RSV_MSB,
	MT6336_RG_A_PWR_RSV_LSB,
	MT6336_RG_A_PWR_UG_VTHSEL,
	MT6336_RG_A_PWR_UG_SRC,
	MT6336_RG_A_PWR_UG_SRCEH,
	MT6336_RG_A_PWR_UG_DTC,
	MT6336_RG_A_PWR_LG_VTHSEL,
	MT6336_RG_A_PWR_LG_SRC,
	MT6336_RG_A_PWR_LG_SRCEH,
	MT6336_RG_A_PWR_LG_DTC,
	MT6336_RG_A_REVFET_ICS_AZC_DIS,
	MT6336_RG_A_REVFET_ICS_CLKEXT_EN,
	MT6336_RG_A_REVFET_ICS_HFREQ,
	MT6336_RG_A_REVFET_DPH_AZC_DIS,
	MT6336_RG_A_REVFET_K_TRIM_MSB,
	MT6336_RG_A_REVFET_K_TRIM_LSB,
	MT6336_RG_A_REVFET_SEL,
	MT6336_RG_A_IBAT_K_TRIM_MSB,
	MT6336_RG_A_IBAT_K_TRIM_LSB,
	MT6336_RG_A_SWCHR_RSV_TRIM_MSB,
	MT6336_RG_A_SWCHR_RSV_TRIM_LSB,
	MT6336_RG_A_FLASH1_TRIM,
	MT6336_RG_A_FLASH2_TRIM,
	MT6336_RG_A_FLASH_VCLAMP_SEL,
	MT6336_RG_A_IOS_DET_ITH,
	MT6336_RG_A_SWCHR_ZCD_TRIM_EN,
	MT6336_RG_A_LOOP_FTR_DROP_EN,
	MT6336_RG_A_LOOP_FTR_SHOOT_EN,
	MT6336_RG_A_LOOP_FTR_DROP,
	MT6336_RG_A_LOOP_FTR_RC,
	MT6336_RG_A_LOOP_FTR_DELAY,
	MT6336_RG_A_LOOP_FTR_DROP_MODE,
	MT6336_RG_A_LOOP_FTR_SHOOT_MODE,
	MT6336_RG_A_LOOP_FTR_DISCHARGE_EN,
	MT6336_RG_A_SWCHR_ZCD_TRIM_MODE,
	MT6336_RG_A_SWCHR_TESTMODE1,
	MT6336_RG_A_SWCHR_TESTMODE2,
	MT6336_RG_A_BASE_TESTMODE,
	MT6336_RG_A_FLA_TESTMODE,
	MT6336_RG_A_PPFET_TESTMODE,
	MT6336_RG_A_ANABASE_RSV,
	MT6336_RG_A_BC12_IBIAS_EN,
	MT6336_RG_A_BC12_IPD_EN,
	MT6336_RG_A_BC12_IPU_EN,
	MT6336_RG_A_BC12_IPDC_EN,
	MT6336_RG_A_BC12_VSRC_EN,
	MT6336_RG_A_BC12_CMP_EN,
	MT6336_RG_A_BC12_VREF_VTH_EN,
	MT6336_RG_A_BC12_IPD_HALF_EN,
	MT6336_RG_A_BC12_BB_CTRL,
	MT6336_RG_A_DIS_ADC18,
	MT6336_RG_A_SSCTRL_SEL,
	MT6336_AUXADC_LBAT2_DEBT_MAX,
	MT6336_AUXADC_LBAT2_DEBT_MIN,
	MT6336_AUXADC_LBAT2_DET_PRD_15_0_L,
	MT6336_AUXADC_LBAT2_DET_PRD_15_0_H,
	MT6336_AUXADC_LBAT2_DET_PRD_19_16,
	MT6336_AUXADC_LBAT2_VOLT_MAX_L,
	MT6336_AUXADC_LBAT2_VOLT_MAX_H,
	MT6336_AUXADC_LBAT2_IRQ_EN_MAX,
	MT6336_AUXADC_LBAT2_EN_MAX,
	MT6336_AUXADC_LBAT2_MAX_IRQ_B,
	MT6336_AUXADC_LBAT2_VOLT_MIN_L,
	MT6336_AUXADC_LBAT2_VOLT_MIN_H,
	MT6336_AUXADC_LBAT2_IRQ_EN_MIN,
	MT6336_AUXADC_LBAT2_EN_MIN,
	MT6336_AUXADC_LBAT2_MIN_IRQ_B,
	MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MAX_L,
	MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MAX_H,
	MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN_L,
	MT6336_AUXADC_LBAT2_DEBOUNCE_COUNT_MIN_H,
	MT6336_AUXADC_JEITA_DEBT,
	MT6336_AUXADC_JEITA_MIPI_DIS,
	MT6336_AUXADC_JEITA_FROZE_EN,
	MT6336_AUXADC_JEITA_IRQ_EN,
	MT6336_AUXADC_JEITA_EN,
	MT6336_AUXADC_JEITA_DET_PRD,
	MT6336_AUXADC_JEITA_VOLT_HOT_L,
	MT6336_AUXADC_JEITA_VOLT_HOT_H,
	MT6336_AUXADC_JEITA_HOT_IRQ,
	MT6336_AUXADC_JEITA_VOLT_WARM_L,
	MT6336_AUXADC_JEITA_VOLT_WARM_H,
	MT6336_AUXADC_JEITA_WARM_IRQ,
	MT6336_AUXADC_JEITA_VOLT_COOL_L,
	MT6336_AUXADC_JEITA_VOLT_COOL_H,
	MT6336_AUXADC_JEITA_COOL_IRQ,
	MT6336_AUXADC_JEITA_VOLT_COLD_L,
	MT6336_AUXADC_JEITA_VOLT_COLD_H,
	MT6336_AUXADC_JEITA_COLD_IRQ,
	MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_WARM,
	MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_HOT,
	MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_COLD,
	MT6336_AUXADC_JEITA_DEBOUNCE_COUNT_COOL,
	MT6336_AUXADC_NAG_EN,
	MT6336_AUXADC_NAG_CLR,
	MT6336_AUXADC_NAG_VBAT1_SEL,
	MT6336_AUXADC_NAG_IRQ_EN,
	MT6336_AUXADC_NAG_C_DLTV_IRQ,
	MT6336_AUXADC_NAG_PRD,
	MT6336_AUXADC_NAG_ZCV_L,
	MT6336_AUXADC_NAG_ZCV_H,
	MT6336_AUXADC_NAG_C_DLTV_TH_15_0_L,
	MT6336_AUXADC_NAG_C_DLTV_TH_15_0_H,
	MT6336_AUXADC_NAG_C_DLTV_TH_26_16_L,
	MT6336_AUXADC_NAG_C_DLTV_TH_26_16_H,
	MT6336_AUXADC_NAG_CNT_15_0_L,
	MT6336_AUXADC_NAG_CNT_15_0_H,
	MT6336_AUXADC_NAG_CNT_25_16_L,
	MT6336_AUXADC_NAG_CNT_25_16_H,
	MT6336_AUXADC_NAG_DLTV_L,
	MT6336_AUXADC_NAG_DLTV_H,
	MT6336_AUXADC_NAG_C_DLTV_15_0_L,
	MT6336_AUXADC_NAG_C_DLTV_15_0_H,
	MT6336_AUXADC_NAG_C_DLTV_26_16_L,
	MT6336_AUXADC_NAG_C_DLTV_26_16_H,
	MT6336_AUXADC_VBAT_DET_PRD_15_0_L,
	MT6336_AUXADC_VBAT_DET_PRD_15_0_H,
	MT6336_AUXADC_VBAT_DET_PRD_19_16,
	MT6336_AUXADC_VBAT_EN_ADC_RECHG,
	MT6336_AUXADC_VBAT_EN_MODE_SEL,
	MT6336_AUXADC_VBAT_IRQ_EN,
	MT6336_AUXADC_VBAT_1_RSV_0,
	MT6336_AUXADC_VBAT_DET_DEBT,
	MT6336_AUXADC_VBAT_DET_VOLT_11_0_L,
	MT6336_AUXADC_VBAT_DET_VOLT_11_0_H,
	MT6336_AUXADC_VBAT_2_RSV_0,
	MT6336_AUXADC_VBAT_VTH_MODE_SEL,
	MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_HIGH_8_0_L,
	MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_HIGH_8_0_H,
	MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_LOW_8_0_L,
	MT6336_AUXADC_VBAT_DEBOUNCE_COUNT_LOW_8_0_H,
	MT6336_RG_AD_QI_VBAT_LT_V3P2,
	MT6336_RG_AD_QI_VBAT_LT_V2P7,
	MT6336_RG_AD_QI_OTG_BVALID,
	MT6336_RG_AD_QI_PP_EN_IN,
	MT6336_RG_AD_QI_ICL150PIN_LVL,
	MT6336_RG_AD_QI_TEMP_GT_150,
	MT6336_RG_AD_QI_VIO18_READY,
	MT6336_RG_AD_QI_VBGR_READY,
	MT6336_RG_AD_QI_VLED2_OPEN,
	MT6336_RG_AD_QI_VLED1_OPEN,
	MT6336_RG_AD_QI_VLED2_SHORT,
	MT6336_RG_AD_QI_VLED1_SHORT,
	MT6336_RG_AD_QI_VUSB33_READY,
	MT6336_RG_AD_QI_VPREG_RDY,
	MT6336_RG_AD_QI_VBUS_GT_POR,
	MT6336_RG_AD_QI_BC12_CMP_OUT_SW,
	MT6336_RG_AD_QI_BC12_CMP_OUT,
	MT6336_RG_AD_QI_MBATPP_DIS_OC,
	MT6336_RG_AD_NS_VPRECHG_FAIL,
	MT6336_RG_AD_NI_ICHR_LT_ITERM,
	MT6336_RG_AD_QI_VBAT_LT_PRECC1,
	MT6336_RG_AD_QI_VBAT_LT_PRECC0,
	MT6336_RG_AD_QI_MBATPP_DTEST2,
	MT6336_RG_AD_QI_MBATPP_DTEST1,
	MT6336_RG_AD_QI_OTG_VM_OVP,
	MT6336_RG_AD_QI_OTG_VM_UVLO,
	MT6336_RG_AD_QI_OTG_VBAT_UVLO,
	MT6336_RG_AD_NI_VBAT_OVP,
	MT6336_RG_AD_NI_VSYS_OVP,
	MT6336_RG_AD_NI_WEAKBUS,
	MT6336_RG_AD_QI_VBUS_UVLO,
	MT6336_RG_AD_QI_VBUS_OVP,
	MT6336_RG_AD_QI_PAM_MODE,
	MT6336_RG_AD_QI_CV_MODE,
	MT6336_RG_AD_QI_ICC_MODE,
	MT6336_RG_AD_QI_ICL_MODE,
	MT6336_RG_AD_QI_FLASH_VFLA_OVP,
	MT6336_RG_AD_QI_FLASH_VFLA_UVLO,
	MT6336_RG_AD_QI_SWCHR_OC_STATUS,
	MT6336_RG_AD_QI_BOOT_UVLO,
	MT6336_RG_AD_QI_ZX_SWCHR_ZCD_FLAG,
	MT6336_RG_AD_NI_ZX_OTG_TEST,
	MT6336_RG_AD_NI_ZX_FLASH_TEST,
	MT6336_RG_AD_NI_ZX_SWCHR_TEST,
	MT6336_RG_AD_QI_OTG_OLP,
	MT6336_RG_AD_QI_THR_MODE,
	MT6336_RG_AD_NI_FTR_SHOOT_DB,
	MT6336_RG_AD_NI_FTR_SHOOT,
	MT6336_RG_AD_NI_FTR_DROP_DB,
	MT6336_RG_AD_NI_FTR_DROP,
	MT6336_RG_AD_QI_VADC18_READY,
	MT6336_RG_AD_QI_SWCHR_ENPWM_STATUS,
	MT6336_RG_AD_QI_SWCHR_OCASYN_STATUS,
	MT6336_RG_DA_QI_TORCH_MODE,
	MT6336_RG_DA_QI_OTG_MODE_DB,
	MT6336_RG_DA_QI_FLASH_MODE_DB,
	MT6336_RG_DA_QI_BUCK_MODE_DB,
	MT6336_RG_DA_QI_BASE_READY_DB,
	MT6336_RG_DA_QI_BASE_READY,
	MT6336_RG_DA_QI_LOWQ_STAT,
	MT6336_RG_DA_QI_SHIP_STAT,
	MT6336_RG_DA_QI_TORCH_MODE_DRV_EN,
	MT6336_RG_DA_QI_TORCH_MODE_DB,
	MT6336_RG_DA_QI_BASE_CLK_TRIM,
	MT6336_RG_DA_QI_OSC_TRIM,
	MT6336_RG_DA_QI_CLKSQ_IN_SEL,
	MT6336_RG_DA_QI_CLKSQ_EN,
	MT6336_RG_DA_QI_EFUSE_READ_RDY_DLY,
	MT6336_RG_DA_QI_VBGR_READY_DB,
	MT6336_RG_DA_QI_BGR_SPEEDUP_EN,
	MT6336_RG_DA_QI_ILED1_ITH,
	MT6336_RG_DA_QI_ADC18_EN,
	MT6336_RG_DA_QI_ILED1_EN,
	MT6336_RG_DA_QI_ILED2_ITH,
	MT6336_RG_DA_QI_EN_ADCIN_VLED1,
	MT6336_RG_DA_QI_EN_ADCIN_VBATON,
	MT6336_RG_DA_QI_EN_ADCIN_VBUS,
	MT6336_RG_DA_QI_EN_ADCIN_VBATSNS,
	MT6336_RG_DA_QI_IOSDET2_EN,
	MT6336_RG_DA_QI_IOSDET1_EN,
	MT6336_RG_DA_QI_OSDET_EN,
	MT6336_RG_DA_QI_ILED2_EN,
	MT6336_RG_DA_QI_EN_ADCIN_VLED2,
	MT6336_RG_DA_QI_BC12_IPDC_EN,
	MT6336_RG_DA_QI_BC12_IPU_EN,
	MT6336_RG_DA_QI_BC12_IPD_EN,
	MT6336_RG_DA_QI_BC12_IBIAS_EN,
	MT6336_RG_DA_QI_BC12_BB_CTRL,
	MT6336_RG_DA_QI_BC12_IPD_HALF_EN,
	MT6336_RG_DA_QI_BC12_VREF_VTH_EN,
	MT6336_RG_DA_QI_BC12_CMP_EN,
	MT6336_RG_DA_QI_BC12_VSRC_EN,
	MT6336_RG_DA_QI_VPREG_RDY_DB,
	MT6336_RG_DA_QI_IPRECC1_ITH,
	MT6336_RG_DA_QI_EN_IBIASGEN,
	MT6336_RG_DA_NS_VPRECHG_FAIL_DB,
	MT6336_RG_DA_QI_EN_CP_HIQ,
	MT6336_RG_DA_QI_VBAT_LT_PRECC1_DB,
	MT6336_RG_DA_QI_VBAT_LT_PRECC0_DB,
	MT6336_RG_DA_QI_IPRECC1_EN,
	MT6336_RG_DA_QI_IPRECC0_EN,
	MT6336_RG_DA_QI_VBATFETON_VTH,
	MT6336_RG_DA_QI_BATOC_ANA_SEL,
	MT6336_RG_DA_QI_FETON_OK_LOQCP_EN,
	MT6336_RG_DA_QI_EN_VDC_IBIAS,
	MT6336_RG_DA_NI_CHRIND_EN,
	MT6336_RG_DA_NI_CHRIND_BIAS_EN,
	MT6336_RG_DA_QI_VSYS_IDIS_EN,
	MT6336_RG_DA_QI_VBAT_IDIS_EN,
	MT6336_RG_DA_QI_ITERM_ITH,
	MT6336_RG_DA_NI_CHRIND_STEP,
	MT6336_RG_DA_QI_CHR_DET,
	MT6336_RG_DA_QI_REVFET_EN,
	MT6336_RG_DA_QI_SSFNSH,
	MT6336_RG_DA_QI_IBACKBOOST_EN,
	MT6336_RG_DA_NI_WEAKBUS_DB,
	MT6336_RG_DA_QI_VBUS_OVP_DB,
	MT6336_RG_DA_QI_VBUS_UVLO_DB,
	MT6336_RG_DA_QI_ICL_ITH,
	MT6336_RG_DA_QI_CV_REGNODE_SEL,
	MT6336_RG_DA_QI_ICC_ITH,
	MT6336_RG_DA_QI_VSYSREG_VTH,
	MT6336_RG_DA_QI_VCV_VTH,
	MT6336_RG_DA_QI_OTG_VCV_VTH,
	MT6336_RG_DA_QI_FLASH_MODE_DRV_EN,
	MT6336_RG_DA_QI_OTG_MODE,
	MT6336_RG_DA_QI_OTG_MODE_DRV_EN,
	MT6336_RG_DA_NI_VBSTCHK_FEN,
	MT6336_RG_DA_QI_VFLA_VTH,
	MT6336_RG_DA_QI_FLASH_VFLA_OVP_DB,
	MT6336_RG_DA_QI_FLASH_VFLA_UVLO_DB,
	MT6336_RG_DA_QI_OTG_VM_UVLO_DB,
	MT6336_RG_DA_QI_OTG_VM_OVP_DB,
	MT6336_RG_DA_QI_OSC_REF_EN,
	MT6336_RG_DA_QI_BUCK_MODE,
	MT6336_RG_DA_QI_BUCK_MODE_DRV_EN,
	MT6336_RG_DA_QI_FLASH_MODE,
	MT6336_RG_DA_QI_MFLAPP_EN,
	MT6336_RG_DA_QI_OTG_IOLP_ITH,
	MT6336_RG_DA_QI_VSYS_REGV_SEL,
	MT6336_RG_DA_QI_OTG_VBAT_UVLO_DB,
	MT6336_RG_DA_NI_VBAT_OVP_DB,
	MT6336_RG_DA_NI_VSYS_OVP_DB,
	MT6336_RG_DA_QI_BACKGROUND_STAT,
	MT6336_RG_DA_QI_DEADBAT_STAT,
	MT6336_RG_DA_QI_EOC_STAT,
	MT6336_RG_DA_QI_FASTCC_STAT,
	MT6336_RG_DA_QI_POSTPRECC1_STAT,
	MT6336_RG_DA_QI_VBUS_PLUGIN,
	MT6336_RG_DA_QI_VBUS_IDIS_EN,
	MT6336_RG_DA_QI_MTORPP_EN,
	MT6336_RG_DA_QI_VPAM_VTH,
	MT6336_RG_AD_AUXADC_COMP,
	MT6336_RG_DA_AUXADC_DAC_0,
	MT6336_RG_DA_AUXADC_DAC_1,
	MT6336_RG_DA_AUXADC_SEL,
	MT6336_RG_DA_TS_VBE_SEL,
	MT6336_RG_DA_VBUF_EN,
	MT6336_RG_DA_AUXADC_RNG,
	MT6336_RG_DA_AUXADC_SPL,
	MT6336_RG_DA_AUXADC_ADC_PWDB,
	MT6336_RG_AD_PD_CC2_OVP,
	MT6336_RG_AD_PD_CC1_OVP,
	MT6336_RG_AD_PD_VCONN_UVP,
	MT6336_RG_AD_PD_RX_DATA,
	MT6336_RG_AD_CC_VUSB33_RDY,
	MT6336_RG_AD_CC_CMP_OUT,
	MT6336_RG_DA_CC_LPF_EN,
	MT6336_RG_DA_CC_BIAS_EN,
	MT6336_RG_DA_CC_RACC2_EN,
	MT6336_RG_DA_CC_RDCC2_EN,
	MT6336_RG_DA_CC_RPCC2_EN,
	MT6336_RG_DA_CC_RACC1_EN,
	MT6336_RG_DA_CC_RDCC1_EN,
	MT6336_RG_DA_CC_RPCC1_EN,
	MT6336_RG_DA_PD_TX_DATA,
	MT6336_RG_DA_PD_TX_EN,
	MT6336_RG_DA_PD_BIAS_EN,
	MT6336_RG_DA_CC_DAC_EN,
	MT6336_RG_DA_CC_SASW_EN,
	MT6336_RG_DA_CC_ADCSW_EN,
	MT6336_RG_DA_CC_SW_SEL,
	MT6336_RG_DA_CC_LEV_EN,
	MT6336_RG_DA_CC_DAC_IN,
	MT6336_RG_DA_PD_RX_EN,
	MT6336_RG_DA_CC_DAC_GAIN_CAL,
	MT6336_RG_DA_CC_DAC_CAL,
	MT6336_RG_DA_PD_CONNSW,
	MT6336_RG_DA_PD_CCSW,
	MT6336_AD_QI_OTG_BVALID,
	MT6336_AD_QI_PP_EN_IN,
	MT6336_AD_QI_ICL150PIN_LVL,
	MT6336_AD_QI_TEMP_GT_150,
	MT6336_AD_QI_VIO18_READY,
	MT6336_AD_QI_VDIG18_READY,
	MT6336_AD_QI_VBGR_READY,
	MT6336_AD_QI_TESTMODE,
	MT6336_AD_QI_VLED1_OPEN,
	MT6336_AD_QI_VLED2_SHORT,
	MT6336_AD_QI_VLED1_SHORT,
	MT6336_AD_QI_VUSB33_READY,
	MT6336_AD_QI_VPREG_RDY,
	MT6336_AD_QI_VBUS_GT_POR,
	MT6336_AD_QI_VBAT_LT_V3P2,
	MT6336_AD_QI_VBAT_LT_V2P7,
	MT6336_AD_NI_ICHR_LT_ITERM,
	MT6336_AD_QI_VBAT_LT_PRECC1,
	MT6336_AD_QI_VBAT_LT_PRECC0,
	MT6336_AD_QI_MBATPP_DTEST2,
	MT6336_AD_QI_MBATPP_DTEST1,
	MT6336_AD_QI_BC12_CMP_OUT_SW,
	MT6336_AD_QI_BC12_CMP_OUT,
	MT6336_AD_QI_VLED2_OPEN,
	MT6336_AD_QI_OTG_VBAT_UVLO,
	MT6336_AD_NI_VBAT_OVP,
	MT6336_AD_NI_VSYS_OVP,
	MT6336_AD_NI_WEAKBUS,
	MT6336_AD_QI_VBUS_UVLO,
	MT6336_AD_QI_VBUS_OVP,
	MT6336_AD_QI_MBATPP_DIS_OC,
	MT6336_AD_NS_VPRECHG_FAIL,
	MT6336_AD_QI_PAM_MODE,
	MT6336_AD_QI_CV_MODE,
	MT6336_AD_QI_ICC_MODE,
	MT6336_AD_QI_ICL_MODE,
	MT6336_AD_QI_FLASH_VFLA_OVP,
	MT6336_AD_QI_FLASH_VFLA_UVLO,
	MT6336_AD_QI_OTG_VM_OVP,
	MT6336_AD_QI_OTG_VM_UVLO,
	MT6336_AD_QI_SWCHR_OC_STATUS,
	MT6336_AD_QI_BOOT_UVLO,
	MT6336_AD_QI_ZX_SWCHR_ZCD_FLAG,
	MT6336_AD_NI_ZX_OTG_TEST,
	MT6336_AD_NI_ZX_FLASH_TEST,
	MT6336_AD_NI_ZX_SWCHR_TEST,
	MT6336_AD_QI_OTG_OLP,
	MT6336_AD_QI_THR_MODE,
	MT6336_AD_QI_VADC18_READY,
	MT6336_AD_QI_SWCHR_ENPWM_STATUS,
	MT6336_AD_QI_SWCHR_OCASYN_STATUS,
	MT6336_RESERVED_1,
	MT6336_DA_QI_OTG_MODE_DB_MUX,
	MT6336_DA_QI_FLASH_MODE_DB_MUX,
	MT6336_DA_QI_BUCK_MODE_DB_MUX,
	MT6336_DA_QI_BASE_READY_DB_MUX,
	MT6336_DA_QI_BASE_READY_MUX,
	MT6336_DA_QI_LOWQ_STAT_MUX,
	MT6336_DA_QI_SHIP_STAT_MUX,
	MT6336_DA_QI_EN_CP_HIQ_MUX,
	MT6336_DA_QI_VBAT_LT_PRECC1_DB_MUX,
	MT6336_DA_QI_VBAT_LT_PRECC0_DB_MUX,
	MT6336_DA_QI_EFUSE_READ_RDY_DLY_MUX,
	MT6336_DA_QI_VBGR_READY_DB_MUX,
	MT6336_DA_QI_TORCH_MODE_DRV_EN_MUX,
	MT6336_DA_QI_TORCH_MODE_DB_MUX,
	MT6336_DA_QI_TORCH_MODE_MUX,
	MT6336_DA_QI_VBAT_IDIS_EN_MUX,
	MT6336_DA_QI_IPRECC1_EN_MUX,
	MT6336_DA_QI_IPRECC0_EN_MUX,
	MT6336_DA_QI_FETON_OK_LOQCP_EN_MUX,
	MT6336_DA_QI_EN_VDC_IBIAS_MUX,
	MT6336_DA_QI_VPREG_RDY_DB_MUX,
	MT6336_DA_QI_EN_IBIASGEN_MUX,
	MT6336_DA_NS_VPRECHG_FAIL_DB_MUX,
	MT6336_DA_QI_CV_REGNODE_SEL_MUX,
	MT6336_DA_QI_CHR_DET_MUX,
	MT6336_DA_QI_REVFET_EN_MUX,
	MT6336_DA_QI_IBACKBOOST_EN_MUX,
	MT6336_DA_NI_WEAKBUS_DB_MUX,
	MT6336_DA_QI_VBUS_OVP_DB_MUX,
	MT6336_DA_QI_VBUS_UVLO_DB_MUX,
	MT6336_DA_QI_VSYS_IDIS_EN_MUX,
	MT6336_DA_QI_ICL_ITH_MUX,
	MT6336_DA_QI_ICC_ITH_MUX,
	MT6336_DA_QI_VSYSREG_VTH_MUX,
	MT6336_DA_QI_VCV_VTH_MUX,
	MT6336_DA_QI_VPAM_VTH_MUX,
	MT6336_DA_QI_VFLA_VTH_MUX,
	MT6336_DA_QI_OTG_VCV_VTH_MUX,
	MT6336_DA_QI_OTG_VM_UVLO_DB_MUX,
	MT6336_DA_QI_OTG_VM_OVP_DB_MUX,
	MT6336_DA_QI_BUCK_MODE_MUX,
	MT6336_DA_QI_BUCK_MODE_DRV_EN_MUX,
	MT6336_DA_QI_FLASH_MODE_MUX,
	MT6336_DA_QI_FLASH_MODE_DRV_EN_MUX,
	MT6336_DA_QI_OTG_MODE_MUX,
	MT6336_DA_QI_OTG_MODE_DRV_EN_MUX,
	MT6336_DA_QI_POSTPRECC1_STAT_MUX,
	MT6336_DA_QI_VBUS_PLUGIN_MUX,
	MT6336_DA_QI_VSYS_LT_FETON_DB_MUX,
	MT6336_DA_QI_OTG_VBAT_UVLO_DB_MUX,
	MT6336_DA_NI_VBAT_OVP_DB_MUX,
	MT6336_DA_NI_VSYS_OVP_DB_MUX,
	MT6336_DA_QI_FLASH_VFLA_OVP_DB_MUX,
	MT6336_DA_QI_FLASH_VFLA_UVLO_DB_MUX,
	MT6336_DA_QI_BACKGROUND_STAT_MUX,
	MT6336_DA_QI_DEADBAT_STAT_MUX,
	MT6336_DA_QI_EOC_STAT_MUX,
	MT6336_DA_QI_FASTCC_STAT_MUX,
	MT6336_RESERVED_2,
	MT6336_RESERVED_3,
	MT6336_RESERVED_4,
	MT6336_RESERVED_5,
	MT6336_RESERVED_6,
	MT6336_RESERVED_7,
	MT6336_RESERVED_8,
	MT6336_RESERVED_9,
	MT6336_RESERVED_10,
	MT6336_RESERVED_11,
	MT6336_RESERVED_12,
	MT6336_RESERVED_13,
	MT6336_RESERVED_14,
	MT6336_RESERVED_15,
	MT6336_RESERVED_16,
	MT6336_RESERVED_17,
	MT6336_RESERVED_18,
	MT6336_RESERVED_19,
	MT6336_RESERVED_20,
	MT6336_RESERVED_21,
	MT6336_RESERVED_22,
	MT6336_RESERVED_23,
	MT6336_RESERVED_24,
	MT6336_RESERVED_25,
	MT6336_RESERVED_26,
	MT6336_RESERVED_27,
	MT6336_RESERVED_28,
	MT6336_RESERVED_29,
	MT6336_RESERVED_30,
	MT6336_RESERVED_31,
	MT6336_RESERVED_32,
	MT6336_RESERVED_33,
	MT6336_RESERVED_34,
	MT6336_RESERVED_35,
	MT6336_RESERVED_36,
	MT6336_RESERVED_37,
	MT6336_RESERVED_38,
	MT6336_RESERVED_39,
	MT6336_RESERVED_40,
	MT6336_RESERVED_41,
	MT6336_RESERVED_42,
	MT6336_RESERVED_43,
	MT6336_RG_AD_QI_VBAT_LT_V3P2_SEL,
	MT6336_RG_AD_QI_VBAT_LT_V2P7_SEL,
	MT6336_RG_AD_QI_OTG_BVALID_SEL,
	MT6336_RG_AD_QI_PP_EN_IN_SEL,
	MT6336_RG_AD_QI_ICL150PIN_LVL_SEL,
	MT6336_RG_AD_QI_TEMP_GT_150_SEL,
	MT6336_RG_AD_QI_VIO18_READY_SEL,
	MT6336_RG_AD_QI_VBGR_READY_SEL,
	MT6336_RG_AD_QI_VLED2_OPEN_SEL,
	MT6336_RG_AD_QI_VLED1_OPEN_SEL,
	MT6336_RG_AD_QI_VLED2_SHORT_SEL,
	MT6336_RG_AD_QI_VLED1_SHORT_SEL,
	MT6336_RG_AD_QI_VUSB33_READY_SEL,
	MT6336_RG_AD_QI_VPREG_RDY_SEL,
	MT6336_RG_AD_QI_VBUS_GT_POR_SEL,
	MT6336_RG_AD_QI_BC12_CMP_OUT_SW_SEL,
	MT6336_RG_AD_QI_BC12_CMP_OUT_SEL,
	MT6336_RG_AD_QI_MBATPP_DIS_OC_SEL,
	MT6336_RG_AD_NS_VPRECHG_FAIL_SEL,
	MT6336_RG_AD_NI_ICHR_LT_ITERM_SEL,
	MT6336_RG_AD_QI_VBAT_LT_PRECC1_SEL,
	MT6336_RG_AD_QI_VBAT_LT_PRECC0_SEL,
	MT6336_RG_AD_QI_MBATPP_DTEST2_SEL,
	MT6336_RG_AD_QI_MBATPP_DTEST1_SEL,
	MT6336_RG_AD_QI_OTG_VM_OVP_SEL,
	MT6336_RG_AD_QI_OTG_VM_UVLO_SEL,
	MT6336_RG_AD_QI_OTG_VBAT_UVLO_SEL,
	MT6336_RG_AD_NI_VBAT_OVP_SEL,
	MT6336_RG_AD_NI_VSYS_OVP_SEL,
	MT6336_RG_AD_NI_WEAKBUS_SEL,
	MT6336_RG_AD_QI_VBUS_UVLO_SEL,
	MT6336_RG_AD_QI_VBUS_OVP_SEL,
	MT6336_RG_AD_QI_PAM_MODE_SEL,
	MT6336_RG_AD_QI_CV_MODE_SEL,
	MT6336_RG_AD_QI_ICC_MODE_SEL,
	MT6336_RG_AD_QI_ICL_MODE_SEL,
	MT6336_RG_AD_QI_FLASH_VFLA_OVP_SEL,
	MT6336_RG_AD_QI_FLASH_VFLA_UVLO_SEL,
	MT6336_RG_AD_QI_SWCHR_OC_STATUS_SEL,
	MT6336_RG_AD_QI_BOOT_UVLO_SEL,
	MT6336_RG_AD_QI_ZX_SWCHR_ZCD_FLAG_SEL,
	MT6336_RG_AD_NI_ZX_OTG_TEST_SEL,
	MT6336_RG_AD_NI_ZX_FLASH_TEST_SEL,
	MT6336_RG_AD_NI_ZX_SWCHR_TEST_SEL,
	MT6336_RG_AD_QI_OTG_OLP_SEL,
	MT6336_RG_AD_QI_THR_MODE_SEL,
	MT6336_RG_AD_NI_FTR_SHOOT_DB_SEL,
	MT6336_RG_AD_NI_FTR_SHOOT_SEL,
	MT6336_RG_AD_NI_FTR_DROP_DB_SEL,
	MT6336_RG_AD_NI_FTR_DROP_SEL,
	MT6336_RG_AD_QI_VADC18_READY_SEL,
	MT6336_RG_AD_QI_SWCHR_ENPWM_STATUS_SEL,
	MT6336_RG_AD_QI_SWCHR_OCASYN_STATUS_SEL,
	MT6336_RG_DA_QI_TORCH_MODE_SEL,
	MT6336_RG_DA_QI_OTG_MODE_DB_SEL,
	MT6336_RG_DA_QI_FLASH_MODE_DB_SEL,
	MT6336_RG_DA_QI_BUCK_MODE_DB_SEL,
	MT6336_RG_DA_QI_BASE_READY_DB_SEL,
	MT6336_RG_DA_QI_BASE_READY_SEL,
	MT6336_RG_DA_QI_LOWQ_STAT_SEL,
	MT6336_RG_DA_QI_SHIP_STAT_SEL,
	MT6336_RG_DA_QI_TORCH_MODE_DRV_EN_SEL,
	MT6336_RG_DA_QI_TORCH_MODE_DB_SEL,
	MT6336_RG_DA_QI_BASE_CLK_TRIM_SEL,
	MT6336_RG_DA_QI_OSC_TRIM_SEL,
	MT6336_RG_DA_QI_CLKSQ_IN_SEL_SEL,
	MT6336_RG_DA_QI_CLKSQ_EN_SEL,
	MT6336_RG_DA_QI_EFUSE_READ_RDY_DLY_SEL,
	MT6336_RG_DA_QI_VBGR_READY_DB_SEL,
	MT6336_RG_DA_QI_BGR_SPEEDUP_EN_SEL,
	MT6336_RG_DA_QI_ILED1_ITH_SEL,
	MT6336_RG_DA_QI_ADC18_EN_SEL,
	MT6336_RG_DA_QI_ILED1_EN_SEL,
	MT6336_RG_DA_QI_ILED2_ITH_SEL,
	MT6336_RG_DA_QI_EN_ADCIN_VLED1_SEL,
	MT6336_RG_DA_QI_EN_ADCIN_VBATON_SEL,
	MT6336_RG_DA_QI_EN_ADCIN_VBUS_SEL,
	MT6336_RG_DA_QI_EN_ADCIN_VBATSNS_SEL,
	MT6336_RG_DA_QI_IOSDET2_EN_SEL,
	MT6336_RG_DA_QI_IOSDET1_EN_SEL,
	MT6336_RG_DA_QI_OSDET_EN_SEL,
	MT6336_RG_DA_QI_ILED2_EN_SEL,
	MT6336_RG_DA_QI_EN_ADCIN_VLED2_SEL,
	MT6336_RG_DA_QI_BC12_IPDC_EN_SEL,
	MT6336_RG_DA_QI_BC12_IPU_EN_SEL,
	MT6336_RG_DA_QI_BC12_IPD_EN_SEL,
	MT6336_RG_DA_QI_BC12_IBIAS_EN_SEL,
	MT6336_RG_DA_QI_BC12_BB_CTRL_SEL,
	MT6336_RG_DA_QI_BC12_IPD_HALF_EN_SEL,
	MT6336_RG_DA_QI_BC12_VREF_VTH_EN_SEL,
	MT6336_RG_DA_QI_BC12_CMP_EN_SEL,
	MT6336_RG_DA_QI_BC12_VSRC_EN_SEL,
	MT6336_RG_DA_QI_VPREG_RDY_DB_SEL,
	MT6336_RG_DA_QI_IPRECC1_ITH_SEL,
	MT6336_RG_DA_QI_EN_IBIASGEN_SEL,
	MT6336_RG_DA_NS_VPRECHG_FAIL_DB_SEL,
	MT6336_RG_DA_QI_EN_CP_HIQ_SEL,
	MT6336_RG_DA_QI_VBAT_LT_PRECC1_DB_SEL,
	MT6336_RG_DA_QI_VBAT_LT_PRECC0_DB_SEL,
	MT6336_RG_DA_QI_IPRECC1_EN_SEL,
	MT6336_RG_DA_QI_IPRECC0_EN_SEL,
	MT6336_RG_DA_QI_VBATFETON_VTH_SEL,
	MT6336_RG_DA_QI_BATOC_ANA_SEL_SEL,
	MT6336_RG_DA_QI_FETON_OK_LOQCP_EN_SEL,
	MT6336_RG_DA_QI_EN_VDC_IBIAS_SEL,
	MT6336_RG_DA_NI_CHRIND_EN_SEL,
	MT6336_RG_DA_NI_CHRIND_BIAS_EN_SEL,
	MT6336_RG_DA_QI_VSYS_IDIS_EN_SEL,
	MT6336_RG_DA_QI_VBAT_IDIS_EN_SEL,
	MT6336_RG_DA_QI_ITERM_ITH_SEL,
	MT6336_RG_DA_NI_CHRIND_STEP_SEL,
	MT6336_RG_DA_QI_CHR_DET_SEL,
	MT6336_RG_DA_QI_REVFET_EN_SEL,
	MT6336_RG_DA_QI_SSFNSH_SEL,
	MT6336_RG_DA_QI_IBACKBOOST_EN_SEL,
	MT6336_RG_DA_NI_WEAKBUS_DB_SEL,
	MT6336_RG_DA_QI_VBUS_OVP_DB_SEL,
	MT6336_RG_DA_QI_VBUS_UVLO_DB_SEL,
	MT6336_RG_DA_QI_ICL_ITH_SEL,
	MT6336_RG_DA_QI_CV_REGNODE_SEL_SEL,
	MT6336_RG_DA_QI_ICC_ITH_SEL,
	MT6336_RG_DA_QI_VSYSREG_VTH_SEL,
	MT6336_RG_DA_QI_VCV_VTH_SEL,
	MT6336_RG_DA_QI_OTG_VCV_VTH_SEL,
	MT6336_RG_DA_QI_FLASH_MODE_DRV_EN_SEL,
	MT6336_RG_DA_QI_OTG_MODE_SEL,
	MT6336_RG_DA_QI_OTG_MODE_DRV_EN_SEL,
	MT6336_RG_DA_NI_VBSTCHK_FEN_SEL,
	MT6336_RG_DA_QI_VFLA_VTH_SEL,
	MT6336_RG_DA_QI_FLASH_VFLA_OVP_DB_SEL,
	MT6336_RG_DA_QI_FLASH_VFLA_UVLO_DB_SEL,
	MT6336_RG_DA_QI_OTG_VM_UVLO_DB_SEL,
	MT6336_RG_DA_QI_OTG_VM_OVP_DB_SEL,
	MT6336_RG_DA_QI_OSC_REF_EN_SEL,
	MT6336_RG_DA_QI_BUCK_MODE_SEL,
	MT6336_RG_DA_QI_BUCK_MODE_DRV_EN_SEL,
	MT6336_RG_DA_QI_FLASH_MODE_SEL,
	MT6336_RG_DA_QI_MFLAPP_EN_SEL,
	MT6336_RG_DA_QI_OTG_IOLP_ITH_SEL,
	MT6336_RG_DA_QI_VSYS_REGV_SEL_SEL,
	MT6336_RG_DA_QI_OTG_VBAT_UVLO_DB_SEL,
	MT6336_RG_DA_NI_VBAT_OVP_DB_SEL,
	MT6336_RG_DA_NI_VSYS_OVP_DB_SEL,
	MT6336_RG_DA_QI_BACKGROUND_STAT_SEL,
	MT6336_RG_DA_QI_DEADBAT_STAT_SEL,
	MT6336_RG_DA_QI_EOC_STAT_SEL,
	MT6336_RG_DA_QI_FASTCC_STAT_SEL,
	MT6336_RG_DA_QI_POSTPRECC1_STAT_SEL,
	MT6336_RG_DA_QI_VBUS_PLUGIN_SEL,
	MT6336_RG_DA_QI_VBUS_IDIS_EN_SEL,
	MT6336_RG_DA_QI_MTORPP_EN_SEL,
	MT6336_RG_DA_QI_VPAM_VTH_SEL,
	MT6336_RG_AD_AUXADC_COMP_SEL,
	MT6336_RG_DA_AUXADC_DAC_0_SEL,
	MT6336_RG_DA_AUXADC_DAC_1_SEL,
	MT6336_RG_DA_AUXADC_SEL_SEL,
	MT6336_RG_DA_TS_VBE_SEL_SEL,
	MT6336_RG_DA_VBUF_EN_SEL,
	MT6336_RG_DA_AUXADC_RNG_SEL,
	MT6336_RG_DA_AUXADC_SPL_SEL,
	MT6336_RG_DA_AUXADC_ADC_PWDB_SEL,
	MT6336_RG_AD_PD_CC2_OVP_SEL,
	MT6336_RG_AD_PD_CC1_OVP_SEL,
	MT6336_RG_AD_PD_VCONN_UVP_SEL,
	MT6336_RG_AD_PD_RX_DATA_SEL,
	MT6336_RG_AD_CC_VUSB33_RDY_SEL,
	MT6336_RG_AD_CC_CMP_OUT_SEL,
	MT6336_RG_DA_CC_LPF_EN_SEL,
	MT6336_RG_DA_CC_BIAS_EN_SEL,
	MT6336_RG_DA_CC_RACC2_EN_SEL,
	MT6336_RG_DA_CC_RDCC2_EN_SEL,
	MT6336_RG_DA_CC_RPCC2_EN_SEL,
	MT6336_RG_DA_CC_RACC1_EN_SEL,
	MT6336_RG_DA_CC_RDCC1_EN_SEL,
	MT6336_RG_DA_CC_RPCC1_EN_SEL,
	MT6336_RG_DA_PD_TX_DATA_SEL,
	MT6336_RG_DA_PD_TX_EN_SEL,
	MT6336_RG_DA_PD_BIAS_EN_SEL,
	MT6336_RG_DA_CC_DAC_EN_SEL,
	MT6336_RG_DA_CC_SASW_EN_SEL,
	MT6336_RG_DA_CC_ADCSW_EN_SEL,
	MT6336_RG_DA_CC_SW_SEL_SEL,
	MT6336_RG_DA_CC_LEV_EN_SEL,
	MT6336_RG_DA_CC_DAC_IN_SEL,
	MT6336_RG_DA_PD_RX_EN_SEL,
	MT6336_RG_DA_CC_DAC_GAIN_CAL_SEL,
	MT6336_RG_DA_CC_DAC_CAL_SEL,
	MT6336_RG_DA_PD_CONNSW_SEL,
	MT6336_RG_DA_PD_CCSW_SEL,
	MT6336_RG_DA_NI_CCLK_SEL,
	MT6336_RG_DA_NI_BGR_TEST_CKIN_SEL,
	MT6336_RG_DA_CC_SACLK_SEL,
	MT6336_RG_AD_PD_SLEW_CK_SEL,
	MT6336_RG_AD_NS_CLK_26M_SEL,
	MT6336_RG_AD_NI_PMU_CLK75K_SEL,
	MT6336_RG_AD_NI_BASE_CLK_SEL,
	MT6336_RG_AD_NI_HF_CLK_SEL,
	MT6336_RG_DA_NI_HF_SSCLK_SEL,
	MT6336_GPIO_DIR0,
	MT6336_GPIO_DIR0_SET,
	MT6336_GPIO_DIR0_CLR,
	MT6336_GPIO_DIR1,
	MT6336_GPIO_DIR1_SET,
	MT6336_GPIO_DIR1_CLR,
	MT6336_GPIO_PULLEN0,
	MT6336_GPIO_PULLEN0_SET,
	MT6336_GPIO_PULLEN0_CLR,
	MT6336_GPIO_PULLEN1,
	MT6336_GPIO_PULLEN1_SET,
	MT6336_GPIO_PULLEN1_CLR,
	MT6336_GPIO_PULLSEL0,
	MT6336_GPIO_PULLSEL0_SET,
	MT6336_GPIO_PULLSEL0_CLR,
	MT6336_GPIO_PULLSEL1,
	MT6336_GPIO_PULLSEL1_SET,
	MT6336_GPIO_PULLSEL1_CLR,
	MT6336_GPIO_DINV0,
	MT6336_GPIO_DINV0_SET,
	MT6336_GPIO_DINV0_CLR,
	MT6336_GPIO_DINV1,
	MT6336_GPIO_DINV1_SET,
	MT6336_GPIO_DINV1_CLR,
	MT6336_GPIO_DOUT0,
	MT6336_GPIO_DOUT0_SET,
	MT6336_GPIO_DOUT0_CLR,
	MT6336_GPIO_DOUT1,
	MT6336_GPIO_DOUT1_SET,
	MT6336_GPIO_DOUT1_CLR,
	MT6336_GPIO_PI0,
	MT6336_GPIO_PI1,
	MT6336_GPIO_POE0,
	MT6336_GPIO_POE1,
	MT6336_GPIO0_MODE,
	MT6336_GPIO1_MODE,
	MT6336_GPIO_MODE0_SET,
	MT6336_GPIO_MODE0_CLR,
	MT6336_GPIO2_MODE,
	MT6336_GPIO3_MODE,
	MT6336_GPIO_MODE1_SET,
	MT6336_GPIO_MODE1_CLR,
	MT6336_GPIO4_MODE,
	MT6336_GPIO5_MODE,
	MT6336_GPIO_MODE2_SET,
	MT6336_GPIO_MODE2_CLR,
	MT6336_GPIO6_MODE,
	MT6336_GPIO7_MODE,
	MT6336_GPIO_MODE3_SET,
	MT6336_GPIO_MODE3_CLR,
	MT6336_GPIO8_MODE,
	MT6336_GPIO9_MODE,
	MT6336_GPIO_MODE4_SET,
	MT6336_GPIO_MODE4_CLR,
	MT6336_GPIO10_MODE,
	MT6336_GPIO11_MODE,
	MT6336_GPIO_MODE5_SET,
	MT6336_GPIO_MODE5_CLR,
	MT6336_GPIO12_MODE,
	MT6336_GPIO13_MODE,
	MT6336_GPIO_MODE6_SET,
	MT6336_GPIO_MODE6_CLR,
	MT6336_GPIO_RSV,
	MT6336_PMU_COMMAND_MAX
} MT6336_PMU_FLAGS_LIST_ENUM;

typedef struct {
	MT6336_PMU_FLAGS_LIST_ENUM flagname;
	unsigned short offset;
	unsigned char mask;
	unsigned char shift;
} MT6336_PMU_FLAG_TABLE_ENTRY;

#endif	/* _MT_PMIC_6336_UPMU_HW_H_ */
