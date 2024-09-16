/******************************************************************************
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
** Id: @(#) gl_rst.c@@
*/

/*! \file   gl_rst.c
*    \brief  Main routines for supporintg MT6620 whole-chip reset mechanism
*
*    This file contains the support routines of Linux driver for MediaTek Inc. 802.11
*    Wireless LAN Adapters.
*/


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"
#include "gl_os.h"
#include <linux/timer.h>
#include <linux/mmc/card.h>

#if CFG_CHIP_RESET_SUPPORT

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#ifdef _HIF_SDIO
#define WAIT_CD_STAR_TIME	3
#define WAIT_CD_END_TIME	30

#define MT76x8_PMU_EN_PIN_NAME		"mt76x8_pmu_en_gpio"
#define MT76x8_PMU_EN_DELAY_NAME	"mt76x8_pmu_en_delay"
#define MT76x8_PMU_EN_DEFAULT_DELAY	(5) /* Default delay 5ms */

#define BT_NOTIFY	1
#define TIMEOUT_NOTIFY	2

#define NOTIFY_SUCCESS	0
#define RESETTING	1
#define NOTIFY_REPEAT	2
#endif

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
struct rst_struct rst_data;

#ifdef _HIF_SDIO

BOOLEAN g_fgIsCoreDumpStart = FALSE;
BOOLEAN g_fgIsCoreDumpEnd = FALSE;
BOOLEAN g_fgIsNotifyWlanRemove = FALSE;
BOOLEAN gl_sdio_fail = FALSE;

struct work_struct remove_work;


wait_queue_head_t wait_core_dump_start;
wait_queue_head_t wait_core_dump_end;
struct timer_list wait_CD_start_timer;
struct timer_list wait_CD_end_timer;

#endif

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
#ifdef _HIF_SDIO
BOOLEAN g_fgIsWifiTrig = FALSE;
BOOLEAN g_fgIsBTTrig = FALSE;

static BOOLEAN g_fgIsBTExist = TRUE;
static BOOLEAN g_fgIsCDEndTimeout = FALSE;
static BOOLEAN g_fgIsNotifyBTRemoveEnd = FALSE;

#endif
/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#ifdef _HIF_SDIO

#define IS_CORE_DUMP_START()	(g_fgIsCoreDumpStart)
#define IS_CORE_DUMP_END()	(g_fgIsCoreDumpEnd)

#endif

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
#ifdef _HIF_SDIO
static void resetInit(void);

static INT_32 pmu_toggle(struct rst_struct *data);

static void init_wait_CD_start_timer(void);
static void start_wait_CD_start_timer(void);
static void wait_CD_start_timeout(unsigned long data);

static void init_wait_CD_end_timer(void);
static void start_wait_CD_end_timer(void);
static void wait_CD_end_timeout(unsigned long data);
static void removeWlanSelf(struct work_struct *work);
static void probeWlanSelf(struct work_struct *work);

static void RSTClearResource(void);
static void RSTClearState(void);
static VOID set_core_dump_start(BOOLEAN fgVal);
static void RSTP2pDestroyWirelessDevice(void);

#endif



/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#ifdef _HIF_USB
VOID glResetTrigger(P_ADAPTER_T prAdapter)
{
	void (*btmtk_usb_toggle_rst_pin)(void);

	/* Call POR(Power On Reset) off->on API provided by BT driver */
	btmtk_usb_toggle_rst_pin = (void *)kallsyms_lookup_name("btmtk_usb_toggle_rst_pin");

	if (!btmtk_usb_toggle_rst_pin) {
		DBGLOG(HAL, ERROR, "btmtk_usb_toggle_rst_pin() is not found\n");
	} else {
		DBGLOG(HAL, ERROR, "Trigger MT7668 POR(Power On Reset) off->on by BT driver\n");
		btmtk_usb_toggle_rst_pin();
	}

}
#endif

