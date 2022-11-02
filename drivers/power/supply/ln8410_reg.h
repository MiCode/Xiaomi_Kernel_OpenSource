#ifndef __LN8410_HEADER__
#define __LN8410_HEADER__
/* Register 00h */
#define LN8410_REG_00                       0x00
/* Register 01h */
#define LN8410_REG_01                       0x01
#define LN8410_BAT_OVP_DIS_MASK             0x80
#define LN8410_BAT_OVP_DIS_SHIFT            7
#define LN8410_BAT_OVP_ENABLE               0
#define LN8410_BAT_OVP_DISABLE              1
#define LN8410_BAT_OVP_DG_SET_MASK          0x40
#define LN8410_BAT_OVP_DG_SET_SHIFT         6
#define LN8410_BAT_OVP_NO_DEGLITCH          0
#define LN8410_BAT_OVP_80_US                1
#define LN8410_BAT_OVP_MASK_MASK            0x20
#define LN8410_BAT_OVP_MASK_SHIFT           5
#define LN8410_BAT_OVP_NOT_MASK             0
#define LN8410_BAT_OVP_IS_MASK              1
#define LN8410_BAT_OVP_MASK                 0x1f
#define LN8410_BAT_OVP_SHIFT                0
#define LN8410_BAT_OVP_BASE                 4450
#define LN8410_BAT_OVP_LSB                  25
/* Register 02h */
#define LN8410_REG_02                       0x02
#define	LN8410_BAT_OCP_DIS_MASK             0x80
#define	LN8410_BAT_OCP_DIS_SHIFT            7
#define LN8410_BAT_OCP_ENABLE               0
#define LN8410_BAT_OCP_DISABLE              1
#define LN8410_BAT_OCP_MASK_MASK            0x20
#define LN8410_BAT_OCP_MASK_SHIFT           5
#define LN8410_BAT_OCP_NOT_MASK             0
#define LN8410_BAT_OCP_IS_MASK              1
//#define LN8410_BAT_OCP_FLAG_MASK            0x10
//#define LN8410_BAT_OCP_FLAG_SHIFT           4
#define LN8410_BAT_OCP_MASK                 0x0F
#define LN8410_BAT_OCP_SHIFT                0
#define LN8410_BAT_OCP_BASE                 6000
#define LN8410_BAT_OCP_LSB                  500
/* Register 03h */
#define LN8410_REG_03                       0x03
#define LN8410_OVPGATE_ON_DG_MASK           0x80
#define LN8410_OVPGATE_ON_DG_SHIFT          7
#define LN8410_OVPGATE_ON_DG_20MS           0
#define LN8410_OVPGATE_ON_DG_120MS          1
#define LN8410_USB_OVP_MASK_MASK            0x40
#define LN8410_USB_OVP_MASK_SHIFT           6
#define LN8410_USB_OVP_NOT_MASK             0
#define LN8410_USB_OVP_IS_MASK                 1
//#define LN8410_USB_OVP_FLAG_MASK            0x20
//#define LN8410_USB_OVP_FLAG_SHIFT           5
//#define LN8410_USB_OVP_STAT_MASK            0x10
//#define LN8410_USB_OVP_STAT_SHIFT           4
#define LN8410_USB_OVP_MASK                 0x0F
#define LN8410_USB_OVP_SHIFT                0
#define LN8410_USB_OVP_BASE                 11000
#define LN8410_USB_OVP_LSB                  1000
#define LN8410_USB_OVP_6PV5                 0x0F
/* Register 04h */
#define LN8410_REG_04                       0x04
#define LN8410_WPCGATE_ON_DG_MASK           0x80
#define LN8410_WPCGATE_ON_DG_SHIFT          7
#define LN8410_WPCGATE_ON_DG_20MS           0
#define LN8410_WPCGATE_ON_DG_120MS          1
#define LN8410_WPC_OVP_MASK_MASK            0x40
#define LN8410_WPC_OVP_MASK_SHIFT           6
#define LN8410_WPC_OVP_NOT_MASK             0
#define LN8410_WPC_OVP_IS_MASK                 1
//#define LN8410_WPC_OVP_FLAG_MASK            0x20
//#define LN8410_WPC_OVP_FLAG_SHIFT           5
//#define LN8410_WPC_OVP_STAT_MASK            0x10
//#define LN8410_WPC_OVP_STAT_SHIFT           4
#define LN8410_WPC_OVP_MASK                 0x0F
#define LN8410_WPC_OVP_SHIFT                0
#define LN8410_WPC_OVP_BASE                 11000
#define LN8410_WPC_OVP_LSB                  1000
#define LN8410_WPC_OVP_6PV5                 0x0F
/* Register 05h */
#define LN8410_REG_05                       0x05
#define LN8410_BUS_OCP_DIS_MASK             0x80
#define LN8410_BUS_OCP_DIS_SHIFT            7
#define LN8410_BUS_OCP_ENABLE               0
#define LN8410_BUS_OCP_DISABLE              1
#define LN8410_BUS_OCP_MASK_MASK            0x20
#define LN8410_BUS_OCP_MASK_SHIFT           5
#define LN8410_BUS_OCP_NOT_MASK             0
#define LN8410_BUS_OCP_IS_MASK                 1
#define LN8410_BUS_OCP_MASK                 0x0F
#define LN8410_BUS_OCP_SHIFT                0
#define LN8410_BUS_OCP_BASE                 2500
#define LN8410_BUS_OCP_LSB                  250
/* Register 06h */
#define LN8410_REG_06                       0x06
#define LN8410_BUS_UCP_DIS_MASK             0x80
#define LN8410_BUS_UCP_DIS_SHIFT            7
#define LN8410_BUS_UCP_ENABLE               0
#define LN8410_BUS_UCP_DISABLE              1
#define LN8410_BUS_UCP_FALL_DG_MASK         0x30
#define LN8410_BUS_UCP_FALL_DG_SHIFT        4
#define LN8410_BUS_UCP_FALL_DG_8US          0
#define LN8410_BUS_UCP_FALL_DG_5MS          1
#define LN8410_BUS_UCP_FALL_DG_20MS         2
#define LN8410_BUS_UCP_FALL_DG_50MS         3
#define LN8410_BUS_UCP_RISE_MASK_MASK       0x08
#define LN8410_BUS_UCP_RISE_MASK_SHIFT      3
#define LN8410_BUS_UCP_RISE_NOT_MASK        0
#define LN8410_BUS_UCP_RISE_MASK            1
//#define LN8410_BUS_UCP_RISE_FLAG_MASK       0x04
//#define LN8410_BUS_UCP_RISE_FLAG_SHIFT      2
#define LN8410_BUS_UCP_FALL_MASK_MASK       0x02
#define LN8410_BUS_UCP_FALL_MASK_SHIFT      1
#define LN8410_BUS_UCP_FALL_NOT_MASK        0
#define LN8410_BUS_UCP_FALL_MASK            1
//#define LN8410_BUS_UCP_FALL_FLAG_MASK       0x01
//#define LN8410_BUS_UCP_FALL_FLAG_SHIFT      0
/* Register 05h */
/*
#define LN8410_REG_05                       0x05
#define LN8410_BUS_OVP_MASK                 0xFC
#define LN8410_BUS_OVP_SHIFT                2
#define LN8410_BUS_OVP_41MODE_BASE          14000
#define LN8410_BUS_OVP_41MODE_LSB           200
#define LN8410_BUS_OVP_21MODE_BASE          7000
#define LN8410_BUS_OVP_21MODE_LSB           100
#define LN8410_BUS_OVP_11MODE_BASE          3500
#define LN8410_BUS_OVP_11MODE_LSB           50
#define LN8410_OUT_OVP_MASK                 0x03
#define LN8410_OUT_OVP_SHIFT                0
#define LN8410_OUT_OVP_BASE                 4800
#define LN8410_OUT_OVP_LSB                  200
*/
/* Register 07h */
#define LN8410_REG_07                       0x07
#define LN8410_PMID2OUT_OVP_DIS_MASK        0x80
#define LN8410_PMID2OUT_OVP_DIS_SHIFT           7
#define	LN8410_PMID2OUT_OVP_ENABLE          0
#define	LN8410_PMID2OUT_OVP_DISABLE         1
#define LN8410_PMID2OUT_OVP_MASK_MASK       0x10
#define LN8410_PMID2OUT_OVP_MASK_SHIFT      4
#define LN8410_PMID2OUT_OVP_NOT_MASK        0
#define LN8410_PMID2OUT_OVP_IS_MASK            1
//#define LN8410_PMID2OUT_OVP_FLAG_MASK       0x08
//#define LN8410_PMID2OUT_OVP_FLAG_SHIFT      3
#define LN8410_PMID2OUT_OVP_MASK            0x07
#define LN8410_PMID2OUT_OVP_SHIFT           0
#define LN8410_PMID2OUT_OVP_BASE            200
#define LN8410_PMID2OUT_OVP_LSB             50
/* Register 08h */
#define LN8410_REG_08						0x08
#define LN8410_PMID2OUT_UVP_DIS_MASK        0x80
#define LN8410_PMID2OUT_UVP_DIS_SHIFT           7
#define	LN8410_PMID2OUT_UVP_ENABLE          0
#define	LN8410_PMID2OUT_UVP_DISABLE         1
#define LN8410_PMID2OUT_UVP_MASK_MASK       0x10
#define LN8410_PMID2OUT_UVP_MASK_SHIFT      4
#define LN8410_PMID2OUT_UVP_NOT_MASK        0
#define LN8410_PMID2OUT_UVP_IS_MASK            1
//#define LN8410_PMID2OUT_UVP_FLAG_MASK       0x08
//#define LN8410_PMID2OUT_UVP_FLAG_SHIFT      3
#define LN8410_PMID2OUT_UVP_MASK            0x07
#define LN8410_PMID2OUT_UVP_SHIFT           0
#define LN8410_PMID2OUT_UVP_BASE            50
#define LN8410_PMID2OUT_UVP_LSB             50
/* Register 09h */
#define LN8410_REG_09                       0x09
#define LN8410_POR_FLAG_MASK                0x80
#define LN8410_POR_FLAG_SHIFT               7
#define LN8410_ACRB_WPC_STAT_MASK           0x40
#define LN8410_ACRB_WPC_STAT_SHIFT          6
#define LN8410_WPCGATE_TIED_TO_GND          0
#define LN8410_WPCGATE_NOT_TIED_TO_GND      1
#define LN8410_ACRB_USB_STAT_MASK           0x20
#define LN8410_ACRB_USB_STAT_SHIFT          5
#define LN8410_OVPGATE_TIED_TO_GND          0
#define LN8410_OVPGATE_NOT_TIED_TO_GND      1
#define LN8410_VBUS_ERRORLO_STAT_MASK       0x10
#define LN8410_VBUS_ERRORLO_STAT_SHIFT      4
#define LN8410_VBUS_ERRORHI_STAT_MASK       0x08
#define LN8410_VBUS_ERRORHI_STAT_SHIFT      3
#define LN8410_QB_ON_STAT_MASK              0x04
#define LN8410_QB_ON_STAT_SHIFT             2
#define LN8410_QB_OFF                       0
#define LN8410_QB_ON                        1
#define LN8410_CP_SWITCHING_STAT_MASK       0x02
#define LN8410_CP_SWITCHING_STAT_SHIFT      1
#define LN8410_CP_NOT_SWITCHING             0
#define LN8410_CP_SWITCHING                 1
#define LN8410_PIN_DIAG_FALL_FLAG_MASK      0x01
#define LN8410_PIN_DIAG_FALL_FLAG_SHIFT     0
/* Register 0Ah */
#define LN8410_REG_0A                       0x0A
#define LN8410_CHG_EN_MASK                  0x80
#define LN8410_CHG_EN_SHIFT                 7
#define LN8410_CHG_ENABLE                   1
#define LN8410_CHG_DISABLE                  0
#define LN8410_QB_EN_MASK                   0x40
#define LN8410_QB_EN_SHIFT                  6
#define LN8410_QB_ENABLE                    1
#define LN8410_QB_DISABLE                   0
#define LN8410_ACDRV_MANUAL_EN_MASK         0x20
#define LN8410_ACDRV_MANUAL_EN_SHIFT        5
#define LN8410_ACDRV_AUTO_MODE              0
#define LN8410_ACDRV_MANUAL_MODE            1
#define LN8410_WPCGATE_EN_MASK              0x10
#define LN8410_WPCGATE_EN_SHIFT             4
#define LN8410_WPCGATE_ENABLE               1
#define LN8410_WPCGATE_DISABLE              0
#define LN8410_OVPGATE_EN_MASK              0x08
#define LN8410_OVPGATE_EN_SHIFT             3
#define LN8410_OVPGATE_ENABLE               1
#define LN8410_OVPGATE_DISABLE              0
#define LN8410_VBUS_PD_EN_MASK              0x04
#define LN8410_VBUS_PD_EN_SHIFT             2
#define LN8410_VBUS_PD_ENABLE               1
#define LN8410_VBUS_PD_DISABLE              0
#define LN8410_VWPC_PD_EN_MASK              0x02
#define LN8410_VWPC_PD_EN_SHIFT             1
#define LN8410_VWPC_PD_ENABLE               1
#define LN8410_VWPC_PD_DISABLE              0
#define LN8410_VUSB_PD_EN_MASK              0x01
#define LN8410_VUSB_PD_EN_SHIFT             0
#define LN8410_VUSB_PD_ENABLE               1
#define LN8410_VUSB_PD_DISABLE              0
/* Register 0Bh */
#define LN8410_REG_0B                       0x0B
#define LN8410_FSW_SET_MASK                 0xF8
#define LN8410_FSW_SET_SHIFT                3
#define LN8410_FSW_SET_580K                 0x13
#define LN8410_FREQ_SHIFT_MASK              0x04
#define LN8410_FREQ_SHIFT_SHIFT             2
#define LN8410_FSW_NORMANL                  0
#define LN8410_FSW_SPREAD                   1
#define LN8410_SYNC_MASK                    0x03
#define LN8410_SYNC_SHIFT                   0
#define LN8410_SYNC_NO_SHIFT_1              0
#define LN8410_SYNC_0DEG                    1
#define LN8410_SYNC_90DEG                   2
#define LN8410_SYNC_NO_SHIFT_2              3
/* Register 0Ch */
#define LN8410_REG_0C                       0x0C
#define LN8410_SS_TIMEOUT_SET_MASK          0x38
#define LN8410_SS_TIMEOUT_SET_SHIFT         3
#define LN8410_SS_TIMEOUT_DISABLE           0
#define LN8410_SS_TIMEOUT_40MS              1
#define LN8410_SS_TIMEOUT_80MS              2
#define LN8410_SS_TIMEOUT_320MS             3
#define LN8410_SS_TIMEOUT_1280MS            4
#define LN8410_SS_TIMEOUT_5120MS            5
#define LN8410_SS_TIMEOUT_20480MS           6
#define LN8410_SS_TIMEOUT_81920MS           7
#define LN8410_WD_TIMEOUT_SET_MASK          0x07
#define LN8410_WD_TIMEOUT_SET_SHIFT         0
#define LN8410_WD_TIMEOUT_DISABLE           0
#define LN8410_WD_TIMEOUT_0P2S              1
#define LN8410_WD_TIMEOUT_0P5S              2
#define LN8410_WD_TIMEOUT_1S                3
#define LN8410_WD_TIMEOUT_5S                4
#define LN8410_WD_TIMEOUT_30S               5
#define LN8410_WD_TIMEOUT_100S              6
#define LN8410_WD_TIMEOUT_255S              7
/* Register 0Dh */
#define LN8410_REG_0D                       0x0D
#define LN8410_SYNC_FUNCTION_EN_MASK        0x80
#define LN8410_SYNC_FUNCTION_EN_SHIFT       7
#define LN8410_SYNC_FUNCTION_DISABLE        0
#define LN8410_SYNC_FUNCTION_ENABLE         1
#define LN8410_SYNC_MASTER_EN_MASK          0x40
#define LN8410_SYNC_MASTER_EN_SHIFT         6
#define LN8410_SYNC_CONFIG_SLAVE            0
#define LN8410_SYNC_CONFIG_MASTER           1
/*
#define LN8410_VBAT_OVP_DG_MASK             0x20
#define LN8410_VBAT_OVP_DG_SHIFT            5
#define LN8410_VBAT_OVP_NO_DG               0
#define LN8410_VBAT_OVP_DG_10US             1
*/
#define LN8410_VBUS_OVP_SET_MASK             0x20
#define LN8410_VBUS_OVP_SET_SHIFT            5
#define LN8410_VBUS_OVP_LOW_SET              0
#define LN8410_VBUS_OVP_HIGH_SET             1
#define LN8410_IBAT_SNS_RES_MASK            0x10
#define LN8410_IBAT_SNS_RES_SHIFT           4
#define LN8410_IBAT_SNS_RES_1MHM            0
#define LN8410_IBAT_SNS_RES_2MHM            1
#define LN8410_REG_RST_MASK                 0x08
#define LN8410_REG_RST_SHIFT                7
#define LN8410_REG_NO_RESET                 0
#define LN8410_REG_RESET                    1
#define LN8410_MODE_MASK                    0x07
#define LN8410_MODE_SHIFT                   0
#define LN8410_FORWARD_4_1_CHARGER_MODE     0
#define LN8410_FORWARD_2_1_CHARGER_MODE     1
#define LN8410_FORWARD_1_1_CHARGER_MODE     2
#define LN8410_FORWARD_1_1_CHARGER_MODE1    3
#define LN8410_REVERSE_1_4_CONVERTER_MODE   4
#define LN8410_REVERSE_1_2_CONVERTER_MODE   5
#define LN8410_REVERSE_1_1_CONVERTER_MODE   6
#define LN8410_REVERSE_1_1_CONVERTER_MODE1  7
/* Register 0Eh */
#define LN8410_REG_0E                       0x0E
#define LN8410_OVPGATE_STAT_MASK            0x80
#define LN8410_OVPFATE_STAT_SHIFT           7
#define LN8410_OVPFATE_OFF                  0
#define LN8410_OVPFATE_ON                   1
#define LN8410_WPCGATE_STAT_MASK            0x40
#define LN8410_WPCFATE_STAT_SHIFT           6
#define LN8410_WPCFATE_OFF                  0
#define LN8410_WPCFATE_ON                   1
/* Register 0Fh */
#define LN8410_REG_0F                       0x0F
#define LN8410_VOUT_OK_REV_STAT_MASK        0x20
#define LN8410_VOUT_OK_REV_STAT_SHIFT       5
#define LN8410_VOUT_OK_CHG_STAT_MASK        0x10
#define LN8410_VOUT_OK_CHG_STAT_SHIFT       4
#define LN8410_VOUT_INSERT_STAT_MASK        0x08
#define LN8410_VOUT_INSERT_STAT_SHIFT       3
#define LN8410_VBUS_PRESENT_STAT_MASK       0x04
#define LN8410_VBUS_PRESENT_STAT_SHIFT      2
#define LN8410_VWPC_INSERT_STAT_MASK        0x02
#define LN8410_VWPC_INSERT_STAT_SHIFT       1
#define LN8410_VUSB_INSERT_STAT_MASK        0x01
#define LN8410_VUSB_INSERT_STAT_SHIFT       0
/* Register 10h */
#define LN8410_REG_10                       0x10
#define LN8410_VOUT_OK_REV_STAT_MASK        0x20
#define LN8410_VOUT_OK_REV_STAT_SHIFT       5
#define LN8410_VOUT_OK_CHG_STAT_MASK        0x10
#define LN8410_VOUT_OK_CHG_STAT_SHIFT       4
#define LN8410_VOUT_INSERT_STAT_MASK        0x08
#define LN8410_VOUT_INSERT_STAT_SHIFT       3
#define LN8410_VBUS_PRESENT_STAT_MASK       0x04
#define LN8410_VBUS_PRESENT_STAT_SHIFT      2
#define LN8410_VWPC_INSERT_STAT_MASK        0x02
#define LN8410_VWPC_INSERT_STAT_SHIFT       1
#define LN8410_VUSB_INSERT_STAT_MASK        0x01
#define LN8410_VUSB_INSERT_STAT_SHIFT       0
/* Register 11h */
#define LN8410_REG_11                       0x11
#define LN8410_VOUT_OK_REV_FLAG_MASK        0x20
#define LN8410_VOUT_OK_REV_FLAG_SHIFT       5
#define LN8410_VOUT_OK_CHG_FLAG_MASK        0x10
#define LN8410_VOUT_OK_CHG_FLAG_SHIFT       4
#define LN8410_VOUT_INSERT_FLAG_MASK        0x08
#define LN8410_VOUT_INSERT_FLAG_SHIFT       3
#define LN8410_VBUS_PRESENT_FLAG_MASK       0x04
#define LN8410_VBUS_PRESENT_FLAG_SHIFT      2
#define LN8410_VWPC_INSERT_FLAG_MASK        0x02
#define LN8410_VWPC_INSERT_FLAG_SHIFT       1
#define LN8410_VUSB_INSERT_FLAG_MASK        0x01
#define LN8410_VUSB_INSERT_FLAG_SHIFT       0
/* Register 12h */
#define LN8410_REG_12                       0x12
#define LN8410_VOUT_OK_REV_MASK_MASK        0x20
#define LN8410_VOUT_OK_REV_MASK_SHIFT       5
#define LN8410_VOUT_OK_REV_NOT_MASK         0
#define LN8410_VOUT_OK_REV_MASK             1
#define LN8410_VOUT_OK_CHG_MASK_MASK        0x10
#define LN8410_VOUT_OK_CHG_MASK_SHIFT       4
#define LN8410_VOUT_OK_CHG_NOT_MASK         0
#define LN8410_VOUT_OK_CHG_MASK             1
#define LN8410_VOUT_INSERT_MASK_MASK        0x08
#define LN8410_VOUT_INSERT_MASK_SHIFT       3
#define LN8410_VOUT_INSERT_NOT_MASK         0
#define LN8410_VOUT_INSERT_MASK             1
#define LN8410_VBUS_PRESENT_MASK_MASK       0x04
#define LN8410_VBUS_PRESENT_MASK_SHIFT      2
#define LN8410_VBUS_PRESENT_NOT_MASK        0
#define LN8410_VBUS_PRESENT_MASK            1
#define LN8410_VWPC_INSERT_MASK_MASK        0x02
#define LN8410_VWPC_INSERT_MASK_SHIFT       1
#define LN8410_VWPC_INSERT_NOT_MASK         0
#define LN8410_VWPC_INSERT_MASK             1
#define LN8410_VUSB_INSERT_MASK_MASK        0x01
#define LN8410_VUSB_INSERT_MASK_SHIFT       0
#define LN8410_VUSB_INSERT_NOT_MASK         0
#define LN8410_VUSB_INSERT_MASK             1
/* Register 13h */
#define LN8410_REG_13                       0x13
#define LN8410_TSBAT_FLT_FLAG_MASK          0x80
#define LN8410_TSBAT_FLT_FLAG_SHIFT         7
#define LN8410_TSHUT_FLAG_MASK              0x40
#define LN8410_TSHUT_FLAG_SHIFT             6
#define LN8410_SS_TIMEOUT_FLAG_MASK         0x20
#define LN8410_SS_TIMEOUT_FLAG_SHIFT        5
#define LN8410_WD_TIMEOUT_FLAG_MASK         0x10
#define LN8410_WD_TIMEOUT_FLAG_SHIFT        4
#define LN8410_CONV_OCP_FLAG_MASK           0x08
#define LN8410_CONV_OCP_FLAG_SHIFT          3
#define LN8410_SS_FAIL_FLAG_MASK            0x04
#define LN8410_SS_FAIL_FLAG_SHIFT           2
#define LN8410_VBUS_OVP_FLAG_MASK           0x02
#define LN8410_VBUS_OVP_FLAG_SHIFT          1
#define LN8410_VOUT_OVP_FLAG_MASK           0x01
#define LN8410_VOUT_OVP_FLAG_SHIFT          0
/* Register 14h */
#define LN8410_REG_14                       0x14
#define LN8410_TSBAT_FLT_MASK_MASK          0x80
#define LN8410_TSBAT_FLT_MASK_SHIFT         7
#define LN8410_TSBAT_FLT_NOT_MASK           0
#define LN8410_TSBAT_FLT_MASK               1
#define LN8410_TSHUT_MASK_MASK              0x40
#define LN8410_TSHUT_MASK_SHIFT             6
#define LN8410_TSHUT_NOT_MASK               0
#define LN8410_TSHUT_MASK                   1
#define LN8410_SS_TIMEOUT_MASK_MASK         0x20
#define LN8410_SS_TIMEOUT_MASK_SHIFT        5
#define LN8410_SS_TIMEOUT_NOT_MASK          0
#define LN8410_SS_TIMEOUT_MASK              1
#define LN8410_WD_TIMEOUT_MASK_MASK         0x10
#define LN8410_WD_TIMEOUT_MASK_SHIFT        4
#define LN8410_WD_TIMEOUT_NOT_MASK          0
#define LN8410_WD_TIMEOUT_MASK              1
#define LN8410_CONV_OCP_MASK_MASK           0x08
#define LN8410_CONV_OCP_MASK_SHIFT          3
#define LN8410_CONV_OCP_NOT_MASK            0
#define LN8410_CONV_OCP_MASK                1
#define LN8410_SS_FAIL_MASK_MASK            0x04
#define LN8410_SS_FAIL_MASK_SHIFT           2
#define LN8410_SS_FAIL_NOT_MASK             0
#define LN8410_SS_FAIL_MASK                 1
#define LN8410_VBUS_OVP_MASK_MASK           0x02
#define LN8410_VBUS_OVP_MASK_SHIFT          1
#define LN8410_VBUS_OVP_NOT_MASK            0
#define LN8410_VBUS_OVP_MASK                1
#define LN8410_VOUT_OVP_MASK_MASK           0x01
#define LN8410_VOUT_OVP_MASK_SHIFT          0
#define LN8410_VOUT_OVP_NOT_MASK            0
#define LN8410_VOUT_OVP_MASK                1
/* Register 14h */
#define LN8410_REG_14                       0x14
#define LN8410_ADC_EN_MASK                  0x80
#define LN8410_ADC_EN_SHIFT                 7
#define LN8410_ADC_DISABLE                  0
#define LN8410_ADC_ENABLE                   1
#define LN8410_ADC_RATE_MASK                0x40
#define LN8410_ADC_RATE_SHIFT               6
#define LN8410_ADC_RATE_CONTINOUS           0
#define LN8410_ADC_RATE_ONESHOT             1
//#define LN8410_ADC_DONE_STAT_MASK           0x20
//#define LN8410_ADC_DONE_STAT_SHIFT          5
//#define LN8410_ADC_DONE_FLAG_MASK           0x10
//#define LN8410_ADC_DONE_FALG_SHIFT          4
#define LN8410_ADC_DONE_MASK_MASK           0x08
#define LN8410_ADC_DONE_MASK_SHIFT          3
#define LN8410_ADC_DONE_NOT_MASK            0
#define LN8410_ADC_DONE_MASK                1
#define LN8410_IBUS_ADC_DIS_MASK            0x01
#define LN8410_IBUS_ADC_DIS_SHIFT           0
#define LN8410_IBUS_ADC_ENABLE              0
#define LN8410_IBUS_ADC_DISABLE             1
/* Register 15h */
#define LN8410_REG_15                       0x15
#define LN8410_VBUS_ADC_DIS_MASK            0x80
#define LN8410_VBUS_ADC_DIS_SHIFT           7
#define LN8410_VBUS_ADC_ENABLE              0
#define LN8410_VBUS_ADC_DISABLE             1
#define LN8410_VUSB_ADC_DIS_MASK            0x40
#define LN8410_VUSB_ADC_DIS_SHIFT           6
#define LN8410_VUSB_ADC_ENABLE              0
#define LN8410_VUSB_ADC_DISABLE             1
#define LN8410_VWPC_ADC_DIS_MASK            0x20
#define LN8410_VWPC_ADC_DIS_SHIFT           5
#define LN8410_VWPC_ADC_ENABLE              0
#define LN8410_VWPC_ADC_DISABLE             1
#define LN8410_VOUT_ADC_DIS_MASK            0x10
#define LN8410_VOUT_ADC_DIS_SHIFT           4
#define LN8410_VOUT_ADC_ENABLE              0
#define LN8410_VOUT_ADC_DISABLE             1
#define LN8410_VBAT_ADC_DIS_MASK            0x08
#define LN8410_VBAT_ADC_DIS_SHIFT           3
#define LN8410_VBAT_ADC_ENABLE              0
#define LN8410_VBAT_ADC_DISABLE             1
#define LN8410_IBAT_ADC_DIS_MASK            0x04
#define LN8410_IBAT_ADC_DIS_SHIFT           2
#define LN8410_IBAT_ADC_ENABLE              0
#define LN8410_IBAT_ADC_DISABLE             1
#define LN8410_TSBAT_ADC_DIS_MASK           0x02
#define LN8410_TSBAT_ADC_DIS_SHIFT          1
#define LN8410_TSBAT_ADC_ENABLE             0
#define LN8410_TSBAT_ADC_DISABLE            1
#define LN8410_TDIE_ADC_DIS_MASK            0x01
#define LN8410_TDIE_ADC_DIS_SHIFT           0
#define LN8410_TDIE_ADC_ENABLE              0
#define LN8410_TDIE_ADC_DISABLE             1
/* Register 16h */
#define LN8410_REG_16                       0x16
#define LN8410_IBUS_POL_H_MASK              0x0F
#define LN8410_IBUS_ADC_LSB                 15625 / 10000
/* Register 17h */
#define LN8410_REG_17                       0x17
#define LN8410_IBUS_POL_L_MASK              0xFF
/* Register 18h */
#define LN8410_REG_18                       0x18
#define LN8410_VBUS_POL_H_MASK              0x0F
#define LN8410_VBUS_ADC_LSB                 625 / 100
/* Register 19h */
#define LN8410_REG_19                       0x19
#define LN8410_VBUS_POL_L_MASK              0xFF
/* Register 1Ah */
#define LN8410_REG_1A                       0x1A
#define LN8410_VUSB_POL_H_MASK              0x0F
#define LN8410_VUSB_ADC_LSB                 625 / 100
/* Register 1Bh */
#define LN8410_REG_1B                       0x1B
#define LN8410_VUSB_POL_L_MASK              0xFF
/* Register 1Ch */
#define LN8410_REG_1C                       0x1C
#define LN8410_VWPC_POL_H_MASK              0x0F
#define LN8410_VWPC_ADC_LSB                 625 / 100
/* Register 1Dh */
#define LN8410_REG_1D                       0x1D
#define LN8410_VWPC_POL_L_MASK              0xFF
/* Register 1Eh */
#define LN8410_REG_1E                       0x1E
#define LN8410_VOUT_POL_H_MASK              0x0F
#define LN8410_VOUT_ADC_LSB                 125 / 100
/* Register 1Fh */
#define LN8410_REG_1F                       0x1F
#define LN8410_VOUT_POL_L_MASK              0x0F
/* Register 20h */
#define LN8410_REG_20                       0x20
#define LN8410_VBAT_POL_H_MASK              0x0F
#define LN8410_VBAT_ADC_LSB                 125 / 100
/* Register 21h */
#define LN8410_REG_21                       0x21
#define LN8410_VBAT_POL_L_MASK              0xFF
/* Register 22h */
#define LN8410_REG_22                       0x22
#define LN8410_IBAT_POL_H_MASK              0x0F
#define LN8410_IBAT_ADC_LSB                 3125 / 1000
/* Register 23h */
#define LN8410_REG_23                       0x23
#define LN8410_IBAT_POL_L_MASK              0xFF
/* Register 24h */
#define LN8410_REG_24                       0x24
#define LN8410_TSBAT_POL_H_MASK             0x03
#define LN8410_TSBAT_ADC_LSB                9766 / 100000
/* Register 25h */
#define LN8410_REG_25                       0x25
#define LN8410_TSBAT_POL_L_MASK             0xFF
/* Register 26h */
#define LN8410_REG_26                       0x26
#define LN8410_TDIE_POL_H_MASK              0x01
#define LN8410_TDIE_ADC_LSB                 5 / 10
/* Register 27h */
#define LN8410_REG_27                       0x27
#define LN8410_TDIE_POL_L_MASK              0xFF
/* Register 28h */
#define LN8410_REG_28                       0x28
#define LN8410_TSBAT_FLT1_MASK              0xFF
#define LN8410_TSBAT_FLT1_SHIFT             0
#define LN8410_TSBAT_FLT1_BASE              0
#define LN8410_TSBAT_FLT1_LSB               19531 / 100000

