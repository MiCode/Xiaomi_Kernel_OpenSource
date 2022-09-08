/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */

#ifndef _MT6357_H_
#define _MT6357_H_

/*************Register Bit Define*************/
#define TOP0_ANA_ID_ADDR                              \
	MT6357_TOP0_ID
#define TOP0_ANA_ID_SFT                               0
#define TOP0_ANA_ID_MASK                              0xFF
#define TOP0_ANA_ID_MASK_SFT                          (0xFF << 0)
#define AUXADC_RQST_CH0_ADDR                          \
	MT6357_AUXADC_RQST0
#define AUXADC_RQST_CH0_SFT                           0
#define AUXADC_RQST_CH0_MASK                          0x1
#define AUXADC_RQST_CH0_MASK_SFT                      (0x1 << 0)
#define AUXADC_ACCDET_ANASWCTRL_EN_ADDR               \
	MT6357_AUXADC_CON15
#define AUXADC_ACCDET_ANASWCTRL_EN_SFT                6
#define AUXADC_ACCDET_ANASWCTRL_EN_MASK               0x1
#define AUXADC_ACCDET_ANASWCTRL_EN_MASK_SFT           (0x1 << 6)

#define AUXADC_ACCDET_AUTO_SPL_ADDR                   \
	MT6357_AUXADC_ACCDET
#define AUXADC_ACCDET_AUTO_SPL_SFT                    0
#define AUXADC_ACCDET_AUTO_SPL_MASK                   0x1
#define AUXADC_ACCDET_AUTO_SPL_MASK_SFT               (0x1 << 0)
#define AUXADC_ACCDET_AUTO_RQST_CLR_ADDR              \
	MT6357_AUXADC_ACCDET
#define AUXADC_ACCDET_AUTO_RQST_CLR_SFT               1
#define AUXADC_ACCDET_AUTO_RQST_CLR_MASK              0x1
#define AUXADC_ACCDET_AUTO_RQST_CLR_MASK_SFT          (0x1 << 1)
#define AUXADC_ACCDET_DIG1_RSV0_ADDR                  \
	MT6357_AUXADC_ACCDET
#define AUXADC_ACCDET_DIG1_RSV0_SFT                   2
#define AUXADC_ACCDET_DIG1_RSV0_MASK                  0x3F
#define AUXADC_ACCDET_DIG1_RSV0_MASK_SFT              (0x3F << 2)
#define AUXADC_ACCDET_DIG0_RSV0_ADDR                  \
	MT6357_AUXADC_ACCDET
#define AUXADC_ACCDET_DIG0_RSV0_SFT                   8
#define AUXADC_ACCDET_DIG0_RSV0_MASK                  0xFF
#define AUXADC_ACCDET_DIG0_RSV0_MASK_SFT              (0xFF << 8)

#define RG_ACCDET_CK_PDN_ADDR                         \
	MT6357_AUD_TOP_CKPDN_CON0
#define RG_ACCDET_CK_PDN_SFT                          0
#define RG_ACCDET_CK_PDN_MASK                         0x1
#define RG_ACCDET_CK_PDN_MASK_SFT                     (0x1 << 0)
#define RG_ACCDET_RST_ADDR                            \
	MT6357_AUD_TOP_RST_CON0
#define RG_ACCDET_RST_SFT                             1
#define RG_ACCDET_RST_MASK                            0x1
#define RG_ACCDET_RST_MASK_SFT                        (0x1 << 1)
#define BANK_ACCDET_SWRST_ADDR                        \
	MT6357_AUD_TOP_RST_BANK_CON0
#define BANK_ACCDET_SWRST_SFT                         0
#define BANK_ACCDET_SWRST_MASK                        0x1
#define BANK_ACCDET_SWRST_MASK_SFT                    (0x1 << 0)
#define RG_INT_EN_ACCDET_ADDR                         \
	MT6357_AUD_TOP_INT_CON0
#define RG_INT_EN_ACCDET_SFT                          5
#define RG_INT_EN_ACCDET_MASK                         0x1
#define RG_INT_EN_ACCDET_MASK_SFT                     (0x1 << 5)
#define RG_INT_EN_ACCDET_EINT0_ADDR                   \
	MT6357_AUD_TOP_INT_CON0
#define RG_INT_EN_ACCDET_EINT0_SFT                    6
#define RG_INT_EN_ACCDET_EINT0_MASK                   0x1
#define RG_INT_EN_ACCDET_EINT0_MASK_SFT               (0x1 << 6)
#define RG_INT_EN_ACCDET_EINT1_ADDR                   \
	MT6357_AUD_TOP_INT_CON0
#define RG_INT_EN_ACCDET_EINT1_SFT                    7
#define RG_INT_EN_ACCDET_EINT1_MASK                   0x1
#define RG_INT_EN_ACCDET_EINT1_MASK_SFT               (0x1 << 7)
#define RG_INT_MASK_ACCDET_ADDR                       \
	MT6357_AUD_TOP_INT_MASK_CON0
#define RG_INT_MASK_ACCDET_SFT                        5
#define RG_INT_MASK_ACCDET_MASK                       0x1
#define RG_INT_MASK_ACCDET_MASK_SFT                   (0x1 << 5)
#define RG_INT_MASK_ACCDET_EINT0_ADDR                 \
	MT6357_AUD_TOP_INT_MASK_CON0
#define RG_INT_MASK_ACCDET_EINT0_SFT                  6
#define RG_INT_MASK_ACCDET_EINT0_MASK                 0x1
#define RG_INT_MASK_ACCDET_EINT0_MASK_SFT             (0x1 << 6)
#define RG_INT_MASK_ACCDET_EINT1_ADDR                 \
	MT6357_AUD_TOP_INT_MASK_CON0
#define RG_INT_MASK_ACCDET_EINT1_SFT                  7
#define RG_INT_MASK_ACCDET_EINT1_MASK                 0x1
#define RG_INT_MASK_ACCDET_EINT1_MASK_SFT             (0x1 << 7)
#define RG_INT_STATUS_ACCDET_ADDR                     \
	MT6357_AUD_TOP_INT_STATUS0
#define RG_INT_STATUS_ACCDET_SFT                      5
#define RG_INT_STATUS_ACCDET_MASK                     0x1
#define RG_INT_STATUS_ACCDET_MASK_SFT                 (0x1 << 5)
#define RG_INT_STATUS_ACCDET_EINT0_ADDR               \
	MT6357_AUD_TOP_INT_STATUS0
#define RG_INT_STATUS_ACCDET_EINT0_SFT                6
#define RG_INT_STATUS_ACCDET_EINT0_MASK               0x1
#define RG_INT_STATUS_ACCDET_EINT0_MASK_SFT           (0x1 << 6)
#define RG_INT_STATUS_ACCDET_EINT1_ADDR               \
	MT6357_AUD_TOP_INT_STATUS0
#define RG_INT_STATUS_ACCDET_EINT1_SFT                7
#define RG_INT_STATUS_ACCDET_EINT1_MASK               0x1
#define RG_INT_STATUS_ACCDET_EINT1_MASK_SFT           (0x1 << 7)
#define RG_INT_RAW_STATUS_ACCDET_ADDR                 \
	MT6357_AUD_TOP_INT_RAW_STATUS0
#define RG_INT_RAW_STATUS_ACCDET_SFT                  5
#define RG_INT_RAW_STATUS_ACCDET_MASK                 0x1
#define RG_INT_RAW_STATUS_ACCDET_MASK_SFT             (0x1 << 5)
#define RG_INT_RAW_STATUS_ACCDET_EINT0_ADDR           \
	MT6357_AUD_TOP_INT_RAW_STATUS0
#define RG_INT_RAW_STATUS_ACCDET_EINT0_SFT            6
#define RG_INT_RAW_STATUS_ACCDET_EINT0_MASK           0x1
#define RG_INT_RAW_STATUS_ACCDET_EINT0_MASK_SFT       (0x1 << 6)
#define RG_INT_RAW_STATUS_ACCDET_EINT1_ADDR           \
	MT6357_AUD_TOP_INT_RAW_STATUS0
#define RG_INT_RAW_STATUS_ACCDET_EINT1_SFT            7
#define RG_INT_RAW_STATUS_ACCDET_EINT1_MASK           0x1
#define RG_INT_RAW_STATUS_ACCDET_EINT1_MASK_SFT       (0x1 << 7)

