/*
 * aw8896_reg.h   aw8896 codec register
 *
 * Version: v1.0.7
 *
 * Copyright (c) 2017 AWINIC Technology CO., LTD
 *
 * Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _AW8896_REG_H_
#define _AW8896_REG_H_

/*
 * Register List
 */
#define AW8896_REG_ID           0x00
#define AW8896_REG_SYSST        0x01
#define AW8896_REG_SYSINT       0x02
#define AW8896_REG_SYSINTM      0x03
#define AW8896_REG_SYSCTRL      0x04
#define AW8896_REG_I2SCTRL      0x05
#define AW8896_REG_I2STXCFG     0x06
#define AW8896_REG_DSPCFG       0x07
#define AW8896_REG_PWMCTRL      0x08
#define AW8896_REG_HAGCCFG1     0x09
#define AW8896_REG_HAGCCFG2     0x0A
#define AW8896_REG_DBGCTRL      0x20
#define AW8896_REG_I2SCFG       0x21
#define AW8896_REG_I2SSTAT      0x22
#define AW8896_REG_I2SCAPCNT    0x23
#define AW8896_REG_DSPMADD      0x40
#define AW8896_REG_DSPMDAT      0x41
#define AW8896_REG_WDT          0x42
#define AW8896_REG_GENCTRL      0x60
#define AW8896_REG_BSTCTRL      0x61
#define AW8896_REG_PLLCTRL1     0x62
#define AW8896_REG_PLLCTRL2     0x63
#define AW8896_REG_TESTCTRL     0x64
#define AW8896_REG_AMPDBG1      0x65
#define AW8896_REG_AMPDBG2      0x66
#define AW8896_REG_BSTDBG1      0x67

#define AW8896_REG_MAX          0x6F
#define AW8896_REG_DSP_START    0X3F
#define AW8896_REG_DSP_END      0X43

/*
 * Register Access
 */
#define REG_NONE_ACCESS 0
#define REG_RD_ACCESS  (1 << 0)
#define REG_WR_ACCESS  (1 << 1)

const unsigned char aw8896_reg_access[AW8896_REG_MAX] = {
	[AW8896_REG_ID] = REG_RD_ACCESS,
	[AW8896_REG_SYSST] = REG_RD_ACCESS,
	[AW8896_REG_SYSINT] = REG_RD_ACCESS,
	[AW8896_REG_SYSINTM] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_SYSCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_I2SCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_I2STXCFG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_DSPCFG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_PWMCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_HAGCCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_HAGCCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_DBGCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_I2SCFG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_I2SSTAT] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_I2SCAPCNT] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_DSPMADD] = REG_NONE_ACCESS,
	[AW8896_REG_DSPMDAT] = REG_NONE_ACCESS,
	[AW8896_REG_WDT] = REG_RD_ACCESS,
	[AW8896_REG_GENCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_BSTCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_PLLCTRL1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_PLLCTRL2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_TESTCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_AMPDBG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_AMPDBG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW8896_REG_BSTDBG1] = REG_RD_ACCESS | REG_WR_ACCESS,
};

/*
 * Register Detail
 */

/* SYSST */
#define AW8896_BIT_SYSST_PLLS                       (1 << 0)
#define AW8896_BIT_SYSST_OTHS                       (1 << 1)
#define AW8896_BIT_SYSST_OTLS                       (1 << 2)
#define AW8896_BIT_SYSST_OCDS                       (1 << 3)
#define AW8896_BIT_SYSST_CLKS                       (1 << 4)
#define AW8896_BIT_SYSST_NOCLKS                     (1 << 5)
#define AW8896_BIT_SYSST_WDS                        (1 << 6)
#define AW8896_BIT_SYSST_CLIPS                      (1 << 7)
#define AW8896_BIT_SYSST_SWS                        (1 << 8)
#define AW8896_BIT_SYSST_BSTS                       (1 << 9)
#define AW8896_BIT_SYSST_OVPS                       (1 << 10)
#define AW8896_BIT_SYSST_BSTOCS                     (1 << 11)
#define AW8896_BIT_SYSST_DSPS                       (1 << 12)
#define AW8896_BIT_SYSST_ADPS                       (1 << 13)
#define AW8896_BIT_SYSST_UVLOS                      (1 << 14)