#ifdef _HIF_SDIO
VOID glResetTrigger(P_ADAPTER_T prAdapter)
{
	int bet = 0;
	typedef int (*p_bt_fun_type) (int);
	p_bt_fun_type bt_func;

	/* int btmtk_sdio_bt_trigger_core_dump(int reserved); */
	char *bt_func_name = "btmtk_sdio_bt_trigger_core_dump";

	DBGLOG(HAL, INFO, "[RST] rst_mutex lock\n");
	mutex_lock(&(rst_data.rst_mutex));

	if (checkResetState()) {
		DBGLOG(HAL, STATE, "[RST] resetting...\n");
		mutex_unlock(&(rst_data.rst_mutex));
		DBGLOG(INIT, INFO, "[RST] rst_mutex unlock\n");
		return;
	}

	dump_stack();
	DBGLOG(HAL, STATE, "[RST] glResetTrigger start\n");
	g_fgIsWifiTrig = TRUE;

	mutex_unlock(&(rst_data.rst_mutex));
	DBGLOG(INIT, INFO, "[RST] rst_mutex unlock\n");

	resetInit();

	/* check bt module */
	bt_func = (p_bt_fun_type) kallsyms_lookup_name(bt_func_name);
	if (bt_func) {
		BOOLEAN is_coredump = (~(prAdapter->fgIsChipNoAck)) & 0x1;

		DBGLOG(INIT, STATE, "[RST] wifi driver trigger rst\n");
		DBGLOG(INIT, STATE, "[RST] is_coredump = %d\n", is_coredump);

#if CFG_ASSERT_DUMP
		bet = bt_func(is_coredump);
#else
		bet = bt_func(0);
#endif
		if (bet) {
			g_fgIsBTExist = FALSE;
			DBGLOG(INIT, ERROR, "[RST] bt driver is not ready\n");
			/* error handle not yet */
			goto RESET_START;
		} else {
			g_fgIsBTExist = TRUE;
			start_wait_CD_start_timer();
			DBGLOG(INIT, STATE,
				"[RST] wait bt core dump start...\n");

			if (!IS_CORE_DUMP_START()) {
				/* wait bt notify */
				wait_event(wait_core_dump_start,
					g_fgIsCoreDumpStart);
				DBGLOG(INIT, STATE,
					"[RST] wait_core_dump_start end\n");
			}
			bet = del_timer_sync(&wait_CD_start_timer);
			DBGLOG(INIT, INFO,
				"[RST] cancel wait_CD_start_timer=%d\n", bet);

			goto RESET_START;
		}
	} else {
		g_fgIsBTExist = FALSE;
		DBGLOG(INIT, ERROR, "[RST] %s: do not get %s\n", __func__,
			bt_func_name);
		goto RESET_START;

	}

RESET_START:

	/* checkResetState(); // for debug */
	schedule_work(&remove_work);
	DBGLOG(INIT, STATE, "[RST] creat remove_work\n");
}
#endif

#ifdef _HIF_PCIE
VOID glResetTrigger(P_ADAPTER_T prAdapter)
{
	DBGLOG(HAL, ERROR, "pcie does not support trigger core dump yet !!\n");
}
#endif

#ifdef _HIF_SDIO
/*----------------------------------------------------------------------------*/
/*!
* \brief .
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
void resetInit(void)
{
	DBGLOG(INIT, STATE, "[RST] resetInit start\n");

	g_fgIsCoreDumpStart = FALSE;
	g_fgIsCoreDumpEnd = FALSE;
	g_fgIsCDEndTimeout = FALSE;
	g_fgIsNotifyWlanRemove = FALSE;
	g_fgIsNotifyBTRemoveEnd = FALSE;

	INIT_WORK(&remove_work, removeWlanSelf);
	INIT_WORK(&(rst_data.rst_work), probeWlanSelf);

	init_waitqueue_head(&wait_core_dump_start);
	/* init_waitqueue_head(&wait_core_dump_end); */
	init_wait_CD_start_timer();
	init_wait_CD_end_timer();

	DBGLOG(INIT, STATE, "[RST] resetInit end\n");

}
static void RSTClearResource(void)
{
	DBGLOG(INIT, STATE, "[RST] resetClear start\n");

	del_timer_sync(&wait_CD_start_timer);
	del_timer_sync(&wait_CD_end_timer);

	/* cancel_work_sync(&rst_data.rst_work); */
	cancel_work_sync(&remove_work);

	gl_sdio_fail = FALSE;
	DBGLOG(INIT, STATE, "[RST] resetClear end\n");
}

