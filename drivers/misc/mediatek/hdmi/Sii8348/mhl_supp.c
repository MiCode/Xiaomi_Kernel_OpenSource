/*

SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc
.
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             

*/

#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>

#include "sii_hal.h"
#include "si_fw_macros.h"
#include "si_mhl_defs.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include "si_mdt_inputdev.h"
#endif
#include "mhl_linux_tx.h"
#include "mhl_supp.h"
#include "si_infoframe.h"
#include "si_app_devcap.h"
#include "platform.h"
#include "si_8348_drv.h"
#include "hdmi_drv.h"
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
#include "smartbook.h"
#endif
int si_mhl_tx_post_initialize(struct mhl_dev_context *dev_context, bool bootup);

static void cbus_abort_timer_callback(void *callback_param);

bool si_mhl_tx_ucpk_send(struct mhl_dev_context *dev_context,
						 uint8_t ucp_key_code);


#define	MHL_DEV_LD_DISPLAY					(0x01 << 0)
#define	MHL_DEV_LD_VIDEO					(0x01 << 1)
#define	MHL_DEV_LD_AUDIO					(0x01 << 2)
#define	MHL_DEV_LD_MEDIA					(0x01 << 3)
#define	MHL_DEV_LD_TUNER					(0x01 << 4)
#define	MHL_DEV_LD_RECORD					(0x01 << 5)
#define	MHL_DEV_LD_SPEAKER					(0x01 << 6)
#define	MHL_DEV_LD_GUI						(0x01 << 7)

#define	MHL_LOGICAL_DEVICE_MAP	(MHL_DEV_LD_AUDIO | MHL_DEV_LD_VIDEO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_GUI)
#define	MHL_MAX_RCP_KEY_CODE	(0x7F + 1)	// inclusive

uint8_t rcpSupportTable [MHL_MAX_RCP_KEY_CODE] = {
	(MHL_DEV_LD_GUI),		// 0x00 = Select
	(MHL_DEV_LD_GUI),		// 0x01 = Up
	(MHL_DEV_LD_GUI),		// 0x02 = Down
	(MHL_DEV_LD_GUI),		// 0x03 = Left
	(MHL_DEV_LD_GUI),		// 0x04 = Right
	0, 0, 0, 0,				// 05-08 Reserved
	(MHL_DEV_LD_GUI),		// 0x09 = Root Menu
	0, 0, 0,				// 0A-0C Reserved
	(MHL_DEV_LD_GUI),		// 0x0D = Select
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0E-1F Reserved
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Numeric keys 0x20-0x29
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,						// 0x2A = Dot
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Enter key = 0x2B
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Clear key = 0x2C
	0, 0, 0,				// 2D-2F Reserved
	(MHL_DEV_LD_TUNER),		// 0x30 = Channel Up
	(MHL_DEV_LD_TUNER),		// 0x31 = Channel Dn
	(MHL_DEV_LD_TUNER),		// 0x32 = Previous Channel
	(MHL_DEV_LD_AUDIO),		// 0x33 = Sound Select
	0,						// 0x34 = Input Select
	0,						// 0x35 = Show Information
	0,						// 0x36 = Help
	0,						// 0x37 = Page Up
	0,						// 0x38 = Page Down
	0, 0, 0, 0, 0, 0, 0,	// 0x39-0x3F Reserved
	0,						// 0x40 = Undefined

	(MHL_DEV_LD_SPEAKER),	// 0x41 = Volume Up
	(MHL_DEV_LD_SPEAKER),	// 0x42 = Volume Down
	(MHL_DEV_LD_SPEAKER),	// 0x43 = Mute
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x44 = Play
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x45 = Stop
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x46 = Pause
	(MHL_DEV_LD_RECORD),	// 0x47 = Record
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x48 = Rewind
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x49 = Fast Forward
	(MHL_DEV_LD_MEDIA),		// 0x4A = Eject
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA),	// 0x4B = Forward
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA),	// 0x4C = Backward
	0, 0, 0,				// 4D-4F Reserved
	0,						// 0x50 = Angle
	0,						// 0x51 = Subpicture
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 52-5F Reserved
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x60 = Play Function
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x61 = Pause the Play Function
	(MHL_DEV_LD_RECORD),	// 0x62 = Record Function
	(MHL_DEV_LD_RECORD),	// 0x63 = Pause the Record Function
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x64 = Stop Function

	(MHL_DEV_LD_SPEAKER),	// 0x65 = Mute Function
	(MHL_DEV_LD_SPEAKER),	// 0x66 = Restore Mute Function
	0, 0, 0, 0, 0, 0, 0, 0, 0, 	                        // 0x67-0x6F Undefined or reserved
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 		// 0x70-0x7F Undefined or reserved
};


/*
   @file si_mhl_tx.c
*/
//#include "si_mhl_defs.h"
//#include "si_mhl_tx_api.h"
//#include "si_mhl_tx.h"

//#include "si_drv_mhl_tx.h"  // exported stuff from the driver
//#include "si_hdmi_tx_lite_api.h"

struct mhl_dev_context *get_mhl_device_context(void *context)
{
	struct mhl_dev_context *dev_context = context;

	if (dev_context->signature != MHL_DEV_CONTEXT_SIGNATURE)
		dev_context = container_of(context,
								   struct mhl_dev_context,
								   drv_context);
	return dev_context;
}

void init_cbus_queue(struct mhl_dev_context *dev_context)
{
	struct cbus_req	*entry;
	int				idx;

	INIT_LIST_HEAD(&dev_context->cbus_queue);
	INIT_LIST_HEAD(&dev_context->cbus_free_list);

	dev_context->current_cbus_req = NULL;

	/* Place pre-allocated CBUS queue entries on the free list */
	for (idx = 0; idx < NUM_CBUS_EVENT_QUEUE_EVENTS; idx++) {

		entry = &dev_context->cbus_req_entries[idx];
		memset(entry, 0, sizeof(struct cbus_req));
		list_add(&entry->link, &dev_context->cbus_free_list);
	}
}

static void return_cbus_queue_entry(struct mhl_dev_context *dev_context,
							 struct cbus_req *pReq)
{
	list_add(&pReq->link, &dev_context->cbus_free_list);

}
static struct cbus_req* get_free_cbus_queue_entry(struct mhl_dev_context *dev_context)
{
	struct cbus_req		*req;
	struct list_head	*entry;

	if (list_empty(&dev_context->cbus_free_list)) {
		MHL_TX_DBG_ERR(dev_context, "No free cbus queue entries available and add one\n");
		 si_mhl_tx_post_initialize(dev_context, false);
		return NULL;
		//return_cbus_queue_entry(dev_context, req);
	}

	entry = dev_context->cbus_free_list.next;
	list_del(entry);
	req = list_entry(entry, struct cbus_req, link);

	/* Start clean */
	req->status.flags.cancel = 0;
	return req;
}


void queue_cbus_transaction(struct mhl_dev_context *dev_context,
							struct cbus_req *pReq)
{
	MHL_TX_DBG_INFO(dev_context, "0x%02x 0x%02x 0x%02x\n",
			 pReq->command,
			 (MHL_MSC_MSG == pReq->command)?
					 pReq->msg_data[0]:pReq->reg,
			 (MHL_MSC_MSG == pReq->command)?
					 pReq->msg_data[1]:pReq->reg_data);

	list_add_tail(&pReq->link, &dev_context->cbus_queue);
}

void queue_priority_cbus_transaction(struct mhl_dev_context *dev_context,
									 struct cbus_req *req)
{
	MHL_TX_DBG_INFO(dev_context, "0x%02x 0x%02x 0x%02x\n",
			 req->command,
			 (MHL_MSC_MSG == req->command)?
					 req->msg_data[0] : req->reg,
			 (MHL_MSC_MSG == req->command)?
					 req->msg_data[1] : req->reg_data);

	list_add(&req->link, &dev_context->cbus_queue);
}

struct cbus_req *get_next_cbus_transaction(struct mhl_dev_context *dev_context)
{
	struct cbus_req		*req = NULL;
	struct list_head	*entry;

	if (list_empty(&dev_context->cbus_queue)) {
//		MHL_TX_DBG_INFO(dev_context, "Queue empty\n");	// TODO: FD, TBD
		return NULL;
	}

	entry = dev_context->cbus_queue.next;
	if (entry) {
		list_del(entry);
		req = list_entry(entry, struct cbus_req, link);

		if(req) {
//			MHL_TX_DBG_INFO(dev_context, "0x%02x 0x%02x 0x%02x\n",
//				 req->command,
//			 	(MHL_MSC_MSG == req->command)?
//						 req->msg_data[0] : req->reg,
//			 	(MHL_MSC_MSG == req->command)?
//						 req->msg_data[1] : req->reg_data);
		}
	}
	return req;
}

