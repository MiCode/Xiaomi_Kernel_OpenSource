/* 
 * Copyright (C) 2014-2020 NXP Semiconductors, All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Copyright 2020 GOODIX 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */


/** Filename: Tfa98xx_genregs.h
 *  This file was generated automatically on 09/01/15 at 09:40:23. 
 *  Source file: TFA9888_N1C_I2C_regmap_V1.xlsx
 */

#ifndef TFA2_GENREGS_H
#define TFA2_GENREGS_H


#define TFA98XX_SYS_CONTROL0              0x00
#define TFA98XX_SYS_CONTROL1              0x01
#define TFA98XX_SYS_CONTROL2              0x02
#define TFA98XX_DEVICE_REVISION           0x03
#define TFA98XX_CLOCK_CONTROL             0x04
#define TFA98XX_CLOCK_GATING_CONTROL      0x05
#define TFA98XX_SIDE_TONE_CONFIG          0x0d
#define TFA98XX_CTRL_DIGTOANA_REG         0x0e
#define TFA98XX_STATUS_FLAGS0             0x10
#define TFA98XX_STATUS_FLAGS1             0x11
#define TFA98XX_STATUS_FLAGS2             0x12
#define TFA98XX_STATUS_FLAGS3             0x13
#define TFA98XX_STATUS_FLAGS4             0x14
#define TFA98XX_BATTERY_VOLTAGE           0x15
#define TFA98XX_TEMPERATURE               0x16
#define TFA98XX_TDM_CONFIG0               0x20
#define TFA98XX_TDM_CONFIG1               0x21
#define TFA98XX_TDM_CONFIG2               0x22
#define TFA98XX_TDM_CONFIG3               0x23
#define TFA98XX_TDM_CONFIG4               0x24
#define TFA98XX_TDM_CONFIG5               0x25
#define TFA98XX_TDM_CONFIG6               0x26
#define TFA98XX_TDM_CONFIG7               0x27
#define TFA98XX_TDM_CONFIG8               0x28
#define TFA98XX_TDM_CONFIG9               0x29
#define TFA98XX_PDM_CONFIG0               0x31
#define TFA98XX_PDM_CONFIG1               0x32
#define TFA98XX_HAPTIC_DRIVER_CONFIG      0x33
#define TFA98XX_GPIO_DATAIN_REG           0x34
#define TFA98XX_GPIO_CONFIG               0x35
#define TFA98XX_INTERRUPT_OUT_REG1        0x40
#define TFA98XX_INTERRUPT_OUT_REG2        0x41
#define TFA98XX_INTERRUPT_OUT_REG3        0x42
#define TFA98XX_INTERRUPT_IN_REG1         0x44
#define TFA98XX_INTERRUPT_IN_REG2         0x45
#define TFA98XX_INTERRUPT_IN_REG3         0x46
#define TFA98XX_INTERRUPT_ENABLE_REG1     0x48
#define TFA98XX_INTERRUPT_ENABLE_REG2     0x49
#define TFA98XX_INTERRUPT_ENABLE_REG3     0x4a
#define TFA98XX_STATUS_POLARITY_REG1      0x4c
#define TFA98XX_STATUS_POLARITY_REG2      0x4d
#define TFA98XX_STATUS_POLARITY_REG3      0x4e
#define TFA98XX_BAT_PROT_CONFIG           0x50
#define TFA98XX_AUDIO_CONTROL             0x51
#define TFA98XX_AMPLIFIER_CONFIG          0x52
#define TFA98XX_AUDIO_CONTROL2            0x5a
#define TFA98XX_DCDC_CONTROL0             0x70
#define TFA98XX_CF_CONTROLS               0x90
#define TFA98XX_CF_MAD                    0x91
#define TFA98XX_CF_MEM                    0x92
#define TFA98XX_CF_STATUS                 0x93
#define TFA98XX_MTPKEY2_REG               0xa1
#define TFA98XX_MTP_STATUS                0xa2
#define TFA98XX_KEY_PROTECTED_MTP_CONTROL 0xa3
#define TFA98XX_MTP_DATA_OUT_MSB          0xa5
#define TFA98XX_MTP_DATA_OUT_LSB          0xa6
#define TFA98XX_TEMP_SENSOR_CONFIG        0xb1
#define TFA98XX_KEY2_PROTECTED_MTP0       0xf0
#define TFA98XX_KEY1_PROTECTED_MTP4       0xf4
#define TFA98XX_KEY1_PROTECTED_MTP5       0xf5

/*
 * (0x00)-sys_control0
 */

/*
 * powerdown
 */
#define TFA98XX_SYS_CONTROL0_PWDN                         (0x1<<0)
#define TFA98XX_SYS_CONTROL0_PWDN_POS                            0
#define TFA98XX_SYS_CONTROL0_PWDN_LEN                            1
#define TFA98XX_SYS_CONTROL0_PWDN_MAX                            1
#define TFA98XX_SYS_CONTROL0_PWDN_MSK                          0x1

/*
 * reset
 */
#define TFA98XX_SYS_CONTROL0_I2CR                         (0x1<<1)
#define TFA98XX_SYS_CONTROL0_I2CR_POS                            1
#define TFA98XX_SYS_CONTROL0_I2CR_LEN                            1
#define TFA98XX_SYS_CONTROL0_I2CR_MAX                            1
#define TFA98XX_SYS_CONTROL0_I2CR_MSK                          0x2

/*
 * enbl_coolflux
 */
#define TFA98XX_SYS_CONTROL0_CFE                          (0x1<<2)
#define TFA98XX_SYS_CONTROL0_CFE_POS                             2
#define TFA98XX_SYS_CONTROL0_CFE_LEN                             1
#define TFA98XX_SYS_CONTROL0_CFE_MAX                             1
#define TFA98XX_SYS_CONTROL0_CFE_MSK                           0x4

/*
 * enbl_amplifier
 */
#define TFA98XX_SYS_CONTROL0_AMPE                         (0x1<<3)
#define TFA98XX_SYS_CONTROL0_AMPE_POS                            3
#define TFA98XX_SYS_CONTROL0_AMPE_LEN                            1
#define TFA98XX_SYS_CONTROL0_AMPE_MAX                            1
#define TFA98XX_SYS_CONTROL0_AMPE_MSK                          0x8

/*
 * enbl_boost
 */
#define TFA98XX_SYS_CONTROL0_DCA                          (0x1<<4)
#define TFA98XX_SYS_CONTROL0_DCA_POS                             4
#define TFA98XX_SYS_CONTROL0_DCA_LEN                             1
#define TFA98XX_SYS_CONTROL0_DCA_MAX                             1
#define TFA98XX_SYS_CONTROL0_DCA_MSK                          0x10

/*
 * coolflux_configured
 */
#define TFA98XX_SYS_CONTROL0_SBSL                         (0x1<<5)
#define TFA98XX_SYS_CONTROL0_SBSL_POS                            5
#define TFA98XX_SYS_CONTROL0_SBSL_LEN                            1
#define TFA98XX_SYS_CONTROL0_SBSL_MAX                            1
#define TFA98XX_SYS_CONTROL0_SBSL_MSK                         0x20

/*
 * sel_enbl_amplifier
 */
#define TFA98XX_SYS_CONTROL0_AMPC                         (0x1<<6)
#define TFA98XX_SYS_CONTROL0_AMPC_POS                            6
#define TFA98XX_SYS_CONTROL0_AMPC_LEN                            1
#define TFA98XX_SYS_CONTROL0_AMPC_MAX                            1
#define TFA98XX_SYS_CONTROL0_AMPC_MSK                         0x40

/*
 * int_pad_io
 */
#define TFA98XX_SYS_CONTROL0_INTP                         (0x3<<7)
#define TFA98XX_SYS_CONTROL0_INTP_POS                            7
#define TFA98XX_SYS_CONTROL0_INTP_LEN                            2
#define TFA98XX_SYS_CONTROL0_INTP_MAX                            3
#define TFA98XX_SYS_CONTROL0_INTP_MSK                        0x180

/*
 * fs_pulse_sel
 */
#define TFA98XX_SYS_CONTROL0_FSSSEL                       (0x3<<9)
#define TFA98XX_SYS_CONTROL0_FSSSEL_POS                          9
#define TFA98XX_SYS_CONTROL0_FSSSEL_LEN                          2
#define TFA98XX_SYS_CONTROL0_FSSSEL_MAX                          3
#define TFA98XX_SYS_CONTROL0_FSSSEL_MSK                      0x600

/*
 * bypass_ocp
 */
#define TFA98XX_SYS_CONTROL0_BYPOCP                      (0x1<<11)
#define TFA98XX_SYS_CONTROL0_BYPOCP_POS                         11
#define TFA98XX_SYS_CONTROL0_BYPOCP_LEN                          1
#define TFA98XX_SYS_CONTROL0_BYPOCP_MAX                          1
#define TFA98XX_SYS_CONTROL0_BYPOCP_MSK                      0x800

/*
 * test_ocp
 */
#define TFA98XX_SYS_CONTROL0_TSTOCP                      (0x1<<12)
#define TFA98XX_SYS_CONTROL0_TSTOCP_POS                         12
#define TFA98XX_SYS_CONTROL0_TSTOCP_LEN                          1
#define TFA98XX_SYS_CONTROL0_TSTOCP_MAX                          1
#define TFA98XX_SYS_CONTROL0_TSTOCP_MSK                     0x1000


/*
 * (0x01)-sys_control1
 */

/*
 * vamp_sel
 */
#define TFA98XX_SYS_CONTROL1_AMPINSEL                     (0x3<<0)
#define TFA98XX_SYS_CONTROL1_AMPINSEL_POS                        0
#define TFA98XX_SYS_CONTROL1_AMPINSEL_LEN                        2
#define TFA98XX_SYS_CONTROL1_AMPINSEL_MAX                        3
#define TFA98XX_SYS_CONTROL1_AMPINSEL_MSK                      0x3

/*
 * src_set_configured
 */
#define TFA98XX_SYS_CONTROL1_MANSCONF                     (0x1<<2)
#define TFA98XX_SYS_CONTROL1_MANSCONF_POS                        2
#define TFA98XX_SYS_CONTROL1_MANSCONF_LEN                        1
#define TFA98XX_SYS_CONTROL1_MANSCONF_MAX                        1
#define TFA98XX_SYS_CONTROL1_MANSCONF_MSK                      0x4

/*
 * execute_cold_start
 */
#define TFA98XX_SYS_CONTROL1_MANCOLD                      (0x1<<3)
#define TFA98XX_SYS_CONTROL1_MANCOLD_POS                         3
#define TFA98XX_SYS_CONTROL1_MANCOLD_LEN                         1
#define TFA98XX_SYS_CONTROL1_MANCOLD_MAX                         1
#define TFA98XX_SYS_CONTROL1_MANCOLD_MSK                       0x8

/*
 * enbl_osc1m_auto_off
 */
#define TFA98XX_SYS_CONTROL1_MANAOOSC                     (0x1<<4)
#define TFA98XX_SYS_CONTROL1_MANAOOSC_POS                        4
#define TFA98XX_SYS_CONTROL1_MANAOOSC_LEN                        1
#define TFA98XX_SYS_CONTROL1_MANAOOSC_MAX                        1
#define TFA98XX_SYS_CONTROL1_MANAOOSC_MSK                     0x10

/*
 * man_enbl_brown_out
 */
#define TFA98XX_SYS_CONTROL1_MANROBOD                     (0x1<<5)
#define TFA98XX_SYS_CONTROL1_MANROBOD_POS                        5
#define TFA98XX_SYS_CONTROL1_MANROBOD_LEN                        1
#define TFA98XX_SYS_CONTROL1_MANROBOD_MAX                        1
#define TFA98XX_SYS_CONTROL1_MANROBOD_MSK                     0x20

/*
 * enbl_bod
 */
#define TFA98XX_SYS_CONTROL1_BODE                         (0x1<<6)
#define TFA98XX_SYS_CONTROL1_BODE_POS                            6
#define TFA98XX_SYS_CONTROL1_BODE_LEN                            1
#define TFA98XX_SYS_CONTROL1_BODE_MAX                            1
#define TFA98XX_SYS_CONTROL1_BODE_MSK                         0x40

/*
 * enbl_bod_hyst
 */
#define TFA98XX_SYS_CONTROL1_BODHYS                       (0x1<<7)
#define TFA98XX_SYS_CONTROL1_BODHYS_POS                          7
#define TFA98XX_SYS_CONTROL1_BODHYS_LEN                          1
#define TFA98XX_SYS_CONTROL1_BODHYS_MAX                          1
#define TFA98XX_SYS_CONTROL1_BODHYS_MSK                       0x80

/*
 * bod_delay
 */
#define TFA98XX_SYS_CONTROL1_BODFILT                      (0x3<<8)
#define TFA98XX_SYS_CONTROL1_BODFILT_POS                         8
#define TFA98XX_SYS_CONTROL1_BODFILT_LEN                         2
#define TFA98XX_SYS_CONTROL1_BODFILT_MAX                         3
#define TFA98XX_SYS_CONTROL1_BODFILT_MSK                     0x300

/*
 * bod_lvlsel
 */
#define TFA98XX_SYS_CONTROL1_BODTHLVL                    (0x3<<10)
#define TFA98XX_SYS_CONTROL1_BODTHLVL_POS                       10
#define TFA98XX_SYS_CONTROL1_BODTHLVL_LEN                        2
#define TFA98XX_SYS_CONTROL1_BODTHLVL_MAX                        3
#define TFA98XX_SYS_CONTROL1_BODTHLVL_MSK                    0xc00

/*
 * disable_mute_time_out
 */
#define TFA98XX_SYS_CONTROL1_MUTETO                      (0x1<<13)
#define TFA98XX_SYS_CONTROL1_MUTETO_POS                         13
#define TFA98XX_SYS_CONTROL1_MUTETO_LEN                          1
#define TFA98XX_SYS_CONTROL1_MUTETO_MAX                          1
#define TFA98XX_SYS_CONTROL1_MUTETO_MSK                     0x2000

/*
 * pwm_sel_rcv_ns
 */
#define TFA98XX_SYS_CONTROL1_RCVNS                       (0x1<<14)
#define TFA98XX_SYS_CONTROL1_RCVNS_POS                          14
#define TFA98XX_SYS_CONTROL1_RCVNS_LEN                           1
#define TFA98XX_SYS_CONTROL1_RCVNS_MAX                           1
#define TFA98XX_SYS_CONTROL1_RCVNS_MSK                      0x4000

/*
 * man_enbl_watchdog
 */
#define TFA98XX_SYS_CONTROL1_MANWDE                      (0x1<<15)
#define TFA98XX_SYS_CONTROL1_MANWDE_POS                         15
#define TFA98XX_SYS_CONTROL1_MANWDE_LEN                          1
#define TFA98XX_SYS_CONTROL1_MANWDE_MAX                          1
#define TFA98XX_SYS_CONTROL1_MANWDE_MSK                     0x8000


/*
 * (0x02)-sys_control2
 */

/*
 * audio_fs
 */
#define TFA98XX_SYS_CONTROL2_AUDFS                        (0xf<<0)
#define TFA98XX_SYS_CONTROL2_AUDFS_POS                           0
#define TFA98XX_SYS_CONTROL2_AUDFS_LEN                           4
#define TFA98XX_SYS_CONTROL2_AUDFS_MAX                          15
#define TFA98XX_SYS_CONTROL2_AUDFS_MSK                         0xf

/*
 * input_level
 */
#define TFA98XX_SYS_CONTROL2_INPLEV                       (0x1<<4)
#define TFA98XX_SYS_CONTROL2_INPLEV_POS                          4
#define TFA98XX_SYS_CONTROL2_INPLEV_LEN                          1
#define TFA98XX_SYS_CONTROL2_INPLEV_MAX                          1
#define TFA98XX_SYS_CONTROL2_INPLEV_MSK                       0x10

/*
 * cs_frac_delay
 */
#define TFA98XX_SYS_CONTROL2_FRACTDEL                    (0x3f<<5)
#define TFA98XX_SYS_CONTROL2_FRACTDEL_POS                        5
#define TFA98XX_SYS_CONTROL2_FRACTDEL_LEN                        6
#define TFA98XX_SYS_CONTROL2_FRACTDEL_MAX                       63
#define TFA98XX_SYS_CONTROL2_FRACTDEL_MSK                    0x7e0

/*
 * bypass_hvbat_filter
 */
#define TFA98XX_SYS_CONTROL2_BYPHVBF                     (0x1<<11)
#define TFA98XX_SYS_CONTROL2_BYPHVBF_POS                        11
#define TFA98XX_SYS_CONTROL2_BYPHVBF_LEN                         1
#define TFA98XX_SYS_CONTROL2_BYPHVBF_MAX                         1
#define TFA98XX_SYS_CONTROL2_BYPHVBF_MSK                     0x800

/*
 * ctrl_rcvldop_bypass
 */
#define TFA98XX_SYS_CONTROL2_LDOBYP                      (0x1<<12)
#define TFA98XX_SYS_CONTROL2_LDOBYP_POS                         12
#define TFA98XX_SYS_CONTROL2_LDOBYP_LEN                          1
#define TFA98XX_SYS_CONTROL2_LDOBYP_MAX                          1
#define TFA98XX_SYS_CONTROL2_LDOBYP_MSK                     0x1000


/*
 * (0x03)-device_revision
 */

/*
 * device_rev
 */
#define TFA98XX_DEVICE_REVISION_REV                    (0xffff<<0)
#define TFA98XX_DEVICE_REVISION_REV_POS                          0
#define TFA98XX_DEVICE_REVISION_REV_LEN                         16
#define TFA98XX_DEVICE_REVISION_REV_MAX                      65535
#define TFA98XX_DEVICE_REVISION_REV_MSK                     0xffff


/*
 * (0x04)-clock_control
 */

/*
 * pll_clkin_sel
 */
#define TFA98XX_CLOCK_CONTROL_REFCKEXT                    (0x3<<0)
#define TFA98XX_CLOCK_CONTROL_REFCKEXT_POS                       0
#define TFA98XX_CLOCK_CONTROL_REFCKEXT_LEN                       2
#define TFA98XX_CLOCK_CONTROL_REFCKEXT_MAX                       3
#define TFA98XX_CLOCK_CONTROL_REFCKEXT_MSK                     0x3

/*
 * pll_clkin_sel_osc
 */
#define TFA98XX_CLOCK_CONTROL_REFCKSEL                    (0x1<<2)
#define TFA98XX_CLOCK_CONTROL_REFCKSEL_POS                       2
#define TFA98XX_CLOCK_CONTROL_REFCKSEL_LEN                       1
#define TFA98XX_CLOCK_CONTROL_REFCKSEL_MAX                       1
#define TFA98XX_CLOCK_CONTROL_REFCKSEL_MSK                     0x4


/*
 * (0x05)-clock_gating_control
 */

/*
 * enbl_spkr_ss_left
 */
#define TFA98XX_CLOCK_GATING_CONTROL_SSLEFTE              (0x1<<0)
#define TFA98XX_CLOCK_GATING_CONTROL_SSLEFTE_POS                 0
#define TFA98XX_CLOCK_GATING_CONTROL_SSLEFTE_LEN                 1
#define TFA98XX_CLOCK_GATING_CONTROL_SSLEFTE_MAX                 1
#define TFA98XX_CLOCK_GATING_CONTROL_SSLEFTE_MSK               0x1

/*
 * enbl_spkr_ss_right
 */
#define TFA98XX_CLOCK_GATING_CONTROL_SSRIGHTE             (0x1<<1)
#define TFA98XX_CLOCK_GATING_CONTROL_SSRIGHTE_POS                1
#define TFA98XX_CLOCK_GATING_CONTROL_SSRIGHTE_LEN                1
#define TFA98XX_CLOCK_GATING_CONTROL_SSRIGHTE_MAX                1
#define TFA98XX_CLOCK_GATING_CONTROL_SSRIGHTE_MSK              0x2

/*
 * enbl_volsense_left
 */
#define TFA98XX_CLOCK_GATING_CONTROL_VSLEFTE              (0x1<<2)
#define TFA98XX_CLOCK_GATING_CONTROL_VSLEFTE_POS                 2
#define TFA98XX_CLOCK_GATING_CONTROL_VSLEFTE_LEN                 1
#define TFA98XX_CLOCK_GATING_CONTROL_VSLEFTE_MAX                 1
#define TFA98XX_CLOCK_GATING_CONTROL_VSLEFTE_MSK               0x4

/*
 * enbl_volsense_right
 */
