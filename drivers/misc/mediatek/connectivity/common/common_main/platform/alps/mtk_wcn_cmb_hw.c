/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/*! \file
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/




/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG "[WMT-CMB-HW]"


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "wmt_plat.h"
#include "wmt_lib.h"
#include "mtk_wcn_cmb_hw.h"
#include "osal_typedef.h"


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define DFT_RTC_STABLE_TIME 100
#define DFT_LDO_STABLE_TIME 100
#define DFT_RST_STABLE_TIME 30
#define DFT_OFF_STABLE_TIME 10
#define DFT_ON_STABLE_TIME 30

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/



/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

PWR_SEQ_TIME gPwrSeqTime;




/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/



/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

INT32 mtk_wcn_cmb_hw_pwr_off(VOID)
{
	INT32 iRet = 0;
	UINT32 chip_id = 0x0;

	WMT_INFO_FUNC("CMB-HW, hw_pwr_off start\n");

	/*1. disable irq --> should be done when do wmt-ic swDeinit period */
	/* TODO:[FixMe][GeorgeKuo] clarify this */

	/*2. set bgf eint/all eint to deinit state, namely input low state */
	chip_id = mtk_wcn_wmt_chipid_query();
	if (!((0x6630 == chip_id || 0x6632 == chip_id)
				&& (STP_SDIO_IF_TX == wmt_plat_get_comm_if_type()))) {
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS);
		WMT_INFO_FUNC("CMB-HW, BGF_EINT IRQ unregistered and disabled\n");
		iRet += wmt_plat_gpio_ctrl(PIN_BGF_EINT, PIN_STA_DEINIT);
	}
	/* 2.1 set ALL_EINT pin to correct state even it is not used currently */
	iRet += wmt_plat_eirq_ctrl(PIN_ALL_EINT, PIN_STA_DEINIT);
	WMT_INFO_FUNC("CMB-HW, ALL_EINT IRQ unregistered and disabled\n");
	iRet += wmt_plat_gpio_ctrl(PIN_ALL_EINT, PIN_STA_DEINIT);
	/* 2.2 deinit gps sync */
	iRet += wmt_plat_gpio_ctrl(PIN_GPS_SYNC, PIN_STA_DEINIT);

	/*3. set audio interface to CMB_STUB_AIF_0, BT PCM OFF, I2S OFF */
	iRet += wmt_plat_audio_ctrl(CMB_STUB_AIF_0, CMB_STUB_AIF_CTRL_DIS);

	/*4. set control gpio into deinit state, namely input low state */
	iRet += wmt_plat_gpio_ctrl(PIN_SDIO_GRP, PIN_STA_DEINIT);
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_OUT_L);
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_OUT_L);

	/*5. set uart tx/rx into deinit state, namely input low state */
	iRet += wmt_plat_gpio_ctrl(PIN_UART_GRP, PIN_STA_DEINIT);

	/* 6. Last, LDO output low */
	iRet += wmt_plat_gpio_ctrl(PIN_LDO, PIN_STA_OUT_L);

	/*7. deinit gps_lna */
	iRet += wmt_plat_gpio_ctrl(PIN_GPS_LNA, PIN_STA_DEINIT);

	WMT_INFO_FUNC("CMB-HW, hw_pwr_off finish\n");
	return iRet;
}

