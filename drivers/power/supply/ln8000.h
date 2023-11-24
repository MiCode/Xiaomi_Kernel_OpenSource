/*
 * ln8000.h - LN8000 register map descriptions.
 *
 * Copyright (C) 2022 Cirrus Logic Incorporated - https://www.cirrus.com/
 *
 * Author: Sungdae Choi <sungdae.choi@cirrus.com>
 *
 */

#ifndef LN8000_H
#define LN8000_H

#define LN8000_REG_DEVICE_ID                    0x00
#define LN8000_DEVICE_ID                        0x42

#define LN8000_REG_INT1                         0x01

#define LN8000_REG_INT1_MSK                     0x02
#define LN8000_FAULT_INT                        7
#define LN8000_NTC_PROT_INT                     6
#define LN8000_CHARGE_PHASE_INT                 5
#define LN8000_MODE_INT                         4
#define LN8000_REV_CURR_INT                     3
#define LN8000_TEMP_INT                         2
#define LN8000_ADC_DONE_INT                     1
#define LN8000_TIMER_INT                        0


#define LN8000_REG_SYS_STS                      0x03
#define LN8000_IIN_LOOP_STS                     7
#define LN8000_VFLOAT_LOOP_STS                  6
#define LN8000_1TO1_STS                         5
#define LN8000_SYSLDO_ENABLED                   4
#define LN8000_BYPASS_ENABLED                   3
#define LN8000_SWITCHING_ENABLED                2
#define LN8000_STANDBY_STS                      1
#define LN8000_SHUTDOWN_STS                     0
/*N17 code for HQ-306752 by xm tianye9 at 2023/07/08 start*/
#define LN8000_REG_SYS_MASK						0x0C
/*N17 code for HQ-306752 by xm tianye9 at 2023/07/08 end*/


#define LN8000_REG_SAFETY_STS                   0x04
#define LN8000_TEMP_FAULT_DETECTED              7
#define LN8000_TEMP_MAX_STS                     6
#define LN8000_TEMP_REGULATION_STS              5
#define LN8000_NTC_ALARM_STS                    4
#define LN8000_NTC_SHUTDOWN_STS                 3
#define LN8000_REV_IIN_STS                      2
#define LN8000_REV_IIN_LATCHED                  1
#define LN8000_ADC_DONE_STS                     0

#define LN8000_REG_FAULT1_STS                   0x05
#define LN8000_WATCHDOG_TIMER_STS               7
#define LN8000_VBAT_OV_STS                      6
#define LN8000_V_SHORT_STS                      5
#define LN8000_VAC_UNPLUG_STS                   4
#define LN8000_VAC_OV_STS                       3
#define LN8000_VAC_UV_STS                       2
#define LN8000_VIN_OV_STS                       1
#define LN8000_VIN_UV_TRACK_STS                 0

#define LN8000_REG_FAULT2_STS                   0x06
#define LN8000_IIN_OC_DETECTED                  7
#define LN8000_CFLY_SHORT_DETECTED              6
#define LN8000_VOLT_FAULT_DETECTED              5
#define LN8000_VBAT_OV_LATCHED                  4
#define LN8000_VAC_OV_LATCHED                   3
#define LN8000_VAC_UV_LATCHED                   2
#define LN8000_VIN_OV_LATCHED                   1
#define LN8000_VIN_UV_LATCHED                   0

#define LN8000_REG_CURR1_STS                    0x07

#define LN8000_REG_LDO_STS                      0x08

#define LN8000_REG_ADC01_STS                    0x09

#define LN8000_REG_ADC02_STS                    0x0A

#define LN8000_REG_ADC03_STS                    0x0B

#define LN8000_REG_ADC04_STS                    0x0C

#define LN8000_REG_ADC05_STS                    0x0D

#define LN8000_REG_ADC06_STS                    0x0E

#define LN8000_REG_ADC07_STS                    0x0F

#define LN8000_REG_ADC08_STS                    0x10

#define LN8000_REG_ADC09_STS                    0x11

#define LN8000_REG_ADC10_STS                    0x12

#define LN8000_REG_IIN_CTRL                     0x1B
#define LN8000_IIN_CFG_PROG_MASK                GENMASK(6,0)

