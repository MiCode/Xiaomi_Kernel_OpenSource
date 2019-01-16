/**
 * Copyright (C) 2006 NXP N.V., All Rights Reserved.
 * This source code and any compilation or derivative thereof is the proprietary
 * information of NXP N.V. and is confidential in nature. Under no circumstances
 * is this software to be  exposed to or placed under an Open Source License of
 * any type without the expressed written permission of NXP N.V.
 *
 * \file          tmdlHdmiTx_cfg.c
 *
 * \version       Revision: 1
 *
 * \date          Date: 25/03/11 11:00
 *
 * \brief         devlib driver component API for the TDA998x HDMI Transmitters
 *
 * \section refs  Reference Documents
 * HDMI Tx Driver - FRS.doc,
 *
 * \section info  Change Information
 *
 * \verbatim

   History:       tmdlHdmiTx_cfg.c
 *
 * *****************  Version 2  *****************
 * User: V. Vrignaud Date: March 25th, 2011
 *
 * *****************  Version 1  *****************
 * User: J. Lamotte Date: 08/08/07  Time: 11:00
 * initial version
 *

   \endverbatim
 *
*/

/*============================================================================*/
/*                       INCLUDE FILES                                        */
/*============================================================================*/

#ifndef TMFL_TDA19989
#define TMFL_TDA19989
#endif

#ifndef TMFL_NO_RTOS
#define TMFL_NO_RTOS
#endif

#ifndef TMFL_LINUX_OS_KERNEL_DRIVER
#define TMFL_LINUX_OS_KERNEL_DRIVER
#endif


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <mach/mt_gpio.h>
#include <linux/slab.h>
#include "tmNxCompId.h"
#include "tmdlHdmiTx_Types.h"
#include "tmdlHdmiTx_Functions.h"



#include "tmNxTypes.h"
#include "tmbslHdmiTx_types.h"
#include "tmdlHdmiTx_cfg.h"
#include "tmdlHdmiTx_IW.h"
/* macro for quick error handling */
#define RETIF(cond, rslt) if ((cond)) {return (rslt); }
#define I2C_M_WR 0

/*============================================================================*/
/*                   STATIC FUNCTION DECLARATIONS                             */
/*============================================================================*/
tmErrorCode_t TxI2cReadFunction(tmbslHdmiTxSysArgs_t *pSysArgs);
tmErrorCode_t TxI2cWriteFunction(tmbslHdmiTxSysArgs_t *pSysArgs);

/******************************************************************************
 ******************************************************************************
 *                 THIS PART CAN BE MODIFIED BY CUSTOMER                      *
 ******************************************************************************
 *****************************************************************************/
struct i2c_client *GetThisI2cClient(void);
unsigned char my_i2c_data[255];

/* The following includes are used by I2C access function. If    */
/* you need to rewrite these functions for your own SW infrastructure, then   */
/* it can be removed                                                          */
#	include <linux/kernel.h>
#	include <linux/errno.h>
#	include <linux/string.h>
#	include <linux/types.h>
#	include <linux/i2c.h>
#	include <linux/delay.h>

/* I2C adress of the unit                                                     */
/* Put there the I2C slave adress of the Tx transmitter IC                    */
#define UNIT_I2C_ADDRESS_0 0x70

/* Intel CE 4100 I2C bus number                                               */
/* Put there the number of I2C bus handling the Rx transmitter IC             */
#define I2C_BUS_NUMBER_0 0	/* initial:0 */

/* I2C Number of bytes in the data buffer.                                    */
#define SUB_ADDR_BYTE_COUNT_0 1

/* Priority of the command task                                               */
/* Command task is an internal task that handles incoming event from the IC   */
/* put there a value that will ensure a response time of ~20ms in your system */
#define COMMAND_TASK_PRIORITY_0  250
#define COMMAND_TASK_PRIORITY_1  250

#include "tmdlHdmiTx_IW.h"
#include "tmdlHdmiTx.h"
#include "tmdlHdmiTx_cfg.h"
#include "I2C.h"

#ifdef TMFL_CEC_AVAILABLE
#include "tmdlHdmiCEC_functions.h"
#define CEC_UNIT_I2C_ADDRESS_0 0x34
#define CEC_UNIT_I2C_ADDRESS_1 0x34
#endif

/* #ifdef TMFL_LINUX_OS_KERNEL_DRIVER */
#if defined(TMFL_CFG_ZOOMII)	/* OMAP Zoom II */
#	include "tmdlHdmiTx_Linux_cfg.c"
#elif defined(TMFL_CFG_INTELCE4100)	/* Intel CE 4100 */
#	include "tmdlHdmiTx_IntelCE4100_cfg.c"
#elif defined(TMFL_OS_WINDOWS)	/* Windows demo application */
#	include "tmdlHdmiTx_Win_cfg.c"
#else				/* Section to be modified by customer - Default configuration for NXP evalkit */

/*============================================================================*/
/*                                MACRO                                       */
/*============================================================================*/