/* Register 52h */
#define LN8410_REG_52                       0x52
#define LN8410_USE_HVLDO_MASK         0x10
#define LN8410_USE_HVLDO_SHIFT        4
#define LN8410_NOT_USE_HVLDO           0
#define LN8410_USE_HVLDO           1

/* Register 52h */
#define LN8410_REG_62                       0x62

/* Register 6Ch */
#define LN8410_REG_6C                       0x6C
#define LN8410_BAT_OVP_ALM_DIS_MASK         0x80
#define LN8410_BAT_OVP_ALM_DIS_SHIFT        7
#define LN8410_BAT_OVP_ALM_ENABLE           0
#define LN8410_BAT_OVP_ALM_DISABLE          1
#define LN8410_BAT_OVP_ALM_MASK_MASK        0x40
#define LN8410_BAT_OVP_ALM_MASK_SHIFT       6
#define LN8410_BAT_OVP_ALM_NOT_MASK         0
#define LN8410_BAT_OVP_ALM_IS_MASK          1
#define LN8410_BAT_OVP_ALM_FLAG_MASK        0x20
#define LN8410_BAT_OVP_ALM_FLAG_SHIFT       5
#define LN8410_BAT_OVP_ALM_MASK             0x1F
#define LN8410_BAT_OVP_ALM_SHIFT            0
#define LN8410_BAT_OVP_ALM_BASE             4450
#define LN8410_BAT_OVP_ALM_LSB              25
/* Register 6Dh */
#define LN8410_REG_6D                       0x6D
#define LN8410_BUS_OCP_ALM_DIS_MASK         0x80
#define LN8410_BUS_OCP_ALM_DIS_SHIFT        7
#define LN8410_BUS_OCP_ALM_ENABLE           0
#define LN8410_BUS_OCP_ALM_DISABLE          1
#define LN8410_BUS_OCP_ALM_MASK_MASK        0x40
#define LN8410_BUS_OCP_ALM_MASK_SHIFT       6
#define LN8410_BUS_OCP_ALM_NOT_MASK         0
#define LN8410_BUS_OCP_ALM_IS_MASK          1
#define LN8410_BUS_OCP_ALM_FLAG_MASK        0x20
#define LN8410_BUS_OCP_ALM_FLAG_SHIFT       5
#define LN8410_BUS_OCP_ALM_MASK             0x1F
#define LN8410_BUS_OCP_ALM_SHIFT            0
#define LN8410_BUS_OCP_ALM_BASE             2500
#define LN8410_BUS_OCP_ALM_LSB              125
/* Register 6Eh */
#define LN8410_REG_6E                       0x6E
#define LN8410_DEVICE_ID                    0xCA
#define	LN_VOUT_INSERT                 BIT(3)
#define	LN_VBUS_INSERT                 BIT(2)
#define LN_VWPC_INSERT                 BIT(1)
#define LN_VUSB_INSERT                 BIT(0)
/* Register 70h */
#define LN8410_REG_70              0x70
#define LN8410_TSBAT_EN_MASK              0x08
#define LN8410_TSBAT_EN_SHIFT             3
#define LN8410_TSBAT_ENABLE               0
#define LN8410_TSBAT_DISABLE              1
#define	LN_BAT_OVP_FAULT_SHIFT         0
#define	LN_BAT_OCP_FAULT_SHIFT         1
#define	LN_USB_OVP_FAULT_SHIFT         2
#define	LN_WPC_OVP_FAULT_SHIFT         3
#define	LN_BUS_OCP_FAULT_SHIFT         4
#define	LN_BUS_UCP_FAULT_SHIFT         5
#define LN_BAT_OVP_ALARM_SHIFT         0
#define LN_BUS_OCP_ALARM_SHIFT         1
/* Register 76h */
#define LN8410_REG_76                       0x76
#define LN8410_PAUSE_ADC_UPDATES_MASK       0x20
/* Register 79h */
#define LN8410_REG_79                       0x79
#define LN8410_OUT_OVP_MASK                 0xf0
#define LN8410_OUT_OVP_SHIFT                4
#define LN8410_OUT_OVP_BASE                 4500
#define LN8410_OUT_OVP_LSB                  100