#define RG_AUDPREAMPLON_ADDR                          \
	MT6357_AUDENC_ANA_CON0
#define RG_AUDPREAMPLON_SFT                           0
#define RG_AUDPREAMPLON_MASK                          0x1
#define RG_AUDPREAMPLON_MASK_SFT                      (0x1 << 0)
#define RG_CLKSQ_EN_ADDR                              \
	MT6357_AUDENC_ANA_CON6
#define RG_CLKSQ_EN_SFT                               0
#define RG_CLKSQ_EN_MASK                              0x1
#define RG_CLKSQ_EN_MASK_SFT                          (0x1 << 0)
#define RG_AUDSPARE_ADDR                              \
	MT6357_AUDENC_ANA_CON6
#define RG_AUDSPARE_SFT                               4
#define RG_AUDSPARE_MASK                              0xF
#define RG_AUDSPARE_MASK_SFT                          (0xF << 4)
#define RG_AUDPWDBMICBIAS0_ADDR                       \
	MT6357_AUDENC_ANA_CON8
#define RG_AUDPWDBMICBIAS0_SFT                        0
#define RG_AUDPWDBMICBIAS0_MASK                       0x1
#define RG_AUDPWDBMICBIAS0_MASK_SFT                   (0x1 << 0)
#define RG_AUDPWDBMICBIAS1_ADDR                       \
	MT6357_AUDENC_ANA_CON9
#define RG_AUDPWDBMICBIAS1_SFT                        0
#define RG_AUDPWDBMICBIAS1_MASK                       0x1
#define RG_AUDPWDBMICBIAS1_MASK_SFT                   (0x1 << 0)
#define RG_AUDMICBIAS1BYPASSEN_ADDR                   \
	MT6357_AUDENC_ANA_CON9
#define RG_AUDMICBIAS1BYPASSEN_SFT                    1
#define RG_AUDMICBIAS1BYPASSEN_MASK                   0x1
#define RG_AUDMICBIAS1BYPASSEN_MASK_SFT               (0x1 << 1)
#define RG_AUDMICBIAS1VREF_ADDR                       \
	MT6357_AUDENC_ANA_CON9
#define RG_AUDMICBIAS1VREF_SFT                        4
#define RG_AUDMICBIAS1VREF_MASK                       0x7
#define RG_AUDMICBIAS1VREF_MASK_SFT                   (0x7 << 4)
#define RG_AUDMICBIAS1DCSW1PEN_ADDR                   \
	MT6357_AUDENC_ANA_CON9
#define RG_AUDMICBIAS1DCSW1PEN_SFT                    8
#define RG_AUDMICBIAS1DCSW1PEN_MASK                   0x1
#define RG_AUDMICBIAS1DCSW1PEN_MASK_SFT               (0x1 << 8)
#define RG_AUDMICBIAS1DCSW1NEN_ADDR                   \
	MT6357_AUDENC_ANA_CON9
#define RG_AUDMICBIAS1DCSW1NEN_SFT                    9
#define RG_AUDMICBIAS1DCSW1NEN_MASK                   0x1
#define RG_AUDMICBIAS1DCSW1NEN_MASK_SFT               (0x1 << 9)
#define RG_BANDGAPGEN_ADDR                            \
	MT6357_AUDENC_ANA_CON9
#define RG_BANDGAPGEN_SFT                             12
#define RG_BANDGAPGEN_MASK                            0x1
#define RG_BANDGAPGEN_MASK_SFT                        (0x1 << 12)
#define RG_MTEST_EN_ADDR                              \
	MT6357_AUDENC_ANA_CON9
#define RG_MTEST_EN_SFT                               13
#define RG_MTEST_EN_MASK                              0x1
#define RG_MTEST_EN_MASK_SFT                          (0x1 << 13)
#define RG_MTEST_SEL_ADDR                             \
	MT6357_AUDENC_ANA_CON9
#define RG_MTEST_SEL_SFT                              14
#define RG_MTEST_SEL_MASK                             0x1
#define RG_MTEST_SEL_MASK_SFT                         (0x1 << 14)
#define RG_MTEST_CURRENT_ADDR                         \
	MT6357_AUDENC_ANA_CON9
#define RG_MTEST_CURRENT_SFT                          15
#define RG_MTEST_CURRENT_MASK                         0x1
#define RG_MTEST_CURRENT_MASK_SFT                     (0x1 << 15)
#define RG_AUDACCDETMICBIAS0PULLLOW_ADDR              \
	MT6357_AUDENC_ANA_CON10
#define RG_AUDACCDETMICBIAS0PULLLOW_SFT               0
#define RG_AUDACCDETMICBIAS0PULLLOW_MASK              0x1
#define RG_AUDACCDETMICBIAS0PULLLOW_MASK_SFT          (0x1 << 0)
#define RG_AUDACCDETMICBIAS1PULLLOW_ADDR              \
	MT6357_AUDENC_ANA_CON10
#define RG_AUDACCDETMICBIAS1PULLLOW_SFT               1
#define RG_AUDACCDETMICBIAS1PULLLOW_MASK              0x1
#define RG_AUDACCDETMICBIAS1PULLLOW_MASK_SFT          (0x1 << 1)
#define RG_AUDACCDETVIN1PULLLOW_ADDR                  \
	MT6357_AUDENC_ANA_CON10
#define RG_AUDACCDETVIN1PULLLOW_SFT                   2
#define RG_AUDACCDETVIN1PULLLOW_MASK                  0x1
#define RG_AUDACCDETVIN1PULLLOW_MASK_SFT              (0x1 << 2)
#define RG_AUDACCDETVTHACAL_ADDR                      \
	MT6357_AUDENC_ANA_CON10
#define RG_AUDACCDETVTHACAL_SFT                       4
#define RG_AUDACCDETVTHACAL_MASK                      0x1
#define RG_AUDACCDETVTHACAL_MASK_SFT                  (0x1 << 4)
#define RG_AUDACCDETVTHBCAL_ADDR                      \
	MT6357_AUDENC_ANA_CON10
#define RG_AUDACCDETVTHBCAL_SFT                       5
#define RG_AUDACCDETVTHBCAL_MASK                      0x1
#define RG_AUDACCDETVTHBCAL_MASK_SFT                  (0x1 << 5)
#define RG_AUDACCDETTVDET_ADDR                        \
	MT6357_AUDENC_ANA_CON10
#define RG_AUDACCDETTVDET_SFT                         6
#define RG_AUDACCDETTVDET_MASK                        0x1
#define RG_AUDACCDETTVDET_MASK_SFT                    (0x1 << 6)
#define RG_ACCDETSEL_ADDR                             \
	MT6357_AUDENC_ANA_CON10
#define RG_ACCDETSEL_SFT                              7
#define RG_ACCDETSEL_MASK                             0x1
#define RG_ACCDETSEL_MASK_SFT                         (0x1 << 7)
#define RG_SWBUFMODSEL_ADDR                           \
	MT6357_AUDENC_ANA_CON10
#define RG_SWBUFMODSEL_SFT                            8
#define RG_SWBUFMODSEL_MASK                           0x1
#define RG_SWBUFMODSEL_MASK_SFT                       (0x1 << 8)
#define RG_SWBUFSWEN_ADDR                             \
	MT6357_AUDENC_ANA_CON10
#define RG_SWBUFSWEN_SFT                              9
#define RG_SWBUFSWEN_MASK                             0x1
#define RG_SWBUFSWEN_MASK_SFT                         (0x1 << 9)
#define RG_EINTCOMPVTH_ADDR                           \
	MT6357_AUDENC_ANA_CON10
#define RG_EINTCOMPVTH_SFT                            10
#define RG_EINTCOMPVTH_MASK                           0x1
#define RG_EINTCOMPVTH_MASK_SFT                       (0x1 << 10)
#define RG_EINTCONFIGACCDET_ADDR                      \
	MT6357_AUDENC_ANA_CON10
#define RG_EINTCONFIGACCDET_SFT                       11
#define RG_EINTCONFIGACCDET_MASK                      0x1
#define RG_EINTCONFIGACCDET_MASK_SFT                  (0x1 << 11)
#define RG_EINTHIRENB_ADDR                            \
	MT6357_AUDENC_ANA_CON10