uint8_t calculate_generic_checksum(uint8_t *info_frame_data, uint8_t checksum, uint8_t length)
{
	uint8_t	i;

	for (i = 0; i < length; i++)
		checksum += info_frame_data[i];

	checksum = 0x100 - checksum;

	return checksum;
}
#ifdef	EXAMPLE_ONLY	// This function is not called from anywhere.
int8_t avi_info_frame_cmp(avi_info_frame_t *p0, avi_info_frame_t *p1)
{
	uint8_t i;
	uint8_t ret_val=0;
	uint8_t *puc0,*puc1;
	uint8_t temp0, temp1;

	puc0 = (uint8_t *)p0;
	puc1 = (uint8_t *)p1;
	for (i = 0; i < sizeof(*p0); ++i) {
		temp0 = *puc0++;
		temp1 = *puc1++;
		if (temp0 == temp1)
            continue;

		if (temp0 < temp1) {
			ret_val = -1;
		} else {
			ret_val = 1;
		}
		break;
	}
	return ret_val;
}
#endif	//	EXAMPLE_ONLY	// This function is not called from anywhere.
/*
 * si_mhl_tx_set_status
 *
 * Set MHL defined STATUS bits in peer's register set.
 *
 * register	    MHL register to write
 * value        data to write to the register
 */
bool si_mhl_tx_set_status(struct mhl_dev_context *dev_context, uint8_t reg_to_write, uint8_t value, uint8_t priority_level)
{
	struct cbus_req		*req;

	MHL_TX_DBG_INFO(dev_context, "called\n");

	req = get_free_cbus_queue_entry(dev_context);
	if(req == NULL) {
		dev_err(dev_context->mhl_dev, "si_mhl_tx_set_status:CBUS free queue exhausted\n");
		return false;
	}

	req->retry_count			= 2;
	req->command				= MHL_WRITE_STAT;
	req->reg					= reg_to_write;
	req->reg_data				= value;
	//	req->offset_data			= regToWrite;
	//	req->payload_u.msg_data[0]	= value;

	if(priority_level)
		queue_cbus_transaction(dev_context, req);
	else
		queue_priority_cbus_transaction(dev_context, req);

	return true;
}

/*
 * si_mhl_tx_set_int
 * Set MHL defined INTERRUPT bits in peer's register set.
 * This function returns true if operation was successfully performed.
 *
 *  regToWrite      Remote interrupt register to write
 *  mask            the bits to write to that register
 *
 *  priority        0:  add to head of CBusQueue
 *                  1:  add to tail of CBusQueue
 */
bool si_mhl_tx_set_int(struct mhl_dev_context *dev_context,
					   uint8_t reg_to_write, uint8_t  mask,
					   uint8_t priority_level)
{
	struct cbus_req		*req;

    req = get_free_cbus_queue_entry(dev_context);
    if(req == NULL) {
    	MHL_TX_DBG_ERR(dev_context, "si_mhl_tx_set_int:CBUS free queue exhausted\n");
    	return false;
    }

    req->retry_count			= 2;
	req->command				= MHL_SET_INT;
	req->reg					= reg_to_write;
	req->reg_data				= mask;
//	req->offset_data			= reg_to_write;
//	req->payload_u.msg_data[0]	= mask;

	if(priority_level)
		queue_cbus_transaction(dev_context, req);
	else
		queue_priority_cbus_transaction(dev_context, req);

    return true;
}

//bool si_mhl_tx_do_write_burst(struct mhl_dev_context *dev_context,
//							  uint8_t start_reg, uint8_t *data,
//							  uint8_t length)
//{
//	if (dev_context->misc_flags_u.as_flags.FLAGS_WRITE_BURST_PENDING) {
//
//		struct cbus_req		*req;
//
//		MHL_TX_DBG_INFO(dev_context, "startReg:%d length:%d\n",
//						(int)start_reg, (int)length);
//
//	    req = get_free_cbus_queue_entry(dev_context);
//	    if(req == NULL) {
//	    	MHL_TX_DBG_ERR(dev_context, "CBUS free queue exhausted\n");
//	    	return false;
//	    }
//
//	    req->retry_count				= 1;
//	    req->command					= MHL_WRITE_BURST;
//	    req->length						= length;
//	    req->offset_data				= start_reg;
//	    memcpy(req->payload_u.msg_data, data, length);
//
//		queue_priority_cbus_transaction(dev_context, req);
//
//		dev_context->misc_flags_u.as_flags.FLAGS_WRITE_BURST_PENDING = false;
//		return true;
//	}
//	return false;
//}


void	si_mhl_tx_reset_states(struct mhl_dev_context *dev_context)
{
	init_cbus_queue(dev_context);

	dev_context->mhl_connection_event	= false;
	dev_context->mhl_connected		= MHL_TX_EVENT_DISCONNECTION;

	dev_context->msc_msg_arrived		= false;
	dev_context->status_0            	= 0;
	dev_context->status_1            	= 0;
	dev_context->link_mode            	= MHL_STATUS_CLK_MODE_NORMAL; // indicate normal (24-bit) mode
	dev_context->preferred_clk_mode		= MHL_STATUS_CLK_MODE_NORMAL;  // this can be overridden by the application calling si_mhl_tx_set_preferred_pixel_format()
	dev_context->misc_flags.as_uint32	= 0;

	#ifdef MEDIA_DATA_TUNNEL_SUPPORT	
	memset(dev_context->mdt_devs.is_dev_registered, INPUT_WAITING_FOR_REGISTRATION, MDT_TYPE_COUNT);
	dev_context->mdt_devs.x_max 		= X_MAX;
 	dev_context->mdt_devs.x_screen		= SCALE_X_SCREEN;
	dev_context->mdt_devs.x_raw 		= SCALE_X_RAW;
	dev_context->mdt_devs.x_shift 		= X_SHIFT;
	dev_context->mdt_devs.y_max 		= Y_MAX;
	dev_context->mdt_devs.y_screen 		= SCALE_Y_SCREEN;
	dev_context->mdt_devs.y_raw 		= SCALE_Y_RAW;
	dev_context->mdt_devs.y_shift		= Y_SHIFT;
	dev_context->mdt_devs.swap_xy		= SWAP_XY;
	dev_context->mdt_devs.swap_updown	= SWAP_UPDOWN;
	dev_context->mdt_devs.swap_leftright= SWAP_LEFTRIGHT;
	#endif
	
	memset(&dev_context->dev_cap_cache
		,0
		,sizeof(dev_context->dev_cap_cache)
		);

	dev_context->scratch_pad_read_done = true;	// no 'blocking' by APP level at startup
}

void cbus_DPI_timer_callback(void *callback_param);
int si_mhl_tx_initialize(struct mhl_dev_context *dev_context, bool bootup)
{
	int	ret;

	MHL_TX_DBG_INFO(dev_context, "called\n");
    ///if(bootup)
	{
    	ret = mhl_tx_create_timer(dev_context, cbus_abort_timer_callback,
    			dev_context, &dev_context->cbus_abort_timer);
    	if (ret != 0) {
    		MHL_TX_DBG_ERR(dev_context, "Failed to allocate CBUS abort timer!\n");
    		return ret;
    	}
    	ret = mhl_tx_create_timer(dev_context, cbus_DPI_timer_callback,
    			dev_context, &dev_context->cbus_dpi_timer);
    	if (ret != 0) {
    		MHL_TX_DBG_ERR(dev_context, "Failed to allocate CBUS dpi timer!\n");
    		return ret;
    	}
    }

    ///return 0;
    
	si_mhl_tx_reset_states(dev_context);

	return dev_context->drv_info->mhl_device_initialize(
			(struct drv_hw_context *)(&dev_context->drv_context));
}

int si_mhl_tx_post_initialize(struct mhl_dev_context *dev_context, bool bootup)
{
	MHL_TX_DBG_INFO(dev_context, "called\n");

	si_mhl_tx_reset_states(dev_context);

	return dev_context->drv_info->mhl_device_initialize(
			(struct drv_hw_context *)(&dev_context->drv_context));
}


static void cbus_abort_timer_callback(void *callback_param)
{
	struct mhl_dev_context *dev_context = callback_param;

	MHL_TX_DBG_INFO(dev_context, "CBUS abort timer expired, " \
					"enable CBUS messaging\n");
	dev_context->misc_flags.flags.cbus_abort_delay_active = false;
	si_mhl_tx_drive_states(dev_context);
}

