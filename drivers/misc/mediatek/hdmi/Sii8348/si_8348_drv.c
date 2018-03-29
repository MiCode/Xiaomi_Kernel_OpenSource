/*

SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             

*/

/*
   @file si_8348_drv.c
*/

//#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>

#include "sii_hal.h"
#include "si_fw_macros.h"
#include "si_app_devcap.h"
#include "si_mhl_defs.h"  
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_8348_internal_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include "si_mdt_inputdev.h"
#endif
#include "si_8348_drv.h"
#include "mhl_linux_tx.h"
#include "platform.h"
#include "si_tpi_regs.h"
#include "si_8348_regs.h"

#include "si_timing_defs.h"

#ifdef CONFIG_MTK_MT6306_SUPPORT
#include<mach/dcl_sim_gpio.h>
#endif

#include <mach/irqs.h>
#ifdef CONFIG_MTK_LEGACY
#include "mach/eint.h"
#include <mach/gpio_const.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#endif

/*#include <mach/mt_gpio.h>*/
#include "hdmi_drv.h"
#include "mhl_supp.h"
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#include "smartbook.h"
#endif
//#define PRINT_ALL_INTR
//#define HDCP_ENABLE		// TODO: FD, TBI, to disble this later


/* external functions */
int si_mhl_tx_chip_initialize(struct drv_hw_context *hw_context);

/* Local functions */
static	int	int_4_isr(struct drv_hw_context *hw_context, uint8_t int_4_status);
#ifdef HDCP_ENABLE
static	int	int_hdcp2_isr(struct drv_hw_context *hw_context, uint8_t tpi_int_status);
#endif // HDCP_ENABLE

/*static int to_be_deleted(struct drv_hw_context *hw_context, uint8_t int_status);*/

static	int	int_3_isr(struct drv_hw_context *hw_context, uint8_t int_3_status);
#ifdef HDCP_ENABLE
static	int	hdcp_isr(struct drv_hw_context *hw_context, uint8_t tpi_int_status);
#endif // HDCP_ENABLE
static	int	int_1_isr(struct drv_hw_context *hw_context, uint8_t int_1_status);
static	int	g2wb_isr(struct drv_hw_context *hw_context, uint8_t intr_stat);
static	int	mhl_cbus_isr(struct drv_hw_context *hw_context, uint8_t cbus_int);
static	int	mhl_cbus_err_isr(struct drv_hw_context *hw_context, uint8_t cbus_err_int);

static	void	board_reset(struct drv_hw_context *hw_context, uint16_t hwResetPeriod, uint16_t hwResetDelay);
static	int	get_device_rev(struct drv_hw_context *hw_context);
static	void	enable_intr(struct drv_hw_context *hw_context, uint8_t intr_num, uint8_t intr_mask);
static	void	switch_to_d3(struct drv_hw_context *hw_context,bool do_interrupt_clear);
static	void	disconnect_mhl(struct drv_hw_context *hw_context,bool do_interrupt_clear);
#ifdef HDCP_ENABLE
static	void	start_hdcp(struct drv_hw_context *hw_context);
#endif // HDCP_ENABLE
static	void	stop_video(struct drv_hw_context *hw_context);
static	void	unmute_video(struct drv_hw_context *hw_context);
static	int	set_hdmi_params(struct mhl_dev_context *dev_context);
static	int	start_video(struct drv_hw_context *hw_context, void *edid_parser_context);
/*static	int	get_cbus_connection_status(struct drv_hw_context *hw_context);*/

#ifdef MHL2_ENHANCED_MODE_SUPPORT
static void	mhl2_em_query(struct drv_hw_context *hw_context);
static void	mhl2_em_request(struct drv_hw_context *hw_context, bool em);
#endif // MHL2_ENHANCED_MODE_SUPPORT

/* Local data */
#ifdef ENABLE_GEN2
#define GEN2_WRITE_BURST_LENGTH		MHL_SCRATCHPAD_SIZE
#define GEN2_BUFFER_LENGTH		(GEN2_WRITE_BURST_LENGTH + 1)
#ifdef MHL2_ENHANCED_MODE_SUPPORT
static uint8_t gen2_packet[GEN2_BUFFER_LENGTH];
#endif
#endif // ENABLE_GEN2

#ifdef MHL2_ENHANCED_MODE_SUPPORT
#define MHL2_ENHANCED_MODE_RESERVED_VIC 93

enum mhl2_enhanced_mode_sm {
	MHL2_EM_SM_INIT = 0,
	MHL2_EM_SM_QUERY_CAP,
	MHL2_EM_SM_CAP_QUERIED,
	MHL2_EM_SM_REQ_MODE_EM,
	MHL2_EM_SM_MODE_EM,
	MHL2_EM_SM_REQ_MODE_NORMAL,
	MHL2_EM_SM_MODE_NORMAL,
	MHL2_EM_SM_SEND_PPS,
	MHL2_EM_SM_PPS_SENT,
};

enum mhl2_enhanced_mode_command {
	MHL2_EM_CMD_QUERY_CAP = 0x00,
	MHL2_EM_CMD_REQ_MODE = 0x02,
	MHL2_EM_CMD_SEND_PPS_FULL = 0x04,
	MHL2_EM_CMD_SEND_PPS_PART = 0x06,
	MHL2_EM_RPL_QUERY_CAP = 0x01,
	MHL2_EM_RPL_REQ_MODE = 0x03,
	MHL2_EM_RPL_SEND_PPS_FULL = 0x05,
	MHL2_EM_RPL_SEND_PPS_PART = 0x07
};

#define MHL2_EM_PACKET_LEN		GEN2_WRITE_BURST_LENGTH
#define MHL2_EM_PACKET_FRAME_LEN	0x05
#define MHL2_EM_PACKET_PAYLOAD_LEN_MAX	(MHL2_EM_PACKET_LEN - MHL2_EM_PACKET_FRAME_LEN)	// max payload length in a packet is 11

#define MHL2_EM_PACKET_ADOPTER_ID_HIGH	0x01	// 0x0142 = 0322, SIMG ID
#define MHL2_EM_PACKET_ADOPTER_ID_LOW	0x42

#define MHL2_EM_MESSAGE_FRAME_LEN	0x03

#define MHL2_EM_MODE_NORMAL	0x00
#define MHL2_EM_MODE_ONE	0x01
#define MHL2_EM_MODE_TWO	0x02


#define MHL2_EM_CMD_QUERY_CAP_MSG_LEN	(MHL2_EM_MESSAGE_FRAME_LEN + 0x00)
#define MHL2_EM_CMD_REQ_MODE_MSG_LEN	(MHL2_EM_MESSAGE_FRAME_LEN + 0x01)
#define MHL2_EM_CMD_REQ_MODE_NORMAL	MHL2_EM_MODE_NORMAL
#define MHL2_EM_CMD_REQ_MODE_ONE	MHL2_EM_MODE_ONE
#define MHL2_EM_CMD_REQ_MODE_TWO	MHL2_EM_MODE_TWO


#define MHL2_EM_CAP_NONE	0x00
#define MHL2_EM_CAP_4K24	0x01
#define MHL2_EM_CAP_4K30	0x02
#define MHL2_EM_CAP_4K60	0x04
#define MHL2_EM_CAP_5K		0x08

#define MHL2_EM_RPL_NACK	0x00
#define MHL2_EM_RPL_SUCCESS	0x01
#define MHL2_EM_RPL_FAILURE	0x02
#define MHL2_EM_RPL_FPPS_UNKNOWN	0x03
#define MHL2_EM_RPL_PPPS_UNKNOWN_MODE	0x03
#define MHL2_EM_RPL_PPPS_UNKNOWN_PPPS	0x04

#define MHL2_EM_VIC_4K24	93
#define MHL2_EM_VIC_4K30	95

#endif // MHL2_ENHANCED_MODE_SUPPORT

// Video Mode Constants
//====================================================
#define VMD_ASPECT_RATIO_4x3			0x01
#define VMD_ASPECT_RATIO_16x9			0x02

//====================================================

typedef struct
{
    uint8_t inputColorSpace;
    uint8_t outputColorSpace;
    uint8_t inputVideoCode;
    uint8_t inputcolorimetryAspectRatio;
    uint8_t outputcolorimetryAspectRatio;
    uint8_t input_AR;   
    uint8_t output_AR;  
} video_data_t;
video_data_t video_data;