#define RG_EINTHIRENB_SFT                             12
#define RG_EINTHIRENB_MASK                            0x1
#define RG_EINTHIRENB_MASK_SFT                        (0x1 << 12)
#define RG_ACCDET2AUXRESBYPASS_ADDR                   \
	MT6357_AUDENC_ANA_CON10
#define RG_ACCDET2AUXRESBYPASS_SFT                    13
#define RG_ACCDET2AUXRESBYPASS_MASK                   0x1
#define RG_ACCDET2AUXRESBYPASS_MASK_SFT               (0x1 << 13)
#define RG_ACCDET2AUXBUFFERBYPASS_ADDR                \
	MT6357_AUDENC_ANA_CON10
#define RG_ACCDET2AUXBUFFERBYPASS_SFT                 14
#define RG_ACCDET2AUXBUFFERBYPASS_MASK                0x1
#define RG_ACCDET2AUXBUFFERBYPASS_MASK_SFT            (0x1 << 14)
#define RG_ACCDET2AUXSWEN_ADDR                        \
	MT6357_AUDENC_ANA_CON10
#define RG_ACCDET2AUXSWEN_SFT                         15
#define RG_ACCDET2AUXSWEN_MASK                        0x1
#define RG_ACCDET2AUXSWEN_MASK_SFT                    (0x1 << 15)

#define ACCDET_ANA_ID_ADDR                            \
	MT6357_ACCDET_DSN_DIG_ID
#define ACCDET_ANA_ID_SFT                             0
#define ACCDET_ANA_ID_MASK                            0xFF
#define ACCDET_ANA_ID_MASK_SFT                        (0xFF << 0)
#define ACCDET_DIG_ID_ADDR                            \
	MT6357_ACCDET_DSN_DIG_ID
#define ACCDET_DIG_ID_SFT                             8
#define ACCDET_DIG_ID_MASK                            0xFF
#define ACCDET_DIG_ID_MASK_SFT                        (0xFF << 8)
#define ACCDET_ANA_MINOR_REV_ADDR                     \
	MT6357_ACCDET_DSN_DIG_REV0
#define ACCDET_ANA_MINOR_REV_SFT                      0
#define ACCDET_ANA_MINOR_REV_MASK                     0xF
#define ACCDET_ANA_MINOR_REV_MASK_SFT                 (0xF << 0)
#define ACCDET_ANA_MAJOR_REV_ADDR                     \
	MT6357_ACCDET_DSN_DIG_REV0
#define ACCDET_ANA_MAJOR_REV_SFT                      4
#define ACCDET_ANA_MAJOR_REV_MASK                     0xF
#define ACCDET_ANA_MAJOR_REV_MASK_SFT                 (0xF << 4)
#define ACCDET_DIG_MINOR_REV_ADDR                     \
	MT6357_ACCDET_DSN_DIG_REV0
#define ACCDET_DIG_MINOR_REV_SFT                      8
#define ACCDET_DIG_MINOR_REV_MASK                     0xF
#define ACCDET_DIG_MINOR_REV_MASK_SFT                 (0xF << 8)
#define ACCDET_DIG_MAJOR_REV_ADDR                     \
	MT6357_ACCDET_DSN_DIG_REV0
#define ACCDET_DIG_MAJOR_REV_SFT                      12
#define ACCDET_DIG_MAJOR_REV_MASK                     0xF
#define ACCDET_DIG_MAJOR_REV_MASK_SFT                 (0xF << 12)
#define ACCDET_DSN_CBS_ADDR                           \
	MT6357_ACCDET_DSN_DBI
#define ACCDET_DSN_CBS_SFT                            0
#define ACCDET_DSN_CBS_MASK                           0x3
#define ACCDET_DSN_CBS_MASK_SFT                       (0x3 << 0)
#define ACCDET_DSN_BIX_ADDR                           \
	MT6357_ACCDET_DSN_DBI
#define ACCDET_DSN_BIX_SFT                            2
#define ACCDET_DSN_BIX_MASK                           0x3
#define ACCDET_DSN_BIX_MASK_SFT                       (0x3 << 2)
#define ACCDET_ESP_ADDR                               \
	MT6357_ACCDET_DSN_DBI
#define ACCDET_ESP_SFT                                8
#define ACCDET_ESP_MASK                               0xFF
#define ACCDET_ESP_MASK_SFT                           (0xFF << 8)
#define ACCDET_DSN_FPI_ADDR                           \
	MT6357_ACCDET_DSN_FPI
#define ACCDET_DSN_FPI_SFT                            0
#define ACCDET_DSN_FPI_MASK                           0xFF
#define ACCDET_DSN_FPI_MASK_SFT                       (0xFF << 0)
#define AUDACCDETAUXADCSWCTRL_ADDR                    \
	MT6357_ACCDET_CON0
#define AUDACCDETAUXADCSWCTRL_SFT                     10
#define AUDACCDETAUXADCSWCTRL_MASK                    0x1
#define AUDACCDETAUXADCSWCTRL_MASK_SFT                (0x1 << 10)
#define AUDACCDETAUXADCSWCTRL_SEL_ADDR                \
	MT6357_ACCDET_CON0
#define AUDACCDETAUXADCSWCTRL_SEL_SFT                 11
#define AUDACCDETAUXADCSWCTRL_SEL_MASK                0x1
#define AUDACCDETAUXADCSWCTRL_SEL_MASK_SFT            (0x1 << 11)
#define RG_AUDACCDETRSV_ADDR                          \
	MT6357_ACCDET_CON0
#define RG_AUDACCDETRSV_SFT                           13
#define RG_AUDACCDETRSV_MASK                          0x3
#define RG_AUDACCDETRSV_MASK_SFT                      (0x3 << 13)
#define ACCDET_EN_ADDR                                \
	MT6357_ACCDET_CON1
#define ACCDET_EN_SFT                                 0
#define ACCDET_EN_MASK                                0x1
#define ACCDET_EN_MASK_SFT                            (0x1 << 0)
#define ACCDET_SEQ_INIT_ADDR                          \
	MT6357_ACCDET_CON1
#define ACCDET_SEQ_INIT_SFT                           1
#define ACCDET_SEQ_INIT_MASK                          0x1
#define ACCDET_SEQ_INIT_MASK_SFT                      (0x1 << 1)
#define ACCDET_EINT0_EN_ADDR                          \
	MT6357_ACCDET_CON1
#define ACCDET_EINT0_EN_SFT                           2
#define ACCDET_EINT0_EN_MASK                          0x1
#define ACCDET_EINT0_EN_MASK_SFT                      (0x1 << 2)
#define ACCDET_EINT0_SEQ_INIT_ADDR                    \
	MT6357_ACCDET_CON1
#define ACCDET_EINT0_SEQ_INIT_SFT                     3
#define ACCDET_EINT0_SEQ_INIT_MASK                    0x1
#define ACCDET_EINT0_SEQ_INIT_MASK_SFT                (0x1 << 3)
#define ACCDET_EINT1_EN_ADDR                          \
	MT6357_ACCDET_CON1
#define ACCDET_EINT1_EN_SFT                           4
#define ACCDET_EINT1_EN_MASK                          0x1
#define ACCDET_EINT1_EN_MASK_SFT                      (0x1 << 4)
#define ACCDET_EINT1_SEQ_INIT_ADDR                    \
	MT6357_ACCDET_CON1
#define ACCDET_EINT1_SEQ_INIT_SFT                     5
#define ACCDET_EINT1_SEQ_INIT_MASK                    0x1
#define ACCDET_EINT1_SEQ_INIT_MASK_SFT                (0x1 << 5)
#define ACCDET_ANASWCTRL_SEL_ADDR                     \
	MT6357_ACCDET_CON1
#define ACCDET_ANASWCTRL_SEL_SFT                      6
#define ACCDET_ANASWCTRL_SEL_MASK                     0x1
#define ACCDET_ANASWCTRL_SEL_MASK_SFT                 (0x1 << 6)
#define ACCDET_CMP_PWM_EN_ADDR                        \
	MT6357_ACCDET_CON2
#define ACCDET_CMP_PWM_EN_SFT                         0
#define ACCDET_CMP_PWM_EN_MASK                        0x1
#define ACCDET_CMP_PWM_EN_MASK_SFT                    (0x1 << 0)
#define ACCDET_VTH_PWM_EN_ADDR                        \
	MT6357_ACCDET_CON2