void process_cbus_abort(struct mhl_dev_context *dev_context)
{
	/* Delay the sending of any new CBUS messages for 2 seconds */
	dev_context->misc_flags.flags.cbus_abort_delay_active = true;

	mhl_tx_start_timer(dev_context, dev_context->cbus_abort_timer, 2000);
}

//static bool MHL_connect_state=false;

void cbus_DPI_timer_callback(void *callback_param)
{
	struct mhl_dev_context *dev_context = callback_param;

	MHL_TX_DBG_INFO(dev_context, "CBUS DPI timer expired\n");
	if(!(dev_context->intr_info.flags & DRV_INTR_FLAG_DISCONNECT)){
	    mhl_event_notify(dev_context, MHL_TX_EVENT_EDID_DONE, 0, NULL);
	    }
}
//if MHL can't work well and in conenct state, after DPI timer, DPI will send out the video to 8348
void process_dpi(struct mhl_dev_context *dev_context)
{
    MHL_TX_DBG_INFO(dev_context, "start DPI timer= 3s\n");
	mhl_tx_start_timer(dev_context, dev_context->cbus_dpi_timer, 4000);
}

#ifdef DEBUG //(
static char *get_cbus_command_string(int command)
{
#define CBUS_COMMAND_CASE(command) case command: return #command;
	switch(command){
	CBUS_COMMAND_CASE(MHL_ACK)
	CBUS_COMMAND_CASE(MHL_NACK)
	CBUS_COMMAND_CASE(MHL_ABORT)
	CBUS_COMMAND_CASE(MHL_WRITE_STAT)
	CBUS_COMMAND_CASE(MHL_SET_INT)
	CBUS_COMMAND_CASE(MHL_READ_DEVCAP)
	CBUS_COMMAND_CASE(MHL_GET_STATE)
	CBUS_COMMAND_CASE(MHL_GET_VENDOR_ID)
	CBUS_COMMAND_CASE(MHL_SET_HPD)
	CBUS_COMMAND_CASE(MHL_CLR_HPD)
	CBUS_COMMAND_CASE(MHL_SET_CAP_ID)
	CBUS_COMMAND_CASE(MHL_GET_CAP_ID)
	CBUS_COMMAND_CASE(MHL_MSC_MSG)
	CBUS_COMMAND_CASE(MHL_GET_SC1_ERRORCODE)
	CBUS_COMMAND_CASE(MHL_GET_DDC_ERRORCODE)
	CBUS_COMMAND_CASE(MHL_GET_MSC_ERRORCODE)
	CBUS_COMMAND_CASE(MHL_WRITE_BURST)
	CBUS_COMMAND_CASE(MHL_GET_SC3_ERRORCODE)
	CBUS_COMMAND_CASE(MHL_READ_EDID_BLOCK)
	}
	return "unknown";
}
#endif //)
/*
 * si_mhl_tx_drive_states
 *
 * This function is called by the interrupt handler in the driver layer.
 * to move the MSC engine to do the next thing before allowing the application
 * to run RCP APIs.
 */
void si_mhl_tx_drive_states(struct mhl_dev_context *dev_context)
{
	struct cbus_req		*req;

	///MHL_TX_DBG_INFO(dev_context, "called\n");

	if (dev_context->misc_flags.flags.cbus_abort_delay_active) {
		MHL_TX_DBG_INFO(dev_context, "CBUS abort delay in progress "\
						"can't send any messages\n");
		return;
	}

	if (dev_context->current_cbus_req != NULL) {
		MHL_TX_DBG_INFO(dev_context, "CBUS request:%s in progress\n"
			,get_cbus_command_string(dev_context->current_cbus_req->command));
		return;
	}

	/* process queued CBus transactions */
	req = get_next_cbus_transaction(dev_context);
	if (req == NULL) {
		return;
	}

	MHL_TX_DBG_INFO(dev_context, "req: %p\n",req);
	/* coordinate write burst requests and grants. */
	if (MHL_SET_INT == req->command) {
		if (MHL_RCHANGE_INT == req->reg) {
			// Do not allow to proceed another round of write_burst until the current round is handled by APP level
			if (dev_context->misc_flags.flags.scratchpad_busy || !dev_context->scratch_pad_read_done) {
				if (MHL_INT_REQ_WRT == req->reg_data) {
					/*
					 * Can't handle this request right now so just push it
					 * back onto the front of the queue.
					 */
					queue_priority_cbus_transaction(dev_context, req);
					req = NULL;
					MHL_TX_DBG_INFO(dev_context, "req: %p\n",req);
				}
			} else {
				if (MHL_INT_REQ_WRT == req->reg_data) {
					dev_context->misc_flags.flags.scratchpad_busy = true;
					dev_context->misc_flags.flags.write_burst_pending = true;

				} else if (MHL_INT_GRT_WRT == req->reg_data) {
					dev_context->misc_flags.flags.scratchpad_busy = true;
				}
			}
		}
	} else if (MHL_MSC_MSG == req->command) {
			dev_context->msc_msg_last_data = req->msg_data[1];

	} else if (MHL_WRITE_BURST == req->command) {
		if (dev_context->misc_flags.flags.write_burst_pending) {
			/* Still waiting for write burst grant */
			req = NULL;
			MHL_TX_DBG_INFO(dev_context, "req: %p\n",req);
		}
	}

	MHL_TX_DBG_INFO(dev_context, "req: %p\n",req);
	if (req) {
		bool success;
		dev_context->current_cbus_req = req;
		success =
		si_mhl_tx_drv_send_cbus_command((struct drv_hw_context *)
				(&dev_context->drv_context),
				req);
		if (!success) {
			return_cbus_queue_entry(dev_context, req);
			dev_context->current_cbus_req = NULL;
			if (MHL_READ_EDID_BLOCK == req->command) {
				dev_context->misc_flags.flags.edid_loop_active = 0;
				MHL_TX_DBG_INFO(dev_context, "tag: EDID active: %d\n"
						,dev_context->misc_flags.flags.edid_loop_active);
			}
		}
	}
}

enum scratch_pad_status si_mhl_tx_request_write_burst(
		struct mhl_dev_context *dev_context, uint8_t reg_offset,
		uint8_t length, uint8_t *data)
{
	struct cbus_req		*req;
	enum scratch_pad_status status = SCRATCHPAD_BUSY;

	if (!(dev_context->dev_cap_cache.mdc.featureFlag
		& MHL_FEATURE_SP_SUPPORT)) {
		MHL_TX_DBG_ERR(dev_context, "failed SCRATCHPAD_NOT_SUPPORTED\n");
		status = SCRATCHPAD_NOT_SUPPORTED;

	} else if ((reg_offset + length) > SCRATCHPAD_SIZE) {
		MHL_TX_DBG_ERR(dev_context, "invalid offset + length\n");
		status = SCRATCHPAD_BAD_PARAM;

	} else {

		req = get_free_cbus_queue_entry(dev_context);
		if (req == NULL) {
			status = SCRATCHPAD_FAIL;
			goto err_exit;
		}

		memcpy(req->msg_data, data, length);

		req->retry_count = 2;
		req->command = MHL_WRITE_BURST;
		req->reg = MHL_RCHANGE_INT;
		req->reg_data = MHL_INT_REQ_WRT;
		req->offset = reg_offset;
		req->length = length;

		queue_priority_cbus_transaction(dev_context, req);

		MHL_TX_DBG_INFO(dev_context, "request accepted\n");

		si_mhl_tx_drive_states(dev_context);
		status = SCRATCHPAD_SUCCESS;
	}

err_exit:
	return status;
}

/*
 * si_mhl_tx_send_msc_msg
 *
 * This function sends a MSC_MSG command to the peer.
 * It returns true if successful in doing so.
 */
static bool si_mhl_tx_send_msc_msg(struct mhl_dev_context *dev_context,
								   uint8_t command, uint8_t cmdData)
{
	struct cbus_req		*req;

	MHL_TX_DBG_INFO(dev_context, "called\n");

	req = get_free_cbus_queue_entry(dev_context);
	if (req == NULL) {
		dev_err(dev_context->mhl_dev, "si_mhl_tx_send_msc_msg:CBUS free queue exhausted\n");
		return false;
	}

	req->retry_count	= 2;
	req->command		= MHL_MSC_MSG;
//	req->payload_u.msg_data[0]  = command;
//	req->payload_u.msg_data[1]  = cmdData;
	req->msg_data[0]	= command;
	req->msg_data[1]	= cmdData;

	queue_cbus_transaction(dev_context, req);

    return true;
}

/*
 * si_mhl_rapk_send
 * This function sends RAPK to the peer device.
 */
static bool si_mhl_rapk_send(struct mhl_dev_context *dev_context,
							 uint8_t status)
{
	return (si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RAPK, status));
}