//====================================================
// Audio mode define
static	int Audio_mode_fs=AUDIO_44K_2CH;
uint8_t current_audio_info_frame[14] = {0x84, 0x01, 0x0A, 0x00, 0x01, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void siHdmiTx_VideoSel (int vmode);
void siHdmiTx_AudioSel (int AduioMode);

/* Local data */
#ifdef HDCP_ENABLE
#define	HDCP_RPTR_CTS_DELAY_MS	2875
#define	HDCP_ERROR_THRESHOLD	5
static	int		hdcp_bksv_err_count = 0;
static	int		hdcp_reneg_err_count = 0;
static	int		hdcp_link_err_count = 0;
static	int		hdcp_suspend_err_count = 0;
#endif // HDCP_ENABLE

#define	DDC_ABORT_THRESHOLD		10
static	int		ddc_abort_count = 0;

#define	MSC_ABORT_THRESHOLD		10	// TODO: FD, TBI, not actually used now
static	int		msc_abort_count = 0;

struct	intr_tbl	{
	uint8_t	mask;
	uint8_t	mask_page;
	uint8_t	mask_offset;
	uint8_t	stat_page;
	uint8_t	stat_offset;
	int	(*isr)(struct drv_hw_context *, uint8_t int_5_status);
	char	name[5];
};

#define REG_TIMING_INTR_MASK	0x66, 0x00	// TODO: FD, TBD
#define REG_TIMING_INTR		0x66, 0x01	// TODO: FD, TBD

struct	intr_tbl	g_intr_tbl[] = {
		 {0, REG_INTR4_MASK, REG_INTR4, int_4_isr, "DISC"}
		,{0, REG_CBUS_MDT_INT_0_MASK, REG_CBUS_MDT_INT_0, g2wb_isr, "G2WB"}
		,{0, REG_CBUS_INT_0_MASK, REG_CBUS_INT_0, mhl_cbus_isr, "MSC "}
		,{0, REG_CBUS_INT_1_MASK, REG_CBUS_INT_1, mhl_cbus_err_isr, "MERR"}
		//,{0, REG_TIMING_INTR_MASK, REG_TIMING_INTR, to_be_deleted, "INFR"}	// TODO: FD, TBD
#ifdef HDCP_ENABLE
		,{0, REG_TPI_INTR_ST0_ENABLE, REG_TPI_INTR_ST0, hdcp_isr, "HDCP"}
#endif // HDCP_ENABLE
		,{0, REG_INTR3_MASK, REG_INTR3, int_3_isr, "EDID"}
#ifdef HDCP_ENABLE
		,{0, REG_TPI_INTR_ST1_ENABLE, REG_TPI_INTR_ST1, int_hdcp2_isr, "HDCP2"}
#endif // HDCP_ENABLE
		,{0, REG_INTR1_MASK, REG_INTR1, int_1_isr, "INTR1"}
};

typedef enum
{
	 INTR_DISC	= 0
	,INTR_G2WB	= 1
	,INTR_MSC	= 2
	,INTR_MERR	= 3
	///,INTR_INFR	= 4	// TODO: FD, TBD
#ifdef HDCP_ENABLE
	,INTR_HDCP	= 4
	,INTR_EDID	= 5
//	,INTR_CKDT	= 7
	,INTR_HDCP2	= 6
	,INTR_INTR1	= 7
	,MAX_INTR	= 8
#else
	,INTR_EDID	= 4
	,INTR_INTR1	= 5
	,MAX_INTR	= 6
#endif // HDCP_ENABLE
}intr_nums_t;

#define SILICON_IMAGE_ADOPTER_ID		322

//#define TX_HW_RESET_PERIOD			10	/* 10 ms. */
//#define TX_HW_RESET_DELAY			100
#define TX_HW_RESET_PERIOD			5	/* system: 0.5s is enough */ // TODO: FD, TBI, to reduce this delay smaller
#define TX_HW_RESET_DELAY			5	/* system: 0.5s is enough */ // TODO: FD, TBI, to reduce this delay smaller
#define TX_EDID_POLL_MAX			256

static uint8_t colorSpaceTranslateInfoFrameToHw[] = {
		BIT_TPI_INPUT_FORMAT_RGB,
		BIT_TPI_INPUT_FORMAT_YCbCr422,
		BIT_TPI_INPUT_FORMAT_YCbCr444,
		BIT_TPI_INPUT_FORMAT_INTERNAL_RGB	/* reserved for future */
};

#ifdef ENABLE_GEN2 //(
static	void	enable_gen2_write_burst(struct drv_hw_context *hw_context)
{
	/*  enable Gen2 Write Burst interrupt, MSC and EDID interrupts. */


	{
		MHL_TX_DBG_INFO(hw_context, "called.\n");
		mhl_tx_write_reg(hw_context, REG_CBUS_MDT_RCV_TIMEOUT, 100);	/* 2 second timeout */
		mhl_tx_write_reg(hw_context, REG_CBUS_MDT_XMIT_TIMEOUT, 100);	/* 2 second timeout */
		mhl_tx_write_reg(hw_context, REG_CBUS_MDT_RCV_CONTROL, BIT_CBUS_MDT_RCV_CONTROL_RCV_EN_ENABLE);
		mhl_tx_write_reg(hw_context, REG_CBUS_MDT_XMIT_CONTROL, BIT_CBUS_MDT_XMIT_CONTROL_XMIT_EN_ENABLE);
		enable_intr(hw_context, INTR_G2WB, BIT_MDT_RXFIFO_DATA_RDY);

		hw_context->gen2_write_burst = true;
	}
}

static void disable_gen2_write_burst(struct drv_hw_context *hw_context)
{
	MHL_TX_DBG_INFO(hw_context, "called.\n");

	/*  disable Gen2 Write Burst engine to perform it using legacy WRITE_BURST */
	mhl_tx_write_reg(hw_context, REG_CBUS_MDT_RCV_CONTROL, BIT_CBUS_MDT_RCV_CONTROL_RCV_EN_DISABLE);
	mhl_tx_write_reg(hw_context, REG_CBUS_MDT_XMIT_CONTROL, BIT_CBUS_MDT_XMIT_CONTROL_XMIT_EN_DISABLE);
	enable_intr(hw_context, INTR_G2WB, 0);
	hw_context->gen2_write_burst = false;
}
#endif //)

static void si_mhl_tx_drv_reset_ddc_fifo(struct drv_hw_context *hw_context)
{
	uint8_t	ddc_status;

	ddc_status = mhl_tx_read_reg(hw_context, REG_DDC_STATUS);

	mhl_tx_modify_reg(hw_context, REG_TPI_SEL,
					  BIT_TPI_SEL_SW_TPI_EN_MASK,
					  BIT_TPI_SEL_SW_TPI_EN_NON_HW_TPI);
	if (BIT_DDC_STATUS_DDC_NO_ACK & ddc_status) {
		MHL_TX_DBG_ERR(hw_context, "Clearing DDC ack status\n");
		mhl_tx_write_reg(hw_context, REG_DDC_STATUS,
						 ddc_status & ~BIT_DDC_STATUS_DDC_NO_ACK);
	}
	mhl_tx_modify_reg(hw_context, REG_DDC_CMD,
					  BIT_DDC_CMD_COMMAND_MASK,
					  BIT_DDC_CMD_COMMAND_CLEAR_FIFO);

	mhl_tx_modify_reg(hw_context, REG_TPI_SEL,
					  BIT_TPI_SEL_SW_TPI_EN_MASK,
					  BIT_TPI_SEL_SW_TPI_EN_HW_TPI);
}

// Read specific batch of data in specific EDID block
static void si_mhl_tx_drv_issue_edid_block_batch_read(struct drv_hw_context *hw_context, uint8_t block_number, uint8_t batch_number)
{
	uint8_t	ddc_status;
	uint8_t	offset = 0;

	MHL_TX_DBG_INFO(hw_context, "called.\n");

	ddc_status = mhl_tx_read_reg(hw_context, REG_DDC_STATUS);

	// Enter NON-HW TPI mode
	mhl_tx_modify_reg(hw_context, REG_TPI_SEL,
					  BIT_TPI_SEL_SW_TPI_EN_MASK,
					  BIT_TPI_SEL_SW_TPI_EN_NON_HW_TPI);

	if (BIT_DDC_STATUS_DDC_NO_ACK & ddc_status) {
		MHL_TX_DBG_ERR(hw_context, "Clearing DDC ack status\n");
		mhl_tx_write_reg(hw_context, REG_DDC_STATUS,
						 ddc_status & ~BIT_DDC_STATUS_DDC_NO_ACK);
	}

	// Clear DDC FIFO
	mhl_tx_modify_reg(hw_context, REG_DDC_CMD,
					  BIT_DDC_CMD_COMMAND_MASK,
					  BIT_DDC_CMD_COMMAND_CLEAR_FIFO);

	// Set Segment
	mhl_tx_write_reg(hw_context, REG_DDC_SEGM, block_number / 2);

	// Set EDID slave address
	mhl_tx_write_reg(hw_context, REG_DDC_ADDR, 0xA0);

	// Set EDID offset address
	offset = (0 == (block_number % 2)) ? 0x00 : 0x80;
	offset += batch_number * 16;
	mhl_tx_write_reg(hw_context, REG_DDC_OFFSET, offset);

	// Set count of data to read, 16 bytes per batch
	mhl_tx_write_reg(hw_context, REG_DDC_DIN_CNT1, 0x10);
	mhl_tx_write_reg(hw_context, REG_DDC_DIN_CNT2, 0x00);

	// Trigger Enhanced DDC read
	mhl_tx_write_reg(hw_context, REG_DDC_CMD, BIT_DDC_CMD_COMMAND_ENHANCED_READ_NO_ACK);

	// Cannot enter HW TPI mode during EDID read
}

#ifdef MHL2_ENHANCED_MODE_SUPPORT
static void mhl2_em_init(struct drv_hw_context *hw_context)
{
		hw_context->mhl2_em_enabled = false;
		//hw_context->mhl2_em_input = false;
		hw_context->mhl2_em_sm = MHL2_EM_SM_INIT;
		hw_context->mhl2_em_request_mode = 0;
		hw_context->mhl2_em_current_mode = 0;
		hw_context->mhl2_em_capability = 0;
}

void si_mhl_tx_em_init(struct mhl_dev_context *dev_context)
{
	struct drv_hw_context *hw_context = (struct drv_hw_context *)&dev_context->drv_context;
	mhl2_em_init(hw_context);
}
#endif // MHL2_ENHANCED_MODE_SUPPORT

bool si_mhl_tx_drv_issue_edid_read_request(struct drv_hw_context *hw_context, uint8_t block_number, uint8_t batch_number)
{
	uint8_t reg_val;
			
	reg_val = mhl_tx_read_reg(hw_context, REG_CBUS_STATUS);
	if ( BIT_CBUS_HPD & reg_val ) {
		MHL_TX_EDID_READ(hw_context,
				"\n\tRequesting EDID block:%d\n"	\
				"\tRequesting EDID block batch:%d\n"	\
				"\tcurrentEdidRequestBlock:%d\n"	\
				"\tcurrentEdidRequestBlockBatch:%d\n"	\
				"\tedidFifoBlockNumber:%d\n",
				block_number,
				batch_number,
				hw_context->current_edid_request_block,
				hw_context->current_edid_request_block_batch,
				hw_context->edid_fifo_block_number);
        
#ifdef MHL2_ENHANCED_MODE_SUPPORT
		mhl2_em_init(hw_context);
#endif // MHL2_ENHANCED_MODE_SUPPORT

    	si_mhl_tx_drv_reset_ddc_fifo(hw_context);
		si_mhl_tx_drv_issue_edid_block_batch_read(hw_context, block_number, batch_number);

		return true;
	} else {
		MHL_TX_DBG_INFO(hw_context,
				"\n\tNo HPD for EDID block request:%d\n"	\
				"\tcurrentEdidRequestBlock:%d\n"			\
				"\tRequesting EDID block batch:%d\n"	\
				"\tcurrentEdidRequestBlockBatch:%d\n"			\
				"\tedidFifoBlockNumber:%d\n",
				block_number,
				batch_number,
				hw_context->current_edid_request_block,
				hw_context->current_edid_request_block_batch,
				hw_context->edid_fifo_block_number);
		
		return false;
	}
}

/*
 * si_mhl_tx_drv_send_cbus_command
 *
 * Write the specified Sideband Channel command to the CBUS.
 * such as READ_DEVCAP, SET_INT, WRITE_STAT, etc.
 * Command can be a MSC_MSG command (RCP/RAP/RCPK/RCPE/RAPK), or another command
 * Parameters:
 *              req     - Pointer to a cbus_req_t structure containing the
 *                        command to write
 * Returns:     true    - successful write
 *              false   - write failed
 */
bool si_mhl_tx_drv_send_cbus_command(struct drv_hw_context *hw_context,
									 struct cbus_req *req)
{
	bool	success = true;
	uint8_t	block_write_buffer [3];	// used for efficient block writes

#ifdef ENABLE_GEN2 //(
	/* Disable h/w automation of WRITE_BURST until this command completes */
	disable_gen2_write_burst(hw_context);
#endif //)
    switch (req->command ) {
	case MHL_SET_INT:
		MHL_TX_DBG_INFO(hw_context, "SET_INT reg: 0x%02x data: 0x%02x\n",
						req->reg, req->reg_data);
		mhl_tx_write_reg(hw_context, REG_CBUS_MSC_CMD_OR_OFFSET, req->reg);
		mhl_tx_write_reg(hw_context, REG_CBUS_MSC_1ST_TRANSMIT_DATA, req->reg_data);
		mhl_tx_write_reg(hw_context, REG_CBUS_MSC_COMMAND_START,
						 BIT_CBUS_MSC_WRITE_STAT_OR_SET_INT);
		break;

    case MHL_WRITE_STAT:
		MHL_TX_DBG_INFO(hw_context,
				"WRITE_STAT (0x%02x, 0x%02x)\n",
				req->reg,
				req->reg_data);
		mhl_tx_write_reg(hw_context, REG_CBUS_MSC_CMD_OR_OFFSET, req->reg);
		mhl_tx_write_reg(hw_context, REG_CBUS_MSC_1ST_TRANSMIT_DATA, req->reg_data);
		mhl_tx_write_reg(hw_context, REG_CBUS_MSC_COMMAND_START,
						 BIT_CBUS_MSC_WRITE_STAT_OR_SET_INT);
		break;

	case MHL_READ_DEVCAP:
		MHL_TX_DBG_INFO(hw_context,
				"READ_DEVCAP (0x%02x, 0x%02x)\n",
				req->reg,
				req->reg_data);

		/* don't call si_mhl_tx_drv_reset_ddc_fifo here */	// TODO: FD, TBC

		// TODO: FD, TBC
		mhl_tx_write_reg(hw_context, REG_CBUS_MSC_CMD_OR_OFFSET, req->reg);
		mhl_tx_write_reg(hw_context, REG_CBUS_MSC_1ST_TRANSMIT_DATA, req->reg_data);
		mhl_tx_write_reg(hw_context, REG_CBUS_MSC_COMMAND_START,
						 BIT_CBUS_MSC_READ_DEVCAP);
		break;

	case MHL_READ_EDID_BLOCK:
		hw_context->current_edid_request_block = 0;
		hw_context->current_edid_request_block_batch = 0;
		hw_context->edid_fifo_block_number = 0;
		MHL_TX_DBG_INFO(hw_context, "before si_mhl_tx_drv_issue_edid_read_request: to read block 0\n");
		success = si_mhl_tx_drv_issue_edid_read_request(hw_context,
				hw_context->current_edid_request_block,
				hw_context->current_edid_request_block_batch);
        break;

	case MHL_GET_STATE:			/* 0x62 - */
	case MHL_GET_VENDOR_ID:		/* 0x63 - for vendor id */
	case MHL_SET_HPD:			/* 0x64	- Set Hot Plug Detect */
	case MHL_CLR_HPD:			/* 0x65	- Clear Hot Plug Detect */
	case MHL_GET_SC1_ERRORCODE:	/* 0x69	- Get channel 1 command error code */
	case MHL_GET_DDC_ERRORCODE:	/* 0x6A	- Get DDC channel command error code */
	case MHL_GET_MSC_ERRORCODE:	/* 0x6B	- Get MSC command error code */
	case MHL_GET_SC3_ERRORCODE:	/* 0x6D	- Get channel 3 command error code */
		MHL_TX_DBG_INFO(hw_context, "Sending MSC command %02x, %02x, %02x\n",
						req->command, req->reg, req->reg_data);
		mhl_tx_write_reg(hw_context,REG_CBUS_MSC_CMD_OR_OFFSET, req->command);
		mhl_tx_write_reg(hw_context,REG_CBUS_MSC_1ST_TRANSMIT_DATA, req->reg_data);
		mhl_tx_write_reg(hw_context,REG_CBUS_MSC_COMMAND_START,
						 BIT_CBUS_MSC_PEER_CMD);
		break;

	case MHL_MSC_MSG:
		MHL_TX_DBG_INFO(hw_context,
						"MHL_MSC_MSG sub cmd: 0x%02x data: 0x%02x\n",
						req->msg_data[0], req->msg_data[1]);
		block_write_buffer[0] = req->command;
		block_write_buffer[1] = req->msg_data[0];
		block_write_buffer[2] = req->msg_data[1];

		/*
		mhl_tx_write_reg(hw_context,REG_CBUS_MSC_CMD_OR_OFFSET, req->command);
		mhl_tx_write_reg(hw_context,REG_CBUS_MSC_1ST_TRANSMIT_DATA, req->msg_data[0]);
		mhl_tx_write_reg(hw_context,REG_CBUS_MSC_2ND_TRANSMIT_DATA, req->msg_data[1]);
		*/
		mhl_tx_write_reg_block(hw_context,REG_CBUS_MSC_CMD_OR_OFFSET, 3, block_write_buffer);
		mhl_tx_write_reg(hw_context,REG_CBUS_MSC_COMMAND_START,
						 BIT_CBUS_MSC_MSG);
		break;

	case MHL_WRITE_BURST:
		MHL_TX_DBG_INFO(hw_context, "MHL_WRITE_BURST offset: 0x%02x "\
						"length: 0x%02x\n",
						req->offset, req->length);

		mhl_tx_write_reg(hw_context, REG_CBUS_MSC_CMD_OR_OFFSET,
						 req->offset + REG_CBUS_MHL_SCRPAD_BASE);

		mhl_tx_write_reg(hw_context, REG_CBUS_MSC_WRITE_BURST_DATA_LEN,
						 req->length -1);

		/* Now copy all bytes from array to local scratchpad */
		mhl_tx_write_reg_block(hw_context, REG_CBUS_WB_XMIT_DATA_0,
							   req->length, req->msg_data);
		mhl_tx_write_reg(hw_context,REG_CBUS_MSC_COMMAND_START,
						 BIT_CBUS_MSC_WRITE_BURST);
		break;

	default:
		MHL_TX_DBG_ERR(hw_context, "Unsupported command 0x%02x detected!\n",
					   req->command);
		success = false;
		break;
    }

    return (success);
}

uint16_t si_mhl_tx_drv_get_incoming_horizontal_total(struct drv_hw_context *hw_context)
{
	uint16_t ret_val;

	ret_val = (((uint16_t)mhl_tx_read_reg(hw_context, REG_HRESH)) <<8) |
				(uint16_t)mhl_tx_read_reg(hw_context, REG_HRESL);
	return ret_val;
}

uint16_t si_mhl_tx_drv_get_incoming_vertical_total(struct drv_hw_context *hw_context)
{
	uint16_t ret_val;

	ret_val = (((uint16_t)mhl_tx_read_reg(hw_context, REG_VRESH)) <<8) |
				(uint16_t)mhl_tx_read_reg(hw_context, REG_VRESL);
    return ret_val;
}

int si_mhl_tx_drv_get_edid_fifo_next_block(struct drv_hw_context *hw_context, uint8_t *edid_buf)
{
	int ret_val;
	uint8_t offset;

    MHL_TX_DBG_INFO(hw_context, "called.\n");

	offset = EDID_BLOCK_SIZE * hw_context->edid_fifo_block_number;

	MHL_TX_DBG_INFO(hw_context, "%x %x\n",(unsigned long)hw_context,(unsigned long)edid_buf);
	hw_context->edid_fifo_block_number++;

	memcpy(edid_buf, hw_context->current_edid_block_data + offset , EDID_BLOCK_SIZE);
/*
	// Need to use 'pr_debug' to keep the formatting
	if ( DBG_MSG_LEVEL_EDID_READ > debug_msgs )
	{
		// do nothing
	}
	else*/
	{
		MHL_TX_EDID_READ(hw_context, "current edid data: %d.\n", hw_context->edid_fifo_block_number - 1);
#if 0		
		for ( i = 0; i < 8; i ++)
		{
			pr_debug("ROW %d:\t\t", i);
			for (j=0; j<16; j++)
			{
				pr_debug("%02X:%02X\t", j, edid_buf[i * 16 + j]);		// buffer data
				//pr_debug("%02X:%02X\t", j, hw_context->current_edid_block_data[offset + i * 16 + j]);	// block data
			}
			pr_debug("\n");
		}
		pr_debug("\n");
	
	DUMP_EDID_BLOCK(0,edid_buf, EDID_BLOCK_SIZE);
#endif
    }
	
	ret_val = mhl_tx_read_reg(hw_context,REG_CBUS_STATUS);
	if (ret_val < 0)
	{
		MHL_TX_DBG_ERR(hw_context, "%d", ret_val);
		return ne_NO_HPD;
	}
	else if (BIT_CBUS_HPD & ret_val)
	{
		MHL_TX_DBG_INFO(hw_context, "Done reading EDID from FIFO using Manual DDC read, ret_val:0x%02x\n",ret_val);
		return 0;
	}
	else
	{
		MHL_TX_DBG_INFO(hw_context, "No HPD ret_val:0x%02x\n",ret_val);
		return ne_NO_HPD;
	}
}

int si_mhl_tx_drv_get_scratch_pad(struct drv_hw_context *hw_context,
		uint8_t start_reg, uint8_t *data,
		uint8_t length)
{
	if ((start_reg + length) > (int)MHL_SCRATCHPAD_SIZE)
		return -1;

	memcpy(data, &hw_context->write_burst_data[start_reg], length);

	return 0;
}

bool packed_pixel_available(struct mhl_dev_context *dev_context)
{
	if ((MHL_DEV_VID_LINK_SUPP_PPIXEL & DEVCAP_VAL_VID_LINK_MODE) &&
		(dev_context->dev_cap_cache.mdc.vid_link_mode &
		 MHL_DEV_VID_LINK_SUPP_PPIXEL)) {

		return true;
	}
	return false;
}

int dongle_dsc_dec_available(struct mhl_dev_context *dev_context)
{
	/// bit 0: 4k@24  bit 1: 4k@30
#ifdef MHL2_ENHANCED_MODE_SUPPORT	
	struct drv_hw_context *hw_context = (struct drv_hw_context *)&dev_context->drv_context;
	return ((int)hw_context->mhl2_em_capability);
#else
	return 0;
#endif
}

#define SIZE_AVI_INFOFRAME				14
static uint8_t calculate_avi_info_frame_checksum(hw_avi_payload_t *payload)
{
	uint8_t checksum;

	checksum = 0x82 + 0x02 + 0x0D;	/* these are set by the hardware */
	return calculate_generic_checksum(payload->ifData, checksum, SIZE_AVI_INFOFRAME);
}
#if 0
#define SIZE_AUDIO_IF					14
static uint8_t calculate_audio_if_checksum(uint8_t *audio_if)
{
	uint8_t checksum = 0;

	return calculate_generic_checksum(audio_if, checksum, SIZE_AUDIO_IF);
}

#define SIZE_AVIF					9
static uint8_t calculate_avif_checksum(uint8_t *avif)
{
	uint8_t checksum = 0;

	return calculate_generic_checksum(avif, checksum, SIZE_AVIF);
}
#define SIZE_VSIF					8
static uint8_t calculate_vsif_checksum(uint8_t *vsif)
{
	uint8_t checksum = 0;

	return calculate_generic_checksum(vsif, checksum, SIZE_VSIF);
}

static int is_valid_avi_info_frame(struct mhl_dev_context *dev_context, avi_info_frame_t *avif)
{
	uint8_t	checksum;

	checksum = calculate_generic_checksum((uint8_t *)avif, 0, sizeof(*avif));
	if (0 != checksum) {
		MHL_TX_DBG_ERR(dev_context, "AVI info frame checksum is: 0x%02x should be 0\n", checksum);
		return 0;

	} else if (0x82 != avif->header.type_code) {
		MHL_TX_DBG_ERR(dev_context, "Invalid AVI type code: 0x%02x\n", avif->header.type_code);
		return 0;

	} else if (0x02 != avif->header.version_number) {
		MHL_TX_DBG_ERR(dev_context, "Invalid AVI version: 0x%02x\n", avif->header.version_number);
		return 0;

	} else if (0x0D != avif->header.length) {
		return 0;

	} else {
		return 1;
	}
}

static int is_valid_vsif(struct mhl_dev_context *dev_context, vendor_specific_info_frame_t *vsif)
{
	uint8_t	checksum;

	/*
		Calculate the checksum assuming that the payload includes the checksum
	*/
	checksum = calculate_generic_checksum((uint8_t *)vsif, 0,
			sizeof(vsif->header) + vsif->header.length );
	if (0 != checksum) {
		MHL_TX_DBG_WARN(dev_context, "VSIF info frame checksum is: 0x%02x should be 0\n", checksum);
		/*
			Try again, assuming that the header includes the checksum.
		*/
		checksum = calculate_generic_checksum((uint8_t *)vsif, 0,
			sizeof(vsif->header) + vsif->header.length
			+ sizeof(vsif->payLoad.checksum));
		if (0 != checksum){
			MHL_TX_DBG_ERR(dev_context, "VSIF info frame checksum "
					"(adjusted for checksum itself) is: 0x%02x "
						   "should be 0\n", checksum);
			return 0;
		}
	} 
	if (0x81 != vsif->header.type_code) {
		MHL_TX_DBG_ERR(dev_context, "Invalid VSIF type code: 0x%02x\n",
					   vsif->header.type_code);
		return 0;

	} else if (0x01 != vsif->header.version_number) {
		MHL_TX_DBG_ERR(dev_context, "Invalid VSIF version: 0x%02x\n",
					   vsif->header.version_number);
		return 0;

	} else {
		return 1;
	}
}
#endif

static	void	print_vic_modes(struct drv_hw_context *hw_context,uint8_t vic)
{
	int	i;
	struct	vic_name {
		uint8_t		vic;
		char		name[10];
	} vic_name_table[] = {
				  {2, "480P"}
				 ,{4, "720P60"}
				 ,{5, "1080i60"}
				 ,{6, "480i"}
				 ,{16,"1080P60"}
				 ,{17,"576P50"}
				 ,{19,"720P50"}
				 ,{20,"1080i50"}
				 ,{21,"576i50"}
				 ,{31,"1080P50"}
				 ,{32,"1080P24"}
				 ,{33,"1080P25"}
				 ,{34,"1080P30"}
				 ,{0,""} /* to handle the case where the VIC is not found in the table */
	};
#define	NUM_VIC_NAMES	(sizeof(vic_name_table)/sizeof(vic_name_table[0]) )
	/* stop before the terminator */
	for(i = 0; i < (NUM_VIC_NAMES - 1); i++) {
		if(vic == vic_name_table[i].vic) {
			break;
		}
	}
	MHL_TX_DBG_ERR(hw_context, "VIC = %d (%s)@@!!\n", vic, vic_name_table[i].name);
}


// TODO: FD, TBU, zone control should be reconfigured after tapeout if auto-zone is not deployed
static void set_mhl_zone_settings(struct mhl_dev_context *dev_context , uint32_t pixel_clock_frequency)
{
	struct drv_hw_context *hw_context = (struct drv_hw_context *)&dev_context->drv_context;

	MHL_TX_DBG_INFO(hw_context,"pixel clock:%d %04x rev %02x\n",
					pixel_clock_frequency,
					hw_context->chip_device_id,
					hw_context->chip_rev_id);

#if 0	// TODO: FD, TBC, double-check for SK, check whether there is a need to manual control and what's the threhold
	/*
	 * Modes below 30MHz need a different zone control
	 */
	if (hw_context->chip_rev_id > 0) {
		if (pixel_clock_frequency < 30000000)
			mhl_tx_write_reg(hw_context, REG_TXMZ_CTRL2, 0x01);
		else
			mhl_tx_write_reg(hw_context, REG_TXMZ_CTRL2, 0x00);
	}
#endif

	/*
	 * MSC WRITE_STATUS is required to prepare sink for new mode
	 */
	si_mhl_tx_set_pp_link(dev_context, dev_context->link_mode);
}

/*
 * This function must not be called for DVI mode.
 */
static int set_hdmi_params(struct mhl_dev_context *dev_context)
{
	uint16_t h_total;
	uint16_t v_total;
	uint32_t			pixel_clock_frequency;
	uint32_t			threeDPixelClockRatio;
	uint8_t				packedPixelNeeded=0;
	AviColorSpace_e			input_clr_spc =acsRGB;
	uint8_t				output_clr_spc =acsRGB;
	avi_info_frame_data_byte_4_t	input_video_code;
	struct drv_hw_context	*hw_context = (struct drv_hw_context *)&dev_context->drv_context;
	enum {
		 use_avi_vic
		,use_hardware_totals
	}timing_info_basis=use_avi_vic;

	MHL_TX_DBG_WARN(hw_context,"packed_pixel_available :%d \n",packed_pixel_available(dev_context));
// check if DPI signal is normal
	h_total = si_mhl_tx_drv_get_incoming_horizontal_total((struct drv_hw_context *)dev_context);
	v_total = si_mhl_tx_drv_get_incoming_vertical_total((struct drv_hw_context *)dev_context);
	MHL_TX_DBG_WARN(hw_context, "h_total :%u,v_total:%u\n", h_total,v_total );

//
	/* Extract VIC from incoming AVIF */
	input_video_code.VIC = video_data.inputVideoCode;
	//input_video_code = hw_context->current_avi_info_frame.payLoad.hwPayLoad.namedIfData.ifData_u.bitFields.VIC;

	/*
	 * From VSIF bytes, figure out if we need to perform
	 * frame packing or not. This helps decide if packed pixel
	 * (16-bit) is required or not in conjunction with the VIC.
	 */
	threeDPixelClockRatio = 1;
	si_mhl_tx_drv_get_incoming_horizontal_total(hw_context);

	if (hw_context->valid_vsif && hw_context->valid_3d)
	{
		MHL_TX_DBG_WARN(hw_context, "valid HDMI VSIF\n");
		print_vic_modes(hw_context, (uint8_t) input_video_code.VIC);

		if (0 == input_video_code.VIC) {
			MHL_TX_DBG_ERR(,"AVI VIC is zero!!!\n");
			return false;
		}
		if ( 1 == hw_context->valid_3d_fs )
		{
			MHL_TX_DBG_INFO(dev_context, "3D Pixel Clock Ratio: Frame Packing\n");
			threeDPixelClockRatio = 2;
		}
	}
	else
		{
#ifdef MHL2_ENHANCED_MODE_SUPPORT
			/* no VSIF or EM Mode */
			if (0 == input_video_code.VIC || MHL2_EM_VIC_4K24 == input_video_code.VIC || MHL2_EM_VIC_4K30 == input_video_code.VIC)
#else
			/* no VSIF */
			if (0 == input_video_code.VIC) 
#endif // MHL2_ENHANCED_MODE_SUPPORT
				{

				/* 
				   This routine will not be called until we positively know (from the downstream EDID) 
				   that the sink is HDMI.
				   We do not support DVI only sources.	The upstream source is expected to choose between
				   HDMI and DVI based upon the EDID that we present upstream.
				   The other information in the infoframe, even if it is non-zero, is not helpful for
				   determining the pixel clock frequency.
				   So we try as best we can to infer the pixel clock from the HTOTAL and VTOTAL registers.
				 */
				timing_info_basis = use_hardware_totals;
#ifdef MHL2_ENHANCED_MODE_SUPPORT
				MHL_TX_DBG_WARN(hw_context,"no VSIF and AVI VIC is zero!!! Or EM Mode. trying HTOTAL/VTOTAL\n");
#else
				MHL_TX_DBG_WARN(hw_context,"no VSIF and AVI VIC is zero!!! trying HTOTAL/VTOTAL\n");
#endif // MHL2_ENHANCED_MODE_SUPPORT
			}else{
				print_vic_modes(hw_context, (uint8_t) input_video_code.VIC);
				
				mhl_tx_modify_reg(hw_context, REG_SRST, BIT_MHL_FIFO_RST_MASK, BIT_MHL_FIFO_RST_SET);
				mhl_tx_modify_reg(hw_context, REG_SRST, BIT_MHL_FIFO_RST_MASK, BIT_MHL_FIFO_RST_CLR);
				msleep(5);
			}
		}

	/* make a copy of avif */
//	hw_context->outgoingAviPayLoad  = hw_context->current_avi_info_frame.payLoad.hwPayLoad;	// TODO: FD, TBC, should be ok?
	//memcpy( &(hw_context->outgoingAviPayLoad), &(hw_context->current_avi_info_frame.payLoad.hwPayLoad), sizeof(hw_avi_payload_t) );

	/* compute pixel frequency */
	switch(timing_info_basis){
		case use_avi_vic:
			pixel_clock_frequency = si_edid_find_pixel_clock_from_AVI_VIC(
					dev_context->edid_parser_context,
					input_video_code.VIC);
			break;
		case use_hardware_totals:
			pixel_clock_frequency = si_mhl_tx_find_timings_from_totals(
					dev_context->edid_parser_context);
			if (0 == pixel_clock_frequency){
				MHL_TX_DBG_ERR(,"VIC was zero and totals not supported\n");
				//return false;
			}
			break;
	}

	/* extract input color space */
	input_clr_spc = video_data.outputColorSpace;
	//input_clr_spc = hw_context->current_avi_info_frame.payLoad.hwPayLoad.namedIfData.
	//	ifData_u.bitFields.pb1.colorSpace;
	
	MHL_TX_DBG_INFO(dev_context, "input_clr_spc = %02X input_video_code.VIC:%02X\n",
			input_clr_spc, input_video_code.VIC);

	/*
	 * decide about packed pixel mode
	 */
	pixel_clock_frequency *= threeDPixelClockRatio;
	MHL_TX_DBG_INFO(hw_context, "pixel clock:%u\n", pixel_clock_frequency);

	if (qualify_pixel_clock_for_mhl(dev_context->edid_parser_context,
		pixel_clock_frequency, 24)) {
		MHL_TX_DBG_INFO(hw_context, "OK for 24 bit pixels\n");
	} else {
		/* not enough bandwidth for uncompressed video */
		//if (si_edid_sink_supports_YCbCr422(dev_context->edid_parser_context)) 
		{
			MHL_TX_DBG_INFO(hw_context, "Sink supports YCbCr422\n");

			if (qualify_pixel_clock_for_mhl(
				dev_context->edid_parser_context, pixel_clock_frequency, 16)) {
				/* enough for packed pixel */
				packedPixelNeeded = 1;
			} else {
				MHL_TX_DBG_ERR(hw_context,"unsupported video mode."
						"pixel clock too high %s\n"
						,si_peer_supports_packed_pixel(dev_context)
							? "" :"(peer does not support packed pixel)."
						);
				return	false;
			}
		} 
		/*else {
			MHL_TX_DBG_ERR(hw_context,"unsupported video mode."
					"Sink doesn't support 4:2:2.\n");
			return	false;
		}
		*/
	}

	/*
	 * Determine output color space if it needs to be 4:2:2 or same as input
	 */
	output_clr_spc = input_clr_spc;

	if (packedPixelNeeded){
		if (packed_pixel_available(dev_context)) {
			MHL_TX_DBG_INFO(hw_context, "setting packed pixel mode\n");

			dev_context->link_mode = MHL_STATUS_PATH_ENABLED | MHL_STATUS_CLK_MODE_PACKED_PIXEL;

			/* enforcing 4:2:2 if packed pixel. */
			output_clr_spc = BIT_EDID_FIELD_FORMAT_YCbCr422;

			mhl_tx_write_reg(hw_context
					, REG_VID_MODE
					, REG_VID_MODE_DEFVAL | BIT_VID_MODE_m1080p_ENABLE);

			mhl_tx_modify_reg(hw_context, REG_MHLTX_CTL4,
					BIT_MHLTX_CTL4_MHL_CLK_RATIO_MASK,
					BIT_MHLTX_CTL4_MHL_CLK_RATIO_2X);

			mhl_tx_modify_reg(hw_context, REG_MHLTX_CTL6,
					BIT_MHLTX_CTL6_CLK_MASK,
					BIT_MHLTX_CTL6_CLK_PP);

		} else {
			MHL_TX_DBG_ERR(hw_context,
				"unsupported video mode. Packed Pixel not available on sink."
				"Sink's link mode = 0x%02x\n",
				dev_context->dev_cap_cache.mdc.vid_link_mode);
			return false;
		}
	} else {
		MHL_TX_DBG_INFO(hw_context, "normal Mode ,Packed Pixel mode disabled \n");

		dev_context->link_mode = MHL_STATUS_PATH_ENABLED | MHL_STATUS_CLK_MODE_NORMAL;

		mhl_tx_write_reg(hw_context
				, REG_VID_MODE
				, REG_VID_MODE_DEFVAL | BIT_VID_MODE_m1080p_DISABLE);

		mhl_tx_modify_reg(hw_context, REG_MHLTX_CTL4,
				BIT_MHLTX_CTL4_MHL_CLK_RATIO_MASK,
				BIT_MHLTX_CTL4_MHL_CLK_RATIO_3X);

		
#ifdef MHL2_ENHANCED_MODE_SUPPORT
				if (true == hw_context->mhl2_em_enabled)
				{
					mhl_tx_modify_reg(hw_context, REG_MHLTX_CTL6,
							BIT_MHLTX_CTL6_CLK_MASK,
							BIT_MHLTX_CTL6_CLK_PP);
				}
				else
				{
					mhl_tx_modify_reg(hw_context, REG_MHLTX_CTL6,
							BIT_MHLTX_CTL6_CLK_MASK,
							BIT_MHLTX_CTL6_CLK_NPP);
				}
#else
				mhl_tx_modify_reg(hw_context, REG_MHLTX_CTL6,
						BIT_MHLTX_CTL6_CLK_MASK,
						BIT_MHLTX_CTL6_CLK_NPP);
#endif // MHL2_ENHANCED_MODE_SUPPORT
		
	}

	/* Set input color space */
	mhl_tx_write_reg(hw_context , REG_TPI_INPUT , colorSpaceTranslateInfoFrameToHw[input_clr_spc]);

	/* Set output color space */
	mhl_tx_write_reg(hw_context , REG_TPI_OUTPUT , colorSpaceTranslateInfoFrameToHw[output_clr_spc]);

	set_mhl_zone_settings(dev_context,pixel_clock_frequency);

	/*
	 * Prepare outgoing AVIF for later programming the registers
	 *
	 * the checksum itself is included in the calculation.
	 */
	{
	//hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[0] = 0x00;  
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[0] = output_clr_spc << 5|0x02; 
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[1] = video_data.outputcolorimetryAspectRatio; 

	/*if(VIDEO_CAPABILITY_D_BLOCK_found)
	{
		hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[2] = 0x04; 
		TX_DEBUG_PRINT(("VIDEO_CAPABILITY_D_BLOCK_found = true, limited range\n"));
	}
	else*/
	{
		hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[2] = 0x04;  
		TX_DEBUG_PRINT(("VIDEO_CAPABILITY_D_BLOCK_found= false. defult range\n"));
	}
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[3] = video_data.inputVideoCode; 
	if((video_data.inputVideoCode == HDMI_4k24_DSC) || (video_data.inputVideoCode == HDMI_4k30_DSC))
		hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[3] = 23;
	MHL_TX_DBG_WARN(hw_context,"video_data.inputVideoCode:0x%02x\n",(int)video_data.inputVideoCode);

	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[4] = 0x00;
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[5] = 0x00;
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[6] = 0x00; 
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[7] = 0x00;
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[8] = 0x00; 
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[9] = 0x00;
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[10] = 0x00;
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[11] = 0x00;
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.infoFrameData[12] = 0x00;

	}
	
	hw_context->outgoingAviPayLoad.namedIfData.checksum = 0;
	hw_context->outgoingAviPayLoad.namedIfData.ifData_u.bitFields.pb1.colorSpace
		= output_clr_spc;
	hw_context->outgoingAviPayLoad.namedIfData.checksum =
		calculate_avi_info_frame_checksum(&hw_context->outgoingAviPayLoad);
	TX_DEBUG_PRINT(("hw_context->outgoingAviPayLoad.namedIfData.checksum:0x%02x\n",(int)hw_context->outgoingAviPayLoad.namedIfData.checksum));
	DumpIncomingInfoFrame(&(hw_context->outgoingAviPayLoad),sizeof(hw_context->outgoingAviPayLoad));
	return true;
}


#define dump_edid_fifo(hw_context, block_number) /* do nothing */

int si_mhl_tx_drv_set_upstream_edid(struct drv_hw_context *hw_context, uint8_t *edid, uint16_t length)
{
	uint8_t					reg_val;

	reg_val = mhl_tx_read_reg(hw_context, REG_CBUS_STATUS);
	if (!(BIT_CBUS_HPD & reg_val)){
		return -1;
	}

#ifdef NEVER //(
	if(si_edid_sink_is_hdmi(hw_context->intr_info->edid_parser_context)) {
		mhl_tx_write_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG
				, TMDS_OUTPUT_CONTROL_POWER_DOWN
				| AV_MUTE_MUTED
				| TMDS_OUTPUT_MODE_HDMI
				);
	} else {
		mhl_tx_write_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG
				, TMDS_OUTPUT_CONTROL_POWER_DOWN
				| AV_MUTE_MUTED
				| TMDS_OUTPUT_MODE_DVI
				);
	}