INT32 mtk_wcn_cmb_hw_pwr_on(VOID)
{
	static UINT32 _pwr_first_time = 1;
	INT32 iRet = 0;
	UINT32 chip_id = 0x0;

	WMT_DBG_FUNC("CMB-HW, hw_pwr_on start\n");

	/* disable interrupt firstly */
	chip_id = mtk_wcn_wmt_chipid_query();
	if (!((0x6630 == chip_id || 0x6632 == chip_id)
	      && (STP_SDIO_IF_TX == wmt_plat_get_comm_if_type())))
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS);
	iRet += wmt_plat_eirq_ctrl(PIN_ALL_EINT, PIN_STA_EINT_DIS);

	/*set all control and eint gpio to init state, namely input low mode */
	iRet += wmt_plat_gpio_ctrl(PIN_LDO, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_SDIO_GRP, PIN_STA_INIT);
	if (!((0x6630 == chip_id || 0x6632 == chip_id)
	      && (STP_SDIO_IF_TX == wmt_plat_get_comm_if_type())))
		iRet += wmt_plat_gpio_ctrl(PIN_BGF_EINT, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_ALL_EINT, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_GPS_SYNC, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_GPS_LNA, PIN_STA_INIT);
	/* wmt_plat_gpio_ctrl(PIN_WIFI_EINT, PIN_STA_INIT); *//* WIFI_EINT is controlled by SDIO host driver */
	/* TODO: [FixMe][George]:WIFI_EINT is used in common SDIO */

	/*1. pull high LDO to supply power to chip */
	iRet += wmt_plat_gpio_ctrl(PIN_LDO, PIN_STA_OUT_H);
	osal_sleep_ms(gPwrSeqTime.ldoStableTime);

	/* 2. export RTC clock to chip */
	if (_pwr_first_time) {
		/* rtc clock should be output all the time, so no need to enable output again */
		iRet += wmt_plat_gpio_ctrl(PIN_RTC, PIN_STA_INIT);
		osal_sleep_ms(gPwrSeqTime.rtcStableTime);
		WMT_INFO_FUNC("CMB-HW, rtc clock exported\n");
	}

	/*3. set UART Tx/Rx to UART mode */
	iRet += wmt_plat_gpio_ctrl(PIN_UART_GRP, PIN_STA_INIT);

	if (0x6630 == chip_id || 0x6632 == chip_id) {
		switch (wmt_plat_get_comm_if_type()) {
		case STP_UART_IF_TX:
			iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_OUT_H);
			break;
		case STP_SDIO_IF_TX:
				iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_OUT_L);
			break;
		default:
			WMT_ERR_FUNC("not supported common interface\n");
			break;
		}
	}
	/*4. PMU->output low, RST->output low, sleep off stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_OUT_L);
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_OUT_L);
	osal_sleep_ms(gPwrSeqTime.offStableTime);

	/*5. PMU->output high, sleep rst stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_OUT_H);
	osal_sleep_ms(gPwrSeqTime.rstStableTime);

	/*6. RST->output high, sleep on stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_OUT_H);
	osal_sleep_ms(gPwrSeqTime.onStableTime);

	/*set UART Tx/Rx to UART mode */
	if (0x6630 == chip_id || 0x6632 == chip_id)
		iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_IN_NP);


	/*7. set audio interface to CMB_STUB_AIF_1, BT PCM ON, I2S OFF */
	/* BT PCM bus default mode. Real control is done by audio */
	iRet += wmt_plat_audio_ctrl(CMB_STUB_AIF_1, CMB_STUB_AIF_CTRL_DIS);

	/*8. set EINT< -ommited-> move this to WMT-IC module,
	   where common sdio interface will be identified and do proper operation */
	/* TODO: [FixMe][GeorgeKuo] double check if BGF_INT is implemented ok */
	if (!((0x6630 == chip_id || 0x6632 == chip_id)
	      && (STP_SDIO_IF_TX == wmt_plat_get_comm_if_type()))) {
		iRet += wmt_plat_gpio_ctrl(PIN_BGF_EINT, PIN_STA_MUX);
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_INIT);
		WMT_INFO_FUNC("CMB-HW, BGF_EINT IRQ registered and disabled\n");
	} else {
		WMT_INFO_FUNC("CMB-HW, no need to register BGF_EINT for MT6630 & MT6632 SDIO mode\n");
	}

	/* 8.1 set ALL_EINT pin to correct state even it is not used currently */
	iRet += wmt_plat_gpio_ctrl(PIN_ALL_EINT, PIN_STA_MUX);
	iRet += wmt_plat_eirq_ctrl(PIN_ALL_EINT, PIN_STA_INIT);
	WMT_INFO_FUNC("CMB-HW, hw_pwr_on finish (%d)\n", iRet);

	_pwr_first_time = 0;
	return iRet;

}

