/*
* Copyright (C) 2011-2014 MediaTek Inc.
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

#include <mach/mtk_rtc.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-DETECT]"

#include "wmt_detect.h"



#define INVALID_PIN_ID (0xFFFFFFFF)

#ifdef GPIO_COMBO_6620_LDO_EN_PIN
	#define COMBO_LDO_PIN GPIO_COMBO_6620_LDO_EN_PIN
#else
	#define COMBO_LDO_PIN INVALID_PIN_ID
#endif

#ifdef GPIO_COMBO_PMU_EN_PIN
	#define COMBO_PMU_PIN GPIO_COMBO_PMU_EN_PIN
#else
	#define COMBO_PMU_PIN INVALID_PIN_ID
#endif

#ifdef GPIO_COMBO_PMUV28_EN_PIN
	#define COMBO_PMUV28_PIN GPIO_COMBO_PMUV28_EN_PIN
#else
	#define COMBO_PMUV28_PIN INVALID_PIN_ID
#endif

#ifdef GPIO_COMBO_RST_PIN
	#define COMBO_RST_PIN GPIO_COMBO_RST_PIN
#else
	#define COMBO_RST_PIN INVALID_PIN_ID
#endif

#ifdef GPIO_WIFI_EINT_PIN
	#define COMBO_WIFI_EINT_PIN GPIO_WIFI_EINT_PIN
#else
	#define COMBO_WIFI_EINT_PIN INVALID_PIN_ID
#endif

#ifdef CONFIG_MTK_COMBO_COMM_NPWR
#ifdef GPIO_COMBO_I2S_DAT_PIN
	#define COMBO_I2S_DAT_PIN GPIO_COMBO_I2S_DAT_PIN
#else
	#define COMBO_I2S_DAT_PIN INVALID_PIN_ID
#endif

#ifdef GPIO_PCM_DAISYNC_PIN
	#define COMBO_PCM_SYNC_PIN GPIO_PCM_DAISYNC_PIN
#else
	#define COMBO_PCM_SYNC_PIN INVALID_PIN_ID
#endif

#ifdef GPIO_PCM_DAIPCMOUT_PIN
	#define COMBO_PCM_OUT_PIN GPIO_PCM_DAIPCMOUT_PIN
#else
	#define COMBO_PCM_OUT_PIN INVALID_PIN_ID
#endif

#ifdef GPIO_PCM_DAIPCMIN_PIN
	#define COMBO_PCM_IN_PIN GPIO_PCM_DAIPCMIN_PIN
#else
	#define COMBO_PCM_IN_PIN INVALID_PIN_ID
#endif

#ifdef GPIO_PCM_DAICLK_PIN
	#define COMBO_PCM_CLK_PIN GPIO_PCM_DAICLK_PIN
#else
	#define COMBO_PCM_CLK_PIN INVALID_PIN_ID
#endif
#endif

/*copied form WMT module*/
static int wmt_detect_dump_pin_conf (void)
{
    WMT_DETECT_INFO_FUNC( "[WMT-DETECT]=>dump wmt pin configuration start<=\n");

    #ifdef GPIO_COMBO_6620_LDO_EN_PIN
        WMT_DETECT_INFO_FUNC( "LDO(GPIO%d)\n", GPIO_COMBO_6620_LDO_EN_PIN);
    #else
        WMT_DETECT_INFO_FUNC( "LDO(not defined)\n");
    #endif

    #ifdef GPIO_COMBO_PMU_EN_PIN
        WMT_DETECT_INFO_FUNC( "PMU(GPIO%d)\n", GPIO_COMBO_PMU_EN_PIN);
    #else
        WMT_DETECT_INFO_FUNC( "PMU(not defined)\n");
    #endif

    #ifdef GPIO_COMBO_PMUV28_EN_PIN
        WMT_DETECT_INFO_FUNC( "PMUV28(GPIO%d)\n", GPIO_COMBO_PMUV28_EN_PIN);
    #else
        WMT_DETECT_INFO_FUNC( "PMUV28(not defined)\n");
    #endif

    #ifdef GPIO_COMBO_RST_PIN
        WMT_DETECT_INFO_FUNC( "RST(GPIO%d)\n", GPIO_COMBO_RST_PIN);
    #else
        WMT_DETECT_INFO_FUNC( "RST(not defined)\n");
    #endif

    #ifdef GPIO_COMBO_BGF_EINT_PIN
        WMT_DETECT_INFO_FUNC( "BGF_EINT(GPIO%d)\n", GPIO_COMBO_BGF_EINT_PIN);
    #else
        WMT_DETECT_INFO_FUNC( "BGF_EINT(not defined)\n");
    #endif

    #ifdef CUST_EINT_COMBO_BGF_NUM
        WMT_DETECT_INFO_FUNC( "BGF_EINT_NUM(%d)\n", CUST_EINT_COMBO_BGF_NUM);
    #else
        WMT_DETECT_INFO_FUNC( "BGF_EINT_NUM(not defined)\n");
    #endif

    #ifdef GPIO_WIFI_EINT_PIN
        WMT_DETECT_INFO_FUNC( "WIFI_EINT(GPIO%d)\n", GPIO_WIFI_EINT_PIN);
    #else
        WMT_DETECT_INFO_FUNC( "WIFI_EINT(not defined)\n");
    #endif

    #ifdef CUST_EINT_WIFI_NUM
        WMT_DETECT_INFO_FUNC( "WIFI_EINT_NUM(%d)\n", CUST_EINT_WIFI_NUM);
    #else
        WMT_DETECT_INFO_FUNC( "WIFI_EINT_NUM(not defined)\n");
    #endif

    WMT_DETECT_INFO_FUNC( "[WMT-PLAT]=>dump wmt pin configuration emds<=\n");
    return 0;
}