#define ACCDET_VTH_PWM_EN_SFT                         1
#define ACCDET_VTH_PWM_EN_MASK                        0x1
#define ACCDET_VTH_PWM_EN_MASK_SFT                    (0x1 << 1)
#define ACCDET_MBIAS_PWM_EN_ADDR                      \
	MT6357_ACCDET_CON2
#define ACCDET_MBIAS_PWM_EN_SFT                       2
#define ACCDET_MBIAS_PWM_EN_MASK                      0x1
#define ACCDET_MBIAS_PWM_EN_MASK_SFT                  (0x1 << 2)
#define ACCDET_EINT0_PWM_EN_ADDR                      \
	MT6357_ACCDET_CON2
#define ACCDET_EINT0_PWM_EN_SFT                       3
#define ACCDET_EINT0_PWM_EN_MASK                      0x1
#define ACCDET_EINT0_PWM_EN_MASK_SFT                  (0x1 << 3)
#define ACCDET_EINT1_PWM_EN_ADDR                      \
	MT6357_ACCDET_CON2
#define ACCDET_EINT1_PWM_EN_SFT                       4
#define ACCDET_EINT1_PWM_EN_MASK                      0x1
#define ACCDET_EINT1_PWM_EN_MASK_SFT                  (0x1 << 4)
#define ACCDET_CMP_PWM_IDLE_ADDR                      \
	MT6357_ACCDET_CON2
#define ACCDET_CMP_PWM_IDLE_SFT                       8
#define ACCDET_CMP_PWM_IDLE_MASK                      0x1
#define ACCDET_CMP_PWM_IDLE_MASK_SFT                  (0x1 << 8)
#define ACCDET_VTH_PWM_IDLE_ADDR                      \
	MT6357_ACCDET_CON2
#define ACCDET_VTH_PWM_IDLE_SFT                       9
#define ACCDET_VTH_PWM_IDLE_MASK                      0x1
#define ACCDET_VTH_PWM_IDLE_MASK_SFT                  (0x1 << 9)
#define ACCDET_MBIAS_PWM_IDLE_ADDR                    \
	MT6357_ACCDET_CON2
#define ACCDET_MBIAS_PWM_IDLE_SFT                     10
#define ACCDET_MBIAS_PWM_IDLE_MASK                    0x1
#define ACCDET_MBIAS_PWM_IDLE_MASK_SFT                (0x1 << 10)
#define ACCDET_EINT0_PWM_IDLE_ADDR                    \
	MT6357_ACCDET_CON2
#define ACCDET_EINT0_PWM_IDLE_SFT                     11
#define ACCDET_EINT0_PWM_IDLE_MASK                    0x1
#define ACCDET_EINT0_PWM_IDLE_MASK_SFT                (0x1 << 11)
#define ACCDET_EINT1_PWM_IDLE_ADDR                    \
	MT6357_ACCDET_CON2
#define ACCDET_EINT1_PWM_IDLE_SFT                     12
#define ACCDET_EINT1_PWM_IDLE_MASK                    0x1
#define ACCDET_EINT1_PWM_IDLE_MASK_SFT                (0x1 << 12)
#define ACCDET_PWM_WIDTH_ADDR                         \
	MT6357_ACCDET_CON3
#define ACCDET_PWM_WIDTH_SFT                          0
#define ACCDET_PWM_WIDTH_MASK                         0xFFFF
#define ACCDET_PWM_WIDTH_MASK_SFT                     (0xFFFF << 0)
#define ACCDET_PWM_THRESH_ADDR                        \
	MT6357_ACCDET_CON4
#define ACCDET_PWM_THRESH_SFT                         0
#define ACCDET_PWM_THRESH_MASK                        0xFFFF
#define ACCDET_PWM_THRESH_MASK_SFT                    (0xFFFF << 0)
#define ACCDET_RISE_DELAY_ADDR                        \
	MT6357_ACCDET_CON5
#define ACCDET_RISE_DELAY_SFT                         0
#define ACCDET_RISE_DELAY_MASK                        0x7FFF
#define ACCDET_RISE_DELAY_MASK_SFT                    (0x7FFF << 0)
#define ACCDET_FALL_DELAY_ADDR                        \
	MT6357_ACCDET_CON5
#define ACCDET_FALL_DELAY_SFT                         15
#define ACCDET_FALL_DELAY_MASK                        0x1
#define ACCDET_FALL_DELAY_MASK_SFT                    (0x1 << 15)
#define ACCDET_DEBOUNCE0_ADDR                         \
	MT6357_ACCDET_CON6
#define ACCDET_DEBOUNCE0_SFT                          0
#define ACCDET_DEBOUNCE0_MASK                         0xFFFF
#define ACCDET_DEBOUNCE0_MASK_SFT                     (0xFFFF << 0)
#define ACCDET_DEBOUNCE1_ADDR                         \
	MT6357_ACCDET_CON7
#define ACCDET_DEBOUNCE1_SFT                          0
#define ACCDET_DEBOUNCE1_MASK                         0xFFFF
#define ACCDET_DEBOUNCE1_MASK_SFT                     (0xFFFF << 0)
#define ACCDET_DEBOUNCE2_ADDR                         \
	MT6357_ACCDET_CON8
#define ACCDET_DEBOUNCE2_SFT                          0
#define ACCDET_DEBOUNCE2_MASK                         0xFFFF
#define ACCDET_DEBOUNCE2_MASK_SFT                     (0xFFFF << 0)
#define ACCDET_DEBOUNCE3_ADDR                         \
	MT6357_ACCDET_CON9
#define ACCDET_DEBOUNCE3_SFT                          0
#define ACCDET_DEBOUNCE3_MASK                         0xFFFF
#define ACCDET_DEBOUNCE3_MASK_SFT                     (0xFFFF << 0)
#define ACCDET_DEBOUNCE4_ADDR                         \
	MT6357_ACCDET_CON10
#define ACCDET_DEBOUNCE4_SFT                          0
#define ACCDET_DEBOUNCE4_MASK                         0xFFFF
#define ACCDET_DEBOUNCE4_MASK_SFT                     (0xFFFF << 0)
#define ACCDET_IVAL_CUR_IN_ADDR                       \
	MT6357_ACCDET_CON11
#define ACCDET_IVAL_CUR_IN_SFT                        0
#define ACCDET_IVAL_CUR_IN_MASK                       0x3
#define ACCDET_IVAL_CUR_IN_MASK_SFT                   (0x3 << 0)
#define ACCDET_EINT0_IVAL_CUR_IN_ADDR                 \
	MT6357_ACCDET_CON11
#define ACCDET_EINT0_IVAL_CUR_IN_SFT                  2
#define ACCDET_EINT0_IVAL_CUR_IN_MASK                 0x1
#define ACCDET_EINT0_IVAL_CUR_IN_MASK_SFT             (0x1 << 2)
#define ACCDET_EINT1_IVAL_CUR_IN_ADDR                 \
	MT6357_ACCDET_CON11
#define ACCDET_EINT1_IVAL_CUR_IN_SFT                  3
#define ACCDET_EINT1_IVAL_CUR_IN_MASK                 0x1
#define ACCDET_EINT1_IVAL_CUR_IN_MASK_SFT             (0x1 << 3)
#define ACCDET_IVAL_SAM_IN_ADDR                       \
	MT6357_ACCDET_CON11
#define ACCDET_IVAL_SAM_IN_SFT                        4
#define ACCDET_IVAL_SAM_IN_MASK                       0x3
#define ACCDET_IVAL_SAM_IN_MASK_SFT                   (0x3 << 4)
#define ACCDET_EINT0_IVAL_SAM_IN_ADDR                 \
	MT6357_ACCDET_CON11
#define ACCDET_EINT0_IVAL_SAM_IN_SFT                  6
#define ACCDET_EINT0_IVAL_SAM_IN_MASK                 0x1
#define ACCDET_EINT0_IVAL_SAM_IN_MASK_SFT             (0x1 << 6)
#define ACCDET_EINT1_IVAL_SAM_IN_ADDR                 \
	MT6357_ACCDET_CON11
#define ACCDET_EINT1_IVAL_SAM_IN_SFT                  7
#define ACCDET_EINT1_IVAL_SAM_IN_MASK                 0x1
#define ACCDET_EINT1_IVAL_SAM_IN_MASK_SFT             (0x1 << 7)
#define ACCDET_IVAL_MEM_IN_ADDR                       \
	MT6357_ACCDET_CON11