#define TFA98XX_CLOCK_GATING_CONTROL_VSRIGHTE             (0x1<<3)
#define TFA98XX_CLOCK_GATING_CONTROL_VSRIGHTE_POS                3
#define TFA98XX_CLOCK_GATING_CONTROL_VSRIGHTE_LEN                1
#define TFA98XX_CLOCK_GATING_CONTROL_VSRIGHTE_MAX                1
#define TFA98XX_CLOCK_GATING_CONTROL_VSRIGHTE_MSK              0x8

/*
 * enbl_cursense_left
 */
#define TFA98XX_CLOCK_GATING_CONTROL_CSLEFTE              (0x1<<4)
#define TFA98XX_CLOCK_GATING_CONTROL_CSLEFTE_POS                 4
#define TFA98XX_CLOCK_GATING_CONTROL_CSLEFTE_LEN                 1
#define TFA98XX_CLOCK_GATING_CONTROL_CSLEFTE_MAX                 1
#define TFA98XX_CLOCK_GATING_CONTROL_CSLEFTE_MSK              0x10

/*
 * enbl_cursense_right
 */
#define TFA98XX_CLOCK_GATING_CONTROL_CSRIGHTE             (0x1<<5)
#define TFA98XX_CLOCK_GATING_CONTROL_CSRIGHTE_POS                5
#define TFA98XX_CLOCK_GATING_CONTROL_CSRIGHTE_LEN                1
#define TFA98XX_CLOCK_GATING_CONTROL_CSRIGHTE_MAX                1
#define TFA98XX_CLOCK_GATING_CONTROL_CSRIGHTE_MSK             0x20

/*
 * enbl_pdm_ss
 */
#define TFA98XX_CLOCK_GATING_CONTROL_SSPDME               (0x1<<6)
#define TFA98XX_CLOCK_GATING_CONTROL_SSPDME_POS                  6
#define TFA98XX_CLOCK_GATING_CONTROL_SSPDME_LEN                  1
#define TFA98XX_CLOCK_GATING_CONTROL_SSPDME_MAX                  1
#define TFA98XX_CLOCK_GATING_CONTROL_SSPDME_MSK               0x40


/*
 * (0x0d)-side_tone_config
 */

/*
 * side_tone_gain
 */
#define TFA98XX_SIDE_TONE_CONFIG_STGAIN                 (0x1ff<<1)
#define TFA98XX_SIDE_TONE_CONFIG_STGAIN_POS                      1
#define TFA98XX_SIDE_TONE_CONFIG_STGAIN_LEN                      9
#define TFA98XX_SIDE_TONE_CONFIG_STGAIN_MAX                    511
#define TFA98XX_SIDE_TONE_CONFIG_STGAIN_MSK                  0x3fe

/*
 * mute_side_tone
 */
#define TFA98XX_SIDE_TONE_CONFIG_PDMSMUTE                (0x1<<10)
#define TFA98XX_SIDE_TONE_CONFIG_PDMSMUTE_POS                   10
#define TFA98XX_SIDE_TONE_CONFIG_PDMSMUTE_LEN                    1
#define TFA98XX_SIDE_TONE_CONFIG_PDMSMUTE_MAX                    1
#define TFA98XX_SIDE_TONE_CONFIG_PDMSMUTE_MSK                0x400


/*
 * (0x0e)-ctrl_digtoana_reg
 */

/*
 * ctrl_digtoana
 */
#define TFA98XX_CTRL_DIGTOANA_REG_SWVSTEP                (0x7f<<0)
#define TFA98XX_CTRL_DIGTOANA_REG_SWVSTEP_POS                    0
#define TFA98XX_CTRL_DIGTOANA_REG_SWVSTEP_LEN                    7
#define TFA98XX_CTRL_DIGTOANA_REG_SWVSTEP_MAX                  127
#define TFA98XX_CTRL_DIGTOANA_REG_SWVSTEP_MSK                 0x7f


/*
 * (0x10)-status_flags0
 */

/*
 * flag_por
 */
#define TFA98XX_STATUS_FLAGS0_VDDS                        (0x1<<0)
#define TFA98XX_STATUS_FLAGS0_VDDS_POS                           0
#define TFA98XX_STATUS_FLAGS0_VDDS_LEN                           1
#define TFA98XX_STATUS_FLAGS0_VDDS_MAX                           1
#define TFA98XX_STATUS_FLAGS0_VDDS_MSK                         0x1

/*
 * flag_pll_lock
 */
#define TFA98XX_STATUS_FLAGS0_PLLS                        (0x1<<1)
#define TFA98XX_STATUS_FLAGS0_PLLS_POS                           1
#define TFA98XX_STATUS_FLAGS0_PLLS_LEN                           1
#define TFA98XX_STATUS_FLAGS0_PLLS_MAX                           1
#define TFA98XX_STATUS_FLAGS0_PLLS_MSK                         0x2

/*
 * flag_otpok
 */
#define TFA98XX_STATUS_FLAGS0_OTDS                        (0x1<<2)
#define TFA98XX_STATUS_FLAGS0_OTDS_POS                           2
#define TFA98XX_STATUS_FLAGS0_OTDS_LEN                           1
#define TFA98XX_STATUS_FLAGS0_OTDS_MAX                           1
#define TFA98XX_STATUS_FLAGS0_OTDS_MSK                         0x4

/*
 * flag_ovpok
 */
#define TFA98XX_STATUS_FLAGS0_OVDS                        (0x1<<3)
#define TFA98XX_STATUS_FLAGS0_OVDS_POS                           3
#define TFA98XX_STATUS_FLAGS0_OVDS_LEN                           1
#define TFA98XX_STATUS_FLAGS0_OVDS_MAX                           1
#define TFA98XX_STATUS_FLAGS0_OVDS_MSK                         0x8

/*
 * flag_uvpok
 */
#define TFA98XX_STATUS_FLAGS0_UVDS                        (0x1<<4)
#define TFA98XX_STATUS_FLAGS0_UVDS_POS                           4
#define TFA98XX_STATUS_FLAGS0_UVDS_LEN                           1
#define TFA98XX_STATUS_FLAGS0_UVDS_MAX                           1
#define TFA98XX_STATUS_FLAGS0_UVDS_MSK                        0x10

/*
 * flag_clocks_stable
 */
#define TFA98XX_STATUS_FLAGS0_CLKS                        (0x1<<6)
#define TFA98XX_STATUS_FLAGS0_CLKS_POS                           5
#define TFA98XX_STATUS_FLAGS0_CLKS_LEN                           1
#define TFA98XX_STATUS_FLAGS0_CLKS_MAX                           1
#define TFA98XX_STATUS_FLAGS0_CLKS_MSK                        0x20

/*
 * flag_mtp_busy
 */
#define TFA98XX_STATUS_FLAGS0_MTPB                        (0x1<<6)
#define TFA98XX_STATUS_FLAGS0_MTPB_POS                           6
#define TFA98XX_STATUS_FLAGS0_MTPB_LEN                           1
#define TFA98XX_STATUS_FLAGS0_MTPB_MAX                           1
#define TFA98XX_STATUS_FLAGS0_MTPB_MSK                        0x40

/*
 * flag_lost_clk
 */
#define TFA98XX_STATUS_FLAGS0_NOCLK                       (0x1<<7)
#define TFA98XX_STATUS_FLAGS0_NOCLK_POS                          7
#define TFA98XX_STATUS_FLAGS0_NOCLK_LEN                          1
#define TFA98XX_STATUS_FLAGS0_NOCLK_MAX                          1
#define TFA98XX_STATUS_FLAGS0_NOCLK_MSK                       0x80

/*
 * flag_cf_speakererror
 */
#define TFA98XX_STATUS_FLAGS0_SPKS                        (0x1<<8)
#define TFA98XX_STATUS_FLAGS0_SPKS_POS                           8
#define TFA98XX_STATUS_FLAGS0_SPKS_LEN                           1
#define TFA98XX_STATUS_FLAGS0_SPKS_MAX                           1
#define TFA98XX_STATUS_FLAGS0_SPKS_MSK                       0x100

/*
 * flag_cold_started
 */
#define TFA98XX_STATUS_FLAGS0_ACS                         (0x1<<9)
#define TFA98XX_STATUS_FLAGS0_ACS_POS                            9
#define TFA98XX_STATUS_FLAGS0_ACS_LEN                            1
#define TFA98XX_STATUS_FLAGS0_ACS_MAX                            1
#define TFA98XX_STATUS_FLAGS0_ACS_MSK                        0x200

/*
 * flag_engage
 */
#define TFA98XX_STATUS_FLAGS0_SWS                        (0x1<<10)
#define TFA98XX_STATUS_FLAGS0_SWS_POS                           10
#define TFA98XX_STATUS_FLAGS0_SWS_LEN                            1
#define TFA98XX_STATUS_FLAGS0_SWS_MAX                            1
#define TFA98XX_STATUS_FLAGS0_SWS_MSK                        0x400

/*
 * flag_watchdog_reset
 */
#define TFA98XX_STATUS_FLAGS0_WDS                        (0x1<<11)
#define TFA98XX_STATUS_FLAGS0_WDS_POS                           11
#define TFA98XX_STATUS_FLAGS0_WDS_LEN                            1
#define TFA98XX_STATUS_FLAGS0_WDS_MAX                            1
#define TFA98XX_STATUS_FLAGS0_WDS_MSK                        0x800

/*
 * flag_enbl_amp
 */
#define TFA98XX_STATUS_FLAGS0_AMPS                       (0x1<<12)
#define TFA98XX_STATUS_FLAGS0_AMPS_POS                          12
#define TFA98XX_STATUS_FLAGS0_AMPS_LEN                           1
#define TFA98XX_STATUS_FLAGS0_AMPS_MAX                           1
#define TFA98XX_STATUS_FLAGS0_AMPS_MSK                      0x1000

/*
 * flag_enbl_ref
 */
#define TFA98XX_STATUS_FLAGS0_AREFS                      (0x1<<13)
#define TFA98XX_STATUS_FLAGS0_AREFS_POS                         13
#define TFA98XX_STATUS_FLAGS0_AREFS_LEN                          1
#define TFA98XX_STATUS_FLAGS0_AREFS_MAX                          1
#define TFA98XX_STATUS_FLAGS0_AREFS_MSK                     0x2000

/*
 * flag_adc10_ready
 */
#define TFA98XX_STATUS_FLAGS0_ADCCR                      (0x1<<14)
#define TFA98XX_STATUS_FLAGS0_ADCCR_POS                         14
#define TFA98XX_STATUS_FLAGS0_ADCCR_LEN                          1
#define TFA98XX_STATUS_FLAGS0_ADCCR_MAX                          1
#define TFA98XX_STATUS_FLAGS0_ADCCR_MSK                     0x4000

/*
 * flag_bod_vddd_nok
 */
#define TFA98XX_STATUS_FLAGS0_BODNOK                     (0x1<<15)
#define TFA98XX_STATUS_FLAGS0_BODNOK_POS                        15
#define TFA98XX_STATUS_FLAGS0_BODNOK_LEN                         1
#define TFA98XX_STATUS_FLAGS0_BODNOK_MAX                         1
#define TFA98XX_STATUS_FLAGS0_BODNOK_MSK                    0x8000


/*
 * (0x11)-status_flags1
 */

/*
 * flag_bst_bstcur
 */
#define TFA98XX_STATUS_FLAGS1_DCIL                        (0x1<<0)
#define TFA98XX_STATUS_FLAGS1_DCIL_POS                           0
#define TFA98XX_STATUS_FLAGS1_DCIL_LEN                           1
#define TFA98XX_STATUS_FLAGS1_DCIL_MAX                           1
#define TFA98XX_STATUS_FLAGS1_DCIL_MSK                         0x1

/*
 * flag_bst_hiz
 */
#define TFA98XX_STATUS_FLAGS1_DCDCA                       (0x1<<1)
#define TFA98XX_STATUS_FLAGS1_DCDCA_POS                          1
#define TFA98XX_STATUS_FLAGS1_DCDCA_LEN                          1
#define TFA98XX_STATUS_FLAGS1_DCDCA_MAX                          1
#define TFA98XX_STATUS_FLAGS1_DCDCA_MSK                        0x2

/*
 * flag_bst_ocpok
 */
#define TFA98XX_STATUS_FLAGS1_DCOCPOK                     (0x1<<2)
#define TFA98XX_STATUS_FLAGS1_DCOCPOK_POS                        2
#define TFA98XX_STATUS_FLAGS1_DCOCPOK_LEN                        1
#define TFA98XX_STATUS_FLAGS1_DCOCPOK_MAX                        1
#define TFA98XX_STATUS_FLAGS1_DCOCPOK_MSK                      0x4

/*
 * flag_bst_voutcomp
 */
#define TFA98XX_STATUS_FLAGS1_DCHVBAT                     (0x1<<4)
#define TFA98XX_STATUS_FLAGS1_DCHVBAT_POS                        4
#define TFA98XX_STATUS_FLAGS1_DCHVBAT_LEN                        1
#define TFA98XX_STATUS_FLAGS1_DCHVBAT_MAX                        1
#define TFA98XX_STATUS_FLAGS1_DCHVBAT_MSK                     0x10

/*
 * flag_bst_voutcomp86
 */
#define TFA98XX_STATUS_FLAGS1_DCH114                      (0x1<<5)
#define TFA98XX_STATUS_FLAGS1_DCH114_POS                         5
#define TFA98XX_STATUS_FLAGS1_DCH114_LEN                         1
#define TFA98XX_STATUS_FLAGS1_DCH114_MAX                         1
#define TFA98XX_STATUS_FLAGS1_DCH114_MSK                      0x20

/*
 * flag_bst_voutcomp93
 */
#define TFA98XX_STATUS_FLAGS1_DCH107                      (0x1<<6)
#define TFA98XX_STATUS_FLAGS1_DCH107_POS                         6
#define TFA98XX_STATUS_FLAGS1_DCH107_LEN                         1
#define TFA98XX_STATUS_FLAGS1_DCH107_MAX                         1
#define TFA98XX_STATUS_FLAGS1_DCH107_MSK                      0x40

/*
 * flag_soft_mute_busy
 */
#define TFA98XX_STATUS_FLAGS1_STMUTEB                     (0x1<<7)
#define TFA98XX_STATUS_FLAGS1_STMUTEB_POS                        7
#define TFA98XX_STATUS_FLAGS1_STMUTEB_LEN                        1
#define TFA98XX_STATUS_FLAGS1_STMUTEB_MAX                        1
#define TFA98XX_STATUS_FLAGS1_STMUTEB_MSK                     0x80

/*
 * flag_soft_mute_state
 */
#define TFA98XX_STATUS_FLAGS1_STMUTE                      (0x1<<8)
#define TFA98XX_STATUS_FLAGS1_STMUTE_POS                         8
#define TFA98XX_STATUS_FLAGS1_STMUTE_LEN                         1
#define TFA98XX_STATUS_FLAGS1_STMUTE_MAX                         1
#define TFA98XX_STATUS_FLAGS1_STMUTE_MSK                     0x100

/*
 * flag_tdm_lut_error
 */
#define TFA98XX_STATUS_FLAGS1_TDMLUTER                    (0x1<<9)
#define TFA98XX_STATUS_FLAGS1_TDMLUTER_POS                       9
#define TFA98XX_STATUS_FLAGS1_TDMLUTER_LEN                       1
#define TFA98XX_STATUS_FLAGS1_TDMLUTER_MAX                       1
#define TFA98XX_STATUS_FLAGS1_TDMLUTER_MSK                   0x200

/*
 * flag_tdm_status
 */
#define TFA98XX_STATUS_FLAGS1_TDMSTAT                    (0x7<<10)
#define TFA98XX_STATUS_FLAGS1_TDMSTAT_POS                       10
#define TFA98XX_STATUS_FLAGS1_TDMSTAT_LEN                        3
#define TFA98XX_STATUS_FLAGS1_TDMSTAT_MAX                        7
#define TFA98XX_STATUS_FLAGS1_TDMSTAT_MSK                   0x1c00

/*
 * flag_tdm_error
 */
#define TFA98XX_STATUS_FLAGS1_TDMERR                     (0x1<<13)
#define TFA98XX_STATUS_FLAGS1_TDMERR_POS                        13
#define TFA98XX_STATUS_FLAGS1_TDMERR_LEN                         1
#define TFA98XX_STATUS_FLAGS1_TDMERR_MAX                         1
#define TFA98XX_STATUS_FLAGS1_TDMERR_MSK                    0x2000

/*
 * flag_haptic_busy
 */
#define TFA98XX_STATUS_FLAGS1_HAPTIC                     (0x1<<14)
#define TFA98XX_STATUS_FLAGS1_HAPTIC_POS                        14
#define TFA98XX_STATUS_FLAGS1_HAPTIC_LEN                         1
#define TFA98XX_STATUS_FLAGS1_HAPTIC_MAX                         1
#define TFA98XX_STATUS_FLAGS1_HAPTIC_MSK                    0x4000


/*
 * (0x12)-status_flags2
 */

/*
 * flag_ocpokap_left
 */
#define TFA98XX_STATUS_FLAGS2_OCPOAPL                     (0x1<<0)
#define TFA98XX_STATUS_FLAGS2_OCPOAPL_POS                        0
#define TFA98XX_STATUS_FLAGS2_OCPOAPL_LEN                        1
#define TFA98XX_STATUS_FLAGS2_OCPOAPL_MAX                        1
#define TFA98XX_STATUS_FLAGS2_OCPOAPL_MSK                      0x1

/*
 * flag_ocpokan_left
 */
#define TFA98XX_STATUS_FLAGS2_OCPOANL                     (0x1<<1)
#define TFA98XX_STATUS_FLAGS2_OCPOANL_POS                        1
#define TFA98XX_STATUS_FLAGS2_OCPOANL_LEN                        1
#define TFA98XX_STATUS_FLAGS2_OCPOANL_MAX                        1
#define TFA98XX_STATUS_FLAGS2_OCPOANL_MSK                      0x2

/*
 * flag_ocpokbp_left
 */
#define TFA98XX_STATUS_FLAGS2_OCPOBPL                     (0x1<<2)
#define TFA98XX_STATUS_FLAGS2_OCPOBPL_POS                        2
#define TFA98XX_STATUS_FLAGS2_OCPOBPL_LEN                        1
#define TFA98XX_STATUS_FLAGS2_OCPOBPL_MAX                        1
#define TFA98XX_STATUS_FLAGS2_OCPOBPL_MSK                      0x4

/*
 * flag_ocpokbn_left
 */
#define TFA98XX_STATUS_FLAGS2_OCPOBNL                     (0x1<<3)
#define TFA98XX_STATUS_FLAGS2_OCPOBNL_POS                        3
#define TFA98XX_STATUS_FLAGS2_OCPOBNL_LEN                        1
#define TFA98XX_STATUS_FLAGS2_OCPOBNL_MAX                        1
#define TFA98XX_STATUS_FLAGS2_OCPOBNL_MSK                      0x8

/*
 * flag_clipa_high_left
 */
#define TFA98XX_STATUS_FLAGS2_CLIPAHL                     (0x1<<4)
#define TFA98XX_STATUS_FLAGS2_CLIPAHL_POS                        4
#define TFA98XX_STATUS_FLAGS2_CLIPAHL_LEN                        1
#define TFA98XX_STATUS_FLAGS2_CLIPAHL_MAX                        1
#define TFA98XX_STATUS_FLAGS2_CLIPAHL_MSK                     0x10

/*
 * flag_clipa_low_left
 */
#define TFA98XX_STATUS_FLAGS2_CLIPALL                     (0x1<<5)
#define TFA98XX_STATUS_FLAGS2_CLIPALL_POS                        5
#define TFA98XX_STATUS_FLAGS2_CLIPALL_LEN                        1
#define TFA98XX_STATUS_FLAGS2_CLIPALL_MAX                        1
#define TFA98XX_STATUS_FLAGS2_CLIPALL_MSK                     0x20

/*
 * flag_clipb_high_left
 */
#define TFA98XX_STATUS_FLAGS2_CLIPBHL                     (0x1<<6)
#define TFA98XX_STATUS_FLAGS2_CLIPBHL_POS                        6
#define TFA98XX_STATUS_FLAGS2_CLIPBHL_LEN                        1
#define TFA98XX_STATUS_FLAGS2_CLIPBHL_MAX                        1
#define TFA98XX_STATUS_FLAGS2_CLIPBHL_MSK                     0x40

/*
 * flag_clipb_low_left
 */