/*============================================================================*/
/*                   STATIC FUNCTION DECLARATIONS                             */
/*============================================================================*/
tmErrorCode_t TxI2cReadFunction(tmbslHdmiTxSysArgs_t *pSysArgs);
tmErrorCode_t TxI2cWriteFunction(tmbslHdmiTxSysArgs_t *pSysArgs);

/******************************************************************************
 ******************************************************************************
 *                 THIS PART CAN BE MODIFIED BY CUSTOMER                      *
 ******************************************************************************
 *****************************************************************************/
/* The following includes are used by I2C access function for ARM7. If    */
/* you need to rewrite these functions for your own SW infrastructure, then   */
/* it can be removed                                                          */
/* #include "I2C.h" */
/* #include <LPC21xx.H> */

/* I2C adress of the unit                                                     */
/* Put there the I2C slave adress of the Tx transmitter IC                    */
#define UNIT_I2C_ADDRESS_0 0x70

/* Priority of the command task                                               */
/* Command task is an internal task that handles incoming event from the IC   */
/* put there a value that will ensure a response time of ~20ms in your system */
#define COMMAND_TASK_PRIORITY_0  250
#define COMMAND_TASK_PRIORITY_1  250

/* Priority of the hdcp check tasks */
/* HDCP task is an internal task that handles periodical HDCP processing      */
/* put there a value that will ensure a response time of ~20ms in your system */
#define HDCP_CHECK_TASK_PRIORITY_0  250

/* Stack size of the command tasks */
/* This value depends of the type of CPU used, and also from the length of    */
/* the customer callbacks. Increase this value if you are making a lot of     */
/* processing (function calls & local variables) and that you experience      */
/* stack overflows                                                            */
#define COMMAND_TASK_STACKSIZE_0 128
#define COMMAND_TASK_STACKSIZE_1 128

/* stack size of the hdcp check tasks */
/* This value depends of the type of CPU used, default value should be enough */
/* for all configuration                                                      */
#define HDCP_CHECK_TASK_STACKSIZE_0 128

/* Size of the message queues for command tasks                               */
/* This value defines the size of the message queue used to link the          */
/* the tmdlHdmiTxHandleInterrupt function and the command task. The default   */
/* value below should fit any configuration                                   */
#define COMMAND_TASK_QUEUESIZE_0 128
#define COMMAND_TASK_QUEUESIZE_1 128

/* HDCP key seed                                                              */
/* HDCP key are stored encrypted into the IC, this value allows the IC to     */
/* decrypt them. This value is provided to the customer by NXP customer       */
/* support team.                                                              */
#define KEY_SEED 0x1234

/* Video port configuration for YUV444 input                                  */
/* You can specify in this table how are connected video ports in case of     */
/* YUV444 input signal. Each line of the array corresponds to a quartet of    */
/* pins of one video port (see comment on the left to identify them). Just    */
/* change the enum to specify which signal you connected to it. See file      */
/* tmdlHdmiTx_cfg.h to get the list of possible values                        */
const tmdlHdmiTxCfgVideoSignal444 videoPortMapping_YUV444[MAX_UNITS][6] = {
	{
	 TMDL_HDMITX_VID444_GY_0_TO_3,	/* Signals connected to VPB[0..3] */
	 TMDL_HDMITX_VID444_GY_4_TO_7,	/* Signals connected to VPB[4..7] */
	 TMDL_HDMITX_VID444_BU_0_TO_3,	/* Signals connected to VPA[0..3] */
	 TMDL_HDMITX_VID444_BU_4_TO_7,	/* Signals connected to VPA[4..7] */
	 TMDL_HDMITX_VID444_VR_0_TO_3,	/* Signals connected to VPC[0..3] */
	 TMDL_HDMITX_VID444_VR_4_TO_7	/* Signals connected to VPC[4..7] */
	 }
};

/* Video port configuration for RGB444 input                                  */
/* You can specify in this table how are connected video ports in case of     */
/* RGB444 input signal. Each line of the array corresponds to a quartet of    */
/* pins of one video port (see comment on the left to identify them). Just    */
/* change the enum to specify which signal you connected to it. See file      */
/* tmdlHdmiTx_cfg.h to get the list of possible values                        */
const tmdlHdmiTxCfgVideoSignal444 videoPortMapping_RGB444[MAX_UNITS][6] = {
	{
	 TMDL_HDMITX_VID444_VR_0_TO_3,	/* Signals connected to VPC[0..3] */
	 TMDL_HDMITX_VID444_VR_4_TO_7,	/* Signals connected to VPC[4..7] */

	 TMDL_HDMITX_VID444_BU_0_TO_3,	/* Signals connected to VPA[0..3] */
	 TMDL_HDMITX_VID444_BU_4_TO_7,	/* Signals connected to VPA[4..7] */
	 TMDL_HDMITX_VID444_GY_0_TO_3,	/* Signals connected to VPB[0..3] */
	 TMDL_HDMITX_VID444_GY_4_TO_7,	/* Signals connected to VPB[4..7] */


	 }
};

