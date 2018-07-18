/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * Copyright(c) 2016, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MI2_REG_H__
#define __MI2_REG_H__

#define DEBUG_LOG_OUTPUT

#define _BIT0	0x01
#define _BIT1	0x02
#define _BIT2	0x04
#define _BIT3	0x08
#define _BIT4	0x10
#define _BIT5	0x20
#define _BIT6	0x40
#define _BIT7	0x80

/***************************************************************/
/*Register definition of device address 0x58*/

#define PRODUCT_ID_L 0x02
#define PRODUCT_ID_H 0x03

#define TCPC_ROLE_CONTROL  0x1a
#define TCPC_COMMAND  0x23

#define ANALOG_CTRL_0  0xA0
#define DFP_OR_UFP _BIT6

#define INTR_ALERT_1  0xCC
#define INTR_SOFTWARE_INT _BIT3
#define INTR_RECEIVED_MSG _BIT5

#define  TCPC_CONTROL       0x19
/* bit positions */
#define  DEBUG_ACCESSORY_CONTROL  4
#define  BIST_TEST_MODE           1
#define  PLUG_ORIENTATION         0

#define  POWER_CONTROL       0x1C
/* bit positions */
#define  DISABLE_VBUS_VOLTAGE_MONITOR   6
#define  DISABLE_VOLTAGE_ALARMS         5
#define  VCONN_POWER_SUPPORTED_CONTROL  1
#define  ENABLE_VCONN                   0



#define  ANALOG_CTRL_1      0xA1


#define  ANALOG_CTRL_9      0xA9
/* bit positions */
#define  SAFE_MODE         7
#define  TEST_USB_PD_EN    6
#define  TEST_EN_MI        5
#define  BMC_MODE1_SEL_VL  3
#define  BMC_MODE1_CAP     0

#define  TCPC_SWITCH_0      0xB4
/* bit positions */
#define  SWAP_AUX_R       7
#define  SWAP_AUX_T       6
#define  SW_SEL1_5        5
#define  SW_SEL1_4        4
#define  SW_SEL1_3        3
#define  SW_SEL1_2        2
#define  SW_SEL1_1        1
#define  SW_SEL1_0        0

#define  TCPC_SWITCH_1     0xB5
/* bit positions */
#define  SW_SEL2_5        5
#define  SW_SEL2_4        4
#define  SW_SEL2_3        3
#define  SW_SEL2_2        2
#define  SW_SEL2_1        1
#define  SW_SEL2_0        0

#define  CHIP_POWER_CTRL    0xB6
/* bit positions */
#define  PD_V10_ODFC      5
#define  PD_V10_DPTX      4
#define  PU_DPTX          3
#define  PU_HDMIRX        2
#define  ISO_EN_N_T       1
#define  PU_PART_DIG      0

#define  HPD_CTRL           0xBD
/* bit positions */
#define  HPD_OUT          5
#define  R_HPD_UNPLUG     4
#define  R_HPD_PLUGIN     3
#define  R_HPD_IRQ        2
#define  R_HPD_MODE       1
#define  R_HPD_OUT        0





/*================ END of I2C Address 0x58 ========================*/



/***************************************************************/
/*Register definition of device address 0x70*/
#define  I2C_ADDR_70_DPTX              0x70

#define  SYSTEM              0x80
/* bit positions */
#define  CHIP_AUTH_RESET       7
#define  BYPASS_CHIP_AUTH      6
#define  HDCP_VERSION          5
#define  HDCP2_FW_EN           4
#define  HDCP2_HPD             3
#define  DET_STA               2
#define  FORCE_DET             1
#define  DET_CTRL              0

#define  DP_CONFIG_3         0x84
/* bit positions */
/* bit[7:5] are bpc -  bits per channel */
#define  BPC                   5
/* 011: 12 bit */
/* 010: 10 bit */
/* 001:  8 bit */
/* 000:  6 bit */
/* other: reserved */
#define  YC_COEFF              4 /* ITU-R BT.601 or BT.709 */
#define  D_RANGE               3
/* color space and chroma format: */
/* 00: RGB,  01: YCbCr422, */
/* 10: YCbCr444, */
/* 11: reserved */
/* dynamic range: video or graphics */
#define  COLOR_F               1
#define  SYNC_MODE             0

#define SP_TX_LINK_BW_SET_REG 0xA0
#define SP_TX_LANE_COUNT_SET_REG 0xA1

#define BUF_DATA_COUNT 0xE4

#define AUX_CTRL 0xE5
#define AUX_ADDR_7_0 0xE6
#define AUX_ADDR_15_8  0xE7
#define AUX_ADDR_19_16 0xE8

#define SP_TX_INT_STATUS1 0xF7/*DISPLAYPORT_INTERRUPT*/
#define POLLING_ERR 0x10

#define DP_CONFIG_24 0xB0
#define POLLING_EN 0x02

#define  DP_CONFIG_20             0xB8
/* bit positions */
/* bit[7:6] are reserved */
#define  M_VID_DEBUG                 5
#define  NEW_PRBS7                   4
#define  DIS_FIFO_RST                3
#define  DISABLE_AUTO_RESET_ENCODER  2
#define  INSERT_ER                   1
#define  PRBS31_EN                   0
#define M_VID_0 0xC0

#define M_VID_1 0xC1

#define M_VID_2 0xC2

#define N_VID_0 0xC3

#define N_VID_1 0xC4

#define N_VID_2 0xC5


#define BUF_DATA_0 0xF0
#define SP_TX_AUX_STATUS 0xE0

#define AUX_CTRL2 0xE9
#define ADDR_ONLY_BIT 0x02
#define AUX_OP_EN 0x01
/***************************************************************/
/*Register definition of device address 0x72*/
#define AUX_RST	0x04
#define RST_CTRL2 0x07

#define SP_TX_TOTAL_LINE_STA_L 0x24
#define SP_TX_TOTAL_LINE_STA_H 0x25
#define SP_TX_ACT_LINE_STA_L 0x26
#define SP_TX_ACT_LINE_STA_H 0x27
#define SP_TX_V_F_PORCH_STA 0x28
#define SP_TX_V_SYNC_STA 0x29
#define SP_TX_V_B_PORCH_STA 0x2A
#define SP_TX_TOTAL_PIXEL_STA_L 0x2B
#define SP_TX_TOTAL_PIXEL_STA_H 0x2C
#define SP_TX_ACT_PIXEL_STA_L 0x2D
#define SP_TX_ACT_PIXEL_STA_H 0x2E
#define SP_TX_H_F_PORCH_STA_L 0x2F
#define SP_TX_H_F_PORCH_STA_H 0x30
#define SP_TX_H_SYNC_STA_L 0x31
#define SP_TX_H_SYNC_STA_H 0x32
#define SP_TX_H_B_PORCH_STA_L 0x33
#define SP_TX_H_B_PORCH_STA_H 0x34
#define SP_TX_VID_CTRL 0x84

