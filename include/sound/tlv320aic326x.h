/*
 * Platform data for Texas Instruments TLV320AIC326x codec
 *
 * Copyright 2010 TI Products
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#ifndef __LINUX_SND_TLV320AIC326x_H__
#define __LINUX_SND_TLV320AIC326x_H__

/* codec platform data */
struct aic326x_pdata {

	/* has to be one of 16,32,64,128,256,512 ms
	as per the data sheet */
	unsigned int debounce_time_ms;
};

#endif
