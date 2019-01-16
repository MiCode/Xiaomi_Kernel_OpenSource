/******************************************************************************
 * mt65xx_leds.h
 *
 * Copyright 2010 MediaTek Co.,Ltd.
 *
 ******************************************************************************/
#ifndef _MT65XX_LEDS_H
#define _MT65XX_LEDS_H

#include <linux/leds.h>
#include <cust_leds.h>

extern int mt65xx_leds_brightness_set(enum mt65xx_led_type type, enum led_brightness value);
extern int backlight_brightness_set(int level);

#endif