int _wmt_detect_set_output_mode (unsigned int id)
{
	mt_set_gpio_pull_enable(id, GPIO_PULL_DISABLE);
	mt_set_gpio_dir(id, GPIO_DIR_OUT);
	mt_set_gpio_mode(id, GPIO_MODE_GPIO);
	WMT_DETECT_DBG_FUNC("WMT-DETECT: set GPIO%d to output mode \n", id);
	return 0;
}

int _wmt_detect_set_input_mode (unsigned int id)
{
	mt_set_gpio_pull_enable(id, GPIO_PULL_DISABLE);
	mt_set_gpio_dir(id, GPIO_DIR_IN);
	mt_set_gpio_mode(id, GPIO_MODE_GPIO);
	WMT_DETECT_DBG_FUNC("WMT-DETECT: set GPIO%d to input mode \n", id);
	return 0;
}

int _wmt_detect_output_low (unsigned int id)
{
	/*_wmt_detect_set_output_mode(id);*/
	mt_set_gpio_out(id, GPIO_OUT_ZERO);
	WMT_DETECT_DBG_FUNC("WMT-DETECT: set GPIO%d to output 0 \n", id);
	return 0;
}

int _wmt_detect_output_high (unsigned int id)
{
	/*_wmt_detect_set_output_mode(id);*/
	mt_set_gpio_out(id, GPIO_OUT_ONE);
	WMT_DETECT_DBG_FUNC("WMT-DETECT: set GPIO%d to output 0 \n", id);
	return 0;
}

int _wmt_detect_read_gpio_input (unsigned int id)
{
	int retval = 0;
	_wmt_detect_set_input_mode(id);
	retval = mt_get_gpio_in(id);
	WMT_DETECT_DBG_FUNC("WMT-DETECT: set GPIO%d to output 0 \n", id);
	return retval;
}