#define VIDEO_BIT_MATRIX_12 0x4c
#define VIDEO_BIT_MATRIX_13 0x4d
#define VIDEO_BIT_MATRIX_14 0x4e
#define VIDEO_BIT_MATRIX_15 0x4f
#define VIDEO_BIT_MATRIX_16 0x50
#define VIDEO_BIT_MATRIX_17 0x51
#define VIDEO_BIT_MATRIX_18 0x52
#define VIDEO_BIT_MATRIX_19 0x53
#define VIDEO_BIT_MATRIX_20 0x54
#define VIDEO_BIT_MATRIX_21 0x55
#define VIDEO_BIT_MATRIX_22 0x56
#define VIDEO_BIT_MATRIX_23 0x57

#define AUDIO_CHANNEL_STATUS_1 0xd0
#define AUDIO_CHANNEL_STATUS_2 0xd1
#define AUDIO_CHANNEL_STATUS_3 0xd2
#define AUDIO_CHANNEL_STATUS_4 0xd3
#define AUDIO_CHANNEL_STATUS_5 0xd4
#define AUDIO_CHANNEL_STATUS_6 0xd5
#define TDM_SLAVE_MODE 0x10
#define I2S_SLAVE_MODE 0x08

#define AUDIO_CONTROL_REGISTER 0xe6
#define TDM_TIMING_MODE 0x08

#define  I2C_ADDR_72_DPTX              0x72


#define  POWER_CONTROL_0  0x05
/* bit positions */
#define  PD_REG           7
#define  PD_HDCP2         6
#define  PD_HDCP          5
#define  PD_AUDIO         4
#define  PD_VIDEO         3
#define  PD_LINK          2
#define  PD_TOTAL         1
/* bit[0] is reserved */

#define  RESET_CONTROL_0  0x06
/* bit positions */
#define  MISC_RST         7
#define  VID_CAP_RST      6
#define  VID_FIFO_RST     5
#define  AUD_FIFO_RST     4
#define  AUD_RST          3
#define  HDCP_RST         2
#define  SW_RST           1
#define  HW_RST           0

#define  VIDEO_CONTROL_0  0x08
/* bit positions */
#define  VIDEO_EN             7
/* bit[6] is reserved */
/* DE re-generation mode: 1=enabled, 0=disabled */
#define  DE_GEN               5
/* De YC-MUX: 1=enabled, 0=disabled */
#define  DEMUX                4
/* bit[3] is reserved in MI-2 */
#define  HALF_FREQUENCY_MODE  3
/* alwaysa set bit[2] to 1 in MI-2 */
#define  DDR_MODE             2
#define  DDR_CTRL             1
#define  NEGEDGE_LATCH        0


#define  VIDEO_CONTROL_2  0x0A
/* bit positions */
	/* YCbCr cofficients of input video: 1=ITU709, 0=ITU601 */
#define  IN_YC_COEFFI       7
	/* reserved in MI-2 */
#define  HDMI_HPD           6
/* dalay one of DE: 1=enabled, 0=disabled */
#define  DE_DELAY           5
/* update enable control of video format parameter in capture block: */
/* 1=enabled, 0=disabled */
#define  VID_CHK_UPDATE_EN  4
#define  VIDEO_COLOR_SWAP   1  /* bit[3:1]: reserved in MI-2 */
#define  VIDEO_BIT_SWAP     0  /* reserved in MI-2 */

#define  VIDEO_CONTROL_4  0x0C
/* bit positions */
#define  CSC_STD_SEL        7
#define  XVYCC_RNG_LMT      6
#define  RANGE_Y2R          5  /* reserved in MI-2 */
#define  CSPACE_Y2R         4  /* reserved in MI-2 */
#define  RGB_RNG_LMT        3
#define  YC_RNG_LMT         2
#define  RANGE_R2Y          1
#define  CSPACE_R2Y         0

#define  VIDEO_CONTROL_5  0x0D
/* bit positions */
#define  TEST_PATTERN_EN    7  /* video BIST: 1=enabled, 0=disabled */
#define  VIDEO_PROCESS_EN   6
#define  IN_PIXEL_REPEAT    4  /* bit[5:4]: reserved in MI-2 */
/* up sampling mode:   1=FIR filter, 0=copy sample */
#define  VID_US_MODE        3
/* down sampling mode: 1=FIR filter, 0=skip sample */
#define  VID_DS_MODE        2
/* reserved in MI-2 */
#define  UP_SAMPLE          1
/* 4:4:4 to 4:2:2 down sampling: 1=enabled, 0=disabled */
#define  DOWN_SAMPLE        0



#define  VIDEO_CONTROL_7  0x0F
/* bit positions */
#define  VID_HRES_TH      4
#define  VID_VRES_TH      0


#define  TOTAL_LINE_CFG_L       0x12
#define  TOTAL_LINE_CFG_H       0x13  /* note: bit[7:6] are reserved */
#define  ACTIVE_LINES_L         0x14
#define  ACTIVE_LINES_H         0x15  /* note: bit[7:6] are reserved */
#define  VERTICAL_FRONT_PORCH   0x16
#define  VERTICAL_SYNC_WIDTH    0x17
#define  VERTICAL_BACK_PORCH    0x18

#define  HORIZONTAL_TOTAL_PIXELS_L    0x19
#define  HORIZONTAL_TOTAL_PIXELS_H    0x1A  /* note: bit[7:6] are reserved */
#define  HORIZONTAL_ACTIVE_PIXELS_L   0x1B
#define  HORIZONTAL_ACTIVE_PIXELS_H   0x1C  /* note: bit[7:6] are reserved */
#define  HORIZONTAL_FRONT_PORCH_L     0x1D
#define  HORIZONTAL_FRONT_PORCH_H     0x1E  /* note: bit[7:4] are reserved */
#define  HORIZONTAL_SYNC_WIDTH_L      0x1F
#define  HORIZONTAL_SYNC_WIDTH_H      0x20  /* note: bit[7:4] are reserved */
#define  HORIZONTAL_BACK_PORCH_L      0x21
#define  HORIZONTAL_BACK_PORCH_H      0x22  /* note: bit[7:4] are reserved */




#define NFCU_02    0xC8
#define NFCU_03    0xC9
#define NFCU_04    0xCA
#define NFCU_05    0xCB

/*======================= END of I2C Address 0x72 ===============*/
/***************************************************************/
/*Register definition of device address 0x7A*/
#define  DPPLL_REG4               0xF9
/* bit positions */
/* DPPLL_REG4[7:4] is not used */
#define  atest_enable   3  /* DPPLL_REG4[3] is analog signal test enable */
/*  0: disable test analog signal
 *  1: enable test analog signal
 */