#endif //)

	// TODO: FD, TBI, any need to initialize anything for 9293 side or 9293 side related stuff here?

	// TODO: FD, TBU, to enable 'TIMING_CHANGE'

	// TODO: FD, TBI, to wait until stream from 9293 is stable, then enable 9293 'interrupt'

	/* Disable EDID interrupt */
	enable_intr(hw_context, INTR_EDID, 0);	// TODO: FD, TBI, any chance this will forbid EDID_CHG???

	/* Enable h/w automation of WRITE_BURST */
	hw_context->ready_for_mdt = true;

#ifdef ENABLE_GEN2 //(
	enable_gen2_write_burst(hw_context);
#endif //)

	/*
		Before exposing the EDID to upstream device, setup to drop all packets.
		This ensures we do not get Packet Overflow interrupt.
		Dropping all packets means we still get the AVIF interrupts which is crucial.
		Packet filters must be disabled until after TMDS is enabled.
	*/
	// TODO: FD, TBI, any need to take similar steps to avoid changes from 9293?

	// TODO: FD, TBI, enable 9293 'interrupt' i.e. any need here to enable handling for TIMING_CHANGE 'interrupt' from 9293???

	// TODO: FD, TBI, used to drive_hpd_high here, any chance to be useful in application-level???
	/* HPD was held low all this time. Now we send an HPD high */

	return 0;
}

static void tmds_configure(struct drv_hw_context *hw_context)
{
	MHL_TX_DBG_INFO(hw_context, "called\n");

	mhl_tx_write_reg(hw_context, REG_SRST, 0x9F);
	mhl_tx_write_reg(hw_context, REG_SRST, BIT_MHL_FIFO_AUTO_RST);

	mhl_tx_write_reg(hw_context, REG_DPD, 0x1F);		// TODO: FD, TBC, confirm with system in progress, seems to be wrong

//	mhl_tx_write_reg(hw_context, REG_SYS_CTRL1, 0x37);	// TODO: FD, TBC, wait for feedback: why 0x35 as falling rising edge latch
	mhl_tx_write_reg(hw_context, REG_TMDS_CCTRL, BIT_TMDS_CCTRL_TMDS_OE | BIT_TMDS_CCTRL_SEL_BGR);
	mhl_tx_write_reg(hw_context, REG_USB_CHARGE_PUMP_MHL, BIT_USE_CHARGE_PUMP_MHL_DEFAULT);
	mhl_tx_write_reg(hw_context, REG_USB_CHARGE_PUMP, BIT_USE_CHARGE_PUMP_DEFAULT);

	mhl_tx_write_reg(hw_context, REG_DISC_CTRL3, BIT_DC3_DEFAULT);
	mhl_tx_write_reg(hw_context, REG_MHLTX_CTL1, BIT_MHLTX_CTL1_DISC_OVRIDE_ON);
	mhl_tx_write_reg(hw_context, REG_MHLTX_CTL2, REG_MHLTX_CTL2_DEFVAL);
	mhl_tx_write_reg(hw_context, REG_MHLTX_CTL3, REG_MHLTX_CTL3_DEFVAL);
	mhl_tx_write_reg(hw_context, REG_MHLTX_CTL4, REG_MHLTX_CTL4_DEFVAL);
	mhl_tx_write_reg(hw_context, REG_MHLTX_CTL6, REG_MHLTX_CTL6_DEFVAL);
	mhl_tx_write_reg(hw_context, REG_MHLTX_CTL7, REG_MHLTX_CTL7_DEFVAL);
	mhl_tx_write_reg(hw_context, REG_MHLTX_CTL8, REG_MHLTX_CTL8_DEFVAL);
}

static void power_up(struct drv_hw_context *hw_context)
{
	MHL_TX_DBG_INFO(hw_context, "called\n");

	// Power up TMDS TX core, enable VSYNC/HSYNC and 24-bit input data bus, select falling edge latched
	//mhl_tx_write_reg(hw_context, REG_SYS_CTRL1, 0x35);
	
	//for DDR mode(Pclk Dual edge mode)
	mhl_tx_write_reg(hw_context, REG_SYS_CTRL1, 0x31);//mhl_tx_write_reg(hw_context, REG_SYS_CTRL1, 0x33);

	/* Toggle power strobe on chip */
	mhl_tx_modify_reg(hw_context, REG_DISC_CTRL1, BIT_DISC_CTRL1_STROBE_OFF, 0);	// TODO: FD, MUST, TBC
}

#define	MHL_LOGICAL_DEVICE_MAP		(MHL_DEV_LD_AUDIO | MHL_DEV_LD_VIDEO |	\
		MHL_DEV_LD_MEDIA | MHL_DEV_LD_GUI)
#define DEVCAP_REG(x) REG_CBUS_DEVICE_CAP_0 | DEVCAP_OFFSET_##x

uint8_t	dev_cap_values[] = {
			DEVCAP_VAL_DEV_STATE
		,	DEVCAP_VAL_MHL_VERSION
		,	DEVCAP_VAL_DEV_CAT
		,	DEVCAP_VAL_ADOPTER_ID_H
		,	DEVCAP_VAL_ADOPTER_ID_L
		,	DEVCAP_VAL_VID_LINK_MODE
		,	DEVCAP_VAL_AUD_LINK_MODE
		,	DEVCAP_VAL_VIDEO_TYPE
		,	DEVCAP_VAL_LOG_DEV_MAP
		,	DEVCAP_VAL_BANDWIDTH
		,	DEVCAP_VAL_FEATURE_FLAG
		,	0
		,	0
		,	DEVCAP_VAL_SCRATCHPAD_SIZE
		,	DEVCAP_VAL_INT_STAT_SIZE
		,	DEVCAP_VAL_RESERVED
};

static int init_regs(struct drv_hw_context *hw_context)
{
	int ret_val = 0;

	MHL_TX_DBG_INFO(hw_context, "called\n");

	/* default values for flags */

	// TODO: FD, TBC, double-check & update HDCP/timing_changes_isr related code and then set default to 'false'
	hw_context->video_ready = true;
//	hw_context->video_ready = false;

	// TODO: FD, TBC
	hw_context->video_path = 0;
//	hw_context->video_path = 1;

	hw_context->ready_for_mdt = false;
	hw_context->audio_poll_enabled = false;

	// TODO: FD, MUST, TBC, any need to clear AVIF/VSIF/AIF/etc. here? Need to clear related status/etc.?

	/*
	 * wake pulses necessary in all modes
	 * No OTG, Discovery pulse proceed, Wake pulse not bypassed
	 */
/*
	mhl_tx_write_reg(hw_context, REG_DISC_CTRL9
			, BIT_DC9_WAKE_DRVFLT
			| BIT_DC9_CBUS_LOW_TO_DISCONNECT	// TODO: FD, MORE CHECK
			| BIT_DC9_DISC_PULSE_PROCEED
			);
*/

	{
		/* Enable TPI */
		ret_val = mhl_tx_read_reg(hw_context, REG_TPI_SEL);
		ret_val &= ~BIT_TPI_SEL_SW_TPI_EN_MASK;
		ret_val |= BIT_TPI_SEL_SW_TPI_EN_HW_TPI;
		mhl_tx_write_reg(hw_context, REG_TPI_SEL, ret_val);

#ifdef HDCP_ENABLE
		mhl_tx_write_reg(hw_context, TPI_HDCP_CONTROL_DATA_REG, 0);
#endif // HDCP_ENABLE

		mhl_tx_write_reg(hw_context, REG_TPI_HW_OPT3, 0x76);	// TODO: FD, TBD, seems the default value is OK, may remove this later

		/* TX Source termination ON */
		mhl_tx_write_reg(hw_context, REG_MHLTX_CTL1, BIT_MHLTX_CTL1_TX_TERM_MODE_100DIFF | BIT_MHLTX_CTL1_DISC_OVRIDE_ON);

		/* Ignore VBUS, wait for usbint_clr */
		mhl_tx_write_reg(hw_context, REG_DISC_CTRL8,0x03);
		// mhl_tx_write_reg(hw_context, REG_DISC_CTRL8,0x01);	// TODO: FD, TBC, check whether this line or the above line is better

//		mhl_tx_write_reg(hw_context, REG_DISC_CTRL2, 0xA5);	// TODO: FD, MORE CHECK

		/* Enable CBUS discovery */
//	mhl_tx_write_reg(hw_context, REG_DISC_CTRL1, VAL_DISC_CTRL1_DEFAULT | BIT_DISC_CTRL1_STROBE_OFF | BIT_DISC_CTRL1_MHL_DISCOVERY_ENABLE);
//	mhl_tx_write_reg(hw_context, REG_DISC_CTRL1, VAL_DISC_CTRL1_DEFAULT | BIT_DISC_CTRL1_MHL_DISCOVERY_ENABLE);	// TODO: FD, MORE CHECK

#ifdef HDCP_ENABLE
		/* set time base for one second to be 60Hz/4*5 + 4*/
		mhl_tx_write_reg(hw_context , REG_TPI_HDCP_TIMER_1_SEC , 79);	// TODO: FD, TBI, Design think default value '0' is ok
#endif // HDCP_ENABLE
	}
	/* setup local DEVCAP and few more CBUS registers. */
	{
		/*
		 * Fill-in DEVCAP device ID values with those read
		 * from the transmitter.
		 */
		dev_cap_values[DEVCAP_OFFSET_DEVICE_ID_L] =
			(uint8_t)hw_context->chip_device_id;
		dev_cap_values[DEVCAP_OFFSET_DEVICE_ID_H] =
			(uint8_t)(hw_context->chip_device_id >> 8);

		/* Setup local DEVCAP registers */
		mhl_tx_write_reg_block(hw_context, DEVCAP_REG(DEV_STATE),
				ARRAY_SIZE(dev_cap_values), dev_cap_values);

		/*
		 * Make sure MDT registers are initialized and the MDT
		 * transmit/receive are both disabled.
		 */
		mhl_tx_write_reg(hw_context, REG_CBUS_MDT_XMIT_TIMEOUT, 100);

		/* Clear transmit FIFOs and make sure MDT transmit is disabled */
		mhl_tx_write_reg(hw_context, REG_CBUS_MDT_XMIT_CONTROL, 0x03);

		/* Disable MDT transmit preemptive handshake option */
		mhl_tx_write_reg(hw_context, REG_CBUS_MDT_XFIFO_STAT, 0x00);

		mhl_tx_write_reg(hw_context, REG_CBUS_MDT_RCV_TIMEOUT, 100);

		mhl_tx_write_reg(hw_context, REG_CBUS_LINK_CHECK_HIGH_LIMIT, REG_CBUS_LINK_CHECK_HIGH_LIMIT_DEFVAL);
		mhl_tx_write_reg(hw_context, REG_CBUS_LINK_XMIT_BIT_TIME, REG_CBUS_LINK_XMIT_BIT_TIME_DEFVAL);
	}
#ifdef ENABLE_GEN2 //(
	/*
		Disable h/w automation of WRITE_BURST.
		3D packets are handled using legacy WRITE_BURST method.
	*/
	disable_gen2_write_burst(hw_context);
#endif //)
	hw_context->ready_for_mdt = false;

	// TODO: FD, MUST, should set to power_down status at startup and use RSEN to trigger mode change from D3 to D2
	// i.e. should start in "power down" mode with bit0 set to '0'
	mhl_tx_write_reg(hw_context, REG_DPD, 0x17);		// Bit0/1/2/4 : 1'b0 Power Down : 1'b1 Normal Operarion

// TODO: FD, TBU

	mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x0B, 0x00);	// video mode converter	// TODO: FD, TBI, same as default value, not needed

//	mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x19, 0x00);	// trigger under HW-TPI	// TODO: FD, TBD
//	mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x1A, 0x00);	// tmds output enable	// TODO: FD, TBD, no need to enable tmds at startup

	mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x1F, 0x80);	// SD output enable	// TODO: FD, TBC, double check with system why this value?

	// TODO: FD, TBC, double check the following registers with system why these values?
	mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x20, 0x80);	// 256*Fs	// changed to new value 0x80 from 0x10 per latest macro
	mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x21, 0x00);	//
	mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x22, 0x00);	//
	mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x23, 0x00);	//
	mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x25, 0x0B);	//

	mhl_tx_write_reg(hw_context, TPI_DEVICE_POWER_STATE_CTRL_REG, TX_POWER_STATE_D0); // TODO: FD, MUST, D3/D2/D0 mode switch
	//mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x1A, 0x01);	// enable HDMI output
	mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x1A, 0x00);	// disable HDMI output
	
#ifdef HDCP_ENABLE
	mhl_tx_write_reg(hw_context, TX_PAGE_TPI, 0x2A, 0x01);	// enable HDCP
#endif // HDCP_ENABLE

	return ret_val;
}

void si_mhl_tx_drv_set_hw_tpi_mode(struct drv_hw_context *hw_context, bool hw_tpi_mode)
{
	if ( hw_tpi_mode )
	{
		// Enter HW TPI mode
		mhl_tx_modify_reg(hw_context, REG_TPI_SEL,
				BIT_TPI_SEL_SW_TPI_EN_MASK,
				BIT_TPI_SEL_SW_TPI_EN_HW_TPI);
	}
	else
	{
		// Enter Non-HW TPI mode
		mhl_tx_modify_reg(hw_context, REG_TPI_SEL,
				BIT_TPI_SEL_SW_TPI_EN_MASK,
				BIT_TPI_SEL_SW_TPI_EN_NON_HW_TPI);
	}
}

void si_mhl_tx_drv_disable_video_path(struct drv_hw_context *hw_context)
{
	/* If video was already being output */
	if(hw_context->video_ready && (0 == (AV_MUTE_MUTED &
			 mhl_tx_read_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG)))) {

		/* stop hdcp and video and remember */
		stop_video(hw_context);
		hw_context->video_path = 0;
	}
}

void si_mhl_tx_drv_enable_video_path(struct drv_hw_context *hw_context)
{
#ifdef MHL2_ENHANCED_MODE_SUPPORT
	struct mhl_dev_context	*dev_context;
#endif // MHL2_ENHANCED_MODE_SUPPORT

	MHL_TX_DBG_INFO(dev_context, "called\n");

	/* if a path_en = 0 had stopped the video, restart it unless done already. */
//	if(hw_context->video_ready && (0 == hw_context->video_path)) {		// TODO: FD, MUST, double check this for SK
	{
		/* remember ds has enabled our path */
		hw_context->video_path = 1;
#ifdef MHL2_ENHANCED_MODE_SUPPORT
		// To avoid conflict with DEVCAP read
		dev_context = get_mhl_device_context(hw_context);
		if (true == dev_context->misc_flags.flags.have_complete_devcap)
		{
			// Init Enhanced Mode process after both EDID and DEVCAP are processed
			mhl2_em_query(hw_context);

			/*reg  = mhl_tx_read_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG);
			if(mask == (mask & reg)) {
				start_video(hw_context,hw_context->intr_info->edid_parser_context);
			}*/
		}
/*#else
		reg  = mhl_tx_read_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG);
		if(mask == (mask & reg)) {
			start_video(hw_context,hw_context->intr_info->edid_parser_context);
		}*/
#endif // MHL2_ENHANCED_MODE_SUPPORT
	}
}

void si_mhl_tx_drv_content_off(struct drv_hw_context *hw_context)
{
	MHL_TX_DBG_INFO(dev_context, "RAP CONTENT_OFF video %sready\n",hw_context->video_ready?"":"NOT ");
	/* If video was already being output */
	if(hw_context->video_ready && (0 == (AV_MUTE_MUTED &
			 mhl_tx_read_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG)))) {

		MHL_TX_DBG_INFO(dev_context, "RAP CONTENT_OFF\n");
		/* stop hdcp and video and remember */
		stop_video(hw_context);
	}
}

void si_mhl_tx_drv_content_on(struct drv_hw_context *hw_context)
{
	uint8_t	mask = (TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK);
	uint8_t	reg;

	/* if a path_en = 0 had stopped the video, restart it unless done already. */
	if(hw_context->video_ready) {

		reg  = mhl_tx_read_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG);

		if(mask == (mask & reg)) {
			start_video(hw_context,hw_context->intr_info->edid_parser_context);
		}
	}
}


void dump_audio_register(struct drv_hw_context *hw_context, int audio)
{
	int value = 0;
	int offset = 0;

//	if(audio == AUDIO_44K_2CH || audio == AUDIO_44K_8CH)
	{
		value = mhl_tx_read_reg(hw_context, REG_TPI_CONFIG3);
		MHL_TX_DBG_ERR(hw_context, "REG_TPI_CONFIG3: 0x%x\n", value);

		value = mhl_tx_read_reg(hw_context, TX_PAGE_TPI, 0x0020);
		MHL_TX_DBG_ERR(hw_context, "TX_PAGE_TPI:0x0020: 0x%x\n", value);

		value = mhl_tx_read_reg(hw_context, TX_PAGE_TPI, 0x001F);
		MHL_TX_DBG_ERR(hw_context, "TX_PAGE_TPI:0x001F : 0x%x\n", value);

		value = mhl_tx_read_reg(hw_context, REG_TPI_CONFIG1);
		MHL_TX_DBG_ERR(hw_context, "REG_TPI_CONFIG1 : 0x%x\n", value);

		value = mhl_tx_read_reg(hw_context, REG_TPI_CONFIG2);
		MHL_TX_DBG_ERR(hw_context, "REG_TPI_CONFIG2 : 0x%x\n", value);

		value = mhl_tx_read_reg(hw_context, TX_PAGE_L1, 0x24);
		MHL_TX_DBG_ERR(hw_context, "TX_PAGE_L1:0x24 : 0x%x\n", value);

		value = mhl_tx_read_reg(hw_context, REG_TPI_CONFIG4);
		MHL_TX_DBG_ERR(hw_context, "REG_TPI_CONFIG4 : 0x%x\n", value);

		value = mhl_tx_read_reg(hw_context, TX_PAGE_TPI, 0x0028);
		MHL_TX_DBG_ERR(hw_context, "TX_PAGE_TPI:0x0028 : 0x%x\n", value);

		pr_debug("/********************************Start*********************************/\n");
		pr_debug("Dump TX_PAGE_TPI 0x001F -> 0x0027 Registers\n");
		offset = 0x001F;
		for(offset=0x001F; offset<=0x0028; offset++)
		{
			value = mhl_tx_read_reg(hw_context, TX_PAGE_TPI, offset);
			MHL_TX_DBG_ERR(hw_context, "TX_PAGE_TPI:0x%x : 0x%x\n", offset, value);
		}
		pr_debug("/********************************End***********************************/\n");

		pr_debug("/********************************Start*********************************/\n");
		pr_debug("Dump TX_PAGE_L1 0x0000 -> 0x0034 Registers\n");
		offset = 0x0000;
		for(offset=0x0000; offset<=0x0034; offset++)
		{
			value = mhl_tx_read_reg(hw_context, TX_PAGE_L1, offset);
			MHL_TX_DBG_ERR(hw_context, "TX_PAGE_L1:0x%x : 0x%x\n", offset, value);
		}
		pr_debug("/********************************End***********************************/\n");
	}
}

extern uint8_t CA;//Channel/Speaker Allocation.
extern uint8_t  Samplebit;

extern uint8_t  MAX_channel;

extern uint8_t  Cap_MAX_channel;
extern uint8_t  Cap_Samplebit;
extern uint16_t Cap_SampleRate;

