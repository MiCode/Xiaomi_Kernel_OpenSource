/*
*Copyright (C) 2013-2014 Silicon Image, Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation version 2.
* This program is distributed AS-IS WITHOUT ANY WARRANTY of any
* kind, whether express or implied; INCLUDING without the implied warranty
* of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
* See the GNU General Public License for more details at
* http://www.gnu.org/licenses/gpl-2.0.html.
*/

#ifndef _HEADER_H_
#define _HEADER_H_

#define BASE_ADDRESS 0x0000

#define SABER_REG (BASE_ADDRESS | 0x000)

/* *************************************************************************** */
/* SABER_REG. Address: 68 */
/* Saber ID0 Register */
#define REG_ADDR__SABER_ID0                                              (SABER_REG | 0x0000)
    /* (ReadOnly, Bits 7:0) */
    /* Saber ID: This register holds bits [7:0] of the Saber ID. */
#define BIT_MSK__SABER_ID0__RO_SABER_ID_B7_B0                                     0xFF

/* Saber ID1 Register */
#define REG_ADDR__SABER_ID1                                              (SABER_REG | 0x0001)
    /* (ReadOnly, Bits 7:0) */
    /* Saber ID: This register holds bits [15:8] of the Saber ID. */
#define BIT_MSK__SABER_ID1__RO_SABER_ID_B15_B8                                    0xFF

/* Saber ID2 Register */
#define REG_ADDR__SABER_ID2                                              (SABER_REG | 0x0002)
    /* (ReadOnly, Bits 7:0) */
    /* Saber ID: This register holds bits [23:16] of the Saber ID. */
#define BIT_MSK__SABER_ID2__RO_SABER_ID_B23_B16                                   0xFF

/* Saber ID3 Register */
#define REG_ADDR__SABER_ID3                                              (SABER_REG | 0x0003)
    /* (ReadOnly, Bits 7:0) */
    /* Saber ID: This register holds bits [31:24] of the Saber ID. */
#define BIT_MSK__SABER_ID3__RO_SABER_ID_B31_B24                                   0xFF

/* Saber Control0 Register */
#define REG_ADDR__SABER_CTRL0                                            (SABER_REG | 0x0004)
    /* (ReadWrite, Bits 0) */
    /* 2.4MHz External Test Clock Select: This bit selects the Clock driven to the Digital CC-PD.
    This bit when HIGH, puts the INT Pad to Input mode and expects the external test clock to be
    driven on the INT Pad. The external 2.4 MHz Clock from input path of INT Pad is routed to the
    Digital CC-PD.  This bit when LOW, doesn't put the INT Pad in Input Mode and 2.4MHz Clock
    from Analog CC-PD is routed to the Digital CC-PD. */
#define BIT_MSK__SABER_CTRL0__RI_TEST_CLK_SEL                                       0x01
    /* (ReadWrite, Bits 1) */
    /* Force INT Pad to Output direction: This bit when HIGH forces the Saber INT Pad to Output
    direction when System bReset is Not asserted and Saber is in Functional(i.e. Not Test) mode of
    operation. LOW on this bit, doesn't mean the INT Pad direction is Input.  0 - No Impact on INT
    Pad direction i.e. controlled by Saber System hardware state 1 - INT Pad direction is Output with
    Priority to Saber System Reset and Scan Mode */
#define BIT_MSK__SABER_CTRL0__RI_FORCE_INT_OEN                                      0x02

/* Saber Interrupt Status Register */
#define REG_ADDR__SABER_INTR                                             (SABER_REG | 0x0008)
    /* (ReadOnly, Bits 0) */
    /* Digital CCPD block ORed Interrupt: This bit when HIGH indicates that one or more of the
    Digital CCPD block Interrupts is asserted. */
#define BIT_MSK__SABER_INTR__RI_INTR_STAT0                                         0x01
    /* (ReadOnly, Bits 1) */
    /* Calibration block ORed Interrupt: This bit when HIGH indicates that one or more of the
    Calibration block Interrupts is asserted. */
#define BIT_MSK__SABER_INTR__RI_INTR_STAT1                                         0x02

/* Saber Interrupt Status Mask Register */
#define REG_ADDR__SABER_INTR_MASK                                        (SABER_REG | 0x0009)
    /* (ReadWrite, Bits 0) */
    /* Enable SABER_INTR[0]: 0 - Disable 1 - Enable */
#define BIT_MSK__SABER_INTR_MASK__RI_INTR_MASK0                                         0x01
    /* (ReadWrite, Bits 1) */
    /* Enable SABER_INTR[1]: 0 - Disable 1 - Enable */
#define BIT_MSK__SABER_INTR_MASK__RI_INTR_MASK1                                         0x02

/* Saber Soft Reset Register */
#define REG_ADDR__CLKRST_SRST                                            (SABER_REG | 0x0010)
    /* (ReadWrite, Bits 0) */
    /* Register Block Soft Reset: This register bit controls the Soft Reset of the Saber Register
    Blocks. Logic implementation for this bit is as described below. 0 - No Soft Reset asserted
    i.e. Normal function 1 - Soft Reset asserted i.e. Register blocks are in Reset assertion state
    NOTE: This reset control does not reset the Clock-Reset Registers i.e. clkrst_reg as doing so
    will put the logic in Reset state forever. */
#define BIT_MSK__CLKRST_SRST__RI_PRG_SRST                                           0x01

/* Calibration Control Register */
#define REG_ADDR__CALIB_CTRL                                             (SABER_REG | 0x0018)
    /* (ReadWrite, Bits 0) */
    /* Calibration Start: This bit controls the Start of Oscillator Clock Calibration. On observing
    a rising edge on this bit, Calibration logic starts calibrating the Clock.  Set this bit HIGH after
    all the necessary Calibration related register programming is done.  NOTE: Making this bit LOW
    has no impact on Calibration i.e. already initiated Calibration does not Stop on falling edge of this bit. */
#define BIT_MSK__CALIB_CTRL__RI_CALIB_START                                        0x01
    /* (ReadWrite, Bits 1) */
    /* Software Calibration Valid: This bit controls the selection of Calibration Data from OTP or
    Software programmed value in the registers CALIB_SW_DATA. Firmware can use this bit to
    override the Calibration Data from OTP and enable earlier Calibration Locked condition.
    This bit is more useful in routing correct value to the Analog Oscillator Clock generation block
    in the case of any OTP Write/Read corruption. It can also used in debug cases where calibration
    data is not of much concern and earlier calibration locking is required.  This bit when HIGH,
    routes the value programmed in bits [4:0] of OTP_WR_DATA0 register to the Oscillator Clock
    generation Analog CC-PD block. Apart from this, the bit controls the Calibration Locked condition
    of the design.  NOTE: Program this bit HIGH only after the required programming is done to the
    OTP_WR_DATA registers. */
#define BIT_MSK__CALIB_CTRL__RI_CALIB_VALID                                        0x02
    /* (ReadWrite, Bits 2) */
    /* Hardware Calibration Overwrite: This bit controls the selection of Calibration Data to be
    written into the OTP Memory.  This bit when HIGH, overrides the Calibration Value obtained
    from the Hardware Calibration logic against the OTP Write Data programmed into bits [4:0] of
    OTP_WR_DATA0 register.  This bit is provided as a safety measure to bypass the Hardware
    Calibration Value in case of any miscomputation in the Calibration logic.  NOTE: Program this
    bit HIGH; before or with ri_otp_wr bit of CALIB_CTRL register; if Software programmed value
    has to be written into the OTP. */
#define BIT_MSK__CALIB_CTRL__RI_CALIB_OVWR                                         0x04
    /* (ReadWrite, Bits 3) */
    /* OTP Write: This bit controls the Start of Writing Data into the OTP Memory. On observing
    a rising edge on this bit, OTP Controller starts Writing the data as obtained after programming
    software controls into the OTP Memory.  Set this bit HIGH as per one of the below scenarios
    intended by the firmware. 1. After observing a HIGH on the ro_calib_done Interrupt of CALIB_INTR
    register if Hardware Calibration value has to be written into the OTP Memory. 2. If Software
    programmed value has to be written into the OTP Memory; set this bit HIGH after the required
    registers are programmed.  NOTE: Making this bit LOW has no impact on the OTP Controller
    i.e. already initiated Writes to the OTP Memory do not Stop on falling edge of this bit. */
#define BIT_MSK__CALIB_CTRL__RI_OTP_WR                                             0x08
    /* (ReadWrite, Bits 7) */
    /* Calibration Block Soft Reset: This register bit controls the Soft Reset of the Calibration Block.
    Logic implementation for this bit is as described below. 0 - No Soft Reset asserted i.e. Normal
    function 1 - Soft Reset asserted i.e. Calibration block is in Reset assertion state */
#define BIT_MSK__CALIB_CTRL__RI_CALIB_SRST                                         0x80

/* Calibration Lock Count0 Register */
#define REG_ADDR__CALIB_LOCK_CNT0                                        (SABER_REG | 0x0019)
    /* (ReadWrite, Bits 7:0) */
    /* Calibration Lock Count: This register controls the time where Calibration finishes. This register
    indicates the number of Calibration Clocks to be counted to finish Calibration.  Program this register
    according to the Time required to Calibrate and the Frequency of Calibration Clock. */
#define BIT_MSK__CALIB_LOCK_CNT0__RI_CALIB_LOCK_CNT_B7_B0                               0xFF

/* Calibration Lock Count1 Register */
#define REG_ADDR__CALIB_LOCK_CNT1                                        (SABER_REG | 0x001A)
    /* (ReadWrite, Bits 7:0) */
    /* Calibration Lock Count: This register controls the time where Calibration finishes. This register
    indicates the number of Calibration Clocks to be counted to finish Calibration.  Program this register
    according to the Time required to Calibrate and the Frequency of Calibration Clock. */
#define BIT_MSK__CALIB_LOCK_CNT1__RI_CALIB_LOCK_CNT_B15_B8                              0xFF

/* Calibration Lock Count2 Register */
#define REG_ADDR__CALIB_LOCK_CNT2                                        (SABER_REG | 0x001B)
    /* (ReadWrite, Bits 5:0) */
    /* Calibration Lock Count: This register controls the time where Calibration finishes. This register
    indicates the number of Calibration Clocks to be counted to finish Calibration.  Program this register
    according to the Time required to Calibrate and the Frequency of Calibration Clock. */
#define BIT_MSK__CALIB_LOCK_CNT2__RI_CALIB_LOCK_CNT_B21_B16                             0x3F

/* Calibration Reference Clock Count Register */
#define REG_ADDR__CALIB_REF_CNT                                          (SABER_REG | 0x001D)
    /* (ReadWrite, Bits 7:0) */
    /* Calibration Reference Clock Count: This register controls the time where Calibration tuning
    initiates for a cycle of Calibration finished. This register indicates the number of Calibration Clocks
    to be counted to finish a Calibration cycle.  Program this register according to the Time required
    to tune to a Calibration cycle based on the Calibration and Oscillator Clocks. */
#define BIT_MSK__CALIB_REF_CNT__RI_CALIB_REF_CNT                                      0xFF

/* Calibration Oscillator Clock Count Register */
#define REG_ADDR__CALIB_OSC_CNT                                          (SABER_REG | 0x001E)
    /* (ReadWrite, Bits 7:0) */
    /* Calibration Oscillator Clock Count: This register controls the time where Calibration tuning
    initiates for a cycle of Calibration finished. This register indicates the number of Oscillator Clocks
    to be counted to finish a Calibration cycle.  Program this register according to the Time required
    to tune to a Calibration cycle based on the Calibration and Oscillator Clocks.  In ideal cases,
    default value of this register is 24 for Reference Clock at 2MHz and Oscillator Clock at 2.4MHz.
    In order to accommodate synchronization delays from Oscillator clock to Reference clock, the
    default value is decided as 'd25 to reduce Calibration oscillations. */
#define BIT_MSK__CALIB_OSC_CNT__RI_CALIB_OSC_CNT                                      0xFF

/* Hardware Calibration Value Register */
#define REG_ADDR__CALIB_HW_VAL                                           (SABER_REG | 0x001F)
    /* (ReadOnly, Bits 4:0) */
    /* Hardware Calibration Value: This register indicates the Hardware computed Calibration Value.
    Read this register value if Hardware Calibration is Done i.e. when ri_calib_done Interrupt in
    CALIB_INTR register is asserted. */
#define BIT_MSK__CALIB_HW_VAL__RO_CALIB_HW_VAL                                       0x1F

/* Calibration Interrupt Status Register */
#define REG_ADDR__CALIB_INTR                                             (SABER_REG | 0x0020)
    /* (ReadWrite, Bits 0) */
    /* Calibration Done: This Interrupt is asserted when Hardware initiated Calibration is Done.
    This Interrupt should assert only after the programmed Reference Clock cycles in CALIB_LOCK_CNT
    registers is counted.  Write 1 to Clear. */
#define BIT_MSK__CALIB_INTR__RI_INTR_STAT0                                         0x01
    /* (ReadWrite, Bits 1) */
    /* Calibration Locked: This Interrupt is asserted when Calibration Locked bit from OTP Memory
    is observed HIGH on reading the OTP Memory after reset deassertion.  Write 1 to Clear. */
#define BIT_MSK__CALIB_INTR__RI_INTR_STAT1                                         0x02
    /* (ReadWrite, Bits 6) */
    /* OTP Write Done: This Interrupt is asserted when the OTP Programming is done.
    This Interrupt helps the firmware know when the OTP Programming has finished.  Write 1 to Clear. */
#define BIT_MSK__CALIB_INTR__RI_INTR_STAT6                                         0x40
    /* (ReadWrite, Bits 7) */
    /* OTP Read Done: This Interrupt is asserted when the OTP read after reset deassertion is done.
    This Interrupt helps the firmware know when to Read the OTP Read Data registers.  Write 1 to Clear. */
#define BIT_MSK__CALIB_INTR__RI_INTR_STAT7                                         0x80

/* Calibration Interrupt Status Mask Register */
#define REG_ADDR__CALIB_INTR_MASK                                        (SABER_REG | 0x0021)
    /* (ReadWrite, Bits 0) */
    /* Enable CALIB_INTR[0]: 0 - Disable 1 - Enable */
#define BIT_MSK__CALIB_INTR_MASK__RI_INTR_MASK0                                         0x01
    /* (ReadWrite, Bits 1) */
    /* Enable CALIB_INTR[1]: 0 - Disable 1 - Enable */
#define BIT_MSK__CALIB_INTR_MASK__RI_INTR_MASK1                                         0x02
    /* (ReadWrite, Bits 6) */
    /* Enable CALIB_INTR[6]: 0 - Disable 1 - Enable */
#define BIT_MSK__CALIB_INTR_MASK__RI_INTR_MASK6                                         0x40
    /* (ReadWrite, Bits 7) */
    /* Enable CALIB_INTR[7]: 0 - Disable 1 - Enable */
#define BIT_MSK__CALIB_INTR_MASK__RI_INTR_MASK7                                         0x80

/* Final Calibration Value Register */
#define REG_ADDR__CALIB_DATA                                             (SABER_REG | 0x0022)
    /* (ReadOnly, Bits 4:0) */
    /* Final Calibration Value: This register indicates the Final Calibration Value reaching the
    Analog CC-PD block generating the Oscillator Clock.  This is more a debug register to confirm
    the Calibration Value reaching the Analog CC-PD block generating the Oscillator Clock. */
#define BIT_MSK__CALIB_DATA__RO_CALIB_DATA                                         0x1F