#define ACCDET_IVAL_MEM_IN_SFT                        8
#define ACCDET_IVAL_MEM_IN_MASK                       0x3
#define ACCDET_IVAL_MEM_IN_MASK_SFT                   (0x3 << 8)
#define ACCDET_EINT0_IVAL_MEM_IN_ADDR                 \
	MT6357_ACCDET_CON11
#define ACCDET_EINT0_IVAL_MEM_IN_SFT                  10
#define ACCDET_EINT0_IVAL_MEM_IN_MASK                 0x1
#define ACCDET_EINT0_IVAL_MEM_IN_MASK_SFT             (0x1 << 10)
#define ACCDET_EINT1_IVAL_MEM_IN_ADDR                 \
	MT6357_ACCDET_CON11
#define ACCDET_EINT1_IVAL_MEM_IN_SFT                  11
#define ACCDET_EINT1_IVAL_MEM_IN_MASK                 0x1
#define ACCDET_EINT1_IVAL_MEM_IN_MASK_SFT             (0x1 << 11)
#define ACCDET_IVAL_SEL_ADDR                          \
	MT6357_ACCDET_CON11
#define ACCDET_IVAL_SEL_SFT                           13
#define ACCDET_IVAL_SEL_MASK                          0x1
#define ACCDET_IVAL_SEL_MASK_SFT                      (0x1 << 13)
#define ACCDET_EINT0_IVAL_SEL_ADDR                    \
	MT6357_ACCDET_CON11
#define ACCDET_EINT0_IVAL_SEL_SFT                     14
#define ACCDET_EINT0_IVAL_SEL_MASK                    0x1
#define ACCDET_EINT0_IVAL_SEL_MASK_SFT                (0x1 << 14)
#define ACCDET_EINT1_IVAL_SEL_ADDR                    \
	MT6357_ACCDET_CON11
#define ACCDET_EINT1_IVAL_SEL_SFT                     15
#define ACCDET_EINT1_IVAL_SEL_MASK                    0x1
#define ACCDET_EINT1_IVAL_SEL_MASK_SFT                (0x1 << 15)
#define ACCDET_IRQ_ADDR                               \
	MT6357_ACCDET_CON12
#define ACCDET_IRQ_SFT                                0
#define ACCDET_IRQ_MASK                               0x1
#define ACCDET_IRQ_MASK_SFT                           (0x1 << 0)
#define ACCDET_EINT0_IRQ_ADDR                         \
	MT6357_ACCDET_CON12
#define ACCDET_EINT0_IRQ_SFT                          2
#define ACCDET_EINT0_IRQ_MASK                         0x1
#define ACCDET_EINT0_IRQ_MASK_SFT                     (0x1 << 2)
#define ACCDET_EINT1_IRQ_ADDR                         \
	MT6357_ACCDET_CON12
#define ACCDET_EINT1_IRQ_SFT                          3
#define ACCDET_EINT1_IRQ_MASK                         0x1
#define ACCDET_EINT1_IRQ_MASK_SFT                     (0x1 << 3)
#define ACCDET_IRQ_CLR_ADDR                           \
	MT6357_ACCDET_CON12
#define ACCDET_IRQ_CLR_SFT                            8
#define ACCDET_IRQ_CLR_MASK                           0x1
#define ACCDET_IRQ_CLR_MASK_SFT                       (0x1 << 8)
#define ACCDET_EINT0_IRQ_CLR_ADDR                     \
	MT6357_ACCDET_CON12
#define ACCDET_EINT0_IRQ_CLR_SFT                      10
#define ACCDET_EINT0_IRQ_CLR_MASK                     0x1
#define ACCDET_EINT0_IRQ_CLR_MASK_SFT                 (0x1 << 10)
#define ACCDET_EINT1_IRQ_CLR_ADDR                     \
	MT6357_ACCDET_CON12
#define ACCDET_EINT1_IRQ_CLR_SFT                      11
#define ACCDET_EINT1_IRQ_CLR_MASK                     0x1
#define ACCDET_EINT1_IRQ_CLR_MASK_SFT                 (0x1 << 11)
#define ACCDET_EINT0_IRQ_POLARITY_ADDR                \
	MT6357_ACCDET_CON12
#define ACCDET_EINT0_IRQ_POLARITY_SFT                 14
#define ACCDET_EINT0_IRQ_POLARITY_MASK                0x1
#define ACCDET_EINT0_IRQ_POLARITY_MASK_SFT            (0x1 << 14)
#define ACCDET_EINT1_IRQ_POLARITY_ADDR                \
	MT6357_ACCDET_CON12
#define ACCDET_EINT1_IRQ_POLARITY_SFT                 15
#define ACCDET_EINT1_IRQ_POLARITY_MASK                0x1
#define ACCDET_EINT1_IRQ_POLARITY_MASK_SFT            (0x1 << 15)
#define ACCDET_TEST_MODE0_ADDR                        \
	MT6357_ACCDET_CON13
#define ACCDET_TEST_MODE0_SFT                         0
#define ACCDET_TEST_MODE0_MASK                        0x1
#define ACCDET_TEST_MODE0_MASK_SFT                    (0x1 << 0)
#define ACCDET_CMP_SWSEL_ADDR                         \
	MT6357_ACCDET_CON13
#define ACCDET_CMP_SWSEL_SFT                          1
#define ACCDET_CMP_SWSEL_MASK                         0x1
#define ACCDET_CMP_SWSEL_MASK_SFT                     (0x1 << 1)
#define ACCDET_VTH_SWSEL_ADDR                         \
	MT6357_ACCDET_CON13
#define ACCDET_VTH_SWSEL_SFT                          2
#define ACCDET_VTH_SWSEL_MASK                         0x1
#define ACCDET_VTH_SWSEL_MASK_SFT                     (0x1 << 2)
#define ACCDET_MBIAS_SWSEL_ADDR                       \
	MT6357_ACCDET_CON13
#define ACCDET_MBIAS_SWSEL_SFT                        3
#define ACCDET_MBIAS_SWSEL_MASK                       0x1
#define ACCDET_MBIAS_SWSEL_MASK_SFT                   (0x1 << 3)
#define ACCDET_TEST_MODE4_ADDR                        \
	MT6357_ACCDET_CON13
#define ACCDET_TEST_MODE4_SFT                         4
#define ACCDET_TEST_MODE4_MASK                        0x1
#define ACCDET_TEST_MODE4_MASK_SFT                    (0x1 << 4)
#define ACCDET_TEST_MODE5_ADDR                        \
	MT6357_ACCDET_CON13
#define ACCDET_TEST_MODE5_SFT                         5
#define ACCDET_TEST_MODE5_MASK                        0x1
#define ACCDET_TEST_MODE5_MASK_SFT                    (0x1 << 5)
#define ACCDET_PWM_SEL_ADDR                           \
	MT6357_ACCDET_CON13
#define ACCDET_PWM_SEL_SFT                            6
#define ACCDET_PWM_SEL_MASK                           0x3
#define ACCDET_PWM_SEL_MASK_SFT                       (0x3 << 6)
#define ACCDET_IN_SW_ADDR                             \
	MT6357_ACCDET_CON13
#define ACCDET_IN_SW_SFT                              8
#define ACCDET_IN_SW_MASK                             0x3
#define ACCDET_IN_SW_MASK_SFT                         (0x3 << 8)
#define ACCDET_CMP_EN_SW_ADDR                         \
	MT6357_ACCDET_CON13
#define ACCDET_CMP_EN_SW_SFT                          12
#define ACCDET_CMP_EN_SW_MASK                         0x1
#define ACCDET_CMP_EN_SW_MASK_SFT                     (0x1 << 12)
#define ACCDET_VTH_EN_SW_ADDR                         \
	MT6357_ACCDET_CON13
#define ACCDET_VTH_EN_SW_SFT                          13
#define ACCDET_VTH_EN_SW_MASK                         0x1
#define ACCDET_VTH_EN_SW_MASK_SFT                     (0x1 << 13)
#define ACCDET_MBIAS_EN_SW_ADDR                       \
	MT6357_ACCDET_CON13
#define ACCDET_MBIAS_EN_SW_SFT                        14
#define ACCDET_MBIAS_EN_SW_MASK                       0x1
#define ACCDET_MBIAS_EN_SW_MASK_SFT                   (0x1 << 14)
#define ACCDET_PWM_EN_SW_ADDR                         \
	MT6357_ACCDET_CON13
