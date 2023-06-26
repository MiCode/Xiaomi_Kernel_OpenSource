/*******************************************************************************
 * @file     fusb30X.h
 * @author   USB PD Firmware Team
 *
 * Copyright 2018 ON Semiconductor. All rights reserved.
 *
 * This software and/or documentation is licensed by ON Semiconductor under
 * limited terms and conditions. The terms and conditions pertaining to the
 * software and/or documentation are available at
 * http://www.onsemi.com/site/pdf/ONSEMI_T&C.pdf
 * ("ON Semiconductor Standard Terms and Conditions of Sale, Section 8 Software").
 *
 * DO NOT USE THIS SOFTWARE AND/OR DOCUMENTATION UNLESS YOU HAVE CAREFULLY
 * READ AND YOU AGREE TO THE LIMITED TERMS AND CONDITIONS. BY USING THIS
 * SOFTWARE AND/OR DOCUMENTATION, YOU AGREE TO THE LIMITED TERMS AND CONDITIONS.
 ******************************************************************************/
#ifndef _FUSB30X_H_
#define _FUSB30X_H_

#include "platform.h"

/**
 * @brief Convenience interfaces to platform I2C functionality
 *
 * I2C Address is specified as an 8-bit value that includes the R/W LSB.
 *
 * @param i2cAddr 8-bit value with bit zero (R/W bit) set to zero.
 * @param regAddr 8-bit register address
 * @param length Number of bytes to read or write
 * @param data Pointer to byte array being written from or read to
 * @return TRUE on successful access
 */
FSC_BOOL DeviceWrite(FSC_U8 i2cAddr, FSC_U8 regAddr,
                     FSC_U8 length, FSC_U8* data);

FSC_BOOL DeviceRead(FSC_U8 i2cAddr, FSC_U8 regAddr,
                    FSC_U8 length, FSC_U8* data);

/* FUSB302 Device ID */
#define VERSION_302A 0x08
#define VERSION_302B 0x09
#define VERSION_302T 0x0A
#define VERSION_302P 0x0B

/* FUSB302 I2C Configuration */
#define FUSB300SlaveAddr   0x44
#define FUSB300AddrLength  1        /* One byte address */
#define FUSB300IncSize     1        /* One byte increment */

/* FUSB302 Register Addresses */
#define regDeviceID     0x01
#define regSwitches0    0x02
#define regSwitches1    0x03
#define regMeasure      0x04
#define regSlice        0x05
#define regControl0     0x06
#define regControl1     0x07
#define regControl2     0x08
#define regControl3     0x09
#define regMask         0x0A
#define regPower        0x0B
#define regReset        0x0C
#define regOCPreg       0x0D
#define regMaska        0x0E
#define regMaskb        0x0F
#define regControl4     0x10
#define regStatus0a     0x3C
#define regStatus1a     0x3D
#define regInterrupta   0x3E
#define regInterruptb   0x3F
#define regStatus0      0x40
#define regStatus1      0x41
#define regInterrupt    0x42
#define regFIFO         0x43

/* Coded SOP values that arrive in the RX FIFO */
#define SOP_CODE_SOP            0xE0
#define SOP_CODE_SOP1           0xC0
#define SOP_CODE_SOP2           0xA0
#define SOP_CODE_SOP1_DEBUG     0x80
#define SOP_CODE_SOP2_DEBUG     0x60

/* Device TX FIFO Token Definitions */
#define TXON                    0xA1
#define SYNC1_TOKEN             0x12
#define SYNC2_TOKEN             0x13
#define SYNC3_TOKEN             0x1B
#define RESET1                  0x15
#define RESET2                  0x16
#define PACKSYM                 0x80
#define JAM_CRC                 0xFF
#define EOP                     0x14
#define TXOFF                   0xFE

/*
 * Note: MDAC values are actually (MDAC + 1) * 42/420mV
 * Data sheet is incorrect.
 */
#define SDAC_DEFAULT        0x1F
#define MDAC_0P210V         0x04
#define MDAC_0P420V         0x09
#define MDAC_0P798V         0x12
#define MDAC_0P882V         0x14
#define MDAC_1P596V         0x25
#define MDAC_2P058V         0x30
#define MDAC_2P604V         0x3D
#define MDAC_2P646V         0x3E