/* SYSINT */
#define AW8896_BIT_SYSINT_PLLI                      (1 << 0)
#define AW8896_BIT_SYSINT_OTHI                      (1 << 1)
#define AW8896_BIT_SYSINT_OTLI                      (1 << 2)
#define AW8896_BIT_SYSINT_OCDI                      (1 << 3)
#define AW8896_BIT_SYSINT_CLKI                      (1 << 4)
#define AW8896_BIT_SYSINT_NOCLKI                    (1 << 5)
#define AW8896_BIT_SYSINT_WDI                       (1 << 6)
#define AW8896_BIT_SYSINT_CLIPI                     (1 << 7)
#define AW8896_BIT_SYSINT_SWI                       (1 << 8)
#define AW8896_BIT_SYSINT_BSTI                      (1 << 9)
#define AW8896_BIT_SYSINT_OVPI                      (1 << 10)
#define AW8896_BIT_SYSINT_BSTOCI                    (1 << 11)
#define AW8896_BIT_SYSINT_DSPI                      (1 << 12)
#define AW8896_BIT_SYSINT_ADPI                      (1 << 13)
#define AW8896_BIT_SYSINT_UVLOI                     (1 << 14)

/* SYSINTM */
#define AW8896_BIT_SYSINTM_PLLM                     (1 << 0)
#define AW8896_BIT_SYSINTM_OTHM                     (1 << 1)
#define AW8896_BIT_SYSINTM_OTLM                     (1 << 2)
#define AW8896_BIT_SYSINTM_OCDM                     (1 << 3)
#define AW8896_BIT_SYSINTM_CLKM                     (1 << 4)
#define AW8896_BIT_SYSINTM_NOCLKM                   (1 << 5)
#define AW8896_BIT_SYSINTM_WDM                      (1 << 6)
#define AW8896_BIT_SYSINTM_CLIPM                    (1 << 7)
#define AW8896_BIT_SYSINTM_SWM                      (1 << 8)
#define AW8896_BIT_SYSINTM_BSTM                     (1 << 9)
#define AW8896_BIT_SYSINTM_OVPM                     (1 << 10)
#define AW8896_BIT_SYSINTM_BSTOCM                   (1 << 11)
#define AW8896_BIT_SYSINTM_DSPM                     (1 << 12)
#define AW8896_BIT_SYSINTM_ADPM                     (1 << 13)
#define AW8896_BIT_SYSINTM_UVLOM                    (1 << 14)

/* SYSCTRL */
#define AW8896_BIT_SYSCTRL_PWDN                     (1 << 0)
#define AW8896_BIT_SYSCTRL_BSTPD                    (1 << 1)
#define AW8896_BIT_SYSCTRL_DSPBY                    (1 << 2)
#define AW8896_BIT_SYSCTRL_IPLL                     (1 << 3)
#define AW8896_BIT_SYSCTRL_BCKINV                   (1 << 4)
#define AW8896_BIT_SYSCTRL_WSINV                    (1 << 5)
#define AW8896_BIT_SYSCTRL_I2SEN                    (1 << 6)
#define AW8896_BIT_SYSCTRL_RCV_MODE                 (1 << 7)
#define AW8896_BIT_SYSCTRL_INT_LOW_LVL              (1 << 8)
#define AW8896_BIT_SYSCTRL_INT_OD                   (1 << 9)