/* Video port configuration for YUV422 input                                  */
/* You can specify in this table how are connected video ports in case of     */
/* YUV422 input signal. Each line of the array corresponds to a quartet of    */
/* pins of one video port (see comment on the left to identify them). Just    */
/* change the enum to specify which signal you connected to it. See file      */
/* tmdlHdmiTx_cfg.h to get the list of possible values                        */
const tmdlHdmiTxCfgVideoSignal422 videoPortMapping_YUV422[MAX_UNITS][6] = {
	{
	 TMDL_HDMITX_VID422_NOT_CONNECTED,	/* Signals connected to VPC[0..3] */
	 TMDL_HDMITX_VID422_NOT_CONNECTED,	/* Signals connected to VPB[4..7] */
	 TMDL_HDMITX_VID422_UV_4_TO_7,	/* Signals connected to VPC[4..7] */
	 TMDL_HDMITX_VID422_UV_8_TO_11,	/* Signals connected to VPB[0..3] */
	 TMDL_HDMITX_VID422_Y_4_TO_7,	/* Signals connected to VPA[0..3] */
	 TMDL_HDMITX_VID422_Y_8_TO_11	/* Signals connected to VPA[4..7] */
	 }
};

/* Video port configuration for CCIR656 input                                 */
/* You can specify in this table how are connected video ports in case of     */
/* CCIR656 input signal. Each line of the array corresponds to a quartet of   */
/* pins of one video port (see comment on the left to identify them). Just    */
/* change the enum to specify which signal you connected to it. See file      */
/* tmdlHdmiTx_cfg.h to get the list of possible values                        */
const tmdlHdmiTxCfgVideoSignalCCIR656 videoPortMapping_CCIR656[MAX_UNITS][6] = {
	{
	 TMDL_HDMITX_VIDCCIR_NOT_CONNECTED,	/* Signals connected to VPB[0..3] */
	 TMDL_HDMITX_VIDCCIR_NOT_CONNECTED,	/* Signals connected to VPB[4..7] */
	 TMDL_HDMITX_VIDCCIR_NOT_CONNECTED,	/* Signals connected to VPC[0..3] */
	 TMDL_HDMITX_VIDCCIR_NOT_CONNECTED,	/* Signals connected to VPC[4..7] */
	 TMDL_HDMITX_VIDCCIR_4_TO_7,	/* Signals connected to VPA[4..7] */
	 TMDL_HDMITX_VIDCCIR_8_TO_11	/* Signals connected to VPA[0..3] */
	 }
};

/* The following function must be rewritten by the customer to fit its own    */
/* SW infrastructure. This function allows reading through I2C bus.           */
/* tmbslHdmiTxSysArgs_t definition is located into tmbslHdmiTx_type.h file.   */
tmErrorCode_t TxI2cReadFunction(tmbslHdmiTxSysArgs_t *pSysArgs)
{
	tmErrorCode_t errCode = TM_OK;

	if (pSysArgs->slaveAddr == 0x70) {
		errCode = i2cRead(reg_TDA998X, (tmbslHdmiSysArgs_t *) pSysArgs);
	} else if (pSysArgs->slaveAddr == 0x34) {
		errCode = i2cRead(reg_TDA9989_CEC, (tmbslHdmiSysArgs_t *) pSysArgs);
	} else {
		errCode = ~TM_OK;
	}

	return errCode;
}

/* The following function must be rewritten by the customer to fit its own    */
/* SW infrastructure. This function allows writing through I2C bus.           */
/* tmbslHdmiTxSysArgs_t definition is located into tmbslHdmiTx_type.h file.   */
tmErrorCode_t TxI2cWriteFunction(tmbslHdmiTxSysArgs_t *pSysArgs)
{
	tmErrorCode_t errCode = TM_OK;

	if (pSysArgs->slaveAddr == 0x70) {
		errCode = i2cWrite(reg_TDA998X, (tmbslHdmiSysArgs_t *) pSysArgs);

	} else if (pSysArgs->slaveAddr == 0x34) {
		errCode = i2cWrite(reg_TDA9989_CEC, (tmbslHdmiSysArgs_t *) pSysArgs);
	} else {
		errCode = ~TM_OK;
	}

	return errCode;
}

#endif

#ifdef TMFL_RGB_DDR_12BITS