/*
 * si_mhl_tx_rcpe_send
 *
 * The function will return a value of true if it could successfully send the RCPE
 * subcommand. Otherwise false.
 *
 * When successful, mhl_tx internally sends RCPK with original (last known)
 * keycode.
 */
bool si_mhl_tx_rcpe_send(struct mhl_dev_context *dev_context, uint8_t rcpe_error_code)
{
	bool	status;

	MHL_TX_DBG_INFO(dev_context, "called\n");

	status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RCPE, rcpe_error_code);
	if (status)
		si_mhl_tx_drive_states(dev_context);

	return status;
}

/*
 * si_mhl_tx_process_events
 * This internal function is called at the end of interrupt processing.  It's
 * purpose is to process events detected during the interrupt.  Some events
 * are internally handled here but most are handled by a notification to
 * interested applications.
 */
void si_mhl_tx_process_events(struct mhl_dev_context *dev_context)
{
	uint8_t	rapk_status;

	/* Make sure any events detected during the interrupt are processed. */
	si_mhl_tx_drive_states(dev_context);

	if(dev_context->mhl_connection_event) {
		MHL_TX_DBG_INFO(dev_context, "mhl_connection_event\n");

		/* Consume the message */
		dev_context->mhl_connection_event = false;

		/*
		 * Let interested apps know about the connection state change
		 */
		mhl_event_notify(dev_context, dev_context->mhl_connected,
						 dev_context->dev_cap_cache.mdc.featureFlag,
						 NULL);

		/* If connection has been lost, reset all state flags. */
		if(MHL_TX_EVENT_DISCONNECTION == dev_context->mhl_connected)
		{
			si_mhl_tx_reset_states(dev_context);
		}
		else if (MHL_TX_EVENT_CONNECTION == dev_context->mhl_connected)
		{
			si_mhl_tx_set_status(dev_context, MHL_STATUS_REG_CONNECTED_RDY, MHL_STATUS_DCAP_RDY, 1);
		}
	} else if(dev_context->msc_msg_arrived) {

		MHL_TX_DBG_INFO(dev_context, "MSC MSG <%02X, %02X>\n",
				 dev_context->msc_msg_sub_command,
				 dev_context->msc_msg_data);

		/* Consume the message */
		dev_context->msc_msg_arrived = false;

		/*
		 * Map MSG sub-command to an event ID
		 */
		switch(dev_context->msc_msg_sub_command) {
		case MHL_MSC_MSG_RAP:
			/*
			 * RAP messages are fully handled here.
			 */
			if (dev_context->mhl_flags & MHL_STATE_APPLICATION_RAP_BUSY){
				rapk_status = MHL_RAPK_BUSY;
			}else{
				rapk_status = MHL_RAPK_NO_ERR;
			}
			dev_context->rap_sub_command = dev_context->msc_msg_data;
			if (MHL_RAP_POLL== dev_context->msc_msg_data) {
				// just do the ack
			} else if (MHL_RAP_CONTENT_ON == dev_context->msc_msg_data) {
			    MHL_TX_DBG_INFO(dev_context, "RAP CONTENT_ON\n");
				dev_context->misc_flags.flags.rap_content_on = true;
				si_mhl_tx_drv_content_on(
						(struct drv_hw_context *)&dev_context->drv_context);
			} else if (MHL_RAP_CONTENT_OFF == dev_context->msc_msg_data) {
				MHL_TX_DBG_INFO(dev_context, "RAP CONTENT_OFF\n");
				if (dev_context->misc_flags.flags.rap_content_on){
					dev_context->misc_flags.flags.rap_content_on = false;
					si_mhl_tx_drv_content_off(
						(struct drv_hw_context *)&dev_context->drv_context);
				}
			} else {
				MHL_TX_DBG_INFO(dev_context, "Unrecognized RAP code: 0x%02x "\
								"received\n", dev_context->msc_msg_data);
				rapk_status = MHL_RAPK_UNRECOGNIZED;
			}

			/* Always RAPK to the peer */
			si_mhl_rapk_send(dev_context, rapk_status);

			if (rapk_status == MHL_RAPK_NO_ERR)
				mhl_event_notify(dev_context, MHL_TX_EVENT_RAP_RECEIVED,
								 dev_context->msc_msg_data, NULL);
			break;

		case MHL_MSC_MSG_RCP:
			/*
			 * If we get a RCP key that we do NOT support, send back RCPE
			 * Do not notify app layer.
			 */
			if (rcpSupportTable[dev_context->msc_msg_data & 0x7F]
			    & MHL_LOGICAL_DEVICE_MAP) {
				mhl_event_notify(dev_context, MHL_TX_EVENT_RCP_RECEIVED,
						dev_context->msc_msg_data, NULL);
			} else {
				/* Save keycode to send a RCPK after RCPE. */
				dev_context->msc_save_rcp_key_code = dev_context->msc_msg_data;
				si_mhl_tx_rcpe_send(dev_context, RCPE_INEEFECTIVE_KEY_CODE);
			}
			break;

		case MHL_MSC_MSG_RCPK:
			mhl_event_notify(dev_context, MHL_TX_EVENT_RCPK_RECEIVED,
							 dev_context->msc_msg_data, NULL);
			break;

		case MHL_MSC_MSG_RCPE:
			mhl_event_notify(dev_context, MHL_TX_EVENT_RCPE_RECEIVED,
							 dev_context->msc_msg_data, NULL);
			break;

		case MHL_MSC_MSG_UCP:
			/*
			 * Save keycode  so that we can send an UCPE message in
			 * case the UCP key code is rejected by the host application.
			 *
			 */
			dev_context->msc_save_ucp_key_code = dev_context->msc_msg_data;
			mhl_event_notify(dev_context, MHL_TX_EVENT_UCP_RECEIVED,
					dev_context->msc_save_ucp_key_code, NULL);
			break;

		case MHL_MSC_MSG_UCPK:
			mhl_event_notify(dev_context, MHL_TX_EVENT_UCPK_RECEIVED,
							 dev_context->msc_msg_data, NULL);
			break;

		case MHL_MSC_MSG_UCPE:
			mhl_event_notify(dev_context, MHL_TX_EVENT_UCPE_RECEIVED,
							 dev_context->msc_msg_data, NULL);
			break;

		case MHL_MSC_MSG_RAPK:
			MHL_TX_DBG_INFO(dev_context, "RAPK\n");
			break;

		default:
			MHL_TX_DBG_WARN(dev_context, "Unexpected MSC message "\
					 "sub-command code: 0x%02x received!\n",
					 dev_context->msc_msg_sub_command);
			break;
		}
	}
}

bool si_mhl_tx_read_devcap(struct mhl_dev_context *dev_context,
								  uint8_t offset)
{
	struct cbus_req		*req;

	MHL_TX_DBG_INFO(dev_context, "called\n");

	req = get_free_cbus_queue_entry(dev_context);
	if (req == NULL) {
		dev_err(dev_context->mhl_dev, "si_mhl_tx_read_devcap: CBUS free queue exhausted\n");
		return false;
	}

	req->retry_count	= 2;
	req->command		= MHL_READ_DEVCAP;
	req->reg			= offset;
	req->reg_data		= 0;  /* do this to avoid confusion */
//	req->offset_data  = offset;
//	req->payload_u.msg_data[0]  = 0;  /* do this to avoid confusion */

	queue_cbus_transaction(dev_context, req);

	return true;
}

bool si_mhl_tx_rcpk_send(struct mhl_dev_context *dev_context, uint8_t rcp_key_code)
{
	bool	status;

	MHL_TX_DBG_INFO(dev_context, "called\n");

	status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RCPK, rcp_key_code);
	if (status)
		si_mhl_tx_drive_states(dev_context);

	return status;
}

/*
 * si_mhl_tx_request_first_edid_block
 *
 * This function initiates a CBUS command to read the specified EDID block.
 * Returns true if the command was queued successfully.
 */