/* OTP Write Data0 Register */
#define REG_ADDR__OTP_WR_DATA0                                           (SABER_REG | 0x0028)
    /* (ReadWrite, Bits 7:0) */
    /* OTP Write Data0 Register: This register carries the byte value to be written into Address
    Offset 2'h0 of the OTP Memory.  NOTE: bits [4:0] of this register are written into the OTP
    Memory only if ri_calib_ovwr bit of CALIB_CTRL register is programmed HIGH else ignored. */
#define BIT_MSK__OTP_WR_DATA0__RI_OTP_WR_DATA0                                       0xFF

/* OTP Write Data1 Register */
#define REG_ADDR__OTP_WR_DATA1                                           (SABER_REG | 0x0029)
    /* (ReadWrite, Bits 7:0) */
    /* OTP Write Data1 Register: This register carries the byte value to be written into Address
    Offset 2'h1 of the OTP Memory.  NOTE: bit [0] of this register is written into the OTP Memory
    only if ri_calib_ovwr bit of CALIB_CTRL register is programmed HIGH else ignored. */
#define BIT_MSK__OTP_WR_DATA1__RI_OTP_WR_DATA1                                       0xFF

/* OTP Write Data2 Register */
#define REG_ADDR__OTP_WR_DATA2                                           (SABER_REG | 0x002A)
    /* (ReadWrite, Bits 7:0) */
    /* OTP Write Data2 Register: This register carries the byte value to be written into Address
    Offset 2'h2 of the OTP Memory. */
#define BIT_MSK__OTP_WR_DATA2__RI_OTP_WR_DATA2                                       0xFF

/* OTP Write Data3 Register */
#define REG_ADDR__OTP_WR_DATA3                                           (SABER_REG | 0x002B)
    /* (ReadWrite, Bits 7:0) */
    /* OTP Write Data3 Register: This register carries the byte value to be written into Address
    Offset 2'h3 of the OTP Memory. */
#define BIT_MSK__OTP_WR_DATA3__RI_OTP_WR_DATA3                                       0xFF

/* OTP Read Data0 Register */
#define REG_ADDR__OTP_RD_DATA0                                           (SABER_REG | 0x002C)
    /* (ReadWrite, Bits 7:0) */
    /* OTP Read Data0 Register: This register carries the byte value read from Address
    Offset 2'h0 of the OTP Memory. This register value is valid when intr_otp_rd_done Interrupt
    of CALIB_INTR register is asserted. */
#define BIT_MSK__OTP_RD_DATA0__RI_OTP_RD_DATA_B7_B0                                  0xFF

/* OTP Read Data1 Register */
#define REG_ADDR__OTP_RD_DATA1                                           (SABER_REG | 0x002D)
    /* (ReadWrite, Bits 7:0) */
    /* OTP Read Data1 Register: This register carries the byte value read from Address
    Offset 2'h1 of the OTP Memory. This register value is valid when intr_otp_rd_done Interrupt
    of CALIB_INTR register is asserted. */
#define BIT_MSK__OTP_RD_DATA1__RI_OTP_RD_DATA_B15_B8                                 0xFF

/* OTP Read Data2 Register */
#define REG_ADDR__OTP_RD_DATA2                                           (SABER_REG | 0x002E)
    /* (ReadWrite, Bits 7:0) */
    /* OTP Read Data2 Register: This register carries the byte value read from Address
    Offset 2'h2 of the OTP Memory.  This register value is valid when intr_otp_rd_done Interrupt
    of CALIB_INTR register is asserted. */
#define BIT_MSK__OTP_RD_DATA2__RI_OTP_RD_DATA_B23_B16                                0xFF

/* OTP Read Data3 Register */
#define REG_ADDR__OTP_RD_DATA3                                           (SABER_REG | 0x002F)
    /* (ReadWrite, Bits 7:0) */
    /* OTP Read Data3 Register: This register carries the byte value read from Address
    Offset 2'h3 of the OTP Memory.  This register value is valid when intr_otp_rd_done Interrupt
    of CALIB_INTR register is asserted. */
#define BIT_MSK__OTP_RD_DATA3__RI_OTP_RD_DATA_B31_B24                                0xFF

/* SYS Register */
#define REG_ADDR__PDCCSRST                                               (SABER_REG | 0x0030)
    /* (ReadWrite, Bits 0) */
    /* Active high reset signal for PD 2.4MHz clock domain. Power on reset and should be
    controlled by software. (External synchronizer is required) */
#define BIT_MSK__PDCCSRST__REG_PD24_SRST                                         0x01
    /* (ReadWrite, Bits 1) */
    /* Active high reset signal for CC 2.4MHz clock domain. Power on reset and should be
    controlled by software. (External synchronizer is required) */
#define BIT_MSK__PDCCSRST__REG_CC24_SRST                                         0x02
    /* (ReadWrite, Bits 2) */
    /* Digital PDCC Clock Enable: This register bit controls the enabling of clock to the Digital
    CC logic and PDCC registers. Logic implementation for this bit is as described below.
    0 - Disabled i.e. Clock Stops on a LOW value 1 - Enabled i.e. Clock toggles according
    to the Analog CCPD Clock input */
#define BIT_MSK__PDCCSRST__REG_PDCC_CLK_EN                                       0x04
    /* (ReadWrite, Bits 3) */
    /* Digital PD Clock Enable: This register bit controls the enabling of clock to the Digital
    PD logic. Logic implementation for this bit is as described below. 0 - Disabled
    i.e. Clock Stops on a LOW value 1 - Enabled i.e. Clock toggles according to the Analog CCPD Clock input */
#define BIT_MSK__PDCCSRST__REG_PD_CLK_EN                                         0x08
    /* (ReadOnly, Bits 7) */
    /* general interrupt status. */
#define BIT_MSK__PDCCSRST__REG_INTR                                              0x80

/* Aggregated INT 0 Register */
#define REG_ADDR__AGGINT0                                                (SABER_REG | 0x0031)
    /* (ReadOnly, Bits 4:0) */
    /* Aggregated interrupt status. Bit 0 for H24INT0; Bit 1 for H24INT1; Bit 2 for H24INT2;
    Bit 3 for H24INT3; Bit 4 for PD24INT4; Bit 5 for PD24INT5; */
#define BIT_MSK__AGGINT0__REG_AGGRINTR_B4_B0                                    0x1F

/* PD 2.4MHz domain INTR0 Register */
#define REG_ADDR__PDCC24INT0                                             (SABER_REG | 0x0032)
    /* (ReadWrite, Bits 0) */
    /* ro_intr_prl_tx_msg_sent: PD Protocol Layer Transmitter Transmission done interrupt.
    This bit is set if transmission has been completed. Writing '1' clears the interrupt. Only valid
    when mask bit is 1. */
#define BIT_MSK__PDCC24INT0__REG_PDCC24_INTR0                                      0x01
    /* (ReadWrite, Bits 1) */
    /* ro_intr_prl_tx_error: PD Protocol Layer Transmitter Transmission retry error interrupt.
    This bit is set if the nRetryCount retry transmission fails. Writing '1' clears the interrupt. Only
    valid when mask bit is 1. */
#define BIT_MSK__PDCC24INT0__REG_PDCC24_INTR1                                      0x02
    /* (ReadWrite, Bits 2) */
    /* ro_intr_prl_tx_crcrx_error: PD Protocol Layer Transmitter CRCReceiveTimer timeout
    interrupt. This bit is set if Protocol Layer fails to receive an acknowledgment of a message
    (a GoodCRC message) within tReceive. No action is required for software. Protocol Layer
    automatically retries transmission nRetryCount times within tRetry time. See section 6.5.1
    of USB PD specification. BISTReceiveErrorTimer timeout interrupt: During BIST mode,
    this bit is set when Protocol Layer sends Test Frame but fails to receive an acknowledgment
    of a Test Frame (BIST message with a Dta Object of Returned BIST Counters) within tBISTReceive.
    See section 6.5.2 of USB PD specification. Writing '1' clears the interrupt. Only valid when
    mask bit is 1. */
#define BIT_MSK__PDCC24INT0__REG_PDCC24_INTR2                                      0x04
    /* (ReadWrite, Bits 3) */
    /* ro_intr_prl_tx_msg_discarded: PD Protocol Layer Transmitter message transmission
    discard interrupt. This bit is set if Physical Layer discared message transmission request
    due to CC being busy, and after CC becomes idle again. No action is required for software.
    Protocol Layer automatically retries transmission nRetryCount times within tRetry time.
    See section 6.5.1 of USB PD specification. Writing '1' clears the interrupt. Only valid when
    mask bit is 1. */
#define BIT_MSK__PDCC24INT0__REG_PDCC24_INTR3                                      0x08
    /* (ReadWrite, Bits 4) */
    /* ro_intr_prl_tx_hreset_error: PD Protocol Layer Transmitter HardResetCompleteTimer
    timeout interrupt. This bit is set if Protocol Layer asked the PHY Layer to send Hard Reset
    signaling and the PHY Layer is unable to send the signaling within a reasonable time due
    to a non-idle channel. Refer to Section 6.5.11 of USB PD Specification. Writing '1' clears
    the interrupt. Only valid when mask bit is 1. */
#define BIT_MSK__PDCC24INT0__REG_PDCC24_INTR4                                      0x10
    /* (ReadWrite, Bits 5) */
    /* ro_intr_prl_rx_msg_received: PD Protocol Layer Receiver Message Reception done
    interrupt. This bit is set if message has been received from the port partner or a cable.
    This bit is not set if the received message has CRC error or it is retry message (which has
    same messageID with the stored value). Refer to section 6.8.2.2 of USB PD specification.
    Writing '1' clears the interrupt. Only valid when mask bit is 1. */
#define BIT_MSK__PDCC24INT0__REG_PDCC24_INTR5                                      0x20
    /* (ReadWrite, Bits 6) */
    /* ro_intr_prl_rx_msg_duplicated: PD Protocol Layer Receiver duplicated message interrupt.
    This bit is set if the received message is duplicated. Protocol layer checks if the MessageID
    of the received message equals the stored MessageId value. No action is required for software.
    Protocol Layer automatically drops the duplicated message since the previous message is
    already forwarded to Protocol Engine. Writing '1' clears the interrupt. Only valid when mask
    bit is 1. */
#define BIT_MSK__PDCC24INT0__REG_PDCC24_INTR6                                      0x40
    /* (ReadWrite, Bits 7) */
    /* ro_intr_phy_rx_crc_error: PD Physical Layer Receiver reception CRC error interrupt. This
    bit is set if there is CRC error in the received message. No action is required for software.
    In this case, Protocol Layer automatically skips sending GoodCRC message and waits for
    message retransmission from the Port Partner. Transmission CRC error interrupt. This bit is
    also set if there is CRC error in the returning GoodCRC message. Writing '1' clears the interrupt.
    Only valid when mask bit is 1. */
#define BIT_MSK__PDCC24INT0__REG_PDCC24_INTR7                                      0x80

/* 2.4MHz domain INTR1 Register */
#define REG_ADDR__PDCC24INT1                                             (SABER_REG | 0x0033)
    /* (ReadWrite, Bits 0) */
    /* ro_intr_prl_tx_petx_discarded_tx_busy: PD Protocol Layer Transmitter interrupt. This bit
    is set if message request from Policy Engine is discarded since Protocol Layer Tx is busy. */
#define BIT_MSK__PDCC24INT1__REG_PDCC24_INTR8                                      0x01
    /* (ReadWrite, Bits 1) */
    /* ro_intr_prl_tx_petx_discarded_rx_busy: PD Protocol Layer Transmitter interrupt. This bit
    is set if message request from Policy Engine is discarded since Protocol Layer Receiver received
    a message. Please refer to state 'PRL_Tx_Discard_Message' described in section 6.8.2.1.10 of
    USB PD specification. This bit is also set if message request from PE is discarded since Protocol
    Layer Receiver receives Soft Reset from PHY or 'Exit from Hard Reset'. Please refer to state
    'PRL_Tx_PHY_Layer_Reset' described in section 6.8.2.1.1 of USB PD specification. */
#define BIT_MSK__PDCC24INT1__REG_PDCC24_INTR9                                      0x02
    /* (ReadWrite, Bits 2) */
    /* ro_intr_prl_tx_msg_timeout: PD Protocol Layer Transmitter interrupt. This bit is set due to
    message transmission timeout while transmitting a message through PD Physical Layer. Please
    refer to the description of ri_prl_tTransmitComplete. */
#define BIT_MSK__PDCC24INT1__REG_PDCC24_INTR10                                     0x04
    /* (ReadWrite, Bits 3) */
    /* ro_intr_prl_rx_phyrx_discarded_rxbuf: PD Protocol Layer Receiver error interrupt. This bit is
    set if the received message by PHY is discarded because PDRXBUF is occupied by the previous
    message. Software confirm the reception of PDRXBUF by writing '1' to ri_prl_rx_msg_read_done_wp. */
#define BIT_MSK__PDCC24INT1__REG_PDCC24_INTR11                                     0x08
    /* (ReadWrite, Bits 4) */
    /* ro_intr_prl_rx_phyrx_discarded_rx_busy: PD Protocol Layer Receiver error interrupt. This bit
    is set if the received message by PHY is discarded due to busy Protocol Layer Receiver. If
    ri_prl_rx_skip_goodcrc_rdbuf is '1', this bit is also set if the received message by PHY is ignored
    and not responded with GoodCRC since PDRXBUF is occupied by the previous message. The port
    partner will retry transmission since it GoodCRC is not responded. */
#define BIT_MSK__PDCC24INT1__REG_PDCC24_INTR12                                     0x10
    /* (ReadWrite, Bits 5) */
    /* ro_intr_prl_rx_msg_size_over: PD Physical/Protocol Layer Receiver error interrupt. This bit
    is set if received payload size is over 30 bytes. No action is required for software. */
#define BIT_MSK__PDCC24INT1__REG_PDCC24_INTR13                                     0x20
    /* (ReadWrite, Bits 6) */
    /* ro_intr_prl_rx_msg_timeout: PD Protocol Layer Receiver error interrupt. This bit is set due
    to message transmission timeout while transmitting GoodCRC or BIST count message. Please
    refer to the description of ri_prl_tTransmitComplete. No action is required for software. The port
    partner will retry transmission since it GoodCRC is not responded. */
#define BIT_MSK__PDCC24INT1__REG_PDCC24_INTR14                                     0x40
    /* (ReadWrite, Bits 7) */
    /* ro_intr_phy_tx_bmc_enc_error: PD Physical Layer Transmitter BMC encoding error interrupt.
    This bit is set if BMC encoder has internal state errors: - BMC encoder receives the ending
    request while it is not encoding. - BMC encoder receives another encoding request during it is busy
    with the previous request. - BMC encoder receives the concatenated encoding request right after
    finishing a previous request. No action is required for software. */
#define BIT_MSK__PDCC24INT1__REG_PDCC24_INTR15                                     0x80

/* 2.4MHz domain INTR2 Register */
#define REG_ADDR__PDCC24INT2                                             (SABER_REG | 0x0034)
    /* (ReadWrite, Bits 0) */
    /* ro_intr_phy_tx_4b5b_enc_error: PD Physical Layer Transmitter 4b5b symbol encoding error
    interrupt. This bit is set if 4b5b symbol encoder receives the reserved 4b codes which are described
    in Table 5-1 of USB PD specification. No action is required for software. */
#define BIT_MSK__PDCC24INT2__REG_PDCC24_INTR16                                     0x01
    /* (ReadWrite, Bits 1) */
    /* ro_intr_phy_rx_no_sop_error: PD Physical Layer Receiver error interrupt. This bit is set if PD
    physical layer receiver detects bus idle without receiving SOP symbols after it detects preamble.
    No action is required for software. */