/* I2SCTRL */
#define AW8896_BIT_I2SCTRL_SR_MASK                  (15 << 0)
#define AW8896_BIT_I2SCTRL_SR_8K                    (0 << 0)
#define AW8896_BIT_I2SCTRL_SR_11K                   (1 << 0)
#define AW8896_BIT_I2SCTRL_SR_12K                   (2 << 0)
#define AW8896_BIT_I2SCTRL_SR_16K                   (3 << 0)
#define AW8896_BIT_I2SCTRL_SR_22K                   (4 << 0)
#define AW8896_BIT_I2SCTRL_SR_24K                   (5 << 0)
#define AW8896_BIT_I2SCTRL_SR_32K                   (6 << 0)
#define AW8896_BIT_I2SCTRL_SR_44K                   (7 << 0)
#define AW8896_BIT_I2SCTRL_SR_48K                   (8 << 0)
#define AW8896_BIT_I2SCTRL_BCK_MASK                 (3 << 4)
#define AW8896_BIT_I2SCTRL_BCK_32FS                 (0 << 4)
#define AW8896_BIT_I2SCTRL_BCK_48FS                 (1 << 4)
#define AW8896_BIT_I2SCTRL_BCK_64FS                 (2 << 4)
#define AW8896_BIT_I2SCTRL_FMS_MASK                 (3 << 6)
#define AW8896_BIT_I2SCTRL_FMS_16BIT                (0 << 6)
#define AW8896_BIT_I2SCTRL_FMS_20BIT                (1 << 6)
#define AW8896_BIT_I2SCTRL_FMS_24BIT                (2 << 6)
#define AW8896_BIT_I2SCTRL_FMS_32BIT                (3 << 6)
#define AW8896_BIT_I2SCTRL_MD_MASK                  (3 << 8)
#define AW8896_BIT_I2SCTRL_MD_STD                   (0 << 8)
#define AW8896_BIT_I2SCTRL_MD_MSB                   (1 << 8)
#define AW8896_BIT_I2SCTRL_MD_LSB                   (2 << 8)
#define AW8896_BIT_I2SCTRL_CHS_MASK                 (3 << 10)
#define AW8896_BIT_I2SCTRL_CHS_LEFT                 (1 << 10)
#define AW8896_BIT_I2SCTRL_CHS_RIGHT                (2 << 10)
#define AW8896_BIT_I2SCTRL_CHS_MONO                 (3 << 10)
#define AW8896_BIT_I2SCTRL_STEREO_EN                (1 << 12)
#define AW8896_BIT_I2SCTRL_INPLEV                   (1 << 13)

/* I2STXCFG */
#define AW8896_BIT_I2STXCFG_TXEN                    (1 << 0)
#define AW8896_BIT_I2STXCFG_CHS                     (1 << 1)
#define AW8896_BIT_I2STXCFG_DSEL_MASK               (3 << 2)
#define AW8896_BIT_I2STXCFG_DSEL_ZERO               (0 << 2)
#define AW8896_BIT_I2STXCFG_DSEL_GAIN               (1 << 2)
#define AW8896_BIT_I2STXCFG_DSEL_DSP                (2 << 2)
#define AW8896_BIT_I2STXCFG_DOHZ                    (1 << 4)
#define AW8896_BIT_I2STXCFG_DRVSTREN                (1 << 5)

/* DSPCFG */
#define AW8896_BIT_DSPCFG_SMUTE                     (1 << 0)
#define AW8896_BIT_DSPCFG_SMAT_MASK                 (63 << 1)
#define AW8896_BIT_DSPCFG_SMAT_UNIT                 (1 << 1)
#define AW8896_BIT_DSPCFG_SMODE_8SMP                (1 << 7)
#define AW8896_BIT_DSPCFG_VOL_MASK                  (127 << 8)
#define AW8896_BIT_DSPCFG_VOL_UNIT                  (1 << 8)
#define AW8896_VOLUME_MAX                           (0)
#define AW8896_VOLUME_MIN                           (-200)
#define AW8896_VOL_REG_SHIFT                        (8)

/* PWMCTRL */
#define AW8896_BIT_PWMCTRL_HMUTE                    (1 << 0)
#define AW8896_BIT_PWMCTRL_HDCCE                    (1 << 1)
#define AW8896_BIT_PWMCTRL_PWMRES_8BIT              (1 << 2)
#define AW8896_BIT_PWMCTRL_PWMSH_TRIG               (1 << 3)
#define AW8896_BIT_PWMCTRL_PWMDELB_MASK             (15 << 4)
#define AW8896_BIT_PWMCTRL_PWMDELB_UNIT             (1 << 4)
#define AW8896_BIT_PWMCTRL_PWMDELA_MASK             (15 << 8)
#define AW8896_BIT_PWMCTRL_PWMDELA_UNIT             (1 << 8)
#define AW8896_BIT_PWMCTRL_DSMZTH_MASK              (15 << 12)
#define AW8896_BIT_PWMCTRL_DSMZTH_UNIT              (1 << 12)

/* HAGCCFG1 */
#define AW8896_BIT_HAGCCFG1_ATTH_MASK               (255 << 0)
#define AW8896_BIT_HAGCCFG1_ATTH_UNIT               (1 << 0)
#define AW8896_BIT_HAGCCFG1_HAGCEN                  (1 << 8)
#define AW8896_BIT_HAGCCFG1_PWMPSC_MASK             (31 << 9)
#define AW8896_BIT_HAGCCFG1_PWMPSC_UNIT             (1 << 9)
#define AW8896_BIT_HAGCCFG1_SRAMPD                  (1 << 14)

