/*
 *  Silicon Integrated Co., Ltd haptic sih688x register list
 *
 *  Copyright (c) 2021 kugua <canzhen.peng@si-in.com>
 *  Copyright (c) 2021 tianchi <tianchi.zheng@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#ifndef _SIH688X_REG_H_
#define _SIH688X_REG_H_

/********************************************
 * Register Addr List
 *******************************************/
#define SIH688X_REG_ID                      0x00
#define SHI688X_REG_CHIP_REV_HIGH           0x01
#define SIH688X_REG_SYSINT                  0x02
#define SIH688X_REG_SYSINTM                 0x03
#define SIH688X_REG_SYSCTRL1                0x04
#define SIH688X_REG_RL_VBAT_CTRL            0x05
#define SIH688X_REG_SYSCTRL2                0x06
#define SIH688X_REG_PWM_PRE_GAIN            0x07
#define SIH688X_REG_CHIP_REV_LOW            0x08
#define SIH688X_REG_SYSINT2                 0x09
#define SIH688X_REG_SYSINTM2                0x0a
#define SIH688X_REG_IDLE_DEL_CNT            0x0b
#define SIH688X_REG_MAIN_STATE_CTRL         0x0c
#define SIH688X_REG_INT_STATUS2             0x0d
#define SIH688X_REG_SYSSST                  0x0e
#define SIH688X_REG_GO                      0x0f

#define SIH688X_REG_WAVESEQ1                0x10
#define SIH688X_REG_WAVESEQ2                0x11
#define SIH688X_REG_WAVESEQ3                0x12
#define SIH688X_REG_WAVESEQ4                0x13
#define SIH688X_REG_WAVESEQ5                0x14
#define SIH688X_REG_WAVESEQ6                0x15
#define SIH688X_REG_WAVESEQ7                0x16
#define SIH688X_REG_WAVESEQ8                0x17
#define SIH688X_REG_WAVELOOP1               0x18
#define SIH688X_REG_WAVELOOP2               0x19
#define SIH688X_REG_WAVELOOP3               0x1a
#define SIH688X_REG_WAVELOOP4               0x1b
#define SIH688X_REG_MAINLOOP                0x1c

#define SIH688X_REG_BASE_ADDRH              0x20
#define SIH688X_REG_BASE_ADDRL              0x21
#define SIH688X_REG_RTPCFG1                 0x22
#define SIH688X_REG_RTPCFG2                 0x23
#define SIH688X_REG_RTPCFG3                 0x24
#define SIH688X_REG_RTPCLR                  0x26
#define SIH688X_REG_RTP_START_THRES         0x27
#define SIH688X_REG_RAMADDRH                0x28
#define SIH688X_REG_RAMADDRL                0x29
#define SIH688X_REG_RAMDATA                 0x2a
#define SIH688X_REG_RTPDATA                 0x2b
#define SIH688X_REG_PVDD_DOWN_THRES         0x2c
#define SIH688X_REG_PVDD_UP_THRES           0x2d

#define SIH688X_REG_PVDD_PROT_OUT_THRES     0x32
#define SIH688X_REG_PWM_PROT_OUT_NUM        0x33
#define SIH688X_REG_PWM_UP_SAMPLE_CTRL      0x34
#define SIH688X_REG_TRIG_CTRL1              0x35
#define SIH688X_REG_TRIG_CTRL2              0x36
#define SIH688X_REG_TRIG0_PACK_P            0x37
#define SIH688X_REG_TRIG0_PACK_N            0x38
#define SIH688X_REG_TRIG1_PACK_P            0x39
#define SIH688X_REG_TRIG1_PACK_N            0x3a
#define SIH688X_REG_TRIG2_PACK_P            0x3b
#define SIH688X_REG_TRIG2_PACK_N            0x3c

#define SIH688X_REG_ALARM_REC_CNT0          0x3d
#define SIH688X_REG_ALARM_REC_CNT1          0x3e
#define SIH688X_REG_ALARM_FILTER_CNT        0x3f

#define SIH688X_REG_ANA_CTRL0               0x40
#define SIH688X_REG_ANA_CTRL1               0x41
#define SIH688X_REG_ANA_CTRL2               0x42
#define SIH688X_REG_ANA_CTRL3               0x43
#define SIH688X_REG_ANA_CTRL4               0x44
#define SIH688X_REG_ANA_CTRL5               0x45
#define SIH688X_REG_ANA_CTRL6               0x46
#define SIH688X_REG_ANA_CTRL7               0x47
#define SIH688X_REG_ANA_CTRL8               0x48
#define SIH688X_REG_ANA_CTRL9               0x49
#define SIH688X_REG_ANA_CTRLA               0x4a
#define SIH688X_REG_ANA_CTRLB               0x4b
#define SIH688X_REG_ANA_CTRLC               0x4c
#define SIH688X_REG_ANA_CTRLD               0x4d
#define SIH688X_REG_ANA_CTRLE               0x4e
#define SIH688X_REG_ANA_CTRLF               0x4f
#define SIH688X_REG_ANA_CTRL10              0x50

#define SIH688X_REG_TEST_MODE_EN            0x51
#define SIH688X_REG_DBG_MODE_EN             0x52
#define SIH688X_REG_DEL_LEVEL_VAL           0x53
#define SIH688X_REG_DBG_1                   0x54
#define SIH688X_REG_DBG_2                   0x55
#define SIH688X_REG_DEL_LEVEL_CTRL          0x56
#define SIH688X_REG_ADC_OC_DATA_L           0x57
#define SIH688X_REG_ADC_OC_DATA_H           0x58
#define SIH688X_REG_ADC_VBAT_DATA_L         0x59

#define SIH688X_REG_ADC_RL_DATA_H           0x5a
#define SIH688X_REG_ADC_RL_DATA_L           0x5b
#define SIH688X_REG_GAIN_SET_SEQ1_0         0x5c
#define SIH688X_REG_GAIN_SET_SEQ3_2         0x5d
#define SIH688X_REG_GAIN_SET_SEQ5_4         0x5e
#define SIH688X_REG_GAIN_SET_SEQ7_6         0x5f

#define SIH688X_REG_TEST_CTRL1              0x60
#define SIH688X_REG_MBIST_CTRL              0x61
#define SIH688X_REG_PATTERN0                0x62
#define SIH688X_REG_PATTERN1                0x63
#define SIH688X_REG_ALGO_TEST_CTRL          0x64
#define SIH688X_REG_ADC_EN_CNT              0x65
#define SIH688X_REG_BRK_BOOST				0x68