#define TFA98XX_STATUS_FLAGS2_CLIPBLL                     (0x1<<7)
#define TFA98XX_STATUS_FLAGS2_CLIPBLL_POS                        7
#define TFA98XX_STATUS_FLAGS2_CLIPBLL_LEN                        1
#define TFA98XX_STATUS_FLAGS2_CLIPBLL_MAX                        1
#define TFA98XX_STATUS_FLAGS2_CLIPBLL_MSK                     0x80

/*
 * flag_ocpokap_rcv
 */
#define TFA98XX_STATUS_FLAGS2_OCPOAPRC                    (0x1<<8)
#define TFA98XX_STATUS_FLAGS2_OCPOAPRC_POS                       8
#define TFA98XX_STATUS_FLAGS2_OCPOAPRC_LEN                       1
#define TFA98XX_STATUS_FLAGS2_OCPOAPRC_MAX                       1
#define TFA98XX_STATUS_FLAGS2_OCPOAPRC_MSK                   0x100

/*
 * flag_ocpokan_rcv
 */
#define TFA98XX_STATUS_FLAGS2_OCPOANRC                    (0x1<<9)
#define TFA98XX_STATUS_FLAGS2_OCPOANRC_POS                       9
#define TFA98XX_STATUS_FLAGS2_OCPOANRC_LEN                       1
#define TFA98XX_STATUS_FLAGS2_OCPOANRC_MAX                       1
#define TFA98XX_STATUS_FLAGS2_OCPOANRC_MSK                   0x200

/*
 * flag_ocpokbp_rcv
 */
#define TFA98XX_STATUS_FLAGS2_OCPOBPRC                   (0x1<<10)
#define TFA98XX_STATUS_FLAGS2_OCPOBPRC_POS                      10
#define TFA98XX_STATUS_FLAGS2_OCPOBPRC_LEN                       1
#define TFA98XX_STATUS_FLAGS2_OCPOBPRC_MAX                       1
#define TFA98XX_STATUS_FLAGS2_OCPOBPRC_MSK                   0x400

/*
 * flag_ocpokbn_rcv
 */
#define TFA98XX_STATUS_FLAGS2_OCPOBNRC                   (0x1<<11)
#define TFA98XX_STATUS_FLAGS2_OCPOBNRC_POS                      11
#define TFA98XX_STATUS_FLAGS2_OCPOBNRC_LEN                       1
#define TFA98XX_STATUS_FLAGS2_OCPOBNRC_MAX                       1
#define TFA98XX_STATUS_FLAGS2_OCPOBNRC_MSK                   0x800

/*
 * flag_rcvldop_ready
 */
#define TFA98XX_STATUS_FLAGS2_RCVLDOR                    (0x1<<12)
#define TFA98XX_STATUS_FLAGS2_RCVLDOR_POS                       12
#define TFA98XX_STATUS_FLAGS2_RCVLDOR_LEN                        1
#define TFA98XX_STATUS_FLAGS2_RCVLDOR_MAX                        1
#define TFA98XX_STATUS_FLAGS2_RCVLDOR_MSK                   0x1000

/*
 * flag_rcvldop_bypassready
 */
#define TFA98XX_STATUS_FLAGS2_RCVLDOBR                   (0x1<<13)
#define TFA98XX_STATUS_FLAGS2_RCVLDOBR_POS                      13
#define TFA98XX_STATUS_FLAGS2_RCVLDOBR_LEN                       1
#define TFA98XX_STATUS_FLAGS2_RCVLDOBR_MAX                       1
#define TFA98XX_STATUS_FLAGS2_RCVLDOBR_MSK                  0x2000

/*
 * flag_ocp_alarm_left
 */
#define TFA98XX_STATUS_FLAGS2_OCDSL                      (0x1<<14)
#define TFA98XX_STATUS_FLAGS2_OCDSL_POS                         14
#define TFA98XX_STATUS_FLAGS2_OCDSL_LEN                          1
#define TFA98XX_STATUS_FLAGS2_OCDSL_MAX                          1
#define TFA98XX_STATUS_FLAGS2_OCDSL_MSK                     0x4000

/*
 * flag_clip_left
 */
#define TFA98XX_STATUS_FLAGS2_CLIPSL                     (0x1<<15)
#define TFA98XX_STATUS_FLAGS2_CLIPSL_POS                        15
#define TFA98XX_STATUS_FLAGS2_CLIPSL_LEN                         1
#define TFA98XX_STATUS_FLAGS2_CLIPSL_MAX                         1
#define TFA98XX_STATUS_FLAGS2_CLIPSL_MSK                    0x8000


/*
 * (0x13)-status_flags3
 */

/*
 * flag_ocpokap_right
 */
#define TFA98XX_STATUS_FLAGS3_OCPOAPR                     (0x1<<0)
#define TFA98XX_STATUS_FLAGS3_OCPOAPR_POS                        0
#define TFA98XX_STATUS_FLAGS3_OCPOAPR_LEN                        1
#define TFA98XX_STATUS_FLAGS3_OCPOAPR_MAX                        1
#define TFA98XX_STATUS_FLAGS3_OCPOAPR_MSK                      0x1

/*
 * flag_ocpokan_right
 */
#define TFA98XX_STATUS_FLAGS3_OCPOANR                     (0x1<<1)
#define TFA98XX_STATUS_FLAGS3_OCPOANR_POS                        1
#define TFA98XX_STATUS_FLAGS3_OCPOANR_LEN                        1
#define TFA98XX_STATUS_FLAGS3_OCPOANR_MAX                        1
#define TFA98XX_STATUS_FLAGS3_OCPOANR_MSK                      0x2

/*
 * flag_ocpokbp_right
 */
#define TFA98XX_STATUS_FLAGS3_OCPOBPR                     (0x1<<2)
#define TFA98XX_STATUS_FLAGS3_OCPOBPR_POS                        2
#define TFA98XX_STATUS_FLAGS3_OCPOBPR_LEN                        1
#define TFA98XX_STATUS_FLAGS3_OCPOBPR_MAX                        1
#define TFA98XX_STATUS_FLAGS3_OCPOBPR_MSK                      0x4

/*
 * flag_ocpokbn_right
 */
#define TFA98XX_STATUS_FLAGS3_OCPOBNR                     (0x1<<3)
#define TFA98XX_STATUS_FLAGS3_OCPOBNR_POS                        3
#define TFA98XX_STATUS_FLAGS3_OCPOBNR_LEN                        1
#define TFA98XX_STATUS_FLAGS3_OCPOBNR_MAX                        1
#define TFA98XX_STATUS_FLAGS3_OCPOBNR_MSK                      0x8

/*
 * flag_clipa_high_right
 */
#define TFA98XX_STATUS_FLAGS3_CLIPAHR                     (0x1<<4)
#define TFA98XX_STATUS_FLAGS3_CLIPAHR_POS                        4
#define TFA98XX_STATUS_FLAGS3_CLIPAHR_LEN                        1
#define TFA98XX_STATUS_FLAGS3_CLIPAHR_MAX                        1
#define TFA98XX_STATUS_FLAGS3_CLIPAHR_MSK                     0x10

/*
 * flag_clipa_low_right
 */
#define TFA98XX_STATUS_FLAGS3_CLIPALR                     (0x1<<5)
#define TFA98XX_STATUS_FLAGS3_CLIPALR_POS                        5
#define TFA98XX_STATUS_FLAGS3_CLIPALR_LEN                        1
#define TFA98XX_STATUS_FLAGS3_CLIPALR_MAX                        1
#define TFA98XX_STATUS_FLAGS3_CLIPALR_MSK                     0x20

/*
 * flag_clipb_high_right
 */
#define TFA98XX_STATUS_FLAGS3_CLIPBHR                     (0x1<<6)
#define TFA98XX_STATUS_FLAGS3_CLIPBHR_POS                        6
#define TFA98XX_STATUS_FLAGS3_CLIPBHR_LEN                        1
#define TFA98XX_STATUS_FLAGS3_CLIPBHR_MAX                        1
#define TFA98XX_STATUS_FLAGS3_CLIPBHR_MSK                     0x40

/*
 * flag_clipb_low_right
 */
#define TFA98XX_STATUS_FLAGS3_CLIPBLR                     (0x1<<7)
#define TFA98XX_STATUS_FLAGS3_CLIPBLR_POS                        7
#define TFA98XX_STATUS_FLAGS3_CLIPBLR_LEN                        1
#define TFA98XX_STATUS_FLAGS3_CLIPBLR_MAX                        1
#define TFA98XX_STATUS_FLAGS3_CLIPBLR_MSK                     0x80

/*
 * flag_ocp_alarm_right
 */
#define TFA98XX_STATUS_FLAGS3_OCDSR                       (0x1<<8)
#define TFA98XX_STATUS_FLAGS3_OCDSR_POS                          8
#define TFA98XX_STATUS_FLAGS3_OCDSR_LEN                          1
#define TFA98XX_STATUS_FLAGS3_OCDSR_MAX                          1
#define TFA98XX_STATUS_FLAGS3_OCDSR_MSK                      0x100

/*
 * flag_clip_right
 */
#define TFA98XX_STATUS_FLAGS3_CLIPSR                      (0x1<<9)
#define TFA98XX_STATUS_FLAGS3_CLIPSR_POS                         9
#define TFA98XX_STATUS_FLAGS3_CLIPSR_LEN                         1
#define TFA98XX_STATUS_FLAGS3_CLIPSR_MAX                         1
#define TFA98XX_STATUS_FLAGS3_CLIPSR_MSK                     0x200

/*
 * flag_mic_ocpok
 */
#define TFA98XX_STATUS_FLAGS3_OCPOKMC                    (0x1<<10)
#define TFA98XX_STATUS_FLAGS3_OCPOKMC_POS                       10
#define TFA98XX_STATUS_FLAGS3_OCPOKMC_LEN                        1
#define TFA98XX_STATUS_FLAGS3_OCPOKMC_MAX                        1
#define TFA98XX_STATUS_FLAGS3_OCPOKMC_MSK                    0x400

/*
 * flag_man_alarm_state
 */
#define TFA98XX_STATUS_FLAGS3_MANALARM                   (0x1<<11)
#define TFA98XX_STATUS_FLAGS3_MANALARM_POS                      11
#define TFA98XX_STATUS_FLAGS3_MANALARM_LEN                       1
#define TFA98XX_STATUS_FLAGS3_MANALARM_MAX                       1
#define TFA98XX_STATUS_FLAGS3_MANALARM_MSK                   0x800

/*
 * flag_man_wait_src_settings
 */
#define TFA98XX_STATUS_FLAGS3_MANWAIT1                   (0x1<<12)
#define TFA98XX_STATUS_FLAGS3_MANWAIT1_POS                      12
#define TFA98XX_STATUS_FLAGS3_MANWAIT1_LEN                       1
#define TFA98XX_STATUS_FLAGS3_MANWAIT1_MAX                       1
#define TFA98XX_STATUS_FLAGS3_MANWAIT1_MSK                  0x1000

/*
 * flag_man_wait_cf_config
 */
#define TFA98XX_STATUS_FLAGS3_MANWAIT2                   (0x1<<13)
#define TFA98XX_STATUS_FLAGS3_MANWAIT2_POS                      13
#define TFA98XX_STATUS_FLAGS3_MANWAIT2_LEN                       1
#define TFA98XX_STATUS_FLAGS3_MANWAIT2_MAX                       1
#define TFA98XX_STATUS_FLAGS3_MANWAIT2_MSK                  0x2000

/*
 * flag_man_start_mute_audio
 */
#define TFA98XX_STATUS_FLAGS3_MANMUTE                    (0x1<<14)
#define TFA98XX_STATUS_FLAGS3_MANMUTE_POS                       14
#define TFA98XX_STATUS_FLAGS3_MANMUTE_LEN                        1
#define TFA98XX_STATUS_FLAGS3_MANMUTE_MAX                        1
#define TFA98XX_STATUS_FLAGS3_MANMUTE_MSK                   0x4000

/*
 * flag_man_operating_state
 */
#define TFA98XX_STATUS_FLAGS3_MANOPER                    (0x1<<15)
#define TFA98XX_STATUS_FLAGS3_MANOPER_POS                       15
#define TFA98XX_STATUS_FLAGS3_MANOPER_LEN                        1
#define TFA98XX_STATUS_FLAGS3_MANOPER_MAX                        1
#define TFA98XX_STATUS_FLAGS3_MANOPER_MSK                   0x8000


/*
 * (0x14)-status_flags4
 */

/*
 * flag_cf_speakererror_left
 */
#define TFA98XX_STATUS_FLAGS4_SPKSL                       (0x1<<0)
#define TFA98XX_STATUS_FLAGS4_SPKSL_POS                          0
#define TFA98XX_STATUS_FLAGS4_SPKSL_LEN                          1
#define TFA98XX_STATUS_FLAGS4_SPKSL_MAX                          1
#define TFA98XX_STATUS_FLAGS4_SPKSL_MSK                        0x1

/*
 * flag_cf_speakererror_right
 */
#define TFA98XX_STATUS_FLAGS4_SPKSR                       (0x1<<1)
#define TFA98XX_STATUS_FLAGS4_SPKSR_POS                          1
#define TFA98XX_STATUS_FLAGS4_SPKSR_LEN                          1
#define TFA98XX_STATUS_FLAGS4_SPKSR_MAX                          1
#define TFA98XX_STATUS_FLAGS4_SPKSR_MSK                        0x2

/*
 * flag_clk_out_of_range
 */
#define TFA98XX_STATUS_FLAGS4_CLKOOR                      (0x1<<2)
#define TFA98XX_STATUS_FLAGS4_CLKOOR_POS                         2
#define TFA98XX_STATUS_FLAGS4_CLKOOR_LEN                         1
#define TFA98XX_STATUS_FLAGS4_CLKOOR_MAX                         1
#define TFA98XX_STATUS_FLAGS4_CLKOOR_MSK                       0x4

/*
 * man_state
 */
#define TFA98XX_STATUS_FLAGS4_MANSTATE                    (0xf<<3)
#define TFA98XX_STATUS_FLAGS4_MANSTATE_POS                       3
#define TFA98XX_STATUS_FLAGS4_MANSTATE_LEN                       4
#define TFA98XX_STATUS_FLAGS4_MANSTATE_MAX                      15
#define TFA98XX_STATUS_FLAGS4_MANSTATE_MSK                    0x78


/*
 * (0x15)-battery_voltage
 */

/*
 * bat_adc
 */
#define TFA98XX_BATTERY_VOLTAGE_BATS                    (0x3ff<<0)
#define TFA98XX_BATTERY_VOLTAGE_BATS_POS                         0
#define TFA98XX_BATTERY_VOLTAGE_BATS_LEN                        10
#define TFA98XX_BATTERY_VOLTAGE_BATS_MAX                      1023
#define TFA98XX_BATTERY_VOLTAGE_BATS_MSK                     0x3ff


/*
 * (0x16)-temperature
 */

/*
 * temp_adc
 */
#define TFA98XX_TEMPERATURE_TEMPS                       (0x1ff<<0)
#define TFA98XX_TEMPERATURE_TEMPS_POS                            0
#define TFA98XX_TEMPERATURE_TEMPS_LEN                            9
#define TFA98XX_TEMPERATURE_TEMPS_MAX                          511
#define TFA98XX_TEMPERATURE_TEMPS_MSK                        0x1ff


/*
 * (0x20)-tdm_config0
 */

/*
 * tdm_usecase
 */
#define TFA98XX_TDM_CONFIG0_TDMUC                         (0xf<<0)
#define TFA98XX_TDM_CONFIG0_TDMUC_POS                            0
#define TFA98XX_TDM_CONFIG0_TDMUC_LEN                            4
#define TFA98XX_TDM_CONFIG0_TDMUC_MAX                           15
#define TFA98XX_TDM_CONFIG0_TDMUC_MSK                          0xf

/*
 * tdm_enable
 */
#define TFA98XX_TDM_CONFIG0_TDME                          (0x1<<4)
#define TFA98XX_TDM_CONFIG0_TDME_POS                             4
#define TFA98XX_TDM_CONFIG0_TDME_LEN                             1
#define TFA98XX_TDM_CONFIG0_TDME_MAX                             1
#define TFA98XX_TDM_CONFIG0_TDME_MSK                          0x10

/*
 * tdm_mode
 */
#define TFA98XX_TDM_CONFIG0_TDMMODE                       (0x1<<5)
#define TFA98XX_TDM_CONFIG0_TDMMODE_POS                          5
#define TFA98XX_TDM_CONFIG0_TDMMODE_LEN                          1
#define TFA98XX_TDM_CONFIG0_TDMMODE_MAX                          1
#define TFA98XX_TDM_CONFIG0_TDMMODE_MSK                       0x20

/*
 * tdm_clk_inversion
 */
#define TFA98XX_TDM_CONFIG0_TDMCLINV                      (0x1<<6)
#define TFA98XX_TDM_CONFIG0_TDMCLINV_POS                         6
#define TFA98XX_TDM_CONFIG0_TDMCLINV_LEN                         1
#define TFA98XX_TDM_CONFIG0_TDMCLINV_MAX                         1
#define TFA98XX_TDM_CONFIG0_TDMCLINV_MSK                      0x40

/*
 * tdm_fs_ws_length
 */
#define TFA98XX_TDM_CONFIG0_TDMFSLN                       (0xf<<7)
#define TFA98XX_TDM_CONFIG0_TDMFSLN_POS                          7
#define TFA98XX_TDM_CONFIG0_TDMFSLN_LEN                          4
#define TFA98XX_TDM_CONFIG0_TDMFSLN_MAX                         15
#define TFA98XX_TDM_CONFIG0_TDMFSLN_MSK                      0x780

/*
 * tdm_fs_ws_polarity
 */
#define TFA98XX_TDM_CONFIG0_TDMFSPOL                     (0x1<<11)
#define TFA98XX_TDM_CONFIG0_TDMFSPOL_POS                        11
#define TFA98XX_TDM_CONFIG0_TDMFSPOL_LEN                         1
#define TFA98XX_TDM_CONFIG0_TDMFSPOL_MAX                         1
#define TFA98XX_TDM_CONFIG0_TDMFSPOL_MSK                     0x800

/*
 * tdm_nbck
 */
#define TFA98XX_TDM_CONFIG0_TDMNBCK                      (0xf<<12)
#define TFA98XX_TDM_CONFIG0_TDMNBCK_POS                         12
#define TFA98XX_TDM_CONFIG0_TDMNBCK_LEN                          4
#define TFA98XX_TDM_CONFIG0_TDMNBCK_MAX                         15
#define TFA98XX_TDM_CONFIG0_TDMNBCK_MSK                     0xf000


/*
 * (0x21)-tdm_config1
 */

/*
 * tdm_nb_of_slots
 */
#define TFA98XX_TDM_CONFIG1_TDMSLOTS                      (0xf<<0)
#define TFA98XX_TDM_CONFIG1_TDMSLOTS_POS                         0
#define TFA98XX_TDM_CONFIG1_TDMSLOTS_LEN                         4
#define TFA98XX_TDM_CONFIG1_TDMSLOTS_MAX                        15
#define TFA98XX_TDM_CONFIG1_TDMSLOTS_MSK                       0xf

/*
 * tdm_slot_length
 */
#define TFA98XX_TDM_CONFIG1_TDMSLLN                      (0x1f<<4)
#define TFA98XX_TDM_CONFIG1_TDMSLLN_POS                          4
#define TFA98XX_TDM_CONFIG1_TDMSLLN_LEN                          5
#define TFA98XX_TDM_CONFIG1_TDMSLLN_MAX                         31
#define TFA98XX_TDM_CONFIG1_TDMSLLN_MSK                      0x1f0

/*
 * tdm_bits_remaining
 */
#define TFA98XX_TDM_CONFIG1_TDMBRMG                      (0x1f<<9)
#define TFA98XX_TDM_CONFIG1_TDMBRMG_POS                          9
#define TFA98XX_TDM_CONFIG1_TDMBRMG_LEN                          5
#define TFA98XX_TDM_CONFIG1_TDMBRMG_MAX                         31
#define TFA98XX_TDM_CONFIG1_TDMBRMG_MSK                     0x3e00

/*
 * tdm_data_delay
 */
#define TFA98XX_TDM_CONFIG1_TDMDEL                       (0x1<<14)
#define TFA98XX_TDM_CONFIG1_TDMDEL_POS                          14
#define TFA98XX_TDM_CONFIG1_TDMDEL_LEN                           1
#define TFA98XX_TDM_CONFIG1_TDMDEL_MAX                           1
#define TFA98XX_TDM_CONFIG1_TDMDEL_MSK                      0x4000