#define BIT_MSK__PDCC24INT2__REG_PDCC24_INTR17                                     0x02
    /* (ReadWrite, Bits 2) */
    /* ro_intr_phy_rx_no_eop_error: PD Physical Layer Receiver error interrupt. This bit is set if PD
    physical layer receiver detects bus idle without receiving EOP symbols after it detects preamble and
    SOP symbols. This is not applied for Hard Reset and Cable Reset signaling. No action is required
    for software. */
#define BIT_MSK__PDCC24INT2__REG_PDCC24_INTR18                                     0x04
    /* (ReadWrite, Bits 3) */
    /* ro_intr_phy_rx_data_after_eop_error: PD Physical Layer Receiver error interrupt. This bit is
    set if PD physical layer receiver receives subsequent symbols after receiving EOP code at the end
    of the PD message. Those redundant symbols will be ignored until the receiver detects bus idle.
    No action is required for software. */
#define BIT_MSK__PDCC24INT2__REG_PDCC24_INTR19                                     0x08
    /* (ReadWrite, Bits 4) */
    /* ro_intr_phy_rx_4b5b_dec_error: PD Physical Layer Receiver 4b5b symbol decoding error
    interrupt. This bit is set if 4b5b symbol decoder receives the reserved 5b codes which are described
    in Table 5-1 of USB PD specification. No action is required for software. */
#define BIT_MSK__PDCC24INT2__REG_PDCC24_INTR20                                     0x10
    /* (ReadWrite, Bits 5) */
    /* ro_intr_phy_rx_k_in_payload_error: PD Physical Layer Receiver error interrupt. This bit is set
    if PD physical layer receiver receives K-code in data payload of PD message. Please refer to Table
    5-1 of USB PD specification. No action is required for software. */
#define BIT_MSK__PDCC24INT2__REG_PDCC24_INTR21                                     0x20
    /* (ReadWrite, Bits 6) */
    /* ro_intr_phy_rx_bmc_dec_0atboundary: PD Physical Layer Receiver BMC decoding error interrupt.
    This bit is set if BMC decoder fails to track transitions of input signalling. No action is required for
    software. */
#define BIT_MSK__PDCC24INT2__REG_PDCC24_INTR22                                     0x40
    /* (ReadWrite, Bits 7) */
    /* ro_intr_phy_rx_bmc_dec_state_error: PD Physical Layer Receiver BMC decoding error interrupt.
    This bit is set if BMC decoder detects bus idle before it didnt finishes decoding in progress. No action
    is required for software. */
#define BIT_MSK__PDCC24INT2__REG_PDCC24_INTR23                                     0x80

/* 2.4MHz domain INTR3 Register */
#define REG_ADDR__PDCC24INT3                                             (SABER_REG | 0x0035)
    /* (ReadWrite, Bits 0) */
    /* ro_intr_cc_dfp_attached: CC Logic Interrupt. This bit is set if DFP is attached while state is in
    PullDnStop or PullDnToggle state. */
#define BIT_MSK__PDCC24INT3__REG_PDCC24_INTR24                                     0x01
    /* (ReadWrite, Bits 1) */
    /* ro_intr_cc_dfp_detached: CC Logic Interrupt. This bit is set if DFP is detached while state is in
    PullDnStop or PullDnToggle state. */
#define BIT_MSK__PDCC24INT3__REG_PDCC24_INTR25                                     0x02
    /* (ReadWrite, Bits 2) */
    /* ro_intr_cc_ufp_cp_attached: CC Logic Interrupt. This bit is set if UFP is attached while state is
    in PullUpStop or PullUpToggle state. This is comparator based detection. */
#define BIT_MSK__PDCC24INT3__REG_PDCC24_INTR26                                     0x04
    /* (ReadWrite, Bits 3) */
    /* ro_intr_cc_ufp_cp_detached: CC Logic Interrupt. This bit is set if UFP is detached while state is
    in PullUpStop or PullUpToggle state. This is comparator based detection. */
#define BIT_MSK__PDCC24INT3__REG_PDCC24_INTR27                                     0x08
    /* (ReadWrite, Bits 4) */
    /* ro_intr_cc_ufp_adc_attached: CC Logic Interrupt. This bit is set if UFP is attached while state is
    in PullUpStop or PullUpToggle state. This is ADC based detection. */
#define BIT_MSK__PDCC24INT3__REG_PDCC24_INTR28                                     0x10
    /* (ReadWrite, Bits 5) */
    /* ro_intr_cc_ufp_adc_detached: CC Logic Interrupt. This bit is set if UFP is detached while state is
    in PullUpStop or PullUpToggle state. This is ADC based detection. */
#define BIT_MSK__PDCC24INT3__REG_PDCC24_INTR29                                     0x20
    /* (ReadWrite, Bits 6) */
    /* ro_intr_cc_ha_attached: CC Logic Interrupt. This bit is set if DFP or UFP is attached. Refer to
    ro_cc_ha_status[1:0] for connected status. This is ADC based detection. This interrupt is filtered
    by the internal connection status. */
#define BIT_MSK__PDCC24INT3__REG_PDCC24_INTR30                                     0x40
    /* (ReadWrite, Bits 7) */
    /* ro_intr_cc_ha_detached: CC Logic Interrupt. This bit is set if DFP or UFP is attached. Refer to
    ro_cc_ha_status[1:0] for connected status. This is ADC based detection. This interrupt is filtered by
    the internal connection status. */
#define BIT_MSK__PDCC24INT3__REG_PDCC24_INTR31                                     0x80

/* 2.4MHz domain INTR4 Register */
#define REG_ADDR__PDCC24INT4                                             (SABER_REG | 0x0036)
    /* (ReadWrite, Bits 0) */
    /* ro_intr_prl_rx_hcreset_received_by_phy: PD Protocol Layer Receiver Hard Reset/Cable Recept
    Reception interrupt. This bit is set if Protocol Layer receives Hard Reset or Cable Reset from PHY Layer. */
#define BIT_MSK__PDCC24INT4__REG_PDCC24_INTR32                                     0x01
    /* (ReadWrite, Bits 1) */
    /* ro_intr_cc_ha_error: CC Logic Interrupt. This bit is set if there is internal error in hardware-assisted
    connection state. */
#define BIT_MSK__PDCC24INT4__REG_PDCC24_INTR33                                     0x02
    /* (ReadWrite, Bits 2) */
    /* ro_intr_prl_tx_msgid_mismatch: PD Protocol Layer Transmitter MessageID mismatch interrupt.
    This bit is set if Protocol Layer Transmitter receives GoodCRC with unexpected MessageID. No action
    is required for software. Protocol Layer automatically retries transmission nRetryCount times within
    tRetry time. See section 6.5.1 of USB PD specification. */
#define BIT_MSK__PDCC24INT4__REG_PDCC24_INTR34                                     0x04
    /* (ReadWrite, Bits 3) */
    /* 2.4MHz domain interrupt: Reserved */
#define BIT_MSK__PDCC24INT4__REG_PDCC24_INTR35                                     0x08
    /* (ReadWrite, Bits 4) */
    /* 2.4MHz domain interrupt: Reserved */
#define BIT_MSK__PDCC24INT4__REG_PDCC24_INTR36                                     0x10
    /* (ReadWrite, Bits 5) */
    /* 2.4MHz domain interrupt: Reserved */
#define BIT_MSK__PDCC24INT4__REG_PDCC24_INTR37                                     0x20
    /* (ReadWrite, Bits 6) */
    /* 2.4MHz domain interrupt: Reserved */
#define BIT_MSK__PDCC24INT4__REG_PDCC24_INTR38                                     0x40
    /* (ReadWrite, Bits 7) */
    /* 2.4MHz domain interrupt: Reserved */
#define BIT_MSK__PDCC24INT4__REG_PDCC24_INTR39                                     0x80

/* 2.4MHz domain INTR0 MASK Register */
#define REG_ADDR__PDCC24INTM0                                            (SABER_REG | 0x0037)
    /* (ReadWrite, Bits 0) */
    /* mask bit of intr event 0 */
#define BIT_MSK__PDCC24INTM0__REG_PDCC24_INTRMASK0                                  0x01
    /* (ReadWrite, Bits 1) */
    /* mask bit of intr event 1 */
#define BIT_MSK__PDCC24INTM0__REG_PDCC24_INTRMASK1                                  0x02
    /* (ReadWrite, Bits 2) */
    /* mask bit of intr event 2 */
#define BIT_MSK__PDCC24INTM0__REG_PDCC24_INTRMASK2                                  0x04
    /* (ReadWrite, Bits 3) */
    /* mask bit of intr event 3 */
#define BIT_MSK__PDCC24INTM0__REG_PDCC24_INTRMASK3                                  0x08
    /* (ReadWrite, Bits 4) */
    /* mask bit of intr event 4 */
#define BIT_MSK__PDCC24INTM0__REG_PDCC24_INTRMASK4                                  0x10
    /* (ReadWrite, Bits 5) */
    /* mask bit of intr event 5 */
#define BIT_MSK__PDCC24INTM0__REG_PDCC24_INTRMASK5                                  0x20
    /* (ReadWrite, Bits 6) */
    /* mask bit of intr event 6 */
#define BIT_MSK__PDCC24INTM0__REG_PDCC24_INTRMASK6                                  0x40
    /* (ReadWrite, Bits 7) */
    /* mask bit of intr event 7 */
#define BIT_MSK__PDCC24INTM0__REG_PDCC24_INTRMASK7                                  0x80

/* 2.4MHz domain INTR1 MASK Register */
#define REG_ADDR__PDCC24INTM1                                            (SABER_REG | 0x0038)
    /* (ReadWrite, Bits 0) */
    /* mask bit of intr event 8 */
#define BIT_MSK__PDCC24INTM1__REG_PDCC24_INTRMASK8                                  0x01
    /* (ReadWrite, Bits 1) */
    /* mask bit of intr event 9 */
#define BIT_MSK__PDCC24INTM1__REG_PDCC24_INTRMASK9                                  0x02
    /* (ReadWrite, Bits 2) */
    /* mask bit of intr event 10 */
#define BIT_MSK__PDCC24INTM1__REG_PDCC24_INTRMASK10                                 0x04
    /* (ReadWrite, Bits 3) */
    /* mask bit of intr event 11 */
#define BIT_MSK__PDCC24INTM1__REG_PDCC24_INTRMASK11                                 0x08
    /* (ReadWrite, Bits 4) */
    /* mask bit of intr event 12 */
#define BIT_MSK__PDCC24INTM1__REG_PDCC24_INTRMASK12                                 0x10
    /* (ReadWrite, Bits 5) */
    /* mask bit of intr event 13 */
#define BIT_MSK__PDCC24INTM1__REG_PDCC24_INTRMASK13                                 0x20
    /* (ReadWrite, Bits 6) */
    /* mask bit of intr event 14 */
#define BIT_MSK__PDCC24INTM1__REG_PDCC24_INTRMASK14                                 0x40
    /* (ReadWrite, Bits 7) */
    /* mask bit of intr event 15 */
#define BIT_MSK__PDCC24INTM1__REG_PDCC24_INTRMASK15                                 0x80

/* 2.4MHz domain INTR2 MASK Register */
#define REG_ADDR__PDCC24INTM2                                            (SABER_REG | 0x0039)
    /* (ReadWrite, Bits 0) */
    /* mask bit of intr event 16 */
#define BIT_MSK__PDCC24INTM2__REG_PDCC24_INTRMASK16                                 0x01
    /* (ReadWrite, Bits 1) */
    /* mask bit of intr event 17 */
#define BIT_MSK__PDCC24INTM2__REG_PDCC24_INTRMASK17                                 0x02
    /* (ReadWrite, Bits 2) */
    /* mask bit of intr event 18 */
#define BIT_MSK__PDCC24INTM2__REG_PDCC24_INTRMASK18                                 0x04
    /* (ReadWrite, Bits 3) */
    /* mask bit of intr event 19 */
#define BIT_MSK__PDCC24INTM2__REG_PDCC24_INTRMASK19                                 0x08
    /* (ReadWrite, Bits 4) */
    /* mask bit of intr event 20 */
#define BIT_MSK__PDCC24INTM2__REG_PDCC24_INTRMASK20                                 0x10
    /* (ReadWrite, Bits 5) */
    /* mask bit of intr event 21 */
#define BIT_MSK__PDCC24INTM2__REG_PDCC24_INTRMASK21                                 0x20
    /* (ReadWrite, Bits 6) */
    /* mask bit of intr event 22 */
#define BIT_MSK__PDCC24INTM2__REG_PDCC24_INTRMASK22                                 0x40
    /* (ReadWrite, Bits 7) */
    /* mask bit of intr event 23 */
#define BIT_MSK__PDCC24INTM2__REG_PDCC24_INTRMASK23                                 0x80

/* 2.4MHz domain INTR3 MASK Register */
#define REG_ADDR__PDCC24INTM3                                            (SABER_REG | 0x003A)
    /* (ReadWrite, Bits 0) */
    /* mask bit of intr event 24 */
#define BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK24                                 0x01
    /* (ReadWrite, Bits 1) */
    /* mask bit of intr event 25 */
#define BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK25                                 0x02
    /* (ReadWrite, Bits 2) */
    /* mask bit of intr event 26 */
#define BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK26                                 0x04
    /* (ReadWrite, Bits 3) */
    /* mask bit of intr event 27 */
#define BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK27                                 0x08
    /* (ReadWrite, Bits 4) */
    /* mask bit of intr event 28 */
#define BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK28                                 0x10
    /* (ReadWrite, Bits 5) */
    /* mask bit of intr event 29 */
#define BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK29                                 0x20
    /* (ReadWrite, Bits 6) */
    /* mask bit of intr event 30 */
#define BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK30                                 0x40
    /* (ReadWrite, Bits 7) */
    /* mask bit of intr event 31 */
#define BIT_MSK__PDCC24INTM3__REG_PDCC24_INTRMASK31                                 0x80

/* 2.4MHz domain INTR4 MASK Register */
#define REG_ADDR__PDCC24INTM4                                            (SABER_REG | 0x003B)
    /* (ReadWrite, Bits 0) */
    /* mask bit of intr event 32 */
#define BIT_MSK__PDCC24INTM4__REG_PDCC24_INTRMASK32                                 0x01
    /* (ReadWrite, Bits 1) */
    /* mask bit of intr event 33 */
#define BIT_MSK__PDCC24INTM4__REG_PDCC24_INTRMASK33                                 0x02
    /* (ReadWrite, Bits 2) */
    /* mask bit of intr event 34 */
#define BIT_MSK__PDCC24INTM4__REG_PDCC24_INTRMASK34                                 0x04
    /* (ReadWrite, Bits 3) */
    /* mask bit of intr event 35 */
#define BIT_MSK__PDCC24INTM4__REG_PDCC24_INTRMASK35                                 0x08
    /* (ReadWrite, Bits 4) */
    /* mask bit of intr event 36 */
#define BIT_MSK__PDCC24INTM4__REG_PDCC24_INTRMASK36                                 0x10
    /* (ReadWrite, Bits 5) */
    /* mask bit of intr event 37 */
#define BIT_MSK__PDCC24INTM4__REG_PDCC24_INTRMASK37                                 0x20
    /* (ReadWrite, Bits 6) */
    /* mask bit of intr event 38 */
#define BIT_MSK__PDCC24INTM4__REG_PDCC24_INTRMASK38                                 0x40
    /* (ReadWrite, Bits 7) */
    /* mask bit of intr event 39 */
#define BIT_MSK__PDCC24INTM4__REG_PDCC24_INTRMASK39                                 0x80