#define SIH688X_REG_TRIM1                   0x6b
#define SIH688X_REG_EFUSE_RDATA4            0x6c
#define SIH688X_REG_EFUSE_RDATA5            0x6d
#define SIH688X_REG_EFUSE_RDATA6            0x6e
#define SIH688X_REG_EFUSE_RDATA7            0x6f

#define SIH688X_REG_EFUSE_CTRL              0x70
#define SIH688X_REG_EFUSE_WDATA0            0x71
#define SIH688X_REG_EFUSE_WDATA1            0x72
#define SIH688X_REG_EFUSE_WDATA2            0x73
#define SIH688X_REG_EFUSE_WDATA3            0x74
#define SIH688X_REG_EFUSE_RDATA0            0x75
#define SIH688X_REG_EFUSE_RDATA1            0x76
#define SIH688X_REG_EFUSE_RDATA2            0x77
#define SIH688X_REG_EFUSE_RDATA3            0x78
#define SIH688X_REG_EFUSE_T_CTRL0           0x79
#define SIH688X_REG_EFUSE_T_CTRL1           0x7a
#define SIH688X_REG_TRIMREG0                0x7b
#define SIH688X_REG_TRIMREG1                0x7c
#define SIH688X_REG_TRIMREG2                0x7d
#define SIH688X_REG_TRIMREG3                0x7e
#define SIH688X_REG_TRIMREG4                0x7f

#define SIH688X_REG_T_OUT_LL                0x80
#define SIH688X_REG_T_OUT_LH                0x81
#define SIH688X_REG_T_OUT_S                 0x82
#define SIH688X_REG_T_LAST_MONITOR_L        0x83
#define SIH688X_REG_T_LAST_MONITOR_H        0x84
#define SIH688X_REG_BRK_CTRL1               0x85
#define SIH688X_REG_VREF0                   0x86
#define SIH688X_REG_VREF1                   0x87
#define SIH688X_REG_VREF2                   0x88
#define SIH688X_REG_CYCLE0                  0x89
#define SIH688X_REG_CYCLE1                  0x8a
#define SIH688X_REG_CYCLE2                  0x8b
#define SIH688X_REG_THRES_STOP_0            0x8C
#define SIH688X_REG_THRES_STOP_1            0x8d
#define SIH688X_REG_THRES_STOP_2            0x8e
#define SIH688X_REG_R0_0                    0x8f

#define SIH688X_REG_R0_1                    0x90
#define SIH688X_REG_R0_2                    0x91
#define SIH688X_REG_CONST_DIV_R0_0          0x92
#define SIH688X_REG_CONST_DIV_R0_1          0x93
#define SIH688X_REG_CONST_DIV_R0_2          0x94
#define SIH688X_REG_CONST_FS_DIV_R0R1_0     0x95
#define SIH688X_REG_CONST_FS_DIV_R0R1_1     0x96
#define SIH688X_REG_CONST_FS_DIV_R0R1_2     0x97
#define SIH688X_REG_R0_DRIVER_0             0x98
#define SIH688X_REG_R0_DRIVER_1             0x99
#define SIH688X_REG_R0_DRIVER_2             0x9a
#define SIH688X_REG_R5_0                    0x9b
#define SIH688X_REG_R5_1                    0x9c
#define SIH688X_REG_R5_2                    0x9d
#define SIH688X_REG_R6_0                    0x9e
#define SIH688X_REG_R6_1                    0x9f

#define SIH688X_REG_R6_2                    0xa0
#define SIH688X_REG_R8_0                    0xa1
#define SIH688X_REG_R8_1                    0xa2
#define SIH688X_REG_R8_2                    0xa3
#define SIH688X_REG_R9_0                    0xa4
#define SIH688X_REG_R9_1                    0xa5
#define SIH688X_REG_R9_2                    0xa6
#define SIH688X_REG_R10_MUL2_0              0xa7
#define SIH688X_REG_R10_MUL2_1              0xa8
#define SIH688X_REG_R10_MUL2_2              0xa9
#define SIH688X_REG_T_0                     0xaa
#define SIH688X_REG_T_1                     0xab
#define SIH688X_REG_T_2                     0xac
#define SIH688X_REG_V_CUR_GAIN_0            0xad
#define SIH688X_REG_V_CUR_GAIN_1            0xae
#define SIH688X_REG_V_CUR_GAIN_2            0xaf

#define SIH688X_REG_V_CUR_STEP_0            0xb0
#define SIH688X_REG_V_CUR_STEP_1            0xb1
#define SIH688X_REG_V_CUR_STEP_2            0xb2
#define SIH688X_REG_G0_0                    0xb3
#define SIH688X_REG_G0_1                    0xb4
#define SIH688X_REG_G0_2                    0xb5
#define SIH688X_REG_R0_DIV_R4_0             0xb6
#define SIH688X_REG_R0_DIV_R4_1             0xb7
#define SIH688X_REG_R0_DIV_R4_2             0xb8
#define SIH688X_REG_V_BOOST                 0xb9
#define SIH688X_REG_SMOOTH_CONST_BRK_IN_0   0xba
#define SIH688X_REG_SMOOTH_CONST_BRK_IN_1   0xbb
#define SIH688X_REG_SMOOTH_CONST_BRK_IN_2   0xbc
#define SIH688X_REG_SMOOTH_CONST_P3_0       0xbd
#define SIH688X_REG_SMOOTH_CONST_P3_1       0xbe
#define SIH688X_REG_SMOOTH_CONST_P3_2       0xbf


#define SIH688X_REG_SMOOTH_F0_WINDOW_OUT    0xc2
#define SIH688X_REG_SMOOTH_CONST_ALGO_DATA_0        0xc3
#define SIH688X_REG_SMOOTH_CONST_ALGO_DATA_1        0xc4
#define SIH688X_REG_SMOOTH_CONST_ALGO_DATA_2        0xc5
#define SIH688X_REG_SAMPLE_TIME_L           0xc6
#define SIH688X_REG_SAMPLE_TIME_H           0xc7
#define SIH688X_REG_ZERO_CROSS_THRES_0      0xc8
#define SIH688X_REG_ZERO_CROSS_THRES_1      0xc9
#define SIH688X_REG_ZERO_CROSS_THRES_2      0xca
#define SIH688X_REG_WATCHDOG_CNT_MAX        0xcb
#define SIH688X_REG_SEQ0_T_DRIVER           0xcc
#define SIH688X_REG_SEQ0_T_FLUSH            0xcd
#define SIH688X_REG_SEQ0_T_BEMF             0xce
#define SIH688X_REG_SEQ1_T_DRIVER           0xcf

