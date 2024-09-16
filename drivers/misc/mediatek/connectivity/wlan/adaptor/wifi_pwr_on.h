/*
* Copyright (C) 2019 MediaTek Inc.
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

#ifndef _WIFI_PWR_ON_H_
#define _WIFI_PWR_ON_H_

int wifi_pwr_on_init(void);
int wifi_pwr_on_deinit(void);

typedef int(*wlan_probe_cb) (void);
typedef int(*wlan_remove_cb) (void);


struct MTK_WCN_WLAN_CB_INFO {
	wlan_probe_cb wlan_probe_cb;
	wlan_remove_cb wlan_remove_cb;
};
extern int mtk_wcn_wlan_reg(struct MTK_WCN_WLAN_CB_INFO *pWlanCbInfo);

extern uint8_t get_pre_cal_status(void);

enum ENUM_WLAN_OPID {
	WLAN_OPID_FUNC_ON = 0,
	WLAN_OPID_FUNC_OFF = 1,
	WLAN_OPID_MAX
};
extern int mtk_wcn_wlan_func_ctrl(enum ENUM_WLAN_OPID opId);

extern wlan_probe_cb mtk_wlan_probe_function;
extern wlan_remove_cb mtk_wlan_remove_function;

extern bool g_fgIsWiFiOn;

#ifndef MTK_WCN_BOOL_TRUE
#define MTK_WCN_BOOL_FALSE               ((int) 0)
#define MTK_WCN_BOOL_TRUE                ((int) 1)
#endif

#define MSEC_TO_JIFFIES(_msec)      msecs_to_jiffies(_msec)

#define WIFI_PWR_ON_TIMEOUT 10000

#define ADAPTOR_FLAG_HALT    BIT(0)
#define ADAPTOR_FLAG_ON      BIT(1)
#define ADAPTOR_FLAG_OFF     BIT(2)

#define ADAPTOR_FLAG_HALT_BIT      (0)
#define ADAPTOR_FLAG_ON_BIT        (1)
#define ADAPTOR_FLAG_OFF_BIT       (2)

#define ADAPTOR_FLAG_ON_OFF_PROCESS (ADAPTOR_FLAG_HALT |\
				     ADAPTOR_FLAG_ON |\
				     ADAPTOR_FLAG_OFF)
#define ADAPTOR_INVALID_POINTER 0xdeaddead
#endif /*_WIFI_PWR_ON_H_*/