#define VBUS_MDAC_0P84V     0x01
#define VBUS_MDAC_3P36      0x07
#define VBUS_MDAC_3P78      0x08
#define VBUS_MDAC_4P20      0x09
#define VBUS_MDAC_4P62      0x0A
#define VBUS_MDAC_5P04      0x0B
#define VBUS_MDAC_5P46      0x0C
#define VBUS_MDAC_7P14      0x10    /* (9V detach) */
#define VBUS_MDAC_8P40      0x13
#define VBUS_MDAC_9P66      0x16    /* (12V detach) */
#define VBUS_MDAC_11P34     0x1A
#define VBUS_MDAC_11P76     0x1B
#define VBUS_MDAC_12P18     0x1C    /* (15V detach) */
#define VBUS_MDAC_12P60     0x1D
#define VBUS_MDAC_14P28     0x21
#define VBUS_MDAC_15P96     0x25    /* (20V detach) */
#define VBUS_MDAC_18P90     0x2C
#define VBUS_MDAC_21P00     0x31

#define MDAC_MV_LSB             420     /* MDAC Resolution in mV */

#define VBUS_MV_VSAFE0V         840     /* Closest value for MDAC resolution */
#define VBUS_MV_VSAFE0V_DISCH   600
#define VBUS_MV_VSAFE5V_DISC    3670
#define VBUS_MV_VSAFE5V_L       4750
#define VBUS_MV_VSAFE5V_H       5500

#define VBUS_PD_TO_MV(v)   (v * 50)     /* Convert 50mv PD values to mv */
#define VBUS_PPS_TO_MV(v)  (v * 20)     /* Convert 20mv PD values to mv */
#define VBUS_MV_NEW_MAX(v) (v + (v/20)) /* Value in mv + 5% */
#define VBUS_MV_NEW_MIN(v) (v - (v/20)) /* Value in mv - 5% */
#define VBUS_MV_TO_MDAC(v) ((v/420)-1)  /* MDAC (VBUS) value is 420mv res - 1 */

typedef union {
    FSC_U8 byte;
    struct {
        FSC_U8 REVISION_ID:2;
        FSC_U8 PRODUCT_ID:2;
        FSC_U8 VERSION_ID:4;
    };
} regDeviceID_t;

typedef union {
    FSC_U16 word;
    FSC_U8 byte[2];
    struct {
        /* Switches0 */
        FSC_U8 PDWN1:1;
        FSC_U8 PDWN2:1;
        FSC_U8 MEAS_CC1:1;
        FSC_U8 MEAS_CC2:1;
        FSC_U8 VCONN_CC1:1;
        FSC_U8 VCONN_CC2:1;
        FSC_U8 PU_EN1:1;
        FSC_U8 PU_EN2:1;
        /* Switches1 */
        FSC_U8 TXCC1:1;
        FSC_U8 TXCC2:1;
        FSC_U8 AUTO_CRC:1;
        FSC_U8 :1;
        FSC_U8 DATAROLE:1;
        FSC_U8 SPECREV:2;
        FSC_U8 POWERROLE:1;
    };
} regSwitches_t;

typedef union {
    FSC_U8 byte;
    struct {
        FSC_U8 MDAC:6;
        FSC_U8 MEAS_VBUS:1;
        FSC_U8 :1;
    };
} regMeasure_t;

typedef union {
    FSC_U8 byte;
    struct {
        FSC_U8 SDAC:6;
        FSC_U8 SDAC_HYS:2;
    };
} regSlice_t;

typedef union {
    FSC_U32 dword;
    FSC_U8 byte[4];
    struct {
        /* Control0 */
        FSC_U8 TX_START:1;
        FSC_U8 AUTO_PRE:1;
        FSC_U8 HOST_CUR:2;
        FSC_U8 LOOPBACK:1;
        FSC_U8 INT_MASK:1;
        FSC_U8 TX_FLUSH:1;
        FSC_U8 :1;
        /* Control1 */
        FSC_U8 ENSOP1:1;
        FSC_U8 ENSOP2:1;
        FSC_U8 RX_FLUSH:1;
        FSC_U8 :1;
        FSC_U8 BIST_MODE2:1;
        FSC_U8 ENSOP1DP:1;
        FSC_U8 ENSOP2DB:1;
        FSC_U8 :1;
        /* Control2 */
        FSC_U8 TOGGLE:1;
        FSC_U8 MODE:2;
        FSC_U8 WAKE_EN:1;
        FSC_U8 WAKE_SELF:1;
        FSC_U8 TOG_RD_ONLY:1;
        FSC_U8 :2;
        /* Control3 */
        FSC_U8 AUTO_RETRY:1;
        FSC_U8 N_RETRIES:2;
        FSC_U8 AUTO_SOFTRESET:1;
        FSC_U8 AUTO_HARDRESET:1;
        FSC_U8 BIST_TMODE:1;          /* 302B Only */
        FSC_U8 SEND_HARDRESET:1;
        FSC_U8 :1;
    };
} regControl_t;