#define LN8000_REG_REGULATION_CTRL              0x1C
#define LN8000_ENABLE_VFLOAT_LOOP_INT           7
#define LN8000_ENABLE_IIN_LOOP_INT              6
#define LN8000_DISABLE_VFLOAT_LOOP              5
#define LN8000_DISABLE_IIN_LOOP                 4
#define LN8000_TEMP_MAX_EN                      2
#define LN8000_TEMP_REG_EN                      1

#define LN8000_REG_PWR_CTRL                     0x1D

#define LN8000_REG_SYS_CTRL                     0x1E
#define LN8000_STANDBY_EN                       3
#define LN8000_REV_IIN_DET                      2
#define LN8000_SOFT_START_EN                    1
#define LN8000_EN_1TO1                          0

#define LN8000_REG_LDO_CTRL                     0x1F

#define LN8000_REG_GLITCH_CTRL                  0x20
#define LN8000_VAC_OV_CFG_MASK                  GENMASK(3,2)
#define LN8000_VAC_OV_CFG_SHIFT                 2
#define LN8000_VAC_OVP_6P5V                     0x0
#define LN8000_VAC_OVP_11V                      0x1
#define LN8000_VAC_OVP_12V                      0x2
#define LN8000_VAC_OVP_13V                      0x3


#define LN8000_REG_FAULT_CTRL                   0x21
#define LN8000_CFLY_SHORT_CHECK_EN              7
#define LN8000_DISABLE_IIN_OCP                  6
#define LN8000_DISABLE_VBAT_OV                  5
#define LN8000_DISABLE_VAC_OV                   4
#define LN8000_DISABLE_VAC_UV                   3
#define LN8000_DISABLE_VIN_OV                   2
#define LN8000_DISABLE_VIN_OV_TRACK             1
#define LN8000_DISABLE_VIN_UV_TRACK             0

#define LN8000_REG_NTC_CTRL                     0x22

#define LN8000_REG_ADC_CTRL                     0x23
/* used FORCE_ADC_MODE + ADC_SHUTDOWN_CFG */
#define LN8000_ADC_MODE_MASK                    GENMASK(7,5)
#define LN8000_ADC_MODE_SHIFT                   5
#define LN8000_ADC_AUTO_SHD_MODE                0x0
#define LN8000_ADC_AUTO_HIB_MODE                0x1
#define LN8000_ADC_SHUTDOWN_MODE                0x2
#define LN8000_ADC_HIBERNATE_MODE               0x4
#define LN8000_ADC_NORMAL_MODE                  0x6
#define LN8000_ADC_HIBERNATE_DELAY_MASK         GENMASK(4,3)
#define LN8000_ADC_HIBERNATE_DELAY_SHIFT        3
#define LN8000_ADC_HIBERNATE_500MS              0x0
#define LN8000_ADC_HIBERNATE_1S                 0x1
#define LN8000_ADC_HIBERNATE_2S                 0x2
#define LN8000_ADC_HIBERNATE_4S                 0x3

#define LN8000_REG_ADC_CFG                      0x24

#define LN8000_REG_RECOVERY_CTRL                0x25

#define LN8000_REG_TIMER_CTRL                   0x26
#define LN8000_WATCHDOG_EN                      7
#define LN8000_WATCHDOG_CFG_MASK                GENMASK(6,5)
#define LN8000_WATCHDOG_CFG_SHIFT               5
#define LN8000_WATCHDOG_5SEC                    0x0
#define LN8000_WATCHDOG_10SEC                   0x1
#define LN8000_WATCHDOG_20SEC                   0x2
#define LN8000_WATCHDOG_40SEC                   0x3
#define LN8000_AUTO_CLEAR_LATCHED_STS           3
#define LN8000_CLEAR_LATCHED_STS                2
#define LN8000_PAUSE_ADC_UPDATES                1
#define LN8000_PAUSE_INT_UPDATES                0

#define LN8000_REG_THRESHOLD_CTRL               0x27

#define LN8000_REG_V_FLOAT_CTRL                 0x28

#define LN8000_REG_CHARGE_CTRL                  0x29

#define LN8000_REG_LION_CTRL                    0x30

#define LN8000_REG_PRODUCT_ID                   0x31
#define LN8000_OVPFETDR_HIGH_IMP                5