/*
 * tdm_data_adjustment
 */
#define TFA98XX_TDM_CONFIG1_TDMADJ                       (0x1<<15)
#define TFA98XX_TDM_CONFIG1_TDMADJ_POS                          15
#define TFA98XX_TDM_CONFIG1_TDMADJ_LEN                           1
#define TFA98XX_TDM_CONFIG1_TDMADJ_MAX                           1
#define TFA98XX_TDM_CONFIG1_TDMADJ_MSK                      0x8000


/*
 * (0x22)-tdm_config2
 */

/*
 * tdm_audio_sample_compression
 */
#define TFA98XX_TDM_CONFIG2_TDMOOMP                       (0x3<<0)
#define TFA98XX_TDM_CONFIG2_TDMOOMP_POS                          0
#define TFA98XX_TDM_CONFIG2_TDMOOMP_LEN                          2
#define TFA98XX_TDM_CONFIG2_TDMOOMP_MAX                          3
#define TFA98XX_TDM_CONFIG2_TDMOOMP_MSK                        0x3

/*
 * tdm_sample_size
 */
#define TFA98XX_TDM_CONFIG2_TDMSSIZE                     (0x1f<<2)
#define TFA98XX_TDM_CONFIG2_TDMSSIZE_POS                         2
#define TFA98XX_TDM_CONFIG2_TDMSSIZE_LEN                         5
#define TFA98XX_TDM_CONFIG2_TDMSSIZE_MAX                        31
#define TFA98XX_TDM_CONFIG2_TDMSSIZE_MSK                      0x7c

/*
 * tdm_txdata_format
 */
#define TFA98XX_TDM_CONFIG2_TDMTXDFO                      (0x3<<7)
#define TFA98XX_TDM_CONFIG2_TDMTXDFO_POS                         7
#define TFA98XX_TDM_CONFIG2_TDMTXDFO_LEN                         2
#define TFA98XX_TDM_CONFIG2_TDMTXDFO_MAX                         3
#define TFA98XX_TDM_CONFIG2_TDMTXDFO_MSK                     0x180

/*
 * tdm_txdata_format_unused_slot_sd0
 */
#define TFA98XX_TDM_CONFIG2_TDMTXUS0                      (0x3<<9)
#define TFA98XX_TDM_CONFIG2_TDMTXUS0_POS                         9
#define TFA98XX_TDM_CONFIG2_TDMTXUS0_LEN                         2
#define TFA98XX_TDM_CONFIG2_TDMTXUS0_MAX                         3
#define TFA98XX_TDM_CONFIG2_TDMTXUS0_MSK                     0x600

/*
 * tdm_txdata_format_unused_slot_sd1
 */
#define TFA98XX_TDM_CONFIG2_TDMTXUS1                     (0x3<<11)
#define TFA98XX_TDM_CONFIG2_TDMTXUS1_POS                        11
#define TFA98XX_TDM_CONFIG2_TDMTXUS1_LEN                         2
#define TFA98XX_TDM_CONFIG2_TDMTXUS1_MAX                         3
#define TFA98XX_TDM_CONFIG2_TDMTXUS1_MSK                    0x1800

/*
 * tdm_txdata_format_unused_slot_sd2
 */
#define TFA98XX_TDM_CONFIG2_TDMTXUS2                     (0x3<<13)
#define TFA98XX_TDM_CONFIG2_TDMTXUS2_POS                        13
#define TFA98XX_TDM_CONFIG2_TDMTXUS2_LEN                         2
#define TFA98XX_TDM_CONFIG2_TDMTXUS2_MAX                         3
#define TFA98XX_TDM_CONFIG2_TDMTXUS2_MSK                    0x6000


/*
 * (0x23)-tdm_config3
 */

/*
 * tdm_sink1_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMLE                         (0x1<<1)
#define TFA98XX_TDM_CONFIG3_TDMLE_POS                            1
#define TFA98XX_TDM_CONFIG3_TDMLE_LEN                            1
#define TFA98XX_TDM_CONFIG3_TDMLE_MAX                            1
#define TFA98XX_TDM_CONFIG3_TDMLE_MSK                          0x2

/*
 * tdm_sink2_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMRE                         (0x1<<2)
#define TFA98XX_TDM_CONFIG3_TDMRE_POS                            2
#define TFA98XX_TDM_CONFIG3_TDMRE_LEN                            1
#define TFA98XX_TDM_CONFIG3_TDMRE_MAX                            1
#define TFA98XX_TDM_CONFIG3_TDMRE_MSK                          0x4

/*
 * tdm_source1_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMVSRE                       (0x1<<4)
#define TFA98XX_TDM_CONFIG3_TDMVSRE_POS                          4
#define TFA98XX_TDM_CONFIG3_TDMVSRE_LEN                          1
#define TFA98XX_TDM_CONFIG3_TDMVSRE_MAX                          1
#define TFA98XX_TDM_CONFIG3_TDMVSRE_MSK                       0x10

/*
 * tdm_source2_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMCSRE                       (0x1<<5)
#define TFA98XX_TDM_CONFIG3_TDMCSRE_POS                          5
#define TFA98XX_TDM_CONFIG3_TDMCSRE_LEN                          1
#define TFA98XX_TDM_CONFIG3_TDMCSRE_MAX                          1
#define TFA98XX_TDM_CONFIG3_TDMCSRE_MSK                       0x20

/*
 * tdm_source3_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMVSLE                       (0x1<<6)
#define TFA98XX_TDM_CONFIG3_TDMVSLE_POS                          6
#define TFA98XX_TDM_CONFIG3_TDMVSLE_LEN                          1
#define TFA98XX_TDM_CONFIG3_TDMVSLE_MAX                          1
#define TFA98XX_TDM_CONFIG3_TDMVSLE_MSK                       0x40

/*
 * tdm_source4_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMCSLE                       (0x1<<7)
#define TFA98XX_TDM_CONFIG3_TDMCSLE_POS                          7
#define TFA98XX_TDM_CONFIG3_TDMCSLE_LEN                          1
#define TFA98XX_TDM_CONFIG3_TDMCSLE_MAX                          1
#define TFA98XX_TDM_CONFIG3_TDMCSLE_MSK                       0x80

/*
 * tdm_source5_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMCFRE                       (0x1<<8)
#define TFA98XX_TDM_CONFIG3_TDMCFRE_POS                          8
#define TFA98XX_TDM_CONFIG3_TDMCFRE_LEN                          1
#define TFA98XX_TDM_CONFIG3_TDMCFRE_MAX                          1
#define TFA98XX_TDM_CONFIG3_TDMCFRE_MSK                      0x100

/*
 * tdm_source6_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMCFLE                       (0x1<<9)
#define TFA98XX_TDM_CONFIG3_TDMCFLE_POS                          9
#define TFA98XX_TDM_CONFIG3_TDMCFLE_LEN                          1
#define TFA98XX_TDM_CONFIG3_TDMCFLE_MAX                          1
#define TFA98XX_TDM_CONFIG3_TDMCFLE_MSK                      0x200

/*
 * tdm_source7_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMCF3E                      (0x1<<10)
#define TFA98XX_TDM_CONFIG3_TDMCF3E_POS                         10
#define TFA98XX_TDM_CONFIG3_TDMCF3E_LEN                          1
#define TFA98XX_TDM_CONFIG3_TDMCF3E_MAX                          1
#define TFA98XX_TDM_CONFIG3_TDMCF3E_MSK                      0x400

/*
 * tdm_source8_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMCF4E                      (0x1<<11)
#define TFA98XX_TDM_CONFIG3_TDMCF4E_POS                         11
#define TFA98XX_TDM_CONFIG3_TDMCF4E_LEN                          1
#define TFA98XX_TDM_CONFIG3_TDMCF4E_MAX                          1
#define TFA98XX_TDM_CONFIG3_TDMCF4E_MSK                      0x800

/*
 * tdm_source9_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMPD1E                      (0x1<<12)
#define TFA98XX_TDM_CONFIG3_TDMPD1E_POS                         12
#define TFA98XX_TDM_CONFIG3_TDMPD1E_LEN                          1
#define TFA98XX_TDM_CONFIG3_TDMPD1E_MAX                          1
#define TFA98XX_TDM_CONFIG3_TDMPD1E_MSK                     0x1000

/*
 * tdm_source10_enable
 */
#define TFA98XX_TDM_CONFIG3_TDMPD2E                      (0x1<<13)
#define TFA98XX_TDM_CONFIG3_TDMPD2E_POS                         13
#define TFA98XX_TDM_CONFIG3_TDMPD2E_LEN                          1
#define TFA98XX_TDM_CONFIG3_TDMPD2E_MAX                          1
#define TFA98XX_TDM_CONFIG3_TDMPD2E_MSK                     0x2000


/*
 * (0x24)-tdm_config4
 */

/*
 * tdm_sink1_io
 */
#define TFA98XX_TDM_CONFIG4_TDMLIO                        (0x3<<2)
#define TFA98XX_TDM_CONFIG4_TDMLIO_POS                           2
#define TFA98XX_TDM_CONFIG4_TDMLIO_LEN                           2
#define TFA98XX_TDM_CONFIG4_TDMLIO_MAX                           3
#define TFA98XX_TDM_CONFIG4_TDMLIO_MSK                         0xc

/*
 * tdm_sink2_io
 */
#define TFA98XX_TDM_CONFIG4_TDMRIO                        (0x3<<4)
#define TFA98XX_TDM_CONFIG4_TDMRIO_POS                           4
#define TFA98XX_TDM_CONFIG4_TDMRIO_LEN                           2
#define TFA98XX_TDM_CONFIG4_TDMRIO_MAX                           3
#define TFA98XX_TDM_CONFIG4_TDMRIO_MSK                        0x30

/*
 * tdm_source1_io
 */
#define TFA98XX_TDM_CONFIG4_TDMVSRIO                      (0x3<<8)
#define TFA98XX_TDM_CONFIG4_TDMVSRIO_POS                         8
#define TFA98XX_TDM_CONFIG4_TDMVSRIO_LEN                         2
#define TFA98XX_TDM_CONFIG4_TDMVSRIO_MAX                         3
#define TFA98XX_TDM_CONFIG4_TDMVSRIO_MSK                     0x300

/*
 * tdm_source2_io
 */
#define TFA98XX_TDM_CONFIG4_TDMCSRIO                     (0x3<<10)
#define TFA98XX_TDM_CONFIG4_TDMCSRIO_POS                        10
#define TFA98XX_TDM_CONFIG4_TDMCSRIO_LEN                         2
#define TFA98XX_TDM_CONFIG4_TDMCSRIO_MAX                         3
#define TFA98XX_TDM_CONFIG4_TDMCSRIO_MSK                     0xc00

/*
 * tdm_source3_io
 */
#define TFA98XX_TDM_CONFIG4_TDMVSLIO                     (0x3<<12)
#define TFA98XX_TDM_CONFIG4_TDMVSLIO_POS                        12
#define TFA98XX_TDM_CONFIG4_TDMVSLIO_LEN                         2
#define TFA98XX_TDM_CONFIG4_TDMVSLIO_MAX                         3
#define TFA98XX_TDM_CONFIG4_TDMVSLIO_MSK                    0x3000

/*
 * tdm_source4_io
 */
#define TFA98XX_TDM_CONFIG4_TDMCSLIO                     (0x3<<14)
#define TFA98XX_TDM_CONFIG4_TDMCSLIO_POS                        14
#define TFA98XX_TDM_CONFIG4_TDMCSLIO_LEN                         2
#define TFA98XX_TDM_CONFIG4_TDMCSLIO_MAX                         3
#define TFA98XX_TDM_CONFIG4_TDMCSLIO_MSK                    0xc000


/*
 * (0x25)-tdm_config5
 */

/*
 * tdm_source5_io
 */
#define TFA98XX_TDM_CONFIG5_TDMCFRIO                      (0x3<<0)
#define TFA98XX_TDM_CONFIG5_TDMCFRIO_POS                         0
#define TFA98XX_TDM_CONFIG5_TDMCFRIO_LEN                         2
#define TFA98XX_TDM_CONFIG5_TDMCFRIO_MAX                         3
#define TFA98XX_TDM_CONFIG5_TDMCFRIO_MSK                       0x3

/*
 * tdm_source6_io
 */
#define TFA98XX_TDM_CONFIG5_TDMCFLIO                      (0x3<<2)
#define TFA98XX_TDM_CONFIG5_TDMCFLIO_POS                         2
#define TFA98XX_TDM_CONFIG5_TDMCFLIO_LEN                         2
#define TFA98XX_TDM_CONFIG5_TDMCFLIO_MAX                         3
#define TFA98XX_TDM_CONFIG5_TDMCFLIO_MSK                       0xc

/*
 * tdm_source7_io
 */
#define TFA98XX_TDM_CONFIG5_TDMCF3IO                      (0x3<<4)
#define TFA98XX_TDM_CONFIG5_TDMCF3IO_POS                         4
#define TFA98XX_TDM_CONFIG5_TDMCF3IO_LEN                         2
#define TFA98XX_TDM_CONFIG5_TDMCF3IO_MAX                         3
#define TFA98XX_TDM_CONFIG5_TDMCF3IO_MSK                      0x30

/*
 * tdm_source8_io
 */
#define TFA98XX_TDM_CONFIG5_TDMCF4IO                      (0x3<<6)
#define TFA98XX_TDM_CONFIG5_TDMCF4IO_POS                         6
#define TFA98XX_TDM_CONFIG5_TDMCF4IO_LEN                         2
#define TFA98XX_TDM_CONFIG5_TDMCF4IO_MAX                         3
#define TFA98XX_TDM_CONFIG5_TDMCF4IO_MSK                      0xc0

/*
 * tdm_source9_io
 */
#define TFA98XX_TDM_CONFIG5_TDMPD1IO                      (0x3<<8)
#define TFA98XX_TDM_CONFIG5_TDMPD1IO_POS                         8
#define TFA98XX_TDM_CONFIG5_TDMPD1IO_LEN                         2
#define TFA98XX_TDM_CONFIG5_TDMPD1IO_MAX                         3
#define TFA98XX_TDM_CONFIG5_TDMPD1IO_MSK                     0x300

/*
 * tdm_source10_io
 */
#define TFA98XX_TDM_CONFIG5_TDMPD2IO                     (0x3<<10)
#define TFA98XX_TDM_CONFIG5_TDMPD2IO_POS                        10
#define TFA98XX_TDM_CONFIG5_TDMPD2IO_LEN                         2
#define TFA98XX_TDM_CONFIG5_TDMPD2IO_MAX                         3
#define TFA98XX_TDM_CONFIG5_TDMPD2IO_MSK                     0xc00


/*
 * (0x26)-tdm_config6
 */

/*
 * tdm_sink1_slot
 */
#define TFA98XX_TDM_CONFIG6_TDMLS                         (0xf<<4)
#define TFA98XX_TDM_CONFIG6_TDMLS_POS                            4
#define TFA98XX_TDM_CONFIG6_TDMLS_LEN                            4
#define TFA98XX_TDM_CONFIG6_TDMLS_MAX                           15
#define TFA98XX_TDM_CONFIG6_TDMLS_MSK                         0xf0

/*
 * tdm_sink2_slot
 */
#define TFA98XX_TDM_CONFIG6_TDMRS                         (0xf<<8)
#define TFA98XX_TDM_CONFIG6_TDMRS_POS                            8
#define TFA98XX_TDM_CONFIG6_TDMRS_LEN                            4
#define TFA98XX_TDM_CONFIG6_TDMRS_MAX                           15
#define TFA98XX_TDM_CONFIG6_TDMRS_MSK                        0xf00


/*
 * (0x27)-tdm_config7
 */

/*
 * tdm_source1_slot
 */
#define TFA98XX_TDM_CONFIG7_TDMVSRS                       (0xf<<0)
#define TFA98XX_TDM_CONFIG7_TDMVSRS_POS                          0
#define TFA98XX_TDM_CONFIG7_TDMVSRS_LEN                          4
#define TFA98XX_TDM_CONFIG7_TDMVSRS_MAX                         15
#define TFA98XX_TDM_CONFIG7_TDMVSRS_MSK                        0xf

/*
 * tdm_source2_slot
 */
#define TFA98XX_TDM_CONFIG7_TDMCSRS                       (0xf<<4)
#define TFA98XX_TDM_CONFIG7_TDMCSRS_POS                          4
#define TFA98XX_TDM_CONFIG7_TDMCSRS_LEN                          4
#define TFA98XX_TDM_CONFIG7_TDMCSRS_MAX                         15
#define TFA98XX_TDM_CONFIG7_TDMCSRS_MSK                       0xf0

/*
 * tdm_source3_slot
 */
#define TFA98XX_TDM_CONFIG7_TDMVSLS                       (0xf<<8)
#define TFA98XX_TDM_CONFIG7_TDMVSLS_POS                          8
#define TFA98XX_TDM_CONFIG7_TDMVSLS_LEN                          4
#define TFA98XX_TDM_CONFIG7_TDMVSLS_MAX                         15
#define TFA98XX_TDM_CONFIG7_TDMVSLS_MSK                      0xf00

/*
 * tdm_source4_slot
 */
#define TFA98XX_TDM_CONFIG7_TDMCSLS                      (0xf<<12)
#define TFA98XX_TDM_CONFIG7_TDMCSLS_POS                         12
#define TFA98XX_TDM_CONFIG7_TDMCSLS_LEN                          4
#define TFA98XX_TDM_CONFIG7_TDMCSLS_MAX                         15
#define TFA98XX_TDM_CONFIG7_TDMCSLS_MSK                     0xf000


/*
 * (0x28)-tdm_config8
 */

/*
 * tdm_source5_slot
 */
#define TFA98XX_TDM_CONFIG8_TDMCFRS                       (0xf<<0)
#define TFA98XX_TDM_CONFIG8_TDMCFRS_POS                          0
#define TFA98XX_TDM_CONFIG8_TDMCFRS_LEN                          4
#define TFA98XX_TDM_CONFIG8_TDMCFRS_MAX                         15
#define TFA98XX_TDM_CONFIG8_TDMCFRS_MSK                        0xf

/*
 * tdm_source6_slot
 */
#define TFA98XX_TDM_CONFIG8_TDMCFLS                       (0xf<<4)
#define TFA98XX_TDM_CONFIG8_TDMCFLS_POS                          4
#define TFA98XX_TDM_CONFIG8_TDMCFLS_LEN                          4
#define TFA98XX_TDM_CONFIG8_TDMCFLS_MAX                         15
#define TFA98XX_TDM_CONFIG8_TDMCFLS_MSK                       0xf0

/*
 * tdm_source7_slot
 */
#define TFA98XX_TDM_CONFIG8_TDMCF3S                       (0xf<<8)
#define TFA98XX_TDM_CONFIG8_TDMCF3S_POS                          8
#define TFA98XX_TDM_CONFIG8_TDMCF3S_LEN                          4
#define TFA98XX_TDM_CONFIG8_TDMCF3S_MAX                         15
#define TFA98XX_TDM_CONFIG8_TDMCF3S_MSK                      0xf00

/*
 * tdm_source8_slot
 */
#define TFA98XX_TDM_CONFIG8_TDMCF4S                      (0xf<<12)
#define TFA98XX_TDM_CONFIG8_TDMCF4S_POS                         12
#define TFA98XX_TDM_CONFIG8_TDMCF4S_LEN                          4
#define TFA98XX_TDM_CONFIG8_TDMCF4S_MAX                         15
#define TFA98XX_TDM_CONFIG8_TDMCF4S_MSK                     0xf000


/*
 * (0x29)-tdm_config9
 */

/*
 * tdm_source9_slot
 */
#define TFA98XX_TDM_CONFIG9_TDMPD1S                       (0xf<<0)
#define TFA98XX_TDM_CONFIG9_TDMPD1S_POS                          0
#define TFA98XX_TDM_CONFIG9_TDMPD1S_LEN                          4
#define TFA98XX_TDM_CONFIG9_TDMPD1S_MAX                         15
#define TFA98XX_TDM_CONFIG9_TDMPD1S_MSK                        0xf

/*
 * tdm_source10_slot
 */
#define TFA98XX_TDM_CONFIG9_TDMPD2S                       (0xf<<4)
#define TFA98XX_TDM_CONFIG9_TDMPD2S_POS                          4
#define TFA98XX_TDM_CONFIG9_TDMPD2S_LEN                          4
#define TFA98XX_TDM_CONFIG9_TDMPD2S_MAX                         15
#define TFA98XX_TDM_CONFIG9_TDMPD2S_MSK                       0xf0