static void RSTClearState(void)
{
	DBGLOG(INIT, STATE, "[RST] RSTClearState\n");
	g_fgIsWifiTrig = FALSE;
	g_fgIsBTTrig = FALSE;
	g_fgIsNotifyWlanRemove = FALSE;
}

void removeWlanSelf(struct work_struct *work)
{
	int bet = 0;
	typedef int (*p_bt_fun_type) (void);
	p_bt_fun_type bt_func;
	/* int btmtk_sdio_notify_wlan_remove_end(void) */
	char *bt_func_name = "btmtk_sdio_notify_wlan_remove_end";

	DBGLOG(INIT, STATE, "[RST] check entry_conut = %d\n",
						rst_data.entry_conut);

	while (rst_data.entry_conut != 0)
		kalMsleep(100);

	DBGLOG(INIT, STATE, "[RST] start mtk_sdio_remove\n");
#if MTK_WCN_HIF_SDIO
	bet = mtk_sdio_remove();
#else
	mtk_sdio_remove(rst_data.func);
#endif
	DBGLOG(INIT, STATE, "[RST] mtk_sdio_remove end\n");

	DBGLOG(INIT, STATE, "[RST] RSTP2pDestroyWirelessDevice start\n");
	RSTP2pDestroyWirelessDevice();
	DBGLOG(INIT, STATE, "[RST] RSTP2pDestroyWirelessDevice end\n");

	/* notify bt wifi remove end */
	bt_func = (p_bt_fun_type) kallsyms_lookup_name(bt_func_name);
	if (bt_func) {

		DBGLOG(INIT, STATE, "[RST] notify bt remove...\n");
		bet = bt_func();
		if (bet) {
			g_fgIsBTExist = FALSE;
			DBGLOG(INIT, ERROR, "[RST] notify bt fail\n");
			notify_wlan_toggle_rst_end(0);
		} else {
			g_fgIsNotifyBTRemoveEnd = TRUE;
			DBGLOG(INIT, STATE, "[RST] notify bt remove end\n");
			start_wait_CD_end_timer();
		}
	} else {
		g_fgIsBTExist = FALSE;
		DBGLOG(INIT, ERROR, "[RST] do not get %s\n",
			bt_func_name);
		notify_wlan_toggle_rst_end(0);
	}

}
void probeWlanSelf(struct work_struct *work)
{
	INT_32 i4Status = 0;

	DBGLOG(INIT, INFO, "[RST] g_fgIsBTExist=%d\n", g_fgIsBTExist);
	DBGLOG(INIT, INFO, "[RST] g_fgIsCDEndTimeout=%d\n",
							g_fgIsCDEndTimeout);
	DBGLOG(INIT, INFO, "[RST] g_fgIsNotifyBTRemoveEnd=%d\n",
						g_fgIsNotifyBTRemoveEnd);

	while (g_fgIsNotifyBTRemoveEnd == FALSE && g_fgIsBTExist == TRUE)
		kalMsleep(10);

	DBGLOG(INIT, STATE, "[RST] probeWlanSelf start\n");

	if (g_fgIsBTExist == FALSE ||  g_fgIsCDEndTimeout == TRUE) {

		DBGLOG(INIT, WARN, "[RST] sdio_claim_host\n");
		sdio_claim_host(rst_data.func);

		i4Status = pmu_toggle(&rst_data);
		if (i4Status) {
			DBGLOG(INIT, WARN,
				"[RST] pmu_toggle fail num=%d!\n"
				, i4Status);
			return;
		}
		kalMsleep(500);

		DBGLOG(INIT, WARN, "[RST] sdio_reset_comm\n");
		i4Status = sdio_reset_comm(rst_data.func->card);
		if (i4Status)
			DBGLOG(INIT, ERROR,
				"[RST] sdio_reset_comm, err=%d\n", i4Status);
		kalMsleep(1000);

		sdio_release_host(rst_data.func);
		DBGLOG(INIT, WARN, "[RST] sdio_release_host\n");
	}

	RSTClearResource();

	DBGLOG(INIT, STATE, "[RST] mtk_sdio_probe start\n");
#if MTK_WCN_HIF_SDIO
	i4Status = mtk_sdio_probe(rst_data.func, &mtk_sdio_ids[1]);
#else
	i4Status = mtk_sdio_probe(rst_data.func, &mtk_sdio_ids[1]);
#endif
	if (i4Status) {
		DBGLOG(INIT, ERROR, "[RST] mtk_sdio_probe fail num=%d!\n"
			, i4Status);
	}


	RSTClearState();

	DBGLOG(INIT, STATE, "[RST] resetWlanSelf end\n");
}
void init_wait_CD_start_timer(void)
{
	DBGLOG(INIT, INFO, "[RST] init CD start timer\n");
	init_timer(&wait_CD_start_timer);
	wait_CD_start_timer.function = wait_CD_start_timeout;
}