#define LN8000_REG_BC_OP_1                      0x41
#define LN8000_DUAL_FUNCTION_MASK               GENMASK(2,0)
#define LN8000_DUAL_FUNCTION_EN                 2
#define LN8000_DUAL_CFG                         1
#define LN8000_DUAL_LOCKOUT_EN                  0

#define LN8000_REG_BC_OP_2                      0x42
#define LN8000_SOFT_RESET_REQ                   0

#define LN8000_REG_BC_STS_A                     0x49

#define LN8000_REG_BC_STS_B                     0x4A

#define LN8000_REG_BC_STS_C                     0x4B

#define LN8000_REG_BC_STS_D                     0x4C

#define LN8000_REG_BC_STS_E                     0x4D

#define LN8000_REG_MAX                          0x4E

/* electrical numeric calculation unit description */
#define LN8000_ADC_VOUT_STEP_uV                 5000      /* 5mV= 5000uV LSB (0V ~ 5.115V) */
#define LN8000_ADC_VIN_STEP_uV                  16000     /* 16mV=16000uV LSB (0V ~ 16.386V) */
#define LN8000_ADC_VBAT_STEP_uV                 5000      /* 5mV= 5000uV LSB (0V ~ 5.115V) */
#define LN8000_ADC_VBAT_MIN_uV                  1000000   /* 1V */
#define LN8000_ADC_VAC_STEP_uV                  16000     /* 16mV=16000uV LSB (0V ~ 16.386V) */
#define LN8000_ADC_VAC_OS                       5
#define LN8000_ADC_IIN_STEP_uA                  4890      /* 4.89mA=4890uA LSB (0A ~ 5A) */
#define LN8000_ADC_DIETEMP_MIN                  (-250)    /* -25C = -250dC */
#define LN8000_ADC_DIETEMP_MAX                  1600      /* 160C = 1600dC */
#define LN8000_ADC_NTCV_STEP                    2933      /* 2.933mV=2933uV LSB	(0V ~ 3V) */
#define LN8000_NTC_ALARM_CFG_DEFAULT            226       /* NTC alarm threshold (~40C) */
#define LN8000_NTC_SHUTDOWN_CFG                 2         /* NTC shutdown config (-16LSB ~ 4.3C) */
/* VAC_OV_CFG = 6.5V(00b), 11V(01b), 12V(10b), 13V(11b) */
#define LN8000_VAC_OVP_MIN_uV                   6500000
#define LN8000_VAC_OVP_DEF_uV                   (LN8000_VAC_OVP_MIN_uV)
#define LN8000_VAC_OVP_MAX_uV                   13000000
/* IIN_CFG = 0.0A ~ 6.35A (IBUS_OCP = IIN_CFG + 0.7A) */
#define LN8000_IIN_CFG_LSB_uA                   50000

/* N17 code for HQ-321403 by p-xuyechen at 2023/08/25 */
#define LN8000_OCP_OFFSET_uA                    300000
#define LN8000_BUS_OCP_MAX_uA                   7050000
#define LN8000_BUS_OCP_DEF_uA                   3000000
#define LN8000_BUS_OCP_MIN_uA                   700000
/* VFLOAT = 3.725V ~ 5.0V (VBAT_OVP = VFLOAT x 1.02) */
#define LN8000_VFLOAT_MIN_uV                    3725000   /* unit = uV */
#define LN8000_VFLOAT_MAX_uV                    5000000
#define LN8000_VFLOAT_LSB_uV                    5000
#define LN8000_VFLOAT_BG_OFFSET_uV              30000
#define LN8000_BAT_OVP_MAX_uV                   5100000
#define LN8000_BAT_OVP_DEF_uV                   4550000
#define LN8000_BAT_OVP_MIN_uV                   3800000

/*N17 code for HQ-306752 by xm tianye9 at 2023/07/08 start*/
#define LN8000_FORWARD_2_1_CHARGER_MODE			4
#define LN8000_FORWARD_1_1_CHARGER_MODE			8
#define LN8000_SET_1T1_CHG_MODE					1
#define LN8000_SET_2T1_CHG_MODE					0
/*N17 code for HQ-306752 by xm tianye9 at 2023/07/08 end*/


#endif  /* LN8000_H */

