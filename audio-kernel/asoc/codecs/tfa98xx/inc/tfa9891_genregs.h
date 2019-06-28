/** Filename: Tfa98xx_genregs.h
 *  This file was generated automatically on 07/01/15 at 10:25:08. 
 *  Source file: TFA9891_I2C_list_V11.xls
 */

#ifndef TFA9891_GENREGS_H
#define TFA9891_GENREGS_H


#define TFA98XX_STATUSREG                  0x00
#define TFA98XX_BATTERYVOLTAGE             0x01
#define TFA9891_TEMPERATURE                0x02
#define TFA98XX_REVISIONNUMBER             0x03
#define TFA98XX_I2SREG                     0x04
#define TFA98XX_BAT_PROT                   0x05
#define TFA98XX_AUDIO_CTR                  0x06
#define TFA98XX_DCDCBOOST                  0x07
#define TFA98XX_SPKR_CALIBRATION           0x08
#define TFA98XX_SYS_CTRL                   0x09
#define TFA98XX_I2S_SEL_REG                0x0a
#define TFA98XX_HIDDEN_MTP_KEY2            0x0b
#define TFA98XX_INTERRUPT_REG              0x0f
#define TFA98XX_PDM_CTRL                   0x10
#define TFA98XX_PDM_OUT_CTRL               0x11
#define TFA98XX_PDM_DS4_R                  0x12
#define TFA98XX_PDM_DS4_L                  0x13
#define TFA98XX_CTRL_SAAM_PGA              0x22
#define TFA98XX_MISC_CTRL                  0x25
#define TFA98XX_CURRENTSENSE1              0x46
#define TFA98XX_CURRENTSENSE4              0x49
#define TFA98XX_HIDDEN_MTP_CTRL_REG3       0x62
#define TFA9891_CF_CONTROLS                0x70
#define TFA9891_CF_MAD                     0x71
#define TFA9891_CF_MEM                     0x72
#define TFA9891_CF_STATUS                  0x73
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP 0x80

/*
 * (0x00)-StatusReg
 */

/*
 * POR
 */
#define TFA98XX_STATUSREG_VDDS                              (0x1<<0)
#define TFA98XX_STATUSREG_VDDS_POS                                 0
#define TFA98XX_STATUSREG_VDDS_LEN                                 1
#define TFA98XX_STATUSREG_VDDS_MAX                                 1
#define TFA98XX_STATUSREG_VDDS_MSK                               0x1

/*
 * PLL_LOCK
 */
#define TFA98XX_STATUSREG_PLLS                              (0x1<<1)
#define TFA98XX_STATUSREG_PLLS_POS                                 1
#define TFA98XX_STATUSREG_PLLS_LEN                                 1
#define TFA98XX_STATUSREG_PLLS_MAX                                 1
#define TFA98XX_STATUSREG_PLLS_MSK                               0x2

/*
 * flag_otpok
 */
#define TFA98XX_STATUSREG_OTDS                              (0x1<<2)
#define TFA98XX_STATUSREG_OTDS_POS                                 2
#define TFA98XX_STATUSREG_OTDS_LEN                                 1
#define TFA98XX_STATUSREG_OTDS_MAX                                 1
#define TFA98XX_STATUSREG_OTDS_MSK                               0x4

/*
 * flag_ovpok
 */
#define TFA98XX_STATUSREG_OVDS                              (0x1<<3)
#define TFA98XX_STATUSREG_OVDS_POS                                 3
#define TFA98XX_STATUSREG_OVDS_LEN                                 1
#define TFA98XX_STATUSREG_OVDS_MAX                                 1
#define TFA98XX_STATUSREG_OVDS_MSK                               0x8

/*
 * flag_uvpok
 */
#define TFA98XX_STATUSREG_UVDS                              (0x1<<4)
#define TFA98XX_STATUSREG_UVDS_POS                                 4
#define TFA98XX_STATUSREG_UVDS_LEN                                 1
#define TFA98XX_STATUSREG_UVDS_MAX                                 1
#define TFA98XX_STATUSREG_UVDS_MSK                              0x10

/*
 * flag_OCP_alarm
 */
#define TFA98XX_STATUSREG_OCDS                              (0x1<<5)
#define TFA98XX_STATUSREG_OCDS_POS                                 5
#define TFA98XX_STATUSREG_OCDS_LEN                                 1
#define TFA98XX_STATUSREG_OCDS_MAX                                 1
#define TFA98XX_STATUSREG_OCDS_MSK                              0x20

/*
 * flag_clocks_stable
 */
#define TFA98XX_STATUSREG_CLKS                              (0x1<<6)
#define TFA98XX_STATUSREG_CLKS_POS                                 6
#define TFA98XX_STATUSREG_CLKS_LEN                                 1
#define TFA98XX_STATUSREG_CLKS_MAX                                 1
#define TFA98XX_STATUSREG_CLKS_MSK                              0x40

/*
 * CLIP
 */
#define TFA98XX_STATUSREG_CLIPS                             (0x1<<7)
#define TFA98XX_STATUSREG_CLIPS_POS                                7
#define TFA98XX_STATUSREG_CLIPS_LEN                                1
#define TFA98XX_STATUSREG_CLIPS_MAX                                1
#define TFA98XX_STATUSREG_CLIPS_MSK                             0x80

/*
 * mtp_busy
 */
#define TFA98XX_STATUSREG_MTPB                              (0x1<<8)
#define TFA98XX_STATUSREG_MTPB_POS                                 8
#define TFA98XX_STATUSREG_MTPB_LEN                                 1
#define TFA98XX_STATUSREG_MTPB_MAX                                 1
#define TFA98XX_STATUSREG_MTPB_MSK                             0x100

/*
 * flag_pwrokbst
 */
#define TFA98XX_STATUSREG_DCCS                              (0x1<<9)
#define TFA98XX_STATUSREG_DCCS_POS                                 9
#define TFA98XX_STATUSREG_DCCS_LEN                                 1
#define TFA98XX_STATUSREG_DCCS_MAX                                 1
#define TFA98XX_STATUSREG_DCCS_MSK                             0x200

/*
 * flag_cf_speakererror
 */