/* HAGCCFG2 */
#define AW8896_BIT_HAGCCFG2_HOLDTH_MASK             (255 << 0)
#define AW8896_BIT_HAGCCFG2_HOLDTH_UNIT             (1 << 0)
#define AW8896_BIT_HAGCCFG2_RTTH_MASK               (255 << 8)
#define AW8896_BIT_HAGCCFG2_RTTH_UNIT               (1 << 8)

/* DBGCTRL */
#define AW8896_BIT_DBGCTRL_SYSCE                    (1 << 0)
#define AW8896_BIT_DBGCTRL_DSPFCE                   (1 << 1)
#define AW8896_BIT_DBGCTRL_DSPDCE                   (1 << 2)
#define AW8896_BIT_DBGCTRL_DSPDRST                  (1 << 3)
#define AW8896_BIT_DBGCTRL_SYSRST                   (1 << 4)
#define AW8896_BIT_DBGCTRL_I2SRST                   (1 << 5)
#define AW8896_BIT_DBGCTRL_PLLPD                    (1 << 6)
#define AW8896_BIT_DBGCTRL_AMPPD                    (1 << 7)
#define AW8896_BIT_DBGCTRL_OSCPD                    (1 << 8)
#define AW8896_BIT_DBGCTRL_CLKMD                    (1 << 9)
#define AW8896_BIT_DBGCTRL_DISPLLRST                (1 << 10)
#define AW8896_BIT_DBGCTRL_DISNCKRST                (1 << 11)
#define AW8896_BIT_DBGCTRL_NAMUTE                   (1 << 12)
#define AW8896_BIT_DBGCTRL_PDUVL                    (1 << 13)
#define AW8896_BIT_DBGCTRL_LPBK_FARE                (1 << 14)
#define AW8896_BIT_DBGCTRL_LPBK_NEARE               (1 << 15)

/* I2SCFG */
#define AW8896_BIT_I2SCFG_I2SRXEN                   (1 << 0)
#define AW8896_BIT_I2SCFG_TX_THR_MASK               (3 << 8)
#define AW8896_BIT_I2SCFG_TX_THR0                   (0 << 8)
#define AW8896_BIT_I2SCFG_TX_THR1                   (1 << 8)
#define AW8896_BIT_I2SCFG_TX_THR2                   (2 << 8)
#define AW8896_BIT_I2SCFG_TX_THR3                   (3 << 8)
#define AW8896_BIT_I2SCFG_TX_FLS                    (1 << 11)
#define AW8896_BIT_I2SCFG_RX_THR_MASK               (3 << 12)
#define AW8896_BIT_I2SCFG_RX_THR0                   (0 << 12)
#define AW8896_BIT_I2SCFG_RX_THR1                   (1 << 12)
#define AW8896_BIT_I2SCFG_RX_THR2                   (2 << 12)
#define AW8896_BIT_I2SCFG_RX_THR3                   (3 << 12)
#define AW8896_BIT_I2SCFG_RX_FLS                    (1 << 15)

/* I2SSAT */
#define AW8896_BIT_I2SSAT_I2STOVS                   (1 << 0)
#define AW8896_BIT_I2SSAT_I2SROVS                   (1 << 1)
#define AW8896_BIT_I2SSAT_DPSTAT                    (1 << 2)

/* WDT */
#define AW8896_BIT_WDT_WDT_MASK                     (255 << 0)

