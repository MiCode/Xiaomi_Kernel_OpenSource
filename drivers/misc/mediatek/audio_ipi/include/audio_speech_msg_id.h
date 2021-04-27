/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef AUDIO_SPEECH_MSG_ID_H
#define AUDIO_SPEECH_MSG_ID_H

#define IPI_MSG_A2D_BASE (0xAD00)
#define IPI_MSG_D2A_BASE (0xDA00)

#define IPI_MSG_M2D_BASE (0x3D00)
#define IPI_MSG_D2M_BASE (0xD300)

/* NOTE: all ack behaviors are rely on audio_ipi_msg_ack_t */
enum ipi_msg_id_call_t {
	/*
	 * =====================================================================
	 *                             AP to OpenDSP
	 * =====================================================================
	 */
	/* volume, 0xAD0- */
	IPI_MSG_A2D_UL_GAIN = IPI_MSG_A2D_BASE + 0x00,
	IPI_MSG_A2D_DL_GAIN,

	/* device environment info, 0xAD1-  */
	IPI_MSG_A2D_TASK_CFG = IPI_MSG_A2D_BASE + 0x10,
	IPI_MSG_A2D_LIB_CFG,
	IPI_MSG_A2D_SPH_PARAM,

	/* function control, 0xAD2-*/
	IPI_MSG_A2D_SPH_ON = IPI_MSG_A2D_BASE + 0x20,
	IPI_MSG_A2D_TTY_ON,

	/* speech enhancement control, 0xAD3-*/
	IPI_MSG_A2D_UL_MUTE_ON = IPI_MSG_A2D_BASE + 0x30,
	IPI_MSG_A2D_DL_MUTE_ON,
	IPI_MSG_A2D_UL_ENHANCE_ON,
	IPI_MSG_A2D_DL_ENHANCE_ON,
	IPI_MSG_A2D_BT_NREC_ON,

	/* tuning tool, 0xAD4-*/
	IPI_MSG_A2D_SET_ADDR_VALUE = IPI_MSG_A2D_BASE + 0x40,
	IPI_MSG_A2D_GET_ADDR_VALUE,
	IPI_MSG_A2D_SET_KEY_VALUE,
	IPI_MSG_A2D_GET_KEY_VALUE,

#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
	IPI_MSG_A2D_GET_EMI_ADDRESS  = IPI_MSG_A2D_BASE + 0x50,
#endif

	/* debug, 0xADA- */
	IPI_MSG_A2D_PCM_DUMP_ON = IPI_MSG_A2D_BASE + 0xA0,
	IPI_MSG_A2D_LIB_LOG_ON,

	/*
	 * =====================================================================
	 *                             OpenDSP to AP
	 * =====================================================================
	 */
	IPI_MSG_D2A_PCM_DUMP_DATA_NOTIFY = IPI_MSG_D2A_BASE + 0x00,

	/*
	 * =====================================================================
	 *                             Modem to OpenDSP
	 * =====================================================================
	 */
	/* call data handshake, 0x3D0- */
	IPI_MSG_M2D_CALL_DATA_READY = IPI_MSG_M2D_BASE + 0x00,
};

#endif /* end of AUDIO_SPEECH_MSG_ID_H */