#define TFA98XX_STATUSREG_SPKS                             (0x1<<10)
#define TFA98XX_STATUSREG_SPKS_POS                                10
#define TFA98XX_STATUSREG_SPKS_LEN                                 1
#define TFA98XX_STATUSREG_SPKS_MAX                                 1
#define TFA98XX_STATUSREG_SPKS_MSK                             0x400

/*
 * flag_cold_started
 */
#define TFA98XX_STATUSREG_ACS                              (0x1<<11)
#define TFA98XX_STATUSREG_ACS_POS                                 11
#define TFA98XX_STATUSREG_ACS_LEN                                  1
#define TFA98XX_STATUSREG_ACS_MAX                                  1
#define TFA98XX_STATUSREG_ACS_MSK                              0x800

/*
 * flag_engage
 */
#define TFA98XX_STATUSREG_SWS                              (0x1<<12)
#define TFA98XX_STATUSREG_SWS_POS                                 12
#define TFA98XX_STATUSREG_SWS_LEN                                  1
#define TFA98XX_STATUSREG_SWS_MAX                                  1
#define TFA98XX_STATUSREG_SWS_MSK                             0x1000

/*
 * flag_watchdog_reset
 */
#define TFA98XX_STATUSREG_WDS                              (0x1<<13)
#define TFA98XX_STATUSREG_WDS_POS                                 13
#define TFA98XX_STATUSREG_WDS_LEN                                  1
#define TFA98XX_STATUSREG_WDS_MAX                                  1
#define TFA98XX_STATUSREG_WDS_MSK                             0x2000

/*
 * flag_enbl_amp
 */
#define TFA98XX_STATUSREG_AMPS                             (0x1<<14)
#define TFA98XX_STATUSREG_AMPS_POS                                14
#define TFA98XX_STATUSREG_AMPS_LEN                                 1
#define TFA98XX_STATUSREG_AMPS_MAX                                 1
#define TFA98XX_STATUSREG_AMPS_MSK                            0x4000

/*
 * flag_enbl_ref
 */
#define TFA98XX_STATUSREG_AREFS                            (0x1<<15)
#define TFA98XX_STATUSREG_AREFS_POS                               15
#define TFA98XX_STATUSREG_AREFS_LEN                                1
#define TFA98XX_STATUSREG_AREFS_MAX                                1
#define TFA98XX_STATUSREG_AREFS_MSK                           0x8000

/*
 * (0x01)-BatteryVoltage
 */

/*
 * bat_adc
 */
#define TFA98XX_BATTERYVOLTAGE_BATS                       (0x3ff<<0)
#define TFA98XX_BATTERYVOLTAGE_BATS_POS                            0
#define TFA98XX_BATTERYVOLTAGE_BATS_LEN                           10
#define TFA98XX_BATTERYVOLTAGE_BATS_MAX                         1023
#define TFA98XX_BATTERYVOLTAGE_BATS_MSK                        0x3ff


/*
 * (0x02)-Temperature
 */

/*
 * temp_adc
 */
#define TFA9891_TEMPERATURE_TEMPS                         (0x1ff<<0)
#define TFA9891_TEMPERATURE_TEMPS_POS                              0
#define TFA9891_TEMPERATURE_TEMPS_LEN                              9
#define TFA9891_TEMPERATURE_TEMPS_MAX                            511
#define TFA9891_TEMPERATURE_TEMPS_MSK                          0x1ff


/*
 * (0x03)-RevisionNumber
 */

/*
 * rev_reg
 */
#define TFA98XX_REVISIONNUMBER_REV                         (0xff<<0)
#define TFA98XX_REVISIONNUMBER_REV_POS                             0
#define TFA98XX_REVISIONNUMBER_REV_LEN                             8
#define TFA98XX_REVISIONNUMBER_REV_MAX                           255
#define TFA98XX_REVISIONNUMBER_REV_MSK                          0xff


/*
 * (0x04)-I2SReg
 */

/*
 * i2s_seti
 */
#define TFA98XX_I2SREG_I2SF                                 (0x7<<0)
#define TFA98XX_I2SREG_I2SF_POS                                    0
#define TFA98XX_I2SREG_I2SF_LEN                                    3
#define TFA98XX_I2SREG_I2SF_MAX                                    7
#define TFA98XX_I2SREG_I2SF_MSK                                  0x7

/*
 * chan_sel1
 */
#define TFA98XX_I2SREG_CHS12                                (0x3<<3)
#define TFA98XX_I2SREG_CHS12_POS                                   3
#define TFA98XX_I2SREG_CHS12_LEN                                   2
#define TFA98XX_I2SREG_CHS12_MAX                                   3
#define TFA98XX_I2SREG_CHS12_MSK                                0x18

/*
 * lr_sw_i2si2
 */
#define TFA98XX_I2SREG_CHS3                                 (0x1<<5)
#define TFA98XX_I2SREG_CHS3_POS                                    5
#define TFA98XX_I2SREG_CHS3_LEN                                    1
#define TFA98XX_I2SREG_CHS3_MAX                                    1
#define TFA98XX_I2SREG_CHS3_MSK                                 0x20

/*
 * input_sel
 */
#define TFA98XX_I2SREG_CHSA                                 (0x3<<6)
#define TFA98XX_I2SREG_CHSA_POS                                    6
#define TFA98XX_I2SREG_CHSA_LEN                                    2
#define TFA98XX_I2SREG_CHSA_MAX                                    3
#define TFA98XX_I2SREG_CHSA_MSK                                 0xc0

/*
 * datao_sel
 */
#define TFA98XX_I2SREG_I2SDOC                               (0x3<<8)
#define TFA98XX_I2SREG_I2SDOC_POS                                  8
#define TFA98XX_I2SREG_I2SDOC_LEN                                  2
#define TFA98XX_I2SREG_I2SDOC_MAX                                  3
#define TFA98XX_I2SREG_I2SDOC_MSK                              0x300

/*
 * disable_idp
 */
#define TFA98XX_I2SREG_DISP                                (0x1<<10)
#define TFA98XX_I2SREG_DISP_POS                                   10
#define TFA98XX_I2SREG_DISP_LEN                                    1
#define TFA98XX_I2SREG_DISP_MAX                                    1
#define TFA98XX_I2SREG_DISP_MSK                                0x400

/*
 * enbl_datao
 */
