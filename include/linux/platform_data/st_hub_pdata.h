/*
 * STMicroelectronics sensor-hub platform-data driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Denis Ciocca <denis.ciocca@st.com>
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef ST_HUB_PDATA_H
#define ST_HUB_PDATA_H

/**
 * struct st_hub_pdata - sensor-hub platform data
 * @gpio_wakeup: gpio #number used to wake up the HUB.
 * @gpio_reset: gpio #number used to reset the HUB.
 */
struct st_hub_pdata {
	int gpio_wakeup;
	int gpio_reset;
};

#endif /* ST_HUB_PDATA_H */
