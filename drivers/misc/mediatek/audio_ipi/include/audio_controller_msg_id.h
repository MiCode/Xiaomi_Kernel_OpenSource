/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef AUDIO_CONTROLLER_MSG_ID_H
#define AUDIO_CONTROLLER_MSG_ID_H

#define AUD_CTL_MSG_A2D_BASE (0xAD00)
#define AUD_CTL_MSG_D2A_BASE (0xDA00)
#define AUD_CTL_MSG_D2D_BASE (0xDD00)


/* NOTE: all ack behaviors are rely on audio_ipi_msg_ack_t */
enum aud_ctl_msg_id_t {
	/* Aurisys control msg, 0xADA- */
	AUD_CTL_MSG_A2D_AURISYS_CFG         = AUD_CTL_MSG_A2D_BASE + 0xA0,
	AUD_CTL_MSG_A2D_AURISYS_SET_PARAM   = AUD_CTL_MSG_A2D_BASE + 0xA1,
	AUD_CTL_MSG_A2D_AURISYS_GET_PARAM   = AUD_CTL_MSG_A2D_BASE + 0xA2,
	AUD_CTL_MSG_A2D_AURISYS_ENABLE      = AUD_CTL_MSG_A2D_BASE + 0xA3,

	/* Boot & recovery */
	AUD_CTL_MSG_A2D_HAL_REBOOT          = AUD_CTL_MSG_A2D_BASE + 0xB0,

	/* Test */
	AUD_CTL_MSG_A2D_IPI_TEST            = AUD_CTL_MSG_A2D_BASE + 0xC0,

	/* DMA control msg, 0xADD- */
	AUD_CTL_MSG_A2D_DMA_INIT            = AUD_CTL_MSG_A2D_BASE + 0xD0,
	AUD_CTL_MSG_A2D_DMA_UPDATE_REGION   = AUD_CTL_MSG_A2D_BASE + 0xD1,

	/* Aurisys dump msg, 0xDAA- */
	AUD_CTL_MSG_D2A_AURISYS_DUMP        = AUD_CTL_MSG_D2A_BASE + 0xA0,

	/* IRQ, DSP to DSP, 0xDDD- */
	AUD_CTL_MSG_D2D_IRQ = AUD_CTL_MSG_D2D_BASE + 0x00,

	/* uint16_t  */
	AUD_CTL_MSG_MAX = 0xFFFF
};


#endif /* end of AUDIO_CONTROLLER_MSG_ID_H */