INT32 mtk_wcn_cmb_hw_rst(VOID)
{
	INT32 iRet = 0;
	UINT32 chip_id = 0x0;

	WMT_INFO_FUNC("CMB-HW, hw_rst start, eirq should be disabled before this step\n");
	if (0x6630 == chip_id || 0x6632 == chip_id) {
		switch (wmt_plat_get_comm_if_type()) {
		case STP_UART_IF_TX:
			iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_OUT_H);
			break;
		case STP_SDIO_IF_TX:
				iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_OUT_L);
			break;
		default:
			WMT_ERR_FUNC("not supported common interface\n");
			break;
		}
	}

	/*1. PMU->output low, RST->output low, sleep off stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_OUT_L);
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_OUT_L);
	osal_sleep_ms(gPwrSeqTime.offStableTime);

	/*2. PMU->output high, sleep rst stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_OUT_H);
	osal_sleep_ms(gPwrSeqTime.rstStableTime);

	/*3. RST->output high, sleep on stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_OUT_H);
	osal_sleep_ms(gPwrSeqTime.onStableTime);

	/*set UART Tx/Rx to UART mode */
	if (0x6630 == chip_id || 0x6632 == chip_id)
		iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_IN_NP);

	WMT_INFO_FUNC("CMB-HW, hw_rst finish, eirq should be enabled after this step\n");
	return 0;
}

static VOID mtk_wcn_cmb_hw_dmp_seq(VOID)
{
	PUINT32 pTimeSlot = (PUINT32) &gPwrSeqTime;

	WMT_INFO_FUNC
	    ("combo chip power on sequence time, RTC (%d), LDO (%d), RST(%d), OFF(%d), ON(%d)\n",
	     pTimeSlot[0],
		      /**pTimeSlot++,*/
	     pTimeSlot[1], pTimeSlot[2], pTimeSlot[3], pTimeSlot[4]
	    );
}

INT32 mtk_wcn_cmb_hw_state_show(VOID)
{
	wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_SHOW);
	wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_SHOW);
	wmt_plat_gpio_ctrl(PIN_RTC, PIN_STA_SHOW);
	return 0;
}



INT32 mtk_wcn_cmb_hw_init(P_PWR_SEQ_TIME pPwrSeqTime)
{
	if (NULL != pPwrSeqTime &&
	    pPwrSeqTime->ldoStableTime > 0 &&
	    pPwrSeqTime->rtcStableTime > 0 &&
	    pPwrSeqTime->offStableTime > DFT_OFF_STABLE_TIME &&
	    pPwrSeqTime->onStableTime > DFT_ON_STABLE_TIME &&
	    pPwrSeqTime->rstStableTime > DFT_RST_STABLE_TIME) {
		/*memcpy may be more performance */
		WMT_DBG_FUNC("setting hw init sequence parameters\n");
		osal_memcpy(&gPwrSeqTime, pPwrSeqTime, osal_sizeof(gPwrSeqTime));
	} else {
		WMT_WARN_FUNC
		    ("invalid pPwrSeqTime parameter, use default hw init sequence parameters\n");
		gPwrSeqTime.ldoStableTime = DFT_LDO_STABLE_TIME;
		gPwrSeqTime.offStableTime = DFT_OFF_STABLE_TIME;
		gPwrSeqTime.onStableTime = DFT_ON_STABLE_TIME;
		gPwrSeqTime.rstStableTime = DFT_RST_STABLE_TIME;
		gPwrSeqTime.rtcStableTime = DFT_RTC_STABLE_TIME;
	}
	mtk_wcn_cmb_hw_dmp_seq();
	return 0;
}

INT32 mtk_wcn_cmb_hw_deinit(VOID)
{

	WMT_WARN_FUNC("mtk_wcn_cmb_hw_deinit start, set to default hw init sequence parameters\n");
	gPwrSeqTime.ldoStableTime = DFT_LDO_STABLE_TIME;
	gPwrSeqTime.offStableTime = DFT_OFF_STABLE_TIME;
	gPwrSeqTime.onStableTime = DFT_ON_STABLE_TIME;
	gPwrSeqTime.rstStableTime = DFT_RST_STABLE_TIME;
	gPwrSeqTime.rtcStableTime = DFT_RTC_STABLE_TIME;
	WMT_WARN_FUNC("mtk_wcn_cmb_hw_deinit finish\n");
	return 0;
}
