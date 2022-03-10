/* SPDX-License-Identifier: GPL-2.0 */

/**
 * Copyright (C) 2016-2019 Fourier Semiconductor Inc. All rights reserved.
 * 2019-08-29 File created.
 */

#ifndef __FS1801_H__
#define __FS1801_H__

#define FS1801_STATUS         0xF000
#define FS1801_BOVDS          0x0000
#define FS1801_PLLS           0x0100
#define FS1801_OTDS           0x0200
#define FS1801_OVDS           0x0300
#define FS1801_UVDS           0x0400
#define FS1801_OCDS           0x0500
#define FS1801_CLKS           0x0600
#define FS1801_SPKS           0x0A00
#define FS1801_SPKT           0x0B00

#define FS1801_BATS           0xF001
#define FS1801_BATV           0x9001

#define FS1801_TEMPS          0xF002
#define FS1801_TEMPV          0x8002

#define FS1801_ID             0xF003
#define FS1801_REV            0x7003
#define FS1801_DEVID          0x7803

#define FS1801_I2SCTRL        0xF004
#define FS1801_I2SF           0x2004
#define FS1801_CHS12          0x1304
#define FS1801_DISP           0x0A04
#define FS1801_I2SDOE         0x0B04
#define FS1801_I2SSR          0x3C04

#define FS1801_ANASTAT        0xF005
#define FS1801_VBGS           0x0005
#define FS1801_PLLSTAT        0x0105
#define FS1801_BSTS           0x0205
#define FS1801_AMPS           0x0305
#define FS1801_OTPRDE         0x0805

#define FS1801_AUDIOCTRL      0xF006
#define FS1801_VOL            0x7806

#define FS1801_TEMPSEL        0xF008
#define FS1801_EXTTS          0x8108

#define FS1801_SYSCTRL        0xF009
#define FS1801_PWDN           0x0009
#define FS1801_I2CR           0x0109
#define FS1801_AMPE           0x0309

#define FS1801_SPKSET         0xF00A
#define FS1801_SPKR           0x190A

#define FS1801_OTPACC         0xF00B
#define FS1801_TRIMKEY        0x700B
#define FS1801_REKEY          0x780B

#define FS1801_SPKSTATUS      0xF080
#define FS1801_CALSTAT        0x0180

#define FS1801_ACSCTRL        0xF089
#define FS1801_ACS_COE_SEL    0x0489
#define FS1801_MBDRC_EN       0x0789
#define FS1801_EQEN           0x0C89
#define FS1801_ACS_EN         0x0F89

#define FS1801_ACSDRC         0xF08A
#define FS1801_DRCS1_EN       0x008A
#define FS1801_DRCS2_EN       0x018A
#define FS1801_DRCS3_EN       0x028A
#define FS1801_DRC_ENV_W      0x188A

#define FS1801_ACSDRCS        0xF08B
#define FS1801_DRCS11_EN      0x028B
#define FS1801_DRCS12_EN      0x048B
#define FS1801_DRCS21_EN      0x078B
#define FS1801_DRCS22_EN      0x098B
#define FS1801_DRCS31_EN      0x0C8B
#define FS1801_DRCS32_EN      0x0E8B

#define FS1801_CHIPINI        0xF090
#define FS1801_INIFINISH      0x0090
#define FS1801_INIOK          0x0190

#define FS1801_I2CADD         0xF091
#define FS1801_ADR            0x6091

#define FS1801_BISTCTL1       0xF09C
#define FS1801_GO             0x009C
#define FS1801_MODE           0x019C
#define FS1801_TRAMSEL        0x029C
#define FS1801_HCLKEN         0x039C
#define FS1801_RAM1_SKIP      0x049C
#define FS1801_RAM2_SKIP      0x059C
#define FS1801_RAM3_SKIP      0x069C
#define FS1801_RAM4_SKIP      0x079C
#define FS1801_RAM5_SKIP      0x089C
#define FS1801_RAM6_SKIP      0x099C
#define FS1801_RAM7_SKIP      0x0A9C
#define FS1801_RAM8_SKIP      0x0B9C
#define FS1801_RAM9_SKIP      0x0C9C
#define FS1801_RAMA_SKIP      0x0D9C
#define FS1801_RAMB_SKIP      0x0E9C
#define FS1801_RAMC_SKIP      0x0F9C

