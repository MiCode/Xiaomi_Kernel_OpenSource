/*******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/include
 *     /gl_rst.h#1
 */

/*! \file   gl_rst.h
 *    \brief  Declaration of functions and finite state machine for
 *	    MT6620 Whole-Chip Reset Mechanism
 */

#ifndef _GL_RST_H
#define _GL_RST_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "gl_typedef.h"

#if 0
#include "mtk_porting.h"
#endif
/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#if (MTK_WCN_HIF_SDIO == 1) || (MTK_WCN_HIF_AXI == 1)
#define CFG_WMT_RESET_API_SUPPORT   1
#else
#define CFG_WMT_RESET_API_SUPPORT   0
#endif

#define RST_FLAG_CHIP_RESET        0
#define RST_FLAG_DO_CORE_DUMP      BIT(0)
#define RST_FLAG_PREVENT_POWER_OFF BIT(1)

#if CFG_CHIP_RESET_HANG
#define SER_L0_HANG_RST_NONE		0
#define SER_L0_HANG_RST_TRGING		1
#define SER_L0_HANG_RST_HAND_DISABLE	2
#define SER_L0_HANG_RST_HANG		3
#define SER_L0_HANG_RST_CMD_TRG		9

#define SER_L0_HANG_LOG_TIME_INTERVAL	3000
#endif
/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
enum ENUM_RESET_STATUS {
	RESET_FAIL,
	RESET_SUCCESS
};

enum _ENUM_CHIP_RESET_REASON_TYPE_T {
	RST_PROCESS_ABNORMAL_INT = 1,
	RST_DRV_OWN_FAIL,
	RST_FW_ASSERT,
	RST_BT_TRIGGER,
	RST_OID_TIMEOUT,
	RST_CMD_TRIGGER,
	RST_REASON_MAX
};

struct RESET_STRUCT {
	struct GLUE_INFO *prGlueInfo;
	struct work_struct rst_work;
#if CFG_WMT_RESET_API_SUPPORT
	enum ENUM_RESET_STATUS rst_data;
	struct work_struct rst_trigger_work;
	uint32_t rst_trigger_flag;
#endif
};

#if CFG_WMT_RESET_API_SUPPORT
/* duplicated from wmt_exp.h for better driver isolation */
enum ENUM_WMTDRV_TYPE {
	WMTDRV_TYPE_BT = 0,
	WMTDRV_TYPE_FM = 1,
	WMTDRV_TYPE_GPS = 2,
	WMTDRV_TYPE_WIFI = 3,
	WMTDRV_TYPE_WMT = 4,
	WMTDRV_TYPE_STP = 5,
	WMTDRV_TYPE_SDIO1 = 6,
	WMTDRV_TYPE_SDIO2 = 7,
	WMTDRV_TYPE_LPBK = 8,
	WMTDRV_TYPE_MAX
};

enum ENUM_WMTMSG_TYPE {
	WMTMSG_TYPE_POWER_ON = 0,
	WMTMSG_TYPE_POWER_OFF = 1,
	WMTMSG_TYPE_RESET = 2,
	WMTMSG_TYPE_STP_RDY = 3,
	WMTMSG_TYPE_HW_FUNC_ON = 4,
	WMTMSG_TYPE_MAX
};

enum ENUM_WMTRSTMSG_TYPE {
	WMTRSTMSG_RESET_START = 0x0,
	WMTRSTMSG_RESET_END = 0x1,
	WMTRSTMSG_RESET_END_FAIL = 0x2,
	WMTRSTMSG_RESET_MAX,
	WMTRSTMSG_RESET_INVALID = 0xff
};

typedef void (*PF_WMT_CB) (enum ENUM_WMTDRV_TYPE, /* Source driver type */
			   enum ENUM_WMTDRV_TYPE, /* Destination driver type */
			   enum ENUM_WMTMSG_TYPE, /* Message type */
			   /* READ-ONLY buffer. Buffer is allocated and
			    * freed by WMT_drv. Client can't touch this
			    * buffer after this function return.
			    */
			   void *,
			   unsigned int); /* Buffer size in unit of byte */

