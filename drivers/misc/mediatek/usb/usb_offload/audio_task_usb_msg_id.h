/* SPDX-License-Identifier: GPL-2.0 */
/*
 * MTK USB Offload Driver
 * *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Jeremy Chou <jeremy.chou@mediatek.com>
 */

#ifndef AUDIO_USB_MSG_ID_H
#define AUDIO_USB_MSG_ID_H

#define AUD_USB_MSG_A2D_BASE (0xAD00)
#define AUD_USB_MSG_D2A_BASE (0xDA00)
#define AUD_USB_MSG_D2D_BASE (0xDD00)


/* NOTE: all ack behaviors are rely on audio_ipi_msg_ack_t */
enum aud_usb_msg_id_t {
	/* AP to ADSP, 0xAD-- */
	AUD_USB_MSG_A2D_INIT_ADSP           = AUD_USB_MSG_A2D_BASE + 0x00,

	AUD_USB_MSG_A2D_ENABLE_STREAM       = AUD_USB_MSG_A2D_BASE + 0x10,

	AUD_USB_MSG_A2D_DISCONNECT          = AUD_USB_MSG_A2D_BASE + 0x20,

	AUD_USB_MSG_A2D_TX_ATTACH           = AUD_USB_MSG_A2D_BASE + 0x30,
	AUD_USB_MSG_A2D_TX_DETACH           = AUD_USB_MSG_A2D_BASE + 0x31,
	AUD_USB_MSG_A2D_RX_ATTACH           = AUD_USB_MSG_A2D_BASE + 0x32,
	AUD_USB_MSG_A2D_RX_DETACH           = AUD_USB_MSG_A2D_BASE + 0x33,


	/* ADSP to AP, 0xDA-- */
	AUD_USB_MSG_D2A_DUMP                = AUD_USB_MSG_D2A_BASE + 0x00,


	/* IRQ, DSP to DSP, 0xDD-- */
	AUD_USB_MSG_D2D_IRQ                 = AUD_USB_MSG_D2D_BASE + 0x00,


	/* uint16_t  */
	AUD_USB_MSG_MAX = 0xFFFF
};


#endif /* end of AUDIO_USB_MSG_ID_H */