/* Register 7Bh */
#define LN8410_REG_7B                       0x7B
#define LN8410_VBUS_SHORT_DIS_MASK                   0x40
#define LN8410_VBUS_SHORT_DIS_SHIFT                  6
#define LN8410_VBUS_SHORT_ENABLE                    0
#define LN8410_VBUS_SHORT_DISABLE                   1

/* Register 7Ch */
#define LN8410_REG_7C                       0x7C
#define LN8410_IBUS_UC_CFG_MASK             0xF0
#define LN8410_IBUS_UC_CFG_SHIFT            4
#define LN8410_IBUS_UC_BASE                 100
#define LN8410_IBUS_UC_LSB                  50
#define LN8410_INFET_OFF_DET_DIS_MASK       0x08
#define LN8410_INFET_OFF_DET_DIS_SHIFT      3
#define LN8410_INFET_OFF_DET_CFG_MASK       0x07
#define LN8410_INFET_OFF_DET_CFG_SHIFT      0
#define LN8410_INFET_OFF_DET_0mv            0
#define LN8410_INFET_OFF_DET_100mv          1
#define LN8410_INFET_OFF_DET_200mv          2
#define LN8410_INFET_OFF_DET_300mv          3
#define LN8410_INFET_OFF_DET_400mv          4

/* Register 98h */
#define LN8410_REG_98                       0x98
#define LN8410_PMID_SWITCH_OK_STS_MASK      0x80
#define LN8410_PMID_SWITCH_OK_STS_SHIFT     7
#define LN8410_INFET_OK_STS_MASK            0x40
#define LN8410_INFET_OK_STS_SHIFT           6
#define LN8410_SWITCHING41_ACTIVE_MASK      0x20
#define LN8410_SWITCHING41_ACTIVE_SHIFT     5
#define LN8410_SWITCHING31_ACTIVE_MASK      0x10
#define LN8410_SWITCHING31_ACTIVE_SHIFT     4
#define LN8410_SWITCHING21_ACTIVE_MASK      0x08
#define LN8410_SWITCHING21_ACTIVE_SHIFT     3
#define LN8410_BYPASS_ACTIVE_MASK           0x04
#define LN8410_BYPASS_ACTIVE_SHIFT          2
#define LN8410_STANDBY_STS_MASK             0x02
#define LN8410_STANDBY_STS_SHIFT            1
#define LN8410_SHUTDOWN_STS_MASK            0x01
#define LN8410_SHUTDOWN_STS_SHIFT           0