#define  dtest_enable   2  /* DPPLL_REG4[2] is digital signal test enable */
/*  0: disable test digital signal
 *    1: enable test digital signal
 */
#define  test_sig_sel   0  /* DPPLL_REG4[1:0] is test signal selection */
/*  when atest_enable = 1,
 *    00: vreg_1p45
    01: duty_meas (decided by duty_outputen and duty_sel<1:0>)
    10: avdd10
    11: vcox
 */
/*  when dtest_enable = 1,
 *    00: refbx (\wr 7e 42 f; \wr 7e 81 e1; \wr 7a f9 4
	==> Now on WS pin, you'd see a 27 MHz clock.)
    01: ckfbx (\wr 7e 42 f; \wr 7e 81 e1; \wr 7a f9 5
	==> Now on WS pin, you'd see a 27 MHz signal.
   However, this signal is neither square wave nor sine wave,
   it should be narrow pulses.)
    10: ck_vco
    11: avss10
 */




/*================= END of I2C Address 0x7A =====================*/

/***************************************************************/
/*Register definition of device address 0x7e*/

#define  I2C_ADDR_7E_FLASH_CONTROLLER  0x7E

#define  R_BOOT_RETRY  0x00
/* bit positions */
#define  SRAM_CS          4
#define  FUSE_WRITE       3
#define  FUSE_STATUS      2
#define  BOOT_RETRY_NUM   0

#define  R_RAM_CTRL  0x05
/* bit positions */
#define  FLASH_DONE      7
#define  BOOT_LOAD_DONE  6
#define  CRC_OK          5
#define  LOAD_DONE       4
#define  O_RW_DONE       3
#define  FUSE_BUSY       2
#define  DECRYPT_EN      1
#define  LOAD_START      0


#define  FUSE_DATA_IN7_0    0x0A
#define  FUSE_DATA_IN15_8   0x0B
#define  FUSE_DATA_IN23_16  0x0C


/* note: The actual implementation doesn't match register spec v0.5 */
/* - High byte and low byte are reversed. */
#define  FLASH_ADDR_H  0x0F
#define  FLASH_ADDR_L  0x10

/* note: The actual implementation doesn't match register spec v0.5 */
/* - High byte and low byte are reversed. */
#define  EEPROM_ADDR_H  0x0F
#define  EEPROM_ADDR_L  0x10


#define  FLASH_WRITE_BUF_BASE_ADDR  0x11
#define  FLASH_WRITE_MAX_LENGTH     0x20

#define  EEPROM_WRITE_BUF_BASE_ADDR 0x11
#define  EEPROM_WRITE_MAX_LENGTH    0x10

#define AUTO_PD_MODE 0x2f/*0x6e*/
#define AUTO_PD_ENABLE 0x02
#define MAX_VOLTAGE_SETTING 0x29/*0xd0*/
#define MAX_POWER_SETTING 0x2A/*0xd1*/
#define MIN_POWER_SETTING 0x2B/*0xd2*/
#define RDO_MAX_VOLTAGE 0x2C /* 0x7E:0x2C // 0xD3*/
#define RDO_MAX_POWER 0x2D /* 0x7E:0x2D // 0xD4*/
#define RDO_MAX_CURRENT 0x2E /* 0x7E:0x2E // 0xD5*/

/* note: 0 means 1 byte, x means (x+1) bytes; max x=31 */
/* note: The actual implementation doesn't match register spec v0.5 */
/* - High byte and low byte are reversed. */
#define  FLASH_LEN_H  0x31
#define  FLASH_LEN_L  0x32

/* note: The actual implementation doesn't match register spec v0.5 */
/* - High byte and low byte are reversed. */
#define  EEPROM_LEN_H  0x31
#define  EEPROM_LEN_L  0x32

#define OCM_FW_VERSION 0x31
#define OCM_FW_REVERSION 0x32

#define  R_FLASH_RW_CTRL  0x33
/* bit positions */
#define  READ_DELAY_SELECT       7
#define  GENERAL_INSTRUCTION_EN  6
#define  FLASH_ERASE_EN          5
#define  RDID_READ_EN            4
#define  REMS_READ_EN            3
#define  WRITE_STATUS_EN         2
#define  FLASH_READ              1
#define  FLASH_WRITE             0

/* the value to be written into Flash status register */
#define  STATUS_REGISTER_IN  0x34
/* Flash REMS READ DATA (depend on Flash vendor definition) */
#define  REMS_READ_ADDR  0x35



/* This register is for single-byte commands only, i.e. */
/* in Table 2 in GD25D10B datasheet, all the cells */
/* following "Byte 1" in the row are blank. */
/* For all other multi-byte commands, hardware has a wrapper, */
/* and software shouldn't write the commands in this register directly. */
#define  GENERAL_INSTRUCTION_TYPE  0x36
/* Flash operation commands - refer to Table 2 in GD25D10B/05B datasheet */
#define  WRITE_ENABLE      0x06
#define  WRITE_DISABLE     0x04
#define  DEEP_POWER_DOWN   0xB9
#define  DEEP_PD_REL       0xAB  /* Release from Deep Power-Down */
#define  CHIP_ERASE_A      0xC7
#define  CHIP_ERASE_B      0x60

#define  FLASH_ERASE_TYPE     0x37
#define  SECTOR_ERASE     0x20
#define  BLOCK_ERASE_32K  0x52
#define  BLOCK_ERASE_64K  0xD8

#define  STATUS_REGISTER     0x38  /* Flash status register readback value */
/* bit positions */
/* Status Register Protect bit, operates in conjunction with */
/* the Write Protect (WP#) signal */
/* The SRP bit and WP signal set the device to the Hardware Protected mode.*/
/* When the SRP = 1, and WP# signal is Low, the non-volatile bits */
/* of the Status Register (SRP, BP2, BP1, BP0) */
/* become read-only and the Write Status Register (WRSR) */
/* instruction is not executed. */
/* The default value of SRP bit is 0. */
#define  SRP0   7

/* Block Protect bits */
/* These bits are non-volatile. They define the size of the area */
/* to be software protected against Program and Erase commands. */
/* These bits are written with the Write Status Register (WRSR) command. */
/* When the (BP4, BP3, BP2, BP1, BP0) bits are set to 1, */
/* the relevant memory area becomes protected against Page Program (PP), */
/* Sector Erase (SE), and Block Erase (BE) commands. */
/* Refer to Table 1.0 in GD25D10B/05B datasheet for details. */
/* The (BP4, BP3, BP2, BP1, BP0) bits can be written provided that */
/* the Hardware Protected mode has not been set. */
#define  BP4   6
#define  BP3   5
#define  BP2   4
#define  BP1   3
#define  BP0   2

