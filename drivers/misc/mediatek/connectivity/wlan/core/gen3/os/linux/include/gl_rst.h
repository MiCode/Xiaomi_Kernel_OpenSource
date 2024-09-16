/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/include/gl_rst.h#1
 */

/*
 * ! \file   gl_rst.h
 *   \brief  Declaration of functions and finite state machine for
 *   MT6620 Whole-Chip Reset Mechanism
 */

#ifndef _GL_RST_H
#define _GL_RST_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_typedef.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define RST_FLAG_CHIP_RESET          0
#define RST_FLAG_DO_CORE_DUMP        BIT(0)
#define RST_FLAG_PREVENT_POWER_OFF   BIT(1)
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef enum _ENUM_RESET_STATUS_T {
	RESET_FAIL,
	RESET_SUCCESS
} ENUM_RESET_STATUS_T;

typedef struct _RESET_STRUCT_T {
	ENUM_RESET_STATUS_T rst_data;
	struct work_struct rst_work;
	struct work_struct rst_trigger_work;
	UINT_32 rst_trigger_flag;
} RESET_STRUCT_T;

#if CFG_CHIP_RESET_SUPPORT

/* duplicated from wmt_exp.h for better driver isolation */
enum ENUM_WMTDRV_TYPE {
	WMTDRV_TYPE_BT = 0,
	WMTDRV_TYPE_FM = 1,
	WMTDRV_TYPE_GPS = 2,
	WMTDRV_TYPE_WIFI = 3,
	WMTDRV_TYPE_WMT = 4,
	WMTDRV_TYPE_ANT = 5,
	WMTDRV_TYPE_STP = 6,
	WMTDRV_TYPE_SDIO1 = 7,
	WMTDRV_TYPE_SDIO2 = 8,
	WMTDRV_TYPE_LPBK = 9,
	WMTDRV_TYPE_COREDUMP = 10,
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

typedef VOID(*PF_WMT_CB) (enum ENUM_WMTDRV_TYPE,	/* Source driver type */
			  enum ENUM_WMTDRV_TYPE,	/* Destination driver type */
			  enum ENUM_WMTMSG_TYPE,	/* Message type */
				/* READ-ONLY buffer. Buffer is allocated and freed by WMT_drv.
				 * Client can't touch this buffer after this function return.
				 */
			  PVOID,
			  UINT32	/* Buffer size in unit of byte */
			  );

enum ENUM_WMTRSTMSG_TYPE {
	WMTRSTMSG_RESET_START = 0x0,
	WMTRSTMSG_RESET_END = 0x1,
	WMTRSTMSG_RESET_END_FAIL = 0x2,
	WMTRSTMSG_RESET_MAX,
	WMTRSTMSG_RESET_INVALID = 0xff
};
#endif

struct MTK_WCN_WMT_WLAN_CB_INFO {
		INT_32(*wlan_probe_cb) (VOID);
		INT_32(*wlan_remove_cb) (VOID);
		INT_32(*wlan_bus_cnt_get_cb) (VOID);
		INT_32(*wlan_bus_cnt_clr_cb) (VOID);
		INT_32(*wlan_emi_mpu_set_protection_cb) (BOOLEAN);
		INT_32(*wlan_is_wifi_drv_own_cb) (VOID);
};

/*******************************************************************************
*                    E X T E R N A L   F U N C T I O N S
********************************************************************************
*/

#if CFG_CHIP_RESET_SUPPORT
extern int wifi_reset_start(VOID);
extern int wifi_reset_end(ENUM_RESET_STATUS_T);
extern INT_32 mtk_wcn_wmt_msgcb_unreg(enum ENUM_WMTDRV_TYPE eType);
extern INT_32 mtk_wcn_wmt_msgcb_reg(enum ENUM_WMTDRV_TYPE eType, PF_WMT_CB pCb);
extern INT_32 mtk_wcn_set_connsys_power_off_flag(INT_32 value);
extern INT_32 mtk_wcn_wmt_assert_timeout(enum ENUM_WMTDRV_TYPE type, UINT32 reason, INT_32 timeout);
extern INT_32 mtk_wcn_wmt_do_reset(enum ENUM_WMTDRV_TYPE type);
#endif

extern UINT32 wmt_plat_read_cpupcr(VOID);
extern INT_32 mtk_wcn_wmt_wlan_reg(struct MTK_WCN_WMT_WLAN_CB_INFO *pWmtWlanCbInfo);
extern INT_32 mtk_wcn_wmt_wlan_unreg(VOID);



/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#if CFG_CHIP_RESET_SUPPORT
#define GL_RESET_TRIGGER(_prAdapter, _u4Flags) \
	glResetTrigger(_prAdapter, (_u4Flags), (const PUINT_8)__FILE__, __LINE__)
#else
#define GL_RESET_TRIGGER(_prAdapter, _u4Flags) \
	DBGLOG(INIT, INFO, "DO NOT support chip reset\n")
#endif
/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID glResetInit(VOID);

VOID glResetUninit(VOID);

VOID glSendResetRequest(VOID);

BOOLEAN kalIsResetting(VOID);

BOOLEAN kalIsResetTriggered(VOID);

BOOLEAN glResetTrigger(P_ADAPTER_T prAdapter, UINT_32 u4RstFlag, const PUINT_8 pucFile, UINT_32 u4Line);

UINT32 wlanPollingCpupcr(UINT32 u4Times, UINT32 u4Sleep);

WLAN_STATUS wlanGetCpupcr(PUINT32 pu4Cpupcr);

#endif /* _GL_RST_H */