/* Video port configuration for RGB 24 bits input received with only 12 bits  */
/* using the double data rate                                                 */
/*
  The main difference between RGB12 bits and CCIR 656 formats is that for the new format
  RGB888 theâ€œGreenâ€� data is separated on rising and on falling edge. This is in principle
  no problem only the result is that the colors RGB will be swabbed.
  After the Video Input Processing (VIP) module there will be a multiplexer structure
  implemented which can swab all colors and combinations.

  Extra information on request

  P:\Partages\BCT_TV_FE\Product_Development\Project folders\TDA19988    \
  14_Design\Video_pipe_Schematic_RGB888.pdf

  but ok, let's give it a try...

  In DDR, VIP input latches on failing and raising clock edge,
  so VIP internal input is doubled from 24 to 48 bits

  VIP input ------>[ T ]---[ T ]--------------------> VIP internal input
     /24   |         |       Â°      /24   |                /48
	   |         |       |            |
	   |  pclk-----------             |
	   |         |                    |
	   |         Â°                    |
	    ------>[ T ]------------------
				    /24

  But in the 24 VIP input, only 12 bits are used :

  -------------------------------------------------------------------
  |          |          |          |          |          |          |
  | Vpc[7:4] | Vpc[3:0] | Vpb[7:4] | Vpb[3:0] | Vpa[7:4] | Vpa[3:0] |
  |          |          | ........ |          | ........ | ........ |
  -------------------------------------------------------------------
			     ^                      ^         ^
			     |                      |         |
			     |                      |         |
  location of valid data -------------------------------------

  So we get first RGB 12 bits on bits 24 to 47 of VIP internal input
  and second RGB 12 bits on bits 0 to 23 of VIP internal input

  1)first edge (failing)   R[7:4]                 R[3:0]     G[7:4]
			     |                      |         |
			     |                      |         |
			     V                      V         V
  --------------------------------------------------------------------..
  |          |          |          |          |          |          |
  | 47...44  | 43...40  | 39...36  | 35...32  | 31...28  | 27...24  |
  |          |          |          |          |          |          |
  --------------------------------------------------------------------..

  2)2nd edge (raising)     G[3:0]                 B[7:3]     B[3:0]
			     |                      |         |
			     |                      |         |
			     V                      V         V
..-------------------------------------------------------------------
  |          |          |          |          |          |          |
  | 23...20  | 19...16  | 15...12  | 11...8   |  7...4   |  3...0   |
  |          |          |          |          |          |          |
..-------------------------------------------------------------------






  After port swaping, internal video bus goes back from 48 to 24 bits

  VIP internal ------------>[ swap ]-------[ T ]---------->
		     /48       |             |    /24
			       |             |
	    i2c_swap_a/f ------      pclk ---

  -------------------------------------------------------------------
  |          |          |          |          |          |          |
  | 23...20  | 19...16  | 15...12  | 11...8   |  7...4   |  3...0   |
  |          |          |          |          |          |          |
  -------------------------------------------------------------------
    R[7:4]     R[3:0]     G[7:4]     G[3:0]     B[7:3]     B[3:0]

   Here is the swapping code :

IF    i2c_swap_a = "000" THEN vp_alt_d2(11 downto 8) <= vp_alt_i_r(23 downto 20); vp_alt_d2(23 downto 20) <= vp_alt_i_r(47 downto 44);
ELSIF i2c_swap_a = "001" THEN vp_alt_d2(11 downto 8) <= vp_alt_i_r(19 downto 16); vp_alt_d2(23 downto 20) <= vp_alt_i_r(43 downto 40);
ELSIF i2c_swap_a = "010" THEN vp_alt_d2(11 downto 8) <= vp_alt_i_r(15 downto 12); vp_alt_d2(23 downto 20) <= vp_alt_i_r(39 downto 36);
ELSIF i2c_swap_a = "011" THEN vp_alt_d2(11 downto 8) <= vp_alt_i_r(11 downto 8 ); vp_alt_d2(23 downto 20) <= vp_alt_i_r(35 downto 32);
ELSIF i2c_swap_a = "100" THEN vp_alt_d2(11 downto 8) <= vp_alt_i_r( 7 downto 4 ); vp_alt_d2(23 downto 20) <= vp_alt_i_r(31 downto 28);
ELSE                          vp_alt_d2(11 downto 8) <= vp_alt_i_r( 3 downto 0 ); vp_alt_d2(23 downto 20) <= vp_alt_i_r(27 downto 24); END IF;

IF    i2c_swap_b = "000" THEN vp_alt_d2( 7 downto 4) <= vp_alt_i_r(23 downto 20); vp_alt_d2(19 downto 16) <= vp_alt_i_r(47 downto 44);
ELSIF i2c_swap_b = "001" THEN vp_alt_d2( 7 downto 4) <= vp_alt_i_r(19 downto 16); vp_alt_d2(19 downto 16) <= vp_alt_i_r(43 downto 40);
ELSIF i2c_swap_b = "010" THEN vp_alt_d2( 7 downto 4) <= vp_alt_i_r(15 downto 12); vp_alt_d2(19 downto 16) <= vp_alt_i_r(39 downto 36);
ELSIF i2c_swap_b = "011" THEN vp_alt_d2( 7 downto 4) <= vp_alt_i_r(11 downto 8 ); vp_alt_d2(19 downto 16) <= vp_alt_i_r(35 downto 32);
ELSIF i2c_swap_b = "100" THEN vp_alt_d2( 7 downto 4) <= vp_alt_i_r( 7 downto 4 ); vp_alt_d2(19 downto 16) <= vp_alt_i_r(31 downto 28);
ELSE                          vp_alt_d2( 7 downto 4) <= vp_alt_i_r( 3 downto 0 ); vp_alt_d2(19 downto 16) <= vp_alt_i_r(27 downto 24); END IF;

IF    i2c_swap_c = "000" THEN vp_alt_d2( 3 downto 0) <= vp_alt_i_r(23 downto 20); vp_alt_d2(15 downto 12) <= vp_alt_i_r(47 downto 44);
ELSIF i2c_swap_c = "001" THEN vp_alt_d2( 3 downto 0) <= vp_alt_i_r(19 downto 16); vp_alt_d2(15 downto 12) <= vp_alt_i_r(43 downto 40);
ELSIF i2c_swap_c = "010" THEN vp_alt_d2( 3 downto 0) <= vp_alt_i_r(15 downto 12); vp_alt_d2(15 downto 12) <= vp_alt_i_r(39 downto 36);
ELSIF i2c_swap_c = "011" THEN vp_alt_d2( 3 downto 0) <= vp_alt_i_r(11 downto 8 ); vp_alt_d2(15 downto 12) <= vp_alt_i_r(35 downto 32);
ELSIF i2c_swap_c = "100" THEN vp_alt_d2( 3 downto 0) <= vp_alt_i_r( 7 downto 4 ); vp_alt_d2(15 downto 12) <= vp_alt_i_r(31 downto 28);
ELSE                          vp_alt_d2( 3 downto 0) <= vp_alt_i_r( 3 downto 0 ); vp_alt_d2(15 downto 12) <= vp_alt_i_r(27 downto 24); END IF;

  in case of RGB DDR 12 bits, we get :
    . i2c_swap_a = "010"
    . i2c_swap_b = "011"
    . i2c_swap_c > "100"

 ;)

*/

