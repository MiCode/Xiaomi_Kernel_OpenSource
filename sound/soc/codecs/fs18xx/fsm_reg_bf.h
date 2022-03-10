/* SPDX-License-Identifier: GPL-2.0 */

/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2019-09-20 File created.
 */
#ifndef __FSM_REG_BF_H__
#define __FSM_REG_BF_H__

#define FSM_STATUS         0xF000
#define FSM_BOVDS          0x0000
#define FSM_PLLS           0x0100
#define FSM_OTDS           0x0200
#define FSM_OVDS           0x0300
#define FSM_UVDS           0x0400
#define FSM_OCDS           0x0500
#define FSM_CLKS           0x0600
#define FSM_SPKS           0x0A00
#define FSM_SPKT           0x0B00

#define FSM_BATS           0xF001
#define FSM_BATV           0x9001

#define FSM_TEMPS          0xF002
#define FSM_TEMPV          0x8002

#define FSM_ID             0xF003
#define FSM_REVID          0x7003
#define FSM_DEVID          0x7803

#define FSM_I2SCTRL        0xF004
#define FSM_I2SF           0x2004
#define FSM_CHS12          0x1304
#define FSM_DISP           0x0A04
#define FSM_I2SDOE         0x0B04
#define FSM_I2SSR          0x3C04

#define FSM_ANASTAT        0xF005
#define FSM_STVBG          0x0005
#define FSM_STPLL          0x0105
#define FSM_STBST          0x0205
#define FSM_STAMP          0x0305

#define FSM_AUDIOCTRL      0xF006
#define FSM_VOL            0x7806

#define FSM_TEMPSEL        0xF008
#define FSM_EXTTS          0x8108

#define FSM_SYSCTRL        0xF009
#define FSM_PWDN           0x0009
#define FSM_I2CR           0x0109
#define FSM_AMPE           0x0309

#define FSM_SPKSET         0xF00A
#define FSM_SPKR           0x190A

#define FSM_OTPACC         0xF00B
#define FSM_TRIMKEY        0x700B
#define FSM_REKEY          0x780B

#define FSM_SPKMT01        0xF064
#define FSM_SPKMT02        0xF065
#define FSM_SPKMT03        0xF066
#define FSM_SPKMT04        0xF067
#define FSM_SPKMT05        0xF068
#define FSM_SPKMT06        0xF069

#define FSM_STERC1         0xF070
#define FSM_STERCTRL       0xF07E
#define FSM_STERGAIN       0xF07F
#define FSM_SPKSTATUS      0xF080

#define FSM_ACSCTRL        0xF089
#define FSM_ACS_COE_SEL    0x0489
#define FSM_MBDRC_EN       0x0789
#define FSM_EQEN           0x0C89
#define FSM_ACS_EN         0x0F89

#define FSM_ACSDRC         0xF08A
#define FSM_DRCS1_EN       0x008A
#define FSM_DRCS2_EN       0x018A
#define FSM_DRCS3_EN       0x028A
#define FSM_DRC_ENV_W      0x188A

#define FSM_ACSDRCS        0xF08B

#define FSM_CHIPINI        0xF090
#define FSM_INIFINISH      0x0090
#define FSM_INIOK          0x0190

#define FSM_I2CADD         0xF091

#define FSM_I2SSET         0xF0A0
#define FSM_BCMP           0x13A0
#define FSM_LRCLKP         0x05A0
#define FSM_BCLKP          0x06A0
#define FSM_I2SOSWAP       0x07A0
#define FSM_AECSELL        0x28A0
#define FSM_AECSELR        0x2CA0
#define FSM_BCLKSTA        0x0FA0

#define FSM_DSPCTRL        0xF0A1
#define FSM_DCCOEF         0x20A1
#define FSM_NOFILTEN       0x04A1
#define FSM_POSTEQEN       0x0BA1
#define FSM_DSPEN          0x0CA1
#define FSM_EQCOEFSEL      0x0DA1

#define FSM_DACEQWL        0xF0A2
#define FSM_DACEQWH        0xF0A3
#define FSM_DACEQRL        0xF0A4
#define FSM_DACEQRH        0xF0A5
#define FSM_DACEQA         0xF0A6

#define FSM_BFLCTRL        0xF0A7
#define FSM_BFLSET         0xF0A8
#define FSM_SQC            0xF0A9
#define FSM_AGC            0xF0AA

