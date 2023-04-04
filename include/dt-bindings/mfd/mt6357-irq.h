/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _DT_BINDINGS_MT6357_IRQ_H
#define _DT_BINDINGS_MT6357_IRQ_H

#define SP_BUCK   0
#define SP_LDO    1
#define SP_PSC    2
#define SP_SCK    3
#define SP_BM     4
#define SP_HK     5
#define SP_AUD    6
#define SP_MISC   7


#define INT_VPROC_OC                  0
#define INT_VCORE_OC                  1
#define INT_VMODEM_OC                 2
#define INT_VS1_OC                    3
#define INT_VPA_OC                    4
#define INT_VCORE_PREOC               5
#define INT_VFE28_OC                  16
#define INT_VXO22_OC                  17
#define INT_VRF18_OC                  18
#define INT_VRF12_OC                  19
#define INT_VEFUSE_OC                 20
#define INT_VCN33_OC                  21
#define INT_VCN28_OC                  22
#define INT_VCN18_OC                  23
#define INT_VCAMA_OC                  24
#define INT_VCAMD_OC                  25
#define INT_VCAMIO_OC                 26
#define INT_VLDO28_OC                 27
#define INT_VUSB33_OC                 28
#define INT_VAUX18_OC                 29
#define INT_VAUD28_OC                 30
#define INT_VIO28_OC                  31
#define INT_VIO18_OC                  32
#define INT_VSRAM_PROC_OC             33
#define INT_VSRAM_OTHERS_OC           34
#define INT_VIBR_OC                   35
#define INT_VDRAM_OC                  36
#define INT_VMC_OC                    37
#define INT_VMCH_OC                   38
#define INT_VEMC_OC                   39
#define INT_VSIM1_OC                  40
#define INT_VSIM2_OC                  41
#define INT_PWRKEY                    48
#define INT_HOMEKEY                   49
#define INT_PWRKEY_R                  50
#define INT_HOMEKEY_R                 51
#define INT_NI_LBAT_INT               52
#define INT_CHRDET                    53
#define INT_CHRDET_EDGE               54
#define INT_VCDT_HV_DET               55
#define INT_WATCHDOG                  58
#define INT_VBATON_UNDET              59
#define INT_BVALID_DET                60
#define INT_OV                        61
#define INT_RTC                       64
#define INT_FG_BAT0_H                 80
#define INT_FG_BAT0_L                 81
#define INT_FG_CUR_H                  82
#define INT_FG_CUR_L                  83
#define INT_FG_ZCV                    84
#define INT_BATON_LV                  96
#define INT_BATON_HT                  97
#define INT_BAT_H                     114
#define INT_BAT_L                     115
#define INT_AUXADC_IMP                120
#define INT_NAG_C_DLTV                121
#define INT_AUDIO                     128
#define INT_ACCDET                    133
#define INT_ACCDET_EINT0              134
#define INT_ACCDET_EINT1              135
#define INT_SPI_CMD_ALERT             144

#endif /* _DT_BINDINGS_MT6357_IRQ_H */