void set_platform_bitwidth(int bitWidth)
{
	Samplebit = bitWidth;
	return ;
}
void check_TV_capability(struct drv_hw_context *hw_context, int audio, int bitWidth)
{
	MHL_TX_DBG_INFO(hw_context, "MAX_channel: %d, Cap_Samplebit: %d, Cap_SampleRate: %d\n", MAX_channel, Cap_Samplebit, Cap_SampleRate);

	if(bitWidth == 24)
	{
		if(Cap_Samplebit != HDMI_BITWIDTH_24)
		{
			MHL_TX_DBG_INFO(hw_context, "Samplebit is not supported\n");
			return ;
		}
	}

	if(audio == AUDIO_32K_8CH || audio == AUDIO_44K_8CH || audio == AUDIO_48K_8CH || audio == AUDIO_96K_8CH || audio == AUDIO_192K_8CH)
	{
		if(Cap_MAX_channel != HDMI_CHANNEL_8)
		{
			MHL_TX_DBG_INFO(hw_context, "MAX_channel is not supported\n");
			return ;
		}
	}

	if(audio == AUDIO_192K_2CH || audio == AUDIO_192K_8CH)
	{
		if(Cap_SampleRate < HDMI_SAMPLERATE_192)
		{
			MHL_TX_DBG_INFO(hw_context, "Cap_SampleRate is not supported\n");
			return ;
		}
	}

	return ;
}
static void configure_audio(struct drv_hw_context *hw_context, int audio)
{
	int BIT_TPI_CONFIG4_AUDIO_WIDTH = BIT_TPI_CONFIG4_AUDIO_WIDTH_16_BIT;
	int BIT_TPI_CONFIG3_AUDIO_TDM_BIT_CH= BIT_TPI_CONFIG3_AUDIO_TDM_32B_CH;
	MHL_TX_DBG_INFO(hw_context, "Audio Update to default: Audio_mode_fs=0x%x,CA=0x%x, Samplebit=%d\n",audio, CA, Samplebit);
	current_audio_info_frame[5]=audio<<2;

	check_TV_capability(hw_context, audio, Samplebit);
	
	if(Samplebit==16){
		BIT_TPI_CONFIG4_AUDIO_WIDTH = BIT_TPI_CONFIG4_AUDIO_WIDTH_16_BIT;
		BIT_TPI_CONFIG3_AUDIO_TDM_BIT_CH= BIT_TPI_CONFIG3_AUDIO_TDM_16B_CH;
		}
	else if(Samplebit==20){
		BIT_TPI_CONFIG4_AUDIO_WIDTH = BIT_TPI_CONFIG4_AUDIO_WIDTH_20_BIT;
		BIT_TPI_CONFIG3_AUDIO_TDM_BIT_CH= BIT_TPI_CONFIG3_AUDIO_TDM_32B_CH;
		}
	else if(Samplebit==24){
		BIT_TPI_CONFIG4_AUDIO_WIDTH = BIT_TPI_CONFIG4_AUDIO_WIDTH_24_BIT;
		BIT_TPI_CONFIG3_AUDIO_TDM_BIT_CH= BIT_TPI_CONFIG3_AUDIO_TDM_32B_CH;
		}
	if ( AUDIO_32K_2CH == audio )
	{
		MHL_TX_DBG_INFO(hw_context, "Audio Update: 32K / 2CH\n");

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG3,
				BIT_TPI_CONFIG_3_AUDIO_INTERFACE_I2S
				| BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_2CH
				| BIT_TPI_CONFIG3_MUTE_MUTED);		// Mute the audio

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG1, 0x03);	// Fs=32KHz
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG4, 0x09|BIT_TPI_CONFIG4_AUDIO_WIDTH);	// XX-bit (refer to stream header), 32KHz, 2CH
	}
	else if ( AUDIO_32K_8CH == audio )
	{
		MHL_TX_DBG_INFO(hw_context, "Audio Update: 32K / 8CH / TDM\n");

		current_audio_info_frame[4] = 0x07; //CC2:CC0=3b111(8ch AVI), CT3:CT0=4b0000(Refer to Stream Header)
		current_audio_info_frame[7] = CA; //current_audio_info_frame[7] = 0x13; 

		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_0 | BIT_TPI_AUDIO_SD_SEL_0);	// SD0 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_1 | BIT_TPI_AUDIO_SD_SEL_1);	// SD1 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_2 | BIT_TPI_AUDIO_SD_SEL_2);	// SD2 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_3 | BIT_TPI_AUDIO_SD_SEL_3);	// SD3 output enable

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG3,
				BIT_TPI_CONFIG_3_AUDIO_INTERFACE_HD_AUDIO
				| BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_8CH_MAX
				| BIT_TPI_CONFIG3_AUDIO_TDM_MSB_1CLK_DEALY
				| BIT_TPI_CONFIG3_AUDIO_TDM_BIT_CH
				| BIT_TPI_CONFIG3_MUTE_MUTED);		// Mute the audio

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG1, BIT_TPI_CONFIG1_AUDIO_FREQUENCY_32K);	// Fs=32KHz
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG4,
				  BIT_TPI_CONFIG4_AUDIO_WIDTH
				| BIT_TPI_CONFIG4_AUDIO_FREQUENCY_32K
				| BIT_TPI_CONFIG4_AUDIO_CHANNEL_8CH);	// // XX-bit (refer to stream header), 32KHz, 8CH

		mhl_tx_write_reg(hw_context, TX_PAGE_L1 , 0x0024, 0x02);//16-bit Set the I2S Input Size for I2S extraction

	}
	else if ( AUDIO_44K_2CH == audio )
	{
		MHL_TX_DBG_INFO(hw_context, "Audio Update: 44K / 2CH\n");

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG3,
				BIT_TPI_CONFIG_3_AUDIO_INTERFACE_I2S
				| BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_2CH
				| BIT_TPI_CONFIG3_MUTE_MUTED);		// Mute the audio
        mhl_tx_write_reg(hw_context, TX_PAGE_TPI,0x0020, 0x90);    // Fs=44KHz
        mhl_tx_write_reg(hw_context, TX_PAGE_TPI,0x001F, 0x91);    // Fs=44KHz

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG1, 0x00);	// Fs=44KHz
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG2, 0x02);	
		mhl_tx_write_reg(hw_context, TX_PAGE_L1,0x24, 0x02);	
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG4, 0x11|BIT_TPI_CONFIG4_AUDIO_WIDTH);	// XX-bit (refer to stream header), 44KHz, 2CH
		mhl_tx_write_reg(hw_context, TX_PAGE_TPI,0x0028, 0x80);	// XX-bit (refer to stream header), 44KHz, 2CH
	}
	else if ( AUDIO_44K_8CH == audio )
	{
		MHL_TX_DBG_INFO(hw_context, "Audio Update: 44.1K / 8CH / TDM \n");

		current_audio_info_frame[4] = 0x07; //CC2:CC0=3b111(8ch AVI), CT3:CT0=4b0000(Refer to Stream Header)
		current_audio_info_frame[7] = CA; //current_audio_info_frame[7] = 0x13; 

		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_0 | BIT_TPI_AUDIO_SD_SEL_0);	// 0x92:0x1F=0x80 ; // SD0 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_1 | BIT_TPI_AUDIO_SD_SEL_1);	// 0x92:0x1F=0x91 ; // SD1 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_2 | BIT_TPI_AUDIO_SD_SEL_2);	// 0x92:0x1F=0xA2 ; // SD2 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_3 | BIT_TPI_AUDIO_SD_SEL_3);	// 0x92:0x1F=0xC3 ; // SD3 output enable

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG3,
				BIT_TPI_CONFIG_3_AUDIO_INTERFACE_HD_AUDIO
				| BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_8CH_MAX
				| BIT_TPI_CONFIG3_AUDIO_TDM_MSB_1CLK_DEALY
				| BIT_TPI_CONFIG3_AUDIO_TDM_BIT_CH
				| BIT_TPI_CONFIG3_MUTE_MUTED);		// 0x92:0x26=0xE0  ; // Mute the audio

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG1, BIT_TPI_CONFIG1_AUDIO_FREQUENCY_44K);	// 0x92:0x24=0x00 ; // Fs=44.1KHz
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG4,
				  BIT_TPI_CONFIG4_AUDIO_WIDTH
				| BIT_TPI_CONFIG4_AUDIO_FREQUENCY_44K
				| BIT_TPI_CONFIG4_AUDIO_CHANNEL_8CH); // 0x92:0x27=0x17 // XX-bit (refer to stream header), 44KHz, 8CH

		mhl_tx_write_reg(hw_context, TX_PAGE_L1 , 0x0024, 0x02);//16-bit Set the I2S Input Size for I2S extraction

	}
	else if ( AUDIO_48K_2CH == audio )
	{
		MHL_TX_DBG_INFO(hw_context, "Audio Update: 48K / 2CH\n");

		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_0 | BIT_TPI_AUDIO_SD_SEL_0);	// SD0 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_1 | BIT_TPI_AUDIO_SD_SEL_1);	// SD1 output disable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_2 | BIT_TPI_AUDIO_SD_SEL_2);	// SD2 output disable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_3 | BIT_TPI_AUDIO_SD_SEL_3);	// SD3 output disable

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG3,
				BIT_TPI_CONFIG_3_AUDIO_INTERFACE_I2S
				| BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_2CH
				| BIT_TPI_CONFIG3_MUTE_MUTED);		// Mute the audio

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG1, BIT_TPI_CONFIG1_AUDIO_FREQUENCY_48K);	// Fs=48KHz
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG4,
				  BIT_TPI_CONFIG4_AUDIO_WIDTH
				| BIT_TPI_CONFIG4_AUDIO_FREQUENCY_48K
				| BIT_TPI_CONFIG4_AUDIO_CHANNEL_2CH);	// 24-bit, 48KHz, 2CH
	}
	else if ( AUDIO_48K_8CH == audio )
	{
		MHL_TX_DBG_INFO(hw_context, "Audio Update: 48K / 8CH / TDM\n");

		current_audio_info_frame[4] = 0x07; //CC2:CC0=3b111(8ch AVI), CT3:CT0=4b0000(Refer to Stream Header)
		current_audio_info_frame[7] = CA; //current_audio_info_frame[7] = 0x13; 

		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_0 | BIT_TPI_AUDIO_SD_SEL_0);	// SD0 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_1 | BIT_TPI_AUDIO_SD_SEL_1);	// SD1 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_2 | BIT_TPI_AUDIO_SD_SEL_2);	// SD2 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_3 | BIT_TPI_AUDIO_SD_SEL_3);	// SD3 output enable

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG3,
				BIT_TPI_CONFIG_3_AUDIO_INTERFACE_HD_AUDIO
				| BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_8CH_MAX
				| BIT_TPI_CONFIG3_AUDIO_TDM_MSB_1CLK_DEALY
				| BIT_TPI_CONFIG3_AUDIO_TDM_BIT_CH
				| BIT_TPI_CONFIG3_MUTE_MUTED);		// Mute the audio

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG1, BIT_TPI_CONFIG1_AUDIO_FREQUENCY_48K);	// Fs=48KHz
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG4,
				  BIT_TPI_CONFIG4_AUDIO_WIDTH
				| BIT_TPI_CONFIG4_AUDIO_FREQUENCY_48K
				| BIT_TPI_CONFIG4_AUDIO_CHANNEL_8CH);	// 24-bit, 48KHz, 8CH

		mhl_tx_write_reg(hw_context, TX_PAGE_L1 , 0x0024, 0x02);//16-bit Set the I2S Input Size for I2S extraction

	}
	else if ( AUDIO_96K_2CH == audio )
	{
		MHL_TX_DBG_INFO(hw_context, "Audio Update: 48K / 2CH\n");

		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_0 | BIT_TPI_AUDIO_SD_SEL_0);	// SD0 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_1 | BIT_TPI_AUDIO_SD_SEL_1);	// SD1 output disable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_2 | BIT_TPI_AUDIO_SD_SEL_2);	// SD2 output disable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_3 | BIT_TPI_AUDIO_SD_SEL_3);	// SD3 output disable

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG3,
				BIT_TPI_CONFIG_3_AUDIO_INTERFACE_I2S
				| BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_2CH
				| BIT_TPI_CONFIG3_MUTE_MUTED);		// Mute the audio

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG1, BIT_TPI_CONFIG1_AUDIO_FREQUENCY_96K);	// Fs=48KHz
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG4,
				  BIT_TPI_CONFIG4_AUDIO_WIDTH
				| BIT_TPI_CONFIG4_AUDIO_FREQUENCY_96K
				| BIT_TPI_CONFIG4_AUDIO_CHANNEL_2CH);	// 24-bit, 48KHz, 2CH
	}
	else if ( AUDIO_96K_8CH == audio )
	{
		MHL_TX_DBG_INFO(hw_context, "Audio Update: 48K / 8CH / TDM\n");

		current_audio_info_frame[4] = 0x07; //CC2:CC0=3b111(8ch AVI), CT3:CT0=4b0000(Refer to Stream Header)
		current_audio_info_frame[7] = CA; //current_audio_info_frame[7] = 0x13; 

		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_0 | BIT_TPI_AUDIO_SD_SEL_0);	// SD0 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_1 | BIT_TPI_AUDIO_SD_SEL_1);	// SD1 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_2 | BIT_TPI_AUDIO_SD_SEL_2);	// SD2 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_3 | BIT_TPI_AUDIO_SD_SEL_3);	// SD3 output enable

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG3,
				BIT_TPI_CONFIG_3_AUDIO_INTERFACE_HD_AUDIO
				| BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_8CH_MAX
				| BIT_TPI_CONFIG3_AUDIO_TDM_MSB_1CLK_DEALY
				| BIT_TPI_CONFIG3_AUDIO_TDM_BIT_CH
				| BIT_TPI_CONFIG3_MUTE_MUTED);		// Mute the audio

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG1, BIT_TPI_CONFIG1_AUDIO_FREQUENCY_96K);	// Fs=48KHz
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG4,
				  BIT_TPI_CONFIG4_AUDIO_WIDTH
				| BIT_TPI_CONFIG4_AUDIO_FREQUENCY_96K
				| BIT_TPI_CONFIG4_AUDIO_CHANNEL_8CH);	// 24-bit, 48KHz, 8CH

		mhl_tx_write_reg(hw_context, TX_PAGE_L1 , 0x0024, 0x02);//16-bit Set the I2S Input Size for I2S extraction

	}
	else if ( AUDIO_192K_2CH == audio )
	{
		MHL_TX_DBG_INFO(hw_context, "Audio Update: 192K / 2CH\n");

		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_0 | BIT_TPI_AUDIO_SD_SEL_0);	// SD0 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_1 | BIT_TPI_AUDIO_SD_SEL_1);	// SD1 output disable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_2 | BIT_TPI_AUDIO_SD_SEL_2);	// SD2 output disable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_3 | BIT_TPI_AUDIO_SD_SEL_3);	// SD3 output disable

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG3,
				BIT_TPI_CONFIG_3_AUDIO_INTERFACE_I2S
				| BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_2CH
				| BIT_TPI_CONFIG3_MUTE_MUTED);		// Mute the audio

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG1, BIT_TPI_CONFIG1_AUDIO_FREQUENCY_192K);	// Fs=192KHz
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG4,
				  BIT_TPI_CONFIG4_AUDIO_WIDTH
				| BIT_TPI_CONFIG4_AUDIO_FREQUENCY_192K
				| BIT_TPI_CONFIG4_AUDIO_CHANNEL_2CH);	// 24-bit, 192KHz, 2CH
	}
	else if ( AUDIO_192K_8CH == audio )
	{
		MHL_TX_DBG_INFO(hw_context, "Audio Update: 192K / 8CH / TDM\n");

		current_audio_info_frame[4] = 0x07; //CC2:CC0=3b111(8ch AVI), CT3:CT0=4b0000(Refer to Stream Header)
		current_audio_info_frame[7] = CA; //current_audio_info_frame[7] = 0x13; 

		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_0 | BIT_TPI_AUDIO_SD_SEL_0);	// SD0 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_1 | BIT_TPI_AUDIO_SD_SEL_1);	// SD1 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_2 | BIT_TPI_AUDIO_SD_SEL_2);	// SD2 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_3 | BIT_TPI_AUDIO_SD_SEL_3);	// SD3 output enable

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG3,
				BIT_TPI_CONFIG_3_AUDIO_INTERFACE_HD_AUDIO
				| BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_8CH_MAX
				| BIT_TPI_CONFIG3_AUDIO_TDM_MSB_1CLK_DEALY
				| BIT_TPI_CONFIG3_AUDIO_TDM_BIT_CH
				| BIT_TPI_CONFIG3_MUTE_MUTED);		// Mute the audio

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG1, BIT_TPI_CONFIG1_AUDIO_FREQUENCY_192K);	// Fs=192KHz
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG4,
				  BIT_TPI_CONFIG4_AUDIO_WIDTH
				| BIT_TPI_CONFIG4_AUDIO_FREQUENCY_192K
				| BIT_TPI_CONFIG4_AUDIO_CHANNEL_8CH);	// 24-bit, 192KHz, 8CH

		mhl_tx_write_reg(hw_context, TX_PAGE_L1 , 0x0024, 0x02);//16-bit Set the I2S Input Size for I2S extraction

	}
	else	// default: 48K / 2CH
	{
		MHL_TX_DBG_INFO(hw_context, "Audio Update to default: 48K / 2CH\n");

		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_ENABLE | BIT_TPI_AUDIO_FIFO_MAP_0 | BIT_TPI_AUDIO_SD_SEL_0);	// SD0 output enable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_1 | BIT_TPI_AUDIO_SD_SEL_1);	// SD1 output disable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_2 | BIT_TPI_AUDIO_SD_SEL_2);	// SD2 output disable
		mhl_tx_write_reg(hw_context, REG_TPI_AUDIO_MAPPING_CONFIG, BIT_TPI_AUDIO_SD_DISABLE | BIT_TPI_AUDIO_FIFO_MAP_3 | BIT_TPI_AUDIO_SD_SEL_3);	// SD3 output disable

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG3,
				BIT_TPI_CONFIG_3_AUDIO_INTERFACE_I2S
				| BIT_TPI_CONFIG3_AUDIO_PACKET_HEADER_LAYOUT_2CH
				| BIT_TPI_CONFIG3_MUTE_MUTED);		// Mute the audio

		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG1, BIT_TPI_CONFIG1_AUDIO_FREQUENCY_48K);	// Fs=48KHz
		mhl_tx_write_reg(hw_context, REG_TPI_CONFIG4,
				  BIT_TPI_CONFIG4_AUDIO_WIDTH
				| BIT_TPI_CONFIG4_AUDIO_FREQUENCY_48K
				| BIT_TPI_CONFIG4_AUDIO_CHANNEL_2CH);	// 24-bit, 48KHz, 2CH
	}

	current_audio_info_frame[3]= 0;
	current_audio_info_frame[3]=calculate_generic_checksum(current_audio_info_frame, 0, 14);
}

void configure_and_send_audio_info(struct mhl_dev_context *dev_context, int audio_format)
{
    struct drv_hw_context *hw_context = (struct drv_hw_context *)&dev_context->drv_context;
	MHL_TX_DBG_INFO(hw_context, "Send Audio IF out again....\n");

	configure_audio(hw_context, audio_format);
	DumpIncomingInfoFrame(&(current_audio_info_frame), 14);

	//unmute the audio
	mhl_tx_modify_reg(hw_context, REG_TPI_CONFIG3, BIT_TPI_CONFIG3_MUTE_MASK, BIT_TPI_CONFIG3_MUTE_NORMAL);

	mhl_tx_write_reg(hw_context, REG_TPI_INFO_FSEL, 0xC2);  //send audio if repeatly
	mhl_tx_write_reg_block(hw_context, REG_TPI_INFO_BYTE00, AUDIO_IF_SIZE, current_audio_info_frame);

	return ;
}

#ifdef	EXAMPLE_ONLY	// These functions are not called from anywhere.
static	void	mute_video(struct drv_hw_context *hw_context)
{
	MHL_TX_DBG_INFO(hw_context, "AV muted\n");
	mhl_tx_modify_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG, AV_MUTE_MASK, AV_MUTE_MUTED);
}
#endif	//	EXAMPLE_ONLY

static void unmute_video(struct drv_hw_context *hw_context)
{
			
	MHL_TX_DBG_INFO(hw_context, "AV unmuted.\n");

	/*
	 * Start sending out InfoFrame & Enable HDMI output
	 */
	//if(si_edid_sink_is_hdmi(hw_context->intr_info->edid_parser_context))
	{

		MHL_TX_DBG_INFO(hw_context, "It's an HDMI sink.\n");
		// This MUST be done before VSIF / Audio IF sending
		mhl_tx_write_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG, TMDS_OUTPUT_MODE_HDMI);
		/*
		 * Send AVIF out
		 */
		//if ( 1 == hw_context->valid_avif )
		{
			MHL_TX_DBG_INFO(hw_context, "Send AVIF out...\n");
			DumpIncomingInfoFrame(&(hw_context->outgoingAviPayLoad),sizeof(hw_context->outgoingAviPayLoad));
			mhl_tx_write_reg_block(hw_context, TX_PAGE_TPI , 0x000C,
					sizeof(hw_context->outgoingAviPayLoad.ifData),
					(uint8_t*)&hw_context->outgoingAviPayLoad.namedIfData);
			mhl_tx_write_reg(hw_context, REG_TPI_AVI_BYTE13, 0x00);
		}

		// This MUST be done before VSIF / Audio IF sending
		//mhl_tx_write_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG, TMDS_OUTPUT_MODE_HDMI);

		/*
		 * Send VSIF out
		 */
		if ( 1 == hw_context->valid_vsif && 1 == hw_context->valid_3d )
		{
			MHL_TX_DBG_INFO(hw_context, "Send VSIF out...\n");
			mhl_tx_write_reg(hw_context, REG_TPI_INFO_FSEL, BIT_TPI_INFO_EN | BIT_TPI_INFO_RPT | BIT_TPI_INFO_SEL_3D_VSIF);	// Send 3D VSIF repeatly
			DumpIncomingInfoFrame(&(hw_context->current_vs_info_frame),sizeof(hw_context->current_vs_info_frame));
			mhl_tx_write_reg_block(hw_context, REG_TPI_INFO_BYTE00, 8, (u8 *)(&(hw_context->current_vs_info_frame)) );	// only 8 bytes are valid for MHL VSIF
			mhl_tx_write_reg(hw_context, REG_TPI_INFO_BYTE30, 0x00);// Trigger the infoframe sending
		}

		/*
		 * Send Audio IF out
		 */
		//if ( 1 == hw_context->valid_audio_if )
		{
			MHL_TX_DBG_INFO(hw_context, "Send Audio IF out...\n");

			configure_audio(hw_context, Audio_mode_fs);
			DumpIncomingInfoFrame(&(current_audio_info_frame),14);
			// Unmute the audio
			mhl_tx_modify_reg(hw_context, REG_TPI_CONFIG3, BIT_TPI_CONFIG3_MUTE_MASK, BIT_TPI_CONFIG3_MUTE_NORMAL);

			mhl_tx_write_reg(hw_context, REG_TPI_INFO_FSEL, 0xC2);	// Send Audio IF repeatly
			mhl_tx_write_reg_block(hw_context, REG_TPI_INFO_BYTE00, AUDIO_IF_SIZE, current_audio_info_frame);
		}

		// SWWA for Bug 29055, begin
		mhl_tx_modify_reg(hw_context, REG_SRST, BIT_AUDIO_FIFO_RST_MASK, BIT_AUDIO_FIFO_RST_SET);
		mhl_tx_modify_reg(hw_context, REG_SRST, BIT_AUDIO_FIFO_RST_MASK, BIT_AUDIO_FIFO_RST_CLR);
		// SWWA for Bug 29055, end
		mhl_tx_modify_reg(hw_context, REG_SRST, BIT_MHL_FIFO_RST_MASK, BIT_MHL_FIFO_RST_SET);
		mhl_tx_modify_reg(hw_context, REG_SRST, BIT_MHL_FIFO_RST_MASK, BIT_MHL_FIFO_RST_CLR);
	}
	#if 0
       	/*
	 * Enable DVI output without InfoFrame sending
	 */
	else
	{
		MHL_TX_DBG_INFO(hw_context, "It's a DVI sink.\n");
		mhl_tx_write_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG, TMDS_OUTPUT_MODE_DVI);
	}
    #endif
	/* Now we can entertain PATH_EN */
	hw_context->video_ready = 1;
}

/*
	Sequence of operations in this function is important.
	1. Turn off HDCP interrupt
	2. Turn of TMDS output
	3. Turn off HDCP engine/encryption
	4. Clear any leftover HDCP interrupt
*/
static void stop_video(struct drv_hw_context *hw_context)
{
	MHL_TX_DBG_INFO(hw_context, "stop video.\n");

#ifdef HDCP_ENABLE
	/* Turn off HDCP interrupt */
	enable_intr(hw_context, INTR_HDCP, (0x00));
	enable_intr(hw_context, INTR_HDCP2, (0x00));
#endif // HDCP_ENABLE

	/* We must maintain the output bit (bit 0) to allow just one bit change
	 * later when BKSV read is triggered. */
	if(si_edid_sink_is_hdmi(hw_context->intr_info->edid_parser_context)) {
		mhl_tx_write_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG
				, TMDS_OUTPUT_CONTROL_POWER_DOWN
				| AV_MUTE_MUTED
				| TMDS_OUTPUT_MODE_HDMI
				);
	} else {
		mhl_tx_write_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG
				, TMDS_OUTPUT_CONTROL_POWER_DOWN
				| AV_MUTE_MUTED
				| TMDS_OUTPUT_MODE_DVI
				);
	}

	// TODO: FD, MUST, seems to be a proper place to stop infoframe sending?

#ifdef HDCP_ENABLE
	/* stop hdcp engine */
	mhl_tx_write_reg(hw_context, TPI_HDCP_CONTROL_DATA_REG, 0);

	/* clear any leftover hdcp interrupt */
	mhl_tx_write_reg(hw_context, g_intr_tbl[INTR_HDCP].stat_page, g_intr_tbl[INTR_HDCP].stat_offset, 0xff);
	mhl_tx_write_reg(hw_context, g_intr_tbl[INTR_HDCP2].stat_page, g_intr_tbl[INTR_HDCP2].stat_offset, 0xff);
#endif // HDCP_ENABLE
}