/* GENCTRL */
#define AW8896_BIT_GENCTRL_BSTVOUT_MASK             (15 << 0)
#define AW8896_BIT_GENCTRL_BSTVOUT_5V               (0 << 0)
#define AW8896_BIT_GENCTRL_BSTVOUT_5P25V            (1 << 0)
#define AW8896_BIT_GENCTRL_BSTVOUT_5P5V             (1 << 0)
#define AW8896_BIT_GENCTRL_BSTVOUT_5P75V            (1 << 0)
#define AW8896_BIT_GENCTRL_BSTVOUT_6V               (1 << 0)
#define AW8896_BIT_GENCTRL_BSTVOUT_6P25V            (1 << 0)
#define AW8896_BIT_GENCTRL_BSTVOUT_6P5V             (1 << 0)
#define AW8896_BIT_GENCTRL_BSTVOUT_6P75V            (1 << 0)
#define AW8896_BIT_GENCTRL_BSTVOUT_7V               (1 << 0)
#define AW8896_BIT_GENCTRL_BSTVOUT_7P25V            (1 << 0)
#define AW8896_BIT_GENCTRL_BSTVOUT_7P5V             (1 << 0)
#define AW8896_BIT_GENCTRL_BSTILIMIT_MASK           (7 << 4)
#define AW8896_BIT_GENCTRL_BSTILIMIT_2P5A           (0 << 4)
#define AW8896_BIT_GENCTRL_BSTILIMIT_2P75A          (1 << 4)
#define AW8896_BIT_GENCTRL_BSTILIMIT_3A             (2 << 4)
#define AW8896_BIT_GENCTRL_BSTILIMIT_3P25A          (3 << 4)
#define AW8896_BIT_GENCTRL_BSTILIMIT_3P5A           (4 << 4)
#define AW8896_BIT_GENCTRL_BSTILIMIT_3P75A          (5 << 4)
#define AW8896_BIT_GENCTRL_BSTILIMIT_4A             (6 << 4)
#define AW8896_BIT_GENCTRL_EN_MPD                   (1 << 7)
#define AW8896_BIT_GENCTRL_MPD_ATH_MASK             (3 << 8)
#define AW8896_BIT_GENCTRL_MPD_ATH_0P008            (0 << 8)
#define AW8896_BIT_GENCTRL_MPD_ATH_0P016            (1 << 8)
#define AW8896_BIT_GENCTRL_MPD_ATH_0P032            (2 << 8)
#define AW8896_BIT_GENCTRL_MPD_ATH_0P047            (3 << 8)
#define AW8896_BIT_GENCTRL_MPD_RTH_MASK             (3 << 10)
#define AW8896_BIT_GENCTRL_MPD_RTH_0P016            (1 << 10)
#define AW8896_BIT_GENCTRL_MPD_RTH_0P032            (1 << 10)
#define AW8896_BIT_GENCTRL_MPD_RTH_0P047            (2 << 10)
#define AW8896_BIT_GENCTRL_MPD_RTH_0P063            (3 << 10)
#define AW8896_BIT_GENCTRL_CLIP_BP                  (1 << 12)
#define AW8896_BIT_GENCTRL_PWM_DIFF_DIS             (1 << 13)
#define AW8896_BIT_GENCTRL_BURST_PEAK_MASK          (3 << 14)
#define AW8896_BIT_GENCTRL_BURST_PEAK_120mA         (0 << 14)
#define AW8896_BIT_GENCTRL_BURST_PEAK_100mA         (1 << 14)
#define AW8896_BIT_GENCTRL_BURST_PEAK_150mA         (2 << 14)
#define AW8896_BIT_GENCTRL_BURST_PEAK_190mA         (3 << 14)

/* BSTCTRL */
#define AW8896_BIT_BSTCTRL_ATH_MASK                 (15 << 0)
#define AW8896_BIT_BSTCTRL_RTH_MASK                 (15 << 4)
#define AW8896_BIT_BSTCTRL_TDEG_MASK                (7 << 8)
#define AW8896_BIT_BSTCTRL_TDEG_21MS                (0 << 8)
#define AW8896_BIT_BSTCTRL_TDEG_42MS                (1 << 8)
#define AW8896_BIT_BSTCTRL_TDEG_84MS                (2 << 8)
#define AW8896_BIT_BSTCTRL_TDEG_168MS               (3 << 8)
#define AW8896_BIT_BSTCTRL_TDEG_336MS               (4 << 8)
#define AW8896_BIT_BSTCTRL_TDEG_672MS               (5 << 8)
#define AW8896_BIT_BSTCTRL_TDEG_1344MS              (6 << 8)
#define AW8896_BIT_BSTCTRL_TDEG_2688MS              (7 << 8)
#define AW8896_BIT_BSTCTRL_MODE_MASK                (3 << 11)
#define AW8896_BIT_BSTCTRL_MODE_TRSP                (0 << 11)
#define AW8896_BIT_BSTCTRL_MODE_FRBST               (1 << 11)
#define AW8896_BIT_BSTCTRL_MODE_ADPBST              (2 << 11)
#define AW8896_BIT_BSTCTRL_MODE_TESTBST             (3 << 11)
#define AW8896_BIT_BSTCTRL_OCAP_MASK                (1 << 13)
#define AW8896_BIT_BSTCTRL_OCAP_FAST                (0 << 13)
#define AW8896_BIT_BSTCTRL_OCAP_SLOW                (1 << 13)
#define AW8896_BIT_BSTCTRL_VMAX_MASK                (3 << 14)
#define AW8896_BIT_BSTCTRL_VMAX_2V                  (0 << 14)
#define AW8896_BIT_BSTCTRL_VMAX_2P1V                (1 << 14)
#define AW8896_BIT_BSTCTRL_VMAX_2P2V                (2 << 14)
#define AW8896_BIT_BSTCTRL_VMAX_1P9V                (3 << 14)