const tmdlHdmiTxCfgVideoSignal_RGB_DDR_12bits VideoPortMapping_RGB_DDR_12bits[MAX_UNITS][6] = {
	{
	 TMDL_HDMITX_VID_B_0_3_G_4_7,	/* Signals connected to VPA[0..3] */
	 TMDL_HDMITX_VID_DDR_NOT_CONNECTED,	/* Signals connected to VPA[4..7] */
	 TMDL_HDMITX_VID_B_4_7_R_0_3,	/* Signals connected to VPB[0..3] */
	 TMDL_HDMITX_VID_G_0_3_R_4_7,	/* Signals connected to VPB[4..7] */
	 TMDL_HDMITX_VID_DDR_NOT_CONNECTED,	/* Signals connected to VPC[0..3] */
	 TMDL_HDMITX_VID_DDR_NOT_CONNECTED	/* Signals connected to VPC[4..7] */
	 }
};

/*

  Then VIP targeted order is not RGB but BGR
  so we use a new register for TDA19988 MUX_VP_VIP_OUT
  with VIP_OUTPUT_RGB_GBR as defined in cfg.h file

  -------------------------------------------------------------------
  |          |          |          |          |          |          |
  | 23...20  | 19...16  | 15...12  | 11...8   |  7...4   |  3...0   |
  |          |          |          |          |          |          |
  -------------------------------------------------------------------
    R[7:4]     R[3:0]     G[7:4]     G[3:0]     B[7:3]     B[3:0]
       -                    .                     .
	   -             .                     .
	       -       .                     .
		   - .                     .
		   .   -                 .
		 .         -           .
	       .               -     .
	     .                     -
	   .                     .     -
	 .                     .           -
       .                     .                 -

    G[7:4]     G[3:0]     B[7:4]     B[3:0]     R[7:3]     R[3:0]

*/

const UInt8 VideoPortMux_RGB_DDR_12bits[MAX_UNITS] = {
	VIP_MUX_R_R | VIP_MUX_G_G | VIP_MUX_B_B
};

const UInt8 VideoPortNoMux[MAX_UNITS] = {
	VIP_MUX_G_B | VIP_MUX_B_R | VIP_MUX_R_G
};

#endif				/* TMFL_RGB_DDR_12BITS */

/* Audio port configuration for SPDIF                                         */
/* Here you can specify the audio port routing configuration for SPDIF input. */
/* enableAudioPortSPDIF and groundAudioPortSPDIF should be filled with a      */
/* value build as follows : each bit represent an audio port, LSB is port 0.  */
/* enableAudioClockPortSPDIF and groundAudioClockPortSPDIF can be configured  */
/* with the corresponding enums (See file tmdlHdmiTx_cfg.h for more details). */
UInt8 enableAudioPortSPDIF[MAX_UNITS] = { 0x02 };
UInt8 enableAudioClockPortSPDIF[MAX_UNITS] = { DISABLE_AUDIO_CLOCK_PORT };
UInt8 groundAudioPortSPDIF[MAX_UNITS] = { 0xFD };
UInt8 groundAudioClockPortSPDIF[MAX_UNITS] = { ENABLE_AUDIO_CLOCK_PORT_PULLDOWN };

/* Audio port configuration for I2S                                           */
/* Here you can specify the audio port routing configuration for SPDIF input. */
/* enableAudioPortI2S and groundAudioPortI2S should be filled with a          */
/* value build as follows : each bit represent an audio port, LSB is port 0.  */
/* enableAudioClockPortI2S and groundAudioClockPortI2S can be configured      */
/* with the corresponding enums (See file tmdlHdmiTx_cfg.h for more details). */
UInt8 enableAudioPortI2S[MAX_UNITS] = { 0x03 };
UInt8 enableAudioPortI2S8C[MAX_UNITS] = { 0x1f };
UInt8 enableAudioClockPortI2S[MAX_UNITS] = { ENABLE_AUDIO_CLOCK_PORT };
UInt8 groundAudioPortI2S[MAX_UNITS] = { 0xfc };
UInt8 groundAudioPortI2S8C[MAX_UNITS] = { 0xe0 };
UInt8 groundAudioClockPortI2S[MAX_UNITS] = { DISABLE_AUDIO_CLOCK_PORT_PULLDOWN };

