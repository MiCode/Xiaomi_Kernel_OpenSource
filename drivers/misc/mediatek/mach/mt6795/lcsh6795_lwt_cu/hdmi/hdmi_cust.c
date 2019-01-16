/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 * hdmi_customization.h
 *
 * Project:
 * --------
 *   Android
 *
 * Description:
 * ------------
 *   This file implements Customization base function
 *
 *******************************************************************************/

#include <cust_gpio_usage.h>
#include <hdmi_cust.h>
#include <mach/mt_gpio.h>

/******************************************************************
** Basic define
******************************************************************/
#ifndef s32
	#define s32 signed int
#endif
#ifndef s64
	#define s64 signed long long
#endif

static bool cust_power_on = false;
int cust_hdmi_power_on(int on)
{
{
	if(on > 0) 
	{
	    printk("MHL_Power power %x, rst %x \n" ,GPIO_MHL_POWER_CTRL_PIN, GPIO_MHL_RST_B_PIN);
	    mt_set_gpio_mode(GPIO_MHL_POWER_CTRL_PIN, GPIO_MODE_00);
		mt_set_gpio_dir(GPIO_MHL_POWER_CTRL_PIN, GPIO_DIR_OUT);
		mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
#ifdef PMIC_APP_MHL_POWER_LDO1
		if(cust_power_on == false)
		{
			hwPowerOn(PMIC_APP_MHL_POWER_LDO1, VOL_1200,"MHL");
			cust_power_on = true;
		}
#else
		printk("Error: PMIC_APP_MHL_POWER_LDO1 not defined -\n" );
#endif
	}
	else
	{
#ifdef PMIC_APP_MHL_POWER_LDO1
		if(cust_power_on == true)
		{
			hwPowerDown(PMIC_APP_MHL_POWER_LDO1, "MHL");
			cust_power_on = false;
		}
#endif
	}
	return 0;
}
}

int cust_hdmi_dpi_gpio_on(int on)
{
    unsigned int dpi_pin_start = 0;
    if(on > 0)
    {        	  
#ifdef GPIO_EXT_DISP_DPI0_PIN
		for(dpi_pin_start = GPIO_EXT_DISP_DPI0_PIN; dpi_pin_start < GPIO_EXT_DISP_DPI0_PIN + 16; dpi_pin_start++)
		{
			mt_set_gpio_mode(dpi_pin_start, GPIO_MODE_01);
		}
		printk("%s, %d GPIO_EXT_DISP_DPI0_PIN is defined+ %x\n", __func__, __LINE__, GPIO_EXT_DISP_DPI0_PIN);
#else
		printk("%s,%d Error: GPIO_EXT_DISP_DPI0_PIN is not defined\n", __func__, __LINE__);
#endif
	
    }
    else
    {
#ifdef GPIO_EXT_DISP_DPI0_PIN
		for(dpi_pin_start = GPIO_EXT_DISP_DPI0_PIN; dpi_pin_start < GPIO_EXT_DISP_DPI0_PIN + 16; dpi_pin_start++)
		{
			mt_set_gpio_mode(dpi_pin_start, GPIO_MODE_00);
			mt_set_gpio_dir(dpi_pin_start, GPIO_DIR_IN);
			mt_set_gpio_pull_enable(dpi_pin_start, GPIO_PULL_ENABLE);
			mt_set_gpio_pull_select(dpi_pin_start, GPIO_PULL_DOWN);
		}
		printk("%s, %d GPIO_EXT_DISP_DPI0_PIN is defined- %x\n", __func__, __LINE__, GPIO_EXT_DISP_DPI0_PIN);
#endif
	}
	return 0;
}

int cust_hdmi_i2s_gpio_on(int on)
{
    if(on > 0)
    {
#ifdef GPIO_MHL_I2S_OUT_WS_PIN
        mt_set_gpio_mode(GPIO_MHL_I2S_OUT_WS_PIN, GPIO_MHL_I2S_OUT_WS_PIN_M_I2S3_WS);
        mt_set_gpio_mode(GPIO_MHL_I2S_OUT_CK_PIN, GPIO_MHL_I2S_OUT_CK_PIN_M_I2S3_BCK);
        mt_set_gpio_mode(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_MHL_I2S_OUT_DAT_PIN_M_I2S3_DO);
#else
        printk("%s,%d Error. GPIO_MHL_I2S_OUT_WS_PIN is not defined\n", __func__, __LINE__);
#endif
    }
    else
    {
#ifdef GPIO_MHL_I2S_OUT_WS_PIN
        mt_set_gpio_pull_enable(GPIO_MHL_I2S_OUT_WS_PIN, GPIO_PULL_DISABLE);
        mt_set_gpio_pull_enable(GPIO_MHL_I2S_OUT_CK_PIN, GPIO_PULL_DISABLE);
        mt_set_gpio_pull_enable(GPIO_MHL_I2S_OUT_DAT_PIN, GPIO_PULL_DISABLE);
#endif
    }
    return 0;
}

int get_hdmi_i2c_addr(void)
{
    return (SII_I2C_ADDR);
}

int get_hdmi_i2c_channel(void)
{
    return (HDMI_I2C_CHANNEL);
}