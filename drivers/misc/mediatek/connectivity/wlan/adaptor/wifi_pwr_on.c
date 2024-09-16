
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


#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include "wifi_pwr_on.h"





MODULE_LICENSE("Dual BSD/GPL");

#define PFX                         "[WIFI-FW] "
#define WIFI_FW_LOG_DBG             3
#define WIFI_FW_LOG_INFO            2
#define WIFI_FW_LOG_WARN            1
#define WIFI_FW_LOG_ERR             0

uint32_t DbgLevel = WIFI_FW_LOG_INFO;

#define WIFI_DBG_FUNC(fmt, arg...)	\
	do { \
		if (DbgLevel >= WIFI_FW_LOG_DBG) \
			pr_info(PFX "%s[D]: " fmt, __func__, ##arg); \
	} while (0)
#define WIFI_INFO_FUNC(fmt, arg...)	\
	do { \
		if (DbgLevel >= WIFI_FW_LOG_INFO) \
			pr_info(PFX "%s[I]: " fmt, __func__, ##arg); \
	} while (0)
#define WIFI_INFO_FUNC_LIMITED(fmt, arg...)	\
	do { \
		if (DbgLevel >= WIFI_FW_LOG_INFO) \
			pr_info_ratelimited(PFX "%s[L]: " fmt, __func__, ##arg); \
	} while (0)
#define WIFI_WARN_FUNC(fmt, arg...)	\
	do { \
		if (DbgLevel >= WIFI_FW_LOG_WARN) \
			pr_info(PFX "%s[W]: " fmt, __func__, ##arg); \
	} while (0)
#define WIFI_ERR_FUNC(fmt, arg...)	\
	do { \
		if (DbgLevel >= WIFI_FW_LOG_ERR) \
			pr_info(PFX "%s[E]: " fmt, __func__, ##arg); \
	} while (0)


wlan_probe_cb mtk_wlan_probe_function;
wlan_remove_cb mtk_wlan_remove_function;


struct completion wlan_pendComp;

int g_data;

wait_queue_head_t g_waitq_onoff;
unsigned long g_ulOnoffFlag;
struct completion g_RstOffComp;
bool g_fgIsWiFiOn;
struct task_struct *wland_thread;

static int mtk_wland_thread_main(void *data);

int wifi_pwr_on_init(void)
{
	int result = 0;

	init_completion(&wlan_pendComp);
	init_waitqueue_head(&g_waitq_onoff);

	wland_thread = kthread_run(mtk_wland_thread_main,
				NULL, "mtk_wland_thread");
	WIFI_INFO_FUNC("Do wifi_pwr_on_init.\n");
	return result;
}
EXPORT_SYMBOL(wifi_pwr_on_init);

int wifi_pwr_on_deinit(void)
{
	WIFI_INFO_FUNC("Do wifi_pwr_on_deinit.\n");
	set_bit(ADAPTOR_FLAG_HALT_BIT, &g_ulOnoffFlag);
	wake_up_interruptible(&g_waitq_onoff);
	return 0;
}
EXPORT_SYMBOL(wifi_pwr_on_deinit);
int mtk_wcn_wlan_reg(struct MTK_WCN_WLAN_CB_INFO *pWlanCbInfo)
{
	if (!pWlanCbInfo) {
		WIFI_ERR_FUNC("wlan cb info in null!\n");
		return -1;
	}
	WIFI_INFO_FUNC("wmt wlan cb register\n");
	mtk_wlan_probe_function = pWlanCbInfo->wlan_probe_cb;
	mtk_wlan_remove_function = pWlanCbInfo->wlan_remove_cb;

	return 0;
}
EXPORT_SYMBOL(mtk_wcn_wlan_reg);

int mtk_wcn_wlan_unreg(void)
{
	WIFI_INFO_FUNC("wmt wlan cb unregister\n");
	mtk_wlan_probe_function = NULL;
	mtk_wlan_remove_function = NULL;

	return 0;
}
EXPORT_SYMBOL(mtk_wcn_wlan_unreg);

void kalSetOnoffEvent(enum ENUM_WLAN_OPID opId)
{

	if (opId == WLAN_OPID_FUNC_ON)
		set_bit(ADAPTOR_FLAG_ON_BIT, &g_ulOnoffFlag);
	else if (opId == WLAN_OPID_FUNC_OFF)
		set_bit(ADAPTOR_FLAG_OFF_BIT, &g_ulOnoffFlag);
	else {
		WIFI_ERR_FUNC("Invalid OPID\n");
		return;
	}
	/* when we got event, we wake up servie thread */
	wake_up_interruptible(&g_waitq_onoff);
}


int mtk_wland_thread_main(void *data)
{
	int ret = 0;

	WIFI_INFO_FUNC("%s:%u starts running...\n",
					current->comm, current->pid);
	while (1) {

		/*
		 * sleep on waitqueue if no events occurred. Event contain
		 * (1) ADAPTOR_FLAG_HALT (2) ADAPTOR_FLAG_ON
		 * (3) ADAPTOR_FLAG_OFF
		 */
		do {
			ret = wait_event_interruptible(g_waitq_onoff,
				((g_ulOnoffFlag & ADAPTOR_FLAG_ON_OFF_PROCESS)
				!= 0));
		} while (ret != 0);

		if (test_and_clear_bit(ADAPTOR_FLAG_HALT_BIT, &g_ulOnoffFlag)) {
			WIFI_INFO_FUNC("mtk_wland_thread should stop now...\n");
			break;
		}

		if (test_and_clear_bit(ADAPTOR_FLAG_ON_BIT, &g_ulOnoffFlag)) {
			if (!g_fgIsWiFiOn) {
				if (mtk_wlan_probe_function != NULL) {
					while (get_pre_cal_status() == 1) {
						WIFI_DBG_FUNC("Precal is ongoing.\n");
						msleep(300);
					}
					g_data = (*mtk_wlan_probe_function)();
					if (g_data == 0)
						g_fgIsWiFiOn = MTK_WCN_BOOL_TRUE;
				} else {
					g_data = ADAPTOR_INVALID_POINTER;
					WIFI_ERR_FUNC("Invalid pointer\n");
				}
			} else {
				WIFI_ERR_FUNC("Wi-Fi is already on\n");
			}
		} else if (test_and_clear_bit(ADAPTOR_FLAG_OFF_BIT, &g_ulOnoffFlag)) {
			if (g_fgIsWiFiOn) {
				if (mtk_wlan_remove_function != NULL) {
					g_data = (*mtk_wlan_remove_function)();
					if (g_data == 0)
						g_fgIsWiFiOn = MTK_WCN_BOOL_FALSE;
				} else {
					g_data = ADAPTOR_INVALID_POINTER;
					WIFI_ERR_FUNC("Invalid pointer\n");
				}
			} else {
				WIFI_ERR_FUNC("Wi-Fi is already off\n");
			}
		}
		complete(&wlan_pendComp);
	}

	WIFI_INFO_FUNC("%s:%u stopped!\n", current->comm, current->pid);

	return 0;
}

int mtk_wcn_wlan_func_ctrl(enum ENUM_WLAN_OPID opId)
{
	bool bRet = MTK_WCN_BOOL_TRUE;
	uint32_t waitRet = 0;
	reinit_completion(&wlan_pendComp);

	kalSetOnoffEvent(opId);
	waitRet = wait_for_completion_timeout(&wlan_pendComp, MSEC_TO_JIFFIES(WIFI_PWR_ON_TIMEOUT));
	if (waitRet > 0) {
		/* Case 1: No timeout. */
		if (g_data != 0)
			bRet = MTK_WCN_BOOL_FALSE;
	} else {
		/* Case 2: timeout */
		WIFI_ERR_FUNC("WiFi on/off takes more than %d seconds\n", WIFI_PWR_ON_TIMEOUT/1000);
		bRet = MTK_WCN_BOOL_FALSE;
	}
	return bRet;
}
EXPORT_SYMBOL(mtk_wcn_wlan_func_ctrl);