#define ACCDET_PWM_EN_SW_SFT                          15
#define ACCDET_PWM_EN_SW_MASK                         0x1
#define ACCDET_PWM_EN_SW_MASK_SFT                     (0x1 << 15)
#define ACCDET_IN_ADDR                                \
	MT6357_ACCDET_CON14
#define ACCDET_IN_SFT                                 0
#define ACCDET_IN_MASK                                0x3
#define ACCDET_IN_MASK_SFT                            (0x3 << 0)
#define ACCDET_CUR_IN_ADDR                            \
	MT6357_ACCDET_CON14
#define ACCDET_CUR_IN_SFT                             2
#define ACCDET_CUR_IN_MASK                            0x3
#define ACCDET_CUR_IN_MASK_SFT                        (0x3 << 2)
#define ACCDET_SAM_IN_ADDR                            \
	MT6357_ACCDET_CON14
#define ACCDET_SAM_IN_SFT                             4
#define ACCDET_SAM_IN_MASK                            0x3
#define ACCDET_SAM_IN_MASK_SFT                        (0x3 << 4)
#define ACCDET_MEM_IN_ADDR                            \
	MT6357_ACCDET_CON14
#define ACCDET_MEM_IN_SFT                             6
#define ACCDET_MEM_IN_MASK                            0x3
#define ACCDET_MEM_IN_MASK_SFT                        (0x3 << 6)
#define ACCDET_STATE_ADDR                             \
	MT6357_ACCDET_CON14
#define ACCDET_STATE_SFT                              8
#define ACCDET_STATE_MASK                             0x7
#define ACCDET_STATE_MASK_SFT                         (0x7 << 8)
#define ACCDET_MBIAS_CLK_ADDR                         \
	MT6357_ACCDET_CON14
#define ACCDET_MBIAS_CLK_SFT                          12
#define ACCDET_MBIAS_CLK_MASK                         0x1
#define ACCDET_MBIAS_CLK_MASK_SFT                     (0x1 << 12)
#define ACCDET_VTH_CLK_ADDR                           \
	MT6357_ACCDET_CON14
#define ACCDET_VTH_CLK_SFT                            13
#define ACCDET_VTH_CLK_MASK                           0x1
#define ACCDET_VTH_CLK_MASK_SFT                       (0x1 << 13)
#define ACCDET_CMP_CLK_ADDR                           \
	MT6357_ACCDET_CON14
#define ACCDET_CMP_CLK_SFT                            14
#define ACCDET_CMP_CLK_MASK                           0x1
#define ACCDET_CMP_CLK_MASK_SFT                       (0x1 << 14)
#define DA_AUDACCDETAUXADCSWCTRL_ADDR                 \
	MT6357_ACCDET_CON14
#define DA_AUDACCDETAUXADCSWCTRL_SFT                  15
#define DA_AUDACCDETAUXADCSWCTRL_MASK                 0x1
#define DA_AUDACCDETAUXADCSWCTRL_MASK_SFT             (0x1 << 15)
#define ACCDET_EINT0_DEB_SEL_ADDR                     \
	MT6357_ACCDET_CON15
#define ACCDET_EINT0_DEB_SEL_SFT                      0
#define ACCDET_EINT0_DEB_SEL_MASK                     0x1
#define ACCDET_EINT0_DEB_SEL_MASK_SFT                 (0x1 << 0)
#define ACCDET_EINT0_DEBOUNCE_ADDR                    \
	MT6357_ACCDET_CON15
#define ACCDET_EINT0_DEBOUNCE_SFT                     3
#define ACCDET_EINT0_DEBOUNCE_MASK                    0xF
#define ACCDET_EINT0_DEBOUNCE_MASK_SFT                (0xF << 3)
#define ACCDET_EINT0_PWM_THRESH_ADDR                  \
	MT6357_ACCDET_CON15
#define ACCDET_EINT0_PWM_THRESH_SFT                   8
#define ACCDET_EINT0_PWM_THRESH_MASK                  0x7
#define ACCDET_EINT0_PWM_THRESH_MASK_SFT              (0x7 << 8)
#define ACCDET_EINT0_PWM_WIDTH_ADDR                   \
	MT6357_ACCDET_CON15
#define ACCDET_EINT0_PWM_WIDTH_SFT                    12
#define ACCDET_EINT0_PWM_WIDTH_MASK                   0x3
#define ACCDET_EINT0_PWM_WIDTH_MASK_SFT               (0x3 << 12)
#define ACCDET_EINT0_PWM_FALL_DELAY_ADDR              \
	MT6357_ACCDET_CON16
#define ACCDET_EINT0_PWM_FALL_DELAY_SFT               5
#define ACCDET_EINT0_PWM_FALL_DELAY_MASK              0x1
#define ACCDET_EINT0_PWM_FALL_DELAY_MASK_SFT          (0x1 << 5)
#define ACCDET_EINT0_PWM_RISE_DELAY_ADDR              \
	MT6357_ACCDET_CON16
#define ACCDET_EINT0_PWM_RISE_DELAY_SFT               6
#define ACCDET_EINT0_PWM_RISE_DELAY_MASK              0x3FF
#define ACCDET_EINT0_PWM_RISE_DELAY_MASK_SFT          (0x3FF << 6)
#define ACCDET_TEST_MODE11_ADDR                       \
	MT6357_ACCDET_CON17
#define ACCDET_TEST_MODE11_SFT                        5
#define ACCDET_TEST_MODE11_MASK                       0x1
#define ACCDET_TEST_MODE11_MASK_SFT                   (0x1 << 5)
#define ACCDET_TEST_MODE10_ADDR                       \
	MT6357_ACCDET_CON17
#define ACCDET_TEST_MODE10_SFT                        6
#define ACCDET_TEST_MODE10_MASK                       0x1
#define ACCDET_TEST_MODE10_MASK_SFT                   (0x1 << 6)
#define ACCDET_EINT0_CMPOUT_SW_ADDR                   \
	MT6357_ACCDET_CON17
#define ACCDET_EINT0_CMPOUT_SW_SFT                    7
#define ACCDET_EINT0_CMPOUT_SW_MASK                   0x1
#define ACCDET_EINT0_CMPOUT_SW_MASK_SFT               (0x1 << 7)
#define ACCDET_EINT1_CMPOUT_SW_ADDR                   \
	MT6357_ACCDET_CON17
#define ACCDET_EINT1_CMPOUT_SW_SFT                    8
#define ACCDET_EINT1_CMPOUT_SW_MASK                   0x1
#define ACCDET_EINT1_CMPOUT_SW_MASK_SFT               (0x1 << 8)
#define ACCDET_TEST_MODE9_ADDR                        \
	MT6357_ACCDET_CON17
#define ACCDET_TEST_MODE9_SFT                         9
#define ACCDET_TEST_MODE9_MASK                        0x1
#define ACCDET_TEST_MODE9_MASK_SFT                    (0x1 << 9)
#define ACCDET_TEST_MODE8_ADDR                        \
	MT6357_ACCDET_CON17
#define ACCDET_TEST_MODE8_SFT                         10
#define ACCDET_TEST_MODE8_MASK                        0x1
#define ACCDET_TEST_MODE8_MASK_SFT                    (0x1 << 10)
#define ACCDET_AUXADC_CTRL_SW_ADDR                    \
	MT6357_ACCDET_CON17
#define ACCDET_AUXADC_CTRL_SW_SFT                     11
#define ACCDET_AUXADC_CTRL_SW_MASK                    0x1
#define ACCDET_AUXADC_CTRL_SW_MASK_SFT                (0x1 << 11)
#define ACCDET_TEST_MODE7_ADDR                        \
	MT6357_ACCDET_CON17
#define ACCDET_TEST_MODE7_SFT                         12
#define ACCDET_TEST_MODE7_MASK                        0x1
#define ACCDET_TEST_MODE7_MASK_SFT                    (0x1 << 12)
#define ACCDET_TEST_MODE6_ADDR                        \
	MT6357_ACCDET_CON17
#define ACCDET_TEST_MODE6_SFT                         13
#define ACCDET_TEST_MODE6_MASK                        0x1
#define ACCDET_TEST_MODE6_MASK_SFT                    (0x1 << 13)
#define ACCDET_EINT0_CMP_EN_SW_ADDR                   \
	MT6357_ACCDET_CON17
