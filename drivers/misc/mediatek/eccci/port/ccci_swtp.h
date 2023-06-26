/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __SWTP_H__
#define __SWTP_H__

/* Huaqin add for HQ-123513 by liunianliang at 2021/04/25 start */
#define MAX_PIN_NUM 1
/* Huaqin add for HQ-123513 by liunianliang at 2021/04/25 end */

#define SWTP_COMPATIBLE_DEVICE_ID "mediatek, swtp-eint"

/* Huaqin add for HQ-123513 by liunianliang at 2021/04/25 start */
#define SWTP1_COMPATIBLE_DEVICE_ID "mediatek, swtp1-eint"
//#define SWTP_COMPATIBLE_DEVICE_ID "mediatek, swtp-eint"
/* Huaqin add for HQ-123513 by liunianliang at 2021/04/25 end */

#define SWTP_EINT_PIN_PLUG_IN        (1)
#define SWTP_EINT_PIN_PLUG_OUT       (0)

/* Huaqin modify for HQ-123513 by liunianliang at 2021/04/25 start */
#define SWTP_DO_TX_POWER	(0)
#define SWTP_NO_TX_POWER	(1)

struct swtp_t {
	unsigned int	md_id;
	unsigned int	irq[MAX_PIN_NUM];
	unsigned int	gpiopin[MAX_PIN_NUM];
	unsigned int	setdebounce[MAX_PIN_NUM];
	unsigned int	eint_type[MAX_PIN_NUM];
	unsigned int	gpio_state[MAX_PIN_NUM];
	int	tx_power_mode;
	spinlock_t		spinlock;
	struct delayed_work delayed_work;
};

/*
struct swtp_t {
	unsigned int	md_id;
	unsigned int	irq;
	unsigned int	gpiopin;
	unsigned int	setdebounce;
	unsigned int	eint_type;
	unsigned int	curr_mode;
	unsigned int	retry_cnt;
	spinlock_t		spinlock;
	struct delayed_work delayed_work;
    struct delayed_work delayed_work_swtp;
};
*/
/*****************************************************************************/
/* External API Region called by ccci_swtp object */
/*****************************************************************************/
//extern int ccci_md_get_state_by_id(int md_id);
/* Huaqin modify for HQ-123513 by liunianliang at 2021/04/25 end */
extern int swtp_init(int md_id);
#endif				/* __SWTP_H__ */