/*This power on sequence must support all combo chip's basic power on sequence 
	1. LDO control is a must, if external LDO exist
	2. PMU control is a must 
	3. RST control is a must
	4. WIFI_EINT pin control is a must, used for GPIO mode for EINT status checkup
	5. RTC32k clock control is a must
*/
static int wmt_detect_chip_pwr_on (void)
{
	int retval = -1;
	
	/*setting validiation check*/
	if ((COMBO_PMU_PIN == INVALID_PIN_ID) || \
		(COMBO_RST_PIN == INVALID_PIN_ID) || \
		(COMBO_WIFI_EINT_PIN == INVALID_PIN_ID) )
	{
		WMT_DETECT_ERR_FUNC("WMT-DETECT: either PMU(%d) or RST(%d) or WIFI_EINT(%d) is not set\n",\
			COMBO_PMU_PIN, \
			COMBO_RST_PIN, \
			COMBO_WIFI_EINT_PIN);
		return retval;
	}
	
	/*set LDO/PMU/RST to output 0, no pull*/
	if (COMBO_LDO_PIN != INVALID_PIN_ID)
	{
		_wmt_detect_set_output_mode(COMBO_LDO_PIN);
		_wmt_detect_output_low(COMBO_LDO_PIN);
	}

	
	_wmt_detect_set_output_mode(COMBO_PMU_PIN);
	_wmt_detect_output_low(COMBO_PMU_PIN);
	

	_wmt_detect_set_output_mode(COMBO_RST_PIN);
	_wmt_detect_output_low(COMBO_RST_PIN);

#ifdef CONFIG_MTK_COMBO_COMM_NPWR	
	if ((COMBO_I2S_DAT_PIN != INVALID_PIN_ID) && \
			(COMBO_PCM_SYNC_PIN != INVALID_PIN_ID)
	)
	{
		_wmt_detect_set_output_mode(COMBO_I2S_DAT_PIN);
		_wmt_detect_output_low(COMBO_I2S_DAT_PIN);
		
		_wmt_detect_set_output_mode(COMBO_PCM_SYNC_PIN);
		_wmt_detect_output_low(COMBO_PCM_SYNC_PIN);
		
		if (COMBO_PCM_IN_PIN != INVALID_PIN_ID)
		{
			_wmt_detect_set_output_mode(COMBO_PCM_IN_PIN);
			_wmt_detect_output_low(COMBO_PCM_IN_PIN);
		}
		
		if (COMBO_PCM_OUT_PIN != INVALID_PIN_ID)
		{
			_wmt_detect_set_output_mode(COMBO_PCM_OUT_PIN);
			_wmt_detect_output_low(COMBO_PCM_OUT_PIN);
		}
		
		if (COMBO_PCM_CLK_PIN != INVALID_PIN_ID)
		{
			_wmt_detect_set_output_mode(COMBO_PCM_CLK_PIN);
			_wmt_detect_output_low(COMBO_PCM_CLK_PIN);
		}
	}
	else
	{
		WMT_DETECT_INFO_FUNC("WMT-DETECT: PCM SYNC (%d) and I2S DAT (%d) is not defined\n");
	}
	_wmt_detect_set_output_mode(COMBO_RST_PIN);
	_wmt_detect_output_low(COMBO_RST_PIN);
#endif

#if 0
	_wmt_detect_set_output_mode(COMBO_WIFI_EINT_PIN);
	
	_wmt_detect_output_low(COMBO_WIFI_EINT_PIN);
#endif

	/*pull high LDO*/
	_wmt_detect_output_high(COMBO_LDO_PIN);
	/*sleep for LDO stable time*/
	msleep(MAX_LDO_STABLE_TIME);
	
	/*export RTC clock, sleep for RTC stable time*/
	rtc_gpio_enable_32k(RTC_GPIO_USER_GPS);
	msleep(MAX_RTC_STABLE_TIME);
	
	/*PMU output low, RST output low, to make chip power off completely*/
	/*always done*/
	
	/*sleep for power off stable time*/
	msleep(MAX_OFF_STABLE_TIME);
	/*PMU output high, and sleep for reset stable time*/
	_wmt_detect_output_high(COMBO_PMU_PIN);
#ifdef CONFIG_MTK_COMBO_COMM_NPWR
	if ((COMBO_I2S_DAT_PIN != INVALID_PIN_ID) && \
			(COMBO_PCM_SYNC_PIN != INVALID_PIN_ID)
	)
	{
		msleep(20);
		_wmt_detect_set_output_mode(COMBO_PCM_SYNC_PIN);
		_wmt_detect_output_high(COMBO_PCM_SYNC_PIN);
		
		msleep(20);
		_wmt_detect_set_output_mode(COMBO_I2S_DAT_PIN);
		_wmt_detect_output_high(COMBO_I2S_DAT_PIN);

		msleep(20);
		_wmt_detect_set_output_mode(COMBO_I2S_DAT_PIN);
		_wmt_detect_output_low(COMBO_I2S_DAT_PIN);
		
		msleep(20);
		_wmt_detect_set_output_mode(COMBO_PCM_SYNC_PIN);
		_wmt_detect_output_low(COMBO_PCM_SYNC_PIN);
		
		msleep(20);
	}
#endif
	msleep(MAX_RST_STABLE_TIME);
	/*RST output high, and sleep for power on stable time*/
	_wmt_detect_output_high(COMBO_RST_PIN);
	msleep(MAX_ON_STABLE_TIME);
	
	retval = 0;
	return retval;
}