#define ACCDET_EINT0_CMP_EN_SW_SFT                    14
#define ACCDET_EINT0_CMP_EN_SW_MASK                   0x1
#define ACCDET_EINT0_CMP_EN_SW_MASK_SFT               (0x1 << 14)
#define ACCDET_EINT1_CMP_EN_SW_ADDR                   \
	MT6357_ACCDET_CON17
#define ACCDET_EINT1_CMP_EN_SW_SFT                    15
#define ACCDET_EINT1_CMP_EN_SW_MASK                   0x1
#define ACCDET_EINT1_CMP_EN_SW_MASK_SFT               (0x1 << 15)
#define ACCDET_EINT0_STATE_ADDR                       \
	MT6357_ACCDET_CON18
#define ACCDET_EINT0_STATE_SFT                        0
#define ACCDET_EINT0_STATE_MASK                       0x7
#define ACCDET_EINT0_STATE_MASK_SFT                   (0x7 << 0)
#define ACCDET_AUXADC_DEBOUNCE_END_ADDR               \
	MT6357_ACCDET_CON18
#define ACCDET_AUXADC_DEBOUNCE_END_SFT                3
#define ACCDET_AUXADC_DEBOUNCE_END_MASK               0x1
#define ACCDET_AUXADC_DEBOUNCE_END_MASK_SFT           (0x1 << 3)
#define ACCDET_AUXADC_CONNECT_PRE_ADDR                \
	MT6357_ACCDET_CON18
#define ACCDET_AUXADC_CONNECT_PRE_SFT                 4
#define ACCDET_AUXADC_CONNECT_PRE_MASK                0x1
#define ACCDET_AUXADC_CONNECT_PRE_MASK_SFT            (0x1 << 4)
#define ACCDET_EINT0_CUR_IN_ADDR                      \
	MT6357_ACCDET_CON18
#define ACCDET_EINT0_CUR_IN_SFT                       8
#define ACCDET_EINT0_CUR_IN_MASK                      0x1
#define ACCDET_EINT0_CUR_IN_MASK_SFT                  (0x1 << 8)
#define ACCDET_EINT0_SAM_IN_ADDR                      \
	MT6357_ACCDET_CON18
#define ACCDET_EINT0_SAM_IN_SFT                       9
#define ACCDET_EINT0_SAM_IN_MASK                      0x1
#define ACCDET_EINT0_SAM_IN_MASK_SFT                  (0x1 << 9)
#define ACCDET_EINT0_MEM_IN_ADDR                      \
	MT6357_ACCDET_CON18
#define ACCDET_EINT0_MEM_IN_SFT                       10
#define ACCDET_EINT0_MEM_IN_MASK                      0x1
#define ACCDET_EINT0_MEM_IN_MASK_SFT                  (0x1 << 10)
#define AD_EINT0CMPOUT_ADDR                           \
	MT6357_ACCDET_CON18
#define AD_EINT0CMPOUT_SFT                            14
#define AD_EINT0CMPOUT_MASK                           0x1
#define AD_EINT0CMPOUT_MASK_SFT                       (0x1 << 14)
#define DA_NI_EINT0CMPEN_ADDR                         \
	MT6357_ACCDET_CON18
#define DA_NI_EINT0CMPEN_SFT                          15
#define DA_NI_EINT0CMPEN_MASK                         0x1
#define DA_NI_EINT0CMPEN_MASK_SFT                     (0x1 << 15)
#define ACCDET_CUR_DEB_ADDR                           \
	MT6357_ACCDET_CON19
#define ACCDET_CUR_DEB_SFT                            0
#define ACCDET_CUR_DEB_MASK                           0xFFFF
#define ACCDET_CUR_DEB_MASK_SFT                       (0xFFFF << 0)
#define ACCDET_EINT0_CUR_DEB_ADDR                     \
	MT6357_ACCDET_CON20
#define ACCDET_EINT0_CUR_DEB_SFT                      0
#define ACCDET_EINT0_CUR_DEB_MASK                     0x7FFF
#define ACCDET_EINT0_CUR_DEB_MASK_SFT                 (0x7FFF << 0)
#define ACCDET_MON_FLAG_EN_ADDR                       \
	MT6357_ACCDET_CON21
#define ACCDET_MON_FLAG_EN_SFT                        0
#define ACCDET_MON_FLAG_EN_MASK                       0x1
#define ACCDET_MON_FLAG_EN_MASK_SFT                   (0x1 << 0)
#define ACCDET_MON_FLAG_SEL_ADDR                      \
	MT6357_ACCDET_CON21
#define ACCDET_MON_FLAG_SEL_SFT                       4
#define ACCDET_MON_FLAG_SEL_MASK                      0xFF
#define ACCDET_MON_FLAG_SEL_MASK_SFT                  (0xFF << 4)
#define ACCDET_RSV_CON1_ADDR                          \
	MT6357_ACCDET_CON22
#define ACCDET_RSV_CON1_SFT                           0
#define ACCDET_RSV_CON1_MASK                          0xFFFF
#define ACCDET_RSV_CON1_MASK_SFT                      (0xFFFF << 0)
#define ACCDET_AUXADC_CONNECT_TIME_ADDR               \
	MT6357_ACCDET_CON23
#define ACCDET_AUXADC_CONNECT_TIME_SFT                0
#define ACCDET_AUXADC_CONNECT_TIME_MASK               0xFFFF
#define ACCDET_AUXADC_CONNECT_TIME_MASK_SFT           (0xFFFF << 0)
#define ACCDET_HWEN_SEL_ADDR                          \
	MT6357_ACCDET_CON24
#define ACCDET_HWEN_SEL_SFT                           0
#define ACCDET_HWEN_SEL_MASK                          0x3
#define ACCDET_HWEN_SEL_MASK_SFT                      (0x3 << 0)
#define ACCDET_HWMODE_SEL_ADDR                        \
	MT6357_ACCDET_CON24
#define ACCDET_HWMODE_SEL_SFT                         2
#define ACCDET_HWMODE_SEL_MASK                        0x1
#define ACCDET_HWMODE_SEL_MASK_SFT                    (0x1 << 2)
#define ACCDET_EINT_DEB_OUT_DFF_ADDR                  \
	MT6357_ACCDET_CON24
#define ACCDET_EINT_DEB_OUT_DFF_SFT                   3
#define ACCDET_EINT_DEB_OUT_DFF_MASK                  0x1
#define ACCDET_EINT_DEB_OUT_DFF_MASK_SFT              (0x1 << 3)
#define ACCDET_FAST_DISCHARGE_ADDR                    \
	MT6357_ACCDET_CON24
#define ACCDET_FAST_DISCHARGE_SFT                     4
#define ACCDET_FAST_DISCHARGE_MASK                    0x1
#define ACCDET_FAST_DISCHARGE_MASK_SFT                (0x1 << 4)
#define ACCDET_EINT0_REVERSE_ADDR                     \
	MT6357_ACCDET_CON24
#define ACCDET_EINT0_REVERSE_SFT                      14
#define ACCDET_EINT0_REVERSE_MASK                     0x1
#define ACCDET_EINT0_REVERSE_MASK_SFT                 (0x1 << 14)
#define ACCDET_EINT1_REVERSE_ADDR                     \
	MT6357_ACCDET_CON24
#define ACCDET_EINT1_REVERSE_SFT                      15
#define ACCDET_EINT1_REVERSE_MASK                     0x1
#define ACCDET_EINT1_REVERSE_MASK_SFT                 (0x1 << 15)
#define ACCDET_EINT1_DEB_SEL_ADDR                     \
	MT6357_ACCDET_CON25
#define ACCDET_EINT1_DEB_SEL_SFT                      0
#define ACCDET_EINT1_DEB_SEL_MASK                     0x1
#define ACCDET_EINT1_DEB_SEL_MASK_SFT                 (0x1 << 0)
#define ACCDET_EINT1_DEBOUNCE_ADDR                    \
	MT6357_ACCDET_CON25
#define ACCDET_EINT1_DEBOUNCE_SFT                     3
#define ACCDET_EINT1_DEBOUNCE_MASK                    0xF
#define ACCDET_EINT1_DEBOUNCE_MASK_SFT                (0xF << 3)
#define ACCDET_EINT1_PWM_THRESH_ADDR                  \
	MT6357_ACCDET_CON25
