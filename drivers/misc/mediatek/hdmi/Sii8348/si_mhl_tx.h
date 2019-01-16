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
   @file si_mhl_tx.h
*/

#include "sii_hal.h"

#define SCRATCHPAD_SIZE 16
typedef union
{
    Mhl2VideoFormatData_t   videoFormatData;
    uint8_t     			asBytes[SCRATCHPAD_SIZE];
#ifdef MEDIA_DATA_TUNNEL_SUPPORT //(	
	mdt_burst_01_t			mdtPackets;
#endif //)
}ScratchPad_u,*PScratchPad_u;


typedef enum
{
    qs_auto_select_by_color_space = 0
    ,qs_full_range                = 1
    ,qs_limited_range             = 2
    ,qs_reserved                  = 3
}quantization_settings_e;


typedef struct 
{
    unsigned FLAGS_SCRATCHPAD_BUSY			:1;
    unsigned FLAGS_REQ_WRT_PENDING			:1;
    unsigned FLAGS_WRITE_BURST_PENDING		:1;
    unsigned FLAGS_RCP_READY				:1;

    unsigned FLAGS_HAVE_DEV_CATEGORY		:1;
    unsigned FLAGS_HAVE_DEV_FEATURE_FLAGS	:1;
    unsigned FLAGS_HAVE_COMPLETE_DEVCAP		:1;
    unsigned FLAGS_SENT_DCAP_RDY			:1;

    unsigned FLAGS_SENT_PATH_EN				:1;
    unsigned FLAGS_SENT_3D_REQ				:1;
    unsigned FLAGS_BURST_3D_VIC_DONE		:1;
    unsigned FLAGS_BURST_3D_DTD_DONE		:1;

    unsigned FLAGS_BURST_3D_DTD_VESA_DONE	:1;
    unsigned FLAGS_BURST_3D_DONE			:1;
    unsigned FLAGS_EDID_READ_DONE			:1;
	unsigned RAP_CONTENT_ON 				:1;

	unsigned RAP_STATUS						:1;
    unsigned MHL_HPD	:1;
    unsigned MHL_RSEN	:1;

	unsigned reserved	:13;
}MiscFlags_t;
//
// structure to hold operating information of MhlTx component
//
typedef struct
{
	void		*device_context;

	uint8_t		status_0;			// Received status from peer is stored here
	uint8_t		status_1;			// Received status from peer is stored here

	uint8_t     connected_ready;     // local MHL CONNECTED_RDY register value
	uint8_t     link_mode;           // local MHL LINK_MODE register value

	bool_t		mhl_connection_event;
	uint8_t		mhl_connected;

	// msc_msg_arrived == true when a MSC MSG arrives, false when it has been picked up
	bool_t		msc_msg_arrived;
	uint8_t		msc_msg_sub_command;
	uint8_t		msc_msg_data;

	uint8_t     cbus_reference_count;  // keep track of CBUS requests
	// Remember last command, offset that was sent.
	// Mostly for READ_DEVCAP command and other non-MSC_MSG commands
	uint8_t		msc_last_command;
	uint8_t		msc_last_offset;
	uint8_t		msc_last_data;

	// Remember last MSC_MSG command (RCPE particularly)
	uint8_t		msc_msg_last_command;
	uint8_t		msc_msg_last_data;
	uint8_t		msc_save_rcp_key_code;

	//  support WRITE_BURST
	ScratchPad_u    incoming_scratch_pad;
	ScratchPad_u    outgoing_scratch_pad;
	uint8_t     burst_entry_count_3d_vic;
	uint8_t     vic_2d_index;
	uint8_t     vic_3d_index;
	uint8_t     burst_entry_count_3d_dtd;
	uint8_t     vesa_dtd_index;
	uint8_t     cea_861_dtd_index;
	union
	{
		MiscFlags_t	as_flags;          // such as SCRATCHPAD_BUSY
		uint32_t	as_integer;
	}misc_flags_u;

	uint8_t		preferred_clk_mode;

} mhlTx_config_t;

typedef enum
{
    gebSuccess =0
    ,gebAcquisitionFailed
    ,gebReleaseFailed
    ,gebTimedOut
}si_mhl_tx_drv_get_edid_block_result_e;
uint16_t si_mhl_tx_drv_get_incoming_horizontal_total(void);
uint16_t si_mhl_tx_drv_get_incoming_vertical_total(void);

extern mhlTx_config_t	mhlTxConfig;
#ifdef ENABLE_COLOR_SPACE_DEBUG_PRINT //(

void print_color_settings_impl(char *pszId,int iLine);
#define print_color_settings(id,line) print_color_settings_impl(id,line);

#else //)(

#define print_color_settings(id,line)

#endif //)
void si_mhl_tx_drv_set_output_color_space_impl(uint8_t  outputClrSpc);
void si_mhl_tx_drv_set_input_color_space_impl(uint8_t inputClrSpc);
#define PackedPixelAvailable ((MHL_DEV_VID_LINK_SUPP_PPIXEL & mhlTxConfig.devcap_cache[DEVCAP_OFFSET_VID_LINK_MODE]) && (MHL_DEV_VID_LINK_SUPP_PPIXEL & DEVCAP_VAL_VID_LINK_MODE) )
#define si_mhl_tx_drv_set_output_color_space(outputClrSpc) {si_mhl_tx_drv_set_output_color_space_impl(outputClrSpc);print_color_settings(__FILE__" si_mhl_tx_drv_set_output_color_space",__LINE__)}
#define si_mhl_tx_drv_set_input_color_space(inputClrSpc)   {si_mhl_tx_drv_set_input_color_space_impl(inputClrSpc);  print_color_settings(__FILE__" si_mhl_tx_drv_set_input_color_space",__LINE__)}

#define SetMiscFlag(func,x) { mhlTxConfig.misc_flags_u.as_flags.x=1; TX_DEBUG_PRINT(("mhl_tx:%s set %s\n",#func,#x)); }
#define ClrMiscFlag(func,x) { mhlTxConfig.misc_flags_u.as_flags.x=0; TX_DEBUG_PRINT(("mhl_tx:%s clr %s\n",#func,#x)); }
#define TestMiscFlag(x) (mhlTxConfig.misc_flags_u.as_flags.x)

void si_mhl_tx_drv_set_upstream_edid(uint8_t *pEDID,uint16_t length);
void si_mhl_tx_tmds_enable(void);
si_mhl_tx_drv_get_edid_block_result_e si_mhl_tx_drv_get_edid_block(uint8_t *p_buf_edid,uint8_t blockNumber,uint8_t blockSize);
bool_t si_mhl_tx_set_int( uint8_t regToWrite,uint8_t  mask, uint8_t priorityLevel );