/*
 * (0x31)-pdm_config0
 */

/*
 * pdm_mode
 */
#define TFA98XX_PDM_CONFIG0_PDMSM                         (0x1<<0)
#define TFA98XX_PDM_CONFIG0_PDMSM_POS                            0
#define TFA98XX_PDM_CONFIG0_PDMSM_LEN                            1
#define TFA98XX_PDM_CONFIG0_PDMSM_MAX                            1
#define TFA98XX_PDM_CONFIG0_PDMSM_MSK                          0x1

/*
 * pdm_side_tone_sel
 */
#define TFA98XX_PDM_CONFIG0_PDMSTSEL                      (0x3<<1)
#define TFA98XX_PDM_CONFIG0_PDMSTSEL_POS                         1
#define TFA98XX_PDM_CONFIG0_PDMSTSEL_LEN                         2
#define TFA98XX_PDM_CONFIG0_PDMSTSEL_MAX                         3
#define TFA98XX_PDM_CONFIG0_PDMSTSEL_MSK                       0x6

/*
 * pdm_left_sel
 */
#define TFA98XX_PDM_CONFIG0_PDMLSEL                       (0x1<<3)
#define TFA98XX_PDM_CONFIG0_PDMLSEL_POS                          3
#define TFA98XX_PDM_CONFIG0_PDMLSEL_LEN                          1
#define TFA98XX_PDM_CONFIG0_PDMLSEL_MAX                          1
#define TFA98XX_PDM_CONFIG0_PDMLSEL_MSK                        0x8

/*
 * pdm_right_sel
 */
#define TFA98XX_PDM_CONFIG0_PDMRSEL                       (0x1<<4)
#define TFA98XX_PDM_CONFIG0_PDMRSEL_POS                          4
#define TFA98XX_PDM_CONFIG0_PDMRSEL_LEN                          1
#define TFA98XX_PDM_CONFIG0_PDMRSEL_MAX                          1
#define TFA98XX_PDM_CONFIG0_PDMRSEL_MSK                       0x10

/*
 * enbl_micvdd
 */
#define TFA98XX_PDM_CONFIG0_MICVDDE                       (0x1<<5)
#define TFA98XX_PDM_CONFIG0_MICVDDE_POS                          5
#define TFA98XX_PDM_CONFIG0_MICVDDE_LEN                          1
#define TFA98XX_PDM_CONFIG0_MICVDDE_MAX                          1
#define TFA98XX_PDM_CONFIG0_MICVDDE_MSK                       0x20


/*
 * (0x32)-pdm_config1
 */

/*
 * pdm_nbck
 */
#define TFA98XX_PDM_CONFIG1_PDMCLRAT                      (0x3<<0)
#define TFA98XX_PDM_CONFIG1_PDMCLRAT_POS                         0
#define TFA98XX_PDM_CONFIG1_PDMCLRAT_LEN                         2
#define TFA98XX_PDM_CONFIG1_PDMCLRAT_MAX                         3
#define TFA98XX_PDM_CONFIG1_PDMCLRAT_MSK                       0x3

/*
 * pdm_gain
 */
#define TFA98XX_PDM_CONFIG1_PDMGAIN                       (0xf<<2)
#define TFA98XX_PDM_CONFIG1_PDMGAIN_POS                          2
#define TFA98XX_PDM_CONFIG1_PDMGAIN_LEN                          4
#define TFA98XX_PDM_CONFIG1_PDMGAIN_MAX                         15
#define TFA98XX_PDM_CONFIG1_PDMGAIN_MSK                       0x3c

/*
 * sel_pdm_out_data
 */
#define TFA98XX_PDM_CONFIG1_PDMOSEL                       (0xf<<6)
#define TFA98XX_PDM_CONFIG1_PDMOSEL_POS                          6
#define TFA98XX_PDM_CONFIG1_PDMOSEL_LEN                          4
#define TFA98XX_PDM_CONFIG1_PDMOSEL_MAX                         15
#define TFA98XX_PDM_CONFIG1_PDMOSEL_MSK                      0x3c0

/*
 * sel_cf_haptic_data
 */
#define TFA98XX_PDM_CONFIG1_SELCFHAPD                    (0x1<<10)
#define TFA98XX_PDM_CONFIG1_SELCFHAPD_POS                       10
#define TFA98XX_PDM_CONFIG1_SELCFHAPD_LEN                        1
#define TFA98XX_PDM_CONFIG1_SELCFHAPD_MAX                        1
#define TFA98XX_PDM_CONFIG1_SELCFHAPD_MSK                    0x400


/*
 * (0x33)-haptic_driver_config
 */

/*
 * haptic_duration
 */
#define TFA98XX_HAPTIC_DRIVER_CONFIG_HAPTIME             (0xff<<0)
#define TFA98XX_HAPTIC_DRIVER_CONFIG_HAPTIME_POS                 0
#define TFA98XX_HAPTIC_DRIVER_CONFIG_HAPTIME_LEN                 8
#define TFA98XX_HAPTIC_DRIVER_CONFIG_HAPTIME_MAX               255
#define TFA98XX_HAPTIC_DRIVER_CONFIG_HAPTIME_MSK              0xff

/*
 * haptic_data
 */
#define TFA98XX_HAPTIC_DRIVER_CONFIG_HAPLEVEL            (0xff<<8)
#define TFA98XX_HAPTIC_DRIVER_CONFIG_HAPLEVEL_POS                8
#define TFA98XX_HAPTIC_DRIVER_CONFIG_HAPLEVEL_LEN                8
#define TFA98XX_HAPTIC_DRIVER_CONFIG_HAPLEVEL_MAX              255
#define TFA98XX_HAPTIC_DRIVER_CONFIG_HAPLEVEL_MSK           0xff00


/*
 * (0x34)-gpio_datain_reg
 */

/*
 * gpio_datain
 */
#define TFA98XX_GPIO_DATAIN_REG_GPIODIN                   (0xf<<0)
#define TFA98XX_GPIO_DATAIN_REG_GPIODIN_POS                      0
#define TFA98XX_GPIO_DATAIN_REG_GPIODIN_LEN                      4
#define TFA98XX_GPIO_DATAIN_REG_GPIODIN_MAX                     15
#define TFA98XX_GPIO_DATAIN_REG_GPIODIN_MSK                    0xf


/*
 * (0x35)-gpio_config
 */

/*
 * gpio_ctrl
 */
#define TFA98XX_GPIO_CONFIG_GPIOCTRL                      (0x1<<0)
#define TFA98XX_GPIO_CONFIG_GPIOCTRL_POS                         0
#define TFA98XX_GPIO_CONFIG_GPIOCTRL_LEN                         1
#define TFA98XX_GPIO_CONFIG_GPIOCTRL_MAX                         1
#define TFA98XX_GPIO_CONFIG_GPIOCTRL_MSK                       0x1

/*
 * gpio_dir
 */
#define TFA98XX_GPIO_CONFIG_GPIOCONF                      (0xf<<1)
#define TFA98XX_GPIO_CONFIG_GPIOCONF_POS                         1
#define TFA98XX_GPIO_CONFIG_GPIOCONF_LEN                         4
#define TFA98XX_GPIO_CONFIG_GPIOCONF_MAX                        15
#define TFA98XX_GPIO_CONFIG_GPIOCONF_MSK                      0x1e

/*
 * gpio_dataout
 */
#define TFA98XX_GPIO_CONFIG_GPIODOUT                      (0xf<<5)
#define TFA98XX_GPIO_CONFIG_GPIODOUT_POS                         5
#define TFA98XX_GPIO_CONFIG_GPIODOUT_LEN                         4
#define TFA98XX_GPIO_CONFIG_GPIODOUT_MAX                        15
#define TFA98XX_GPIO_CONFIG_GPIODOUT_MSK                     0x1e0


/*
 * (0x40)-interrupt_out_reg1
 */

/*
 * int_out_flag_por
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTVDDS                (0x1<<0)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTVDDS_POS                   0
#define TFA98XX_INTERRUPT_OUT_REG1_ISTVDDS_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTVDDS_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTVDDS_MSK                 0x1

/*
 * int_out_flag_pll_lock
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTPLLS                (0x1<<1)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTPLLS_POS                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTPLLS_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTPLLS_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTPLLS_MSK                 0x2

/*
 * int_out_flag_otpok
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTOTDS                (0x1<<2)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTOTDS_POS                   2
#define TFA98XX_INTERRUPT_OUT_REG1_ISTOTDS_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTOTDS_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTOTDS_MSK                 0x4

/*
 * int_out_flag_ovpok
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTOVDS                (0x1<<3)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTOVDS_POS                   3
#define TFA98XX_INTERRUPT_OUT_REG1_ISTOVDS_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTOVDS_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTOVDS_MSK                 0x8

/*
 * int_out_flag_uvpok
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTUVDS                (0x1<<4)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTUVDS_POS                   4
#define TFA98XX_INTERRUPT_OUT_REG1_ISTUVDS_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTUVDS_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTUVDS_MSK                0x10

/*
 * int_out_flag_clocks_stable
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTCLKS                (0x1<<5)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTCLKS_POS                   5
#define TFA98XX_INTERRUPT_OUT_REG1_ISTCLKS_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTCLKS_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTCLKS_MSK                0x20

/*
 * int_out_flag_mtp_busy
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTMTPB                (0x1<<6)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTMTPB_POS                   6
#define TFA98XX_INTERRUPT_OUT_REG1_ISTMTPB_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTMTPB_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTMTPB_MSK                0x40

/*
 * int_out_flag_lost_clk
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTNOCLK               (0x1<<7)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTNOCLK_POS                  7
#define TFA98XX_INTERRUPT_OUT_REG1_ISTNOCLK_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTNOCLK_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTNOCLK_MSK               0x80

/*
 * int_out_flag_cf_speakererror
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTSPKS                (0x1<<8)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTSPKS_POS                   8
#define TFA98XX_INTERRUPT_OUT_REG1_ISTSPKS_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTSPKS_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTSPKS_MSK               0x100

/*
 * int_out_flag_cold_started
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTACS                 (0x1<<9)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTACS_POS                    9
#define TFA98XX_INTERRUPT_OUT_REG1_ISTACS_LEN                    1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTACS_MAX                    1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTACS_MSK                0x200

/*
 * int_out_flag_engage
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTSWS                (0x1<<10)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTSWS_POS                   10
#define TFA98XX_INTERRUPT_OUT_REG1_ISTSWS_LEN                    1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTSWS_MAX                    1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTSWS_MSK                0x400

/*
 * int_out_flag_watchdog_reset
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTWDS                (0x1<<11)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTWDS_POS                   11
#define TFA98XX_INTERRUPT_OUT_REG1_ISTWDS_LEN                    1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTWDS_MAX                    1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTWDS_MSK                0x800

/*
 * int_out_flag_enbl_amp
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTAMPS               (0x1<<12)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTAMPS_POS                  12
#define TFA98XX_INTERRUPT_OUT_REG1_ISTAMPS_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTAMPS_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTAMPS_MSK              0x1000

/*
 * int_out_flag_enbl_ref
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTAREFS              (0x1<<13)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTAREFS_POS                 13
#define TFA98XX_INTERRUPT_OUT_REG1_ISTAREFS_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTAREFS_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTAREFS_MSK             0x2000

/*
 * int_out_flag_adc10_ready
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTADCCR              (0x1<<14)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTADCCR_POS                 14
#define TFA98XX_INTERRUPT_OUT_REG1_ISTADCCR_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTADCCR_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTADCCR_MSK             0x4000

/*
 * int_out_flag_bod_vddd_nok
 */
#define TFA98XX_INTERRUPT_OUT_REG1_ISTBODNOK             (0x1<<15)
#define TFA98XX_INTERRUPT_OUT_REG1_ISTBODNOK_POS                15
#define TFA98XX_INTERRUPT_OUT_REG1_ISTBODNOK_LEN                 1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTBODNOK_MAX                 1
#define TFA98XX_INTERRUPT_OUT_REG1_ISTBODNOK_MSK            0x8000


/*
 * (0x41)-interrupt_out_reg2
 */

/*
 * int_out_flag_bst_bstcur
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTCU               (0x1<<0)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTCU_POS                  0
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTCU_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTCU_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTCU_MSK                0x1

/*
 * int_out_flag_bst_hiz
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTHI               (0x1<<1)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTHI_POS                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTHI_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTHI_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTHI_MSK                0x2

/*
 * int_out_flag_bst_ocpok
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTOC               (0x1<<2)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTOC_POS                  2
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTOC_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTOC_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTOC_MSK                0x4

/*
 * int_out_flag_bst_peakcur
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTPKCUR            (0x1<<3)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTPKCUR_POS               3
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTPKCUR_LEN               1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTPKCUR_MAX               1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTPKCUR_MSK             0x8

/*
 * int_out_flag_bst_voutcomp
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTVC               (0x1<<4)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTVC_POS                  4
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTVC_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTVC_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBSTVC_MSK               0x10

/*
 * int_out_flag_bst_voutcomp86
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBST86               (0x1<<5)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBST86_POS                  5
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBST86_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBST86_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBST86_MSK               0x20

/*
 * int_out_flag_bst_voutcomp93
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBST93               (0x1<<6)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBST93_POS                  6
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBST93_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBST93_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTBST93_MSK               0x40

/*
 * int_out_flag_rcvldop_ready
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTRCVLD               (0x1<<7)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTRCVLD_POS                  7
#define TFA98XX_INTERRUPT_OUT_REG2_ISTRCVLD_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTRCVLD_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTRCVLD_MSK               0x80

/*
 * int_out_flag_ocp_alarm_left
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTOCPL                (0x1<<8)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTOCPL_POS                   8
#define TFA98XX_INTERRUPT_OUT_REG2_ISTOCPL_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTOCPL_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTOCPL_MSK               0x100

/*
 * int_out_flag_ocp_alarm_right
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTOCPR                (0x1<<9)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTOCPR_POS                   9
#define TFA98XX_INTERRUPT_OUT_REG2_ISTOCPR_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTOCPR_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTOCPR_MSK               0x200

/*
 * int_out_flag_man_wait_src_settings
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWSRC              (0x1<<10)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWSRC_POS                 10
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWSRC_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWSRC_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWSRC_MSK              0x400

/*
 * int_out_flag_man_wait_cf_config
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWCFC              (0x1<<11)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWCFC_POS                 11
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWCFC_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWCFC_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWCFC_MSK              0x800

/*
 * int_out_flag_man_start_mute_audio
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWSMU              (0x1<<12)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWSMU_POS                 12
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWSMU_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWSMU_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTMWSMU_MSK             0x1000

/*
 * int_out_flag_cfma_err
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCFMER              (0x1<<13)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCFMER_POS                 13
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCFMER_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCFMER_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCFMER_MSK             0x2000

/*
 * int_out_flag_cfma_ack
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCFMAC              (0x1<<14)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCFMAC_POS                 14
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCFMAC_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCFMAC_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCFMAC_MSK             0x4000

/*
 * int_out_flag_clk_out_of_range
 */
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCLKOOR             (0x1<<15)
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCLKOOR_POS                15
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCLKOOR_LEN                 1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCLKOOR_MAX                 1
#define TFA98XX_INTERRUPT_OUT_REG2_ISTCLKOOR_MSK            0x8000


/*
 * (0x42)-interrupt_out_reg3
 */

/*
 * int_out_flag_tdm_error
 */
#define TFA98XX_INTERRUPT_OUT_REG3_ISTTDMER               (0x1<<0)
#define TFA98XX_INTERRUPT_OUT_REG3_ISTTDMER_POS                  0
#define TFA98XX_INTERRUPT_OUT_REG3_ISTTDMER_LEN                  1
#define TFA98XX_INTERRUPT_OUT_REG3_ISTTDMER_MAX                  1
#define TFA98XX_INTERRUPT_OUT_REG3_ISTTDMER_MSK                0x1

/*
 * int_out_flag_clip_left
 */
#define TFA98XX_INTERRUPT_OUT_REG3_ISTCLPL                (0x1<<1)
#define TFA98XX_INTERRUPT_OUT_REG3_ISTCLPL_POS                   1
#define TFA98XX_INTERRUPT_OUT_REG3_ISTCLPL_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG3_ISTCLPL_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG3_ISTCLPL_MSK                 0x2

/*
 * int_out_flag_clip_right
 */
#define TFA98XX_INTERRUPT_OUT_REG3_ISTCLPR                (0x1<<2)
#define TFA98XX_INTERRUPT_OUT_REG3_ISTCLPR_POS                   2
#define TFA98XX_INTERRUPT_OUT_REG3_ISTCLPR_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG3_ISTCLPR_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG3_ISTCLPR_MSK                 0x4

/*
 * int_out_flag_mic_ocpok
 */
#define TFA98XX_INTERRUPT_OUT_REG3_ISTOCPM                (0x1<<3)
#define TFA98XX_INTERRUPT_OUT_REG3_ISTOCPM_POS                   3
#define TFA98XX_INTERRUPT_OUT_REG3_ISTOCPM_LEN                   1
#define TFA98XX_INTERRUPT_OUT_REG3_ISTOCPM_MAX                   1
#define TFA98XX_INTERRUPT_OUT_REG3_ISTOCPM_MSK                 0x8


/*
 * (0x44)-interrupt_in_reg1
 */

/*
 * int_in_flag_por
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLVDDS                 (0x1<<0)
#define TFA98XX_INTERRUPT_IN_REG1_ICLVDDS_POS                    0
#define TFA98XX_INTERRUPT_IN_REG1_ICLVDDS_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLVDDS_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLVDDS_MSK                  0x1

/*
 * int_in_flag_pll_lock
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLPLLS                 (0x1<<1)
#define TFA98XX_INTERRUPT_IN_REG1_ICLPLLS_POS                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLPLLS_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLPLLS_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLPLLS_MSK                  0x2

/*
 * int_in_flag_otpok
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLOTDS                 (0x1<<2)
#define TFA98XX_INTERRUPT_IN_REG1_ICLOTDS_POS                    2
#define TFA98XX_INTERRUPT_IN_REG1_ICLOTDS_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLOTDS_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLOTDS_MSK                  0x4

/*
 * int_in_flag_ovpok
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLOVDS                 (0x1<<3)
#define TFA98XX_INTERRUPT_IN_REG1_ICLOVDS_POS                    3
#define TFA98XX_INTERRUPT_IN_REG1_ICLOVDS_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLOVDS_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLOVDS_MSK                  0x8

/*
 * int_in_flag_uvpok
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLUVDS                 (0x1<<4)
#define TFA98XX_INTERRUPT_IN_REG1_ICLUVDS_POS                    4
#define TFA98XX_INTERRUPT_IN_REG1_ICLUVDS_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLUVDS_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLUVDS_MSK                 0x10

/*
 * int_in_flag_clocks_stable
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLCLKS                 (0x1<<5)
#define TFA98XX_INTERRUPT_IN_REG1_ICLCLKS_POS                    5
#define TFA98XX_INTERRUPT_IN_REG1_ICLCLKS_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLCLKS_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLCLKS_MSK                 0x20

/*
 * int_in_flag_mtp_busy
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLMTPB                 (0x1<<6)
#define TFA98XX_INTERRUPT_IN_REG1_ICLMTPB_POS                    6
#define TFA98XX_INTERRUPT_IN_REG1_ICLMTPB_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLMTPB_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLMTPB_MSK                 0x40

/*
 * int_in_flag_lost_clk
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLNOCLK                (0x1<<7)
#define TFA98XX_INTERRUPT_IN_REG1_ICLNOCLK_POS                   7
#define TFA98XX_INTERRUPT_IN_REG1_ICLNOCLK_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG1_ICLNOCLK_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG1_ICLNOCLK_MSK                0x80

/*
 * int_in_flag_cf_speakererror
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLSPKS                 (0x1<<8)
#define TFA98XX_INTERRUPT_IN_REG1_ICLSPKS_POS                    8
#define TFA98XX_INTERRUPT_IN_REG1_ICLSPKS_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLSPKS_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLSPKS_MSK                0x100

/*
 * int_in_flag_cold_started
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLACS                  (0x1<<9)
#define TFA98XX_INTERRUPT_IN_REG1_ICLACS_POS                     9
#define TFA98XX_INTERRUPT_IN_REG1_ICLACS_LEN                     1
#define TFA98XX_INTERRUPT_IN_REG1_ICLACS_MAX                     1
#define TFA98XX_INTERRUPT_IN_REG1_ICLACS_MSK                 0x200

/*
 * int_in_flag_engage
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLSWS                 (0x1<<10)
#define TFA98XX_INTERRUPT_IN_REG1_ICLSWS_POS                    10
#define TFA98XX_INTERRUPT_IN_REG1_ICLSWS_LEN                     1
#define TFA98XX_INTERRUPT_IN_REG1_ICLSWS_MAX                     1
#define TFA98XX_INTERRUPT_IN_REG1_ICLSWS_MSK                 0x400

/*
 * int_in_flag_watchdog_reset
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLWDS                 (0x1<<11)
#define TFA98XX_INTERRUPT_IN_REG1_ICLWDS_POS                    11
#define TFA98XX_INTERRUPT_IN_REG1_ICLWDS_LEN                     1
#define TFA98XX_INTERRUPT_IN_REG1_ICLWDS_MAX                     1
#define TFA98XX_INTERRUPT_IN_REG1_ICLWDS_MSK                 0x800

/*
 * int_in_flag_enbl_amp
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLAMPS                (0x1<<12)
#define TFA98XX_INTERRUPT_IN_REG1_ICLAMPS_POS                   12
#define TFA98XX_INTERRUPT_IN_REG1_ICLAMPS_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLAMPS_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG1_ICLAMPS_MSK               0x1000

/*
 * int_in_flag_enbl_ref
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLAREFS               (0x1<<13)
#define TFA98XX_INTERRUPT_IN_REG1_ICLAREFS_POS                  13
#define TFA98XX_INTERRUPT_IN_REG1_ICLAREFS_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG1_ICLAREFS_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG1_ICLAREFS_MSK              0x2000

/*
 * int_in_flag_adc10_ready
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLADCCR               (0x1<<14)
#define TFA98XX_INTERRUPT_IN_REG1_ICLADCCR_POS                  14
#define TFA98XX_INTERRUPT_IN_REG1_ICLADCCR_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG1_ICLADCCR_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG1_ICLADCCR_MSK              0x4000

/*
 * int_in_flag_bod_vddd_nok
 */