typedef union {
    FSC_U8 byte;
    struct {
        FSC_U8 M_BC_LVL:1;
        FSC_U8 M_COLLISION:1;
        FSC_U8 M_WAKE:1;
        FSC_U8 M_ALERT:1;
        FSC_U8 M_CRC_CHK:1;
        FSC_U8 M_COMP_CHNG:1;
        FSC_U8 M_ACTIVITY:1;
        FSC_U8 M_VBUSOK:1;
    };
} regMask_t;

typedef union {
    FSC_U8 byte;
    struct {
        FSC_U8 POWER:4;
        FSC_U8 :4;
    };
} regPower_t;

typedef union {
    FSC_U8 byte;
    struct {
        FSC_U8 SW_RES:1;
        FSC_U8 :7;
    };
} regReset_t;

typedef union {
    FSC_U8 byte;
    struct {
        FSC_U8 OCP_CUR:3;
        FSC_U8 OCP_RANGE:1;
        FSC_U8 :4;
    };
} regOCPreg_t;

typedef union {
    FSC_U16 word;
    FSC_U8 byte[2];
    struct {
        /* Maska */
        FSC_U8 M_HARDRST:1;
        FSC_U8 M_SOFTRST:1;
        FSC_U8 M_TXSENT:1;
        FSC_U8 M_HARDSENT:1;
        FSC_U8 M_RETRYFAIL:1;
        FSC_U8 M_SOFTFAIL:1;
        FSC_U8 M_TOGDONE:1;
        FSC_U8 M_OCP_TEMP:1;
        /* Maskb */
        FSC_U8 M_GCRCSENT:1;
        FSC_U8 :7;
    };
} regMaskAdv_t;

typedef union {
    FSC_U8 byte;
    struct {
        FSC_U8 TOG_USRC_EXIT:1;
        FSC_U8 :7;
    };
} regControl4_t;

typedef union {
    FSC_U8 byte[7];
    struct {
        FSC_U16  StatusAdv;
        FSC_U16  InterruptAdv;
        FSC_U16  Status;
        FSC_U8   Interrupt1;
    };
    struct {
        /* Status0a */
        FSC_U8 HARDRST:1;
        FSC_U8 SOFTRST:1;
        FSC_U8 POWER23:2;
        FSC_U8 RETRYFAIL:1;
        FSC_U8 SOFTFAIL:1;
        FSC_U8 TOGDONE:1;
        FSC_U8 M_OCP_TEMP:1;
        /* Status1a */
        FSC_U8 RXSOP:1;
        FSC_U8 RXSOP1DB:1;
        FSC_U8 RXSOP2DB:1;
        FSC_U8 TOGSS:3;
        FSC_U8 :2;
        /* Interrupta */
        FSC_U8 I_HARDRST:1;
        FSC_U8 I_SOFTRST:1;
        FSC_U8 I_TXSENT:1;
        FSC_U8 I_HARDSENT:1;
        FSC_U8 I_RETRYFAIL:1;
        FSC_U8 I_SOFTFAIL:1;
        FSC_U8 I_TOGDONE:1;
        FSC_U8 I_OCP_TEMP:1;
        /* Interruptb */
        FSC_U8 I_GCRCSENT:1;
        FSC_U8 :7;
        /* Status0 */
        FSC_U8 BC_LVL:2;
        FSC_U8 WAKE:1;
        FSC_U8 ALERT:1;
        FSC_U8 CRC_CHK:1;
        FSC_U8 COMPARATOR:1;
        FSC_U8 ACTIVITY:1;
        FSC_U8 VBUSOK:1;
        /* Status1 */
        FSC_U8 OCP:1;
        FSC_U8 OVRTEMP:1;
        FSC_U8 TX_FULL:1;
        FSC_U8 TX_EMPTY:1;
        FSC_U8 RX_FULL:1;
        FSC_U8 RX_EMPTY:1;
        FSC_U8 RXSOP1:1;
        FSC_U8 RXSOP2:1;
        /* Interrupt */
        FSC_U8 I_BC_LVL:1;
        FSC_U8 I_COLLISION:1;
        FSC_U8 I_WAKE:1;
        FSC_U8 I_ALERT:1;
        FSC_U8 I_CRC_CHK:1;
        FSC_U8 I_COMP_CHNG:1;
        FSC_U8 I_ACTIVITY:1;
        FSC_U8 I_VBUSOK:1;
    };
} regStatus_t;

typedef struct
{
    regDeviceID_t   DeviceID;
    regSwitches_t   Switches;
    regMeasure_t    Measure;
    regSlice_t      Slice;
    regControl_t    Control;
    regMask_t       Mask;
    regPower_t      Power;
    regReset_t      Reset;
    regOCPreg_t     OCPreg;
    regMaskAdv_t    MaskAdv;
    regControl4_t   Control4;
    regStatus_t     Status;
} DeviceReg_t;
#endif /* _FUSB30X_H_ */