/* Write Enable Latch bit, indicates the status of */
/* the internal Write Enable Latch. */
/* When WEL bit is 1, the internal Write Enable Latch is set. */
/* When WEL bit is 0, the internal Write Enable Latch is reset, */
/* and Write Status Register, Program or */
/*    Erase commands are NOT accepted. */
/* The default value of WEL bit is 0. */
#define  WEL   1

/* Write In Progress bit, indicates whether the memory is busy */
/* in program/erase/write status register progress. */
/* When WIP bit is 1, it means the device is busy in */
/* program/erase/write status register progress. */
/* When WIP bit is 0, it means the device is not in */
/* program/erase/write status register progress. */
/* The default value of WIP bit is 0. */
#define  WIP   0

#define  MANUFACTURE_ID  0x39
#define  DEVICE_ID       0x3A
#define  MEM_TYPE        0x3B
#define  CAPACITY        0x3C



#define  XTAL_FRQ_SEL    0x3F
/* bit field positions */
#define  XTAL_FRQ_SEL_POS    5
/* bit field values */
#define  XTAL_FRQ_19M2    (0 << XTAL_FRQ_SEL_POS)
#define  XTAL_FRQ_27M     (4 << XTAL_FRQ_SEL_POS)

#define  R_DSC_CTRL_0  0x40
/* bit positions */
#define  READ_STATUS_EN       7
#define  CLK_1MEG_RB     6  /* 1MHz clock reset; 0=reset, 0=reset release */
#define  DSC_BIST_DONE   1  /* bit[5:1]: 1=DSC MBIST pass */
#define  DSC_EN          0x01  /* 1=DSC enabled, 0=DSC disabled */

#define INTERFACE_INTR_MASK 0x43
#define RECEIVED_MSG_MASK 1
#define RECEIVED_ACK_MASK 2
#define VCONN_CHANGE_MASK 4
#define VBUS_CHANGE_MASK 8
#define CC_STATUS_CHANGE_MASK 16
#define DATA_ROLE_CHANGE_MASK 32

#define INTERFACE_CHANGE_INT 0x44
#define RECEIVED_MSG 0x01
#define RECEIVED_ACK 0x02
#define VCONN_CHANGE 0x04
#define VBUS_CHANGE 0x08
#define CC_STATUS_CHANGE 0x10
#define DATA_ROLE_CHANGE 0x20
#define PR_CONSUMER_GOT_POWER 0x40
#define HPD_STATUS_CHANGE 0x80

#define SYSTEM_STSTUS 0x45
/*0: VCONN off; 1: VCONN on*/
#define VCONN_STATUS 0x04
/*0: vbus off; 1: vbus on*/
#define VBUS_STATUS 0x08
/*0: host; 1:device*/
#define DATA_ROLE 0x20

#define HPD_STATUS 0x80
#define NEW_CC_STATUS 0x46

#define  GPIO_CTRL_0     0x47
/* bit positions */
#define  GPIO_3_DATA     7
#define  GPIO_3_OEN      6
#define  GPIO_2_DATA     5
#define  GPIO_2_OEN      4
#define  GPIO_1_DATA     3
#define  GPIO_1_OEN      2
#define  GPIO_0_DATA     1
#define  GPIO_0_OEN      0

#define  GPIO_CTRL_1     0x48
/* bit positions */
/* bit[7:4] are reserved */
/* When bonding with Flash, this register will control the flash WP pin */
#define  FLASH_WP     3
/* 0 = write protect, 1 = no write protect*/
#define  WRITE_UNPROTECTED  1
#define  WRITE_PROTECTED    0
/* bit[2:0] are reserved */


#define  GPIO_CTRL_2     0x49
/* bit positions */
#define  HPD_SOURCE      6
#define  GPIO_10_DATA    5
#define  GPIO_10_OEN     4
#define  GPIO_9_DATA     3
#define  GPIO_9_OEN      2
#define  GPIO_8_DATA     1
#define  GPIO_8_OEN      0
#define  GPIO_STATUS_1  0x4B
/* bit positions */
#define  OCM_RESET             2
#define  INTERRUPT_POLARITY    1
#define  INTERRUPT_OPEN_DRAIN  0
#define  TOTAL_PIXEL_L_7E   0x50
#define  TOTAL_PIXEL_H_7E   0x51   /* note: bit[7:6] are reserved */

#define  ACTIVE_PIXEL_L_7E  0x52
#define  ACTIVE_PIXEL_H_7E  0x53   /* note: bit[7:6] are reserved */

#define  HORIZON_FRONT_PORCH_L_7E  0x54
/* note: bit[7:4] are EEPROM Key 0, which is not used in MI-2 */
#define  HORIZON_FRONT_PORCH_H_7E  0x55

#define  HORIZON_SYNC_WIDTH_L_7E   0x56
#define  HORIZON_SYNC_WIDTH_H_7E   0x57

#define  HORIZON_BACK_PORCH_L_7E   0x58
#define  HORIZON_BACK_PORCH_H_7E   0x59

#define  FLASH_READ_BUF_BASE_ADDR  0x60
#define  FLASH_READ_MAX_LENGTH     0x20

#define  EEPROM_READ_BUF_BASE_ADDR  0x60
#define  EEPROM_READ_MAX_LENGTH     0x10

#define  DSC_REG_ADDR_H_F_PORCH_H    0x55
/* bit positions */
#define  KEY_0                   4
#define  HORIZON_FRONT_PORCH_H   0

#define  DSC_REG_ADDR_H_SYNC_CFG_H   0x57
/* bit positions */
#define  KEY_1                   4
#define  HORIZON_SYNC_WIDTH_H    0

#define  DSC_REG_ADDR_H_PORCH_CFG_H  0x59
/* bit positions */
#define  KEY_2                   4
#define  HORIZON_BACK_PORCH_H    0
#define  R_I2C_0                     0x80
/* bit positions */
#define  COL_CORE_RESET          7
#define  I2C_ASSERT_DELAY        0

#define  R_I2C_1                     0x81
/* bit positions */
#define  RATIO                   6
#define  DEBUG_OE                5
#define  ADDR_80H_SEL            4
#define  WRITE_DELAY_COUNTER     0

#define  OCM_DEBUG_REG_8             0x88
/* bit positions */
#define  STOP_MAIN_OCM           6
#define AP_AUX_ADDR_7_0 0x11
#define AP_AUX_ADDR_15_8  0x12
#define AP_AUX_ADDR_19_16 0x13

/* note: bit[0:3] AUX status, bit 4 op_en, bit 5 address only */
#define AP_AUX_CTRL_STATUS 0x14
#define AP_AUX_CTRL_OP_EN 0x10
#define AP_AUX_CTRL_ADDRONLY 0x20