#define SIH688X_REG_SEQ1_T_FLUSH            0xd0
#define SIH688X_REG_SEQ1_T_BEMF             0xd1
#define SIH688X_REG_SEQ2_T_DRIVER           0xd2
#define SIH688X_REG_SEQ2_T_FLUSH            0xd3
#define SIH688X_REG_SEQ2_T_BEMF             0xd4
#define SIH688X_REG_BRK_T_D_DEL             0xd5
#define SIH688X_REG_BRK_T_FLUSH             0xd6
#define SIH688X_REG_BRK_T_BEMF              0xd7
#define SIH688X_REG_BRK_T_T_DEL             0xd8
#define SIH688X_REG_BRK_ZERO_DETECT_CNT     0xd9
#define SIH688X_REG_T_HALF_TRACKING_0       0xda
#define SIH688X_REG_T_HALF_TRACKING_1       0xdb
#define SIH688X_REG_T_HALF_TRACKING_2       0xdc
#define SIH688X_REG_T_HALF_DETECT_0         0xdd
#define SIH688X_REG_T_HALF_DETECT_1         0xde
#define SIH688X_REG_T_HALF_DETECT_2         0xdf

#define SIH688X_REG_THRES_STOP_20           0xe0
#define SIH688X_REG_THRES_STOP_21           0xe1
#define SIH688X_REG_THRES_STOP_22           0xe2
#define SIH688X_REG_BRK_LAST_RATIO          0xe3
#define SIH688X_REG_ADC_OUT_GAIN_0          0xe4
#define SIH688X_REG_ADC_OUT_GAIN_1          0xe5
#define SIH688X_REG_ADC_OUT_GAIN_2          0xe6

#define SIH688X_REG_Q11_R3_0                0xe7
#define SIH688X_REG_Q11_R3_1                0xe8
#define SIH688X_REG_Q11_R3_2                0xe9

#define SIH688X_REG_Q12_0                   0xea
#define SIH688X_REG_Q12_1                   0xeb
#define SIH688X_REG_Q12_2                   0xec

#define SIH688X_REG_Q21_r3_0                0xed
#define SIH688X_REG_Q21_r3_1                0xee
#define SIH688X_REG_Q21_r3_2                0xef

#define SIH688X_REG_Q22_0                   0xf0
#define SIH688X_REG_Q22_1                   0xf1
#define SIH688X_REG_Q22_2                   0xf2

#define SIH688X_REG_P21_R3_0                0xf3
#define SIH688X_REG_P21_R3_1                0xf4
#define SIH688X_REG_P21_R3_2                0xf5
#define SIH688X_REG_P22_0                   0xf6
#define SIH688X_REG_P22_1                   0xf7
#define SIH688X_REG_P22_2                   0xf8
#define SIH688X_REG_LAST_T_DRIVE_THRESH_0   0xf9
#define SIH688X_REG_LAST_T_DRIVE_THRESH_1   0xfa
#define SIH688X_REG_LAST_T_DRIVE_THRESH_2   0xfb

#define SIH688X_REG_MAX                     0xff
/********************************************
 * Register read/write attribute
 *******************************************/
#define REG_NONE_ACCESS                     0
#define REG_READ_ACCESS                     (1 << 0)
#define REG_WRITE_ACCESS                    (1 << 1)


/********************************************
 * Register bits define
 *******************************************/

/* SYSSST */
#define SIH_SYSSST_BIT_FIF0_AF_MASK             (1 << 7)
#define SIH_SYSSST_BIT_FIF0_AF                  (1 << 7)
#define SIH_SYSSST_BIT_FIFO_AE_MASK             (1 << 6)
#define SIH_SYSSST_BIT_FIFO_AE                  (1 << 6)
#define SIH_SYSSST_BIT_CENTRAL_STATE_MASK       (0xf << 2)
#define SIH_SYSSST_BIT_RTP_STATE                (2 << 2)
#define SIH_SYSSST_BIT_RAM_STATE                (1 << 2)
#define SIH_SYSSST_BIT_TRIG_STATE               (4 << 2)
#define SIH_SYSSST_BIT_F0_TRACK_STATE           (8 << 2)
#define SIH_SYSSST_BIT_OPENLOOP_MODE_MASK       (1 << 1)
#define SIH_SYSSST_BIT_OPENLOOP_MODE            (1 << 1)
#define SIH_SYSSST_BIT_CLOSELOOP_MODE           (0 << 1)
#define SIH_SYSSST_BIT_STANDBYMODE_MASK         (1 << 0)
#define SIH_SYSSST_BIT_STANDBY                  (1 << 0)
#define SIH_SYSSST_BIT_ACTIVE                   (0 << 0)

/* SYSINT */
#define SIH_SYSINT_BIT_OCP_FLAG_INT             (1 << 7)
#define SIH_SYSINT_BIT_UVP_FLAG_INT             (1 << 6)
#define SIH_SYSINT_BIT_OTP_FLAG_INT             (1 << 5)
#define SIH_SYSINT_BIT_MODE_SWITCH_INT          (1 << 4)
#define SIH_SYSINT_BIT_BRK_LONG_TIMEOUT         (1 << 3)
#define SIH_SYSINT_BIT_DONE                     (1 << 2)
#define SIH_SYSINT_BIT_FF_AEI                   (1 << 1)
#define SIH_SYSINT_BIT_FF_AFI                   (1 << 0)