#ifdef HDCP_ENABLE
static void	start_hdcp(struct drv_hw_context *hw_context)
{
	MHL_TX_DBG_INFO(hw_context, "start_hdcp");

	/* First stop hdcp and video */
	stop_video(hw_context);

#if 0 //(
	/* Came here too often, pause a bit. */
	if( (hdcp_bksv_err_count > HDCP_ERROR_THRESHOLD) ||
			(hdcp_link_err_count > HDCP_ERROR_THRESHOLD) ||
			(hdcp_reneg_err_count > HDCP_ERROR_THRESHOLD) ||
			(hdcp_suspend_err_count > HDCP_ERROR_THRESHOLD))
	{
		MHL_TX_DBG_ERR(hw_context, "Too many HDCP Errors: bksv_err= %d, reneg_err= %d, link_err= %d, suspend_err= %d\n",
				hdcp_bksv_err_count, hdcp_reneg_err_count, hdcp_link_err_count, hdcp_suspend_err_count);
		hdcp_bksv_err_count = hdcp_reneg_err_count = hdcp_link_err_count = hdcp_suspend_err_count = 0;

		//msleep(10 * 1000);

		/*
		 * Check CKDT and SCDT.
		 * It is possible that lack of clock instability is why hdcp is not
		 * succeeding after repeated retries
		 *
		 * Do not continue this anymore. Returning without restarting HDCP
		 * ensures this thread is completely killed.
		 */
		return;
	}
#endif //)

	/* Ensure we get HDCP interrupts now onwards. Clear interrupt first. */
	mhl_tx_write_reg(hw_context, g_intr_tbl[INTR_HDCP].stat_page, g_intr_tbl[INTR_HDCP].stat_offset, 0xff);
	mhl_tx_write_reg(hw_context, g_intr_tbl[INTR_HDCP2].stat_page, g_intr_tbl[INTR_HDCP2].stat_offset, 0xff);

	if(get_cbus_connection_status(hw_context)) {
		/* Enable HDCP interrupt */
		enable_intr(hw_context, INTR_HDCP,
				 (  BIT_TPI_INTR_ST0_HDCP_AUTH_STATUS_CHANGE_EVENT
			  	  | BIT_TPI_INTR_ST0_HDCP_SECURITY_CHANGE_EVENT
				 ));

		/* Enable HDCP interrupt */
		enable_intr(hw_context, INTR_HDCP2,
				 (  BIT_TPI_INTR_ST1_BKSV_DONE
			  	  | BIT_TPI_INTR_ST1_BKSV_ERR
				 ));

		msleep(250);
		/*
		 * Chip requires only bit 4 to change for BKSV read
		 * No other bit should change.
		 */
		mhl_tx_modify_reg(hw_context, TPI_SYSTEM_CONTROL_DATA_REG,
					TMDS_OUTPUT_CONTROL_MASK,
					TMDS_OUTPUT_CONTROL_ACTIVE);
	}
}
#endif // HDCP_ENABLE

/*
 * start_video
 *
 *
 */
static	int start_video(struct drv_hw_context *hw_context, void *edid_parser_context)
{
	struct mhl_dev_context	*dev_context;

	MHL_TX_DBG_INFO(hw_context,"called.\n");

	dev_context = get_mhl_device_context(hw_context);

	/*
	 * stop hdcp and video
	 * this kills any hdcp thread going on already
	 */
	stop_video(hw_context);

	/*
	 * if path has been disabled by PATH_EN = 0 return with error;
	 * When enabled, this function will be called again.
	 * if downstream connection has been lost (CLR_HPD), return with error.
	 */
	 /*
	if((0 == hw_context->video_path)
			|| (0 == get_cbus_connection_status(hw_context))
			|| (false == dev_context->misc_flags.flags.rap_content_on)
		) {
		return false;
	}*/

	/*
	 * For DVI, output video w/o infoframe; No video settings are changed.
	 */
	//if(si_edid_sink_is_hdmi(hw_context->intr_info->edid_parser_context)) {
		/*
		 * setup registers for packed pixel, 3D, colors and AVIF
		 * using incoming info frames.
		 */
		if(false == set_hdmi_params(dev_context)) {
			/* Do not disrupt an ongoing video for bad incoming infoframes */
			return false;
		}
	/*}
	else {
		uint32_t pixel_clock_frequency;
		pixel_clock_frequency = si_mhl_tx_find_timings_from_totals(dev_context->edid_parser_context);
		set_mhl_zone_settings(dev_context,pixel_clock_frequency);
	}*/

#ifdef HDCP_ENABLE
	MHL_TX_DBG_INFO(hw_context,"Start HDCP Authentication\n");
	start_hdcp(hw_context);
#else
	unmute_video(hw_context);
#endif // HDCP_ENABLE

	return true;
}

bool si_mhl_tx_set_path_en_I(struct mhl_dev_context *dev_context)
{
	MHL_TX_DBG_INFO(dev_context, "called si_mhl_tx_set_path_en_I\n");
	
	return start_video((struct drv_hw_context *) (&dev_context->drv_context), NULL);
}

#ifdef HDCP_ENABLE
static int hdcp_isr(struct drv_hw_context *hw_context, uint8_t tpi_int_status)
{
	uint8_t		query_data;

	MHL_TX_DBG_INFO(hw_context, "hdcp interrupt handling...\n");

	query_data = mhl_tx_read_reg(hw_context, TPI_HDCP_QUERY_DATA_REG);
	MHL_TX_DBG_INFO(hw_context, "R3D= %02x R29= %02x\n", tpi_int_status, query_data);

	if (BIT_TPI_INTR_ST0_HDCP_SECURITY_CHANGE_EVENT & tpi_int_status) {
		int link_status;

		link_status = query_data & LINK_STATUS_MASK;

		switch (link_status) {
			case LINK_STATUS_NORMAL:
				unmute_video(hw_context);
				break;

			case LINK_STATUS_LINK_LOST:
				hdcp_link_err_count++;
				start_hdcp(hw_context);
				break;
			case LINK_STATUS_RENEGOTIATION_REQ:
				MHL_TX_DBG_INFO(hw_context
						, "tpi BSTATUS2: 0x%x\n"
						, mhl_tx_read_reg(hw_context,REG_TPI_BSTATUS2)
						);
				hdcp_reneg_err_count++;
				/* Disabling TMDS output here will disturb the clock */
				mhl_tx_modify_reg(hw_context
						, TPI_SYSTEM_CONTROL_DATA_REG
						, AV_MUTE_MASK
						, AV_MUTE_MUTED);

				/* send TPI hardware to HDCP_Prep state */
				mhl_tx_write_reg(hw_context, TPI_HDCP_CONTROL_DATA_REG, 0);
				break;
			case LINK_STATUS_LINK_SUSPENDED:	// TODO: FD, TBC, this bit is rsvd in Drake register map
				MHL_TX_DBG_INFO(hw_context , "######### ATTENTION!!! ######### a rsvd bit in Drake is used.\n");

				hdcp_suspend_err_count++;
				start_hdcp(hw_context);
				break;
		}
	}
	/* Check if HDCP state has changed: */
	else if (BIT_TPI_INTR_ST0_HDCP_AUTH_STATUS_CHANGE_EVENT & tpi_int_status) {
		uint8_t new_link_prot_level;

		new_link_prot_level = (uint8_t)
				(query_data & (EXTENDED_LINK_PROTECTION_MASK |
							LOCAL_LINK_PROTECTION_MASK));

		switch (new_link_prot_level) {
			case (EXTENDED_LINK_PROTECTION_NONE | LOCAL_LINK_PROTECTION_NONE):
				hdcp_link_err_count++;
				start_hdcp(hw_context);
				break;

			case EXTENDED_LINK_PROTECTION_SECURE:
			case LOCAL_LINK_PROTECTION_SECURE:
			case (EXTENDED_LINK_PROTECTION_SECURE | LOCAL_LINK_PROTECTION_SECURE):
				unmute_video(hw_context);
				break;
		}
	}
	return 0;
}
#endif // HDCP_ENABLE

// TODO: FD, TBD, begin
/*
static int to_be_deleted(struct drv_hw_context *hw_context, uint8_t int_status)
{
	hw_context = hw_context;
	int_status = int_status;
	return 0;
}
*/
// End

extern void hdmi_state_callback(enum HDMI_STATE state);
extern unsigned int sii_mhl_connected;
static unsigned int int3Number = 0;
static int int_3_isr(struct drv_hw_context *hw_context, uint8_t int_3_status)
{
#if 1
	uint8_t i;
#endif
	uint8_t block_offset;

	MHL_TX_DBG_INFO(hw_context, "interrupt handling...\n");

	mhl_tx_write_reg(hw_context, REG_INTR3, int_3_status);
	if (!int_3_status)
	{
		return -1;	// -1 means no need to further clear interrupts after calling this function
	}

	if (BIT_INTR3_DDC_CMD_DONE & int_3_status) {
	   
		int ddcStatus;
		ddcStatus = mhl_tx_read_reg(hw_context,REG_DDC_STATUS);
		MHL_TX_EDID_READ(hw_context, "mhl_tx_read_reg(hw_context,REG_DDC_STATUS)=)x%x\n",ddcStatus);
		if (BIT_DDC_STATUS_DDC_NO_ACK & ddcStatus) {

			/* Restart EDID read from block 0 */
			MHL_TX_EDID_READ(hw_context, "before si_mhl_tx_drv_issue_edid_read_request: to read block 0\n");
			if (!si_mhl_tx_drv_issue_edid_read_request(hw_context,
						hw_context->current_edid_request_block=0,
						hw_context->current_edid_request_block_batch=0)) {
				hw_context->intr_info->flags |= DRV_INTR_FLAG_MSC_DONE;
				hw_context->intr_info->msc_done_data =1;
			}
		}
		else {
			// TODO: FD, TBI, improve this snippet after double-check
			/*if (0 == (BIT_INTR3_DDC_FIFO_FULL & int_3_status)) {
				// If there is no FIFO_FULL interrupt after the above clear, just ignore this DDC_DONE interrupt & clear the FIFO
				MHL_TX_EDID_READ(hw_context, "BIT_INTR3_DDC_FIFO_FULL & int_3_status!!n");
				si_mhl_tx_drv_reset_ddc_fifo(hw_context);
				mhl_tx_write_reg(hw_context, g_intr_tbl[INTR_EDID].stat_page, g_intr_tbl[INTR_EDID].stat_offset, 0x0F);
				return -1;
			}*/

			MHL_TX_EDID_READ(hw_context, "This batch of EDID read is complete:\n");
			MHL_TX_EDID_READ(hw_context,
					"\tcurrentEdidRequestBlock:%d\t "	\
					"\tcurrentEdidRequestBlockBatch:%d \t"	\
					"\tedidFifoBlockNumber:%d \n",
					hw_context->current_edid_request_block,
					hw_context->current_edid_request_block_batch,
					hw_context->edid_fifo_block_number);

			// Save current batch of EDID block data
			MHL_TX_EDID_READ(hw_context, "This batch:%d of EDID read:\n",hw_context->current_edid_request_block_batch);
			block_offset = 128 * hw_context->current_edid_request_block + 16 * hw_context->current_edid_request_block_batch;

			//mhl_tx_read_reg_block(hw_context, REG_DDC_DATA, 16, hw_context->current_edid_block_data + block_offset);
            for (i = 0; i < 16; i++)
			{
				* (hw_context->current_edid_block_data + block_offset+ i) = mhl_tx_read_reg(hw_context, REG_DDC_DATA);
			}
/*
			for (i = 0; i < 16; i++)
			{
				pr_debug("%02X:%02X    ", i, hw_context->current_edid_block_data[block_offset+i] );
				if (15 == i )
				{
					pr_debug("\n");
				}
			}
*/

			// The FIFO_FULL interrupt should be able to be cleared now
			mhl_tx_write_reg(hw_context, g_intr_tbl[INTR_EDID].stat_page, g_intr_tbl[INTR_EDID].stat_offset, BIT_INTR3_DDC_FIFO_FULL);
            MHL_TX_EDID_READ(hw_context, " %d \n", hw_context->current_edid_request_block_batch );
            i=0;
            //MHL_TX_EDID_READ(hw_context, " mhl_tx_read_reg(hw_context,REG_DDC_STATUS)=0x%x,(mhl_tx_read_reg(hw_context,REG_DDC_STATUS)&0x0B)=0x% 000000000000\n", mhl_tx_read_reg(hw_context,REG_DDC_STATUS),(mhl_tx_read_reg(hw_context,REG_DDC_STATUS)&0x0B));
				    
            while((i++<30)&&(mhl_tx_read_reg(hw_context,REG_DDC_STATUS)&0x0B)){
                    MHL_TX_EDID_READ(hw_context, " mhl_tx_read_reg(hw_context,REG_DDC_STATUS)=0x%x 111111111\n", mhl_tx_read_reg(hw_context,REG_DDC_STATUS));
				    si_mhl_tx_drv_reset_ddc_fifo(hw_context);
				    mdelay(5);
                    MHL_TX_EDID_READ(hw_context, " mhl_tx_read_reg(hw_context,REG_DDC_STATUS)=0x%x,i=%d 22222222\n", mhl_tx_read_reg(hw_context,REG_DDC_STATUS),i);
				}
			// Totally there are 8 batches for each EDID block read
			if ( 7 > hw_context->current_edid_request_block_batch ) // Issue next batch of EDID block read
			{
				si_mhl_tx_drv_reset_ddc_fifo(hw_context);
				si_mhl_tx_drv_issue_edid_block_batch_read(hw_context,
						hw_context->current_edid_request_block,
						++hw_context->current_edid_request_block_batch);

			}
			else if ( 7 == hw_context->current_edid_request_block_batch ) // It's the complate of last batch of this block
			{
				int num_extensions;

				MHL_TX_EDID_READ(hw_context, "EDID block read complete\n");
				num_extensions =si_mhl_tx_get_num_cea_861_extensions(
						hw_context->intr_info->edid_parser_context,
						hw_context->current_edid_request_block);
				if (num_extensions < 0)	{
					MHL_TX_DBG_ERR(hw_context,"edid problem:%d\n",num_extensions);

					hw_context->current_edid_request_block = 0;
					hw_context->current_edid_request_block_batch = 0;
					hw_context->edid_fifo_block_number = 0;

					si_mhl_tx_drv_reset_ddc_fifo(hw_context);

					if (ne_NO_HPD == num_extensions) {

						// no HPD, so start over
						hw_context->intr_info->flags |= DRV_INTR_FLAG_MSC_DONE;
						hw_context->intr_info->msc_done_data =1;
					} else {
						/* Restart EDID read from block 0 */
						MHL_TX_EDID_READ(hw_context, "before si_mhl_tx_drv_issue_edid_read_request: to read block 0\n");
						if (!si_mhl_tx_drv_issue_edid_read_request(hw_context,
									hw_context->current_edid_request_block=0,
									hw_context->current_edid_request_block_batch=0)) {
							// Notify the component layer with error
							hw_context->intr_info->flags |= DRV_INTR_FLAG_MSC_DONE;
							hw_context->intr_info->msc_done_data =1;
						}
					}
				} else if (hw_context->current_edid_request_block < num_extensions) {
					/* EDID read next block */
					MHL_TX_EDID_READ(hw_context, "before si_mhl_tx_drv_issue_edid_read_request: to read block 0+\n");
					if (!si_mhl_tx_drv_issue_edid_read_request(hw_context,
								++hw_context->current_edid_request_block,
								hw_context->current_edid_request_block_batch=0)) {
						/* Notify the MHL module with error */
						hw_context->intr_info->flags |= DRV_INTR_FLAG_MSC_DONE;
						hw_context->intr_info->msc_done_data =1;
					}
				} else {
					struct mhl_dev_context	*dev_context;
					struct cbus_req 		*req;

					dev_context = get_mhl_device_context(hw_context);
				    mhl_tx_write_reg(hw_context, g_intr_tbl[INTR_EDID].stat_page, g_intr_tbl[INTR_EDID].stat_offset, 0x0F);
                    enable_intr(hw_context, INTR_EDID, 0);
					MHL_TX_EDID_READ(hw_context, "All 0+ block(s) is/are read.\n");
					MHL_TX_DBG_INFO(hw_context, "All EDID have been read.\n");

					/* Inform MHL module of EDID read MSC command completion */
					hw_context->intr_info->flags |= DRV_INTR_FLAG_MSC_DONE;
					hw_context->intr_info->msc_done_data =0;
					hw_context->ready_for_mdt = true;
					//if (true == dev_context->misc_flags.flags.have_complete_devcap)
					{
						#ifdef MHL2_ENHANCED_MODE_SUPPORT
						// Init Enhanced Mode process after both EDID and DEVCAP are processed
						if(0x30 <= si_get_peer_mhl_version(dev_context))
							mhl2_em_query(hw_context);
						#endif

					}

					{
						
						req = dev_context->current_cbus_req;
						si_mhl_tx_handle_atomic_hw_edid_read_complete(dev_context->edid_parser_context, req);
						dev_context->edid_parse_done = true;		// TODO: FD, TBC, check carefully
						dev_context->misc_flags.flags.edid_loop_active = 0;
						mhl_event_notify(dev_context, MHL_TX_EVENT_EDID_DONE, 0, NULL);
						int3Number = 0;
						mhl_tx_stop_timer(dev_context, dev_context->cbus_dpi_timer);
					}
				}
			} // end of "else if ( 7 == hw_current_edid_request_block_batch )"
		}
	}

	return int_3_status;
}

/*
static	int		get_cbus_connection_status(struct drv_hw_context *hw_context)
{
	return (BIT_CBUS_HPD & mhl_tx_read_reg(hw_context, REG_CBUS_STATUS));
}
*/

static int mhl_cbus_err_isr(struct drv_hw_context *hw_context,
							uint8_t cbus_err_int)
{
	int	ret_val = 0;
	uint8_t ddc_abort_reason = 0;
	uint8_t msc_abort_reason = 0;

	MHL_TX_DBG_INFO(hw_context, "interrupt handling...\n");

	/*
	 * Three possible errors could be asserted.
	 * 94[2] = DDC_ABORT
	 * 94[3] = ABORT while receiving a command - MSC_RCV_ERROR
	 * 			The ABORT reasons are in 9A
	 * 94[6] = ABORT while sending a command - MSC_SEND_ERROR
	 * 			The ABORT reasons are in 9C
	 */
	if(cbus_err_int & BIT_CBUS_DDC_ABRT) {
		/*
		 * For DDC ABORTs, options are
		 * 1. reset DDC block. This will hamper a broken HDCP or EDID.
		 * 2. if error persists, reset the chip. In any case video is not
		 *    working. So it should not cause blinks.
		 * In either case, call an API to let SoC know what happened.
		 *
		 * Only option 1 has been implemented here.
		 */

		ddc_abort_reason = mhl_tx_read_reg(hw_context, REG_CBUS_DDC_ABORT_INT);

		MHL_TX_DBG_ERR(hw_context, "CBUS DDC ABORT. Reason = %02X\n", ddc_abort_reason);

		if(DDC_ABORT_THRESHOLD < ++ddc_abort_count) {
			si_mhl_tx_drv_reset_ddc_fifo(hw_context);
			ddc_abort_count = 0;
			MHL_TX_DBG_ERR(hw_context, "DDC fifo has been reset.%s\n"
						,((BIT_CBUS_DDC_PEER_ABORT & ddc_abort_reason)
							&&
						  (ddc_abort_count >= DDC_ABORT_THRESHOLD)
						 ) ? "  Please reset sink device!!!"
						   : ""
						);
		}
	}
	if(cbus_err_int & BIT_CBUS_MSC_ABORT_RCVD) {
		/*
		 * For MSC Receive time ABORTs
		 * - Defer submission of new commands by 2 seconds per MHL specs
		 * - This is not even worth reporting to SoC. Action is in the hands of peer.
		 */
		hw_context->intr_info->flags |= DRV_INTR_FLAG_CBUS_ABORT;

		msc_abort_reason = mhl_tx_read_reg(hw_context, REG_MSC_RCV_ERROR);

		++msc_abort_count;	// TODO: FD, TBI, not actually used now

		MHL_TX_DBG_ERR(hw_context, "#%d: ABORT during MSC RCV. Reason = %02X\n",
				msc_abort_count, msc_abort_reason);

		// TODO: FD, TBI, why not clear the error interrupt here as in 'BIT_CBUS_CMD_ABORT' handling???
	}
	if(cbus_err_int & BIT_CBUS_CMD_ABORT) {
		/*
		 * 1. Defer submission of new commands by 2 seconds per MHL specs
		 * 2. For API operations such as RCP/UCP etc., report the situation to
		 *    SoC and let the decision be there. Internal retries have been already
		 *    done.
		 */
		hw_context->intr_info->flags |= DRV_INTR_FLAG_CBUS_ABORT;

		msc_abort_reason = mhl_tx_read_reg(hw_context, REG_CBUS_MSC_MT_ABORT_INT);

		MHL_TX_DBG_ERR(hw_context, "CBUS ABORT during MSC SEND. Reason = %02X\n",
						msc_abort_reason);

		mhl_tx_write_reg(hw_context,REG_CBUS_MSC_MT_ABORT_INT,msc_abort_reason);
	}
	/*
	 * Print the reason for information
	 */
	if (msc_abort_reason) {
		if (BIT_CBUS_MSC_MT_ABORT_INT_MAX_FAIL & msc_abort_reason) {
				MHL_TX_DBG_ERR(hw_context, "Retry threshold exceeded\n");
		}
		if (BIT_CBUS_MSC_MT_ABORT_INT_PROTO_ERR & msc_abort_reason) {
				MHL_TX_DBG_ERR(hw_context, "Protocol Error\n");
		}
		if (BIT_CBUS_MSC_MT_ABORT_INT_TIMEOUT & msc_abort_reason) {
				MHL_TX_DBG_ERR(hw_context, "Translation layer timeout\n");
		}
		if (BIT_CBUS_MSC_MT_ABORT_INT_UNDEF_CMD & msc_abort_reason) {
				MHL_TX_DBG_ERR(hw_context, "Undefined opcode\n");
		}
		if (BIT_CBUS_MSC_MT_ABORT_INT_MSC_MT_PEER_ABORT & msc_abort_reason) {
				MHL_TX_DBG_ERR(hw_context, "MSC Peer sent an ABORT\n");
		}
	}
	return(ret_val);
}

/*
 * mhl_cbus_isr
 *
 * Only when MHL connection has been established. This is where we have the
 * first looks on the CBUS incoming commands or returned data bytes for the
 * previous outgoing command.
 *
 * It simply stores the event and allows application to pick up the event
 * and respond at leisure.
 *
 *	return values:
 *		0	- MHL interrupts (all of them) have been cleared
 *				- calling routine should exit
 *		1	- MHL interrupts (at least one of them) may not have been cleared
 *				- calling routine should proceed with interrupt processing.
 */