#endif

/*******************************************************************************
 *                    E X T E R N A L   F U N C T I O N S
 *******************************************************************************
 */
#if CFG_CHIP_RESET_SUPPORT


#if CFG_WMT_RESET_API_SUPPORT
extern int mtk_wcn_wmt_assert(enum ENUM_WMTDRV_TYPE type,
			      uint32_t reason);
extern int mtk_wcn_wmt_msgcb_reg(enum ENUM_WMTDRV_TYPE
				 eType, PF_WMT_CB pCb);
extern int mtk_wcn_wmt_msgcb_unreg(enum ENUM_WMTDRV_TYPE
				   eType);
extern int wifi_reset_start(void);
extern int wifi_reset_end(enum ENUM_RESET_STATUS);

#if CFG_ENABLE_KEYWORD_EXCEPTION_MECHANISM
extern int mtk_wcn_wmt_assert_keyword(enum ENUM_WMTDRV_TYPE type,
	unsigned char *keyword);
#endif
#endif
#endif

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
#if CFG_CHIP_RESET_SUPPORT
extern u_int8_t fgIsResetting;

#if CFG_CHIP_RESET_HANG
extern u_int8_t fgIsResetHangState;
#endif

#endif
/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define GL_COREDUMP_TRIGGER(_prAdapter)	\
{ \
	wlanoidSerExtCmd(_prAdapter, SER_ACTION_RECOVER, SER_SET_L0_RECOVER, \
			 0); \
}


#if CFG_CHIP_RESET_SUPPORT
#if CFG_WMT_RESET_API_SUPPORT
#define GL_RESET_TRIGGER(_prAdapter, _u4Flags) \
	glResetTrigger(_prAdapter, (_u4Flags), \
	(const uint8_t *)__FILE__, __LINE__)
#else
#define GL_RESET_TRIGGER(_prAdapter, _u4Flags) \
{ \
	if (eResetReason == RST_OID_TIMEOUT || \
		eResetReason == RST_FW_ASSERT || \
		eResetReason == RST_CMD_TRIGGER || \
		eResetReason == RST_BT_TRIGGER) { \
		glResetTrigger(_prAdapter, (_u4Flags), \
			(const uint8_t *)__FILE__, __LINE__); \
	} else { \
		GL_COREDUMP_TRIGGER(_prAdapter);	\
		DBGLOG(INIT, ERROR, "Trigger coredump in %s line %u!\n",  \
							__FILE__, __LINE__); \
	} \
}
#endif
#else
#define GL_RESET_TRIGGER(_prAdapter, _u4Flags) \
	DBGLOG(INIT, INFO, "DO NOT support chip reset\n")
#endif


/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
#if CFG_CHIP_RESET_SUPPORT
extern uint64_t u8ResetTime;
extern enum _ENUM_CHIP_RESET_REASON_TYPE_T eResetReason;

#if CFG_WMT_RESET_API_SUPPORT
extern int mtk_wcn_set_connsys_power_off_flag(int value);
extern int mtk_wcn_wmt_assert_timeout(enum ENUM_WMTDRV_TYPE
				      type, uint32_t reason, int timeout);
extern int mtk_wcn_wmt_do_reset(enum ENUM_WMTDRV_TYPE type);

/* WMT Core Dump Support */
extern u_int8_t mtk_wcn_stp_coredump_start_get(void);
#endif
#else

#endif
/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#if CFG_CHIP_RESET_SUPPORT
void glResetInit(struct GLUE_INFO *prGlueInfo);

void glResetUninit(void);

void glSendResetRequest(void);

u_int8_t kalIsResetting(void);

u_int8_t glResetTrigger(struct ADAPTER *prAdapter,
			uint32_t u4RstFlag, const uint8_t *pucFile,
			uint32_t u4Line);

void glGetRstReason(enum _ENUM_CHIP_RESET_REASON_TYPE_T
		    eReason);
#if CFG_WMT_RESET_API_SUPPORT
u_int8_t glIsWmtCodeDump(void);
#endif
#else

#endif
#endif /* _GL_RST_H */
