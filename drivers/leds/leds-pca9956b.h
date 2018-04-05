/*
 * leds-pca9956b.h - NXP PCA9956B LED segment driver
 *
 * Copyright (C) 2017 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef PCA9956B_H

/* Register address */
enum {
	PCA9956B_MODE1 = 0x00,  /* AIF, SLEEP, SUBn, ALLCALL */
	PCA9956B_MODE2,         /* OVERTEMP, ERROR, DMBLNK, CLRERR, OCH */
	PCA9956B_LEDOUT0,       /* LED driver output state */
	PCA9956B_LEDOUT1,
	PCA9956B_LEDOUT2,
	PCA9956B_LEDOUT3,
	PCA9956B_LEDOUT4,
	PCA9956B_LEDOUT5,
	PCA9956B_GRPPWM,        /* DMBLINK set 0, then GRPPWM controls
				global brightness */
	PCA9956B_GRPFREQ,       /* DMBLINK set 1, then GRPFREQ controls
				global blinking period */

	/* 10 : 0x0A */
	PCA9956B_PWM0,          /* Brightness control */
	PCA9956B_PWM1,
	PCA9956B_PWM2,
	PCA9956B_PWM3,
	PCA9956B_PWM4,
	PCA9956B_PWM5,
	PCA9956B_PWM6,
	PCA9956B_PWM7,
	PCA9956B_PWM8,
	PCA9956B_PWM9,

	/* 20 : 0x14 */
	PCA9956B_PWM10,
	PCA9956B_PWM11,
	PCA9956B_PWM12,
	PCA9956B_PWM13,
	PCA9956B_PWM14,
	PCA9956B_PWM15,
	PCA9956B_PWM16,
	PCA9956B_PWM17,
	PCA9956B_PWM18,
	PCA9956B_PWM19,

	/* 30 : 0x1E */
	PCA9956B_PWM20,
	PCA9956B_PWM21,
	PCA9956B_PWM22,
	PCA9956B_PWM23,
	PCA9956B_IREF0,         /* Output current control */
	PCA9956B_IREF1,
	PCA9956B_IREF2,
	PCA9956B_IREF3,
	PCA9956B_IREF4,
	PCA9956B_IREF5,

	/* 40 : 0x28 */
	PCA9956B_IREF6,
	PCA9956B_IREF7,
	PCA9956B_IREF8,
	PCA9956B_IREF9,
	PCA9956B_IREF10,
	PCA9956B_IREF11,
	PCA9956B_IREF12,
	PCA9956B_IREF13,
	PCA9956B_IREF14,
	PCA9956B_IREF15,

	/* 50 : 0x32 */
	PCA9956B_IREF16,
	PCA9956B_IREF17,
	PCA9956B_IREF18,
	PCA9956B_IREF19,
	PCA9956B_IREF20,
	PCA9956B_IREF21,
	PCA9956B_IREF22,
	PCA9956B_IREF23,
	PCA9956B_OFFSET,        /* led turn-on delay */
	PCA9956B_SUBADR1,       /* I2C bus subaddress */

	/* 60 : 0x3C */
	PCA9956B_SUBADR2,
	PCA9956B_SUBADR3,
	PCA9956B_ALLCALLADR,    /* Allows all the PCA9956Bs on the bus to be
				programmed at the same time */
	PCA9956B_PWMALL,        /* brightness control for all LEDn outputs */
	PCA9956B_IREFALL,       /* output current value for all LED outputs */
	PCA9956B_EFLAG0,        /* LED error detection */
	PCA9956B_EFLAG1,
	PCA9956B_EFLAG2,
	PCA9956B_EFLAG3,
	PCA9956B_EFLAG4,

	/* 70 : 0x46 */
	PCA9956B_EFLAG5,
};

#endif /* PCA9956B_H */