extern void reset_av_info(void);
static int mhl_cbus_isr(struct drv_hw_context *hw_context, uint8_t cbus_int)
{
	struct mhl_dev_context	*dev_context;
	MHL_TX_DBG_INFO(hw_context, "interrupt handling..., cbus_int=0x%02x\n",cbus_int);
	
    if (cbus_int & BIT_CBUS_MSC_MR_WRITE_BURST){ 
            mhl_tx_write_reg(hw_context, REG_CBUS_INT_0,cbus_int & BIT_CBUS_MSC_MR_WRITE_BURST);
            mhl_tx_read_reg_block(hw_context, REG_CBUS_MHL_SCRPAD_0,
						ARRAY_SIZE(hw_context->write_burst_data),
						hw_context->write_burst_data);
			dev_context = get_mhl_device_context(hw_context);
			mhl_event_notify(dev_context, MHL_TX_EVENT_SMB_DATA, 0, (void *)(hw_context->write_burst_data+8));
        }   

	if (cbus_int & ~BIT_CBUS_HPD_RCVD){
		// TODO: FD, TBC
		/* bugzilla 27396
		   Logic to detect missed HPD interrupt.
		   Do not clear BIT_CBUS_HPD_RCVD yet.
		 */
		mhl_tx_write_reg(hw_context, REG_CBUS_INT_0,cbus_int & ~BIT_CBUS_HPD_RCVD);
	}	

	if (BIT_CBUS_HPD_RCVD & cbus_int) {
		uint8_t cbus_status;
		uint8_t status;

		/* Check if a SET_HPD came from the downstream device. */
		cbus_status = mhl_tx_read_reg(hw_context, REG_CBUS_STATUS);
		status = cbus_status & BIT_CBUS_HPD;

		if(BIT_CBUS_HPD & (hw_context->cbus_status ^ cbus_status)) {

			// TODO: FD, TBC, need to be checked together with above 'todo'
			/* bugzilla 27396
			   No HPD interrupt has been missed yet.
			   Clear BIT_CBUS_HPD_RCVD.
			 */
			mhl_tx_write_reg(hw_context, REG_CBUS_INT_0, BIT_CBUS_HPD_RCVD);
			MHL_TX_DBG_INFO(hw_context, "HPD change\n");
		}
		else{
			MHL_TX_DBG_ERR(hw_context, "missed HPD change\n");

			/* leave the BIT_CBUS_HPD_RCVD interrupt uncleared, so that
			 * 	we get another interrupt
			 */
			/* whatever we missed, it's the inverse of what we got */
			status ^= BIT_CBUS_HPD;
			cbus_status ^= BIT_CBUS_HPD;
		}

		MHL_TX_DBG_INFO(hw_context, "DS HPD changed to %02X\n", status);

		hw_context->intr_info->flags |= DRV_INTR_FLAG_HPD_CHANGE;
		hw_context->intr_info->hpd_status = status;

		if ( 0 == status) {
			struct mhl_dev_context	*dev_context;
			dev_context = get_mhl_device_context(hw_context);
			MHL_TX_DBG_ERR(hw_context, "got CLR_HPD\n\n");

			// TODO: FD, TBI, any need to fine-tune some power control here?

			hw_context->current_edid_request_block = 0;

#ifdef ENABLE_GEN2 //(
			/*
			   At DS HPD low:
			   Disable h/w automation of WRITE_BURST.
			   3D packets are handled using legacy WRITE_BURST method.
			 */
			disable_gen2_write_burst(hw_context);
#endif //)
			hw_context->ready_for_mdt = false;

			/* default values for video */
			hw_context->video_ready = true;	// TODO: FD, TBC
		//	hw_context->video_ready = false;
			//hw_context->video_path = 1;	// TODO: FD, TBC
			/*
			 * This cannot wait for the upper layer to notice DRV_INTR_FLAG_HPD_CHANGE.
			 * stop_video relies on the result.
			 */
			si_edid_reset(dev_context->edid_parser_context);
		}
		else {
			MHL_TX_DBG_INFO(hw_context, "\n\nGot SET_HPD\n\n");
			hw_context->video_ready = true;	// TODO: FD, TBC
		//	hw_context->video_path = 0;	// TODO: FD, TBC
		}
		/* if DS sent CLR_HPD or SET_HPD, ensure video is not there */
		stop_video(hw_context);

		hw_context->cbus_status = cbus_status;
	}

	if (BIT_CBUS_MSC_MT_DONE & cbus_int) {
		MHL_TX_DBG_INFO(hw_context, "MSC_REQ_DONE\n");

		hw_context->intr_info->flags |= DRV_INTR_FLAG_MSC_DONE;
		hw_context->intr_info->msc_done_data =
			mhl_tx_read_reg(hw_context, REG_CBUS_PRI_RD_DATA_1ST);

#ifdef ENABLE_GEN2 //(
		/* Enable h/w automation of WRITE_BURST */
		enable_gen2_write_burst(hw_context);
#endif //)
	}
	if (BIT_CBUS_MSC_MT_DONE_NACK & cbus_int){
		MHL_TX_DBG_ERR(hw_context,"MSC_MT_DONE_NACK\n");
		hw_context->intr_info->flags |= DRV_INTR_FLAG_MSC_NAK;
	}

	if (BIT_CBUS_MSC_MR_WRITE_STAT & cbus_int) {

		/* read status bytes */
		mhl_tx_read_reg_block(hw_context, REG_CBUS_WRITE_STAT_0,
				ARRAY_SIZE(hw_context->intr_info->write_stat),
				hw_context->intr_info->write_stat);

		if(MHL_STATUS_DCAP_RDY & hw_context->intr_info->write_stat[0]) {
			MHL_TX_DBG_INFO(hw_context, " got DCAP_RDY\n\n");

			/* Enable EDID interrupt */
			// TODO: FD, TBC, EDID DDC handling interrupt
			//enable_intr(hw_context, INTR_EDID, BIT_INTR3_DDC_CMD_DONE
			//		| BIT_INTR3_DDC_FIFO_FULL);
		}
		/*
		 * Save received write_stat info for later
		 * post interrupt processing
		 */
		hw_context->intr_info->flags |= DRV_INTR_FLAG_WRITE_STAT;
	}

	if ((BIT_CBUS_MSC_MR_MSC_MSG & cbus_int)) {
		/*
		 * Save received MSC message info for later
		 * post interrupt processing
		 */
		hw_context->intr_info->flags |= DRV_INTR_FLAG_MSC_RECVD;
		mhl_tx_read_reg_block(hw_context,
				REG_CBUS_MSC_MR_MSC_MSG_RCVD_1ST_DATA,
				ARRAY_SIZE(hw_context->intr_info->msc_msg),
				hw_context->intr_info->msc_msg);

		MHL_TX_DBG_INFO(hw_context, "MSC MSG: %02X %02X\n",
				hw_context->intr_info->msc_msg[0],
				hw_context->intr_info->msc_msg[1]);
	}

	/*
	 * don't do anything for a scratch pad write received interrupt.
	 * instead wait for the DSCR_CHG interrupt
	 */
	if(BIT_CBUS_MSC_MR_SET_INT & cbus_int) {
		MHL_TX_DBG_INFO(hw_context, "MHL INTR Received\n");
		/*
		 * Save received SET INT message info for later
		 * post interrupt processing
		 */
		hw_context->intr_info->flags |= DRV_INTR_FLAG_SET_INT;
		mhl_tx_read_reg_block(hw_context,
				REG_CBUS_SET_INT_0,
				ARRAY_SIZE(hw_context->intr_info->int_msg),
				hw_context->intr_info->int_msg);

		mhl_tx_write_reg_block(hw_context,
				REG_CBUS_SET_INT_0,
				ARRAY_SIZE(hw_context->intr_info->int_msg),
				hw_context->intr_info->int_msg);

		if (MHL_INT_EDID_CHG & hw_context->intr_info->int_msg[1]) {
			int reg_val;

			MHL_TX_DBG_INFO(hw_context, "\n\ngot EDID_CHG\n\n");

			/* clear this bit so that BIT_TPI_INFO_EN will get cleared by the h/w */
			mhl_tx_modify_reg(hw_context,REG_TPI_INFO_FSEL
					,BIT_TPI_INFO_RPT 
					,0
					);

			/* MHL module will re-read EDID */
			// No need to re-read here, it will be handled in later processing: si_mhl_tx_got_mhl_intr

			stop_video(hw_context);

			/* prevent HDCP interrupts from coming in */
			reg_val = mhl_tx_read_reg(hw_context, REG_TPI_SEL);
			MHL_TX_DBG_INFO(hw_context, "REG_TPI_SEL:%02x\n", reg_val);
			reg_val &= ~BIT_TPI_SEL_SW_TPI_EN_MASK;
			reg_val |= BIT_TPI_SEL_SW_TPI_EN_NON_HW_TPI;
			mhl_tx_write_reg(hw_context, REG_TPI_SEL, reg_val);

			/* SetTPIMode */
			MHL_TX_DBG_INFO(hw_context, "REG_TPI_SEL:%02x\n", reg_val);
			reg_val &= ~BIT_TPI_SEL_SW_TPI_EN_MASK;
			reg_val |= BIT_TPI_SEL_SW_TPI_EN_HW_TPI;
			mhl_tx_write_reg(hw_context, REG_TPI_SEL, reg_val);

#ifdef HDCP_ENABLE
			/*
			 * Clear HDCP interrupt - due to TPI enable, we may get one.
			 * TODO: Document this in the PR.
			 */
			mhl_tx_write_reg(hw_context, g_intr_tbl[INTR_HDCP].stat_page, g_intr_tbl[INTR_HDCP].stat_offset, 0xff);
			mhl_tx_write_reg(hw_context, g_intr_tbl[INTR_HDCP2].stat_page, g_intr_tbl[INTR_HDCP2].stat_offset, 0xff);

#endif // HDCP_ENABLE
		} else if (MHL_INT_DSCR_CHG & hw_context->intr_info->int_msg[0]) {
			MHL_TX_DBG_INFO(hw_context, "got DSCR_CHG\n");
			if(hw_context->gen2_write_burst) {
				MHL_TX_DBG_INFO(hw_context,
						"Ignored DSCR_CHG since MDT is enabled\n");
			} else {
    			struct mhl_dev_context	*dev_context;
    			dev_context = get_mhl_device_context(hw_context);
				mhl_tx_read_reg_block(hw_context, REG_CBUS_MHL_SCRPAD_0,
						ARRAY_SIZE(hw_context->write_burst_data),
						hw_context->write_burst_data);
                si_mhl_tx_process_write_burst_data(dev_context);
			}
		} else if (MHL_INT_DCAP_CHG & hw_context->intr_info->int_msg[0]) {
			MHL_TX_DBG_INFO(hw_context, "\n\ngot DCAP_CHG\n\n");
		}
	}
	return -1;
}

#ifdef HDCP_ENABLE
static int int_hdcp2_isr(struct drv_hw_context *hw_context, uint8_t tpi_int_status)
{
	uint8_t		query_data;

	MHL_TX_DBG_INFO(hw_context, "hdcp2 interrupt handling...\n");

	query_data = mhl_tx_read_reg(hw_context, TPI_HDCP_QUERY_DATA_REG);
	MHL_TX_DBG_INFO(hw_context, "R3E= %02x R29= %02x\n", tpi_int_status, query_data);

	if (BIT_TPI_INTR_ST1_BKSV_DONE & tpi_int_status) {
		MHL_TX_DBG_INFO(hw_context, "BIT_TPI_INTR_ST1_BKSV_DONE handling...\n");
		if (PROTECTION_TYPE_MASK & query_data) {

			// TODO: FD, TBC, check this HDCP CTS SWWA for hardware limitation
			int temp;
			/*
				TODO: Document this as SWWA 27396.
				Wait for bottom five bits of debug 6 register to change before
				reading query_data (0x29) register.
			*/
			do{
				temp = mhl_tx_read_reg(hw_context,REG_TPI_HW_DBG6) & 0x1F;
			} while (temp == 2);
			if (temp < 2)
				return 0; /* TPI hdcp state machine has restarted, just wait for another BKSV_DONE */

			query_data = mhl_tx_read_reg(hw_context, TPI_HDCP_QUERY_DATA_REG);

			/*
				If the downstream device is a repeater, enforce a 5 second delay
				to pass HDCP CTS 1B-03.
				TODO: Describe this in the PR.
			*/
			if (HDCP_REPEATER_YES== (HDCP_REPEATER_MASK & query_data)){
				msleep(HDCP_RPTR_CTS_DELAY_MS);
			}
			// Start authentication here
// TODO: FD, TBC
			mhl_tx_write_reg(hw_context, TPI_HDCP_CONTROL_DATA_REG,
					BIT_TPI_HDCP_CONTROL_DATA_COPP_PROTLEVEL_MAX);
#if 0
			mhl_tx_write_reg(hw_context, TPI_HDCP_CONTROL_DATA_REG,
					  BIT_TPI_HDCP_CONTROL_DATA_COPP_PROTLEVEL_MAX
					| BIT_TPI_HDCP_CONTROL_DATA_DOUBLE_RI_CHECK_ENABLE);
#endif
		}
	}
	else if ( BIT_TPI_INTR_ST1_BKSV_ERR & tpi_int_status) {
		MHL_TX_DBG_INFO(hw_context, "BIT_TPI_INTR_ST1_BKSV_ERR handling...\n");
		hdcp_bksv_err_count++;
		start_hdcp(hw_context);
	}

	return 0;
}
#endif // HDCP_ENABLE

static int int_1_isr(struct drv_hw_context *hw_context, uint8_t int_1_status)
{
	int ret_val = 0;

	MHL_TX_DBG_INFO(hw_context, "interrupt handling...\n");

	if (int_1_status)
	{
		if (BIT_INTR1_RSEN_CHG & int_1_status) {
			uint8_t rsen = 0;

			MHL_TX_DBG_INFO(hw_context, "Got RSEN CHG...\n");
			
			rsen =  mhl_tx_read_reg(hw_context, REG_SYS_STAT);
			if (rsen & BIT_STAT_RSEN)
			{
				MHL_TX_DBG_INFO(hw_context, "Got RSEN_CHG: RSEN_ON\n");
			}
			else
			{
				MHL_TX_DBG_INFO(hw_context, "Got RSEN_CHG: RSEN_OFF,mhl_tx_read_reg(hw_context, REG_INTR4)=0x%x\n",mhl_tx_read_reg(hw_context, REG_INTR4));
			}
		}
	}

	return ret_val;
}

/*
    get_device_id
    returns chip Id
 */
int get_device_id(struct drv_hw_context *hw_context)
{
	int ret_val = 0;
	uint16_t number = 0;

	ret_val =  mhl_tx_read_reg(hw_context, REG_DEV_IDH);
	if (ret_val < 0)
	{
		MHL_TX_DBG_ERR(hw_context, "I2C error 0x%x\n", ret_val);
	}
	else
	{
		number = ret_val << 8;

		ret_val = mhl_tx_read_reg(hw_context, REG_DEV_IDL);
		if (ret_val < 0)
		{
			MHL_TX_DBG_ERR(hw_context, "I2C error 0x%x\n", ret_val);
		}
		else
		{
			ret_val |= number;
			MHL_TX_DBG_ERR(hw_context, "Device ID is: %04X\n", ret_val);
		}
	}

	if ( 0x8348 == ret_val )
	{
		return DEVICE_ID_8348;
	}
	else if ( 0x8346 == ret_val )
	{
		return DEVICE_ID_8346;
	}
	else
	{
		return 0;
	}
}
/*
    get_device_rev
    returns chip revision
 */
static int get_device_rev(struct drv_hw_context *hw_context)
{
	int ret_val;

	#if MHL_PRODUCT_NUM==8348 //(
		ret_val = mhl_tx_read_reg(hw_context, REG_DEV_REV);
	#else //)(
  		///Please execute Makefile with MHL_PRODUCT_NUM={8348}
  		af
  		MHL_TX_DBG_ERR(hw_context, "error MHL_PRODUCT_NUM not defeind \n");
	#endif //)

	if (ret_val < 0) {
		MHL_TX_DBG_ERR(hw_context, "I2C error\n");
		ret_val = -1;
	}

    return ret_val;
}

/*****************************************************************************/
/**
 * @brief Handler for MHL transmitter reset requests.
 *
 * This function is called by the MHL layer to request that the MHL transmitter
 * chip be reset.  Since the MHL layer is platform agnostic and therefore doesn't
 * know how to control the transmitter's reset pin each platform application is
 * required to implement this function to perform the requested reset operation.
 *
 * @param[in]	hwResetPeriod	Time in ms. that the reset pin is to be asserted.
 * @param[in]	hwResetDelay	Time in ms. to wait after reset pin is released.
 *
 *****************************************************************************/
 #ifndef CONFIG_MTK_LEGACY
 extern void reset_mhl_board(int hwResetPeriod, int hwResetDelay);
 #endif 
static void board_reset(struct drv_hw_context *hw_context,
		uint16_t hwResetPeriod,
		uint16_t hwResetDelay)
{
#ifdef CONFIG_MTK_LEGACY

#if defined(GPIO_MHL_RST_B_PIN)
    pr_debug("%s,%d+\n", __func__, __LINE__);
    mt_set_gpio_mode(GPIO_MHL_RST_B_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_MHL_RST_B_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_MHL_RST_B_PIN, GPIO_OUT_ONE);
    mdelay(hwResetPeriod);
    mt_set_gpio_out(GPIO_MHL_RST_B_PIN, GPIO_OUT_ZERO);
    mdelay(hwResetPeriod);
    mt_set_gpio_out(GPIO_MHL_RST_B_PIN, GPIO_OUT_ONE);
    mdelay(hwResetDelay);
#elif defined(CONFIG_MTK_MT6306_SUPPORT)
    pr_debug(":MT6306 %s,%d\n", __func__, __LINE__);
    mt6306_set_gpio_dir(GPIO4, GPIO_DIR_OUT);
    mt6306_set_gpio_out(GPIO4, GPIO_OUT_ONE);
    mdelay(hwResetPeriod);
    mt6306_set_gpio_out(GPIO4, GPIO_OUT_ZERO);
    mdelay(hwResetPeriod);
    mt6306_set_gpio_out(GPIO4, GPIO_OUT_ONE);
    mdelay(hwResetDelay);
#else
    pr_debug("%s,%d Error: GPIO_MHL_RST_B_PIN is not defined\n", __func__, __LINE__);
#endif

#else
    reset_mhl_board(hwResetPeriod, hwResetDelay);
#endif
}

/*
 * clear_and_disable_on_disconnect
 */
static void clear_and_disable_on_disconnect(struct drv_hw_context *hw_context)
{
	uint8_t	intr_num;
	MHL_TX_DBG_INFO(hw_context, "called.\n");
    //mhl_tx_write_reg(hw_context, REG_INTR4, 0x10);
	/* clear and mask all interrupts */
	for(intr_num = 0; intr_num < MAX_INTR; intr_num++)
	{
		// TODO: FD, TBC & TBU, wait for system's report on RGND INTR behaviors to continue
		if(INTR_DISC == intr_num) {
			/* Clear all interrupts */
			mhl_tx_write_reg(hw_context, g_intr_tbl[intr_num].stat_page, g_intr_tbl[intr_num].stat_offset, 0xBF);

			/* Disable all interrupts, but keep only RGND interrupt enabled */
			enable_intr(hw_context, INTR_DISC, BIT_INTR4_RGND_DETECTION);
		}
		// TODO: FD, TBC or TBD???, remove the check for 'INTR_INTR1'
		else if(INTR_INTR1 == intr_num) {
			/* Clear all interrupts */
			mhl_tx_write_reg(hw_context, g_intr_tbl[intr_num].stat_page, g_intr_tbl[intr_num].stat_offset, 0xFF);

			/* Disable all interrupts, but keep only RGND_CHG interrupt enabled */
                        enable_intr(hw_context, INTR_INTR1, 0x00);//enable_intr(hw_context, INTR_INTR1, BIT_INTR1_RSEN_CHG);
		} else {
			/* Clear and disable all interrupts */
			mhl_tx_write_reg(hw_context, g_intr_tbl[intr_num].stat_page, g_intr_tbl[intr_num].stat_offset, 0xFF);
			enable_intr(hw_context, intr_num, 0x00);
		}
	}
}

/*
 * This function performs s/w as well as h/w state transitions.
 */

void switch_to_d3(struct drv_hw_context *hw_context,bool do_interrupt_clear)
{
    // TODO: FD, TBC, any need to change local timing status to INITIAL??? => per latest tests, it works just find during hot-plugs, may not need to do this
    MHL_TX_DBG_INFO(hw_context, "switch_to_d3 called, do_interrupt_clear=%d\n",do_interrupt_clear);
    mhl_tx_vbus_control(VBUS_OFF);

    
//	mhl_tx_write_reg(hw_context, REG_DISC_CTRL1, VAL_DISC_CTRL1_DEFAULT);
   
    /* Meet an MHL CTS timing - Tsrc:cbus_float */
    mdelay(1);
//	mhl_tx_write_reg(hw_context, REG_DISC_CTRL1, VAL_DISC_CTRL1_DEFAULT | BIT_DISC_CTRL1_MHL_DISCOVERY_ENABLE);	// TODO: FD, MORE CHECK

    if (do_interrupt_clear){
               clear_and_disable_on_disconnect(hw_context);
    }
    mhl_tx_modify_reg(hw_context, REG_DISC_CTRL4,
                                 BIT_DC6_USB_OVERRIDE_VALUE,
                                 BIT_DC6_USB_OVERRIDE_VALUE);
    mhl_tx_modify_reg(hw_context, REG_DISC_CTRL6,
                                 BIT_DC6_USB_D_OVERRIDE_ON,
                                 BIT_DC6_USB_D_OVERRIDE_ON);
    
    /*
    * Change state to D3 by clearing bit 0 power control reg
    */
    mhl_tx_modify_reg(hw_context, REG_DPD,
                                 BIT_MASTER_POWER_CTRL,
                                 0x00);
	

	///MHL_TX_DBG_INFO(hw_context, "after switch D3,mhl_tx_read_reg(hw_context, REG_INTR4)=0x%x \n",mhl_tx_read_reg(hw_context, REG_INTR4));
}

/* for otg currency leakage */
void	ForceSwitchToD3( struct mhl_dev_context *dev_context)
{
    MHL_TX_DBG_INFO((struct drv_hw_context *)(&dev_context->drv_context), "ForceSwitchToD3..............\n");
    
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
    mdt_destroy(dev_context);
#endif
	 /*
    * Change state to D3 by clearing bit 0 power control reg
    */
    //mhl_tx_modify_reg((struct drv_hw_context *)(&dev_context->drv_context), REG_DPD,
    //                             BIT_MASTER_POWER_CTRL,  0x00);
    mhl_tx_modify_reg((struct drv_hw_context *)(&dev_context->drv_context), REG_DISC_CTRL1, BIT_DISC_CTRL1_STROBE_OFF, 0);
    mhl_tx_modify_reg((struct drv_hw_context *)(&dev_context->drv_context), REG_DPD, BIT_MASTER_POWER_CTRL, 0);

    MHL_TX_DBG_INFO((struct drv_hw_context *)(&dev_context->drv_context), "!!!!!!!!!!!. ForceSwitchToD3 to D3 0x%x\n", mhl_tx_read_reg((struct drv_hw_context *)(&dev_context->drv_context), REG_INTR4));
	return;
}

void ForceNotSwitchToD3(void)
{    
    board_reset(NULL,TX_HW_RESET_PERIOD,TX_HW_RESET_DELAY);
}


/*
 * disconnect_mhl
 * This function performs s/w as well as h/w state transitions.
 */
static void disconnect_mhl(struct drv_hw_context *hw_context,bool do_interrupt_clear)
{
	MHL_TX_DBG_INFO(hw_context, "called.\n");

	/*
	 * Change TMDS termination to high impedance on disconnection
	 */
	mhl_tx_write_reg(hw_context, REG_MHLTX_CTL1, BIT_MHLTX_CTL1_TX_TERM_MODE_OFF | BIT_MHLTX_CTL1_DISC_OVRIDE_ON);

	mhl_tx_write_reg(hw_context, REG_DISC_CTRL2, REG_DISC_CTRL2_DEFVAL);
	mhl_tx_write_reg(hw_context, REG_DISC_CTRL4, REG_DISC_CTRL4_DEFVAL);
	mhl_tx_write_reg(hw_context, REG_DISC_CTRL5, REG_DISC_CTRL5_DEFVAL);

	/* Enable MHL discovery so we are waken up by h/w on impedance measurement */
//	mhl_tx_write_reg(hw_context, REG_DISC_CTRL1, VAL_DISC_CTRL1_DEFAULT | BIT_DISC_CTRL1_STROBE_OFF | BIT_DISC_CTRL1_MHL_DISCOVERY_ENABLE);
//	mhl_tx_write_reg(hw_context, REG_DISC_CTRL1, VAL_DISC_CTRL1_DEFAULT | BIT_DISC_CTRL1_MHL_DISCOVERY_ENABLE);	// TODO: FD, MORE CHECK

	//if (do_interrupt_clear){
		clear_and_disable_on_disconnect(hw_context);
	//}

	/* 11/23: clear this flag to fix DS hot plug issue */
	hw_context->cbus_status = 0;

}