#define AP_AUX_BUFF_START 0x15
#define PIXEL_CLOCK_L 0x25
#define PIXEL_CLOCK_H 0x26

#define AP_AUX_COMMAND 0x27  /*com+len*/
/*bit 0&1: 3D video structure */
/* 0x01: frame packing,  0x02:Line alternative, 0x03:Side-by-side(full)*/
#define AP_AV_STATUS 0x28
#define AP_VIDEO_CHG _BIT2
#define AP_AUDIO_CHG _BIT3
#define AP_MIPI_MUTE _BIT4 /* 1:MIPI input mute, 0: ummute*/
#define AP_MIPI_RX_EN _BIT5 /* 1: MIPI RX input in  0: no RX in*/
#define AP_DISABLE_PD _BIT6
#define AP_DISABLE_DISPLAY _BIT7
/***************************************************************/
/*Register definition of device address 0x54*/
#define  TOTAL_LINES_L         0x12
#define  TOTAL_LINES_H         0x13
#define  ACTIVE_LINES_L         0x14
#define  ACTIVE_LINES_H         0x15  /* note: bit[7:6] are reserved */
#define  VERTICAL_FRONT_PORCH   0x16
#define  VERTICAL_SYNC_WIDTH    0x17
#define  VERTICAL_BACK_PORCH    0x18

#define  HORIZONTAL_TOTAL_PIXELS_L    0x19
#define  HORIZONTAL_TOTAL_PIXELS_H    0x1A  /* note: bit[7:6] are reserved */
#define  HORIZONTAL_ACTIVE_PIXELS_L   0x1B
#define  HORIZONTAL_ACTIVE_PIXELS_H   0x1C  /* note: bit[7:6] are reserved */
#define  HORIZONTAL_FRONT_PORCH_L     0x1D
#define  HORIZONTAL_FRONT_PORCH_H     0x1E  /* note: bit[7:4] are reserved */
#define  HORIZONTAL_SYNC_WIDTH_L      0x1F
#define  HORIZONTAL_SYNC_WIDTH_H      0x20  /* note: bit[7:4] are reserved */
#define  HORIZONTAL_BACK_PORCH_L      0x21
#define  HORIZONTAL_BACK_PORCH_H      0x22  /* note: bit[7:5] are reserved */

#define R_PPS_REG_0   0x80
/***************************************************************/
/*Register definition of device address 0x84*/
#define  MIPI_PHY_CONTROL_1               0x01
/* bit positions */
#define  MIPI_PD_LPCD_3    7
#define  MIPI_PD_LPCD_2    6
#define  MIPI_PD_LPCD_1    5
#define  MIPI_PD_LPCD_0    4
#define  MIPI_PD_3         3
#define  MIPI_PD_2         2
#define  MIPI_PD_1         1
#define  MIPI_PD_0         0


#define  MIPI_PHY_CONTROL_3               0x03
/* bit positions */
#define  MIPI_HS_PWD_CLK               7
#define  MIPI_HS_RT_CLK                6
#define  MIPI_PD_CLK                   5
#define  MIPI_CLK_RT_MANUAL_PD_EN      4
#define  MIPI_CLK_HS_MANUAL_PD_EN      3
#define  MIPI_CLK_DET_DET_BYPASS       2
#define  MIPI_CLK_MISS_CTRL            1
#define  MIPI_PD_LPTX_CH_MANUAL_PD_EN  0


#define  MIPI_LANE_CTRL_0                0x05
/* bit positions */
#define  MIPI_DATA_REVERSE             7
#define  MIPI_SYNC_LEAD_REVERSE        6
#define  MIPI_FORCE_TIME_LPX           5
#define  MIPI_BYPASS_WAKE_UP           4
#define  MIPI_DESKEW_EN                3
#define  MIPI_EOTP_EN                  2
#define  MIPI_ACTIVE_LANE              0
/* bit[1:0] - 00: 1 lane, 01: 2 lanes, 10: 3 lanes, 11: 4 lanes */
#define  MIPI_TIME_HS_PRPR               0x08

/* After MIPI RX protocol layer received this many video frames, */
/* protocol layer starts to reconstruct video stream from PHY */
#define  MIPI_VIDEO_STABLE_CNT           0x0A


#define  MIPI_LANE_CTRL_10               0x0F
/* bit positions */
#define  MIPI_SW_RESET_N         7
#define  MIPI_ERROR_REPORT_EN    6
#define  MIPI_LANE_SWAP          4
/* bit[5:4] - Debug mode to swap MIPI data lanes */
#define  MIPI_POWER_DOWN         3
#define  MIPI_ECC_ERR_CLR        2
#define  MIPI_PD_LPRX_CLK        1
#define  MIPI_9B8B_EN            0

#define  MIPI_RX_REG0               0x10
#define  MIPI_RX_REG1               0x11
#define  MIPI_RX_REG2               0x12
#define  MIPI_RX_REG3               0x13
#define  MIPI_RX_REG4               0x14
/* bit positions */
#define  hsrx_rterm        4
/* MIPI_RX_REGx[7:4] is hsrx_rterm[3:0], */
/* x=0~4; x=0~3 are for data lanes 0~3, x=4 is for clock */

/* defines the HS RX termination impedance: */
/*      0000: 125 Ohm */
/*      0110: 100 Ohm (default) */
/*      1111:  75 Ohm  */
#define  sel_hs_dly        1
/* MIPI_RX_REGx[3:1] is sel_hs_dly[2:0],*/
/* x=0~4; x=0~3 are for data lanes 0~3, x=4 is for clock */
/* defines the hsrx data delay; adjust the data/clock edge timing*/
/*      000: no timing adjust*/
/*      001: add 80ps delay */
/*      ... */
/*      100: add 4*80ps delay (default)*/
/*      ... */
/*      111: add 7*80ps delay */
/*      For every step, the adjust delay is about 80ps @TT, 130ps@SS, 54ps@FF */
/*      note: The descriptions above come from an internal documentation */
/*      mi2-analog_pinlist_v1.4.xlsx. */
/*      Test shows the actual delay is around 110ps per step for TT chips.*/
/* MIPI_RX_REGx[0] is reserved. */