/* Audio port configuration for OBA                                           */
/* Here you can specify the audio port routing configuration for SPDIF input. */
/* enableAudioPortOBA and groundAudioPortOBA should be filled with a          */
/* value build as follows : each bit represent an audio port, LSB is port 0.  */
/* enableAudioClockPortOBA and groundAudioClockPortOBA can be configured      */
/* with the corresponding enums (See file tmdlHdmiTx_cfg.h for more details). */
UInt8 enableAudioPortOBA[MAX_UNITS] = { 0xFF };
UInt8 enableAudioClockPortOBA[MAX_UNITS] = { ENABLE_AUDIO_CLOCK_PORT };
UInt8 groundAudioPortOBA[MAX_UNITS] = { 0x00 };
UInt8 groundAudioClockPortOBA[MAX_UNITS] = { DISABLE_AUDIO_CLOCK_PORT_PULLDOWN };

/* Audio port configuration for DST                                           */
/* Here you can specify the audio port routing configuration for SPDIF input. */
/* enableAudioPortDST and groundAudioPortDST should be filled with a          */
/* value build as follows : each bit represent an audio port, LSB is port 0.  */
/* enableAudioClockPortDST and groundAudioClockPortDST can be configured      */
/* with the corresponding enums (See file tmdlHdmiTx_cfg.h for more details). */
UInt8 enableAudioPortDST[MAX_UNITS] = { 0xFF };
UInt8 enableAudioClockPortDST[MAX_UNITS] = { ENABLE_AUDIO_CLOCK_PORT };
UInt8 groundAudioPortDST[MAX_UNITS] = { 0x00 };
UInt8 groundAudioClockPortDST[MAX_UNITS] = { DISABLE_AUDIO_CLOCK_PORT_PULLDOWN };

/* Audio port configuration for HBR                                           */
/* Here you can specify the audio port routing configuration for SPDIF input. */
/* enableAudioPortHBR and groundAudioPortHBR should be filled with a          */
/* value build as follows : each bit represent an audio port, LSB is port 0.  */
/* enableAudioClockPortHBR and groundAudioClockPortHBR can be configured      */
/* with the corresponding enums (See file tmdlHdmiTx_cfg.h for more details). */
UInt8 enableAudioPortHBR[MAX_UNITS] = { 0x1f };
UInt8 enableAudioClockPortHBR[MAX_UNITS] = { ENABLE_AUDIO_CLOCK_PORT };
UInt8 groundAudioPortHBR[MAX_UNITS] = { 0xe0 };
UInt8 groundAudioClockPortHBR[MAX_UNITS] = { DISABLE_AUDIO_CLOCK_PORT_PULLDOWN };

/*****************************************************************************
******************************************************************************
*                THIS PART MUST NOT BE MODIFIED BY CUSTOMER                  *
******************************************************************************
*****************************************************************************/

/* DO NOT MODIFY, those tables are filled dynamically by                      */
/* dlHdmiTxGenerateCfgVideoPortTables API                                     */
UInt8 mirrorTableCCIR656[MAX_UNITS][6] = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
UInt8 swapTableCCIR656[MAX_UNITS][6] = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
UInt8 enableVideoPortCCIR656[MAX_UNITS][3] = { {0x00, 0x00, 0x00} };
UInt8 groundVideoPortCCIR656[MAX_UNITS][3] = { {0xFF, 0xFF, 0xFF} };
UInt8 mirrorTableYUV422[MAX_UNITS][6] = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
UInt8 swapTableYUV422[MAX_UNITS][6] = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
UInt8 enableVideoPortYUV422[MAX_UNITS][3] = { {0x00, 0x00, 0x00} };
UInt8 groundVideoPortYUV422[MAX_UNITS][3] = { {0xFF, 0xFF, 0xFF} };
UInt8 mirrorTableYUV444[MAX_UNITS][6] = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
UInt8 swapTableYUV444[MAX_UNITS][6] = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
UInt8 enableVideoPortYUV444[MAX_UNITS][3] = { {0x00, 0x00, 0x00} };
UInt8 groundVideoPortYUV444[MAX_UNITS][3] = { {0xFF, 0xFF, 0xFF} };
UInt8 mirrorTableRGB444[MAX_UNITS][6] = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
UInt8 swapTableRGB444[MAX_UNITS][6] = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
UInt8 enableVideoPortRGB444[MAX_UNITS][3] = { {0x00, 0x00, 0x00} };
UInt8 groundVideoPortRGB444[MAX_UNITS][3] = { {0xFF, 0xFF, 0xFF} };