/* PD Tx Buffer Register 0 */
#define REG_ADDR__PDTXBUF0                                               (SABER_REG | 0x003C)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer: 4b5b unencoded payload of PD message to transmit. Bits are sent out onto the CC
    line LSB first, followed by the MSB. Bytes with lower address are sent out first followed by Bytes with
    higher address as specified in Section 5.5 of Type-C specification 2.0. The size of the payload
    (Header + Data Objects) must be in the range from 2 bytes to 30 bytes. These fields are not valid if
    ri_pdtxsize[4:0] is 0 (e.g., Hard Reest and Cable Reset). This field contains:     - One MEssage Header
    (2 bytes)     - Zero to seven Data Objects (4 bytes each) */
#define BIT_MSK__PDTXBUF0__RI_PDTXBUF_B7_B0                                      0xFF

/* PD Tx Buffer 1 Register */
#define REG_ADDR__PDTXBUF1                                               (SABER_REG | 0x003D)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer: If ri_prl_MessageID_replace is 0, Bit [3:1] of PDTXBUF1 is transmitted for MessageID
    of PD message header.. If ri_prl_MessageID_replace is 1 (default), MessageID field of the transmitted
    message header is managed by Protocol Layer. Bit [3:1] of PDTXBUF1 (bit [11:9] of Message Header) is
    automatically replaced by the Sabre's Protocol Layer. MessageID field is updated based on the result of
    Protocol-level communication. */
#define BIT_MSK__PDTXBUF1__RI_PDTXBUF_B15_B8                                     0xFF

/* PD Tx Buffer 2 Register */
#define REG_ADDR__PDTXBUF2                                               (SABER_REG | 0x003E)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF2__RI_PDTXBUF_B23_B16                                    0xFF

/* PD Tx Buffer 3 Register */
#define REG_ADDR__PDTXBUF3                                               (SABER_REG | 0x003F)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF3__RI_PDTXBUF_B31_B24                                    0xFF

/* PD Tx Buffer 4 Register */
#define REG_ADDR__PDTXBUF4                                               (SABER_REG | 0x0040)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF4__RI_PDTXBUF_B39_B32                                    0xFF

/* PD Tx Buffer 5 Register */
#define REG_ADDR__PDTXBUF5                                               (SABER_REG | 0x0041)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF5__RI_PDTXBUF_B47_B40                                    0xFF

/* PD Tx Buffer 6 Register */
#define REG_ADDR__PDTXBUF6                                               (SABER_REG | 0x0042)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF6__RI_PDTXBUF_B55_B48                                    0xFF

/* PD Tx Buffer 7 Register */
#define REG_ADDR__PDTXBUF7                                               (SABER_REG | 0x0043)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF7__RI_PDTXBUF_B63_B56                                    0xFF

/* PD Tx Buffer 8 Register */
#define REG_ADDR__PDTXBUF8                                               (SABER_REG | 0x0044)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF8__RI_PDTXBUF_B71_B64                                    0xFF

/* PD Tx Buffer 9 Register */
#define REG_ADDR__PDTXBUF9                                               (SABER_REG | 0x0045)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF9__RI_PDTXBUF_B79_B72                                    0xFF

/* PD Tx Buffer 10 Register */
#define REG_ADDR__PDTXBUF10                                              (SABER_REG | 0x0046)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF10__RI_PDTXBUF_B87_B80                                    0xFF

/* PD Tx Buffer 11 Register */
#define REG_ADDR__PDTXBUF11                                              (SABER_REG | 0x0047)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF11__RI_PDTXBUF_B95_B88                                    0xFF

/* PD Tx Buffer 12 Register */
#define REG_ADDR__PDTXBUF12                                              (SABER_REG | 0x0048)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF12__RI_PDTXBUF_B103_B96                                   0xFF

/* PD Tx Buffer 13 Register */
#define REG_ADDR__PDTXBUF13                                              (SABER_REG | 0x0049)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF13__RI_PDTXBUF_B111_B104                                  0xFF

/* PD Tx Buffer 14 Register */
#define REG_ADDR__PDTXBUF14                                              (SABER_REG | 0x004A)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF14__RI_PDTXBUF_B119_B112                                  0xFF

/* PD Tx Buffer 15 Register */
#define REG_ADDR__PDTXBUF15                                              (SABER_REG | 0x004B)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF15__RI_PDTXBUF_B127_B120                                  0xFF

/* PD Tx Buffer 16 Register */
#define REG_ADDR__PDTXBUF16                                              (SABER_REG | 0x004C)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF16__RI_PDTXBUF_B135_B128                                  0xFF

/* PD Tx Buffer 17 Register */
#define REG_ADDR__PDTXBUF17                                              (SABER_REG | 0x004D)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF17__RI_PDTXBUF_B143_B136                                  0xFF

/* PD Tx Buffer 18 Register */
#define REG_ADDR__PDTXBUF18                                              (SABER_REG | 0x004E)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF18__RI_PDTXBUF_B151_B144                                  0xFF

/* PD Tx Buffer 19 Register */
#define REG_ADDR__PDTXBUF19                                              (SABER_REG | 0x004F)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF19__RI_PDTXBUF_B159_B152                                  0xFF

/* PD Tx Buffer 20 Register */
#define REG_ADDR__PDTXBUF20                                              (SABER_REG | 0x0050)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF20__RI_PDTXBUF_B167_B160                                  0xFF

/* PD Tx Buffer 21 Register */
#define REG_ADDR__PDTXBUF21                                              (SABER_REG | 0x0051)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF21__RI_PDTXBUF_B175_B168                                  0xFF

/* PD Tx Buffer 22 Register */
#define REG_ADDR__PDTXBUF22                                              (SABER_REG | 0x0052)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF22__RI_PDTXBUF_B183_B176                                  0xFF

/* PD Tx Buffer 23 Register */
#define REG_ADDR__PDTXBUF23                                              (SABER_REG | 0x0053)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF23__RI_PDTXBUF_B191_B184                                  0xFF

/* PD Tx Buffer 24 Register */
#define REG_ADDR__PDTXBUF24                                              (SABER_REG | 0x0054)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF24__RI_PDTXBUF_B199_B192                                  0xFF

/* PD Tx Buffer 25 Register */
#define REG_ADDR__PDTXBUF25                                              (SABER_REG | 0x0055)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF25__RI_PDTXBUF_B207_B200                                  0xFF

/* PD Tx Buffer 26 Register */
#define REG_ADDR__PDTXBUF26                                              (SABER_REG | 0x0056)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF26__RI_PDTXBUF_B215_B208                                  0xFF

/* PD Tx Buffer 27 Register */
#define REG_ADDR__PDTXBUF27                                              (SABER_REG | 0x0057)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF27__RI_PDTXBUF_B223_B216                                  0xFF

/* PD Tx Buffer 28 Register */
#define REG_ADDR__PDTXBUF28                                              (SABER_REG | 0x0058)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF28__RI_PDTXBUF_B231_B224                                  0xFF

/* PD Tx Buffer 29 Register */
#define REG_ADDR__PDTXBUF29                                              (SABER_REG | 0x0059)
    /* (ReadWrite, Bits 7:0) */
    /* PD Tx Buffer */
#define BIT_MSK__PDTXBUF29__RI_PDTXBUF_B239_B232                                  0xFF

/* PD Tx Byte Count Register */
#define REG_ADDR__PDTXBC                                                 (SABER_REG | 0x005A)
    /* (ReadWrite, Bits 4:0) */
    /* The byte size of the valid payload to transmit     - For Hard Reset, Cable Reset, this field should be
    0 (no Header and Data Object)     - For Control Messages (e.g., Accept), this field should be 2
    (2-byte header).     - For Data Message s(e.g., Source Capabilities), this field should be 2 (2-byte header)
    + 4x(number of Data Objects) */
#define BIT_MSK__PDTXBC__RI_PDTXSIZE_B4_B0                                     0x1F
    /* (ReadWrite, Bits 7:5) */
    /* SOP type of PD message to transmit:     3'h0: SOP     3'h1: SOP'     3'h2: SOP''     3'h3: Hard Reset
    3'h4: Cable Reset     3'h5: SOP'_Debug     3'h6: SOP''_Debug     3'h7: reserved */
#define BIT_MSK__PDTXBC__RI_PDTXSOP_B2_B0                                      0xE0

/* PD Tx Control Register */
#define REG_ADDR__PDTXCS                                                 (SABER_REG | 0x005B)
    /* (ReadWrite, Bits 0) */
    /* Software sets this field after setting PDTXBUF and PDTXBC. If this field is set when it is '0' (idle),
    PD controller starts transmitting PD message based on the information on PDTXBUF and PDTXBC. This
    field remains '1' (busy) while transmitting the message. This field is automatically cleared to '0' after
    the message has been sent or retry operation has been halted due to multiple errors. If
    ri_pdcc_wp_manual = '1', software need to write '0' to deassert the strobe for next transaction. */
#define BIT_MSK__PDTXCS__RI_PDTXTRANSMIT_WP                                    0x01
    /* (ReadOnly, Bits 1) */
    /* This field indicates that PD controller is busy and not ready to accept transmission request from
    software. */
#define BIT_MSK__PDTXCS__RI_PDTXBUSY                                           0x02
    /* (ReadOnly, Bits 2) */
    /* This field indicates that CC bus is not idle condition (Refer to 5.7 of USB PD specification). */
#define BIT_MSK__PDTXCS__RI_PDBUSBUSY                                          0x04
    /* (ReadWrite, Bits 7:3) */
    /* rsvd */
#define BIT_MSK__PDTXCS__RSVD                                                  0xF8

/* PD Rx Buffer Register 0 */
#define REG_ADDR__PDRXBUF0                                               (SABER_REG | 0x005C)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer: The 4b5b decoded payload of the received PD message from the port partner or from
    a cable. Bits have been received from the CC line LSB first, followed by the MSB. Bytes with lower address
    have been received first followed by Bytes with higher address as specified in section 5.5 of
    Type-C specification 2.0. The bRxLength bytes (from offset 0) of these fields contain valid payload
    (Header + Data Objects). These fields are not valid if bRxLength is 0 (e.g., Hard Reset and Cable Reset)
    These fields contain:     - One Message Header (2 bytes)     - Zero to seven Data Objects (4 byte each) */
#define BIT_MSK__PDRXBUF0__RO_PDRXBUF_B7_B0                                      0xFF

/* PD Rx Buffer 1 Register */
#define REG_ADDR__PDRXBUF1                                               (SABER_REG | 0x005D)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF1__RO_PDRXBUF_B15_B8                                     0xFF

/* PD Rx Buffer 2 Register */
#define REG_ADDR__PDRXBUF2                                               (SABER_REG | 0x005E)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF2__RO_PDRXBUF_B23_B16                                    0xFF

/* PD Rx Buffer 3 Register */
#define REG_ADDR__PDRXBUF3                                               (SABER_REG | 0x005F)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF3__RO_PDRXBUF_B31_B24                                    0xFF

/* PD Rx Buffer 4 Register */
#define REG_ADDR__PDRXBUF4                                               (SABER_REG | 0x0060)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF4__RO_PDRXBUF_B39_B32                                    0xFF

/* PD Rx Buffer 5 Register */
#define REG_ADDR__PDRXBUF5                                               (SABER_REG | 0x0061)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF5__RO_PDRXBUF_B47_B40                                    0xFF

/* PD Rx Buffer 6 Register */
#define REG_ADDR__PDRXBUF6                                               (SABER_REG | 0x0062)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF6__RO_PDRXBUF_B55_B48                                    0xFF

/* PD Rx Buffer 7 Register */
#define REG_ADDR__PDRXBUF7                                               (SABER_REG | 0x0063)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF7__RO_PDRXBUF_B63_B56                                    0xFF

/* PD Rx Buffer 8 Register */
#define REG_ADDR__PDRXBUF8                                               (SABER_REG | 0x0064)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF8__RO_PDRXBUF_B71_B64                                    0xFF

/* PD Rx Buffer 9 Register */
#define REG_ADDR__PDRXBUF9                                               (SABER_REG | 0x0065)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF9__RO_PDRXBUF_B79_B72                                    0xFF

/* PD Rx Buffer 10 Register */
#define REG_ADDR__PDRXBUF10                                              (SABER_REG | 0x0066)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF10__RO_PDRXBUF_B87_B80                                    0xFF

/* PD Rx Buffer 11 Register */
#define REG_ADDR__PDRXBUF11                                              (SABER_REG | 0x0067)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF11__RO_PDRXBUF_B95_B88                                    0xFF

/* PD Rx Buffer 12 Register */
#define REG_ADDR__PDRXBUF12                                              (SABER_REG | 0x0068)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF12__RO_PDRXBUF_B103_B96                                   0xFF

/* PD Rx Buffer 13 Register */
#define REG_ADDR__PDRXBUF13                                              (SABER_REG | 0x0069)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF13__RO_PDRXBUF_B111_B104                                  0xFF

/* PD Rx Buffer 14 Register */
#define REG_ADDR__PDRXBUF14                                              (SABER_REG | 0x006A)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF14__RO_PDRXBUF_B119_B112                                  0xFF

/* PD Rx Buffer 15 Register */
#define REG_ADDR__PDRXBUF15                                              (SABER_REG | 0x006B)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF15__RO_PDRXBUF_B127_B120                                  0xFF

/* PD Rx Buffer 16 Register */
#define REG_ADDR__PDRXBUF16                                              (SABER_REG | 0x006C)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF16__RO_PDRXBUF_B135_B128                                  0xFF

/* PD Rx Buffer 17 Register */
#define REG_ADDR__PDRXBUF17                                              (SABER_REG | 0x006D)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF17__RO_PDRXBUF_B143_B136                                  0xFF

/* PD Rx Buffer 18 Register */
#define REG_ADDR__PDRXBUF18                                              (SABER_REG | 0x006E)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF18__RO_PDRXBUF_B151_B144                                  0xFF

/* PD Rx Buffer 19 Register */
#define REG_ADDR__PDRXBUF19                                              (SABER_REG | 0x006F)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF19__RO_PDRXBUF_B159_B152                                  0xFF

/* PD Rx Buffer 20 Register */
#define REG_ADDR__PDRXBUF20                                              (SABER_REG | 0x0070)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF20__RO_PDRXBUF_B167_B160                                  0xFF

/* PD Rx Buffer 21 Register */
#define REG_ADDR__PDRXBUF21                                              (SABER_REG | 0x0071)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF21__RO_PDRXBUF_B175_B168                                  0xFF

/* PD Rx Buffer 22 Register */
#define REG_ADDR__PDRXBUF22                                              (SABER_REG | 0x0072)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF22__RO_PDRXBUF_B183_B176                                  0xFF

/* PD Rx Buffer 23 Register */
#define REG_ADDR__PDRXBUF23                                              (SABER_REG | 0x0073)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF23__RO_PDRXBUF_B191_B184                                  0xFF

/* PD Rx Buffer 24 Register */
#define REG_ADDR__PDRXBUF24                                              (SABER_REG | 0x0074)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF24__RO_PDRXBUF_B199_B192                                  0xFF

/* PD Rx Buffer 25 Register */
#define REG_ADDR__PDRXBUF25                                              (SABER_REG | 0x0075)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF25__RO_PDRXBUF_B207_B200                                  0xFF

/* PD Rx Buffer 26 Register */
#define REG_ADDR__PDRXBUF26                                              (SABER_REG | 0x0076)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF26__RO_PDRXBUF_B215_B208                                  0xFF

/* PD Rx Buffer 27 Register */
#define REG_ADDR__PDRXBUF27                                              (SABER_REG | 0x0077)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF27__RO_PDRXBUF_B223_B216                                  0xFF

/* PD Rx Buffer 28 Register */
#define REG_ADDR__PDRXBUF28                                              (SABER_REG | 0x0078)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF28__RO_PDRXBUF_B231_B224                                  0xFF