#define TFA98XX_INTERRUPT_IN_REG1_ICLBODNOK              (0x1<<15)
#define TFA98XX_INTERRUPT_IN_REG1_ICLBODNOK_POS                 15
#define TFA98XX_INTERRUPT_IN_REG1_ICLBODNOK_LEN                  1
#define TFA98XX_INTERRUPT_IN_REG1_ICLBODNOK_MAX                  1
#define TFA98XX_INTERRUPT_IN_REG1_ICLBODNOK_MSK             0x8000


/*
 * (0x45)-interrupt_in_reg2
 */

/*
 * int_in_flag_bst_bstcur
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTCU                (0x1<<0)
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTCU_POS                   0
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTCU_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTCU_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTCU_MSK                 0x1

/*
 * int_in_flag_bst_hiz
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTHI                (0x1<<1)
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTHI_POS                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTHI_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTHI_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTHI_MSK                 0x2

/*
 * int_in_flag_bst_ocpok
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTOC                (0x1<<2)
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTOC_POS                   2
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTOC_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTOC_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTOC_MSK                 0x4

/*
 * int_in_flag_bst_peakcur
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTPC                (0x1<<3)
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTPC_POS                   3
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTPC_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTPC_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTPC_MSK                 0x8

/*
 * int_in_flag_bst_voutcomp
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTVC                (0x1<<4)
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTVC_POS                   4
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTVC_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTVC_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBSTVC_MSK                0x10

/*
 * int_in_flag_bst_voutcomp86
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLBST86                (0x1<<5)
#define TFA98XX_INTERRUPT_IN_REG2_ICLBST86_POS                   5
#define TFA98XX_INTERRUPT_IN_REG2_ICLBST86_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBST86_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBST86_MSK                0x20

/*
 * int_in_flag_bst_voutcomp93
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLBST93                (0x1<<6)
#define TFA98XX_INTERRUPT_IN_REG2_ICLBST93_POS                   6
#define TFA98XX_INTERRUPT_IN_REG2_ICLBST93_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBST93_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLBST93_MSK                0x40

/*
 * int_in_flag_rcvldop_ready
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLRCVLD                (0x1<<7)
#define TFA98XX_INTERRUPT_IN_REG2_ICLRCVLD_POS                   7
#define TFA98XX_INTERRUPT_IN_REG2_ICLRCVLD_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLRCVLD_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLRCVLD_MSK                0x80

/*
 * int_in_flag_ocp_alarm_left
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLOCPL                 (0x1<<8)
#define TFA98XX_INTERRUPT_IN_REG2_ICLOCPL_POS                    8
#define TFA98XX_INTERRUPT_IN_REG2_ICLOCPL_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG2_ICLOCPL_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG2_ICLOCPL_MSK                0x100

/*
 * int_in_flag_ocp_alarm_right
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLOCPR                 (0x1<<9)
#define TFA98XX_INTERRUPT_IN_REG2_ICLOCPR_POS                    9
#define TFA98XX_INTERRUPT_IN_REG2_ICLOCPR_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG2_ICLOCPR_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG2_ICLOCPR_MSK                0x200

/*
 * int_in_flag_man_wait_src_settings
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWSRC               (0x1<<10)
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWSRC_POS                  10
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWSRC_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWSRC_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWSRC_MSK               0x400

/*
 * int_in_flag_man_wait_cf_config
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWCFC               (0x1<<11)
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWCFC_POS                  11
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWCFC_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWCFC_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWCFC_MSK               0x800

/*
 * int_in_flag_man_start_mute_audio
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWSMU               (0x1<<12)
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWSMU_POS                  12
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWSMU_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWSMU_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLMWSMU_MSK              0x1000

/*
 * int_in_flag_cfma_err
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLCFMER               (0x1<<13)
#define TFA98XX_INTERRUPT_IN_REG2_ICLCFMER_POS                  13
#define TFA98XX_INTERRUPT_IN_REG2_ICLCFMER_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLCFMER_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLCFMER_MSK              0x2000

/*
 * int_in_flag_cfma_ack
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLCFMAC               (0x1<<14)
#define TFA98XX_INTERRUPT_IN_REG2_ICLCFMAC_POS                  14
#define TFA98XX_INTERRUPT_IN_REG2_ICLCFMAC_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLCFMAC_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG2_ICLCFMAC_MSK              0x4000

/*
 * int_in_flag_clk_out_of_range
 */
#define TFA98XX_INTERRUPT_IN_REG2_ICLCLKOOR              (0x1<<15)
#define TFA98XX_INTERRUPT_IN_REG2_ICLCLKOOR_POS                 15
#define TFA98XX_INTERRUPT_IN_REG2_ICLCLKOOR_LEN                  1
#define TFA98XX_INTERRUPT_IN_REG2_ICLCLKOOR_MAX                  1
#define TFA98XX_INTERRUPT_IN_REG2_ICLCLKOOR_MSK             0x8000


/*
 * (0x46)-interrupt_in_reg3
 */

/*
 * int_in_flag_tdm_error
 */
#define TFA98XX_INTERRUPT_IN_REG3_ICLTDMER                (0x1<<0)
#define TFA98XX_INTERRUPT_IN_REG3_ICLTDMER_POS                   0
#define TFA98XX_INTERRUPT_IN_REG3_ICLTDMER_LEN                   1
#define TFA98XX_INTERRUPT_IN_REG3_ICLTDMER_MAX                   1
#define TFA98XX_INTERRUPT_IN_REG3_ICLTDMER_MSK                 0x1

/*
 * int_in_flag_clip_left
 */
#define TFA98XX_INTERRUPT_IN_REG3_ICLCLPL                 (0x1<<1)
#define TFA98XX_INTERRUPT_IN_REG3_ICLCLPL_POS                    1
#define TFA98XX_INTERRUPT_IN_REG3_ICLCLPL_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG3_ICLCLPL_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG3_ICLCLPL_MSK                  0x2

/*
 * int_in_flag_clip_right
 */
#define TFA98XX_INTERRUPT_IN_REG3_ICLCLPR                 (0x1<<2)
#define TFA98XX_INTERRUPT_IN_REG3_ICLCLPR_POS                    2
#define TFA98XX_INTERRUPT_IN_REG3_ICLCLPR_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG3_ICLCLPR_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG3_ICLCLPR_MSK                  0x4

/*
 * int_in_flag_mic_ocpok
 */
#define TFA98XX_INTERRUPT_IN_REG3_ICLOCPM                 (0x1<<3)
#define TFA98XX_INTERRUPT_IN_REG3_ICLOCPM_POS                    3
#define TFA98XX_INTERRUPT_IN_REG3_ICLOCPM_LEN                    1
#define TFA98XX_INTERRUPT_IN_REG3_ICLOCPM_MAX                    1
#define TFA98XX_INTERRUPT_IN_REG3_ICLOCPM_MSK                  0x8


/*
 * (0x48)-interrupt_enable_reg1
 */

/*
 * int_enable_flag_por
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEVDDS              (0x1<<0)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEVDDS_POS                 0
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEVDDS_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEVDDS_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEVDDS_MSK               0x1

/*
 * int_enable_flag_pll_lock
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEPLLS              (0x1<<1)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEPLLS_POS                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEPLLS_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEPLLS_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEPLLS_MSK               0x2

/*
 * int_enable_flag_otpok
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEOTDS              (0x1<<2)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEOTDS_POS                 2
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEOTDS_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEOTDS_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEOTDS_MSK               0x4

/*
 * int_enable_flag_ovpok
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEOVDS              (0x1<<3)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEOVDS_POS                 3
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEOVDS_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEOVDS_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEOVDS_MSK               0x8

/*
 * int_enable_flag_uvpok
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEUVDS              (0x1<<4)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEUVDS_POS                 4
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEUVDS_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEUVDS_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEUVDS_MSK              0x10

/*
 * int_enable_flag_clocks_stable
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IECLKS              (0x1<<5)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IECLKS_POS                 5
#define TFA98XX_INTERRUPT_ENABLE_REG1_IECLKS_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IECLKS_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IECLKS_MSK              0x20

/*
 * int_enable_flag_mtp_busy
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEMTPB              (0x1<<6)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEMTPB_POS                 6
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEMTPB_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEMTPB_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEMTPB_MSK              0x40

/*
 * int_enable_flag_lost_clk
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IENOCLK             (0x1<<7)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IENOCLK_POS                7
#define TFA98XX_INTERRUPT_ENABLE_REG1_IENOCLK_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IENOCLK_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IENOCLK_MSK             0x80

/*
 * int_enable_flag_cf_speakererror
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IESPKS              (0x1<<8)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IESPKS_POS                 8
#define TFA98XX_INTERRUPT_ENABLE_REG1_IESPKS_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IESPKS_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IESPKS_MSK             0x100

/*
 * int_enable_flag_cold_started
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEACS               (0x1<<9)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEACS_POS                  9
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEACS_LEN                  1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEACS_MAX                  1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEACS_MSK              0x200

/*
 * int_enable_flag_engage
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IESWS              (0x1<<10)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IESWS_POS                 10
#define TFA98XX_INTERRUPT_ENABLE_REG1_IESWS_LEN                  1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IESWS_MAX                  1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IESWS_MSK              0x400

/*
 * int_enable_flag_watchdog_reset
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEWDS              (0x1<<11)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEWDS_POS                 11
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEWDS_LEN                  1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEWDS_MAX                  1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEWDS_MSK              0x800

/*
 * int_enable_flag_enbl_amp
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEAMPS             (0x1<<12)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEAMPS_POS                12
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEAMPS_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEAMPS_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEAMPS_MSK            0x1000

/*
 * int_enable_flag_enbl_ref
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEAREFS            (0x1<<13)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEAREFS_POS               13
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEAREFS_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEAREFS_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEAREFS_MSK           0x2000

/*
 * int_enable_flag_adc10_ready
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEADCCR            (0x1<<14)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEADCCR_POS               14
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEADCCR_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEADCCR_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEADCCR_MSK           0x4000

/*
 * int_enable_flag_bod_vddd_nok
 */
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEBODNOK           (0x1<<15)
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEBODNOK_POS              15
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEBODNOK_LEN               1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEBODNOK_MAX               1
#define TFA98XX_INTERRUPT_ENABLE_REG1_IEBODNOK_MSK          0x8000


/*
 * (0x49)-interrupt_enable_reg2
 */

/*
 * int_enable_flag_bst_bstcur
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTCU             (0x1<<0)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTCU_POS                0
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTCU_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTCU_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTCU_MSK              0x1

/*
 * int_enable_flag_bst_hiz
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTHI             (0x1<<1)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTHI_POS                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTHI_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTHI_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTHI_MSK              0x2

/*
 * int_enable_flag_bst_ocpok
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTOC             (0x1<<2)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTOC_POS                2
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTOC_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTOC_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTOC_MSK              0x4

/*
 * int_enable_flag_bst_peakcur
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTPC             (0x1<<3)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTPC_POS                3
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTPC_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTPC_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTPC_MSK              0x8

/*
 * int_enable_flag_bst_voutcomp
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTVC             (0x1<<4)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTVC_POS                4
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTVC_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTVC_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBSTVC_MSK             0x10

/*
 * int_enable_flag_bst_voutcomp86
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBST86             (0x1<<5)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBST86_POS                5
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBST86_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBST86_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBST86_MSK             0x20

/*
 * int_enable_flag_bst_voutcomp93
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBST93             (0x1<<6)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBST93_POS                6
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBST93_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBST93_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEBST93_MSK             0x40

/*
 * int_enable_flag_rcvldop_ready
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IERCVLD             (0x1<<7)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IERCVLD_POS                7
#define TFA98XX_INTERRUPT_ENABLE_REG2_IERCVLD_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IERCVLD_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IERCVLD_MSK             0x80

/*
 * int_enable_flag_ocp_alarm_left
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEOCPL              (0x1<<8)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEOCPL_POS                 8
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEOCPL_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEOCPL_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEOCPL_MSK             0x100

/*
 * int_enable_flag_ocp_alarm_right
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEOCPR              (0x1<<9)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEOCPR_POS                 9
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEOCPR_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEOCPR_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEOCPR_MSK             0x200

/*
 * int_enable_flag_man_wait_src_settings
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWSRC            (0x1<<10)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWSRC_POS               10
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWSRC_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWSRC_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWSRC_MSK            0x400

/*
 * int_enable_flag_man_wait_cf_config
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWCFC            (0x1<<11)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWCFC_POS               11
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWCFC_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWCFC_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWCFC_MSK            0x800

/*
 * int_enable_flag_man_start_mute_audio
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWSMU            (0x1<<12)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWSMU_POS               12
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWSMU_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWSMU_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IEMWSMU_MSK           0x1000

/*
 * int_enable_flag_cfma_err
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECFMER            (0x1<<13)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECFMER_POS               13
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECFMER_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECFMER_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECFMER_MSK           0x2000

/*
 * int_enable_flag_cfma_ack
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECFMAC            (0x1<<14)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECFMAC_POS               14
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECFMAC_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECFMAC_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECFMAC_MSK           0x4000

/*
 * int_enable_flag_clk_out_of_range
 */
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECLKOOR           (0x1<<15)
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECLKOOR_POS              15
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECLKOOR_LEN               1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECLKOOR_MAX               1
#define TFA98XX_INTERRUPT_ENABLE_REG2_IECLKOOR_MSK          0x8000


/*
 * (0x4a)-interrupt_enable_reg3
 */

/*
 * int_enable_flag_tdm_error
 */
#define TFA98XX_INTERRUPT_ENABLE_REG3_IETDMER             (0x1<<0)
#define TFA98XX_INTERRUPT_ENABLE_REG3_IETDMER_POS                0
#define TFA98XX_INTERRUPT_ENABLE_REG3_IETDMER_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG3_IETDMER_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG3_IETDMER_MSK              0x1

/*
 * int_enable_flag_clip_left
 */
#define TFA98XX_INTERRUPT_ENABLE_REG3_IECLPL              (0x1<<1)
#define TFA98XX_INTERRUPT_ENABLE_REG3_IECLPL_POS                 1
#define TFA98XX_INTERRUPT_ENABLE_REG3_IECLPL_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG3_IECLPL_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG3_IECLPL_MSK               0x2

/*
 * int_enable_flag_clip_right
 */
#define TFA98XX_INTERRUPT_ENABLE_REG3_IECLPR              (0x1<<2)
#define TFA98XX_INTERRUPT_ENABLE_REG3_IECLPR_POS                 2
#define TFA98XX_INTERRUPT_ENABLE_REG3_IECLPR_LEN                 1
#define TFA98XX_INTERRUPT_ENABLE_REG3_IECLPR_MAX                 1
#define TFA98XX_INTERRUPT_ENABLE_REG3_IECLPR_MSK               0x4

/*
 * int_enable_flag_mic_ocpok
 */
#define TFA98XX_INTERRUPT_ENABLE_REG3_IEOCPM1             (0x1<<3)
#define TFA98XX_INTERRUPT_ENABLE_REG3_IEOCPM1_POS                3
#define TFA98XX_INTERRUPT_ENABLE_REG3_IEOCPM1_LEN                1
#define TFA98XX_INTERRUPT_ENABLE_REG3_IEOCPM1_MAX                1
#define TFA98XX_INTERRUPT_ENABLE_REG3_IEOCPM1_MSK              0x8


/*
 * (0x4c)-status_polarity_reg1
 */

/*
 * int_polarity_flag_por
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOVDDS              (0x1<<0)
#define TFA98XX_STATUS_POLARITY_REG1_IPOVDDS_POS                 0
#define TFA98XX_STATUS_POLARITY_REG1_IPOVDDS_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOVDDS_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOVDDS_MSK               0x1

/*
 * int_polarity_flag_pll_lock
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOPLLS              (0x1<<1)
#define TFA98XX_STATUS_POLARITY_REG1_IPOPLLS_POS                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOPLLS_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOPLLS_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOPLLS_MSK               0x2

/*
 * int_polarity_flag_otpok
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOOTDS              (0x1<<2)
#define TFA98XX_STATUS_POLARITY_REG1_IPOOTDS_POS                 2
#define TFA98XX_STATUS_POLARITY_REG1_IPOOTDS_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOOTDS_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOOTDS_MSK               0x4

/*
 * int_polarity_flag_ovpok
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOOVDS              (0x1<<3)
#define TFA98XX_STATUS_POLARITY_REG1_IPOOVDS_POS                 3
#define TFA98XX_STATUS_POLARITY_REG1_IPOOVDS_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOOVDS_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOOVDS_MSK               0x8

/*
 * int_polarity_flag_uvpok
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOUVDS              (0x1<<4)
#define TFA98XX_STATUS_POLARITY_REG1_IPOUVDS_POS                 4
#define TFA98XX_STATUS_POLARITY_REG1_IPOUVDS_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOUVDS_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOUVDS_MSK              0x10

/*
 * int_polarity_flag_clocks_stable
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOCLKS              (0x1<<5)
#define TFA98XX_STATUS_POLARITY_REG1_IPOCLKS_POS                 5
#define TFA98XX_STATUS_POLARITY_REG1_IPOCLKS_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOCLKS_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOCLKS_MSK              0x20

/*
 * int_polarity_flag_mtp_busy
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOMTPB              (0x1<<6)
#define TFA98XX_STATUS_POLARITY_REG1_IPOMTPB_POS                 6
#define TFA98XX_STATUS_POLARITY_REG1_IPOMTPB_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOMTPB_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOMTPB_MSK              0x40

/*
 * int_polarity_flag_lost_clk
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPONOCLK             (0x1<<7)
#define TFA98XX_STATUS_POLARITY_REG1_IPONOCLK_POS                7
#define TFA98XX_STATUS_POLARITY_REG1_IPONOCLK_LEN                1
#define TFA98XX_STATUS_POLARITY_REG1_IPONOCLK_MAX                1
#define TFA98XX_STATUS_POLARITY_REG1_IPONOCLK_MSK             0x80

/*
 * int_polarity_flag_cf_speakererror
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOSPKS              (0x1<<8)
#define TFA98XX_STATUS_POLARITY_REG1_IPOSPKS_POS                 8
#define TFA98XX_STATUS_POLARITY_REG1_IPOSPKS_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOSPKS_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOSPKS_MSK             0x100

/*
 * int_polarity_flag_cold_started
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOACS               (0x1<<9)
#define TFA98XX_STATUS_POLARITY_REG1_IPOACS_POS                  9
#define TFA98XX_STATUS_POLARITY_REG1_IPOACS_LEN                  1
#define TFA98XX_STATUS_POLARITY_REG1_IPOACS_MAX                  1
#define TFA98XX_STATUS_POLARITY_REG1_IPOACS_MSK              0x200

/*
 * int_polarity_flag_engage
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOSWS              (0x1<<10)
#define TFA98XX_STATUS_POLARITY_REG1_IPOSWS_POS                 10
#define TFA98XX_STATUS_POLARITY_REG1_IPOSWS_LEN                  1
#define TFA98XX_STATUS_POLARITY_REG1_IPOSWS_MAX                  1
#define TFA98XX_STATUS_POLARITY_REG1_IPOSWS_MSK              0x400

/*
 * int_polarity_flag_watchdog_reset
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOWDS              (0x1<<11)
#define TFA98XX_STATUS_POLARITY_REG1_IPOWDS_POS                 11
#define TFA98XX_STATUS_POLARITY_REG1_IPOWDS_LEN                  1
#define TFA98XX_STATUS_POLARITY_REG1_IPOWDS_MAX                  1
#define TFA98XX_STATUS_POLARITY_REG1_IPOWDS_MSK              0x800

/*
 * int_polarity_flag_enbl_amp
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOAMPS             (0x1<<12)
#define TFA98XX_STATUS_POLARITY_REG1_IPOAMPS_POS                12
#define TFA98XX_STATUS_POLARITY_REG1_IPOAMPS_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOAMPS_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG1_IPOAMPS_MSK            0x1000

/*
 * int_polarity_flag_enbl_ref
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOAREFS            (0x1<<13)
#define TFA98XX_STATUS_POLARITY_REG1_IPOAREFS_POS               13
#define TFA98XX_STATUS_POLARITY_REG1_IPOAREFS_LEN                1
#define TFA98XX_STATUS_POLARITY_REG1_IPOAREFS_MAX                1
#define TFA98XX_STATUS_POLARITY_REG1_IPOAREFS_MSK           0x2000

/*
 * int_polarity_flag_adc10_ready
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOADCCR            (0x1<<14)
#define TFA98XX_STATUS_POLARITY_REG1_IPOADCCR_POS               14
#define TFA98XX_STATUS_POLARITY_REG1_IPOADCCR_LEN                1
#define TFA98XX_STATUS_POLARITY_REG1_IPOADCCR_MAX                1
#define TFA98XX_STATUS_POLARITY_REG1_IPOADCCR_MSK           0x4000

/*
 * int_polarity_flag_bod_vddd_nok
 */