#define  MIPI_RX_REG5               0x15
/* bit positions */
#define  ref_sel_lpcd      6  /* MIPI_RX_REG5[7:6] is ref_sel_lpcd[1:0] */
/*  define the lpcd reference voltage */
/*       00: 250 mV */
/*       01: 300 mV (default) */
/*       10: 350 mV */
/*       11: 400 mV  */
#define  ref_sel_lprx      4  /* MIPI_RX_REG5[5:4] is ref_sel_lprx[1:0] */
/*  define the lprx reference voltage */
/*       00: 650 mV */
/*       01: 700 mV (default) */
/*       10: 750 mV */
/*       11: 800 mV */
/* MIPI_RX_REG5[3] is reserved. */
#define  sel_lptx_term     0  /* MIPI_RX_REG5[2:0] is sel_lptx_term[2:0] */
/*  define the lptx termination (thermal code) */
/*       111: 120 Ohm */
/*       011: 150 Ohm */
/*       001: 200 Ohm (default) */
/*       000: 310 Ohm */

#define  MIPI_RX_REG7               0x17
/* bit positions */
#define  dpi_ck_delay      5  /* MIPI_RX_REG7[7:5] is dpi ck delay config */
/*  0xx no delay */
/*     100 120 ps */
/*     101 120*2 ps (default) */
/*     110 120*3 ps */
/*     111 120*4 ps  */
/* MIPI_RX_REG7[4] is test enable: 1: enable, 0: disable */
#define  test_enable       4
/* MIPI_RX_REG7[3:0] is test signal selection */
#define  test_signal_sel   0
/*         dtesto / atesto */
/*       0000 pd_rt_ch2 / vref_lprx */
/*       0001 pd_lpcd_ch2 / vref_lpcd */
/*       0010 pd_lprx_ch2 / vgate_lp */
/*       0011 pd_lptx_ch2 / avss */
/*       0100 swap_ch2 */
/*       0101 pd_rx */
/*       0110 pd_ch2 */
/*       0111 pd_hsrx_ch2 */
/*       1000 lp_outn_ch2 */
/*       1001 lp_outp_ch2 */
/*       1010 lpcd_outn_ch2 */
/*       1011 lpcd_outp_ch2 */
/*       1100 data_ch2<2> */
/*       1101 data_ch2<1> */
/*       1110 data_ch2<0> */
/*       1111 ck_byte_r  */

#define  MIPI_DIGITAL_ADJ_1   0x1B
/* bit positions */
/* bit[7:4]: Integral part ratio of the adjust loop */
#define  DIFF_I_RATIO      4
/*  0~7: 1/(2^(n+3)) */
/*    >= 8: reserved */
/* bit[3:0]: The total adjust loop ratio to the feedback block */
#define  DIFF_K_RATIO      0
/*  0~0xF: 1/(2^n) */

#define  MIPI_PLL_M_NUM_23_16   0x1E
#define  MIPI_PLL_M_NUM_15_8    0x1F
#define  MIPI_PLL_M_NUM_7_0     0x20
#define  MIPI_PLL_N_NUM_23_16   0x21
#define  MIPI_PLL_N_NUM_15_8    0x22
#define  MIPI_PLL_N_NUM_7_0     0x23

#define  MIPI_DIGITAL_PLL_6     0x2A
/* bit positions */
/* bit[7:6]: VCO band control, only effective */
/* when MIPI_PLL_FORCE_BAND_EN (0x84:0x2B[6]) is 1 */
#define  MIPI_PLL_BAND_REG       6
/* f_vco frequency: */
/*      band 0:    1 ~ 1.15 GHz */
/*      band 1: 1.15 ~ 1.3  GHz */
/*      band 2:  1.3 ~ 1.5  GHz */
/*      band 3:  1.5 ~ 2.0  GHz */

/* band 3 is usable but not recommended, as using band 3: */
/* a) The power consumption is higher. */
/* b) For SS corner chips, VCO may not work at 2GHz. */
/* bit 5 is reserved */
#define  MIPI_M_NUM_READY        0x10
#define  MIPI_N_NUM_READY        0x08
#define  STABLE_INTEGER_CNT_EN   0x04
#define  MIPI_PLL_TEST_BIT       0
/* bit[1:0]: test point output select - */
/* 00: VCO power, 01: dvdd_pdt, 10: dvdd, 11: vcox */

#define  MIPI_DIGITAL_PLL_7     0x2B
/* bit positions */
#define  MIPI_PLL_FORCE_N_EN     7
#define  MIPI_PLL_FORCE_BAND_EN  6

#define  MIPI_PLL_VCO_TUNE_REG   4
/* bit[5:4]: VCO metal capacitance - */
/* 00: +20% fast, 01: +10% fast (default), 10: typical, 11: -10% slow */
#define  MIPI_PLL_VCO_TUNE_REG_VAL   0x30
/* bit[5:4]: VCO metal capacitance */

#define  MIPI_PLL_PLL_LDO_BIT    2
/* bit[3:2]: vco_v2i power - */
/* 00: 1.40V, 01: 1.45V (default), 10: 1.50V, 11: 1.55V */
#define  MIPI_PLL_RESET_N        0x02
#define  MIPI_FRQ_FORCE_NDET     0



#define  MIPI_ALERT_CLR_0     0x2D
/* bit positions */
#define  HS_link_error_clear      7
/* This bit itself is S/C, and it clears 0x84:0x31[7] */


#define  MIPI_ALERT_OUT_0     0x31
/* bit positions */
#define  check_sum_err_hs_sync      7
/* This bit is cleared by 0x84:0x2D[7] */

#define  MIPI_DIGITAL_PLL_8   0x33
/* bit positions */
#define  MIPI_POST_DIV_VAL     4
/* n means divided by (n+1), n = 0~15 */
#define  MIPI_EN_LOCK_FRZ      3
#define  MIPI_FRQ_COUNTER_RST  2
#define  MIPI_FRQ_SET_REG_8    1
/* bit 0 is reserved */

#define  MIPI_DIGITAL_PLL_9   0x34

#define  MIPI_DIGITAL_PLL_16  0x3B
/* bit positions */
#define  MIPI_FRQ_FREEZE_NDET          7
#define  MIPI_FRQ_REG_SET_ENABLE       6
#define  MIPI_REG_FORCE_SEL_EN         5
#define  MIPI_REG_SEL_DIV_REG          4
#define  MIPI_REG_FORCE_PRE_DIV_EN     3
/* bit 2 is reserved */
#define  MIPI_FREF_D_IND               1
#define  REF_CLK_27000kHz    1
#define  REF_CLK_19200kHz    0
#define  MIPI_REG_PLL_PLL_TEST_ENABLE  0

#define  MIPI_DIGITAL_PLL_18  0x3D
/* bit positions */
#define  FRQ_COUNT_RB_SEL       7
#define  REG_FORCE_POST_DIV_EN  6
#define  MIPI_DPI_SELECT        5
#define  SELECT_DSI  1
#define  SELECT_DPI  0
#define  REG_BAUD_DIV_RATIO     0

#define  H_BLANK_L            0x3E
/* for DSC only */