/* PLLCTRL1 */
#define AW8896_BIT_PLLCTRL1_ICP1_MASK               (31 << 0)
#define AW8896_BIT_PLLCTRL1_ICP2_MASK               (15 << 5)
#define AW8896_BIT_PLLCTRL1_PLLDBG                  (1 << 9)
#define AW8896_BIT_PLLCTRL1_BPQA                    (1 << 10)
#define AW8896_BIT_PLLCTRL1_BPQB                    (1 << 11)
#define AW8896_BIT_PLLCTRL1_PFD_DLY_MASK            (3 << 12)
#define AW8896_BIT_PLLCTRL1_PFD_DLY_5NS             (0 << 12)
#define AW8896_BIT_PLLCTRL1_PFD_DLY_10NS            (1 << 12)
#define AW8896_BIT_PLLCTRL1_PFD_DLY_20NS            (2 << 12)
#define AW8896_BIT_PLLCTRL1_PFD_DLY_40NS            (3 << 12)
#define AW8896_BIT_PLLCTRL1_TOE                     (1 << 14)
#define AW8896_BIT_PLLCTRL1_TOS_8_DIV               (1 << 15)

/* PLLCTRL2 */
#define AW8896_BIT_PLLCTRL2_RSEL_MASK               (255 << 0)
#define AW8896_BIT_PLLCTRL2_IDFT_MASK               (31 << 8)
#define AW8896_BIT_PLLCTRL2_DFT                     (1 << 14)

/* AMPDBG1 */
#define AW8896_BIT_AMPDBG1_RFB_MASK                 (7 << 0)
#define AW8896_BIT_AMPDBG1_IPWM_20UA                (1 << 3)
#define AW8896_BIT_AMPDBG1_CAP                      (1 << 4)
#define AW8896_BIT_AMPDBG1_R2_MASK                  (3 << 5)
#define AW8896_BIT_AMPDBG1_VCOM                     (1 << 7)
#define AW8896_BIT_AMPDBG1_POPT_MASK                (3 << 8)
#define AW8896_BIT_AMPDBG1_POPT_0P4MS               (0 << 8)
#define AW8896_BIT_AMPDBG1_POPT_1P6MS               (1 << 8)
#define AW8896_BIT_AMPDBG1_POPT_6P4MS               (2 << 8)
#define AW8896_BIT_AMPDBG1_POPT_12P8MS              (3 << 8)
#define AW8896_BIT_AMPDBG1_OPD                      (1 << 10)

/* AMPDBG2 */
#define AW8896_BIT_AMPDBG2_TOCP_MASK                (3 << 0)
#define AW8896_BIT_AMPDBG2_TOCP_200MS               (0 << 0)
#define AW8896_BIT_AMPDBG2_TOCP_800MS               (1 << 0)
#define AW8896_BIT_AMPDBG2_TOCP_1600MS              (2 << 0)
#define AW8896_BIT_AMPDBG2_TOCP_NO                  (3 << 0)
#define AW8896_BIT_AMPDBG2_IOC_MASK                 (3 << 2)
#define AW8896_BIT_AMPDBG2_IOC_1P8A                 (0 << 2)
#define AW8896_BIT_AMPDBG2_IOC_2P3A                 (1 << 2)
#define AW8896_BIT_AMPDBG2_IOC_3A                   (2 << 2)
#define AW8896_BIT_AMPDBG2_IOC_3P5A                 (3 << 2)
#define AW8896_BIT_AMPDBG2_OCDT_MASK                (3 << 4)
#define AW8896_BIT_AMPDBG2_OCDT_25NS                (0 << 4)
#define AW8896_BIT_AMPDBG2_OCDT_50NS                (1 << 4)
#define AW8896_BIT_AMPDBG2_OCDT_80NS                (2 << 4)
#define AW8896_BIT_AMPDBG2_OCDT_120NS               (3 << 4)
#define AW8896_BIT_AMPDBG2_OCSWD                    (1 << 6)
#define AW8896_BIT_AMPDBG2_SR_CTRL_MASK             (3 << 7)
#define AW8896_BIT_AMPDBG2_SR_CTRL_25NS             (0 << 7)
#define AW8896_BIT_AMPDBG2_SR_CTRL_15NS             (1 << 7)
#define AW8896_BIT_AMPDBG2_SR_CTRL_10NS             (2 << 7)
#define AW8896_BIT_AMPDBG2_SR_CTRL_5NS              (3 << 7)
#define AW8896_BIT_AMPDBG2_IDAC_SPLIT               (1 << 9)
#define AW8896_BIT_AMPDBG2_PLL_PREDIV_MASK          (3 << 10)
#define AW8896_BIT_AMPDBG2_PLL_FBDIV_MASK           (3 << 12)
#define AW8896_BIT_AMPDBG2_AMP_OCPF                 (1 << 15)