#define TFA98XX_I2SREG_I2SDOE                              (0x1<<11)
#define TFA98XX_I2SREG_I2SDOE_POS                                 11
#define TFA98XX_I2SREG_I2SDOE_LEN                                  1
#define TFA98XX_I2SREG_I2SDOE_MAX                                  1
#define TFA98XX_I2SREG_I2SDOE_MSK                              0x800

/*
 * i2s_fs
 */
#define TFA98XX_I2SREG_I2SSR                               (0xf<<12)
#define TFA98XX_I2SREG_I2SSR_POS                                  12
#define TFA98XX_I2SREG_I2SSR_LEN                                   4
#define TFA98XX_I2SREG_I2SSR_MAX                                  15
#define TFA98XX_I2SREG_I2SSR_MSK                              0xf000


/*
 * (0x05)-bat_prot
 */

/*
 * vbat_prot_attacktime
 */
#define TFA98XX_BAT_PROT_BSSCR                              (0x3<<0)
#define TFA98XX_BAT_PROT_BSSCR_POS                                 0
#define TFA98XX_BAT_PROT_BSSCR_LEN                                 2
#define TFA98XX_BAT_PROT_BSSCR_MAX                                 3
#define TFA98XX_BAT_PROT_BSSCR_MSK                               0x3

/*
 * vbat_prot_thlevel
 */
#define TFA98XX_BAT_PROT_BSST                               (0xf<<2)
#define TFA98XX_BAT_PROT_BSST_POS                                  2
#define TFA98XX_BAT_PROT_BSST_LEN                                  4
#define TFA98XX_BAT_PROT_BSST_MAX                                 15
#define TFA98XX_BAT_PROT_BSST_MSK                               0x3c

/*
 * vbat_prot_max_reduct
 */
#define TFA98XX_BAT_PROT_BSSRL                              (0x3<<6)
#define TFA98XX_BAT_PROT_BSSRL_POS                                 6
#define TFA98XX_BAT_PROT_BSSRL_LEN                                 2
#define TFA98XX_BAT_PROT_BSSRL_MAX                                 3
#define TFA98XX_BAT_PROT_BSSRL_MSK                              0xc0

/*
 * vbat_prot_release_t
 */
#define TFA98XX_BAT_PROT_BSSRR                              (0x7<<8)
#define TFA98XX_BAT_PROT_BSSRR_POS                                 8
#define TFA98XX_BAT_PROT_BSSRR_LEN                                 3
#define TFA98XX_BAT_PROT_BSSRR_MAX                                 7
#define TFA98XX_BAT_PROT_BSSRR_MSK                             0x700

/*
 * vbat_prot_hysterese
 */
#define TFA98XX_BAT_PROT_BSSHY                             (0x3<<11)
#define TFA98XX_BAT_PROT_BSSHY_POS                                11
#define TFA98XX_BAT_PROT_BSSHY_LEN                                 2
#define TFA98XX_BAT_PROT_BSSHY_MAX                                 3
#define TFA98XX_BAT_PROT_BSSHY_MSK                            0x1800

/*
 * sel_vbat
 */
#define TFA98XX_BAT_PROT_BSSR                              (0x1<<14)
#define TFA98XX_BAT_PROT_BSSR_POS                                 14
#define TFA98XX_BAT_PROT_BSSR_LEN                                  1
#define TFA98XX_BAT_PROT_BSSR_MAX                                  1
#define TFA98XX_BAT_PROT_BSSR_MSK                             0x4000

/*
 * bypass_clipper
 */
#define TFA98XX_BAT_PROT_BSSBY                             (0x1<<15)
#define TFA98XX_BAT_PROT_BSSBY_POS                                15
#define TFA98XX_BAT_PROT_BSSBY_LEN                                 1
#define TFA98XX_BAT_PROT_BSSBY_MAX                                 1
#define TFA98XX_BAT_PROT_BSSBY_MSK                            0x8000


/*
 * (0x06)-audio_ctr
 */

/*
 * dpsa
 */
#define TFA98XX_AUDIO_CTR_DPSA                              (0x1<<0)
#define TFA98XX_AUDIO_CTR_DPSA_POS                                 0
#define TFA98XX_AUDIO_CTR_DPSA_LEN                                 1
#define TFA98XX_AUDIO_CTR_DPSA_MAX                                 1
#define TFA98XX_AUDIO_CTR_DPSA_MSK                               0x1

/*
 * ctrl_slope
 */
#define TFA98XX_AUDIO_CTR_AMPSL                             (0xf<<1)
#define TFA98XX_AUDIO_CTR_AMPSL_POS                                1
#define TFA98XX_AUDIO_CTR_AMPSL_LEN                                4
#define TFA98XX_AUDIO_CTR_AMPSL_MAX                               15
#define TFA98XX_AUDIO_CTR_AMPSL_MSK                             0x1e

/*
 * cf_mute
 */
#define TFA98XX_AUDIO_CTR_CFSM                              (0x1<<5)
#define TFA98XX_AUDIO_CTR_CFSM_POS                                 5
#define TFA98XX_AUDIO_CTR_CFSM_LEN                                 1
#define TFA98XX_AUDIO_CTR_CFSM_MAX                                 1
#define TFA98XX_AUDIO_CTR_CFSM_MSK                              0x20

/*
 * ctrl_batsensesteepness
 */
#define TFA98XX_AUDIO_CTR_BSSS                              (0x1<<7)
#define TFA98XX_AUDIO_CTR_BSSS_POS                                 7
#define TFA98XX_AUDIO_CTR_BSSS_LEN                                 1
#define TFA98XX_AUDIO_CTR_BSSS_MAX                                 1
#define TFA98XX_AUDIO_CTR_BSSS_MSK                              0x80

/*
 * vol
 */
#define TFA98XX_AUDIO_CTR_VOL                              (0xff<<8)
#define TFA98XX_AUDIO_CTR_VOL_POS                                  8
#define TFA98XX_AUDIO_CTR_VOL_LEN                                  8
#define TFA98XX_AUDIO_CTR_VOL_MAX                                255
#define TFA98XX_AUDIO_CTR_VOL_MSK                             0xff00


/*
 * (0x07)-DCDCboost
 */

/*
 * ctrl_bstvolt
 */