void start_wait_CD_start_timer(void)
{
	DBGLOG(INIT, LOUD, "[RST] create CD start timer\n");

	wait_CD_start_timer.expires = jiffies + (WAIT_CD_STAR_TIME * HZ);
	wait_CD_start_timer.data = ((unsigned long) 0);
	add_timer(&wait_CD_start_timer);
}

void wait_CD_start_timeout(unsigned long data)
{
	DBGLOG(INIT, ERROR, "[RST] timeout=%d\n", WAIT_CD_STAR_TIME);
	DBGLOG(INIT, ERROR, "[RST] g_fgIsCoreDumpStart = %d\n",
						g_fgIsCoreDumpStart);
	DBGLOG(INIT, ERROR, "[RST] wake up glResetTrigger\n");
	wake_up(&wait_core_dump_start);
	set_core_dump_start(TRUE);
	g_fgIsBTExist = FALSE;
}

void init_wait_CD_end_timer(void)
{
	DBGLOG(INIT, INFO, "[RST] init CD end timer\n");
	init_timer(&wait_CD_end_timer);
	wait_CD_end_timer.function = wait_CD_end_timeout;
}

void start_wait_CD_end_timer(void)
{
	DBGLOG(INIT, STATE, "[RST] create CD end timer\n");
	wait_CD_end_timer.expires = jiffies + (WAIT_CD_END_TIME * HZ);
	wait_CD_end_timer.data = ((unsigned long) 0);
	add_timer(&wait_CD_end_timer);
}
void wait_CD_end_timeout(unsigned long data)
{
	DBGLOG(INIT, ERROR, "[RST] timeout=%ld\n", WAIT_CD_END_TIME);
	DBGLOG(INIT, ERROR, "[RST] g_fgIsCoreDumpEnd = %d\n",
		g_fgIsCoreDumpEnd);

	if (g_fgIsCoreDumpEnd == FALSE) {
		g_fgIsCDEndTimeout = TRUE;
		g_fgIsCoreDumpEnd = TRUE;
		schedule_work(&rst_data.rst_work);
	}
}

BOOLEAN checkResetState(void)
{
	return (g_fgIsWifiTrig | g_fgIsBTTrig);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief .
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID set_core_dump_start(BOOLEAN fgVal)
{
	DBGLOG(INIT, ERROR, "[RST] set_core_dump_start = %d\n", fgVal);
	g_fgIsCoreDumpStart = fgVal;
}
INT_32 bt_notify_wlan_remove(INT_32 reserved)
{
	if (g_fgIsNotifyWlanRemove) {
		DBGLOG(INIT, ERROR, "[RST] g_fgIsNotifyWlanRemove=%d\n",
			g_fgIsNotifyWlanRemove);
		return NOTIFY_REPEAT;
	}
	g_fgIsNotifyWlanRemove = TRUE;
	DBGLOG(INIT, ERROR, "[RST] bt notify cd start...\n");
	if (g_fgIsWifiTrig) {
		DBGLOG(INIT, ERROR, "[RST] wake up glResetTrigger\n");
		set_core_dump_start(TRUE);
		wake_up(&wait_core_dump_start);
	} else {
		g_fgIsBTTrig = TRUE;
		g_fgIsBTExist = TRUE;
		resetInit();
		set_core_dump_start(TRUE);
		schedule_work(&remove_work);
		DBGLOG(INIT, ERROR, "[RST] creat remove_work\n");
	}
	DBGLOG(INIT, ERROR, "[RST] bt notify cd start end\n");
	return NOTIFY_SUCCESS;

}

INT_32 notify_wlan_remove_start(INT_32 reserved)
{
	return bt_notify_wlan_remove(reserved);
}
EXPORT_SYMBOL(notify_wlan_remove_start);
/*----------------------------------------------------------------------------*/
/*!
* \brief .
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID set_core_dump_end(BOOLEAN fgVal)
{
	DBGLOG(INIT, INFO, "[RST] g_fgIsCoreDumpEnd = %d\n", fgVal);
	g_fgIsCoreDumpEnd = fgVal;
}

INT_32 notify_wlan_toggle_rst_end(INT_32 reserved)
{
	/* create work queue */
	DBGLOG(INIT, STATE, "[RST] bt notify CD end\n");
	set_core_dump_end(TRUE);
	schedule_work(&rst_data.rst_work);
	DBGLOG(INIT, STATE, "[RST] create probe work\n");

	/* cencel timer */
	del_timer_sync(&wait_CD_end_timer);

	return NOTIFY_SUCCESS;
}
EXPORT_SYMBOL(notify_wlan_toggle_rst_end);