/* SYSINTM */
#define SIH_SYSINT_BIT_OCP_FLAG_INT_MASK        (1 << 7)
#define SIH_SYSINT_BIT_OCP_FLAG_INT_EN          (1 << 7)
#define SIH_SYSINT_BIT_OCP_FLAG_INT_OFF         (0 << 7)
#define SIH_SYSINT_BIT_UVP_FLAG_INT_MASK        (1 << 6)
#define SIH_SYSINT_BIT_UVP_FLAG_INT_EN          (1 << 6)
#define SIH_SYSINT_BIT_UVP_FLAG_INT_OFF         (0 << 6)
#define SIH_SYSINT_BIT_OTP_FLAG_INT_MASK        (1 << 5)
#define SIH_SYSINT_BIT_OTP_FLAG_INT_EN          (1 << 5)
#define SIH_SYSINT_BIT_OTP_FLAG_INT_OFF         (0 << 5)
#define SIH_SYSINT_BIT_MODE_SWITCH_INT_MASK     (1 << 4)
#define SIH_SYSINT_BIT_MODE_SWITCH_INT_EN       (1 << 4)
#define SIH_SYSINT_BIT_MODE_SWITCH_INT_OFF      (0 << 4)
#define SIH_SYSINT_BIT_BRK_LONG_TIMEOUT_MASK    (1 << 3)
#define SIH_SYSINT_BIT_BRK_LONG_TIMEOUT_EN      (1 << 3)
#define SIH_SYSINT_BIT_BRK_LONG_TIMEOUT_OFF     (0 << 3)
#define SIH_SYSINTM_BIT_DONE_MASK               (1 << 2)
#define SIH_SYSINTM_BIT_DONE_EN                 (1 << 2)
#define SIH_SYSINTM_BIT_DONE_OFF                (0 << 2)
#define SIH_SYSINTM_BIT_FF_AEI_MASK             (1 << 1)
#define SIH_SYSINTM_BIT_FF_AEI_EN               (1 << 1)
#define SIH_SYSINTM_BIT_FF_AEI_OFF              (0 << 1)
#define SIH_SYSINTM_BIT_FF_AFI_MASK             (1 << 0)
#define SIH_SYSINTM_BIT_FF_AFI_EN               (1 << 0)
#define SIH_SYSINTM_BIT_FF_AFI_OFF              (0 << 0)
/* SYSCTRL1 */
#define SIH_SYSCTRL1_BIT_ENRAMINIT_MASK         (1 << 7)
#define SIH_SYSCTRL1_BIT_RAMINIT_EN             (1 << 7)
#define SIH_SYSCTRL1_BIT_RAMINIT_OFF            (0 << 7)
#define SIH_SYSCTRL1_BIT_LOWPOWER_MASK          (1 << 6)
#define SIH_SYSCTRL1_BIT_LOWPOWER_EN            (1 << 6)
#define SIH_SYSCTRL1_BIT_LOWPOWER_OFF           (0 << 6)
#define SIH_SYSCTRL1_BIT_RTP_DUMMY_MASK         (1 << 5)
#define SIH_SYSCTRL1_BIT_RTP_DUMMY_EN           (1 << 5)
#define SIH_SYSCTRL1_BIT_RTP_DUMMY_OFF          (0 << 5)
#define SIH_SYSCTRL1_BIT_SYNC_MODE_MASK         (1 << 4)
#define SIH_SYSCTRL1_BIT_SYNC_MODE_EN           (1 << 4)
#define SIH_SYSCTRL1_BIT_SYNC_MODE_OFF          (0 << 4)
#define SIH_SYSCTRL1_BIT_TRIG_RTP_PRIO_MASK     (1 << 3)
#define SIH_SYSCTRL1_BIT_TRIG_RTP_PRIO_EN       (1 << 3)
#define SIH_SYSCTRL1_BIT_TRIG_RTP_PRIO_OFF      (0 << 3)
#define SIH_SYSCTRL1_BIT_TRIG_RAM_PRIO_MASK     (1 << 2)
#define SIH_SYSCTRL1_BIT_TRIG_RAM_PRIO_EN       (1 << 2)
#define SIH_SYSCTRL1_BIT_TRIG_RAM_PRIO_OFF      (0 << 2)
#define SIH_SYSCTRL1_BIT_STOP_MODE_MASK         (1 << 1)
#define SIH_SYSCTRL1_BIT_STOP_RIGHT_NOW         (1 << 1)
#define SIH_SYSCTRL1_BIT_STOP_CUR_OVER          (0 << 1)
#define SIH_SYSCTRL1_BIT_AUTO_PVDD_MASK         (1 << 0)
#define SIH_SYSCTRL1_BIT_AUTO_PVDD_EN           (1 << 0)
#define SIH_SYSCTRL1_BIT_AUTO_PVDD_OFF          (0 << 0)

/* RL_VBAT_CTRL */
#define SIH_RL_VBAT_CTRL_BIT_ADC_OC_TEST_EN_MASK  (1 << 2)
#define SIH_RL_VBAT_CTRL_BIT_ADC_OC_TEST_EN_EN    (1 << 2)
#define SIH_RL_VBAT_CTRL_BIT_ADC_OC_TEST_EN_OFF   (0 << 2)
#define SIH_RL_VBAT_CTRL_BIT_DET_MODE_MASK        (1 << 1)
#define SIH_RL_VBAT_CTRL_BIT_DET_MODE_EN          (1 << 1)
#define SIH_RL_VBAT_CTRL_BIT_DET_MODE_OFF         (0 << 1)
#define SIH_RL_VBAT_CTRL_BIT_DET_GO_MASK          (1 << 0)
#define SIH_RL_VBAT_CTRL_BIT_DET_GO_EN            (1 << 0)
#define SIH_RL_VBAT_CTRL_BIT_DET_GO_OFF           (0 << 0)

/* SYSCTRL2 */
#define SIH_SYSCTRL2_BIT_VMAX_PVDD_DIG_MASK              (1 << 7)
#define SIH_SYSCTRL2_BIT_VMAX_PVDD_DIG_EN                (1 << 7)
#define SIH_SYSCTRL2_BIT_VMAX_PVDD_DIG_OFF               (0 << 7)
#define SIH_SYSCTRL2_BIT_GAIN_SEL_MASK                   (1 << 6)
#define SIH_SYSCTRL2_BIT_GAIN_FLEXIBLE                   (1 << 6)
#define SIH_SYSCTRL2_BIT_GAIN_UNIFY                      (0 << 6)
#define SIH_SYSCTRL2_BIT_PWM_EN_PULLDOWN_A_SET_MASK      (1 << 5)
#define SIH_SYSCTRL2_BIT_PWM_EN_PULLDOWN_A_SET_LOW       (1 << 5)
#define SIH_SYSCTRL2_BIT_PWM_EN_PULLDOWN_A_SET_HIGH      (0 << 5)
#define SIH_SYSCTRL2_BIT_PWM_EN_PULLDOWN_B_SET_MASK      (1 << 4)
#define SIH_SYSCTRL2_BIT_PWM_EN_PULLDOWN_B_SET_LOW       (1 << 4)
#define SIH_SYSCTRL2_BIT_PWM_EN_PULLDOWN_B_SET_HIGH      (0 << 4)
#define SIH_SYSCTRL2_BIT_PWM_SWAP_MASK                   (1 << 3)
#define SIH_SYSCTRL2_BIT_PWM_SWAP_EN                     (1 << 3)
#define SIH_SYSCTRL2_BIT_PWM_SWAP_OFF                    (0 << 3)
#define SIH_SYSCTRL2_BIT_INTN_POLAR_MASK                 (1 << 2)
#define SIH_SYSCTRL2_BIT_POLAR_HIGH                      (1 << 2)
#define SIH_SYSCTRL2_BIT_POLAR_LOW                       (0 << 2)
#define SIH_SYSCTRL2_BIT_TRIG_BACKTO_RTP_BYPASS_MASK     (1 << 1)
#define SIH_SYSCTRL2_BIT_TRIG_BACKTO_RTP_BYPASS_EN       (1 << 1)
#define SIH_SYSCTRL2_BIT_TRIG_BACKTO_RTP_BYPASS_OFF      (0 << 1)
#define SIH_SYSCTRL2_BIT_BOOST_BYPASS_MASK               (1 << 0)
#define SIH_SYSCTRL2_BIT_BOOST_ENABLE                    (0 << 0)
#define SIH_SYSCTRL2_BIT_BOOST_BYPASS                    (1 << 0)