extern void enable_intr3(struct drv_hw_context *hw_context);
void si_mhl_tx_request_first_edid_block(struct mhl_dev_context *dev_context)
{
	MHL_TX_DBG_INFO(dev_context, "tag: EDID active: %d\n" ,dev_context->misc_flags.flags.edid_loop_active);

	dev_context->edid_parse_done = false;	// TOOD: FD, TBI
	/* Enable EDID interrupt */
	//TODO: FD, TBC, EDID DDC handling interrupt
	enable_intr3((struct drv_hw_context *) (&dev_context->drv_context));

	if (!dev_context->misc_flags.flags.edid_loop_active) {
		struct cbus_req	*req;

		req = get_free_cbus_queue_entry(dev_context);
		if (req == NULL) {
			MHL_TX_DBG_INFO(dev_context, "couldn't get free cbus req \n");
		} else{
			dev_context->misc_flags.flags.edid_loop_active = 1;
			MHL_TX_DBG_INFO(dev_context, "tag: EDID active: %d\n"
					,dev_context->misc_flags.flags.edid_loop_active);

			/* Send MHL_READ_EDID_BLOCK command */
			req->retry_count	= 2;
			req->command		= MHL_READ_EDID_BLOCK;
			req->offset		= 0; /* block number */
			req->msg_data[0]	= 0;  /* do this to avoid confusion */

			queue_cbus_transaction(dev_context, req);
			si_mhl_tx_drive_states(dev_context);
            dev_context->current_cbus_req = NULL;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// si_mhl_tx_msc_command_done
//
// This function is called by the driver to inform of completion of last command.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printfs.
//
//#define FLAG_OR_NOT(x) TestMiscFlag(FLAGS_HAVE_##x)?#x:""
//#define SENT_OR_NOT(x) TestMiscFlag(FLAGS_SENT_##x)?#x:""

void si_mhl_tx_msc_command_done(struct mhl_dev_context *dev_context, uint8_t data1)
{
	struct cbus_req			*req;


	req = dev_context->current_cbus_req;
	if (req == NULL) {
		MHL_TX_DBG_ERR(dev_context, "No message to associate with "\
				 "completion notification\n");
		return;
	}
	
    ///MHL_TX_DBG_ERR(dev_context, " cmd--0x%x(reg0x%x) data1 = %02X\n", req->command, req->reg, data1);

	dev_context->current_cbus_req = NULL;

	if (req->status.flags.cancel == true) {
		MHL_TX_DBG_INFO(dev_context, "Canceling request with command 0x%02x\n",
						req->command);

	} else if (MHL_READ_DEVCAP == req->command ) {
		bool temp;
		int i;
		MHLDevCap_u	devcap_changes;

		if (req->reg < DEVCAP_SIZE)	// req->reg keep the current offset
		{
			dev_context->dev_cap_cache_new.devcap_cache[req->reg] = data1;
			MHL_TX_DBG_INFO(dev_context, "MHL_READ_DEVCAP DONE, idx: 0x%02x, data: 0x%02x\n",
					req->reg, data1);
		}

		/*
		 * Check if all DEVCAP registers have been read
		 */           
        if(0x0F == req->reg)
        {
            if((data1 == 0xB9) || (data1 == 0xBA))
            {
            	//SMB
            	mhl_event_notify(dev_context, MHL_TX_EVENT_DEV_CAP_UPDATE, data1, NULL);
            }
			
        }
		// Not all DEVCAP registers have been read
		if (++dev_context->dev_cap_cache_index < DEVCAP_SIZE)
		{
			si_mhl_tx_read_devcap(dev_context, dev_context->dev_cap_cache_index);
		}
		// All DEVCAP registers have been read
		else
		{
			/*
			 *  Generate a change mask between the old and new devcaps
			 */
			for (i=0; i< sizeof(dev_context->dev_cap_cache);++i){
				devcap_changes.devcap_cache[i] 
					= dev_context->dev_cap_cache.devcap_cache[i]
					^ dev_context->dev_cap_cache_new.devcap_cache[i];
			}

			// update the DEVCAP cache
			dev_context->dev_cap_cache = dev_context->dev_cap_cache_new;
			dev_context->dev_cap_cache_index = 0;

			// look for a change in the pow bit
			if (MHL_DEV_CATEGORY_POW_BIT & devcap_changes.mdc.deviceCategory){
				uint8_t param;
				param  = dev_context->dev_cap_cache.mdc.deviceCategory
					& MHL_DEV_CATEGORY_POW_BIT;

				if (param) {
					/*
					 * Since downstream device is supplying VBUS power we turn
					 * off our VBUS power here.  If the platform application
					 * can control VBUS power it should turn off it's VBUS
					 * power now.
					 */
					mhl_tx_vbus_control(VBUS_OFF);
					//set_pin((struct drv_hw_context *)&dev_context->drv_context
					//		,LED_SINK_VBUS_ON
					//		,GPIO_LED_ON
					 //      );
				}else{
					mhl_tx_vbus_control(VBUS_ON);
					//set_pin((struct drv_hw_context *)&dev_context->drv_context
					//		,LED_SINK_VBUS_ON
					//		,GPIO_LED_OFF
					 //      );
				}
				/* Inform interested Apps of the MHL power change */
				mhl_event_notify(dev_context, MHL_TX_EVENT_POW_BIT_CHG,
						param, NULL);
			}

			

			/*
			 * Check to see if any other bits besides POW_BIT have changed
			 */
			devcap_changes.mdc.deviceCategory &= ~MHL_DEV_CATEGORY_POW_BIT;
#if 0//def MHL2_ENHANCED_MODE_SUPPORT
			// Init Enhanced Mode process after both EDID and DEVCAP are processed
			//else
			{
				struct drv_hw_context *hw_context = (struct drv_hw_context *)(&dev_context->drv_context);
				MHL_TX_DBG_INFO(dev_context, "@@@@@@@@@Init Enhanced Mode process after both EDID and DEVCAP are processed, si_mhl_tx_em_query() called.\n");
				hw_context->ready_for_mdt = true;
				si_mhl_tx_em_query(dev_context);
			}
#endif // MHL2_ENHANCED_MODE_SUPPORT

			temp = 0;
			for (i = 0; i < sizeof(devcap_changes);++i){
				temp |= devcap_changes.devcap_cache[i];
			}
			if (temp){
				if (dev_context->misc_flags.flags.mhl_hpd) {
					MHL_TX_DBG_INFO(dev_context, "Have HPD\n");
					si_mhl_tx_initiate_edid_sequence(dev_context->edid_parser_context);
				} else {
					MHL_TX_DBG_INFO(dev_context, "No HPD\n");
				}
			}
			/* indicate that the DEVCAP cache is up to date. */
			dev_context->misc_flags.flags.have_complete_devcap = true;
		}
	} else if (MHL_READ_EDID_BLOCK == req->command) {
		si_mhl_tx_drive_states(dev_context);			// TODO: FD, TBC, may need to check
/*
		if (0 == data1) {
			si_mhl_tx_handle_atomic_hw_edid_read_complete( dev_context->edid_parser_context, req);
			dev_context->edid_parse_done = true;		// TODO: FD, TBC, check carefully
		} 
		dev_context->misc_flags.flags.edid_loop_active = 0;
*/
		MHL_TX_DBG_INFO(dev_context, "tag: EDID active: %d\n"
				,dev_context->misc_flags.flags.edid_loop_active);

		// EDID read need to be done in SW TPI mode, as it is done now, switch back to default TPI mode: HW TPI mode
//		si_mhl_tx_drv_set_hw_tpi_mode((struct drv_hw_context *) (&dev_context->drv_context), true );

	} else if (MHL_WRITE_STAT == req->command) {
		MHL_TX_DBG_INFO(dev_context, "WRITE_STAT miscFlags: %08X\n\n",
						dev_context->misc_flags.as_uint32);
		if (MHL_STATUS_REG_CONNECTED_RDY == req->reg) {
			if (MHL_STATUS_DCAP_RDY & req->reg_data) {
				dev_context->misc_flags.flags.sent_dcap_rdy = true;

				MHL_TX_DBG_INFO(dev_context, " Sent DCAP_RDY\n");
				si_mhl_tx_set_int(dev_context, MHL_RCHANGE_INT,
								  MHL_INT_DCAP_CHG, 0);
			}
		} else if (MHL_STATUS_REG_LINK_MODE == req->reg) {
			if ( MHL_STATUS_PATH_ENABLED & req->reg_data) {
				dev_context->misc_flags.flags.sent_path_en = true;
				MHL_TX_DBG_INFO(dev_context, "FLAGS_SENT_PATH_EN\n");
			}
		}

	} else if (MHL_MSC_MSG == req->command) {
		// TODO: FD, TBI, seems there is no chance to get to here... This function will only be called after DRV_INTR_FLAG_MSC_DONE...
		if (dev_context->intr_info.flags & DRV_INTR_FLAG_MSC_NAK) {

			msleep(1000); 
			MHL_TX_DBG_INFO(dev_context, "MSC_NAK, re-trying... \n");
			/*
			 * Request must be retried, so place it back
			 * on the front of the queue.
			 */
			req->status.as_uint8 = 0;
			queue_priority_cbus_transaction(dev_context, req);
			req = NULL;
		} else {
			if (MHL_MSC_MSG_RCPE == req->msg_data[0]) {
				/*
				 * RCPE is always followed by an RCPK with original
				 * key code received.
				 */
				si_mhl_tx_rcpk_send(dev_context, dev_context->msc_save_rcp_key_code);
			} else if (MHL_MSC_MSG_UCPE == req->msg_data[0]) {
				/*
				 * UCPE is always followed by an UCPK with original
				 * key code received.
				 */
				si_mhl_tx_ucpk_send(dev_context, dev_context->msc_save_ucp_key_code);
			} else {
				MHL_TX_DBG_INFO(dev_context, "default\n" \
						 "\tcommand: 0x%02X \n" \
						 "\tmsg_data: 0x%02X " \
						 "msc_msg_last_data: 0x%02X\n",
						 req->command,
						 req->msg_data[0],
						 dev_context->msc_msg_last_data);
			}
		}

	} else if (MHL_WRITE_BURST == req->command) {
		MHL_TX_DBG_INFO(dev_context, "MHL_WRITE_BURST\n");

		/*
		 * Write to scratch pad of downstream device is complete.
		 * Send a SET_INT message to the device to inform it of the
		 * completion.  Use priority 0 to place this message at the
		 * head of the queue.
		 */
		si_mhl_tx_set_int(dev_context, MHL_RCHANGE_INT,
						  MHL_INT_DSCR_CHG, 0);

	} else if (MHL_SET_INT == req->command) {
		MHL_TX_DBG_INFO(dev_context, "MHL_SET_INT\n");
		if (MHL_RCHANGE_INT == req->reg) {
			MHL_TX_DBG_INFO(dev_context, "\n\nSent MHL_RCHANGE_INT\n");

			if (MHL_INT_DSCR_CHG == req->reg_data) {
				MHL_TX_DBG_INFO(dev_context, "MHL_INT_DSCR_CHG\n");
				dev_context->misc_flags.flags.scratchpad_busy = false;

			} else if (MHL_INT_REQ_WRT == req->reg_data) {
				/*
				 * Successfully sent scratch pad write request.
				 * Now reformat the command queue entry used to send the
				 * write request to send the write burst data once a
				 * write grant interrupt is received.
				 */
			    req->retry_count				= 1;
			    req->command					= MHL_WRITE_BURST;
				queue_priority_cbus_transaction(dev_context, req);
				req = NULL;
			}
		}

	} else {
		MHL_TX_DBG_INFO(dev_context, "default\n"
				 "\tcommand: 0x%02X reg: 0x%02x reg_data: 0x%02x "\
				 "offset: 0x%02x msg_data[0]: 0x%02x msg_data[1]: 0x%02x\n",
				 req->command,
				 req->reg, req->reg_data,
				 req->offset,
				 req->msg_data[0], req->msg_data[1]);
	}

	if (req != NULL)
		return_cbus_queue_entry(dev_context, req);

	if (!(dev_context->misc_flags.flags.rcp_ready)) {
		MHL_TX_DBG_INFO(dev_context, "have(%s) sent(%s %s)\n",
				 (dev_context->misc_flags.flags.have_complete_devcap) ?
				  "complete DEV_CAP" : "",
				 (dev_context->misc_flags.flags.sent_path_en) ?
				  "PATH_EN" : "",
				 (dev_context->misc_flags.flags.sent_dcap_rdy) ?
				  "DCAP_RDY" : "");

		if (dev_context->misc_flags.flags.have_complete_devcap) {
			if (dev_context->misc_flags.flags.sent_path_en) {
				if (dev_context->misc_flags.flags.sent_dcap_rdy) {
					/*
					 * Now we can entertain App commands for RCP, UCP, RAP
					 */
					dev_context->misc_flags.flags.rcp_ready = true;
				}
			}
		}
	}
}

extern bool si_mhl_tx_set_path_en_I(struct mhl_dev_context *dev_context);
extern void siHdmiTx_VideoSel (int vmode);
void si_mhl_tx_process_write_burst_data(struct mhl_dev_context *dev_context)
{
	int	ret_val = 0;
	BurstId_e burst_id;

	MHL_TX_DBG_INFO(NULL,"\n");
	// continue else statement to support 3D along with MDT
	ret_val = si_mhl_tx_drv_get_scratch_pad((struct drv_hw_context *)
						(&dev_context->drv_context), 0,
						dev_context->incoming_scratch_pad.asBytes,
						sizeof(dev_context->incoming_scratch_pad));
	if (ret_val < 0) {
		MHL_TX_DBG_INFO(dev_context, "scratch pad failure 0x%x\n",
						ret_val);
	} else {
		burst_id = BURST_ID(dev_context->incoming_scratch_pad.
						   videoFormatData.burst_id);

		switch(burst_id) {
		case burst_id_3D_VIC:
			si_mhl_tx_process_3d_vic_burst(
					dev_context->edid_parser_context,
					&dev_context->incoming_scratch_pad.videoFormatData);
			break;

		case burst_id_3D_DTD:
			si_mhl_tx_process_3d_dtd_burst(
					dev_context->edid_parser_context,
					&dev_context->incoming_scratch_pad.videoFormatData);
			break;

		case LOCAL_ADOPTER_ID:
#if defined(MHL2_ENHANCED_MODE_SUPPORT)
			si_mhl_tx_em_process_packet(dev_context,(void *)&dev_context->incoming_scratch_pad.asBytes);
#elif defined(MEDIA_DATA_TUNNEL_SUPPORT) //(
		case MHL_TEST_ADOPTER_ID:
			si_mhl_tx_mdt_process_packet(dev_context,(void *)&dev_context->incoming_scratch_pad.asBytes);
#else //)(
			// Set flag to 'unread' before the notification to APP level
			dev_context->scratch_pad_read_done = false;

			/*
			 * Cause a notification event to be raised to allow
			 * interested applications a chance to process the
			 * received write burst data.
			 */
			mhl_event_notify(dev_context, MHL_TX_EVENT_SPAD_RECEIVED,
							 sizeof(dev_context->incoming_scratch_pad),
							 dev_context->incoming_scratch_pad.asBytes);
#endif //)
			break;

		default:
			MHL_TX_DBG_INFO(dev_context, "Dropping write burst with "\
							"invalid adopter id: 0x%04x\n", burst_id);
			break;
		}
	}
}

void si_mhl_tx_set_pp_link(struct mhl_dev_context *dev_context, uint8_t value)
{
        bool status = false;

        status = si_mhl_tx_set_status(dev_context, MHL_STATUS_REG_LINK_MODE, value, 0);
        if (status)
        {
                si_mhl_tx_drive_states(dev_context);
        }
}

static bool si_mhl_tx_set_path_en(struct mhl_dev_context *dev_context)
{
	MHL_TX_DBG_INFO(dev_context, "called\n");
	/*si_mhl_tx_drv_enable_video_path((struct drv_hw_context *) (&dev_context->drv_context));	// TODO: FD, TBI, check references of this function for details */
	dev_context->link_mode |= MHL_STATUS_PATH_ENABLED;
	return si_mhl_tx_set_status(dev_context, MHL_STATUS_REG_LINK_MODE, dev_context->link_mode, 1);
}

static bool si_mhl_tx_clr_path_en(struct mhl_dev_context *dev_context)
{
	MHL_TX_DBG_ERR(dev_context, "called\n");
	si_mhl_tx_drv_disable_video_path((struct drv_hw_context *) (&dev_context->drv_context));
	dev_context->link_mode &= ~MHL_STATUS_PATH_ENABLED;
	return si_mhl_tx_set_status(dev_context, MHL_STATUS_REG_LINK_MODE, dev_context->link_mode, 1);
}

static void si_mhl_tx_refresh_peer_devcap_entries(
		struct mhl_dev_context *dev_context)
{
	if (MHL_STATUS_DCAP_RDY & dev_context->status_0) {
		MHL_TX_DBG_INFO(dev_context, "DCAP_RDY DEVCAP: %s\n"
				,dev_context->misc_flags.flags.have_complete_devcap
				?"current":"stale");
		dev_context->misc_flags.flags.have_complete_devcap = false;//SET the para to default;

		// TODO: FD, TBC, begin
		// bugzilla 27431 - dongle power cord attachment fix. if (!dev_context->misc_flags.flags.have_complete_devcap)
		{
			MHL_TX_DBG_INFO(dev_context, "devcap is stale\n");

			/*
			 * If there is a DEV CAP read operation in progress
			 * cancel it and issue a new DEV CAP read to make sure
			 * we pick up all the DEV CAP register changes.
			 */
			if (dev_context->current_cbus_req != NULL) {
				if (dev_context->current_cbus_req->command == MHL_READ_DEVCAP) {
					dev_context->current_cbus_req->status.flags.cancel = true;
				}
			}
#ifdef MHL2_ENHANCED_MODE_SUPPORT
		si_mhl_tx_em_init(dev_context);
#endif // MHL2_ENHANCED_MODE_SUPPORT

			// TODO: FD, TBC, to read_devcap
			dev_context->dev_cap_cache_index = 0;
			si_mhl_tx_read_devcap(dev_context, dev_context->dev_cap_cache_index);
		}
		// TODO: FD, TBC, end

	} else {
		MHL_TX_DBG_INFO(dev_context, "Can't read DEV CAP registers, DCAP_RDY not set yet\n");
	}
}

/*
 * si_mhl_tx_got_mhl_msc_message
 *
 * This function is called by the driver to inform of arrival of a MHL MSC_MSG
 * such as RCP, RCPK, RCPE.
 */
//void si_mhl_tx_got_mhl_msc_message(struct mhl_dev_context *dev_context,
//								   uint8_t sub_command, uint8_t cmd_data)
//{
//
//	/* Remember the event for processing at the completion of the interrupt. */
//	dev_context->msc_msg_arrived		= true;
//	dev_context->msc_msg_sub_command	= sub_command;
//	dev_context->msc_msg_data			= cmd_data;
//}

/*
 * si_mhl_tx_got_mhl_intr
 *
 * This function is called to inform of the arrival
 * of an MHL INTERRUPT message.
 */
void si_mhl_tx_got_mhl_intr(struct mhl_dev_context *dev_context, uint8_t intr_0, uint8_t intr_1)
{

	MHL_TX_DBG_INFO(dev_context, "INTERRUPT Arrived. %02X, %02X\n",
					intr_0, intr_1);

	/* Handle DCAP_CHG INTR here */
	if (MHL_INT_DCAP_CHG & intr_0) {
		MHL_TX_DBG_INFO(dev_context, "got DCAP_CHG\n");
		if (MHL_STATUS_DCAP_RDY & dev_context->status_0) {

			MHL_TX_DBG_INFO(dev_context, "got DCAP_CHG & DCAP_RDY\n");
			si_mhl_tx_refresh_peer_devcap_entries(dev_context);
		}
	}

	if (MHL_INT_DSCR_CHG & intr_0) {

		/* remote WRITE_BURST is complete */
		dev_context->misc_flags.flags.scratchpad_busy = false;
		si_mhl_tx_process_write_burst_data(dev_context);
	}

	if( MHL_INT_REQ_WRT  & intr_0) {
		/* Scratch pad write request from the sink device. */
		/*
		 * Also need to consider the 'read' status of scratch pad from APP level
		 */
		if (dev_context->misc_flags.flags.scratchpad_busy || !dev_context->scratch_pad_read_done) {
			/*
			 * Use priority 1 to defer sending grant until
			 * local traffic is done
			 */
			si_mhl_tx_set_int(dev_context, MHL_RCHANGE_INT,
					MHL_INT_GRT_WRT, 1);
		} else {
			dev_context->misc_flags.flags.scratchpad_busy = true;
			/* use priority 0 to respond immediately */
			si_mhl_tx_set_int(dev_context, MHL_RCHANGE_INT,
					MHL_INT_GRT_WRT, 0);
		}
	}

	if (MHL_INT_GRT_WRT  & intr_0) {
		/*
		 * Write burst grant received so enable
		 * write burst message to be sent.
		 */
		dev_context->misc_flags.flags.write_burst_pending = false;
	}
//    	uint8_t length = sizeof(dev_context->outgoing_scratch_pad);
//    	MHL_TX_DBG_INFO(dev_context, "MHL_INT_GRT_WRT length:%d\n",
//    					length);
//        si_mhl_tx_do_write_burst(dev_context, 0x40,
//        						 dev_context->outgoing_scratch_pad.asBytes,
//        						 length);
//    }

	if(MHL_INT_EDID_CHG & intr_1) {

		MHL_TX_DBG_INFO(dev_context, "MHL_INT_EDID_CHG\n");
		si_edid_reset(dev_context->edid_parser_context);
		if (dev_context->misc_flags.flags.have_complete_devcap) {
			if (dev_context->misc_flags.flags.mhl_hpd){
				MHL_TX_DBG_INFO(dev_context, "tag: EDID_CHG\n");
				si_mhl_tx_initiate_edid_sequence(dev_context->edid_parser_context);
			}
		} else {
			MHL_TX_DBG_INFO(dev_context, "refreshing DEVCAP");
			si_mhl_tx_refresh_peer_devcap_entries(dev_context);
		}
	}
}

/*
 * si_mhl_tx_got_mhl_status
 *
 * This function is called by the driver to inform of arrival of a MHL STATUS.
 */
void si_mhl_tx_got_mhl_status(struct mhl_dev_context *dev_context, uint8_t status_0, uint8_t status_1)
{
	uint8_t status_change_bit_mask_0;
	uint8_t status_change_bit_mask_1;

	MHL_TX_DBG_INFO(dev_context, "STATUS Arrived. %02X, %02X\n",
					status_0, status_1);
	/*
	 * Handle DCAP_RDY STATUS here itself
	 */
	status_change_bit_mask_0 = status_0 ^ dev_context->status_0;
	status_change_bit_mask_1 = status_1 ^ dev_context->status_1;

	/*
	 * Remember the event.   (other code checks the saved values,
	 * so save the values early, but not before the XOR operations above)
	 */
	dev_context->status_0 = status_0;
	dev_context->status_1 = status_1;

	if(MHL_STATUS_DCAP_RDY & status_change_bit_mask_0) {
		MHL_TX_DBG_INFO(dev_context, "DCAP_RDY changed\n");
		if (MHL_STATUS_DCAP_RDY & status_0)
			si_mhl_tx_refresh_peer_devcap_entries(dev_context);
	}

	/* did PATH_EN change? */
	if(MHL_STATUS_PATH_ENABLED & status_change_bit_mask_1) {
		MHL_TX_DBG_INFO(dev_context, "PATH_EN changed\n");
		if(MHL_STATUS_PATH_ENABLED & status_1)
			si_mhl_tx_set_path_en(dev_context);
		else
			si_mhl_tx_clr_path_en(dev_context);
	}
}

/*
 * si_mhl_tx_rcp_send
 *
 * This function checks if the peer device supports RCP and sends rcpKeyCode. The
 * function will return a value of true if it could successfully send the RCP
 * subcommand and the key code. Otherwise false.
 *
 */
bool si_mhl_tx_rcp_send(struct mhl_dev_context *dev_context, uint8_t rcpKeyCode)
{
	bool	status;

	MHL_TX_DBG_INFO(dev_context, "called\n");

	/*
	 * Make sure peer supports RCP
	 */
	if ((dev_context->dev_cap_cache.mdc.featureFlag & MHL_FEATURE_RCP_SUPPORT) &&
			(dev_context->misc_flags.flags.rcp_ready)) {

		status = si_mhl_tx_send_msc_msg (dev_context, MHL_MSC_MSG_RCP, rcpKeyCode);
		if(status)
			si_mhl_tx_drive_states(dev_context);
	} else {
		MHL_TX_DBG_ERR(dev_context, "failed\n");
		status = false;
	}

    return status;
}

/*
 * si_ucp_msg_send
 *
 * This function sends the requested UCP message if UCP reception is
 * supported by the downstream device.
 *
 * The function returns true if the message can be sent, false otherise.
 */
bool si_ucp_msg_send(struct mhl_dev_context *dev_context,
					 uint8_t ucp_msg_sub_cmd, uint8_t ucp_msg_data)
{
	bool	status;

	MHL_TX_DBG_INFO(dev_context, "called\n");

	/*
	 * Make sure peer supports UCP and that the connection is
	 * in a state where a UCP message can be sent.
	 */
	if ((dev_context->dev_cap_cache.mdc.featureFlag & MHL_FEATURE_UCP_RECV_SUPPORT) &&
			(dev_context->misc_flags.flags.rcp_ready)) {

		status = si_mhl_tx_send_msc_msg(dev_context, ucp_msg_sub_cmd, ucp_msg_data);
		if (status) {
			si_mhl_tx_drive_states(dev_context);
		}
	} else {
		MHL_TX_DBG_ERR(dev_context, "failed\n");
		status = false;
	}

    return status;
}

/*
 * si_mhl_tx_ucp_send
 *
 * This function is (indirectly) called by a host application to send
 * a UCP key code to the downstream device.
 *
 * Returns true if the key code can be sent, false otherwise.
 */
bool si_mhl_tx_ucp_send(struct mhl_dev_context *dev_context,
						uint8_t ucp_key_code)
{
	MHL_TX_DBG_INFO(dev_context, "called key code: 0x%02x\n", ucp_key_code);

	return (si_ucp_msg_send(dev_context, MHL_MSC_MSG_UCP, ucp_key_code));
}

/*
 * si_mhl_tx_ucp_send
 *
 * This function is (indirectly) called by a host application to send
 * a UCP acknowledge message for a received UCP key code message.
 *
 * Returns true if the message can be sent, false otherwise.
 */
bool si_mhl_tx_ucpk_send(struct mhl_dev_context *dev_context,
						 uint8_t ucp_key_code)
{
	MHL_TX_DBG_INFO(dev_context, "called key code: 0x%02x\n", ucp_key_code);

	return (si_ucp_msg_send(dev_context, MHL_MSC_MSG_UCPK, ucp_key_code));
}

/*
 * si_mhl_tx_ucpe_send
 *
 * This function is (indirectly) called by a host application to send a
 * UCP negative acknowledgment message for a received UCP key code message.
 *
 * Returns true if the message can be sent, false otherwise.
 *
 * When successful, mhl_tx internally sends UCPK with original (last known)
 * UCP keycode.
 */
bool si_mhl_tx_ucpe_send(struct mhl_dev_context *dev_context,
						 uint8_t ucpe_error_code)
{
	MHL_TX_DBG_INFO(dev_context, "called\n");

	return (si_ucp_msg_send(dev_context, MHL_MSC_MSG_UCPE, ucpe_error_code));
}

/*
 * si_mhl_tx_rap_send
 *
 * This function sends the requested RAP action code message if RAP
 * is supported by the downstream device.
 *
 * The function returns true if the message can be sent, false otherwise.
 */
bool si_mhl_tx_rap_send(struct mhl_dev_context *dev_context,
						uint8_t rap_action_code)
{
	bool	status;

	MHL_TX_DBG_INFO(dev_context, "called\n");

	/*
	 * Make sure peer supports RAP and that the connection is
	 * in a state where a RAP message can be sent.
	 */
	if ((dev_context->dev_cap_cache.mdc.featureFlag & MHL_FEATURE_RAP_SUPPORT) &&
		(dev_context->misc_flags.flags.rcp_ready)) {

		status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RAP, rap_action_code);
		if (status) {
			si_mhl_tx_drive_states(dev_context);
		}
	} else {
		MHL_TX_DBG_ERR(dev_context, "failed\n");
		status = false;
	}

    return status;
}

/*
 * si_mhl_tx_notify_downstream_hpd_change
 *
 * Handle the arrival of SET_HPD or CLEAR_HPD messages.
 *
 * Turn the content off or on based on what we got.
 */
void si_mhl_tx_notify_downstream_hpd_change( struct mhl_dev_context *dev_context, uint8_t downstream_hpd)
{

	MHL_TX_DBG_INFO(dev_context, "HPD = %s\n",
			downstream_hpd ? "HIGH" : "LOW");

	if (0 == downstream_hpd) {
		struct cbus_req	*req=dev_context->current_cbus_req;
		dev_context->misc_flags.flags.mhl_hpd = false;

        mhl_tx_stop_timer(dev_context, dev_context->cbus_dpi_timer);
		mhl_event_notify(dev_context, MHL_TX_EVENT_HPD_CLEAR, 0, NULL);
		if (req) {
			if (MHL_READ_EDID_BLOCK  == req->command){

				return_cbus_queue_entry(dev_context, req);
				dev_context->current_cbus_req = NULL;
				/*dev_context->misc_flags.flags.edid_loop_active = 0;
				MHL_TX_DBG_INFO(dev_context, "tag: EDID active: %d\n"
						,dev_context->misc_flags.flags.edid_loop_active);*/
			}
		}
		dev_context->misc_flags.flags.edid_loop_active = 0;
		MHL_TX_DBG_INFO(dev_context, "tag: EDID active: %d\n"
				,dev_context->misc_flags.flags.edid_loop_active);
		si_edid_reset(dev_context->edid_parser_context);
	} else {
		dev_context->misc_flags.flags.mhl_hpd = true;
       		/*
		 *  possible EDID read is complete here
		 *  see MHL spec section 5.9.1
		 */
		if (dev_context->misc_flags.flags.have_complete_devcap) {
			 process_dpi(dev_context);
			/* Devcap refresh is complete */
			MHL_TX_DBG_INFO(dev_context, "tag:\n");
			si_mhl_tx_initiate_edid_sequence(dev_context->edid_parser_context);
		} else {
			si_mhl_tx_refresh_peer_devcap_entries(dev_context);
		}
	}
}

/*
 * 	si_mhl_tx_get_peer_dev_cap_entry
 *
 * 	index -- the devcap index to get
 * 	*data pointer to location to write data
 *
 * 	returns
 * 		0 -- success
 * 		1 -- busy.
 */
uint8_t si_mhl_tx_get_peer_dev_cap_entry(struct mhl_dev_context *dev_context,
										 uint8_t index, uint8_t *data)
{
	if (!dev_context->misc_flags.flags.have_complete_devcap) {
		/* update is in progress */
		return 1;
	} else {
		*data = dev_context->dev_cap_cache.devcap_cache[index];
		return 0;
	}
}

/*
	si_get_scratch_pad_vector
	offset -- The beginning offset into the scratch pad from which to fetch entries.
	length -- The number of entries to fetch
	*data -- A pointer to an array of bytes where the data should be placed.

	returns:
		scratch_pad_status see si_mhl_tx_api.h for details

 */
enum scratch_pad_status si_get_scratch_pad_vector(
		struct mhl_dev_context *dev_context,
		uint8_t offset,uint8_t length,
		uint8_t *data)
{
	if (!(dev_context->dev_cap_cache.mdc.featureFlag
				& MHL_FEATURE_SP_SUPPORT)) {

		MHL_TX_DBG_INFO(dev_context, "failed SCRATCHPAD_NOT_SUPPORTED\n");
		return SCRATCHPAD_NOT_SUPPORTED;

	} else if (dev_context->misc_flags.flags.scratchpad_busy)	{
		return SCRATCHPAD_BUSY;

	} else if ((offset >= sizeof(dev_context->incoming_scratch_pad)) ||
			(length > (sizeof(dev_context->incoming_scratch_pad)- offset))) {
		return SCRATCHPAD_BAD_PARAM;
	} else {
		uint8_t	*scratch_pad = dev_context->incoming_scratch_pad.asBytes;

		scratch_pad += offset;
		memcpy(data, scratch_pad, length);

		// Set flag to 'read' after data is retrieved by APP level
		dev_context->scratch_pad_read_done = true;
	}
	return SCRATCHPAD_SUCCESS;
}

#ifdef ENABLE_DUMP_INFOFRAME //(

#define AT_ROW_END(i,length) (i & (length-1)) == (length-1)

void DumpIncomingInfoFrameImpl(char *pszId,char *pszFile,int iLine,info_frame_t *pInfoFrame,uint8_t length)
{
	uint8_t j;
	uint8_t *pData = (uint8_t *)pInfoFrame;
	pr_debug("mhl_tx infoframe: %s: length:0x%02x -- ",pszId,length);
	for (j = 0; j < length; j++)
	{
	    pr_debug("%02X ", pData[j]);
		if (AT_ROW_END(j,32))
		{
	        pr_debug("\n");
		}
	}
	pr_debug("\n");
}
#endif //)

void *si_mhl_tx_get_drv_context(void *context)
{
	struct mhl_dev_context *dev_context = context;

	if (dev_context->signature == MHL_DEV_CONTEXT_SIGNATURE) {
		return &dev_context->drv_context;
	} else {
		return context;
	}
}

uint8_t	si_get_peer_mhl_version(void *dev_context)
{
	struct mhl_dev_context	*dev_context_ptr = (struct mhl_dev_context *)dev_context;
	uint8_t	ret_val = dev_context_ptr->dev_cap_cache.mdc.mhl_version;

	MHL_TX_DBG_INFO(dev_context_ptr, "0x%02x\n", ret_val);
	return ret_val;
}

int si_peer_supports_packed_pixel(void *dev_context)
{
	struct mhl_dev_context	*dev_context_ptr =
								(struct mhl_dev_context *)dev_context;
	return PACKED_PIXEL_AVAILABLE(dev_context_ptr);
}

int si_mhl_tx_shutdown(struct mhl_dev_context *dev_context)
{
	MHL_TX_DBG_ERR(dev_context, "SiI8348 may continue to output video. Driver features and APIs will not work.\n");
	si_mhl_tx_drv_shutdown((struct drv_hw_context *)&dev_context->drv_context);
	return 0;
}