/* PD Rx Buffer 29 Register */
#define REG_ADDR__PDRXBUF29                                              (SABER_REG | 0x0079)
    /* (ReadWrite, Bits 7:0) */
    /* PD Rx Buffer */
#define BIT_MSK__PDRXBUF29__RO_PDRXBUF_B239_B232                                  0xFF

/* PD Rx Byte Count Register */
#define REG_ADDR__PDRXBC                                                 (SABER_REG | 0x007A)
    /* (ReadWrite, Bits 4:0) */
    /* The byte size of the received payload: The byte size of the received payload which has been stored
    in PDRXBUF buffer. This value is expected to be in the range from 0 to 30.     - For Hard Reset, Cable
    Reset, this field should be 0 (no Header and Data Object)     - For Control Messages (e.g., Accept),
    this field is 2 (2-byte header). - For Data Messages (e.g., Source Capabilities), this field is 2
    (2-byte header) + 4x(number of Data Objects) */
#define BIT_MSK__PDRXBC__RO_PDRXSIZE_B4_B0                                     0x1F
    /* (ReadWrite, Bits 7:5) */
    /* SOP type of the received PD message from the port partner or from a cable:     3'h0: SOP
    3'h1: SOP' 3'h2: SOP''     3'h3: Hard Reset     3'h4: Cable Reset     3'h5: SOP'_Debug     3'h6: SOP''_Debug
    3'h7: reserved */
#define BIT_MSK__PDRXBC__RO_PDRXSOP_B2_B0                                      0xE0

/* PD Rx Valid Register */
#define REG_ADDR__PDRXCS                                                 (SABER_REG | 0x007B)
    /* (ReadWrite, Bits 0) */
    /* PD Rx status: PDRXBUF buffer is valid */
#define BIT_MSK__PDRXCS__RO_PDRXVAL                                            0x01
    /* (ReadWrite, Bits 1) */
    /* PD Rx status: PDRXBUF payload had good CRC */
#define BIT_MSK__PDRXCS__RO_PDRXCRCOK                                          0x02

/* PD Control CTR0 Register */
#define REG_ADDR__PDCTR0                                                 (SABER_REG | 0x007C)
    /* (ReadWrite, Bits 0) */
    /* PRL bypass mode:  Write '1' to this field to request reset to Physical Layer (write pulse) Read this
    field to get the status of reset: 0=reset done, 1=resetting (busy) If ri_pdcc_wp_manual = '1', software
    need to write '0' to deassert the strobe for next transaction. */
#define BIT_MSK__PDCTR0__RI_PRLBYPASS_RESET_REQ_WP                             0x01
    /* (ReadWrite, Bits 1) */
    /* Hard Reset complete from Policy Engine: Write '1' to this field to notify to Protocol Layer of PE's
    completion of Hard Reset (write pulse) Read value is always '0' If ri_prl_hcreset_done_handshake is '0',
    Policy Engine is not required to write to this field to notify of PE's completion of Hard Reset. Protocol Layer
    does not wait for Policy Engine to complete Hard Reset process. If ri_pdcc_wp_manual = '1', software
    need to write '0' to deassert the strobe for next transaction. */
#define BIT_MSK__PDCTR0__RI_PRL_HCRESET_DONE_BY_PE_WP                          0x02
    /* (ReadWrite, Bits 2) */
    /* Read buffer read complete from Policy Engine: Write '1' to this field to notify to Protocol Layer of
    PE's completion of Read buffer (PDRXBUF) Read (write pulse). PDRXBUF becomes available for consecutive
    incoming message from PHY. Read value is always '0' If ri_prl_rx_msg_read_handshake is '0', Policy Engine
    is not required to write to this field to notify of PE's completion of Read Buffer Read. Protocol Layer does
    not wait for Policy Engine to complete Read Buffer Read. If ri_pdcc_wp_manual = '1', software need to write
    '0' to deassert the strobe for next transaction. */
#define BIT_MSK__PDCTR0__RI_PRL_RX_MSG_READ_DONE_WP                            0x04

/* PD Control CTR1 Register */
#define REG_ADDR__PDCTR1                                                 (SABER_REG | 0x007D)
    /* (ReadWrite, Bits 0) */
    /* Protocol Layer bypass mode: Allow software to access directly to PHY */
#define BIT_MSK__PDCTR1__RI_PRLBYPASS_MODE                                     0x01
    /* (ReadWrite, Bits 1) */
    /* High if cable plug which does not retry (6.8.2.1.8) */
#define BIT_MSK__PDCTR1__RI_PRL_CABLE_PLUG                                     0x02
    /* (ReadWrite, Bits 2) */
    /* Protocol Layer arbitration mode for message transmission:   0: Protocol Layer Receiver (GoodCRC
    transmitter) has higher priority over Protocol Layer Transmitter (Default)   1. Protocol Layer Transmitter
    has higher priority over Protocol Layer Receiver (GoodCRC transmitter) */
#define BIT_MSK__PDCTR1__RI_PRL_ARB_PRLTX_HIGHER_PRIORITY                      0x04

/* PD Control CTR2 Register */
#define REG_ADDR__PDCTR2                                                 (SABER_REG | 0x007E)
    /* (ReadWrite, Bits 7:0) */
    /* CRCReceiveTimer expiration 0.9~1.1msec (6.5.1 Table 6-30) Default=1.0ms=12'd2400=12'h960 */
#define BIT_MSK__PDCTR2__RI_PRL_TRECEIVE_B7_B0                                 0xFF

/* PD Control CTR3 Register */
#define REG_ADDR__PDCTR3                                                 (SABER_REG | 0x007F)
    /* (ReadWrite, Bits 3:0) */
    /* CRCReceiveTimer expiration 0.9~1.1msec (6.5.1 Table 6-30) Default=1.0ms=12'd2400=12'h960 */
#define BIT_MSK__PDCTR3__RI_PRL_TRECEIVE_B11_B8                                0x0F
    /* (ReadWrite, Bits 7:4) */
    /* nRetryCount (Table 6-32) Default=4'h2 */
#define BIT_MSK__PDCTR3__RI_PRL_NRETRYCOUNT_B3_B0                              0xF0

/* PD Control CTR4 Register */
#define REG_ADDR__PDCTR4                                                 (SABER_REG | 0x0080)
    /* (ReadWrite, Bits 7:0) */
    /* HardResetCompleteTimer expiration 5ms  (6.5.1 Table 6-30) Default=5.0ms=16'd12000=16'h2EE0 */
#define BIT_MSK__PDCTR4__RI_PRL_THARDRESETCOMPLETE_B7_B0                       0xFF

/* PD Control CTR5 Register */
#define REG_ADDR__PDCTR5                                                 (SABER_REG | 0x0081)
    /* (ReadWrite, Bits 7:0) */
    /* HardResetCompleteTimer expiration 5ms  (6.5.1 Table 6-30) Default=5.0ms=16'd12000=16'h2EE0 */
#define BIT_MSK__PDCTR5__RI_PRL_THARDRESETCOMPLETE_B15_B8                      0xFF

/* PD Control CTR6 Register */
#define REG_ADDR__PDCTR6                                                 (SABER_REG | 0x0082)
    /* (ReadWrite, Bits 7:0) */
    /* TransmitCompleteTimer expiration (Not spec defined). The TransmitCompleteTimer is used  to ensure
    that a message transmission is completed within a time specified by this field. The ro_intr_prl_tx_msg_timeout
    interrupt is set if TransmitCompleteTimer expires. Default=5.0ms=16'd12000=16'h2EE0 */
#define BIT_MSK__PDCTR6__RI_PRL_TTRANSMITCOMPLETE_B7_B0                        0xFF

/* PD Control CTR7 Register */
#define REG_ADDR__PDCTR7                                                 (SABER_REG | 0x0083)
    /* (ReadWrite, Bits 7:0) */
    /* TransmitCompleteTimer expiration (Not spec defined). The TransmitCompleteTimer is used  to ensure
    that a message transmission is completed within a time specified by this field. The ro_intr_prl_tx_msg_timeout
    interrupt is set if TransmitCompleteTimer expires. Default=5.0ms=16'd12000=16'h2EE0 */
#define BIT_MSK__PDCTR7__RI_PRL_TTRANSMITCOMPLETE_B15_B8                       0xFF

/* PD Control CTR8 Register */
#define REG_ADDR__PDCTR8                                                 (SABER_REG | 0x0084)
    /* (ReadWrite, Bits 7:0) */
    /* BISTReceiveErrorTimer expiration 1.0~1.2msec (6.5.2 Table 6-30) Default=1.0ms=12'd2400=12'h960 */
#define BIT_MSK__PDCTR8__RI_BIST_TBISTRECEIVE_B7_B0                            0xFF

/* PD Control CTR9 Register */
#define REG_ADDR__PDCTR9                                                 (SABER_REG | 0x0085)
    /* (ReadWrite, Bits 3:0) */
    /* BISTReceiveErrorTimer expiration 1.0~1.2msec (6.5.2 Table 6-30) Default=1.0ms=12'd2400=12'h960 */
#define BIT_MSK__PDCTR9__RI_BIST_TBISTRECEIVE_B11_B8                           0x0F

/* PD Control CTR11 Register */
#define REG_ADDR__PDCTR11                                                (SABER_REG | 0x0087)
    /* (ReadWrite, Bits 1:0) */
    /* PRL Rx: 00=Revision 1.0, 01=Revision 2.0(default), 10-11=Reserved (6.2.1.5) */
#define BIT_MSK__PDCTR11__RI_PRL_SPEC_REV                                       0x03
    /* (ReadWrite, Bits 2) */
    /* PRL Rx: current power role of the Port: 0=Sink, 1=Source (6.2.1.4) */
#define BIT_MSK__PDCTR11__RI_PRL_POWER_ROLE                                     0x04
    /* (ReadWrite, Bits 3) */
    /* PRL Rx: current data role of the Port: 0=UFP,  1=DFP (6.2.1.6) */
#define BIT_MSK__PDCTR11__RI_PRL_DATA_ROLE                                      0x08
    /* (ReadWrite, Bits 4) */
    /* PRL Rx: SW handshake enable for reading out the rxbuf */
#define BIT_MSK__PDCTR11__RI_PRL_RX_MSG_READ_HANDSHAKE                          0x10
    /* (ReadWrite, Bits 5) */
    /* PRL Rx: Disable dicarding GoodCRC message for PRL Rx (footnote 1 of Figure 6-20) Default=1'b1 */
#define BIT_MSK__PDCTR11__RI_PRL_RX_DISABLE_GOODCRC_DISCARD                     0x20
    /* (ReadWrite, Bits 6) */
    /* PRL Rx: Protocol Layer Receiver ignores the received message from PHY and skips sending GoodCRC if
    PDRXBUF is occupied by the previous message. The port partner will retry transmission since it GoodCRC is
    not responded. This is applied to Control Messages and Data message. However it is not applied to Hard
    Reset and Cable Reset signaling. */
#define BIT_MSK__PDCTR11__RI_PRL_RX_SKIP_GOODCRC_RDBUF                          0x40

/* PD Control CTR12 Register */
#define REG_ADDR__PDCTR12                                                (SABER_REG | 0x0088)
    /* (ReadWrite, Bits 0) */
    /* CC1/CC2 flip orientation:      0=CC1-PD,     CC2-VCONN     1=CC1-VCONN,  CC2-PD */
#define BIT_MSK__PDCTR12__RI_MODE_FLIP                                          0x01
    /* (ReadWrite, Bits 1) */
    /* Disable echo cancellation (force to enable) Default=1'b0 */
#define BIT_MSK__PDCTR12__RI_PHY_DISABLE_ECHO_CANCEL                            0x02
    /* (ReadWrite, Bits 2) */
    /* Automatically adjust sample position 2 when local clock is slower than far-end Default=1'b0, 1'b1 for
    6x worstcase (far-fast local-slow) */
#define BIT_MSK__PDCTR12__RI_PHY_ADJUST_SAMPLE_POS2                             0x04
    /* (ReadWrite, Bits 3) */
    /* Waive discard of GoodCRC by PHY reset request from PRL Default=1'b1 */
#define BIT_MSK__PDCTR12__RI_PHY_WAIVE_GOODCRC_DISCARD                          0x08

/* PD Control CTR13 Register */
#define REG_ADDR__PDCTR13                                                (SABER_REG | 0x0089)
    /* (ReadWrite, Bits 3:0) */
    /* Number of oversample: Default=4'h8(8x oversample), 4'h6(6x oversample) */
#define BIT_MSK__PDCTR13__RI_PHY_NUM_OF_OVERSAMPLE                              0x0F
    /* (ReadWrite, Bits 7:4) */
    /* Time between driving us and reactivating bmc_rx Default=4'h2 */
#define BIT_MSK__PDCTR13__RI_PHY_RX_EN_MARGIN                                   0xF0

/* PD Control CTR14 Register */
#define REG_ADDR__PDCTR14                                                (SABER_REG | 0x008A)
    /* (ReadWrite, Bits 7:0) */
    /* Number of cycle to drive CC low after the end of message:  Default=8'h10 Range=1us(8'h3) ~
    23us(55=8'h37) Table 5-25 */
#define BIT_MSK__PDCTR14__RI_PHY_THOLDLOWBMC                                    0xFF

/* PD Control CTR15 Register */
#define REG_ADDR__PDCTR15                                                (SABER_REG | 0x008B)
    /* (ReadWrite, Bits 7:0) */
    /* Bitwidth of Preamble for BMC encoder: Default=8'h40(64bit) */
#define BIT_MSK__PDCTR15__RI_PHY_BITWIDTH_OF_PREAMBLE                           0xFF

/* PD Control CTR16 Register */
#define REG_ADDR__PDCTR16                                                (SABER_REG | 0x008C)
    /* (ReadWrite, Bits 7:0) */
    /* Time window for detecting non-idle: Default 8'h22(8'd34)=14.166usec(nom) Table 5-26
    (12 usec ~ 20 usec) */
#define BIT_MSK__PDCTR16__RI_PHY_TRANSITION_WINDOW                              0xFF

/* PD Control CTR17 Register */
#define REG_ADDR__PDCTR17                                                (SABER_REG | 0x008D)
    /* (ReadWrite, Bits 7:0) */
    /* Time from end of last bit of a Frame until the start of the first bit of the next Preamble Default
    8'd61=8'h3D (x8 25.41usec) Table 5-14 */
#define BIT_MSK__PDCTR17__RI_PHY_INTERFRAME_GAP                                 0xFF

/* PD Control CTR18 Register */
#define REG_ADDR__PDCTR18                                                (SABER_REG | 0x008E)
    /* (ReadWrite, Bits 3:0) */
    /* BIST mode:     4'h0=Receive     4'h1=Transmit     4'h2=Reserved     4'h3=Carrier0     4'h4=Carrier1
    4'h5=Carrier2     4'h6=Carrier3     4'h7=EyePattn     4'h8=TestData */
#define BIT_MSK__PDCTR18__RI_BIST_MODE                                          0x0F
    /* (ReadWrite, Bits 4) */
    /* BIST enable: Assert low to initialize BIST logic */
#define BIT_MSK__PDCTR18__RI_BIST_ENABLE                                        0x10
    /* (ReadWrite, Bits 5) */
    /* Burst Test Frame transmit mode Default=1'b0 */
#define BIT_MSK__PDCTR18__RI_BIST_BURST_TRANSMIT                                0x20