/* G0 */
#define SIH_GO_BIT_F0_TRACK_SKIP_FINAL_FLUSH_MASK        (1 << 4)
#define SIH_GO_BIT_F0_TRACK_SKIP_FINAL_FLUSH_EN          (1 << 4)
#define SIH_GO_BIT_F0_TRACK_SKIP_FINAL_FLUSH_OFF         (0 << 4)
#define SIH_GO_BIT_STOP_TRIG_MASK                        (1 << 3)
#define SIH_GO_BIT_STOP_TRIG_EN                          (1 << 3)
#define SIH_GO_BIT_STOP_TRIG_OFF                         (0 << 3)
#define SIH_GO_BIT_F0_SEQ_GO_MASK                        (1 << 2)
#define SIH_GO_BIT_F0_SEQ_GO_ENABLE                      (1 << 2)
#define SIH_GO_BIT_F0_SEQ_GO_DISABLE                     (0 << 2)
#define SIH_GO_BIT_RTP_GO_MASK                           (1 << 1)
#define SIH_GO_BIT_RTP_GO_ENABLE                         (1 << 1)
#define SIH_GO_BIT_RTP_GO_DISABLE                        (0 << 1)
#define SIH_GO_BIT_RAM_GO_MASK                           (1 << 0)
#define SIH_GO_BIT_RAM_GO_ENABLE                         (1 << 0)
#define SIH_GO_BIT_RAM_GO_DISABLE                        (0 << 0)

/* SYSINT2 */
#define SIH_SYSINT2_BIT_F0_TRACKING_INT                  (1 << 2)
#define SIH_SYSINT2_BIT_F0_DETECT_DONE_INT               (1 << 1)
#define SIH_SYSINT2_BIT_BRK_SHORT_TIMEOUT                (1 << 0)

/* SYSINTM2 */
#define SIH_SYSINT2_BIT_F0_TRACKING_INT_MASK             (1 << 2)
#define SIH_SYSINT2_BIT_F0_TRACKING_INT_EN               (1 << 2)
#define SIH_SYSINT2_BIT_F0_TRACKING_INT_OFF              (0 << 2)
#define SIH_SYSINT2_BIT_F0_DETECT_DONE_INT_MASK          (1 << 1)
#define SIH_SYSINT2_BIT_F0_DETECT_DONE_INT_EN            (1 << 1)
#define SIH_SYSINT2_BIT_F0_DETECT_DONE_INT_OFF           (0 << 1)
#define SIH_SYSINT2_BIT_BRK_SHORT_TIMEOUT_MASK           (1 << 1)
#define SIH_SYSINT2_BIT_BRK_SHORT_TIMEOUT_EN             (1 << 1)
#define SIH_SYSINT2_BIT_BRK_SHORT_TIMEOUT_OFF            (0 << 1)

/* MAIN_STATE_CTRL */
#define SIH_MODECTRL_BIT_TRIG_BRK_MASK          (1 << 7)
#define SIH_MODECTRL_BIT_TRIG_BRK_EN            (1 << 7)
#define SIH_MODECTRL_BIT_TRIG_BRK_OFF           (0 << 7)
#define SIH_MODECTRL_BIT_TRIG_F0_MASK           (1 << 6)
#define SIH_MODECTRL_BIT_TRIG_F0_EN             (1 << 6)
#define SIH_MODECTRL_BIT_TRIG_F0_OFF            (0 << 6)
#define SIH_MODECTRL_BIT_RAM_BRK_MASK           (1 << 5)
#define SIH_MODECTRL_BIT_RAM_BRK_EN             (1 << 5)
#define SIH_MODECTRL_BIT_RAM_BRK_OFF            (0 << 5)
#define SIH_MODECTRL_BIT_RAM_F0_MASK            (1 << 4)
#define SIH_MODECTRL_BIT_RAM_F0_EN              (1 << 4)
#define SIH_MODECTRL_BIT_RAM_F0_OFF             (0 << 4)
#define SIH_MODECTRL_BIT_RTP_BRK_MASK           (1 << 3)
#define SIH_MODECTRL_BIT_RTP_BRK_EN             (1 << 3)
#define SIH_MODECTRL_BIT_RTP_BRK_OFF            (0 << 3)
#define SIH_MODECTRL_BIT_RTP_F0_MASK            (1 << 2)
#define SIH_MODECTRL_BIT_RTP_F0_EN              (1 << 2)
#define SIH_MODECTRL_BIT_RTP_F0_OFF             (0 << 2)
#define SIH_MODECTRL_BIT_TRACK_BRK_MASK         (1 << 1)
#define SIH_MODECTRL_BIT_TRACK_BRK_EN           (1 << 1)
#define SIH_MODECTRL_BIT_TRACK_BRK_OFF          (0 << 1)
#define SIH_MODECTRL_BIT_TRACK_F0_MASK          (1 << 0)
#define SIH_MODECTRL_BIT_TRACK_F0_EN            (1 << 0)
#define SIH_MODECTRL_BIT_TRACK_F0_OFF           (0 << 0)

/* WAVESEQ */
#define WAIT_MASK                               (1 << 7)
#define WAIT_EN                                 (1 << 7)
#define WAIT_OFF                                (0 << 7)
#define WAVE_OR_TIME_MASK                       (0x7f << 0)
/* WAVESEQ1 */
#define SIH_WAVESEQ1_BIT_WAIT_MASK              WAIT_MASK
#define SIH_WAVESEQ1_BIT_WAIT_EN                WAIT_EN
#define SIH_WAVESEQ1_BIT_WAIT_OFF               WAIT_OFF
#define SIH_WAVESEQ1_BIT_WAVE_OR_TIME_MASK      WAVE_OR_TIME_MASK