#ifdef TMFL_RGB_DDR_12BITS
UInt8 mirrorTableRGB_DDR_12bits[MAX_UNITS][6] = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
UInt8 swapTableRGB_DDR_12bits[MAX_UNITS][6] = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
UInt8 enableVideoPortRGB_DDR_12bits[MAX_UNITS][3] = { {0x00, 0x00, 0x00} };
UInt8 groundVideoPortRGB_DDR_12bits[MAX_UNITS][3] = { {0xFF, 0xFF, 0xFF} };
UInt8 NoMux[MAX_UNITS] = { 0x00 };
UInt8 Mux_RGB_DDR_12bits[MAX_UNITS] = { 0x00 };
#endif

/* DO NOT MODIFY, this table is used for transmission of the configuration to */
/* the core driver                                                            */
tmdlHdmiTxDriverConfigTable_t driverConfigTableTx[MAX_UNITS] = {
	{
	 COMMAND_TASK_PRIORITY_0,
	 COMMAND_TASK_STACKSIZE_0,
	 COMMAND_TASK_QUEUESIZE_0,
	 HDCP_CHECK_TASK_PRIORITY_0,
	 HDCP_CHECK_TASK_STACKSIZE_0,
	 UNIT_I2C_ADDRESS_0,
	 TxI2cReadFunction,
	 TxI2cWriteFunction,
	 Null,			/* filled dynamically, do not modify */
	 &mirrorTableCCIR656[0][0],	/* filled dynamically, do not modify */
	 &swapTableCCIR656[0][0],	/* filled dynamically, do not modify */
	 &enableVideoPortCCIR656[0][0],	/* filled dynamically, do not modify */
	 &groundVideoPortCCIR656[0][0],	/* filled dynamically, do not modify */
	 &mirrorTableYUV422[0][0],	/* filled dynamically, do not modify */
	 &swapTableYUV422[0][0],	/* filled dynamically, do not modify */
	 &enableVideoPortYUV422[0][0],	/* filled dynamically, do not modify */
	 &groundVideoPortYUV422[0][0],	/* filled dynamically, do not modify */
	 &mirrorTableYUV444[0][0],	/* filled dynamically, do not modify */
	 &swapTableYUV444[0][0],	/* filled dynamically, do not modify */
	 &enableVideoPortYUV444[0][0],	/* filled dynamically, do not modify */
	 &groundVideoPortYUV444[0][0],	/* filled dynamically, do not modify */
	 &mirrorTableRGB444[0][0],	/* filled dynamically, do not modify */
	 &swapTableRGB444[0][0],	/* filled dynamically, do not modify */
	 &enableVideoPortRGB444[0][0],	/* filled dynamically, do not modify */
	 &groundVideoPortRGB444[0][0],	/* filled dynamically, do not modify */
#ifdef TMFL_RGB_DDR_12BITS
	 &mirrorTableRGB_DDR_12bits[0][0],
	 &swapTableRGB_DDR_12bits[0][0],
	 &NoMux[0],
	 &Mux_RGB_DDR_12bits[0],
	 &enableVideoPortRGB_DDR_12bits[0][0],
	 &groundVideoPortRGB_DDR_12bits[0][0],
#endif
	 &enableAudioPortSPDIF[0],
	 &groundAudioPortSPDIF[0],
	 &enableAudioClockPortSPDIF[0],
	 &groundAudioClockPortSPDIF[0],
	 &enableAudioPortI2S[0],
	 &groundAudioPortI2S[0],
	 &enableAudioPortI2S8C[0],
	 &groundAudioPortI2S8C[0],
	 &enableAudioClockPortI2S[0],
	 &groundAudioClockPortI2S[0],
	 &enableAudioPortOBA[0],
	 &groundAudioPortOBA[0],
	 &enableAudioClockPortOBA[0],
	 &groundAudioClockPortOBA[0],
	 &enableAudioPortDST[0],
	 &groundAudioPortDST[0],
	 &enableAudioClockPortDST[0],
	 &groundAudioClockPortDST[0],
	 &enableAudioPortHBR[0],
	 &groundAudioPortHBR[0],
	 &enableAudioClockPortHBR[0],
	 &groundAudioClockPortHBR[0],
	 KEY_SEED,
	 TMDL_HDMITX_PATTERN_BLUE,
	 1			/* DE signal is available */
	 }
};

#ifdef TMFL_CEC_AVAILABLE

tmdlHdmiCecCapabilities_t CeccapabilitiesList = { TMDL_HDMICEC_DEVICE_UNKNOWN, CEC_VERSION_1_3a };

/**
 * \brief Configuration Tables. This table can be modified by the customer
	    to choose its prefered configuration
 */

tmdlHdmiCecDriverConfigTable_t CecdriverConfigTable[MAX_UNITS] = {
	{
	 COMMAND_TASK_PRIORITY_0,
	 COMMAND_TASK_STACKSIZE_0,
	 COMMAND_TASK_QUEUESIZE_0,
	 CEC_UNIT_I2C_ADDRESS_0,
	 TxI2cReadFunction,
	 TxI2cWriteFunction,
	 &CeccapabilitiesList}
};


/******************************************************************************
******************************************************************************
*                THIS PART MUST NOT BE MODIFIED BY CUSTOMER                  *
******************************************************************************
*****************************************************************************/