static int wmt_detect_chip_pwr_off (void)
{

	/*set RST pin to input low status*/
	if (COMBO_RST_PIN != INVALID_PIN_ID)
	{
		_wmt_detect_set_output_mode(COMBO_RST_PIN);
		_wmt_detect_output_low(COMBO_RST_PIN);
	}		
	/*set RST pin to input low status*/
	if (COMBO_PMU_PIN != INVALID_PIN_ID)
	{
		_wmt_detect_set_output_mode(COMBO_PMU_PIN);
		_wmt_detect_output_low(COMBO_PMU_PIN);
	}
	/*set LDO pin to input low status*/
	if (COMBO_LDO_PIN != INVALID_PIN_ID)
	{
		_wmt_detect_set_output_mode(COMBO_LDO_PIN);
		_wmt_detect_output_low(COMBO_LDO_PIN);
	}
	return 0;	
}

int wmt_detect_read_ext_cmb_status (void)
{
	int retval = 0;
	/*read WIFI_EINT pin status*/
	if (COMBO_WIFI_EINT_PIN == INVALID_PIN_ID)
	{
		retval = 0;
		WMT_DETECT_ERR_FUNC("WMT-DETECT: no WIFI_EINT pin set\n");
	}
	else
	{
		retval = _wmt_detect_read_gpio_input(COMBO_WIFI_EINT_PIN);
		WMT_DETECT_ERR_FUNC("WMT-DETECT: WIFI_EINT input status:%d\n", retval);
		
	}
	return retval;
}



int wmt_detect_chip_pwr_ctrl (int on)
{
	int retval = -1;
	if (0 == on)
	{
		/*power off combo chip*/
		retval = wmt_detect_chip_pwr_off();
	}
	else
	{
		wmt_detect_dump_pin_conf();
		/*power on combo chip*/
		retval = wmt_detect_chip_pwr_on();
	}
	return retval;
}


int wmt_detect_sdio_pwr_ctrl (int on)
{
	int retval = -1;
#ifdef MTK_WCN_COMBO_CHIP_SUPPORT	
	if (0 == on)
	{
		/*power off SDIO slot*/
		retval = board_sdio_ctrl(1, 0);
	}
	else
	{
		/*power on SDIO slot*/
		retval = board_sdio_ctrl(1, 1);
	}
#else
	WMT_DETECT_WARN_FUNC("WMT-DETECT: MTK_WCN_COMBO_CHIP_SUPPORT is not set\n");
#endif
	return retval;
}