#define FS1801_BISTSTAT1      0xF09D
#define FS1801_RAM            0x309D
#define FS1801_FAIL           0x049D
#define FS1801_ACTIVE         0x069D
#define FS1801_DONE           0x079D
#define FS1801_ADDR           0x789D

#define FS1801_BISTSTAT2      0xF09E

#define FS1801_BISTSTAT3      0xF09F

#define FS1801_I2SSET         0xF0A0
#define FS1801_AECRS          0x10A0
#define FS1801_BCMP           0x13A0
#define FS1801_LRCLKP         0x05A0
#define FS1801_BCLKP          0x06A0
#define FS1801_I2SOSWAP       0x07A0
#define FS1801_AECSELL        0x28A0
#define FS1801_AECSELR        0x2CA0
#define FS1801_BCLKSTA        0x0FA0

#define FS1801_DSPCTRL        0xF0A1
#define FS1801_DCCOEF         0x20A1
#define FS1801_NOFILTEN       0x04A1
#define FS1801_POSTEQEN       0x0BA1
#define FS1801_DSPEN          0x0CA1
#define FS1801_EQCOEFSEL      0x0DA1

#define FS1801_DACEQWL        0xF0A2

#define FS1801_DACEQWH        0xF0A3

#define FS1801_DACEQRL        0xF0A4

#define FS1801_DACEQRH        0xF0A5

#define FS1801_DACEQA         0xF0A6

#define FS1801_BFLCTRL        0xF0A7

#define FS1801_BFLSET         0xF0A8

#define FS1801_SQC            0xF0A9

#define FS1801_AGC            0xF0AA

#define FS1801_DACPEAKRD      0xF0AB
#define FS1801_SEL            0x40AB

#define FS1801_PDCCTRL        0xF0AC
#define FS1801_RMEN           0x08AC
#define FS1801_TIME           0x29AC
#define FS1801_THD            0x2CAC

#define FS1801_DRPARA         0xF0AD
#define FS1801_DRC            0xC0AD

#define FS1801_DACCTRL        0xF0AE
#define FS1801_SDMSTLBYP      0x04AE
#define FS1801_MUTE           0x08AE
#define FS1801_FADE           0x09AE

#define FS1801_TSCTRL         0xF0AF
#define FS1801_TSGAIN         0x20AF
#define FS1801_TSEN           0x03AF
#define FS1801_OFF_THD        0x24AF
#define FS1801_OFF_DELAY      0x28AF
#define FS1801_ZEROOFF        0x0CAF
#define FS1801_AUTOOFF        0x0DAF
#define FS1801_OFFSTA         0x0EAF

#define FS1801_MODCTRL        0xF0B0
#define FS1801_G1_SEL         0x20B0
#define FS1801_G2_SEL         0x23B0
#define FS1801_DEMBYP         0x07B0
#define FS1801_DITHPOS        0x48B0
#define FS1801_DITHRANGE      0x0DB0
#define FS1801_DITHCLR        0x0EB0
#define FS1801_DITHEN         0x0FB0

#define FS1801_DTINI          0xF0B1

#define FS1801_DTFD           0xF0B2

#define FS1801_ADCCTRL        0xF0B3
#define FS1801_EQB1EN_R       0x08B3
#define FS1801_EQB0EN_R       0x09B3
#define FS1801_ADCRGAIN       0x1AB3
#define FS1801_ADCREN         0x0CB3
#define FS1801_ADCRSEL        0x0DB3

#define FS1801_ADCEQWL        0xF0B4

#define FS1801_ADCEQWH        0xF0B5

#define FS1801_ADCEQRL        0xF0B6

#define FS1801_ADCEQRH        0xF0B7

#define FS1801_ADCEQA         0xF0B8

#define FS1801_ADCENV         0xF0B9
#define FS1801_AMP_DT_A       0xC0B9
#define FS1801_AVG_NUM        0x1DB9
#define FS1801_AMP_DT_EN      0x0FB9