#define TFA98XX_DCDCBOOST_DCVO                              (0x7<<0)
#define TFA98XX_DCDCBOOST_DCVO_POS                                 0
#define TFA98XX_DCDCBOOST_DCVO_LEN                                 3
#define TFA98XX_DCDCBOOST_DCVO_MAX                                 7
#define TFA98XX_DCDCBOOST_DCVO_MSK                               0x7

/*
 * ctrl_bstcur
 */
#define TFA98XX_DCDCBOOST_DCMCC                             (0x7<<3)
#define TFA98XX_DCDCBOOST_DCMCC_POS                                3
#define TFA98XX_DCDCBOOST_DCMCC_LEN                                3
#define TFA98XX_DCDCBOOST_DCMCC_MAX                                7
#define TFA98XX_DCDCBOOST_DCMCC_MSK                             0x38

/*
 * boost_intel
 */
#define TFA98XX_DCDCBOOST_DCIE                             (0x1<<10)
#define TFA98XX_DCDCBOOST_DCIE_POS                                10
#define TFA98XX_DCDCBOOST_DCIE_LEN                                 1
#define TFA98XX_DCDCBOOST_DCIE_MAX                                 1
#define TFA98XX_DCDCBOOST_DCIE_MSK                             0x400

/*
 * boost_speed
 */
#define TFA98XX_DCDCBOOST_DCSR                             (0x1<<11)
#define TFA98XX_DCDCBOOST_DCSR_POS                                11
#define TFA98XX_DCDCBOOST_DCSR_LEN                                 1
#define TFA98XX_DCDCBOOST_DCSR_MAX                                 1
#define TFA98XX_DCDCBOOST_DCSR_MSK                             0x800


/*
 * (0x08)-spkr_calibration
 */

/*
 * ext_temp_sel
 */
#define TFA98XX_SPKR_CALIBRATION_TROS                       (0x1<<0)
#define TFA98XX_SPKR_CALIBRATION_TROS_POS                          0
#define TFA98XX_SPKR_CALIBRATION_TROS_LEN                          1
#define TFA98XX_SPKR_CALIBRATION_TROS_MAX                          1
#define TFA98XX_SPKR_CALIBRATION_TROS_MSK                        0x1

/*
 * ext_temp
 */
#define TFA98XX_SPKR_CALIBRATION_EXTTS                    (0x1ff<<1)
#define TFA98XX_SPKR_CALIBRATION_EXTTS_POS                         1
#define TFA98XX_SPKR_CALIBRATION_EXTTS_LEN                         9
#define TFA98XX_SPKR_CALIBRATION_EXTTS_MAX                       511
#define TFA98XX_SPKR_CALIBRATION_EXTTS_MSK                     0x3fe


/*
 * (0x09)-sys_ctrl
 */

/*
 * PowerDown
 */
#define TFA98XX_SYS_CTRL_PWDN                               (0x1<<0)
#define TFA98XX_SYS_CTRL_PWDN_POS                                  0
#define TFA98XX_SYS_CTRL_PWDN_LEN                                  1
#define TFA98XX_SYS_CTRL_PWDN_MAX                                  1
#define TFA98XX_SYS_CTRL_PWDN_MSK                                0x1

/*
 * reset
 */
#define TFA98XX_SYS_CTRL_I2CR                               (0x1<<1)
#define TFA98XX_SYS_CTRL_I2CR_POS                                  1
#define TFA98XX_SYS_CTRL_I2CR_LEN                                  1
#define TFA98XX_SYS_CTRL_I2CR_MAX                                  1
#define TFA98XX_SYS_CTRL_I2CR_MSK                                0x2

/*
 * enbl_coolflux
 */
#define TFA98XX_SYS_CTRL_CFE                                (0x1<<2)
#define TFA98XX_SYS_CTRL_CFE_POS                                   2
#define TFA98XX_SYS_CTRL_CFE_LEN                                   1
#define TFA98XX_SYS_CTRL_CFE_MAX                                   1
#define TFA98XX_SYS_CTRL_CFE_MSK                                 0x4

/*
 * enbl_amplifier
 */
#define TFA98XX_SYS_CTRL_AMPE                               (0x1<<3)
#define TFA98XX_SYS_CTRL_AMPE_POS                                  3
#define TFA98XX_SYS_CTRL_AMPE_LEN                                  1
#define TFA98XX_SYS_CTRL_AMPE_MAX                                  1
#define TFA98XX_SYS_CTRL_AMPE_MSK                                0x8

/*
 * enbl_boost
 */
#define TFA98XX_SYS_CTRL_DCA                                (0x1<<4)
#define TFA98XX_SYS_CTRL_DCA_POS                                   4
#define TFA98XX_SYS_CTRL_DCA_LEN                                   1
#define TFA98XX_SYS_CTRL_DCA_MAX                                   1
#define TFA98XX_SYS_CTRL_DCA_MSK                                0x10

/*
 * cf_configured
 */
#define TFA98XX_SYS_CTRL_SBSL                               (0x1<<5)
#define TFA98XX_SYS_CTRL_SBSL_POS                                  5
#define TFA98XX_SYS_CTRL_SBSL_LEN                                  1
#define TFA98XX_SYS_CTRL_SBSL_MAX                                  1
#define TFA98XX_SYS_CTRL_SBSL_MSK                               0x20

/*
 * sel_enbl_amplifier
 */
#define TFA98XX_SYS_CTRL_AMPC                               (0x1<<6)
#define TFA98XX_SYS_CTRL_AMPC_POS                                  6
#define TFA98XX_SYS_CTRL_AMPC_LEN                                  1
#define TFA98XX_SYS_CTRL_AMPC_MAX                                  1
#define TFA98XX_SYS_CTRL_AMPC_MSK                               0x40

/*
 * dcdcoff_mode
 */
#define TFA98XX_SYS_CTRL_DCDIS                              (0x1<<7)
#define TFA98XX_SYS_CTRL_DCDIS_POS                                 7
#define TFA98XX_SYS_CTRL_DCDIS_LEN                                 1
#define TFA98XX_SYS_CTRL_DCDIS_MAX                                 1
#define TFA98XX_SYS_CTRL_DCDIS_MSK                              0x80

/*
 * cttr_iddqtest
 */