/* Register 9Ah */
#define LN8410_REG_9A                       0x9A
#define PMID2OUT_OV_STS						BIT(3)
#define PMID2OUT_UV_STS						BIT(2)

#define LN8410_REG_2E                       0x2E
#define LN8410_REG_45                       0x45
#define LN8410_REG_46                       0x46
#define LN8410_REG_4C                       0x4C
#define LN8410_REG_90                       0x90
#define LN8410_REG_91                       0x91
#define LN8410_REG_93                       0x93
#define LN8410_REG_94                       0x94
#define LN8410_REG_95                       0x95
#define LN8410_REG_96                       0x96
enum ln8410_reg_addr {
	LN8410_REG_DEVICE_ID            = 0x00,
	LN8410_REG_VBAT_OVP             = 0x01,
	LN8410_REG_IBAT_OCP             = 0x02,
	LN8410_REG_VUSB_OVP             = 0x03,
	LN8410_REG_VWPC_OVP             = 0x04,
	LN8410_REG_IBUS_OCP             = 0x05,
	LN8410_REG_IBUS_UCP             = 0x06,
	LN8410_REG_PMID2OUT_OVP         = 0x07,
	LN8410_REG_PMID2OUT_UVP         = 0x08,
	LN8410_REG_CONVERTER_STATE      = 0x09,
	LN8410_REG_CTRL1                = 0x0A,
	LN8410_REG_CTRL2                = 0x0B,
	LN8410_REG_CTRL3                = 0x0C,
	LN8410_REG_CTRL4                = 0x0D,
	LN8410_REG_CTRL5                = 0x0E,
	LN8410_REG_INT_STAT             = 0x0F,
	LN8410_REG_INT_FLAG             = 0x10,
	LN8410_REG_INT_MASK             = 0x11,
	LN8410_REG_FLT_FLAG             = 0x12,
	LN8410_REG_FLT_MASK             = 0x13,
	LN8410_REG_ADC_CTRL             = 0x14,
	LN8410_REG_ADC_FN_DISABLE1      = 0x15,
	LN8410_REG_IBUS_ADC1            = 0x16,
	LN8410_REG_IBUS_ADC0            = 0x17,
	LN8410_REG_VBUS_ADC1            = 0x18,
	LN8410_REG_VBUS_ADC0            = 0x19,
	LN8410_REG_VUSB_ADC1            = 0x1A,
	LN8410_REG_VUSB_ADC0            = 0x1B,
	LN8410_REG_VWPC_ADC1            = 0x1C,
	LN8410_REG_VWPC_ADC0            = 0x1D,
	LN8410_REG_VOUT_ADC1            = 0x1E,
	LN8410_REG_VOUT_ADC0            = 0x1F,
	LN8410_REG_VBAT_ADC1            = 0x20,
	LN8410_REG_VBAT_ADC0            = 0x21,
	LN8410_REG_IBAT_ADC1            = 0x22,
	LN8410_REG_IBAT_ADC0            = 0x23,
	LN8410_REG_TSBAT_ADC1           = 0x24,
	LN8410_REG_TSBAT_ADC0           = 0x25,
	LN8410_REG_TDIE_ADC1            = 0x26,
	LN8410_REG_TDIE_ADC0            = 0x27,
	/* LN8410 Extention Registers */
	LN8410_REG_TSBAT_FLT            = 0x28,
	LN8410_REG_COMP_STAT0           = 0x29,
	LN8410_REG_COMP_FLAG0           = 0x2A,
	LN8410_REG_COMP_STAT1           = 0x2B,
	LN8410_REG_COMP_FLAG1           = 0x2C,
	LN8410_REG_ADC_CTRL2            = 0x2D,
	LN8410_REG_RECOVERY_CTRL        = 0x2E,
	LN8410_REG_FORCE_MODE_CFG_CTRL  = 0x2F,
	LN8410_REG_LION_INT_MASK        = 0x30,
	LN8410_REG_LION_CFG_1           = 0x31,
	LN8410_REG_LION_INT_MASK_2      = 0x32,
	LN8410_REG_LION_SPARE_REG       = 0x33,
	LN8410_REG_LB_CTRL              = 0x38,
	LN8410_REG_LION_CTRL            = 0x40,
	LN8410_REG_PRODUCT_ID           = 0x41,
	LN8410_REG_TRIM_4               = 0x45,
	LN8410_REG_TRIM_5               = 0x46,
	LN8410_REG_TRIM_11              = 0x4C,
	LN8410_REG_TEST_MODE_CTRL       = 0x56,
	LN8410_REG_CFG8                 = 0x54,
	LN8410_REG_CFG9                 = 0x59,
	LN8410_REG_BC_STS_A             = 0x60,
	LN8410_REG_BC_STS_B             = 0x61,
	LN8410_REG_FORCE_SC_MISC        = 0x69,
	LN8410_REG_PCODE_REG            = 0x6E,
	LN8410_REG_ALARM_CTRL           = 0x74,
	LN8410_REG_ADC_CFG2             = 0x76,
	LN8410_REG_LION_COMP_CTRL1      = 0x79,
	LN8410_REG_LION_COMP_CTRL2      = 0x7A,
	LN8410_REG_LION_COMP_CTRL3      = 0x7B,
	LN8410_REG_LION_COMP_CTRL4      = 0x7C,
	LN8410_REG_LION_COMP_CTRL5      = 0x7D,
	LN8410_REG_LION_COMP_CTRL6      = 0x7E,
	LN8410_REG_LION_STARTUP_CTRL    = 0x80,
	LN8410_REG_TEST_MODE_CTRL_2     = 0x8D,
	LN8410_REG_SWAP_CTRL_0          = 0x90,
	LN8410_REG_SWAP_CTRL_1          = 0x91,
	LN8410_REG_SWAP_CTRL_2          = 0x92,
	LN8410_REG_SWAP_CTRL_3          = 0x93,
	LN8410_REG_SWAP_CTRL_4          = 0x94,
	LN8410_REG_SWAP_CTRL_5          = 0x95,
	LN8410_REG_SWAP_CTRL_6          = 0x96,
	LN8410_REG_SYS_STS              = 0x98,
	LN8410_REG_SAFETY_STS           = 0x99,
	LN8410_REG_FAULT1_STS           = 0x9A,
	LN8410_REG_FAULT2_STS           = 0x9B,
	LN8410_REG_FAULT3_STS           = 0x9C,
};

#endif