#define FS1801_ADCTIME        0xF0BA
#define FS1801_STABLE_TM      0x20BA
#define FS1801_CHK_TM         0x23BA

#define FS1801_ZMDATA         0xF0BB
#define FS1801_XAVGM          0x70BB
#define FS1801_XAVGH          0x78BB

#define FS1801_DACENV         0xF0BC
#define FS1801_MBYTE          0x70BC
#define FS1801_HBYTE          0x78BC

#define FS1801_DIGSTAT        0xF0BD
#define FS1801_ADCRUN         0x00BD
#define FS1801_DACRUN         0x01BD
#define FS1801_DSPFLAG        0x03BD
#define FS1801_STSPKM24       0x04BD
#define FS1801_STSPKM6        0x05BD
#define FS1801_STSPKRE        0x06BD
#define FS1801_SPKFSM         0x3CBD

#define FS1801_I2SPINC        0xF0BE
#define FS1801_BCPDD          0x00BE
#define FS1801_LRPDD          0x01BE
#define FS1801_SDOPDD         0x02BE
#define FS1801_SDIPDD         0x03BE

#define FS1801_BSTCTRL        0xF0C0
#define FS1801_DISCHARGE      0x00C0
#define FS1801_DAC_GAIN       0x11C0
#define FS1801_BSTEN          0x03C0
#define FS1801_MODE_CTRL      0x14C0
#define FS1801_ILIM_SEL       0x36C0
#define FS1801_VOUT_SEL       0x3AC0
#define FS1801_SSEND          0x0FC0

#define FS1801_PLLCTRL1       0xF0C1
#define FS1801_POSTSEL        0x12C1
#define FS1801_FINSEL         0x14C1
#define FS1801_BWSEL          0x16C1
#define FS1801_ICPSEL         0x18C1

#define FS1801_PLLCTRL2       0xF0C2

#define FS1801_PLLCTRL3       0xF0C3

#define FS1801_PLLCTRL4       0xF0C4
#define FS1801_PLLEN          0x00C4
#define FS1801_OSCEN          0x01C4
#define FS1801_ZMEN           0x02C4
#define FS1801_VBGEN          0x03C4

#define FS1801_OCCTRL         0xF0C5
#define FS1801_OCNUM          0x70C5

#define FS1801_OTCTRL         0xF0C6
#define FS1801_OTTHD_L        0x70C6
#define FS1801_OTTHD_H        0x78C6

#define FS1801_UVCTRL         0xF0C7
#define FS1801_UVTHD_L        0x70C7
#define FS1801_UVTHD_H        0x78C7

#define FS1801_OVCTRL         0xF0C8
#define FS1801_OVTHD_L        0x70C8
#define FS1801_OVTHD_H        0x78C8

#define FS1801_SPKERR         0xF0C9

#define FS1801_SPKM24         0xF0CA

#define FS1801_SPKM6          0xF0CB

#define FS1801_SPKRE          0xF0CC

#define FS1801_SPKMDB         0xF0CD
#define FS1801_DBVSPKM6       0x70CD
#define FS1801_DBVSPKM24      0x78CD

#define FS1801_ADPBST         0xF0CF
#define FS1801_TFSEL          0x12CF
#define FS1801_TRSEL          0x14CF
#define FS1801_DCG_SEL        0x1ECF

#define FS1801_ANACTRL        0xF0D0
#define FS1801_BPCLKCK        0x00D0
#define FS1801_BPUVOV         0x01D0
#define FS1801_BPOT           0x02D0
#define FS1801_BPOC           0x03D0
#define FS1801_BPOV           0x04D0
#define FS1801_BPSPKOT        0x05D0
#define FS1801_PTSEQBP        0x06D0
#define FS1801_HWSEQEN        0x08D0
#define FS1801_FSWSDLY        0x2CD0
#define FS1801_FSWSDLYEN      0x0FD0