/* WAVESEQ2 */
#define SIH_WAVESEQ2_BIT_WAIT_MASK              WAIT_MASK
#define SIH_WAVESEQ2_BIT_WAIT_EN                WAIT_EN
#define SIH_WAVESEQ2_BIT_WAIT_OFF               WAIT_OFF
#define SIH_WAVESEQ2_BIT_WAVE_OR_TIME_MASK      WAVE_OR_TIME_MASK

/* WAVESEQ3 */
#define SIH_WAVESEQ3_BIT_WAIT_MASK              WAIT_MASK
#define SIH_WAVESEQ3_BIT_WAIT_EN                WAIT_EN
#define SIH_WAVESEQ3_BIT_WAIT_OFF               WAIT_OFF
#define SIH_WAVESEQ3_BIT_WAVE_OR_TIME_MASK      WAVE_OR_TIME_MASK

/* WAVESEQ4 */
#define SIH_WAVESEQ4_BIT_WAIT_MASK              WAIT_MASK
#define SIH_WAVESEQ4_BIT_WAIT_EN                WAIT_EN
#define SIH_WAVESEQ4_BIT_WAIT_OFF               WAIT_OFF
#define SIH_WAVESEQ4_BIT_WAVE_OR_TIME_MASK      WAVE_OR_TIME_MASK

/* WAVESEQ5 */
#define SIH_WAVESEQ5_BIT_WAIT_MASK              WAIT_MASK
#define SIH_WAVESEQ5_BIT_WAIT_EN                WAIT_EN
#define SIH_WAVESEQ5_BIT_WAIT_OFF               WAIT_OFF
#define SIH_WAVESEQ5_BIT_WAVE_OR_TIME_MASK      WAVE_OR_TIME_MASK

/* WAVESEQ6 */
#define SIH_WAVESEQ6_BIT_WAIT_MASK              WAIT_MASK
#define SIH_WAVESEQ6_BIT_WAIT_EN                WAIT_EN
#define SIH_WAVESEQ6_BIT_WAIT_OFF               WAIT_OFF
#define SIH_WAVESEQ6_BIT_WAVE_OR_TIME_MASK      WAVE_OR_TIME_MASK

/* WAVESEQ7 */
#define SIH_WAVESEQ7_BIT_WAIT_MASK              WAIT_MASK
#define SIH_WAVESEQ7_BIT_WAIT_EN                WAIT_EN
#define SIH_WAVESEQ7_BIT_WAIT_OFF               WAIT_OFF
#define SIH_WAVESEQ7_BIT_WAVE_OR_TIME_MASK      WAVE_OR_TIME_MASK

/* WAVESEQ8 */
#define SIH_WAVESEQ8_BIT_WAIT_MASK              WAIT_MASK
#define SIH_WAVESEQ8_BIT_WAIT_EN                WAIT_EN
#define SIH_WAVESEQ8_BIT_WAIT_OFF               WAIT_OFF
#define SIH_WAVESEQ8_BIT_WAVE_OR_TIME_MASK      WAVE_OR_TIME_MASK

#define WAVELOOP_SEQ_ODD_MASK                   (0x0F << 4)
#define WAVELOOP_SEQ_EVEN_MASK                  (0X0F << 0)
#define WAVELOOP_SEQ_ODD_INFINNTE_TIME          (0x0F << 4)
#define WAVELOOP_SEQ_EVEN_INFINNTE_TIME         (0x0F << 0)

/* WAVELOOP1 */
#define SIH_WAVELOOP1_BIT_SEQ1_MASK             WAVELOOP_SEQ_ODD_MASK
#define SIH_WAVELOOP1_BIT_SEQ1_INFINITE         WAVELOOP_SEQ_ODD_INFINNTE_TIME
#define SIH_WAVELOOP1_BIT_SEQ2_MASK             WAVELOOP_SEQ_EVEN_MASK
#define SIH_WAVELOOP1_BIT_SEQ2_INFINITE         WAVELOOP_SEQ_EVEN_INFINNTE_TIME

/* WAVELOOP2 */
#define SIH_WAVELOOP2_BIT_SEQ3_MASK             WAVELOOP_SEQ_ODD_MASK
#define SIH_WAVELOOP2_BIT_SEQ3_INFINITE         WAVELOOP_SEQ_ODD_INFINNTE_TIME
#define SIH_WAVELOOP2_BIT_SEQ4_MASK             WAVELOOP_SEQ_EVEN_MASK
#define SIH_WAVELOOP2_BIT_SEQ4_INFINITE         WAVELOOP_SEQ_EVEN_INFINNTE_TIME

/* WAVELOOP3 */
#define SIH_WAVELOOP3_BIT_SEQ5_MASK             WAVELOOP_SEQ_ODD_MASK
#define SIH_WAVELOOP3_BIT_SEQ5_INFINITE         WAVELOOP_SEQ_ODD_INFINNTE_TIME
#define SIH_WAVELOOP3_BIT_SEQ6_MASK             WAVELOOP_SEQ_EVEN_MASK
#define SIH_WAVELOOP3_BIT_SEQ6_INFINITE         WAVELOOP_SEQ_EVEN_INFINNTE_TIME

/* WAVELOOP4 */
#define SIH_WAVELOOP4_BIT_SEQ7_MASK             WAVELOOP_SEQ_ODD_MASK
#define SIH_WAVELOOP4_BIT_SEQ7_INFINITE         WAVELOOP_SEQ_ODD_INFINNTE_TIME
#define SIH_WAVELOOP4_BIT_SEQ8_MASK             WAVELOOP_SEQ_EVEN_MASK
#define SIH_WAVELOOP4_BIT_SEQ8_INFINITE         WAVELOOP_SEQ_EVEN_INFINNTE_TIME

/* MAINLOOP */
#define SIH_MAINLOOP_BIT_WAITSLOT_MASK          (0x3 << 4)
/* 120*166ns = 19920ns */
#define SIH_MAINLOOP_BIT_WAITSLOT_20_US         (0 << 4)
/* 960*166ns = 159360ns */
#define SIH_MAINLOOP_BIT_WAITSLOT_160_US        (1 << 4)
/* 7680*166ns = 1274880ns */
#define SIH_MAINLOOP_BIT_WAITSLOT_1_MS          (2 << 4)
/* 60000*166ns = 9960000ns */
#define SIH_MAINLOOP_BIT_WAITSLOT_10_MS         (3 << 4)
#define SIH_MAINLOOP_BIT_MAIN_LOOP_MASK         (0x0f << 0)
#define SIH_MAINLOOP_BIT_MAIN_LOOP_INIFINTE     (0x0f)