#define TFA98XX_STATUS_POLARITY_REG1_IPOBODNOK           (0x1<<15)
#define TFA98XX_STATUS_POLARITY_REG1_IPOBODNOK_POS              15
#define TFA98XX_STATUS_POLARITY_REG1_IPOBODNOK_LEN               1
#define TFA98XX_STATUS_POLARITY_REG1_IPOBODNOK_MAX               1
#define TFA98XX_STATUS_POLARITY_REG1_IPOBODNOK_MSK          0x8000


/*
 * (0x4d)-status_polarity_reg2
 */

/*
 * int_polarity_flag_bst_bstcur
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTCU             (0x1<<0)
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTCU_POS                0
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTCU_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTCU_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTCU_MSK              0x1

/*
 * int_polarity_flag_bst_hiz
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTHI             (0x1<<1)
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTHI_POS                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTHI_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTHI_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTHI_MSK              0x2

/*
 * int_polarity_flag_bst_ocpok
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTOC             (0x1<<2)
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTOC_POS                2
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTOC_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTOC_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTOC_MSK              0x4

/*
 * int_polarity_flag_bst_peakcur
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTPC             (0x1<<3)
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTPC_POS                3
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTPC_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTPC_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTPC_MSK              0x8

/*
 * int_polarity_flag_bst_voutcomp
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTVC             (0x1<<4)
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTVC_POS                4
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTVC_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTVC_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBSTVC_MSK             0x10

/*
 * int_polarity_flag_bst_voutcomp86
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOBST86             (0x1<<5)
#define TFA98XX_STATUS_POLARITY_REG2_IPOBST86_POS                5
#define TFA98XX_STATUS_POLARITY_REG2_IPOBST86_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBST86_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBST86_MSK             0x20

/*
 * int_polarity_flag_bst_voutcomp93
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOBST93             (0x1<<6)
#define TFA98XX_STATUS_POLARITY_REG2_IPOBST93_POS                6
#define TFA98XX_STATUS_POLARITY_REG2_IPOBST93_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBST93_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOBST93_MSK             0x40

/*
 * int_polarity_flag_rcvldop_ready
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPORCVLD             (0x1<<7)
#define TFA98XX_STATUS_POLARITY_REG2_IPORCVLD_POS                7
#define TFA98XX_STATUS_POLARITY_REG2_IPORCVLD_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPORCVLD_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPORCVLD_MSK             0x80

/*
 * int_polarity_flag_ocp_alarm_left
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOOCPL              (0x1<<8)
#define TFA98XX_STATUS_POLARITY_REG2_IPOOCPL_POS                 8
#define TFA98XX_STATUS_POLARITY_REG2_IPOOCPL_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG2_IPOOCPL_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG2_IPOOCPL_MSK             0x100

/*
 * int_polarity_flag_ocp_alarm_right
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOOCPR              (0x1<<9)
#define TFA98XX_STATUS_POLARITY_REG2_IPOOCPR_POS                 9
#define TFA98XX_STATUS_POLARITY_REG2_IPOOCPR_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG2_IPOOCPR_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG2_IPOOCPR_MSK             0x200

/*
 * int_polarity_flag_man_wait_src_settings
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWSRC            (0x1<<10)
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWSRC_POS               10
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWSRC_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWSRC_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWSRC_MSK            0x400

/*
 * int_polarity_flag_man_wait_cf_config
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWCFC            (0x1<<11)
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWCFC_POS               11
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWCFC_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWCFC_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWCFC_MSK            0x800

/*
 * int_polarity_flag_man_start_mute_audio
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWSMU            (0x1<<12)
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWSMU_POS               12
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWSMU_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWSMU_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOMWSMU_MSK           0x1000

/*
 * int_polarity_flag_cfma_err
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOCFMER            (0x1<<13)
#define TFA98XX_STATUS_POLARITY_REG2_IPOCFMER_POS               13
#define TFA98XX_STATUS_POLARITY_REG2_IPOCFMER_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOCFMER_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOCFMER_MSK           0x2000

/*
 * int_polarity_flag_cfma_ack
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPOCFMAC            (0x1<<14)
#define TFA98XX_STATUS_POLARITY_REG2_IPOCFMAC_POS               14
#define TFA98XX_STATUS_POLARITY_REG2_IPOCFMAC_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOCFMAC_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPOCFMAC_MSK           0x4000

/*
 * int_polarity_flag_clk_out_of_range
 */
#define TFA98XX_STATUS_POLARITY_REG2_IPCLKOOR            (0x1<<15)
#define TFA98XX_STATUS_POLARITY_REG2_IPCLKOOR_POS               15
#define TFA98XX_STATUS_POLARITY_REG2_IPCLKOOR_LEN                1
#define TFA98XX_STATUS_POLARITY_REG2_IPCLKOOR_MAX                1
#define TFA98XX_STATUS_POLARITY_REG2_IPCLKOOR_MSK           0x8000


/*
 * (0x4e)-status_polarity_reg3
 */

/*
 * int_polarity_flag_tdm_error
 */
#define TFA98XX_STATUS_POLARITY_REG3_IPOTDMER             (0x1<<0)
#define TFA98XX_STATUS_POLARITY_REG3_IPOTDMER_POS                0
#define TFA98XX_STATUS_POLARITY_REG3_IPOTDMER_LEN                1
#define TFA98XX_STATUS_POLARITY_REG3_IPOTDMER_MAX                1
#define TFA98XX_STATUS_POLARITY_REG3_IPOTDMER_MSK              0x1

/*
 * int_polarity_flag_clip_left
 */
#define TFA98XX_STATUS_POLARITY_REG3_IPOCLPL              (0x1<<1)
#define TFA98XX_STATUS_POLARITY_REG3_IPOCLPL_POS                 1
#define TFA98XX_STATUS_POLARITY_REG3_IPOCLPL_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG3_IPOCLPL_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG3_IPOCLPL_MSK               0x2

/*
 * int_polarity_flag_clip_right
 */
#define TFA98XX_STATUS_POLARITY_REG3_IPOCLPR              (0x1<<2)
#define TFA98XX_STATUS_POLARITY_REG3_IPOCLPR_POS                 2
#define TFA98XX_STATUS_POLARITY_REG3_IPOCLPR_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG3_IPOCLPR_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG3_IPOCLPR_MSK               0x4

/*
 * int_polarity_flag_mic_ocpok
 */
#define TFA98XX_STATUS_POLARITY_REG3_IPOOCPM              (0x1<<3)
#define TFA98XX_STATUS_POLARITY_REG3_IPOOCPM_POS                 3
#define TFA98XX_STATUS_POLARITY_REG3_IPOOCPM_LEN                 1
#define TFA98XX_STATUS_POLARITY_REG3_IPOOCPM_MAX                 1
#define TFA98XX_STATUS_POLARITY_REG3_IPOOCPM_MSK               0x8


/*
 * (0x50)-bat_prot_config
 */

/*
 * vbat_prot_attack_time
 */
#define TFA98XX_BAT_PROT_CONFIG_BSSCR                     (0x3<<0)
#define TFA98XX_BAT_PROT_CONFIG_BSSCR_POS                        0
#define TFA98XX_BAT_PROT_CONFIG_BSSCR_LEN                        2
#define TFA98XX_BAT_PROT_CONFIG_BSSCR_MAX                        3
#define TFA98XX_BAT_PROT_CONFIG_BSSCR_MSK                      0x3

/*
 * vbat_prot_thlevel
 */
#define TFA98XX_BAT_PROT_CONFIG_BSST                      (0xf<<2)
#define TFA98XX_BAT_PROT_CONFIG_BSST_POS                         2
#define TFA98XX_BAT_PROT_CONFIG_BSST_LEN                         4
#define TFA98XX_BAT_PROT_CONFIG_BSST_MAX                        15
#define TFA98XX_BAT_PROT_CONFIG_BSST_MSK                      0x3c

/*
 * vbat_prot_max_reduct
 */
#define TFA98XX_BAT_PROT_CONFIG_BSSRL                     (0x3<<6)
#define TFA98XX_BAT_PROT_CONFIG_BSSRL_POS                        6
#define TFA98XX_BAT_PROT_CONFIG_BSSRL_LEN                        2
#define TFA98XX_BAT_PROT_CONFIG_BSSRL_MAX                        3
#define TFA98XX_BAT_PROT_CONFIG_BSSRL_MSK                     0xc0

/*
 * vbat_prot_release_time
 */
#define TFA98XX_BAT_PROT_CONFIG_BSSRR                     (0x7<<8)
#define TFA98XX_BAT_PROT_CONFIG_BSSRR_POS                        8
#define TFA98XX_BAT_PROT_CONFIG_BSSRR_LEN                        3
#define TFA98XX_BAT_PROT_CONFIG_BSSRR_MAX                        7
#define TFA98XX_BAT_PROT_CONFIG_BSSRR_MSK                    0x700

/*
 * vbat_prot_hysterese
 */
#define TFA98XX_BAT_PROT_CONFIG_BSSHY                    (0x3<<11)
#define TFA98XX_BAT_PROT_CONFIG_BSSHY_POS                       11
#define TFA98XX_BAT_PROT_CONFIG_BSSHY_LEN                        2
#define TFA98XX_BAT_PROT_CONFIG_BSSHY_MAX                        3
#define TFA98XX_BAT_PROT_CONFIG_BSSHY_MSK                   0x1800

/*
 * sel_vbat
 */
#define TFA98XX_BAT_PROT_CONFIG_BSSR                     (0x1<<14)
#define TFA98XX_BAT_PROT_CONFIG_BSSR_POS                        14
#define TFA98XX_BAT_PROT_CONFIG_BSSR_LEN                         1
#define TFA98XX_BAT_PROT_CONFIG_BSSR_MAX                         1
#define TFA98XX_BAT_PROT_CONFIG_BSSR_MSK                    0x4000

/*
 * bypass_clipper
 */
#define TFA98XX_BAT_PROT_CONFIG_BSSBY                    (0x1<<15)
#define TFA98XX_BAT_PROT_CONFIG_BSSBY_POS                       15
#define TFA98XX_BAT_PROT_CONFIG_BSSBY_LEN                        1
#define TFA98XX_BAT_PROT_CONFIG_BSSBY_MAX                        1
#define TFA98XX_BAT_PROT_CONFIG_BSSBY_MSK                   0x8000


/*
 * (0x51)-audio_control
 */

/*
 * batsense_steepness
 */
#define TFA98XX_AUDIO_CONTROL_BSSS                        (0x1<<0)
#define TFA98XX_AUDIO_CONTROL_BSSS_POS                           0
#define TFA98XX_AUDIO_CONTROL_BSSS_LEN                           1
#define TFA98XX_AUDIO_CONTROL_BSSS_MAX                           1
#define TFA98XX_AUDIO_CONTROL_BSSS_MSK                         0x1

/*
 * soft_mute
 */
#define TFA98XX_AUDIO_CONTROL_INTSMUTE                    (0x1<<1)
#define TFA98XX_AUDIO_CONTROL_INTSMUTE_POS                       1
#define TFA98XX_AUDIO_CONTROL_INTSMUTE_LEN                       1
#define TFA98XX_AUDIO_CONTROL_INTSMUTE_MAX                       1
#define TFA98XX_AUDIO_CONTROL_INTSMUTE_MSK                     0x2

/*
 * cf_mute_left
 */
#define TFA98XX_AUDIO_CONTROL_CFSML                       (0x1<<2)
#define TFA98XX_AUDIO_CONTROL_CFSML_POS                          2
#define TFA98XX_AUDIO_CONTROL_CFSML_LEN                          1
#define TFA98XX_AUDIO_CONTROL_CFSML_MAX                          1
#define TFA98XX_AUDIO_CONTROL_CFSML_MSK                        0x4

/*
 * cf_mute_right
 */
#define TFA98XX_AUDIO_CONTROL_CFSMR                       (0x1<<3)
#define TFA98XX_AUDIO_CONTROL_CFSMR_POS                          3
#define TFA98XX_AUDIO_CONTROL_CFSMR_LEN                          1
#define TFA98XX_AUDIO_CONTROL_CFSMR_MAX                          1
#define TFA98XX_AUDIO_CONTROL_CFSMR_MSK                        0x8

/*
 * bypass_hp_left
 */
#define TFA98XX_AUDIO_CONTROL_HPFBYPL                     (0x1<<4)
#define TFA98XX_AUDIO_CONTROL_HPFBYPL_POS                        4
#define TFA98XX_AUDIO_CONTROL_HPFBYPL_LEN                        1
#define TFA98XX_AUDIO_CONTROL_HPFBYPL_MAX                        1
#define TFA98XX_AUDIO_CONTROL_HPFBYPL_MSK                     0x10

/*
 * bypass_hp_right
 */
#define TFA98XX_AUDIO_CONTROL_HPFBYPR                     (0x1<<5)
#define TFA98XX_AUDIO_CONTROL_HPFBYPR_POS                        5
#define TFA98XX_AUDIO_CONTROL_HPFBYPR_LEN                        1
#define TFA98XX_AUDIO_CONTROL_HPFBYPR_MAX                        1
#define TFA98XX_AUDIO_CONTROL_HPFBYPR_MSK                     0x20

/*
 * enbl_dpsa_left
 */
#define TFA98XX_AUDIO_CONTROL_DPSAL                       (0x1<<6)
#define TFA98XX_AUDIO_CONTROL_DPSAL_POS                          6
#define TFA98XX_AUDIO_CONTROL_DPSAL_LEN                          1
#define TFA98XX_AUDIO_CONTROL_DPSAL_MAX                          1
#define TFA98XX_AUDIO_CONTROL_DPSAL_MSK                       0x40

/*
 * enbl_dpsa_right
 */
#define TFA98XX_AUDIO_CONTROL_DPSAR                       (0x1<<7)
#define TFA98XX_AUDIO_CONTROL_DPSAR_POS                          7
#define TFA98XX_AUDIO_CONTROL_DPSAR_LEN                          1
#define TFA98XX_AUDIO_CONTROL_DPSAR_MAX                          1
#define TFA98XX_AUDIO_CONTROL_DPSAR_MSK                       0x80

/*
 * cf_volume
 */
#define TFA98XX_AUDIO_CONTROL_VOL                        (0xff<<8)
#define TFA98XX_AUDIO_CONTROL_VOL_POS                            8
#define TFA98XX_AUDIO_CONTROL_VOL_LEN                            8
#define TFA98XX_AUDIO_CONTROL_VOL_MAX                          255
#define TFA98XX_AUDIO_CONTROL_VOL_MSK                       0xff00


/*
 * (0x52)-amplifier_config
 */

/*
 * ctrl_rcv
 */
#define TFA98XX_AMPLIFIER_CONFIG_HNDSFRCV                 (0x1<<0)
#define TFA98XX_AMPLIFIER_CONFIG_HNDSFRCV_POS                    0
#define TFA98XX_AMPLIFIER_CONFIG_HNDSFRCV_LEN                    1
#define TFA98XX_AMPLIFIER_CONFIG_HNDSFRCV_MAX                    1
#define TFA98XX_AMPLIFIER_CONFIG_HNDSFRCV_MSK                  0x1

/*
 * ctrl_cc
 */
#define TFA98XX_AMPLIFIER_CONFIG_CLIPCTRL                 (0x7<<2)
#define TFA98XX_AMPLIFIER_CONFIG_CLIPCTRL_POS                    2
#define TFA98XX_AMPLIFIER_CONFIG_CLIPCTRL_LEN                    3
#define TFA98XX_AMPLIFIER_CONFIG_CLIPCTRL_MAX                    7
#define TFA98XX_AMPLIFIER_CONFIG_CLIPCTRL_MSK                 0x1c

/*
 * gain
 */
#define TFA98XX_AMPLIFIER_CONFIG_AMPGAIN                 (0xff<<5)
#define TFA98XX_AMPLIFIER_CONFIG_AMPGAIN_POS                     5
#define TFA98XX_AMPLIFIER_CONFIG_AMPGAIN_LEN                     8
#define TFA98XX_AMPLIFIER_CONFIG_AMPGAIN_MAX                   255
#define TFA98XX_AMPLIFIER_CONFIG_AMPGAIN_MSK                0x1fe0

/*
 * ctrl_slopectrl
 */
#define TFA98XX_AMPLIFIER_CONFIG_SLOPEE                  (0x1<<13)
#define TFA98XX_AMPLIFIER_CONFIG_SLOPEE_POS                     13
#define TFA98XX_AMPLIFIER_CONFIG_SLOPEE_LEN                      1
#define TFA98XX_AMPLIFIER_CONFIG_SLOPEE_MAX                      1
#define TFA98XX_AMPLIFIER_CONFIG_SLOPEE_MSK                 0x2000

/*
 * ctrl_slope
 */
#define TFA98XX_AMPLIFIER_CONFIG_SLOPESET                (0x3<<14)
#define TFA98XX_AMPLIFIER_CONFIG_SLOPESET_POS                   14
#define TFA98XX_AMPLIFIER_CONFIG_SLOPESET_LEN                    2
#define TFA98XX_AMPLIFIER_CONFIG_SLOPESET_MAX                    3
#define TFA98XX_AMPLIFIER_CONFIG_SLOPESET_MSK               0xc000


/*
 * (0x5a)-audio_control2
 */

/*
 * cf_volume_sec
 */
#define TFA98XX_AUDIO_CONTROL2_VOLSEC                    (0xff<<0)
#define TFA98XX_AUDIO_CONTROL2_VOLSEC_POS                        0
#define TFA98XX_AUDIO_CONTROL2_VOLSEC_LEN                        8
#define TFA98XX_AUDIO_CONTROL2_VOLSEC_MAX                      255
#define TFA98XX_AUDIO_CONTROL2_VOLSEC_MSK                     0xff

/*
 * sw_profile
 */
#define TFA98XX_AUDIO_CONTROL2_SWPROFIL                  (0xff<<8)
#define TFA98XX_AUDIO_CONTROL2_SWPROFIL_POS                      8
#define TFA98XX_AUDIO_CONTROL2_SWPROFIL_LEN                      8
#define TFA98XX_AUDIO_CONTROL2_SWPROFIL_MAX                    255
#define TFA98XX_AUDIO_CONTROL2_SWPROFIL_MSK                 0xff00