#define FS1801_BSTTEST        0xF0D1
#define FS1801_BSTMODE        0x20D1
#define FS1801_BSTTMEN        0x04D1
#define FS1801_CLADEN         0x05D1
#define FS1801_ENVSEL         0x08D1
#define FS1801_ENV            0x19D1

#define FS1801_CLDTEST        0xF0D2
#define FS1801_DRVTM          0x00D2
#define FS1801_TB2EN          0x01D2
#define FS1801_TB2SEL         0x32D2
#define FS1801_TB1EN          0x06D2
#define FS1801_TB1SEL         0x37D2
#define FS1801_PORTWM         0x0BD2

#define FS1801_CLDCTRL        0xF0D3
#define FS1801_DAMPTB         0x70D3
#define FS1801_SDZREG         0x08D3
#define FS1801_DEGCLKBP       0x0AD3
#define FS1801_SEQBP          0x0BD3

#define FS1801_REFGEN         0xF0D4
#define FS1801_TMSELVIN3      0x0CD4
#define FS1801_TMSELVBG       0x0DD4

#define FS1801_ZMCONFIG       0xF0D5
#define FS1801_CALIBEN        0x04D5

#define FS1801_AUXCFG         0xF0D7
#define FS1801_CRAMACKSEL     0x01D7
#define FS1801_CLKSWSEL       0x04D7
#define FS1801_CLKSWFORCE     0x05D7
#define FS1801_DSPRST         0x08D7
#define FS1801_DCBP           0x0CD7

#define FS1801_CHPTEST        0xF0D8
#define FS1801_TESTMODE       0x30D8
#define FS1801_TESTEN         0x04D8
#define FS1801_VOUTADS1       0x08D8
#define FS1801_VOUTADS1EN     0x09D8
#define FS1801_VOUTADS2       0x0AD8
#define FS1801_VOUTADS2EN     0x0BD8
#define FS1801_VINADS1EN      0x0CD8
#define FS1801_VINADS2EN      0x0DD8
#define FS1801_ATEADS2EN      0x0ED8

#define FS1801_I2CCTRL        0xF0D9
#define FS1801_TIMEOUTEN      0x00D9
#define FS1801_BPDEGLITCH     0x01D9

#define FS1801_ANATEST1       0xF0DA
#define FS1801_SARDO          0x60DA
#define FS1801_SARINSEL       0x19DA
#define FS1801_SARTEN         0x0CDA
#define FS1801_TP1DINDIS      0x0DDA
#define FS1801_OTPPON         0x0EDA
#define FS1801_OTPVPP         0x0FDA

#define FS1801_ANATEST2       0xF0DB
#define FS1801_PLLTEST        0x00DB
#define FS1801_TPIN1MUX       0x31DB
#define FS1801_TPIN2MUX       0x35DB
#define FS1801_TPINOEN        0x09DB
#define FS1801_EXTPEN         0x0ADB
#define FS1801_SARBATSAMP     0x0BDB
#define FS1801_SARBATCOND     0x0CDB
#define FS1801_ADCENVDTEN     0x0EDB
#define FS1801_SARTM          0x0FDB

#define FS1801_OTPCMD         0xF0DC
#define FS1801_OTPCMDR        0x00DC
#define FS1801_OTPCMDW        0x01DC
#define FS1801_OTPBUSY        0x02DC
#define FS1801_EPROM_LD       0x08DC
#define FS1801_OTPPGW         0x0CDC
#define FS1801_WMODE          0x0DDC

#define FS1801_OTPADDR        0xF0DD

#define FS1801_OTPWDATA       0xF0DE

#define FS1801_OTPRDATA       0xF0DF

#define FS1801_OTPPG0W0       0xF0E0

#define FS1801_OTPPG0W1       0xF0E1

#define FS1801_OTPPG0W2       0xF0E2

#define FS1801_OTPPG0W3       0xF0E3

#define FS1801_OTPPG1W0       0xF0E4

#define FS1801_OTPPG1W1       0xF0E5

#define FS1801_OTPPG1W2       0xF0E6

#define FS1801_OTPPG1W3       0xF0E7

#define FS1801_OTPPG2         0xF0E8


#endif /* Generated at: 2019-08-29.11:01:17 */
