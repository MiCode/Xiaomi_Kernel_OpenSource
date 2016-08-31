/*
 * Raydium RM31080 touchscreen driver
 *
 * Copyright (C) 2012-2013, Raydium Semiconductor Corporation.
 * All Rights Reserved.
 * Copyright (C) 2012-2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
/*=============================================================================
	INCLUDED FILES
=============================================================================*/
#include <linux/device.h>
#include <linux/uaccess.h>	/* copy_to_user() */
#include <linux/delay.h>
#include <linux/module.h>	/* Module definition */

#include <linux/spi/rm31080a_ts.h>
#include <linux/spi/rm31080a_ctrl.h>

/*=============================================================================
	GLOBAL VARIABLES DECLARATION
=============================================================================*/
struct rm_tch_ctrl_para g_stCtrl;

/*=============================================================================
	FUNCTION DECLARATION
=============================================================================*/
/*=============================================================================
	Description:

	Input:
			N/A
	Output:
			N/A
=============================================================================*/
void rm_tch_ctrl_init(void)
{
	memset(&g_stCtrl, 0, sizeof(struct rm_tch_ctrl_para));
}

/*=============================================================================
	Description: To transfer the value to HAL layer

	Input:
			N/A
	Output:
			N/A
=============================================================================*/
unsigned char rm_tch_ctrl_get_idle_mode(u8 *p)
{
	u32 u32Ret;
	u32Ret = copy_to_user(p, &g_stCtrl.bfIdleModeCheck, 1);
	if (u32Ret != 0)
		return 0;
	return 1;
}

/*=============================================================================
	Description:

	Input:
			N/A
	Output:
			N/A
=============================================================================*/
void rm_tch_ctrl_set_parameter(void *arg)
{
	ssize_t missing;
	missing = copy_from_user(&g_stCtrl, arg, sizeof(struct rm_tch_ctrl_para));
	if (missing)
		return;
}

/*===========================================================================*/
MODULE_AUTHOR("xxxxxxxxxx <xxxxxxxx@rad-ic.com>");
MODULE_DESCRIPTION("Raydium touchscreen control functions");
MODULE_LICENSE("GPL");