#define  H_BLANK_H            0x3F
/* for DSC only; note: bit[7:6] are reserved */

#define  MIPI_SWAP  0x4A
/* bit positions */
#define  MIPI_SWAP_CH0    7
#define  MIPI_SWAP_CH1    6
#define  MIPI_SWAP_CH2    5
#define  MIPI_SWAP_CH3    4
#define  MIPI_SWAP_CLK    3
/* bit[2:0] are reserved */

#define  MIPI_HS_FIRST_PACKET_HEADER     0x67
#define  MIPI_HS_FIRST_PACKET_WIDTH_L    0x68
#define  MIPI_HS_FIRST_PACKET_WIDTH_H    0x69
#define  MIPI_HS_FIRST_PACKET            0x6A



/*========================= END of I2C Address 0x84 ================*/


/*  DEV_ADDR = 0x7A or 0x7B , MIPI Rx Registers*/
/*  DEV_ADDR = 0x7A or 0x7B , MIPI Rx Registers*/
#define MIPI_ANALOG_PWD_CTRL0				 0x00
#define MIPI_ANALOG_PWD_CTRL1			        0x01
#define MIPI_ANALOG_PWD_CTRL2			        0x02



/*DPCD regs*/
#define DPCD_DPCD_REV                  0x00
#define DPCD_MAX_LINK_RATE             0x01
#define DPCD_MAX_LANE_COUNT            0x02


/***************************************************************/

/*Comands status*/
enum interface_status {
	CMD_SUCCESS,
	CMD_REJECT,
	CMD_FAIL,
	CMD_BUSY,
	CMD_STATUS
};

enum PD_MSG_TYPE {
	TYPE_PWR_SRC_CAP = 0x00,
	TYPE_PWR_SNK_CAP = 0x01,
	TYPE_DP_SNK_IDENTITY = 0x02,
	TYPE_SVID = 0x03,
	TYPE_GET_DP_SNK_CAP = 0x04,
	TYPE_ACCEPT = 0x05,
	TYPE_REJECT = 0x06,
	TYPE_PSWAP_REQ = 0x10,
	TYPE_DSWAP_REQ = 0x11,
	TYPE_GOTO_MIN_REQ = 0x12,
	TYPE_VCONN_SWAP_REQ = 0x13,
	TYPE_VDM = 0x14,
	TYPE_DP_SNK_CFG = 0x15,
	TYPE_PWR_OBJ_REQ = 0x16,
	TYPE_PD_STATUS_REQ = 0x17,
	TYPE_DP_ALT_ENTER = 0x19,
	TYPE_DP_ALT_EXIT = 0x1A,
	TYPE_GET_SNK_CAP = 0x1B,
	TYPE_SOP_PRIME = 0x1C,
	TYPE_SOP_DOUBLE_PRIME = 0x1D,
	TYPE_RESPONSE_TO_REQ = 0xF0,
	TYPE_SOFT_RST = 0xF1,
	TYPE_HARD_RST = 0xF2,
	TYPE_RESTART = 0xF3,
	TYPE_EXT_SRC_CAP = 0xA1, /* Source_Capabilities_Extended*/
	TYPE_EXT_SRC_STS = 0xA2, /* Source_Status*/
	TYPE_EXT_GET_BATT_CAP  = 0xA3, /* Get_Battery_Cap*/
	TYPE_EXT_GET_BATT_STS = 0xA4, /* Get_Battery_ Status*/
	TYPE_EXT_BATT_CAP = 0xA5, /* Battery_Capabilities*/
	TYPE_EXT_GET_MFR_INFO = 0xA6, /* Get_Manufacturer_Info*/
	TYPE_EXT_MFR_INFO = 0xA7, /* Manufacturer_Info*/
	TYPE_EXT_PDFU_REQUEST = 0xA8, /* FW update Request*/
	TYPE_EXT_PDFU_RESPONSE = 0xA9, /* FW update Response*/
	TYPE_EXT_BATT_STS = 0xAA, /* PD_DATA_BATTERY_STATUS*/
	TYPE_EXT_ALERT = 0xAB, /* PD_DATA_ALERT*/
	TYPE_EXT_NOT_SUPPORTED = 0xAC, /* PD_CTRL_NOT_SUPPORTED*/
	TYPE_EXT_GET_SRC_CAP = 0xAD, /* PD_CTRL_GET_SOURCE_CAP_EXTENDED*/
	TYPE_EXT_GET_SRC_STS = 0xAE, /* PD_CTRL_GET_STATUS*/
	TYPE_EXT_FR_SWAP = 0xAF,  /* PD_CTRL_FR_SWAP*/
	TYPE_FR_SWAP_SIGNAL = 0xB0, /* Fast Role Swap signal*/
};

/* PDO : Power Data Object
 * 1. The vSafe5V Fixed Supply Object shall always be the first object.
 * 2. The remaining Fixed Supply Objects,
 * if present, shall be sent in voltage order; lowest to highest.
 * 3. The Battery Supply Objects,
 * if present shall be sent in Minimum Voltage order; lowest to highest.
 * 4. The Variable Supply (non battery) Objects,
 * if present, shall be sent in Minimum Voltage order; lowest to highest.
 */
#define PDO_TYPE_FIXED ((u32)0 << 30)
#define PDO_TYPE_BATTERY ((u32)1 << 30)
#define PDO_TYPE_VARIABLE ((u32)2 << 30)
#define PDO_TYPE_MASK ((u32)3 << 30)
#define PDO_FIXED_DUAL_ROLE ((u32)1 << 29)	/* Dual role device */
#define PDO_FIXED_SUSPEND ((u32)1 << 28)	/* USB Suspend supported */
#define PDO_FIXED_EXTERNAL ((u32)1 << 27)	/* Externally powered */
#define PDO_FIXED_COMM_CAP ((u32)1 << 26)	/* USB Communications Capable */
#define PDO_FIXED_DATA_SWAP ((u32)1 << 25)	/* Data role swap command */
#define PDO_FIXED_PEAK_CURR ((u32)1 << 20)	/* [21..20] Peak current */
/* Voltage in 50mV units */
#define PDO_FIXED_VOLT(mv) (u32)((((u32)mv)/50) << 10)
/* Max current in 10mA units */
#define PDO_FIXED_CURR(ma) (u32)((((u32)ma)/10))

/*build a fixed PDO packet*/
#define PDO_FIXED(mv, ma, flags) \
	(PDO_FIXED_VOLT(mv)\
	| PDO_FIXED_CURR(ma)\
	| (flags))