/* BASE_ADDRH */
#define SIH_BASE_ADDRH_BIT_MASK                 (0x1f << 0)

/* RTPCFG3 */
#define SIH_RTPCFG3_BIT_FIFO_AFH_MASK           (0x0f << 4)

/* RTPCFG3 */
#define SIH_RTPCFG3_BIT_FIFO_AEH_MASK           (0x0f << 0)

/* RTPCLR */
#define SIH_RTPCLR_BIT_FIFO_CLR_MASK            (1 << 0)
#define SIH_RTPCLR_BIT_FIFO_CLR_EN              (1 << 0)

#define SIH_FIFO_AE_ADDR_H(base_addr)	((base_addr >> 1) >> 8)
#define SIH_FIFO_AE_ADDR_L(base_addr)	((base_addr >> 1) & 0x00ff)
#define SIH_FIFO_AF_ADDR_H(base_addr)	((base_addr - (base_addr >> 2)) >> 8)
#define SIH_FIFO_AF_ADDR_L(base_addr)	((base_addr - (base_addr >> 2)) & 0xff)

/* RAMADDRH */
#define SIH_RAMADDRH_BIT_MASK			(0x1f << 0)
#define SIH_RAM_ADDR_H(base_addr)		(base_addr >> 8)
#define SIH_RAM_ADDR_L(base_addr)		(base_addr & 0x00FF)

/* PWM_UP_SAMPLE_CTRL */
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_FIR_BRK_EN_MASK               (1 << 5)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_FIR_BRK_EN                    (1 << 5)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_FIR_BRK_OFF                   (0 << 5)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_SAMPLE_BRK_EN_MASK     (1 << 4)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_SAMPLE_BRK_EN          (1 << 4)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_SAMPLE_BRK_OFF         (0 << 4)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_FIR_EN_MASK                   (1 << 3)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_FIR_EN                        (1 << 3)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_FIR_OFF                       (0 << 3)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_RPT_MASK               (0x03 << 1)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_ONE_TIME               (0 << 1)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_TWO_TIME               (1 << 1)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_FOUR_TIME              (3 << 1)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_SPLIT_MASK                (1 << 0)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_SPLIT_EN                  (1 << 0)
#define SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_SPLIT_OFF                 (0 << 0)

/* TRIG_CTRL1 */
#define SIH_TRIG_CTRL1_BIT_TPOLAR2_MASK			(1 << 6)
#define SIH_TRIG_CTRL1_BIT_TPOLAR2_H			(1 << 6)
#define SIH_TRIG_CTRL1_BIT_TPOLAR2_L			(0 << 6)
#define SIH_TRIG_CTRL1_BIT_TPOLAR1_MASK			(1 << 5)
#define SIH_TRIG_CTRL1_BIT_TPOLAR1_H			(1 << 5)
#define SIH_TRIG_CTRL1_BIT_TPOLAR1_L			(0 << 5)
#define SIH_TRIG_CTRL1_BIT_TPOLAR0_MASK			(1 << 4)
#define SIH_TRIG_CTRL1_BIT_TPOLAR0_H			(1 << 4)
#define SIH_TRIG_CTRL1_BIT_TPOLAR0_L			(0 << 4)
#define SIH_TRIG_CTRL1_BIT_TRIG2_EN_MASK		(1 << 2)
#define SIH_TRIG_CTRL1_BIT_TRIG2_EN				(1 << 2)
#define SIH_TRIG_CTRL1_BIT_TRIG2_OFF			(0 << 2)
#define SIH_TRIG_CTRL1_BIT_TRIG1_EN_MASK		(1 << 1)
#define SIH_TRIG_CTRL1_BIT_TRIG1_EN				(1 << 1)
#define SIH_TRIG_CTRL1_BIT_TRIG1_OFF			(0 << 1)
#define SIH_TRIG_CTRL1_BIT_TRIG0_EN_MASK		(1 << 0)
#define SIH_TRIG_CTRL1_BIT_TRIG0_EN				(1 << 0)
#define SIH_TRIG_CTRL1_BIT_TRIG0_OFF			(0 << 0)

/* TRIG_CTRL2 */
#define SIH_TRIG_CTRL2_BIT_TRIG2_MODE_MASK       (0x3 << 4)
#define SIH_TRIG_CTRL2_BIT_TRIG2_POSEDGE         (0 << 6)
#define SIH_TRIG_CTRL2_BIT_TRIG2_NEGEDGE         (1 << 6)
#define SIH_TRIG_CTRL2_BIT_TRIG2_BEDGE           (2 << 6)
#define SIH_TRIG_CTRL2_BIT_TRIG2_LEVEL           (3 << 6)
#define SIH_TRIG_CTRL2_BIT_TRIG1_MODE_MASK       (0x3 << 2)
#define SIH_TRIG_CTRL2_BIT_TRIG1_POSEDGE         (0 << 6)
#define SIH_TRIG_CTRL2_BIT_TRIG1_NEGEDGE         (1 << 6)
#define SIH_TRIG_CTRL2_BIT_TRIG1_BEDGE           (2 << 6)
#define SIH_TRIG_CTRL2_BIT_TRIG1_LEVEL           (3 << 6)
#define SIH_TRIG_CTRL2_BIT_TRIG0_MODE_MASK       (0x3 << 0)
#define SIH_TRIG_CTRL2_BIT_TRIG0_POSEDGE         (0 << 6)
#define SIH_TRIG_CTRL2_BIT_TRIG0_NEGEDGE         (1 << 6)
#define SIH_TRIG_CTRL2_BIT_TRIG0_BEDGE           (2 << 6)
#define SIH_TRIG_CTRL2_BIT_TRIG0_LEVEL           (3 << 6)

/* TRIG_PACK_P */
#define SIH_TRIG_PACK_P_BIT_TRIG_PACK_MASK		(0x7f << 0)
#define SIH_TRIG_PACK_P_BIT_BOOST_BYPASS_MASK	(1 << 7)
/* TRIG_PACK_N */
#define SIH_TRIG_PACK_N_BIT_TRIG_PACK_MASK		(0x7f << 0)

#define SIH_TRIG_PACK_BIT_WAVEID_MASK            (0x7f << 0)

/* TEST_CTRL_1 */
#define SIH_TEST_CTRL1_BIT_FORCE_OSC_MASK        (1 << 7)
#define SIH_TEST_CTRL1_BIT_FORCE_OSC_ON          (1 << 7)
#define SIH_TEST_CTRL1_BIT_FORCE_OSC_SHUT        (0 << 7)

#define SIH_688X_ID_SOFTWARE_RESET                0xaa
#define SIH_688X_ID_HARDWARE_RESET                0x55