#define TFA98XX_SYS_CTRL_PSDR                               (0x1<<8)
#define TFA98XX_SYS_CTRL_PSDR_POS                                  8
#define TFA98XX_SYS_CTRL_PSDR_LEN                                  1
#define TFA98XX_SYS_CTRL_PSDR_MAX                                  1
#define TFA98XX_SYS_CTRL_PSDR_MSK                              0x100

/*
 * ctrl_coil_value
 */
#define TFA98XX_SYS_CTRL_DCCV                               (0x3<<9)
#define TFA98XX_SYS_CTRL_DCCV_POS                                  9
#define TFA98XX_SYS_CTRL_DCCV_LEN                                  2
#define TFA98XX_SYS_CTRL_DCCV_MAX                                  3
#define TFA98XX_SYS_CTRL_DCCV_MSK                              0x600

/*
 * ctrl_sel_cf_clock
 */
#define TFA98XX_SYS_CTRL_CCFD                              (0x3<<11)
#define TFA98XX_SYS_CTRL_CCFD_POS                                 11
#define TFA98XX_SYS_CTRL_CCFD_LEN                                  2
#define TFA98XX_SYS_CTRL_CCFD_MAX                                  3
#define TFA98XX_SYS_CTRL_CCFD_MSK                             0x1800

/*
 * intf_sel
 */
#define TFA98XX_SYS_CTRL_ISEL                              (0x1<<13)
#define TFA98XX_SYS_CTRL_ISEL_POS                                 13
#define TFA98XX_SYS_CTRL_ISEL_LEN                                  1
#define TFA98XX_SYS_CTRL_ISEL_MAX                                  1
#define TFA98XX_SYS_CTRL_ISEL_MSK                             0x2000

/*
 * sel_ws_bck
 */
#define TFA98XX_SYS_CTRL_IPLL                              (0x1<<14)
#define TFA98XX_SYS_CTRL_IPLL_POS                                 14
#define TFA98XX_SYS_CTRL_IPLL_LEN                                  1
#define TFA98XX_SYS_CTRL_IPLL_MAX                                  1
#define TFA98XX_SYS_CTRL_IPLL_MSK                             0x4000


/*
 * (0x0a)-I2S_sel_reg
 */

/*
 * sel_i2so_l
 */
#define TFA98XX_I2S_SEL_REG_DOLS                            (0x7<<0)
#define TFA98XX_I2S_SEL_REG_DOLS_POS                               0
#define TFA98XX_I2S_SEL_REG_DOLS_LEN                               3
#define TFA98XX_I2S_SEL_REG_DOLS_MAX                               7
#define TFA98XX_I2S_SEL_REG_DOLS_MSK                             0x7

/*
 * sel_i2so_r
 */
#define TFA98XX_I2S_SEL_REG_DORS                            (0x7<<3)
#define TFA98XX_I2S_SEL_REG_DORS_POS                               3
#define TFA98XX_I2S_SEL_REG_DORS_LEN                               3
#define TFA98XX_I2S_SEL_REG_DORS_MAX                               7
#define TFA98XX_I2S_SEL_REG_DORS_MSK                            0x38

/*
 * ctrl_spkr_coil
 */
#define TFA98XX_I2S_SEL_REG_SPKL                            (0x7<<6)
#define TFA98XX_I2S_SEL_REG_SPKL_POS                               6
#define TFA98XX_I2S_SEL_REG_SPKL_LEN                               3
#define TFA98XX_I2S_SEL_REG_SPKL_MAX                               7
#define TFA98XX_I2S_SEL_REG_SPKL_MSK                           0x1c0

/*
 * ctrl_spr_res
 */
#define TFA98XX_I2S_SEL_REG_SPKR                            (0x3<<9)
#define TFA98XX_I2S_SEL_REG_SPKR_POS                               9
#define TFA98XX_I2S_SEL_REG_SPKR_LEN                               2
#define TFA98XX_I2S_SEL_REG_SPKR_MAX                               3
#define TFA98XX_I2S_SEL_REG_SPKR_MSK                           0x600

/*
 * ctrl_dcdc_spkr_i_comp_gain
 */
#define TFA98XX_I2S_SEL_REG_DCFG                           (0xf<<11)
#define TFA98XX_I2S_SEL_REG_DCFG_POS                              11
#define TFA98XX_I2S_SEL_REG_DCFG_LEN                               4
#define TFA98XX_I2S_SEL_REG_DCFG_MAX                              15
#define TFA98XX_I2S_SEL_REG_DCFG_MSK                          0x7800


/*
 * (0x0b)-Hidden_mtp_key2
 */

/*
 * MTP_key2
 */
#define TFA98XX_HIDDEN_MTP_KEY2_MTPK                       (0xff<<0)
#define TFA98XX_HIDDEN_MTP_KEY2_MTPK_POS                           0
#define TFA98XX_HIDDEN_MTP_KEY2_MTPK_LEN                           8
#define TFA98XX_HIDDEN_MTP_KEY2_MTPK_MAX                         255
#define TFA98XX_HIDDEN_MTP_KEY2_MTPK_MSK                        0xff


/*
 * (0x0f)-interrupt_reg
 */

/*
 * flag_por_mask
 */
#define TFA98XX_INTERRUPT_REG_VDDD                          (0x1<<0)
#define TFA98XX_INTERRUPT_REG_VDDD_POS                             0
#define TFA98XX_INTERRUPT_REG_VDDD_LEN                             1
#define TFA98XX_INTERRUPT_REG_VDDD_MAX                             1
#define TFA98XX_INTERRUPT_REG_VDDD_MSK                           0x1

/*
 * flag_otpok_mask
 */
#define TFA98XX_INTERRUPT_REG_OTDD                          (0x1<<1)
#define TFA98XX_INTERRUPT_REG_OTDD_POS                             1
#define TFA98XX_INTERRUPT_REG_OTDD_LEN                             1
#define TFA98XX_INTERRUPT_REG_OTDD_MAX                             1
#define TFA98XX_INTERRUPT_REG_OTDD_MSK                           0x2

/*
 * flag_ovpok_mask
 */
#define TFA98XX_INTERRUPT_REG_OVDD                          (0x1<<2)
#define TFA98XX_INTERRUPT_REG_OVDD_POS                             2
#define TFA98XX_INTERRUPT_REG_OVDD_LEN                             1
#define TFA98XX_INTERRUPT_REG_OVDD_MAX                             1
#define TFA98XX_INTERRUPT_REG_OVDD_MSK                           0x4