#define ACCDET_EINT1_PWM_THRESH_SFT                   8
#define ACCDET_EINT1_PWM_THRESH_MASK                  0x7
#define ACCDET_EINT1_PWM_THRESH_MASK_SFT              (0x7 << 8)
#define ACCDET_EINT1_PWM_WIDTH_ADDR                   \
	MT6357_ACCDET_CON25
#define ACCDET_EINT1_PWM_WIDTH_SFT                    12
#define ACCDET_EINT1_PWM_WIDTH_MASK                   0x3
#define ACCDET_EINT1_PWM_WIDTH_MASK_SFT               (0x3 << 12)
#define ACCDET_EINT1_PWM_FALL_DELAY_ADDR              \
	MT6357_ACCDET_CON26
#define ACCDET_EINT1_PWM_FALL_DELAY_SFT               5
#define ACCDET_EINT1_PWM_FALL_DELAY_MASK              0x1
#define ACCDET_EINT1_PWM_FALL_DELAY_MASK_SFT          (0x1 << 5)
#define ACCDET_EINT1_PWM_RISE_DELAY_ADDR              \
	MT6357_ACCDET_CON26
#define ACCDET_EINT1_PWM_RISE_DELAY_SFT               6
#define ACCDET_EINT1_PWM_RISE_DELAY_MASK              0x3FF
#define ACCDET_EINT1_PWM_RISE_DELAY_MASK_SFT          (0x3FF << 6)
#define ACCDET_EINT1_STATE_ADDR                       \
	MT6357_ACCDET_CON27
#define ACCDET_EINT1_STATE_SFT                        0
#define ACCDET_EINT1_STATE_MASK                       0x7
#define ACCDET_EINT1_STATE_MASK_SFT                   (0x7 << 0)
#define ACCDET_EINT1_CUR_IN_ADDR                      \
	MT6357_ACCDET_CON27
#define ACCDET_EINT1_CUR_IN_SFT                       8
#define ACCDET_EINT1_CUR_IN_MASK                      0x1
#define ACCDET_EINT1_CUR_IN_MASK_SFT                  (0x1 << 8)
#define ACCDET_EINT1_SAM_IN_ADDR                      \
	MT6357_ACCDET_CON27
#define ACCDET_EINT1_SAM_IN_SFT                       9
#define ACCDET_EINT1_SAM_IN_MASK                      0x1
#define ACCDET_EINT1_SAM_IN_MASK_SFT                  (0x1 << 9)
#define ACCDET_EINT1_MEM_IN_ADDR                      \
	MT6357_ACCDET_CON27
#define ACCDET_EINT1_MEM_IN_SFT                       10
#define ACCDET_EINT1_MEM_IN_MASK                      0x1
#define ACCDET_EINT1_MEM_IN_MASK_SFT                  (0x1 << 10)
#define AD_EINT1CMPOUT_ADDR                           \
	MT6357_ACCDET_CON27
#define AD_EINT1CMPOUT_SFT                            14
#define AD_EINT1CMPOUT_MASK                           0x1
#define AD_EINT1CMPOUT_MASK_SFT                       (0x1 << 14)
#define DA_NI_EINT1CMPEN_ADDR                         \
	MT6357_ACCDET_CON27
#define DA_NI_EINT1CMPEN_SFT                          15
#define DA_NI_EINT1CMPEN_MASK                         0x1
#define DA_NI_EINT1CMPEN_MASK_SFT                     (0x1 << 15)
#define ACCDET_EINT1_CUR_DEB_ADDR                     \
	MT6357_ACCDET_CON28
#define ACCDET_EINT1_CUR_DEB_SFT                      0
#define ACCDET_EINT1_CUR_DEB_MASK                     0x7FFF
#define ACCDET_EINT1_CUR_DEB_MASK_SFT                 (0x7FFF << 0)

#define RG_RTC32K_CK_PDN_ADDR                         \
	MT6357_TOP_CKPDN_CON0
#define RG_RTC32K_CK_PDN_SFT                          15
#define RG_RTC32K_CK_PDN_MASK                         0x1
#define RG_RTC32K_CK_PDN_MASK_SFT                     (0x1 << 15)
#define AUXADC_RQST_CH5_ADDR                          \
	MT6357_AUXADC_RQST0
#define AUXADC_RQST_CH5_SFT                           5
#define AUXADC_RQST_CH5_MASK                          0x1
#define AUXADC_RQST_CH5_MASK_SFT                      (0x1 << 5)
#define ACCDET_EINT0_IRQ_POLARITY_ADDR                \
	MT6357_ACCDET_CON12
#define ACCDET_EINT0_IRQ_POLARITY_SFT                 14
#define ACCDET_EINT0_IRQ_POLARITY_MASK                0x1
#define ACCDET_EINT0_IRQ_POLARITY_MASK_SFT            (0x1 << 14)
#define ACCDET_EINT1_IRQ_POLARITY_ADDR                \
	MT6357_ACCDET_CON12
#define ACCDET_EINT1_IRQ_POLARITY_SFT                 15
#define ACCDET_EINT1_IRQ_POLARITY_MASK                0x1
#define ACCDET_EINT1_IRQ_POLARITY_MASK_SFT            (0x1 << 15)

#define ACCDET_HWMODE_SEL_BIT		BIT(2)
#define ACCDET_FAST_DISCAHRGE		BIT(4)

/* AUDENC_ANA_CON6:  analog fast discharge*/
#define RG_AUDSPARE				(0x00A0)
#define RG_AUDSPARE_FSTDSCHRG_ANALOG_DIR_EN	BIT(5)
#define RG_AUDSPARE_FSTDSCHRG_IMPR_EN		BIT(7)

/* 0ms */
#define ACCDET_EINT1_DEB_BYPASS		(0x00<<3)
/* 0.12ms */
#define ACCDET_EINT1_DEB_OUT_012	(0x01<<3)
/* 32ms */
#define ACCDET_EINT1_DEB_IN_32		(0x0A<<3)
/* 64ms */
#define ACCDET_EINT1_DEB_IN_64		(0x0C<<3)
/* 256ms */
#define ACCDET_EINT1_DEB_IN_256		(0x0E<<3)
/* 512ms */
#define ACCDET_EINT1_DEB_512		(0x0F<<3)

/* ACCDET_CON15: accdet eint0 debounce, PWM width&thresh, etc.
 * bit0: ACCDET_EINT0_DEB_SEL, 1,debounce_multi_sync_path;0,from register
 */
#define ACCDET_EINT0_DEB_SEL		(0x01<<0)
/* 0ms */
#define ACCDET_EINT0_DEB_BYPASS		(0x00<<3)
/* 0.12ms */
#define ACCDET_EINT0_DEB_OUT_012	(0x01)
/* 32ms */
#define ACCDET_EINT0_DEB_IN_32		(0x0A)
/* 64ms */
#define ACCDET_EINT0_DEB_IN_64		(0x0C)
/* 256ms */
#define ACCDET_EINT0_DEB_IN_256		(0x0E)
/* 512ms */
#define ACCDET_EINT0_DEB_512		(0x0F)
#define ACCDET_EINT0_DEB_CLR		(0x0F)

/* AUDENC_ANA_CON10: */
#define RG_ACCDET_MODE_ANA11_MODE1	(0x0807)
#define RG_ACCDET_MODE_ANA11_MODE2	(0x0887)
#define RG_ACCDET_MODE_ANA11_MODE6	(0x0887)

#define ACCDET_CALI_MASK0		(0xFF)
#define ACCDET_CALI_MASK1		(0xFF<<8)
#define ACCDET_CALI_MASK2		(0xFF)
#define ACCDET_CALI_MASK3		(0xFF<<8)
#define ACCDET_CALI_MASK4		(0xFF)

#define ACCDET_EINT_IRQ_B2_B3		(0x03<<ACCDET_EINT0_IRQ_SFT)

/* ACCDET_CON25: RO, accdet FSM state,etc.*/
#define ACCDET_STATE_MEM_IN_OFFSET	(ACCDET_MEM_IN_SFT)
#define ACCDET_STATE_AB_MASK		(0x03)
#define ACCDET_STATE_AB_00		(0x00)
#define ACCDET_STATE_AB_01		(0x01)
#define ACCDET_STATE_AB_10		(0x02)
#define ACCDET_STATE_AB_11		(0x03)

#endif/* end _MT6357_H_ */