/* GAIN_SET_SEQ */
#define WAVEGAIN_SEQ_ODD_MASK                   (0x0F << 4)
#define WAVEGAIN_SEQ_EVEN_MASK                  (0X0F << 0)

/* WAVEGAIN1 */
#define SIH_WAVEGAIN1_BIT_SEQ1_MASK             WAVEGAIN_SEQ_ODD_MASK
#define SIH_WAVEGAIN1_BIT_SEQ2_MASK             WAVEGAIN_SEQ_EVEN_MASK

/* WAVEGAIN2 */
#define SIH_WAVEGAIN2_BIT_SEQ3_MASK             WAVEGAIN_SEQ_ODD_MASK
#define SIH_WAVEGAIN2_BIT_SEQ4_MASK             WAVEGAIN_SEQ_EVEN_MASK

/* WAVEGAIN3 */
#define SIH_WAVEGAIN3_BIT_SEQ5_MASK             WAVEGAIN_SEQ_ODD_MASK
#define SIH_WAVEGAIN3_BIT_SEQ6_MASK             WAVEGAIN_SEQ_EVEN_MASK

/* WAVEGAIN4 */
#define SIH_WAVEGAIN4_BIT_SEQ7_MASK             WAVEGAIN_SEQ_ODD_MASK
#define SIH_WAVEGAIN4_BIT_SEQ8_MASK             WAVEGAIN_SEQ_EVEN_MASK

/* ANA_CTRL1 */
#define SIH_ANA_CTRL1_LPF_CAP_O_MASK			(0x1f << 3)
#define SIH_ANA_CTRL1_LPF_CAP_O_0_82_GAIN		(0x1b << 3)
#define SIH_ANA_CTRL1_LPF_CAP_O_2_5_GAIN		(0x0c << 3)
#define SIH_ANA_CTRL1_LPF_CAP_O_4_GAIN			(0x07 << 3)
#define SIH_ANA_CTRL1_LPF_CAP_O_6_GAIN			(0x03 << 3)
#define SIH_ANA_CTRL1_LPF_CAP_O_8_GAIN			(0x01 << 3)

/* ANA_CTRL2 */
#define SIH_ANA_CTRL2_LPF_GAIN_O_MASK			(0x0f << 4)
#define SIH_ANA_CTRL2_LPF_GAIN_O_0_82_GAIN		(0x0b << 4)
#define SIH_ANA_CTRL2_LPF_GAIN_O_2_5_GAIN		(0x04 << 4)
#define SIH_ANA_CTRL2_LPF_GAIN_O_4_GAIN			(0x00 << 4)
#define SIH_ANA_CTRL2_LPF_GAIN_O_6_GAIN			(0x02 << 4)
#define SIH_ANA_CTRL2_LPF_GAIN_O_8_GAIN			(0x03 << 4)
#define SIH_ANA_CTRL2_BST_EA_SEL_O_MASK			(0x0f << 0)
#define SIH_ANA_CTRL2_BST_EA_6_7				(0x09 << 0)
#define SIH_ANA_CTRL2_BST_EA_7_9				(0x05 << 0)
#define SIH_ANA_CTRL2_BST_EA_9_11				(0x06 << 0)

/* ANA_CTRL3 */
#define SIH_ANA_CTRL3_BST_OUT_SEL_O_MASK		(0x7f << 0)
#define SIH_ANA_CTRL3_BST_OUT_SEL_3_5000		(0x00 << 0)
#define SIH_ANA_CTRL3_BST_OUT_SEL_3_5625		(0x01 << 0)
#define SIH_ANA_CTRL3_BST_OUT_SEL_3_6250		(0x02 << 0)
#define SIH_ANA_CTRL3_BST_OUT_SEL_6				(0x28 << 0)
#define SIH_ANA_CTRL3_BST_OUT_SEL_7				(0x38 << 0)
#define SIH_ANA_CTRL3_BST_OUT_SEL_8				(0x48 << 0)
#define SIH_ANA_CTRL3_BST_OUT_SEL_9				(0x58 << 0)
#define SIH_ANA_CTRL3_BST_OUT_SEL_10			(0x68 << 0)
#define SIH_ANA_CTRL3_BST_OUT_SEL_11			(0x78 << 0)

#define SIH_ANA_CTRL_BST_LEVEL_6				60
#define SIH_ANA_CTRL_BST_LEVEL_7				70
#define SIH_ANA_CTRL_BST_LEVEL_8				80
#define SIH_ANA_CTRL_BST_LEVEL_9				90
#define SIH_ANA_CTRL_BST_LEVEL_10				100
#define SIH_ANA_CTRL_BST_LEVEL_11				110

/* ANA_CTRL5 */
#define SIH_ANA_CTRL5_BST_IOS_SEL_O_MASK		(0x7 << 5)
#define SIH_ANA_CTRL5_BST_IOS_SEL_6_8			(0x4 << 5)
#define SIH_ANA_CTRL5_BST_IOS_SEL_8_10			(0x2 << 5)
#define SIH_ANA_CTRL5_BST_IOS_SEL_10_11			(0x0 << 5)
#define SIH_ANA_CTRL5_BST_OCP_VRSEL_O_MASK		(0x7 << 1)
#define SIH_ANA_CTRL5_BST_OCP_VRSEL_6_8			(0x4 << 1)
#define SIH_ANA_CTRL5_BST_OCP_VRSEL_8_10		(0x3 << 1)
#define SIH_ANA_CTRL5_BST_OCP_VRSEL_10_11		(0x2 << 1)

/* ANA_CTRL6 */
#define SIH_ANA_CTRL6_BST_ZCD_IOS_O_MASK		(0x3 << 5)
#define SIH_ANA_CTRL5_BST_ZCD_IOS_6_8			(0x1 << 5)
#define SIH_ANA_CTRL5_BST_ZCD_IOS_8_11			(0x2 << 5)


/* ANA_CTRL6 */


/* SMOOTH F0 WINDOW OUT */
#define SIH_DETECT_FIFO_CTRL_MASK               (1 << 5)
#define SIH_DETECT_FIFO_CTRL_EN                 (1 << 5)
#define SIH_DETECT_FIFO_CTRL_OFF                (0 << 5)
#define SIH_WAIT_FIFO_DETECT_MASK               (1 << 6)
#define SIH_WAIT_FIFO_DETECT_EN                 (1 << 6)
#define SIH_WAIT_FIFO_DETECT_OFF                (0 << 6)

extern const struct regmap_config sih688x_regmap_config;


#endif