/* PD Control CTR19 Register */
#define REG_ADDR__PDCTR19                                                (SABER_REG | 0x008F)
    /* (ReadWrite, Bits 7:0) */
    /* Ignore the specified SOP types:     Bit 0: SOP     Bit 1: SOP'     Bit 2: SOP''     Bit 3: Hard Reset
    Bit 4: Cable Reset     Bit 5: SOP'_Debug     Bit 6: SOP''_Debug     Bit 7: Reserved For example, set 0x66
    to ignore SOP' and SOP''. */
#define BIT_MSK__PDCTR19__RI_PHY_SOP_TYPE_IGNORE_LIST_B7_B0                     0xFF

/* PD Control CTR20 Register */
#define REG_ADDR__PDCTR20                                                (SABER_REG | 0x0090)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PDCTR20__RI_PD_CONF20                                          0xFF

/* PD Control CTR21 Register */
#define REG_ADDR__PDCTR21                                                (SABER_REG | 0x0091)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PDCTR21__RI_PD_CONF21                                          0xFF

/* PD Control CTR22 Register */
#define REG_ADDR__PDCTR22                                                (SABER_REG | 0x0092)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PDCTR22__RI_PD_CONF22                                          0xFF

/* PD Control CTR23 Register */
#define REG_ADDR__PDCTR23                                                (SABER_REG | 0x0093)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PDCTR23__RI_PD_CONF23                                          0xFF

/* PD Control CTR24 Register */
#define REG_ADDR__PDCTR24                                                (SABER_REG | 0x0094)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PDCTR24__RI_PD_CONF24                                          0xFF

/* PD Control CTR25 Register */
#define REG_ADDR__PDCTR25                                                (SABER_REG | 0x0095)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PDCTR25__RI_PD_CONF25                                          0xFF

/* PD Control CTR26 Register */
#define REG_ADDR__PDCTR26                                                (SABER_REG | 0x0096)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PDCTR26__RI_PD_CONF26                                          0xFF

/* PD 2.4MHz domain STAT0 Register */
#define REG_ADDR__PD24STAT0                                              (SABER_REG | 0x0097)
    /* (ReadOnly, Bits 7:0) */
    /* ro_pd24_status[4:0]: PD Protocol Layer Transmitter state   5'h00=PRL_TX_PHY_LAYER_RESET
    5'h01=PRL_TX_DISCARD_MESSAGE   5'h02=PRL_TX_WF_MSG_REQUEST   5'h03=PRL_TX_CONSTRUCT_MSG
    5'h04=PRL_TX_RESET_FOR_TX   5'h05=PRL_TX_WF_PHY_RESPONSE   5'h06=PRL_TX_MESSAGE_SENT
    5'h07=PRL_TX_CHECK_RETRYCNT   5'h08=PRL_TX_WF_BUS_IDLE   5'h09=PRL_TX_ERROR
    5'h0A=PRL_HR_WF_PHY_HARD_RESET   5'h0B=PRL_HR_WF_PE_HARD_CABLE_RESET
    5'h10=PRL_BT_WF_MSG_REQUEST   5'h11=PRL_BT_CONSTRUCT_MSG   5'h12=PRL_BT_WF_PHY_RESPONSE
    5'h13=PRL_BT_MESSAGE_SENT   5'h14=PRL_BT_ERROR ro_pd24_status[8:5]: RetryCounter.
    Please refer to 6.6.2 of USB PD specification. */
#define BIT_MSK__PD24STAT0__RO_PD24_STATUS_B7_B0                                  0xFF

/* PD 2.4MHz domain STAT1 Register */
#define REG_ADDR__PD24STAT1                                              (SABER_REG | 0x0098)
    /* (ReadOnly, Bits 7:0) */
    /* ro_pd24_status[11:9]: PD Protocol Layer MessageID Counter. Please refer to 6.6.1 of USB PD
    specification. ro_pd24_status[15:12]: PD Protocol Layer Receiver state   4'h0=PRL_RX_WF_PHY_MSG
    4'h1=PRL_RX_RESET_FOR_RX    4'h2=PRL_RX_SEND_GOODCRC    4'h3=PRL_RX_SEND_GOODCRC_WFBI
    4'h4=PRL_RX_CHECK_MESSAGEID    4'h5=PRL_RX_STORE_MESSAGEID    4'h6=PRL_RX_RECIVED_GOODCRC
    4'h7=PRL_HR_RESET_LAYER   4'h8=PRL_BR_WF_PHY_MSG   4'h9=PRL_BR_SEND_BIST_CNT
    4'hA=PRL_BR_BIST_CNT_SENT   4'hB=PRL_BT_WF_BIST_CNT */
#define BIT_MSK__PD24STAT1__RO_PD24_STATUS_B15_B8                                 0xFF

/* PD 2.4MHz domain STAT2 Register */
#define REG_ADDR__PD24STAT2                                              (SABER_REG | 0x0099)
    /* (ReadOnly, Bits 7:0) */
    /* ro_pd24_status[18:16]: PD Protocol Layer StoredMessageID. Refer to Section 6.8.2.2 of USB PD
    specification. ro_pd24_status[19]: Valid of StoredMessageID ro_pd24_status[23:16]: PD Physical Layer
    Arbiter state   4'h0=IDLE    4'h1=WF_IDLE_DISCARD   4'h2=PRLTX_TRANSMIT   4'h3=PRLRX_TRANSMIT
    4'h4=PRLBY_TRANSMIT */
#define BIT_MSK__PD24STAT2__RO_PD24_STATUS_B23_B16                                0xFF

/* PD 2.4MHz domain STAT3 Register */
#define REG_ADDR__PD24STAT3                                              (SABER_REG | 0x009A)
    /* (ReadOnly, Bits 7:0) */
    /* ro_pd24_status[27:24]: PD Physical Layer BMC encoder state   4'h0=IDLE   4'h1=RUN   4'h2=LAST
    4'h3=HIGHTAIL   4'h4=HOLDLOW   4'h7=ERROR ro_pd24_status[31:28]: PD Physical Layer Transmitter state
    4'h0=IDLE    4'h1=PREAMBLE    4'h2=SOP    4'h3=PAYLOAD    4'h4=CRC    4'h5=EOP    4'h6=BIST_CARRIER */
#define BIT_MSK__PD24STAT3__RO_PD24_STATUS_B31_B24                                0xFF

/* PD 2.4MHz domain STAT4 Register */
#define REG_ADDR__PD24STAT4                                              (SABER_REG | 0x009B)
    /* (ReadOnly, Bits 7:0) */
    /* ro_pd24_status[35:32]: PD Physical Layer BMC decoder state   4'h0=IDLE    4'h1=WFZERO    4'h2=DATA
    4'h3=BOUNDARY    4'h7=ERROR  ro_pd24_status[39:36]: PD Physical Layer Receiver state   4'h0=IDLE
    4'h1=WFSOP   4'h2=WFEOP   4'h3=WFBIDLE */
#define BIT_MSK__PD24STAT4__RO_PD24_STATUS_B39_B32                                0xFF

/* PD 2.4MHz domain STAT5 Register */
#define REG_ADDR__PD24STAT5                                              (SABER_REG | 0x009C)
    /* (ReadOnly, Bits 7:0) */
    /* ro_pd24_status[55:40]: BISTErrorCounter. Refer to Section 6.6.5 of USB PD specification. This field
    contains the number of bits in error during ri_bist_mode is Receive mode. */
#define BIT_MSK__PD24STAT5__RO_PD24_STATUS_B47_B40                                0xFF

/* PD 2.4MHz domain STAT6 Register */
#define REG_ADDR__PD24STAT6                                              (SABER_REG | 0x009D)
    /* (ReadOnly, Bits 7:0) */
    /* ro_pd24_status[55:40]: BISTErrorCounter. Refer to Section 6.6.5 of USB PD specification. This field
    contains the number of bits in error during ri_bist_mode is Receive mode. */
#define BIT_MSK__PD24STAT6__RO_PD24_STATUS_B55_B48                                0xFF

/* PD 2.4MHz domain STAT7 Register */
#define REG_ADDR__PD24STAT7                                              (SABER_REG | 0x009E)
    /* (ReadOnly, Bits 7:0) */
    /* ro_pd24_status[71:56]: BISTPassCounter. This field contains the number of bits matched during ri_bist_mode
    is Receive mode. */
#define BIT_MSK__PD24STAT7__RO_PD24_STATUS_B63_B56                                0xFF

/* PD 2.4MHz domain STAT8 Register */
#define REG_ADDR__PD24STAT8                                              (SABER_REG | 0x009F)
    /* (ReadOnly, Bits 7:0) */
    /* ro_pd24_status[71:56]: BISTPassCounter. This field contains the number of bits matched during ri_bist_mode
    is Receive mode. */
#define BIT_MSK__PD24STAT8__RO_PD24_STATUS_B71_B64                                0xFF

/* PD 2.4MHz domain STAT9 Register */
#define REG_ADDR__PD24STAT9                                              (SABER_REG | 0x00A0)
    /* (ReadOnly, Bits 7:0) */
    /* ro_pd24_status[72]: Channel disable status. Refer to Figure 8-8 and Figure 8-9 of USB PD specification.
    ro_pd24_status[73]: Bus idle status. Refer to Section 5.8.3.6.1 of USB PD specification. ro_pd24_status[74]:
    Interframe keepout period. This bit is '1' during time period specified by ri_phy_interframe_gap beginning
    from the end of last bit of a message. ro_pd24_status[79:75]: Reserved */
#define BIT_MSK__PD24STAT9__RO_PD24_STATUS_B79_B72                                0xFF

/* PD 2.4MHz domain STAT10 Register */
#define REG_ADDR__PD24STAT10                                             (SABER_REG | 0x00A1)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PD24STAT10__RO_PD24_STATUS_B87_B80                                0xFF

/* PD 2.4MHz domain STAT11 Register */
#define REG_ADDR__PD24STAT11                                             (SABER_REG | 0x00A2)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PD24STAT11__RO_PD24_STATUS_B95_B88                                0xFF

/* PD 2.4MHz domain STAT12 Register */
#define REG_ADDR__PD24STAT12                                             (SABER_REG | 0x00A3)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PD24STAT12__RO_PD24_STATUS_B103_B96                               0xFF

/* PD 2.4MHz domain STAT13 Register */
#define REG_ADDR__PD24STAT13                                             (SABER_REG | 0x00A4)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PD24STAT13__RO_PD24_STATUS_B111_B104                              0xFF

/* PD 2.4MHz domain STAT14 Register */
#define REG_ADDR__PD24STAT14                                             (SABER_REG | 0x00A5)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PD24STAT14__RO_PD24_STATUS_B119_B112                              0xFF

/* PD 2.4MHz domain STAT15 Register */
#define REG_ADDR__PD24STAT15                                             (SABER_REG | 0x00A6)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PD24STAT15__RO_PD24_STATUS_B127_B120                              0xFF

/* PD 2.4MHz domain STAT16 Register */
#define REG_ADDR__PD24STAT16                                             (SABER_REG | 0x00A7)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__PD24STAT16__RO_PD24_STATUS_B135_B128                              0xFF

/* CC Control CTR0 Register */
#define REG_ADDR__CCCTR0                                                 (SABER_REG | 0x00A8)
    /* (ReadWrite, Bits 3:0) */
    /* Fine clock ratio Default=4'h6=2.4MHz/400MHz */
#define BIT_MSK__CCCTR0__RI_CC_FINE_CLK_RATIO                                  0x0F
    /* (ReadWrite, Bits 4) */
    /* Fine clock enable in manual mode:     0=400KHz (Default)     1=2.4MHz */
#define BIT_MSK__CCCTR0__RI_CC_FINE_CLK_EN                                     0x10
    /* (ReadWrite, Bits 5) */
    /* Fine clock enable operational mode:     0=Manual mode     1=Automatic mode (Default) */
#define BIT_MSK__CCCTR0__RI_CC_FINE_CLK_EN_AUTO_MODE                           0x20
    /* (ReadWrite, Bits 6) */
    /* Write strobe operational mode:     0=Automatic clear mode (Default)     1=Manual on/Manual clear mode
    This mode is applicable to the following registers:     ri_pdtxtransmit_wp     ri_prlbypass_reset_req_wp
    ri_prl_hcreset_done_by_pe_wp     ri_prl_rx_msg_read_done_wp     ri_cc_adc_mode_wp     ri_cc_toggle_mode_wp */
#define BIT_MSK__CCCTR0__RI_PDCC_WP_MANUAL                                     0x40
    /* (ReadWrite, Bits 7) */
    /* CC status freeze mode:     0 = Update CC status (Default)     1=  Freeze CC status for safe read This field
    is used to prevent software read operation from CDC issues. CC1VOL and CC2VOL keep updating its value.
    Software may read transition value in error. This field is applicable to the following status registers:
    - ro_adc1_out[5:0], ro_adc1_settle, ro_adc1_det - ro_adc2_out[5:0], ro_adc2_settle, ro_adc2_det */
#define BIT_MSK__CCCTR0__RI_CC_FREEZE_STATUS                                   0x80

/* CC Control CTR1 Register */
#define REG_ADDR__CCCTR1                                                 (SABER_REG | 0x00A9)
    /* (ReadWrite, Bits 0) */
    /* Disable filter for DFP detection */
#define BIT_MSK__CCCTR1__RI_CC_DFP_FILTER_DISABLE                              0x01
    /* (ReadWrite, Bits 1) */
    /* Disable filter for UFP detection (Comparator) */
#define BIT_MSK__CCCTR1__RI_CC_UFP_CP_FILTER_DISABLE                           0x02
    /* (ReadWrite, Bits 2) */
    /* Disable filter for UFP detection (ADC) */
#define BIT_MSK__CCCTR1__RI_CC_UFP_ADC_FILTER_DISABLE                          0x04
    /* (ReadWrite, Bits 3) */
    /* Unknown detachment status is reported by '0' for the following detachment status:     ro_cc_dfp_detached
    '0': Keep the previous detachment state while toggle state is PullUp.           '1': The signals is '0' indicating
    'unknown' while toggle state is PullUp.     ro_cc_ufp_cp_detached           '0': Keep the previous detachment
    state while toggle state is PullDown.           '1': The signals is always '0' indicating 'unknown'  while toggle
    state is PullDown.     ro_cc_ufp_adc_detached           '0': Keep the previous detachment state while toggle
    state is PullDown. '1': The signals is always '0' indicating 'unknown'  while toggle state is PullDown. */
#define BIT_MSK__CCCTR1__RI_CC_ZERO_UNKNOWN_FOR_RO_DETACHED                    0x08

/* CC Control CTR2 Register */
#define REG_ADDR__CCCTR2                                                 (SABER_REG | 0x00AA)
    /* (ReadWrite, Bits 7:0) */
    /* Time window for DFP detection settlement (unit = 2.5usec)  Default=8'd4 (10usec) */
#define BIT_MSK__CCCTR2__RI_CC_DFP_FILTER_SETTLE_WINDOW_B7_B0                  0xFF

/* CC Control CTR3 Register */
#define REG_ADDR__CCCTR3                                                 (SABER_REG | 0x00AB)
    /* (ReadWrite, Bits 7:0) */
    /* Time window for DFP detection settlement (unit = 2.5usec)  Default=8'd4 (10usec) */
#define BIT_MSK__CCCTR3__RI_CC_DFP_FILTER_SETTLE_WINDOW_B15_B8                 0xFF

/* CC Control CTR4 Register */
#define REG_ADDR__CCCTR4                                                 (SABER_REG | 0x00AC)
    /* (ReadWrite, Bits 7:0) */
    /* Time window for UFP detection settlement - comparator  (unit = 2.5usec)  Default=8'd4 (10usec) */
#define BIT_MSK__CCCTR4__RI_CC_UFP_CP_FILTER_SETTLE_WINDOW_B7_B0               0xFF