/*
 * flag_uvpok_mask
 */
#define TFA98XX_INTERRUPT_REG_UVDD                          (0x1<<3)
#define TFA98XX_INTERRUPT_REG_UVDD_POS                             3
#define TFA98XX_INTERRUPT_REG_UVDD_LEN                             1
#define TFA98XX_INTERRUPT_REG_UVDD_MAX                             1
#define TFA98XX_INTERRUPT_REG_UVDD_MSK                           0x8

/*
 * flag_ocp_alarm_mask
 */
#define TFA98XX_INTERRUPT_REG_OCDD                          (0x1<<4)
#define TFA98XX_INTERRUPT_REG_OCDD_POS                             4
#define TFA98XX_INTERRUPT_REG_OCDD_LEN                             1
#define TFA98XX_INTERRUPT_REG_OCDD_MAX                             1
#define TFA98XX_INTERRUPT_REG_OCDD_MSK                          0x10

/*
 * flag_clocks_stable_mask
 */
#define TFA98XX_INTERRUPT_REG_CLKD                          (0x1<<5)
#define TFA98XX_INTERRUPT_REG_CLKD_POS                             5
#define TFA98XX_INTERRUPT_REG_CLKD_LEN                             1
#define TFA98XX_INTERRUPT_REG_CLKD_MAX                             1
#define TFA98XX_INTERRUPT_REG_CLKD_MSK                          0x20

/*
 * flag_pwrokbst_mask
 */
#define TFA98XX_INTERRUPT_REG_DCCD                          (0x1<<6)
#define TFA98XX_INTERRUPT_REG_DCCD_POS                             6
#define TFA98XX_INTERRUPT_REG_DCCD_LEN                             1
#define TFA98XX_INTERRUPT_REG_DCCD_MAX                             1
#define TFA98XX_INTERRUPT_REG_DCCD_MSK                          0x40

/*
 * flag_cf_speakererror_mask
 */
#define TFA98XX_INTERRUPT_REG_SPKD                          (0x1<<7)
#define TFA98XX_INTERRUPT_REG_SPKD_POS                             7
#define TFA98XX_INTERRUPT_REG_SPKD_LEN                             1
#define TFA98XX_INTERRUPT_REG_SPKD_MAX                             1
#define TFA98XX_INTERRUPT_REG_SPKD_MSK                          0x80

/*
 * flag_watchdog_reset_mask
 */
#define TFA98XX_INTERRUPT_REG_WDD                           (0x1<<8)
#define TFA98XX_INTERRUPT_REG_WDD_POS                              8
#define TFA98XX_INTERRUPT_REG_WDD_LEN                              1
#define TFA98XX_INTERRUPT_REG_WDD_MAX                              1
#define TFA98XX_INTERRUPT_REG_WDD_MSK                          0x100

/*
 * enable_interrupt
 */
#define TFA98XX_INTERRUPT_REG_INT                          (0x1<<14)
#define TFA98XX_INTERRUPT_REG_INT_POS                             14
#define TFA98XX_INTERRUPT_REG_INT_LEN                              1
#define TFA98XX_INTERRUPT_REG_INT_MAX                              1
#define TFA98XX_INTERRUPT_REG_INT_MSK                         0x4000

/*
 * invert_int_polarity
 */
#define TFA98XX_INTERRUPT_REG_INTP                         (0x1<<15)
#define TFA98XX_INTERRUPT_REG_INTP_POS                            15
#define TFA98XX_INTERRUPT_REG_INTP_LEN                             1
#define TFA98XX_INTERRUPT_REG_INTP_MAX                             1
#define TFA98XX_INTERRUPT_REG_INTP_MSK                        0x8000


/*
 * (0x10)-pdm_ctrl
 */

/*
 * pdm_i2s_input
 */
#define TFA98XX_PDM_CTRL_PDMSEL                             (0x1<<0)
#define TFA98XX_PDM_CTRL_PDMSEL_POS                                0
#define TFA98XX_PDM_CTRL_PDMSEL_LEN                                1
#define TFA98XX_PDM_CTRL_PDMSEL_MAX                                1
#define TFA98XX_PDM_CTRL_PDMSEL_MSK                              0x1

/*
 * I2S_master_ena
 */
#define TFA98XX_PDM_CTRL_I2SMOUTEN                          (0x1<<1)
#define TFA98XX_PDM_CTRL_I2SMOUTEN_POS                             1
#define TFA98XX_PDM_CTRL_I2SMOUTEN_LEN                             1
#define TFA98XX_PDM_CTRL_I2SMOUTEN_MAX                             1
#define TFA98XX_PDM_CTRL_I2SMOUTEN_MSK                           0x2

/*
 * pdm_out_sel_r
 */
#define TFA98XX_PDM_CTRL_PDMORSEL                           (0x3<<2)
#define TFA98XX_PDM_CTRL_PDMORSEL_POS                              2
#define TFA98XX_PDM_CTRL_PDMORSEL_LEN                              2
#define TFA98XX_PDM_CTRL_PDMORSEL_MAX                              3
#define TFA98XX_PDM_CTRL_PDMORSEL_MSK                            0xc

/*
 * pdm_out_sel_l
 */
#define TFA98XX_PDM_CTRL_PDMOLSEL                           (0x3<<4)
#define TFA98XX_PDM_CTRL_PDMOLSEL_POS                              4
#define TFA98XX_PDM_CTRL_PDMOLSEL_LEN                              2
#define TFA98XX_PDM_CTRL_PDMOLSEL_MAX                              3
#define TFA98XX_PDM_CTRL_PDMOLSEL_MSK                           0x30

/*
 * micdat_out_sel
 */
#define TFA98XX_PDM_CTRL_PADSEL                             (0x3<<6)
#define TFA98XX_PDM_CTRL_PADSEL_POS                                6
#define TFA98XX_PDM_CTRL_PADSEL_LEN                                2
#define TFA98XX_PDM_CTRL_PADSEL_MAX                                3
#define TFA98XX_PDM_CTRL_PADSEL_MSK                             0xc0


/*
 * (0x11)-pdm_out_ctrl
 */

/*
 * secure_dly
 */
