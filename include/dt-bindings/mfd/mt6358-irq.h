/*
 * Copyright (C) 2018 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _DT_BINDINGS_MT6358_IRQ_H
#define _DT_BINDINGS_MT6358_IRQ_H

#define SP_BUCK   0
#define SP_LDO    1
#define SP_PSC    2
#define SP_SCK    3
#define SP_BM     4
#define SP_HK     5
#define SP_AUD    6
#define SP_MISC   7


#define INT_VPROC11_OC                0
#define INT_VPROC12_OC                1
#define INT_VCORE_OC                  2
#define INT_VGPU_OC                   3
#define INT_VMODEM_OC                 4
#define INT_VDRAM1_OC                 5
#define INT_VS1_OC                    6
#define INT_VS2_OC                    7
#define INT_VPA_OC                    8
#define INT_VCORE_PREOC               9
#define INT_VFE28_OC                  16
#define INT_VXO22_OC                  17
#define INT_VRF18_OC                  18
#define INT_VRF12_OC                  19
#define INT_VEFUSE_OC                 20
#define INT_VCN33_OC                  21
#define INT_VCN28_OC                  22
#define INT_VCN18_OC                  23
#define INT_VCAMA1_OC                 24
#define INT_VCAMA2_OC                 25
#define INT_VCAMD_OC                  26
#define INT_VCAMIO_OC                 27
#define INT_VLDO28_OC                 28
#define INT_VA12_OC                   29
#define INT_VAUX18_OC                 30
#define INT_VAUD28_OC                 31
#define INT_VIO28_OC                  32
#define INT_VIO18_OC                  33
#define INT_VSRAM_PROC11_OC           34
#define INT_VSRAM_PROC12_OC           35
#define INT_VSRAM_OTHERS_OC           36
#define INT_VSRAM_GPU_OC              37
#define INT_VDRAM2_OC                 38
#define INT_VMC_OC                    39
#define INT_VMCH_OC                   40
#define INT_VEMC_OC                   41
#define INT_VSIM1_OC                  42
#define INT_VSIM2_OC                  43
#define INT_VIBR_OC                   44
#define INT_VUSB_OC                   45
#define INT_VBIF28_OC                 46
#define INT_PWRKEY                    48
#define INT_HOMEKEY                   49
#define INT_PWRKEY_R                  50
#define INT_HOMEKEY_R                 51
#define INT_NI_LBAT_INT               52
#define INT_CHRDET                    53
#define INT_CHRDET_EDGE               54
#define INT_VCDT_HV_DET               55
#define INT_RTC                       64
#define INT_FG_BAT0_H                 80
#define INT_FG_BAT0_L                 81
#define INT_FG_CUR_H                  82
#define INT_FG_CUR_L                  83
#define INT_FG_ZCV                    84
#define INT_FG_BAT1_H                 85
#define INT_FG_BAT1_L                 86
#define INT_FG_N_CHARGE_L             87
#define INT_FG_IAVG_H                 88
#define INT_FG_IAVG_L                 89
#define INT_FG_TIME_H                 90
#define INT_FG_DISCHARGE              91
#define INT_FG_CHARGE                 92
#define INT_BATON_LV                  96
#define INT_BATON_HT                  97
#define INT_BATON_BAT_IN              98
#define INT_BATON_BAT_OUT             99
#define INT_BIF                       100
#define INT_BAT_H                     112
#define INT_BAT_L                     113
#define INT_BAT2_H                    114
#define INT_BAT2_L                    115
#define INT_BAT_TEMP_H                116
#define INT_BAT_TEMP_L                117
#define INT_AUXADC_IMP                118
#define INT_NAG_C_DLTV                119
#define INT_AUDIO                     128
#define INT_ACCDET                    133
#define INT_ACCDET_EINT0              134
#define INT_ACCDET_EINT1              135
#define INT_SPI_CMD_ALERT             144

#endif /* _DT_BINDINGS_MT6358_IRQ_H */