INT_32 pmu_toggle(struct rst_struct *data)
{
	UINT_32 pmu_en_delay = MT76x8_PMU_EN_DEFAULT_DELAY;
	int pmu_en;
	struct device *prDev;
	INT_32 i4Status = 0;

	typedef void (*p_sdio_fun_type) (int);
	p_sdio_fun_type sdio_func;
	/* int btmtk_sdio_notify_wlan_remove_end(void) */
	char *sdio_func_name = "sdio_set_card_clkpd";

	ASSERT(data);

	/* stop mtk sdio clk */
	sdio_func = (p_sdio_fun_type) kallsyms_lookup_name(sdio_func_name);
	if (sdio_func) {
		sdio_func(0);
		DBGLOG(INIT, STATE, "[RST] stop sdio clk\n");
	} else {
		DBGLOG(INIT, ERROR, "[RST] do not get %s\n", sdio_func);
	}

	DBGLOG(INIT, STATE, "[RST] get device\n");
	prDev = mmc_dev(data->func->card->host);
	if (!prDev) {
		DBGLOG(INIT, ERROR, "unable to get struct dev for wlan\n");
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(INIT, STATE, "[RST] pmu reset\n");
	pmu_en = of_get_named_gpio(prDev->of_node, MT76x8_PMU_EN_PIN_NAME, 0);

	if (gpio_is_valid(pmu_en)) {
		i4Status = of_property_read_u32(prDev->of_node,
					   MT76x8_PMU_EN_DELAY_NAME,
					   &pmu_en_delay);
		if (i4Status) {
			DBGLOG(INIT, ERROR,
			"[RST] undefined pmu_en delay, use default %ums\n",
				i4Status);
		}
		gpio_direction_output(pmu_en, 0);
		mdelay(pmu_en_delay);
		gpio_direction_output(pmu_en, 1);
	} else {
		DBGLOG(INIT, ERROR, "[RST] invalid gpio %s\n",
			MT76x8_PMU_EN_PIN_NAME);
		return WLAN_STATUS_FAILURE;
	}

	return WLAN_STATUS_SUCCESS;
}

void RSTP2pDestroyWirelessDevice(void)
{
	int i = 0;

	for (i = 1; i < KAL_P2P_NUM; i++) {

		if (gprP2pRoleWdev[i] == NULL)
			continue;

		DBGLOG(INIT, STATE,
			"glP2pDestroyWirelessDevice[%d] (0x%p)\n",
					i, gprP2pRoleWdev[i]->wiphy);
		set_wiphy_dev(gprP2pRoleWdev[i]->wiphy, NULL);
		wiphy_unregister(gprP2pRoleWdev[i]->wiphy);
		wiphy_free(gprP2pRoleWdev[i]->wiphy);
		kfree(gprP2pRoleWdev[i]);

		gprP2pRoleWdev[i] = NULL;
	}

}


#else
BOOLEAN checkResetState(void)
{
	return FALSE;
}
#endif
#endif /* CFG_CHIP_RESET_SUPPORT */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is called for checking if connectivity chip is resetting
 *
 * @param   None
 *
 * @retval  TRUE
 *          FALSE
 */
/*----------------------------------------------------------------------------*/
BOOLEAN kalIsResetting(VOID)
{
	return FALSE;
}