#define TFA98XX_PDM_OUT_CTRL_PDMOSDEN                       (0x1<<0)
#define TFA98XX_PDM_OUT_CTRL_PDMOSDEN_POS                          0
#define TFA98XX_PDM_OUT_CTRL_PDMOSDEN_LEN                          1
#define TFA98XX_PDM_OUT_CTRL_PDMOSDEN_MAX                          1
#define TFA98XX_PDM_OUT_CTRL_PDMOSDEN_MSK                        0x1

/*
 * d_out_valid_rf_mux
 */
#define TFA98XX_PDM_OUT_CTRL_PDMOSDCF                       (0x1<<1)
#define TFA98XX_PDM_OUT_CTRL_PDMOSDCF_POS                          1
#define TFA98XX_PDM_OUT_CTRL_PDMOSDCF_LEN                          1
#define TFA98XX_PDM_OUT_CTRL_PDMOSDCF_MAX                          1
#define TFA98XX_PDM_OUT_CTRL_PDMOSDCF_MSK                        0x2

/*
 * Speak_As_Mic_en
 */
#define TFA98XX_PDM_OUT_CTRL_SAAMEN                         (0x1<<4)
#define TFA98XX_PDM_OUT_CTRL_SAAMEN_POS                            4
#define TFA98XX_PDM_OUT_CTRL_SAAMEN_LEN                            1
#define TFA98XX_PDM_OUT_CTRL_SAAMEN_MAX                            1
#define TFA98XX_PDM_OUT_CTRL_SAAMEN_MSK                         0x10

/*
 * speak_as_mic_lp_mode
 */
#define TFA98XX_PDM_OUT_CTRL_SAAMLPEN                       (0x1<<5)
#define TFA98XX_PDM_OUT_CTRL_SAAMLPEN_POS                          5
#define TFA98XX_PDM_OUT_CTRL_SAAMLPEN_LEN                          1
#define TFA98XX_PDM_OUT_CTRL_SAAMLPEN_MAX                          1
#define TFA98XX_PDM_OUT_CTRL_SAAMLPEN_MSK                       0x20

/*
 * pdm_out_rate
 */
#define TFA98XX_PDM_OUT_CTRL_PDMOINTEN                      (0x1<<6)
#define TFA98XX_PDM_OUT_CTRL_PDMOINTEN_POS                         6
#define TFA98XX_PDM_OUT_CTRL_PDMOINTEN_LEN                         1
#define TFA98XX_PDM_OUT_CTRL_PDMOINTEN_MAX                         1
#define TFA98XX_PDM_OUT_CTRL_PDMOINTEN_MSK                      0x40


/*
 * (0x12)-pdm_ds4_r
 */

/*
 * ds4_g1_r
 */
#define TFA98XX_PDM_DS4_R_PDMORG1                           (0xf<<0)
#define TFA98XX_PDM_DS4_R_PDMORG1_POS                              0
#define TFA98XX_PDM_DS4_R_PDMORG1_LEN                              4
#define TFA98XX_PDM_DS4_R_PDMORG1_MAX                             15
#define TFA98XX_PDM_DS4_R_PDMORG1_MSK                            0xf

/*
 * ds4_g2_r
 */
#define TFA98XX_PDM_DS4_R_PDMORG2                           (0xf<<4)
#define TFA98XX_PDM_DS4_R_PDMORG2_POS                              4
#define TFA98XX_PDM_DS4_R_PDMORG2_LEN                              4
#define TFA98XX_PDM_DS4_R_PDMORG2_MAX                             15
#define TFA98XX_PDM_DS4_R_PDMORG2_MSK                           0xf0


/*
 * (0x13)-pdm_ds4_l
 */

/*
 * ds4_g1_l
 */
#define TFA98XX_PDM_DS4_L_PDMOLG1                           (0xf<<0)
#define TFA98XX_PDM_DS4_L_PDMOLG1_POS                              0
#define TFA98XX_PDM_DS4_L_PDMOLG1_LEN                              4
#define TFA98XX_PDM_DS4_L_PDMOLG1_MAX                             15
#define TFA98XX_PDM_DS4_L_PDMOLG1_MSK                            0xf

/*
 * ds4_g2_l
 */
#define TFA98XX_PDM_DS4_L_PDMOLG2                           (0xf<<4)
#define TFA98XX_PDM_DS4_L_PDMOLG2_POS                              4
#define TFA98XX_PDM_DS4_L_PDMOLG2_LEN                              4
#define TFA98XX_PDM_DS4_L_PDMOLG2_MAX                             15
#define TFA98XX_PDM_DS4_L_PDMOLG2_MSK                           0xf0


/*
 * (0x22)-ctrl_saam_pga
 */

/*
 * Ctrl_saam_pga_gain
 */
#define TFA98XX_CTRL_SAAM_PGA_SAAMGAIN                      (0x7<<0)
#define TFA98XX_CTRL_SAAM_PGA_SAAMGAIN_POS                         0
#define TFA98XX_CTRL_SAAM_PGA_SAAMGAIN_LEN                         3
#define TFA98XX_CTRL_SAAM_PGA_SAAMGAIN_MAX                         7
#define TFA98XX_CTRL_SAAM_PGA_SAAMGAIN_MSK                       0x7

/*
 * ctrl_saam_pga_src
 */
#define TFA98XX_CTRL_SAAM_PGA_SAAMPGACTRL                   (0x1<<5)
#define TFA98XX_CTRL_SAAM_PGA_SAAMPGACTRL_POS                      5
#define TFA98XX_CTRL_SAAM_PGA_SAAMPGACTRL_LEN                      1
#define TFA98XX_CTRL_SAAM_PGA_SAAMPGACTRL_MAX                      1
#define TFA98XX_CTRL_SAAM_PGA_SAAMPGACTRL_MSK                   0x20


/*
 * (0x25)-misc_ctrl
 */

/*
 * pll_fcco
 */
#define TFA98XX_MISC_CTRL_PLLCCOSEL                         (0x1<<0)
#define TFA98XX_MISC_CTRL_PLLCCOSEL_POS                            0
#define TFA98XX_MISC_CTRL_PLLCCOSEL_LEN                            1
#define TFA98XX_MISC_CTRL_PLLCCOSEL_MAX                            1
#define TFA98XX_MISC_CTRL_PLLCCOSEL_MSK                          0x1