/*
 * (0x70)-dcdc_control0
 */

/*
 * boost_volt
 */
#define TFA98XX_DCDC_CONTROL0_DCVO                        (0x7<<0)
#define TFA98XX_DCDC_CONTROL0_DCVO_POS                           0
#define TFA98XX_DCDC_CONTROL0_DCVO_LEN                           3
#define TFA98XX_DCDC_CONTROL0_DCVO_MAX                           7
#define TFA98XX_DCDC_CONTROL0_DCVO_MSK                         0x7

/*
 * boost_cur
 */
#define TFA98XX_DCDC_CONTROL0_DCMCC                       (0xf<<3)
#define TFA98XX_DCDC_CONTROL0_DCMCC_POS                          3
#define TFA98XX_DCDC_CONTROL0_DCMCC_LEN                          4
#define TFA98XX_DCDC_CONTROL0_DCMCC_MAX                         15
#define TFA98XX_DCDC_CONTROL0_DCMCC_MSK                       0x78

/*
 * bst_coil_value
 */
#define TFA98XX_DCDC_CONTROL0_DCCV                        (0x3<<7)
#define TFA98XX_DCDC_CONTROL0_DCCV_POS                           7
#define TFA98XX_DCDC_CONTROL0_DCCV_LEN                           2
#define TFA98XX_DCDC_CONTROL0_DCCV_MAX                           3
#define TFA98XX_DCDC_CONTROL0_DCCV_MSK                       0x180

/*
 * boost_intel
 */
#define TFA98XX_DCDC_CONTROL0_DCIE                        (0x1<<9)
#define TFA98XX_DCDC_CONTROL0_DCIE_POS                           9
#define TFA98XX_DCDC_CONTROL0_DCIE_LEN                           1
#define TFA98XX_DCDC_CONTROL0_DCIE_MAX                           1
#define TFA98XX_DCDC_CONTROL0_DCIE_MSK                       0x200

/*
 * boost_speed
 */
#define TFA98XX_DCDC_CONTROL0_DCSR                       (0x1<<10)
#define TFA98XX_DCDC_CONTROL0_DCSR_POS                          10
#define TFA98XX_DCDC_CONTROL0_DCSR_LEN                           1
#define TFA98XX_DCDC_CONTROL0_DCSR_MAX                           1
#define TFA98XX_DCDC_CONTROL0_DCSR_MSK                       0x400

/*
 * dcdc_synchronisation
 */
#define TFA98XX_DCDC_CONTROL0_DCSYNCP                    (0x7<<11)
#define TFA98XX_DCDC_CONTROL0_DCSYNCP_POS                       11
#define TFA98XX_DCDC_CONTROL0_DCSYNCP_LEN                        3
#define TFA98XX_DCDC_CONTROL0_DCSYNCP_MAX                        7
#define TFA98XX_DCDC_CONTROL0_DCSYNCP_MSK                   0x3800

/*
 * dcdcoff_mode
 */
#define TFA98XX_DCDC_CONTROL0_DCDIS                      (0x1<<14)
#define TFA98XX_DCDC_CONTROL0_DCDIS_POS                         14
#define TFA98XX_DCDC_CONTROL0_DCDIS_LEN                          1
#define TFA98XX_DCDC_CONTROL0_DCDIS_MAX                          1
#define TFA98XX_DCDC_CONTROL0_DCDIS_MSK                     0x4000


/*
 * (0x90)-cf_controls
 */

/*
 * cf_rst_dsp
 */
#define TFA98XX_CF_CONTROLS_RST                           (0x1<<0)
#define TFA98XX_CF_CONTROLS_RST_POS                              0
#define TFA98XX_CF_CONTROLS_RST_LEN                              1
#define TFA98XX_CF_CONTROLS_RST_MAX                              1
#define TFA98XX_CF_CONTROLS_RST_MSK                            0x1

/*
 * cf_dmem
 */
#define TFA98XX_CF_CONTROLS_DMEM                          (0x3<<1)
#define TFA98XX_CF_CONTROLS_DMEM_POS                             1
#define TFA98XX_CF_CONTROLS_DMEM_LEN                             2
#define TFA98XX_CF_CONTROLS_DMEM_MAX                             3
#define TFA98XX_CF_CONTROLS_DMEM_MSK                           0x6

/*
 * cf_aif
 */
#define TFA98XX_CF_CONTROLS_AIF                           (0x1<<3)
#define TFA98XX_CF_CONTROLS_AIF_POS                              3
#define TFA98XX_CF_CONTROLS_AIF_LEN                              1
#define TFA98XX_CF_CONTROLS_AIF_MAX                              1
#define TFA98XX_CF_CONTROLS_AIF_MSK                            0x8

/*
 * cf_int
 */
#define TFA98XX_CF_CONTROLS_CFINT                         (0x1<<4)
#define TFA98XX_CF_CONTROLS_CFINT_POS                            4
#define TFA98XX_CF_CONTROLS_CFINT_LEN                            1
#define TFA98XX_CF_CONTROLS_CFINT_MAX                            1
#define TFA98XX_CF_CONTROLS_CFINT_MSK                         0x10

/*
 * cf_cgate_off
 */
#define TFA98XX_CF_CONTROLS_CFCGATE                       (0x1<<5)
#define TFA98XX_CF_CONTROLS_CFCGATE_POS                          5
#define TFA98XX_CF_CONTROLS_CFCGATE_LEN                          1
#define TFA98XX_CF_CONTROLS_CFCGATE_MAX                          1
#define TFA98XX_CF_CONTROLS_CFCGATE_MSK                       0x20

/*
 * cf_req_cmd
 */
#define TFA98XX_CF_CONTROLS_REQCMD                        (0x1<<8)
#define TFA98XX_CF_CONTROLS_REQCMD_POS                           8
#define TFA98XX_CF_CONTROLS_REQCMD_LEN                           1
#define TFA98XX_CF_CONTROLS_REQCMD_MAX                           1
#define TFA98XX_CF_CONTROLS_REQCMD_MSK                       0x100

/*
 * cf_req_reset
 */
#define TFA98XX_CF_CONTROLS_REQRST                        (0x1<<9)
#define TFA98XX_CF_CONTROLS_REQRST_POS                           9
#define TFA98XX_CF_CONTROLS_REQRST_LEN                           1
#define TFA98XX_CF_CONTROLS_REQRST_MAX                           1
#define TFA98XX_CF_CONTROLS_REQRST_MSK                       0x200

/*
 * cf_req_mips
 */
#define TFA98XX_CF_CONTROLS_REQMIPS                      (0x1<<10)
#define TFA98XX_CF_CONTROLS_REQMIPS_POS                         10
#define TFA98XX_CF_CONTROLS_REQMIPS_LEN                          1
#define TFA98XX_CF_CONTROLS_REQMIPS_MAX                          1
#define TFA98XX_CF_CONTROLS_REQMIPS_MSK                      0x400

/*
 * cf_req_mute_ready
 */
#define TFA98XX_CF_CONTROLS_REQMUTED                     (0x1<<11)
#define TFA98XX_CF_CONTROLS_REQMUTED_POS                        11
#define TFA98XX_CF_CONTROLS_REQMUTED_LEN                         1
#define TFA98XX_CF_CONTROLS_REQMUTED_MAX                         1
#define TFA98XX_CF_CONTROLS_REQMUTED_MSK                     0x800

/*
 * cf_req_volume_ready
 */
#define TFA98XX_CF_CONTROLS_REQVOL                       (0x1<<12)
#define TFA98XX_CF_CONTROLS_REQVOL_POS                          12
#define TFA98XX_CF_CONTROLS_REQVOL_LEN                           1
#define TFA98XX_CF_CONTROLS_REQVOL_MAX                           1
#define TFA98XX_CF_CONTROLS_REQVOL_MSK                      0x1000

/*
 * cf_req_damage
 */
#define TFA98XX_CF_CONTROLS_REQDMG                       (0x1<<13)
#define TFA98XX_CF_CONTROLS_REQDMG_POS                          13
#define TFA98XX_CF_CONTROLS_REQDMG_LEN                           1
#define TFA98XX_CF_CONTROLS_REQDMG_MAX                           1
#define TFA98XX_CF_CONTROLS_REQDMG_MSK                      0x2000

/*
 * cf_req_calibrate_ready
 */
#define TFA98XX_CF_CONTROLS_REQCAL                       (0x1<<14)
#define TFA98XX_CF_CONTROLS_REQCAL_POS                          14
#define TFA98XX_CF_CONTROLS_REQCAL_LEN                           1
#define TFA98XX_CF_CONTROLS_REQCAL_MAX                           1
#define TFA98XX_CF_CONTROLS_REQCAL_MSK                      0x4000

/*
 * cf_req_reserved
 */
#define TFA98XX_CF_CONTROLS_REQRSV                       (0x1<<15)
#define TFA98XX_CF_CONTROLS_REQRSV_POS                          15
#define TFA98XX_CF_CONTROLS_REQRSV_LEN                           1
#define TFA98XX_CF_CONTROLS_REQRSV_MAX                           1
#define TFA98XX_CF_CONTROLS_REQRSV_MSK                      0x8000


/*
 * (0x91)-cf_mad
 */

/*
 * cf_madd
 */
#define TFA98XX_CF_MAD_MADD                            (0xffff<<0)
#define TFA98XX_CF_MAD_MADD_POS                                  0
#define TFA98XX_CF_MAD_MADD_LEN                                 16
#define TFA98XX_CF_MAD_MADD_MAX                              65535
#define TFA98XX_CF_MAD_MADD_MSK                             0xffff


/*
 * (0x92)-cf_mem
 */

/*
 * cf_mema
 */
#define TFA98XX_CF_MEM_MEMA                            (0xffff<<0)
#define TFA98XX_CF_MEM_MEMA_POS                                  0
#define TFA98XX_CF_MEM_MEMA_LEN                                 16
#define TFA98XX_CF_MEM_MEMA_MAX                              65535
#define TFA98XX_CF_MEM_MEMA_MSK                             0xffff


/*
 * (0x93)-cf_status
 */

/*
 * cf_err
 */
#define TFA98XX_CF_STATUS_ERR                            (0xff<<0)
#define TFA98XX_CF_STATUS_ERR_POS                                0
#define TFA98XX_CF_STATUS_ERR_LEN                                8
#define TFA98XX_CF_STATUS_ERR_MAX                              255
#define TFA98XX_CF_STATUS_ERR_MSK                             0xff

/*
 * cf_ack_cmd
 */
#define TFA98XX_CF_STATUS_ACKCMD                          (0x1<<8)
#define TFA98XX_CF_STATUS_ACKCMD_POS                             8
#define TFA98XX_CF_STATUS_ACKCMD_LEN                             1
#define TFA98XX_CF_STATUS_ACKCMD_MAX                             1
#define TFA98XX_CF_STATUS_ACKCMD_MSK                         0x100

/*
 * cf_ack_reset
 */
#define TFA98XX_CF_STATUS_ACKRST                          (0x1<<9)
#define TFA98XX_CF_STATUS_ACKRST_POS                             9
#define TFA98XX_CF_STATUS_ACKRST_LEN                             1
#define TFA98XX_CF_STATUS_ACKRST_MAX                             1
#define TFA98XX_CF_STATUS_ACKRST_MSK                         0x200

/*
 * cf_ack_mips
 */
#define TFA98XX_CF_STATUS_ACKMIPS                        (0x1<<10)
#define TFA98XX_CF_STATUS_ACKMIPS_POS                           10
#define TFA98XX_CF_STATUS_ACKMIPS_LEN                            1
#define TFA98XX_CF_STATUS_ACKMIPS_MAX                            1
#define TFA98XX_CF_STATUS_ACKMIPS_MSK                        0x400

/*
 * cf_ack_mute_ready
 */
#define TFA98XX_CF_STATUS_ACKMUTED                       (0x1<<11)
#define TFA98XX_CF_STATUS_ACKMUTED_POS                          11
#define TFA98XX_CF_STATUS_ACKMUTED_LEN                           1
#define TFA98XX_CF_STATUS_ACKMUTED_MAX                           1
#define TFA98XX_CF_STATUS_ACKMUTED_MSK                       0x800

/*
 * cf_ack_volume_ready
 */
#define TFA98XX_CF_STATUS_ACKVOL                         (0x1<<12)
#define TFA98XX_CF_STATUS_ACKVOL_POS                            12
#define TFA98XX_CF_STATUS_ACKVOL_LEN                             1
#define TFA98XX_CF_STATUS_ACKVOL_MAX                             1
#define TFA98XX_CF_STATUS_ACKVOL_MSK                        0x1000

/*
 * cf_ack_damage
 */
#define TFA98XX_CF_STATUS_ACKDMG                         (0x1<<13)
#define TFA98XX_CF_STATUS_ACKDMG_POS                            13
#define TFA98XX_CF_STATUS_ACKDMG_LEN                             1
#define TFA98XX_CF_STATUS_ACKDMG_MAX                             1
#define TFA98XX_CF_STATUS_ACKDMG_MSK                        0x2000

/*
 * cf_ack_calibrate_ready
 */
#define TFA98XX_CF_STATUS_ACKCAL                         (0x1<<14)
#define TFA98XX_CF_STATUS_ACKCAL_POS                            14
#define TFA98XX_CF_STATUS_ACKCAL_LEN                             1
#define TFA98XX_CF_STATUS_ACKCAL_MAX                             1
#define TFA98XX_CF_STATUS_ACKCAL_MSK                        0x4000

/*
 * cf_ack_reserved
 */
#define TFA98XX_CF_STATUS_ACKRSV                         (0x1<<15)
#define TFA98XX_CF_STATUS_ACKRSV_POS                            15
#define TFA98XX_CF_STATUS_ACKRSV_LEN                             1
#define TFA98XX_CF_STATUS_ACKRSV_MAX                             1
#define TFA98XX_CF_STATUS_ACKRSV_MSK                        0x8000


/*
 * (0xa1)-mtpkey2_reg
 */

/*
 * mtpkey2
 */
#define TFA98XX_MTPKEY2_REG_MTPK                         (0xff<<0)
#define TFA98XX_MTPKEY2_REG_MTPK_POS                             0
#define TFA98XX_MTPKEY2_REG_MTPK_LEN                             8
#define TFA98XX_MTPKEY2_REG_MTPK_MAX                           255
#define TFA98XX_MTPKEY2_REG_MTPK_MSK                          0xff


/*
 * (0xa2)-mtp_status
 */

/*
 * key01_locked
 */
#define TFA98XX_MTP_STATUS_KEY1LOCKED                     (0x1<<0)
#define TFA98XX_MTP_STATUS_KEY1LOCKED_POS                        0
#define TFA98XX_MTP_STATUS_KEY1LOCKED_LEN                        1
#define TFA98XX_MTP_STATUS_KEY1LOCKED_MAX                        1
#define TFA98XX_MTP_STATUS_KEY1LOCKED_MSK                      0x1

/*
 * key02_locked
 */
#define TFA98XX_MTP_STATUS_KEY2LOCKED                     (0x1<<1)
#define TFA98XX_MTP_STATUS_KEY2LOCKED_POS                        1
#define TFA98XX_MTP_STATUS_KEY2LOCKED_LEN                        1
#define TFA98XX_MTP_STATUS_KEY2LOCKED_MAX                        1
#define TFA98XX_MTP_STATUS_KEY2LOCKED_MSK                      0x2


/*
 * (0xa3)-KEY_protected_mtp_control
 */

/*
 * auto_copy_iic_to_mtp
 */
#define TFA98XX_KEY_PROTECTED_MTP_CONTROL_CIMTP           (0x1<<6)
#define TFA98XX_KEY_PROTECTED_MTP_CONTROL_CIMTP_POS              6
#define TFA98XX_KEY_PROTECTED_MTP_CONTROL_CIMTP_LEN              1
#define TFA98XX_KEY_PROTECTED_MTP_CONTROL_CIMTP_MAX              1
#define TFA98XX_KEY_PROTECTED_MTP_CONTROL_CIMTP_MSK           0x40


/*
 * (0xa5)-mtp_data_out_msb
 */

/*
 * mtp_man_data_out_msb
 */
#define TFA98XX_MTP_DATA_OUT_MSB_MTPRDMSB              (0xffff<<0)
#define TFA98XX_MTP_DATA_OUT_MSB_MTPRDMSB_POS                    0
#define TFA98XX_MTP_DATA_OUT_MSB_MTPRDMSB_LEN                   16
#define TFA98XX_MTP_DATA_OUT_MSB_MTPRDMSB_MAX                65535
#define TFA98XX_MTP_DATA_OUT_MSB_MTPRDMSB_MSK               0xffff


/*
 * (0xa6)-mtp_data_out_lsb
 */

/*
 * mtp_man_data_out_lsb
 */
#define TFA98XX_MTP_DATA_OUT_LSB_MTPRDLSB              (0xffff<<0)
#define TFA98XX_MTP_DATA_OUT_LSB_MTPRDLSB_POS                    0
#define TFA98XX_MTP_DATA_OUT_LSB_MTPRDLSB_LEN                   16
#define TFA98XX_MTP_DATA_OUT_LSB_MTPRDLSB_MAX                65535
#define TFA98XX_MTP_DATA_OUT_LSB_MTPRDLSB_MSK               0xffff


/*
 * (0xb1)-temp_sensor_config
 */

/*
 * ext_temp
 */
#define TFA98XX_TEMP_SENSOR_CONFIG_EXTTS                (0x1ff<<0)
#define TFA98XX_TEMP_SENSOR_CONFIG_EXTTS_POS                     0
#define TFA98XX_TEMP_SENSOR_CONFIG_EXTTS_LEN                     9
#define TFA98XX_TEMP_SENSOR_CONFIG_EXTTS_MAX                   511
#define TFA98XX_TEMP_SENSOR_CONFIG_EXTTS_MSK                 0x1ff

/*
 * ext_temp_sel
 */
#define TFA98XX_TEMP_SENSOR_CONFIG_TROS                   (0x1<<9)
#define TFA98XX_TEMP_SENSOR_CONFIG_TROS_POS                      9
#define TFA98XX_TEMP_SENSOR_CONFIG_TROS_LEN                      1
#define TFA98XX_TEMP_SENSOR_CONFIG_TROS_MAX                      1
#define TFA98XX_TEMP_SENSOR_CONFIG_TROS_MSK                  0x200


/*
 * (0xf0)-KEY2_protected_MTP0
 */

/*
 * calibration_onetime
 */
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC                (0x1<<0)
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS                   0
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_LEN                   1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MAX                   1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK                 0x1

/*
 * calibr_ron_done
 */
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX                 (0x1<<1)
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS                    1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_LEN                    1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MAX                    1
#define TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK                  0x2

/*
 * calibr_dcdc_api_calibrate
 */
#define TFA98XX_KEY2_PROTECTED_MTP0_DCMCCAPI              (0x1<<2)
#define TFA98XX_KEY2_PROTECTED_MTP0_DCMCCAPI_POS                 2
#define TFA98XX_KEY2_PROTECTED_MTP0_DCMCCAPI_LEN                 1
#define TFA98XX_KEY2_PROTECTED_MTP0_DCMCCAPI_MAX                 1
#define TFA98XX_KEY2_PROTECTED_MTP0_DCMCCAPI_MSK               0x4

/*
 * calibr_dcdc_delta_sign
 */
#define TFA98XX_KEY2_PROTECTED_MTP0_DCMCCSB               (0x1<<3)
#define TFA98XX_KEY2_PROTECTED_MTP0_DCMCCSB_POS                  3
#define TFA98XX_KEY2_PROTECTED_MTP0_DCMCCSB_LEN                  1
#define TFA98XX_KEY2_PROTECTED_MTP0_DCMCCSB_MAX                  1
#define TFA98XX_KEY2_PROTECTED_MTP0_DCMCCSB_MSK                0x8

/*
 * calibr_dcdc_delta
 */
#define TFA98XX_KEY2_PROTECTED_MTP0_USERDEF               (0x7<<4)
#define TFA98XX_KEY2_PROTECTED_MTP0_USERDEF_POS                  4
#define TFA98XX_KEY2_PROTECTED_MTP0_USERDEF_LEN                  3
#define TFA98XX_KEY2_PROTECTED_MTP0_USERDEF_MAX                  7
#define TFA98XX_KEY2_PROTECTED_MTP0_USERDEF_MSK               0x70


/*
 * (0xf4)-KEY1_protected_MTP4
 */


/*
 * (0xf5)-KEY1_protected_MTP5
 */

#endif /* TFA98XX_GENREGS_H */