/*Pos in Data Object, the first index number begin from 0 */
#define PDO_INDEX(n, dat) (dat << (n * PD_ONE_DATA_OBJECT_SIZE*sizeof(u8)))
#define PDO_VAR_MAX_VOLT(mv) ((((mv) / 50) & 0x3FF) << 20)
#define PDO_VAR_MIN_VOLT(mv) ((((mv) / 50) & 0x3FF) << 10)
#define PDO_VAR_OP_CURR(ma) ((((ma) / 10) & 0x3FF) << 0)

#define PDO_VAR(min_mv, max_mv, op_ma) \
	(PDO_VAR_MIN_VOLT(min_mv) | PDO_VAR_MAX_VOLT(max_mv) | \
	PDO_VAR_OP_CURR(op_ma) | PDO_TYPE_VARIABLE)
#define PDO_BATT_MAX_VOLT(mv) ((((mv) / 50) & 0x3FF) << 20)
#define PDO_BATT_MIN_VOLT(mv) ((((mv) / 50) & 0x3FF) << 10)
#define PDO_BATT_OP_POWER(mw) ((((mw) / 250) & 0x3FF) << 0)
#define PDO_BATT(min_mv, max_mv, op_mw) \
	(PDO_BATT_MIN_VOLT(min_mv)\
	| PDO_BATT_MAX_VOLT(max_mv)\
	| PDO_BATT_OP_POWER(op_mw)\
	| PDO_TYPE_BATTERY)

#define GET_PDO_TYPE(PDO) ((PDO & PDO_TYPE_MASK) >> 30)
#define GET_PDO_FIXED_DUAL_ROLE(PDO) ((PDO & PDO_FIXED_DUAL_ROLE) >> 29)
#define GET_PDO_FIXED_SUSPEND(PDO) ((PDO & PDO_FIXED_SUSPEND) >> 28)
#define GET_PDO_FIXED_EXTERNAL(PDO) ((PDO & PDO_FIXED_EXTERNAL) >> 27)
#define GET_PDO_FIXED_COMM_CAP(PDO) ((PDO & PDO_FIXED_COMM_CAP) >> 26)
#define GET_PDO_FIXED_DATA_SWAP(PDO) ((PDO & PDO_FIXED_DATA_SWAP) >> 25)
#define GET_PDO_FIXED_PEAK_CURR(PDO) ((PDO >> 20) & 0x03)

#define GET_PDO_FIXED_VOLT(PDO) (((PDO >> 10) & 0x3FF) * 50)
#define GET_PDO_FIXED_CURR(PDO) ((PDO & 0x3FF) * 10)
#define GET_VAR_MAX_VOLT(PDO) (((PDO >> 20) & 0x3FF) * 50)
#define GET_VAR_MIN_VOLT(PDO) (((PDO >> 10) & 0x3FF) * 50)
#define GET_VAR_MAX_CURR(PDO) ((PDO & 0x3FF) * 10)
#define GET_BATT_MAX_VOLT(PDO) (((PDO >> 20) & 0x3FF) * 50)
#define GET_BATT_MIN_VOLT(PDO) (((PDO >> 10) & 0x3FF) * 50)
#define GET_BATT_OP_POWER(PDO) (((PDO) & 0x3FF) * 250)

#define INTERFACE_TIMEOUT 30
#define InterfaceSendBuf_Addr 0xc0
#define InterfaceRecvBuf_Addr 0xe0
#define YES     1
#define NO      0
#define ERR_CABLE_UNPLUG -1
#define PD_ONE_DATA_OBJECT_SIZE  4
#define PD_MAX_DATA_OBJECT_NUM  7
#define VDO_SIZE (PD_ONE_DATA_OBJECT_SIZE * PD_MAX_DATA_OBJECT_NUM)
#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP)

/*5000mv voltage*/
#define PD_VOLTAGE_5V 5000

#define PD_MAX_VOLTAGE_20V 20000
#define PD_MAX_VOLTAGE_21V 21000

/*0.9A current */
#define PD_CURRENT_900MA   900
#define PD_CURRENT_1500MA 1500

#define PD_CURRENT_3A   3000

#define PD_POWER_15W  15000

#define PD_POWER_60W  60000

/* RDO : Request Data Object */
#define RDO_OBJ_POS(n)             (((u32)(n) & 0x7) << 28)
#define RDO_POS(rdo)               ((((32)rdo) >> 28) & 0x7)
#define RDO_GIVE_BACK              ((u32)1 << 27)
#define RDO_CAP_MISMATCH           ((u32)1 << 26)
#define RDO_COMM_CAP               ((u32)1 << 25)
#define RDO_NO_SUSPEND             ((u32)1 << 24)
#define RDO_FIXED_VAR_OP_CURR(ma)  (((((u32)ma) / 10) & 0x3FF) << 10)
#define RDO_FIXED_VAR_MAX_CURR(ma) (((((u32)ma) / 10) & 0x3FF) << 0)

#define RDO_BATT_OP_POWER(mw)      (((((u32)mw) / 250) & 0x3FF) << 10)
#define RDO_BATT_MAX_POWER(mw)     (((((u32)mw) / 250) & 0x3FF) << 10)

#define RDO_FIXED(n, op_ma, max_ma, flags)	\
	(RDO_OBJ_POS(n) | (flags) |		\
	RDO_FIXED_VAR_OP_CURR(op_ma) |		\
	RDO_FIXED_VAR_MAX_CURR(max_ma))

#ifdef DEBUG_LOG_OUTPUT
#define TRACE pr_info
#define TRACE1 pr_info
#define TRACE2 pr_info
#define TRACE3 pr_info
#else
#define TRACE(fmt, arg...)
#define TRACE1(fmt, arg...)
#define TRACE2(fmt, arg...)
#define TRACE3(fmt, arg...)
#endif

#define BYTE unsigned char
#define unchar unsigned char
#define uint unsigned int
#define ulong unsigned long

enum {
	VIDEO_3D_NONE		= 0x00,
	VIDEO_3D_FRAME_PACKING		= 0x01,
	VIDEO_3D_TOP_AND_BOTTOM		= 0x02,
	VIDEO_3D_SIDE_BY_SIDE		= 0x03,
};

struct RegisterValueConfig {
	unsigned char slave_addr;
	unsigned char reg;
	unsigned char val;
};

#define AUX_ERR  1
#define AUX_OK   0

#define MAX_BUF_CNT 6
#define INTR_MASK_SETTING 0x0

enum HDCP_CAP_TYPE {
	NO_HDCP_SUPPORT = 0x00,
	HDCP14_SUPPORT = 0x01,
	HDCP22_SUPPORT = 0x02,
	HDCP_ALL_SUPPORT = 0x03
};

#define XTAL_FRQ  27000000UL  /* MI-2 clock frequency in Hz: 27 MHz*/

#define FLASH_LOAD_STA 0x05
#define FLASH_LOAD_STA_CHK	(1<<7)

#endif  /* __MI2_REG_H__ */
