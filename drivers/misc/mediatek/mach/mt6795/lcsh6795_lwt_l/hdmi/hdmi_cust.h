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

#ifndef HDMI_CUSTOMIZATION_H
#define HDMI_CUSTOMIZATION_H

/******************************************************************
** scale adjustment
******************************************************************/
#define USING_SCALE_ADJUSTMENT

///if mhl chip CI2CA pin is pull up, the folloing should be defined. otherwise, please mask it.
#define SII_I2C_ADDR  (0x72)
#define HDMI_I2C_CHANNEL 3



/******************************************************************
** MHL GPIO Customization
******************************************************************/
//#define MHL_PHONE_GPIO_REUSAGE
void ChangeGPIOToI2S();
void ChangeI2SToGPIO();

int cust_hdmi_power_on(int on);

int cust_hdmi_dpi_gpio_on(int on);

int cust_hdmi_i2s_gpio_on(int on);

int get_hdmi_i2c_addr(void);

int get_hdmi_i2c_channel(void);


#endif