/*
 * (0x46)-CurrentSense1
 */

/*
 * bypass_gc
 */
#define TFA98XX_CURRENTSENSE1_CSBYPGC                       (0x1<<0)
#define TFA98XX_CURRENTSENSE1_CSBYPGC_POS                          0
#define TFA98XX_CURRENTSENSE1_CSBYPGC_LEN                          1
#define TFA98XX_CURRENTSENSE1_CSBYPGC_MAX                          1
#define TFA98XX_CURRENTSENSE1_CSBYPGC_MSK                        0x1


/*
 * (0x49)-CurrentSense4
 */

/*
 * ctrl_bypassclip
 */
#define TFA98XX_CURRENTSENSE4_CLIP                          (0x1<<0)
#define TFA98XX_CURRENTSENSE4_CLIP_POS                             0
#define TFA98XX_CURRENTSENSE4_CLIP_LEN                             1
#define TFA98XX_CURRENTSENSE4_CLIP_MAX                             1
#define TFA98XX_CURRENTSENSE4_CLIP_MSK                           0x1

/*
 * ctrl_bypassclip2
 */
#define TFA98XX_CURRENTSENSE4_CLIP2                         (0x1<<1)
#define TFA98XX_CURRENTSENSE4_CLIP2_POS                            1
#define TFA98XX_CURRENTSENSE4_CLIP2_LEN                            1
#define TFA98XX_CURRENTSENSE4_CLIP2_MAX                            1
#define TFA98XX_CURRENTSENSE4_CLIP2_MSK                          0x2


/*
 * (0x62)-Hidden_mtp_ctrl_reg3
 */


/*
 * (0x70)-cf_controls
 */

/*
 * cf_rst_dsp
 */
#define TFA98XX_CF_CONTROLS_RST                             (0x1<<0)
#define TFA98XX_CF_CONTROLS_RST_POS                                0
#define TFA98XX_CF_CONTROLS_RST_LEN                                1
#define TFA98XX_CF_CONTROLS_RST_MAX                                1
#define TFA98XX_CF_CONTROLS_RST_MSK                              0x1

/*
 * cf_dmem
 */
#define TFA98XX_CF_CONTROLS_DMEM                            (0x3<<1)
#define TFA98XX_CF_CONTROLS_DMEM_POS                               1
#define TFA98XX_CF_CONTROLS_DMEM_LEN                               2
#define TFA98XX_CF_CONTROLS_DMEM_MAX                               3
#define TFA98XX_CF_CONTROLS_DMEM_MSK                             0x6

/*
 * cf_aif
 */
#define TFA98XX_CF_CONTROLS_AIF                             (0x1<<3)
#define TFA98XX_CF_CONTROLS_AIF_POS                                3
#define TFA98XX_CF_CONTROLS_AIF_LEN                                1
#define TFA98XX_CF_CONTROLS_AIF_MAX                                1
#define TFA98XX_CF_CONTROLS_AIF_MSK                              0x8

/*
 * cf_int
 */
#define TFA98XX_CF_CONTROLS_CFINT                           (0x1<<4)
#define TFA98XX_CF_CONTROLS_CFINT_POS                              4
#define TFA98XX_CF_CONTROLS_CFINT_LEN                              1
#define TFA98XX_CF_CONTROLS_CFINT_MAX                              1
#define TFA98XX_CF_CONTROLS_CFINT_MSK                           0x10

/*
 * cf_req
 */
#define TFA98XX_CF_CONTROLS_REQ                            (0xff<<8)
#define TFA98XX_CF_CONTROLS_REQ_POS                                8
#define TFA98XX_CF_CONTROLS_REQ_LEN                                8
#define TFA98XX_CF_CONTROLS_REQ_MAX                              255
#define TFA98XX_CF_CONTROLS_REQ_MSK                           0xff00


/*
 * (0x71)-cf_mad
 */

/*
 * cf_madd
 */
#define TFA9891_CF_MAD_MADD                              (0xffff<<0)
#define TFA9891_CF_MAD_MADD_POS                                    0
#define TFA9891_CF_MAD_MADD_LEN                                   16
#define TFA9891_CF_MAD_MADD_MAX                                65535
#define TFA9891_CF_MAD_MADD_MSK                               0xffff


/*
 * (0x72)-cf_mem
 */

/*
 * cf_mema
 */
#define TFA9891_CF_MEM_MEMA                              (0xffff<<0)
#define TFA9891_CF_MEM_MEMA_POS                                    0
#define TFA9891_CF_MEM_MEMA_LEN                                   16
#define TFA9891_CF_MEM_MEMA_MAX                                65535
#define TFA9891_CF_MEM_MEMA_MSK                               0xffff


/*
 * (0x73)-cf_status
 */

/*
 * cf_err
 */
#define TFA9891_CF_STATUS_ERR                              (0xff<<0)
#define TFA9891_CF_STATUS_ERR_POS                                  0
#define TFA9891_CF_STATUS_ERR_LEN                                  8
#define TFA9891_CF_STATUS_ERR_MAX                                255
#define TFA9891_CF_STATUS_ERR_MSK                               0xff

/*
 * cf_ack
 */
#define TFA9891_CF_STATUS_ACK                              (0xff<<8)
#define TFA9891_CF_STATUS_ACK_POS                                  8
#define TFA9891_CF_STATUS_ACK_LEN                                  8
#define TFA9891_CF_STATUS_ACK_MAX                                255
#define TFA9891_CF_STATUS_ACK_MSK                             0xff00


/*
 * (0x80)-Key2Protected_spkr_cal_mtp
 */

/*
 * calibration_onetime
 */
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP_MTPOTC           (0x1<<0)
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP_MTPOTC_POS              0
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP_MTPOTC_LEN              1
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP_MTPOTC_MAX              1
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP_MTPOTC_MSK            0x1

/*
 * calibr_ron_done
 */
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP_MTPEX            (0x1<<1)
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP_MTPEX_POS               1
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP_MTPEX_LEN               1
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP_MTPEX_MAX               1
#define TFA98XX_KEY2PROTECTED_SPKR_CAL_MTP_MTPEX_MSK             0x2

#endif /* TFA9891_GENREGS_H */