/* BSTDBG1 */
#define AW8896_BIT_BSTDBG1_BURST_IN_DLY_MASK        (3 << 0)
#define AW8896_BIT_BSTDBG1_BURST_IN_DLY_8US         (0 << 0)
#define AW8896_BIT_BSTDBG1_BURST_IN_DLY_12US        (1 << 0)
#define AW8896_BIT_BSTDBG1_BURST_IN_DLY_4US         (2 << 0)
#define AW8896_BIT_BSTDBG1_BURST_IN_DLY_2US         (3 << 0)
#define AW8896_BIT_BSTDBG1_BURST_OUT_DLY_MASK       (3 << 2)
#define AW8896_BIT_BSTDBG1_BURST_OUT_DLY_2US        (0 << 2)
#define AW8896_BIT_BSTDBG1_BURST_OUT_DLY_4US        (1 << 2)
#define AW8896_BIT_BSTDBG1_BURST_OUT_DLY_1P3US      (2 << 2)
#define AW8896_BIT_BSTDBG1_BURST_OUT_DLY_1US        (3 << 2)
#define AW8896_BIT_BSTDBG1_NONLAP                   (1 << 4)
#define AW8896_BIT_BSTDBG1_OVP_DEG                  (1 << 5)
#define AW8896_BIT_BSTDBG1_OVP_1P1_TIME             (1 << 6)
#define AW8896_BIT_BSTDBG1_ENDLY_MASK               (3 << 7)
#define AW8896_BIT_BSTDBG1_ENDLY_8NS                (0 << 7)
#define AW8896_BIT_BSTDBG1_ENDLY_80NS               (1 << 7)
#define AW8896_BIT_BSTDBG1_ENDLY_130NS              (2 << 7)
#define AW8896_BIT_BSTDBG1_ENDLY_200NS              (3 << 7)
#define AW8896_BIT_BSTDBG1_ITH_MASK                 (3 << 9)
#define AW8896_BIT_BSTDBG1_ITH_3P5A                 (0 << 9)
#define AW8896_BIT_BSTDBG1_ITH_4P2A                 (1 << 9)
#define AW8896_BIT_BSTDBG1_ITH_4P9A                 (2 << 9)
#define AW8896_BIT_BSTDBG1_ITH_5P6A                 (3 << 9)
#define AW8896_BIT_BSTDBG1_SOFTDLY_MASK             (3 << 11)
#define AW8896_BIT_BSTDBG1_SOFTDLY_120US            (0 << 11)
#define AW8896_BIT_BSTDBG1_SOFTDLY_240US            (1 << 11)
#define AW8896_BIT_BSTDBG1_SOFTDLY_80US             (2 << 11)
#define AW8896_BIT_BSTDBG1_SOFTDLY_60US             (3 << 11)
#define AW8896_BIT_BSTDBG1_VTH_MASK                 (3 << 13)
#define AW8896_BIT_BSTDBG1_VTH_0P64                 (0 << 13)
#define AW8896_BIT_BSTDBG1_VTH_0P86                 (1 << 13)
#define AW8896_BIT_BSTDBG1_VTH_1P1                  (2 << 13)
#define AW8896_BIT_BSTDBG1_VTH_1P65                 (3 << 13)

#endif