/* CC Control CTR5 Register */
#define REG_ADDR__CCCTR5                                                 (SABER_REG | 0x00AD)
    /* (ReadWrite, Bits 7:0) */
    /* Time window for UFP detection settlement - comparator  (unit = 2.5usec)  Default=8'd4 (10usec) */
#define BIT_MSK__CCCTR5__RI_CC_UFP_CP_FILTER_SETTLE_WINDOW_B15_B8              0xFF

/* CC Control CTR6 Register */
#define REG_ADDR__CCCTR6                                                 (SABER_REG | 0x00AE)
    /* (ReadWrite, Bits 7:0) */
    /* Time window for UFP detection settlement - ADC (unit=128 cycles=320.00usec @ 400KHz)
    Default=16'h4 (1280usec) */
#define BIT_MSK__CCCTR6__RI_CC_UFP_ADC_FILTER_SETTLE_WINDOW_B7_B0              0xFF

/* CC Control CTR7 Register */
#define REG_ADDR__CCCTR7                                                 (SABER_REG | 0x00AF)
    /* (ReadWrite, Bits 7:0) */
    /* Time window for UFP detection settlement - ADC  (unit=128 cycles=320.00usec @ 400KHz)
    Default=16'h4 (1280usec) */
#define BIT_MSK__CCCTR7__RI_CC_UFP_ADC_FILTER_SETTLE_WINDOW_B15_B8             0xFF

/* CC Control CTR8 Register */
#define REG_ADDR__CCCTR8                                                 (SABER_REG | 0x00B0)
    /* (ReadWrite, Bits 5:0) */
    /* UFP detection: Voltage threshold for UFP attachment detection     0x38 ('d56): 2.184V (Typical)
    UFP attachemnt (ro_cc_ufp_adc_attached, ro_intr_cc_ufp_adc_attached) is declared if CC1 or CC2 pin
    is below than the specified voltage while toggle state is PullUpToggle or PullUpStop mode. Refer to Saber
    PD/CC Analog Core Specification for details. */
#define BIT_MSK__CCCTR8__RI_CC_UFP_ADC_FILTER_VOLTH_B5_B0                      0x3F

/* CC Control CTR9 Register */
#define REG_ADDR__CCCTR9                                                 (SABER_REG | 0x00B1)
    /* (ReadWrite, Bits 5:0) */
    /* UFP detection: Settle is declared if Min/Max difference is leass then allowance */
#define BIT_MSK__CCCTR9__RI_CC_UFP_ADC_FILTER_SETTLE_ALLOWANCE_B5_B0           0x3F

/* CC Control CTR10 Register */
#define REG_ADDR__CCCTR10                                                (SABER_REG | 0x00B2)
    /* (ReadWrite, Bits 2:0) */
    /* ADC mode: The specified ADC mode is applied by writing the following value: 3'b000=AUTO:
    Automatic on/off mode 3'b001=AON_MOFF: Automatic on/Manual off mode (Default) Hardware
    automatically turns on ADC when connection is detected. Software may rewrite 3'b001 to manually
    turn off ADC and restarts the procedure. This is used when cable is detached and Sabre is waiting
    for next attachment. 3'b010=MON_AOFF: Manual on/Automatic off mode 3'b101=FON: Force on
    3'b110=FOFF: Force off */
#define BIT_MSK__CCCTR10__RI_CC_ADC_MODE_B2_B0                                  0x07
    /* (ReadWrite, Bits 3) */
    /* ADC mode write strobe: If ri_pdcc_wp_manual = '0':   Software write ri_cc_adc_mode[2:0]
    to apply ADC mode without writing to this field.   Write to this field has no effect. If
    ri_pdcc_wp_manual = '1':   Write '1' to this field to notify to apply ADC mode (write pulse)   Software
    need to write '0' to deassert the strobe for next transaction. */
#define BIT_MSK__CCCTR10__RI_CC_ADC_MODE_WP                                     0x08

/* CC Control CTR11 Register */
#define REG_ADDR__CCCTR11                                                (SABER_REG | 0x00B3)
    /* (ReadWrite, Bits 7:0) */
    /* ADC sleep duration. Default=8'h0 (disabled sleeping) */
#define BIT_MSK__CCCTR11__RI_CC_ADC_SLEEP_B7_B0                                 0xFF

#define REG_ADDR__CCCTR12                                                (SABER_REG | 0x00B4)

/* CC Control CTR13 Register */
#define REG_ADDR__CCCTR13                                                (SABER_REG | 0x00B5)
    /* (ReadWrite, Bits 0) */
    /* Use ufp_cp_detached for automatic off Default=1'b0 */
#define BIT_MSK__CCCTR13__RI_CC_ADC_USE_CP_FOR_AOFF                             0x01
    /* (ReadWrite, Bits 1) */
    /* Use ufp_cp_attached for stopping toggle Default=1'b0 */
#define BIT_MSK__CCCTR13__RI_CC_DRP_USE_CP_FOR_STOP                             0x02

/* CC Control CTR14 Register */
#define REG_ADDR__CCCTR14                                                (SABER_REG | 0x00B6)
    /* (ReadWrite, Bits 7:0) */
    /* Number of 400KHz clk cycles for 5msec Default=5000000/2500=16'd2000=16'h7D0 */
#define BIT_MSK__CCCTR14__RI_CC_DRP_5MSEC_B7_B0                                 0xFF

/* CC Control CTR15 Register */
#define REG_ADDR__CCCTR15                                                (SABER_REG | 0x00B7)
    /* (ReadWrite, Bits 7:0) */
    /* Number of 400KHz clk cycles for 5msec Default=5000000/2500=16'd2000=16'h7D0 */
#define BIT_MSK__CCCTR15__RI_CC_DRP_5MSEC_B15_B8                                0xFF

/* CC Control CTR16 Register */
#define REG_ADDR__CCCTR16                                                (SABER_REG | 0x00B8)
    /* (ReadWrite, Bits 7:0) */
    /* The period a Dp/Dn toggling stays at pull-down (Rd) state 10'h0=5msec Default=8'h8 (40msec) */
#define BIT_MSK__CCCTR16__RI_CC_DRP_DN_B7_B0                                    0xFF

/* CC Control CTR17 Register */
#define REG_ADDR__CCCTR17                                                (SABER_REG | 0x00B9)
    /* (ReadWrite, Bits 1:0) */
    /* The period a Dp/Dn toggling stays at pull-down (Rd) state 10'h0=5msec Default=8'h8 (40msec) */
#define BIT_MSK__CCCTR17__RI_CC_DRP_DN_B9_B8                                    0x03

/* CC Control CTR18 Register */
#define REG_ADDR__CCCTR18                                                (SABER_REG | 0x00BA)
    /* (ReadWrite, Bits 7:0) */
    /* The period a Dp/Dn toggling stays at pull-up   (Rp) state 10'h0=5msec Default=8'h8 (40msec) */
#define BIT_MSK__CCCTR18__RI_CC_DRP_UP_B7_B0                                    0xFF

/* CC Control CTR19 Register */
#define REG_ADDR__CCCTR19                                                (SABER_REG | 0x00BB)
    /* (ReadWrite, Bits 1:0) */
    /* The period a Dp/Dn toggling stays at pull-up   (Rp) state 10'h0=5msec Default=8'h8 (40msec) */
#define BIT_MSK__CCCTR19__RI_CC_DRP_UP_B9_B8                                    0x03

/* CC Control CTR20 Register */
#define REG_ADDR__CCCTR20                                                (SABER_REG | 0x00BC)
    /* (ReadWrite, Bits 7:0) */
    /* The period a Dp/Dn toggling stays at pull-down wait state 12'h0=1 cycle (2.5usec) (unit=2.5usec)
    Default=12'h240=12'd576=1.44msec (] tDRPTransition 1msec) (] ri_cc_ufp_adc_filter_settle_window 1280usec) */
#define BIT_MSK__CCCTR20__RI_CC_DRP_DN_WAIT_B7_B0                               0xFF

/* CC Control CTR21 Register */
#define REG_ADDR__CCCTR21                                                (SABER_REG | 0x00BD)
    /* (ReadWrite, Bits 3:0) */
    /* The period a Dp/Dn toggling stays at pull-down wait state (PullDnToggleWait)  During
    PullDnToggleWait state, connection detection is temporarily disabled since the resistor is just switched from
    Rp to Rd. Refer to description of ri_cc_toggle_mode. 12'h0=1 cycle (2.5usec) (unit=2.5usec)
    Default=12'h240=12'd576=1.44msec (] tDRPTransition 1msec)  (] ri_cc_ufp_adc_filter_settle_window 1280usec) */
#define BIT_MSK__CCCTR21__RI_CC_DRP_DN_WAIT_B11_B8                              0x0F
    /* (ReadWrite, Bits 7:4) */
    /* The period a Dp/Dn toggling stays at pull-up wait state (PullUpToggleWait) During PullUpToggleWait state,
    connection detection is temporarily disabled since the resistor is just switched from Rd to Rp. Refer to
    description of ri_cc_toggle_mode. 12'h0=1 cycle (2.5usec) (unit=2.5usec) Default=12'h240=12'd576=1.44msec
    (] tDRPTransition 1msec)  (] ri_cc_ufp_adc_filter_settle_window 1280usec) */
#define BIT_MSK__CCCTR21__RI_CC_DRP_UP_WAIT_B3_B0                               0xF0

/* CC Control CTR22 Register */
#define REG_ADDR__CCCTR22                                                (SABER_REG | 0x00BE)
    /* (ReadWrite, Bits 7:0) */
    /* The period a Dp/Dn toggling stays at pull-up wait state 12'h0=1 cycle (2.5usec) (unit=2.5usec)
    Default=12'h240=12'd576=1.44msec (] tDRPTransition 1msec)  (] ri_cc_ufp_adc_filter_settle_window 1280usec) */
#define BIT_MSK__CCCTR22__RI_CC_DRP_UP_WAIT_B11_B4                              0xFF

/* CC Control CTR23 Register */
#define REG_ADDR__CCCTR23                                                (SABER_REG | 0x00BF)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CCCTR23__RI_CC_CONF23                                          0xFF

/* CC Control CTR24 Register */
#define REG_ADDR__CCCTR24                                                (SABER_REG | 0x00C0)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CCCTR24__RI_CC_CONF24                                          0xFF

/* CC Control CTR25 Register */
#define REG_ADDR__CCCTR25                                                (SABER_REG | 0x00C1)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CCCTR25__RI_CC_CONF25                                          0xFF

/* CC Control CTR26 Register */
#define REG_ADDR__CCCTR26                                                (SABER_REG | 0x00C2)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CCCTR26__RI_CC_CONF26                                          0xFF

/* CC Control CTR27 Register */
#define REG_ADDR__CCCTR27                                                (SABER_REG | 0x00C3)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CCCTR27__RI_CC_CONF27                                          0xFF

/* CC Control CTR28 Register */
#define REG_ADDR__CCCTR28                                                (SABER_REG | 0x00C4)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CCCTR28__RI_CC_CONF28                                          0xFF

/* CC Control CTR29 Register */
#define REG_ADDR__CCCTR29                                                (SABER_REG | 0x00C5)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CCCTR29__RI_CC_CONF29                                          0xFF

/* CC Control CTR30 Register */
#define REG_ADDR__CCCTR30                                                (SABER_REG | 0x00C6)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CCCTR30__RI_CC_CONF30                                          0xFF

/* CC Control CTR31 Register */
#define REG_ADDR__CCCTR31                                                (SABER_REG | 0x00C7)
    /* (ReadWrite, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CCCTR31__RI_CC_CONF31                                          0xFF

/* CC Connection Status Register */
#define REG_ADDR__CC_CONN_STAT                                           (SABER_REG | 0x00C8)
    /* (ReadOnly, Bits 0) */
    /* DFP attached */
#define BIT_MSK__CC_CONN_STAT__RO_CC_DFP_ATTACHED                                    0x01
    /* (ReadOnly, Bits 1) */
    /* DFP detached */
#define BIT_MSK__CC_CONN_STAT__RO_CC_DFP_DETACHED                                    0x02
    /* (ReadOnly, Bits 2) */
    /* UFP attached - Comparator */
#define BIT_MSK__CC_CONN_STAT__RO_CC_UFP_CP_ATTACHED                                 0x04
    /* (ReadOnly, Bits 3) */
    /* UFP deatache - Comparator */
#define BIT_MSK__CC_CONN_STAT__RO_CC_UFP_CP_DETACHED                                 0x08
    /* (ReadOnly, Bits 4) */
    /* UFP attached - ADC */
#define BIT_MSK__CC_CONN_STAT__RO_CC_UFP_ADC_ATTACHED                                0x10
    /* (ReadOnly, Bits 5) */
    /* UFP detached - ADC */
#define BIT_MSK__CC_CONN_STAT__RO_CC_UFP_ADC_DETACHED                                0x20
    /* (ReadOnly, Bits 7:6) */
    /* Hardware-assisted connection status: 2'h0=Disconnected 2'h1=Reserved 2'h2=UFP attached
    2'h3=DFP attached */
#define BIT_MSK__CC_CONN_STAT__RO_CC_HA_STATUS_B1_B0                                 0xC0

/* CC ADC1 Status Register */
#define REG_ADDR__CC1VOL                                                 (SABER_REG | 0x00C9)
    /* (ReadOnly, Bits 5:0) */
    /* Current Voltage Level on CC1 pin: Please refer to the section 4.11.3 of USB Type-C Specification
    for CC voltage levels. */
#define BIT_MSK__CC1VOL__RO_ADC1_OUT_B5_B0                                     0x3F
    /* (ReadOnly, Bits 6) */
    /* Settlement of Voltage Level on CC1 pin: This bit is set if the difference between minmum and
    maximum of CC1 voltage is less than ri_cc_ufp_adc_filter_settle_allowance[5:0] within a time window
    ri_cc_ufp_adc_filter_settle_window[15:0] */
#define BIT_MSK__CC1VOL__RO_ADC1_SETTLE                                        0x40
    /* (ReadOnly, Bits 7) */
    /* UFP detected on CC1 pin based on ADC: This bit is set if the CC1 voltage is below
    ri_cc_ufp_adc_filter_volth[5:0]. */
#define BIT_MSK__CC1VOL__RO_ADC1_DET                                           0x80

/* CC ADC2 Status Register */
#define REG_ADDR__CC2VOL                                                 (SABER_REG | 0x00CA)
    /* (ReadOnly, Bits 5:0) */
    /* Current Voltage Level on CC2 pin: Please refer to the section 4.11.3 of USB Type-C Specification
    for CC voltage levels. */
#define BIT_MSK__CC2VOL__RO_ADC2_OUT_B5_B0                                     0x3F
    /* (ReadOnly, Bits 6) */
    /* Settlement of Voltage Level on CC2 pin: This bit is set if the difference between minmum and maximum
    of CC1 voltage is less than ri_cc_ufp_adc_filter_settle_allowance[5:0] within a time window
    ri_cc_ufp_adc_filter_settle_window[15:0] */
#define BIT_MSK__CC2VOL__RO_ADC2_SETTLE                                        0x40
    /* (ReadOnly, Bits 7) */
    /* UFP detected on CC2 pin based on ADC: This bit is set if the CC1 voltage is below
    ri_cc_ufp_adc_filter_volth[5:0]. */
#define BIT_MSK__CC2VOL__RO_ADC2_DET                                           0x80