/**
    \brief This function allows to the main driver to retrieve its
	   configuration parameters.

    \param pConfig Pointer to the config structure

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMICEC_BAD_UNIT_NUMBER: the unit number is wrong or
	      the receiver instance is not initialised
	    - TMDL_ERR_DLHDMICEC_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
tmErrorCode_t tmdlHdmiCecCfgGetConfig(tmUnitSelect_t unit, tmdlHdmiCecDriverConfigTable_t *pConfig) {
	/* check if unit number is in range */
	if ((unit < 0) || (unit >= MAX_UNITS))
		return TMDL_ERR_DLHDMICEC_BAD_UNIT_NUMBER;

	/* check if pointer is Null */
	if (pConfig == Null)
		return TMDL_ERR_DLHDMICEC_INCONSISTENT_PARAMS;

	*pConfig = CecdriverConfigTable[unit];

	return TM_OK;
}


/*============================================================================*/
/*                            END OF FILE                                     */
/*============================================================================*/

#endif
/******************************************************************************
    \brief  This function blocks the current task for the specified amount time.
	    This is a passive wait.

    \param  Duration    Duration of the task blocking in milliseconds.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_NO_RESOURCES: the resource is not available

******************************************************************************/
tmErrorCode_t tmdlHdmiTxIWWait(UInt16 duration) {
	msleep((unsigned long)duration);

	return (TM_OK);
}

/******************************************************************************
    \brief  This function creates a semaphore.

    \param  pHandle Pointer to the handle buffer.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_NO_RESOURCES: the resource is not available
	    - TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS: an input parameter is
	      inconsistent

******************************************************************************/
/*
tmErrorCode_t tmdlHdmiTxIWSemaphoreCreate
(
    tmdlHdmiTxIWSemHandle_t *pHandle
)
{
    struct semaphore * mutex;

    // check that input pointer is not NULL
    RETIF(pHandle == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

    mutex = (struct semaphore *)kmalloc(sizeof(struct semaphore),GFP_KERNEL);
    if (!mutex) {
       pr_err( "malloc failed in %s\n",__func__);
       return TMDL_ERR_DLHDMITX_NO_RESOURCES;
    }
	//pr_debug("%s, mutex=0x%08x\n", __func__, (unsigned int)mutex);

    mutex_init(mutex);
    *pHandle = (tmdlHdmiTxIWSemHandle_t)mutex;

    RETIF(pHandle == NULL, TMDL_ERR_DLHDMITX_NO_RESOURCES)

    return(TM_OK);
}
*/

DEFINE_SEMAPHORE(mutex0);
DEFINE_SEMAPHORE(mutex1);
DEFINE_SEMAPHORE(mutex2);

tmErrorCode_t tmdlHdmiTxIWSemaphoreCreate(tmdlHdmiTxIWSemHandle_t *pHandle)
{


	static int i;
	struct semaphore *mutex[3];
	/* check that input pointer is not NULL */
	RETIF(pHandle == Null, TMDL_ERR_DLHDMITX_INCONSISTENT_PARAMS)

	    mutex[0] = &mutex0;
	mutex[1] = &mutex1;
	mutex[2] = &mutex2;

	if (i > 2 || i < 0) {
		pr_debug("%s,create semphore error\n", __func__);
		return -1;
	}
	*pHandle = (tmdlHdmiTxIWSemHandle_t) mutex[i];
	i++;

	RETIF(pHandle == NULL, TMDL_ERR_DLHDMITX_NO_RESOURCES)

	    return (TM_OK);
}


/******************************************************************************
    \brief  This function destroys an existing semaphore.

    \param  Handle  Handle of the semaphore to be destroyed.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong

******************************************************************************/
tmErrorCode_t tmdlHdmiTxIWSemaphoreDestroy(tmdlHdmiTxIWSemHandle_t handle) {
	RETIF(handle == False, TMDL_ERR_DLHDMITX_BAD_HANDLE);
	/*
	   if (atomic_read((atomic_t*)&((struct semaphore *)handle)->count) < 1) {
	   pr_debug("release catched semaphore");
	   }

	   //kfree((void*)handle);
	 */
	return (TM_OK);
}

/******************************************************************************
    \brief  This function acquires the specified semaphore.

    \param  Handle  Handle of the semaphore to be acquired.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong

******************************************************************************/
tmErrorCode_t tmdlHdmiTxIWSemaphoreP(tmdlHdmiTxIWSemHandle_t handle) {
	/* pr_debug("%s, handle=0x%08x\n", __func__, (unsigned int)handle); */
	down((struct semaphore *)handle);

	return (TM_OK);
}

/******************************************************************************
    \brief  This function releases the specified semaphore.

    \param  Handle  Handle of the semaphore to be released.

    \return The call result:
	    - TM_OK: the call was successful
	    - TMDL_ERR_DLHDMITX_BAD_HANDLE: the handle number is wrong

******************************************************************************/
tmErrorCode_t tmdlHdmiTxIWSemaphoreV(tmdlHdmiTxIWSemHandle_t handle) {
	/* pr_debug("%s, handle=0x%08x\n", __func__, (unsigned int)handle); */
	up((struct semaphore *)handle);

	return (TM_OK);
}

/*============================================================================*/
/*                            END OF FILE                                     */
/*============================================================================*/