/*
 * MHL device discovery interrupt handler
 * 	1. When impedance is measured as 1k, RGND interrupt is asserted.
 * 	2. Chip sends out wake pulses and discovery pulses.
 * 	   Then asserts MHL_EST if CBUS stays high to meet MHL timings.
 * 	3. If discovery fails, NON_MHL_EST is asserted.
 *	4. If MHL cable is removed, CBUS_DIS is asserted.
 *	   (Need to check this bit all the time)
 */
static int int_4_isr(struct drv_hw_context *hw_context, uint8_t int_4_status)
{
	int	ret_val = 0; /* Safe to clear interrupt from master handler */

	MHL_TX_DBG_INFO(hw_context, "interrupt handling...\n");

//we li
	if(BIT_INTR4_CBUS_LKOUT & int_4_status){
		mhl_tx_write_reg(hw_context, REG_INTR4, 0x30);
		int_4_status = int_4_status & 0xCF;
		MHL_TX_DBG_INFO(hw_context, "BIT_INTR4_CBUS_LKOUT...\n");
		si_mhl_tx_chip_initialize(hw_context);
	}

	if ((BIT_INTR4_CBUS_DISCONNECT  & int_4_status) ||
		(BIT_INTR4_NON_MHL_EST & int_4_status)) {
		struct mhl_dev_context	*dev_context;
    	dev_context = get_mhl_device_context(hw_context);
		MHL_TX_DBG_ERR(hw_context, "got CBUS_DIS. MHL disconnection or USB\n");

		//set_pin(hw_context,LED_MHL, GPIO_LED_OFF);
               //set_pin(hw_context,LED_USB, GPIO_LED_ON);
               /* Setup termination etc. */
               hw_context->intr_info->flags |= DRV_INTR_FLAG_DISCONNECT;
#if 0
               if (BIT_INTR4_CBUS_DISCONNECT & int_4_status){
                        disconnect_mhl(hw_context,true);
                        switch_to_d3(hw_context,false);
               }else{ /* must be BIT_INTR4_NON_MHL_EST */
                        disconnect_mhl(hw_context,false);
                        switch_to_d3(hw_context,true);
               }
#else
                mhl_tx_stop_timer(dev_context, dev_context->cbus_dpi_timer);
                mhl_tx_stop_timer(dev_context, dev_context->cbus_abort_timer);
                disconnect_mhl(hw_context,true);
                switch_to_d3(hw_context,false);
#endif
		// TODO: FD, TBI, configure LED_VBUS properly, here?

		ret_val = 0xFF;	/* interrupt has been cleared already in disconnect_mhl */

		MHL_TX_DBG_INFO(hw_context, "CBUS disconnection or USB handling done.\n");
	}

	else if (int_4_status & BIT_INTR4_RGND_DETECTION) {
		mhl_tx_modify_reg(hw_context, REG_DISC_STAT2,
					0x80, 0x80);
		if(0x02 == (mhl_tx_read_reg(hw_context, REG_DISC_STAT2) & 0x03)) {
			MHL_TX_DBG_INFO(hw_context, "Cable impedance = 1k (MHL Device)\n");
			/* Call platform function to turn the VBUS on for unpowered dongle */
			mhl_tx_vbus_control(VBUS_ON);			
			mhl_tx_write_reg(hw_context, REG_DISC_CTRL1, 0x27);
			/*************************************************************/
			
            //mhl_tx_write_reg(hw_context, REG_INT_CTRL, BIT_INT_CTRL_POLARITY_LEVEL_LOW | BIT_INT_CTRL_OPEN_DRAIN);  // configure INT: open drain & polarity level as '1'

            tmds_configure(hw_context);

            /* Power up to read device ID */
            power_up(hw_context);

            mhl_tx_write_reg(hw_context, REG_INTR4_MASK, 0x00); // clear INT4 mask

            //mhl_tx_write_reg(hw_context, REG_DISC_CTRL1, VAL_DISC_CTRL1_DEFAULT | BIT_DISC_CTRL1_MHL_DISCOVERY_ENABLE); // TODO: FD, MORE CHECK
            mhl_tx_write_reg(hw_context, REG_MHLTX_CTL1, BIT_MHLTX_CTL1_TX_TERM_MODE_OFF | BIT_MHLTX_CTL1_DISC_OVRIDE_ON);

            mhl_tx_write_reg(hw_context, REG_DISC_CTRL2, REG_DISC_CTRL2_DEFVAL);
            mhl_tx_write_reg(hw_context, REG_DISC_CTRL4, REG_DISC_CTRL4_DEFVAL);
            mhl_tx_write_reg(hw_context, REG_DISC_CTRL5, REG_DISC_CTRL5_DEFVAL);
			/* Power up the chip cores to access registers */
//			power_up(hw_context);	// TODO: FD, MUST, this is required for D0/D2/D3 support: to wake up from D2 to D3

			/* Ensure Wake up pulse is sent */
			mhl_tx_write_reg(hw_context, REG_DISC_CTRL9
					, BIT_DC9_WAKE_DRVFLT
					| BIT_DC9_DISC_PULSE_PROCEED
					);

			
		}
		/* enable remaining discovery interrupts */
		enable_intr(hw_context, INTR_DISC,
				  BIT_INTR4_MHL_EST			
				| BIT_INTR4_NON_MHL_EST		
				| BIT_INTR4_CBUS_LKOUT		
				| BIT_INTR4_CBUS_DISCONNECT	
				| BIT_INTR4_RGND_DETECTION
				| BIT_INTR4_VBUS_CHG // this is required	// TODO: FD, TBI, check PR_Notes for details on this, however, seems no other project support this
				);
		/* Enable MSC interrupt to handle initial exchanges */
		/*enable_intr(hw_context, INTR_MERR,
				 ( BIT_CBUS_DDC_ABRT
				 | BIT_CBUS_MSC_ABORT_RCVD
				 | BIT_CBUS_CMD_ABORT
				 ));
		enable_intr(hw_context, INTR_MSC,
				(  BIT_CBUS_MSC_MT_DONE
				 | BIT_CBUS_HPD_RCVD
				 | BIT_CBUS_MSC_MR_WRITE_STAT
				 | BIT_CBUS_MSC_MR_MSC_MSG
				 | BIT_CBUS_MSC_MR_WRITE_BURST
				 | BIT_CBUS_MSC_MR_SET_INT
				 | BIT_CBUS_MSC_MT_DONE_NACK
				  | BIT_CBUS_CNX_CHG
				));

		enable_intr(hw_context, INTR_INTR1, BIT_INTR1_RSEN_CHG);*/
	}

	else if(int_4_status & BIT_INTR4_MHL_EST) {
		struct mhl_dev_context	*dev_context;
    	dev_context = get_mhl_device_context(hw_context);
		/*
		 * ENABLE_DISCOVERY ensures it sends wake up and discovery pulse
		 * 	 and as result sink/dongle would respond CBUS high.
		 */
		MHL_TX_DBG_ERR(hw_context, "got MHL_EST.  MHL connection\n");

		/* turn on MHL LED */
		//set_pin(hw_context,LED_MHL,GPIO_LED_ON);
		//set_pin(hw_context,LED_USB, GPIO_LED_OFF);

		// TODO: FD, TBI, configure LED_VBUS properly, here?

		/* Setup registers and enable interrupts for DCAP_RDY, SET_HPD etc. */
		init_regs(hw_context);
		
		/*
		 *  Setting this flag triggers sending DCAP_RDY.
		 */
		hw_context->intr_info->flags |= DRV_INTR_FLAG_CONNECT;

		/* Enable MSC interrupt to handle initial exchanges */
		enable_intr(hw_context, INTR_MERR,
				 ( BIT_CBUS_DDC_ABRT
				 | BIT_CBUS_MSC_ABORT_RCVD
				 | BIT_CBUS_CMD_ABORT
				 ));
		enable_intr(hw_context, INTR_MSC,
				(  BIT_CBUS_MSC_MT_DONE
				 | BIT_CBUS_HPD_RCVD
				 | BIT_CBUS_MSC_MR_WRITE_STAT
				 | BIT_CBUS_MSC_MR_MSC_MSG
				 | BIT_CBUS_MSC_MR_WRITE_BURST
				 | BIT_CBUS_MSC_MR_SET_INT
				 | BIT_CBUS_MSC_MT_DONE_NACK
				  | BIT_CBUS_CNX_CHG
				));
		//process_dpi(dev_context);
		//enable_intr(hw_context, INTR_INTR1, BIT_INTR1_RSEN_CHG);
	}

	return ret_val;
}

void SiiMhlTxDrvGetScratchPad(struct drv_hw_context *hw_context,uint8_t startReg,uint8_t *pData,uint8_t length)
{
    int i;
    for (i = 0; i < length;++i,++startReg)
    {
        //*pData++ = mhl_tx_read_reg(hw_context, (TX_PAGE_CBUS | 0x00C0) + startReg);
    }
}

static	int	g2wb_isr(struct drv_hw_context *hw_context, uint8_t intr_stat)
{
	//uint8_t ret_val = 0;
	uint8_t gen2_buffer[GEN2_BUFFER_LENGTH];
	
	MHL_TX_DBG_INFO(hw_context, "interrupt handling...,g2wb_isr()\n");
	//msleep(5);
	/* Read error register if there was any problem */
	/*ret_val  = mhl_tx_read_reg(hw_context, REG_CBUS_MDT_INT_1);

	if(ret_val)
	{
		mhl_tx_write_reg(hw_context, REG_CBUS_MDT_INT_1, ret_val);
		MHL_TX_DBG_INFO(hw_context, "\n\ngot MDT Error = %02X\n", ret_val);
	}
	else
	{*/
		if (BIT_MDT_RXFIFO_DATA_RDY & intr_stat)
		{
			uint8_t length, i;
			//MHL_TX_DBG_INFO(hw_context, "got HAWB data in FIFO\n");
			/* Read all bytes */
			mhl_tx_read_reg_block(hw_context,
					REG_CBUS_MDT_RCV_READ_PORT,
					GEN2_BUFFER_LENGTH,
					gen2_buffer);
			/*
			 * Clear current level in the FIFO.
			 * Moves pointer to the next keep RSM enabled
			 */
			mhl_tx_write_reg(hw_context,
					REG_CBUS_MDT_RCV_CONTROL,
					BIT_CBUS_MDT_RCV_CONTROL_RFIFO_CLR_CUR_CLEAR
					| BIT_CBUS_MDT_RCV_CONTROL_RCV_EN_ENABLE
					);

			/* first byte contains the length of data */
			length = gen2_buffer[0];

			memset(hw_context->write_burst_data, 0, sizeof(hw_context->write_burst_data));
			memcpy(hw_context->write_burst_data, &gen2_buffer[1], GEN2_WRITE_BURST_LENGTH);
			for(i = 0; i <= length; i++) {
				pr_info("0x%x @@", gen2_buffer[i+1]);
				if(i==length)
					pr_info("\n");
			}
			MHL_TX_DBG_INFO(hw_context, "got HAWB data in FIFO end\n");
			/* Signal upper layer of this arrival */
			hw_context->intr_info->flags |= DRV_INTR_FLAG_WRITE_BURST;

			
		}
		//else
		//{
			// FD: TBI, check interrupt bits in this register ONE BY ONE
			//MHL_TX_DBG_INFO(hw_context, "got HAWB INTs other than data arrival, may need to check.\n");
		//}
	//}
	return 0;
}


static void enable_intr(struct drv_hw_context *hw_context, uint8_t intr_num, uint8_t intr_mask)
{
	g_intr_tbl[intr_num].mask = intr_mask;
	mhl_tx_write_reg(hw_context, g_intr_tbl[intr_num].mask_page, g_intr_tbl[intr_num].mask_offset, intr_mask);
}

void enable_intr3(struct drv_hw_context *hw_context)
{
	enable_intr(hw_context, INTR_EDID, BIT_INTR3_DDC_CMD_DONE);
		//| BIT_INTR3_DDC_FIFO_FULL);
}

void si_mhl_tx_drv_device_isr(struct drv_hw_context *hw_context, struct interrupt_info *intr_info)
{
	uint8_t	intr_num;

	hw_context->intr_info = intr_info;

	//MHL_TX_DBG_INFO(hw_context, "got INTR\n");

	/* Skip checking interrupts if GPIO pin is not asserted anymore */
	for(intr_num = 0; (intr_num < MAX_INTR) ; intr_num++) {
		if(g_intr_tbl[intr_num].mask)
		{
			int reg_value;
			uint8_t	intr_stat;

			reg_value = mhl_tx_read_reg(hw_context,
					g_intr_tbl[intr_num].stat_page,
					g_intr_tbl[intr_num].stat_offset);
            //MHL_TX_DBG_INFO(hw_context, "got INTR11111111111111\n");

			if(reg_value < 0) {
				return;
			}

			intr_stat = (uint8_t)reg_value;
			if(intr_stat) {
					/* Clear interrupt since specific ISR did not. */
					mhl_tx_write_reg(hw_context,
							g_intr_tbl[intr_num].stat_page,
							g_intr_tbl[intr_num].stat_offset,
							intr_stat);
					//}
			
            //MHL_TX_DBG_INFO(hw_context, "got INTR,intr_stat1111111=0x%x,intr_num=%d\n",intr_stat,intr_num);
			/* Process only specific interrupts we have enabled. Ignore others*/
			//intr_stat = intr_stat & g_intr_tbl[intr_num].mask;
			//MHL_TX_DBG_INFO(hw_context, "got INTR,intr_stat2222222222222=0x%x\n",intr_stat);
			//if(intr_stat) 
				{
					int already_cleared;

#ifdef	PRINT_ALL_INTR
					MHL_TX_DBG_ERR(hw_context, "INTR-%s = %02X\n", g_intr_tbl[intr_num].name, intr_stat);
#else	// PRINT_ALL_INTR
					MHL_TX_DBG_INFO(hw_context, "INTR-%s = %02X\n", g_intr_tbl[intr_num].name, intr_stat);
#endif	// PRINT_ALL_INTR

					already_cleared = g_intr_tbl[intr_num].isr(hw_context, intr_stat);
					//if (already_cleared >=0){
						/*
						 * only clear the interrupts that were not cleared by the specific ISR.
						 */
						//intr_stat &= ~already_cleared;
				}
			}

		} // end of "if(g_intr_tbl[intr_num].mask)"
#ifdef	PRINT_ALL_INTR
		/* These lines print all interrupt status irrespective of mask */
		else {
			uint8_t	intr_stat;
			/* Only for debugging - Print other masked interrupts - do not clear */
			intr_stat = mhl_tx_read_reg(hw_context,
					g_intr_tbl[intr_num].stat_page,
					g_intr_tbl[intr_num].stat_offset);
			MHL_TX_DBG_ERR(hw_context, "INTN-%s = %02X\n", g_intr_tbl[intr_num].name, intr_stat);

		}
#endif	// PRINT_ALL_INTR
	}
}

/*
 * si_mhl_tx_chip_initialize
 *
 * Chip specific initialization.
 * This function resets and initializes the transmitter and puts chip into sleep.
 * MHL Detection interrupt setups up the chip for video.
 */
int si_mhl_tx_chip_initialize(struct drv_hw_context *hw_context)
{
	int ret_val;
	int status = -1;

	siHdmiTx_VideoSel(HDMI_1080P60);
	siHdmiTx_AudioSel(AUDIO_44K_2CH);
	board_reset(hw_context,TX_HW_RESET_PERIOD,TX_HW_RESET_DELAY);
/*
	if(true ==need_reset_usb_switch)
	{
    	mhl_tx_modify_reg(hw_context, REG_DISC_CTRL7, 0xC0,0x40);
    	
    	mdelay(5);    	
    	board_reset(hw_context,TX_HW_RESET_PERIOD,TX_HW_RESET_DELAY);
    	
    	mhl_tx_modify_reg(hw_context, REG_DISC_CTRL7, 0xC0,0x20);
    }
*/
	mhl_tx_write_reg(hw_context, REG_INT_CTRL, BIT_INT_CTRL_POLARITY_LEVEL_LOW | BIT_INT_CTRL_OPEN_DRAIN);	// configure INT: open drain & polarity level as '1'

	tmds_configure(hw_context);

	/* Power up to read device ID */
	power_up(hw_context);

	mhl_tx_write_reg(hw_context, REG_INTR4_MASK, 0x00);	// clear INT4 mask

	mhl_tx_write_reg(hw_context, REG_DISC_CTRL1, VAL_DISC_CTRL1_DEFAULT | BIT_DISC_CTRL1_MHL_DISCOVERY_ENABLE);	// TODO: FD, MORE CHECK

	ret_val = get_device_rev(hw_context);
	hw_context->chip_rev_id = (uint8_t)ret_val;

	ret_val = get_device_id(hw_context);
	hw_context->chip_device_id = (uint16_t)ret_val;
    chip_device_id = ret_val;
	if (ret_val > 0)
	{
		MHL_TX_DBG_ERR(hw_context, "Found SiI%04X rev: %01X.%01X\n",
				hw_context->chip_device_id,
				hw_context->chip_rev_id >> 4,
				(hw_context->chip_rev_id & 0x0f));

		/* Move to disconnected state. Let RGND/MHL connection event start the driver */
		disconnect_mhl(hw_context, true);
		//mhl_tx_modify_reg(hw_context, REG_DISC_STAT2, 0x80,0x00);
		switch_to_d3(hw_context, false);

		status = 0;

		// TODO: FD, TBI, any better place to initialze the following?
		//hw_context->valid_audio_if = 0;
		hw_context->valid_vsif = 0;
		//hw_context->valid_avif = 0;
		hw_context->valid_3d = 0;
		hw_context->valid_3d_fs = 0;
#ifdef MHL2_ENHANCED_MODE_SUPPORT
		mhl2_em_init(hw_context);
#endif // MHL2_ENHANCED_MODE_SUPPORT

		//hw_context->current_audio_configure = 0;
		//memset( hw_context->current_audio_info_frame, 0, AUDIO_IF_SIZE );
		memset(&hw_context->current_vs_info_frame, 0, sizeof(hw_context->current_vs_info_frame));
		//memset(&hw_context->current_avi_info_frame, 0, sizeof(hw_context->current_avi_info_frame));
	}
	else
	{
		MHL_TX_DBG_ERR(hw_context, "SiI8348 is not found!\n");
		MHL_TX_DBG_ERR(hw_context, "Found %04X rev: %01X.%01X\n",
				hw_context->chip_device_id,
				hw_context->chip_rev_id >> 4,
				(hw_context->chip_rev_id & 0x0f));
	}

	return status;
}


void si_mhl_tx_drv_shutdown(struct drv_hw_context *hw_context)
{
	if( is_reset_on_exit_requested() )
	{
		board_reset(hw_context,TX_HW_RESET_PERIOD,TX_HW_RESET_DELAY);
		//gpio_expander_reset();
		MHL_TX_DBG_ERR(hw_context, "MHL hardware was reset\n");
	}
}
//------------------------------------------------------------------------------
// Function Name: siHdmiTx_VideoSel()
// Function Description: Select output video mode
//
// Accepts: Video mode
// Returns: none
// Globals: none
//------------------------------------------------------------------------------
void siHdmiTx_VideoSel (int vmode)
{
	int AspectRatio = 0;
	video_data.inputColorSpace	= acsRGB;
	video_data.outputColorSpace = acsRGB;
	video_data.inputVideoCode	= vmode;
	
	TX_DEBUG_PRINT(("video_data.inputVideoCode:0x%02x\n",(int)video_data.inputVideoCode));
	//siHdmiTx.ColorDepth			= VMD_COLOR_DEPTH_8BIT;
	//siHdmiTx.SyncMode 			= EXTERNAL_HSVSDE;
		
	switch (vmode)
	{
		case HDMI_480I60_4X3:
		case HDMI_576I50_4X3:
			AspectRatio = VMD_ASPECT_RATIO_4x3;
			break;
			
		case HDMI_480I60_16X9:
		case HDMI_576I50_16X9:
			AspectRatio  	= VMD_ASPECT_RATIO_16x9;
			break;
			
		case HDMI_480P60_4X3:
		case HDMI_576P50_4X3:
		case HDMI_640X480P:
			AspectRatio  	= VMD_ASPECT_RATIO_4x3;
			break;

		case HDMI_480P60_16X9:
		case HDMI_576P50_16X9:
			AspectRatio  	= VMD_ASPECT_RATIO_16x9;
			break;
			
		case HDMI_720P60:
		case HDMI_720P50:
		case HDMI_1080I60:
		case HDMI_1080I50:
		case HDMI_1080P24:
		case HDMI_1080P25:
		case HDMI_1080P30:	
		case HDMI_1080P50:
		case HDMI_1080P60:
		case HDMI_4k30_DSC:
		case HDMI_4k24_DSC:
		case 23:
			AspectRatio  	= VMD_ASPECT_RATIO_16x9;
			break;
				
		default:
			break;
		}
	if (AspectRatio == VMD_ASPECT_RATIO_4x3)
        video_data.inputcolorimetryAspectRatio = 0x18;
    else video_data.inputcolorimetryAspectRatio = 0x28;
   		video_data.input_AR = AspectRatio;

}

void siHdmiTx_AudioSel (int AduioMode)
{
	Audio_mode_fs			= AduioMode;
	/*
	siHdmiTx.AudioChannels		= ACHANNEL_2CH;
	siHdmiTx.AudioFs				= Afs;
	siHdmiTx.AudioWordLength		= ALENGTH_24BITS;
	siHdmiTx.AudioI2SFormat		= (MCLK256FS << 4) |SCK_SAMPLE_RISING_EDGE |0x00; //last num 0x00-->0x02
	*/
}

#define SIZE_VSIF					8
static uint8_t calculate_vsif_checksum(uint8_t *vsif)
{
	uint8_t checksum = 0;

	return calculate_generic_checksum(vsif, checksum, SIZE_VSIF);
}

static int is_valid_vsif(struct mhl_dev_context *dev_context, vendor_specific_info_frame_t *vsif)
{
	uint8_t	checksum;

	/*
		Calculate the checksum assuming that the payload includes the checksum
	*/
	checksum = calculate_generic_checksum((uint8_t *)vsif, 0,
			sizeof(vsif->header) + vsif->header.length );
	if (0 != checksum) {
		MHL_TX_DBG_WARN(dev_context, "VSIF info frame checksum is: 0x%02x should be 0\n", checksum);
		/*
			Try again, assuming that the header includes the checksum.
		*/
		checksum = calculate_generic_checksum((uint8_t *)vsif, 0,
			sizeof(vsif->header) + vsif->header.length
			+ sizeof(vsif->payLoad.checksum));
		if (0 != checksum){
			MHL_TX_DBG_ERR(dev_context, "VSIF info frame checksum "
					"(adjusted for checksum itself) is: 0x%02x "
						   "should be 0\n", checksum);
			return 0;
		}
	} 
	if (0x81 != vsif->header.type_code) {
		MHL_TX_DBG_ERR(dev_context, "Invalid VSIF type code: 0x%02x\n",
					   vsif->header.type_code);
		return 0;

	} else if (0x01 != vsif->header.version_number) {
		MHL_TX_DBG_ERR(dev_context, "Invalid VSIF version: 0x%02x\n",
					   vsif->header.version_number);
		return 0;

	} else {
		return 1;
	}
}

/*
	process_info_frame_change
		called by the MHL Tx driver when a
        new AVI info frame is received from upstream
		OR
		called by customer's SOC video driver when a mode change is desired.
*/

