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

#if !defined(SI_MHL_TX_DRV_API_H)
#define SI_MHL_TX_DRV_API_H


/*
 * Structure to hold command details from upper layer to CBUS module
 */
struct cbus_req {
	struct list_head	link;
	union {
		struct {
			uint8_t	cancel	: 1;	/* this command has been canceled */
			uint8_t	resvd	: 7;
		} flags;
		uint8_t	as_uint8;
	} status;
//	uint8_t				status;			/* CBUS_IDLE, CBUS_PENDING */
	uint8_t				retry_count;
	uint8_t				command;		/* VS_CMD or RCP opcode */
	uint8_t				reg;
	uint8_t				reg_data;
	uint8_t				offset;			/* register offset */
//	uint8_t				offset_data;	/* Offset of register on CBUS or RCP data */
	uint8_t				length;			/* Only applicable to write burst */
	uint8_t				msg_data[16];	/* scratch pad data area. */
};

typedef enum {
	qs_auto_select_by_color_space = 0
	,qs_full_range                = 1
	,qs_limited_range             = 2
	,qs_reserved                  = 3
} quantization_settings_e;

/*
 * The APIs listed below must be implemented by the MHL transmitter
 * hardware support module.
 */

struct drv_hw_context;
struct interrupt_info;


int si_mhl_tx_chip_initialize(struct drv_hw_context *hw_context);
void si_mhl_tx_drv_device_isr(struct drv_hw_context *hw_context, struct interrupt_info *intr_info);

void si_mhl_tx_drv_set_hw_tpi_mode(struct drv_hw_context *hw_context, bool hw_tpi_mode);

void si_mhl_tx_drv_disable_video_path(struct drv_hw_context *hw_context);
void si_mhl_tx_drv_enable_video_path(struct drv_hw_context *hw_context);

void si_mhl_tx_drv_content_on(struct drv_hw_context *hw_context);
void si_mhl_tx_drv_content_off(struct drv_hw_context *hw_context);
bool si_mhl_tx_drv_send_cbus_command(struct drv_hw_context *hw_context, struct cbus_req *req);
int si_mhl_tx_drv_get_scratch_pad(struct drv_hw_context *hw_context, uint8_t start_reg, uint8_t *data, uint8_t length);

void si_mhl_tx_drv_shutdown(struct drv_hw_context *hw_context);
/*
void si_mhl_tx_drv_audio_update(struct drv_hw_context *hw_context, int audio);
void si_mhl_tx_drv_video_update(struct drv_hw_context *hw_context, int video, int video_3d);
void si_mhl_tx_drv_video_3d_update(struct drv_hw_context *hw_context, int video, int video_3d);
void si_mhl_tx_drv_video_all_update(struct drv_hw_context *hw_context, int video, int video_3d);
*/
#ifdef	NEVER
typedef enum
{
    AUTH_IDLE
    ,AUTH_PENDING
    ,AUTH_CURRENT
}authentication_state_e;
void si_mhl_tx_drv_set_authentication_state(void *drv_context, authentication_state_e state);
authentication_state_e si_mhl_tx_drv_get_authentication_state(void *drv_context);
#endif // NEVER
void siHdmiTx_AudioSel (int AduioMode);
#endif /* if !defined(SI_MHL_TX_DRV_API_H) */