/* CC 2.4MHz domain STAT0 Register */
#define REG_ADDR__CC24STAT0                                              (SABER_REG | 0x00CB)
    /* (ReadOnly, Bits 7:0) */
    /* Bit 2:0: Current ADC controller state   3'h0=ADC is off (waiting for connection)   3'h1=ADC is on
    (waiting for ADC ready)   3'h2=ADC is on (normal operation)   3'h3=ADC is in power saving mode Bit 3
    : Current ADC enable/disable state (cc_adc_en)   1'b0=ADC is enabled   1'b1=ADC is disabled Bit 6:4:
    Current toggle controller state   3'h000=PullDnStop   3'h001=PullUpStop   3'h010=PullDnToggle   3'h011=PullUpToggle
    3'h110=PullDnToggleWait   3'h111=PullUpToggleWait Bit 7   : Current Pullup/Pulldown state (mode_dfp_ufp)
    1'b0=Pull down   1'b1=Pull up */
#define BIT_MSK__CC24STAT0__RO_CC24_STATUS_B7_B0                                  0xFF

/* CC 2.4MHz domain STAT1 Register */
#define REG_ADDR__CC24STAT1                                              (SABER_REG | 0x00CC)
    /* (ReadOnly, Bits 7:0) */
    /* Bit  8: int_vbus_det value from analog core Bit  9: vbus_det_out: Filtered value of int_vbus_det Bit 10:
    vbus_det_settle: vbus_det_out is stable Bit 11: cc_attached value from analog core Bit 12: cc_attached_out:
    Filtered value of cc_attached Bit 13: cc_attached_settle: cc_attached_out is stable Bit 14: int_battery_good
    value from analog core Bit 15: reserved */
#define BIT_MSK__CC24STAT1__RO_CC24_STATUS_B15_B8                                 0xFF

/* CC 2.4MHz domain STAT2 Register */
#define REG_ADDR__CC24STAT2                                              (SABER_REG | 0x00CD)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CC24STAT2__RO_CC24_STATUS_B23_B16                                0xFF

/* CC 2.4MHz domain STAT3 Register */
#define REG_ADDR__CC24STAT3                                              (SABER_REG | 0x00CE)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CC24STAT3__RO_CC24_STATUS_B31_B24                                0xFF

/* CC 2.4MHz domain STAT4 Register */
#define REG_ADDR__CC24STAT4                                              (SABER_REG | 0x00CF)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CC24STAT4__RO_CC24_STATUS_B39_B32                                0xFF

/* CC 2.4MHz domain STAT5 Register */
#define REG_ADDR__CC24STAT5                                              (SABER_REG | 0x00D0)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CC24STAT5__RO_CC24_STATUS_B47_B40                                0xFF

/* CC 2.4MHz domain STAT6 Register */
#define REG_ADDR__CC24STAT6                                              (SABER_REG | 0x00D1)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CC24STAT6__RO_CC24_STATUS_B55_B48                                0xFF

/* CC 2.4MHz domain STAT7 Register */
#define REG_ADDR__CC24STAT7                                              (SABER_REG | 0x00D2)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CC24STAT7__RO_CC24_STATUS_B63_B56                                0xFF

/* CC 2.4MHz domain STAT8 Register */
#define REG_ADDR__CC24STAT8                                              (SABER_REG | 0x00D3)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CC24STAT8__RO_CC24_STATUS_B71_B64                                0xFF

/* CC 2.4MHz domain STAT9 Register */
#define REG_ADDR__CC24STAT9                                              (SABER_REG | 0x00D4)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CC24STAT9__RO_CC24_STATUS_B79_B72                                0xFF

/* CC 2.4MHz domain STAT10 Register */
#define REG_ADDR__CC24STAT10                                             (SABER_REG | 0x00D5)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CC24STAT10__RO_CC24_STATUS_B87_B80                                0xFF

/* CC 2.4MHz domain STAT11 Register */
#define REG_ADDR__CC24STAT11                                             (SABER_REG | 0x00D6)
    /* (ReadOnly, Bits 7:0) */
    /* Reserved */
#define BIT_MSK__CC24STAT11__RO_CC24_STATUS_B95_B88                                0xFF

/* Analog Switch Control0 Register */
#define REG_ADDR__ANA_SWCH_CTRL0                                         (SABER_REG | 0x00E8)
    /* (ReadWrite, Bits 0) */
    /* Analog Switch Enable: This register field controls the enabling of the Analog Switch. 0 - Disable 1 - Enable */
#define BIT_MSK__ANA_SWCH_CTRL0__RI_ANA_SWCH_EN                                        0x01
    /* (ReadWrite, Bits 1) */
    /* Analog Switch SBUin0 Enable: This register field controls the enabling of the Analog Switch SBUin0.
    0 - Disable 1 - Enable */
#define BIT_MSK__ANA_SWCH_CTRL0__RI_EN_SBUIN0                                          0x02
    /* (ReadWrite, Bits 2) */
    /* Analog Switch SBUin1 Enable: This register field controls the enabling of the Analog Switch SBUin1.
    0 - Disable 1 - Enable */
#define BIT_MSK__ANA_SWCH_CTRL0__RI_EN_SBUIN1                                          0x04

/* Analog Switch Control1 Register */
#define REG_ADDR__ANA_SWCH_CTRL1                                         (SABER_REG | 0x00E9)
    /* (ReadWrite, Bits 0) */
    /* Analog Switch Oscillator Enable: This register field controls the enabling of the Analog Switch Oscillator.
    0 - Disable 1 - Enable */
#define BIT_MSK__ANA_SWCH_CTRL1__RI_OSC_EN                                             0x01
    /* (ReadWrite, Bits 1) */
    /* Analog Switch Bypass Comparator: This register field controls the bypassing of the Analog Switch Comparator.
    0 - Not Bypassed 1 - Bypassed */
#define BIT_MSK__ANA_SWCH_CTRL1__RI_BYP_COMP                                           0x02
    /* (ReadWrite, Bits 7:4) */
    /* Analog Switch Oscillator Calibration Value: This register field controls the Calibration value of the Oscillator
    in the Analog Switch. */
#define BIT_MSK__ANA_SWCH_CTRL1__RI_OSC_CTL                                            0xF0

/* Analog Switch Test Bus 0 Register */
#define REG_ADDR__ANA_SWCH_TBUS0                                         (SABER_REG | 0x00EA)
    /* (ReadWrite, Bits 7:0) */
    /* Analog Switch Test Bus: This register field controls bits [7:0] of Analog Switch input testbus. */
#define BIT_MSK__ANA_SWCH_TBUS0__RI_TESTBUS_B7_B0                                      0xFF

/* Analog Switch Test Bus 1 Register */
#define REG_ADDR__ANA_SWCH_TBUS1                                         (SABER_REG | 0x00EB)
    /* (ReadWrite, Bits 7:0) */
    /* Analog Switch Test Bus: This register field controls bits [15:8] of Analog Switch input testbus. */
#define BIT_MSK__ANA_SWCH_TBUS1__RI_TESTBUS_B15_B8                                     0xFF

/* Analog Switch Control2 Register */
#define REG_ADDR__ANA_SWCH_CTRL2                                         (SABER_REG | 0x00EC)
    /* (ReadWrite, Bits 7:0) */
    /* Analog Switch Test Bus: This register field controls bits [7:0] of Analog Switch input Sw. */
#define BIT_MSK__ANA_SWCH_CTRL2__RI_ANA_SWCH_B7_B0                                     0xFF

/* Analog Switch Control3 Register */
#define REG_ADDR__ANA_SWCH_CTRL3                                         (SABER_REG | 0x00ED)
    /* (ReadWrite, Bits 7:0) */
    /* Analog Switch Test Bus: This register field controls bits [15:8] of Analog Switch input Sw. */
#define BIT_MSK__ANA_SWCH_CTRL3__RI_ANA_SWCH_B15_B8                                    0xFF

/* Analog Switch Control4 Register */
#define REG_ADDR__ANA_SWCH_CTRL4                                         (SABER_REG | 0x00EE)
    /* (ReadWrite, Bits 7:0) */
    /* Analog Switch Test Bus: This register field controls bits [23:16] of Analog Switch input Sw. */
#define BIT_MSK__ANA_SWCH_CTRL4__RI_ANA_SWCH_B23_B16                                   0xFF

/* Analog CCPD Default Resistor Tune Register */
#define REG_ADDR__ANA_CCPD_DEF_TUNE                                      (SABER_REG | 0x00F0)
    /* (ReadWrite, Bits 1:0) */
    /* Analog CCPD Default Resistor Tune0 This register field controls the Default Resistor Tune0 of Analog CCPD. */
#define BIT_MSK__ANA_CCPD_DEF_TUNE__RI_RES_DEFAULT_TUNE0                                  0x03
    /* (ReadWrite, Bits 5:4) */
    /* Analog CCPD Default Resistor Tune1 This register field controls the Default Resistor Tune1 of Analog CCPD. */
#define BIT_MSK__ANA_CCPD_DEF_TUNE__RI_RES_DEFAULT_TUNE1                                  0x30

/* Analog CCPD Pull-down Resistor Tune Register */
#define REG_ADDR__ANA_CCPD_RD_TUNE                                       (SABER_REG | 0x00F1)
    /* (ReadWrite, Bits 1:0) */
    /* Analog CCPD Pull-down Resistor Tune0 This register field controls the Pull-down Resistor Tune0 of Analog CCPD. */
#define BIT_MSK__ANA_CCPD_RD_TUNE__RI_RES_RD_TUNE0                                       0x03
    /* (ReadWrite, Bits 5:4) */
    /* Analog CCPD Pull-down Resistor Tune1 This register field controls the Pull-down Resistor Tune1 of Analog CCPD. */
#define BIT_MSK__ANA_CCPD_RD_TUNE__RI_RES_RD_TUNE1                                       0x30

/* Analog CCPD 1.5A Resistor Tune Register */
#define REG_ADDR__ANA_CCPD_1P5A_TUNE                                     (SABER_REG | 0x00F2)
    /* (ReadWrite, Bits 2:0) */
    /* Analog CCPD 1.5A Resistor Tune0 This register field controls the 1.5A Resistor Tune0 of Analog CCPD. */
#define BIT_MSK__ANA_CCPD_1P5A_TUNE__RI_RES_1P5A_TUNE0                                     0x07
    /* (ReadWrite, Bits 6:4) */
    /* Analog CCPD 1.5A Resistor Tune1 This register field controls the 1.5A Resistor Tune1 of Analog CCPD. */
#define BIT_MSK__ANA_CCPD_1P5A_TUNE__RI_RES_1P5A_TUNE1                                     0x70

/* Analog CCPD 3A Resistor Tune Register */
#define REG_ADDR__ANA_CCPD_3A_TUNE                                       (SABER_REG | 0x00F3)
    /* (ReadWrite, Bits 2:0) */
    /* Analog CCPD 3A Resistor Tune0 This register field controls the 3A Resistor Tune0 of Analog CCPD. */
#define BIT_MSK__ANA_CCPD_3A_TUNE__RI_RES_3A_TUNE0                                       0x07
    /* (ReadWrite, Bits 6:4) */
    /* Analog CCPD 3A Resistor Tune1 This register field controls the 3A Resistor Tune1 of Analog CCPD. */
#define BIT_MSK__ANA_CCPD_3A_TUNE__RI_RES_3A_TUNE1                                       0x70

/* Analog CCPD Mode Control Register */
#define REG_ADDR__ANA_CCPD_MODE_CTRL                                     (SABER_REG | 0x00F4)
    /* (ReadWrite, Bits 1:0) */
    /* Analog CCPD Power Mode 0 - Default 1 - 1.5A 2 - 3A 3 - reserved */
#define BIT_MSK__ANA_CCPD_MODE_CTRL__RI_POWER_MODE                                         0x03
    /* (ReadWrite, Bits 4) */
    /* 0 - Auto Mode 1 - Manual Mode */
#define BIT_MSK__ANA_CCPD_MODE_CTRL__RI_MANUAL_AUTOMODEB                                   0x10
    /* (ReadWrite, Bits 5) */
    /* 0 - Do Not Bypass 2.4MHz Oscillator 1 - Bypass 2.4MHz Oscillator */
#define BIT_MSK__ANA_CCPD_MODE_CTRL__RI_BYPASS_2P4MHZ                                      0x20

/* Analog CCPD Manual Resistor Control Register */
#define REG_ADDR__ANA_CCPD_RES_CTRL                                      (SABER_REG | 0x00F5)
    /* (ReadWrite, Bits 1:0) */
    /* Manually Turn ON Default Resistor */
#define BIT_MSK__ANA_CCPD_RES_CTRL__RI_DEFAULT_ON                                         0x03
    /* (ReadWrite, Bits 3:2) */
    /* Manually Turn ON Pull-down Resistor */
#define BIT_MSK__ANA_CCPD_RES_CTRL__RI_RD_ON                                              0x0C
    /* (ReadWrite, Bits 5:4) */
    /* Manually Turn ON 1.5A Resistor */
#define BIT_MSK__ANA_CCPD_RES_CTRL__RI_1P5A_ON                                            0x30
    /* (ReadWrite, Bits 7:6) */
    /* Manually Turn ON 3A Resistor */
#define BIT_MSK__ANA_CCPD_RES_CTRL__RI_3A_ON                                              0xC0

/* Analog CCPD Driver Control Register */
#define REG_ADDR__ANA_CCPD_DRV_CTRL                                      (SABER_REG | 0x00F6)
    /* (ReadWrite, Bits 1:0) */
    /* PD Driver Slew Rate Control */
#define BIT_MSK__ANA_CCPD_DRV_CTRL__RI_SR                                                 0x03
    /* (ReadWrite, Bits 3:2) */
    /* PD Input Pole Position Control */
#define BIT_MSK__ANA_CCPD_DRV_CTRL__RI_R_SEL                                              0x0C
    /* (ReadWrite, Bits 6:4) */
    /* PD Driver Level Tuning bits */
#define BIT_MSK__ANA_CCPD_DRV_CTRL__RI_DRV_LEVEL_SEL                                      0x70
    /* (ReadWrite, Bits 7) */
    /* VCONN Enable */
#define BIT_MSK__ANA_CCPD_DRV_CTRL__RI_VCONN_EN                                           0x80

/* Analog CCPD Calibration Value Register */
#define REG_ADDR__ANA_CCPD_CAL_VAL                                       (SABER_REG | 0x00F7)
    /* (ReadWrite, Bits 3:0) */
    /* Coarse Calibration Value */
#define BIT_MSK__ANA_CCPD_CAL_VAL__RI_CAL_VAL_COARSE                                     0x0F

/* Analog CCPD Control0 Register */
#define REG_ADDR__ANA_CCPD_CTRL0                                         (SABER_REG | 0x00F8)
    /* (ReadWrite, Bits 0) */
    /* Firmware programs this bit with the Cable Connection status */
#define BIT_MSK__ANA_CCPD_CTRL0__RI_CABLE_CONN                                         0x01
    /* (ReadWrite, Bits 1) */
    /* This bit controls the forceable turn ON/OFF of the LDO and BGR. Implemented for test/debug purposes. */
#define BIT_MSK__ANA_CCPD_CTRL0__RI_FORCE_ON                                           0x02
    /* (ReadWrite, Bits 5:4) */
    /* Select pin for LDO/BGR ON 2'h0 - adc_en 2'h1 - force_on register 2'h2 - cable_conn register 2'h3
    - dont care or reserved */
#define BIT_MSK__ANA_CCPD_CTRL0__RI_POR_SEL                                            0x30

/* Analog CCPD Control1 Register */
#define REG_ADDR__ANA_CCPD_CTRL1                                         (SABER_REG | 0x00F9)
    /* (ReadWrite, Bits 7:0) */
    /* This register field controls bits [7:0] of Analog CCPD input rsvd. */
#define BIT_MSK__ANA_CCPD_CTRL1__RI_RSVD                                               0xFF

#endif				/* _HEADER_H_ */