void process_info_frame_change(struct drv_hw_context *hw_context
		, vendor_specific_info_frame_t *vsif
		, avi_info_frame_t *avif)
{
	bool mode_change = false;
	struct mhl_dev_context	*dev_context;

	dev_context = container_of((void *)hw_context, struct mhl_dev_context, drv_context);

	if (NULL != vsif) {
		if(is_valid_vsif(dev_context, vsif)) {
		    MHL_TX_DBG_INFO(hw_context, "refresh VISF!!!!!!!\n");
			memcpy( (void *)&(hw_context->current_vs_info_frame), (void *)vsif, sizeof(vendor_specific_info_frame_t));
			hw_context->valid_vsif = 1;
			mode_change = true;
		}
		else
		{
			MHL_TX_DBG_INFO(hw_context, "It's NOT a valid VSIF!\n");
			hw_context->valid_vsif = 0;
		}
	}
	/*if (NULL != avif) {
		if(is_valid_avi_info_frame(dev_context, avif)) {
			memcpy( (void *)&(hw_context->current_avi_info_frame), (void *)avif, sizeof(avi_info_frame_t));
			hw_context->valid_avif = 1;
			mode_change = true;
		}
		else
		{
			MHL_TX_DBG_INFO(hw_context, "It's NOT a valid AVIF!\n");
			hw_context->valid_avif = 0;
		}
	}

	// No need to check Audio IF

	// Save the changes only, no need to proceed if EDID is not yet parsed
	if ( false == dev_context->edid_parse_done )
	{
		return;
	}

	if (mode_change) {
		start_video(hw_context,dev_context->edid_parser_context);
	}*/
}
static void fill_vsif(struct drv_hw_context *hw_context, uint8_t *p_vsif, int video_3d)
{
//	uint8_t video_3d_structure = 0;			// per HDMI
	uint8_t mhl_3d_fmt_type = 0;
	uint8_t mhl_vid_fmt = 0;

	MHL_TX_DBG_INFO(hw_context, "Fill VSIF, video_3d:%02X ...\n", video_3d);

	if ( VIDEO_3D_NONE == video_3d )
	{
		MHL_TX_DBG_INFO(hw_context, "Input Video 3D Update: NO 3D\n");
		mhl_vid_fmt = 0x00;
		hw_context->valid_3d = 0;
		hw_context->valid_3d_fs = 0;
	}
	else if ( VIDEO_3D_FS == video_3d )
	{
		MHL_TX_DBG_INFO(hw_context, "Input Video 3D Update: 3D - Frame Sequential\n");
//		video_3d_structure = 0x00;		// per HDMI
		mhl_vid_fmt = 0x01;
		mhl_3d_fmt_type = 0x00;
		hw_context->valid_3d = 1;
		hw_context->valid_3d_fs = 1;
	}
	else if ( VIDEO_3D_TB == video_3d )
	{
		MHL_TX_DBG_INFO(hw_context, "Input Video 3D Update: 3D - Top Bottom\n");
//		video_3d_structure = 0x06;		// per HDMI
		mhl_vid_fmt = 0x01;
		mhl_3d_fmt_type = 0x01;
		hw_context->valid_3d = 1;
		hw_context->valid_3d_fs = 0;
	}
	else if ( VIDEO_3D_SS == video_3d )
	{
		MHL_TX_DBG_INFO(hw_context, "Input Video 3D Update: 3D - Side by side\n");
//		video_3d_structure = 0x08;		// per HDMI
		mhl_vid_fmt = 0x01;
		mhl_3d_fmt_type = 0x02;
		hw_context->valid_3d = 1;
		hw_context->valid_3d_fs = 0;
	}

	// Fill VSIF

	p_vsif[0] = 0x81;	// VSIF header
	p_vsif[1] = 0x01;	// Version
	p_vsif[2] = 0x04;	// Length

	p_vsif[3] = 0x00;	// Initial checksum

	// Per MHL, begin
	// Should use this for MHL
	p_vsif[4] = 0x1D;	// 0x7CA61D in Little Endian
	p_vsif[5] = 0xA6;
	p_vsif[6] = 0x7C;
	p_vsif[7] = mhl_3d_fmt_type << 2 | mhl_vid_fmt;
	// Per MHL, end

	p_vsif[3] = calculate_vsif_checksum(p_vsif);	// Checksum

	// In case there is a need to use HDMI VSIF
	// Per HDMI, begin
	/*
	   p_vsif[2] = 0x07;	// Length

	// For compatibility with 'bad' MHL sinks, which accept HDMI IEEE OUI instead of MHL IEEE OUI
	   p_vsif[4] = 0x03;	// 0x000C03 in Little Endian
	   p_vsif[5] = 0x0C;
	   p_vsif[6] = 0x00;

	   p_vsif[7] = 0x40;	// 0x010-00000 -- 3D format indication present
	   p_vsif[8] = video_3d_structure << 4;	// 3D structure & 3D_Meta_present=0
	   p_vsif[9] = 0x00;
	   p_vsif[10] = 0x00;
	 */
	// Per HDMI, end
}

#ifdef MHL2_ENHANCED_MODE_SUPPORT

bool si_mhl_tx_em_process_packet(struct mhl_dev_context *dev_context,void *pkt)
{
	struct drv_hw_context *hw_context = (struct drv_hw_context *)&dev_context->drv_context;

	bool process_status = false;

	uint8_t *packet = (uint8_t *)pkt;

	uint8_t adopter_id_high = 0;
	uint8_t adopter_id_low = 0;
	uint8_t packet_sequence = 0;
	uint8_t packet_count = 0;
	uint8_t packet_length = 0;
	uint8_t packet_checksum = 0;
	uint8_t packet_payload[MHL2_EM_PACKET_PAYLOAD_LEN_MAX] = {0};

	uint8_t msg_id = 0;
	uint8_t msg_len = 0;
	uint8_t msg_cks = 0;

	uint8_t mhl2_em_rpl = 0;

	// Verify Packet Checksum first
	packet_checksum = calculate_generic_checksum(packet, 0, GEN2_WRITE_BURST_LENGTH);
	if (0 != packet_checksum)
	{
		MHL_TX_DBG_INFO(hw_context, "Checksum is wrong in received EM packet.\n");
		goto exit;
	}

	// Extrack Packet Fields
	adopter_id_high = packet[0];
	adopter_id_low = packet[1];
	packet_sequence = packet[2] >> 4;
	packet_count = packet[2] & 0x0F;
	packet_length = packet[3];

	if (0x42 != adopter_id_low || 0x01 != adopter_id_high)
	{
		MHL_TX_DBG_INFO(hw_context, "Adopter ID is not SIMG ID.\n");
		goto exit;
	}

	if (0x00 != packet_sequence || 0x01 != packet_count)
	{
		// FD: TODO, to handle multi-packet Message
		MHL_TX_DBG_INFO(hw_context, "Need to handle multi-packet Message.\n");
		goto exit;
	}

	// Extract Message from Packet
	memcpy(packet_payload, packet + MHL2_EM_PACKET_FRAME_LEN, MHL2_EM_PACKET_PAYLOAD_LEN_MAX);

	// Extract Message Fields
	msg_id = packet_payload[0];
	msg_len = packet_payload[1];

	// Verify Message Checksum
	msg_cks = calculate_generic_checksum(packet_payload, 0, msg_len);
	if (0 != msg_cks)
	{
		MHL_TX_DBG_INFO(hw_context, "Checksum is wrong in received EM Message.msg_id:0x%x\n",msg_id);
		goto exit;
	}

	switch (msg_id)
	{
	case MHL2_EM_CMD_QUERY_CAP:
	case MHL2_EM_CMD_REQ_MODE:
	case MHL2_EM_CMD_SEND_PPS_FULL:
	case MHL2_EM_CMD_SEND_PPS_PART:
		MHL_TX_DBG_INFO(hw_context, "Slave side packet, should not be here.\n");
		break;

	case MHL2_EM_RPL_QUERY_CAP:

		// Get Enhanced Mode Capability
		hw_context->mhl2_em_capability = packet_payload[MHL2_EM_MESSAGE_FRAME_LEN];
		MHL_TX_DBG_INFO(hw_context, "MHL2 EM Message MHL2_EM_RPL_QUERY_CAP received.-0x%x\n",hw_context->mhl2_em_capability);

		// Move the state machine forward
		hw_context->mhl2_em_sm = MHL2_EM_SM_CAP_QUERIED;
		//if(video_data.inputVideoCode > 90)
		//	hw_context->mhl2_em_input = true;
		// In case source is already there before this connection
		/*if (video_data.inputVideoCode > 90)
		{
			MHL_TX_DBG_INFO(hw_context, "Source is waiting, call mhl2_em_request after RPL_QUERY_CAP received.\n");
			mhl2_em_request(hw_context, true);
		}
		else
			mhl2_em_request(hw_context, false);*/

		process_status = true;
		break;

	case MHL2_EM_RPL_REQ_MODE:
		MHL_TX_DBG_INFO(hw_context, "MHL2 EM Message MHL2_EM_RPL_REQ_MODE received.\n");

		// Get Request Mode Reply
		mhl2_em_rpl = packet_payload[MHL2_EM_MESSAGE_FRAME_LEN];

		switch(mhl2_em_rpl)
		{
		case MHL2_EM_RPL_NACK:
			MHL_TX_DBG_INFO(hw_context, "MHL2 EM reply: NACK\n");
			goto exit;
			break;

		case MHL2_EM_RPL_SUCCESS:
			MHL_TX_DBG_INFO(hw_context, "MHL2 EM reply: SUCCESS\n");
			break;

		case MHL2_EM_RPL_FAILURE:
			MHL_TX_DBG_INFO(hw_context, "MHL2 EM reply: FAILURE\n");
			goto exit;
			break;

		default:
			MHL_TX_DBG_INFO(hw_context, "WRONG: unknown MHL2 EM reply!\n");
			goto exit;
			break;
		}

		// Update current Mode
		hw_context->mhl2_em_current_mode = hw_context->mhl2_em_request_mode;

		MHL_TX_DBG_INFO(hw_context, "Current EM Mode is : %d\n", hw_context->mhl2_em_sm);	// FD: TBD, debug

		if (MHL2_EM_SM_REQ_MODE_EM == hw_context->mhl2_em_sm)
		{
			hw_context->mhl2_em_sm = MHL2_EM_SM_MODE_EM;
			hw_context->mhl2_em_enabled = true;

			// FD: TODO, need to send PPS if necessary
			// b. send PPS when request is completed successfully

			// Add for better stability per System
			mhl_tx_modify_reg(hw_context, REG_SRST, BIT_MHL_FIFO_RST_MASK, BIT_MHL_FIFO_RST_SET);
			mhl_tx_modify_reg(hw_context, REG_SRST, BIT_MHL_FIFO_RST_MASK, BIT_MHL_FIFO_RST_CLR);

			// Process both AVI infoframe and VSIF
			// FD: TBC, use process_info_frame_change after tests
//			hw_context->valid_avif = 1;
//			hw_context->valid_vsif = 0;
			start_video(hw_context,dev_context->edid_parser_context);
//			process_info_frame_change(hw_context, NULL, &(hw_context->current_avi_info_frame));

			process_status = true;
		}
		else if (MHL2_EM_SM_REQ_MODE_NORMAL == hw_context->mhl2_em_sm)
		{
			hw_context->mhl2_em_sm = MHL2_EM_SM_MODE_NORMAL;
			hw_context->mhl2_em_enabled = false;

			// Process both AVI infoframe and VSIF
			//process_info_frame_change(hw_context, &(hw_context->current_vs_info_frame), &(hw_context->current_avi_info_frame));

			process_status = true;
		}
		else
		{
			MHL_TX_DBG_INFO(hw_context, "Some error happens, should not be here.\n");
		}

		break;

	case MHL2_EM_RPL_SEND_PPS_FULL:
		MHL_TX_DBG_INFO(hw_context, "MHL2 EM Message MHL2_EM_RPL_SEND_PPS_FULL received.\n");

		// FD: TODO, verify the ACK & move the state machine forward
		hw_context->mhl2_em_sm = MHL2_EM_SM_PPS_SENT;

		process_status = true;
		break;

	case MHL2_EM_RPL_SEND_PPS_PART:
		MHL_TX_DBG_INFO(hw_context, "MHL2 EM Message MHL2_EM_RPL_SEND_PPS_PART received.\n");

		// FD: TODO, verify the ACK & move the state machine forward
		hw_context->mhl2_em_sm = MHL2_EM_SM_PPS_SENT;

		process_status = true;
		break;

	default:
		MHL_TX_DBG_INFO(hw_context, "Known Enhanced Mode Packet.\n");
		break;
	}

exit:
	return process_status;
}

// FD: TBI, may need re-try mechanism
static bool gen2_send(struct drv_hw_context *hw_context, uint8_t *packet, uint8_t len)
{
	bool status = false;
	uint8_t xfifo_stat = 0;
	struct mhl_dev_context	*dev_context;
	extern void init_cbus_queue(struct mhl_dev_context *dev_context);
	dev_context = container_of((void *)hw_context, struct mhl_dev_context, drv_context);

	MHL_TX_DBG_INFO(hw_context, "called.\n");
	// Send packet through HAWB
	init_cbus_queue(dev_context);
	enable_gen2_write_burst(hw_context);
	xfifo_stat = mhl_tx_read_reg(hw_context, REG_CBUS_MDT_XFIFO_STAT);
	if (0 != (xfifo_stat & BIT_CBUS_MDT_XFIFO_STAT_LEVEL_AVAIL_MASK))
	{
		// Reset the packet buffer
		memset(gen2_packet, 0, sizeof(gen2_packet) );

		// Fill the packet buffer
		gen2_packet[0] = len;
		memcpy(gen2_packet+1, packet, len);

		// Send through HAWB
		mhl_tx_write_reg_block(hw_context, REG_CBUS_MDT_XMIT_WRITE_PORT, GEN2_BUFFER_LENGTH, gen2_packet);

		// Successfully send the packet
		status = true;

		MHL_TX_DBG_INFO(hw_context, "HAWB packet is sent.\n");
	}
	else
	{
		MHL_TX_DBG_INFO(hw_context, "HAWB sending is not successful!\n");
	}

	return status;
}

static void mhl2_em_query(struct drv_hw_context *hw_context)
{
	uint8_t query_msg[MHL2_EM_CMD_QUERY_CAP_MSG_LEN] = {0};
	uint8_t query_pkt[MHL2_EM_PACKET_FRAME_LEN + MHL2_EM_CMD_QUERY_CAP_MSG_LEN] = {0};
	bool send_status = false;

	MHL_TX_DBG_INFO(hw_context, "called.\n");

	hw_context->mhl2_em_sm = MHL2_EM_SM_QUERY_CAP;

	// Prepare the Message
	query_msg[0] = MHL2_EM_CMD_QUERY_CAP;
	query_msg[1] = MHL2_EM_CMD_QUERY_CAP_MSG_LEN;	// ID, Length, Checksum, no Data
	query_msg[2] = calculate_generic_checksum(query_msg, 0, MHL2_EM_CMD_QUERY_CAP_MSG_LEN);

	// Prepare the Packet
	query_pkt[0] = MHL2_EM_PACKET_ADOPTER_ID_HIGH;
	query_pkt[1] = MHL2_EM_PACKET_ADOPTER_ID_LOW;
	query_pkt[2] = 0x01;				// sequence: 0, count: 1
	query_pkt[3] = MHL2_EM_PACKET_FRAME_LEN + MHL2_EM_CMD_QUERY_CAP_MSG_LEN;
	memcpy(query_pkt + MHL2_EM_PACKET_FRAME_LEN, query_msg, MHL2_EM_CMD_QUERY_CAP_MSG_LEN);
	query_pkt[4] = calculate_generic_checksum(query_pkt, 0, MHL2_EM_PACKET_FRAME_LEN + MHL2_EM_CMD_QUERY_CAP_MSG_LEN);

	// The Packet is small enough to be put in one single HAWB slot
	send_status = gen2_send(hw_context, query_pkt, MHL2_EM_PACKET_FRAME_LEN + MHL2_EM_CMD_QUERY_CAP_MSG_LEN);
	if (true != send_status)
	{
		MHL_TX_DBG_INFO(hw_context, "MHL2EM Query Message is not sent successfully!\n");
	}
}

void si_mhl_tx_em_query(struct mhl_dev_context *dev_context)
{
	struct drv_hw_context *hw_context = (struct drv_hw_context *)&dev_context->drv_context;
	mhl2_em_query(hw_context);
}

static void mhl2_em_request(struct drv_hw_context *hw_context, bool em)
{
	uint8_t i=0;
	uint8_t req_msg[MHL2_EM_CMD_REQ_MODE_MSG_LEN] = {0};
	uint8_t req_pkt[MHL2_EM_PACKET_FRAME_LEN + MHL2_EM_CMD_REQ_MODE_MSG_LEN] = {0};
	bool send_status = false;

	MHL_TX_DBG_INFO(hw_context, "called: %s\n", em ? "true":"false");

	// Prepare the Message - Specific Data
	if (true == em)
	{
		// Request to go to MHL2 Enhanced Mode
		hw_context->mhl2_em_sm = MHL2_EM_SM_REQ_MODE_EM;
		hw_context->mhl2_em_request_mode = MHL2_EM_MODE_ONE;
		if(video_data.inputVideoCode == HDMI_4k30_DSC)
			req_msg[3] = MHL2_EM_CMD_REQ_MODE_TWO;
		else
			req_msg[3] = MHL2_EM_CMD_REQ_MODE_ONE;
	}
	else
	{
		// Request to go back to Normal Mode from MHL2 Enhanced Mode
		hw_context->mhl2_em_sm = MHL2_EM_SM_REQ_MODE_NORMAL;
		hw_context->mhl2_em_request_mode = MHL2_EM_MODE_NORMAL;
		req_msg[3] = MHL2_EM_CMD_REQ_MODE_NORMAL;
	}

	// Prepare the Message - General Data
	req_msg[0] = MHL2_EM_CMD_REQ_MODE;
	req_msg[1] = MHL2_EM_CMD_REQ_MODE_MSG_LEN;	// ID, Length, Checksum, no Data
	req_msg[2] = calculate_generic_checksum(req_msg, 0, MHL2_EM_CMD_REQ_MODE_MSG_LEN);

	// Prepare the Packet
	req_pkt[0] = MHL2_EM_PACKET_ADOPTER_ID_HIGH;
	req_pkt[1] = MHL2_EM_PACKET_ADOPTER_ID_LOW;
	req_pkt[2] = 0x01;				// sequence: 0, count: 1
	req_pkt[3] = MHL2_EM_PACKET_FRAME_LEN + MHL2_EM_CMD_REQ_MODE_MSG_LEN;
	memcpy(req_pkt + MHL2_EM_PACKET_FRAME_LEN, req_msg, MHL2_EM_CMD_REQ_MODE_MSG_LEN);
	req_pkt[4] = calculate_generic_checksum(req_pkt, 0, MHL2_EM_PACKET_FRAME_LEN + MHL2_EM_CMD_REQ_MODE_MSG_LEN);
	for(i = 0; i <= 8; i++) {
		pr_info("0x%x @@", req_pkt[i]);
		if(i==8)
			pr_info("\n");
		}

	// The Packet is small enough to be put in one single HAWB slot
	send_status = gen2_send(hw_context, req_pkt, MHL2_EM_PACKET_FRAME_LEN + MHL2_EM_CMD_REQ_MODE_MSG_LEN);
	if (true != send_status)
	{
		MHL_TX_DBG_INFO(hw_context, "MHL2EM Request Message is not sent successfully!\n");
	}
}
#endif // MHL2_ENHANCED_MODE_SUPPORT

//void si_mhl_tx_drv_video_3d_update(struct drv_hw_context *hw_context, int video, int video_3d)
void si_mhl_tx_drv_video_3d(struct mhl_dev_context *dev_context, int video_3d)
{
	vendor_specific_info_frame_t vsif;
	struct drv_hw_context *hw_context = (struct drv_hw_context *)&dev_context->drv_context;

    	MHL_TX_DBG_INFO((struct drv_hw_context *) (&dev_context->drv_context), "Input Timing Update: video 3D configuration changed %d\n", video_3d);
		
	//hw_context->mhl2_em_input = false;
#ifdef MHL2_ENHANCED_MODE_SUPPORT	
	if ((MHL2_EM_SM_CAP_QUERIED <= hw_context->mhl2_em_sm)&&(video_data.inputVideoCode > 90))
		mhl2_em_request(hw_context, true);
	else if(0x30 <= si_get_peer_mhl_version(dev_context))
		mhl2_em_request(hw_context, false);
#endif
	memset( &vsif, 0, sizeof(vendor_specific_info_frame_t) );

	fill_vsif( (struct drv_hw_context *) (&dev_context->drv_context), (uint8_t *)(&vsif), video_3d );	// Fill VISF

	// Process VSIF only
	process_info_frame_change((struct drv_hw_context *) (&dev_context->drv_context),&vsif,NULL);

	//fill_video_lcd( hw_context, video, video_3d );

	//set_pin(hw_context, VID_CTRL_ACK, 1);  // tell to allow further VID_CTRL_INT
}

#if 0
struct timer_list restart_timer;
struct drv_hw_context *temp_hw_context = NULL;
void __restart_timer_isr(unsigned long n)
{
    unmute_video(temp_hw_context);
    del_timer(&restart_timer);
}
#endif

extern enum HDMI_CABLE_TYPE MHL_Connect_type;
extern enum HDMI_STATE hdmi_drv_get_state(void);
void si_mhl_tx_drv_video_3d_update(struct mhl_dev_context *dev_context, int video_3d)
{
    vendor_specific_info_frame_t vsif;	
    struct drv_hw_context *hw_context = (struct drv_hw_context *)&dev_context->drv_context;
    static unsigned long long old_time = 0;

    MHL_TX_DBG_INFO((struct drv_hw_context *) (&dev_context->drv_context), "Input Timing Update to: video 3D configuration changed-%d, cable type: %d\n", video_3d, MHL_Connect_type);
    if(MHL_Connect_type != MHL_3D_GLASSES)
        stop_video(hw_context);
    
	memset( &vsif, 0, sizeof(vendor_specific_info_frame_t) );

	fill_vsif( (struct drv_hw_context *) (&dev_context->drv_context), (uint8_t *)(&vsif), video_3d );	// Fill VISF

	// Process VSIF only
	process_info_frame_change((struct drv_hw_context *) (&dev_context->drv_context),&vsif,NULL);

	/*
		 * Send VSIF out
		 */
		///if ( 1 == hw_context->valid_vsif && 1 == hw_context->valid_3d )
		{
			MHL_TX_DBG_INFO((struct drv_hw_context *) (&dev_context->drv_context), "Send VSIF out...\n");
			mhl_tx_write_reg((struct drv_hw_context *) (&dev_context->drv_context), REG_TPI_INFO_FSEL, BIT_TPI_INFO_EN | BIT_TPI_INFO_RPT | BIT_TPI_INFO_SEL_3D_VSIF);	// Send 3D VSIF repeatly
			///DumpIncomingInfoFrame(&((struct drv_hw_context *) (&dev_context->drv_context)->current_vs_info_frame),sizeof((struct drv_hw_context *) (&dev_context->drv_context)->current_vs_info_frame));
			///mhl_tx_write_reg_block((struct drv_hw_context *) (&dev_context->drv_context), REG_TPI_INFO_BYTE00, 8, (u8 *)(&((struct drv_hw_context *) (&dev_context->drv_context)->current_vs_info_frame)) );	// only 8 bytes are valid for MHL VSIF
            DumpIncomingInfoFrame(&(hw_context->current_vs_info_frame),sizeof(hw_context->current_vs_info_frame));
			mhl_tx_write_reg_block((struct drv_hw_context *) (&dev_context->drv_context), REG_TPI_INFO_BYTE00, 8, (u8 *)(&(hw_context->current_vs_info_frame)) );	// only 8 bytes are valid for MHL VSIF
			mhl_tx_write_reg((struct drv_hw_context *) (&dev_context->drv_context), REG_TPI_INFO_BYTE30, 0x00);// Trigger the infoframe sending
		}

#if 0    
    restart_timer.expires = jiffies + msecs_to_jiffies(500);
    restart_timer.function = __restart_timer_isr;
    init_timer(&restart_timer);
    add_timer(&restart_timer);
#else
    if(MHL_Connect_type != MHL_3D_GLASSES)
    {
        msleep(120);
        if(hdmi_drv_get_state()==HDMI_STATE_ACTIVE) {
		/*for some tv can't switch time in short time, so add delay time */
		if(sched_clock() - old_time < 2000000000)
			msleep(2500 -(sched_clock() - old_time)/1000000);

		unmute_video(hw_context);
	}
	old_time = sched_clock();
    }  
    
    MHL_TX_DBG_INFO((struct drv_hw_context *) (&dev_context->drv_context), "Input Timing Update to: video 3D configuration changed done\n");
#endif
}