#define FSM_DRPARA         0xF0AD
#define FSM_DRC            0xC0AD

#define FSM_DACCTRL        0xF0AE
#define FSM_SDMSTLBYP      0x04AE
#define FSM_DACMUTE        0x08AE
#define FSM_DACFADE        0x09AE

#define FSM_TSCTRL         0xF0AF
#define FSM_GAIN           0x20AF
#define FSM_TSEN           0x03AF
#define FSM_OFF_THD        0x24AF
#define FSM_OFF_DELAY      0x28AF
#define FSM_OFF_ZEROCRS    0x0CAF
#define FSM_OFF_AUTOEN     0x0DAF
#define FSM_OFFSTA         0x0EAF

#define FSM_ADCCTRL        0xF0B3
#define FSM_EQB1EN_R       0x08B3
#define FSM_EQB0EN_R       0x09B3
#define FSM_ADCRGAIN       0x1AB3
#define FSM_ADCREN         0x0CB3
#define FSM_ADCRSEL        0x0DB3

#define FSM_ADCEQWL        0xF0B4
#define FSM_ADCEQWH        0xF0B5
#define FSM_ADCEQRL        0xF0B6
#define FSM_ADCEQRH        0xF0B7
#define FSM_ADCEQA         0xF0B8
#define FSM_ADCENV         0xF0B9
#define FSM_ADCTIME        0xF0BA

#define FSM_ZMDATA         0xF0BB
#define FSM_DACENV         0xF0BC

#define FSM_DIGSTAT        0xF0BD
#define FSM_ADCRUN         0x00BD
#define FSM_DACRUN         0x01BD
#define FSM_DSPFLAG        0x03BD
#define FSM_SSPKM24        0x04BD
#define FSM_SSPKM6         0x05BD
#define FSM_SSPKRE         0x06BD
#define FSM_SSPKFSM        0x3CBD

#define FSM_BSTCTRL        0xF0C0
#define FSM_DISCHARGE      0x00C0
#define FSM_DAC_GAIN       0x11C0
#define FSM_BSTEN          0x03C0
#define FSM_MODE_CTRL      0x14C0
#define FSM_ILIM_SEL       0x36C0
#define FSM_VOUT_SEL       0x3AC0
#define FSM_SSEND          0x0FC0

#define FSM_PLLCTRL1       0xF0C1
#define FSM_PLLCTRL2       0xF0C2
#define FSM_PLLCTRL3       0xF0C3

#define FSM_PLLCTRL4       0xF0C4
#define FSM_PLLEN          0x00C4
#define FSM_OSCEN          0x01C4
#define FSM_ZMEN           0x02C4
#define FSM_VBGEN          0x03C4

#define FSM_OCCTRL         0xF0C5
#define FSM_OTCTRL         0xF0C6
#define FSM_UVCTRL         0xF0C7
#define FSM_OVCTRL         0xF0C8
#define FSM_SPKERR         0xF0C9
#define FSM_SPKM24         0xF0CA
#define FSM_SPKM6          0xF0CB
#define FSM_SPKRE          0xF0CC
#define FSM_SPKMDB         0xF0CD
#define FSM_ADPBST         0xF0CF
#define FSM_ANACTRL        0xF0D0
#define FSM_CLDCTRL        0xF0D3
#define FSM_REFGEN         0xF0D4
#define FSM_ZMCONFIG       0xF0D5
#define FSM_AUXCFG         0xF0D7

#define FSM_OTPCMD         0xF0DC
#define FSM_OTPCMDR        0x00DC
#define FSM_OTPCMDW        0x01DC
#define FSM_OTPBUSY        0x02DC
#define FSM_EPROM_LD       0x08DC
#define FSM_OTPPGW         0x0CDC

#define FSM_OTPADDR        0xF0DD
#define FSM_OTPWDATA       0xF0DE
#define FSM_OTPRDATA       0xF0DF
#define FSM_OTPPG0W0       0xF0E0
#define FSM_OTPPG0W1       0xF0E1
#define FSM_OTPPG0W2       0xF0E2
#define FSM_OTPPG0W3       0xF0E3
#define FSM_OTPPG1W0       0xF0E4
#define FSM_OTPPG1W1       0xF0E5
#define FSM_OTPPG1W2       0xF0E6
#define FSM_OTPPG1W3       0xF0E7
#define FSM_OTPPG2         0xF0E8

#endif /* Generated at: 2019-09-20.16:09:45 */
